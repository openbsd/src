//===- Trace.cpp - XRay Trace Loading implementation. ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// XRay log reader implementation.
//
//===----------------------------------------------------------------------===//
#include "llvm/XRay/Trace.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/DataExtractor.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/XRay/YAMLXRayRecord.h"

using namespace llvm;
using namespace llvm::xray;
using llvm::yaml::Input;

namespace {
using XRayRecordStorage =
    std::aligned_storage<sizeof(XRayRecord), alignof(XRayRecord)>::type;

// Populates the FileHeader reference by reading the first 32 bytes of the file.
Error readBinaryFormatHeader(StringRef Data, XRayFileHeader &FileHeader) {
  // FIXME: Maybe deduce whether the data is little or big-endian using some
  // magic bytes in the beginning of the file?

  // First 32 bytes of the file will always be the header. We assume a certain
  // format here:
  //
  //   (2)   uint16 : version
  //   (2)   uint16 : type
  //   (4)   uint32 : bitfield
  //   (8)   uint64 : cycle frequency
  //   (16)  -      : padding

  DataExtractor HeaderExtractor(Data, true, 8);
  uint32_t OffsetPtr = 0;
  FileHeader.Version = HeaderExtractor.getU16(&OffsetPtr);
  FileHeader.Type = HeaderExtractor.getU16(&OffsetPtr);
  uint32_t Bitfield = HeaderExtractor.getU32(&OffsetPtr);
  FileHeader.ConstantTSC = Bitfield & 1uL;
  FileHeader.NonstopTSC = Bitfield & 1uL << 1;
  FileHeader.CycleFrequency = HeaderExtractor.getU64(&OffsetPtr);
  std::memcpy(&FileHeader.FreeFormData, Data.bytes_begin() + OffsetPtr, 16);
  if (FileHeader.Version != 1 && FileHeader.Version != 2 &&
      FileHeader.Version != 3)
    return make_error<StringError>(
        Twine("Unsupported XRay file version: ") + Twine(FileHeader.Version),
        std::make_error_code(std::errc::invalid_argument));
  return Error::success();
}

Error loadNaiveFormatLog(StringRef Data, XRayFileHeader &FileHeader,
                         std::vector<XRayRecord> &Records) {
  if (Data.size() < 32)
    return make_error<StringError>(
        "Not enough bytes for an XRay log.",
        std::make_error_code(std::errc::invalid_argument));

  if (Data.size() - 32 == 0 || Data.size() % 32 != 0)
    return make_error<StringError>(
        "Invalid-sized XRay data.",
        std::make_error_code(std::errc::invalid_argument));

  if (auto E = readBinaryFormatHeader(Data, FileHeader))
    return E;

  // Each record after the header will be 32 bytes, in the following format:
  //
  //   (2)   uint16 : record type
  //   (1)   uint8  : cpu id
  //   (1)   uint8  : type
  //   (4)   sint32 : function id
  //   (8)   uint64 : tsc
  //   (4)   uint32 : thread id
  //   (4)   uint32 : process id
  //   (8)   -      : padding
  for (auto S = Data.drop_front(32); !S.empty(); S = S.drop_front(32)) {
    DataExtractor RecordExtractor(S, true, 8);
    uint32_t OffsetPtr = 0;
    switch (auto RecordType = RecordExtractor.getU16(&OffsetPtr)) {
    case 0: { // Normal records.
      Records.emplace_back();
      auto &Record = Records.back();
      Record.RecordType = RecordType;
      Record.CPU = RecordExtractor.getU8(&OffsetPtr);
      auto Type = RecordExtractor.getU8(&OffsetPtr);
      switch (Type) {
      case 0:
        Record.Type = RecordTypes::ENTER;
        break;
      case 1:
        Record.Type = RecordTypes::EXIT;
        break;
      case 2:
        Record.Type = RecordTypes::TAIL_EXIT;
        break;
      case 3:
        Record.Type = RecordTypes::ENTER_ARG;
        break;
      default:
        return make_error<StringError>(
            Twine("Unknown record type '") + Twine(int{Type}) + "'",
            std::make_error_code(std::errc::executable_format_error));
      }
      Record.FuncId = RecordExtractor.getSigned(&OffsetPtr, sizeof(int32_t));
      Record.TSC = RecordExtractor.getU64(&OffsetPtr);
      Record.TId = RecordExtractor.getU32(&OffsetPtr);
      Record.PId = RecordExtractor.getU32(&OffsetPtr);
      break;
    }
    case 1: { // Arg payload record.
      auto &Record = Records.back();
      // Advance two bytes to avoid padding.
      OffsetPtr += 2;
      int32_t FuncId = RecordExtractor.getSigned(&OffsetPtr, sizeof(int32_t));
      auto TId = RecordExtractor.getU32(&OffsetPtr);
      auto PId = RecordExtractor.getU32(&OffsetPtr);

      // Make a check for versions above 3 for the Pid field
      if (Record.FuncId != FuncId || Record.TId != TId ||
          (FileHeader.Version >= 3 ? Record.PId != PId : false))
        return make_error<StringError>(
            Twine("Corrupted log, found arg payload following non-matching "
                  "function + thread record. Record for function ") +
                Twine(Record.FuncId) + " != " + Twine(FuncId) + "; offset: " +
                Twine(S.data() - Data.data()),
            std::make_error_code(std::errc::executable_format_error));

      auto Arg = RecordExtractor.getU64(&OffsetPtr);
      Record.CallArgs.push_back(Arg);
      break;
    }
    default:
      return make_error<StringError>(
          Twine("Unknown record type == ") + Twine(RecordType),
          std::make_error_code(std::errc::executable_format_error));
    }
  }
  return Error::success();
}

/// When reading from a Flight Data Recorder mode log, metadata records are
/// sparse compared to packed function records, so we must maintain state as we
/// read through the sequence of entries. This allows the reader to denormalize
/// the CPUId and Thread Id onto each Function Record and transform delta
/// encoded TSC values into absolute encodings on each record.
struct FDRState {
  uint16_t CPUId;
  uint16_t ThreadId;
  int32_t ProcessId;
  uint64_t BaseTSC;

  /// Encode some of the state transitions for the FDR log reader as explicit
  /// checks. These are expectations for the next Record in the stream.
  enum class Token {
    NEW_BUFFER_RECORD_OR_EOF,
    WALLCLOCK_RECORD,
    NEW_CPU_ID_RECORD,
    FUNCTION_SEQUENCE,
    SCAN_TO_END_OF_THREAD_BUF,
    CUSTOM_EVENT_DATA,
    CALL_ARGUMENT,
    BUFFER_EXTENTS,
    PID_RECORD,
  };
  Token Expects;

  // Each threads buffer may have trailing garbage to scan over, so we track our
  // progress.
  uint64_t CurrentBufferSize;
  uint64_t CurrentBufferConsumed;
};

const char *fdrStateToTwine(const FDRState::Token &state) {
  switch (state) {
  case FDRState::Token::NEW_BUFFER_RECORD_OR_EOF:
    return "NEW_BUFFER_RECORD_OR_EOF";
  case FDRState::Token::WALLCLOCK_RECORD:
    return "WALLCLOCK_RECORD";
  case FDRState::Token::NEW_CPU_ID_RECORD:
    return "NEW_CPU_ID_RECORD";
  case FDRState::Token::FUNCTION_SEQUENCE:
    return "FUNCTION_SEQUENCE";
  case FDRState::Token::SCAN_TO_END_OF_THREAD_BUF:
    return "SCAN_TO_END_OF_THREAD_BUF";
  case FDRState::Token::CUSTOM_EVENT_DATA:
    return "CUSTOM_EVENT_DATA";
  case FDRState::Token::CALL_ARGUMENT:
    return "CALL_ARGUMENT";
  case FDRState::Token::BUFFER_EXTENTS:
    return "BUFFER_EXTENTS";
  case FDRState::Token::PID_RECORD:
    return "PID_RECORD";
  }
  return "UNKNOWN";
}

/// State transition when a NewBufferRecord is encountered.
Error processFDRNewBufferRecord(FDRState &State, uint8_t RecordFirstByte,
                                DataExtractor &RecordExtractor) {

  if (State.Expects != FDRState::Token::NEW_BUFFER_RECORD_OR_EOF)
    return make_error<StringError>(
        Twine("Malformed log. Read New Buffer record kind out of sequence; "
              "expected: ") +
            fdrStateToTwine(State.Expects),
        std::make_error_code(std::errc::executable_format_error));
  uint32_t OffsetPtr = 1; // 1 byte into record.
  State.ThreadId = RecordExtractor.getU16(&OffsetPtr);
  State.Expects = FDRState::Token::WALLCLOCK_RECORD;
  return Error::success();
}

/// State transition when an EndOfBufferRecord is encountered.
Error processFDREndOfBufferRecord(FDRState &State, uint8_t RecordFirstByte,
                                  DataExtractor &RecordExtractor) {
  if (State.Expects == FDRState::Token::NEW_BUFFER_RECORD_OR_EOF)
    return make_error<StringError>(
        Twine("Malformed log. Received EOB message without current buffer; "
              "expected: ") +
            fdrStateToTwine(State.Expects),
        std::make_error_code(std::errc::executable_format_error));
  State.Expects = FDRState::Token::SCAN_TO_END_OF_THREAD_BUF;
  return Error::success();
}

/// State transition when a NewCPUIdRecord is encountered.
Error processFDRNewCPUIdRecord(FDRState &State, uint8_t RecordFirstByte,
                               DataExtractor &RecordExtractor) {
  if (State.Expects != FDRState::Token::FUNCTION_SEQUENCE &&
      State.Expects != FDRState::Token::NEW_CPU_ID_RECORD)
    return make_error<StringError>(
        Twine("Malformed log. Read NewCPUId record kind out of sequence; "
              "expected: ") +
            fdrStateToTwine(State.Expects),
        std::make_error_code(std::errc::executable_format_error));
  uint32_t OffsetPtr = 1; // Read starting after the first byte.
  State.CPUId = RecordExtractor.getU16(&OffsetPtr);
  State.BaseTSC = RecordExtractor.getU64(&OffsetPtr);
  State.Expects = FDRState::Token::FUNCTION_SEQUENCE;
  return Error::success();
}

/// State transition when a TSCWrapRecord (overflow detection) is encountered.
Error processFDRTSCWrapRecord(FDRState &State, uint8_t RecordFirstByte,
                              DataExtractor &RecordExtractor) {
  if (State.Expects != FDRState::Token::FUNCTION_SEQUENCE)
    return make_error<StringError>(
        Twine("Malformed log. Read TSCWrap record kind out of sequence; "
              "expecting: ") +
            fdrStateToTwine(State.Expects),
        std::make_error_code(std::errc::executable_format_error));
  uint32_t OffsetPtr = 1; // Read starting after the first byte.
  State.BaseTSC = RecordExtractor.getU64(&OffsetPtr);
  return Error::success();
}

/// State transition when a WallTimeMarkerRecord is encountered.
Error processFDRWallTimeRecord(FDRState &State, uint8_t RecordFirstByte,
                               DataExtractor &RecordExtractor) {
  if (State.Expects != FDRState::Token::WALLCLOCK_RECORD)
    return make_error<StringError>(
        Twine("Malformed log. Read Wallclock record kind out of sequence; "
              "expecting: ") +
            fdrStateToTwine(State.Expects),
        std::make_error_code(std::errc::executable_format_error));

  // TODO: Someday, reconcile the TSC ticks to wall clock time for presentation
  // purposes. For now, we're ignoring these records.
  State.Expects = FDRState::Token::NEW_CPU_ID_RECORD;
  return Error::success();
}

/// State transition when a PidRecord is encountered.
Error processFDRPidRecord(FDRState &State, uint8_t RecordFirstByte,
                          DataExtractor &RecordExtractor) {

  if (State.Expects != FDRState::Token::PID_RECORD)
    return make_error<StringError>(
        Twine("Malformed log. Read Pid record kind out of sequence; "
              "expected: ") +
            fdrStateToTwine(State.Expects),
        std::make_error_code(std::errc::executable_format_error));

  uint32_t OffsetPtr = 1; // Read starting after the first byte.
  State.ProcessId = RecordExtractor.getU32(&OffsetPtr);
  State.Expects = FDRState::Token::NEW_CPU_ID_RECORD;
  return Error::success();
}

/// State transition when a CustomEventMarker is encountered.
Error processCustomEventMarker(FDRState &State, uint8_t RecordFirstByte,
                               DataExtractor &RecordExtractor,
                               size_t &RecordSize) {
  // We can encounter a CustomEventMarker anywhere in the log, so we can handle
  // it regardless of the expectation. However, we do set the expectation to
  // read a set number of fixed bytes, as described in the metadata.
  uint32_t OffsetPtr = 1; // Read after the first byte.
  uint32_t DataSize = RecordExtractor.getU32(&OffsetPtr);
  uint64_t TSC = RecordExtractor.getU64(&OffsetPtr);

  // FIXME: Actually represent the record through the API. For now we only
  // skip through the data.
  (void)TSC;
  RecordSize = 16 + DataSize;
  return Error::success();
}

/// State transition when an BufferExtents record is encountered.
Error processBufferExtents(FDRState &State, uint8_t RecordFirstByte,
                           DataExtractor &RecordExtractor) {
  if (State.Expects != FDRState::Token::BUFFER_EXTENTS)
    return make_error<StringError>(
        Twine("Malformed log. Buffer Extents unexpected; expected: ") +
            fdrStateToTwine(State.Expects),
        std::make_error_code(std::errc::executable_format_error));
  uint32_t OffsetPtr = 1; // Read after the first byte.
  State.CurrentBufferSize = RecordExtractor.getU64(&OffsetPtr);
  State.Expects = FDRState::Token::NEW_BUFFER_RECORD_OR_EOF;
  return Error::success();
}

/// State transition when a CallArgumentRecord is encountered.
Error processFDRCallArgumentRecord(FDRState &State, uint8_t RecordFirstByte,
                                   DataExtractor &RecordExtractor,
                                   std::vector<XRayRecord> &Records) {
  uint32_t OffsetPtr = 1; // Read starting after the first byte.
  auto &Enter = Records.back();

  if (Enter.Type != RecordTypes::ENTER)
    return make_error<StringError>(
        "CallArgument needs to be right after a function entry",
        std::make_error_code(std::errc::executable_format_error));
  Enter.Type = RecordTypes::ENTER_ARG;
  Enter.CallArgs.emplace_back(RecordExtractor.getU64(&OffsetPtr));
  return Error::success();
}

/// Advances the state machine for reading the FDR record type by reading one
/// Metadata Record and updating the State appropriately based on the kind of
/// record encountered. The RecordKind is encoded in the first byte of the
/// Record, which the caller should pass in because they have already read it
/// to determine that this is a metadata record as opposed to a function record.
///
/// Beginning with Version 2 of the FDR log, we do not depend on the size of the
/// buffer, but rather use the extents to determine how far to read in the log
/// for this particular buffer.
///
/// In Version 3, FDR log now includes a pid metadata record after
/// WallTimeMarker
Error processFDRMetadataRecord(FDRState &State, uint8_t RecordFirstByte,
                               DataExtractor &RecordExtractor,
                               size_t &RecordSize,
                               std::vector<XRayRecord> &Records,
                               uint16_t Version) {
  // The remaining 7 bits are the RecordKind enum.
  uint8_t RecordKind = RecordFirstByte >> 1;
  switch (RecordKind) {
  case 0: // NewBuffer
    if (auto E =
            processFDRNewBufferRecord(State, RecordFirstByte, RecordExtractor))
      return E;
    break;
  case 1: // EndOfBuffer
    if (Version >= 2)
      return make_error<StringError>(
          "Since Version 2 of FDR logging, we no longer support EOB records.",
          std::make_error_code(std::errc::executable_format_error));
    if (auto E = processFDREndOfBufferRecord(State, RecordFirstByte,
                                             RecordExtractor))
      return E;
    break;
  case 2: // NewCPUId
    if (auto E =
            processFDRNewCPUIdRecord(State, RecordFirstByte, RecordExtractor))
      return E;
    break;
  case 3: // TSCWrap
    if (auto E =
            processFDRTSCWrapRecord(State, RecordFirstByte, RecordExtractor))
      return E;
    break;
  case 4: // WallTimeMarker
    if (auto E =
            processFDRWallTimeRecord(State, RecordFirstByte, RecordExtractor))
      return E;
    // In Version 3 and and above, a PidRecord is expected after WallTimeRecord
    if (Version >= 3)
      State.Expects = FDRState::Token::PID_RECORD;
    break;
  case 5: // CustomEventMarker
    if (auto E = processCustomEventMarker(State, RecordFirstByte,
                                          RecordExtractor, RecordSize))
      return E;
    break;
  case 6: // CallArgument
    if (auto E = processFDRCallArgumentRecord(State, RecordFirstByte,
                                              RecordExtractor, Records))
      return E;
    break;
  case 7: // BufferExtents
    if (auto E = processBufferExtents(State, RecordFirstByte, RecordExtractor))
      return E;
    break;
  case 9: // Pid
    if (auto E = processFDRPidRecord(State, RecordFirstByte, RecordExtractor))
      return E;
    break;
  default:
    // Widen the record type to uint16_t to prevent conversion to char.
    return make_error<StringError>(
        Twine("Illegal metadata record type: ")
            .concat(Twine(static_cast<unsigned>(RecordKind))),
        std::make_error_code(std::errc::executable_format_error));
  }
  return Error::success();
}

/// Reads a function record from an FDR format log, appending a new XRayRecord
/// to the vector being populated and updating the State with a new value
/// reference value to interpret TSC deltas.
///
/// The XRayRecord constructed includes information from the function record
/// processed here as well as Thread ID and CPU ID formerly extracted into
/// State.
Error processFDRFunctionRecord(FDRState &State, uint8_t RecordFirstByte,
                               DataExtractor &RecordExtractor,
                               std::vector<XRayRecord> &Records) {
  switch (State.Expects) {
  case FDRState::Token::NEW_BUFFER_RECORD_OR_EOF:
    return make_error<StringError>(
        "Malformed log. Received Function Record before new buffer setup.",
        std::make_error_code(std::errc::executable_format_error));
  case FDRState::Token::WALLCLOCK_RECORD:
    return make_error<StringError>(
        "Malformed log. Received Function Record when expecting wallclock.",
        std::make_error_code(std::errc::executable_format_error));
  case FDRState::Token::PID_RECORD:
    return make_error<StringError>(
        "Malformed log. Received Function Record when expecting pid.",
        std::make_error_code(std::errc::executable_format_error));
  case FDRState::Token::NEW_CPU_ID_RECORD:
    return make_error<StringError>(
        "Malformed log. Received Function Record before first CPU record.",
        std::make_error_code(std::errc::executable_format_error));
  default:
    Records.emplace_back();
    auto &Record = Records.back();
    Record.RecordType = 0; // Record is type NORMAL.
    // Strip off record type bit and use the next three bits.
    uint8_t RecordType = (RecordFirstByte >> 1) & 0x07;
    switch (RecordType) {
    case static_cast<uint8_t>(RecordTypes::ENTER):
      Record.Type = RecordTypes::ENTER;
      break;
    case static_cast<uint8_t>(RecordTypes::EXIT):
      Record.Type = RecordTypes::EXIT;
      break;
    case static_cast<uint8_t>(RecordTypes::TAIL_EXIT):
      Record.Type = RecordTypes::TAIL_EXIT;
      break;
    default:
      // Cast to an unsigned integer to not interpret the record type as a char.
      return make_error<StringError>(
          Twine("Illegal function record type: ")
              .concat(Twine(static_cast<unsigned>(RecordType))),
          std::make_error_code(std::errc::executable_format_error));
    }
    Record.CPU = State.CPUId;
    Record.TId = State.ThreadId;
    Record.PId = State.ProcessId;
    // Back up to read first 32 bits, including the 4 we pulled RecordType
    // and RecordKind out of. The remaining 28 are FunctionId.
    uint32_t OffsetPtr = 0;
    // Despite function Id being a signed int on XRayRecord,
    // when it is written to an FDR format, the top bits are truncated,
    // so it is effectively an unsigned value. When we shift off the
    // top four bits, we want the shift to be logical, so we read as
    // uint32_t.
    uint32_t FuncIdBitField = RecordExtractor.getU32(&OffsetPtr);
    Record.FuncId = FuncIdBitField >> 4;
    // FunctionRecords have a 32 bit delta from the previous absolute TSC
    // or TSC delta. If this would overflow, we should read a TSCWrap record
    // with an absolute TSC reading.
    uint64_t NewTSC = State.BaseTSC + RecordExtractor.getU32(&OffsetPtr);
    State.BaseTSC = NewTSC;
    Record.TSC = NewTSC;
  }
  return Error::success();
}

/// Reads a log in FDR mode for version 1 of this binary format. FDR mode is
/// defined as part of the compiler-rt project in xray_fdr_logging.h, and such
/// a log consists of the familiar 32 bit XRayHeader, followed by sequences of
/// of interspersed 16 byte Metadata Records and 8 byte Function Records.
///
/// The following is an attempt to document the grammar of the format, which is
/// parsed by this function for little-endian machines. Since the format makes
/// use of BitFields, when we support big-endian architectures, we will need to
/// adjust not only the endianness parameter to llvm's RecordExtractor, but also
/// the bit twiddling logic, which is consistent with the little-endian
/// convention that BitFields within a struct will first be packed into the
/// least significant bits the address they belong to.
///
/// We expect a format complying with the grammar in the following pseudo-EBNF
/// in Version 1 of the FDR log.
///
/// FDRLog: XRayFileHeader ThreadBuffer*
/// XRayFileHeader: 32 bytes to identify the log as FDR with machine metadata.
///     Includes BufferSize
/// ThreadBuffer: NewBuffer WallClockTime NewCPUId FunctionSequence EOB
/// BufSize: 8 byte unsigned integer indicating how large the buffer is.
/// NewBuffer: 16 byte metadata record with Thread Id.
/// WallClockTime: 16 byte metadata record with human readable time.
/// Pid: 16 byte metadata record with Pid
/// NewCPUId: 16 byte metadata record with CPUId and a 64 bit TSC reading.
/// EOB: 16 byte record in a thread buffer plus mem garbage to fill BufSize.
/// FunctionSequence: NewCPUId | TSCWrap | FunctionRecord
/// TSCWrap: 16 byte metadata record with a full 64 bit TSC reading.
/// FunctionRecord: 8 byte record with FunctionId, entry/exit, and TSC delta.
///
/// In Version 2, we make the following changes:
///
/// ThreadBuffer: BufferExtents NewBuffer WallClockTime NewCPUId
///               FunctionSequence
/// BufferExtents: 16 byte metdata record describing how many usable bytes are
///                in the buffer. This is measured from the start of the buffer
///                and must always be at least 48 (bytes).
///
/// In Version 3, we make the following changes:
///
/// ThreadBuffer: BufferExtents NewBuffer WallClockTime Pid NewCPUId
///               FunctionSequence
/// EOB: *deprecated*
Error loadFDRLog(StringRef Data, XRayFileHeader &FileHeader,
                 std::vector<XRayRecord> &Records) {
  if (Data.size() < 32)
    return make_error<StringError>(
        "Not enough bytes for an XRay log.",
        std::make_error_code(std::errc::invalid_argument));

  // For an FDR log, there are records sized 16 and 8 bytes.
  // There actually may be no records if no non-trivial functions are
  // instrumented.
  if (Data.size() % 8 != 0)
    return make_error<StringError>(
        "Invalid-sized XRay data.",
        std::make_error_code(std::errc::invalid_argument));

  if (auto E = readBinaryFormatHeader(Data, FileHeader))
    return E;

  uint64_t BufferSize = 0;
  {
    StringRef ExtraDataRef(FileHeader.FreeFormData, 16);
    DataExtractor ExtraDataExtractor(ExtraDataRef, true, 8);
    uint32_t ExtraDataOffset = 0;
    BufferSize = ExtraDataExtractor.getU64(&ExtraDataOffset);
  }

  FDRState::Token InitialExpectation;
  switch (FileHeader.Version) {
  case 1:
    InitialExpectation = FDRState::Token::NEW_BUFFER_RECORD_OR_EOF;
    break;
  case 2:
  case 3:
    InitialExpectation = FDRState::Token::BUFFER_EXTENTS;
    break;
  default:
    return make_error<StringError>(
        Twine("Unsupported version '") + Twine(FileHeader.Version) + "'",
        std::make_error_code(std::errc::executable_format_error));
  }
  FDRState State{0, 0, 0, 0, InitialExpectation, BufferSize, 0};

  // RecordSize will tell the loop how far to seek ahead based on the record
  // type that we have just read.
  size_t RecordSize = 0;
  for (auto S = Data.drop_front(32); !S.empty(); S = S.drop_front(RecordSize)) {
    DataExtractor RecordExtractor(S, true, 8);
    uint32_t OffsetPtr = 0;
    if (State.Expects == FDRState::Token::SCAN_TO_END_OF_THREAD_BUF) {
      RecordSize = State.CurrentBufferSize - State.CurrentBufferConsumed;
      if (S.size() < RecordSize) {
        return make_error<StringError>(
            Twine("Incomplete thread buffer. Expected at least ") +
                Twine(RecordSize) + " bytes but found " + Twine(S.size()),
            make_error_code(std::errc::invalid_argument));
      }
      State.CurrentBufferConsumed = 0;
      State.Expects = FDRState::Token::NEW_BUFFER_RECORD_OR_EOF;
      continue;
    }
    uint8_t BitField = RecordExtractor.getU8(&OffsetPtr);
    bool isMetadataRecord = BitField & 0x01uL;
    bool isBufferExtents =
        (BitField >> 1) == 7; // BufferExtents record kind == 7
    if (isMetadataRecord) {
      RecordSize = 16;
      if (auto E =
              processFDRMetadataRecord(State, BitField, RecordExtractor,
                                       RecordSize, Records, FileHeader.Version))
        return E;
    } else { // Process Function Record
      RecordSize = 8;
      if (auto E = processFDRFunctionRecord(State, BitField, RecordExtractor,
                                            Records))
        return E;
    }

    // The BufferExtents record is technically not part of the buffer, so we
    // don't count the size of that record against the buffer's actual size.
    if (!isBufferExtents)
      State.CurrentBufferConsumed += RecordSize;
    assert(State.CurrentBufferConsumed <= State.CurrentBufferSize);
    if ((FileHeader.Version == 2 || FileHeader.Version == 3) &&
        State.CurrentBufferSize == State.CurrentBufferConsumed) {
      // In Version 2 of the log, we don't need to scan to the end of the thread
      // buffer if we've already consumed all the bytes we need to.
      State.Expects = FDRState::Token::BUFFER_EXTENTS;
      State.CurrentBufferSize = BufferSize;
      State.CurrentBufferConsumed = 0;
    }
  }

  // Having iterated over everything we've been given, we've either consumed
  // everything and ended up in the end state, or were told to skip the rest.
  bool Finished = State.Expects == FDRState::Token::SCAN_TO_END_OF_THREAD_BUF &&
                  State.CurrentBufferSize == State.CurrentBufferConsumed;
  if ((State.Expects != FDRState::Token::NEW_BUFFER_RECORD_OR_EOF &&
       State.Expects != FDRState::Token::BUFFER_EXTENTS) &&
      !Finished)
    return make_error<StringError>(
        Twine("Encountered EOF with unexpected state expectation ") +
            fdrStateToTwine(State.Expects) +
            ". Remaining expected bytes in thread buffer total " +
            Twine(State.CurrentBufferSize - State.CurrentBufferConsumed),
        std::make_error_code(std::errc::executable_format_error));

  return Error::success();
}

Error loadYAMLLog(StringRef Data, XRayFileHeader &FileHeader,
                  std::vector<XRayRecord> &Records) {
  YAMLXRayTrace Trace;
  Input In(Data);
  In >> Trace;
  if (In.error())
    return make_error<StringError>("Failed loading YAML Data.", In.error());

  FileHeader.Version = Trace.Header.Version;
  FileHeader.Type = Trace.Header.Type;
  FileHeader.ConstantTSC = Trace.Header.ConstantTSC;
  FileHeader.NonstopTSC = Trace.Header.NonstopTSC;
  FileHeader.CycleFrequency = Trace.Header.CycleFrequency;

  if (FileHeader.Version != 1)
    return make_error<StringError>(
        Twine("Unsupported XRay file version: ") + Twine(FileHeader.Version),
        std::make_error_code(std::errc::invalid_argument));

  Records.clear();
  std::transform(Trace.Records.begin(), Trace.Records.end(),
                 std::back_inserter(Records), [&](const YAMLXRayRecord &R) {
                   return XRayRecord{R.RecordType, R.CPU, R.Type, R.FuncId,
                                     R.TSC,        R.TId, R.PId,  R.CallArgs};
                 });
  return Error::success();
}
} // namespace

Expected<Trace> llvm::xray::loadTraceFile(StringRef Filename, bool Sort) {
  int Fd;
  if (auto EC = sys::fs::openFileForRead(Filename, Fd)) {
    return make_error<StringError>(
        Twine("Cannot read log from '") + Filename + "'", EC);
  }

  uint64_t FileSize;
  if (auto EC = sys::fs::file_size(Filename, FileSize)) {
    return make_error<StringError>(
        Twine("Cannot read log from '") + Filename + "'", EC);
  }
  if (FileSize < 4) {
    return make_error<StringError>(
        Twine("File '") + Filename + "' too small for XRay.",
        std::make_error_code(std::errc::executable_format_error));
  }

  // Map the opened file into memory and use a StringRef to access it later.
  std::error_code EC;
  sys::fs::mapped_file_region MappedFile(
      Fd, sys::fs::mapped_file_region::mapmode::readonly, FileSize, 0, EC);
  if (EC) {
    return make_error<StringError>(
        Twine("Cannot read log from '") + Filename + "'", EC);
  }
  auto Data = StringRef(MappedFile.data(), MappedFile.size());

  // Attempt to detect the file type using file magic. We have a slight bias
  // towards the binary format, and we do this by making sure that the first 4
  // bytes of the binary file is some combination of the following byte
  // patterns: (observe the code loading them assumes they're little endian)
  //
  //   0x01 0x00 0x00 0x00 - version 1, "naive" format
  //   0x01 0x00 0x01 0x00 - version 1, "flight data recorder" format
  //   0x02 0x00 0x01 0x00 - version 2, "flight data recorder" format
  //
  // YAML files don't typically have those first four bytes as valid text so we
  // try loading assuming YAML if we don't find these bytes.
  //
  // Only if we can't load either the binary or the YAML format will we yield an
  // error.
  StringRef Magic(MappedFile.data(), 4);
  DataExtractor HeaderExtractor(Magic, true, 8);
  uint32_t OffsetPtr = 0;
  uint16_t Version = HeaderExtractor.getU16(&OffsetPtr);
  uint16_t Type = HeaderExtractor.getU16(&OffsetPtr);

  enum BinaryFormatType { NAIVE_FORMAT = 0, FLIGHT_DATA_RECORDER_FORMAT = 1 };

  Trace T;
  switch (Type) {
  case NAIVE_FORMAT:
    if (Version == 1 || Version == 2 || Version == 3) {
      if (auto E = loadNaiveFormatLog(Data, T.FileHeader, T.Records))
        return std::move(E);
    } else {
      return make_error<StringError>(
          Twine("Unsupported version for Basic/Naive Mode logging: ") +
              Twine(Version),
          std::make_error_code(std::errc::executable_format_error));
    }
    break;
  case FLIGHT_DATA_RECORDER_FORMAT:
    if (Version == 1 || Version == 2 || Version == 3) {
      if (auto E = loadFDRLog(Data, T.FileHeader, T.Records))
        return std::move(E);
    } else {
      return make_error<StringError>(
          Twine("Unsupported version for FDR Mode logging: ") + Twine(Version),
          std::make_error_code(std::errc::executable_format_error));
    }
    break;
  default:
    if (auto E = loadYAMLLog(Data, T.FileHeader, T.Records))
      return std::move(E);
  }

  if (Sort)
    std::stable_sort(T.Records.begin(), T.Records.end(),
              [&](const XRayRecord &L, const XRayRecord &R) {
                return L.TSC < R.TSC;
              });

  return std::move(T);
}

//===- Config.h -------------------------------------------------*- C++ -*-===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_ELF_CONFIG_H
#define LLD_ELF_CONFIG_H

#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/CachePruning.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/Endian.h"

#include <vector>

namespace lld {
namespace elf {

class InputFile;
struct Symbol;

enum ELFKind {
  ELFNoneKind,
  ELF32LEKind,
  ELF32BEKind,
  ELF64LEKind,
  ELF64BEKind
};

// For --build-id.
enum class BuildIdKind { None, Fast, Md5, Sha1, Hexstring, Uuid };

// For --discard-{all,locals,none}.
enum class DiscardPolicy { Default, All, Locals, None };

// For --strip-{all,debug}.
enum class StripPolicy { None, All, Debug };

// For --unresolved-symbols.
enum class UnresolvedPolicy { ReportError, Warn, WarnAll, Ignore, IgnoreAll };

// For --sort-section and linkerscript sorting rules.
enum class SortSectionPolicy { Default, None, Alignment, Name, Priority };

// For --target2
enum class Target2Policy { Abs, Rel, GotRel };

struct SymbolVersion {
  llvm::StringRef Name;
  bool IsExternCpp;
  bool HasWildcard;
};

// This struct contains symbols version definition that
// can be found in version script if it is used for link.
struct VersionDefinition {
  llvm::StringRef Name;
  uint16_t Id = 0;
  std::vector<SymbolVersion> Globals;
  size_t NameOff = 0; // Offset in the string table
};

// Structure for mapping renamed symbols
struct RenamedSymbol {
  Symbol *Target;
  uint8_t OriginalBinding;
};

// This struct contains the global configuration for the linker.
// Most fields are direct mapping from the command line options
// and such fields have the same name as the corresponding options.
// Most fields are initialized by the driver.
struct Configuration {
  InputFile *FirstElf = nullptr;
  uint8_t OSABI = 0;
  llvm::CachePruningPolicy ThinLTOCachePolicy;
  llvm::StringMap<uint64_t> SectionStartMap;
  llvm::StringRef DynamicLinker;
  llvm::StringRef Entry;
  llvm::StringRef Emulation;
  llvm::StringRef Fini;
  llvm::StringRef Init;
  llvm::StringRef LTOAAPipeline;
  llvm::StringRef LTONewPmPasses;
  llvm::StringRef MapFile;
  llvm::StringRef OutputFile;
  llvm::StringRef OptRemarksFilename;
  llvm::StringRef SoName;
  llvm::StringRef Sysroot;
  llvm::StringRef ThinLTOCacheDir;
  std::string Rpath;
  std::vector<VersionDefinition> VersionDefinitions;
  std::vector<llvm::StringRef> Argv;
  std::vector<llvm::StringRef> AuxiliaryList;
  std::vector<llvm::StringRef> FilterList;
  std::vector<llvm::StringRef> SearchPaths;
  std::vector<llvm::StringRef> SymbolOrderingFile;
  std::vector<llvm::StringRef> Undefined;
  std::vector<SymbolVersion> VersionScriptGlobals;
  std::vector<SymbolVersion> VersionScriptLocals;
  std::vector<uint8_t> BuildIdVector;
  llvm::MapVector<Symbol *, RenamedSymbol> RenamedSymbols;
  bool AllowMultipleDefinition;
  bool AsNeeded = false;
  bool Bsymbolic;
  bool BsymbolicFunctions;
  bool ColorDiagnostics = false;
  bool CompressDebugSections;
  bool DefineCommon;
  bool Demangle = true;
  bool DisableVerify;
  bool EhFrameHdr;
  bool EmitRelocs;
  bool EnableNewDtags;
  bool ExportDynamic;
  bool FatalWarnings;
  bool GcSections;
  bool GdbIndex;
  bool GnuHash;
  bool ICF;
  bool MipsN32Abi = false;
  bool NoGnuUnique;
  bool NoUndefinedVersion;
  bool Nostdlib;
  bool OFormatBinary;
  bool Omagic;
  bool OptRemarksWithHotness;
  bool Pie;
  bool PrintGcSections;
  bool Relocatable;
  bool SaveTemps;
  bool SingleRoRx;
  bool Shared;
  bool Static = false;
  bool SysvHash;
  bool Target1Rel;
  bool Threads;
  bool Trace;
  bool Verbose;
  bool WarnCommon;
  bool WarnMissingEntry;
  bool ZCombreloc;
  bool ZExecstack;
  bool ZNocopyreloc;
  bool ZNodelete;
  bool ZNodlopen;
  bool ZNow;
  bool ZOrigin;
  bool ZRelro;
  bool ZRodynamic;
  bool ZText;
  bool ExitEarly;
  bool ZWxneeded;
  DiscardPolicy Discard;
  SortSectionPolicy SortSection;
  StripPolicy Strip;
  UnresolvedPolicy UnresolvedSymbols;
  Target2Policy Target2;
  BuildIdKind BuildId = BuildIdKind::None;
  ELFKind EKind = ELFNoneKind;
  uint16_t DefaultSymbolVersion = llvm::ELF::VER_NDX_GLOBAL;
  uint16_t EMachine = llvm::ELF::EM_NONE;
  uint64_t ErrorLimit = 20;
  uint64_t ImageBase;
  uint64_t MaxPageSize;
  uint64_t ZStackSize;
  unsigned LTOPartitions;
  unsigned LTOO;
  unsigned Optimize;
  unsigned ThinLTOJobs;

  // The following config options do not directly correspond to any
  // particualr command line options.

  // True if we need to pass through relocations in input files to the
  // output file. Usually false because we consume relocations.
  bool CopyRelocs;

  // True if the target is ELF64. False if ELF32.
  bool Is64;

  // True if the target is little-endian. False if big-endian.
  bool IsLE;

  // endianness::little if IsLE is true. endianness::big otherwise.
  llvm::support::endianness Endianness;

  // True if the target is the little-endian MIPS64.
  //
  // The reason why we have this variable only for the MIPS is because
  // we use this often.  Some ELF headers for MIPS64EL are in a
  // mixed-endian (which is horrible and I'd say that's a serious spec
  // bug), and we need to know whether we are reading MIPS ELF files or
  // not in various places.
  //
  // (Note that MIPS64EL is not a typo for MIPS64LE. This is the official
  // name whatever that means. A fun hypothesis is that "EL" is short for
  // little-endian written in the little-endian order, but I don't know
  // if that's true.)
  bool IsMips64EL;

  // The ELF spec defines two types of relocation table entries, RELA and
  // REL. RELA is a triplet of (offset, info, addend) while REL is a
  // tuple of (offset, info). Addends for REL are implicit and read from
  // the location where the relocations are applied. So, REL is more
  // compact than RELA but requires a bit of more work to process.
  //
  // (From the linker writer's view, this distinction is not necessary.
  // If the ELF had chosen whichever and sticked with it, it would have
  // been easier to write code to process relocations, but it's too late
  // to change the spec.)
  //
  // Each ABI defines its relocation type. IsRela is true if target
  // uses RELA. As far as we know, all 64-bit ABIs are using RELA. A
  // few 32-bit ABIs are using RELA too.
  bool IsRela;

  // True if we are creating position-independent code.
  bool Pic;

  // 4 for ELF32, 8 for ELF64.
  int Wordsize;
};

// The only instance of Configuration struct.
extern Configuration *Config;

} // namespace elf
} // namespace lld

#endif

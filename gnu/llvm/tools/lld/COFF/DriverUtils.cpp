//===- DriverUtils.cpp ----------------------------------------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains utility functions for the driver. Because there
// are so many small functions, we created this separate file to make
// Driver.cpp less cluttered.
//
//===----------------------------------------------------------------------===//

#include "Config.h"
#include "Driver.h"
#include "Error.h"
#include "Memory.h"
#include "Symbols.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/BinaryFormat/COFF.h"
#include "llvm/Object/COFF.h"
#include "llvm/Object/WindowsResource.h"
#include "llvm/Option/Arg.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Option/Option.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

using namespace llvm::COFF;
using namespace llvm;
using llvm::cl::ExpandResponseFiles;
using llvm::cl::TokenizeWindowsCommandLine;
using llvm::sys::Process;

namespace lld {
namespace coff {
namespace {

const uint16_t SUBLANG_ENGLISH_US = 0x0409;
const uint16_t RT_MANIFEST = 24;

class Executor {
public:
  explicit Executor(StringRef S) : Prog(Saver.save(S)) {}
  void add(StringRef S) { Args.push_back(Saver.save(S)); }
  void add(std::string &S) { Args.push_back(Saver.save(S)); }
  void add(Twine S) { Args.push_back(Saver.save(S)); }
  void add(const char *S) { Args.push_back(Saver.save(S)); }

  void run() {
    ErrorOr<std::string> ExeOrErr = sys::findProgramByName(Prog);
    if (auto EC = ExeOrErr.getError())
      fatal(EC, "unable to find " + Prog + " in PATH: ");
    StringRef Exe = Saver.save(*ExeOrErr);
    Args.insert(Args.begin(), Exe);

    std::vector<const char *> Vec;
    for (StringRef S : Args)
      Vec.push_back(S.data());
    Vec.push_back(nullptr);

    if (sys::ExecuteAndWait(Args[0], Vec.data()) != 0)
      fatal("ExecuteAndWait failed: " +
            llvm::join(Args.begin(), Args.end(), " "));
  }

private:
  StringRef Prog;
  std::vector<StringRef> Args;
};

} // anonymous namespace

// Returns /machine's value.
MachineTypes getMachineType(StringRef S) {
  MachineTypes MT = StringSwitch<MachineTypes>(S.lower())
                        .Cases("x64", "amd64", AMD64)
                        .Cases("x86", "i386", I386)
                        .Case("arm", ARMNT)
                        .Case("arm64", ARM64)
                        .Default(IMAGE_FILE_MACHINE_UNKNOWN);
  if (MT != IMAGE_FILE_MACHINE_UNKNOWN)
    return MT;
  fatal("unknown /machine argument: " + S);
}

StringRef machineToStr(MachineTypes MT) {
  switch (MT) {
  case ARMNT:
    return "arm";
  case ARM64:
    return "arm64";
  case AMD64:
    return "x64";
  case I386:
    return "x86";
  default:
    llvm_unreachable("unknown machine type");
  }
}

// Parses a string in the form of "<integer>[,<integer>]".
void parseNumbers(StringRef Arg, uint64_t *Addr, uint64_t *Size) {
  StringRef S1, S2;
  std::tie(S1, S2) = Arg.split(',');
  if (S1.getAsInteger(0, *Addr))
    fatal("invalid number: " + S1);
  if (Size && !S2.empty() && S2.getAsInteger(0, *Size))
    fatal("invalid number: " + S2);
}

// Parses a string in the form of "<integer>[.<integer>]".
// If second number is not present, Minor is set to 0.
void parseVersion(StringRef Arg, uint32_t *Major, uint32_t *Minor) {
  StringRef S1, S2;
  std::tie(S1, S2) = Arg.split('.');
  if (S1.getAsInteger(0, *Major))
    fatal("invalid number: " + S1);
  *Minor = 0;
  if (!S2.empty() && S2.getAsInteger(0, *Minor))
    fatal("invalid number: " + S2);
}

// Parses a string in the form of "<subsystem>[,<integer>[.<integer>]]".
void parseSubsystem(StringRef Arg, WindowsSubsystem *Sys, uint32_t *Major,
                    uint32_t *Minor) {
  StringRef SysStr, Ver;
  std::tie(SysStr, Ver) = Arg.split(',');
  *Sys = StringSwitch<WindowsSubsystem>(SysStr.lower())
    .Case("boot_application", IMAGE_SUBSYSTEM_WINDOWS_BOOT_APPLICATION)
    .Case("console", IMAGE_SUBSYSTEM_WINDOWS_CUI)
    .Case("efi_application", IMAGE_SUBSYSTEM_EFI_APPLICATION)
    .Case("efi_boot_service_driver", IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER)
    .Case("efi_rom", IMAGE_SUBSYSTEM_EFI_ROM)
    .Case("efi_runtime_driver", IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER)
    .Case("native", IMAGE_SUBSYSTEM_NATIVE)
    .Case("posix", IMAGE_SUBSYSTEM_POSIX_CUI)
    .Case("windows", IMAGE_SUBSYSTEM_WINDOWS_GUI)
    .Default(IMAGE_SUBSYSTEM_UNKNOWN);
  if (*Sys == IMAGE_SUBSYSTEM_UNKNOWN)
    fatal("unknown subsystem: " + SysStr);
  if (!Ver.empty())
    parseVersion(Ver, Major, Minor);
}

// Parse a string of the form of "<from>=<to>".
// Results are directly written to Config.
void parseAlternateName(StringRef S) {
  StringRef From, To;
  std::tie(From, To) = S.split('=');
  if (From.empty() || To.empty())
    fatal("/alternatename: invalid argument: " + S);
  auto It = Config->AlternateNames.find(From);
  if (It != Config->AlternateNames.end() && It->second != To)
    fatal("/alternatename: conflicts: " + S);
  Config->AlternateNames.insert(It, std::make_pair(From, To));
}

// Parse a string of the form of "<from>=<to>".
// Results are directly written to Config.
void parseMerge(StringRef S) {
  StringRef From, To;
  std::tie(From, To) = S.split('=');
  if (From.empty() || To.empty())
    fatal("/merge: invalid argument: " + S);
  auto Pair = Config->Merge.insert(std::make_pair(From, To));
  bool Inserted = Pair.second;
  if (!Inserted) {
    StringRef Existing = Pair.first->second;
    if (Existing != To)
      warn(S + ": already merged into " + Existing);
  }
}

static uint32_t parseSectionAttributes(StringRef S) {
  uint32_t Ret = 0;
  for (char C : S.lower()) {
    switch (C) {
    case 'd':
      Ret |= IMAGE_SCN_MEM_DISCARDABLE;
      break;
    case 'e':
      Ret |= IMAGE_SCN_MEM_EXECUTE;
      break;
    case 'k':
      Ret |= IMAGE_SCN_MEM_NOT_CACHED;
      break;
    case 'p':
      Ret |= IMAGE_SCN_MEM_NOT_PAGED;
      break;
    case 'r':
      Ret |= IMAGE_SCN_MEM_READ;
      break;
    case 's':
      Ret |= IMAGE_SCN_MEM_SHARED;
      break;
    case 'w':
      Ret |= IMAGE_SCN_MEM_WRITE;
      break;
    default:
      fatal("/section: invalid argument: " + S);
    }
  }
  return Ret;
}

// Parses /section option argument.
void parseSection(StringRef S) {
  StringRef Name, Attrs;
  std::tie(Name, Attrs) = S.split(',');
  if (Name.empty() || Attrs.empty())
    fatal("/section: invalid argument: " + S);
  Config->Section[Name] = parseSectionAttributes(Attrs);
}

// Parses a string in the form of "EMBED[,=<integer>]|NO".
// Results are directly written to Config.
void parseManifest(StringRef Arg) {
  if (Arg.equals_lower("no")) {
    Config->Manifest = Configuration::No;
    return;
  }
  if (!Arg.startswith_lower("embed"))
    fatal("invalid option " + Arg);
  Config->Manifest = Configuration::Embed;
  Arg = Arg.substr(strlen("embed"));
  if (Arg.empty())
    return;
  if (!Arg.startswith_lower(",id="))
    fatal("invalid option " + Arg);
  Arg = Arg.substr(strlen(",id="));
  if (Arg.getAsInteger(0, Config->ManifestID))
    fatal("invalid option " + Arg);
}

// Parses a string in the form of "level=<string>|uiAccess=<string>|NO".
// Results are directly written to Config.
void parseManifestUAC(StringRef Arg) {
  if (Arg.equals_lower("no")) {
    Config->ManifestUAC = false;
    return;
  }
  for (;;) {
    Arg = Arg.ltrim();
    if (Arg.empty())
      return;
    if (Arg.startswith_lower("level=")) {
      Arg = Arg.substr(strlen("level="));
      std::tie(Config->ManifestLevel, Arg) = Arg.split(" ");
      continue;
    }
    if (Arg.startswith_lower("uiaccess=")) {
      Arg = Arg.substr(strlen("uiaccess="));
      std::tie(Config->ManifestUIAccess, Arg) = Arg.split(" ");
      continue;
    }
    fatal("invalid option " + Arg);
  }
}

// An RAII temporary file class that automatically removes a temporary file.
namespace {
class TemporaryFile {
public:
  TemporaryFile(StringRef Prefix, StringRef Extn, StringRef Contents = "") {
    SmallString<128> S;
    if (auto EC = sys::fs::createTemporaryFile("lld-" + Prefix, Extn, S))
      fatal(EC, "cannot create a temporary file");
    Path = S.str();

    if (!Contents.empty()) {
      std::error_code EC;
      raw_fd_ostream OS(Path, EC, sys::fs::F_None);
      if (EC)
        fatal(EC, "failed to open " + Path);
      OS << Contents;
    }
  }

  TemporaryFile(TemporaryFile &&Obj) {
    std::swap(Path, Obj.Path);
  }

  ~TemporaryFile() {
    if (Path.empty())
      return;
    if (sys::fs::remove(Path))
      fatal("failed to remove " + Path);
  }

  // Returns a memory buffer of this temporary file.
  // Note that this function does not leave the file open,
  // so it is safe to remove the file immediately after this function
  // is called (you cannot remove an opened file on Windows.)
  std::unique_ptr<MemoryBuffer> getMemoryBuffer() {
    // IsVolatileSize=true forces MemoryBuffer to not use mmap().
    return check(MemoryBuffer::getFile(Path, /*FileSize=*/-1,
                                       /*RequiresNullTerminator=*/false,
                                       /*IsVolatileSize=*/true),
                 "could not open " + Path);
  }

  std::string Path;
};
}

// Create the default manifest file as a temporary file.
TemporaryFile createDefaultXml() {
  // Create a temporary file.
  TemporaryFile File("defaultxml", "manifest");

  // Open the temporary file for writing.
  std::error_code EC;
  raw_fd_ostream OS(File.Path, EC, sys::fs::F_Text);
  if (EC)
    fatal(EC, "failed to open " + File.Path);

  // Emit the XML. Note that we do *not* verify that the XML attributes are
  // syntactically correct. This is intentional for link.exe compatibility.
  OS << "<?xml version=\"1.0\" standalone=\"yes\"?>\n"
     << "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\"\n"
     << "          manifestVersion=\"1.0\">\n";
  if (Config->ManifestUAC) {
    OS << "  <trustInfo>\n"
       << "    <security>\n"
       << "      <requestedPrivileges>\n"
       << "         <requestedExecutionLevel level=" << Config->ManifestLevel
       << " uiAccess=" << Config->ManifestUIAccess << "/>\n"
       << "      </requestedPrivileges>\n"
       << "    </security>\n"
       << "  </trustInfo>\n";
    if (!Config->ManifestDependency.empty()) {
      OS << "  <dependency>\n"
         << "    <dependentAssembly>\n"
         << "      <assemblyIdentity " << Config->ManifestDependency << " />\n"
         << "    </dependentAssembly>\n"
         << "  </dependency>\n";
    }
  }
  OS << "</assembly>\n";
  OS.close();
  return File;
}

static std::string readFile(StringRef Path) {
  std::unique_ptr<MemoryBuffer> MB =
      check(MemoryBuffer::getFile(Path), "could not open " + Path);
  return MB->getBuffer();
}

static std::string createManifestXml() {
  // Create the default manifest file.
  TemporaryFile File1 = createDefaultXml();
  if (Config->ManifestInput.empty())
    return readFile(File1.Path);

  // If manifest files are supplied by the user using /MANIFESTINPUT
  // option, we need to merge them with the default manifest.
  TemporaryFile File2("user", "manifest");

  Executor E("mt.exe");
  E.add("/manifest");
  E.add(File1.Path);
  for (StringRef Filename : Config->ManifestInput) {
    E.add("/manifest");
    E.add(Filename);
  }
  E.add("/nologo");
  E.add("/out:" + StringRef(File2.Path));
  E.run();
  return readFile(File2.Path);
}

static std::unique_ptr<MemoryBuffer>
createMemoryBufferForManifestRes(size_t ManifestSize) {
  size_t ResSize = alignTo(
      object::WIN_RES_MAGIC_SIZE + object::WIN_RES_NULL_ENTRY_SIZE +
          sizeof(object::WinResHeaderPrefix) + sizeof(object::WinResIDs) +
          sizeof(object::WinResHeaderSuffix) + ManifestSize,
      object::WIN_RES_DATA_ALIGNMENT);
  return MemoryBuffer::getNewMemBuffer(ResSize);
}

static void writeResFileHeader(char *&Buf) {
  memcpy(Buf, COFF::WinResMagic, sizeof(COFF::WinResMagic));
  Buf += sizeof(COFF::WinResMagic);
  memset(Buf, 0, object::WIN_RES_NULL_ENTRY_SIZE);
  Buf += object::WIN_RES_NULL_ENTRY_SIZE;
}

static void writeResEntryHeader(char *&Buf, size_t ManifestSize) {
  // Write the prefix.
  auto *Prefix = reinterpret_cast<object::WinResHeaderPrefix *>(Buf);
  Prefix->DataSize = ManifestSize;
  Prefix->HeaderSize = sizeof(object::WinResHeaderPrefix) +
                       sizeof(object::WinResIDs) +
                       sizeof(object::WinResHeaderSuffix);
  Buf += sizeof(object::WinResHeaderPrefix);

  // Write the Type/Name IDs.
  auto *IDs = reinterpret_cast<object::WinResIDs *>(Buf);
  IDs->setType(RT_MANIFEST);
  IDs->setName(Config->ManifestID);
  Buf += sizeof(object::WinResIDs);

  // Write the suffix.
  auto *Suffix = reinterpret_cast<object::WinResHeaderSuffix *>(Buf);
  Suffix->DataVersion = 0;
  Suffix->MemoryFlags = object::WIN_RES_PURE_MOVEABLE;
  Suffix->Language = SUBLANG_ENGLISH_US;
  Suffix->Version = 0;
  Suffix->Characteristics = 0;
  Buf += sizeof(object::WinResHeaderSuffix);
}

// Create a resource file containing a manifest XML.
std::unique_ptr<MemoryBuffer> createManifestRes() {
  std::string Manifest = createManifestXml();

  std::unique_ptr<MemoryBuffer> Res =
      createMemoryBufferForManifestRes(Manifest.size());

  char *Buf = const_cast<char *>(Res->getBufferStart());
  writeResFileHeader(Buf);
  writeResEntryHeader(Buf, Manifest.size());

  // Copy the manifest data into the .res file.
  std::copy(Manifest.begin(), Manifest.end(), Buf);
  return Res;
}

void createSideBySideManifest() {
  std::string Path = Config->ManifestFile;
  if (Path == "")
    Path = Config->OutputFile + ".manifest";
  std::error_code EC;
  raw_fd_ostream Out(Path, EC, sys::fs::F_Text);
  if (EC)
    fatal(EC, "failed to create manifest");
  Out << createManifestXml();
}

// Parse a string in the form of
// "<name>[=<internalname>][,@ordinal[,NONAME]][,DATA][,PRIVATE]"
// or "<name>=<dllname>.<name>".
// Used for parsing /export arguments.
Export parseExport(StringRef Arg) {
  Export E;
  StringRef Rest;
  std::tie(E.Name, Rest) = Arg.split(",");
  if (E.Name.empty())
    goto err;

  if (E.Name.find('=') != StringRef::npos) {
    StringRef X, Y;
    std::tie(X, Y) = E.Name.split("=");

    // If "<name>=<dllname>.<name>".
    if (Y.find(".") != StringRef::npos) {
      E.Name = X;
      E.ForwardTo = Y;
      return E;
    }

    E.ExtName = X;
    E.Name = Y;
    if (E.Name.empty())
      goto err;
  }

  // If "<name>=<internalname>[,@ordinal[,NONAME]][,DATA][,PRIVATE]"
  while (!Rest.empty()) {
    StringRef Tok;
    std::tie(Tok, Rest) = Rest.split(",");
    if (Tok.equals_lower("noname")) {
      if (E.Ordinal == 0)
        goto err;
      E.Noname = true;
      continue;
    }
    if (Tok.equals_lower("data")) {
      E.Data = true;
      continue;
    }
    if (Tok.equals_lower("constant")) {
      E.Constant = true;
      continue;
    }
    if (Tok.equals_lower("private")) {
      E.Private = true;
      continue;
    }
    if (Tok.startswith("@")) {
      int32_t Ord;
      if (Tok.substr(1).getAsInteger(0, Ord))
        goto err;
      if (Ord <= 0 || 65535 < Ord)
        goto err;
      E.Ordinal = Ord;
      continue;
    }
    goto err;
  }
  return E;

err:
  fatal("invalid /export: " + Arg);
}

static StringRef undecorate(StringRef Sym) {
  if (Config->Machine != I386)
    return Sym;
  return Sym.startswith("_") ? Sym.substr(1) : Sym;
}

// Performs error checking on all /export arguments.
// It also sets ordinals.
void fixupExports() {
  // Symbol ordinals must be unique.
  std::set<uint16_t> Ords;
  for (Export &E : Config->Exports) {
    if (E.Ordinal == 0)
      continue;
    if (!Ords.insert(E.Ordinal).second)
      fatal("duplicate export ordinal: " + E.Name);
  }

  for (Export &E : Config->Exports) {
    SymbolBody *Sym = E.Sym;
    if (!E.ForwardTo.empty() || !Sym) {
      E.SymbolName = E.Name;
    } else {
      if (auto *U = dyn_cast<Undefined>(Sym))
        if (U->WeakAlias)
          Sym = U->WeakAlias;
      E.SymbolName = Sym->getName();
    }
  }

  for (Export &E : Config->Exports) {
    if (!E.ForwardTo.empty()) {
      E.ExportName = undecorate(E.Name);
    } else {
      E.ExportName = undecorate(E.ExtName.empty() ? E.Name : E.ExtName);
    }
  }

  // Uniquefy by name.
  std::map<StringRef, Export *> Map;
  std::vector<Export> V;
  for (Export &E : Config->Exports) {
    auto Pair = Map.insert(std::make_pair(E.ExportName, &E));
    bool Inserted = Pair.second;
    if (Inserted) {
      V.push_back(E);
      continue;
    }
    Export *Existing = Pair.first->second;
    if (E == *Existing || E.Name != Existing->Name)
      continue;
    warn("duplicate /export option: " + E.Name);
  }
  Config->Exports = std::move(V);

  // Sort by name.
  std::sort(Config->Exports.begin(), Config->Exports.end(),
            [](const Export &A, const Export &B) {
              return A.ExportName < B.ExportName;
            });
}

void assignExportOrdinals() {
  // Assign unique ordinals if default (= 0).
  uint16_t Max = 0;
  for (Export &E : Config->Exports)
    Max = std::max(Max, E.Ordinal);
  for (Export &E : Config->Exports)
    if (E.Ordinal == 0)
      E.Ordinal = ++Max;
}

// Parses a string in the form of "key=value" and check
// if value matches previous values for the same key.
void checkFailIfMismatch(StringRef Arg) {
  StringRef K, V;
  std::tie(K, V) = Arg.split('=');
  if (K.empty() || V.empty())
    fatal("/failifmismatch: invalid argument: " + Arg);
  StringRef Existing = Config->MustMatch[K];
  if (!Existing.empty() && V != Existing)
    fatal("/failifmismatch: mismatch detected: " + Existing + " and " + V +
          " for key " + K);
  Config->MustMatch[K] = V;
}

// Convert Windows resource files (.res files) to a .obj file
// using cvtres.exe.
std::unique_ptr<MemoryBuffer>
convertResToCOFF(const std::vector<MemoryBufferRef> &MBs) {
  object::WindowsResourceParser Parser;

  for (MemoryBufferRef MB : MBs) {
    std::unique_ptr<object::Binary> Bin = check(object::createBinary(MB));
    object::WindowsResource *RF = dyn_cast<object::WindowsResource>(Bin.get());
    if (!RF)
      fatal("cannot compile non-resource file as resource");
    if (auto EC = Parser.parse(RF))
      fatal(EC, "failed to parse .res file");
  }

  Expected<std::unique_ptr<MemoryBuffer>> E =
      llvm::object::writeWindowsResourceCOFF(Config->Machine, Parser);
  if (!E)
    fatal(errorToErrorCode(E.takeError()), "failed to write .res to COFF");
  return std::move(E.get());
}

// Run MSVC link.exe for given in-memory object files.
// Command line options are copied from those given to LLD.
// This is for the /msvclto option.
void runMSVCLinker(std::string Rsp, ArrayRef<StringRef> Objects) {
  // Write the in-memory object files to disk.
  std::vector<TemporaryFile> Temps;
  for (StringRef S : Objects) {
    Temps.emplace_back("lto", "obj", S);
    Rsp += quote(Temps.back().Path) + "\n";
  }

  log("link.exe " + Rsp);

  // Run MSVC link.exe.
  Temps.emplace_back("lto", "rsp", Rsp);
  Executor E("link.exe");
  E.add(Twine("@" + Temps.back().Path));
  E.run();
}

// Create OptTable

// Create prefix string literals used in Options.td
#define PREFIX(NAME, VALUE) const char *const NAME[] = VALUE;
#include "Options.inc"
#undef PREFIX

// Create table mapping all options defined in Options.td
static const llvm::opt::OptTable::Info infoTable[] = {
#define OPTION(X1, X2, ID, KIND, GROUP, ALIAS, X7, X8, X9, X10, X11, X12)      \
  {X1, X2, X10,         X11,         OPT_##ID, llvm::opt::Option::KIND##Class, \
   X9, X8, OPT_##GROUP, OPT_##ALIAS, X7,       X12},
#include "Options.inc"
#undef OPTION
};

class COFFOptTable : public llvm::opt::OptTable {
public:
  COFFOptTable() : OptTable(infoTable, true) {}
};

// Parses a given list of options.
opt::InputArgList ArgParser::parse(ArrayRef<const char *> ArgsArr) {
  // First, replace respnose files (@<file>-style options).
  std::vector<const char *> Argv = replaceResponseFiles(ArgsArr);

  // Make InputArgList from string vectors.
  COFFOptTable Table;
  unsigned MissingIndex;
  unsigned MissingCount;
  opt::InputArgList Args = Table.ParseArgs(Argv, MissingIndex, MissingCount);

  // Print the real command line if response files are expanded.
  if (Args.hasArg(OPT_verbose) && ArgsArr.size() != Argv.size()) {
    std::string Msg = "Command line:";
    for (const char *S : Argv)
      Msg += " " + std::string(S);
    message(Msg);
  }

  if (MissingCount)
    fatal(Twine(Args.getArgString(MissingIndex)) + ": missing argument");
  for (auto *Arg : Args.filtered(OPT_UNKNOWN))
    warn("ignoring unknown argument: " + Arg->getSpelling());
  return Args;
}

// link.exe has an interesting feature. If LINK or _LINK_ environment
// variables exist, their contents are handled as command line strings.
// So you can pass extra arguments using them.
opt::InputArgList ArgParser::parseLINK(std::vector<const char *> Args) {
  // Concatenate LINK env and command line arguments, and then parse them.
  if (Optional<std::string> S = Process::GetEnv("LINK")) {
    std::vector<const char *> V = tokenize(*S);
    Args.insert(Args.begin(), V.begin(), V.end());
  }
  if (Optional<std::string> S = Process::GetEnv("_LINK_")) {
    std::vector<const char *> V = tokenize(*S);
    Args.insert(Args.begin(), V.begin(), V.end());
  }
  return parse(Args);
}

std::vector<const char *> ArgParser::tokenize(StringRef S) {
  SmallVector<const char *, 16> Tokens;
  cl::TokenizeWindowsCommandLine(S, Saver, Tokens);
  return std::vector<const char *>(Tokens.begin(), Tokens.end());
}

// Creates a new command line by replacing options starting with '@'
// character. '@<filename>' is replaced by the file's contents.
std::vector<const char *>
ArgParser::replaceResponseFiles(std::vector<const char *> Argv) {
  SmallVector<const char *, 256> Tokens(Argv.data(), Argv.data() + Argv.size());
  ExpandResponseFiles(Saver, TokenizeWindowsCommandLine, Tokens);
  return std::vector<const char *>(Tokens.begin(), Tokens.end());
}

void printHelp(const char *Argv0) {
  COFFOptTable Table;
  Table.PrintHelp(outs(), Argv0, "LLVM Linker", false);
}

} // namespace coff
} // namespace lld

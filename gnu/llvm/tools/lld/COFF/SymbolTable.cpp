//===- SymbolTable.cpp ----------------------------------------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "SymbolTable.h"
#include "Config.h"
#include "Driver.h"
#include "LTO.h"
#include "PDB.h"
#include "Symbols.h"
#include "lld/Common/ErrorHandler.h"
#include "lld/Common/Memory.h"
#include "lld/Common/Timer.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <utility>

using namespace llvm;

namespace lld {
namespace coff {

static Timer LTOTimer("LTO", Timer::root());

SymbolTable *Symtab;

void SymbolTable::addFile(InputFile *File) {
  log("Reading " + toString(File));
  File->parse();

  MachineTypes MT = File->getMachineType();
  if (Config->Machine == IMAGE_FILE_MACHINE_UNKNOWN) {
    Config->Machine = MT;
  } else if (MT != IMAGE_FILE_MACHINE_UNKNOWN && Config->Machine != MT) {
    error(toString(File) + ": machine type " + machineToStr(MT) +
          " conflicts with " + machineToStr(Config->Machine));
    return;
  }

  if (auto *F = dyn_cast<ObjFile>(File)) {
    ObjFile::Instances.push_back(F);
  } else if (auto *F = dyn_cast<BitcodeFile>(File)) {
    BitcodeFile::Instances.push_back(F);
  } else if (auto *F = dyn_cast<ImportFile>(File)) {
    ImportFile::Instances.push_back(F);
  }

  StringRef S = File->getDirectives();
  if (S.empty())
    return;

  log("Directives: " + toString(File) + ": " + S);
  Driver->parseDirectives(S);
}

static void errorOrWarn(const Twine &S) {
  if (Config->Force)
    warn(S);
  else
    error(S);
}

// Returns the name of the symbol in SC whose value is <= Addr that is closest
// to Addr. This is generally the name of the global variable or function whose
// definition contains Addr.
static StringRef getSymbolName(SectionChunk *SC, uint32_t Addr) {
  DefinedRegular *Candidate = nullptr;

  for (Symbol *S : SC->File->getSymbols()) {
    auto *D = dyn_cast_or_null<DefinedRegular>(S);
    if (!D || D->getChunk() != SC || D->getValue() > Addr ||
        (Candidate && D->getValue() < Candidate->getValue()))
      continue;

    Candidate = D;
  }

  if (!Candidate)
    return "";
  return Candidate->getName();
}

static std::string getSymbolLocations(ObjFile *File, uint32_t SymIndex) {
  struct Location {
    StringRef SymName;
    std::pair<StringRef, uint32_t> FileLine;
  };
  std::vector<Location> Locations;

  for (Chunk *C : File->getChunks()) {
    auto *SC = dyn_cast<SectionChunk>(C);
    if (!SC)
      continue;
    for (const coff_relocation &R : SC->Relocs) {
      if (R.SymbolTableIndex != SymIndex)
        continue;
      std::pair<StringRef, uint32_t> FileLine =
          getFileLine(SC, R.VirtualAddress);
      StringRef SymName = getSymbolName(SC, R.VirtualAddress);
      if (!FileLine.first.empty() || !SymName.empty())
        Locations.push_back({SymName, FileLine});
    }
  }

  if (Locations.empty())
    return "\n>>> referenced by " + toString(File) + "\n";

  std::string Out;
  llvm::raw_string_ostream OS(Out);
  for (Location Loc : Locations) {
    OS << "\n>>> referenced by ";
    if (!Loc.FileLine.first.empty())
      OS << Loc.FileLine.first << ":" << Loc.FileLine.second
         << "\n>>>               ";
    OS << toString(File);
    if (!Loc.SymName.empty())
      OS << ":(" << Loc.SymName << ')';
  }
  OS << '\n';
  return OS.str();
}

void SymbolTable::reportRemainingUndefines() {
  SmallPtrSet<Symbol *, 8> Undefs;
  DenseMap<Symbol *, Symbol *> LocalImports;

  for (auto &I : SymMap) {
    Symbol *Sym = I.second;
    auto *Undef = dyn_cast<Undefined>(Sym);
    if (!Undef)
      continue;
    if (!Sym->IsUsedInRegularObj)
      continue;

    StringRef Name = Undef->getName();

    // A weak alias may have been resolved, so check for that.
    if (Defined *D = Undef->getWeakAlias()) {
      // We want to replace Sym with D. However, we can't just blindly
      // copy sizeof(SymbolUnion) bytes from D to Sym because D may be an
      // internal symbol, and internal symbols are stored as "unparented"
      // Symbols. For that reason we need to check which type of symbol we
      // are dealing with and copy the correct number of bytes.
      if (isa<DefinedRegular>(D))
        memcpy(Sym, D, sizeof(DefinedRegular));
      else if (isa<DefinedAbsolute>(D))
        memcpy(Sym, D, sizeof(DefinedAbsolute));
      else
        memcpy(Sym, D, sizeof(SymbolUnion));
      continue;
    }

    // If we can resolve a symbol by removing __imp_ prefix, do that.
    // This odd rule is for compatibility with MSVC linker.
    if (Name.startswith("__imp_")) {
      Symbol *Imp = find(Name.substr(strlen("__imp_")));
      if (Imp && isa<Defined>(Imp)) {
        auto *D = cast<Defined>(Imp);
        replaceSymbol<DefinedLocalImport>(Sym, Name, D);
        LocalImportChunks.push_back(cast<DefinedLocalImport>(Sym)->getChunk());
        LocalImports[Sym] = D;
        continue;
      }
    }

    // Remaining undefined symbols are not fatal if /force is specified.
    // They are replaced with dummy defined symbols.
    if (Config->Force)
      replaceSymbol<DefinedAbsolute>(Sym, Name, 0);
    Undefs.insert(Sym);
  }

  if (Undefs.empty() && LocalImports.empty())
    return;

  for (Symbol *B : Config->GCRoot) {
    if (Undefs.count(B))
      errorOrWarn("<root>: undefined symbol: " + B->getName());
    if (Config->WarnLocallyDefinedImported)
      if (Symbol *Imp = LocalImports.lookup(B))
        warn("<root>: locally defined symbol imported: " + Imp->getName() +
             " (defined in " + toString(Imp->getFile()) + ") [LNK4217]");
  }

  for (ObjFile *File : ObjFile::Instances) {
    size_t SymIndex = (size_t)-1;
    for (Symbol *Sym : File->getSymbols()) {
      ++SymIndex;
      if (!Sym)
        continue;
      if (Undefs.count(Sym))
        errorOrWarn("undefined symbol: " + Sym->getName() +
                    getSymbolLocations(File, SymIndex));
      if (Config->WarnLocallyDefinedImported)
        if (Symbol *Imp = LocalImports.lookup(Sym))
          warn(toString(File) + ": locally defined symbol imported: " +
               Imp->getName() + " (defined in " + toString(Imp->getFile()) +
               ") [LNK4217]");
    }
  }
}

std::pair<Symbol *, bool> SymbolTable::insert(StringRef Name) {
  Symbol *&Sym = SymMap[CachedHashStringRef(Name)];
  if (Sym)
    return {Sym, false};
  Sym = reinterpret_cast<Symbol *>(make<SymbolUnion>());
  Sym->IsUsedInRegularObj = false;
  Sym->PendingArchiveLoad = false;
  return {Sym, true};
}

Symbol *SymbolTable::addUndefined(StringRef Name, InputFile *F,
                                  bool IsWeakAlias) {
  Symbol *S;
  bool WasInserted;
  std::tie(S, WasInserted) = insert(Name);
  if (!F || !isa<BitcodeFile>(F))
    S->IsUsedInRegularObj = true;
  if (WasInserted || (isa<Lazy>(S) && IsWeakAlias)) {
    replaceSymbol<Undefined>(S, Name);
    return S;
  }
  if (auto *L = dyn_cast<Lazy>(S)) {
    if (!S->PendingArchiveLoad) {
      S->PendingArchiveLoad = true;
      L->File->addMember(&L->Sym);
    }
  }
  return S;
}

void SymbolTable::addLazy(ArchiveFile *F, const Archive::Symbol Sym) {
  StringRef Name = Sym.getName();
  Symbol *S;
  bool WasInserted;
  std::tie(S, WasInserted) = insert(Name);
  if (WasInserted) {
    replaceSymbol<Lazy>(S, F, Sym);
    return;
  }
  auto *U = dyn_cast<Undefined>(S);
  if (!U || U->WeakAlias || S->PendingArchiveLoad)
    return;
  S->PendingArchiveLoad = true;
  F->addMember(&Sym);
}

void SymbolTable::reportDuplicate(Symbol *Existing, InputFile *NewFile) {
  error("duplicate symbol: " + toString(*Existing) + " in " +
        toString(Existing->getFile()) + " and in " + toString(NewFile));
}

Symbol *SymbolTable::addAbsolute(StringRef N, COFFSymbolRef Sym) {
  Symbol *S;
  bool WasInserted;
  std::tie(S, WasInserted) = insert(N);
  S->IsUsedInRegularObj = true;
  if (WasInserted || isa<Undefined>(S) || isa<Lazy>(S))
    replaceSymbol<DefinedAbsolute>(S, N, Sym);
  else if (!isa<DefinedCOFF>(S))
    reportDuplicate(S, nullptr);
  return S;
}

Symbol *SymbolTable::addAbsolute(StringRef N, uint64_t VA) {
  Symbol *S;
  bool WasInserted;
  std::tie(S, WasInserted) = insert(N);
  S->IsUsedInRegularObj = true;
  if (WasInserted || isa<Undefined>(S) || isa<Lazy>(S))
    replaceSymbol<DefinedAbsolute>(S, N, VA);
  else if (!isa<DefinedCOFF>(S))
    reportDuplicate(S, nullptr);
  return S;
}

Symbol *SymbolTable::addSynthetic(StringRef N, Chunk *C) {
  Symbol *S;
  bool WasInserted;
  std::tie(S, WasInserted) = insert(N);
  S->IsUsedInRegularObj = true;
  if (WasInserted || isa<Undefined>(S) || isa<Lazy>(S))
    replaceSymbol<DefinedSynthetic>(S, N, C);
  else if (!isa<DefinedCOFF>(S))
    reportDuplicate(S, nullptr);
  return S;
}

Symbol *SymbolTable::addRegular(InputFile *F, StringRef N,
                                const coff_symbol_generic *Sym,
                                SectionChunk *C) {
  Symbol *S;
  bool WasInserted;
  std::tie(S, WasInserted) = insert(N);
  if (!isa<BitcodeFile>(F))
    S->IsUsedInRegularObj = true;
  if (WasInserted || !isa<DefinedRegular>(S))
    replaceSymbol<DefinedRegular>(S, F, N, /*IsCOMDAT*/ false,
                                  /*IsExternal*/ true, Sym, C);
  else
    reportDuplicate(S, F);
  return S;
}

std::pair<Symbol *, bool>
SymbolTable::addComdat(InputFile *F, StringRef N,
                       const coff_symbol_generic *Sym) {
  Symbol *S;
  bool WasInserted;
  std::tie(S, WasInserted) = insert(N);
  if (!isa<BitcodeFile>(F))
    S->IsUsedInRegularObj = true;
  if (WasInserted || !isa<DefinedRegular>(S)) {
    replaceSymbol<DefinedRegular>(S, F, N, /*IsCOMDAT*/ true,
                                  /*IsExternal*/ true, Sym, nullptr);
    return {S, true};
  }
  if (!cast<DefinedRegular>(S)->isCOMDAT())
    reportDuplicate(S, F);
  return {S, false};
}

Symbol *SymbolTable::addCommon(InputFile *F, StringRef N, uint64_t Size,
                               const coff_symbol_generic *Sym, CommonChunk *C) {
  Symbol *S;
  bool WasInserted;
  std::tie(S, WasInserted) = insert(N);
  if (!isa<BitcodeFile>(F))
    S->IsUsedInRegularObj = true;
  if (WasInserted || !isa<DefinedCOFF>(S))
    replaceSymbol<DefinedCommon>(S, F, N, Size, Sym, C);
  else if (auto *DC = dyn_cast<DefinedCommon>(S))
    if (Size > DC->getSize())
      replaceSymbol<DefinedCommon>(S, F, N, Size, Sym, C);
  return S;
}

Symbol *SymbolTable::addImportData(StringRef N, ImportFile *F) {
  Symbol *S;
  bool WasInserted;
  std::tie(S, WasInserted) = insert(N);
  S->IsUsedInRegularObj = true;
  if (WasInserted || isa<Undefined>(S) || isa<Lazy>(S)) {
    replaceSymbol<DefinedImportData>(S, N, F);
    return S;
  }

  reportDuplicate(S, F);
  return nullptr;
}

Symbol *SymbolTable::addImportThunk(StringRef Name, DefinedImportData *ID,
                                    uint16_t Machine) {
  Symbol *S;
  bool WasInserted;
  std::tie(S, WasInserted) = insert(Name);
  S->IsUsedInRegularObj = true;
  if (WasInserted || isa<Undefined>(S) || isa<Lazy>(S)) {
    replaceSymbol<DefinedImportThunk>(S, Name, ID, Machine);
    return S;
  }

  reportDuplicate(S, ID->File);
  return nullptr;
}

std::vector<Chunk *> SymbolTable::getChunks() {
  std::vector<Chunk *> Res;
  for (ObjFile *File : ObjFile::Instances) {
    ArrayRef<Chunk *> V = File->getChunks();
    Res.insert(Res.end(), V.begin(), V.end());
  }
  return Res;
}

Symbol *SymbolTable::find(StringRef Name) {
  return SymMap.lookup(CachedHashStringRef(Name));
}

Symbol *SymbolTable::findUnderscore(StringRef Name) {
  if (Config->Machine == I386)
    return find(("_" + Name).str());
  return find(Name);
}

StringRef SymbolTable::findByPrefix(StringRef Prefix) {
  for (auto Pair : SymMap) {
    StringRef Name = Pair.first.val();
    if (Name.startswith(Prefix))
      return Name;
  }
  return "";
}

StringRef SymbolTable::findMangle(StringRef Name) {
  if (Symbol *Sym = find(Name))
    if (!isa<Undefined>(Sym))
      return Name;
  if (Config->Machine != I386)
    return findByPrefix(("?" + Name + "@@Y").str());
  if (!Name.startswith("_"))
    return "";
  // Search for x86 stdcall function.
  StringRef S = findByPrefix((Name + "@").str());
  if (!S.empty())
    return S;
  // Search for x86 fastcall function.
  S = findByPrefix(("@" + Name.substr(1) + "@").str());
  if (!S.empty())
    return S;
  // Search for x86 vectorcall function.
  S = findByPrefix((Name.substr(1) + "@@").str());
  if (!S.empty())
    return S;
  // Search for x86 C++ non-member function.
  return findByPrefix(("?" + Name.substr(1) + "@@Y").str());
}

void SymbolTable::mangleMaybe(Symbol *B) {
  auto *U = dyn_cast<Undefined>(B);
  if (!U || U->WeakAlias)
    return;
  StringRef Alias = findMangle(U->getName());
  if (!Alias.empty()) {
    log(U->getName() + " aliased to " + Alias);
    U->WeakAlias = addUndefined(Alias);
  }
}

Symbol *SymbolTable::addUndefined(StringRef Name) {
  return addUndefined(Name, nullptr, false);
}

std::vector<StringRef> SymbolTable::compileBitcodeFiles() {
  LTO.reset(new BitcodeCompiler);
  for (BitcodeFile *F : BitcodeFile::Instances)
    LTO->add(*F);
  return LTO->compile();
}

void SymbolTable::addCombinedLTOObjects() {
  if (BitcodeFile::Instances.empty())
    return;

  ScopedTimer T(LTOTimer);
  for (StringRef Object : compileBitcodeFiles()) {
    auto *Obj = make<ObjFile>(MemoryBufferRef(Object, "lto.tmp"));
    Obj->parse();
    ObjFile::Instances.push_back(Obj);
  }
}

} // namespace coff
} // namespace lld

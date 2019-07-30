//===- SymbolTable.h --------------------------------------------*- C++ -*-===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_COFF_SYMBOL_TABLE_H
#define LLD_COFF_SYMBOL_TABLE_H

#include "InputFiles.h"
#include "LTO.h"
#include "llvm/ADT/CachedHashString.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
struct LTOCodeGenerator;
}

namespace lld {
namespace coff {

class Chunk;
class CommonChunk;
class Defined;
class DefinedAbsolute;
class DefinedRelative;
class Lazy;
class SectionChunk;
class SymbolBody;
struct Symbol;

// SymbolTable is a bucket of all known symbols, including defined,
// undefined, or lazy symbols (the last one is symbols in archive
// files whose archive members are not yet loaded).
//
// We put all symbols of all files to a SymbolTable, and the
// SymbolTable selects the "best" symbols if there are name
// conflicts. For example, obviously, a defined symbol is better than
// an undefined symbol. Or, if there's a conflict between a lazy and a
// undefined, it'll read an archive member to read a real definition
// to replace the lazy symbol. The logic is implemented in the
// add*() functions, which are called by input files as they are parsed.
// There is one add* function per symbol type.
class SymbolTable {
public:
  void addFile(InputFile *File);

  // Try to resolve any undefined symbols and update the symbol table
  // accordingly, then print an error message for any remaining undefined
  // symbols.
  void reportRemainingUndefines();

  // Returns a list of chunks of selected symbols.
  std::vector<Chunk *> getChunks();

  // Returns a symbol for a given name. Returns a nullptr if not found.
  Symbol *find(StringRef Name);
  Symbol *findUnderscore(StringRef Name);

  // Occasionally we have to resolve an undefined symbol to its
  // mangled symbol. This function tries to find a mangled name
  // for U from the symbol table, and if found, set the symbol as
  // a weak alias for U.
  void mangleMaybe(SymbolBody *B);
  StringRef findMangle(StringRef Name);

  // Build a set of COFF objects representing the combined contents of
  // BitcodeFiles and add them to the symbol table. Called after all files are
  // added and before the writer writes results to a file.
  void addCombinedLTOObjects();
  std::vector<StringRef> compileBitcodeFiles();

  // The writer needs to handle DLL import libraries specially in
  // order to create the import descriptor table.
  std::vector<ImportFile *> ImportFiles;

  // The writer needs to infer the machine type from the object files.
  std::vector<ObjectFile *> ObjectFiles;

  // Creates an Undefined symbol for a given name.
  SymbolBody *addUndefined(StringRef Name);

  Symbol *addSynthetic(StringRef N, Chunk *C);
  Symbol *addAbsolute(StringRef N, uint64_t VA);

  Symbol *addUndefined(StringRef Name, InputFile *F, bool IsWeakAlias);
  void addLazy(ArchiveFile *F, const Archive::Symbol Sym);
  Symbol *addAbsolute(StringRef N, COFFSymbolRef S);
  Symbol *addRegular(InputFile *F, StringRef N, bool IsCOMDAT,
                     const llvm::object::coff_symbol_generic *S = nullptr,
                     SectionChunk *C = nullptr);
  Symbol *addCommon(InputFile *F, StringRef N, uint64_t Size,
                    const llvm::object::coff_symbol_generic *S = nullptr,
                    CommonChunk *C = nullptr);
  Symbol *addImportData(StringRef N, ImportFile *F);
  Symbol *addImportThunk(StringRef Name, DefinedImportData *S,
                         uint16_t Machine);

  void reportDuplicate(Symbol *Existing, InputFile *NewFile);

  // A list of chunks which to be added to .rdata.
  std::vector<Chunk *> LocalImportChunks;

private:
  std::pair<Symbol *, bool> insert(StringRef Name);
  StringRef findByPrefix(StringRef Prefix);

  llvm::DenseMap<llvm::CachedHashStringRef, Symbol *> Symtab;

  std::vector<BitcodeFile *> BitcodeFiles;
  std::unique_ptr<BitcodeCompiler> LTO;
};

extern SymbolTable *Symtab;

} // namespace coff
} // namespace lld

#endif

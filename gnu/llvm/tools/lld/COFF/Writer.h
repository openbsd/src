//===- Writer.h -------------------------------------------------*- C++ -*-===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLD_COFF_WRITER_H
#define LLD_COFF_WRITER_H

#include "Chunks.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Object/COFF.h"
#include <cstdint>
#include <vector>

namespace lld {
namespace coff {
class SymbolTable;

static const int PageSize = 4096;

void writeResult(SymbolTable *T);

// OutputSection represents a section in an output file. It's a
// container of chunks. OutputSection and Chunk are 1:N relationship.
// Chunks cannot belong to more than one OutputSections. The writer
// creates multiple OutputSections and assign them unique,
// non-overlapping file offsets and RVAs.
class OutputSection {
public:
  OutputSection(llvm::StringRef N) : Name(N), Header({}) {}
  void setRVA(uint64_t);
  void setFileOffset(uint64_t);
  void addChunk(Chunk *C);
  llvm::StringRef getName() { return Name; }
  std::vector<Chunk *> &getChunks() { return Chunks; }
  void addPermissions(uint32_t C);
  void setPermissions(uint32_t C);
  uint32_t getPermissions() { return Header.Characteristics & PermMask; }
  uint32_t getCharacteristics() { return Header.Characteristics; }
  uint64_t getRVA() { return Header.VirtualAddress; }
  uint64_t getFileOff() { return Header.PointerToRawData; }
  void writeHeaderTo(uint8_t *Buf);

  // Returns the size of this section in an executable memory image.
  // This may be smaller than the raw size (the raw size is multiple
  // of disk sector size, so there may be padding at end), or may be
  // larger (if that's the case, the loader reserves spaces after end
  // of raw data).
  uint64_t getVirtualSize() { return Header.VirtualSize; }

  // Returns the size of the section in the output file.
  uint64_t getRawSize() { return Header.SizeOfRawData; }

  // Set offset into the string table storing this section name.
  // Used only when the name is longer than 8 bytes.
  void setStringTableOff(uint32_t V) { StringTableOff = V; }

  // N.B. The section index is one based.
  uint32_t SectionIndex = 0;

private:
  llvm::StringRef Name;
  llvm::object::coff_section Header;
  uint32_t StringTableOff = 0;
  std::vector<Chunk *> Chunks;
};

}
}

#endif

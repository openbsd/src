//===- SPARCV9.cpp --------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "OutputSections.h"
#include "Symbols.h"
#include "SyntheticSections.h"
#include "Target.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/Endian.h"
#include <algorithm>
#include <cassert>

using namespace llvm;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {
constexpr uint32_t firstLargePltIndex = 32768 - 4;
constexpr uint32_t pltLargeEntriesPerBlock = 160;
constexpr uint32_t pltLargeCodeSize = 24;
constexpr uint32_t pltLargePointerSize = 8;
constexpr uint32_t pltLargeBlockSize =
    pltLargeEntriesPerBlock * (pltLargeCodeSize + pltLargePointerSize);

class SPARCV9 final : public TargetInfo {
public:
  SPARCV9(Ctx &);
  RelExpr getRelExpr(RelType type, const Symbol &s,
                     const uint8_t *loc) const override;
  RelType getDynRel(RelType type) const override;
  void prepareDynamicReloc(RelType type, const Symbol &sym,
                           InputSectionBase &sec) override;
  void finalizeDynamicReloc(DynamicReloc &rel) const override;
  void writeGotHeader(uint8_t *buf) const override;
  uint64_t getPltEntryOffset(uint32_t pltIdx,
                             uint64_t headerSize) const override;
  void writePlt(uint8_t *buf, const Symbol &sym,
                uint64_t pltEntryAddr) const override;
  void finalizePlt(uint8_t *buf) const override;
  void relocate(uint8_t *loc, const Relocation &rel,
                uint64_t val) const override;
  RelExpr adjustGotOffExpr(RelType type, const Symbol &sym, int64_t addend,
                           const uint8_t *loc) const override;

private:
  uint64_t getLargePltPointerOffset(uint32_t pltIdx) const;
  void relaxGot(uint8_t *loc, const Relocation &rel, uint64_t val) const;

  DenseMap<std::pair<OutputSection *, unsigned>, Defined *>
      dynamicSectionSymbols;
};
} // namespace

SPARCV9::SPARCV9(Ctx &ctx) : TargetInfo(ctx) {
  copyRel = R_SPARC_COPY;
  gotRel = R_SPARC_GLOB_DAT;
  pltRel = R_SPARC_JMP_SLOT;
  relativeRel = R_SPARC_RELATIVE;
  iRelativeRel = R_SPARC_IRELATIVE;
  symbolicRel = R_SPARC_64;
  tlsGotRel = R_SPARC_TLS_TPOFF64;
  tlsModuleIndexRel = R_SPARC_TLS_DTPMOD64;
  tlsOffsetRel = R_SPARC_TLS_DTPOFF64;

  gotHeaderEntriesNum = 1;
  pltEntrySize = 32;
  pltHeaderSize = 4 * pltEntrySize;
  usesGotPlt = false;

  defaultCommonPageSize = 8192;
  defaultMaxPageSize = 0x100000;
  defaultImageBase = 0x100000;
}

RelExpr SPARCV9::getRelExpr(RelType type, const Symbol &s,
                            const uint8_t *loc) const {
  switch (type) {
  case R_SPARC_NONE:
    return R_NONE;
  case R_SPARC_8:
  case R_SPARC_16:
  case R_SPARC_32:
  case R_SPARC_HI22:
  case R_SPARC_13:
  case R_SPARC_LO10:
  case R_SPARC_UA32:
  case R_SPARC_64:
  case R_SPARC_HH22:
  case R_SPARC_HM10:
  case R_SPARC_LM22:
  case R_SPARC_HIX22:
  case R_SPARC_LOX10:
  case R_SPARC_H44:
  case R_SPARC_M44:
  case R_SPARC_L44:
  case R_SPARC_UA64:
  case R_SPARC_UA16:
    return R_ABS;
  case R_SPARC_DISP8:
  case R_SPARC_DISP16:
  case R_SPARC_DISP32:
  case R_SPARC_WDISP30:
  case R_SPARC_WDISP22:
  case R_SPARC_PC10:
  case R_SPARC_PC22:
  case R_SPARC_WDISP16:
  case R_SPARC_WDISP19:
  case R_SPARC_DISP64:
    return R_PC;
  case R_SPARC_GOT10:
  case R_SPARC_GOT13:
  case R_SPARC_GOT22:
  case R_SPARC_GOTDATA_OP_HIX22:
  case R_SPARC_GOTDATA_OP_LOX10:
  case R_SPARC_GOTDATA_OP:
    return R_GOT_OFF;
  case R_SPARC_WPLT30:
  case R_SPARC_TLS_GD_CALL:
  case R_SPARC_TLS_LDM_CALL:
    return R_PLT_PC;
  case R_SPARC_TLS_GD_HI22:
  case R_SPARC_TLS_GD_LO10:
    return R_TLSGD_GOT;
  case R_SPARC_TLS_GD_ADD:
  case R_SPARC_TLS_LDM_ADD:
  case R_SPARC_TLS_LDO_ADD:
  case R_SPARC_TLS_IE_LD:
  case R_SPARC_TLS_IE_LDX:
  case R_SPARC_TLS_IE_ADD:
    return R_NONE; // TODO: Relax TLS relocations.
  case R_SPARC_TLS_LDM_HI22:
  case R_SPARC_TLS_LDM_LO10:
    return R_TLSLD_GOT;
  case R_SPARC_TLS_LDO_HIX22:
  case R_SPARC_TLS_LDO_LOX10:
    return R_DTPREL;
  case R_SPARC_TLS_IE_HI22:
  case R_SPARC_TLS_IE_LO10:
    return R_GOT;
  case R_SPARC_TLS_LE_HIX22:
  case R_SPARC_TLS_LE_LOX10:
    return R_TPREL;
  case R_SPARC_GOTDATA_HIX22:
  case R_SPARC_GOTDATA_LOX10:
    return R_GOTREL;
  default:
    Err(ctx) << getErrorLoc(ctx, loc) << "unknown relocation (" << type.v
             << ") against symbol " << &s;
    return R_NONE;
  }
}

RelType SPARCV9::getDynRel(RelType type) const {
  switch (type) {
  case R_SPARC_16:
  case R_SPARC_32:
  case R_SPARC_64:
  case R_SPARC_UA16:
  case R_SPARC_UA32:
  case R_SPARC_UA64:
    return type;
  default:
    break;
  }
  return R_SPARC_NONE;
}

static bool getSparcAbsRelocPair(RelType type, RelType &aligned,
                                 RelType &unaligned, uint64_t &alignment) {
  switch (type) {
  case R_SPARC_16:
  case R_SPARC_UA16:
    aligned = R_SPARC_16;
    unaligned = R_SPARC_UA16;
    alignment = 2;
    return true;
  case R_SPARC_32:
  case R_SPARC_UA32:
    aligned = R_SPARC_32;
    unaligned = R_SPARC_UA32;
    alignment = 4;
    return true;
  case R_SPARC_64:
  case R_SPARC_UA64:
    aligned = R_SPARC_64;
    unaligned = R_SPARC_UA64;
    alignment = 8;
    return true;
  default:
    return false;
  }
}

void SPARCV9::prepareDynamicReloc(RelType type, const Symbol &sym,
                                  InputSectionBase &sec) {
  if (auto *eh = dyn_cast<EhInputSection>(&sec)) {
    SyntheticSection *parent = eh->getParent();
    parent->flags |= sec.flags & SHF_WRITE;
    parent->getParent()->flags |= sec.flags & SHF_WRITE;
  }

  RelType aligned = R_SPARC_NONE, unaligned = R_SPARC_NONE;
  uint64_t alignment = 1;
  if (!getSparcAbsRelocPair(type, aligned, unaligned, alignment) ||
      sym.isPreemptible)
    return;

  OutputSection *osec = sym.getOutputSection();
  if (!osec)
    return;

  Partition &part = sec.getPartition(ctx);
  unsigned partition = part.getNumber(ctx);
  auto key = std::make_pair(osec, partition);
  if (dynamicSectionSymbols.contains(key))
    return;

  Defined *sectionSym =
      makeDefined(ctx, ctx.internalFile, "", STB_LOCAL, STV_DEFAULT,
                  STT_SECTION, 0, 0, osec);
  sectionSym->partition = partition;
  part.dynSymTab->addLocalSectionSymbol(sectionSym);
  dynamicSectionSymbols[key] = sectionSym;
}

void SPARCV9::finalizeDynamicReloc(DynamicReloc &rel) const {
  // scanEhSection records an offset relative to the merged .eh_frame.
  if (auto *eh = dyn_cast<EhInputSection>(rel.inputSec)) {
    rel.inputSec = eh->getParent();
    rel.r_offset = rel.inputSec->getVA(rel.offsetInSec);
  }

  if (rel.type == R_SPARC_JMP_SLOT && rel.inputSec == ctx.in.plt.get()) {
    uint32_t pltIdx = rel.sym->getPltIdx(ctx);
    if (pltIdx >= firstLargePltIndex) {
      uint64_t off =
          getPltEntryOffset(pltIdx, ctx.in.plt->headerSize);
      rel.r_offset =
          ctx.in.plt->getVA() + getLargePltPointerOffset(pltIdx);
      rel.addend = -static_cast<int64_t>(ctx.in.plt->getVA() + off + 4);
    }
  }

  RelType aligned = R_SPARC_NONE, unaligned = R_SPARC_NONE;
  uint64_t alignment = 1;
  if (!getSparcAbsRelocPair(rel.type, aligned, unaligned, alignment))
    return;

  rel.type = rel.r_offset % alignment == 0 ? aligned : unaligned;
  if (!rel.needsDynSymIndex() || rel.sym->isPreemptible)
    return;

  if (rel.type == R_SPARC_64) {
    rel.convertToRelative(relativeRel);
    return;
  }

  OutputSection *osec = rel.sym->getOutputSection();
  unsigned partition = rel.inputSec->getPartition(ctx).getNumber(ctx);
  auto it = dynamicSectionSymbols.find(std::make_pair(osec, partition));
  assert(it != dynamicSectionSymbols.end());
  rel.addend = rel.sym->getVA(ctx, rel.addend);
  rel.sym = it->second;
}

void SPARCV9::relocate(uint8_t *loc, const Relocation &rel,
                       uint64_t val) const {
  switch (rel.expr) {
  case R_RELAX_GOT_OFF:
    return relaxGot(loc, rel, val);
  default:
    break;
  }

  switch (rel.type) {
  case R_SPARC_8:
    // V-byte8
    checkUInt(ctx, loc, val, 8, rel);
    *loc = val;
    break;
  case R_SPARC_16:
  case R_SPARC_UA16:
    // V-half16
    checkUInt(ctx, loc, val, 16, rel);
    write16be(loc, val);
    break;
  case R_SPARC_32:
  case R_SPARC_UA32:
    // V-word32
    checkUInt(ctx, loc, val, 32, rel);
    write32be(loc, val);
    break;
  case R_SPARC_DISP8:
    // V-byte8
    checkIntUInt(ctx, loc, val, 8, rel);
    *loc = val;
    break;
  case R_SPARC_DISP16:
    // V-half16
    checkIntUInt(ctx, loc, val, 16, rel);
    write16be(loc, val);
    break;
  case R_SPARC_DISP32:
    // V-disp32
    checkIntUInt(ctx, loc, val, 32, rel);
    write32be(loc, val);
    break;
  case R_SPARC_WDISP30:
  case R_SPARC_WPLT30:
  case R_SPARC_TLS_GD_CALL:
  case R_SPARC_TLS_LDM_CALL:
    // V-disp30
    checkIntUInt(ctx, loc, val, 32, rel);
    write32be(loc, (read32be(loc) & ~0x3fffffff) | ((val >> 2) & 0x3fffffff));
    break;
  case R_SPARC_WDISP22:
    // V-disp22
    checkIntUInt(ctx, loc, val, 24, rel);
    write32be(loc, (read32be(loc) & ~0x003fffff) | ((val >> 2) & 0x003fffff));
    break;
  case R_SPARC_HI22: // Only T-imm22 on 32-bit, despite binutils behavior.
    // V-imm22
    checkUInt(ctx, loc, val, 32, rel);
    write32be(loc, (read32be(loc) & ~0x003fffff) | ((val >> 10) & 0x003fffff));
    break;
  case R_SPARC_22:
    // V-imm22
    checkUInt(ctx, loc, val, 22, rel);
    write32be(loc, (read32be(loc) & ~0x003fffff) | (val & 0x003fffff));
    break;
  case R_SPARC_13:
  case R_SPARC_GOT13:
    // V-simm13
    checkIntUInt(ctx, loc, val, 13, rel);
    write32be(loc, (read32be(loc) & ~0x00001fff) | (val & 0x00001fff));
    break;
  case R_SPARC_LO10:
  case R_SPARC_GOT10:
  case R_SPARC_PC10:
  case R_SPARC_TLS_GD_LO10:
  case R_SPARC_TLS_LDM_LO10:
  case R_SPARC_TLS_IE_LO10:
    // T-simm13
    write32be(loc, (read32be(loc) & ~0x000003ff) | (val & 0x000003ff));
    break;
  case R_SPARC_TLS_LDO_LOX10:
    // T-simm13
    write32be(loc, (read32be(loc) & ~0x00001fff) | (val & 0x000003ff));
    break;
  case R_SPARC_GOT22:
  case R_SPARC_LM22:
  case R_SPARC_TLS_GD_HI22:
  case R_SPARC_TLS_LDM_HI22:
  case R_SPARC_TLS_LDO_HIX22: // Not V-simm22, despite binutils behavior.
  case R_SPARC_TLS_IE_HI22:
    // T-(s)imm22
    write32be(loc, (read32be(loc) & ~0x003fffff) | ((val >> 10) & 0x003fffff));
    break;
  case R_SPARC_PC22:
    // V-disp22
    checkIntUInt(ctx, loc, val, 32, rel);
    write32be(loc, (read32be(loc) & ~0x003fffff) | ((val >> 10) & 0x003fffff));
    break;
  case R_SPARC_64:
  case R_SPARC_DISP64:
  case R_SPARC_UA64:
    // V-xword64
    write64be(loc, val);
    break;
  case R_SPARC_HH22:
    // V-imm22
    write32be(loc, (read32be(loc) & ~0x003fffff) | ((val >> 42) & 0x003fffff));
    break;
  case R_SPARC_HM10:
    // T-simm13
    write32be(loc, (read32be(loc) & ~0x000003ff) | ((val >> 32) & 0x000003ff));
    break;
  case R_SPARC_WDISP16:
    // V-d2/disp14
    checkIntUInt(ctx, loc, val, 18, rel);
    write32be(loc, (read32be(loc) & ~0x0303fff) | (((val >> 2) & 0xc000) << 6) |
                       ((val >> 2) & 0x00003fff));
    break;
  case R_SPARC_WDISP19:
    // V-disp19
    checkIntUInt(ctx, loc, val, 21, rel);
    write32be(loc, (read32be(loc) & ~0x0007ffff) | ((val >> 2) & 0x0007ffff));
    break;
  case R_SPARC_HIX22:
    // V-imm22
    checkUInt(ctx, loc, ~val, 32, rel);
    write32be(loc, (read32be(loc) & ~0x003fffff) | ((~val >> 10) & 0x003fffff));
    break;
  case R_SPARC_LOX10:
  case R_SPARC_TLS_LE_LOX10:
    // T-simm13
    write32be(loc, (read32be(loc) & ~0x00001fff) | (val & 0x000003ff) | 0x1c00);
    break;
  case R_SPARC_H44:
    // V-imm22
    checkUInt(ctx, loc, val, 44, rel);
    write32be(loc, (read32be(loc) & ~0x003fffff) | ((val >> 22) & 0x003fffff));
    break;
  case R_SPARC_M44:
    // T-imm10
    write32be(loc, (read32be(loc) & ~0x000003ff) | ((val >> 12) & 0x000003ff));
    break;
  case R_SPARC_L44:
    // T-imm13
    write32be(loc, (read32be(loc) & ~0x00000fff) | (val & 0x00000fff));
    break;
  case R_SPARC_TLS_GD_ADD:
  case R_SPARC_TLS_LDM_ADD:
  case R_SPARC_TLS_LDO_ADD:
  case R_SPARC_TLS_IE_LD:
  case R_SPARC_TLS_IE_LDX:
  case R_SPARC_TLS_IE_ADD:
    // None
    break;
  case R_SPARC_TLS_LE_HIX22: // Not V-imm2, despite binutils behavior.
    // T-imm22
    write32be(loc, (read32be(loc) & ~0x003fffff) | ((~val >> 10) & 0x003fffff));
    break;
  case R_SPARC_GOTDATA_HIX22:
    // V-imm22
    checkUInt(ctx, loc, ((int64_t)val < 0 ? ~val : val), 32, rel);
    write32be(loc, (read32be(loc) & ~0x003fffff) |
                       ((((int64_t)val < 0 ? ~val : val) >> 10) & 0x003fffff));
    break;
  case R_SPARC_GOTDATA_OP_HIX22: // Not V-imm22, despite binutils behavior.
                                 // Non-relaxed case.
    // T-imm22
    write32be(loc, (read32be(loc) & ~0x003fffff) |
                       ((((int64_t)val < 0 ? ~val : val) >> 10) & 0x003fffff));
    break;
  case R_SPARC_GOTDATA_LOX10:
  case R_SPARC_GOTDATA_OP_LOX10: // Non-relaxed case.
    // T-imm13
    write32be(loc, (read32be(loc) & ~0x00001fff) | (val & 0x000003ff) |
                       ((int64_t)val < 0 ? 0x1c00 : 0));
    break;
  case R_SPARC_GOTDATA_OP: // Non-relaxed case.
    // word32
    // Nothing needs to be done in the non-relaxed case.
    break;
  default:
    llvm_unreachable("unknown relocation");
  }
}

RelExpr SPARCV9::adjustGotOffExpr(RelType type, const Symbol &sym,
                                  int64_t addend, const uint8_t *loc) const {
  switch (type) {
  case R_SPARC_GOTDATA_OP_HIX22:
  case R_SPARC_GOTDATA_OP_LOX10:
  case R_SPARC_GOTDATA_OP:
    if (sym.isLocal())
      return R_RELAX_GOT_OFF;

    [[fallthrough]];
  default:
    return R_GOT_OFF;
  }
}

void SPARCV9::relaxGot(uint8_t *loc, const Relocation &rel,
                       uint64_t val) const {
  switch (rel.type) {
  case R_SPARC_GOTDATA_OP_HIX22: // Not V-imm22, despite binutils behavior.
    // T-imm22
    write32be(loc, (read32be(loc) & ~0x003fffff) |
                       ((((int64_t)val < 0 ? ~val : val) >> 10) & 0x003fffff));
    break;
  case R_SPARC_GOTDATA_OP_LOX10:
    // T-imm13
    write32be(loc, (read32be(loc) & ~0x00001fff) | (val & 0x000003ff) |
                       ((int64_t)val < 0 ? 0x1c00 : 0));
    break;
  case R_SPARC_GOTDATA_OP:
    // word32
    // ldx [%rs1 + %rs2], %rd -> add %rs1, %rs2, %rd
    write32be(loc, (read32be(loc) & 0x3e07c01f) | 0x80000000);
    break;
  default:
    llvm_unreachable("unknown relocation");
  }
}

void SPARCV9::writeGotHeader(uint8_t *buf) const {
  // _GLOBAL_OFFSET_TABLE_[0] = _DYNAMIC when a dynamic section exists.
  if (ctx.mainPart->dynamic)
    write64(ctx, buf, ctx.mainPart->dynamic->getVA());
}

uint64_t SPARCV9::getPltEntryOffset(uint32_t pltIdx,
                                    uint64_t headerSize) const {
  if (pltIdx < firstLargePltIndex)
    return headerSize + uint64_t{pltIdx} * pltEntrySize;

  uint32_t largeIdx = pltIdx - firstLargePltIndex;
  return headerSize + uint64_t{firstLargePltIndex} * pltEntrySize +
         largeIdx / pltLargeEntriesPerBlock * pltLargeBlockSize +
         largeIdx % pltLargeEntriesPerBlock * pltLargeCodeSize;
}

uint64_t SPARCV9::getLargePltPointerOffset(uint32_t pltIdx) const {
  assert(pltIdx >= firstLargePltIndex);
  uint32_t largeIdx = pltIdx - firstLargePltIndex;
  uint32_t entriesBeforeBlock =
      largeIdx / pltLargeEntriesPerBlock * pltLargeEntriesPerBlock;
  uint32_t indexInBlock = largeIdx % pltLargeEntriesPerBlock;
  uint64_t largeEntries = ctx.in.plt->getNumEntries() - firstLargePltIndex;
  uint64_t entriesInBlock =
      std::min<uint64_t>(pltLargeEntriesPerBlock,
                         largeEntries - entriesBeforeBlock);
  uint64_t blockOffset = getPltEntryOffset(
      firstLargePltIndex + entriesBeforeBlock, ctx.in.plt->headerSize);

  return blockOffset + entriesInBlock * pltLargeCodeSize +
         indexInBlock * pltLargePointerSize;
}

void SPARCV9::writePlt(uint8_t *buf, const Symbol &sym,
                       uint64_t pltEntryAddr) const {
  uint64_t off = pltEntryAddr - ctx.in.plt->getVA();
  if (sym.getPltIdx(ctx) >= firstLargePltIndex) {
    uint64_t ptrOff = getLargePltPointerOffset(sym.getPltIdx(ctx));
    write32be(buf, 0x8a10000f);      // mov %o7, %g5
    write32be(buf + 4, 0x40000002);  // call .+8
    write32be(buf + 8, 0x01000000);  // nop
    write32be(buf + 12,
              0xc25be000 | ((ptrOff - (off + 4)) & 0x1fff));
    write32be(buf + 16, 0x83c3c001); // jmpl %o7+%g1, %g1
    write32be(buf + 20, 0x9e100005); // mov %g5, %o7
    return;
  }

  const uint8_t pltData[] = {
      0x03, 0x00, 0x00, 0x00, // sethi   (. - .PLT0), %g1
      0x30, 0x68, 0x00, 0x00, // ba,a    %xcc, .PLT1
      0x01, 0x00, 0x00, 0x00, // nop
      0x01, 0x00, 0x00, 0x00, // nop
      0x01, 0x00, 0x00, 0x00, // nop
      0x01, 0x00, 0x00, 0x00, // nop
      0x01, 0x00, 0x00, 0x00, // nop
      0x01, 0x00, 0x00, 0x00  // nop
  };
  memcpy(buf, pltData, sizeof(pltData));

  relocateNoSym(buf, R_SPARC_22, off);
  relocateNoSym(buf + 4, R_SPARC_WDISP19, -(off + 4 - pltEntrySize));
}

void SPARCV9::finalizePlt(uint8_t *buf) const {
  for (uint32_t i = firstLargePltIndex; i < ctx.in.plt->getNumEntries(); ++i) {
    uint64_t off = getPltEntryOffset(i, ctx.in.plt->headerSize);
    write64be(buf + getLargePltPointerOffset(i), -(off + 4));
  }
}

void elf::setSPARCV9TargetInfo(Ctx &ctx) { ctx.target.reset(new SPARCV9(ctx)); }

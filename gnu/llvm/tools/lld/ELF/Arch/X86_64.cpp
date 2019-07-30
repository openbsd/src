//===- X86_64.cpp ---------------------------------------------------------===//
//
//                             The LLVM Linker
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Error.h"
#include "InputFiles.h"
#include "Symbols.h"
#include "SyntheticSections.h"
#include "Target.h"
#include "llvm/Object/ELF.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::object;
using namespace llvm::support::endian;
using namespace llvm::ELF;
using namespace lld;
using namespace lld::elf;

namespace {
template <class ELFT> class X86_64 final : public TargetInfo {
public:
  X86_64();
  RelExpr getRelExpr(uint32_t Type, const SymbolBody &S,
                     const uint8_t *Loc) const override;
  bool isPicRel(uint32_t Type) const override;
  void writeGotPltHeader(uint8_t *Buf) const override;
  void writeGotPlt(uint8_t *Buf, const SymbolBody &S) const override;
  void writePltHeader(uint8_t *Buf) const override;
  void writePlt(uint8_t *Buf, uint64_t GotPltEntryAddr, uint64_t PltEntryAddr,
                int32_t Index, unsigned RelOff) const override;
  void relocateOne(uint8_t *Loc, uint32_t Type, uint64_t Val) const override;

  RelExpr adjustRelaxExpr(uint32_t Type, const uint8_t *Data,
                          RelExpr Expr) const override;
  void relaxGot(uint8_t *Loc, uint64_t Val) const override;
  void relaxTlsGdToIe(uint8_t *Loc, uint32_t Type, uint64_t Val) const override;
  void relaxTlsGdToLe(uint8_t *Loc, uint32_t Type, uint64_t Val) const override;
  void relaxTlsIeToLe(uint8_t *Loc, uint32_t Type, uint64_t Val) const override;
  void relaxTlsLdToLe(uint8_t *Loc, uint32_t Type, uint64_t Val) const override;

private:
  void relaxGotNoPic(uint8_t *Loc, uint64_t Val, uint8_t Op,
                     uint8_t ModRm) const;
};
} // namespace

template <class ELFT> X86_64<ELFT>::X86_64() {
  GotBaseSymOff = -1;
  CopyRel = R_X86_64_COPY;
  GotRel = R_X86_64_GLOB_DAT;
  PltRel = R_X86_64_JUMP_SLOT;
  RelativeRel = R_X86_64_RELATIVE;
  IRelativeRel = R_X86_64_IRELATIVE;
  TlsGotRel = R_X86_64_TPOFF64;
  TlsModuleIndexRel = R_X86_64_DTPMOD64;
  TlsOffsetRel = R_X86_64_DTPOFF64;
  GotEntrySize = 8;
  GotPltEntrySize = 8;
  PltEntrySize = 16;
  PltHeaderSize = 16;
  TlsGdRelaxSkip = 2;
  TrapInstr = 0xcccccccc; // 0xcc = INT3

  // Align to the large page size (known as a superpage or huge page).
  // FreeBSD automatically promotes large, superpage-aligned allocations.
  DefaultImageBase = 0x200000;
}

template <class ELFT>
RelExpr X86_64<ELFT>::getRelExpr(uint32_t Type, const SymbolBody &S,
                                 const uint8_t *Loc) const {
  switch (Type) {
  case R_X86_64_8:
  case R_X86_64_16:
  case R_X86_64_32:
  case R_X86_64_32S:
  case R_X86_64_64:
  case R_X86_64_DTPOFF32:
  case R_X86_64_DTPOFF64:
    return R_ABS;
  case R_X86_64_TPOFF32:
    return R_TLS;
  case R_X86_64_TLSLD:
    return R_TLSLD_PC;
  case R_X86_64_TLSGD:
    return R_TLSGD_PC;
  case R_X86_64_SIZE32:
  case R_X86_64_SIZE64:
    return R_SIZE;
  case R_X86_64_PLT32:
    return R_PLT_PC;
  case R_X86_64_PC32:
  case R_X86_64_PC64:
    return R_PC;
  case R_X86_64_GOT32:
  case R_X86_64_GOT64:
    return R_GOT_FROM_END;
  case R_X86_64_GOTPCREL:
  case R_X86_64_GOTPCRELX:
  case R_X86_64_REX_GOTPCRELX:
  case R_X86_64_GOTTPOFF:
    return R_GOT_PC;
  case R_X86_64_NONE:
    return R_NONE;
  default:
    error(toString(S.File) + ": unknown relocation type: " + toString(Type));
    return R_HINT;
  }
}

template <class ELFT> void X86_64<ELFT>::writeGotPltHeader(uint8_t *Buf) const {
  // The first entry holds the value of _DYNAMIC. It is not clear why that is
  // required, but it is documented in the psabi and the glibc dynamic linker
  // seems to use it (note that this is relevant for linking ld.so, not any
  // other program).
  write64le(Buf, InX::Dynamic->getVA());
}

template <class ELFT>
void X86_64<ELFT>::writeGotPlt(uint8_t *Buf, const SymbolBody &S) const {
  // See comments in X86TargetInfo::writeGotPlt.
  write32le(Buf, S.getPltVA() + 6);
}

template <class ELFT> void X86_64<ELFT>::writePltHeader(uint8_t *Buf) const {
  const uint8_t PltData[] = {
      0xff, 0x35, 0x00, 0x00, 0x00, 0x00, // pushq GOTPLT+8(%rip)
      0xff, 0x25, 0x00, 0x00, 0x00, 0x00, // jmp *GOTPLT+16(%rip)
      0x0f, 0x1f, 0x40, 0x00              // nop
  };
  memcpy(Buf, PltData, sizeof(PltData));
  uint64_t GotPlt = InX::GotPlt->getVA();
  uint64_t Plt = InX::Plt->getVA();
  write32le(Buf + 2, GotPlt - Plt + 2); // GOTPLT+8
  write32le(Buf + 8, GotPlt - Plt + 4); // GOTPLT+16
}

template <class ELFT>
void X86_64<ELFT>::writePlt(uint8_t *Buf, uint64_t GotPltEntryAddr,
                            uint64_t PltEntryAddr, int32_t Index,
                            unsigned RelOff) const {
  const uint8_t Inst[] = {
      0xff, 0x25, 0x00, 0x00, 0x00, 0x00, // jmpq *got(%rip)
      0x68, 0x00, 0x00, 0x00, 0x00,       // pushq <relocation index>
      0xe9, 0x00, 0x00, 0x00, 0x00        // jmpq plt[0]
  };
  memcpy(Buf, Inst, sizeof(Inst));

  write32le(Buf + 2, GotPltEntryAddr - PltEntryAddr - 6);
  write32le(Buf + 7, Index);
  write32le(Buf + 12, -Index * PltEntrySize - PltHeaderSize - 16);
}

template <class ELFT> bool X86_64<ELFT>::isPicRel(uint32_t Type) const {
  return Type != R_X86_64_PC32 && Type != R_X86_64_32 &&
         Type != R_X86_64_TPOFF32;
}

template <class ELFT>
void X86_64<ELFT>::relaxTlsGdToLe(uint8_t *Loc, uint32_t Type,
                                  uint64_t Val) const {
  // Convert
  //   .byte 0x66
  //   leaq x@tlsgd(%rip), %rdi
  //   .word 0x6666
  //   rex64
  //   call __tls_get_addr@plt
  // to
  //   mov %fs:0x0,%rax
  //   lea x@tpoff,%rax
  const uint8_t Inst[] = {
      0x64, 0x48, 0x8b, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00, // mov %fs:0x0,%rax
      0x48, 0x8d, 0x80, 0x00, 0x00, 0x00, 0x00              // lea x@tpoff,%rax
  };
  memcpy(Loc - 4, Inst, sizeof(Inst));

  // The original code used a pc relative relocation and so we have to
  // compensate for the -4 in had in the addend.
  write32le(Loc + 8, Val + 4);
}

template <class ELFT>
void X86_64<ELFT>::relaxTlsGdToIe(uint8_t *Loc, uint32_t Type,
                                  uint64_t Val) const {
  // Convert
  //   .byte 0x66
  //   leaq x@tlsgd(%rip), %rdi
  //   .word 0x6666
  //   rex64
  //   call __tls_get_addr@plt
  // to
  //   mov %fs:0x0,%rax
  //   addq x@tpoff,%rax
  const uint8_t Inst[] = {
      0x64, 0x48, 0x8b, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00, // mov %fs:0x0,%rax
      0x48, 0x03, 0x05, 0x00, 0x00, 0x00, 0x00              // addq x@tpoff,%rax
  };
  memcpy(Loc - 4, Inst, sizeof(Inst));

  // Both code sequences are PC relatives, but since we are moving the constant
  // forward by 8 bytes we have to subtract the value by 8.
  write32le(Loc + 8, Val - 8);
}

// In some conditions, R_X86_64_GOTTPOFF relocation can be optimized to
// R_X86_64_TPOFF32 so that it does not use GOT.
template <class ELFT>
void X86_64<ELFT>::relaxTlsIeToLe(uint8_t *Loc, uint32_t Type,
                                  uint64_t Val) const {
  uint8_t *Inst = Loc - 3;
  uint8_t Reg = Loc[-1] >> 3;
  uint8_t *RegSlot = Loc - 1;

  // Note that ADD with RSP or R12 is converted to ADD instead of LEA
  // because LEA with these registers needs 4 bytes to encode and thus
  // wouldn't fit the space.

  if (memcmp(Inst, "\x48\x03\x25", 3) == 0) {
    // "addq foo@gottpoff(%rip),%rsp" -> "addq $foo,%rsp"
    memcpy(Inst, "\x48\x81\xc4", 3);
  } else if (memcmp(Inst, "\x4c\x03\x25", 3) == 0) {
    // "addq foo@gottpoff(%rip),%r12" -> "addq $foo,%r12"
    memcpy(Inst, "\x49\x81\xc4", 3);
  } else if (memcmp(Inst, "\x4c\x03", 2) == 0) {
    // "addq foo@gottpoff(%rip),%r[8-15]" -> "leaq foo(%r[8-15]),%r[8-15]"
    memcpy(Inst, "\x4d\x8d", 2);
    *RegSlot = 0x80 | (Reg << 3) | Reg;
  } else if (memcmp(Inst, "\x48\x03", 2) == 0) {
    // "addq foo@gottpoff(%rip),%reg -> "leaq foo(%reg),%reg"
    memcpy(Inst, "\x48\x8d", 2);
    *RegSlot = 0x80 | (Reg << 3) | Reg;
  } else if (memcmp(Inst, "\x4c\x8b", 2) == 0) {
    // "movq foo@gottpoff(%rip),%r[8-15]" -> "movq $foo,%r[8-15]"
    memcpy(Inst, "\x49\xc7", 2);
    *RegSlot = 0xc0 | Reg;
  } else if (memcmp(Inst, "\x48\x8b", 2) == 0) {
    // "movq foo@gottpoff(%rip),%reg" -> "movq $foo,%reg"
    memcpy(Inst, "\x48\xc7", 2);
    *RegSlot = 0xc0 | Reg;
  } else {
    error(getErrorLocation(Loc - 3) +
          "R_X86_64_GOTTPOFF must be used in MOVQ or ADDQ instructions only");
  }

  // The original code used a PC relative relocation.
  // Need to compensate for the -4 it had in the addend.
  write32le(Loc, Val + 4);
}

template <class ELFT>
void X86_64<ELFT>::relaxTlsLdToLe(uint8_t *Loc, uint32_t Type,
                                  uint64_t Val) const {
  // Convert
  //   leaq bar@tlsld(%rip), %rdi
  //   callq __tls_get_addr@PLT
  //   leaq bar@dtpoff(%rax), %rcx
  // to
  //   .word 0x6666
  //   .byte 0x66
  //   mov %fs:0,%rax
  //   leaq bar@tpoff(%rax), %rcx
  if (Type == R_X86_64_DTPOFF64) {
    write64le(Loc, Val);
    return;
  }
  if (Type == R_X86_64_DTPOFF32) {
    write32le(Loc, Val);
    return;
  }

  const uint8_t Inst[] = {
      0x66, 0x66,                                          // .word 0x6666
      0x66,                                                // .byte 0x66
      0x64, 0x48, 0x8b, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00 // mov %fs:0,%rax
  };
  memcpy(Loc - 3, Inst, sizeof(Inst));
}

template <class ELFT>
void X86_64<ELFT>::relocateOne(uint8_t *Loc, uint32_t Type,
                               uint64_t Val) const {
  switch (Type) {
  case R_X86_64_8:
    checkUInt<8>(Loc, Val, Type);
    *Loc = Val;
    break;
  case R_X86_64_16:
    checkUInt<16>(Loc, Val, Type);
    write16le(Loc, Val);
    break;
  case R_X86_64_32:
    checkUInt<32>(Loc, Val, Type);
    write32le(Loc, Val);
    break;
  case R_X86_64_32S:
  case R_X86_64_TPOFF32:
  case R_X86_64_GOT32:
  case R_X86_64_GOTPCREL:
  case R_X86_64_GOTPCRELX:
  case R_X86_64_REX_GOTPCRELX:
  case R_X86_64_PC32:
  case R_X86_64_GOTTPOFF:
  case R_X86_64_PLT32:
  case R_X86_64_TLSGD:
  case R_X86_64_TLSLD:
  case R_X86_64_DTPOFF32:
  case R_X86_64_SIZE32:
    checkInt<32>(Loc, Val, Type);
    write32le(Loc, Val);
    break;
  case R_X86_64_64:
  case R_X86_64_DTPOFF64:
  case R_X86_64_GLOB_DAT:
  case R_X86_64_PC64:
  case R_X86_64_SIZE64:
  case R_X86_64_GOT64:
    write64le(Loc, Val);
    break;
  default:
    llvm_unreachable("unexpected relocation");
  }
}

template <class ELFT>
RelExpr X86_64<ELFT>::adjustRelaxExpr(uint32_t Type, const uint8_t *Data,
                                      RelExpr RelExpr) const {
  if (Type != R_X86_64_GOTPCRELX && Type != R_X86_64_REX_GOTPCRELX)
    return RelExpr;
  const uint8_t Op = Data[-2];
  const uint8_t ModRm = Data[-1];

  // FIXME: When PIC is disabled and foo is defined locally in the
  // lower 32 bit address space, memory operand in mov can be converted into
  // immediate operand. Otherwise, mov must be changed to lea. We support only
  // latter relaxation at this moment.
  if (Op == 0x8b)
    return R_RELAX_GOT_PC;

  // Relax call and jmp.
  if (Op == 0xff && (ModRm == 0x15 || ModRm == 0x25))
    return R_RELAX_GOT_PC;

  // Relaxation of test, adc, add, and, cmp, or, sbb, sub, xor.
  // If PIC then no relaxation is available.
  // We also don't relax test/binop instructions without REX byte,
  // they are 32bit operations and not common to have.
  assert(Type == R_X86_64_REX_GOTPCRELX);
  return Config->Pic ? RelExpr : R_RELAX_GOT_PC_NOPIC;
}

// A subset of relaxations can only be applied for no-PIC. This method
// handles such relaxations. Instructions encoding information was taken from:
// "Intel 64 and IA-32 Architectures Software Developer's Manual V2"
// (http://www.intel.com/content/dam/www/public/us/en/documents/manuals/
//    64-ia-32-architectures-software-developer-instruction-set-reference-manual-325383.pdf)
template <class ELFT>
void X86_64<ELFT>::relaxGotNoPic(uint8_t *Loc, uint64_t Val, uint8_t Op,
                                 uint8_t ModRm) const {
  const uint8_t Rex = Loc[-3];
  // Convert "test %reg, foo@GOTPCREL(%rip)" to "test $foo, %reg".
  if (Op == 0x85) {
    // See "TEST-Logical Compare" (4-428 Vol. 2B),
    // TEST r/m64, r64 uses "full" ModR / M byte (no opcode extension).

    // ModR/M byte has form XX YYY ZZZ, where
    // YYY is MODRM.reg(register 2), ZZZ is MODRM.rm(register 1).
    // XX has different meanings:
    // 00: The operand's memory address is in reg1.
    // 01: The operand's memory address is reg1 + a byte-sized displacement.
    // 10: The operand's memory address is reg1 + a word-sized displacement.
    // 11: The operand is reg1 itself.
    // If an instruction requires only one operand, the unused reg2 field
    // holds extra opcode bits rather than a register code
    // 0xC0 == 11 000 000 binary.
    // 0x38 == 00 111 000 binary.
    // We transfer reg2 to reg1 here as operand.
    // See "2.1.3 ModR/M and SIB Bytes" (Vol. 2A 2-3).
    Loc[-1] = 0xc0 | (ModRm & 0x38) >> 3; // ModR/M byte.

    // Change opcode from TEST r/m64, r64 to TEST r/m64, imm32
    // See "TEST-Logical Compare" (4-428 Vol. 2B).
    Loc[-2] = 0xf7;

    // Move R bit to the B bit in REX byte.
    // REX byte is encoded as 0100WRXB, where
    // 0100 is 4bit fixed pattern.
    // REX.W When 1, a 64-bit operand size is used. Otherwise, when 0, the
    //   default operand size is used (which is 32-bit for most but not all
    //   instructions).
    // REX.R This 1-bit value is an extension to the MODRM.reg field.
    // REX.X This 1-bit value is an extension to the SIB.index field.
    // REX.B This 1-bit value is an extension to the MODRM.rm field or the
    // SIB.base field.
    // See "2.2.1.2 More on REX Prefix Fields " (2-8 Vol. 2A).
    Loc[-3] = (Rex & ~0x4) | (Rex & 0x4) >> 2;
    write32le(Loc, Val);
    return;
  }

  // If we are here then we need to relax the adc, add, and, cmp, or, sbb, sub
  // or xor operations.

  // Convert "binop foo@GOTPCREL(%rip), %reg" to "binop $foo, %reg".
  // Logic is close to one for test instruction above, but we also
  // write opcode extension here, see below for details.
  Loc[-1] = 0xc0 | (ModRm & 0x38) >> 3 | (Op & 0x3c); // ModR/M byte.

  // Primary opcode is 0x81, opcode extension is one of:
  // 000b = ADD, 001b is OR, 010b is ADC, 011b is SBB,
  // 100b is AND, 101b is SUB, 110b is XOR, 111b is CMP.
  // This value was wrote to MODRM.reg in a line above.
  // See "3.2 INSTRUCTIONS (A-M)" (Vol. 2A 3-15),
  // "INSTRUCTION SET REFERENCE, N-Z" (Vol. 2B 4-1) for
  // descriptions about each operation.
  Loc[-2] = 0x81;
  Loc[-3] = (Rex & ~0x4) | (Rex & 0x4) >> 2;
  write32le(Loc, Val);
}

template <class ELFT>
void X86_64<ELFT>::relaxGot(uint8_t *Loc, uint64_t Val) const {
  const uint8_t Op = Loc[-2];
  const uint8_t ModRm = Loc[-1];

  // Convert "mov foo@GOTPCREL(%rip),%reg" to "lea foo(%rip),%reg".
  if (Op == 0x8b) {
    Loc[-2] = 0x8d;
    write32le(Loc, Val);
    return;
  }

  if (Op != 0xff) {
    // We are relaxing a rip relative to an absolute, so compensate
    // for the old -4 addend.
    assert(!Config->Pic);
    relaxGotNoPic(Loc, Val + 4, Op, ModRm);
    return;
  }

  // Convert call/jmp instructions.
  if (ModRm == 0x15) {
    // ABI says we can convert "call *foo@GOTPCREL(%rip)" to "nop; call foo".
    // Instead we convert to "addr32 call foo" where addr32 is an instruction
    // prefix. That makes result expression to be a single instruction.
    Loc[-2] = 0x67; // addr32 prefix
    Loc[-1] = 0xe8; // call
    write32le(Loc, Val);
    return;
  }

  // Convert "jmp *foo@GOTPCREL(%rip)" to "jmp foo; nop".
  // jmp doesn't return, so it is fine to use nop here, it is just a stub.
  assert(ModRm == 0x25);
  Loc[-2] = 0xe9; // jmp
  Loc[3] = 0x90;  // nop
  write32le(Loc - 1, Val + 1);
}

TargetInfo *elf::getX32TargetInfo() {
  static X86_64<ELF32LE> Target;
  return &Target;
}

TargetInfo *elf::getX86_64TargetInfo() {
  static X86_64<ELF64LE> Target;
  return &Target;
}

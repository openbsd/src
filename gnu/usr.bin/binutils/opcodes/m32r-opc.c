/* CGEN support code for m32r.

This file is machine generated.

Copyright (C) 1996, 1997 Free Software Foundation, Inc.

This file is part of the GNU Binutils and/or GDB, the GNU debugger.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/


#include "sysdep.h"
#include <stdio.h>
#include "ansidecl.h"
#include "libiberty.h"
#include "bfd.h"
#include "m32r-opc.h"

struct cgen_keyword_entry m32r_cgen_opval_mach_entries[] = {
  { "m32r", 0 },
  { "test", 1 }
};

struct cgen_keyword m32r_cgen_opval_mach = {
  & m32r_cgen_opval_mach_entries[0],
  2
};

struct cgen_keyword_entry m32r_cgen_opval_h_gr_entries[] = {
  { "fp", 13 },
  { "lr", 14 },
  { "sp", 15 },
  { "r0", 0 },
  { "r1", 1 },
  { "r2", 2 },
  { "r3", 3 },
  { "r4", 4 },
  { "r5", 5 },
  { "r6", 6 },
  { "r7", 7 },
  { "r8", 8 },
  { "r9", 9 },
  { "r10", 10 },
  { "r11", 11 },
  { "r12", 12 },
  { "r13", 13 },
  { "r14", 14 },
  { "r15", 15 }
};

struct cgen_keyword m32r_cgen_opval_h_gr = {
  & m32r_cgen_opval_h_gr_entries[0],
  19
};

struct cgen_keyword_entry m32r_cgen_opval_h_cr_entries[] = {
  { "psw", 0 },
  { "cbr", 1 },
  { "spi", 2 },
  { "spu", 3 },
  { "bpc", 6 },
  { "cr0", 0 },
  { "cr1", 1 },
  { "cr2", 2 },
  { "cr3", 3 },
  { "cr4", 4 },
  { "cr5", 5 },
  { "cr6", 6 }
};

struct cgen_keyword m32r_cgen_opval_h_cr = {
  & m32r_cgen_opval_h_cr_entries[0],
  12
};


static CGEN_HW_ENTRY m32r_cgen_hw_entries[] = {
  { & m32r_cgen_hw_entries[1], "pc", CGEN_ASM_KEYWORD /*FIXME*/, 0 },
  { & m32r_cgen_hw_entries[2], "h-memory", CGEN_ASM_KEYWORD /*FIXME*/, 0 },
  { & m32r_cgen_hw_entries[3], "h-sint", CGEN_ASM_KEYWORD /*FIXME*/, 0 },
  { & m32r_cgen_hw_entries[4], "h-uint", CGEN_ASM_KEYWORD /*FIXME*/, 0 },
  { & m32r_cgen_hw_entries[5], "h-addr", CGEN_ASM_KEYWORD /*FIXME*/, 0 },
  { & m32r_cgen_hw_entries[6], "h-iaddr", CGEN_ASM_KEYWORD /*FIXME*/, 0 },
  { & m32r_cgen_hw_entries[7], "h-hi16", CGEN_ASM_KEYWORD /*FIXME*/, 0 },
  { & m32r_cgen_hw_entries[8], "h-slo16", CGEN_ASM_KEYWORD /*FIXME*/, 0 },
  { & m32r_cgen_hw_entries[9], "h-ulo16", CGEN_ASM_KEYWORD /*FIXME*/, 0 },
  { & m32r_cgen_hw_entries[10], "h-gr", CGEN_ASM_KEYWORD /*FIXME*/, & m32r_cgen_opval_h_gr },
  { & m32r_cgen_hw_entries[11], "h-cr", CGEN_ASM_KEYWORD /*FIXME*/, & m32r_cgen_opval_h_cr },
  { & m32r_cgen_hw_entries[12], "h-accum", CGEN_ASM_KEYWORD /*FIXME*/, 0 },
  { & m32r_cgen_hw_entries[13], "h-cond", CGEN_ASM_KEYWORD /*FIXME*/, 0 },
  { & m32r_cgen_hw_entries[14], "h-sm", CGEN_ASM_KEYWORD /*FIXME*/, 0 },
  { & m32r_cgen_hw_entries[15], "h-bsm", CGEN_ASM_KEYWORD /*FIXME*/, 0 },
  { & m32r_cgen_hw_entries[16], "h-ie", CGEN_ASM_KEYWORD /*FIXME*/, 0 },
  { & m32r_cgen_hw_entries[17], "h-bie", CGEN_ASM_KEYWORD /*FIXME*/, 0 },
  { & m32r_cgen_hw_entries[18], "h-bcond", CGEN_ASM_KEYWORD /*FIXME*/, 0 },
  { NULL, "h-bpc", CGEN_ASM_KEYWORD /*FIXME*/, 0 }
};


const struct cgen_operand m32r_cgen_operand_table[CGEN_NUM_OPERANDS] =
{
/* sr: source register */
  { "sr", 12, 4, { 0, 0|(1<<CGEN_OPERAND_UNSIGNED), { 0 } }  },
/* dr: destination register */
  { "dr", 4, 4, { 0, 0|(1<<CGEN_OPERAND_UNSIGNED), { 0 } }  },
/* src1: source register 1 */
  { "src1", 4, 4, { 0, 0|(1<<CGEN_OPERAND_UNSIGNED), { 0 } }  },
/* src2: source register 2 */
  { "src2", 12, 4, { 0, 0|(1<<CGEN_OPERAND_UNSIGNED), { 0 } }  },
/* scr: source control register */
  { "scr", 12, 4, { 0, 0|(1<<CGEN_OPERAND_UNSIGNED), { 0 } }  },
/* dcr: destination control register */
  { "dcr", 4, 4, { 0, 0|(1<<CGEN_OPERAND_UNSIGNED), { 0 } }  },
/* simm8: 8 bit signed immediate */
  { "simm8", 8, 8, { 0, 0, { 0 } }  },
/* simm16: 16 bit signed immediate */
  { "simm16", 16, 16, { 0, 0, { 0 } }  },
/* uimm4: 4 bit trap number */
  { "uimm4", 12, 4, { 0, 0|(1<<CGEN_OPERAND_UNSIGNED), { 0 } }  },
/* uimm5: 5 bit shift count */
  { "uimm5", 11, 5, { 0, 0|(1<<CGEN_OPERAND_UNSIGNED), { 0 } }  },
/* uimm16: 16 bit unsigned immediate */
  { "uimm16", 16, 16, { 0, 0|(1<<CGEN_OPERAND_UNSIGNED), { 0 } }  },
/* hi16: high 16 bit immediate, sign optional */
  { "hi16", 16, 16, { 0, 0|(1<<CGEN_OPERAND_SIGN_OPT)|(1<<CGEN_OPERAND_UNSIGNED), { 0 } }  },
/* slo16: 16 bit signed immediate, for low() */
  { "slo16", 16, 16, { 0, 0, { 0 } }  },
/* ulo16: 16 bit unsigned immediate, for low() */
  { "ulo16", 16, 16, { 0, 0|(1<<CGEN_OPERAND_UNSIGNED), { 0 } }  },
/* uimm24: 24 bit address */
  { "uimm24", 8, 24, { 0, 0|(1<<CGEN_OPERAND_RELOC)|(1<<CGEN_OPERAND_ABS_ADDR)|(1<<CGEN_OPERAND_UNSIGNED), { 0 } }  },
/* disp8: 8 bit displacement */
  { "disp8", 8, 8, { 0, 0|(1<<CGEN_OPERAND_RELAX)|(1<<CGEN_OPERAND_RELOC)|(1<<CGEN_OPERAND_PCREL_ADDR), { 0 } }  },
/* disp16: 16 bit displacement */
  { "disp16", 16, 16, { 0, 0|(1<<CGEN_OPERAND_RELOC)|(1<<CGEN_OPERAND_PCREL_ADDR), { 0 } }  },
/* disp24: 24 bit displacement */
  { "disp24", 8, 24, { 0, 0|(1<<CGEN_OPERAND_RELAX)|(1<<CGEN_OPERAND_RELOC)|(1<<CGEN_OPERAND_PCREL_ADDR), { 0 } }  },
};

const struct cgen_insn m32r_cgen_insn_table_entries[CGEN_NUM_INSNS] = {
/* null first entry, end of all hash chains */
  { { 0 }, { 0 } },
/* add $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "add $dr,$sr", "add", "add", {'a', 'd', 'd', ' ', 129, ',', 128, }, 0xf0f0, 0xa0, 16 }
  },
/* add3 $dr,$sr,$slo16 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "add3 $dr,$sr,$slo16", "add3", "add3", {'a', 'd', 'd', '3', ' ', 129, ',', 128, ',', 140, }, 0xf0f00000, 0x80a00000, 32 }
  },
/* and $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "and $dr,$sr", "and", "and", {'a', 'n', 'd', ' ', 129, ',', 128, }, 0xf0f0, 0xc0, 16 }
  },
/* and3 $dr,$sr,$uimm16 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "and3 $dr,$sr,$uimm16", "and3", "and3", {'a', 'n', 'd', '3', ' ', 129, ',', 128, ',', 138, }, 0xf0f00000, 0x80c00000, 32 }
  },
/* or $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "or $dr,$sr", "or", "or", {'o', 'r', ' ', 129, ',', 128, }, 0xf0f0, 0xe0, 16 }
  },
/* or3 $dr,$sr,$ulo16 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "or3 $dr,$sr,$ulo16", "or3", "or3", {'o', 'r', '3', ' ', 129, ',', 128, ',', 141, }, 0xf0f00000, 0x80e00000, 32 }
  },
/* xor $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "xor $dr,$sr", "xor", "xor", {'x', 'o', 'r', ' ', 129, ',', 128, }, 0xf0f0, 0xd0, 16 }
  },
/* xor3 $dr,$sr,$uimm16 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "xor3 $dr,$sr,$uimm16", "xor3", "xor3", {'x', 'o', 'r', '3', ' ', 129, ',', 128, ',', 138, }, 0xf0f00000, 0x80d00000, 32 }
  },
/* addi $dr,$simm8 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "addi $dr,$simm8", "addi", "addi", {'a', 'd', 'd', 'i', ' ', 129, ',', 134, }, 0xf000, 0x4000, 16 }
  },
/* addv $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "addv $dr,$sr", "addv", "addv", {'a', 'd', 'd', 'v', ' ', 129, ',', 128, }, 0xf0f0, 0x80, 16 }
  },
/* addv3 $dr,$sr,$simm16 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "addv3 $dr,$sr,$simm16", "addv3", "addv3", {'a', 'd', 'd', 'v', '3', ' ', 129, ',', 128, ',', 135, }, 0xf0f00000, 0x80800000, 32 }
  },
/* addx $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "addx $dr,$sr", "addx", "addx", {'a', 'd', 'd', 'x', ' ', 129, ',', 128, }, 0xf0f0, 0x90, 16 }
  },
/* bc $disp8 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_RELAX_BC)|(1<<CGEN_INSN_RELAXABLE)|(1<<CGEN_INSN_COND_CTI), { 0 } } },
    { "bc $disp8", "bc8", "bc", {'b', 'c', ' ', 143, }, 0xff00, 0x7c00, 16 }
  },
/* bc.s $disp8 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS)|(1<<CGEN_INSN_COND_CTI), { 0 } } },
    { "bc.s $disp8", "bc8.s", "bc", {'b', 'c', '.', 's', ' ', 143, }, 0xff00, 0x7c00, 16 }
  },
/* bc $disp24 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_RELAX_BC)|(1<<CGEN_INSN_RELAX)|(1<<CGEN_INSN_COND_CTI), { 0 } } },
    { "bc $disp24", "bc24", "bc", {'b', 'c', ' ', 145, }, 0xff000000, 0xfc000000, 32 }
  },
/* bc.l $disp24 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS)|(1<<CGEN_INSN_COND_CTI), { 0 } } },
    { "bc.l $disp24", "bc24.l", "bc", {'b', 'c', '.', 'l', ' ', 145, }, 0xff000000, 0xfc000000, 32 }
  },
/* beq $src1,$src2,$disp16 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_COND_CTI), { 0 } } },
    { "beq $src1,$src2,$disp16", "beq", "beq", {'b', 'e', 'q', ' ', 130, ',', 131, ',', 144, }, 0xf0f00000, 0xb0000000, 32 }
  },
/* beqz $src2,$disp16 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_COND_CTI), { 0 } } },
    { "beqz $src2,$disp16", "beqz", "beqz", {'b', 'e', 'q', 'z', ' ', 131, ',', 144, }, 0xfff00000, 0xb0800000, 32 }
  },
/* bgez $src2,$disp16 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_COND_CTI), { 0 } } },
    { "bgez $src2,$disp16", "bgez", "bgez", {'b', 'g', 'e', 'z', ' ', 131, ',', 144, }, 0xfff00000, 0xb0b00000, 32 }
  },
/* bgtz $src2,$disp16 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_COND_CTI), { 0 } } },
    { "bgtz $src2,$disp16", "bgtz", "bgtz", {'b', 'g', 't', 'z', ' ', 131, ',', 144, }, 0xfff00000, 0xb0d00000, 32 }
  },
/* blez $src2,$disp16 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_COND_CTI), { 0 } } },
    { "blez $src2,$disp16", "blez", "blez", {'b', 'l', 'e', 'z', ' ', 131, ',', 144, }, 0xfff00000, 0xb0c00000, 32 }
  },
/* bltz $src2,$disp16 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_COND_CTI), { 0 } } },
    { "bltz $src2,$disp16", "bltz", "bltz", {'b', 'l', 't', 'z', ' ', 131, ',', 144, }, 0xfff00000, 0xb0a00000, 32 }
  },
/* bnez $src2,$disp16 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_COND_CTI), { 0 } } },
    { "bnez $src2,$disp16", "bnez", "bnez", {'b', 'n', 'e', 'z', ' ', 131, ',', 144, }, 0xfff00000, 0xb0900000, 32 }
  },
/* bl $disp8 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_FILL_SLOT)|(1<<CGEN_INSN_RELAX_BL)|(1<<CGEN_INSN_RELAXABLE)|(1<<CGEN_INSN_UNCOND_CTI), { 0 } } },
    { "bl $disp8", "bl8", "bl", {'b', 'l', ' ', 143, }, 0xff00, 0x7e00, 16 }
  },
/* bl.s $disp8 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_FILL_SLOT)|(1<<CGEN_INSN_ALIAS)|(1<<CGEN_INSN_UNCOND_CTI), { 0 } } },
    { "bl.s $disp8", "bl8.s", "bl", {'b', 'l', '.', 's', ' ', 143, }, 0xff00, 0x7e00, 16 }
  },
/* bl $disp24 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_RELAX_BL)|(1<<CGEN_INSN_RELAX)|(1<<CGEN_INSN_UNCOND_CTI), { 0 } } },
    { "bl $disp24", "bl24", "bl", {'b', 'l', ' ', 145, }, 0xff000000, 0xfe000000, 32 }
  },
/* bl.l $disp24 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS)|(1<<CGEN_INSN_UNCOND_CTI), { 0 } } },
    { "bl.l $disp24", "bl24.l", "bl", {'b', 'l', '.', 'l', ' ', 145, }, 0xff000000, 0xfe000000, 32 }
  },
/* bnc $disp8 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_RELAX_BNC)|(1<<CGEN_INSN_RELAXABLE)|(1<<CGEN_INSN_COND_CTI), { 0 } } },
    { "bnc $disp8", "bnc8", "bnc", {'b', 'n', 'c', ' ', 143, }, 0xff00, 0x7d00, 16 }
  },
/* bnc.s $disp8 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS)|(1<<CGEN_INSN_COND_CTI), { 0 } } },
    { "bnc.s $disp8", "bnc8.s", "bnc", {'b', 'n', 'c', '.', 's', ' ', 143, }, 0xff00, 0x7d00, 16 }
  },
/* bnc $disp24 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_RELAX_BNC)|(1<<CGEN_INSN_RELAX)|(1<<CGEN_INSN_COND_CTI), { 0 } } },
    { "bnc $disp24", "bnc24", "bnc", {'b', 'n', 'c', ' ', 145, }, 0xff000000, 0xfd000000, 32 }
  },
/* bnc.l $disp24 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS)|(1<<CGEN_INSN_COND_CTI), { 0 } } },
    { "bnc.l $disp24", "bnc24.l", "bnc", {'b', 'n', 'c', '.', 'l', ' ', 145, }, 0xff000000, 0xfd000000, 32 }
  },
/* bne $src1,$src2,$disp16 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_COND_CTI), { 0 } } },
    { "bne $src1,$src2,$disp16", "bne", "bne", {'b', 'n', 'e', ' ', 130, ',', 131, ',', 144, }, 0xf0f00000, 0xb0100000, 32 }
  },
/* bra $disp8 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_RELAX_BRA)|(1<<CGEN_INSN_RELAXABLE)|(1<<CGEN_INSN_UNCOND_CTI), { 0 } } },
    { "bra $disp8", "bra8", "bra", {'b', 'r', 'a', ' ', 143, }, 0xff00, 0x7f00, 16 }
  },
/* bra.s $disp8 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS)|(1<<CGEN_INSN_UNCOND_CTI), { 0 } } },
    { "bra.s $disp8", "bra8.s", "bra", {'b', 'r', 'a', '.', 's', ' ', 143, }, 0xff00, 0x7f00, 16 }
  },
/* bra $disp24 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_RELAX_BRA)|(1<<CGEN_INSN_RELAX)|(1<<CGEN_INSN_UNCOND_CTI), { 0 } } },
    { "bra $disp24", "bra24", "bra", {'b', 'r', 'a', ' ', 145, }, 0xff000000, 0xff000000, 32 }
  },
/* bra.l $disp24 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS)|(1<<CGEN_INSN_UNCOND_CTI), { 0 } } },
    { "bra.l $disp24", "bra24.l", "bra", {'b', 'r', 'a', '.', 'l', ' ', 145, }, 0xff000000, 0xff000000, 32 }
  },
/* cmp $src1,$src2 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "cmp $src1,$src2", "cmp", "cmp", {'c', 'm', 'p', ' ', 130, ',', 131, }, 0xf0f0, 0x40, 16 }
  },
/* cmpi $src2,$simm16 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "cmpi $src2,$simm16", "cmpi", "cmpi", {'c', 'm', 'p', 'i', ' ', 131, ',', 135, }, 0xfff00000, 0x80400000, 32 }
  },
/* cmpu $src1,$src2 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "cmpu $src1,$src2", "cmpu", "cmpu", {'c', 'm', 'p', 'u', ' ', 130, ',', 131, }, 0xf0f0, 0x50, 16 }
  },
/* cmpui $src2,$simm16 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "cmpui $src2,$simm16", "cmpui", "cmpui", {'c', 'm', 'p', 'u', 'i', ' ', 131, ',', 135, }, 0xfff00000, 0x80500000, 32 }
  },
/* div $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "div $dr,$sr", "div", "div", {'d', 'i', 'v', ' ', 129, ',', 128, }, 0xf0f0ffff, 0x90000000, 32 }
  },
/* divu $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "divu $dr,$sr", "divu", "divu", {'d', 'i', 'v', 'u', ' ', 129, ',', 128, }, 0xf0f0ffff, 0x90100000, 32 }
  },
/* rem $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "rem $dr,$sr", "rem", "rem", {'r', 'e', 'm', ' ', 129, ',', 128, }, 0xf0f0ffff, 0x90200000, 32 }
  },
/* remu $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "remu $dr,$sr", "remu", "remu", {'r', 'e', 'm', 'u', ' ', 129, ',', 128, }, 0xf0f0ffff, 0x90300000, 32 }
  },
/* jl $sr */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_FILL_SLOT)|(1<<CGEN_INSN_UNCOND_CTI), { 0 } } },
    { "jl $sr", "jl", "jl", {'j', 'l', ' ', 128, }, 0xfff0, 0x1ec0, 16 }
  },
/* jmp $sr */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_UNCOND_CTI), { 0 } } },
    { "jmp $sr", "jmp", "jmp", {'j', 'm', 'p', ' ', 128, }, 0xfff0, 0x1fc0, 16 }
  },
/* ld $dr,@$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "ld $dr,@$sr", "ld", "ld", {'l', 'd', ' ', 129, ',', '@', 128, }, 0xf0f0, 0x20c0, 16 }
  },
/* ld $dr,@($sr) */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "ld $dr,@($sr)", "ld-2", "ld", {'l', 'd', ' ', 129, ',', '@', '(', 128, ')', }, 0xf0f0, 0x20c0, 16 }
  },
/* ld $dr,@($slo16,$sr) */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "ld $dr,@($slo16,$sr)", "ld-d", "ld", {'l', 'd', ' ', 129, ',', '@', '(', 140, ',', 128, ')', }, 0xf0f00000, 0xa0c00000, 32 }
  },
/* ld $dr,@($sr,$slo16) */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "ld $dr,@($sr,$slo16)", "ld-d2", "ld", {'l', 'd', ' ', 129, ',', '@', '(', 128, ',', 140, ')', }, 0xf0f00000, 0xa0c00000, 32 }
  },
/* ldb $dr,@$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "ldb $dr,@$sr", "ldb", "ldb", {'l', 'd', 'b', ' ', 129, ',', '@', 128, }, 0xf0f0, 0x2080, 16 }
  },
/* ldb $dr,@($sr) */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "ldb $dr,@($sr)", "ldb-2", "ldb", {'l', 'd', 'b', ' ', 129, ',', '@', '(', 128, ')', }, 0xf0f0, 0x2080, 16 }
  },
/* ldb $dr,@($slo16,$sr) */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "ldb $dr,@($slo16,$sr)", "ldb-d", "ldb", {'l', 'd', 'b', ' ', 129, ',', '@', '(', 140, ',', 128, ')', }, 0xf0f00000, 0xa0800000, 32 }
  },
/* ldb $dr,@($sr,$slo16) */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "ldb $dr,@($sr,$slo16)", "ldb-d2", "ldb", {'l', 'd', 'b', ' ', 129, ',', '@', '(', 128, ',', 140, ')', }, 0xf0f00000, 0xa0800000, 32 }
  },
/* ldh $dr,@$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "ldh $dr,@$sr", "ldh", "ldh", {'l', 'd', 'h', ' ', 129, ',', '@', 128, }, 0xf0f0, 0x20a0, 16 }
  },
/* ldh $dr,@($sr) */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "ldh $dr,@($sr)", "ldh-2", "ldh", {'l', 'd', 'h', ' ', 129, ',', '@', '(', 128, ')', }, 0xf0f0, 0x20a0, 16 }
  },
/* ldh $dr,@($slo16,$sr) */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "ldh $dr,@($slo16,$sr)", "ldh-d", "ldh", {'l', 'd', 'h', ' ', 129, ',', '@', '(', 140, ',', 128, ')', }, 0xf0f00000, 0xa0a00000, 32 }
  },
/* ldh $dr,@($sr,$slo16) */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "ldh $dr,@($sr,$slo16)", "ldh-d2", "ldh", {'l', 'd', 'h', ' ', 129, ',', '@', '(', 128, ',', 140, ')', }, 0xf0f00000, 0xa0a00000, 32 }
  },
/* ldub $dr,@$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "ldub $dr,@$sr", "ldub", "ldub", {'l', 'd', 'u', 'b', ' ', 129, ',', '@', 128, }, 0xf0f0, 0x2090, 16 }
  },
/* ldub $dr,@($sr) */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "ldub $dr,@($sr)", "ldub-2", "ldub", {'l', 'd', 'u', 'b', ' ', 129, ',', '@', '(', 128, ')', }, 0xf0f0, 0x2090, 16 }
  },
/* ldub $dr,@($slo16,$sr) */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "ldub $dr,@($slo16,$sr)", "ldub-d", "ldub", {'l', 'd', 'u', 'b', ' ', 129, ',', '@', '(', 140, ',', 128, ')', }, 0xf0f00000, 0xa0900000, 32 }
  },
/* ldub $dr,@($sr,$slo16) */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "ldub $dr,@($sr,$slo16)", "ldub-d2", "ldub", {'l', 'd', 'u', 'b', ' ', 129, ',', '@', '(', 128, ',', 140, ')', }, 0xf0f00000, 0xa0900000, 32 }
  },
/* lduh $dr,@$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "lduh $dr,@$sr", "lduh", "lduh", {'l', 'd', 'u', 'h', ' ', 129, ',', '@', 128, }, 0xf0f0, 0x20b0, 16 }
  },
/* lduh $dr,@($sr) */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "lduh $dr,@($sr)", "lduh-2", "lduh", {'l', 'd', 'u', 'h', ' ', 129, ',', '@', '(', 128, ')', }, 0xf0f0, 0x20b0, 16 }
  },
/* lduh $dr,@($slo16,$sr) */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "lduh $dr,@($slo16,$sr)", "lduh-d", "lduh", {'l', 'd', 'u', 'h', ' ', 129, ',', '@', '(', 140, ',', 128, ')', }, 0xf0f00000, 0xa0b00000, 32 }
  },
/* lduh $dr,@($sr,$slo16) */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "lduh $dr,@($sr,$slo16)", "lduh-d2", "lduh", {'l', 'd', 'u', 'h', ' ', 129, ',', '@', '(', 128, ',', 140, ')', }, 0xf0f00000, 0xa0b00000, 32 }
  },
/* ld $dr,@$sr+ */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "ld $dr,@$sr+", "ld-plus", "ld", {'l', 'd', ' ', 129, ',', '@', 128, '+', }, 0xf0f0, 0x20e0, 16 }
  },
/* ld24 $dr,$uimm24 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "ld24 $dr,$uimm24", "ld24", "ld24", {'l', 'd', '2', '4', ' ', 129, ',', 142, }, 0xf0000000, 0xe0000000, 32 }
  },
/* ldi $dr,$simm8 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "ldi $dr,$simm8", "ldi8", "ldi", {'l', 'd', 'i', ' ', 129, ',', 134, }, 0xf000, 0x6000, 16 }
  },
/* ldi8 $dr,$simm8 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "ldi8 $dr,$simm8", "ldi8a", "ldi8", {'l', 'd', 'i', '8', ' ', 129, ',', 134, }, 0xf000, 0x6000, 16 }
  },
/* ldi $dr,$slo16 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "ldi $dr,$slo16", "ldi16", "ldi", {'l', 'd', 'i', ' ', 129, ',', 140, }, 0xf0ff0000, 0x90f00000, 32 }
  },
/* ldi16 $dr,$slo16 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "ldi16 $dr,$slo16", "ldi16a", "ldi16", {'l', 'd', 'i', '1', '6', ' ', 129, ',', 140, }, 0xf0ff0000, 0x90f00000, 32 }
  },
/* lock $dr,@$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "lock $dr,@$sr", "lock", "lock", {'l', 'o', 'c', 'k', ' ', 129, ',', '@', 128, }, 0xf0f0, 0x20d0, 16 }
  },
/* machi $src1,$src2 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "machi $src1,$src2", "machi", "machi", {'m', 'a', 'c', 'h', 'i', ' ', 130, ',', 131, }, 0xf0f0, 0x3040, 16 }
  },
/* maclo $src1,$src2 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "maclo $src1,$src2", "maclo", "maclo", {'m', 'a', 'c', 'l', 'o', ' ', 130, ',', 131, }, 0xf0f0, 0x3050, 16 }
  },
/* macwhi $src1,$src2 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "macwhi $src1,$src2", "macwhi", "macwhi", {'m', 'a', 'c', 'w', 'h', 'i', ' ', 130, ',', 131, }, 0xf0f0, 0x3060, 16 }
  },
/* macwlo $src1,$src2 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "macwlo $src1,$src2", "macwlo", "macwlo", {'m', 'a', 'c', 'w', 'l', 'o', ' ', 130, ',', 131, }, 0xf0f0, 0x3070, 16 }
  },
/* mul $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "mul $dr,$sr", "mul", "mul", {'m', 'u', 'l', ' ', 129, ',', 128, }, 0xf0f0, 0x1060, 16 }
  },
/* mulhi $src1,$src2 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "mulhi $src1,$src2", "mulhi", "mulhi", {'m', 'u', 'l', 'h', 'i', ' ', 130, ',', 131, }, 0xf0f0, 0x3000, 16 }
  },
/* mullo $src1,$src2 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "mullo $src1,$src2", "mullo", "mullo", {'m', 'u', 'l', 'l', 'o', ' ', 130, ',', 131, }, 0xf0f0, 0x3010, 16 }
  },
/* mulwhi $src1,$src2 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "mulwhi $src1,$src2", "mulwhi", "mulwhi", {'m', 'u', 'l', 'w', 'h', 'i', ' ', 130, ',', 131, }, 0xf0f0, 0x3020, 16 }
  },
/* mulwlo $src1,$src2 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "mulwlo $src1,$src2", "mulwlo", "mulwlo", {'m', 'u', 'l', 'w', 'l', 'o', ' ', 130, ',', 131, }, 0xf0f0, 0x3030, 16 }
  },
/* mv $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "mv $dr,$sr", "mv", "mv", {'m', 'v', ' ', 129, ',', 128, }, 0xf0f0, 0x1080, 16 }
  },
/* mvfachi $dr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "mvfachi $dr", "mvfachi", "mvfachi", {'m', 'v', 'f', 'a', 'c', 'h', 'i', ' ', 129, }, 0xf0ff, 0x50f0, 16 }
  },
/* mvfaclo $dr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "mvfaclo $dr", "mvfaclo", "mvfaclo", {'m', 'v', 'f', 'a', 'c', 'l', 'o', ' ', 129, }, 0xf0ff, 0x50f1, 16 }
  },
/* mvfacmi $dr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "mvfacmi $dr", "mvfacmi", "mvfacmi", {'m', 'v', 'f', 'a', 'c', 'm', 'i', ' ', 129, }, 0xf0ff, 0x50f2, 16 }
  },
/* mvfc $dr,$scr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "mvfc $dr,$scr", "mvfc", "mvfc", {'m', 'v', 'f', 'c', ' ', 129, ',', 132, }, 0xf0f0, 0x1090, 16 }
  },
/* mvtachi $src1 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "mvtachi $src1", "mvtachi", "mvtachi", {'m', 'v', 't', 'a', 'c', 'h', 'i', ' ', 130, }, 0xf0ff, 0x5070, 16 }
  },
/* mvtaclo $src1 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "mvtaclo $src1", "mvtaclo", "mvtaclo", {'m', 'v', 't', 'a', 'c', 'l', 'o', ' ', 130, }, 0xf0ff, 0x5071, 16 }
  },
/* mvtc $sr,$dcr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "mvtc $sr,$dcr", "mvtc", "mvtc", {'m', 'v', 't', 'c', ' ', 128, ',', 133, }, 0xf0f0, 0x10a0, 16 }
  },
/* neg $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "neg $dr,$sr", "neg", "neg", {'n', 'e', 'g', ' ', 129, ',', 128, }, 0xf0f0, 0x30, 16 }
  },
/* nop */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "nop", "nop", "nop", {'n', 'o', 'p', }, 0xffff, 0x7000, 16 }
  },
/* not $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "not $dr,$sr", "not", "not", {'n', 'o', 't', ' ', 129, ',', 128, }, 0xf0f0, 0xb0, 16 }
  },
/* rac */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "rac", "rac", "rac", {'r', 'a', 'c', }, 0xffff, 0x5090, 16 }
  },
/* rach */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "rach", "rach", "rach", {'r', 'a', 'c', 'h', }, 0xffff, 0x5080, 16 }
  },
/* rte */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_UNCOND_CTI), { 0 } } },
    { "rte", "rte", "rte", {'r', 't', 'e', }, 0xffff, 0x10d6, 16 }
  },
/* seth $dr,$hi16 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "seth $dr,$hi16", "seth", "seth", {'s', 'e', 't', 'h', ' ', 129, ',', 139, }, 0xf0ff0000, 0xd0c00000, 32 }
  },
/* sll $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "sll $dr,$sr", "sll", "sll", {'s', 'l', 'l', ' ', 129, ',', 128, }, 0xf0f0, 0x1040, 16 }
  },
/* sll3 $dr,$sr,$simm16 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "sll3 $dr,$sr,$simm16", "sll3", "sll3", {'s', 'l', 'l', '3', ' ', 129, ',', 128, ',', 135, }, 0xf0f00000, 0x90c00000, 32 }
  },
/* slli $dr,$uimm5 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "slli $dr,$uimm5", "slli", "slli", {'s', 'l', 'l', 'i', ' ', 129, ',', 137, }, 0xf0e0, 0x5040, 16 }
  },
/* sra $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "sra $dr,$sr", "sra", "sra", {'s', 'r', 'a', ' ', 129, ',', 128, }, 0xf0f0, 0x1020, 16 }
  },
/* sra3 $dr,$sr,$simm16 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "sra3 $dr,$sr,$simm16", "sra3", "sra3", {'s', 'r', 'a', '3', ' ', 129, ',', 128, ',', 135, }, 0xf0f00000, 0x90a00000, 32 }
  },
/* srai $dr,$uimm5 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "srai $dr,$uimm5", "srai", "srai", {'s', 'r', 'a', 'i', ' ', 129, ',', 137, }, 0xf0e0, 0x5020, 16 }
  },
/* srl $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "srl $dr,$sr", "srl", "srl", {'s', 'r', 'l', ' ', 129, ',', 128, }, 0xf0f0, 0x1000, 16 }
  },
/* srl3 $dr,$sr,$simm16 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "srl3 $dr,$sr,$simm16", "srl3", "srl3", {'s', 'r', 'l', '3', ' ', 129, ',', 128, ',', 135, }, 0xf0f00000, 0x90800000, 32 }
  },
/* srli $dr,$uimm5 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "srli $dr,$uimm5", "srli", "srli", {'s', 'r', 'l', 'i', ' ', 129, ',', 137, }, 0xf0e0, 0x5000, 16 }
  },
/* st $src1,@$src2 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "st $src1,@$src2", "st", "st", {'s', 't', ' ', 130, ',', '@', 131, }, 0xf0f0, 0x2040, 16 }
  },
/* st $src1,@($src2) */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "st $src1,@($src2)", "st-2", "st", {'s', 't', ' ', 130, ',', '@', '(', 131, ')', }, 0xf0f0, 0x2040, 16 }
  },
/* st $src1,@($slo16,$src2) */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "st $src1,@($slo16,$src2)", "st-d", "st", {'s', 't', ' ', 130, ',', '@', '(', 140, ',', 131, ')', }, 0xf0f00000, 0xa0400000, 32 }
  },
/* st $src1,@($src2,$slo16) */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "st $src1,@($src2,$slo16)", "st-d2", "st", {'s', 't', ' ', 130, ',', '@', '(', 131, ',', 140, ')', }, 0xf0f00000, 0xa0400000, 32 }
  },
/* stb $src1,@$src2 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "stb $src1,@$src2", "stb", "stb", {'s', 't', 'b', ' ', 130, ',', '@', 131, }, 0xf0f0, 0x2000, 16 }
  },
/* stb $src1,@($src2) */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "stb $src1,@($src2)", "stb-2", "stb", {'s', 't', 'b', ' ', 130, ',', '@', '(', 131, ')', }, 0xf0f0, 0x2000, 16 }
  },
/* stb $src1,@($slo16,$src2) */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "stb $src1,@($slo16,$src2)", "stb-d", "stb", {'s', 't', 'b', ' ', 130, ',', '@', '(', 140, ',', 131, ')', }, 0xf0f00000, 0xa0000000, 32 }
  },
/* stb $src1,@($src2,$slo16) */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "stb $src1,@($src2,$slo16)", "stb-d2", "stb", {'s', 't', 'b', ' ', 130, ',', '@', '(', 131, ',', 140, ')', }, 0xf0f00000, 0xa0000000, 32 }
  },
/* sth $src1,@$src2 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "sth $src1,@$src2", "sth", "sth", {'s', 't', 'h', ' ', 130, ',', '@', 131, }, 0xf0f0, 0x2020, 16 }
  },
/* sth $src1,@($src2) */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "sth $src1,@($src2)", "sth-2", "sth", {'s', 't', 'h', ' ', 130, ',', '@', '(', 131, ')', }, 0xf0f0, 0x2020, 16 }
  },
/* sth $src1,@($slo16,$src2) */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "sth $src1,@($slo16,$src2)", "sth-d", "sth", {'s', 't', 'h', ' ', 130, ',', '@', '(', 140, ',', 131, ')', }, 0xf0f00000, 0xa0200000, 32 }
  },
/* sth $src1,@($src2,$slo16) */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "sth $src1,@($src2,$slo16)", "sth-d2", "sth", {'s', 't', 'h', ' ', 130, ',', '@', '(', 131, ',', 140, ')', }, 0xf0f00000, 0xa0200000, 32 }
  },
/* st $src1,@+$src2 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "st $src1,@+$src2", "st-plus", "st", {'s', 't', ' ', 130, ',', '@', '+', 131, }, 0xf0f0, 0x2060, 16 }
  },
/* st $src1,@-$src2 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "st $src1,@-$src2", "st-minus", "st", {'s', 't', ' ', 130, ',', '@', '-', 131, }, 0xf0f0, 0x2070, 16 }
  },
/* sub $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "sub $dr,$sr", "sub", "sub", {'s', 'u', 'b', ' ', 129, ',', 128, }, 0xf0f0, 0x20, 16 }
  },
/* subv $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "subv $dr,$sr", "subv", "subv", {'s', 'u', 'b', 'v', ' ', 129, ',', 128, }, 0xf0f0, 0x0, 16 }
  },
/* subx $dr,$sr */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "subx $dr,$sr", "subx", "subx", {'s', 'u', 'b', 'x', ' ', 129, ',', 128, }, 0xf0f0, 0x10, 16 }
  },
/* trap $uimm4 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_FILL_SLOT)|(1<<CGEN_INSN_UNCOND_CTI), { 0 } } },
    { "trap $uimm4", "trap", "trap", {'t', 'r', 'a', 'p', ' ', 136, }, 0xfff0, 0x10f0, 16 }
  },
/* unlock $src1,@$src2 */
  {
    { 1, 1, 1, 1, { 0, 0, { 0 } } },
    { "unlock $src1,@$src2", "unlock", "unlock", {'u', 'n', 'l', 'o', 'c', 'k', ' ', 130, ',', '@', 131, }, 0xf0f0, 0x2050, 16 }
  },
/* push $src1 */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "push $src1", "push", "push", {'p', 'u', 's', 'h', ' ', 130, }, 0xf0ff, 0x207f, 16 }
  },
/* pop $dr */
  {
    { 1, 1, 1, 1, { 0, 0|(1<<CGEN_INSN_ALIAS), { 0 } } },
    { "pop $dr", "pop", "pop", {'p', 'o', 'p', ' ', 129, }, 0xf0ff, 0x20ef, 16 }
  },
};

CGEN_INSN_TABLE m32r_cgen_insn_table = {
  & m32r_cgen_insn_table_entries[0],
  CGEN_NUM_INSNS,
  NULL,
  m32r_cgen_asm_hash_insn, CGEN_ASM_HASH_SIZE,
  m32r_cgen_dis_hash_insn, CGEN_DIS_HASH_SIZE
};

/* The hash functions are recorded here to help keep assembler code out of
   the disassembler and vice versa.  */

unsigned int
m32r_cgen_asm_hash_insn (insn)
     const char *insn;
{
  return CGEN_ASM_HASH (insn);
}

unsigned int
m32r_cgen_dis_hash_insn (buf, value)
     const char *buf;
     unsigned long value;
{
  return CGEN_DIS_HASH (buf, value);
}

CGEN_OPCODE_DATA m32r_cgen_opcode_data = {
  & m32r_cgen_hw_entries[0],
  & m32r_cgen_insn_table,
};

void
m32r_cgen_init_tables (mach)
    int mach;
{
}

/* Main entry point for stuffing values in cgen_fields.  */

CGEN_INLINE void
m32r_cgen_set_operand (opindex, valuep, fields)
     int opindex;
     const long *valuep;
     struct cgen_fields *fields;
{
  switch (opindex)
    {
    case 0 :
      fields->f_r2 = *valuep;
      break;
    case 1 :
      fields->f_r1 = *valuep;
      break;
    case 2 :
      fields->f_r1 = *valuep;
      break;
    case 3 :
      fields->f_r2 = *valuep;
      break;
    case 4 :
      fields->f_r2 = *valuep;
      break;
    case 5 :
      fields->f_r1 = *valuep;
      break;
    case 6 :
      fields->f_simm8 = *valuep;
      break;
    case 7 :
      fields->f_simm16 = *valuep;
      break;
    case 8 :
      fields->f_uimm4 = *valuep;
      break;
    case 9 :
      fields->f_uimm5 = *valuep;
      break;
    case 10 :
      fields->f_uimm16 = *valuep;
      break;
    case 11 :
      fields->f_hi16 = *valuep;
      break;
    case 12 :
      fields->f_simm16 = *valuep;
      break;
    case 13 :
      fields->f_uimm16 = *valuep;
      break;
    case 14 :
      fields->f_uimm24 = *valuep;
      break;
    case 15 :
      fields->f_disp8 = *valuep;
      break;
    case 16 :
      fields->f_disp16 = *valuep;
      break;
    case 17 :
      fields->f_disp24 = *valuep;
      break;

    default :
      fprintf (stderr, "Unrecognized field %d while setting operand.\n",
		       opindex);
      abort ();
  }
}

/* Main entry point for getting values from cgen_fields.  */

CGEN_INLINE long
m32r_cgen_get_operand (opindex, fields)
     int opindex;
     const struct cgen_fields *fields;
{
  long value;

  switch (opindex)
    {
    case 0 :
      value = fields->f_r2;
      break;
    case 1 :
      value = fields->f_r1;
      break;
    case 2 :
      value = fields->f_r1;
      break;
    case 3 :
      value = fields->f_r2;
      break;
    case 4 :
      value = fields->f_r2;
      break;
    case 5 :
      value = fields->f_r1;
      break;
    case 6 :
      value = fields->f_simm8;
      break;
    case 7 :
      value = fields->f_simm16;
      break;
    case 8 :
      value = fields->f_uimm4;
      break;
    case 9 :
      value = fields->f_uimm5;
      break;
    case 10 :
      value = fields->f_uimm16;
      break;
    case 11 :
      value = fields->f_hi16;
      break;
    case 12 :
      value = fields->f_simm16;
      break;
    case 13 :
      value = fields->f_uimm16;
      break;
    case 14 :
      value = fields->f_uimm24;
      break;
    case 15 :
      value = fields->f_disp8;
      break;
    case 16 :
      value = fields->f_disp16;
      break;
    case 17 :
      value = fields->f_disp24;
      break;

    default :
      fprintf (stderr, "Unrecognized field %d while getting operand.\n",
		       opindex);
      abort ();
  }

  return value;
}


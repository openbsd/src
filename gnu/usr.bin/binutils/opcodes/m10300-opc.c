/* Assemble Matsushita MN10300 instructions.
   Copyright (C) 1996, 1997 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "ansidecl.h"
#include "opcode/mn10300.h"


const struct mn10300_operand mn10300_operands[] = {
#define UNUSED	0
  {0, 0, 0}, 

/* dn register in the first register operand position.  */
#define DN0      (UNUSED+1)
  {2, 0, MN10300_OPERAND_DREG},

/* dn register in the second register operand position.  */
#define DN1      (DN0+1)
  {2, 2, MN10300_OPERAND_DREG},

/* dn register in the third register operand position.  */
#define DN2      (DN1+1)
  {2, 4, MN10300_OPERAND_DREG},

/* dm register in the first register operand position.  */
#define DM0      (DN2+1)
  {2, 0, MN10300_OPERAND_DREG},

/* dm register in the second register operand position.  */
#define DM1      (DM0+1)
  {2, 2, MN10300_OPERAND_DREG},

/* dm register in the third register operand position.  */
#define DM2      (DM1+1)
  {2, 4, MN10300_OPERAND_DREG},

/* an register in the first register operand position.  */
#define AN0      (DM2+1)
  {2, 0, MN10300_OPERAND_AREG},

/* an register in the second register operand position.  */
#define AN1      (AN0+1)
  {2, 2, MN10300_OPERAND_AREG},

/* an register in the third register operand position.  */
#define AN2      (AN1+1)
  {2, 4, MN10300_OPERAND_AREG},

/* am register in the first register operand position.  */
#define AM0      (AN2+1)
  {2, 0, MN10300_OPERAND_AREG},

/* am register in the second register operand position.  */
#define AM1      (AM0+1)
  {2, 2, MN10300_OPERAND_AREG},

/* am register in the third register operand position.  */
#define AM2      (AM1+1)
  {2, 4, MN10300_OPERAND_AREG},

/* 8 bit unsigned immediate which may promote to a 16bit
   unsigned immediate.  */
#define IMM8    (AM2+1)
  {8, 0, MN10300_OPERAND_PROMOTE},

/* 16 bit unsigned immediate which may promote to a 32bit
   unsigned immediate.  */
#define IMM16    (IMM8+1)
  {16, 0, MN10300_OPERAND_PROMOTE},

/* 16 bit pc-relative immediate which may promote to a 16bit
   pc-relative immediate.  */
#define IMM16_PCREL    (IMM16+1)
  {16, 0, MN10300_OPERAND_PCREL | MN10300_OPERAND_RELAX | MN10300_OPERAND_SIGNED},

/* 16bit unsigned dispacement in a memory operation which
   may promote to a 32bit displacement.  */
#define IMM16_MEM    (IMM16_PCREL+1)
  {16, 0, MN10300_OPERAND_PROMOTE | MN10300_OPERAND_MEMADDR},

/* 32bit immediate, high 16 bits in the main instruction
   word, 16bits in the extension word. 

   The "bits" field indicates how many bits are in the
   main instruction word for MN10300_OPERAND_SPLIT!  */
#define IMM32    (IMM16_MEM+1)
  {16, 0, MN10300_OPERAND_SPLIT},

/* 32bit pc-relative offset.  */
#define IMM32_PCREL    (IMM32+1)
  {16, 0, MN10300_OPERAND_SPLIT | MN10300_OPERAND_PCREL},

/* 32bit memory offset.  */
#define IMM32_MEM    (IMM32_PCREL+1)
  {16, 0, MN10300_OPERAND_SPLIT | MN10300_OPERAND_MEMADDR},

/* 32bit immediate, high 16 bits in the main instruction
   word, 16bits in the extension word, low 16bits are left
   shifted 8 places. 

   The "bits" field indicates how many bits are in the
   main instruction word for MN10300_OPERAND_SPLIT!  */
#define IMM32_LOWSHIFT8    (IMM32_MEM+1)
  {16, 8, MN10300_OPERAND_SPLIT | MN10300_OPERAND_MEMADDR},

/* 32bit immediate, high 24 bits in the main instruction
   word, 8 in the extension word.

   The "bits" field indicates how many bits are in the
   main instruction word for MN10300_OPERAND_SPLIT!  */
#define IMM32_HIGH24    (IMM32_LOWSHIFT8+1)
  {24, 0, MN10300_OPERAND_SPLIT | MN10300_OPERAND_PCREL},

/* 32bit immediate, high 24 bits in the main instruction
   word, 8 in the extension word, low 8 bits are left
   shifted 16 places. 

   The "bits" field indicates how many bits are in the
   main instruction word for MN10300_OPERAND_SPLIT!  */
#define IMM32_HIGH24_LOWSHIFT16    (IMM32_HIGH24+1)
  {24, 16, MN10300_OPERAND_SPLIT | MN10300_OPERAND_PCREL},

/* Stack pointer.  */
#define SP    (IMM32_HIGH24_LOWSHIFT16+1)
  {8, 0, MN10300_OPERAND_SP},

/* Processor status word.  */
#define PSW    (SP+1)
  {0, 0, MN10300_OPERAND_PSW},

/* MDR register.  */
#define MDR    (PSW+1)
  {0, 0, MN10300_OPERAND_MDR},

/* Index register.  */
#define DI (MDR+1)
  {2, 2, MN10300_OPERAND_DREG},

/* 8 bit signed displacement, may promote to 16bit signed dispacement.  */
#define SD8    (DI+1)
  {8, 0, MN10300_OPERAND_SIGNED | MN10300_OPERAND_PROMOTE},

/* 16 bit signed displacement, may promote to 32bit dispacement.  */
#define SD16    (SD8+1)
  {16, 0, MN10300_OPERAND_SIGNED | MN10300_OPERAND_PROMOTE},

/* 8 bit signed displacement that can not promote.  */
#define SD8N    (SD16+1)
  {8, 0, MN10300_OPERAND_SIGNED},

/* 8 bit pc-relative displacement.  */
#define SD8N_PCREL    (SD8N+1)
  {8, 0, MN10300_OPERAND_SIGNED | MN10300_OPERAND_PCREL | MN10300_OPERAND_RELAX},

/* 8 bit signed displacement shifted left 8 bits in the instruction.  */
#define SD8N_SHIFT8    (SD8N_PCREL+1)
  {8, 8, MN10300_OPERAND_SIGNED},

/* 8 bit signed immediate which may promote to 16bit signed immediate.  */
#define SIMM8    (SD8N_SHIFT8+1)
  {8, 0, MN10300_OPERAND_SIGNED | MN10300_OPERAND_PROMOTE},

/* 16 bit signed immediate which may promote to 32bit  immediate.  */
#define SIMM16    (SIMM8+1)
  {16, 0, MN10300_OPERAND_SIGNED | MN10300_OPERAND_PROMOTE},

/* Either an open paren or close paren.  */
#define PAREN	(SIMM16+1)
  {0, 0, MN10300_OPERAND_PAREN}, 

/* dn register that appears in the first and second register positions.  */
#define DN01     (PAREN+1)
  {2, 0, MN10300_OPERAND_DREG | MN10300_OPERAND_REPEATED},

/* an register that appears in the first and second register positions.  */
#define AN01     (DN01+1)
  {2, 0, MN10300_OPERAND_AREG | MN10300_OPERAND_REPEATED},

/* 16bit pc-relative displacement which may promote to 32bit pc-relative
   displacement.  */
#define D16_SHIFT (AN01+1)
  {16, 8, MN10300_OPERAND_PCREL | MN10300_OPERAND_RELAX | MN10300_OPERAND_SIGNED},

/* 8 bit immediate found in the extension word.  */
#define IMM8E    (D16_SHIFT+1)
  {8, 0, MN10300_OPERAND_EXTENDED},

/* Register list found in the extension word shifted 8 bits left.  */
#define REGSE_SHIFT8    (IMM8E+1)
  {8, 8, MN10300_OPERAND_EXTENDED | MN10300_OPERAND_REG_LIST},

/* Register list shifted 8 bits left.  */
#define REGS_SHIFT8 (REGSE_SHIFT8 + 1)
  {8, 8, MN10300_OPERAND_REG_LIST},

/* Reigster list.  */
#define REGS    (REGS_SHIFT8+1)
  {8, 0, MN10300_OPERAND_REG_LIST},
} ; 

#define MEM(ADDR) PAREN, ADDR, PAREN 
#define MEM2(ADDR1,ADDR2) PAREN, ADDR1, ADDR2, PAREN 

/* The opcode table.

   The format of the opcode table is:

   NAME		OPCODE		MASK		{ OPERANDS }

   NAME is the name of the instruction.
   OPCODE is the instruction opcode.
   MASK is the opcode mask; this is used to tell the disassembler
     which bits in the actual opcode must match OPCODE.
   OPERANDS is the list of operands.

   The disassembler reads the table in order and prints the first
   instruction which matches, so this table is sorted to put more
   specific instructions before more general instructions.  It is also
   sorted by major opcode.  */

const struct mn10300_opcode mn10300_opcodes[] = {
{ "mov",	0x8000,		0xf000,		FMT_S1, {SIMM8, DN01}},
{ "mov",	0x80,		0xf0,		FMT_S0, {DM1, DN0}},
{ "mov",	0xf1e0,		0xfff0,		FMT_D0, {DM1, AN0}},
{ "mov",	0xf1d0,		0xfff0,		FMT_D0, {AM1, DN0}},
{ "mov",	0x9000,		0xf000,		FMT_S1, {IMM8, AN01}},
{ "mov",	0x90,		0xf0,		FMT_S0, {AM1, AN0}},
{ "mov",	0x3c,		0xfc,		FMT_S0, {SP, AN0}},
{ "mov",	0xf2f0,		0xfff3,		FMT_D0, {AM1, SP}},
{ "mov",	0xf2e4,		0xfffc,		FMT_D0, {PSW, DN0}},
{ "mov",	0xf2f3,		0xfff3,		FMT_D0, {DM1, PSW}},
{ "mov",	0xf2e0,		0xfffc,		FMT_D0, {MDR, DN0}},
{ "mov",	0xf2f2,		0xfff3,		FMT_D0, {DM1, MDR}},
{ "mov",	0x70,		0xf0,		FMT_S0, {MEM(AM0), DN1}},
{ "mov",	0xf80000,	0xfff000,	FMT_D1, {MEM2(SD8, AM0), DN1}},
{ "mov",	0xfa000000,	0xfff00000,	FMT_D2, {MEM2(SD16, AM0), DN1}},
{ "mov",	0xfc000000,	0xfff00000,	FMT_D4, {MEM2(IMM32,AM0), DN1}},
{ "mov",	0x5800,		0xfc00,		FMT_S1, {MEM2(IMM8, SP), DN0}},
{ "mov",	0xfab40000,	0xfffc0000,	FMT_D2, {MEM2(IMM16, SP), DN0}},
{ "mov",	0xfcb40000,	0xfffc0000,	FMT_D4, {MEM2(IMM32, SP), DN0}},
{ "mov",	0xf300,		0xffc0,		FMT_D0, {MEM2(DI, AM0), DN2}},
{ "mov",	0x300000,	0xfc0000,	FMT_S2, {MEM(IMM16_MEM), DN0}},
{ "mov",	0xfca40000,	0xfffc0000,	FMT_D4, {MEM(IMM32_MEM), DN0}},
{ "mov",	0xf000,		0xfff0,		FMT_D0, {MEM(AM0), AN1}},
{ "mov",	0xf82000,	0xfff000,	FMT_D1, {MEM2(SD8,AM0), AN1}},
{ "mov",	0xfa200000,	0xfff00000,	FMT_D2, {MEM2(SD16, AM0), AN1}},
{ "mov",	0xfc200000,	0xfff00000,	FMT_D4, {MEM2(IMM32,AM0), AN1}},
{ "mov",	0x5c00,		0xfc00,		FMT_S1, {MEM2(IMM8, SP), AN0}},
{ "mov",	0xfab00000,	0xfffc0000,	FMT_D2, {MEM2(IMM16, SP), AN0}},
{ "mov",	0xfcb00000,	0xfffc0000,	FMT_D4, {MEM2(IMM32, SP), AN0}},
{ "mov",	0xf380,		0xffc0,		FMT_D0, {MEM2(DI, AM0), AN2}},
{ "mov",	0xfaa00000,	0xfffc0000,	FMT_D2, {MEM(IMM16_MEM), AN0}},
{ "mov",	0xfca00000,	0xfffc0000,	FMT_D4, {MEM(IMM32_MEM), AN0}},
{ "mov",	0xf8f000,	0xfffc00,	FMT_D1, {MEM2(SD8N, AM0), SP}},
{ "mov",	0x60,		0xf0,		FMT_S0, {DM1, MEM(AN0)}},
{ "mov",	0xf81000,	0xfff000,	FMT_D1, {DM1, MEM2(SD8, AN0)}},
{ "mov",	0xfa100000,	0xfff00000,	FMT_D2, {DM1, MEM2(SD16, AN0)}},
{ "mov",	0xfc100000,	0xfff00000,	FMT_D4, {DM1, MEM2(IMM32,AN0)}},
{ "mov",	0x4200,		0xf300,		FMT_S1, {DM1, MEM2(IMM8, SP)}},
{ "mov",	0xfa910000,	0xfff30000,	FMT_D2, {DM1, MEM2(IMM16, SP)}},
{ "mov",	0xfc910000,	0xfff30000,	FMT_D4, {DM1, MEM2(IMM32, SP)}},
{ "mov",	0xf340,		0xffc0,		FMT_D0, {DM2, MEM2(DI, AN0)}},
{ "mov",	0x010000,	0xf30000,	FMT_S2, {DM1, MEM(IMM16_MEM)}},
{ "mov",	0xfc810000,	0xfff30000,	FMT_D4, {DM1, MEM(IMM32_MEM)}},
{ "mov",	0xf010,		0xfff0,		FMT_D0, {AM1, MEM(AN0)}},
{ "mov",	0xf83000,	0xfff000,	FMT_D1, {AM1, MEM2(SD8, AN0)}},
{ "mov",	0xfa300000,	0xfff00000,	FMT_D2, {AM1, MEM2(SD16, AN0)}},
{ "mov",	0xfc300000,	0xfff00000,	FMT_D4, {AM1, MEM2(IMM32,AN0)}},
{ "mov",	0x4300,		0xf300,		FMT_S1, {AM1, MEM2(IMM8, SP)}},
{ "mov",	0xfa900000,	0xfff30000,	FMT_D2, {AM1, MEM2(IMM16, SP)}},
{ "mov",	0xfc900000,	0xfff30000,	FMT_D4, {AM1, MEM2(IMM32, SP)}},
{ "mov",	0xf3c0,		0xffc0,		FMT_D0, {AM2, MEM2(DI, AN0)}},
{ "mov",	0xfa800000,	0xfff30000,	FMT_D2, {AM1, MEM(IMM16_MEM)}},
{ "mov",	0xfc800000,	0xfff30000,	FMT_D4, {AM1, MEM(IMM32_MEM)}},
{ "mov",	0xf8f400,	0xfffc00,	FMT_D1, {SP, MEM2(SD8N, AN0)}},
{ "mov",	0x2c0000,	0xfc0000,	FMT_S2, {SIMM16, DN0}},
{ "mov",	0xfccc0000,	0xfffc0000,	FMT_D4, {IMM32, DN0}},
{ "mov",	0x240000,	0xfc0000,	FMT_S2, {IMM16, AN0}},
{ "mov",	0xfcdc0000,	0xfffc0000,	FMT_D4, {IMM32, AN0}},

{ "movbu",	0xf040,		0xfff0,		FMT_D0, {MEM(AM0), DN1}},
{ "movbu",	0xf84000,	0xfff000,	FMT_D1, {MEM2(SD8, AM0), DN1}},
{ "movbu",	0xfa400000,	0xfff00000,	FMT_D2, {MEM2(SD16, AM0), DN1}},
{ "movbu",	0xfc400000,	0xfff00000,	FMT_D4, {MEM2(IMM32,AM0), DN1}},
{ "movbu",	0xf8b800,	0xfffc00,	FMT_D1, {MEM2(IMM8, SP), DN0}},
{ "movbu",	0xfab80000,	0xfffc0000,	FMT_D2, {MEM2(IMM16, SP), DN0}},
{ "movbu",	0xfcb80000,	0xfffc0000,	FMT_D4, {MEM2(IMM32, SP), DN0}},
{ "movbu",	0xf400,		0xffc0,		FMT_D0, {MEM2(DI, AM0), DN2}},
{ "movbu",	0x340000,	0xfc0000,	FMT_S2, {MEM(IMM16_MEM), DN0}},
{ "movbu",	0xfca80000,	0xfffc0000,	FMT_D4, {MEM(IMM32_MEM), DN0}},
{ "movbu",	0xf050,		0xfff0,		FMT_D0, {DM1, MEM(AN0)}},
{ "movbu",	0xf85000,	0xfff000,	FMT_D1, {DM1, MEM2(SD8, AN0)}},
{ "movbu",	0xfa500000,	0xfff00000,	FMT_D2, {DM1, MEM2(SD16, AN0)}},
{ "movbu",	0xfc500000,	0xfff00000,	FMT_D4, {DM1, MEM2(IMM32,AN0)}},
{ "movbu",	0xf89200,	0xfff300,	FMT_D1, {DM1, MEM2(IMM8, SP)}},
{ "movbu",	0xfa920000,	0xfff30000,	FMT_D2, {DM1, MEM2(IMM16, SP)}},
{ "movbu",	0xfc920000,	0xfff30000,	FMT_D4, {DM1, MEM2(IMM32, SP)}},
{ "movbu",	0xf440,		0xffc0,		FMT_D0, {DM2, MEM2(DI, AN0)}},
{ "movbu",	0x020000,	0xf30000,	FMT_S2, {DM1, MEM(IMM16_MEM)}},
{ "movbu",	0xfc820000,	0xfff30000,	FMT_D4, {DM1, MEM(IMM32_MEM)}},

{ "movhu",	0xf060,		0xfff0,		FMT_D0, {MEM(AM0), DN1}},
{ "movhu",	0xf86000,	0xfff000,	FMT_D1, {MEM2(SD8, AM0), DN1}},
{ "movhu",	0xfa600000,	0xfff00000,	FMT_D2, {MEM2(SD16, AM0), DN1}},
{ "movhu",	0xfc600000,	0xfff00000,	FMT_D4, {MEM2(IMM32,AM0), DN1}},
{ "movhu",	0xf8bc00,	0xfffc00,	FMT_D1, {MEM2(IMM8, SP), DN0}},
{ "movhu",	0xfabc0000,	0xfffc0000,	FMT_D2, {MEM2(IMM16, SP), DN0}},
{ "movhu",	0xfcbc0000,	0xfffc0000,	FMT_D4, {MEM2(IMM32, SP), DN0}},
{ "movhu",	0xf480,		0xffc0,		FMT_D0, {MEM2(DI, AM0), DN2}},
{ "movhu",	0x380000,	0xfc0000,	FMT_S2, {MEM(IMM16_MEM), DN0}},
{ "movhu",	0xfcac0000,	0xfffc0000,	FMT_D4, {MEM(IMM32_MEM), DN0}},
{ "movhu",	0xf070,		0xfff0,		FMT_D0, {DM1, MEM(AN0)}},
{ "movhu",	0xf87000,	0xfff000,	FMT_D1, {DM1, MEM2(SD8, AN0)}},
{ "movhu",	0xfa700000,	0xfff00000,	FMT_D2, {DM1, MEM2(SD16, AN0)}},
{ "movhu",	0xfc700000,	0xfff00000,	FMT_D4, {DM1, MEM2(IMM32,AN0)}},
{ "movhu",	0xf89300,	0xfff300,	FMT_D1, {DM1, MEM2(IMM8, SP)}},
{ "movhu",	0xfa930000,	0xfff30000,	FMT_D2, {DM1, MEM2(IMM16, SP)}},
{ "movhu",	0xfc930000,	0xfff30000,	FMT_D4, {DM1, MEM2(IMM32, SP)}},
{ "movhu",	0xf4c0,		0xffc0,		FMT_D0, {DM2, MEM2(DI, AN0)}},
{ "movhu",	0x030000,	0xf30000,	FMT_S2, {DM1, MEM(IMM16_MEM)}},
{ "movhu",	0xfc830000,	0xfff30000,	FMT_D4, {DM1, MEM(IMM32_MEM)}},

{ "ext",	0xf2d0,		0xfffc,		FMT_D0, {DN0}},
{ "extb",	0x10, 		0xfc,		FMT_S0, {DN0}},
{ "extbu",	0x14,		0xfc,		FMT_S0, {DN0}},
{ "exth",	0x18,		0xfc,		FMT_S0, {DN0}},
{ "exthu",	0x1c,		0xfc,		FMT_S0, {DN0}},

{ "movm",	0xce00,		0xff00,		FMT_S1, {MEM(SP), REGS}},
{ "movm",	0xcf00,		0xff00,		FMT_S1, {REGS, MEM(SP)}},

{ "clr",	0x00,		0xf3,		FMT_S0, {DN1}},

{ "add",	0xe0,		0xf0,		FMT_S0, {DM1, DN0}},
{ "add",	0xf160,		0xfff0,		FMT_D0, {DM1, AN0}},
{ "add",	0xf150,		0xfff0,		FMT_D0, {AM1, DN0}},
{ "add",	0xf170,		0xfff0,		FMT_D0, {AM1, AN0}},
{ "add",	0x2800,		0xfc00,		FMT_S1, {SIMM8, DN0}},
{ "add",	0xfac00000,	0xfffc0000,	FMT_D2, {SIMM16, DN0}},
{ "add",	0xfcc00000,	0xfffc0000,	FMT_D4, {IMM32, DN0}},
{ "add",	0x2000,		0xfc00,		FMT_S1, {SIMM8, AN0}},
{ "add",	0xfad00000,	0xfffc0000,	FMT_D2, {SIMM16, AN0}},
{ "add",	0xfcd00000,	0xfffc0000,	FMT_D4, {IMM32, AN0}},
{ "add",	0xf8fe00,	0xffff00,	FMT_D1, {SIMM8, SP}},
{ "add",	0xfafe0000,	0xffff0000,	FMT_D2, {SIMM16, SP}},
{ "add",	0xfcfe0000,	0xffff0000,	FMT_D4, {IMM32, SP}},
{ "addc",	0xf140,		0xfff0,		FMT_D0, {DM1, DN0}},

{ "sub",	0xf100,		0xfff0,		FMT_D0, {DM1, DN0}},
{ "sub",	0xf120,		0xfff0,		FMT_D0, {DM1, AN0}},
{ "sub",	0xf110,		0xfff0,		FMT_D0, {AM1, DN0}},
{ "sub",	0xf130,		0xfff0,		FMT_D0, {AM1, AN0}},
{ "sub",	0xfcc40000,	0xfffc0000,	FMT_D4, {IMM32, DN0}},
{ "sub",	0xfcd40000,	0xfffc0000,	FMT_D4, {IMM32, AN0}},
{ "subc",	0xf180,		0xfff0,		FMT_D0, {DM1, DN0}},

{ "mul",	0xf240,		0xfff0,		FMT_D0, {DM1, DN0}},
{ "mulu",	0xf250,		0xfff0,		FMT_D0, {DM1, DN0}},

{ "div",	0xf260,		0xfff0,		FMT_D0, {DM1, DN0}},
{ "divu",	0xf270,		0xfff0,		FMT_D0, {DM1, DN0}},

{ "inc",	0x40,		0xf3,		FMT_S0, {DN1}},
{ "inc",	0x41,		0xf3,		FMT_S0, {AN1}},
{ "inc4",	0x50,		0xfc,		FMT_S0, {AN0}},

{ "cmp",	0xa000,		0xf000,		FMT_S1, {SIMM8, DN01}},
{ "cmp",	0xa0,		0xf0,		FMT_S0, {DM1, DN0}},
{ "cmp",	0xf1a0,		0xfff0,		FMT_D0, {DM1, AN0}},
{ "cmp",	0xf190,		0xfff0,		FMT_D0, {AM1, DN0}},
{ "cmp",	0xb000,		0xf000,		FMT_S1, {IMM8, AN01}},
{ "cmp",	0xb0,		0xf0,		FMT_S0, {AM1, AN0}},
{ "cmp",	0xfac80000,	0xfffc0000,	FMT_D2, {SIMM16, DN0}},
{ "cmp",	0xfcc80000,	0xfffc0000,	FMT_D4, {IMM32, DN0}},
{ "cmp",	0xfad80000,	0xfffc0000,	FMT_D2, {IMM16, AN0}},
{ "cmp",	0xfcd80000,	0xfffc0000,	FMT_D4, {IMM32, AN0}},

{ "and",	0xf200,		0xfff0,		FMT_D0, {DM1, DN0}},
{ "and",	0xf8e000,	0xfffc00,	FMT_D1, {IMM8, DN0}},
{ "and",	0xfae00000,	0xfffc0000,	FMT_D2, {IMM16, DN0}},
{ "and",	0xfce00000,	0xfffc0000,	FMT_D4, {IMM32, DN0}},
{ "and",	0xfafc0000,	0xffff0000,	FMT_D2, {IMM16, PSW}},
{ "or",		0xf210,		0xfff0,		FMT_D0, {DM1, DN0}},
{ "or",		0xf8e400,	0xfffc00,	FMT_D1, {IMM8, DN0}},
{ "or",		0xfae40000,	0xfffc0000,	FMT_D2, {IMM16, DN0}},
{ "or",		0xfce40000,	0xfffc0000,	FMT_D4, {IMM32, DN0}},
{ "or",		0xfafd0000,	0xffff0000,	FMT_D2, {IMM16, PSW}},
{ "xor",	0xf220,		0xfff0,		FMT_D0, {DM1, DN0}},
{ "xor",	0xfae80000,	0xfffc0000,	FMT_D2, {IMM16, DN0}},
{ "xor",	0xfce80000,	0xfffc0000,	FMT_D4, {IMM32, DN0}},
{ "not",	0xf230,		0xfffc,		FMT_D0, {DN0}},

{ "btst",	0xf8ec00,	0xfffc00,	FMT_D1, {IMM8, DN0}},
{ "btst",	0xfaec0000,	0xfffc0000,	FMT_D2, {IMM16, DN0}},
{ "btst",	0xfcec0000,	0xfffc0000,	FMT_D4, {IMM32, DN0}},
{ "btst",	0xfe020000,	0xffff0000,	FMT_D5, {IMM8E,
							 MEM(IMM32_LOWSHIFT8)}},
{ "btst",	0xfaf80000,	0xfffc0000,	FMT_D2,
					{IMM8, MEM2(SD8N_SHIFT8,AN0)}},
{ "bset",	0xf080,		0xfff0,		FMT_D0, {DM1, MEM(AN0)}},
{ "bset",	0xfe000000,	0xffff0000,	FMT_D5, {IMM8E,
							 MEM(IMM32_LOWSHIFT8)}},
{ "bset",	0xfaf00000,	0xfffc0000,	FMT_D2,
					{IMM8, MEM2(SD8N_SHIFT8,AN0)}},
{ "bclr",	0xf090,		0xfff0,		FMT_D0, {DM1, MEM(AN0)}},
{ "bclr",	0xfe010000,	0xffff0000,	FMT_D5, {IMM8E,
							 MEM(IMM32_LOWSHIFT8)}},
{ "bclr",	0xfaf40000,	0xfffc0000,	FMT_D2, {IMM8,
						MEM2(SD8N_SHIFT8,AN0)}},

{ "asr",	0xf2b0,		0xfff0,		FMT_D0, {DM1, DN0}},
{ "asr",	0xf8c800,	0xfffc00,	FMT_D1, {IMM8, DN0}},
{ "lsr",	0xf2a0,		0xfff0,		FMT_D0, {DM1, DN0}},
{ "lsr",	0xf8c400,	0xfffc00,	FMT_D1, {IMM8, DN0}},
{ "asl",	0xf290,		0xfff0,		FMT_D0, {DM1, DN0}},
{ "asl",	0xf8c000,	0xfffc00,	FMT_D1, {IMM8, DN0}},
{ "asl2",	0x54,		0xfc,		FMT_S0, {DN0}},
{ "ror",	0xf284,		0xfffc,		FMT_D0, {DN0}},
{ "rol",	0xf280,		0xfffc,		FMT_D0, {DN0}},

{ "beq",	0xc800,		0xff00,		FMT_S1, {SD8N_PCREL}},
{ "bne",	0xc900,		0xff00,		FMT_S1, {SD8N_PCREL}},
{ "bgt",	0xc100,		0xff00,		FMT_S1, {SD8N_PCREL}},
{ "bge",	0xc200,		0xff00,		FMT_S1, {SD8N_PCREL}},
{ "ble",	0xc300,		0xff00,		FMT_S1, {SD8N_PCREL}},
{ "blt",	0xc000,		0xff00,		FMT_S1, {SD8N_PCREL}},
{ "bhi",	0xc500,		0xff00,		FMT_S1, {SD8N_PCREL}},
{ "bcc",	0xc600,		0xff00,		FMT_S1, {SD8N_PCREL}},
{ "bls",	0xc700,		0xff00,		FMT_S1, {SD8N_PCREL}},
{ "bcs",	0xc400,		0xff00,		FMT_S1, {SD8N_PCREL}},
{ "bvc",	0xf8e800,	0xffff00,	FMT_D1, {SD8N_PCREL}},
{ "bvs",	0xf8e900,	0xffff00,	FMT_D1, {SD8N_PCREL}},
{ "bnc",	0xf8ea00,	0xffff00,	FMT_D1, {SD8N_PCREL}},
{ "bns",	0xf8eb00,	0xffff00,	FMT_D1, {SD8N_PCREL}},
{ "bra",	0xca00,		0xff00,		FMT_S1, {SD8N_PCREL}},

{ "leq",	0xd8,		0xff,		FMT_S0, {UNUSED}},
{ "lne",	0xd9,		0xff,		FMT_S0, {UNUSED}},
{ "lgt",	0xd1,		0xff,		FMT_S0, {UNUSED}},
{ "lge",	0xd2,		0xff,		FMT_S0, {UNUSED}},
{ "lle",	0xd3,		0xff,		FMT_S0, {UNUSED}},
{ "llt",	0xd0,		0xff,		FMT_S0, {UNUSED}},
{ "lhi",	0xd5,		0xff,		FMT_S0, {UNUSED}},
{ "lcc",	0xd6,		0xff,		FMT_S0, {UNUSED}},
{ "lls",	0xd7,		0xff,		FMT_S0, {UNUSED}},
{ "lcs",	0xd4,		0xff,		FMT_S0, {UNUSED}},
{ "lra",	0xda,		0xff,		FMT_S0, {UNUSED}},
{ "setlb",	0xdb,		0xff,	 	FMT_S0, {UNUSED}},

{ "jmp",	0xf0f4,		0xfffc,		FMT_D0, {PAREN,AN0,PAREN}},
{ "jmp",	0xcc0000,	0xff0000,	FMT_S2, {IMM16_PCREL}},
{ "jmp",	0xdc000000,	0xff000000,	FMT_S4, {IMM32_HIGH24}},
{ "call",	0xcd000000,	0xff000000,	FMT_S4, {D16_SHIFT,REGS,IMM8E}},
{ "call",	0xdd000000,	0xff000000,	FMT_S6,
					{IMM32_HIGH24_LOWSHIFT16,REGSE_SHIFT8,IMM8E}},
{ "calls",	0xf0f0,		0xfffc,		FMT_D0, {PAREN,AN0,PAREN}},
{ "calls",	0xfaff0000,	0xffff0000,	FMT_D2, {IMM16_PCREL}},
{ "calls",	0xfcff0000,	0xffff0000,	FMT_D4, {IMM32_PCREL}},

{ "ret",	0xdf0000,	0xff0000,	FMT_S2, {REGS_SHIFT8, IMM8}},
{ "retf",	0xde0000,	0xff0000,	FMT_S2, {REGS_SHIFT8, IMM8}},
{ "rets",	0xf0fc,		0xffff,		FMT_D0, {UNUSED}},
{ "rti",	0xf0fd,		0xffff,		FMT_D0, {UNUSED}},
{ "trap",	0xf0fe,		0xffff,		FMT_D0, {UNUSED}},
{ "rtm",	0xf0ff,		0xffff,		FMT_D0, {UNUSED}},
{ "nop",	0xcb,		0xff,		FMT_S0, {UNUSED}},
/* { "udf", 0, 0, {0}},  */

{ "putx",	0xf500,		0xfff0,		FMT_D0, {DN01}},
{ "getx",	0xf6f0,		0xfff0,		FMT_D0, {DN01}},
{ "mulq",	0xf600,		0xfff0,		FMT_D0, {DM1, DN0}},
{ "mulq",	0xf90000,	0xfffc00,	FMT_D1, {SIMM8, DN0}},
{ "mulq",	0xfb000000,	0xfffc0000,	FMT_D2, {SIMM16, DN0}},
{ "mulq",	0xfd000000,	0xfffc0000,	FMT_D4, {IMM32, DN0}},
{ "mulqu",	0xf610,		0xfff0,		FMT_D0, {DM1, DN0}},
{ "mulqu",	0xf91400,	0xfffc00,	FMT_D1, {SIMM8, DN0}},
{ "mulqu",	0xfb140000,	0xfffc0000,	FMT_D2, {SIMM16, DN0}},
{ "mulqu",	0xfd140000,	0xfffc0000,	FMT_D4, {IMM32, DN0}},
{ "sat16",	0xf640,		0xfff0,		FMT_D0, {DM1, DN0}},
{ "sat24",	0xf650,		0xfff0,		FMT_D0, {DM1, DN0}},
{ "bsch",	0xf670,		0xfff0,		FMT_D0, {DM1, DN0}},

/* Extension.  We need some instruction to trigger "emulated syscalls"
   for our simulator.  */
{ "syscall",    0xf020,         0xffff,         FMT_D0, {UNUSED}},

/* Extension.  When talking to the simulator, gdb requires some instruction
   that will trigger a "breakpoint" (really just an instruction that isn't
   otherwise used by the tools.  This instruction must be the same size
   as the smallest instruction on the target machine.  In the case of the
   mn10x00 the "break" instruction must be one byte.  0xff is available on
   both mn10x00 architectures.  */
{ "break",	0xff,		0xff,		FMT_S0, {UNUSED}},
{ 0, 0, 0, 0, {0}},

} ;

const int mn10300_num_opcodes =
  sizeof (mn10300_opcodes) / sizeof (mn10300_opcodes[0]);



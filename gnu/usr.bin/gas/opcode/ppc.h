/*	$OpenBSD: ppc.h,v 1.1 1998/02/15 18:50:07 niklas Exp $	*/

/* ppc.h -- Header file for PowerPC opcode table
   Copyright 1994, 1995 Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support

This file is part of GDB, GAS, and the GNU binutils.

GDB, GAS, and the GNU binutils are free software; you can redistribute
them and/or modify them under the terms of the GNU General Public
License as published by the Free Software Foundation; either version
1, or (at your option) any later version.

GDB, GAS, and the GNU binutils are distributed in the hope that they
will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef PPC_H
#define PPC_H

/* The opcode table is an array of struct powerpc_opcode.  */

struct powerpc_opcode
{
  /* The opcode name.  */
  const char *name;

  /* The opcode itself.  Those bits which will be filled in with
     operands are zeroes.  */
  unsigned long opcode;

  /* The opcode mask.  This is used by the disassembler.  This is a
     mask containing ones indicating those bits which must match the
     opcode field, and zeroes indicating those bits which need not
     match (and are presumably filled in by operands).  */
  unsigned long mask;

  /* One bit flags for the opcode.  These are used to indicate which
     specific processors support the instructions.  The defined values
     are listed below.  */
  unsigned long flags;

  /* An array of operand codes.  Each code is an index into the
     operand table.  They appear in the order which the operands must
     appear in assembly code, and are terminated by a zero.  */
  unsigned char operands[8];
};

/* The table itself is sorted by major opcode number, and is otherwise
   in the order in which the disassembler should consider
   instructions.  */
extern const struct powerpc_opcode powerpc_opcodes[];
extern const int powerpc_num_opcodes;

/* Values defined for the flags field of a struct powerpc_opcode.  */

/* Opcode is defined for the PowerPC architecture.  */
#define PPC_OPCODE_PPC (01)

/* Opcode is defined for the POWER (RS/6000) architecture.  */
#define PPC_OPCODE_POWER (02)

/* Opcode is defined for the POWER2 (Rios 2) architecture.  */
#define PPC_OPCODE_POWER2 (04)

/* Opcode is only defined on 32 bit architectures.  */
#define PPC_OPCODE_32 (010)

/* Opcode is only defined on 64 bit architectures.  */
#define PPC_OPCODE_64 (020)

/* Opcode is supported by the Motorola PowerPC 601 processor.  The 601
   is assumed to support all PowerPC (PPC_OPCODE_PPC) instructions,
   but it also supports many additional POWER instructions.  */
#define PPC_OPCODE_601 (040)

/* Opcode is supported in both the Power and PowerPC architectures
   (ie, compiler's -mcpu=common or assembler's -mcom).  */
#define PPC_OPCODE_COMMON (0100)

/* Opcode is supported for any Power or PowerPC platform (this is
   for the assembler's -many option, and it eliminates duplicates).  */
#define PPC_OPCODE_ANY (0200)

/* A macro to extract the major opcode from an instruction.  */
#define PPC_OP(i) (((i) >> 26) & 0x3f)

/* The operands table is an array of struct powerpc_operand.  */

struct powerpc_operand
{
  /* The number of bits in the operand.  */
  int bits;

  /* How far the operand is left shifted in the instruction.  */
  int shift;

  /* Insertion function.  This is used by the assembler.  To insert an
     operand value into an instruction, check this field.

     If it is NULL, execute
         i |= (op & ((1 << o->bits) - 1)) << o->shift;
     (i is the instruction which we are filling in, o is a pointer to
     this structure, and op is the opcode value; this assumes twos
     complement arithmetic).

     If this field is not NULL, then simply call it with the
     instruction and the operand value.  It will return the new value
     of the instruction.  If the ERRMSG argument is not NULL, then if
     the operand value is illegal, *ERRMSG will be set to a warning
     string (the operand will be inserted in any case).  If the
     operand value is legal, *ERRMSG will be unchanged (most operands
     can accept any value).  */
  unsigned long (*insert) PARAMS ((unsigned long instruction, long op,
				   const char **errmsg));

  /* Extraction function.  This is used by the disassembler.  To
     extract this operand type from an instruction, check this field.

     If it is NULL, compute
         op = ((i) >> o->shift) & ((1 << o->bits) - 1);
	 if ((o->flags & PPC_OPERAND_SIGNED) != 0
	     && (op & (1 << (o->bits - 1))) != 0)
	   op -= 1 << o->bits;
     (i is the instruction, o is a pointer to this structure, and op
     is the result; this assumes twos complement arithmetic).

     If this field is not NULL, then simply call it with the
     instruction value.  It will return the value of the operand.  If
     the INVALID argument is not NULL, *INVALID will be set to
     non-zero if this operand type can not actually be extracted from
     this operand (i.e., the instruction does not match).  If the
     operand is valid, *INVALID will not be changed.  */
  long (*extract) PARAMS ((unsigned long instruction, int *invalid));

  /* One bit syntax flags.  */
  unsigned long flags;
};

/* Elements in the table are retrieved by indexing with values from
   the operands field of the powerpc_opcodes table.  */

extern const struct powerpc_operand powerpc_operands[];

/* Values defined for the flags field of a struct powerpc_operand.  */

/* This operand takes signed values.  */
#define PPC_OPERAND_SIGNED (01)

/* This operand takes signed values, but also accepts a full positive
   range of values when running in 32 bit mode.  That is, if bits is
   16, it takes any value from -0x8000 to 0xffff.  In 64 bit mode,
   this flag is ignored.  */
#define PPC_OPERAND_SIGNOPT (02)

/* This operand does not actually exist in the assembler input.  This
   is used to support extended mnemonics such as mr, for which two
   operands fields are identical.  The assembler should call the
   insert function with any op value.  The disassembler should call
   the extract function, ignore the return value, and check the value
   placed in the valid argument.  */
#define PPC_OPERAND_FAKE (04)

/* The next operand should be wrapped in parentheses rather than
   separated from this one by a comma.  This is used for the load and
   store instructions which want their operands to look like
       reg,displacement(reg)
   */
#define PPC_OPERAND_PARENS (010)

/* This operand may use the symbolic names for the CR fields, which
   are
       lt  0	gt  1	eq  2	so  3	un  3
       cr0 0	cr1 1	cr2 2	cr3 3
       cr4 4	cr5 5	cr6 6	cr7 7
   These may be combined arithmetically, as in cr2*4+gt.  These are
   only supported on the PowerPC, not the POWER.  */
#define PPC_OPERAND_CR (020)

/* This operand names a register.  The disassembler uses this to print
   register names with a leading 'r'.  */
#define PPC_OPERAND_GPR (040)

/* This operand names a floating point register.  The disassembler
   prints these with a leading 'f'.  */
#define PPC_OPERAND_FPR (0100)

/* This operand is a relative branch displacement.  The disassembler
   prints these symbolically if possible.  */
#define PPC_OPERAND_RELATIVE (0200)

/* This operand is an absolute branch address.  The disassembler
   prints these symbolically if possible.  */
#define PPC_OPERAND_ABSOLUTE (0400)

/* This operand is optional, and is zero if omitted.  This is used for
   the optional BF and L fields in the comparison instructions.  The
   assembler must count the number of operands remaining on the line,
   and the number of operands remaining for the opcode, and decide
   whether this operand is present or not.  The disassembler should
   print this operand out only if it is not zero.  */
#define PPC_OPERAND_OPTIONAL (01000)

/* This flag is only used with PPC_OPERAND_OPTIONAL.  If this operand
   is omitted, then for the next operand use this operand value plus
   1, ignoring the next operand field for the opcode.  This wretched
   hack is needed because the Power rotate instructions can take
   either 4 or 5 operands.  The disassembler should print this operand
   out regardless of the PPC_OPERAND_OPTIONAL field.  */
#define PPC_OPERAND_NEXT (02000)

/* This operand should be regarded as a negative number for the
   purposes of overflow checking (i.e., the normal most negative
   number is disallowed and one more than the normal most positive
   number is allowed).  This flag will only be set for a signed
   operand.  */
#define PPC_OPERAND_NEGATIVE (04000)

/* The POWER and PowerPC assemblers use a few macros.  We keep them
   with the operands table for simplicity.  The macro table is an
   array of struct powerpc_macro.  */

struct powerpc_macro
{
  /* The macro name.  */
  const char *name;

  /* The number of operands the macro takes.  */
  unsigned int operands;

  /* One bit flags for the opcode.  These are used to indicate which
     specific processors support the instructions.  The values are the
     same as those for the struct powerpc_opcode flags field.  */
  unsigned long flags;

  /* A format string to turn the macro into a normal instruction.
     Each %N in the string is replaced with operand number N (zero
     based).  */
  const char *format;
};

extern const struct powerpc_macro powerpc_macros[];
extern const int powerpc_num_macros;

/* ppc-opc.c -- PowerPC opcode list
   Copyright 1994 Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Cygnus Support

This file is part of GDB, GAS, and the GNU binutils.

GDB, GAS, and the GNU binutils are free software; you can redistribute
them and/or modify them under the terms of the GNU General Public
License as published by the Free Software Foundation; either version
2, or (at your option) any later version.

GDB, GAS, and the GNU binutils are distributed in the hope that they
will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this file; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/*
#include <stdio.h>
#include "ansidecl.h"
#include "opcode/ppc.h"
*/
/* This file holds the PowerPC opcode table.  The opcode table
   includes almost all of the extended instruction mnemonics.  This
   permits the disassembler to use them, and simplifies the assembler
   logic, at the cost of increasing the table size.  The table is
   strictly constant data, so the compiler should be able to put it in
   the .text section.

   This file also holds the operand table.  All knowledge about
   inserting operands into instructions and vice-versa is kept in this
   file.  */

/* Local insertion and extraction functions.  */

static unsigned long insert_bat PARAMS ((unsigned long, long, const char **));
static long extract_bat PARAMS ((unsigned long, int *));
static unsigned long insert_bba PARAMS ((unsigned long, long, const char **));
static long extract_bba PARAMS ((unsigned long, int *));
static unsigned long insert_bd PARAMS ((unsigned long, long, const char **));
static long extract_bd PARAMS ((unsigned long, int *));
static unsigned long insert_bdm PARAMS ((unsigned long, long, const char **));
static long extract_bdm PARAMS ((unsigned long, int *));
static unsigned long insert_bdp PARAMS ((unsigned long, long, const char **));
static long extract_bdp PARAMS ((unsigned long, int *));
static unsigned long insert_bo PARAMS ((unsigned long, long, const char **));
static long extract_bo PARAMS ((unsigned long, int *));
static unsigned long insert_boe PARAMS ((unsigned long, long, const char **));
static long extract_boe PARAMS ((unsigned long, int *));
static unsigned long insert_ds PARAMS ((unsigned long, long, const char **));
static long extract_ds PARAMS ((unsigned long, int *));
static unsigned long insert_li PARAMS ((unsigned long, long, const char **));
static long extract_li PARAMS ((unsigned long, int *));
static unsigned long insert_mbe PARAMS ((unsigned long, long, const char **));
static long extract_mbe PARAMS ((unsigned long, int *));
static unsigned long insert_mb6 PARAMS ((unsigned long, long, const char **));
static long extract_mb6 PARAMS ((unsigned long, int *));
static unsigned long insert_nb PARAMS ((unsigned long, long, const char **));
static long extract_nb PARAMS ((unsigned long, int *));
static unsigned long insert_nsi PARAMS ((unsigned long, long, const char **));
static long extract_nsi PARAMS ((unsigned long, int *));
static unsigned long insert_ral PARAMS ((unsigned long, long, const char **));
static unsigned long insert_ram PARAMS ((unsigned long, long, const char **));
static unsigned long insert_ras PARAMS ((unsigned long, long, const char **));
static unsigned long insert_rbs PARAMS ((unsigned long, long, const char **));
static long extract_rbs PARAMS ((unsigned long, int *));
static unsigned long insert_sh6 PARAMS ((unsigned long, long, const char **));
static long extract_sh6 PARAMS ((unsigned long, int *));
static unsigned long insert_spr PARAMS ((unsigned long, long, const char **));
static long extract_spr PARAMS ((unsigned long, int *));
static unsigned long insert_tbr PARAMS ((unsigned long, long, const char **));
static long extract_tbr PARAMS ((unsigned long, int *));

/* The operands table.

   The fields are bits, shift, signed, insert, extract, flags.  */

const struct powerpc_operand powerpc_operands[] =
{
  /* The zero index is used to indicate the end of the list of
     operands.  */
#define UNUSED (0)
  { 0, 0, 0, 0, 0 },

  /* The BA field in an XL form instruction.  */
#define BA (UNUSED + 1)
#define BA_MASK (0x1f << 16)
  { 5, 16, 0, 0, PPC_OPERAND_CR },

  /* The BA field in an XL form instruction when it must be the same
     as the BT field in the same instruction.  */
#define BAT (BA + 1)
  { 5, 16, insert_bat, extract_bat, PPC_OPERAND_FAKE },

  /* The BB field in an XL form instruction.  */
#define BB (BAT + 1)
#define BB_MASK (0x1f << 11)
  { 5, 11, 0, 0, PPC_OPERAND_CR },

  /* The BB field in an XL form instruction when it must be the same
     as the BA field in the same instruction.  */
#define BBA (BB + 1)
  { 5, 11, insert_bba, extract_bba, PPC_OPERAND_FAKE },

  /* The BD field in a B form instruction.  The lower two bits are
     forced to zero.  */
#define BD (BBA + 1)
  { 16, 0, insert_bd, extract_bd, PPC_OPERAND_RELATIVE | PPC_OPERAND_SIGNED },

  /* The BD field in a B form instruction when absolute addressing is
     used.  */
#define BDA (BD + 1)
  { 16, 0, insert_bd, extract_bd, PPC_OPERAND_ABSOLUTE | PPC_OPERAND_SIGNED },

  /* The BD field in a B form instruction when the - modifier is used.
     This sets the y bit of the BO field appropriately.  */
#define BDM (BDA + 1)
  { 16, 0, insert_bdm, extract_bdm,
      PPC_OPERAND_RELATIVE | PPC_OPERAND_SIGNED },

  /* The BD field in a B form instruction when the - modifier is used
     and absolute address is used.  */
#define BDMA (BDM + 1)
  { 16, 0, insert_bdm, extract_bdm,
      PPC_OPERAND_ABSOLUTE | PPC_OPERAND_SIGNED },

  /* The BD field in a B form instruction when the + modifier is used.
     This sets the y bit of the BO field appropriately.  */
#define BDP (BDMA + 1)
  { 16, 0, insert_bdp, extract_bdp,
      PPC_OPERAND_RELATIVE | PPC_OPERAND_SIGNED },

  /* The BD field in a B form instruction when the + modifier is used
     and absolute addressing is used.  */
#define BDPA (BDP + 1)
  { 16, 0, insert_bdp, extract_bdp,
      PPC_OPERAND_ABSOLUTE | PPC_OPERAND_SIGNED },

  /* The BF field in an X or XL form instruction.  */
#define BF (BDPA + 1)
  { 3, 23, 0, 0, PPC_OPERAND_CR },

  /* An optional BF field.  This is used for comparison instructions,
     in which an omitted BF field is taken as zero.  */
#define OBF (BF + 1)
  { 3, 23, 0, 0, PPC_OPERAND_CR | PPC_OPERAND_OPTIONAL },

  /* The BFA field in an X or XL form instruction.  */
#define BFA (OBF + 1)
  { 3, 18, 0, 0, PPC_OPERAND_CR },

  /* The BI field in a B form or XL form instruction.  */
#define BI (BFA + 1)
#define BI_MASK (0x1f << 16)
  { 5, 16, 0, 0, PPC_OPERAND_CR },

  /* The BO field in a B form instruction.  Certain values are
     illegal.  */
#define BO (BI + 1)
#define BO_MASK (0x1f << 21)
  { 5, 21, insert_bo, extract_bo, 0 },

  /* The BO field in a B form instruction when the + or - modifier is
     used.  This is like the BO field, but it must be even.  */
#define BOE (BO + 1)
  { 5, 21, insert_boe, extract_boe, 0 },

  /* The BT field in an X or XL form instruction.  */
#define BT (BOE + 1)
  { 5, 21, 0, 0, PPC_OPERAND_CR },

  /* The condition register number portion of the BI field in a B form
     or XL form instruction.  This is used for the extended
     conditional branch mnemonics, which set the lower two bits of the
     BI field.  This field is optional.  */
#define CR (BT + 1)
  { 3, 18, 0, 0, PPC_OPERAND_CR | PPC_OPERAND_OPTIONAL },

  /* The D field in a D form instruction.  This is a displacement off
     a register, and implies that the next operand is a register in
     parentheses.  */
#define D (CR + 1)
  { 16, 0, 0, 0, PPC_OPERAND_PARENS | PPC_OPERAND_SIGNED },

  /* The DS field in a DS form instruction.  This is like D, but the
     lower two bits are forced to zero.  */
#define DS (D + 1)
  { 16, 0, insert_ds, extract_ds, PPC_OPERAND_PARENS | PPC_OPERAND_SIGNED },

  /* The FL1 field in a POWER SC form instruction.  */
#define FL1 (DS + 1)
  { 4, 12, 0, 0, 0 },

  /* The FL2 field in a POWER SC form instruction.  */
#define FL2 (FL1 + 1)
  { 3, 2, 0, 0, 0 },

  /* The FLM field in an XFL form instruction.  */
#define FLM (FL2 + 1)
  { 8, 17, 0, 0, 0 },

  /* The FRA field in an X or A form instruction.  */
#define FRA (FLM + 1)
#define FRA_MASK (0x1f << 16)
  { 5, 16, 0, 0, PPC_OPERAND_FPR },

  /* The FRB field in an X or A form instruction.  */
#define FRB (FRA + 1)
#define FRB_MASK (0x1f << 11)
  { 5, 11, 0, 0, PPC_OPERAND_FPR },

  /* The FRC field in an A form instruction.  */
#define FRC (FRB + 1)
#define FRC_MASK (0x1f << 6)
  { 5, 6, 0, 0, PPC_OPERAND_FPR },

  /* The FRS field in an X form instruction or the FRT field in a D, X
     or A form instruction.  */
#define FRS (FRC + 1)
#define FRT (FRS)
  { 5, 21, 0, 0, PPC_OPERAND_FPR },

  /* The FXM field in an XFX instruction.  */
#define FXM (FRS + 1)
#define FXM_MASK (0xff << 12)
  { 8, 12, 0, 0, 0 },

  /* The L field in a D or X form instruction.  */
#define L (FXM + 1)
  { 1, 21, 0, 0, PPC_OPERAND_OPTIONAL },

  /* The LEV field in a POWER SC form instruction.  */
#define LEV (L + 1)
  { 7, 5, 0, 0, 0 },

  /* The LI field in an I form instruction.  The lower two bits are
     forced to zero.  */
#define LI (LEV + 1)
  { 26, 0, insert_li, extract_li, PPC_OPERAND_RELATIVE | PPC_OPERAND_SIGNED },

  /* The LI field in an I form instruction when used as an absolute
     address.  */
#define LIA (LI + 1)
  { 26, 0, insert_li, extract_li, PPC_OPERAND_ABSOLUTE | PPC_OPERAND_SIGNED },

  /* The MB field in an M form instruction.  */
#define MB (LIA + 1)
#define MB_MASK (0x1f << 6)
  { 5, 6, 0, 0, 0 },

  /* The ME field in an M form instruction.  */
#define ME (MB + 1)
#define ME_MASK (0x1f << 1)
  { 5, 1, 0, 0, 0 },

  /* The MB and ME fields in an M form instruction expressed a single
     operand which is a bitmask indicating which bits to select.  This
     is a two operand form using PPC_OPERAND_NEXT.  See the
     description in opcode/ppc.h for what this means.  */
#define MBE (ME + 1)
  { 5, 6, 0, 0, PPC_OPERAND_OPTIONAL | PPC_OPERAND_NEXT },
  { 32, 0, insert_mbe, extract_mbe, 0 },

  /* The MB or ME field in an MD or MDS form instruction.  The high
     bit is wrapped to the low end.  */
#define MB6 (MBE + 2)
#define ME6 (MB6)
#define MB6_MASK (0x3f << 5)
  { 6, 5, insert_mb6, extract_mb6, 0 },

  /* The NB field in an X form instruction.  The value 32 is stored as
     0.  */
#define NB (MB6 + 1)
  { 6, 11, insert_nb, extract_nb, 0 },

  /* The NSI field in a D form instruction.  This is the same as the
     SI field, only negated.  */
#define NSI (NB + 1)
  { 16, 0, insert_nsi, extract_nsi,
      PPC_OPERAND_NEGATIVE | PPC_OPERAND_SIGNED },

  /* The RA field in an D, DS, X, XO, M, or MDS form instruction.  */
#define RA (NSI + 1)
#define RA_MASK (0x1f << 16)
  { 5, 16, 0, 0, PPC_OPERAND_GPR },

  /* The RA field in a D or X form instruction which is an updating
     load, which means that the RA field may not be zero and may not
     equal the RT field.  */
#define RAL (RA + 1)
  { 5, 16, insert_ral, 0, PPC_OPERAND_GPR },

  /* The RA field in an lmw instruction, which has special value
     restrictions.  */
#define RAM (RAL + 1)
  { 5, 16, insert_ram, 0, PPC_OPERAND_GPR },

  /* The RA field in a D or X form instruction which is an updating
     store or an updating floating point load, which means that the RA
     field may not be zero.  */
#define RAS (RAM + 1)
  { 5, 16, insert_ras, 0, PPC_OPERAND_GPR },

  /* The RB field in an X, XO, M, or MDS form instruction.  */
#define RB (RAS + 1)
#define RB_MASK (0x1f << 11)
  { 5, 11, 0, 0, PPC_OPERAND_GPR },

  /* The RB field in an X form instruction when it must be the same as
     the RS field in the instruction.  This is used for extended
     mnemonics like mr.  */
#define RBS (RB + 1)
  { 5, 1, insert_rbs, extract_rbs, PPC_OPERAND_FAKE },

  /* The RS field in a D, DS, X, XFX, XS, M, MD or MDS form
     instruction or the RT field in a D, DS, X, XFX or XO form
     instruction.  */
#define RS (RBS + 1)
#define RT (RS)
#define RT_MASK (0x1f << 21)
  { 5, 21, 0, 0, PPC_OPERAND_GPR },

  /* The SH field in an X or M form instruction.  */
#define SH (RS + 1)
#define SH_MASK (0x1f << 11)
  { 5, 11, 0, 0, 0 },

  /* The SH field in an MD form instruction.  This is split.  */
#define SH6 (SH + 1)
#define SH6_MASK ((0x1f << 11) | (1 << 1))
  { 6, 1, insert_sh6, extract_sh6, 0 },

  /* The SI field in a D form instruction.  */
#define SI (SH6 + 1)
  { 16, 0, 0, 0, PPC_OPERAND_SIGNED },

  /* The SI field in a D form instruction when we accept a wide range
     of positive values.  */
#define SISIGNOPT (SI + 1)
  { 16, 0, 0, 0, PPC_OPERAND_SIGNED | PPC_OPERAND_SIGNOPT },

  /* The SPR field in an XFX form instruction.  This is flipped--the
     lower 5 bits are stored in the upper 5 and vice- versa.  */
#define SPR (SISIGNOPT + 1)
#define SPR_MASK (0x3ff << 11)
  { 10, 11, insert_spr, extract_spr, 0 },

  /* The BAT index number in an XFX form m[ft]ibat[lu] instruction.  */
#define SPRBAT (SPR + 1)
#define SPRBAT_MASK (0x3 << 17)
  { 2, 17, 0, 0, 0 },

  /* The SPRG register number in an XFX form m[ft]sprg instruction.  */
#define SPRG (SPRBAT + 1)
#define SPRG_MASK (0x3 << 16)
  { 2, 16, 0, 0, 0 },

  /* The SR field in an X form instruction.  */
#define SR (SPRG + 1)
  { 4, 16, 0, 0, 0 },

  /* The SV field in a POWER SC form instruction.  */
#define SV (SR + 1)
  { 14, 2, 0, 0, 0 },

  /* The TBR field in an XFX form instruction.  This is like the SPR
     field, but it is optional.  */
#define TBR (SV + 1)
  { 10, 11, insert_tbr, extract_tbr, PPC_OPERAND_OPTIONAL },

  /* The TO field in a D or X form instruction.  */
#define TO (TBR + 1)
#define TO_MASK (0x1f << 21)
  { 5, 21, 0, 0, 0 },

  /* The U field in an X form instruction.  */
#define U (TO + 1)
  { 4, 12, 0, 0, 0 },

  /* The UI field in a D form instruction.  */
#define UI (U + 1)
  { 16, 0, 0, 0, 0 },
};

/* The functions used to insert and extract complicated operands.  */

/* The BA field in an XL form instruction when it must be the same as
   the BT field in the same instruction.  This operand is marked FAKE.
   The insertion function just copies the BT field into the BA field,
   and the extraction function just checks that the fields are the
   same.  */

/*ARGSUSED*/
static unsigned long 
insert_bat (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  return insn | (((insn >> 21) & 0x1f) << 16);
}

static long
extract_bat (insn, invalid)
     unsigned long insn;
     int *invalid;
{
  if (invalid != (int *) NULL
      && ((insn >> 21) & 0x1f) != ((insn >> 16) & 0x1f))
    *invalid = 1;
  return 0;
}

/* The BB field in an XL form instruction when it must be the same as
   the BA field in the same instruction.  This operand is marked FAKE.
   The insertion function just copies the BA field into the BB field,
   and the extraction function just checks that the fields are the
   same.  */

/*ARGSUSED*/
static unsigned long
insert_bba (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  return insn | (((insn >> 16) & 0x1f) << 11);
}

static long
extract_bba (insn, invalid)
     unsigned long insn;
     int *invalid;
{
  if (invalid != (int *) NULL
      && ((insn >> 16) & 0x1f) != ((insn >> 11) & 0x1f))
    *invalid = 1;
  return 0;
}

/* The BD field in a B form instruction.  The lower two bits are
   forced to zero.  */

/*ARGSUSED*/
static unsigned long
insert_bd (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  return insn | (value & 0xfffc);
}

/*ARGSUSED*/
static long
extract_bd (insn, invalid)
     unsigned long insn;
     int *invalid;
{
  if ((insn & 0x8000) != 0)
    return (insn & 0xfffc) - 0x10000;
  else
    return insn & 0xfffc;
}

/* The BD field in a B form instruction when the - modifier is used.
   This modifier means that the branch is not expected to be taken.
   We must set the y bit of the BO field to 1 if the offset is
   negative.  When extracting, we require that the y bit be 1 and that
   the offset be positive, since if the y bit is 0 we just want to
   print the normal form of the instruction.  */

/*ARGSUSED*/
static unsigned long
insert_bdm (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  if ((value & 0x8000) != 0)
    insn |= 1 << 21;
  return insn | (value & 0xfffc);
}

static long
extract_bdm (insn, invalid)
     unsigned long insn;
     int *invalid;
{
  if (invalid != (int *) NULL
      && ((insn & (1 << 21)) == 0
	  || (insn & (1 << 15)) == 0))
    *invalid = 1;
  if ((insn & 0x8000) != 0)
    return (insn & 0xfffc) - 0x10000;
  else
    return insn & 0xfffc;
}

/* The BD field in a B form instruction when the + modifier is used.
   This is like BDM, above, except that the branch is expected to be
   taken.  */

/*ARGSUSED*/
static unsigned long
insert_bdp (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  if ((value & 0x8000) == 0)
    insn |= 1 << 21;
  return insn | (value & 0xfffc);
}

static long
extract_bdp (insn, invalid)
     unsigned long insn;
     int *invalid;
{
  if (invalid != (int *) NULL
      && ((insn & (1 << 21)) == 0
	  || (insn & (1 << 15)) != 0))
    *invalid = 1;
  if ((insn & 0x8000) != 0)
    return (insn & 0xfffc) - 0x10000;
  else
    return insn & 0xfffc;
}

/* Check for legal values of a BO field.  */

static int
valid_bo (value)
     long value;
{
  /* Certain encodings have bits that are required to be zero.  These
     are (z must be zero, y may be anything):
         001zy
	 011zy
	 1z00y
	 1z01y
	 1z1zz
     */
  switch (value & 0x14)
    {
    default:
    case 0:
      return 1;
    case 0x4:
      return (value & 0x2) == 0;
    case 0x10:
      return (value & 0x8) == 0;
    case 0x14:
      return value == 0x14;
    }
}

/* The BO field in a B form instruction.  Warn about attempts to set
   the field to an illegal value.  */

static unsigned long
insert_bo (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  if (errmsg != (const char **) NULL
      && ! valid_bo (value))
    *errmsg = "invalid conditional option";
  return insn | ((value & 0x1f) << 21);
}

static long
extract_bo (insn, invalid)
     unsigned long insn;
     int *invalid;
{
  long value;

  value = (insn >> 21) & 0x1f;
  if (invalid != (int *) NULL
      && ! valid_bo (value))
    *invalid = 1;
  return value;
}

/* The BO field in a B form instruction when the + or - modifier is
   used.  This is like the BO field, but it must be even.  When
   extracting it, we force it to be even.  */

static unsigned long
insert_boe (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  if (errmsg != (const char **) NULL)
    {
      if (! valid_bo (value))
	*errmsg = "invalid conditional option";
      else if ((value & 1) != 0)
	*errmsg = "attempt to set y bit when using + or - modifier";
    }
  return insn | ((value & 0x1f) << 21);
}

static long
extract_boe (insn, invalid)
     unsigned long insn;
     int *invalid;
{
  long value;

  value = (insn >> 21) & 0x1f;
  if (invalid != (int *) NULL
      && ! valid_bo (value))
    *invalid = 1;
  return value & 0x1e;
}

/* The DS field in a DS form instruction.  This is like D, but the
   lower two bits are forced to zero.  */

/*ARGSUSED*/
static unsigned long
insert_ds (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  return insn | (value & 0xfffc);
}

/*ARGSUSED*/
static long
extract_ds (insn, invalid)
     unsigned long insn;
     int *invalid;
{
  if ((insn & 0x8000) != 0)
    return (insn & 0xfffc) - 0x10000;
  else
    return insn & 0xfffc;
}

/* The LI field in an I form instruction.  The lower two bits are
   forced to zero.  */

/*ARGSUSED*/
static unsigned long
insert_li (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  return insn | (value & 0x3fffffc);
}

/*ARGSUSED*/
static long
extract_li (insn, invalid)
     unsigned long insn;
     int *invalid;
{
  if ((insn & 0x2000000) != 0)
    return (insn & 0x3fffffc) - 0x4000000;
  else
    return insn & 0x3fffffc;
}

/* The MB and ME fields in an M form instruction expressed as a single
   operand which is itself a bitmask.  The extraction function always
   marks it as invalid, since we never want to recognize an
   instruction which uses a field of this type.  */

static unsigned long
insert_mbe (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  unsigned long uval;
  int mb, me;

  uval = value;

  if (uval == 0)
    {
      if (errmsg != (const char **) NULL)
	*errmsg = "illegal bitmask";
      return insn;
    }

  me = 31;
  while ((uval & 1) == 0)
    {
      uval >>= 1;
      --me;
    }

  mb = me;
  uval >>= 1;
  while ((uval & 1) != 0)
    {
      uval >>= 1;
      --mb;
    }

  if (uval != 0)
    {
      if (errmsg != (const char **) NULL)
	*errmsg = "illegal bitmask";
    }

  return insn | (mb << 6) | (me << 1);
}

static long
extract_mbe (insn, invalid)
     unsigned long insn;
     int *invalid;
{
  long ret;
  int mb, me;
  int i;

  if (invalid != (int *) NULL)
    *invalid = 1;

  ret = 0;
  mb = (insn >> 6) & 0x1f;
  me = (insn >> 1) & 0x1f;
  for (i = mb; i < me; i++)
    ret |= 1 << (31 - i);
  return ret;
}

/* The MB or ME field in an MD or MDS form instruction.  The high bit
   is wrapped to the low end.  */

/*ARGSUSED*/
static unsigned long
insert_mb6 (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  return insn | ((value & 0x1f) << 6) | (value & 0x20);
}

/*ARGSUSED*/
static long
extract_mb6 (insn, invalid)
     unsigned long insn;
     int *invalid;
{
  return ((insn >> 6) & 0x1f) | (insn & 0x20);
}

/* The NB field in an X form instruction.  The value 32 is stored as
   0.  */

static unsigned long
insert_nb (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  if (value < 0 || value > 32)
    *errmsg = "value out of range";
  if (value == 32)
    value = 0;
  return insn | ((value & 0x1f) << 11);
}

/*ARGSUSED*/
static long
extract_nb (insn, invalid)
     unsigned long insn;
     int *invalid;
{
  long ret;

  ret = (insn >> 11) & 0x1f;
  if (ret == 0)
    ret = 32;
  return ret;
}

/* The NSI field in a D form instruction.  This is the same as the SI
   field, only negated.  The extraction function always marks it as
   invalid, since we never want to recognize an instruction which uses
   a field of this type.  */

/*ARGSUSED*/
static unsigned long
insert_nsi (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  return insn | ((- value) & 0xffff);
}

static long
extract_nsi (insn, invalid)
     unsigned long insn;
     int *invalid;
{
  if (invalid != (int *) NULL)
    *invalid = 1;
  if ((insn & 0x8000) != 0)
    return - ((insn & 0xffff) - 0x10000);
  else
    return - (insn & 0xffff);
}

/* The RA field in a D or X form instruction which is an updating
   load, which means that the RA field may not be zero and may not
   equal the RT field.  */

static unsigned long
insert_ral (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  if (value == 0
      || value == ((insn >> 21) & 0x1f))
    *errmsg = "invalid register operand when updating";
  return insn | ((value & 0x1f) << 16);
}

/* The RA field in an lmw instruction, which has special value
   restrictions.  */

static unsigned long
insert_ram (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  if (value >= ((insn >> 21) & 0x1f))
    *errmsg = "index register in load range";
  return insn | ((value & 0x1f) << 16);
}

/* The RA field in a D or X form instruction which is an updating
   store or an updating floating point load, which means that the RA
   field may not be zero.  */

static unsigned long
insert_ras (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  if (value == 0)
    *errmsg = "invalid register operand when updating";
  return insn | ((value & 0x1f) << 16);
}

/* The RB field in an X form instruction when it must be the same as
   the RS field in the instruction.  This is used for extended
   mnemonics like mr.  This operand is marked FAKE.  The insertion
   function just copies the BT field into the BA field, and the
   extraction function just checks that the fields are the same.  */

/*ARGSUSED*/
static unsigned long 
insert_rbs (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  return insn | (((insn >> 21) & 0x1f) << 11);
}

static long
extract_rbs (insn, invalid)
     unsigned long insn;
     int *invalid;
{
  if (invalid != (int *) NULL
      && ((insn >> 21) & 0x1f) != ((insn >> 11) & 0x1f))
    *invalid = 1;
  return 0;
}

/* The SH field in an MD form instruction.  This is split.  */

/*ARGSUSED*/
static unsigned long
insert_sh6 (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  return insn | ((value & 0x1f) << 11) | ((value & 0x20) >> 4);
}

/*ARGSUSED*/
static long
extract_sh6 (insn, invalid)
     unsigned long insn;
     int *invalid;
{
  return ((insn >> 11) & 0x1f) | ((insn << 4) & 0x20);
}

/* The SPR field in an XFX form instruction.  This is flipped--the
   lower 5 bits are stored in the upper 5 and vice- versa.  */

static unsigned long
insert_spr (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  return insn | ((value & 0x1f) << 16) | ((value & 0x3e0) << 6);
}

static long
extract_spr (insn, invalid)
     unsigned long insn;
     int *invalid;
{
  return ((insn >> 16) & 0x1f) | ((insn >> 6) & 0x3e0);
}

/* The TBR field in an XFX instruction.  This is just like SPR, but it
   is optional.  When TBR is omitted, it must be inserted as 268 (the
   magic number of the TB register).  These functions treat 0
   (indicating an omitted optional operand) as 268.  This means that
   ``mftb 4,0'' is not handled correctly.  This does not matter very
   much, since the architecture manual does not define mftb as
   accepting any values other than 268 or 269.  */

#define TB (268)

static unsigned long
insert_tbr (insn, value, errmsg)
     unsigned long insn;
     long value;
     const char **errmsg;
{
  if (value == 0)
    value = TB;
  return insn | ((value & 0x1f) << 16) | ((value & 0x3e0) << 6);
}

static long
extract_tbr (insn, invalid)
     unsigned long insn;
     int *invalid;
{
  long ret;

  ret = ((insn >> 16) & 0x1f) | ((insn >> 6) & 0x3e0);
  if (ret == TB)
    ret = 0;
  return ret;
}

/* Macros used to form opcodes.  */

/* The main opcode.  */
#define OP(x) (((x) & 0x3f) << 26)
#define OP_MASK OP (0x3f)

/* The main opcode combined with a trap code in the TO field of a D
   form instruction.  Used for extended mnemonics for the trap
   instructions.  */
#define OPTO(x,to) (OP (x) | (((to) & 0x1f) << 21))
#define OPTO_MASK (OP_MASK | TO_MASK)

/* The main opcode combined with a comparison size bit in the L field
   of a D form or X form instruction.  Used for extended mnemonics for
   the comparison instructions.  */
#define OPL(x,l) (OP (x) | (((l) & 1) << 21))
#define OPL_MASK OPL (0x3f,1)

/* An A form instruction.  */
#define A(op, xop, rc) (OP (op) | (((xop) & 0x1f) << 1) | ((rc) & 1))
#define A_MASK A (0x3f, 0x1f, 1)

/* An A_MASK with the FRB field fixed.  */
#define AFRB_MASK (A_MASK | FRB_MASK)

/* An A_MASK with the FRC field fixed.  */
#define AFRC_MASK (A_MASK | FRC_MASK)

/* An A_MASK with the FRA and FRC fields fixed.  */
#define AFRAFRC_MASK (A_MASK | FRA_MASK | FRC_MASK)

/* A B form instruction.  */
#define B(op, aa, lk) (OP (op) | (((aa) & 1) << 1) | ((lk) & 1))
#define B_MASK B (0x3f, 1, 1)

/* A B form instruction setting the BO field.  */
#define BBO(op, bo, aa, lk) (B ((op), (aa), (lk)) | (((bo) & 0x1f) << 21))
#define BBO_MASK BBO (0x3f, 0x1f, 1, 1)

/* A BBO_MASK with the y bit of the BO field removed.  This permits
   matching a conditional branch regardless of the setting of the y
   bit.  */
#define Y_MASK (1 << 21)
#define BBOY_MASK (BBO_MASK &~ Y_MASK)

/* A B form instruction setting the BO field and the condition bits of
   the BI field.  */
#define BBOCB(op, bo, cb, aa, lk) \
  (BBO ((op), (bo), (aa), (lk)) | (((cb) & 0x3) << 16))
#define BBOCB_MASK BBOCB (0x3f, 0x1f, 0x3, 1, 1)

/* A BBOCB_MASK with the y bit of the BO field removed.  */
#define BBOYCB_MASK (BBOCB_MASK &~ Y_MASK)

/* A BBOYCB_MASK in which the BI field is fixed.  */
#define BBOYBI_MASK (BBOYCB_MASK | BI_MASK)

/* The main opcode mask with the RA field clear.  */
#define DRA_MASK (OP_MASK | RA_MASK)

/* A DS form instruction.  */
#define DSO(op, xop) (OP (op) | ((xop) & 0x3))
#define DS_MASK DSO (0x3f, 3)

/* An M form instruction.  */
#define M(op, rc) (OP (op) | ((rc) & 1))
#define M_MASK M (0x3f, 1)

/* An M form instruction with the ME field specified.  */
#define MME(op, me, rc) (M ((op), (rc)) | (((me) & 0x1f) << 1))

/* An M_MASK with the MB and ME fields fixed.  */
#define MMBME_MASK (M_MASK | MB_MASK | ME_MASK)

/* An M_MASK with the SH and ME fields fixed.  */
#define MSHME_MASK (M_MASK | SH_MASK | ME_MASK)

/* An MD form instruction.  */
#define MD(op, xop, rc) (OP (op) | (((xop) & 0x7) << 2) | ((rc) & 1))
#define MD_MASK MD (0x3f, 0x7, 1)

/* An MD_MASK with the MB field fixed.  */
#define MDMB_MASK (MD_MASK | MB6_MASK)

/* An MD_MASK with the SH field fixed.  */
#define MDSH_MASK (MD_MASK | SH6_MASK)

/* An MDS form instruction.  */
#define MDS(op, xop, rc) (OP (op) | (((xop) & 0xf) << 1) | ((rc) & 1))
#define MDS_MASK MDS (0x3f, 0xf, 1)

/* An MDS_MASK with the MB field fixed.  */
#define MDSMB_MASK (MDS_MASK | MB6_MASK)

/* An SC form instruction.  */
#define SC(op, sa, lk) (OP (op) | (((sa) & 1) << 1) | ((lk) & 1))
#define SC_MASK (OP_MASK | (0x3ff << 16) | (1 << 1) | 1)

/* An X form instruction.  */
#define X(op, xop) (OP (op) | (((xop) & 0x3ff) << 1))

/* An X form instruction with the RC bit specified.  */
#define XRC(op, xop, rc) (X ((op), (xop)) | ((rc) & 1))

/* The mask for an X form instruction.  */
#define X_MASK XRC (0x3f, 0x3ff, 1)

/* An X_MASK with the RA field fixed.  */
#define XRA_MASK (X_MASK | RA_MASK)

/* An X_MASK with the RB field fixed.  */
#define XRB_MASK (X_MASK | RB_MASK)

/* An X_MASK with the RT field fixed.  */
#define XRT_MASK (X_MASK | RT_MASK)

/* An X_MASK with the RA and RB fields fixed.  */
#define XRARB_MASK (X_MASK | RA_MASK | RB_MASK)

/* An X_MASK with the RT and RA fields fixed.  */
#define XRTRA_MASK (X_MASK | RT_MASK | RA_MASK)

/* An X form comparison instruction.  */
#define XCMPL(op, xop, l) (X ((op), (xop)) | (((l) & 1) << 21))

/* The mask for an X form comparison instruction.  */
#define XCMP_MASK (X_MASK | (1 << 22))

/* The mask for an X form comparison instruction with the L field
   fixed.  */
#define XCMPL_MASK (XCMP_MASK | (1 << 21))

/* An X form trap instruction with the TO field specified.  */
#define XTO(op, xop, to) (X ((op), (xop)) | (((to) & 0x1f) << 21))
#define XTO_MASK (X_MASK | TO_MASK)

/* An XFL form instruction.  */
#define XFL(op, xop, rc) (OP (op) | (((xop) & 0x3ff) << 1) | ((rc) & 1))
#define XFL_MASK (XFL (0x3f, 0x3ff, 1) | (1 << 25) | (1 << 16))

/* An XL form instruction with the LK field set to 0.  */
#define XL(op, xop) (OP (op) | (((xop) & 0x3ff) << 1))

/* An XL form instruction which uses the LK field.  */
#define XLLK(op, xop, lk) (XL ((op), (xop)) | ((lk) & 1))

/* The mask for an XL form instruction.  */
#define XL_MASK XLLK (0x3f, 0x3ff, 1)

/* An XL form instruction which explicitly sets the BO field.  */
#define XLO(op, bo, xop, lk) \
  (XLLK ((op), (xop), (lk)) | (((bo) & 0x1f) << 21))
#define XLO_MASK (XL_MASK | BO_MASK)

/* An XL form instruction which explicitly sets the y bit of the BO
   field.  */
#define XLYLK(op, xop, y, lk) (XLLK ((op), (xop), (lk)) | (((y) & 1) << 21))
#define XLYLK_MASK (XL_MASK | Y_MASK)

/* An XL form instruction which sets the BO field and the condition
   bits of the BI field.  */
#define XLOCB(op, bo, cb, xop, lk) \
  (XLO ((op), (bo), (xop), (lk)) | (((cb) & 3) << 16))
#define XLOCB_MASK XLOCB (0x3f, 0x1f, 0x3, 0x3ff, 1)

/* An XL_MASK or XLYLK_MASK or XLOCB_MASK with the BB field fixed.  */
#define XLBB_MASK (XL_MASK | BB_MASK)
#define XLYBB_MASK (XLYLK_MASK | BB_MASK)
#define XLBOCBBB_MASK (XLOCB_MASK | BB_MASK)

/* An XL_MASK with the BO and BB fields fixed.  */
#define XLBOBB_MASK (XL_MASK | BO_MASK | BB_MASK)

/* An XL_MASK with the BO, BI and BB fields fixed.  */
#define XLBOBIBB_MASK (XL_MASK | BO_MASK | BI_MASK | BB_MASK)

/* An XO form instruction.  */
#define XO(op, xop, oe, rc) \
  (OP (op) | (((xop) & 0x1ff) << 1) | (((oe) & 1) << 10) | ((rc) & 1))
#define XO_MASK XO (0x3f, 0x1ff, 1, 1)

/* An XO_MASK with the RB field fixed.  */
#define XORB_MASK (XO_MASK | RB_MASK)

/* An XS form instruction.  */
#define XS(op, xop, rc) (OP (op) | (((xop) & 0x1ff) << 2) | ((rc) & 1))
#define XS_MASK XS (0x3f, 0x1ff, 1)

/* A mask for the FXM version of an XFX form instruction.  */
#define XFXFXM_MASK (X_MASK | (1 << 20) | (1 << 11))

/* An XFX form instruction with the FXM field filled in.  */
#define XFXM(op, xop, fxm) \
  (X ((op), (xop)) | (((fxm) & 0xff) << 12))

/* An XFX form instruction with the SPR field filled in.  */
#define XSPR(op, xop, spr) \
  (X ((op), (xop)) | (((spr) & 0x1f) << 16) | (((spr) & 0x3e0) << 6))
#define XSPR_MASK (X_MASK | SPR_MASK)

/* An XFX form instruction with the SPR field filled in except for the
   SPRBAT field.  */
#define XSPRBAT_MASK (XSPR_MASK &~ SPRBAT_MASK)

/* An XFX form instruction with the SPR field filled in except for the
   SPRG field.  */
#define XSPRG_MASK (XSPR_MASK &~ SPRG_MASK)

/* The BO encodings used in extended conditional branch mnemonics.  */
#define BODNZF	(0x0)
#define BODNZFP	(0x1)
#define BODZF	(0x2)
#define BODZFP	(0x3)
#define BOF	(0x4)
#define BOFP	(0x5)
#define BODNZT	(0x8)
#define BODNZTP	(0x9)
#define BODZT	(0xa)
#define BODZTP	(0xb)
#define BOT	(0xc)
#define BOTP	(0xd)
#define BODNZ	(0x10)
#define BODNZP	(0x11)
#define BODZ	(0x12)
#define BODZP	(0x13)
#define BOU	(0x14)

/* The BI condition bit encodings used in extended conditional branch
   mnemonics.  */
#define CBLT	(0)
#define CBGT	(1)
#define CBEQ	(2)
#define CBSO	(3)

/* The TO encodings used in extended trap mnemonics.  */
#define TOLGT	(0x1)
#define TOLLT	(0x2)
#define TOEQ	(0x4)
#define TOLGE	(0x5)
#define TOLNL	(0x5)
#define TOLLE	(0x6)
#define TOLNG	(0x6)
#define TOGT	(0x8)
#define TOGE	(0xc)
#define TONL	(0xc)
#define TOLT	(0x10)
#define TOLE	(0x14)
#define TONG	(0x14)
#define TONE	(0x18)
#define TOU	(0x1f)

/* Smaller names for the flags so each entry in the opcodes table will
   fit on a single line.  */
#define PPC	PPC_OPCODE_PPC | PPC_OPCODE_ANY
#define PPCCOM	PPC_OPCODE_PPC | PPC_OPCODE_COMMON | PPC_OPCODE_ANY
#define PPC32	PPC_OPCODE_PPC | PPC_OPCODE_32 | PPC_OPCODE_ANY
#define PPC64	PPC_OPCODE_PPC | PPC_OPCODE_64 | PPC_OPCODE_ANY
#define PPCONLY	PPC_OPCODE_PPC
#define	POWER	PPC_OPCODE_POWER | PPC_OPCODE_ANY
#define	POWER2	PPC_OPCODE_POWER | PPC_OPCODE_POWER2 | PPC_OPCODE_ANY
#define	POWER32	PPC_OPCODE_POWER | PPC_OPCODE_ANY | PPC_OPCODE_32
#define	COM	PPC_OPCODE_POWER | PPC_OPCODE_PPC | PPC_OPCODE_COMMON | PPC_OPCODE_ANY
#define	COM32	PPC_OPCODE_POWER | PPC_OPCODE_PPC | PPC_OPCODE_COMMON | PPC_OPCODE_ANY | PPC_OPCODE_32
#define	M601	PPC_OPCODE_POWER | PPC_OPCODE_601 | PPC_OPCODE_ANY
#define PWRCOM	PPC_OPCODE_POWER | PPC_OPCODE_601 | PPC_OPCODE_COMMON | PPC_OPCODE_ANY
#define	MFDEC1	PPC_OPCODE_POWER
#define	MFDEC2	PPC_OPCODE_PPC | PPC_OPCODE_601

/* The opcode table.

   The format of the opcode table is:

   NAME	     OPCODE	MASK		FLAGS		{ OPERANDS }

   NAME is the name of the instruction.
   OPCODE is the instruction opcode.
   MASK is the opcode mask; this is used to tell the disassembler
     which bits in the actual opcode must match OPCODE.
   FLAGS are flags indicated what processors support the instruction.
   OPERANDS is the list of operands.

   The disassembler reads the table in order and prints the first
   instruction which matches, so this table is sorted to put more
   specific instructions before more general instructions.  It is also
   sorted by major opcode.  */

const struct powerpc_opcode powerpc_opcodes[] = {
{ "tdlgti",  OPTO(2,TOLGT), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdllti",  OPTO(2,TOLLT), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdeqi",   OPTO(2,TOEQ), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdlgei",  OPTO(2,TOLGE), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdlnli",  OPTO(2,TOLNL), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdllei",  OPTO(2,TOLLE), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdlngi",  OPTO(2,TOLNG), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdgti",   OPTO(2,TOGT), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdgei",   OPTO(2,TOGE), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdnli",   OPTO(2,TONL), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdlti",   OPTO(2,TOLT), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdlei",   OPTO(2,TOLE), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdngi",   OPTO(2,TONG), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdnei",   OPTO(2,TONE), OPTO_MASK,	PPC64,		{ RA, SI } },
{ "tdi",     OP(2),	OP_MASK,	PPC64,		{ TO, RA, SI } },

{ "twlgti",  OPTO(3,TOLGT), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tlgti",   OPTO(3,TOLGT), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twllti",  OPTO(3,TOLLT), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tllti",   OPTO(3,TOLLT), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "tweqi",   OPTO(3,TOEQ), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "teqi",    OPTO(3,TOEQ), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twlgei",  OPTO(3,TOLGE), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tlgei",   OPTO(3,TOLGE), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twlnli",  OPTO(3,TOLNL), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tlnli",   OPTO(3,TOLNL), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twllei",  OPTO(3,TOLLE), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tllei",   OPTO(3,TOLLE), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twlngi",  OPTO(3,TOLNG), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tlngi",   OPTO(3,TOLNG), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twgti",   OPTO(3,TOGT), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tgti",    OPTO(3,TOGT), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twgei",   OPTO(3,TOGE), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tgei",    OPTO(3,TOGE), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twnli",   OPTO(3,TONL), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tnli",    OPTO(3,TONL), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twlti",   OPTO(3,TOLT), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tlti",    OPTO(3,TOLT), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twlei",   OPTO(3,TOLE), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tlei",    OPTO(3,TOLE), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twngi",   OPTO(3,TONG), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tngi",    OPTO(3,TONG), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twnei",   OPTO(3,TONE), OPTO_MASK,	PPCCOM,		{ RA, SI } },
{ "tnei",    OPTO(3,TONE), OPTO_MASK,	PWRCOM,		{ RA, SI } },
{ "twi",     OP(3),	OP_MASK,	PPCCOM,		{ TO, RA, SI } },
{ "ti",      OP(3),	OP_MASK,	PWRCOM,		{ TO, RA, SI } },

{ "mulli",   OP(7),	OP_MASK,	PPCCOM,		{ RT, RA, SI } },
{ "muli",    OP(7),	OP_MASK,	PWRCOM,		{ RT, RA, SI } },

{ "subfic",  OP(8),	OP_MASK,	PPCCOM,		{ RT, RA, SI } },
{ "sfi",     OP(8),	OP_MASK,	PWRCOM,		{ RT, RA, SI } },

{ "dozi",    OP(9),	OP_MASK,	M601,		{ RT, RA, SI } },

{ "cmplwi",  OPL(10,0),	OPL_MASK,	PPCCOM,		{ OBF, RA, UI } },
{ "cmpldi",  OPL(10,1), OPL_MASK,	PPC64,		{ OBF, RA, UI } },
{ "cmpli",   OP(10),	OP_MASK,	PPCONLY,	{ BF, L, RA, UI } },
{ "cmpli",   OP(10),	OP_MASK,	PWRCOM,		{ BF, RA, UI } },

{ "cmpwi",   OPL(11,0),	OPL_MASK,	PPCCOM,		{ OBF, RA, SI } },
{ "cmpdi",   OPL(11,1),	OPL_MASK,	PPC64,		{ OBF, RA, SI } },
{ "cmpi",    OP(11),	OP_MASK,	PPCONLY,	{ BF, L, RA, SI } },
{ "cmpi",    OP(11),	OP_MASK,	PWRCOM,		{ BF, RA, SI } },

{ "addic",   OP(12),	OP_MASK,	PPCCOM,		{ RT, RA, SI } },
{ "ai",	     OP(12),	OP_MASK,	PWRCOM,		{ RT, RA, SI } },
{ "subic",   OP(12),	OP_MASK,	PPCCOM,		{ RT, RA, NSI } },

{ "addic.",  OP(13),	OP_MASK,	PPCCOM,		{ RT, RA, SI } },
{ "ai.",     OP(13),	OP_MASK,	PWRCOM,		{ RT, RA, SI } },
{ "subic.",  OP(13),	OP_MASK,	PPCCOM,		{ RT, RA, NSI } },

{ "li",	     OP(14),	DRA_MASK,	PPCCOM,		{ RT, SI } },
{ "lil",     OP(14),	DRA_MASK,	PWRCOM,		{ RT, SI } },
{ "addi",    OP(14),	OP_MASK,	PPCCOM,		{ RT, RA, SI } },
{ "cal",     OP(14),	OP_MASK,	PWRCOM,		{ RT, D, RA } },
{ "subi",    OP(14),	OP_MASK,	PPCCOM,		{ RT, RA, NSI } },
{ "la",	     OP(14),	OP_MASK,	PPCCOM,		{ RT, D, RA } },

{ "lis",     OP(15),	DRA_MASK,	PPCCOM,		{ RT, SISIGNOPT } },
{ "liu",     OP(15),	DRA_MASK,	PWRCOM,		{ RT, SISIGNOPT } },
{ "addis",   OP(15),	OP_MASK,	PPCCOM,		{ RT,RA,SISIGNOPT } },
{ "cau",     OP(15),	OP_MASK,	PWRCOM,		{ RT,RA,SISIGNOPT } },
{ "subis",   OP(15),	OP_MASK,	PPCCOM,		{ RT, RA, NSI } },

{ "bdnz-",   BBO(16,BODNZ,0,0), BBOYBI_MASK, PPC,	{ BDM } },
{ "bdnz+",   BBO(16,BODNZ,0,0), BBOYBI_MASK, PPC,	{ BDP } },
{ "bdnz",    BBO(16,BODNZ,0,0), BBOYBI_MASK, PPCCOM,	{ BD } },
{ "bdn",     BBO(16,BODNZ,0,0), BBOYBI_MASK, PWRCOM,	{ BD } },
{ "bdnzl-",  BBO(16,BODNZ,0,1), BBOYBI_MASK, PPC,	{ BDM } },
{ "bdnzl+",  BBO(16,BODNZ,0,1), BBOYBI_MASK, PPC,	{ BDP } },
{ "bdnzl",   BBO(16,BODNZ,0,1), BBOYBI_MASK, PPCCOM,	{ BD } },
{ "bdnl",    BBO(16,BODNZ,0,1), BBOYBI_MASK, PWRCOM,	{ BD } },
{ "bdnza-",  BBO(16,BODNZ,1,0), BBOYBI_MASK, PPC,	{ BDMA } },
{ "bdnza+",  BBO(16,BODNZ,1,0), BBOYBI_MASK, PPC,	{ BDPA } },
{ "bdnza",   BBO(16,BODNZ,1,0), BBOYBI_MASK, PPCCOM,	{ BDA } },
{ "bdna",    BBO(16,BODNZ,1,0), BBOYBI_MASK, PWRCOM,	{ BDA } },
{ "bdnzla-", BBO(16,BODNZ,1,1), BBOYBI_MASK, PPC,	{ BDMA } },
{ "bdnzla+", BBO(16,BODNZ,1,1), BBOYBI_MASK, PPC,	{ BDPA } },
{ "bdnzla",  BBO(16,BODNZ,1,1), BBOYBI_MASK, PPCCOM,	{ BDA } },
{ "bdnla",   BBO(16,BODNZ,1,1), BBOYBI_MASK, PWRCOM,	{ BDA } },
{ "bdz-",    BBO(16,BODZ,0,0),  BBOYBI_MASK, PPC,	{ BDM } },
{ "bdz+",    BBO(16,BODZ,0,0),  BBOYBI_MASK, PPC,	{ BDP } },
{ "bdz",     BBO(16,BODZ,0,0),  BBOYBI_MASK, COM,	{ BD } },
{ "bdzl-",   BBO(16,BODZ,0,1),  BBOYBI_MASK, PPC,	{ BDM } },
{ "bdzl+",   BBO(16,BODZ,0,1),  BBOYBI_MASK, PPC,	{ BDP } },
{ "bdzl",    BBO(16,BODZ,0,1),  BBOYBI_MASK, COM,	{ BD } },
{ "bdza-",   BBO(16,BODZ,1,0),  BBOYBI_MASK, PPC,	{ BDMA } },
{ "bdza+",   BBO(16,BODZ,1,0),  BBOYBI_MASK, PPC,	{ BDPA } },
{ "bdza",    BBO(16,BODZ,1,0),  BBOYBI_MASK, COM,	{ BDA } },
{ "bdzla-",  BBO(16,BODZ,1,1),  BBOYBI_MASK, PPC,	{ BDMA } },
{ "bdzla+",  BBO(16,BODZ,1,1),  BBOYBI_MASK, PPC,	{ BDPA } },
{ "bdzla",   BBO(16,BODZ,1,1),  BBOYBI_MASK, COM,	{ BDA } },
{ "blt-",    BBOCB(16,BOT,CBLT,0,0), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "blt+",    BBOCB(16,BOT,CBLT,0,0), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "blt",     BBOCB(16,BOT,CBLT,0,0), BBOYCB_MASK, COM,	{ CR, BD } },
{ "bltl-",   BBOCB(16,BOT,CBLT,0,1), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bltl+",   BBOCB(16,BOT,CBLT,0,1), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bltl",    BBOCB(16,BOT,CBLT,0,1), BBOYCB_MASK, COM,	{ CR, BD } },
{ "blta-",   BBOCB(16,BOT,CBLT,1,0), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "blta+",   BBOCB(16,BOT,CBLT,1,0), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "blta",    BBOCB(16,BOT,CBLT,1,0), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "bltla-",  BBOCB(16,BOT,CBLT,1,1), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bltla+",  BBOCB(16,BOT,CBLT,1,1), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bltla",   BBOCB(16,BOT,CBLT,1,1), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "bgt-",    BBOCB(16,BOT,CBGT,0,0), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bgt+",    BBOCB(16,BOT,CBGT,0,0), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bgt",     BBOCB(16,BOT,CBGT,0,0), BBOYCB_MASK, COM,	{ CR, BD } },
{ "bgtl-",   BBOCB(16,BOT,CBGT,0,1), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bgtl+",   BBOCB(16,BOT,CBGT,0,1), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bgtl",    BBOCB(16,BOT,CBGT,0,1), BBOYCB_MASK, COM,	{ CR, BD } },
{ "bgta-",   BBOCB(16,BOT,CBGT,1,0), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bgta+",   BBOCB(16,BOT,CBGT,1,0), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bgta",    BBOCB(16,BOT,CBGT,1,0), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "bgtla-",  BBOCB(16,BOT,CBGT,1,1), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bgtla+",  BBOCB(16,BOT,CBGT,1,1), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bgtla",   BBOCB(16,BOT,CBGT,1,1), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "beq-",    BBOCB(16,BOT,CBEQ,0,0), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "beq+",    BBOCB(16,BOT,CBEQ,0,0), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "beq",     BBOCB(16,BOT,CBEQ,0,0), BBOYCB_MASK, COM,	{ CR, BD } },
{ "beql-",   BBOCB(16,BOT,CBEQ,0,1), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "beql+",   BBOCB(16,BOT,CBEQ,0,1), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "beql",    BBOCB(16,BOT,CBEQ,0,1), BBOYCB_MASK, COM,	{ CR, BD } },
{ "beqa-",   BBOCB(16,BOT,CBEQ,1,0), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "beqa+",   BBOCB(16,BOT,CBEQ,1,0), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "beqa",    BBOCB(16,BOT,CBEQ,1,0), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "beqla-",  BBOCB(16,BOT,CBEQ,1,1), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "beqla+",  BBOCB(16,BOT,CBEQ,1,1), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "beqla",   BBOCB(16,BOT,CBEQ,1,1), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "bso-",    BBOCB(16,BOT,CBSO,0,0), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bso+",    BBOCB(16,BOT,CBSO,0,0), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bso",     BBOCB(16,BOT,CBSO,0,0), BBOYCB_MASK, COM,	{ CR, BD } },
{ "bsol-",   BBOCB(16,BOT,CBSO,0,1), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bsol+",   BBOCB(16,BOT,CBSO,0,1), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bsol",    BBOCB(16,BOT,CBSO,0,1), BBOYCB_MASK, COM,	{ CR, BD } },
{ "bsoa-",   BBOCB(16,BOT,CBSO,1,0), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bsoa+",   BBOCB(16,BOT,CBSO,1,0), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bsoa",    BBOCB(16,BOT,CBSO,1,0), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "bsola-",  BBOCB(16,BOT,CBSO,1,1), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bsola+",  BBOCB(16,BOT,CBSO,1,1), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bsola",   BBOCB(16,BOT,CBSO,1,1), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "bun-",    BBOCB(16,BOT,CBSO,0,0), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bun+",    BBOCB(16,BOT,CBSO,0,0), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bun",     BBOCB(16,BOT,CBSO,0,0), BBOYCB_MASK, PPCCOM, { CR, BD } },
{ "bunl-",   BBOCB(16,BOT,CBSO,0,1), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bunl+",   BBOCB(16,BOT,CBSO,0,1), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bunl",    BBOCB(16,BOT,CBSO,0,1), BBOYCB_MASK, PPCCOM, { CR, BD } },
{ "buna-",   BBOCB(16,BOT,CBSO,1,0), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "buna+",   BBOCB(16,BOT,CBSO,1,0), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "buna",    BBOCB(16,BOT,CBSO,1,0), BBOYCB_MASK, PPCCOM, { CR, BDA } },
{ "bunla-",  BBOCB(16,BOT,CBSO,1,1), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bunla+",  BBOCB(16,BOT,CBSO,1,1), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bunla",   BBOCB(16,BOT,CBSO,1,1), BBOYCB_MASK, PPCCOM, { CR, BDA } },
{ "bge-",    BBOCB(16,BOF,CBLT,0,0), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bge+",    BBOCB(16,BOF,CBLT,0,0), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bge",     BBOCB(16,BOF,CBLT,0,0), BBOYCB_MASK, COM,	{ CR, BD } },
{ "bgel-",   BBOCB(16,BOF,CBLT,0,1), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bgel+",   BBOCB(16,BOF,CBLT,0,1), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bgel",    BBOCB(16,BOF,CBLT,0,1), BBOYCB_MASK, COM,	{ CR, BD } },
{ "bgea-",   BBOCB(16,BOF,CBLT,1,0), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bgea+",   BBOCB(16,BOF,CBLT,1,0), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bgea",    BBOCB(16,BOF,CBLT,1,0), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "bgela-",  BBOCB(16,BOF,CBLT,1,1), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bgela+",  BBOCB(16,BOF,CBLT,1,1), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bgela",   BBOCB(16,BOF,CBLT,1,1), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "bnl-",    BBOCB(16,BOF,CBLT,0,0), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bnl+",    BBOCB(16,BOF,CBLT,0,0), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bnl",     BBOCB(16,BOF,CBLT,0,0), BBOYCB_MASK, COM,	{ CR, BD } },
{ "bnll-",   BBOCB(16,BOF,CBLT,0,1), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bnll+",   BBOCB(16,BOF,CBLT,0,1), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bnll",    BBOCB(16,BOF,CBLT,0,1), BBOYCB_MASK, COM,	{ CR, BD } },
{ "bnla-",   BBOCB(16,BOF,CBLT,1,0), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bnla+",   BBOCB(16,BOF,CBLT,1,0), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bnla",    BBOCB(16,BOF,CBLT,1,0), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "bnlla-",  BBOCB(16,BOF,CBLT,1,1), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bnlla+",  BBOCB(16,BOF,CBLT,1,1), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bnlla",   BBOCB(16,BOF,CBLT,1,1), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "ble-",    BBOCB(16,BOF,CBGT,0,0), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "ble+",    BBOCB(16,BOF,CBGT,0,0), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "ble",     BBOCB(16,BOF,CBGT,0,0), BBOYCB_MASK, COM,	{ CR, BD } },
{ "blel-",   BBOCB(16,BOF,CBGT,0,1), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "blel+",   BBOCB(16,BOF,CBGT,0,1), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "blel",    BBOCB(16,BOF,CBGT,0,1), BBOYCB_MASK, COM,	{ CR, BD } },
{ "blea-",   BBOCB(16,BOF,CBGT,1,0), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "blea+",   BBOCB(16,BOF,CBGT,1,0), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "blea",    BBOCB(16,BOF,CBGT,1,0), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "blela-",  BBOCB(16,BOF,CBGT,1,1), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "blela+",  BBOCB(16,BOF,CBGT,1,1), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "blela",   BBOCB(16,BOF,CBGT,1,1), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "bng-",    BBOCB(16,BOF,CBGT,0,0), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bng+",    BBOCB(16,BOF,CBGT,0,0), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bng",     BBOCB(16,BOF,CBGT,0,0), BBOYCB_MASK, COM,	{ CR, BD } },
{ "bngl-",   BBOCB(16,BOF,CBGT,0,1), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bngl+",   BBOCB(16,BOF,CBGT,0,1), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bngl",    BBOCB(16,BOF,CBGT,0,1), BBOYCB_MASK, COM,	{ CR, BD } },
{ "bnga-",   BBOCB(16,BOF,CBGT,1,0), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bnga+",   BBOCB(16,BOF,CBGT,1,0), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bnga",    BBOCB(16,BOF,CBGT,1,0), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "bngla-",  BBOCB(16,BOF,CBGT,1,1), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bngla+",  BBOCB(16,BOF,CBGT,1,1), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bngla",   BBOCB(16,BOF,CBGT,1,1), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "bne-",    BBOCB(16,BOF,CBEQ,0,0), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bne+",    BBOCB(16,BOF,CBEQ,0,0), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bne",     BBOCB(16,BOF,CBEQ,0,0), BBOYCB_MASK, COM,	{ CR, BD } },
{ "bnel-",   BBOCB(16,BOF,CBEQ,0,1), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bnel+",   BBOCB(16,BOF,CBEQ,0,1), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bnel",    BBOCB(16,BOF,CBEQ,0,1), BBOYCB_MASK, COM,	{ CR, BD } },
{ "bnea-",   BBOCB(16,BOF,CBEQ,1,0), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bnea+",   BBOCB(16,BOF,CBEQ,1,0), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bnea",    BBOCB(16,BOF,CBEQ,1,0), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "bnela-",  BBOCB(16,BOF,CBEQ,1,1), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bnela+",  BBOCB(16,BOF,CBEQ,1,1), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bnela",   BBOCB(16,BOF,CBEQ,1,1), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "bns-",    BBOCB(16,BOF,CBSO,0,0), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bns+",    BBOCB(16,BOF,CBSO,0,0), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bns",     BBOCB(16,BOF,CBSO,0,0), BBOYCB_MASK, COM,	{ CR, BD } },
{ "bnsl-",   BBOCB(16,BOF,CBSO,0,1), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bnsl+",   BBOCB(16,BOF,CBSO,0,1), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bnsl",    BBOCB(16,BOF,CBSO,0,1), BBOYCB_MASK, COM,	{ CR, BD } },
{ "bnsa-",   BBOCB(16,BOF,CBSO,1,0), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bnsa+",   BBOCB(16,BOF,CBSO,1,0), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bnsa",    BBOCB(16,BOF,CBSO,1,0), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "bnsla-",  BBOCB(16,BOF,CBSO,1,1), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bnsla+",  BBOCB(16,BOF,CBSO,1,1), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bnsla",   BBOCB(16,BOF,CBSO,1,1), BBOYCB_MASK, COM,	{ CR, BDA } },
{ "bnu-",    BBOCB(16,BOF,CBSO,0,0), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bnu+",    BBOCB(16,BOF,CBSO,0,0), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bnu",     BBOCB(16,BOF,CBSO,0,0), BBOYCB_MASK, PPCCOM, { CR, BD } },
{ "bnul-",   BBOCB(16,BOF,CBSO,0,1), BBOYCB_MASK, PPC,	{ CR, BDM } },
{ "bnul+",   BBOCB(16,BOF,CBSO,0,1), BBOYCB_MASK, PPC,	{ CR, BDP } },
{ "bnul",    BBOCB(16,BOF,CBSO,0,1), BBOYCB_MASK, PPCCOM, { CR, BD } },
{ "bnua-",   BBOCB(16,BOF,CBSO,1,0), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bnua+",   BBOCB(16,BOF,CBSO,1,0), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bnua",    BBOCB(16,BOF,CBSO,1,0), BBOYCB_MASK, PPCCOM, { CR, BDA } },
{ "bnula-",  BBOCB(16,BOF,CBSO,1,1), BBOYCB_MASK, PPC,	{ CR, BDMA } },
{ "bnula+",  BBOCB(16,BOF,CBSO,1,1), BBOYCB_MASK, PPC,	{ CR, BDPA } },
{ "bnula",   BBOCB(16,BOF,CBSO,1,1), BBOYCB_MASK, PPCCOM, { CR, BDA } },
{ "bdnzt-",  BBO(16,BODNZT,0,0), BBOY_MASK, PPC,	{ BI, BDM } },
{ "bdnzt+",  BBO(16,BODNZT,0,0), BBOY_MASK, PPC,	{ BI, BDP } },
{ "bdnzt",   BBO(16,BODNZT,0,0), BBOY_MASK, PPCCOM,	{ BI, BD } },
{ "bdnztl-", BBO(16,BODNZT,0,1), BBOY_MASK, PPC,	{ BI, BDM } },
{ "bdnztl+", BBO(16,BODNZT,0,1), BBOY_MASK, PPC,	{ BI, BDP } },
{ "bdnztl",  BBO(16,BODNZT,0,1), BBOY_MASK, PPCCOM,	{ BI, BD } },
{ "bdnzta-", BBO(16,BODNZT,1,0), BBOY_MASK, PPC,	{ BI, BDMA } },
{ "bdnzta+", BBO(16,BODNZT,1,0), BBOY_MASK, PPC,	{ BI, BDPA } },
{ "bdnzta",  BBO(16,BODNZT,1,0), BBOY_MASK, PPCCOM,	{ BI, BDA } },
{ "bdnztla-",BBO(16,BODNZT,1,1), BBOY_MASK, PPC,	{ BI, BDMA } },
{ "bdnztla+",BBO(16,BODNZT,1,1), BBOY_MASK, PPC,	{ BI, BDPA } },
{ "bdnztla", BBO(16,BODNZT,1,1), BBOY_MASK, PPCCOM,	{ BI, BDA } },
{ "bdnzf-",  BBO(16,BODNZF,0,0), BBOY_MASK, PPC,	{ BI, BDM } },
{ "bdnzf+",  BBO(16,BODNZF,0,0), BBOY_MASK, PPC,	{ BI, BDP } },
{ "bdnzf",   BBO(16,BODNZF,0,0), BBOY_MASK, PPCCOM,	{ BI, BD } },
{ "bdnzfl-", BBO(16,BODNZF,0,1), BBOY_MASK, PPC,	{ BI, BDM } },
{ "bdnzfl+", BBO(16,BODNZF,0,1), BBOY_MASK, PPC,	{ BI, BDP } },
{ "bdnzfl",  BBO(16,BODNZF,0,1), BBOY_MASK, PPCCOM,	{ BI, BD } },
{ "bdnzfa-", BBO(16,BODNZF,1,0), BBOY_MASK, PPC,	{ BI, BDMA } },
{ "bdnzfa+", BBO(16,BODNZF,1,0), BBOY_MASK, PPC,	{ BI, BDPA } },
{ "bdnzfa",  BBO(16,BODNZF,1,0), BBOY_MASK, PPCCOM,	{ BI, BDA } },
{ "bdnzfla-",BBO(16,BODNZF,1,1), BBOY_MASK, PPC,	{ BI, BDMA } },
{ "bdnzfla+",BBO(16,BODNZF,1,1), BBOY_MASK, PPC,	{ BI, BDPA } },
{ "bdnzfla", BBO(16,BODNZF,1,1), BBOY_MASK, PPCCOM,	{ BI, BDA } },
{ "bt-",     BBO(16,BOT,0,0), BBOY_MASK, PPC,		{ BI, BDM } },
{ "bt+",     BBO(16,BOT,0,0), BBOY_MASK, PPC,		{ BI, BDP } },
{ "bt",	     BBO(16,BOT,0,0), BBOY_MASK, PPCCOM,	{ BI, BD } },
{ "bbt",     BBO(16,BOT,0,0), BBOY_MASK, PWRCOM,	{ BI, BD } },
{ "btl-",    BBO(16,BOT,0,1), BBOY_MASK, PPC,		{ BI, BDM } },
{ "btl+",    BBO(16,BOT,0,1), BBOY_MASK, PPC,		{ BI, BDP } },
{ "btl",     BBO(16,BOT,0,1), BBOY_MASK, PPCCOM,	{ BI, BD } },
{ "bbtl",    BBO(16,BOT,0,1), BBOY_MASK, PWRCOM,	{ BI, BD } },
{ "bta-",    BBO(16,BOT,1,0), BBOY_MASK, PPC,		{ BI, BDMA } },
{ "bta+",    BBO(16,BOT,1,0), BBOY_MASK, PPC,		{ BI, BDPA } },
{ "bta",     BBO(16,BOT,1,0), BBOY_MASK, PPCCOM,	{ BI, BDA } },
{ "bbta",    BBO(16,BOT,1,0), BBOY_MASK, PWRCOM,	{ BI, BDA } },
{ "btla-",   BBO(16,BOT,1,1), BBOY_MASK, PPC,		{ BI, BDMA } },
{ "btla+",   BBO(16,BOT,1,1), BBOY_MASK, PPC,		{ BI, BDPA } },
{ "btla",    BBO(16,BOT,1,1), BBOY_MASK, PPCCOM,	{ BI, BDA } },
{ "bbtla",   BBO(16,BOT,1,1), BBOY_MASK, PWRCOM,	{ BI, BDA } },
{ "bf-",     BBO(16,BOF,0,0), BBOY_MASK, PPC,		{ BI, BDM } },
{ "bf+",     BBO(16,BOF,0,0), BBOY_MASK, PPC,		{ BI, BDP } },
{ "bf",	     BBO(16,BOF,0,0), BBOY_MASK, PPCCOM,	{ BI, BD } },
{ "bbf",     BBO(16,BOF,0,0), BBOY_MASK, PWRCOM,	{ BI, BD } },
{ "bfl-",    BBO(16,BOF,0,1), BBOY_MASK, PPC,		{ BI, BDM } },
{ "bfl+",    BBO(16,BOF,0,1), BBOY_MASK, PPC,		{ BI, BDP } },
{ "bfl",     BBO(16,BOF,0,1), BBOY_MASK, PPCCOM,	{ BI, BD } },
{ "bbfl",    BBO(16,BOF,0,1), BBOY_MASK, PWRCOM,	{ BI, BD } },
{ "bfa-",    BBO(16,BOF,1,0), BBOY_MASK, PPC,		{ BI, BDMA } },
{ "bfa+",    BBO(16,BOF,1,0), BBOY_MASK, PPC,		{ BI, BDPA } },
{ "bfa",     BBO(16,BOF,1,0), BBOY_MASK, PPCCOM,	{ BI, BDA } },
{ "bbfa",    BBO(16,BOF,1,0), BBOY_MASK, PWRCOM,	{ BI, BDA } },
{ "bfla-",   BBO(16,BOF,1,1), BBOY_MASK, PPC,		{ BI, BDMA } },
{ "bfla+",   BBO(16,BOF,1,1), BBOY_MASK, PPC,		{ BI, BDPA } },
{ "bfla",    BBO(16,BOF,1,1), BBOY_MASK, PPCCOM,	{ BI, BDA } },
{ "bbfla",   BBO(16,BOF,1,1), BBOY_MASK, PWRCOM,	{ BI, BDA } },
{ "bdzt-",   BBO(16,BODZT,0,0), BBOY_MASK, PPC,		{ BI, BDM } },
{ "bdzt+",   BBO(16,BODZT,0,0), BBOY_MASK, PPC,		{ BI, BDP } },
{ "bdzt",    BBO(16,BODZT,0,0), BBOY_MASK, PPCCOM,	{ BI, BD } },
{ "bdztl-",  BBO(16,BODZT,0,1), BBOY_MASK, PPC,		{ BI, BDM } },
{ "bdztl+",  BBO(16,BODZT,0,1), BBOY_MASK, PPC,		{ BI, BDP } },
{ "bdztl",   BBO(16,BODZT,0,1), BBOY_MASK, PPCCOM,	{ BI, BD } },
{ "bdzta-",  BBO(16,BODZT,1,0), BBOY_MASK, PPC,		{ BI, BDMA } },
{ "bdzta+",  BBO(16,BODZT,1,0), BBOY_MASK, PPC,		{ BI, BDPA } },
{ "bdzta",   BBO(16,BODZT,1,0), BBOY_MASK, PPCCOM,	{ BI, BDA } },
{ "bdztla-", BBO(16,BODZT,1,1), BBOY_MASK, PPC,		{ BI, BDMA } },
{ "bdztla+", BBO(16,BODZT,1,1), BBOY_MASK, PPC,		{ BI, BDPA } },
{ "bdztla",  BBO(16,BODZT,1,1), BBOY_MASK, PPCCOM,	{ BI, BDA } },
{ "bdzf-",   BBO(16,BODZF,0,0), BBOY_MASK, PPC,		{ BI, BDM } },
{ "bdzf+",   BBO(16,BODZF,0,0), BBOY_MASK, PPC,		{ BI, BDP } },
{ "bdzf",    BBO(16,BODZF,0,0), BBOY_MASK, PPCCOM,	{ BI, BD } },
{ "bdzfl-",  BBO(16,BODZF,0,1), BBOY_MASK, PPC,		{ BI, BDM } },
{ "bdzfl+",  BBO(16,BODZF,0,1), BBOY_MASK, PPC,		{ BI, BDP } },
{ "bdzfl",   BBO(16,BODZF,0,1), BBOY_MASK, PPCCOM,	{ BI, BD } },
{ "bdzfa-",  BBO(16,BODZF,1,0), BBOY_MASK, PPC,		{ BI, BDMA } },
{ "bdzfa+",  BBO(16,BODZF,1,0), BBOY_MASK, PPC,		{ BI, BDPA } },
{ "bdzfa",   BBO(16,BODZF,1,0), BBOY_MASK, PPCCOM,	{ BI, BDA } },
{ "bdzfla-", BBO(16,BODZF,1,1), BBOY_MASK, PPC,		{ BI, BDMA } },
{ "bdzfla+", BBO(16,BODZF,1,1), BBOY_MASK, PPC,		{ BI, BDPA } },
{ "bdzfla",  BBO(16,BODZF,1,1), BBOY_MASK, PPCCOM,	{ BI, BDA } },
{ "bc-",     B(16,0,0),	B_MASK,		PPC,		{ BOE, BI, BDM } },
{ "bc+",     B(16,0,0),	B_MASK,		PPC,		{ BOE, BI, BDP } },
{ "bc",	     B(16,0,0),	B_MASK,		COM,		{ BO, BI, BD } },
{ "bcl-",    B(16,0,1),	B_MASK,		PPC,		{ BOE, BI, BDM } },
{ "bcl+",    B(16,0,1),	B_MASK,		PPC,		{ BOE, BI, BDP } },
{ "bcl",     B(16,0,1),	B_MASK,		COM,		{ BO, BI, BD } },
{ "bca-",    B(16,1,0),	B_MASK,		PPC,		{ BOE, BI, BDMA } },
{ "bca+",    B(16,1,0),	B_MASK,		PPC,		{ BOE, BI, BDPA } },
{ "bca",     B(16,1,0),	B_MASK,		COM,		{ BO, BI, BDA } },
{ "bcla-",   B(16,1,1),	B_MASK,		PPC,		{ BOE, BI, BDMA } },
{ "bcla+",   B(16,1,1),	B_MASK,		PPC,		{ BOE, BI, BDPA } },
{ "bcla",    B(16,1,1),	B_MASK,		COM,		{ BO, BI, BDA } },

{ "sc",      SC(17,1,0), 0xffffffff,	PPC,		{ 0 } },
{ "svc",     SC(17,0,0), SC_MASK,	POWER,		{ LEV, FL1, FL2 } },
{ "svcl",    SC(17,0,1), SC_MASK,	POWER,		{ LEV, FL1, FL2 } },
{ "svca",    SC(17,1,0), SC_MASK,	PWRCOM,		{ SV } },
{ "svcla",   SC(17,1,1), SC_MASK,	POWER,		{ SV } },

{ "b",	     B(18,0,0),	B_MASK,		COM,	{ LI } },
{ "bl",      B(18,0,1),	B_MASK,		COM,	{ LI } },
{ "ba",      B(18,1,0),	B_MASK,		COM,	{ LIA } },
{ "bla",     B(18,1,1),	B_MASK,		COM,	{ LIA } },

{ "mcrf",    XL(19,0),	XLBB_MASK|(3<<21)|(3<<16), COM,	{ BF, BFA } },

{ "blr",     XLO(19,BOU,16,0), XLBOBIBB_MASK, PPCCOM,	{ 0 } },
{ "br",      XLO(19,BOU,16,0), XLBOBIBB_MASK, PWRCOM,	{ 0 } },
{ "blrl",    XLO(19,BOU,16,1), XLBOBIBB_MASK, PPCCOM,	{ 0 } },
{ "brl",     XLO(19,BOU,16,1), XLBOBIBB_MASK, PWRCOM,	{ 0 } },
{ "bdnzlr",  XLO(19,BODNZ,16,0), XLBOBIBB_MASK, PPCCOM,	{ 0 } },
{ "bdnzlr-", XLO(19,BODNZ,16,0), XLBOBIBB_MASK, PPC,	{ 0 } },
{ "bdnzlr+", XLO(19,BODNZP,16,0), XLBOBIBB_MASK, PPC,	{ 0 } },
{ "bdnzlrl", XLO(19,BODNZ,16,1), XLBOBIBB_MASK, PPCCOM,	{ 0 } },
{ "bdnzlrl-",XLO(19,BODNZ,16,1), XLBOBIBB_MASK, PPC,	{ 0 } },
{ "bdnzlrl+",XLO(19,BODNZP,16,1), XLBOBIBB_MASK, PPC,	{ 0 } },
{ "bdzlr",   XLO(19,BODZ,16,0), XLBOBIBB_MASK, PPCCOM,	{ 0 } },
{ "bdzlr-",  XLO(19,BODZ,16,0), XLBOBIBB_MASK, PPC,	{ 0 } },
{ "bdzlr+",  XLO(19,BODZP,16,0), XLBOBIBB_MASK, PPC,	{ 0 } },
{ "bdzlrl",  XLO(19,BODZ,16,1), XLBOBIBB_MASK, PPCCOM,	{ 0 } },
{ "bdzlrl-", XLO(19,BODZ,16,1), XLBOBIBB_MASK, PPC,	{ 0 } },
{ "bdzlrl+", XLO(19,BODZP,16,1), XLBOBIBB_MASK, PPC,	{ 0 } },
{ "bltlr",   XLOCB(19,BOT,CBLT,16,0), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bltlr-",  XLOCB(19,BOT,CBLT,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bltlr+",  XLOCB(19,BOTP,CBLT,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bltr",    XLOCB(19,BOT,CBLT,16,0), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "bltlrl",  XLOCB(19,BOT,CBLT,16,1), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bltlrl-", XLOCB(19,BOT,CBLT,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bltlrl+", XLOCB(19,BOTP,CBLT,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bltrl",   XLOCB(19,BOT,CBLT,16,1), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "bgtlr",   XLOCB(19,BOT,CBGT,16,0), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bgtlr-",  XLOCB(19,BOT,CBGT,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bgtlr+",  XLOCB(19,BOTP,CBGT,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bgtr",    XLOCB(19,BOT,CBGT,16,0), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "bgtlrl",  XLOCB(19,BOT,CBGT,16,1), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bgtlrl-", XLOCB(19,BOT,CBGT,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bgtlrl+", XLOCB(19,BOTP,CBGT,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bgtrl",   XLOCB(19,BOT,CBGT,16,1), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "beqlr",   XLOCB(19,BOT,CBEQ,16,0), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "beqlr-",  XLOCB(19,BOT,CBEQ,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "beqlr+",  XLOCB(19,BOTP,CBEQ,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "beqr",    XLOCB(19,BOT,CBEQ,16,0), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "beqlrl",  XLOCB(19,BOT,CBEQ,16,1), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "beqlrl-", XLOCB(19,BOT,CBEQ,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "beqlrl+", XLOCB(19,BOTP,CBEQ,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "beqrl",   XLOCB(19,BOT,CBEQ,16,1), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "bsolr",   XLOCB(19,BOT,CBSO,16,0), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bsolr-",  XLOCB(19,BOT,CBSO,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bsolr+",  XLOCB(19,BOTP,CBSO,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bsor",    XLOCB(19,BOT,CBSO,16,0), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "bsolrl",  XLOCB(19,BOT,CBSO,16,1), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bsolrl-", XLOCB(19,BOT,CBSO,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bsolrl+", XLOCB(19,BOTP,CBSO,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bsorl",   XLOCB(19,BOT,CBSO,16,1), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "bunlr",   XLOCB(19,BOT,CBSO,16,0), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bunlr-",  XLOCB(19,BOT,CBSO,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bunlr+",  XLOCB(19,BOTP,CBSO,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bunlrl",  XLOCB(19,BOT,CBSO,16,1), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bunlrl-", XLOCB(19,BOT,CBSO,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bunlrl+", XLOCB(19,BOTP,CBSO,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bgelr",   XLOCB(19,BOF,CBLT,16,0), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bgelr-",  XLOCB(19,BOF,CBLT,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bgelr+",  XLOCB(19,BOFP,CBLT,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bger",    XLOCB(19,BOF,CBLT,16,0), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "bgelrl",  XLOCB(19,BOF,CBLT,16,1), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bgelrl-", XLOCB(19,BOF,CBLT,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bgelrl+", XLOCB(19,BOFP,CBLT,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bgerl",   XLOCB(19,BOF,CBLT,16,1), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "bnllr",   XLOCB(19,BOF,CBLT,16,0), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bnllr-",  XLOCB(19,BOF,CBLT,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bnllr+",  XLOCB(19,BOFP,CBLT,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bnlr",    XLOCB(19,BOF,CBLT,16,0), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "bnllrl",  XLOCB(19,BOF,CBLT,16,1), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bnllrl-", XLOCB(19,BOF,CBLT,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bnllrl+", XLOCB(19,BOFP,CBLT,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bnlrl",   XLOCB(19,BOF,CBLT,16,1), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "blelr",   XLOCB(19,BOF,CBGT,16,0), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "blelr-",  XLOCB(19,BOF,CBGT,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "blelr+",  XLOCB(19,BOFP,CBGT,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bler",    XLOCB(19,BOF,CBGT,16,0), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "blelrl",  XLOCB(19,BOF,CBGT,16,1), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "blelrl-", XLOCB(19,BOF,CBGT,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "blelrl+", XLOCB(19,BOFP,CBGT,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "blerl",   XLOCB(19,BOF,CBGT,16,1), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "bnglr",   XLOCB(19,BOF,CBGT,16,0), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bnglr-",  XLOCB(19,BOF,CBGT,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bnglr+",  XLOCB(19,BOFP,CBGT,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bngr",    XLOCB(19,BOF,CBGT,16,0), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "bnglrl",  XLOCB(19,BOF,CBGT,16,1), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bnglrl-", XLOCB(19,BOF,CBGT,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bnglrl+", XLOCB(19,BOFP,CBGT,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bngrl",   XLOCB(19,BOF,CBGT,16,1), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "bnelr",   XLOCB(19,BOF,CBEQ,16,0), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bnelr-",  XLOCB(19,BOF,CBEQ,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bnelr+",  XLOCB(19,BOFP,CBEQ,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bner",    XLOCB(19,BOF,CBEQ,16,0), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "bnelrl",  XLOCB(19,BOF,CBEQ,16,1), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bnelrl-", XLOCB(19,BOF,CBEQ,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bnelrl+", XLOCB(19,BOFP,CBEQ,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bnerl",   XLOCB(19,BOF,CBEQ,16,1), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "bnslr",   XLOCB(19,BOF,CBSO,16,0), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bnslr-",  XLOCB(19,BOF,CBSO,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bnslr+",  XLOCB(19,BOFP,CBSO,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bnsr",    XLOCB(19,BOF,CBSO,16,0), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "bnslrl",  XLOCB(19,BOF,CBSO,16,1), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bnslrl-", XLOCB(19,BOF,CBSO,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bnslrl+", XLOCB(19,BOFP,CBSO,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bnsrl",   XLOCB(19,BOF,CBSO,16,1), XLBOCBBB_MASK, PWRCOM, { CR } },
{ "bnulr",   XLOCB(19,BOF,CBSO,16,0), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bnulr-",  XLOCB(19,BOF,CBSO,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bnulr+",  XLOCB(19,BOFP,CBSO,16,0), XLBOCBBB_MASK, PPC, { CR } },
{ "bnulrl",  XLOCB(19,BOF,CBSO,16,1), XLBOCBBB_MASK, PPCCOM, { CR } },
{ "bnulrl-", XLOCB(19,BOF,CBSO,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "bnulrl+", XLOCB(19,BOFP,CBSO,16,1), XLBOCBBB_MASK, PPC, { CR } },
{ "btlr",    XLO(19,BOT,16,0), XLBOBB_MASK, PPCCOM,	{ BI } },
{ "btlr-",   XLO(19,BOT,16,0), XLBOBB_MASK, PPC,	{ BI } },
{ "btlr+",   XLO(19,BOTP,16,0), XLBOBB_MASK, PPC,	{ BI } },
{ "bbtr",    XLO(19,BOT,16,0), XLBOBB_MASK, PWRCOM,	{ BI } },
{ "btlrl",   XLO(19,BOT,16,1), XLBOBB_MASK, PPCCOM,	{ BI } },
{ "btlrl-",  XLO(19,BOT,16,1), XLBOBB_MASK, PPC,	{ BI } },
{ "btlrl+",  XLO(19,BOTP,16,1), XLBOBB_MASK, PPC,	{ BI } },
{ "bbtrl",   XLO(19,BOT,16,1), XLBOBB_MASK, PWRCOM,	{ BI } },
{ "bflr",    XLO(19,BOF,16,0), XLBOBB_MASK, PPCCOM,	{ BI } },
{ "bflr-",   XLO(19,BOF,16,0), XLBOBB_MASK, PPC,	{ BI } },
{ "bflr+",   XLO(19,BOFP,16,0), XLBOBB_MASK, PPC,	{ BI } },
{ "bbfr",    XLO(19,BOF,16,0), XLBOBB_MASK, PWRCOM,	{ BI } },
{ "bflrl",   XLO(19,BOF,16,1), XLBOBB_MASK, PPCCOM,	{ BI } },
{ "bflrl-",  XLO(19,BOF,16,1), XLBOBB_MASK, PPC,	{ BI } },
{ "bflrl+",  XLO(19,BOFP,16,1), XLBOBB_MASK, PPC,	{ BI } },
{ "bbfrl",   XLO(19,BOF,16,1), XLBOBB_MASK, PWRCOM,	{ BI } },
{ "bdnztlr", XLO(19,BODNZT,16,0), XLBOBB_MASK, PPCCOM,	{ BI } },
{ "bdnztlr-",XLO(19,BODNZT,16,0), XLBOBB_MASK, PPC,	{ BI } },
{ "bdnztlr+",XLO(19,BODNZTP,16,0), XLBOBB_MASK, PPC,	{ BI } },
{ "bdnztlrl",XLO(19,BODNZT,16,1), XLBOBB_MASK, PPCCOM,	{ BI } },
{ "bdnztlrl-",XLO(19,BODNZT,16,1), XLBOBB_MASK, PPC,	{ BI } },
{ "bdnztlrl+",XLO(19,BODNZTP,16,1), XLBOBB_MASK, PPC,	{ BI } },
{ "bdnzflr", XLO(19,BODNZF,16,0), XLBOBB_MASK, PPCCOM,	{ BI } },
{ "bdnzflr-",XLO(19,BODNZF,16,0), XLBOBB_MASK, PPC,	{ BI } },
{ "bdnzflr+",XLO(19,BODNZFP,16,0), XLBOBB_MASK, PPC,	{ BI } },
{ "bdnzflrl",XLO(19,BODNZF,16,1), XLBOBB_MASK, PPCCOM,	{ BI } },
{ "bdnzflrl-",XLO(19,BODNZF,16,1), XLBOBB_MASK, PPC,	{ BI } },
{ "bdnzflrl+",XLO(19,BODNZFP,16,1), XLBOBB_MASK, PPC,	{ BI } },
{ "bdztlr",  XLO(19,BODZT,16,0), XLBOBB_MASK, PPCCOM,	{ BI } },
{ "bdztlr-", XLO(19,BODZT,16,0), XLBOBB_MASK, PPC,	{ BI } },
{ "bdztlr+", XLO(19,BODZTP,16,0), XLBOBB_MASK, PPC,	{ BI } },
{ "bdztlrl", XLO(19,BODZT,16,1), XLBOBB_MASK, PPCCOM,	{ BI } },
{ "bdztlrl-",XLO(19,BODZT,16,1), XLBOBB_MASK, PPC,	{ BI } },
{ "bdztlrl+",XLO(19,BODZTP,16,1), XLBOBB_MASK, PPC,	{ BI } },
{ "bdzflr",  XLO(19,BODZF,16,0), XLBOBB_MASK, PPCCOM,	{ BI } },
{ "bdzflr-", XLO(19,BODZF,16,0), XLBOBB_MASK, PPC,	{ BI } },
{ "bdzflr+", XLO(19,BODZFP,16,0), XLBOBB_MASK, PPC,	{ BI } },
{ "bdzflrl", XLO(19,BODZF,16,1), XLBOBB_MASK, PPCCOM,	{ BI } },
{ "bdzflrl-",XLO(19,BODZF,16,1), XLBOBB_MASK, PPC,	{ BI } },
{ "bdzflrl+",XLO(19,BODZFP,16,1), XLBOBB_MASK, PPC,	{ BI } },
{ "bclr",    XLLK(19,16,0), XLYBB_MASK,	PPCCOM,		{ BO, BI } },
{ "bclrl",   XLLK(19,16,1), XLYBB_MASK,	PPCCOM,		{ BO, BI } },
{ "bclr+",   XLYLK(19,16,1,0), XLYBB_MASK, PPC,		{ BOE, BI } },
{ "bclrl+",  XLYLK(19,16,1,1), XLYBB_MASK, PPC,		{ BOE, BI } },
{ "bclr-",   XLYLK(19,16,0,0), XLYBB_MASK, PPC,		{ BOE, BI } },
{ "bclrl-",  XLYLK(19,16,0,1), XLYBB_MASK, PPC,		{ BOE, BI } },
{ "bcr",     XLLK(19,16,0), XLBB_MASK,	PWRCOM,		{ BO, BI } },
{ "bcrl",    XLLK(19,16,1), XLBB_MASK,	PWRCOM,		{ BO, BI } },

{ "crnot",   XL(19,33), XL_MASK,	PPCCOM,		{ BT, BA, BBA } },
{ "crnor",   XL(19,33),	XL_MASK,	COM,		{ BT, BA, BB } },

{ "rfi",     XL(19,50),	0xffffffff,	COM,		{ 0 } },
{ "rfci",    XL(19,51),	0xffffffff,	PPC,		{ 0 } },

{ "rfsvc",   XL(19,82),	0xffffffff,	POWER,		{ 0 } },

{ "crandc",  XL(19,129), XL_MASK,	COM,		{ BT, BA, BB } },

{ "isync",   XL(19,150), 0xffffffff,	PPCCOM,		{ 0 } },
{ "ics",     XL(19,150), 0xffffffff,	PWRCOM,		{ 0 } },

{ "crclr",   XL(19,193), XL_MASK,	PPCCOM,		{ BT, BAT, BBA } },
{ "crxor",   XL(19,193), XL_MASK,	COM,		{ BT, BA, BB } },

{ "crnand",  XL(19,225), XL_MASK,	COM,		{ BT, BA, BB } },

{ "crand",   XL(19,257), XL_MASK,	COM,		{ BT, BA, BB } },

{ "crset",   XL(19,289), XL_MASK,	PPCCOM,		{ BT, BAT, BBA } },
{ "creqv",   XL(19,289), XL_MASK,	COM,		{ BT, BA, BB } },

{ "crorc",   XL(19,417), XL_MASK,	COM,		{ BT, BA, BB } },

{ "crmove",  XL(19,449), XL_MASK,	PPCCOM,		{ BT, BA, BBA } },
{ "cror",    XL(19,449), XL_MASK,	COM,		{ BT, BA, BB } },

{ "bctr",    XLO(19,BOU,528,0), XLBOBIBB_MASK, COM,	{ 0 } },
{ "bctrl",   XLO(19,BOU,528,1), XLBOBIBB_MASK, COM,	{ 0 } },
{ "bltctr",  XLOCB(19,BOT,CBLT,528,0),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bltctr-", XLOCB(19,BOT,CBLT,528,0),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bltctr+", XLOCB(19,BOTP,CBLT,528,0), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bltctrl", XLOCB(19,BOT,CBLT,528,1),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bltctrl-",XLOCB(19,BOT,CBLT,528,1),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bltctrl+",XLOCB(19,BOTP,CBLT,528,1), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bgtctr",  XLOCB(19,BOT,CBGT,528,0),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bgtctr-", XLOCB(19,BOT,CBGT,528,0),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bgtctr+", XLOCB(19,BOTP,CBGT,528,0), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bgtctrl", XLOCB(19,BOT,CBGT,528,1),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bgtctrl-",XLOCB(19,BOT,CBGT,528,1),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bgtctrl+",XLOCB(19,BOTP,CBGT,528,1), XLBOCBBB_MASK, PPC,	{ CR } },
{ "beqctr",  XLOCB(19,BOT,CBEQ,528,0),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "beqctr-", XLOCB(19,BOT,CBEQ,528,0),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "beqctr+", XLOCB(19,BOTP,CBEQ,528,0), XLBOCBBB_MASK, PPC,	{ CR } },
{ "beqctrl", XLOCB(19,BOT,CBEQ,528,1),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "beqctrl-",XLOCB(19,BOT,CBEQ,528,1),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "beqctrl+",XLOCB(19,BOTP,CBEQ,528,1), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bsoctr",  XLOCB(19,BOT,CBSO,528,0),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bsoctr-", XLOCB(19,BOT,CBSO,528,0),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bsoctr+", XLOCB(19,BOTP,CBSO,528,0), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bsoctrl", XLOCB(19,BOT,CBSO,528,1),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bsoctrl-",XLOCB(19,BOT,CBSO,528,1),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bsoctrl+",XLOCB(19,BOTP,CBSO,528,1), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bunctr",  XLOCB(19,BOT,CBSO,528,0),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bunctr-", XLOCB(19,BOT,CBSO,528,0),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bunctr+", XLOCB(19,BOTP,CBSO,528,0), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bunctrl", XLOCB(19,BOT,CBSO,528,1),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bunctrl-",XLOCB(19,BOT,CBSO,528,1),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bunctrl+",XLOCB(19,BOTP,CBSO,528,1), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bgectr",  XLOCB(19,BOF,CBLT,528,0),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bgectr-", XLOCB(19,BOF,CBLT,528,0),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bgectr+", XLOCB(19,BOFP,CBLT,528,0), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bgectrl", XLOCB(19,BOF,CBLT,528,1),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bgectrl-",XLOCB(19,BOF,CBLT,528,1),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bgectrl+",XLOCB(19,BOFP,CBLT,528,1), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bnlctr",  XLOCB(19,BOF,CBLT,528,0),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bnlctr-", XLOCB(19,BOF,CBLT,528,0),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bnlctr+", XLOCB(19,BOFP,CBLT,528,0), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bnlctrl", XLOCB(19,BOF,CBLT,528,1),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bnlctrl-",XLOCB(19,BOF,CBLT,528,1),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bnlctrl+",XLOCB(19,BOFP,CBLT,528,1), XLBOCBBB_MASK, PPC,	{ CR } },
{ "blectr",  XLOCB(19,BOF,CBGT,528,0),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "blectr-", XLOCB(19,BOF,CBGT,528,0),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "blectr+", XLOCB(19,BOFP,CBGT,528,0), XLBOCBBB_MASK, PPC,	{ CR } },
{ "blectrl", XLOCB(19,BOF,CBGT,528,1),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "blectrl-",XLOCB(19,BOF,CBGT,528,1),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "blectrl+",XLOCB(19,BOFP,CBGT,528,1), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bngctr",  XLOCB(19,BOF,CBGT,528,0),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bngctr-", XLOCB(19,BOF,CBGT,528,0),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bngctr+", XLOCB(19,BOFP,CBGT,528,0), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bngctrl", XLOCB(19,BOF,CBGT,528,1),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bngctrl-",XLOCB(19,BOF,CBGT,528,1),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bngctrl+",XLOCB(19,BOFP,CBGT,528,1), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bnectr",  XLOCB(19,BOF,CBEQ,528,0),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bnectr-", XLOCB(19,BOF,CBEQ,528,0),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bnectr+", XLOCB(19,BOFP,CBEQ,528,0), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bnectrl", XLOCB(19,BOF,CBEQ,528,1),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bnectrl-",XLOCB(19,BOF,CBEQ,528,1),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bnectrl+",XLOCB(19,BOFP,CBEQ,528,1), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bnsctr",  XLOCB(19,BOF,CBSO,528,0),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bnsctr-", XLOCB(19,BOF,CBSO,528,0),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bnsctr+", XLOCB(19,BOFP,CBSO,528,0), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bnsctrl", XLOCB(19,BOF,CBSO,528,1),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bnsctrl-",XLOCB(19,BOF,CBSO,528,1),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bnsctrl+",XLOCB(19,BOFP,CBSO,528,1), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bnuctr",  XLOCB(19,BOF,CBSO,528,0),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bnuctr-", XLOCB(19,BOF,CBSO,528,0),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bnuctr+", XLOCB(19,BOFP,CBSO,528,0), XLBOCBBB_MASK, PPC,	{ CR } },
{ "bnuctrl", XLOCB(19,BOF,CBSO,528,1),  XLBOCBBB_MASK, PPCCOM,	{ CR } },
{ "bnuctrl-",XLOCB(19,BOF,CBSO,528,1),  XLBOCBBB_MASK, PPC,	{ CR } },
{ "bnuctrl+",XLOCB(19,BOFP,CBSO,528,1), XLBOCBBB_MASK, PPC,	{ CR } },
{ "btctr",   XLO(19,BOT,528,0),  XLBOBB_MASK, PPCCOM,	{ BI } },
{ "btctr-",  XLO(19,BOT,528,0),  XLBOBB_MASK, PPC,	{ BI } },
{ "btctr+",  XLO(19,BOTP,528,0), XLBOBB_MASK, PPC,	{ BI } },
{ "btctrl",  XLO(19,BOT,528,1),  XLBOBB_MASK, PPCCOM,	{ BI } },
{ "btctrl-", XLO(19,BOT,528,1),  XLBOBB_MASK, PPC,	{ BI } },
{ "btctrl+", XLO(19,BOTP,528,1), XLBOBB_MASK, PPC,	{ BI } },
{ "bfctr",   XLO(19,BOF,528,0),  XLBOBB_MASK, PPCCOM,	{ BI } },
{ "bfctr-",  XLO(19,BOF,528,0),  XLBOBB_MASK, PPC,	{ BI } },
{ "bfctr+",  XLO(19,BOFP,528,0), XLBOBB_MASK, PPC,	{ BI } },
{ "bfctrl",  XLO(19,BOF,528,1),  XLBOBB_MASK, PPCCOM,	{ BI } },
{ "bfctrl-", XLO(19,BOF,528,1),  XLBOBB_MASK, PPC,	{ BI } },
{ "bfctrl+", XLO(19,BOFP,528,1), XLBOBB_MASK, PPC,	{ BI } },
{ "bcctr",   XLLK(19,528,0),     XLYBB_MASK,  PPCCOM,	{ BO, BI } },
{ "bcctr-",  XLYLK(19,528,0,0),  XLYBB_MASK,  PPC,	{ BOE, BI } },
{ "bcctr+",  XLYLK(19,528,1,0),  XLYBB_MASK,  PPC,	{ BOE, BI } },
{ "bcctrl",  XLLK(19,528,1),     XLYBB_MASK,  PPCCOM,	{ BO, BI } },
{ "bcctrl-", XLYLK(19,528,0,1),  XLYBB_MASK,  PPC,	{ BOE, BI } },
{ "bcctrl+", XLYLK(19,528,1,1),  XLYBB_MASK,  PPC,	{ BOE, BI } },
{ "bcc",     XLLK(19,528,0),     XLBB_MASK,   PWRCOM,	{ BO, BI } },
{ "bccl",    XLLK(19,528,1),     XLBB_MASK,   PWRCOM,	{ BO, BI } },

{ "rlwimi",  M(20,0),	M_MASK,		PPCCOM,		{ RA,RS,SH,MBE,ME } },
{ "rlimi",   M(20,0),	M_MASK,		PWRCOM,		{ RA,RS,SH,MBE,ME } },

{ "rlwimi.", M(20,1),	M_MASK,		PPCCOM,		{ RA,RS,SH,MBE,ME } },
{ "rlimi.",  M(20,1),	M_MASK,		PWRCOM,		{ RA,RS,SH,MBE,ME } },

{ "rotlwi",  MME(21,31,0), MMBME_MASK,	PPCCOM,		{ RA, RS, SH } },
{ "clrlwi",  MME(21,31,0), MSHME_MASK,	PPCCOM,		{ RA, RS, MB } },
{ "rlwinm",  M(21,0),	M_MASK,		PPCCOM,		{ RA,RS,SH,MBE,ME } },
{ "rlinm",   M(21,0),	M_MASK,		PWRCOM,		{ RA,RS,SH,MBE,ME } },
{ "rotlwi.", MME(21,31,1), MMBME_MASK,	PPCCOM,		{ RA,RS,SH } },
{ "clrlwi.", MME(21,31,1), MSHME_MASK,	PPCCOM,		{ RA, RS, MB } },
{ "rlwinm.", M(21,1),	M_MASK,		PPCCOM,		{ RA,RS,SH,MBE,ME } },
{ "rlinm.",  M(21,1),	M_MASK,		PWRCOM,		{ RA,RS,SH,MBE,ME } },

{ "rlmi",    M(22,0),	M_MASK,		M601,		{ RA,RS,RB,MBE,ME } },
{ "rlmi.",   M(22,1),	M_MASK,		M601,		{ RA,RS,RB,MBE,ME } },

{ "rotlw",   MME(23,31,0), MMBME_MASK,	PPCCOM,		{ RA, RS, RB } },
{ "rlwnm",   M(23,0),	M_MASK,		PPCCOM,		{ RA,RS,RB,MBE,ME } },
{ "rlnm",    M(23,0),	M_MASK,		PWRCOM,		{ RA,RS,RB,MBE,ME } },
{ "rotlw.",  MME(23,31,1), MMBME_MASK,	PPCCOM,		{ RA, RS, RB } },
{ "rlwnm.",  M(23,1),	M_MASK,		PPCCOM,		{ RA,RS,RB,MBE,ME } },
{ "rlnm.",   M(23,1),	M_MASK,		PWRCOM,		{ RA,RS,RB,MBE,ME } },

{ "nop",     OP(24),	0xffffffff,	PPCCOM,		{ 0 } },
{ "ori",     OP(24),	OP_MASK,	PPCCOM,		{ RA, RS, UI } },
{ "oril",    OP(24),	OP_MASK,	PWRCOM,		{ RA, RS, UI } },

{ "oris",    OP(25),	OP_MASK,	PPCCOM,		{ RA, RS, UI } },
{ "oriu",    OP(25),	OP_MASK,	PWRCOM,		{ RA, RS, UI } },

{ "xori",    OP(26),	OP_MASK,	PPCCOM,		{ RA, RS, UI } },
{ "xoril",   OP(26),	OP_MASK,	PWRCOM,		{ RA, RS, UI } },

{ "xoris",   OP(27),	OP_MASK,	PPCCOM,		{ RA, RS, UI } },
{ "xoriu",   OP(27),	OP_MASK,	PWRCOM,		{ RA, RS, UI } },

{ "andi.",   OP(28),	OP_MASK,	PPCCOM,		{ RA, RS, UI } },
{ "andil.",  OP(28),	OP_MASK,	PWRCOM,		{ RA, RS, UI } },

{ "andis.",  OP(29),	OP_MASK,	PPCCOM,		{ RA, RS, UI } },
{ "andiu.",  OP(29),	OP_MASK,	PWRCOM,		{ RA, RS, UI } },

{ "rotldi",  MD(30,0,0), MDMB_MASK,	PPC64,		{ RA, RS, SH6 } },
{ "clrldi",  MD(30,0,0), MDSH_MASK,	PPC64,		{ RA, RS, MB6 } },
{ "rldicl",  MD(30,0,0), MD_MASK,	PPC64,		{ RA, RS, SH6, MB6 } },
{ "rotldi.", MD(30,0,1), MDMB_MASK,	PPC64,		{ RA, RS, SH6 } },
{ "clrldi.", MD(30,0,1), MDSH_MASK,	PPC64,		{ RA, RS, MB6 } },
{ "rldicl.", MD(30,0,1), MD_MASK,	PPC64,		{ RA, RS, SH6, MB6 } },

{ "rldicr",  MD(30,1,0), MD_MASK,	PPC64,		{ RA, RS, SH6, ME6 } },
{ "rldicr.", MD(30,1,1), MD_MASK,	PPC64,		{ RA, RS, SH6, ME6 } },

{ "rldic",   MD(30,2,0), MD_MASK,	PPC64,		{ RA, RS, SH6, MB6 } },
{ "rldic.",  MD(30,2,1), MD_MASK,	PPC64,		{ RA, RS, SH6, MB6 } },

{ "rldimi",  MD(30,3,0), MD_MASK,	PPC64,		{ RA, RS, SH6, MB6 } },
{ "rldimi.", MD(30,3,1), MD_MASK,	PPC64,		{ RA, RS, SH6, MB6 } },

{ "rotld",   MDS(30,8,0), MDSMB_MASK,	PPC64,		{ RA, RS, RB } },
{ "rldcl",   MDS(30,8,0), MDS_MASK,	PPC64,		{ RA, RS, RB, MB6 } },
{ "rotld.",  MDS(30,8,1), MDSMB_MASK,	PPC64,		{ RA, RS, RB } },
{ "rldcl.",  MDS(30,8,1), MDS_MASK,	PPC64,		{ RA, RS, RB, MB6 } },

{ "rldcr",   MDS(30,9,0), MDS_MASK,	PPC64,		{ RA, RS, RB, ME6 } },
{ "rldcr.",  MDS(30,9,1), MDS_MASK,	PPC64,		{ RA, RS, RB, ME6 } },

{ "cmpw",    XCMPL(31,0,0), XCMPL_MASK, PPCCOM,		{ OBF, RA, RB } },
{ "cmpd",    XCMPL(31,0,1), XCMPL_MASK, PPC64,		{ OBF, RA, RB } },
{ "cmp",     X(31,0),	XCMP_MASK,	PPCONLY,	{ BF, L, RA, RB } },
{ "cmp",     X(31,0),	XCMPL_MASK,	PWRCOM,		{ BF, RA, RB } },

{ "twlgt",   XTO(31,4,TOLGT), XTO_MASK, PPCCOM,		{ RA, RB } },
{ "tlgt",    XTO(31,4,TOLGT), XTO_MASK, PWRCOM,		{ RA, RB } },
{ "twllt",   XTO(31,4,TOLLT), XTO_MASK, PPCCOM,		{ RA, RB } },
{ "tllt",    XTO(31,4,TOLLT), XTO_MASK, PWRCOM,		{ RA, RB } },
{ "tweq",    XTO(31,4,TOEQ), XTO_MASK,	PPCCOM,		{ RA, RB } },
{ "teq",     XTO(31,4,TOEQ), XTO_MASK,	PWRCOM,		{ RA, RB } },
{ "twlge",   XTO(31,4,TOLGE), XTO_MASK, PPCCOM,		{ RA, RB } },
{ "tlge",    XTO(31,4,TOLGE), XTO_MASK, PWRCOM,		{ RA, RB } },
{ "twlnl",   XTO(31,4,TOLNL), XTO_MASK, PPCCOM,		{ RA, RB } },
{ "tlnl",    XTO(31,4,TOLNL), XTO_MASK, PWRCOM,		{ RA, RB } },
{ "twlle",   XTO(31,4,TOLLE), XTO_MASK, PPCCOM,		{ RA, RB } },
{ "tlle",    XTO(31,4,TOLLE), XTO_MASK, PWRCOM,		{ RA, RB } },
{ "twlng",   XTO(31,4,TOLNG), XTO_MASK, PPCCOM,		{ RA, RB } },
{ "tlng",    XTO(31,4,TOLNG), XTO_MASK, PWRCOM,		{ RA, RB } },
{ "twgt",    XTO(31,4,TOGT), XTO_MASK,	PPCCOM,		{ RA, RB } },
{ "tgt",     XTO(31,4,TOGT), XTO_MASK,	PWRCOM,		{ RA, RB } },
{ "twge",    XTO(31,4,TOGE), XTO_MASK,	PPCCOM,		{ RA, RB } },
{ "tge",     XTO(31,4,TOGE), XTO_MASK,	PWRCOM,		{ RA, RB } },
{ "twnl",    XTO(31,4,TONL), XTO_MASK,	PPCCOM,		{ RA, RB } },
{ "tnl",     XTO(31,4,TONL), XTO_MASK,	PWRCOM,		{ RA, RB } },
{ "twlt",    XTO(31,4,TOLT), XTO_MASK,	PPCCOM,		{ RA, RB } },
{ "tlt",     XTO(31,4,TOLT), XTO_MASK,	PWRCOM,		{ RA, RB } },
{ "twle",    XTO(31,4,TOLE), XTO_MASK,	PPCCOM,		{ RA, RB } },
{ "tle",     XTO(31,4,TOLE), XTO_MASK,	PWRCOM,		{ RA, RB } },
{ "twng",    XTO(31,4,TONG), XTO_MASK,	PPCCOM,		{ RA, RB } },
{ "tng",     XTO(31,4,TONG), XTO_MASK,	PWRCOM,		{ RA, RB } },
{ "twne",    XTO(31,4,TONE), XTO_MASK,	PPCCOM,		{ RA, RB } },
{ "tne",     XTO(31,4,TONE), XTO_MASK,	PWRCOM,		{ RA, RB } },
{ "trap",    XTO(31,4,TOU), 0xffffffff,	PPCCOM,		{ 0 } },
{ "tw",      X(31,4),	X_MASK,		PPCCOM,		{ TO, RA, RB } },
{ "t",       X(31,4),	X_MASK,		PWRCOM,		{ TO, RA, RB } },

{ "subfc",   XO(31,8,0,0), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "sf",      XO(31,8,0,0), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "subc",    XO(31,8,0,0), XO_MASK,	PPC,		{ RT, RB, RA } },
{ "subfc.",  XO(31,8,0,1), XO_MASK,	PPC,		{ RT, RA, RB } },
{ "sf.",     XO(31,8,0,1), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "subc.",   XO(31,8,0,1), XO_MASK,	PPCCOM,		{ RT, RB, RA } },
{ "subfco",  XO(31,8,1,0), XO_MASK,	PPC,		{ RT, RA, RB } },
{ "sfo",     XO(31,8,1,0), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "subco",   XO(31,8,1,0), XO_MASK,	PPCCOM,		{ RT, RB, RA } },
{ "subfco.", XO(31,8,1,1), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "sfo.",    XO(31,8,1,1), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "subco.",  XO(31,8,1,1), XO_MASK,	PPCCOM,		{ RT, RB, RA } },

{ "mulhdu",  XO(31,9,0,0), XO_MASK,	PPC64,		{ RT, RA, RB } },
{ "mulhdu.", XO(31,9,0,1), XO_MASK,	PPC64,		{ RT, RA, RB } },

{ "addc",    XO(31,10,0,0), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "a",       XO(31,10,0,0), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "addc.",   XO(31,10,0,1), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "a.",      XO(31,10,0,1), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "addco",   XO(31,10,1,0), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "ao",      XO(31,10,1,0), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "addco.",  XO(31,10,1,1), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "ao.",     XO(31,10,1,1), XO_MASK,	PWRCOM,		{ RT, RA, RB } },

{ "mulhwu",  XO(31,11,0,0), XO_MASK,	PPC,		{ RT, RA, RB } },
{ "mulhwu.", XO(31,11,0,1), XO_MASK,	PPC,		{ RT, RA, RB } },

{ "mfcr",    X(31,19),	XRARB_MASK,	COM,		{ RT } },

{ "lwarx",   X(31,20),	X_MASK,		PPC,		{ RT, RA, RB } },

{ "ldx",     X(31,21),	X_MASK,		PPC64,		{ RT, RA, RB } },

{ "lwzx",    X(31,23),	X_MASK,		PPCCOM,		{ RT, RA, RB } },
{ "lx",      X(31,23),	X_MASK,		PWRCOM,		{ RT, RA, RB } },

{ "slw",     XRC(31,24,0), X_MASK,	PPCCOM,		{ RA, RS, RB } },
{ "sl",      XRC(31,24,0), X_MASK,	PWRCOM,		{ RA, RS, RB } },
{ "slw.",    XRC(31,24,1), X_MASK,	PPCCOM,		{ RA, RS, RB } },
{ "sl.",     XRC(31,24,1), X_MASK,	PWRCOM,		{ RA, RS, RB } },

{ "cntlzw",  XRC(31,26,0), XRB_MASK,	PPCCOM,		{ RA, RS } },
{ "cntlz",   XRC(31,26,0), XRB_MASK,	PWRCOM,		{ RA, RS } },
{ "cntlzw.", XRC(31,26,1), XRB_MASK,	PPCCOM,		{ RA, RS } },
{ "cntlz.",  XRC(31,26,1), XRB_MASK, 	PWRCOM,		{ RA, RS } },

{ "sld",     XRC(31,27,0), X_MASK,	PPC64,		{ RA, RS, RB } },
{ "sld.",    XRC(31,27,1), X_MASK,	PPC64,		{ RA, RS, RB } },

{ "and",     XRC(31,28,0), X_MASK,	COM,		{ RA, RS, RB } },
{ "and.",    XRC(31,28,1), X_MASK,	COM,		{ RA, RS, RB } },

{ "maskg",   XRC(31,29,0), X_MASK,	M601,		{ RA, RS, RB } },
{ "maskg.",  XRC(31,29,1), X_MASK,	M601,		{ RA, RS, RB } },

{ "cmplw",   XCMPL(31,32,0), XCMPL_MASK, PPCCOM,	{ OBF, RA, RB } },
{ "cmpld",   XCMPL(31,32,1), XCMPL_MASK, PPC64,		{ OBF, RA, RB } },
{ "cmpl",    X(31,32),	XCMP_MASK,	 PPCONLY,	{ BF, L, RA, RB } },
{ "cmpl",    X(31,32),	XCMPL_MASK,	 PWRCOM,	{ BF, RA, RB } },

{ "subf",    XO(31,40,0,0), XO_MASK,	PPC,		{ RT, RA, RB } },
{ "sub",     XO(31,40,0,0), XO_MASK,	PPC,		{ RT, RB, RA } },
{ "subf.",   XO(31,40,0,1), XO_MASK,	PPC,		{ RT, RA, RB } },
{ "sub.",    XO(31,40,0,1), XO_MASK,	PPC,		{ RT, RB, RA } },
{ "subfo",   XO(31,40,1,0), XO_MASK,	PPC,		{ RT, RA, RB } },
{ "subo",    XO(31,40,1,0), XO_MASK,	PPC,		{ RT, RB, RA } },
{ "subfo.",  XO(31,40,1,1), XO_MASK,	PPC,		{ RT, RA, RB } },
{ "subo.",   XO(31,40,1,1), XO_MASK,	PPC,		{ RT, RB, RA } },

{ "ldux",    X(31,53),	X_MASK,		PPC64,		{ RT, RAL, RB } },

{ "dcbst",   X(31,54),	XRT_MASK,	PPC,		{ RA, RB } },

{ "lwzux",   X(31,55),	X_MASK,		PPCCOM,		{ RT, RAL, RB } },
{ "lux",     X(31,55),	X_MASK,		PWRCOM,		{ RT, RA, RB } },

{ "cntlzd",  XRC(31,58,0), XRB_MASK,	PPC64,		{ RA, RS } },
{ "cntlzd.", XRC(31,58,1), XRB_MASK,	PPC64,		{ RA, RS } },

{ "andc",    XRC(31,60,0), X_MASK,	COM,	{ RA, RS, RB } },
{ "andc.",   XRC(31,60,1), X_MASK,	COM,	{ RA, RS, RB } },

{ "tdlgt",   XTO(31,68,TOLGT), XTO_MASK, PPC64,		{ RA, RB } },
{ "tdllt",   XTO(31,68,TOLLT), XTO_MASK, PPC64,		{ RA, RB } },
{ "tdeq",    XTO(31,68,TOEQ), XTO_MASK,  PPC64,		{ RA, RB } },
{ "tdlge",   XTO(31,68,TOLGE), XTO_MASK, PPC64,		{ RA, RB } },
{ "tdlnl",   XTO(31,68,TOLNL), XTO_MASK, PPC64,		{ RA, RB } },
{ "tdlle",   XTO(31,68,TOLLE), XTO_MASK, PPC64,		{ RA, RB } },
{ "tdlng",   XTO(31,68,TOLNG), XTO_MASK, PPC64,		{ RA, RB } },
{ "tdgt",    XTO(31,68,TOGT), XTO_MASK,  PPC64,		{ RA, RB } },
{ "tdge",    XTO(31,68,TOGE), XTO_MASK,  PPC64,		{ RA, RB } },
{ "tdnl",    XTO(31,68,TONL), XTO_MASK,  PPC64,		{ RA, RB } },
{ "tdlt",    XTO(31,68,TOLT), XTO_MASK,  PPC64,		{ RA, RB } },
{ "tdle",    XTO(31,68,TOLE), XTO_MASK,  PPC64,		{ RA, RB } },
{ "tdng",    XTO(31,68,TONG), XTO_MASK,  PPC64,		{ RA, RB } },
{ "tdne",    XTO(31,68,TONE), XTO_MASK,  PPC64,		{ RA, RB } },
{ "td",	     X(31,68),	X_MASK,		 PPC64,		{ TO, RA, RB } },

{ "mulhd",   XO(31,73,0,0), XO_MASK,	 PPC64,		{ RT, RA, RB } },
{ "mulhd.",  XO(31,73,0,1), XO_MASK,	 PPC64,		{ RT, RA, RB } },

{ "mulhw",   XO(31,75,0,0), XO_MASK,	PPC,		{ RT, RA, RB } },
{ "mulhw.",  XO(31,75,0,1), XO_MASK,	PPC,		{ RT, RA, RB } },

{ "mfmsr",   X(31,83),	XRARB_MASK,	COM,		{ RT } },

{ "ldarx",   X(31,84),	X_MASK,		PPC64,		{ RT, RA, RB } },

{ "dcbf",    X(31,86),	XRT_MASK,	PPC,		{ RA, RB } },

{ "lbzx",    X(31,87),	X_MASK,		COM,		{ RT, RA, RB } },

{ "neg",     XO(31,104,0,0), XORB_MASK,	COM,		{ RT, RA } },
{ "neg.",    XO(31,104,0,1), XORB_MASK,	COM,		{ RT, RA } },
{ "nego",    XO(31,104,1,0), XORB_MASK,	COM,		{ RT, RA } },
{ "nego.",   XO(31,104,1,1), XORB_MASK,	COM,		{ RT, RA } },

{ "mul",     XO(31,107,0,0), XO_MASK,	M601,		{ RT, RA, RB } },
{ "mul.",    XO(31,107,0,1), XO_MASK,	M601,		{ RT, RA, RB } },
{ "mulo",    XO(31,107,1,0), XO_MASK,	M601,		{ RT, RA, RB } },
{ "mulo.",   XO(31,107,1,1), XO_MASK,	M601,		{ RT, RA, RB } },

{ "clf",     X(31,118), XRB_MASK,	POWER,		{ RT, RA } },

{ "lbzux",   X(31,119),	X_MASK,		COM,		{ RT, RAL, RB } },

{ "not",     XRC(31,124,0), X_MASK,	COM,		{ RA, RS, RBS } },
{ "nor",     XRC(31,124,0), X_MASK,	COM,		{ RA, RS, RB } },
{ "not.",    XRC(31,124,1), X_MASK,	COM,		{ RA, RS, RBS } },
{ "nor.",    XRC(31,124,1), X_MASK,	COM,		{ RA, RS, RB } },

{ "subfe",   XO(31,136,0,0), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "sfe",     XO(31,136,0,0), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "subfe.",  XO(31,136,0,1), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "sfe.",    XO(31,136,0,1), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "subfeo",  XO(31,136,1,0), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "sfeo",    XO(31,136,1,0), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "subfeo.", XO(31,136,1,1), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "sfeo.",   XO(31,136,1,1), XO_MASK,	PWRCOM,		{ RT, RA, RB } },

{ "adde",    XO(31,138,0,0), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "ae",      XO(31,138,0,0), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "adde.",   XO(31,138,0,1), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "ae.",     XO(31,138,0,1), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "addeo",   XO(31,138,1,0), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "aeo",     XO(31,138,1,0), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "addeo.",  XO(31,138,1,1), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "aeo.",    XO(31,138,1,1), XO_MASK,	PWRCOM,		{ RT, RA, RB } },

{ "mtcr",    XFXM(31,144,0xff), XFXFXM_MASK|FXM_MASK, COM,	{ RS }},
{ "mtcrf",   X(31,144),	XFXFXM_MASK,	COM,		{ FXM, RS } },

{ "mtmsr",   X(31,146),	XRARB_MASK,	COM,		{ RS } },

{ "stdx",    X(31,149), X_MASK,		PPC64,		{ RS, RA, RB } },

{ "stwcx.",  XRC(31,150,1), X_MASK,	PPC,		{ RS, RA, RB } },

{ "stwx",    X(31,151), X_MASK,		PPCCOM,		{ RS, RA, RB } },
{ "stx",     X(31,151), X_MASK,		PWRCOM,		{ RS, RA, RB } },

{ "slq",     XRC(31,152,0), X_MASK,	M601,		{ RA, RS, RB } },
{ "slq.",    XRC(31,152,1), X_MASK,	M601,		{ RA, RS, RB } },

{ "sle",     XRC(31,153,0), X_MASK,	M601,		{ RA, RS, RB } },
{ "sle.",    XRC(31,153,1), X_MASK,	M601,		{ RA, RS, RB } },

{ "stdux",   X(31,181),	X_MASK,		PPC64,		{ RS, RAS, RB } },

{ "stwux",   X(31,183),	X_MASK,		PPCCOM,		{ RS, RAS, RB } },
{ "stux",    X(31,183),	X_MASK,		PWRCOM,		{ RS, RA, RB } },

{ "sliq",    XRC(31,184,0), X_MASK,	M601,		{ RA, RS, SH } },
{ "sliq.",   XRC(31,184,1), X_MASK,	M601,		{ RA, RS, SH } },

{ "subfze",  XO(31,200,0,0), XORB_MASK, PPCCOM,		{ RT, RA } },
{ "sfze",    XO(31,200,0,0), XORB_MASK, PWRCOM,		{ RT, RA } },
{ "subfze.", XO(31,200,0,1), XORB_MASK, PPCCOM,		{ RT, RA } },
{ "sfze.",   XO(31,200,0,1), XORB_MASK, PWRCOM,		{ RT, RA } },
{ "subfzeo", XO(31,200,1,0), XORB_MASK, PPCCOM,		{ RT, RA } },
{ "sfzeo",   XO(31,200,1,0), XORB_MASK, PWRCOM,		{ RT, RA } },
{ "subfzeo.",XO(31,200,1,1), XORB_MASK, PPCCOM,		{ RT, RA } },
{ "sfzeo.",  XO(31,200,1,1), XORB_MASK, PWRCOM,		{ RT, RA } },

{ "addze",   XO(31,202,0,0), XORB_MASK, PPCCOM,		{ RT, RA } },
{ "aze",     XO(31,202,0,0), XORB_MASK, PWRCOM,		{ RT, RA } },
{ "addze.",  XO(31,202,0,1), XORB_MASK, PPCCOM,		{ RT, RA } },
{ "aze.",    XO(31,202,0,1), XORB_MASK, PWRCOM,		{ RT, RA } },
{ "addzeo",  XO(31,202,1,0), XORB_MASK, PPCCOM,		{ RT, RA } },
{ "azeo",    XO(31,202,1,0), XORB_MASK, PWRCOM,		{ RT, RA } },
{ "addzeo.", XO(31,202,1,1), XORB_MASK, PPCCOM,		{ RT, RA } },
{ "azeo.",   XO(31,202,1,1), XORB_MASK, PWRCOM,		{ RT, RA } },

{ "mtsr",    X(31,210),	XRB_MASK|(1<<20), COM32,	{ SR, RS } },

{ "stdcx.",  XRC(31,214,1), X_MASK,	PPC64,		{ RS, RA, RB } },

{ "stbx",    X(31,215),	X_MASK,		COM,	{ RS, RA, RB } },

{ "sllq",    XRC(31,216,0), X_MASK,	M601,		{ RA, RS, RB } },
{ "sllq.",   XRC(31,216,1), X_MASK,	M601,		{ RA, RS, RB } },

{ "sleq",    XRC(31,217,0), X_MASK,	M601,		{ RA, RS, RB } },
{ "sleq.",   XRC(31,217,1), X_MASK,	M601,		{ RA, RS, RB } },

{ "subfme",  XO(31,232,0,0), XORB_MASK, PPCCOM,		{ RT, RA } },
{ "sfme",    XO(31,232,0,0), XORB_MASK, PWRCOM,		{ RT, RA } },
{ "subfme.", XO(31,232,0,1), XORB_MASK, PPCCOM,		{ RT, RA } },
{ "sfme.",   XO(31,232,0,1), XORB_MASK, PWRCOM,		{ RT, RA } },
{ "subfmeo", XO(31,232,1,0), XORB_MASK, PPCCOM,		{ RT, RA } },
{ "sfmeo",   XO(31,232,1,0), XORB_MASK, PWRCOM,		{ RT, RA } },
{ "subfmeo.",XO(31,232,1,1), XORB_MASK, PPCCOM,		{ RT, RA } },
{ "sfmeo.",  XO(31,232,1,1), XORB_MASK, PWRCOM,		{ RT, RA } },

{ "mulld",   XO(31,233,0,0), XO_MASK,	PPC64,		{ RT, RA, RB } },
{ "mulld.",  XO(31,233,0,1), XO_MASK,	PPC64,		{ RT, RA, RB } },
{ "mulldo",  XO(31,233,1,0), XO_MASK,	PPC64,		{ RT, RA, RB } },
{ "mulldo.", XO(31,233,1,1), XO_MASK,	PPC64,		{ RT, RA, RB } },

{ "addme",   XO(31,234,0,0), XORB_MASK, PPCCOM,		{ RT, RA } },
{ "ame",     XO(31,234,0,0), XORB_MASK, PWRCOM,		{ RT, RA } },
{ "addme.",  XO(31,234,0,1), XORB_MASK, PPCCOM,		{ RT, RA } },
{ "ame.",    XO(31,234,0,1), XORB_MASK, PWRCOM,		{ RT, RA } },
{ "addmeo",  XO(31,234,1,0), XORB_MASK, PPCCOM,		{ RT, RA } },
{ "ameo",    XO(31,234,1,0), XORB_MASK, PWRCOM,		{ RT, RA } },
{ "addmeo.", XO(31,234,1,1), XORB_MASK, PPCCOM,		{ RT, RA } },
{ "ameo.",   XO(31,234,1,1), XORB_MASK, PWRCOM,		{ RT, RA } },

{ "mullw",   XO(31,235,0,0), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "muls",    XO(31,235,0,0), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "mullw.",  XO(31,235,0,1), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "muls.",   XO(31,235,0,1), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "mullwo",  XO(31,235,1,0), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "mulso",   XO(31,235,1,0), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "mullwo.", XO(31,235,1,1), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "mulso.",  XO(31,235,1,1), XO_MASK,	PWRCOM,		{ RT, RA, RB } },

{ "mtsrin",  X(31,242),	XRA_MASK,	PPC32,		{ RS, RB } },
{ "mtsri",   X(31,242),	XRA_MASK,	POWER32,	{ RS, RB } },

{ "dcbtst",  X(31,246),	XRT_MASK,	PPC,		{ RA, RB } },

{ "stbux",   X(31,247),	X_MASK,		COM,		{ RS, RAS, RB } },

{ "slliq",   XRC(31,248,0), X_MASK,	M601,		{ RA, RS, SH } },
{ "slliq.",  XRC(31,248,1), X_MASK,	M601,		{ RA, RS, SH } },

{ "doz",     XO(31,264,0,0), XO_MASK,	M601,		{ RT, RA, RB } },
{ "doz.",    XO(31,264,0,1), XO_MASK,	M601,		{ RT, RA, RB } },
{ "dozo",    XO(31,264,1,0), XO_MASK,	M601,		{ RT, RA, RB } },
{ "dozo.",   XO(31,264,1,1), XO_MASK,	M601,		{ RT, RA, RB } },

{ "add",     XO(31,266,0,0), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "cax",     XO(31,266,0,0), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "add.",    XO(31,266,0,1), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "cax.",    XO(31,266,0,1), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "addo",    XO(31,266,1,0), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "caxo",    XO(31,266,1,0), XO_MASK,	PWRCOM,		{ RT, RA, RB } },
{ "addo.",   XO(31,266,1,1), XO_MASK,	PPCCOM,		{ RT, RA, RB } },
{ "caxo.",   XO(31,266,1,1), XO_MASK,	PWRCOM,		{ RT, RA, RB } },

{ "lscbx",   XRC(31,277,0), X_MASK,	M601,		{ RT, RA, RB } },
{ "lscbx.",  XRC(31,277,1), X_MASK,	M601,		{ RT, RA, RB } },

{ "dcbt",    X(31,278),	XRT_MASK,	PPC,		{ RA, RB } },

{ "lhzx",    X(31,279),	X_MASK,		COM,		{ RT, RA, RB } },

{ "icbt",    X(31,262),	XRT_MASK,	PPC,		{ RA, RB } },

{ "eqv",     XRC(31,284,0), X_MASK,	COM,		{ RA, RS, RB } },
{ "eqv.",    XRC(31,284,1), X_MASK,	COM,		{ RA, RS, RB } },

{ "tlbie",   X(31,306),	XRTRA_MASK,	PPCCOM,		{ RB } },
{ "tlbi",    X(31,306),	XRTRA_MASK,	PWRCOM,		{ RB } },

{ "eciwx",   X(31,310), X_MASK,		PPC,		{ RT, RA, RB } },

{ "lhzux",   X(31,311),	X_MASK,		COM,		{ RT, RAL, RB } },

{ "xor",     XRC(31,316,0), X_MASK,	COM,		{ RA, RS, RB } },
{ "xor.",    XRC(31,316,1), X_MASK,	COM,		{ RA, RS, RB } },

{ "mfdcr",   X(31,323),	X_MASK,		PPC,		{ RT, SPR } },

{ "div",     XO(31,331,0,0), XO_MASK,	M601,		{ RT, RA, RB } },
{ "div.",    XO(31,331,0,1), XO_MASK,	M601,		{ RT, RA, RB } },
{ "divo",    XO(31,331,1,0), XO_MASK,	M601,		{ RT, RA, RB } },
{ "divo.",   XO(31,331,1,1), XO_MASK,	M601,		{ RT, RA, RB } },

{ "mfmq",    XSPR(31,339,0),   XSPR_MASK, M601,		{ RT } },
{ "mfxer",   XSPR(31,339,1),   XSPR_MASK, COM,		{ RT } },
{ "mfrtcu",  XSPR(31,339,4),   XSPR_MASK, COM,		{ RT } },
{ "mfrtcl",  XSPR(31,339,5),   XSPR_MASK, COM,		{ RT } },
{ "mfdec",   XSPR(31,339,6),   XSPR_MASK, MFDEC1,	{ RT } },
{ "mflr",    XSPR(31,339,8),   XSPR_MASK, COM,		{ RT } },
{ "mfctr",   XSPR(31,339,9),   XSPR_MASK, COM,		{ RT } },
{ "mftid",   XSPR(31,339,17),  XSPR_MASK, POWER,	{ RT } },
{ "mfdsisr", XSPR(31,339,18),  XSPR_MASK, COM,		{ RT } },
{ "mfdar",   XSPR(31,339,19),  XSPR_MASK, COM,		{ RT } },
{ "mfdec",   XSPR(31,339,22),  XSPR_MASK, MFDEC2,	{ RT } },
{ "mfsdr0",  XSPR(31,339,24),  XSPR_MASK, POWER,	{ RT } },
{ "mfsdr1",  XSPR(31,339,25),  XSPR_MASK, COM,		{ RT } },
{ "mfsrr0",  XSPR(31,339,26),  XSPR_MASK, COM,		{ RT } },
{ "mfsrr1",  XSPR(31,339,27),  XSPR_MASK, COM,		{ RT } },
{ "mfsprg",  XSPR(31,339,272), XSPRG_MASK, PPC,		{ RT, SPRG } },
{ "mfasr",   XSPR(31,339,280), XSPR_MASK, PPC64,	{ RT } },
{ "mfear",   XSPR(31,339,282), XSPR_MASK, PPC,		{ RT } },
{ "mfpvr",   XSPR(31,339,287), XSPR_MASK, PPC,		{ RT } },
{ "mfibatu", XSPR(31,339,528), XSPRBAT_MASK, PPC,	{ RT, SPRBAT } },
{ "mfibatl", XSPR(31,339,529), XSPRBAT_MASK, PPC,	{ RT, SPRBAT } },
{ "mfdbatu", XSPR(31,339,536), XSPRBAT_MASK, PPC,	{ RT, SPRBAT } },
{ "mfdbatl", XSPR(31,339,537), XSPRBAT_MASK, PPC,	{ RT, SPRBAT } },
{ "mfspr",   X(31,339),	X_MASK,		COM,		{ RT, SPR } },

{ "lwax",    X(31,341),	X_MASK,		PPC64,		{ RT, RA, RB } },

{ "lhax",    X(31,343),	X_MASK,		COM,		{ RT, RA, RB } },

{ "dccci",   X(31,454),	XRT_MASK,	PPC,		{ RA, RB } },

{ "abs",     XO(31,360,0,0), XORB_MASK, M601,		{ RT, RA } },
{ "abs.",    XO(31,360,0,1), XORB_MASK, M601,		{ RT, RA } },
{ "abso",    XO(31,360,1,0), XORB_MASK, M601,		{ RT, RA } },
{ "abso.",   XO(31,360,1,1), XORB_MASK, M601,		{ RT, RA } },

{ "divs",    XO(31,363,0,0), XO_MASK,	M601,		{ RT, RA, RB } },
{ "divs.",   XO(31,363,0,1), XO_MASK,	M601,		{ RT, RA, RB } },
{ "divso",   XO(31,363,1,0), XO_MASK,	M601,		{ RT, RA, RB } },
{ "divso.",  XO(31,363,1,1), XO_MASK,	M601,		{ RT, RA, RB } },

{ "tlbia",   X(31,370),	0xffffffff,	PPC,		{ 0 } },

{ "mftbu",   XSPR(31,371,269), XSPR_MASK, PPC,		{ RT } },
{ "mftb",    X(31,371),	X_MASK,		PPC,		{ RT, TBR } },

{ "lwaux",   X(31,373),	X_MASK,		PPC64,		{ RT, RAL, RB } },

{ "lhaux",   X(31,375),	X_MASK,		COM,		{ RT, RAL, RB } },

{ "sthx",    X(31,407),	X_MASK,		COM,		{ RS, RA, RB } },

{ "lfqx",    X(31,791),	X_MASK,		POWER2,		{ FRT, RA, RB } },

{ "lfqux",   X(31,823),	X_MASK,		POWER2,		{ FRT, RA, RB } },

{ "stfqx",   X(31,919),	X_MASK,		POWER2,		{ FRS, RA, RB } },

{ "stfqux",  X(31,951),	X_MASK,		POWER2,		{ FRS, RA, RB } },

{ "orc",     XRC(31,412,0), X_MASK,	COM,		{ RA, RS, RB } },
{ "orc.",    XRC(31,412,1), X_MASK,	COM,		{ RA, RS, RB } },

{ "sradi",   XS(31,413,0), XS_MASK,	PPC64,		{ RA, RS, SH6 } },
{ "sradi.",  XS(31,413,1), XS_MASK,	PPC64,		{ RA, RS, SH6 } },

{ "slbie",   X(31,434),	XRTRA_MASK,	PPC64,		{ RB } },

{ "ecowx",   X(31,438),	X_MASK,		PPC,		{ RT, RA, RB } },

{ "sthux",   X(31,439),	X_MASK,		COM,		{ RS, RAS, RB } },

{ "mr",	     XRC(31,444,0), X_MASK,	COM,		{ RA, RS, RBS } },
{ "or",      XRC(31,444,0), X_MASK,	COM,		{ RA, RS, RB } },
{ "mr.",     XRC(31,444,1), X_MASK,	COM,		{ RA, RS, RBS } },
{ "or.",     XRC(31,444,1), X_MASK,	COM,		{ RA, RS, RB } },

{ "mtdcr",   X(31,451),	X_MASK,		PPC,		{ SPR, RS } },

{ "divdu",   XO(31,457,0,0), XO_MASK,	PPC64,		{ RT, RA, RB } },
{ "divdu.",  XO(31,457,0,1), XO_MASK,	PPC64,		{ RT, RA, RB } },
{ "divduo",  XO(31,457,1,0), XO_MASK,	PPC64,		{ RT, RA, RB } },
{ "divduo.", XO(31,457,1,1), XO_MASK,	PPC64,		{ RT, RA, RB } },

{ "divwu",   XO(31,459,0,0), XO_MASK,	PPC,		{ RT, RA, RB } },
{ "divwu.",  XO(31,459,0,1), XO_MASK,	PPC,		{ RT, RA, RB } },
{ "divwuo",  XO(31,459,1,0), XO_MASK,	PPC,		{ RT, RA, RB } },
{ "divwuo.", XO(31,459,1,1), XO_MASK,	PPC,		{ RT, RA, RB } },

{ "mtmq",    XSPR(31,467,0),   XSPR_MASK,    M601,	{ RS } },
{ "mtxer",   XSPR(31,467,1),   XSPR_MASK,    COM,	{ RS } },
{ "mtlr",    XSPR(31,467,8),   XSPR_MASK,    COM,	{ RS } },
{ "mtctr",   XSPR(31,467,9),   XSPR_MASK,    COM,	{ RS } },
{ "mttid",   XSPR(31,467,17),  XSPR_MASK,    POWER,	{ RS } },
{ "mtdsisr", XSPR(31,467,18),  XSPR_MASK,    COM,	{ RS } },
{ "mtdar",   XSPR(31,467,19),  XSPR_MASK,    COM,	{ RS } },
{ "mtrtcu",  XSPR(31,467,20),  XSPR_MASK,    COM,	{ RS } },
{ "mtrtcl",  XSPR(31,467,21),  XSPR_MASK,    COM,	{ RS } },
{ "mtdec",   XSPR(31,467,22),  XSPR_MASK,    COM,	{ RS } },
{ "mtsdr0",  XSPR(31,467,24),  XSPR_MASK,    POWER,	{ RS } },
{ "mtsdr1",  XSPR(31,467,25),  XSPR_MASK,    COM,	{ RS } },
{ "mtsrr0",  XSPR(31,467,26),  XSPR_MASK,    COM,	{ RS } },
{ "mtsrr1",  XSPR(31,467,27),  XSPR_MASK,    COM,	{ RS } },
{ "mtsprg",  XSPR(31,467,272), XSPRG_MASK,   PPC,	{ SPRG, RS } },
{ "mtasr",   XSPR(31,467,280), XSPR_MASK,    PPC64,	{ RS } },
{ "mtear",   XSPR(31,467,282), XSPR_MASK,    PPC,	{ RS } },
{ "mttbl",   XSPR(31,467,284), XSPR_MASK,    PPC,	{ RS } },
{ "mttbu",   XSPR(31,467,285), XSPR_MASK,    PPC,	{ RS } },
{ "mtibatu", XSPR(31,467,528), XSPRBAT_MASK, PPC,	{ SPRBAT, RS } },
{ "mtibatl", XSPR(31,467,529), XSPRBAT_MASK, PPC,	{ SPRBAT, RS } },
{ "mtdbatu", XSPR(31,467,536), XSPRBAT_MASK, PPC,	{ SPRBAT, RS } },
{ "mtdbatl", XSPR(31,467,537), XSPRBAT_MASK, PPC,	{ SPRBAT, RS } },
{ "mtspr",   X(31,467),	       X_MASK,	     COM,	{ SPR, RS } },

{ "dcbi",    X(31,470),	XRT_MASK,	PPC,		{ RA, RB } },

{ "nand",    XRC(31,476,0), X_MASK,	COM,		{ RA, RS, RB } },
{ "nand.",   XRC(31,476,1), X_MASK,	COM,		{ RA, RS, RB } },

{ "nabs",    XO(31,488,0,0), XORB_MASK, M601,		{ RT, RA } },
{ "nabs.",   XO(31,488,0,1), XORB_MASK, M601,		{ RT, RA } },
{ "nabso",   XO(31,488,1,0), XORB_MASK, M601,		{ RT, RA } },
{ "nabso.",  XO(31,488,1,1), XORB_MASK, M601,		{ RT, RA } },

{ "divd",    XO(31,489,0,0), XO_MASK,	PPC64,		{ RT, RA, RB } },
{ "divd.",   XO(31,489,0,1), XO_MASK,	PPC64,		{ RT, RA, RB } },
{ "divdo",   XO(31,489,1,0), XO_MASK,	PPC64,		{ RT, RA, RB } },
{ "divdo.",  XO(31,489,1,1), XO_MASK,	PPC64,		{ RT, RA, RB } },

{ "divw",    XO(31,491,0,0), XO_MASK,	PPC,		{ RT, RA, RB } },
{ "divw.",   XO(31,491,0,1), XO_MASK,	PPC,		{ RT, RA, RB } },
{ "divwo",   XO(31,491,1,0), XO_MASK,	PPC,		{ RT, RA, RB } },
{ "divwo.",  XO(31,491,1,1), XO_MASK,	PPC,		{ RT, RA, RB } },

{ "slbia",   X(31,498),	0xffffffff,	PPC64,		{ 0 } },

{ "cli",     X(31,502), XRB_MASK,	POWER,		{ RT, RA } },

{ "mcrxr",   X(31,512),	XRARB_MASK|(3<<21), COM,	{ BF } },

{ "clcs",    X(31,531), XRB_MASK,	M601,		{ RT, RA } },

{ "lswx",    X(31,533),	X_MASK,		PPCCOM,		{ RT, RA, RB } },
{ "lsx",     X(31,533),	X_MASK,		PWRCOM,		{ RT, RA, RB } },

{ "lwbrx",   X(31,534),	X_MASK,		PPCCOM,		{ RT, RA, RB } },
{ "lbrx",    X(31,534),	X_MASK,		PWRCOM,		{ RT, RA, RB } },

{ "lfsx",    X(31,535),	X_MASK,		COM,		{ FRT, RA, RB } },

{ "srw",     XRC(31,536,0), X_MASK,	PPCCOM,		{ RA, RS, RB } },
{ "sr",      XRC(31,536,0), X_MASK,	PWRCOM,		{ RA, RS, RB } },
{ "srw.",    XRC(31,536,1), X_MASK,	PPCCOM,		{ RA, RS, RB } },
{ "sr.",     XRC(31,536,1), X_MASK,	PWRCOM,		{ RA, RS, RB } },

{ "rrib",    XRC(31,537,0), X_MASK,	M601,		{ RA, RS, RB } },
{ "rrib.",   XRC(31,537,1), X_MASK,	M601,		{ RA, RS, RB } },

{ "srd",     XRC(31,539,0), X_MASK,	PPC64,		{ RA, RS, RB } },
{ "srd.",    XRC(31,539,1), X_MASK,	PPC64,		{ RA, RS, RB } },

{ "maskir",  XRC(31,541,0), X_MASK,	M601,		{ RA, RS, RB } },
{ "maskir.", XRC(31,541,1), X_MASK,	M601,		{ RA, RS, RB } },

{ "tlbsync", X(31,566),	0xffffffff,	PPC,		{ 0 } },

{ "lfsux",   X(31,567),	X_MASK,		COM,		{ FRT, RAS, RB } },

{ "mfsr",    X(31,595),	XRB_MASK|(1<<20), COM32,	{ RT, SR } },

{ "lswi",    X(31,597),	X_MASK,		PPCCOM,		{ RT, RA, NB } },
{ "lsi",     X(31,597),	X_MASK,		PWRCOM,		{ RT, RA, NB } },

{ "sync",    X(31,598), 0xffffffff,	PPCCOM,		{ 0 } },
{ "dcs",     X(31,598), 0xffffffff,	PWRCOM,		{ 0 } },

{ "lfdx",    X(31,599), X_MASK,		COM,		{ FRT, RA, RB } },

{ "mfsri",   X(31,627), X_MASK,		PWRCOM,		{ RT, RA, RB } },

{ "dclst",   X(31,630), XRB_MASK,	PWRCOM,		{ RS, RA } },

{ "lfdux",   X(31,631), X_MASK,		COM,		{ FRT, RAS, RB } },

{ "mfsrin",  X(31,659), XRA_MASK,	PPC32,		{ RT, RB } },

{ "stswx",   X(31,661), X_MASK,		PPCCOM,		{ RS, RA, RB } },
{ "stsx",    X(31,661), X_MASK,		PWRCOM,		{ RS, RA, RB } },

{ "stwbrx",  X(31,662), X_MASK,		PPCCOM,		{ RS, RA, RB } },
{ "stbrx",   X(31,662), X_MASK,		PWRCOM,		{ RS, RA, RB } },

{ "stfsx",   X(31,663), X_MASK,		COM,		{ FRS, RA, RB } },

{ "srq",     XRC(31,664,0), X_MASK,	M601,		{ RA, RS, RB } },
{ "srq.",    XRC(31,664,1), X_MASK,	M601,		{ RA, RS, RB } },

{ "sre",     XRC(31,665,0), X_MASK,	M601,		{ RA, RS, RB } },
{ "sre.",    XRC(31,665,1), X_MASK,	M601,		{ RA, RS, RB } },

{ "stfsux",  X(31,695),	X_MASK,		COM,		{ FRS, RAS, RB } },

{ "sriq",    XRC(31,696,0), X_MASK,	M601,		{ RA, RS, SH } },
{ "sriq.",   XRC(31,696,1), X_MASK,	M601,		{ RA, RS, SH } },

{ "stswi",   X(31,725),	X_MASK,		PPCCOM,		{ RS, RA, NB } },
{ "stsi",    X(31,725),	X_MASK,		PWRCOM,		{ RS, RA, NB } },

{ "stfdx",   X(31,727),	X_MASK,		COM,		{ FRS, RA, RB } },

{ "srlq",    XRC(31,728,0), X_MASK,	M601,		{ RA, RS, RB } },
{ "srlq.",   XRC(31,728,1), X_MASK,	M601,		{ RA, RS, RB } },

{ "sreq",    XRC(31,729,0), X_MASK,	M601,		{ RA, RS, RB } },
{ "sreq.",   XRC(31,729,1), X_MASK,	M601,		{ RA, RS, RB } },

{ "stfdux",  X(31,759),	X_MASK,		COM,		{ FRS, RAS, RB } },

{ "srliq",   XRC(31,760,0), X_MASK,	M601,		{ RA, RS, SH } },
{ "srliq.",  XRC(31,760,1), X_MASK,	M601,		{ RA, RS, SH } },

{ "lhbrx",   X(31,790),	X_MASK,		COM,		{ RT, RA, RB } },

{ "sraw",    XRC(31,792,0), X_MASK,	PPCCOM,		{ RA, RS, RB } },
{ "sra",     XRC(31,792,0), X_MASK,	PWRCOM,		{ RA, RS, RB } },
{ "sraw.",   XRC(31,792,1), X_MASK,	PPCCOM,		{ RA, RS, RB } },
{ "sra.",    XRC(31,792,1), X_MASK,	PWRCOM,		{ RA, RS, RB } },

{ "srad",    XRC(31,794,0), X_MASK,	PPC64,		{ RA, RS, RB } },
{ "srad.",   XRC(31,794,1), X_MASK,	PPC64,		{ RA, RS, RB } },

{ "rac",     X(31,818),	X_MASK,		PWRCOM,		{ RT, RA, RB } },

{ "srawi",   XRC(31,824,0), X_MASK,	PPCCOM,		{ RA, RS, SH } },
{ "srai",    XRC(31,824,0), X_MASK,	PWRCOM,		{ RA, RS, SH } },
{ "srawi.",  XRC(31,824,1), X_MASK,	PPCCOM,		{ RA, RS, SH } },
{ "srai.",   XRC(31,824,1), X_MASK,	PWRCOM,		{ RA, RS, SH } },

{ "eieio",   X(31,854),	0xffffffff,	PPC,		{ 0 } },

{ "sthbrx",  X(31,918),	X_MASK,		COM,		{ RS, RA, RB } },

{ "sraq",    XRC(31,920,0), X_MASK,	M601,		{ RA, RS, RB } },
{ "sraq.",   XRC(31,920,1), X_MASK,	M601,		{ RA, RS, RB } },

{ "srea",    XRC(31,921,0), X_MASK,	M601,		{ RA, RS, RB } },
{ "srea.",   XRC(31,921,1), X_MASK,	M601,		{ RA, RS, RB } },

{ "extsh",   XRC(31,922,0), XRB_MASK,	PPCCOM,		{ RA, RS } },
{ "exts",    XRC(31,922,0), XRB_MASK,	PWRCOM,		{ RA, RS } },
{ "extsh.",  XRC(31,922,1), XRB_MASK,	PPCCOM,		{ RA, RS } },
{ "exts.",   XRC(31,922,1), XRB_MASK,	PWRCOM,		{ RA, RS } },

{ "sraiq",   XRC(31,952,0), X_MASK,	M601,		{ RA, RS, SH } },
{ "sraiq.",  XRC(31,952,1), X_MASK,	M601,		{ RA, RS, SH } },

{ "extsb",   XRC(31,954,0), XRB_MASK,	PPC,		{ RA, RS} },
{ "extsb.",  XRC(31,954,1), XRB_MASK,	PPC,		{ RA, RS} },

{ "iccci",   X(31,966),	XRT_MASK,	PPC,		{ RA, RB } },

{ "icbi",    X(31,982),	XRT_MASK,	PPC,		{ RA, RB } },

{ "stfiwx",  X(31,983),	X_MASK,		PPC,		{ FRS, RA, RB } },

{ "extsw",   XRC(31,986,0), XRB_MASK,	PPC,		{ RA, RS } },
{ "extsw.",  XRC(31,986,1), XRB_MASK,	PPC,		{ RA, RS } },

{ "dcbz",    X(31,1014), XRT_MASK,	PPC,		{ RA, RB } },
{ "dclz",    X(31,1014), XRT_MASK,	PPC,		{ RA, RB } },

{ "lwz",     OP(32),	OP_MASK,	PPCCOM,		{ RT, D, RA } },
{ "l",	     OP(32),	OP_MASK,	PWRCOM,		{ RT, D, RA } },

{ "lwzu",    OP(33),	OP_MASK,	PPCCOM,		{ RT, D, RAL } },
{ "lu",      OP(33),	OP_MASK,	PWRCOM,		{ RT, D, RA } },

{ "lbz",     OP(34),	OP_MASK,	COM,		{ RT, D, RA } },

{ "lbzu",    OP(35),	OP_MASK,	COM,		{ RT, D, RAL } },

{ "stw",     OP(36),	OP_MASK,	PPCCOM,		{ RS, D, RA } },
{ "st",      OP(36),	OP_MASK,	PWRCOM,		{ RS, D, RA } },

{ "stwu",    OP(37),	OP_MASK,	PPCCOM,		{ RS, D, RAS } },
{ "stu",     OP(37),	OP_MASK,	PWRCOM,		{ RS, D, RA } },

{ "stb",     OP(38),	OP_MASK,	COM,		{ RS, D, RA } },

{ "stbu",    OP(39),	OP_MASK,	COM,		{ RS, D, RAS } },

{ "lhz",     OP(40),	OP_MASK,	COM,		{ RT, D, RA } },

{ "lhzu",    OP(41),	OP_MASK,	COM,		{ RT, D, RAL } },

{ "lha",     OP(42),	OP_MASK,	COM,		{ RT, D, RA } },

{ "lhau",    OP(43),	OP_MASK,	COM,		{ RT, D, RAL } },

{ "sth",     OP(44),	OP_MASK,	COM,		{ RS, D, RA } },

{ "sthu",    OP(45),	OP_MASK,	COM,		{ RS, D, RAS } },

{ "lmw",     OP(46),	OP_MASK,	PPCCOM,		{ RT, D, RAM } },
{ "lm",      OP(46),	OP_MASK,	PWRCOM,		{ RT, D, RA } },

{ "stmw",    OP(47),	OP_MASK,	PPCCOM,		{ RS, D, RA } },
{ "stm",     OP(47),	OP_MASK,	PWRCOM,		{ RS, D, RA } },

{ "lfs",     OP(48),	OP_MASK,	COM,		{ FRT, D, RA } },

{ "lfsu",    OP(49),	OP_MASK,	COM,		{ FRT, D, RAS } },

{ "lfd",     OP(50),	OP_MASK,	COM,		{ FRT, D, RA } },

{ "lfdu",    OP(51),	OP_MASK,	COM,		{ FRT, D, RAS } },

{ "stfs",    OP(52),	OP_MASK,	COM,		{ FRS, D, RA } },

{ "stfsu",   OP(53),	OP_MASK,	COM,		{ FRS, D, RAS } },

{ "stfd",    OP(54),	OP_MASK,	COM,		{ FRS, D, RA } },

{ "stfdu",   OP(55),	OP_MASK,	COM,		{ FRS, D, RAS } },

{ "lfq",     OP(56),	OP_MASK,	POWER2,		{ FRT, D, RA } },

{ "lfqu",    OP(57),	OP_MASK,	POWER2,		{ FRT, D, RA } },

{ "ld",      DSO(58,0),	DS_MASK,	PPC64,		{ RT, DS, RA } },

{ "ldu",     DSO(58,1), DS_MASK,	PPC64,		{ RT, DS, RAL } },

{ "lwa",     DSO(58,2), DS_MASK,	PPC64,		{ RT, DS, RA } },

{ "fdivs",   A(59,18,0), AFRC_MASK,	PPC,		{ FRT, FRA, FRB } },
{ "fdivs.",  A(59,18,1), AFRC_MASK,	PPC,		{ FRT, FRA, FRB } },

{ "fsubs",   A(59,20,0), AFRC_MASK,	PPC,		{ FRT, FRA, FRB } },
{ "fsubs.",  A(59,20,1), AFRC_MASK,	PPC,		{ FRT, FRA, FRB } },

{ "fadds",   A(59,21,0), AFRC_MASK,	PPC,		{ FRT, FRA, FRB } },
{ "fadds.",  A(59,21,1), AFRC_MASK,	PPC,		{ FRT, FRA, FRB } },

{ "fsqrts",  A(59,22,0), AFRAFRC_MASK,	PPC,		{ FRT, FRB } },
{ "fsqrts.", A(59,22,1), AFRAFRC_MASK,	PPC,		{ FRT, FRB } },

{ "fres",    A(59,24,0), AFRAFRC_MASK,	PPC,		{ FRT, FRB } },
{ "fres.",   A(59,24,1), AFRAFRC_MASK,	PPC,		{ FRT, FRB } },

{ "fmuls",   A(59,25,0), AFRB_MASK,	PPC,		{ FRT, FRA, FRC } },
{ "fmuls.",  A(59,25,1), AFRB_MASK,	PPC,		{ FRT, FRA, FRC } },

{ "fmsubs",  A(59,28,0), A_MASK,	PPC,		{ FRT,FRA,FRC,FRB } },
{ "fmsubs.", A(59,28,1), A_MASK,	PPC,		{ FRT,FRA,FRC,FRB } },

{ "fmadds",  A(59,29,0), A_MASK,	PPC,		{ FRT,FRA,FRC,FRB } },
{ "fmadds.", A(59,29,1), A_MASK,	PPC,		{ FRT,FRA,FRC,FRB } },

{ "fnmsubs", A(59,30,0), A_MASK,	PPC,		{ FRT,FRA,FRC,FRB } },
{ "fnmsubs.",A(59,30,1), A_MASK,	PPC,		{ FRT,FRA,FRC,FRB } },

{ "fnmadds", A(59,31,0), A_MASK,	PPC,		{ FRT,FRA,FRC,FRB } },
{ "fnmadds.",A(59,31,1), A_MASK,	PPC,		{ FRT,FRA,FRC,FRB } },

{ "stfq",    OP(60),	OP_MASK,	POWER2,		{ FRS, D, RA } },

{ "stfqu",   OP(61),	OP_MASK,	POWER2,		{ FRS, D, RA } },

{ "std",     DSO(62,0),	DS_MASK,	PPC64,		{ RS, DS, RA } },

{ "stdu",    DSO(62,1),	DS_MASK,	PPC64,		{ RS, DS, RAS } },

{ "fcmpu",   X(63,0),	X_MASK|(3<<21),	COM,		{ BF, FRA, FRB } },

{ "frsp",    XRC(63,12,0), XRA_MASK,	COM,		{ FRT, FRB } },
{ "frsp.",   XRC(63,12,1), XRA_MASK,	COM,		{ FRT, FRB } },

{ "fctiw",   XRC(63,14,0), XRA_MASK,	PPCCOM,		{ FRT, FRB } },
{ "fcir",    XRC(63,14,0), XRA_MASK,	POWER2,		{ FRT, FRB } },
{ "fctiw.",  XRC(63,14,1), XRA_MASK,	PPCCOM,		{ FRT, FRB } },
{ "fcir.",   XRC(63,14,1), XRA_MASK,	POWER2,		{ FRT, FRB } },

{ "fctiwz",  XRC(63,15,0), XRA_MASK,	PPCCOM,		{ FRT, FRB } },
{ "fcirz",   XRC(63,15,0), XRA_MASK,	POWER2,		{ FRT, FRB } },
{ "fctiwz.", XRC(63,15,1), XRA_MASK,	PPCCOM,		{ FRT, FRB } },
{ "fcirz.",  XRC(63,15,1), XRA_MASK,	POWER2,		{ FRT, FRB } },

{ "fdiv",    A(63,18,0), AFRC_MASK,	PPCCOM,		{ FRT, FRA, FRB } },
{ "fd",      A(63,18,0), AFRC_MASK,	PWRCOM,		{ FRT, FRA, FRB } },
{ "fdiv.",   A(63,18,1), AFRC_MASK,	PPCCOM,		{ FRT, FRA, FRB } },
{ "fd.",     A(63,18,1), AFRC_MASK,	PWRCOM,		{ FRT, FRA, FRB } },

{ "fsub",    A(63,20,0), AFRC_MASK,	PPCCOM,		{ FRT, FRA, FRB } },
{ "fs",      A(63,20,0), AFRC_MASK,	PWRCOM,		{ FRT, FRA, FRB } },
{ "fsub.",   A(63,20,1), AFRC_MASK,	PPCCOM,		{ FRT, FRA, FRB } },
{ "fs.",     A(63,20,1), AFRC_MASK,	PWRCOM,		{ FRT, FRA, FRB } },

{ "fadd",    A(63,21,0), AFRC_MASK,	PPCCOM,		{ FRT, FRA, FRB } },
{ "fa",      A(63,21,0), AFRC_MASK,	PWRCOM,		{ FRT, FRA, FRB } },
{ "fadd.",   A(63,21,1), AFRC_MASK,	PPCCOM,		{ FRT, FRA, FRB } },
{ "fa.",     A(63,21,1), AFRC_MASK,	PWRCOM,		{ FRT, FRA, FRB } },

{ "fsqrt",   A(63,22,0), AFRAFRC_MASK,	POWER2,		{ FRT, FRB } },
{ "fsqrt.",  A(63,22,1), AFRAFRC_MASK,	POWER2,		{ FRT, FRB } },

{ "fsel",    A(63,23,0), A_MASK,	PPC,		{ FRT,FRA,FRC,FRB } },
{ "fsel.",   A(63,23,1), A_MASK,	PPC,		{ FRT,FRA,FRC,FRB } },

{ "fmul",    A(63,25,0), AFRB_MASK,	PPCCOM,		{ FRT, FRA, FRC } },
{ "fm",      A(63,25,0), AFRB_MASK,	PWRCOM,		{ FRT, FRA, FRC } },
{ "fmul.",   A(63,25,1), AFRB_MASK,	PPCCOM,		{ FRT, FRA, FRC } },
{ "fm.",     A(63,25,1), AFRB_MASK,	PWRCOM,		{ FRT, FRA, FRC } },

{ "frsqrte", A(63,26,0), AFRAFRC_MASK,	PPC,		{ FRT, FRB } },
{ "frsqrte.",A(63,26,1), AFRAFRC_MASK,	PPC,		{ FRT, FRB } },

{ "fmsub",   A(63,28,0), A_MASK,	PPCCOM,		{ FRT,FRA,FRC,FRB } },
{ "fms",     A(63,28,0), A_MASK,	PWRCOM,		{ FRT,FRA,FRC,FRB } },
{ "fmsub.",  A(63,28,1), A_MASK,	PPCCOM,		{ FRT,FRA,FRC,FRB } },
{ "fms.",    A(63,28,1), A_MASK,	PWRCOM,		{ FRT,FRA,FRC,FRB } },

{ "fmadd",   A(63,29,0), A_MASK,	PPCCOM,		{ FRT,FRA,FRC,FRB } },
{ "fma",     A(63,29,0), A_MASK,	PWRCOM,		{ FRT,FRA,FRC,FRB } },
{ "fmadd.",  A(63,29,1), A_MASK,	PPCCOM,		{ FRT,FRA,FRC,FRB } },
{ "fma.",    A(63,29,1), A_MASK,	PWRCOM,		{ FRT,FRA,FRC,FRB } },

{ "fnmsub",  A(63,30,0), A_MASK,	PPCCOM,		{ FRT,FRA,FRC,FRB } },
{ "fnms",    A(63,30,0), A_MASK,	PWRCOM,		{ FRT,FRA,FRC,FRB } },
{ "fnmsub.", A(63,30,1), A_MASK,	PPCCOM,		{ FRT,FRA,FRC,FRB } },
{ "fnms.",   A(63,30,1), A_MASK,	PWRCOM,		{ FRT,FRA,FRC,FRB } },

{ "fnmadd",  A(63,31,0), A_MASK,	PPCCOM,		{ FRT,FRA,FRC,FRB } },
{ "fnma",    A(63,31,0), A_MASK,	PWRCOM,		{ FRT,FRA,FRC,FRB } },
{ "fnmadd.", A(63,31,1), A_MASK,	PPCCOM,		{ FRT,FRA,FRC,FRB } },
{ "fnma.",   A(63,31,1), A_MASK,	PWRCOM,		{ FRT,FRA,FRC,FRB } },

{ "fcmpo",   X(63,30),	X_MASK|(3<<21),	COM,		{ BF, FRA, FRB } },

{ "mtfsb1",  XRC(63,38,0), XRARB_MASK,	COM,		{ BT } },
{ "mtfsb1.", XRC(63,38,1), XRARB_MASK,	COM,		{ BT } },

{ "fneg",    XRC(63,40,0), XRA_MASK,	COM,		{ FRT, FRB } },
{ "fneg.",   XRC(63,40,1), XRA_MASK,	COM,		{ FRT, FRB } },

{ "mcrfs",   X(63,64),	XRB_MASK|(3<<21)|(3<<16), COM,	{ BF, BFA } },

{ "mtfsb0",  XRC(63,70,0), XRARB_MASK,	COM,		{ BT } },
{ "mtfsb0.", XRC(63,70,1), XRARB_MASK,	COM,		{ BT } },

{ "fmr",     XRC(63,72,0), XRA_MASK,	COM,		{ FRT, FRB } },
{ "fmr.",    XRC(63,72,1), XRA_MASK,	COM,		{ FRT, FRB } },

{ "mtfsfi",  XRC(63,134,0), XRA_MASK|(3<<21)|(1<<11), COM, { BF, U } },
{ "mtfsfi.", XRC(63,134,1), XRA_MASK|(3<<21)|(1<<11), COM, { BF, U } },

{ "fnabs",   XRC(63,136,0), XRA_MASK,	COM,		{ FRT, FRB } },
{ "fnabs.",  XRC(63,136,1), XRA_MASK,	COM,		{ FRT, FRB } },
	
{ "fabs",    XRC(63,264,0), XRA_MASK,	COM,		{ FRT, FRB } },
{ "fabs.",   XRC(63,264,1), XRA_MASK,	COM,		{ FRT, FRB } },
	
{ "mffs",    XRC(63,583,0), XRARB_MASK,	COM,		{ FRT } },
{ "mffs.",   XRC(63,583,1), XRARB_MASK,	COM,		{ FRT } },
	
{ "mtfsf",   XFL(63,711,0), XFL_MASK,	COM,		{ FLM, FRB } },
{ "mtfsf.",  XFL(63,711,1), XFL_MASK,	COM,		{ FLM, FRB } },

{ "fctid",   XRC(63,814,0), XRA_MASK,	PPC64,		{ FRT, FRB } },
{ "fctid.",  XRC(63,814,1), XRA_MASK,	PPC64,		{ FRT, FRB } },

{ "fctidz",  XRC(63,815,0), XRA_MASK,	PPC64,		{ FRT, FRB } },
{ "fctidz.", XRC(63,815,1), XRA_MASK,	PPC64,		{ FRT, FRB } },

{ "fcfid",   XRC(63,846,0), XRA_MASK,	PPC64,		{ FRT, FRB } },
{ "fcfid.",  XRC(63,846,1), XRA_MASK,	PPC64,		{ FRT, FRB } },

};

const int powerpc_num_opcodes =
  sizeof (powerpc_opcodes) / sizeof (powerpc_opcodes[0]);

/* The macro table.  This is only used by the assembler.  */

const struct powerpc_macro powerpc_macros[] = {
{ "extldi",  4,   PPC64,	"rldicr %0,%1,%3,(%2)-1" },
{ "extldi.", 4,   PPC64,	"rldicr. %0,%1,%3,(%2)-1" },
{ "extrdi",  4,   PPC64,	"rldicl %0,%1,(%2)+(%3),64-(%2)" },
{ "extrdi.", 4,   PPC64,	"rldicl. %0,%1,(%2)+(%3),64-(%2)" },
{ "insrdi",  4,   PPC64,	"rldimi %0,%1,64-((%2)+(%3)),%3" },
{ "insrdi.", 4,   PPC64,	"rldimi. %0,%1,64-((%2)+(%3)),%3" },
{ "rotrdi",  3,   PPC64,	"rldicl %0,%1,64-(%2),0" },
{ "rotrdi.", 3,   PPC64,	"rldicl. %0,%1,64-(%2),0" },
{ "sldi",    3,   PPC64,	"rldicr %0,%1,%2,63-(%2)" },
{ "sldi.",   3,   PPC64,	"rldicr. %0,%1,%2,63-(%2)" },
{ "srdi",    3,   PPC64,	"rldicl %0,%1,64-(%2),%2" },
{ "srdi.",   3,   PPC64,	"rldicl. %0,%1,64-(%2),%2" },
{ "clrrdi",  3,   PPC64,	"rldicr %0,%1,0,63-(%2)" },
{ "clrrdi.", 3,   PPC64,	"rldicr. %0,%1,0,63-(%2)" },
{ "clrlsldi",4,   PPC64,	"rldic %0,%1,%3,(%2)-(%3)" },
{ "clrlsldi.",4,  PPC64,	"rldic. %0,%1,%3,(%2)-(%3)" },

{ "extlwi",  4,   PPCCOM,	"rlwinm %0,%1,%3,0,(%2)-1" },
{ "extlwi.", 4,   PPCCOM,	"rlwinm. %0,%1,%3,0,(%2)-1" },
{ "extrwi",  4,   PPCCOM,	"rlwinm %0,%1,(%2)+(%3),32-(%2),31" },
{ "extrwi.", 4,   PPCCOM,	"rlwinm. %0,%1,(%2)+(%3),32-(%2),31" },
{ "inslwi",  4,   PPCCOM,	"rlwimi %0,%1,32-(%3),%3,(%2)+(%3)-1" },
{ "inslwi.", 4,   PPCCOM,	"rlwimi. %0,%1,32-(%3),%3,(%2)+(%3)-1" },
{ "insrwi",  4,   PPCCOM,	"rlwimi %0,%1,32-((%2)+(%3)),%3,(%2)+(%3)-1" },
{ "insrwi.", 4,   PPCCOM,	"rlwimi. %0,%1,32-((%2)+(%3)),%3,(%2)+(%3)-1"},
{ "rotrwi",  3,   PPCCOM,	"rlwinm %0,%1,32-(%2),0,31" },
{ "rotrwi.", 3,   PPCCOM,	"rlwinm. %0,%1,32-(%2),0,31" },
{ "slwi",    3,   PPCCOM,	"rlwinm %0,%1,%2,0,31-(%2)" },
{ "sli",     3,   PWRCOM,	"rlinm %0,%1,%2,0,31-(%2)" },
{ "slwi.",   3,   PPCCOM,	"rlwinm. %0,%1,%2,0,31-(%2)" },
{ "sli.",    3,   PWRCOM,	"rlinm. %0,%1,%2,0,31-(%2)" },
{ "srwi",    3,   PPCCOM,	"rlwinm %0,%1,32-(%2),%2,31" },
{ "sri",     3,   PWRCOM,	"rlinm %0,%1,32-(%2),%2,31" },
{ "srwi.",   3,   PPCCOM,	"rlwinm. %0,%1,32-(%2),%2,31" },
{ "sri.",    3,   PWRCOM,	"rlinm. %0,%1,32-(%2),%2,31" },
{ "clrrwi",  3,   PPCCOM,	"rlwinm %0,%1,0,0,31-(%2)" },
{ "clrrwi.", 3,   PPCCOM,	"rlwinm. %0,%1,0,0,31-(%2)" },
{ "clrlslwi",4,   PPCCOM,	"rlwinm %0,%1,%3,(%2)-(%3),31-(%3)" },
{ "clrlslwi.",4,  PPCCOM,	"rlwinm. %0,%1,%3,(%2)-(%3),31-(%3)" },

};

const int powerpc_num_macros =
  sizeof (powerpc_macros) / sizeof (powerpc_macros[0]);

#endif /* PPC_H */

/* m68k-parse.h -- header file for m68k assembler
   Copyright (C) 1987, 91, 92, 93, 94, 1995 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef M68K_PARSE_H
#define M68K_PARSE_H

/* This header file defines things which are shared between the
   operand parser in m68k.y and the m68k assembler proper in
   tc-m68k.c.  */

/* The various m68k registers.  */

/* DATA and ADDR have to be contiguous, so that reg-DATA gives
   0-7==data reg, 8-15==addr reg for operands that take both types.

   We don't use forms like "ADDR0 = ADDR" here because this file is
   likely to be used on an Apollo, and the broken Apollo compiler
   gives an `undefined variable' error if we do that, according to
   troy@cbme.unsw.edu.au.  */

#define DATA DATA0
#define ADDR ADDR0
#define SP ADDR7
#define BAD BAD0
#define BAC BAC0

enum m68k_register
{
  DATA0 = 1,			/*   1- 8 == data registers 0-7 */
  DATA1,
  DATA2,
  DATA3,
  DATA4,
  DATA5,
  DATA6,
  DATA7,

  ADDR0,
  ADDR1,
  ADDR2,
  ADDR3,
  ADDR4,
  ADDR5,
  ADDR6,
  ADDR7,

  FP0,				/* Eight FP registers */
  FP1,
  FP2,
  FP3,
  FP4,
  FP5,
  FP6,
  FP7,

  COP0,				/* Co-processor #0-#7 */
  COP1,
  COP2,
  COP3,
  COP4,
  COP5,
  COP6,
  COP7,

  PC,				/* Program counter */
  ZPC,				/* Hack for Program space, but 0 addressing */
  SR,				/* Status Reg */
  CCR,				/* Condition code Reg */

  /* These have to be grouped together for the movec instruction to work. */
  USP,				/*  User Stack Pointer */
  ISP,				/*  Interrupt stack pointer */
  SFC,
  DFC,
  CACR,
  VBR,
  CAAR,
  MSP,
  ITT0,
  ITT1,
  DTT0,
  DTT1,
  MMUSR,
  TC,
  SRP,
  URP,
  BUSCR,			/* 68060 added these */
  PCR,
#define last_movec_reg PCR
  /* end of movec ordering constraints */

  FPI,
  FPS,
  FPC,

  DRP,				/* 68851 or 68030 MMU regs */
  CRP,
  CAL,
  VAL,
  SCC,
  AC,
  BAD0,
  BAD1,
  BAD2,
  BAD3,
  BAD4,
  BAD5,
  BAD6,
  BAD7,
  BAC0,
  BAC1,
  BAC2,
  BAC3,
  BAC4,
  BAC5,
  BAC6,
  BAC7,
  PSR,				/* aka MMUSR on 68030 (but not MMUSR on 68040)
				   and ACUSR on 68ec030 */
  PCSR,

  IC,				/* instruction cache token */
  DC,				/* data cache token */
  NC,				/* no cache token */
  BC,				/* both caches token */

  TT0,				/* 68030 access control unit regs */
  TT1,

  ZDATA0,			/* suppressed data registers.  */
  ZDATA1,
  ZDATA2,
  ZDATA3,
  ZDATA4,
  ZDATA5,
  ZDATA6,
  ZDATA7,

  ZADDR0,			/* suppressed address registers.  */
  ZADDR1,
  ZADDR2,
  ZADDR3,
  ZADDR4,
  ZADDR5,
  ZADDR6,
  ZADDR7,
};

/* Size information.  */

enum m68k_size
{
  /* Unspecified.  */
  SIZE_UNSPEC,

  /* Byte.  */
  SIZE_BYTE,

  /* Word (2 bytes).  */
  SIZE_WORD,

  /* Longword (4 bytes).  */
  SIZE_LONG
};

/* The structure used to hold information about an index register.  */

struct m68k_indexreg
{
  /* The index register itself.  */
  enum m68k_register reg;

  /* The size to use.  */
  enum m68k_size size;

  /* The value to scale by.  */
  int scale;
};

/* The structure used to hold information about an expression.  */

struct m68k_exp
{
  /* The size to use.  */
  enum m68k_size size;

  /* The expression itself.  */
  expressionS exp;
};

/* The operand modes.  */

enum m68k_operand_type
{
  IMMED = 1,
  ABSL,
  DREG,
  AREG,
  FPREG,
  CONTROL,
  AINDR,
  AINC,
  ADEC,
  DISP,
  BASE,
  POST,
  PRE,
  REGLST
};

/* The structure used to hold a parsed operand.  */

struct m68k_op
{
  /* The type of operand.  */
  enum m68k_operand_type mode;

  /* The main register.  */
  enum m68k_register reg;

  /* The register mask for mode REGLST.  */
  unsigned long mask;

  /* An error message.  */
  const char *error;

  /* The index register.  */
  struct m68k_indexreg index;

  /* The displacement.  */
  struct m68k_exp disp;

  /* The outer displacement.  */
  struct m68k_exp odisp;
};

#endif /* ! defined (M68K_PARSE_H) */

/* The parsing function.  */

extern int m68k_ip_op PARAMS ((char *, struct m68k_op *));

/* Whether register prefixes are optional.  */
extern int flag_reg_prefix_optional;

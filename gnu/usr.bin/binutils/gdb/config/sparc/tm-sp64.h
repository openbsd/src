/* Target machine sub-parameters for SPARC64, for GDB, the GNU debugger.
   This is included by other tm-*.h files to define SPARC64 cpu-related info.
   Copyright 1994, 1995, 1996 Free Software Foundation, Inc.
   This is (obviously) based on the SPARC Vn (n<9) port.
   Contributed by Doug Evans (dje@cygnus.com).

This file is part of GDB.

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

#define GDB_TARGET_IS_SPARC64

#include "sparc/tm-sparc.h"

/* Stack must be aligned on 128-bit boundaries when synthesizing
   function calls. */

#undef  STACK_ALIGN
#define STACK_ALIGN(ADDR) (((ADDR) + 15 ) & -16)

/* Number of machine registers.  */

#undef  NUM_REGS
#define NUM_REGS 125

/* Initializer for an array of names of registers.
   There should be NUM_REGS strings in this initializer.  */
/* Some of these registers are only accessible from priviledged mode.
   They are here for kernel debuggers, etc.  */
/* FIXME: icc and xcc are currently considered separate registers.
   This may have to change and consider them as just one (ccr).
   Let's postpone this as long as we can.  It's nice to be able to set
   them individually.  */
/* FIXME: fcc0-3 are currently separate, even though they are also part of
   fsr.  May have to remove them but let's postpone this as long as
   possible.  It's nice to be able to set them individually.  */
/* FIXME: Whether to include f33, f35, etc. here is not clear.
   There are advantages and disadvantages.  */

#undef  REGISTER_NAMES
#define REGISTER_NAMES  \
{ "g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",	\
  "o0", "o1", "o2", "o3", "o4", "o5", "sp", "o7",	\
  "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7",	\
  "i0", "i1", "i2", "i3", "i4", "i5", "fp", "i7",	\
								\
  "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",		\
  "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",		\
  "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",	\
  "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",	\
  "f32", "f34", "f36", "f38", "f40", "f42", "f44", "f46",	\
  "f48", "f50", "f52", "f54", "f56", "f58", "f60", "f62",	\
                                                                \
  "pc", "npc", "ccr", "fsr", "fprs", "y", "asi",		\
  "ver", "tick", "pil", "pstate",				\
  "tstate", "tba", "tl", "tt", "tpc", "tnpc", "wstate",		\
  "cwp", "cansave", "canrestore", "cleanwin", "otherwin",	\
  "asr16", "asr17", "asr18", "asr19", "asr20", "asr21",		\
  "asr22", "asr23", "asr24", "asr25", "asr26", "asr27",		\
  "asr28", "asr29", "asr30", "asr31",				\
  /* These are here at the end to simplify removing them if we have to.  */ \
  "icc", "xcc", "fcc0", "fcc1", "fcc2", "fcc3"			\
}

/* Register numbers of various important registers.
   Note that some of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and some are "phony" register numbers which are too large
   to be actual register numbers as far as the user is concerned
   but do serve to get the desired values when passed to read_register.  */

#if 0 /* defined in tm-sparc.h, replicated for doc purposes */
#define	G0_REGNUM 0             /* %g0 */
#define	G1_REGNUM 1		/* %g1 */
#define O0_REGNUM 8		/* %o0 */
#define	SP_REGNUM 14		/* Contains address of top of stack, \
				   which is also the bottom of the frame.  */
#define	RP_REGNUM 15		/* Contains return address value, *before* \
				   any windows get switched.  */
#define	O7_REGNUM 15		/* Last local reg not saved on stack frame */
#define	L0_REGNUM 16		/* First local reg that's saved on stack frame
				   rather than in machine registers */
#define	I0_REGNUM 24		/* %i0 */
#define	FP_REGNUM 30		/* Contains address of executing stack frame */
#define	I7_REGNUM 31		/* Last local reg saved on stack frame */
#define	FP0_REGNUM 32		/* Floating point register 0 */
#endif

#define FP_MAX_REGNUM 80	/* 1 + last fp reg number */

/* #undef v8 misc. regs */

#undef Y_REGNUM
#undef PS_REGNUM
#undef WIM_REGNUM
#undef TBR_REGNUM
#undef PC_REGNUM
#undef NPC_REGNUM
#undef FPS_REGNUM
#undef CPS_REGNUM

/* v9 misc. and priv. regs */

#define C0_REGNUM FP_MAX_REGNUM			/* Start of control registers */
#define PC_REGNUM (C0_REGNUM + 0)		/* Current PC */
#define NPC_REGNUM (C0_REGNUM + 1)		/* Next PC */
#define CCR_REGNUM (C0_REGNUM + 2)		/* Condition Code Register (%xcc,%icc) */
#define FSR_REGNUM (C0_REGNUM + 3)		/* Floating Point State */
#define FPRS_REGNUM (C0_REGNUM + 4)		/* Floating Point Registers State */
#define	Y_REGNUM (C0_REGNUM + 5)		/* Temp register for multiplication, etc.  */
#define ASI_REGNUM (C0_REGNUM + 6)		/* Alternate Space Identifier */
#define VER_REGNUM (C0_REGNUM + 7)		/* Version register */
#define TICK_REGNUM (C0_REGNUM + 8)		/* Tick register */
#define PIL_REGNUM (C0_REGNUM + 9)		/* Processor Interrupt Level */
#define PSTATE_REGNUM (C0_REGNUM + 10)		/* Processor State */
#define TSTATE_REGNUM (C0_REGNUM + 11)		/* Trap State */
#define TBA_REGNUM (C0_REGNUM + 12)		/* Trap Base Address */
#define TL_REGNUM (C0_REGNUM + 13)		/* Trap Level */
#define TT_REGNUM (C0_REGNUM + 14)		/* Trap Type */
#define TPC_REGNUM (C0_REGNUM + 15)		/* Trap pc */
#define TNPC_REGNUM (C0_REGNUM + 16)		/* Trap npc */
#define WSTATE_REGNUM (C0_REGNUM + 17)		/* Window State */
#define CWP_REGNUM (C0_REGNUM + 18)		/* Current Window Pointer */
#define CANSAVE_REGNUM (C0_REGNUM + 19)		/* Savable Windows */
#define CANRESTORE_REGNUM (C0_REGNUM + 20)	/* Restorable Windows */
#define CLEANWIN_REGNUM (C0_REGNUM + 21)	/* Clean Windows */
#define OTHERWIN_REGNUM (C0_REGNUM + 22)	/* Other Windows */
#define ASR_REGNUM(n) (C0_REGNUM+(23-16)+(n))	/* Ancillary State Register
						   (n = 16...31) */
#define ICC_REGNUM (C0_REGNUM + 39)		/* 32 bit condition codes */
#define XCC_REGNUM (C0_REGNUM + 40)		/* 64 bit condition codes */
#define FCC0_REGNUM (C0_REGNUM + 41)		/* fp cc reg 0 */
#define FCC1_REGNUM (C0_REGNUM + 42)		/* fp cc reg 1 */
#define FCC2_REGNUM (C0_REGNUM + 43)		/* fp cc reg 2 */
#define FCC3_REGNUM (C0_REGNUM + 44)		/* fp cc reg 3 */

/* Total amount of space needed to store our copies of the machine's
   register state, the array `registers'.
   Some of the registers aren't 64 bits, but it's a lot simpler just to assume
   they all are (since most of them are).  */
#undef  REGISTER_BYTES
#define REGISTER_BYTES (32*8+32*8+45*8)

/* Index within `registers' of the first byte of the space for
   register N.  */
#undef  REGISTER_BYTE
#define REGISTER_BYTE(N) \
  ((N) < 32 ? (N)*8				\
   : (N) < 64 ? 32*8 + ((N)-32)*4		\
   : (N) < C0_REGNUM ? 32*8 + 32*4 + ((N)-64)*8	\
   : 64*8 + ((N)-C0_REGNUM)*8)

/* Say how long (ordinary) registers are.  This is a piece of bogosity
   used in push_word and a few other places; REGISTER_RAW_SIZE is the
   real way to know how big a register is.  */

#undef  REGISTER_SIZE
#define REGISTER_SIZE 8

/* Number of bytes of storage in the actual machine representation
   for register N.  */

#undef  REGISTER_RAW_SIZE
#define REGISTER_RAW_SIZE(N) \
  ((N) < 32 ? 8 : (N) < 64 ? 4 : 8)

/* Number of bytes of storage in the program's representation
   for register N.  */

#undef  REGISTER_VIRTUAL_SIZE
#define REGISTER_VIRTUAL_SIZE(N) \
  ((N) < 32 ? 8 : (N) < 64 ? 4 : 8)

/* Largest value REGISTER_RAW_SIZE can have.  */
/* tm-sparc.h defines this as 8, but play it safe.  */

#undef  MAX_REGISTER_RAW_SIZE
#define MAX_REGISTER_RAW_SIZE 8

/* Largest value REGISTER_VIRTUAL_SIZE can have.  */
/* tm-sparc.h defines this as 8, but play it safe.  */

#undef  MAX_REGISTER_VIRTUAL_SIZE
#define MAX_REGISTER_VIRTUAL_SIZE 8

/* Return the GDB type object for the "standard" data type
   of data in register N.  */

#undef  REGISTER_VIRTUAL_TYPE
#define REGISTER_VIRTUAL_TYPE(N) \
 ((N) < 32 ? builtin_type_long_long \
  : (N) < 64 ? builtin_type_float \
  : (N) < 80 ? builtin_type_double \
  : builtin_type_long_long)

/* We use to support both 32 bit and 64 bit pointers.
   We can't anymore because TARGET_PTR_BIT must now be a constant.  */
#undef  TARGET_PTR_BIT
#define TARGET_PTR_BIT 64

/* Does the specified function use the "struct returning" convention
   or the "value returning" convention?  The "value returning" convention
   almost invariably returns the entire value in registers.  The
   "struct returning" convention often returns the entire value in
   memory, and passes a pointer (out of or into the function) saying
   where the value (is or should go).

   Since this sometimes depends on whether it was compiled with GCC,
   this is also an argument.  This is used in call_function to build a
   stack, and in value_being_returned to print return values.

   On sparc64, all structs are returned via a pointer.  */

#undef  USE_STRUCT_CONVENTION
#define USE_STRUCT_CONVENTION(gcc_p, type) 1

/* Store the address of the place in which to copy the structure the
   subroutine will return.  This is called from call_function. */
/* FIXME: V9 uses %o0 for this.  */

#undef  STORE_STRUCT_RETURN
#define STORE_STRUCT_RETURN(ADDR, SP) \
  { target_write_memory ((SP)+(16*8), (char *)&(ADDR), 8); }

/* Return number of bytes at start of arglist that are not really args.  */

#undef  FRAME_ARGS_SKIP
#define FRAME_ARGS_SKIP 136

/* We need two arguments (in general) to the "info frame" command.
   Note that the definition of this macro implies that there exists a
   function "setup_arbitrary_frame" in sparc-tdep.c */

#undef  SETUP_ARBITRARY_FRAME		/*FIXME*/
#undef  FRAME_SPECIFICATION_DYADIC
#define FRAME_SPECIFICATION_DYADIC

/* To print every pair of float registers as a double, we use this hook.
   We also print the condition code registers in a readable format
   (FIXME: can expand this to all control regs).  */

#undef 	PRINT_REGISTER_HOOK
#define	PRINT_REGISTER_HOOK(regno)	\
  sparc_print_register_hook (regno)

/* Offsets into jmp_buf.
   FIXME: This was borrowed from the v8 stuff and will probably have to change
   for v9.  */

#define JB_ELEMENT_SIZE 8	/* Size of each element in jmp_buf */

#define JB_ONSSTACK 0
#define JB_SIGMASK 1
#define JB_SP 2
#define JB_PC 3
#define JB_NPC 4
#define JB_PSR 5
#define JB_G1 6
#define JB_O0 7
#define JB_WBCNT 8

/* Figure out where the longjmp will land.  We expect that we have just entered
   longjmp and haven't yet setup the stack frame, so the args are still in the
   output regs.  %o0 (O0_REGNUM) points at the jmp_buf structure from which we
   extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
   This routine returns true on success */

extern int
get_longjmp_target PARAMS ((CORE_ADDR *));

#define GET_LONGJMP_TARGET(ADDR) get_longjmp_target(ADDR)

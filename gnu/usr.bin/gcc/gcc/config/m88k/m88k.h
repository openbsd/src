/* Definitions of target machine for GNU compiler for
   Motorola m88100 in an 88open OCS/BCS environment.
   Copyright (C) 1988, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
   2001, 2002 Free Software Foundation, Inc.
   Contributed by Michael Tiemann (tiemann@cygnus.com).
   Currently maintained by (gcc@dg-rtp.dg.com)

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* The m88100 port of GNU CC adheres to the various standards from 88open.
   These documents are available by writing:

	88open Consortium Ltd.
	100 Homeland Court, Suite 800
	San Jose, CA  95112
	(408) 436-6600

   In brief, the current standards are:

   Binary Compatibility Standard, Release 1.1A, May 1991
	This provides for portability of application-level software at the
	executable level for AT&T System V Release 3.2.

   Object Compatibility Standard, Release 1.1A, May 1991
	This provides for portability of application-level software at the
	object file and library level for C, Fortran, and Cobol, and again,
	largely for SVR3.

   Under development are standards for AT&T System V Release 4, based on the
   [generic] System V Application Binary Interface from AT&T.  These include:

   System V Application Binary Interface, Motorola 88000 Processor Supplement
	Another document from AT&T for SVR4 specific to the m88100.
	Available from Prentice Hall.

   System V Application Binary Interface, Motorola 88000 Processor Supplement,
   Release 1.1, Draft H, May 6, 1991
	A proposed update to the AT&T document from 88open.

   System V ABI Implementation Guide for the M88000 Processor,
   Release 1.0, January 1991
	A companion ABI document from 88open.  */

/* External types used.  */

/* What instructions are needed to manufacture an integer constant.  */
enum m88k_instruction {
  m88k_zero,
  m88k_or,
  m88k_subu,
  m88k_or_lo16,
  m88k_or_lo8,
  m88k_set,
  m88k_oru_hi16,
  m88k_oru_or
};

/* Which processor to schedule for.  The elements of the enumeration
   must match exactly the cpu attribute in the m88k.md machine description. */

enum processor_type {
  PROCESSOR_M88100,
  PROCESSOR_M88110,
  PROCESSOR_M88000
};

/* Recast the cpu class to be the cpu attribute.  */
#define m88k_cpu_attr ((enum attr_cpu)m88k_cpu)

/* External variables/functions defined in m88k.c.  */

extern char m88k_volatile_code;

extern int m88k_fp_offset;
extern int m88k_stack_size;
extern int m88k_case_index;

extern struct rtx_def *m88k_compare_reg;
extern struct rtx_def *m88k_compare_op0;
extern struct rtx_def *m88k_compare_op1;

extern enum processor_type m88k_cpu;

/* external variables defined elsewhere in the compiler */

extern int target_flags;			/* -m compiler switches */

/*** Controlling the Compilation Driver, `gcc' ***/
/* Show we can debug even without a frame pointer.  */
#define CAN_DEBUG_WITHOUT_FP

/* If -m88100 is in effect, add -D__m88100__; similarly for -m88110.
   Here, the CPU_DEFAULT is assumed to be -m88100.  */
#undef	CPP_SPEC
#define	CPP_SPEC "%{!m88000:%{!m88100:%{m88110:-D__m88110__}}} \
		  %{!m88000:%{!m88110:-D__m88100__}}"

/*** Run-time Target Specification ***/

/* Names to predefine in the preprocessor for this target machine.  */
#define CPP_PREDEFINES "-Dm88000 -Dm88k -Dunix -D__CLASSIFY_TYPE__=2"

#define TARGET_VERSION fprintf (stderr, " (%s)", VERSION_INFO)

#define VERSION_INFO	"m88k"

/* Run-time compilation parameters selecting different hardware subsets.  */

/* Macro to define tables used to set the flags.
   This is a list in braces of pairs in braces,
   each pair being { "NAME", VALUE }
   where VALUE is the bits to set or minus the bits to clear.
   An empty string NAME is used to identify the default VALUE.  */

#define MASK_88100		0x00000001 /* Target m88100 */
#define MASK_88110		0x00000002 /* Target m88110 */
#define MASK_88000 		(MASK_88100 | MASK_88110)

#define MASK_TRAP_LARGE_SHIFT	0x00000100 /* Trap if shift not <= 31 */
#define MASK_HANDLE_LARGE_SHIFT	0x00000200 /* Handle shift count >= 32 */
#define MASK_CHECK_ZERO_DIV	0x00000400 /* Check for int div. by 0 */
#define MASK_USE_DIV		0x00000800 /* No signed div. checks */
#define MASK_NO_SERIALIZE_VOLATILE 0x00001000 /* Serialize volatile refs */
#define MASK_MEMCPY		0x00002000 /* Always use memcpy for movstr */
#define MASK_EITHER_LARGE_SHIFT	(MASK_TRAP_LARGE_SHIFT | \
				 MASK_HANDLE_LARGE_SHIFT)
#define MASK_OMIT_LEAF_FRAME_POINTER 0x00004000 /* omit leaf frame pointers */


#define TARGET_88100   		 ((target_flags & MASK_88000) == MASK_88100)
#define TARGET_88110		 ((target_flags & MASK_88000) == MASK_88110)
#define TARGET_88000		 ((target_flags & MASK_88000) == MASK_88000)

#define TARGET_TRAP_LARGE_SHIFT   (target_flags & MASK_TRAP_LARGE_SHIFT)
#define TARGET_HANDLE_LARGE_SHIFT (target_flags & MASK_HANDLE_LARGE_SHIFT)
#define TARGET_CHECK_ZERO_DIV	  (target_flags & MASK_CHECK_ZERO_DIV)
#define	TARGET_USE_DIV		  (target_flags & MASK_USE_DIV)
#define TARGET_SERIALIZE_VOLATILE (!(target_flags & MASK_NO_SERIALIZE_VOLATILE))
#define TARGET_MEMCPY		  (target_flags & MASK_MEMCPY)

#define TARGET_EITHER_LARGE_SHIFT (target_flags & MASK_EITHER_LARGE_SHIFT)
#define TARGET_OMIT_LEAF_FRAME_POINTER (target_flags & MASK_OMIT_LEAF_FRAME_POINTER)

#define TARGET_DEFAULT	(MASK_CHECK_ZERO_DIV)
#define CPU_DEFAULT MASK_88100

#define TARGET_SWITCHES							\
{									\
  { "88110",				 MASK_88110,			\
    N_("Generate code for a 88110 processor") },			\
  { "88100",				 MASK_88100,			\
    N_("Generate code for a 88100 processor") },			\
  { "88000",			         MASK_88000,			\
    N_("Generate code compatible with both 88100 and 88110 processors") }, \
  { "trap-large-shift",			 MASK_TRAP_LARGE_SHIFT,		\
    N_("Add code to trap on logical shift counts larger than 31") },	\
  { "handle-large-shift",		 MASK_HANDLE_LARGE_SHIFT,	\
    N_("Add code to handle logical shift counts larger than 31") },	\
  { "check-zero-division",		 MASK_CHECK_ZERO_DIV,		\
    N_("Add code to trap on integer divide by zero") },			\
  { "no-check-zero-division",		-MASK_CHECK_ZERO_DIV,		\
    N_("Do not add code to trap on integer divide by zero") },		\
  { "use-div-instruction",		 MASK_USE_DIV,			\
    N_("Use the \"div\" instruction for signed integer divide") },	\
  { "no-serialize-volatile",		 MASK_NO_SERIALIZE_VOLATILE,	\
    N_("Do not force serialization on volatile memory access") },	\
  { "serialize-volatile",		-MASK_NO_SERIALIZE_VOLATILE,	\
    N_("Force serialization on volatile memory access") },		\
  { "omit-leaf-frame-pointer",		 MASK_OMIT_LEAF_FRAME_POINTER,	\
    N_("Do not save the frame pointer in leaf functions") },		\
  { "no-omit-leaf-frame-pointer",	-MASK_OMIT_LEAF_FRAME_POINTER,	\
    N_("Save the frame pointer in leaf functions") },			\
  { "memcpy",				 MASK_MEMCPY,			\
    N_("Force all memory copies to use memcpy()") },			\
  { "no-memcpy",			-MASK_MEMCPY,			\
    N_("Allow the use of specific memory copy code") },			\
  SUBTARGET_SWITCHES							\
  /* Default switches */						\
  { "",				 	TARGET_DEFAULT,			\
    NULL },								\
}

#define SUBTARGET_SWITCHES

/* Do any checking or such that is needed after processing the -m switches.  */
#define OVERRIDE_OPTIONS m88k_override_options ()

/*** Storage Layout ***/

/* Sizes in bits of the various types.  */
#define SHORT_TYPE_SIZE		16
#define INT_TYPE_SIZE		32
#define LONG_TYPE_SIZE		32
#define LONG_LONG_TYPE_SIZE	64
#define FLOAT_TYPE_SIZE		32
#define	DOUBLE_TYPE_SIZE	64
#define LONG_DOUBLE_TYPE_SIZE	64

/* Define this if most significant bit is lowest numbered
   in instructions that operate on numbered bit-fields.
   Somewhat arbitrary.  It matches the bit field patterns.  */
#define BITS_BIG_ENDIAN 1

/* Define this if most significant byte of a word is the lowest numbered.
   That is true on the m88000.  */
#define BYTES_BIG_ENDIAN 1

/* Define this if most significant word of a multiword number is the lowest
   numbered.
   For the m88000 we can decide arbitrarily since there are no machine
   instructions for them.  */
#define WORDS_BIG_ENDIAN 1

/* Width of a word, in units (bytes).  */
#define UNITS_PER_WORD 4

/* Allocation boundary (in *bits*) for storing arguments in argument list.  */
#define PARM_BOUNDARY 32

/* Largest alignment for stack parameters (if greater than PARM_BOUNDARY).  */
#define MAX_PARM_BOUNDARY 64

/* Boundary (in *bits*) on which stack pointer should be aligned.  */
#define STACK_BOUNDARY 128

/* Allocation boundary (in *bits*) for the code of a function.  */
#define FUNCTION_BOUNDARY 32

/* No data type wants to be aligned rounder than this.  */
#define BIGGEST_ALIGNMENT 64

/* The best alignment to use in cases where we have a choice.  */
#define FASTEST_ALIGNMENT (TARGET_88100 ? 32 : 64)

/* Make strings 4/8 byte aligned so strcpy from constants will be faster.  */
#define CONSTANT_ALIGNMENT(EXP, ALIGN)  \
  ((TREE_CODE (EXP) == STRING_CST	\
    && (ALIGN) < FASTEST_ALIGNMENT)	\
   ? FASTEST_ALIGNMENT : (ALIGN))

/* Make arrays of chars 4/8 byte aligned for the same reasons.  */
#define DATA_ALIGNMENT(TYPE, ALIGN)		\
  (TREE_CODE (TYPE) == ARRAY_TYPE		\
   && TYPE_MODE (TREE_TYPE (TYPE)) == QImode	\
   && (ALIGN) < FASTEST_ALIGNMENT ? FASTEST_ALIGNMENT : (ALIGN))

/* Alignment of field after `int : 0' in a structure.
   Ignored with PCC_BITFIELD_TYPE_MATTERS.  */
/* #define EMPTY_FIELD_BOUNDARY 8 */

/* Every structure's size must be a multiple of this.  */
#define STRUCTURE_SIZE_BOUNDARY 8

/* Set this nonzero if move instructions will actually fail to work
   when given unaligned data.  */
#define STRICT_ALIGNMENT 1

/* A bit-field declared as `int' forces `int' alignment for the struct.  */
#define PCC_BITFIELD_TYPE_MATTERS 1

/* Maximum size (in bits) to use for the largest integral type that
   replaces a BLKmode type. */
/* #define MAX_FIXED_MODE_SIZE 0 */

/*** Register Usage ***/

/* No register prefixes by default.  Will be overriden if necessary.  */
#undef REGISTER_PREFIX

/* Number of actual hardware registers.
   The hardware registers are assigned numbers for the compiler
   from 0 to just below FIRST_PSEUDO_REGISTER.
   All registers that the compiler knows about must be given numbers,
   even those that are not normally considered general registers.

   The m88100 has a General Register File (GRF) of 32 32-bit registers.
   The m88110 adds an Extended Register File (XRF) of 32 80-bit registers.  */
#define FIRST_PSEUDO_REGISTER 64
#define FIRST_EXTENDED_REGISTER 32

/*  General notes on extended registers, their use and misuse.

    Possible good uses:

    spill area instead of memory.
      -waste if only used once

    floating point calculations
      -probably a waste unless we have run out of general purpose registers

    freeing up general purpose registers
      -e.g. may be able to have more loop invariants if floating
       point is moved into extended registers.


    I've noticed wasteful moves into and out of extended registers; e.g. a load
    into x21, then inside a loop a move into r24, then r24 used as input to
    an fadd.  Why not just load into r24 to begin with?  Maybe the new cse.c
    will address this.  This wastes a move, but the load,store and move could
    have been saved had extended registers been used throughout.
    E.g. in the code following code, if z and xz are placed in extended
    registers, there is no need to save preserve registers.

	long c=1,d=1,e=1,f=1,g=1,h=1,i=1,j=1,k;

	double z=0,xz=4.5;

	foo(a,b)
	long a,b;
	{
	  while (a < b)
	    {
	      k = b + c + d + e + f + g + h + a + i + j++;
	      z += xz;
	      a++;
	    }
	  printf("k= %d; z=%f;\n", k, z);
	}

    I've found that it is possible to change the constraints (putting * before
    the 'r' constraints int the fadd.ddd instruction) and get the entire
    addition and store to go into extended registers.  However, this also
    forces simple addition and return of floating point arguments to a
    function into extended registers.  Not the correct solution.

    Found the following note in local-alloc.c which may explain why I can't
    get both registers to be in extended registers since two are allocated in
    local-alloc and one in global-alloc.  Doesn't explain (I don't believe)
    why an extended register is used instead of just using the preserve
    register.

	from local-alloc.c:
	We have provision to exempt registers, even when they are contained
	within the block, that can be tied to others that are not contained in it.
	This is so that global_alloc could process them both and tie them then.
	But this is currently disabled since tying in global_alloc is not
	yet implemented.

    The explanation of why the preserved register is not used is as follows,
    I believe.  The registers are being allocated in order.  Tying is not
    done so efficiently, so when it comes time to do the first allocation,
    there are no registers left to use without spilling except extended
    registers.  Then when the next pseudo register needs a hard reg, there
    are still no registers to be had for free, but this one must be a GRF
    reg instead of an extended reg, so a preserve register is spilled.  Thus
    the move from extended to GRF is necessitated.  I do not believe this can
    be 'fixed' through the files in config/m88k.

    gcc seems to sometimes make worse use of register allocation -- not counting
    moves -- whenever extended registers are present.  For example in the
    whetstone, the simple for loop (slightly modified)
      for(i = 1; i <= n1; i++)
	{
	  x1 = (x1 + x2 + x3 - x4) * t;
	  x2 = (x1 + x2 - x3 + x4) * t;
	  x3 = (x1 - x2 + x3 + x4) * t;
	  x4 = (x1 + x2 + x3 + x4) * t;
	}
    in general loads the high bits of the addresses of x2-x4 and i into registers
    outside the loop.  Whenever extended registers are used, it loads all of
    these inside the loop. My conjecture is that since the 88110 has so many
    registers, and gcc makes no distinction at this point -- just that they are
    not fixed, that in loop.c it believes it can expect a number of registers
    to be available.  Then it allocates 'too many' in local-alloc which causes
    problems later.  'Too many' are allocated because a large portion of the
    registers are extended registers and cannot be used for certain purposes
    ( e.g. hold the address of a variable).  When this loop is compiled on its
    own, the problem does not occur.  I don't know the solution yet, though it
    is probably in the base sources.  Possibly a different way to calculate
    "threshold".  */

/* 1 for registers that have pervasive standard uses and are not available
   for the register allocator.  Registers r14-r25 and x22-x29 are expected
   to be preserved across function calls.

   On the 88000, the standard uses of the General Register File (GRF) are:
   Reg 0	= Pseudo argument pointer (hardware fixed to 0).
   Reg 1	= Subroutine return pointer (hardware).
   Reg 2-9	= Parameter registers (OCS).
   Reg 10	= OCS reserved temporary.
   Reg 11	= Static link if needed [OCS reserved temporary].
   Reg 12	= Address of structure return (OCS).
   Reg 13	= OCS reserved temporary.
   Reg 14-25	= Preserved register set.
   Reg 26-29	= Reserved by OCS and ABI.
   Reg 30	= Frame pointer (Common use).
   Reg 31	= Stack pointer.

   The following follows the current 88open UCS specification for the
   Extended Register File (XRF):
   Reg 32       = x0		Always equal to zero
   Reg 33-53	= x1-x21	Temporary registers (Caller Save)
   Reg 54-61	= x22-x29	Preserver registers (Callee Save)
   Reg 62-63	= x30-x31	Reserved for future ABI use.

   Note:  The current 88110 extended register mapping is subject to change.
	  The bias towards caller-save registers is based on the
	  presumption that memory traffic can potentially be reduced by
	  allowing the "caller" to save only that part of the register
	  which is actually being used.  (i.e. don't do a st.x if a st.d
	  is sufficient).  Also, in scientific code (a.k.a. Fortran), the
	  large number of variables defined in common blocks may require
	  that almost all registers be saved across calls anyway.  */

#define FIXED_REGISTERS \
 {1, 0, 0, 0,  0, 0, 0, 0,   0, 0, 0, 0,  0, 0, 0, 0, \
  0, 0, 0, 0,  0, 0, 0, 0,   0, 0, 1, 1,  1, 1, 1, 1, \
  1, 0, 0, 0,  0, 0, 0, 0,   0, 0, 0, 0,  0, 0, 0, 0, \
  0, 0, 0, 0,  0, 0, 0, 0,   0, 0, 0, 0,  0, 0, 1, 1}

/* 1 for registers not available across function calls.
   These must include the FIXED_REGISTERS and also any
   registers that can be used without being saved.
   The latter must include the registers where values are returned
   and the register where structure-value addresses are passed.
   Aside from that, you can include as many other registers as you like.  */

#define CALL_USED_REGISTERS \
 {1, 1, 1, 1,  1, 1, 1, 1,   1, 1, 1, 1,  1, 1, 0, 0, \
  0, 0, 0, 0,  0, 0, 0, 0,   0, 0, 1, 1,  1, 1, 1, 1, \
  1, 1, 1, 1,  1, 1, 1, 1,   1, 1, 1, 1,  1, 1, 1, 1, \
  1, 1, 1, 1,  1, 1, 0, 0,   0, 0, 0, 0,  0, 0, 1, 1}

/* Macro to conditionally modify fixed_regs/call_used_regs.  */
#define CONDITIONAL_REGISTER_USAGE			\
  {							\
    if (! TARGET_88110)					\
      {							\
	register int i;					\
	  for (i = FIRST_EXTENDED_REGISTER; i < FIRST_PSEUDO_REGISTER; i++) \
	    {						\
	      fixed_regs[i] = 1;			\
	      call_used_regs[i] = 1;			\
	    }						\
      }							\
    if (flag_pic)					\
      {							\
	fixed_regs[PIC_OFFSET_TABLE_REGNUM] = 1;	\
	call_used_regs[PIC_OFFSET_TABLE_REGNUM] = 1;	\
      }							\
  }

/* True if register is an extended register.  */
#define XRF_REGNO_P(N) ((N) < FIRST_PSEUDO_REGISTER && (N) >= FIRST_EXTENDED_REGISTER)
 
/* Return number of consecutive hard regs needed starting at reg REGNO
   to hold something of mode MODE.
   This is ordinarily the length in words of a value of mode MODE
   but can be less for certain modes in special long registers.

   On the m88000, GRF registers hold 32-bits and XRF registers hold 80-bits.
   An XRF register can hold any mode, but two GRF registers are required
   for larger modes.  */
#define HARD_REGNO_NREGS(REGNO, MODE)					\
  (XRF_REGNO_P (REGNO)                                                 \
   ? 1 : ((GET_MODE_SIZE (MODE) + UNITS_PER_WORD - 1) / UNITS_PER_WORD))

/* Value is 1 if hard register REGNO can hold a value of machine-mode MODE.

   For double integers, we never put the value into an odd register so that
   the operators don't run into the situation where the high part of one of
   the inputs is the low part of the result register.  (It's ok if the output
   registers are the same as the input registers.)  The XRF registers can
   hold all modes, but only DF and SF modes can be manipulated in these
   registers.  The compiler should be allowed to use these as a fast spill
   area.  */
#define HARD_REGNO_MODE_OK(REGNO, MODE)					\
  (XRF_REGNO_P (REGNO)							\
    ? (TARGET_88110 && GET_MODE_CLASS (MODE) == MODE_FLOAT)             \
    : (((MODE) != DImode && (MODE) != DFmode && (MODE) != DCmode)	\
       || ((REGNO) & 1) == 0))

/* Value is 1 if it is a good idea to tie two pseudo registers
   when one has mode MODE1 and one has mode MODE2.
   If HARD_REGNO_MODE_OK could produce different values for MODE1 and MODE2,
   for any hard reg, then this must be 0 for correct output.  */
#define MODES_TIEABLE_P(MODE1, MODE2) \
  (((MODE1) == DFmode || (MODE1) == DCmode || (MODE1) == DImode \
    || (TARGET_88110 && GET_MODE_CLASS (MODE1) == MODE_FLOAT)) \
   == ((MODE2) == DFmode || (MODE2) == DCmode || (MODE2) == DImode \
       || (TARGET_88110 && GET_MODE_CLASS (MODE2) == MODE_FLOAT)))

/* Specify the registers used for certain standard purposes.
   The values of these macros are register numbers.  */

/* the m88000 pc isn't overloaded on a register that the compiler knows about.  */
/* #define PC_REGNUM  */

/* Register to use for pushing function arguments.  */
#define STACK_POINTER_REGNUM 31

/* Base register for access to local variables of the function.  */
#define FRAME_POINTER_REGNUM 30

/* Base register for access to arguments of the function.  */
#define ARG_POINTER_REGNUM 0

/* Register used in cases where a temporary is known to be safe to use.  */
#define TEMP_REGNUM 10

/* Register in which static-chain is passed to a function.  */
#define STATIC_CHAIN_REGNUM 11

/* Register in which address to store a structure value
   is passed to a function.  */
#define STRUCT_VALUE_REGNUM 12

/* Register to hold the addressing base for position independent
   code access to data items.  */
#define PIC_OFFSET_TABLE_REGNUM (flag_pic ? 25 : INVALID_REGNUM)

/* Order in which registers are preferred (most to least).  Use temp
   registers, then param registers top down.  Preserve registers are
   top down to maximize use of double memory ops for register save.
   The 88open reserved registers (r26-r29 and x30-x31) may commonly be used
   in most environments with the -fcall-used- or -fcall-saved- options.  */
#define REG_ALLOC_ORDER		  \
 {				  \
  13, 12, 11, 10, 29, 28, 27, 26, \
  62, 63,  9,  8,  7,  6,  5,  4, \
   3,  2,  1, 53, 52, 51, 50, 49, \
  48, 47, 46, 45, 44, 43, 42, 41, \
  40, 39, 38, 37, 36, 35, 34, 33, \
  25, 24, 23, 22, 21, 20, 19, 18, \
  17, 16, 15, 14, 61, 60, 59, 58, \
  57, 56, 55, 54, 30, 31,  0, 32}

/* Order for leaf functions.  */
#define REG_LEAF_ALLOC_ORDER	  \
 {				  \
   9,  8,  7,  6, 13, 12, 11, 10, \
  29, 28, 27, 26, 62, 63,  5,  4, \
   3,  2,  0, 53, 52, 51, 50, 49, \
  48, 47, 46, 45, 44, 43, 42, 41, \
  40, 39, 38, 37, 36, 35, 34, 33, \
  25, 24, 23, 22, 21, 20, 19, 18, \
  17, 16, 15, 14, 61, 60, 59, 58, \
  57, 56, 55, 54, 30, 31,  1, 32}

/* Switch between the leaf and non-leaf orderings.  The purpose is to avoid
   write-over scoreboard delays between caller and callee.  */
#define ORDER_REGS_FOR_LOCAL_ALLOC				\
{								\
  static const int leaf[] = REG_LEAF_ALLOC_ORDER;		\
  static const int nonleaf[] = REG_ALLOC_ORDER;			\
								\
  memcpy (reg_alloc_order, regs_ever_live[1] ? nonleaf : leaf,	\
	  FIRST_PSEUDO_REGISTER * sizeof (int));		\
}

/*** Register Classes ***/

/* Define the classes of registers for register constraints in the
   machine description.  Also define ranges of constants.

   One of the classes must always be named ALL_REGS and include all hard regs.
   If there is more than one class, another class must be named NO_REGS
   and contain no registers.

   The name GENERAL_REGS must be the name of a class (or an alias for
   another name such as ALL_REGS).  This is the class of registers
   that is allowed by "g" or "r" in a register constraint.
   Also, registers outside this class are allocated only when
   instructions express preferences for them.

   The classes must be numbered in nondecreasing order; that is,
   a larger-numbered class must never be contained completely
   in a smaller-numbered class.

   For any two classes, it is very desirable that there be another
   class that represents their union.  */

/* The m88000 hardware has two kinds of registers.  In addition, we denote
   the arg pointer as a separate class.  */

enum reg_class { NO_REGS, AP_REG, XRF_REGS, GENERAL_REGS, AGRF_REGS,
		 XGRF_REGS, ALL_REGS, LIM_REG_CLASSES };

#define N_REG_CLASSES (int) LIM_REG_CLASSES

/* Give names of register classes as strings for dump file.   */
#define REG_CLASS_NAMES {"NO_REGS", "AP_REG", "XRF_REGS", "GENERAL_REGS", \
			 "AGRF_REGS", "XGRF_REGS", "ALL_REGS" }

/* Define which registers fit in which classes.
   This is an initializer for a vector of HARD_REG_SET
   of length N_REG_CLASSES.  */
#define REG_CLASS_CONTENTS {{0x00000000, 0x00000000},	\
			    {0x00000001, 0x00000000},	\
			    {0x00000000, 0xffffffff},	\
			    {0xfffffffe, 0x00000000},	\
			    {0xffffffff, 0x00000000},	\
			    {0xfffffffe, 0xffffffff},	\
			    {0xffffffff, 0xffffffff}}

/* The same information, inverted:
   Return the class number of the smallest class containing
   reg number REGNO.  This could be a conditional expression
   or could index an array.  */
#define REGNO_REG_CLASS(REGNO) \
  ((REGNO) ? ((REGNO) < 32 ? GENERAL_REGS : XRF_REGS) : AP_REG)

/* The class value for index registers, and the one for base regs.  */
#define BASE_REG_CLASS AGRF_REGS
#define INDEX_REG_CLASS GENERAL_REGS

/* Get reg_class from a letter such as appears in the machine description.
   For the 88000, the following class/letter is defined for the XRF:
	x - Extended register file  */
#define REG_CLASS_FROM_LETTER(C) 	\
   (((C) == 'x') ? XRF_REGS : NO_REGS)

/* Macros to check register numbers against specific register classes.
   These assume that REGNO is a hard or pseudo reg number.
   They give nonzero only if REGNO is a hard reg of the suitable class
   or a pseudo reg currently allocated to a suitable hard reg.
   Since they use reg_renumber, they are safe only once reg_renumber
   has been allocated, which happens in local-alloc.c.  */
#define REGNO_OK_FOR_BASE_P(REGNO)				\
  ((REGNO) < FIRST_EXTENDED_REGISTER				\
   || (unsigned) reg_renumber[REGNO] < FIRST_EXTENDED_REGISTER)
#define REGNO_OK_FOR_INDEX_P(REGNO)				\
  (((REGNO) && (REGNO) < FIRST_EXTENDED_REGISTER)		\
   || (unsigned) reg_renumber[REGNO] < FIRST_EXTENDED_REGISTER)

/* Given an rtx X being reloaded into a reg required to be
   in class CLASS, return the class of reg to actually use.
   In general this is just CLASS; but on some machines
   in some cases it is preferable to use a more restrictive class.
   Double constants should be in a register iff they can be made cheaply.  */
#define PREFERRED_RELOAD_CLASS(X,CLASS)	\
   (CONSTANT_P (X) && ((CLASS) == XRF_REGS) ? NO_REGS : (CLASS))

/* Return the register class of a scratch register needed to load IN
   into a register of class CLASS in MODE.  On the m88k, when PIC, we
   need a temporary when loading some addresses into a register.  */
#define SECONDARY_INPUT_RELOAD_CLASS(CLASS, MODE, IN)		\
  ((flag_pic							\
    && GET_CODE (IN) == CONST					\
    && GET_CODE (XEXP (IN, 0)) == PLUS				\
    && GET_CODE (XEXP (XEXP (IN, 0), 0)) == CONST_INT		\
    && ! SMALL_INT (XEXP (XEXP (IN, 0), 1))) ? GENERAL_REGS : NO_REGS)

/* Return the maximum number of consecutive registers
   needed to represent mode MODE in a register of class CLASS.  */
#define CLASS_MAX_NREGS(CLASS, MODE) \
  ((((CLASS) == XRF_REGS) ? 1 \
    : ((GET_MODE_SIZE (MODE) + UNITS_PER_WORD - 1) / UNITS_PER_WORD)))

/* Letters in the range `I' through `P' in a register constraint string can
   be used to stand for particular ranges of immediate operands.  The C
   expression is true iff C is a known letter and VALUE is appropriate for
   that letter.

   For the m88000, the following constants are used:
   `I' requires a non-negative 16-bit value.
   `J' requires a non-positive 16-bit value.
   `K' requires a non-negative value < 32.
   `L' requires a constant with only the upper 16-bits set.
   `M' requires constant values that can be formed with `set'.
   `N' requires a negative value.
   `O' requires zero.
   `P' requires a non-negative value.  */

/* Quick tests for certain values.  */
#define SMALL_INT(X) (SMALL_INTVAL (INTVAL (X)))
#define SMALL_INTVAL(I) ((unsigned HOST_WIDE_INT) (I) < 0x10000)
#define ADD_INT(X) (ADD_INTVAL (INTVAL (X)))
#define ADD_INTVAL(I) ((unsigned HOST_WIDE_INT) (I) + 0xffff < 0x1ffff)
#define POWER_OF_2(I) ((I) && POWER_OF_2_or_0(I))
#define POWER_OF_2_or_0(I) (((I) & ((unsigned HOST_WIDE_INT)(I) - 1)) == 0)

#define CONST_OK_FOR_LETTER_P(VALUE, C)			\
  ((C) == 'I' ? SMALL_INTVAL (VALUE)			\
   : (C) == 'J' ? SMALL_INTVAL (-(VALUE))		\
   : (C) == 'K' ? (unsigned HOST_WIDE_INT)(VALUE) < 32	\
   : (C) == 'L' ? ((VALUE) & 0xffff) == 0		\
   : (C) == 'M' ? integer_ok_for_set (VALUE)		\
   : (C) == 'N' ? (VALUE) < 0				\
   : (C) == 'O' ? (VALUE) == 0				\
   : (C) == 'P' ? (VALUE) >= 0				\
   : 0)

/* Similar, but for floating constants, and defining letters G and H.
   Here VALUE is the CONST_DOUBLE rtx itself.  For the m88000, the
   constraints are:  `G' requires zero, and `H' requires one or two.  */
#define CONST_DOUBLE_OK_FOR_LETTER_P(VALUE, C)				\
  ((C) == 'G' ? (CONST_DOUBLE_HIGH (VALUE) == 0				\
		 && CONST_DOUBLE_LOW (VALUE) == 0)			\
   : 0)

/* Letters in the range `Q' through `U' in a register constraint string
   may be defined in a machine-dependent fashion to stand for arbitrary
   operand types.

   For the m88k, `Q' handles addresses in a call context.  */

#define EXTRA_CONSTRAINT(OP, C)				\
  ((C) == 'Q' ? symbolic_address_p (OP) : 0)

/*** Describing Stack Layout ***/

/* Define this if pushing a word on the stack moves the stack pointer
   to a smaller address.  */
#define STACK_GROWS_DOWNWARD

/* Define this if the addresses of local variable slots are at negative
   offsets from the frame pointer.  */
/* #define FRAME_GROWS_DOWNWARD */

/* Offset from the frame pointer to the first local variable slot to be
   allocated. For the m88k, the debugger wants the return address (r1)
   stored at location r30+4, and the previous frame pointer stored at
   location r30.  */
#define STARTING_FRAME_OFFSET 8

/* If we generate an insn to push BYTES bytes, this says how many the
   stack pointer really advances by.  The m88k has no push instruction.  */
/*  #define PUSH_ROUNDING(BYTES) */

/* If defined, the maximum amount of space required for outgoing arguments
   will be computed and placed into the variable
   `current_function_outgoing_args_size'.  No space will be pushed
   onto the stack for each call; instead, the function prologue should
   increase the stack frame size by this amount.  */
#define ACCUMULATE_OUTGOING_ARGS 1

/* Offset from the stack pointer register to the first location at which
   outgoing arguments are placed.  Use the default value zero.  */
/* #define STACK_POINTER_OFFSET 0 */

/* Offset of first parameter from the argument pointer register value.
   Using an argument pointer, this is 0 for the m88k.  GCC knows
   how to eliminate the argument pointer references if necessary.  */
#define FIRST_PARM_OFFSET(FNDECL) 0

/* Define this if functions should assume that stack space has been
   allocated for arguments even when their values are passed in
   registers.

   The value of this macro is the size, in bytes, of the area reserved for
   arguments passed in registers.

   This space can either be allocated by the caller or be a part of the
   machine-dependent stack frame: `OUTGOING_REG_PARM_STACK_SPACE'
   says which.  */
/* #undef REG_PARM_STACK_SPACE(FNDECL) */

/* Define this macro if REG_PARM_STACK_SPACE is defined but stack
   parameters don't skip the area specified by REG_PARM_STACK_SPACE.
   Normally, when a parameter is not passed in registers, it is placed on
   the stack beyond the REG_PARM_STACK_SPACE area.  Defining this macro
   suppresses this behavior and causes the parameter to be passed on the
   stack in its natural location.  */
/* #undef STACK_PARMS_IN_REG_PARM_AREA */

/* Define this if it is the responsibility of the caller to allocate the
   area reserved for arguments passed in registers.  If
   `ACCUMULATE_OUTGOING_ARGS' is also defined, the only effect of this
   macro is to determine whether the space is included in
   `current_function_outgoing_args_size'.  */
/* #define OUTGOING_REG_PARM_STACK_SPACE */

/* Offset from the stack pointer register to an item dynamically allocated
   on the stack, e.g., by `alloca'.

   The default value for this macro is `STACK_POINTER_OFFSET' plus the
   length of the outgoing arguments.  The default is correct for most
   machines.  See `function.c' for details.  */
/* #define STACK_DYNAMIC_OFFSET(FUNDECL) ... */

/* Value is the number of bytes of arguments automatically
   popped when returning from a subroutine call.
   FUNDECL is the declaration node of the function (as a tree),
   FUNTYPE is the data type of the function (as a tree),
   or for a library call it is an identifier node for the subroutine name.
   SIZE is the number of bytes of arguments passed on the stack.  */
#define RETURN_POPS_ARGS(FUNDECL,FUNTYPE,SIZE) 0

/* Define how to find the value returned by a function.
   VALTYPE is the data type of the value (as a tree).
   If the precise function being called is known, FUNC is its FUNCTION_DECL;
   otherwise, FUNC is 0.  */
#define FUNCTION_VALUE(VALTYPE, FUNC) \
  gen_rtx_REG (TYPE_MODE (VALTYPE) == BLKmode ? SImode : TYPE_MODE (VALTYPE), \
	       2)

/* Define this if it differs from FUNCTION_VALUE.  */
/* #define FUNCTION_OUTGOING_VALUE(VALTYPE, FUNC) ... */

/* Disable the promotion of some structures and unions to registers.
   Note that this matches FUNCTION_ARG behaviour.  */
#define RETURN_IN_MEMORY(TYPE) \
  (TYPE_MODE (TYPE) == BLKmode \
   || ((TREE_CODE (TYPE) == RECORD_TYPE || TREE_CODE (TYPE) == UNION_TYPE) \
       && (TYPE_ALIGN (TYPE) != BITS_PER_WORD || \
	   GET_MODE_SIZE (TYPE_MODE (TYPE)) != UNITS_PER_WORD)))

/* Don't default to pcc-struct-return, because we have already specified
   exactly how to return structures in the RETURN_IN_MEMORY macro.  */
#define DEFAULT_PCC_STRUCT_RETURN 0

/* Define how to find the value returned by a library function
   assuming the value has mode MODE.  */
#define LIBCALL_VALUE(MODE)  gen_rtx_REG (MODE, 2)

/* True if N is a possible register number for a function value
   as seen by the caller.  */
#define FUNCTION_VALUE_REGNO_P(N) ((N) == 2)

/* Determine whether a function argument is passed in a register, and
   which register.  See m88k.c.  */
#define FUNCTION_ARG(CUM, MODE, TYPE, NAMED) \
  m88k_function_arg (CUM, MODE, TYPE, NAMED)

/* Define this if it differs from FUNCTION_ARG.  */
/* #define FUNCTION_INCOMING_ARG(CUM, MODE, TYPE, NAMED) ... */

/* A C expression for the number of words, at the beginning of an
   argument, must be put in registers.  The value must be zero for
   arguments that are passed entirely in registers or that are entirely
   pushed on the stack.  */
#define FUNCTION_ARG_PARTIAL_NREGS(CUM, MODE, TYPE, NAMED) (0)

/* A C expression that indicates when an argument must be passed by
   reference.  If nonzero for an argument, a copy of that argument is
   made in memory and a pointer to the argument is passed instead of the
   argument itself.  The pointer is passed in whatever way is appropriate
   for passing a pointer to that type.  */
#define FUNCTION_ARG_PASS_BY_REFERENCE(CUM, MODE, TYPE, NAMED) \
  m88k_function_arg_pass_by_reference(&CUM, MODE, TYPE, NAMED)

/* A C type for declaring a variable that is used as the first argument
   of `FUNCTION_ARG' and other related values.  It suffices to count
   the number of words of argument so far.  */
#define CUMULATIVE_ARGS int

/* Initialize a variable CUM of type CUMULATIVE_ARGS for a call to a
   function whose data type is FNTYPE.  For a library call, FNTYPE is 0. */
#define INIT_CUMULATIVE_ARGS(CUM,FNTYPE,LIBNAME,INDIRECT) ((CUM) = 0)

/* Update the summarizer variable to advance past an argument in an
   argument list.  See m88k.c.  */
#define FUNCTION_ARG_ADVANCE(CUM, MODE, TYPE, NAMED) \
  m88k_function_arg_advance (& (CUM), MODE, TYPE, NAMED)

/* True if N is a possible register number for function argument passing.
   On the m88000, these are registers 2 through 9.  */
#define FUNCTION_ARG_REGNO_P(N) ((N) <= 9 && (N) >= 2)

/* A C expression which determines whether, and in which direction,
   to pad out an argument with extra space.  The value should be of
   type `enum direction': either `upward' to pad above the argument,
   `downward' to pad below, or `none' to inhibit padding.

   This macro does not control the *amount* of padding; that is always
   just enough to reach the next multiple of `FUNCTION_ARG_BOUNDARY'.  */
#define FUNCTION_ARG_PADDING(MODE, TYPE) \
  ((MODE) == BLKmode \
   || ((TYPE) && (TREE_CODE (TYPE) == RECORD_TYPE \
		  || TREE_CODE (TYPE) == UNION_TYPE)) \
   ? upward : GET_MODE_BITSIZE (MODE) < PARM_BOUNDARY ? downward : none)

/* If defined, a C expression that gives the alignment boundary, in bits,
   of an argument with the specified mode and type.  If it is not defined,
   `PARM_BOUNDARY' is used for all arguments.  */
#define FUNCTION_ARG_BOUNDARY(MODE, TYPE) \
  (((TYPE) ? TYPE_ALIGN (TYPE) : GET_MODE_BITSIZE (MODE)) <= PARM_BOUNDARY \
    ? PARM_BOUNDARY : 2 * PARM_BOUNDARY)

/* Perform any actions needed for a function that is receiving a
   variable number of arguments.  */
#define SETUP_INCOMING_VARARGS(CUM,MODE,TYPE,PRETEND_SIZE,NO_RTL) \
  m88k_setup_incoming_varargs (& (CUM), MODE, TYPE, & (PRETEND_SIZE), NO_RTL)

/* Define the `__builtin_va_list' type for the ABI.  */
#define BUILD_VA_LIST_TYPE(VALIST) \
  (VALIST) = m88k_build_va_list ()

/* Implement `va_start' for varargs and stdarg.  */
#define EXPAND_BUILTIN_VA_START(valist, nextarg) \
  m88k_va_start (valist, nextarg)

/* Implement `va_arg'.  */
#define EXPAND_BUILTIN_VA_ARG(valist, type) \
  m88k_va_arg (valist, type)

/* Output assembler code to FILE to increment profiler label # LABELNO
   for profiling a function entry.  */
#define FUNCTION_PROFILER(FILE, LABELNO) \
  output_function_profiler (FILE, LABELNO, "mcount")

/* Maximum length in instructions of the code output by FUNCTION_PROFILER.  */
#define FUNCTION_PROFILER_LENGTH (5+3+1+5)

/* EXIT_IGNORE_STACK should be nonzero if, when returning from a function,
   the stack pointer does not matter.  The value is tested only in
   functions that have frame pointers.
   No definition is equivalent to always zero.  */
#define EXIT_IGNORE_STACK (1)

/* Value should be nonzero if functions must have frame pointers.
   Zero means the frame pointer need not be set up (and parms
   may be accessed via the stack pointer) in functions that seem suitable.
   This is computed in `reload', in reload1.c.  */
#define FRAME_POINTER_REQUIRED						\
((current_function_profile || !leaf_function_p ()			\
  || !TARGET_OMIT_LEAF_FRAME_POINTER)					\
 || (write_symbols != NO_DEBUG))

/* Define registers used by the epilogue and return instruction.  */
#define EPILOGUE_USES(REGNO) \
(reload_completed && ((REGNO) == 1 \
		      || (current_function_profile \
			  && (REGNO) == FRAME_POINTER_REGNUM)))

/* Before the prologue, RA is in r1.  */
#define INCOMING_RETURN_ADDR_RTX gen_rtx_REG (Pmode, 1)
#define DWARF_FRAME_RETURN_COLUMN DWARF_FRAME_REGNUM (1)

/* Definitions for register eliminations.

   We have two registers that can be eliminated on the m88k.  First, the
   frame pointer register can often be eliminated in favor of the stack
   pointer register.  Secondly, the argument pointer register can always be
   eliminated; it is replaced with either the stack or frame pointer.  */

/* This is an array of structures.  Each structure initializes one pair
   of eliminable registers.  The "from" register number is given first,
   followed by "to".  Eliminations of the same "from" register are listed
   in order of preference.  */
#define ELIMINABLE_REGS				\
{{ ARG_POINTER_REGNUM, STACK_POINTER_REGNUM},	\
 { ARG_POINTER_REGNUM, FRAME_POINTER_REGNUM},	\
 { FRAME_POINTER_REGNUM, STACK_POINTER_REGNUM}}

/* Given FROM and TO register numbers, say whether this elimination
   is allowed.  */
#define CAN_ELIMINATE(FROM, TO) \
  (!((FROM) == FRAME_POINTER_REGNUM && FRAME_POINTER_REQUIRED))

/* Define the offset between two registers, one to be eliminated, and the other
   its replacement, at the start of a routine.  */
#define INITIAL_ELIMINATION_OFFSET(FROM, TO, OFFSET)			 \
{ m88k_layout_frame ();							 \
  if ((FROM) == FRAME_POINTER_REGNUM && (TO) == STACK_POINTER_REGNUM)	 \
      (OFFSET) = m88k_fp_offset;					 \
  else if ((FROM) == ARG_POINTER_REGNUM && (TO) == FRAME_POINTER_REGNUM) \
    (OFFSET) = m88k_stack_size - m88k_fp_offset;			 \
  else if ((FROM) == ARG_POINTER_REGNUM && (TO) == STACK_POINTER_REGNUM) \
    (OFFSET) = m88k_stack_size;						 \
  else									 \
    abort ();								 \
}

/*** Trampolines for Nested Functions ***/

#ifndef FINALIZE_TRAMPOLINE
#define FINALIZE_TRAMPOLINE(TRAMP)
#endif

/* Output assembler code for a block containing the constant parts
   of a trampoline, leaving space for the variable parts.

   This block is placed on the stack and filled in.  It is aligned
   0 mod 128 and those portions that are executed are constant.
   This should work for instruction caches that have cache lines up
   to the aligned amount (128 is arbitrary), provided no other code
   producer is attempting to play the same game.  This of course is
   in violation of any number of 88open standards.  */

#define TRAMPOLINE_TEMPLATE(FILE)					\
{									\
  char buf[256];							\
  static int labelno = 0;						\
  labelno++;								\
  ASM_GENERATE_INTERNAL_LABEL (buf, "LTRMP", labelno);			\
  /* Save the return address (r1) in the static chain reg (r11).  */	\
  asm_fprintf (FILE, "\tor\t %R%s,%R%s,0\n",				\
	       reg_names[11], reg_names[1]);				\
  /* Locate this block; transfer to the next instruction.  */		\
  fprintf (FILE, "\tbsr\t %s\n", &buf[1]);				\
  ASM_OUTPUT_INTERNAL_LABEL (FILE, "LTRMP", labelno);			\
  /* Save r10; use it as the relative pointer; restore r1.  */		\
  asm_fprintf (FILE, "\tst\t %R%s,%R%s,24\n",				\
	       reg_names[10], reg_names[1]);				\
  asm_fprintf (FILE, "\tor\t %R%s,%R%s,0\n",				\
	       reg_names[10], reg_names[1]);				\
  asm_fprintf (FILE, "\tor\t %R%s,%R%s,0\n",				\
	       reg_names[1], reg_names[11]);				\
  /* Load the function's address and go there.  */			\
  asm_fprintf (FILE, "\tld\t %R%s,%R%s,32\n",				\
	       reg_names[11], reg_names[10]);				\
  asm_fprintf (FILE, "\tjmp.n\t %R%s\n", reg_names[11]);		\
  /* Restore r10 and load the static chain register.  */		\
  asm_fprintf (FILE, "\tld.d\t %R%s,%R%s,24\n",				\
	       reg_names[10], reg_names[10]);				\
  /* Storage: r10 save area, static chain, function address.  */	\
  assemble_aligned_integer (UNITS_PER_WORD, const0_rtx);		\
  assemble_aligned_integer (UNITS_PER_WORD, const0_rtx);		\
  assemble_aligned_integer (UNITS_PER_WORD, const0_rtx);		\
}

/* Length in units of the trampoline for entering a nested function.
   This is really two components.  The first 32 bytes are fixed and
   must be copied; the last 12 bytes are just storage that's filled
   in later.  So for allocation purposes, it's 32+12 bytes, but for
   initialization purposes, it's 32 bytes.  */

#define TRAMPOLINE_SIZE (32+12)

/* Alignment required for a trampoline.  128 is used to find the
   beginning of a line in the instruction cache and to allow for
   instruction cache lines of up to 128 bytes.  */

#define TRAMPOLINE_ALIGNMENT 128

/* Emit RTL insns to initialize the variable parts of a trampoline.
   FNADDR is an RTX for the address of the function's pure code.
   CXT is an RTX for the static chain value for the function.  */

#define INITIALIZE_TRAMPOLINE(TRAMP, FNADDR, CXT)			\
{									\
  emit_move_insn (gen_rtx_MEM (SImode, plus_constant (TRAMP, 40)), FNADDR); \
  emit_move_insn (gen_rtx_MEM (SImode, plus_constant (TRAMP, 36)), CXT); \
  FINALIZE_TRAMPOLINE (TRAMP);						\
}

/*** Library Subroutine Names ***/

/* Define this macro if GNU CC should generate calls to the System V
   (and ANSI C) library functions `memcpy' and `memset' rather than
   the BSD functions `bcopy' and `bzero'.  */
#define TARGET_MEM_FUNCTIONS

/*** Addressing Modes ***/

#define SELECT_CC_MODE(OP,X,Y) CCmode

/* #define HAVE_POST_INCREMENT 0 */
/* #define HAVE_POST_DECREMENT 0 */

/* #define HAVE_PRE_DECREMENT 0 */
/* #define HAVE_PRE_INCREMENT 0 */

/* Recognize any constant value that is a valid address.
   When PIC, we do not accept an address that would require a scratch reg
   to load into a register.  */

#define CONSTANT_ADDRESS_P(X)   \
  (GET_CODE (X) == LABEL_REF || GET_CODE (X) == SYMBOL_REF		\
   || GET_CODE (X) == CONST_INT || GET_CODE (X) == HIGH                 \
   || (GET_CODE (X) == CONST                                            \
       && ! (flag_pic && pic_address_needs_scratch (X))))


/* Maximum number of registers that can appear in a valid memory address.  */
#define MAX_REGS_PER_ADDRESS 2

/* The condition for memory shift insns.  */
#define SCALED_ADDRESS_P(ADDR)			\
  (GET_CODE (ADDR) == PLUS			\
   && (GET_CODE (XEXP (ADDR, 0)) == MULT	\
       || GET_CODE (XEXP (ADDR, 1)) == MULT))

/* GO_IF_LEGITIMATE_ADDRESS recognizes an RTL expression
   that is a valid memory address for an instruction.
   The MODE argument is the machine mode for the MEM expression
   that wants to use this address.

   On the m88000, a legitimate address has the form REG, REG+REG,
   REG+SMALLINT, REG+(REG*modesize) (REG[REG]), or SMALLINT.

   The register elimination process should deal with the argument
   pointer and frame pointer changing to REG+SMALLINT.  */

#define LEGITIMATE_INDEX_P(X, MODE)			\
   ((GET_CODE (X) == CONST_INT				\
     && SMALL_INT (X))					\
    || (REG_P (X)					\
	&& REG_OK_FOR_INDEX_P (X))			\
    || (GET_CODE (X) == MULT				\
	&& REG_P (XEXP (X, 0))				\
	&& REG_OK_FOR_INDEX_P (XEXP (X, 0))		\
	&& GET_CODE (XEXP (X, 1)) == CONST_INT		\
	&& INTVAL (XEXP (X, 1)) == GET_MODE_SIZE (MODE)))

#define RTX_OK_FOR_BASE_P(X)						\
  ((GET_CODE (X) == REG && REG_OK_FOR_BASE_P (X))			\
  || (GET_CODE (X) == SUBREG						\
      && GET_CODE (SUBREG_REG (X)) == REG				\
      && REG_OK_FOR_BASE_P (SUBREG_REG (X))))

#define RTX_OK_FOR_INDEX_P(X)						\
  ((GET_CODE (X) == REG && REG_OK_FOR_INDEX_P (X))			\
  || (GET_CODE (X) == SUBREG						\
      && GET_CODE (SUBREG_REG (X)) == REG				\
      && REG_OK_FOR_INDEX_P (SUBREG_REG (X))))

#define GO_IF_LEGITIMATE_ADDRESS(MODE, X, ADDR)		\
{							\
  if (REG_P (X))					\
    {							\
      if (REG_OK_FOR_BASE_P (X))			\
	goto ADDR;					\
    }							\
  else if (GET_CODE (X) == PLUS)			\
    {							\
      register rtx _x0 = XEXP (X, 0);			\
      register rtx _x1 = XEXP (X, 1);			\
      if ((flag_pic					\
	   && _x0 == pic_offset_table_rtx		\
	   && (flag_pic == 2				\
	       ? RTX_OK_FOR_BASE_P (_x1)		\
	       : (GET_CODE (_x1) == SYMBOL_REF		\
		  || GET_CODE (_x1) == LABEL_REF)))	\
	  || (RTX_OK_FOR_BASE_P (_x0)			\
	      && LEGITIMATE_INDEX_P (_x1, MODE))	\
	  || (RTX_OK_FOR_BASE_P (_x1)			\
	      && LEGITIMATE_INDEX_P (_x0, MODE)))	\
	goto ADDR;					\
    }							\
  else if (GET_CODE (X) == LO_SUM)			\
    {							\
      register rtx _x0 = XEXP (X, 0);			\
      register rtx _x1 = XEXP (X, 1);			\
      if (RTX_OK_FOR_BASE_P (_x0)			\
	  && CONSTANT_P (_x1))				\
	goto ADDR;					\
    }							\
  else if (GET_CODE (X) == CONST_INT			\
	   && SMALL_INT (X))				\
    goto ADDR;						\
}

/* The macros REG_OK_FOR..._P assume that the arg is a REG rtx
   and check its validity for a certain class.
   We have two alternate definitions for each of them.
   The usual definition accepts all pseudo regs; the other rejects
   them unless they have been allocated suitable hard regs.
   The symbol REG_OK_STRICT causes the latter definition to be used.

   Most source files want to accept pseudo regs in the hope that
   they will get allocated to the class that the insn wants them to be in.
   Source files for reload pass need to be strict.
   After reload, it makes no difference, since pseudo regs have
   been eliminated by then.  */

#ifndef REG_OK_STRICT

/* Nonzero if X is a hard reg that can be used as an index
   or if it is a pseudo reg.  Not the argument pointer.  */
#define REG_OK_FOR_INDEX_P(X)                                         \
  (!XRF_REGNO_P(REGNO (X)))
/* Nonzero if X is a hard reg that can be used as a base reg
   or if it is a pseudo reg.  */
#define REG_OK_FOR_BASE_P(X) (REG_OK_FOR_INDEX_P (X))

#else

/* Nonzero if X is a hard reg that can be used as an index.  */
#define REG_OK_FOR_INDEX_P(X) REGNO_OK_FOR_INDEX_P (REGNO (X))
/* Nonzero if X is a hard reg that can be used as a base reg.  */
#define REG_OK_FOR_BASE_P(X) REGNO_OK_FOR_BASE_P (REGNO (X))

#endif

/* Try machine-dependent ways of modifying an illegitimate address
   to be legitimate.  If we find one, return the new, valid address.
   This macro is used in only one place: `memory_address' in explow.c.

   OLDX is the address as it was before break_out_memory_refs was called.
   In some cases it is useful to look at this to decide what needs to be done.

   MODE and WIN are passed so that this macro can use
   GO_IF_LEGITIMATE_ADDRESS.

   It is always safe for this macro to do nothing.  It exists to recognize
   opportunities to optimize the output.  */

/* On the m88000, change REG+N into REG+REG, and REG+(X*Y) into REG+REG.  */

#define LEGITIMIZE_ADDRESS(X,OLDX,MODE,WIN)			\
{								\
  if (GET_CODE (X) == PLUS && CONSTANT_ADDRESS_P (XEXP (X, 1)))	\
    (X) = gen_rtx_PLUS (SImode, XEXP (X, 0),			\
			copy_to_mode_reg (SImode, XEXP (X, 1))); \
  if (GET_CODE (X) == PLUS && CONSTANT_ADDRESS_P (XEXP (X, 0)))	\
    (X) = gen_rtx_PLUS (SImode, XEXP (X, 1),			\
			copy_to_mode_reg (SImode, XEXP (X, 0))); \
  if (GET_CODE (X) == PLUS && GET_CODE (XEXP (X, 0)) == MULT)	\
    (X) = gen_rtx_PLUS (SImode, XEXP (X, 1),			\
			force_operand (XEXP (X, 0), 0));	\
  if (GET_CODE (X) == PLUS && GET_CODE (XEXP (X, 1)) == MULT)	\
    (X) = gen_rtx_PLUS (SImode, XEXP (X, 0),			\
			force_operand (XEXP (X, 1), 0));	\
  if (GET_CODE (X) == PLUS && GET_CODE (XEXP (X, 0)) == PLUS)	\
    (X) = gen_rtx_PLUS (Pmode, force_operand (XEXP (X, 0), NULL_RTX),\
			XEXP (X, 1));				\
  if (GET_CODE (X) == PLUS && GET_CODE (XEXP (X, 1)) == PLUS)	\
    (X) = gen_rtx_PLUS (Pmode, XEXP (X, 0),			\
			force_operand (XEXP (X, 1), NULL_RTX));	\
  if (GET_CODE (X) == SYMBOL_REF || GET_CODE (X) == CONST	\
	   || GET_CODE (X) == LABEL_REF)			\
    (X) = legitimize_address (flag_pic, X, 0, 0);		\
  if (memory_address_p (MODE, X))				\
    goto WIN; }

/* Go to LABEL if ADDR (a legitimate address expression)
   has an effect that depends on the machine mode it is used for.
   On the m88000 this is never true.  */

#define GO_IF_MODE_DEPENDENT_ADDRESS(ADDR,LABEL)

/* Nonzero if the constant value X is a legitimate general operand.
   It is given that X satisfies CONSTANT_P or is a CONST_DOUBLE.  */
#define LEGITIMATE_CONSTANT_P(X) (1)

/* Define this, so that when PIC, reload won't try to reload invalid
   addresses which require two reload registers.  */

#define LEGITIMATE_PIC_OPERAND_P(X)  (! pic_address_needs_scratch (X))


/*** Condition Code Information ***/

/* C code for a data type which is used for declaring the `mdep'
   component of `cc_status'.  It defaults to `int'.  */
/* #define CC_STATUS_MDEP int */

/* A C expression to initialize the `mdep' field to "empty".  */
/* #define CC_STATUS_MDEP_INIT (cc_status.mdep = 0) */

/* Macro to zap the normal portions of CC_STATUS, but leave the
   machine dependent parts (ie, literal synthesis) alone.  */
/* #define CC_STATUS_INIT_NO_MDEP \
  (cc_status.flags = 0, cc_status.value1 = 0, cc_status.value2 = 0) */

/* When using a register to hold the condition codes, the cc_status
   mechanism cannot be used.  */
#define NOTICE_UPDATE_CC(EXP, INSN) (0)

/*** Miscellaneous Parameters ***/

/* Define the codes that are matched by predicates in m88k.c.  */
#define PREDICATE_CODES	  						\
  {"move_operand", {SUBREG, REG, CONST_INT, LO_SUM, MEM}},		\
  {"call_address_operand", {SUBREG, REG, SYMBOL_REF, LABEL_REF, CONST}}, \
  {"arith_operand", {SUBREG, REG, CONST_INT}},				\
  {"arith5_operand", {SUBREG, REG, CONST_INT}},				\
  {"arith32_operand", {SUBREG, REG, CONST_INT}},			\
  {"arith64_operand", {SUBREG, REG, CONST_INT}},			\
  {"int5_operand", {CONST_INT}},					\
  {"int32_operand", {CONST_INT}},					\
  {"add_operand", {SUBREG, REG, CONST_INT}},				\
  {"reg_or_bbx_mask_operand", {SUBREG, REG, CONST_INT}},		\
  {"real_or_0_operand", {SUBREG, REG, CONST_DOUBLE}},			\
  {"reg_or_0_operand", {SUBREG, REG, CONST_INT}},                       \
  {"relop", {EQ, NE, LT, LE, GE, GT, LTU, LEU, GEU, GTU}},		\
  {"even_relop", {EQ, LT, GT, LTU, GTU}},		\
  {"odd_relop", { NE, LE, GE, LEU, GEU}},		\
  {"partial_ccmode_register_operand", { SUBREG, REG}},			\
  {"relop_no_unsigned", {EQ, NE, LT, LE, GE, GT}},			\
  {"equality_op", {EQ, NE}},						\
  {"pc_or_label_ref", {PC, LABEL_REF}},					\
  {"label_ref", {LABEL_REF}},


/* A list of predicates that do special things with modes, and so
   should not elicit warnings for VOIDmode match_operand.  */

#define SPECIAL_MODE_PREDICATES		\
  "partial_ccmode_register_operand",	\
  "pc_or_label_ref",

/* The case table contains either words or branch instructions.  This says
   which.  We always claim that the vector is PC-relative.  It is position
   independent when -fpic is used.  */
#define CASE_VECTOR_INSNS (TARGET_88100 || flag_pic)

/* An alias for a machine mode name.  This is the machine mode that
   elements of a jump-table should have.  */
#define CASE_VECTOR_MODE SImode

/* Define as C expression which evaluates to nonzero if the tablejump
   instruction expects the table to contain offsets from the address of the
   table.
   Do not define this if the table should contain absolute addresses. */
#define CASE_VECTOR_PC_RELATIVE 1

/* Define this if control falls through a `case' insn when the index
   value is out of range.  This means the specified default-label is
   actually ignored by the `case' insn proper.  */
/* #define CASE_DROPS_THROUGH */

/* Define this to be the smallest number of different values for which it
   is best to use a jump-table instead of a tree of conditional branches.
   The default is 4 for machines with a casesi instruction and 5 otherwise.
   The best 88110 number is around 7, though the exact number isn't yet
   known.  A third alternative for the 88110 is to use a binary tree of
   bb1 instructions on bits 2/1/0 if the range is dense.  This may not
   win very much though.  */
#define CASE_VALUES_THRESHOLD (TARGET_88100 ? 4 : 7)

/* Define this as 1 if `char' should by default be signed; else as 0.  */
#define DEFAULT_SIGNED_CHAR 1

/* The 88open ABI says size_t is unsigned int.  */
#define SIZE_TYPE "unsigned int"

/* Handle #pragma pack and sometimes #pragma weak.  */
#define HANDLE_SYSV_PRAGMA 1

/* Max number of bytes we can move from memory to memory
   in one reasonably fast instruction.  */
#define MOVE_MAX 8

/* Define if normal loads of shorter-than-word items from memory clears
   the rest of the bigs in the register.  */
#define BYTE_LOADS_ZERO_EXTEND

/* Zero if access to memory by bytes is faster.  */
#define SLOW_BYTE_ACCESS 1

/* Value is 1 if truncating an integer of INPREC bits to OUTPREC bits
   is done just by pretending it is already truncated.  */
#define TRULY_NOOP_TRUNCATION(OUTPREC, INPREC) 1

/* Define this if addresses of constant functions
   shouldn't be put through pseudo regs where they can be cse'd.
   Desirable on machines where ordinary constants are expensive
   but a CALL with constant address is cheap.  */
#define NO_FUNCTION_CSE

/* Define this macro if an argument declared as `char' or
   `short' in a prototype should actually be passed as an
   `int'.  In addition to avoiding errors in certain cases of
   mismatch, it also makes for better code on certain machines.  */
#define PROMOTE_PROTOTYPES 1

/* We assume that the store-condition-codes instructions store 0 for false
   and some other value for true.  This is the value stored for true.  */
#define STORE_FLAG_VALUE (-1)

/* Specify the machine mode that pointers have.
   After generation of rtl, the compiler makes no further distinction
   between pointers and any other objects of this machine mode.  */
#define Pmode SImode

/* A function address in a call instruction
   is a word address (for indexing purposes)
   so give the MEM rtx word mode.  */
#define FUNCTION_MODE SImode

/* A barrier will be aligned so account for the possible expansion.
   A volatile load may be preceded by a serializing instruction.
   Account for profiling code output at NOTE_INSN_PROLOGUE_END.
   Account for block profiling code at basic block boundaries.  */
#define ADJUST_INSN_LENGTH(RTX, LENGTH)					\
  if (GET_CODE (RTX) == BARRIER						\
      || (TARGET_SERIALIZE_VOLATILE					\
	  && GET_CODE (RTX) == INSN					\
	  && GET_CODE (PATTERN (RTX)) == SET				\
	  && ((GET_CODE (SET_SRC (PATTERN (RTX))) == MEM		\
	       && MEM_VOLATILE_P (SET_SRC (PATTERN (RTX)))))))		\
    (LENGTH) += 1;							\
  else if (GET_CODE (RTX) == NOTE					\
	   && NOTE_LINE_NUMBER (RTX) == NOTE_INSN_PROLOGUE_END)		\
    {									\
      if (current_function_profile)					\
	(LENGTH) += (FUNCTION_PROFILER_LENGTH + REG_PUSH_LENGTH		\
		     + REG_POP_LENGTH);					\
    }									\

/* Track the state of the last volatile memory reference.  Clear the
   state with CC_STATUS_INIT for now.  */
#define CC_STATUS_INIT m88k_volatile_code = '\0'

/* Compute the cost of computing a constant rtl expression RTX
   whose rtx-code is CODE.  The body of this macro is a portion
   of a switch statement.  If the code is computed here,
   return it with a return statement.  Otherwise, break from the switch.

   We assume that any 16 bit integer can easily be recreated, so we
   indicate 0 cost, in an attempt to get GCC not to optimize things
   like comparison against a constant.

   The cost of CONST_DOUBLE is zero (if it can be placed in an insn, it
   is as good as a register; since it can't be placed in any insn, it
   won't do anything in cse, but it will cause expand_binop to pass the
   constant to the define_expands).  */
#define CONST_COSTS(RTX,CODE,OUTER_CODE)		\
  case CONST_INT:					\
    if (SMALL_INT (RTX))				\
      return 0;						\
    else if (SMALL_INTVAL (- INTVAL (RTX)))		\
      return 2;						\
    else if (classify_integer (SImode, INTVAL (RTX)) != m88k_oru_or) \
      return 4;						\
    return 7;						\
  case HIGH:						\
    return 2;						\
  case CONST:						\
  case LABEL_REF:					\
  case SYMBOL_REF:					\
    if (flag_pic)					\
      return (flag_pic == 2) ? 11 : 8;			\
    return 5;						\
  case CONST_DOUBLE:					\
    return 0;

/* Provide the costs of an addressing mode that contains ADDR.
   If ADDR is not a valid address, its cost is irrelevant.
   REG+REG is made slightly more expensive because it might keep
   a register live for longer than we might like.  */
#define ADDRESS_COST(ADDR)				\
  (GET_CODE (ADDR) == REG ? 1 :				\
   GET_CODE (ADDR) == LO_SUM ? 1 :			\
   GET_CODE (ADDR) == HIGH ? 2 :			\
   GET_CODE (ADDR) == MULT ? 1 :			\
   GET_CODE (ADDR) != PLUS ? 4 :			\
   (REG_P (XEXP (ADDR, 0)) && REG_P (XEXP (ADDR, 1))) ? 2 : 1)

/* Provide the costs of a rtl expression.  This is in the body of a
   switch on CODE.  */
#define RTX_COSTS(X,CODE,OUTER_CODE)				\
  case MEM:						\
    return COSTS_N_INSNS (2);				\
  case MULT:						\
    return COSTS_N_INSNS (3);				\
  case DIV:						\
  case UDIV:						\
  case MOD:						\
  case UMOD:						\
    return COSTS_N_INSNS (38);

/* A C expressions returning the cost of moving data of MODE from a register
   to or from memory.  This is more costly than between registers.  */
#define MEMORY_MOVE_COST(MODE,CLASS,IN) 4

/* Provide the cost of a branch.  Exact meaning under development.  */
#define BRANCH_COST (TARGET_88100 ? 1 : 2)

/* Do not break .stabs pseudos into continuations.  */
#define DBX_CONTIN_LENGTH 0

/*** Output of Assembler Code ***/

/* Control the assembler format that we output.  */

/* A C string constant describing how to begin a comment in the target
   assembler language.  The compiler assumes that the comment will end at
   the end of the line.  */
#define ASM_COMMENT_START ";"

/* Assembler specific opcodes.
   Not overriding <elfos.h> if already included.  */
#ifndef OBJECT_FORMAT_ELF

/* These are used in varasm.c as well.  */
#define TEXT_SECTION_ASM_OP	"\ttext"
#define DATA_SECTION_ASM_OP	"\tdata"

/* These are pretty much common to all assemblers.  */
#undef	IDENT_ASM_OP
#define IDENT_ASM_OP		"\tident\t"
#define FILE_ASM_OP		"\tfile\t"
#undef	SET_ASM_OP
#define SET_ASM_OP		"\tdef\t"
#define GLOBAL_ASM_OP		"\tglobal\t"
#undef	ALIGN_ASM_OP
#define ALIGN_ASM_OP		"\talign\t"
#undef	SKIP_ASM_OP
#define SKIP_ASM_OP		"\tzero\t"
#undef	COMMON_ASM_OP
#define COMMON_ASM_OP		"\tcomm\t"
#define BSS_ASM_OP		"\tbss\t"
#define FLOAT_ASM_OP		"\tfloat\t"
#define DOUBLE_ASM_OP		"\tdouble\t"
#undef	ASCII_DATA_ASM_OP
#define ASCII_DATA_ASM_OP	"\tstring\t"

/* These are specific to PIC.  */
#undef	TYPE_ASM_OP
#define TYPE_ASM_OP		"\ttype\t"
#undef	SIZE_ASM_OP
#define SIZE_ASM_OP		"\tsize\t"
#ifndef AS_BUG_POUND_TYPE /* Faulty assemblers require @ rather than #.  */
#undef	TYPE_OPERAND_FMT
#define TYPE_OPERAND_FMT	"#%s"
#endif

/* These are specific to version 03.00 assembler syntax.  */
#define INTERNAL_ASM_OP		"\tlocal\t"
#define PUSHSECTION_ASM_OP	"\tsection\t"
#define POPSECTION_ASM_OP	"\tprevious"

/* These are specific to the version 04.00 assembler syntax.  */
#define REQUIRES_88110_ASM_OP	"\trequires_88110"

/* This is how we tell the assembler that a symbol is weak.  */
#undef ASM_WEAKEN_LABEL
#define ASM_WEAKEN_LABEL(FILE,NAME) \
  do { fputs ("\tweak\t", FILE); assemble_name (FILE, NAME); \
       fputc ('\n', FILE); } while (0)

/* Write the extra assembler code needed to declare a function properly.  */
#undef	ASM_DECLARE_FUNCTION_NAME
#define ASM_DECLARE_FUNCTION_NAME(FILE, NAME, DECL)			\
  do {									\
    ASM_OUTPUT_LABEL(FILE, NAME);					\
  } while (0)

/* Write the extra assembler code needed to declare an object properly.  */
#undef	ASM_DECLARE_OBJECT_NAME
#define ASM_DECLARE_OBJECT_NAME(FILE, NAME, DECL)			\
  do {									\
    ASM_OUTPUT_LABEL(FILE, NAME);					\
  } while (0);

/* Output the size directive for a decl in rest_of_decl_compilation
   in the case where we did not do so before the initializer.
   Once we find the error_mark_node, we know that the value of
   size_directive_output was set
   by ASM_DECLARE_OBJECT_NAME when it was run for the same decl.  */

#undef ASM_FINISH_DECLARE_OBJECT
#define ASM_FINISH_DECLARE_OBJECT(FILE, DECL, TOP_LEVEL, AT_END)	 \
do {									 \
   } while (0)

/* This is how to declare the size of a function.  */
#undef	ASM_DECLARE_FUNCTION_SIZE
#define ASM_DECLARE_FUNCTION_SIZE(FILE, FNAME, DECL)			\
  do {									\
  } while (0)

/* The single-byte pseudo-op is the default.  */
#undef	ASM_OUTPUT_ASCII
#define ASM_OUTPUT_ASCII(FILE, P, SIZE)  \
  output_ascii (FILE, ASCII_DATA_ASM_OP, 48, P, SIZE)

/* Code to handle #ident directives.  */
#undef	ASM_OUTPUT_IDENT
#ifdef DBX_DEBUGGING_INFO
#define ASM_OUTPUT_IDENT(FILE, NAME)
#else
#define ASM_OUTPUT_IDENT(FILE, NAME) \
  output_ascii (FILE, IDENT_ASM_OP, 4000, NAME, strlen (NAME));
#endif

/* This is how to store into the string LABEL
   the symbol_ref name of an internal numbered label where
   PREFIX is the class of label and NUM is the number within the class.
   This is suitable for output with `assemble_name'.  This must agree
   with ASM_OUTPUT_INTERNAL_LABEL above, except for being prefixed
   with an `*'.  */

#undef ASM_GENERATE_INTERNAL_LABEL
#define ASM_GENERATE_INTERNAL_LABEL(LABEL,PREFIX,NUM)			\
  sprintf (LABEL, "*@%s%u", PREFIX, (unsigned) (NUM))

/* This is how to output an internal numbered label where
   PREFIX is the class of label and NUM is the number within the class.
   For ELF, labels use `.' rather than `@'.  */

#undef ASM_OUTPUT_INTERNAL_LABEL
#define ASM_OUTPUT_INTERNAL_LABEL(FILE,PREFIX,NUM)			\
  fprintf (FILE, "@%s%u:\n", PREFIX, (unsigned) (NUM))

/* The prefix to add to user-visible assembler symbols.  */
#undef USER_LABEL_PREFIX
#define USER_LABEL_PREFIX "_"

#undef	ASM_OUTPUT_COMMON
#undef	ASM_OUTPUT_ALIGNED_COMMON
#define ASM_OUTPUT_COMMON(FILE, NAME, SIZE, ROUNDED)	\
( fprintf ((FILE), "%s", COMMON_ASM_OP), 		\
  assemble_name ((FILE), (NAME)),			\
  fprintf ((FILE), ",%u\n", (SIZE) ? (SIZE) : 1))

/* This says how to output an assembler line to define a local common
   symbol.  */
#undef	ASM_OUTPUT_LOCAL
#undef	ASM_OUTPUT_ALIGNED_LOCAL
#define ASM_OUTPUT_LOCAL(FILE, NAME, SIZE, ROUNDED)	\
( fprintf ((FILE), "%s", BSS_ASM_OP),			\
  assemble_name ((FILE), (NAME)),			\
  fprintf ((FILE), ",%u,%d\n", (SIZE) ? (SIZE) : 1, (SIZE) <= 4 ? 4 : 8))

#undef	ASM_OUTPUT_SKIP
#define ASM_OUTPUT_SKIP(FILE,SIZE)  \
  fprintf (FILE, "%s%u\n", SKIP_ASM_OP, (SIZE))

#endif /* OBJECT_FORMAT_ELF */

/* Output any initial stuff to the assembly file.  Always put out
   a file directive, even if not debugging.

   Immediately after putting out the file, put out a "sem.<value>"
   declaration.  This should be harmless on other systems, and
   is used in DG/UX by the debuggers to supplement COFF.  The
   fields in the integer value are as follows:

   Bits	Value	Meaning
   ----	-----	-------
   0-1	0	No information about stack locations
	1	Auto/param locations are based on r30
	2	Auto/param locations are based on CFA

   3-2	0	No information on dimension order
	1	Array dims in sym table matches source language
	2	Array dims in sym table is in reverse order

   5-4	0	No information about the case of global names
	1	Global names appear in the symbol table as in the source
	2	Global names have been converted to lower case
	3	Global names have been converted to upper case.  */

#undef	ASM_FILE_START
#define ASM_FILE_START(FILE) \
  output_file_start (FILE)

#undef	ASM_FILE_END

#define ASM_OUTPUT_SOURCE_FILENAME(FILE, NAME) \
  do {                                         \
    fputs (FILE_ASM_OP, FILE);                 \
    output_quoted_string (FILE, NAME);         \
    putc ('\n', FILE);                         \
  } while (0)

/* Output to assembler file text saying following lines
   may contain character constants, extra white space, comments, etc.  */
#define ASM_APP_ON ""

/* Output to assembler file text saying following lines
   no longer contain unusual constructs.  */
#define ASM_APP_OFF ""

/* Format the assembly opcode so that the arguments are all aligned.
   The maximum instruction size is 8 characters (fxxx.xxx), so a tab and a
   space will do to align the output.  Abandon the output if a `%' is
   encountered.  */
#define ASM_OUTPUT_OPCODE(STREAM, PTR)					\
  {									\
    int ch;								\
    const char *orig_ptr;						\
									\
    for (orig_ptr = (PTR);						\
	 (ch = *(PTR)) && ch != ' ' && ch != '\t' && ch != '\n' && ch != '%'; \
	 (PTR)++)							\
      putc (ch, STREAM);						\
									\
    if (ch == ' ' && orig_ptr != (PTR) && (PTR) - orig_ptr < 8)		\
      putc ('\t', STREAM);						\
  }

/* How to refer to registers in assembler output.
   This sequence is indexed by compiler's hard-register-number.  */

#define REGISTER_NAMES \
  { "r0",  "r1",  "r2",  "r3",  "r4",  "r5",  "r6",  "r7",	\
    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",	\
    "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",	\
    "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",	\
    "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",	\
    "x8",  "x9",  "x10", "x11", "x12", "x13", "x14", "x15",	\
    "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",	\
    "x24", "x25", "x26", "x27", "x28", "x29", "x30", "x31" }

/* Define additional names for use in asm clobbers and asm declarations.

   We define the fake Condition Code register as an alias for reg 0 (which
   is our `condition code' register), so that condition codes can easily
   be clobbered by an asm.  The carry bit in the PSR is now used.  */

#define ADDITIONAL_REGISTER_NAMES	{{"psr", 0}, {"cc", 0}}

/* This is how to output a reference to a user-level label named NAME.  */
#undef	ASM_OUTPUT_LABELREF
#define ASM_OUTPUT_LABELREF(FILE,NAME)			\
  asm_fprintf ((FILE), "%U%s", (NAME))

/* Store in OUTPUT a string (made with alloca) containing
   an assembler-name for a local static variable named NAME.
   LABELNO is an integer which is different for each call.  */
#define ASM_FORMAT_PRIVATE_NAME(OUTPUT, NAME, LABELNO)			\
  do {									\
    (OUTPUT) = (char *) alloca (strlen ((NAME)) + 10);			\
    sprintf ((OUTPUT), "%s.%u", (NAME), (unsigned) (LABELNO));		\
  } while (0)

/* Change to the readonly data section for a table of addresses.
   final_scan_insn changes back to the text section.  */
#undef	ASM_OUTPUT_CASE_LABEL
#define ASM_OUTPUT_CASE_LABEL(FILE, PREFIX, NUM, TABLE)			\
  do {									\
    if (! CASE_VECTOR_INSNS)						\
      {									\
        readonly_data_section ();					\
        ASM_OUTPUT_ALIGN (FILE, 2);					\
      }									\
    ASM_OUTPUT_INTERNAL_LABEL (FILE, PREFIX, NUM);			\
  } while (0)

/* Epilogue for case labels.  This jump instruction is called by casesi
   to transfer to the appropriate branch instruction within the table.
   The label `@L<n>e' is coined to mark the end of the table.  */
#define ASM_OUTPUT_CASE_END(FILE, NUM, TABLE)				\
  do {									\
    if (CASE_VECTOR_INSNS)						\
      {									\
	char label[256]; 						\
	ASM_GENERATE_INTERNAL_LABEL (label, "L", NUM);			\
	fprintf (FILE, "%se:\n", &label[1]);				\
	if (! flag_delayed_branch)					\
	  asm_fprintf (FILE, "\tlda\t %R%s,%R%s[%R%s]\n", reg_names[1],	\
		       reg_names[1], reg_names[m88k_case_index]);	\
	asm_fprintf (FILE, "\tjmp\t %R%s\n", reg_names[1]);		\
      }									\
  } while (0)

/* This is how to output an element of a case-vector that is absolute.  */
#define ASM_OUTPUT_ADDR_VEC_ELT(FILE, VALUE)				\
  do {									\
    char buffer[256];							\
    ASM_GENERATE_INTERNAL_LABEL (buffer, "L", VALUE);			\
    fprintf (FILE, CASE_VECTOR_INSNS ? "\tbr\t %s\n" : "\tword\t %s\n",	\
	     &buffer[1]);						\
  } while (0)

/* This is how to output an element of a case-vector that is relative.  */
#define ASM_OUTPUT_ADDR_DIFF_ELT(FILE, BODY, VALUE, REL) \
  ASM_OUTPUT_ADDR_VEC_ELT (FILE, VALUE)

/* This is how to output an assembler line
   that says to advance the location counter
   to a multiple of 2**LOG bytes.  */
#define ASM_OUTPUT_ALIGN(FILE,LOG)	\
  if ((LOG) != 0)			\
    fprintf (FILE, "%s%d\n", ALIGN_ASM_OP, 1<<(LOG))

/* Override elfos.h.  */
#undef	ASM_OUTPUT_EXTERNAL_LIBCALL

/* This is how to output an insn to push a register on the stack.
   It need not be very fast code.  */
#define ASM_OUTPUT_REG_PUSH(FILE,REGNO)  \
  asm_fprintf (FILE, "\tsubu\t %R%s,%R%s,%d\n\tst\t %R%s,%R%s,0\n",	\
	       reg_names[STACK_POINTER_REGNUM],				\
	       reg_names[STACK_POINTER_REGNUM],				\
	       (STACK_BOUNDARY / BITS_PER_UNIT),			\
	       reg_names[REGNO],					\
	       reg_names[STACK_POINTER_REGNUM])

/* Length in instructions of the code output by ASM_OUTPUT_REG_PUSH.  */
#define REG_PUSH_LENGTH 2

/* This is how to output an insn to pop a register from the stack.  */
#define ASM_OUTPUT_REG_POP(FILE,REGNO)  \
  asm_fprintf (FILE, "\tld\t %R%s,%R%s,0\n\taddu\t %R%s,%R%s,%d\n",	\
	       reg_names[REGNO],					\
	       reg_names[STACK_POINTER_REGNUM],				\
	       reg_names[STACK_POINTER_REGNUM],				\
	       reg_names[STACK_POINTER_REGNUM],				\
	       (STACK_BOUNDARY / BITS_PER_UNIT))

/* Length in instructions of the code output by ASM_OUTPUT_REG_POP.  */
#define REG_POP_LENGTH 2

/* Macros for debug information */
#define DEBUGGER_AUTO_OFFSET(X) \
  (m88k_debugger_offset (X, 0) + (m88k_stack_size - m88k_fp_offset))

#define DEBUGGER_ARG_OFFSET(OFFSET, X) \
  (m88k_debugger_offset (X, OFFSET) + (m88k_stack_size - m88k_fp_offset))

/* Jump tables consist of branch instructions and should be output in
   the text section.  When we use a table of addresses, we explicitly
   change to the readonly data section.  */
#define JUMP_TABLES_IN_TEXT_SECTION 1

/* Print operand X (an rtx) in assembler syntax to file FILE.
   CODE is a letter or dot (`z' in `%z0') or 0 if no letter was specified.
   For `%' followed by punctuation, CODE is the punctuation and X is null.  */
#define PRINT_OPERAND_PUNCT_VALID_P(c) \
  ((c) == '#' || (c) == '.' || (c) == '!' || (c) == '*' || (c) == ';')

#define PRINT_OPERAND(FILE, X, CODE) print_operand (FILE, X, CODE)

/* Print a memory address as an operand to reference that memory location.  */
#define PRINT_OPERAND_ADDRESS(FILE, ADDR) print_operand_address (FILE, ADDR)

/* This says not to strength reduce the addr calculations within loops
   (otherwise it does not take advantage of m88k scaled loads and stores */
#define DONT_REDUCE_ADDR

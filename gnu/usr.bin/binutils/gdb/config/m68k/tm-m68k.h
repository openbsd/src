/* Parameters for execution on a 68000 series machine.
   Copyright 1986, 1987, 1989, 1990, 1992 Free Software Foundation, Inc.

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

/* Generic 68000 stuff, to be included by other tm-*.h files.  */

#define IEEE_FLOAT 1

/* Define the bit, byte, and word ordering of the machine.  */
#define TARGET_BYTE_ORDER BIG_ENDIAN

/* Offset from address of function to start of its code.
   Zero on most machines.  */

#define FUNCTION_START_OFFSET 0

/* Advance PC across any function entry prologue instructions
   to reach some "real" code.  */

#if !defined(SKIP_PROLOGUE)
#define SKIP_PROLOGUE(ip)   {(ip) = m68k_skip_prologue(ip);}
extern CORE_ADDR m68k_skip_prologue PARAMS ((CORE_ADDR ip));
#endif

/* Immediately after a function call, return the saved pc.
   Can't always go through the frames for this because on some machines
   the new frame is not set up until the new function executes
   some instructions.  */

#ifdef __STDC__
struct frame_info;
struct frame_saved_regs;
#endif

extern CORE_ADDR m68k_saved_pc_after_call PARAMS ((struct frame_info *));
extern void m68k_find_saved_regs PARAMS ((struct frame_info *, struct frame_saved_regs *));

#define SAVED_PC_AFTER_CALL(frame) \
  m68k_saved_pc_after_call(frame)

/* Stack grows downward.  */

#define INNER_THAN <

/* Stack must be kept short aligned when doing function calls.  */

#define STACK_ALIGN(ADDR) (((ADDR) + 1) & ~1)

/* Sequence of bytes for breakpoint instruction.
   This is a TRAP instruction.  The last 4 bits (0xf below) is the
   vector.  Systems which don't use 0xf should define BPT_VECTOR
   themselves before including this file.  */

#if !defined (BPT_VECTOR)
#define BPT_VECTOR 0xf
#endif

#if !defined (BREAKPOINT)
#define BREAKPOINT {0x4e, (0x40 | BPT_VECTOR)}
#endif

/* We default to vector 1 for the "remote" target, but allow targets
   to override.  */
#if !defined (REMOTE_BPT_VECTOR)
#define REMOTE_BPT_VECTOR 1
#endif

#if !defined (REMOTE_BREAKPOINT)
#define REMOTE_BREAKPOINT {0x4e, (0x40 | REMOTE_BPT_VECTOR)}
#endif

/* If your kernel resets the pc after the trap happens you may need to
   define this before including this file.  */

#if !defined (DECR_PC_AFTER_BREAK)
#define DECR_PC_AFTER_BREAK 2
#endif

/* Nonzero if instruction at PC is a return instruction.  */
/* Allow any of the return instructions, including a trapv and a return
   from interupt.  */

#define ABOUT_TO_RETURN(pc) ((read_memory_integer (pc, 2) & ~0x3) == 0x4e74)

/* Say how long (ordinary) registers are.  This is a piece of bogosity
   used in push_word and a few other places; REGISTER_RAW_SIZE is the
   real way to know how big a register is.  */

#define REGISTER_SIZE 4

#define REGISTER_BYTES_FP (16*4 + 8 + 8*12 + 3*4)
#define REGISTER_BYTES_NOFP (16*4 + 8)

#ifndef NUM_REGS
#define NUM_REGS 29
#endif

#ifndef REGISTER_BYTES_OK
#define REGISTER_BYTES_OK(b) \
   ((b) == REGISTER_BYTES_FP \
    || (b) == REGISTER_BYTES_NOFP)
#endif

#ifndef REGISTER_BYTES
#define REGISTER_BYTES (16*4 + 8 + 8*12 + 3*4)
#endif

/* Index within `registers' of the first byte of the space for
   register N.  */

#define REGISTER_BYTE(N)  \
 ((N) >= FPC_REGNUM ? (((N) - FPC_REGNUM) * 4) + 168	\
  : (N) >= FP0_REGNUM ? (((N) - FP0_REGNUM) * 12) + 72	\
  : (N) * 4)

/* Number of bytes of storage in the actual machine representation
   for register N.  On the 68000, all regs are 4 bytes
   except the floating point regs which are 12 bytes.  */
/* Note that the unsigned cast here forces the result of the
   subtraction to very high positive values if N < FP0_REGNUM */

#define REGISTER_RAW_SIZE(N) (((unsigned)(N) - FP0_REGNUM) < 8 ? 12 : 4)

/* Number of bytes of storage in the program's representation
   for register N.  On the 68000, all regs are 4 bytes
   except the floating point regs which are 8-byte doubles.  */

#define REGISTER_VIRTUAL_SIZE(N) (((unsigned)(N) - FP0_REGNUM) < 8 ? 8 : 4)

/* Largest value REGISTER_RAW_SIZE can have.  */

#define MAX_REGISTER_RAW_SIZE 12

/* Largest value REGISTER_VIRTUAL_SIZE can have.  */

#define MAX_REGISTER_VIRTUAL_SIZE 8

/* Nonzero if register N requires conversion
   from raw format to virtual format.  */

#define REGISTER_CONVERTIBLE(N) (((unsigned)(N) - FP0_REGNUM) < 8)

#include "floatformat.h"

/* Convert data from raw format for register REGNUM in buffer FROM
   to virtual format with type TYPE in buffer TO.  */

#define REGISTER_CONVERT_TO_VIRTUAL(REGNUM,TYPE,FROM,TO) \
{ \
  double dbl_tmp_val; \
  floatformat_to_double (&floatformat_m68881_ext, (FROM), &dbl_tmp_val); \
  store_floating ((TO), TYPE_LENGTH (TYPE), dbl_tmp_val); \
}

/* Convert data from virtual format with type TYPE in buffer FROM
   to raw format for register REGNUM in buffer TO.  */

#define REGISTER_CONVERT_TO_RAW(TYPE,REGNUM,FROM,TO)	\
{ \
  double dbl_tmp_val = extract_floating ((FROM), TYPE_LENGTH (TYPE)); \
  floatformat_from_double (&floatformat_m68881_ext, &dbl_tmp_val, (TO)); \
}

/* Return the GDB type object for the "standard" data type
   of data in register N.  */
/* Note, for registers which contain addresses return
   pointer to void, not pointer to char, because we don't
   want to attempt to print the string after printing the address.  */
#define REGISTER_VIRTUAL_TYPE(N) \
 (((unsigned)(N) - FP0_REGNUM) < 8 ? builtin_type_double :           \
  (N) == PC_REGNUM || (N) == FP_REGNUM || (N) == SP_REGNUM ?         \
  lookup_pointer_type (builtin_type_void) : builtin_type_int)

/* Initializer for an array of names of registers.
   Entries beyond the first NUM_REGS are ignored.  */

#define REGISTER_NAMES  \
 {"d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", \
  "a0", "a1", "a2", "a3", "a4", "a5", "fp", "sp", \
  "ps", "pc",  \
  "fp0", "fp1", "fp2", "fp3", "fp4", "fp5", "fp6", "fp7", \
  "fpcontrol", "fpstatus", "fpiaddr", "fpcode", "fpflags" }

/* Register numbers of various important registers.
   Note that some of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and some are "phony" register numbers which are too large
   to be actual register numbers as far as the user is concerned
   but do serve to get the desired values when passed to read_register.  */

#define D0_REGNUM 0
#define A0_REGNUM 8
#define A1_REGNUM 9
#define FP_REGNUM 14		/* Contains address of executing stack frame */
#define SP_REGNUM 15		/* Contains address of top of stack */
#define PS_REGNUM 16		/* Contains processor status */
#define PC_REGNUM 17		/* Contains program counter */
#define FP0_REGNUM 18		/* Floating point register 0 */
#define FPC_REGNUM 26		/* 68881 control register */
#define FPS_REGNUM 27		/* 68881 status register */
#define FPI_REGNUM 28		/* 68881 iaddr register */

/* Store the address of the place in which to copy the structure the
   subroutine will return.  This is called from call_function. */

#define STORE_STRUCT_RETURN(ADDR, SP) \
  { write_register (A1_REGNUM, (ADDR)); }

/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  This is assuming that floating point values are returned
   as doubles in d0/d1.  */

#if !defined (EXTRACT_RETURN_VALUE)
#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF) \
  memcpy ((VALBUF),							\
	  (char *)(REGBUF) +						\
	         (TYPE_LENGTH(TYPE) >= 4 ? 0 : 4 - TYPE_LENGTH(TYPE)),	\
	  TYPE_LENGTH(TYPE))
#endif

/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.  Assumes floats are passed
   in d0/d1.  */

#if !defined (STORE_RETURN_VALUE)
#define STORE_RETURN_VALUE(TYPE,VALBUF) \
  write_register_bytes (0, VALBUF, TYPE_LENGTH (TYPE))
#endif

/* Extract from an array REGBUF containing the (raw) register state
   the address in which a function should return its structure value,
   as a CORE_ADDR (or an expression that can be used as one).  */

#define EXTRACT_STRUCT_VALUE_ADDRESS(REGBUF) (*(CORE_ADDR *)(REGBUF))

/* Describe the pointer in each stack frame to the previous stack frame
   (its caller).  */

/* FRAME_CHAIN takes a frame's nominal address and produces the frame's
   chain-pointer.
   In the case of the 68000, the frame's nominal address
   is the address of a 4-byte word containing the calling frame's address.  */

/* If we are chaining from sigtramp, then manufacture a sigtramp frame
   (which isn't really on the stack.  I'm not sure this is right for anything
   but BSD4.3 on an hp300.  */
#define FRAME_CHAIN(thisframe)  \
  (thisframe->signal_handler_caller \
   ? thisframe->frame \
   : (!inside_entry_file ((thisframe)->pc) \
      ? read_memory_integer ((thisframe)->frame, 4) \
      : 0))

/* Define other aspects of the stack frame.  */

/* A macro that tells us whether the function invocation represented
   by FI does not have a frame on the stack associated with it.  If it
   does not, FRAMELESS is set to 1, else 0.  */
#define FRAMELESS_FUNCTION_INVOCATION(FI, FRAMELESS) \
  do { \
    if ((FI)->signal_handler_caller) \
      (FRAMELESS) = 0; \
    else \
      (FRAMELESS) = frameless_look_for_prologue(FI); \
  } while (0)

/* This was determined by experimentation on hp300 BSD 4.3.  Perhaps
   it corresponds to some offset in /usr/include/sys/user.h or
   something like that.  Using some system include file would
   have the advantage of probably being more robust in the face
   of OS upgrades, but the disadvantage of being wrong for
   cross-debugging.  */

#define SIG_PC_FP_OFFSET 530

#define FRAME_SAVED_PC(FRAME) \
  (((FRAME)->signal_handler_caller \
    ? ((FRAME)->next \
       ? read_memory_integer ((FRAME)->next->frame + SIG_PC_FP_OFFSET, 4) \
       : read_memory_integer (read_register (SP_REGNUM) \
			      + SIG_PC_FP_OFFSET - 8, 4) \
       ) \
    : read_memory_integer ((FRAME)->frame + 4, 4)) \
   )

#define FRAME_ARGS_ADDRESS(fi) ((fi)->frame)

#define FRAME_LOCALS_ADDRESS(fi) ((fi)->frame)

/* Set VAL to the number of args passed to frame described by FI.
   Can set VAL to -1, meaning no way to tell.  */

/* We can't tell how many args there are
   now that the C compiler delays popping them.  */
#if !defined (FRAME_NUM_ARGS)
#define FRAME_NUM_ARGS(val,fi) (val = -1)
#endif

/* Return number of bytes at start of arglist that are not really args.  */

#define FRAME_ARGS_SKIP 8

/* Put here the code to store, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.  */

#if !defined (FRAME_FIND_SAVED_REGS)
#define FRAME_FIND_SAVED_REGS(fi,fsr) m68k_find_saved_regs ((fi), &(fsr))
#endif /* no FIND_FRAME_SAVED_REGS.  */


/* Things needed for making the inferior call functions.  */

/* The CALL_DUMMY macro is the sequence of instructions, as disassembled
   by gdb itself:

   These instructions exist only so that m68k_find_saved_regs can parse
   them as a "prologue"; they are never executed.

	fmovemx fp0-fp7,sp@-			0xf227 0xe0ff
	moveml d0-a5,sp@-			0x48e7 0xfffc
	clrw sp@-				0x4267
	movew ccr,sp@-				0x42e7

   The arguments are pushed at this point by GDB; no code is needed in
   the dummy for this.  The CALL_DUMMY_START_OFFSET gives the position
   of the following jsr instruction.  That is where we start
   executing.

	jsr @#0x32323232			0x4eb9 0x3232 0x3232
	addal #0x69696969,sp			0xdffc 0x6969 0x6969
	trap #<your BPT_VECTOR number here>	0x4e4?
	nop					0x4e71

   Note this is CALL_DUMMY_LENGTH bytes (28 for the above example).

   The dummy frame always saves the floating-point registers, whether they
   actually exist on this target or not.  */

/* FIXME: Wrong to hardwire this as BPT_VECTOR when sometimes it
   should be REMOTE_BPT_VECTOR.  Best way to fix it would be to define
   CALL_DUMMY_BREAKPOINT_OFFSET.  */

#define CALL_DUMMY {0xf227e0ff, 0x48e7fffc, 0x426742e7, 0x4eb93232, 0x3232dffc, 0x69696969, (0x4e404e71 | (BPT_VECTOR << 16))}
#define CALL_DUMMY_LENGTH 28		/* Size of CALL_DUMMY */
#define CALL_DUMMY_START_OFFSET 12	/* Offset to jsr instruction*/
#define CALL_DUMMY_BREAKPOINT_OFFSET (CALL_DUMMY_START_OFFSET + 12)

/* Insert the specified number of args and function address
   into a call sequence of the above form stored at DUMMYNAME.
   We use the BFD routines to store a big-endian value of known size.  */

#define FIX_CALL_DUMMY(dummyname, pc, fun, nargs, args, type, gcc_p)     \
{ bfd_putb32 (fun,     (unsigned char *) dummyname + CALL_DUMMY_START_OFFSET + 2);  \
  bfd_putb32 (nargs*4, (unsigned char *) dummyname + CALL_DUMMY_START_OFFSET + 8); }

/* Push an empty stack frame, to record the current PC, etc.  */

#define PUSH_DUMMY_FRAME	{ m68k_push_dummy_frame (); }

extern void m68k_push_dummy_frame PARAMS ((void));

extern void m68k_pop_frame PARAMS ((void));

/* Discard from the stack the innermost frame, restoring all registers.  */

#define POP_FRAME		{ m68k_pop_frame (); }

/* Offset from SP to first arg on stack at first instruction of a function */

#define SP_ARG0 (1 * 4)

#define TARGET_M68K

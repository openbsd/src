/* Parameters for execution on a z8000 series machine.
   Copyright 1992, 1993 Free Software Foundation, Inc.

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

#define IEEE_FLOAT 1

#undef TARGET_INT_BIT
#undef TARGET_LONG_BIT
#undef TARGET_SHORT_BIT
#undef TARGET_PTR_BIT

#define TARGET_SHORT_BIT 16
#define TARGET_INT_BIT 16
#define TARGET_LONG_BIT 32
#define TARGET_PTR_BIT (BIG ? 32: 16)

/* Define the bit, byte, and word ordering of the machine.  */
#define TARGET_BYTE_ORDER BIG_ENDIAN

/* Offset from address of function to start of its code.
   Zero on most machines.  */

#define FUNCTION_START_OFFSET 0

/* Advance PC across any function entry prologue instructions
   to reach some "real" code.  */

#define SKIP_PROLOGUE(ip)   {(ip) = z8k_skip_prologue(ip);}
extern CORE_ADDR mz8k_skip_prologue PARAMS ((CORE_ADDR ip));


/* Immediately after a function call, return the saved pc.
   Can't always go through the frames for this because on some machines
   the new frame is not set up until the new function executes
   some instructions.  */

#define SAVED_PC_AFTER_CALL(frame) saved_pc_after_call(frame)

/* Stack grows downward.  */

#define INNER_THAN <

/* Sequence of bytes for breakpoint instruction. */

#define BREAKPOINT {0x36,0x00}

/* If your kernel resets the pc after the trap happens you may need to
   define this before including this file.  */

#define DECR_PC_AFTER_BREAK 0

/* Nonzero if instruction at PC is a return instruction.  */
/* Allow any of the return instructions, including a trapv and a return
   from interupt.  */

#define ABOUT_TO_RETURN(pc) about_to_return(pc)

/* Say how long registers are.  */

#define REGISTER_TYPE unsigned int

#define NUM_REGS 	23   /* 16 registers + 1 ccr + 1 pc + 3 debug
				regs + fake fp + fake sp*/
#define REGISTER_BYTES  (NUM_REGS *4)

/* Index within `registers' of the first byte of the space for
   register N.  */

#define REGISTER_BYTE(N)  ((N)*4)

/* Number of bytes of storage in the actual machine representation
   for register N.  On the z8k, all but the pc are 2 bytes, but we
   keep them all as 4 bytes and trim them on I/O */


#define REGISTER_RAW_SIZE(N) (((N) < 16)? 2:4)

/* Number of bytes of storage in the program's representation
   for register N.  */

#define REGISTER_VIRTUAL_SIZE(N) REGISTER_RAW_SIZE(N) 

/* Largest value REGISTER_RAW_SIZE can have.  */

#define MAX_REGISTER_RAW_SIZE 4

/* Largest value REGISTER_VIRTUAL_SIZE can have.  */

#define MAX_REGISTER_VIRTUAL_SIZE 4

/* Return the GDB type object for the "standard" data type
   of data in register N.  */

#define REGISTER_VIRTUAL_TYPE(N) \
 (REGISTER_VIRTUAL_SIZE(N) == 2? builtin_type_unsigned_int : builtin_type_long)

/*#define INIT_FRAME_PC(x,y) init_frame_pc(x,y)*/
/* Initializer for an array of names of registers.
   Entries beyond the first NUM_REGS are ignored.  */

#define REGISTER_NAMES  \
 {"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", \
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", \
  "ccr", "pc", "cycles","insts","time","fp","sp"}

/* Register numbers of various important registers.
   Note that some of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and some are "phony" register numbers which are too large
   to be actual register numbers as far as the user is concerned
   but do serve to get the desired values when passed to read_register.  */

#define CCR_REGNUM 	16	/* Contains processor status */
#define PC_REGNUM 	17	/* Contains program counter */
#define CYCLES_REGNUM 	18
#define INSTS_REGNUM 	19
#define TIME_REGNUM 	20
#define FP_REGNUM 	21	/* Contains fp, whatever memory model */
#define SP_REGNUM 	22	/* Conatins sp, whatever memory model */



#define PTR_SIZE (BIG ? 4: 2)
#define PTR_MASK (BIG ? 0xff00ffff : 0x0000ffff)

/* Store the address of the place in which to copy the structure the
   subroutine will return.  This is called from call_function. */

#define STORE_STRUCT_RETURN(ADDR, SP) abort();

/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  This is assuming that floating point values are returned
   as doubles in d0/d1.  */


#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF) \
  memcpy(VALBUF, REGBUF + REGISTER_BYTE(2), TYPE_LENGTH(TYPE));

/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format. */

#define STORE_RETURN_VALUE(TYPE,VALBUF) abort();

/* Extract from an array REGBUF containing the (raw) register state
   the address in which a function should return its structure value,
   as a CORE_ADDR (or an expression that can be used as one).  */

#define EXTRACT_STRUCT_VALUE_ADDRESS(REGBUF) (*(CORE_ADDR *)(REGBUF))

/* Describe the pointer in each stack frame to the previous stack frame
   (its caller).  */

/* FRAME_CHAIN takes a frame's nominal address and produces the frame's
   chain-pointer.
   In the case of the Z8000, the frame's nominal address
   is the address of a ptr sized byte word containing the calling
   frame's address.  */

#define FRAME_CHAIN(thisframe) frame_chain(thisframe);



/* Define other aspects of the stack frame.  */

/* A macro that tells us whether the function invocation represented
   by FI does not have a frame on the stack associated with it.  If it
   does not, FRAMELESS is set to 1, else 0.  */
#define FRAMELESS_FUNCTION_INVOCATION(FI, FRAMELESS) \
  (FRAMELESS) = frameless_look_for_prologue(FI)

#define FRAME_SAVED_PC(FRAME) frame_saved_pc(FRAME)

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



/* Things needed for making the inferior call functions.
   It seems like every m68k based machine has almost identical definitions
   in the individual machine's configuration files.  Most other cpu types
   (mips, i386, etc) have routines in their *-tdep.c files to handle this
   for most configurations.  The m68k family should be able to do this as
   well.  These macros can still be overridden when necessary.  */

/* The CALL_DUMMY macro is the sequence of instructions, as disassembled
   by gdb itself:

	fmovemx fp0-fp7,sp@-			0xf227 0xe0ff
	moveml d0-a5,sp@-			0x48e7 0xfffc
	clrw sp@-				0x4267
	movew ccr,sp@-				0x42e7

	/..* The arguments are pushed at this point by GDB;
	no code is needed in the dummy for this.
	The CALL_DUMMY_START_OFFSET gives the position of 
	the following jsr instruction.  *../

	jsr @#0x32323232			0x4eb9 0x3232 0x3232
	addal #0x69696969,sp			0xdffc 0x6969 0x6969
	trap #<your BPT_VECTOR number here>	0x4e4?
	nop					0x4e71

   Note this is CALL_DUMMY_LENGTH bytes (28 for the above example).
   We actually start executing at the jsr, since the pushing of the
   registers is done by PUSH_DUMMY_FRAME.  If this were real code,
   the arguments for the function called by the jsr would be pushed
   between the moveml and the jsr, and we could allow it to execute through.
   But the arguments have to be pushed by GDB after the PUSH_DUMMY_FRAME is
   done, and we cannot allow the moveml to push the registers again lest
   they be taken for the arguments.  */


#define CALL_DUMMY { 0 }
#define CALL_DUMMY_LENGTH 24		/* Size of CALL_DUMMY */
#define CALL_DUMMY_START_OFFSET 8	/* Offset to jsr instruction*/


/* Insert the specified number of args and function address
   into a call sequence of the above form stored at DUMMYNAME.
   We use the BFD routines to store a big-endian value of known size.  */

#define FIX_CALL_DUMMY(dummyname, pc, fun, nargs, args, type, gcc_p)     \
{ bfd_putb32 (fun,     (char *) dummyname + CALL_DUMMY_START_OFFSET + 2);  \
  bfd_putb32 (nargs*4, (char *) dummyname + CALL_DUMMY_START_OFFSET + 8); }

/* Push an empty stack frame, to record the current PC, etc.  */

#define PUSH_DUMMY_FRAME	{ z8k_push_dummy_frame (); }

extern void z8k_push_dummy_frame PARAMS ((void));

extern void z8k_pop_frame PARAMS ((void));

/* Discard from the stack the innermost frame, restoring all registers.  */

#define POP_FRAME		{ z8k_pop_frame (); }

/* Offset from SP to first arg on stack at first instruction of a function */

#define SP_ARG0 (1 * 4)

#define ADDR_BITS_REMOVE(x) addr_bits_remove(x)
int sim_z8001_mode;
#define BIG (sim_z8001_mode)

#define read_memory_short(x)  (read_memory_integer(x,2) & 0xffff)

#define NO_STD_REGS

#define	PRINT_REGISTER_HOOK(regno) print_register_hook(regno)


#define INIT_EXTRA_SYMTAB_INFO \
  z8k_set_pointer_size(objfile->obfd->arch_info->bits_per_address);

#define REGISTER_SIZE 4


/* Parameters for execution on a H8/500 series machine.
   Copyright (C) 1993, 1995 Free Software Foundation, Inc.

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

/* Contributed by Steve Chamberlain sac@cygnus.com */

#define GDB_TARGET_IS_H8500

#define IEEE_FLOAT 1

/* Define the bit, byte, and word ordering of the machine.  */

#define TARGET_BYTE_ORDER BIG_ENDIAN

/* Define the sizes of integers and pointers.  */

#define TARGET_INT_BIT 16

#define TARGET_LONG_BIT 32

#define TARGET_PTR_BIT (minimum_mode ? 16 : 32)

/* Offset from address of function to start of its code.
   Zero on most machines.  */

#define FUNCTION_START_OFFSET 0

/* Advance PC across any function entry prologue instructions
   to reach some "real" code.  */

#define SKIP_PROLOGUE(ip)   { (ip) = h8500_skip_prologue(ip); }
extern CORE_ADDR h8500_skip_prologue PARAMS ((CORE_ADDR));

/* Immediately after a function call, return the saved pc.
   Can't always go through the frames for this because on some machines
   the new frame is not set up until the new function executes
   some instructions.  */

#define SAVED_PC_AFTER_CALL(frame) saved_pc_after_call()
extern CORE_ADDR saved_pc_after_call PARAMS ((void));

/* Stack grows downward.  */

#define INNER_THAN <

/* Illegal instruction - used by the simulator for breakpoint
   detection */

#define BREAKPOINT {0x0b}

/* If your kernel resets the pc after the trap happens you may need to
   define this before including this file.  */

#define DECR_PC_AFTER_BREAK 0

#if 0 /* never used */
/* Nonzero if instruction at PC is a return instruction.  */

#define ABOUT_TO_RETURN(pc) about_to_return(pc)
#endif

/* Say how long registers are.  */

#define REGISTER_TYPE  unsigned long

/* Say how much memory is needed to store a copy of the register set */

#define REGISTER_BYTES    (NUM_REGS * 4) 

/* Index within `registers' of the first byte of the space for
   register N.  */

#define REGISTER_BYTE(N)  ((N)*4)

/* Number of bytes of storage in the actual machine representation
   for register N.  */

#define REGISTER_RAW_SIZE(N) h8500_register_size(N)
extern int h8500_register_size PARAMS ((int regno));

#define REGISTER_SIZE 4

#define REGISTER_VIRTUAL_SIZE(N) h8500_register_size(N)

/* Largest value REGISTER_RAW_SIZE can have.  */

#define MAX_REGISTER_RAW_SIZE 4

/* Largest value REGISTER_VIRTUAL_SIZE can have.  */

#define MAX_REGISTER_VIRTUAL_SIZE 4

/* Return the GDB type object for the "standard" data type
   of data in register N.  */

#define REGISTER_VIRTUAL_TYPE(N) h8500_register_virtual_type(N)
extern struct type *h8500_register_virtual_type PARAMS ((int regno));

/* Initializer for an array of names of registers.
   Entries beyond the first NUM_REGS are ignored.  */

#define REGISTER_NAMES \
  { "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", \
   "pr0","pr1","pr2","pr3","pr4","pr5","pr6","pr7", \
    "cp", "dp", "ep", "tp", "sr", "pc"}

/* Register numbers of various important registers.  Note that some of
   these values are "real" register numbers, and correspond to the
   general registers of the machine, and some are "phony" register
   numbers which are too large to be actual register numbers as far as
   the user is concerned but do serve to get the desired values when
   passed to read_register.  */

#define R0_REGNUM	0
#define R1_REGNUM	1
#define R2_REGNUM	2
#define R3_REGNUM	3
#define R4_REGNUM	4
#define R5_REGNUM	5
#define R6_REGNUM	6
#define R7_REGNUM	7

#define PR0_REGNUM	8
#define PR1_REGNUM	9
#define PR2_REGNUM	10
#define PR3_REGNUM	11
#define PR4_REGNUM	12
#define PR5_REGNUM	13
#define PR6_REGNUM	14
#define PR7_REGNUM	15

#define SEG_C_REGNUM	16	/* Segment registers */
#define SEG_D_REGNUM	17
#define SEG_E_REGNUM	18
#define SEG_T_REGNUM	19

#define CCR_REGNUM      20	/* Contains processor status */
#define PC_REGNUM       21	/* Contains program counter */

#define NUM_REGS 	22

#define SP_REGNUM       PR7_REGNUM /* Contains address of top of stack */
#define FP_REGNUM       PR6_REGNUM /* Contains address of executing stack frame */

#define PTR_SIZE (minimum_mode ? 2 : 4)
#define PTR_MASK (minimum_mode ? 0x0000ffff : 0x00ffffff)

/* Store the address of the place in which to copy the structure the
   subroutine will return.  This is called from call_function. */

/*#define STORE_STRUCT_RETURN(ADDR, SP) \
  { write_register (0, (ADDR)); abort(); }*/

/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  */

#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF) \
  memcpy (VALBUF, (char *)(REGBUF), TYPE_LENGTH(TYPE))

/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.  */

#define STORE_RETURN_VALUE(TYPE,VALBUF) \
  write_register_bytes (0, VALBUF, TYPE_LENGTH (TYPE))

/* Extract from an array REGBUF containing the (raw) register state
   the address in which a function should return its structure value,
   as a CORE_ADDR (or an expression that can be used as one).  */

#define EXTRACT_STRUCT_VALUE_ADDRESS(REGBUF) (*(CORE_ADDR *)(REGBUF))


/* Define other aspects of the stack frame.  */

/* A macro that tells us whether the function invocation represented
   by FI does not have a frame on the stack associated with it.  If it
   does not, FRAMELESS is set to 1, else 0.  */

#define FRAMELESS_FUNCTION_INVOCATION(FI, FRAMELESS) \
  (FRAMELESS) = frameless_look_for_prologue(FI)

/* Any function with a frame looks like this
   SECOND ARG
   FIRST ARG
   RET PC
   SAVED R2
   SAVED R3
   SAVED FP   <-FP POINTS HERE
   LOCALS0
   LOCALS1    <-SP POINTS HERE
   
   */

#define INIT_EXTRA_FRAME_INFO(fromleaf, fci)  ;
/*       (fci)->frame |= read_register(SEG_T_REGNUM) << 16;*/

#define FRAME_CHAIN(FRAME) h8500_frame_chain(FRAME)
struct frame_info;
extern CORE_ADDR h8500_frame_chain PARAMS ((struct frame_info *));

#define FRAME_SAVED_PC(FRAME) frame_saved_pc(FRAME)
extern CORE_ADDR frame_saved_pc PARAMS ((struct frame_info *frame));

#define FRAME_ARGS_ADDRESS(fi) ((fi)->frame)

#define FRAME_LOCALS_ADDRESS(fi) ((fi)->frame)

/* Set VAL to the number of args passed to frame described by FI.
   Can set VAL to -1, meaning no way to tell.  */

/* We can't tell how many args there are
   now that the C compiler delays popping them.  */

#define FRAME_NUM_ARGS(val,fi) (val = -1)

/* Return number of bytes at start of arglist that are not really args.  */

#define FRAME_ARGS_SKIP 0

/* Put here the code to store, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.  */

#define FRAME_FIND_SAVED_REGS(frame_info, frame_saved_regs)	    \
   frame_find_saved_regs(frame_info, &(frame_saved_regs))
struct frame_saved_regs;
extern void frame_find_saved_regs PARAMS ((struct frame_info *frame_info, struct frame_saved_regs *frame_saved_regs));


/* Discard from the stack the innermost frame, restoring all registers.  */

#define POP_FRAME { h8500_pop_frame (); }
extern void h8500_pop_frame PARAMS ((void));

#define SHORT_INT_MAX 32767
#define SHORT_INT_MIN -32768

#define NAMES_HAVE_UNDERSCORE

typedef unsigned short INSN_WORD;

#define ADDR_BITS_REMOVE(addr) ((addr) & 0xffffff)

#define read_memory_short(x)  (read_memory_integer(x,2) & 0xffff)

#define	PRINT_REGISTER_HOOK(regno) print_register_hook(regno)
extern void print_register_hook PARAMS ((int));

extern int minimum_mode;

#define CALL_DUMMY_LENGTH 10

/* Fake variables to make it easy to use 24 bit register pointers */

#define IS_TRAPPED_INTERNALVAR h8500_is_trapped_internalvar
extern int h8500_is_trapped_internalvar PARAMS ((char *name));

#define VALUE_OF_TRAPPED_INTERNALVAR h8500_value_of_trapped_internalvar
extern struct value * h8500_value_of_trapped_internalvar (/* struct internalvar *var */);

#define SET_TRAPPED_INTERNALVAR h8500_set_trapped_internalvar
extern void h8500_set_trapped_internalvar (/* struct internalvar *var, value newval, int bitpos, int bitsize, int offset */);

extern CORE_ADDR h8500_read_sp PARAMS ((void));
extern void h8500_write_sp PARAMS ((CORE_ADDR));

extern CORE_ADDR h8500_read_fp PARAMS ((void));
extern void h8500_write_fp PARAMS ((CORE_ADDR));

extern CORE_ADDR h8500_read_pc PARAMS ((int));
extern void h8500_write_pc PARAMS ((CORE_ADDR, int));

#define TARGET_READ_SP() h8500_read_sp()
#define TARGET_WRITE_SP(x) h8500_write_sp(x)

#define TARGET_READ_PC(pid) h8500_read_pc(pid)
#define TARGET_WRITE_PC(x,pid) h8500_write_pc(x,pid)

#define TARGET_READ_FP() h8500_read_fp()
#define TARGET_WRITE_FP(x) h8500_write_fp(x)

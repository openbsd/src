/* Target machine sub-parameters for SPARC, for GDB, the GNU debugger.
   This is included by other tm-*.h files to define SPARC cpu-related info.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1994
   Free Software Foundation, Inc.
   Contributed by Michael Tiemann (tiemann@mcc.com)

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

#define TARGET_BYTE_ORDER BIG_ENDIAN

/* Floating point is IEEE compatible.  */
#define IEEE_FLOAT

/* If an argument is declared "register", Sun cc will keep it in a register,
   never saving it onto the stack.  So we better not believe the "p" symbol
   descriptor stab.  */

#define USE_REGISTER_NOT_ARG

/* When passing a structure to a function, Sun cc passes the address
   not the structure itself.  It (under SunOS4) creates two symbols,
   which we need to combine to a LOC_REGPARM.  Gcc version two (as of
   1.92) behaves like sun cc.  REG_STRUCT_HAS_ADDR is smart enough to
   distinguish between Sun cc, gcc version 1 and gcc version 2.  */

#define REG_STRUCT_HAS_ADDR(gcc_p,type) (gcc_p != 1)

/* Sun /bin/cc gets this right as of SunOS 4.1.x.  We need to define
   BELIEVE_PCC_PROMOTION to get this right now that the code which
   detects gcc2_compiled. is broken.  This loses for SunOS 4.0.x and
   earlier.  */

#define BELIEVE_PCC_PROMOTION 1

/* For acc, there's no need to correct LBRAC entries by guessing how
   they should work.  In fact, this is harmful because the LBRAC
   entries now all appear at the end of the function, not intermixed
   with the SLINE entries.  n_opt_found detects acc for Solaris binaries;
   function_stab_type detects acc for SunOS4 binaries.

   For binary from SunOS4 /bin/cc, need to correct LBRAC's.

   For gcc, like acc, don't correct.  */

#define	SUN_FIXED_LBRAC_BUG \
  (n_opt_found \
   || function_stab_type == N_STSYM \
   || function_stab_type == N_GSYM \
   || processing_gcc_compilation)

/* Do variables in the debug stabs occur after the N_LBRAC or before it?
   acc: after, gcc: before, SunOS4 /bin/cc: before.  */

#define VARIABLES_INSIDE_BLOCK(desc, gcc_p) \
  (!(gcc_p) \
   && (n_opt_found \
       || function_stab_type == N_STSYM \
       || function_stab_type == N_GSYM))

/* Offset from address of function to start of its code.
   Zero on most machines.  */

#define FUNCTION_START_OFFSET 0

/* Advance PC across any function entry prologue instructions
   to reach some "real" code.  SKIP_PROLOGUE_FRAMELESS_P advances
   the PC past some of the prologue, but stops as soon as it
   knows that the function has a frame.  Its result is equal
   to its input PC if the function is frameless, unequal otherwise.  */

#define SKIP_PROLOGUE(pc) \
  { pc = skip_prologue (pc, 0); }
#define SKIP_PROLOGUE_FRAMELESS_P(pc) \
  { pc = skip_prologue (pc, 1); }
extern CORE_ADDR skip_prologue PARAMS ((CORE_ADDR, int));

/* Immediately after a function call, return the saved pc.
   Can't go through the frames for this because on some machines
   the new frame is not set up until the new function executes
   some instructions.  */

/* On the Sun 4 under SunOS, the compile will leave a fake insn which
   encodes the structure size being returned.  If we detect such
   a fake insn, step past it.  */

#define PC_ADJUST(pc) sparc_pc_adjust(pc)
extern CORE_ADDR sparc_pc_adjust PARAMS ((CORE_ADDR));

#define SAVED_PC_AFTER_CALL(frame) PC_ADJUST (read_register (RP_REGNUM))

/* Stack grows downward.  */

#define INNER_THAN <

/* Stack must be aligned on 64-bit boundaries when synthesizing
   function calls. */

#define STACK_ALIGN(ADDR) (((ADDR) + 7) & -8)

/* Sequence of bytes for breakpoint instruction (ta 1). */

#define BREAKPOINT {0x91, 0xd0, 0x20, 0x01}

/* Amount PC must be decremented by after a breakpoint.
   This is often the number of bytes in BREAKPOINT
   but not always.  */

#define DECR_PC_AFTER_BREAK 0

/* Nonzero if instruction at PC is a return instruction.  */
/* For SPARC, this is either a "jmpl %o7+8,%g0" or "jmpl %i7+8,%g0".

   Note: this does not work for functions returning structures under SunOS.
   v9 does not have such critters though.  */
#define ABOUT_TO_RETURN(pc) \
  ((read_memory_integer (pc, 4)|0x00040000) == 0x81c7e008)

/* Say how long (ordinary) registers are.  This is a piece of bogosity
   used in push_word and a few other places; REGISTER_RAW_SIZE is the
   real way to know how big a register is.  */

#define REGISTER_SIZE 4

/* Number of machine registers */

#define NUM_REGS 72

/* Initializer for an array of names of registers.
   There should be NUM_REGS strings in this initializer.  */

#define REGISTER_NAMES  \
{ "g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",	\
  "o0", "o1", "o2", "o3", "o4", "o5", "sp", "o7",	\
  "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7",	\
  "i0", "i1", "i2", "i3", "i4", "i5", "fp", "i7",	\
								\
  "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",	\
  "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",	\
  "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",	\
  "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",	\
                                                                \
  "y", "psr", "wim", "tbr", "pc", "npc", "fpsr", "cpsr" }

/* Register numbers of various important registers.
   Note that some of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and some are "phony" register numbers which are too large
   to be actual register numbers as far as the user is concerned
   but do serve to get the desired values when passed to read_register.  */

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
#define	Y_REGNUM 64		/* Temp register for multiplication, etc.  */
#define	PS_REGNUM 65		/* Contains processor status */
#define PS_FLAG_CARRY 0x100000	/* Carry bit in PS */
#define	WIM_REGNUM 66		/* Window Invalid Mask (not really supported) */
#define	TBR_REGNUM 67		/* Trap Base Register (not really supported) */
#define	PC_REGNUM 68		/* Contains program counter */
#define	NPC_REGNUM 69           /* Contains next PC */
#define	FPS_REGNUM 70		/* Floating point status register */
#define	CPS_REGNUM 71		/* Coprocessor status register */

/* Total amount of space needed to store our copies of the machine's
   register state, the array `registers'.  On the sparc, `registers'
   contains the ins and locals, even though they are saved on the
   stack rather than with the other registers, and this causes hair
   and confusion in places like pop_frame.  It might be
   better to remove the ins and locals from `registers', make sure
   that get_saved_register can get them from the stack (even in the
   innermost frame), and make this the way to access them.  For the
   frame pointer we would do that via TARGET_READ_FP.  On the other hand,
   that is likely to be confusing or worse for flat frames.  */

#define REGISTER_BYTES (32*4+32*4+8*4)

/* Index within `registers' of the first byte of the space for
   register N.  */
/* ?? */
#define REGISTER_BYTE(N)  ((N)*4)

/* We need to override GET_SAVED_REGISTER so that we can deal with the way
   outs change into ins in different frames.  HAVE_REGISTER_WINDOWS can't
   deal with this case and also handle flat frames at the same time.  */

#define GET_SAVED_REGISTER 1

/* Number of bytes of storage in the actual machine representation
   for register N.  */

/* On the SPARC, all regs are 4 bytes.  */

#define REGISTER_RAW_SIZE(N) (4)

/* Number of bytes of storage in the program's representation
   for register N.  */

/* On the SPARC, all regs are 4 bytes.  */

#define REGISTER_VIRTUAL_SIZE(N) (4)

/* Largest value REGISTER_RAW_SIZE can have.  */

#define MAX_REGISTER_RAW_SIZE 8

/* Largest value REGISTER_VIRTUAL_SIZE can have.  */

#define MAX_REGISTER_VIRTUAL_SIZE 8

/* Return the GDB type object for the "standard" data type
   of data in register N.  */

#define REGISTER_VIRTUAL_TYPE(N) \
 ((N) < 32 ? builtin_type_int : (N) < 64 ? builtin_type_float : \
  builtin_type_int)

/* Writing to %g0 is a noop (not an error or exception or anything like
   that, however).  */

#define CANNOT_STORE_REGISTER(regno) ((regno) == G0_REGNUM)

/* Store the address of the place in which to copy the structure the
   subroutine will return.  This is called from call_function. */

#define STORE_STRUCT_RETURN(ADDR, SP) \
  { target_write_memory ((SP)+(16*4), (char *)&(ADDR), 4); }

/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  */

#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF)	      \
  {      	       	       	       	       	       	       	           \
    if (TYPE_CODE (TYPE) == TYPE_CODE_FLT)		       		   \
      {							       		   \
	memcpy ((VALBUF), ((int *)(REGBUF))+FP0_REGNUM, TYPE_LENGTH(TYPE));\
      }							       		   \
    else						       		   \
      memcpy ((VALBUF),						   	   \
	      (char *)(REGBUF) + REGISTER_RAW_SIZE (O0_REGNUM) * 8 +	   \
	      (TYPE_LENGTH(TYPE) >= REGISTER_RAW_SIZE (O0_REGNUM)	   \
	       ? 0 : REGISTER_RAW_SIZE (O0_REGNUM) - TYPE_LENGTH(TYPE)),   \
	      TYPE_LENGTH(TYPE));					   \
  }

/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.  */
/* On sparc, values are returned in register %o0.  */
#define STORE_RETURN_VALUE(TYPE,VALBUF) \
  {    	       	       	       	       	       	       	       	       	     \
    if (TYPE_CODE (TYPE) == TYPE_CODE_FLT)				     \
      /* Floating-point values are returned in the register pair */          \
      /* formed by %f0 and %f1 (doubles are, anyway).  */                    \
      write_register_bytes (REGISTER_BYTE (FP0_REGNUM), (VALBUF),	     \
			    TYPE_LENGTH (TYPE));			     \
    else								     \
      /* Other values are returned in register %o0.  */                      \
      write_register_bytes (REGISTER_BYTE (O0_REGNUM), (VALBUF),	     \
			    TYPE_LENGTH (TYPE));  \
  }

/* Extract from an array REGBUF containing the (raw) register state
   the address in which a function should return its structure value,
   as a CORE_ADDR (or an expression that can be used as one).  */

#define EXTRACT_STRUCT_VALUE_ADDRESS(REGBUF) \
  (sparc_extract_struct_value_address (REGBUF))

extern CORE_ADDR
sparc_extract_struct_value_address PARAMS ((char [REGISTER_BYTES]));


/* Describe the pointer in each stack frame to the previous stack frame
   (its caller).  */

/* FRAME_CHAIN takes a frame's nominal address
   and produces the frame's chain-pointer. */

/* In the case of the Sun 4, the frame-chain's nominal address
   is held in the frame pointer register.

   On the Sun4, the frame (in %fp) is %sp for the previous frame.
   From the previous frame's %sp, we can find the previous frame's
   %fp: it is in the save area just above the previous frame's %sp.

   If we are setting up an arbitrary frame, we'll need to know where
   it ends.  Hence the following.  This part of the frame cache
   structure should be checked before it is assumed that this frame's
   bottom is in the stack pointer.

   If there isn't a frame below this one, the bottom of this frame is
   in the stack pointer.

   If there is a frame below this one, and the frame pointers are
   identical, it's a leaf frame and the bottoms are the same also.

   Otherwise the bottom of this frame is the top of the next frame.

   The bottom field is misnamed, since it might imply that memory from
   bottom to frame contains this frame.  That need not be true if
   stack frames are allocated in different segments (e.g. some on a
   stack, some on a heap in the data segment).

   GCC 2.6 and later can generate ``flat register window'' code that
   makes frames by explicitly saving those registers that need to be
   saved.  %i7 is used as the frame pointer, and the frame is laid out so
   that flat and non-flat calls can be intermixed freely within a
   program.  Unfortunately for GDB, this means it must detect and record
   the flatness of frames.

   Since the prologue in a flat frame also tells us where fp and pc
   have been stashed (the frame is of variable size, so their location
   is not fixed), it's convenient to record them in the frame info.  */

#ifdef __STDC__
struct frame_info;
#endif

#define EXTRA_FRAME_INFO  \
  CORE_ADDR bottom;  \
  int flat;  \
  /* Following fields only relevant for flat frames.  */ \
  CORE_ADDR pc_addr;  \
  CORE_ADDR fp_addr;  \
  /* Add this to ->frame to get the value of the stack pointer at the */ \
  /* time of the register saves.  */ \
  int sp_offset;

#define INIT_EXTRA_FRAME_INFO(fromleaf, fci) \
  sparc_init_extra_frame_info (fromleaf, fci)
extern void sparc_init_extra_frame_info PARAMS((int, struct frame_info *));

#define	PRINT_EXTRA_FRAME_INFO(fi) \
  { \
    if ((fi) && (fi)->flat) \
      printf_filtered (" flat, pc saved at 0x%x, fp saved at 0x%x\n", \
                       (fi)->pc_addr, (fi)->fp_addr); \
  }

#define FRAME_CHAIN(thisframe) (sparc_frame_chain (thisframe))
extern CORE_ADDR sparc_frame_chain PARAMS ((struct frame_info *));

/* INIT_EXTRA_FRAME_INFO needs the PC to detect flat frames.  */

#define	INIT_FRAME_PC(fromleaf, prev) /* nothing */
#define INIT_FRAME_PC_FIRST(fromleaf, prev) \
  (prev)->pc = ((fromleaf) ? SAVED_PC_AFTER_CALL ((prev)->next) : \
	      (prev)->next ? FRAME_SAVED_PC ((prev)->next) : read_pc ());

/* Define other aspects of the stack frame.  */

/* A macro that tells us whether the function invocation represented
   by FI does not have a frame on the stack associated with it.  If it
   does not, FRAMELESS is set to 1, else 0.  */
#define FRAMELESS_FUNCTION_INVOCATION(FI, FRAMELESS) \
  (FRAMELESS) = frameless_look_for_prologue(FI)

/* The location of I0 w.r.t SP.  This is actually dependent on how the system's
   window overflow/underflow routines are written.  Most vendors save the L regs
   followed by the I regs (at the higher address).  Some vendors get it wrong.
 */

#define	FRAME_SAVED_L0	0
#define	FRAME_SAVED_I0	(8 * REGISTER_RAW_SIZE (L0_REGNUM))

/* Where is the PC for a specific frame */

#define FRAME_SAVED_PC(FRAME) sparc_frame_saved_pc (FRAME)
extern CORE_ADDR sparc_frame_saved_pc PARAMS ((struct frame_info *));

/* If the argument is on the stack, it will be here.  */
#define FRAME_ARGS_ADDRESS(fi) ((fi)->frame)

#define FRAME_STRUCT_ARGS_ADDRESS(fi) ((fi)->frame)

#define FRAME_LOCALS_ADDRESS(fi) ((fi)->frame)

/* Set VAL to the number of args passed to frame described by FI.
   Can set VAL to -1, meaning no way to tell.  */

/* We can't tell how many args there are
   now that the C compiler delays popping them.  */
#define FRAME_NUM_ARGS(val,fi) (val = -1)

/* Return number of bytes at start of arglist that are not really args.  */

#define FRAME_ARGS_SKIP 68

/* Things needed for making the inferior call functions.  */
/*
 * First of all, let me give my opinion of what the DUMMY_FRAME
 * actually looks like.
 *
 *               |                                 |
 *               |                                 |
 *               + - - - - - - - - - - - - - - - - +<-- fp (level 0)
 *               |                                 |
 *               |                                 |
 *               |                                 |
 *               |                                 |
 *               |  Frame of innermost program     |
 *               |           function              |
 *               |                                 |
 *               |                                 |
 *               |                                 |
 *               |                                 |
 *               |                                 |
 *               |---------------------------------|<-- sp (level 0), fp (c)
 *               |                                 |
 *     DUMMY     |             fp0-31              |
 *               |                                 |
 *               |             ------              |<-- fp - 0x80
 *     FRAME     |              g0-7               |<-- fp - 0xa0
 *               |              i0-7               |<-- fp - 0xc0
 *               |             other               |<-- fp - 0xe0
 *               |               ?                 |
 *               |               ?                 |
 *               |---------------------------------|<-- sp' = fp - 0x140
 *               |                                 |
 * xcution start |                                 |
 * sp' + 0x94 -->|        CALL_DUMMY (x code)      |
 *               |                                 |
 *               |                                 |
 *               |---------------------------------|<-- sp'' = fp - 0x200
 *               |  align sp to 8 byte boundary    |
 *               |     ==> args to fn <==          |
 *  Room for     |                                 |
 * i & l's + agg | CALL_DUMMY_STACK_ADJUST = 0x0x44|
 *               |---------------------------------|<-- final sp (variable)
 *               |                                 |
 *               |   Where function called will    |
 *               |           build frame.          |
 *               |                                 |
 *               |                                 |
 *
 *   I understand everything in this picture except what the space
 * between fp - 0xe0 and fp - 0x140 is used for.  Oh, and I don't
 * understand why there's a large chunk of CALL_DUMMY that never gets
 * executed (its function is superceeded by PUSH_DUMMY_FRAME; they
 * are designed to do the same thing).
 *
 *   PUSH_DUMMY_FRAME saves the registers above sp' and pushes the
 * register file stack down one.
 *
 *   call_function then writes CALL_DUMMY, pushes the args onto the
 * stack, and adjusts the stack pointer.
 *
 *   run_stack_dummy then starts execution (in the middle of
 * CALL_DUMMY, as directed by call_function).
 */

/* Push an empty stack frame, to record the current PC, etc.  */

#define PUSH_DUMMY_FRAME	sparc_push_dummy_frame ()
#define POP_FRAME	sparc_pop_frame ()

void sparc_push_dummy_frame PARAMS ((void)), sparc_pop_frame PARAMS ((void));
/* This sequence of words is the instructions

   save %sp,-0x140,%sp
   std	%f30,[%fp-0x08]
   std	%f28,[%fp-0x10]
   std	%f26,[%fp-0x18]
   std	%f24,[%fp-0x20]
   std	%f22,[%fp-0x28]
   std	%f20,[%fp-0x30]
   std	%f18,[%fp-0x38]
   std	%f16,[%fp-0x40]
   std	%f14,[%fp-0x48]
   std	%f12,[%fp-0x50]
   std	%f10,[%fp-0x58]
   std	%f8,[%fp-0x60]
   std	%f6,[%fp-0x68]
   std	%f4,[%fp-0x70]
   std	%f2,[%fp-0x78]
   std	%f0,[%fp-0x80]
   std	%g6,[%fp-0x88]
   std	%g4,[%fp-0x90]
   std	%g2,[%fp-0x98]
   std	%g0,[%fp-0xa0]
   std	%i6,[%fp-0xa8]
   std	%i4,[%fp-0xb0]
   std	%i2,[%fp-0xb8]
   std	%i0,[%fp-0xc0]
   nop ! stcsr	[%fp-0xc4]
   nop ! stfsr	[%fp-0xc8]
   nop ! wr	%npc,[%fp-0xcc]
   nop ! wr	%pc,[%fp-0xd0]
   rd	%tbr,%o0
   st	%o0,[%fp-0xd4]
   rd	%wim,%o1
   st	%o0,[%fp-0xd8]
   rd	%psr,%o0
   st	%o0,[%fp-0xdc]
   rd	%y,%o0
   st	%o0,[%fp-0xe0]

     /..* The arguments are pushed at this point by GDB;
	no code is needed in the dummy for this.
	The CALL_DUMMY_START_OFFSET gives the position of
	the following ld instruction.  *../

   ld	[%sp+0x58],%o5
   ld	[%sp+0x54],%o4
   ld	[%sp+0x50],%o3
   ld	[%sp+0x4c],%o2
   ld	[%sp+0x48],%o1
   call 0x00000000
   ld	[%sp+0x44],%o0
   nop
   ta 1
   nop

   note that this is 192 bytes, which is a multiple of 8 (not only 4) bytes.
   note that the `call' insn is a relative, not an absolute call.
   note that the `nop' at the end is needed to keep the trap from
        clobbering things (if NPC pointed to garbage instead).

We actually start executing at the `sethi', since the pushing of the
registers (as arguments) is done by PUSH_DUMMY_FRAME.  If this were
real code, the arguments for the function called by the CALL would be
pushed between the list of ST insns and the CALL, and we could allow
it to execute through.  But the arguments have to be pushed by GDB
after the PUSH_DUMMY_FRAME is done, and we cannot allow these ST
insns to be performed again, lest the registers saved be taken for
arguments.  */

#define CALL_DUMMY { 0x9de3bee0, 0xfd3fbff8, 0xf93fbff0, 0xf53fbfe8,	\
		     0xf13fbfe0, 0xed3fbfd8, 0xe93fbfd0, 0xe53fbfc8,	\
		     0xe13fbfc0, 0xdd3fbfb8, 0xd93fbfb0, 0xd53fbfa8,	\
		     0xd13fbfa0, 0xcd3fbf98, 0xc93fbf90, 0xc53fbf88,	\
		     0xc13fbf80, 0xcc3fbf78, 0xc83fbf70, 0xc43fbf68,	\
		     0xc03fbf60, 0xfc3fbf58, 0xf83fbf50, 0xf43fbf48,	\
		     0xf03fbf40, 0x01000000, 0x01000000, 0x01000000,	\
		     0x01000000, 0x91580000, 0xd027bf50, 0x93500000,	\
		     0xd027bf4c, 0x91480000, 0xd027bf48, 0x91400000,	\
		     0xd027bf44, 0xda03a058, 0xd803a054, 0xd603a050,	\
		     0xd403a04c, 0xd203a048, 0x40000000, 0xd003a044,	\
		     0x01000000, 0x91d02001, 0x01000000, 0x01000000}

#define CALL_DUMMY_LENGTH 192

#define CALL_DUMMY_START_OFFSET 148

#define CALL_DUMMY_BREAKPOINT_OFFSET (CALL_DUMMY_START_OFFSET + (8 * 4))

#define CALL_DUMMY_STACK_ADJUST 68

/* Insert the specified number of args and function address
   into a call sequence of the above form stored at DUMMYNAME.

   For structs and unions, if the function was compiled with Sun cc,
   it expects 'unimp' after the call.  But gcc doesn't use that
   (twisted) convention.  So leave a nop there for gcc (FIX_CALL_DUMMY
   can assume it is operating on a pristine CALL_DUMMY, not one that
   has already been customized for a different function).  */

#define FIX_CALL_DUMMY(dummyname, pc, fun, nargs, args, type, gcc_p)	\
{									\
  store_unsigned_integer (dummyname + 168, 4,				\
			  0x40000000 | ((fun - (pc + 168)) >> 2));	\
  if (!gcc_p								\
      && (TYPE_CODE (type) == TYPE_CODE_STRUCT				\
	  || TYPE_CODE (type) == TYPE_CODE_UNION))			\
    store_unsigned_integer (dummyname + 176, 4, TYPE_LENGTH (type) & 0x1fff); \
}

/* The Sparc returns long doubles on the stack.  */

#define RETURN_VALUE_ON_STACK(TYPE) \
  (TYPE_CODE(TYPE) == TYPE_CODE_FLT \
   && TYPE_LENGTH(TYPE) > 8)

/* Sparc has no reliable single step ptrace call */

#define NO_SINGLE_STEP 1

/* We need more arguments in a frame specification for the
   "frame" or "info frame" command.  */

#define SETUP_ARBITRARY_FRAME(argc, argv) setup_arbitrary_frame (argc, argv)
extern struct frame_info *setup_arbitrary_frame PARAMS ((int, CORE_ADDR *));

/* To print every pair of float registers as a double, we use this hook.  */

#define	PRINT_REGISTER_HOOK(regno)	\
  if (((regno) >= FP0_REGNUM)		\
   && ((regno) <  FP0_REGNUM + 32)	\
   && (0 == ((regno) & 1))) {		\
    char doublereg[8];		/* two float regs */	\
    if (!read_relative_register_raw_bytes ((regno)  , doublereg  )	\
     && !read_relative_register_raw_bytes ((regno)+1, doublereg+4)) {	\
      printf("\t");			\
      print_floating (doublereg, builtin_type_double, stdout);	\
    }					\
  }

/* Optimization for storing registers to the inferior.  The hook
   DO_DEFERRED_STORES
   actually executes any deferred stores.  It is called any time
   we are going to proceed the child, or read its registers.
   The hook CLEAR_DEFERRED_STORES is called when we want to throw
   away the inferior process, e.g. when it dies or we kill it.
   FIXME, this does not handle remote debugging cleanly.  */

extern int deferred_stores;
#define	DO_DEFERRED_STORES	\
  if (deferred_stores)		\
    target_store_registers (-2);
#define	CLEAR_DEFERRED_STORES	\
  deferred_stores = 0;

/* If the current gcc for for this target does not produce correct debugging
   information for float parameters, both prototyped and unprototyped, then
   define this macro.  This forces gdb to  always assume that floats are
   passed as doubles and then converted in the callee. */

#define COERCE_FLOAT_TO_DOUBLE 1

/* Select the sparc disassembler */

#define TM_PRINT_INSN_MACH bfd_mach_sparc

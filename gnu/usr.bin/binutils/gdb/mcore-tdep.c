/* Target-machine dependent code for Motorola MCore for GDB, the GNU debugger

   Copyright 1999, 2000, 2001, 2002, 2003, 2004 Free Software
   Foundation, Inc.

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

#include "defs.h"
#include "frame.h"
#include "symtab.h"
#include "value.h"
#include "gdbcmd.h"
#include "regcache.h"
#include "objfiles.h"
#include "gdbcore.h"
#include "inferior.h"
#include "arch-utils.h"
#include "gdb_string.h"
#include "disasm.h"
#include "dis-asm.h"

static CORE_ADDR mcore_analyze_prologue (struct frame_info *fi, CORE_ADDR pc,
					 int skip_prologue);
static int get_insn (CORE_ADDR pc);

#ifdef MCORE_DEBUG
int mcore_debug = 0;
#endif


/* All registers are 4 bytes long.  */
#define MCORE_REG_SIZE 4
#define MCORE_NUM_REGS 65

/* Some useful register numbers.  */
#define PR_REGNUM 15
#define FIRST_ARGREG 2
#define LAST_ARGREG 7
#define RETVAL_REGNUM 2

  
/* Additional info that we use for managing frames */
struct frame_extra_info
  {
    /* A generic status word */
    int status;

    /* Size of this frame */
    int framesize;

    /* The register that is acting as a frame pointer, if
       it is being used.  This is undefined if status
       does not contain the flag MY_FRAME_IN_FP. */
    int fp_regnum;
  };

/* frame_extra_info status flags */

/* The base of the current frame is actually in the stack pointer.
   This happens when there is no frame pointer (MCore ABI does not
   require a frame pointer) or when we're stopped in the prologue or
   epilogue itself.  In these cases, mcore_analyze_prologue will need
   to update fi->frame before returning or analyzing the register
   save instructions. */
#define MY_FRAME_IN_SP 0x1

/* The base of the current frame is in a frame pointer register.
   This register is noted in frame_extra_info->fp_regnum.

   Note that the existence of an FP might also indicate that the
   function has called alloca. */
#define MY_FRAME_IN_FP 0x2

/* This flag is set to indicate that this frame is the top-most
   frame. This tells frame chain not to bother trying to unwind
   beyond this frame. */
#define NO_MORE_FRAMES 0x4

/* Instruction macros used for analyzing the prologue */
#define IS_SUBI0(x)   (((x) & 0xfe0f) == 0x2400)	/* subi r0,oimm5    */
#define IS_STM(x)     (((x) & 0xfff0) == 0x0070)	/* stm rf-r15,r0    */
#define IS_STWx0(x)   (((x) & 0xf00f) == 0x9000)	/* stw rz,(r0,disp) */
#define IS_STWxy(x)   (((x) & 0xf000) == 0x9000)	/* stw rx,(ry,disp) */
#define IS_MOVx0(x)   (((x) & 0xfff0) == 0x1200)	/* mov rn,r0        */
#define IS_LRW1(x)    (((x) & 0xff00) == 0x7100)	/* lrw r1,literal   */
#define IS_MOVI1(x)   (((x) & 0xf80f) == 0x6001)	/* movi r1,imm7     */
#define IS_BGENI1(x)  (((x) & 0xfe0f) == 0x3201)	/* bgeni r1,imm5    */
#define IS_BMASKI1(x) (((x) & 0xfe0f) == 0x2C01)	/* bmaski r1,imm5   */
#define IS_ADDI1(x)   (((x) & 0xfe0f) == 0x2001)	/* addi r1,oimm5    */
#define IS_SUBI1(x)   (((x) & 0xfe0f) == 0x2401)	/* subi r1,oimm5    */
#define IS_RSUBI1(x)  (((x) & 0xfe0f) == 0x2801)	/* rsubi r1,imm5    */
#define IS_NOT1(x)    (((x) & 0xffff) == 0x01f1)	/* not r1           */
#define IS_ROTLI1(x)  (((x) & 0xfe0f) == 0x3801)	/* rotli r1,imm5    */
#define IS_BSETI1(x)  (((x) & 0xfe0f) == 0x3401)	/* bseti r1,imm5    */
#define IS_BCLRI1(x)  (((x) & 0xfe0f) == 0x3001)	/* bclri r1,imm5    */
#define IS_IXH1(x)    (((x) & 0xffff) == 0x1d11)	/* ixh r1,r1        */
#define IS_IXW1(x)    (((x) & 0xffff) == 0x1511)	/* ixw r1,r1        */
#define IS_SUB01(x)   (((x) & 0xffff) == 0x0510)	/* subu r0,r1       */
#define IS_RTS(x)     (((x) & 0xffff) == 0x00cf)	/* jmp r15          */

#define IS_R1_ADJUSTER(x) \
    (IS_ADDI1(x) || IS_SUBI1(x) || IS_ROTLI1(x) || IS_BSETI1(x) \
     || IS_BCLRI1(x) || IS_RSUBI1(x) || IS_NOT1(x) \
     || IS_IXH1(x) || IS_IXW1(x))


#ifdef MCORE_DEBUG
static void
mcore_dump_insn (char *commnt, CORE_ADDR pc, int insn)
{
  if (mcore_debug)
    {
      printf_filtered ("MCORE:  %s %08x %08x ",
		       commnt, (unsigned int) pc, (unsigned int) insn);
      gdb_print_insn (pc, gdb_stdout);
      printf_filtered ("\n");
    }
}
#define mcore_insn_debug(args) { if (mcore_debug) printf_filtered args; }
#else /* !MCORE_DEBUG */
#define mcore_dump_insn(a,b,c) {}
#define mcore_insn_debug(args) {}
#endif


static struct type *
mcore_register_virtual_type (int regnum)
{
  if (regnum < 0 || regnum >= MCORE_NUM_REGS)
    internal_error (__FILE__, __LINE__,
		    "mcore_register_virtual_type: illegal register number %d",
		    regnum);
  else
    return builtin_type_int;
}

static int
mcore_register_byte (int regnum)
{
  if (regnum < 0 || regnum >= MCORE_NUM_REGS)
    internal_error (__FILE__, __LINE__,
		    "mcore_register_byte: illegal register number %d",
		    regnum);
  else 
    return (regnum * MCORE_REG_SIZE);
}

static int
mcore_register_size (int regnum)
{
  
  if (regnum < 0 || regnum >= MCORE_NUM_REGS)
    internal_error (__FILE__, __LINE__,
		    "mcore_register_size: illegal register number %d",
		    regnum);
  else
    return MCORE_REG_SIZE;
}

/* The registers of the Motorola MCore processors */

static const char *
mcore_register_name (int regnum)
{

  static char *register_names[] = { 
    "r0",   "r1",  "r2",    "r3",   "r4",   "r5",   "r6",   "r7",
    "r8",   "r9",  "r10",   "r11",  "r12",  "r13",  "r14",  "r15",
    "ar0",  "ar1", "ar2",   "ar3",  "ar4",  "ar5",  "ar6",  "ar7",
    "ar8",  "ar9", "ar10", "ar11",  "ar12", "ar13", "ar14", "ar15",
    "psr",  "vbr", "epsr",  "fpsr", "epc",  "fpc",  "ss0",  "ss1",
    "ss2",  "ss3", "ss4",   "gcr",  "gsr",  "cr13", "cr14", "cr15",
    "cr16", "cr17", "cr18", "cr19", "cr20", "cr21", "cr22", "cr23",
    "cr24", "cr25", "cr26", "cr27", "cr28", "cr29", "cr30", "cr31",
    "pc" 
  };

  if (regnum < 0 ||
      regnum >= sizeof (register_names) / sizeof (register_names[0]))
    internal_error (__FILE__, __LINE__,
		    "mcore_register_name: illegal register number %d",
		    regnum);
  else
    return register_names[regnum];
}

/* Given the address at which to insert a breakpoint (BP_ADDR),
   what will that breakpoint be?

   For MCore, we have a breakpoint instruction. Since all MCore
   instructions are 16 bits, this is all we need, regardless of
   address. bpkt = 0x0000 */

static const unsigned char *
mcore_breakpoint_from_pc (CORE_ADDR * bp_addr, int *bp_size)
{
  static char breakpoint[] =
  {0x00, 0x00};
  *bp_size = 2;
  return breakpoint;
}

static CORE_ADDR
mcore_saved_pc_after_call (struct frame_info *frame)
{
  return read_register (PR_REGNUM);
}

/* This is currently handled by init_extra_frame_info.  */
static void
mcore_frame_init_saved_regs (struct frame_info *frame)
{

}

/* This is currently handled by mcore_push_arguments  */
static void
mcore_store_struct_return (CORE_ADDR addr, CORE_ADDR sp)
{

}

static int
mcore_reg_struct_has_addr (int gcc_p, struct type *type)
{
  return 0;
}


/* Helper function for several routines below.  This funtion simply
   sets up a fake, aka dummy, frame (not a _call_ dummy frame) that
   we can analyze with mcore_analyze_prologue. */

static struct frame_info *
analyze_dummy_frame (CORE_ADDR pc, CORE_ADDR frame)
{
  struct cleanup *old_chain = make_cleanup (null_cleanup, NULL);
  struct frame_info *dummy
    = deprecated_frame_xmalloc_with_cleanup (SIZEOF_FRAME_SAVED_REGS,
					     sizeof (struct frame_extra_info));
  deprecated_update_frame_pc_hack (dummy, pc);
  deprecated_update_frame_base_hack (dummy, frame);
  get_frame_extra_info (dummy)->status = 0;
  get_frame_extra_info (dummy)->framesize = 0;
  mcore_analyze_prologue (dummy, 0, 0);
  do_cleanups (old_chain);
  return dummy;
}

/* Function prologues on the Motorola MCore processors consist of:

   - adjustments to the stack pointer (r1 used as scratch register)
   - store word/multiples that use r0 as the base address
   - making a copy of r0 into another register (a "frame" pointer)

   Note that the MCore really doesn't have a real frame pointer.
   Instead, the compiler may copy the SP into a register (usually
   r8) to act as an arg pointer.  For our target-dependent purposes,
   the frame info's "frame" member will be the beginning of the
   frame. The SP could, in fact, point below this.

   The prologue ends when an instruction fails to meet either of
   the first two criteria or when an FP is made.  We make a special
   exception for gcc. When compiling unoptimized code, gcc will
   setup stack slots. We need to make sure that we skip the filling
   of these stack slots as much as possible. This is only done
   when SKIP_PROLOGUE is set, so that it does not mess up
   backtraces. */

/* Analyze the prologue of frame FI to determine where registers are saved,
   the end of the prologue, etc. Return the address of the first line
   of "real" code (i.e., the end of the prologue). */

static CORE_ADDR
mcore_analyze_prologue (struct frame_info *fi, CORE_ADDR pc, int skip_prologue)
{
  CORE_ADDR func_addr, func_end, addr, stop;
  CORE_ADDR stack_size;
  int insn, rn;
  int status;
  int fp_regnum = 0; /* dummy, valid when (flags & MY_FRAME_IN_FP) */
  int flags;
  int framesize;
  int register_offsets[NUM_REGS];
  char *name;

  /* If provided, use the PC in the frame to look up the
     start of this function. */
  pc = (fi == NULL ? pc : get_frame_pc (fi));

  /* Find the start of this function. */
  status = find_pc_partial_function (pc, &name, &func_addr, &func_end);

  /* If the start of this function could not be found or if the debbuger
     is stopped at the first instruction of the prologue, do nothing. */
  if (status == 0)
    return pc;

  /* If the debugger is entry function, give up. */
  if (func_addr == entry_point_address ())
    {
      if (fi != NULL)
	get_frame_extra_info (fi)->status |= NO_MORE_FRAMES;
      return pc;
    }

  /* At the start of a function, our frame is in the stack pointer. */
  flags = MY_FRAME_IN_SP;

  /* Start decoding the prologue.  We start by checking two special cases:

     1. We're about to return
     2. We're at the first insn of the prologue.

     If we're about to return, our frame has already been deallocated.
     If we are stopped at the first instruction of a prologue,
     then our frame has not yet been set up. */

  /* Get the first insn from memory (all MCore instructions are 16 bits) */
  mcore_insn_debug (("MCORE: starting prologue decoding\n"));
  insn = get_insn (pc);
  mcore_dump_insn ("got 1: ", pc, insn);

  /* Check for return. */
  if (fi != NULL && IS_RTS (insn))
    {
      mcore_insn_debug (("MCORE: got jmp r15"));
      if (get_next_frame (fi) == NULL)
	deprecated_update_frame_base_hack (fi, read_sp ());
      return get_frame_pc (fi);
    }

  /* Check for first insn of prologue */
  if (fi != NULL && get_frame_pc (fi) == func_addr)
    {
      if (get_next_frame (fi) == NULL)
	deprecated_update_frame_base_hack (fi, read_sp ());
      return get_frame_pc (fi);
    }

  /* Figure out where to stop scanning */
  stop = (fi ? get_frame_pc (fi) : func_end);

  /* Don't walk off the end of the function */
  stop = (stop > func_end ? func_end : stop);

  /* REGISTER_OFFSETS will contain offsets, from the top of the frame
     (NOT the frame pointer), for the various saved registers or -1
     if the register is not saved. */
  for (rn = 0; rn < NUM_REGS; rn++)
    register_offsets[rn] = -1;

  /* Analyze the prologue. Things we determine from analyzing the
     prologue include:
     * the size of the frame
     * where saved registers are located (and which are saved)
     * FP used? */
  mcore_insn_debug (("MCORE: Scanning prologue: func_addr=0x%x, stop=0x%x\n",
		     (unsigned int) func_addr, (unsigned int) stop));

  framesize = 0;
  for (addr = func_addr; addr < stop; addr += 2)
    {
      /* Get next insn */
      insn = get_insn (addr);
      mcore_dump_insn ("got 2: ", addr, insn);

      if (IS_SUBI0 (insn))
	{
	  int offset = 1 + ((insn >> 4) & 0x1f);
	  mcore_insn_debug (("MCORE: got subi r0,%d; continuing\n", offset));
	  framesize += offset;
	  continue;
	}
      else if (IS_STM (insn))
	{
	  /* Spill register(s) */
	  int offset;
	  int start_register;

	  /* BIG WARNING! The MCore ABI does not restrict functions
	     to taking only one stack allocation. Therefore, when
	     we save a register, we record the offset of where it was
	     saved relative to the current framesize. This will
	     then give an offset from the SP upon entry to our
	     function. Remember, framesize is NOT constant until
	     we're done scanning the prologue. */
	  start_register = (insn & 0xf);
	  mcore_insn_debug (("MCORE: got stm r%d-r15,(r0)\n", start_register));

	  for (rn = start_register, offset = 0; rn <= 15; rn++, offset += 4)
	    {
	      register_offsets[rn] = framesize - offset;
	      mcore_insn_debug (("MCORE: r%d saved at 0x%x (offset %d)\n", rn,
				 register_offsets[rn], offset));
	    }
	  mcore_insn_debug (("MCORE: continuing\n"));
	  continue;
	}
      else if (IS_STWx0 (insn))
	{
	  /* Spill register: see note for IS_STM above. */
	  int imm;

	  rn = (insn >> 8) & 0xf;
	  imm = (insn >> 4) & 0xf;
	  register_offsets[rn] = framesize - (imm << 2);
	  mcore_insn_debug (("MCORE: r%d saved at offset 0x%x\n", rn, register_offsets[rn]));
	  mcore_insn_debug (("MCORE: continuing\n"));
	  continue;
	}
      else if (IS_MOVx0 (insn))
	{
	  /* We have a frame pointer, so this prologue is over.  Note
	     the register which is acting as the frame pointer. */
	  flags |= MY_FRAME_IN_FP;
	  flags &= ~MY_FRAME_IN_SP;
	  fp_regnum = insn & 0xf;
	  mcore_insn_debug (("MCORE: Found a frame pointer: r%d\n", fp_regnum));

	  /* If we found an FP, we're at the end of the prologue. */
	  mcore_insn_debug (("MCORE: end of prologue\n"));
	  if (skip_prologue)
	    continue;

	  /* If we're decoding prologue, stop here. */
	  addr += 2;
	  break;
	}
      else if (IS_STWxy (insn) && (flags & MY_FRAME_IN_FP) && ((insn & 0xf) == fp_regnum))
	{
	  /* Special case. Skip over stack slot allocs, too. */
	  mcore_insn_debug (("MCORE: push arg onto stack.\n"));
	  continue;
	}
      else if (IS_LRW1 (insn) || IS_MOVI1 (insn)
	       || IS_BGENI1 (insn) || IS_BMASKI1 (insn))
	{
	  int adjust = 0;
	  int offset = 0;
	  int insn2;

	  mcore_insn_debug (("MCORE: looking at large frame\n"));
	  if (IS_LRW1 (insn))
	    {
	      adjust =
		read_memory_integer ((addr + 2 + ((insn & 0xff) << 2)) & 0xfffffffc, 4);
	    }
	  else if (IS_MOVI1 (insn))
	    adjust = (insn >> 4) & 0x7f;
	  else if (IS_BGENI1 (insn))
	    adjust = 1 << ((insn >> 4) & 0x1f);
	  else			/* IS_BMASKI (insn) */
	    adjust = (1 << (adjust >> 4) & 0x1f) - 1;

	  mcore_insn_debug (("MCORE: base framesize=0x%x\n", adjust));

	  /* May have zero or more insns which modify r1 */
	  mcore_insn_debug (("MCORE: looking for r1 adjusters...\n"));
	  offset = 2;
	  insn2 = get_insn (addr + offset);
	  while (IS_R1_ADJUSTER (insn2))
	    {
	      int imm;

	      imm = (insn2 >> 4) & 0x1f;
	      mcore_dump_insn ("got 3: ", addr + offset, insn);
	      if (IS_ADDI1 (insn2))
		{
		  adjust += (imm + 1);
		  mcore_insn_debug (("MCORE: addi r1,%d\n", imm + 1));
		}
	      else if (IS_SUBI1 (insn2))
		{
		  adjust -= (imm + 1);
		  mcore_insn_debug (("MCORE: subi r1,%d\n", imm + 1));
		}
	      else if (IS_RSUBI1 (insn2))
		{
		  adjust = imm - adjust;
		  mcore_insn_debug (("MCORE: rsubi r1,%d\n", imm + 1));
		}
	      else if (IS_NOT1 (insn2))
		{
		  adjust = ~adjust;
		  mcore_insn_debug (("MCORE: not r1\n"));
		}
	      else if (IS_ROTLI1 (insn2))
		{
		  adjust <<= imm;
		  mcore_insn_debug (("MCORE: rotli r1,%d\n", imm + 1));
		}
	      else if (IS_BSETI1 (insn2))
		{
		  adjust |= (1 << imm);
		  mcore_insn_debug (("MCORE: bseti r1,%d\n", imm));
		}
	      else if (IS_BCLRI1 (insn2))
		{
		  adjust &= ~(1 << imm);
		  mcore_insn_debug (("MCORE: bclri r1,%d\n", imm));
		}
	      else if (IS_IXH1 (insn2))
		{
		  adjust *= 3;
		  mcore_insn_debug (("MCORE: ix.h r1,r1\n"));
		}
	      else if (IS_IXW1 (insn2))
		{
		  adjust *= 5;
		  mcore_insn_debug (("MCORE: ix.w r1,r1\n"));
		}

	      offset += 2;
	      insn2 = get_insn (addr + offset);
	    };

	  mcore_insn_debug (("MCORE: done looking for r1 adjusters\n"));

	  /* If the next insn adjusts the stack pointer, we keep everything;
	     if not, we scrap it and we've found the end of the prologue. */
	  if (IS_SUB01 (insn2))
	    {
	      addr += offset;
	      framesize += adjust;
	      mcore_insn_debug (("MCORE: found stack adjustment of 0x%x bytes.\n", adjust));
	      mcore_insn_debug (("MCORE: skipping to new address 0x%x\n", addr));
	      mcore_insn_debug (("MCORE: continuing\n"));
	      continue;
	    }

	  /* None of these instructions are prologue, so don't touch
	     anything. */
	  mcore_insn_debug (("MCORE: no subu r1,r0, NOT altering framesize.\n"));
	  break;
	}

      /* This is not a prologue insn, so stop here. */
      mcore_insn_debug (("MCORE: insn is not a prologue insn -- ending scan\n"));
      break;
    }

  mcore_insn_debug (("MCORE: done analyzing prologue\n"));
  mcore_insn_debug (("MCORE: prologue end = 0x%x\n", addr));

  /* Save everything we have learned about this frame into FI. */
  if (fi != NULL)
    {
      get_frame_extra_info (fi)->framesize = framesize;
      get_frame_extra_info (fi)->fp_regnum = fp_regnum;
      get_frame_extra_info (fi)->status = flags;

      /* Fix the frame pointer. When gcc uses r8 as a frame pointer,
         it is really an arg ptr. We adjust fi->frame to be a "real"
         frame pointer. */
      if (get_next_frame (fi) == NULL)
	{
	  if (get_frame_extra_info (fi)->status & MY_FRAME_IN_SP)
	    deprecated_update_frame_base_hack (fi, read_sp () + framesize);
	  else
	    deprecated_update_frame_base_hack (fi, read_register (fp_regnum) + framesize);
	}

      /* Note where saved registers are stored. The offsets in REGISTER_OFFSETS
         are computed relative to the top of the frame. */
      for (rn = 0; rn < NUM_REGS; rn++)
	{
	  if (register_offsets[rn] >= 0)
	    {
	      deprecated_get_frame_saved_regs (fi)[rn] = get_frame_base (fi) - register_offsets[rn];
	      mcore_insn_debug (("Saved register %s stored at 0x%08x, value=0x%08x\n",
			       mcore_register_names[rn], fi->saved_regs[rn],
			      read_memory_integer (fi->saved_regs[rn], 4)));
	    }
	}
    }

  /* Return addr of first non-prologue insn. */
  return addr;
}

/* Given a GDB frame, determine the address of the calling function's
   frame.  This will be used to create a new GDB frame struct, and
   then DEPRECATED_INIT_EXTRA_FRAME_INFO and DEPRECATED_INIT_FRAME_PC
   will be called for the new frame. */

static CORE_ADDR
mcore_frame_chain (struct frame_info * fi)
{
  struct frame_info *dummy;
  CORE_ADDR callers_addr;

  /* Analyze the prologue of this function. */
  if (get_frame_extra_info (fi)->status == 0)
    mcore_analyze_prologue (fi, 0, 0);

  /* If mcore_analyze_prologue set NO_MORE_FRAMES, quit now. */
  if (get_frame_extra_info (fi)->status & NO_MORE_FRAMES)
    return 0;

  /* Now that we've analyzed our prologue, we can start to ask
     for information about our caller. The easiest way to do
     this is to analyze our caller's prologue. 

     If our caller has a frame pointer, then we need to find
     the value of that register upon entry to our frame.
     This value is either in fi->saved_regs[rn] if it's saved,
     or it's still in a register.

     If our caller does not have a frame pointer, then his frame base
     is <our base> + -<caller's frame size>. */
  dummy = analyze_dummy_frame (DEPRECATED_FRAME_SAVED_PC (fi), get_frame_base (fi));

  if (get_frame_extra_info (dummy)->status & MY_FRAME_IN_FP)
    {
      int fp = get_frame_extra_info (dummy)->fp_regnum;

      /* Our caller has a frame pointer. */
      if (deprecated_get_frame_saved_regs (fi)[fp] != 0)
	{
	  /* The "FP" was saved on the stack.  Don't forget to adjust
	     the "FP" with the framesize to get a real FP. */
	  callers_addr = read_memory_integer (deprecated_get_frame_saved_regs (fi)[fp],
					      DEPRECATED_REGISTER_SIZE)
	    + get_frame_extra_info (dummy)->framesize;
	}
      else
	{
	  /* It's still in the register.  Don't forget to adjust
	     the "FP" with the framesize to get a real FP. */
	  callers_addr = read_register (fp) + get_frame_extra_info (dummy)->framesize;
	}
    }
  else
    {
      /* Our caller does not have a frame pointer. */
      callers_addr = get_frame_base (fi) + get_frame_extra_info (dummy)->framesize;
    }

  return callers_addr;
}

/* Skip the prologue of the function at PC. */

static CORE_ADDR
mcore_skip_prologue (CORE_ADDR pc)
{
  CORE_ADDR func_addr, func_end;
  struct symtab_and_line sal;

  /* If we have line debugging information, then the end of the
     prologue should be the first assembly instruction of the first
     source line */
  if (find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      sal = find_pc_line (func_addr, 0);
      if (sal.end && sal.end < func_end)
	return sal.end;
    }

  return mcore_analyze_prologue (NULL, pc, 1);
}

/* Return the address at which function arguments are offset. */
static CORE_ADDR
mcore_frame_args_address (struct frame_info * fi)
{
  return get_frame_base (fi) - get_frame_extra_info (fi)->framesize;
}

static CORE_ADDR
mcore_frame_locals_address (struct frame_info * fi)
{
  return get_frame_base (fi) - get_frame_extra_info (fi)->framesize;
}

/* Return the frame pointer in use at address PC. */

static void
mcore_virtual_frame_pointer (CORE_ADDR pc, int *reg, LONGEST *offset)
{
  struct frame_info *dummy = analyze_dummy_frame (pc, 0);
  if (get_frame_extra_info (dummy)->status & MY_FRAME_IN_SP)
    {
      *reg = SP_REGNUM;
      *offset = 0;
    }
  else
    {
      *reg = get_frame_extra_info (dummy)->fp_regnum;
      *offset = 0;
    }
}

/* Find the value of register REGNUM in frame FI. */

static CORE_ADDR
mcore_find_callers_reg (struct frame_info *fi, int regnum)
{
  for (; fi != NULL; fi = get_next_frame (fi))
    {
      if (DEPRECATED_PC_IN_CALL_DUMMY (get_frame_pc (fi), get_frame_base (fi),
				       get_frame_base (fi)))
	return deprecated_read_register_dummy (get_frame_pc (fi),
					       get_frame_base (fi), regnum);
      else if (deprecated_get_frame_saved_regs (fi)[regnum] != 0)
	return read_memory_integer (deprecated_get_frame_saved_regs (fi)[regnum],
				    DEPRECATED_REGISTER_SIZE);
    }

  return read_register (regnum);
}

/* Find the saved pc in frame FI. */

static CORE_ADDR
mcore_frame_saved_pc (struct frame_info * fi)
{

  if (DEPRECATED_PC_IN_CALL_DUMMY (get_frame_pc (fi), get_frame_base (fi),
				   get_frame_base (fi)))
    return deprecated_read_register_dummy (get_frame_pc (fi),
					   get_frame_base (fi), PC_REGNUM);
  else
    return mcore_find_callers_reg (fi, PR_REGNUM);
}

/* INFERIOR FUNCTION CALLS */

/* This routine gets called when either the user uses the "return"
   command, or the call dummy breakpoint gets hit. */

static void
mcore_pop_frame (void)
{
  int rn;
  struct frame_info *fi = get_current_frame ();

  if (DEPRECATED_PC_IN_CALL_DUMMY (get_frame_pc (fi), get_frame_base (fi),
				   get_frame_base (fi)))
    generic_pop_dummy_frame ();
  else
    {
      /* Write out the PC we saved. */
      write_register (PC_REGNUM, DEPRECATED_FRAME_SAVED_PC (fi));

      /* Restore any saved registers. */
      for (rn = 0; rn < NUM_REGS; rn++)
	{
	  if (deprecated_get_frame_saved_regs (fi)[rn] != 0)
	    {
	      ULONGEST value;

	      value = read_memory_unsigned_integer (deprecated_get_frame_saved_regs (fi)[rn],
						    DEPRECATED_REGISTER_SIZE);
	      write_register (rn, value);
	    }
	}

      /* Actually cut back the stack. */
      write_register (SP_REGNUM, get_frame_base (fi));
    }

  /* Finally, throw away any cached frame information. */
  flush_cached_frames ();
}

/* Setup arguments and PR for a call to the target. First six arguments
   go in FIRST_ARGREG -> LAST_ARGREG, subsequent args go on to the stack.

   - Types with lengths greater than DEPRECATED_REGISTER_SIZE may not
   be split between registers and the stack, and they must start in an
   even-numbered register. Subsequent args will go onto the stack.

   * Structs may be split between registers and stack, left-aligned.

   * If the function returns a struct which will not fit into registers (it's
   more than eight bytes), we must allocate for that, too. Gdb will tell
   us where this buffer is (STRUCT_ADDR), and we simply place it into
   FIRST_ARGREG, since the MCORE treats struct returns (of less than eight
   bytes) as hidden first arguments. */

static CORE_ADDR
mcore_push_arguments (int nargs, struct value **args, CORE_ADDR sp,
		      int struct_return, CORE_ADDR struct_addr)
{
  int argreg;
  int argnum;
  struct stack_arg
    {
      int len;
      char *val;
    }
   *stack_args;
  int nstack_args = 0;

  stack_args = (struct stack_arg *) alloca (nargs * sizeof (struct stack_arg));

  argreg = FIRST_ARGREG;

  /* Align the stack. This is mostly a nop, but not always. It will be needed
     if we call a function which has argument overflow. */
  sp &= ~3;

  /* If this function returns a struct which does not fit in the
     return registers, we must pass a buffer to the function
     which it can use to save the return value. */
  if (struct_return)
    write_register (argreg++, struct_addr);

  /* FIXME: what about unions? */
  for (argnum = 0; argnum < nargs; argnum++)
    {
      char *val = (char *) VALUE_CONTENTS (args[argnum]);
      int len = TYPE_LENGTH (VALUE_TYPE (args[argnum]));
      struct type *type = VALUE_TYPE (args[argnum]);
      int olen;

      mcore_insn_debug (("MCORE PUSH: argreg=%d; len=%d; %s\n",
			 argreg, len, TYPE_CODE (type) == TYPE_CODE_STRUCT ? "struct" : "not struct"));
      /* Arguments larger than a register must start in an even
         numbered register. */
      olen = len;

      if (TYPE_CODE (type) != TYPE_CODE_STRUCT && len > DEPRECATED_REGISTER_SIZE && argreg % 2)
	{
	  mcore_insn_debug (("MCORE PUSH: %d > DEPRECATED_REGISTER_SIZE: and %s is not even\n",
			     len, mcore_register_names[argreg]));
	  argreg++;
	}

      if ((argreg <= LAST_ARGREG && len <= (LAST_ARGREG - argreg + 1) * DEPRECATED_REGISTER_SIZE)
	  || (TYPE_CODE (type) == TYPE_CODE_STRUCT))
	{
	  /* Something that will fit entirely into registers (or a struct
	     which may be split between registers and stack). */
	  mcore_insn_debug (("MCORE PUSH: arg %d going into regs\n", argnum));

	  if (TYPE_CODE (type) == TYPE_CODE_STRUCT && olen < DEPRECATED_REGISTER_SIZE)
	    {
	      /* Small structs must be right aligned within the register,
	         the most significant bits are undefined. */
	      write_register (argreg, extract_unsigned_integer (val, len));
	      argreg++;
	      len = 0;
	    }

	  while (len > 0 && argreg <= LAST_ARGREG)
	    {
	      write_register (argreg, extract_unsigned_integer (val, DEPRECATED_REGISTER_SIZE));
	      argreg++;
	      val += DEPRECATED_REGISTER_SIZE;
	      len -= DEPRECATED_REGISTER_SIZE;
	    }

	  /* Any remainder for the stack is noted below... */
	}
      else if (TYPE_CODE (VALUE_TYPE (args[argnum])) != TYPE_CODE_STRUCT
	       && len > DEPRECATED_REGISTER_SIZE)
	{
	  /* All subsequent args go onto the stack. */
	  mcore_insn_debug (("MCORE PUSH: does not fit into regs, going onto stack\n"));
	  argnum = LAST_ARGREG + 1;
	}

      if (len > 0)
	{
	  /* Note that this must be saved onto the stack */
	  mcore_insn_debug (("MCORE PUSH: adding arg %d to stack\n", argnum));
	  stack_args[nstack_args].val = val;
	  stack_args[nstack_args].len = len;
	  nstack_args++;
	}

    }

  /* We're done with registers and stack allocation. Now do the actual
     stack pushes. */
  while (nstack_args--)
    {
      sp -= stack_args[nstack_args].len;
      write_memory (sp, stack_args[nstack_args].val, stack_args[nstack_args].len);
    }

  /* Return adjusted stack pointer.  */
  return sp;
}

/* Store the return address for the call dummy. For MCore, we've opted
   to use generic call dummies, so we simply store the entry-point
   address into the PR register (r15). */

static CORE_ADDR
mcore_push_return_address (CORE_ADDR pc, CORE_ADDR sp)
{
  write_register (PR_REGNUM, entry_point_address ());
  return sp;
}

/* Setting/getting return values from functions.

   The Motorola MCore processors use r2/r3 to return anything
   not larger than 32 bits. Everything else goes into a caller-
   supplied buffer, which is passed in via a hidden first
   argument.

   For gdb, this leaves us two routes, based on what
   USE_STRUCT_CONVENTION (mcore_use_struct_convention) returns.  If
   this macro returns 1, gdb will call STORE_STRUCT_RETURN to store
   the return value.

   If USE_STRUCT_CONVENTION returns 0, then gdb uses
   STORE_RETURN_VALUE and EXTRACT_RETURN_VALUE to store/fetch the
   functions return value.  */

static int
mcore_use_struct_convention (int gcc_p, struct type *type)
{
  return (TYPE_LENGTH (type) > 8);
}

/* Given a function which returns a value of type TYPE, extract the
   the function's return value and place the result into VALBUF.
   REGBUF is the register contents of the target. */

static void
mcore_extract_return_value (struct type *type, char *regbuf, char *valbuf)
{
  /* Copy the return value (starting) in RETVAL_REGNUM to VALBUF. */
  /* Only getting the first byte! if len = 1, we need the last byte of
     the register, not the first. */
  memcpy (valbuf, regbuf + DEPRECATED_REGISTER_BYTE (RETVAL_REGNUM) +
  (TYPE_LENGTH (type) < 4 ? 4 - TYPE_LENGTH (type) : 0), TYPE_LENGTH (type));
}

/* Store the return value in VALBUF (of type TYPE) where the caller
   expects to see it.

   Values less than 32 bits are stored in r2, right justified and
   sign or zero extended.

   Values between 32 and 64 bits are stored in r2 (most
   significant word) and r3 (least significant word, left justified).
   Note that this includes structures of less than eight bytes, too. */

static void
mcore_store_return_value (struct type *type, char *valbuf)
{
  int value_size;
  int return_size;
  int offset;
  char *zeros;

  value_size = TYPE_LENGTH (type);

  /* Return value fits into registers. */
  return_size = (value_size + DEPRECATED_REGISTER_SIZE - 1) & ~(DEPRECATED_REGISTER_SIZE - 1);
  offset = DEPRECATED_REGISTER_BYTE (RETVAL_REGNUM) + (return_size - value_size);
  zeros = alloca (return_size);
  memset (zeros, 0, return_size);

  deprecated_write_register_bytes (DEPRECATED_REGISTER_BYTE (RETVAL_REGNUM), zeros,
				   return_size);
  deprecated_write_register_bytes (offset, valbuf, value_size);
}

/* Initialize our target-dependent "stuff" for this newly created frame.

   This includes allocating space for saved registers and analyzing
   the prologue of this frame. */

static void
mcore_init_extra_frame_info (int fromleaf, struct frame_info *fi)
{
  if (fi && get_next_frame (fi))
    deprecated_update_frame_pc_hack (fi, DEPRECATED_FRAME_SAVED_PC (get_next_frame (fi)));

  frame_saved_regs_zalloc (fi);

  frame_extra_info_zalloc (fi, sizeof (struct frame_extra_info));
  get_frame_extra_info (fi)->status = 0;
  get_frame_extra_info (fi)->framesize = 0;

  if (DEPRECATED_PC_IN_CALL_DUMMY (get_frame_pc (fi), get_frame_base (fi),
				   get_frame_base (fi)))
    {
      /* We need to setup fi->frame here because call_function_by_hand
         gets it wrong by assuming it's always FP.  */
      deprecated_update_frame_base_hack (fi, deprecated_read_register_dummy (get_frame_pc (fi), get_frame_base (fi), SP_REGNUM));
    }
  else
    mcore_analyze_prologue (fi, 0, 0);
}

/* Get an insturction from memory. */

static int
get_insn (CORE_ADDR pc)
{
  char buf[4];
  int status = read_memory_nobpt (pc, buf, 2);
  if (status != 0)
    return 0;

  return extract_unsigned_integer (buf, 2);
}

static struct gdbarch *
mcore_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  static LONGEST call_dummy_words[7] = { };
  struct gdbarch_tdep *tdep = NULL;
  struct gdbarch *gdbarch;

  /* find a candidate among the list of pre-declared architectures. */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return (arches->gdbarch);

  gdbarch = gdbarch_alloc (&info, 0);

  /* NOTE: cagney/2002-12-06: This can be deleted when this arch is
     ready to unwind the PC first (see frame.c:get_prev_frame()).  */
  set_gdbarch_deprecated_init_frame_pc (gdbarch, deprecated_init_frame_pc_default);

  /* Registers: */

  /* All registers are 32 bits */
  set_gdbarch_deprecated_register_size (gdbarch, MCORE_REG_SIZE);
  set_gdbarch_deprecated_max_register_raw_size (gdbarch, MCORE_REG_SIZE);
  set_gdbarch_deprecated_max_register_virtual_size (gdbarch, MCORE_REG_SIZE);
  set_gdbarch_register_name (gdbarch, mcore_register_name);
  set_gdbarch_deprecated_register_virtual_type (gdbarch, mcore_register_virtual_type);
  set_gdbarch_deprecated_register_virtual_size (gdbarch, mcore_register_size);
  set_gdbarch_deprecated_register_raw_size (gdbarch, mcore_register_size);
  set_gdbarch_deprecated_register_byte (gdbarch, mcore_register_byte);
  set_gdbarch_deprecated_register_bytes (gdbarch, MCORE_REG_SIZE * MCORE_NUM_REGS);
  set_gdbarch_num_regs (gdbarch, MCORE_NUM_REGS);
  set_gdbarch_pc_regnum (gdbarch, 64);
  set_gdbarch_sp_regnum (gdbarch, 0);
  set_gdbarch_deprecated_fp_regnum (gdbarch, 0);

  /* Call Dummies:  */

  set_gdbarch_deprecated_call_dummy_words (gdbarch, call_dummy_words);
  set_gdbarch_deprecated_sizeof_call_dummy_words (gdbarch, 0);
  set_gdbarch_deprecated_save_dummy_frame_tos (gdbarch, generic_save_dummy_frame_tos);
  set_gdbarch_deprecated_saved_pc_after_call (gdbarch, mcore_saved_pc_after_call);
  set_gdbarch_breakpoint_from_pc (gdbarch, mcore_breakpoint_from_pc);
  set_gdbarch_deprecated_push_return_address (gdbarch, mcore_push_return_address);
  set_gdbarch_deprecated_push_arguments (gdbarch, mcore_push_arguments);

  /* Frames:  */

  set_gdbarch_deprecated_init_extra_frame_info (gdbarch, mcore_init_extra_frame_info);
  set_gdbarch_deprecated_frame_chain (gdbarch, mcore_frame_chain);
  set_gdbarch_deprecated_frame_init_saved_regs (gdbarch, mcore_frame_init_saved_regs);
  set_gdbarch_deprecated_frame_saved_pc (gdbarch, mcore_frame_saved_pc);
  set_gdbarch_deprecated_store_return_value (gdbarch, mcore_store_return_value);
  set_gdbarch_deprecated_extract_return_value (gdbarch, 
					       mcore_extract_return_value);
  set_gdbarch_deprecated_store_struct_return (gdbarch, mcore_store_struct_return);
  set_gdbarch_skip_prologue (gdbarch, mcore_skip_prologue);
  set_gdbarch_deprecated_frame_args_address (gdbarch, mcore_frame_args_address);
  set_gdbarch_deprecated_frame_locals_address (gdbarch, mcore_frame_locals_address);
  set_gdbarch_deprecated_pop_frame (gdbarch, mcore_pop_frame);
  set_gdbarch_virtual_frame_pointer (gdbarch, mcore_virtual_frame_pointer);

  /* Misc.:  */

  /* Stack grows down.  */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_use_struct_convention (gdbarch, mcore_use_struct_convention);
  set_gdbarch_believe_pcc_promotion (gdbarch, 1);
  /* MCore will never pass a sturcture by reference. It will always be split
     between registers and stack.  */
  set_gdbarch_deprecated_reg_struct_has_addr
    (gdbarch, mcore_reg_struct_has_addr);

  /* Should be using push_dummy_call.  */
  set_gdbarch_deprecated_dummy_write_sp (gdbarch, deprecated_write_sp);

  set_gdbarch_print_insn (gdbarch, print_insn_mcore);

  return gdbarch;
}

static void
mcore_dump_tdep (struct gdbarch *current_gdbarch, struct ui_file *file)
{

}

extern initialize_file_ftype _initialize_mcore_tdep; /* -Wmissing-prototypes */

void
_initialize_mcore_tdep (void)
{
  gdbarch_register (bfd_arch_mcore, mcore_gdbarch_init, mcore_dump_tdep);

#ifdef MCORE_DEBUG
  add_show_from_set (add_set_cmd ("mcoredebug", no_class,
				  var_boolean, (char *) &mcore_debug,
				  "Set mcore debugging.\n", &setlist),
		     &showlist);
#endif
}

/* Target-dependent code for the Matsushita MN10300 for GDB, the GNU debugger.

   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004 Free
   Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "target.h"
#include "value.h"
#include "bfd.h"
#include "gdb_string.h"
#include "gdbcore.h"
#include "objfiles.h"
#include "regcache.h"
#include "arch-utils.h"
#include "gdb_assert.h"
#include "dis-asm.h"

#define D0_REGNUM 0
#define D2_REGNUM 2
#define D3_REGNUM 3
#define A0_REGNUM 4
#define A2_REGNUM 6
#define A3_REGNUM 7
#define MDR_REGNUM 10
#define PSW_REGNUM 11
#define LIR_REGNUM 12
#define LAR_REGNUM 13
#define MDRQ_REGNUM 14
#define E0_REGNUM 15
#define MCRH_REGNUM 26
#define MCRL_REGNUM 27
#define MCVF_REGNUM 28

enum movm_register_bits {
  movm_exother_bit = 0x01,
  movm_exreg1_bit  = 0x02,
  movm_exreg0_bit  = 0x04,
  movm_other_bit   = 0x08,
  movm_a3_bit      = 0x10,
  movm_a2_bit      = 0x20,
  movm_d3_bit      = 0x40,
  movm_d2_bit      = 0x80
};

extern void _initialize_mn10300_tdep (void);
static CORE_ADDR mn10300_analyze_prologue (struct frame_info *fi,
					   CORE_ADDR pc);

/* mn10300 private data */
struct gdbarch_tdep
{
  int am33_mode;
#define AM33_MODE (gdbarch_tdep (current_gdbarch)->am33_mode)
};

/* Additional info used by the frame */

struct frame_extra_info
  {
    int status;
    int stack_size;
  };


static char *
register_name (int reg, char **regs, long sizeof_regs)
{
  if (reg < 0 || reg >= sizeof_regs / sizeof (regs[0]))
    return NULL;
  else
    return regs[reg];
}

static const char *
mn10300_generic_register_name (int reg)
{
  static char *regs[] =
  { "d0", "d1", "d2", "d3", "a0", "a1", "a2", "a3",
    "sp", "pc", "mdr", "psw", "lir", "lar", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "fp"
  };
  return register_name (reg, regs, sizeof regs);
}


static const char *
am33_register_name (int reg)
{
  static char *regs[] =
  { "d0", "d1", "d2", "d3", "a0", "a1", "a2", "a3",
    "sp", "pc", "mdr", "psw", "lir", "lar", "",
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
    "ssp", "msp", "usp", "mcrh", "mcrl", "mcvf", "", "", ""
  };
  return register_name (reg, regs, sizeof regs);
}
  
static CORE_ADDR
mn10300_saved_pc_after_call (struct frame_info *fi)
{
  return read_memory_integer (read_register (SP_REGNUM), 4);
}

static void
mn10300_extract_return_value (struct gdbarch *gdbarch, struct type *type,
			      struct regcache *regcache, void *valbuf)
{
  char buf[MAX_REGISTER_SIZE];
  int len = TYPE_LENGTH (type);
  int reg, regsz;

  if (TYPE_CODE (type) == TYPE_CODE_PTR)
    reg = 4;
  else
    reg = 0;

  regsz = register_size (gdbarch, reg);
  if (len <= regsz)
    {
      regcache_raw_read (regcache, reg, buf);
      memcpy (valbuf, buf, len);
    }
  else if (len <= 2 * regsz)
    {
      regcache_raw_read (regcache, reg, buf);
      memcpy (valbuf, buf, regsz);
      gdb_assert (regsz == register_size (gdbarch, reg + 1));
      regcache_raw_read (regcache, reg + 1, buf);
      memcpy ((char *) valbuf + regsz, buf, len - regsz);
    }
  else
    internal_error (__FILE__, __LINE__,
		    "Cannot extract return value %d bytes long.", len);
}

static void
mn10300_store_return_value (struct gdbarch *gdbarch, struct type *type,
			    struct regcache *regcache, const void *valbuf)
{
  int len = TYPE_LENGTH (type);
  int reg, regsz;
  
  if (TYPE_CODE (type) == TYPE_CODE_PTR)
    reg = 4;
  else
    reg = 0;

  regsz = register_size (gdbarch, reg);

  if (len <= regsz)
    regcache_raw_write_part (regcache, reg, 0, len, valbuf);
  else if (len <= 2 * regsz)
    {
      regcache_raw_write (regcache, reg, valbuf);
      gdb_assert (regsz == register_size (gdbarch, reg + 1));
      regcache_raw_write_part (regcache, reg+1, 0,
			       len - regsz, (char *) valbuf + regsz);
    }
  else
    internal_error (__FILE__, __LINE__,
		    "Cannot store return value %d bytes long.", len);
}

static struct frame_info *analyze_dummy_frame (CORE_ADDR, CORE_ADDR);
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
  get_frame_extra_info (dummy)->stack_size = 0;
  mn10300_analyze_prologue (dummy, pc);
  do_cleanups (old_chain);
  return dummy;
}

/* Values for frame_info.status */

#define MY_FRAME_IN_SP 0x1
#define MY_FRAME_IN_FP 0x2
#define NO_MORE_FRAMES 0x4

/* Compute the alignment required by a type.  */

static int
mn10300_type_align (struct type *type)
{
  int i, align = 1;

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_INT:
    case TYPE_CODE_ENUM:
    case TYPE_CODE_SET:
    case TYPE_CODE_RANGE:
    case TYPE_CODE_CHAR:
    case TYPE_CODE_BOOL:
    case TYPE_CODE_FLT:
    case TYPE_CODE_PTR:
    case TYPE_CODE_REF:
      return TYPE_LENGTH (type);

    case TYPE_CODE_COMPLEX:
      return TYPE_LENGTH (type) / 2;

    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      for (i = 0; i < TYPE_NFIELDS (type); i++)
	{
	  int falign = mn10300_type_align (TYPE_FIELD_TYPE (type, i));
	  while (align < falign)
	    align <<= 1;
	}
      return align;

    case TYPE_CODE_ARRAY:
      /* HACK!  Structures containing arrays, even small ones, are not
	 elligible for returning in registers.  */
      return 256;

    case TYPE_CODE_TYPEDEF:
      return mn10300_type_align (check_typedef (type));

    default:
      internal_error (__FILE__, __LINE__, "bad switch");
    }
}

/* Should call_function allocate stack space for a struct return?  */
static int
mn10300_use_struct_convention (struct type *type)
{
  /* Structures bigger than a pair of words can't be returned in
     registers.  */
  if (TYPE_LENGTH (type) > 8)
    return 1;

  switch (TYPE_CODE (type))
    {
    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      /* Structures with a single field are handled as the field
	 itself.  */
      if (TYPE_NFIELDS (type) == 1)
	return mn10300_use_struct_convention (TYPE_FIELD_TYPE (type, 0));

      /* Structures with word or double-word size are passed in memory, as
	 long as they require at least word alignment.  */
      if (mn10300_type_align (type) >= 4)
	return 0;

      return 1;

      /* Arrays are addressable, so they're never returned in
	 registers.  This condition can only hold when the array is
	 the only field of a struct or union.  */
    case TYPE_CODE_ARRAY:
      return 1;

    case TYPE_CODE_TYPEDEF:
      return mn10300_use_struct_convention (check_typedef (type));

    default:
      return 0;
    }
}

/* Determine, for architecture GDBARCH, how a return value of TYPE
   should be returned.  If it is supposed to be returned in registers,
   and READBUF is non-zero, read the appropriate value from REGCACHE,
   and copy it into READBUF.  If WRITEBUF is non-zero, write the value
   from WRITEBUF into REGCACHE.  */

static enum return_value_convention
mn10300_return_value (struct gdbarch *gdbarch, struct type *type,
		      struct regcache *regcache, void *readbuf,
		      const void *writebuf)
{
  if (mn10300_use_struct_convention (type))
    return RETURN_VALUE_STRUCT_CONVENTION;

  if (readbuf)
    mn10300_extract_return_value (gdbarch, type, regcache, readbuf);
  if (writebuf)
    mn10300_store_return_value (gdbarch, type, regcache, writebuf);

  return RETURN_VALUE_REGISTER_CONVENTION;
}

/* The breakpoint instruction must be the same size as the smallest
   instruction in the instruction set.

   The Matsushita mn10x00 processors have single byte instructions
   so we need a single byte breakpoint.  Matsushita hasn't defined
   one, so we defined it ourselves.  */

const static unsigned char *
mn10300_breakpoint_from_pc (CORE_ADDR *bp_addr, int *bp_size)
{
  static char breakpoint[] =
  {0xff};
  *bp_size = 1;
  return breakpoint;
}


/* Fix fi->frame if it's bogus at this point.  This is a helper
   function for mn10300_analyze_prologue. */

static void
fix_frame_pointer (struct frame_info *fi, int stack_size)
{
  if (fi && get_next_frame (fi) == NULL)
    {
      if (get_frame_extra_info (fi)->status & MY_FRAME_IN_SP)
	deprecated_update_frame_base_hack (fi, read_sp () - stack_size);
      else if (get_frame_extra_info (fi)->status & MY_FRAME_IN_FP)
	deprecated_update_frame_base_hack (fi, read_register (A3_REGNUM));
    }
}


/* Set offsets of registers saved by movm instruction.
   This is a helper function for mn10300_analyze_prologue.  */

static void
set_movm_offsets (struct frame_info *fi, int movm_args)
{
  int offset = 0;

  if (fi == NULL || movm_args == 0)
    return;

  if (movm_args & movm_other_bit)
    {
      /* The `other' bit leaves a blank area of four bytes at the
         beginning of its block of saved registers, making it 32 bytes
         long in total.  */
      deprecated_get_frame_saved_regs (fi)[LAR_REGNUM]    = get_frame_base (fi) + offset + 4;
      deprecated_get_frame_saved_regs (fi)[LIR_REGNUM]    = get_frame_base (fi) + offset + 8;
      deprecated_get_frame_saved_regs (fi)[MDR_REGNUM]    = get_frame_base (fi) + offset + 12;
      deprecated_get_frame_saved_regs (fi)[A0_REGNUM + 1] = get_frame_base (fi) + offset + 16;
      deprecated_get_frame_saved_regs (fi)[A0_REGNUM]     = get_frame_base (fi) + offset + 20;
      deprecated_get_frame_saved_regs (fi)[D0_REGNUM + 1] = get_frame_base (fi) + offset + 24;
      deprecated_get_frame_saved_regs (fi)[D0_REGNUM]     = get_frame_base (fi) + offset + 28;
      offset += 32;
    }
  if (movm_args & movm_a3_bit)
    {
      deprecated_get_frame_saved_regs (fi)[A3_REGNUM] = get_frame_base (fi) + offset;
      offset += 4;
    }
  if (movm_args & movm_a2_bit)
    {
      deprecated_get_frame_saved_regs (fi)[A2_REGNUM] = get_frame_base (fi) + offset;
      offset += 4;
    }
  if (movm_args & movm_d3_bit)
    {
      deprecated_get_frame_saved_regs (fi)[D3_REGNUM] = get_frame_base (fi) + offset;
      offset += 4;
    }
  if (movm_args & movm_d2_bit)
    {
      deprecated_get_frame_saved_regs (fi)[D2_REGNUM] = get_frame_base (fi) + offset;
      offset += 4;
    }
  if (AM33_MODE)
    {
      if (movm_args & movm_exother_bit)
        {
          deprecated_get_frame_saved_regs (fi)[MCVF_REGNUM]   = get_frame_base (fi) + offset;
          deprecated_get_frame_saved_regs (fi)[MCRL_REGNUM]   = get_frame_base (fi) + offset + 4;
          deprecated_get_frame_saved_regs (fi)[MCRH_REGNUM]   = get_frame_base (fi) + offset + 8;
          deprecated_get_frame_saved_regs (fi)[MDRQ_REGNUM]   = get_frame_base (fi) + offset + 12;
          deprecated_get_frame_saved_regs (fi)[E0_REGNUM + 1] = get_frame_base (fi) + offset + 16;
          deprecated_get_frame_saved_regs (fi)[E0_REGNUM + 0] = get_frame_base (fi) + offset + 20;
          offset += 24;
        }
      if (movm_args & movm_exreg1_bit)
        {
          deprecated_get_frame_saved_regs (fi)[E0_REGNUM + 7] = get_frame_base (fi) + offset;
          deprecated_get_frame_saved_regs (fi)[E0_REGNUM + 6] = get_frame_base (fi) + offset + 4;
          deprecated_get_frame_saved_regs (fi)[E0_REGNUM + 5] = get_frame_base (fi) + offset + 8;
          deprecated_get_frame_saved_regs (fi)[E0_REGNUM + 4] = get_frame_base (fi) + offset + 12;
          offset += 16;
        }
      if (movm_args & movm_exreg0_bit)
        {
          deprecated_get_frame_saved_regs (fi)[E0_REGNUM + 3] = get_frame_base (fi) + offset;
          deprecated_get_frame_saved_regs (fi)[E0_REGNUM + 2] = get_frame_base (fi) + offset + 4;
          offset += 8;
        }
    }
}


/* The main purpose of this file is dealing with prologues to extract
   information about stack frames and saved registers.

   In gcc/config/mn13000/mn10300.c, the expand_prologue prologue
   function is pretty readable, and has a nice explanation of how the
   prologue is generated.  The prologues generated by that code will
   have the following form (NOTE: the current code doesn't handle all
   this!):

   + If this is an old-style varargs function, then its arguments
     need to be flushed back to the stack:
     
        mov d0,(4,sp)
        mov d1,(4,sp)

   + If we use any of the callee-saved registers, save them now.
     
        movm [some callee-saved registers],(sp)

   + If we have any floating-point registers to save:

     - Decrement the stack pointer to reserve space for the registers.
       If the function doesn't need a frame pointer, we may combine
       this with the adjustment that reserves space for the frame.

        add -SIZE, sp

     - Save the floating-point registers.  We have two possible
       strategies:

       . Save them at fixed offset from the SP:

        fmov fsN,(OFFSETN,sp)
        fmov fsM,(OFFSETM,sp)
        ...

       Note that, if OFFSETN happens to be zero, you'll get the
       different opcode: fmov fsN,(sp)

       . Or, set a0 to the start of the save area, and then use
       post-increment addressing to save the FP registers.

        mov sp, a0
        add SIZE, a0
        fmov fsN,(a0+)
        fmov fsM,(a0+)
        ...

   + If the function needs a frame pointer, we set it here.

        mov sp, a3

   + Now we reserve space for the stack frame proper.  This could be
     merged into the `add -SIZE, sp' instruction for FP saves up
     above, unless we needed to set the frame pointer in the previous
     step, or the frame is so large that allocating the whole thing at
     once would put the FP register save slots out of reach of the
     addressing mode (128 bytes).
      
        add -SIZE, sp        

   One day we might keep the stack pointer constant, that won't
   change the code for prologues, but it will make the frame
   pointerless case much more common.  */

/* Analyze the prologue to determine where registers are saved,
   the end of the prologue, etc etc.  Return the end of the prologue
   scanned.

   We store into FI (if non-null) several tidbits of information:

   * stack_size -- size of this stack frame.  Note that if we stop in
   certain parts of the prologue/epilogue we may claim the size of the
   current frame is zero.  This happens when the current frame has
   not been allocated yet or has already been deallocated.

   * fsr -- Addresses of registers saved in the stack by this frame.

   * status -- A (relatively) generic status indicator.  It's a bitmask
   with the following bits: 

   MY_FRAME_IN_SP: The base of the current frame is actually in
   the stack pointer.  This can happen for frame pointerless
   functions, or cases where we're stopped in the prologue/epilogue
   itself.  For these cases mn10300_analyze_prologue will need up
   update fi->frame before returning or analyzing the register
   save instructions.

   MY_FRAME_IN_FP: The base of the current frame is in the
   frame pointer register ($a3).

   NO_MORE_FRAMES: Set this if the current frame is "start" or
   if the first instruction looks like mov <imm>,sp.  This tells
   frame chain to not bother trying to unwind past this frame.  */

static CORE_ADDR
mn10300_analyze_prologue (struct frame_info *fi, CORE_ADDR pc)
{
  CORE_ADDR func_addr, func_end, addr, stop;
  CORE_ADDR stack_size;
  int imm_size;
  unsigned char buf[4];
  int status, movm_args = 0;
  char *name;

  /* Use the PC in the frame if it's provided to look up the
     start of this function.

     Note: kevinb/2003-07-16: We used to do the following here:
	pc = (fi ? get_frame_pc (fi) : pc);
     But this is (now) badly broken when called from analyze_dummy_frame().
  */
  pc = (pc ? pc : get_frame_pc (fi));

  /* Find the start of this function.  */
  status = find_pc_partial_function (pc, &name, &func_addr, &func_end);

  /* Do nothing if we couldn't find the start of this function or if we're
     stopped at the first instruction in the prologue.  */
  if (status == 0)
    {
      return pc;
    }

  /* If we're in start, then give up.  */
  if (strcmp (name, "start") == 0)
    {
      if (fi != NULL)
	get_frame_extra_info (fi)->status = NO_MORE_FRAMES;
      return pc;
    }

  /* At the start of a function our frame is in the stack pointer.  */
  if (fi)
    get_frame_extra_info (fi)->status = MY_FRAME_IN_SP;

  /* Get the next two bytes into buf, we need two because rets is a two
     byte insn and the first isn't enough to uniquely identify it.  */
  status = deprecated_read_memory_nobpt (pc, buf, 2);
  if (status != 0)
    return pc;

#if 0
  /* Note: kevinb/2003-07-16: We shouldn't be making these sorts of
     changes to the frame in prologue examination code.  */
  /* If we're physically on an "rets" instruction, then our frame has
     already been deallocated.  Note this can also be true for retf
     and ret if they specify a size of zero.

     In this case fi->frame is bogus, we need to fix it.  */
  if (fi && buf[0] == 0xf0 && buf[1] == 0xfc)
    {
      if (get_next_frame (fi) == NULL)
	deprecated_update_frame_base_hack (fi, read_sp ());
      return get_frame_pc (fi);
    }

  /* Similarly if we're stopped on the first insn of a prologue as our
     frame hasn't been allocated yet.  */
  if (fi && get_frame_pc (fi) == func_addr)
    {
      if (get_next_frame (fi) == NULL)
	deprecated_update_frame_base_hack (fi, read_sp ());
      return get_frame_pc (fi);
    }
#endif

  /* Figure out where to stop scanning.  */
  stop = fi ? pc : func_end;

  /* Don't walk off the end of the function.  */
  stop = stop > func_end ? func_end : stop;

  /* Start scanning on the first instruction of this function.  */
  addr = func_addr;

  /* Suck in two bytes.  */
  if (addr + 2 >= stop
      || (status = deprecated_read_memory_nobpt (addr, buf, 2)) != 0)
    {
      fix_frame_pointer (fi, 0);
      return addr;
    }

  /* First see if this insn sets the stack pointer from a register; if
     so, it's probably the initialization of the stack pointer in _start,
     so mark this as the bottom-most frame.  */
  if (buf[0] == 0xf2 && (buf[1] & 0xf3) == 0xf0)
    {
      if (fi)
	get_frame_extra_info (fi)->status = NO_MORE_FRAMES;
      return addr;
    }

  /* Now look for movm [regs],sp, which saves the callee saved registers.

     At this time we don't know if fi->frame is valid, so we only note
     that we encountered a movm instruction.  Later, we'll set the entries
     in fsr.regs as needed.  */
  if (buf[0] == 0xcf)
    {
      /* Extract the register list for the movm instruction.  */
      status = deprecated_read_memory_nobpt (addr + 1, buf, 1);
      movm_args = *buf;

      addr += 2;

      /* Quit now if we're beyond the stop point.  */
      if (addr >= stop)
	{
	  /* Fix fi->frame since it's bogus at this point.  */
	  if (fi && get_next_frame (fi) == NULL)
	    deprecated_update_frame_base_hack (fi, read_sp ());

	  /* Note if/where callee saved registers were saved.  */
	  set_movm_offsets (fi, movm_args);
	  return addr;
	}

      /* Get the next two bytes so the prologue scan can continue.  */
      status = deprecated_read_memory_nobpt (addr, buf, 2);
      if (status != 0)
	{
	  /* Fix fi->frame since it's bogus at this point.  */
	  if (fi && get_next_frame (fi) == NULL)
	    deprecated_update_frame_base_hack (fi, read_sp ());

	  /* Note if/where callee saved registers were saved.  */
	  set_movm_offsets (fi, movm_args);
	  return addr;
	}
    }

  /* Now see if we set up a frame pointer via "mov sp,a3" */
  if (buf[0] == 0x3f)
    {
      addr += 1;

      /* The frame pointer is now valid.  */
      if (fi)
	{
	  get_frame_extra_info (fi)->status |= MY_FRAME_IN_FP;
	  get_frame_extra_info (fi)->status &= ~MY_FRAME_IN_SP;
	}

      /* Quit now if we're beyond the stop point.  */
      if (addr >= stop)
	{
	  /* Fix fi->frame if it's bogus at this point.  */
	  fix_frame_pointer (fi, 0);

	  /* Note if/where callee saved registers were saved.  */
	  set_movm_offsets (fi, movm_args);
	  return addr;
	}

      /* Get two more bytes so scanning can continue.  */
      status = deprecated_read_memory_nobpt (addr, buf, 2);
      if (status != 0)
	{
	  /* Fix fi->frame if it's bogus at this point.  */
	  fix_frame_pointer (fi, 0);

	  /* Note if/where callee saved registers were saved.  */
	  set_movm_offsets (fi, movm_args);
	  return addr;
	}
    }

  /* Next we should allocate the local frame.  No more prologue insns
     are found after allocating the local frame.

     Search for add imm8,sp (0xf8feXX)
     or add imm16,sp (0xfafeXXXX)
     or add imm32,sp (0xfcfeXXXXXXXX).

     If none of the above was found, then this prologue has no 
     additional stack.  */

  status = deprecated_read_memory_nobpt (addr, buf, 2);
  if (status != 0)
    {
      /* Fix fi->frame if it's bogus at this point.  */
      fix_frame_pointer (fi, 0);

      /* Note if/where callee saved registers were saved.  */
      set_movm_offsets (fi, movm_args);
      return addr;
    }

  imm_size = 0;
  if (buf[0] == 0xf8 && buf[1] == 0xfe)
    imm_size = 1;
  else if (buf[0] == 0xfa && buf[1] == 0xfe)
    imm_size = 2;
  else if (buf[0] == 0xfc && buf[1] == 0xfe)
    imm_size = 4;

  if (imm_size != 0)
    {
      /* Suck in imm_size more bytes, they'll hold the size of the
         current frame.  */
      status = deprecated_read_memory_nobpt (addr + 2, buf, imm_size);
      if (status != 0)
	{
	  /* Fix fi->frame if it's bogus at this point.  */
	  fix_frame_pointer (fi, 0);

	  /* Note if/where callee saved registers were saved.  */
	  set_movm_offsets (fi, movm_args);
	  return addr;
	}

      /* Note the size of the stack in the frame info structure.  */
      stack_size = extract_signed_integer (buf, imm_size);
      if (fi)
	get_frame_extra_info (fi)->stack_size = stack_size;

      /* We just consumed 2 + imm_size bytes.  */
      addr += 2 + imm_size;

      /* No more prologue insns follow, so begin preparation to return.  */
      /* Fix fi->frame if it's bogus at this point.  */
      fix_frame_pointer (fi, stack_size);

      /* Note if/where callee saved registers were saved.  */
      set_movm_offsets (fi, movm_args);
      return addr;
    }

  /* We never found an insn which allocates local stack space, regardless
     this is the end of the prologue.  */
  /* Fix fi->frame if it's bogus at this point.  */
  fix_frame_pointer (fi, 0);

  /* Note if/where callee saved registers were saved.  */
  set_movm_offsets (fi, movm_args);
  return addr;
}


/* Function: saved_regs_size
   Return the size in bytes of the register save area, based on the
   saved_regs array in FI.  */
static int
saved_regs_size (struct frame_info *fi)
{
  int adjust = 0;
  int i;

  /* Reserve four bytes for every register saved.  */
  for (i = 0; i < NUM_REGS; i++)
    if (deprecated_get_frame_saved_regs (fi)[i])
      adjust += 4;

  /* If we saved LIR, then it's most likely we used a `movm'
     instruction with the `other' bit set, in which case the SP is
     decremented by an extra four bytes, "to simplify calculation
     of the transfer area", according to the processor manual.  */
  if (deprecated_get_frame_saved_regs (fi)[LIR_REGNUM])
    adjust += 4;

  return adjust;
}


/* Function: frame_chain
   Figure out and return the caller's frame pointer given current
   frame_info struct.

   We don't handle dummy frames yet but we would probably just return the
   stack pointer that was in use at the time the function call was made?  */

static CORE_ADDR
mn10300_frame_chain (struct frame_info *fi)
{
  struct frame_info *dummy;
  /* Walk through the prologue to determine the stack size,
     location of saved registers, end of the prologue, etc.  */
  if (get_frame_extra_info (fi)->status == 0)
    mn10300_analyze_prologue (fi, (CORE_ADDR) 0);

  /* Quit now if mn10300_analyze_prologue set NO_MORE_FRAMES.  */
  if (get_frame_extra_info (fi)->status & NO_MORE_FRAMES)
    return 0;

  /* Now that we've analyzed our prologue, determine the frame
     pointer for our caller.

     If our caller has a frame pointer, then we need to
     find the entry value of $a3 to our function.

     If fsr.regs[A3_REGNUM] is nonzero, then it's at the memory
     location pointed to by fsr.regs[A3_REGNUM].

     Else it's still in $a3.

     If our caller does not have a frame pointer, then his
     frame base is fi->frame + -caller's stack size.  */

  /* The easiest way to get that info is to analyze our caller's frame.
     So we set up a dummy frame and call mn10300_analyze_prologue to
     find stuff for us.  */
  dummy = analyze_dummy_frame (DEPRECATED_FRAME_SAVED_PC (fi), get_frame_base (fi));

  if (get_frame_extra_info (dummy)->status & MY_FRAME_IN_FP)
    {
      /* Our caller has a frame pointer.  So find the frame in $a3 or
         in the stack.  */
      if (deprecated_get_frame_saved_regs (fi)[A3_REGNUM])
	return (read_memory_integer (deprecated_get_frame_saved_regs (fi)[A3_REGNUM],
				     DEPRECATED_REGISTER_SIZE));
      else
	return read_register (A3_REGNUM);
    }
  else
    {
      int adjust = saved_regs_size (fi);

      /* Our caller does not have a frame pointer.  So his frame starts
         at the base of our frame (fi->frame) + register save space
         + <his size>.  */
      return get_frame_base (fi) + adjust + -get_frame_extra_info (dummy)->stack_size;
    }
}

/* Function: skip_prologue
   Return the address of the first inst past the prologue of the function.  */

static CORE_ADDR
mn10300_skip_prologue (CORE_ADDR pc)
{
  /* We used to check the debug symbols, but that can lose if
     we have a null prologue.  */
  return mn10300_analyze_prologue (NULL, pc);
}

/* generic_pop_current_frame calls this function if the current
   frame isn't a dummy frame.  */
static void
mn10300_pop_frame_regular (struct frame_info *frame)
{
  int regnum;

  write_register (PC_REGNUM, DEPRECATED_FRAME_SAVED_PC (frame));

  /* Restore any saved registers.  */
  for (regnum = 0; regnum < NUM_REGS; regnum++)
    if (deprecated_get_frame_saved_regs (frame)[regnum] != 0)
      {
        ULONGEST value;

        value = read_memory_unsigned_integer (deprecated_get_frame_saved_regs (frame)[regnum],
                                              register_size (current_gdbarch, regnum));
        write_register (regnum, value);
      }

  /* Actually cut back the stack, adjusted by the saved registers like
     ret would.  */
  write_register (SP_REGNUM, get_frame_base (frame) + saved_regs_size (frame));
}

/* Function: pop_frame
   This routine gets called when either the user uses the `return'
   command, or the call dummy breakpoint gets hit.  */
static void
mn10300_pop_frame (void)
{
  struct frame_info *frame = get_current_frame ();
  if (get_frame_type (frame) == DUMMY_FRAME)
    /* NOTE: cagney/2002-22-23: Does this ever occure?  Surely a dummy
       frame will have already been poped by the "infrun.c" code.  */
    deprecated_pop_dummy_frame ();
  else
    mn10300_pop_frame_regular (frame);
  /* Throw away any cached frame information.  */
  flush_cached_frames ();
}

/* Function: push_arguments
   Setup arguments for a call to the target.  Arguments go in
   order on the stack.  */

static CORE_ADDR
mn10300_push_arguments (int nargs, struct value **args, CORE_ADDR sp,
			int struct_return, CORE_ADDR struct_addr)
{
  int argnum = 0;
  int len = 0;
  int stack_offset = 0;
  int regsused = struct_return ? 1 : 0;

  /* This should be a nop, but align the stack just in case something
     went wrong.  Stacks are four byte aligned on the mn10300.  */
  sp &= ~3;

  /* Now make space on the stack for the args.

     XXX This doesn't appear to handle pass-by-invisible reference
     arguments.  */
  for (argnum = 0; argnum < nargs; argnum++)
    {
      int arg_length = (TYPE_LENGTH (VALUE_TYPE (args[argnum])) + 3) & ~3;

      while (regsused < 2 && arg_length > 0)
	{
	  regsused++;
	  arg_length -= 4;
	}
      len += arg_length;
    }

  /* Allocate stack space.  */
  sp -= len;

  regsused = struct_return ? 1 : 0;
  /* Push all arguments onto the stack. */
  for (argnum = 0; argnum < nargs; argnum++)
    {
      int len;
      char *val;

      /* XXX Check this.  What about UNIONS?  */
      if (TYPE_CODE (VALUE_TYPE (*args)) == TYPE_CODE_STRUCT
	  && TYPE_LENGTH (VALUE_TYPE (*args)) > 8)
	{
	  /* XXX Wrong, we want a pointer to this argument.  */
	  len = TYPE_LENGTH (VALUE_TYPE (*args));
	  val = (char *) VALUE_CONTENTS (*args);
	}
      else
	{
	  len = TYPE_LENGTH (VALUE_TYPE (*args));
	  val = (char *) VALUE_CONTENTS (*args);
	}

      while (regsused < 2 && len > 0)
	{
	  write_register (regsused, extract_unsigned_integer (val, 4));
	  val += 4;
	  len -= 4;
	  regsused++;
	}

      while (len > 0)
	{
	  write_memory (sp + stack_offset, val, 4);
	  len -= 4;
	  val += 4;
	  stack_offset += 4;
	}

      args++;
    }

  /* Make space for the flushback area.  */
  sp -= 8;
  return sp;
}

/* Function: push_return_address (pc)
   Set up the return address for the inferior function call.
   Needed for targets where we don't actually execute a JSR/BSR instruction */

static CORE_ADDR
mn10300_push_return_address (CORE_ADDR pc, CORE_ADDR sp)
{
  unsigned char buf[4];

  store_unsigned_integer (buf, 4, entry_point_address ());
  write_memory (sp - 4, buf, 4);
  return sp - 4;
}

/* Function: store_struct_return (addr,sp)
   Store the structure value return address for an inferior function
   call.  */

static void
mn10300_store_struct_return (CORE_ADDR addr, CORE_ADDR sp)
{
  /* The structure return address is passed as the first argument.  */
  write_register (0, addr);
}

/* Function: frame_saved_pc 
   Find the caller of this frame.  We do this by seeing if RP_REGNUM
   is saved in the stack anywhere, otherwise we get it from the
   registers.  If the inner frame is a dummy frame, return its PC
   instead of RP, because that's where "caller" of the dummy-frame
   will be found.  */

static CORE_ADDR
mn10300_frame_saved_pc (struct frame_info *fi)
{
  int adjust = saved_regs_size (fi);

  return (read_memory_integer (get_frame_base (fi) + adjust,
			       DEPRECATED_REGISTER_SIZE));
}

/* Function: mn10300_init_extra_frame_info
   Setup the frame's frame pointer, pc, and frame addresses for saved
   registers.  Most of the work is done in mn10300_analyze_prologue().

   Note that when we are called for the last frame (currently active frame),
   that get_frame_pc (fi) and fi->frame will already be setup.  However, fi->frame will
   be valid only if this routine uses FP.  For previous frames, fi-frame will
   always be correct.  mn10300_analyze_prologue will fix fi->frame if
   it's not valid.

   We can be called with the PC in the call dummy under two
   circumstances.  First, during normal backtracing, second, while
   figuring out the frame pointer just prior to calling the target
   function (see call_function_by_hand).  */

static void
mn10300_init_extra_frame_info (int fromleaf, struct frame_info *fi)
{
  if (get_next_frame (fi))
    deprecated_update_frame_pc_hack (fi, DEPRECATED_FRAME_SAVED_PC (get_next_frame (fi)));

  frame_saved_regs_zalloc (fi);
  frame_extra_info_zalloc (fi, sizeof (struct frame_extra_info));

  get_frame_extra_info (fi)->status = 0;
  get_frame_extra_info (fi)->stack_size = 0;

  mn10300_analyze_prologue (fi, 0);
}


/* This function's job is handled by init_extra_frame_info.  */
static void
mn10300_frame_init_saved_regs (struct frame_info *frame)
{
}


/* Function: mn10300_virtual_frame_pointer
   Return the register that the function uses for a frame pointer, 
   plus any necessary offset to be applied to the register before
   any frame pointer offsets.  */

static void
mn10300_virtual_frame_pointer (CORE_ADDR pc,
			       int *reg,
			       LONGEST *offset)
{
  struct frame_info *dummy = analyze_dummy_frame (pc, 0);
  /* Set up a dummy frame_info, Analyze the prolog and fill in the
     extra info.  */
  /* Results will tell us which type of frame it uses.  */
  if (get_frame_extra_info (dummy)->status & MY_FRAME_IN_SP)
    {
      *reg = SP_REGNUM;
      *offset = -(get_frame_extra_info (dummy)->stack_size);
    }
  else
    {
      *reg = A3_REGNUM;
      *offset = 0;
    }
}

static int
mn10300_reg_struct_has_addr (int gcc_p, struct type *type)
{
  return (TYPE_LENGTH (type) > 8);
}

static struct type *
mn10300_register_virtual_type (int reg)
{
  return builtin_type_int;
}

static int
mn10300_register_byte (int reg)
{
  return (reg * 4);
}

static int
mn10300_register_virtual_size (int reg)
{
  return 4;
}

static int
mn10300_register_raw_size (int reg)
{
  return 4;
}

/* If DWARF2 is a register number appearing in Dwarf2 debug info, then
   mn10300_dwarf2_reg_to_regnum (DWARF2) is the corresponding GDB
   register number.  Why don't Dwarf2 and GDB use the same numbering?
   Who knows?  But since people have object files lying around with
   the existing Dwarf2 numbering, and other people have written stubs
   to work with the existing GDB, neither of them can change.  So we
   just have to cope.  */
static int
mn10300_dwarf2_reg_to_regnum (int dwarf2)
{
  /* This table is supposed to be shaped like the REGISTER_NAMES
     initializer in gcc/config/mn10300/mn10300.h.  Registers which
     appear in GCC's numbering, but have no counterpart in GDB's
     world, are marked with a -1.  */
  static int dwarf2_to_gdb[] = {
    0,  1,  2,  3,  4,  5,  6,  7, -1, 8,
    15, 16, 17, 18, 19, 20, 21, 22
  };
  int gdb;

  if (dwarf2 < 0
      || dwarf2 >= (sizeof (dwarf2_to_gdb) / sizeof (dwarf2_to_gdb[0]))
      || dwarf2_to_gdb[dwarf2] == -1)
    internal_error (__FILE__, __LINE__,
                    "bogus register number in debug info: %d", dwarf2);

  return dwarf2_to_gdb[dwarf2];
}

static void
mn10300_print_register (const char *name, int regnum, int reg_width)
{
  char raw_buffer[MAX_REGISTER_SIZE];

  if (reg_width)
    printf_filtered ("%*s: ", reg_width, name);
  else
    printf_filtered ("%s: ", name);

  /* Get the data */
  if (!frame_register_read (deprecated_selected_frame, regnum, raw_buffer))
    {
      printf_filtered ("[invalid]");
      return;
    }
  else
    {
      int byte;
      if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
	{
	  for (byte = register_size (current_gdbarch, regnum) - DEPRECATED_REGISTER_VIRTUAL_SIZE (regnum);
	       byte < register_size (current_gdbarch, regnum);
	       byte++)
	    printf_filtered ("%02x", (unsigned char) raw_buffer[byte]);
	}
      else
	{
	  for (byte = DEPRECATED_REGISTER_VIRTUAL_SIZE (regnum) - 1;
	       byte >= 0;
	       byte--)
	    printf_filtered ("%02x", (unsigned char) raw_buffer[byte]);
	}
    }
}

static void
mn10300_do_registers_info (int regnum, int fpregs)
{
  if (regnum >= 0)
    {
      const char *name = REGISTER_NAME (regnum);
      if (name == NULL || name[0] == '\0')
	error ("Not a valid register for the current processor type");
      mn10300_print_register (name, regnum, 0);
      printf_filtered ("\n");
    }
  else
    {
      /* print registers in an array 4x8 */
      int r;
      int reg;
      const int nr_in_row = 4;
      const int reg_width = 4;
      for (r = 0; r < NUM_REGS; r += nr_in_row)
	{
	  int c;
	  int printing = 0;
	  int padding = 0;
	  for (c = r; c < r + nr_in_row; c++)
	    {
	      const char *name = REGISTER_NAME (c);
	      if (name != NULL && *name != '\0')
		{
		  printing = 1;
		  while (padding > 0)
		    {
		      printf_filtered (" ");
		      padding--;
		    }
		  mn10300_print_register (name, c, reg_width);
		  printf_filtered (" ");
		}
	      else
		{
		  padding += (reg_width + 2 + 8 + 1);
		}
	    }
	  if (printing)
	    printf_filtered ("\n");
	}
    }
}

static CORE_ADDR
mn10300_read_fp (void)
{
  /* That's right, we're using the stack pointer as our frame pointer.  */
  gdb_assert (SP_REGNUM >= 0);
  return read_register (SP_REGNUM);
}

/* Dump out the mn10300 speciic architecture information. */

static void
mn10300_dump_tdep (struct gdbarch *current_gdbarch, struct ui_file *file)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  fprintf_unfiltered (file, "mn10300_dump_tdep: am33_mode = %d\n",
		      tdep->am33_mode);
}

static struct gdbarch *
mn10300_gdbarch_init (struct gdbarch_info info,
		      struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *tdep = NULL;
  int am33_mode;
  gdbarch_register_name_ftype *register_name;
  int mach;
  int num_regs;

  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;
  tdep = xmalloc (sizeof (struct gdbarch_tdep));
  gdbarch = gdbarch_alloc (&info, tdep);

  if (info.bfd_arch_info != NULL
      && info.bfd_arch_info->arch == bfd_arch_mn10300)
    mach = info.bfd_arch_info->mach;
  else
    mach = 0;
  switch (mach)
    {
    case 0:
    case bfd_mach_mn10300:
      am33_mode = 0;
      register_name = mn10300_generic_register_name;
      num_regs = 32;
      break;
    case bfd_mach_am33:
      am33_mode = 1;
      register_name = am33_register_name;
      num_regs = 32;
      break;
    default:
      internal_error (__FILE__, __LINE__,
		      "mn10300_gdbarch_init: Unknown mn10300 variant");
      return NULL; /* keep GCC happy. */
    }

  /* Registers.  */
  set_gdbarch_num_regs (gdbarch, num_regs);
  set_gdbarch_register_name (gdbarch, register_name);
  set_gdbarch_deprecated_register_size (gdbarch, 4);
  set_gdbarch_deprecated_register_raw_size (gdbarch, mn10300_register_raw_size);
  set_gdbarch_deprecated_register_byte (gdbarch, mn10300_register_byte);
  set_gdbarch_deprecated_register_virtual_size (gdbarch, mn10300_register_virtual_size);
  set_gdbarch_deprecated_register_virtual_type (gdbarch, mn10300_register_virtual_type);
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, mn10300_dwarf2_reg_to_regnum);
  set_gdbarch_deprecated_do_registers_info (gdbarch, mn10300_do_registers_info);
  set_gdbarch_sp_regnum (gdbarch, 8);
  set_gdbarch_pc_regnum (gdbarch, 9);
  set_gdbarch_deprecated_fp_regnum (gdbarch, 31);
  set_gdbarch_virtual_frame_pointer (gdbarch, mn10300_virtual_frame_pointer);

  /* Breakpoints.  */
  set_gdbarch_breakpoint_from_pc (gdbarch, mn10300_breakpoint_from_pc);

  /* Stack unwinding.  */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_deprecated_saved_pc_after_call (gdbarch, mn10300_saved_pc_after_call);
  set_gdbarch_deprecated_init_extra_frame_info (gdbarch, mn10300_init_extra_frame_info);
  set_gdbarch_deprecated_frame_init_saved_regs (gdbarch, mn10300_frame_init_saved_regs);
  set_gdbarch_deprecated_frame_chain (gdbarch, mn10300_frame_chain);
  set_gdbarch_deprecated_frame_saved_pc (gdbarch, mn10300_frame_saved_pc);
  set_gdbarch_return_value (gdbarch, mn10300_return_value);
  set_gdbarch_deprecated_store_struct_return (gdbarch, mn10300_store_struct_return);
  set_gdbarch_deprecated_pop_frame (gdbarch, mn10300_pop_frame);
  set_gdbarch_skip_prologue (gdbarch, mn10300_skip_prologue);
  /* That's right, we're using the stack pointer as our frame pointer.  */
  set_gdbarch_deprecated_target_read_fp (gdbarch, mn10300_read_fp);

  /* Calling functions in the inferior from GDB.  */
  set_gdbarch_deprecated_push_arguments (gdbarch, mn10300_push_arguments);
  set_gdbarch_deprecated_reg_struct_has_addr
    (gdbarch, mn10300_reg_struct_has_addr);
  set_gdbarch_deprecated_push_return_address (gdbarch, mn10300_push_return_address);

  tdep->am33_mode = am33_mode;

  /* Should be using push_dummy_call.  */
  set_gdbarch_deprecated_dummy_write_sp (gdbarch, deprecated_write_sp);

  set_gdbarch_print_insn (gdbarch, print_insn_mn10300);

  return gdbarch;
}
 
void
_initialize_mn10300_tdep (void)
{
/*  printf("_initialize_mn10300_tdep\n"); */
  gdbarch_register (bfd_arch_mn10300, mn10300_gdbarch_init, mn10300_dump_tdep);
}

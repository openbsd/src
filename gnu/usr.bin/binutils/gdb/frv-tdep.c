/* Target-dependent code for the Fujitsu FR-V, for GDB, the GNU Debugger.
   Copyright 2002, 2003, 2004 Free Software Foundation, Inc.

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
#include "gdb_string.h"
#include "inferior.h"
#include "gdbcore.h"
#include "arch-utils.h"
#include "regcache.h"
#include "frame.h"
#include "frame-unwind.h"
#include "frame-base.h"
#include "trad-frame.h"
#include "dis-asm.h"
#include "gdb_assert.h"
#include "sim-regno.h"
#include "gdb/sim-frv.h"
#include "opcodes/frv-desc.h"	/* for the H_SPR_... enums */
#include "symtab.h"

extern void _initialize_frv_tdep (void);

static gdbarch_init_ftype frv_gdbarch_init;

static gdbarch_register_name_ftype frv_register_name;
static gdbarch_breakpoint_from_pc_ftype frv_breakpoint_from_pc;
static gdbarch_adjust_breakpoint_address_ftype frv_gdbarch_adjust_breakpoint_address;
static gdbarch_skip_prologue_ftype frv_skip_prologue;

/* Register numbers.  The order in which these appear define the
   remote protocol, so take care in changing them.  */
enum {
  /* Register numbers 0 -- 63 are always reserved for general-purpose
     registers.  The chip at hand may have less.  */
  first_gpr_regnum = 0,
  sp_regnum = 1,
  fp_regnum = 2,
  struct_return_regnum = 3,
  last_gpr_regnum = 63,

  /* Register numbers 64 -- 127 are always reserved for floating-point
     registers.  The chip at hand may have less.  */
  first_fpr_regnum = 64,
  last_fpr_regnum = 127,

  /* The PC register.  */
  pc_regnum = 128,

  /* Register numbers 129 on up are always reserved for special-purpose
     registers.  */
  first_spr_regnum = 129,
  psr_regnum = 129,
  ccr_regnum = 130,
  cccr_regnum = 131,
  tbr_regnum = 135,
  brr_regnum = 136,
  dbar0_regnum = 137,
  dbar1_regnum = 138,
  dbar2_regnum = 139,
  dbar3_regnum = 140,
  lr_regnum = 145,
  lcr_regnum = 146,
  iacc0h_regnum = 147,
  iacc0l_regnum = 148,
  last_spr_regnum = 148,

  /* The total number of registers we know exist.  */
  frv_num_regs = last_spr_regnum + 1,

  /* Pseudo registers */
  first_pseudo_regnum = frv_num_regs,

  /* iacc0 - the 64-bit concatenation of iacc0h and iacc0l.  */
  iacc0_regnum = first_pseudo_regnum + 0,

  last_pseudo_regnum = iacc0_regnum,
  frv_num_pseudo_regs = last_pseudo_regnum - first_pseudo_regnum + 1,
};

static LONGEST frv_call_dummy_words[] =
{0};


struct frv_unwind_cache		/* was struct frame_extra_info */
  {
    /* The previous frame's inner-most stack address.  Used as this
       frame ID's stack_addr.  */
    CORE_ADDR prev_sp;

    /* The frame's base, optionally used by the high-level debug info.  */
    CORE_ADDR base;

    /* Table indicating the location of each and every register.  */
    struct trad_frame_saved_reg *saved_regs;
  };


/* A structure describing a particular variant of the FRV.
   We allocate and initialize one of these structures when we create
   the gdbarch object for a variant.

   At the moment, all the FR variants we support differ only in which
   registers are present; the portable code of GDB knows that
   registers whose names are the empty string don't exist, so the
   `register_names' array captures all the per-variant information we
   need.

   in the future, if we need to have per-variant maps for raw size,
   virtual type, etc., we should replace register_names with an array
   of structures, each of which gives all the necessary info for one
   register.  Don't stick parallel arrays in here --- that's so
   Fortran.  */
struct gdbarch_tdep
{
  /* How many general-purpose registers does this variant have?  */
  int num_gprs;

  /* How many floating-point registers does this variant have?  */
  int num_fprs;

  /* How many hardware watchpoints can it support?  */
  int num_hw_watchpoints;

  /* How many hardware breakpoints can it support?  */
  int num_hw_breakpoints;

  /* Register names.  */
  char **register_names;
};

#define CURRENT_VARIANT (gdbarch_tdep (current_gdbarch))


/* Allocate a new variant structure, and set up default values for all
   the fields.  */
static struct gdbarch_tdep *
new_variant (void)
{
  struct gdbarch_tdep *var;
  int r;
  char buf[20];

  var = xmalloc (sizeof (*var));
  memset (var, 0, sizeof (*var));
  
  var->num_gprs = 64;
  var->num_fprs = 64;
  var->num_hw_watchpoints = 0;
  var->num_hw_breakpoints = 0;

  /* By default, don't supply any general-purpose or floating-point
     register names.  */
  var->register_names 
    = (char **) xmalloc ((frv_num_regs + frv_num_pseudo_regs)
                         * sizeof (char *));
  for (r = 0; r < frv_num_regs + frv_num_pseudo_regs; r++)
    var->register_names[r] = "";

  /* Do, however, supply default names for the known special-purpose
     registers.  */

  var->register_names[pc_regnum] = "pc";
  var->register_names[lr_regnum] = "lr";
  var->register_names[lcr_regnum] = "lcr";
     
  var->register_names[psr_regnum] = "psr";
  var->register_names[ccr_regnum] = "ccr";
  var->register_names[cccr_regnum] = "cccr";
  var->register_names[tbr_regnum] = "tbr";

  /* Debug registers.  */
  var->register_names[brr_regnum] = "brr";
  var->register_names[dbar0_regnum] = "dbar0";
  var->register_names[dbar1_regnum] = "dbar1";
  var->register_names[dbar2_regnum] = "dbar2";
  var->register_names[dbar3_regnum] = "dbar3";

  /* iacc0 (Only found on MB93405.)  */
  var->register_names[iacc0h_regnum] = "iacc0h";
  var->register_names[iacc0l_regnum] = "iacc0l";
  var->register_names[iacc0_regnum] = "iacc0";

  return var;
}


/* Indicate that the variant VAR has NUM_GPRS general-purpose
   registers, and fill in the names array appropriately.  */
static void
set_variant_num_gprs (struct gdbarch_tdep *var, int num_gprs)
{
  int r;

  var->num_gprs = num_gprs;

  for (r = 0; r < num_gprs; ++r)
    {
      char buf[20];

      sprintf (buf, "gr%d", r);
      var->register_names[first_gpr_regnum + r] = xstrdup (buf);
    }
}


/* Indicate that the variant VAR has NUM_FPRS floating-point
   registers, and fill in the names array appropriately.  */
static void
set_variant_num_fprs (struct gdbarch_tdep *var, int num_fprs)
{
  int r;

  var->num_fprs = num_fprs;

  for (r = 0; r < num_fprs; ++r)
    {
      char buf[20];

      sprintf (buf, "fr%d", r);
      var->register_names[first_fpr_regnum + r] = xstrdup (buf);
    }
}


static const char *
frv_register_name (int reg)
{
  if (reg < 0)
    return "?toosmall?";
  if (reg >= frv_num_regs + frv_num_pseudo_regs)
    return "?toolarge?";

  return CURRENT_VARIANT->register_names[reg];
}


static struct type *
frv_register_type (struct gdbarch *gdbarch, int reg)
{
  if (reg >= first_fpr_regnum && reg <= last_fpr_regnum)
    return builtin_type_float;
  else if (reg == iacc0_regnum)
    return builtin_type_int64;
  else
    return builtin_type_int32;
}

static void
frv_pseudo_register_read (struct gdbarch *gdbarch, struct regcache *regcache,
                          int reg, void *buffer)
{
  if (reg == iacc0_regnum)
    {
      regcache_raw_read (regcache, iacc0h_regnum, buffer);
      regcache_raw_read (regcache, iacc0l_regnum, (bfd_byte *) buffer + 4);
    }
}

static void
frv_pseudo_register_write (struct gdbarch *gdbarch, struct regcache *regcache,
                          int reg, const void *buffer)
{
  if (reg == iacc0_regnum)
    {
      regcache_raw_write (regcache, iacc0h_regnum, buffer);
      regcache_raw_write (regcache, iacc0l_regnum, (bfd_byte *) buffer + 4);
    }
}

static int
frv_register_sim_regno (int reg)
{
  static const int spr_map[] =
    {
      H_SPR_PSR,		/* psr_regnum */
      H_SPR_CCR,		/* ccr_regnum */
      H_SPR_CCCR,		/* cccr_regnum */
      -1,			/* 132 */
      -1,			/* 133 */
      -1,			/* 134 */
      H_SPR_TBR,		/* tbr_regnum */
      H_SPR_BRR,		/* brr_regnum */
      H_SPR_DBAR0,		/* dbar0_regnum */
      H_SPR_DBAR1,		/* dbar1_regnum */
      H_SPR_DBAR2,		/* dbar2_regnum */
      H_SPR_DBAR3,		/* dbar3_regnum */
      -1,			/* 141 */
      -1,			/* 142 */
      -1,			/* 143 */
      -1,			/* 144 */
      H_SPR_LR,			/* lr_regnum */
      H_SPR_LCR,		/* lcr_regnum */
      H_SPR_IACC0H,		/* iacc0h_regnum */
      H_SPR_IACC0L		/* iacc0l_regnum */
    };

  gdb_assert (reg >= 0 && reg < NUM_REGS);

  if (first_gpr_regnum <= reg && reg <= last_gpr_regnum)
    return reg - first_gpr_regnum + SIM_FRV_GR0_REGNUM;
  else if (first_fpr_regnum <= reg && reg <= last_fpr_regnum)
    return reg - first_fpr_regnum + SIM_FRV_FR0_REGNUM;
  else if (pc_regnum == reg)
    return SIM_FRV_PC_REGNUM;
  else if (reg >= first_spr_regnum
           && reg < first_spr_regnum + sizeof (spr_map) / sizeof (spr_map[0]))
    {
      int spr_reg_offset = spr_map[reg - first_spr_regnum];

      if (spr_reg_offset < 0)
	return SIM_REGNO_DOES_NOT_EXIST;
      else
	return SIM_FRV_SPR0_REGNUM + spr_reg_offset;
    }

  internal_error (__FILE__, __LINE__, "Bad register number %d", reg);
}

static const unsigned char *
frv_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenp)
{
  static unsigned char breakpoint[] = {0xc0, 0x70, 0x00, 0x01};
  *lenp = sizeof (breakpoint);
  return breakpoint;
}

/* Define the maximum number of instructions which may be packed into a
   bundle (VLIW instruction).  */
static const int max_instrs_per_bundle = 8;

/* Define the size (in bytes) of an FR-V instruction.  */
static const int frv_instr_size = 4;

/* Adjust a breakpoint's address to account for the FR-V architecture's
   constraint that a break instruction must not appear as any but the
   first instruction in the bundle.  */
static CORE_ADDR
frv_gdbarch_adjust_breakpoint_address (struct gdbarch *gdbarch, CORE_ADDR bpaddr)
{
  int count = max_instrs_per_bundle;
  CORE_ADDR addr = bpaddr - frv_instr_size;
  CORE_ADDR func_start = get_pc_function_start (bpaddr);

  /* Find the end of the previous packing sequence.  This will be indicated
     by either attempting to access some inaccessible memory or by finding
     an instruction word whose packing bit is set to one. */
  while (count-- > 0 && addr >= func_start)
    {
      char instr[frv_instr_size];
      int status;

      status = read_memory_nobpt (addr, instr, sizeof instr);

      if (status != 0)
	break;

      /* This is a big endian architecture, so byte zero will have most
         significant byte.  The most significant bit of this byte is the
         packing bit.  */
      if (instr[0] & 0x80)
	break;

      addr -= frv_instr_size;
    }

  if (count > 0)
    bpaddr = addr + frv_instr_size;

  return bpaddr;
}


/* Return true if REG is a caller-saves ("scratch") register,
   false otherwise.  */
static int
is_caller_saves_reg (int reg)
{
  return ((4 <= reg && reg <= 7)
          || (14 <= reg && reg <= 15)
          || (32 <= reg && reg <= 47));
}


/* Return true if REG is a callee-saves register, false otherwise.  */
static int
is_callee_saves_reg (int reg)
{
  return ((16 <= reg && reg <= 31)
          || (48 <= reg && reg <= 63));
}


/* Return true if REG is an argument register, false otherwise.  */
static int
is_argument_reg (int reg)
{
  return (8 <= reg && reg <= 13);
}

/* Scan an FR-V prologue, starting at PC, until frame->PC.
   If FRAME is non-zero, fill in its saved_regs with appropriate addresses.
   We assume FRAME's saved_regs array has already been allocated and cleared.
   Return the first PC value after the prologue.

   Note that, for unoptimized code, we almost don't need this function
   at all; all arguments and locals live on the stack, so we just need
   the FP to find everything.  The catch: structures passed by value
   have their addresses living in registers; they're never spilled to
   the stack.  So if you ever want to be able to get to these
   arguments in any frame but the top, you'll need to do this serious
   prologue analysis.  */
static CORE_ADDR
frv_analyze_prologue (CORE_ADDR pc, struct frame_info *next_frame,
                      struct frv_unwind_cache *info)
{
  /* When writing out instruction bitpatterns, we use the following
     letters to label instruction fields:
     P - The parallel bit.  We don't use this.
     J - The register number of GRj in the instruction description.
     K - The register number of GRk in the instruction description.
     I - The register number of GRi.
     S - a signed imediate offset.
     U - an unsigned immediate offset.

     The dots below the numbers indicate where hex digit boundaries
     fall, to make it easier to check the numbers.  */

  /* Non-zero iff we've seen the instruction that initializes the
     frame pointer for this function's frame.  */
  int fp_set = 0;

  /* If fp_set is non_zero, then this is the distance from
     the stack pointer to frame pointer: fp = sp + fp_offset.  */
  int fp_offset = 0;

  /* Total size of frame prior to any alloca operations. */
  int framesize = 0;

  /* Flag indicating if lr has been saved on the stack.  */
  int lr_saved_on_stack = 0;

  /* The number of the general-purpose register we saved the return
     address ("link register") in, or -1 if we haven't moved it yet.  */
  int lr_save_reg = -1;

  /* Offset (from sp) at which lr has been saved on the stack.  */

  int lr_sp_offset = 0;

  /* If gr_saved[i] is non-zero, then we've noticed that general
     register i has been saved at gr_sp_offset[i] from the stack
     pointer.  */
  char gr_saved[64];
  int gr_sp_offset[64];

  /* The address of the most recently scanned prologue instruction.  */
  CORE_ADDR last_prologue_pc;

  /* The address of the next instruction. */
  CORE_ADDR next_pc;

  /* The upper bound to of the pc values to scan.  */
  CORE_ADDR lim_pc;

  memset (gr_saved, 0, sizeof (gr_saved));

  last_prologue_pc = pc;

  /* Try to compute an upper limit (on how far to scan) based on the
     line number info.  */
  lim_pc = skip_prologue_using_sal (pc);
  /* If there's no line number info, lim_pc will be 0.  In that case,
     set the limit to be 100 instructions away from pc.  Hopefully, this
     will be far enough away to account for the entire prologue.  Don't
     worry about overshooting the end of the function.  The scan loop
     below contains some checks to avoid scanning unreasonably far.  */
  if (lim_pc == 0)
    lim_pc = pc + 400;

  /* If we have a frame, we don't want to scan past the frame's pc.  This
     will catch those cases where the pc is in the prologue.  */
  if (next_frame)
    {
      CORE_ADDR frame_pc = frame_pc_unwind (next_frame);
      if (frame_pc < lim_pc)
	lim_pc = frame_pc;
    }

  /* Scan the prologue.  */
  while (pc < lim_pc)
    {
      LONGEST op = read_memory_integer (pc, 4);
      next_pc = pc + 4;

      /* The tests in this chain of ifs should be in order of
	 decreasing selectivity, so that more particular patterns get
	 to fire before less particular patterns.  */

      /* Some sort of control transfer instruction: stop scanning prologue.
	 Integer Conditional Branch:
	  X XXXX XX 0000110 XX XXXXXXXXXXXXXXXX
	 Floating-point / media Conditional Branch:
	  X XXXX XX 0000111 XX XXXXXXXXXXXXXXXX
	 LCR Conditional Branch to LR
	  X XXXX XX 0001110 XX XX 001 X XXXXXXXXXX
	 Integer conditional Branches to LR
	  X XXXX XX 0001110 XX XX 010 X XXXXXXXXXX
	  X XXXX XX 0001110 XX XX 011 X XXXXXXXXXX
	 Floating-point/Media Branches to LR
	  X XXXX XX 0001110 XX XX 110 X XXXXXXXXXX
	  X XXXX XX 0001110 XX XX 111 X XXXXXXXXXX
	 Jump and Link
	  X XXXXX X 0001100 XXXXXX XXXXXX XXXXXX
	  X XXXXX X 0001101 XXXXXX XXXXXX XXXXXX
	 Call
	  X XXXXXX 0001111 XXXXXXXXXXXXXXXXXX
	 Return from Trap
	  X XXXXX X 0000101 XXXXXX XXXXXX XXXXXX
	 Integer Conditional Trap
	  X XXXX XX 0000100 XXXXXX XXXX 00 XXXXXX
	  X XXXX XX 0011100 XXXXXX XXXXXXXXXXXX
	 Floating-point /media Conditional Trap
	  X XXXX XX 0000100 XXXXXX XXXX 01 XXXXXX
	  X XXXX XX 0011101 XXXXXX XXXXXXXXXXXX
	 Break
	  X XXXX XX 0000100 XXXXXX XXXX 11 XXXXXX
	 Media Trap
	  X XXXX XX 0000100 XXXXXX XXXX 10 XXXXXX */
      if ((op & 0x01d80000) == 0x00180000 /* Conditional branches and Call */
          || (op & 0x01f80000) == 0x00300000  /* Jump and Link */
	  || (op & 0x01f80000) == 0x00100000  /* Return from Trap, Trap */
	  || (op & 0x01f80000) == 0x00700000) /* Trap immediate */
	{
	  /* Stop scanning; not in prologue any longer.  */
	  break;
	}

      /* Loading something from memory into fp probably means that
         we're in the epilogue.  Stop scanning the prologue.
         ld @(GRi, GRk), fp
	 X 000010 0000010 XXXXXX 000100 XXXXXX
	 ldi @(GRi, d12), fp
	 X 000010 0110010 XXXXXX XXXXXXXXXXXX */
      else if ((op & 0x7ffc0fc0) == 0x04080100
               || (op & 0x7ffc0000) == 0x04c80000)
	{
	  break;
	}

      /* Setting the FP from the SP:
	 ori sp, 0, fp
	 P 000010 0100010 000001 000000000000 = 0x04881000
	 0 111111 1111111 111111 111111111111 = 0x7fffffff
             .    .   .    .   .    .   .   .
	 We treat this as part of the prologue.  */
      else if ((op & 0x7fffffff) == 0x04881000)
	{
	  fp_set = 1;
	  fp_offset = 0;
	  last_prologue_pc = next_pc;
	}

      /* Move the link register to the scratch register grJ, before saving:
         movsg lr, grJ
         P 000100 0000011 010000 000111 JJJJJJ = 0x080d01c0
         0 111111 1111111 111111 111111 000000 = 0x7fffffc0
             .    .   .    .   .    .    .   .
	 We treat this as part of the prologue.  */
      else if ((op & 0x7fffffc0) == 0x080d01c0)
        {
          int gr_j = op & 0x3f;

          /* If we're moving it to a scratch register, that's fine.  */
          if (is_caller_saves_reg (gr_j))
	    {
	      lr_save_reg = gr_j;
	      last_prologue_pc = next_pc;
	    }
        }

      /* To save multiple callee-saves registers on the stack, at
         offset zero:

	 std grK,@(sp,gr0)
	 P KKKKKK 0000011 000001 000011 000000 = 0x000c10c0
	 0 000000 1111111 111111 111111 111111 = 0x01ffffff

	 stq grK,@(sp,gr0)
	 P KKKKKK 0000011 000001 000100 000000 = 0x000c1100
	 0 000000 1111111 111111 111111 111111 = 0x01ffffff
             .    .   .    .   .    .    .   .
         We treat this as part of the prologue, and record the register's
	 saved address in the frame structure.  */
      else if ((op & 0x01ffffff) == 0x000c10c0
            || (op & 0x01ffffff) == 0x000c1100)
	{
	  int gr_k = ((op >> 25) & 0x3f);
	  int ope  = ((op >> 6)  & 0x3f);
          int count;
	  int i;

          /* Is it an std or an stq?  */
          if (ope == 0x03)
            count = 2;
          else
            count = 4;

	  /* Is it really a callee-saves register?  */
	  if (is_callee_saves_reg (gr_k))
	    {
	      for (i = 0; i < count; i++)
	        {
		  gr_saved[gr_k + i] = 1;
		  gr_sp_offset[gr_k + i] = 4 * i;
		}
	      last_prologue_pc = next_pc;
	    }
	}

      /* Adjusting the stack pointer.  (The stack pointer is GR1.)
	 addi sp, S, sp
         P 000001 0010000 000001 SSSSSSSSSSSS = 0x02401000
         0 111111 1111111 111111 000000000000 = 0x7ffff000
             .    .   .    .   .    .   .   .
	 We treat this as part of the prologue.  */
      else if ((op & 0x7ffff000) == 0x02401000)
        {
	  if (framesize == 0)
	    {
	      /* Sign-extend the twelve-bit field.
		 (Isn't there a better way to do this?)  */
	      int s = (((op & 0xfff) - 0x800) & 0xfff) - 0x800;

	      framesize -= s;
	      last_prologue_pc = pc;
	    }
	  else
	    {
	      /* If the prologue is being adjusted again, we've
	         likely gone too far; i.e. we're probably in the
		 epilogue.  */
	      break;
	    }
	}

      /* Setting the FP to a constant distance from the SP:
	 addi sp, S, fp
         P 000010 0010000 000001 SSSSSSSSSSSS = 0x04401000
         0 111111 1111111 111111 000000000000 = 0x7ffff000
             .    .   .    .   .    .   .   .
	 We treat this as part of the prologue.  */
      else if ((op & 0x7ffff000) == 0x04401000)
	{
	  /* Sign-extend the twelve-bit field.
	     (Isn't there a better way to do this?)  */
	  int s = (((op & 0xfff) - 0x800) & 0xfff) - 0x800;
	  fp_set = 1;
	  fp_offset = s;
	  last_prologue_pc = pc;
	}

      /* To spill an argument register to a scratch register:
	    ori GRi, 0, GRk
	 P KKKKKK 0100010 IIIIII 000000000000 = 0x00880000
	 0 000000 1111111 000000 111111111111 = 0x01fc0fff
	     .    .   .    .   .    .   .   .
	 For the time being, we treat this as a prologue instruction,
	 assuming that GRi is an argument register.  This one's kind
	 of suspicious, because it seems like it could be part of a
	 legitimate body instruction.  But we only come here when the
	 source info wasn't helpful, so we have to do the best we can.
	 Hopefully once GCC and GDB agree on how to emit line number
	 info for prologues, then this code will never come into play.  */
      else if ((op & 0x01fc0fff) == 0x00880000)
	{
	  int gr_i = ((op >> 12) & 0x3f);

          /* Make sure that the source is an arg register; if it is, we'll
	     treat it as a prologue instruction.  */
	  if (is_argument_reg (gr_i))
	    last_prologue_pc = next_pc;
	}

      /* To spill 16-bit values to the stack:
	     sthi GRk, @(fp, s)
	 P KKKKKK 1010001 000010 SSSSSSSSSSSS = 0x01442000
	 0 000000 1111111 111111 000000000000 = 0x01fff000
             .    .   .    .   .    .   .   . 
         And for 8-bit values, we use STB instructions.
	     stbi GRk, @(fp, s)
	 P KKKKKK 1010000 000010 SSSSSSSSSSSS = 0x01402000
	 0 000000 1111111 111111 000000000000 = 0x01fff000
	     .    .   .    .   .    .   .   .
         We check that GRk is really an argument register, and treat
         all such as part of the prologue.  */
      else if (   (op & 0x01fff000) == 0x01442000
	       || (op & 0x01fff000) == 0x01402000)
	{
	  int gr_k = ((op >> 25) & 0x3f);

          /* Make sure that GRk is really an argument register; treat
	     it as a prologue instruction if so.  */
	  if (is_argument_reg (gr_k))
	    last_prologue_pc = next_pc;
	}

      /* To save multiple callee-saves register on the stack, at a
         non-zero offset:

	 stdi GRk, @(sp, s)
	 P KKKKKK 1010011 000001 SSSSSSSSSSSS = 0x014c1000
	 0 000000 1111111 111111 000000000000 = 0x01fff000
             .    .   .    .   .    .   .   .
	 stqi GRk, @(sp, s)
	 P KKKKKK 1010100 000001 SSSSSSSSSSSS = 0x01501000
	 0 000000 1111111 111111 000000000000 = 0x01fff000
	     .    .   .    .   .    .   .   .
         We treat this as part of the prologue, and record the register's
	 saved address in the frame structure.  */
      else if ((op & 0x01fff000) == 0x014c1000
            || (op & 0x01fff000) == 0x01501000)
	{
	  int gr_k = ((op >> 25) & 0x3f);
          int count;
	  int i;

          /* Is it a stdi or a stqi?  */
          if ((op & 0x01fff000) == 0x014c1000)
            count = 2;
          else
            count = 4;

	  /* Is it really a callee-saves register?  */
	  if (is_callee_saves_reg (gr_k))
	    {
	      /* Sign-extend the twelve-bit field.
		 (Isn't there a better way to do this?)  */
	      int s = (((op & 0xfff) - 0x800) & 0xfff) - 0x800;

	      for (i = 0; i < count; i++)
		{
		  gr_saved[gr_k + i] = 1;
		  gr_sp_offset[gr_k + i] = s + (4 * i);
		}
	      last_prologue_pc = next_pc;
	    }
	}

      /* Storing any kind of integer register at any constant offset
         from any other register.

	 st GRk, @(GRi, gr0)
         P KKKKKK 0000011 IIIIII 000010 000000 = 0x000c0080
         0 000000 1111111 000000 111111 111111 = 0x01fc0fff
             .    .   .    .   .    .    .   .
	 sti GRk, @(GRi, d12)
	 P KKKKKK 1010010 IIIIII SSSSSSSSSSSS = 0x01480000
	 0 000000 1111111 000000 000000000000 = 0x01fc0000
             .    .   .    .   .    .   .   .
         These could be almost anything, but a lot of prologue
         instructions fall into this pattern, so let's decode the
         instruction once, and then work at a higher level.  */
      else if (((op & 0x01fc0fff) == 0x000c0080)
            || ((op & 0x01fc0000) == 0x01480000))
        {
          int gr_k = ((op >> 25) & 0x3f);
          int gr_i = ((op >> 12) & 0x3f);
          int offset;

          /* Are we storing with gr0 as an offset, or using an
             immediate value?  */
          if ((op & 0x01fc0fff) == 0x000c0080)
            offset = 0;
          else
            offset = (((op & 0xfff) - 0x800) & 0xfff) - 0x800;

          /* If the address isn't relative to the SP or FP, it's not a
             prologue instruction.  */
          if (gr_i != sp_regnum && gr_i != fp_regnum)
	    {
	      /* Do nothing; not a prologue instruction.  */
	    }

          /* Saving the old FP in the new frame (relative to the SP).  */
          else if (gr_k == fp_regnum && gr_i == sp_regnum)
	    {
	      gr_saved[fp_regnum] = 1;
              gr_sp_offset[fp_regnum] = offset;
	      last_prologue_pc = next_pc;
	    }

          /* Saving callee-saves register(s) on the stack, relative to
             the SP.  */
          else if (gr_i == sp_regnum
                   && is_callee_saves_reg (gr_k))
            {
              gr_saved[gr_k] = 1;
	      if (gr_i == sp_regnum)
		gr_sp_offset[gr_k] = offset;
	      else
		gr_sp_offset[gr_k] = offset + fp_offset;
	      last_prologue_pc = next_pc;
            }

          /* Saving the scratch register holding the return address.  */
          else if (lr_save_reg != -1
                   && gr_k == lr_save_reg)
	    {
	      lr_saved_on_stack = 1;
	      if (gr_i == sp_regnum)
		lr_sp_offset = offset;
	      else
	        lr_sp_offset = offset + fp_offset;
	      last_prologue_pc = next_pc;
	    }

          /* Spilling int-sized arguments to the stack.  */
          else if (is_argument_reg (gr_k))
	    last_prologue_pc = next_pc;
        }
      pc = next_pc;
    }

  if (next_frame && info)
    {
      int i;
      ULONGEST this_base;

      /* If we know the relationship between the stack and frame
         pointers, record the addresses of the registers we noticed.
         Note that we have to do this as a separate step at the end,
         because instructions may save relative to the SP, but we need
         their addresses relative to the FP.  */
      if (fp_set)
	  frame_unwind_unsigned_register (next_frame, fp_regnum, &this_base);
      else
	  frame_unwind_unsigned_register (next_frame, sp_regnum, &this_base);

      for (i = 0; i < 64; i++)
	if (gr_saved[i])
	  info->saved_regs[i].addr = this_base - fp_offset + gr_sp_offset[i];

      info->prev_sp = this_base - fp_offset + framesize;
      info->base = this_base;

      /* If LR was saved on the stack, record its location.  */
      if (lr_saved_on_stack)
	info->saved_regs[lr_regnum].addr = this_base - fp_offset + lr_sp_offset;

      /* The call instruction moves the caller's PC in the callee's LR.
	 Since this is an unwind, do the reverse.  Copy the location of LR
	 into PC (the address / regnum) so that a request for PC will be
	 converted into a request for the LR.  */
      info->saved_regs[pc_regnum] = info->saved_regs[lr_regnum];

      /* Save the previous frame's computed SP value.  */
      trad_frame_set_value (info->saved_regs, sp_regnum, info->prev_sp);
    }

  return last_prologue_pc;
}


static CORE_ADDR
frv_skip_prologue (CORE_ADDR pc)
{
  CORE_ADDR func_addr, func_end, new_pc;

  new_pc = pc;

  /* If the line table has entry for a line *within* the function
     (i.e., not in the prologue, and not past the end), then that's
     our location.  */
  if (find_pc_partial_function (pc, NULL, &func_addr, &func_end))
    {
      struct symtab_and_line sal;

      sal = find_pc_line (func_addr, 0);

      if (sal.line != 0 && sal.end < func_end)
	{
	  new_pc = sal.end;
	}
    }

  /* The FR-V prologue is at least five instructions long (twenty bytes).
     If we didn't find a real source location past that, then
     do a full analysis of the prologue.  */
  if (new_pc < pc + 20)
    new_pc = frv_analyze_prologue (pc, 0, 0);

  return new_pc;
}


static struct frv_unwind_cache *
frv_frame_unwind_cache (struct frame_info *next_frame,
			 void **this_prologue_cache)
{
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  CORE_ADDR pc;
  ULONGEST prev_sp;
  ULONGEST this_base;
  struct frv_unwind_cache *info;

  if ((*this_prologue_cache))
    return (*this_prologue_cache);

  info = FRAME_OBSTACK_ZALLOC (struct frv_unwind_cache);
  (*this_prologue_cache) = info;
  info->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  /* Prologue analysis does the rest...  */
  frv_analyze_prologue (frame_func_unwind (next_frame), next_frame, info);

  return info;
}

static void
frv_extract_return_value (struct type *type, struct regcache *regcache,
                          void *valbuf)
{
  int len = TYPE_LENGTH (type);

  if (len <= 4)
    {
      ULONGEST gpr8_val;
      regcache_cooked_read_unsigned (regcache, 8, &gpr8_val);
      store_unsigned_integer (valbuf, len, gpr8_val);
    }
  else if (len == 8)
    {
      ULONGEST regval;
      regcache_cooked_read_unsigned (regcache, 8, &regval);
      store_unsigned_integer (valbuf, 4, regval);
      regcache_cooked_read_unsigned (regcache, 9, &regval);
      store_unsigned_integer ((bfd_byte *) valbuf + 4, 4, regval);
    }
  else
    internal_error (__FILE__, __LINE__, "Illegal return value length: %d", len);
}

static CORE_ADDR
frv_extract_struct_value_address (struct regcache *regcache)
{
  ULONGEST addr;
  regcache_cooked_read_unsigned (regcache, struct_return_regnum, &addr);
  return addr;
}

static void
frv_store_struct_return (CORE_ADDR addr, CORE_ADDR sp)
{
  write_register (struct_return_regnum, addr);
}

static int
frv_frameless_function_invocation (struct frame_info *frame)
{
  return legacy_frameless_look_for_prologue (frame);
}

static CORE_ADDR
frv_frame_align (struct gdbarch *gdbarch, CORE_ADDR sp)
{
  /* Require dword alignment.  */
  return align_down (sp, 8);
}

static CORE_ADDR
frv_push_dummy_call (struct gdbarch *gdbarch, CORE_ADDR func_addr,
                     struct regcache *regcache, CORE_ADDR bp_addr,
                     int nargs, struct value **args, CORE_ADDR sp,
		     int struct_return, CORE_ADDR struct_addr)
{
  int argreg;
  int argnum;
  char *val;
  char valbuf[4];
  struct value *arg;
  struct type *arg_type;
  int len;
  enum type_code typecode;
  CORE_ADDR regval;
  int stack_space;
  int stack_offset;

#if 0
  printf("Push %d args at sp = %x, struct_return=%d (%x)\n",
	 nargs, (int) sp, struct_return, struct_addr);
#endif

  stack_space = 0;
  for (argnum = 0; argnum < nargs; ++argnum)
    stack_space += align_up (TYPE_LENGTH (VALUE_TYPE (args[argnum])), 4);

  stack_space -= (6 * 4);
  if (stack_space > 0)
    sp -= stack_space;

  /* Make sure stack is dword aligned. */
  sp = align_down (sp, 8);

  stack_offset = 0;

  argreg = 8;

  if (struct_return)
    regcache_cooked_write_unsigned (regcache, struct_return_regnum,
                                    struct_addr);

  for (argnum = 0; argnum < nargs; ++argnum)
    {
      arg = args[argnum];
      arg_type = check_typedef (VALUE_TYPE (arg));
      len = TYPE_LENGTH (arg_type);
      typecode = TYPE_CODE (arg_type);

      if (typecode == TYPE_CODE_STRUCT || typecode == TYPE_CODE_UNION)
	{
	  store_unsigned_integer (valbuf, 4, VALUE_ADDRESS (arg));
	  typecode = TYPE_CODE_PTR;
	  len = 4;
	  val = valbuf;
	}
      else
	{
	  val = (char *) VALUE_CONTENTS (arg);
	}

      while (len > 0)
	{
	  int partial_len = (len < 4 ? len : 4);

	  if (argreg < 14)
	    {
	      regval = extract_unsigned_integer (val, partial_len);
#if 0
	      printf("  Argnum %d data %x -> reg %d\n",
		     argnum, (int) regval, argreg);
#endif
	      regcache_cooked_write_unsigned (regcache, argreg, regval);
	      ++argreg;
	    }
	  else
	    {
#if 0
	      printf("  Argnum %d data %x -> offset %d (%x)\n",
		     argnum, *((int *)val), stack_offset, (int) (sp + stack_offset));
#endif
	      write_memory (sp + stack_offset, val, partial_len);
	      stack_offset += align_up (partial_len, 4);
	    }
	  len -= partial_len;
	  val += partial_len;
	}
    }

  /* Set the return address.  For the frv, the return breakpoint is
     always at BP_ADDR.  */
  regcache_cooked_write_unsigned (regcache, lr_regnum, bp_addr);

  /* Finally, update the SP register.  */
  regcache_cooked_write_unsigned (regcache, sp_regnum, sp);

  return sp;
}

static void
frv_store_return_value (struct type *type, struct regcache *regcache,
                        const void *valbuf)
{
  int len = TYPE_LENGTH (type);

  if (len <= 4)
    {
      bfd_byte val[4];
      memset (val, 0, sizeof (val));
      memcpy (val + (4 - len), valbuf, len);
      regcache_cooked_write (regcache, 8, val);
    }
  else if (len == 8)
    {
      regcache_cooked_write (regcache, 8, valbuf);
      regcache_cooked_write (regcache, 9, (bfd_byte *) valbuf + 4);
    }
  else
    internal_error (__FILE__, __LINE__,
                    "Don't know how to return a %d-byte value.", len);
}


/* Hardware watchpoint / breakpoint support for the FR500
   and FR400.  */

int
frv_check_watch_resources (int type, int cnt, int ot)
{
  struct gdbarch_tdep *var = CURRENT_VARIANT;

  /* Watchpoints not supported on simulator.  */
  if (strcmp (target_shortname, "sim") == 0)
    return 0;

  if (type == bp_hardware_breakpoint)
    {
      if (var->num_hw_breakpoints == 0)
	return 0;
      else if (cnt <= var->num_hw_breakpoints)
	return 1;
    }
  else
    {
      if (var->num_hw_watchpoints == 0)
	return 0;
      else if (ot)
	return -1;
      else if (cnt <= var->num_hw_watchpoints)
	return 1;
    }
  return -1;
}


CORE_ADDR
frv_stopped_data_address (void)
{
  CORE_ADDR brr, dbar0, dbar1, dbar2, dbar3;

  brr = read_register (brr_regnum);
  dbar0 = read_register (dbar0_regnum);
  dbar1 = read_register (dbar1_regnum);
  dbar2 = read_register (dbar2_regnum);
  dbar3 = read_register (dbar3_regnum);

  if (brr & (1<<11))
    return dbar0;
  else if (brr & (1<<10))
    return dbar1;
  else if (brr & (1<<9))
    return dbar2;
  else if (brr & (1<<8))
    return dbar3;
  else
    return 0;
}

static CORE_ADDR
frv_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, pc_regnum);
}

/* Given a GDB frame, determine the address of the calling function's
   frame.  This will be used to create a new GDB frame struct.  */

static void
frv_frame_this_id (struct frame_info *next_frame,
		    void **this_prologue_cache, struct frame_id *this_id)
{
  struct frv_unwind_cache *info
    = frv_frame_unwind_cache (next_frame, this_prologue_cache);
  CORE_ADDR base;
  CORE_ADDR func;
  struct minimal_symbol *msym_stack;
  struct frame_id id;

  /* The FUNC is easy.  */
  func = frame_func_unwind (next_frame);

  /* Check if the stack is empty.  */
  msym_stack = lookup_minimal_symbol ("_stack", NULL, NULL);
  if (msym_stack && info->base == SYMBOL_VALUE_ADDRESS (msym_stack))
    return;

  /* Hopefully the prologue analysis either correctly determined the
     frame's base (which is the SP from the previous frame), or set
     that base to "NULL".  */
  base = info->prev_sp;
  if (base == 0)
    return;

  id = frame_id_build (base, func);

  /* Check that we're not going round in circles with the same frame
     ID (but avoid applying the test to sentinel frames which do go
     round in circles).  Can't use frame_id_eq() as that doesn't yet
     compare the frame's PC value.  */
  if (frame_relative_level (next_frame) >= 0
      && get_frame_type (next_frame) != DUMMY_FRAME
      && frame_id_eq (get_frame_id (next_frame), id))
    return;

  (*this_id) = id;
}

static void
frv_frame_prev_register (struct frame_info *next_frame,
			  void **this_prologue_cache,
			  int regnum, int *optimizedp,
			  enum lval_type *lvalp, CORE_ADDR *addrp,
			  int *realnump, void *bufferp)
{
  struct frv_unwind_cache *info
    = frv_frame_unwind_cache (next_frame, this_prologue_cache);
  trad_frame_prev_register (next_frame, info->saved_regs, regnum,
			    optimizedp, lvalp, addrp, realnump, bufferp);
}

static const struct frame_unwind frv_frame_unwind = {
  NORMAL_FRAME,
  frv_frame_this_id,
  frv_frame_prev_register
};

static const struct frame_unwind *
frv_frame_sniffer (struct frame_info *next_frame)
{
  return &frv_frame_unwind;
}

static CORE_ADDR
frv_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct frv_unwind_cache *info
    = frv_frame_unwind_cache (next_frame, this_cache);
  return info->base;
}

static const struct frame_base frv_frame_base = {
  &frv_frame_unwind,
  frv_frame_base_address,
  frv_frame_base_address,
  frv_frame_base_address
};

static CORE_ADDR
frv_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, sp_regnum);
}


/* Assuming NEXT_FRAME->prev is a dummy, return the frame ID of that
   dummy frame.  The frame ID's base needs to match the TOS value
   saved by save_dummy_frame_tos(), and the PC match the dummy frame's
   breakpoint.  */

static struct frame_id
frv_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_id_build (frv_unwind_sp (gdbarch, next_frame),
			 frame_pc_unwind (next_frame));
}


static struct gdbarch *
frv_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *var;

  /* Check to see if we've already built an appropriate architecture
     object for this executable.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches)
    return arches->gdbarch;

  /* Select the right tdep structure for this variant.  */
  var = new_variant ();
  switch (info.bfd_arch_info->mach)
    {
    case bfd_mach_frv:
    case bfd_mach_frvsimple:
    case bfd_mach_fr500:
    case bfd_mach_frvtomcat:
    case bfd_mach_fr550:
      set_variant_num_gprs (var, 64);
      set_variant_num_fprs (var, 64);
      break;

    case bfd_mach_fr400:
      set_variant_num_gprs (var, 32);
      set_variant_num_fprs (var, 32);
      break;

    default:
      /* Never heard of this variant.  */
      return 0;
    }
  
  gdbarch = gdbarch_alloc (&info, var);

  set_gdbarch_short_bit (gdbarch, 16);
  set_gdbarch_int_bit (gdbarch, 32);
  set_gdbarch_long_bit (gdbarch, 32);
  set_gdbarch_long_long_bit (gdbarch, 64);
  set_gdbarch_float_bit (gdbarch, 32);
  set_gdbarch_double_bit (gdbarch, 64);
  set_gdbarch_long_double_bit (gdbarch, 64);
  set_gdbarch_ptr_bit (gdbarch, 32);

  set_gdbarch_num_regs (gdbarch, frv_num_regs);
  set_gdbarch_num_pseudo_regs (gdbarch, frv_num_pseudo_regs);

  set_gdbarch_sp_regnum (gdbarch, sp_regnum);
  set_gdbarch_deprecated_fp_regnum (gdbarch, fp_regnum);
  set_gdbarch_pc_regnum (gdbarch, pc_regnum);

  set_gdbarch_register_name (gdbarch, frv_register_name);
  set_gdbarch_register_type (gdbarch, frv_register_type);
  set_gdbarch_register_sim_regno (gdbarch, frv_register_sim_regno);

  set_gdbarch_pseudo_register_read (gdbarch, frv_pseudo_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, frv_pseudo_register_write);

  set_gdbarch_skip_prologue (gdbarch, frv_skip_prologue);
  set_gdbarch_breakpoint_from_pc (gdbarch, frv_breakpoint_from_pc);
  set_gdbarch_adjust_breakpoint_address (gdbarch, frv_gdbarch_adjust_breakpoint_address);

  set_gdbarch_deprecated_frameless_function_invocation (gdbarch, frv_frameless_function_invocation);

  set_gdbarch_use_struct_convention (gdbarch, always_use_struct_convention);
  set_gdbarch_extract_return_value (gdbarch, frv_extract_return_value);

  set_gdbarch_deprecated_store_struct_return (gdbarch, frv_store_struct_return);
  set_gdbarch_store_return_value (gdbarch, frv_store_return_value);
  set_gdbarch_deprecated_extract_struct_value_address (gdbarch, frv_extract_struct_value_address);

  /* Frame stuff.  */
  set_gdbarch_unwind_pc (gdbarch, frv_unwind_pc);
  set_gdbarch_unwind_sp (gdbarch, frv_unwind_sp);
  set_gdbarch_frame_align (gdbarch, frv_frame_align);
  frame_unwind_append_sniffer (gdbarch, frv_frame_sniffer);
  frame_base_set_default (gdbarch, &frv_frame_base);

  /* Settings for calling functions in the inferior.  */
  set_gdbarch_push_dummy_call (gdbarch, frv_push_dummy_call);
  set_gdbarch_unwind_dummy_id (gdbarch, frv_unwind_dummy_id);

  /* Settings that should be unnecessary.  */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  set_gdbarch_write_pc (gdbarch, generic_target_write_pc);

  set_gdbarch_remote_translate_xfer_address
    (gdbarch, generic_remote_translate_xfer_address);

  /* Hardware watchpoint / breakpoint support.  */
  switch (info.bfd_arch_info->mach)
    {
    case bfd_mach_frv:
    case bfd_mach_frvsimple:
    case bfd_mach_fr500:
    case bfd_mach_frvtomcat:
      /* fr500-style hardware debugging support.  */
      var->num_hw_watchpoints = 4;
      var->num_hw_breakpoints = 4;
      break;

    case bfd_mach_fr400:
      /* fr400-style hardware debugging support.  */
      var->num_hw_watchpoints = 2;
      var->num_hw_breakpoints = 4;
      break;

    default:
      /* Otherwise, assume we don't have hardware debugging support.  */
      var->num_hw_watchpoints = 0;
      var->num_hw_breakpoints = 0;
      break;
    }

  set_gdbarch_print_insn (gdbarch, print_insn_frv);

  return gdbarch;
}

void
_initialize_frv_tdep (void)
{
  register_gdbarch_init (bfd_arch_frv, frv_gdbarch_init);
}

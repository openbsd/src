/* Target-dependent code for PA-RISC.

   Copyright 2003, 2004 Free Software Foundation, Inc.

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
#include "arch-utils.h"
#include "dis-asm.h"
#include "floatformat.h"
#include "frame.h"
#include "frame-base.h"
#include "frame-unwind.h"
#include "gdbtypes.h"
#include "symtab.h"
#include "objfiles.h"
#include "osabi.h"
#include "regcache.h"
#include "target.h"
#include "trad-frame.h"

#include "gdb_assert.h"

#include "hppa-tdep.h"

/* This file implements a PA-RISC 32-bit ABI.  */

/* Please use the hppa32_-prefix for 32-bit specific code, the
   hppa64_-prefix for 64-bit specific code and the sparc_-prefix for
   sparc64-tdep.c.  The 64-bit specific code lives in hppa64-tdep.c;
   don't add any here.  */

/* The PA-RISCC Floating-Point Quad-Precision format is similar to
   big-endian IA-64 Quad-recision format.  */
#define floatformat_hppa_quad floatformat_ia64_quad_big

/* Macros to extract fields from PA-RISC instructions.  */
#define X_OP(i) (((i) >> 26) & 0x3f)
#define X_B(i) (((i) >> 21) & 0x1f)
#define X_R(i) (((i) >> 16) & 0x1f)
#define X_T(i) (((i) >> 16) & 0x1f)
#define X_IM14(i) ((i) & 0x3fff)
/* Sign extension macros.  */
#define X_DISP14(i) (low_sign_extend (X_IM14 (i), 14))

/* Fetch the instruction at PC.  Instructions may be fetched
   big-endian or little-endian dependent on the (optional) E-bit in
   the PSW.  Traditionally, the PA-RISC architecture is big-endian
   though.  */

unsigned long
hppa_fetch_instruction (CORE_ADDR pc)
{
  unsigned char buf[4];

  /* If we can't read the instruction at PC, return zero.  */
  if (target_read_memory (pc, buf, sizeof (buf)))
    return 0;

  return extract_unsigned_integer (buf, 4);
}

static CORE_ADDR
hppa_addr_bits_remove (CORE_ADDR addr)
{
  return addr & ~0x3;
}

static LONGEST
low_sign_extend (ULONGEST val, ULONGEST bits)
{
  return (LONGEST) ((val & 0x1 ? (-1 << (bits - 1)) : 0) | val >> 1);
}

static const char *hppa32_register_names[] =
{
  "r0", "r1", "rp", "r3", "r4", "r5", "r6", "r7",
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
  "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
  "r24", "r25", "r26", "r27", "r28", "r29", "sp", "r31",
  "sar", "pcoqh", "pcoqt"
};

/* Total number of registers.  */
#define HPPA32_NUM_REGS ARRAY_SIZE (hppa32_register_names)

/* Return the name of register REGNUM.  */

static const char *
hppa32_register_name (int regnum)
{
  if (regnum >= 0 && regnum < HPPA32_NUM_REGS)
    return hppa32_register_names[regnum];

  return NULL;
}

/* Return the GDB type object for the "standard" data type of data in
   register REGNUM. */

static struct type *
hppa32_register_type (struct gdbarch *gdbarch, int regnum)
{
  if (regnum == HPPA_SP_REGNUM)
    return builtin_type_void_data_ptr;

  if (regnum == HPPA_RP_REGNUM
      || regnum == HPPA_PCOQ_HEAD_REGNUM
      || regnum == HPPA_PCOQ_TAIL_REGNUM)
    return builtin_type_void_func_ptr;

  return builtin_type_int32;
}


/* Use the program counter to determine the contents and size of a
   breakpoint instruction.  Return a pointer to a string of bytes that
   encode a breakpoint instruction, store the length of the string in
   *LEN and optionally adjust *PC to point to the correct memory
   location for inserting the breakpoint.  */
   
static const unsigned char *
hppa_breakpoint_from_pc (CORE_ADDR *pc, int *len)
{
  static unsigned char break_insn[] = { 0x00, 0x01, 0x00, 0x04 };

  *len = sizeof (break_insn);
  return break_insn;
}


CORE_ADDR
hppa32_analyze_prologue (CORE_ADDR start_pc, CORE_ADDR current_pc,
			 struct hppa_frame_cache *cache)
{
  unsigned long insn;
  CORE_ADDR pc;

  /* When compiling without optimization, GCC uses %r3 as a frame
     pointer and emits the following prologue to set up the stack
     frame:

        stw %rp,-20(%sp)
        copy %r3,%r1
	copy %r30,%r3
	stwm %r1,d(%sp)

     This instruction sequence is followed by instructions that save
     the callee-saved registers.  BSD system call stubs use a somewhat
     different calling convention where the return pointer (%rp) is
     saved in the "External/Stub RP" slot instead of the "Current RP"
     slot.  In that case the first instruction is "stw %rp,-24(%sp)".  */

  /* We'll increase START_PC as we encounter instructions that we
     recognize as part of the prologue.  */

  /* If CURRENT_PC isn't set, provide a reasonable default that's
     guaranteed to cover the entire prologue.  */
  if (current_pc == (CORE_ADDR) -1)
    current_pc = start_pc + 64;

  /* Short-circuit if CURRENT_PC point at the start of this function.  */
  else if (current_pc <= start_pc)
    return start_pc;

  /* Provide a dummy cache if necessary.  */
  if (cache == NULL)
    {
      size_t sizeof_saved_regs =
	HPPA32_NUM_REGS * sizeof (struct trad_frame_saved_reg);

      cache = alloca (sizeof (struct hppa_frame_cache));
      cache->saved_regs = alloca (sizeof_saved_regs);

      /* We only initialize the members we care about.  */
      cache->frame_size = 0;
      cache->saved_regs[HPPA_R3_REGNUM].realreg = -1;
    }

  for (pc = start_pc; pc < current_pc; pc += 4)
    {
      /* Fetch the next instruction.  */
      insn = hppa_fetch_instruction (pc);

      /* stw %rp,-20(%sp) or stw %rp,-24(%sp) */
      if (insn == 0x6bc23fd9 || insn == 0x6bc23fd1)
	{
	  /* This instruction saves the return pointer (%rp) into the
	     "Current RP" slot (or the "External/Stub RP" slot for BSD
	     system call stubs) in the in the frame marker.  */
	  cache->saved_regs[HPPA_RP_REGNUM].addr = X_DISP14 (insn);
	  start_pc = pc + 4;
	}

      /* copy %r3,%r1 */
      else if (insn == 0x08030241)
	{
	  /* We've found the instruction that saves the frame pointer
	     (%r3) into %r1.  */
	  cache->saved_regs[HPPA_R3_REGNUM].realreg = HPPA_R1_REGNUM;
	  start_pc = pc + 4;
	}

      /* copy %sp,%r3 */
      else if (insn == 0x081e0243)
	{
	  /* We've found the instruction that sets up the new frame
             pointer.  */
	  cache->saved_regs[HPPA_SP_REGNUM].realreg = HPPA_R3_REGNUM;
	  start_pc = pc + 4;
	}

      else if (X_OP (insn) == 0x1b && X_B (insn) == HPPA_SP_REGNUM
	       && X_R (insn) == HPPA_R1_REGNUM && X_DISP14 (insn) > 0)
	{
	  /* We've found the instruction that saves the old frame
	     pointer (living in %r1) onto the stack.  */
	  cache->saved_regs[HPPA_R3_REGNUM].addr = 0;
	  start_pc = pc + 4;

	  /* Only set the frame size of we don't have a frame pointer.  */
	  if (cache->saved_regs[HPPA_SP_REGNUM].realreg != HPPA_R3_REGNUM)
	    cache->frame_size = X_DISP14 (insn);

	  /* The frame is fully set up now.  */
	  return pc + 4;
	}

      /* stwm r,d(%sp) */
      else if (X_OP (insn) == 0x1b && X_B (insn) == HPPA_SP_REGNUM
	       && X_R (insn) >= HPPA_R3_REGNUM
	       && X_R (insn) <= HPPA_R18_REGNUM
	       && X_DISP14 (insn) > 0)
	{
	  /* stwm %r1,d(%sp) */
	  if (X_R (insn) == HPPA_R1_REGNUM
	      && cache->saved_regs[HPPA_R3_REGNUM].realreg == HPPA_R1_REGNUM)
	    cache->saved_regs[HPPA_R3_REGNUM].addr = 0;
	  else
	    cache->saved_regs[X_R (insn)].addr = 0;

	  /* Only set the frame size of we don't have a frame pointer.  */
	  if (cache->saved_regs[HPPA_SP_REGNUM].realreg != HPPA_R3_REGNUM)
	    cache->frame_size = X_DISP14 (insn);

	  /* The frame is fully set up now.  */
	  return pc + 4;
	}

      /* ldo d(%sp),%sp */
      else if (X_OP (insn) == 0x0d && X_B (insn) == HPPA_SP_REGNUM
	       && X_T (insn) == HPPA_SP_REGNUM && X_DISP14 (insn) > 0)
	{
	  cache->frame_size = X_DISP14 (insn);

	  /* Only set the frame size of we don't have a frame pointer.  */
	  if (cache->saved_regs[HPPA_SP_REGNUM].realreg != HPPA_R3_REGNUM)
	    cache->frame_size = X_DISP14 (insn);

	  /* The frame is fully set up now.  */
	  return pc + 4;
	}
    }

  return start_pc;
}

static CORE_ADDR
hppa32_skip_prologue (CORE_ADDR start_pc)
{
  struct symtab_and_line sal;
  CORE_ADDR func_start, func_end;

  /* This is the preferred method, find the end of the prologue by
     using the debugging information.  */
  if (find_pc_partial_function (start_pc, NULL, &func_start, &func_end))
    {
      sal = find_pc_line (func_start, 0);

      if (sal.end < func_end
	  && start_pc <= sal.end)
	return sal.end;
    }

  /* Analyze the prologue.  */
  return hppa32_analyze_prologue (start_pc, (CORE_ADDR) -1, NULL);
}

static struct hppa_frame_cache *
hppa32_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct hppa_frame_cache *cache;
  int regnum;

  if (*this_cache)
    return *this_cache;

  /* Allocate a new cache.  */
  cache = FRAME_OBSTACK_ZALLOC (struct hppa_frame_cache);
  cache->saved_regs = trad_frame_alloc_saved_regs (next_frame);
  cache->frame_size = 0;

  cache->pc = frame_func_unwind (next_frame);
  if (cache->pc != 0)
    {
      CORE_ADDR addr_in_block = frame_unwind_address_in_block (next_frame);
      hppa32_analyze_prologue (cache->pc, addr_in_block, cache);
    }

  /* Calculate this frame's base.  */
  gdb_assert (trad_frame_realreg_p (cache->saved_regs, HPPA_SP_REGNUM));
  regnum = cache->saved_regs[HPPA_SP_REGNUM].realreg;
  cache->base = frame_unwind_register_unsigned (next_frame, regnum);
  if (cache->frame_size > 0)
    {
      cache->base -= cache->frame_size;
      trad_frame_set_value (cache->saved_regs, HPPA_SP_REGNUM, cache->base);
    }

  for (regnum = HPPA_R1_REGNUM;  regnum < HPPA32_NUM_REGS; regnum++)
    {
      if (trad_frame_addr_p (cache->saved_regs, regnum))
	cache->saved_regs[regnum].addr += cache->base;
    }

  /* Identify the head of the program counter offset queue (%pcoqh)
     with the return pointer (%rp).  */
  cache->saved_regs[HPPA_PCOQ_HEAD_REGNUM] = cache->saved_regs[HPPA_RP_REGNUM];

  *this_cache = cache;
  return cache;
}

static void
hppa32_frame_this_id (struct frame_info *next_frame, void **this_cache,
		      struct frame_id *this_id)
{
  struct hppa_frame_cache *cache =
    hppa32_frame_cache (next_frame, this_cache);

  /* This marks the outermost frame.  */
  if (cache->base == 0)
    return;

  (*this_id) = frame_id_build (cache->base, cache->pc);
}

static void
hppa32_frame_prev_register (struct frame_info *next_frame, void **this_cache,
			    int regnum, int *optimizedp,
			    enum lval_type *lvalp, CORE_ADDR *addrp,
			    int *realnump, void *valuep)
{
  struct hppa_frame_cache *cache =
    hppa32_frame_cache (next_frame, this_cache);

  if (regnum == HPPA_PCOQ_TAIL_REGNUM)
    {
      if (valuep)
	{
	  CORE_ADDR pc;

	  /* Fetch the head of the program counter offset queue
             (%pcoqh).  */
	  trad_frame_prev_register (next_frame, cache->saved_regs,
				    HPPA_PCOQ_HEAD_REGNUM, optimizedp,
				    lvalp, addrp, realnump, valuep);

	  /* Now compute its tail (%pcoqt) by adding four bytes such
             that it points at the next instruction.  */
	  pc = extract_unsigned_integer (valuep, 4);
	  store_unsigned_integer (valuep, 4, pc + 4);
	}

      /* It's a computed value.  */
      *optimizedp = 0;
      *lvalp = not_lval;
      *addrp = 0;
      *realnump = -1;
      return;
    }

  trad_frame_prev_register (next_frame, cache->saved_regs, regnum,
			    optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind hppa32_frame_unwind =
{
  NORMAL_FRAME,
  hppa32_frame_this_id,
  hppa32_frame_prev_register
};

static const struct frame_unwind *
hppa32_frame_sniffer (struct frame_info *next_frame)
{
  return &hppa32_frame_unwind;
}


static CORE_ADDR
hppa32_frame_base_address (struct frame_info *next_frame, void **this_cache)
{
  struct hppa_frame_cache *cache =
    hppa32_frame_cache (next_frame, this_cache);

  return cache->base;
}

static const struct frame_base hppa32_frame_base =
{
  &hppa32_frame_unwind,
  hppa32_frame_base_address,
  hppa32_frame_base_address,
  hppa32_frame_base_address
};


static void
hppa_write_pc (CORE_ADDR pc, ptid_t ptid)
{
  write_register_pid (HPPA_PCOQ_HEAD_REGNUM, pc, ptid);
  write_register_pid (HPPA_PCOQ_TAIL_REGNUM, pc + 4, ptid);
}


static CORE_ADDR
hppa_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  CORE_ADDR pc;

  pc = frame_unwind_register_unsigned (next_frame, HPPA_PCOQ_HEAD_REGNUM);
  return hppa_addr_bits_remove (pc);
}


static struct gdbarch *
hppa32_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch_tdep *tdep;
  struct gdbarch *gdbarch;

  /* If there is already a candidate, use it.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

  /* Allocate space for the new architecture.  */
  tdep = XMALLOC (struct gdbarch_tdep);
  gdbarch = gdbarch_alloc (&info, tdep);

  set_gdbarch_long_double_bit (gdbarch, 128);
  set_gdbarch_long_double_format (gdbarch, &floatformat_hppa_quad);

  set_gdbarch_num_regs (gdbarch, HPPA32_NUM_REGS);
  set_gdbarch_register_name (gdbarch, hppa32_register_name);
  set_gdbarch_register_type (gdbarch, hppa32_register_type);

  /* Register numbers of various important registers.  */
  set_gdbarch_sp_regnum (gdbarch, HPPA_SP_REGNUM); /* %sp */
  set_gdbarch_pc_regnum (gdbarch, HPPA_PCOQ_HEAD_REGNUM); /* %pc */

  set_gdbarch_addr_bits_remove (gdbarch, hppa_addr_bits_remove);

  set_gdbarch_skip_prologue (gdbarch, hppa32_skip_prologue);

  /* Stack grows upward.  */
  set_gdbarch_inner_than (gdbarch, core_addr_greaterthan);

  set_gdbarch_breakpoint_from_pc (gdbarch, hppa_breakpoint_from_pc);

  set_gdbarch_print_insn (gdbarch, print_insn_hppa);

  set_gdbarch_unwind_pc (gdbarch, hppa_unwind_pc);
  set_gdbarch_write_pc (gdbarch, hppa_write_pc);

  frame_base_set_default (gdbarch, &hppa32_frame_base);

  /* Hook in ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);

  frame_unwind_append_sniffer (gdbarch, hppa32_frame_sniffer);

  return gdbarch;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_hppa_tdep (void);

void
_initialize_hppa_tdep (void)
{
  register_gdbarch_init (bfd_arch_hppa, hppa32_gdbarch_init);
}

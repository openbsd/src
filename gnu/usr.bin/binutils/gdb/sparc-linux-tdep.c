/* Target-dependent code for GNU/Linux SPARC.

   Copyright 2003 Free Software Foundation, Inc.

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
#include "floatformat.h"
#include "frame.h"
#include "frame-unwind.h"
#include "gdbarch.h"
#include "gdbcore.h"
#include "osabi.h"
#include "regcache.h"
#include "solib-svr4.h"
#include "symtab.h"
#include "trad-frame.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "sparc-tdep.h"

/* Recognizing signal handler frames.  */

/* GNU/Linux has two flavors of signals.  Normal signal handlers, and
   "realtime" (RT) signals.  The RT signals can provide additional
   information to the signal handler if the SA_SIGINFO flag is set
   when establishing a signal handler using `sigaction'.  It is not
   unlikely that future versions of GNU/Linux will support SA_SIGINFO
   for normal signals too.  */

/* When the sparc Linux kernel calls a signal handler and the
   SA_RESTORER flag isn't set, the return address points to a bit of
   code on the stack.  This function returns whether the PC appears to
   be within this bit of code.

   The instruction sequence for normal signals is
	mov __NR_sigreturn, %g1		! hex: 0x821020d8
	ta  0x10			! hex: 0x91d02010

   Checking for the code sequence should be somewhat reliable, because
   the effect is to call the system call sigreturn.  This is unlikely
   to occur anywhere other than a signal trampoline.

   It kind of sucks that we have to read memory from the process in
   order to identify a signal trampoline, but there doesn't seem to be
   any other way.  However, sparc32_linux_pc_in_sigtramp arranges to
   only call us if no function name could be identified, which should
   be the case since the code is on the stack.  */

#define LINUX32_SIGTRAMP_INSN0	0x821020d8	/* mov __NR_sigreturn, %g1 */
#define LINUX32_SIGTRAMP_INSN1	0x91d02010	/* ta  0x10 */

/* The instruction sequence for RT signals is
       mov __NR_rt_sigreturn, %g1	! hex: 0x82102065
       ta  {0x10,0x6d}			! hex: 0x91d02010 or 0x91d0206d

   The effect is to call the system call rt_sigreturn.  The trap number
   is variable based upon whether this is a 32-bit or 64-bit sparc binary.
   Note that 64-bit binaries only use this RT signal return method.  */

#define LINUX32_RT_SIGTRAMP_INSN0	0x82102065
#define LINUX32_RT_SIGTRAMP_INSN1	0x91d02010

/* If PC is in a sigtramp routine consisting of the instructions INSN0
   and INSN1, return the address of the start of the routine.
   Otherwise, return 0.  */

CORE_ADDR
sparc_linux_sigtramp_start (CORE_ADDR pc, ULONGEST insn0, ULONGEST insn1)
{
  ULONGEST word0, word1;
  char buf[8];			/* Two instructions.  */

  /* We only recognize a signal trampoline if PC is at the start of
     one of the instructions.  We optimize for finding the PC at the
     start of the instruction sequence, as will be the case when the
     trampoline is not the first frame on the stack.  We assume that
     in the case where the PC is not at the start of the instruction
     sequence, there will be a few trailing readable bytes on the
     stack.  */

  if (read_memory_nobpt (pc, buf, sizeof buf) != 0)
    return 0;

  word0 = extract_unsigned_integer (buf, 4);
  if (word0 != insn0)
    {
      if (word0 != insn1)
	return 0;

      pc -= 4;
      if (read_memory_nobpt (pc, buf, sizeof buf) != 0)
	return 0;

      word0 = extract_unsigned_integer (buf, 4);
    }

  word1 = extract_unsigned_integer (buf + 4, 4);
  if (word0 != insn0 || word1 != insn1)
    return 0;

  return pc;
}

static CORE_ADDR
sparc32_linux_sigtramp_start (CORE_ADDR pc)
{
  return sparc_linux_sigtramp_start (pc, LINUX32_SIGTRAMP_INSN0,
				     LINUX32_SIGTRAMP_INSN1);
}

static CORE_ADDR
sparc32_linux_rt_sigtramp_start (CORE_ADDR pc)
{
  return sparc_linux_sigtramp_start (pc, LINUX32_RT_SIGTRAMP_INSN0,
				     LINUX32_RT_SIGTRAMP_INSN1);
}

static int
sparc32_linux_pc_in_sigtramp (CORE_ADDR pc, char *name)
{
  /* If we have NAME, we can optimize the search.  The trampolines are
     named __restore and __restore_rt.  However, they aren't dynamically
     exported from the shared C library, so the trampoline may appear to
     be part of the preceding function.  This should always be sigaction,
     __sigaction, or __libc_sigaction (all aliases to the same function).  */
  if (name == NULL || strstr (name, "sigaction") != NULL)
    return (sparc32_linux_sigtramp_start (pc) != 0
	    || sparc32_linux_rt_sigtramp_start (pc) != 0);

  return (strcmp ("__restore", name) == 0
	  || strcmp ("__restore_rt", name) == 0);
}

static struct sparc_frame_cache *
sparc32_linux_sigtramp_frame_cache (struct frame_info *next_frame,
				    void **this_cache)
{
  struct sparc_frame_cache *cache;
  CORE_ADDR sigcontext_addr, addr;
  int regnum;

  if (*this_cache)
    return *this_cache;

  cache = sparc32_frame_cache (next_frame, this_cache);
  gdb_assert (cache == *this_cache);

  /* ??? What about signal trampolines that aren't frameless?  */
  regnum = SPARC_SP_REGNUM;
  cache->base = frame_unwind_register_unsigned (next_frame, regnum);

  regnum = SPARC_O1_REGNUM;
  sigcontext_addr = frame_unwind_register_unsigned (next_frame, regnum);

  cache->pc = frame_pc_unwind (next_frame);
  addr = sparc32_linux_sigtramp_start (cache->pc);
  if (addr == 0)
    {
      /* If this is a RT signal trampoline, adjust SIGCONTEXT_ADDR
         accordingly.  */
      addr = sparc32_linux_rt_sigtramp_start (cache->pc);
      if (addr)
	sigcontext_addr += 128;
      else
	addr = frame_func_unwind (next_frame);
    }
  cache->pc = addr;

  cache->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  cache->saved_regs[SPARC32_PSR_REGNUM].addr = sigcontext_addr + 0;
  cache->saved_regs[SPARC32_PC_REGNUM].addr = sigcontext_addr + 4;
  cache->saved_regs[SPARC32_NPC_REGNUM].addr = sigcontext_addr + 8;
  cache->saved_regs[SPARC32_Y_REGNUM].addr = sigcontext_addr + 12;

  /* Since %g0 is always zero, keep the identity encoding.  */
  for (regnum = SPARC_G1_REGNUM, addr = sigcontext_addr + 20;
       regnum <= SPARC_O7_REGNUM; regnum++, addr += 4)
    cache->saved_regs[regnum].addr = addr;

  for (regnum = SPARC_L0_REGNUM, addr = cache->base;
       regnum <= SPARC_I7_REGNUM; regnum++, addr += 4)
    cache->saved_regs[regnum].addr = addr;

  return cache;
}

static void
sparc32_linux_sigtramp_frame_this_id (struct frame_info *next_frame,
				      void **this_cache,
				      struct frame_id *this_id)
{
  struct sparc_frame_cache *cache =
    sparc32_linux_sigtramp_frame_cache (next_frame, this_cache);

  (*this_id) = frame_id_build (cache->base, cache->pc);
}

static void
sparc32_linux_sigtramp_frame_prev_register (struct frame_info *next_frame,
					    void **this_cache,
					    int regnum, int *optimizedp,
					    enum lval_type *lvalp,
					    CORE_ADDR *addrp,
					    int *realnump, void *valuep)
{
  struct sparc_frame_cache *cache =
    sparc32_linux_sigtramp_frame_cache (next_frame, this_cache);

  trad_frame_prev_register (next_frame, cache->saved_regs, regnum,
			    optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind sparc32_linux_sigtramp_frame_unwind =
{
  SIGTRAMP_FRAME,
  sparc32_linux_sigtramp_frame_this_id,
  sparc32_linux_sigtramp_frame_prev_register
};

static const struct frame_unwind *
sparc32_linux_sigtramp_frame_sniffer (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  char *name;

  find_pc_partial_function (pc, &name, NULL, NULL);
  if (sparc32_linux_pc_in_sigtramp (pc, name))
    return &sparc32_linux_sigtramp_frame_unwind;

  return NULL;
}


static struct link_map_offsets *
sparc32_linux_svr4_fetch_link_map_offsets (void)
{
  static struct link_map_offsets lmo;
  static struct link_map_offsets *lmp = NULL;

  if (lmp == NULL)
    {
      lmp = &lmo;

      /* Everything we need is in the first 8 bytes.  */
      lmo.r_debug_size = 8;
      lmo.r_map_offset = 4;
      lmo.r_map_size   = 4;

      /* Everything we need is in the first 20 bytes.  */
      lmo.link_map_size = 20;
      lmo.l_addr_offset = 0;
      lmo.l_addr_size   = 4;
      lmo.l_name_offset = 4;
      lmo.l_name_size   = 4;
      lmo.l_next_offset = 12;
      lmo.l_next_size   = 4;
      lmo.l_prev_offset = 16;
      lmo.l_prev_size   = 4;
    }

  return lmp;
}

static void
sparc32_linux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* GNU/Linux is very similar to Solaris ...  */
  sparc32_sol2_init_abi (info, gdbarch);

  /* ... but doesn't have kernel-assisted single-stepping support.  */
  set_gdbarch_software_single_step (gdbarch, sparc_software_single_step);

  /* GNU/Linux doesn't support the 128-bit `long double' from the psABI.  */
  set_gdbarch_long_double_bit (gdbarch, 64);
  set_gdbarch_long_double_format (gdbarch, &floatformat_ieee_double_big);

  set_gdbarch_pc_in_sigtramp (gdbarch, sparc32_linux_pc_in_sigtramp);
  frame_unwind_append_sniffer (gdbarch, sparc32_linux_sigtramp_frame_sniffer);

  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, sparc32_linux_svr4_fetch_link_map_offsets);
}

/* Provide a prototype to silence -Wmissing-prototypes.  */
extern void _initialize_sparc_linux_tdep (void);

void
_initialize_sparc_linux_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_sparc, 0, GDB_OSABI_LINUX,
			  sparc32_linux_init_abi);
}

/* Target-dependent code for OpenBSD/sparc64.

   Copyright 2004, 2005, 2006 Free Software Foundation, Inc.

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
#include "frame-unwind.h"
#include "osabi.h"
#include "regset.h"
#include "symtab.h"
#include "objfiles.h"
#include "solib-svr4.h"
#include "trad-frame.h"

#include "gdb_assert.h"

#include "obsd-tdep.h"
#include "sparc64-tdep.h"

/* OpenBSD uses the traditional NetBSD core file format, even for
   ports that use ELF.  The core files don't use multiple register
   sets.  Instead, the general-purpose and floating-point registers
   are lumped together in a single section.  Unlike on NetBSD, OpenBSD
   uses a different layout for its general-purpose registers than the
   layout used for ptrace(2).  */

/* From <machine/reg.h>.  */
const struct sparc_gregset sparc64obsd_core_gregset =
{
  0 * 8,			/* "tstate" */
  1 * 8,			/* %pc */
  2 * 8,			/* %npc */
  3 * 8,			/* %y */
  -1,				/* %fprs */
  -1,
  7 * 8,			/* %g1 */
  22 * 8,			/* %l0 */
  4				/* sizeof (%y) */
};

static void
sparc64obsd_supply_gregset (const struct regset *regset,
			    struct regcache *regcache,
			    int regnum, const void *gregs, size_t len)
{
  const char *regs = gregs;

  sparc64_supply_gregset (&sparc64obsd_core_gregset, regcache, regnum, regs);
  sparc64_supply_fpregset (regcache, regnum, regs + 288);
}


/* Signal trampolines.  */

/* The OpenBSD kernel maps the signal trampoline at some random
   location in user space, which means that the traditional BSD way of
   detecting it won't work.

   The signal trampoline will be mapped at an address that is page
   aligned.  We recognize the signal trampoline by the looking for the
   sigreturn system call.  The offset where we can find the code that
   makes this system call varies from release to release.  For OpenBSD
   3.6 and later releases we can find the code at offset 0xec.  For
   OpenBSD 3.5 and earlier releases, we find it at offset 0xe8.  */

static const int sparc64obsd_page_size = 8192;
static const int sparc64obsd_sigreturn_offset[] = { 0xf0, 0xec, 0xe8, -1 };

static int
sparc64obsd_pc_in_sigtramp (CORE_ADDR pc, char *name)
{
  CORE_ADDR start_pc = (pc & ~(sparc64obsd_page_size - 1));
  unsigned long insn;
  const int *offset;

  if (name)
    return 0;

  for (offset = sparc64obsd_sigreturn_offset; *offset != -1; offset++)
    {
      /* Check for "restore %g0, SYS_sigreturn, %g1".  */
      insn = sparc_fetch_instruction (start_pc + *offset);
      if (insn != 0x83e82067)
	continue;

      /* Check for "t ST_SYSCALL".  */
      insn = sparc_fetch_instruction (start_pc + *offset + 8);
      if (insn != 0x91d02000)
	continue;

      return 1;
    }

  return 0;
}

static struct sparc_frame_cache *
sparc64obsd_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct sparc_frame_cache *cache;
  CORE_ADDR addr;

  if (*this_cache)
    return *this_cache;

  cache = sparc_frame_cache (next_frame, this_cache);
  gdb_assert (cache == *this_cache);

  /* If we couldn't find the frame's function, we're probably dealing
     with an on-stack signal trampoline.  */
  if (cache->pc == 0)
    {
      cache->pc = frame_pc_unwind (next_frame);
      cache->pc &= ~(sparc64obsd_page_size - 1);

      /* Since we couldn't find the frame's function, the cache was
         initialized under the assumption that we're frameless.  */
      cache->frameless_p = 0;
      addr = frame_unwind_register_unsigned (next_frame, SPARC_FP_REGNUM);
      cache->base = addr;
    }

  /* We find the appropriate instance of `struct sigcontext' at a
     fixed offset in the signal frame.  */
  addr = cache->base + BIAS + 128 + 16;
  cache->saved_regs = sparc64nbsd_sigcontext_saved_regs (addr, next_frame);

  return cache;
}

static void
sparc64obsd_frame_this_id (struct frame_info *next_frame, void **this_cache,
			   struct frame_id *this_id)
{
  struct sparc_frame_cache *cache =
    sparc64obsd_frame_cache (next_frame, this_cache);

  (*this_id) = frame_id_build (cache->base, cache->pc);
}

static void
sparc64obsd_frame_prev_register (struct frame_info *next_frame,
				 void **this_cache,
				 int regnum, int *optimizedp,
				 enum lval_type *lvalp, CORE_ADDR *addrp,
				 int *realnump, void *valuep)
{
  struct sparc_frame_cache *cache =
    sparc64obsd_frame_cache (next_frame, this_cache);

  trad_frame_get_prev_register (next_frame, cache->saved_regs, regnum,
				optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind sparc64obsd_frame_unwind =
{
  SIGTRAMP_FRAME,
  sparc64obsd_frame_this_id,
  sparc64obsd_frame_prev_register
};

static const struct frame_unwind *
sparc64obsd_sigtramp_frame_sniffer (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  char *name;

  find_pc_partial_function (pc, &name, NULL, NULL);
  if (sparc64obsd_pc_in_sigtramp (pc, name))
    return &sparc64obsd_frame_unwind;

  return NULL;
}

/* Kernel debugging support.  */

static struct sparc_frame_cache *
sparc64obsd_trapframe_cache(struct frame_info *next_frame, void **this_cache)
{
  struct sparc_frame_cache *cache;
  CORE_ADDR sp, trapframe_addr;
  int regnum;

  if (*this_cache)
    return *this_cache;

  cache = sparc_frame_cache (next_frame, this_cache);
  gdb_assert (cache == *this_cache);

  sp = frame_unwind_register_unsigned (next_frame, SPARC_SP_REGNUM);
  trapframe_addr = sp + BIAS + 176;

  cache->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  cache->saved_regs[SPARC64_STATE_REGNUM].addr = trapframe_addr;
  cache->saved_regs[SPARC64_PC_REGNUM].addr = trapframe_addr + 8;
  cache->saved_regs[SPARC64_NPC_REGNUM].addr = trapframe_addr + 16;

  for (regnum = SPARC_G0_REGNUM; regnum <= SPARC_I7_REGNUM; regnum++)
    cache->saved_regs[regnum].addr =
      trapframe_addr + 48 + (regnum - SPARC_G0_REGNUM) * 8;

  return cache;
}

static void
sparc64obsd_trapframe_this_id (struct frame_info *next_frame,
			       void **this_cache, struct frame_id *this_id)
{
  struct sparc_frame_cache *cache =
    sparc64obsd_trapframe_cache (next_frame, this_cache);

  (*this_id) = frame_id_build (cache->base, cache->pc);
}

static void
sparc64obsd_trapframe_prev_register (struct frame_info *next_frame,
				     void **this_cache,
				     int regnum, int *optimizedp,
				     enum lval_type *lvalp, CORE_ADDR *addrp,
				     int *realnump, void *valuep)
{
  struct sparc_frame_cache *cache =
    sparc64obsd_trapframe_cache (next_frame, this_cache);

  trad_frame_get_prev_register (next_frame, cache->saved_regs, regnum,
				optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind sparc64obsd_trapframe_unwind =
{
  NORMAL_FRAME,
  sparc64obsd_trapframe_this_id,
  sparc64obsd_trapframe_prev_register
};

static const struct frame_unwind *
sparc64obsd_trapframe_sniffer (struct frame_info *next_frame)
{
  ULONGEST pstate;
  char *name;

  /* Check whether we are in privileged mode, and bail out if we're not.  */
  pstate = frame_unwind_register_unsigned (next_frame, SPARC64_PSTATE_REGNUM);
  if ((pstate & SPARC64_PSTATE_PRIV) == 0)
    return NULL;

  find_pc_partial_function (frame_pc_unwind (next_frame), &name, NULL, NULL);
  if (name && ((strcmp (name, "Lslowtrap_reenter") == 0)
	       || (strcmp (name, "Ldatafault_internal") == 0)))
    return &sparc64obsd_trapframe_unwind;

  return NULL;
}


static void
sparc64obsd_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->gregset = regset_alloc (gdbarch, sparc64obsd_supply_gregset, NULL);
  tdep->sizeof_gregset = 832;

  frame_unwind_append_sniffer (gdbarch, sparc64obsd_sigtramp_frame_sniffer);
  frame_unwind_append_sniffer (gdbarch, sparc64obsd_trapframe_sniffer);

  sparc64_init_abi (info, gdbarch);

  /* OpenBSD/sparc64 has SVR4-style shared libraries...  */
  set_gdbarch_in_solib_call_trampoline (gdbarch, in_plt_section);
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_lp64_fetch_link_map_offsets);
  set_gdbarch_skip_solib_resolver (gdbarch, obsd_skip_solib_resolver);
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_sparc64obsd_tdep (void);

void
_initialize_sparc64obsd_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_sparc, bfd_mach_sparc_v9,
			  GDB_OSABI_OPENBSD_ELF, sparc64obsd_init_abi);
}

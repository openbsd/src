/* Target-dependent code for OpenBSD/powerpc64.

   Copyright (C) 2004, 2005, 2006 Free Software Foundation, Inc.

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#include "defs.h"
#include "arch-utils.h"
#include "floatformat.h"
#include "frame.h"
#include "frame-unwind.h"
#include "osabi.h"
#include "regcache.h"
#include "regset.h"
#include "symtab.h"
#include "trad-frame.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "ppc-tdep.h"
#include "ppc64obsd-tdep.h"
#include "solib-svr4.h"

/* Register offsets from <machine/reg.h>.  */
struct ppc_reg_offsets ppc64obsd_reg_offsets;
struct ppc_reg_offsets ppc64obsd_fpreg_offsets;


/* Register set support functions.  */

static void
ppc64_supply_reg (struct regcache *regcache, int regnum, 
		  const char *regs, size_t offset)
{
  if (regnum != -1 && offset != -1)
    regcache_raw_supply (regcache, regnum, regs + offset);
}

static void
ppc64_collect_reg (const struct regcache *regcache, int regnum,
		   char *regs, size_t offset)
{
  if (regnum != -1 && offset != -1)
    regcache_raw_collect (regcache, regnum, regs + offset);
}
    
/* Supply register REGNUM in the general-purpose register set REGSET
   from the buffer specified by GREGS and LEN to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

void
ppc64_supply_gregset (const struct regset *regset, struct regcache *regcache,
		      int regnum, const void *gregs, size_t len)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  const struct ppc_reg_offsets *offsets = regset->descr;
  size_t offset;
  int i;

  for (i = tdep->ppc_gp0_regnum, offset = offsets->r0_offset;
       i < tdep->ppc_gp0_regnum + ppc_num_gprs;
       i++, offset += 8)
    {
      if (regnum == -1 || regnum == i)
	ppc64_supply_reg (regcache, i, gregs, offset);
    }

  if (regnum == -1 || regnum == PC_REGNUM)
    ppc64_supply_reg (regcache, PC_REGNUM, gregs, offsets->pc_offset);
  if (regnum == -1 || regnum == tdep->ppc_ps_regnum)
    ppc64_supply_reg (regcache, tdep->ppc_ps_regnum,
		      gregs, offsets->ps_offset);
  if (regnum == -1 || regnum == tdep->ppc_cr_regnum)
    ppc64_supply_reg (regcache, tdep->ppc_cr_regnum,
		      gregs, offsets->cr_offset);
  if (regnum == -1 || regnum == tdep->ppc_lr_regnum)
    ppc64_supply_reg (regcache, tdep->ppc_lr_regnum,
		      gregs, offsets->lr_offset);
  if (regnum == -1 || regnum == tdep->ppc_ctr_regnum)
    ppc64_supply_reg (regcache, tdep->ppc_ctr_regnum,
		      gregs, offsets->ctr_offset);
  if (regnum == -1 || regnum == tdep->ppc_xer_regnum)
    ppc64_supply_reg (regcache, tdep->ppc_xer_regnum,
		      gregs, offsets->cr_offset);
}

/* Collect register REGNUM in the general-purpose register set
   REGSET. from register cache REGCACHE into the buffer specified by
   GREGS and LEN.  If REGNUM is -1, do this for all registers in
   REGSET.  */

void
ppc64_collect_gregset (const struct regset *regset,
		       const struct regcache *regcache,
		       int regnum, void *gregs, size_t len)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  const struct ppc_reg_offsets *offsets = regset->descr;
  size_t offset;
  int i;

  offset = offsets->r0_offset;
  for (i = tdep->ppc_gp0_regnum;
       i < tdep->ppc_gp0_regnum + ppc_num_gprs;
       i++, offset += 8)
    {
      if (regnum == -1 || regnum == i)
	ppc64_collect_reg (regcache, i, gregs, offset);
    }

  if (regnum == -1 || regnum == PC_REGNUM)
    ppc64_collect_reg (regcache, PC_REGNUM, gregs, offsets->pc_offset);
  if (regnum == -1 || regnum == tdep->ppc_ps_regnum)
    ppc64_collect_reg (regcache, tdep->ppc_ps_regnum,
		       gregs, offsets->ps_offset);
  if (regnum == -1 || regnum == tdep->ppc_cr_regnum)
    ppc64_collect_reg (regcache, tdep->ppc_cr_regnum,
		       gregs, offsets->cr_offset);
  if (regnum == -1 || regnum == tdep->ppc_lr_regnum)
    ppc64_collect_reg (regcache, tdep->ppc_lr_regnum,
		       gregs, offsets->lr_offset);
  if (regnum == -1 || regnum == tdep->ppc_ctr_regnum)
    ppc64_collect_reg (regcache, tdep->ppc_ctr_regnum,
		       gregs, offsets->ctr_offset);
  if (regnum == -1 || regnum == tdep->ppc_xer_regnum)
    ppc64_collect_reg (regcache, tdep->ppc_xer_regnum,
		       gregs, offsets->xer_offset);
}

/* Core file support.  */

/* Supply register REGNUM in the general-purpose register set REGSET
   from the buffer specified by GREGS and LEN to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

void
ppc64obsd_supply_gregset (const struct regset *regset,
			  struct regcache *regcache, int regnum,
			  const void *gregs, size_t len)
{
  ppc64_supply_gregset (regset, regcache, regnum, gregs, len);
}

/* Collect register REGNUM in the general-purpose register set
   REGSET. from register cache REGCACHE into the buffer specified by
   GREGS and LEN.  If REGNUM is -1, do this for all registers in
   REGSET.  */

void
ppc64obsd_collect_gregset (const struct regset *regset,
			   const struct regcache *regcache, int regnum,
			   void *gregs, size_t len)
{
  ppc64_collect_gregset (regset, regcache, regnum, gregs, len);
}

/* OpenBSD/powerpc register set.  */

struct regset ppc64obsd_gregset =
{
  &ppc64obsd_reg_offsets,
  ppc64obsd_supply_gregset
};

struct regset ppc64obsd_fpregset =
{
  &ppc64obsd_fpreg_offsets,
  ppc_supply_fpregset
};

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

static const struct regset *
ppc64obsd_regset_from_core_section (struct gdbarch *gdbarch,
				  const char *sect_name, size_t sect_size)
{
  if (strcmp (sect_name, ".reg") == 0 && sect_size >= 304)
    return &ppc64obsd_gregset;

  if (strcmp (sect_name, ".reg2") == 0 && sect_size >= 1048)
    return &ppc64obsd_fpregset;

  return NULL;
}


/* Signal trampolines.  */

/* Since OpenBSD 3.2, the sigtramp routine is mapped at a random page
   in virtual memory.  The randomness makes it somewhat tricky to
   detect it, but fortunately we can rely on the fact that the start
   of the sigtramp routine is page-aligned.  We recognize the
   trampoline by looking for the code that invokes the sigreturn
   system call.  The offset where we can find that code varies from
   release to release.

   By the way, the mapping mentioned above is read-only, so you cannot
   place a breakpoint in the signal trampoline.  */

/* Default page size.  */
static const int ppc64obsd_page_size = 4096;

/* Offset for sigreturn(2).  */
static const int ppc64obsd_sigreturn_offset[] = {
  0x98,				/* OpenBSD 3.8 */
  0x0c,				/* OpenBSD 3.2 */
  -1
};

static int
ppc64obsd_sigtramp_p (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  CORE_ADDR start_pc = (pc & ~(ppc64obsd_page_size - 1));
  const int *offset;
  char *name;

  find_pc_partial_function (pc, &name, NULL, NULL);
  if (name)
    return 0;

  for (offset = ppc64obsd_sigreturn_offset; *offset != -1; offset++)
    {
      char buf[2 * PPC_INSN_SIZE];
      unsigned long insn;

      if (!safe_frame_unwind_memory (next_frame, start_pc + *offset,
				     buf, sizeof buf))
	continue;

      /* Check for "li r0,SYS_sigreturn".  */
      insn = extract_unsigned_integer (buf, PPC_INSN_SIZE);
      if (insn != 0x38000067)
	continue;

      /* Check for "sc".  */
      insn = extract_unsigned_integer (buf + PPC_INSN_SIZE, PPC_INSN_SIZE);
      if (insn != 0x44000002)
	continue;

      return 1;
    }

  return 0;
}

static struct trad_frame_cache *
ppc64obsd_sigtramp_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  struct trad_frame_cache *cache;
  CORE_ADDR addr, base, func;
  char buf[PPC_INSN_SIZE];
  unsigned long insn, sigcontext_offset;
  int i;

  if (*this_cache)
    return *this_cache;

  cache = trad_frame_cache_zalloc (next_frame);
  *this_cache = cache;

  func = frame_pc_unwind (next_frame);
  func &= ~(ppc64obsd_page_size - 1);
  if (!safe_frame_unwind_memory (next_frame, func, buf, sizeof buf))
    return cache;

  /* Calculate the offset where we can find `struct sigcontext'.  We
     base our calculation on the amount of stack space reserved by the
     first instruction of the signal trampoline.  */
  insn = extract_unsigned_integer (buf, PPC_INSN_SIZE);
  sigcontext_offset = (0x10000 - (insn & 0x0000ffff)) + 8;

  base = frame_unwind_register_unsigned (next_frame, SP_REGNUM);
  addr = base + sigcontext_offset + 2 * tdep->wordsize;
  for (i = 0; i < ppc_num_gprs; i++, addr += tdep->wordsize)
    {
      int regnum = i + tdep->ppc_gp0_regnum;
      trad_frame_set_reg_addr (cache, regnum, addr);
    }
  trad_frame_set_reg_addr (cache, tdep->ppc_lr_regnum, addr);
  addr += tdep->wordsize;
  trad_frame_set_reg_addr (cache, tdep->ppc_cr_regnum, addr);
  addr += tdep->wordsize;
  trad_frame_set_reg_addr (cache, tdep->ppc_xer_regnum, addr);
  addr += tdep->wordsize;
  trad_frame_set_reg_addr (cache, tdep->ppc_ctr_regnum, addr);
  addr += tdep->wordsize;
  trad_frame_set_reg_addr (cache, PC_REGNUM, addr); /* SRR0? */
  addr += tdep->wordsize;

  /* Construct the frame ID using the function start.  */
  trad_frame_set_id (cache, frame_id_build (base, func));

  return cache;
}

static void
ppc64obsd_sigtramp_frame_this_id (struct frame_info *next_frame,
				void **this_cache, struct frame_id *this_id)
{
  struct trad_frame_cache *cache =
    ppc64obsd_sigtramp_frame_cache (next_frame, this_cache);

  trad_frame_get_id (cache, this_id);
}

static void
ppc64obsd_sigtramp_frame_prev_register (struct frame_info *next_frame,
				      void **this_cache, int regnum,
				      int *optimizedp, enum lval_type *lvalp,
				      CORE_ADDR *addrp, int *realnump,
				      char *valuep)
{
  struct trad_frame_cache *cache =
    ppc64obsd_sigtramp_frame_cache (next_frame, this_cache);

  trad_frame_get_register (cache, next_frame, regnum,
			   optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind ppc64obsd_sigtramp_frame_unwind = {
  SIGTRAMP_FRAME,
  ppc64obsd_sigtramp_frame_this_id,
  ppc64obsd_sigtramp_frame_prev_register
};

static const struct frame_unwind *
ppc64obsd_sigtramp_frame_sniffer (struct frame_info *next_frame)
{
  if (ppc64obsd_sigtramp_p (next_frame))
    return &ppc64obsd_sigtramp_frame_unwind;

  return NULL;
}


static void
ppc64obsd_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* OpenBSD doesn't support the 128-bit `long double' from the psABI.  */
  set_gdbarch_long_double_bit (gdbarch, 64);
  set_gdbarch_long_double_format (gdbarch, &floatformat_ieee_double_big);

  /* OpenBSD currently uses a broken GCC.  */
  set_gdbarch_return_value (gdbarch, ppc_sysv_abi_broken_return_value);

  /* OpenBSD uses SVR4-style shared libraries.  */
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_lp64_fetch_link_map_offsets);

  set_gdbarch_regset_from_core_section
    (gdbarch, ppc64obsd_regset_from_core_section);

  frame_unwind_append_sniffer (gdbarch, ppc64obsd_sigtramp_frame_sniffer);
}


/* OpenBSD uses uses the traditional NetBSD core file format, even for
   ports that use ELF.  */
#define GDB_OSABI_NETBSD_CORE GDB_OSABI_OPENBSD_ELF

static enum gdb_osabi
ppc64obsd_core_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "netbsd-core") == 0)
    return GDB_OSABI_NETBSD_CORE;

  return GDB_OSABI_UNKNOWN;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_ppc64obsd_tdep (void);

void
_initialize_ppc64obsd_tdep (void)
{
  /* BFD doesn't set a flavour for NetBSD style a.out core files.  */
  gdbarch_register_osabi_sniffer (bfd_arch_powerpc, bfd_target_unknown_flavour,
                                  ppc64obsd_core_osabi_sniffer);

#if 0
  gdbarch_register_osabi (bfd_arch_rs6000, 0, GDB_OSABI_OPENBSD_ELF,
			  ppc64obsd_init_abi);
#endif
  gdbarch_register_osabi (bfd_arch_powerpc, bfd_mach_ppc64,
			  GDB_OSABI_OPENBSD_ELF, ppc64obsd_init_abi);

  /* Avoid initializing the register offsets again if they were
     already initailized by ppc64obsd-nat.c.  */
  if (ppc64obsd_reg_offsets.pc_offset == 0)
    {
      /* General-purpose registers.  */
      ppc64obsd_reg_offsets.r0_offset = 0;
      ppc64obsd_reg_offsets.pc_offset = 288;
      ppc64obsd_reg_offsets.ps_offset = 296;
      ppc64obsd_reg_offsets.cr_offset = 264;
      ppc64obsd_reg_offsets.lr_offset = 256;
      ppc64obsd_reg_offsets.ctr_offset = 280;
      ppc64obsd_reg_offsets.xer_offset = 272;
      ppc64obsd_reg_offsets.mq_offset = -1;

      /* Floating-point registers.  */
      ppc64obsd_reg_offsets.f0_offset = -1;
      ppc64obsd_reg_offsets.fpscr_offset = -1;

      /* AltiVec registers.  */
      ppc64obsd_reg_offsets.vr0_offset = -1;
      ppc64obsd_reg_offsets.vscr_offset = -1;
      ppc64obsd_reg_offsets.vrsave_offset = -1;
    }

  if (ppc64obsd_fpreg_offsets.fpscr_offset == 0)
    {
      /* Floating-point registers.  */
      ppc64obsd_reg_offsets.f0_offset = -1;
      ppc64obsd_reg_offsets.fpscr_offset = -1;
    }
}

/* Target-dependent code for OpenBSD/powerpc.

   Copyright 2004 Free Software Foundation, Inc.

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
#include "osabi.h"
#include "regcache.h"
#include "regset.h"

#include "gdb_string.h"

#include "ppc-tdep.h"
#include "ppcobsd-tdep.h"
#include "solib-svr4.h"


/* Register set support functions.  */

/* FIXME: kettenis/20040418: There is nothing OpenBSD-specific about
   the functions on this page; they were written to be as general as
   possible.  This stuff should probably be moved to rs6000-tdep.c or
   perhaps a new ppc-tdep.c.  */

static void
ppc_supply_reg (struct regcache *regcache, int regnum, 
		const char *regs, size_t offset)
{
  if (regnum != -1 && offset != -1)
    regcache_raw_supply (regcache, regnum, regs + offset);
}

static void
ppc_collect_reg (const struct regcache *regcache, int regnum,
		 char *regs, size_t offset)
{
  if (regnum != -1 && offset != -1)
    regcache_raw_collect (regcache, regnum, regs + offset);
}
    
/* Supply the general-purpose registers stored in GREGS to REGCACHE.  */

void
ppc_supply_gregset (const struct regset *regset, struct regcache *regcache,
		    int regnum, const void *gregs, size_t len)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  const struct ppc_reg_offsets *offsets = regset->descr;
  size_t offset;
  int i;

  for (i = 0, offset = offsets->r0_offset; i < 32; i++, offset += 4)
    {
      if (regnum == -1 || regnum == i)
	ppc_supply_reg (regcache, i, gregs, offset);
    }

  if (regnum == -1 || regnum == PC_REGNUM)
    ppc_supply_reg (regcache, PC_REGNUM, gregs, offsets->pc_offset);
  if (regnum == -1 || regnum == tdep->ppc_ps_regnum)
    ppc_supply_reg (regcache, tdep->ppc_ps_regnum,
		    gregs, offsets->ps_offset);
  if (regnum == -1 || regnum == tdep->ppc_cr_regnum)
    ppc_supply_reg (regcache, tdep->ppc_cr_regnum,
		    gregs, offsets->cr_offset);
  if (regnum == -1 || regnum == tdep->ppc_lr_regnum)
    ppc_supply_reg (regcache, tdep->ppc_lr_regnum,
		    gregs, offsets->lr_offset);
  if (regnum == -1 || regnum == tdep->ppc_ctr_regnum)
    ppc_supply_reg (regcache, tdep->ppc_ctr_regnum,
		    gregs, offsets->ctr_offset);
  if (regnum == -1 || regnum == tdep->ppc_xer_regnum)
    ppc_supply_reg (regcache, tdep->ppc_xer_regnum,
		    gregs, offsets->cr_offset);
  if (regnum == -1 || regnum == tdep->ppc_mq_regnum)
    ppc_supply_reg (regcache, tdep->ppc_mq_regnum, gregs, offsets->mq_offset);
}

/* Supply the floating-point registers stored in FPREGS to REGCACHE.  */

void
ppc_supply_fpregset (const struct regset *regset, struct regcache *regcache,
		     int regnum, const void *fpregs, size_t len)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  const struct ppc_reg_offsets *offsets = regset->descr;
  size_t offset;
  int i;

  offset = offsets->f0_offset;
  for (i = FP0_REGNUM; i < FP0_REGNUM + 32; i++, offset += 4)
    {
      if (regnum == -1 || regnum == i)
	ppc_supply_reg (regcache, i, fpregs, offset);
    }

  if (regnum == -1 || regnum == tdep->ppc_fpscr_regnum)
    ppc_supply_reg (regcache, tdep->ppc_fpscr_regnum,
		    fpregs, offsets->fpscr_offset);
}

/* Collect the general-purpose registers from REGCACHE and store them
   in GREGS.  */

void
ppc_collect_gregset (const struct regset *regset,
		     const struct regcache *regcache,
		     int regnum, void *gregs, size_t len)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  const struct ppc_reg_offsets *offsets = &ppcobsd_reg_offsets;
  size_t offset;
  int i;

  offset = offsets->r0_offset;
  for (i = 0; i <= 32; i++, offset += 4)
    {
      if (regnum == -1 || regnum == i)
	ppc_collect_reg (regcache, regnum, gregs, offset);
    }

  if (regnum == -1 || regnum == PC_REGNUM)
    ppc_collect_reg (regcache, PC_REGNUM, gregs, offsets->pc_offset);
  if (regnum == -1 || regnum == tdep->ppc_ps_regnum)
    ppc_collect_reg (regcache, tdep->ppc_ps_regnum,
		     gregs, offsets->ps_offset);
  if (regnum == -1 || regnum == tdep->ppc_cr_regnum)
    ppc_collect_reg (regcache, tdep->ppc_cr_regnum,
		     gregs, offsets->cr_offset);
  if (regnum == -1 || regnum == tdep->ppc_lr_regnum)
    ppc_collect_reg (regcache, tdep->ppc_lr_regnum,
		     gregs, offsets->lr_offset);
  if (regnum == -1 || regnum == tdep->ppc_ctr_regnum)
    ppc_collect_reg (regcache, tdep->ppc_ctr_regnum,
		     gregs, offsets->ctr_offset);
  if (regnum == -1 || regnum == tdep->ppc_xer_regnum)
    ppc_collect_reg (regcache, tdep->ppc_xer_regnum,
		     gregs, offsets->xer_offset);
  if (regnum == -1 || regnum == tdep->ppc_mq_regnum)
    ppc_collect_reg (regcache, tdep->ppc_mq_regnum,
		     gregs, offsets->mq_offset);
}

/* Collect the floating-point registers from REGCACHE and store them
   in FPREGS.  */

void
ppc_collect_fpregset (const struct regset *regset,
		      const struct regcache *regcache,
		      int regnum, void *fpregs, size_t len)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  const struct ppc_reg_offsets *offsets = &ppcobsd_reg_offsets;
  size_t offset;
  int i;

  offset = offsets->f0_offset;
  for (i = FP0_REGNUM; i <= FP0_REGNUM + 32; i++, offset += 4)
    {
      if (regnum == -1 || regnum == i)
	ppc_collect_reg (regcache, regnum, fpregs, offset);
    }

  if (regnum == -1 || regnum == tdep->ppc_fpscr_regnum)
    ppc_collect_reg (regcache, tdep->ppc_fpscr_regnum,
		     fpregs, offsets->fpscr_offset);
}


/* Register offsets from <machine/reg.h>.  */
struct ppc_reg_offsets ppcobsd_reg_offsets;


/* Core file support.  */

/* Supply register REGNUM from the buffer specified by GREGS and LEN
   in the general-purpose register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

void
ppcobsd_supply_gregset (const struct regset *regset,
			struct regcache *regcache, int regnum,
			const void *gregs, size_t len)
{
  ppc_supply_gregset (regset, regcache, regnum, gregs, len);
  ppc_supply_fpregset (regset, regcache, regnum, gregs, len);
}

void
ppcobsd_collect_gregset (const struct regset *regset,
			 const struct regcache *regcache, int regnum,
			 void *gregs, size_t len)
{
  ppc_collect_gregset (regset, regcache, regnum, gregs, len);
  ppc_collect_fpregset (regset, regcache, regnum, gregs, len);
}

/* OpenBS/macppc register set.  */

struct regset ppcobsd_gregset =
{
  &ppcobsd_reg_offsets,
  ppcobsd_supply_gregset
};

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

static const struct regset *
ppcobsd_regset_from_core_section (struct gdbarch *gdbarch,
				  const char *sect_name, size_t sect_size)
{
  if (strcmp (sect_name, ".reg") == 0 && sect_size >= 412)
    return &ppcobsd_gregset;

  return NULL;
}


static void
ppcobsd_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* OpenBSD uses SVR4-style shared libraries.  */
  set_gdbarch_in_solib_call_trampoline
    (gdbarch, generic_in_solib_call_trampoline);
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);

  set_gdbarch_regset_from_core_section
    (gdbarch, ppcobsd_regset_from_core_section);
}


/* OpenBSD uses uses the traditional NetBSD core file format, even for
   ports that use ELF.  */
#define GDB_OSABI_NETBSD_CORE GDB_OSABI_OPENBSD_ELF

static enum gdb_osabi
ppcobsd_core_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "netbsd-core") == 0)
    return GDB_OSABI_NETBSD_CORE;

  return GDB_OSABI_UNKNOWN;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_ppcobsd_tdep (void);

void
_initialize_ppcobsd_tdep (void)
{
  /* BFD doesn't set a flavour for NetBSD style a.out core files.  */
  gdbarch_register_osabi_sniffer (bfd_arch_powerpc, bfd_target_unknown_flavour,
                                  ppcobsd_core_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_powerpc, 0, GDB_OSABI_OPENBSD_ELF,
			  ppcobsd_init_abi);

  /* Avoid initializing the register offsets again if they were
     already initailized by ppcobsd-nat.c.  */
  if (ppcobsd_reg_offsets.pc_offset == 0)
    {
      /* General-purpose registers.  */
      ppcobsd_reg_offsets.r0_offset = 0;
      ppcobsd_reg_offsets.pc_offset = 384;
      ppcobsd_reg_offsets.ps_offset = 388;
      ppcobsd_reg_offsets.cr_offset = 392;
      ppcobsd_reg_offsets.lr_offset = 396;
      ppcobsd_reg_offsets.ctr_offset = 400;
      ppcobsd_reg_offsets.xer_offset = 404;
      ppcobsd_reg_offsets.mq_offset = 408;

      /* Floating-point registers.  */
      ppcobsd_reg_offsets.f0_offset = 128;
      ppcobsd_reg_offsets.fpscr_offset = -1;

      /* AltiVec registers.  */
      ppcobsd_reg_offsets.vr0_offset = 0;
      ppcobsd_reg_offsets.vscr_offset = 512;
      ppcobsd_reg_offsets.vrsave_offset = 520;
    }
}

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
#include "gdb_assert.h"

#include "gdb_string.h"

#include "ppc-tdep.h"
#include "ppcobsd-tdep.h"
#include "solib-svr4.h"

/* Register offsets from <machine/reg.h>.  */
struct ppc_reg_offsets ppcobsd_reg_offsets;


/* Core file support.  */

/* Supply register REGNUM in the general-purpose register set REGSET
   from the buffer specified by GREGS and LEN to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

void
ppcobsd_supply_gregset (const struct regset *regset,
			struct regcache *regcache, int regnum,
			const void *gregs, size_t len)
{
  /* FIXME: jimb/2004-05-05: Some PPC variants don't have floating
     point registers.  Traditionally, GDB's register set has still
     listed the floating point registers for such machines, so this
     code is harmless.  However, the new E500 port actually omits the
     floating point registers entirely from the register set --- they
     don't even have register numbers assigned to them.

     It's not clear to me how best to update this code, so this assert
     will alert the first person to encounter the OpenBSD/E500
     combination to the problem.  */
  gdb_assert (ppc_floating_point_unit_p (current_gdbarch));

  ppc_supply_gregset (regset, regcache, regnum, gregs, len);
  ppc_supply_fpregset (regset, regcache, regnum, gregs, len);
}

/* Collect register REGNUM in the general-purpose register set
   REGSET. from register cache REGCACHE into the buffer specified by
   GREGS and LEN.  If REGNUM is -1, do this for all registers in
   REGSET.  */

void
ppcobsd_collect_gregset (const struct regset *regset,
			 const struct regcache *regcache, int regnum,
			 void *gregs, size_t len)
{
  /* FIXME: jimb/2004-05-05: Some PPC variants don't have floating
     point registers.  Traditionally, GDB's register set has still
     listed the floating point registers for such machines, so this
     code is harmless.  However, the new E500 port actually omits the
     floating point registers entirely from the register set --- they
     don't even have register numbers assigned to them.

     It's not clear to me how best to update this code, so this assert
     will alert the first person to encounter the OpenBSD/E500
     combination to the problem.  */
  gdb_assert (ppc_floating_point_unit_p (current_gdbarch));

  ppc_collect_gregset (regset, regcache, regnum, gregs, len);
  ppc_collect_fpregset (regset, regcache, regnum, gregs, len);
}

/* OpenBS/powerpc register set.  */

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

  gdbarch_register_osabi (bfd_arch_rs6000, 0, GDB_OSABI_OPENBSD_ELF,
			  ppcobsd_init_abi);
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

/* Target-dependent code for Motorola 68000 BSD's.

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

#include "m68k-tdep.h"
#include "solib-svr4.h"

/* Core file support.  */

/* Sizeof `struct reg' in <machine/reg.h>.  */
#define M68KBSD_SIZEOF_GREGS	(18 * 4)

/* Sizeof `struct fpreg' in <machine/reg.h.  */
#define M68KBSD_SIZEOF_FPREGS	(((8 * 3) + 3) * 4)

int
m68kbsd_fpreg_offset (int regnum)
{
  if (regnum >= M68K_FPC_REGNUM)
    return 8 * 12 + (regnum - M68K_FPC_REGNUM) * 4;

  return (regnum - M68K_FP0_REGNUM) * 12;
}

/* Supply register REGNUM from the buffer specified by FPREGS and LEN
   in the floating-point register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
m68kbsd_supply_fpregset (const struct regset *regset,
			 struct regcache *regcache,
			 int regnum, const void *fpregs, size_t len)
{
  const char *regs = fpregs;
  int i;

  gdb_assert (len >= M68KBSD_SIZEOF_FPREGS);

  for (i = M68K_FP0_REGNUM; i <= M68K_PC_REGNUM; i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_supply (regcache, i, regs + m68kbsd_fpreg_offset (i));
    }
}

/* Supply register REGNUM from the buffer specified by GREGS and LEN
   in the general-purpose register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
m68kbsd_supply_gregset (const struct regset *regset,
			struct regcache *regcache,
			int regnum, const void *gregs, size_t len)
{
  const char *regs = gregs;
  int i;

  gdb_assert (len >= M68KBSD_SIZEOF_GREGS);

  for (i = M68K_D0_REGNUM; i <= M68K_PC_REGNUM; i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_supply (regcache, i, regs + i * 4);
    }

  if (len >= M68KBSD_SIZEOF_GREGS + M68KBSD_SIZEOF_FPREGS)
    {
      regs += M68KBSD_SIZEOF_GREGS;
      len -= M68KBSD_SIZEOF_GREGS;
      m68kbsd_supply_fpregset (regset, regcache, regnum, regs, len);
    }
}

/* Motorola 68000 register sets.  */

static struct regset m68kbsd_gregset =
{
  NULL,
  m68kbsd_supply_gregset
};

static struct regset m68kbsd_fpregset =
{
  NULL,
  m68kbsd_supply_fpregset
};

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

static const struct regset *
m68kbsd_regset_from_core_section (struct gdbarch *gdbarch,
				  const char *sect_name, size_t sect_size)
{
  if (strcmp (sect_name, ".reg") == 0 && sect_size >= M68KBSD_SIZEOF_GREGS)
    return &m68kbsd_gregset;

  if (strcmp (sect_name, ".reg2") == 0 && sect_size >= M68KBSD_SIZEOF_FPREGS)
    return &m68kbsd_fpregset;

  return NULL;
}


/* Support for shared libraries.  */

/* Return non-zero if we are in a shared library trampoline code stub.  */

int
m68kbsd_aout_in_solib_call_trampoline (CORE_ADDR pc, char *name)
{
  return (name && !strcmp (name, "_DYNAMIC"));
}


static void
m68kbsd_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->jb_pc = 5;
  tdep->jb_elt_size = 4;

  set_gdbarch_regset_from_core_section
    (gdbarch, m68kbsd_regset_from_core_section);
}

/* OpenBSD and NetBSD a.out.  */

static void
m68kbsd_aout_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  m68kbsd_init_abi (info, gdbarch);

  tdep->struct_return = reg_struct_return;

  /* Assume SunOS-style shared libraries.  */
  set_gdbarch_in_solib_call_trampoline
    (gdbarch, m68kbsd_aout_in_solib_call_trampoline);
}

/* NetBSD ELF.  */

static void
m68kbsd_elf_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  m68kbsd_init_abi (info, gdbarch);

  /* NetBSD ELF uses the SVR4 ABI.  */
  m68k_svr4_init_abi (info, gdbarch);
  tdep->struct_return = pcc_struct_return;

  /* NetBSD ELF uses SVR4-style shared libraries.  */
  set_gdbarch_in_solib_call_trampoline
    (gdbarch, generic_in_solib_call_trampoline);
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);
}


static enum gdb_osabi
m68kbsd_aout_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "a.out-m68k-netbsd") == 0
      || strcmp (bfd_get_target (abfd), "a.out-m68k4k-netbsd") == 0)
    return GDB_OSABI_NETBSD_AOUT;

  return GDB_OSABI_UNKNOWN;
}

static enum gdb_osabi
m68kbsd_core_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "netbsd-core") == 0)
    return GDB_OSABI_NETBSD_AOUT;

  return GDB_OSABI_UNKNOWN;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_m68kbsd_tdep (void);

void
_initialize_m68kbsd_tdep (void)
{
  gdbarch_register_osabi_sniffer (bfd_arch_m68k, bfd_target_aout_flavour,
				  m68kbsd_aout_osabi_sniffer);

  /* BFD doesn't set a flavour for NetBSD style a.out core files.  */
  gdbarch_register_osabi_sniffer (bfd_arch_m68k, bfd_target_unknown_flavour,
				  m68kbsd_core_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_m68k, 0, GDB_OSABI_NETBSD_AOUT,
			  m68kbsd_aout_init_abi);
  gdbarch_register_osabi (bfd_arch_m68k, 0, GDB_OSABI_NETBSD_ELF,
			  m68kbsd_elf_init_abi);
}

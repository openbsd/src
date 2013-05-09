/* Target-dependent code for Motorola 88000 BSD's.

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

#include "m88k-tdep.h"
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "solib-svr4.h"

/* OpenBSD ELF.  */

static void
m88kbsd_elf_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* OpenBSD ELF uses SVR4-style shared libraries.  */
  set_gdbarch_in_solib_call_trampoline (gdbarch, in_plt_section);
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);
}


static enum gdb_osabi
m88kbsd_aout_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "a.out-m88k-openbsd") == 0)
    return GDB_OSABI_NETBSD_AOUT;

  return GDB_OSABI_UNKNOWN;
}

static enum gdb_osabi
m88kbsd_core_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "netbsd-core") == 0)
    return GDB_OSABI_NETBSD_AOUT;

  return GDB_OSABI_UNKNOWN;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_m88kbsd_tdep (void);

void
_initialize_m88kbsd_tdep (void)
{
  gdbarch_register_osabi_sniffer (bfd_arch_m88k, bfd_target_aout_flavour,
				  m88kbsd_aout_osabi_sniffer);

  /* BFD doesn't set a flavour for NetBSD style a.out core files.  */
  gdbarch_register_osabi_sniffer (bfd_arch_m88k, bfd_target_unknown_flavour,
				  m88kbsd_core_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_m88k, 0, GDB_OSABI_OPENBSD_ELF,
			  m88kbsd_elf_init_abi);
}

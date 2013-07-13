/* Target-dependent code for NetBSD/vax.

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

#include "vax-tdep.h"
#include "solib-svr4.h"

#include "gdb_string.h"

/* Support for shared libraries.  */

/* Return non-zero if we are in a shared library trampoline code stub.  */

int
vaxnbsd_aout_in_solib_call_trampoline (CORE_ADDR pc, char *name)
{
  return (name && !strcmp (name, "_DYNAMIC"));
}


/* NetBSD a.out.  */

static void
vaxnbsd_aout_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* Assume SunOS-style shared libraries.  */
  set_gdbarch_in_solib_call_trampoline
    (gdbarch, vaxnbsd_aout_in_solib_call_trampoline);
}

/* NetBSD ELF.  */

static void
vaxnbsd_elf_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* NetBSD ELF uses SVR4-style shared libraries.  */
  set_gdbarch_in_solib_call_trampoline
    (gdbarch, generic_in_solib_call_trampoline);
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_vaxnbsd_tdep (void);

void
_initialize_vaxnbsd_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_vax, 0, GDB_OSABI_NETBSD_AOUT,
			  vaxnbsd_aout_init_abi);
  gdbarch_register_osabi (bfd_arch_vax, 0, GDB_OSABI_NETBSD_ELF,
			  vaxnbsd_elf_init_abi);
  gdbarch_register_osabi (bfd_arch_vax, 0, GDB_OSABI_OPENBSD_ELF,
			  vaxnbsd_elf_init_abi);
}

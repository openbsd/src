/* Target-dependent code for NS32000 systems running NetBSD.
   Copyright 2002, 2003 Free Software Foundation, Inc. 
   Contributed by Wasabi Systems, Inc. 
 
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
#include "osabi.h"

#include "ns32k-tdep.h"
#include "gdb_string.h"

static int
ns32knbsd_aout_in_solib_call_trampoline (CORE_ADDR pc, char *name)
{
  if (strcmp (name, "_DYNAMIC") == 0)
    return 1;

  return 0;
}

static void
ns32knbsd_init_abi_common (struct gdbarch_info info,
                           struct gdbarch *gdbarch)
{
  /* We only support machines with the 32382 FPU.  */
  ns32k_gdbarch_init_32382 (gdbarch);
}

static void
ns32knbsd_init_abi_aout (struct gdbarch_info info,
                         struct gdbarch *gdbarch)
{
  ns32knbsd_init_abi_common (info, gdbarch);

  set_gdbarch_in_solib_call_trampoline (gdbarch,
                                     ns32knbsd_aout_in_solib_call_trampoline);
}

static enum gdb_osabi
ns32knbsd_aout_osabi_sniffer (bfd *abfd)
{
  if (strcmp (bfd_get_target (abfd), "a.out-ns32k-netbsd") == 0)
    return GDB_OSABI_NETBSD_AOUT;

  return GDB_OSABI_UNKNOWN;
}

extern initialize_file_ftype _initialize_ns32knbsd_tdep; /* -Wmissing-prototypes */

void
_initialize_ns32knbsd_tdep (void)
{
  gdbarch_register_osabi_sniffer (bfd_arch_ns32k, bfd_target_aout_flavour,
				  ns32knbsd_aout_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_ns32k, 0, GDB_OSABI_NETBSD_AOUT,
			  ns32knbsd_init_abi_aout);
}

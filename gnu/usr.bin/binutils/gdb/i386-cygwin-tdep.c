/* Target-dependent code for Cygwin running on i386's, for GDB.

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
#include "osabi.h"

#include "gdb_string.h"

#include "i386-tdep.h"

static void
i386_cygwin_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->struct_return = reg_struct_return;
}

static enum gdb_osabi
i386_cygwin_osabi_sniffer (bfd * abfd)
{ 
  char *target_name = bfd_get_target (abfd);

  /* Interix also uses pei-i386. 
     We need a way to distinguish between the two. */
  if (strcmp (target_name, "pei-i386") == 0)
    return GDB_OSABI_CYGWIN;

  return GDB_OSABI_UNKNOWN;
}

/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_i386_cygwin_tdep (void);

void
_initialize_i386_cygwin_tdep (void)
{
  gdbarch_register_osabi_sniffer (bfd_arch_i386, bfd_target_coff_flavour,
                                  i386_cygwin_osabi_sniffer);

  gdbarch_register_osabi (bfd_arch_i386, 0, GDB_OSABI_CYGWIN,
                          i386_cygwin_init_abi);
}

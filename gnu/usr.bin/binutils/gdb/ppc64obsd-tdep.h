/* Target-dependent code for OpenBSD/powerpc64.

   Copyright (C) 2004, 2006 Free Software Foundation, Inc.

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

#ifndef PPC64OBSD_TDEP_H
#define PPC64OBSD_TDEP_H

#include <stddef.h>

struct regset;
struct regcache;

/* Register offsets for OpenBSD/powerpc64.  */
extern struct ppc_reg_offsets ppc64obsd_reg_offsets;
extern struct ppc_reg_offsets ppc64obsd_fpreg_offsets;

/* Register sets for OpenBSD/powerpc64.  */
extern struct regset ppc64obsd_gregset;
extern struct regset ppc64obsd_fpregset;


/* Supply register REGNUM in the general-purpose register set REGSET
   from the buffer specified by GREGS and LEN to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

extern void ppc64obsd_supply_gregset (const struct regset *regset,
				      struct regcache *regcache, int regnum,
				      const void *gregs, size_t len);

/* Collect register REGNUM in the general-purpose register set
   REGSET. from register cache REGCACHE into the buffer specified by
   GREGS and LEN.  If REGNUM is -1, do this for all registers in
   REGSET.  */

extern void ppc64obsd_collect_gregset (const struct regset *regset,
				       const struct regcache *regcache,
				       int regnum, void *gregs, size_t len);

#endif /* ppc64obsd-tdep.h */

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

#ifndef PPCOBSD_TDEP_H
#define PPCOBSD_TDEP_H

#include <stddef.h>

struct regset;
struct regcache;

/* Register set description.  */

/* FIXME: kettenis/20040418: There is nothing OpenBSD-specific about
   this structure; it was written to be as general as possible.  This
   stuff should probably be moved to ppc-tdep.h.  */

struct ppc_reg_offsets
{
  /* General-purpose registers.  */
  int r0_offset;
  int pc_offset;
  int ps_offset;
  int cr_offset;
  int lr_offset;
  int ctr_offset;
  int xer_offset;
  int mq_offset;

  /* Floating-point registers.  */
  int f0_offset;
  int fpscr_offset;

  /* AltiVec registers.  */
  int vr0_offset;
  int vscr_offset;
  int vrsave_offset;
};


/* Register offsets for OpenBSD/macppc.  */
extern struct ppc_reg_offsets ppcobsd_reg_offsets;

/* Register sets for OpenBSD/macppc.  */
extern struct regset ppcobsd_gregset;


extern void ppcobsd_supply_gregset (const struct regset *regset,
				    struct regcache *regcache, int regnum,
				    const void *gregs, size_t len);

extern void ppcobsd_collect_gregset (const struct regset *regset,
				     const struct regcache *regcache,
				     int regnum, void *gregs, size_t len);

#endif /* ppcobsd-tdep.h */

/* Target-dependent definitions for PA-RISC.

   Copyright 2003, 2004 Free Software Foundation, Inc.

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

#ifndef HPPA_TDEP_H
#define HPPA_TDEP_H 1

struct trad_frame_saved_reg;

/* PA-RISC acrhitecture-specific information.  */

struct gdbarch_tdep
{
  int dummy;
};

/* Register numbers of various important registers.  */

enum hppa_regnum
{
  HPPA_R0_REGNUM,		/* %r0 */
  HPPA_R1_REGNUM,		/* %r1 */
  HPPA_RP_REGNUM,		/* %rp (%r2) */
  HPPA_R3_REGNUM,		/* %r3 */
  HPPA_R18_REGNUM = 18,		/* %r18 */
  HPPA_SP_REGNUM = 30,		/* %sp (%r30) */
  HPPA_R31_REGNUM = 31, 	/* %r31 */
  HPPA_SAR_REGNUM = 32,
  HPPA_PCOQ_HEAD_REGNUM,
  HPPA_PCOQ_TAIL_REGNUM
};


struct hppa_frame_cache
{
  /* Base address.  */
  CORE_ADDR base;
  CORE_ADDR pc;

  /* Frame size.  */
  size_t frame_size;

  /* Table of saved registers.  */
  struct trad_frame_saved_reg *saved_regs;
};

#endif  /* hppa-tdep.h */

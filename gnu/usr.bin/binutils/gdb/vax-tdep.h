/* Target-dependent code for the VAX.

   Copyright 2002, 2003, 2004 Free Software Foundation, Inc.

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

#ifndef VAX_TDEP_H
#define VAX_TDEP_H

/* Register numbers of various important registers.  */

enum vax_regnum
{
  VAX_R0_REGNUM,
  VAX_R1_REGNUM,
  VAX_AP_REGNUM = 12,		/* Argument pointer on user stack.  */
  VAX_FP_REGNUM,		/* Address of executing stack frame.  */
  VAX_SP_REGNUM,		/* Address of top of stack.  */
  VAX_PC_REGNUM,		/* Program counter.  */
  VAX_PS_REGNUM			/* Processor status.  */
};

/* Number of machine registers.  */
#define VAX_NUM_REGS 17

#endif /* vax-tdep.h */

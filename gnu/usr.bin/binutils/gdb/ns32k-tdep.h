/* Target-dependent definitions for GDB on NS32000 systems.
   Copyright 1987, 1989, 1991, 1993, 1994, 1998, 1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.

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

#ifndef NS32K_TDEP_H
#define NS32K_TDEP_H

/* Register numbers of various important registers.
   Note that some of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and some are "phony" register numbers which are too large
   to be actual register numbers as far as the user is concerned
   but do serve to get the desired values when passed to read_register.  */

#define NS32K_R0_REGNUM   0	/* General register 0 */
#define NS32K_FP0_REGNUM  8	/* Floating point register 0 */
#define NS32K_SP_REGNUM	  16	/* Contains address of top of stack */
#define NS32K_AP_REGNUM   NS32K_FP_REGNUM
#define NS32K_FP_REGNUM   17	/* Contains address of executing stack frame */
#define NS32K_PC_REGNUM   18	/* Contains program counter */
#define NS32K_PS_REGNUM   19	/* Contains processor status */
#define NS32K_FPS_REGNUM  20	/* Floating point status register */
#define NS32K_LP0_REGNUM  21	/* Double register 0 (same as FP0) */

#define NS32K_NUM_REGS_32082 25
#define NS32K_REGISTER_BYTES_32082 \
  ((NS32K_NUM_REGS_32082 - 4) * 4 /* size of general purpose regs */ \
   + 4                        * 8 /* size of floating point regs */)

#define NS32K_NUM_REGS_32382 29
#define NS32K_REGISTER_BYTES_32382 \
  ((NS32K_NUM_REGS_32382 - 4) * 4 /* size of general purpose regs */ \
   + 8                        * 8 /* size of floating point regs */)

#define NS32K_REGISTER_SIZE             4

void ns32k_gdbarch_init_32082 (struct gdbarch *);
void ns32k_gdbarch_init_32382 (struct gdbarch *);

#endif /* NS32K_TDEP_H */

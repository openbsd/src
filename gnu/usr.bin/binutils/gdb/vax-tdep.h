/* Common target dependent code for GDB on VAX systems.
   Copyright 2002, 2003 Free Software Foundation, Inc.

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

/* Say how long (ordinary) registers are.  This is a piece of bogosity
   used in push_word and a few other places;  DEPRECATED_REGISTER_RAW_SIZE is the
   real way to know how big a register is.  */
#define VAX_REGISTER_SIZE 4

/* Number of machine registers.  */
#define VAX_NUM_REGS 17

/* Total amount of space needed to store our copies of the machine's
   register state.  */
#define VAX_REGISTER_BYTES (VAX_NUM_REGS * 4)

/* Largest value DEPRECATED_REGISTER_RAW_SIZE can have.  */
#define VAX_MAX_REGISTER_RAW_SIZE 4

/* Largest value DEPRECATED_REGISTER_VIRTUAL_SIZE can have.  */
#define VAX_MAX_REGISTER_VIRTUAL_SIZE 4

/* Register numbers of various important registers.
   Note that most of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and are "phony" register numbers which is too large
   to be an actual register number as far as the user is concerned
   but serves to get the desired value when passed to read_register.  */

#define VAX_AP_REGNUM     12  /* argument pointer */
#define VAX_FP_REGNUM     13  /* Contains address of executing stack frame */
#define VAX_SP_REGNUM     14  /* Contains address of top of stack */
#define VAX_PC_REGNUM     15  /* Contains program counter */
#define VAX_PS_REGNUM     16  /* Contains processor status */

#endif /* VAX_TDEP_H */

/* Common definitions for GDB native support on Vaxen under 4.2bsd and Ultrix.
   Copyright (C) 1986, 1987, 1989, 1992 Free Software Foundation, Inc.

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

#define AP_REGNUM 12	/* XXXJRT */

#define REGISTER_U_ADDR(addr, blockend, regno)		\
{ addr = blockend - 0110 + regno * 4;			\
  if (regno == PC_REGNUM) addr = blockend - 8;		\
  if (regno == PS_REGNUM) addr = blockend - 4;		\
  if (regno == DEPRECATED_FP_REGNUM) addr = blockend - 0120;	\
  if (regno == AP_REGNUM) addr = blockend - 0124;	\
  if (regno == SP_REGNUM) addr = blockend - 20; }

/* Native-dependent definitions for VAXen under 4.2 BSD and ULTRIX.

   Copyright 1986, 1987, 1989, 1992, 2004 Free Software Foundation, Inc.

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

#ifndef NM_VAX_H
#define NM_VAX_H

extern CORE_ADDR vax_kernel_u_addr;
#define KERNEL_U_ADDR vax_kernel_u_addr

extern CORE_ADDR vax_register_u_addr (CORE_ADDR u_ar0, int regnum);
#define REGISTER_U_ADDR(addr, u_ar0, regnum) \
  (addr) = vax_register_u_addr (u_ar0, regnum)

#endif /* nm-vax.h */

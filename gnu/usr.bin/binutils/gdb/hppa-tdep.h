/* Common target dependent code for GDB on HPPA systems.
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

#ifndef HPPA_TDEP_H
#define HPPA_TDEP_H

/* Target-dependent structure in gdbarch.  */
struct gdbarch_tdep
{
  /* The number of bytes in an address.  For now, this field is designed
     to allow us to differentiate hppa32 from hppa64 targets.  */
  int bytes_per_address;
};

#endif  /* HPPA_TDEP_H */

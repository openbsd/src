/* Definitions to target GDB to GNU/Linux on hppa-linux.
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

#ifndef TM_LINUX_H
#define TM_LINUX_H

#include "pa/tm-hppa.h"
#include "config/tm-linux.h"

#undef IN_SOLIB_CALL_TRAMPOLINE
#undef SKIP_TRAMPOLINE_CODE

#endif /* #ifndef TM_LINUX_H */

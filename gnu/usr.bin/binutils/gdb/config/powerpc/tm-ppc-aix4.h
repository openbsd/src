/* Macro definitions for Power PC running AIX4.
   Copyright 1995 Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef TM_PPC_AIX4_H
#define TM_PPC_AIX4_H

/* The main executable doesn't need relocation in aix4.  Otherwise
   it looks just like any other AIX system.  */
#define DONT_RELOCATE_SYMFILE_OBJFILE

/* Use generic RS6000 definitions. */
#include "rs6000/tm-rs6000.h"

#define GDB_TARGET_POWERPC

#endif /* TM_PPC_AIX4_H */

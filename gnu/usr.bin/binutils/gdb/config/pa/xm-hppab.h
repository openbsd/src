/* Parameters for hosting on an HPPA PA-RISC machine, running BSD, for GDB.
   Copyright 1991, 1992 Free Software Foundation, Inc. 

   Contributed by the Center for Software Science at the
   University of Utah (pa-gdb-bugs@cs.utah.edu).

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* This is a big-endian host.  */

#define	HOST_BYTE_ORDER	BIG_ENDIAN

#include "pa/xm-pa.h"

#ifndef SEEK_SET
#  define SEEK_SET    0		/* Set file pointer to "offset" */
#  define SEEK_CUR    1		/* Set file pointer to current plus "offset" */
#  define SEEK_END    2		/* Set file pointer to EOF plus "offset" */
#endif /* SEEK_SET */

/* Parameters for hosting on an HPPA-RISC machine running HPUX, for GDB.
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

/* Host is big-endian. */
#define	HOST_BYTE_ORDER	BIG_ENDIAN

#include "pa/xm-pa.h"

#define USG

#ifndef __STDC__
/* This define is discussed in decode_line_1 in symtab.c  */
#define HPPA_COMPILER_BUG
#endif

#define HAVE_TERMIOS

/* HP defines malloc and realloc as returning void *, even for non-ANSI
   compilations (such as with the native compiler). */

#define MALLOC_INCOMPATIBLE

extern void *
malloc PARAMS ((size_t));

extern void *
realloc PARAMS ((void *, size_t));

extern void
free PARAMS ((void *));

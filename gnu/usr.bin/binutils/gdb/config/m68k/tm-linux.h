/* Definitions to target GDB to GNU/Linux on m680x0.

   Copyright 1996, 1998, 1999, 2000, 2002, 2003 Free Software Foundation,
   Inc.

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

/* We define SVR4_SHARED_LIBS unconditionally, on the assumption that
   link.h is available on all linux platforms.  For I386 and SH3/4, 
   we hard-code the information rather than use link.h anyway (for 
   the benefit of cross-debugging).  We may move to doing that for
   other architectures as well.  */

#define SVR4_SHARED_LIBS
#include "solib.h"		/* Support for shared libraries. */

/* Configuration file for an host running rs6000 OpenBSD.
   Copyright (C) 1999 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include <xm-openbsd.h>
#include <rs6000/xm-rs6000.h>

/* Don't get mistaken for systemV. */
#undef USG

/* OpenBSD is using the gnu-linker, and has no COFF dynamic library 
   specific support on rs6000 yet. */
#undef COLLECT_EXPORT_LIST

/* OpenBSD is not a broken system... */
#undef IO_BUFFER_SIZE

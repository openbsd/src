/* Native-dependent definitions for OpenBSD/i386.

   Copyright 2001, 2004 Free Software Foundation, Inc.

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

#ifndef NM_OBSD_H
#define NM_OBSD_H

/* Get generic BSD native definitions.  */
#include "config/nm-bsd.h"

/* Support for the user struct.  */

/* Return the size of the user struct.  */

#define KERNEL_U_SIZE kernel_u_size ()
extern int kernel_u_size (void);


/* Shared library support.  */

#include "solib.h"

#endif /* nm-obsd.h */

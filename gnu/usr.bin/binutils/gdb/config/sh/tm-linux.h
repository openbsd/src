/* Target-specific definitions for GNU/Linux running on a Renesas
   Super-H.

   Copyright 2000, 2002 Free Software Foundation, Inc.

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

/* Pull in GNU/Linux generic defs.  */
#include "config/tm-linux.h"

/* Pull in sh-target defs */
#include "sh/tm-sh.h"

/* Use target_specific function to define link map offsets.  */
extern struct link_map_offsets *sh_linux_svr4_fetch_link_map_offsets (void);
#define SVR4_FETCH_LINK_MAP_OFFSETS() sh_linux_svr4_fetch_link_map_offsets ()


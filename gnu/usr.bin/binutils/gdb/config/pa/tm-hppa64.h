/* Parameters for execution on any Hewlett-Packard PA-RISC machine.
   Copyright 1986, 1987, 1989, 1990, 1991, 1992, 1993, 1995, 1999, 2000
   Free Software Foundation, Inc.

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

/* PA 64-bit specific definitions.  Override those which are in
   tm-hppa.h */

struct frame_info;

/* jimb: this must go.  I'm just using it to disable code I haven't
   gotten working yet.  */
#define GDB_TARGET_IS_HPPA_20W

/* NOTE: cagney/2003-07-27: Using CC='cc +DA2.0W -Ae' configure
   hppa64-hp-hpux11.00; GDB managed to build / start / break main /
   run with multi-arch enabled.  Not sure about much else as there
   appears to be an unrelated problem in the SOM symbol table reader
   causing GDB to lose line number information.  Since prior to this
   switch and a other recent tweaks, 64 bit PA hadn't been building
   for some months, this is probably the lesser of several evils.  */

#include "pa/tm-hppah.h"

#undef FP4_REGNUM
#define FP4_REGNUM 68
#define AP_REGNUM 29  /* Argument Pointer Register */
#define DP_REGNUM 27
#define FP5_REGNUM 70
#define SR5_REGNUM 48


/* jimb: omitted dynamic linking stuff here */

#undef FUNC_LDIL_OFFSET
#undef FUNC_LDO_OFFSET
#undef SR4EXPORT_LDIL_OFFSET
#undef SR4EXPORT_LDO_OFFSET

/* jimb: omitted purify call support */

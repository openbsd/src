/* Parameters for execution on an HP PA-RISC machine, running HPUX, for GDB.
   Copyright 1991, 1992, 1995, 1998, 2002, 2003, 2004
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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

struct frame_info;

/* The solib hooks are not really designed to have a list of hook
   and handler routines.  So until we clean up those interfaces you
   either get SOM shared libraries or HP's unusual PA64 ELF shared
   libraries, but not both.  */
#ifdef GDB_TARGET_IS_HPPA_20W
#include "pa64solib.h"
#endif

#ifndef GDB_TARGET_IS_HPPA_20W
#include "somsolib.h"
#endif

/* For HP-UX on PA-RISC we have an implementation
   for the exception handling target op (in hppa-tdep.c) */
#define CHILD_ENABLE_EXCEPTION_CALLBACK
#define CHILD_GET_CURRENT_EXCEPTION_EVENT

/* Mostly it's common to all HPPA's.  */
#include "pa/tm-hppa.h"

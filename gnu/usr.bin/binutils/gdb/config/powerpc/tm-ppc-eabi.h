/* Macro definitions for Power PC running embedded ABI.
   Copyright 1995, 1996 Free Software Foundation, Inc.

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

#ifndef TM_PPC_EABI_H
#define TM_PPC_EABI_H

/* Use generic RS6000 definitions. */
#include "rs6000/tm-rs6000.h"
/* except we want to allow single stepping */
#undef NO_SINGLE_STEP

#undef	DEFAULT_LR_SAVE
#define	DEFAULT_LR_SAVE 4	/* eabi saves LR at 4 off of SP */

#define GDB_TARGET_POWERPC

#undef PC_LOAD_SEGMENT
#undef PROCESS_LINENUMBER_HOOK

#undef TEXT_SEGMENT_BASE
#define TEXT_SEGMENT_BASE 1

/* Say that we're using ELF, not XCOFF.  */
#define ELF_OBJECT_FORMAT 1

/* return true if a given `pc' value is in `call dummy' function. */
/* FIXME: This just checks for the end of the stack, which is broken
   for things like stepping through gcc nested function stubs.  */
#undef PC_IN_CALL_DUMMY
#define	PC_IN_CALL_DUMMY(STOP_PC, STOP_SP, STOP_FRAME_ADDR)	\
	(STOP_SP < STOP_PC)

#define TARGET_BYTE_ORDER_SELECTABLE

#endif /* TM_PPC_EABI_H */

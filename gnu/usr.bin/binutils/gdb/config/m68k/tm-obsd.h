/* Macro definitions for m68k running under OpenBSD.
   Copyright 1994 Free Software Foundation, Inc.

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

#ifndef TM_OBSD_H
#define TM_OBSD_H

/* Net- and OpenBSD uses trap 15 for both user and kernel breakpoints. */
#define BPT_VECTOR 0xf
#define REMOTE_BPT_VECTOR 0xf

/* For Net- and OpenBSD, sigtramp is 32 bytes before STACK_END_ADDR,
   but we don't know where that is until run-time!  */
extern int nbsd_in_sigtramp(CORE_ADDR);
#define IN_SIGTRAMP(pc, name) nbsd_in_sigtramp (pc)

/* Generic definitions.  */
#include "m68k/tm-m68k.h"
#include "tm-obsd.h"

/* Offset to saved PC in sigcontext, from <m68k/signal.h>.  */
#define SIGCONTEXT_PC_OFFSET 20

#endif /* TM_OBSD_H */

/* Target machine definitions for a generic m68k monitor/emulator.
   Copyright (C) 1986, 1987, 1989, 1993, 1995 Free Software Foundation, Inc.

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

/* The definitions here are appropriate for several embedded m68k-based
   targets, including IDP (rom68k), BCC (cpu32bug), and EST's emulator.  */

/* GCC is probably the only compiler used on this configuration.  So
   get this right even if the code which detects gcc2_compiled. is
   still broken.  */

#define BELIEVE_PCC_PROMOTION 1

/* The target system handles breakpoints.  */

#define DECR_PC_AFTER_BREAK 0

/* No float registers.  */

/*#define NUM_REGS 18*/

#include "m68k/tm-m68k.h"

/* Need to do this for ELF targets, where we can't figure out the boundaries of
   the entry file.  This method stops the backtrace when we reach main.  */

#undef FRAME_CHAIN_VALID
#define FRAME_CHAIN_VALID_ALTERNATE

/* FIXME, should do GET_LONGJMP_TARGET for newlib.  */

/* Parameters for execution on ES-1800 emulator for 68000.
   The code was originally written by Johan Holmberg TT/SJ Ericsson Telecom
   AB and later modified by Johan Henriksson TT/SJ. It was adapted to GDB 4.0
   by Jan Norden TX/DK.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993 Free Software Foundation, Inc.

This file is part of GDB.

GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#define	GDBINIT_FILENAME	".esgdbinit"

#define	DEFAULT_PROMPT		"(esgdb) "

#include "m68k/tm-m68k.h"

/* Longjmp stuff borrowed from sun3 configuration.  Don't know if correct. 
   FIXME.  */
/* Offsets (in target ints) into jmp_buf.  Not defined by Sun, but at least
   documented in a comment in <machine/setjmp.h>! */

#define JB_ELEMENT_SIZE 4

#define JB_ONSSTACK 0
#define JB_SIGMASK 1
#define JB_SP 2
#define JB_PC 3
#define JB_PSL 4
#define JB_D2 5
#define JB_D3 6
#define JB_D4 7
#define JB_D5 8
#define JB_D6 9
#define JB_D7 10
#define JB_A2 11
#define JB_A3 12
#define JB_A4 13
#define JB_A5 14
#define JB_A6 15

/* Figure out where the longjmp will land.  Slurp the args out of the stack.
   We expect the first arg to be a pointer to the jmp_buf structure from which
   we extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
   This routine returns true on success */

#define GET_LONGJMP_TARGET(ADDR) get_longjmp_target(ADDR)
extern int get_longjmp_target PARAMS ((CORE_ADDR *));

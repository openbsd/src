/* Target definitions for GDB on a Motorola 680x0 running SVR4.
   (Commodore Amiga with amix or Atari TT with ASV)
   Copyright (C) 1991, 1995 Free Software Foundation, Inc.
   Written by Fred Fish at Cygnus Support (fnf@cygint)

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

/* Define BPT_VECTOR if it is different than the default.
   This is the vector number used by traps to indicate a breakpoint. */

#define BPT_VECTOR 0x1

/* How much to decrement the PC after a trap.  Depends on kernel. */

#define DECR_PC_AFTER_BREAK 0		/* No decrement required */

/* Use the alternate method of determining valid frame chains. */

#define FRAME_CHAIN_VALID_ALTERNATE

#include "tm-sysv4.h"
#include "m68k/tm-m68k.h"

/* Offsets (in target ints) into jmp_buf.  Not defined in any system header
   file, so we have to step through setjmp/longjmp with a debugger and figure
   them out.  As a double check, note that <setjmp> defines _JBLEN as 13,
   which matches the number of elements we see saved by setjmp(). */

#define JB_ELEMENT_SIZE sizeof(int)	/* jmp_buf[_JBLEN] is array of ints */

#define JB_D2	0
#define JB_D3	1
#define JB_D4	2
#define JB_D5	3
#define JB_D6	4
#define JB_D7	5
#define JB_A1	6
#define JB_A2	7
#define JB_A3	8
#define JB_A4	9
#define JB_A5	10
#define JB_A6	11
#define JB_A7	12

#define JB_PC	JB_A1	/* Setjmp()'s return PC saved in A1 */

/* Figure out where the longjmp will land.  Slurp the args out of the stack.
   We expect the first arg to be a pointer to the jmp_buf structure from which
   we extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
   This routine returns true on success */

#define GET_LONGJMP_TARGET(ADDR) get_longjmp_target(ADDR)
extern int get_longjmp_target PARAMS ((CORE_ADDR *));

/* Convert a DWARF register number to a gdb REGNUM.  */
#define DWARF_REG_TO_REGNUM(num) ((num) < 16 ? (num) : (num)+FP0_REGNUM-16)

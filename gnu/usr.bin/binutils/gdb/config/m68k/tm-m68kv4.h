// OBSOLETE /* Target definitions for GDB on a Motorola 680x0 running SVR4.
// OBSOLETE    (Commodore Amiga with amix or Atari TT with ASV)
// OBSOLETE    Copyright 1991, 1994, 1995, 1996, 1998, 1999, 2000, 2003
// OBSOLETE    Free Software Foundation, Inc.
// OBSOLETE    Written by Fred Fish at Cygnus Support (fnf@cygint)
// OBSOLETE 
// OBSOLETE    This file is part of GDB.
// OBSOLETE 
// OBSOLETE    This program is free software; you can redistribute it and/or modify
// OBSOLETE    it under the terms of the GNU General Public License as published by
// OBSOLETE    the Free Software Foundation; either version 2 of the License, or
// OBSOLETE    (at your option) any later version.
// OBSOLETE 
// OBSOLETE    This program is distributed in the hope that it will be useful,
// OBSOLETE    but WITHOUT ANY WARRANTY; without even the implied warranty of
// OBSOLETE    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// OBSOLETE    GNU General Public License for more details.
// OBSOLETE 
// OBSOLETE    You should have received a copy of the GNU General Public License
// OBSOLETE    along with this program; if not, write to the Free Software
// OBSOLETE    Foundation, Inc., 59 Temple Place - Suite 330,
// OBSOLETE    Boston, MA 02111-1307, USA.  */
// OBSOLETE 
// OBSOLETE /* Define BPT_VECTOR if it is different than the default.
// OBSOLETE    This is the vector number used by traps to indicate a breakpoint. */
// OBSOLETE 
// OBSOLETE #define BPT_VECTOR 0x1
// OBSOLETE 
// OBSOLETE /* How much to decrement the PC after a trap.  Depends on kernel. */
// OBSOLETE 
// OBSOLETE #define DECR_PC_AFTER_BREAK 0	/* No decrement required */
// OBSOLETE 
// OBSOLETE #include "config/tm-sysv4.h"
// OBSOLETE #include "m68k/tm-m68k.h"
// OBSOLETE 
// OBSOLETE /* Offsets (in target ints) into jmp_buf.  Not defined in any system header
// OBSOLETE    file, so we have to step through setjmp/longjmp with a debugger and figure
// OBSOLETE    them out.  As a double check, note that <setjmp> defines _JBLEN as 13,
// OBSOLETE    which matches the number of elements we see saved by setjmp(). */
// OBSOLETE 
// OBSOLETE #define JB_ELEMENT_SIZE sizeof(int)	/* jmp_buf[_JBLEN] is array of ints */
// OBSOLETE 
// OBSOLETE #define JB_D2	0
// OBSOLETE #define JB_D3	1
// OBSOLETE #define JB_D4	2
// OBSOLETE #define JB_D5	3
// OBSOLETE #define JB_D6	4
// OBSOLETE #define JB_D7	5
// OBSOLETE #define JB_A1	6
// OBSOLETE #define JB_A2	7
// OBSOLETE #define JB_A3	8
// OBSOLETE #define JB_A4	9
// OBSOLETE #define JB_A5	10
// OBSOLETE #define JB_A6	11
// OBSOLETE #define JB_A7	12
// OBSOLETE 
// OBSOLETE #define JB_PC	JB_A1		/* Setjmp()'s return PC saved in A1 */
// OBSOLETE 
// OBSOLETE /* Figure out where the longjmp will land.  Slurp the args out of the stack.
// OBSOLETE    We expect the first arg to be a pointer to the jmp_buf structure from which
// OBSOLETE    we extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
// OBSOLETE    This routine returns true on success */
// OBSOLETE 
// OBSOLETE #define GET_LONGJMP_TARGET(ADDR) m68k_get_longjmp_target(ADDR)
// OBSOLETE 
// OBSOLETE /* Convert a DWARF register number to a gdb REGNUM.  */
// OBSOLETE #define DWARF_REG_TO_REGNUM(num) ((num) < 16 ? (num) : (num)+FP0_REGNUM-16)

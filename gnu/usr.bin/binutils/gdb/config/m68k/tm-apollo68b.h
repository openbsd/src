/* Parameters for execution on Apollo 68k running BSD.
   Copyright (C) 1986, 1987, 1989, 1991 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

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

/* Apollos use vector 0xb for the breakpoint vector */

#define BPT_VECTOR 0xb

#include "m68k/tm-m68k.h"

#define FRAME_CHAIN_VALID(chain, thisframe) (chain != 0)

/* These are the jmp_buf registers I could guess. There are 13 registers
 * in the buffer. There are 8 data registers, 6 general address registers,
 * the Frame Pointer, the Stack Pointer, the PC and the SR in the chip. I would
 * guess that 12 is the SR, but we don't need that anyway. 0 and 1 have
 * me stumped. 4 appears to be a5 for some unknown reason. If you care
 * about this, disassemble setjmp to find out. But don't do it with gdb :)
 */

#undef JB_SP
#undef JB_FP
#undef JB_PC
#undef JB_D0
#undef JB_D1
#undef JB_D2
#undef JB_D3
#undef JB_D4
#undef JB_D5

#define JB_SP 2
#define JB_FP 3
#define JB_PC 5
#define JB_D0 6
#define JB_D1 7
#define JB_D2 8
#define JB_D3 9
#define JB_D4 10
#define JB_D5 11

/* How to decide if we're in a shared library function.  (Probably a wrong
   definintion inherited from the VxWorks config file).  */
#define	IN_SOLIB_CALL_TRAMPOLINE(pc, name) (name && strcmp(name, "<end_of_program>") == 0)

/* Target machine description for VxWorks m68k's, for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1996, 1998, 1999, 2000,
   2002, 2003
   Free Software Foundation, Inc.
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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* GCC is probably the only compiler used on this configuration.  So
   get this right even if the code which detects gcc2_compiled. is
   still broken.  */

#define BELIEVE_PCC_PROMOTION 1

/* We have more complex, useful breakpoints on the target.  */
#define	DECR_PC_AFTER_BREAK	0

#include "m68k/tm-m68k.h"
#include "config/tm-vxworks.h"

/* Takes the current frame-struct pointer and returns the chain-pointer
   to get to the calling frame.

   If our current frame pointer is zero, we're at the top; else read out
   the saved FP from memory pointed to by the current FP.  */

#undef	DEPRECATED_FRAME_CHAIN
#define DEPRECATED_FRAME_CHAIN(thisframe) ((thisframe)->frame? read_memory_integer ((thisframe)->frame, 4): 0)

/* FIXME, Longjmp information stolen from Sun-3 config.  Dunno if right.  */
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

#define GET_LONGJMP_TARGET(ADDR) m68k_get_longjmp_target(ADDR)

/* Number of registers in a ptrace_getfpregs call. */

#define VX_SIZE_FPREGS (8 * DEPRECATED_REGISTER_RAW_SIZE (FP0_REGNUM) \
			+ (3 * DEPRECATED_REGISTER_SIZE))

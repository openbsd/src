/* Parameters for VxWorks Intel 960's, for GDB, the GNU debugger.
   Copyright (C) 1986-1991 Free Software Foundation, Inc.
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

#include "i960/tm-i960.h"

/* Under VxWorks the IP isn't filled in.  Skip it, go with RIP, which has
   the real value.  */
#undef PC_REGNUM
#define PC_REGNUM RIP_REGNUM

#define	GDBINIT_FILENAME	".vxgdbinit"

#define	DEFAULT_PROMPT		"(vxgdb) "

/* We have more complex, useful breakpoints on the target.
   Amount ip must be decremented by after a breakpoint.  */

#define	DECR_PC_AFTER_BREAK	0

/* We are guaranteed to have a zero frame pointer at bottom of stack, too. */

#define FRAME_CHAIN_VALID(chain, thisframe) (chain != 0)

/* Breakpoint patching is handled at the target end in VxWorks.  */
/* #define BREAKPOINT {0x00, 0x3e, 0x00, 0x66} */

/* Number of registers in a ptrace_getregs call. */

#define VX_NUM_REGS (16 + 16 + 3)

/* Number of registers in a ptrace_getfpregs call. */

    /* @@ Can't use this -- the rdb library for the 960 target
       doesn't support setting or retrieving FP regs.  KR  */

/* #define VX_SIZE_FPREGS (REGISTER_RAW_SIZE (FP0_REGNUM) * 4) */

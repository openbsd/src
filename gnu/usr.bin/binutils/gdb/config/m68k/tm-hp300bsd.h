/* Parameters for target machine Hewlett-Packard 9000/300, running bsd.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993 Free Software Foundation, Inc.

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

/* Configuration file for HP9000/300 series machine running BSD,
   including Utah, Mt. Xinu or Berkeley variants.  This is NOT for HP-UX.
   Problems to hpbsd-bugs@cs.utah.edu.  */

/* GCC is the only compiler used on this OS.  So get this right even if
   the code which detects gcc2_compiled. is still broken.  */

#define BELIEVE_PCC_PROMOTION 1

/* Define BPT_VECTOR if it is different than the default.
   This is the vector number used by traps to indicate a breakpoint.

   For hp300bsd the normal breakpoint vector is 0x2 (for debugging via
   ptrace); for remote kernel debugging the breakpoint vector is 0xf.  */

#define BPT_VECTOR 0x2
#define REMOTE_BPT_VECTOR 0xf

#define TARGET_NBPG 4096

/* For 4.4 this would be 2, but it is OK for us to detect an area a
   bit bigger than necessary.  This way the same gdb binary can target
   either 4.3 or 4.4.  */

#define TARGET_UPAGES 3

/* On the HP300, sigtramp is in the u area.  Gak!  User struct is not
   mapped to the same virtual address in user/kernel address space
   (hence STACK_END_ADDR as opposed to KERNEL_U_ADDR).  This tests
   for the whole u area, since we don't necessarily have hp300bsd
   include files around.  */

/* For 4.4, it is actually right 20 bytes *before* STACK_END_ADDR, so
   include that in the area we test for.  */

#define SIGTRAMP_START(pc) (STACK_END_ADDR - 20)
#define SIGTRAMP_END(pc) (STACK_END_ADDR + TARGET_UPAGES * TARGET_NBPG)

/* Address of end of stack space.  */

#define STACK_END_ADDR 0xfff00000

#include "m68k/tm-m68k.h"

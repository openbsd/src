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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	$Id: tm.h,v 1.1.1.1 1995/10/18 08:40:07 deraadt Exp $
*/
#include <machine/vmparam.h>

/* Configuration file for HP9000/300 series machine running BSD,
   including Utah, Mt. Xinu or Berkeley variants.  This is NOT for HP-UX.
   Problems to hpbsd-bugs@cs.utah.edu.  */

/* Define BPT_VECTOR if it is different than the default.
   This is the vector number used by traps to indicate a breakpoint. */

#define BPT_VECTOR 0x2

#define TARGET_NBPG NBPG

/* For 4.4 this would be 2, but it is OK for us to detect an area a
   bit bigger than necessary.  This way the same gdb binary can target
   either 4.3 or 4.4.  */

#define TARGET_UPAGES UPAGES

/* On the HP300, sigtramp is in the u area.  Gak!  User struct is not
   mapped to the same virtual address in user/kernel address space
   (hence STACK_END_ADDR as opposed to KERNEL_U_ADDR).  This tests
   for the whole u area, since we don't necessarily have hp300bsd
   include files around.  */

/* For 4.4, it is actually right 20 bytes before STACK_END_ADDR.  For
   NetBSD, it is 32 bytes before STACK_END_ADDR.  We include both
   regions in the area we test for.  */

#define SIGTRAMP_START (STACK_END_ADDR - 32)
#define SIGTRAMP_END (STACK_END_ADDR + TARGET_UPAGES * TARGET_NBPG)

/* Address of end of stack space.  */

#define STACK_END_ADDR USRSTACK

/* We define our own fetch and store methods. */

#define FETCH_INFERIOR_REGISTERS

/* Include most of the common m68k definitions. */
#include "tm-m68k.h"

/* Include shared library handling. */
#include "solib.h"

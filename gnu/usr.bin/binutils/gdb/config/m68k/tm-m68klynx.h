/* Macro definitions for Motorola 680x0 running under LynxOS.
   Copyright 1993 Free Software Foundation, Inc.

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

#ifndef TM_M68KLYNX_H
#define TM_M68KLYNX_H

#include "tm-lynx.h"

/* If PC-2 contains this instruction, then we know what we are in a system
   call stub, and the return PC is is at SP+4, instead of SP. */

#define SYSCALL_TRAP 0x4e4a	/* trap #10 */
#define SYSCALL_TRAP_OFFSET 2	/* PC is after trap instruction */

/* Use the generic 68k definitions. */

#include "m68k/tm-m68k.h"

/* Disable dumbshit alternate breakpoint mechanism needed by 68k stub. */
#undef REMOTE_BREAKPOINT

#endif /* TM_M68KLYNX_H */

// OBSOLETE /* Macro definitions for Motorola 680x0 running under LynxOS.
// OBSOLETE    Copyright 1993 Free Software Foundation, Inc.
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
// OBSOLETE #ifndef TM_M68KLYNX_H
// OBSOLETE #define TM_M68KLYNX_H
// OBSOLETE 
// OBSOLETE #include "config/tm-lynx.h"
// OBSOLETE 
// OBSOLETE /* If PC-2 contains this instruction, then we know what we are in a system
// OBSOLETE    call stub, and the return PC is is at SP+4, instead of SP. */
// OBSOLETE 
// OBSOLETE #define SYSCALL_TRAP 0x4e4a	/* trap #10 */
// OBSOLETE #define SYSCALL_TRAP_OFFSET 2	/* PC is after trap instruction */
// OBSOLETE 
// OBSOLETE /* Use the generic 68k definitions. */
// OBSOLETE 
// OBSOLETE #include "m68k/tm-m68k.h"
// OBSOLETE 
// OBSOLETE /* Disable dumbshit alternate breakpoint mechanism needed by 68k stub. */
// OBSOLETE #undef DEPRECATED_REMOTE_BREAKPOINT
// OBSOLETE 
// OBSOLETE #endif /* TM_M68KLYNX_H */

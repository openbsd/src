/* Macro defintions for an Apollo m68k in BSD mode
   Copyright (C) 1992 Free Software Foundation, Inc.

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

#define PTRACE_IN_WRONG_PLACE

#define	FETCH_INFERIOR_REGISTERS

/* Tell gdb that we can attach and detach other processes */
#define ATTACH_DETACH

#define U_REGS_OFFSET 6

/* This is the amount to subtract from u.u_ar0
   to get the offset in the core file of the register values.  */

#define KERNEL_U_ADDR 0

#undef FLOAT_INFO	/* No float info yet */

#define REGISTER_U_ADDR(addr, blockend, regno) \
	(addr) = (6 + 4 * (regno))

/* Apollos don't really have a USER area,so trying to read it from the
 * process address space will fail. It does support a read from a faked
 * USER area using the "PEEKUSER" ptrace call.
 */
#define PT_READ_U 3

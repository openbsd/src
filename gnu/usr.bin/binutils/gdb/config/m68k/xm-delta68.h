/* Macro definitions for a Delta.
   Copyright (C) 1986, 1987, 1989, 1992 Free Software Foundation, Inc.

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

#define HOST_BYTE_ORDER BIG_ENDIAN

/* I'm running gdb 4.9 under sysV68 R3V7.1.

   On some machines, gdb crashes when it's starting up while calling the
   vendor's termio tgetent() routine.  It always works when run under
   itself (actually, under 3.2, it's not an infinitely recursive bug.)
   After some poking around, it appears that depending on the environment
   size, or whether you're running YP, or the phase of the moon or something,
   the stack is not always long-aligned when main() is called, and tgetent()
   takes strong offense at that.  On some machines this bug never appears, but
   on those where it does, it occurs quite reliably.  */
#define ALIGN_STACK_ON_STARTUP

#define USG

#define HAVE_TERMIO

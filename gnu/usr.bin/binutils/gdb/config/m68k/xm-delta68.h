// OBSOLETE /* Macro definitions for a Delta.
// OBSOLETE    Copyright (C) 1986, 1987, 1989, 1992 Free Software Foundation, Inc.
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
// OBSOLETE /* I'm running gdb 4.9 under sysV68 R3V7.1.
// OBSOLETE 
// OBSOLETE    On some machines, gdb crashes when it's starting up while calling the
// OBSOLETE    vendor's termio tgetent() routine.  It always works when run under
// OBSOLETE    itself (actually, under 3.2, it's not an infinitely recursive bug.)
// OBSOLETE    After some poking around, it appears that depending on the environment
// OBSOLETE    size, or whether you're running YP, or the phase of the moon or something,
// OBSOLETE    the stack is not always long-aligned when main() is called, and tgetent()
// OBSOLETE    takes strong offense at that.  On some machines this bug never appears, but
// OBSOLETE    on those where it does, it occurs quite reliably.  */
// OBSOLETE #define ALIGN_STACK_ON_STARTUP
// OBSOLETE 
// OBSOLETE #define USG
// OBSOLETE 
// OBSOLETE #define HAVE_TERMIO

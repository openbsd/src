// OBSOLETE /* Parameters for targeting GDB to a 3b1.
// OBSOLETE    Copyright 1986, 1987, 1989, 1991, 1993 Free Software Foundation, Inc.
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
// OBSOLETE /* The child target can't deal with floating registers.  */
// OBSOLETE #define CANNOT_STORE_REGISTER(regno) ((regno) >= FP0_REGNUM)
// OBSOLETE 
// OBSOLETE /* Define BPT_VECTOR if it is different than the default.
// OBSOLETE    This is the vector number used by traps to indicate a breakpoint. */
// OBSOLETE 
// OBSOLETE #define BPT_VECTOR 0x1
// OBSOLETE 
// OBSOLETE /* Address of end of stack space.  */
// OBSOLETE 
// OBSOLETE #define STACK_END_ADDR 0x300000
// OBSOLETE 
// OBSOLETE #include "m68k/tm-m68k.h"

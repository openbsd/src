// OBSOLETE /* Native-only definitions for Sun-3 for GDB, the GNU debugger.
// OBSOLETE    Copyright 1986, 1987, 1989, 1991, 1992, 1996
// OBSOLETE    Free Software Foundation, Inc.
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
// OBSOLETE #include "regcache.h"
// OBSOLETE 
// OBSOLETE /* Do implement the attach and detach commands.  */
// OBSOLETE 
// OBSOLETE #define ATTACH_DETACH
// OBSOLETE 
// OBSOLETE /* Override copies of {fetch,store}_inferior_registers in infptrace.c.  */
// OBSOLETE 
// OBSOLETE #define FETCH_INFERIOR_REGISTERS
// OBSOLETE 
// OBSOLETE /* We have to grab the regs since we store all regs at once.  */
// OBSOLETE 
// OBSOLETE #define CHILD_PREPARE_TO_STORE() deprecated_read_register_bytes (0, NULL, DEPRECATED_REGISTER_BYTES)

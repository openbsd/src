// OBSOLETE /* Native support for a Bull DPX2.
// OBSOLETE    Copyright 1986, 1987, 1989, 1993, 2000 Free Software Foundation, Inc.
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
// OBSOLETE /* KERNEL_U_ADDR is determined upon startup in dpx2-xdep.c. */
// OBSOLETE 
// OBSOLETE #define REGISTER_U_ADDR(addr, blockend, regno) \
// OBSOLETE 	(addr) = dpx2_register_u_addr ((blockend),(regno));
// OBSOLETE 
// OBSOLETE extern int dpx2_register_u_addr (int, int);
// OBSOLETE 
// OBSOLETE /* Kernel is a bit tenacious about sharing text segments, disallowing bpts.  */
// OBSOLETE #define	ONE_PROCESS_WRITETEXT

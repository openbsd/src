/* Parameters for execution on a H8/300 series machine.
   Copyright 1992, 1993, 1994, 1996, 1998, 1999, 2000
   Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */
/* Contributed by Steve Chamberlain sac@cygnus.com */

/* Needed for remote.c */
#define DEPRECATED_REMOTE_BREAKPOINT { 0x57, 0x30}		/* trapa #3 */
/* Needed for remote-hms.c */
#define CCR_REGNUM 8
/* Needed for remote-e7000.c */
#define NUM_REALREGS ((TARGET_ARCHITECTURE->mach == bfd_mach_h8300s || \
		        TARGET_ARCHITECTURE->mach == bfd_mach_h8300sn || \
		        TARGET_ARCHITECTURE->mach == bfd_mach_h8300sx || \
		        TARGET_ARCHITECTURE->mach == bfd_mach_h8300sxn) ? 11 : 10)


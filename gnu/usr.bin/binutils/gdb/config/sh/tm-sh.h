/* Target-specific definition for a Renesas Super-H.
   Copyright 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002
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

#define NUM_REALREGS 59 /* used in remote-e7000.c which is not multiarched. */

#define DEPRECATED_BIG_REMOTE_BREAKPOINT    { 0xc3, 0x20 } /* Used in remote.c */
#define DEPRECATED_LITTLE_REMOTE_BREAKPOINT { 0x20, 0xc3 } /* Used in remote.c */

/*#define NOP   {0x20, 0x0b}*/ /* Who uses this???*/

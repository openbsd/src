/* Definitions to make GDB run on Convex Unix (4bsd)
   Copyright 1989, 1991, 1992, 1996  Free Software Foundation, Inc.

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

#define ATTACH_DETACH
#define HAVE_WAIT_STRUCT
#define NO_SIGINTERRUPT

/* Use SIGCONT rather than SIGTSTP because convex Unix occasionally
   turkeys SIGTSTP.  I think.  */

#define STOP_SIGNAL SIGCONT

/* Hook to call after creating inferior process.  Now init_trace_fun
   is in the same place.  So re-write this to use the init_trace_fun
   (making convex a debugging target).  FIXME.  */

#define CREATE_INFERIOR_HOOK create_inferior_hook

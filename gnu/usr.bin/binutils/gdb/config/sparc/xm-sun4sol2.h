/* Macro definitions for running gdb on a Sun 4 running Solaris 2.
   Copyright 1989, 1992 Free Software Foundation, Inc.

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

/* Most of what we know is generic to SPARC hosts.  */

#include "sparc/xm-sparc.h"

/* Pick up more stuff from the generic SVR4 host include file. */

#include "xm-sysv4.h"

/* SVR4's can't seem to agree on what to call the type that contains the
   general registers.  Kludge around it with a #define.  */

#define	gregset_t	prgregset_t
#define	fpregset_t	prfpregset_t

/* These are not currently used in SVR4 (but should be, FIXME!).  */
#undef	DO_DEFERRED_STORES
#undef	CLEAR_DEFERRED_STORES

/* May be needed, may be not?  From Pace Willisson's port.  FIXME.  */
#define NEED_POSIX_SETPGID

/* solaris doesn't have siginterrupt, though it has sigaction; however,
   in this case siginterrupt would just be setting the default. */
#define NO_SIGINTERRUPT

/* Native-dependent code for GNU/Linux UltraSPARC.

   Copyright 2003 Free Software Foundation, Inc.

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

#include "defs.h"

#include "sparc64-tdep.h"
#include "sparc-nat.h"

static const struct sparc_gregset sparc64_linux_ptrace_gregset =
{
  16 * 8,			/* "tstate" */
  17 * 8,			/* %pc */
  18 * 8,			/* %npc */
  19 * 8,			/* %y */
  -1,				/* %wim */
  -1,				/* %tbr */
  0 * 8,			/* %g1 */
  -1,				/* %l0 */
  4				/* sizeof (%y) */
};


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_sparc64_linux_nat (void);

void
_initialize_sparc64_linux_nat (void)
{
  sparc_gregset = &sparc64_linux_ptrace_gregset;
}

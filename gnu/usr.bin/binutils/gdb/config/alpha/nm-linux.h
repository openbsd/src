/* Native definitions for alpha running GNU/Linux.

   Copyright 1993, 1994, 1996, 1998, 2000, 2001, 2002, 2003
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

#ifndef NM_LINUX_H
#define NM_LINUX_H

#include "config/nm-linux.h"

/* ptrace register ``addresses'' are absolute.  */

#define U_REGS_OFFSET 0

/* FIXME: This is probably true, or should be, on all GNU/Linux ports.
   IA64?  Sparc64?  */
#define PTRACE_ARG3_TYPE long

/* ptrace transfers longs, the ptrace man page is lying.  */

#define PTRACE_XFER_TYPE long

/* The alpha does not step over a breakpoint, the manpage is lying again.  */

#define CANNOT_STEP_BREAKPOINT 1

/* Given a pointer to either a gregset_t or fpregset_t, return a
   pointer to the first register.  */
#define ALPHA_REGSET_BASE(regsetp)  ((long *) (regsetp))

/* Given a pointer to a gregset_t, locate the UNIQUE value.  */
#define ALPHA_REGSET_UNIQUE(regsetp)  ((long *)(regsetp) + 32)

/* The address of UNIQUE for ptrace.  */
#define ALPHA_UNIQUE_PTRACE_ADDR 65

#endif /* NM_LINUX_H */

/* Native-dependent definitions for Sparc running OpenBSD, for GDB.
   Copyright (C) 1986, 1987, 1989, 1992, 1995, 1996
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef NM_OBSD_H
#define NM_OBSD_H

/* Get generic OpenBSD native definitions. */

#include "nm-obsd.h"

#define FETCH_INFERIOR_REGISTERS

/* Before storing, we need to read all the registers.  */

#define CHILD_PREPARE_TO_STORE() read_register_bytes (0, NULL, REGISTER_BYTES)

/* Make things match up with what is expected in sparc-nat.c.  */

#define regs		trapframe
#define fp_status	fpstate

#define r_g1		tf_global[1]
#define r_ps		tf_psr
#define r_pc		tf_pc
#define r_npc		tf_npc
#define r_y		tf_y

#define fpu		fpstate
#define fpu_regs	fs_regs
#define fpu_fsr		fs_fsr
#define fpu_fr		fs_regs
#define Fpu_fsr		fs_fsr
#define FPU_FSR_TYPE	int

#define PTRACE_GETREGS	 PT_GETREGS
#define PTRACE_GETFPREGS PT_GETFPREGS
#define PTRACE_SETREGS	 PT_SETREGS
#define PTRACE_SETFPREGS PT_SETFPREGS

#endif /* NM_OBSD_H */

/* Target-dependent definitions for GNU/Linux MIPS.

   Copyright 2001, 2002, 2004 Free Software Foundation, Inc.

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

#ifndef TM_MIPSLINUX_H
#define TM_MIPSLINUX_H

#include "mips/tm-mips.h"

/* We don't want to inherit tm-mips.h's shared library trampoline code.  */

#undef IN_SOLIB_RETURN_TRAMPOLINE
#undef SKIP_TRAMPOLINE_CODE
#undef IGNORE_HELPER_CALL

/* GNU/Linux MIPS has __SIGRTMAX == 127.  */

#ifndef REALTIME_LO
#define REALTIME_LO 32
#define REALTIME_HI 128
#endif

#include "config/tm-linux.h"

/* We do single stepping in software.  */

#define SOFTWARE_SINGLE_STEP_P() 1
#define SOFTWARE_SINGLE_STEP(sig,bp_p) mips_software_single_step (sig, bp_p)

/* FIXME: This still needs to be implemented.  */

#undef  IN_SIGTRAMP
#define IN_SIGTRAMP(pc, name)	(0)

#undef IN_SOLIB_DYNSYM_RESOLVE_CODE
#define IN_SOLIB_DYNSYM_RESOLVE_CODE(PC) mips_linux_in_dynsym_resolve_code (PC)
int mips_linux_in_dynsym_resolve_code (CORE_ADDR pc);

/* We don't want all of tm-sysv4.h's shared library trampoline code either.
   Note that by undefining IN_SOLIB_CALL_TRAMPOLINE here we will use the
   gdbarch vector's version instead.  */

#undef IN_SOLIB_CALL_TRAMPOLINE

#endif /* TM_MIPSLINUX_H */

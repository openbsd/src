/* Macro definitions for m68k running under NetBSD.
   Copyright 1994, 1996, 2001 Free Software Foundation, Inc.

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

#ifndef TM_NBSD_H
#define TM_NBSD_H

#include <sys/param.h>
#include <machine/vmparam.h>

/* Define BPT_VECTOR if it is different than the default.
   This is the vector number used by traps to indicate a breakpoint. */
#define BPT_VECTOR		0xf
#define REMOTE_BPT_VECTOR	0xf

/* Address of end of stack space.  */
#define STACK_END_ADDR USRSTACK

/* For NetBSD, sigtramp is 32 bytes before STACK_END_ADDR.  */
#define SIGTRAMP_START(pc) (STACK_END_ADDR - 32)
#define SIGTRAMP_END(pc) (STACK_END_ADDR)

#include "m68k/tm-m68k.h"

/* Return non-zero if we are in a shared library trampoline code stub. */
#define IN_SOLIB_CALL_TRAMPOLINE(pc, name) \
  (name && !strcmp(name, "_DYNAMIC"))

extern use_struct_convention_fn m68knbsd_use_struct_convention;
#define USE_STRUCT_CONVENTION(gcc_p, type) \
        m68knbsd_use_struct_convention(gcc_p, type)

#endif /* TM_NBSD_H */

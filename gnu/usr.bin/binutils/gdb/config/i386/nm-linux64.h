/* Native support for GNU/Linux x86-64.

   Copyright 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

   Contributed by Jiri Smid, SuSE Labs.

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

#ifndef NM_LINUX64_H
#define NM_LINUX64_H

/* GNU/Linux supports the i386 hardware debugging registers.  */
#define I386_USE_GENERIC_WATCHPOINTS

#include "i386/nm-i386.h"
#include "config/nm-linux.h"

/* Support for 8-byte wide hardware watchpoints.  */
#define TARGET_HAS_DR_LEN_8 1

/* Provide access to the i386 hardware debugging registers.  */

extern void amd64_linux_dr_set_control (unsigned long control);
#define I386_DR_LOW_SET_CONTROL(control) \
  amd64_linux_dr_set_control (control)

extern void amd64_linux_dr_set_addr (int regnum, CORE_ADDR addr);
#define I386_DR_LOW_SET_ADDR(regnum, addr) \
  amd64_linux_dr_set_addr (regnum, addr)

extern void amd64_linux_dr_reset_addr (int regnum);
#define I386_DR_LOW_RESET_ADDR(regnum) \
  amd64_linux_dr_reset_addr (regnum)

extern unsigned long amd64_linux_dr_get_status (void);
#define I386_DR_LOW_GET_STATUS() \
  amd64_linux_dr_get_status ()


/* Type of the third argument to the `ptrace' system call.  */
#define PTRACE_ARG3_TYPE long

/* Type of the fourth argument to the `ptrace' system call.  */
#define PTRACE_XFER_TYPE long

/* Override copies of {fetch,store}_inferior_registers in `infptrace.c'.  */
#define FETCH_INFERIOR_REGISTERS

/* `linux-nat.c' and `i386-nat.c' have their own versions of
   child_post_startup_inferior.  Define this to use the copy in
   `x86-86-linux-nat.c' instead, which calls both.  */
#define LINUX_CHILD_POST_STARTUP_INFERIOR

#endif /* nm-linux64.h */

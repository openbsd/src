/* Native-dependent definitions for GNU/Linux SPARC.

   Copyright 1989, 1992, 1996, 1998, 1999, 2000, 2002, 2003
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

/* Type of the third argument to the `ptrace' system call.  */
#define PTRACE_ARG3_TYPE long

/* Type of the fourth argument to the `ptrace' system call.  */
#define PTRACE_XFER_TYPE long

/* Override copies of {fetch,store}_inferior_registers in `infptrace.c'.  */
#define FETCH_INFERIOR_REGISTERS

#endif /* nm-linux.h */

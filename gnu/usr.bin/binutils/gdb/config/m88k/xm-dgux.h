/* Host-machine dependent parameters for Motorola 88000, for GDB.
   Copyright 1986, 1987, 1988, 1989, 1990, 1991, 1993
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

#include "m88k/xm-m88k.h"

#define HOST_BYTE_ORDER BIG_ENDIAN

#if !defined (USG)
#define USG 1
#endif

/* Really native, but as long as only native files check this macro we
   are OK.  */
#define NO_PTRACE_H

#include <sys/param.h>

#ifdef __GNUC__
#define memcpy __builtin_memcpy
/* gcc doesn't have this, at least not gcc 1.92.  */
/* #define memset __builtin_memset */
#define strcmp __builtin_strcmp
#endif

#define x_foff _x_x._x_offset
#define x_fname _x_name
#define USER ptrace_user
#define _BSD_WAIT_FLAVOR

#define HAVE_TERMIO

#ifndef USIZE
#define USIZE 2048
#endif
#define NBPG NBPC
#define UPAGES USIZE

/* This is the amount to subtract from u.u_ar0
   to get the offset in the core file of the register values.  */

/* Since registers r0 through r31 are stored directly in the struct ptrace_user,
   (for m88k BCS)
   the ptrace_user offsets are sufficient and KERNEL_U_ADDRESS can be 0 */

#define KERNEL_U_ADDR 0

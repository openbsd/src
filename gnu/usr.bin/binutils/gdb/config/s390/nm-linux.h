/* Native support for GNU/Linux on S390.

   Copyright 2001, 2002, 2003 Free Software Foundation, Inc.

   Ported by D.J. Barrow for IBM Deutschland Entwicklung GmbH, IBM
   Corporation.  derived from i390-nmlinux.h

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


/* ptrace access.  */

#define PTRACE_ARG3_TYPE long
#define PTRACE_XFER_TYPE long

#define FETCH_INFERIOR_REGISTERS

#define KERNEL_U_SIZE kernel_u_size()
extern int kernel_u_size (void);


/* Hardware watchpoints.  */

extern int s390_stopped_by_watchpoint (void);
extern int s390_insert_watchpoint (CORE_ADDR addr, int len);
extern int s390_remove_watchpoint (CORE_ADDR addr, int len);

#define TARGET_HAS_HARDWARE_WATCHPOINTS
#define TARGET_CAN_USE_HARDWARE_WATCHPOINT(type, cnt, ot) 1
#define TARGET_REGION_OK_FOR_HW_WATCHPOINT(addr, len) 1
#define HAVE_CONTINUABLE_WATCHPOINT 1

#define STOPPED_BY_WATCHPOINT(w) \
  s390_stopped_by_watchpoint ()

#define target_insert_watchpoint(addr, len, type) \
  s390_insert_watchpoint (addr, len)

#define target_remove_watchpoint(addr, len, type) \
  s390_remove_watchpoint (addr, len)


#endif /* nm_linux.h */

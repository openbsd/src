/* Macro definitions for ns532, Mach 3.0
   Copyright (C) 1992 Free Software Foundation, Inc.

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

/* Include common definitions for Mach3 systems */
#include "nm-m3.h"

/* Define offsets to access CPROC stack when it does not have
 * a kernel thread.
 */
#define MACHINE_CPROC_SP_OFFSET 20
#define MACHINE_CPROC_PC_OFFSET 16
#define MACHINE_CPROC_FP_OFFSET 12

#include <ns532/psl.h>
#include <ns532/vmparam.h>

/* Thread flavors used in re-setting the T bit.
 * @@ this is also bad for cross debugging.
 */
#define TRACE_FLAVOR		NS532_THREAD_STATE
#define TRACE_FLAVOR_SIZE	NS532_THREAD_STATE_COUNT
#define TRACE_SET(x,state) \
  	((struct ns532_thread_state *)state)->psr |= PSR_T
#define TRACE_CLEAR(x,state) \
  	((((struct ns532_thread_state *)state)->psr &= ~PSR_T), 1)

/* we can do it */
#define ATTACH_DETACH 1

/* Address of end of stack space.
 * for MACH, see <ns532/vmparam.h>
 */
#define STACK_END_ADDR USRSTACK

#include "ns32k/tm-umax.h"

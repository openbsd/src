/* Native support for HPPA-RISC machine running HPUX, for GDB.
   Copyright 1991, 1992 Free Software Foundation, Inc. 

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

#include "somsolib.h"

#define U_REGS_OFFSET 0

#define KERNEL_U_ADDR 0

/* What a coincidence! */
#define REGISTER_U_ADDR(addr, blockend, regno)				\
{ addr = (int)(blockend) + REGISTER_BYTE (regno);}

/* HPUX 8.0, in its infinite wisdom, has chosen to prototype ptrace
   with five arguments, so programs written for normal ptrace lose.  */
#define FIVE_ARG_PTRACE

/* We need to figure out where the text region is so that we use the
   appropriate ptrace operator to manipulate text.  Simply reading/writing
   user space will crap out HPUX.  */
#define NEED_TEXT_START_END

/* This macro defines the register numbers (from REGISTER_NAMES) that
   are effectively unavailable to the user through ptrace().  It allows
   us to include the whole register set in REGISTER_NAMES (inorder to
   better support remote debugging).  If it is used in
   fetch/store_inferior_registers() gdb will not complain about I/O errors
   on fetching these registers.  If all registers in REGISTER_NAMES
   are available, then return false (0).  */

#define CANNOT_STORE_REGISTER(regno)            \
                   ((regno) == 0) ||     \
                   ((regno) == PCSQ_HEAD_REGNUM) || \
                   ((regno) >= PCSQ_TAIL_REGNUM && (regno) < IPSW_REGNUM) ||  \
                   ((regno) > IPSW_REGNUM && (regno) < FP4_REGNUM)

/* fetch_inferior_registers is in hppah-nat.c.  */
#define FETCH_INFERIOR_REGISTERS

/* child_xfer_memory is in hppah-nat.c.  */
#define CHILD_XFER_MEMORY

/* So we can cleanly use code in infptrace.c.  */
#define PT_KILL		PT_EXIT
#define PT_STEP		PT_SINGLE
#define PT_CONTINUE	PT_CONTIN
#define PT_READ_U	PT_RDUAREA
#define PT_WRITE_U	PT_WUAREA
#define PT_READ_I	PT_RIUSER
#define PT_READ_D	PT_RDUSER
#define PT_WRITE_I	PT_WIUSER
#define PT_WRITE_D	PT_WDUSER

/* attach/detach works to some extent under BSD and HPUX.  So long
   as the process you're attaching to isn't blocked waiting on io,
   blocked waiting on a signal, or in a system call things work 
   fine.  (The problems in those cases are related to the fact that
   the kernel can't provide complete register information for the
   target process...  Which really pisses off GDB.)  */

#define ATTACH_DETACH

#ifdef HAVE_HPUX_THREAD_SUPPORT

#ifdef __STDC__
struct objfile;
#endif

void hpux_thread_new_objfile PARAMS ((struct objfile *objfile));
#define target_new_objfile(OBJFILE) hpux_thread_new_objfile (OBJFILE)

extern char *hpux_pid_to_str PARAMS ((int pid));
#define target_pid_to_str(PID) hpux_pid_to_str (PID)

#endif /* HAVE_HPUX_THREAD_SUPPORT */

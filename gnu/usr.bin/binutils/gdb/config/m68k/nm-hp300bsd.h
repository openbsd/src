/* Parameters for Hewlett-Packard 9000/300 native support under bsd.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993  Free Software Foundation, Inc.

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

/* Detect whether this is 4.3 or 4.4.  */

#include <errno.h>
#include <sys/param.h>
#ifdef	BSD4_4

/* BSD 4.4 alpha or better */

/* We can attach to processes using ptrace.  */

#define	ATTACH_DETACH
#define	PTRACE_ATTACH	10
#define	PTRACE_DETACH	11

/* The third argument of ptrace is declared as this type.  */

#define	PTRACE_ARG3_TYPE	caddr_t

/* U_REGS_OFFSET is the offset of the registers within the u area for
   ptrace purposes.  */
#define U_REGS_OFFSET \
  ptrace (PT_READ_U, inferior_pid, \
	  (PTRACE_ARG3_TYPE) \
	   (offsetof (struct user, u_kproc.kp_proc.p_md.md_regs)), 0) \
    - USRSTACK

/* No user structure in 4.4, registers are relative to kernel stack
   which is fixed.  */
#define KERNEL_U_ADDR	0xFFF00000

/* FIXME: Is ONE_PROCESS_WRITETEXT still true now that the kernel has
   copy-on-write?  It not, move it to the 4.3-specific section below
   (now it is in xm-hp300bsd.h).  */

#else

/* This is BSD 4.3 or something like it.  */

/* Get kernel u area address at run-time using BSD style nlist ().  */
#define KERNEL_U_ADDR_BSD

#endif

/* This was once broken for 4.4, but probably because we had the wrong
   KERNEL_U_ADDR.  */

/* This is a piece of magic that is given a register number REGNO
   and as BLOCKEND the address in the system of the end of the user structure
   and stores in ADDR the address in the kernel or core dump
   of that register.  */

#define REGISTER_U_ADDR(addr, blockend, regno)				\
{									\
  if (regno < PS_REGNUM)						\
    addr = (int) &((struct frame *)(blockend))->f_regs[regno];		\
  else if (regno == PS_REGNUM)						\
    addr = (int) &((struct frame *)(blockend))->f_stackadj;		\
  else if (regno == PC_REGNUM)						\
    addr = (int) &((struct frame *)(blockend))->f_pc;			\
  else if (regno < FPC_REGNUM)						\
    addr = (int)							\
      &((struct user *)0)->u_pcb.pcb_fpregs.fpf_regs[((regno)-FP0_REGNUM)*3];\
  else if (regno == FPC_REGNUM)						\
    addr = (int) &((struct user *)0)->u_pcb.pcb_fpregs.fpf_fpcr;	\
  else if (regno == FPS_REGNUM)						\
    addr = (int) &((struct user *)0)->u_pcb.pcb_fpregs.fpf_fpsr;	\
  else									\
    addr = (int) &((struct user *)0)->u_pcb.pcb_fpregs.fpf_fpiar;	\
}

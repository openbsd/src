/* HP PA-RISC machine native support for BSD, for GDB.
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

#define U_REGS_OFFSET 0

#define KERNEL_U_ADDR 0

/* What a coincidence! */
#define REGISTER_U_ADDR(addr, blockend, regno)				\
{ addr = (int)(blockend) + REGISTER_BYTE (regno);}

/* 3rd argument to ptrace is supposed to be a caddr_t.  */

#define	PTRACE_ARG3_TYPE caddr_t

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

/* fetch_inferior_registers is in hppab-nat.c.  */
#define FETCH_INFERIOR_REGISTERS

/* attach/detach works to some extent under BSD and HPUX.  So long
   as the process you're attaching to isn't blocked waiting on io,
   blocked waiting on a signal, or in a system call things work 
   fine.  (The problems in those cases are related to the fact that
   the kernel can't provide complete register information for the
   target process...  Which really pisses off GDB.)  */

#define ATTACH_DETACH

/* The PA-BSD kernel has support for using the data memory break bit
   to implement fast watchpoints.

   Watchpoints on the PA act much like traditional page protection
   schemes, but with some notable differences.

   First, a special bit in the page table entry is used to cause
   a trap when a specific page is written to.  This avoids having
   to overload watchpoints on the page protection bits.  This makes
   it possible for the kernel to easily decide if a trap was caused
   by a watchpoint or by the user writing to protected memory and can
   signal the user program differently in each case.  
   
   Second, the PA has a bit in the processor status word which causes
   data memory breakpoints (aka watchpoints) to be disabled for a single
   instruction.  This bit can be used to avoid the overhead of unprotecting
   and reprotecting pages when it becomes necessary to step over a watchpoint.


   When the kernel receives a trap indicating a write to a page which
   is being watched, the kernel performs a couple of simple actions.  First
   is sets the magic "disable memory breakpoint" bit in the processor
   status word, it then sends a SIGTRAP to the process which caused the
   trap.

   GDB will take control and catch the signal for the inferior.  GDB then
   examines the PSW-X bit to determine if the SIGTRAP was caused by a 
   watchpoint firing.  If so GDB single steps the inferior over the
   instruction which caused the watchpoint to trigger (note because the
   kernel disabled the data memory break bit for one instruction no trap
   will be taken!).  GDB will then determines the appropriate action to
   take.  (this may include restarting the inferior if the watchpoint
   fired because of a write to an address on the same page as a watchpoint,
   but no write to the watched address occured).  */

#define TARGET_HAS_HARDWARE_WATCHPOINTS	/* Enable the code in procfs.c */

/* The PA can watch any number of locations, there's no need for it to reject
   anything (generic routines already check that all intermediates are
   in memory).  */
#define TARGET_CAN_USE_HARDWARE_WATCHPOINT(type, cnt, ot) \
	((type) == bp_hardware_watchpoint)

/* When a hardware watchpoint fires off the PC will be left at the
   instruction which caused the watchpoint.  It will be necessary for
   GDB to step over the watchpoint.

   On a PA running BSD, it is trivial to identify when it will be
   necessary to step over a hardware watchpoint as we can examine
   the PSW-X bit.  If the bit is on, then we trapped because of a 
   watchpoint, else we trapped for some other reason.  */
#define STOPPED_BY_WATCHPOINT(W) \
  ((W).kind == TARGET_WAITKIND_STOPPED \
   && (W).value.sig == TARGET_SIGNAL_TRAP \
   && ((int) read_register (IPSW_REGNUM) & 0x00100000))

/* The PA can single step over a watchpoint if the kernel has set the
   "X" bit in the processor status word (disable data memory breakpoint
   for one instruction).

   The kernel will always set this bit before notifying the inferior
   that it hit a watchpoint.  Thus, the inferior can single step over
   the instruction which caused the watchpoint to fire.  This avoids
   the traditional need to disable the watchpoint, step the inferior,
   then enable the watchpoint again.  */
#define HAVE_STEPPABLE_WATCHPOINT

/* Use these macros for watchpoint insertion/deletion.  */
/* type can be 0: write watch, 1: read watch, 2: access watch (read/write) */
#define target_insert_watchpoint(addr, len, type) hppa_set_watchpoint (addr, len, 1)
#define target_remove_watchpoint(addr, len, type) hppa_set_watchpoint (addr, len, 0)

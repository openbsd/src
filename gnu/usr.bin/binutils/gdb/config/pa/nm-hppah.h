/* Native support for HPPA-RISC machine running HPUX, for GDB.
   Copyright 1991, 1992, 1994, 1996, 1998, 1999, 2000, 2002
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

struct target_ops;

#define U_REGS_OFFSET 0

#define KERNEL_U_ADDR 0

/* What a coincidence! */
#define REGISTER_U_ADDR(addr, blockend, regno)				\
{ addr = (int)(blockend) + DEPRECATED_REGISTER_BYTE (regno);}

/* This isn't really correct, because ptrace is actually a 32-bit
   interface.  However, the modern HP-UX targets all really use
   ttrace, which is a 64-bit interface --- a debugger running in
   either 32- or 64-bit mode can debug a 64-bit process.  BUT, the
   code doesn't use ttrace directly --- it calls call_ptrace instead,
   which is supposed to be drop-in substitute for ptrace.  In other
   words, they access a 64-bit system call (ttrace) through a
   compatibility layer which is allegedly a 32-bit interface.

   So I don't feel the least bit guilty about this.  */
#define PTRACE_ARG3_TYPE CORE_ADDR

/* HPUX 8.0, in its infinite wisdom, has chosen to prototype ptrace
   with five arguments, so programs written for normal ptrace lose.  */
#define FIVE_ARG_PTRACE

/* We need to figure out where the text region is so that we use the
   appropriate ptrace operator to manipulate text.  Simply
   reading/writing user space will crap out HPUX.  */
#define DEPRECATED_HPUX_TEXT_END deprecated_hpux_text_end
extern void deprecated_hpux_text_end (struct target_ops *exec_ops);

/* In hppah-nat.c: */
#define FETCH_INFERIOR_REGISTERS
#define CHILD_XFER_MEMORY
#define CHILD_FOLLOW_FORK

/* In infptrace.c or infttrace.c: */
#define CHILD_PID_TO_EXEC_FILE
#define CHILD_POST_STARTUP_INFERIOR
#define CHILD_ACKNOWLEDGE_CREATED_INFERIOR
#define CHILD_INSERT_FORK_CATCHPOINT
#define CHILD_REMOVE_FORK_CATCHPOINT
#define CHILD_INSERT_VFORK_CATCHPOINT
#define CHILD_REMOVE_VFORK_CATCHPOINT
#define CHILD_INSERT_EXEC_CATCHPOINT
#define CHILD_REMOVE_EXEC_CATCHPOINT
#define CHILD_REPORTED_EXEC_EVENTS_PER_EXEC_CALL
#define CHILD_POST_ATTACH
#define CHILD_THREAD_ALIVE
#define CHILD_PID_TO_STR
#define CHILD_WAIT
struct target_waitstatus;
extern ptid_t child_wait (ptid_t, struct target_waitstatus *);

extern int hppa_require_attach (int);
extern int hppa_require_detach (int, int);

/* So we can cleanly use code in infptrace.c.  */
#define PT_KILL		PT_EXIT
#define PT_STEP		PT_SINGLE
#define PT_CONTINUE	PT_CONTIN

/* FIXME HP MERGE : Previously, PT_RDUAREA. this is actually fixed
   in gdb-hp-snapshot-980509  */
#define PT_READ_U	PT_RUAREA
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

/* In infptrace or infttrace.c: */

/* Starting with HP-UX 10.30, support is provided (in the form of
   ttrace requests) for memory-protection-based hardware watchpoints.

   The 10.30 implementation of these functions reside in infttrace.c.

   Stubs of these functions will be provided in infptrace.c, so that
   10.20 will at least link.  However, the "can I use a fast watchpoint?"
   query will always return "No" for 10.20. */

#define TARGET_HAS_HARDWARE_WATCHPOINTS

/* The PA can watch any number of locations (generic routines already check
   that all intermediates are in watchable memory locations). */
extern int hppa_can_use_hw_watchpoint (int type, int cnt, int ot);
#define TARGET_CAN_USE_HARDWARE_WATCHPOINT(type, cnt, ot) \
        hppa_can_use_hw_watchpoint(type, cnt, ot)

/* The PA can also watch memory regions of arbitrary size, since we're
   using a page-protection scheme.  (On some targets, apparently watch
   registers are used, which can only accomodate regions of
   DEPRECATED_REGISTER_SIZE.)  */
#define TARGET_REGION_SIZE_OK_FOR_HW_WATCHPOINT(byte_count) \
        (1)

/* However, some addresses may not be profitable to use hardware to watch,
   or may be difficult to understand when the addressed object is out of
   scope, and hence should be unwatched.  On some targets, this may have
   severe performance penalties, such that we might as well use regular
   watchpoints, and save (possibly precious) hardware watchpoints for other
   locations.

   On HP-UX, we choose not to watch stack-based addresses, because

   [1] Our implementation relies on page protection traps.  The granularity
   of these is large and so can generate many false hits, which are expensive
   to respond to.

   [2] Watches of "*p" where we may not know the symbol that p points to,
   make it difficult to know when the addressed object is out of scope, and
   hence shouldn't be watched.  Page protection that isn't removed when the
   addressed object is out of scope will either degrade execution speed
   (false hits) or give false triggers (when the address is recycled by
   other calls).

   Since either of these points results in a slow-running inferior, we might
   as well use normal watchpoints, aka single-step & test. */
#define TARGET_RANGE_PROFITABLE_FOR_HW_WATCHPOINT(pid,start,len) \
        hppa_range_profitable_for_hw_watchpoint(pid, start, (LONGEST)(len))

/* On HP-UX, we're using page-protection to implement hardware watchpoints.
   When an instruction attempts to write to a write-protected memory page,
   a SIGBUS is raised.  At that point, the write has not actually occurred.

   We must therefore remove page-protections; single-step the inferior (to
   allow the write to happen); restore page-protections; and check whether
   any watchpoint triggered.

   If none did, then the write was to a "nearby" location that just happens
   to fall on the same page as a watched location, and so can be ignored.

   The only intended client of this macro is wait_for_inferior(), in infrun.c.
   When HAVE_NONSTEPPABLE_WATCHPOINT is true, that function will take care
   of the stepping & etc. */

#define STOPPED_BY_WATCHPOINT(W) \
        ((W.kind == TARGET_WAITKIND_STOPPED) && \
         (stop_signal == TARGET_SIGNAL_BUS) && \
         ! stepped_after_stopped_by_watchpoint && \
         bpstat_have_active_hw_watchpoints ())

/* Our implementation of "hardware" watchpoints uses memory page-protection
   faults.  However, HP-UX has unfortunate interactions between these and
   system calls; basically, it's unsafe to have page protections on when a
   syscall is running.  Therefore, we also ask for notification of syscall
   entries and returns.  When the inferior enters a syscall, we disable
   h/w watchpoints.  When the inferior returns from a syscall, we reenable
   h/w watchpoints.

   infptrace.c supplies dummy versions of these; infttrace.c is where the
   meaningful implementations are.
 */
#define TARGET_ENABLE_HW_WATCHPOINTS(pid) \
        hppa_enable_page_protection_events (pid)
extern void hppa_enable_page_protection_events (int);

#define TARGET_DISABLE_HW_WATCHPOINTS(pid) \
        hppa_disable_page_protection_events (pid)
extern void hppa_disable_page_protection_events (int);

/* Use these macros for watchpoint insertion/deletion.  */
extern int hppa_insert_hw_watchpoint (int pid, CORE_ADDR start, LONGEST len,
				      int type);
#define target_insert_watchpoint(addr, len, type) \
        hppa_insert_hw_watchpoint (PIDGET (inferior_ptid), addr, (LONGEST)(len), type)

extern int hppa_remove_hw_watchpoint (int pid, CORE_ADDR start, LONGEST len,
				      int type);
#define target_remove_watchpoint(addr, len, type) \
        hppa_remove_hw_watchpoint (PIDGET (inferior_ptid), addr, (LONGEST)(len), type)

/* We call our k-thread processes "threads", rather
 * than processes.  So we need a new way to print
 * the string.  Code is in hppah-nat.c.
 */

extern char *child_pid_to_str (ptid_t);

#define target_tid_to_str( ptid ) \
        hppa_tid_to_str( ptid )
extern char *hppa_tid_to_str (ptid_t);

/* For this, ID can be either a process or thread ID, and the function
   will describe it appropriately, returning the description as a printable
   string.

   The function that implements this macro is defined in infptrace.c and
   infttrace.c.
 */
#define target_pid_or_tid_to_str(ID) \
        hppa_pid_or_tid_to_str (ID)
extern char *hppa_pid_or_tid_to_str (ptid_t);

/* This is used when handling events caused by a call to vfork().  On ptrace-
   based HP-UXs, when you resume the vforked child, the parent automagically
   begins running again.  To prevent this runaway, this function is used.

   Note that for vfork on HP-UX, we receive three events of interest:

   1. the vfork event for the new child process
   2. the exit or exec event of the new child process (actually, you get
   two exec events on ptrace-based HP-UXs)
   3. the vfork event for the original parent process

   The first is always received first.  The other two may be received in any
   order; HP-UX doesn't guarantee an order.
 */
#define ENSURE_VFORKING_PARENT_REMAINS_STOPPED(PID) \
        hppa_ensure_vforking_parent_remains_stopped (PID)
extern void hppa_ensure_vforking_parent_remains_stopped (int);

/* This is used when handling events caused by a call to vfork().

   On ttrace-based HP-UXs, the parent vfork and child exec arrive more or less
   together.  That is, you could do two wait()s without resuming either parent
   or child, and get both events.

   On ptrace-based HP-UXs, you must resume the child after its exec event is
   delivered or you won't get the parent's vfork.  I.e., you can't just wait()
   and get the parent vfork, after receiving the child exec.
 */
#define RESUME_EXECD_VFORKING_CHILD_TO_GET_PARENT_VFORK() \
        hppa_resume_execd_vforking_child_to_get_parent_vfork ()
extern int hppa_resume_execd_vforking_child_to_get_parent_vfork (void);

#define HPUXHPPA

#define MAY_SWITCH_FROM_INFERIOR_PID (1)

#define MAY_FOLLOW_EXEC (1)

#define USE_THREAD_STEP_NEEDED (1)

#include "infttrace.h" /* For parent_attach_all.  */

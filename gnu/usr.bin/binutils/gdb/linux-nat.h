/* Native debugging support for GNU/Linux (LWP layer).
   Copyright 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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

#include "target.h"

/* Structure describing an LWP.  */

struct lwp_info
{
  /* The process id of the LWP.  This is a combination of the LWP id
     and overall process id.  */
  ptid_t ptid;

  /* Non-zero if this LWP is cloned.  In this context "cloned" means
     that the LWP is reporting to its parent using a signal other than
     SIGCHLD.  */
  int cloned;

  /* Non-zero if we sent this LWP a SIGSTOP (but the LWP didn't report
     it back yet).  */
  int signalled;

  /* Non-zero if this LWP is stopped.  */
  int stopped;

  /* Non-zero if this LWP will be/has been resumed.  Note that an LWP
     can be marked both as stopped and resumed at the same time.  This
     happens if we try to resume an LWP that has a wait status
     pending.  We shouldn't let the LWP run until that wait status has
     been processed, but we should not report that wait status if GDB
     didn't try to let the LWP run.  */
  int resumed;

  /* If non-zero, a pending wait status.  */
  int status;

  /* Non-zero if we were stepping this LWP.  */
  int step;

  /* If WAITSTATUS->KIND != TARGET_WAITKIND_SPURIOUS, the waitstatus
     for this LWP's last event.  This may correspond to STATUS above,
     or to a local variable in lin_lwp_wait.  */
  struct target_waitstatus waitstatus;

  /* Next LWP in list.  */
  struct lwp_info *next;
};

/* Read/write to target memory via the Linux kernel's "proc file
   system".  */
struct mem_attrib;
struct target_ops;

extern int linux_proc_xfer_memory (CORE_ADDR addr, char *myaddr, int len,
				   int write, struct mem_attrib *attrib,
				   struct target_ops *target);

/* Find process PID's pending signal set from /proc/pid/status.  */
void linux_proc_pending_signals (int pid, sigset_t *pending, sigset_t *blocked, sigset_t *ignored);

/* linux-nat functions for handling fork events.  */
extern void linux_record_stopped_pid (int pid);
extern void linux_enable_event_reporting (ptid_t ptid);
extern ptid_t linux_handle_extended_wait (int pid, int status,
					  struct target_waitstatus *ourstatus);
extern void linux_child_post_startup_inferior (ptid_t ptid);

/* Iterator function for lin-lwp's lwp list.  */
struct lwp_info *iterate_over_lwps (int (*callback) (struct lwp_info *, 
						     void *), 
				    void *data);

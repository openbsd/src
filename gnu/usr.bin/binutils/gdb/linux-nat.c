/* GNU/Linux native-dependent code common to multiple platforms.
   Copyright (C) 2003 Free Software Foundation, Inc.

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

#include "defs.h"
#include "inferior.h"
#include "target.h"

#include "gdb_wait.h"
#include <sys/ptrace.h>

#include "linux-nat.h"

/* If the system headers did not provide the constants, hard-code the normal
   values.  */
#ifndef PTRACE_EVENT_FORK

#define PTRACE_SETOPTIONS	0x4200
#define PTRACE_GETEVENTMSG	0x4201

/* options set using PTRACE_SETOPTIONS */
#define PTRACE_O_TRACESYSGOOD	0x00000001
#define PTRACE_O_TRACEFORK	0x00000002
#define PTRACE_O_TRACEVFORK	0x00000004
#define PTRACE_O_TRACECLONE	0x00000008
#define PTRACE_O_TRACEEXEC	0x00000010
#define PTRACE_O_TRACEVFORKDONE	0x00000020
#define PTRACE_O_TRACEEXIT	0x00000040

/* Wait extended result codes for the above trace options.  */
#define PTRACE_EVENT_FORK	1
#define PTRACE_EVENT_VFORK	2
#define PTRACE_EVENT_CLONE	3
#define PTRACE_EVENT_EXEC	4
#define PTRACE_EVENT_VFORKDONE	5
#define PTRACE_EVENT_EXIT	6

#endif /* PTRACE_EVENT_FORK */

/* We can't always assume that this flag is available, but all systems
   with the ptrace event handlers also have __WALL, so it's safe to use
   here.  */
#ifndef __WALL
#define __WALL          0x40000000 /* Wait for any child.  */
#endif

extern struct target_ops child_ops;

static int linux_parent_pid;

struct simple_pid_list
{
  int pid;
  struct simple_pid_list *next;
};
struct simple_pid_list *stopped_pids;

/* This variable is a tri-state flag: -1 for unknown, 0 if PTRACE_O_TRACEFORK
   can not be used, 1 if it can.  */

static int linux_supports_tracefork_flag = -1;

/* If we have PTRACE_O_TRACEFORK, this flag indicates whether we also have
   PTRACE_O_TRACEVFORKDONE.  */

static int linux_supports_tracevforkdone_flag = -1;


/* Trivial list manipulation functions to keep track of a list of
   new stopped processes.  */
static void
add_to_pid_list (struct simple_pid_list **listp, int pid)
{
  struct simple_pid_list *new_pid = xmalloc (sizeof (struct simple_pid_list));
  new_pid->pid = pid;
  new_pid->next = *listp;
  *listp = new_pid;
}

static int
pull_pid_from_list (struct simple_pid_list **listp, int pid)
{
  struct simple_pid_list **p;

  for (p = listp; *p != NULL; p = &(*p)->next)
    if ((*p)->pid == pid)
      {
	struct simple_pid_list *next = (*p)->next;
	xfree (*p);
	*p = next;
	return 1;
      }
  return 0;
}

void
linux_record_stopped_pid (int pid)
{
  add_to_pid_list (&stopped_pids, pid);
}


/* A helper function for linux_test_for_tracefork, called after fork ().  */

static void
linux_tracefork_child (void)
{
  int ret;

  ptrace (PTRACE_TRACEME, 0, 0, 0);
  kill (getpid (), SIGSTOP);
  fork ();
  exit (0);
}

/* Determine if PTRACE_O_TRACEFORK can be used to follow fork events.  We
   create a child process, attach to it, use PTRACE_SETOPTIONS to enable
   fork tracing, and let it fork.  If the process exits, we assume that
   we can't use TRACEFORK; if we get the fork notification, and we can
   extract the new child's PID, then we assume that we can.  */

static void
linux_test_for_tracefork (void)
{
  int child_pid, ret, status;
  long second_pid;

  child_pid = fork ();
  if (child_pid == -1)
    perror_with_name ("linux_test_for_tracefork: fork");

  if (child_pid == 0)
    linux_tracefork_child ();

  ret = waitpid (child_pid, &status, 0);
  if (ret == -1)
    perror_with_name ("linux_test_for_tracefork: waitpid");
  else if (ret != child_pid)
    error ("linux_test_for_tracefork: waitpid: unexpected result %d.", ret);
  if (! WIFSTOPPED (status))
    error ("linux_test_for_tracefork: waitpid: unexpected status %d.", status);

  linux_supports_tracefork_flag = 0;

  ret = ptrace (PTRACE_SETOPTIONS, child_pid, 0, PTRACE_O_TRACEFORK);
  if (ret != 0)
    {
      ptrace (PTRACE_KILL, child_pid, 0, 0);
      waitpid (child_pid, &status, 0);
      return;
    }

  /* Check whether PTRACE_O_TRACEVFORKDONE is available.  */
  ret = ptrace (PTRACE_SETOPTIONS, child_pid, 0,
		PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORKDONE);
  linux_supports_tracevforkdone_flag = (ret == 0);

  ptrace (PTRACE_CONT, child_pid, 0, 0);
  ret = waitpid (child_pid, &status, 0);
  if (ret == child_pid && WIFSTOPPED (status)
      && status >> 16 == PTRACE_EVENT_FORK)
    {
      second_pid = 0;
      ret = ptrace (PTRACE_GETEVENTMSG, child_pid, 0, &second_pid);
      if (ret == 0 && second_pid != 0)
	{
	  int second_status;

	  linux_supports_tracefork_flag = 1;
	  waitpid (second_pid, &second_status, 0);
	  ptrace (PTRACE_DETACH, second_pid, 0, 0);
	}
    }

  if (WIFSTOPPED (status))
    {
      ptrace (PTRACE_DETACH, child_pid, 0, 0);
      waitpid (child_pid, &status, 0);
    }
}

/* Return non-zero iff we have tracefork functionality available.
   This function also sets linux_supports_tracefork_flag.  */

static int
linux_supports_tracefork (void)
{
  if (linux_supports_tracefork_flag == -1)
    linux_test_for_tracefork ();
  return linux_supports_tracefork_flag;
}

static int
linux_supports_tracevforkdone (void)
{
  if (linux_supports_tracefork_flag == -1)
    linux_test_for_tracefork ();
  return linux_supports_tracevforkdone_flag;
}


void
linux_enable_event_reporting (ptid_t ptid)
{
  int pid = ptid_get_pid (ptid);
  int options;

  if (! linux_supports_tracefork ())
    return;

  options = PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | PTRACE_O_TRACEEXEC;
  if (linux_supports_tracevforkdone ())
    options |= PTRACE_O_TRACEVFORKDONE;

  /* Do not enable PTRACE_O_TRACEEXIT until GDB is more prepared to support
     read-only process state.  */

  ptrace (PTRACE_SETOPTIONS, pid, 0, options);
}

void
child_post_attach (int pid)
{
  linux_enable_event_reporting (pid_to_ptid (pid));
}

void
linux_child_post_startup_inferior (ptid_t ptid)
{
  linux_enable_event_reporting (ptid);
}

#ifndef LINUX_CHILD_POST_STARTUP_INFERIOR
void
child_post_startup_inferior (ptid_t ptid)
{
  linux_child_post_startup_inferior (ptid);
}
#endif

int
child_follow_fork (int follow_child)
{
  ptid_t last_ptid;
  struct target_waitstatus last_status;
  int has_vforked;
  int parent_pid, child_pid;

  get_last_target_status (&last_ptid, &last_status);
  has_vforked = (last_status.kind == TARGET_WAITKIND_VFORKED);
  parent_pid = ptid_get_pid (last_ptid);
  child_pid = last_status.value.related_pid;

  if (! follow_child)
    {
      /* We're already attached to the parent, by default. */

      /* Before detaching from the child, remove all breakpoints from
         it.  (This won't actually modify the breakpoint list, but will
         physically remove the breakpoints from the child.) */
      /* If we vforked this will remove the breakpoints from the parent
	 also, but they'll be reinserted below.  */
      detach_breakpoints (child_pid);

      fprintf_filtered (gdb_stdout,
			"Detaching after fork from child process %d.\n",
			child_pid);

      ptrace (PTRACE_DETACH, child_pid, 0, 0);

      if (has_vforked)
	{
	  if (linux_supports_tracevforkdone ())
	    {
	      int status;

	      ptrace (PTRACE_CONT, parent_pid, 0, 0);
	      waitpid (parent_pid, &status, __WALL);
	      if ((status >> 16) != PTRACE_EVENT_VFORKDONE)
		warning ("Unexpected waitpid result %06x when waiting for "
			 "vfork-done", status);
	    }
	  else
	    {
	      /* We can't insert breakpoints until the child has
		 finished with the shared memory region.  We need to
		 wait until that happens.  Ideal would be to just
		 call:
		 - ptrace (PTRACE_SYSCALL, parent_pid, 0, 0);
		 - waitpid (parent_pid, &status, __WALL);
		 However, most architectures can't handle a syscall
		 being traced on the way out if it wasn't traced on
		 the way in.

		 We might also think to loop, continuing the child
		 until it exits or gets a SIGTRAP.  One problem is
		 that the child might call ptrace with PTRACE_TRACEME.

		 There's no simple and reliable way to figure out when
		 the vforked child will be done with its copy of the
		 shared memory.  We could step it out of the syscall,
		 two instructions, let it go, and then single-step the
		 parent once.  When we have hardware single-step, this
		 would work; with software single-step it could still
		 be made to work but we'd have to be able to insert
		 single-step breakpoints in the child, and we'd have
		 to insert -just- the single-step breakpoint in the
		 parent.  Very awkward.

		 In the end, the best we can do is to make sure it
		 runs for a little while.  Hopefully it will be out of
		 range of any breakpoints we reinsert.  Usually this
		 is only the single-step breakpoint at vfork's return
		 point.  */

	      usleep (10000);
	    }

	  /* Since we vforked, breakpoints were removed in the parent
	     too.  Put them back.  */
	  reattach_breakpoints (parent_pid);
	}
    }
  else
    {
      char child_pid_spelling[40];

      /* Needed to keep the breakpoint lists in sync.  */
      if (! has_vforked)
	detach_breakpoints (child_pid);

      /* Before detaching from the parent, remove all breakpoints from it. */
      remove_breakpoints ();

      fprintf_filtered (gdb_stdout,
			"Attaching after fork to child process %d.\n",
			child_pid);

      /* If we're vforking, we may want to hold on to the parent until
	 the child exits or execs.  At exec time we can remove the old
	 breakpoints from the parent and detach it; at exit time we
	 could do the same (or even, sneakily, resume debugging it - the
	 child's exec has failed, or something similar).

	 This doesn't clean up "properly", because we can't call
	 target_detach, but that's OK; if the current target is "child",
	 then it doesn't need any further cleanups, and lin_lwp will
	 generally not encounter vfork (vfork is defined to fork
	 in libpthread.so).

	 The holding part is very easy if we have VFORKDONE events;
	 but keeping track of both processes is beyond GDB at the
	 moment.  So we don't expose the parent to the rest of GDB.
	 Instead we quietly hold onto it until such time as we can
	 safely resume it.  */

      if (has_vforked)
	linux_parent_pid = parent_pid;
      else
	target_detach (NULL, 0);

      inferior_ptid = pid_to_ptid (child_pid);
      push_target (&child_ops);

      /* Reset breakpoints in the child as appropriate.  */
      follow_inferior_reset_breakpoints ();
    }

  return 0;
}

ptid_t
linux_handle_extended_wait (int pid, int status,
			    struct target_waitstatus *ourstatus)
{
  int event = status >> 16;

  if (event == PTRACE_EVENT_CLONE)
    internal_error (__FILE__, __LINE__,
		    "unexpected clone event");

  if (event == PTRACE_EVENT_FORK || event == PTRACE_EVENT_VFORK)
    {
      unsigned long new_pid;
      int ret;

      ptrace (PTRACE_GETEVENTMSG, pid, 0, &new_pid);

      /* If we haven't already seen the new PID stop, wait for it now.  */
      if (! pull_pid_from_list (&stopped_pids, new_pid))
	{
	  /* The new child has a pending SIGSTOP.  We can't affect it until it
	     hits the SIGSTOP, but we're already attached.

	     It won't be a clone (we didn't ask for clones in the event mask)
	     so we can just call waitpid and wait for the SIGSTOP.  */
	  do {
	    ret = waitpid (new_pid, &status, 0);
	  } while (ret == -1 && errno == EINTR);
	  if (ret == -1)
	    perror_with_name ("waiting for new child");
	  else if (ret != new_pid)
	    internal_error (__FILE__, __LINE__,
			    "wait returned unexpected PID %d", ret);
	  else if (!WIFSTOPPED (status) || WSTOPSIG (status) != SIGSTOP)
	    internal_error (__FILE__, __LINE__,
			    "wait returned unexpected status 0x%x", status);
	}

      ourstatus->kind = (event == PTRACE_EVENT_FORK)
	? TARGET_WAITKIND_FORKED : TARGET_WAITKIND_VFORKED;
      ourstatus->value.related_pid = new_pid;
      return inferior_ptid;
    }

  if (event == PTRACE_EVENT_EXEC)
    {
      ourstatus->kind = TARGET_WAITKIND_EXECD;
      ourstatus->value.execd_pathname
	= xstrdup (child_pid_to_exec_file (pid));

      if (linux_parent_pid)
	{
	  detach_breakpoints (linux_parent_pid);
	  ptrace (PTRACE_DETACH, linux_parent_pid, 0, 0);

	  linux_parent_pid = 0;
	}

      return inferior_ptid;
    }

  internal_error (__FILE__, __LINE__,
		  "unknown ptrace event %d", event);
}


int
child_insert_fork_catchpoint (int pid)
{
  if (! linux_supports_tracefork ())
    error ("Your system does not support fork catchpoints.");

  return 0;
}

int
child_insert_vfork_catchpoint (int pid)
{
  if (!linux_supports_tracefork ())
    error ("Your system does not support vfork catchpoints.");

  return 0;
}

int
child_insert_exec_catchpoint (int pid)
{
  if (!linux_supports_tracefork ())
    error ("Your system does not support exec catchpoints.");

  return 0;
}

void
kill_inferior (void)
{
  int status;
  int pid =  PIDGET (inferior_ptid);
  struct target_waitstatus last;
  ptid_t last_ptid;
  int ret;

  if (pid == 0)
    return;

  /* If we're stopped while forking and we haven't followed yet, kill the
     other task.  We need to do this first because the parent will be
     sleeping if this is a vfork.  */

  get_last_target_status (&last_ptid, &last);

  if (last.kind == TARGET_WAITKIND_FORKED
      || last.kind == TARGET_WAITKIND_VFORKED)
    {
      ptrace (PT_KILL, last.value.related_pid);
      ptrace_wait (null_ptid, &status);
    }

  /* Kill the current process.  */
  ptrace (PT_KILL, pid, (PTRACE_ARG3_TYPE) 0, 0);
  ret = ptrace_wait (null_ptid, &status);

  /* We might get a SIGCHLD instead of an exit status.  This is
     aggravated by the first kill above - a child has just died.  */

  while (ret == pid && WIFSTOPPED (status))
    {
      ptrace (PT_KILL, pid, (PTRACE_ARG3_TYPE) 0, 0);
      ret = ptrace_wait (null_ptid, &status);
    }

  target_mourn_inferior ();
}

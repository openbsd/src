/* GNU/Linux native-dependent code common to multiple platforms.

   Copyright 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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
#include "gdb_string.h"
#include "gdb_wait.h"
#include "gdb_assert.h"
#ifdef HAVE_TKILL_SYSCALL
#include <unistd.h>
#include <sys/syscall.h>
#endif
#include <sys/ptrace.h>
#include "linux-nat.h"
#include "gdbthread.h"
#include "gdbcmd.h"
#include "regcache.h"
#include <sys/param.h>		/* for MAXPATHLEN */
#include <sys/procfs.h>		/* for elf_gregset etc. */
#include "elf-bfd.h"		/* for elfcore_write_* */
#include "gregset.h"		/* for gregset */
#include "gdbcore.h"		/* for get_exec_file */
#include <ctype.h>		/* for isdigit */
#include "gdbthread.h"		/* for struct thread_info etc. */
#include "gdb_stat.h"		/* for struct stat */
#include <fcntl.h>		/* for O_RDONLY */

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

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

static int debug_linux_nat;

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

  options = PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | PTRACE_O_TRACEEXEC
    | PTRACE_O_TRACECLONE;
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
      push_target (&deprecated_child_ops);

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

  if (event == PTRACE_EVENT_FORK || event == PTRACE_EVENT_VFORK
      || event == PTRACE_EVENT_CLONE)
    {
      unsigned long new_pid;
      int ret;

      ptrace (PTRACE_GETEVENTMSG, pid, 0, &new_pid);

      /* If we haven't already seen the new PID stop, wait for it now.  */
      if (! pull_pid_from_list (&stopped_pids, new_pid))
	{
	  /* The new child has a pending SIGSTOP.  We can't affect it until it
	     hits the SIGSTOP, but we're already attached.  */
	  do {
	    ret = waitpid (new_pid, &status,
			   (event == PTRACE_EVENT_CLONE) ? __WCLONE : 0);
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

      if (event == PTRACE_EVENT_FORK)
	ourstatus->kind = TARGET_WAITKIND_FORKED;
      else if (event == PTRACE_EVENT_VFORK)
	ourstatus->kind = TARGET_WAITKIND_VFORKED;
      else
	ourstatus->kind = TARGET_WAITKIND_SPURIOUS;

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
      ptrace (PT_KILL, last.value.related_pid, 0, 0);
      wait (&status);
    }

  /* Kill the current process.  */
  ptrace (PT_KILL, pid, 0, 0);
  ret = wait (&status);

  /* We might get a SIGCHLD instead of an exit status.  This is
     aggravated by the first kill above - a child has just died.  */

  while (ret == pid && WIFSTOPPED (status))
    {
      ptrace (PT_KILL, pid, 0, 0);
      ret = wait (&status);
    }

  target_mourn_inferior ();
}

/* On GNU/Linux there are no real LWP's.  The closest thing to LWP's
   are processes sharing the same VM space.  A multi-threaded process
   is basically a group of such processes.  However, such a grouping
   is almost entirely a user-space issue; the kernel doesn't enforce
   such a grouping at all (this might change in the future).  In
   general, we'll rely on the threads library (i.e. the GNU/Linux
   Threads library) to provide such a grouping.

   It is perfectly well possible to write a multi-threaded application
   without the assistance of a threads library, by using the clone
   system call directly.  This module should be able to give some
   rudimentary support for debugging such applications if developers
   specify the CLONE_PTRACE flag in the clone system call, and are
   using the Linux kernel 2.4 or above.

   Note that there are some peculiarities in GNU/Linux that affect
   this code:

   - In general one should specify the __WCLONE flag to waitpid in
     order to make it report events for any of the cloned processes
     (and leave it out for the initial process).  However, if a cloned
     process has exited the exit status is only reported if the
     __WCLONE flag is absent.  Linux kernel 2.4 has a __WALL flag, but
     we cannot use it since GDB must work on older systems too.

   - When a traced, cloned process exits and is waited for by the
     debugger, the kernel reassigns it to the original parent and
     keeps it around as a "zombie".  Somehow, the GNU/Linux Threads
     library doesn't notice this, which leads to the "zombie problem":
     When debugged a multi-threaded process that spawns a lot of
     threads will run out of processes, even if the threads exit,
     because the "zombies" stay around.  */

/* List of known LWPs.  */
static struct lwp_info *lwp_list;

/* Number of LWPs in the list.  */
static int num_lwps;

/* Non-zero if we're running in "threaded" mode.  */
static int threaded;


#define GET_LWP(ptid)		ptid_get_lwp (ptid)
#define GET_PID(ptid)		ptid_get_pid (ptid)
#define is_lwp(ptid)		(GET_LWP (ptid) != 0)
#define BUILD_LWP(lwp, pid)	ptid_build (pid, lwp, 0)

/* If the last reported event was a SIGTRAP, this variable is set to
   the process id of the LWP/thread that got it.  */
ptid_t trap_ptid;


/* This module's target-specific operations.  */
static struct target_ops linux_nat_ops;

/* Since we cannot wait (in linux_nat_wait) for the initial process and
   any cloned processes with a single call to waitpid, we have to use
   the WNOHANG flag and call waitpid in a loop.  To optimize
   things a bit we use `sigsuspend' to wake us up when a process has
   something to report (it will send us a SIGCHLD if it has).  To make
   this work we have to juggle with the signal mask.  We save the
   original signal mask such that we can restore it before creating a
   new process in order to avoid blocking certain signals in the
   inferior.  We then block SIGCHLD during the waitpid/sigsuspend
   loop.  */

/* Original signal mask.  */
static sigset_t normal_mask;

/* Signal mask for use with sigsuspend in linux_nat_wait, initialized in
   _initialize_linux_nat.  */
static sigset_t suspend_mask;

/* Signals to block to make that sigsuspend work.  */
static sigset_t blocked_mask;


/* Prototypes for local functions.  */
static int stop_wait_callback (struct lwp_info *lp, void *data);
static int linux_nat_thread_alive (ptid_t ptid);

/* Convert wait status STATUS to a string.  Used for printing debug
   messages only.  */

static char *
status_to_str (int status)
{
  static char buf[64];

  if (WIFSTOPPED (status))
    snprintf (buf, sizeof (buf), "%s (stopped)",
	      strsignal (WSTOPSIG (status)));
  else if (WIFSIGNALED (status))
    snprintf (buf, sizeof (buf), "%s (terminated)",
	      strsignal (WSTOPSIG (status)));
  else
    snprintf (buf, sizeof (buf), "%d (exited)", WEXITSTATUS (status));

  return buf;
}

/* Initialize the list of LWPs.  Note that this module, contrary to
   what GDB's generic threads layer does for its thread list,
   re-initializes the LWP lists whenever we mourn or detach (which
   doesn't involve mourning) the inferior.  */

static void
init_lwp_list (void)
{
  struct lwp_info *lp, *lpnext;

  for (lp = lwp_list; lp; lp = lpnext)
    {
      lpnext = lp->next;
      xfree (lp);
    }

  lwp_list = NULL;
  num_lwps = 0;
  threaded = 0;
}

/* Add the LWP specified by PID to the list.  If this causes the
   number of LWPs to become larger than one, go into "threaded" mode.
   Return a pointer to the structure describing the new LWP.  */

static struct lwp_info *
add_lwp (ptid_t ptid)
{
  struct lwp_info *lp;

  gdb_assert (is_lwp (ptid));

  lp = (struct lwp_info *) xmalloc (sizeof (struct lwp_info));

  memset (lp, 0, sizeof (struct lwp_info));

  lp->waitstatus.kind = TARGET_WAITKIND_IGNORE;

  lp->ptid = ptid;

  lp->next = lwp_list;
  lwp_list = lp;
  if (++num_lwps > 1)
    threaded = 1;

  return lp;
}

/* Remove the LWP specified by PID from the list.  */

static void
delete_lwp (ptid_t ptid)
{
  struct lwp_info *lp, *lpprev;

  lpprev = NULL;

  for (lp = lwp_list; lp; lpprev = lp, lp = lp->next)
    if (ptid_equal (lp->ptid, ptid))
      break;

  if (!lp)
    return;

  /* We don't go back to "non-threaded" mode if the number of threads
     becomes less than two.  */
  num_lwps--;

  if (lpprev)
    lpprev->next = lp->next;
  else
    lwp_list = lp->next;

  xfree (lp);
}

/* Return a pointer to the structure describing the LWP corresponding
   to PID.  If no corresponding LWP could be found, return NULL.  */

static struct lwp_info *
find_lwp_pid (ptid_t ptid)
{
  struct lwp_info *lp;
  int lwp;

  if (is_lwp (ptid))
    lwp = GET_LWP (ptid);
  else
    lwp = GET_PID (ptid);

  for (lp = lwp_list; lp; lp = lp->next)
    if (lwp == GET_LWP (lp->ptid))
      return lp;

  return NULL;
}

/* Call CALLBACK with its second argument set to DATA for every LWP in
   the list.  If CALLBACK returns 1 for a particular LWP, return a
   pointer to the structure describing that LWP immediately.
   Otherwise return NULL.  */

struct lwp_info *
iterate_over_lwps (int (*callback) (struct lwp_info *, void *), void *data)
{
  struct lwp_info *lp, *lpnext;

  for (lp = lwp_list; lp; lp = lpnext)
    {
      lpnext = lp->next;
      if ((*callback) (lp, data))
	return lp;
    }

  return NULL;
}

/* Attach to the LWP specified by PID.  If VERBOSE is non-zero, print
   a message telling the user that a new LWP has been added to the
   process.  */

void
lin_lwp_attach_lwp (ptid_t ptid, int verbose)
{
  struct lwp_info *lp, *found_lp;

  gdb_assert (is_lwp (ptid));

  /* Make sure SIGCHLD is blocked.  We don't want SIGCHLD events
     to interrupt either the ptrace() or waitpid() calls below.  */
  if (!sigismember (&blocked_mask, SIGCHLD))
    {
      sigaddset (&blocked_mask, SIGCHLD);
      sigprocmask (SIG_BLOCK, &blocked_mask, NULL);
    }

  if (verbose)
    printf_filtered ("[New %s]\n", target_pid_to_str (ptid));

  found_lp = lp = find_lwp_pid (ptid);
  if (lp == NULL)
    lp = add_lwp (ptid);

  /* We assume that we're already attached to any LWP that has an id
     equal to the overall process id, and to any LWP that is already
     in our list of LWPs.  If we're not seeing exit events from threads
     and we've had PID wraparound since we last tried to stop all threads,
     this assumption might be wrong; fortunately, this is very unlikely
     to happen.  */
  if (GET_LWP (ptid) != GET_PID (ptid) && found_lp == NULL)
    {
      pid_t pid;
      int status;

      if (ptrace (PTRACE_ATTACH, GET_LWP (ptid), 0, 0) < 0)
	error ("Can't attach %s: %s", target_pid_to_str (ptid),
	       safe_strerror (errno));

      if (debug_linux_nat)
	fprintf_unfiltered (gdb_stdlog,
			    "LLAL: PTRACE_ATTACH %s, 0, 0 (OK)\n",
			    target_pid_to_str (ptid));

      pid = waitpid (GET_LWP (ptid), &status, 0);
      if (pid == -1 && errno == ECHILD)
	{
	  /* Try again with __WCLONE to check cloned processes.  */
	  pid = waitpid (GET_LWP (ptid), &status, __WCLONE);
	  lp->cloned = 1;
	}

      gdb_assert (pid == GET_LWP (ptid)
		  && WIFSTOPPED (status) && WSTOPSIG (status));

      child_post_attach (pid);

      lp->stopped = 1;

      if (debug_linux_nat)
	{
	  fprintf_unfiltered (gdb_stdlog,
			      "LLAL: waitpid %s received %s\n",
			      target_pid_to_str (ptid),
			      status_to_str (status));
	}
    }
  else
    {
      /* We assume that the LWP representing the original process is
         already stopped.  Mark it as stopped in the data structure
         that the linux ptrace layer uses to keep track of threads.
         Note that this won't have already been done since the main
         thread will have, we assume, been stopped by an attach from a
         different layer.  */
      lp->stopped = 1;
    }
}

static void
linux_nat_attach (char *args, int from_tty)
{
  struct lwp_info *lp;
  pid_t pid;
  int status;

  /* FIXME: We should probably accept a list of process id's, and
     attach all of them.  */
  deprecated_child_ops.to_attach (args, from_tty);

  /* Add the initial process as the first LWP to the list.  */
  lp = add_lwp (BUILD_LWP (GET_PID (inferior_ptid), GET_PID (inferior_ptid)));

  /* Make sure the initial process is stopped.  The user-level threads
     layer might want to poke around in the inferior, and that won't
     work if things haven't stabilized yet.  */
  pid = waitpid (GET_PID (inferior_ptid), &status, 0);
  if (pid == -1 && errno == ECHILD)
    {
      warning ("%s is a cloned process", target_pid_to_str (inferior_ptid));

      /* Try again with __WCLONE to check cloned processes.  */
      pid = waitpid (GET_PID (inferior_ptid), &status, __WCLONE);
      lp->cloned = 1;
    }

  gdb_assert (pid == GET_PID (inferior_ptid)
	      && WIFSTOPPED (status) && WSTOPSIG (status) == SIGSTOP);

  lp->stopped = 1;

  /* Fake the SIGSTOP that core GDB expects.  */
  lp->status = W_STOPCODE (SIGSTOP);
  lp->resumed = 1;
  if (debug_linux_nat)
    {
      fprintf_unfiltered (gdb_stdlog,
			  "LLA: waitpid %ld, faking SIGSTOP\n", (long) pid);
    }
}

static int
detach_callback (struct lwp_info *lp, void *data)
{
  gdb_assert (lp->status == 0 || WIFSTOPPED (lp->status));

  if (debug_linux_nat && lp->status)
    fprintf_unfiltered (gdb_stdlog, "DC:  Pending %s for %s on detach.\n",
			strsignal (WSTOPSIG (lp->status)),
			target_pid_to_str (lp->ptid));

  while (lp->signalled && lp->stopped)
    {
      errno = 0;
      if (ptrace (PTRACE_CONT, GET_LWP (lp->ptid), 0,
		  WSTOPSIG (lp->status)) < 0)
	error ("Can't continue %s: %s", target_pid_to_str (lp->ptid),
	       safe_strerror (errno));

      if (debug_linux_nat)
	fprintf_unfiltered (gdb_stdlog,
			    "DC:  PTRACE_CONTINUE (%s, 0, %s) (OK)\n",
			    target_pid_to_str (lp->ptid),
			    status_to_str (lp->status));

      lp->stopped = 0;
      lp->signalled = 0;
      lp->status = 0;
      /* FIXME drow/2003-08-26: There was a call to stop_wait_callback
	 here.  But since lp->signalled was cleared above,
	 stop_wait_callback didn't do anything; the process was left
	 running.  Shouldn't we be waiting for it to stop?
	 I've removed the call, since stop_wait_callback now does do
	 something when called with lp->signalled == 0.  */

      gdb_assert (lp->status == 0 || WIFSTOPPED (lp->status));
    }

  /* We don't actually detach from the LWP that has an id equal to the
     overall process id just yet.  */
  if (GET_LWP (lp->ptid) != GET_PID (lp->ptid))
    {
      errno = 0;
      if (ptrace (PTRACE_DETACH, GET_LWP (lp->ptid), 0,
		  WSTOPSIG (lp->status)) < 0)
	error ("Can't detach %s: %s", target_pid_to_str (lp->ptid),
	       safe_strerror (errno));

      if (debug_linux_nat)
	fprintf_unfiltered (gdb_stdlog,
			    "PTRACE_DETACH (%s, %s, 0) (OK)\n",
			    target_pid_to_str (lp->ptid),
			    strsignal (WSTOPSIG (lp->status)));

      delete_lwp (lp->ptid);
    }

  return 0;
}

static void
linux_nat_detach (char *args, int from_tty)
{
  iterate_over_lwps (detach_callback, NULL);

  /* Only the initial process should be left right now.  */
  gdb_assert (num_lwps == 1);

  trap_ptid = null_ptid;

  /* Destroy LWP info; it's no longer valid.  */
  init_lwp_list ();

  /* Restore the original signal mask.  */
  sigprocmask (SIG_SETMASK, &normal_mask, NULL);
  sigemptyset (&blocked_mask);

  inferior_ptid = pid_to_ptid (GET_PID (inferior_ptid));
  deprecated_child_ops.to_detach (args, from_tty);
}

/* Resume LP.  */

static int
resume_callback (struct lwp_info *lp, void *data)
{
  if (lp->stopped && lp->status == 0)
    {
      struct thread_info *tp;

      child_resume (pid_to_ptid (GET_LWP (lp->ptid)), 0, TARGET_SIGNAL_0);
      if (debug_linux_nat)
	fprintf_unfiltered (gdb_stdlog,
			    "RC:  PTRACE_CONT %s, 0, 0 (resume sibling)\n",
			    target_pid_to_str (lp->ptid));
      lp->stopped = 0;
      lp->step = 0;
    }

  return 0;
}

static int
resume_clear_callback (struct lwp_info *lp, void *data)
{
  lp->resumed = 0;
  return 0;
}

static int
resume_set_callback (struct lwp_info *lp, void *data)
{
  lp->resumed = 1;
  return 0;
}

static void
linux_nat_resume (ptid_t ptid, int step, enum target_signal signo)
{
  struct lwp_info *lp;
  int resume_all;

  /* A specific PTID means `step only this process id'.  */
  resume_all = (PIDGET (ptid) == -1);

  if (resume_all)
    iterate_over_lwps (resume_set_callback, NULL);
  else
    iterate_over_lwps (resume_clear_callback, NULL);

  /* If PID is -1, it's the current inferior that should be
     handled specially.  */
  if (PIDGET (ptid) == -1)
    ptid = inferior_ptid;

  lp = find_lwp_pid (ptid);
  if (lp)
    {
      ptid = pid_to_ptid (GET_LWP (lp->ptid));

      /* Remember if we're stepping.  */
      lp->step = step;

      /* Mark this LWP as resumed.  */
      lp->resumed = 1;

      /* If we have a pending wait status for this thread, there is no
         point in resuming the process.  */
      if (lp->status)
	{
	  /* FIXME: What should we do if we are supposed to continue
	     this thread with a signal?  */
	  gdb_assert (signo == TARGET_SIGNAL_0);
	  return;
	}

      /* Mark LWP as not stopped to prevent it from being continued by
         resume_callback.  */
      lp->stopped = 0;
    }

  if (resume_all)
    iterate_over_lwps (resume_callback, NULL);

  child_resume (ptid, step, signo);
  if (debug_linux_nat)
    fprintf_unfiltered (gdb_stdlog,
			"LLR: %s %s, %s (resume event thread)\n",
			step ? "PTRACE_SINGLESTEP" : "PTRACE_CONT",
			target_pid_to_str (ptid),
			signo ? strsignal (signo) : "0");
}

/* Issue kill to specified lwp.  */

static int tkill_failed;

static int
kill_lwp (int lwpid, int signo)
{
  errno = 0;

/* Use tkill, if possible, in case we are using nptl threads.  If tkill
   fails, then we are not using nptl threads and we should be using kill.  */

#ifdef HAVE_TKILL_SYSCALL
  if (!tkill_failed)
    {
      int ret = syscall (__NR_tkill, lwpid, signo);
      if (errno != ENOSYS)
	return ret;
      errno = 0;
      tkill_failed = 1;
    }
#endif

  return kill (lwpid, signo);
}

/* Handle a GNU/Linux extended wait response.  Most of the work we
   just pass off to linux_handle_extended_wait, but if it reports a
   clone event we need to add the new LWP to our list (and not report
   the trap to higher layers).  This function returns non-zero if
   the event should be ignored and we should wait again.  */

static int
linux_nat_handle_extended (struct lwp_info *lp, int status)
{
  linux_handle_extended_wait (GET_LWP (lp->ptid), status,
			      &lp->waitstatus);

  /* TARGET_WAITKIND_SPURIOUS is used to indicate clone events.  */
  if (lp->waitstatus.kind == TARGET_WAITKIND_SPURIOUS)
    {
      struct lwp_info *new_lp;
      new_lp = add_lwp (BUILD_LWP (lp->waitstatus.value.related_pid,
				   GET_PID (inferior_ptid)));
      new_lp->cloned = 1;
      new_lp->stopped = 1;

      lp->waitstatus.kind = TARGET_WAITKIND_IGNORE;

      if (debug_linux_nat)
	fprintf_unfiltered (gdb_stdlog,
			    "LLHE: Got clone event from LWP %ld, resuming\n",
			    GET_LWP (lp->ptid));
      ptrace (PTRACE_CONT, GET_LWP (lp->ptid), 0, 0);

      return 1;
    }

  return 0;
}

/* Wait for LP to stop.  Returns the wait status, or 0 if the LWP has
   exited.  */

static int
wait_lwp (struct lwp_info *lp)
{
  pid_t pid;
  int status;
  int thread_dead = 0;

  gdb_assert (!lp->stopped);
  gdb_assert (lp->status == 0);

  pid = waitpid (GET_LWP (lp->ptid), &status, 0);
  if (pid == -1 && errno == ECHILD)
    {
      pid = waitpid (GET_LWP (lp->ptid), &status, __WCLONE);
      if (pid == -1 && errno == ECHILD)
	{
	  /* The thread has previously exited.  We need to delete it
	     now because, for some vendor 2.4 kernels with NPTL
	     support backported, there won't be an exit event unless
	     it is the main thread.  2.6 kernels will report an exit
	     event for each thread that exits, as expected.  */
	  thread_dead = 1;
	  if (debug_linux_nat)
	    fprintf_unfiltered (gdb_stdlog, "WL: %s vanished.\n",
				target_pid_to_str (lp->ptid));
	}
    }

  if (!thread_dead)
    {
      gdb_assert (pid == GET_LWP (lp->ptid));

      if (debug_linux_nat)
	{
	  fprintf_unfiltered (gdb_stdlog,
			      "WL: waitpid %s received %s\n",
			      target_pid_to_str (lp->ptid),
			      status_to_str (status));
	}
    }

  /* Check if the thread has exited.  */
  if (WIFEXITED (status) || WIFSIGNALED (status))
    {
      thread_dead = 1;
      if (debug_linux_nat)
	fprintf_unfiltered (gdb_stdlog, "WL: %s exited.\n",
			    target_pid_to_str (lp->ptid));
    }

  if (thread_dead)
    {
      if (in_thread_list (lp->ptid))
	{
	  /* Core GDB cannot deal with us deleting the current thread.  */
	  if (!ptid_equal (lp->ptid, inferior_ptid))
	    delete_thread (lp->ptid);
	  printf_unfiltered ("[%s exited]\n",
			     target_pid_to_str (lp->ptid));
	}

      delete_lwp (lp->ptid);
      return 0;
    }

  gdb_assert (WIFSTOPPED (status));

  /* Handle GNU/Linux's extended waitstatus for trace events.  */
  if (WIFSTOPPED (status) && WSTOPSIG (status) == SIGTRAP && status >> 16 != 0)
    {
      if (debug_linux_nat)
	fprintf_unfiltered (gdb_stdlog,
			    "WL: Handling extended status 0x%06x\n",
			    status);
      if (linux_nat_handle_extended (lp, status))
	return wait_lwp (lp);
    }

  return status;
}

/* Send a SIGSTOP to LP.  */

static int
stop_callback (struct lwp_info *lp, void *data)
{
  if (!lp->stopped && !lp->signalled)
    {
      int ret;

      if (debug_linux_nat)
	{
	  fprintf_unfiltered (gdb_stdlog,
			      "SC:  kill %s **<SIGSTOP>**\n",
			      target_pid_to_str (lp->ptid));
	}
      errno = 0;
      ret = kill_lwp (GET_LWP (lp->ptid), SIGSTOP);
      if (debug_linux_nat)
	{
	  fprintf_unfiltered (gdb_stdlog,
			      "SC:  lwp kill %d %s\n",
			      ret,
			      errno ? safe_strerror (errno) : "ERRNO-OK");
	}

      lp->signalled = 1;
      gdb_assert (lp->status == 0);
    }

  return 0;
}

/* Wait until LP is stopped.  If DATA is non-null it is interpreted as
   a pointer to a set of signals to be flushed immediately.  */

static int
stop_wait_callback (struct lwp_info *lp, void *data)
{
  sigset_t *flush_mask = data;

  if (!lp->stopped)
    {
      int status;

      status = wait_lwp (lp);
      if (status == 0)
	return 0;

      /* Ignore any signals in FLUSH_MASK.  */
      if (flush_mask && sigismember (flush_mask, WSTOPSIG (status)))
	{
	  if (!lp->signalled)
	    {
	      lp->stopped = 1;
	      return 0;
	    }

	  errno = 0;
	  ptrace (PTRACE_CONT, GET_LWP (lp->ptid), 0, 0);
	  if (debug_linux_nat)
	    fprintf_unfiltered (gdb_stdlog,
				"PTRACE_CONT %s, 0, 0 (%s)\n",
				target_pid_to_str (lp->ptid),
				errno ? safe_strerror (errno) : "OK");

	  return stop_wait_callback (lp, flush_mask);
	}

      if (WSTOPSIG (status) != SIGSTOP)
	{
	  if (WSTOPSIG (status) == SIGTRAP)
	    {
	      /* If a LWP other than the LWP that we're reporting an
	         event for has hit a GDB breakpoint (as opposed to
	         some random trap signal), then just arrange for it to
	         hit it again later.  We don't keep the SIGTRAP status
	         and don't forward the SIGTRAP signal to the LWP.  We
	         will handle the current event, eventually we will
	         resume all LWPs, and this one will get its breakpoint
	         trap again.

	         If we do not do this, then we run the risk that the
	         user will delete or disable the breakpoint, but the
	         thread will have already tripped on it.  */

	      /* Now resume this LWP and get the SIGSTOP event. */
	      errno = 0;
	      ptrace (PTRACE_CONT, GET_LWP (lp->ptid), 0, 0);
	      if (debug_linux_nat)
		{
		  fprintf_unfiltered (gdb_stdlog,
				      "PTRACE_CONT %s, 0, 0 (%s)\n",
				      target_pid_to_str (lp->ptid),
				      errno ? safe_strerror (errno) : "OK");

		  fprintf_unfiltered (gdb_stdlog,
				      "SWC: Candidate SIGTRAP event in %s\n",
				      target_pid_to_str (lp->ptid));
		}
	      /* Hold the SIGTRAP for handling by linux_nat_wait. */
	      stop_wait_callback (lp, data);
	      /* If there's another event, throw it back into the queue. */
	      if (lp->status)
		{
		  if (debug_linux_nat)
		    {
		      fprintf_unfiltered (gdb_stdlog,
					  "SWC: kill %s, %s\n",
					  target_pid_to_str (lp->ptid),
					  status_to_str ((int) status));
		    }
		  kill_lwp (GET_LWP (lp->ptid), WSTOPSIG (lp->status));
		}
	      /* Save the sigtrap event. */
	      lp->status = status;
	      return 0;
	    }
	  else
	    {
	      /* The thread was stopped with a signal other than
	         SIGSTOP, and didn't accidentally trip a breakpoint. */

	      if (debug_linux_nat)
		{
		  fprintf_unfiltered (gdb_stdlog,
				      "SWC: Pending event %s in %s\n",
				      status_to_str ((int) status),
				      target_pid_to_str (lp->ptid));
		}
	      /* Now resume this LWP and get the SIGSTOP event. */
	      errno = 0;
	      ptrace (PTRACE_CONT, GET_LWP (lp->ptid), 0, 0);
	      if (debug_linux_nat)
		fprintf_unfiltered (gdb_stdlog,
				    "SWC: PTRACE_CONT %s, 0, 0 (%s)\n",
				    target_pid_to_str (lp->ptid),
				    errno ? safe_strerror (errno) : "OK");

	      /* Hold this event/waitstatus while we check to see if
	         there are any more (we still want to get that SIGSTOP). */
	      stop_wait_callback (lp, data);
	      /* If the lp->status field is still empty, use it to hold
	         this event.  If not, then this event must be returned
	         to the event queue of the LWP.  */
	      if (lp->status == 0)
		lp->status = status;
	      else
		{
		  if (debug_linux_nat)
		    {
		      fprintf_unfiltered (gdb_stdlog,
					  "SWC: kill %s, %s\n",
					  target_pid_to_str (lp->ptid),
					  status_to_str ((int) status));
		    }
		  kill_lwp (GET_LWP (lp->ptid), WSTOPSIG (status));
		}
	      return 0;
	    }
	}
      else
	{
	  /* We caught the SIGSTOP that we intended to catch, so
	     there's no SIGSTOP pending.  */
	  lp->stopped = 1;
	  lp->signalled = 0;
	}
    }

  return 0;
}

/* Check whether PID has any pending signals in FLUSH_MASK.  If so set
   the appropriate bits in PENDING, and return 1 - otherwise return 0.  */

static int
linux_nat_has_pending (int pid, sigset_t *pending, sigset_t *flush_mask)
{
  sigset_t blocked, ignored;
  int i;

  linux_proc_pending_signals (pid, pending, &blocked, &ignored);

  if (!flush_mask)
    return 0;

  for (i = 1; i < NSIG; i++)
    if (sigismember (pending, i))
      if (!sigismember (flush_mask, i)
	  || sigismember (&blocked, i)
	  || sigismember (&ignored, i))
	sigdelset (pending, i);

  if (sigisemptyset (pending))
    return 0;

  return 1;
}

/* DATA is interpreted as a mask of signals to flush.  If LP has
   signals pending, and they are all in the flush mask, then arrange
   to flush them.  LP should be stopped, as should all other threads
   it might share a signal queue with.  */

static int
flush_callback (struct lwp_info *lp, void *data)
{
  sigset_t *flush_mask = data;
  sigset_t pending, intersection, blocked, ignored;
  int pid, status;

  /* Normally, when an LWP exits, it is removed from the LWP list.  The
     last LWP isn't removed till later, however.  So if there is only
     one LWP on the list, make sure it's alive.  */
  if (lwp_list == lp && lp->next == NULL)
    if (!linux_nat_thread_alive (lp->ptid))
      return 0;

  /* Just because the LWP is stopped doesn't mean that new signals
     can't arrive from outside, so this function must be careful of
     race conditions.  However, because all threads are stopped, we
     can assume that the pending mask will not shrink unless we resume
     the LWP, and that it will then get another signal.  We can't
     control which one, however.  */

  if (lp->status)
    {
      if (debug_linux_nat)
	printf_unfiltered ("FC: LP has pending status %06x\n", lp->status);
      if (WIFSTOPPED (lp->status) && sigismember (flush_mask, WSTOPSIG (lp->status)))
	lp->status = 0;
    }

  while (linux_nat_has_pending (GET_LWP (lp->ptid), &pending, flush_mask))
    {
      int ret;
      
      errno = 0;
      ret = ptrace (PTRACE_CONT, GET_LWP (lp->ptid), 0, 0);
      if (debug_linux_nat)
	fprintf_unfiltered (gdb_stderr,
			    "FC: Sent PTRACE_CONT, ret %d %d\n", ret, errno);

      lp->stopped = 0;
      stop_wait_callback (lp, flush_mask);
      if (debug_linux_nat)
	fprintf_unfiltered (gdb_stderr,
			    "FC: Wait finished; saved status is %d\n",
			    lp->status);
    }

  return 0;
}

/* Return non-zero if LP has a wait status pending.  */

static int
status_callback (struct lwp_info *lp, void *data)
{
  /* Only report a pending wait status if we pretend that this has
     indeed been resumed.  */
  return (lp->status != 0 && lp->resumed);
}

/* Return non-zero if LP isn't stopped.  */

static int
running_callback (struct lwp_info *lp, void *data)
{
  return (lp->stopped == 0 || (lp->status != 0 && lp->resumed));
}

/* Count the LWP's that have had events.  */

static int
count_events_callback (struct lwp_info *lp, void *data)
{
  int *count = data;

  gdb_assert (count != NULL);

  /* Count only LWPs that have a SIGTRAP event pending.  */
  if (lp->status != 0
      && WIFSTOPPED (lp->status) && WSTOPSIG (lp->status) == SIGTRAP)
    (*count)++;

  return 0;
}

/* Select the LWP (if any) that is currently being single-stepped.  */

static int
select_singlestep_lwp_callback (struct lwp_info *lp, void *data)
{
  if (lp->step && lp->status != 0)
    return 1;
  else
    return 0;
}

/* Select the Nth LWP that has had a SIGTRAP event.  */

static int
select_event_lwp_callback (struct lwp_info *lp, void *data)
{
  int *selector = data;

  gdb_assert (selector != NULL);

  /* Select only LWPs that have a SIGTRAP event pending. */
  if (lp->status != 0
      && WIFSTOPPED (lp->status) && WSTOPSIG (lp->status) == SIGTRAP)
    if ((*selector)-- == 0)
      return 1;

  return 0;
}

static int
cancel_breakpoints_callback (struct lwp_info *lp, void *data)
{
  struct lwp_info *event_lp = data;

  /* Leave the LWP that has been elected to receive a SIGTRAP alone.  */
  if (lp == event_lp)
    return 0;

  /* If a LWP other than the LWP that we're reporting an event for has
     hit a GDB breakpoint (as opposed to some random trap signal),
     then just arrange for it to hit it again later.  We don't keep
     the SIGTRAP status and don't forward the SIGTRAP signal to the
     LWP.  We will handle the current event, eventually we will resume
     all LWPs, and this one will get its breakpoint trap again.

     If we do not do this, then we run the risk that the user will
     delete or disable the breakpoint, but the LWP will have already
     tripped on it.  */

  if (lp->status != 0
      && WIFSTOPPED (lp->status) && WSTOPSIG (lp->status) == SIGTRAP
      && breakpoint_inserted_here_p (read_pc_pid (lp->ptid) -
				     DECR_PC_AFTER_BREAK))
    {
      if (debug_linux_nat)
	fprintf_unfiltered (gdb_stdlog,
			    "CBC: Push back breakpoint for %s\n",
			    target_pid_to_str (lp->ptid));

      /* Back up the PC if necessary.  */
      if (DECR_PC_AFTER_BREAK)
	write_pc_pid (read_pc_pid (lp->ptid) - DECR_PC_AFTER_BREAK, lp->ptid);

      /* Throw away the SIGTRAP.  */
      lp->status = 0;
    }

  return 0;
}

/* Select one LWP out of those that have events pending.  */

static void
select_event_lwp (struct lwp_info **orig_lp, int *status)
{
  int num_events = 0;
  int random_selector;
  struct lwp_info *event_lp;

  /* Record the wait status for the origional LWP.  */
  (*orig_lp)->status = *status;

  /* Give preference to any LWP that is being single-stepped.  */
  event_lp = iterate_over_lwps (select_singlestep_lwp_callback, NULL);
  if (event_lp != NULL)
    {
      if (debug_linux_nat)
	fprintf_unfiltered (gdb_stdlog,
			    "SEL: Select single-step %s\n",
			    target_pid_to_str (event_lp->ptid));
    }
  else
    {
      /* No single-stepping LWP.  Select one at random, out of those
         which have had SIGTRAP events.  */

      /* First see how many SIGTRAP events we have.  */
      iterate_over_lwps (count_events_callback, &num_events);

      /* Now randomly pick a LWP out of those that have had a SIGTRAP.  */
      random_selector = (int)
	((num_events * (double) rand ()) / (RAND_MAX + 1.0));

      if (debug_linux_nat && num_events > 1)
	fprintf_unfiltered (gdb_stdlog,
			    "SEL: Found %d SIGTRAP events, selecting #%d\n",
			    num_events, random_selector);

      event_lp = iterate_over_lwps (select_event_lwp_callback,
				    &random_selector);
    }

  if (event_lp != NULL)
    {
      /* Switch the event LWP.  */
      *orig_lp = event_lp;
      *status = event_lp->status;
    }

  /* Flush the wait status for the event LWP.  */
  (*orig_lp)->status = 0;
}

/* Return non-zero if LP has been resumed.  */

static int
resumed_callback (struct lwp_info *lp, void *data)
{
  return lp->resumed;
}

#ifdef CHILD_WAIT

/* We need to override child_wait to support attaching to cloned
   processes, since a normal wait (as done by the default version)
   ignores those processes.  */

/* Wait for child PTID to do something.  Return id of the child,
   minus_one_ptid in case of error; store status into *OURSTATUS.  */

ptid_t
child_wait (ptid_t ptid, struct target_waitstatus *ourstatus)
{
  int save_errno;
  int status;
  pid_t pid;

  ourstatus->kind = TARGET_WAITKIND_IGNORE;

  do
    {
      set_sigint_trap ();	/* Causes SIGINT to be passed on to the
				   attached process.  */
      set_sigio_trap ();

      pid = waitpid (GET_PID (ptid), &status, 0);
      if (pid == -1 && errno == ECHILD)
	/* Try again with __WCLONE to check cloned processes.  */
	pid = waitpid (GET_PID (ptid), &status, __WCLONE);

      if (debug_linux_nat)
	{
	  fprintf_unfiltered (gdb_stdlog,
			      "CW:  waitpid %ld received %s\n",
			      (long) pid, status_to_str (status));
	}

      save_errno = errno;

      /* Make sure we don't report an event for the exit of the
         original program, if we've detached from it.  */
      if (pid != -1 && !WIFSTOPPED (status) && pid != GET_PID (inferior_ptid))
	{
	  pid = -1;
	  save_errno = EINTR;
	}

      /* Check for stop events reported by a process we didn't already
	 know about - in this case, anything other than inferior_ptid.

	 If we're expecting to receive stopped processes after fork,
	 vfork, and clone events, then we'll just add the new one to
	 our list and go back to waiting for the event to be reported
	 - the stopped process might be returned from waitpid before
	 or after the event is.  If we want to handle debugging of
	 CLONE_PTRACE processes we need to do more here, i.e. switch
	 to multi-threaded mode.  */
      if (pid != -1 && WIFSTOPPED (status) && WSTOPSIG (status) == SIGSTOP
	  && pid != GET_PID (inferior_ptid))
	{
	  linux_record_stopped_pid (pid);
	  pid = -1;
	  save_errno = EINTR;
	}

      /* Handle GNU/Linux's extended waitstatus for trace events.  */
      if (pid != -1 && WIFSTOPPED (status) && WSTOPSIG (status) == SIGTRAP
	  && status >> 16 != 0)
	{
	  linux_handle_extended_wait (pid, status, ourstatus);

	  /* If we see a clone event, detach the child, and don't
	     report the event.  It would be nice to offer some way to
	     switch into a non-thread-db based threaded mode at this
	     point.  */
	  if (ourstatus->kind == TARGET_WAITKIND_SPURIOUS)
	    {
	      ptrace (PTRACE_DETACH, ourstatus->value.related_pid, 0, 0);
	      ourstatus->kind = TARGET_WAITKIND_IGNORE;
	      ptrace (PTRACE_CONT, pid, 0, 0);
	      pid = -1;
	      save_errno = EINTR;
	    }
	}

      clear_sigio_trap ();
      clear_sigint_trap ();
    }
  while (pid == -1 && save_errno == EINTR);

  if (pid == -1)
    {
      warning ("Child process unexpectedly missing: %s",
	       safe_strerror (errno));

      /* Claim it exited with unknown signal.  */
      ourstatus->kind = TARGET_WAITKIND_SIGNALLED;
      ourstatus->value.sig = TARGET_SIGNAL_UNKNOWN;
      return minus_one_ptid;
    }

  if (ourstatus->kind == TARGET_WAITKIND_IGNORE)
    store_waitstatus (ourstatus, status);

  return pid_to_ptid (pid);
}

#endif

/* Stop an active thread, verify it still exists, then resume it.  */

static int
stop_and_resume_callback (struct lwp_info *lp, void *data)
{
  struct lwp_info *ptr;

  if (!lp->stopped && !lp->signalled)
    {
      stop_callback (lp, NULL);
      stop_wait_callback (lp, NULL);
      /* Resume if the lwp still exists.  */
      for (ptr = lwp_list; ptr; ptr = ptr->next)
	if (lp == ptr)
	  {
	    resume_callback (lp, NULL);
	    resume_set_callback (lp, NULL);
	  }
    }
  return 0;
}

static ptid_t
linux_nat_wait (ptid_t ptid, struct target_waitstatus *ourstatus)
{
  struct lwp_info *lp = NULL;
  int options = 0;
  int status = 0;
  pid_t pid = PIDGET (ptid);
  sigset_t flush_mask;

  sigemptyset (&flush_mask);

  /* Make sure SIGCHLD is blocked.  */
  if (!sigismember (&blocked_mask, SIGCHLD))
    {
      sigaddset (&blocked_mask, SIGCHLD);
      sigprocmask (SIG_BLOCK, &blocked_mask, NULL);
    }

retry:

  /* Make sure there is at least one LWP that has been resumed, at
     least if there are any LWPs at all.  */
  gdb_assert (num_lwps == 0 || iterate_over_lwps (resumed_callback, NULL));

  /* First check if there is a LWP with a wait status pending.  */
  if (pid == -1)
    {
      /* Any LWP that's been resumed will do.  */
      lp = iterate_over_lwps (status_callback, NULL);
      if (lp)
	{
	  status = lp->status;
	  lp->status = 0;

	  if (debug_linux_nat && status)
	    fprintf_unfiltered (gdb_stdlog,
				"LLW: Using pending wait status %s for %s.\n",
				status_to_str (status),
				target_pid_to_str (lp->ptid));
	}

      /* But if we don't fine one, we'll have to wait, and check both
         cloned and uncloned processes.  We start with the cloned
         processes.  */
      options = __WCLONE | WNOHANG;
    }
  else if (is_lwp (ptid))
    {
      if (debug_linux_nat)
	fprintf_unfiltered (gdb_stdlog,
			    "LLW: Waiting for specific LWP %s.\n",
			    target_pid_to_str (ptid));

      /* We have a specific LWP to check.  */
      lp = find_lwp_pid (ptid);
      gdb_assert (lp);
      status = lp->status;
      lp->status = 0;

      if (debug_linux_nat && status)
	fprintf_unfiltered (gdb_stdlog,
			    "LLW: Using pending wait status %s for %s.\n",
			    status_to_str (status),
			    target_pid_to_str (lp->ptid));

      /* If we have to wait, take into account whether PID is a cloned
         process or not.  And we have to convert it to something that
         the layer beneath us can understand.  */
      options = lp->cloned ? __WCLONE : 0;
      pid = GET_LWP (ptid);
    }

  if (status && lp->signalled)
    {
      /* A pending SIGSTOP may interfere with the normal stream of
         events.  In a typical case where interference is a problem,
         we have a SIGSTOP signal pending for LWP A while
         single-stepping it, encounter an event in LWP B, and take the
         pending SIGSTOP while trying to stop LWP A.  After processing
         the event in LWP B, LWP A is continued, and we'll never see
         the SIGTRAP associated with the last time we were
         single-stepping LWP A.  */

      /* Resume the thread.  It should halt immediately returning the
         pending SIGSTOP.  */
      registers_changed ();
      child_resume (pid_to_ptid (GET_LWP (lp->ptid)), lp->step,
		    TARGET_SIGNAL_0);
      if (debug_linux_nat)
	fprintf_unfiltered (gdb_stdlog,
			    "LLW: %s %s, 0, 0 (expect SIGSTOP)\n",
			    lp->step ? "PTRACE_SINGLESTEP" : "PTRACE_CONT",
			    target_pid_to_str (lp->ptid));
      lp->stopped = 0;
      gdb_assert (lp->resumed);

      /* This should catch the pending SIGSTOP.  */
      stop_wait_callback (lp, NULL);
    }

  set_sigint_trap ();		/* Causes SIGINT to be passed on to the
				   attached process. */
  set_sigio_trap ();

  while (status == 0)
    {
      pid_t lwpid;

      lwpid = waitpid (pid, &status, options);
      if (lwpid > 0)
	{
	  gdb_assert (pid == -1 || lwpid == pid);

	  if (debug_linux_nat)
	    {
	      fprintf_unfiltered (gdb_stdlog,
				  "LLW: waitpid %ld received %s\n",
				  (long) lwpid, status_to_str (status));
	    }

	  lp = find_lwp_pid (pid_to_ptid (lwpid));

	  /* Check for stop events reported by a process we didn't
	     already know about - anything not already in our LWP
	     list.

	     If we're expecting to receive stopped processes after
	     fork, vfork, and clone events, then we'll just add the
	     new one to our list and go back to waiting for the event
	     to be reported - the stopped process might be returned
	     from waitpid before or after the event is.  */
	  if (WIFSTOPPED (status) && !lp)
	    {
	      linux_record_stopped_pid (lwpid);
	      status = 0;
	      continue;
	    }

	  /* Make sure we don't report an event for the exit of an LWP not in
	     our list, i.e.  not part of the current process.  This can happen
	     if we detach from a program we original forked and then it
	     exits.  */
	  if (!WIFSTOPPED (status) && !lp)
	    {
	      status = 0;
	      continue;
	    }

	  /* NOTE drow/2003-06-17: This code seems to be meant for debugging
	     CLONE_PTRACE processes which do not use the thread library -
	     otherwise we wouldn't find the new LWP this way.  That doesn't
	     currently work, and the following code is currently unreachable
	     due to the two blocks above.  If it's fixed some day, this code
	     should be broken out into a function so that we can also pick up
	     LWPs from the new interface.  */
	  if (!lp)
	    {
	      lp = add_lwp (BUILD_LWP (lwpid, GET_PID (inferior_ptid)));
	      if (options & __WCLONE)
		lp->cloned = 1;

	      if (threaded)
		{
		  gdb_assert (WIFSTOPPED (status)
			      && WSTOPSIG (status) == SIGSTOP);
		  lp->signalled = 1;

		  if (!in_thread_list (inferior_ptid))
		    {
		      inferior_ptid = BUILD_LWP (GET_PID (inferior_ptid),
						 GET_PID (inferior_ptid));
		      add_thread (inferior_ptid);
		    }

		  add_thread (lp->ptid);
		  printf_unfiltered ("[New %s]\n",
				     target_pid_to_str (lp->ptid));
		}
	    }

	  /* Handle GNU/Linux's extended waitstatus for trace events.  */
	  if (WIFSTOPPED (status) && WSTOPSIG (status) == SIGTRAP && status >> 16 != 0)
	    {
	      if (debug_linux_nat)
		fprintf_unfiltered (gdb_stdlog,
				    "LLW: Handling extended status 0x%06x\n",
				    status);
	      if (linux_nat_handle_extended (lp, status))
		{
		  status = 0;
		  continue;
		}
	    }

	  /* Check if the thread has exited.  */
	  if ((WIFEXITED (status) || WIFSIGNALED (status)) && num_lwps > 1)
	    {
	      if (in_thread_list (lp->ptid))
		{
		  /* Core GDB cannot deal with us deleting the current
		     thread.  */
		  if (!ptid_equal (lp->ptid, inferior_ptid))
		    delete_thread (lp->ptid);
		  printf_unfiltered ("[%s exited]\n",
				     target_pid_to_str (lp->ptid));
		}

	      /* If this is the main thread, we must stop all threads and
	         verify if they are still alive.  This is because in the nptl
	         thread model, there is no signal issued for exiting LWPs
	         other than the main thread.  We only get the main thread
	         exit signal once all child threads have already exited.
	         If we stop all the threads and use the stop_wait_callback
	         to check if they have exited we can determine whether this
	         signal should be ignored or whether it means the end of the
	         debugged application, regardless of which threading model
	         is being used.  */
	      if (GET_PID (lp->ptid) == GET_LWP (lp->ptid))
		{
		  lp->stopped = 1;
		  iterate_over_lwps (stop_and_resume_callback, NULL);
		}

	      if (debug_linux_nat)
		fprintf_unfiltered (gdb_stdlog,
				    "LLW: %s exited.\n",
				    target_pid_to_str (lp->ptid));

	      delete_lwp (lp->ptid);

	      /* If there is at least one more LWP, then the exit signal
	         was not the end of the debugged application and should be
	         ignored.  */
	      if (num_lwps > 0)
		{
		  /* Make sure there is at least one thread running.  */
		  gdb_assert (iterate_over_lwps (running_callback, NULL));

		  /* Discard the event.  */
		  status = 0;
		  continue;
		}
	    }

	  /* Check if the current LWP has previously exited.  In the nptl
	     thread model, LWPs other than the main thread do not issue
	     signals when they exit so we must check whenever the thread
	     has stopped.  A similar check is made in stop_wait_callback().  */
	  if (num_lwps > 1 && !linux_nat_thread_alive (lp->ptid))
	    {
	      if (in_thread_list (lp->ptid))
		{
		  /* Core GDB cannot deal with us deleting the current
		     thread.  */
		  if (!ptid_equal (lp->ptid, inferior_ptid))
		    delete_thread (lp->ptid);
		  printf_unfiltered ("[%s exited]\n",
				     target_pid_to_str (lp->ptid));
		}
	      if (debug_linux_nat)
		fprintf_unfiltered (gdb_stdlog,
				    "LLW: %s exited.\n",
				    target_pid_to_str (lp->ptid));

	      delete_lwp (lp->ptid);

	      /* Make sure there is at least one thread running.  */
	      gdb_assert (iterate_over_lwps (running_callback, NULL));

	      /* Discard the event.  */
	      status = 0;
	      continue;
	    }

	  /* Make sure we don't report a SIGSTOP that we sent
	     ourselves in an attempt to stop an LWP.  */
	  if (lp->signalled
	      && WIFSTOPPED (status) && WSTOPSIG (status) == SIGSTOP)
	    {
	      if (debug_linux_nat)
		fprintf_unfiltered (gdb_stdlog,
				    "LLW: Delayed SIGSTOP caught for %s.\n",
				    target_pid_to_str (lp->ptid));

	      /* This is a delayed SIGSTOP.  */
	      lp->signalled = 0;

	      registers_changed ();
	      child_resume (pid_to_ptid (GET_LWP (lp->ptid)), lp->step,
			    TARGET_SIGNAL_0);
	      if (debug_linux_nat)
		fprintf_unfiltered (gdb_stdlog,
				    "LLW: %s %s, 0, 0 (discard SIGSTOP)\n",
				    lp->step ?
				    "PTRACE_SINGLESTEP" : "PTRACE_CONT",
				    target_pid_to_str (lp->ptid));

	      lp->stopped = 0;
	      gdb_assert (lp->resumed);

	      /* Discard the event.  */
	      status = 0;
	      continue;
	    }

	  break;
	}

      if (pid == -1)
	{
	  /* Alternate between checking cloned and uncloned processes.  */
	  options ^= __WCLONE;

	  /* And suspend every time we have checked both.  */
	  if (options & __WCLONE)
	    sigsuspend (&suspend_mask);
	}

      /* We shouldn't end up here unless we want to try again.  */
      gdb_assert (status == 0);
    }

  clear_sigio_trap ();
  clear_sigint_trap ();

  gdb_assert (lp);

  /* Don't report signals that GDB isn't interested in, such as
     signals that are neither printed nor stopped upon.  Stopping all
     threads can be a bit time-consuming so if we want decent
     performance with heavily multi-threaded programs, especially when
     they're using a high frequency timer, we'd better avoid it if we
     can.  */

  if (WIFSTOPPED (status))
    {
      int signo = target_signal_from_host (WSTOPSIG (status));

      if (signal_stop_state (signo) == 0
	  && signal_print_state (signo) == 0
	  && signal_pass_state (signo) == 1)
	{
	  /* FIMXE: kettenis/2001-06-06: Should we resume all threads
	     here?  It is not clear we should.  GDB may not expect
	     other threads to run.  On the other hand, not resuming
	     newly attached threads may cause an unwanted delay in
	     getting them running.  */
	  registers_changed ();
	  child_resume (pid_to_ptid (GET_LWP (lp->ptid)), lp->step, signo);
	  if (debug_linux_nat)
	    fprintf_unfiltered (gdb_stdlog,
				"LLW: %s %s, %s (preempt 'handle')\n",
				lp->step ?
				"PTRACE_SINGLESTEP" : "PTRACE_CONT",
				target_pid_to_str (lp->ptid),
				signo ? strsignal (signo) : "0");
	  lp->stopped = 0;
	  status = 0;
	  goto retry;
	}

      if (signo == TARGET_SIGNAL_INT && signal_pass_state (signo) == 0)
	{
	  /* If ^C/BREAK is typed at the tty/console, SIGINT gets
	     forwarded to the entire process group, that is, all LWP's
	     will receive it.  Since we only want to report it once,
	     we try to flush it from all LWPs except this one.  */
	  sigaddset (&flush_mask, SIGINT);
	}
    }

  /* This LWP is stopped now.  */
  lp->stopped = 1;

  if (debug_linux_nat)
    fprintf_unfiltered (gdb_stdlog, "LLW: Candidate event %s in %s.\n",
			status_to_str (status), target_pid_to_str (lp->ptid));

  /* Now stop all other LWP's ...  */
  iterate_over_lwps (stop_callback, NULL);

  /* ... and wait until all of them have reported back that they're no
     longer running.  */
  iterate_over_lwps (stop_wait_callback, &flush_mask);
  iterate_over_lwps (flush_callback, &flush_mask);

  /* If we're not waiting for a specific LWP, choose an event LWP from
     among those that have had events.  Giving equal priority to all
     LWPs that have had events helps prevent starvation.  */
  if (pid == -1)
    select_event_lwp (&lp, &status);

  /* Now that we've selected our final event LWP, cancel any
     breakpoints in other LWPs that have hit a GDB breakpoint.  See
     the comment in cancel_breakpoints_callback to find out why.  */
  iterate_over_lwps (cancel_breakpoints_callback, lp);

  /* If we're not running in "threaded" mode, we'll report the bare
     process id.  */

  if (WIFSTOPPED (status) && WSTOPSIG (status) == SIGTRAP)
    {
      trap_ptid = (threaded ? lp->ptid : pid_to_ptid (GET_LWP (lp->ptid)));
      if (debug_linux_nat)
	fprintf_unfiltered (gdb_stdlog,
			    "LLW: trap_ptid is %s.\n",
			    target_pid_to_str (trap_ptid));
    }
  else
    trap_ptid = null_ptid;

  if (lp->waitstatus.kind != TARGET_WAITKIND_IGNORE)
    {
      *ourstatus = lp->waitstatus;
      lp->waitstatus.kind = TARGET_WAITKIND_IGNORE;
    }
  else
    store_waitstatus (ourstatus, status);

  return (threaded ? lp->ptid : pid_to_ptid (GET_LWP (lp->ptid)));
}

static int
kill_callback (struct lwp_info *lp, void *data)
{
  errno = 0;
  ptrace (PTRACE_KILL, GET_LWP (lp->ptid), 0, 0);
  if (debug_linux_nat)
    fprintf_unfiltered (gdb_stdlog,
			"KC:  PTRACE_KILL %s, 0, 0 (%s)\n",
			target_pid_to_str (lp->ptid),
			errno ? safe_strerror (errno) : "OK");

  return 0;
}

static int
kill_wait_callback (struct lwp_info *lp, void *data)
{
  pid_t pid;

  /* We must make sure that there are no pending events (delayed
     SIGSTOPs, pending SIGTRAPs, etc.) to make sure the current
     program doesn't interfere with any following debugging session.  */

  /* For cloned processes we must check both with __WCLONE and
     without, since the exit status of a cloned process isn't reported
     with __WCLONE.  */
  if (lp->cloned)
    {
      do
	{
	  pid = waitpid (GET_LWP (lp->ptid), NULL, __WCLONE);
	  if (pid != (pid_t) -1 && debug_linux_nat)
	    {
	      fprintf_unfiltered (gdb_stdlog,
				  "KWC: wait %s received unknown.\n",
				  target_pid_to_str (lp->ptid));
	    }
	}
      while (pid == GET_LWP (lp->ptid));

      gdb_assert (pid == -1 && errno == ECHILD);
    }

  do
    {
      pid = waitpid (GET_LWP (lp->ptid), NULL, 0);
      if (pid != (pid_t) -1 && debug_linux_nat)
	{
	  fprintf_unfiltered (gdb_stdlog,
			      "KWC: wait %s received unk.\n",
			      target_pid_to_str (lp->ptid));
	}
    }
  while (pid == GET_LWP (lp->ptid));

  gdb_assert (pid == -1 && errno == ECHILD);
  return 0;
}

static void
linux_nat_kill (void)
{
  /* Kill all LWP's ...  */
  iterate_over_lwps (kill_callback, NULL);

  /* ... and wait until we've flushed all events.  */
  iterate_over_lwps (kill_wait_callback, NULL);

  target_mourn_inferior ();
}

static void
linux_nat_create_inferior (char *exec_file, char *allargs, char **env,
			 int from_tty)
{
  deprecated_child_ops.to_create_inferior (exec_file, allargs, env, from_tty);
}

static void
linux_nat_mourn_inferior (void)
{
  trap_ptid = null_ptid;

  /* Destroy LWP info; it's no longer valid.  */
  init_lwp_list ();

  /* Restore the original signal mask.  */
  sigprocmask (SIG_SETMASK, &normal_mask, NULL);
  sigemptyset (&blocked_mask);

  deprecated_child_ops.to_mourn_inferior ();
}

static int
linux_nat_xfer_memory (CORE_ADDR memaddr, char *myaddr, int len, int write,
		     struct mem_attrib *attrib, struct target_ops *target)
{
  struct cleanup *old_chain = save_inferior_ptid ();
  int xfer;

  if (is_lwp (inferior_ptid))
    inferior_ptid = pid_to_ptid (GET_LWP (inferior_ptid));

  xfer = linux_proc_xfer_memory (memaddr, myaddr, len, write, attrib, target);
  if (xfer == 0)
    xfer = child_xfer_memory (memaddr, myaddr, len, write, attrib, target);

  do_cleanups (old_chain);
  return xfer;
}

static int
linux_nat_thread_alive (ptid_t ptid)
{
  gdb_assert (is_lwp (ptid));

  errno = 0;
  ptrace (PTRACE_PEEKUSER, GET_LWP (ptid), 0, 0);
  if (debug_linux_nat)
    fprintf_unfiltered (gdb_stdlog,
			"LLTA: PTRACE_PEEKUSER %s, 0, 0 (%s)\n",
			target_pid_to_str (ptid),
			errno ? safe_strerror (errno) : "OK");
  if (errno)
    return 0;

  return 1;
}

static char *
linux_nat_pid_to_str (ptid_t ptid)
{
  static char buf[64];

  if (is_lwp (ptid))
    {
      snprintf (buf, sizeof (buf), "LWP %ld", GET_LWP (ptid));
      return buf;
    }

  return normal_pid_to_str (ptid);
}

static void
init_linux_nat_ops (void)
{
#if 0
  linux_nat_ops.to_open = linux_nat_open;
#endif
  linux_nat_ops.to_shortname = "lwp-layer";
  linux_nat_ops.to_longname = "lwp-layer";
  linux_nat_ops.to_doc = "Low level threads support (LWP layer)";
  linux_nat_ops.to_attach = linux_nat_attach;
  linux_nat_ops.to_detach = linux_nat_detach;
  linux_nat_ops.to_resume = linux_nat_resume;
  linux_nat_ops.to_wait = linux_nat_wait;
  /* fetch_inferior_registers and store_inferior_registers will
     honor the LWP id, so we can use them directly.  */
  linux_nat_ops.to_fetch_registers = fetch_inferior_registers;
  linux_nat_ops.to_store_registers = store_inferior_registers;
  linux_nat_ops.deprecated_xfer_memory = linux_nat_xfer_memory;
  linux_nat_ops.to_kill = linux_nat_kill;
  linux_nat_ops.to_create_inferior = linux_nat_create_inferior;
  linux_nat_ops.to_mourn_inferior = linux_nat_mourn_inferior;
  linux_nat_ops.to_thread_alive = linux_nat_thread_alive;
  linux_nat_ops.to_pid_to_str = linux_nat_pid_to_str;
  linux_nat_ops.to_post_startup_inferior = child_post_startup_inferior;
  linux_nat_ops.to_post_attach = child_post_attach;
  linux_nat_ops.to_insert_fork_catchpoint = child_insert_fork_catchpoint;
  linux_nat_ops.to_insert_vfork_catchpoint = child_insert_vfork_catchpoint;
  linux_nat_ops.to_insert_exec_catchpoint = child_insert_exec_catchpoint;

  linux_nat_ops.to_stratum = thread_stratum;
  linux_nat_ops.to_has_thread_control = tc_schedlock;
  linux_nat_ops.to_magic = OPS_MAGIC;
}

static void
sigchld_handler (int signo)
{
  /* Do nothing.  The only reason for this handler is that it allows
     us to use sigsuspend in linux_nat_wait above to wait for the
     arrival of a SIGCHLD.  */
}

/* Accepts an integer PID; Returns a string representing a file that
   can be opened to get the symbols for the child process.  */

char *
child_pid_to_exec_file (int pid)
{
  char *name1, *name2;

  name1 = xmalloc (MAXPATHLEN);
  name2 = xmalloc (MAXPATHLEN);
  make_cleanup (xfree, name1);
  make_cleanup (xfree, name2);
  memset (name2, 0, MAXPATHLEN);

  sprintf (name1, "/proc/%d/exe", pid);
  if (readlink (name1, name2, MAXPATHLEN) > 0)
    return name2;
  else
    return name1;
}

/* Service function for corefiles and info proc.  */

static int
read_mapping (FILE *mapfile,
	      long long *addr,
	      long long *endaddr,
	      char *permissions,
	      long long *offset,
	      char *device, long long *inode, char *filename)
{
  int ret = fscanf (mapfile, "%llx-%llx %s %llx %s %llx",
		    addr, endaddr, permissions, offset, device, inode);

  if (ret > 0 && ret != EOF && *inode != 0)
    {
      /* Eat everything up to EOL for the filename.  This will prevent
         weird filenames (such as one with embedded whitespace) from
         confusing this code.  It also makes this code more robust in
         respect to annotations the kernel may add after the filename.

         Note the filename is used for informational purposes
         only.  */
      ret += fscanf (mapfile, "%[^\n]\n", filename);
    }
  else
    {
      filename[0] = '\0';	/* no filename */
      fscanf (mapfile, "\n");
    }
  return (ret != 0 && ret != EOF);
}

/* Fills the "to_find_memory_regions" target vector.  Lists the memory
   regions in the inferior for a corefile.  */

static int
linux_nat_find_memory_regions (int (*func) (CORE_ADDR,
					    unsigned long,
					    int, int, int, void *), void *obfd)
{
  long long pid = PIDGET (inferior_ptid);
  char mapsfilename[MAXPATHLEN];
  FILE *mapsfile;
  long long addr, endaddr, size, offset, inode;
  char permissions[8], device[8], filename[MAXPATHLEN];
  int read, write, exec;
  int ret;

  /* Compose the filename for the /proc memory map, and open it.  */
  sprintf (mapsfilename, "/proc/%lld/maps", pid);
  if ((mapsfile = fopen (mapsfilename, "r")) == NULL)
    error ("Could not open %s\n", mapsfilename);

  if (info_verbose)
    fprintf_filtered (gdb_stdout,
		      "Reading memory regions from %s\n", mapsfilename);

  /* Now iterate until end-of-file.  */
  while (read_mapping (mapsfile, &addr, &endaddr, &permissions[0],
		       &offset, &device[0], &inode, &filename[0]))
    {
      size = endaddr - addr;

      /* Get the segment's permissions.  */
      read = (strchr (permissions, 'r') != 0);
      write = (strchr (permissions, 'w') != 0);
      exec = (strchr (permissions, 'x') != 0);

      if (info_verbose)
	{
	  fprintf_filtered (gdb_stdout,
			    "Save segment, %lld bytes at 0x%s (%c%c%c)",
			    size, paddr_nz (addr),
			    read ? 'r' : ' ',
			    write ? 'w' : ' ', exec ? 'x' : ' ');
	  if (filename && filename[0])
	    fprintf_filtered (gdb_stdout, " for %s", filename);
	  fprintf_filtered (gdb_stdout, "\n");
	}

      /* Invoke the callback function to create the corefile
	 segment.  */
      func (addr, size, read, write, exec, obfd);
    }
  fclose (mapsfile);
  return 0;
}

/* Records the thread's register state for the corefile note
   section.  */

static char *
linux_nat_do_thread_registers (bfd *obfd, ptid_t ptid,
			       char *note_data, int *note_size)
{
  gdb_gregset_t gregs;
  gdb_fpregset_t fpregs;
#ifdef FILL_FPXREGSET
  gdb_fpxregset_t fpxregs;
#endif
  unsigned long lwp = ptid_get_lwp (ptid);

  fill_gregset (&gregs, -1);
  note_data = (char *) elfcore_write_prstatus (obfd,
					       note_data,
					       note_size,
					       lwp,
					       stop_signal, &gregs);

  fill_fpregset (&fpregs, -1);
  note_data = (char *) elfcore_write_prfpreg (obfd,
					      note_data,
					      note_size,
					      &fpregs, sizeof (fpregs));
#ifdef FILL_FPXREGSET
  fill_fpxregset (&fpxregs, -1);
  note_data = (char *) elfcore_write_prxfpreg (obfd,
					       note_data,
					       note_size,
					       &fpxregs, sizeof (fpxregs));
#endif
  return note_data;
}

struct linux_nat_corefile_thread_data
{
  bfd *obfd;
  char *note_data;
  int *note_size;
  int num_notes;
};

/* Called by gdbthread.c once per thread.  Records the thread's
   register state for the corefile note section.  */

static int
linux_nat_corefile_thread_callback (struct lwp_info *ti, void *data)
{
  struct linux_nat_corefile_thread_data *args = data;
  ptid_t saved_ptid = inferior_ptid;

  inferior_ptid = ti->ptid;
  registers_changed ();
  target_fetch_registers (-1);	/* FIXME should not be necessary;
				   fill_gregset should do it automatically. */
  args->note_data = linux_nat_do_thread_registers (args->obfd,
						   ti->ptid,
						   args->note_data,
						   args->note_size);
  args->num_notes++;
  inferior_ptid = saved_ptid;
  registers_changed ();
  target_fetch_registers (-1);	/* FIXME should not be necessary;
				   fill_gregset should do it automatically. */
  return 0;
}

/* Records the register state for the corefile note section.  */

static char *
linux_nat_do_registers (bfd *obfd, ptid_t ptid,
			char *note_data, int *note_size)
{
  registers_changed ();
  target_fetch_registers (-1);	/* FIXME should not be necessary;
				   fill_gregset should do it automatically. */
  return linux_nat_do_thread_registers (obfd,
					ptid_build (ptid_get_pid (inferior_ptid),
						    ptid_get_pid (inferior_ptid),
						    0),
					note_data, note_size);
  return note_data;
}

/* Fills the "to_make_corefile_note" target vector.  Builds the note
   section for a corefile, and returns it in a malloc buffer.  */

static char *
linux_nat_make_corefile_notes (bfd *obfd, int *note_size)
{
  struct linux_nat_corefile_thread_data thread_args;
  struct cleanup *old_chain;
  char fname[16] = { '\0' };
  char psargs[80] = { '\0' };
  char *note_data = NULL;
  ptid_t current_ptid = inferior_ptid;
  char *auxv;
  int auxv_len;

  if (get_exec_file (0))
    {
      strncpy (fname, strrchr (get_exec_file (0), '/') + 1, sizeof (fname));
      strncpy (psargs, get_exec_file (0), sizeof (psargs));
      if (get_inferior_args ())
	{
	  strncat (psargs, " ", sizeof (psargs) - strlen (psargs));
	  strncat (psargs, get_inferior_args (),
		   sizeof (psargs) - strlen (psargs));
	}
      note_data = (char *) elfcore_write_prpsinfo (obfd,
						   note_data,
						   note_size, fname, psargs);
    }

  /* Dump information for threads.  */
  thread_args.obfd = obfd;
  thread_args.note_data = note_data;
  thread_args.note_size = note_size;
  thread_args.num_notes = 0;
  iterate_over_lwps (linux_nat_corefile_thread_callback, &thread_args);
  if (thread_args.num_notes == 0)
    {
      /* iterate_over_threads didn't come up with any threads; just
         use inferior_ptid.  */
      note_data = linux_nat_do_registers (obfd, inferior_ptid,
					  note_data, note_size);
    }
  else
    {
      note_data = thread_args.note_data;
    }

  auxv_len = target_auxv_read (&current_target, &auxv);
  if (auxv_len > 0)
    {
      note_data = elfcore_write_note (obfd, note_data, note_size,
				      "CORE", NT_AUXV, auxv, auxv_len);
      xfree (auxv);
    }

  make_cleanup (xfree, note_data);
  return note_data;
}

/* Implement the "info proc" command.  */

static void
linux_nat_info_proc_cmd (char *args, int from_tty)
{
  long long pid = PIDGET (inferior_ptid);
  FILE *procfile;
  char **argv = NULL;
  char buffer[MAXPATHLEN];
  char fname1[MAXPATHLEN], fname2[MAXPATHLEN];
  int cmdline_f = 1;
  int cwd_f = 1;
  int exe_f = 1;
  int mappings_f = 0;
  int environ_f = 0;
  int status_f = 0;
  int stat_f = 0;
  int all = 0;
  struct stat dummy;

  if (args)
    {
      /* Break up 'args' into an argv array.  */
      if ((argv = buildargv (args)) == NULL)
	nomem (0);
      else
	make_cleanup_freeargv (argv);
    }
  while (argv != NULL && *argv != NULL)
    {
      if (isdigit (argv[0][0]))
	{
	  pid = strtoul (argv[0], NULL, 10);
	}
      else if (strncmp (argv[0], "mappings", strlen (argv[0])) == 0)
	{
	  mappings_f = 1;
	}
      else if (strcmp (argv[0], "status") == 0)
	{
	  status_f = 1;
	}
      else if (strcmp (argv[0], "stat") == 0)
	{
	  stat_f = 1;
	}
      else if (strcmp (argv[0], "cmd") == 0)
	{
	  cmdline_f = 1;
	}
      else if (strncmp (argv[0], "exe", strlen (argv[0])) == 0)
	{
	  exe_f = 1;
	}
      else if (strcmp (argv[0], "cwd") == 0)
	{
	  cwd_f = 1;
	}
      else if (strncmp (argv[0], "all", strlen (argv[0])) == 0)
	{
	  all = 1;
	}
      else
	{
	  /* [...] (future options here) */
	}
      argv++;
    }
  if (pid == 0)
    error ("No current process: you must name one.");

  sprintf (fname1, "/proc/%lld", pid);
  if (stat (fname1, &dummy) != 0)
    error ("No /proc directory: '%s'", fname1);

  printf_filtered ("process %lld\n", pid);
  if (cmdline_f || all)
    {
      sprintf (fname1, "/proc/%lld/cmdline", pid);
      if ((procfile = fopen (fname1, "r")) > 0)
	{
	  fgets (buffer, sizeof (buffer), procfile);
	  printf_filtered ("cmdline = '%s'\n", buffer);
	  fclose (procfile);
	}
      else
	warning ("unable to open /proc file '%s'", fname1);
    }
  if (cwd_f || all)
    {
      sprintf (fname1, "/proc/%lld/cwd", pid);
      memset (fname2, 0, sizeof (fname2));
      if (readlink (fname1, fname2, sizeof (fname2)) > 0)
	printf_filtered ("cwd = '%s'\n", fname2);
      else
	warning ("unable to read link '%s'", fname1);
    }
  if (exe_f || all)
    {
      sprintf (fname1, "/proc/%lld/exe", pid);
      memset (fname2, 0, sizeof (fname2));
      if (readlink (fname1, fname2, sizeof (fname2)) > 0)
	printf_filtered ("exe = '%s'\n", fname2);
      else
	warning ("unable to read link '%s'", fname1);
    }
  if (mappings_f || all)
    {
      sprintf (fname1, "/proc/%lld/maps", pid);
      if ((procfile = fopen (fname1, "r")) > 0)
	{
	  long long addr, endaddr, size, offset, inode;
	  char permissions[8], device[8], filename[MAXPATHLEN];

	  printf_filtered ("Mapped address spaces:\n\n");
	  if (TARGET_ADDR_BIT == 32)
	    {
	      printf_filtered ("\t%10s %10s %10s %10s %7s\n",
			   "Start Addr",
			   "  End Addr",
			   "      Size", "    Offset", "objfile");
            }
	  else
            {
	      printf_filtered ("  %18s %18s %10s %10s %7s\n",
			   "Start Addr",
			   "  End Addr",
			   "      Size", "    Offset", "objfile");
	    }

	  while (read_mapping (procfile, &addr, &endaddr, &permissions[0],
			       &offset, &device[0], &inode, &filename[0]))
	    {
	      size = endaddr - addr;

	      /* FIXME: carlton/2003-08-27: Maybe the printf_filtered
		 calls here (and possibly above) should be abstracted
		 out into their own functions?  Andrew suggests using
		 a generic local_address_string instead to print out
		 the addresses; that makes sense to me, too.  */

	      if (TARGET_ADDR_BIT == 32)
	        {
	          printf_filtered ("\t%#10lx %#10lx %#10x %#10x %7s\n",
			       (unsigned long) addr,	/* FIXME: pr_addr */
			       (unsigned long) endaddr,
			       (int) size,
			       (unsigned int) offset,
			       filename[0] ? filename : "");
		}
	      else
	        {
	          printf_filtered ("  %#18lx %#18lx %#10x %#10x %7s\n",
			       (unsigned long) addr,	/* FIXME: pr_addr */
			       (unsigned long) endaddr,
			       (int) size,
			       (unsigned int) offset,
			       filename[0] ? filename : "");
	        }
	    }

	  fclose (procfile);
	}
      else
	warning ("unable to open /proc file '%s'", fname1);
    }
  if (status_f || all)
    {
      sprintf (fname1, "/proc/%lld/status", pid);
      if ((procfile = fopen (fname1, "r")) > 0)
	{
	  while (fgets (buffer, sizeof (buffer), procfile) != NULL)
	    puts_filtered (buffer);
	  fclose (procfile);
	}
      else
	warning ("unable to open /proc file '%s'", fname1);
    }
  if (stat_f || all)
    {
      sprintf (fname1, "/proc/%lld/stat", pid);
      if ((procfile = fopen (fname1, "r")) > 0)
	{
	  int itmp;
	  char ctmp;

	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("Process: %d\n", itmp);
	  if (fscanf (procfile, "%s ", &buffer[0]) > 0)
	    printf_filtered ("Exec file: %s\n", buffer);
	  if (fscanf (procfile, "%c ", &ctmp) > 0)
	    printf_filtered ("State: %c\n", ctmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("Parent process: %d\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("Process group: %d\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("Session id: %d\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("TTY: %d\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("TTY owner process group: %d\n", itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Flags: 0x%x\n", itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Minor faults (no memory page): %u\n",
			     (unsigned int) itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Minor faults, children: %u\n",
			     (unsigned int) itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Major faults (memory page faults): %u\n",
			     (unsigned int) itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Major faults, children: %u\n",
			     (unsigned int) itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("utime: %d\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("stime: %d\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("utime, children: %d\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("stime, children: %d\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("jiffies remaining in current time slice: %d\n",
			     itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("'nice' value: %d\n", itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("jiffies until next timeout: %u\n",
			     (unsigned int) itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("jiffies until next SIGALRM: %u\n",
			     (unsigned int) itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("start time (jiffies since system boot): %d\n",
			     itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Virtual memory size: %u\n",
			     (unsigned int) itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Resident set size: %u\n", (unsigned int) itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("rlim: %u\n", (unsigned int) itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Start of text: 0x%x\n", itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("End of text: 0x%x\n", itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Start of stack: 0x%x\n", itmp);
#if 0				/* Don't know how architecture-dependent the rest is...
				   Anyway the signal bitmap info is available from "status".  */
	  if (fscanf (procfile, "%u ", &itmp) > 0)	/* FIXME arch? */
	    printf_filtered ("Kernel stack pointer: 0x%x\n", itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)	/* FIXME arch? */
	    printf_filtered ("Kernel instr pointer: 0x%x\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("Pending signals bitmap: 0x%x\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("Blocked signals bitmap: 0x%x\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("Ignored signals bitmap: 0x%x\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("Catched signals bitmap: 0x%x\n", itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)	/* FIXME arch? */
	    printf_filtered ("wchan (system call): 0x%x\n", itmp);
#endif
	  fclose (procfile);
	}
      else
	warning ("unable to open /proc file '%s'", fname1);
    }
}

int
linux_proc_xfer_memory (CORE_ADDR addr, char *myaddr, int len, int write,
			struct mem_attrib *attrib, struct target_ops *target)
{
  int fd, ret;
  char filename[64];

  if (write)
    return 0;

  /* Don't bother for one word.  */
  if (len < 3 * sizeof (long))
    return 0;

  /* We could keep this file open and cache it - possibly one per
     thread.  That requires some juggling, but is even faster.  */
  sprintf (filename, "/proc/%d/mem", PIDGET (inferior_ptid));
  fd = open (filename, O_RDONLY | O_LARGEFILE);
  if (fd == -1)
    return 0;

  /* If pread64 is available, use it.  It's faster if the kernel
     supports it (only one syscall), and it's 64-bit safe even on
     32-bit platforms (for instance, SPARC debugging a SPARC64
     application).  */
#ifdef HAVE_PREAD64
  if (pread64 (fd, myaddr, len, addr) != len)
#else
  if (lseek (fd, addr, SEEK_SET) == -1 || read (fd, myaddr, len) != len)
#endif
    ret = 0;
  else
    ret = len;

  close (fd);
  return ret;
}

/* Parse LINE as a signal set and add its set bits to SIGS.  */

static void
add_line_to_sigset (const char *line, sigset_t *sigs)
{
  int len = strlen (line) - 1;
  const char *p;
  int signum;

  if (line[len] != '\n')
    error ("Could not parse signal set: %s", line);

  p = line;
  signum = len * 4;
  while (len-- > 0)
    {
      int digit;

      if (*p >= '0' && *p <= '9')
	digit = *p - '0';
      else if (*p >= 'a' && *p <= 'f')
	digit = *p - 'a' + 10;
      else
	error ("Could not parse signal set: %s", line);

      signum -= 4;

      if (digit & 1)
	sigaddset (sigs, signum + 1);
      if (digit & 2)
	sigaddset (sigs, signum + 2);
      if (digit & 4)
	sigaddset (sigs, signum + 3);
      if (digit & 8)
	sigaddset (sigs, signum + 4);

      p++;
    }
}

/* Find process PID's pending signals from /proc/pid/status and set
   SIGS to match.  */

void
linux_proc_pending_signals (int pid, sigset_t *pending, sigset_t *blocked, sigset_t *ignored)
{
  FILE *procfile;
  char buffer[MAXPATHLEN], fname[MAXPATHLEN];
  int signum;

  sigemptyset (pending);
  sigemptyset (blocked);
  sigemptyset (ignored);
  sprintf (fname, "/proc/%d/status", pid);
  procfile = fopen (fname, "r");
  if (procfile == NULL)
    error ("Could not open %s", fname);

  while (fgets (buffer, MAXPATHLEN, procfile) != NULL)
    {
      /* Normal queued signals are on the SigPnd line in the status
	 file.  However, 2.6 kernels also have a "shared" pending
	 queue for delivering signals to a thread group, so check for
	 a ShdPnd line also.

	 Unfortunately some Red Hat kernels include the shared pending
	 queue but not the ShdPnd status field.  */

      if (strncmp (buffer, "SigPnd:\t", 8) == 0)
	add_line_to_sigset (buffer + 8, pending);
      else if (strncmp (buffer, "ShdPnd:\t", 8) == 0)
	add_line_to_sigset (buffer + 8, pending);
      else if (strncmp (buffer, "SigBlk:\t", 8) == 0)
	add_line_to_sigset (buffer + 8, blocked);
      else if (strncmp (buffer, "SigIgn:\t", 8) == 0)
	add_line_to_sigset (buffer + 8, ignored);
    }

  fclose (procfile);
}

void
_initialize_linux_nat (void)
{
  struct sigaction action;
  extern void thread_db_init (struct target_ops *);

  deprecated_child_ops.to_find_memory_regions = linux_nat_find_memory_regions;
  deprecated_child_ops.to_make_corefile_notes = linux_nat_make_corefile_notes;

  add_info ("proc", linux_nat_info_proc_cmd,
	    "Show /proc process information about any running process.\n\
Specify any process id, or use the program being debugged by default.\n\
Specify any of the following keywords for detailed info:\n\
  mappings -- list of mapped memory regions.\n\
  stat     -- list a bunch of random process info.\n\
  status   -- list a different bunch of random process info.\n\
  all      -- list all available /proc info.");

  init_linux_nat_ops ();
  add_target (&linux_nat_ops);
  thread_db_init (&linux_nat_ops);

  /* Save the original signal mask.  */
  sigprocmask (SIG_SETMASK, NULL, &normal_mask);

  action.sa_handler = sigchld_handler;
  sigemptyset (&action.sa_mask);
  action.sa_flags = 0;
  sigaction (SIGCHLD, &action, NULL);

  /* Make sure we don't block SIGCHLD during a sigsuspend.  */
  sigprocmask (SIG_SETMASK, NULL, &suspend_mask);
  sigdelset (&suspend_mask, SIGCHLD);

  sigemptyset (&blocked_mask);

  deprecated_add_show_from_set
    (add_set_cmd ("lin-lwp", no_class, var_zinteger,
		  (char *) &debug_linux_nat,
		  "Set debugging of GNU/Linux lwp module.\n\
Enables printf debugging output.\n", &setdebuglist), &showdebuglist);
}


/* FIXME: kettenis/2000-08-26: The stuff on this page is specific to
   the GNU/Linux Threads library and therefore doesn't really belong
   here.  */

/* Read variable NAME in the target and return its value if found.
   Otherwise return zero.  It is assumed that the type of the variable
   is `int'.  */

static int
get_signo (const char *name)
{
  struct minimal_symbol *ms;
  int signo;

  ms = lookup_minimal_symbol (name, NULL, NULL);
  if (ms == NULL)
    return 0;

  if (target_read_memory (SYMBOL_VALUE_ADDRESS (ms), (char *) &signo,
			  sizeof (signo)) != 0)
    return 0;

  return signo;
}

/* Return the set of signals used by the threads library in *SET.  */

void
lin_thread_get_thread_signals (sigset_t *set)
{
  struct sigaction action;
  int restart, cancel;

  sigemptyset (set);

  restart = get_signo ("__pthread_sig_restart");
  if (restart == 0)
    return;

  cancel = get_signo ("__pthread_sig_cancel");
  if (cancel == 0)
    return;

  sigaddset (set, restart);
  sigaddset (set, cancel);

  /* The GNU/Linux Threads library makes terminating threads send a
     special "cancel" signal instead of SIGCHLD.  Make sure we catch
     those (to prevent them from terminating GDB itself, which is
     likely to be their default action) and treat them the same way as
     SIGCHLD.  */

  action.sa_handler = sigchld_handler;
  sigemptyset (&action.sa_mask);
  action.sa_flags = 0;
  sigaction (cancel, &action, NULL);

  /* We block the "cancel" signal throughout this code ...  */
  sigaddset (&blocked_mask, cancel);
  sigprocmask (SIG_BLOCK, &blocked_mask, NULL);

  /* ... except during a sigsuspend.  */
  sigdelset (&suspend_mask, cancel);
}

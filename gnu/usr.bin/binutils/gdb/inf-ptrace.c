/* Low level Unix child interface to ptrace, for GDB when running under Unix.

   Copyright 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996,
   1998, 1999, 2000, 2001, 2002, 2004 Free Software Foundation, Inc.

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
#include "observer.h"
#include "gdb_ptrace.h"
#include "inflow.h"
#include "inferior.h"
#include "regcache.h"
#include "command.h"
#include "gdbcore.h"
#include "inf-child.h"
#include "gdbcmd.h"
#include "gdb_string.h"

#include "gdb_wait.h"
#include <signal.h>

/* HACK: Save the ptrace ops returned by ptrace_target.  */
static struct target_ops *ptrace_ops_hack;

static void
inf_ptrace_kill_inferior (void)
{
  int status;
  int pid = PIDGET (inferior_ptid);

  if (pid == 0)
    return;

  /* This once used to call "kill" to kill the inferior just in case
     the inferior was still running.  As others have noted in the past
     (kingdon) there shouldn't be any way to get here if the inferior
     is still running -- else there's a major problem elsewere in gdb
     and it needs to be fixed.

     The kill call causes problems under hpux10, so it's been removed;
     if this causes problems we'll deal with them as they arise.  */
  ptrace (PT_KILL, pid, (PTRACE_TYPE_ARG3) 0, 0);
  wait (&status);
  target_mourn_inferior ();
}

/* Resume execution of the inferior process.  If STEP is nonzero,
   single-step it.  If SIGNAL is nonzero, give it that signal.  */

static void
inf_ptrace_resume (ptid_t ptid, int step, enum target_signal signal)
{
  int request = PT_CONTINUE;
  int pid = PIDGET (ptid);

  if (pid == -1)
    /* Resume all threads.  */
    /* I think this only gets used in the non-threaded case, where
       "resume all threads" and "resume inferior_ptid" are the
       same.  */
    pid = PIDGET (inferior_ptid);

  if (step)
    {
      /* If this system does not support PT_STEP, a higher level
         function will have called single_step() to transmute the step
         request into a continue request (by setting breakpoints on
         all possible successor instructions), so we don't have to
         worry about that here.  */
      request = PT_STEP;
    }

  /* An address of (PTRACE_TYPE_ARG3)1 tells ptrace to continue from
     where it was.  If GDB wanted it to start some other way, we have
     already written a new PC value to the child.  */
  errno = 0;
  ptrace (request, pid, (PTRACE_TYPE_ARG3) 1, target_signal_to_host (signal));
  if (errno != 0)
    perror_with_name ("ptrace");
}

/* Wait for child to do something.  Return pid of child, or -1 in case
   of error; store status through argument pointer OURSTATUS.  */

static ptid_t
inf_ptrace_wait (ptid_t ptid, struct target_waitstatus *ourstatus)
{
  int save_errno;
  int status;
  char *execd_pathname = NULL;
  int exit_status;
  int related_pid;
  int syscall_id;
  enum target_waitkind kind;
  int pid;

  do
    {
      set_sigint_trap ();	/* Causes SIGINT to be passed on to the
				   attached process. */
      set_sigio_trap ();

      pid = wait (&status);

      save_errno = errno;

      clear_sigio_trap ();

      clear_sigint_trap ();

      if (pid == -1)
	{
	  if (save_errno == EINTR)
	    continue;

	  fprintf_unfiltered (gdb_stderr,
			      "Child process unexpectedly missing: %s.\n",
			      safe_strerror (save_errno));

	  /* Claim it exited with unknown signal.  */
	  ourstatus->kind = TARGET_WAITKIND_SIGNALLED;
	  ourstatus->value.sig = TARGET_SIGNAL_UNKNOWN;
	  return pid_to_ptid (-1);
	}

      /* Did it exit?
       */
      if (target_has_exited (pid, status, &exit_status))
	{
	  /* ??rehrauer: For now, ignore this. */
	  continue;
	}

      if (!target_thread_alive (pid_to_ptid (pid)))
	{
	  ourstatus->kind = TARGET_WAITKIND_SPURIOUS;
	  return pid_to_ptid (pid);
	}
    }
  while (pid != PIDGET (inferior_ptid));	/* Some other child died or stopped */

  store_waitstatus (ourstatus, status);
  return pid_to_ptid (pid);
}

/* Check to see if the given thread is alive.

   FIXME: Is kill() ever the right way to do this?  I doubt it, but
   for now we're going to try and be compatable with the old thread
   code.  */

static int
inf_ptrace_thread_alive (ptid_t ptid)
{
  pid_t pid = PIDGET (ptid);

  return (kill (pid, 0) != -1);
}

/* Attach to process PID, then initialize for debugging it.  */

static void
inf_ptrace_attach (char *args, int from_tty)
{
  char *exec_file;
  int pid;
  char *dummy;

  if (!args)
    error_no_arg ("process-id to attach");

  dummy = args;
  pid = strtol (args, &dummy, 0);
  /* Some targets don't set errno on errors, grrr! */
  if (pid == 0 && args == dummy)
    error ("Illegal process-id: %s\n", args);

  if (pid == getpid ())		/* Trying to masturbate? */
    error ("I refuse to debug myself!");

  if (from_tty)
    {
      exec_file = (char *) get_exec_file (0);

      if (exec_file)
	printf_unfiltered ("Attaching to program: %s, %s\n", exec_file,
			   target_pid_to_str (pid_to_ptid (pid)));
      else
	printf_unfiltered ("Attaching to %s\n",
			   target_pid_to_str (pid_to_ptid (pid)));

      gdb_flush (gdb_stdout);
    }

#ifdef PT_ATTACH
  errno = 0;
  ptrace (PT_ATTACH, pid, (PTRACE_TYPE_ARG3) 0, 0);
  if (errno != 0)
    perror_with_name ("ptrace");
  attach_flag = 1;
#else
  error ("This system does not support attaching to a process");
#endif

  inferior_ptid = pid_to_ptid (pid);
  push_target (ptrace_ops_hack);
}

static void
inf_ptrace_post_attach (int pid)
{
  /* This version of Unix doesn't require a meaningful "post attach"
     operation by a debugger.  */
}

/* Take a program previously attached to and detaches it.  The program
   resumes execution and will no longer stop on signals, etc.  We'd
   better not have left any breakpoints in the program or it'll die
   when it hits one.  For this to work, it may be necessary for the
   process to have been previously attached.  It *might* work if the
   program was started via the normal ptrace (PTRACE_TRACEME).  */

static void
inf_ptrace_detach (char *args, int from_tty)
{
  int sig = 0;
  int pid = PIDGET (inferior_ptid);

  if (from_tty)
    {
      char *exec_file = get_exec_file (0);
      if (exec_file == 0)
	exec_file = "";
      printf_unfiltered ("Detaching from program: %s, %s\n", exec_file,
			 target_pid_to_str (pid_to_ptid (pid)));
      gdb_flush (gdb_stdout);
    }
  if (args)
    sig = atoi (args);

#ifdef PT_DETACH
  errno = 0;
  ptrace (PT_DETACH, pid, (PTRACE_TYPE_ARG3) 1, sig);
  if (errno != 0)
    perror_with_name ("ptrace");
  attach_flag = 0;
#else
  error ("This system does not support detaching from a process");
#endif

  inferior_ptid = null_ptid;
  unpush_target (ptrace_ops_hack);
}

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On
   machines which store all the registers in one fell swoop, this
   makes sure that registers contains all the registers from the
   program being debugged.  */

static void
inf_ptrace_prepare_to_store (void)
{
}

/* Print status information about what we're accessing.  */

static void
inf_ptrace_files_info (struct target_ops *ignore)
{
  printf_unfiltered ("\tUsing the running image of %s %s.\n",
		     attach_flag ? "attached" : "child",
		     target_pid_to_str (inferior_ptid));
}

static void
inf_ptrace_open (char *arg, int from_tty)
{
  error ("Use the \"run\" command to start a Unix child process.");
}

/* Stub function which causes the inferior that runs it, to be ptrace-able
   by its parent process.  */

static void
inf_ptrace_me (void)
{
  /* "Trace me, Dr. Memory!" */
  ptrace (0, 0, (PTRACE_TYPE_ARG3) 0, 0);
}

/* Stub function which causes the GDB that runs it, to start ptrace-ing
   the child process.  */

static void
inf_ptrace_him (int pid)
{
  push_target (ptrace_ops_hack);

  /* On some targets, there must be some explicit synchronization
     between the parent and child processes after the debugger
     forks, and before the child execs the debuggee program.  This
     call basically gives permission for the child to exec.
   */

  target_acknowledge_created_inferior (pid);

  /* START_INFERIOR_TRAPS_EXPECTED is defined in inferior.h,
   * and will be 1 or 2 depending on whether we're starting
   * without or with a shell.
   */
  startup_inferior (START_INFERIOR_TRAPS_EXPECTED);

  /* On some targets, there must be some explicit actions taken after
     the inferior has been started up.
   */
  target_post_startup_inferior (pid_to_ptid (pid));
}

/* Start an inferior Unix child process and sets inferior_ptid to its
   pid.  EXEC_FILE is the file to run.  ALLARGS is a string containing
   the arguments to the program.  ENV is the environment vector to
   pass.  Errors reported with error().  */

static void
inf_ptrace_create_inferior (char *exec_file, char *allargs, char **env,
			    int from_tty)
{
  fork_inferior (exec_file, allargs, env, inf_ptrace_me, inf_ptrace_him,
		 NULL, NULL);
  /* We are at the first instruction we care about.  */
  observer_notify_inferior_created (&current_target, from_tty);
  /* Pedal to the metal... */
  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_0, 0);
}

static void
inf_ptrace_post_startup_inferior (ptid_t ptid)
{
  /* This version of Unix doesn't require a meaningful "post startup inferior"
     operation by a debugger.
   */
}

static void
inf_ptrace_acknowledge_created_inferior (int pid)
{
  /* This version of Unix doesn't require a meaningful "acknowledge created inferior"
     operation by a debugger.
   */
}

static int
inf_ptrace_insert_fork_catchpoint (int pid)
{
  /* This version of Unix doesn't support notification of fork events.  */
  return 0;
}

static int
inf_ptrace_remove_fork_catchpoint (int pid)
{
  /* This version of Unix doesn't support notification of fork events.  */
  return 0;
}

static int
inf_ptrace_insert_vfork_catchpoint (int pid)
{
  /* This version of Unix doesn't support notification of vfork events.  */
  return 0;
}

static int
inf_ptrace_remove_vfork_catchpoint (int pid)
{
  /* This version of Unix doesn't support notification of vfork events.  */
  return 0;
}

static int
inf_ptrace_follow_fork (int follow_child)
{
  /* This version of Unix doesn't support following fork or vfork events.  */
  return 0;
}

static int
inf_ptrace_insert_exec_catchpoint (int pid)
{
  /* This version of Unix doesn't support notification of exec events.  */
  return 0;
}

static int
inf_ptrace_remove_exec_catchpoint (int pid)
{
  /* This version of Unix doesn't support notification of exec events.  */
  return 0;
}

static int
inf_ptrace_reported_exec_events_per_exec_call (void)
{
  /* This version of Unix doesn't support notification of exec events.
   */
  return 1;
}

static int
inf_ptrace_has_exited (int pid, int wait_status, int *exit_status)
{
  if (WIFEXITED (wait_status))
    {
      *exit_status = WEXITSTATUS (wait_status);
      return 1;
    }

  if (WIFSIGNALED (wait_status))
    {
      *exit_status = 0;		/* ?? Don't know what else to say here. */
      return 1;
    }

  /* ?? Do we really need to consult the event state, too?  Assume the
     wait_state alone suffices.
   */
  return 0;
}

static void
inf_ptrace_mourn_inferior (void)
{
  unpush_target (ptrace_ops_hack);
  generic_mourn_inferior ();
}

static int
inf_ptrace_can_run (void)
{
  return 1;
}

/* Send a SIGINT to the process group.  This acts just like the user
   typed a ^C on the controlling terminal.

   XXX - This may not be correct for all systems.  Some may want to
   use killpg() instead of kill (-pgrp). */

static void
inf_ptrace_stop (void)
{
  kill (-inferior_process_group, SIGINT);
}

/* Perform a partial transfer to/from the specified object.  For
   memory transfers, fall back to the old memory xfer functions.  */

static LONGEST
inf_ptrace_xfer_partial (struct target_ops *ops, enum target_object object,
			 const char *annex, void *readbuf,
			 const void *writebuf, ULONGEST offset, LONGEST len)
{
  switch (object)
    {
    case TARGET_OBJECT_MEMORY:
#ifdef PT_IO
      /* OpenBSD 3.1, NetBSD 1.6 and FreeBSD 5.0 have a new PT_IO
	 request that promises to be much more efficient in reading
	 and writing data in the traced process's address space.  */
      {
	struct ptrace_io_desc piod;
	
	/* NOTE: We assume that there are no distinct address spaces
	   for instruction and data.  */
	piod.piod_op = writebuf ? PIOD_WRITE_D : PIOD_READ_D;
	piod.piod_addr = writebuf ? (void *) writebuf : readbuf;
	piod.piod_offs = (void *) (long) offset;
	piod.piod_len = len;

	errno = 0;
	if (ptrace (PT_IO, PIDGET (inferior_ptid), (caddr_t) &piod, 0) == 0)
	  /* Return the actual number of bytes read or written.  */
	  return piod.piod_len;
	/* If the PT_IO request is somehow not supported, fallback on
	   using PT_WRITE_D/PT_READ_D.  Otherwise we will return zero
	   to indicate failure.  */
	if (errno != EINVAL)
	  return 0;
      }
#endif
      {
	union
	{
	  PTRACE_TYPE_RET word;
	  unsigned char byte[sizeof (PTRACE_TYPE_RET)];
	} buffer;
	ULONGEST rounded_offset;
	LONGEST partial_len;
	
	/* Round the start offset down to the next long word
	   boundary.  */
	rounded_offset = offset & -(ULONGEST) sizeof (PTRACE_TYPE_RET);
	
	/* Since ptrace will transfer a single word starting at that
	   rounded_offset the partial_len needs to be adjusted down to
	   that (remember this function only does a single transfer).
	   Should the required length be even less, adjust it down
	   again.  */
	partial_len = (rounded_offset + sizeof (PTRACE_TYPE_RET)) - offset;
	if (partial_len > len)
	  partial_len = len;
	
	if (writebuf)
	  {
	    /* If OFFSET:PARTIAL_LEN is smaller than
	       ROUNDED_OFFSET:WORDSIZE then a read/modify write will
	       be needed.  Read in the entire word.  */
	    if (rounded_offset < offset
		|| (offset + partial_len
		    < rounded_offset + sizeof (PTRACE_TYPE_RET)))
	      /* Need part of initial word -- fetch it.  */
	      buffer.word = ptrace (PT_READ_I, PIDGET (inferior_ptid),
				    (PTRACE_TYPE_ARG3) (long) rounded_offset,
				    0);
	    
	    /* Copy data to be written over corresponding part of
	       buffer.  */
	    memcpy (buffer.byte + (offset - rounded_offset), writebuf, partial_len);
	    
	    errno = 0;
	    ptrace (PT_WRITE_D, PIDGET (inferior_ptid),
		    (PTRACE_TYPE_ARG3) (long) rounded_offset,
		    buffer.word);
	    if (errno)
	      {
		/* Using the appropriate one (I or D) is necessary for
		   Gould NP1, at least.  */
		errno = 0;
		ptrace (PT_WRITE_I, PIDGET (inferior_ptid),
			(PTRACE_TYPE_ARG3) (long) rounded_offset,
			buffer.word);
		if (errno)
		  return 0;
	      }
	  }
	if (readbuf)
	  {
	    errno = 0;
	    buffer.word = ptrace (PT_READ_I, PIDGET (inferior_ptid),
				  (PTRACE_TYPE_ARG3) (long) rounded_offset, 0);
	    if (errno)
	      return 0;
	    /* Copy appropriate bytes out of the buffer.  */
	    memcpy (readbuf, buffer.byte + (offset - rounded_offset),
		    partial_len);
	  }
	return partial_len;
      }

    case TARGET_OBJECT_UNWIND_TABLE:
      return -1;

    case TARGET_OBJECT_AUXV:
      return -1;

    case TARGET_OBJECT_WCOOKIE:
      return -1;

    default:
      return -1;
    }
}

static char *
inf_ptrace_pid_to_str (ptid_t ptid)
{
  return normal_pid_to_str (ptid);
}

struct target_ops *
inf_ptrace_target (void)
{
  struct target_ops *t = inf_child_target ();
  t->to_open = inf_ptrace_open;
  t->to_attach = inf_ptrace_attach;
  t->to_post_attach = inf_ptrace_post_attach;
  t->to_detach = inf_ptrace_detach;
  t->to_resume = inf_ptrace_resume;
  t->to_wait = inf_ptrace_wait;
  t->to_prepare_to_store = inf_ptrace_prepare_to_store;
  t->to_xfer_partial = inf_ptrace_xfer_partial;
  t->to_files_info = inf_ptrace_files_info;
  t->to_kill = inf_ptrace_kill_inferior;
  t->to_create_inferior = inf_ptrace_create_inferior;
  t->to_post_startup_inferior = inf_ptrace_post_startup_inferior;
  t->to_acknowledge_created_inferior =
    inf_ptrace_acknowledge_created_inferior;
  t->to_insert_fork_catchpoint = inf_ptrace_insert_fork_catchpoint;
  t->to_remove_fork_catchpoint = inf_ptrace_remove_fork_catchpoint;
  t->to_insert_vfork_catchpoint = inf_ptrace_insert_vfork_catchpoint;
  t->to_remove_vfork_catchpoint = inf_ptrace_remove_vfork_catchpoint;
  t->to_follow_fork = inf_ptrace_follow_fork;
  t->to_insert_exec_catchpoint = inf_ptrace_insert_exec_catchpoint;
  t->to_remove_exec_catchpoint = inf_ptrace_remove_exec_catchpoint;
  t->to_reported_exec_events_per_exec_call =
    inf_ptrace_reported_exec_events_per_exec_call;
  t->to_has_exited = inf_ptrace_has_exited;
  t->to_mourn_inferior = inf_ptrace_mourn_inferior;
  t->to_can_run = inf_ptrace_can_run;
  t->to_thread_alive = inf_ptrace_thread_alive;
  t->to_pid_to_str = inf_ptrace_pid_to_str;
  t->to_stop = inf_ptrace_stop;
  t->to_stratum = process_stratum;
  t->to_has_all_memory = 1;
  t->to_has_memory = 1;
  t->to_has_stack = 1;
  t->to_has_registers = 1;
  t->to_has_execution = 1;
  t->to_magic = OPS_MAGIC;
  ptrace_ops_hack = t;
  return t;
}

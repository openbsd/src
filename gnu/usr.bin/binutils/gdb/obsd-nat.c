/* Native-dependent code for OpenBSD.

   Copyright 2012 Free Software Foundation, Inc.

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
#include "gdbthread.h"
#include "inferior.h"
#include "target.h"

#include "gdb_assert.h"
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include "obsd-nat.h"

char *
obsd_pid_to_str (ptid_t ptid)
{
  if (ptid_get_lwp (ptid) != 0)
    {
      static char buf[64];

      xsnprintf (buf, sizeof buf, "thread %ld", ptid_get_lwp (ptid));
      return buf;
    }

  return normal_pid_to_str (ptid);
}

void
obsd_find_new_threads (void)
{
  pid_t pid = ptid_get_pid (inferior_ptid);
  struct ptrace_thread_state pts;

  if (ptrace(PT_GET_THREAD_FIRST, pid, (caddr_t)&pts, sizeof pts) == -1)
    perror_with_name (("ptrace"));

  while (pts.pts_tid != -1)
    {
      ptid_t ptid = ptid_build (pid, pts.pts_tid, 0);

      if (!in_thread_list (ptid))
	{
	  /* HACK: Twiddle INFERIOR_PTID such that the initial thread
	     of a process isn't recognized as a new thread.  */
	  if (ptid_get_lwp (inferior_ptid) == 0)
	    inferior_ptid = ptid;
	  add_thread (ptid);
	}

      if (ptrace(PT_GET_THREAD_NEXT, pid, (caddr_t)&pts, sizeof pts) == -1)
	perror_with_name (("ptrace"));
    }
}

ptid_t
obsd_wait (ptid_t ptid, struct target_waitstatus *ourstatus)
{
  pid_t pid;
  int status, save_errno;

  do
    {
      set_sigint_trap ();
      set_sigio_trap ();

      do
	{
	  pid = waitpid (ptid_get_pid (ptid), &status, 0);
	  save_errno = errno;
	}
      while (pid == -1 && errno == EINTR);

      clear_sigio_trap ();
      clear_sigint_trap ();

      if (pid == -1)
	{
	  fprintf_unfiltered (gdb_stderr,
			      _("Child process unexpectedly missing: %s.\n"),
			      safe_strerror (save_errno));

	  /* Claim it exited with unknown signal.  */
	  ourstatus->kind = TARGET_WAITKIND_SIGNALLED;
	  ourstatus->value.sig = TARGET_SIGNAL_UNKNOWN;
	  return minus_one_ptid;
	}

      /* Ignore terminated detached child processes.  */
      if (!WIFSTOPPED (status) && pid != ptid_get_pid (inferior_ptid))
	pid = -1;
    }
  while (pid == -1);

  ptid = pid_to_ptid (pid);

  if (WIFSTOPPED (status))
    {
      ptrace_state_t pe;
      pid_t fpid;

      if (ptrace (PT_GET_PROCESS_STATE, pid, (caddr_t)&pe, sizeof pe) == -1)
	perror_with_name (("ptrace"));

      switch (pe.pe_report_event)
	{
	case PTRACE_FORK:
	  ourstatus->kind = TARGET_WAITKIND_FORKED;
	  ourstatus->value.related_pid = pe.pe_other_pid;

	  /* Make sure the other end of the fork is stopped too.  */
	  fpid = waitpid (pe.pe_other_pid, &status, 0);
	  if (fpid == -1)
	    perror_with_name (("waitpid"));

	  if (ptrace (PT_GET_PROCESS_STATE, fpid,
		      (caddr_t)&pe, sizeof pe) == -1)
	    perror_with_name (("ptrace"));

	  gdb_assert (pe.pe_report_event == PTRACE_FORK);
	  gdb_assert (pe.pe_other_pid == pid);
	  if (fpid == ptid_get_pid (inferior_ptid))
	    {
	      ourstatus->value.related_pid = pe.pe_other_pid;
	      return pid_to_ptid (fpid);
	    }

	  return pid_to_ptid (pid);
	}

      ptid = ptid_build (pid, pe.pe_tid, 0);
      if (!in_thread_list (ptid))
	{
	  /* HACK: Twiddle INFERIOR_PTID such that the initial thread
	     of a process isn't recognized as a new thread.  */
	  if (ptid_get_lwp (inferior_ptid) == 0)
	    inferior_ptid = ptid;
	  add_thread (ptid);
	}
    }

  store_waitstatus (ourstatus, status);
  return ptid;
}

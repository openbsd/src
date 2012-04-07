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

#include <sys/types.h>
#include <sys/ptrace.h>

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

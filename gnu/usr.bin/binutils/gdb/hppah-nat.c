/* Native support code for HPUX PA-RISC.
   Copyright 1986, 1987, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996,
   1998, 1999, 2000, 2001
   Free Software Foundation, Inc.

   Contributed by the Center for Software Science at the
   University of Utah (pa-gdb-bugs@cs.utah.edu).

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
#include <sys/ptrace.h>
#include "gdbcore.h"
#include "gdb_wait.h"
#include "regcache.h"
#include "gdb_string.h"
#include <signal.h>

extern int hpux_has_forked (int pid, int *childpid);
extern int hpux_has_vforked (int pid, int *childpid);
extern int hpux_has_execd (int pid, char **execd_pathname);
extern int hpux_has_syscall_event (int pid, enum target_waitkind *kind,
				   int *syscall_id);

static CORE_ADDR text_end;

void
deprecated_hpux_text_end (struct target_ops *exec_ops)
{
  struct section_table *p;

  /* Set text_end to the highest address of the end of any readonly
     code section.  */
  /* FIXME: The comment above does not match the code.  The code
     checks for sections with are either code *or* readonly.  */
  text_end = (CORE_ADDR) 0;
  for (p = exec_ops->to_sections; p < exec_ops->to_sections_end; p++)
    if (bfd_get_section_flags (p->bfd, p->the_bfd_section)
	& (SEC_CODE | SEC_READONLY))
      {
	if (text_end < p->endaddr)
	  text_end = p->endaddr;
      }
}


static void fetch_register (int);

void
fetch_inferior_registers (int regno)
{
  if (regno == -1)
    for (regno = 0; regno < NUM_REGS; regno++)
      fetch_register (regno);
  else
    fetch_register (regno);
}

/* Our own version of the offsetof macro, since we can't assume ANSI C.  */
#define HPPAH_OFFSETOF(type, member) ((int) (&((type *) 0)->member))

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (int regno)
{
  unsigned int regaddr;
  char buf[80];
  int i;
  unsigned int offset = U_REGS_OFFSET;
  int scratch;

  if (regno >= 0)
    {
      unsigned int addr, len, offset;

      if (CANNOT_STORE_REGISTER (regno))
	return;

      offset = 0;
      len = DEPRECATED_REGISTER_RAW_SIZE (regno);

      /* Requests for register zero actually want the save_state's
	 ss_flags member.  As RM says: "Oh, what a hack!"  */
      if (regno == 0)
	{
	  save_state_t ss;
	  addr = HPPAH_OFFSETOF (save_state_t, ss_flags);
	  len = sizeof (ss.ss_flags);

	  /* Note that ss_flags is always an int, no matter what
	     DEPRECATED_REGISTER_RAW_SIZE(0) says.  Assuming all HP-UX
	     PA machines are big-endian, put it at the least
	     significant end of the value, and zap the rest of the
	     buffer.  */
	  offset = DEPRECATED_REGISTER_RAW_SIZE (0) - len;
	}

      /* Floating-point registers come from the ss_fpblock area.  */
      else if (regno >= FP0_REGNUM)
	addr = (HPPAH_OFFSETOF (save_state_t, ss_fpblock) 
		+ (DEPRECATED_REGISTER_BYTE (regno) - DEPRECATED_REGISTER_BYTE (FP0_REGNUM)));

      /* Wide registers come from the ss_wide area.
	 I think it's more PC to test (ss_flags & SS_WIDEREGS) to select
	 between ss_wide and ss_narrow than to use the raw register size.
	 But checking ss_flags would require an extra ptrace call for
	 every register reference.  Bleah.  */
      else if (len == 8)
	addr = (HPPAH_OFFSETOF (save_state_t, ss_wide) 
		+ DEPRECATED_REGISTER_BYTE (regno));

      /* Narrow registers come from the ss_narrow area.  Note that
	 ss_narrow starts with gr1, not gr0.  */
      else if (len == 4)
	addr = (HPPAH_OFFSETOF (save_state_t, ss_narrow)
		+ (DEPRECATED_REGISTER_BYTE (regno) - DEPRECATED_REGISTER_BYTE (1)));
      else
	internal_error (__FILE__, __LINE__,
			"hppah-nat.c (write_register): unexpected register size");

#ifdef GDB_TARGET_IS_HPPA_20W
      /* Unbelieveable.  The PC head and tail must be written in 64bit hunks
	 or we will get an error.  Worse yet, the oddball ptrace/ttrace
	 layering will not allow us to perform a 64bit register store.

	 What a crock.  */
      if (regno == PCOQ_HEAD_REGNUM || regno == PCOQ_TAIL_REGNUM && len == 8)
	{
	  CORE_ADDR temp;

	  temp = *(CORE_ADDR *)&deprecated_registers[DEPRECATED_REGISTER_BYTE (regno)];

	  /* Set the priv level (stored in the low two bits of the PC.  */
	  temp |= 0x3;

	  ttrace_write_reg_64 (PIDGET (inferior_ptid), (CORE_ADDR)addr,
	                       (CORE_ADDR)&temp);

	  /* If we fail to write the PC, give a true error instead of
	     just a warning.  */
	  if (errno != 0)
	    {
	      char *err = safe_strerror (errno);
	      char *msg = alloca (strlen (err) + 128);
	      sprintf (msg, "writing `%s' register: %s",
		        REGISTER_NAME (regno), err);
	      perror_with_name (msg);
	    }
	  return;
	}

      /* Another crock.  HPUX complains if you write a nonzero value to
	 the high part of IPSW.  What will it take for HP to catch a
	 clue about building sensible interfaces?  */
     if (regno == IPSW_REGNUM && len == 8)
	*(int *)&deprecated_registers[DEPRECATED_REGISTER_BYTE (regno)] = 0;
#endif

      for (i = 0; i < len; i += sizeof (int))
	{
	  errno = 0;
	  call_ptrace (PT_WUREGS, PIDGET (inferior_ptid),
	               (PTRACE_ARG3_TYPE) addr + i,
		       *(int *) &deprecated_registers[DEPRECATED_REGISTER_BYTE (regno) + i]);
	  if (errno != 0)
	    {
	      /* Warning, not error, in case we are attached; sometimes
		 the kernel doesn't let us at the registers. */
	      char *err = safe_strerror (errno);
	      char *msg = alloca (strlen (err) + 128);
	      sprintf (msg, "writing `%s' register: %s",
		        REGISTER_NAME (regno), err);
	      /* If we fail to write the PC, give a true error instead of
		 just a warning.  */
	      if (regno == PCOQ_HEAD_REGNUM || regno == PCOQ_TAIL_REGNUM)
		perror_with_name (msg);
	      else
		warning (msg);
	      return;
	    }
	}
    }
  else
    for (regno = 0; regno < NUM_REGS; regno++)
      store_inferior_registers (regno);
}


/* Fetch a register's value from the process's U area.  */
static void
fetch_register (int regno)
{
  char buf[MAX_REGISTER_SIZE];
  unsigned int addr, len, offset;
  int i;

  offset = 0;
  len = DEPRECATED_REGISTER_RAW_SIZE (regno);

  /* Requests for register zero actually want the save_state's
     ss_flags member.  As RM says: "Oh, what a hack!"  */
  if (regno == 0)
    {
      save_state_t ss;
      addr = HPPAH_OFFSETOF (save_state_t, ss_flags);
      len = sizeof (ss.ss_flags);

      /* Note that ss_flags is always an int, no matter what
	 DEPRECATED_REGISTER_RAW_SIZE(0) says.  Assuming all HP-UX PA
	 machines are big-endian, put it at the least significant end
	 of the value, and zap the rest of the buffer.  */
      offset = DEPRECATED_REGISTER_RAW_SIZE (0) - len;
      memset (buf, 0, sizeof (buf));
    }

  /* Floating-point registers come from the ss_fpblock area.  */
  else if (regno >= FP0_REGNUM)
    addr = (HPPAH_OFFSETOF (save_state_t, ss_fpblock) 
	    + (DEPRECATED_REGISTER_BYTE (regno) - DEPRECATED_REGISTER_BYTE (FP0_REGNUM)));

  /* Wide registers come from the ss_wide area.
     I think it's more PC to test (ss_flags & SS_WIDEREGS) to select
     between ss_wide and ss_narrow than to use the raw register size.
     But checking ss_flags would require an extra ptrace call for
     every register reference.  Bleah.  */
  else if (len == 8)
    addr = (HPPAH_OFFSETOF (save_state_t, ss_wide) 
	    + DEPRECATED_REGISTER_BYTE (regno));

  /* Narrow registers come from the ss_narrow area.  Note that
     ss_narrow starts with gr1, not gr0.  */
  else if (len == 4)
    addr = (HPPAH_OFFSETOF (save_state_t, ss_narrow)
	    + (DEPRECATED_REGISTER_BYTE (regno) - DEPRECATED_REGISTER_BYTE (1)));

  else
    internal_error (__FILE__, __LINE__,
		    "hppa-nat.c (fetch_register): unexpected register size");

  for (i = 0; i < len; i += sizeof (int))
    {
      errno = 0;
      /* Copy an int from the U area to buf.  Fill the least
         significant end if len != raw_size.  */
      * (int *) &buf[offset + i] =
	  call_ptrace (PT_RUREGS, PIDGET (inferior_ptid),
		       (PTRACE_ARG3_TYPE) addr + i, 0);
      if (errno != 0)
	{
	  /* Warning, not error, in case we are attached; sometimes
	     the kernel doesn't let us at the registers. */
	  char *err = safe_strerror (errno);
	  char *msg = alloca (strlen (err) + 128);
	  sprintf (msg, "reading `%s' register: %s",
		   REGISTER_NAME (regno), err);
	  warning (msg);
	  return;
	}
    }

  /* If we're reading an address from the instruction address queue,
     mask out the bottom two bits --- they contain the privilege
     level.  */
  if (regno == PCOQ_HEAD_REGNUM || regno == PCOQ_TAIL_REGNUM)
    buf[len - 1] &= ~0x3;

  supply_register (regno, buf);
}


/* Copy LEN bytes to or from inferior's memory starting at MEMADDR
   to debugger memory starting at MYADDR.   Copy to inferior if
   WRITE is nonzero.

   Returns the length copied, which is either the LEN argument or zero.
   This xfer function does not do partial moves, since child_ops
   doesn't allow memory operations to cross below us in the target stack
   anyway.  TARGET is ignored.  */

int
child_xfer_memory (CORE_ADDR memaddr, char *myaddr, int len, int write,
		   struct mem_attrib *mem,
		   struct target_ops *target)
{
  int i;
  /* Round starting address down to longword boundary.  */
  CORE_ADDR addr = memaddr & - (CORE_ADDR)(sizeof (int));
  /* Round ending address up; get number of longwords that makes.  */
  int count
  = (((memaddr + len) - addr) + sizeof (int) - 1) / sizeof (int);

  /* Allocate buffer of that many longwords.
     Note -- do not use alloca to allocate this buffer since there is no
     guarantee of when the buffer will actually be deallocated.

     This routine can be called over and over with the same call chain;
     this (in effect) would pile up all those alloca requests until a call
     to alloca was made from a point higher than this routine in the
     call chain.  */
  int *buffer = (int *) xmalloc (count * sizeof (int));

  if (write)
    {
      /* Fill start and end extra bytes of buffer with existing memory data.  */
      if (addr != memaddr || len < (int) sizeof (int))
	{
	  /* Need part of initial word -- fetch it.  */
	  buffer[0] = call_ptrace (addr < text_end ? PT_RIUSER : PT_RDUSER,
				   PIDGET (inferior_ptid),
				   (PTRACE_ARG3_TYPE) addr, 0);
	}

      if (count > 1)		/* FIXME, avoid if even boundary */
	{
	  buffer[count - 1]
	    = call_ptrace (addr < text_end ? PT_RIUSER : PT_RDUSER,
			   PIDGET (inferior_ptid),
			   (PTRACE_ARG3_TYPE) (addr
					       + (count - 1) * sizeof (int)),
			   0);
	}

      /* Copy data to be written over corresponding part of buffer */
      memcpy ((char *) buffer + (memaddr & (sizeof (int) - 1)), myaddr, len);

      /* Write the entire buffer.  */
      for (i = 0; i < count; i++, addr += sizeof (int))
	{
	  int pt_status;
	  int pt_request;
	  /* The HP-UX kernel crashes if you use PT_WDUSER to write into the
	     text segment.  FIXME -- does it work to write into the data
	     segment using WIUSER, or do these idiots really expect us to
	     figure out which segment the address is in, so we can use a
	     separate system call for it??!  */
	  errno = 0;
	  pt_request = (addr < text_end) ? PT_WIUSER : PT_WDUSER;
	  pt_status = call_ptrace (pt_request,
				   PIDGET (inferior_ptid),
				   (PTRACE_ARG3_TYPE) addr,
				   buffer[i]);

	  /* Did we fail?  Might we've guessed wrong about which
	     segment this address resides in?  Try the other request,
	     and see if that works...  */
	  if ((pt_status == -1) && errno)
	    {
	      errno = 0;
	      pt_request = (pt_request == PT_WIUSER) ? PT_WDUSER : PT_WIUSER;
	      pt_status = call_ptrace (pt_request,
				       PIDGET (inferior_ptid),
				       (PTRACE_ARG3_TYPE) addr,
				       buffer[i]);

	      /* No, we still fail.  Okay, time to punt. */
	      if ((pt_status == -1) && errno)
		{
		  xfree (buffer);
		  return 0;
		}
	    }
	}
    }
  else
    {
      /* Read all the longwords */
      for (i = 0; i < count; i++, addr += sizeof (int))
	{
	  errno = 0;
	  buffer[i] = call_ptrace (addr < text_end ? PT_RIUSER : PT_RDUSER,
				   PIDGET (inferior_ptid),
				   (PTRACE_ARG3_TYPE) addr, 0);
	  if (errno)
	    {
	      xfree (buffer);
	      return 0;
	    }
	  QUIT;
	}

      /* Copy appropriate bytes out of the buffer.  */
      memcpy (myaddr, (char *) buffer + (memaddr & (sizeof (int) - 1)), len);
    }
  xfree (buffer);
  return len;
}

char *saved_child_execd_pathname = NULL;
int saved_vfork_pid;
enum {
  STATE_NONE,
  STATE_GOT_CHILD,
  STATE_GOT_EXEC,
  STATE_GOT_PARENT,
  STATE_FAKE_EXEC
} saved_vfork_state = STATE_NONE;

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

  /* At this point, if we are vforking, breakpoints were already
     detached from the child in child_wait; and the child has already
     called execve().  If we are forking, both the parent and child
     have breakpoints inserted.  */

  if (! follow_child)
    {
      if (! has_vforked)
	{
	  detach_breakpoints (child_pid);
#ifdef SOLIB_REMOVE_INFERIOR_HOOK
	  SOLIB_REMOVE_INFERIOR_HOOK (child_pid);
#endif
	}

      /* Detach from the child. */
      printf_unfiltered ("Detaching after fork from %s\n",
			 target_pid_to_str (pid_to_ptid (child_pid)));
      hppa_require_detach (child_pid, 0);

      /* The parent and child of a vfork share the same address space.
	 Also, on some targets the order in which vfork and exec events
	 are received for parent in child requires some delicate handling
	 of the events.

	 For instance, on ptrace-based HPUX we receive the child's vfork
	 event first, at which time the parent has been suspended by the
	 OS and is essentially untouchable until the child's exit or second
	 exec event arrives.  At that time, the parent's vfork event is
	 delivered to us, and that's when we see and decide how to follow
	 the vfork.  But to get to that point, we must continue the child
	 until it execs or exits.  To do that smoothly, all breakpoints
	 must be removed from the child, in case there are any set between
	 the vfork() and exec() calls.  But removing them from the child
	 also removes them from the parent, due to the shared-address-space
	 nature of a vfork'd parent and child.  On HPUX, therefore, we must
	 take care to restore the bp's to the parent before we continue it.
	 Else, it's likely that we may not stop in the expected place.  (The
	 worst scenario is when the user tries to step over a vfork() call;
	 the step-resume bp must be restored for the step to properly stop
	 in the parent after the call completes!)

	 Sequence of events, as reported to gdb from HPUX:

	 Parent        Child           Action for gdb to take
	 -------------------------------------------------------
	 1                VFORK               Continue child
	 2                EXEC
	 3                EXEC or EXIT
	 4  VFORK

	 Now that the child has safely exec'd or exited, we must restore
	 the parent's breakpoints before we continue it.  Else, we may
	 cause it run past expected stopping points.  */

      if (has_vforked)
	reattach_breakpoints (parent_pid);
    }
  else
    {
      /* Needed to keep the breakpoint lists in sync.  */
      if (! has_vforked)
	detach_breakpoints (child_pid);

      /* Before detaching from the parent, remove all breakpoints from it. */
      remove_breakpoints ();

      /* Also reset the solib inferior hook from the parent. */
#ifdef SOLIB_REMOVE_INFERIOR_HOOK
      SOLIB_REMOVE_INFERIOR_HOOK (PIDGET (inferior_ptid));
#endif

      /* Detach from the parent. */
      target_detach (NULL, 1);

      /* Attach to the child. */
      printf_unfiltered ("Attaching after fork to %s\n",
			 target_pid_to_str (pid_to_ptid (child_pid)));
      hppa_require_attach (child_pid);
      inferior_ptid = pid_to_ptid (child_pid);

      /* If we vforked, then we've also execed by now.  The exec will be
	 reported momentarily.  follow_exec () will handle breakpoints, so
	 we don't have to..  */
      if (!has_vforked)
	follow_inferior_reset_breakpoints ();
    }

  if (has_vforked)
    {
      /* If we followed the parent, don't try to follow the child's exec.  */
      if (saved_vfork_state != STATE_GOT_PARENT
	  && saved_vfork_state != STATE_FAKE_EXEC)
	fprintf_unfiltered (gdb_stdout,
			    "hppa: post follow vfork: confused state\n");

      if (! follow_child || saved_vfork_state == STATE_GOT_PARENT)
	saved_vfork_state = STATE_NONE;
      else
	return 1;
    }
  return 0;
}

/* Format a process id, given PID.  Be sure to terminate
   this with a null--it's going to be printed via a "%s".  */
char *
child_pid_to_str (ptid_t ptid)
{
  /* Static because address returned */
  static char buf[30];
  pid_t pid = PIDGET (ptid);

  /* Extra NUL for paranoia's sake */
  sprintf (buf, "process %d%c", pid, '\0');

  return buf;
}

/* Format a thread id, given TID.  Be sure to terminate
   this with a null--it's going to be printed via a "%s".

   Note: This is a core-gdb tid, not the actual system tid.
   See infttrace.c for details.  */
char *
hppa_tid_to_str (ptid_t ptid)
{
  /* Static because address returned */
  static char buf[30];
  /* This seems strange, but when I did the ptid conversion, it looked
     as though a pid was always being passed.  - Kevin Buettner  */
  pid_t tid = PIDGET (ptid);

  /* Extra NULLs for paranoia's sake */
  sprintf (buf, "system thread %d%c", tid, '\0');

  return buf;
}

/*## */
/* Enable HACK for ttrace work.  In
 * infttrace.c/require_notification_of_events,
 * this is set to 0 so that the loop in child_wait
 * won't loop.
 */
int not_same_real_pid = 1;
/*## */

/* Wait for child to do something.  Return pid of child, or -1 in case
   of error; store status through argument pointer OURSTATUS.  */

ptid_t
child_wait (ptid_t ptid, struct target_waitstatus *ourstatus)
{
  int save_errno;
  int status;
  char *execd_pathname = NULL;
  int exit_status;
  int related_pid;
  int syscall_id;
  enum target_waitkind kind;
  int pid;

  if (saved_vfork_state == STATE_FAKE_EXEC)
    {
      saved_vfork_state = STATE_NONE;
      ourstatus->kind = TARGET_WAITKIND_EXECD;
      ourstatus->value.execd_pathname = saved_child_execd_pathname;
      return inferior_ptid;
    }

  do
    {
      set_sigint_trap ();	/* Causes SIGINT to be passed on to the
				   attached process. */
      set_sigio_trap ();

      pid = ptrace_wait (inferior_ptid, &status);

      save_errno = errno;

      clear_sigio_trap ();

      clear_sigint_trap ();

      if (pid == -1)
	{
	  if (save_errno == EINTR)
	    continue;

	  fprintf_unfiltered (gdb_stderr, "Child process unexpectedly missing: %s.\n",
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

      if (hpux_has_forked (pid, &related_pid))
	{
	  /* Ignore the parent's fork event.  */
	  if (pid == PIDGET (inferior_ptid))
	    {
	      ourstatus->kind = TARGET_WAITKIND_IGNORE;
	      return inferior_ptid;
	    }

	  /* If this is the child's fork event, report that the
	     process has forked.  */
	  if (related_pid == PIDGET (inferior_ptid))
	    {
	      ourstatus->kind = TARGET_WAITKIND_FORKED;
	      ourstatus->value.related_pid = pid;
	      return inferior_ptid;
	    }
	}

      if (hpux_has_vforked (pid, &related_pid))
	{
	  if (pid == PIDGET (inferior_ptid))
	    {
	      if (saved_vfork_state == STATE_GOT_CHILD)
		saved_vfork_state = STATE_GOT_PARENT;
	      else if (saved_vfork_state == STATE_GOT_EXEC)
		saved_vfork_state = STATE_FAKE_EXEC;
	      else
		fprintf_unfiltered (gdb_stdout,
				    "hppah: parent vfork: confused\n");
	    }
	  else if (related_pid == PIDGET (inferior_ptid))
	    {
	      if (saved_vfork_state == STATE_NONE)
		saved_vfork_state = STATE_GOT_CHILD;
	      else
		fprintf_unfiltered (gdb_stdout,
				    "hppah: child vfork: confused\n");
	    }
	  else
	    fprintf_unfiltered (gdb_stdout,
				"hppah: unknown vfork: confused\n");

	  if (saved_vfork_state == STATE_GOT_CHILD)
	    {
	      child_post_startup_inferior (pid_to_ptid (pid));
	      detach_breakpoints (pid);
#ifdef SOLIB_REMOVE_INFERIOR_HOOK
	      SOLIB_REMOVE_INFERIOR_HOOK (pid);
#endif
	      child_resume (pid_to_ptid (pid), 0, TARGET_SIGNAL_0);
	      ourstatus->kind = TARGET_WAITKIND_IGNORE;
	      return pid_to_ptid (related_pid);
	    }
	  else if (saved_vfork_state == STATE_FAKE_EXEC)
	    {
	      ourstatus->kind = TARGET_WAITKIND_VFORKED;
	      ourstatus->value.related_pid = related_pid;
	      return pid_to_ptid (pid);
	    }
	  else
	    {
	      /* We saw the parent's vfork, but we haven't seen the exec yet.
		 Wait for it, for simplicity's sake.  It should be pending.  */
	      saved_vfork_pid = related_pid;
	      ourstatus->kind = TARGET_WAITKIND_IGNORE;
	      return pid_to_ptid (pid);
	    }
	}

      if (hpux_has_execd (pid, &execd_pathname))
	{
	  /* On HP-UX, events associated with a vforking inferior come in
	     threes: a vfork event for the child (always first), followed
	     a vfork event for the parent and an exec event for the child.
	     The latter two can come in either order.  Make sure we get
	     both.  */
	  if (saved_vfork_state != STATE_NONE)
	    {
	      if (saved_vfork_state == STATE_GOT_CHILD)
		{
		  saved_vfork_state = STATE_GOT_EXEC;
		  /* On HP/UX with ptrace, the child must be resumed before
		     the parent vfork event is delivered.  A single-step
		     suffices.  */
		  if (RESUME_EXECD_VFORKING_CHILD_TO_GET_PARENT_VFORK ())
		    target_resume (pid_to_ptid (pid), 1, TARGET_SIGNAL_0);
		  ourstatus->kind = TARGET_WAITKIND_IGNORE;
		}
	      else if (saved_vfork_state == STATE_GOT_PARENT)
		{
		  saved_vfork_state = STATE_FAKE_EXEC;
		  ourstatus->kind = TARGET_WAITKIND_VFORKED;
		  ourstatus->value.related_pid = saved_vfork_pid;
		}
	      else
		fprintf_unfiltered (gdb_stdout,
				    "hppa: exec: unexpected state\n");

	      saved_child_execd_pathname = execd_pathname;

	      return inferior_ptid;
	    }
	  
	  /* Are we ignoring initial exec events?  (This is likely because
	     we're in the process of starting up the inferior, and another
	     (older) mechanism handles those.)  If so, we'll report this
	     as a regular stop, not an exec.
	   */
	  if (inferior_ignoring_startup_exec_events)
	    {
	      inferior_ignoring_startup_exec_events--;
	    }
	  else
	    {
	      ourstatus->kind = TARGET_WAITKIND_EXECD;
	      ourstatus->value.execd_pathname = execd_pathname;
	      return pid_to_ptid (pid);
	    }
	}

      /* All we must do with these is communicate their occurrence
         to wait_for_inferior...
       */
      if (hpux_has_syscall_event (pid, &kind, &syscall_id))
	{
	  ourstatus->kind = kind;
	  ourstatus->value.syscall_id = syscall_id;
	  return pid_to_ptid (pid);
	}

      /*##  } while (pid != PIDGET (inferior_ptid)); ## *//* Some other child died or stopped */
/* hack for thread testing */
    }
  while ((pid != PIDGET (inferior_ptid)) && not_same_real_pid);
/*## */

  store_waitstatus (ourstatus, status);
  return pid_to_ptid (pid);
}

#if !defined (GDB_NATIVE_HPUX_11)

/* The following code is a substitute for the infttrace.c versions used
   with ttrace() in HPUX 11.  */

/* This value is an arbitrary integer. */
#define PT_VERSION 123456

/* This semaphore is used to coordinate the child and parent processes
   after a fork(), and before an exec() by the child.  See
   parent_attach_all for details.  */

typedef struct
{
  int parent_channel[2];	/* Parent "talks" to [1], child "listens" to [0] */
  int child_channel[2];		/* Child "talks" to [1], parent "listens" to [0] */
}
startup_semaphore_t;

#define SEM_TALK (1)
#define SEM_LISTEN (0)

static startup_semaphore_t startup_semaphore;

#ifdef PT_SETTRC
/* This function causes the caller's process to be traced by its
   parent.  This is intended to be called after GDB forks itself,
   and before the child execs the target.

   Note that HP-UX ptrace is rather funky in how this is done.
   If the parent wants to get the initial exec event of a child,
   it must set the ptrace event mask of the child to include execs.
   (The child cannot do this itself.)  This must be done after the
   child is forked, but before it execs.

   To coordinate the parent and child, we implement a semaphore using
   pipes.  After SETTRC'ing itself, the child tells the parent that
   it is now traceable by the parent, and waits for the parent's
   acknowledgement.  The parent can then set the child's event mask,
   and notify the child that it can now exec.

   (The acknowledgement by parent happens as a result of a call to
   child_acknowledge_created_inferior.)  */

int
parent_attach_all (int pid, PTRACE_ARG3_TYPE addr, int data)
{
  int pt_status = 0;

  /* We need a memory home for a constant.  */
  int tc_magic_child = PT_VERSION;
  int tc_magic_parent = 0;

  /* The remainder of this function is only useful for HPUX 10.0 and
     later, as it depends upon the ability to request notification
     of specific kinds of events by the kernel.  */
#if defined(PT_SET_EVENT_MASK)

  /* Notify the parent that we're potentially ready to exec(). */
  write (startup_semaphore.child_channel[SEM_TALK],
	 &tc_magic_child,
	 sizeof (tc_magic_child));

  /* Wait for acknowledgement from the parent. */
  read (startup_semaphore.parent_channel[SEM_LISTEN],
	&tc_magic_parent,
	sizeof (tc_magic_parent));
  if (tc_magic_child != tc_magic_parent)
    warning ("mismatched semaphore magic");

  /* Discard our copy of the semaphore. */
  (void) close (startup_semaphore.parent_channel[SEM_LISTEN]);
  (void) close (startup_semaphore.parent_channel[SEM_TALK]);
  (void) close (startup_semaphore.child_channel[SEM_LISTEN]);
  (void) close (startup_semaphore.child_channel[SEM_TALK]);
#endif

  return 0;
}
#endif

int
hppa_require_attach (int pid)
{
  int pt_status;
  CORE_ADDR pc;
  CORE_ADDR pc_addr;
  unsigned int regs_offset;

  /* Are we already attached?  There appears to be no explicit way to
     answer this via ptrace, so we try something which should be
     innocuous if we are attached.  If that fails, then we assume
     we're not attached, and so attempt to make it so. */

  errno = 0;
  regs_offset = U_REGS_OFFSET;
  pc_addr = register_addr (PC_REGNUM, regs_offset);
  pc = call_ptrace (PT_READ_U, pid, (PTRACE_ARG3_TYPE) pc_addr, 0);

  if (errno)
    {
      errno = 0;
      pt_status = call_ptrace (PT_ATTACH, pid, (PTRACE_ARG3_TYPE) 0, 0);

      if (errno)
	return -1;

      /* Now we really are attached. */
      errno = 0;
    }
  attach_flag = 1;
  return pid;
}

int
hppa_require_detach (int pid, int signal)
{
  errno = 0;
  call_ptrace (PT_DETACH, pid, (PTRACE_ARG3_TYPE) 1, signal);
  errno = 0;			/* Ignore any errors. */
  return pid;
}

/* Since ptrace doesn't support memory page-protection events, which
   are used to implement "hardware" watchpoints on HP-UX, these are
   dummy versions, which perform no useful work.  */

void
hppa_enable_page_protection_events (int pid)
{
}

void
hppa_disable_page_protection_events (int pid)
{
}

int
hppa_insert_hw_watchpoint (int pid, CORE_ADDR start, LONGEST len, int type)
{
  error ("Hardware watchpoints not implemented on this platform.");
}

int
hppa_remove_hw_watchpoint (int pid, CORE_ADDR start, LONGEST len, int type)
{
  error ("Hardware watchpoints not implemented on this platform.");
}

int
hppa_can_use_hw_watchpoint (int type, int cnt, int ot)
{
  return 0;
}

int
hppa_range_profitable_for_hw_watchpoint (int pid, CORE_ADDR start, LONGEST len)
{
  error ("Hardware watchpoints not implemented on this platform.");
}

char *
hppa_pid_or_tid_to_str (ptid_t id)
{
  /* In the ptrace world, there are only processes. */
  return child_pid_to_str (id);
}

void
hppa_ensure_vforking_parent_remains_stopped (int pid)
{
  /* This assumes that the vforked parent is presently stopped, and
     that the vforked child has just delivered its first exec event.
     Calling kill() this way will cause the SIGTRAP to be delivered as
     soon as the parent is resumed, which happens as soon as the
     vforked child is resumed.  See wait_for_inferior for the use of
     this function.  */
  kill (pid, SIGTRAP);
}

int
hppa_resume_execd_vforking_child_to_get_parent_vfork (void)
{
  return 1;			/* Yes, the child must be resumed. */
}

void
require_notification_of_events (int pid)
{
#if defined(PT_SET_EVENT_MASK)
  int pt_status;
  ptrace_event_t ptrace_events;
  int nsigs;
  int signum;

  /* Instruct the kernel as to the set of events we wish to be
     informed of.  (This support does not exist before HPUX 10.0.
     We'll assume if PT_SET_EVENT_MASK has not been defined by
     <sys/ptrace.h>, then we're being built on pre-10.0.)  */
  memset (&ptrace_events, 0, sizeof (ptrace_events));

  /* Note: By default, all signals are visible to us.  If we wish
     the kernel to keep certain signals hidden from us, we do it
     by calling sigdelset (ptrace_events.pe_signals, signal) for
     each such signal here, before doing PT_SET_EVENT_MASK.  */
  /* RM: The above comment is no longer true. We start with ignoring
     all signals, and then add the ones we are interested in. We could
     do it the other way: start by looking at all signals and then
     deleting the ones that we aren't interested in, except that
     multiple gdb signals may be mapped to the same host signal
     (eg. TARGET_SIGNAL_IO and TARGET_SIGNAL_POLL both get mapped to
     signal 22 on HPUX 10.20) We want to be notified if we are
     interested in either signal.  */
  sigfillset (&ptrace_events.pe_signals);

  /* RM: Let's not bother with signals we don't care about */
  nsigs = (int) TARGET_SIGNAL_LAST;
  for (signum = nsigs; signum > 0; signum--)
    {
      if ((signal_stop_state (signum)) ||
	  (signal_print_state (signum)) ||
	  (!signal_pass_state (signum)))
	{
	  if (target_signal_to_host_p (signum))
	    sigdelset (&ptrace_events.pe_signals,
		       target_signal_to_host (signum));
	}
    }

  ptrace_events.pe_set_event = 0;

  ptrace_events.pe_set_event |= PTRACE_SIGNAL;
  ptrace_events.pe_set_event |= PTRACE_EXEC;
  ptrace_events.pe_set_event |= PTRACE_FORK;
  ptrace_events.pe_set_event |= PTRACE_VFORK;
  /* ??rehrauer: Add this one when we're prepared to catch it...
     ptrace_events.pe_set_event |= PTRACE_EXIT;
   */

  errno = 0;
  pt_status = call_ptrace (PT_SET_EVENT_MASK,
			   pid,
			   (PTRACE_ARG3_TYPE) & ptrace_events,
			   sizeof (ptrace_events));
  if (errno)
    perror_with_name ("ptrace");
  if (pt_status < 0)
    return;
#endif
}

void
require_notification_of_exec_events (int pid)
{
#if defined(PT_SET_EVENT_MASK)
  int pt_status;
  ptrace_event_t ptrace_events;

  /* Instruct the kernel as to the set of events we wish to be
     informed of.  (This support does not exist before HPUX 10.0.
     We'll assume if PT_SET_EVENT_MASK has not been defined by
     <sys/ptrace.h>, then we're being built on pre-10.0.)  */
  memset (&ptrace_events, 0, sizeof (ptrace_events));

  /* Note: By default, all signals are visible to us.  If we wish
     the kernel to keep certain signals hidden from us, we do it
     by calling sigdelset (ptrace_events.pe_signals, signal) for
     each such signal here, before doing PT_SET_EVENT_MASK.  */
  sigemptyset (&ptrace_events.pe_signals);

  ptrace_events.pe_set_event = 0;

  ptrace_events.pe_set_event |= PTRACE_EXEC;
  /* ??rehrauer: Add this one when we're prepared to catch it...
     ptrace_events.pe_set_event |= PTRACE_EXIT;
   */

  errno = 0;
  pt_status = call_ptrace (PT_SET_EVENT_MASK,
			   pid,
			   (PTRACE_ARG3_TYPE) & ptrace_events,
			   sizeof (ptrace_events));
  if (errno)
    perror_with_name ("ptrace");
  if (pt_status < 0)
    return;
#endif
}

/* This function is called by the parent process, with pid being the
   ID of the child process, after the debugger has forked.  */

void
child_acknowledge_created_inferior (int pid)
{
  /* We need a memory home for a constant.  */
  int tc_magic_parent = PT_VERSION;
  int tc_magic_child = 0;

  /* The remainder of this function is only useful for HPUX 10.0 and
     later, as it depends upon the ability to request notification
     of specific kinds of events by the kernel.  */
#if defined(PT_SET_EVENT_MASK)
  /* Wait for the child to tell us that it has forked. */
  read (startup_semaphore.child_channel[SEM_LISTEN],
	&tc_magic_child,
	sizeof (tc_magic_child));

  /* Notify the child that it can exec.

     In the infttrace.c variant of this function, we set the child's
     event mask after the fork but before the exec.  In the ptrace
     world, it seems we can't set the event mask until after the exec.  */
  write (startup_semaphore.parent_channel[SEM_TALK],
	 &tc_magic_parent,
	 sizeof (tc_magic_parent));

  /* We'd better pause a bit before trying to set the event mask,
     though, to ensure that the exec has happened.  We don't want to
     wait() on the child, because that'll screw up the upper layers
     of gdb's execution control that expect to see the exec event.

     After an exec, the child is no longer executing gdb code.  Hence,
     we can't have yet another synchronization via the pipes.  We'll
     just sleep for a second, and hope that's enough delay...  */
  sleep (1);

  /* Instruct the kernel as to the set of events we wish to be
     informed of.  */
  require_notification_of_exec_events (pid);

  /* Discard our copy of the semaphore. */
  (void) close (startup_semaphore.parent_channel[SEM_LISTEN]);
  (void) close (startup_semaphore.parent_channel[SEM_TALK]);
  (void) close (startup_semaphore.child_channel[SEM_LISTEN]);
  (void) close (startup_semaphore.child_channel[SEM_TALK]);
#endif
}

void
child_post_startup_inferior (ptid_t ptid)
{
  require_notification_of_events (PIDGET (ptid));
}

void
child_post_attach (int pid)
{
  require_notification_of_events (pid);
}

int
child_insert_fork_catchpoint (int pid)
{
  /* This request is only available on HPUX 10.0 and later.  */
#if !defined(PT_SET_EVENT_MASK)
  error ("Unable to catch forks prior to HPUX 10.0");
#else
  /* Enable reporting of fork events from the kernel. */
  /* ??rehrauer: For the moment, we're always enabling these events,
     and just ignoring them if there's no catchpoint to catch them.  */
  return 0;
#endif
}

int
child_remove_fork_catchpoint (int pid)
{
  /* This request is only available on HPUX 10.0 and later.  */
#if !defined(PT_SET_EVENT_MASK)
  error ("Unable to catch forks prior to HPUX 10.0");
#else
  /* Disable reporting of fork events from the kernel. */
  /* ??rehrauer: For the moment, we're always enabling these events,
     and just ignoring them if there's no catchpoint to catch them.  */
  return 0;
#endif
}

int
child_insert_vfork_catchpoint (int pid)
{
  /* This request is only available on HPUX 10.0 and later.  */
#if !defined(PT_SET_EVENT_MASK)
  error ("Unable to catch vforks prior to HPUX 10.0");
#else
  /* Enable reporting of vfork events from the kernel. */
  /* ??rehrauer: For the moment, we're always enabling these events,
     and just ignoring them if there's no catchpoint to catch them.  */
  return 0;
#endif
}

int
child_remove_vfork_catchpoint (int pid)
{
  /* This request is only available on HPUX 10.0 and later.  */
#if !defined(PT_SET_EVENT_MASK)
  error ("Unable to catch vforks prior to HPUX 10.0");
#else
  /* Disable reporting of vfork events from the kernel. */
  /* ??rehrauer: For the moment, we're always enabling these events,
     and just ignoring them if there's no catchpoint to catch them.  */
  return 0;
#endif
}

int
hpux_has_forked (int pid, int *childpid)
{
  /* This request is only available on HPUX 10.0 and later.  */
#if !defined(PT_GET_PROCESS_STATE)
  *childpid = 0;
  return 0;
#else
  int pt_status;
  ptrace_state_t ptrace_state;

  errno = 0;
  pt_status = call_ptrace (PT_GET_PROCESS_STATE,
			   pid,
			   (PTRACE_ARG3_TYPE) & ptrace_state,
			   sizeof (ptrace_state));
  if (errno)
    perror_with_name ("ptrace");
  if (pt_status < 0)
    return 0;

  if (ptrace_state.pe_report_event & PTRACE_FORK)
    {
      *childpid = ptrace_state.pe_other_pid;
      return 1;
    }

  return 0;
#endif
}

int
hpux_has_vforked (int pid, int *childpid)
{
  /* This request is only available on HPUX 10.0 and later.  */
#if !defined(PT_GET_PROCESS_STATE)
  *childpid = 0;
  return 0;

#else
  int pt_status;
  ptrace_state_t ptrace_state;

  errno = 0;
  pt_status = call_ptrace (PT_GET_PROCESS_STATE,
			   pid,
			   (PTRACE_ARG3_TYPE) & ptrace_state,
			   sizeof (ptrace_state));
  if (errno)
    perror_with_name ("ptrace");
  if (pt_status < 0)
    return 0;

  if (ptrace_state.pe_report_event & PTRACE_VFORK)
    {
      *childpid = ptrace_state.pe_other_pid;
      return 1;
    }

  return 0;
#endif
}

int
child_insert_exec_catchpoint (int pid)
{
  /* This request is only available on HPUX 10.0 and later.   */
#if !defined(PT_SET_EVENT_MASK)
  error ("Unable to catch execs prior to HPUX 10.0");

#else
  /* Enable reporting of exec events from the kernel.  */
  /* ??rehrauer: For the moment, we're always enabling these events,
     and just ignoring them if there's no catchpoint to catch them.  */
  return 0;
#endif
}

int
child_remove_exec_catchpoint (int pid)
{
  /* This request is only available on HPUX 10.0 and later.  */
#if !defined(PT_SET_EVENT_MASK)
  error ("Unable to catch execs prior to HPUX 10.0");

#else
  /* Disable reporting of exec events from the kernel. */
  /* ??rehrauer: For the moment, we're always enabling these events,
     and just ignoring them if there's no catchpoint to catch them.  */
  return 0;
#endif
}

int
hpux_has_execd (int pid, char **execd_pathname)
{
  /* This request is only available on HPUX 10.0 and later.  */
#if !defined(PT_GET_PROCESS_STATE)
  *execd_pathname = NULL;
  return 0;

#else
  int pt_status;
  ptrace_state_t ptrace_state;

  errno = 0;
  pt_status = call_ptrace (PT_GET_PROCESS_STATE,
			   pid,
			   (PTRACE_ARG3_TYPE) & ptrace_state,
			   sizeof (ptrace_state));
  if (errno)
    perror_with_name ("ptrace");
  if (pt_status < 0)
    return 0;

  if (ptrace_state.pe_report_event & PTRACE_EXEC)
    {
      char *exec_file = target_pid_to_exec_file (pid);
      *execd_pathname = savestring (exec_file, strlen (exec_file));
      return 1;
    }

  return 0;
#endif
}

int
child_reported_exec_events_per_exec_call (void)
{
  return 2;			/* ptrace reports the event twice per call. */
}

int
hpux_has_syscall_event (int pid, enum target_waitkind *kind, int *syscall_id)
{
  /* This request is only available on HPUX 10.30 and later, via
     the ttrace interface.  */

  *kind = TARGET_WAITKIND_SPURIOUS;
  *syscall_id = -1;
  return 0;
}

char *
child_pid_to_exec_file (int pid)
{
  static char exec_file_buffer[1024];
  int pt_status;
  CORE_ADDR top_of_stack;
  char four_chars[4];
  int name_index;
  int i;
  ptid_t saved_inferior_ptid;
  int done;

#ifdef PT_GET_PROCESS_PATHNAME
  /* As of 10.x HP-UX, there's an explicit request to get the pathname. */
  pt_status = call_ptrace (PT_GET_PROCESS_PATHNAME,
			   pid,
			   (PTRACE_ARG3_TYPE) exec_file_buffer,
			   sizeof (exec_file_buffer) - 1);
  if (pt_status == 0)
    return exec_file_buffer;
#endif

  /* It appears that this request is broken prior to 10.30.
     If it fails, try a really, truly amazingly gross hack
     that DDE uses, of pawing through the process' data
     segment to find the pathname.  */

  top_of_stack = 0x7b03a000;
  name_index = 0;
  done = 0;

  /* On the chance that pid != inferior_ptid, set inferior_ptid
     to pid, so that (grrrr!) implicit uses of inferior_ptid get
     the right id.  */

  saved_inferior_ptid = inferior_ptid;
  inferior_ptid = pid_to_ptid (pid);

  /* Try to grab a null-terminated string. */
  while (!done)
    {
      if (target_read_memory (top_of_stack, four_chars, 4) != 0)
	{
	  inferior_ptid = saved_inferior_ptid;
	  return NULL;
	}
      for (i = 0; i < 4; i++)
	{
	  exec_file_buffer[name_index++] = four_chars[i];
	  done = (four_chars[i] == '\0');
	  if (done)
	    break;
	}
      top_of_stack += 4;
    }

  if (exec_file_buffer[0] == '\0')
    {
      inferior_ptid = saved_inferior_ptid;
      return NULL;
    }

  inferior_ptid = saved_inferior_ptid;
  return exec_file_buffer;
}

void
pre_fork_inferior (void)
{
  int status;

  status = pipe (startup_semaphore.parent_channel);
  if (status < 0)
    {
      warning ("error getting parent pipe for startup semaphore");
      return;
    }

  status = pipe (startup_semaphore.child_channel);
  if (status < 0)
    {
      warning ("error getting child pipe for startup semaphore");
      return;
    }
}


/* Check to see if the given thread is alive.

   This is a no-op, as ptrace doesn't support threads, so we just
   return "TRUE".  */

int
child_thread_alive (ptid_t ptid)
{
  return 1;
}

#endif /* ! GDB_NATIVE_HPUX_11 */

/* Native-dependent code for AMD64 BSD's.

   Copyright 2003, 2004 Free Software Foundation, Inc.

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
#include "regcache.h"
#include "target.h"

/* We include <signal.h> to make sure `struct fxsave64' is defined on
   NetBSD, since NetBSD's <machine/reg.h> needs it.  */
#include "gdb_assert.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>

#include "amd64-tdep.h"
#include "amd64-nat.h"
#include "inf-ptrace.h"


/* Fetch register REGNUM from the inferior.  If REGNUM is -1, do this
   for all registers (including the floating-point registers).  */

static void
amd64bsd_fetch_inferior_registers (int regnum)
{
  int pid;

  /* Cater for systems like OpenBSD, that implement threads as
     separate processes.  */
  pid = ptid_get_lwp (inferior_ptid);
  if (pid == 0)
    pid = ptid_get_pid (inferior_ptid);

  if (regnum == -1 || amd64_native_gregset_supplies_p (regnum))
    {
      struct reg regs;

      if (ptrace (PT_GETREGS, pid, (PTRACE_TYPE_ARG3) &regs, 0) == -1)
	perror_with_name ("Couldn't get registers");

      amd64_supply_native_gregset (current_regcache, &regs, -1);
      if (regnum != -1)
	return;
    }

  if (regnum == -1 || !amd64_native_gregset_supplies_p (regnum))
    {
      struct fpreg fpregs;

      if (ptrace (PT_GETFPREGS, pid, (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name ("Couldn't get floating point status");

      amd64_supply_fxsave (current_regcache, -1, &fpregs);
    }
}

/* Store register REGNUM back into the inferior.  If REGNUM is -1, do
   this for all registers (including the floating-point registers).  */

static void
amd64bsd_store_inferior_registers (int regnum)
{
  int pid;

  /* Cater for systems like OpenBSD, that implement threads as
     separate processes.  */
  pid = ptid_get_lwp (inferior_ptid);
  if (pid == 0)
    pid = ptid_get_pid (inferior_ptid);

  if (regnum == -1 || amd64_native_gregset_supplies_p (regnum))
    {
      struct reg regs;

      if (ptrace (PT_GETREGS, pid, (PTRACE_TYPE_ARG3) &regs, 0) == -1)
        perror_with_name ("Couldn't get registers");

      amd64_collect_native_gregset (current_regcache, &regs, regnum);

      if (ptrace (PT_SETREGS, pid, (PTRACE_TYPE_ARG3) &regs, 0) == -1)
        perror_with_name ("Couldn't write registers");

      if (regnum != -1)
	return;
    }

  if (regnum == -1 || !amd64_native_gregset_supplies_p (regnum))
    {
      struct fpreg fpregs;

      if (ptrace (PT_GETFPREGS, pid, (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name ("Couldn't get floating point status");

      amd64_collect_fxsave (current_regcache, regnum, &fpregs);

      if (ptrace (PT_SETFPREGS, pid, (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name ("Couldn't write floating point status");
    }
}

/* Create a prototype *BSD/amd64 target.  The client can override it
   with local methods.  */

struct target_ops *
amd64bsd_target (void)
{
  struct target_ops *t;

  t = inf_ptrace_target ();
  t->to_fetch_registers = amd64bsd_fetch_inferior_registers;
  t->to_store_registers = amd64bsd_store_inferior_registers;
  return t;
}

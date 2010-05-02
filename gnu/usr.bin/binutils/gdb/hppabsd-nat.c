/* Native-dependent code for HP PA-RISC BSD's.

   Copyright 2004, 2005 Free Software Foundation, Inc.

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

#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>

#include "hppa-tdep.h"
#include "inf-ptrace.h"

static int
hppabsd_gregset_supplies_p (int regnum)
{
  return (regnum >= HPPA_R0_REGNUM && regnum <= HPPA_PCOQ_TAIL_REGNUM);
}

static int
hppabsd_fpregset_supplies_p (int regnum)
{
  return (regnum >= HPPA_FP0_REGNUM && regnum < HPPA_FP0_REGNUM + 32 * 2);
}

/* Supply the general-purpose registers stored in GREGS to REGCACHE.  */

static void
hppabsd_supply_gregset (struct regcache *regcache, const void *gregs)
{
  const char *regs = gregs;
  int regnum;

  for (regnum = HPPA_R1_REGNUM; regnum <= HPPA_R31_REGNUM; regnum++)
    regcache_raw_supply (regcache, regnum, regs + regnum * 4);

  regcache_raw_supply (regcache, HPPA_SAR_REGNUM, regs);
  regcache_raw_supply (regcache, HPPA_PCOQ_HEAD_REGNUM, regs + 32 * 4);
  regcache_raw_supply (regcache, HPPA_PCOQ_TAIL_REGNUM, regs + 33 * 4);
}

/* Supply the floating-point registers stored in FPREGS to REGCACHE.  */

static void
hppabsd_supply_fpregset (struct regcache *regcache, const void *fpregs)
{
  const char *regs = fpregs;
  int regnum;

  for (regnum = HPPA_FP0_REGNUM; regnum < HPPA_FP0_REGNUM + 32 * 2; regnum++)
    regcache_raw_supply (regcache, regnum,
			 regs + (regnum - HPPA_FP0_REGNUM) * 4);
}

/* Collect the general-purpose registers from REGCACHE and store them
   in GREGS.  */

static void
hppabsd_collect_gregset (const struct regcache *regcache,
			  void *gregs, int regnum)
{
  char *regs = gregs;
  int i;

  for (i = HPPA_R1_REGNUM; i <= HPPA_R31_REGNUM; i++)
    {
      if (regnum == -1 || regnum == i)
	regcache_raw_collect (regcache, i, regs + i * 4);
    }

  if (regnum == -1 || regnum == HPPA_SAR_REGNUM)
    regcache_raw_collect (regcache, HPPA_SAR_REGNUM, regs);
  if (regnum == -1 || regnum == HPPA_PCOQ_HEAD_REGNUM)
    regcache_raw_collect (regcache, HPPA_PCOQ_HEAD_REGNUM, regs + 32 * 4);
  if (regnum == -1 || regnum == HPPA_PCOQ_TAIL_REGNUM)
    regcache_raw_collect (regcache, HPPA_PCOQ_TAIL_REGNUM, regs + 33 * 4);
}

/* Collect the floating-point registers from REGCACHE and store them
   in FPREGS.  */

static void
hppabsd_collect_fpregset (struct regcache *regcache,
			  void *fpregs, int regnum)
{
  char *regs = fpregs;
  int i;

  for (i = HPPA_FP0_REGNUM; i < HPPA_FP0_REGNUM + 32 * 2; i++)
    {
      if (regnum == -1 || regnum == i)
	regcache_raw_collect (regcache, i, regs + (i - HPPA_FP0_REGNUM) * 4);
    }
}


/* Fetch register REGNUM from the inferior.  If REGNUM is -1, do this
   for all registers (including the floating-point registers).  */

static void
hppabsd_fetch_registers (int regnum)
{
  struct regcache *regcache = current_regcache;

  if (regnum == -1 || hppabsd_gregset_supplies_p (regnum))
    {
      struct reg regs;

      if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &regs, 0) == -1)
	perror_with_name (_("Couldn't get registers"));

      hppabsd_supply_gregset (regcache, &regs);
    }

  if (regnum == -1 || hppabsd_fpregset_supplies_p (regnum))
    {
      struct fpreg fpregs;

      if (ptrace (PT_GETFPREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name (_("Couldn't get floating point status"));

      hppabsd_supply_fpregset (current_regcache, &fpregs);
    }
}

/* Store register REGNUM back into the inferior.  If REGNUM is -1, do
   this for all registers (including the floating-point registers).  */

static void
hppabsd_store_registers (int regnum)
{
  if (regnum == -1 || hppabsd_gregset_supplies_p (regnum))
    {
      struct reg regs;

      if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
                  (PTRACE_TYPE_ARG3) &regs, 0) == -1)
        perror_with_name (_("Couldn't get registers"));

      hppabsd_collect_gregset (current_regcache, &regs, regnum);

      if (ptrace (PT_SETREGS, PIDGET (inferior_ptid),
	          (PTRACE_TYPE_ARG3) &regs, 0) == -1)
        perror_with_name (_("Couldn't write registers"));
    }

  if (regnum == -1 || hppabsd_fpregset_supplies_p (regnum))
    {
      struct fpreg fpregs;

      if (ptrace (PT_GETFPREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name (_("Couldn't get floating point status"));

      hppabsd_collect_fpregset (current_regcache, &fpregs, regnum);

      if (ptrace (PT_SETFPREGS, PIDGET (inferior_ptid),
		  (PTRACE_TYPE_ARG3) &fpregs, 0) == -1)
	perror_with_name (_("Couldn't write floating point status"));
    }
}

/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_hppabsd_nat (void);

void
_initialize_hppabsd_nat (void)
{
  struct target_ops *t;

  /* Add in local overrides.  */
  t = inf_ptrace_target ();
  t->to_fetch_registers = hppabsd_fetch_registers;
  t->to_store_registers = hppabsd_store_registers;
  add_target (t);
}

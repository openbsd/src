/* Native-dependent code for HP PA-RISC BSD's.

   Copyright 2004 Free Software Foundation, Inc.

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

#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>

#include "hppa-tdep.h"

static int
hppabsd_gregset_supplies_p (int regnum)
{
  return (regnum >= HPPA_R0_REGNUM && regnum <= HPPA_PCOQ_TAIL_REGNUM);
}

static int
hppabsd_fpregset_supplies_p (int regnum)
{
  return 0;
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
	regcache_raw_collect (regcache, regnum, regs + i * 4);
    }

  if (regnum == -1 || regnum == HPPA_SAR_REGNUM)
    regcache_raw_collect (regcache, HPPA_SAR_REGNUM, regs);
  if (regnum == -1 || regnum == HPPA_PCOQ_HEAD_REGNUM)
    regcache_raw_collect (regcache, HPPA_PCOQ_HEAD_REGNUM, regs + 32 * 4);
  if (regnum == -1 || regnum == HPPA_PCOQ_TAIL_REGNUM)
    regcache_raw_collect (regcache, HPPA_PCOQ_TAIL_REGNUM, regs + 33 * 4);
}


/* Fetch register REGNUM from the inferior.  If REGNUM is -1, do this
   for all registers (including the floating-point registers).  */

void
fetch_inferior_registers (int regnum)
{
  struct regcache *regcache = current_regcache;

  if (regnum == -1 || hppabsd_gregset_supplies_p (regnum))
    {
      struct reg regs;

      if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
		  (PTRACE_ARG3_TYPE) &regs, 0) == -1)
	perror_with_name ("Couldn't get registers");

      hppabsd_supply_gregset (regcache, &regs);
    }
}

/* Store register REGNUM back into the inferior.  If REGNUM is -1, do
   this for all registers (including the floating-point registers).  */

void
store_inferior_registers (int regnum)
{
  if (regnum == -1 || hppabsd_gregset_supplies_p (regnum))
    {
      struct reg regs;

      if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
                  (PTRACE_ARG3_TYPE) &regs, 0) == -1)
        perror_with_name ("Couldn't get registers");

      hppabsd_collect_gregset (current_regcache, &regs, regnum);

      if (ptrace (PT_SETREGS, PIDGET (inferior_ptid),
	          (PTRACE_ARG3_TYPE) &regs, 0) == -1)
        perror_with_name ("Couldn't write registers");
    }
}

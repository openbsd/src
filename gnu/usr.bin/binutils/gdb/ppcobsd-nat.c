/* Native-dependent code for OpenBSD/powerpc.

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

#include "nm-bsd.h"

#include "defs.h"
#include "inferior.h"
#include "regcache.h"

#include <stddef.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>

#include "ppcobsd-tdep.h"


/* Fetch register REGNUM from the inferior.  If REGNUM is -1, do this
   for all registers.  */

void
fetch_inferior_registers (int regnum)
{
  struct reg regs;

  if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
	      (PTRACE_ARG3_TYPE) &regs, 0) == -1)
    perror_with_name ("Couldn't get registers");

  ppcobsd_supply_gregset (&ppcobsd_gregset, current_regcache, -1,
			  &regs, sizeof regs);
}

/* Store register REGNUM back into the inferior.  If REGNUM is -1, do
   this for all registers.  */

void
store_inferior_registers (int regnum)
{
  struct reg regs;

  if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
	      (PTRACE_ARG3_TYPE) &regs, 0) == -1)
    perror_with_name ("Couldn't get registers");

  ppcobsd_collect_gregset (&ppcobsd_gregset, current_regcache,
			   regnum, &regs, sizeof regs);

  if (ptrace (PT_SETREGS, PIDGET (inferior_ptid),
	      (PTRACE_ARG3_TYPE) &regs, 0) == -1)
    perror_with_name ("Couldn't write registers");
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_ppcobsd_nat (void);

void
_initialize_ppcobsd_nat (void)
{
  /* General-purpose registers.  */
  ppcobsd_reg_offsets.r0_offset = offsetof (struct reg, gpr);
  ppcobsd_reg_offsets.pc_offset = offsetof (struct reg, pc);
  ppcobsd_reg_offsets.ps_offset = offsetof (struct reg, ps);
  ppcobsd_reg_offsets.cr_offset = offsetof (struct reg, cnd);
  ppcobsd_reg_offsets.lr_offset = offsetof (struct reg, lr);
  ppcobsd_reg_offsets.ctr_offset = offsetof (struct reg, cnt);
  ppcobsd_reg_offsets.xer_offset = offsetof (struct reg, xer);
  ppcobsd_reg_offsets.mq_offset = offsetof (struct reg, mq);

  /* Floating-point registers.  */
  ppcobsd_reg_offsets.f0_offset = offsetof (struct reg, fpr);
  ppcobsd_reg_offsets.fpscr_offset = -1;

  /* AltiVec registers.  */
  ppcobsd_reg_offsets.vr0_offset = offsetof (struct vreg, vreg);
  ppcobsd_reg_offsets.vscr_offset = offsetof (struct vreg, vscr);
  ppcobsd_reg_offsets.vrsave_offset = offsetof (struct vreg, vrsave);
}

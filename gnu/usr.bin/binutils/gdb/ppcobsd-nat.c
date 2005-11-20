/* Native-dependent code for OpenBSD/powerpc.

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
#include "gdbcore.h"
#include "inferior.h"
#include "regcache.h"

#include <stddef.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/signal.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/reg.h>

#include "ppc-tdep.h"
#include "ppcobsd-tdep.h"
#include "inf-ptrace.h"
#include "bsd-kvm.h"

/* OpenBSD/powerpc doesn't have PT_GETFPREGS/PT_SETFPREGS like
   NetBSD/powerpc and FreeBSD/powerpc.  */

/* Fetch register REGNUM from the inferior.  If REGNUM is -1, do this
   for all registers.  */

static void
ppcobsd_fetch_registers (int regnum)
{
  struct reg regs;

  if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
	      (PTRACE_TYPE_ARG3) &regs, 0) == -1)
    perror_with_name (_("Couldn't get registers"));

  ppcobsd_supply_gregset (&ppcobsd_gregset, current_regcache, -1,
			  &regs, sizeof regs);
}

/* Store register REGNUM back into the inferior.  If REGNUM is -1, do
   this for all registers.  */

static void
ppcobsd_store_registers (int regnum)
{
  struct reg regs;

  if (ptrace (PT_GETREGS, PIDGET (inferior_ptid),
	      (PTRACE_TYPE_ARG3) &regs, 0) == -1)
    perror_with_name (_("Couldn't get registers"));

  ppcobsd_collect_gregset (&ppcobsd_gregset, current_regcache,
			   regnum, &regs, sizeof regs);

  if (ptrace (PT_SETREGS, PIDGET (inferior_ptid),
	      (PTRACE_TYPE_ARG3) &regs, 0) == -1)
    perror_with_name (_("Couldn't write registers"));
}


static int
ppcobsd_supply_pcb (struct regcache *regcache, struct pcb *pcb)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (get_regcache_arch (regcache));
  struct switchframe sf;
  struct callframe cf;
  int i, regnum;

  /* The following is true for OpenBSD 3.7:

     The pcb contains %r1 (the stack pointer) at the point of the
     context switch in cpu_switch().  At that point we have a stack
     frame as described by `struct switchframe', and below that a call
     frame as described by `struct callframe'.  From this information
     we reconstruct the register state as it would look when we are in
     cpu_switch().  */

  /* The stack pointer shouldn't be zero.  */
  if (pcb->pcb_sp == 0)
    return 0;

  read_memory (pcb->pcb_sp, (char *)&sf, sizeof sf);
  regcache_raw_supply (regcache, SP_REGNUM, &sf.sp);
  regcache_raw_supply (regcache, tdep->ppc_cr_regnum, &sf.cr);
  regcache_raw_supply (regcache, tdep->ppc_gp0_regnum + 2, &sf.fixreg2);
  for (i = 0, regnum = tdep->ppc_gp0_regnum + 13; i < 19; i++, regnum++)
    regcache_raw_supply (regcache, regnum, &sf.fixreg[i]);

  read_memory (sf.sp, (char *)&cf, sizeof cf);
  regcache_raw_supply (regcache, PC_REGNUM, &cf.lr);
  regcache_raw_supply (regcache, tdep->ppc_gp0_regnum + 30, &cf.r30);
  regcache_raw_supply (regcache, tdep->ppc_gp0_regnum + 31, &cf.r31);

  return 1;
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_ppcobsd_nat (void);

void
_initialize_ppcobsd_nat (void)
{
  struct target_ops *t;

  /* Add in local overrides.  */
  t = inf_ptrace_target ();
  t->to_fetch_registers = ppcobsd_fetch_registers;
  t->to_store_registers = ppcobsd_store_registers;
  add_target (t);

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

  /* Support debugging kernel virtual memory images.  */
  bsd_kvm_add_target (ppcobsd_supply_pcb);
}

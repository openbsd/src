/* Functions specific to running gdb native on a Sun 4 running sunos4.
   Copyright (C) 1989, 1992, Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	$Id: sparc-nat.c,v 1.1.1.1 1995/10/18 08:40:08 deraadt Exp $
*/

#include "defs.h"
#include "inferior.h"
#include "target.h"

#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <machine/reg.h>
#include <machine/pcb.h>

/* We don't store all registers immediately when requested, since they
   get sent over in large chunks anyway.  Instead, we accumulate most
   of the changes and send them over once.  "deferred_stores" keeps
   track of which sets of registers we have locally-changed copies of,
   so we only need send the groups that have changed.  */

#define	INT_REGS	1
#define	STACK_REGS	2
#define	FP_REGS		4

/* Fetch one or more registers from the inferior.  REGNO == -1 to get
   them all.  We actually fetch more than requested, when convenient,
   marking them as valid so we won't fetch them again.  */

void
fetch_inferior_registers (regno)
     int regno;
{
  struct trapframe tf;
  struct fpstate fpstate;
  int i;

  /* We should never be called with deferred stores, because a prerequisite
     for writing regs is to have fetched them all (PREPARE_TO_STORE), sigh.  */
  if (deferred_stores) abort();

  DO_DEFERRED_STORES;

  /* Global and Out regs are fetched directly, as well as the control
     registers.  If we're getting one of the in or local regs,
     and the stack pointer has not yet been fetched,
     we have to do that first, since they're found in memory relative
     to the stack pointer.  */
  if (regno < O7_REGNUM  /* including -1 */
      || regno >= Y_REGNUM
      || (!register_valid[SP_REGNUM] && regno < I7_REGNUM)) {

      if (0 != ptrace (PT_GETREGS, inferior_pid, (PTRACE_ARG3_TYPE) &tf, 0))
		perror("ptrace_getregs");
      
      registers[REGISTER_BYTE (0)] = 0;
      memcpy (&registers[REGISTER_BYTE (1)], &tf.tf_global[1],
	      15 * REGISTER_RAW_SIZE (G0_REGNUM));
      *(int *)&registers[REGISTER_BYTE (PS_REGNUM)] = tf.tf_psr; 
      *(int *)&registers[REGISTER_BYTE (PC_REGNUM)] = tf.tf_pc;
      *(int *)&registers[REGISTER_BYTE (NPC_REGNUM)] = tf.tf_npc;
      *(int *)&registers[REGISTER_BYTE (Y_REGNUM)] = tf.tf_y;

      for (i = G0_REGNUM; i <= O7_REGNUM; i++)
	register_valid[i] = 1;
      register_valid[Y_REGNUM] = 1;
      register_valid[PS_REGNUM] = 1;
      register_valid[PC_REGNUM] = 1;
      register_valid[NPC_REGNUM] = 1;
      /* If we don't set these valid, read_register_bytes() rereads
	 all the regs every time it is called!  FIXME.  */
      register_valid[WIM_REGNUM] = 1;	/* Not true yet, FIXME */
      register_valid[TBR_REGNUM] = 1;	/* Not true yet, FIXME */
      register_valid[FPS_REGNUM] = 1;	/* Not true yet, FIXME */
      register_valid[CPS_REGNUM] = 1;	/* Not true yet, FIXME */
    }

  /* Floating point registers */
  if (regno == -1 || (regno >= FP0_REGNUM && regno <= FP0_REGNUM + 31))
    {
      if (0 != ptrace (PT_GETFPREGS, inferior_pid,
		       (PTRACE_ARG3_TYPE) &fpstate,
		       0))
	    perror("ptrace_getfpregs");
      memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)], fpstate.fs_regs,
	      sizeof fpstate.fs_regs);
      /* memcpy (&registers[REGISTER_BYTE (FPS_REGNUM)],
	     &fpstate.fs_fsr,
	     sizeof (fpstate.fs_fsr));  FIXME???  -- gnu@cyg */
      for (i = FP0_REGNUM; i <= FP0_REGNUM+31; i++)
	register_valid[i] = 1;
      register_valid[FPS_REGNUM] = 1;
    }

  /* These regs are saved on the stack by the kernel.  Only read them
     all (16 ptrace calls!) if we really need them.  */
  if (regno == -1)
    {
      target_xfer_memory (*(CORE_ADDR*)&registers[REGISTER_BYTE (SP_REGNUM)],
		          &registers[REGISTER_BYTE (L0_REGNUM)],
			  16*REGISTER_RAW_SIZE (L0_REGNUM), 0);
      for (i = L0_REGNUM; i <= I7_REGNUM; i++)
	register_valid[i] = 1;
    }
  else if (regno >= L0_REGNUM && regno <= I7_REGNUM)
    {
      CORE_ADDR sp = *(CORE_ADDR*)&registers[REGISTER_BYTE (SP_REGNUM)];
      i = REGISTER_BYTE (regno);
      if (register_valid[regno])
	printf("register %d valid and read\n", regno);
      target_xfer_memory (sp + i - REGISTER_BYTE (L0_REGNUM),
			  &registers[i], REGISTER_RAW_SIZE (regno), 0);
      register_valid[regno] = 1;
    }
}

/* Store our register values back into the inferior.
   If REGNO is -1, do this for all registers.
   Otherwise, REGNO specifies which register (so we can save time).  */

void
store_inferior_registers (regno)
     int regno;
{
  struct trapframe tf;
  struct fpstate fpstate;
  int wanna_store = INT_REGS + STACK_REGS + FP_REGS;

  /* First decide which pieces of machine-state we need to modify.  
     Default for regno == -1 case is all pieces.  */
  if (regno >= 0)
    if (FP0_REGNUM <= regno && regno < FP0_REGNUM + 32)
      {
	wanna_store = FP_REGS;
      }
    else 
      {
	if (regno == SP_REGNUM)
	  wanna_store = INT_REGS + STACK_REGS;
	else if (regno < L0_REGNUM || regno > I7_REGNUM)
	  wanna_store = INT_REGS;
	else
	  wanna_store = STACK_REGS;
      }

  /* See if we're forcing the stores to happen now, or deferring. */
  if (regno == -2)
    {
      wanna_store = deferred_stores;
      deferred_stores = 0;
    }
  else
    {
      if (wanna_store == STACK_REGS)
	{
	  /* Fall through and just store one stack reg.  If we deferred
	     it, we'd have to store them all, or remember more info.  */
	}
      else
	{
	  deferred_stores |= wanna_store;
	  return;
	}
    }

  if (wanna_store & STACK_REGS)
    {
      CORE_ADDR sp = *(CORE_ADDR *)&registers[REGISTER_BYTE (SP_REGNUM)];

      if (regno < 0 || regno == SP_REGNUM)
	{
	  if (!register_valid[L0_REGNUM+5]) abort();
	  target_xfer_memory (sp, 
			      &registers[REGISTER_BYTE (L0_REGNUM)],
			      16*REGISTER_RAW_SIZE (L0_REGNUM), 1);
	}
      else
	{
	  if (!register_valid[regno]) abort();
	  target_xfer_memory (sp + REGISTER_BYTE (regno) - REGISTER_BYTE (L0_REGNUM),
			      &registers[REGISTER_BYTE (regno)],
			      REGISTER_RAW_SIZE (regno), 1);
	}
	
    }

  if (wanna_store & INT_REGS)
    {
      if (!register_valid[G1_REGNUM]) abort();

      memcpy (&tf.tf_global[1], &registers[REGISTER_BYTE (G1_REGNUM)],
	      15 * REGISTER_RAW_SIZE (G1_REGNUM));

      tf.tf_psr =
	*(int *)&registers[REGISTER_BYTE (PS_REGNUM)];
      tf.tf_pc =
	*(int *)&registers[REGISTER_BYTE (PC_REGNUM)];
      tf.tf_npc =
	*(int *)&registers[REGISTER_BYTE (NPC_REGNUM)];
      tf.tf_y =
	*(int *)&registers[REGISTER_BYTE (Y_REGNUM)];

      if (0 != ptrace (PT_SETREGS, inferior_pid, (PTRACE_ARG3_TYPE) &tf, 0))
		perror("ptrace_setregs");
    }

  if (wanna_store & FP_REGS)
    {
      if (!register_valid[FP0_REGNUM+9]) abort();
      /* Initialize inferior_fp_registers members that gdb doesn't set
	 by reading them from the inferior.  */
      if (0 !=
	 ptrace (PT_GETFPREGS, inferior_pid,
		 (PTRACE_ARG3_TYPE) &fpstate, 0))
	 perror("ptrace_getfpregs");
      memcpy (&fpstate, &registers[REGISTER_BYTE (FP0_REGNUM)],
	      sizeof fpstate.fs_regs);

/*    memcpy (&fpstate.fs_fsr, 
	      &registers[REGISTER_BYTE (FPS_REGNUM)], sizeof (fpstate.fs_fsr));
****/
      if (0 !=
	 ptrace (PT_SETFPREGS, inferior_pid,
		 (PTRACE_ARG3_TYPE) &fpstate, 0))
	 perror("ptrace_setfpregs");
    }
}


void
fetch_core_registers (core_reg_sect, core_reg_size, which, ignore)
  char *core_reg_sect;
  unsigned core_reg_size;
  int which;
  unsigned int ignore;	/* reg addr, unused in this version */
{

  if (which == 0) {

    /* Integer registers */

#define gregs ((struct trapframe *)core_reg_sect)
    /* G0 *always* holds 0.  */
    *(int *)&registers[REGISTER_BYTE (0)] = 0;

    /* The globals and output registers.  */
    memcpy (&registers[REGISTER_BYTE (G1_REGNUM)], &gregs->tf_global[1], 
	    15 * REGISTER_RAW_SIZE (G1_REGNUM));
    *(int *)&registers[REGISTER_BYTE (PS_REGNUM)] = gregs->tf_psr;
    *(int *)&registers[REGISTER_BYTE (PC_REGNUM)] = gregs->tf_pc;
    *(int *)&registers[REGISTER_BYTE (NPC_REGNUM)] = gregs->tf_npc;
    *(int *)&registers[REGISTER_BYTE (Y_REGNUM)] = gregs->tf_y;

    /* My best guess at where to get the locals and input
       registers is exactly where they usually are, right above
       the stack pointer.  If the core dump was caused by a bus error
       from blowing away the stack pointer (as is possible) then this
       won't work, but it's worth the try. */
    {
      int sp;

      sp = *(int *)&registers[REGISTER_BYTE (SP_REGNUM)];
      if (0 != target_read_memory (sp, &registers[REGISTER_BYTE (L0_REGNUM)], 
			  16 * REGISTER_RAW_SIZE (L0_REGNUM)))
	{
	  /* fprintf so user can still use gdb */
	  fprintf (stderr,
		   "Couldn't read input and local registers from core file\n");
	}
    }
  } else if (which == 2) {

    /* Floating point registers */

#define fpuregs  ((struct fpstate *) core_reg_sect)
    if (core_reg_size >= sizeof (struct fpstate))
      {
	memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)], fpuregs->fs_regs,
		sizeof (fpuregs->fs_regs));
	memcpy (&registers[REGISTER_BYTE (FPS_REGNUM)], &fpuregs->fs_fsr,
		sizeof (fpuregs->fs_fsr));
      }
    else
      fprintf (stderr, "Couldn't read float regs from core file\n");
  }
}

void
fetch_kcore_registers (pcb)
struct pcb *pcb;
{
	struct rwindow win;
	int i;
	u_long sp;

	/* We only do integer registers */
	sp = pcb->pcb_sp;

	supply_register(SP_REGNUM, (char *)&pcb->pcb_sp);
	supply_register(PC_REGNUM, (char *)&pcb->pcb_pc);
	supply_register(O7_REGNUM, (char *)&pcb->pcb_pc);
	supply_register(PS_REGNUM, (char *)&pcb->pcb_psr);
	supply_register(WIM_REGNUM, (char *)&pcb->pcb_wim);
	/*
	 * Read last register window saved on stack.
	 */
	if (target_read_memory(sp, (char *)&win, sizeof win)) {
		printf("cannot read register window at sp=%x\n", pcb->pcb_sp);
		bzero((char *)&win, sizeof win);
	}
	for (i = 0; i < sizeof(win.rw_local); ++i)
		supply_register(i + L0_REGNUM, (char *)&win.rw_local[i]);
	for (i = 0; i < sizeof(win.rw_in); ++i)
		supply_register(i + I0_REGNUM, (char *)&win.rw_in[i]);
	/*
	 * read the globals & outs saved on the stack (for a trap frame).
	 */
	sp += 92 + 12; /* XXX - MINFRAME + R_Y */
	for (i = 1; i < 14; ++i) {
		u_long val;
 
		if (target_read_memory(sp + i*4, (char *)&val, sizeof val) == 0)
			supply_register(i, (char *)&val);
	}
#if 0
	if (kvread(pcb.pcb_cpctxp, &cps) == 0)
		supply_register(CPS_REGNUM, (char *)&cps);
#endif
}

void
clear_regs()
{
	u_long reg = 0;
	float freg = 0.0;
	int i;

	for (i = 0; i < FP0_REGNUM; ++i)
		supply_register(i, (char *)&reg);
	for (; i < FP0_REGNUM + 32; ++i) /* XXX */
		supply_register(i, (char *)&freg);
	for (; i < NUM_REGS; ++i)
		supply_register(i, (char *)&reg);
}


/* Functions specific to running gdb native on a SPARC running NetBSD
   Copyright 1989, 1992, 1993, 1994, 1996 Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <sys/types.h>
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include <machine/pcb.h>

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "gdbcore.h"

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
  struct reg inferior_registers;
  struct fpreg inferior_fp_registers;
  int save_g0;
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
      || (!register_valid[SP_REGNUM] && regno < I7_REGNUM))
    {
      if (0 != ptrace (PT_GETREGS, inferior_pid,
		       (PTRACE_ARG3_TYPE) &inferior_registers, 0))
	perror("ptrace_getregs");

      /* Copy them (in order shown in reg.h) */
      memcpy (&registers[REGISTER_BYTE (G0_REGNUM)],
	      &inferior_registers.r_global[0],
	      sizeof(inferior_registers.r_global));
      memcpy (&registers[REGISTER_BYTE (O0_REGNUM)],
	      &inferior_registers.r_out[0],
	      sizeof(inferior_registers.r_out));
      *(int *)&registers[REGISTER_BYTE (PS_REGNUM)] =
	inferior_registers.r_psr;
      *(int *)&registers[REGISTER_BYTE (PC_REGNUM)] =
	inferior_registers.r_pc;
      *(int *)&registers[REGISTER_BYTE (NPC_REGNUM)] =
	inferior_registers.r_npc;
      *(int *)&registers[REGISTER_BYTE (Y_REGNUM)] =
	inferior_registers.r_y;

      /*
       * Note that the G0 slot actually carries the
       * value of the %wim register, and G0 is zero.
       */
      *(int *)&registers[REGISTER_BYTE(WIM_REGNUM)] =
        *(int *)&registers[REGISTER_BYTE(G0_REGNUM)];
      *(int *)&registers[REGISTER_BYTE(G0_REGNUM)] = 0;

      /* Mark what is valid (not the %i regs). */
      for (i = G0_REGNUM; i <= O7_REGNUM; i++)
	register_valid[i] = 1;
      register_valid[PS_REGNUM] = 1;
      register_valid[PC_REGNUM] = 1;
      register_valid[NPC_REGNUM] = 1;
      register_valid[Y_REGNUM] = 1;
      register_valid[WIM_REGNUM] = 1;

      /* If we don't set these valid, read_register_bytes() rereads
	 all the regs every time it is called!  FIXME.  */
      register_valid[TBR_REGNUM] = 1;	/* Not true yet, FIXME */
      register_valid[CPS_REGNUM] = 1;	/* Not true yet, FIXME */
    }

  /* Floating point registers */
  if (regno == -1 || regno == FPS_REGNUM ||
      (regno >= FP0_REGNUM && regno <= FP0_REGNUM + 31))
    {
      if (0 != ptrace (PT_GETFPREGS, inferior_pid,
		       (PTRACE_ARG3_TYPE) &inferior_fp_registers,
		       0))
	perror("ptrace_getfpregs");
      memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)],
	      &inferior_fp_registers.fr_regs[0],
	      sizeof (inferior_fp_registers.fr_regs));
      memcpy (&registers[REGISTER_BYTE (FPS_REGNUM)],
	      &inferior_fp_registers.fr_fsr,
	      sizeof (inferior_fp_registers.fr_fsr));
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
	printf_unfiltered("register %d valid and read\n", regno);
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
  struct reg inferior_registers;
  struct fpreg inferior_fp_registers;
  int wanna_store = INT_REGS + STACK_REGS + FP_REGS;
  int save_g0;

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
	else if (regno == FPS_REGNUM)
	  wanna_store = FP_REGS;
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
	  target_xfer_memory ((sp + REGISTER_BYTE (regno) -
			       REGISTER_BYTE (L0_REGNUM)),
			      &registers[REGISTER_BYTE (regno)],
			      REGISTER_RAW_SIZE (regno), 1);
	}
	
    }

  if (wanna_store & INT_REGS)
    {
      if (!register_valid[G1_REGNUM]) abort();

      /* The G0 slot really holds %wim (leave it alone). */
      save_g0 = inferior_registers.r_global[0];
      memcpy (&inferior_registers.r_global[0],
	      &registers[REGISTER_BYTE (G0_REGNUM)],
	      sizeof(inferior_registers.r_global));
      inferior_registers.r_global[0] = save_g0;
      memcpy (&inferior_registers.r_out[0],
	      &registers[REGISTER_BYTE (O0_REGNUM)],
	      sizeof(inferior_registers.r_out));

      inferior_registers.r_psr =
	*(int *)&registers[REGISTER_BYTE (PS_REGNUM)];
      inferior_registers.r_pc =
	*(int *)&registers[REGISTER_BYTE (PC_REGNUM)];
      inferior_registers.r_npc =
	*(int *)&registers[REGISTER_BYTE (NPC_REGNUM)];
      inferior_registers.r_y =
	*(int *)&registers[REGISTER_BYTE (Y_REGNUM)];

      if (0 != ptrace (PT_SETREGS, inferior_pid,
		       (PTRACE_ARG3_TYPE) &inferior_registers, 0))
	perror("ptrace_setregs");
    }

  if (wanna_store & FP_REGS)
    {
      if (!register_valid[FP0_REGNUM+9]) abort();
      memcpy (&inferior_fp_registers.fr_regs[0],
	      &registers[REGISTER_BYTE (FP0_REGNUM)],
	      sizeof(inferior_fp_registers.fr_regs));
      memcpy (&inferior_fp_registers.fr_fsr, 
	      &registers[REGISTER_BYTE (FPS_REGNUM)],
	      sizeof(inferior_fp_registers.fr_fsr));
      if (0 !=
	 ptrace (PT_SETFPREGS, inferior_pid,
		 (PTRACE_ARG3_TYPE) &inferior_fp_registers, 0))
	 perror("ptrace_setfpregs");
    }
}


static void
fetch_core_registers (core_reg_sect, core_reg_size, which, reg_addr)
     char *core_reg_sect;
     unsigned core_reg_size;
     int which;
     unsigned int reg_addr;	/* Unused in this version */
{
  struct md_coredump *core_reg;
  struct trapframe *tf;
  struct fpstate *fs;

  core_reg = (struct md_coredump *)core_reg_sect;
  tf = &core_reg->md_tf;
  fs = &core_reg->md_fpstate;

  /* We get everything from the .reg section. */
  if (which != 0)
    return;

  if (core_reg_size < sizeof(*core_reg)) {
    fprintf_unfiltered (gdb_stderr, "Couldn't read regs from core file\n");
    return;
  }

  /* Integer registers */
  memcpy(&registers[REGISTER_BYTE (G0_REGNUM)],
	 &tf->tf_global[0], sizeof(tf->tf_global));
  memcpy(&registers[REGISTER_BYTE (O0_REGNUM)],
	 &tf->tf_out[0], sizeof(tf->tf_out));
  *(int *)&registers[REGISTER_BYTE (PS_REGNUM)]  = tf->tf_psr;
  *(int *)&registers[REGISTER_BYTE (PC_REGNUM)]  = tf->tf_pc;
  *(int *)&registers[REGISTER_BYTE (NPC_REGNUM)] = tf->tf_npc;
  *(int *)&registers[REGISTER_BYTE (Y_REGNUM)]   = tf->tf_y;

  /* Clear out the G0 slot (see reg.h) */
  *(int *)&registers[REGISTER_BYTE(G0_REGNUM)] = 0;

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
	/* fprintf_unfiltered so user can still use gdb */
	fprintf_unfiltered (gdb_stderr,
		"Couldn't read input and local registers from core file\n");
      }
  }

  /* Floating point registers */
  memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)],
	  &fs->fs_regs[0], sizeof (fs->fs_regs));
  memcpy (&registers[REGISTER_BYTE (FPS_REGNUM)],
	  &fs->fs_fsr,	sizeof (fs->fs_fsr));

  registers_fetched ();
}

/* Register that we are able to handle sparcnbsd core file formats.
   FIXME: is this really bfd_target_unknown_flavour? */

static struct core_fns nat_core_fns =
{
  bfd_target_unknown_flavour,
  fetch_core_registers,
  NULL
};

void
_initialize_sparcnbsd_nat ()
{
  add_core_fns (&nat_core_fns);
}


/*
 * kernel_u_size() is not helpful on NetBSD because
 * the "u" struct is NOT in the core dump file.
 */

#ifdef	FETCH_KCORE_REGISTERS
/*
 * Get registers from a kernel crash dump or live kernel.
 * Called by kcore-nbsd.c:get_kcore_registers().
 */
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

  /* The kernel does not use the FPU, so ignore it. */
  registers_fetched ();
}
#endif	/* FETCH_KCORE_REGISTERS */

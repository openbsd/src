/* Target-dependent code for GDB, the GNU debugger.

   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1994, 1995, 1996,
   1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004 Free Software
   Foundation, Inc.

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
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "target.h"
#include "gdbcore.h"
#include "gdbcmd.h"
#include "objfiles.h"
#include "arch-utils.h"
#include "regcache.h"
#include "regset.h"
#include "doublest.h"
#include "value.h"
#include "parser-defs.h"
#include "osabi.h"
#include "infcall.h"
#include "sim-regno.h"
#include "gdb/sim-ppc.h"
#include "reggroups.h"

#include "libbfd.h"		/* for bfd_default_set_arch_mach */
#include "coff/internal.h"	/* for libcoff.h */
#include "libcoff.h"		/* for xcoff_data */
#include "coff/xcoff.h"
#include "libxcoff.h"

#include "elf-bfd.h"

#include "solib-svr4.h"
#include "ppc-tdep.h"

#include "gdb_assert.h"
#include "dis-asm.h"

#include "trad-frame.h"
#include "frame-unwind.h"
#include "frame-base.h"

/* If the kernel has to deliver a signal, it pushes a sigcontext
   structure on the stack and then calls the signal handler, passing
   the address of the sigcontext in an argument register. Usually
   the signal handler doesn't save this register, so we have to
   access the sigcontext structure via an offset from the signal handler
   frame.
   The following constants were determined by experimentation on AIX 3.2.  */
#define SIG_FRAME_PC_OFFSET 96
#define SIG_FRAME_LR_OFFSET 108
#define SIG_FRAME_FP_OFFSET 284

/* To be used by skip_prologue. */

struct rs6000_framedata
  {
    int offset;			/* total size of frame --- the distance
				   by which we decrement sp to allocate
				   the frame */
    int saved_gpr;		/* smallest # of saved gpr */
    int saved_fpr;		/* smallest # of saved fpr */
    int saved_vr;               /* smallest # of saved vr */
    int saved_ev;               /* smallest # of saved ev */
    int alloca_reg;		/* alloca register number (frame ptr) */
    char frameless;		/* true if frameless functions. */
    char nosavedpc;		/* true if pc not saved. */
    int gpr_offset;		/* offset of saved gprs from prev sp */
    int fpr_offset;		/* offset of saved fprs from prev sp */
    int vr_offset;              /* offset of saved vrs from prev sp */
    int ev_offset;              /* offset of saved evs from prev sp */
    int lr_offset;		/* offset of saved lr */
    int cr_offset;		/* offset of saved cr */
    int vrsave_offset;          /* offset of saved vrsave register */
  };

/* Description of a single register. */

struct reg
  {
    char *name;			/* name of register */
    unsigned char sz32;		/* size on 32-bit arch, 0 if nonextant */
    unsigned char sz64;		/* size on 64-bit arch, 0 if nonextant */
    unsigned char fpr;		/* whether register is floating-point */
    unsigned char pseudo;       /* whether register is pseudo */
    int spr_num;                /* PowerPC SPR number, or -1 if not an SPR.
                                   This is an ISA SPR number, not a GDB
                                   register number.  */
  };

/* Breakpoint shadows for the single step instructions will be kept here. */

static struct sstep_breaks
  {
    /* Address, or 0 if this is not in use.  */
    CORE_ADDR address;
    /* Shadow contents.  */
    char data[4];
  }
stepBreaks[2];

/* Hook for determining the TOC address when calling functions in the
   inferior under AIX. The initialization code in rs6000-nat.c sets
   this hook to point to find_toc_address.  */

CORE_ADDR (*rs6000_find_toc_address_hook) (CORE_ADDR) = NULL;

/* Hook to set the current architecture when starting a child process. 
   rs6000-nat.c sets this. */

void (*rs6000_set_host_arch_hook) (int) = NULL;

/* Static function prototypes */

static CORE_ADDR branch_dest (int opcode, int instr, CORE_ADDR pc,
			      CORE_ADDR safety);
static CORE_ADDR skip_prologue (CORE_ADDR, CORE_ADDR,
                                struct rs6000_framedata *);

/* Is REGNO an AltiVec register?  Return 1 if so, 0 otherwise.  */
int
altivec_register_p (int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  if (tdep->ppc_vr0_regnum < 0 || tdep->ppc_vrsave_regnum < 0)
    return 0;
  else
    return (regno >= tdep->ppc_vr0_regnum && regno <= tdep->ppc_vrsave_regnum);
}


/* Return true if REGNO is an SPE register, false otherwise.  */
int
spe_register_p (int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  
  /* Is it a reference to EV0 -- EV31, and do we have those?  */
  if (tdep->ppc_ev0_regnum >= 0
      && tdep->ppc_ev31_regnum >= 0
      && tdep->ppc_ev0_regnum <= regno && regno <= tdep->ppc_ev31_regnum)
    return 1;

  /* Is it a reference to one of the raw upper GPR halves?  */
  if (tdep->ppc_ev0_upper_regnum >= 0
      && tdep->ppc_ev0_upper_regnum <= regno
      && regno < tdep->ppc_ev0_upper_regnum + ppc_num_gprs)
    return 1;

  /* Is it a reference to the 64-bit accumulator, and do we have that?  */
  if (tdep->ppc_acc_regnum >= 0
      && tdep->ppc_acc_regnum == regno)
    return 1;

  /* Is it a reference to the SPE floating-point status and control register,
     and do we have that?  */
  if (tdep->ppc_spefscr_regnum >= 0
      && tdep->ppc_spefscr_regnum == regno)
    return 1;

  return 0;
}


/* Return non-zero if the architecture described by GDBARCH has
   floating-point registers (f0 --- f31 and fpscr).  */
int
ppc_floating_point_unit_p (struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  return (tdep->ppc_fp0_regnum >= 0
          && tdep->ppc_fpscr_regnum >= 0);
}


/* Check that TABLE[GDB_REGNO] is not already initialized, and then
   set it to SIM_REGNO.

   This is a helper function for init_sim_regno_table, constructing
   the table mapping GDB register numbers to sim register numbers; we
   initialize every element in that table to -1 before we start
   filling it in.  */
static void
set_sim_regno (int *table, int gdb_regno, int sim_regno)
{
  /* Make sure we don't try to assign any given GDB register a sim
     register number more than once.  */
  gdb_assert (table[gdb_regno] == -1);
  table[gdb_regno] = sim_regno;
}


/* Initialize ARCH->tdep->sim_regno, the table mapping GDB register
   numbers to simulator register numbers, based on the values placed
   in the ARCH->tdep->ppc_foo_regnum members.  */
static void
init_sim_regno_table (struct gdbarch *arch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (arch);
  int total_regs = gdbarch_num_regs (arch) + gdbarch_num_pseudo_regs (arch);
  const struct reg *regs = tdep->regs;
  int *sim_regno = GDBARCH_OBSTACK_CALLOC (arch, total_regs, int);
  int i;

  /* Presume that all registers not explicitly mentioned below are
     unavailable from the sim.  */
  for (i = 0; i < total_regs; i++)
    sim_regno[i] = -1;

  /* General-purpose registers.  */
  for (i = 0; i < ppc_num_gprs; i++)
    set_sim_regno (sim_regno, tdep->ppc_gp0_regnum + i, sim_ppc_r0_regnum + i);
  
  /* Floating-point registers.  */
  if (tdep->ppc_fp0_regnum >= 0)
    for (i = 0; i < ppc_num_fprs; i++)
      set_sim_regno (sim_regno,
                     tdep->ppc_fp0_regnum + i,
                     sim_ppc_f0_regnum + i);
  if (tdep->ppc_fpscr_regnum >= 0)
    set_sim_regno (sim_regno, tdep->ppc_fpscr_regnum, sim_ppc_fpscr_regnum);

  set_sim_regno (sim_regno, gdbarch_pc_regnum (arch), sim_ppc_pc_regnum);
  set_sim_regno (sim_regno, tdep->ppc_ps_regnum, sim_ppc_ps_regnum);
  set_sim_regno (sim_regno, tdep->ppc_cr_regnum, sim_ppc_cr_regnum);

  /* Segment registers.  */
  if (tdep->ppc_sr0_regnum >= 0)
    for (i = 0; i < ppc_num_srs; i++)
      set_sim_regno (sim_regno,
                     tdep->ppc_sr0_regnum + i,
                     sim_ppc_sr0_regnum + i);

  /* Altivec registers.  */
  if (tdep->ppc_vr0_regnum >= 0)
    {
      for (i = 0; i < ppc_num_vrs; i++)
        set_sim_regno (sim_regno,
                       tdep->ppc_vr0_regnum + i,
                       sim_ppc_vr0_regnum + i);

      /* FIXME: jimb/2004-07-15: when we have tdep->ppc_vscr_regnum,
         we can treat this more like the other cases.  */
      set_sim_regno (sim_regno,
                     tdep->ppc_vr0_regnum + ppc_num_vrs,
                     sim_ppc_vscr_regnum);
    }
  /* vsave is a special-purpose register, so the code below handles it.  */

  /* SPE APU (E500) registers.  */
  if (tdep->ppc_ev0_regnum >= 0)
    for (i = 0; i < ppc_num_gprs; i++)
      set_sim_regno (sim_regno,
                     tdep->ppc_ev0_regnum + i,
                     sim_ppc_ev0_regnum + i);
  if (tdep->ppc_ev0_upper_regnum >= 0)
    for (i = 0; i < ppc_num_gprs; i++)
      set_sim_regno (sim_regno,
                     tdep->ppc_ev0_upper_regnum + i,
                     sim_ppc_rh0_regnum + i);
  if (tdep->ppc_acc_regnum >= 0)
    set_sim_regno (sim_regno, tdep->ppc_acc_regnum, sim_ppc_acc_regnum);
  /* spefscr is a special-purpose register, so the code below handles it.  */

  /* Now handle all special-purpose registers.  Verify that they
     haven't mistakenly been assigned numbers by any of the above
     code).  */
  for (i = 0; i < total_regs; i++)
    if (regs[i].spr_num >= 0)
      set_sim_regno (sim_regno, i, regs[i].spr_num + sim_ppc_spr0_regnum);

  /* Drop the initialized array into place.  */
  tdep->sim_regno = sim_regno;
}


/* Given a GDB register number REG, return the corresponding SIM
   register number.  */
static int
rs6000_register_sim_regno (int reg)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int sim_regno;

  gdb_assert (0 <= reg && reg <= NUM_REGS + NUM_PSEUDO_REGS);
  sim_regno = tdep->sim_regno[reg];

  if (sim_regno >= 0)
    return sim_regno;
  else
    return LEGACY_SIM_REGNO_IGNORE;
}



/* Register set support functions.  */

static void
ppc_supply_reg (struct regcache *regcache, int regnum, 
		const char *regs, size_t offset)
{
  if (regnum != -1 && offset != -1)
    regcache_raw_supply (regcache, regnum, regs + offset);
}

static void
ppc_collect_reg (const struct regcache *regcache, int regnum,
		 char *regs, size_t offset)
{
  if (regnum != -1 && offset != -1)
    regcache_raw_collect (regcache, regnum, regs + offset);
}
    
/* Supply register REGNUM in the general-purpose register set REGSET
   from the buffer specified by GREGS and LEN to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

void
ppc_supply_gregset (const struct regset *regset, struct regcache *regcache,
		    int regnum, const void *gregs, size_t len)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  const struct ppc_reg_offsets *offsets = regset->descr;
  size_t offset;
  int i;

  for (i = tdep->ppc_gp0_regnum, offset = offsets->r0_offset;
       i < tdep->ppc_gp0_regnum + ppc_num_gprs;
       i++, offset += 4)
    {
      if (regnum == -1 || regnum == i)
	ppc_supply_reg (regcache, i, gregs, offset);
    }

  if (regnum == -1 || regnum == PC_REGNUM)
    ppc_supply_reg (regcache, PC_REGNUM, gregs, offsets->pc_offset);
  if (regnum == -1 || regnum == tdep->ppc_ps_regnum)
    ppc_supply_reg (regcache, tdep->ppc_ps_regnum,
		    gregs, offsets->ps_offset);
  if (regnum == -1 || regnum == tdep->ppc_cr_regnum)
    ppc_supply_reg (regcache, tdep->ppc_cr_regnum,
		    gregs, offsets->cr_offset);
  if (regnum == -1 || regnum == tdep->ppc_lr_regnum)
    ppc_supply_reg (regcache, tdep->ppc_lr_regnum,
		    gregs, offsets->lr_offset);
  if (regnum == -1 || regnum == tdep->ppc_ctr_regnum)
    ppc_supply_reg (regcache, tdep->ppc_ctr_regnum,
		    gregs, offsets->ctr_offset);
  if (regnum == -1 || regnum == tdep->ppc_xer_regnum)
    ppc_supply_reg (regcache, tdep->ppc_xer_regnum,
		    gregs, offsets->cr_offset);
  if (regnum == -1 || regnum == tdep->ppc_mq_regnum)
    ppc_supply_reg (regcache, tdep->ppc_mq_regnum, gregs, offsets->mq_offset);
}

/* Supply register REGNUM in the floating-point register set REGSET
   from the buffer specified by FPREGS and LEN to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

void
ppc_supply_fpregset (const struct regset *regset, struct regcache *regcache,
		     int regnum, const void *fpregs, size_t len)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  const struct ppc_reg_offsets *offsets = regset->descr;
  size_t offset;
  int i;

  gdb_assert (ppc_floating_point_unit_p (gdbarch));

  offset = offsets->f0_offset;
  for (i = tdep->ppc_fp0_regnum;
       i < tdep->ppc_fp0_regnum + ppc_num_fprs;
       i++, offset += 4)
    {
      if (regnum == -1 || regnum == i)
	ppc_supply_reg (regcache, i, fpregs, offset);
    }

  if (regnum == -1 || regnum == tdep->ppc_fpscr_regnum)
    ppc_supply_reg (regcache, tdep->ppc_fpscr_regnum,
		    fpregs, offsets->fpscr_offset);
}

/* Collect register REGNUM in the general-purpose register set
   REGSET. from register cache REGCACHE into the buffer specified by
   GREGS and LEN.  If REGNUM is -1, do this for all registers in
   REGSET.  */

void
ppc_collect_gregset (const struct regset *regset,
		     const struct regcache *regcache,
		     int regnum, void *gregs, size_t len)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  const struct ppc_reg_offsets *offsets = regset->descr;
  size_t offset;
  int i;

  offset = offsets->r0_offset;
  for (i = tdep->ppc_gp0_regnum;
       i < tdep->ppc_gp0_regnum + ppc_num_gprs;
       i++, offset += 4)
    {
      if (regnum == -1 || regnum == i)
	ppc_collect_reg (regcache, i, gregs, offset);
    }

  if (regnum == -1 || regnum == PC_REGNUM)
    ppc_collect_reg (regcache, PC_REGNUM, gregs, offsets->pc_offset);
  if (regnum == -1 || regnum == tdep->ppc_ps_regnum)
    ppc_collect_reg (regcache, tdep->ppc_ps_regnum,
		     gregs, offsets->ps_offset);
  if (regnum == -1 || regnum == tdep->ppc_cr_regnum)
    ppc_collect_reg (regcache, tdep->ppc_cr_regnum,
		     gregs, offsets->cr_offset);
  if (regnum == -1 || regnum == tdep->ppc_lr_regnum)
    ppc_collect_reg (regcache, tdep->ppc_lr_regnum,
		     gregs, offsets->lr_offset);
  if (regnum == -1 || regnum == tdep->ppc_ctr_regnum)
    ppc_collect_reg (regcache, tdep->ppc_ctr_regnum,
		     gregs, offsets->ctr_offset);
  if (regnum == -1 || regnum == tdep->ppc_xer_regnum)
    ppc_collect_reg (regcache, tdep->ppc_xer_regnum,
		     gregs, offsets->xer_offset);
  if (regnum == -1 || regnum == tdep->ppc_mq_regnum)
    ppc_collect_reg (regcache, tdep->ppc_mq_regnum,
		     gregs, offsets->mq_offset);
}

/* Collect register REGNUM in the floating-point register set
   REGSET. from register cache REGCACHE into the buffer specified by
   FPREGS and LEN.  If REGNUM is -1, do this for all registers in
   REGSET.  */

void
ppc_collect_fpregset (const struct regset *regset,
		      const struct regcache *regcache,
		      int regnum, void *fpregs, size_t len)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  const struct ppc_reg_offsets *offsets = regset->descr;
  size_t offset;
  int i;

  gdb_assert (ppc_floating_point_unit_p (gdbarch));

  offset = offsets->f0_offset;
  for (i = tdep->ppc_fp0_regnum;
       i <= tdep->ppc_fp0_regnum + ppc_num_fprs;
       i++, offset += 4)
    {
      if (regnum == -1 || regnum == i)
	ppc_collect_reg (regcache, regnum, fpregs, offset);
    }

  if (regnum == -1 || regnum == tdep->ppc_fpscr_regnum)
    ppc_collect_reg (regcache, tdep->ppc_fpscr_regnum,
		     fpregs, offsets->fpscr_offset);
}


/* Read a LEN-byte address from debugged memory address MEMADDR. */

static CORE_ADDR
read_memory_addr (CORE_ADDR memaddr, int len)
{
  return read_memory_unsigned_integer (memaddr, len);
}

static CORE_ADDR
rs6000_skip_prologue (CORE_ADDR pc)
{
  struct rs6000_framedata frame;
  pc = skip_prologue (pc, 0, &frame);
  return pc;
}


/* Fill in fi->saved_regs */

struct frame_extra_info
{
  /* Functions calling alloca() change the value of the stack
     pointer. We need to use initial stack pointer (which is saved in
     r31 by gcc) in such cases. If a compiler emits traceback table,
     then we should use the alloca register specified in traceback
     table. FIXME. */
  CORE_ADDR initial_sp;		/* initial stack pointer. */
};

/* Get the ith function argument for the current function.  */
static CORE_ADDR
rs6000_fetch_pointer_argument (struct frame_info *frame, int argi, 
			       struct type *type)
{
  CORE_ADDR addr;
  get_frame_register (frame, 3 + argi, &addr);
  return addr;
}

/* Calculate the destination of a branch/jump.  Return -1 if not a branch.  */

static CORE_ADDR
branch_dest (int opcode, int instr, CORE_ADDR pc, CORE_ADDR safety)
{
  CORE_ADDR dest;
  int immediate;
  int absolute;
  int ext_op;

  absolute = (int) ((instr >> 1) & 1);

  switch (opcode)
    {
    case 18:
      immediate = ((instr & ~3) << 6) >> 6;	/* br unconditional */
      if (absolute)
	dest = immediate;
      else
	dest = pc + immediate;
      break;

    case 16:
      immediate = ((instr & ~3) << 16) >> 16;	/* br conditional */
      if (absolute)
	dest = immediate;
      else
	dest = pc + immediate;
      break;

    case 19:
      ext_op = (instr >> 1) & 0x3ff;

      if (ext_op == 16)		/* br conditional register */
	{
          dest = read_register (gdbarch_tdep (current_gdbarch)->ppc_lr_regnum) & ~3;

	  /* If we are about to return from a signal handler, dest is
	     something like 0x3c90.  The current frame is a signal handler
	     caller frame, upon completion of the sigreturn system call
	     execution will return to the saved PC in the frame.  */
	  if (dest < TEXT_SEGMENT_BASE)
	    {
	      struct frame_info *fi;

	      fi = get_current_frame ();
	      if (fi != NULL)
		dest = read_memory_addr (get_frame_base (fi) + SIG_FRAME_PC_OFFSET,
					 gdbarch_tdep (current_gdbarch)->wordsize);
	    }
	}

      else if (ext_op == 528)	/* br cond to count reg */
	{
          dest = read_register (gdbarch_tdep (current_gdbarch)->ppc_ctr_regnum) & ~3;

	  /* If we are about to execute a system call, dest is something
	     like 0x22fc or 0x3b00.  Upon completion the system call
	     will return to the address in the link register.  */
	  if (dest < TEXT_SEGMENT_BASE)
            dest = read_register (gdbarch_tdep (current_gdbarch)->ppc_lr_regnum) & ~3;
	}
      else
	return -1;
      break;

    default:
      return -1;
    }
  return (dest < TEXT_SEGMENT_BASE) ? safety : dest;
}


/* Sequence of bytes for breakpoint instruction.  */

const static unsigned char *
rs6000_breakpoint_from_pc (CORE_ADDR *bp_addr, int *bp_size)
{
  static unsigned char big_breakpoint[] = { 0x7d, 0x82, 0x10, 0x08 };
  static unsigned char little_breakpoint[] = { 0x08, 0x10, 0x82, 0x7d };
  *bp_size = 4;
  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
    return big_breakpoint;
  else
    return little_breakpoint;
}


/* AIX does not support PT_STEP. Simulate it. */

void
rs6000_software_single_step (enum target_signal signal,
			     int insert_breakpoints_p)
{
  CORE_ADDR dummy;
  int breakp_sz;
  const char *breakp = rs6000_breakpoint_from_pc (&dummy, &breakp_sz);
  int ii, insn;
  CORE_ADDR loc;
  CORE_ADDR breaks[2];
  int opcode;

  if (insert_breakpoints_p)
    {

      loc = read_pc ();

      insn = read_memory_integer (loc, 4);

      breaks[0] = loc + breakp_sz;
      opcode = insn >> 26;
      breaks[1] = branch_dest (opcode, insn, loc, breaks[0]);

      /* Don't put two breakpoints on the same address. */
      if (breaks[1] == breaks[0])
	breaks[1] = -1;

      stepBreaks[1].address = 0;

      for (ii = 0; ii < 2; ++ii)
	{

	  /* ignore invalid breakpoint. */
	  if (breaks[ii] == -1)
	    continue;
	  target_insert_breakpoint (breaks[ii], stepBreaks[ii].data);
	  stepBreaks[ii].address = breaks[ii];
	}

    }
  else
    {

      /* remove step breakpoints. */
      for (ii = 0; ii < 2; ++ii)
	if (stepBreaks[ii].address != 0)
	  target_remove_breakpoint (stepBreaks[ii].address,
				    stepBreaks[ii].data);
    }
  errno = 0;			/* FIXME, don't ignore errors! */
  /* What errors?  {read,write}_memory call error().  */
}


/* return pc value after skipping a function prologue and also return
   information about a function frame.

   in struct rs6000_framedata fdata:
   - frameless is TRUE, if function does not have a frame.
   - nosavedpc is TRUE, if function does not save %pc value in its frame.
   - offset is the initial size of this stack frame --- the amount by
   which we decrement the sp to allocate the frame.
   - saved_gpr is the number of the first saved gpr.
   - saved_fpr is the number of the first saved fpr.
   - saved_vr is the number of the first saved vr.
   - saved_ev is the number of the first saved ev.
   - alloca_reg is the number of the register used for alloca() handling.
   Otherwise -1.
   - gpr_offset is the offset of the first saved gpr from the previous frame.
   - fpr_offset is the offset of the first saved fpr from the previous frame.
   - vr_offset is the offset of the first saved vr from the previous frame.
   - ev_offset is the offset of the first saved ev from the previous frame.
   - lr_offset is the offset of the saved lr
   - cr_offset is the offset of the saved cr
   - vrsave_offset is the offset of the saved vrsave register
 */

#define SIGNED_SHORT(x) 						\
  ((sizeof (short) == 2)						\
   ? ((int)(short)(x))							\
   : ((int)((((x) & 0xffff) ^ 0x8000) - 0x8000)))

#define GET_SRC_REG(x) (((x) >> 21) & 0x1f)

/* Limit the number of skipped non-prologue instructions, as the examining
   of the prologue is expensive.  */
static int max_skip_non_prologue_insns = 10;

/* Given PC representing the starting address of a function, and
   LIM_PC which is the (sloppy) limit to which to scan when looking
   for a prologue, attempt to further refine this limit by using
   the line data in the symbol table.  If successful, a better guess
   on where the prologue ends is returned, otherwise the previous
   value of lim_pc is returned.  */

/* FIXME: cagney/2004-02-14: This function and logic have largely been
   superseded by skip_prologue_using_sal.  */

static CORE_ADDR
refine_prologue_limit (CORE_ADDR pc, CORE_ADDR lim_pc)
{
  struct symtab_and_line prologue_sal;

  prologue_sal = find_pc_line (pc, 0);
  if (prologue_sal.line != 0)
    {
      int i;
      CORE_ADDR addr = prologue_sal.end;

      /* Handle the case in which compiler's optimizer/scheduler
         has moved instructions into the prologue.  We scan ahead
	 in the function looking for address ranges whose corresponding
	 line number is less than or equal to the first one that we
	 found for the function.  (It can be less than when the
	 scheduler puts a body instruction before the first prologue
	 instruction.)  */
      for (i = 2 * max_skip_non_prologue_insns; 
           i > 0 && (lim_pc == 0 || addr < lim_pc);
	   i--)
        {
	  struct symtab_and_line sal;

	  sal = find_pc_line (addr, 0);
	  if (sal.line == 0)
	    break;
	  if (sal.line <= prologue_sal.line 
	      && sal.symtab == prologue_sal.symtab)
	    {
	      prologue_sal = sal;
	    }
	  addr = sal.end;
	}

      if (lim_pc == 0 || prologue_sal.end < lim_pc)
	lim_pc = prologue_sal.end;
    }
  return lim_pc;
}

/* Return nonzero if the given instruction OP can be part of the prologue
   of a function and saves a parameter on the stack.  FRAMEP should be
   set if one of the previous instructions in the function has set the
   Frame Pointer.  */

static int
store_param_on_stack_p (unsigned long op, int framep, int *r0_contains_arg)
{
  /* Move parameters from argument registers to temporary register.  */
  if ((op & 0xfc0007fe) == 0x7c000378)         /* mr(.)  Rx,Ry */
    {
      /* Rx must be scratch register r0.  */
      const int rx_regno = (op >> 16) & 31;
      /* Ry: Only r3 - r10 are used for parameter passing.  */
      const int ry_regno = GET_SRC_REG (op);

      if (rx_regno == 0 && ry_regno >= 3 && ry_regno <= 10)
        {
          *r0_contains_arg = 1;
          return 1;
        }
      else
        return 0;
    }

  /* Save a General Purpose Register on stack.  */

  if ((op & 0xfc1f0003) == 0xf8010000 ||       /* std  Rx,NUM(r1) */
      (op & 0xfc1f0000) == 0xd8010000)         /* stfd Rx,NUM(r1) */
    {
      /* Rx: Only r3 - r10 are used for parameter passing.  */
      const int rx_regno = GET_SRC_REG (op);

      return (rx_regno >= 3 && rx_regno <= 10);
    }
           
  /* Save a General Purpose Register on stack via the Frame Pointer.  */

  if (framep &&
      ((op & 0xfc1f0000) == 0x901f0000 ||     /* st rx,NUM(r31) */
       (op & 0xfc1f0000) == 0x981f0000 ||     /* stb Rx,NUM(r31) */
       (op & 0xfc1f0000) == 0xd81f0000))      /* stfd Rx,NUM(r31) */
    {
      /* Rx: Usually, only r3 - r10 are used for parameter passing.
         However, the compiler sometimes uses r0 to hold an argument.  */
      const int rx_regno = GET_SRC_REG (op);

      return ((rx_regno >= 3 && rx_regno <= 10)
              || (rx_regno == 0 && *r0_contains_arg));
    }

  if ((op & 0xfc1f0000) == 0xfc010000)         /* frsp, fp?,NUM(r1) */
    {
      /* Only f2 - f8 are used for parameter passing.  */
      const int src_regno = GET_SRC_REG (op);

      return (src_regno >= 2 && src_regno <= 8);
    }

  if (framep && ((op & 0xfc1f0000) == 0xfc1f0000))  /* frsp, fp?,NUM(r31) */
    {
      /* Only f2 - f8 are used for parameter passing.  */
      const int src_regno = GET_SRC_REG (op);

      return (src_regno >= 2 && src_regno <= 8);
    }

  /* Not an insn that saves a parameter on stack.  */
  return 0;
}

static CORE_ADDR
skip_prologue (CORE_ADDR pc, CORE_ADDR lim_pc, struct rs6000_framedata *fdata)
{
  CORE_ADDR orig_pc = pc;
  CORE_ADDR last_prologue_pc = pc;
  CORE_ADDR li_found_pc = 0;
  char buf[4];
  unsigned long op;
  long offset = 0;
  long vr_saved_offset = 0;
  int lr_reg = -1;
  int cr_reg = -1;
  int vr_reg = -1;
  int ev_reg = -1;
  long ev_offset = 0;
  int vrsave_reg = -1;
  int reg;
  int framep = 0;
  int minimal_toc_loaded = 0;
  int prev_insn_was_prologue_insn = 1;
  int num_skip_non_prologue_insns = 0;
  int r0_contains_arg = 0;
  const struct bfd_arch_info *arch_info = gdbarch_bfd_arch_info (current_gdbarch);
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  
  /* Attempt to find the end of the prologue when no limit is specified.
     Note that refine_prologue_limit() has been written so that it may
     be used to "refine" the limits of non-zero PC values too, but this
     is only safe if we 1) trust the line information provided by the
     compiler and 2) iterate enough to actually find the end of the
     prologue.  
     
     It may become a good idea at some point (for both performance and
     accuracy) to unconditionally call refine_prologue_limit().  But,
     until we can make a clear determination that this is beneficial,
     we'll play it safe and only use it to obtain a limit when none
     has been specified.  */
  if (lim_pc == 0)
    lim_pc = refine_prologue_limit (pc, lim_pc);

  memset (fdata, 0, sizeof (struct rs6000_framedata));
  fdata->saved_gpr = -1;
  fdata->saved_fpr = -1;
  fdata->saved_vr = -1;
  fdata->saved_ev = -1;
  fdata->alloca_reg = -1;
  fdata->frameless = 1;
  fdata->nosavedpc = 1;

  for (;; pc += 4)
    {
      /* Sometimes it isn't clear if an instruction is a prologue
         instruction or not.  When we encounter one of these ambiguous
	 cases, we'll set prev_insn_was_prologue_insn to 0 (false).
	 Otherwise, we'll assume that it really is a prologue instruction. */
      if (prev_insn_was_prologue_insn)
	last_prologue_pc = pc;

      /* Stop scanning if we've hit the limit.  */
      if (lim_pc != 0 && pc >= lim_pc)
	break;

      prev_insn_was_prologue_insn = 1;

      /* Fetch the instruction and convert it to an integer.  */
      if (target_read_memory (pc, buf, 4))
	break;
      op = extract_signed_integer (buf, 4);

      if ((op & 0xfc1fffff) == 0x7c0802a6)
	{			/* mflr Rx */
	  /* Since shared library / PIC code, which needs to get its
	     address at runtime, can appear to save more than one link
	     register vis:

	     *INDENT-OFF*
	     stwu r1,-304(r1)
	     mflr r3
	     bl 0xff570d0 (blrl)
	     stw r30,296(r1)
	     mflr r30
	     stw r31,300(r1)
	     stw r3,308(r1);
	     ...
	     *INDENT-ON*

	     remember just the first one, but skip over additional
	     ones.  */
	  if (lr_reg < 0)
	    lr_reg = (op & 0x03e00000);
          if (lr_reg == 0)
            r0_contains_arg = 0;
	  continue;
	}
      else if ((op & 0xfc1fffff) == 0x7c000026)
	{			/* mfcr Rx */
	  cr_reg = (op & 0x03e00000);
          if (cr_reg == 0)
            r0_contains_arg = 0;
	  continue;

	}
      else if ((op & 0xfc1f0000) == 0xd8010000)
	{			/* stfd Rx,NUM(r1) */
	  reg = GET_SRC_REG (op);
	  if (fdata->saved_fpr == -1 || fdata->saved_fpr > reg)
	    {
	      fdata->saved_fpr = reg;
	      fdata->fpr_offset = SIGNED_SHORT (op) + offset;
	    }
	  continue;

	}
      else if (((op & 0xfc1f0000) == 0xbc010000) ||	/* stm Rx, NUM(r1) */
	       (((op & 0xfc1f0000) == 0x90010000 ||	/* st rx,NUM(r1) */
		 (op & 0xfc1f0003) == 0xf8010000) &&	/* std rx,NUM(r1) */
		(op & 0x03e00000) >= 0x01a00000))	/* rx >= r13 */
	{

	  reg = GET_SRC_REG (op);
	  if (fdata->saved_gpr == -1 || fdata->saved_gpr > reg)
	    {
	      fdata->saved_gpr = reg;
	      if ((op & 0xfc1f0003) == 0xf8010000)
		op &= ~3UL;
	      fdata->gpr_offset = SIGNED_SHORT (op) + offset;
	    }
	  continue;

	}
      else if ((op & 0xffff0000) == 0x60000000)
        {
	  /* nop */
	  /* Allow nops in the prologue, but do not consider them to
	     be part of the prologue unless followed by other prologue
	     instructions. */
	  prev_insn_was_prologue_insn = 0;
	  continue;

	}
      else if ((op & 0xffff0000) == 0x3c000000)
	{			/* addis 0,0,NUM, used
				   for >= 32k frames */
	  fdata->offset = (op & 0x0000ffff) << 16;
	  fdata->frameless = 0;
          r0_contains_arg = 0;
	  continue;

	}
      else if ((op & 0xffff0000) == 0x60000000)
	{			/* ori 0,0,NUM, 2nd ha
				   lf of >= 32k frames */
	  fdata->offset |= (op & 0x0000ffff);
	  fdata->frameless = 0;
          r0_contains_arg = 0;
	  continue;

	}
      else if (lr_reg != -1 &&
	       /* std Rx, NUM(r1) || stdu Rx, NUM(r1) */
	       (((op & 0xffff0000) == (lr_reg | 0xf8010000)) ||
		/* stw Rx, NUM(r1) */
		((op & 0xffff0000) == (lr_reg | 0x90010000)) ||
		/* stwu Rx, NUM(r1) */
		((op & 0xffff0000) == (lr_reg | 0x94010000))))
	{	/* where Rx == lr */
	  fdata->lr_offset = offset;
	  fdata->nosavedpc = 0;
	  lr_reg = 0;
	  if ((op & 0xfc000003) == 0xf8000000 ||	/* std */
	      (op & 0xfc000000) == 0x90000000)		/* stw */
	    {
	      /* Does not update r1, so add displacement to lr_offset.  */
	      fdata->lr_offset += SIGNED_SHORT (op);
	    }
	  continue;

	}
      else if (cr_reg != -1 &&
	       /* std Rx, NUM(r1) || stdu Rx, NUM(r1) */
	       (((op & 0xffff0000) == (cr_reg | 0xf8010000)) ||
		/* stw Rx, NUM(r1) */
		((op & 0xffff0000) == (cr_reg | 0x90010000)) ||
		/* stwu Rx, NUM(r1) */
		((op & 0xffff0000) == (cr_reg | 0x94010000))))
	{	/* where Rx == cr */
	  fdata->cr_offset = offset;
	  cr_reg = 0;
	  if ((op & 0xfc000003) == 0xf8000000 ||
	      (op & 0xfc000000) == 0x90000000)
	    {
	      /* Does not update r1, so add displacement to cr_offset.  */
	      fdata->cr_offset += SIGNED_SHORT (op);
	    }
	  continue;

	}
      else if (op == 0x48000005)
	{			/* bl .+4 used in 
				   -mrelocatable */
	  continue;

	}
      else if (op == 0x48000004)
	{			/* b .+4 (xlc) */
	  break;

	}
      else if ((op & 0xffff0000) == 0x3fc00000 ||  /* addis 30,0,foo@ha, used
						      in V.4 -mminimal-toc */
	       (op & 0xffff0000) == 0x3bde0000)
	{			/* addi 30,30,foo@l */
	  continue;

	}
      else if ((op & 0xfc000001) == 0x48000001)
	{			/* bl foo, 
				   to save fprs??? */

	  fdata->frameless = 0;
	  /* Don't skip over the subroutine call if it is not within
	     the first three instructions of the prologue.  */
	  if ((pc - orig_pc) > 8)
	    break;

	  op = read_memory_integer (pc + 4, 4);

	  /* At this point, make sure this is not a trampoline
	     function (a function that simply calls another functions,
	     and nothing else).  If the next is not a nop, this branch
	     was part of the function prologue. */

	  if (op == 0x4def7b82 || op == 0)	/* crorc 15, 15, 15 */
	    break;		/* don't skip over 
				   this branch */
	  continue;

	}
      /* update stack pointer */
      else if ((op & 0xfc1f0000) == 0x94010000)
	{		/* stu rX,NUM(r1) ||  stwu rX,NUM(r1) */
	  fdata->frameless = 0;
	  fdata->offset = SIGNED_SHORT (op);
	  offset = fdata->offset;
	  continue;
	}
      else if ((op & 0xfc1f016a) == 0x7c01016e)
	{			/* stwux rX,r1,rY */
	  /* no way to figure out what r1 is going to be */
	  fdata->frameless = 0;
	  offset = fdata->offset;
	  continue;
	}
      else if ((op & 0xfc1f0003) == 0xf8010001)
	{			/* stdu rX,NUM(r1) */
	  fdata->frameless = 0;
	  fdata->offset = SIGNED_SHORT (op & ~3UL);
	  offset = fdata->offset;
	  continue;
	}
      else if ((op & 0xfc1f016a) == 0x7c01016a)
	{			/* stdux rX,r1,rY */
	  /* no way to figure out what r1 is going to be */
	  fdata->frameless = 0;
	  offset = fdata->offset;
	  continue;
	}
      /* Load up minimal toc pointer */
      else if (((op >> 22) == 0x20f	||	/* l r31,... or l r30,... */
	       (op >> 22) == 0x3af)		/* ld r31,... or ld r30,... */
	       && !minimal_toc_loaded)
	{
	  minimal_toc_loaded = 1;
	  continue;

	  /* move parameters from argument registers to local variable
             registers */
 	}
      else if ((op & 0xfc0007fe) == 0x7c000378 &&	/* mr(.)  Rx,Ry */
               (((op >> 21) & 31) >= 3) &&              /* R3 >= Ry >= R10 */
               (((op >> 21) & 31) <= 10) &&
               ((long) ((op >> 16) & 31) >= fdata->saved_gpr)) /* Rx: local var reg */
	{
	  continue;

	  /* store parameters in stack */
	}
      /* Move parameters from argument registers to temporary register.  */
      else if (store_param_on_stack_p (op, framep, &r0_contains_arg))
        {
	  continue;

	  /* Set up frame pointer */
	}
      else if (op == 0x603f0000	/* oril r31, r1, 0x0 */
	       || op == 0x7c3f0b78)
	{			/* mr r31, r1 */
	  fdata->frameless = 0;
	  framep = 1;
	  fdata->alloca_reg = (tdep->ppc_gp0_regnum + 31);
	  continue;

	  /* Another way to set up the frame pointer.  */
	}
      else if ((op & 0xfc1fffff) == 0x38010000)
	{			/* addi rX, r1, 0x0 */
	  fdata->frameless = 0;
	  framep = 1;
	  fdata->alloca_reg = (tdep->ppc_gp0_regnum
			       + ((op & ~0x38010000) >> 21));
	  continue;
	}
      /* AltiVec related instructions.  */
      /* Store the vrsave register (spr 256) in another register for
	 later manipulation, or load a register into the vrsave
	 register.  2 instructions are used: mfvrsave and
	 mtvrsave.  They are shorthand notation for mfspr Rn, SPR256
	 and mtspr SPR256, Rn.  */
      /* mfspr Rn SPR256 == 011111 nnnnn 0000001000 01010100110
	 mtspr SPR256 Rn == 011111 nnnnn 0000001000 01110100110  */
      else if ((op & 0xfc1fffff) == 0x7c0042a6)    /* mfvrsave Rn */
	{
          vrsave_reg = GET_SRC_REG (op);
	  continue;
	}
      else if ((op & 0xfc1fffff) == 0x7c0043a6)     /* mtvrsave Rn */
        {
          continue;
        }
      /* Store the register where vrsave was saved to onto the stack:
         rS is the register where vrsave was stored in a previous
	 instruction.  */
      /* 100100 sssss 00001 dddddddd dddddddd */
      else if ((op & 0xfc1f0000) == 0x90010000)     /* stw rS, d(r1) */
        {
          if (vrsave_reg == GET_SRC_REG (op))
	    {
	      fdata->vrsave_offset = SIGNED_SHORT (op) + offset;
	      vrsave_reg = -1;
	    }
          continue;
        }
      /* Compute the new value of vrsave, by modifying the register
         where vrsave was saved to.  */
      else if (((op & 0xfc000000) == 0x64000000)    /* oris Ra, Rs, UIMM */
	       || ((op & 0xfc000000) == 0x60000000))/* ori Ra, Rs, UIMM */
	{
	  continue;
	}
      /* li r0, SIMM (short for addi r0, 0, SIMM).  This is the first
	 in a pair of insns to save the vector registers on the
	 stack.  */
      /* 001110 00000 00000 iiii iiii iiii iiii  */
      /* 001110 01110 00000 iiii iiii iiii iiii  */
      else if ((op & 0xffff0000) == 0x38000000         /* li r0, SIMM */
               || (op & 0xffff0000) == 0x39c00000)     /* li r14, SIMM */
	{
          if ((op & 0xffff0000) == 0x38000000)
            r0_contains_arg = 0;
	  li_found_pc = pc;
	  vr_saved_offset = SIGNED_SHORT (op);

          /* This insn by itself is not part of the prologue, unless
             if part of the pair of insns mentioned above. So do not
             record this insn as part of the prologue yet.  */
          prev_insn_was_prologue_insn = 0;
	}
      /* Store vector register S at (r31+r0) aligned to 16 bytes.  */      
      /* 011111 sssss 11111 00000 00111001110 */
      else if ((op & 0xfc1fffff) == 0x7c1f01ce)   /* stvx Vs, R31, R0 */
        {
	  if (pc == (li_found_pc + 4))
	    {
	      vr_reg = GET_SRC_REG (op);
	      /* If this is the first vector reg to be saved, or if
		 it has a lower number than others previously seen,
		 reupdate the frame info.  */
	      if (fdata->saved_vr == -1 || fdata->saved_vr > vr_reg)
		{
		  fdata->saved_vr = vr_reg;
		  fdata->vr_offset = vr_saved_offset + offset;
		}
	      vr_saved_offset = -1;
	      vr_reg = -1;
	      li_found_pc = 0;
	    }
	}
      /* End AltiVec related instructions.  */

      /* Start BookE related instructions.  */
      /* Store gen register S at (r31+uimm).
         Any register less than r13 is volatile, so we don't care.  */
      /* 000100 sssss 11111 iiiii 01100100001 */
      else if (arch_info->mach == bfd_mach_ppc_e500
	       && (op & 0xfc1f07ff) == 0x101f0321)    /* evstdd Rs,uimm(R31) */
	{
          if ((op & 0x03e00000) >= 0x01a00000)	/* Rs >= r13 */
	    {
              unsigned int imm;
	      ev_reg = GET_SRC_REG (op);
              imm = (op >> 11) & 0x1f;
	      ev_offset = imm * 8;
	      /* If this is the first vector reg to be saved, or if
		 it has a lower number than others previously seen,
		 reupdate the frame info.  */
	      if (fdata->saved_ev == -1 || fdata->saved_ev > ev_reg)
		{
		  fdata->saved_ev = ev_reg;
		  fdata->ev_offset = ev_offset + offset;
		}
	    }
          continue;
        }
      /* Store gen register rS at (r1+rB).  */
      /* 000100 sssss 00001 bbbbb 01100100000 */
      else if (arch_info->mach == bfd_mach_ppc_e500
	       && (op & 0xffe007ff) == 0x13e00320)     /* evstddx RS,R1,Rb */
	{
          if (pc == (li_found_pc + 4))
            {
              ev_reg = GET_SRC_REG (op);
	      /* If this is the first vector reg to be saved, or if
                 it has a lower number than others previously seen,
                 reupdate the frame info.  */
              /* We know the contents of rB from the previous instruction.  */
	      if (fdata->saved_ev == -1 || fdata->saved_ev > ev_reg)
		{
                  fdata->saved_ev = ev_reg;
                  fdata->ev_offset = vr_saved_offset + offset;
		}
	      vr_saved_offset = -1;
	      ev_reg = -1;
	      li_found_pc = 0;
            }
          continue;
        }
      /* Store gen register r31 at (rA+uimm).  */
      /* 000100 11111 aaaaa iiiii 01100100001 */
      else if (arch_info->mach == bfd_mach_ppc_e500
	       && (op & 0xffe007ff) == 0x13e00321)   /* evstdd R31,Ra,UIMM */
        {
          /* Wwe know that the source register is 31 already, but
             it can't hurt to compute it.  */
	  ev_reg = GET_SRC_REG (op);
          ev_offset = ((op >> 11) & 0x1f) * 8;
	  /* If this is the first vector reg to be saved, or if
	     it has a lower number than others previously seen,
	     reupdate the frame info.  */
	  if (fdata->saved_ev == -1 || fdata->saved_ev > ev_reg)
	    {
	      fdata->saved_ev = ev_reg;
	      fdata->ev_offset = ev_offset + offset;
	    }

	  continue;
      	}
      /* Store gen register S at (r31+r0).
         Store param on stack when offset from SP bigger than 4 bytes.  */
      /* 000100 sssss 11111 00000 01100100000 */
      else if (arch_info->mach == bfd_mach_ppc_e500
	       && (op & 0xfc1fffff) == 0x101f0320)     /* evstddx Rs,R31,R0 */
	{
          if (pc == (li_found_pc + 4))
            {
              if ((op & 0x03e00000) >= 0x01a00000)
		{
		  ev_reg = GET_SRC_REG (op);
		  /* If this is the first vector reg to be saved, or if
		     it has a lower number than others previously seen,
		     reupdate the frame info.  */
                  /* We know the contents of r0 from the previous
                     instruction.  */
		  if (fdata->saved_ev == -1 || fdata->saved_ev > ev_reg)
		    {
		      fdata->saved_ev = ev_reg;
		      fdata->ev_offset = vr_saved_offset + offset;
		    }
		  ev_reg = -1;
		}
	      vr_saved_offset = -1;
	      li_found_pc = 0;
	      continue;
            }
	}
      /* End BookE related instructions.  */

      else
	{
	  /* Not a recognized prologue instruction.
	     Handle optimizer code motions into the prologue by continuing
	     the search if we have no valid frame yet or if the return
	     address is not yet saved in the frame.  */
	  if (fdata->frameless == 0
	      && (lr_reg == -1 || fdata->nosavedpc == 0))
	    break;

	  if (op == 0x4e800020		/* blr */
	      || op == 0x4e800420)	/* bctr */
	    /* Do not scan past epilogue in frameless functions or
	       trampolines.  */
	    break;
	  if ((op & 0xf4000000) == 0x40000000) /* bxx */
	    /* Never skip branches.  */
	    break;

	  if (num_skip_non_prologue_insns++ > max_skip_non_prologue_insns)
	    /* Do not scan too many insns, scanning insns is expensive with
	       remote targets.  */
	    break;

	  /* Continue scanning.  */
	  prev_insn_was_prologue_insn = 0;
	  continue;
	}
    }

#if 0
/* I have problems with skipping over __main() that I need to address
 * sometime. Previously, I used to use misc_function_vector which
 * didn't work as well as I wanted to be.  -MGO */

  /* If the first thing after skipping a prolog is a branch to a function,
     this might be a call to an initializer in main(), introduced by gcc2.
     We'd like to skip over it as well.  Fortunately, xlc does some extra
     work before calling a function right after a prologue, thus we can
     single out such gcc2 behaviour.  */


  if ((op & 0xfc000001) == 0x48000001)
    {				/* bl foo, an initializer function? */
      op = read_memory_integer (pc + 4, 4);

      if (op == 0x4def7b82)
	{			/* cror 0xf, 0xf, 0xf (nop) */

	  /* Check and see if we are in main.  If so, skip over this
	     initializer function as well.  */

	  tmp = find_pc_misc_function (pc);
	  if (tmp >= 0
	      && strcmp (misc_function_vector[tmp].name, main_name ()) == 0)
	    return pc + 8;
	}
    }
#endif /* 0 */

  fdata->offset = -fdata->offset;
  return last_prologue_pc;
}


/*************************************************************************
  Support for creating pushing a dummy frame into the stack, and popping
  frames, etc. 
*************************************************************************/


/* All the ABI's require 16 byte alignment.  */
static CORE_ADDR
rs6000_frame_align (struct gdbarch *gdbarch, CORE_ADDR addr)
{
  return (addr & -16);
}

/* Pass the arguments in either registers, or in the stack. In RS/6000,
   the first eight words of the argument list (that might be less than
   eight parameters if some parameters occupy more than one word) are
   passed in r3..r10 registers.  float and double parameters are
   passed in fpr's, in addition to that.  Rest of the parameters if any
   are passed in user stack.  There might be cases in which half of the
   parameter is copied into registers, the other half is pushed into
   stack.

   Stack must be aligned on 64-bit boundaries when synthesizing
   function calls.

   If the function is returning a structure, then the return address is passed
   in r3, then the first 7 words of the parameters can be passed in registers,
   starting from r4.  */

static CORE_ADDR
rs6000_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
			struct regcache *regcache, CORE_ADDR bp_addr,
			int nargs, struct value **args, CORE_ADDR sp,
			int struct_return, CORE_ADDR struct_addr)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int ii;
  int len = 0;
  int argno;			/* current argument number */
  int argbytes;			/* current argument byte */
  char tmp_buffer[50];
  int f_argno = 0;		/* current floating point argno */
  int wordsize = gdbarch_tdep (current_gdbarch)->wordsize;
  CORE_ADDR func_addr = find_function_addr (function, NULL);

  struct value *arg = 0;
  struct type *type;

  CORE_ADDR saved_sp;

  /* The calling convention this function implements assumes the
     processor has floating-point registers.  We shouldn't be using it
     on PPC variants that lack them.  */
  gdb_assert (ppc_floating_point_unit_p (current_gdbarch));

  /* The first eight words of ther arguments are passed in registers.
     Copy them appropriately.  */
  ii = 0;

  /* If the function is returning a `struct', then the first word
     (which will be passed in r3) is used for struct return address.
     In that case we should advance one word and start from r4
     register to copy parameters.  */
  if (struct_return)
    {
      regcache_raw_write_unsigned (regcache, tdep->ppc_gp0_regnum + 3,
				   struct_addr);
      ii++;
    }

/* 
   effectively indirect call... gcc does...

   return_val example( float, int);

   eabi: 
   float in fp0, int in r3
   offset of stack on overflow 8/16
   for varargs, must go by type.
   power open:
   float in r3&r4, int in r5
   offset of stack on overflow different 
   both: 
   return in r3 or f0.  If no float, must study how gcc emulates floats;
   pay attention to arg promotion.  
   User may have to cast\args to handle promotion correctly 
   since gdb won't know if prototype supplied or not.
 */

  for (argno = 0, argbytes = 0; argno < nargs && ii < 8; ++ii)
    {
      int reg_size = register_size (current_gdbarch, ii + 3);

      arg = args[argno];
      type = check_typedef (VALUE_TYPE (arg));
      len = TYPE_LENGTH (type);

      if (TYPE_CODE (type) == TYPE_CODE_FLT)
	{

	  /* Floating point arguments are passed in fpr's, as well as gpr's.
	     There are 13 fpr's reserved for passing parameters. At this point
	     there is no way we would run out of them.  */

	  gdb_assert (len <= 8);

	  regcache_cooked_write (regcache,
	                         tdep->ppc_fp0_regnum + 1 + f_argno,
	                         VALUE_CONTENTS (arg));
	  ++f_argno;
	}

      if (len > reg_size)
	{

	  /* Argument takes more than one register.  */
	  while (argbytes < len)
	    {
	      char word[MAX_REGISTER_SIZE];
	      memset (word, 0, reg_size);
	      memcpy (word,
		      ((char *) VALUE_CONTENTS (arg)) + argbytes,
		      (len - argbytes) > reg_size
		        ? reg_size : len - argbytes);
	      regcache_cooked_write (regcache,
	                            tdep->ppc_gp0_regnum + 3 + ii,
				    word);
	      ++ii, argbytes += reg_size;

	      if (ii >= 8)
		goto ran_out_of_registers_for_arguments;
	    }
	  argbytes = 0;
	  --ii;
	}
      else
	{
	  /* Argument can fit in one register.  No problem.  */
	  int adj = TARGET_BYTE_ORDER == BFD_ENDIAN_BIG ? reg_size - len : 0;
	  char word[MAX_REGISTER_SIZE];

	  memset (word, 0, reg_size);
	  memcpy (word, VALUE_CONTENTS (arg), len);
	  regcache_cooked_write (regcache, tdep->ppc_gp0_regnum + 3 +ii, word);
	}
      ++argno;
    }

ran_out_of_registers_for_arguments:

  saved_sp = read_sp ();

  /* Location for 8 parameters are always reserved.  */
  sp -= wordsize * 8;

  /* Another six words for back chain, TOC register, link register, etc.  */
  sp -= wordsize * 6;

  /* Stack pointer must be quadword aligned.  */
  sp &= -16;

  /* If there are more arguments, allocate space for them in 
     the stack, then push them starting from the ninth one.  */

  if ((argno < nargs) || argbytes)
    {
      int space = 0, jj;

      if (argbytes)
	{
	  space += ((len - argbytes + 3) & -4);
	  jj = argno + 1;
	}
      else
	jj = argno;

      for (; jj < nargs; ++jj)
	{
	  struct value *val = args[jj];
	  space += ((TYPE_LENGTH (VALUE_TYPE (val))) + 3) & -4;
	}

      /* Add location required for the rest of the parameters.  */
      space = (space + 15) & -16;
      sp -= space;

      /* This is another instance we need to be concerned about
         securing our stack space. If we write anything underneath %sp
         (r1), we might conflict with the kernel who thinks he is free
         to use this area.  So, update %sp first before doing anything
         else.  */

      regcache_raw_write_signed (regcache, SP_REGNUM, sp);

      /* If the last argument copied into the registers didn't fit there 
         completely, push the rest of it into stack.  */

      if (argbytes)
	{
	  write_memory (sp + 24 + (ii * 4),
			((char *) VALUE_CONTENTS (arg)) + argbytes,
			len - argbytes);
	  ++argno;
	  ii += ((len - argbytes + 3) & -4) / 4;
	}

      /* Push the rest of the arguments into stack.  */
      for (; argno < nargs; ++argno)
	{

	  arg = args[argno];
	  type = check_typedef (VALUE_TYPE (arg));
	  len = TYPE_LENGTH (type);


	  /* Float types should be passed in fpr's, as well as in the
             stack.  */
	  if (TYPE_CODE (type) == TYPE_CODE_FLT && f_argno < 13)
	    {

	      gdb_assert (len <= 8);

	      regcache_cooked_write (regcache,
				     tdep->ppc_fp0_regnum + 1 + f_argno,
				     VALUE_CONTENTS (arg));
	      ++f_argno;
	    }

	  write_memory (sp + 24 + (ii * 4),
                        (char *) VALUE_CONTENTS (arg),
                        len);
	  ii += ((len + 3) & -4) / 4;
	}
    }

  /* Set the stack pointer.  According to the ABI, the SP is meant to
     be set _before_ the corresponding stack space is used.  On AIX,
     this even applies when the target has been completely stopped!
     Not doing this can lead to conflicts with the kernel which thinks
     that it still has control over this not-yet-allocated stack
     region.  */
  regcache_raw_write_signed (regcache, SP_REGNUM, sp);

  /* Set back chain properly.  */
  store_unsigned_integer (tmp_buffer, 4, saved_sp);
  write_memory (sp, tmp_buffer, 4);

  /* Point the inferior function call's return address at the dummy's
     breakpoint.  */
  regcache_raw_write_signed (regcache, tdep->ppc_lr_regnum, bp_addr);

  /* Set the TOC register, get the value from the objfile reader
     which, in turn, gets it from the VMAP table.  */
  if (rs6000_find_toc_address_hook != NULL)
    {
      CORE_ADDR tocvalue = (*rs6000_find_toc_address_hook) (func_addr);
      regcache_raw_write_signed (regcache, tdep->ppc_toc_regnum, tocvalue);
    }

  target_store_registers (-1);
  return sp;
}

/* PowerOpen always puts structures in memory.  Vectors, which were
   added later, do get returned in a register though.  */

static int     
rs6000_use_struct_convention (int gcc_p, struct type *value_type)
{  
  if ((TYPE_LENGTH (value_type) == 16 || TYPE_LENGTH (value_type) == 8)
      && TYPE_VECTOR (value_type))
    return 0;                            
  return 1;
}

static void
rs6000_extract_return_value (struct type *valtype, char *regbuf, char *valbuf)
{
  int offset = 0;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  /* The calling convention this function implements assumes the
     processor has floating-point registers.  We shouldn't be using it
     on PPC variants that lack them.  */
  gdb_assert (ppc_floating_point_unit_p (current_gdbarch));

  if (TYPE_CODE (valtype) == TYPE_CODE_FLT)
    {

      /* floats and doubles are returned in fpr1. fpr's have a size of 8 bytes.
         We need to truncate the return value into float size (4 byte) if
         necessary.  */

      convert_typed_floating (&regbuf[DEPRECATED_REGISTER_BYTE
                                      (tdep->ppc_fp0_regnum + 1)],
                              builtin_type_double,
                              valbuf,
                              valtype);
    }
  else if (TYPE_CODE (valtype) == TYPE_CODE_ARRAY
           && TYPE_LENGTH (valtype) == 16
           && TYPE_VECTOR (valtype))
    {
      memcpy (valbuf, regbuf + DEPRECATED_REGISTER_BYTE (tdep->ppc_vr0_regnum + 2),
	      TYPE_LENGTH (valtype));
    }
  else
    {
      /* return value is copied starting from r3. */
      if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG
	  && TYPE_LENGTH (valtype) < register_size (current_gdbarch, 3))
	offset = register_size (current_gdbarch, 3) - TYPE_LENGTH (valtype);

      memcpy (valbuf,
	      regbuf + DEPRECATED_REGISTER_BYTE (3) + offset,
	      TYPE_LENGTH (valtype));
    }
}

/* Return whether handle_inferior_event() should proceed through code
   starting at PC in function NAME when stepping.

   The AIX -bbigtoc linker option generates functions @FIX0, @FIX1, etc. to
   handle memory references that are too distant to fit in instructions
   generated by the compiler.  For example, if 'foo' in the following
   instruction:

     lwz r9,foo(r2)

   is greater than 32767, the linker might replace the lwz with a branch to
   somewhere in @FIX1 that does the load in 2 instructions and then branches
   back to where execution should continue.

   GDB should silently step over @FIX code, just like AIX dbx does.
   Unfortunately, the linker uses the "b" instruction for the branches,
   meaning that the link register doesn't get set.  Therefore, GDB's usual
   step_over_function() mechanism won't work.

   Instead, use the IN_SOLIB_RETURN_TRAMPOLINE and SKIP_TRAMPOLINE_CODE hooks
   in handle_inferior_event() to skip past @FIX code.  */

int
rs6000_in_solib_return_trampoline (CORE_ADDR pc, char *name)
{
  return name && !strncmp (name, "@FIX", 4);
}

/* Skip code that the user doesn't want to see when stepping:

   1. Indirect function calls use a piece of trampoline code to do context
   switching, i.e. to set the new TOC table.  Skip such code if we are on
   its first instruction (as when we have single-stepped to here).

   2. Skip shared library trampoline code (which is different from
   indirect function call trampolines).

   3. Skip bigtoc fixup code.

   Result is desired PC to step until, or NULL if we are not in
   code that should be skipped.  */

CORE_ADDR
rs6000_skip_trampoline_code (CORE_ADDR pc)
{
  unsigned int ii, op;
  int rel;
  CORE_ADDR solib_target_pc;
  struct minimal_symbol *msymbol;

  static unsigned trampoline_code[] =
  {
    0x800b0000,			/*     l   r0,0x0(r11)  */
    0x90410014,			/*    st   r2,0x14(r1)  */
    0x7c0903a6,			/* mtctr   r0           */
    0x804b0004,			/*     l   r2,0x4(r11)  */
    0x816b0008,			/*     l  r11,0x8(r11)  */
    0x4e800420,			/*  bctr                */
    0x4e800020,			/*    br                */
    0
  };

  /* Check for bigtoc fixup code.  */
  msymbol = lookup_minimal_symbol_by_pc (pc);
  if (msymbol && rs6000_in_solib_return_trampoline (pc, DEPRECATED_SYMBOL_NAME (msymbol)))
    {
      /* Double-check that the third instruction from PC is relative "b".  */
      op = read_memory_integer (pc + 8, 4);
      if ((op & 0xfc000003) == 0x48000000)
	{
	  /* Extract bits 6-29 as a signed 24-bit relative word address and
	     add it to the containing PC.  */
	  rel = ((int)(op << 6) >> 6);
	  return pc + 8 + rel;
	}
    }

  /* If pc is in a shared library trampoline, return its target.  */
  solib_target_pc = find_solib_trampoline_target (pc);
  if (solib_target_pc)
    return solib_target_pc;

  for (ii = 0; trampoline_code[ii]; ++ii)
    {
      op = read_memory_integer (pc + (ii * 4), 4);
      if (op != trampoline_code[ii])
	return 0;
    }
  ii = read_register (11);	/* r11 holds destination addr   */
  pc = read_memory_addr (ii, gdbarch_tdep (current_gdbarch)->wordsize); /* (r11) value */
  return pc;
}

/* Return the size of register REG when words are WORDSIZE bytes long.  If REG
   isn't available with that word size, return 0.  */

static int
regsize (const struct reg *reg, int wordsize)
{
  return wordsize == 8 ? reg->sz64 : reg->sz32;
}

/* Return the name of register number N, or null if no such register exists
   in the current architecture.  */

static const char *
rs6000_register_name (int n)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  const struct reg *reg = tdep->regs + n;

  if (!regsize (reg, tdep->wordsize))
    return NULL;
  return reg->name;
}

/* Return the GDB type object for the "standard" data type
   of data in register N.  */

static struct type *
rs6000_register_type (struct gdbarch *gdbarch, int n)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  const struct reg *reg = tdep->regs + n;

  if (reg->fpr)
    return builtin_type_double;
  else
    {
      int size = regsize (reg, tdep->wordsize);
      switch (size)
	{
	case 0:
	  return builtin_type_int0;
	case 4:
	  return builtin_type_uint32;
	case 8:
	  if (tdep->ppc_ev0_regnum <= n && n <= tdep->ppc_ev31_regnum)
	    return builtin_type_vec64;
	  else
	    return builtin_type_uint64;
	  break;
	case 16:
	  return builtin_type_vec128;
	  break;
	default:
	  internal_error (__FILE__, __LINE__, "Register %d size %d unknown",
			  n, size);
	}
    }
}

/* The register format for RS/6000 floating point registers is always
   double, we need a conversion if the memory format is float.  */

static int
rs6000_convert_register_p (int regnum, struct type *type)
{
  const struct reg *reg = gdbarch_tdep (current_gdbarch)->regs + regnum;
  
  return (reg->fpr
          && TYPE_CODE (type) == TYPE_CODE_FLT
          && TYPE_LENGTH (type) != TYPE_LENGTH (builtin_type_double));
}

static void
rs6000_register_to_value (struct frame_info *frame,
                          int regnum,
                          struct type *type,
                          void *to)
{
  const struct reg *reg = gdbarch_tdep (current_gdbarch)->regs + regnum;
  char from[MAX_REGISTER_SIZE];
  
  gdb_assert (reg->fpr);
  gdb_assert (TYPE_CODE (type) == TYPE_CODE_FLT);

  get_frame_register (frame, regnum, from);
  convert_typed_floating (from, builtin_type_double, to, type);
}

static void
rs6000_value_to_register (struct frame_info *frame,
                          int regnum,
                          struct type *type,
                          const void *from)
{
  const struct reg *reg = gdbarch_tdep (current_gdbarch)->regs + regnum;
  char to[MAX_REGISTER_SIZE];

  gdb_assert (reg->fpr);
  gdb_assert (TYPE_CODE (type) == TYPE_CODE_FLT);

  convert_typed_floating (from, type, to, builtin_type_double);
  put_frame_register (frame, regnum, to);
}

/* Move SPE vector register values between a 64-bit buffer and the two
   32-bit raw register halves in a regcache.  This function handles
   both splitting a 64-bit value into two 32-bit halves, and joining
   two halves into a whole 64-bit value, depending on the function
   passed as the MOVE argument.

   EV_REG must be the number of an SPE evN vector register --- a
   pseudoregister.  REGCACHE must be a regcache, and BUFFER must be a
   64-bit buffer.

   Call MOVE once for each 32-bit half of that register, passing
   REGCACHE, the number of the raw register corresponding to that
   half, and the address of the appropriate half of BUFFER.

   For example, passing 'regcache_raw_read' as the MOVE function will
   fill BUFFER with the full 64-bit contents of EV_REG.  Or, passing
   'regcache_raw_supply' will supply the contents of BUFFER to the
   appropriate pair of raw registers in REGCACHE.

   You may need to cast away some 'const' qualifiers when passing
   MOVE, since this function can't tell at compile-time which of
   REGCACHE or BUFFER is acting as the source of the data.  If C had
   co-variant type qualifiers, ...  */
static void
e500_move_ev_register (void (*move) (struct regcache *regcache,
                                     int regnum, void *buf),
                       struct regcache *regcache, int ev_reg,
                       void *buffer)
{
  struct gdbarch *arch = get_regcache_arch (regcache);
  struct gdbarch_tdep *tdep = gdbarch_tdep (arch); 
  int reg_index;
  char *byte_buffer = buffer;

  gdb_assert (tdep->ppc_ev0_regnum <= ev_reg
              && ev_reg < tdep->ppc_ev0_regnum + ppc_num_gprs);

  reg_index = ev_reg - tdep->ppc_ev0_regnum;

  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
    {
      move (regcache, tdep->ppc_ev0_upper_regnum + reg_index, byte_buffer);
      move (regcache, tdep->ppc_gp0_regnum + reg_index, byte_buffer + 4);
    }
  else
    {
      move (regcache, tdep->ppc_gp0_regnum + reg_index, byte_buffer);
      move (regcache, tdep->ppc_ev0_upper_regnum + reg_index, byte_buffer + 4);
    }
}

static void
e500_pseudo_register_read (struct gdbarch *gdbarch, struct regcache *regcache,
			   int reg_nr, void *buffer)
{
  struct gdbarch *regcache_arch = get_regcache_arch (regcache);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch); 

  gdb_assert (regcache_arch == gdbarch);
 
  if (tdep->ppc_ev0_regnum <= reg_nr
      && reg_nr < tdep->ppc_ev0_regnum + ppc_num_gprs)
    e500_move_ev_register (regcache_raw_read, regcache, reg_nr, buffer);
  else
    internal_error (__FILE__, __LINE__,
                    "e500_pseudo_register_read: "
                    "called on unexpected register '%s' (%d)",
                    gdbarch_register_name (gdbarch, reg_nr), reg_nr);
}

static void
e500_pseudo_register_write (struct gdbarch *gdbarch, struct regcache *regcache,
			    int reg_nr, const void *buffer)
{
  struct gdbarch *regcache_arch = get_regcache_arch (regcache);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch); 

  gdb_assert (regcache_arch == gdbarch);
 
  if (tdep->ppc_ev0_regnum <= reg_nr
      && reg_nr < tdep->ppc_ev0_regnum + ppc_num_gprs)
    e500_move_ev_register ((void (*) (struct regcache *, int, void *))
                           regcache_raw_write,
                           regcache, reg_nr, (void *) buffer);
  else
    internal_error (__FILE__, __LINE__,
                    "e500_pseudo_register_read: "
                    "called on unexpected register '%s' (%d)",
                    gdbarch_register_name (gdbarch, reg_nr), reg_nr);
}

/* The E500 needs a custom reggroup function: it has anonymous raw
   registers, and default_register_reggroup_p assumes that anonymous
   registers are not members of any reggroup.  */
static int
e500_register_reggroup_p (struct gdbarch *gdbarch,
                          int regnum,
                          struct reggroup *group)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* The save and restore register groups need to include the
     upper-half registers, even though they're anonymous.  */
  if ((group == save_reggroup
       || group == restore_reggroup)
      && (tdep->ppc_ev0_upper_regnum <= regnum
          && regnum < tdep->ppc_ev0_upper_regnum + ppc_num_gprs))
    return 1;

  /* In all other regards, the default reggroup definition is fine.  */
  return default_register_reggroup_p (gdbarch, regnum, group);
}

/* Convert a DBX STABS register number to a GDB register number.  */
static int
rs6000_stab_reg_to_regnum (int num)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if (0 <= num && num <= 31)
    return tdep->ppc_gp0_regnum + num;
  else if (32 <= num && num <= 63)
    /* FIXME: jimb/2004-05-05: What should we do when the debug info
       specifies registers the architecture doesn't have?  Our
       callers don't check the value we return.  */
    return tdep->ppc_fp0_regnum + (num - 32);
  else if (77 <= num && num <= 108)
    return tdep->ppc_vr0_regnum + (num - 77);
  else if (1200 <= num && num < 1200 + 32)
    return tdep->ppc_ev0_regnum + (num - 1200);
  else
    switch (num)
      {
      case 64: 
        return tdep->ppc_mq_regnum;
      case 65:
        return tdep->ppc_lr_regnum;
      case 66: 
        return tdep->ppc_ctr_regnum;
      case 76: 
        return tdep->ppc_xer_regnum;
      case 109:
        return tdep->ppc_vrsave_regnum;
      case 110:
        return tdep->ppc_vrsave_regnum - 1; /* vscr */
      case 111:
        return tdep->ppc_acc_regnum;
      case 112:
        return tdep->ppc_spefscr_regnum;
      default: 
        return num;
      }
}


/* Convert a Dwarf 2 register number to a GDB register number.  */
static int
rs6000_dwarf2_reg_to_regnum (int num)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if (0 <= num && num <= 31)
    return tdep->ppc_gp0_regnum + num;
  else if (32 <= num && num <= 63)
    /* FIXME: jimb/2004-05-05: What should we do when the debug info
       specifies registers the architecture doesn't have?  Our
       callers don't check the value we return.  */
    return tdep->ppc_fp0_regnum + (num - 32);
  else if (1124 <= num && num < 1124 + 32)
    return tdep->ppc_vr0_regnum + (num - 1124);
  else if (1200 <= num && num < 1200 + 32)
    return tdep->ppc_ev0_regnum + (num - 1200);
  else
    switch (num)
      {
      case 67:
        return tdep->ppc_vrsave_regnum - 1; /* vscr */
      case 99:
        return tdep->ppc_acc_regnum;
      case 100:
        return tdep->ppc_mq_regnum;
      case 101:
        return tdep->ppc_xer_regnum;
      case 108:
        return tdep->ppc_lr_regnum;
      case 109:
        return tdep->ppc_ctr_regnum;
      case 356:
        return tdep->ppc_vrsave_regnum;
      case 612:
        return tdep->ppc_spefscr_regnum;
      default:
        return num;
      }
}


static void
rs6000_store_return_value (struct type *type,
                           struct regcache *regcache,
                           const void *valbuf)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  int regnum = -1;

  /* The calling convention this function implements assumes the
     processor has floating-point registers.  We shouldn't be using it
     on PPC variants that lack them.  */
  gdb_assert (ppc_floating_point_unit_p (gdbarch));

  if (TYPE_CODE (type) == TYPE_CODE_FLT)
    /* Floating point values are returned starting from FPR1 and up.
       Say a double_double_double type could be returned in
       FPR1/FPR2/FPR3 triple.  */
    regnum = tdep->ppc_fp0_regnum + 1;
  else if (TYPE_CODE (type) == TYPE_CODE_ARRAY)
    {
      if (TYPE_LENGTH (type) == 16
          && TYPE_VECTOR (type))
        regnum = tdep->ppc_vr0_regnum + 2;
      else
        internal_error (__FILE__, __LINE__,
                        "rs6000_store_return_value: "
                        "unexpected array return type");
    }
  else
    /* Everything else is returned in GPR3 and up.  */
    regnum = tdep->ppc_gp0_regnum + 3;

  {
    size_t bytes_written = 0;

    while (bytes_written < TYPE_LENGTH (type))
      {
        /* How much of this value can we write to this register?  */
        size_t bytes_to_write = min (TYPE_LENGTH (type) - bytes_written,
                                     register_size (gdbarch, regnum));
        regcache_cooked_write_part (regcache, regnum,
                                    0, bytes_to_write,
                                    (char *) valbuf + bytes_written);
        regnum++;
        bytes_written += bytes_to_write;
      }
  }
}


/* Extract from an array REGBUF containing the (raw) register state
   the address in which a function should return its structure value,
   as a CORE_ADDR (or an expression that can be used as one).  */

static CORE_ADDR
rs6000_extract_struct_value_address (struct regcache *regcache)
{
  /* FIXME: cagney/2002-09-26: PR gdb/724: When making an inferior
     function call GDB knows the address of the struct return value
     and hence, should not need to call this function.  Unfortunately,
     the current call_function_by_hand() code only saves the most
     recent struct address leading to occasional calls.  The code
     should instead maintain a stack of such addresses (in the dummy
     frame object).  */
  /* NOTE: cagney/2002-09-26: Return 0 which indicates that we've
     really got no idea where the return value is being stored.  While
     r3, on function entry, contained the address it will have since
     been reused (scratch) and hence wouldn't be valid */
  return 0;
}

/* Hook called when a new child process is started.  */

void
rs6000_create_inferior (int pid)
{
  if (rs6000_set_host_arch_hook)
    rs6000_set_host_arch_hook (pid);
}

/* Support for CONVERT_FROM_FUNC_PTR_ADDR (ARCH, ADDR, TARG).

   Usually a function pointer's representation is simply the address
   of the function. On the RS/6000 however, a function pointer is
   represented by a pointer to a TOC entry. This TOC entry contains
   three words, the first word is the address of the function, the
   second word is the TOC pointer (r2), and the third word is the
   static chain value.  Throughout GDB it is currently assumed that a
   function pointer contains the address of the function, which is not
   easy to fix.  In addition, the conversion of a function address to
   a function pointer would require allocation of a TOC entry in the
   inferior's memory space, with all its drawbacks.  To be able to
   call C++ virtual methods in the inferior (which are called via
   function pointers), find_function_addr uses this function to get the
   function address from a function pointer.  */

/* Return real function address if ADDR (a function pointer) is in the data
   space and is therefore a special function pointer.  */

static CORE_ADDR
rs6000_convert_from_func_ptr_addr (struct gdbarch *gdbarch,
				   CORE_ADDR addr,
				   struct target_ops *targ)
{
  struct obj_section *s;

  s = find_pc_section (addr);
  if (s && s->the_bfd_section->flags & SEC_CODE)
    return addr;

  /* ADDR is in the data space, so it's a special function pointer. */
  return read_memory_addr (addr, gdbarch_tdep (current_gdbarch)->wordsize);
}


/* Handling the various POWER/PowerPC variants.  */


/* The arrays here called registers_MUMBLE hold information about available
   registers.

   For each family of PPC variants, I've tried to isolate out the
   common registers and put them up front, so that as long as you get
   the general family right, GDB will correctly identify the registers
   common to that family.  The common register sets are:

   For the 60x family: hid0 hid1 iabr dabr pir

   For the 505 and 860 family: eie eid nri

   For the 403 and 403GC: icdbdr esr dear evpr cdbcr tsr tcr pit tbhi
   tblo srr2 srr3 dbsr dbcr iac1 iac2 dac1 dac2 dccr iccr pbl1
   pbu1 pbl2 pbu2

   Most of these register groups aren't anything formal.  I arrived at
   them by looking at the registers that occurred in more than one
   processor.
   
   Note: kevinb/2002-04-30: Support for the fpscr register was added
   during April, 2002.  Slot 70 is being used for PowerPC and slot 71
   for Power.  For PowerPC, slot 70 was unused and was already in the
   PPC_UISA_SPRS which is ideally where fpscr should go.  For Power,
   slot 70 was being used for "mq", so the next available slot (71)
   was chosen.  It would have been nice to be able to make the
   register numbers the same across processor cores, but this wasn't
   possible without either 1) renumbering some registers for some
   processors or 2) assigning fpscr to a really high slot that's
   larger than any current register number.  Doing (1) is bad because
   existing stubs would break.  Doing (2) is undesirable because it
   would introduce a really large gap between fpscr and the rest of
   the registers for most processors.  */

/* Convenience macros for populating register arrays.  */

/* Within another macro, convert S to a string.  */

#define STR(s)	#s

/* Return a struct reg defining register NAME that's 32 bits on 32-bit systems
   and 64 bits on 64-bit systems.  */
#define R(name)		{ STR(name), 4, 8, 0, 0, -1 }

/* Return a struct reg defining register NAME that's 32 bits on all
   systems.  */
#define R4(name)	{ STR(name), 4, 4, 0, 0, -1 }

/* Return a struct reg defining register NAME that's 64 bits on all
   systems.  */
#define R8(name)	{ STR(name), 8, 8, 0, 0, -1 }

/* Return a struct reg defining register NAME that's 128 bits on all
   systems.  */
#define R16(name)       { STR(name), 16, 16, 0, 0, -1 }

/* Return a struct reg defining floating-point register NAME.  */
#define F(name)		{ STR(name), 8, 8, 1, 0, -1 }

/* Return a struct reg defining a pseudo register NAME that is 64 bits
   long on all systems.  */
#define P8(name)	{ STR(name), 8, 8, 0, 1, -1 }

/* Return a struct reg defining register NAME that's 32 bits on 32-bit
   systems and that doesn't exist on 64-bit systems.  */
#define R32(name)	{ STR(name), 4, 0, 0, 0, -1 }

/* Return a struct reg defining register NAME that's 64 bits on 64-bit
   systems and that doesn't exist on 32-bit systems.  */
#define R64(name)	{ STR(name), 0, 8, 0, 0, -1 }

/* Return a struct reg placeholder for a register that doesn't exist.  */
#define R0		{ 0, 0, 0, 0, 0, -1 }

/* Return a struct reg defining an anonymous raw register that's 32
   bits on all systems.  */
#define A4              { 0, 4, 4, 0, 0, -1 }

/* Return a struct reg defining an SPR named NAME that is 32 bits on
   32-bit systems and 64 bits on 64-bit systems.  */
#define S(name)         { STR(name), 4, 8, 0, 0, ppc_spr_ ## name }
  
/* Return a struct reg defining an SPR named NAME that is 32 bits on
   all systems.  */
#define S4(name)        { STR(name), 4, 4, 0, 0, ppc_spr_ ## name }
  
/* Return a struct reg defining an SPR named NAME that is 32 bits on
   all systems, and whose SPR number is NUMBER.  */
#define SN4(name, number) { STR(name), 4, 4, 0, 0, (number) }
  
/* Return a struct reg defining an SPR named NAME that's 64 bits on
   64-bit systems and that doesn't exist on 32-bit systems.  */
#define S64(name)       { STR(name), 0, 8, 0, 0, ppc_spr_ ## name }
  
/* UISA registers common across all architectures, including POWER.  */

#define COMMON_UISA_REGS \
  /*  0 */ R(r0), R(r1), R(r2), R(r3), R(r4), R(r5), R(r6), R(r7),  \
  /*  8 */ R(r8), R(r9), R(r10),R(r11),R(r12),R(r13),R(r14),R(r15), \
  /* 16 */ R(r16),R(r17),R(r18),R(r19),R(r20),R(r21),R(r22),R(r23), \
  /* 24 */ R(r24),R(r25),R(r26),R(r27),R(r28),R(r29),R(r30),R(r31), \
  /* 32 */ F(f0), F(f1), F(f2), F(f3), F(f4), F(f5), F(f6), F(f7),  \
  /* 40 */ F(f8), F(f9), F(f10),F(f11),F(f12),F(f13),F(f14),F(f15), \
  /* 48 */ F(f16),F(f17),F(f18),F(f19),F(f20),F(f21),F(f22),F(f23), \
  /* 56 */ F(f24),F(f25),F(f26),F(f27),F(f28),F(f29),F(f30),F(f31), \
  /* 64 */ R(pc), R(ps)

/* UISA-level SPRs for PowerPC.  */
#define PPC_UISA_SPRS \
  /* 66 */ R4(cr),  S(lr), S(ctr), S4(xer), R4(fpscr)

/* UISA-level SPRs for PowerPC without floating point support.  */
#define PPC_UISA_NOFP_SPRS \
  /* 66 */ R4(cr),  S(lr), S(ctr), S4(xer), R0

/* Segment registers, for PowerPC.  */
#define PPC_SEGMENT_REGS \
  /* 71 */ R32(sr0),  R32(sr1),  R32(sr2),  R32(sr3),  \
  /* 75 */ R32(sr4),  R32(sr5),  R32(sr6),  R32(sr7),  \
  /* 79 */ R32(sr8),  R32(sr9),  R32(sr10), R32(sr11), \
  /* 83 */ R32(sr12), R32(sr13), R32(sr14), R32(sr15)

/* OEA SPRs for PowerPC.  */
#define PPC_OEA_SPRS \
  /*  87 */ S4(pvr), \
  /*  88 */ S(ibat0u), S(ibat0l), S(ibat1u), S(ibat1l), \
  /*  92 */ S(ibat2u), S(ibat2l), S(ibat3u), S(ibat3l), \
  /*  96 */ S(dbat0u), S(dbat0l), S(dbat1u), S(dbat1l), \
  /* 100 */ S(dbat2u), S(dbat2l), S(dbat3u), S(dbat3l), \
  /* 104 */ S(sdr1),   S64(asr),  S(dar),    S4(dsisr), \
  /* 108 */ S(sprg0),  S(sprg1),  S(sprg2),  S(sprg3),  \
  /* 112 */ S(srr0),   S(srr1),   S(tbl),    S(tbu),    \
  /* 116 */ S4(dec),   S(dabr),   S4(ear)

/* AltiVec registers.  */
#define PPC_ALTIVEC_REGS \
  /*119*/R16(vr0), R16(vr1), R16(vr2), R16(vr3), R16(vr4), R16(vr5), R16(vr6), R16(vr7),  \
  /*127*/R16(vr8), R16(vr9), R16(vr10),R16(vr11),R16(vr12),R16(vr13),R16(vr14),R16(vr15), \
  /*135*/R16(vr16),R16(vr17),R16(vr18),R16(vr19),R16(vr20),R16(vr21),R16(vr22),R16(vr23), \
  /*143*/R16(vr24),R16(vr25),R16(vr26),R16(vr27),R16(vr28),R16(vr29),R16(vr30),R16(vr31), \
  /*151*/R4(vscr), R4(vrsave)


/* On machines supporting the SPE APU, the general-purpose registers
   are 64 bits long.  There are SIMD vector instructions to treat them
   as pairs of floats, but the rest of the instruction set treats them
   as 32-bit registers, and only operates on their lower halves.

   In the GDB regcache, we treat their high and low halves as separate
   registers.  The low halves we present as the general-purpose
   registers, and then we have pseudo-registers that stitch together
   the upper and lower halves and present them as pseudo-registers.  */

/* SPE GPR lower halves --- raw registers.  */
#define PPC_SPE_GP_REGS \
  /*  0 */ R4(r0), R4(r1), R4(r2), R4(r3), R4(r4), R4(r5), R4(r6), R4(r7),  \
  /*  8 */ R4(r8), R4(r9), R4(r10),R4(r11),R4(r12),R4(r13),R4(r14),R4(r15), \
  /* 16 */ R4(r16),R4(r17),R4(r18),R4(r19),R4(r20),R4(r21),R4(r22),R4(r23), \
  /* 24 */ R4(r24),R4(r25),R4(r26),R4(r27),R4(r28),R4(r29),R4(r30),R4(r31)

/* SPE GPR upper halves --- anonymous raw registers.  */
#define PPC_SPE_UPPER_GP_REGS                   \
  /*  0 */ A4, A4, A4, A4, A4, A4, A4, A4,      \
  /*  8 */ A4, A4, A4, A4, A4, A4, A4, A4,      \
  /* 16 */ A4, A4, A4, A4, A4, A4, A4, A4,      \
  /* 24 */ A4, A4, A4, A4, A4, A4, A4, A4

/* SPE GPR vector registers --- pseudo registers based on underlying
   gprs and the anonymous upper half raw registers.  */
#define PPC_EV_PSEUDO_REGS \
/* 0*/P8(ev0), P8(ev1), P8(ev2), P8(ev3), P8(ev4), P8(ev5), P8(ev6), P8(ev7), \
/* 8*/P8(ev8), P8(ev9), P8(ev10),P8(ev11),P8(ev12),P8(ev13),P8(ev14),P8(ev15),\
/*16*/P8(ev16),P8(ev17),P8(ev18),P8(ev19),P8(ev20),P8(ev21),P8(ev22),P8(ev23),\
/*24*/P8(ev24),P8(ev25),P8(ev26),P8(ev27),P8(ev28),P8(ev29),P8(ev30),P8(ev31)

/* IBM POWER (pre-PowerPC) architecture, user-level view.  We only cover
   user-level SPR's.  */
static const struct reg registers_power[] =
{
  COMMON_UISA_REGS,
  /* 66 */ R4(cnd), S(lr), S(cnt), S4(xer), S4(mq),
  /* 71 */ R4(fpscr)
};

/* PowerPC UISA - a PPC processor as viewed by user-level code.  A UISA-only
   view of the PowerPC.  */
static const struct reg registers_powerpc[] =
{
  COMMON_UISA_REGS,
  PPC_UISA_SPRS,
  PPC_ALTIVEC_REGS
};

/* IBM PowerPC 403.

   Some notes about the "tcr" special-purpose register:
   - On the 403 and 403GC, SPR 986 is named "tcr", and it controls the
     403's programmable interval timer, fixed interval timer, and
     watchdog timer.
   - On the 602, SPR 984 is named "tcr", and it controls the 602's
     watchdog timer, and nothing else.

   Some of the fields are similar between the two, but they're not
   compatible with each other.  Since the two variants have different
   registers, with different numbers, but the same name, we can't
   splice the register name to get the SPR number.  */
static const struct reg registers_403[] =
{
  COMMON_UISA_REGS,
  PPC_UISA_SPRS,
  PPC_SEGMENT_REGS,
  PPC_OEA_SPRS,
  /* 119 */ S(icdbdr), S(esr),  S(dear), S(evpr),
  /* 123 */ S(cdbcr),  S(tsr),  SN4(tcr, ppc_spr_403_tcr), S(pit),
  /* 127 */ S(tbhi),   S(tblo), S(srr2), S(srr3),
  /* 131 */ S(dbsr),   S(dbcr), S(iac1), S(iac2),
  /* 135 */ S(dac1),   S(dac2), S(dccr), S(iccr),
  /* 139 */ S(pbl1),   S(pbu1), S(pbl2), S(pbu2)
};

/* IBM PowerPC 403GC.
   See the comments about 'tcr' for the 403, above.  */
static const struct reg registers_403GC[] =
{
  COMMON_UISA_REGS,
  PPC_UISA_SPRS,
  PPC_SEGMENT_REGS,
  PPC_OEA_SPRS,
  /* 119 */ S(icdbdr), S(esr),  S(dear), S(evpr),
  /* 123 */ S(cdbcr),  S(tsr),  SN4(tcr, ppc_spr_403_tcr), S(pit),
  /* 127 */ S(tbhi),   S(tblo), S(srr2), S(srr3),
  /* 131 */ S(dbsr),   S(dbcr), S(iac1), S(iac2),
  /* 135 */ S(dac1),   S(dac2), S(dccr), S(iccr),
  /* 139 */ S(pbl1),   S(pbu1), S(pbl2), S(pbu2),
  /* 143 */ S(zpr),    S(pid),  S(sgr),  S(dcwr),
  /* 147 */ S(tbhu),   S(tblu)
};

/* Motorola PowerPC 505.  */
static const struct reg registers_505[] =
{
  COMMON_UISA_REGS,
  PPC_UISA_SPRS,
  PPC_SEGMENT_REGS,
  PPC_OEA_SPRS,
  /* 119 */ S(eie), S(eid), S(nri)
};

/* Motorola PowerPC 860 or 850.  */
static const struct reg registers_860[] =
{
  COMMON_UISA_REGS,
  PPC_UISA_SPRS,
  PPC_SEGMENT_REGS,
  PPC_OEA_SPRS,
  /* 119 */ S(eie), S(eid), S(nri), S(cmpa),
  /* 123 */ S(cmpb), S(cmpc), S(cmpd), S(icr),
  /* 127 */ S(der), S(counta), S(countb), S(cmpe),
  /* 131 */ S(cmpf), S(cmpg), S(cmph), S(lctrl1),
  /* 135 */ S(lctrl2), S(ictrl), S(bar), S(ic_cst),
  /* 139 */ S(ic_adr), S(ic_dat), S(dc_cst), S(dc_adr),
  /* 143 */ S(dc_dat), S(dpdr), S(dpir), S(immr),
  /* 147 */ S(mi_ctr), S(mi_ap), S(mi_epn), S(mi_twc),
  /* 151 */ S(mi_rpn), S(md_ctr), S(m_casid), S(md_ap),
  /* 155 */ S(md_epn), S(m_twb), S(md_twc), S(md_rpn),
  /* 159 */ S(m_tw), S(mi_dbcam), S(mi_dbram0), S(mi_dbram1),
  /* 163 */ S(md_dbcam), S(md_dbram0), S(md_dbram1)
};

/* Motorola PowerPC 601.  Note that the 601 has different register numbers
   for reading and writing RTCU and RTCL.  However, how one reads and writes a
   register is the stub's problem.  */
static const struct reg registers_601[] =
{
  COMMON_UISA_REGS,
  PPC_UISA_SPRS,
  PPC_SEGMENT_REGS,
  PPC_OEA_SPRS,
  /* 119 */ S(hid0), S(hid1), S(iabr), S(dabr),
  /* 123 */ S(pir), S(mq), S(rtcu), S(rtcl)
};

/* Motorola PowerPC 602.
   See the notes under the 403 about 'tcr'.  */
static const struct reg registers_602[] =
{
  COMMON_UISA_REGS,
  PPC_UISA_SPRS,
  PPC_SEGMENT_REGS,
  PPC_OEA_SPRS,
  /* 119 */ S(hid0), S(hid1), S(iabr), R0,
  /* 123 */ R0, SN4(tcr, ppc_spr_602_tcr), S(ibr), S(esasrr),
  /* 127 */ S(sebr), S(ser), S(sp), S(lt)
};

/* Motorola/IBM PowerPC 603 or 603e.  */
static const struct reg registers_603[] =
{
  COMMON_UISA_REGS,
  PPC_UISA_SPRS,
  PPC_SEGMENT_REGS,
  PPC_OEA_SPRS,
  /* 119 */ S(hid0), S(hid1), S(iabr), R0,
  /* 123 */ R0, S(dmiss), S(dcmp), S(hash1),
  /* 127 */ S(hash2), S(imiss), S(icmp), S(rpa)
};

/* Motorola PowerPC 604 or 604e.  */
static const struct reg registers_604[] =
{
  COMMON_UISA_REGS,
  PPC_UISA_SPRS,
  PPC_SEGMENT_REGS,
  PPC_OEA_SPRS,
  /* 119 */ S(hid0), S(hid1), S(iabr), S(dabr),
  /* 123 */ S(pir), S(mmcr0), S(pmc1), S(pmc2),
  /* 127 */ S(sia), S(sda)
};

/* Motorola/IBM PowerPC 750 or 740.  */
static const struct reg registers_750[] =
{
  COMMON_UISA_REGS,
  PPC_UISA_SPRS,
  PPC_SEGMENT_REGS,
  PPC_OEA_SPRS,
  /* 119 */ S(hid0), S(hid1), S(iabr), S(dabr),
  /* 123 */ R0, S(ummcr0), S(upmc1), S(upmc2),
  /* 127 */ S(usia), S(ummcr1), S(upmc3), S(upmc4),
  /* 131 */ S(mmcr0), S(pmc1), S(pmc2), S(sia),
  /* 135 */ S(mmcr1), S(pmc3), S(pmc4), S(l2cr),
  /* 139 */ S(ictc), S(thrm1), S(thrm2), S(thrm3)
};


/* Motorola PowerPC 7400.  */
static const struct reg registers_7400[] =
{
  /* gpr0-gpr31, fpr0-fpr31 */
  COMMON_UISA_REGS,
  /* cr, lr, ctr, xer, fpscr */
  PPC_UISA_SPRS,
  /* sr0-sr15 */
  PPC_SEGMENT_REGS,
  PPC_OEA_SPRS,
  /* vr0-vr31, vrsave, vscr */
  PPC_ALTIVEC_REGS
  /* FIXME? Add more registers? */
};

/* Motorola e500.  */
static const struct reg registers_e500[] =
{
  /*   0 ..  31 */ PPC_SPE_GP_REGS,
  /*  32 ..  63 */ PPC_SPE_UPPER_GP_REGS,
  /*  64 ..  65 */ R(pc), R(ps),
  /*  66 ..  70 */ PPC_UISA_NOFP_SPRS,
  /*  71 ..  72 */ R8(acc), S4(spefscr),
  /* NOTE: Add new registers here the end of the raw register
     list and just before the first pseudo register.  */
  /*  73 .. 104 */ PPC_EV_PSEUDO_REGS
};

/* Information about a particular processor variant.  */

struct variant
  {
    /* Name of this variant.  */
    char *name;

    /* English description of the variant.  */
    char *description;

    /* bfd_arch_info.arch corresponding to variant.  */
    enum bfd_architecture arch;

    /* bfd_arch_info.mach corresponding to variant.  */
    unsigned long mach;

    /* Number of real registers.  */
    int nregs;

    /* Number of pseudo registers.  */
    int npregs;

    /* Number of total registers (the sum of nregs and npregs).  */
    int num_tot_regs;

    /* Table of register names; registers[R] is the name of the register
       number R.  */
    const struct reg *regs;
  };

#define tot_num_registers(list) (sizeof (list) / sizeof((list)[0]))

static int
num_registers (const struct reg *reg_list, int num_tot_regs)
{
  int i;
  int nregs = 0;

  for (i = 0; i < num_tot_regs; i++)
    if (!reg_list[i].pseudo)
      nregs++;
       
  return nregs;
}

static int
num_pseudo_registers (const struct reg *reg_list, int num_tot_regs)
{
  int i;
  int npregs = 0;

  for (i = 0; i < num_tot_regs; i++)
    if (reg_list[i].pseudo)
      npregs ++; 

  return npregs;
}

/* Information in this table comes from the following web sites:
   IBM:       http://www.chips.ibm.com:80/products/embedded/
   Motorola:  http://www.mot.com/SPS/PowerPC/

   I'm sure I've got some of the variant descriptions not quite right.
   Please report any inaccuracies you find to GDB's maintainer.

   If you add entries to this table, please be sure to allow the new
   value as an argument to the --with-cpu flag, in configure.in.  */

static struct variant variants[] =
{

  {"powerpc", "PowerPC user-level", bfd_arch_powerpc,
   bfd_mach_ppc, -1, -1, tot_num_registers (registers_powerpc),
   registers_powerpc},
  {"power", "POWER user-level", bfd_arch_rs6000,
   bfd_mach_rs6k, -1, -1, tot_num_registers (registers_power),
   registers_power},
  {"403", "IBM PowerPC 403", bfd_arch_powerpc,
   bfd_mach_ppc_403, -1, -1, tot_num_registers (registers_403),
   registers_403},
  {"601", "Motorola PowerPC 601", bfd_arch_powerpc,
   bfd_mach_ppc_601, -1, -1, tot_num_registers (registers_601),
   registers_601},
  {"602", "Motorola PowerPC 602", bfd_arch_powerpc,
   bfd_mach_ppc_602, -1, -1, tot_num_registers (registers_602),
   registers_602},
  {"603", "Motorola/IBM PowerPC 603 or 603e", bfd_arch_powerpc,
   bfd_mach_ppc_603, -1, -1, tot_num_registers (registers_603),
   registers_603},
  {"604", "Motorola PowerPC 604 or 604e", bfd_arch_powerpc,
   604, -1, -1, tot_num_registers (registers_604),
   registers_604},
  {"403GC", "IBM PowerPC 403GC", bfd_arch_powerpc,
   bfd_mach_ppc_403gc, -1, -1, tot_num_registers (registers_403GC),
   registers_403GC},
  {"505", "Motorola PowerPC 505", bfd_arch_powerpc,
   bfd_mach_ppc_505, -1, -1, tot_num_registers (registers_505),
   registers_505},
  {"860", "Motorola PowerPC 860 or 850", bfd_arch_powerpc,
   bfd_mach_ppc_860, -1, -1, tot_num_registers (registers_860),
   registers_860},
  {"750", "Motorola/IBM PowerPC 750 or 740", bfd_arch_powerpc,
   bfd_mach_ppc_750, -1, -1, tot_num_registers (registers_750),
   registers_750},
  {"7400", "Motorola/IBM PowerPC 7400 (G4)", bfd_arch_powerpc,
   bfd_mach_ppc_7400, -1, -1, tot_num_registers (registers_7400),
   registers_7400},
  {"e500", "Motorola PowerPC e500", bfd_arch_powerpc,
   bfd_mach_ppc_e500, -1, -1, tot_num_registers (registers_e500),
   registers_e500},

  /* 64-bit */
  {"powerpc64", "PowerPC 64-bit user-level", bfd_arch_powerpc,
   bfd_mach_ppc64, -1, -1, tot_num_registers (registers_powerpc),
   registers_powerpc},
  {"620", "Motorola PowerPC 620", bfd_arch_powerpc,
   bfd_mach_ppc_620, -1, -1, tot_num_registers (registers_powerpc),
   registers_powerpc},
  {"630", "Motorola PowerPC 630", bfd_arch_powerpc,
   bfd_mach_ppc_630, -1, -1, tot_num_registers (registers_powerpc),
   registers_powerpc},
  {"a35", "PowerPC A35", bfd_arch_powerpc,
   bfd_mach_ppc_a35, -1, -1, tot_num_registers (registers_powerpc),
   registers_powerpc},
  {"rs64ii", "PowerPC rs64ii", bfd_arch_powerpc,
   bfd_mach_ppc_rs64ii, -1, -1, tot_num_registers (registers_powerpc),
   registers_powerpc},
  {"rs64iii", "PowerPC rs64iii", bfd_arch_powerpc,
   bfd_mach_ppc_rs64iii, -1, -1, tot_num_registers (registers_powerpc),
   registers_powerpc},

  /* FIXME: I haven't checked the register sets of the following.  */
  {"rs1", "IBM POWER RS1", bfd_arch_rs6000,
   bfd_mach_rs6k_rs1, -1, -1, tot_num_registers (registers_power),
   registers_power},
  {"rsc", "IBM POWER RSC", bfd_arch_rs6000,
   bfd_mach_rs6k_rsc, -1, -1, tot_num_registers (registers_power),
   registers_power},
  {"rs2", "IBM POWER RS2", bfd_arch_rs6000,
   bfd_mach_rs6k_rs2, -1, -1, tot_num_registers (registers_power),
   registers_power},

  {0, 0, 0, 0, 0, 0, 0, 0}
};

/* Initialize the number of registers and pseudo registers in each variant.  */

static void
init_variants (void)
{
  struct variant *v;

  for (v = variants; v->name; v++)
    {
      if (v->nregs == -1)
        v->nregs = num_registers (v->regs, v->num_tot_regs);
      if (v->npregs == -1)
        v->npregs = num_pseudo_registers (v->regs, v->num_tot_regs);
    }  
}

/* Return the variant corresponding to architecture ARCH and machine number
   MACH.  If no such variant exists, return null.  */

static const struct variant *
find_variant_by_arch (enum bfd_architecture arch, unsigned long mach)
{
  const struct variant *v;

  for (v = variants; v->name; v++)
    if (arch == v->arch && mach == v->mach)
      return v;

  return NULL;
}

static int
gdb_print_insn_powerpc (bfd_vma memaddr, disassemble_info *info)
{
  if (TARGET_BYTE_ORDER == BFD_ENDIAN_BIG)
    return print_insn_big_powerpc (memaddr, info);
  else
    return print_insn_little_powerpc (memaddr, info);
}

static CORE_ADDR
rs6000_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, PC_REGNUM);
}

static struct frame_id
rs6000_unwind_dummy_id (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_id_build (frame_unwind_register_unsigned (next_frame,
							 SP_REGNUM),
			 frame_pc_unwind (next_frame));
}

struct rs6000_frame_cache
{
  CORE_ADDR base;
  CORE_ADDR initial_sp;
  struct trad_frame_saved_reg *saved_regs;
};

static struct rs6000_frame_cache *
rs6000_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct rs6000_frame_cache *cache;
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  struct rs6000_framedata fdata;
  int wordsize = tdep->wordsize;

  if ((*this_cache) != NULL)
    return (*this_cache);
  cache = FRAME_OBSTACK_ZALLOC (struct rs6000_frame_cache);
  (*this_cache) = cache;
  cache->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  skip_prologue (frame_func_unwind (next_frame), frame_pc_unwind (next_frame),
		 &fdata);

  /* If there were any saved registers, figure out parent's stack
     pointer.  */
  /* The following is true only if the frame doesn't have a call to
     alloca(), FIXME.  */

  if (fdata.saved_fpr == 0
      && fdata.saved_gpr == 0
      && fdata.saved_vr == 0
      && fdata.saved_ev == 0
      && fdata.lr_offset == 0
      && fdata.cr_offset == 0
      && fdata.vr_offset == 0
      && fdata.ev_offset == 0)
    cache->base = frame_unwind_register_unsigned (next_frame, SP_REGNUM);
  else
    {
      /* NOTE: cagney/2002-04-14: The ->frame points to the inner-most
	 address of the current frame.  Things might be easier if the
	 ->frame pointed to the outer-most address of the frame.  In
	 the mean time, the address of the prev frame is used as the
	 base address of this frame.  */
      cache->base = frame_unwind_register_unsigned (next_frame, SP_REGNUM);
      if (!fdata.frameless)
	/* Frameless really means stackless.  */
	cache->base = read_memory_addr (cache->base, wordsize);
    }
  trad_frame_set_value (cache->saved_regs, SP_REGNUM, cache->base);

  /* if != -1, fdata.saved_fpr is the smallest number of saved_fpr.
     All fpr's from saved_fpr to fp31 are saved.  */

  if (fdata.saved_fpr >= 0)
    {
      int i;
      CORE_ADDR fpr_addr = cache->base + fdata.fpr_offset;

      /* If skip_prologue says floating-point registers were saved,
         but the current architecture has no floating-point registers,
         then that's strange.  But we have no indices to even record
         the addresses under, so we just ignore it.  */
      if (ppc_floating_point_unit_p (gdbarch))
        for (i = fdata.saved_fpr; i < ppc_num_fprs; i++)
          {
            cache->saved_regs[tdep->ppc_fp0_regnum + i].addr = fpr_addr;
            fpr_addr += 8;
          }
    }

  /* if != -1, fdata.saved_gpr is the smallest number of saved_gpr.
     All gpr's from saved_gpr to gpr31 are saved.  */

  if (fdata.saved_gpr >= 0)
    {
      int i;
      CORE_ADDR gpr_addr = cache->base + fdata.gpr_offset;
      for (i = fdata.saved_gpr; i < ppc_num_gprs; i++)
	{
	  cache->saved_regs[tdep->ppc_gp0_regnum + i].addr = gpr_addr;
	  gpr_addr += wordsize;
	}
    }

  /* if != -1, fdata.saved_vr is the smallest number of saved_vr.
     All vr's from saved_vr to vr31 are saved.  */
  if (tdep->ppc_vr0_regnum != -1 && tdep->ppc_vrsave_regnum != -1)
    {
      if (fdata.saved_vr >= 0)
	{
	  int i;
	  CORE_ADDR vr_addr = cache->base + fdata.vr_offset;
	  for (i = fdata.saved_vr; i < 32; i++)
	    {
	      cache->saved_regs[tdep->ppc_vr0_regnum + i].addr = vr_addr;
	      vr_addr += register_size (gdbarch, tdep->ppc_vr0_regnum);
	    }
	}
    }

  /* if != -1, fdata.saved_ev is the smallest number of saved_ev.
     All vr's from saved_ev to ev31 are saved. ????? */
  if (tdep->ppc_ev0_regnum != -1 && tdep->ppc_ev31_regnum != -1)
    {
      if (fdata.saved_ev >= 0)
	{
	  int i;
	  CORE_ADDR ev_addr = cache->base + fdata.ev_offset;
	  for (i = fdata.saved_ev; i < ppc_num_gprs; i++)
	    {
	      cache->saved_regs[tdep->ppc_ev0_regnum + i].addr = ev_addr;
              cache->saved_regs[tdep->ppc_gp0_regnum + i].addr = ev_addr + 4;
	      ev_addr += register_size (gdbarch, tdep->ppc_ev0_regnum);
            }
	}
    }

  /* If != 0, fdata.cr_offset is the offset from the frame that
     holds the CR.  */
  if (fdata.cr_offset != 0)
    cache->saved_regs[tdep->ppc_cr_regnum].addr = cache->base + fdata.cr_offset;

  /* If != 0, fdata.lr_offset is the offset from the frame that
     holds the LR.  */
  if (fdata.lr_offset != 0)
    cache->saved_regs[tdep->ppc_lr_regnum].addr = cache->base + fdata.lr_offset;
  /* The PC is found in the link register.  */
  cache->saved_regs[PC_REGNUM] = cache->saved_regs[tdep->ppc_lr_regnum];

  /* If != 0, fdata.vrsave_offset is the offset from the frame that
     holds the VRSAVE.  */
  if (fdata.vrsave_offset != 0)
    cache->saved_regs[tdep->ppc_vrsave_regnum].addr = cache->base + fdata.vrsave_offset;

  if (fdata.alloca_reg < 0)
    /* If no alloca register used, then fi->frame is the value of the
       %sp for this frame, and it is good enough.  */
    cache->initial_sp = frame_unwind_register_unsigned (next_frame, SP_REGNUM);
  else
    cache->initial_sp = frame_unwind_register_unsigned (next_frame,
							fdata.alloca_reg);

  return cache;
}

static void
rs6000_frame_this_id (struct frame_info *next_frame, void **this_cache,
		      struct frame_id *this_id)
{
  struct rs6000_frame_cache *info = rs6000_frame_cache (next_frame,
							this_cache);
  (*this_id) = frame_id_build (info->base, frame_func_unwind (next_frame));
}

static void
rs6000_frame_prev_register (struct frame_info *next_frame,
				 void **this_cache,
				 int regnum, int *optimizedp,
				 enum lval_type *lvalp, CORE_ADDR *addrp,
				 int *realnump, void *valuep)
{
  struct rs6000_frame_cache *info = rs6000_frame_cache (next_frame,
							this_cache);
  trad_frame_get_prev_register (next_frame, info->saved_regs, regnum,
				optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind rs6000_frame_unwind =
{
  NORMAL_FRAME,
  rs6000_frame_this_id,
  rs6000_frame_prev_register
};

static const struct frame_unwind *
rs6000_frame_sniffer (struct frame_info *next_frame)
{
  return &rs6000_frame_unwind;
}



static CORE_ADDR
rs6000_frame_base_address (struct frame_info *next_frame,
				void **this_cache)
{
  struct rs6000_frame_cache *info = rs6000_frame_cache (next_frame,
							this_cache);
  return info->initial_sp;
}

static const struct frame_base rs6000_frame_base = {
  &rs6000_frame_unwind,
  rs6000_frame_base_address,
  rs6000_frame_base_address,
  rs6000_frame_base_address
};

static const struct frame_base *
rs6000_frame_base_sniffer (struct frame_info *next_frame)
{
  return &rs6000_frame_base;
}

/* Initialize the current architecture based on INFO.  If possible, re-use an
   architecture from ARCHES, which is a list of architectures already created
   during this debugging session.

   Called e.g. at program startup, when reading a core file, and when reading
   a binary file.  */

static struct gdbarch *
rs6000_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *tdep;
  int wordsize, from_xcoff_exec, from_elf_exec, i, off;
  struct reg *regs;
  const struct variant *v;
  enum bfd_architecture arch;
  unsigned long mach;
  bfd abfd;
  int sysv_abi;
  asection *sect;

  from_xcoff_exec = info.abfd && info.abfd->format == bfd_object &&
    bfd_get_flavour (info.abfd) == bfd_target_xcoff_flavour;

  from_elf_exec = info.abfd && info.abfd->format == bfd_object &&
    bfd_get_flavour (info.abfd) == bfd_target_elf_flavour;

  sysv_abi = info.abfd && bfd_get_flavour (info.abfd) == bfd_target_elf_flavour;

  /* Check word size.  If INFO is from a binary file, infer it from
     that, else choose a likely default.  */
  if (from_xcoff_exec)
    {
      if (bfd_xcoff_is_xcoff64 (info.abfd))
	wordsize = 8;
      else
	wordsize = 4;
    }
  else if (from_elf_exec)
    {
      if (elf_elfheader (info.abfd)->e_ident[EI_CLASS] == ELFCLASS64)
	wordsize = 8;
      else
	wordsize = 4;
    }
  else
    {
      if (info.bfd_arch_info != NULL && info.bfd_arch_info->bits_per_word != 0)
	wordsize = info.bfd_arch_info->bits_per_word /
	  info.bfd_arch_info->bits_per_byte;
      else
	wordsize = 4;
    }

  /* Find a candidate among extant architectures.  */
  for (arches = gdbarch_list_lookup_by_info (arches, &info);
       arches != NULL;
       arches = gdbarch_list_lookup_by_info (arches->next, &info))
    {
      /* Word size in the various PowerPC bfd_arch_info structs isn't
         meaningful, because 64-bit CPUs can run in 32-bit mode.  So, perform
         separate word size check.  */
      tdep = gdbarch_tdep (arches->gdbarch);
      if (tdep && tdep->wordsize == wordsize)
	return arches->gdbarch;
    }

  /* None found, create a new architecture from INFO, whose bfd_arch_info
     validity depends on the source:
       - executable		useless
       - rs6000_host_arch()	good
       - core file		good
       - "set arch"		trust blindly
       - GDB startup		useless but harmless */

  if (!from_xcoff_exec)
    {
      arch = info.bfd_arch_info->arch;
      mach = info.bfd_arch_info->mach;
    }
  else
    {
      arch = bfd_arch_powerpc;
      bfd_default_set_arch_mach (&abfd, arch, 0);
      info.bfd_arch_info = bfd_get_arch_info (&abfd);
      mach = info.bfd_arch_info->mach;
    }
  tdep = xmalloc (sizeof (struct gdbarch_tdep));
  tdep->wordsize = wordsize;

  /* For e500 executables, the apuinfo section is of help here.  Such
     section contains the identifier and revision number of each
     Application-specific Processing Unit that is present on the
     chip.  The content of the section is determined by the assembler
     which looks at each instruction and determines which unit (and
     which version of it) can execute it. In our case we just look for
     the existance of the section.  */

  if (info.abfd)
    {
      sect = bfd_get_section_by_name (info.abfd, ".PPC.EMB.apuinfo");
      if (sect)
	{
	  arch = info.bfd_arch_info->arch;
	  mach = bfd_mach_ppc_e500;
	  bfd_default_set_arch_mach (&abfd, arch, mach);
	  info.bfd_arch_info = bfd_get_arch_info (&abfd);
	}
    }

  gdbarch = gdbarch_alloc (&info, tdep);

  /* Initialize the number of real and pseudo registers in each variant.  */
  init_variants ();

  /* Choose variant.  */
  v = find_variant_by_arch (arch, mach);
  if (!v)
    return NULL;

  tdep->regs = v->regs;

  tdep->ppc_gp0_regnum = 0;
  tdep->ppc_toc_regnum = 2;
  tdep->ppc_ps_regnum = 65;
  tdep->ppc_cr_regnum = 66;
  tdep->ppc_lr_regnum = 67;
  tdep->ppc_ctr_regnum = 68;
  tdep->ppc_xer_regnum = 69;
  if (v->mach == bfd_mach_ppc_601)
    tdep->ppc_mq_regnum = 124;
  else if (arch == bfd_arch_rs6000)
    tdep->ppc_mq_regnum = 70;
  else
    tdep->ppc_mq_regnum = -1;
  tdep->ppc_fp0_regnum = 32;
  tdep->ppc_fpscr_regnum = (arch == bfd_arch_rs6000) ? 71 : 70;
  tdep->ppc_sr0_regnum = 71;
  tdep->ppc_vr0_regnum = -1;
  tdep->ppc_vrsave_regnum = -1;
  tdep->ppc_ev0_upper_regnum = -1;
  tdep->ppc_ev0_regnum = -1;
  tdep->ppc_ev31_regnum = -1;
  tdep->ppc_acc_regnum = -1;
  tdep->ppc_spefscr_regnum = -1;

  set_gdbarch_pc_regnum (gdbarch, 64);
  set_gdbarch_sp_regnum (gdbarch, 1);
  set_gdbarch_deprecated_fp_regnum (gdbarch, 1);
  set_gdbarch_register_sim_regno (gdbarch, rs6000_register_sim_regno);
  if (sysv_abi && wordsize == 8)
    set_gdbarch_return_value (gdbarch, ppc64_sysv_abi_return_value);
  else if (sysv_abi && wordsize == 4)
    set_gdbarch_return_value (gdbarch, ppc_sysv_abi_return_value);
  else
    {
      set_gdbarch_deprecated_extract_return_value (gdbarch, rs6000_extract_return_value);
      set_gdbarch_store_return_value (gdbarch, rs6000_store_return_value);
    }

  /* Set lr_frame_offset.  */
  if (wordsize == 8)
    tdep->lr_frame_offset = 16;
  else if (sysv_abi)
    tdep->lr_frame_offset = 4;
  else
    tdep->lr_frame_offset = 8;

  if (v->arch == bfd_arch_rs6000)
    tdep->ppc_sr0_regnum = -1;
  else if (v->arch == bfd_arch_powerpc)
    switch (v->mach)
      {
      case bfd_mach_ppc: 
        tdep->ppc_sr0_regnum = -1;
	tdep->ppc_vr0_regnum = 71;
	tdep->ppc_vrsave_regnum = 104;
	break;
      case bfd_mach_ppc_7400:
	tdep->ppc_vr0_regnum = 119;
	tdep->ppc_vrsave_regnum = 152;
	break;
      case bfd_mach_ppc_e500:
        tdep->ppc_toc_regnum = -1;
        tdep->ppc_ev0_upper_regnum = 32;
	tdep->ppc_ev0_regnum = 73;
	tdep->ppc_ev31_regnum = 104;
        tdep->ppc_acc_regnum = 71;
        tdep->ppc_spefscr_regnum = 72;
        tdep->ppc_fp0_regnum = -1;
        tdep->ppc_fpscr_regnum = -1;
        tdep->ppc_sr0_regnum = -1;
        set_gdbarch_pseudo_register_read (gdbarch, e500_pseudo_register_read);
        set_gdbarch_pseudo_register_write (gdbarch, e500_pseudo_register_write);
        set_gdbarch_register_reggroup_p (gdbarch, e500_register_reggroup_p);
	break;

      case bfd_mach_ppc64:
      case bfd_mach_ppc_620:
      case bfd_mach_ppc_630:
      case bfd_mach_ppc_a35:
      case bfd_mach_ppc_rs64ii:
      case bfd_mach_ppc_rs64iii:
        /* These processor's register sets don't have segment registers.  */
        tdep->ppc_sr0_regnum = -1;
        break;
      }   
  else
    internal_error (__FILE__, __LINE__,
                    "rs6000_gdbarch_init: "
                    "received unexpected BFD 'arch' value");

  /* Sanity check on registers.  */
  gdb_assert (strcmp (tdep->regs[tdep->ppc_gp0_regnum].name, "r0") == 0);

  /* Select instruction printer.  */
  if (arch == bfd_arch_rs6000)
    set_gdbarch_print_insn (gdbarch, print_insn_rs6000);
  else
    set_gdbarch_print_insn (gdbarch, gdb_print_insn_powerpc);

  set_gdbarch_write_pc (gdbarch, generic_target_write_pc);

  set_gdbarch_num_regs (gdbarch, v->nregs);
  set_gdbarch_num_pseudo_regs (gdbarch, v->npregs);
  set_gdbarch_register_name (gdbarch, rs6000_register_name);
  set_gdbarch_register_type (gdbarch, rs6000_register_type);

  set_gdbarch_ptr_bit (gdbarch, wordsize * TARGET_CHAR_BIT);
  set_gdbarch_short_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_int_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_bit (gdbarch, wordsize * TARGET_CHAR_BIT);
  set_gdbarch_long_long_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_float_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  if (sysv_abi)
    set_gdbarch_long_double_bit (gdbarch, 16 * TARGET_CHAR_BIT);
  else
    set_gdbarch_long_double_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_char_signed (gdbarch, 0);

  set_gdbarch_frame_align (gdbarch, rs6000_frame_align);
  if (sysv_abi && wordsize == 8)
    /* PPC64 SYSV.  */
    set_gdbarch_frame_red_zone_size (gdbarch, 288);
  else if (!sysv_abi && wordsize == 4)
    /* PowerOpen / AIX 32 bit.  The saved area or red zone consists of
       19 4 byte GPRS + 18 8 byte FPRs giving a total of 220 bytes.
       Problem is, 220 isn't frame (16 byte) aligned.  Round it up to
       224.  */
    set_gdbarch_frame_red_zone_size (gdbarch, 224);

  set_gdbarch_convert_register_p (gdbarch, rs6000_convert_register_p);
  set_gdbarch_register_to_value (gdbarch, rs6000_register_to_value);
  set_gdbarch_value_to_register (gdbarch, rs6000_value_to_register);

  set_gdbarch_stab_reg_to_regnum (gdbarch, rs6000_stab_reg_to_regnum);
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, rs6000_dwarf2_reg_to_regnum);
  /* Note: kevinb/2002-04-12: I'm not convinced that rs6000_push_arguments()
     is correct for the SysV ABI when the wordsize is 8, but I'm also
     fairly certain that ppc_sysv_abi_push_arguments() will give even
     worse results since it only works for 32-bit code.  So, for the moment,
     we're better off calling rs6000_push_arguments() since it works for
     64-bit code.  At some point in the future, this matter needs to be
     revisited.  */
  if (sysv_abi && wordsize == 4)
    set_gdbarch_push_dummy_call (gdbarch, ppc_sysv_abi_push_dummy_call);
  else if (sysv_abi && wordsize == 8)
    set_gdbarch_push_dummy_call (gdbarch, ppc64_sysv_abi_push_dummy_call);
  else
    set_gdbarch_push_dummy_call (gdbarch, rs6000_push_dummy_call);

  set_gdbarch_deprecated_extract_struct_value_address (gdbarch, rs6000_extract_struct_value_address);

  set_gdbarch_skip_prologue (gdbarch, rs6000_skip_prologue);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_breakpoint_from_pc (gdbarch, rs6000_breakpoint_from_pc);

  /* Handle the 64-bit SVR4 minimal-symbol convention of using "FN"
     for the descriptor and ".FN" for the entry-point -- a user
     specifying "break FN" will unexpectedly end up with a breakpoint
     on the descriptor and not the function.  This architecture method
     transforms any breakpoints on descriptors into breakpoints on the
     corresponding entry point.  */
  if (sysv_abi && wordsize == 8)
    set_gdbarch_adjust_breakpoint_address (gdbarch, ppc64_sysv_abi_adjust_breakpoint_address);

  /* Not sure on this. FIXMEmgo */
  set_gdbarch_frame_args_skip (gdbarch, 8);

  if (!sysv_abi)
    set_gdbarch_deprecated_use_struct_convention (gdbarch, rs6000_use_struct_convention);

  if (!sysv_abi)
    {
      /* Handle RS/6000 function pointers (which are really function
         descriptors).  */
      set_gdbarch_convert_from_func_ptr_addr (gdbarch,
	rs6000_convert_from_func_ptr_addr);
    }

  /* Helpers for function argument information.  */
  set_gdbarch_fetch_pointer_argument (gdbarch, rs6000_fetch_pointer_argument);

  /* Hook in ABI-specific overrides, if they have been registered.  */
  gdbarch_init_osabi (info, gdbarch);

  switch (info.osabi)
    {
    case GDB_OSABI_NETBSD_AOUT:
    case GDB_OSABI_NETBSD_ELF:
    case GDB_OSABI_UNKNOWN:
    case GDB_OSABI_LINUX:
      set_gdbarch_unwind_pc (gdbarch, rs6000_unwind_pc);
      frame_unwind_append_sniffer (gdbarch, rs6000_frame_sniffer);
      set_gdbarch_unwind_dummy_id (gdbarch, rs6000_unwind_dummy_id);
      frame_base_append_sniffer (gdbarch, rs6000_frame_base_sniffer);
      break;
    default:
      set_gdbarch_believe_pcc_promotion (gdbarch, 1);

      set_gdbarch_unwind_pc (gdbarch, rs6000_unwind_pc);
      frame_unwind_append_sniffer (gdbarch, rs6000_frame_sniffer);
      set_gdbarch_unwind_dummy_id (gdbarch, rs6000_unwind_dummy_id);
      frame_base_append_sniffer (gdbarch, rs6000_frame_base_sniffer);
    }

  if (from_xcoff_exec)
    {
      /* NOTE: jimix/2003-06-09: This test should really check for
	 GDB_OSABI_AIX when that is defined and becomes
	 available. (Actually, once things are properly split apart,
	 the test goes away.) */
       /* RS6000/AIX does not support PT_STEP.  Has to be simulated.  */
       set_gdbarch_software_single_step (gdbarch, rs6000_software_single_step);
    }

  init_sim_regno_table (gdbarch);

  return gdbarch;
}

static void
rs6000_dump_tdep (struct gdbarch *current_gdbarch, struct ui_file *file)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  if (tdep == NULL)
    return;

  /* FIXME: Dump gdbarch_tdep.  */
}

static struct cmd_list_element *info_powerpc_cmdlist = NULL;

static void
rs6000_info_powerpc_command (char *args, int from_tty)
{
  help_list (info_powerpc_cmdlist, "info powerpc ", class_info, gdb_stdout);
}

/* Initialization code.  */

extern initialize_file_ftype _initialize_rs6000_tdep; /* -Wmissing-prototypes */

void
_initialize_rs6000_tdep (void)
{
  gdbarch_register (bfd_arch_rs6000, rs6000_gdbarch_init, rs6000_dump_tdep);
  gdbarch_register (bfd_arch_powerpc, rs6000_gdbarch_init, rs6000_dump_tdep);

  /* Add root prefix command for "info powerpc" commands */
  add_prefix_cmd ("powerpc", class_info, rs6000_info_powerpc_command,
		  "Various POWERPC info specific commands.",
		  &info_powerpc_cmdlist, "info powerpc ", 0, &infolist);
}

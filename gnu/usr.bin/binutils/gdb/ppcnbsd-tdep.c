/* Target-dependent code for PowerPC systems running NetBSD.

   Copyright 2002, 2003, 2004 Free Software Foundation, Inc.

   Contributed by Wasabi Systems, Inc.

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
#include "regcache.h"
#include "target.h"
#include "breakpoint.h"
#include "value.h"
#include "osabi.h"

#include "ppc-tdep.h"
#include "ppcnbsd-tdep.h"
#include "nbsd-tdep.h"
#include "tramp-frame.h"
#include "trad-frame.h"
#include "gdb_assert.h"
#include "solib-svr4.h"

#define REG_FIXREG_OFFSET(x)	((x) * 4)
#define REG_LR_OFFSET		(32 * 4)
#define REG_CR_OFFSET		(33 * 4)
#define REG_XER_OFFSET		(34 * 4)
#define REG_CTR_OFFSET		(35 * 4)
#define REG_PC_OFFSET		(36 * 4)
#define SIZEOF_STRUCT_REG	(37 * 4)

#define FPREG_FPR_OFFSET(x)	((x) * 8)
#define FPREG_FPSCR_OFFSET	(32 * 8)
#define SIZEOF_STRUCT_FPREG	(33 * 8)

void
ppcnbsd_supply_reg (char *regs, int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int i;

  for (i = 0; i < ppc_num_gprs; i++)
    {
      if (regno == tdep->ppc_gp0_regnum + i || regno == -1)
	regcache_raw_supply (current_regcache, tdep->ppc_gp0_regnum + i,
			     regs + REG_FIXREG_OFFSET (i));
    }

  if (regno == tdep->ppc_lr_regnum || regno == -1)
    regcache_raw_supply (current_regcache, tdep->ppc_lr_regnum,
			 regs + REG_LR_OFFSET);

  if (regno == tdep->ppc_cr_regnum || regno == -1)
    regcache_raw_supply (current_regcache, tdep->ppc_cr_regnum,
			 regs + REG_CR_OFFSET);

  if (regno == tdep->ppc_xer_regnum || regno == -1)
    regcache_raw_supply (current_regcache, tdep->ppc_xer_regnum,
			 regs + REG_XER_OFFSET);

  if (regno == tdep->ppc_ctr_regnum || regno == -1)
    regcache_raw_supply (current_regcache, tdep->ppc_ctr_regnum,
			 regs + REG_CTR_OFFSET);

  if (regno == PC_REGNUM || regno == -1)
    regcache_raw_supply (current_regcache, PC_REGNUM,
			 regs + REG_PC_OFFSET);
}

void
ppcnbsd_fill_reg (char *regs, int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int i;

  for (i = 0; i < ppc_num_gprs; i++)
    {
      if (regno == tdep->ppc_gp0_regnum + i || regno == -1)
	regcache_raw_collect (current_regcache, tdep->ppc_gp0_regnum + i,
			      regs + REG_FIXREG_OFFSET (i));
    }

  if (regno == tdep->ppc_lr_regnum || regno == -1)
    regcache_raw_collect (current_regcache, tdep->ppc_lr_regnum,
			  regs + REG_LR_OFFSET);

  if (regno == tdep->ppc_cr_regnum || regno == -1)
    regcache_raw_collect (current_regcache, tdep->ppc_cr_regnum,
			  regs + REG_CR_OFFSET);

  if (regno == tdep->ppc_xer_regnum || regno == -1)
    regcache_raw_collect (current_regcache, tdep->ppc_xer_regnum,
			  regs + REG_XER_OFFSET);

  if (regno == tdep->ppc_ctr_regnum || regno == -1)
    regcache_raw_collect (current_regcache, tdep->ppc_ctr_regnum,
			  regs + REG_CTR_OFFSET);

  if (regno == PC_REGNUM || regno == -1)
    regcache_raw_collect (current_regcache, PC_REGNUM, regs + REG_PC_OFFSET);
}

void
ppcnbsd_supply_fpreg (char *fpregs, int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int i;

  /* FIXME: jimb/2004-05-05: Some PPC variants don't have floating
     point registers.  Traditionally, GDB's register set has still
     listed the floating point registers for such machines, so this
     code is harmless.  However, the new E500 port actually omits the
     floating point registers entirely from the register set --- they
     don't even have register numbers assigned to them.

     It's not clear to me how best to update this code, so this assert
     will alert the first person to encounter the NetBSD/E500
     combination to the problem.  */
  gdb_assert (ppc_floating_point_unit_p (current_gdbarch));

  for (i = 0; i < ppc_num_fprs; i++)
    {
      if (regno == tdep->ppc_fp0_regnum + i || regno == -1)
	regcache_raw_supply (current_regcache, tdep->ppc_fp0_regnum + i,
			     fpregs + FPREG_FPR_OFFSET (i));
    }

  if (regno == tdep->ppc_fpscr_regnum || regno == -1)
    regcache_raw_supply (current_regcache, tdep->ppc_fpscr_regnum,
			 fpregs + FPREG_FPSCR_OFFSET);
}

void
ppcnbsd_fill_fpreg (char *fpregs, int regno)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  int i;

  /* FIXME: jimb/2004-05-05: Some PPC variants don't have floating
     point registers.  Traditionally, GDB's register set has still
     listed the floating point registers for such machines, so this
     code is harmless.  However, the new E500 port actually omits the
     floating point registers entirely from the register set --- they
     don't even have register numbers assigned to them.

     It's not clear to me how best to update this code, so this assert
     will alert the first person to encounter the NetBSD/E500
     combination to the problem.  */
  gdb_assert (ppc_floating_point_unit_p (current_gdbarch));

  for (i = 0; i < ppc_num_fprs; i++)
    {
      if (regno == tdep->ppc_fp0_regnum + i || regno == -1)
	regcache_raw_collect (current_regcache, tdep->ppc_fp0_regnum + i,
			      fpregs + FPREG_FPR_OFFSET (i));
    }

  if (regno == tdep->ppc_fpscr_regnum || regno == -1)
    regcache_raw_collect (current_regcache, tdep->ppc_fpscr_regnum,
			  fpregs + FPREG_FPSCR_OFFSET);
}

static void
fetch_core_registers (char *core_reg_sect, unsigned core_reg_size, int which,
                      CORE_ADDR ignore)
{
  char *regs, *fpregs;

  /* We get everything from one section.  */
  if (which != 0)
    return;

  regs = core_reg_sect;
  fpregs = core_reg_sect + SIZEOF_STRUCT_REG;

  /* Integer registers.  */
  ppcnbsd_supply_reg (regs, -1);

  /* Floating point registers.  */
  ppcnbsd_supply_fpreg (fpregs, -1);
}

static void
fetch_elfcore_registers (char *core_reg_sect, unsigned core_reg_size, int which,
                         CORE_ADDR ignore)
{
  switch (which)
    {
    case 0:  /* Integer registers.  */
      if (core_reg_size != SIZEOF_STRUCT_REG)
	warning ("Wrong size register set in core file.");
      else
	ppcnbsd_supply_reg (core_reg_sect, -1);
      break;

    case 2:  /* Floating point registers.  */
      if (core_reg_size != SIZEOF_STRUCT_FPREG)
	warning ("Wrong size FP register set in core file.");
      else
	ppcnbsd_supply_fpreg (core_reg_sect, -1);
      break;

    default:
      /* Don't know what kind of register request this is; just ignore it.  */
      break;
    }
}

static struct core_fns ppcnbsd_core_fns =
{
  bfd_target_unknown_flavour,		/* core_flavour */
  default_check_format,			/* check_format */
  default_core_sniffer,			/* core_sniffer */
  fetch_core_registers,			/* core_read_registers */
  NULL					/* next */
};

static struct core_fns ppcnbsd_elfcore_fns =
{
  bfd_target_elf_flavour,		/* core_flavour */
  default_check_format,			/* check_format */
  default_core_sniffer,			/* core_sniffer */
  fetch_elfcore_registers,		/* core_read_registers */
  NULL					/* next */
};

/* NetBSD is confused.  It appears that 1.5 was using the correct SVr4
   convention but, 1.6 switched to the below broken convention.  For
   the moment use the broken convention.  Ulgh!.  */

static enum return_value_convention
ppcnbsd_return_value (struct gdbarch *gdbarch, struct type *valtype,
		      struct regcache *regcache, void *readbuf,
		      const void *writebuf)
{
  if ((TYPE_CODE (valtype) == TYPE_CODE_STRUCT
       || TYPE_CODE (valtype) == TYPE_CODE_UNION)
      && !((TYPE_LENGTH (valtype) == 16 || TYPE_LENGTH (valtype) == 8)
	    && TYPE_VECTOR (valtype))
      && !(TYPE_LENGTH (valtype) == 1
	   || TYPE_LENGTH (valtype) == 2
	   || TYPE_LENGTH (valtype) == 4
	   || TYPE_LENGTH (valtype) == 8))
    return RETURN_VALUE_STRUCT_CONVENTION;
  else
    return ppc_sysv_abi_broken_return_value (gdbarch, valtype, regcache,
					     readbuf, writebuf);
}

static void
ppcnbsd_sigtramp_cache_init (const struct tramp_frame *self,
			     struct frame_info *next_frame,
			     struct trad_frame_cache *this_cache,
			     CORE_ADDR func)
{
  CORE_ADDR base;
  CORE_ADDR offset;
  int i;
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  base = frame_unwind_register_unsigned (next_frame, SP_REGNUM);
  offset = base + 0x18 + 2 * tdep->wordsize;
  for (i = 0; i < ppc_num_gprs; i++)
    {
      int regnum = i + tdep->ppc_gp0_regnum;
      trad_frame_set_reg_addr (this_cache, regnum, offset);
      offset += tdep->wordsize;
    }
  trad_frame_set_reg_addr (this_cache, tdep->ppc_lr_regnum, offset);
  offset += tdep->wordsize;
  trad_frame_set_reg_addr (this_cache, tdep->ppc_cr_regnum, offset);
  offset += tdep->wordsize;
  trad_frame_set_reg_addr (this_cache, tdep->ppc_xer_regnum, offset);
  offset += tdep->wordsize;
  trad_frame_set_reg_addr (this_cache, tdep->ppc_ctr_regnum, offset);
  offset += tdep->wordsize;
  trad_frame_set_reg_addr (this_cache, PC_REGNUM, offset); /* SRR0? */
  offset += tdep->wordsize;

  /* Construct the frame ID using the function start.  */
  trad_frame_set_id (this_cache, frame_id_build (base, func));
}

/* Given the NEXT frame, examine the instructions at and around this
   frame's resume address (aka PC) to see of they look like a signal
   trampoline.  Return the address of the trampolines first
   instruction, or zero if it isn't a signal trampoline.  */

static const struct tramp_frame ppcnbsd_sigtramp = {
  SIGTRAMP_FRAME,
  4, /* insn size */
  { /* insn */
    { 0x38610018, -1 }, /* addi r3,r1,24 */
    { 0x38000127, -1 }, /* li r0,295 */
    { 0x44000002, -1 }, /* sc */
    { 0x38000001, -1 }, /* li r0,1 */
    { 0x44000002, -1 }, /* sc */
    { TRAMP_SENTINEL_INSN, -1 }
  },
  ppcnbsd_sigtramp_cache_init
};

static void
ppcnbsd_init_abi (struct gdbarch_info info,
                  struct gdbarch *gdbarch)
{
  /* For NetBSD, this is an on again, off again thing.  Some systems
     do use the broken struct convention, and some don't.  */
  set_gdbarch_return_value (gdbarch, ppcnbsd_return_value);
  set_solib_svr4_fetch_link_map_offsets (gdbarch,
                                nbsd_ilp32_solib_svr4_fetch_link_map_offsets);
  tramp_frame_prepend_unwinder (gdbarch, &ppcnbsd_sigtramp);
}

void
_initialize_ppcnbsd_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_powerpc, 0, GDB_OSABI_NETBSD_ELF,
			  ppcnbsd_init_abi);

  deprecated_add_core_fns (&ppcnbsd_core_fns);
  deprecated_add_core_fns (&ppcnbsd_elfcore_fns);
}

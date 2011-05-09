/* Target-dependent code for OpenBSD/mips64.

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
#include "gdbcore.h"
#include "osabi.h"
#include "regcache.h"
#include "regset.h"
#include "trad-frame.h"
#include "tramp-frame.h"

#include "gdb_assert.h"
#include "gdb_string.h"

#include "mips-tdep.h"
#include "obsd-tdep.h"
#include "solib-svr4.h"

#define MIPS64OBSD_NUM_REGS 73

/* Core file support.  */

/* Supply register REGNUM from the buffer specified by GREGS and LEN
   in the general-purpose register set REGSET to register cache
   REGCACHE.  If REGNUM is -1, do this for all registers in REGSET.  */

static void
mips64obsd_supply_gregset (const struct regset *regset,
			   struct regcache *regcache, int regnum,
			   const void *gregs, size_t len)
{
  const char *regs = gregs;
  int i;

  for (i = 0; i < MIPS64OBSD_NUM_REGS; i++)
    {
      if (regnum == i || regnum == -1)
	regcache_raw_supply (regcache, i, regs + i * 8);
    }
}

/* OpenBSD/mips64 register set.  */

static struct regset mips64obsd_gregset =
{
  NULL,
  mips64obsd_supply_gregset
};

/* Return the appropriate register set for the core section identified
   by SECT_NAME and SECT_SIZE.  */

static const struct regset *
mips64obsd_regset_from_core_section (struct gdbarch *gdbarch,
				     const char *sect_name, size_t sect_size)
{
  if (strcmp (sect_name, ".reg") == 0 && sect_size >= MIPS64OBSD_NUM_REGS * 8)
    return &mips64obsd_gregset;

  return NULL;
}


/* Signal trampolines.  */

static void
mips64obsd_sigframe_init (const struct tramp_frame *self,
			  struct frame_info *next_frame,
			  struct trad_frame_cache *cache,
			  CORE_ADDR func)
{
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  CORE_ADDR sp, sigcontext_addr, addr;
  int regnum;

  /* We find the appropriate instance of `struct sigcontext' at a
     fixed offset in the signal frame.  */
  sp = frame_unwind_register_signed (next_frame, MIPS_SP_REGNUM + NUM_REGS);
  sigcontext_addr = sp + 32;

  /* PC.  */
  regnum = mips_regnum (gdbarch)->pc;
  trad_frame_set_reg_addr (cache, regnum + NUM_REGS, sigcontext_addr + 16);

  /* GPRs.  */
  for (regnum = MIPS_AT_REGNUM, addr = sigcontext_addr + 32;
       regnum <= MIPS_RA_REGNUM; regnum++, addr += 8)
    trad_frame_set_reg_addr (cache, regnum + NUM_REGS, addr);

  /* HI and LO.  */
  regnum = mips_regnum (gdbarch)->lo;
  trad_frame_set_reg_addr (cache, regnum + NUM_REGS, sigcontext_addr + 280);
  regnum = mips_regnum (gdbarch)->hi;
  trad_frame_set_reg_addr (cache, regnum + NUM_REGS, sigcontext_addr + 288);

  /* TODO: Handle the floating-point registers.  */

  trad_frame_set_id (cache, frame_id_build (sp, func));
}

static const struct tramp_frame mips64obsd_sigframe =
{
  SIGTRAMP_FRAME,
  MIPS_INSN32_SIZE,
  {
    { 0x67a40020, -1 },		/* daddiu  a0,sp,32 */
    { 0x24020067, -1 },		/* li      v0,103 */
    { 0x0000000c, -1 },		/* syscall */
    { 0x0000000d, -1 },		/* break */
    { TRAMP_SENTINEL_INSN, -1 }
  },
  mips64obsd_sigframe_init
};


/* Check the code at PC for a dynamic linker lazy resolution stub.  Because
   they aren't in the .plt section, we pattern-match on the code generated
   by GNU ld.  They look like this:

   ld t9,0x8010(gp)
   daddu t7,ra
   jalr t9,ra
   daddiu t8,zero,INDEX

   Also return the dynamic symbol index used in the last instruction.  */

static int
mips64obsd_in_dynsym_stub (CORE_ADDR pc, char *name)
{
  unsigned char buf[28], *p;
  ULONGEST insn;

  read_memory (pc - 12, buf, 28);

  p = buf + 12;
  while (p >= buf)
    {
      insn = extract_unsigned_integer (p, 4);
      /* ld t9,0x8010(gp) */
      if (insn == 0xdf998010)
	break;
      p -= 4;
    }
  if (p < buf)
    return 0;

  insn = extract_unsigned_integer (p + 4, 4);
  /* daddu t7,ra */
  if (insn != 0x03e0782d)
    return 0;
  
  insn = extract_unsigned_integer (p + 8, 4);
  /* jalr t9,ra */
  if (insn != 0x0320f809)
    return 0;

  insn = extract_unsigned_integer (p + 12, 4);
  /* daddiu t8,zero,0 */
  if ((insn & 0xffff0000) != 0x64180000)
    return 0;

  return (insn & 0xffff);
}

/* Return non-zero iff PC belongs to the dynamic linker resolution code
   or to a stub.  */

int
mips64obsd_in_dynsym_resolve_code (CORE_ADDR pc)
{
  /* Check whether PC is in the dynamic linker.  This also checks whether
     it is in the .plt section, which MIPS does not use.  */
  if (in_solib_dynsym_resolve_code (pc))
    return 1;

  /* Pattern match for the stub.  It would be nice if there were a more
     efficient way to avoid this check.  */
  if (mips64obsd_in_dynsym_stub (pc, NULL))
    return 1;

  return 0;
}


static void
mips64obsd_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* OpenBSD/mips64 only supports the n64 ABI, but the braindamaged
     way GDB works, forces us to pretend we can handle them all.  */

  set_gdbarch_regset_from_core_section
    (gdbarch, mips64obsd_regset_from_core_section);

  tramp_frame_prepend_unwinder (gdbarch, &mips64obsd_sigframe);

#if 0
  set_gdbarch_software_single_step (gdbarch, mips_software_single_step);
#endif

  /* OpenBSD/mips64 has SVR4-style shared libraries.  */
  set_gdbarch_in_solib_call_trampoline (gdbarch, mips64obsd_in_dynsym_stub);
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_lp64_fetch_link_map_offsets);
  set_gdbarch_skip_solib_resolver (gdbarch, obsd_skip_solib_resolver);
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_mips64obsd_tdep (void);

void
_initialize_mips64obsd_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_mips, 0, GDB_OSABI_OPENBSD_ELF,
			  mips64obsd_init_abi);
}

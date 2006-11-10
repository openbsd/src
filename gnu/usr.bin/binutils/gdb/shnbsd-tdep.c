/* Target-dependent code for SuperH running NetBSD, for GDB.
   Copyright 2002, 2003 Free Software Foundation, Inc.
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
#include "value.h"
#include "osabi.h"

#include "solib-svr4.h"

#include "nbsd-tdep.h"
#include "sh-tdep.h"
#include "shnbsd-tdep.h"

/* Convert an r0-r15 register number into an offset into a ptrace
   register structure.  */
static const int regmap[] =
{
  (20 * 4),	/* r0 */
  (19 * 4),	/* r1 */
  (18 * 4),	/* r2 */ 
  (17 * 4),	/* r3 */ 
  (16 * 4),	/* r4 */
  (15 * 4),	/* r5 */
  (14 * 4),	/* r6 */
  (13 * 4),	/* r7 */
  (12 * 4),	/* r8 */ 
  (11 * 4),	/* r9 */
  (10 * 4),	/* r10 */
  ( 9 * 4),	/* r11 */
  ( 8 * 4),	/* r12 */
  ( 7 * 4),	/* r13 */
  ( 6 * 4),	/* r14 */
  ( 5 * 4),	/* r15 */
};

#define SIZEOF_STRUCT_REG (21 * 4)

void
shnbsd_supply_reg (char *regs, int regno)
{
  int i;

  if (regno == PC_REGNUM || regno == -1)
    regcache_raw_supply (current_regcache, PC_REGNUM, regs + (0 * 4));

  if (regno == SR_REGNUM || regno == -1)
    regcache_raw_supply (current_regcache, SR_REGNUM, regs + (1 * 4));

  if (regno == PR_REGNUM || regno == -1)
    regcache_raw_supply (current_regcache, PR_REGNUM, regs + (2 * 4));

  if (regno == MACH_REGNUM || regno == -1)
    regcache_raw_supply (current_regcache, MACH_REGNUM, regs + (3 * 4));

  if (regno == MACL_REGNUM || regno == -1)
    regcache_raw_supply (current_regcache, MACL_REGNUM, regs + (4 * 4));

  if ((regno >= R0_REGNUM && regno <= (R0_REGNUM + 15)) || regno == -1)
    {
      for (i = R0_REGNUM; i <= (R0_REGNUM + 15); i++)
	if (regno == i || regno == -1)
          regcache_raw_supply (current_regcache, i,
			       regs + regmap[i - R0_REGNUM]);
    }
}

void
shnbsd_fill_reg (char *regs, int regno)
{
  int i;

  if (regno == PC_REGNUM || regno == -1)
    regcache_raw_collect (current_regcache, PC_REGNUM, regs + (0 * 4));

  if (regno == SR_REGNUM || regno == -1)
    regcache_raw_collect (current_regcache, SR_REGNUM, regs + (1 * 4));

  if (regno == PR_REGNUM || regno == -1)
    regcache_raw_collect (current_regcache, PR_REGNUM, regs + (2 * 4));

  if (regno == MACH_REGNUM || regno == -1)
    regcache_raw_collect (current_regcache, MACH_REGNUM, regs + (3 * 4));

  if (regno == MACL_REGNUM || regno == -1)
    regcache_raw_collect (current_regcache, MACL_REGNUM, regs + (4 * 4));

  if ((regno >= R0_REGNUM && regno <= (R0_REGNUM + 15)) || regno == -1)
    {
      for (i = R0_REGNUM; i <= (R0_REGNUM + 15); i++)
	if (regno == i || regno == -1)
          regcache_raw_collect (current_regcache, i,
				regs + regmap[i - R0_REGNUM]);
    }
}

static void
fetch_core_registers (char *core_reg_sect, unsigned core_reg_size,
                      int which, CORE_ADDR ignore)
{
  /* We get everything from the .reg section.  */
  if (which != 0)
    return;

  if (core_reg_size < SIZEOF_STRUCT_REG)
    {
      warning ("Wrong size register set in core file.");
      return;
    }

  /* Integer registers.  */
  shnbsd_supply_reg (core_reg_sect, -1);
}

static void
fetch_elfcore_registers (char *core_reg_sect, unsigned core_reg_size,
                         int which, CORE_ADDR ignore)
{
  switch (which)
    {
    case 0:  /* Integer registers.  */
      if (core_reg_size != SIZEOF_STRUCT_REG)
	warning ("Wrong size register set in core file.");
      else
	shnbsd_supply_reg (core_reg_sect, -1);
      break;

    default:
      /* Don't know what kind of register request this is; just ignore it.  */
      break;
    }
}

static struct core_fns shnbsd_core_fns =
{
  bfd_target_unknown_flavour,		/* core_flavour */
  default_check_format,			/* check_format */
  default_core_sniffer,			/* core_sniffer */
  fetch_core_registers,			/* core_read_registers */
  NULL					/* next */
};

static struct core_fns shnbsd_elfcore_fns =
{
  bfd_target_elf_flavour,		/* core_flavour */
  default_check_format,			/* check_format */
  default_core_sniffer,			/* core_sniffer */
  fetch_elfcore_registers,		/* core_read_registers */
  NULL					/* next */
};

static void
shnbsd_init_abi (struct gdbarch_info info,
                  struct gdbarch *gdbarch)
{
  set_solib_svr4_fetch_link_map_offsets (gdbarch,
		                nbsd_ilp32_solib_svr4_fetch_link_map_offsets);
}

void
_initialize_shnbsd_tdep (void)
{
  deprecated_add_core_fns (&shnbsd_core_fns);
  deprecated_add_core_fns (&shnbsd_elfcore_fns);

  gdbarch_register_osabi (bfd_arch_sh, 0, GDB_OSABI_NETBSD_ELF,
			  shnbsd_init_abi);
  gdbarch_register_osabi (bfd_arch_sh, 0, GDB_OSABI_OPENBSD_ELF,
			  shnbsd_init_abi);
}

/* Target-dependent code for HPUX running on PA-RISC, for GDB.

   Copyright 2002, 2003 Free Software Foundation, Inc.

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

#include "defs.h"
#include "arch-utils.h"
#include "gdbcore.h"
#include "osabi.h"
#include "gdb_string.h"
#include "frame.h"

/* Forward declarations.  */
extern void _initialize_hppa_hpux_tdep (void);
extern initialize_file_ftype _initialize_hppa_hpux_tdep;

/* FIXME: brobecker 2002-12-25.  The following functions will eventually
   become static, after the multiarching conversion is done.  */
int hppa_hpux_pc_in_sigtramp (CORE_ADDR pc, char *name);
void hppa32_hpux_frame_saved_pc_in_sigtramp (struct frame_info *fi,
                                             CORE_ADDR *tmp);
void hppa32_hpux_frame_base_before_sigtramp (struct frame_info *fi,
                                             CORE_ADDR *tmp);
void hppa32_hpux_frame_find_saved_regs_in_sigtramp (struct frame_info *fi,
                                                    CORE_ADDR *fsr);
void hppa64_hpux_frame_saved_pc_in_sigtramp (struct frame_info *fi,
                                             CORE_ADDR *tmp);
void hppa64_hpux_frame_base_before_sigtramp (struct frame_info *fi,
                                             CORE_ADDR *tmp);
void hppa64_hpux_frame_find_saved_regs_in_sigtramp (struct frame_info *fi,
                                                    CORE_ADDR *fsr);

int
hppa_hpux_pc_in_sigtramp (CORE_ADDR pc, char *name)
{
  /* Actually, for a PA running HPUX the kernel calls the signal handler
     without an intermediate trampoline.  Luckily the kernel always sets
     the return pointer for the signal handler to point to _sigreturn.  */
  return (name && (strcmp ("_sigreturn", name) == 0));
}

/* For hppa32_hpux_frame_saved_pc_in_sigtramp, 
   hppa32_hpux_frame_base_before_sigtramp and
   hppa32_hpux_frame_find_saved_regs_in_sigtramp:

   The signal context structure pointer is always saved at the base
   of the frame which "calls" the signal handler.  We only want to find
   the hardware save state structure, which lives 10 32bit words into
   sigcontext structure.

   Within the hardware save state structure, registers are found in the
   same order as the register numbers in GDB.

   At one time we peeked at %r31 rather than the PC queues to determine
   what instruction took the fault.  This was done on purpose, but I don't
   remember why.  Looking at the PC queues is really the right way, and
   I don't remember why that didn't work when this code was originally
   written.  */

void
hppa32_hpux_frame_saved_pc_in_sigtramp (struct frame_info *fi, CORE_ADDR *tmp)
{
  *tmp = read_memory_integer (get_frame_base (fi) + (43 * 4), 4);
}

void
hppa32_hpux_frame_base_before_sigtramp (struct frame_info *fi,
                                        CORE_ADDR *tmp)
{
  *tmp = read_memory_integer (get_frame_base (fi) + (40 * 4), 4);
}

void
hppa32_hpux_frame_find_saved_regs_in_sigtramp (struct frame_info *fi,
					       CORE_ADDR *fsr)
{
  int i;
  const CORE_ADDR tmp = get_frame_base (fi) + (10 * 4);

  for (i = 0; i < NUM_REGS; i++)
    {
      if (i == SP_REGNUM)
	fsr[SP_REGNUM] = read_memory_integer (tmp + SP_REGNUM * 4, 4);
      else
	fsr[i] = tmp + i * 4;
    }
}

/* For hppa64_hpux_frame_saved_pc_in_sigtramp, 
   hppa64_hpux_frame_base_before_sigtramp and
   hppa64_hpux_frame_find_saved_regs_in_sigtramp:

   These functions are the PA64 ABI equivalents of the 32bits counterparts
   above. See the comments there.

   For PA64, the save_state structure is at an offset of 24 32-bit words
   from the sigcontext structure. The 64 bit general registers are at an
   offset of 640 bytes from the beginning of the save_state structure,
   and the floating pointer register are at an offset of 256 bytes from
   the beginning of the save_state structure.  */

void
hppa64_hpux_frame_saved_pc_in_sigtramp (struct frame_info *fi, CORE_ADDR *tmp)
{
  *tmp = read_memory_integer
           (get_frame_base (fi) + (24 * 4) + 640 + (33 * 8), 8);
}

void
hppa64_hpux_frame_base_before_sigtramp (struct frame_info *fi,
                                        CORE_ADDR *tmp)
{
  *tmp = read_memory_integer
           (get_frame_base (fi) + (24 * 4) + 640 + (30 * 8), 8);
}

void
hppa64_hpux_frame_find_saved_regs_in_sigtramp (struct frame_info *fi,
					       CORE_ADDR *fsr)
{
  int i;
  const CORE_ADDR tmp1 = get_frame_base (fi) + (24 * 4) + 640;
  const CORE_ADDR tmp2 = get_frame_base (fi) + (24 * 4) + 256;

  for (i = 0; i < NUM_REGS; i++)
    {
      if (i == SP_REGNUM)
        fsr[SP_REGNUM] = read_memory_integer (tmp1 + SP_REGNUM * 8, 8);
      else if (i >= FP0_REGNUM)
        fsr[i] = tmp2 + (i - FP0_REGNUM) * 8;
      else
        fsr[i] = tmp1 + i * 8;
    }
}

static void
hppa_hpux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  set_gdbarch_pc_in_sigtramp (gdbarch, hppa_hpux_pc_in_sigtramp);
}

static void
hppa_hpux_som_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  hppa_hpux_init_abi (info, gdbarch);
}

static void
hppa_hpux_elf_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  hppa_hpux_init_abi (info, gdbarch);
}

void
_initialize_hppa_hpux_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_hppa, 0, GDB_OSABI_HPUX_SOM,
                          hppa_hpux_som_init_abi);
  gdbarch_register_osabi (bfd_arch_hppa, bfd_mach_hppa20w, GDB_OSABI_HPUX_ELF,
                          hppa_hpux_elf_init_abi);
}

/* Common target dependent code for the Motorola 68000 series.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1999, 2000, 2001, 2003
   Free Software Foundation, Inc.

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

#ifndef M68K_TDEP_H
#define M68K_TDEP_H

struct frame_info;

/* Register numbers of various important registers.
   Note that some of these values are "real" register numbers,
   and correspond to the general registers of the machine,
   and some are "phony" register numbers which are too large
   to be actual register numbers as far as the user is concerned
   but do serve to get the desired values when passed to read_register.  */

enum
{
  M68K_D0_REGNUM = 0,
  M68K_D1_REGNUM = 1,
  M68K_A0_REGNUM = 8,
  M68K_A1_REGNUM = 9,
  M68K_FP_REGNUM = 14,		/* Contains address of executing stack frame */
  M68K_SP_REGNUM = 15,		/* Contains address of top of stack */
  M68K_PS_REGNUM = 16,		/* Contains processor status */
  M68K_PC_REGNUM = 17,		/* Contains program counter */
  M68K_FP0_REGNUM = 18,		/* Floating point register 0 */
  M68K_FPC_REGNUM = 26,		/* 68881 control register */
  M68K_FPS_REGNUM = 27,		/* 68881 status register */
  M68K_FPI_REGNUM = 28
};

#define M68K_NUM_REGS (M68K_FPI_REGNUM + 1)

/* Size of the largest register.  */
#define M68K_MAX_REGISTER_SIZE	12

struct m68k_sigtramp_info
{
  /* Address of sigcontext.  */
  CORE_ADDR sigcontext_addr;

  /* Offset of registers in `struct sigcontext'.  */
  int *sc_reg_offset;
};

/* Convention for returning structures.  */

enum struct_return
{
  pcc_struct_return,		/* Return "short" structures in memory.  */
  reg_struct_return		/* Return "short" structures in registers.  */
};

/* Target-dependent structure in gdbarch.  */
struct gdbarch_tdep
{
  /* Offset to PC value in the jump buffer.  If this is negative,
     longjmp support will be disabled.  */
  int jb_pc;
  /* The size of each entry in the jump buffer.  */
  size_t jb_elt_size;

  /* Get info about sigtramp.  */
  struct m68k_sigtramp_info (*get_sigtramp_info) (struct frame_info *);

  /* Convention for returning structures.  */
  enum struct_return struct_return;
};

#endif /* M68K_TDEP_H */

/* GNU/Linux/MIPS specific low level interface, for the remote server for GDB.
   Copyright 1995, 1996, 1998, 1999, 2000, 2001, 2002
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

#include "server.h"
#include "linux-low.h"

#ifdef HAVE_SYS_REG_H
#include <sys/reg.h>
#endif

#define mips_num_regs 90

#include <asm/ptrace.h>

/* Return the ptrace ``address'' of register REGNO. */

/* Matches mips_generic32_regs */
static int mips_regmap[] = {
  0,  1,  2,  3,  4,  5,  6,  7,
  8,  9,  10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23,
  24, 25, 26, 27, 28, 29, 30, 31,

  -1, MMLO, MMHI, BADVADDR, CAUSE, PC,

  FPR_BASE,      FPR_BASE + 1,  FPR_BASE + 2,  FPR_BASE + 3,
  FPR_BASE + 4,  FPR_BASE + 5,  FPR_BASE + 6,  FPR_BASE + 7,
  FPR_BASE + 8,  FPR_BASE + 8,  FPR_BASE + 10, FPR_BASE + 11,
  FPR_BASE + 12, FPR_BASE + 13, FPR_BASE + 14, FPR_BASE + 15,
  FPR_BASE + 16, FPR_BASE + 17, FPR_BASE + 18, FPR_BASE + 19,
  FPR_BASE + 20, FPR_BASE + 21, FPR_BASE + 22, FPR_BASE + 23,
  FPR_BASE + 24, FPR_BASE + 25, FPR_BASE + 26, FPR_BASE + 27,
  FPR_BASE + 28, FPR_BASE + 29, FPR_BASE + 30, FPR_BASE + 31,
  FPC_CSR, FPC_EIR,

  -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,
};

/* From mips-linux-nat.c.  */

/* Pseudo registers can not be read.  ptrace does not provide a way to
   read (or set) PS_REGNUM, and there's no point in reading or setting
   ZERO_REGNUM.  We also can not set BADVADDR, CAUSE, or FCRIR via
   ptrace().  */

static int
mips_cannot_fetch_register (int regno)
{
  if (mips_regmap[regno] == -1)
    return 1;

  if (find_regno ("zero") == regno)
    return 1;

  return 0;
}

static int
mips_cannot_store_register (int regno)
{
  if (mips_regmap[regno] == -1)
    return 1;

  if (find_regno ("zero") == regno)
    return 1;

  if (find_regno ("cause") == regno)
    return 1;

  if (find_regno ("bad") == regno)
    return 1;

  if (find_regno ("fir") == regno)
    return 1;

  return 0;
}

static CORE_ADDR
mips_get_pc ()
{
  unsigned long pc;
  collect_register_by_name ("pc", &pc);
  return pc;
}

static void
mips_set_pc (CORE_ADDR pc)
{
  unsigned long newpc = pc;
  supply_register_by_name ("pc", &newpc);
}

/* Correct in either endianness.  */
static const unsigned long mips_breakpoint = 0x0005000d;
#define mips_breakpoint_len 4

/* We only place breakpoints in empty marker functions, and thread locking
   is outside of the function.  So rather than importing software single-step,
   we can just run until exit.  */
static CORE_ADDR
mips_reinsert_addr ()
{
  unsigned long pc;
  collect_register_by_name ("ra", &pc);
  return pc;
}

static int
mips_breakpoint_at (CORE_ADDR where)
{
  unsigned long insn;

  (*the_target->read_memory) (where, (char *) &insn, 4);
  if (insn == mips_breakpoint)
    return 1;

  /* If necessary, recognize more trap instructions here.  GDB only uses the
     one.  */
  return 0;
}

struct linux_target_ops the_low_target = {
  mips_num_regs,
  mips_regmap,
  mips_cannot_fetch_register,
  mips_cannot_store_register,
  mips_get_pc,
  mips_set_pc,
  (const char *) &mips_breakpoint,
  mips_breakpoint_len,
  mips_reinsert_addr,
  0,
  mips_breakpoint_at,
};

/* GNU/Linux/x86-64 specific low level interface, for the remote server
   for GDB.
   Copyright 2002
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
#include "i387-fp.h"

#include <sys/reg.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>

static int x86_64_regmap[] = {
  RAX * 8, RBX * 8, RCX * 8, RDX * 8,
  RSI * 8, RDI * 8, RBP * 8, RSP * 8,
  R8 * 8, R9 * 8, R10 * 8, R11 * 8,
  R12 * 8, R13 * 8, R14 * 8, R15 * 8,
  RIP * 8, EFLAGS * 8, CS * 8, SS * 8, 
  DS * 8, ES * 8, FS * 8, GS * 8
};

#define X86_64_NUM_GREGS (sizeof(x86_64_regmap)/sizeof(int))

static void
x86_64_fill_gregset (void *buf)
{
  int i;

  for (i = 0; i < X86_64_NUM_GREGS; i++)
    collect_register (i, ((char *) buf) + x86_64_regmap[i]);
}

static void
x86_64_store_gregset (const void *buf)
{
  int i;

  for (i = 0; i < X86_64_NUM_GREGS; i++)
    supply_register (i, ((char *) buf) + x86_64_regmap[i]);
}

static void
x86_64_fill_fpregset (void *buf)
{
  i387_cache_to_fxsave (buf);
}

static void
x86_64_store_fpregset (const void *buf)
{
  i387_fxsave_to_cache (buf);
}

struct regset_info target_regsets[] = {
  { PTRACE_GETREGS, PTRACE_SETREGS, sizeof (elf_gregset_t),
    GENERAL_REGS,
    x86_64_fill_gregset, x86_64_store_gregset },
  { PTRACE_GETFPREGS, PTRACE_SETFPREGS, sizeof (elf_fpregset_t),
    FP_REGS,
    x86_64_fill_fpregset, x86_64_store_fpregset },
  { 0, 0, -1, -1, NULL, NULL }
};

struct linux_target_ops the_low_target = {
  -1,
  NULL,
  NULL,
  NULL,
};

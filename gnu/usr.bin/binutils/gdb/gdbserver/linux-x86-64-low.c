/* GNU/Linux/x86-64 specific low level interface, for the remote server
   for GDB.
   Copyright 2002, 2004
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

/* Correct for all GNU/Linux targets (for quite some time).  */
#define GDB_GREGSET_T elf_gregset_t
#define GDB_FPREGSET_T elf_fpregset_t

#ifndef HAVE_ELF_FPREGSET_T
/* Make sure we have said types.  Not all platforms bring in <linux/elf.h>
   via <sys/procfs.h>.  */
#ifdef HAVE_LINUX_ELF_H   
#include <linux/elf.h>    
#endif
#endif
   
#include "../gdb_proc_service.h"

#include <sys/reg.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>

/* This definition comes from prctl.h, but some kernels may not have it.  */
#ifndef PTRACE_ARCH_PRCTL
#define PTRACE_ARCH_PRCTL      30
#endif

/* The following definitions come from prctl.h, but may be absent
   for certain configurations.  */
#ifndef ARCH_GET_FS
#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_GET_GS 0x1004
#endif

static int x86_64_regmap[] = {
  RAX * 8, RBX * 8, RCX * 8, RDX * 8,
  RSI * 8, RDI * 8, RBP * 8, RSP * 8,
  R8 * 8, R9 * 8, R10 * 8, R11 * 8,
  R12 * 8, R13 * 8, R14 * 8, R15 * 8,
  RIP * 8, EFLAGS * 8, CS * 8, SS * 8, 
  DS * 8, ES * 8, FS * 8, GS * 8
};

#define X86_64_NUM_GREGS (sizeof(x86_64_regmap)/sizeof(int))

/* Called by libthread_db.  */

ps_err_e
ps_get_thread_area (const struct ps_prochandle *ph,
                    lwpid_t lwpid, int idx, void **base)
{
  switch (idx)
    {
    case FS:
      if (ptrace (PTRACE_ARCH_PRCTL, lwpid, base, ARCH_GET_FS) == 0)
	return PS_OK;
      break;
    case GS:
      if (ptrace (PTRACE_ARCH_PRCTL, lwpid, base, ARCH_GET_GS) == 0)
	return PS_OK;
      break;
    default:
      return PS_BADADDR;
    }
  return PS_ERR;
}

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

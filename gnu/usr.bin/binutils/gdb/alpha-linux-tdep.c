/* Target-dependent code for GNU/Linux on Alpha.
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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "frame.h"
#include "gdb_assert.h"
#include "osabi.h"

#include "alpha-tdep.h"

/* Under GNU/Linux, signal handler invocations can be identified by
   the designated code sequence that is used to return from a signal
   handler.  In particular, the return address of a signal handler
   points to a sequence that copies $sp to $16, loads $0 with the
   appropriate syscall number, and finally enters the kernel.

   This is somewhat complicated in that:
     (1) the expansion of the "mov" assembler macro has changed over
         time, from "bis src,src,dst" to "bis zero,src,dst",
     (2) the kernel has changed from using "addq" to "lda" to load the
         syscall number,
     (3) there is a "normal" sigreturn and an "rt" sigreturn which
         has a different stack layout.
*/

static long
alpha_linux_sigtramp_offset_1 (CORE_ADDR pc)
{
  switch (alpha_read_insn (pc))
    {
    case 0x47de0410:		/* bis $30,$30,$16 */
    case 0x47fe0410:		/* bis $31,$30,$16 */
      return 0;

    case 0x43ecf400:		/* addq $31,103,$0 */
    case 0x201f0067:		/* lda $0,103($31) */
    case 0x201f015f:		/* lda $0,351($31) */
      return 4;

    case 0x00000083:		/* call_pal callsys */
      return 8;

    default:
      return -1;
    }
}

static LONGEST
alpha_linux_sigtramp_offset (CORE_ADDR pc)
{
  long i, off;

  if (pc & 3)
    return -1;

  /* Guess where we might be in the sequence.  */
  off = alpha_linux_sigtramp_offset_1 (pc);
  if (off < 0)
    return -1;

  /* Verify that the other two insns of the sequence are as we expect.  */
  pc -= off;
  for (i = 0; i < 12; i += 4)
    {
      if (i == off)
	continue;
      if (alpha_linux_sigtramp_offset_1 (pc + i) != i)
	return -1;
    }

  return off;
}

static int
alpha_linux_pc_in_sigtramp (CORE_ADDR pc, char *func_name)
{
  return alpha_linux_sigtramp_offset (pc) >= 0;
}

static CORE_ADDR
alpha_linux_sigcontext_addr (struct frame_info *next_frame)
{
  CORE_ADDR pc;
  ULONGEST sp;
  long off;

  pc = frame_pc_unwind (next_frame);
  frame_unwind_unsigned_register (next_frame, ALPHA_SP_REGNUM, &sp);

  off = alpha_linux_sigtramp_offset (pc);
  gdb_assert (off >= 0);

  /* __NR_rt_sigreturn has a couple of structures on the stack.  This is:

	struct rt_sigframe {
	  struct siginfo info;
	  struct ucontext uc;
        };

	offsetof (struct rt_sigframe, uc.uc_mcontext);
  */
  if (alpha_read_insn (pc - off + 4) == 0x201f015f)
    return sp + 176;

  /* __NR_sigreturn has the sigcontext structure at the top of the stack.  */
  return sp;
}

static void
alpha_linux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep;

  /* Hook into the DWARF CFI frame unwinder.  */
  alpha_dwarf2_init_abi (info, gdbarch);

  /* Hook into the MDEBUG frame unwinder.  */
  alpha_mdebug_init_abi (info, gdbarch);

  set_gdbarch_pc_in_sigtramp (gdbarch, alpha_linux_pc_in_sigtramp);

  tdep = gdbarch_tdep (gdbarch);
  tdep->dynamic_sigtramp_offset = alpha_linux_sigtramp_offset;
  tdep->sigcontext_addr = alpha_linux_sigcontext_addr;
  tdep->jb_pc = 2;
  tdep->jb_elt_size = 8;
}

void
_initialize_alpha_linux_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_alpha, 0, GDB_OSABI_LINUX,
                          alpha_linux_init_abi);
}

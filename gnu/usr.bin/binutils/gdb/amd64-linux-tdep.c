/* Target-dependent code for GNU/Linux x86-64.

   Copyright 2001, 2003, 2004 Free Software Foundation, Inc.
   Contributed by Jiri Smid, SuSE Labs.

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
#include "gdbcore.h"
#include "regcache.h"
#include "osabi.h"
#include "symtab.h"

#include "gdb_string.h"

#include "amd64-tdep.h"
#include "solib-svr4.h"

/* Mapping between the general-purpose registers in `struct user'
   format and GDB's register cache layout.  */

/* From <sys/reg.h>.  */
static int amd64_linux_gregset_reg_offset[] =
{
  10 * 8,			/* %rax */
  5 * 8,			/* %rbx */
  11 * 8,			/* %rcx */
  12 * 8,			/* %rdx */
  13 * 8,			/* %rsi */
  14 * 8,			/* %rdi */
  4 * 8,			/* %rbp */
  19 * 8,			/* %rsp */
  9 * 8,			/* %r8 ... */
  8 * 8,
  7 * 8,
  6 * 8,
  3 * 8,
  2 * 8,
  1 * 8,
  0 * 8,			/* ... %r15 */
  16 * 8,			/* %rip */
  18 * 8,			/* %eflags */
  17 * 8,			/* %cs */
  20 * 8,			/* %ss */
  23 * 8,			/* %ds */
  24 * 8,			/* %es */
  25 * 8,			/* %fs */
  26 * 8			/* %gs */
};


/* Support for signal handlers.  */

#define LINUX_SIGTRAMP_INSN0	0x48	/* mov $NNNNNNNN, %rax */
#define LINUX_SIGTRAMP_OFFSET0	0
#define LINUX_SIGTRAMP_INSN1	0x0f	/* syscall */
#define LINUX_SIGTRAMP_OFFSET1	7

static const unsigned char linux_sigtramp_code[] =
{
  /* mov $__NR_rt_sigreturn, %rax */
  LINUX_SIGTRAMP_INSN0, 0xc7, 0xc0, 0x0f, 0x00, 0x00, 0x00,
  /* syscall */
  LINUX_SIGTRAMP_INSN1, 0x05
};

#define LINUX_SIGTRAMP_LEN (sizeof linux_sigtramp_code)

/* If PC is in a sigtramp routine, return the address of the start of
   the routine.  Otherwise, return 0.  */

static CORE_ADDR
amd64_linux_sigtramp_start (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  unsigned char buf[LINUX_SIGTRAMP_LEN];

  /* We only recognize a signal trampoline if PC is at the start of
     one of the two instructions.  We optimize for finding the PC at
     the start, as will be the case when the trampoline is not the
     first frame on the stack.  We assume that in the case where the
     PC is not at the start of the instruction sequence, there will be
     a few trailing readable bytes on the stack.  */

  if (!safe_frame_unwind_memory (next_frame, pc, buf, LINUX_SIGTRAMP_LEN))
    return 0;

  if (buf[0] != LINUX_SIGTRAMP_INSN0)
    {
      if (buf[0] != LINUX_SIGTRAMP_INSN1)
	return 0;

      pc -= LINUX_SIGTRAMP_OFFSET1;

      if (!safe_frame_unwind_memory (next_frame, pc, buf, LINUX_SIGTRAMP_LEN))
	return 0;
    }

  if (memcmp (buf, linux_sigtramp_code, LINUX_SIGTRAMP_LEN) != 0)
    return 0;

  return pc;
}

/* Return whether the frame preceding NEXT_FRAME corresponds to a
   GNU/Linux sigtramp routine.  */

static int
amd64_linux_sigtramp_p (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  char *name;

  find_pc_partial_function (pc, &name, NULL, NULL);

  /* If we have NAME, we can optimize the search.  The trampoline is
     named __restore_rt.  However, it isn't dynamically exported from
     the shared C library, so the trampoline may appear to be part of
     the preceding function.  This should always be sigaction,
     __sigaction, or __libc_sigaction (all aliases to the same
     function).  */
  if (name == NULL || strstr (name, "sigaction") != NULL)
    return (amd64_linux_sigtramp_start (next_frame) != 0);

  return (strcmp ("__restore_rt", name) == 0);
}

/* Offset to struct sigcontext in ucontext, from <asm/ucontext.h>.  */
#define AMD64_LINUX_UCONTEXT_SIGCONTEXT_OFFSET 40

/* Assuming NEXT_FRAME is a frame following a GNU/Linux sigtramp
   routine, return the address of the associated sigcontext structure.  */

static CORE_ADDR
amd64_linux_sigcontext_addr (struct frame_info *next_frame)
{
  CORE_ADDR sp;
  char buf[8];

  frame_unwind_register (next_frame, SP_REGNUM, buf);
  sp = extract_unsigned_integer (buf, 8);

  /* The sigcontext structure is part of the user context.  A pointer
     to the user context is passed as the third argument to the signal
     handler, i.e. in %rdx.  Unfortunately %rdx isn't preserved across
     function calls so we can't use it.  Fortunately the user context
     is part of the signal frame and the unwound %rsp directly points
     at it.  */
  return sp + AMD64_LINUX_UCONTEXT_SIGCONTEXT_OFFSET;
}


/* From <asm/sigcontext.h>.  */
static int amd64_linux_sc_reg_offset[] =
{
  13 * 8,			/* %rax */
  11 * 8,			/* %rbx */
  14 * 8,			/* %rcx */
  12 * 8,			/* %rdx */
  9 * 8,			/* %rsi */
  8 * 8,			/* %rdi */
  10 * 8,			/* %rbp */
  15 * 8,			/* %rsp */
  0 * 8,			/* %r8 */
  1 * 8,			/* %r9 */
  2 * 8,			/* %r10 */
  3 * 8,			/* %r11 */
  4 * 8,			/* %r12 */
  5 * 8,			/* %r13 */
  6 * 8,			/* %r14 */
  7 * 8,			/* %r15 */
  16 * 8,			/* %rip */
  17 * 8,			/* %eflags */

  /* FIXME: kettenis/2002030531: The registers %cs, %fs and %gs are
     available in `struct sigcontext'.  However, they only occupy two
     bytes instead of four, which makes using them here rather
     difficult.  Leave them out for now.  */
  -1,				/* %cs */
  -1,				/* %ss */
  -1,				/* %ds */
  -1,				/* %es */
  -1,				/* %fs */
  -1				/* %gs */
};

static void
amd64_linux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  tdep->gregset_reg_offset = amd64_linux_gregset_reg_offset;
  tdep->gregset_num_regs = ARRAY_SIZE (amd64_linux_gregset_reg_offset);
  tdep->sizeof_gregset = 27 * 8;

  amd64_init_abi (info, gdbarch);

  tdep->sigtramp_p = amd64_linux_sigtramp_p;
  tdep->sigcontext_addr = amd64_linux_sigcontext_addr;
  tdep->sc_reg_offset = amd64_linux_sc_reg_offset;
  tdep->sc_num_regs = ARRAY_SIZE (amd64_linux_sc_reg_offset);

  /* GNU/Linux uses SVR4-style shared libraries.  */
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_lp64_fetch_link_map_offsets);
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
extern void _initialize_amd64_linux_tdep (void);

void
_initialize_amd64_linux_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_i386, bfd_mach_x86_64,
			  GDB_OSABI_LINUX, amd64_linux_init_abi);
}

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
#include "inferior.h"
#include "gdbcore.h"
#include "regcache.h"
#include "osabi.h"

#include "gdb_string.h"

#include "amd64-tdep.h"
#include "amd64-linux-tdep.h"

/* Register indexes to 'struct user' come from <sys/reg.h>.  */

#define USER_R15    0
#define USER_R14    1
#define USER_R13    2
#define USER_R12    3
#define USER_RBP    4
#define USER_RBX    5
#define USER_R11    6
#define USER_R10    7
#define USER_R9     8
#define USER_R8     9
#define USER_RAX    10
#define USER_RCX    11
#define USER_RDX    12
#define USER_RSI    13
#define USER_RDI    14
#define USER_RIP    16
#define USER_CS     17
#define USER_EFLAGS 18
#define USER_RSP    19
#define USER_SS     20
#define USER_DS     23
#define USER_ES     24
#define USER_FS     25
#define USER_GS     26

/* Mapping between the general-purpose registers in `struct user'
   format and GDB's register array layout.  */

static int user_to_gdb_regmap[] =
{
  USER_RAX, USER_RBX, USER_RCX, USER_RDX,
  USER_RSI, USER_RDI, USER_RBP, USER_RSP,
  USER_R8, USER_R9, USER_R10, USER_R11,
  USER_R12, USER_R13, USER_R14, USER_R15,
  USER_RIP, USER_EFLAGS,
  USER_CS, USER_SS,
  USER_DS, USER_ES, USER_FS, USER_GS
};

/* Fill GDB's register array with the general-purpose register values
   in *GREGSETP.  */

void
amd64_linux_supply_gregset (char *regp)
{
  int i;

  for (i = 0; i < AMD64_NUM_GREGS; i++)
    supply_register (i, regp + (user_to_gdb_regmap[i] * 8));
}

/* Fill register REGNO (if it is a general-purpose register) in
   *GREGSETPS with the value in GDB's register array.  If REGNO is -1,
   do this for all registers.  */

void
amd64_linux_fill_gregset (char *regp, int regno)
{
  int i;

  for (i = 0; i < AMD64_NUM_GREGS; i++)
    if (regno == -1 || regno == i)
      regcache_collect (i, regp + (user_to_gdb_regmap[i] * 8));
}

/* The register sets used in GNU/Linux ELF core-dumps are identical to
   the register sets used by `ptrace'.  The corresponding types are
   `elf_gregset_t' for the general-purpose registers (with
   `elf_greg_t' the type of a single GP register) and `elf_fpregset_t'
   for the floating-point registers.  */

static void
fetch_core_registers (char *core_reg_sect, unsigned core_reg_size,
			 int which, CORE_ADDR ignore)
{
  switch (which)
    {
    case 0:  /* Integer registers.  */
      if (core_reg_size != 216)
	warning ("Wrong size register set in core file.");
      else
	amd64_linux_supply_gregset (core_reg_sect);
      break;

    case 2:  /* Floating point registers.  */
    case 3:  /* "Extended" floating point registers.  This is gdb-speak
		for SSE/SSE2. */
      if (core_reg_size != 512)
	warning ("Wrong size XMM register set in core file.");
      else
	amd64_supply_fxsave (current_regcache, -1, core_reg_sect);
      break;

    default:
      /* Don't know what kind of register request this is; just ignore it.  */
      break;
    }
}

static struct core_fns amd64_core_fns = 
{
  bfd_target_elf_flavour,		/* core_flavour */
  default_check_format,			/* check_format */
  default_core_sniffer,			/* core_sniffer */
  fetch_core_registers,			/* core_read_registers */
  NULL					/* next */
};

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
amd64_linux_sigtramp_start (CORE_ADDR pc)
{
  unsigned char buf[LINUX_SIGTRAMP_LEN];

  /* We only recognize a signal trampoline if PC is at the start of
     one of the two instructions.  We optimize for finding the PC at
     the start, as will be the case when the trampoline is not the
     first frame on the stack.  We assume that in the case where the
     PC is not at the start of the instruction sequence, there will be
     a few trailing readable bytes on the stack.  */

  if (read_memory_nobpt (pc, (char *) buf, LINUX_SIGTRAMP_LEN) != 0)
    return 0;

  if (buf[0] != LINUX_SIGTRAMP_INSN0)
    {
      if (buf[0] != LINUX_SIGTRAMP_INSN1)
	return 0;

      pc -= LINUX_SIGTRAMP_OFFSET1;

      if (read_memory_nobpt (pc, (char *) buf, LINUX_SIGTRAMP_LEN) != 0)
	return 0;
    }

  if (memcmp (buf, linux_sigtramp_code, LINUX_SIGTRAMP_LEN) != 0)
    return 0;

  return pc;
}

/* Return whether PC is in a GNU/Linux sigtramp routine.  */

static int
amd64_linux_pc_in_sigtramp (CORE_ADDR pc, char *name)
{
  /* If we have NAME, we can optimize the search.  The trampoline is
     named __restore_rt.  However, it isn't dynamically exported from
     the shared C library, so the trampoline may appear to be part of
     the preceding function.  This should always be sigaction,
     __sigaction, or __libc_sigaction (all aliases to the same
     function).  */
  if (name == NULL || strstr (name, "sigaction") != NULL)
    return (amd64_linux_sigtramp_start (pc) != 0);

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
  amd64_init_abi (info, gdbarch);

  set_gdbarch_pc_in_sigtramp (gdbarch, amd64_linux_pc_in_sigtramp);

  tdep->sigcontext_addr = amd64_linux_sigcontext_addr;
  tdep->sc_reg_offset = amd64_linux_sc_reg_offset;
  tdep->sc_num_regs = ARRAY_SIZE (amd64_linux_sc_reg_offset);
}


/* Provide a prototype to silence -Wmissing-prototypes.  */
extern void _initialize_amd64_linux_tdep (void);

void
_initialize_amd64_linux_tdep (void)
{
  add_core_fns (&amd64_core_fns);

  gdbarch_register_osabi (bfd_arch_i386, bfd_mach_x86_64,
			  GDB_OSABI_LINUX, amd64_linux_init_abi);
}

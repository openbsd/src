/* Target-dependent code for GNU/Linux running on the Fujitsu FR-V,
   for GDB.
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
#include "target.h"
#include "frame.h"
#include "osabi.h"
#include "elf-bfd.h"
#include "elf/frv.h"
#include "frv-tdep.h"
#include "trad-frame.h"
#include "frame-unwind.h"

/* Define the size (in bytes) of an FR-V instruction.  */
static const int frv_instr_size = 4;

enum {
  NORMAL_SIGTRAMP = 1,
  RT_SIGTRAMP = 2
};

static int
frv_linux_pc_in_sigtramp (CORE_ADDR pc, char *name)
{
  char buf[frv_instr_size];
  LONGEST instr;
  int retval = 0;

  if (target_read_memory (pc, buf, sizeof buf) != 0)
    return 0;

  instr = extract_unsigned_integer (buf, sizeof buf);

  if (instr == 0x8efc0077)	/* setlos #__NR_sigreturn, gr7 */
    retval = NORMAL_SIGTRAMP;
  else if (instr -= 0x8efc00ad)	/* setlos #__NR_rt_sigreturn, gr7 */
    retval = RT_SIGTRAMP;
  else
    return 0;

  if (target_read_memory (pc + frv_instr_size, buf, sizeof buf) != 0)
    return 0;
  instr = extract_unsigned_integer (buf, sizeof buf);
  if (instr != 0xc0700000)	/* tira	gr0, 0 */
    return 0;

  /* If we get this far, we'll return a non-zero value, either
     NORMAL_SIGTRAMP (1) or RT_SIGTRAMP (2).  */
  return retval;
}

/* Given NEXT_FRAME, "callee" frame of the sigtramp frame that we
   wish to decode, and REGNO, one of the frv register numbers defined
   in frv-tdep.h, return the address of the saved register (corresponding
   to REGNO) in the sigtramp frame.  Return -1 if the register is not
   found in the sigtramp frame.  The magic numbers in the code below
   were computed by examining the following kernel structs:

   From arch/frvnommu/signal.c:

      struct sigframe
      {
	      void (*pretcode)(void);
	      int sig;
	      struct sigcontext sc;
	      unsigned long extramask[_NSIG_WORDS-1];
	      uint32_t retcode[2];
      };

      struct rt_sigframe
      {
	      void (*pretcode)(void);
	      int sig;
	      struct siginfo *pinfo;
	      void *puc;
	      struct siginfo info;
	      struct ucontext uc;
	      uint32_t retcode[2];
      };

   From include/asm-frvnommu/ucontext.h:

      struct ucontext {
	      unsigned long		uc_flags;
	      struct ucontext		*uc_link;
	      stack_t			uc_stack;
	      struct sigcontext	uc_mcontext;
	      sigset_t		uc_sigmask;
      };

   From include/asm-frvnommu/sigcontext.h:

      struct sigcontext {
	      struct user_context	sc_context;
	      unsigned long		sc_oldmask;
      } __attribute__((aligned(8)));

   From include/asm-frvnommu/registers.h:
      struct user_int_regs
      {
	      unsigned long		psr;
	      unsigned long		isr;
	      unsigned long		ccr;
	      unsigned long		cccr;
	      unsigned long		lr;
	      unsigned long		lcr;
	      unsigned long		pc;
	      unsigned long		__status;
	      unsigned long		syscallno;
	      unsigned long		orig_gr8;
	      unsigned long		gner[2];
	      unsigned long long	iacc[1];

	      union {
		      unsigned long	tbr;
		      unsigned long	gr[64];
	      };
      };

      struct user_fpmedia_regs
      {
	      unsigned long	fr[64];
	      unsigned long	fner[2];
	      unsigned long	msr[2];
	      unsigned long	acc[8];
	      unsigned char	accg[8];
	      unsigned long	fsr[1];
      };

      struct user_context
      {
	      struct user_int_regs		i;
	      struct user_fpmedia_regs	f;

	      void *extension;
      } __attribute__((aligned(8)));  */

static LONGEST
frv_linux_sigcontext_reg_addr (struct frame_info *next_frame, int regno,
                               CORE_ADDR *sc_addr_cache_ptr)
{
  CORE_ADDR sc_addr;

  if (sc_addr_cache_ptr && *sc_addr_cache_ptr)
    {
      sc_addr = *sc_addr_cache_ptr;
    }
  else
    {
      CORE_ADDR pc, sp;
      char buf[4];
      int tramp_type;

      pc = frame_pc_unwind (next_frame);
      tramp_type = frv_linux_pc_in_sigtramp (pc, 0);

      frame_unwind_register (next_frame, sp_regnum, buf);
      sp = extract_unsigned_integer (buf, sizeof buf);

      if (tramp_type == NORMAL_SIGTRAMP)
	{
	  /* For a normal sigtramp frame, the sigcontext struct starts
	     at SP + 8.  */
	  sc_addr = sp + 8;
	}
      else if (tramp_type == RT_SIGTRAMP)
	{
	  /* For a realtime sigtramp frame, SP + 12 contains a pointer
	     to the a ucontext struct.  The ucontext struct contains
	     a sigcontext struct starting 12 bytes in.  */
	  if (target_read_memory (sp + 12, buf, sizeof buf) != 0)
	    {
	      warning ("Can't read realtime sigtramp frame.");
	      return 0;
	    }
	  sc_addr = extract_unsigned_integer (buf, sizeof buf);
	  sc_addr += 12;
	}
      else
	internal_error (__FILE__, __LINE__, "not a signal trampoline");

      if (sc_addr_cache_ptr)
	*sc_addr_cache_ptr = sc_addr;
    }

  switch (regno)
    {
    case psr_regnum :
      return sc_addr + 0;
    /* sc_addr + 4 has "isr", the Integer Status Register.  */
    case ccr_regnum :
      return sc_addr + 8;
    case cccr_regnum :
      return sc_addr + 12;
    case lr_regnum :
      return sc_addr + 16;
    case lcr_regnum :
      return sc_addr + 20;
    case pc_regnum :
      return sc_addr + 24;
    /* sc_addr + 28 is __status, the exception status.
       sc_addr + 32 is syscallno, the syscall number or -1.
       sc_addr + 36 is orig_gr8, the original syscall arg #1.
       sc_addr + 40 is gner[0].
       sc_addr + 44 is gner[1]. */
    case iacc0h_regnum :
      return sc_addr + 48;
    case iacc0l_regnum :
      return sc_addr + 52;
    default : 
      if (first_gpr_regnum <= regno && regno <= last_gpr_regnum)
	return sc_addr + 56 + 4 * (regno - first_gpr_regnum);
      else if (first_fpr_regnum <= regno && regno <= last_fpr_regnum)
	return sc_addr + 312 + 4 * (regno - first_fpr_regnum);
      else
	return -1;  /* not saved. */
    }
}

/* Signal trampolines.  */

static struct trad_frame_cache *
frv_linux_sigtramp_frame_cache (struct frame_info *next_frame, void **this_cache)
{
  struct trad_frame_cache *cache;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);
  CORE_ADDR addr;
  char buf[4];
  int regnum;
  CORE_ADDR sc_addr_cache_val = 0;
  struct frame_id this_id;

  if (*this_cache)
    return *this_cache;

  cache = trad_frame_cache_zalloc (next_frame);

  /* FIXME: cagney/2004-05-01: This is is long standing broken code.
     The frame ID's code address should be the start-address of the
     signal trampoline and not the current PC within that
     trampoline.  */
  frame_unwind_register (next_frame, sp_regnum, buf);
  this_id = frame_id_build (extract_unsigned_integer (buf, sizeof buf),
			    frame_pc_unwind (next_frame));
  trad_frame_set_id (cache, this_id);

  for (regnum = 0; regnum < frv_num_regs; regnum++)
    {
      LONGEST reg_addr = frv_linux_sigcontext_reg_addr (next_frame, regnum,
							&sc_addr_cache_val);
      if (reg_addr != -1)
	trad_frame_set_reg_addr (cache, regnum, reg_addr);
    }

  *this_cache = cache;
  return cache;
}

static void
frv_linux_sigtramp_frame_this_id (struct frame_info *next_frame, void **this_cache,
			     struct frame_id *this_id)
{
  struct trad_frame_cache *cache =
    frv_linux_sigtramp_frame_cache (next_frame, this_cache);
  trad_frame_get_id (cache, this_id);
}

static void
frv_linux_sigtramp_frame_prev_register (struct frame_info *next_frame,
				   void **this_cache,
				   int regnum, int *optimizedp,
				   enum lval_type *lvalp, CORE_ADDR *addrp,
				   int *realnump, void *valuep)
{
  /* Make sure we've initialized the cache.  */
  struct trad_frame_cache *cache =
    frv_linux_sigtramp_frame_cache (next_frame, this_cache);
  trad_frame_get_register (cache, next_frame, regnum, optimizedp, lvalp,
			   addrp, realnump, valuep);
}

static const struct frame_unwind frv_linux_sigtramp_frame_unwind =
{
  SIGTRAMP_FRAME,
  frv_linux_sigtramp_frame_this_id,
  frv_linux_sigtramp_frame_prev_register
};

static const struct frame_unwind *
frv_linux_sigtramp_frame_sniffer (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);
  char *name;

  find_pc_partial_function (pc, &name, NULL, NULL);
  if (frv_linux_pc_in_sigtramp (pc, name))
    return &frv_linux_sigtramp_frame_unwind;

  return NULL;
}

static void
frv_linux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* Set the sigtramp frame sniffer.  */
  frame_unwind_append_sniffer (gdbarch, frv_linux_sigtramp_frame_sniffer); 
}

static enum gdb_osabi
frv_linux_elf_osabi_sniffer (bfd *abfd)
{
  int elf_flags;

  elf_flags = elf_elfheader (abfd)->e_flags;

  /* Assume GNU/Linux if using the FDPIC ABI.  If/when another OS shows
     up that uses this ABI, we'll need to start using .note sections
     or some such.  */
  if (elf_flags & EF_FRV_FDPIC)
    return GDB_OSABI_LINUX;
  else
    return GDB_OSABI_UNKNOWN;
}

/* Provide a prototype to silence -Wmissing-prototypes.  */
void _initialize_frv_linux_tdep (void);

void
_initialize_frv_linux_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_frv, 0, GDB_OSABI_LINUX, frv_linux_init_abi);
  gdbarch_register_osabi_sniffer (bfd_arch_frv,
				  bfd_target_elf_flavour,
				  frv_linux_elf_osabi_sniffer);
}

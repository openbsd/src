/* Target-dependent code for GNU/Linux running on PA-RISC, for GDB.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "gdbcore.h"
#include "osabi.h"
#include "target.h"
#include "objfiles.h"
#include "solib-svr4.h"
#include "glibc-tdep.h"
#include "frame-unwind.h"
#include "trad-frame.h"
#include "dwarf2-frame.h"
#include "value.h"
#include "hppa-tdep.h"

#include "elf/common.h"

#if 0
/* Convert DWARF register number REG to the appropriate register
   number used by GDB.  */
static int
hppa_dwarf_reg_to_regnum (int reg)
{
  /* registers 0 - 31 are the same in both sets */
  if (reg < 32)
    return reg;

  /* dwarf regs 32 to 85 are fpregs 4 - 31 */
  if (reg >= 32 && reg <= 85)
    return HPPA_FP4_REGNUM + (reg - 32);

  warning ("Unmapped DWARF Register #%d encountered\n", reg);
  return -1;
}
#endif

static void
hppa_linux_target_write_pc (CORE_ADDR v, ptid_t ptid)
{
  /* Probably this should be done by the kernel, but it isn't.  */
  write_register_pid (HPPA_PCOQ_HEAD_REGNUM, v | 0x3, ptid);
  write_register_pid (HPPA_PCOQ_TAIL_REGNUM, (v + 4) | 0x3, ptid);
}

/* An instruction to match.  */
struct insn_pattern
{
  unsigned int data;            /* See if it matches this....  */
  unsigned int mask;            /* ... with this mask.  */
};

/* See bfd/elf32-hppa.c */
static struct insn_pattern hppa_long_branch_stub[] = {
  /* ldil LR'xxx,%r1 */
  { 0x20200000, 0xffe00000 },
  /* be,n RR'xxx(%sr4,%r1) */
  { 0xe0202002, 0xffe02002 }, 
  { 0, 0 }
};

static struct insn_pattern hppa_long_branch_pic_stub[] = {
  /* b,l .+8, %r1 */
  { 0xe8200000, 0xffe00000 },
  /* addil LR'xxx - ($PIC_pcrel$0 - 4), %r1 */
  { 0x28200000, 0xffe00000 },
  /* be,n RR'xxxx - ($PIC_pcrel$0 - 8)(%sr4, %r1) */
  { 0xe0202002, 0xffe02002 }, 
  { 0, 0 }
};

static struct insn_pattern hppa_import_stub[] = {
  /* addil LR'xxx, %dp */
  { 0x2b600000, 0xffe00000 },
  /* ldw RR'xxx(%r1), %r21 */
  { 0x48350000, 0xffffb000 },
  /* bv %r0(%r21) */
  { 0xeaa0c000, 0xffffffff },
  /* ldw RR'xxx+4(%r1), %r19 */
  { 0x48330000, 0xffffb000 },
  { 0, 0 }
};

static struct insn_pattern hppa_import_pic_stub[] = {
  /* addil LR'xxx,%r19 */
  { 0x2a600000, 0xffe00000 },
  /* ldw RR'xxx(%r1),%r21 */
  { 0x48350000, 0xffffb000 },
  /* bv %r0(%r21) */
  { 0xeaa0c000, 0xffffffff },
  /* ldw RR'xxx+4(%r1),%r19 */
  { 0x48330000, 0xffffb000 },
  { 0, 0 },
};

static struct insn_pattern hppa_plt_stub[] = {
  /* b,l 1b, %r20 - 1b is 3 insns before here */
  { 0xea9f1fdd, 0xffffffff },
  /* depi 0,31,2,%r20 */
  { 0xd6801c1e, 0xffffffff },
  { 0, 0 }
};

static struct insn_pattern hppa_sigtramp[] = {
  /* ldi 0, %r25 or ldi 1, %r25 */
  { 0x34190000, 0xfffffffd },
  /* ldi __NR_rt_sigreturn, %r20 */
  { 0x3414015a, 0xffffffff },
  /* be,l 0x100(%sr2, %r0), %sr0, %r31 */
  { 0xe4008200, 0xffffffff },
  /* nop */
  { 0x08000240, 0xffffffff },
  { 0, 0 }
};

#define HPPA_MAX_INSN_PATTERN_LEN (4)

/* Return non-zero if the instructions at PC match the series
   described in PATTERN, or zero otherwise.  PATTERN is an array of
   'struct insn_pattern' objects, terminated by an entry whose mask is
   zero.

   When the match is successful, fill INSN[i] with what PATTERN[i]
   matched.  */
static int
insns_match_pattern (CORE_ADDR pc,
                     struct insn_pattern *pattern,
                     unsigned int *insn)
{
  int i;
  CORE_ADDR npc = pc;

  for (i = 0; pattern[i].mask; i++)
    {
      char buf[4];

      deprecated_read_memory_nobpt (npc, buf, 4);
      insn[i] = extract_unsigned_integer (buf, 4);
      if ((insn[i] & pattern[i].mask) == pattern[i].data)
        npc += 4;
      else
        return 0;
    }
  return 1;
}

static int
hppa_linux_in_dyncall (CORE_ADDR pc)
{
  return pc == hppa_symbol_address("$$dyncall");
}

/* There are several kinds of "trampolines" that we need to deal with:
   - long branch stubs: these are inserted by the linker when a branch
     target is too far away for a branch insn to reach
   - plt stubs: these should go into the .plt section, so are easy to find
   - import stubs: used to call from object to shared lib or shared lib to 
     shared lib; these go in regular text sections.  In fact the linker tries
     to put them throughout the code because branches have limited reachability.
     We use the same mechanism as ppc64 to recognize the stub insn patterns.
   - $$dyncall: similar to hpux, hppa-linux uses $$dyncall for indirect function
     calls. $$dyncall is exported by libgcc.a  */
static int
hppa_linux_in_solib_call_trampoline (CORE_ADDR pc, char *name)
{
  unsigned int insn[HPPA_MAX_INSN_PATTERN_LEN];
  int r;

  r = in_plt_section (pc, name)
      || hppa_linux_in_dyncall (pc)
      || insns_match_pattern (pc, hppa_import_stub, insn)
      || insns_match_pattern (pc, hppa_import_pic_stub, insn)
      || insns_match_pattern (pc, hppa_long_branch_stub, insn)
      || insns_match_pattern (pc, hppa_long_branch_pic_stub, insn);

  return r;
}

static CORE_ADDR
hppa_linux_skip_trampoline_code (CORE_ADDR pc)
{
  unsigned int insn[HPPA_MAX_INSN_PATTERN_LEN];
  int dp_rel, pic_rel;

  /* dyncall handles both PLABELs and direct addresses */
  if (hppa_linux_in_dyncall (pc))
    {
      pc = (CORE_ADDR) read_register (22);

      /* PLABELs have bit 30 set; if it's a PLABEL, then dereference it */
      if (pc & 0x2)
	pc = (CORE_ADDR) read_memory_integer (pc & ~0x3, TARGET_PTR_BIT / 8);

      return pc;
    }

  dp_rel = pic_rel = 0;
  if ((dp_rel = insns_match_pattern (pc, hppa_import_stub, insn))
      || (pic_rel = insns_match_pattern (pc, hppa_import_pic_stub, insn)))
    {
      /* Extract the target address from the addil/ldw sequence.  */
      pc = hppa_extract_21 (insn[0]) + hppa_extract_14 (insn[1]);

      if (dp_rel)
        pc += (CORE_ADDR) read_register (27);
      else
        pc += (CORE_ADDR) read_register (19);

      /* fallthrough */
    }

  if (in_plt_section (pc, NULL))
    {
      pc = (CORE_ADDR) read_memory_integer (pc, TARGET_PTR_BIT / 8);

      /* if the plt slot has not yet been resolved, the target will
         be the plt stub */
      if (in_plt_section (pc, NULL))
	{
	  /* Sanity check: are we pointing to the plt stub? */
  	  if (insns_match_pattern (pc, hppa_plt_stub, insn))
	    {
	      /* this should point to the fixup routine */
      	      pc = (CORE_ADDR) read_memory_integer (pc + 8, TARGET_PTR_BIT / 8);
	    }
	  else
	    {
	      error ("Cannot resolve plt stub at 0x%s\n",
		     paddr_nz (pc));
	      pc = 0;
	    }
	}
    }

  return pc;
}

/* Signal frames.  */

/* (This is derived from MD_FALLBACK_FRAME_STATE_FOR in gcc.)
 
   Unfortunately, because of various bugs and changes to the kernel,
   we have several cases to deal with.

   In 2.4, the signal trampoline is 4 bytes, and pc should point directly at 
   the beginning of the trampoline and struct rt_sigframe.

   In <= 2.6.5-rc2-pa3, the signal trampoline is 9 bytes, and pc points at
   the 4th word in the trampoline structure.  This is wrong, it should point 
   at the 5th word.  This is fixed in 2.6.5-rc2-pa4.

   To detect these cases, we first take pc, align it to 64-bytes
   to get the beginning of the signal frame, and then check offsets 0, 4
   and 5 to see if we found the beginning of the trampoline.  This will
   tell us how to locate the sigcontext structure.

   Note that with a 2.4 64-bit kernel, the signal context is not properly
   passed back to userspace so the unwind will not work correctly.  */
static CORE_ADDR
hppa_linux_sigtramp_find_sigcontext (CORE_ADDR pc)
{
  unsigned int dummy[HPPA_MAX_INSN_PATTERN_LEN];
  int offs = 0;
  int try;
  /* offsets to try to find the trampoline */
  static int pcoffs[] = { 0, 4*4, 5*4 };
  /* offsets to the rt_sigframe structure */
  static int sfoffs[] = { 4*4, 10*4, 10*4 };
  CORE_ADDR sp;

  /* Most of the time, this will be correct.  The one case when this will
     fail is if the user defined an alternate stack, in which case the
     beginning of the stack will not be align_down (pc, 64).  */
  sp = align_down (pc, 64);

  /* rt_sigreturn trampoline:
     3419000x ldi 0, %r25 or ldi 1, %r25   (x = 0 or 2)
     3414015a ldi __NR_rt_sigreturn, %r20 
     e4008200 be,l 0x100(%sr2, %r0), %sr0, %r31
     08000240 nop  */

  for (try = 0; try < ARRAY_SIZE (pcoffs); try++)
    {
      if (insns_match_pattern (sp + pcoffs[try], hppa_sigtramp, dummy))
	{
          offs = sfoffs[try];
	  break;
	}
    }

  if (offs == 0)
    {
      if (insns_match_pattern (pc, hppa_sigtramp, dummy))
	{
	  /* sigaltstack case: we have no way of knowing which offset to 
	     use in this case; default to new kernel handling. If this is
	     wrong the unwinding will fail.  */
	  try = 2;
	  sp = pc - pcoffs[try];
	}
      else
      {
        return 0;
      }
    }

  /* sp + sfoffs[try] points to a struct rt_sigframe, which contains
     a struct siginfo and a struct ucontext.  struct ucontext contains
     a struct sigcontext. Return an offset to this sigcontext here.  Too 
     bad we cannot include system specific headers :-(.  
     sizeof(struct siginfo) == 128
     offsetof(struct ucontext, uc_mcontext) == 24.  */
  return sp + sfoffs[try] + 128 + 24;
}

struct hppa_linux_sigtramp_unwind_cache
{
  CORE_ADDR base;
  struct trad_frame_saved_reg *saved_regs;
};

static struct hppa_linux_sigtramp_unwind_cache *
hppa_linux_sigtramp_frame_unwind_cache (struct frame_info *next_frame,
					void **this_cache)
{
  struct gdbarch *gdbarch = get_frame_arch (next_frame);
  struct hppa_linux_sigtramp_unwind_cache *info;
  CORE_ADDR pc, scptr;
  int i;

  if (*this_cache)
    return *this_cache;

  info = FRAME_OBSTACK_ZALLOC (struct hppa_linux_sigtramp_unwind_cache);
  *this_cache = info;
  info->saved_regs = trad_frame_alloc_saved_regs (next_frame);

  pc = frame_pc_unwind (next_frame);
  scptr = hppa_linux_sigtramp_find_sigcontext (pc);

  /* structure of struct sigcontext:
   
     struct sigcontext {
	unsigned long sc_flags;
	unsigned long sc_gr[32]; 
	unsigned long long sc_fr[32];
	unsigned long sc_iasq[2];
	unsigned long sc_iaoq[2];
	unsigned long sc_sar;           */

  /* Skip sc_flags.  */
  scptr += 4;

  /* GR[0] is the psw, we don't restore that.  */
  scptr += 4;

  /* General registers.  */
  for (i = 1; i < 32; i++)
    {
      info->saved_regs[HPPA_R0_REGNUM + i].addr = scptr;
      scptr += 4;
    }

  /* Pad.  */
  scptr += 4;

  /* FP regs; FP0-3 are not restored.  */
  scptr += (8 * 4);

  for (i = 4; i < 32; i++)
    {
      info->saved_regs[HPPA_FP0_REGNUM + (i * 2)].addr = scptr;
      scptr += 4;
      info->saved_regs[HPPA_FP0_REGNUM + (i * 2) + 1].addr = scptr;
      scptr += 4;
    }

  /* IASQ/IAOQ. */
  info->saved_regs[HPPA_PCSQ_HEAD_REGNUM].addr = scptr;
  scptr += 4;
  info->saved_regs[HPPA_PCSQ_TAIL_REGNUM].addr = scptr;
  scptr += 4;

  info->saved_regs[HPPA_PCOQ_HEAD_REGNUM].addr = scptr;
  scptr += 4;
  info->saved_regs[HPPA_PCOQ_TAIL_REGNUM].addr = scptr;
  scptr += 4;

  info->base = frame_unwind_register_unsigned (next_frame, HPPA_SP_REGNUM);

  return info;
}

static void
hppa_linux_sigtramp_frame_this_id (struct frame_info *next_frame,
				   void **this_prologue_cache,
				   struct frame_id *this_id)
{
  struct hppa_linux_sigtramp_unwind_cache *info
    = hppa_linux_sigtramp_frame_unwind_cache (next_frame, this_prologue_cache);
  *this_id = frame_id_build (info->base, frame_pc_unwind (next_frame));
}

static void
hppa_linux_sigtramp_frame_prev_register (struct frame_info *next_frame,
					 void **this_prologue_cache,
					 int regnum, int *optimizedp,
					 enum lval_type *lvalp, 
					 CORE_ADDR *addrp,
					 int *realnump, void *valuep)
{
  struct hppa_linux_sigtramp_unwind_cache *info
    = hppa_linux_sigtramp_frame_unwind_cache (next_frame, this_prologue_cache);
  hppa_frame_prev_register_helper (next_frame, info->saved_regs, regnum,
		                   optimizedp, lvalp, addrp, realnump, valuep);
}

static const struct frame_unwind hppa_linux_sigtramp_frame_unwind = {
  SIGTRAMP_FRAME,
  hppa_linux_sigtramp_frame_this_id,
  hppa_linux_sigtramp_frame_prev_register
};

/* hppa-linux always uses "new-style" rt-signals.  The signal handler's return
   address should point to a signal trampoline on the stack.  The signal
   trampoline is embedded in a rt_sigframe structure that is aligned on
   the stack.  We take advantage of the fact that sp must be 64-byte aligned,
   and the trampoline is small, so by rounding down the trampoline address
   we can find the beginning of the struct rt_sigframe.  */
static const struct frame_unwind *
hppa_linux_sigtramp_unwind_sniffer (struct frame_info *next_frame)
{
  CORE_ADDR pc = frame_pc_unwind (next_frame);

  if (hppa_linux_sigtramp_find_sigcontext (pc))
    return &hppa_linux_sigtramp_frame_unwind;

  return NULL;
}

/* Attempt to find (and return) the global pointer for the given
   function.

   This is a rather nasty bit of code searchs for the .dynamic section
   in the objfile corresponding to the pc of the function we're trying
   to call.  Once it finds the addresses at which the .dynamic section
   lives in the child process, it scans the Elf32_Dyn entries for a
   DT_PLTGOT tag.  If it finds one of these, the corresponding
   d_un.d_ptr value is the global pointer.  */

static CORE_ADDR
hppa_linux_find_global_pointer (struct value *function)
{
  struct obj_section *faddr_sect;
  CORE_ADDR faddr;
  
  faddr = value_as_address (function);

  /* Is this a plabel? If so, dereference it to get the gp value.  */
  if (faddr & 2)
    {
      int status;
      char buf[4];

      faddr &= ~3;

      status = target_read_memory (faddr + 4, buf, sizeof (buf));
      if (status == 0)
	return extract_unsigned_integer (buf, sizeof (buf));
    }

  /* If the address is in the plt section, then the real function hasn't 
     yet been fixed up by the linker so we cannot determine the gp of 
     that function.  */
  if (in_plt_section (faddr, NULL))
    return 0;

  faddr_sect = find_pc_section (faddr);
  if (faddr_sect != NULL)
    {
      struct obj_section *osect;

      ALL_OBJFILE_OSECTIONS (faddr_sect->objfile, osect)
	{
	  if (strcmp (osect->the_bfd_section->name, ".dynamic") == 0)
	    break;
	}

      if (osect < faddr_sect->objfile->sections_end)
	{
	  CORE_ADDR addr;

	  addr = osect->addr;
	  while (addr < osect->endaddr)
	    {
	      int status;
	      LONGEST tag;
	      char buf[4];

	      status = target_read_memory (addr, buf, sizeof (buf));
	      if (status != 0)
		break;
	      tag = extract_signed_integer (buf, sizeof (buf));

	      if (tag == DT_PLTGOT)
		{
		  CORE_ADDR global_pointer;

		  status = target_read_memory (addr + 4, buf, sizeof (buf));
		  if (status != 0)
		    break;
		  global_pointer = extract_unsigned_integer (buf, sizeof (buf));

		  /* The payoff... */
		  return global_pointer;
		}

	      if (tag == DT_NULL)
		break;

	      addr += 8;
	    }
	}
    }
  return 0;
}

/* Forward declarations.  */
extern initialize_file_ftype _initialize_hppa_linux_tdep;

static void
hppa_linux_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  /* GNU/Linux is always ELF.  */
  tdep->is_elf = 1;

  tdep->find_global_pointer = hppa_linux_find_global_pointer;

  set_gdbarch_write_pc (gdbarch, hppa_linux_target_write_pc);

  frame_unwind_append_sniffer (gdbarch, hppa_linux_sigtramp_unwind_sniffer);

  /* GNU/Linux uses SVR4-style shared libraries.  */
  set_solib_svr4_fetch_link_map_offsets
    (gdbarch, svr4_ilp32_fetch_link_map_offsets);

  set_gdbarch_in_solib_call_trampoline
        (gdbarch, hppa_linux_in_solib_call_trampoline);
  set_gdbarch_skip_trampoline_code
	(gdbarch, hppa_linux_skip_trampoline_code);

  /* GNU/Linux uses the dynamic linker included in the GNU C Library.  */
  set_gdbarch_skip_solib_resolver (gdbarch, glibc_skip_solib_resolver);

  /* On hppa-linux, currently, sizeof(long double) == 8.  There has been
     some discussions to support 128-bit long double, but it requires some
     more work in gcc and glibc first.  */
  set_gdbarch_long_double_bit (gdbarch, 64);

#if 0
  /* Dwarf-2 unwinding support.  Not yet working.  */
  set_gdbarch_dwarf_reg_to_regnum (gdbarch, hppa_dwarf_reg_to_regnum);
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, hppa_dwarf_reg_to_regnum);
  frame_unwind_append_sniffer (gdbarch, dwarf2_frame_sniffer);
  frame_base_append_sniffer (gdbarch, dwarf2_frame_base_sniffer);
#endif
}

void
_initialize_hppa_linux_tdep (void)
{
  gdbarch_register_osabi (bfd_arch_hppa, 0, GDB_OSABI_LINUX, hppa_linux_init_abi);
}

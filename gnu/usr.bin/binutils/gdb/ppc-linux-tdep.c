/* Target-dependent code for GDB, the GNU debugger.

   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1994, 1995, 1996,
   1997, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

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
#include "inferior.h"
#include "symtab.h"
#include "target.h"
#include "gdbcore.h"
#include "gdbcmd.h"
#include "symfile.h"
#include "objfiles.h"
#include "regcache.h"
#include "value.h"
#include "osabi.h"

#include "solib-svr4.h"
#include "ppc-tdep.h"

/* The following instructions are used in the signal trampoline code
   on GNU/Linux PPC. The kernel used to use magic syscalls 0x6666 and
   0x7777 but now uses the sigreturn syscalls.  We check for both.  */
#define INSTR_LI_R0_0x6666		0x38006666
#define INSTR_LI_R0_0x7777		0x38007777
#define INSTR_LI_R0_NR_sigreturn	0x38000077
#define INSTR_LI_R0_NR_rt_sigreturn	0x380000AC

#define INSTR_SC			0x44000002

/* Since the *-tdep.c files are platform independent (i.e, they may be
   used to build cross platform debuggers), we can't include system
   headers.  Therefore, details concerning the sigcontext structure
   must be painstakingly rerecorded.  What's worse, if these details
   ever change in the header files, they'll have to be changed here
   as well. */

/* __SIGNAL_FRAMESIZE from <asm/ptrace.h> */
#define PPC_LINUX_SIGNAL_FRAMESIZE 64

/* From <asm/sigcontext.h>, offsetof(struct sigcontext_struct, regs) == 0x1c */
#define PPC_LINUX_REGS_PTR_OFFSET (PPC_LINUX_SIGNAL_FRAMESIZE + 0x1c)

/* From <asm/sigcontext.h>, 
   offsetof(struct sigcontext_struct, handler) == 0x14 */
#define PPC_LINUX_HANDLER_PTR_OFFSET (PPC_LINUX_SIGNAL_FRAMESIZE + 0x14)

/* From <asm/ptrace.h>, values for PT_NIP, PT_R1, and PT_LNK */
#define PPC_LINUX_PT_R0		0
#define PPC_LINUX_PT_R1		1
#define PPC_LINUX_PT_R2		2
#define PPC_LINUX_PT_R3		3
#define PPC_LINUX_PT_R4		4
#define PPC_LINUX_PT_R5		5
#define PPC_LINUX_PT_R6		6
#define PPC_LINUX_PT_R7		7
#define PPC_LINUX_PT_R8		8
#define PPC_LINUX_PT_R9		9
#define PPC_LINUX_PT_R10	10
#define PPC_LINUX_PT_R11	11
#define PPC_LINUX_PT_R12	12
#define PPC_LINUX_PT_R13	13
#define PPC_LINUX_PT_R14	14
#define PPC_LINUX_PT_R15	15
#define PPC_LINUX_PT_R16	16
#define PPC_LINUX_PT_R17	17
#define PPC_LINUX_PT_R18	18
#define PPC_LINUX_PT_R19	19
#define PPC_LINUX_PT_R20	20
#define PPC_LINUX_PT_R21	21
#define PPC_LINUX_PT_R22	22
#define PPC_LINUX_PT_R23	23
#define PPC_LINUX_PT_R24	24
#define PPC_LINUX_PT_R25	25
#define PPC_LINUX_PT_R26	26
#define PPC_LINUX_PT_R27	27
#define PPC_LINUX_PT_R28	28
#define PPC_LINUX_PT_R29	29
#define PPC_LINUX_PT_R30	30
#define PPC_LINUX_PT_R31	31
#define PPC_LINUX_PT_NIP	32
#define PPC_LINUX_PT_MSR	33
#define PPC_LINUX_PT_CTR	35
#define PPC_LINUX_PT_LNK	36
#define PPC_LINUX_PT_XER	37
#define PPC_LINUX_PT_CCR	38
#define PPC_LINUX_PT_MQ		39
#define PPC_LINUX_PT_FPR0	48	/* each FP reg occupies 2 slots in this space */
#define PPC_LINUX_PT_FPR31 (PPC_LINUX_PT_FPR0 + 2*31)
#define PPC_LINUX_PT_FPSCR (PPC_LINUX_PT_FPR0 + 2*32 + 1)

static int ppc_linux_at_sigtramp_return_path (CORE_ADDR pc);

/* Determine if pc is in a signal trampoline...

   Ha!  That's not what this does at all.  wait_for_inferior in
   infrun.c calls PC_IN_SIGTRAMP in order to detect entry into a
   signal trampoline just after delivery of a signal.  But on
   GNU/Linux, signal trampolines are used for the return path only.
   The kernel sets things up so that the signal handler is called
   directly.

   If we use in_sigtramp2() in place of in_sigtramp() (see below)
   we'll (often) end up with stop_pc in the trampoline and prev_pc in
   the (now exited) handler.  The code there will cause a temporary
   breakpoint to be set on prev_pc which is not very likely to get hit
   again.

   If this is confusing, think of it this way...  the code in
   wait_for_inferior() needs to be able to detect entry into a signal
   trampoline just after a signal is delivered, not after the handler
   has been run.

   So, we define in_sigtramp() below to return 1 if the following is
   true:

   1) The previous frame is a real signal trampoline.

   - and -

   2) pc is at the first or second instruction of the corresponding
   handler.

   Why the second instruction?  It seems that wait_for_inferior()
   never sees the first instruction when single stepping.  When a
   signal is delivered while stepping, the next instruction that
   would've been stepped over isn't, instead a signal is delivered and
   the first instruction of the handler is stepped over instead.  That
   puts us on the second instruction.  (I added the test for the
   first instruction long after the fact, just in case the observed
   behavior is ever fixed.)

   PC_IN_SIGTRAMP is called from blockframe.c as well in order to set
   the frame's type (if a SIGTRAMP_FRAME).  Because of our strange
   definition of in_sigtramp below, we can't rely on the frame's type
   getting set correctly from within blockframe.c.  This is why we
   take pains to set it in init_extra_frame_info().

   NOTE: cagney/2002-11-10: I suspect the real problem here is that
   the get_prev_frame() only initializes the frame's type after the
   call to INIT_FRAME_INFO.  get_prev_frame() should be fixed, this
   code shouldn't be working its way around a bug :-(.  */

int
ppc_linux_in_sigtramp (CORE_ADDR pc, char *func_name)
{
  CORE_ADDR lr;
  CORE_ADDR sp;
  CORE_ADDR tramp_sp;
  char buf[4];
  CORE_ADDR handler;

  lr = read_register (gdbarch_tdep (current_gdbarch)->ppc_lr_regnum);
  if (!ppc_linux_at_sigtramp_return_path (lr))
    return 0;

  sp = read_register (SP_REGNUM);

  if (target_read_memory (sp, buf, sizeof (buf)) != 0)
    return 0;

  tramp_sp = extract_unsigned_integer (buf, 4);

  if (target_read_memory (tramp_sp + PPC_LINUX_HANDLER_PTR_OFFSET, buf,
			  sizeof (buf)) != 0)
    return 0;

  handler = extract_unsigned_integer (buf, 4);

  return (pc == handler || pc == handler + 4);
}

static int
insn_is_sigreturn (unsigned long pcinsn)
{
  switch(pcinsn)
    {
    case INSTR_LI_R0_0x6666:
    case INSTR_LI_R0_0x7777:
    case INSTR_LI_R0_NR_sigreturn:
    case INSTR_LI_R0_NR_rt_sigreturn:
      return 1;
    default:
      return 0;
    }
}

/*
 * The signal handler trampoline is on the stack and consists of exactly
 * two instructions.  The easiest and most accurate way of determining
 * whether the pc is in one of these trampolines is by inspecting the
 * instructions.  It'd be faster though if we could find a way to do this
 * via some simple address comparisons.
 */
static int
ppc_linux_at_sigtramp_return_path (CORE_ADDR pc)
{
  char buf[12];
  unsigned long pcinsn;
  if (target_read_memory (pc - 4, buf, sizeof (buf)) != 0)
    return 0;

  /* extract the instruction at the pc */
  pcinsn = extract_unsigned_integer (buf + 4, 4);

  return (
	   (insn_is_sigreturn (pcinsn)
	    && extract_unsigned_integer (buf + 8, 4) == INSTR_SC)
	   ||
	   (pcinsn == INSTR_SC
	    && insn_is_sigreturn (extract_unsigned_integer (buf, 4))));
}

static CORE_ADDR
ppc_linux_skip_trampoline_code (CORE_ADDR pc)
{
  char buf[4];
  struct obj_section *sect;
  struct objfile *objfile;
  unsigned long insn;
  CORE_ADDR plt_start = 0;
  CORE_ADDR symtab = 0;
  CORE_ADDR strtab = 0;
  int num_slots = -1;
  int reloc_index = -1;
  CORE_ADDR plt_table;
  CORE_ADDR reloc;
  CORE_ADDR sym;
  long symidx;
  char symname[1024];
  struct minimal_symbol *msymbol;

  /* Find the section pc is in; return if not in .plt */
  sect = find_pc_section (pc);
  if (!sect || strcmp (sect->the_bfd_section->name, ".plt") != 0)
    return 0;

  objfile = sect->objfile;

  /* Pick up the instruction at pc.  It had better be of the
     form
     li r11, IDX

     where IDX is an index into the plt_table.  */

  if (target_read_memory (pc, buf, 4) != 0)
    return 0;
  insn = extract_unsigned_integer (buf, 4);

  if ((insn & 0xffff0000) != 0x39600000 /* li r11, VAL */ )
    return 0;

  reloc_index = (insn << 16) >> 16;

  /* Find the objfile that pc is in and obtain the information
     necessary for finding the symbol name. */
  for (sect = objfile->sections; sect < objfile->sections_end; ++sect)
    {
      const char *secname = sect->the_bfd_section->name;
      if (strcmp (secname, ".plt") == 0)
	plt_start = sect->addr;
      else if (strcmp (secname, ".rela.plt") == 0)
	num_slots = ((int) sect->endaddr - (int) sect->addr) / 12;
      else if (strcmp (secname, ".dynsym") == 0)
	symtab = sect->addr;
      else if (strcmp (secname, ".dynstr") == 0)
	strtab = sect->addr;
    }

  /* Make sure we have all the information we need. */
  if (plt_start == 0 || num_slots == -1 || symtab == 0 || strtab == 0)
    return 0;

  /* Compute the value of the plt table */
  plt_table = plt_start + 72 + 8 * num_slots;

  /* Get address of the relocation entry (Elf32_Rela) */
  if (target_read_memory (plt_table + reloc_index, buf, 4) != 0)
    return 0;
  reloc = extract_unsigned_integer (buf, 4);

  sect = find_pc_section (reloc);
  if (!sect)
    return 0;

  if (strcmp (sect->the_bfd_section->name, ".text") == 0)
    return reloc;

  /* Now get the r_info field which is the relocation type and symbol
     index. */
  if (target_read_memory (reloc + 4, buf, 4) != 0)
    return 0;
  symidx = extract_unsigned_integer (buf, 4);

  /* Shift out the relocation type leaving just the symbol index */
  /* symidx = ELF32_R_SYM(symidx); */
  symidx = symidx >> 8;

  /* compute the address of the symbol */
  sym = symtab + symidx * 4;

  /* Fetch the string table index */
  if (target_read_memory (sym, buf, 4) != 0)
    return 0;
  symidx = extract_unsigned_integer (buf, 4);

  /* Fetch the string; we don't know how long it is.  Is it possible
     that the following will fail because we're trying to fetch too
     much? */
  if (target_read_memory (strtab + symidx, symname, sizeof (symname)) != 0)
    return 0;

  /* This might not work right if we have multiple symbols with the
     same name; the only way to really get it right is to perform
     the same sort of lookup as the dynamic linker. */
  msymbol = lookup_minimal_symbol_text (symname, NULL);
  if (!msymbol)
    return 0;

  return SYMBOL_VALUE_ADDRESS (msymbol);
}

/* The rs6000 version of FRAME_SAVED_PC will almost work for us.  The
   signal handler details are different, so we'll handle those here
   and call the rs6000 version to do the rest. */
CORE_ADDR
ppc_linux_frame_saved_pc (struct frame_info *fi)
{
  if ((get_frame_type (fi) == SIGTRAMP_FRAME))
    {
      CORE_ADDR regs_addr =
	read_memory_integer (get_frame_base (fi)
			     + PPC_LINUX_REGS_PTR_OFFSET, 4);
      /* return the NIP in the regs array */
      return read_memory_integer (regs_addr + 4 * PPC_LINUX_PT_NIP, 4);
    }
  else if (get_next_frame (fi)
	   && (get_frame_type (get_next_frame (fi)) == SIGTRAMP_FRAME))
    {
      CORE_ADDR regs_addr =
	read_memory_integer (get_frame_base (get_next_frame (fi))
			     + PPC_LINUX_REGS_PTR_OFFSET, 4);
      /* return LNK in the regs array */
      return read_memory_integer (regs_addr + 4 * PPC_LINUX_PT_LNK, 4);
    }
  else
    return rs6000_frame_saved_pc (fi);
}

void
ppc_linux_init_extra_frame_info (int fromleaf, struct frame_info *fi)
{
  rs6000_init_extra_frame_info (fromleaf, fi);

  if (get_next_frame (fi) != 0)
    {
      /* We're called from get_prev_frame_info; check to see if
         this is a signal frame by looking to see if the pc points
         at trampoline code */
      if (ppc_linux_at_sigtramp_return_path (get_frame_pc (fi)))
	deprecated_set_frame_type (fi, SIGTRAMP_FRAME);
      else
	/* FIXME: cagney/2002-11-10: Is this double bogus?  What
           happens if the frame has previously been marked as a dummy?  */
	deprecated_set_frame_type (fi, NORMAL_FRAME);
    }
}

int
ppc_linux_frameless_function_invocation (struct frame_info *fi)
{
  /* We'll find the wrong thing if we let 
     rs6000_frameless_function_invocation () search for a signal trampoline */
  if (ppc_linux_at_sigtramp_return_path (get_frame_pc (fi)))
    return 0;
  else
    return rs6000_frameless_function_invocation (fi);
}

void
ppc_linux_frame_init_saved_regs (struct frame_info *fi)
{
  if ((get_frame_type (fi) == SIGTRAMP_FRAME))
    {
      CORE_ADDR regs_addr;
      int i;
      if (deprecated_get_frame_saved_regs (fi))
	return;

      frame_saved_regs_zalloc (fi);

      regs_addr =
	read_memory_integer (get_frame_base (fi)
			     + PPC_LINUX_REGS_PTR_OFFSET, 4);
      deprecated_get_frame_saved_regs (fi)[PC_REGNUM] = regs_addr + 4 * PPC_LINUX_PT_NIP;
      deprecated_get_frame_saved_regs (fi)[gdbarch_tdep (current_gdbarch)->ppc_ps_regnum] =
        regs_addr + 4 * PPC_LINUX_PT_MSR;
      deprecated_get_frame_saved_regs (fi)[gdbarch_tdep (current_gdbarch)->ppc_cr_regnum] =
        regs_addr + 4 * PPC_LINUX_PT_CCR;
      deprecated_get_frame_saved_regs (fi)[gdbarch_tdep (current_gdbarch)->ppc_lr_regnum] =
        regs_addr + 4 * PPC_LINUX_PT_LNK;
      deprecated_get_frame_saved_regs (fi)[gdbarch_tdep (current_gdbarch)->ppc_ctr_regnum] =
        regs_addr + 4 * PPC_LINUX_PT_CTR;
      deprecated_get_frame_saved_regs (fi)[gdbarch_tdep (current_gdbarch)->ppc_xer_regnum] =
        regs_addr + 4 * PPC_LINUX_PT_XER;
      deprecated_get_frame_saved_regs (fi)[gdbarch_tdep (current_gdbarch)->ppc_mq_regnum] =
	regs_addr + 4 * PPC_LINUX_PT_MQ;
      for (i = 0; i < 32; i++)
	deprecated_get_frame_saved_regs (fi)[gdbarch_tdep (current_gdbarch)->ppc_gp0_regnum + i] =
	  regs_addr + 4 * PPC_LINUX_PT_R0 + 4 * i;
      for (i = 0; i < 32; i++)
	deprecated_get_frame_saved_regs (fi)[FP0_REGNUM + i] = regs_addr + 4 * PPC_LINUX_PT_FPR0 + 8 * i;
    }
  else
    rs6000_frame_init_saved_regs (fi);
}

CORE_ADDR
ppc_linux_frame_chain (struct frame_info *thisframe)
{
  /* Kernel properly constructs the frame chain for the handler */
  if ((get_frame_type (thisframe) == SIGTRAMP_FRAME))
    return read_memory_integer (get_frame_base (thisframe), 4);
  else
    return rs6000_frame_chain (thisframe);
}

/* ppc_linux_memory_remove_breakpoints attempts to remove a breakpoint
   in much the same fashion as memory_remove_breakpoint in mem-break.c,
   but is careful not to write back the previous contents if the code
   in question has changed in between inserting the breakpoint and
   removing it.

   Here is the problem that we're trying to solve...

   Once upon a time, before introducing this function to remove
   breakpoints from the inferior, setting a breakpoint on a shared
   library function prior to running the program would not work
   properly.  In order to understand the problem, it is first
   necessary to understand a little bit about dynamic linking on
   this platform.

   A call to a shared library function is accomplished via a bl
   (branch-and-link) instruction whose branch target is an entry
   in the procedure linkage table (PLT).  The PLT in the object
   file is uninitialized.  To gdb, prior to running the program, the
   entries in the PLT are all zeros.

   Once the program starts running, the shared libraries are loaded
   and the procedure linkage table is initialized, but the entries in
   the table are not (necessarily) resolved.  Once a function is
   actually called, the code in the PLT is hit and the function is
   resolved.  In order to better illustrate this, an example is in
   order; the following example is from the gdb testsuite.
	    
	We start the program shmain.

	    [kev@arroyo testsuite]$ ../gdb gdb.base/shmain
	    [...]

	We place two breakpoints, one on shr1 and the other on main.

	    (gdb) b shr1
	    Breakpoint 1 at 0x100409d4
	    (gdb) b main
	    Breakpoint 2 at 0x100006a0: file gdb.base/shmain.c, line 44.

	Examine the instruction (and the immediatly following instruction)
	upon which the breakpoint was placed.  Note that the PLT entry
	for shr1 contains zeros.

	    (gdb) x/2i 0x100409d4
	    0x100409d4 <shr1>:      .long 0x0
	    0x100409d8 <shr1+4>:    .long 0x0

	Now run 'til main.

	    (gdb) r
	    Starting program: gdb.base/shmain 
	    Breakpoint 1 at 0xffaf790: file gdb.base/shr1.c, line 19.

	    Breakpoint 2, main ()
		at gdb.base/shmain.c:44
	    44        g = 1;

	Examine the PLT again.  Note that the loading of the shared
	library has initialized the PLT to code which loads a constant
	(which I think is an index into the GOT) into r11 and then
	branchs a short distance to the code which actually does the
	resolving.

	    (gdb) x/2i 0x100409d4
	    0x100409d4 <shr1>:      li      r11,4
	    0x100409d8 <shr1+4>:    b       0x10040984 <sg+4>
	    (gdb) c
	    Continuing.

	    Breakpoint 1, shr1 (x=1)
		at gdb.base/shr1.c:19
	    19        l = 1;

	Now we've hit the breakpoint at shr1.  (The breakpoint was
	reset from the PLT entry to the actual shr1 function after the
	shared library was loaded.) Note that the PLT entry has been
	resolved to contain a branch that takes us directly to shr1. 
	(The real one, not the PLT entry.)

	    (gdb) x/2i 0x100409d4
	    0x100409d4 <shr1>:      b       0xffaf76c <shr1>
	    0x100409d8 <shr1+4>:    b       0x10040984 <sg+4>

   The thing to note here is that the PLT entry for shr1 has been
   changed twice.

   Now the problem should be obvious.  GDB places a breakpoint (a
   trap instruction) on the zero value of the PLT entry for shr1. 
   Later on, after the shared library had been loaded and the PLT
   initialized, GDB gets a signal indicating this fact and attempts
   (as it always does when it stops) to remove all the breakpoints.

   The breakpoint removal was causing the former contents (a zero
   word) to be written back to the now initialized PLT entry thus
   destroying a portion of the initialization that had occurred only a
   short time ago.  When execution continued, the zero word would be
   executed as an instruction an an illegal instruction trap was
   generated instead.  (0 is not a legal instruction.)

   The fix for this problem was fairly straightforward.  The function
   memory_remove_breakpoint from mem-break.c was copied to this file,
   modified slightly, and renamed to ppc_linux_memory_remove_breakpoint.
   In tm-linux.h, MEMORY_REMOVE_BREAKPOINT is defined to call this new
   function.

   The differences between ppc_linux_memory_remove_breakpoint () and
   memory_remove_breakpoint () are minor.  All that the former does
   that the latter does not is check to make sure that the breakpoint
   location actually contains a breakpoint (trap instruction) prior
   to attempting to write back the old contents.  If it does contain
   a trap instruction, we allow the old contents to be written back. 
   Otherwise, we silently do nothing.

   The big question is whether memory_remove_breakpoint () should be
   changed to have the same functionality.  The downside is that more
   traffic is generated for remote targets since we'll have an extra
   fetch of a memory word each time a breakpoint is removed.

   For the time being, we'll leave this self-modifying-code-friendly
   version in ppc-linux-tdep.c, but it ought to be migrated somewhere
   else in the event that some other platform has similar needs with
   regard to removing breakpoints in some potentially self modifying
   code.  */
int
ppc_linux_memory_remove_breakpoint (CORE_ADDR addr, char *contents_cache)
{
  const unsigned char *bp;
  int val;
  int bplen;
  char old_contents[BREAKPOINT_MAX];

  /* Determine appropriate breakpoint contents and size for this address.  */
  bp = BREAKPOINT_FROM_PC (&addr, &bplen);
  if (bp == NULL)
    error ("Software breakpoints not implemented for this target.");

  val = target_read_memory (addr, old_contents, bplen);

  /* If our breakpoint is no longer at the address, this means that the
     program modified the code on us, so it is wrong to put back the
     old value */
  if (val == 0 && memcmp (bp, old_contents, bplen) == 0)
    val = target_write_memory (addr, contents_cache, bplen);

  return val;
}

/* For historic reasons, PPC 32 GNU/Linux follows PowerOpen rather
   than the 32 bit SYSV R4 ABI structure return convention - all
   structures, no matter their size, are put in memory.  Vectors,
   which were added later, do get returned in a register though.  */

static enum return_value_convention
ppc_linux_return_value (struct gdbarch *gdbarch, struct type *valtype,
			struct regcache *regcache, void *readbuf,
			const void *writebuf)
{  
  if ((TYPE_CODE (valtype) == TYPE_CODE_STRUCT
       || TYPE_CODE (valtype) == TYPE_CODE_UNION)
      && !((TYPE_LENGTH (valtype) == 16 || TYPE_LENGTH (valtype) == 8)
	   && TYPE_VECTOR (valtype)))
    return RETURN_VALUE_STRUCT_CONVENTION;
  else
    return ppc_sysv_abi_return_value (gdbarch, valtype, regcache, readbuf,
				      writebuf);
}

/* Fetch (and possibly build) an appropriate link_map_offsets
   structure for GNU/Linux PPC targets using the struct offsets
   defined in link.h (but without actual reference to that file).

   This makes it possible to access GNU/Linux PPC shared libraries
   from a GDB that was not built on an GNU/Linux PPC host (for cross
   debugging).  */

struct link_map_offsets *
ppc_linux_svr4_fetch_link_map_offsets (void)
{
  static struct link_map_offsets lmo;
  static struct link_map_offsets *lmp = NULL;

  if (lmp == NULL)
    {
      lmp = &lmo;

      lmo.r_debug_size = 8;	/* The actual size is 20 bytes, but
				   this is all we need.  */
      lmo.r_map_offset = 4;
      lmo.r_map_size   = 4;

      lmo.link_map_size = 20;	/* The actual size is 560 bytes, but
				   this is all we need.  */
      lmo.l_addr_offset = 0;
      lmo.l_addr_size   = 4;

      lmo.l_name_offset = 4;
      lmo.l_name_size   = 4;

      lmo.l_next_offset = 12;
      lmo.l_next_size   = 4;

      lmo.l_prev_offset = 16;
      lmo.l_prev_size   = 4;
    }

  return lmp;
}


/* Macros for matching instructions.  Note that, since all the
   operands are masked off before they're or-ed into the instruction,
   you can use -1 to make masks.  */

#define insn_d(opcd, rts, ra, d)                \
  ((((opcd) & 0x3f) << 26)                      \
   | (((rts) & 0x1f) << 21)                     \
   | (((ra) & 0x1f) << 16)                      \
   | ((d) & 0xffff))

#define insn_ds(opcd, rts, ra, d, xo)           \
  ((((opcd) & 0x3f) << 26)                      \
   | (((rts) & 0x1f) << 21)                     \
   | (((ra) & 0x1f) << 16)                      \
   | ((d) & 0xfffc)                             \
   | ((xo) & 0x3))

#define insn_xfx(opcd, rts, spr, xo)            \
  ((((opcd) & 0x3f) << 26)                      \
   | (((rts) & 0x1f) << 21)                     \
   | (((spr) & 0x1f) << 16)                     \
   | (((spr) & 0x3e0) << 6)                     \
   | (((xo) & 0x3ff) << 1))

/* Read a PPC instruction from memory.  PPC instructions are always
   big-endian, no matter what endianness the program is running in, so
   we can't use read_memory_integer or one of its friends here.  */
static unsigned int
read_insn (CORE_ADDR pc)
{
  unsigned char buf[4];

  read_memory (pc, buf, 4);
  return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}


/* An instruction to match.  */
struct insn_pattern
{
  unsigned int mask;            /* mask the insn with this... */
  unsigned int data;            /* ...and see if it matches this. */
  int optional;                 /* If non-zero, this insn may be absent.  */
};

/* Return non-zero if the instructions at PC match the series
   described in PATTERN, or zero otherwise.  PATTERN is an array of
   'struct insn_pattern' objects, terminated by an entry whose mask is
   zero.

   When the match is successful, fill INSN[i] with what PATTERN[i]
   matched.  If PATTERN[i] is optional, and the instruction wasn't
   present, set INSN[i] to 0 (which is not a valid PPC instruction).
   INSN should have as many elements as PATTERN.  Note that, if
   PATTERN contains optional instructions which aren't present in
   memory, then INSN will have holes, so INSN[i] isn't necessarily the
   i'th instruction in memory.  */
static int
insns_match_pattern (CORE_ADDR pc,
                     struct insn_pattern *pattern,
                     unsigned int *insn)
{
  int i;

  for (i = 0; pattern[i].mask; i++)
    {
      insn[i] = read_insn (pc);
      if ((insn[i] & pattern[i].mask) == pattern[i].data)
        pc += 4;
      else if (pattern[i].optional)
        insn[i] = 0;
      else
        return 0;
    }

  return 1;
}


/* Return the 'd' field of the d-form instruction INSN, properly
   sign-extended.  */
static CORE_ADDR
insn_d_field (unsigned int insn)
{
  return ((((CORE_ADDR) insn & 0xffff) ^ 0x8000) - 0x8000);
}


/* Return the 'ds' field of the ds-form instruction INSN, with the two
   zero bits concatenated at the right, and properly
   sign-extended.  */
static CORE_ADDR
insn_ds_field (unsigned int insn)
{
  return ((((CORE_ADDR) insn & 0xfffc) ^ 0x8000) - 0x8000);
}


/* If DESC is the address of a 64-bit PowerPC GNU/Linux function
   descriptor, return the descriptor's entry point.  */
static CORE_ADDR
ppc64_desc_entry_point (CORE_ADDR desc)
{
  /* The first word of the descriptor is the entry point.  */
  return (CORE_ADDR) read_memory_unsigned_integer (desc, 8);
}


/* Pattern for the standard linkage function.  These are built by
   build_plt_stub in elf64-ppc.c, whose GLINK argument is always
   zero.  */
static struct insn_pattern ppc64_standard_linkage[] =
  {
    /* addis r12, r2, <any> */
    { insn_d (-1, -1, -1, 0), insn_d (15, 12, 2, 0), 0 },

    /* std r2, 40(r1) */
    { -1, insn_ds (62, 2, 1, 40, 0), 0 },

    /* ld r11, <any>(r12) */
    { insn_ds (-1, -1, -1, 0, -1), insn_ds (58, 11, 12, 0, 0), 0 },

    /* addis r12, r12, 1 <optional> */
    { insn_d (-1, -1, -1, -1), insn_d (15, 12, 2, 1), 1 },

    /* ld r2, <any>(r12) */
    { insn_ds (-1, -1, -1, 0, -1), insn_ds (58, 2, 12, 0, 0), 0 },

    /* addis r12, r12, 1 <optional> */
    { insn_d (-1, -1, -1, -1), insn_d (15, 12, 2, 1), 1 },

    /* mtctr r11 */
    { insn_xfx (-1, -1, -1, -1), insn_xfx (31, 11, 9, 467),
      0 },

    /* ld r11, <any>(r12) */
    { insn_ds (-1, -1, -1, 0, -1), insn_ds (58, 11, 12, 0, 0), 0 },
      
    /* bctr */
    { -1, 0x4e800420, 0 },

    { 0, 0, 0 }
  };
#define PPC64_STANDARD_LINKAGE_LEN \
  (sizeof (ppc64_standard_linkage) / sizeof (ppc64_standard_linkage[0]))


/* Recognize a 64-bit PowerPC GNU/Linux linkage function --- what GDB
   calls a "solib trampoline".  */
static int
ppc64_in_solib_call_trampoline (CORE_ADDR pc, char *name)
{
  /* Detecting solib call trampolines on PPC64 GNU/Linux is a pain.

     It's not specifically solib call trampolines that are the issue.
     Any call from one function to another function that uses a
     different TOC requires a trampoline, to save the caller's TOC
     pointer and then load the callee's TOC.  An executable or shared
     library may have more than one TOC, so even intra-object calls
     may require a trampoline.  Since executable and shared libraries
     will all have their own distinct TOCs, every inter-object call is
     also an inter-TOC call, and requires a trampoline --- so "solib
     call trampolines" are just a special case.

     The 64-bit PowerPC GNU/Linux ABI calls these call trampolines
     "linkage functions".  Since they need to be near the functions
     that call them, they all appear in .text, not in any special
     section.  The .plt section just contains an array of function
     descriptors, from which the linkage functions load the callee's
     entry point, TOC value, and environment pointer.  So
     in_plt_section is useless.  The linkage functions don't have any
     special linker symbols to name them, either.

     The only way I can see to recognize them is to actually look at
     their code.  They're generated by ppc_build_one_stub and some
     other functions in bfd/elf64-ppc.c, so that should show us all
     the instruction sequences we need to recognize.  */
  unsigned int insn[PPC64_STANDARD_LINKAGE_LEN];

  return insns_match_pattern (pc, ppc64_standard_linkage, insn);
}


/* When the dynamic linker is doing lazy symbol resolution, the first
   call to a function in another object will go like this:

   - The user's function calls the linkage function:

     100007c4:	4b ff fc d5 	bl	10000498
     100007c8:	e8 41 00 28 	ld	r2,40(r1)

   - The linkage function loads the entry point (and other stuff) from
     the function descriptor in the PLT, and jumps to it:

     10000498:	3d 82 00 00 	addis	r12,r2,0
     1000049c:	f8 41 00 28 	std	r2,40(r1)
     100004a0:	e9 6c 80 98 	ld	r11,-32616(r12)
     100004a4:	e8 4c 80 a0 	ld	r2,-32608(r12)
     100004a8:	7d 69 03 a6 	mtctr	r11
     100004ac:	e9 6c 80 a8 	ld	r11,-32600(r12)
     100004b0:	4e 80 04 20 	bctr

   - But since this is the first time that PLT entry has been used, it
     sends control to its glink entry.  That loads the number of the
     PLT entry and jumps to the common glink0 code:

     10000c98:	38 00 00 00 	li	r0,0
     10000c9c:	4b ff ff dc 	b	10000c78

   - The common glink0 code then transfers control to the dynamic
     linker's fixup code:

     10000c78:	e8 41 00 28 	ld	r2,40(r1)
     10000c7c:	3d 82 00 00 	addis	r12,r2,0
     10000c80:	e9 6c 80 80 	ld	r11,-32640(r12)
     10000c84:	e8 4c 80 88 	ld	r2,-32632(r12)
     10000c88:	7d 69 03 a6 	mtctr	r11
     10000c8c:	e9 6c 80 90 	ld	r11,-32624(r12)
     10000c90:	4e 80 04 20 	bctr

   Eventually, this code will figure out how to skip all of this,
   including the dynamic linker.  At the moment, we just get through
   the linkage function.  */

/* If the current thread is about to execute a series of instructions
   at PC matching the ppc64_standard_linkage pattern, and INSN is the result
   from that pattern match, return the code address to which the
   standard linkage function will send them.  (This doesn't deal with
   dynamic linker lazy symbol resolution stubs.)  */
static CORE_ADDR
ppc64_standard_linkage_target (CORE_ADDR pc, unsigned int *insn)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch);

  /* The address of the function descriptor this linkage function
     references.  */
  CORE_ADDR desc
    = ((CORE_ADDR) read_register (tdep->ppc_gp0_regnum + 2)
       + (insn_d_field (insn[0]) << 16)
       + insn_ds_field (insn[2]));

  /* The first word of the descriptor is the entry point.  Return that.  */
  return ppc64_desc_entry_point (desc);
}


/* Given that we've begun executing a call trampoline at PC, return
   the entry point of the function the trampoline will go to.  */
static CORE_ADDR
ppc64_skip_trampoline_code (CORE_ADDR pc)
{
  unsigned int ppc64_standard_linkage_insn[PPC64_STANDARD_LINKAGE_LEN];

  if (insns_match_pattern (pc, ppc64_standard_linkage,
                           ppc64_standard_linkage_insn))
    return ppc64_standard_linkage_target (pc, ppc64_standard_linkage_insn);
  else
    return 0;
}


/* Support for CONVERT_FROM_FUNC_PTR_ADDR (ARCH, ADDR, TARG) on PPC64
   GNU/Linux.

   Usually a function pointer's representation is simply the address
   of the function. On GNU/Linux on the 64-bit PowerPC however, a
   function pointer is represented by a pointer to a TOC entry. This
   TOC entry contains three words, the first word is the address of
   the function, the second word is the TOC pointer (r2), and the
   third word is the static chain value.  Throughout GDB it is
   currently assumed that a function pointer contains the address of
   the function, which is not easy to fix.  In addition, the
   conversion of a function address to a function pointer would
   require allocation of a TOC entry in the inferior's memory space,
   with all its drawbacks.  To be able to call C++ virtual methods in
   the inferior (which are called via function pointers),
   find_function_addr uses this function to get the function address
   from a function pointer.  */

/* If ADDR points at what is clearly a function descriptor, transform
   it into the address of the corresponding function.  Be
   conservative, otherwize GDB will do the transformation on any
   random addresses such as occures when there is no symbol table.  */

static CORE_ADDR
ppc64_linux_convert_from_func_ptr_addr (struct gdbarch *gdbarch,
					CORE_ADDR addr,
					struct target_ops *targ)
{
  struct section_table *s = target_section_by_addr (targ, addr);

  /* Check if ADDR points to a function descriptor.  */
  if (s && strcmp (s->the_bfd_section->name, ".opd") == 0)
    return get_target_memory_unsigned (targ, addr, 8);

  return addr;
}


enum {
  ELF_NGREG = 48,
  ELF_NFPREG = 33,
  ELF_NVRREG = 33
};

enum {
  ELF_GREGSET_SIZE = (ELF_NGREG * 4),
  ELF_FPREGSET_SIZE = (ELF_NFPREG * 8)
};

void
ppc_linux_supply_gregset (char *buf)
{
  int regi;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch); 

  for (regi = 0; regi < 32; regi++)
    supply_register (regi, buf + 4 * regi);

  supply_register (PC_REGNUM, buf + 4 * PPC_LINUX_PT_NIP);
  supply_register (tdep->ppc_lr_regnum, buf + 4 * PPC_LINUX_PT_LNK);
  supply_register (tdep->ppc_cr_regnum, buf + 4 * PPC_LINUX_PT_CCR);
  supply_register (tdep->ppc_xer_regnum, buf + 4 * PPC_LINUX_PT_XER);
  supply_register (tdep->ppc_ctr_regnum, buf + 4 * PPC_LINUX_PT_CTR);
  if (tdep->ppc_mq_regnum != -1)
    supply_register (tdep->ppc_mq_regnum, buf + 4 * PPC_LINUX_PT_MQ);
  supply_register (tdep->ppc_ps_regnum, buf + 4 * PPC_LINUX_PT_MSR);
}

void
ppc_linux_supply_fpregset (char *buf)
{
  int regi;
  struct gdbarch_tdep *tdep = gdbarch_tdep (current_gdbarch); 

  for (regi = 0; regi < 32; regi++)
    supply_register (FP0_REGNUM + regi, buf + 8 * regi);

  /* The FPSCR is stored in the low order word of the last doubleword in the
     fpregset.  */
  supply_register (tdep->ppc_fpscr_regnum, buf + 8 * 32 + 4);
}

/*
  Use a local version of this function to get the correct types for regsets.
*/

static void
fetch_core_registers (char *core_reg_sect,
		      unsigned core_reg_size,
		      int which,
		      CORE_ADDR reg_addr)
{
  if (which == 0)
    {
      if (core_reg_size == ELF_GREGSET_SIZE)
	ppc_linux_supply_gregset (core_reg_sect);
      else
	warning ("wrong size gregset struct in core file");
    }
  else if (which == 2)
    {
      if (core_reg_size == ELF_FPREGSET_SIZE)
	ppc_linux_supply_fpregset (core_reg_sect);
      else
	warning ("wrong size fpregset struct in core file");
    }
}

/* Register that we are able to handle ELF file formats using standard
   procfs "regset" structures.  */

static struct core_fns ppc_linux_regset_core_fns =
{
  bfd_target_elf_flavour,	/* core_flavour */
  default_check_format,		/* check_format */
  default_core_sniffer,		/* core_sniffer */
  fetch_core_registers,		/* core_read_registers */
  NULL				/* next */
};

static void
ppc_linux_init_abi (struct gdbarch_info info,
                    struct gdbarch *gdbarch)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  if (tdep->wordsize == 4)
    {
      /* Until November 2001, gcc did not comply with the 32 bit SysV
	 R4 ABI requirement that structures less than or equal to 8
	 bytes should be returned in registers.  Instead GCC was using
	 the the AIX/PowerOpen ABI - everything returned in memory
	 (well ignoring vectors that is).  When this was corrected, it
	 wasn't fixed for GNU/Linux native platform.  Use the
	 PowerOpen struct convention.  */
      set_gdbarch_return_value (gdbarch, ppc_linux_return_value);

      /* Note: kevinb/2002-04-12: See note in rs6000_gdbarch_init regarding
	 *_push_arguments().  The same remarks hold for the methods below.  */
      set_gdbarch_deprecated_frameless_function_invocation (gdbarch, ppc_linux_frameless_function_invocation);
      set_gdbarch_deprecated_frame_chain (gdbarch, ppc_linux_frame_chain);
      set_gdbarch_deprecated_frame_saved_pc (gdbarch, ppc_linux_frame_saved_pc);

      set_gdbarch_deprecated_frame_init_saved_regs (gdbarch,
                                         ppc_linux_frame_init_saved_regs);
      set_gdbarch_deprecated_init_extra_frame_info (gdbarch,
                                         ppc_linux_init_extra_frame_info);

      set_gdbarch_memory_remove_breakpoint (gdbarch,
                                            ppc_linux_memory_remove_breakpoint);
      /* Shared library handling.  */
      set_gdbarch_in_solib_call_trampoline (gdbarch, in_plt_section);
      set_gdbarch_skip_trampoline_code (gdbarch,
                                        ppc_linux_skip_trampoline_code);
      set_solib_svr4_fetch_link_map_offsets
        (gdbarch, ppc_linux_svr4_fetch_link_map_offsets);
    }
  
  if (tdep->wordsize == 8)
    {
      /* Handle PPC64 GNU/Linux function pointers (which are really
         function descriptors).  */
      set_gdbarch_convert_from_func_ptr_addr
        (gdbarch, ppc64_linux_convert_from_func_ptr_addr);

      set_gdbarch_in_solib_call_trampoline
        (gdbarch, ppc64_in_solib_call_trampoline);
      set_gdbarch_skip_trampoline_code (gdbarch, ppc64_skip_trampoline_code);

      /* PPC64 malloc's entry-point is called ".malloc".  */
      set_gdbarch_name_of_malloc (gdbarch, ".malloc");
    }
}

void
_initialize_ppc_linux_tdep (void)
{
  /* Register for all sub-familes of the POWER/PowerPC: 32-bit and
     64-bit PowerPC, and the older rs6k.  */
  gdbarch_register_osabi (bfd_arch_powerpc, bfd_mach_ppc, GDB_OSABI_LINUX,
                         ppc_linux_init_abi);
  gdbarch_register_osabi (bfd_arch_powerpc, bfd_mach_ppc64, GDB_OSABI_LINUX,
                         ppc_linux_init_abi);
  gdbarch_register_osabi (bfd_arch_rs6000, bfd_mach_rs6k, GDB_OSABI_LINUX,
                         ppc_linux_init_abi);
  add_core_fns (&ppc_linux_regset_core_fns);
}

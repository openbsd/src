/* Target-dependent code for the SPARC for GDB, the GNU debugger.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993 Free Software Foundation, Inc.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	$Id: sparc-tdep.c,v 1.1.1.1 1995/10/18 08:40:09 deraadt Exp $
*/

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "obstack.h"
#include "target.h"
#include "ieee-float.h"

#include "symfile.h" /* for objfiles.h */
#include "objfiles.h" /* for find_pc_section */

#ifdef	USE_PROC_FS
#include <sys/procfs.h>
#endif

#include "gdbcore.h"

/* From infrun.c */
extern int stop_after_trap;

/* We don't store all registers immediately when requested, since they
   get sent over in large chunks anyway.  Instead, we accumulate most
   of the changes and send them over once.  "deferred_stores" keeps
   track of which sets of registers we have locally-changed copies of,
   so we only need send the groups that have changed.  */

int deferred_stores = 0;	/* Cumulates stores we want to do eventually. */

typedef enum
{
  Error, not_branch, bicc, bicca, ba, baa, ticc, ta
} branch_type;

/* Simulate single-step ptrace call for sun4.  Code written by Gary
   Beihl (beihl@mcc.com).  */

/* npc4 and next_pc describe the situation at the time that the
   step-breakpoint was set, not necessary the current value of NPC_REGNUM.  */
static CORE_ADDR next_pc, npc4, target;
static int brknpc4, brktrg;
typedef char binsn_quantum[BREAKPOINT_MAX];
static binsn_quantum break_mem[3];

/* Non-zero if we just simulated a single-step ptrace call.  This is
   needed because we cannot remove the breakpoints in the inferior
   process until after the `wait' in `wait_for_inferior'.  Used for
   sun4. */

int one_stepped;

/* single_step() is called just before we want to resume the inferior,
   if we want to single-step it but there is no hardware or kernel single-step
   support (as on all SPARCs).  We find all the possible targets of the
   coming instruction and breakpoint them.

   single_step is also called just after the inferior stops.  If we had
   set up a simulated single-step, we undo our damage.  */

void
single_step (ignore)
     int ignore; /* pid, but we don't need it */
{
  branch_type br, isannulled();
  CORE_ADDR pc;
  long pc_instruction;

  if (!one_stepped)
    {
      /* Always set breakpoint for NPC.  */
      next_pc = read_register (NPC_REGNUM);
      npc4 = next_pc + 4; /* branch not taken */

      target_insert_breakpoint (next_pc, break_mem[0]);
      /* printf ("set break at %x\n",next_pc); */

      pc = read_register (PC_REGNUM);
      pc_instruction = read_memory_integer (pc, sizeof(pc_instruction));
      br = isannulled (pc_instruction, pc, &target);
      brknpc4 = brktrg = 0;

      if (br == bicca)
	{
	  /* Conditional annulled branch will either end up at
	     npc (if taken) or at npc+4 (if not taken).
	     Trap npc+4.  */
	  brknpc4 = 1;
	  target_insert_breakpoint (npc4, break_mem[1]);
	}
      else if (br == baa && target != next_pc)
	{
	  /* Unconditional annulled branch will always end up at
	     the target.  */
	  brktrg = 1;
	  target_insert_breakpoint (target, break_mem[2]);
	}

      /* We are ready to let it go */
      one_stepped = 1;
      return;
    }
  else
    {
      /* Remove breakpoints */
      target_remove_breakpoint (next_pc, break_mem[0]);

      if (brknpc4)
	target_remove_breakpoint (npc4, break_mem[1]);

      if (brktrg)
	target_remove_breakpoint (target, break_mem[2]);

      one_stepped = 0;
    }
}

#define	FRAME_SAVED_L0	0			    /* Byte offset from SP */
#define	FRAME_SAVED_I0	(8 * REGISTER_RAW_SIZE (0)) /* Byte offset from SP */

CORE_ADDR
sparc_frame_chain (thisframe)
     FRAME thisframe;
{
  char buf[MAX_REGISTER_RAW_SIZE];
  int err;
  CORE_ADDR addr;

  addr = thisframe->frame + FRAME_SAVED_I0 +
	 REGISTER_RAW_SIZE (FP_REGNUM) * (FP_REGNUM - I0_REGNUM);
  err = target_read_memory (addr, buf, REGISTER_RAW_SIZE (FP_REGNUM));
  if (err)
    return 0;
  return extract_address (buf, REGISTER_RAW_SIZE (FP_REGNUM));
}

CORE_ADDR
sparc_extract_struct_value_address (regbuf)
     char regbuf[REGISTER_BYTES];
{
  return read_memory_integer (((int *)(regbuf))[SP_REGNUM]+(16*4), 
	       		      TARGET_PTR_BIT / TARGET_CHAR_BIT);
}

/* Find the pc saved in frame FRAME.  */

CORE_ADDR
frame_saved_pc (frame)
     FRAME frame;
{
  char buf[MAX_REGISTER_RAW_SIZE];
  CORE_ADDR addr;

  addr = (frame->bottom + FRAME_SAVED_I0 +
	  REGISTER_RAW_SIZE (I7_REGNUM) * (I7_REGNUM - I0_REGNUM));
  read_memory (addr, buf, REGISTER_RAW_SIZE (I7_REGNUM));
  return PC_ADJUST (extract_address (buf, REGISTER_RAW_SIZE (I7_REGNUM)));
}

/*
 * Since an individual frame in the frame cache is defined by two
 * arguments (a frame pointer and a stack pointer), we need two
 * arguments to get info for an arbitrary stack frame.  This routine
 * takes two arguments and makes the cached frames look as if these
 * two arguments defined a frame on the cache.  This allows the rest
 * of info frame to extract the important arguments without
 * difficulty. 
 */
FRAME
setup_arbitrary_frame (argc, argv)
     int argc;
     FRAME_ADDR *argv;
{
  FRAME fid;

  if (argc != 2)
    error ("Sparc frame specifications require two arguments: fp and sp");

  fid = create_new_frame (argv[0], 0);

  if (!fid)
    fatal ("internal: create_new_frame returned invalid frame id");
  
  fid->bottom = argv[1];
  fid->pc = FRAME_SAVED_PC (fid);
  return fid;
}

/* Given a pc value, skip it forward past the function prologue by
   disassembling instructions that appear to be a prologue.

   If FRAMELESS_P is set, we are only testing to see if the function
   is frameless.  This allows a quicker answer.

   This routine should be more specific in its actions; making sure
   that it uses the same register in the initial prologue section.  */
CORE_ADDR 
skip_prologue (start_pc, frameless_p)
     CORE_ADDR start_pc;
     int frameless_p;
{
  union
    {
      unsigned long int code;
      struct
	{
	  unsigned int op:2;
	  unsigned int rd:5;
	  unsigned int op2:3;
	  unsigned int imm22:22;
	} sethi;
      struct
	{
	  unsigned int op:2;
	  unsigned int rd:5;
	  unsigned int op3:6;
	  unsigned int rs1:5;
	  unsigned int i:1;
	  unsigned int simm13:13;
	} add;
      int i;
    } x;
  int dest = -1;
  CORE_ADDR pc = start_pc;

  x.i = read_memory_integer (pc, 4);

  /* Recognize the `sethi' insn and record its destination.  */
  if (x.sethi.op == 0 && x.sethi.op2 == 4)
    {
      dest = x.sethi.rd;
      pc += 4;
      x.i = read_memory_integer (pc, 4);
    }

  /* Recognize an add immediate value to register to either %g1 or
     the destination register recorded above.  Actually, this might
     well recognize several different arithmetic operations.
     It doesn't check that rs1 == rd because in theory "sub %g0, 5, %g1"
     followed by "save %sp, %g1, %sp" is a valid prologue (Not that
     I imagine any compiler really does that, however).  */
  if (x.add.op == 2 && x.add.i && (x.add.rd == 1 || x.add.rd == dest))
    {
      pc += 4;
      x.i = read_memory_integer (pc, 4);
    }

  /* This recognizes any SAVE insn.  But why do the XOR and then
     the compare?  That's identical to comparing against 60 (as long
     as there isn't any sign extension).  */
  if (x.add.op == 2 && (x.add.op3 ^ 32) == 28)
    {
      pc += 4;
      if (frameless_p)			/* If the save is all we care about, */
	return pc;			/* return before doing more work */
      x.i = read_memory_integer (pc, 4);
    }
  else
    {
      /* Without a save instruction, it's not a prologue.  */
      return start_pc;
    }

  /* Now we need to recognize stores into the frame from the input
     registers.  This recognizes all non alternate stores of input
     register, into a location offset from the frame pointer.  */
  while (x.add.op == 3
	 && (x.add.op3 & 0x3c) == 4 /* Store, non-alternate.  */
	 && (x.add.rd & 0x18) == 0x18 /* Input register.  */
	 && x.add.i		/* Immediate mode.  */
	 && x.add.rs1 == 30	/* Off of frame pointer.  */
	 /* Into reserved stack space.  */
	 && x.add.simm13 >= 0x44
	 && x.add.simm13 < 0x5b)
    {
      pc += 4;
      x.i = read_memory_integer (pc, 4);
    }
  return pc;
}

/* Check instruction at ADDR to see if it is an annulled branch.
   All other instructions will go to NPC or will trap.
   Set *TARGET if we find a canidate branch; set to zero if not. */
   
branch_type
isannulled (instruction, addr, target)
     long instruction;
     CORE_ADDR addr, *target;
{
  branch_type val = not_branch;
  long int offset;		/* Must be signed for sign-extend.  */
  union
    {
      unsigned long int code;
      struct
	{
	  unsigned int op:2;
	  unsigned int a:1;
	  unsigned int cond:4;
	  unsigned int op2:3;
	  unsigned int disp22:22;
	} b;
    } insn;

  *target = 0;
  insn.code = instruction;

  if (insn.b.op == 0
      && (insn.b.op2 == 2 || insn.b.op2 == 6 || insn.b.op2 == 7))
    {
      if (insn.b.cond == 8)
	val = insn.b.a ? baa : ba;
      else
	val = insn.b.a ? bicca : bicc;
      offset = 4 * ((int) (insn.b.disp22 << 10) >> 10);
      *target = addr + offset;
    }

  return val;
}

/* sparc_frame_find_saved_regs ()

   Stores, into a struct frame_saved_regs,
   the addresses of the saved registers of frame described by FRAME_INFO.
   This includes special registers such as pc and fp saved in special
   ways in the stack frame.  sp is even more special:
   the address we return for it IS the sp for the next frame.

   Note that on register window machines, we are currently making the
   assumption that window registers are being saved somewhere in the
   frame in which they are being used.  If they are stored in an
   inferior frame, find_saved_register will break.

   On the Sun 4, the only time all registers are saved is when
   a dummy frame is involved.  Otherwise, the only saved registers
   are the LOCAL and IN registers which are saved as a result
   of the "save/restore" opcodes.  This condition is determined
   by address rather than by value.

   The "pc" is not stored in a frame on the SPARC.  (What is stored
   is a return address minus 8.)  sparc_pop_frame knows how to
   deal with that.  Other routines might or might not.

   See tm-sparc.h (PUSH_FRAME and friends) for CRITICAL information
   about how this works.  */

void
sparc_frame_find_saved_regs (fi, saved_regs_addr)
     struct frame_info *fi;
     struct frame_saved_regs *saved_regs_addr;
{
  register int regnum;
  FRAME_ADDR frame = read_register (FP_REGNUM);
  FRAME fid = FRAME_INFO_ID (fi);

  if (!fid)
    fatal ("Bad frame info struct in FRAME_FIND_SAVED_REGS");

  memset (saved_regs_addr, 0, sizeof (*saved_regs_addr));

  /* Old test.
  if (fi->pc >= frame - CALL_DUMMY_LENGTH - 0x140
      && fi->pc <= frame) */

  if (fi->pc >= (fi->bottom ? fi->bottom :
		   read_register (SP_REGNUM))
      && fi->pc <= FRAME_FP(fi))
    {
      /* Dummy frame.  All but the window regs are in there somewhere. */
      for (regnum = G1_REGNUM; regnum < G1_REGNUM+7; regnum++)
	saved_regs_addr->regs[regnum] =
	  frame + (regnum - G0_REGNUM) * 4 - 0xa0;
      for (regnum = I0_REGNUM; regnum < I0_REGNUM+8; regnum++)
	saved_regs_addr->regs[regnum] =
	  frame + (regnum - I0_REGNUM) * 4 - 0xc0;
      for (regnum = FP0_REGNUM; regnum < FP0_REGNUM + 32; regnum++)
	saved_regs_addr->regs[regnum] =
	  frame + (regnum - FP0_REGNUM) * 4 - 0x80;
      for (regnum = Y_REGNUM; regnum < NUM_REGS; regnum++)
	saved_regs_addr->regs[regnum] =
	  frame + (regnum - Y_REGNUM) * 4 - 0xe0;
      frame = fi->bottom ?
	fi->bottom : read_register (SP_REGNUM);
    }
  else
    {
      /* Normal frame.  Just Local and In registers */
      frame = fi->bottom ?
	fi->bottom : read_register (SP_REGNUM);
      for (regnum = L0_REGNUM; regnum < L0_REGNUM+16; regnum++)
	saved_regs_addr->regs[regnum] = frame + (regnum-L0_REGNUM) * 4;
    }
  if (fi->next)
    {
      /* Pull off either the next frame pointer or the stack pointer */
      FRAME_ADDR next_next_frame =
	(fi->next->bottom ?
	 fi->next->bottom :
	 read_register (SP_REGNUM));
      for (regnum = O0_REGNUM; regnum < O0_REGNUM+8; regnum++)
	saved_regs_addr->regs[regnum] = next_next_frame + regnum * 4;
    }
  /* Otherwise, whatever we would get from ptrace(GETREGS) is accurate */
  saved_regs_addr->regs[SP_REGNUM] = FRAME_FP (fi);
}

/* Push an empty stack frame, and record in it the current PC, regs, etc.

   We save the non-windowed registers and the ins.  The locals and outs
   are new; they don't need to be saved. The i's and l's of
   the last frame were already saved on the stack

   The return pointer register %i7 does not have the pc saved into it
   (return from this frame will be accomplished by a POP_FRAME).  In
   fact, we must leave it unclobbered, since we must preserve it in
   the calling routine except across call instructions.  I'm not sure
   the preceding sentence is true; isn't it based on confusing the %i7
   saved in the dummy frame versus the one saved in the frame of the
   calling routine?  */

/* Definitely see tm-sparc.h for more doc of the frame format here.  */

void
sparc_push_dummy_frame ()
{
  CORE_ADDR sp, old_sp;
  char register_temp[0x140];

  old_sp = sp = read_register (SP_REGNUM);

  /* Y, PS, WIM, TBR, PC, NPC, FPS, CPS regs */
  read_register_bytes (REGISTER_BYTE (Y_REGNUM), &register_temp[0],
		       REGISTER_RAW_SIZE (Y_REGNUM) * 8);

  read_register_bytes (REGISTER_BYTE (O0_REGNUM), &register_temp[8 * 4],
		       REGISTER_RAW_SIZE (O0_REGNUM) * 8);

  read_register_bytes (REGISTER_BYTE (G0_REGNUM), &register_temp[16 * 4],
		       REGISTER_RAW_SIZE (G0_REGNUM) * 8);

  read_register_bytes (REGISTER_BYTE (FP0_REGNUM), &register_temp[24 * 4],
		       REGISTER_RAW_SIZE (FP0_REGNUM) * 32);

  sp -= 0x140;

  write_register (SP_REGNUM, sp);

  write_memory (sp + 0x60, &register_temp[0], (8 + 8 + 8 + 32) * 4);

  write_register (FP_REGNUM, old_sp);
}

/* Discard from the stack the innermost frame, restoring all saved registers.

   Note that the values stored in fsr by get_frame_saved_regs are *in
   the context of the called frame*.  What this means is that the i
   regs of fsr must be restored into the o regs of the (calling) frame that
   we pop into.  We don't care about the output regs of the calling frame,
   since unless it's a dummy frame, it won't have any output regs in it.

   We never have to bother with %l (local) regs, since the called routine's
   locals get tossed, and the calling routine's locals are already saved
   on its stack.  */

/* Definitely see tm-sparc.h for more doc of the frame format here.  */

void
sparc_pop_frame ()
{
  register FRAME frame = get_current_frame ();
  register CORE_ADDR pc;
  struct frame_saved_regs fsr;
  struct frame_info *fi;
  char raw_buffer[REGISTER_BYTES];

  fi = get_frame_info (frame);
  get_frame_saved_regs (fi, &fsr);
  if (fsr.regs[FP0_REGNUM])
    {
      read_memory (fsr.regs[FP0_REGNUM], raw_buffer, 32 * 4);
      write_register_bytes (REGISTER_BYTE (FP0_REGNUM), raw_buffer, 32 * 4);
    }
  if (fsr.regs[G1_REGNUM])
    {
      read_memory (fsr.regs[G1_REGNUM], raw_buffer, 7 * 4);
      write_register_bytes (REGISTER_BYTE (G1_REGNUM), raw_buffer, 7 * 4);
    }
  if (fsr.regs[I0_REGNUM])
    {
      CORE_ADDR sp;

      char reg_temp[REGISTER_BYTES];

      read_memory (fsr.regs[I0_REGNUM], raw_buffer, 8 * 4);

      /* Get the ins and locals which we are about to restore.  Just
	 moving the stack pointer is all that is really needed, except
	 store_inferior_registers is then going to write the ins and
	 locals from the registers array, so we need to muck with the
	 registers array.  */
      sp = fsr.regs[SP_REGNUM];
      read_memory (sp, reg_temp, REGISTER_RAW_SIZE (L0_REGNUM) * 16);

      /* Restore the out registers.
	 Among other things this writes the new stack pointer.  */
      write_register_bytes (REGISTER_BYTE (O0_REGNUM), raw_buffer,
			    REGISTER_RAW_SIZE (O0_REGNUM) * 8);

      write_register_bytes (REGISTER_BYTE (L0_REGNUM), reg_temp,
			    REGISTER_RAW_SIZE (L0_REGNUM) * 16);
    }
  if (fsr.regs[PS_REGNUM])
    write_register (PS_REGNUM, read_memory_integer (fsr.regs[PS_REGNUM], 4));
  if (fsr.regs[Y_REGNUM])
    write_register (Y_REGNUM, read_memory_integer (fsr.regs[Y_REGNUM], 4));
  if (fsr.regs[PC_REGNUM])
    {
      /* Explicitly specified PC (and maybe NPC) -- just restore them. */
      write_register (PC_REGNUM, read_memory_integer (fsr.regs[PC_REGNUM], 4));
      if (fsr.regs[NPC_REGNUM])
	write_register (NPC_REGNUM,
			read_memory_integer (fsr.regs[NPC_REGNUM], 4));
    }
  else if (fsr.regs[I7_REGNUM])
    {
      /* Return address in %i7 -- adjust it, then restore PC and NPC from it */
      pc = PC_ADJUST (read_memory_integer (fsr.regs[I7_REGNUM], 4));
      write_register (PC_REGNUM,  pc);
      write_register (NPC_REGNUM, pc + 4);
    }
  flush_cached_frames ();
  set_current_frame ( create_new_frame (read_register (FP_REGNUM),
					read_pc ()));
}

/* On the Sun 4 under SunOS, the compile will leave a fake insn which
   encodes the structure size being returned.  If we detect such
   a fake insn, step past it.  */

CORE_ADDR
sparc_pc_adjust(pc)
     CORE_ADDR pc;
{
  unsigned long insn;
  char buf[4];
  int err;

  err = target_read_memory (pc + 8, buf, sizeof(long));
  insn = extract_unsigned_integer (buf, 4);
  if ((err == 0) && (insn & 0xfffffe00) == 0)
    return pc+12;
  else
    return pc+8;
}


/* Structure of SPARC extended floating point numbers.
   This information is not currently used by GDB, since no current SPARC
   implementations support extended float.  */

const struct ext_format ext_format_sparc = {
/* tot sbyte smask expbyte manbyte */
   16, 0,    0x80, 0,1,	   4,8,		/* sparc */
};

#ifdef USE_PROC_FS	/* Target dependent support for /proc */

/*  The /proc interface divides the target machine's register set up into
    two different sets, the general register set (gregset) and the floating
    point register set (fpregset).  For each set, there is an ioctl to get
    the current register set and another ioctl to set the current values.

    The actual structure passed through the ioctl interface is, of course,
    naturally machine dependent, and is different for each set of registers.
    For the sparc for example, the general register set is typically defined
    by:

	typedef int gregset_t[38];

	#define	R_G0	0
	...
	#define	R_TBR	37

    and the floating point set by:

	typedef struct prfpregset {
		union { 
			u_long  pr_regs[32]; 
			double  pr_dregs[16];
		} pr_fr;
		void *  pr_filler;
		u_long  pr_fsr;
		u_char  pr_qcnt;
		u_char  pr_q_entrysize;
		u_char  pr_en;
		u_long  pr_q[64];
	} prfpregset_t;

    These routines provide the packing and unpacking of gregset_t and
    fpregset_t formatted data.

 */


/*  Given a pointer to a general register set in /proc format (gregset_t *),
    unpack the register contents and supply them as gdb's idea of the current
    register values. */

void
supply_gregset (gregsetp)
prgregset_t *gregsetp;
{
  register int regi;
  register prgreg_t *regp = (prgreg_t *) gregsetp;

  /* GDB register numbers for Gn, On, Ln, In all match /proc reg numbers.  */
  for (regi = G0_REGNUM ; regi <= I7_REGNUM ; regi++)
    {
      supply_register (regi, (char *) (regp + regi));
    }

  /* These require a bit more care.  */
  supply_register (PS_REGNUM, (char *) (regp + R_PS));
  supply_register (PC_REGNUM, (char *) (regp + R_PC));
  supply_register (NPC_REGNUM,(char *) (regp + R_nPC));
  supply_register (Y_REGNUM,  (char *) (regp + R_Y));
}

void
fill_gregset (gregsetp, regno)
prgregset_t *gregsetp;
int regno;
{
  int regi;
  register prgreg_t *regp = (prgreg_t *) gregsetp;
  extern char registers[];

  for (regi = 0 ; regi <= R_I7 ; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  *(regp + regi) = *(int *) &registers[REGISTER_BYTE (regi)];
	}
    }
  if ((regno == -1) || (regno == PS_REGNUM))
    {
      *(regp + R_PS) = *(int *) &registers[REGISTER_BYTE (PS_REGNUM)];
    }
  if ((regno == -1) || (regno == PC_REGNUM))
    {
      *(regp + R_PC) = *(int *) &registers[REGISTER_BYTE (PC_REGNUM)];
    }
  if ((regno == -1) || (regno == NPC_REGNUM))
    {
      *(regp + R_nPC) = *(int *) &registers[REGISTER_BYTE (NPC_REGNUM)];
    }
  if ((regno == -1) || (regno == Y_REGNUM))
    {
      *(regp + R_Y) = *(int *) &registers[REGISTER_BYTE (Y_REGNUM)];
    }
}

#if defined (FP0_REGNUM)

/*  Given a pointer to a floating point register set in /proc format
    (fpregset_t *), unpack the register contents and supply them as gdb's
    idea of the current floating point register values. */

void 
supply_fpregset (fpregsetp)
prfpregset_t *fpregsetp;
{
  register int regi;
  char *from;
  
  for (regi = FP0_REGNUM ; regi < FP0_REGNUM+32 ; regi++)
    {
      from = (char *) &fpregsetp->pr_fr.pr_regs[regi-FP0_REGNUM];
      supply_register (regi, from);
    }
  supply_register (FPS_REGNUM, (char *) &(fpregsetp->pr_fsr));
}

/*  Given a pointer to a floating point register set in /proc format
    (fpregset_t *), update the register specified by REGNO from gdb's idea
    of the current floating point register set.  If REGNO is -1, update
    them all. */

void
fill_fpregset (fpregsetp, regno)
prfpregset_t *fpregsetp;
int regno;
{
  int regi;
  char *to;
  char *from;
  extern char registers[];

  for (regi = FP0_REGNUM ; regi < FP0_REGNUM+32 ; regi++)
    {
      if ((regno == -1) || (regno == regi))
	{
	  from = (char *) &registers[REGISTER_BYTE (regi)];
	  to = (char *) &fpregsetp->pr_fr.pr_regs[regi-FP0_REGNUM];
	  memcpy (to, from, REGISTER_RAW_SIZE (regi));
	}
    }
  if ((regno == -1) || (regno == FPS_REGNUM))
    {
      fpregsetp->pr_fsr = *(int *) &registers[REGISTER_BYTE (FPS_REGNUM)];
    }
}

#endif	/* defined (FP0_REGNUM) */

#endif  /* USE_PROC_FS */


#ifdef GET_LONGJMP_TARGET

/* Figure out where the longjmp will land.  We expect that we have just entered
   longjmp and haven't yet setup the stack frame, so the args are still in the
   output regs.  %o0 (O0_REGNUM) points at the jmp_buf structure from which we
   extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
   This routine returns true on success */

int
get_longjmp_target(pc)
     CORE_ADDR *pc;
{
  CORE_ADDR jb_addr;
#define LONGJMP_TARGET_SIZE 4
  char buf[LONGJMP_TARGET_SIZE];

  jb_addr = read_register(O0_REGNUM);

  if (target_read_memory(jb_addr + JB_PC * JB_ELEMENT_SIZE, buf,
			 LONGJMP_TARGET_SIZE))
    return 0;

  *pc = extract_address (buf, LONGJMP_TARGET_SIZE);

  return 1;
}
#endif /* GET_LONGJMP_TARGET */

/* So far used only for sparc solaris.  In sparc solaris, we recognize
   a trampoline by it's section name.  That is, if the pc is in a
   section named ".plt" then we are in a trampline.  */

int
in_solib_trampoline(pc, name)
     CORE_ADDR pc;
     char *name;
{
  struct obj_section *s;
  int retval = 0;
  
  s = find_pc_section(pc);
  
  retval = (s != NULL
	    && s->sec_ptr->name != NULL
	    && STREQ (s->sec_ptr->name, ".plt"));
  return(retval);
}


/* Target-machine dependent code for Renesas H8/300, for GDB.

   Copyright 1988, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1998,
   1999, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

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

/*
   Contributed by Steve Chamberlain
   sac@cygnus.com
 */

#include "defs.h"
#include "value.h"
#include "inferior.h"
#include "symfile.h"
#include "arch-utils.h"
#include "regcache.h"
#include "gdbcore.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "gdb_assert.h"
#include "dis-asm.h"

/* Extra info which is saved in each frame_info. */
struct frame_extra_info
{
  CORE_ADDR from_pc;
};

enum
{
  h8300_reg_size = 2,
  h8300h_reg_size = 4,
  h8300_max_reg_size = 4,
};

static int is_h8300hmode (struct gdbarch *gdbarch);
static int is_h8300smode (struct gdbarch *gdbarch);
static int is_h8300sxmode (struct gdbarch *gdbarch);
static int is_h8300_normal_mode (struct gdbarch *gdbarch);

#define BINWORD (is_h8300hmode (current_gdbarch) && \
		  !is_h8300_normal_mode (current_gdbarch) ? h8300h_reg_size : h8300_reg_size)

enum gdb_regnum
{
  E_R0_REGNUM, E_ER0_REGNUM = E_R0_REGNUM, E_ARG0_REGNUM = E_R0_REGNUM,
					   E_RET0_REGNUM = E_R0_REGNUM,
  E_R1_REGNUM, E_ER1_REGNUM = E_R1_REGNUM, E_RET1_REGNUM = E_R1_REGNUM,
  E_R2_REGNUM, E_ER2_REGNUM = E_R2_REGNUM, E_ARGLAST_REGNUM = E_R2_REGNUM,
  E_R3_REGNUM, E_ER3_REGNUM = E_R3_REGNUM,
  E_R4_REGNUM, E_ER4_REGNUM = E_R4_REGNUM,
  E_R5_REGNUM, E_ER5_REGNUM = E_R5_REGNUM,
  E_R6_REGNUM, E_ER6_REGNUM = E_R6_REGNUM, E_FP_REGNUM = E_R6_REGNUM,
  E_SP_REGNUM,
  E_CCR_REGNUM,
  E_PC_REGNUM,
  E_CYCLES_REGNUM,
  E_TICK_REGNUM, E_EXR_REGNUM = E_TICK_REGNUM,
  E_INST_REGNUM, E_TICKS_REGNUM = E_INST_REGNUM,
  E_INSTS_REGNUM,
  E_MACH_REGNUM,
  E_MACL_REGNUM,
  E_SBR_REGNUM,
  E_VBR_REGNUM
};

#define E_PSEUDO_CCR_REGNUM (NUM_REGS)
#define E_PSEUDO_EXR_REGNUM (NUM_REGS+1)

#define UNSIGNED_SHORT(X) ((X) & 0xffff)

#define IS_PUSH(x) ((x & 0xfff0)==0x6df0)
#define IS_PUSH_FP(x) (x == 0x6df6)
#define IS_MOVE_FP(x) (x == 0x0d76 || x == 0x0ff6)
#define IS_MOV_SP_FP(x) (x == 0x0d76 || x == 0x0ff6)
#define IS_SUB2_SP(x) (x==0x1b87)
#define IS_SUB4_SP(x) (x==0x1b97)
#define IS_SUBL_SP(x) (x==0x7a37)
#define IS_MOVK_R5(x) (x==0x7905)
#define IS_SUB_R5SP(x) (x==0x1957)

/* If the instruction at PC is an argument register spill, return its
   length.  Otherwise, return zero.

   An argument register spill is an instruction that moves an argument
   from the register in which it was passed to the stack slot in which
   it really lives.  It is a byte, word, or longword move from an
   argument register to a negative offset from the frame pointer.
   
   CV, 2003-06-16: Or, in optimized code or when the `register' qualifier
   is used, it could be a byte, word or long move to registers r3-r5.  */

static int
h8300_is_argument_spill (CORE_ADDR pc)
{
  int w = read_memory_unsigned_integer (pc, 2);

  if (((w & 0xff88) == 0x0c88                 /* mov.b Rsl, Rdl */
       || (w & 0xff88) == 0x0d00              /* mov.w Rs, Rd */
       || (w & 0xff88) == 0x0f80)             /* mov.l Rs, Rd */
      && (w & 0x70) <= 0x20                   /* Rs is R0, R1 or R2 */
      && (w & 0x7) >= 0x3 && (w & 0x7) <= 0x5)/* Rd is R3, R4 or R5 */
    return 2;

  if ((w & 0xfff0) == 0x6ee0                  /* mov.b Rs,@(d:16,er6) */
      && 8 <= (w & 0xf) && (w & 0xf) <= 10)   /* Rs is R0L, R1L, or R2L  */
    {
      int w2 = read_memory_integer (pc + 2, 2);

      /* ... and d:16 is negative.  */
      if (w2 < 0)
        return 4;
    }
  else if (w == 0x7860)
    {
      int w2 = read_memory_integer (pc + 2, 2);

      if ((w2 & 0xfff0) == 0x6aa0)              /* mov.b Rs, @(d:24,er6) */
        {
          LONGEST disp = read_memory_integer (pc + 4, 4);

          /* ... and d:24 is negative.  */
          if (disp < 0 && disp > 0xffffff)
            return 8;
        }
    }
  else if ((w & 0xfff0) == 0x6fe0             /* mov.w Rs,@(d:16,er6) */
           && (w & 0xf) <= 2)                 /* Rs is R0, R1, or R2 */
    {
      int w2 = read_memory_integer (pc + 2, 2);

      /* ... and d:16 is negative.  */
      if (w2 < 0)
        return 4;
    }
  else if (w == 0x78e0)
    {
      int w2 = read_memory_integer (pc + 2, 2);

      if ((w2 & 0xfff0) == 0x6ba0)              /* mov.b Rs, @(d:24,er6) */
        {
          LONGEST disp = read_memory_integer (pc + 4, 4);

          /* ... and d:24 is negative.  */
          if (disp < 0 && disp > 0xffffff)
            return 8;
        }
    }
  else if (w == 0x0100)
    {
      int w2 = read_memory_integer (pc + 2, 2);

      if ((w2 & 0xfff0) == 0x6fe0             /* mov.l Rs,@(d:16,er6) */
          && (w2 & 0xf) <= 2)                /* Rs is ER0, ER1, or ER2 */
        {
          int w3 = read_memory_integer (pc + 4, 2);

          /* ... and d:16 is negative.  */
          if (w3 < 0)
            return 6;
        }
      else if (w2 == 0x78e0)
        {
          int w3 = read_memory_integer (pc + 4, 2);

          if ((w3 & 0xfff0) == 0x6ba0)          /* mov.l Rs, @(d:24,er6) */
            {
              LONGEST disp = read_memory_integer (pc + 6, 4);

              /* ... and d:24 is negative.  */
              if (disp < 0 && disp > 0xffffff)
                return 10;
            }
        }
    }

  return 0;
}

static CORE_ADDR
h8300_skip_prologue (CORE_ADDR start_pc)
{
  short int w;
  int adjust = 0;

  /* Skip past all push and stm insns.  */
  while (1)
    {
      w = read_memory_unsigned_integer (start_pc, 2);
      /* First look for push insns.  */
      if (w == 0x0100 || w == 0x0110 || w == 0x0120 || w == 0x0130)
	{
	  w = read_memory_unsigned_integer (start_pc + 2, 2);
	  adjust = 2;
	}

      if (IS_PUSH (w))
	{
	  start_pc += 2 + adjust;
	  w = read_memory_unsigned_integer (start_pc, 2);
	  continue;
	}
      adjust = 0;
      break;
    }

  /* Skip past a move to FP, either word or long sized */
  w = read_memory_unsigned_integer (start_pc, 2);
  if (w == 0x0100)
    {
      w = read_memory_unsigned_integer (start_pc + 2, 2);
      adjust += 2;
    }

  if (IS_MOVE_FP (w))
    {
      start_pc += 2 + adjust;
      w = read_memory_unsigned_integer (start_pc, 2);
    }

  /* Check for loading either a word constant into r5;
     long versions are handled by the SUBL_SP below.  */
  if (IS_MOVK_R5 (w))
    {
      start_pc += 2;
      w = read_memory_unsigned_integer (start_pc, 2);
    }

  /* Now check for subtracting r5 from sp, word sized only.  */
  if (IS_SUB_R5SP (w))
    {
      start_pc += 2 + adjust;
      w = read_memory_unsigned_integer (start_pc, 2);
    }

  /* Check for subs #2 and subs #4. */
  while (IS_SUB2_SP (w) || IS_SUB4_SP (w))
    {
      start_pc += 2 + adjust;
      w = read_memory_unsigned_integer (start_pc, 2);
    }

  /* Check for a 32bit subtract.  */
  if (IS_SUBL_SP (w))
    start_pc += 6 + adjust;

  /* Skip past another possible stm insn for registers R3 to R5 (possibly used
     for register qualified arguments.  */
  w = read_memory_unsigned_integer (start_pc, 2);
  /* First look for push insns.  */
  if (w == 0x0110 || w == 0x0120 || w == 0x0130)
    {
      w = read_memory_unsigned_integer (start_pc + 2, 2);
      if (IS_PUSH (w) && (w & 0xf) >= 0x3 && (w & 0xf) <= 0x5)
	start_pc += 4;
    }

  /* Check for spilling an argument register to the stack frame.
     This could also be an initializing store from non-prologue code,
     but I don't think there's any harm in skipping that.  */
  for (;;)
    {
      int spill_size = h8300_is_argument_spill (start_pc);
      if (spill_size == 0)
        break;
      start_pc += spill_size;
    }

  return start_pc;
}

/* Fetch the instruction at ADDR, returning 0 if ADDR is beyond LIM or
   is not the address of a valid instruction, the address of the next
   instruction beyond ADDR otherwise.  *PWORD1 receives the first word
   of the instruction. */

static CORE_ADDR
h8300_next_prologue_insn (CORE_ADDR addr, 
			  CORE_ADDR lim, 
			  unsigned short* pword1)
{
  char buf[2];
  if (addr < lim + 8)
    {
      read_memory (addr, buf, 2);
      *pword1 = extract_signed_integer (buf, 2);

      return addr + 2;
    }
  return 0;
}

/* Examine the prologue of a function.  `ip' points to the first instruction.
   `limit' is the limit of the prologue (e.g. the addr of the first
   linenumber, or perhaps the program counter if we're stepping through).
   `frame_sp' is the stack pointer value in use in this frame.
   `fsr' is a pointer to a frame_saved_regs structure into which we put
   info about the registers saved by this frame.
   `fi' is a struct frame_info pointer; we fill in various fields in it
   to reflect the offsets of the arg pointer and the locals pointer.  */

/* Any function with a frame looks like this
   SECOND ARG
   FIRST ARG
   RET PC
   SAVED R2
   SAVED R3
   SAVED FP   <-FP POINTS HERE
   LOCALS0
   LOCALS1    <-SP POINTS HERE
 */

static CORE_ADDR
h8300_examine_prologue (CORE_ADDR ip, CORE_ADDR limit,
			CORE_ADDR after_prolog_fp, CORE_ADDR *fsr,
			struct frame_info *fi)
{
  CORE_ADDR next_ip;
  int r;
  int have_fp = 0;
  unsigned short insn_word;
  /* Number of things pushed onto stack, starts at 2/4, 'cause the
     PC is already there */
  unsigned int reg_save_depth = BINWORD;

  unsigned int auto_depth = 0;	/* Number of bytes of autos */

  char in_frame[11];		/* One for each reg */

  int adjust = 0;

  memset (in_frame, 1, 11);
  for (r = 0; r < 8; r++)
    {
      fsr[r] = 0;
    }
  if (after_prolog_fp == 0)
    {
      after_prolog_fp = read_register (E_SP_REGNUM);
    }

  /* If the PC isn't valid, quit now.  */
  if (ip == 0 || ip & (is_h8300hmode (current_gdbarch) &&
			 !is_h8300_normal_mode (current_gdbarch) ? ~0xffffff : ~0xffff))
    return 0;

  next_ip = h8300_next_prologue_insn (ip, limit, &insn_word);

  if (insn_word == 0x0100)	/* mov.l */
    {
      insn_word = read_memory_unsigned_integer (ip + 2, 2);
      adjust = 2;
    }

  /* Skip over any fp push instructions */
  fsr[E_FP_REGNUM] = after_prolog_fp;
  while (next_ip && IS_PUSH_FP (insn_word))
    {
      ip = next_ip + adjust;

      in_frame[insn_word & 0x7] = reg_save_depth;
      next_ip = h8300_next_prologue_insn (ip, limit, &insn_word);
      reg_save_depth += 2 + adjust;
    }

  /* Is this a move into the fp */
  if (next_ip && IS_MOV_SP_FP (insn_word))
    {
      ip = next_ip;
      next_ip = h8300_next_prologue_insn (ip, limit, &insn_word);
      have_fp = 1;
    }

  /* Skip over any stack adjustment, happens either with a number of
     sub#2,sp or a mov #x,r5 sub r5,sp */

  if (next_ip && (IS_SUB2_SP (insn_word) || IS_SUB4_SP (insn_word)))
    {
      while (next_ip && (IS_SUB2_SP (insn_word) || IS_SUB4_SP (insn_word)))
	{
	  auto_depth += IS_SUB2_SP (insn_word) ? 2 : 4;
	  ip = next_ip;
	  next_ip = h8300_next_prologue_insn (ip, limit, &insn_word);
	}
    }
  else
    {
      if (next_ip && IS_MOVK_R5 (insn_word))
	{
	  ip = next_ip;
	  next_ip = h8300_next_prologue_insn (ip, limit, &insn_word);
	  auto_depth += insn_word;

	  next_ip = h8300_next_prologue_insn (next_ip, limit, &insn_word);
	  auto_depth += insn_word;
	}
      if (next_ip && IS_SUBL_SP (insn_word))
	{
	  ip = next_ip;
	  auto_depth += read_memory_unsigned_integer (ip, 4);
	  ip += 4;

	  next_ip = h8300_next_prologue_insn (ip, limit, &insn_word);
	}
    }

  /* Now examine the push insns to determine where everything lives
     on the stack.  */
  while (1)
    {
      adjust = 0;
      if (!next_ip)
	break;

      if (insn_word == 0x0100)
	{
	  ip = next_ip;
	  next_ip = h8300_next_prologue_insn (ip, limit, &insn_word);
	  adjust = 2;
	}

      if (IS_PUSH (insn_word))
	{
	  auto_depth += 2 + adjust;
	  fsr[insn_word & 0x7] = after_prolog_fp - auto_depth;
	  ip = next_ip;
	  next_ip = h8300_next_prologue_insn (ip, limit, &insn_word);
	  continue;
	}

      /* Now check for push multiple insns.  */
      if (insn_word == 0x0110 || insn_word == 0x0120 || insn_word == 0x0130)
	{
	  int count = ((insn_word >> 4) & 0xf) + 1;
	  int start, i;

	  ip = next_ip;
	  next_ip = h8300_next_prologue_insn (ip, limit, &insn_word);
	  start = insn_word & 0x7;

	  for (i = start; i < start + count; i++)
	    {
	      auto_depth += 4;
	      fsr[i] = after_prolog_fp - auto_depth;
	    }
	}
      break;
    }

  /* The PC is at a known place */
  get_frame_extra_info (fi)->from_pc =
    read_memory_unsigned_integer (after_prolog_fp + BINWORD, BINWORD);

  /* Rememeber any others too */
  in_frame[E_PC_REGNUM] = 0;

  if (have_fp)
    /* We keep the old FP in the SP spot */
    fsr[E_SP_REGNUM] = read_memory_unsigned_integer (fsr[E_FP_REGNUM], 
						     BINWORD);
  else
    fsr[E_SP_REGNUM] = after_prolog_fp + auto_depth;

  return (ip);
}

static void
h8300_frame_init_saved_regs (struct frame_info *fi)
{
  CORE_ADDR func_addr, func_end;

  if (!deprecated_get_frame_saved_regs (fi))
    {
      frame_saved_regs_zalloc (fi);

      /* Find the beginning of this function, so we can analyze its
	 prologue. */
      if (find_pc_partial_function (get_frame_pc (fi), NULL, 
				    &func_addr, &func_end))
        {
	  struct symtab_and_line sal = find_pc_line (func_addr, 0);
	  CORE_ADDR limit = (sal.end && sal.end < get_frame_pc (fi)) 
	    ? sal.end : get_frame_pc (fi);
	  /* This will fill in fields in fi. */
	  h8300_examine_prologue (func_addr, limit, get_frame_base (fi),
				  deprecated_get_frame_saved_regs (fi), fi);
	}
      /* Else we're out of luck (can't debug completely stripped code). 
	 FIXME. */
    }
}

/* Given a GDB frame, determine the address of the calling function's
   frame.  This will be used to create a new GDB frame struct, and
   then DEPRECATED_INIT_EXTRA_FRAME_INFO and DEPRECATED_INIT_FRAME_PC
   will be called for the new frame.

   For us, the frame address is its stack pointer value, so we look up
   the function prologue to determine the caller's sp value, and
   return it.  */

static CORE_ADDR
h8300_frame_chain (struct frame_info *thisframe)
{
  if (deprecated_pc_in_call_dummy (get_frame_pc (thisframe)))
    {				/* initialize the from_pc now */
      get_frame_extra_info (thisframe)->from_pc =
	deprecated_read_register_dummy (get_frame_pc (thisframe),
					get_frame_base (thisframe),
					E_PC_REGNUM);
      return get_frame_base (thisframe);
    }
  return deprecated_get_frame_saved_regs (thisframe)[E_SP_REGNUM];
}

/* Return the saved PC from this frame.

   If the frame has a memory copy of SRP_REGNUM, use that.  If not,
   just use the register SRP_REGNUM itself.  */

static CORE_ADDR
h8300_frame_saved_pc (struct frame_info *frame)
{
  if (deprecated_pc_in_call_dummy (get_frame_pc (frame)))
    return deprecated_read_register_dummy (get_frame_pc (frame),
					   get_frame_base (frame),
					   E_PC_REGNUM);
  else
    return get_frame_extra_info (frame)->from_pc;
}

static void
h8300_init_extra_frame_info (int fromleaf, struct frame_info *fi)
{
  if (!get_frame_extra_info (fi))
    {
      frame_extra_info_zalloc (fi, sizeof (struct frame_extra_info));
      get_frame_extra_info (fi)->from_pc = 0;
      
      if (!get_frame_pc (fi))
        {
	  if (get_next_frame (fi))
	    deprecated_update_frame_pc_hack (fi, h8300_frame_saved_pc (get_next_frame (fi)));
	}
      h8300_frame_init_saved_regs (fi);
    }
}

/* Function: push_dummy_call
   Setup the function arguments for calling a function in the inferior.
   In this discussion, a `word' is 16 bits on the H8/300s, and 32 bits
   on the H8/300H.

   There are actually two ABI's here: -mquickcall (the default) and
   -mno-quickcall.  With -mno-quickcall, all arguments are passed on
   the stack after the return address, word-aligned.  With
   -mquickcall, GCC tries to use r0 -- r2 to pass registers.  Since
   GCC doesn't indicate in the object file which ABI was used to
   compile it, GDB only supports the default --- -mquickcall.

   Here are the rules for -mquickcall, in detail:

   Each argument, whether scalar or aggregate, is padded to occupy a
   whole number of words.  Arguments smaller than a word are padded at
   the most significant end; those larger than a word are padded at
   the least significant end.

   The initial arguments are passed in r0 -- r2.  Earlier arguments go in
   lower-numbered registers.  Multi-word arguments are passed in
   consecutive registers, with the most significant end in the
   lower-numbered register.

   If an argument doesn't fit entirely in the remaining registers, it
   is passed entirely on the stack.  Stack arguments begin just after
   the return address.  Once an argument has overflowed onto the stack
   this way, all subsequent arguments are passed on the stack.

   The above rule has odd consequences.  For example, on the h8/300s,
   if a function takes two longs and an int as arguments:
   - the first long will be passed in r0/r1,
   - the second long will be passed entirely on the stack, since it
     doesn't fit in r2,
   - and the int will be passed on the stack, even though it could fit
     in r2.

   A weird exception: if an argument is larger than a word, but not a
   whole number of words in length (before padding), it is passed on
   the stack following the rules for stack arguments above, even if
   there are sufficient registers available to hold it.  Stranger
   still, the argument registers are still `used up' --- even though
   there's nothing in them.

   So, for example, on the h8/300s, if a function expects a three-byte
   structure and an int, the structure will go on the stack, and the
   int will go in r2, not r0.
  
   If the function returns an aggregate type (struct, union, or class)
   by value, the caller must allocate space to hold the return value,
   and pass the callee a pointer to this space as an invisible first
   argument, in R0.

   For varargs functions, the last fixed argument and all the variable
   arguments are always passed on the stack.  This means that calls to
   varargs functions don't work properly unless there is a prototype
   in scope.

   Basically, this ABI is not good, for the following reasons:
   - You can't call vararg functions properly unless a prototype is in scope.
   - Structure passing is inconsistent, to no purpose I can see.
   - It often wastes argument registers, of which there are only three
     to begin with.  */

static CORE_ADDR
h8300_push_dummy_call (struct gdbarch *gdbarch, struct value *function,
		       struct regcache *regcache, CORE_ADDR bp_addr, int nargs,
		       struct value **args, CORE_ADDR sp, int struct_return,
		       CORE_ADDR struct_addr)
{
  int stack_alloc = 0, stack_offset = 0;
  int wordsize = BINWORD;
  int reg = E_ARG0_REGNUM;
  int argument;

  /* First, make sure the stack is properly aligned.  */
  sp = align_down (sp, wordsize);

  /* Now make sure there's space on the stack for the arguments.  We
     may over-allocate a little here, but that won't hurt anything.  */
  for (argument = 0; argument < nargs; argument++)
    stack_alloc += align_up (TYPE_LENGTH (VALUE_TYPE (args[argument])),
                             wordsize);
  sp -= stack_alloc;

  /* Now load as many arguments as possible into registers, and push
     the rest onto the stack.
     If we're returning a structure by value, then we must pass a
     pointer to the buffer for the return value as an invisible first
     argument.  */
  if (struct_return)
    regcache_cooked_write_unsigned (regcache, reg++, struct_addr);

  for (argument = 0; argument < nargs; argument++)
    {
      struct type *type = VALUE_TYPE (args[argument]);
      int len = TYPE_LENGTH (type);
      char *contents = (char *) VALUE_CONTENTS (args[argument]);

      /* Pad the argument appropriately.  */
      int padded_len = align_up (len, wordsize);
      char *padded = alloca (padded_len);

      memset (padded, 0, padded_len);
      memcpy (len < wordsize ? padded + padded_len - len : padded,
              contents, len);

      /* Could the argument fit in the remaining registers?  */
      if (padded_len <= (E_ARGLAST_REGNUM - reg + 1) * wordsize)
        {
          /* Are we going to pass it on the stack anyway, for no good
             reason?  */
          if (len > wordsize && len % wordsize)
            {
              /* I feel so unclean.  */
              write_memory (sp + stack_offset, padded, padded_len);
              stack_offset += padded_len;

              /* That's right --- even though we passed the argument
                 on the stack, we consume the registers anyway!  Love
                 me, love my dog.  */
              reg += padded_len / wordsize;
            }
          else
            {
              /* Heavens to Betsy --- it's really going in registers!
                 It would be nice if we could use write_register_bytes
                 here, but on the h8/300s, there are gaps between
                 the registers in the register file.  */
              int offset;

              for (offset = 0; offset < padded_len; offset += wordsize)
                {
                  ULONGEST word = extract_unsigned_integer (padded + offset, 
							    wordsize);
		  regcache_cooked_write_unsigned (regcache, reg++, word);
                }
            }
        }
      else
        {
          /* It doesn't fit in registers!  Onto the stack it goes.  */
          write_memory (sp + stack_offset, padded, padded_len);
          stack_offset += padded_len;

          /* Once one argument has spilled onto the stack, all
             subsequent arguments go on the stack.  */
          reg = E_ARGLAST_REGNUM + 1;
        }
    }

  /* Store return address.  */
  sp -= wordsize;
  write_memory_unsigned_integer (sp, wordsize, bp_addr);

  /* Update stack pointer.  */
  regcache_cooked_write_unsigned (regcache, E_SP_REGNUM, sp);

  return sp;
}

/* Function: h8300_pop_frame
   Restore the machine to the state it had before the current frame 
   was created.  Usually used either by the "RETURN" command, or by
   call_function_by_hand after the dummy_frame is finished. */

static void
h8300_pop_frame (void)
{
  unsigned regno;
  struct frame_info *frame = get_current_frame ();

  if (deprecated_pc_in_call_dummy (get_frame_pc (frame)))
    {
      deprecated_pop_dummy_frame ();
    }
  else
    {
      for (regno = 0; regno < 8; regno++)
	{
	  /* Don't forget E_SP_REGNUM is a frame_saved_regs struct is the
	     actual value we want, not the address of the value we want.  */
	  if (deprecated_get_frame_saved_regs (frame)[regno] && regno != E_SP_REGNUM)
	    write_register (regno,
			    read_memory_integer 
			    (deprecated_get_frame_saved_regs (frame)[regno], BINWORD));
	  else if (deprecated_get_frame_saved_regs (frame)[regno] && regno == E_SP_REGNUM)
	    write_register (regno, get_frame_base (frame) + 2 * BINWORD);
	}

      /* Don't forget to update the PC too!  */
      write_register (E_PC_REGNUM, get_frame_extra_info (frame)->from_pc);
    }
  flush_cached_frames ();
}

/* Function: extract_return_value
   Figure out where in REGBUF the called function has left its return value.
   Copy that into VALBUF.  Be sure to account for CPU type.   */

static void
h8300_extract_return_value (struct type *type, struct regcache *regcache,
			    void *valbuf)
{
  int len = TYPE_LENGTH (type);
  ULONGEST c, addr;

  switch (len)
    {
      case 1:
      case 2:
	regcache_cooked_read_unsigned (regcache, E_RET0_REGNUM, &c);
	store_unsigned_integer (valbuf, len, c);
	break;
      case 4:	/* Needs two registers on plain H8/300 */
	regcache_cooked_read_unsigned (regcache, E_RET0_REGNUM, &c);
	store_unsigned_integer (valbuf, 2, c);
	regcache_cooked_read_unsigned (regcache, E_RET1_REGNUM, &c);
	store_unsigned_integer ((void*)((char *)valbuf + 2), 2, c);
	break;
      case 8:	/* long long is now 8 bytes.  */
	if (TYPE_CODE (type) == TYPE_CODE_INT)
	  {
	    regcache_cooked_read_unsigned (regcache, E_RET0_REGNUM, &addr);
	    c = read_memory_unsigned_integer ((CORE_ADDR) addr, len);
	    store_unsigned_integer (valbuf, len, c);
	  }
	else
	  {
	    error ("I don't know how this 8 byte value is returned.");
	  }
	break;
    }
}

static void
h8300h_extract_return_value (struct type *type, struct regcache *regcache,
			    void *valbuf)
{
  int len = TYPE_LENGTH (type);
  ULONGEST c, addr;

  switch (len)
    {
      case 1:
      case 2:
      case 4:
	regcache_cooked_read_unsigned (regcache, E_RET0_REGNUM, &c);
	store_unsigned_integer (valbuf, len, c);
	break;
      case 8:	/* long long is now 8 bytes.  */
	if (TYPE_CODE (type) == TYPE_CODE_INT)
	  {
	    regcache_cooked_read_unsigned (regcache, E_RET0_REGNUM, &addr);
	    c = read_memory_unsigned_integer ((CORE_ADDR) addr, len);
	    store_unsigned_integer (valbuf, len, c);
	  }
	else
	  {
	    error ("I don't know how this 8 byte value is returned.");
	  }
	break;
    }
}


/* Function: store_return_value
   Place the appropriate value in the appropriate registers.
   Primarily used by the RETURN command.  */

static void
h8300_store_return_value (struct type *type, struct regcache *regcache,
			  const void *valbuf)
{
  int len = TYPE_LENGTH (type);
  ULONGEST val;

  switch (len)
    {
      case 1:
    case 2:	/* short... */
	val = extract_unsigned_integer (valbuf, len);
	regcache_cooked_write_unsigned (regcache, E_RET0_REGNUM, val);
	break;
      case 4:	/* long, float */
	val = extract_unsigned_integer (valbuf, len);
	regcache_cooked_write_unsigned (regcache, E_RET0_REGNUM,
					(val >> 16) &0xffff);
	regcache_cooked_write_unsigned (regcache, E_RET1_REGNUM, val & 0xffff);
	break;
      case 8:	/* long long, double and long double are all defined
		   as 4 byte types so far so this shouldn't happen.  */
	error ("I don't know how to return an 8 byte value.");
	break;
    }
}

static void
h8300h_store_return_value (struct type *type, struct regcache *regcache,
			   const void *valbuf)
{
  int len = TYPE_LENGTH (type);
  ULONGEST val;

  switch (len)
    {
      case 1:
      case 2:
      case 4:	/* long, float */
	val = extract_unsigned_integer (valbuf, len);
	regcache_cooked_write_unsigned (regcache, E_RET0_REGNUM, val);
	break;
      case 8:	/* long long, double and long double are all defined
		   as 4 byte types so far so this shouldn't happen.  */
	error ("I don't know how to return an 8 byte value.");
	break;
    }
}

static struct cmd_list_element *setmachinelist;

static const char *
h8300_register_name (int regno)
{
  /* The register names change depending on which h8300 processor
     type is selected. */
  static char *register_names[] = {
    "r0", "r1", "r2", "r3", "r4", "r5", "r6",
    "sp", "","pc","cycles", "tick", "inst",
    "ccr", /* pseudo register */
  };
  if (regno < 0
      || regno >= (sizeof (register_names) / sizeof (*register_names)))
    internal_error (__FILE__, __LINE__,
                    "h8300_register_name: illegal register number %d", regno);
  else
    return register_names[regno];
}

static const char *
h8300s_register_name (int regno)
{
  static char *register_names[] = {
    "er0", "er1", "er2", "er3", "er4", "er5", "er6",
    "sp", "", "pc", "cycles", "", "tick", "inst",
    "mach", "macl",
    "ccr", "exr" /* pseudo registers */
  };
  if (regno < 0
      || regno >= (sizeof (register_names) / sizeof (*register_names)))
    internal_error (__FILE__, __LINE__,
                    "h8300s_register_name: illegal register number %d", regno);
  else
    return register_names[regno];
}

static const char *
h8300sx_register_name (int regno)
{
  static char *register_names[] = {
    "er0", "er1", "er2", "er3", "er4", "er5", "er6",
    "sp", "", "pc", "cycles", "", "tick", "inst",
    "mach", "macl", "sbr", "vbr",
    "ccr", "exr" /* pseudo registers */
  };
  if (regno < 0
      || regno >= (sizeof (register_names) / sizeof (*register_names)))
    internal_error (__FILE__, __LINE__,
		    "h8300sx_register_name: illegal register number %d", regno);
  else
    return register_names[regno];
}

static void
h8300_print_register (struct gdbarch *gdbarch, struct ui_file *file,
		      struct frame_info *frame, int regno)
{
  LONGEST rval;
  const char *name = gdbarch_register_name (gdbarch, regno);

  if (!name || !*name)
    return;

  rval = get_frame_register_signed (frame, regno);

  fprintf_filtered (file, "%-14s ", name);
  if (regno == E_PSEUDO_CCR_REGNUM ||
       (regno == E_PSEUDO_EXR_REGNUM && is_h8300smode (current_gdbarch)))
    {
      fprintf_filtered (file, "0x%02x        ", (unsigned char)rval);
      print_longest (file, 'u', 1, rval);
    }
  else
    {
      fprintf_filtered (file, "0x%s  ", phex ((ULONGEST)rval, BINWORD));
      print_longest (file, 'd', 1, rval);
    }
  if (regno == E_PSEUDO_CCR_REGNUM)
    {
      /* CCR register */
      int C, Z, N, V;
      unsigned char l = rval & 0xff;
      fprintf_filtered (file, "\t");
      fprintf_filtered (file, "I-%d ", (l & 0x80) != 0);
      fprintf_filtered (file, "UI-%d ", (l & 0x40) != 0);
      fprintf_filtered (file, "H-%d ", (l & 0x20) != 0);
      fprintf_filtered (file, "U-%d ", (l & 0x10) != 0);
      N = (l & 0x8) != 0;
      Z = (l & 0x4) != 0;
      V = (l & 0x2) != 0;
      C = (l & 0x1) != 0;
      fprintf_filtered (file, "N-%d ", N);
      fprintf_filtered (file, "Z-%d ", Z);
      fprintf_filtered (file, "V-%d ", V);
      fprintf_filtered (file, "C-%d ", C);
      if ((C | Z) == 0)
	fprintf_filtered (file, "u> ");
      if ((C | Z) == 1)
	fprintf_filtered (file, "u<= ");
      if ((C == 0))
	fprintf_filtered (file, "u>= ");
      if (C == 1)
	fprintf_filtered (file, "u< ");
      if (Z == 0)
	fprintf_filtered (file, "!= ");
      if (Z == 1)
	fprintf_filtered (file, "== ");
      if ((N ^ V) == 0)
	fprintf_filtered (file, ">= ");
      if ((N ^ V) == 1)
	fprintf_filtered (file, "< ");
      if ((Z | (N ^ V)) == 0)
	fprintf_filtered (file, "> ");
      if ((Z | (N ^ V)) == 1)
	fprintf_filtered (file, "<= ");
    }
  else if (regno == E_PSEUDO_EXR_REGNUM && is_h8300smode (current_gdbarch))
    {
      /* EXR register */
      unsigned char l = rval & 0xff;
      fprintf_filtered (file, "\t");
      fprintf_filtered (file, "T-%d - - - ", (l & 0x80) != 0);
      fprintf_filtered (file, "I2-%d ", (l & 4) != 0);
      fprintf_filtered (file, "I1-%d ", (l & 2) != 0);
      fprintf_filtered (file, "I0-%d", (l & 1) != 0);
    }
  fprintf_filtered (file, "\n");
}

static void
h8300_print_registers_info (struct gdbarch *gdbarch, struct ui_file *file,
			    struct frame_info *frame, int regno, int cpregs)
{
  if (regno < 0)
    {
      for (regno = E_R0_REGNUM; regno <= E_SP_REGNUM; ++regno)
	h8300_print_register (gdbarch, file, frame, regno);
      h8300_print_register (gdbarch, file, frame, E_PSEUDO_CCR_REGNUM);
      h8300_print_register (gdbarch, file, frame, E_PC_REGNUM);
      if (is_h8300smode (current_gdbarch))
        {
	  h8300_print_register (gdbarch, file, frame, E_PSEUDO_EXR_REGNUM);
	  if (is_h8300sxmode (current_gdbarch))
	    {
	      h8300_print_register (gdbarch, file, frame, E_SBR_REGNUM);
	      h8300_print_register (gdbarch, file, frame, E_VBR_REGNUM);
	    }
	  h8300_print_register (gdbarch, file, frame, E_MACH_REGNUM);
	  h8300_print_register (gdbarch, file, frame, E_MACL_REGNUM);
	  h8300_print_register (gdbarch, file, frame, E_CYCLES_REGNUM);
	  h8300_print_register (gdbarch, file, frame, E_TICKS_REGNUM);
	  h8300_print_register (gdbarch, file, frame, E_INSTS_REGNUM);
	}
      else
        {
	  h8300_print_register (gdbarch, file, frame, E_CYCLES_REGNUM);
	  h8300_print_register (gdbarch, file, frame, E_TICK_REGNUM);
	  h8300_print_register (gdbarch, file, frame, E_INST_REGNUM);
	}
    }
  else
    {
      if (regno == E_CCR_REGNUM)
        h8300_print_register (gdbarch, file, frame, E_PSEUDO_CCR_REGNUM);
      else if (regno == E_PSEUDO_EXR_REGNUM && is_h8300smode (current_gdbarch))
	h8300_print_register (gdbarch, file, frame, E_PSEUDO_EXR_REGNUM);
      else
	h8300_print_register (gdbarch, file, frame, regno);
    }
}

static CORE_ADDR
h8300_saved_pc_after_call (struct frame_info *ignore)
{
  return read_memory_unsigned_integer (read_register (E_SP_REGNUM), BINWORD);
}

static struct type *
h8300_register_type (struct gdbarch *gdbarch, int regno)
{
  if (regno < 0 || regno >= NUM_REGS + NUM_PSEUDO_REGS)
    internal_error (__FILE__, __LINE__,
		    "h8300_register_type: illegal register number %d",
		    regno);
  else
    {
      switch (regno)
        {
	  case E_PC_REGNUM:
	    return builtin_type_void_func_ptr;
	  case E_SP_REGNUM:
	  case E_FP_REGNUM:
	    return builtin_type_void_data_ptr;
	  default:
	    if (regno == E_PSEUDO_CCR_REGNUM)
	      return builtin_type_uint8;
	    else if (regno == E_PSEUDO_EXR_REGNUM)
	      return builtin_type_uint8;
	    else if (is_h8300hmode (current_gdbarch))
	      return builtin_type_int32;
	    else
	      return builtin_type_int16;
        }
    }
}

static void
h8300_pseudo_register_read (struct gdbarch *gdbarch, struct regcache *regcache,
			    int regno, void *buf)
{
  if (regno == E_PSEUDO_CCR_REGNUM)
    regcache_raw_read (regcache, E_CCR_REGNUM, buf);
  else if (regno == E_PSEUDO_EXR_REGNUM)
    regcache_raw_read (regcache, E_EXR_REGNUM, buf);
  else
    regcache_raw_read (regcache, regno, buf);
}

static void
h8300_pseudo_register_write (struct gdbarch *gdbarch, struct regcache *regcache,
			     int regno, const void *buf)
{
  if (regno == E_PSEUDO_CCR_REGNUM)
    regcache_raw_write (regcache, E_CCR_REGNUM, buf);
  else if (regno == E_PSEUDO_EXR_REGNUM)
    regcache_raw_write (regcache, E_EXR_REGNUM, buf);
  else
    regcache_raw_write (regcache, regno, buf);
}

static int
h8300_dbg_reg_to_regnum (int regno)
{
  if (regno == E_CCR_REGNUM)
    return E_PSEUDO_CCR_REGNUM;
  return regno;
}

static int
h8300s_dbg_reg_to_regnum (int regno)
{
  if (regno == E_CCR_REGNUM)
    return E_PSEUDO_CCR_REGNUM;
  if (regno == E_EXR_REGNUM)
    return E_PSEUDO_EXR_REGNUM;
  return regno;
}

static CORE_ADDR
h8300_extract_struct_value_address (struct regcache *regcache)
{
  ULONGEST addr;
  regcache_cooked_read_unsigned (regcache, E_RET0_REGNUM, &addr);
  return addr;
}

const static unsigned char *
h8300_breakpoint_from_pc (CORE_ADDR *pcptr, int *lenptr)
{
  /*static unsigned char breakpoint[] = { 0x7A, 0xFF };*/	/* ??? */
  static unsigned char breakpoint[] = { 0x01, 0x80 };		/* Sleep */

  *lenptr = sizeof (breakpoint);
  return breakpoint;
}

static CORE_ADDR
h8300_push_dummy_code (struct gdbarch *gdbarch,
		       CORE_ADDR sp, CORE_ADDR funaddr, int using_gcc,
		       struct value **args, int nargs,
		       struct type *value_type,
		       CORE_ADDR *real_pc, CORE_ADDR *bp_addr)
{
  /* Allocate space sufficient for a breakpoint.  */
  sp = (sp - 2) & ~1;
  /* Store the address of that breakpoint */
  *bp_addr = sp;
  /* h8300 always starts the call at the callee's entry point.  */
  *real_pc = funaddr;
  return sp;
}

static void
h8300_print_float_info (struct gdbarch *gdbarch, struct ui_file *file,
			struct frame_info *frame, const char *args)
{
  fprintf_filtered (file, "\
No floating-point info available for this processor.\n");
}

static struct gdbarch *
h8300_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch_tdep *tdep = NULL;
  struct gdbarch *gdbarch;

  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches != NULL)
    return arches->gdbarch;

#if 0
  tdep = (struct gdbarch_tdep *) xmalloc (sizeof (struct gdbarch_tdep));
#endif

  if (info.bfd_arch_info->arch != bfd_arch_h8300)
    return NULL;

  gdbarch = gdbarch_alloc (&info, 0);

  switch (info.bfd_arch_info->mach)
    {
    case bfd_mach_h8300:
      set_gdbarch_num_regs (gdbarch, 13);
      set_gdbarch_num_pseudo_regs (gdbarch, 1);
      set_gdbarch_ecoff_reg_to_regnum (gdbarch, h8300_dbg_reg_to_regnum);
      set_gdbarch_dwarf_reg_to_regnum (gdbarch, h8300_dbg_reg_to_regnum);
      set_gdbarch_dwarf2_reg_to_regnum (gdbarch, h8300_dbg_reg_to_regnum);
      set_gdbarch_stab_reg_to_regnum (gdbarch, h8300_dbg_reg_to_regnum);
      set_gdbarch_register_name (gdbarch, h8300_register_name);
      set_gdbarch_ptr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
      set_gdbarch_addr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
      set_gdbarch_extract_return_value (gdbarch, h8300_extract_return_value);
      set_gdbarch_store_return_value (gdbarch, h8300_store_return_value);
      set_gdbarch_print_insn (gdbarch, print_insn_h8300);
      break;
    case bfd_mach_h8300h:
    case bfd_mach_h8300hn:
      set_gdbarch_num_regs (gdbarch, 13);
      set_gdbarch_num_pseudo_regs (gdbarch, 1);
      set_gdbarch_ecoff_reg_to_regnum (gdbarch, h8300_dbg_reg_to_regnum);
      set_gdbarch_dwarf_reg_to_regnum (gdbarch, h8300_dbg_reg_to_regnum);
      set_gdbarch_dwarf2_reg_to_regnum (gdbarch, h8300_dbg_reg_to_regnum);
      set_gdbarch_stab_reg_to_regnum (gdbarch, h8300_dbg_reg_to_regnum);
      set_gdbarch_register_name (gdbarch, h8300_register_name);
      if(info.bfd_arch_info->mach != bfd_mach_h8300hn)
        {
          set_gdbarch_ptr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
          set_gdbarch_addr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
        }
      else
        {
          set_gdbarch_ptr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
          set_gdbarch_addr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
        }
      set_gdbarch_extract_return_value (gdbarch, h8300h_extract_return_value);
      set_gdbarch_store_return_value (gdbarch, h8300h_store_return_value);
      set_gdbarch_print_insn (gdbarch, print_insn_h8300h);
      break;
    case bfd_mach_h8300s:
    case bfd_mach_h8300sn:
      set_gdbarch_num_regs (gdbarch, 16);
      set_gdbarch_num_pseudo_regs (gdbarch, 2);
      set_gdbarch_ecoff_reg_to_regnum (gdbarch, h8300s_dbg_reg_to_regnum);
      set_gdbarch_dwarf_reg_to_regnum (gdbarch, h8300s_dbg_reg_to_regnum);
      set_gdbarch_dwarf2_reg_to_regnum (gdbarch, h8300s_dbg_reg_to_regnum);
      set_gdbarch_stab_reg_to_regnum (gdbarch, h8300s_dbg_reg_to_regnum);
      set_gdbarch_register_name (gdbarch, h8300s_register_name);
      if(info.bfd_arch_info->mach != bfd_mach_h8300sn)
        {
          set_gdbarch_ptr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
          set_gdbarch_addr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
        }
      else
        {
          set_gdbarch_ptr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
          set_gdbarch_addr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
        }
      set_gdbarch_extract_return_value (gdbarch, h8300h_extract_return_value);
      set_gdbarch_store_return_value (gdbarch, h8300h_store_return_value);
      set_gdbarch_print_insn (gdbarch, print_insn_h8300s);
      break;
    case bfd_mach_h8300sx:
    case bfd_mach_h8300sxn:
      set_gdbarch_num_regs (gdbarch, 18);
      set_gdbarch_num_pseudo_regs (gdbarch, 2);
      set_gdbarch_ecoff_reg_to_regnum (gdbarch, h8300s_dbg_reg_to_regnum);
      set_gdbarch_dwarf_reg_to_regnum (gdbarch, h8300s_dbg_reg_to_regnum);
      set_gdbarch_dwarf2_reg_to_regnum (gdbarch, h8300s_dbg_reg_to_regnum);
      set_gdbarch_stab_reg_to_regnum (gdbarch, h8300s_dbg_reg_to_regnum);
      set_gdbarch_register_name (gdbarch, h8300sx_register_name);
      if(info.bfd_arch_info->mach != bfd_mach_h8300sxn)
        {
          set_gdbarch_ptr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
          set_gdbarch_addr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
        }
      else
        {
          set_gdbarch_ptr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
          set_gdbarch_addr_bit (gdbarch, 2 * TARGET_CHAR_BIT);
        }
      set_gdbarch_extract_return_value (gdbarch, h8300h_extract_return_value);
      set_gdbarch_store_return_value (gdbarch, h8300h_store_return_value);
      set_gdbarch_print_insn (gdbarch, print_insn_h8300s);
      break;
    }

  set_gdbarch_pseudo_register_read (gdbarch, h8300_pseudo_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, h8300_pseudo_register_write);

  /* NOTE: cagney/2002-12-06: This can be deleted when this arch is
     ready to unwind the PC first (see frame.c:get_prev_frame()).  */
  set_gdbarch_deprecated_init_frame_pc (gdbarch, deprecated_init_frame_pc_default);

  /*
   * Basic register fields and methods.
   */

  set_gdbarch_sp_regnum (gdbarch, E_SP_REGNUM);
  set_gdbarch_deprecated_fp_regnum (gdbarch, E_FP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, E_PC_REGNUM);
  set_gdbarch_register_type (gdbarch, h8300_register_type);
  set_gdbarch_print_registers_info (gdbarch, h8300_print_registers_info);
  set_gdbarch_print_float_info (gdbarch, h8300_print_float_info);

  /*
   * Frame Info
   */
  set_gdbarch_skip_prologue (gdbarch, h8300_skip_prologue);

  set_gdbarch_deprecated_frame_init_saved_regs (gdbarch, 
						h8300_frame_init_saved_regs);
  set_gdbarch_deprecated_init_extra_frame_info (gdbarch, 
						h8300_init_extra_frame_info);
  set_gdbarch_deprecated_frame_chain (gdbarch, h8300_frame_chain);
  set_gdbarch_deprecated_saved_pc_after_call (gdbarch, 
					      h8300_saved_pc_after_call);
  set_gdbarch_deprecated_frame_saved_pc (gdbarch, h8300_frame_saved_pc);
  set_gdbarch_deprecated_pop_frame (gdbarch, h8300_pop_frame);

  /* 
   * Miscelany
   */
  /* Stack grows up. */
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  set_gdbarch_deprecated_extract_struct_value_address (gdbarch, h8300_extract_struct_value_address);
  set_gdbarch_deprecated_use_struct_convention (gdbarch, always_use_struct_convention);
  set_gdbarch_breakpoint_from_pc (gdbarch, h8300_breakpoint_from_pc);
  set_gdbarch_push_dummy_code (gdbarch, h8300_push_dummy_code);
  set_gdbarch_push_dummy_call (gdbarch, h8300_push_dummy_call);

  set_gdbarch_int_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_long_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_long_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_double_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_double_bit (gdbarch, 4 * TARGET_CHAR_BIT);

  set_gdbarch_believe_pcc_promotion (gdbarch, 1);

  /* Char is unsigned.  */
  set_gdbarch_char_signed (gdbarch, 0);

  return gdbarch;
}

extern initialize_file_ftype _initialize_h8300_tdep; /* -Wmissing-prototypes */

void
_initialize_h8300_tdep (void)
{
  register_gdbarch_init (bfd_arch_h8300, h8300_gdbarch_init);
}

static int
is_h8300hmode (struct gdbarch *gdbarch)
{
  return gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sx
	 || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sxn
	 || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300s
	 || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sn
	 || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300h
	 || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300hn;
}

static int
is_h8300smode (struct gdbarch *gdbarch)
{
  return gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sx
	 || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sxn
	 || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300s
	 || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sn;
}

static int
is_h8300sxmode (struct gdbarch *gdbarch)
{
  return gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sx
	 || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sxn;
}

static int
is_h8300_normal_mode (struct gdbarch *gdbarch)
{
  return gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sxn
	 || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300sn
	 || gdbarch_bfd_arch_info (gdbarch)->mach == bfd_mach_h8300hn;
}


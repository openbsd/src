/* Target-dependent code for the Acorn Risc Machine, for GDB, the GNU Debugger.
   Copyright (C) 1988, 1989, 1991, 1992, 1993, 1995, 1996
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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "gdbcmd.h"
#include "gdbcore.h"

/* Set to true if the 32-bit mode is in use. */

int arm_apcs_32 = 1;

CORE_ADDR
arm_addr_bits_remove (val)
CORE_ADDR val;
{
  return (val & (arm_apcs_32 ? 0xfffffffc : 0x03fffffc));
}

CORE_ADDR
arm_saved_pc_after_call (frame)
     struct frame_info *frame;
{
  return ADDR_BITS_REMOVE (read_register (LR_REGNUM));
}

/* APCS (ARM procedure call standard) defines the following prologue:

   mov		ip, sp
  [stmfd	sp!, {a1,a2,a3,a4}]
   stmfd	sp!, {...,fp,ip,lr,pc}
  [stfe		f7, [sp, #-12]!]
  [stfe		f6, [sp, #-12]!]
  [stfe		f5, [sp, #-12]!]
  [stfe		f4, [sp, #-12]!]
   sub		fp, ip, #nn	// nn == 20 or 4 depending on second ins
*/

CORE_ADDR
arm_skip_prologue (pc)
CORE_ADDR pc;
{
  unsigned long inst;
  CORE_ADDR skip_pc = pc;

  inst = read_memory_integer (skip_pc, 4);
  if (inst != 0xe1a0c00d)  /* mov ip, sp */
    return pc;

  skip_pc += 4;
  inst = read_memory_integer (skip_pc, 4);
  if ((inst & 0xfffffff0) == 0xe92d0000)  /* stmfd sp!,{a1,a2,a3,a4}  */
    {
      skip_pc += 4;
      inst = read_memory_integer (skip_pc, 4);
    }

  if ((inst & 0xfffff800) != 0xe92dd800)  /* stmfd sp!,{...,fp,ip,lr,pc} */
    return pc;

  skip_pc += 4;
  inst = read_memory_integer (skip_pc, 4);

  /* Any insns after this point may float into the code, if it makes
     for better instruction scheduling, so we skip them only if
     we find them, but still consdier the function to be frame-ful  */

  /* We may have either one sfmfd instruction here, or several stfe insns,
     depending on the version of floating point code we support.  */
  if ((inst & 0xffbf0fff) == 0xec2d0200)  /* sfmfd fn, <cnt>, [sp]! */
    {
      skip_pc += 4;
      inst = read_memory_integer (skip_pc, 4);
    }
  else
    {
      while ((inst & 0xffff8fff) == 0xed6d0103) /* stfe fn, [sp, #-12]! */
        {
          skip_pc += 4;
          inst = read_memory_integer (skip_pc, 4);
        }
    }

  if ((inst & 0xfffff000) == 0xe24cb000) /* sub fp, ip, #nn */
    skip_pc += 4;

  return skip_pc;
}

void
arm_frame_find_saved_regs (frame_info, saved_regs_addr)
     struct frame_info *frame_info;
     struct frame_saved_regs *saved_regs_addr;
{
  register int regnum;
  register int frame;
  register int next_addr;
  register int return_data_save;
  register int saved_register_mask;

  memset (saved_regs_addr, '\0', sizeof (*saved_regs_addr));
  frame = frame_info->frame;
  return_data_save = read_memory_integer (frame, 4) & 0x03fffffc - 12;
  saved_register_mask = read_memory_integer (return_data_save, 4);
  next_addr = frame - 12;
  for (regnum = 4; regnum < 10; regnum++)
    if (saved_register_mask & (1 << regnum))
      {
	next_addr -= 4;
	saved_regs_addr->regs[regnum] = next_addr;
      }
  if (read_memory_integer (return_data_save + 4, 4) == 0xed6d7103)
    {
      next_addr -= 12;
      saved_regs_addr->regs[F0_REGNUM + 7] = next_addr;
    }
  if (read_memory_integer (return_data_save + 8, 4) == 0xed6d6103)
    {
      next_addr -= 12;
      saved_regs_addr->regs[F0_REGNUM + 6] = next_addr;
    }
  if (read_memory_integer (return_data_save + 12, 4) == 0xed6d5103)
    {
      next_addr -= 12;
      saved_regs_addr->regs[F0_REGNUM + 5] = next_addr;
    }
  if (read_memory_integer(return_data_save + 16, 4) == 0xed6d4103)
    {
      next_addr -= 12;
      saved_regs_addr->regs[F0_REGNUM + 4] = next_addr;
    }
  saved_regs_addr->regs[SP_REGNUM] = next_addr;
  saved_regs_addr->regs[PC_REGNUM] = frame - 4;
  saved_regs_addr->regs[PS_REGNUM] = frame - 4;
  saved_regs_addr->regs[FP_REGNUM] = frame - 12;
}

void
arm_push_dummy_frame ()
{
  register CORE_ADDR sp = read_register (SP_REGNUM);
  register int regnum;

  /* opcode for ldmdb fp,{v1-v6,fp,ip,lr,pc}^ */
  sp = push_word (sp, 0xe92dbf0); /* dummy return_data_save ins */
  /* push a pointer to the dummy instruction minus 12 */
  sp = push_word (sp, read_register (SP_REGNUM) - 16);
  sp = push_word (sp, read_register (PS_REGNUM));
  sp = push_word (sp, read_register (SP_REGNUM));
  sp = push_word (sp, read_register (FP_REGNUM));
  for (regnum = 9; regnum >= 4; regnum --)
    sp = push_word (sp, read_register (regnum));
  write_register (FP_REGNUM, read_register (SP_REGNUM) - 8);
  write_register (SP_REGNUM, sp);
}

void
arm_pop_frame ()
{
  register CORE_ADDR fp = read_register (FP_REGNUM);
  register unsigned long return_data_save =
    read_memory_integer (ADDR_BITS_REMOVE (read_memory_integer (fp, 4)) - 12,
			 4);
  register int regnum;

  write_register (PS_REGNUM, read_memory_integer (fp - 4, 4));
  write_register (PC_REGNUM, ADDR_BITS_REMOVE (read_register (PS_REGNUM)));
  write_register (SP_REGNUM, read_memory_integer (fp - 8, 4));
  write_register (FP_REGNUM, read_memory_integer (fp - 12, 4));
  fp -= 12;
  for (regnum = 9; regnum >= 4; regnum--)
    {
      if (return_data_save & (1 << regnum))
	{
	  fp -= 4;
	  write_register (regnum, read_memory_integer (fp, 4));
	}
    }
  flush_cached_frames ();
}

static void
print_fpu_flags (flags)
int flags;
{
    if (flags & (1 << 0)) fputs ("IVO ", stdout);
    if (flags & (1 << 1)) fputs ("DVZ ", stdout);
    if (flags & (1 << 2)) fputs ("OFL ", stdout);
    if (flags & (1 << 3)) fputs ("UFL ", stdout);
    if (flags & (1 << 4)) fputs ("INX ", stdout);
    putchar ('\n');
}

void
arm_float_info ()
{
    register unsigned long status = read_register (FPS_REGNUM);
    int type;

    type = (status >> 24) & 127;
    printf ("%s FPU type %d\n",
	    (status & (1<<31)) ? "Hardware" : "Software",
	    type);
    fputs ("mask: ", stdout);
    print_fpu_flags (status >> 16);
    fputs ("flags: ", stdout);
    print_fpu_flags (status);
}

static void
arm_othernames ()
{
  static int toggle;
  static char *original[] = ORIGINAL_REGISTER_NAMES;
  static char *extra_crispy[] = ADDITIONAL_REGISTER_NAMES;

  memcpy (reg_names, toggle ? extra_crispy : original, sizeof(original));
  toggle = !toggle;
}

/* FIXME:  Fill in with the 'right thing', see asm 
   template in arm-convert.s */

void 
convert_from_extended (ptr, dbl)
void *ptr;
double *dbl;
{
  *dbl = *(double*)ptr;
}

void 
convert_to_extended (dbl, ptr)
void *ptr;
double *dbl;
{
  *(double*)ptr = *dbl;
}

int
arm_nullified_insn (inst)
     unsigned long inst;
{
  unsigned long cond = inst & 0xf0000000;
  unsigned long status_reg;

  if (cond == INST_AL || cond == INST_NV)
    return 0;

  status_reg = read_register (PS_REGNUM);

  switch (cond)
    {
    case INST_EQ:
      return ((status_reg & FLAG_Z) == 0);
    case INST_NE:
      return ((status_reg & FLAG_Z) != 0);
    case INST_CS:
      return ((status_reg & FLAG_C) == 0);
    case INST_CC:
      return ((status_reg & FLAG_C) != 0);
    case INST_MI:
      return ((status_reg & FLAG_N) == 0);
    case INST_PL:
      return ((status_reg & FLAG_N) != 0);
    case INST_VS:
      return ((status_reg & FLAG_V) == 0);
    case INST_VC:
      return ((status_reg & FLAG_V) != 0);
    case INST_HI:
      return ((status_reg & (FLAG_C | FLAG_Z)) != FLAG_C);
    case INST_LS:
      return (((status_reg & (FLAG_C | FLAG_Z)) ^ FLAG_C) == 0);
    case INST_GE:
      return (((status_reg & FLAG_N) == 0) != ((status_reg & FLAG_V) == 0));
    case INST_LT:
      return (((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0));
    case INST_GT:
      return (((status_reg & FLAG_Z) != 0) ||
	      (((status_reg & FLAG_N) == 0) != ((status_reg & FLAG_V) == 0)));
    case INST_LE:
      return (((status_reg & FLAG_Z) == 0) &&
	      (((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0)));
    }
  return 0;
}

#define submask(x) ((1L << ((x) + 1)) - 1)
#define bit(obj,st) (((obj) & (1L << (st))) >> st)
#define bits(obj,st,fn) \
  (((obj) & submask (fn) & ~ submask ((st) - 1)) >> (st))
#define sbits(obj,st,fn) \
  ((long) (bits(obj,st,fn) | ((long) bit(obj,fn) * ~ submask (fn - st))))
#define BranchDest(addr,instr) \
  ((CORE_ADDR) (((long) (addr)) + 8 + (sbits (instr, 0, 23) << 2)))
#define ARM_PC_32 1

static unsigned long
shifted_reg_val (inst, carry, pc_val)
     unsigned long inst;
     int carry;
     unsigned long pc_val;
{
  unsigned long res, shift;
  int rm = bits (inst, 0, 3);
  unsigned long shifttype = bits (inst, 5, 6);
 
  if (bit(inst, 4))
    {
      int rs = bits (inst, 8, 11);
      shift = (rs == 15 ? pc_val + 8 : read_register (rs)) & 0xFF;
    }
  else
    shift = bits (inst, 7, 11);
 
  res = (rm == 15 
	 ? ((pc_val | (ARM_PC_32 ? 0 : read_register (PS_REGNUM)))
	    + (bit (inst, 4) ? 12 : 8)) 
	 : read_register (rm));

  switch (shifttype)
    {
    case 0: /* LSL */
      res = shift >= 32 ? 0 : res << shift;
      break;
      
    case 1: /* LSR */
      res = shift >= 32 ? 0 : res >> shift;
      break;

    case 2: /* ASR */
      if (shift >= 32) shift = 31;
      res = ((res & 0x80000000L)
	     ? ~((~res) >> shift) : res >> shift);
      break;

    case 3: /* ROR/RRX */
      shift &= 31;
      if (shift == 0)
	res = (res >> 1) | (carry ? 0x80000000L : 0);
      else
	res = (res >> shift) | (res << (32-shift));
      break;
    }

  return res & 0xffffffff;
}


CORE_ADDR
arm_get_next_pc (pc)
     CORE_ADDR pc;
{
  unsigned long pc_val = (unsigned long) pc;
  unsigned long this_instr = read_memory_integer (pc, 4);
  unsigned long status = read_register (PS_REGNUM);
  CORE_ADDR nextpc = (CORE_ADDR) (pc_val + 4);  /* Default case */

  if (! arm_nullified_insn (this_instr))
    {
      switch (bits(this_instr, 24, 27))
	{
	case 0x0: case 0x1: /* data processing */
	case 0x2: case 0x3:
	  {
	    unsigned long operand1, operand2, result = 0;
	    unsigned long rn;
	    int c;
 
	    if (bits(this_instr, 12, 15) != 15)
	      break;

	    if (bits (this_instr, 22, 25) == 0
		&& bits (this_instr, 4, 7) == 9)  /* multiply */
	      error ("Illegal update to pc in instruction");

	    /* Multiply into PC */
	    c = (status & FLAG_C) ? 1 : 0;
	    rn = bits (this_instr, 16, 19);
	    operand1 = (rn == 15) ? pc_val + 8 : read_register (rn);
 
	    if (bit (this_instr, 25))
	      {
		unsigned long immval = bits (this_instr, 0, 7);
		unsigned long rotate = 2 * bits (this_instr, 8, 11);
		operand2 = ((immval >> rotate) | (immval << (32-rotate))
			    & 0xffffffff);
	      }
	    else  /* operand 2 is a shifted register */
	      operand2 = shifted_reg_val (this_instr, c, pc_val);
 
	    switch (bits (this_instr, 21, 24))
	      {
	      case 0x0: /*and*/
		result = operand1 & operand2;
		break;

	      case 0x1: /*eor*/
		result = operand1 ^ operand2;
		break;

	      case 0x2: /*sub*/
		result = operand1 - operand2;
		break;

	      case 0x3: /*rsb*/
		result = operand2 - operand1;
		break;

	      case 0x4:  /*add*/
		result = operand1 + operand2;
		break;

	      case 0x5: /*adc*/
		result = operand1 + operand2 + c;
		break;

	      case 0x6: /*sbc*/
		result = operand1 - operand2 + c;
		break;

	      case 0x7: /*rsc*/
		result = operand2 - operand1 + c;
		break;

	      case 0x8: case 0x9: case 0xa: case 0xb: /* tst, teq, cmp, cmn */
		result = (unsigned long) nextpc;
		break;

	      case 0xc: /*orr*/
		result = operand1 | operand2;
		break;

	      case 0xd: /*mov*/
		/* Always step into a function.  */
		result = operand2;
                break;

	      case 0xe: /*bic*/
		result = operand1 & ~operand2;
		break;

	      case 0xf: /*mvn*/
		result = ~operand2;
		break;
	      }
	    nextpc = (CORE_ADDR) ADDR_BITS_REMOVE (result);

	    if (nextpc == pc)
	      error ("Infinite loop detected");
	    break;
	  }
 
	case 0x4: case 0x5: /* data transfer */
	case 0x6: case 0x7:
	  if (bit (this_instr, 20))
	    {
	      /* load */
	      if (bits (this_instr, 12, 15) == 15)
		{
		  /* rd == pc */
		  unsigned long  rn;
		  unsigned long base;
 
		  if (bit (this_instr, 22))
		    error ("Illegal update to pc in instruction");

		  /* byte write to PC */
		  rn = bits (this_instr, 16, 19);
		  base = (rn == 15) ? pc_val + 8 : read_register (rn);
		  if (bit (this_instr, 24))
		    {
		      /* pre-indexed */
		      int c = (status & FLAG_C) ? 1 : 0;
		      unsigned long offset =
			(bit (this_instr, 25)
			 ? shifted_reg_val (this_instr, c, pc_val)
			 : bits (this_instr, 0, 11));

		      if (bit (this_instr, 23))
			base += offset;
		      else
			base -= offset;
		    }
		  nextpc = (CORE_ADDR) read_memory_integer ((CORE_ADDR) base, 
							    4);
 
		  nextpc = ADDR_BITS_REMOVE (nextpc);

		  if (nextpc == pc)
		    error ("Infinite loop detected");
		}
	    }
	  break;
 
	case 0x8: case 0x9: /* block transfer */
	  if (bit (this_instr, 20))
	    {
	      /* LDM */
	      if (bit (this_instr, 15))
		{
		  /* loading pc */
		  int offset = 0;

		  if (bit (this_instr, 23))
		    {
		      /* up */
		      unsigned long reglist = bits (this_instr, 0, 14);
		      unsigned long regbit;

		      for (; reglist != 0; reglist &= ~regbit)
			{
			  regbit = reglist & (-reglist);
			  offset += 4;
			}

		      if (bit (this_instr, 24)) /* pre */
			offset += 4;
		    }
		  else if (bit (this_instr, 24))
		    offset = -4;
 
		  {
		    unsigned long rn_val = 
		      read_register (bits (this_instr, 16, 19));
		    nextpc =
		      (CORE_ADDR) read_memory_integer ((CORE_ADDR) (rn_val
								    + offset),
						       4);
		  }
		  nextpc = ADDR_BITS_REMOVE (nextpc);
		  if (nextpc == pc)
		    error ("Infinite loop detected");
		}
	    }
	  break;
 
	case 0xb:           /* branch & link */
	case 0xa:           /* branch */
	  {
	    nextpc = BranchDest (pc, this_instr);

	    nextpc = ADDR_BITS_REMOVE (nextpc);
	    if (nextpc == pc)
	      error ("Infinite loop detected");
	    break;
	  }
 
	case 0xc: case 0xd:
	case 0xe:           /* coproc ops */
	case 0xf:           /* SWI */
	  break;

	default:
	  fprintf (stderr, "Bad bit-field extraction\n");
	  return (pc);
	}
    }

  return nextpc;
}

void
_initialize_arm_tdep ()
{
  tm_print_insn = print_insn_little_arm;

  add_com ("othernames", class_obscure, arm_othernames,
	   "Switch to the other set of register names.");

  /* ??? Maybe this should be a boolean.  */
  add_show_from_set (add_set_cmd ("apcs32", no_class,
				  var_zinteger, (char *)&arm_apcs_32,
				  "Set usage of ARM 32-bit mode.\n", &setlist),
		     &showlist);

}

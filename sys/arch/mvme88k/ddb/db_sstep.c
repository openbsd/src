/*	$OpenBSD: db_sstep.c,v 1.4 1999/02/09 06:36:25 smurph Exp $	*/
/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <machine/db_machdep.h>
#include <ddb/db_access.h>	/* db_get_value() */

/*
 * Support routines for software single step.
 *
 * Author: Daniel Stodolsky (danner@cs.cmu.edu)
 *
 */

/* is the instruction a branch or jump instruction (br, bb0, bb1, bcnd, jmp)
   but not a function call (bsr or jsr) */

boolean_t
inst_branch(unsigned ins)
{
  /* check high five bits */
 
  switch (ins >> (32-5))
    {
    case 0x18: /* br */
    case 0x1a: /* bb0 */
    case 0x1b: /* bb1 */
    case 0x1d: /* bcnd */
      return TRUE;
      break;
    case 0x1e: /* could be jmp */
      if ((ins & 0xfffffbe0U) == 0xf400c000U)
	return TRUE;
    }

  return FALSE;
}

/* inst_load(ins) - returns the number of words the instruction loads. byte,
   half and word count as 1; double word as 2 */

unsigned
inst_load(unsigned ins)
{
  /* look at the top six bits, for starters */

  switch (ins >> (32-6))
    {
    case 0x0: /* xmem byte imm */
    case 0x1: /* xmem word imm */

    case 0x2: /* unsigned half-word load imm */
    case 0x3: /* unsigned byte load imm */
    case 0x5: /* signed word load imm */
    case 0x6: /* signed half-word load imm */
    case 0x7: /* signed byte load imm */
      return 1;

    case 0x4: /* signed double word load imm */
      return 2;

    case 0x3d: /* load/store/xmem scaled/unscaled instruction */
      if ((ins & 0xf400c0e0U) == 0xf4000000U) /* is load/xmem */
	switch ((ins & 0x0000fce0)>>5)  /* look at bits 15-5, but mask bits 8-9 */
	  {
	  case 0x0: /* xmem byte */
	  case 0x1: /* xmem word */
	  case 0x2: /* unsigned half word */
	  case 0x3: /* unsigned byte load */
	  case 0x5: /* signed word load */
	  case 0x6: /* signed half-word load */
	  case 0x7: /* signed byte load */
	    return 1;

	  case 0x4: /* signed double word load */
	    return 2;
	  } /* end switch load/xmem  */
      break;
    } /* end switch 32-6 */

  return 0;
}

/* inst_store - like inst_load, except for store instructions. */

unsigned
inst_store(unsigned ins)
{
  /* decode top 6 bits again */
  switch (ins >> (32-6))
    {
    case 0x0: /* xmem byte imm */
    case 0x1: /* xmem word imm */
    case 0x9: /* store word imm */
    case 0xa: /* store half-word imm */
    case 0xb: /* store byte imm */
      return 1;

    case 0x8: /* store double word */
      return 2;
    case 0x3d: /* load/store/xmem scaled/unscaled instruction */
      /* check bits 15,14,12,7,6,5 are all 0 */
      if ((ins & 0x0000d0e0U) == 0)
	switch ((ins & 0x00003c00U) >> 10 ) /* decode bits 10-13 */
	  {
	  case 0x0: /* xmem byte imm */
	  case 0x1: /* xmem word imm */
	  case 0x9: /* store word */
	  case 0xa: /* store half-word */
	  case 0xb: /* store byte */
	    return 1;

	  case 0x8: /* store double word */
	    return 2;
	  } /* end switch store/xmem */
      break;
    } /* end switch 32-6 */

  return 0;
}

/* inst_delayed - this instruction is followed by a delay slot. Could be
   br.n, bsr.n bb0.n, bb1.n, bcnd.n or jmp.n or jsr.n */

boolean_t
inst_delayed(unsigned ins)
{
  /* check the br, bsr, bb0, bb1, bcnd cases */
  switch ((ins & 0xfc000000U)>>(32-6))
    {
    case 0x31: /* br */
    case 0x33: /* bsr */
    case 0x35: /* bb0 */
    case 0x37: /* bb1 */
    case 0x3b: /* bcnd */
      return TRUE;
    }

 /* check the jmp, jsr cases */
 /* mask out bits 0-4, bit 11 */
  return ((ins & 0xfffff7e0U) == 0xf400c400U) ? TRUE : FALSE;
}

 
/*
 * next_instr_address(pc,delay_slot,task) has the following semantics.
 * Let inst be the instruction at pc.
 * If delay_slot = 1, next_instr_address should return
 * the address of the instruction in the delay slot; if this instruction
 * does not have a delay slot, it should return pc.
 * If delay_slot = 0, next_instr_address should return the
 * address of next sequential instruction, or pc if the instruction is
 * followed by a delay slot.
 *
 * 91-11-28 jfriedl: I think the above is wrong. I think it should be:
 *	if delay_slot true, return address of the delay slot if there is one,
 *			    return pc otherwise.
 *	if delay_slot false, return (pc + 4) regardless.
 *
 */
db_addr_t
next_instr_address(db_addr_t pc, unsigned delay_slot)
{
    if (delay_slot == 0)
	return pc + 4;
    else
    {
	if (inst_delayed(db_get_value(pc,sizeof(int),FALSE)))
	   return pc + 4;
	else
	   return pc;
    }
}


/*
 * branch_taken(instruction, program counter, func, func_data)
 *
 * instruction will be a control flow instruction location at address pc.
 * Branch taken is supposed to return the address to which the instruction
 * would jump if the branch is taken. Func can be used to get the current
 * register values when invoked with a register number and func_data as
 * arguments.
 *
 * If the instruction is not a control flow instruction, panic.
 */
unsigned
branch_taken(
    unsigned inst,
    unsigned pc,
    db_expr_t (*func)(unsigned int, db_regs_t *),
    db_regs_t *func_data)  /* 'opaque' */
{

  /* check if br/bsr */
  if ((inst & 0xf0000000U) == 0xc0000000U)
    {
      /* signed 26 bit pc relative displacement, shift left two bits */
      inst = (inst & 0x03ffffffU)<<2;
      /* check if sign extension is needed */
      if (inst & 0x08000000U)
	inst |= 0xf0000000U;
      return pc + inst;
    }

  /* check if bb0/bb1/bcnd case */
  switch ((inst & 0xf8000000U))
    {
    case 0xd0000000U: /* bb0 */
    case 0xd8000000U: /* bb1 */
    case 0xe8000000U: /* bcnd */
      /* signed 16 bit pc relative displacement, shift left two bits */
      inst = (inst & 0x0000ffffU)<<2;
      /* check if sign extension is needed */
      if (inst & 0x00020000U)
	inst |= 0xfffc0000U;
      return pc + inst;
    }

  /* check jmp/jsr case */
  /* check bits 5-31, skipping 10 & 11 */
  if ((inst & 0xfffff3e0U) == 0xf400c000U)
    return (*func)(inst & 0x1f, func_data);  /* the register value */

  panic("branch_taken");
  return 0; /* keeps compiler happy */
}

/*
 * getreg_val - handed a register number and an exception frame.
 *              Returns the value of the register in the specified
 *              frame. Only makes sense for general registers.
 */
db_expr_t
getreg_val(unsigned regno, db_regs_t *frame)
{
    if (regno == 0)
	return 0;
    else if (regno < 31)
	return frame->r[regno];
    else {
	panic("bad register number to getreg_val.");
	return 0;/*to make compiler happy */
    }
}

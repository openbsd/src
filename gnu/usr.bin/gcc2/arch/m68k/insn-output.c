/* Generated automatically by the program `genoutput'
from the machine description file `md'.  */

#include "config.h"
#include "rtl.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "real.h"
#include "insn-config.h"

#include "conditions.h"
#include "insn-flags.h"
#include "insn-attr.h"

#include "insn-codes.h"

#include "recog.h"

#include <stdio.h>
#include "output.h"

static char *
output_0 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[1]))
    return "fmove%.d %f1,%0";
  if (FPA_REG_P (operands[1]))
    return "fpmove%.d %1, %x0";
  return output_move_double (operands);
}
}

static char *
output_1 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  return output_move_double (operands);
}
}

static char *
output_2 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifdef ISI_OV
  /* ISI's assembler fails to handle tstl a0.  */
  if (! ADDRESS_REG_P (operands[0]))
#else
  if (TARGET_68020 || ! ADDRESS_REG_P (operands[0]))
#endif
    return "tst%.l %0";
  /* If you think that the 68020 does not support tstl a0,
     reread page B-167 of the 68020 manual more carefully.  */
  /* On an address reg, cmpw may replace cmpl.  */
#ifdef SGS_CMP_ORDER
  return "cmp%.w %0,%#0";
#else
  return "cmp%.w %#0,%0";
#endif
}
}

static char *
output_7 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  cc_status.flags = CC_IN_68881;
  if (FP_REG_P (operands[0]))
    return "ftst%.x %0";
  return "ftst%.s %0";
}
}

static char *
output_10 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  cc_status.flags = CC_IN_68881;
  if (FP_REG_P (operands[0]))
    return "ftst%.x %0";
  return "ftst%.d %0";
}
}

static char *
output_11 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[0]) == MEM && GET_CODE (operands[1]) == MEM)
    return "cmpm%.l %1,%0";
  if (REG_P (operands[1])
      || (!REG_P (operands[0]) && GET_CODE (operands[0]) != MEM))
    { cc_status.flags |= CC_REVERSED;
#ifdef SGS_CMP_ORDER
      return "cmp%.l %d1,%d0";
#else
      return "cmp%.l %d0,%d1";
#endif
    }
#ifdef SGS_CMP_ORDER
  return "cmp%.l %d0,%d1";
#else
  return "cmp%.l %d1,%d0";
#endif
}
}

static char *
output_12 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[0]) == MEM && GET_CODE (operands[1]) == MEM)
    return "cmpm%.w %1,%0";
  if ((REG_P (operands[1]) && !ADDRESS_REG_P (operands[1]))
      || (!REG_P (operands[0]) && GET_CODE (operands[0]) != MEM))
    { cc_status.flags |= CC_REVERSED;
#ifdef SGS_CMP_ORDER
      return "cmp%.w %d1,%d0";
#else
      return "cmp%.w %d0,%d1";
#endif
    }
#ifdef SGS_CMP_ORDER
  return "cmp%.w %d0,%d1";
#else
  return "cmp%.w %d1,%d0";
#endif
}
}

static char *
output_13 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[0]) == MEM && GET_CODE (operands[1]) == MEM)
    return "cmpm%.b %1,%0";
  if (REG_P (operands[1])
      || (!REG_P (operands[0]) && GET_CODE (operands[0]) != MEM))
    { cc_status.flags |= CC_REVERSED;
#ifdef SGS_CMP_ORDER
      return "cmp%.b %d1,%d0";
#else
      return "cmp%.b %d0,%d1";
#endif
    }
#ifdef SGS_CMP_ORDER
  return "cmp%.b %d0,%d1";
#else
  return "cmp%.b %d1,%d0";
#endif
}
}

static char *
output_16 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  cc_status.flags = CC_IN_68881;
#ifdef SGS_CMP_ORDER
  if (REG_P (operands[0]))
    {
      if (REG_P (operands[1]))
	return "fcmp%.x %0,%1";
      else
        return "fcmp%.d %0,%f1";
    }
  cc_status.flags |= CC_REVERSED;
  return "fcmp%.d %1,%f0";
#else
  if (REG_P (operands[0]))
    {
      if (REG_P (operands[1]))
	return "fcmp%.x %1,%0";
      else
        return "fcmp%.d %f1,%0";
    }
  cc_status.flags |= CC_REVERSED;
  return "fcmp%.d %f0,%1";
#endif
}
}

static char *
output_19 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  cc_status.flags = CC_IN_68881;
#ifdef SGS_CMP_ORDER
  if (FP_REG_P (operands[0]))
    {
      if (FP_REG_P (operands[1]))
	return "fcmp%.x %0,%1";
      else
        return "fcmp%.s %0,%f1";
    }
  cc_status.flags |= CC_REVERSED;
  return "fcmp%.s %1,%f0";
#else
  if (FP_REG_P (operands[0]))
    {
      if (FP_REG_P (operands[1]))
	return "fcmp%.x %1,%0";
      else
        return "fcmp%.s %f1,%0";
    }
  cc_status.flags |= CC_REVERSED;
  return "fcmp%.s %f0,%1";
#endif
}
}

static char *
output_20 (operands, insn)
     rtx *operands;
     rtx insn;
{
 { return output_btst (operands, operands[1], operands[0], insn, 7); }
}

static char *
output_21 (operands, insn)
     rtx *operands;
     rtx insn;
{
 { return output_btst (operands, operands[1], operands[0], insn, 31); }
}

static char *
output_22 (operands, insn)
     rtx *operands;
     rtx insn;
{
 { return output_btst (operands, operands[1], operands[0], insn, 7); }
}

static char *
output_23 (operands, insn)
     rtx *operands;
     rtx insn;
{
 { return output_btst (operands, operands[1], operands[0], insn, 31); }
}

static char *
output_24 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  operands[1] = gen_rtx (CONST_INT, VOIDmode, 7 - INTVAL (operands[1]));
  return output_btst (operands, operands[1], operands[0], insn, 7);
}
}

static char *
output_25 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[0]) == MEM)
    {
      operands[0] = adj_offsettable_operand (operands[0],
					     INTVAL (operands[1]) / 8);
      operands[1] = gen_rtx (CONST_INT, VOIDmode, 
			     7 - INTVAL (operands[1]) % 8);
      return output_btst (operands, operands[1], operands[0], insn, 7);
    }
  operands[1] = gen_rtx (CONST_INT, VOIDmode,
			 31 - INTVAL (operands[1]));
  return output_btst (operands, operands[1], operands[0], insn, 31);
}
}

static char *
output_26 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (operands[1] == const0_rtx)
    return "clr%.l %0";
  return "pea %a1";
}
}

static char *
output_27 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (ADDRESS_REG_P (operands[0]))
    return "sub%.l %0,%0";
  /* moveq is faster on the 68000.  */
  if (DATA_REG_P (operands[0]) && !TARGET_68020)
#if defined(MOTOROLA) && !defined(CRDS)
    return "moveq%.l %#0,%0";
#else
    return "moveq %#0,%0";
#endif
  return "clr%.l %0";
}
}

static char *
output_29 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (which_alternative == 3)
    return "fpmove%.l %x1,fpa0\n\tfpmove%.l fpa0,%x0";	
  if (FPA_REG_P (operands[1]) || FPA_REG_P (operands[0]))
    return "fpmove%.l %x1,%x0";
  if (GET_CODE (operands[1]) == CONST_INT)
    {
      if (operands[1] == const0_rtx
	  && (DATA_REG_P (operands[0])
	      || GET_CODE (operands[0]) == MEM)
	  /* clr insns on 68000 read before writing.
	     This isn't so on the 68010, but we have no alternative for it.  */
	  && (TARGET_68020
	      || !(GET_CODE (operands[0]) == MEM
		   && MEM_VOLATILE_P (operands[0]))))
	return "clr%.l %0";
      else if (DATA_REG_P (operands[0])
	       && INTVAL (operands[1]) < 128
	       && INTVAL (operands[1]) >= -128)
        {
#if defined(MOTOROLA) && !defined(CRDS)
          return "moveq%.l %1,%0";
#else
	  return "moveq %1,%0";
#endif
	}
#ifndef NO_ADDSUB_Q
      else if (DATA_REG_P (operands[0])
	       /* Do this with a moveq #N-8, dreg; addq #8,dreg */
	       && INTVAL (operands[1]) < 136
	       && INTVAL (operands[1]) >= 128)
        {
	  operands[1] = gen_rtx (CONST_INT, VOIDmode, INTVAL (operands[1]) - 8);
#if defined(MOTOROLA) && !defined(CRDS)
          return "moveq%.l %1,%0\n\taddq%.w %#8,%0";
#else
	  return "moveq %1,%0\n\taddq%.w %#8,%0";
#endif
	}
      else if (DATA_REG_P (operands[0])
	       /* Do this with a moveq #N+8, dreg; subq #8,dreg */
	       && INTVAL (operands[1]) < -128
	       && INTVAL (operands[1]) >= -136)
        {
	  operands[1] = gen_rtx (CONST_INT, VOIDmode, INTVAL (operands[1]) + 8);
#if defined(MOTOROLA) && !defined(CRDS)
          return "moveq%.l %1,%0;subq%.w %#8,%0";
#else
	  return "moveq %1,%0;subq%.w %#8,%0";
#endif
	}
#endif
      else if (DATA_REG_P (operands[0])
	       /* If N is in the right range and is even, then use
	          moveq #N/2, dreg; addl dreg,dreg */
	       && INTVAL (operands[1]) > 127
	       && INTVAL (operands[1]) <= 254
	       && INTVAL (operands[1]) % 2 == 0)
        {
	  operands[1] = gen_rtx (CONST_INT, VOIDmode, INTVAL (operands[1]) / 2);
#if defined(MOTOROLA) && !defined(CRDS)
          return "moveq%.l %1,%0\n\tadd%.w %0,%0";
#else
	  return "moveq %1,%0\n\tadd%.w %0,%0";
#endif
	}
      else if (ADDRESS_REG_P (operands[0])
	       && INTVAL (operands[1]) < 0x8000
	       && INTVAL (operands[1]) >= -0x8000)
	return "move%.w %1,%0";
      else if (push_operand (operands[0], SImode)
	       && INTVAL (operands[1]) < 0x8000
	       && INTVAL (operands[1]) >= -0x8000)
        return "pea %a1";
    }
  else if ((GET_CODE (operands[1]) == SYMBOL_REF
	    || GET_CODE (operands[1]) == CONST)
	   && push_operand (operands[0], SImode))
    return "pea %a1";
  else if ((GET_CODE (operands[1]) == SYMBOL_REF
	    || GET_CODE (operands[1]) == CONST)
	   && ADDRESS_REG_P (operands[0]))
    return "lea %a1,%0";
  return "move%.l %1,%0";
}
}

static char *
output_30 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT)
    {
      if (operands[1] == const0_rtx
	  && (DATA_REG_P (operands[0])
	      || GET_CODE (operands[0]) == MEM)
	  /* clr insns on 68000 read before writing.
	     This isn't so on the 68010, but we have no alternative for it.  */
	  && (TARGET_68020
	      || !(GET_CODE (operands[0]) == MEM
		   && MEM_VOLATILE_P (operands[0]))))
	return "clr%.w %0";
      else if (DATA_REG_P (operands[0])
	       && INTVAL (operands[1]) < 128
	       && INTVAL (operands[1]) >= -128)
        {
#if defined(MOTOROLA) && !defined(CRDS)
          return "moveq%.l %1,%0";
#else
	  return "moveq %1,%0";
#endif
	}
      else if (INTVAL (operands[1]) < 0x8000
	       && INTVAL (operands[1]) >= -0x8000)
	return "move%.w %1,%0";
    }
  else if (CONSTANT_P (operands[1]))
    return "move%.l %1,%0";
#ifndef SGS_NO_LI
  /* Recognize the insn before a tablejump, one that refers
     to a table of offsets.  Such an insn will need to refer
     to a label on the insn.  So output one.  Use the label-number
     of the table of offsets to generate this label.  */
  if (GET_CODE (operands[1]) == MEM
      && GET_CODE (XEXP (operands[1], 0)) == PLUS
      && (GET_CODE (XEXP (XEXP (operands[1], 0), 0)) == LABEL_REF
	  || GET_CODE (XEXP (XEXP (operands[1], 0), 1)) == LABEL_REF)
      && GET_CODE (XEXP (XEXP (operands[1], 0), 0)) != PLUS
      && GET_CODE (XEXP (XEXP (operands[1], 0), 1)) != PLUS)
    {
      rtx labelref;
      if (GET_CODE (XEXP (XEXP (operands[1], 0), 0)) == LABEL_REF)
	labelref = XEXP (XEXP (operands[1], 0), 0);
      else
	labelref = XEXP (XEXP (operands[1], 0), 1);
#if defined (MOTOROLA) && !defined (SGS_SWITCH_TABLES)
#ifdef SGS
      asm_fprintf (asm_out_file, "\tset %LLI%d,.+2\n",
		   CODE_LABEL_NUMBER (XEXP (labelref, 0)));
#else /* not SGS */
      asm_fprintf (asm_out_file, "\t.set %LLI%d,.+2\n",
	           CODE_LABEL_NUMBER (XEXP (labelref, 0)));
#endif /* not SGS */
#else /* SGS_SWITCH_TABLES or not MOTOROLA */
      ASM_OUTPUT_INTERNAL_LABEL (asm_out_file, "LI",
				 CODE_LABEL_NUMBER (XEXP (labelref, 0)));
#ifdef SGS_SWITCH_TABLES
      /* Set flag saying we need to define the symbol
	 LD%n (with value L%n-LI%n) at the end of the switch table.  */
      switch_table_difference_label_flag = 1;
#endif /* SGS_SWITCH_TABLES */
#endif /* SGS_SWITCH_TABLES or not MOTOROLA */
    }
#endif /* SGS_NO_LI */
  return "move%.w %1,%0";
}
}

static char *
output_31 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT)
    {
      if (operands[1] == const0_rtx
	  && (DATA_REG_P (operands[0])
	      || GET_CODE (operands[0]) == MEM)
	  /* clr insns on 68000 read before writing.
	     This isn't so on the 68010, but we have no alternative for it.  */
	  && (TARGET_68020
	      || !(GET_CODE (operands[0]) == MEM
		   && MEM_VOLATILE_P (operands[0]))))
	return "clr%.w %0";
    }
  return "move%.w %1,%0";
}
}

static char *
output_32 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx xoperands[4];

  /* This is probably useless, since it loses for pushing a struct
     of several bytes a byte at a time.  */
  if (GET_CODE (operands[0]) == MEM
      && GET_CODE (XEXP (operands[0], 0)) == PRE_DEC
      && XEXP (XEXP (operands[0], 0), 0) == stack_pointer_rtx)
    {
      xoperands[1] = operands[1];
      xoperands[2]
        = gen_rtx (MEM, QImode,
		   gen_rtx (PLUS, VOIDmode, stack_pointer_rtx, const1_rtx));
      /* Just pushing a byte puts it in the high byte of the halfword.  */
      /* We must put it in the low-order, high-numbered byte.  */
      output_asm_insn ("move%.b %1,%-\n\tmove%.b %@,%2", xoperands);
      return "";
    }

  /* Moving a byte into an address register is not possible.  */
  /* Use d0 as an intermediate, but don't clobber its contents.  */
  if (ADDRESS_REG_P (operands[0]) && GET_CODE (operands[1]) == MEM)
    {
      /* ??? For 2.5, don't allow this choice and use secondary reloads
	 instead.

	 See if the address register is used in the address.  If it
	 is, we have to generate a more complex sequence than those below.  */
      if (refers_to_regno_p (REGNO (operands[0]), REGNO (operands[0]) + 1,
			     operands[1], NULL_RTX))
	{
	  /* See if the stack pointer is used in the address.  If it isn't,
	     we can push d0 or d1 (the insn can't use both of them) on
	     the stack, perform our move into d0/d1, copy the byte from d0/1,
	     and pop d0/1.  */
	  if (! reg_mentioned_p (stack_pointer_rtx, operands[1]))
	    {
	      if (refers_to_regno_p (0, 1, operands[1], NULL_RTX))
		return "move%.l %/d0,%-\n\tmove%.b %1,%/d0\n\tmove%.l %/d0,%0\n\tmove%.l %+,%/d0";
	      else
		return "move%.l %/d1,%-\n\tmove%.b %1,%/d1\n\tmove%.l %/d1,%0\n\tmove%.l %+,%/d1";
	    }
	  else
	    {
	      /* Otherwise, we know that d0 cannot be used in the address
		 (since sp and one address register is).  Assume that sp is
		 being used as a base register and replace the address
		 register that is our operand[0] with d0.  */
	      rtx reg_map[FIRST_PSEUDO_REGISTER];
	      int i;

	      for (i = 0; i < FIRST_PSEUDO_REGISTER; i++)
		reg_map[i] = 0;

	      reg_map[REGNO (operands[0])] = gen_rtx (REG, Pmode, 0);
	      operands[1] = copy_rtx (operands[1]);
	      replace_regs (operands[1], reg_map, FIRST_PSEUDO_REGISTER, 0);
	      return "exg %/d0,%0\n\tmove%.b %1,%/d0\n\texg %/d0,%0";
	    }
	}

      /* If the address of operand 1 uses d0, choose d1 as intermediate.  */
      if (refers_to_regno_p (0, 1, operands[1], NULL_RTX))
	return "exg %/d1,%0\n\tmove%.b %1,%/d1\n\texg %/d1,%0";
      /* Otherwise d0 is usable.
	 (An effective address on the 68k can't use two d-regs.)  */
      else
	return "exg %/d0,%0\n\tmove%.b %1,%/d0\n\texg %/d0,%0";
    }
    
  /* Likewise for moving from an address reg.  */
  if (ADDRESS_REG_P (operands[1]) && GET_CODE (operands[0]) == MEM)
    {
      /* ??? For 2.5, don't allow this choice and use secondary reloads
	 instead.

	 See if the address register is used in the address.  If it
	 is, we have to generate a more complex sequence than those below.  */
      if (refers_to_regno_p (REGNO (operands[1]), REGNO (operands[1]) + 1,
			     operands[0], NULL_RTX))
	{
	  /* See if the stack pointer is used in the address.  If it isn't,
	     we can push d0 or d1 (the insn can't use both of them) on
	     the stack, copy the byte to d0/1, perform our move from d0/d1, 
	     and pop d0/1.  */
	  if (! reg_mentioned_p (stack_pointer_rtx, operands[0]))
	    {
	      if (refers_to_regno_p (0, 1, operands[0], NULL_RTX))
		return "move%.l %/d0,%-\n\tmove%.l %1,%/d0\n\tmove%.b %/d0,%0\n\tmove%.l %+,%/d0";
	      else
		return "move%.l %/d1,%-\n\tmove%.l %1,%/d1\n\tmove%.b %/d1,%0\n\tmove%.l %+,%/d1";
	    }
	  else
	    {
	      /* Otherwise, we know that d0 cannot be used in the address
		 (since sp and one address register is).  Assume that sp is
		 being used as a base register and replace the address
		 register that is our operand[1] with d0.  */
	      rtx reg_map[FIRST_PSEUDO_REGISTER];
	      int i;

	      for (i = 0; i < FIRST_PSEUDO_REGISTER; i++)
		reg_map[i] = 0;

	      reg_map[REGNO (operands[1])] = gen_rtx (REG, Pmode, 0);
	      operands[0] = copy_rtx (operands[0]);
	      replace_regs (operands[0], reg_map, FIRST_PSEUDO_REGISTER, 0);
	      return "exg %/d0,%1\n\tmove%.b %/d0,%0\n\texg %/d0,%1";
	    }
	}

      if (refers_to_regno_p (0, 1, operands[0], NULL_RTX))
        return "exg %/d1,%1\n\tmove%.b %/d1,%0\n\texg %/d1,%1";
      else
        return "exg %/d0,%1\n\tmove%.b %/d0,%0\n\texg %/d0,%1";
    }

  /* clr and st insns on 68000 read before writing.
     This isn't so on the 68010, but we have no alternative for it.  */
  if (TARGET_68020
      || !(GET_CODE (operands[0]) == MEM && MEM_VOLATILE_P (operands[0])))
    {
      if (operands[1] == const0_rtx)
	return "clr%.b %0";
      if (GET_CODE (operands[1]) == CONST_INT
	  && INTVAL (operands[1]) == -1)
	{
	  CC_STATUS_INIT;
	  return "st %0";
	}
    }
  if (GET_CODE (operands[1]) != CONST_INT && CONSTANT_P (operands[1]))
    return "move%.l %1,%0";
  if (ADDRESS_REG_P (operands[0]) || ADDRESS_REG_P (operands[1]))
    return "move%.w %1,%0";
  return "move%.b %1,%0";
}
}

static char *
output_33 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (operands[1] == const0_rtx
      /* clr insns on 68000 read before writing.
         This isn't so on the 68010, but we have no alternative for it.  */
      && (TARGET_68020
          || !(GET_CODE (operands[0]) == MEM && MEM_VOLATILE_P (operands[0]))))
    return "clr%.b %0";
  return "move%.b %1,%0";
}
}

static char *
output_34 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (which_alternative >= 4)
    return "fpmove%.s %1,fpa0\n\tfpmove%.s fpa0,%0";
  if (FPA_REG_P (operands[0]))
    {
      if (FPA_REG_P (operands[1]))
	return "fpmove%.s %x1,%x0";
      else if (GET_CODE (operands[1]) == CONST_DOUBLE)
	return output_move_const_single (operands);
      else if (FP_REG_P (operands[1]))
        return "fmove%.s %1,sp@-\n\tfpmove%.d sp@+, %0";
      return "fpmove%.s %x1,%x0";
    }
  if (FPA_REG_P (operands[1]))
    {
      if (FP_REG_P (operands[0]))
	return "fpmove%.s %x1,sp@-\n\tfmove%.s sp@+,%0";
      else
	return "fpmove%.s %x1,%x0";
    }
  if (FP_REG_P (operands[0]))
    {
      if (FP_REG_P (operands[1]))
	return "f%$move%.x %1,%0";
      else if (ADDRESS_REG_P (operands[1]))
	return "move%.l %1,%-\n\tf%$move%.s %+,%0";
      else if (GET_CODE (operands[1]) == CONST_DOUBLE)
	return output_move_const_single (operands);
      return "f%$move%.s %f1,%0";
    }
  if (FP_REG_P (operands[1]))
    {
      if (ADDRESS_REG_P (operands[0]))
	return "fmove%.s %1,%-\n\tmove%.l %+,%0";
      return "fmove%.s %f1,%0";
    }
  return "move%.l %1,%0";
}
}

static char *
output_35 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (which_alternative == 6)
    return "fpmove%.d %x1,fpa0\n\tfpmove%.d fpa0,%x0";
  if (FPA_REG_P (operands[0]))
    {
      if (GET_CODE (operands[1]) == CONST_DOUBLE)
	return output_move_const_double (operands);
      if (FP_REG_P (operands[1]))
        return "fmove%.d %1,sp@-\n\tfpmove%.d sp@+,%x0";
      return "fpmove%.d %x1,%x0";
    }
  else if (FPA_REG_P (operands[1]))
    {
      if (FP_REG_P(operands[0]))
        return "fpmove%.d %x1,sp@-\n\tfmoved sp@+,%0";
      else
        return "fpmove%.d %x1,%x0";
    }
  if (FP_REG_P (operands[0]))
    {
      if (FP_REG_P (operands[1]))
	return "f%&move%.x %1,%0";
      if (REG_P (operands[1]))
	{
	  rtx xoperands[2];
	  xoperands[1] = gen_rtx (REG, SImode, REGNO (operands[1]) + 1);
	  output_asm_insn ("move%.l %1,%-", xoperands);
	  output_asm_insn ("move%.l %1,%-", operands);
	  return "f%&move%.d %+,%0";
	}
      if (GET_CODE (operands[1]) == CONST_DOUBLE)
	return output_move_const_double (operands);
      return "f%&move%.d %f1,%0";
    }
  else if (FP_REG_P (operands[1]))
    {
      if (REG_P (operands[0]))
	{
	  output_asm_insn ("fmove%.d %f1,%-\n\tmove%.l %+,%0", operands);
	  operands[0] = gen_rtx (REG, SImode, REGNO (operands[0]) + 1);
	  return "move%.l %+,%0";
	}
      else
        return "fmove%.d %f1,%0";
    }
  return output_move_double (operands);
}

}

static char *
output_37 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[0]))
    {
      if (FP_REG_P (operands[1]))
	return "fmove%.x %1,%0";
      if (REG_P (operands[1]))
	{
	  rtx xoperands[2];
	  xoperands[1] = gen_rtx (REG, SImode, REGNO (operands[1]) + 2);
	  output_asm_insn ("move%.l %1,%-", xoperands);
	  xoperands[1] = gen_rtx (REG, SImode, REGNO (operands[1]) + 1);
	  output_asm_insn ("move%.l %1,%-", xoperands);
	  output_asm_insn ("move%.l %1,%-", operands);
	  return "fmove%.x %+,%0";
	}
      if (GET_CODE (operands[1]) == CONST_DOUBLE)
        return "fmove%.x %1,%0";
      return "fmove%.x %f1,%0";
    }
  if (REG_P (operands[0]))
    {
      output_asm_insn ("fmove%.x %f1,%-\n\tmove%.l %+,%0", operands);
      operands[0] = gen_rtx (REG, SImode, REGNO (operands[0]) + 1);
      output_asm_insn ("move%.l %+,%0", operands);
      operands[0] = gen_rtx (REG, SImode, REGNO (operands[0]) + 1);
      return "move%.l %+,%0";
    }
  return "fmove%.x %f1,%0";
}

}

static char *
output_38 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[0]))
    {
      if (FP_REG_P (operands[1]))
	return "fmove%.x %1,%0";
      if (REG_P (operands[1]))
	{
	  rtx xoperands[2];
	  xoperands[1] = gen_rtx (REG, SImode, REGNO (operands[1]) + 2);
	  output_asm_insn ("move%.l %1,%-", xoperands);
	  xoperands[1] = gen_rtx (REG, SImode, REGNO (operands[1]) + 1);
	  output_asm_insn ("move%.l %1,%-", xoperands);
	  output_asm_insn ("move%.l %1,%-", operands);
	  return "fmove%.x %+,%0";
	}
      if (GET_CODE (operands[1]) == CONST_DOUBLE)
        return "fmove%.x %1,%0";
      return "fmove%.x %f1,%0";
    }
  if (FP_REG_P (operands[1]))
    {
      if (REG_P (operands[0]))
        {
          output_asm_insn ("fmove%.x %f1,%-\n\tmove%.l %+,%0", operands);
          operands[0] = gen_rtx (REG, SImode, REGNO (operands[0]) + 1);
          output_asm_insn ("move%.l %+,%0", operands);
          operands[0] = gen_rtx (REG, SImode, REGNO (operands[0]) + 1);
          return "move%.l %+,%0";
        }
      else
        return "fmove%.x %f1,%0";
    }
  return output_move_double (operands);
}

}

static char *
output_39 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (which_alternative == 8)
    return "fpmove%.d %x1,fpa0\n\tfpmove%.d fpa0,%x0";
  if (FPA_REG_P (operands[0]) || FPA_REG_P (operands[1]))
    return "fpmove%.d %x1,%x0";
  if (FP_REG_P (operands[0]))
    {
      if (FP_REG_P (operands[1]))
	return "fmove%.x %1,%0";
      if (REG_P (operands[1]))
	{
	  rtx xoperands[2];
	  xoperands[1] = gen_rtx (REG, SImode, REGNO (operands[1]) + 1);
	  output_asm_insn ("move%.l %1,%-", xoperands);
	  output_asm_insn ("move%.l %1,%-", operands);
	  return "fmove%.d %+,%0";
	}
      if (GET_CODE (operands[1]) == CONST_DOUBLE)
	return output_move_const_double (operands);
      return "fmove%.d %f1,%0";
    }
  else if (FP_REG_P (operands[1]))
    {
      if (REG_P (operands[0]))
	{
	  output_asm_insn ("fmove%.d %f1,%-\n\tmove%.l %+,%0", operands);
	  operands[0] = gen_rtx (REG, SImode, REGNO (operands[0]) + 1);
	  return "move%.l %+,%0";
	}
      else
        return "fmove%.d %f1,%0";
    }
  return output_move_double (operands);
}

}

static char *
output_41 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[0]) == REG)
    {
      /* Must clear condition codes, since the move.l bases them on
	 the entire 32 bits, not just the desired 8 bits.  */
      CC_STATUS_INIT;
      return "move%.l %1,%0";
    }
  if (GET_CODE (operands[1]) == MEM)
    operands[1] = adj_offsettable_operand (operands[1], 3);
  return "move%.b %1,%0";
}
}

static char *
output_42 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[0]) == REG
      && (GET_CODE (operands[1]) == MEM
	  || GET_CODE (operands[1]) == CONST_INT))
    {
      /* Must clear condition codes, since the move.w bases them on
	 the entire 16 bits, not just the desired 8 bits.  */
      CC_STATUS_INIT;
      return "move%.w %1,%0";
    }
  if (GET_CODE (operands[0]) == REG)
    {
      /* Must clear condition codes, since the move.l bases them on
	 the entire 32 bits, not just the desired 8 bits.  */
      CC_STATUS_INIT;
      return "move%.l %1,%0";
    }
  if (GET_CODE (operands[1]) == MEM)
    operands[1] = adj_offsettable_operand (operands[1], 1);
  return "move%.b %1,%0";
}
}

static char *
output_43 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[0]) == REG)
    {
      /* Must clear condition codes, since the move.l bases them on
	 the entire 32 bits, not just the desired 8 bits.  */
      CC_STATUS_INIT;
      return "move%.l %1,%0";
    }
  if (GET_CODE (operands[1]) == MEM)
    operands[1] = adj_offsettable_operand (operands[1], 2);
  return "move%.w %1,%0";
}
}

static char *
output_47 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (DATA_REG_P (operands[0]))
    {
      if (GET_CODE (operands[1]) == REG
	  && REGNO (operands[0]) == REGNO (operands[1]))
	return "and%.l %#0xFFFF,%0";
      if (reg_mentioned_p (operands[0], operands[1]))
        return "move%.w %1,%0\n\tand%.l %#0xFFFF,%0";
      return "clr%.l %0\n\tmove%.w %1,%0";
    }
  else if (GET_CODE (operands[0]) == MEM
	   && GET_CODE (XEXP (operands[0], 0)) == PRE_DEC)
    return "move%.w %1,%0\n\tclr%.w %0";
  else if (GET_CODE (operands[0]) == MEM
	   && GET_CODE (XEXP (operands[0], 0)) == POST_INC)
    return "clr%.w %0\n\tmove%.w %1,%0";
  else
    {
      output_asm_insn ("clr%.w %0", operands);
      operands[0] = adj_offsettable_operand (operands[0], 2);
      return "move%.w %1,%0";
    }
}
}

static char *
output_48 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (DATA_REG_P (operands[0]))
    {
      if (GET_CODE (operands[1]) == REG
	  && REGNO (operands[0]) == REGNO (operands[1]))
	return "and%.w %#0xFF,%0";
      if (reg_mentioned_p (operands[0], operands[1]))
        return "move%.b %1,%0\n\tand%.w %#0xFF,%0";
      return "clr%.w %0\n\tmove%.b %1,%0";
    }
  else if (GET_CODE (operands[0]) == MEM
	   && GET_CODE (XEXP (operands[0], 0)) == PRE_DEC)
    {
      if (REGNO (XEXP (XEXP (operands[0], 0), 0))
	  == STACK_POINTER_REGNUM)
	{
	  output_asm_insn ("clr%.w %-", operands);
	  operands[0] = gen_rtx (MEM, GET_MODE (operands[0]),
				 plus_constant (stack_pointer_rtx, 1));
	  return "move%.b %1,%0";
	}
      else
	return "move%.b %1,%0\n\tclr%.b %0";
    }
  else if (GET_CODE (operands[0]) == MEM
	   && GET_CODE (XEXP (operands[0], 0)) == POST_INC)
    return "clr%.b %0\n\tmove%.b %1,%0";
  else
    {
      output_asm_insn ("clr%.b %0", operands);
      operands[0] = adj_offsettable_operand (operands[0], 1);
      return "move%.b %1,%0";
    }
}
}

static char *
output_49 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (DATA_REG_P (operands[0]))
    {
      if (GET_CODE (operands[1]) == REG
	  && REGNO (operands[0]) == REGNO (operands[1]))
	return "and%.l %#0xFF,%0";
      if (reg_mentioned_p (operands[0], operands[1]))
        return "move%.b %1,%0\n\tand%.l %#0xFF,%0";
      return "clr%.l %0\n\tmove%.b %1,%0";
    }
  else if (GET_CODE (operands[0]) == MEM
	   && GET_CODE (XEXP (operands[0], 0)) == PRE_DEC)
    {
      operands[0] = XEXP (XEXP (operands[0], 0), 0);
#ifdef MOTOROLA
#ifdef SGS
      return "clr%.l -(%0)\n\tmove%.b %1,3(%0)";
#else
      return "clr%.l -(%0)\n\tmove%.b %1,(3,%0)";
#endif
#else
      return "clrl %0@-\n\tmoveb %1,%0@(3)";
#endif
    }
  else if (GET_CODE (operands[0]) == MEM
	   && GET_CODE (XEXP (operands[0], 0)) == POST_INC)
    {
      operands[0] = XEXP (XEXP (operands[0], 0), 0);
#ifdef MOTOROLA
#ifdef SGS
      return "clr%.l (%0)+\n\tmove%.b %1,-1(%0)";
#else
      return "clr%.l (%0)+\n\tmove%.b %1,(-1,%0)";
#endif
#else
      return "clrl %0@+\n\tmoveb %1,%0@(-1)";
#endif
    }
  else
    {
      output_asm_insn ("clr%.l %0", operands);
      operands[0] = adj_offsettable_operand (operands[0], 3);
      return "move%.b %1,%0";
    }
}
}

static char *
output_50 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (ADDRESS_REG_P (operands[0]))
    return "move%.w %1,%0";
  return "ext%.l %0";
}
}

static char *
output_55 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[0]) && FP_REG_P (operands[1]))
    {
      if (REGNO (operands[0]) == REGNO (operands[1]))
	{
	  /* Extending float to double in an fp-reg is a no-op.
	     NOTICE_UPDATE_CC has already assumed that the
	     cc will be set.  So cancel what it did.  */
	  cc_status = cc_prev_status;
	  return "";
	}
      return "f%&move%.x %1,%0";
    }
  if (FP_REG_P (operands[0]))
    return "f%&move%.s %f1,%0";
  if (DATA_REG_P (operands[0]) && FP_REG_P (operands[1]))
    {
      output_asm_insn ("fmove%.d %f1,%-\n\tmove%.l %+,%0", operands);
      operands[0] = gen_rtx (REG, SImode, REGNO (operands[0]) + 1);
      return "move%.l %+,%0";
    }
  return "fmove%.d %f1,%0";
}
}

static char *
output_58 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[1]))
    return "f%$move%.x %1,%0";
  return "f%$move%.d %f1,%0";
}
}

static char *
output_70 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
  return "fmovem%.l %!,%2\n\tmoveq %#16,%3\n\tor%.l %2,%3\n\tand%.w %#-33,%3\n\tfmovem%.l %3,%!\n\tfmove%.l %1,%0\n\tfmovem%.l %2,%!";
}
}

static char *
output_71 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
  return "fmovem%.l %!,%2\n\tmoveq %#16,%3\n\tor%.l %2,%3\n\tand%.w %#-33,%3\n\tfmovem%.l %3,%!\n\tfmove%.w %1,%0\n\tfmovem%.l %2,%!";
}
}

static char *
output_72 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
  return "fmovem%.l %!,%2\n\tmoveq %#16,%3\n\tor%.l %2,%3\n\tand%.w %#-33,%3\n\tfmovem%.l %3,%!\n\tfmove%.b %1,%0\n\tfmovem%.l %2,%!";
}
}

static char *
output_73 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[1]))
    return "fintrz%.x %f1,%0";
  return "fintrz%.d %f1,%0";
}
}

static char *
output_74 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[1]))
    return "fintrz%.x %f1,%0";
  return "fintrz%.s %f1,%0";
}
}

static char *
output_83 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (! operands_match_p (operands[0], operands[1]))
    {
      if (!ADDRESS_REG_P (operands[1]))
	{
	  rtx tmp = operands[1];

	  operands[1] = operands[2];
	  operands[2] = tmp;
	}

      /* These insns can result from reloads to access
	 stack slots over 64k from the frame pointer.  */
      if (GET_CODE (operands[2]) == CONST_INT
	  && INTVAL (operands[2]) + 0x8000 >= (unsigned) 0x10000)
        return "move%.l %2,%0\n\tadd%.l %1,%0";
#ifdef SGS
      if (GET_CODE (operands[2]) == REG)
	return "lea 0(%1,%2.l),%0";
      else
	return "lea %c2(%1),%0";
#else /* not SGS */
#ifdef MOTOROLA
      if (GET_CODE (operands[2]) == REG)
	return "lea (%1,%2.l),%0";
      else
	return "lea (%c2,%1),%0";
#else /* not MOTOROLA (MIT syntax) */
      if (GET_CODE (operands[2]) == REG)
	return "lea %1@(0,%2:l),%0";
      else
	return "lea %1@(%c2),%0";
#endif /* not MOTOROLA */
#endif /* not SGS */
    }
  if (GET_CODE (operands[2]) == CONST_INT)
    {
#ifndef NO_ADDSUB_Q
      if (INTVAL (operands[2]) > 0
	  && INTVAL (operands[2]) <= 8)
	return (ADDRESS_REG_P (operands[0])
		? "addq%.w %2,%0"
		: "addq%.l %2,%0");
      if (INTVAL (operands[2]) < 0
	  && INTVAL (operands[2]) >= -8)
        {
	  operands[2] = gen_rtx (CONST_INT, VOIDmode,
			         - INTVAL (operands[2]));
	  return (ADDRESS_REG_P (operands[0])
		  ? "subq%.w %2,%0"
		  : "subq%.l %2,%0");
	}
      /* On everything except the 68000 it is faster to use two
	 addqw instructions to add a small integer (8 < N <= 16)
	 to an address register.  Likewise for subqw.*/
      if (INTVAL (operands[2]) > 8
	  && INTVAL (operands[2]) <= 16
	  && ADDRESS_REG_P (operands[0])
	  && TARGET_68020) 
	{
	  operands[2] = gen_rtx (CONST_INT, VOIDmode, INTVAL (operands[2]) - 8);
	  return "addq%.w %#8,%0\n\taddq%.w %2,%0";
	}
      if (INTVAL (operands[2]) < -8
	  && INTVAL (operands[2]) >= -16
	  && ADDRESS_REG_P (operands[0])
	  && TARGET_68020) 
	{
	  operands[2] = gen_rtx (CONST_INT, VOIDmode, 
				  - INTVAL (operands[2]) - 8);
	  return "subq%.w %#8,%0\n\tsubq%.w %2,%0";
	}
#endif
      if (ADDRESS_REG_P (operands[0])
	  && INTVAL (operands[2]) >= -0x8000
	  && INTVAL (operands[2]) < 0x8000)
	return "add%.w %2,%0";
    }
  return "add%.l %2,%0";
}
}

static char *
output_85 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifndef NO_ADDSUB_Q
  if (GET_CODE (operands[2]) == CONST_INT)
    {
      /* If the constant would be a negative number when interpreted as
	 HImode, make it negative.  This is usually, but not always, done
	 elsewhere in the compiler.  First check for constants out of range,
	 which could confuse us.  */

      if (INTVAL (operands[2]) >= 32768)
	operands[2] = gen_rtx (CONST_INT, VOIDmode,
			       INTVAL (operands[2]) - 65536);

      if (INTVAL (operands[2]) > 0
	  && INTVAL (operands[2]) <= 8)
	return "addq%.w %2,%0";
      if (INTVAL (operands[2]) < 0
	  && INTVAL (operands[2]) >= -8)
	{
	  operands[2] = gen_rtx (CONST_INT, VOIDmode,
			         - INTVAL (operands[2]));
	  return "subq%.w %2,%0";
	}
      /* On everything except the 68000 it is faster to use two
	 addqw instructions to add a small integer (8 < N <= 16)
	 to an address register.  Likewise for subqw. */
      if (INTVAL (operands[2]) > 8
	  && INTVAL (operands[2]) <= 16
	  && ADDRESS_REG_P (operands[0])
	  && TARGET_68020) 
	{
	  operands[2] = gen_rtx (CONST_INT, VOIDmode, INTVAL (operands[2]) - 8);
	  return "addq%.w %#8,%0\n\taddq%.w %2,%0";
	}
      if (INTVAL (operands[2]) < -8
	  && INTVAL (operands[2]) >= -16
	  && ADDRESS_REG_P (operands[0])
	  && TARGET_68020) 
	{
	  operands[2] = gen_rtx (CONST_INT, VOIDmode, 
				 - INTVAL (operands[2]) - 8);
	  return "subq%.w %#8,%0\n\tsubq%.w %2,%0";
	}
    }
#endif
  return "add%.w %2,%0";
}
}

static char *
output_86 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifndef NO_ADDSUB_Q
  if (GET_CODE (operands[1]) == CONST_INT)
    {
      /* If the constant would be a negative number when interpreted as
	 HImode, make it negative.  This is usually, but not always, done
	 elsewhere in the compiler.  First check for constants out of range,
	 which could confuse us.  */

      if (INTVAL (operands[1]) >= 32768)
	operands[1] = gen_rtx (CONST_INT, VOIDmode,
			       INTVAL (operands[1]) - 65536);

      if (INTVAL (operands[1]) > 0
	  && INTVAL (operands[1]) <= 8)
	return "addq%.w %1,%0";
      if (INTVAL (operands[1]) < 0
	  && INTVAL (operands[1]) >= -8)
	{
	  operands[1] = gen_rtx (CONST_INT, VOIDmode,
			         - INTVAL (operands[1]));
	  return "subq%.w %1,%0";
	}
      /* On everything except the 68000 it is faster to use two
	 addqw instructions to add a small integer (8 < N <= 16)
	 to an address register.  Likewise for subqw. */
      if (INTVAL (operands[1]) > 8
	  && INTVAL (operands[1]) <= 16
	  && ADDRESS_REG_P (operands[0])
	  && TARGET_68020) 
	{
	  operands[1] = gen_rtx (CONST_INT, VOIDmode, INTVAL (operands[1]) - 8);
	  return "addq%.w %#8,%0\n\taddq%.w %1,%0";
	}
      if (INTVAL (operands[1]) < -8
	  && INTVAL (operands[1]) >= -16
	  && ADDRESS_REG_P (operands[0])
	  && TARGET_68020) 
	{
	  operands[1] = gen_rtx (CONST_INT, VOIDmode, 
				 - INTVAL (operands[1]) - 8);
	  return "subq%.w %#8,%0\n\tsubq%.w %1,%0";
	}
    }
#endif
  return "add%.w %1,%0";
}
}

static char *
output_87 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifndef NO_ADDSUB_Q
  if (GET_CODE (operands[1]) == CONST_INT)
    {
      /* If the constant would be a negative number when interpreted as
	 HImode, make it negative.  This is usually, but not always, done
	 elsewhere in the compiler.  First check for constants out of range,
	 which could confuse us.  */

      if (INTVAL (operands[1]) >= 32768)
	operands[1] = gen_rtx (CONST_INT, VOIDmode,
			       INTVAL (operands[1]) - 65536);

      if (INTVAL (operands[1]) > 0
	  && INTVAL (operands[1]) <= 8)
	return "addq%.w %1,%0";
      if (INTVAL (operands[1]) < 0
	  && INTVAL (operands[1]) >= -8)
	{
	  operands[1] = gen_rtx (CONST_INT, VOIDmode,
			         - INTVAL (operands[1]));
	  return "subq%.w %1,%0";
	}
      /* On everything except the 68000 it is faster to use two
	 addqw instructions to add a small integer (8 < N <= 16)
	 to an address register.  Likewise for subqw. */
      if (INTVAL (operands[1]) > 8
	  && INTVAL (operands[1]) <= 16
	  && ADDRESS_REG_P (operands[0])
	  && TARGET_68020) 
	{
	  operands[1] = gen_rtx (CONST_INT, VOIDmode, INTVAL (operands[1]) - 8);
	  return "addq%.w %#8,%0\n\taddq%.w %1,%0";
	}
      if (INTVAL (operands[1]) < -8
	  && INTVAL (operands[1]) >= -16
	  && ADDRESS_REG_P (operands[0])
	  && TARGET_68020) 
	{
	  operands[1] = gen_rtx (CONST_INT, VOIDmode, 
				 - INTVAL (operands[1]) - 8);
	  return "subq%.w %#8,%0\n\tsubq%.w %1,%0";
	}
    }
#endif
  return "add%.w %1,%0";
}
}

static char *
output_88 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifndef NO_ADDSUB_Q
  if (GET_CODE (operands[2]) == CONST_INT)
    {
      if (INTVAL (operands[2]) >= 128)
	operands[2] = gen_rtx (CONST_INT, VOIDmode,
			       INTVAL (operands[2]) - 256);

      if (INTVAL (operands[2]) > 0
	  && INTVAL (operands[2]) <= 8)
	return "addq%.b %2,%0";
      if (INTVAL (operands[2]) < 0 && INTVAL (operands[2]) >= -8)
       {
	 operands[2] = gen_rtx (CONST_INT, VOIDmode, - INTVAL (operands[2]));
	 return "subq%.b %2,%0";
       }
    }
#endif
  return "add%.b %2,%0";
}
}

static char *
output_89 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifndef NO_ADDSUB_Q
  if (GET_CODE (operands[1]) == CONST_INT)
    {
      if (INTVAL (operands[1]) >= 128)
	operands[1] = gen_rtx (CONST_INT, VOIDmode,
			       INTVAL (operands[1]) - 256);

      if (INTVAL (operands[1]) > 0
	  && INTVAL (operands[1]) <= 8)
	return "addq%.b %1,%0";
      if (INTVAL (operands[1]) < 0 && INTVAL (operands[1]) >= -8)
       {
	 operands[1] = gen_rtx (CONST_INT, VOIDmode, - INTVAL (operands[1]));
	 return "subq%.b %1,%0";
       }
    }
#endif
  return "add%.b %1,%0";
}
}

static char *
output_90 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifndef NO_ADDSUB_Q
  if (GET_CODE (operands[1]) == CONST_INT)
    {
      if (INTVAL (operands[1]) >= 128)
	operands[1] = gen_rtx (CONST_INT, VOIDmode,
			       INTVAL (operands[1]) - 256);

      if (INTVAL (operands[1]) > 0
	  && INTVAL (operands[1]) <= 8)
	return "addq%.b %1,%0";
      if (INTVAL (operands[1]) < 0 && INTVAL (operands[1]) >= -8)
       {
	 operands[1] = gen_rtx (CONST_INT, VOIDmode, - INTVAL (operands[1]));
	 return "subq%.b %1,%0";
       }
    }
#endif
  return "add%.b %1,%0";
}
}

static char *
output_92 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (rtx_equal_p (operands[0], operands[1]))
    return "fpadd%.d %y2,%0";
  if (rtx_equal_p (operands[0], operands[2]))
    return "fpadd%.d %y1,%0";
  if (which_alternative == 0)
    return "fpadd3%.d %w2,%w1,%0";
  return "fpadd3%.d %x2,%x1,%0";
}
}

static char *
output_93 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return "f%&add%.x %2,%0";
  return "f%&add%.d %f2,%0";
}
}

static char *
output_95 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (rtx_equal_p (operands[0], operands[1]))
    return "fpadd%.s %w2,%0";
  if (rtx_equal_p (operands[0], operands[2]))
    return "fpadd%.s %w1,%0";
  if (which_alternative == 0)
    return "fpadd3%.s %w2,%w1,%0";
  return "fpadd3%.s %2,%1,%0";
}
}

static char *
output_96 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]) && ! DATA_REG_P (operands[2]))
    return "f%$add%.x %2,%0";
  return "f%$add%.s %f2,%0";
}
}

static char *
output_97 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (! operands_match_p (operands[0], operands[1]))
    {
      if (operands_match_p (operands[0], operands[2]))
	{
#ifndef NO_ADDSUB_Q
	  if (GET_CODE (operands[1]) == CONST_INT)
	    {
	      if (INTVAL (operands[1]) > 0
		  && INTVAL (operands[1]) <= 8)
		return "subq%.l %1,%0\n\tneg%.l %0";
	    }
#endif
	  return "sub%.l %1,%0\n\tneg%.l %0";
	}
      /* This case is matched by J, but negating -0x8000
         in an lea would give an invalid displacement.
	 So do this specially.  */
      if (INTVAL (operands[2]) == -0x8000)
	return "move%.l %1,%0\n\tsub%.l %2,%0";
#ifdef SGS
      return "lea %n2(%1),%0";
#else
#ifdef MOTOROLA
      return "lea (%n2,%1),%0";
#else /* not MOTOROLA (MIT syntax) */
      return "lea %1@(%n2),%0";
#endif /* not MOTOROLA */
#endif /* not SGS */
    }
  if (GET_CODE (operands[2]) == CONST_INT)
    {
#ifndef NO_ADDSUB_Q
      if (INTVAL (operands[2]) > 0
	  && INTVAL (operands[2]) <= 8)
	return "subq%.l %2,%0";
      /* Using two subqw for 8 < N <= 16 being subtracted from an
	 address register is faster on all but 68000 */
      if (INTVAL (operands[2]) > 8
	  && INTVAL (operands[2]) <= 16
	  && ADDRESS_REG_P (operands[0])
	  && TARGET_68020)
	{
	  operands[2] = gen_rtx (CONST_INT, VOIDmode, INTVAL (operands[2]) - 8);
	  return "subq%.w %#8,%0\n\tsubq%.w %2,%0";
	}
#endif
      if (ADDRESS_REG_P (operands[0])
	  && INTVAL (operands[2]) >= -0x8000
	  && INTVAL (operands[2]) < 0x8000)
	return "sub%.w %2,%0";
    }
  return "sub%.l %2,%0";
}
}

static char *
output_104 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (rtx_equal_p (operands[0], operands[2]))
    return "fprsub%.d %y1,%0";
  if (rtx_equal_p (operands[0], operands[1]))
    return "fpsub%.d %y2,%0";
  if (which_alternative == 0)
    return "fpsub3%.d %w2,%w1,%0";
  return "fpsub3%.d %x2,%x1,%0";
}
}

static char *
output_105 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return "f%&sub%.x %2,%0";
  return "f%&sub%.d %f2,%0";
}
}

static char *
output_107 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (rtx_equal_p (operands[0], operands[2]))
    return "fprsub%.s %w1,%0";
  if (rtx_equal_p (operands[0], operands[1]))
    return "fpsub%.s %w2,%0";
  if (which_alternative == 0)
    return "fpsub3%.s %w2,%w1,%0";
  return "fpsub3%.s %2,%1,%0";
}
}

static char *
output_108 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]) && ! DATA_REG_P (operands[2]))
    return "f%$sub%.x %2,%0";
  return "f%$sub%.s %f2,%0";
}
}

static char *
output_109 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#if defined(MOTOROLA) && !defined(CRDS)
  return "muls%.w %2,%0";
#else
  return "muls %2,%0";
#endif
}
}

static char *
output_110 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#if defined(MOTOROLA) && !defined(CRDS)
  return "muls%.w %2,%0";
#else
  return "muls %2,%0";
#endif
}
}

static char *
output_111 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#if defined(MOTOROLA) && !defined(CRDS)
  return "muls%.w %2,%0";
#else
  return "muls %2,%0";
#endif
}
}

static char *
output_113 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#if defined(MOTOROLA) && !defined(CRDS)
  return "mulu%.w %2,%0";
#else
  return "mulu %2,%0";
#endif
}
}

static char *
output_114 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#if defined(MOTOROLA) && !defined(CRDS)
  return "mulu%.w %2,%0";
#else
  return "mulu %2,%0";
#endif
}
}

static char *
output_122 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (rtx_equal_p (operands[1], operands[2]))
    return "fpsqr%.d %y1,%0";
  if (rtx_equal_p (operands[0], operands[1]))
    return "fpmul%.d %y2,%0";
  if (rtx_equal_p (operands[0], operands[2]))
    return "fpmul%.d %y1,%0";
  if (which_alternative == 0)
    return "fpmul3%.d %w2,%w1,%0";
  return "fpmul3%.d %x2,%x1,%0";
}
}

static char *
output_123 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[2]) == CONST_DOUBLE
      && floating_exact_log2 (operands[2]) && !TARGET_68040)
    {
      int i = floating_exact_log2 (operands[2]);
      operands[2] = gen_rtx (CONST_INT, VOIDmode, i);
      return "fscale%.l %2,%0";
    }
  if (REG_P (operands[2]))
    return "f%&mul%.x %2,%0";
  return "f%&mul%.d %f2,%0";
}
}

static char *
output_125 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (rtx_equal_p (operands[1], operands[2]))
    return "fpsqr%.s %w1,%0";
  if (rtx_equal_p (operands[0], operands[1]))
    return "fpmul%.s %w2,%0";
  if (rtx_equal_p (operands[0], operands[2]))
    return "fpmul%.s %w1,%0";
  if (which_alternative == 0)
    return "fpmul3%.s %w2,%w1,%0";
  return "fpmul3%.s %2,%1,%0";
}
}

static char *
output_126 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifdef FSGLMUL_USE_S
  if (REG_P (operands[2]) && ! DATA_REG_P (operands[2]))
    return (TARGET_68040_ONLY
	    ? "fsmul%.s %2,%0"
	    : "fsglmul%.s %2,%0");
#else
  if (REG_P (operands[2]) && ! DATA_REG_P (operands[2]))
    return (TARGET_68040_ONLY
	    ? "fsmul%.x %2,%0"
	    : "fsglmul%.x %2,%0");
#endif
  return (TARGET_68040_ONLY
	  ? "fsmul%.s %f2,%0"
	  : "fsglmul%.s %f2,%0");
}
}

static char *
output_127 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifdef MOTOROLA
  return "ext%.l %0\n\tdivs%.w %2,%0";
#else
  return "extl %0\n\tdivs %2,%0";
#endif
}
}

static char *
output_128 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifdef MOTOROLA
  return "divs%.w %2,%0";
#else
  return "divs %2,%0";
#endif
}
}

static char *
output_129 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifdef MOTOROLA
  return "divs%.w %2,%0";
#else
  return "divs %2,%0";
#endif
}
}

static char *
output_130 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifdef MOTOROLA
  return "and%.l %#0xFFFF,%0\n\tdivu%.w %2,%0";
#else
  return "andl %#0xFFFF,%0\n\tdivu %2,%0";
#endif
}
}

static char *
output_131 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifdef MOTOROLA
  return "divu%.w %2,%0";
#else
  return "divu %2,%0";
#endif
}
}

static char *
output_132 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifdef MOTOROLA
  return "divu%.w %2,%0";
#else
  return "divu %2,%0";
#endif
}
}

static char *
output_134 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (rtx_equal_p (operands[0], operands[2]))
    return "fprdiv%.d %y1,%0";
  if (rtx_equal_p (operands[0], operands[1]))
    return "fpdiv%.d %y2,%0";
  if (which_alternative == 0)
    return "fpdiv3%.d %w2,%w1,%0";
  return "fpdiv3%.d %x2,%x1,%x0";
}
}

static char *
output_135 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return "f%&div%.x %2,%0";
  return "f%&div%.d %f2,%0";
}
}

static char *
output_137 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (rtx_equal_p (operands[0], operands[1]))
    return "fpdiv%.s %w2,%0";
  if (rtx_equal_p (operands[0], operands[2]))
    return "fprdiv%.s %w1,%0";
  if (which_alternative == 0)
    return "fpdiv3%.s %w2,%w1,%0";
  return "fpdiv3%.s %2,%1,%0";
}
}

static char *
output_138 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifdef FSGLDIV_USE_S
  if (REG_P (operands[2]) && ! DATA_REG_P (operands[2]))
    return (TARGET_68040_ONLY
	    ? "fsdiv%.s %2,%0"
	    : "fsgldiv%.s %2,%0");
#else
  if (REG_P (operands[2]) && ! DATA_REG_P (operands[2]))
    return (TARGET_68040_ONLY
	    ? "fsdiv%.x %2,%0"
	    : "fsgldiv%.x %2,%0");
#endif
  return (TARGET_68040_ONLY
	  ? "fsdiv%.s %f2,%0"
	  : "fsgldiv%.s %f2,%0");
}
}

static char *
output_139 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  /* The swap insn produces cc's that don't correspond to the result.  */
  CC_STATUS_INIT;
#ifdef MOTOROLA
  return "ext%.l %0\n\tdivs%.w %2,%0\n\tswap %0";
#else
  return "extl %0\n\tdivs %2,%0\n\tswap %0";
#endif
}
}

static char *
output_140 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  /* The swap insn produces cc's that don't correspond to the result.  */
  CC_STATUS_INIT;
#ifdef MOTOROLA
  return "divs%.w %2,%0\n\tswap %0";
#else
  return "divs %2,%0\n\tswap %0";
#endif
}
}

static char *
output_141 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  /* The swap insn produces cc's that don't correspond to the result.  */
  CC_STATUS_INIT;
#ifdef MOTOROLA
  return "divs%.w %2,%0\n\tswap %0";
#else
  return "divs %2,%0\n\tswap %0";
#endif
}
}

static char *
output_142 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  /* The swap insn produces cc's that don't correspond to the result.  */
  CC_STATUS_INIT;
#ifdef MOTOROLA
  return "and%.l %#0xFFFF,%0\n\tdivu%.w %2,%0\n\tswap %0";
#else
  return "andl %#0xFFFF,%0\n\tdivu %2,%0\n\tswap %0";
#endif
}
}

static char *
output_143 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  /* The swap insn produces cc's that don't correspond to the result.  */
  CC_STATUS_INIT;
#ifdef MOTOROLA
  return "divu%.w %2,%0\n\tswap %0";
#else
  return "divu %2,%0\n\tswap %0";
#endif
}
}

static char *
output_144 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  /* The swap insn produces cc's that don't correspond to the result.  */
  CC_STATUS_INIT;
#ifdef MOTOROLA
  return "divu%.w %2,%0\n\tswap %0";
#else
  return "divu %2,%0\n\tswap %0";
#endif
}
}

static char *
output_145 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (find_reg_note (insn, REG_UNUSED, operands[3]))
    return "divs%.l %2,%0";
  else
    return "divsl%.l %2,%3:%0";
}
}

static char *
output_146 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (find_reg_note (insn, REG_UNUSED, operands[3]))
    return "divu%.l %2,%0";
  else
    return "divul%.l %2,%3:%0";
}
}

static char *
output_147 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  int logval;
  if (GET_CODE (operands[2]) == CONST_INT
      && (INTVAL (operands[2]) | 0xffff) == 0xffffffff
      && (DATA_REG_P (operands[0])
	  || offsettable_memref_p (operands[0])))
    { 
      if (GET_CODE (operands[0]) != REG)
        operands[0] = adj_offsettable_operand (operands[0], 2);
      operands[2] = gen_rtx (CONST_INT, VOIDmode,
			     INTVAL (operands[2]) & 0xffff);
      /* Do not delete a following tstl %0 insn; that would be incorrect.  */
      CC_STATUS_INIT;
      if (operands[2] == const0_rtx)
        return "clr%.w %0";
      return "and%.w %2,%0";
    }
  if (GET_CODE (operands[2]) == CONST_INT
      && (logval = exact_log2 (~ INTVAL (operands[2]))) >= 0
      && (DATA_REG_P (operands[0])
          || offsettable_memref_p (operands[0])))
    { 
      if (DATA_REG_P (operands[0]))
        {
          operands[1] = gen_rtx (CONST_INT, VOIDmode, logval);
        }
      else
        {
          operands[0] = adj_offsettable_operand (operands[0], 3 - (logval / 8));          operands[1] = gen_rtx (CONST_INT, VOIDmode, logval % 8);
        }
      /* This does not set condition codes in a standard way.  */
      CC_STATUS_INIT;
      return "bclr %1,%0";
    }
  return "and%.l %2,%0";
}
}

static char *
output_154 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  register int logval;
  if (GET_CODE (operands[2]) == CONST_INT
      && INTVAL (operands[2]) >> 16 == 0
      && (DATA_REG_P (operands[0])
	  || offsettable_memref_p (operands[0])))
    { 
      if (GET_CODE (operands[0]) != REG)
        operands[0] = adj_offsettable_operand (operands[0], 2);
      /* Do not delete a following tstl %0 insn; that would be incorrect.  */
      CC_STATUS_INIT;
      return "or%.w %2,%0";
    }
  if (GET_CODE (operands[2]) == CONST_INT
      && (logval = exact_log2 (INTVAL (operands[2]))) >= 0
      && (DATA_REG_P (operands[0])
	  || offsettable_memref_p (operands[0])))
    { 
      if (DATA_REG_P (operands[0]))
	{
	  operands[1] = gen_rtx (CONST_INT, VOIDmode, logval);
	}
      else
        {
	  operands[0] = adj_offsettable_operand (operands[0], 3 - (logval / 8));
	  operands[1] = gen_rtx (CONST_INT, VOIDmode, logval % 8);
	}
      CC_STATUS_INIT;
      return "bset %1,%0";
    }
  return "or%.l %2,%0";
}
}

static char *
output_161 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[2]) == CONST_INT
      && INTVAL (operands[2]) >> 16 == 0
      && (offsettable_memref_p (operands[0]) || DATA_REG_P (operands[0])))
    { 
      if (! DATA_REG_P (operands[0]))
	operands[0] = adj_offsettable_operand (operands[0], 2);
      /* Do not delete a following tstl %0 insn; that would be incorrect.  */
      CC_STATUS_INIT;
      return "eor%.w %2,%0";
    }
  return "eor%.l %2,%0";
}
}

static char *
output_175 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (DATA_REG_P (operands[0]))
    {
      operands[1] = gen_rtx (CONST_INT, VOIDmode, 31);
      return "bchg %1,%0";
    }
  if (REG_P (operands[1]) && ! DATA_REG_P (operands[1]))
    return "f%$neg%.x %1,%0";
  return "f%$neg%.s %f1,%0";
}
}

static char *
output_178 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (DATA_REG_P (operands[0]))
    {
      operands[1] = gen_rtx (CONST_INT, VOIDmode, 31);
      return "bchg %1,%0";
    }
  if (REG_P (operands[1]) && ! DATA_REG_P (operands[1]))
    return "f%&neg%.x %1,%0";
  return "f%&neg%.d %f1,%0";
}
}

static char *
output_179 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[1]))
    return "fsqrt%.x %1,%0";
  else
    return "fsqrt%.d %1,%0";
}
}

static char *
output_182 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[1]) && ! DATA_REG_P (operands[1]))
    return "f%$abs%.x %1,%0";
  return "f%$abs%.s %f1,%0";
}
}

static char *
output_185 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[1]) && ! DATA_REG_P (operands[1]))
    return "f%&abs%.x %1,%0";
  return "f%&abs%.d %f1,%0";
}
}

static char *
output_191 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
  return "swap %0\n\tclr%.w %0";
}
}

static char *
output_192 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;

  operands[2] = gen_rtx (CONST_INT, VOIDmode, INTVAL (operands[2]) - 16);
  return "asl%.w %2,%0\n\tswap %0\n\tclr%.w %0";
}
}

static char *
output_193 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (operands[2] == const1_rtx)
    return "add%.l %0,%0";
  return "asl%.l %2,%0";
}
}

static char *
output_199 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  operands[2] = gen_rtx (CONST_INT, VOIDmode, INTVAL (operands[2]) - 16);
  return "swap %0\n\tasr%.w %2,%0\n\text%.l %0";
}
}

static char *
output_200 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  return "asr%.l %2,%0";
}
}

static char *
output_205 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
  return "swap %0\n\tclr%.w %0";
}
}

static char *
output_206 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;

  operands[2] = gen_rtx (CONST_INT, VOIDmode, INTVAL (operands[2]) - 16);
  return "lsl%.w %2,%0\n\tswap %0\n\tclr%.w %0";
}
}

static char *
output_207 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (operands[2] == const1_rtx)
    return "add%.l %0,%0";
  return "lsl%.l %2,%0";
}
}

static char *
output_212 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
  return "clr%.w %0\n\tswap %0";
}
}

static char *
output_213 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  /* I think lsr%.w sets the CC properly.  */
  operands[2] = gen_rtx (CONST_INT, VOIDmode, INTVAL (operands[2]) - 16);
  return "clr%.w %0\n\tswap %0\n\tlsr%.w %2,%0";
}
}

static char *
output_214 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  return "lsr%.l %2,%0";
}
}

static char *
output_229 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  operands[0]
    = adj_offsettable_operand (operands[0], INTVAL (operands[2]) / 8);

  return "move%.l %3,%0";
}
}

static char *
output_230 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[0]))
    {
      if (INTVAL (operands[1]) + INTVAL (operands[2]) != 32)
        return "bfins %3,%0{%b2:%b1}";
    }
  else
    operands[0]
      = adj_offsettable_operand (operands[0], INTVAL (operands[2]) / 8);

  if (GET_CODE (operands[3]) == MEM)
    operands[3] = adj_offsettable_operand (operands[3],
					   (32 - INTVAL (operands[1])) / 8);
  if (INTVAL (operands[1]) == 8)
    return "move%.b %3,%0";
  return "move%.w %3,%0";
}
}

static char *
output_231 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  operands[1]
    = adj_offsettable_operand (operands[1], INTVAL (operands[3]) / 8);

  return "move%.l %1,%0";
}
}

static char *
output_232 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  cc_status.flags |= CC_NOT_NEGATIVE;
  if (REG_P (operands[1]))
    {
      if (INTVAL (operands[2]) + INTVAL (operands[3]) != 32)
	return "bfextu %1{%b3:%b2},%0";
    }
  else
    operands[1]
      = adj_offsettable_operand (operands[1], INTVAL (operands[3]) / 8);

  output_asm_insn ("clr%.l %0", operands);
  if (GET_CODE (operands[0]) == MEM)
    operands[0] = adj_offsettable_operand (operands[0],
					   (32 - INTVAL (operands[1])) / 8);
  if (INTVAL (operands[2]) == 8)
    return "move%.b %1,%0";
  return "move%.w %1,%0";
}
}

static char *
output_233 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  operands[1]
    = adj_offsettable_operand (operands[1], INTVAL (operands[3]) / 8);

  return "move%.l %1,%0";
}
}

static char *
output_234 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[1]))
    {
      if (INTVAL (operands[2]) + INTVAL (operands[3]) != 32)
	return "bfexts %1{%b3:%b2},%0";
    }
  else
    operands[1]
      = adj_offsettable_operand (operands[1], INTVAL (operands[3]) / 8);

  if (INTVAL (operands[2]) == 8)
    return "move%.b %1,%0\n\textb%.l %0";
  return "move%.w %1,%0\n\text%.l %0";
}
}

static char *
output_236 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  cc_status.flags |= CC_NOT_NEGATIVE;
  return "bfextu %1{%b3:%b2},%0";
}
}

static char *
output_237 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
  return "bfchg %0{%b2:%b1}";
}
}

static char *
output_238 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
  return "bfclr %0{%b2:%b1}";
}
}

static char *
output_239 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
  return "bfset %0{%b2:%b1}";
}
}

static char *
output_242 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  cc_status.flags |= CC_NOT_NEGATIVE;
  return "bfextu %1{%b3:%b2},%0";
}
}

static char *
output_243 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
  return "bfclr %0{%b2:%b1}";
}
}

static char *
output_244 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
  return "bfset %0{%b2:%b1}";
}
}

static char *
output_245 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#if 0
  /* These special cases are now recognized by a specific pattern.  */
  if (GET_CODE (operands[1]) == CONST_INT && GET_CODE (operands[2]) == CONST_INT
      && INTVAL (operands[1]) == 16 && INTVAL (operands[2]) == 16)
    return "move%.w %3,%0";
  if (GET_CODE (operands[1]) == CONST_INT && GET_CODE (operands[2]) == CONST_INT
      && INTVAL (operands[1]) == 24 && INTVAL (operands[2]) == 8)
    return "move%.b %3,%0";
#endif
  return "bfins %3,%0{%b2:%b1}";
}
}

static char *
output_246 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (operands[1] == const1_rtx
      && GET_CODE (operands[2]) == CONST_INT)
    {    
      int width = GET_CODE (operands[0]) == REG ? 31 : 7;
      return output_btst (operands,
			  gen_rtx (CONST_INT, VOIDmode,
				   width - INTVAL (operands[2])),
			  operands[0],
			  insn, 1000);
      /* Pass 1000 as SIGNPOS argument so that btst will
         not think we are testing the sign bit for an `and'
	 and assume that nonzero implies a negative result.  */
    }
  if (INTVAL (operands[1]) != 32)
    cc_status.flags = CC_NOT_NEGATIVE;
  return "bftst %0{%b2:%b1}";
}
}

static char *
output_247 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (operands[1] == const1_rtx
      && GET_CODE (operands[2]) == CONST_INT)
    {    
      int width = GET_CODE (operands[0]) == REG ? 31 : 7;
      return output_btst (operands,
			  gen_rtx (CONST_INT, VOIDmode,
				   width - INTVAL (operands[2])),
			  operands[0],
			  insn, 1000);
      /* Pass 1000 as SIGNPOS argument so that btst will
         not think we are testing the sign bit for an `and'
	 and assume that nonzero implies a negative result.  */
    }
  if (INTVAL (operands[1]) != 32)
    cc_status.flags = CC_NOT_NEGATIVE;
  return "bftst %0{%b2:%b1}";
}
}

static char *
output_248 (operands, insn)
     rtx *operands;
     rtx insn;
{

  cc_status = cc_prev_status;
  OUTPUT_JUMP ("seq %0", "fseq %0", "seq %0");

}

static char *
output_249 (operands, insn)
     rtx *operands;
     rtx insn;
{

  cc_status = cc_prev_status;
  OUTPUT_JUMP ("sne %0", "fsne %0", "sne %0");

}

static char *
output_250 (operands, insn)
     rtx *operands;
     rtx insn;
{

  cc_status = cc_prev_status;
  OUTPUT_JUMP ("sgt %0", "fsgt %0", 0);

}

static char *
output_251 (operands, insn)
     rtx *operands;
     rtx insn;
{
 cc_status = cc_prev_status;
     return "shi %0"; 
}

static char *
output_252 (operands, insn)
     rtx *operands;
     rtx insn;
{
 cc_status = cc_prev_status;
     OUTPUT_JUMP ("slt %0", "fslt %0", "smi %0"); 
}

static char *
output_253 (operands, insn)
     rtx *operands;
     rtx insn;
{
 cc_status = cc_prev_status;
     return "scs %0"; 
}

static char *
output_254 (operands, insn)
     rtx *operands;
     rtx insn;
{
 cc_status = cc_prev_status;
     OUTPUT_JUMP ("sge %0", "fsge %0", "spl %0"); 
}

static char *
output_255 (operands, insn)
     rtx *operands;
     rtx insn;
{
 cc_status = cc_prev_status;
     return "scc %0"; 
}

static char *
output_256 (operands, insn)
     rtx *operands;
     rtx insn;
{

  cc_status = cc_prev_status;
  OUTPUT_JUMP ("sle %0", "fsle %0", 0);

}

static char *
output_257 (operands, insn)
     rtx *operands;
     rtx insn;
{
 cc_status = cc_prev_status;
     return "sls %0"; 
}

static char *
output_258 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifdef MOTOROLA
  OUTPUT_JUMP ("jbeq %l0", "fbeq %l0", "jbeq %l0");
#else
  OUTPUT_JUMP ("jeq %l0", "fjeq %l0", "jeq %l0");
#endif
}
}

static char *
output_259 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifdef MOTOROLA
  OUTPUT_JUMP ("jbne %l0", "fbne %l0", "jbne %l0");
#else
  OUTPUT_JUMP ("jne %l0", "fjne %l0", "jne %l0");
#endif
}
}

static char *
output_260 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  OUTPUT_JUMP ("jbgt %l0", "fbgt %l0", 0);
#else
  OUTPUT_JUMP ("jgt %l0", "fjgt %l0", 0);
#endif

}

static char *
output_261 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  return "jbhi %l0";
#else
  return "jhi %l0";
#endif

}

static char *
output_262 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  OUTPUT_JUMP ("jblt %l0", "fblt %l0", "jbmi %l0");
#else
  OUTPUT_JUMP ("jlt %l0", "fjlt %l0", "jmi %l0");
#endif

}

static char *
output_263 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  return "jbcs %l0";
#else
  return "jcs %l0";
#endif

}

static char *
output_264 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  OUTPUT_JUMP ("jbge %l0", "fbge %l0", "jbpl %l0");
#else
  OUTPUT_JUMP ("jge %l0", "fjge %l0", "jpl %l0");
#endif

}

static char *
output_265 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  return "jbcc %l0";
#else
  return "jcc %l0";
#endif

}

static char *
output_266 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  OUTPUT_JUMP ("jble %l0", "fble %l0", 0);
#else
  OUTPUT_JUMP ("jle %l0", "fjle %l0", 0);
#endif

}

static char *
output_267 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  return "jbls %l0";
#else
  return "jls %l0";
#endif

}

static char *
output_268 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifdef MOTOROLA
  OUTPUT_JUMP ("jbne %l0", "fbne %l0", "jbne %l0");
#else
  OUTPUT_JUMP ("jne %l0", "fjne %l0", "jne %l0");
#endif
}
}

static char *
output_269 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifdef MOTOROLA
  OUTPUT_JUMP ("jbeq %l0", "fbeq %l0", "jbeq %l0");
#else
  OUTPUT_JUMP ("jeq %l0", "fjeq %l0", "jeq %l0");
#endif
}
}

static char *
output_270 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  OUTPUT_JUMP ("jble %l0", "fbngt %l0", 0);
#else
  OUTPUT_JUMP ("jle %l0", "fjngt %l0", 0);
#endif

}

static char *
output_271 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  return "jbls %l0";
#else
  return "jls %l0";
#endif

}

static char *
output_272 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  OUTPUT_JUMP ("jbge %l0", "fbnlt %l0", "jbpl %l0");
#else
  OUTPUT_JUMP ("jge %l0", "fjnlt %l0", "jpl %l0");
#endif

}

static char *
output_273 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  return "jbcc %l0";
#else
  return "jcc %l0";
#endif

}

static char *
output_274 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  OUTPUT_JUMP ("jblt %l0", "fbnge %l0", "jbmi %l0");
#else
  OUTPUT_JUMP ("jlt %l0", "fjnge %l0", "jmi %l0");
#endif

}

static char *
output_275 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  return "jbcs %l0";
#else
  return "jcs %l0";
#endif

}

static char *
output_276 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  OUTPUT_JUMP ("jbgt %l0", "fbnle %l0", 0);
#else
  OUTPUT_JUMP ("jgt %l0", "fjnle %l0", 0);
#endif

}

static char *
output_277 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  return "jbhi %l0";
#else
  return "jhi %l0";
#endif

}

static char *
output_278 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  return "jbra %l0";
#else
  return "jra %l0";
#endif

}

static char *
output_280 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  return "jmp (%0)";
#else
  return "jmp %0@";
#endif

}

static char *
output_281 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef ASM_RETURN_CASE_JUMP
 ASM_RETURN_CASE_JUMP;
#else
#ifdef SGS
#ifdef ASM_OUTPUT_CASE_LABEL
  return "jmp 6(%%pc,%0.w)";
#else
#ifdef CRDS
  return "jmp 2(pc,%0.w)";
#else
  return "jmp 2(%%pc,%0.w)";
#endif  /* end !CRDS */
#endif
#else /* not SGS */
#ifdef MOTOROLA
  return "jmp (2,pc,%0.w)";
#else
  return "jmp pc@(2,%0:w)";
#endif
#endif
#endif

}

static char *
output_282 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
  if (DATA_REG_P (operands[0]))
    return "dbra %0,%l1";
  if (GET_CODE (operands[0]) == MEM)
    {
#ifdef MOTOROLA
#ifdef NO_ADDSUB_Q
      return "sub%.w %#1,%0\n\tjbcc %l1";
#else
      return "subq%.w %#1,%0\n\tjbcc %l1";
#endif
#else /* not MOTOROLA */
      return "subqw %#1,%0\n\tjcc %l1";
#endif
    }
#ifdef MOTOROLA
#ifdef SGS_CMP_ORDER
#ifdef NO_ADDSUB_Q
  return "sub%.w %#1,%0\n\tcmp%.w %0,%#-1\n\tjbne %l1";
#else
  return "subq%.w %#1,%0\n\tcmp%.w %0,%#-1\n\tjbne %l1";
#endif
#else /* not SGS_CMP_ORDER */
  return "subq%.w %#1,%0\n\tcmp%.w %#-1,%0\n\tjbne %l1";
#endif
#else /* not MOTOROLA */
  return "subqw %#1,%0\n\tcmpw %#-1,%0\n\tjne %l1";
#endif
}
}

static char *
output_283 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
#ifdef MOTOROLA
#ifdef NO_ADDSUB_Q
  if (DATA_REG_P (operands[0]))
    return "dbra %0,%l1\n\tclr%.w %0\n\tsub%.l %#1,%0\n\tjbcc %l1";
  if (GET_CODE (operands[0]) == MEM)
    return "sub%.l %#1,%0\n\tjbcc %l1";
#else
  if (DATA_REG_P (operands[0]))
    return "dbra %0,%l1\n\tclr%.w %0\n\tsubq%.l %#1,%0\n\tjbcc %l1";
  if (GET_CODE (operands[0]) == MEM)
    return "subq%.l %#1,%0\n\tjbcc %l1";
#endif /* NO_ADDSUB_Q */
#ifdef SGS_CMP_ORDER
#ifdef NO_ADDSUB_Q
  return "sub.l %#1,%0\n\tcmp.l %0,%#-1\n\tjbne %l1";
#else
  return "subq.l %#1,%0\n\tcmp.l %0,%#-1\n\tjbne %l1";
#endif
#else /* not SGS_CMP_ORDER */
  return "subq.l %#1,%0\n\tcmp.l %#-1,%0\n\tjbne %l1";
#endif /* not SGS_CMP_ORDER */
#else /* not MOTOROLA */
  if (DATA_REG_P (operands[0]))
    return "dbra %0,%l1\n\tclr%.w %0\n\tsubql %#1,%0\n\tjcc %l1";
  if (GET_CODE (operands[0]) == MEM)
    return "subql %#1,%0\n\tjcc %l1";
  return "subql %#1,%0\n\tcmpl %#-1,%0\n\tjne %l1";
#endif /* not MOTOROLA */
}
}

static char *
output_284 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
#ifdef MOTOROLA
#ifdef NO_ADDSUB_Q
  if (DATA_REG_P (operands[0]))
    return "dbra %0,%l1";
  if (GET_CODE (operands[0]) == MEM)
    return "sub%.w %#1,%0\n\tjbcc %l1";
#else
  if (DATA_REG_P (operands[0]))
    return "dbra %0,%l1";
  if (GET_CODE (operands[0]) == MEM)
    return "subq%.w %#1,%0\n\tjbcc %l1";
#endif
#ifdef SGS_CMP_ORDER
#ifdef NO_ADDSUB_Q
  return "sub.w %#1,%0\n\tcmp.w %0,%#-1\n\tjbne %l1";
#else
  return "subq.w %#1,%0\n\tcmp.w %0,%#-1\n\tjbne %l1";
#endif
#else /* not SGS_CMP_ORDER */
  return "subq.w %#1,%0\n\tcmp.w %#-1,%0\n\tjbne %l1";
#endif /* not SGS_CMP_ORDER */
#else /* not MOTOROLA */
  if (DATA_REG_P (operands[0]))
    return "dbra %0,%l1";
  if (GET_CODE (operands[0]) == MEM)
    return "subqw %#1,%0\n\tjcc %l1";
  return "subqw %#1,%0\n\tcmpw %#-1,%0\n\tjne %l1";
#endif /* not MOTOROLA */
}
}

static char *
output_285 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
#ifdef MOTOROLA
#ifdef NO_ADDSUB_Q
  if (DATA_REG_P (operands[0]))
    return "dbra %0,%l1\n\tclr%.w %0\n\tsub%.l %#1,%0\n\tjbcc %l1";
  if (GET_CODE (operands[0]) == MEM)
    return "sub%.l %#1,%0\n\tjbcc %l1";
#else
  if (DATA_REG_P (operands[0]))
    return "dbra %0,%l1\n\tclr%.w %0\n\tsubq%.l %#1,%0\n\tjbcc %l1";
  if (GET_CODE (operands[0]) == MEM)
    return "subq%.l %#1,%0\n\tjbcc %l1";
#endif
#ifdef SGS_CMP_ORDER
#ifdef NO_ADDSUB_Q
  return "sub.l %#1,%0\n\tcmp.l %0,%#-1\n\tjbne %l1";
#else
  return "subq.l %#1,%0\n\tcmp.l %0,%#-1\n\tjbne %l1";
#endif
#else /* not SGS_CMP_ORDER */
  return "subq.l %#1,%0\n\tcmp.l %#-1,%0\n\tjbne %l1";
#endif /* not SGS_CMP_ORDER */
#else /* not MOTOROLA */
  if (DATA_REG_P (operands[0]))
    return "dbra %0,%l1\n\tclr%.w %0\n\tsubql %#1,%0\n\tjcc %l1";
  if (GET_CODE (operands[0]) == MEM)
    return "subql %#1,%0\n\tjcc %l1";
  return "subql %#1,%0\n\tcmpl %#-1,%0\n\tjne %l1";
#endif /* not MOTOROLA */
}
}

static char *
output_287 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  return "jsr %0";
#else
  return "jbsr %0";
#endif

}

static char *
output_288 (operands, insn)
     rtx *operands;
     rtx insn;
{

  if (GET_CODE (operands[0]) == MEM 
      && GET_CODE (XEXP (operands[0], 0)) == SYMBOL_REF)
#ifdef MOTOROLA
    return "bsr %0@PLTPC";
#else
    return "jbsr %0,a1";
#endif
  return "jsr %0";

}

static char *
output_290 (operands, insn)
     rtx *operands;
     rtx insn;
{

#ifdef MOTOROLA
  return "jsr %1";
#else
  return "jbsr %1";
#endif

}

static char *
output_291 (operands, insn)
     rtx *operands;
     rtx insn;
{

  if (GET_CODE (operands[1]) == MEM 
      && GET_CODE (XEXP (operands[1], 0)) == SYMBOL_REF)
#ifdef MOTOROLA
    return "bsr %1@PLTPC";
#else
    return "jbsr %1,a1";
#endif
  return "jsr %1";

}

static char *
output_295 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  operands[0] = gen_rtx (PLUS, SImode, stack_pointer_rtx,
			 gen_rtx (CONST_INT, VOIDmode, NEED_PROBE));
  return "tstl %a0";
}
}

static char *
output_296 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (current_function_pops_args == 0)
    return "rts";
  operands[0] = gen_rtx (CONST_INT, VOIDmode, current_function_pops_args);
  return "rtd %0";
}
}

static char *
output_299 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx xoperands[2];
  xoperands[1] = gen_rtx (REG, SImode, REGNO (operands[1]) + 1);
  output_asm_insn ("move%.l %1,%@", xoperands);
  output_asm_insn ("move%.l %1,%-", operands);
  return "fmove%.d %+,%0";
}

}

static char *
output_300 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (INTVAL (operands[0]) > 4)
    {
      rtx xoperands[2];
      xoperands[0] = stack_pointer_rtx;
      xoperands[1] = gen_rtx (CONST_INT, VOIDmode, INTVAL (operands[0]) - 4);
#ifndef NO_ADDSUB_Q
      if (INTVAL (xoperands[1]) <= 8)
        output_asm_insn ("addq%.w %1,%0", xoperands);
      else if (INTVAL (xoperands[1]) <= 16 && TARGET_68020)
	{
	  xoperands[1] = gen_rtx (CONST_INT, VOIDmode, 
				  INTVAL (xoperands[1]) - 8);
	  output_asm_insn ("addq%.w %#8,%0\n\taddq%.w %1,%0", xoperands);
	}
      else
#endif
        if (INTVAL (xoperands[1]) <= 0x7FFF)
          output_asm_insn ("add%.w %1,%0", xoperands);
      else
        output_asm_insn ("add%.l %1,%0", xoperands);
    }
  if (FP_REG_P (operands[2]))
    return "fmove%.s %2,%@";
  return "move%.l %2,%@";
}
}

static char *
output_301 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (INTVAL (operands[0]) > 4)
    {
      rtx xoperands[2];
      xoperands[0] = stack_pointer_rtx;
      xoperands[1] = gen_rtx (CONST_INT, VOIDmode, INTVAL (operands[0]) - 4);
#ifndef NO_ADDSUB_Q
      if (INTVAL (xoperands[1]) <= 8)
        output_asm_insn ("addq%.w %1,%0", xoperands);
      else if (INTVAL (xoperands[1]) <= 16 && TARGET_68020)
	{
	  xoperands[1] = gen_rtx (CONST_INT, VOIDmode, 
				  INTVAL (xoperands[1]) - 8);
	  output_asm_insn ("addq%.w %#8,%0\n\taddq%.w %1,%0", xoperands);
	}
      else
#endif
        if (INTVAL (xoperands[1]) <= 0x7FFF)
          output_asm_insn ("add%.w %1,%0", xoperands);
      else
        output_asm_insn ("add%.l %1,%0", xoperands);
    }
  if (operands[2] == const0_rtx)
    return "clr%.l %@";
  return "move%.l %2,%@";
}
}

static char *
output_302 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx xoperands[4];

  if (GET_CODE (operands[1]) == REG)
    return "move%.l %1,%-";

  xoperands[1] = operands[1];
  xoperands[2]
    = gen_rtx (MEM, QImode,
	       gen_rtx (PLUS, VOIDmode, stack_pointer_rtx,
			gen_rtx (CONST_INT, VOIDmode, 3)));
  xoperands[3] = stack_pointer_rtx;
  output_asm_insn ("subq%.w %#4,%3\n\tmove%.b %1,%2", xoperands);
  return "";
}
}

static char *
output_303 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT)
    {
      if (operands[1] == const0_rtx
	  && (DATA_REG_P (operands[0])
	      || GET_CODE (operands[0]) == MEM)
	  /* clr insns on 68000 read before writing.
	     This isn't so on the 68010, but we have no alternative for it.  */
	  && (TARGET_68020
	      || !(GET_CODE (operands[0]) == MEM
		   && MEM_VOLATILE_P (operands[0]))))
	return "clr%.w %0";
    }
  return "move%.w %1,%0";
}
}

static char *
output_304 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
  output_dbcc_and_branch (operands);
  return "";
}
}

static char *
output_305 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
  output_dbcc_and_branch (operands);
  return "";
}
}

static char *
output_306 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_306[] = {
    "fpma%.d %1,%w2,%w3,%0",
    "fpma%.d %x1,%x2,%x3,%0",
    "fpma%.d %x1,%x2,%x3,%0",
  };
  return strings_306[which_alternative];
}

static char *
output_307 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_307[] = {
    "fpma%.s %1,%w2,%w3,%0",
    "fpma%.s %1,%2,%3,%0",
    "fpma%.s %1,%2,%3,%0",
  };
  return strings_307[which_alternative];
}

static char *
output_308 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_308[] = {
    "fpms%.d %3,%w2,%w1,%0",
    "fpms%.d %x3,%2,%x1,%0",
    "fpms%.d %x3,%2,%x1,%0",
  };
  return strings_308[which_alternative];
}

static char *
output_309 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_309[] = {
    "fpms%.s %3,%w2,%w1,%0",
    "fpms%.s %3,%2,%1,%0",
    "fpms%.s %3,%2,%1,%0",
  };
  return strings_309[which_alternative];
}

static char *
output_310 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_310[] = {
    "fpmr%.d %2,%w1,%w3,%0",
    "fpmr%.d %x2,%1,%x3,%0",
    "fpmr%.d %x2,%1,%x3,%0",
  };
  return strings_310[which_alternative];
}

static char *
output_311 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_311[] = {
    "fpmr%.s %2,%w1,%w3,%0",
    "fpmr%.s %x2,%1,%x3,%0",
    "fpmr%.s %x2,%1,%x3,%0",
  };
  return strings_311[which_alternative];
}

static char *
output_312 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_312[] = {
    "fpam%.d %2,%w1,%w3,%0",
    "fpam%.d %x2,%1,%x3,%0",
    "fpam%.d %x2,%1,%x3,%0",
  };
  return strings_312[which_alternative];
}

static char *
output_313 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_313[] = {
    "fpam%.s %2,%w1,%w3,%0",
    "fpam%.s %x2,%1,%x3,%0",
    "fpam%.s %x2,%1,%x3,%0",
  };
  return strings_313[which_alternative];
}

static char *
output_314 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_314[] = {
    "fpsm%.d %2,%w1,%w3,%0",
    "fpsm%.d %x2,%1,%x3,%0",
    "fpsm%.d %x2,%1,%x3,%0",
  };
  return strings_314[which_alternative];
}

static char *
output_315 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_315[] = {
    "fpsm%.d %3,%w2,%w1,%0",
    "fpsm%.d %x3,%2,%x1,%0",
    "fpsm%.d %x3,%2,%x1,%0",
  };
  return strings_315[which_alternative];
}

static char *
output_316 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_316[] = {
    "fpsm%.s %2,%w1,%w3,%0",
    "fpsm%.s %x2,%1,%x3,%0",
    "fpsm%.s %x2,%1,%x3,%0",
  };
  return strings_316[which_alternative];
}

static char *
output_317 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_317[] = {
    "fpsm%.s %3,%w2,%w1,%0",
    "fpsm%.s %x3,%2,%x1,%0",
    "fpsm%.s %x3,%2,%x1,%0",
  };
  return strings_317[which_alternative];
}

static char *
output_318 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  cc_status.flags = CC_IN_68881;
  return "ftst%.x %0";
}
}

static char *
output_320 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  cc_status.flags = CC_IN_68881;
#ifdef SGS_CMP_ORDER
  if (REG_P (operands[0]))
    {
      if (REG_P (operands[1]))
	return "fcmp%.x %0,%1";
      else
        return "fcmp%.x %0,%f1";
    }
  cc_status.flags |= CC_REVERSED;
  return "fcmp%.x %1,%f0";
#else
  if (REG_P (operands[0]))
    {
      if (REG_P (operands[1]))
	return "fcmp%.x %1,%0";
      else
        return "fcmp%.x %f1,%0";
    }
  cc_status.flags |= CC_REVERSED;
  return "fcmp%.x %f0,%1";
#endif
}
}

static char *
output_321 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[0]) && FP_REG_P (operands[1]))
    {
      if (REGNO (operands[0]) == REGNO (operands[1]))
	{
	  /* Extending float to double in an fp-reg is a no-op.
	     NOTICE_UPDATE_CC has already assumed that the
	     cc will be set.  So cancel what it did.  */
	  cc_status = cc_prev_status;
	  return "";
	}
      return "f%$move%.x %1,%0";
    }
  if (FP_REG_P (operands[0]))
    return "f%$move%.s %f1,%0";
  return "fmove%.x %f1,%0";
}
}

static char *
output_322 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[0]) && FP_REG_P (operands[1]))
    {
      if (REGNO (operands[0]) == REGNO (operands[1]))
	{
	  /* Extending float to double in an fp-reg is a no-op.
	     NOTICE_UPDATE_CC has already assumed that the
	     cc will be set.  So cancel what it did.  */
	  cc_status = cc_prev_status;
	  return "";
	}
      return "fmove%.x %1,%0";
    }
  if (FP_REG_P (operands[0]))
    return "f%&move%.d %f1,%0";
  return "fmove%.x %f1,%0";
}
}

static char *
output_323 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[0]))
    {
      output_asm_insn ("fmove%.d %f1,%-\n\tmove%.l %+,%0", operands);
      operands[0] = gen_rtx (REG, SImode, REGNO (operands[0]) + 1);
      return "move%.l %+,%0";
    }
  return "fmove%.d %f1,%0";
}
}

static char *
output_328 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[1]))
    return "fintrz%.x %f1,%0";
  return "fintrz%.x %f1,%0";
}
}

static char *
output_333 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return "fadd%.x %2,%0";
  return "fadd%.x %f2,%0";
}
}

static char *
output_335 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return "fsub%.x %2,%0";
  return "fsub%.x %f2,%0";
}
}

static char *
output_337 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return "fmul%.x %2,%0";
  return "fmul%.x %f2,%0";
}
}

static char *
output_339 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return "fdiv%.x %2,%0";
  return "fdiv%.x %f2,%0";
}
}

static char *
output_340 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[1]) && ! DATA_REG_P (operands[1]))
    return "fneg%.x %1,%0";
  return "fneg%.x %f1,%0";
}
}

static char *
output_341 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[1]) && ! DATA_REG_P (operands[1]))
    return "fabs%.x %1,%0";
  return "fabs%.x %f1,%0";
}
}

static char *
output_342 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
    return "fsqrt%.x %1,%0";
}
}

char * const insn_template[] =
  {
    0,
    0,
    0,
    "tst%.w %0",
    "tst%.b %0",
    0,
    "fptst%.s %x0\n\tfpmove fpastatus,%1\n\tmovw %1,cc",
    0,
    0,
    "fptst%.d %x0\n\tfpmove fpastatus,%1\n\tmovw %1,cc",
    0,
    0,
    0,
    0,
    0,
    "fpcmp%.d %y1,%0\n\tfpmove fpastatus,%2\n\tmovw %2,cc",
    0,
    0,
    "fpcmp%.s %w1,%x0\n\tfpmove fpastatus,%2\n\tmovw %2,cc",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "pea %a1",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "ext%.w %0",
    "extb%.l %0",
    0,
    "fpstod %w1,%0",
    0,
    0,
    "fpdtos %y1,%0",
    0,
    "fmove%.s %f1,%0",
    0,
    "fpltos %1,%0",
    "f%$move%.l %1,%0",
    0,
    "fpltod %1,%0",
    "f%&move%.l %1,%0",
    "f%$move%.w %1,%0",
    "fmove%.w %1,%0",
    "fmove%.b %1,%0",
    "f%&move%.b %1,%0",
    0,
    0,
    0,
    0,
    0,
    "fmove%.b %1,%0",
    "fmove%.w %1,%0",
    "fmove%.l %1,%0",
    "fmove%.b %1,%0",
    "fmove%.w %1,%0",
    "fmove%.l %1,%0",
    "fpstol %w1,%0",
    "fpdtol %y1,%0",
    0,
    "add%.w %2,%0",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "sub%.w %2,%0",
    "sub%.w %2,%0",
    "sub%.w %1,%0",
    "sub%.b %2,%0",
    "sub%.b %1,%0",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "muls%.l %2,%0",
    0,
    0,
    0,
    "mulu%.l %2,%3:%0",
    "mulu%.l %2,%3:%0",
    0,
    "muls%.l %2,%3:%0",
    "muls%.l %2,%3:%0",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "and%.w %2,%0",
    "and%.w %1,%0",
    "and%.w %1,%0",
    "and%.b %2,%0",
    "and%.b %1,%0",
    "and%.b %1,%0",
    0,
    "or%.w %2,%0",
    "or%.w %1,%0",
    "or%.w %1,%0",
    "or%.b %2,%0",
    "or%.b %1,%0",
    "or%.b %1,%0",
    0,
    "eor%.w %2,%0",
    "eor%.w %1,%0",
    "eor%.w %1,%0",
    "eor%.b %2,%0",
    "eor%.b %1,%0",
    "eor%.b %1,%0",
    "neg%.l %0",
    "neg%.w %0",
    "neg%.w %0",
    "neg%.b %0",
    "neg%.b %0",
    0,
    "fpneg%.s %w1,%0",
    0,
    0,
    "fpneg%.d %y1, %0",
    0,
    0,
    0,
    "fpabs%.s %y1,%0",
    0,
    0,
    "fpabs%.d %y1,%0",
    0,
    "not%.l %0",
    "not%.w %0",
    "not%.w %0",
    "not%.b %0",
    "not%.b %0",
    0,
    0,
    0,
    "asl%.w %2,%0",
    "asl%.w %1,%0",
    "asl%.b %2,%0",
    "asl%.b %1,%0",
    "swap %0\n\text%.l %0",
    0,
    0,
    "asr%.w %2,%0",
    "asr%.w %1,%0",
    "asr%.b %2,%0",
    "asr%.b %1,%0",
    0,
    0,
    0,
    "lsl%.w %2,%0",
    "lsl%.w %1,%0",
    "lsl%.b %2,%0",
    "lsl%.b %1,%0",
    0,
    0,
    0,
    "lsr%.w %2,%0",
    "lsr%.w %1,%0",
    "lsr%.b %2,%0",
    "lsr%.b %1,%0",
    "rol%.l %2,%0",
    "rol%.w %2,%0",
    "rol%.w %1,%0",
    "rol%.b %2,%0",
    "rol%.b %1,%0",
    "ror%.l %2,%0",
    "ror%.w %2,%0",
    "ror%.w %1,%0",
    "ror%.b %2,%0",
    "ror%.b %1,%0",
    0,
    0,
    0,
    0,
    0,
    0,
    "bfexts %1{%b3:%b2},%0",
    0,
    0,
    0,
    0,
    "bfins %3,%0{%b2:%b1}",
    "bfexts %1{%b3:%b2},%0",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "",
    "nop",
    0,
    0,
    "jmp %a0",
    "lea %a1,%0",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "fmove%.s %f1,%0",
    "fmove%.l %1,%0",
    "fmove%.w %1,%0",
    "fmove%.b %1,%0",
    0,
    "fmove%.b %1,%0",
    "fmove%.w %1,%0",
    "fmove%.l %1,%0",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
  };

char *(*const insn_outfun[])() =
  {
    output_0,
    output_1,
    output_2,
    0,
    0,
    0,
    0,
    output_7,
    0,
    0,
    output_10,
    output_11,
    output_12,
    output_13,
    0,
    0,
    output_16,
    0,
    0,
    output_19,
    output_20,
    output_21,
    output_22,
    output_23,
    output_24,
    output_25,
    output_26,
    output_27,
    0,
    output_29,
    output_30,
    output_31,
    output_32,
    output_33,
    output_34,
    output_35,
    0,
    output_37,
    output_38,
    output_39,
    0,
    output_41,
    output_42,
    output_43,
    0,
    0,
    0,
    output_47,
    output_48,
    output_49,
    output_50,
    0,
    0,
    0,
    0,
    output_55,
    0,
    0,
    output_58,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_70,
    output_71,
    output_72,
    output_73,
    output_74,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_83,
    0,
    output_85,
    output_86,
    output_87,
    output_88,
    output_89,
    output_90,
    0,
    output_92,
    output_93,
    0,
    output_95,
    output_96,
    output_97,
    0,
    0,
    0,
    0,
    0,
    0,
    output_104,
    output_105,
    0,
    output_107,
    output_108,
    output_109,
    output_110,
    output_111,
    0,
    output_113,
    output_114,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_122,
    output_123,
    0,
    output_125,
    output_126,
    output_127,
    output_128,
    output_129,
    output_130,
    output_131,
    output_132,
    0,
    output_134,
    output_135,
    0,
    output_137,
    output_138,
    output_139,
    output_140,
    output_141,
    output_142,
    output_143,
    output_144,
    output_145,
    output_146,
    output_147,
    0,
    0,
    0,
    0,
    0,
    0,
    output_154,
    0,
    0,
    0,
    0,
    0,
    0,
    output_161,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_175,
    0,
    0,
    output_178,
    output_179,
    0,
    0,
    output_182,
    0,
    0,
    output_185,
    0,
    0,
    0,
    0,
    0,
    output_191,
    output_192,
    output_193,
    0,
    0,
    0,
    0,
    0,
    output_199,
    output_200,
    0,
    0,
    0,
    0,
    output_205,
    output_206,
    output_207,
    0,
    0,
    0,
    0,
    output_212,
    output_213,
    output_214,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_229,
    output_230,
    output_231,
    output_232,
    output_233,
    output_234,
    0,
    output_236,
    output_237,
    output_238,
    output_239,
    0,
    0,
    output_242,
    output_243,
    output_244,
    output_245,
    output_246,
    output_247,
    output_248,
    output_249,
    output_250,
    output_251,
    output_252,
    output_253,
    output_254,
    output_255,
    output_256,
    output_257,
    output_258,
    output_259,
    output_260,
    output_261,
    output_262,
    output_263,
    output_264,
    output_265,
    output_266,
    output_267,
    output_268,
    output_269,
    output_270,
    output_271,
    output_272,
    output_273,
    output_274,
    output_275,
    output_276,
    output_277,
    output_278,
    0,
    output_280,
    output_281,
    output_282,
    output_283,
    output_284,
    output_285,
    0,
    output_287,
    output_288,
    0,
    output_290,
    output_291,
    0,
    0,
    0,
    output_295,
    output_296,
    0,
    0,
    output_299,
    output_300,
    output_301,
    output_302,
    output_303,
    output_304,
    output_305,
    output_306,
    output_307,
    output_308,
    output_309,
    output_310,
    output_311,
    output_312,
    output_313,
    output_314,
    output_315,
    output_316,
    output_317,
    output_318,
    0,
    output_320,
    output_321,
    output_322,
    output_323,
    0,
    0,
    0,
    0,
    output_328,
    0,
    0,
    0,
    0,
    output_333,
    0,
    output_335,
    0,
    output_337,
    0,
    output_339,
    output_340,
    output_341,
    output_342,
  };

rtx (*const insn_gen_function[]) () =
  {
    0,
    0,
    gen_tstsi,
    gen_tsthi,
    gen_tstqi,
    gen_tstsf,
    gen_tstsf_fpa,
    0,
    gen_tstdf,
    gen_tstdf_fpa,
    0,
    gen_cmpsi,
    gen_cmphi,
    gen_cmpqi,
    gen_cmpdf,
    gen_cmpdf_fpa,
    0,
    gen_cmpsf,
    gen_cmpsf_fpa,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    gen_movsi,
    0,
    gen_movhi,
    gen_movstricthi,
    gen_movqi,
    gen_movstrictqi,
    gen_movsf,
    gen_movdf,
    gen_movxf,
    0,
    0,
    gen_movdi,
    gen_pushasi,
    gen_truncsiqi2,
    gen_trunchiqi2,
    gen_truncsihi2,
    gen_zero_extendhisi2,
    gen_zero_extendqihi2,
    gen_zero_extendqisi2,
    0,
    0,
    0,
    gen_extendhisi2,
    gen_extendqihi2,
    gen_extendqisi2,
    gen_extendsfdf2,
    0,
    0,
    gen_truncdfsf2,
    0,
    0,
    0,
    gen_floatsisf2,
    0,
    0,
    gen_floatsidf2,
    0,
    0,
    gen_floathisf2,
    gen_floathidf2,
    gen_floatqisf2,
    gen_floatqidf2,
    gen_fix_truncdfsi2,
    gen_fix_truncdfhi2,
    gen_fix_truncdfqi2,
    gen_ftruncdf2,
    gen_ftruncsf2,
    gen_fixsfqi2,
    gen_fixsfhi2,
    gen_fixsfsi2,
    gen_fixdfqi2,
    gen_fixdfhi2,
    gen_fixdfsi2,
    0,
    0,
    gen_addsi3,
    0,
    gen_addhi3,
    0,
    0,
    gen_addqi3,
    0,
    0,
    gen_adddf3,
    0,
    0,
    gen_addsf3,
    0,
    0,
    gen_subsi3,
    0,
    gen_subhi3,
    0,
    gen_subqi3,
    0,
    gen_subdf3,
    0,
    0,
    gen_subsf3,
    0,
    0,
    gen_mulhi3,
    gen_mulhisi3,
    0,
    gen_mulsi3,
    gen_umulhisi3,
    0,
    gen_umulsidi3,
    0,
    0,
    gen_mulsidi3,
    0,
    0,
    gen_muldf3,
    0,
    0,
    gen_mulsf3,
    0,
    0,
    gen_divhi3,
    gen_divhisi3,
    0,
    gen_udivhi3,
    gen_udivhisi3,
    0,
    gen_divdf3,
    0,
    0,
    gen_divsf3,
    0,
    0,
    gen_modhi3,
    gen_modhisi3,
    0,
    gen_umodhi3,
    gen_umodhisi3,
    0,
    gen_divmodsi4,
    gen_udivmodsi4,
    gen_andsi3,
    gen_andhi3,
    0,
    0,
    gen_andqi3,
    0,
    0,
    gen_iorsi3,
    gen_iorhi3,
    0,
    0,
    gen_iorqi3,
    0,
    0,
    gen_xorsi3,
    gen_xorhi3,
    0,
    0,
    gen_xorqi3,
    0,
    0,
    gen_negsi2,
    gen_neghi2,
    0,
    gen_negqi2,
    0,
    gen_negsf2,
    0,
    0,
    gen_negdf2,
    0,
    0,
    gen_sqrtdf2,
    gen_abssf2,
    0,
    0,
    gen_absdf2,
    0,
    0,
    gen_one_cmplsi2,
    gen_one_cmplhi2,
    0,
    gen_one_cmplqi2,
    0,
    0,
    0,
    gen_ashlsi3,
    gen_ashlhi3,
    0,
    gen_ashlqi3,
    0,
    0,
    0,
    gen_ashrsi3,
    gen_ashrhi3,
    0,
    gen_ashrqi3,
    0,
    0,
    0,
    gen_lshlsi3,
    gen_lshlhi3,
    0,
    gen_lshlqi3,
    0,
    0,
    0,
    gen_lshrsi3,
    gen_lshrhi3,
    0,
    gen_lshrqi3,
    0,
    gen_rotlsi3,
    gen_rotlhi3,
    0,
    gen_rotlqi3,
    0,
    gen_rotrsi3,
    gen_rotrhi3,
    0,
    gen_rotrqi3,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    gen_extv,
    gen_extzv,
    0,
    0,
    0,
    gen_insv,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    gen_seq,
    gen_sne,
    gen_sgt,
    gen_sgtu,
    gen_slt,
    gen_sltu,
    gen_sge,
    gen_sgeu,
    gen_sle,
    gen_sleu,
    gen_beq,
    gen_bne,
    gen_bgt,
    gen_bgtu,
    gen_blt,
    gen_bltu,
    gen_bge,
    gen_bgeu,
    gen_ble,
    gen_bleu,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    gen_jump,
    gen_tablejump,
    0,
    0,
    0,
    0,
    0,
    gen_decrement_and_branch_until_zero,
    gen_call,
    0,
    0,
    gen_call_value,
    0,
    0,
    gen_untyped_call,
    gen_blockage,
    gen_nop,
    gen_probe,
    gen_return,
    gen_indirect_jump,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    gen_tstxf,
    gen_cmpxf,
    0,
    gen_extendsfxf2,
    gen_extenddfxf2,
    gen_truncxfdf2,
    gen_truncxfsf2,
    gen_floatsixf2,
    gen_floathixf2,
    gen_floatqixf2,
    gen_ftruncxf2,
    gen_fixxfqi2,
    gen_fixxfhi2,
    gen_fixxfsi2,
    gen_addxf3,
    0,
    gen_subxf3,
    0,
    gen_mulxf3,
    0,
    gen_divxf3,
    0,
    gen_negxf2,
    gen_absxf2,
    gen_sqrtxf2,
  };

char *insn_name[] =
  {
    "tstsi-1",
    "tstsi-0",
    "tstsi",
    "tsthi",
    "tstqi",
    "tstsf",
    "tstsf_fpa",
    "tstsf_fpa+1",
    "tstdf",
    "tstdf_fpa",
    "tstdf_fpa+1",
    "cmpsi",
    "cmphi",
    "cmpqi",
    "cmpdf",
    "cmpdf_fpa",
    "cmpdf_fpa+1",
    "cmpsf",
    "cmpsf_fpa",
    "cmpsf_fpa+1",
    "cmpsf_fpa+2",
    "cmpsf_fpa+3",
    "cmpsf_fpa+4",
    "cmpsf_fpa+5",
    "movsi-4",
    "movsi-3",
    "movsi-2",
    "movsi-1",
    "movsi",
    "movsi+1",
    "movhi",
    "movstricthi",
    "movqi",
    "movstrictqi",
    "movsf",
    "movdf",
    "movxf",
    "movxf+1",
    "movdi-1",
    "movdi",
    "pushasi",
    "truncsiqi2",
    "trunchiqi2",
    "truncsihi2",
    "zero_extendhisi2",
    "zero_extendqihi2",
    "zero_extendqisi2",
    "zero_extendqisi2+1",
    "zero_extendqisi2+2",
    "extendhisi2-1",
    "extendhisi2",
    "extendqihi2",
    "extendqisi2",
    "extendsfdf2",
    "extendsfdf2+1",
    "truncdfsf2-1",
    "truncdfsf2",
    "truncdfsf2+1",
    "truncdfsf2+2",
    "floatsisf2-1",
    "floatsisf2",
    "floatsisf2+1",
    "floatsidf2-1",
    "floatsidf2",
    "floatsidf2+1",
    "floathisf2-1",
    "floathisf2",
    "floathidf2",
    "floatqisf2",
    "floatqidf2",
    "fix_truncdfsi2",
    "fix_truncdfhi2",
    "fix_truncdfqi2",
    "ftruncdf2",
    "ftruncsf2",
    "fixsfqi2",
    "fixsfhi2",
    "fixsfsi2",
    "fixdfqi2",
    "fixdfhi2",
    "fixdfsi2",
    "fixdfsi2+1",
    "addsi3-1",
    "addsi3",
    "addsi3+1",
    "addhi3",
    "addhi3+1",
    "addqi3-1",
    "addqi3",
    "addqi3+1",
    "adddf3-1",
    "adddf3",
    "adddf3+1",
    "addsf3-1",
    "addsf3",
    "addsf3+1",
    "subsi3-1",
    "subsi3",
    "subsi3+1",
    "subhi3",
    "subhi3+1",
    "subqi3",
    "subqi3+1",
    "subdf3",
    "subdf3+1",
    "subsf3-1",
    "subsf3",
    "subsf3+1",
    "mulhi3-1",
    "mulhi3",
    "mulhisi3",
    "mulhisi3+1",
    "mulsi3",
    "umulhisi3",
    "umulhisi3+1",
    "umulsidi3",
    "umulsidi3+1",
    "mulsidi3-1",
    "mulsidi3",
    "mulsidi3+1",
    "muldf3-1",
    "muldf3",
    "muldf3+1",
    "mulsf3-1",
    "mulsf3",
    "mulsf3+1",
    "divhi3-1",
    "divhi3",
    "divhisi3",
    "divhisi3+1",
    "udivhi3",
    "udivhisi3",
    "udivhisi3+1",
    "divdf3",
    "divdf3+1",
    "divsf3-1",
    "divsf3",
    "divsf3+1",
    "modhi3-1",
    "modhi3",
    "modhisi3",
    "modhisi3+1",
    "umodhi3",
    "umodhisi3",
    "umodhisi3+1",
    "divmodsi4",
    "udivmodsi4",
    "andsi3",
    "andhi3",
    "andhi3+1",
    "andqi3-1",
    "andqi3",
    "andqi3+1",
    "iorsi3-1",
    "iorsi3",
    "iorhi3",
    "iorhi3+1",
    "iorqi3-1",
    "iorqi3",
    "iorqi3+1",
    "xorsi3-1",
    "xorsi3",
    "xorhi3",
    "xorhi3+1",
    "xorqi3-1",
    "xorqi3",
    "xorqi3+1",
    "negsi2-1",
    "negsi2",
    "neghi2",
    "neghi2+1",
    "negqi2",
    "negqi2+1",
    "negsf2",
    "negsf2+1",
    "negdf2-1",
    "negdf2",
    "negdf2+1",
    "sqrtdf2-1",
    "sqrtdf2",
    "abssf2",
    "abssf2+1",
    "absdf2-1",
    "absdf2",
    "absdf2+1",
    "one_cmplsi2-1",
    "one_cmplsi2",
    "one_cmplhi2",
    "one_cmplhi2+1",
    "one_cmplqi2",
    "one_cmplqi2+1",
    "one_cmplqi2+2",
    "ashlsi3-1",
    "ashlsi3",
    "ashlhi3",
    "ashlhi3+1",
    "ashlqi3",
    "ashlqi3+1",
    "ashlqi3+2",
    "ashrsi3-1",
    "ashrsi3",
    "ashrhi3",
    "ashrhi3+1",
    "ashrqi3",
    "ashrqi3+1",
    "ashrqi3+2",
    "lshlsi3-1",
    "lshlsi3",
    "lshlhi3",
    "lshlhi3+1",
    "lshlqi3",
    "lshlqi3+1",
    "lshlqi3+2",
    "lshrsi3-1",
    "lshrsi3",
    "lshrhi3",
    "lshrhi3+1",
    "lshrqi3",
    "lshrqi3+1",
    "rotlsi3",
    "rotlhi3",
    "rotlhi3+1",
    "rotlqi3",
    "rotlqi3+1",
    "rotrsi3",
    "rotrhi3",
    "rotrhi3+1",
    "rotrqi3",
    "rotrqi3+1",
    "rotrqi3+2",
    "rotrqi3+3",
    "rotrqi3+4",
    "extv-3",
    "extv-2",
    "extv-1",
    "extv",
    "extzv",
    "extzv+1",
    "extzv+2",
    "insv-1",
    "insv",
    "insv+1",
    "insv+2",
    "insv+3",
    "insv+4",
    "seq-3",
    "seq-2",
    "seq-1",
    "seq",
    "sne",
    "sgt",
    "sgtu",
    "slt",
    "sltu",
    "sge",
    "sgeu",
    "sle",
    "sleu",
    "beq",
    "bne",
    "bgt",
    "bgtu",
    "blt",
    "bltu",
    "bge",
    "bgeu",
    "ble",
    "bleu",
    "bleu+1",
    "bleu+2",
    "bleu+3",
    "bleu+4",
    "bleu+5",
    "jump-5",
    "jump-4",
    "jump-3",
    "jump-2",
    "jump-1",
    "jump",
    "tablejump",
    "tablejump+1",
    "tablejump+2",
    "tablejump+3",
    "decrement_and_branch_until_zero-2",
    "decrement_and_branch_until_zero-1",
    "decrement_and_branch_until_zero",
    "call",
    "call+1",
    "call_value-1",
    "call_value",
    "call_value+1",
    "untyped_call-1",
    "untyped_call",
    "blockage",
    "nop",
    "probe",
    "return",
    "indirect_jump",
    "indirect_jump+1",
    "indirect_jump+2",
    "indirect_jump+3",
    "indirect_jump+4",
    "indirect_jump+5",
    "indirect_jump+6",
    "indirect_jump+7",
    "indirect_jump+8",
    "indirect_jump+9",
    "indirect_jump+10",
    "tstxf-10",
    "tstxf-9",
    "tstxf-8",
    "tstxf-7",
    "tstxf-6",
    "tstxf-5",
    "tstxf-4",
    "tstxf-3",
    "tstxf-2",
    "tstxf-1",
    "tstxf",
    "cmpxf",
    "cmpxf+1",
    "extendsfxf2",
    "extenddfxf2",
    "truncxfdf2",
    "truncxfsf2",
    "floatsixf2",
    "floathixf2",
    "floatqixf2",
    "ftruncxf2",
    "fixxfqi2",
    "fixxfhi2",
    "fixxfsi2",
    "addxf3",
    "addxf3+1",
    "subxf3",
    "subxf3+1",
    "mulxf3",
    "mulxf3+1",
    "divxf3",
    "divxf3+1",
    "negxf2",
    "absxf2",
    "sqrtxf2",
  };
char **insn_name_ptr = insn_name;

const int insn_n_operands[] =
  {
    2,
    2,
    1,
    1,
    1,
    1,
    2,
    1,
    1,
    2,
    1,
    2,
    2,
    2,
    2,
    3,
    2,
    2,
    3,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    4,
    4,
    4,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    3,
    3,
    3,
    2,
    2,
    3,
    2,
    2,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    2,
    3,
    2,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    4,
    4,
    3,
    4,
    4,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    4,
    4,
    3,
    3,
    2,
    2,
    3,
    2,
    2,
    3,
    3,
    2,
    2,
    3,
    2,
    2,
    3,
    3,
    2,
    2,
    3,
    2,
    2,
    2,
    2,
    1,
    2,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    1,
    2,
    1,
    3,
    3,
    3,
    3,
    2,
    3,
    2,
    3,
    3,
    3,
    3,
    2,
    3,
    2,
    3,
    3,
    3,
    3,
    2,
    3,
    2,
    3,
    3,
    3,
    3,
    2,
    3,
    2,
    3,
    3,
    2,
    3,
    2,
    3,
    3,
    2,
    3,
    2,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    3,
    3,
    4,
    4,
    4,
    3,
    3,
    4,
    3,
    3,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    3,
    3,
    3,
    3,
    0,
    0,
    0,
    0,
    1,
    2,
    2,
    3,
    3,
    2,
    2,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    2,
    2,
    2,
  };

const int insn_n_dups[] =
  {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    2,
    2,
    3,
    2,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    2,
    0,
    0,
    1,
    1,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    1,
    1,
    0,
    0,
    1,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    0,
    1,
    0,
    1,
    0,
    0,
    1,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    2,
    2,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
  };

char *const insn_operand_constraint[][MAX_RECOG_OPERANDS] =
  {
    { "=m", "ro<>fyE", },
    { "=m", "ro<>Fy", },
    { "rm", },
    { "dm", },
    { "dm", },
    { "", },
    { "xmdF", "=d", },
    { "fdm", },
    { "", },
    { "xrmF", "=d", },
    { "fm", },
    { "rKs,mr,>", "mr,Ksr,>", },
    { "rnm,d,n,m", "d,rnm,m,n", },
    { "dn,md,>", "dm,nd,>", },
    { "", "", },
    { "x,y", "xH,rmF", "=d,d", },
    { "f,mG", "fmG,f", },
    { "", "", },
    { "x,y", "xH,rmF", "=d,d", },
    { "f,mdG", "fmdG,f", },
    { "do", "di", },
    { "d", "di", },
    { "do", "d", },
    { "d", "d", },
    { "md", "i", },
    { "do", "i", },
    { "=m", "J", },
    { "=g", },
    { "", "", },
    { "=g,da,y,!*x*r*m", "daymKs,i,g,*x*r*m", },
    { "=g", "g", },
    { "+dm", "rmn", },
    { "=d,*a,m,m,?*a", "dmi*a,d*a,dmi,?*a,m", },
    { "+dm", "dmn", },
    { "=rmf,x,y,rm,!x,!rm", "rmfF,xH,rmF,y,rm,x", },
    { "=rm,&rf,&rof<>,y,rm,x,!x,!rm", "rf,m,rofE<>,rmE,y,xH,rm,x", },
    { "", "", },
    { "=f,m,f,!r,!f", "m,f,f,f,r", },
    { "=rm,&rf,&rof<>", "rf,m,rof<>", },
    { "=rm,&r,&ro<>,y,rm,!*x,!rm", "rF,m,roi<>F,rmiF,y,rmF,*x", },
    { "=m", "p", },
    { "=dm,d", "doJ,i", },
    { "=dm,d", "doJ,i", },
    { "=dm,d", "roJ,i", },
    { "", "", },
    { "", "", },
    { "", "", },
    { "=do<>,d<", "r,m", },
    { "=do<>,d", "d,m", },
    { "=do<>,d", "d,m", },
    { "=*d,a", "0,rm", },
    { "=d", "0", },
    { "=d", "0", },
    { "", "", },
    { "=x,y", "xH,rmF", },
    { "=*fdm,f", "f,dmF", },
    { "", "", },
    { "=x,y", "xH,rmF", },
    { "=f", "fmG", },
    { "=dm", "f", },
    { "", "", },
    { "=y,x", "rmi,x", },
    { "=f", "dmi", },
    { "", "", },
    { "=y,x", "rmi,x", },
    { "=f", "dmi", },
    { "=f", "dmn", },
    { "=f", "dmn", },
    { "=f", "dmn", },
    { "=f", "dmn", },
    { "=dm", "f", "=d", "=d", },
    { "=dm", "f", "=d", "=d", },
    { "=dm", "f", "=d", "=d", },
    { "=f", "fFm", },
    { "=f", "dfFm", },
    { "=dm", "f", },
    { "=dm", "f", },
    { "=dm", "f", },
    { "=dm", "f", },
    { "=dm", "f", },
    { "=dm", "f", },
    { "=x,y", "xH,rmF", },
    { "=x,y", "xH,rmF", },
    { "=m,?a,?a,r", "%0,a,rJK,0", "dIKLs,rJK,a,mrIKLs", },
    { "=a", "0", "rm", },
    { "=m,r", "%0,0", "dn,rmn", },
    { "+m,d", "dn,rmn", },
    { "+m,d", "dn,rmn", },
    { "=m,d", "%0,0", "dn,dmn", },
    { "+m,d", "dn,dmn", },
    { "+m,d", "dn,dmn", },
    { "", "", "", },
    { "=x,y", "%xH,y", "xH,dmF", },
    { "=f", "%0", "fmG", },
    { "", "", "", },
    { "=x,y", "%xH,y", "xH,rmF", },
    { "=f", "%0", "fdmF", },
    { "=m,r,!a,?d", "0,0,a,mrIKs", "dIKs,mrIKs,J,0", },
    { "=a", "0", "rm", },
    { "=m,r", "0,0", "dn,rmn", },
    { "+m,d", "dn,rmn", },
    { "=m,d", "0,0", "dn,dmn", },
    { "+m,d", "dn,dmn", },
    { "", "", "", },
    { "=x,y,y", "xH,y,dmF", "xH,dmF,0", },
    { "=f", "0", "fmG", },
    { "", "", "", },
    { "=x,y,y", "xH,y,rmF", "xH,rmF,0", },
    { "=f", "0", "fdmF", },
    { "=d", "%0", "dmn", },
    { "=d", "%0", "dm", },
    { "=d", "%0", "n", },
    { "=d", "%0", "dmsK", },
    { "=d", "%0", "dm", },
    { "=d", "%0", "n", },
    { "", "", "", },
    { "=d", "%0", "dm", "=d", },
    { "=d", "%0", "n", "=d", },
    { "", "", "", },
    { "=d", "%0", "dm", "=d", },
    { "=d", "%0", "n", "=d", },
    { "", "", "", },
    { "=x,y", "%xH,y", "xH,rmF", },
    { "=f", "%0", "fmG", },
    { "", "", "", },
    { "=x,y", "%xH,y", "xH,rmF", },
    { "=f", "%0", "fdmF", },
    { "=d", "0", "dmn", },
    { "=d", "0", "dm", },
    { "=d", "0", "n", },
    { "=d", "0", "dmn", },
    { "=d", "0", "dm", },
    { "=d", "0", "n", },
    { "", "", "", },
    { "=x,y,y", "xH,y,rmF", "xH,rmF,0", },
    { "=f", "0", "fmG", },
    { "", "", "", },
    { "=x,y,y", "xH,y,rmF", "xH,rmF,0", },
    { "=f", "0", "fdmF", },
    { "=d", "0", "dmn", },
    { "=d", "0", "dm", },
    { "=d", "0", "n", },
    { "=d", "0", "dmn", },
    { "=d", "0", "dm", },
    { "=d", "0", "n", },
    { "=d", "0", "dmsK", "=d", },
    { "=d", "0", "dmsK", "=d", },
    { "=m,d", "%0,0", "dKs,dmKs", },
    { "=m,d", "%0,0", "dn,dmn", },
    { "+m,d", "dn,dmn", },
    { "+m,d", "dn,dmn", },
    { "=m,d", "%0,0", "dn,dmn", },
    { "+m,d", "dn,dmn", },
    { "+m,d", "dn,dmn", },
    { "=m,d", "%0,0", "dKs,dmKs", },
    { "=m,d", "%0,0", "dn,dmn", },
    { "+m,d", "dn,dmn", },
    { "+m,d", "dn,dmn", },
    { "=m,d", "%0,0", "dn,dmn", },
    { "+m,d", "dn,dmn", },
    { "+m,d", "dn,dmn", },
    { "=do,m", "%0,0", "di,dKs", },
    { "=dm", "%0", "dn", },
    { "+dm", "dn", },
    { "+dm", "dn", },
    { "=dm", "%0", "dn", },
    { "+dm", "dn", },
    { "+dm", "dn", },
    { "=dm", "0", },
    { "=dm", "0", },
    { "+dm", },
    { "=dm", "0", },
    { "+dm", },
    { "", "", },
    { "=x,y", "xH,rmF", },
    { "=f,d", "fdmF,0", },
    { "", "", },
    { "=x,y", "xH,rmF", },
    { "=f,d", "fmF,0", },
    { "=f", "fm", },
    { "", "", },
    { "=x,y", "xH,rmF", },
    { "=f", "fdmF", },
    { "", "", },
    { "=x,y", "xH,rmF", },
    { "=f", "fmF", },
    { "=dm", "0", },
    { "=dm", "0", },
    { "+dm", },
    { "=dm", "0", },
    { "+dm", },
    { "=d", "0", "i", },
    { "=d", "0", "i", },
    { "=d", "0", "dI", },
    { "=d", "0", "dI", },
    { "+d", "dI", },
    { "=d", "0", "dI", },
    { "+d", "dI", },
    { "=d", "0", "i", },
    { "=d", "0", "i", },
    { "=d", "0", "dI", },
    { "=d", "0", "dI", },
    { "+d", "dI", },
    { "=d", "0", "dI", },
    { "+d", "dI", },
    { "=d", "0", "i", },
    { "=d", "0", "i", },
    { "=d", "0", "dI", },
    { "=d", "0", "dI", },
    { "+d", "dI", },
    { "=d", "0", "dI", },
    { "+d", "dI", },
    { "=d", "0", "i", },
    { "=d", "0", "i", },
    { "=d", "0", "dI", },
    { "=d", "0", "dI", },
    { "+d", "dI", },
    { "=d", "0", "dI", },
    { "+d", "dI", },
    { "=d", "0", "dI", },
    { "=d", "0", "dI", },
    { "+d", "dI", },
    { "=d", "0", "dI", },
    { "+d", "dI", },
    { "=d", "0", "dI", },
    { "=d", "0", "dI", },
    { "+d", "dI", },
    { "=d", "0", "dI", },
    { "+d", "dI", },
    { "+o", "i", "i", "rmi", },
    { "+do", "i", "i", "d", },
    { "=rm", "o", "i", "i", },
    { "=&d", "do", "i", "i", },
    { "=rm", "o", "i", "i", },
    { "=d", "do", "i", "i", },
    { "=d,d", "o,d", "di,di", "di,di", },
    { "=d,d", "o,d", "di,di", "di,di", },
    { "+o,d", "di,di", "di,di", "i,i", },
    { "+o,d", "di,di", "di,di", },
    { "+o,d", "di,di", "di,di", },
    { "+o,d", "di,di", "di,di", "d,d", },
    { "=d", "d", "di", "di", },
    { "=d", "d", "di", "di", },
    { "+d", "di", "di", },
    { "+d", "di", "di", },
    { "+d", "di", "di", "d", },
    { "o", "di", "di", },
    { "d", "di", "di", },
    { "=d", },
    { "=d", },
    { "=d", },
    { "=d", },
    { "=d", },
    { "=d", },
    { "=d", },
    { "=d", },
    { "=d", },
    { "=d", },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { "", },
    { "a", },
    { "r", },
    { "+g", },
    { "+g", },
    { "+g", },
    { "+g", },
    { "", "", },
    { "o", "g", },
    { "o", "g", },
    { "", "", "", },
    { "=rf", "o", "g", },
    { "=rf", "o", "g", },
    { "", "", "", },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { "p", },
    { "=a", "p", },
    { "=f", "ad", },
    { "n", "=m", "rmfF", },
    { "n", "=m", "g", },
    { "", "dami", },
    { "=d", "rmn", },
    { "+d", "", "", "", },
    { "+d", "", "", "", },
    { "=x,y,y", "%x,dmF,y", "xH,y,y", "xH,y,dmF", },
    { "=x,y,y", "%x,ydmF,y", "xH,y,ydmF", "xH,ydmF,ydmF", },
    { "=x,y,y", "xH,rmF,y", "%xH,y,y", "x,y,rmF", },
    { "=x,y,y", "xH,rmF,yrmF", "%xH,rmF,y", "x,y,yrmF", },
    { "=x,y,y", "%xH,y,y", "x,y,rmF", "xH,rmF,y", },
    { "=x,y,y", "%xH,rmF,y", "x,y,yrmF", "xH,rmF,yrmF", },
    { "=x,y,y", "%xH,y,y", "x,y,rmF", "xH,rmF,y", },
    { "=x,y,y", "%xH,rmF,y", "x,y,yrmF", "xH,rmF,yrmF", },
    { "=x,y,y", "xH,y,y", "x,y,rmF", "xH,rmF,y", },
    { "=x,y,y", "xH,rmF,y", "xH,y,y", "x,y,rmF", },
    { "=x,y,y", "xH,rmF,y", "x,y,yrmF", "xH,rmF,yrmF", },
    { "=x,y,y", "xH,rmF,yrmF", "xH,rmF,y", "x,y,yrmF", },
    { "fm", },
    { "f,mG", "fmG,f", },
    { "f,mG", "fmG,f", },
    { "=fm,f", "f,m", },
    { "=fm,f", "f,m", },
    { "=m,!r", "f,f", },
    { "=dm", "f", },
    { "=f", "dmi", },
    { "=f", "dmn", },
    { "=f", "dmn", },
    { "=f", "fFm", },
    { "=dm", "f", },
    { "=dm", "f", },
    { "=dm", "f", },
    { "", "", "", },
    { "=f", "%0", "fmG", },
    { "", "", "", },
    { "=f", "0", "fmG", },
    { "", "", "", },
    { "=f", "%0", "fmG", },
    { "", "", "", },
    { "=f", "0", "fmG", },
    { "=f", "fmF", },
    { "=f", "fmF", },
    { "=f", "fm", },
  };

const enum machine_mode insn_operand_mode[][MAX_RECOG_OPERANDS] =
  {
    { DFmode, DFmode, },
    { DImode, DImode, },
    { SImode, },
    { HImode, },
    { QImode, },
    { SFmode, },
    { SFmode, SImode, },
    { SFmode, },
    { DFmode, },
    { DFmode, SImode, },
    { DFmode, },
    { SImode, SImode, },
    { HImode, HImode, },
    { QImode, QImode, },
    { DFmode, DFmode, },
    { DFmode, DFmode, SImode, },
    { DFmode, DFmode, },
    { SFmode, SFmode, },
    { SFmode, SFmode, SImode, },
    { SFmode, SFmode, },
    { QImode, SImode, },
    { SImode, SImode, },
    { QImode, SImode, },
    { SImode, SImode, },
    { QImode, SImode, },
    { SImode, SImode, },
    { SImode, SImode, },
    { SImode, },
    { SImode, SImode, },
    { SImode, SImode, },
    { HImode, HImode, },
    { HImode, HImode, },
    { QImode, QImode, },
    { QImode, QImode, },
    { SFmode, SFmode, },
    { DFmode, DFmode, },
    { XFmode, XFmode, },
    { XFmode, XFmode, },
    { XFmode, XFmode, },
    { DImode, DImode, },
    { SImode, SImode, },
    { QImode, SImode, },
    { QImode, HImode, },
    { HImode, SImode, },
    { SImode, HImode, },
    { HImode, QImode, },
    { SImode, QImode, },
    { SImode, HImode, },
    { HImode, QImode, },
    { SImode, QImode, },
    { SImode, HImode, },
    { HImode, QImode, },
    { SImode, QImode, },
    { DFmode, SFmode, },
    { DFmode, SFmode, },
    { DFmode, SFmode, },
    { SFmode, DFmode, },
    { SFmode, DFmode, },
    { SFmode, DFmode, },
    { SFmode, DFmode, },
    { SFmode, SImode, },
    { SFmode, SImode, },
    { SFmode, SImode, },
    { DFmode, SImode, },
    { DFmode, SImode, },
    { DFmode, SImode, },
    { SFmode, HImode, },
    { DFmode, HImode, },
    { SFmode, QImode, },
    { DFmode, QImode, },
    { SImode, DFmode, SImode, SImode, },
    { HImode, DFmode, SImode, SImode, },
    { QImode, DFmode, SImode, SImode, },
    { DFmode, DFmode, },
    { SFmode, SFmode, },
    { QImode, SFmode, },
    { HImode, SFmode, },
    { SImode, SFmode, },
    { QImode, DFmode, },
    { HImode, DFmode, },
    { SImode, DFmode, },
    { SImode, SFmode, },
    { SImode, DFmode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, HImode, },
    { HImode, HImode, HImode, },
    { HImode, HImode, },
    { HImode, HImode, },
    { QImode, QImode, QImode, },
    { QImode, QImode, },
    { QImode, QImode, },
    { DFmode, DFmode, DFmode, },
    { DFmode, DFmode, DFmode, },
    { DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, },
    { SFmode, SFmode, SFmode, },
    { SFmode, SFmode, SFmode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, HImode, },
    { HImode, HImode, HImode, },
    { HImode, HImode, },
    { QImode, QImode, QImode, },
    { QImode, QImode, },
    { DFmode, DFmode, DFmode, },
    { DFmode, DFmode, DFmode, },
    { DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, },
    { SFmode, SFmode, SFmode, },
    { SFmode, SFmode, SFmode, },
    { HImode, HImode, HImode, },
    { SImode, HImode, HImode, },
    { SImode, HImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, HImode, HImode, },
    { SImode, HImode, SImode, },
    { DImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { DImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { DFmode, DFmode, DFmode, },
    { DFmode, DFmode, DFmode, },
    { DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, },
    { SFmode, SFmode, SFmode, },
    { SFmode, SFmode, SFmode, },
    { HImode, HImode, HImode, },
    { HImode, SImode, HImode, },
    { HImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { HImode, SImode, HImode, },
    { HImode, SImode, SImode, },
    { DFmode, DFmode, DFmode, },
    { DFmode, DFmode, DFmode, },
    { DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, },
    { SFmode, SFmode, SFmode, },
    { SFmode, SFmode, SFmode, },
    { HImode, HImode, HImode, },
    { HImode, SImode, HImode, },
    { HImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { HImode, SImode, HImode, },
    { HImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { HImode, HImode, },
    { HImode, HImode, },
    { QImode, QImode, QImode, },
    { QImode, QImode, },
    { QImode, QImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { HImode, HImode, },
    { HImode, HImode, },
    { QImode, QImode, QImode, },
    { QImode, QImode, },
    { QImode, QImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { HImode, HImode, },
    { HImode, HImode, },
    { QImode, QImode, QImode, },
    { QImode, QImode, },
    { QImode, QImode, },
    { SImode, SImode, },
    { HImode, HImode, },
    { HImode, },
    { QImode, QImode, },
    { QImode, },
    { SFmode, SFmode, },
    { SFmode, SFmode, },
    { SFmode, SFmode, },
    { DFmode, DFmode, },
    { DFmode, DFmode, },
    { DFmode, DFmode, },
    { DFmode, DFmode, },
    { SFmode, SFmode, },
    { SFmode, SFmode, },
    { SFmode, SFmode, },
    { DFmode, DFmode, },
    { DFmode, DFmode, },
    { DFmode, DFmode, },
    { SImode, SImode, },
    { HImode, HImode, },
    { HImode, },
    { QImode, QImode, },
    { QImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { HImode, HImode, },
    { QImode, QImode, QImode, },
    { QImode, QImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { HImode, HImode, },
    { QImode, QImode, QImode, },
    { QImode, QImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { HImode, HImode, },
    { QImode, QImode, QImode, },
    { QImode, QImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { HImode, HImode, },
    { QImode, QImode, QImode, },
    { QImode, QImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { HImode, HImode, },
    { QImode, QImode, QImode, },
    { QImode, QImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { HImode, HImode, },
    { QImode, QImode, QImode, },
    { QImode, QImode, },
    { QImode, SImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { SImode, QImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { SImode, QImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { SImode, QImode, SImode, SImode, },
    { SImode, QImode, SImode, SImode, },
    { QImode, SImode, SImode, VOIDmode, },
    { QImode, SImode, SImode, },
    { QImode, SImode, SImode, },
    { QImode, SImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { QImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode, },
    { SImode, },
    { HImode, },
    { HImode, },
    { SImode, },
    { HImode, },
    { SImode, },
    { QImode, SImode, },
    { QImode, SImode, },
    { QImode, SImode, },
    { VOIDmode, QImode, SImode, },
    { VOIDmode, QImode, SImode, },
    { VOIDmode, QImode, SImode, },
    { VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { SImode, },
    { SImode, QImode, },
    { VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
    { DFmode, DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, SFmode, },
    { DFmode, DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, SFmode, },
    { DFmode, DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, SFmode, },
    { DFmode, DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, SFmode, },
    { DFmode, DFmode, DFmode, DFmode, },
    { DFmode, DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, SFmode, },
    { SFmode, SFmode, SFmode, SFmode, },
    { XFmode, },
    { XFmode, XFmode, },
    { XFmode, XFmode, },
    { XFmode, SFmode, },
    { XFmode, DFmode, },
    { DFmode, XFmode, },
    { SFmode, XFmode, },
    { XFmode, SImode, },
    { XFmode, HImode, },
    { XFmode, QImode, },
    { XFmode, XFmode, },
    { QImode, XFmode, },
    { HImode, XFmode, },
    { SImode, XFmode, },
    { XFmode, XFmode, XFmode, },
    { XFmode, XFmode, XFmode, },
    { XFmode, XFmode, XFmode, },
    { XFmode, XFmode, XFmode, },
    { XFmode, XFmode, XFmode, },
    { XFmode, XFmode, XFmode, },
    { XFmode, XFmode, XFmode, },
    { XFmode, XFmode, XFmode, },
    { XFmode, XFmode, },
    { XFmode, XFmode, },
    { XFmode, DFmode, },
  };

const char insn_operand_strict_low[][MAX_RECOG_OPERANDS] =
  {
    { 0, 0, },
    { 0, 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, 0, },
    { 0, },
    { 0, },
    { 0, 0, },
    { 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 1, 0, },
    { 0, 0, },
    { 1, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 1, 0, },
    { 0, 0, },
    { 0, 0, },
    { 1, },
    { 0, 0, },
    { 1, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 1, },
    { 0, 0, },
    { 1, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 0, 0, 0, },
    { 1, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
  };

extern int push_operand ();
extern int general_operand ();
extern int nonimmediate_operand ();
extern int scratch_operand ();
extern int address_operand ();
extern int register_operand ();
extern int const_int_operand ();
extern int not_sp_operand ();
extern int immediate_operand ();
extern int memory_operand ();

int (*const insn_operand_predicate[][MAX_RECOG_OPERANDS])() =
  {
    { push_operand, general_operand, },
    { push_operand, general_operand, },
    { nonimmediate_operand, },
    { nonimmediate_operand, },
    { nonimmediate_operand, },
    { general_operand, },
    { general_operand, scratch_operand, },
    { general_operand, },
    { general_operand, },
    { general_operand, scratch_operand, },
    { general_operand, },
    { nonimmediate_operand, general_operand, },
    { nonimmediate_operand, general_operand, },
    { nonimmediate_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, scratch_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, scratch_operand, },
    { general_operand, general_operand, },
    { nonimmediate_operand, general_operand, },
    { nonimmediate_operand, general_operand, },
    { nonimmediate_operand, general_operand, },
    { nonimmediate_operand, general_operand, },
    { nonimmediate_operand, general_operand, },
    { nonimmediate_operand, general_operand, },
    { push_operand, general_operand, },
    { general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { nonimmediate_operand, general_operand, },
    { nonimmediate_operand, nonimmediate_operand, },
    { nonimmediate_operand, nonimmediate_operand, },
    { general_operand, general_operand, },
    { push_operand, address_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, general_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, register_operand, scratch_operand, scratch_operand, },
    { general_operand, register_operand, scratch_operand, scratch_operand, },
    { general_operand, register_operand, scratch_operand, scratch_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, nonimmediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, nonimmediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, nonimmediate_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, const_int_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, nonimmediate_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, const_int_operand, },
    { register_operand, register_operand, nonimmediate_operand, },
    { register_operand, register_operand, nonimmediate_operand, register_operand, },
    { register_operand, register_operand, const_int_operand, register_operand, },
    { register_operand, register_operand, nonimmediate_operand, },
    { register_operand, register_operand, nonimmediate_operand, register_operand, },
    { register_operand, register_operand, const_int_operand, register_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, nonimmediate_operand, },
    { general_operand, general_operand, const_int_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, nonimmediate_operand, },
    { general_operand, general_operand, const_int_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, nonimmediate_operand, },
    { general_operand, general_operand, const_int_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, nonimmediate_operand, },
    { general_operand, general_operand, const_int_operand, },
    { general_operand, general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, general_operand, },
    { not_sp_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, },
    { general_operand, general_operand, },
    { general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, },
    { general_operand, general_operand, },
    { general_operand, },
    { register_operand, register_operand, immediate_operand, },
    { register_operand, register_operand, immediate_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, register_operand, immediate_operand, },
    { register_operand, register_operand, immediate_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, register_operand, immediate_operand, },
    { register_operand, register_operand, immediate_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, register_operand, immediate_operand, },
    { register_operand, register_operand, immediate_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, register_operand, general_operand, },
    { register_operand, general_operand, },
    { nonimmediate_operand, immediate_operand, immediate_operand, general_operand, },
    { nonimmediate_operand, immediate_operand, immediate_operand, general_operand, },
    { general_operand, nonimmediate_operand, immediate_operand, immediate_operand, },
    { general_operand, nonimmediate_operand, immediate_operand, immediate_operand, },
    { general_operand, nonimmediate_operand, immediate_operand, immediate_operand, },
    { general_operand, nonimmediate_operand, immediate_operand, immediate_operand, },
    { general_operand, nonimmediate_operand, general_operand, general_operand, },
    { general_operand, nonimmediate_operand, general_operand, general_operand, },
    { nonimmediate_operand, general_operand, general_operand, immediate_operand, },
    { nonimmediate_operand, general_operand, general_operand, },
    { nonimmediate_operand, general_operand, general_operand, },
    { nonimmediate_operand, general_operand, general_operand, general_operand, },
    { general_operand, nonimmediate_operand, general_operand, general_operand, },
    { general_operand, nonimmediate_operand, general_operand, general_operand, },
    { nonimmediate_operand, general_operand, general_operand, },
    { nonimmediate_operand, general_operand, general_operand, },
    { nonimmediate_operand, general_operand, general_operand, general_operand, },
    { memory_operand, general_operand, general_operand, },
    { nonimmediate_operand, general_operand, general_operand, },
    { general_operand, },
    { general_operand, },
    { general_operand, },
    { general_operand, },
    { general_operand, },
    { general_operand, },
    { general_operand, },
    { general_operand, },
    { general_operand, },
    { general_operand, },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0, },
    { register_operand, },
    { register_operand, },
    { general_operand, },
    { general_operand, },
    { general_operand, },
    { general_operand, },
    { memory_operand, general_operand, },
    { memory_operand, general_operand, },
    { memory_operand, general_operand, },
    { 0, memory_operand, general_operand, },
    { 0, memory_operand, general_operand, },
    { 0, memory_operand, general_operand, },
    { 0, 0, 0, },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { address_operand, },
    { general_operand, address_operand, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { register_operand, general_operand, general_operand, general_operand, },
    { register_operand, general_operand, general_operand, general_operand, },
    { register_operand, general_operand, general_operand, general_operand, },
    { register_operand, general_operand, general_operand, general_operand, },
    { register_operand, general_operand, general_operand, general_operand, },
    { register_operand, general_operand, general_operand, general_operand, },
    { register_operand, general_operand, general_operand, general_operand, },
    { register_operand, general_operand, general_operand, general_operand, },
    { register_operand, general_operand, general_operand, general_operand, },
    { register_operand, general_operand, general_operand, general_operand, },
    { register_operand, general_operand, general_operand, general_operand, },
    { register_operand, general_operand, general_operand, general_operand, },
    { nonimmediate_operand, },
    { general_operand, general_operand, },
    { nonimmediate_operand, nonimmediate_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, nonimmediate_operand, nonimmediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, nonimmediate_operand, nonimmediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, nonimmediate_operand, nonimmediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, nonimmediate_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, },
  };

const int insn_n_alternatives[] =
  {
    1,
    1,
    1,
    1,
    1,
    0,
    1,
    1,
    0,
    1,
    1,
    3,
    4,
    3,
    0,
    2,
    2,
    0,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    4,
    1,
    1,
    5,
    1,
    6,
    8,
    0,
    5,
    3,
    7,
    1,
    2,
    2,
    2,
    0,
    0,
    0,
    2,
    2,
    2,
    2,
    1,
    1,
    0,
    2,
    2,
    0,
    2,
    1,
    1,
    0,
    2,
    1,
    0,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    4,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    0,
    2,
    1,
    0,
    2,
    1,
    4,
    1,
    2,
    2,
    2,
    2,
    0,
    3,
    1,
    0,
    3,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    1,
    1,
    0,
    1,
    1,
    0,
    2,
    1,
    0,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    3,
    1,
    0,
    3,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    2,
    2,
    0,
    2,
    2,
    1,
    0,
    2,
    1,
    0,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    1,
    1,
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    1,
    2,
    2,
    2,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    1,
    1,
    1,
  };

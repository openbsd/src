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

{ cc_status.flags |= CC_REVERSED;
  operands[1] = const0_rtx;
  return "cmpqd %1,%0"; }
}

static char *
output_1 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ cc_status.flags |= CC_REVERSED;
  operands[1] = const0_rtx;
  return "cmpqw %1,%0"; }
}

static char *
output_2 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ cc_status.flags |= CC_REVERSED;
  operands[1] = const0_rtx;
  return "cmpqb %1,%0"; }
}

static char *
output_3 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ cc_status.flags |= CC_REVERSED;
  operands[1] = CONST0_RTX (DFmode);
  return "cmpl %1,%0"; }
}

static char *
output_4 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ cc_status.flags |= CC_REVERSED;
  operands[1] = CONST0_RTX (SFmode);
  return "cmpf %1,%0"; }
}

static char *
output_5 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT)
    {
      int i = INTVAL (operands[1]);
      if (i <= 7 && i >= -8)
	{
	  cc_status.flags |= CC_REVERSED;
	  return "cmpqd %1,%0";
	}
    }
  cc_status.flags &= ~CC_REVERSED;
  if (GET_CODE (operands[0]) == CONST_INT)
    {
      int i = INTVAL (operands[0]);
      if (i <= 7 && i >= -8)
	return "cmpqd %0,%1";
    }
  return "cmpd %0,%1";
}
}

static char *
output_6 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT)
    {
      short i = INTVAL (operands[1]);
    if (i <= 7 && i >= -8)
      {
	cc_status.flags |= CC_REVERSED;
	if (INTVAL (operands[1]) > 7)
	  operands[1] = gen_rtx(CONST_INT, VOIDmode, i);
	return "cmpqw %1,%0";
      }
    }
  cc_status.flags &= ~CC_REVERSED;
  if (GET_CODE (operands[0]) == CONST_INT)
    {
      short i = INTVAL (operands[0]);
      if (i <= 7 && i >= -8)
	{
	  if (INTVAL (operands[0]) > 7)
	    operands[0] = gen_rtx(CONST_INT, VOIDmode, i);
	  return "cmpqw %0,%1";
	}
    }
  return "cmpw %0,%1";
}
}

static char *
output_7 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT)
    {
      char i = INTVAL (operands[1]);
      if (i <= 7 && i >= -8)
	{
	  cc_status.flags |= CC_REVERSED;
	  if (INTVAL (operands[1]) > 7)
	    operands[1] = gen_rtx(CONST_INT, VOIDmode, i);
	  return "cmpqb %1,%0";
	}
    }
  cc_status.flags &= ~CC_REVERSED;
  if (GET_CODE (operands[0]) == CONST_INT)
    {
      char i = INTVAL (operands[0]);
      if (i <= 7 && i >= -8)
	{
	  if (INTVAL (operands[0]) > 7)
	    operands[0] = gen_rtx(CONST_INT, VOIDmode, i);
	  return "cmpqb %0,%1";
	}
    }
  return "cmpb %0,%1";
}
}

static char *
output_10 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[0]))
    {
      if (FP_REG_P (operands[1]) || GET_CODE (operands[1]) == CONST_DOUBLE)
	return "movl %1,%0";
      if (REG_P (operands[1]))
	{
	  rtx xoperands[2];
	  xoperands[1] = gen_rtx (REG, SImode, REGNO (operands[1]) + 1);
	  output_asm_insn ("movd %1,tos", xoperands);
	  output_asm_insn ("movd %1,tos", operands);
	  return "movl tos,%0";
	}
      return "movl %1,%0";
    }
  else if (FP_REG_P (operands[1]))
    {
      if (REG_P (operands[0]))
	{
	  output_asm_insn ("movl %1,tos\n\tmovd tos,%0", operands);
	  operands[0] = gen_rtx (REG, SImode, REGNO (operands[0]) + 1);
	  return "movd tos,%0";
	}
      else
        return "movl %1,%0";
    }
  return output_move_double (operands);
}
}

static char *
output_11 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[0]))
    {
      if (GET_CODE (operands[1]) == REG && REGNO (operands[1]) < 8)
	return "movd %1,tos\n\tmovf tos,%0";
      else
	return "movf %1,%0";
    }
  else if (FP_REG_P (operands[1]))
    {
      if (REG_P (operands[0]))
	return "movf %1,tos\n\tmovd tos,%0";
      return "movf %1,%0";
    }
#if 0 /* Someone suggested this for the Sequent.  Is it needed?  */
  else if (GET_CODE (operands[1]) == CONST_DOUBLE)
    return "movf %1,%0";
#endif
/* There was a #if 0 around this, but that was erroneous
   for many machines -- rms.  */
#ifndef MOVD_FLOAT_OK
  /* GAS understands floating constants in ordinary movd instructions
     but other assemblers might object.  */
  else if (GET_CODE (operands[1]) == CONST_DOUBLE)
    {
      union {int i[2]; float f; double d;} convrt;
      convrt.i[0] = CONST_DOUBLE_LOW (operands[1]);
      convrt.i[1] = CONST_DOUBLE_HIGH (operands[1]);
      convrt.f = convrt.d;

      /* Is there a better machine-independent way to to this?  */
      operands[1] = gen_rtx (CONST_INT, VOIDmode, convrt.i[0]);
      return "movd %1,%0";
    }
#endif
  else return "movd %1,%0";
}
}

static char *
output_13 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[0]))
    {
      if (FP_REG_P (operands[1]) || GET_CODE (operands[1]) == CONST_DOUBLE)
	return "movl %1,%0";
      if (REG_P (operands[1]))
	{
	  rtx xoperands[2];
	  xoperands[1] = gen_rtx (REG, SImode, REGNO (operands[1]) + 1);
	  output_asm_insn ("movd %1,tos", xoperands);
	  output_asm_insn ("movd %1,tos", operands);
	  return "movl tos,%0";
	}
      return "movl %1,%0";
    }
  else if (FP_REG_P (operands[1]))
    {
      if (REG_P (operands[0]))
	{
	  output_asm_insn ("movl %1,tos\n\tmovd tos,%0", operands);
	  operands[0] = gen_rtx (REG, SImode, REGNO (operands[0]) + 1);
	  return "movd tos,%0";
	}
      else
        return "movl %1,%0";
    }
  return output_move_double (operands);
}
}

static char *
output_15 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[0]))
    {
      if (GET_CODE (operands[1]) == REG && REGNO (operands[1]) < 8)
	return "movd %1,tos\n\tmovf tos,%0";
      else
	return "movf %1,%0";
    }
  else if (FP_REG_P (operands[1]))
    {
      if (REG_P (operands[0]))
	return "movf %1,tos\n\tmovd tos,%0";
      return "movf %1,%0";
    }
  if (GET_CODE (operands[0]) == REG
      && REGNO (operands[0]) == FRAME_POINTER_REGNUM)
    return "lprd fp,%1";
  if (GET_CODE (operands[1]) == CONST_DOUBLE)
    operands[1]
      = gen_rtx (CONST_INT, VOIDmode, CONST_DOUBLE_LOW (operands[1]));
  if (GET_CODE (operands[1]) == CONST_INT)
    {
      int i = INTVAL (operands[1]);
      if (! TARGET_32532)
	{
	  if (i <= 7 && i >= -8)
	    return "movqd %1,%0";
	  if (i < 0x4000 && i >= -0x4000)
#if defined (GNX_V3) || defined (UTEK_ASM)
	    return "addr %c1,%0";
#else
	    return "addr @%c1,%0";
#endif
	}
      else
        return output_move_dconst(i, "%$%1,%0");
    }
  else if (GET_CODE (operands[1]) == REG)
    {
      if (REGNO (operands[1]) < 16)
        return "movd %1,%0";
      else if (REGNO (operands[1]) == FRAME_POINTER_REGNUM)
	{
	  if (GET_CODE(operands[0]) == REG)
	    return "sprd fp,%0";
	  else
	    return "addr 0(fp),%0" ;
	}
      else if (REGNO (operands[1]) == STACK_POINTER_REGNUM)
	{
	  if (GET_CODE(operands[0]) == REG)
	    return "sprd sp,%0";
	  else
	    return "addr 0(sp),%0" ;
	}
      else abort ();
    }
  else if (GET_CODE (operands[1]) == MEM)
    return "movd %1,%0";

  /* Check if this effective address can be
     calculated faster by pulling it apart.  */
  if (REG_P (operands[0])
      && GET_CODE (operands[1]) == MULT
      && GET_CODE (XEXP (operands[1], 1)) == CONST_INT
      && (INTVAL (XEXP (operands[1], 1)) == 2
	  || INTVAL (XEXP (operands[1], 1)) == 4))
    {
      rtx xoperands[3];
      xoperands[0] = operands[0];
      xoperands[1] = XEXP (operands[1], 0);
      xoperands[2] = gen_rtx (CONST_INT, VOIDmode, INTVAL (XEXP (operands[1], 1)) >> 1);
      return output_shift_insn (xoperands);
    }
  return "addr %a1,%0";
}
}

static char *
output_16 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT)
    {
      short i = INTVAL (operands[1]);
      if (i <= 7 && i >= -8)
	{
	  if (INTVAL (operands[1]) > 7)
	    operands[1] =
	      gen_rtx (CONST_INT, VOIDmode, i);
	  return "movqw %1,%0";
	}
	return "movw %1,%0";
    }
  else if (FP_REG_P (operands[0]))
    {
      if (GET_CODE (operands[1]) == REG && REGNO (operands[1]) < 8)
	return "movwf %1,tos\n\tmovf tos,%0";
      else
	return "movwf %1,%0";
    }
  else if (FP_REG_P (operands[1]))
    {
      if (REG_P (operands[0]))
	return "movf %1,tos\n\tmovd tos,%0";
      return "movf %1,%0";
    }
  else
     return "movw %1,%0";
}
}

static char *
output_17 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT
      && INTVAL(operands[1]) <= 7 && INTVAL(operands[1]) >= -8)
    return "movqw %1,%0";
  return "movw %1,%0";
}
}

static char *
output_18 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (GET_CODE (operands[1]) == CONST_INT)
    {
      char char_val = (char)INTVAL (operands[1]);
      if (char_val <= 7 && char_val >= -8)
	{
	  if (INTVAL (operands[1]) > 7)
	    operands[1] =
	      gen_rtx (CONST_INT, VOIDmode, char_val);
	  return "movqb %1,%0";
	}
	return "movb %1,%0";
    }
  else if (FP_REG_P (operands[0]))
    {
      if (GET_CODE (operands[1]) == REG && REGNO (operands[1]) < 8)
	return "movbf %1,tos\n\tmovf tos,%0";
      else
	return "movbf %1,%0";
    }
  else if (FP_REG_P (operands[1]))
    {
      if (REG_P (operands[0]))
	return "movf %1,tos\n\tmovd tos,%0";
      return "movf %1,%0";
    }
  else
     return "movb %1,%0";
}
}

static char *
output_19 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT
      && INTVAL(operands[1]) < 8 && INTVAL(operands[1]) > -9)
    return "movqb %1,%0";
  return "movb %1,%0";
}
}

static char *
output_21 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM)
    abort ();
  operands[0] = XEXP (operands[0], 0);
  operands[1] = XEXP (operands[1], 0);
  if (GET_CODE (operands[0]) == MEM)
    if (GET_CODE (operands[1]) == MEM)
      output_asm_insn ("movd %0,r2\n\tmovd %1,r1", operands);
    else
      output_asm_insn ("movd %0,r2\n\taddr %a1,r1", operands);
  else if (GET_CODE (operands[1]) == MEM)
    output_asm_insn ("addr %a0,r2\n\tmovd %1,r1", operands);
  else
    output_asm_insn ("addr %a0,r2\n\taddr %a1,r1", operands);

#ifdef UTEK_ASM
  if (GET_CODE (operands[2]) == CONST_INT && (INTVAL (operands[2]) & 0x3) == 0)
    {
      operands[2] = gen_rtx (CONST_INT, VOIDmode, INTVAL (operands[2]) >> 2);
      if ((unsigned) INTVAL (operands[2]) <= 7)
	return "movqd %2,r0\n\tmovsd $0";
      else 
	return "movd %2,r0\n\tmovsd $0";
    }
  else
    {
      return "movd %2,r0\n\tmovsb $0";
    }
#else
  if (GET_CODE (operands[2]) == CONST_INT && (INTVAL (operands[2]) & 0x3) == 0)
    {
      operands[2] = gen_rtx (CONST_INT, VOIDmode, INTVAL (operands[2]) >> 2);
      if ((unsigned) INTVAL (operands[2]) <= 7)
	return "movqd %2,r0\n\tmovsd";
      else 
	return "movd %2,r0\n\tmovsd";
    }
  else
    {
      return "movd %2,r0\n\tmovsb";
    }
#endif
}
}

static char *
output_58 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifndef SEQUENT_ADJUST_STACK
  if (TARGET_32532)
    if (INTVAL (operands[0]) == 8)
      return "cmpd tos,tos";
  if (TARGET_32532 || TARGET_32332)
    if (INTVAL (operands[0]) == 4)
      return "cmpqd %$0,tos";
#endif
  if (! TARGET_32532)
    {
      if (INTVAL (operands[0]) < 64 && INTVAL (operands[0]) > -64)
        return "adjspb %$%n0";
      else if (INTVAL (operands[0]) < 8192 && INTVAL (operands[0]) >= -8192)
        return "adjspw %$%n0";
    }
  return "adjspd %$%n0";
}
}

static char *
output_61 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (which_alternative == 1)
    {
      int i = INTVAL (operands[2]);
      if (NS32K_DISPLACEMENT_P (i))
	return "addr %c2(%1),%0";
      else
	return "movd %1,%0\n\taddd %2,%0";
    }
  if (GET_CODE (operands[2]) == CONST_INT)
    {
      int i = INTVAL (operands[2]);

      if (i <= 7 && i >= -8)
	return "addqd %2,%0";
      else if (GET_CODE (operands[0]) == REG
	       && i < 0x4000 && i >= -0x4000 && ! TARGET_32532)
	return "addr %c2(%0),%0";
    }
  return "addd %2,%0";
}
}

static char *
output_62 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (GET_CODE (operands[2]) == CONST_INT)
    {
      int i = INTVAL (operands[2]);
      if (i <= 7 && i >= -8)
	return "addqw %2,%0";
    }
  return "addw %2,%0";
}
}

static char *
output_63 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT
      && INTVAL (operands[1]) >-9 && INTVAL(operands[1]) < 8)
    return "addqw %2,%0";
  return "addw %2,%0";
}
}

static char *
output_64 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (GET_CODE (operands[2]) == CONST_INT)
    {
      int i = INTVAL (operands[2]);
      if (i <= 7 && i >= -8)
	return "addqb %2,%0";
    }
  return "addb %2,%0";
}
}

static char *
output_65 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT
      && INTVAL (operands[1]) >-9 && INTVAL(operands[1]) < 8)
    return "addqb %2,%0";
  return "addb %2,%0";
}
}

static char *
output_68 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE(operands[0]) == CONST_INT && INTVAL(operands[0]) < 64
      && INTVAL(operands[0]) > -64 && ! TARGET_32532)
    return "adjspb %$%0";
  return "adjspd %$%0";
}
}

static char *
output_69 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (GET_CODE (operands[2]) == CONST_INT)
    {
      int i = INTVAL (operands[2]);

      if (i <= 8 && i >= -7)
        return "addqd %$%n2,%0";
    }
  return "subd %2,%0";
}
}

static char *
output_70 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (GET_CODE (operands[2]) == CONST_INT)
    {
      int i = INTVAL (operands[2]);

      if (i <= 8 && i >= -7)
        return "addqw %$%n2,%0";
    }
  return "subw %2,%0";
}
}

static char *
output_71 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT
      && INTVAL (operands[1]) >-8 && INTVAL(operands[1]) < 9)
    return "addqw %$%n2,%0";
  return "subw %2,%0";
}
}

static char *
output_72 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (GET_CODE (operands[2]) == CONST_INT)
    {
      int i = INTVAL (operands[2]);

      if (i <= 8 && i >= -7)
	return "addqb %$%n2,%0";
    }
  return "subb %2,%0";
}
}

static char *
output_73 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT
      && INTVAL (operands[1]) >-8 && INTVAL(operands[1]) < 9)
    return "addqb %$%n2,%0";
  return "subb %2,%0";
}
}

static char *
output_85 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  operands[1] = gen_rtx (REG, SImode, REGNO (operands[0]) + 1);
  return "deid %2,%0\n\tmovd %1,%0";
}
}

static char *
output_86 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  operands[1] = gen_rtx (REG, HImode, REGNO (operands[0]) + 1);
  return "deiw %2,%0\n\tmovw %1,%0";
}
}

static char *
output_87 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  operands[1] = gen_rtx (REG, QImode, REGNO (operands[0]) + 1);
  return "deib %2,%0\n\tmovb %1,%0";
}
}

static char *
output_94 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[2]) == CONST_INT)
    {
      if ((INTVAL (operands[2]) | 0xff) == 0xffffffff)
	{
	  if (INTVAL (operands[2]) == 0xffffff00)
	    return "movqb %$0,%0";
	  else
	    {
	      operands[2] = gen_rtx (CONST_INT, VOIDmode,
				     INTVAL (operands[2]) & 0xff);
	      return "andb %2,%0";
	    }
	}
      if ((INTVAL (operands[2]) | 0xffff) == 0xffffffff)
        {
	  if (INTVAL (operands[2]) == 0xffff0000)
	    return "movqw %$0,%0";
	  else
	    {
	      operands[2] = gen_rtx (CONST_INT, VOIDmode,
				     INTVAL (operands[2]) & 0xffff);
	      return "andw %2,%0";
	    }
	}
    }
  return "andd %2,%0";
}
}

static char *
output_95 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[2]) == CONST_INT
      && (INTVAL (operands[2]) | 0xff) == 0xffffffff)
    {
      if (INTVAL (operands[2]) == 0xffffff00)
	return "movqb %$0,%0";
      else
	{
	  operands[2] = gen_rtx (CONST_INT, VOIDmode,
				 INTVAL (operands[2]) & 0xff);
	  return "andb %2,%0";
	}
    }
  return "andw %2,%0";
}
}

static char *
output_100 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[2]) == CONST_INT) {
    if ((INTVAL (operands[2]) & 0xffffff00) == 0)
      return "orb %2,%0";
    if ((INTVAL (operands[2]) & 0xffff0000) == 0)
      return "orw %2,%0";
  }
  return "ord %2,%0";
}
}

static char *
output_101 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE(operands[2]) == CONST_INT &&
      (INTVAL(operands[2]) & 0xffffff00) == 0)
    return "orb %2,%0";
  return "orw %2,%0";
}
}

static char *
output_103 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[2]) == CONST_INT) {
    if ((INTVAL (operands[2]) & 0xffffff00) == 0)
      return "xorb %2,%0";
    if ((INTVAL (operands[2]) & 0xffff0000) == 0)
      return "xorw %2,%0";
  }
  return "xord %2,%0";
}
}

static char *
output_104 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE(operands[2]) == CONST_INT &&
      (INTVAL(operands[2]) & 0xffffff00) == 0)
    return "xorb %2,%0";
  return "xorw %2,%0";
}
}

static char *
output_114 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (TARGET_32532)
    return "lshd %2,%0";
  else
    return output_shift_insn (operands);
}
}

static char *
output_115 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (GET_CODE (operands[2]) == CONST_INT)
    {
      if (INTVAL (operands[2]) == 1)
	return "addw %0,%0";
      else if (INTVAL (operands[2]) == 2 && !TARGET_32532)
	return "addw %0,%0\n\taddw %0,%0";
    }
  if (TARGET_32532)
    return "lshw %2,%0";
  else
    return "ashw %2,%0";
}
}

static char *
output_116 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (GET_CODE (operands[2]) == CONST_INT)
    {
      if (INTVAL (operands[2]) == 1)
	return "addb %0,%0";
      else if (INTVAL (operands[2]) == 2 && !TARGET_32532)
	return "addb %0,%0\n\taddb %0,%0";
    }
  if (TARGET_32532)
    return "lshb %2,%0";
  else
    return "ashb %2,%0";
}
}

static char *
output_150 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[0])
      && GET_CODE (operands[1]) == MULT
      && GET_CODE (XEXP (operands[1], 1)) == CONST_INT
      && (INTVAL (XEXP (operands[1], 1)) == 2
	  || INTVAL (XEXP (operands[1], 1)) == 4))
    {
      rtx xoperands[3];
      xoperands[0] = operands[0];
      xoperands[1] = XEXP (operands[1], 0);
      xoperands[2] = gen_rtx (CONST_INT, VOIDmode, INTVAL (XEXP (operands[1], 1)) >> 1);
      return output_shift_insn (xoperands);
    }
  return "addr %a1,%0";
}
}

static char *
output_155 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ cc_status.flags = CC_Z_IN_F;
  return "tbitd %1,%0";
}
}

static char *
output_156 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  output_asm_insn ("movd %1,tos", operands);
  if (INTVAL (operands[2]) == 16)
    {
      if (INTVAL (operands[3]) == 8)
	output_asm_insn ("movzwd 1(sp),%0", operands);
      else
	output_asm_insn ("movzwd 2(sp),%0", operands);
    }
  else
    {
      if (INTVAL (operands[3]) == 8)
	output_asm_insn ("movzbd 1(sp),%0", operands);
      else if (INTVAL (operands[3]) == 16)
	output_asm_insn ("movzbd 2(sp),%0", operands);
      else
	output_asm_insn ("movzbd 3(sp),%0", operands);
    }
  if (TARGET_32532 || TARGET_32332)
    return "cmpqd %$0,tos";
  else
    return "adjspb %$-4";
}
}

static char *
output_157 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (GET_CODE (operands[3]) == CONST_INT)
    return "extsd %1,%0,%3,%2";
  else return "extd %3,%1,%0,%2";
}
}

static char *
output_158 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (GET_CODE (operands[3]) == CONST_INT)
    return "extsd %1,%0,%3,%2";
  else return "extd %3,%1,%0,%2";
}
}

static char *
output_159 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (GET_CODE (operands[2]) == CONST_INT)
    {
      if (INTVAL (operands[2]) >= 8)
	{
	  operands[0] = adj_offsettable_operand (operands[0],
					        INTVAL (operands[2]) / 8);
          operands[2] = gen_rtx (CONST_INT, VOIDmode, INTVAL (operands[2]) % 8);
	}
      if (INTVAL (operands[1]) <= 8)
        return "inssb %3,%0,%2,%1";
      else if (INTVAL (operands[1]) <= 16)
	return "inssw %3,%0,%2,%1";
      else
	return "inssd %3,%0,%2,%1";
    }
  return "insd %2,%3,%0,%1";
}
}

static char *
output_160 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (GET_CODE (operands[2]) == CONST_INT)
    if (INTVAL (operands[1]) <= 8)
      return "inssb %3,%0,%2,%1";
    else if (INTVAL (operands[1]) <= 16)
      return "inssw %3,%0,%2,%1";
    else
      return "inssd %3,%0,%2,%1";
  return "insd %2,%3,%0,%1";
}
}

static char *
output_161 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (GET_CODE (operands[2]) == CONST_INT)
    if (INTVAL (operands[1]) <= 8)
      return "inssb %3,%0,%2,%1";
    else if (INTVAL (operands[1]) <= 16)
      return "inssw %3,%0,%2,%1";
    else
      return "inssd %3,%0,%2,%1";
  return "insd %2,%3,%0,%1";
}
}

static char *
output_163 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (cc_prev_status.flags & CC_Z_IN_F)
    return "bfc %l0";
  else if (cc_prev_status.flags & CC_Z_IN_NOT_F)
    return "bfs %l0";
  else return "beq %l0";
}
}

static char *
output_164 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (cc_prev_status.flags & CC_Z_IN_F)
    return "bfs %l0";
  else if (cc_prev_status.flags & CC_Z_IN_NOT_F)
    return "bfc %l0";
  else return "bne %l0";
}
}

static char *
output_173 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (cc_prev_status.flags & CC_Z_IN_F)
    return "bfs %l0";
  else if (cc_prev_status.flags & CC_Z_IN_NOT_F)
    return "bfc %l0";
  else return "bne %l0";
}
}

static char *
output_174 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (cc_prev_status.flags & CC_Z_IN_F)
    return "bfc %l0";
  else if (cc_prev_status.flags & CC_Z_IN_NOT_F)
    return "bfs %l0";
  else return "beq %l0";
}
}

static char *
output_185 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifndef JSR_ALWAYS
  if (GET_CODE (operands[0]) == MEM)
    {
      rtx temp = XEXP (operands[0], 0);
      if (CONSTANT_ADDRESS_P (temp))
	{
#ifdef ENCORE_ASM
	  return "bsr %?%0";
#else
#ifdef CALL_MEMREF_IMPLICIT
	  operands[0] = temp;
	  return "bsr %0";
#else
#ifdef GNX_V3
	  return "bsr %0";
#else
	  return "bsr %?%a0";
#endif
#endif
#endif
	}
      if (GET_CODE (XEXP (operands[0], 0)) == REG)
#if defined (GNX_V3) || defined (CALL_MEMREF_IMPLICIT)
	return "jsr %0";
#else
        return "jsr %a0";
#endif
    }
#endif /* not JSR_ALWAYS */
  return "jsr %0";
}
}

static char *
output_186 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifndef JSR_ALWAYS
  if (GET_CODE (operands[1]) == MEM)
    {
      rtx temp = XEXP (operands[1], 0);
      if (CONSTANT_ADDRESS_P (temp))
	{
#ifdef ENCORE_ASM
	  return "bsr %?%1";
#else
#ifdef CALL_MEMREF_IMPLICIT
	  operands[1] = temp;
	  return "bsr %1";
#else
#ifdef GNX_V3
	  return "bsr %1";
#else
	  return "bsr %?%a1";
#endif
#endif
#endif
	}
      if (GET_CODE (XEXP (operands[1], 0)) == REG)
#if defined (GNX_V3) || defined (CALL_MEMREF_IMPLICIT)
	return "jsr %1";
#else
        return "jsr %a1";
#endif
    }
#endif /* not JSR_ALWAYS */
  return "jsr %1";
}
}

static char *
output_197 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  ASM_OUTPUT_INTERNAL_LABEL (asm_out_file, "LI",
			     CODE_LABEL_NUMBER (operands[1]));
  return "cased %0";
}
}

static char *
output_198 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (cc_prev_status.flags & CC_Z_IN_F)
    return "sfcd %0";
  else if (cc_prev_status.flags & CC_Z_IN_NOT_F)
    return "sfsd %0";
  else return "seqd %0";
}
}

static char *
output_199 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (cc_prev_status.flags & CC_Z_IN_F)
    return "sfcw %0";
  else if (cc_prev_status.flags & CC_Z_IN_NOT_F)
    return "sfsw %0";
  else return "seqw %0";
}
}

static char *
output_200 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (cc_prev_status.flags & CC_Z_IN_F)
    return "sfcb %0";
  else if (cc_prev_status.flags & CC_Z_IN_NOT_F)
    return "sfsb %0";
  else return "seqb %0";
}
}

static char *
output_201 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (cc_prev_status.flags & CC_Z_IN_F)
    return "sfsd %0";
  else if (cc_prev_status.flags & CC_Z_IN_NOT_F)
    return "sfcd %0";
  else return "sned %0";
}
}

static char *
output_202 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (cc_prev_status.flags & CC_Z_IN_F)
    return "sfsw %0";
  else if (cc_prev_status.flags & CC_Z_IN_NOT_F)
    return "sfcw %0";
  else return "snew %0";
}
}

static char *
output_203 (operands, insn)
     rtx *operands;
     rtx insn;
{

{ if (cc_prev_status.flags & CC_Z_IN_F)
    return "sfsb %0";
  else if (cc_prev_status.flags & CC_Z_IN_NOT_F)
    return "sfcb %0";
  else return "sneb %0";
}
}

static char *
output_228 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  return "movqb 0,%0; ffsd %1,%0; bfs 1f; addqb 1,%0; 1:";
}
}

static char *
output_229 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  return "movqw 0,%0; ffsd %1,%0; bfs 1f; addqw 1,%0; 1:";
}
}

static char *
output_230 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  return "movqd 0,%0; ffsd %1,%0; bfs 1f; addqd 1,%0; 1:";
}
}

static char *
output_231 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT)
	output_asm_insn (output_move_dconst (INTVAL (operands[1]), "%$%1,tos"),
			 operands);
  else
	output_asm_insn ("movzwd %1,tos", operands);
  return "";
}
}

static char *
output_232 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT)
	output_asm_insn (output_move_dconst (INTVAL (operands[1]), "%$%1,tos"),
			 operands);
  else
	output_asm_insn ("movzbd %1,tos", operands);
  return "";
}
}

static char *
output_233 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT)
	output_asm_insn (output_move_dconst (INTVAL (operands[1]), "%$%1,tos"),
			 operands);
  else
	output_asm_insn ("movxbd %1,tos", operands);
  return "";
}
}

static char *
output_234 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT)
	output_asm_insn (output_move_dconst (INTVAL (operands[1]), "%$%1,tos"),
			 operands);
  else
	output_asm_insn ("movzbd %1,tos", operands);
  return "";
}
}

static char *
output_235 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT)
	output_asm_insn (output_move_dconst (INTVAL (operands[1]), "%$%1,0(sp)"),
			 operands);
  else
	output_asm_insn ("movd %1,0(sp)", operands);
  return "";
}
}

static char *
output_236 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT)
	output_asm_insn (output_move_dconst (INTVAL (operands[1]), "%$%1,4(sp)"),
			 operands);
  else
	output_asm_insn ("movd %1,4(sp)", operands);

  if (GET_CODE (operands[3]) == CONST_INT)
	output_asm_insn (output_move_dconst (INTVAL (operands[3]), "%$%3,0(sp)"),
			 operands);
  else
	output_asm_insn ("movd %3,0(sp)", operands);
  return "";
}
}

char * const insn_template[] =
  {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "cmpl %0,%1",
    "cmpf %0,%1",
    0,
    0,
    "movmd %1,%0,4",
    0,
    "lprd sp,%0",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "movb %1,%0",
    "movw %1,%0",
    "movb %1,%0",
    "movxwd %1,%0",
    "movxbw %1,%0",
    "movxbd %1,%0",
    "movfl %1,%0",
    "movlf %1,%0",
    "movzwd %1,%0",
    "movzbw %1,%0",
    "movzbd %1,%0",
    "movdf %1,%0",
    "movdl %1,%0",
    "movwf %1,%0",
    "movwl %1,%0",
    "movbf %1,%0",
    "truncfb %1,%0",
    "truncfw %1,%0",
    "truncfd %1,%0",
    "trunclb %1,%0",
    "trunclw %1,%0",
    "truncld %1,%0",
    "truncfb %1,%0",
    "truncfw %1,%0",
    "truncfd %1,%0",
    "trunclb %1,%0",
    "trunclw %1,%0",
    "truncld %1,%0",
    "truncfb %1,%0",
    "truncfw %1,%0",
    "truncfd %1,%0",
    "trunclb %1,%0",
    "trunclw %1,%0",
    "truncld %1,%0",
    "addl %2,%0",
    "addf %2,%0",
    0,
    "addr %c1(fp),%0",
    "addr %c1(sp),%0",
    0,
    0,
    0,
    0,
    0,
    "subl %2,%0",
    "subf %2,%0",
    0,
    0,
    0,
    0,
    0,
    0,
    "mull %2,%0",
    "mulf %2,%0",
    "muld %2,%0",
    "mulw %2,%0",
    "mulb %2,%0",
    "meid %2,%0",
    "divl %2,%0",
    "divf %2,%0",
    "quod %2,%0",
    "quow %2,%0",
    "quob %2,%0",
    0,
    0,
    0,
    "remd %2,%0",
    "remw %2,%0",
    "remb %2,%0",
    "deid %2,%0",
    "deiw %2,%0",
    "deib %2,%0",
    0,
    0,
    "andb %2,%0",
    "bicd %1,%0",
    "bicw %1,%0",
    "bicb %1,%0",
    0,
    0,
    "orb %2,%0",
    0,
    0,
    "xorb %2,%0",
    "negl %1,%0",
    "negf %1,%0",
    "negd %1,%0",
    "negw %1,%0",
    "negb %1,%0",
    "comd %1,%0",
    "comw %1,%0",
    "comb %1,%0",
    0,
    0,
    0,
    0,
    "ashd %$%n2,%0",
    "ashd %2,%0",
    0,
    "ashw %$%n2,%0",
    "ashw %2,%0",
    0,
    "ashb %$%n2,%0",
    "ashb %2,%0",
    "lshd %2,%0",
    "lshw %2,%0",
    "lshb %2,%0",
    0,
    "lshd %$%n2,%0",
    "lshd %2,%0",
    0,
    "lshw %$%n2,%0",
    "lshw %2,%0",
    0,
    "lshb %$%n2,%0",
    "lshb %2,%0",
    "rotd %2,%0",
    "rotw %2,%0",
    "rotb %2,%0",
    0,
    "rotd %$%n2,%0",
    "rotd %2,%0",
    0,
    "rotw %$%n2,%0",
    "rotw %2,%0",
    0,
    "rotb %$%n2,%0",
    "rotb %2,%0",
    0,
    "sbitd %1,%0",
    "cbitd %1,%0",
    "ibitd %1,%0",
    "ibitb %1,%0",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "br %l0",
    0,
    0,
    "bgt %l0",
    "bhi %l0",
    "blt %l0",
    "blo %l0",
    "bge %l0",
    "bhs %l0",
    "ble %l0",
    "bls %l0",
    0,
    0,
    "ble %l0",
    "bls %l0",
    "bge %l0",
    "bhs %l0",
    "blt %l0",
    "blo %l0",
    "bgt %l0",
    "bhi %l0",
    "acbd %$%n1,%0,%l2",
    "acbd %3,%0,%l2",
    0,
    0,
    0,
    "",
    "ret 0",
    "absf %1,%0",
    "absl %1,%0",
    "absd %1,%0",
    "absw %1,%0",
    "absb %1,%0",
    "nop",
    "jump %0",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "sgtd %0",
    "sgtw %0",
    "sgtb %0",
    "shid %0",
    "shiw %0",
    "shib %0",
    "sltd %0",
    "sltw %0",
    "sltb %0",
    "slod %0",
    "slow %0",
    "slob %0",
    "sged %0",
    "sgew %0",
    "sgeb %0",
    "shsd %0",
    "shsw %0",
    "shsb %0",
    "sled %0",
    "slew %0",
    "sleb %0",
    "slsd %0",
    "slsw %0",
    "slsb %0",
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
    output_3,
    output_4,
    output_5,
    output_6,
    output_7,
    0,
    0,
    output_10,
    output_11,
    0,
    output_13,
    0,
    output_15,
    output_16,
    output_17,
    output_18,
    output_19,
    0,
    output_21,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_58,
    0,
    0,
    output_61,
    output_62,
    output_63,
    output_64,
    output_65,
    0,
    0,
    output_68,
    output_69,
    output_70,
    output_71,
    output_72,
    output_73,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_85,
    output_86,
    output_87,
    0,
    0,
    0,
    0,
    0,
    0,
    output_94,
    output_95,
    0,
    0,
    0,
    0,
    output_100,
    output_101,
    0,
    output_103,
    output_104,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_114,
    output_115,
    output_116,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_150,
    0,
    0,
    0,
    0,
    output_155,
    output_156,
    output_157,
    output_158,
    output_159,
    output_160,
    output_161,
    0,
    output_163,
    output_164,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_173,
    output_174,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_185,
    output_186,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_197,
    output_198,
    output_199,
    output_200,
    output_201,
    output_202,
    output_203,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_228,
    output_229,
    output_230,
    output_231,
    output_232,
    output_233,
    output_234,
    output_235,
    output_236,
  };

rtx (*const insn_gen_function[]) () =
  {
    gen_tstsi,
    gen_tsthi,
    gen_tstqi,
    gen_tstdf,
    gen_tstsf,
    gen_cmpsi,
    gen_cmphi,
    gen_cmpqi,
    gen_cmpdf,
    gen_cmpsf,
    gen_movdf,
    gen_movsf,
    0,
    gen_movdi,
    0,
    gen_movsi,
    gen_movhi,
    gen_movstricthi,
    gen_movqi,
    gen_movstrictqi,
    gen_movstrsi,
    gen_movstrsi1,
    gen_truncsiqi2,
    gen_truncsihi2,
    gen_trunchiqi2,
    gen_extendhisi2,
    gen_extendqihi2,
    gen_extendqisi2,
    gen_extendsfdf2,
    gen_truncdfsf2,
    gen_zero_extendhisi2,
    gen_zero_extendqihi2,
    gen_zero_extendqisi2,
    gen_floatsisf2,
    gen_floatsidf2,
    gen_floathisf2,
    gen_floathidf2,
    gen_floatqisf2,
    gen_fixsfqi2,
    gen_fixsfhi2,
    gen_fixsfsi2,
    gen_fixdfqi2,
    gen_fixdfhi2,
    gen_fixdfsi2,
    gen_fixunssfqi2,
    gen_fixunssfhi2,
    gen_fixunssfsi2,
    gen_fixunsdfqi2,
    gen_fixunsdfhi2,
    gen_fixunsdfsi2,
    gen_fix_truncsfqi2,
    gen_fix_truncsfhi2,
    gen_fix_truncsfsi2,
    gen_fix_truncdfqi2,
    gen_fix_truncdfhi2,
    gen_fix_truncdfsi2,
    gen_adddf3,
    gen_addsf3,
    0,
    0,
    0,
    gen_addsi3,
    gen_addhi3,
    0,
    gen_addqi3,
    0,
    gen_subdf3,
    gen_subsf3,
    0,
    gen_subsi3,
    gen_subhi3,
    0,
    gen_subqi3,
    0,
    gen_muldf3,
    gen_mulsf3,
    gen_mulsi3,
    gen_mulhi3,
    gen_mulqi3,
    gen_umulsidi3,
    gen_divdf3,
    gen_divsf3,
    gen_divsi3,
    gen_divhi3,
    gen_divqi3,
    gen_udivsi3,
    gen_udivhi3,
    gen_udivqi3,
    gen_modsi3,
    gen_modhi3,
    gen_modqi3,
    gen_umodsi3,
    gen_umodhi3,
    gen_umodqi3,
    gen_andsi3,
    gen_andhi3,
    gen_andqi3,
    0,
    0,
    0,
    gen_iorsi3,
    gen_iorhi3,
    gen_iorqi3,
    gen_xorsi3,
    gen_xorhi3,
    gen_xorqi3,
    gen_negdf2,
    gen_negsf2,
    gen_negsi2,
    gen_neghi2,
    gen_negqi2,
    gen_one_cmplsi2,
    gen_one_cmplhi2,
    gen_one_cmplqi2,
    gen_ashlsi3,
    gen_ashlhi3,
    gen_ashlqi3,
    gen_ashrsi3,
    0,
    0,
    gen_ashrhi3,
    0,
    0,
    gen_ashrqi3,
    0,
    0,
    gen_lshlsi3,
    gen_lshlhi3,
    gen_lshlqi3,
    gen_lshrsi3,
    0,
    0,
    gen_lshrhi3,
    0,
    0,
    gen_lshrqi3,
    0,
    0,
    gen_rotlsi3,
    gen_rotlhi3,
    gen_rotlqi3,
    gen_rotrsi3,
    0,
    0,
    gen_rotrhi3,
    0,
    0,
    gen_rotrqi3,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    gen_extzv,
    0,
    0,
    gen_insv,
    gen_jump,
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
    0,
    0,
    gen_call,
    gen_call_value,
    gen_untyped_call,
    gen_blockage,
    gen_return,
    gen_abssf2,
    gen_absdf2,
    gen_abssi2,
    gen_abshi2,
    gen_absqi2,
    gen_nop,
    gen_indirect_jump,
    gen_tablejump,
    gen_seq,
    0,
    0,
    gen_sne,
    0,
    0,
    gen_sgt,
    0,
    0,
    gen_sgtu,
    0,
    0,
    gen_slt,
    0,
    0,
    gen_sltu,
    0,
    0,
    gen_sge,
    0,
    0,
    gen_sgeu,
    0,
    0,
    gen_sle,
    0,
    0,
    gen_sleu,
    0,
    0,
    gen_ffsqi2,
    gen_ffshi2,
    gen_ffssi2,
    0,
    0,
    0,
    0,
    0,
    0,
  };

char *insn_name[] =
  {
    "tstsi",
    "tsthi",
    "tstqi",
    "tstdf",
    "tstsf",
    "cmpsi",
    "cmphi",
    "cmpqi",
    "cmpdf",
    "cmpsf",
    "movdf",
    "movsf",
    "movsf+1",
    "movdi",
    "movdi+1",
    "movsi",
    "movhi",
    "movstricthi",
    "movqi",
    "movstrictqi",
    "movstrsi",
    "movstrsi1",
    "truncsiqi2",
    "truncsihi2",
    "trunchiqi2",
    "extendhisi2",
    "extendqihi2",
    "extendqisi2",
    "extendsfdf2",
    "truncdfsf2",
    "zero_extendhisi2",
    "zero_extendqihi2",
    "zero_extendqisi2",
    "floatsisf2",
    "floatsidf2",
    "floathisf2",
    "floathidf2",
    "floatqisf2",
    "fixsfqi2",
    "fixsfhi2",
    "fixsfsi2",
    "fixdfqi2",
    "fixdfhi2",
    "fixdfsi2",
    "fixunssfqi2",
    "fixunssfhi2",
    "fixunssfsi2",
    "fixunsdfqi2",
    "fixunsdfhi2",
    "fixunsdfsi2",
    "fix_truncsfqi2",
    "fix_truncsfhi2",
    "fix_truncsfsi2",
    "fix_truncdfqi2",
    "fix_truncdfhi2",
    "fix_truncdfsi2",
    "adddf3",
    "addsf3",
    "addsf3+1",
    "addsf3+2",
    "addsi3-1",
    "addsi3",
    "addhi3",
    "addhi3+1",
    "addqi3",
    "addqi3+1",
    "subdf3",
    "subsf3",
    "subsf3+1",
    "subsi3",
    "subhi3",
    "subhi3+1",
    "subqi3",
    "subqi3+1",
    "muldf3",
    "mulsf3",
    "mulsi3",
    "mulhi3",
    "mulqi3",
    "umulsidi3",
    "divdf3",
    "divsf3",
    "divsi3",
    "divhi3",
    "divqi3",
    "udivsi3",
    "udivhi3",
    "udivqi3",
    "modsi3",
    "modhi3",
    "modqi3",
    "umodsi3",
    "umodhi3",
    "umodqi3",
    "andsi3",
    "andhi3",
    "andqi3",
    "andqi3+1",
    "andqi3+2",
    "iorsi3-1",
    "iorsi3",
    "iorhi3",
    "iorqi3",
    "xorsi3",
    "xorhi3",
    "xorqi3",
    "negdf2",
    "negsf2",
    "negsi2",
    "neghi2",
    "negqi2",
    "one_cmplsi2",
    "one_cmplhi2",
    "one_cmplqi2",
    "ashlsi3",
    "ashlhi3",
    "ashlqi3",
    "ashrsi3",
    "ashrsi3+1",
    "ashrhi3-1",
    "ashrhi3",
    "ashrhi3+1",
    "ashrqi3-1",
    "ashrqi3",
    "ashrqi3+1",
    "lshlsi3-1",
    "lshlsi3",
    "lshlhi3",
    "lshlqi3",
    "lshrsi3",
    "lshrsi3+1",
    "lshrhi3-1",
    "lshrhi3",
    "lshrhi3+1",
    "lshrqi3-1",
    "lshrqi3",
    "lshrqi3+1",
    "rotlsi3-1",
    "rotlsi3",
    "rotlhi3",
    "rotlqi3",
    "rotrsi3",
    "rotrsi3+1",
    "rotrhi3-1",
    "rotrhi3",
    "rotrhi3+1",
    "rotrqi3-1",
    "rotrqi3",
    "rotrqi3+1",
    "rotrqi3+2",
    "rotrqi3+3",
    "rotrqi3+4",
    "rotrqi3+5",
    "extzv-5",
    "extzv-4",
    "extzv-3",
    "extzv-2",
    "extzv-1",
    "extzv",
    "extzv+1",
    "insv-1",
    "insv",
    "jump",
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
    "bleu+6",
    "call-6",
    "call-5",
    "call-4",
    "call-3",
    "call-2",
    "call-1",
    "call",
    "call_value",
    "untyped_call",
    "blockage",
    "return",
    "abssf2",
    "absdf2",
    "abssi2",
    "abshi2",
    "absqi2",
    "nop",
    "indirect_jump",
    "tablejump",
    "seq",
    "seq+1",
    "sne-1",
    "sne",
    "sne+1",
    "sgt-1",
    "sgt",
    "sgt+1",
    "sgtu-1",
    "sgtu",
    "sgtu+1",
    "slt-1",
    "slt",
    "slt+1",
    "sltu-1",
    "sltu",
    "sltu+1",
    "sge-1",
    "sge",
    "sge+1",
    "sgeu-1",
    "sgeu",
    "sgeu+1",
    "sle-1",
    "sle",
    "sle+1",
    "sleu-1",
    "sleu",
    "sleu+1",
    "ffsqi2-1",
    "ffsqi2",
    "ffshi2",
    "ffssi2",
    "ffssi2+1",
    "ffssi2+2",
    "ffssi2+3",
    "ffssi2+4",
    "ffssi2+5",
    "ffssi2+6",
  };
char **insn_name_ptr = insn_name;

const int insn_n_operands[] =
  {
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
    1,
    2,
    2,
    2,
    2,
    2,
    4,
    3,
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
    3,
    3,
    1,
    2,
    2,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
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
    3,
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
    3,
    3,
    3,
    3,
    2,
    2,
    2,
    2,
    2,
    2,
    4,
    4,
    4,
    4,
    4,
    4,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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
    4,
    2,
    3,
    3,
    0,
    0,
    2,
    2,
    2,
    2,
    2,
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
    2,
    2,
    4,
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
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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
    { "rm", },
    { "g", },
    { "g", },
    { "fmF", },
    { "fmF", },
    { "rmn", "rmn", },
    { "g", "g", },
    { "g", "g", },
    { "fmF", "fmF", },
    { "fmF", "fmF", },
    { "=fg<", "fFg", },
    { "=fg<", "fFg", },
    { "=m", "m", },
    { "=g<,*f,g", "gF,g,*f", },
    { "rmn", },
    { "=g<,g<,*f,g,x", "g,?xy,g,*f,rmn", },
    { "=g<,*f,g", "g,g,*f", },
    { "+r", "g", },
    { "=g<,*f,g", "g,g,*f", },
    { "+r", "g", },
    { "=g", "g", "rmn", "", },
    { "=g", "g", "rmn", },
    { "=g<", "rmn", },
    { "=g<", "rmn", },
    { "=g<", "g", },
    { "=g<", "g", },
    { "=g<", "g", },
    { "=g<", "g", },
    { "=fm<", "fmF", },
    { "=fm<", "fmF", },
    { "=g<", "g", },
    { "=g<", "g", },
    { "=g<", "g", },
    { "=fm<", "rm", },
    { "=fm<", "rm", },
    { "=fm<", "rm", },
    { "=fm<", "rm", },
    { "=fm<", "rm", },
    { "=g<", "fm", },
    { "=g<", "fm", },
    { "=g<", "fm", },
    { "=g<", "fm", },
    { "=g<", "fm", },
    { "=g<", "fm", },
    { "=g<", "fm", },
    { "=g<", "fm", },
    { "=g<", "fm", },
    { "=g<", "fm", },
    { "=g<", "fm", },
    { "=g<", "fm", },
    { "=g<", "fm", },
    { "=g<", "fm", },
    { "=g<", "fm", },
    { "=g<", "fm", },
    { "=g<", "fm", },
    { "=g<", "fm", },
    { "=fm", "%0", "fmF", },
    { "=fm", "%0", "fmF", },
    { "i", },
    { "=g<", "i", },
    { "=g<", "i", },
    { "=g,=g&<", "%0,r", "rmn,n", },
    { "=g", "%0", "g", },
    { "=r", "0", "g", },
    { "=g", "%0", "g", },
    { "=r", "0", "g", },
    { "=fm", "0", "fmF", },
    { "=fm", "0", "fmF", },
    { "i", },
    { "=g", "0", "rmn", },
    { "=g", "0", "g", },
    { "=r", "0", "g", },
    { "=g", "0", "g", },
    { "=r", "0", "g", },
    { "=fm", "%0", "fmF", },
    { "=fm", "%0", "fmF", },
    { "=g", "%0", "rmn", },
    { "=g", "%0", "g", },
    { "=g", "%0", "g", },
    { "=g", "0", "rmn", },
    { "=fm", "0", "fmF", },
    { "=fm", "0", "fmF", },
    { "=g", "0", "rmn", },
    { "=g", "0", "g", },
    { "=g", "0", "g", },
    { "=r", "0", "rmn", },
    { "=r", "0", "g", },
    { "=r", "0", "g", },
    { "=g", "0", "rmn", },
    { "=g", "0", "g", },
    { "=g", "0", "g", },
    { "=r", "0", "rmn", },
    { "=r", "0", "g", },
    { "=r", "0", "g", },
    { "=g", "%0", "rmn", },
    { "=g", "%0", "g", },
    { "=g", "%0", "g", },
    { "=g", "rmn", "0", },
    { "=g", "g", "0", },
    { "=g", "g", "0", },
    { "=g", "%0", "rmn", },
    { "=g", "%0", "g", },
    { "=g", "%0", "g", },
    { "=g", "%0", "rmn", },
    { "=g", "%0", "g", },
    { "=g", "%0", "g", },
    { "=fm<", "fmF", },
    { "=fm<", "fmF", },
    { "=g<", "rmn", },
    { "=g<", "g", },
    { "=g<", "g", },
    { "=g<", "rmn", },
    { "=g<", "g", },
    { "=g<", "g", },
    { "=g,g", "r,0", "I,rmn", },
    { "=g", "0", "rmn", },
    { "=g", "0", "rmn", },
    { "=g", "g", "g", },
    { "=g", "0", "i", },
    { "=g", "0", "r", },
    { "=g", "g", "g", },
    { "=g", "0", "i", },
    { "=g", "0", "r", },
    { "=g", "g", "g", },
    { "=g", "0", "i", },
    { "=g", "0", "r", },
    { "=g", "0", "rmn", },
    { "=g", "0", "rmn", },
    { "=g", "0", "rmn", },
    { "=g", "g", "g", },
    { "=g", "0", "i", },
    { "=g", "0", "r", },
    { "=g", "g", "g", },
    { "=g", "0", "i", },
    { "=g", "0", "r", },
    { "=g", "g", "g", },
    { "=g", "0", "i", },
    { "=g", "0", "r", },
    { "=g", "0", "rmn", },
    { "=g", "0", "rmn", },
    { "=g", "0", "rmn", },
    { "=g", "g", "g", },
    { "=g", "0", "i", },
    { "=g", "0", "r", },
    { "=g", "g", "g", },
    { "=g", "0", "i", },
    { "=g", "0", "r", },
    { "=g", "g", "g", },
    { "=g", "0", "i", },
    { "=g", "0", "r", },
    { "=g<", "p", },
    { "+g", "rmn", },
    { "+g", "rmn", },
    { "+g", "rmn", },
    { "=g", "rmn", },
    { "rm", "g", },
    { "=ro", "r", "i", "i", },
    { "=g<", "g", "i", "rK", },
    { "=g<", "g", "i", "rK", },
    { "+o", "i", "rn", "rm", },
    { "+r", "i", "rK", "rm", },
    { "+g", "i", "rK", "rm", },
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
    { "+g", "i", },
    { "+g", "i", "", "i", },
    { "m", "g", },
    { "=rf", "m", "g", },
    { "", "", "", },
    { 0 },
    { 0 },
    { "=fm<", "fmF", },
    { "=fm<", "fmF", },
    { "=g<", "rmn", },
    { "=g<", "g", },
    { "=g<", "g", },
    { 0 },
    { "r", },
    { "g", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g<", },
    { "=g", "g", },
    { "=g", "g", },
    { "=g", "g", },
    { "=m", "g", },
    { "=m", "g", },
    { "=m", "g", },
    { "=m", "g", },
    { "=m", "g", },
    { "=m", "g", "=m", "g", },
  };

const enum machine_mode insn_operand_mode[][MAX_RECOG_OPERANDS] =
  {
    { SImode, },
    { HImode, },
    { QImode, },
    { DFmode, },
    { SFmode, },
    { SImode, SImode, },
    { HImode, HImode, },
    { QImode, QImode, },
    { DFmode, DFmode, },
    { SFmode, SFmode, },
    { DFmode, DFmode, },
    { SFmode, SFmode, },
    { TImode, TImode, },
    { DImode, DImode, },
    { SImode, },
    { SImode, SImode, },
    { HImode, HImode, },
    { HImode, HImode, },
    { QImode, QImode, },
    { QImode, QImode, },
    { BLKmode, BLKmode, SImode, VOIDmode, },
    { BLKmode, BLKmode, SImode, },
    { QImode, SImode, },
    { HImode, SImode, },
    { QImode, HImode, },
    { SImode, HImode, },
    { HImode, QImode, },
    { SImode, QImode, },
    { DFmode, SFmode, },
    { SFmode, DFmode, },
    { SImode, HImode, },
    { HImode, QImode, },
    { SImode, QImode, },
    { SFmode, SImode, },
    { DFmode, SImode, },
    { SFmode, HImode, },
    { DFmode, HImode, },
    { SFmode, QImode, },
    { QImode, SFmode, },
    { HImode, SFmode, },
    { SImode, SFmode, },
    { QImode, DFmode, },
    { HImode, DFmode, },
    { SImode, DFmode, },
    { QImode, SFmode, },
    { HImode, SFmode, },
    { SImode, SFmode, },
    { QImode, DFmode, },
    { HImode, DFmode, },
    { SImode, DFmode, },
    { QImode, SFmode, },
    { HImode, SFmode, },
    { SImode, SFmode, },
    { QImode, DFmode, },
    { HImode, DFmode, },
    { SImode, DFmode, },
    { DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, },
    { SImode, },
    { SImode, SImode, },
    { SImode, SImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { QImode, QImode, QImode, },
    { DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, },
    { SImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { QImode, QImode, QImode, },
    { DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { DImode, SImode, SImode, },
    { DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { SImode, DImode, SImode, },
    { HImode, DImode, HImode, },
    { QImode, DImode, QImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { SImode, DImode, SImode, },
    { HImode, DImode, HImode, },
    { QImode, DImode, QImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { DFmode, DFmode, },
    { SFmode, SFmode, },
    { SImode, SImode, },
    { HImode, HImode, },
    { QImode, QImode, },
    { SImode, SImode, },
    { HImode, HImode, },
    { QImode, QImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, SImode, },
    { QImode, QImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, SImode, },
    { HImode, HImode, SImode, },
    { HImode, HImode, SImode, },
    { QImode, QImode, SImode, },
    { QImode, QImode, SImode, },
    { QImode, QImode, SImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, SImode, },
    { QImode, QImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, SImode, },
    { HImode, HImode, SImode, },
    { HImode, HImode, SImode, },
    { QImode, QImode, SImode, },
    { QImode, QImode, SImode, },
    { QImode, QImode, SImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, SImode, },
    { QImode, QImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, SImode, },
    { HImode, HImode, SImode, },
    { HImode, HImode, SImode, },
    { QImode, QImode, SImode, },
    { QImode, QImode, SImode, },
    { QImode, QImode, SImode, },
    { SImode, QImode, },
    { SImode, SImode, },
    { SImode, SImode, },
    { SImode, SImode, },
    { QImode, QImode, },
    { SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { SImode, QImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { QImode, SImode, SImode, SImode, },
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
    { SImode, SImode, },
    { SImode, SImode, VOIDmode, SImode, },
    { QImode, QImode, },
    { VOIDmode, QImode, QImode, },
    { VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode },
    { VOIDmode },
    { SFmode, SFmode, },
    { DFmode, DFmode, },
    { SImode, SImode, },
    { HImode, HImode, },
    { QImode, QImode, },
    { VOIDmode },
    { SImode, },
    { SImode, },
    { SImode, },
    { HImode, },
    { QImode, },
    { SImode, },
    { HImode, },
    { QImode, },
    { SImode, },
    { HImode, },
    { QImode, },
    { SImode, },
    { HImode, },
    { QImode, },
    { SImode, },
    { HImode, },
    { QImode, },
    { SImode, },
    { HImode, },
    { QImode, },
    { SImode, },
    { HImode, },
    { QImode, },
    { SImode, },
    { HImode, },
    { QImode, },
    { SImode, },
    { HImode, },
    { QImode, },
    { SImode, },
    { HImode, },
    { QImode, },
    { QImode, SImode, },
    { HImode, SImode, },
    { SImode, SImode, },
    { VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
  };

const char insn_operand_strict_low[][MAX_RECOG_OPERANDS] =
  {
    { 0, },
    { 0, },
    { 0, },
    { 0, },
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
    { 0, },
    { 0, 0, },
    { 0, 0, },
    { 1, 0, },
    { 0, 0, },
    { 1, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
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
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 1, 0, 0, },
    { 0, 0, 0, },
    { 1, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 1, 0, 0, },
    { 0, 0, 0, },
    { 1, 0, 0, },
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
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
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
    { 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0 },
    { 0 },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0 },
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
    { 0, },
    { 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, 0, },
  };

extern int nonimmediate_operand ();
extern int general_operand ();
extern int memory_operand ();
extern int immediate_operand ();
extern int register_operand ();
extern int reg_or_mem_operand ();
extern int address_operand ();
extern int const_int_operand ();

int (*const insn_operand_predicate[][MAX_RECOG_OPERANDS])() =
  {
    { nonimmediate_operand, },
    { nonimmediate_operand, },
    { nonimmediate_operand, },
    { general_operand, },
    { general_operand, },
    { nonimmediate_operand, general_operand, },
    { nonimmediate_operand, general_operand, },
    { nonimmediate_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { memory_operand, memory_operand, },
    { general_operand, general_operand, },
    { general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, general_operand, 0, },
    { general_operand, general_operand, general_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
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
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { immediate_operand, },
    { general_operand, immediate_operand, },
    { general_operand, immediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { immediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, nonimmediate_operand, nonimmediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { register_operand, reg_or_mem_operand, general_operand, },
    { register_operand, reg_or_mem_operand, general_operand, },
    { register_operand, reg_or_mem_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { register_operand, reg_or_mem_operand, general_operand, },
    { register_operand, reg_or_mem_operand, general_operand, },
    { register_operand, reg_or_mem_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, immediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, immediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, immediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, immediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, immediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, immediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, immediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, immediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, immediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, address_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, register_operand, const_int_operand, const_int_operand, },
    { general_operand, register_operand, const_int_operand, general_operand, },
    { general_operand, general_operand, const_int_operand, general_operand, },
    { memory_operand, const_int_operand, general_operand, general_operand, },
    { register_operand, const_int_operand, general_operand, general_operand, },
    { general_operand, const_int_operand, general_operand, general_operand, },
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
    { general_operand, const_int_operand, },
    { general_operand, const_int_operand, 0, const_int_operand, },
    { memory_operand, general_operand, },
    { 0, memory_operand, general_operand, },
    { 0, 0, 0, },
    { 0 },
    { 0 },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { 0 },
    { register_operand, },
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
    { general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, 0, },
  };

const int insn_n_alternatives[] =
  {
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
    3,
    1,
    5,
    3,
    1,
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
    1,
    1,
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
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
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
  };

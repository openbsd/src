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
output_44 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return output_scc_insn (operands, insn); 
}

static char *
output_55 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  return output_cbranch (operands[0], 1, 0,
			 final_sequence && INSN_ANNULLED_BRANCH_P (insn),
			 ! final_sequence);
}
}

static char *
output_56 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  return output_cbranch (operands[0], 1, 1,
			 final_sequence && INSN_ANNULLED_BRANCH_P (insn),
			 ! final_sequence);
}
}

static char *
output_59 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_59[] = {
    "mov %1,%0",
    "fmovs %1,%0",
    "sethi %%hi(%a1),%0",
    "ld %1,%0",
    "ld %1,%0",
    "st %r1,%0",
    "st %r1,%0",
  };
  return strings_59[which_alternative];
}

static char *
output_61 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx op0 = operands[0];
  rtx op1 = operands[1];

  if (GET_CODE (op1) == CONST_INT)
    {
      operands[0] = operand_subword (op0, 1, 0, DImode);
      output_asm_insn ("sethi %%hi(%a1),%0", operands);

      operands[0] = operand_subword (op0, 0, 0, DImode);
      if (INTVAL (op1) < 0)
	return "mov -1,%0";
      else
	return "mov 0,%0";
    }
  else if (GET_CODE (op1) == CONST_DOUBLE)
    {
      operands[0] = operand_subword (op0, 1, 0, DImode);
      operands[1] = gen_rtx (CONST_INT, VOIDmode, CONST_DOUBLE_LOW (op1));
      output_asm_insn ("sethi %%hi(%a1),%0", operands);

      operands[0] = operand_subword (op0, 0, 0, DImode);
      operands[1] = gen_rtx (CONST_INT, VOIDmode, CONST_DOUBLE_HIGH (op1));
      return singlemove_string (operands);
    }
  else
    abort ();
  return "";
}
}

static char *
output_65 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  /* Don't output a 64 bit constant, since we can't trust the assembler to
     handle it correctly.  */
  if (GET_CODE (operands[2]) == CONST_DOUBLE)
    operands[2] = gen_rtx (CONST_INT, VOIDmode, CONST_DOUBLE_LOW (operands[2]));
  return "or %R1,%%lo(%a2),%R0";
}
}

static char *
output_70 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_70[] = {
    "mov %1,%0",
    "sethi %%hi(%a1),%0",
    "lduh %1,%0",
    "sth %r1,%0",
  };
  return strings_70[which_alternative];
}

static char *
output_74 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_74[] = {
    "mov %1,%0",
    "sethi %%hi(%a1),%0",
    "ldub %1,%0",
    "stb %r1,%0",
  };
  return strings_74[which_alternative];
}

static char *
output_77 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  switch (which_alternative)
    {
    case 0:
      return output_move_quad (operands);
    case 1:
      return output_fp_move_quad (operands);
    case 2:
      operands[1] = adj_offsettable_operand (operands[0], 4);
      operands[2] = adj_offsettable_operand (operands[0], 8);
      operands[3] = adj_offsettable_operand (operands[0], 12);
      return "st %%g0,%0\n\tst %%g0,%1\n\tst %%g0,%2\n\tst %%g0,%3";
    }
}
}

static char *
output_79 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[0]) || FP_REG_P (operands[1]))
    return output_fp_move_quad (operands);
  return output_move_quad (operands);
}
}

static char *
output_80 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[0]) || FP_REG_P (operands[1]))
    return output_fp_move_quad (operands);
  return output_move_quad (operands);
}
}

static char *
output_81 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  output_asm_insn ("sethi %%hi(%a0),%2", operands);
  if (which_alternative == 0)
    return "std %1,[%2+%%lo(%a0)]\n\tstd %S1,[%2+%%lo(%a0+8)]";
  else
    return "st %%g0,[%2+%%lo(%a0)]\n\tst %%g0,[%2+%%lo(%a0+4)]\n\t st %%g0,[%2+%%lo(%a0+8)]\n\tst %%g0,[%2+%%lo(%a0+12)]";
}
}

static char *
output_82 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  switch (which_alternative)
    {
    case 0:
      return output_move_double (operands);
    case 1:
      return output_fp_move_double (operands);
    case 2:
      operands[1] = adj_offsettable_operand (operands[0], 4);
      return "st %%g0,%0\n\tst %%g0,%1";
    }
}
}

static char *
output_84 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[0]) || FP_REG_P (operands[1]))
    return output_fp_move_double (operands);
  return output_move_double (operands);
}
}

static char *
output_85 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return output_move_double (operands);
}

static char *
output_87 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  output_asm_insn ("sethi %%hi(%a0),%2", operands);
  if (which_alternative == 0)
    return "std %1,[%2+%%lo(%a0)]";
  else
    return "st %%g0,[%2+%%lo(%a0)]\n\tst %%g0,[%2+%%lo(%a0+4)]";
}
}

static char *
output_89 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (FP_REG_P (operands[0]) || FP_REG_P (operands[1]))
    return output_fp_move_double (operands);
  return output_move_double (operands);
}
}

static char *
output_90 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  switch (which_alternative)
    {
    case 0:
      return singlemove_string (operands);
    case 1:
      return "ld %1,%0";
    case 2:
      return "st %%g0,%0";
    }
}
}

static char *
output_92 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_92[] = {
    "fmovs %1,%0",
    "mov %1,%0",
    "ld %1,%0",
    "ld %1,%0",
    "st %r1,%0",
    "st %r1,%0",
  };
  return strings_92[which_alternative];
}

static char *
output_93 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_93[] = {
    "mov %1,%0",
    "ld %1,%0",
    "st %r1,%0",
  };
  return strings_93[which_alternative];
}

static char *
output_98 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_98[] = {
    "and %1,0xff,%0",
    "ldub %1,%0",
  };
  return strings_98[which_alternative];
}

static char *
output_100 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_100[] = {
    "and %1,0xff,%0",
    "ldub %1,%0",
  };
  return strings_100[which_alternative];
}

static char *
output_111 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  int len = INTVAL (operands[1]);
  int pos = 32 - INTVAL (operands[2]) - len;
  unsigned mask = ((1 << len) - 1) << pos;

  operands[1] = gen_rtx (CONST_INT, VOIDmode, mask);
  return "andcc %0,%1,%%g0";
}
}

static char *
output_124 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx op2 = operands[2];

  /* If constant is positive, upper bits zeroed, otherwise unchanged.
     Give the assembler a chance to pick the move instruction. */
  if (GET_CODE (op2) == CONST_INT)
    {
      int sign = INTVAL (op2);
      if (sign < 0)
	return "addcc %R1,%2,%R0\n\taddx %1,-1,%0";
      return "addcc %R1,%2,%R0\n\taddx %1,0,%0";
    }
  else if (GET_CODE (op2) == CONST_DOUBLE)
    {
      int sign = CONST_DOUBLE_HIGH (op2);
      operands[2] = gen_rtx (CONST_INT, VOIDmode,
			     CONST_DOUBLE_LOW (operands[1]));
      if (sign < 0)
        return "addcc %R1,%2,%R0\n\taddx %1,-1,%0";
      return "addcc %R1,%2,%R0\n\taddx %1,0,%0";
    }
  return "addcc %R1,%R2,%R0\n\taddx %1,%2,%0";
}
}

static char *
output_128 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx op2 = operands[2];

  /* If constant is positive, upper bits zeroed, otherwise unchanged.
     Give the assembler a chance to pick the move instruction. */
  if (GET_CODE (op2) == CONST_INT)
    {
      int sign = INTVAL (op2);
      if (sign < 0)
	return "subcc %R1,%2,%R0\n\tsubx %1,-1,%0";
      return "subcc %R1,%2,%R0\n\tsubx %1,0,%0";
    }
  else if (GET_CODE (op2) == CONST_DOUBLE)
    {
      int sign = CONST_DOUBLE_HIGH (op2);
      operands[2] = gen_rtx (CONST_INT, VOIDmode,
			     CONST_DOUBLE_LOW (operands[1]));
      if (sign < 0)
        return "subcc %R1,%2,%R0\n\tsubx %1,-1,%0";
      return "subcc %R1,%2,%R0\n\tsubx %1,0,%0";
    }
  return "subcc %R1,%R2,%R0\n\tsubx %1,%2,%0";
}
}

static char *
output_145 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx op2 = operands[2];

  /* If constant is positive, upper bits zeroed, otherwise unchanged.
     Give the assembler a chance to pick the move instruction. */
  if (GET_CODE (op2) == CONST_INT)
    {
      int sign = INTVAL (op2);
      if (sign < 0)
	return "mov %1,%0\n\tand %R1,%2,%R0";
      return "mov 0,%0\n\tand %R1,%2,%R0";
    }
  else if (GET_CODE (op2) == CONST_DOUBLE)
    {
      int sign = CONST_DOUBLE_HIGH (op2);
      operands[2] = gen_rtx (CONST_INT, VOIDmode,
			     CONST_DOUBLE_LOW (operands[1]));
      if (sign < 0)
	return "mov %1,%0\n\tand %R1,%2,%R0";
      return "mov 0,%0\n\tand %R1,%2,%R0";
    }
  return "and %1,%2,%0\n\tand %R1,%R2,%R0";
}
}

static char *
output_151 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx op2 = operands[2];

  /* If constant is positive, upper bits zeroed, otherwise unchanged.
     Give the assembler a chance to pick the move instruction. */
  if (GET_CODE (op2) == CONST_INT)
    {
      int sign = INTVAL (op2);
      if (sign < 0)
	return "mov -1,%0\n\tor %R1,%2,%R0";
      return "mov %1,%0\n\tor %R1,%2,%R0";
    }
  else if (GET_CODE (op2) == CONST_DOUBLE)
    {
      int sign = CONST_DOUBLE_HIGH (op2);
      operands[2] = gen_rtx (CONST_INT, VOIDmode,
			     CONST_DOUBLE_LOW (operands[1]));
      if (sign < 0)
	return "mov -1,%0\n\tor %R1,%2,%R0";
      return "mov %1,%0\n\tor %R1,%2,%R0";
    }
  return "or %1,%2,%0\n\tor %R1,%R2,%R0";
}
}

static char *
output_157 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx op2 = operands[2];

  /* If constant is positive, upper bits zeroed, otherwise unchanged.
     Give the assembler a chance to pick the move instruction. */
  if (GET_CODE (op2) == CONST_INT)
    {
      int sign = INTVAL (op2);
      if (sign < 0)
	return "xor %1,-1,%0\n\txor %R1,%2,%R0";
      return "mov %1,%0\n\txor %R1,%2,%R0";
    }
  else if (GET_CODE (op2) == CONST_DOUBLE)
    {
      int sign = CONST_DOUBLE_HIGH (op2);
      operands[2] = gen_rtx (CONST_INT, VOIDmode,
			     CONST_DOUBLE_LOW (operands[1]));
      if (sign < 0)
	return "xor %1,-1,%0\n\txor %R1,%2,%R0";
      return "mov %1,%0\n\txor %R1,%2,%R0";
    }
  return "xor %1,%2,%0\n\txor %R1,%R2,%R0";
}
}

static char *
output_174 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx op1 = operands[1];

  if (GET_CODE (op1) == CONST_INT)
    {
      int sign = INTVAL (op1);
      if (sign < 0)
	return "xnor %%g0,%1,%R0\n\txnor %%g0,-1,%0";
      return "xnor %%g0,%1,%R0\n\txnor %%g0,0,%0";
    }
  else if (GET_CODE (op1) == CONST_DOUBLE)
    {
      int sign = CONST_DOUBLE_HIGH (op1);
      operands[1] = gen_rtx (CONST_INT, VOIDmode,
			     CONST_DOUBLE_LOW (operands[1]));
      if (sign < 0)
	return "xnor %%g0,%1,%R0\n\txnor %%g0,-1,%0";
      return "xnor %%g0,%1,%R0\n\txnor %%g0,0,%0";
    }
  return "xnor %%g0,%1,%0\n\txnor %%g0,%R1,%R0";
}
}

static char *
output_192 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_192[] = {
    "fnegs %0,%0",
    "fnegs %1,%0\n\tfmovs %R1,%R0\n\tfmovs %S1,%S0\n\tfmovs %T1,%T0",
  };
  return strings_192[which_alternative];
}

static char *
output_193 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_193[] = {
    "fnegs %0,%0",
    "fnegs %1,%0\n\tfmovs %R1,%R0",
  };
  return strings_193[which_alternative];
}

static char *
output_195 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_195[] = {
    "fabss %0,%0",
    "fabss %1,%0\n\tfmovs %R1,%R0\n\tfmovs %S1,%S0\n\tfmovs %T1,%T0",
  };
  return strings_195[which_alternative];
}

static char *
output_196 (operands, insn)
     rtx *operands;
     rtx insn;
{
  static /*const*/ char *const strings_196[] = {
    "fabss %0,%0",
    "fabss %1,%0\n\tfmovs %R1,%R0",
  };
  return strings_196[which_alternative];
}

static char *
output_205 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  operands[4] = GEN_INT (32 - INTVAL (operands[2]));
  return "srl %R1,%4,%3\n\tsll %R1,%2,%R0\n\tsll %1,%2,%0\n\tor %3,%0,%0";
}
}

static char *
output_206 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  operands[4] = GEN_INT (INTVAL (operands[2]) - 32);
  return "sll %R1,%4,%0\n\tmov %%g0,%R0";
}
}

static char *
output_210 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  operands[4] = GEN_INT (32 - INTVAL (operands[2]));
  return "sll %1,%4,%3\n\tsrl %1,%2,%0\n\tsrl %R1,%2,%R0\n\tor %3,%R0,%R0";
}
}

static char *
output_211 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  operands[4] = GEN_INT (INTVAL (operands[2]) - 32);
  return "srl %1,%4,%R0\n\tmov %%g0,%0";
}
}

static char *
output_218 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  return "call %a0,%1%#";
}
}

static char *
output_219 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  return "call %a0,%1\n\tnop\n\tunimp %2";
}
}

static char *
output_221 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  return "call %a1,%2%#";
}
}

static char *
output_223 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  operands[2] = adj_offsettable_operand (operands[1], 8);
  return "call %a0,0\n\tnop\n\tnop\n\tstd %%o0,%1\n\tst %%f0,%2";
}
}

static char *
output_226 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return output_return (operands);
}

static char *
output_258 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  warning ("bad peephole");
  if (! MEM_VOLATILE_P (operands[1]))
    abort ();
  return "ldsh %1,%2";
}
}

static char *
output_259 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  warning ("bad peephole");
  if (! MEM_VOLATILE_P (operands[1]))
    abort ();
  return "ldsb %1,%2";
}
}

static char *
output_260 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  /* Go by way of output_move_double in case the register in operand 2
     is not properly aligned for ldd.  */
  operands[1] = gen_rtx (MEM, DFmode,
			 gen_rtx (LO_SUM, SImode, operands[0], operands[1]));
  operands[0] = operands[2];
  return output_move_double (operands);
}
}

static char *
output_262 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (current_function_returns_struct)
    return "jmp %%i7+12\n\trestore %%g0,%1,%Y0";
  else
    return "ret\n\trestore %%g0,%1,%Y0";
}
}

static char *
output_263 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (current_function_returns_struct)
    return "jmp %%i7+12\n\trestore %%g0,%1,%Y0";
  else
    return "ret\n\trestore %%g0,%1,%Y0";
}
}

static char *
output_264 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (current_function_returns_struct)
    return "jmp %%i7+12\n\trestore %%g0,%1,%Y0";
  else
    return "ret\n\trestore %%g0,%1,%Y0";
}
}

static char *
output_265 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (current_function_returns_struct)
    return "jmp %%i7+12\n\trestore %%g0,%1,%Y0";
  else
    return "ret\n\trestore %%g0,%1,%Y0";
}
}

static char *
output_266 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (current_function_returns_struct)
    return "jmp %%i7+12\n\trestore %r1,%2,%Y0";
  else
    return "ret\n\trestore %r1,%2,%Y0";
}
}

static char *
output_268 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  return "call %a1,%2\n\tadd %%o7,(%l3-.-4),%%o7";
}
}

static char *
output_269 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  return "call %a0,%1\n\tadd %%o7,(%l2-.-4),%%o7";
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
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "cmp %r0,%1",
    "fcmped %0,%1",
    "fcmpes %0,%1",
    "fcmpeq %0,%1",
    "fcmpd %0,%1",
    "fcmps %0,%1",
    "fcmpq %0,%1",
    "subcc %%g0,%1,%%g0\n\taddx %%g0,0,%0",
    "subcc %%g0,%1,%%g0\n\tsubx %%g0,0,%0",
    "subcc %%g0,%1,%%g0\n\tsubx %%g0,-1,%0",
    "subcc %%g0,%1,%%g0\n\taddx %%g0,-1,%0",
    "subcc %%g0,%1,%%g0\n\taddx %2,0,%0",
    "subcc %%g0,%1,%%g0\n\tsubx %2,0,%0",
    "subcc %%g0,%1,%%g0\n\tsubx %2,-1,%0",
    "subcc %%g0,%1,%%g0\n\taddx %2,-1,%0",
    "addx %%g0,0,%0",
    "subx %%g0,0,%0",
    "subx %%g0,%1,%0",
    "subx %%g0,%1,%0",
    "subx %%g0,-1,%0",
    "addx %%g0,-1,%0",
    "addx %%g0,%1,%0",
    "addx %1,%2,%0",
    "subx %1,0,%0",
    "subx %1,%2,%0",
    "subx %1,%2,%0",
    "subx %1,-1,%0",
    "addx %1,-1,%0",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "\n1:\n\tcall 2f\n\tsethi %%hi(%l1-1b),%0\n2:\tor %0,%%lo(%l1-1b),%0\n\tadd %0,%%o7,%0",
    0,
    "sethi %%hi(%a1),%0",
    "sethi %%hi(%a1),%0",
    "sethi %%hi(%a1),%0",
    0,
    "or %1,%%lo(%a2),%0",
    "or %1,%%lo(%a2),%0",
    "sethi %%hi(%a0),%2\n\tst %r1,[%2+%%lo(%a0)]",
    0,
    0,
    "or %1,%%lo(%a2),%0",
    "sethi %%hi(%a0),%2\n\tsth %r1,[%2+%%lo(%a0)]",
    0,
    0,
    "or %1,%%lo(%a2),%0",
    "sethi %%hi(%a0),%2\n\tstb %r1,[%2+%%lo(%a0)]",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "sethi %%hi(%a0),%2\n\tst %r1,[%2+%%lo(%a0)]",
    0,
    "lduh %1,%0",
    0,
    0,
    0,
    0,
    "andcc %0,0xff,%%g0",
    "andcc %1,0xff,%0",
    "andcc %0,0xff,%%g0",
    "andcc %1,0xff,%0",
    0,
    "ldsh %1,%0",
    0,
    "ldsb %1,%0",
    0,
    "ldsb %1,%0",
    0,
    "fstod %1,%0",
    "fstoq %1,%0",
    "fdtoq %1,%0",
    "fdtos %1,%0",
    "fqtos %1,%0",
    "fqtod %1,%0",
    "fitos %1,%0",
    "fitod %1,%0",
    "fitoq %1,%0",
    "fstoi %1,%0",
    "fdtoi %1,%0",
    "fqtoi %1,%0",
    0,
    "add %1,%2,%0",
    "addcc %0,%1,%%g0",
    "addcc %1,%2,%0",
    0,
    "sub %1,%2,%0",
    "subcc %0,%1,%%g0",
    "subcc %1,%2,%0",
    "smul %1,%2,%0",
    "smulcc %1,%2,%0",
    0,
    "smul %1,%2,%R0\n\trd %%y,%0",
    "smul %1,%2,%R0\n\trd %%y,%0",
    0,
    "umul %1,%2,%R0\n\trd %%y,%0",
    "umul %1,%2,%R0\n\trd %%y,%0",
    "sra %1,31,%3\n\twr %%g0,%3,%%y\n\tnop\n\tnop\n\tnop\n\tsdiv %1,%2,%0",
    "sra %1,31,%3\n\twr %%g0,%3,%%y\n\tnop\n\tnop\n\tnop\n\tsdivcc %1,%2,%0",
    "wr %%g0,%%g0,%%y\n\tnop\n\tnop\n\tnop\n\tudiv %1,%2,%0",
    "wr %%g0,%%g0,%%y\n\tnop\n\tnop\n\tnop\n\tudivcc %1,%2,%0",
    0,
    0,
    "and %1,%2,%0",
    0,
    "andn %2,%1,%0\n\tandn %R2,%R1,%R0",
    "andn %2,%1,%0",
    0,
    0,
    "or %1,%2,%0",
    0,
    "orn %2,%1,%0\n\torn %R2,%R1,%R0",
    "orn %2,%1,%0",
    0,
    0,
    "xor %r1,%2,%0",
    0,
    0,
    "xnor %1,%2,%0\n\txnor %R1,%R2,%R0",
    "xnor %r1,%2,%0",
    "%A2cc %0,%1,%%g0",
    "%A3cc %1,%2,%0",
    "xnorcc %r0,%1,%%g0",
    "xnorcc %r1,%2,%0",
    "%B2cc %r1,%0,%%g0",
    "%B3cc %r2,%1,%0",
    "subcc %%g0,%R1,%R0\n\tsubx %%g0,%1,%0",
    "sub %%g0,%1,%0",
    "subcc %%g0,%0,%%g0",
    "subcc %%g0,%1,%0",
    0,
    0,
    "xnor %%g0,%1,%0",
    "xnorcc %%g0,%0,%%g0",
    "xnorcc %%g0,%1,%0",
    "faddq %1,%2,%0",
    "faddd %1,%2,%0",
    "fadds %1,%2,%0",
    "fsubq %1,%2,%0",
    "fsubd %1,%2,%0",
    "fsubs %1,%2,%0",
    "fmulq %1,%2,%0",
    "fmuld %1,%2,%0",
    "fmuls %1,%2,%0",
    "fsmuld %1,%2,%0",
    "fdmulq %1,%2,%0",
    "fdivq %1,%2,%0",
    "fdivd %1,%2,%0",
    "fdivs %1,%2,%0",
    0,
    0,
    "fnegs %1,%0",
    0,
    0,
    "fabss %1,%0",
    "fsqrtq %1,%0",
    "fsqrtd %1,%0",
    "fsqrts %1,%0",
    0,
    "subcc %1,32,%%g0\n\taddx %%g0,0,%R0\n\txor %R0,1,%0\n\tsll %R0,%1,%R0\n\tsll %0,%1,%0",
    "sll %1,%2,%0",
    0,
    0,
    0,
    "sra %1,%2,%0",
    "srl %1,%2,%0",
    0,
    0,
    0,
    "b%* %l0%(",
    0,
    "jmp %%o7+%0%#",
    "jmp %a0%#",
    "call %l0%#",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "cmp %1,0\n\tbe,a .+8\n\tadd %0,4,%0",
    0,
    "nop",
    "jmp %a0%#",
    0,
    "ta 3",
    "jmp %%o0+0\n\trestore",
    "sub %%g0,%1,%0\n\tand %0,%1,%0\n\tscan %0,0,%0\n\tmov 32,%2\n\tsub %2,%0,%0\n\tsra %0,31,%2\n\tand %2,31,%2\n\tadd %2,%0,%0",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "ldd %1,%0",
    "std %1,%0",
    "ldd %1,%0",
    "std %1,%0",
    "ldd %3,%2",
    "std %3,%2",
    "ldd %3,%2",
    "std %3,%2",
    "orcc %1,%%g0,%0",
    "ldsh %1,%0\n\torcc %0,%%g0,%2",
    "ldsb %1,%0\n\torcc %0,%%g0,%2",
    0,
    0,
    0,
    "ld [%0+%%lo(%a1)],%2",
    0,
    0,
    0,
    0,
    0,
    "ret\n\tfmovs %0,%%f0",
    0,
    0,
    "subxcc %r1,0,%0",
  };

char *(*const insn_outfun[])() =
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
    output_44,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_55,
    output_56,
    0,
    0,
    output_59,
    0,
    output_61,
    0,
    0,
    0,
    output_65,
    0,
    0,
    0,
    0,
    output_70,
    0,
    0,
    0,
    output_74,
    0,
    0,
    output_77,
    0,
    output_79,
    output_80,
    output_81,
    output_82,
    0,
    output_84,
    output_85,
    0,
    output_87,
    0,
    output_89,
    output_90,
    0,
    output_92,
    output_93,
    0,
    0,
    0,
    0,
    output_98,
    0,
    output_100,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_111,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_124,
    0,
    0,
    0,
    output_128,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_145,
    0,
    0,
    0,
    0,
    0,
    output_151,
    0,
    0,
    0,
    0,
    0,
    output_157,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_192,
    output_193,
    0,
    output_195,
    output_196,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_205,
    output_206,
    0,
    0,
    0,
    output_210,
    output_211,
    0,
    0,
    0,
    0,
    0,
    0,
    output_218,
    output_219,
    0,
    output_221,
    0,
    output_223,
    0,
    0,
    output_226,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_258,
    output_259,
    output_260,
    0,
    output_262,
    output_263,
    output_264,
    output_265,
    output_266,
    0,
    output_268,
    output_269,
    0,
  };

rtx (*const insn_gen_function[]) () =
  {
    gen_cmpsi,
    gen_cmpsf,
    gen_cmpdf,
    gen_cmptf,
    gen_seq_special,
    gen_sne_special,
    gen_seq,
    gen_sne,
    gen_sgt,
    gen_slt,
    gen_sge,
    gen_sle,
    gen_sgtu,
    gen_sltu,
    gen_sgeu,
    gen_sleu,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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
    gen_movsi,
    gen_reload_insi,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    gen_movhi,
    0,
    0,
    0,
    gen_movqi,
    0,
    0,
    0,
    0,
    gen_movtf,
    0,
    0,
    0,
    0,
    gen_movdf,
    0,
    0,
    0,
    0,
    gen_movdi,
    0,
    0,
    gen_movsf,
    0,
    0,
    0,
    gen_zero_extendhisi2,
    0,
    gen_zero_extendqihi2,
    0,
    gen_zero_extendqisi2,
    0,
    0,
    0,
    0,
    0,
    gen_extendhisi2,
    0,
    gen_extendqihi2,
    0,
    gen_extendqisi2,
    0,
    0,
    gen_extendsfdf2,
    gen_extendsftf2,
    gen_extenddftf2,
    gen_truncdfsf2,
    gen_trunctfsf2,
    gen_trunctfdf2,
    gen_floatsisf2,
    gen_floatsidf2,
    gen_floatsitf2,
    gen_fix_truncsfsi2,
    gen_fix_truncdfsi2,
    gen_fix_trunctfsi2,
    gen_adddi3,
    gen_addsi3,
    0,
    0,
    gen_subdi3,
    gen_subsi3,
    0,
    0,
    gen_mulsi3,
    0,
    gen_mulsidi3,
    0,
    gen_const_mulsidi3,
    gen_umulsidi3,
    0,
    gen_const_umulsidi3,
    gen_divsi3,
    0,
    gen_udivsi3,
    0,
    gen_anddi3,
    0,
    gen_andsi3,
    0,
    0,
    0,
    gen_iordi3,
    0,
    gen_iorsi3,
    0,
    0,
    0,
    gen_xordi3,
    0,
    gen_xorsi3,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    gen_negdi2,
    gen_negsi2,
    0,
    0,
    gen_one_cmpldi2,
    0,
    gen_one_cmplsi2,
    0,
    0,
    gen_addtf3,
    gen_adddf3,
    gen_addsf3,
    gen_subtf3,
    gen_subdf3,
    gen_subsf3,
    gen_multf3,
    gen_muldf3,
    gen_mulsf3,
    0,
    0,
    gen_divtf3,
    gen_divdf3,
    gen_divsf3,
    gen_negtf2,
    gen_negdf2,
    gen_negsf2,
    gen_abstf2,
    gen_absdf2,
    gen_abssf2,
    gen_sqrttf2,
    gen_sqrtdf2,
    gen_sqrtsf2,
    gen_ashldi3,
    0,
    gen_ashlsi3,
    gen_lshldi3,
    0,
    0,
    gen_ashrsi3,
    gen_lshrsi3,
    gen_lshrdi3,
    0,
    0,
    gen_jump,
    gen_tablejump,
    gen_pic_tablejump,
    0,
    0,
    gen_call,
    0,
    0,
    gen_call_value,
    0,
    gen_untyped_call,
    0,
    gen_untyped_return,
    gen_update_return,
    gen_return,
    gen_nop,
    gen_indirect_jump,
    gen_nonlocal_goto,
    gen_flush_register_windows,
    gen_goto_handler_and_restore,
    gen_ffssi2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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

char *insn_name[] =
  {
    "cmpsi",
    "cmpsf",
    "cmpdf",
    "cmptf",
    "seq_special",
    "sne_special",
    "seq",
    "sne",
    "sgt",
    "slt",
    "sge",
    "sle",
    "sgtu",
    "sltu",
    "sgeu",
    "sleu",
    "sleu+1",
    "sleu+2",
    "sleu+3",
    "sleu+4",
    "sleu+5",
    "sleu+6",
    "sleu+7",
    "sleu+8",
    "sleu+9",
    "sleu+10",
    "sleu+11",
    "sleu+12",
    "sleu+13",
    "sleu+14",
    "sleu+15",
    "beq-14",
    "beq-13",
    "beq-12",
    "beq-11",
    "beq-10",
    "beq-9",
    "beq-8",
    "beq-7",
    "beq-6",
    "beq-5",
    "beq-4",
    "beq-3",
    "beq-2",
    "beq-1",
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
    "movsi-1",
    "movsi",
    "reload_insi",
    "reload_insi+1",
    "reload_insi+2",
    "reload_insi+3",
    "reload_insi+4",
    "reload_insi+5",
    "movhi-5",
    "movhi-4",
    "movhi-3",
    "movhi-2",
    "movhi-1",
    "movhi",
    "movhi+1",
    "movhi+2",
    "movqi-1",
    "movqi",
    "movqi+1",
    "movqi+2",
    "movtf-2",
    "movtf-1",
    "movtf",
    "movtf+1",
    "movtf+2",
    "movdf-2",
    "movdf-1",
    "movdf",
    "movdf+1",
    "movdf+2",
    "movdi-2",
    "movdi-1",
    "movdi",
    "movdi+1",
    "movsf-1",
    "movsf",
    "movsf+1",
    "movsf+2",
    "zero_extendhisi2-1",
    "zero_extendhisi2",
    "zero_extendhisi2+1",
    "zero_extendqihi2",
    "zero_extendqihi2+1",
    "zero_extendqisi2",
    "zero_extendqisi2+1",
    "zero_extendqisi2+2",
    "zero_extendqisi2+3",
    "extendhisi2-2",
    "extendhisi2-1",
    "extendhisi2",
    "extendhisi2+1",
    "extendqihi2",
    "extendqihi2+1",
    "extendqisi2",
    "extendqisi2+1",
    "extendsfdf2-1",
    "extendsfdf2",
    "extendsftf2",
    "extenddftf2",
    "truncdfsf2",
    "trunctfsf2",
    "trunctfdf2",
    "floatsisf2",
    "floatsidf2",
    "floatsitf2",
    "fix_truncsfsi2",
    "fix_truncdfsi2",
    "fix_trunctfsi2",
    "adddi3",
    "addsi3",
    "addsi3+1",
    "subdi3-1",
    "subdi3",
    "subsi3",
    "subsi3+1",
    "mulsi3-1",
    "mulsi3",
    "mulsi3+1",
    "mulsidi3",
    "mulsidi3+1",
    "const_mulsidi3",
    "umulsidi3",
    "umulsidi3+1",
    "const_umulsidi3",
    "divsi3",
    "divsi3+1",
    "udivsi3",
    "udivsi3+1",
    "anddi3",
    "anddi3+1",
    "andsi3",
    "andsi3+1",
    "andsi3+2",
    "iordi3-1",
    "iordi3",
    "iordi3+1",
    "iorsi3",
    "iorsi3+1",
    "iorsi3+2",
    "xordi3-1",
    "xordi3",
    "xordi3+1",
    "xorsi3",
    "xorsi3+1",
    "xorsi3+2",
    "xorsi3+3",
    "xorsi3+4",
    "xorsi3+5",
    "negdi2-5",
    "negdi2-4",
    "negdi2-3",
    "negdi2-2",
    "negdi2-1",
    "negdi2",
    "negsi2",
    "negsi2+1",
    "one_cmpldi2-1",
    "one_cmpldi2",
    "one_cmpldi2+1",
    "one_cmplsi2",
    "one_cmplsi2+1",
    "addtf3-1",
    "addtf3",
    "adddf3",
    "addsf3",
    "subtf3",
    "subdf3",
    "subsf3",
    "multf3",
    "muldf3",
    "mulsf3",
    "mulsf3+1",
    "divtf3-1",
    "divtf3",
    "divdf3",
    "divsf3",
    "negtf2",
    "negdf2",
    "negsf2",
    "abstf2",
    "absdf2",
    "abssf2",
    "sqrttf2",
    "sqrtdf2",
    "sqrtsf2",
    "ashldi3",
    "ashldi3+1",
    "ashlsi3",
    "lshldi3",
    "lshldi3+1",
    "ashrsi3-1",
    "ashrsi3",
    "lshrsi3",
    "lshrdi3",
    "lshrdi3+1",
    "jump-1",
    "jump",
    "tablejump",
    "pic_tablejump",
    "pic_tablejump+1",
    "call-1",
    "call",
    "call+1",
    "call_value-1",
    "call_value",
    "call_value+1",
    "untyped_call",
    "untyped_call+1",
    "untyped_return",
    "update_return",
    "return",
    "nop",
    "indirect_jump",
    "nonlocal_goto",
    "flush_register_windows",
    "goto_handler_and_restore",
    "ffssi2",
    "ffssi2+1",
    "ffssi2+2",
    "ffssi2+3",
    "ffssi2+4",
    "ffssi2+5",
    "ffssi2+6",
    "ffssi2+7",
    "ffssi2+8",
    "ffssi2+9",
    "ffssi2+10",
    "ffssi2+11",
    "ffssi2+12",
    "ffssi2+13",
    "ffssi2+14",
    "ffssi2+15",
    "ffssi2+16",
    "ffssi2+17",
    "ffssi2+18",
    "ffssi2+19",
    "ffssi2+20",
    "ffssi2+21",
    "ffssi2+22",
    "ffssi2+23",
    "ffssi2+24",
    "ffssi2+25",
    "ffssi2+26",
    "ffssi2+27",
    "ffssi2+28",
    "ffssi2+29",
    "ffssi2+30",
    "ffssi2+31",
    "ffssi2+32",
    "ffssi2+33",
    "ffssi2+34",
    "ffssi2+35",
    "ffssi2+36",
    "ffssi2+37",
    "ffssi2+38",
  };
char **insn_name_ptr = insn_name;

const int insn_n_operands[] =
  {
    2,
    2,
    2,
    2,
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
    1,
    1,
    2,
    2,
    1,
    1,
    2,
    3,
    2,
    3,
    3,
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
    1,
    1,
    2,
    3,
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
    2,
    2,
    3,
    3,
    2,
    2,
    3,
    3,
    2,
    2,
    2,
    2,
    3,
    2,
    2,
    2,
    2,
    2,
    3,
    2,
    2,
    2,
    2,
    2,
    2,
    3,
    2,
    2,
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
    3,
    3,
    2,
    3,
    3,
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
    4,
    4,
    3,
    3,
    3,
    3,
    3,
    4,
    3,
    3,
    3,
    3,
    3,
    4,
    3,
    3,
    3,
    3,
    3,
    4,
    4,
    3,
    3,
    3,
    4,
    2,
    3,
    3,
    4,
    2,
    2,
    1,
    2,
    2,
    2,
    2,
    1,
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
    2,
    3,
    4,
    4,
    4,
    3,
    3,
    4,
    4,
    4,
    0,
    1,
    1,
    1,
    0,
    4,
    2,
    3,
    5,
    3,
    3,
    3,
    2,
    2,
    0,
    0,
    1,
    4,
    0,
    0,
    3,
    4,
    4,
    3,
    4,
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
    3,
    3,
    3,
    3,
    3,
    2,
    2,
    2,
    2,
    3,
    1,
    3,
    2,
    2,
  };

const int insn_n_dups[] =
  {
    0,
    0,
    0,
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
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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
    0,
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
    0,
    0,
    0,
    2,
    0,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    0,
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
    1,
    0,
    2,
    0,
    1,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
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
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
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
    { "", "", },
    { "", "", },
    { "", "", },
    { "", "", },
    { "", "", "", },
    { "", "", "", },
    { "", },
    { "", },
    { "", },
    { "", },
    { "", },
    { "", },
    { "", },
    { "", },
    { "", },
    { "", },
    { "r", "rI", },
    { "f", "f", },
    { "f", "f", },
    { "f", "f", },
    { "f", "f", },
    { "f", "f", },
    { "f", "f", },
    { "=r", "r", },
    { "=r", "r", },
    { "=r", "r", },
    { "=r", "r", },
    { "=r", "r", "r", },
    { "=r", "r", "r", },
    { "=r", "r", "r", },
    { "=r", "r", "r", },
    { "=r", },
    { "=r", },
    { "=r", "rI", },
    { "=r", "rI", },
    { "=r", },
    { "=r", },
    { "=r", "rI", },
    { "=r", "%r", "rI", },
    { "=r", "r", },
    { "=r", "r", "rI", },
    { "=r", "r", "rI", },
    { "=r", "r", },
    { "=r", "r", },
    { "=r", "", },
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
    { "", },
    { "", "", },
    { "=r", "", "=&r", },
    { "=r,f,r,r,f,Q,Q", "rI,!f,K,Q,!Q,rJ,!f", },
    { "=r", "i", },
    { "=r", "", },
    { "=r", "", },
    { "=r", "", },
    { "=r", "", },
    { "=r", "0", "in", },
    { "=r", "r", "in", },
    { "=r", "r", "in", },
    { "", "rJ", "=&r", },
    { "", "", },
    { "=r,r,r,Q", "rI,K,Q,rJ", },
    { "=r", "r", "in", },
    { "", "rJ", "=&r", },
    { "", "", },
    { "=r,r,r,Q", "rI,K,Q,rJ", },
    { "=r", "r", "in", },
    { "", "rJ", "=&r", },
    { "=?r,f,o", "?E,m,G", },
    { "", "", },
    { "=f,r,Q,Q,f,&r", "f,r,f,r,Q,Q", },
    { "=r,Q,&r", "r,r,Q", },
    { "i,i", "rf,G", "=&r,&r", },
    { "=?r,f,o", "?E,m,G", },
    { "", "", },
    { "=T,U,f,r,Q,Q,f,&r", "U,T,f,r,f,r,Q,Q", },
    { "=T,U,r,Q,&r", "U,T,r,r,Q", },
    { "", "", },
    { "i,i", "rf,G", "=&r,&r", },
    { "", "", },
    { "=r,Q,&r,&r,?f,?f,?Q", "r,r,Q,i,f,Q,f", },
    { "=?r,f,m", "?E,m,G", },
    { "", "", },
    { "=f,r,f,r,Q,Q", "f,r,Q,Q,f,r", },
    { "=r,r,Q", "r,Q,r", },
    { "i", "rfG", "=&r", },
    { "", "", },
    { "=r", "m", },
    { "", "", },
    { "=r,r", "r,Q", },
    { "", "", },
    { "=r,r", "r,Q", },
    { "r", },
    { "=r", "r", },
    { "r", },
    { "=r", "r", },
    { "", "", },
    { "=r", "m", },
    { "", "", },
    { "=r", "m", },
    { "", "", },
    { "=r", "m", },
    { "r", "n", "n", },
    { "=f", "f", },
    { "=f", "f", },
    { "=f", "f", },
    { "=f", "f", },
    { "=f", "f", },
    { "=f", "f", },
    { "=f", "f", },
    { "=f", "f", },
    { "=f", "f", },
    { "=f", "f", },
    { "=f", "f", },
    { "=f", "f", },
    { "=r", "%r", "rHI", },
    { "=r", "%r", "rI", },
    { "%r", "rI", },
    { "=r", "%r", "rI", },
    { "=r", "r", "rHI", },
    { "=r", "r", "rI", },
    { "r", "rI", },
    { "=r", "r", "rI", },
    { "=r", "%r", "rI", },
    { "=r", "%r", "rI", },
    { "", "", "", },
    { "=r", "r", "r", },
    { "=r", "r", "I", },
    { "", "", "", },
    { "=r", "r", "r", },
    { "=r", "r", "I", },
    { "=r", "r", "rI", "=&r", },
    { "=r", "r", "rI", "=&r", },
    { "=r", "r", "rI", },
    { "=r", "r", "rI", },
    { "", "", "", },
    { "=r", "%r", "rHI", },
    { "=r", "%r", "rI", },
    { "", "", "", "", },
    { "=r", "r", "r", },
    { "=r", "r", "r", },
    { "", "", "", },
    { "=r", "%r", "rHI", },
    { "=r", "%r", "rI", },
    { "", "", "", "", },
    { "=r", "r", "r", },
    { "=r", "r", "r", },
    { "", "", "", },
    { "=r", "%r", "rHI", },
    { "=r", "%rJ", "rI", },
    { "", "", "", "", },
    { "", "", "", "", },
    { "=r", "r", "r", },
    { "=r", "rJ", "rI", },
    { "%r", "rI", "", },
    { "=r", "%r", "rI", "", },
    { "%rJ", "rI", },
    { "=r", "%rJ", "rI", },
    { "rI", "rJ", "", },
    { "=r", "rI", "rJ", "", },
    { "=r", "r", },
    { "=r", "rI", },
    { "rI", },
    { "=r", "rI", },
    { "=r", "rHI", },
    { "=r", "rHI", },
    { "=r", "rI", },
    { "rI", },
    { "=r", "rI", },
    { "=f", "f", "f", },
    { "=f", "f", "f", },
    { "=f", "f", "f", },
    { "=f", "f", "f", },
    { "=f", "f", "f", },
    { "=f", "f", "f", },
    { "=f", "f", "f", },
    { "=f", "f", "f", },
    { "=f", "f", "f", },
    { "=f", "f", "f", },
    { "=f", "f", "f", },
    { "=f", "f", "f", },
    { "=f", "f", "f", },
    { "=f", "f", "f", },
    { "=f,f", "0,f", },
    { "=f,f", "0,f", },
    { "=f", "f", },
    { "=f,f", "0,f", },
    { "=f,f", "0,f", },
    { "=f", "f", },
    { "=f", "f", },
    { "=f", "f", },
    { "=f", "f", },
    { "", "", "", },
    { "=&r", "r", },
    { "=r", "r", "rI", },
    { "", "", "", "", },
    { "=r", "r", "I", "=r", },
    { "=r", "r", "I", "=X", },
    { "=r", "r", "rI", },
    { "=r", "r", "rI", },
    { "", "", "", "", },
    { "=r", "r", "I", "=r", },
    { "=r", "r", "I", "=X", },
    { 0 },
    { "r", },
    { "r", },
    { "p", },
    { 0 },
    { "", "", "", "i", },
    { "S,r", "", },
    { "S,r", "", "", },
    { "=rf", "", "", "", "", },
    { "=rf", "rS", "", },
    { "", "", "", },
    { "rS", "o", "", },
    { "", "", },
    { "r", "r", },
    { 0 },
    { 0 },
    { "p", },
    { "", "", "", "", },
    { 0 },
    { 0 },
    { "=&r", "r", "=&r", },
    { "", "", "", "", },
    { "", "", "", "", },
    { "", "", "", },
    { "", "", "", "", },
    { "", "", },
    { "", "", },
    { "", "", },
    { "", "", },
    { "", "", },
    { "", "", },
    { "", "", "", },
    { "", "", "", },
    { "", "", "", },
    { "", "", "", },
    { "=rf", "", "=rf", "", },
    { "", "rf", "", "rf", },
    { "=fr", "", "=fr", "", },
    { "", "fr", "", "fr", },
    { "=rf", "", "=rf", "", },
    { "", "rf", "", "rf", },
    { "=fr", "", "=fr", "", },
    { "", "fr", "", "fr", },
    { "=r", "r", "r", },
    { "", "", "", },
    { "", "", "", },
    { "", "", "", },
    { "", "", "", },
    { "=r", "i", "=fr", },
    { "=r", "i", "=fr", },
    { "", "rI", },
    { "", "rI", },
    { "", "rI", },
    { "r", "r", },
    { "", "%r", "rI", },
    { "f", },
    { "", "S,r", "", },
    { "S,r", "", },
    { "=r", "rJ", },
  };

const enum machine_mode insn_operand_mode[][MAX_RECOG_OPERANDS] =
  {
    { SImode, SImode, },
    { SFmode, SFmode, },
    { DFmode, DFmode, },
    { TFmode, TFmode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, },
    { SImode, },
    { SImode, },
    { SImode, },
    { SImode, },
    { SImode, },
    { SImode, },
    { SImode, },
    { SImode, },
    { SImode, },
    { SImode, SImode, },
    { DFmode, DFmode, },
    { SFmode, SFmode, },
    { TFmode, TFmode, },
    { DFmode, DFmode, },
    { SFmode, SFmode, },
    { TFmode, TFmode, },
    { SImode, SImode, },
    { SImode, SImode, },
    { SImode, SImode, },
    { SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, },
    { SImode, },
    { SImode, SImode, },
    { SImode, SImode, },
    { SImode, },
    { SImode, },
    { SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, },
    { SImode, SImode, },
    { SImode, SImode, },
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
    { VOIDmode, },
    { SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, },
    { SImode, SImode, },
    { DImode, VOIDmode, },
    { SImode, VOIDmode, },
    { SImode, VOIDmode, },
    { HImode, VOIDmode, },
    { DImode, DImode, DImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, },
    { HImode, HImode, },
    { HImode, HImode, VOIDmode, },
    { SImode, HImode, SImode, },
    { QImode, QImode, },
    { QImode, QImode, },
    { QImode, QImode, VOIDmode, },
    { SImode, QImode, SImode, },
    { TFmode, TFmode, },
    { TFmode, TFmode, },
    { TFmode, TFmode, },
    { TFmode, TFmode, },
    { SImode, TFmode, SImode, },
    { DFmode, DFmode, },
    { DFmode, DFmode, },
    { DFmode, DFmode, },
    { DFmode, DFmode, },
    { VOIDmode, VOIDmode, },
    { SImode, DFmode, SImode, },
    { DImode, DImode, },
    { DImode, DImode, },
    { SFmode, SFmode, },
    { SFmode, SFmode, },
    { SFmode, SFmode, },
    { SFmode, SFmode, },
    { SImode, SFmode, SImode, },
    { SImode, HImode, },
    { SImode, HImode, },
    { HImode, QImode, },
    { HImode, QImode, },
    { SImode, QImode, },
    { SImode, QImode, },
    { QImode, },
    { SImode, QImode, },
    { SImode, },
    { QImode, SImode, },
    { SImode, HImode, },
    { SImode, HImode, },
    { HImode, QImode, },
    { HImode, QImode, },
    { SImode, QImode, },
    { SImode, QImode, },
    { SImode, SImode, SImode, },
    { DFmode, SFmode, },
    { TFmode, SFmode, },
    { TFmode, DFmode, },
    { SFmode, DFmode, },
    { SFmode, TFmode, },
    { DFmode, TFmode, },
    { SFmode, SImode, },
    { DFmode, SImode, },
    { TFmode, SImode, },
    { SImode, SFmode, },
    { SImode, DFmode, },
    { SImode, TFmode, },
    { DImode, DImode, DImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, },
    { SImode, SImode, SImode, },
    { DImode, DImode, DImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { DImode, SImode, SImode, },
    { DImode, SImode, SImode, },
    { DImode, SImode, SImode, },
    { DImode, SImode, SImode, },
    { DImode, SImode, SImode, },
    { DImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { DImode, DImode, DImode, },
    { DImode, DImode, DImode, },
    { SImode, SImode, SImode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
    { DImode, DImode, DImode, },
    { SImode, SImode, SImode, },
    { DImode, DImode, DImode, },
    { DImode, DImode, DImode, },
    { SImode, SImode, SImode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
    { DImode, DImode, DImode, },
    { SImode, SImode, SImode, },
    { DImode, DImode, DImode, },
    { DImode, DImode, DImode, },
    { SImode, SImode, SImode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
    { DImode, DImode, DImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { DImode, DImode, },
    { SImode, SImode, },
    { SImode, },
    { SImode, SImode, },
    { DImode, DImode, },
    { DImode, DImode, },
    { SImode, SImode, },
    { SImode, },
    { SImode, SImode, },
    { TFmode, TFmode, TFmode, },
    { DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, },
    { TFmode, TFmode, TFmode, },
    { DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, },
    { TFmode, TFmode, TFmode, },
    { DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, },
    { DFmode, SFmode, SFmode, },
    { TFmode, DFmode, DFmode, },
    { TFmode, TFmode, TFmode, },
    { DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, },
    { TFmode, TFmode, },
    { DFmode, DFmode, },
    { SFmode, SFmode, },
    { TFmode, TFmode, },
    { DFmode, DFmode, },
    { SFmode, SFmode, },
    { TFmode, TFmode, },
    { DFmode, DFmode, },
    { SFmode, SFmode, },
    { DImode, DImode, SImode, },
    { DImode, SImode, },
    { SImode, SImode, SImode, },
    { DImode, DImode, DImode, SImode, },
    { DImode, DImode, DImode, SImode, },
    { DImode, DImode, DImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { DImode, DImode, DImode, SImode, },
    { DImode, DImode, DImode, SImode, },
    { DImode, DImode, DImode, SImode, },
    { VOIDmode },
    { SImode, },
    { SImode, },
    { SImode, },
    { VOIDmode },
    { SImode, VOIDmode, VOIDmode, VOIDmode, },
    { SImode, VOIDmode, },
    { SImode, VOIDmode, VOIDmode, },
    { VOIDmode, SImode, VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, SImode, VOIDmode, },
    { SImode, BLKmode, VOIDmode, },
    { SImode, DImode, VOIDmode, },
    { BLKmode, VOIDmode, },
    { SImode, SImode, },
    { VOIDmode },
    { VOIDmode },
    { SImode, },
    { SImode, SImode, SImode, SImode, },
    { VOIDmode },
    { VOIDmode },
    { SImode, SImode, SImode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, VOIDmode, },
    { QImode, QImode, },
    { HImode, HImode, },
    { SImode, SImode, },
    { SFmode, SFmode, },
    { SImode, SImode, SImode, },
    { SFmode, },
    { VOIDmode, VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, },
    { VOIDmode, VOIDmode, },
  };

const char insn_operand_strict_low[][MAX_RECOG_OPERANDS] =
  {
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
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
    { 0, },
    { 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, },
    { 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
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
    { 0, 0, },
    { 0, 0, 0, },
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
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, },
    { 0, 0, },
    { 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
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
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
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
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, },
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
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0 },
    { 0, },
    { 0, },
    { 0, },
    { 0 },
    { 0, 0, 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0 },
    { 0 },
    { 0, },
    { 0, 0, 0, 0, },
    { 0 },
    { 0 },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
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
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
  };

extern int register_operand ();
extern int arith_operand ();
extern int noov_compare_op ();
extern int general_operand ();
extern int reg_or_nonsymb_mem_operand ();
extern int move_operand ();
extern int move_pic_label ();
extern int immediate_operand ();
extern int symbolic_operand ();
extern int reg_or_0_operand ();
extern int scratch_operand ();
extern int memory_operand ();
extern int sparc_operand ();
extern int small_int ();
extern int arith_double_operand ();
extern int cc_arithop ();
extern int cc_arithopn ();
extern int const_double_operand ();
extern int const_int_operand ();
extern int address_operand ();
extern int call_operand ();
extern int call_operand_address ();
extern int restore_operand ();

int (*const insn_operand_predicate[][MAX_RECOG_OPERANDS])() =
  {
    { register_operand, arith_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, arith_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, arith_operand, },
    { register_operand, arith_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, arith_operand, },
    { register_operand, arith_operand, arith_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, arith_operand, },
    { register_operand, register_operand, arith_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, noov_compare_op, },
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
    { noov_compare_op, },
    { noov_compare_op, },
    { general_operand, general_operand, },
    { register_operand, general_operand, register_operand, },
    { reg_or_nonsymb_mem_operand, move_operand, },
    { register_operand, move_pic_label, },
    { register_operand, 0, },
    { register_operand, 0, },
    { register_operand, 0, },
    { register_operand, 0, },
    { register_operand, register_operand, immediate_operand, },
    { register_operand, register_operand, immediate_operand, },
    { register_operand, register_operand, immediate_operand, },
    { symbolic_operand, reg_or_0_operand, scratch_operand, },
    { general_operand, general_operand, },
    { reg_or_nonsymb_mem_operand, move_operand, },
    { register_operand, register_operand, immediate_operand, },
    { symbolic_operand, reg_or_0_operand, scratch_operand, },
    { general_operand, general_operand, },
    { reg_or_nonsymb_mem_operand, move_operand, },
    { register_operand, register_operand, immediate_operand, },
    { symbolic_operand, reg_or_0_operand, scratch_operand, },
    { general_operand, 0, },
    { general_operand, general_operand, },
    { reg_or_nonsymb_mem_operand, reg_or_nonsymb_mem_operand, },
    { reg_or_nonsymb_mem_operand, reg_or_nonsymb_mem_operand, },
    { symbolic_operand, reg_or_0_operand, scratch_operand, },
    { general_operand, 0, },
    { general_operand, general_operand, },
    { reg_or_nonsymb_mem_operand, reg_or_nonsymb_mem_operand, },
    { reg_or_nonsymb_mem_operand, reg_or_nonsymb_mem_operand, },
    { 0, 0, },
    { symbolic_operand, reg_or_0_operand, scratch_operand, },
    { reg_or_nonsymb_mem_operand, general_operand, },
    { reg_or_nonsymb_mem_operand, general_operand, },
    { general_operand, 0, },
    { general_operand, general_operand, },
    { reg_or_nonsymb_mem_operand, reg_or_nonsymb_mem_operand, },
    { reg_or_nonsymb_mem_operand, reg_or_nonsymb_mem_operand, },
    { symbolic_operand, reg_or_0_operand, scratch_operand, },
    { register_operand, register_operand, },
    { register_operand, memory_operand, },
    { register_operand, register_operand, },
    { register_operand, sparc_operand, },
    { register_operand, register_operand, },
    { register_operand, sparc_operand, },
    { register_operand, },
    { register_operand, register_operand, },
    { register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, memory_operand, },
    { register_operand, register_operand, },
    { register_operand, memory_operand, },
    { register_operand, register_operand, },
    { register_operand, memory_operand, },
    { register_operand, small_int, small_int, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, arith_double_operand, arith_double_operand, },
    { register_operand, arith_operand, arith_operand, },
    { arith_operand, arith_operand, },
    { register_operand, arith_operand, arith_operand, },
    { register_operand, register_operand, arith_double_operand, },
    { register_operand, register_operand, arith_operand, },
    { register_operand, arith_operand, },
    { register_operand, register_operand, arith_operand, },
    { register_operand, arith_operand, arith_operand, },
    { register_operand, arith_operand, arith_operand, },
    { register_operand, register_operand, arith_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, small_int, },
    { register_operand, register_operand, arith_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, small_int, },
    { register_operand, register_operand, arith_operand, scratch_operand, },
    { register_operand, register_operand, arith_operand, scratch_operand, },
    { register_operand, register_operand, arith_operand, },
    { register_operand, register_operand, arith_operand, },
    { register_operand, arith_double_operand, arith_double_operand, },
    { register_operand, arith_double_operand, arith_double_operand, },
    { register_operand, arith_operand, arith_operand, },
    { 0, 0, 0, 0, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, arith_double_operand, arith_double_operand, },
    { register_operand, arith_double_operand, arith_double_operand, },
    { register_operand, arith_operand, arith_operand, },
    { 0, 0, 0, 0, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, arith_double_operand, arith_double_operand, },
    { register_operand, arith_double_operand, arith_double_operand, },
    { register_operand, arith_operand, arith_operand, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { register_operand, register_operand, register_operand, },
    { register_operand, reg_or_0_operand, arith_operand, },
    { arith_operand, arith_operand, cc_arithop, },
    { register_operand, arith_operand, arith_operand, cc_arithop, },
    { reg_or_0_operand, arith_operand, },
    { register_operand, reg_or_0_operand, arith_operand, },
    { arith_operand, reg_or_0_operand, cc_arithopn, },
    { register_operand, arith_operand, reg_or_0_operand, cc_arithopn, },
    { register_operand, register_operand, },
    { general_operand, arith_operand, },
    { arith_operand, },
    { register_operand, arith_operand, },
    { register_operand, arith_double_operand, },
    { register_operand, arith_double_operand, },
    { register_operand, arith_operand, },
    { arith_operand, },
    { register_operand, arith_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, const_double_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, arith_operand, },
    { register_operand, register_operand, const_int_operand, scratch_operand, },
    { register_operand, register_operand, const_int_operand, scratch_operand, },
    { register_operand, register_operand, const_int_operand, scratch_operand, },
    { register_operand, register_operand, arith_operand, },
    { register_operand, register_operand, arith_operand, },
    { register_operand, register_operand, const_int_operand, scratch_operand, },
    { register_operand, register_operand, const_int_operand, scratch_operand, },
    { register_operand, register_operand, const_int_operand, scratch_operand, },
    { 0 },
    { register_operand, },
    { register_operand, },
    { address_operand, },
    { 0 },
    { call_operand, 0, 0, 0, },
    { call_operand_address, 0, },
    { call_operand_address, 0, immediate_operand, },
    { register_operand, 0, 0, 0, 0, },
    { 0, call_operand_address, 0, },
    { call_operand, memory_operand, 0, },
    { call_operand_address, memory_operand, 0, },
    { memory_operand, 0, },
    { register_operand, register_operand, },
    { 0 },
    { 0 },
    { address_operand, },
    { general_operand, general_operand, general_operand, 0, },
    { 0 },
    { 0 },
    { register_operand, register_operand, scratch_operand, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
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
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { restore_operand, arith_operand, },
    { restore_operand, arith_operand, },
    { restore_operand, arith_operand, },
    { restore_operand, register_operand, },
    { restore_operand, arith_operand, arith_operand, },
    { register_operand, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
  };

const int insn_n_alternatives[] =
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
    1,
    7,
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
    4,
    1,
    1,
    0,
    4,
    1,
    1,
    3,
    0,
    6,
    3,
    2,
    3,
    0,
    8,
    5,
    0,
    2,
    0,
    7,
    3,
    0,
    6,
    3,
    1,
    0,
    1,
    0,
    2,
    0,
    2,
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
    1,
    1,
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
    1,
    2,
    2,
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
    1,
    1,
    0,
    1,
    1,
    0,
    1,
    1,
    1,
    0,
    1,
    2,
    2,
    1,
    1,
    0,
    1,
    0,
    1,
    0,
    0,
    1,
    0,
    0,
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
    1,
  };

/* Generated automatically by the program `genpeep'
from the machine description file `md'.  */

#include "config.h"
#include "rtl.h"
#include "regs.h"
#include "output.h"
#include "real.h"

extern rtx peep_operand[];

#define operands peep_operand

rtx
peephole (ins1)
     rtx ins1;
{
  rtx insn, x, pat;
  int i;

  if (NEXT_INSN (ins1)
      && GET_CODE (NEXT_INSN (ins1)) == BARRIER)
    return 0;

  insn = ins1;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L299;
  x = XEXP (pat, 0);
  if (GET_CODE (x) != REG) goto L299;
  if (GET_MODE (x) != SImode) goto L299;
  if (XINT (x, 0) != 15) goto L299;
  x = XEXP (pat, 1);
  if (GET_CODE (x) != PLUS) goto L299;
  if (GET_MODE (x) != SImode) goto L299;
  x = XEXP (XEXP (pat, 1), 0);
  if (GET_CODE (x) != REG) goto L299;
  if (GET_MODE (x) != SImode) goto L299;
  if (XINT (x, 0) != 15) goto L299;
  x = XEXP (XEXP (pat, 1), 1);
  if (GET_CODE (x) != CONST_INT) goto L299;
  if (XWINT (x, 0) != 4) goto L299;
  do { insn = NEXT_INSN (insn);
       if (insn == 0) goto L299; }
  while (GET_CODE (insn) == NOTE
	 || (GET_CODE (insn) == INSN
	     && (GET_CODE (PATTERN (insn)) == USE
		 || GET_CODE (PATTERN (insn)) == CLOBBER)));
  if (GET_CODE (insn) == CODE_LABEL
      || GET_CODE (insn) == BARRIER)
    goto L299;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L299;
  x = XEXP (pat, 0);
  operands[0] = x;
  if (! register_operand (x, DFmode)) goto L299;
  x = XEXP (pat, 1);
  operands[1] = x;
  if (! register_operand (x, DFmode)) goto L299;
  if (! (FP_REG_P (operands[0]) && ! FP_REG_P (operands[1]))) goto L299;
  PATTERN (ins1) = gen_rtx (PARALLEL, VOIDmode, gen_rtvec_v (2, operands));
  INSN_CODE (ins1) = 299;
  delete_for_peephole (NEXT_INSN (ins1), insn);
  return NEXT_INSN (insn);
 L299:

  insn = ins1;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L300;
  x = XEXP (pat, 0);
  if (GET_CODE (x) != REG) goto L300;
  if (GET_MODE (x) != SImode) goto L300;
  if (XINT (x, 0) != 15) goto L300;
  x = XEXP (pat, 1);
  if (GET_CODE (x) != PLUS) goto L300;
  if (GET_MODE (x) != SImode) goto L300;
  x = XEXP (XEXP (pat, 1), 0);
  if (GET_CODE (x) != REG) goto L300;
  if (GET_MODE (x) != SImode) goto L300;
  if (XINT (x, 0) != 15) goto L300;
  x = XEXP (XEXP (pat, 1), 1);
  operands[0] = x;
  if (! immediate_operand (x, SImode)) goto L300;
  do { insn = NEXT_INSN (insn);
       if (insn == 0) goto L300; }
  while (GET_CODE (insn) == NOTE
	 || (GET_CODE (insn) == INSN
	     && (GET_CODE (PATTERN (insn)) == USE
		 || GET_CODE (PATTERN (insn)) == CLOBBER)));
  if (GET_CODE (insn) == CODE_LABEL
      || GET_CODE (insn) == BARRIER)
    goto L300;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L300;
  x = XEXP (pat, 0);
  operands[1] = x;
  if (! push_operand (x, SFmode)) goto L300;
  x = XEXP (pat, 1);
  operands[2] = x;
  if (! general_operand (x, SFmode)) goto L300;
  if (! (GET_CODE (operands[0]) == CONST_INT && INTVAL (operands[0]) >= 4
   && ! reg_mentioned_p (stack_pointer_rtx, operands[2]))) goto L300;
  PATTERN (ins1) = gen_rtx (PARALLEL, VOIDmode, gen_rtvec_v (3, operands));
  INSN_CODE (ins1) = 300;
  delete_for_peephole (NEXT_INSN (ins1), insn);
  return NEXT_INSN (insn);
 L300:

  insn = ins1;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L301;
  x = XEXP (pat, 0);
  if (GET_CODE (x) != REG) goto L301;
  if (GET_MODE (x) != SImode) goto L301;
  if (XINT (x, 0) != 15) goto L301;
  x = XEXP (pat, 1);
  if (GET_CODE (x) != PLUS) goto L301;
  if (GET_MODE (x) != SImode) goto L301;
  x = XEXP (XEXP (pat, 1), 0);
  if (GET_CODE (x) != REG) goto L301;
  if (GET_MODE (x) != SImode) goto L301;
  if (XINT (x, 0) != 15) goto L301;
  x = XEXP (XEXP (pat, 1), 1);
  operands[0] = x;
  if (! immediate_operand (x, SImode)) goto L301;
  do { insn = NEXT_INSN (insn);
       if (insn == 0) goto L301; }
  while (GET_CODE (insn) == NOTE
	 || (GET_CODE (insn) == INSN
	     && (GET_CODE (PATTERN (insn)) == USE
		 || GET_CODE (PATTERN (insn)) == CLOBBER)));
  if (GET_CODE (insn) == CODE_LABEL
      || GET_CODE (insn) == BARRIER)
    goto L301;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L301;
  x = XEXP (pat, 0);
  operands[1] = x;
  if (! push_operand (x, SImode)) goto L301;
  x = XEXP (pat, 1);
  operands[2] = x;
  if (! general_operand (x, SImode)) goto L301;
  if (! (GET_CODE (operands[0]) == CONST_INT && INTVAL (operands[0]) >= 4
   && ! reg_mentioned_p (stack_pointer_rtx, operands[2]))) goto L301;
  PATTERN (ins1) = gen_rtx (PARALLEL, VOIDmode, gen_rtvec_v (3, operands));
  INSN_CODE (ins1) = 301;
  delete_for_peephole (NEXT_INSN (ins1), insn);
  return NEXT_INSN (insn);
 L301:

  insn = ins1;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L302;
  x = XEXP (pat, 0);
  if (GET_CODE (x) != MEM) goto L302;
  if (GET_MODE (x) != QImode) goto L302;
  x = XEXP (XEXP (pat, 0), 0);
  if (GET_CODE (x) != PRE_DEC) goto L302;
  if (GET_MODE (x) != SImode) goto L302;
  x = XEXP (XEXP (XEXP (pat, 0), 0), 0);
  if (GET_CODE (x) != REG) goto L302;
  if (GET_MODE (x) != SImode) goto L302;
  if (XINT (x, 0) != 15) goto L302;
  x = XEXP (pat, 1);
  operands[1] = x;
  if (! general_operand (x, QImode)) goto L302;
  do { insn = NEXT_INSN (insn);
       if (insn == 0) goto L302; }
  while (GET_CODE (insn) == NOTE
	 || (GET_CODE (insn) == INSN
	     && (GET_CODE (PATTERN (insn)) == USE
		 || GET_CODE (PATTERN (insn)) == CLOBBER)));
  if (GET_CODE (insn) == CODE_LABEL
      || GET_CODE (insn) == BARRIER)
    goto L302;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L302;
  x = XEXP (pat, 0);
  if (GET_CODE (x) != REG) goto L302;
  if (GET_MODE (x) != SImode) goto L302;
  if (XINT (x, 0) != 15) goto L302;
  x = XEXP (pat, 1);
  if (GET_CODE (x) != MINUS) goto L302;
  if (GET_MODE (x) != SImode) goto L302;
  x = XEXP (XEXP (pat, 1), 0);
  if (GET_CODE (x) != REG) goto L302;
  if (GET_MODE (x) != SImode) goto L302;
  if (XINT (x, 0) != 15) goto L302;
  x = XEXP (XEXP (pat, 1), 1);
  if (GET_CODE (x) != CONST_INT) goto L302;
  if (XWINT (x, 0) != 2) goto L302;
  if (! (! reg_mentioned_p (stack_pointer_rtx, operands[1]))) goto L302;
  PATTERN (ins1) = gen_rtx (PARALLEL, VOIDmode, gen_rtvec_v (2, operands));
  INSN_CODE (ins1) = 302;
  delete_for_peephole (NEXT_INSN (ins1), insn);
  return NEXT_INSN (insn);
 L302:

  insn = ins1;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L303;
  x = XEXP (pat, 0);
  operands[0] = x;
  if (! register_operand (x, SImode)) goto L303;
  x = XEXP (pat, 1);
  if (GET_CODE (x) != CONST_INT) goto L303;
  if (XWINT (x, 0) != 0) goto L303;
  do { insn = NEXT_INSN (insn);
       if (insn == 0) goto L303; }
  while (GET_CODE (insn) == NOTE
	 || (GET_CODE (insn) == INSN
	     && (GET_CODE (PATTERN (insn)) == USE
		 || GET_CODE (PATTERN (insn)) == CLOBBER)));
  if (GET_CODE (insn) == CODE_LABEL
      || GET_CODE (insn) == BARRIER)
    goto L303;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L303;
  x = XEXP (pat, 0);
  if (GET_CODE (x) != STRICT_LOW_PART) goto L303;
  x = XEXP (XEXP (pat, 0), 0);
  if (GET_CODE (x) != SUBREG) goto L303;
  if (GET_MODE (x) != HImode) goto L303;
  x = XEXP (XEXP (XEXP (pat, 0), 0), 0);
  if (!rtx_equal_p (operands[0], x)) goto L303;
  x = XEXP (XEXP (pat, 0), 0);
  if (XINT (x, 1) != 0) goto L303;
  x = XEXP (pat, 1);
  operands[1] = x;
  if (! general_operand (x, HImode)) goto L303;
  if (! (strict_low_part_peephole_ok (HImode, prev_nonnote_insn (insn), operands[0]))) goto L303;
  PATTERN (ins1) = gen_rtx (PARALLEL, VOIDmode, gen_rtvec_v (2, operands));
  INSN_CODE (ins1) = 303;
  delete_for_peephole (NEXT_INSN (ins1), insn);
  return NEXT_INSN (insn);
 L303:

  insn = ins1;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L304;
  x = XEXP (pat, 0);
  if (GET_CODE (x) != PC) goto L304;
  x = XEXP (pat, 1);
  if (GET_CODE (x) != IF_THEN_ELSE) goto L304;
  x = XEXP (XEXP (pat, 1), 0);
  operands[3] = x;
  if (! valid_dbcc_comparison_p (x, VOIDmode)) goto L304;
  x = XEXP (XEXP (XEXP (pat, 1), 0), 0);
  if (GET_CODE (x) != CC0) goto L304;
  x = XEXP (XEXP (XEXP (pat, 1), 0), 1);
  if (GET_CODE (x) != CONST_INT) goto L304;
  if (XWINT (x, 0) != 0) goto L304;
  x = XEXP (XEXP (pat, 1), 1);
  if (GET_CODE (x) != LABEL_REF) goto L304;
  x = XEXP (XEXP (XEXP (pat, 1), 1), 0);
  operands[2] = x;
  x = XEXP (XEXP (pat, 1), 2);
  if (GET_CODE (x) != PC) goto L304;
  do { insn = NEXT_INSN (insn);
       if (insn == 0) goto L304; }
  while (GET_CODE (insn) == NOTE
	 || (GET_CODE (insn) == INSN
	     && (GET_CODE (PATTERN (insn)) == USE
		 || GET_CODE (PATTERN (insn)) == CLOBBER)));
  if (GET_CODE (insn) == CODE_LABEL
      || GET_CODE (insn) == BARRIER)
    goto L304;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != PARALLEL) goto L304;
  if (XVECLEN (x, 0) != 2) goto L304;
  x = XVECEXP (pat, 0, 0);
  if (GET_CODE (x) != SET) goto L304;
  x = XEXP (XVECEXP (pat, 0, 0), 0);
  if (GET_CODE (x) != PC) goto L304;
  x = XEXP (XVECEXP (pat, 0, 0), 1);
  if (GET_CODE (x) != IF_THEN_ELSE) goto L304;
  x = XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0);
  if (GET_CODE (x) != GE) goto L304;
  x = XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0), 0);
  if (GET_CODE (x) != PLUS) goto L304;
  if (GET_MODE (x) != HImode) goto L304;
  x = XEXP (XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0), 0), 0);
  operands[0] = x;
  if (! register_operand (x, HImode)) goto L304;
  x = XEXP (XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0), 0), 1);
  if (GET_CODE (x) != CONST_INT) goto L304;
  if (XWINT (x, 0) != -1) goto L304;
  x = XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0), 1);
  if (GET_CODE (x) != CONST_INT) goto L304;
  if (XWINT (x, 0) != 0) goto L304;
  x = XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 1);
  if (GET_CODE (x) != LABEL_REF) goto L304;
  x = XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 1), 0);
  operands[1] = x;
  x = XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 2);
  if (GET_CODE (x) != PC) goto L304;
  x = XVECEXP (pat, 0, 1);
  if (GET_CODE (x) != SET) goto L304;
  x = XEXP (XVECEXP (pat, 0, 1), 0);
  if (!rtx_equal_p (operands[0], x)) goto L304;
  x = XEXP (XVECEXP (pat, 0, 1), 1);
  if (GET_CODE (x) != PLUS) goto L304;
  if (GET_MODE (x) != HImode) goto L304;
  x = XEXP (XEXP (XVECEXP (pat, 0, 1), 1), 0);
  if (!rtx_equal_p (operands[0], x)) goto L304;
  x = XEXP (XEXP (XVECEXP (pat, 0, 1), 1), 1);
  if (GET_CODE (x) != CONST_INT) goto L304;
  if (XWINT (x, 0) != -1) goto L304;
  if (! (DATA_REG_P (operands[0]))) goto L304;
  PATTERN (ins1) = gen_rtx (PARALLEL, VOIDmode, gen_rtvec_v (4, operands));
  INSN_CODE (ins1) = 304;
  delete_for_peephole (NEXT_INSN (ins1), insn);
  return NEXT_INSN (insn);
 L304:

  insn = ins1;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L305;
  x = XEXP (pat, 0);
  if (GET_CODE (x) != PC) goto L305;
  x = XEXP (pat, 1);
  if (GET_CODE (x) != IF_THEN_ELSE) goto L305;
  x = XEXP (XEXP (pat, 1), 0);
  operands[3] = x;
  if (! valid_dbcc_comparison_p (x, VOIDmode)) goto L305;
  x = XEXP (XEXP (XEXP (pat, 1), 0), 0);
  if (GET_CODE (x) != CC0) goto L305;
  x = XEXP (XEXP (XEXP (pat, 1), 0), 1);
  if (GET_CODE (x) != CONST_INT) goto L305;
  if (XWINT (x, 0) != 0) goto L305;
  x = XEXP (XEXP (pat, 1), 1);
  if (GET_CODE (x) != LABEL_REF) goto L305;
  x = XEXP (XEXP (XEXP (pat, 1), 1), 0);
  operands[2] = x;
  x = XEXP (XEXP (pat, 1), 2);
  if (GET_CODE (x) != PC) goto L305;
  do { insn = NEXT_INSN (insn);
       if (insn == 0) goto L305; }
  while (GET_CODE (insn) == NOTE
	 || (GET_CODE (insn) == INSN
	     && (GET_CODE (PATTERN (insn)) == USE
		 || GET_CODE (PATTERN (insn)) == CLOBBER)));
  if (GET_CODE (insn) == CODE_LABEL
      || GET_CODE (insn) == BARRIER)
    goto L305;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != PARALLEL) goto L305;
  if (XVECLEN (x, 0) != 2) goto L305;
  x = XVECEXP (pat, 0, 0);
  if (GET_CODE (x) != SET) goto L305;
  x = XEXP (XVECEXP (pat, 0, 0), 0);
  if (GET_CODE (x) != PC) goto L305;
  x = XEXP (XVECEXP (pat, 0, 0), 1);
  if (GET_CODE (x) != IF_THEN_ELSE) goto L305;
  x = XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0);
  if (GET_CODE (x) != GE) goto L305;
  x = XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0), 0);
  if (GET_CODE (x) != PLUS) goto L305;
  if (GET_MODE (x) != SImode) goto L305;
  x = XEXP (XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0), 0), 0);
  operands[0] = x;
  if (! register_operand (x, SImode)) goto L305;
  x = XEXP (XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0), 0), 1);
  if (GET_CODE (x) != CONST_INT) goto L305;
  if (XWINT (x, 0) != -1) goto L305;
  x = XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0), 1);
  if (GET_CODE (x) != CONST_INT) goto L305;
  if (XWINT (x, 0) != 0) goto L305;
  x = XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 1);
  if (GET_CODE (x) != LABEL_REF) goto L305;
  x = XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 1), 0);
  operands[1] = x;
  x = XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 2);
  if (GET_CODE (x) != PC) goto L305;
  x = XVECEXP (pat, 0, 1);
  if (GET_CODE (x) != SET) goto L305;
  x = XEXP (XVECEXP (pat, 0, 1), 0);
  if (!rtx_equal_p (operands[0], x)) goto L305;
  x = XEXP (XVECEXP (pat, 0, 1), 1);
  if (GET_CODE (x) != PLUS) goto L305;
  if (GET_MODE (x) != SImode) goto L305;
  x = XEXP (XEXP (XVECEXP (pat, 0, 1), 1), 0);
  if (!rtx_equal_p (operands[0], x)) goto L305;
  x = XEXP (XEXP (XVECEXP (pat, 0, 1), 1), 1);
  if (GET_CODE (x) != CONST_INT) goto L305;
  if (XWINT (x, 0) != -1) goto L305;
  if (! (DATA_REG_P (operands[0]))) goto L305;
  PATTERN (ins1) = gen_rtx (PARALLEL, VOIDmode, gen_rtvec_v (4, operands));
  INSN_CODE (ins1) = 305;
  delete_for_peephole (NEXT_INSN (ins1), insn);
  return NEXT_INSN (insn);
 L305:

  return 0;
}

rtx peep_operand[4];

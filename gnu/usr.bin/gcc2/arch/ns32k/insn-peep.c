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
  if (GET_CODE (x) != SET) goto L231;
  x = XEXP (pat, 0);
  if (GET_CODE (x) != REG) goto L231;
  if (GET_MODE (x) != SImode) goto L231;
  if (XINT (x, 0) != 17) goto L231;
  x = XEXP (pat, 1);
  if (GET_CODE (x) != PLUS) goto L231;
  if (GET_MODE (x) != SImode) goto L231;
  x = XEXP (XEXP (pat, 1), 0);
  if (GET_CODE (x) != REG) goto L231;
  if (GET_MODE (x) != SImode) goto L231;
  if (XINT (x, 0) != 17) goto L231;
  x = XEXP (XEXP (pat, 1), 1);
  if (GET_CODE (x) != CONST_INT) goto L231;
  if (XWINT (x, 0) != -2) goto L231;
  do { insn = NEXT_INSN (insn);
       if (insn == 0) goto L231; }
  while (GET_CODE (insn) == NOTE
	 || (GET_CODE (insn) == INSN
	     && (GET_CODE (PATTERN (insn)) == USE
		 || GET_CODE (PATTERN (insn)) == CLOBBER)));
  if (GET_CODE (insn) == CODE_LABEL
      || GET_CODE (insn) == BARRIER)
    goto L231;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L231;
  x = XEXP (pat, 0);
  operands[0] = x;
  if (! push_operand (x, HImode)) goto L231;
  x = XEXP (pat, 1);
  operands[1] = x;
  if (! general_operand (x, HImode)) goto L231;
  if (! (! reg_mentioned_p (stack_pointer_rtx, operands[1]))) goto L231;
  PATTERN (ins1) = gen_rtx (PARALLEL, VOIDmode, gen_rtvec_v (2, operands));
  INSN_CODE (ins1) = 231;
  delete_for_peephole (NEXT_INSN (ins1), insn);
  return NEXT_INSN (insn);
 L231:

  insn = ins1;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L232;
  x = XEXP (pat, 0);
  if (GET_CODE (x) != REG) goto L232;
  if (GET_MODE (x) != SImode) goto L232;
  if (XINT (x, 0) != 17) goto L232;
  x = XEXP (pat, 1);
  if (GET_CODE (x) != PLUS) goto L232;
  if (GET_MODE (x) != SImode) goto L232;
  x = XEXP (XEXP (pat, 1), 0);
  if (GET_CODE (x) != REG) goto L232;
  if (GET_MODE (x) != SImode) goto L232;
  if (XINT (x, 0) != 17) goto L232;
  x = XEXP (XEXP (pat, 1), 1);
  if (GET_CODE (x) != CONST_INT) goto L232;
  if (XWINT (x, 0) != -2) goto L232;
  do { insn = NEXT_INSN (insn);
       if (insn == 0) goto L232; }
  while (GET_CODE (insn) == NOTE
	 || (GET_CODE (insn) == INSN
	     && (GET_CODE (PATTERN (insn)) == USE
		 || GET_CODE (PATTERN (insn)) == CLOBBER)));
  if (GET_CODE (insn) == CODE_LABEL
      || GET_CODE (insn) == BARRIER)
    goto L232;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L232;
  x = XEXP (pat, 0);
  operands[0] = x;
  if (! push_operand (x, HImode)) goto L232;
  x = XEXP (pat, 1);
  if (GET_CODE (x) != ZERO_EXTEND) goto L232;
  if (GET_MODE (x) != HImode) goto L232;
  x = XEXP (XEXP (pat, 1), 0);
  operands[1] = x;
  if (! general_operand (x, QImode)) goto L232;
  if (! (! reg_mentioned_p (stack_pointer_rtx, operands[1]))) goto L232;
  PATTERN (ins1) = gen_rtx (PARALLEL, VOIDmode, gen_rtvec_v (2, operands));
  INSN_CODE (ins1) = 232;
  delete_for_peephole (NEXT_INSN (ins1), insn);
  return NEXT_INSN (insn);
 L232:

  insn = ins1;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L233;
  x = XEXP (pat, 0);
  if (GET_CODE (x) != REG) goto L233;
  if (GET_MODE (x) != SImode) goto L233;
  if (XINT (x, 0) != 17) goto L233;
  x = XEXP (pat, 1);
  if (GET_CODE (x) != PLUS) goto L233;
  if (GET_MODE (x) != SImode) goto L233;
  x = XEXP (XEXP (pat, 1), 0);
  if (GET_CODE (x) != REG) goto L233;
  if (GET_MODE (x) != SImode) goto L233;
  if (XINT (x, 0) != 17) goto L233;
  x = XEXP (XEXP (pat, 1), 1);
  if (GET_CODE (x) != CONST_INT) goto L233;
  if (XWINT (x, 0) != -2) goto L233;
  do { insn = NEXT_INSN (insn);
       if (insn == 0) goto L233; }
  while (GET_CODE (insn) == NOTE
	 || (GET_CODE (insn) == INSN
	     && (GET_CODE (PATTERN (insn)) == USE
		 || GET_CODE (PATTERN (insn)) == CLOBBER)));
  if (GET_CODE (insn) == CODE_LABEL
      || GET_CODE (insn) == BARRIER)
    goto L233;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L233;
  x = XEXP (pat, 0);
  operands[0] = x;
  if (! push_operand (x, HImode)) goto L233;
  x = XEXP (pat, 1);
  if (GET_CODE (x) != SIGN_EXTEND) goto L233;
  if (GET_MODE (x) != HImode) goto L233;
  x = XEXP (XEXP (pat, 1), 0);
  operands[1] = x;
  if (! general_operand (x, QImode)) goto L233;
  if (! (! reg_mentioned_p (stack_pointer_rtx, operands[1]))) goto L233;
  PATTERN (ins1) = gen_rtx (PARALLEL, VOIDmode, gen_rtvec_v (2, operands));
  INSN_CODE (ins1) = 233;
  delete_for_peephole (NEXT_INSN (ins1), insn);
  return NEXT_INSN (insn);
 L233:

  insn = ins1;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L234;
  x = XEXP (pat, 0);
  if (GET_CODE (x) != REG) goto L234;
  if (GET_MODE (x) != SImode) goto L234;
  if (XINT (x, 0) != 17) goto L234;
  x = XEXP (pat, 1);
  if (GET_CODE (x) != PLUS) goto L234;
  if (GET_MODE (x) != SImode) goto L234;
  x = XEXP (XEXP (pat, 1), 0);
  if (GET_CODE (x) != REG) goto L234;
  if (GET_MODE (x) != SImode) goto L234;
  if (XINT (x, 0) != 17) goto L234;
  x = XEXP (XEXP (pat, 1), 1);
  if (GET_CODE (x) != CONST_INT) goto L234;
  if (XWINT (x, 0) != -3) goto L234;
  do { insn = NEXT_INSN (insn);
       if (insn == 0) goto L234; }
  while (GET_CODE (insn) == NOTE
	 || (GET_CODE (insn) == INSN
	     && (GET_CODE (PATTERN (insn)) == USE
		 || GET_CODE (PATTERN (insn)) == CLOBBER)));
  if (GET_CODE (insn) == CODE_LABEL
      || GET_CODE (insn) == BARRIER)
    goto L234;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L234;
  x = XEXP (pat, 0);
  operands[0] = x;
  if (! push_operand (x, QImode)) goto L234;
  x = XEXP (pat, 1);
  operands[1] = x;
  if (! general_operand (x, QImode)) goto L234;
  if (! (! reg_mentioned_p (stack_pointer_rtx, operands[1]))) goto L234;
  PATTERN (ins1) = gen_rtx (PARALLEL, VOIDmode, gen_rtvec_v (2, operands));
  INSN_CODE (ins1) = 234;
  delete_for_peephole (NEXT_INSN (ins1), insn);
  return NEXT_INSN (insn);
 L234:

  insn = ins1;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L235;
  x = XEXP (pat, 0);
  if (GET_CODE (x) != REG) goto L235;
  if (GET_MODE (x) != SImode) goto L235;
  if (XINT (x, 0) != 17) goto L235;
  x = XEXP (pat, 1);
  if (GET_CODE (x) != PLUS) goto L235;
  if (GET_MODE (x) != SImode) goto L235;
  x = XEXP (XEXP (pat, 1), 0);
  if (GET_CODE (x) != REG) goto L235;
  if (GET_MODE (x) != SImode) goto L235;
  if (XINT (x, 0) != 17) goto L235;
  x = XEXP (XEXP (pat, 1), 1);
  if (GET_CODE (x) != CONST_INT) goto L235;
  if (XWINT (x, 0) != 4) goto L235;
  do { insn = NEXT_INSN (insn);
       if (insn == 0) goto L235; }
  while (GET_CODE (insn) == NOTE
	 || (GET_CODE (insn) == INSN
	     && (GET_CODE (PATTERN (insn)) == USE
		 || GET_CODE (PATTERN (insn)) == CLOBBER)));
  if (GET_CODE (insn) == CODE_LABEL
      || GET_CODE (insn) == BARRIER)
    goto L235;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L235;
  x = XEXP (pat, 0);
  operands[0] = x;
  if (! push_operand (x, SImode)) goto L235;
  x = XEXP (pat, 1);
  operands[1] = x;
  if (! general_operand (x, SImode)) goto L235;
  if (! (! reg_mentioned_p (stack_pointer_rtx, operands[1]))) goto L235;
  PATTERN (ins1) = gen_rtx (PARALLEL, VOIDmode, gen_rtvec_v (2, operands));
  INSN_CODE (ins1) = 235;
  delete_for_peephole (NEXT_INSN (ins1), insn);
  return NEXT_INSN (insn);
 L235:

  insn = ins1;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L236;
  x = XEXP (pat, 0);
  if (GET_CODE (x) != REG) goto L236;
  if (GET_MODE (x) != SImode) goto L236;
  if (XINT (x, 0) != 17) goto L236;
  x = XEXP (pat, 1);
  if (GET_CODE (x) != PLUS) goto L236;
  if (GET_MODE (x) != SImode) goto L236;
  x = XEXP (XEXP (pat, 1), 0);
  if (GET_CODE (x) != REG) goto L236;
  if (GET_MODE (x) != SImode) goto L236;
  if (XINT (x, 0) != 17) goto L236;
  x = XEXP (XEXP (pat, 1), 1);
  if (GET_CODE (x) != CONST_INT) goto L236;
  if (XWINT (x, 0) != 8) goto L236;
  do { insn = NEXT_INSN (insn);
       if (insn == 0) goto L236; }
  while (GET_CODE (insn) == NOTE
	 || (GET_CODE (insn) == INSN
	     && (GET_CODE (PATTERN (insn)) == USE
		 || GET_CODE (PATTERN (insn)) == CLOBBER)));
  if (GET_CODE (insn) == CODE_LABEL
      || GET_CODE (insn) == BARRIER)
    goto L236;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L236;
  x = XEXP (pat, 0);
  operands[0] = x;
  if (! push_operand (x, SImode)) goto L236;
  x = XEXP (pat, 1);
  operands[1] = x;
  if (! general_operand (x, SImode)) goto L236;
  do { insn = NEXT_INSN (insn);
       if (insn == 0) goto L236; }
  while (GET_CODE (insn) == NOTE
	 || (GET_CODE (insn) == INSN
	     && (GET_CODE (PATTERN (insn)) == USE
		 || GET_CODE (PATTERN (insn)) == CLOBBER)));
  if (GET_CODE (insn) == CODE_LABEL
      || GET_CODE (insn) == BARRIER)
    goto L236;
  pat = PATTERN (insn);
  x = pat;
  if (GET_CODE (x) != SET) goto L236;
  x = XEXP (pat, 0);
  operands[2] = x;
  if (! push_operand (x, SImode)) goto L236;
  x = XEXP (pat, 1);
  operands[3] = x;
  if (! general_operand (x, SImode)) goto L236;
  if (! (! reg_mentioned_p (stack_pointer_rtx, operands[1])
   && ! reg_mentioned_p (stack_pointer_rtx, operands[3]))) goto L236;
  PATTERN (ins1) = gen_rtx (PARALLEL, VOIDmode, gen_rtvec_v (4, operands));
  INSN_CODE (ins1) = 236;
  delete_for_peephole (NEXT_INSN (ins1), insn);
  return NEXT_INSN (insn);
 L236:

  return 0;
}

rtx peep_operand[4];

/* Generated automatically by the program `genemit'
from the machine description file `md'.  */

#include "config.h"
#include "rtl.h"
#include "expr.h"
#include "real.h"
#include "output.h"
#include "insn-config.h"

#include "insn-flags.h"

#include "insn-codes.h"

extern char *insn_operand_constraint[][MAX_RECOG_OPERANDS];

extern rtx recog_operand[];
#define operands emit_operand

#define FAIL goto _fail

#define DONE goto _done

rtx
gen_cmpsi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  sparc_compare_op0 = operands[0];
  sparc_compare_op1 = operands[1];
  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, gen_rtx (REG, CCmode, 0), gen_rtx (COMPARE, CCmode, operand0, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_cmpsf (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  sparc_compare_op0 = operands[0];
  sparc_compare_op1 = operands[1];
  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, gen_rtx (REG, CCFPmode, 0), gen_rtx (COMPARE, CCFPmode, operand0, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_cmpdf (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  sparc_compare_op0 = operands[0];
  sparc_compare_op1 = operands[1];
  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, gen_rtx (REG, CCFPmode, 0), gen_rtx (COMPARE, CCFPmode, operand0, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_cmptf (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  sparc_compare_op0 = operands[0];
  sparc_compare_op1 = operands[1];
  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, gen_rtx (REG, CCFPmode, 0), gen_rtx (COMPARE, CCFPmode, operand0, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_seq_special (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  rtx operand3;
  rtx operands[4];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;
{ operands[3] = gen_reg_rtx (SImode); }
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  emit_insn (gen_rtx (SET, VOIDmode, operand3, gen_rtx (XOR, SImode, operand1, operand2)));
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (EQ, SImode, operand3, const0_rtx)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (REG, CCmode, 0)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_sne_special (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  rtx operand3;
  rtx operands[4];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;
{ operands[3] = gen_reg_rtx (SImode); }
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  emit_insn (gen_rtx (SET, VOIDmode, operand3, gen_rtx (XOR, SImode, operand1, operand2)));
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (NE, SImode, operand3, const0_rtx)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (REG, CCmode, 0)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_seq (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ if (GET_MODE (sparc_compare_op0) == SImode)
    {
      emit_insn (gen_seq_special (operands[0], sparc_compare_op0,
				  sparc_compare_op1));
      DONE;
    }
  else
    operands[1] = gen_compare_reg (EQ, sparc_compare_op0, sparc_compare_op1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (EQ, SImode, operand1, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_sne (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ if (GET_MODE (sparc_compare_op0) == SImode)
    {
      emit_insn (gen_sne_special (operands[0], sparc_compare_op0,
				  sparc_compare_op1));
      DONE;
    }
  else
    operands[1] = gen_compare_reg (NE, sparc_compare_op0, sparc_compare_op1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (NE, SImode, operand1, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_sgt (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ operands[1] = gen_compare_reg (GT, sparc_compare_op0, sparc_compare_op1); }
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (GT, SImode, operand1, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_slt (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ operands[1] = gen_compare_reg (LT, sparc_compare_op0, sparc_compare_op1); }
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (LT, SImode, operand1, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_sge (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ operands[1] = gen_compare_reg (GE, sparc_compare_op0, sparc_compare_op1); }
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (GE, SImode, operand1, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_sle (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ operands[1] = gen_compare_reg (LE, sparc_compare_op0, sparc_compare_op1); }
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (LE, SImode, operand1, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_sgtu (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{
  rtx tem;

  /* We can do ltu easily, so if both operands are registers, swap them and
     do a LTU.  */
  if ((GET_CODE (sparc_compare_op0) == REG
       || GET_CODE (sparc_compare_op0) == SUBREG)
      && (GET_CODE (sparc_compare_op1) == REG
	  || GET_CODE (sparc_compare_op1) == SUBREG))
    {
      tem = sparc_compare_op0;
      sparc_compare_op0 = sparc_compare_op1;
      sparc_compare_op1 = tem;
      emit_insn (gen_sltu (operands[0]));
      DONE;
    }

  operands[1] = gen_compare_reg (LEU, sparc_compare_op0, sparc_compare_op1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (GTU, SImode, operand1, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_sltu (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ operands[1] = gen_compare_reg (LTU, sparc_compare_op0, sparc_compare_op1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (LTU, SImode, operand1, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_sgeu (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ operands[1] = gen_compare_reg (GEU, sparc_compare_op0, sparc_compare_op1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (GEU, SImode, operand1, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_sleu (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{
  rtx tem;

  /* We can do geu easily, so if both operands are registers, swap them and
     do a GEU.  */
  if ((GET_CODE (sparc_compare_op0) == REG
       || GET_CODE (sparc_compare_op0) == SUBREG)
      && (GET_CODE (sparc_compare_op1) == REG
	  || GET_CODE (sparc_compare_op1) == SUBREG))
    {
      tem = sparc_compare_op0;
      sparc_compare_op0 = sparc_compare_op1;
      sparc_compare_op1 = tem;
      emit_insn (gen_sgeu (operands[0]));
      DONE;
    }

  operands[1] = gen_compare_reg (LEU, sparc_compare_op0, sparc_compare_op1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (LEU, SImode, operand1, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_beq (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ operands[1] = gen_compare_reg (EQ, sparc_compare_op0, sparc_compare_op1); }
  operand0 = operands[0];
  operand1 = operands[1];
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (EQ, VOIDmode, operand1, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_bne (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ operands[1] = gen_compare_reg (NE, sparc_compare_op0, sparc_compare_op1); }
  operand0 = operands[0];
  operand1 = operands[1];
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (NE, VOIDmode, operand1, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_bgt (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ operands[1] = gen_compare_reg (GT, sparc_compare_op0, sparc_compare_op1); }
  operand0 = operands[0];
  operand1 = operands[1];
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (GT, VOIDmode, operand1, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_bgtu (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ operands[1] = gen_compare_reg (GTU, sparc_compare_op0, sparc_compare_op1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (GTU, VOIDmode, operand1, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_blt (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ operands[1] = gen_compare_reg (LT, sparc_compare_op0, sparc_compare_op1); }
  operand0 = operands[0];
  operand1 = operands[1];
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (LT, VOIDmode, operand1, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_bltu (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ operands[1] = gen_compare_reg (LTU, sparc_compare_op0, sparc_compare_op1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (LTU, VOIDmode, operand1, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_bge (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ operands[1] = gen_compare_reg (GE, sparc_compare_op0, sparc_compare_op1); }
  operand0 = operands[0];
  operand1 = operands[1];
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (GE, VOIDmode, operand1, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_bgeu (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ operands[1] = gen_compare_reg (GEU, sparc_compare_op0, sparc_compare_op1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (GEU, VOIDmode, operand1, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_ble (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ operands[1] = gen_compare_reg (LE, sparc_compare_op0, sparc_compare_op1); }
  operand0 = operands[0];
  operand1 = operands[1];
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (LE, VOIDmode, operand1, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_bleu (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{ operands[1] = gen_compare_reg (LEU, sparc_compare_op0, sparc_compare_op1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (LEU, VOIDmode, operand1, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_movsi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  if (emit_move_sequence (operands, SImode, NULL_RTX))
    DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, operand1));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_reload_insi (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  rtx operands[3];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;

{
  if (emit_move_sequence (operands, SImode, operands[2]))
    DONE;

  /* We don't want the clobber emitted, so handle this ourselves.  */
  emit_insn (gen_rtx (SET, VOIDmode, operands[0], operands[1]));
  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, operand1));
  emit_insn (gen_rtx (CLOBBER, VOIDmode, operand2));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_movhi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  if (emit_move_sequence (operands, HImode, NULL_RTX))
    DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, operand1));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_movqi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  if (emit_move_sequence (operands, QImode, NULL_RTX))
    DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, operand1));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_movtf (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  if (emit_move_sequence (operands, TFmode, NULL_RTX))
    DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, operand1));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_movdf (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  if (emit_move_sequence (operands, DFmode, NULL_RTX))
    DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, operand1));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_split_86 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx operand2;
  rtx operand3;
  rtx operand4;
  rtx operand5;
  rtx _val = 0;
  start_sequence ();

{ operands[2] = operand_subword (operands[0], 0, 0, DFmode);
  operands[3] = operand_subword (operands[1], 0, 0, DFmode);
  operands[4] = operand_subword (operands[0], 1, 0, DFmode);
  operands[5] = operand_subword (operands[1], 1, 0, DFmode); }
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  operand4 = operands[4];
  operand5 = operands[5];
  emit_insn (gen_rtx (SET, VOIDmode, operand2, operand3));
  emit_insn (gen_rtx (SET, VOIDmode, operand4, operand5));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_movdi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  if (emit_move_sequence (operands, DImode, NULL_RTX))
    DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, operand1));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_movsf (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  if (emit_move_sequence (operands, SFmode, NULL_RTX))
    DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, operand1));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_zero_extendhisi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  rtx temp = gen_reg_rtx (SImode);
  rtx shift_16 = gen_rtx (CONST_INT, VOIDmode, 16);

  if (GET_CODE (operand1) == SUBREG)
    operand1 = XEXP (operand1, 0);

  emit_insn (gen_ashlsi3 (temp, gen_rtx (SUBREG, SImode, operand1, 0),
			  shift_16));
  emit_insn (gen_lshrsi3 (operand0, temp, shift_16));
  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (ZERO_EXTEND, SImode, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_zero_extendqihi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ZERO_EXTEND, HImode, operand1));
}

rtx
gen_zero_extendqisi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ZERO_EXTEND, SImode, operand1));
}

rtx
gen_extendhisi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  rtx temp = gen_reg_rtx (SImode);
  rtx shift_16 = gen_rtx (CONST_INT, VOIDmode, 16);

  if (GET_CODE (operand1) == SUBREG)
    operand1 = XEXP (operand1, 0);

  emit_insn (gen_ashlsi3 (temp, gen_rtx (SUBREG, SImode, operand1, 0),
			  shift_16));
  emit_insn (gen_ashrsi3 (operand0, temp, shift_16));
  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (SIGN_EXTEND, SImode, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_extendqihi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  rtx temp = gen_reg_rtx (SImode);
  rtx shift_24 = gen_rtx (CONST_INT, VOIDmode, 24);

  if (GET_CODE (operand1) == SUBREG)
    operand1 = XEXP (operand1, 0);
  if (GET_CODE (operand0) == SUBREG)
    operand0 = XEXP (operand0, 0);
  emit_insn (gen_ashlsi3 (temp, gen_rtx (SUBREG, SImode, operand1, 0),
			  shift_24));
  if (GET_MODE (operand0) != SImode)
    operand0 = gen_rtx (SUBREG, SImode, operand0, 0);
  emit_insn (gen_ashrsi3 (operand0, temp, shift_24));
  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (SIGN_EXTEND, HImode, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_extendqisi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  rtx temp = gen_reg_rtx (SImode);
  rtx shift_24 = gen_rtx (CONST_INT, VOIDmode, 24);

  if (GET_CODE (operand1) == SUBREG)
    operand1 = XEXP (operand1, 0);
  emit_insn (gen_ashlsi3 (temp, gen_rtx (SUBREG, SImode, operand1, 0),
			  shift_24));
  emit_insn (gen_ashrsi3 (operand0, temp, shift_24));
  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (SIGN_EXTEND, SImode, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_extendsfdf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT_EXTEND, DFmode, operand1));
}

rtx
gen_extendsftf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT_EXTEND, TFmode, operand1));
}

rtx
gen_extenddftf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT_EXTEND, TFmode, operand1));
}

rtx
gen_truncdfsf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT_TRUNCATE, SFmode, operand1));
}

rtx
gen_trunctfsf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT_TRUNCATE, SFmode, operand1));
}

rtx
gen_trunctfdf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT_TRUNCATE, DFmode, operand1));
}

rtx
gen_floatsisf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT, SFmode, operand1));
}

rtx
gen_floatsidf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT, DFmode, operand1));
}

rtx
gen_floatsitf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT, TFmode, operand1));
}

rtx
gen_fix_truncsfsi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, SImode, gen_rtx (FIX, SFmode, operand1)));
}

rtx
gen_fix_truncdfsi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, SImode, gen_rtx (FIX, DFmode, operand1)));
}

rtx
gen_fix_trunctfsi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, SImode, gen_rtx (FIX, TFmode, operand1)));
}

rtx
gen_adddi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, DImode, operand1, operand2)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (REG, SImode, 0))));
}

rtx
gen_addsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, SImode, operand1, operand2));
}

rtx
gen_subdi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, DImode, operand1, operand2)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (REG, SImode, 0))));
}

rtx
gen_subsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, SImode, operand1, operand2));
}

rtx
gen_mulsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MULT, SImode, operand1, operand2));
}

rtx
gen_mulsidi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  rtx operands[3];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;

{
  if (CONSTANT_P (operands[2]))
    {
      emit_insn (gen_const_mulsidi3 (operands[0], operands[1], operands[2]));
      DONE;
    }
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (MULT, DImode, gen_rtx (SIGN_EXTEND, DImode, operand1), gen_rtx (SIGN_EXTEND, DImode, operand2))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_const_mulsidi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MULT, DImode, gen_rtx (SIGN_EXTEND, DImode, operand1), operand2));
}

rtx
gen_umulsidi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  rtx operands[3];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;

{
  if (CONSTANT_P (operands[2]))
    {
      emit_insn (gen_const_umulsidi3 (operands[0], operands[1], operands[2]));
      DONE;
    }
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (MULT, DImode, gen_rtx (ZERO_EXTEND, DImode, operand1), gen_rtx (ZERO_EXTEND, DImode, operand2))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_const_umulsidi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MULT, DImode, gen_rtx (ZERO_EXTEND, DImode, operand1), operand2));
}

rtx
gen_divsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (DIV, SImode, operand1, operand2)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0))));
}

rtx
gen_udivsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (UDIV, SImode, operand1, operand2));
}

rtx
gen_anddi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (AND, DImode, operand1, operand2));
}

rtx
gen_andsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (AND, SImode, operand1, operand2));
}

rtx
gen_split_147 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx operand2;
  rtx operand3;
  rtx operand4;
  rtx _val = 0;
  start_sequence ();

{
  operands[4] = gen_rtx (CONST_INT, VOIDmode, ~INTVAL (operands[2]));
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  operand4 = operands[4];
  emit_insn (gen_rtx (SET, VOIDmode, operand3, operand4));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (AND, SImode, gen_rtx (NOT, SImode, operand3), operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_iordi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (IOR, DImode, operand1, operand2));
}

rtx
gen_iorsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (IOR, SImode, operand1, operand2));
}

rtx
gen_split_153 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx operand2;
  rtx operand3;
  rtx operand4;
  rtx _val = 0;
  start_sequence ();

{
  operands[4] = gen_rtx (CONST_INT, VOIDmode, ~INTVAL (operands[2]));
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  operand4 = operands[4];
  emit_insn (gen_rtx (SET, VOIDmode, operand3, operand4));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (IOR, SImode, gen_rtx (NOT, SImode, operand3), operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_xordi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (XOR, DImode, operand1, operand2));
}

rtx
gen_xorsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (XOR, SImode, operand1, operand2));
}

rtx
gen_split_159 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx operand2;
  rtx operand3;
  rtx operand4;
  rtx _val = 0;
  start_sequence ();

{
  operands[4] = gen_rtx (CONST_INT, VOIDmode, ~INTVAL (operands[2]));
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  operand4 = operands[4];
  emit_insn (gen_rtx (SET, VOIDmode, operand3, operand4));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (NOT, SImode, gen_rtx (XOR, SImode, operand3, operand1))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_split_160 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx operand2;
  rtx operand3;
  rtx operand4;
  rtx _val = 0;
  start_sequence ();

{
  operands[4] = gen_rtx (CONST_INT, VOIDmode, ~INTVAL (operands[2]));
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  operand4 = operands[4];
  emit_insn (gen_rtx (SET, VOIDmode, operand3, operand4));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (XOR, SImode, operand3, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_negdi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, DImode, operand1)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (REG, SImode, 0))));
}

rtx
gen_negsi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, SImode, operand1));
}

rtx
gen_one_cmpldi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NOT, DImode, operand1));
}

rtx
gen_one_cmplsi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NOT, SImode, operand1));
}

rtx
gen_addtf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, TFmode, operand1, operand2));
}

rtx
gen_adddf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, DFmode, operand1, operand2));
}

rtx
gen_addsf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, SFmode, operand1, operand2));
}

rtx
gen_subtf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, TFmode, operand1, operand2));
}

rtx
gen_subdf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, DFmode, operand1, operand2));
}

rtx
gen_subsf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, SFmode, operand1, operand2));
}

rtx
gen_multf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MULT, TFmode, operand1, operand2));
}

rtx
gen_muldf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MULT, DFmode, operand1, operand2));
}

rtx
gen_mulsf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MULT, SFmode, operand1, operand2));
}

rtx
gen_divtf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (DIV, TFmode, operand1, operand2));
}

rtx
gen_divdf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (DIV, DFmode, operand1, operand2));
}

rtx
gen_divsf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (DIV, SFmode, operand1, operand2));
}

rtx
gen_negtf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, TFmode, operand1));
}

rtx
gen_negdf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, DFmode, operand1));
}

rtx
gen_negsf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, SFmode, operand1));
}

rtx
gen_abstf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ABS, TFmode, operand1));
}

rtx
gen_absdf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ABS, DFmode, operand1));
}

rtx
gen_abssf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ABS, SFmode, operand1));
}

rtx
gen_sqrttf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (SQRT, TFmode, operand1));
}

rtx
gen_sqrtdf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (SQRT, DFmode, operand1));
}

rtx
gen_sqrtsf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (SQRT, SFmode, operand1));
}

rtx
gen_ashldi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  rtx operands[3];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;

{
  if (GET_CODE (operands[1]) == CONST_DOUBLE
      && CONST_DOUBLE_HIGH (operands[1]) == 0
      && CONST_DOUBLE_LOW (operands[1]) == 1)
    operands[1] = const1_rtx;
  else if (operands[1] != const1_rtx)
    FAIL;
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFT, DImode, operand1, operand2)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (REG, SImode, 0)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_ashlsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFT, SImode, operand1, operand2));
}

rtx
gen_lshldi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  rtx operands[3];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;

{
  if (GET_CODE (operands[2]) != CONST_INT)
    FAIL;
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (LSHIFT, DImode, operand1, operand2)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_ashrsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFTRT, SImode, operand1, operand2));
}

rtx
gen_lshrsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (LSHIFTRT, SImode, operand1, operand2));
}

rtx
gen_lshrdi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  rtx operands[3];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;

{
  if (GET_CODE (operands[2]) != CONST_INT)
    FAIL;
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (LSHIFTRT, DImode, operand1, operand2)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_jump (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (LABEL_REF, VOIDmode, operand0));
}

rtx
gen_tablejump (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  /* We need to use the PC value in %o7 that was set up when the address
     of the label was loaded into a register, so we need different RTL.  */
  if (flag_pic)
    {
      emit_insn (gen_pic_tablejump (operands[0], operands[1]));
      DONE;
    }
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_jump_insn (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, pc_rtx, operand0),
		gen_rtx (USE, VOIDmode, gen_rtx (LABEL_REF, VOIDmode, operand1)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_pic_tablejump (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (3,
		gen_rtx (SET, VOIDmode, pc_rtx, operand0),
		gen_rtx (USE, VOIDmode, gen_rtx (LABEL_REF, VOIDmode, operand1)),
		gen_rtx (USE, VOIDmode, gen_rtx (REG, SImode, 15))));
}

rtx
gen_call (operand0, operand1, operand2, operand3)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
{
  rtx operands[4];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;
  operands[3] = operand3;

{
  rtx fn_rtx, nregs_rtx;

  if (GET_CODE (XEXP (operands[0], 0)) == LABEL_REF)
    {
      /* This is really a PIC sequence.  We want to represent
	 it as a funny jump so it's delay slots can be filled. 

	 ??? But if this really *is* a CALL, will not it clobber the
	 call-clobbered registers?  We lose this if it is a JUMP_INSN.
	 Why cannot we have delay slots filled if it were a CALL?  */

      if (INTVAL (operands[3]) > 0)
	emit_jump_insn (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (3,
				 gen_rtx (SET, VOIDmode, pc_rtx,
					  XEXP (operands[0], 0)),
				 operands[3],
				 gen_rtx (CLOBBER, VOIDmode,
					  gen_rtx (REG, SImode, 15)))));
      else
	emit_jump_insn (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
				 gen_rtx (SET, VOIDmode, pc_rtx,
					  XEXP (operands[0], 0)),
				 gen_rtx (CLOBBER, VOIDmode,
					  gen_rtx (REG, SImode, 15)))));
      goto finish_call;
    }

  fn_rtx = operands[0];

  /* Count the number of parameter registers being used by this call.
     if that argument is NULL, it means we are using them all, which
     means 6 on the sparc.  */
#if 0
  if (operands[2])
    nregs_rtx = gen_rtx (CONST_INT, VOIDmode, REGNO (operands[2]) - 8);
  else
    nregs_rtx = gen_rtx (CONST_INT, VOIDmode, 6);
#else
  nregs_rtx = const0_rtx;
#endif

  if (INTVAL (operands[3]) > 0)
    emit_call_insn (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (3,
			     gen_rtx (CALL, VOIDmode, fn_rtx, nregs_rtx),
			     operands[3],
			     gen_rtx (CLOBBER, VOIDmode,
					       gen_rtx (REG, SImode, 15)))));
  else
    emit_call_insn (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
			     gen_rtx (CALL, VOIDmode, fn_rtx, nregs_rtx),
			     gen_rtx (CLOBBER, VOIDmode,
					       gen_rtx (REG, SImode, 15)))));

 finish_call:
#if 0
  /* If this call wants a structure value,
     emit an unimp insn to let the called function know about this.  */
  if (INTVAL (operands[3]) > 0)
    {
      rtx insn = emit_insn (operands[3]);
      SCHED_GROUP_P (insn) = 1;
    }
#endif

  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  emit_call_insn (gen_rtx (CALL, VOIDmode, operand0, operand3));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_call_value (operand0, operand1, operand2, operand3, operand4)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
     rtx operand4;
{
  rtx operands[5];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;
  operands[3] = operand3;
  operands[4] = operand4;

{
  rtx fn_rtx, nregs_rtx;
  rtvec vec;

  fn_rtx = operands[1];

#if 0
  if (operands[3])
    nregs_rtx = gen_rtx (CONST_INT, VOIDmode, REGNO (operands[3]) - 8);
  else
    nregs_rtx = gen_rtx (CONST_INT, VOIDmode, 6);
#else
  nregs_rtx = const0_rtx;
#endif

  vec = gen_rtvec (2,
		   gen_rtx (SET, VOIDmode, operands[0],
			    gen_rtx (CALL, VOIDmode, fn_rtx, nregs_rtx)),
		   gen_rtx (CLOBBER, VOIDmode, gen_rtx (REG, SImode, 15)));

  emit_call_insn (gen_rtx (PARALLEL, VOIDmode, vec));

  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  operand4 = operands[4];
  emit_call_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (CALL, VOIDmode, operand1, operand4)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_untyped_call (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  rtx operands[3];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;

{
  operands[1] = change_address (operands[1], DImode, XEXP (operands[1], 0));
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_call_insn (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (4,
		gen_rtx (CALL, VOIDmode, operand0, const0_rtx),
		operand1,
		operand2,
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (REG, SImode, 15)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_untyped_return (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  rtx valreg1 = gen_rtx (REG, DImode, 24);
  rtx valreg2 = gen_rtx (REG, DFmode, 32);
  rtx result = operands[0];
  rtx rtnreg = gen_rtx (REG, SImode, (leaf_function ? 15 : 31));
  rtx value = gen_reg_rtx (SImode);

  /* Fetch the instruction where we will return to and see if it's an unimp
     instruction (the most significant 10 bits will be zero).  If so,
     update the return address to skip the unimp instruction.  */
  emit_move_insn (value,
		  gen_rtx (MEM, SImode, plus_constant (rtnreg, 8)));
  emit_insn (gen_lshrsi3 (value, value, GEN_INT (22)));
  emit_insn (gen_update_return (rtnreg, value));

  /* Reload the function value registers.  */
  emit_move_insn (valreg1, change_address (result, DImode, XEXP (result, 0)));
  emit_move_insn (valreg2,
		  change_address (result, DFmode,
				  plus_constant (XEXP (result, 0), 8)));

  /* Put USE insns before the return.  */
  emit_insn (gen_rtx (USE, VOIDmode, valreg1));
  emit_insn (gen_rtx (USE, VOIDmode, valreg2));

  /* Construct the return.  */
  expand_null_return ();

  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand0);
  emit (operand1);
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_update_return (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (UNSPEC, SImode, gen_rtvec (2,
		operand0,
		operand1), 0);
}

rtx
gen_return ()
{
  return gen_rtx (RETURN, VOIDmode);
}

rtx
gen_nop ()
{
  return const0_rtx;
}

rtx
gen_indirect_jump (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, pc_rtx, operand0);
}

rtx
gen_nonlocal_goto (operand0, operand1, operand2, operand3)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
{
  rtx operands[4];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;
  operands[3] = operand3;

{
  /* Trap instruction to flush all the registers window.  */
  emit_insn (gen_flush_register_windows ());
  /* Load the fp value for the containing fn into %fp.
     This is needed because operands[2] refers to %fp.
     Virtual register instantiation fails if the virtual %fp isn't set from a
     register.  Thus we must copy operands[0] into a register if it isn't
     already one.  */
  if (GET_CODE (operands[0]) != REG)
    operands[0] = force_reg (SImode, operands[0]);
  emit_move_insn (virtual_stack_vars_rtx, operands[0]);
  /* Find the containing function's current nonlocal goto handler,
     which will do any cleanups and then jump to the label.  */
  emit_move_insn (gen_rtx (REG, SImode, 8), operands[1]);
  /* Restore %fp from stack pointer value for containing function.
     The restore insn that follows will move this to %sp,
     and reload the appropriate value into %fp.  */
  emit_move_insn (frame_pointer_rtx, operands[2]);
  /* Put in the static chain register the nonlocal label address.  */
  emit_move_insn (static_chain_rtx, operands[3]);
  /* USE of frame_pointer_rtx added for consistency; not clear if
     really needed.  */
  emit_insn (gen_rtx (USE, VOIDmode, frame_pointer_rtx));
  emit_insn (gen_rtx (USE, VOIDmode, stack_pointer_rtx));
  emit_insn (gen_rtx (USE, VOIDmode, static_chain_rtx));
  emit_insn (gen_rtx (USE, VOIDmode, gen_rtx (REG, SImode, 8)));
  /* Return, restoring reg window and jumping to goto handler.  */
  emit_insn (gen_goto_handler_and_restore ());
  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  emit (operand0);
  emit (operand1);
  emit (operand2);
  emit (operand3);
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_flush_register_windows ()
{
  return gen_rtx (UNSPEC_VOLATILE, VOIDmode, gen_rtvec (1,
		const0_rtx), 0);
}

rtx
gen_goto_handler_and_restore ()
{
  return gen_rtx (UNSPEC_VOLATILE, VOIDmode, gen_rtvec (1,
		const0_rtx), 1);
}

rtx
gen_ffssi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (FFS, SImode, operand1)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0))));
}

rtx
gen_split_233 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx operand2;
  rtx operand3;
  rtx _val = 0;
  start_sequence ();

  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  emit_insn (gen_rtx (SET, VOIDmode, operand3, gen_rtx (HIGH, SImode, operand1)));
  emit_insn (gen_rtx (SET, VOIDmode, gen_rtx (GET_CODE (operand0), GET_MODE (operand0),
		gen_rtx (LO_SUM, SImode, operand3, operand1)), operand2));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_split_234 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx operand2;
  rtx _val = 0;
  start_sequence ();

{
  operands[1] = legitimize_pic_address (operands[1], GET_MODE (operands[0]),
					operands[3], 0);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, gen_rtx (GET_CODE (operand0), GET_MODE (operand0),
		operand1), operand2));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_split_235 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx operand2;
  rtx _val = 0;
  start_sequence ();

{
  operands[2] = legitimize_pic_address (operands[2], GET_MODE (operands[1]),
					operands[0], 0);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (GET_CODE (operand1), GET_MODE (operand1),
		operand2)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_split_236 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx operand2;
  rtx operand3;
  rtx _val = 0;
  start_sequence ();

{
  operands[3] = legitimize_pic_address (operands[3], GET_MODE (operands[2]),
					operands[0], 0);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (GET_CODE (operand1), GET_MODE (operand1),
		gen_rtx (GET_CODE (operand2), GET_MODE (operand2),
		operand3))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_split_237 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx _val = 0;
  start_sequence ();

  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (HIGH, SImode, operand1)));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (LO_SUM, SImode, operand0, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_split_238 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx _val = 0;
  start_sequence ();

{
  operands[1] = legitimize_pic_address (operands[1], Pmode, operands[0], 0);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, operand1));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_split_239 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx _val = 0;
  start_sequence ();

  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, gen_rtx (REG, CC_NOOVmode, 0), gen_rtx (COMPARE, CC_NOOVmode, gen_rtx (NEG, SImode, operand1), const0_rtx)));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (LTU, SImode, gen_rtx (REG, CCmode, 0), const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_split_240 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx _val = 0;
  start_sequence ();

  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, gen_rtx (REG, CC_NOOVmode, 0), gen_rtx (COMPARE, CC_NOOVmode, gen_rtx (NEG, SImode, operand1), const0_rtx)));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, SImode, gen_rtx (LTU, SImode, gen_rtx (REG, CCmode, 0), const0_rtx))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_split_241 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx _val = 0;
  start_sequence ();

  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, gen_rtx (REG, CC_NOOVmode, 0), gen_rtx (COMPARE, CC_NOOVmode, gen_rtx (NEG, SImode, operand1), const0_rtx)));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (GEU, SImode, gen_rtx (REG, CCmode, 0), const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_split_242 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx _val = 0;
  start_sequence ();

  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, gen_rtx (REG, CC_NOOVmode, 0), gen_rtx (COMPARE, CC_NOOVmode, gen_rtx (NEG, SImode, operand1), const0_rtx)));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, SImode, gen_rtx (GEU, SImode, gen_rtx (REG, CCmode, 0), const0_rtx))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_split_243 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx operand2;
  rtx _val = 0;
  start_sequence ();

  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, gen_rtx (REG, CC_NOOVmode, 0), gen_rtx (COMPARE, CC_NOOVmode, gen_rtx (NEG, SImode, operand1), const0_rtx)));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, SImode, gen_rtx (LTU, SImode, gen_rtx (REG, CCmode, 0), const0_rtx), operand2)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_split_244 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx operand2;
  rtx _val = 0;
  start_sequence ();

  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, gen_rtx (REG, CC_NOOVmode, 0), gen_rtx (COMPARE, CC_NOOVmode, gen_rtx (NEG, SImode, operand1), const0_rtx)));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, SImode, operand2, gen_rtx (LTU, SImode, gen_rtx (REG, CCmode, 0), const0_rtx))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_split_245 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx operand2;
  rtx _val = 0;
  start_sequence ();

  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, gen_rtx (REG, CC_NOOVmode, 0), gen_rtx (COMPARE, CC_NOOVmode, gen_rtx (NEG, SImode, operand1), const0_rtx)));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, SImode, gen_rtx (GEU, SImode, gen_rtx (REG, CCmode, 0), const0_rtx), operand2)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_split_246 (operands)
     rtx *operands;
{
  rtx operand0;
  rtx operand1;
  rtx operand2;
  rtx _val = 0;
  start_sequence ();

  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, gen_rtx (REG, CC_NOOVmode, 0), gen_rtx (COMPARE, CC_NOOVmode, gen_rtx (NEG, SImode, operand1), const0_rtx)));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, SImode, operand2, gen_rtx (GEU, SImode, gen_rtx (REG, CCmode, 0), const0_rtx))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}



void
add_clobbers (pattern, insn_code_number)
     rtx pattern;
     int insn_code_number;
{
  int i;

  switch (insn_code_number)
    {
    case 223:
      XVECEXP (pattern, 0, 3) = gen_rtx (CLOBBER, VOIDmode, gen_rtx (REG, SImode, 15));
      break;

    case 219:
      XVECEXP (pattern, 0, 2) = gen_rtx (CLOBBER, VOIDmode, gen_rtx (REG, SImode, 15));
      break;

    case 221:
    case 218:
      XVECEXP (pattern, 0, 1) = gen_rtx (CLOBBER, VOIDmode, gen_rtx (REG, SImode, 15));
      break;

    case 141:
      XVECEXP (pattern, 0, 2) = gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0));
      break;

    case 202:
    case 169:
    case 128:
    case 124:
      XVECEXP (pattern, 0, 1) = gen_rtx (CLOBBER, VOIDmode, gen_rtx (REG, SImode, 0));
      break;

    case 232:
    case 211:
    case 210:
    case 206:
    case 205:
    case 140:
    case 94:
    case 87:
    case 81:
    case 76:
    case 72:
    case 68:
      XVECEXP (pattern, 0, 1) = gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0));
      break;

    case 30:
    case 29:
    case 28:
    case 27:
    case 26:
    case 25:
    case 24:
    case 23:
      XVECEXP (pattern, 0, 1) = gen_rtx (CLOBBER, VOIDmode, gen_rtx (REG, CCmode, 0));
      break;

    default:
      abort ();
    }
}

void
init_mov_optab ()
{
#ifdef HAVE_movcc_noov
  if (HAVE_movcc_noov)
    mov_optab->handlers[(int) CC_NOOVmode].insn_code = CODE_FOR_movcc_noov;
#endif
#ifdef HAVE_movccfp
  if (HAVE_movccfp)
    mov_optab->handlers[(int) CCFPmode].insn_code = CODE_FOR_movccfp;
#endif
#ifdef HAVE_movccfpe
  if (HAVE_movccfpe)
    mov_optab->handlers[(int) CCFPEmode].insn_code = CODE_FOR_movccfpe;
#endif
}

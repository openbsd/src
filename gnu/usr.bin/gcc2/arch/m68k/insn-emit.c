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
gen_tstsi (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, cc0_rtx, operand0);
}

rtx
gen_tsthi (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, cc0_rtx, operand0);
}

rtx
gen_tstqi (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, cc0_rtx, operand0);
}

rtx
gen_tstsf (operand0)
     rtx operand0;
{
  rtx operands[1];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{
  if (TARGET_FPA)
    {
      emit_insn (gen_tstsf_fpa (operands[0]));
      DONE;
    }
}
  operand0 = operands[0];
  emit_insn (gen_rtx (SET, VOIDmode, cc0_rtx, operand0));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_tstsf_fpa (operand0)
     rtx operand0;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, cc0_rtx, operand0),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0))));
}

rtx
gen_tstdf (operand0)
     rtx operand0;
{
  rtx operands[1];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{
  if (TARGET_FPA)
    {
      emit_insn (gen_tstsf_fpa (operands[0]));
      DONE;
    }
}
  operand0 = operands[0];
  emit_insn (gen_rtx (SET, VOIDmode, cc0_rtx, operand0));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_tstdf_fpa (operand0)
     rtx operand0;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, cc0_rtx, operand0),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0))));
}

rtx
gen_cmpsi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1));
}

rtx
gen_cmphi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1));
}

rtx
gen_cmpqi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1));
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
  if (TARGET_FPA)
    {
      emit_insn (gen_cmpdf_fpa (operands[0], operands[1]));
      DONE;
    }
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_cmpdf_fpa (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0))));
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
  if (TARGET_FPA)
    {
      emit_insn (gen_cmpsf_fpa (operands[0], operands[1]));
      DONE;
    }
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_cmpsf_fpa (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0))));
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
  if (flag_pic && symbolic_operand (operands[1], SImode)) 
    {
      /* The source is an address which requires PIC relocation.  
         Call legitimize_pic_address with the source, mode, and a relocation
         register (a new pseudo, or the final destination if reload_in_progress
         is set).   Then fall through normally */
      extern rtx legitimize_pic_address();
      rtx temp = reload_in_progress ? operands[0] : gen_reg_rtx (Pmode);
      operands[1] = legitimize_pic_address (operands[1], SImode, temp);
    }
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
gen_movhi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, operand1);
}

rtx
gen_movstricthi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, gen_rtx (STRICT_LOW_PART, VOIDmode, operand0), operand1);
}

rtx
gen_movqi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, operand1);
}

rtx
gen_movstrictqi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, gen_rtx (STRICT_LOW_PART, VOIDmode, operand0), operand1);
}

rtx
gen_movsf (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, operand1);
}

rtx
gen_movdf (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, operand1);
}

rtx
gen_movxf (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  if (CONSTANT_P (operands[1]))
    {
      operands[1] = force_const_mem (XFmode, operands[1]);
      if (! memory_address_p (XFmode, XEXP (operands[1], 0))
	  && ! reload_in_progress)
	operands[1] = change_address (operands[1], XFmode,
				      XEXP (operands[1], 0));
    }
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
gen_movdi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, operand1);
}

rtx
gen_pushasi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, operand1);
}

rtx
gen_truncsiqi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (TRUNCATE, QImode, operand1));
}

rtx
gen_trunchiqi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (TRUNCATE, QImode, operand1));
}

rtx
gen_truncsihi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (TRUNCATE, HImode, operand1));
}

rtx
gen_zero_extendhisi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operand2;
  rtx operands[3];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  operands[1] = make_safe_from (operands[1], operands[0]);
  if (GET_CODE (operands[0]) == SUBREG)
    operands[2] = gen_rtx (SUBREG, HImode, SUBREG_REG (operands[0]),
			   SUBREG_WORD (operands[0]));
  else
    operands[2] = gen_rtx (SUBREG, HImode, operands[0], 0);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, const0_rtx));
  emit_insn (gen_rtx (SET, VOIDmode, gen_rtx (STRICT_LOW_PART, VOIDmode, operand2), operand1));
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
  rtx operand2;
  rtx operands[3];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  operands[1] = make_safe_from (operands[1], operands[0]);
  if (GET_CODE (operands[0]) == SUBREG)
    operands[2] = gen_rtx (SUBREG, QImode, SUBREG_REG (operands[0]),
			   SUBREG_WORD (operands[0]));
  else
    operands[2] = gen_rtx (SUBREG, QImode, operands[0], 0);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, const0_rtx));
  emit_insn (gen_rtx (SET, VOIDmode, gen_rtx (STRICT_LOW_PART, VOIDmode, operand2), operand1));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_zero_extendqisi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operand2;
  rtx operands[3];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  operands[1] = make_safe_from (operands[1], operands[0]);
  if (GET_CODE (operands[0]) == SUBREG)
    operands[2] = gen_rtx (SUBREG, QImode, SUBREG_REG (operands[0]),
			   SUBREG_WORD (operands[0]));
  else
    operands[2] = gen_rtx (SUBREG, QImode, operands[0], 0);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, const0_rtx));
  emit_insn (gen_rtx (SET, VOIDmode, gen_rtx (STRICT_LOW_PART, VOIDmode, operand2), operand1));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_extendhisi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (SIGN_EXTEND, SImode, operand1));
}

rtx
gen_extendqihi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (SIGN_EXTEND, HImode, operand1));
}

rtx
gen_extendqisi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (SIGN_EXTEND, SImode, operand1));
}

rtx
gen_extendsfdf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT_EXTEND, DFmode, operand1));
}

rtx
gen_truncdfsf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT_TRUNCATE, SFmode, operand1));
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
gen_floathisf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT, SFmode, operand1));
}

rtx
gen_floathidf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT, DFmode, operand1));
}

rtx
gen_floatqisf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT, SFmode, operand1));
}

rtx
gen_floatqidf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT, DFmode, operand1));
}

rtx
gen_fix_truncdfsi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (3,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, SImode, gen_rtx (FIX, DFmode, operand1))),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0))));
}

rtx
gen_fix_truncdfhi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (3,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, HImode, gen_rtx (FIX, DFmode, operand1))),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0))));
}

rtx
gen_fix_truncdfqi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (3,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, QImode, gen_rtx (FIX, DFmode, operand1))),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0))));
}

rtx
gen_ftruncdf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, DFmode, operand1));
}

rtx
gen_ftruncsf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, SFmode, operand1));
}

rtx
gen_fixsfqi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, QImode, operand1));
}

rtx
gen_fixsfhi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, HImode, operand1));
}

rtx
gen_fixsfsi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, SImode, operand1));
}

rtx
gen_fixdfqi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, QImode, operand1));
}

rtx
gen_fixdfhi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, HImode, operand1));
}

rtx
gen_fixdfsi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, SImode, operand1));
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
gen_addhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, HImode, operand1, operand2));
}

rtx
gen_addqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, QImode, operand1, operand2));
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
gen_subsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, SImode, operand1, operand2));
}

rtx
gen_subhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, HImode, operand1, operand2));
}

rtx
gen_subqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, QImode, operand1, operand2));
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
gen_mulhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MULT, HImode, operand1, operand2));
}

rtx
gen_mulhisi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MULT, SImode, gen_rtx (SIGN_EXTEND, SImode, operand1), gen_rtx (SIGN_EXTEND, SImode, operand2)));
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
gen_umulhisi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MULT, SImode, gen_rtx (ZERO_EXTEND, SImode, operand1), gen_rtx (ZERO_EXTEND, SImode, operand2)));
}

rtx
gen_umulsidi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, gen_rtx (SUBREG, SImode, operand0, 1), gen_rtx (MULT, SImode, operand1, operand2)),
		gen_rtx (SET, VOIDmode, gen_rtx (SUBREG, SImode, operand0, 0), gen_rtx (TRUNCATE, SImode, gen_rtx (LSHIFTRT, DImode, gen_rtx (MULT, DImode, gen_rtx (ZERO_EXTEND, DImode, operand1), gen_rtx (ZERO_EXTEND, DImode, operand2)), GEN_INT (32))))));
}

rtx
gen_mulsidi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, gen_rtx (SUBREG, SImode, operand0, 1), gen_rtx (MULT, SImode, operand1, operand2)),
		gen_rtx (SET, VOIDmode, gen_rtx (SUBREG, SImode, operand0, 0), gen_rtx (TRUNCATE, SImode, gen_rtx (ASHIFT, DImode, gen_rtx (MULT, DImode, gen_rtx (SIGN_EXTEND, DImode, operand1), gen_rtx (SIGN_EXTEND, DImode, operand2)), GEN_INT (32))))));
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
gen_divhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (DIV, HImode, operand1, operand2));
}

rtx
gen_divhisi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (TRUNCATE, HImode, gen_rtx (DIV, SImode, operand1, gen_rtx (SIGN_EXTEND, SImode, operand2))));
}

rtx
gen_udivhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (UDIV, HImode, operand1, operand2));
}

rtx
gen_udivhisi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (TRUNCATE, HImode, gen_rtx (UDIV, SImode, operand1, gen_rtx (ZERO_EXTEND, SImode, operand2))));
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
gen_modhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MOD, HImode, operand1, operand2));
}

rtx
gen_modhisi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (TRUNCATE, HImode, gen_rtx (MOD, SImode, operand1, gen_rtx (SIGN_EXTEND, SImode, operand2))));
}

rtx
gen_umodhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (UMOD, HImode, operand1, operand2));
}

rtx
gen_umodhisi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (TRUNCATE, HImode, gen_rtx (UMOD, SImode, operand1, gen_rtx (ZERO_EXTEND, SImode, operand2))));
}

rtx
gen_divmodsi4 (operand0, operand1, operand2, operand3)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (DIV, SImode, operand1, operand2)),
		gen_rtx (SET, VOIDmode, operand3, gen_rtx (MOD, SImode, operand1, operand2))));
}

rtx
gen_udivmodsi4 (operand0, operand1, operand2, operand3)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (UDIV, SImode, operand1, operand2)),
		gen_rtx (SET, VOIDmode, operand3, gen_rtx (UMOD, SImode, operand1, operand2))));
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
gen_andhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (AND, HImode, operand1, operand2));
}

rtx
gen_andqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (AND, QImode, operand1, operand2));
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
gen_iorhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (IOR, HImode, operand1, operand2));
}

rtx
gen_iorqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (IOR, QImode, operand1, operand2));
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
gen_xorhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (XOR, HImode, operand1, operand2));
}

rtx
gen_xorqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (XOR, QImode, operand1, operand2));
}

rtx
gen_negsi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, SImode, operand1));
}

rtx
gen_neghi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, HImode, operand1));
}

rtx
gen_negqi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, QImode, operand1));
}

rtx
gen_negsf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, SFmode, operand1));
}

rtx
gen_negdf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, DFmode, operand1));
}

rtx
gen_sqrtdf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (SQRT, DFmode, operand1));
}

rtx
gen_abssf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ABS, SFmode, operand1));
}

rtx
gen_absdf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ABS, DFmode, operand1));
}

rtx
gen_one_cmplsi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NOT, SImode, operand1));
}

rtx
gen_one_cmplhi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NOT, HImode, operand1));
}

rtx
gen_one_cmplqi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NOT, QImode, operand1));
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
gen_ashlhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFT, HImode, operand1, operand2));
}

rtx
gen_ashlqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFT, QImode, operand1, operand2));
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
gen_ashrhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFTRT, HImode, operand1, operand2));
}

rtx
gen_ashrqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFTRT, QImode, operand1, operand2));
}

rtx
gen_lshlsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (LSHIFT, SImode, operand1, operand2));
}

rtx
gen_lshlhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (LSHIFT, HImode, operand1, operand2));
}

rtx
gen_lshlqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (LSHIFT, QImode, operand1, operand2));
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
gen_lshrhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (LSHIFTRT, HImode, operand1, operand2));
}

rtx
gen_lshrqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (LSHIFTRT, QImode, operand1, operand2));
}

rtx
gen_rotlsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ROTATE, SImode, operand1, operand2));
}

rtx
gen_rotlhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ROTATE, HImode, operand1, operand2));
}

rtx
gen_rotlqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ROTATE, QImode, operand1, operand2));
}

rtx
gen_rotrsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ROTATERT, SImode, operand1, operand2));
}

rtx
gen_rotrhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ROTATERT, HImode, operand1, operand2));
}

rtx
gen_rotrqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ROTATERT, QImode, operand1, operand2));
}

rtx
gen_extv (operand0, operand1, operand2, operand3)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (SIGN_EXTRACT, SImode, operand1, operand2, operand3));
}

rtx
gen_extzv (operand0, operand1, operand2, operand3)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ZERO_EXTRACT, SImode, operand1, operand2, operand3));
}

rtx
gen_insv (operand0, operand1, operand2, operand3)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
{
  return gen_rtx (SET, VOIDmode, gen_rtx (ZERO_EXTRACT, SImode, operand0, operand1, operand2), operand3);
}

rtx
gen_seq (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (EQ, QImode, cc0_rtx, const0_rtx));
}

rtx
gen_sne (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NE, QImode, cc0_rtx, const0_rtx));
}

rtx
gen_sgt (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (GT, QImode, cc0_rtx, const0_rtx));
}

rtx
gen_sgtu (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (GTU, QImode, cc0_rtx, const0_rtx));
}

rtx
gen_slt (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (LT, QImode, cc0_rtx, const0_rtx));
}

rtx
gen_sltu (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (LTU, QImode, cc0_rtx, const0_rtx));
}

rtx
gen_sge (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (GE, QImode, cc0_rtx, const0_rtx));
}

rtx
gen_sgeu (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (GEU, QImode, cc0_rtx, const0_rtx));
}

rtx
gen_sle (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (LE, QImode, cc0_rtx, const0_rtx));
}

rtx
gen_sleu (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (LEU, QImode, cc0_rtx, const0_rtx));
}

rtx
gen_beq (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (EQ, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx));
}

rtx
gen_bne (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (NE, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx));
}

rtx
gen_bgt (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (GT, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx));
}

rtx
gen_bgtu (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (GTU, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx));
}

rtx
gen_blt (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (LT, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx));
}

rtx
gen_bltu (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (LTU, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx));
}

rtx
gen_bge (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (GE, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx));
}

rtx
gen_bgeu (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (GEU, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx));
}

rtx
gen_ble (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (LE, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx));
}

rtx
gen_bleu (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (LEU, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx));
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
#ifdef CASE_VECTOR_PC_RELATIVE
    operands[0] = gen_rtx (PLUS, SImode, pc_rtx, operands[0]);
#endif
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
gen_decrement_and_branch_until_zero (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (GE, VOIDmode, gen_rtx (PLUS, SImode, operand0, constm1_rtx), const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand1), pc_rtx)),
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, SImode, operand0, constm1_rtx))));
}

rtx
gen_call (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  if (flag_pic && GET_CODE (XEXP (operands[0], 0)) == SYMBOL_REF)
    SYMBOL_REF_FLAG (XEXP (operands[0], 0)) = 1;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_call_insn (gen_rtx (CALL, VOIDmode, operand0, operand1));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_call_value (operand0, operand1, operand2)
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
  if (flag_pic && GET_CODE (XEXP (operands[1], 0)) == SYMBOL_REF)
    SYMBOL_REF_FLAG (XEXP (operands[1], 0)) = 1;
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_call_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (CALL, VOIDmode, operand1, operand2)));
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
  int i;

  emit_call_insn (gen_call (operands[0], const0_rtx, NULL, const0_rtx));

  for (i = 0; i < XVECLEN (operands[2], 0); i++)
    {
      rtx set = XVECEXP (operands[2], 0, i);
      emit_move_insn (SET_DEST (set), SET_SRC (set));
    }

  /* The optimizer does not know that the call sets the function value
     registers we stored in the result block.  We avoid problems by
     claiming that all hard registers are used and clobbered at this
     point.  */
  emit_insn (gen_blockage ());

  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_call_insn (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (3,
		gen_rtx (CALL, VOIDmode, operand0, const0_rtx),
		operand1,
		operand2)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_blockage ()
{
  return gen_rtx (UNSPEC_VOLATILE, VOIDmode, gen_rtvec (1,
		const0_rtx), 0);
}

rtx
gen_nop ()
{
  return const0_rtx;
}

rtx
gen_probe ()
{
  return gen_rtx (REG, SImode, 15);
}

rtx
gen_return ()
{
  return gen_rtx (RETURN, VOIDmode);
}

rtx
gen_indirect_jump (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, pc_rtx, operand0);
}

rtx
gen_tstxf (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, cc0_rtx, operand0);
}

rtx
gen_cmpxf (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  if (CONSTANT_P (operands[0]))
      operands[0] = force_const_mem (XFmode, operands[0]);
  if (CONSTANT_P (operands[1]))
      operands[1] = force_const_mem (XFmode, operands[1]);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_extendsfxf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT_EXTEND, XFmode, operand1));
}

rtx
gen_extenddfxf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT_EXTEND, XFmode, operand1));
}

rtx
gen_truncxfdf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT_TRUNCATE, DFmode, operand1));
}

rtx
gen_truncxfsf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT_TRUNCATE, SFmode, operand1));
}

rtx
gen_floatsixf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT, XFmode, operand1));
}

rtx
gen_floathixf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT, XFmode, operand1));
}

rtx
gen_floatqixf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT, XFmode, operand1));
}

rtx
gen_ftruncxf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, XFmode, operand1));
}

rtx
gen_fixxfqi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, QImode, operand1));
}

rtx
gen_fixxfhi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, HImode, operand1));
}

rtx
gen_fixxfsi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, SImode, operand1));
}

rtx
gen_addxf3 (operand0, operand1, operand2)
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
  if (CONSTANT_P (operands[1]))
    operands[1] = force_const_mem (XFmode, operands[1]);
  if (CONSTANT_P (operands[2]))
    operands[2] = force_const_mem (XFmode, operands[2]);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, XFmode, operand1, operand2)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_subxf3 (operand0, operand1, operand2)
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
  if (CONSTANT_P (operands[1]))
    operands[1] = force_const_mem (XFmode, operands[1]);
  if (CONSTANT_P (operands[2]))
    operands[2] = force_const_mem (XFmode, operands[2]);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, XFmode, operand1, operand2)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_mulxf3 (operand0, operand1, operand2)
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
  if (CONSTANT_P (operands[1]))
    operands[1] = force_const_mem (XFmode, operands[1]);
  if (CONSTANT_P (operands[2]))
    operands[2] = force_const_mem (XFmode, operands[2]);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (MULT, XFmode, operand1, operand2)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_divxf3 (operand0, operand1, operand2)
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
  if (CONSTANT_P (operands[1]))
    operands[1] = force_const_mem (XFmode, operands[1]);
  if (CONSTANT_P (operands[2]))
    operands[2] = force_const_mem (XFmode, operands[2]);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (DIV, XFmode, operand1, operand2)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_negxf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, XFmode, operand1));
}

rtx
gen_absxf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ABS, XFmode, operand1));
}

rtx
gen_sqrtxf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (SQRT, XFmode, operand1));
}



void
add_clobbers (pattern, insn_code_number)
     rtx pattern;
     int insn_code_number;
{
  int i;

  switch (insn_code_number)
    {
    case 72:
    case 71:
    case 70:
      XVECEXP (pattern, 0, 1) = gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0));
      XVECEXP (pattern, 0, 2) = gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0));
      break;

    case 18:
    case 15:
    case 9:
    case 6:
      XVECEXP (pattern, 0, 1) = gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0));
      break;

    default:
      abort ();
    }
}

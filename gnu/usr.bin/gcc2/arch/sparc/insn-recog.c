/* Generated automatically by the program `genrecog'
from the machine description file `md'.  */

#include "config.h"
#include "rtl.h"
#include "insn-config.h"
#include "recog.h"
#include "real.h"
#include "output.h"
#include "flags.h"

extern rtx gen_split_86 ();
extern rtx gen_split_147 ();
extern rtx gen_split_153 ();
extern rtx gen_split_159 ();
extern rtx gen_split_160 ();
extern rtx gen_split_233 ();
extern rtx gen_split_234 ();
extern rtx gen_split_235 ();
extern rtx gen_split_236 ();
extern rtx gen_split_237 ();
extern rtx gen_split_238 ();
extern rtx gen_split_239 ();
extern rtx gen_split_240 ();
extern rtx gen_split_241 ();
extern rtx gen_split_242 ();
extern rtx gen_split_243 ();
extern rtx gen_split_244 ();
extern rtx gen_split_245 ();
extern rtx gen_split_246 ();

/* `recog' contains a decision tree
   that recognizes whether the rtx X0 is a valid instruction.

   recog returns -1 if the rtx is not valid.
   If the rtx is valid, recog returns a nonnegative number
   which is the insn code number for the pattern that matched.
   This is the same as the order in the machine description of
   the entry that matched.  This number can be used as an index into
   entry that matched.  This number can be used as an index into various
   insn_* tables, such as insn_templates, insn_outfun, and insn_n_operands
   (found in insn-output.c).

   The third argument to recog is an optional pointer to an int.
   If present, recog will accept a pattern if it matches except for
   missing CLOBBER expressions at the end.  In that case, the value
   pointed to by the optional pointer will be set to the number of
   CLOBBERs that need to be added (it should be initialized to zero by
   the caller).  If it is set nonzero, the caller should allocate a
   PARALLEL of the appropriate size, copy the initial entries, and call
   add_clobbers (found in insn-emit.c) to fill in the CLOBBERs.

   The function split_insns returns 0 if the rtl could not
   be split or the split rtl in a SEQUENCE if it can be.*/

rtx recog_operand[MAX_RECOG_OPERANDS];

rtx *recog_operand_loc[MAX_RECOG_OPERANDS];

rtx *recog_dup_loc[MAX_DUP_OPERANDS];

char recog_dup_num[MAX_DUP_OPERANDS];

#define operands recog_operand

int
recog_1 (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5;
  int tem;

  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case SImode:
      switch (GET_CODE (x1))
	{
	case NE:
	  goto L46;
	case NEG:
	  goto L60;
	case EQ:
	  goto L74;
	case PLUS:
	  goto L104;
	case MINUS:
	  goto L173;
	case LTU:
	  goto L162;
	case GEU:
	  goto L189;
	}
    }
  L254:
  if (noov_compare_op (x1, SImode))
    {
      ro[1] = x1;
      goto L255;
    }
  goto ret0;

  L46:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L47;
    }
  goto L254;

  L47:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && pnum_clobbers != 0 && 1)
    {
      *pnum_clobbers = 1;
      return 23;
    }
  goto L254;

  L60:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) != SImode)
    {
      goto L254;
    }
  switch (GET_CODE (x2))
    {
    case NE:
      goto L61;
    case EQ:
      goto L89;
    case LTU:
      goto L168;
    case PLUS:
      goto L182;
    case GEU:
      goto L195;
    }
  goto L254;

  L61:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L62;
    }
  goto L254;

  L62:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && pnum_clobbers != 0 && 1)
    {
      *pnum_clobbers = 1;
      return 24;
    }
  goto L254;

  L89:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L90;
    }
  goto L254;

  L90:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && pnum_clobbers != 0 && 1)
    {
      *pnum_clobbers = 1;
      return 26;
    }
  goto L254;

  L168:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == CCmode && GET_CODE (x3) == REG && XINT (x3, 0) == 0 && 1)
    goto L169;
  goto L254;

  L169:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    return 32;
  goto L254;

  L182:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == LTU && 1)
    goto L183;
  goto L254;

  L183:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) == CCmode && GET_CODE (x4) == REG && XINT (x4, 0) == 0 && 1)
    goto L184;
  goto L254;

  L184:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L185;
  goto L254;

  L185:
  x3 = XEXP (x2, 1);
  if (arith_operand (x3, SImode))
    {
      ro[1] = x3;
      return 34;
    }
  goto L254;

  L195:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == CCmode && GET_CODE (x3) == REG && XINT (x3, 0) == 0 && 1)
    goto L196;
  goto L254;

  L196:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    return 36;
  goto L254;

  L74:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L75;
    }
  goto L254;

  L75:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && pnum_clobbers != 0 && 1)
    {
      *pnum_clobbers = 1;
      return 25;
    }
  goto L254;

  L104:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) != SImode)
    {
      goto L254;
    }
  switch (GET_CODE (x2))
    {
    case NE:
      goto L105;
    case EQ:
      goto L139;
    case LTU:
      goto L201;
    case GEU:
      goto L242;
    }
  goto L254;

  L105:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L106;
    }
  goto L254;

  L106:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L107;
  goto L254;

  L107:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && register_operand (x2, SImode))
    {
      ro[2] = x2;
      *pnum_clobbers = 1;
      return 27;
    }
  goto L254;

  L139:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L140;
    }
  goto L254;

  L140:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L141;
  goto L254;

  L141:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && register_operand (x2, SImode))
    {
      ro[2] = x2;
      *pnum_clobbers = 1;
      return 29;
    }
  goto L254;

  L201:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == CCmode && GET_CODE (x3) == REG && XINT (x3, 0) == 0 && 1)
    goto L202;
  goto L254;

  L202:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L203;
  goto L254;

  L203:
  x2 = XEXP (x1, 1);
  if (arith_operand (x2, SImode))
    {
      ro[1] = x2;
      return 37;
    }
  L210:
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == PLUS && 1)
    goto L211;
  goto L254;

  L211:
  x3 = XEXP (x2, 0);
  if (arith_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L212;
    }
  goto L254;

  L212:
  x3 = XEXP (x2, 1);
  if (arith_operand (x3, SImode))
    {
      ro[2] = x3;
      return 38;
    }
  goto L254;

  L242:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == CCmode && GET_CODE (x3) == REG && XINT (x3, 0) == 0 && 1)
    goto L243;
  goto L254;

  L243:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L244;
  goto L254;

  L244:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      return 42;
    }
  goto L254;

  L173:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) != SImode)
    {
      goto L254;
    }
  switch (GET_CODE (x2))
    {
    case NEG:
      goto L174;
    case MINUS:
      goto L224;
    case SUBREG:
    case REG:
      if (register_operand (x2, SImode))
	{
	  ro[2] = x2;
	  goto L122;
	}
    }
  L216:
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L217;
    }
  goto L254;

  L174:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == LTU && 1)
    goto L175;
  goto L254;

  L175:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) == CCmode && GET_CODE (x4) == REG && XINT (x4, 0) == 0 && 1)
    goto L176;
  goto L254;

  L176:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L177;
  goto L254;

  L177:
  x2 = XEXP (x1, 1);
  if (arith_operand (x2, SImode))
    {
      ro[1] = x2;
      return 33;
    }
  goto L254;

  L224:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L225;
    }
  goto L254;

  L225:
  x3 = XEXP (x2, 1);
  if (arith_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L226;
    }
  goto L254;

  L226:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == LTU && 1)
    goto L227;
  goto L254;

  L227:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == CCmode && GET_CODE (x3) == REG && XINT (x3, 0) == 0 && 1)
    goto L228;
  goto L254;

  L228:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    return 40;
  goto L254;

  L122:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != SImode)
    {
      x2 = XEXP (x1, 0);
      goto L216;
    }
  switch (GET_CODE (x2))
    {
    case NE:
      goto L123;
    case EQ:
      goto L157;
    }
  x2 = XEXP (x1, 0);
  goto L216;

  L123:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L124;
    }
  x2 = XEXP (x1, 0);
  goto L216;

  L124:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && pnum_clobbers != 0 && 1)
    {
      *pnum_clobbers = 1;
      return 28;
    }
  x2 = XEXP (x1, 0);
  goto L216;

  L157:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L158;
    }
  x2 = XEXP (x1, 0);
  goto L216;

  L158:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && pnum_clobbers != 0 && 1)
    {
      *pnum_clobbers = 1;
      return 30;
    }
  x2 = XEXP (x1, 0);
  goto L216;

  L217:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != SImode)
    {
      goto L254;
    }
  switch (GET_CODE (x2))
    {
    case LTU:
      goto L218;
    case PLUS:
      goto L234;
    case GEU:
      goto L250;
    }
  goto L254;

  L218:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == CCmode && GET_CODE (x3) == REG && XINT (x3, 0) == 0 && 1)
    goto L219;
  goto L254;

  L219:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    return 39;
  goto L254;

  L234:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == LTU && 1)
    goto L235;
  goto L254;

  L235:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) == CCmode && GET_CODE (x4) == REG && XINT (x4, 0) == 0 && 1)
    goto L236;
  goto L254;

  L236:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L237;
  goto L254;

  L237:
  x3 = XEXP (x2, 1);
  if (arith_operand (x3, SImode))
    {
      ro[2] = x3;
      return 41;
    }
  goto L254;

  L250:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == CCmode && GET_CODE (x3) == REG && XINT (x3, 0) == 0 && 1)
    goto L251;
  goto L254;

  L251:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    return 43;
  goto L254;

  L162:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    goto L163;
  goto L254;

  L163:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 31;
  goto L254;

  L189:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    goto L190;
  goto L254;

  L190:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 35;
  goto L254;

  L255:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    goto L256;
  goto ret0;

  L256:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 44;
  goto ret0;
 ret0: return -1;
}

int
recog_2 (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5;
  int tem;

  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != SImode)
    goto ret0;
  switch (GET_CODE (x1))
    {
    case HIGH:
      goto L292;
    case LO_SUM:
      goto L310;
    case ZERO_EXTEND:
      goto L443;
    case SIGN_EXTEND:
      goto L488;
    case FIX:
      goto L544;
    case PLUS:
      goto L572;
    case MINUS:
      goto L610;
    case MULT:
      goto L635;
    case DIV:
      goto L687;
    case UDIV:
      goto L720;
    case AND:
      goto L743;
    case IOR:
      goto L773;
    case XOR:
      goto L803;
    case NOT:
      goto L831;
    case ASHIFT:
      goto L1078;
    case ASHIFTRT:
      goto L1113;
    case LSHIFTRT:
      goto L1118;
    }
  goto ret0;

  L292:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == UNSPEC && XINT (x2, 1) == 0 && XVECLEN (x2, 0) == 1 && 1)
    goto L293;
  L297:
  ro[1] = x2;
  if (check_pic (1))
    return 63;
  goto ret0;

  L293:
  x3 = XVECEXP (x2, 0, 0);
  ro[1] = x3;
  if (check_pic (1))
    return 62;
  goto L297;

  L310:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L311;
    }
  goto ret0;

  L311:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == UNSPEC && XINT (x2, 1) == 0 && XVECLEN (x2, 0) == 1 && 1)
    goto L312;
  if (immediate_operand (x2, SImode))
    {
      ro[2] = x2;
      return 67;
    }
  goto ret0;

  L312:
  x3 = XVECEXP (x2, 0, 0);
  if (immediate_operand (x3, SImode))
    {
      ro[2] = x3;
      return 66;
    }
  goto ret0;

  L443:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      if (memory_operand (x2, HImode))
	{
	  ro[1] = x2;
	  return 96;
	}
      break;
    case QImode:
      if (sparc_operand (x2, QImode))
	{
	  ro[1] = x2;
	  if (GET_CODE (operands[1]) != CONST_INT)
	    return 100;
	  }
    }
  goto ret0;

  L488:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      if (memory_operand (x2, HImode))
	{
	  ro[1] = x2;
	  return 106;
	}
      break;
    case QImode:
      if (memory_operand (x2, QImode))
	{
	  ro[1] = x2;
	  return 110;
	}
    }
  goto ret0;

  L544:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) != FIX)
    goto ret0;
  switch (GET_MODE (x2))
    {
    case SFmode:
      goto L545;
    case DFmode:
      goto L550;
    case TFmode:
      goto L555;
    }
  goto ret0;

  L545:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_FPU)
	return 121;
      }
  goto ret0;

  L550:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DFmode))
    {
      ro[1] = x3;
      if (TARGET_FPU)
	return 122;
      }
  goto ret0;

  L555:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, TFmode))
    {
      ro[1] = x3;
      if (TARGET_FPU)
	return 123;
      }
  goto ret0;

  L572:
  x2 = XEXP (x1, 0);
  if (arith_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L573;
    }
  goto ret0;

  L573:
  x2 = XEXP (x1, 1);
  if (arith_operand (x2, SImode))
    {
      ro[2] = x2;
      return 125;
    }
  goto ret0;

  L610:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L611;
    }
  goto ret0;

  L611:
  x2 = XEXP (x1, 1);
  if (arith_operand (x2, SImode))
    {
      ro[2] = x2;
      return 129;
    }
  goto ret0;

  L635:
  x2 = XEXP (x1, 0);
  if (arith_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L636;
    }
  goto ret0;

  L636:
  x2 = XEXP (x1, 1);
  if (arith_operand (x2, SImode))
    {
      ro[2] = x2;
      if (TARGET_V8 || TARGET_SPARCLITE)
	return 132;
      }
  goto ret0;

  L687:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L688;
    }
  goto ret0;

  L688:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && arith_operand (x2, SImode))
    {
      ro[2] = x2;
      if (TARGET_V8)
	{
	  *pnum_clobbers = 1;
	  return 140;
	}
      }
  goto ret0;

  L720:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L721;
    }
  goto ret0;

  L721:
  x2 = XEXP (x1, 1);
  if (arith_operand (x2, SImode))
    {
      ro[2] = x2;
      if (TARGET_V8)
	return 142;
      }
  goto ret0;

  L743:
  x2 = XEXP (x1, 0);
  if (arith_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L744;
    }
  L762:
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == NOT && 1)
    goto L763;
  goto ret0;

  L744:
  x2 = XEXP (x1, 1);
  if (arith_operand (x2, SImode))
    {
      ro[2] = x2;
      return 146;
    }
  x2 = XEXP (x1, 0);
  goto L762;

  L763:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L764;
    }
  goto ret0;

  L764:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, SImode))
    {
      ro[2] = x2;
      return 149;
    }
  goto ret0;

  L773:
  x2 = XEXP (x1, 0);
  if (arith_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L774;
    }
  L792:
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == NOT && 1)
    goto L793;
  goto ret0;

  L774:
  x2 = XEXP (x1, 1);
  if (arith_operand (x2, SImode))
    {
      ro[2] = x2;
      return 152;
    }
  x2 = XEXP (x1, 0);
  goto L792;

  L793:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L794;
    }
  goto ret0;

  L794:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, SImode))
    {
      ro[2] = x2;
      return 155;
    }
  goto ret0;

  L803:
  x2 = XEXP (x1, 0);
  if (arith_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L804;
    }
  goto ret0;

  L804:
  x2 = XEXP (x1, 1);
  if (arith_operand (x2, SImode))
    {
      ro[2] = x2;
      return 158;
    }
  goto ret0;

  L831:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == XOR && 1)
    goto L832;
  L934:
  if (arith_operand (x2, SImode))
    {
      ro[1] = x2;
      return 175;
    }
  goto ret0;

  L832:
  x3 = XEXP (x2, 0);
  if (reg_or_0_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L833;
    }
  goto L934;

  L833:
  x3 = XEXP (x2, 1);
  if (arith_operand (x3, SImode))
    {
      ro[2] = x3;
      return 162;
    }
  goto L934;

  L1078:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1079;
    }
  goto ret0;

  L1079:
  x2 = XEXP (x1, 1);
  if (arith_operand (x2, SImode))
    {
      ro[2] = x2;
      return 203;
    }
  goto ret0;

  L1113:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1114;
    }
  goto ret0;

  L1114:
  x2 = XEXP (x1, 1);
  if (arith_operand (x2, SImode))
    {
      ro[2] = x2;
      return 207;
    }
  goto ret0;

  L1118:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1119;
    }
  goto ret0;

  L1119:
  x2 = XEXP (x1, 1);
  if (arith_operand (x2, SImode))
    {
      ro[2] = x2;
      return 208;
    }
  goto ret0;
 ret0: return -1;
}

int
recog_3 (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5;
  int tem;

  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != DImode)
    goto ret0;
  switch (GET_CODE (x1))
    {
    case HIGH:
      goto L288;
    case LO_SUM:
      goto L305;
    case PLUS:
      goto L567;
    case MINUS:
      goto L605;
    case MULT:
      goto L653;
    case AND:
      goto L738;
    case IOR:
      goto L768;
    case XOR:
      goto L798;
    case NOT:
      goto L825;
    case NEG:
      goto L905;
    case ASHIFT:
      goto L1073;
    case LSHIFT:
      goto L1091;
    case LSHIFTRT:
      goto L1131;
    }
  goto ret0;

  L288:
  x2 = XEXP (x1, 0);
  ro[1] = x2;
  if (check_pic (1))
    return 61;
  goto ret0;

  L305:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L306;
    }
  goto ret0;

  L306:
  x2 = XEXP (x1, 1);
  if (immediate_operand (x2, DImode))
    {
      ro[2] = x2;
      return 65;
    }
  goto ret0;

  L567:
  x2 = XEXP (x1, 0);
  if (arith_double_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L568;
    }
  goto ret0;

  L568:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && arith_double_operand (x2, DImode))
    {
      ro[2] = x2;
      *pnum_clobbers = 1;
      return 124;
    }
  goto ret0;

  L605:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L606;
    }
  goto ret0;

  L606:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && arith_double_operand (x2, DImode))
    {
      ro[2] = x2;
      *pnum_clobbers = 1;
      return 128;
    }
  goto ret0;

  L653:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) != DImode)
    goto ret0;
  switch (GET_CODE (x2))
    {
    case SIGN_EXTEND:
      goto L654;
    case ZERO_EXTEND:
      goto L667;
    }
  goto ret0;

  L654:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L655;
    }
  goto ret0;

  L655:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == SIGN_EXTEND && 1)
    goto L656;
  L662:
  if (small_int (x2, SImode))
    {
      ro[2] = x2;
      if (TARGET_V8 || TARGET_SPARCLITE)
	return 136;
      }
  goto ret0;

  L656:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[2] = x3;
      if (TARGET_V8 || TARGET_SPARCLITE)
	return 135;
      }
  goto L662;

  L667:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L668;
    }
  goto ret0;

  L668:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == ZERO_EXTEND && 1)
    goto L669;
  L675:
  if (small_int (x2, SImode))
    {
      ro[2] = x2;
      if (TARGET_V8 || TARGET_SPARCLITE)
	return 139;
      }
  goto ret0;

  L669:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[2] = x3;
      if (TARGET_V8 || TARGET_SPARCLITE)
	return 138;
      }
  goto L675;

  L738:
  x2 = XEXP (x1, 0);
  if (arith_double_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L739;
    }
  L756:
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == NOT && 1)
    goto L757;
  goto ret0;

  L739:
  x2 = XEXP (x1, 1);
  if (arith_double_operand (x2, DImode))
    {
      ro[2] = x2;
      return 145;
    }
  x2 = XEXP (x1, 0);
  goto L756;

  L757:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L758;
    }
  goto ret0;

  L758:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, DImode))
    {
      ro[2] = x2;
      return 148;
    }
  goto ret0;

  L768:
  x2 = XEXP (x1, 0);
  if (arith_double_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L769;
    }
  L786:
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == NOT && 1)
    goto L787;
  goto ret0;

  L769:
  x2 = XEXP (x1, 1);
  if (arith_double_operand (x2, DImode))
    {
      ro[2] = x2;
      return 151;
    }
  x2 = XEXP (x1, 0);
  goto L786;

  L787:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L788;
    }
  goto ret0;

  L788:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, DImode))
    {
      ro[2] = x2;
      return 154;
    }
  goto ret0;

  L798:
  x2 = XEXP (x1, 0);
  if (arith_double_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L799;
    }
  goto ret0;

  L799:
  x2 = XEXP (x1, 1);
  if (arith_double_operand (x2, DImode))
    {
      ro[2] = x2;
      return 157;
    }
  goto ret0;

  L825:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == XOR && 1)
    goto L826;
  L930:
  if (arith_double_operand (x2, DImode))
    {
      ro[1] = x2;
      return 174;
    }
  goto ret0;

  L826:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L827;
    }
  goto L930;

  L827:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, DImode))
    {
      ro[2] = x3;
      return 161;
    }
  goto L930;

  L905:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && register_operand (x2, DImode))
    {
      ro[1] = x2;
      *pnum_clobbers = 1;
      return 169;
    }
  goto ret0;

  L1073:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 1 && 1)
    goto L1074;
  goto ret0;

  L1074:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && register_operand (x2, SImode))
    {
      ro[1] = x2;
      *pnum_clobbers = 1;
      return 202;
    }
  goto ret0;

  L1091:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L1092;
    }
  goto ret0;

  L1092:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    goto L1108;
  goto ret0;

  L1108:
  if (pnum_clobbers != 0 && 1)
    {
      ro[2] = x2;
      if (INTVAL (operands[2]) < 32)
	{
	  *pnum_clobbers = 1;
	  return 205;
	}
      }
  L1109:
  if (pnum_clobbers != 0 && 1)
    {
      ro[2] = x2;
      if (INTVAL (operands[2]) >= 32)
	{
	  *pnum_clobbers = 1;
	  return 206;
	}
      }
  goto ret0;

  L1131:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L1132;
    }
  goto ret0;

  L1132:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    goto L1148;
  goto ret0;

  L1148:
  if (pnum_clobbers != 0 && 1)
    {
      ro[2] = x2;
      if (INTVAL (operands[2]) < 32)
	{
	  *pnum_clobbers = 1;
	  return 210;
	}
      }
  L1149:
  if (pnum_clobbers != 0 && 1)
    {
      ro[2] = x2;
      if (INTVAL (operands[2]) >= 32)
	{
	  *pnum_clobbers = 1;
	  return 211;
	}
      }
  goto ret0;
 ret0: return -1;
}

int
recog_4 (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5;
  int tem;

  x1 = XEXP (x0, 0);
  switch (GET_MODE (x1))
    {
    case CCmode:
      switch (GET_CODE (x1))
	{
	case REG:
	  if (XINT (x1, 0) == 0 && 1)
	    goto L2;
	}
      break;
    case CCFPEmode:
      switch (GET_CODE (x1))
	{
	case REG:
	  if (XINT (x1, 0) == 0 && 1)
	    goto L7;
	}
      break;
    case CCFPmode:
      if (GET_CODE (x1) == REG && XINT (x1, 0) == 0 && 1)
	goto L22;
      break;
    case SImode:
      if (register_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L45;
	}
    }
  if (GET_CODE (x1) == PC && 1)
    goto L259;
  L276:
  switch (GET_MODE (x1))
    {
    case SImode:
      if (reg_or_nonsymb_mem_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L277;
	}
    L290:
      if (register_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L291;
	}
      break;
    case DImode:
      if (register_operand (x1, DImode))
	{
	  ro[0] = x1;
	  goto L287;
	}
    L416:
      if (reg_or_nonsymb_mem_operand (x1, DImode))
	{
	  ro[0] = x1;
	  goto L417;
	}
      break;
    case HImode:
      if (register_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L300;
	}
    L330:
      if (reg_or_nonsymb_mem_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L331;
	}
    L333:
      if (register_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L334;
	}
    }
  switch (GET_MODE (x1))
    {
    case SImode:
      if (GET_CODE (x1) == MEM && 1)
	goto L327;
    L907:
      if (general_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L908;
	}
      break;
    case HImode:
      if (GET_CODE (x1) == MEM && 1)
	goto L346;
      break;
    case QImode:
      if (reg_or_nonsymb_mem_operand (x1, QImode))
	{
	  ro[0] = x1;
	  goto L350;
	}
    L352:
      if (register_operand (x1, QImode))
	{
	  ro[0] = x1;
	  goto L353;
	}
      if (GET_CODE (x1) == MEM && 1)
	goto L366;
      break;
    case TFmode:
      if (general_operand (x1, TFmode))
	{
	  ro[0] = x1;
	  goto L370;
	}
    L372:
      if (reg_or_nonsymb_mem_operand (x1, TFmode))
	{
	  ro[0] = x1;
	  goto L373;
	}
    L510:
      if (register_operand (x1, TFmode))
	{
	  ro[0] = x1;
	  goto L511;
	}
      if (GET_CODE (x1) == MEM && 1)
	goto L388;
      break;
    case DFmode:
      if (general_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L392;
	}
    L394:
      if (reg_or_nonsymb_mem_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L395;
	}
    L506:
      if (register_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L507;
	}
      if (GET_CODE (x1) == MEM && 1)
	goto L413;
      break;
    case SFmode:
      if (general_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L420;
	}
    L422:
      if (reg_or_nonsymb_mem_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L423;
	}
    L518:
      if (register_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L519;
	}
      switch (GET_CODE (x1))
	{
	case MEM:
	  goto L438;
	}
      break;
    case CC_NOOVmode:
      switch (GET_CODE (x1))
	{
	case REG:
	  if (XINT (x1, 0) == 0 && 1)
	    goto L576;
	}
    }
  L1151:
  if (GET_CODE (x1) == PC && 1)
    goto L1152;
  L1214:
  ro[0] = x1;
  goto L1215;
  L1241:
  if (GET_CODE (x1) == PC && 1)
    goto L1242;
  if (register_operand (x1, SImode))
    {
      ro[0] = x1;
      goto L1256;
    }
  goto ret0;

  L2:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == CCmode && GET_CODE (x1) == COMPARE && 1)
    goto L455;
  x1 = XEXP (x0, 0);
  goto L1214;

  L455:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SImode:
      switch (GET_CODE (x2))
	{
	case ZERO_EXTEND:
	  goto L456;
	case ZERO_EXTRACT:
	  goto L501;
	case SUBREG:
	case REG:
	  if (register_operand (x2, SImode))
	    {
	      ro[0] = x2;
	      goto L4;
	    }
	}
    L837:
      if (cc_arithop (x2, SImode))
	{
	  ro[2] = x2;
	  goto L838;
	}
    L878:
      if (cc_arithopn (x2, SImode))
	{
	  ro[2] = x2;
	  goto L879;
	}
      break;
    case QImode:
      switch (GET_CODE (x2))
	{
	case SUBREG:
	  if (XINT (x2, 1) == 0 && 1)
	    goto L473;
	}
    }
  L855:
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == NOT && 1)
    goto L856;
  x1 = XEXP (x0, 0);
  goto L1214;

  L456:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, QImode))
    {
      ro[0] = x3;
      goto L457;
    }
  goto L837;

  L457:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 101;
  x2 = XEXP (x1, 0);
  goto L837;

  L501:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L502;
    }
  goto L837;

  L502:
  x3 = XEXP (x2, 1);
  if (small_int (x3, SImode))
    {
      ro[1] = x3;
      goto L503;
    }
  goto L837;

  L503:
  x3 = XEXP (x2, 2);
  if (small_int (x3, SImode))
    {
      ro[2] = x3;
      goto L504;
    }
  goto L837;

  L504:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    if (INTVAL (operands[2]) > 19)
      return 111;
  x2 = XEXP (x1, 0);
  goto L837;

  L4:
  x2 = XEXP (x1, 1);
  if (arith_operand (x2, SImode))
    {
      ro[1] = x2;
      return 16;
    }
  x2 = XEXP (x1, 0);
  goto L837;

  L838:
  x3 = XEXP (x2, 0);
  if (arith_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L839;
    }
  goto L878;

  L839:
  x3 = XEXP (x2, 1);
  if (arith_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L840;
    }
  goto L878;

  L840:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 163;
  x2 = XEXP (x1, 0);
  goto L878;

  L879:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == NOT && 1)
    goto L880;
  goto L855;

  L880:
  x4 = XEXP (x3, 0);
  if (arith_operand (x4, SImode))
    {
      ro[0] = x4;
      goto L881;
    }
  goto L855;

  L881:
  x3 = XEXP (x2, 1);
  if (reg_or_0_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L882;
    }
  goto L855;

  L882:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 167;
  x2 = XEXP (x1, 0);
  goto L855;

  L473:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L474;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L474:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 103;
  x1 = XEXP (x0, 0);
  goto L1214;

  L856:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == XOR && 1)
    goto L857;
  L939:
  if (arith_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L940;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L857:
  x4 = XEXP (x3, 0);
  if (reg_or_0_operand (x4, SImode))
    {
      ro[0] = x4;
      goto L858;
    }
  goto L939;

  L858:
  x4 = XEXP (x3, 1);
  if (arith_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L859;
    }
  goto L939;

  L859:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 165;
  x2 = XEXP (x1, 0);
  x3 = XEXP (x2, 0);
  goto L939;

  L940:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 176;
  x1 = XEXP (x0, 0);
  goto L1214;

  L7:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == CCFPEmode && GET_CODE (x1) == COMPARE && 1)
    goto L8;
  x1 = XEXP (x0, 0);
  goto L1214;

  L8:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DFmode:
      if (register_operand (x2, DFmode))
	{
	  ro[0] = x2;
	  goto L9;
	}
      break;
    case SFmode:
      if (register_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L14;
	}
      break;
    case TFmode:
      if (register_operand (x2, TFmode))
	{
	  ro[0] = x2;
	  goto L19;
	}
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L9:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 17;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L14:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 18;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L19:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, TFmode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 19;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L22:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == CCFPmode && GET_CODE (x1) == COMPARE && 1)
    goto L23;
  x1 = XEXP (x0, 0);
  goto L1214;

  L23:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DFmode:
      if (register_operand (x2, DFmode))
	{
	  ro[0] = x2;
	  goto L24;
	}
      break;
    case SFmode:
      if (register_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L29;
	}
      break;
    case TFmode:
      if (register_operand (x2, TFmode))
	{
	  ro[0] = x2;
	  goto L34;
	}
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L24:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 20;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L29:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 21;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L34:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, TFmode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 22;
      }
  x1 = XEXP (x0, 0);
  goto L1214;
 L45:
  tem = recog_1 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L276;

  L259:
  x1 = XEXP (x0, 1);
  if (GET_CODE (x1) == IF_THEN_ELSE && 1)
    goto L260;
  x1 = XEXP (x0, 0);
  goto L276;

  L260:
  x2 = XEXP (x1, 0);
  if (noov_compare_op (x2, VOIDmode))
    {
      ro[0] = x2;
      goto L261;
    }
  x1 = XEXP (x0, 0);
  goto L276;

  L261:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == REG && XINT (x3, 0) == 0 && 1)
    goto L262;
  x1 = XEXP (x0, 0);
  goto L276;

  L262:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L263;
  x1 = XEXP (x0, 0);
  goto L276;

  L263:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L264;
    case PC:
      goto L273;
    }
  x1 = XEXP (x0, 0);
  goto L276;

  L264:
  x3 = XEXP (x2, 0);
  ro[1] = x3;
  goto L265;

  L265:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 55;
  x1 = XEXP (x0, 0);
  goto L276;

  L273:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L274;
  x1 = XEXP (x0, 0);
  goto L276;

  L274:
  x3 = XEXP (x2, 0);
  ro[1] = x3;
  return 56;

  L277:
  x1 = XEXP (x0, 1);
  if (move_operand (x1, SImode))
    {
      ro[1] = x1;
      if (register_operand (operands[0], SImode)
   || register_operand (operands[1], SImode)
   || operands[1] == const0_rtx)
	return 59;
      }
  x1 = XEXP (x0, 0);
  goto L290;
 L291:
  tem = recog_2 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L907;
 L287:
  tem = recog_3 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L416;

  L417:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, DImode))
    {
      ro[1] = x1;
      if (register_operand (operands[0], DImode)
   || register_operand (operands[1], DImode)
   || operands[1] == const0_rtx)
	return 89;
      }
  x1 = XEXP (x0, 0);
  goto L1151;

  L300:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == HImode && GET_CODE (x1) == HIGH && 1)
    goto L301;
  x1 = XEXP (x0, 0);
  goto L330;

  L301:
  x2 = XEXP (x1, 0);
  ro[1] = x2;
  if (check_pic (1))
    return 64;
  x1 = XEXP (x0, 0);
  goto L330;

  L331:
  x1 = XEXP (x0, 1);
  if (move_operand (x1, HImode))
    {
      ro[1] = x1;
      if (register_operand (operands[0], HImode)
   || register_operand (operands[1], HImode)
   || operands[1] == const0_rtx)
	return 70;
      }
  x1 = XEXP (x0, 0);
  goto L333;

  L334:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != HImode)
    {
      x1 = XEXP (x0, 0);
      goto L1214;
    }
  switch (GET_CODE (x1))
    {
    case LO_SUM:
      goto L335;
    case ZERO_EXTEND:
      goto L447;
    case SIGN_EXTEND:
      goto L492;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L335:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L336;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L336:
  x2 = XEXP (x1, 1);
  if (immediate_operand (x2, VOIDmode))
    {
      ro[2] = x2;
      return 71;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L447:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == QImode && sparc_operand (x2, QImode))
    {
      ro[1] = x2;
      if (GET_CODE (operands[1]) != CONST_INT)
	return 98;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L492:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, QImode))
    {
      ro[1] = x2;
      return 108;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L327:
  x2 = XEXP (x1, 0);
  if (symbolic_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L328;
    }
  goto L907;

  L328:
  x1 = XEXP (x0, 1);
  if (pnum_clobbers != 0 && reg_or_0_operand (x1, SImode))
    {
      ro[1] = x1;
      *pnum_clobbers = 1;
      return 68;
    }
  x1 = XEXP (x0, 0);
  goto L907;

  L908:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == SImode && GET_CODE (x1) == NEG && 1)
    goto L909;
  x1 = XEXP (x0, 0);
  goto L1214;

  L909:
  x2 = XEXP (x1, 0);
  if (arith_operand (x2, SImode))
    {
      ro[1] = x2;
      return 170;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L346:
  x2 = XEXP (x1, 0);
  if (symbolic_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L347;
    }
  goto L1214;

  L347:
  x1 = XEXP (x0, 1);
  if (pnum_clobbers != 0 && reg_or_0_operand (x1, HImode))
    {
      ro[1] = x1;
      *pnum_clobbers = 1;
      return 72;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L350:
  x1 = XEXP (x0, 1);
  if (move_operand (x1, QImode))
    {
      ro[1] = x1;
      if (register_operand (operands[0], QImode)
   || register_operand (operands[1], QImode)
   || operands[1] == const0_rtx)
	return 74;
      }
  x1 = XEXP (x0, 0);
  goto L352;

  L353:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == QImode && GET_CODE (x1) == SUBREG && XINT (x1, 1) == 0 && 1)
    goto L354;
  x1 = XEXP (x0, 0);
  goto L1214;

  L354:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == LO_SUM && 1)
    goto L355;
  x1 = XEXP (x0, 0);
  goto L1214;

  L355:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, QImode))
    {
      ro[1] = x3;
      goto L356;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L356:
  x3 = XEXP (x2, 1);
  if (immediate_operand (x3, VOIDmode))
    {
      ro[2] = x3;
      return 75;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L366:
  x2 = XEXP (x1, 0);
  if (symbolic_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L367;
    }
  goto L1214;

  L367:
  x1 = XEXP (x0, 1);
  if (pnum_clobbers != 0 && reg_or_0_operand (x1, QImode))
    {
      ro[1] = x1;
      *pnum_clobbers = 1;
      return 76;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L370:
  x1 = XEXP (x0, 1);
  ro[1] = x1;
  if (TARGET_FPU && GET_CODE (operands[1]) == CONST_DOUBLE)
    return 77;
  x1 = XEXP (x0, 0);
  goto L372;

  L373:
  x1 = XEXP (x0, 1);
  if (reg_or_nonsymb_mem_operand (x1, TFmode))
    goto L377;
  x1 = XEXP (x0, 0);
  goto L510;

  L377:
  ro[1] = x1;
  if (TARGET_FPU
   && (register_operand (operands[0], TFmode)
       || register_operand (operands[1], TFmode)))
    return 79;
  L378:
  ro[1] = x1;
  if (! TARGET_FPU
   && (register_operand (operands[0], TFmode)
       || register_operand (operands[1], TFmode)))
    return 80;
  x1 = XEXP (x0, 0);
  goto L510;

  L511:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != TFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1214;
    }
  switch (GET_CODE (x1))
    {
    case FLOAT_EXTEND:
      goto L512;
    case FLOAT:
      goto L540;
    case PLUS:
      goto L955;
    case MINUS:
      goto L970;
    case MULT:
      goto L1007;
    case DIV:
      goto L1014;
    case NEG:
      goto L1029;
    case ABS:
      goto L1041;
    case SQRT:
      goto L1053;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L512:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SFmode:
      if (register_operand (x2, SFmode))
	{
	  ro[1] = x2;
	  if (TARGET_FPU)
	    return 113;
	  }
      break;
    case DFmode:
      if (register_operand (x2, DFmode))
	{
	  ro[1] = x2;
	  if (TARGET_FPU)
	    return 114;
	  }
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L540:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 120;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L955:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, TFmode))
    {
      ro[1] = x2;
      goto L956;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L956:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, TFmode))
    {
      ro[2] = x2;
      if (TARGET_FPU)
	return 178;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L970:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, TFmode))
    {
      ro[1] = x2;
      goto L971;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L971:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, TFmode))
    {
      ro[2] = x2;
      if (TARGET_FPU)
	return 181;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1007:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) != TFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1214;
    }
  if (GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L1008;
  if (register_operand (x2, TFmode))
    {
      ro[1] = x2;
      goto L986;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1008:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L1009;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1009:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == TFmode && GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L1010;
  x1 = XEXP (x0, 0);
  goto L1214;

  L1010:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DFmode))
    {
      ro[2] = x3;
      if (TARGET_V8 && TARGET_FPU)
	return 188;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L986:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, TFmode))
    {
      ro[2] = x2;
      if (TARGET_FPU)
	return 184;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1014:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, TFmode))
    {
      ro[1] = x2;
      goto L1015;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1015:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, TFmode))
    {
      ro[2] = x2;
      if (TARGET_FPU)
	return 189;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1029:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, TFmode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 192;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1041:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, TFmode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 195;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1053:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, TFmode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 198;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L388:
  x2 = XEXP (x1, 0);
  if (symbolic_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L389;
    }
  goto L1214;

  L389:
  x1 = XEXP (x0, 1);
  if (pnum_clobbers != 0 && reg_or_0_operand (x1, TFmode))
    {
      ro[1] = x1;
      *pnum_clobbers = 1;
      return 81;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L392:
  x1 = XEXP (x0, 1);
  ro[1] = x1;
  if (TARGET_FPU && GET_CODE (operands[1]) == CONST_DOUBLE)
    return 82;
  x1 = XEXP (x0, 0);
  goto L394;

  L395:
  x1 = XEXP (x0, 1);
  if (reg_or_nonsymb_mem_operand (x1, DFmode))
    goto L399;
  x1 = XEXP (x0, 0);
  goto L506;

  L399:
  ro[1] = x1;
  if (TARGET_FPU
   && (register_operand (operands[0], DFmode)
       || register_operand (operands[1], DFmode)))
    return 84;
  L400:
  ro[1] = x1;
  if (! TARGET_FPU
   && (register_operand (operands[0], DFmode)
       || register_operand (operands[1], DFmode)))
    return 85;
  x1 = XEXP (x0, 0);
  goto L506;

  L507:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != DFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1214;
    }
  switch (GET_CODE (x1))
    {
    case FLOAT_EXTEND:
      goto L508;
    case FLOAT_TRUNCATE:
      goto L528;
    case FLOAT:
      goto L536;
    case PLUS:
      goto L960;
    case MINUS:
      goto L975;
    case MULT:
      goto L1000;
    case DIV:
      goto L1019;
    case NEG:
      goto L1033;
    case ABS:
      goto L1045;
    case SQRT:
      goto L1057;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L508:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 112;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L528:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, TFmode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 117;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L536:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 119;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L960:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L961;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L961:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, DFmode))
    {
      ro[2] = x2;
      if (TARGET_FPU)
	return 179;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L975:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L976;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L976:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, DFmode))
    {
      ro[2] = x2;
      if (TARGET_FPU)
	return 182;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1000:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) != DFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1214;
    }
  if (GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L1001;
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L991;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1001:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L1002;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1002:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == DFmode && GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L1003;
  x1 = XEXP (x0, 0);
  goto L1214;

  L1003:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SFmode))
    {
      ro[2] = x3;
      if (TARGET_V8 && TARGET_FPU)
	return 187;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L991:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, DFmode))
    {
      ro[2] = x2;
      if (TARGET_FPU)
	return 185;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1019:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L1020;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1020:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, DFmode))
    {
      ro[2] = x2;
      if (TARGET_FPU)
	return 190;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1033:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 193;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1045:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 196;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1057:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 199;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L413:
  x2 = XEXP (x1, 0);
  if (symbolic_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L414;
    }
  goto L1214;

  L414:
  x1 = XEXP (x0, 1);
  if (pnum_clobbers != 0 && reg_or_0_operand (x1, DFmode))
    {
      ro[1] = x1;
      *pnum_clobbers = 1;
      return 87;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L420:
  x1 = XEXP (x0, 1);
  ro[1] = x1;
  if (TARGET_FPU && GET_CODE (operands[1]) == CONST_DOUBLE)
    return 90;
  x1 = XEXP (x0, 0);
  goto L422;

  L423:
  x1 = XEXP (x0, 1);
  if (reg_or_nonsymb_mem_operand (x1, SFmode))
    goto L427;
  x1 = XEXP (x0, 0);
  goto L518;

  L427:
  ro[1] = x1;
  if (TARGET_FPU
   && (register_operand (operands[0], SFmode)
       || register_operand (operands[1], SFmode)))
    return 92;
  L428:
  ro[1] = x1;
  if (! TARGET_FPU
   && (register_operand (operands[0], SFmode)
       || register_operand (operands[1], SFmode)))
    return 93;
  x1 = XEXP (x0, 0);
  goto L518;

  L519:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != SFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1214;
    }
  switch (GET_CODE (x1))
    {
    case FLOAT_TRUNCATE:
      goto L520;
    case FLOAT:
      goto L532;
    case PLUS:
      goto L965;
    case MINUS:
      goto L980;
    case MULT:
      goto L995;
    case DIV:
      goto L1024;
    case NEG:
      goto L1037;
    case ABS:
      goto L1049;
    case SQRT:
      goto L1061;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L520:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DFmode:
      if (register_operand (x2, DFmode))
	{
	  ro[1] = x2;
	  if (TARGET_FPU)
	    return 115;
	  }
      break;
    case TFmode:
      if (register_operand (x2, TFmode))
	{
	  ro[1] = x2;
	  if (TARGET_FPU)
	    return 116;
	  }
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L532:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 118;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L965:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L966;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L966:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, SFmode))
    {
      ro[2] = x2;
      if (TARGET_FPU)
	return 180;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L980:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L981;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L981:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, SFmode))
    {
      ro[2] = x2;
      if (TARGET_FPU)
	return 183;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L995:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L996;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L996:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, SFmode))
    {
      ro[2] = x2;
      if (TARGET_FPU)
	return 186;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1024:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L1025;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1025:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, SFmode))
    {
      ro[2] = x2;
      if (TARGET_FPU)
	return 191;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1037:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 194;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1049:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 197;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L1061:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_FPU)
	return 200;
      }
  x1 = XEXP (x0, 0);
  goto L1214;

  L438:
  x2 = XEXP (x1, 0);
  if (symbolic_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L439;
    }
  goto L1214;

  L439:
  x1 = XEXP (x0, 1);
  if (pnum_clobbers != 0 && reg_or_0_operand (x1, SFmode))
    {
      ro[1] = x1;
      *pnum_clobbers = 1;
      return 94;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L576:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == CC_NOOVmode && GET_CODE (x1) == COMPARE && 1)
    goto L577;
  x1 = XEXP (x0, 0);
  goto L1214;

  L577:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) != SImode)
    {
      x1 = XEXP (x0, 0);
      goto L1214;
    }
  switch (GET_CODE (x2))
    {
    case PLUS:
      goto L578;
    case MINUS:
      goto L616;
    case NEG:
      goto L914;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L578:
  x3 = XEXP (x2, 0);
  if (arith_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L579;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L579:
  x3 = XEXP (x2, 1);
  if (arith_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L580;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L580:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 126;
  x1 = XEXP (x0, 0);
  goto L1214;

  L616:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L617;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L617:
  x3 = XEXP (x2, 1);
  if (arith_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L618;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L618:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 130;
  x1 = XEXP (x0, 0);
  goto L1214;

  L914:
  x3 = XEXP (x2, 0);
  if (arith_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L915;
    }
  x1 = XEXP (x0, 0);
  goto L1214;

  L915:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 171;
  x1 = XEXP (x0, 0);
  goto L1214;

  L1152:
  x1 = XEXP (x0, 1);
  if (GET_CODE (x1) == LABEL_REF && 1)
    goto L1153;
  x1 = XEXP (x0, 0);
  goto L1214;

  L1153:
  x2 = XEXP (x1, 0);
  ro[0] = x2;
  return 212;

  L1215:
  x1 = XEXP (x0, 1);
  if (GET_CODE (x1) == CALL && 1)
    goto L1216;
  x1 = XEXP (x0, 0);
  goto L1241;

  L1216:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MEM && 1)
    goto L1217;
  x1 = XEXP (x0, 0);
  goto L1241;

  L1217:
  x3 = XEXP (x2, 0);
  if (call_operand_address (x3, SImode))
    {
      ro[1] = x3;
      goto L1218;
    }
  x1 = XEXP (x0, 0);
  goto L1241;

  L1218:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && 1)
    {
      ro[2] = x2;
      *pnum_clobbers = 1;
      return 221;
    }
  x1 = XEXP (x0, 0);
  goto L1241;

  L1242:
  x1 = XEXP (x0, 1);
  if (address_operand (x1, SImode))
    {
      ro[0] = x1;
      return 228;
    }
  goto ret0;

  L1256:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == SImode && GET_CODE (x1) == FFS && 1)
    goto L1257;
  goto ret0;

  L1257:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && register_operand (x2, SImode))
    {
      ro[1] = x2;
      if (TARGET_SPARCLITE)
	{
	  *pnum_clobbers = 1;
	  return 232;
	}
      }
  goto ret0;
 ret0: return -1;
}

int
recog_5 (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5;
  int tem;

  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  switch (GET_MODE (x2))
    {
    case SImode:
      switch (GET_CODE (x2))
	{
	case NE:
	  goto L39;
	case NEG:
	  goto L52;
	case EQ:
	  goto L67;
	case PLUS:
	  goto L95;
	case MINUS:
	  goto L112;
	case MULT:
	  goto L641;
	case DIV:
	  goto L680;
	case UDIV:
	  goto L726;
	}
    }
  L281:
  if (move_pic_label (x2, SImode))
    {
      ro[1] = x2;
      goto L282;
    }
  goto ret0;

  L39:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L40;
    }
  goto L281;

  L40:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L41;
  goto L281;

  L41:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L42;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L42:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return 23;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L52:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) != SImode)
    {
      goto L281;
    }
  switch (GET_CODE (x3))
    {
    case NE:
      goto L53;
    case EQ:
      goto L81;
    }
  goto L281;

  L53:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L54;
    }
  goto L281;

  L54:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L55;
  goto L281;

  L55:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L56;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L56:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return 24;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L81:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L82;
    }
  goto L281;

  L82:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L83;
  goto L281;

  L83:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L84;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L84:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return 26;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L67:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L68;
    }
  goto L281;

  L68:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L69;
  goto L281;

  L69:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L70;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L70:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return 25;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L95:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) != SImode)
    {
      goto L281;
    }
  switch (GET_CODE (x3))
    {
    case NE:
      goto L96;
    case EQ:
      goto L130;
    }
  goto L281;

  L96:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L97;
    }
  goto L281;

  L97:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L98;
  goto L281;

  L98:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L99;
    }
  goto L281;

  L99:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L100;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L100:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return 27;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L130:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L131;
    }
  goto L281;

  L131:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L132;
  goto L281;

  L132:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L133;
    }
  goto L281;

  L133:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L134;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L134:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return 29;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L112:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L113;
    }
  goto L281;

  L113:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) != SImode)
    {
      goto L281;
    }
  switch (GET_CODE (x3))
    {
    case NE:
      goto L114;
    case EQ:
      goto L148;
    }
  goto L281;

  L114:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L115;
    }
  goto L281;

  L115:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L116;
  goto L281;

  L116:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L117;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L117:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return 28;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L148:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L149;
    }
  goto L281;

  L149:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L150;
  goto L281;

  L150:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L151;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L151:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return 30;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L641:
  x3 = XEXP (x2, 0);
  if (arith_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L642;
    }
  goto L281;

  L642:
  x3 = XEXP (x2, 1);
  if (arith_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L643;
    }
  goto L281;

  L643:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L644;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L644:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CC_NOOVmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    goto L645;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L645:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == CC_NOOVmode && GET_CODE (x2) == COMPARE && 1)
    goto L646;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L646:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == MULT && 1)
    goto L647;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L647:
  x4 = XEXP (x3, 0);
  if (rtx_equal_p (x4, ro[1]) && 1)
    goto L648;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L648:
  x4 = XEXP (x3, 1);
  if (rtx_equal_p (x4, ro[2]) && 1)
    goto L649;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L649:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    if (TARGET_V8 || TARGET_SPARCLITE)
      return 133;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L680:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L681;
    }
  goto L281;

  L681:
  x3 = XEXP (x2, 1);
  if (arith_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L682;
    }
  goto L281;

  L682:
  x1 = XVECEXP (x0, 0, 1);
  switch (GET_CODE (x1))
    {
    case CLOBBER:
      goto L683;
    case SET:
      goto L711;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L683:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_V8)
	return 140;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L711:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    goto L712;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L712:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == COMPARE && 1)
    goto L713;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L713:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == DIV && 1)
    goto L714;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L714:
  x4 = XEXP (x3, 0);
  if (rtx_equal_p (x4, ro[1]) && 1)
    goto L715;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L715:
  x4 = XEXP (x3, 1);
  if (rtx_equal_p (x4, ro[2]) && 1)
    goto L716;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L716:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && pnum_clobbers != 0 && 1)
    if (TARGET_V8)
      {
	*pnum_clobbers = 1;
	return 141;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L726:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L727;
    }
  goto L281;

  L727:
  x3 = XEXP (x2, 1);
  if (arith_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L728;
    }
  goto L281;

  L728:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L729;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L729:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    goto L730;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L730:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == COMPARE && 1)
    goto L731;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L731:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == UDIV && 1)
    goto L732;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L732:
  x4 = XEXP (x3, 0);
  if (rtx_equal_p (x4, ro[1]) && 1)
    goto L733;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L733:
  x4 = XEXP (x3, 1);
  if (rtx_equal_p (x4, ro[2]) && 1)
    goto L734;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L734:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    if (TARGET_V8)
      return 143;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L281;

  L282:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L283;
  goto ret0;

  L283:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 15 && 1)
    goto L284;
  goto ret0;

  L284:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == PC && 1)
    return 60;
  goto ret0;
 ret0: return -1;
}

int
recog_6 (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5;
  int tem;

  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SImode:
      if (GET_CODE (x2) == MEM && 1)
	goto L321;
      if (register_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L38;
	}
      break;
    case HImode:
      switch (GET_CODE (x2))
	{
	case MEM:
	  goto L340;
	}
      break;
    case QImode:
      switch (GET_CODE (x2))
	{
	case MEM:
	  goto L360;
	}
      break;
    case TFmode:
      switch (GET_CODE (x2))
	{
	case MEM:
	  goto L382;
	}
      break;
    case DFmode:
      switch (GET_CODE (x2))
	{
	case MEM:
	  goto L407;
	}
      break;
    case SFmode:
      switch (GET_CODE (x2))
	{
	case MEM:
	  goto L432;
	}
      break;
    case CCmode:
      if (GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
	goto L461;
      break;
    case DImode:
      if (register_operand (x2, DImode))
	{
	  ro[0] = x2;
	  goto L559;
	}
      break;
    case CC_NOOVmode:
      switch (GET_CODE (x2))
	{
	case REG:
	  if (XINT (x2, 0) == 0 && 1)
	    goto L584;
	}
    }
  if (GET_CODE (x2) == PC && 1)
    goto L1166;
  L1206:
  ro[0] = x2;
  goto L1207;
  L1249:
  switch (GET_MODE (x2))
    {
    case SImode:
      if (register_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L1250;
	}
    L1375:
      if (restore_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L1376;
	}
      break;
    case QImode:
      if (restore_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L1366;
	}
      break;
    case HImode:
      if (restore_operand (x2, HImode))
	{
	  ro[0] = x2;
	  goto L1371;
	}
      break;
    case SFmode:
      if (restore_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L1381;
	}
    L1392:
      if (GET_CODE (x2) == REG && XINT (x2, 0) == 32 && 1)
	goto L1393;
    }
  goto ret0;

  L321:
  x3 = XEXP (x2, 0);
  if (symbolic_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L322;
    }
  goto L1206;

  L322:
  x2 = XEXP (x1, 1);
  if (reg_or_0_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L323;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L323:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L324;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L324:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      return 68;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;
 L38:
  tem = recog_5 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x2 = XEXP (x1, 0);
  goto L1206;

  L340:
  x3 = XEXP (x2, 0);
  if (symbolic_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L341;
    }
  goto L1206;

  L341:
  x2 = XEXP (x1, 1);
  if (reg_or_0_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L342;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L342:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L343;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L343:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      return 72;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L360:
  x3 = XEXP (x2, 0);
  if (symbolic_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L361;
    }
  goto L1206;

  L361:
  x2 = XEXP (x1, 1);
  if (reg_or_0_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L362;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L362:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L363;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L363:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      return 76;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L382:
  x3 = XEXP (x2, 0);
  if (symbolic_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L383;
    }
  goto L1206;

  L383:
  x2 = XEXP (x1, 1);
  if (reg_or_0_operand (x2, TFmode))
    {
      ro[1] = x2;
      goto L384;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L384:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L385;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L385:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      return 81;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L407:
  x3 = XEXP (x2, 0);
  if (symbolic_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L408;
    }
  goto L1206;

  L408:
  x2 = XEXP (x1, 1);
  if (reg_or_0_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L409;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L409:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L410;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L410:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      return 87;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L432:
  x3 = XEXP (x2, 0);
  if (symbolic_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L433;
    }
  goto L1206;

  L433:
  x2 = XEXP (x1, 1);
  if (reg_or_0_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L434;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L434:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L435;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L435:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      return 94;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L461:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == COMPARE && 1)
    goto L462;
  x2 = XEXP (x1, 0);
  goto L1206;

  L462:
  x3 = XEXP (x2, 0);
  switch (GET_MODE (x3))
    {
    case SImode:
      switch (GET_CODE (x3))
	{
	case ZERO_EXTEND:
	  goto L463;
	case NOT:
	  goto L865;
	}
    L845:
      if (cc_arithop (x3, SImode))
	{
	  ro[3] = x3;
	  goto L846;
	}
    L887:
      if (cc_arithopn (x3, SImode))
	{
	  ro[3] = x3;
	  goto L888;
	}
      break;
    case QImode:
      if (GET_CODE (x3) == SUBREG && XINT (x3, 1) == 0 && 1)
	goto L480;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L463:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, QImode))
    {
      ro[1] = x4;
      goto L464;
    }
  goto L845;

  L464:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L465;
  x3 = XEXP (x2, 0);
  goto L845;

  L465:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L466;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L845;

  L466:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L467;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L845;

  L467:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == ZERO_EXTEND && 1)
    goto L468;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L845;

  L468:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    return 102;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L845;

  L865:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) == SImode && GET_CODE (x4) == XOR && 1)
    goto L866;
  L946:
  if (arith_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L947;
    }
  goto L845;

  L866:
  x5 = XEXP (x4, 0);
  if (reg_or_0_operand (x5, SImode))
    {
      ro[1] = x5;
      goto L867;
    }
  goto L946;

  L867:
  x5 = XEXP (x4, 1);
  if (arith_operand (x5, SImode))
    {
      ro[2] = x5;
      goto L868;
    }
  goto L946;

  L868:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L869;
  x3 = XEXP (x2, 0);
  x4 = XEXP (x3, 0);
  goto L946;

  L869:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L870;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  x4 = XEXP (x3, 0);
  goto L946;

  L870:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L871;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  x4 = XEXP (x3, 0);
  goto L946;

  L871:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == NOT && 1)
    goto L872;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  x4 = XEXP (x3, 0);
  goto L946;

  L872:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == XOR && 1)
    goto L873;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  x4 = XEXP (x3, 0);
  goto L946;

  L873:
  x4 = XEXP (x3, 0);
  if (rtx_equal_p (x4, ro[1]) && 1)
    goto L874;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  x4 = XEXP (x3, 0);
  goto L946;

  L874:
  x4 = XEXP (x3, 1);
  if (rtx_equal_p (x4, ro[2]) && 1)
    return 166;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  x4 = XEXP (x3, 0);
  goto L946;

  L947:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L948;
  x3 = XEXP (x2, 0);
  goto L845;

  L948:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L949;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L845;

  L949:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L950;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L845;

  L950:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == NOT && 1)
    goto L951;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L845;

  L951:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    return 177;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L845;

  L846:
  x4 = XEXP (x3, 0);
  if (arith_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L847;
    }
  goto L887;

  L847:
  x4 = XEXP (x3, 1);
  if (arith_operand (x4, SImode))
    {
      ro[2] = x4;
      goto L848;
    }
  goto L887;

  L848:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L849;
  x3 = XEXP (x2, 0);
  goto L887;

  L849:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L850;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L887;

  L850:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L851;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L887;

  L851:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[3]) && 1)
    return 164;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L887;

  L888:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) == SImode && GET_CODE (x4) == NOT && 1)
    goto L889;
  x2 = XEXP (x1, 0);
  goto L1206;

  L889:
  x5 = XEXP (x4, 0);
  if (arith_operand (x5, SImode))
    {
      ro[1] = x5;
      goto L890;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L890:
  x4 = XEXP (x3, 1);
  if (reg_or_0_operand (x4, SImode))
    {
      ro[2] = x4;
      goto L891;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L891:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L892;
  x2 = XEXP (x1, 0);
  goto L1206;

  L892:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L893;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L893:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L894;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L894:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[3]) && 1)
    return 168;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L480:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L481;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L481:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L482;
  x2 = XEXP (x1, 0);
  goto L1206;

  L482:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L483;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L483:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, QImode))
    {
      ro[0] = x2;
      goto L484;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L484:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[1]) && 1)
    return 104;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L559:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != DImode)
    {
      x2 = XEXP (x1, 0);
      goto L1206;
    }
  switch (GET_CODE (x2))
    {
    case PLUS:
      goto L560;
    case MINUS:
      goto L598;
    case NEG:
      goto L899;
    case ASHIFT:
      goto L1066;
    case LSHIFT:
      goto L1084;
    case LSHIFTRT:
      goto L1124;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L560:
  x3 = XEXP (x2, 0);
  if (arith_double_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L561;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L561:
  x3 = XEXP (x2, 1);
  if (arith_double_operand (x3, DImode))
    {
      ro[2] = x3;
      goto L562;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L562:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L563;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L563:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return 124;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L598:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L599;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L599:
  x3 = XEXP (x2, 1);
  if (arith_double_operand (x3, DImode))
    {
      ro[2] = x3;
      goto L600;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L600:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L601;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L601:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return 128;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L899:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L900;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L900:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L901;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L901:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return 169;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L1066:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 1 && 1)
    goto L1067;
  x2 = XEXP (x1, 0);
  goto L1206;

  L1067:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1068;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L1068:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1069;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L1069:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return 202;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L1084:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L1085;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L1085:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && 1)
    {
      ro[2] = x3;
      goto L1086;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L1086:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1087;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L1087:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    goto L1106;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L1106:
  ro[3] = x2;
  if (INTVAL (operands[2]) < 32)
    return 205;
  L1107:
  ro[3] = x2;
  if (INTVAL (operands[2]) >= 32)
    return 206;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L1124:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L1125;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L1125:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && 1)
    {
      ro[2] = x3;
      goto L1126;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L1126:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1127;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L1127:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    goto L1146;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L1146:
  ro[3] = x2;
  if (INTVAL (operands[2]) < 32)
    return 210;
  L1147:
  ro[3] = x2;
  if (INTVAL (operands[2]) >= 32)
    return 211;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L584:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == CC_NOOVmode && GET_CODE (x2) == COMPARE && 1)
    goto L585;
  x2 = XEXP (x1, 0);
  goto L1206;

  L585:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) != SImode)
    {
      x2 = XEXP (x1, 0);
      goto L1206;
    }
  switch (GET_CODE (x3))
    {
    case PLUS:
      goto L586;
    case MINUS:
      goto L624;
    case NEG:
      goto L921;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L586:
  x4 = XEXP (x3, 0);
  if (arith_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L587;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L587:
  x4 = XEXP (x3, 1);
  if (arith_operand (x4, SImode))
    {
      ro[2] = x4;
      goto L588;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L588:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L589;
  x2 = XEXP (x1, 0);
  goto L1206;

  L589:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L590;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L590:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L591;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L591:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == PLUS && 1)
    goto L592;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L592:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    goto L593;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L593:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[2]) && 1)
    return 127;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L624:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L625;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L625:
  x4 = XEXP (x3, 1);
  if (arith_operand (x4, SImode))
    {
      ro[2] = x4;
      goto L626;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L626:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L627;
  x2 = XEXP (x1, 0);
  goto L1206;

  L627:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L628;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L628:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L629;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L629:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MINUS && 1)
    goto L630;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L630:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    goto L631;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L631:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[2]) && 1)
    return 131;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L921:
  x4 = XEXP (x3, 0);
  if (arith_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L922;
    }
  x2 = XEXP (x1, 0);
  goto L1206;

  L922:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L923;
  x2 = XEXP (x1, 0);
  goto L1206;

  L923:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L924;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L924:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L925;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L925:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == NEG && 1)
    goto L926;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L926:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    return 172;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L1166:
  x2 = XEXP (x1, 1);
  if (address_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L1167;
    }
  L1173:
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1174;
  x2 = XEXP (x1, 0);
  goto L1206;

  L1167:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L1168;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L1173;

  L1168:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1169;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L1173;

  L1169:
  x3 = XEXP (x2, 0);
  ro[1] = x3;
  return 215;

  L1174:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1175;

  L1175:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L1176;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L1176:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 15 && 1)
    goto L1177;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L1177:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1178;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L1178:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[0]) && 1)
    return 216;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1206;

  L1207:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CALL && 1)
    goto L1208;
  x2 = XEXP (x1, 0);
  goto L1249;

  L1208:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == MEM && 1)
    goto L1209;
  x2 = XEXP (x1, 0);
  goto L1249;

  L1209:
  x4 = XEXP (x3, 0);
  if (call_operand_address (x4, SImode))
    {
      ro[1] = x4;
      goto L1210;
    }
  x2 = XEXP (x1, 0);
  goto L1249;

  L1210:
  x3 = XEXP (x2, 1);
  ro[2] = x3;
  goto L1211;

  L1211:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1212;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1249;

  L1212:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 15 && 1)
    return 221;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1249;

  L1250:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == FFS && 1)
    goto L1251;
  x2 = XEXP (x1, 0);
  goto L1375;

  L1251:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1252;
    }
  x2 = XEXP (x1, 0);
  goto L1375;

  L1252:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1253;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1375;

  L1253:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      if (TARGET_SPARCLITE)
	return 232;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1375;

  L1376:
  x2 = XEXP (x1, 1);
  if (arith_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1377;
    }
  L1386:
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == PLUS && 1)
    goto L1387;
  goto ret0;

  L1377:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == RETURN && 1)
    if (! TARGET_EPILOGUE)
      return 264;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L1386;

  L1387:
  x3 = XEXP (x2, 0);
  if (arith_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1388;
    }
  goto ret0;

  L1388:
  x3 = XEXP (x2, 1);
  if (arith_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L1389;
    }
  goto ret0;

  L1389:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == RETURN && 1)
    if (! TARGET_EPILOGUE)
      return 266;
  goto ret0;

  L1366:
  x2 = XEXP (x1, 1);
  if (arith_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L1367;
    }
  goto ret0;

  L1367:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == RETURN && 1)
    if (! TARGET_EPILOGUE)
      return 262;
  goto ret0;

  L1371:
  x2 = XEXP (x1, 1);
  if (arith_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L1372;
    }
  goto ret0;

  L1372:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == RETURN && 1)
    if (! TARGET_EPILOGUE)
      return 263;
  goto ret0;

  L1381:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L1382;
    }
  x2 = XEXP (x1, 0);
  goto L1392;

  L1382:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == RETURN && 1)
    if (! TARGET_FPU && ! TARGET_EPILOGUE)
      return 265;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1392;

  L1393:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, SFmode))
    {
      ro[0] = x2;
      goto L1394;
    }
  goto ret0;

  L1394:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == RETURN && 1)
    if (! TARGET_EPILOGUE)
      return 267;
  goto ret0;
 ret0: return -1;
}

int
recog (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5;
  int tem;

  L1235:
  switch (GET_CODE (x0))
    {
    case UNSPEC:
      if (GET_MODE (x0) == SImode && XINT (x0, 1) == 0 && XVECLEN (x0, 0) == 2 && 1)
	goto L1236;
      break;
    case SET:
      goto L1;
    case PARALLEL:
      if (XVECLEN (x0, 0) == 2 && 1)
	goto L36;
      if (XVECLEN (x0, 0) == 3 && 1)
	goto L690;
      if (XVECLEN (x0, 0) == 4 && 1)
	goto L1220;
      break;
    case CALL:
      goto L1187;
    case RETURN:
      if (! TARGET_EPILOGUE)
	return 226;
      break;
    case CONST_INT:
      if (XWINT (x0, 0) == 0 && 1)
	return 227;
      break;
    case UNSPEC_VOLATILE:
      if (XINT (x0, 1) == 0 && XVECLEN (x0, 0) == 1 && 1)
	goto L1244;
      if (XINT (x0, 1) == 1 && XVECLEN (x0, 0) == 1 && 1)
	goto L1246;
    }
  goto ret0;

  L1236:
  x1 = XVECEXP (x0, 0, 0);
  if (register_operand (x1, SImode))
    {
      ro[0] = x1;
      goto L1237;
    }
  goto ret0;

  L1237:
  x1 = XVECEXP (x0, 0, 1);
  if (register_operand (x1, SImode))
    {
      ro[1] = x1;
      return 225;
    }
  goto ret0;
 L1:
  return recog_4 (x0, insn, pnum_clobbers);

  L36:
  x1 = XVECEXP (x0, 0, 0);
  switch (GET_CODE (x1))
    {
    case SET:
      goto L320;
    case CALL:
      goto L1181;
    }
  goto ret0;
 L320:
  return recog_6 (x0, insn, pnum_clobbers);

  L1181:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MEM && 1)
    goto L1182;
  goto ret0;

  L1182:
  x3 = XEXP (x2, 0);
  if (call_operand_address (x3, SImode))
    {
      ro[0] = x3;
      goto L1183;
    }
  goto ret0;

  L1183:
  x2 = XEXP (x1, 1);
  ro[1] = x2;
  goto L1184;

  L1184:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1185;
  if (pnum_clobbers != 0 && immediate_operand (x1, VOIDmode))
    {
      ro[2] = x1;
      if (GET_CODE (operands[2]) == CONST_INT && INTVAL (operands[2]) > 0)
	{
	  *pnum_clobbers = 1;
	  return 219;
	}
      }
  goto ret0;

  L1185:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 15 && 1)
    return 218;
  goto ret0;

  L690:
  x1 = XVECEXP (x0, 0, 0);
  switch (GET_CODE (x1))
    {
    case SET:
      goto L691;
    case CALL:
      goto L1192;
    }
  goto ret0;

  L691:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L692;
    }
  if (GET_CODE (x2) == PC && 1)
    goto L1157;
  goto ret0;

  L692:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == DIV && 1)
    goto L693;
  goto ret0;

  L693:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L694;
    }
  goto ret0;

  L694:
  x3 = XEXP (x2, 1);
  if (arith_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L695;
    }
  goto ret0;

  L695:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L696;
  goto ret0;

  L696:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    goto L697;
  goto ret0;

  L697:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == COMPARE && 1)
    goto L698;
  goto ret0;

  L698:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == DIV && 1)
    goto L699;
  goto ret0;

  L699:
  x4 = XEXP (x3, 0);
  if (rtx_equal_p (x4, ro[1]) && 1)
    goto L700;
  goto ret0;

  L700:
  x4 = XEXP (x3, 1);
  if (rtx_equal_p (x4, ro[2]) && 1)
    goto L701;
  goto ret0;

  L701:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L702;
  goto ret0;

  L702:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L703;
  goto ret0;

  L703:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_V8)
	return 141;
      }
  goto ret0;

  L1157:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L1158;
    }
  goto ret0;

  L1158:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L1159;
  goto ret0;

  L1159:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1160;
  goto ret0;

  L1160:
  x3 = XEXP (x2, 0);
  ro[1] = x3;
  goto L1161;

  L1161:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == USE && 1)
    goto L1162;
  goto ret0;

  L1162:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 15 && 1)
    return 214;
  goto ret0;

  L1192:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MEM && 1)
    goto L1193;
  goto ret0;

  L1193:
  x3 = XEXP (x2, 0);
  if (call_operand_address (x3, SImode))
    {
      ro[0] = x3;
      goto L1194;
    }
  goto ret0;

  L1194:
  x2 = XEXP (x1, 1);
  ro[1] = x2;
  goto L1195;
  L1232:
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    goto L1233;
  goto ret0;

  L1195:
  x1 = XVECEXP (x0, 0, 1);
  if (immediate_operand (x1, VOIDmode))
    {
      ro[2] = x1;
      goto L1196;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L1232;

  L1196:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1197;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L1232;

  L1197:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 15 && 1)
    if (GET_CODE (operands[2]) == CONST_INT && INTVAL (operands[2]) > 0)
      return 219;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L1232;

  L1233:
  x1 = XVECEXP (x0, 0, 1);
  if (memory_operand (x1, DImode))
    {
      ro[1] = x1;
      goto L1234;
    }
  goto ret0;

  L1234:
  x1 = XVECEXP (x0, 0, 2);
  if (pnum_clobbers != 0 && 1)
    {
      ro[2] = x1;
      *pnum_clobbers = 1;
      return 223;
    }
  goto ret0;

  L1220:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == CALL && 1)
    goto L1221;
  goto ret0;

  L1221:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MEM && 1)
    goto L1222;
  goto ret0;

  L1222:
  x3 = XEXP (x2, 0);
  if (call_operand_address (x3, SImode))
    {
      ro[0] = x3;
      goto L1223;
    }
  goto ret0;

  L1223:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    goto L1224;
  goto ret0;

  L1224:
  x1 = XVECEXP (x0, 0, 1);
  if (memory_operand (x1, DImode))
    {
      ro[1] = x1;
      goto L1225;
    }
  goto ret0;

  L1225:
  x1 = XVECEXP (x0, 0, 2);
  ro[2] = x1;
  goto L1226;

  L1226:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1227;
  goto ret0;

  L1227:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 15 && 1)
    return 223;
  goto ret0;

  L1187:
  x1 = XEXP (x0, 0);
  if (GET_MODE (x1) == SImode && GET_CODE (x1) == MEM && 1)
    goto L1188;
  goto ret0;

  L1188:
  x2 = XEXP (x1, 0);
  if (call_operand_address (x2, SImode))
    {
      ro[0] = x2;
      goto L1189;
    }
  goto ret0;

  L1189:
  x1 = XEXP (x0, 1);
  if (pnum_clobbers != 0 && 1)
    {
      ro[1] = x1;
      *pnum_clobbers = 1;
      return 218;
    }
  goto ret0;

  L1244:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == CONST_INT && XWINT (x1, 0) == 0 && 1)
    return 230;
  goto ret0;

  L1246:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == CONST_INT && XWINT (x1, 0) == 0 && 1)
    return 231;
  goto ret0;
 ret0: return -1;
}

rtx
split_1 (x0, insn)
     register rtx x0;
     rtx insn;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5;
  rtx tem;

  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != SImode)
    goto ret0;
  switch (GET_CODE (x2))
    {
    case AND:
      goto L749;
    case IOR:
      goto L779;
    case XOR:
      goto L809;
    case NOT:
      goto L817;
    case NE:
      goto L1293;
    case NEG:
      goto L1301;
    case EQ:
      goto L1310;
    case PLUS:
      goto L1327;
    case MINUS:
      goto L1337;
    }
  goto ret0;

  L749:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L750;
    }
  goto ret0;

  L750:
  x3 = XEXP (x2, 1);
  ro[2] = x3;
  goto L751;

  L751:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L752;
  goto ret0;

  L752:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[3] = x2;
      if (GET_CODE (operands[2]) == CONST_INT
   && !SMALL_INT (operands[2])
   && (INTVAL (operands[2]) & 0x3ff) == 0x3ff)
	return gen_split_147 (operands);
      }
  goto ret0;

  L779:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L780;
    }
  goto ret0;

  L780:
  x3 = XEXP (x2, 1);
  ro[2] = x3;
  goto L781;

  L781:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L782;
  goto ret0;

  L782:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[3] = x2;
      if (GET_CODE (operands[2]) == CONST_INT
   && !SMALL_INT (operands[2])
   && (INTVAL (operands[2]) & 0x3ff) == 0x3ff)
	return gen_split_153 (operands);
      }
  goto ret0;

  L809:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L810;
    }
  goto ret0;

  L810:
  x3 = XEXP (x2, 1);
  ro[2] = x3;
  goto L811;

  L811:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L812;
  goto ret0;

  L812:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[3] = x2;
      if (GET_CODE (operands[2]) == CONST_INT
   && !SMALL_INT (operands[2])
   && (INTVAL (operands[2]) & 0x3ff) == 0x3ff)
	return gen_split_159 (operands);
      }
  goto ret0;

  L817:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == XOR && 1)
    goto L818;
  goto ret0;

  L818:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L819;
    }
  goto ret0;

  L819:
  x4 = XEXP (x3, 1);
  ro[2] = x4;
  goto L820;

  L820:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L821;
  goto ret0;

  L821:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[3] = x2;
      if (GET_CODE (operands[2]) == CONST_INT
   && !SMALL_INT (operands[2])
   && (INTVAL (operands[2]) & 0x3ff) == 0x3ff)
	return gen_split_160 (operands);
      }
  goto ret0;

  L1293:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1294;
    }
  goto ret0;

  L1294:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1295;
  goto ret0;

  L1295:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1296;
  goto ret0;

  L1296:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return gen_split_239 (operands);
  goto ret0;

  L1301:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) != SImode)
    goto ret0;
  switch (GET_CODE (x3))
    {
    case NE:
      goto L1302;
    case EQ:
      goto L1319;
    }
  goto ret0;

  L1302:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1303;
    }
  goto ret0;

  L1303:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L1304;
  goto ret0;

  L1304:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1305;
  goto ret0;

  L1305:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return gen_split_240 (operands);
  goto ret0;

  L1319:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1320;
    }
  goto ret0;

  L1320:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L1321;
  goto ret0;

  L1321:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1322;
  goto ret0;

  L1322:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return gen_split_242 (operands);
  goto ret0;

  L1310:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1311;
    }
  goto ret0;

  L1311:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1312;
  goto ret0;

  L1312:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1313;
  goto ret0;

  L1313:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return gen_split_241 (operands);
  goto ret0;

  L1327:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) != SImode)
    goto ret0;
  switch (GET_CODE (x3))
    {
    case NE:
      goto L1328;
    case EQ:
      goto L1348;
    }
  goto ret0;

  L1328:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1329;
    }
  goto ret0;

  L1329:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L1330;
  goto ret0;

  L1330:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L1331;
    }
  goto ret0;

  L1331:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1332;
  goto ret0;

  L1332:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return gen_split_243 (operands);
  goto ret0;

  L1348:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1349;
    }
  goto ret0;

  L1349:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L1350;
  goto ret0;

  L1350:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L1351;
    }
  goto ret0;

  L1351:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1352;
  goto ret0;

  L1352:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return gen_split_245 (operands);
  goto ret0;

  L1337:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L1338;
    }
  goto ret0;

  L1338:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) != SImode)
    goto ret0;
  switch (GET_CODE (x3))
    {
    case NE:
      goto L1339;
    case EQ:
      goto L1359;
    }
  goto ret0;

  L1339:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1340;
    }
  goto ret0;

  L1340:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L1341;
  goto ret0;

  L1341:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1342;
  goto ret0;

  L1342:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return gen_split_244 (operands);
  goto ret0;

  L1359:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1360;
    }
  goto ret0;

  L1360:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L1361;
  goto ret0;

  L1361:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1362;
  goto ret0;

  L1362:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == CCmode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    return gen_split_246 (operands);
  goto ret0;
 ret0: return 0;
}

rtx
split_insns (x0, insn)
     register rtx x0;
     rtx insn;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5;
  rtx tem;

  L401:
  switch (GET_CODE (x0))
    {
    case SET:
      goto L402;
    case PARALLEL:
      if (XVECLEN (x0, 0) == 2 && 1)
	goto L746;
    }
  goto ret0;

  L402:
  x1 = XEXP (x0, 0);
  if (register_operand (x1, DFmode))
    {
      ro[0] = x1;
      goto L403;
    }
  L1273:
  if (register_operand (x1, VOIDmode))
    {
      ro[0] = x1;
      goto L1274;
    }
  L1282:
  if (register_operand (x1, SImode))
    {
      ro[0] = x1;
      goto L1283;
    }
  goto ret0;

  L403:
  x1 = XEXP (x0, 1);
  if (register_operand (x1, DFmode))
    {
      ro[1] = x1;
      if (reload_completed)
	return gen_split_86 (operands);
      }
  x1 = XEXP (x0, 0);
  goto L1273;

  L1274:
  x1 = XEXP (x0, 1);
  if (memop (x1, VOIDmode))
    {
      ro[1] = x1;
      goto L1275;
    }
  L1278:
  if (extend_op (x1, VOIDmode))
    {
      ro[1] = x1;
      goto L1279;
    }
  x1 = XEXP (x0, 0);
  goto L1282;

  L1275:
  x2 = XEXP (x1, 0);
  if (immediate_operand (x2, SImode))
    {
      ro[2] = x2;
      if (flag_pic)
	return gen_split_235 (operands);
      }
  goto L1278;

  L1279:
  x2 = XEXP (x1, 0);
  if (memop (x2, VOIDmode))
    {
      ro[2] = x2;
      goto L1280;
    }
  x1 = XEXP (x0, 0);
  goto L1282;

  L1280:
  x3 = XEXP (x2, 0);
  if (immediate_operand (x3, SImode))
    {
      ro[3] = x3;
      if (flag_pic)
	return gen_split_236 (operands);
      }
  x1 = XEXP (x0, 0);
  goto L1282;

  L1283:
  x1 = XEXP (x0, 1);
  if (immediate_operand (x1, SImode))
    goto L1287;
  goto ret0;

  L1287:
  ro[1] = x1;
  if (! flag_pic && (GET_CODE (operands[1]) == SYMBOL_REF
		  || GET_CODE (operands[1]) == CONST
		  || GET_CODE (operands[1]) == LABEL_REF))
    return gen_split_237 (operands);
  L1288:
  ro[1] = x1;
  if (flag_pic && (GET_CODE (operands[1]) == SYMBOL_REF
		|| GET_CODE (operands[1]) == CONST))
    return gen_split_238 (operands);
  goto ret0;

  L746:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == SET && 1)
    goto L747;
  goto ret0;

  L747:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L748;
    }
  L1260:
  if (memop (x2, VOIDmode))
    {
      ro[0] = x2;
      goto L1261;
    }
  goto ret0;
 L748:
  tem = split_1 (x0, insn);
  if (tem != 0) return tem;
  x2 = XEXP (x1, 0);
  goto L1260;

  L1261:
  x3 = XEXP (x2, 0);
  if (symbolic_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1262;
    }
  L1268:
  if (immediate_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1269;
    }
  goto ret0;

  L1262:
  x2 = XEXP (x1, 1);
  if (reg_or_0_operand (x2, VOIDmode))
    {
      ro[2] = x2;
      goto L1263;
    }
  x2 = XEXP (x1, 0);
  x3 = XEXP (x2, 0);
  goto L1268;

  L1263:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1264;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  x3 = XEXP (x2, 0);
  goto L1268;

  L1264:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[3] = x2;
      if (! flag_pic)
	return gen_split_233 (operands);
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  x3 = XEXP (x2, 0);
  goto L1268;

  L1269:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, VOIDmode))
    {
      ro[2] = x2;
      goto L1270;
    }
  goto ret0;

  L1270:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1271;
  goto ret0;

  L1271:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[3] = x2;
      if (flag_pic)
	return gen_split_234 (operands);
      }
  goto ret0;
 ret0: return 0;
}


/* Generated automatically by the program `genrecog'
from the machine description file `md'.  */

#include "config.h"
#include "rtl.h"
#include "insn-config.h"
#include "recog.h"
#include "real.h"
#include "output.h"
#include "flags.h"


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
   add_clobbers (found in insn-emit.c) to fill in the CLOBBERs.*/

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
  register rtx x1, x2, x3, x4, x5, x6;
  int tem;

  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case HImode:
      switch (GET_CODE (x1))
	{
	case TRUNCATE:
	  goto L628;
	case ZERO_EXTEND:
	  goto L208;
	case SIGN_EXTEND:
	  goto L220;
	case FIX:
	  goto L313;
	case PLUS:
	  goto L386;
	case MINUS:
	  goto L455;
	case MULT:
	  goto L501;
	case DIV:
	  goto L623;
	case UDIV:
	  goto L641;
	case MOD:
	  goto L683;
	case UMOD:
	  goto L701;
	case AND:
	  goto L746;
	case IOR:
	  goto L785;
	case XOR:
	  goto L824;
	case NEG:
	  goto L862;
	case NOT:
	  goto L928;
	}
    }
  if (general_operand (x1, HImode))
    {
      ro[1] = x1;
      return 30;
    }
  goto ret0;

  L628:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SImode:
      switch (GET_CODE (x2))
	{
	case DIV:
	  goto L629;
	case UDIV:
	  goto L647;
	case MOD:
	  goto L689;
	case UMOD:
	  goto L707;
	}
    }
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 43;
    }
  goto ret0;

  L629:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L630;
    }
  goto ret0;

  L630:
  x3 = XEXP (x2, 1);
  switch (GET_CODE (x3))
    {
    case SIGN_EXTEND:
      if (GET_MODE (x3) == SImode && 1)
	goto L631;
      break;
    case CONST_INT:
      ro[2] = x3;
      return 129;
    }
  goto ret0;

  L631:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, HImode))
    {
      ro[2] = x4;
      return 128;
    }
  goto ret0;

  L647:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L648;
    }
  goto ret0;

  L648:
  x3 = XEXP (x2, 1);
  switch (GET_CODE (x3))
    {
    case ZERO_EXTEND:
      if (GET_MODE (x3) == SImode && 1)
	goto L649;
      break;
    case CONST_INT:
      ro[2] = x3;
      return 132;
    }
  goto ret0;

  L649:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, HImode))
    {
      ro[2] = x4;
      return 131;
    }
  goto ret0;

  L689:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L690;
    }
  goto ret0;

  L690:
  x3 = XEXP (x2, 1);
  switch (GET_CODE (x3))
    {
    case SIGN_EXTEND:
      if (GET_MODE (x3) == SImode && 1)
	goto L691;
      break;
    case CONST_INT:
      ro[2] = x3;
      return 141;
    }
  goto ret0;

  L691:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, HImode))
    {
      ro[2] = x4;
      return 140;
    }
  goto ret0;

  L707:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L708;
    }
  goto ret0;

  L708:
  x3 = XEXP (x2, 1);
  switch (GET_CODE (x3))
    {
    case ZERO_EXTEND:
      if (GET_MODE (x3) == SImode && 1)
	goto L709;
      break;
    case CONST_INT:
      ro[2] = x3;
      return 144;
    }
  goto ret0;

  L709:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, HImode))
    {
      ro[2] = x4;
      return 143;
    }
  goto ret0;

  L208:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, QImode))
    {
      ro[1] = x2;
      if (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM)
	return 48;
      }
  goto ret0;

  L220:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, QImode))
    {
      ro[1] = x2;
      return 51;
    }
  goto ret0;

  L313:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DFmode && GET_CODE (x2) == FIX && 1)
    goto L314;
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 76;
      }
  L357:
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 79;
      }
  goto ret0;

  L314:
  x3 = XEXP (x2, 0);
  if (pnum_clobbers != 0 && register_operand (x3, DFmode))
    {
      ro[1] = x3;
      if (TARGET_68040)
	{
	  *pnum_clobbers = 2;
	  return 71;
	}
      }
  goto ret0;

  L386:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L387;
    }
  goto ret0;

  L387:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 85;
    }
  goto ret0;

  L455:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L456;
    }
  goto ret0;

  L456:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 99;
    }
  goto ret0;

  L501:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L502;
    }
  goto ret0;

  L502:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 109;
    }
  goto ret0;

  L623:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L624;
    }
  goto ret0;

  L624:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 127;
    }
  goto ret0;

  L641:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L642;
    }
  goto ret0;

  L642:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 130;
    }
  goto ret0;

  L683:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L684;
    }
  goto ret0;

  L684:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 139;
    }
  goto ret0;

  L701:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L702;
    }
  goto ret0;

  L702:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 142;
    }
  goto ret0;

  L746:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L747;
    }
  goto ret0;

  L747:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 148;
    }
  goto ret0;

  L785:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L786;
    }
  goto ret0;

  L786:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 155;
    }
  goto ret0;

  L824:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L825;
    }
  goto ret0;

  L825:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 162;
    }
  goto ret0;

  L862:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 169;
    }
  goto ret0;

  L928:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 187;
    }
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
  register rtx x1, x2, x3, x4, x5, x6;
  int tem;

  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case QImode:
      switch (GET_CODE (x1))
	{
	case TRUNCATE:
	  goto L192;
	case FIX:
	  goto L328;
	case PLUS:
	  goto L403;
	case MINUS:
	  goto L466;
	case AND:
	  goto L763;
	case IOR:
	  goto L802;
	case XOR:
	  goto L841;
	case NEG:
	  goto L871;
	case NOT:
	  goto L937;
	case EQ:
	  goto L1275;
	case NE:
	  goto L1280;
	case GT:
	  goto L1285;
	case GTU:
	  goto L1290;
	case LT:
	  goto L1295;
	case LTU:
	  goto L1300;
	case GE:
	  goto L1305;
	case GEU:
	  goto L1310;
	case LE:
	  goto L1315;
	case LEU:
	  goto L1320;
	}
    }
  if (general_operand (x1, QImode))
    {
      ro[1] = x1;
      return 32;
    }
  goto ret0;

  L192:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 41;
    }
  L196:
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 42;
    }
  goto ret0;

  L328:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DFmode && GET_CODE (x2) == FIX && 1)
    goto L329;
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 75;
      }
  L353:
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 78;
      }
  goto ret0;

  L329:
  x3 = XEXP (x2, 0);
  if (pnum_clobbers != 0 && register_operand (x3, DFmode))
    {
      ro[1] = x3;
      if (TARGET_68040)
	{
	  *pnum_clobbers = 2;
	  return 72;
	}
      }
  goto ret0;

  L403:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L404;
    }
  goto ret0;

  L404:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 88;
    }
  goto ret0;

  L466:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L467;
    }
  goto ret0;

  L467:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 101;
    }
  goto ret0;

  L763:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L764;
    }
  goto ret0;

  L764:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 151;
    }
  goto ret0;

  L802:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L803;
    }
  goto ret0;

  L803:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 158;
    }
  goto ret0;

  L841:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L842;
    }
  goto ret0;

  L842:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 165;
    }
  goto ret0;

  L871:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 171;
    }
  goto ret0;

  L937:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 189;
    }
  goto ret0;

  L1275:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1276;
  goto ret0;

  L1276:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 248;
  goto ret0;

  L1280:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1281;
  goto ret0;

  L1281:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 249;
  goto ret0;

  L1285:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1286;
  goto ret0;

  L1286:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 250;
  goto ret0;

  L1290:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1291;
  goto ret0;

  L1291:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 251;
  goto ret0;

  L1295:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1296;
  goto ret0;

  L1296:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 252;
  goto ret0;

  L1300:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1301;
  goto ret0;

  L1301:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 253;
  goto ret0;

  L1305:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1306;
  goto ret0;

  L1306:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 254;
  goto ret0;

  L1310:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1311;
  goto ret0;

  L1311:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 255;
  goto ret0;

  L1315:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1316;
  goto ret0;

  L1316:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 256;
  goto ret0;

  L1320:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1321;
  goto ret0;

  L1321:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 257;
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
  register rtx x1, x2, x3, x4, x5, x6;
  int tem;

  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != SImode)
    goto ret0;
  switch (GET_CODE (x1))
    {
    case PLUS:
      goto L375;
    case MINUS:
      goto L444;
    case MULT:
      goto L506;
    case IOR:
      goto L780;
    case XOR:
      goto L819;
    case NEG:
      goto L858;
    case NOT:
      goto L924;
    case ZERO_EXTRACT:
      goto L1168;
    case SIGN_EXTRACT:
      goto L1180;
    }
  goto ret0;

  L375:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L381;
    }
  goto ret0;

  L381:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == SIGN_EXTEND && 1)
    goto L382;
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 83;
    }
  goto ret0;

  L382:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, HImode))
    {
      ro[2] = x3;
      return 84;
    }
  goto ret0;

  L444:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L450;
    }
  goto ret0;

  L450:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == SIGN_EXTEND && 1)
    goto L451;
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 97;
    }
  goto ret0;

  L451:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, HImode))
    {
      ro[2] = x3;
      return 98;
    }
  goto ret0;

  L506:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SImode:
      switch (GET_CODE (x2))
	{
	case SIGN_EXTEND:
	  goto L507;
	case ZERO_EXTEND:
	  goto L525;
	}
    }
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L520;
    }
  goto ret0;

  L507:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, HImode))
    {
      ro[1] = x3;
      goto L508;
    }
  goto ret0;

  L508:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case SIGN_EXTEND:
      if (GET_MODE (x2) == SImode && 1)
	goto L509;
      break;
    case CONST_INT:
      ro[2] = x2;
      if (INTVAL (operands[2]) >= -0x8000 && INTVAL (operands[2]) <= 0x7fff)
	return 111;
    }
  goto ret0;

  L509:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, HImode))
    {
      ro[2] = x3;
      return 110;
    }
  goto ret0;

  L525:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, HImode))
    {
      ro[1] = x3;
      goto L526;
    }
  goto ret0;

  L526:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case ZERO_EXTEND:
      if (GET_MODE (x2) == SImode && 1)
	goto L527;
      break;
    case CONST_INT:
      ro[2] = x2;
      if (INTVAL (operands[2]) >= 0 && INTVAL (operands[2]) <= 0xffff)
	return 114;
    }
  goto ret0;

  L527:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, HImode))
    {
      ro[2] = x3;
      return 113;
    }
  goto ret0;

  L520:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      if (TARGET_68020)
	return 112;
      }
  goto ret0;

  L780:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L781;
    }
  goto ret0;

  L781:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 154;
    }
  goto ret0;

  L819:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L820;
    }
  goto ret0;

  L820:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 161;
    }
  goto ret0;

  L858:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 168;
    }
  goto ret0;

  L924:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 186;
    }
  goto ret0;

  L1168:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[1] = x2;
	  goto L1169;
	}
      break;
    case SImode:
      if (nonimmediate_operand (x2, SImode))
	{
	  ro[1] = x2;
	  goto L1175;
	}
    }
  goto ret0;

  L1169:
  x2 = XEXP (x1, 1);
  if (immediate_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L1170;
    }
  L1199:
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L1200;
    }
  goto ret0;

  L1170:
  x2 = XEXP (x1, 2);
  if (immediate_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_68020 && TARGET_BITFIELD
   && GET_CODE (operands[2]) == CONST_INT
   && (INTVAL (operands[2]) == 32)
   && GET_CODE (operands[3]) == CONST_INT
   && (INTVAL (operands[3]) % 8) == 0
   && ! mode_dependent_address_p (XEXP (operands[1], 0)))
	return 231;
      }
  x2 = XEXP (x1, 1);
  goto L1199;

  L1200:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_68020 && TARGET_BITFIELD)
	return 236;
      }
  goto ret0;

  L1175:
  x2 = XEXP (x1, 1);
  if (immediate_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L1176;
    }
  L1240:
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L1241;
    }
  goto ret0;

  L1176:
  x2 = XEXP (x1, 2);
  if (immediate_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_68020 && TARGET_BITFIELD
   && GET_CODE (operands[2]) == CONST_INT
   && (INTVAL (operands[2]) == 8 || INTVAL (operands[2]) == 16)
   && GET_CODE (operands[3]) == CONST_INT
   && INTVAL (operands[3]) % INTVAL (operands[2]) == 0
   && (GET_CODE (operands[1]) == REG
       || ! mode_dependent_address_p (XEXP (operands[1], 0))))
	return 232;
      }
  x2 = XEXP (x1, 1);
  goto L1240;

  L1241:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_68020 && TARGET_BITFIELD)
	return 242;
      }
  goto ret0;

  L1180:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[1] = x2;
	  goto L1181;
	}
      break;
    case SImode:
      if (nonimmediate_operand (x2, SImode))
	{
	  ro[1] = x2;
	  goto L1187;
	}
    }
  goto ret0;

  L1181:
  x2 = XEXP (x1, 1);
  if (immediate_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L1182;
    }
  L1193:
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L1194;
    }
  goto ret0;

  L1182:
  x2 = XEXP (x1, 2);
  if (immediate_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_68020 && TARGET_BITFIELD
   && GET_CODE (operands[2]) == CONST_INT
   && (INTVAL (operands[2]) == 32)
   && GET_CODE (operands[3]) == CONST_INT
   && (INTVAL (operands[3]) % 8) == 0
   && ! mode_dependent_address_p (XEXP (operands[1], 0)))
	return 233;
      }
  x2 = XEXP (x1, 1);
  goto L1193;

  L1194:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_68020 && TARGET_BITFIELD)
	return 235;
      }
  goto ret0;

  L1187:
  x2 = XEXP (x1, 1);
  if (immediate_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L1188;
    }
  L1234:
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L1235;
    }
  goto ret0;

  L1188:
  x2 = XEXP (x1, 2);
  if (immediate_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_68020 && TARGET_BITFIELD
   && GET_CODE (operands[2]) == CONST_INT
   && (INTVAL (operands[2]) == 8 || INTVAL (operands[2]) == 16)
   && GET_CODE (operands[3]) == CONST_INT
   && INTVAL (operands[3]) % INTVAL (operands[2]) == 0
   && (GET_CODE (operands[1]) == REG
       || ! mode_dependent_address_p (XEXP (operands[1], 0))))
	return 234;
      }
  x2 = XEXP (x1, 1);
  goto L1234;

  L1235:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_68020 && TARGET_BITFIELD)
	return 241;
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
  register rtx x1, x2, x3, x4, x5, x6;
  int tem;

  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      if (general_operand (x2, HImode))
	{
	  ro[0] = x2;
	  goto L391;
	}
    L967:
      if (register_operand (x2, HImode))
	{
	  ro[0] = x2;
	  goto L968;
	}
      break;
    case QImode:
      if (general_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L408;
	}
    L978:
      if (register_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L979;
	}
    }
  goto ret0;

  L391:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case HImode:
      switch (GET_CODE (x1))
	{
	case PLUS:
	  goto L392;
	case MINUS:
	  goto L461;
	case AND:
	  goto L752;
	case IOR:
	  goto L791;
	case XOR:
	  goto L830;
	case NEG:
	  goto L867;
	case NOT:
	  goto L933;
	}
    }
  if (general_operand (x1, HImode))
    {
      ro[1] = x1;
      return 31;
    }
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L967;

  L392:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L393;
  L398:
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L399;
    }
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L967;

  L393:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 86;
    }
  x2 = XEXP (x1, 0);
  goto L398;

  L399:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 87;
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L967;

  L461:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L462;
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L967;

  L462:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 100;
    }
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L967;

  L752:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L753;
  L758:
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L759;
    }
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L967;

  L753:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 149;
    }
  x2 = XEXP (x1, 0);
  goto L758;

  L759:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 150;
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L967;

  L791:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L792;
  L797:
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L798;
    }
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L967;

  L792:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 156;
    }
  x2 = XEXP (x1, 0);
  goto L797;

  L798:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 157;
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L967;

  L830:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L831;
  L836:
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L837;
    }
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L967;

  L831:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 163;
    }
  x2 = XEXP (x1, 0);
  goto L836;

  L837:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 164;
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L967;

  L867:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 170;
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L967;

  L933:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 188;
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L967;

  L968:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != HImode)
    goto ret0;
  switch (GET_CODE (x1))
    {
    case ASHIFT:
      goto L969;
    case ASHIFTRT:
      goto L1008;
    case LSHIFT:
      goto L1047;
    case LSHIFTRT:
      goto L1086;
    case ROTATE:
      goto L1113;
    case ROTATERT:
      goto L1140;
    }
  goto ret0;

  L969:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L970;
  goto ret0;

  L970:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 195;
    }
  goto ret0;

  L1008:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1009;
  goto ret0;

  L1009:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 202;
    }
  goto ret0;

  L1047:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1048;
  goto ret0;

  L1048:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 209;
    }
  goto ret0;

  L1086:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1087;
  goto ret0;

  L1087:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 216;
    }
  goto ret0;

  L1113:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1114;
  goto ret0;

  L1114:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 221;
    }
  goto ret0;

  L1140:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1141;
  goto ret0;

  L1141:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 226;
    }
  goto ret0;

  L408:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case QImode:
      switch (GET_CODE (x1))
	{
	case PLUS:
	  goto L409;
	case MINUS:
	  goto L472;
	case AND:
	  goto L769;
	case IOR:
	  goto L808;
	case XOR:
	  goto L847;
	case NEG:
	  goto L876;
	case NOT:
	  goto L942;
	}
    }
  if (general_operand (x1, QImode))
    {
      ro[1] = x1;
      return 33;
    }
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L978;

  L409:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L410;
  L415:
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L416;
    }
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L978;

  L410:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 89;
    }
  x2 = XEXP (x1, 0);
  goto L415;

  L416:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 90;
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L978;

  L472:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L473;
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L978;

  L473:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 102;
    }
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L978;

  L769:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L770;
  L775:
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L776;
    }
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L978;

  L770:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 152;
    }
  x2 = XEXP (x1, 0);
  goto L775;

  L776:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 153;
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L978;

  L808:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L809;
  L814:
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L815;
    }
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L978;

  L809:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 159;
    }
  x2 = XEXP (x1, 0);
  goto L814;

  L815:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 160;
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L978;

  L847:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L848;
  L853:
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L854;
    }
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L978;

  L848:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 166;
    }
  x2 = XEXP (x1, 0);
  goto L853;

  L854:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 167;
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L978;

  L876:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 172;
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L978;

  L942:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 190;
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L978;

  L979:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != QImode)
    goto ret0;
  switch (GET_CODE (x1))
    {
    case ASHIFT:
      goto L980;
    case ASHIFTRT:
      goto L1019;
    case LSHIFT:
      goto L1058;
    case LSHIFTRT:
      goto L1097;
    case ROTATE:
      goto L1124;
    case ROTATERT:
      goto L1151;
    }
  goto ret0;

  L980:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L981;
  goto ret0;

  L981:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 197;
    }
  goto ret0;

  L1019:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1020;
  goto ret0;

  L1020:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 204;
    }
  goto ret0;

  L1058:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1059;
  goto ret0;

  L1059:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 211;
    }
  goto ret0;

  L1097:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1098;
  goto ret0;

  L1098:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 218;
    }
  goto ret0;

  L1124:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1125;
  goto ret0;

  L1125:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 223;
    }
  goto ret0;

  L1151:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1152;
  goto ret0;

  L1152:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 228;
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
  register rtx x1, x2, x3, x4, x5, x6;
  int tem;

  x1 = XEXP (x0, 1);
  x2 = XEXP (x1, 0);
  switch (GET_CODE (x2))
    {
    case EQ:
      goto L1326;
    case NE:
      goto L1335;
    case GT:
      goto L1344;
    case GTU:
      goto L1353;
    case LT:
      goto L1362;
    case LTU:
      goto L1371;
    case GE:
      goto L1380;
    case GEU:
      goto L1389;
    case LE:
      goto L1398;
    case LEU:
      goto L1407;
    }
  goto ret0;

  L1326:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1327;
  goto ret0;

  L1327:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1328;
  goto ret0;

  L1328:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1329;
    case PC:
      goto L1419;
    }
  goto ret0;

  L1329:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1330;

  L1330:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 258;
  goto ret0;

  L1419:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1420;
  goto ret0;

  L1420:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 268;

  L1335:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1336;
  goto ret0;

  L1336:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1337;
  goto ret0;

  L1337:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1338;
    case PC:
      goto L1428;
    }
  goto ret0;

  L1338:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1339;

  L1339:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 259;
  goto ret0;

  L1428:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1429;
  goto ret0;

  L1429:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 269;

  L1344:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1345;
  goto ret0;

  L1345:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1346;
  goto ret0;

  L1346:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1347;
    case PC:
      goto L1437;
    }
  goto ret0;

  L1347:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1348;

  L1348:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 260;
  goto ret0;

  L1437:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1438;
  goto ret0;

  L1438:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 270;

  L1353:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1354;
  goto ret0;

  L1354:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1355;
  goto ret0;

  L1355:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1356;
    case PC:
      goto L1446;
    }
  goto ret0;

  L1356:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1357;

  L1357:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 261;
  goto ret0;

  L1446:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1447;
  goto ret0;

  L1447:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 271;

  L1362:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1363;
  goto ret0;

  L1363:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1364;
  goto ret0;

  L1364:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1365;
    case PC:
      goto L1455;
    }
  goto ret0;

  L1365:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1366;

  L1366:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 262;
  goto ret0;

  L1455:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1456;
  goto ret0;

  L1456:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 272;

  L1371:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1372;
  goto ret0;

  L1372:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1373;
  goto ret0;

  L1373:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1374;
    case PC:
      goto L1464;
    }
  goto ret0;

  L1374:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1375;

  L1375:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 263;
  goto ret0;

  L1464:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1465;
  goto ret0;

  L1465:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 273;

  L1380:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1381;
  goto ret0;

  L1381:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1382;
  goto ret0;

  L1382:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1383;
    case PC:
      goto L1473;
    }
  goto ret0;

  L1383:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1384;

  L1384:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 264;
  goto ret0;

  L1473:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1474;
  goto ret0;

  L1474:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 274;

  L1389:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1390;
  goto ret0;

  L1390:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1391;
  goto ret0;

  L1391:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1392;
    case PC:
      goto L1482;
    }
  goto ret0;

  L1392:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1393;

  L1393:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 265;
  goto ret0;

  L1482:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1483;
  goto ret0;

  L1483:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 275;

  L1398:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1399;
  goto ret0;

  L1399:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1400;
  goto ret0;

  L1400:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1401;
    case PC:
      goto L1491;
    }
  goto ret0;

  L1401:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1402;

  L1402:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 266;
  goto ret0;

  L1491:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1492;
  goto ret0;

  L1492:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 276;

  L1407:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1408;
  goto ret0;

  L1408:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1409;
  goto ret0;

  L1409:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1410;
    case PC:
      goto L1500;
    }
  goto ret0;

  L1410:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1411;

  L1411:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 267;
  goto ret0;

  L1500:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1501;
  goto ret0;

  L1501:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 277;
 ret0: return -1;
}

int
recog_6 (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5, x6;
  int tem;

  x1 = XEXP (x0, 0);
  switch (GET_MODE (x1))
    {
    case DFmode:
      if (GET_CODE (x1) == MEM && push_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L2;
	}
    L173:
      if (general_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L227;
	}
      break;
    case DImode:
      if (GET_CODE (x1) == MEM && push_operand (x1, DImode))
	{
	  ro[0] = x1;
	  goto L5;
	}
    L184:
      if (general_operand (x1, DImode))
	{
	  ro[0] = x1;
	  goto L185;
	}
      break;
    case SImode:
      if (GET_CODE (x1) == MEM && push_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L148;
	}
    L150:
      if (general_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L203;
	}
      break;
    case HImode:
      if (general_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L199;
	}
    L961:
      if (register_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L962;
	}
      break;
    case QImode:
      if (general_operand (x1, QImode))
	{
	  ro[0] = x1;
	  goto L191;
	}
    L972:
      if (register_operand (x1, QImode))
	{
	  ro[0] = x1;
	  goto L973;
	}
      break;
    case SFmode:
      if (general_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L237;
	}
      break;
    case XFmode:
      if (nonimmediate_operand (x1, XFmode))
	{
	  ro[0] = x1;
	  goto L177;
	}
    }
  L187:
  switch (GET_MODE (x1))
    {
    case SImode:
      switch (GET_CODE (x1))
	{
	case MEM:
	  if (push_operand (x1, SImode))
	    {
	      ro[0] = x1;
	      goto L188;
	    }
	  break;
	case ZERO_EXTRACT:
	  goto L1155;
	}
    L373:
      if (general_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L374;
	}
    L739:
      if (not_sp_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L740;
	}
    L944:
      if (register_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L945;
	}
    }
  switch (GET_CODE (x1))
    {
    case CC0:
      goto L8;
    case STRICT_LOW_PART:
      goto L160;
    case PC:
      goto L1324;
    }
  L1595:
  ro[0] = x1;
  goto L1596;
  L1612:
  switch (GET_CODE (x1))
    {
    case PC:
      goto L1613;
    case CC0:
      goto L1703;
    }
  switch (GET_MODE (x1))
    {
    case SImode:
      if (general_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L1751;
	}
      break;
    case DFmode:
      if (register_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L1619;
	}
    L1718:
      if (general_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L1719;
	}
      break;
    case SFmode:
      if (register_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L1626;
	}
    L1722:
      if (general_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L1723;
	}
      break;
    case XFmode:
      if (general_operand (x1, XFmode))
	{
	  ro[0] = x1;
	  goto L1711;
	}
      break;
    case QImode:
      if (general_operand (x1, QImode))
	{
	  ro[0] = x1;
	  goto L1743;
	}
      break;
    case HImode:
      if (general_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L1747;
	}
    }
  goto ret0;

  L2:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, DFmode))
    {
      ro[1] = x1;
      return 0;
    }
  x1 = XEXP (x0, 0);
  goto L173;

  L227:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case DFmode:
      switch (GET_CODE (x1))
	{
	case FLOAT_EXTEND:
	  goto L228;
	case FLOAT:
	  goto L262;
	case FIX:
	  goto L333;
	case PLUS:
	  goto L420;
	case MINUS:
	  goto L477;
	case MULT:
	  goto L599;
	case DIV:
	  goto L659;
	case NEG:
	  goto L890;
	case SQRT:
	  goto L900;
	case ABS:
	  goto L914;
	}
    }
  if (general_operand (x1, DFmode))
    {
      ro[1] = x1;
      return 35;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L228:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    goto L233;
  x1 = XEXP (x0, 0);
  goto L1595;

  L233:
  ro[1] = x2;
  if (TARGET_FPA)
    return 54;
  L234:
  ro[1] = x2;
  if (TARGET_68881)
    return 55;
  x1 = XEXP (x0, 0);
  goto L1595;

  L262:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    goto L267;
  L276:
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 67;
      }
  L284:
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 69;
      }
  x1 = XEXP (x0, 0);
  goto L1595;

  L267:
  ro[1] = x2;
  if (TARGET_FPA)
    return 64;
  L268:
  ro[1] = x2;
  if (TARGET_68881)
    return 65;
  goto L276;

  L333:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_68881 && !TARGET_68040)
	return 73;
      }
  x1 = XEXP (x0, 0);
  goto L1595;

  L420:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L421;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L421:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    goto L427;
  x1 = XEXP (x0, 0);
  goto L1595;

  L427:
  ro[2] = x2;
  if (TARGET_FPA)
    return 92;
  L428:
  ro[2] = x2;
  if (TARGET_68881)
    return 93;
  x1 = XEXP (x0, 0);
  goto L1595;

  L477:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L478;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L478:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    goto L484;
  x1 = XEXP (x0, 0);
  goto L1595;

  L484:
  ro[2] = x2;
  if (TARGET_FPA)
    return 104;
  L485:
  ro[2] = x2;
  if (TARGET_68881)
    return 105;
  x1 = XEXP (x0, 0);
  goto L1595;

  L599:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L600;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L600:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    goto L606;
  x1 = XEXP (x0, 0);
  goto L1595;

  L606:
  ro[2] = x2;
  if (TARGET_FPA)
    return 122;
  L607:
  ro[2] = x2;
  if (TARGET_68881)
    return 123;
  x1 = XEXP (x0, 0);
  goto L1595;

  L659:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L660;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L660:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    goto L666;
  x1 = XEXP (x0, 0);
  goto L1595;

  L666:
  ro[2] = x2;
  if (TARGET_FPA)
    return 134;
  L667:
  ro[2] = x2;
  if (TARGET_68881)
    return 135;
  x1 = XEXP (x0, 0);
  goto L1595;

  L890:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    goto L895;
  x1 = XEXP (x0, 0);
  goto L1595;

  L895:
  ro[1] = x2;
  if (TARGET_FPA)
    return 177;
  L896:
  ro[1] = x2;
  if (TARGET_68881)
    return 178;
  x1 = XEXP (x0, 0);
  goto L1595;

  L900:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 179;
      }
  x1 = XEXP (x0, 0);
  goto L1595;

  L914:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    goto L919;
  x1 = XEXP (x0, 0);
  goto L1595;

  L919:
  ro[1] = x2;
  if (TARGET_FPA)
    return 184;
  L920:
  ro[1] = x2;
  if (TARGET_68881)
    return 185;
  x1 = XEXP (x0, 0);
  goto L1595;

  L5:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, DImode))
    {
      ro[1] = x1;
      return 1;
    }
  x1 = XEXP (x0, 0);
  goto L184;

  L185:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, DImode))
    {
      ro[1] = x1;
      return 39;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L148:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SImode))
    {
      ro[1] = x1;
      if (GET_CODE (operands[1]) == CONST_INT
   && INTVAL (operands[1]) >= -0x8000
   && INTVAL (operands[1]) < 0x8000)
	return 26;
      }
  x1 = XEXP (x0, 0);
  goto L150;

  L203:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case SImode:
      switch (GET_CODE (x1))
	{
	case ZERO_EXTEND:
	  goto L204;
	case SIGN_EXTEND:
	  goto L216;
	case FIX:
	  goto L298;
	}
    }
  if (GET_CODE (x1) == CONST_INT && XWINT (x1, 0) == 0 && 1)
    if ((TARGET_68020
    || !(GET_CODE (operands[0]) == MEM && MEM_VOLATILE_P (operands[0]))))
      return 27;
  L154:
  if (general_operand (x1, SImode))
    {
      ro[1] = x1;
      return 29;
    }
  x1 = XEXP (x0, 0);
  goto L187;

  L204:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      if (nonimmediate_operand (x2, HImode))
	{
	  ro[1] = x2;
	  if (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM)
	    return 47;
	  }
      break;
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[1] = x2;
	  if (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM)
	    return 49;
	  }
    }
  x1 = XEXP (x0, 0);
  goto L187;

  L216:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      if (nonimmediate_operand (x2, HImode))
	{
	  ro[1] = x2;
	  return 50;
	}
      break;
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[1] = x2;
	  if (TARGET_68020)
	    return 52;
	  }
    }
  x1 = XEXP (x0, 0);
  goto L187;

  L298:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DFmode:
      switch (GET_CODE (x2))
	{
	case FIX:
	  goto L299;
	}
      break;
    case SFmode:
      if (GET_CODE (x2) == FIX && 1)
	goto L366;
    }
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 77;
      }
  L361:
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 80;
      }
  x1 = XEXP (x0, 0);
  goto L187;

  L299:
  x3 = XEXP (x2, 0);
  if (pnum_clobbers != 0 && register_operand (x3, DFmode))
    {
      ro[1] = x3;
      if (TARGET_68040)
	{
	  *pnum_clobbers = 2;
	  return 70;
	}
      }
  L371:
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      if (TARGET_FPA)
	return 82;
      }
  x1 = XEXP (x0, 0);
  goto L187;

  L366:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_FPA)
	return 81;
      }
  x1 = XEXP (x0, 0);
  goto L187;
 L199:
  tem = recog_1 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L961;

  L962:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != HImode)
    {
      x1 = XEXP (x0, 0);
      goto L1595;
    }
  switch (GET_CODE (x1))
    {
    case ASHIFT:
      goto L963;
    case ASHIFTRT:
      goto L1002;
    case LSHIFT:
      goto L1041;
    case LSHIFTRT:
      goto L1080;
    case ROTATE:
      goto L1107;
    case ROTATERT:
      goto L1134;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L963:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L964;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L964:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 194;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1002:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L1003;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1003:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 201;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1041:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L1042;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1042:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 208;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1080:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L1081;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1081:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 215;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1107:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L1108;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1108:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 220;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1134:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L1135;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1135:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 225;
    }
  x1 = XEXP (x0, 0);
  goto L1595;
 L191:
  tem = recog_2 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L972;

  L973:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != QImode)
    {
      x1 = XEXP (x0, 0);
      goto L1595;
    }
  switch (GET_CODE (x1))
    {
    case ASHIFT:
      goto L974;
    case ASHIFTRT:
      goto L1013;
    case LSHIFT:
      goto L1052;
    case LSHIFTRT:
      goto L1091;
    case ROTATE:
      goto L1118;
    case ROTATERT:
      goto L1145;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L974:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L975;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L975:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 196;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1013:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L1014;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1014:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 203;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1052:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L1053;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1053:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 210;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1091:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L1092;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1092:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 217;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1118:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L1119;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1119:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 222;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1145:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L1146;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1146:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 227;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L237:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case SFmode:
      switch (GET_CODE (x1))
	{
	case FLOAT_TRUNCATE:
	  goto L238;
	case FLOAT:
	  goto L252;
	case FIX:
	  goto L337;
	case PLUS:
	  goto L432;
	case MINUS:
	  goto L489;
	case MULT:
	  goto L611;
	case DIV:
	  goto L671;
	case NEG:
	  goto L880;
	case ABS:
	  goto L904;
	}
    }
  if (general_operand (x1, SFmode))
    {
      ro[1] = x1;
      return 34;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L238:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    goto L243;
  L248:
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 59;
      }
  x1 = XEXP (x0, 0);
  goto L1595;

  L243:
  ro[1] = x2;
  if (TARGET_FPA)
    return 57;
  L244:
  ro[1] = x2;
  if (TARGET_68040_ONLY)
    return 58;
  goto L248;

  L252:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    goto L257;
  L272:
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 66;
      }
  L280:
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 68;
      }
  x1 = XEXP (x0, 0);
  goto L1595;

  L257:
  ro[1] = x2;
  if (TARGET_FPA)
    return 61;
  L258:
  ro[1] = x2;
  if (TARGET_68881)
    return 62;
  goto L272;

  L337:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_68881 && !TARGET_68040)
	return 74;
      }
  x1 = XEXP (x0, 0);
  goto L1595;

  L432:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L433;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L433:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    goto L439;
  x1 = XEXP (x0, 0);
  goto L1595;

  L439:
  ro[2] = x2;
  if (TARGET_FPA)
    return 95;
  L440:
  ro[2] = x2;
  if (TARGET_68881)
    return 96;
  x1 = XEXP (x0, 0);
  goto L1595;

  L489:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L490;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L490:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    goto L496;
  x1 = XEXP (x0, 0);
  goto L1595;

  L496:
  ro[2] = x2;
  if (TARGET_FPA)
    return 107;
  L497:
  ro[2] = x2;
  if (TARGET_68881)
    return 108;
  x1 = XEXP (x0, 0);
  goto L1595;

  L611:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L612;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L612:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    goto L618;
  x1 = XEXP (x0, 0);
  goto L1595;

  L618:
  ro[2] = x2;
  if (TARGET_FPA)
    return 125;
  L619:
  ro[2] = x2;
  if (TARGET_68881)
    return 126;
  x1 = XEXP (x0, 0);
  goto L1595;

  L671:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L672;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L672:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    goto L678;
  x1 = XEXP (x0, 0);
  goto L1595;

  L678:
  ro[2] = x2;
  if (TARGET_FPA)
    return 137;
  L679:
  ro[2] = x2;
  if (TARGET_68881)
    return 138;
  x1 = XEXP (x0, 0);
  goto L1595;

  L880:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    goto L885;
  x1 = XEXP (x0, 0);
  goto L1595;

  L885:
  ro[1] = x2;
  if (TARGET_FPA)
    return 174;
  L886:
  ro[1] = x2;
  if (TARGET_68881)
    return 175;
  x1 = XEXP (x0, 0);
  goto L1595;

  L904:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    goto L909;
  x1 = XEXP (x0, 0);
  goto L1595;

  L909:
  ro[1] = x2;
  if (TARGET_FPA)
    return 181;
  L910:
  ro[1] = x2;
  if (TARGET_68881)
    return 182;
  x1 = XEXP (x0, 0);
  goto L1595;

  L177:
  x1 = XEXP (x0, 1);
  if (nonimmediate_operand (x1, XFmode))
    goto L181;
  x1 = XEXP (x0, 0);
  goto L1595;

  L181:
  ro[1] = x1;
  if (TARGET_68881)
    return 37;
  L182:
  ro[1] = x1;
  if (! TARGET_68881)
    return 38;
  x1 = XEXP (x0, 0);
  goto L1595;

  L188:
  x1 = XEXP (x0, 1);
  if (address_operand (x1, SImode))
    {
      ro[1] = x1;
      return 40;
    }
  x1 = XEXP (x0, 0);
  goto L373;

  L1155:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L1156;
	}
      break;
    case SImode:
      if (nonimmediate_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L1162;
	}
    }
  goto L739;

  L1156:
  x2 = XEXP (x1, 1);
  if (immediate_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1157;
    }
  L1204:
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1205;
    }
  goto L739;

  L1157:
  x2 = XEXP (x1, 2);
  if (immediate_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L1158;
    }
  x2 = XEXP (x1, 1);
  goto L1204;

  L1158:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SImode))
    {
      ro[3] = x1;
      if (TARGET_68020 && TARGET_BITFIELD
   && GET_CODE (operands[1]) == CONST_INT
   && (INTVAL (operands[1]) == 32)
   && GET_CODE (operands[2]) == CONST_INT
   && (INTVAL (operands[2]) % 8) == 0
   && ! mode_dependent_address_p (XEXP (operands[0], 0)))
	return 229;
      }
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 1);
  goto L1204;

  L1205:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L1206;
    }
  goto L739;

  L1206:
  x1 = XEXP (x0, 1);
  switch (GET_CODE (x1))
    {
    case XOR:
      if (GET_MODE (x1) == SImode && 1)
	goto L1207;
      break;
    case CONST_INT:
      if (XWINT (x1, 0) == 0 && 1)
	if (TARGET_68020 && TARGET_BITFIELD)
	  return 238;
      if (XWINT (x1, 0) == -1 && 1)
	if (TARGET_68020 && TARGET_BITFIELD)
	  return 239;
    }
  L1229:
  if (general_operand (x1, SImode))
    {
      ro[3] = x1;
      if (TARGET_68020 && TARGET_BITFIELD)
	return 240;
      }
  x1 = XEXP (x0, 0);
  goto L739;

  L1207:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == ZERO_EXTRACT && 1)
    goto L1208;
  x1 = XEXP (x0, 0);
  goto L739;

  L1208:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[0]) && 1)
    goto L1209;
  x1 = XEXP (x0, 0);
  goto L739;

  L1209:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[1]) && 1)
    goto L1210;
  x1 = XEXP (x0, 0);
  goto L739;

  L1210:
  x3 = XEXP (x2, 2);
  if (rtx_equal_p (x3, ro[2]) && 1)
    goto L1211;
  x1 = XEXP (x0, 0);
  goto L739;

  L1211:
  x2 = XEXP (x1, 1);
  if (immediate_operand (x2, VOIDmode))
    {
      ro[3] = x2;
      if (TARGET_68020 && TARGET_BITFIELD
   && GET_CODE (operands[3]) == CONST_INT
   && (INTVAL (operands[3]) == -1
       || (GET_CODE (operands[1]) == CONST_INT
           && (~ INTVAL (operands[3]) & ((1 << INTVAL (operands[1]))- 1)) == 0)))
	return 237;
      }
  x1 = XEXP (x0, 0);
  goto L739;

  L1162:
  x2 = XEXP (x1, 1);
  if (immediate_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1163;
    }
  L1245:
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1246;
    }
  goto L739;

  L1163:
  x2 = XEXP (x1, 2);
  if (immediate_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L1164;
    }
  x2 = XEXP (x1, 1);
  goto L1245;

  L1164:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SImode))
    {
      ro[3] = x1;
      if (TARGET_68020 && TARGET_BITFIELD
   && GET_CODE (operands[1]) == CONST_INT
   && (INTVAL (operands[1]) == 8 || INTVAL (operands[1]) == 16)
   && GET_CODE (operands[2]) == CONST_INT
   && INTVAL (operands[2]) % INTVAL (operands[1]) == 0
   && (GET_CODE (operands[0]) == REG
       || ! mode_dependent_address_p (XEXP (operands[0], 0))))
	return 230;
      }
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 1);
  goto L1245;

  L1246:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L1247;
    }
  goto L739;

  L1247:
  x1 = XEXP (x0, 1);
  switch (GET_CODE (x1))
    {
    case CONST_INT:
      if (XWINT (x1, 0) == 0 && 1)
	if (TARGET_68020 && TARGET_BITFIELD)
	  return 243;
      if (XWINT (x1, 0) == -1 && 1)
	if (TARGET_68020 && TARGET_BITFIELD)
	  return 244;
    }
  L1259:
  if (general_operand (x1, SImode))
    {
      ro[3] = x1;
      if (TARGET_68020 && TARGET_BITFIELD)
	return 245;
      }
  x1 = XEXP (x0, 0);
  goto L739;
 L374:
  tem = recog_3 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L739;

  L740:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == SImode && GET_CODE (x1) == AND && 1)
    goto L741;
  x1 = XEXP (x0, 0);
  goto L944;

  L741:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L742;
    }
  x1 = XEXP (x0, 0);
  goto L944;

  L742:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 147;
    }
  x1 = XEXP (x0, 0);
  goto L944;

  L945:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != SImode)
    {
      x1 = XEXP (x0, 0);
      goto L1595;
    }
  switch (GET_CODE (x1))
    {
    case ASHIFT:
      goto L946;
    case ASHIFTRT:
      goto L985;
    case LSHIFT:
      goto L1024;
    case LSHIFTRT:
      goto L1063;
    case ROTATE:
      goto L1102;
    case ROTATERT:
      goto L1129;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L946:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L947;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L947:
  x2 = XEXP (x1, 1);
  if (immediate_operand (x2, SImode))
    goto L953;
  L959:
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 193;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L953:
  ro[2] = x2;
  if ((GET_CODE (operands[2]) == CONST_INT && INTVAL (operands[2]) == 16))
    return 191;
  L954:
  ro[2] = x2;
  if ((! TARGET_68020 && GET_CODE (operands[2]) == CONST_INT
    && INTVAL (operands[2]) > 16 && INTVAL (operands[2]) <= 24))
    return 192;
  goto L959;

  L985:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L986;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L986:
  x2 = XEXP (x1, 1);
  if (immediate_operand (x2, SImode))
    goto L992;
  L998:
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 200;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L992:
  ro[2] = x2;
  if ((GET_CODE (operands[2]) == CONST_INT && INTVAL (operands[2]) == 16))
    return 198;
  L993:
  ro[2] = x2;
  if ((! TARGET_68020 && GET_CODE (operands[2]) == CONST_INT
    && INTVAL (operands[2]) > 16 && INTVAL (operands[2]) <= 24))
    return 199;
  goto L998;

  L1024:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1025;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1025:
  x2 = XEXP (x1, 1);
  if (immediate_operand (x2, SImode))
    goto L1031;
  L1037:
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 207;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1031:
  ro[2] = x2;
  if ((GET_CODE (operands[2]) == CONST_INT && INTVAL (operands[2]) == 16))
    return 205;
  L1032:
  ro[2] = x2;
  if ((! TARGET_68020 && GET_CODE (operands[2]) == CONST_INT
    && INTVAL (operands[2]) > 16 && INTVAL (operands[2]) <= 24))
    return 206;
  goto L1037;

  L1063:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1064;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1064:
  x2 = XEXP (x1, 1);
  if (immediate_operand (x2, SImode))
    goto L1070;
  L1076:
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 214;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1070:
  ro[2] = x2;
  if ((GET_CODE (operands[2]) == CONST_INT && INTVAL (operands[2]) == 16))
    return 212;
  L1071:
  ro[2] = x2;
  if ((! TARGET_68020 && GET_CODE (operands[2]) == CONST_INT
    && INTVAL (operands[2]) > 16 && INTVAL (operands[2]) <= 24))
    return 213;
  goto L1076;

  L1102:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1103;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1103:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 219;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1129:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1130;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1130:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 224;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L8:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case SImode:
      if (nonimmediate_operand (x1, SImode))
	{
	  ro[0] = x1;
	  return 2;
	}
      break;
    case HImode:
      if (nonimmediate_operand (x1, HImode))
	{
	  ro[0] = x1;
	  return 3;
	}
      break;
    case QImode:
      if (nonimmediate_operand (x1, QImode))
	{
	  ro[0] = x1;
	  return 4;
	}
    }
  switch (GET_CODE (x1))
    {
    case COMPARE:
      goto L46;
    case ZERO_EXTRACT:
      goto L101;
    case CONST_INT:
    case CONST_DOUBLE:
    case CONST:
    case SYMBOL_REF:
    case LABEL_REF:
    case SUBREG:
    case REG:
    case MEM:
    L23:
      if (general_operand (x1, SFmode))
	goto L27;
    }
  L37:
  if (general_operand (x1, DFmode))
    goto L41;
  L1262:
  if (GET_MODE (x1) == SImode && GET_CODE (x1) == ZERO_EXTRACT && 1)
    goto L1263;
  x1 = XEXP (x0, 0);
  goto L1595;

  L46:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SImode:
      if (nonimmediate_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L47;
	}
      break;
    case HImode:
      if (nonimmediate_operand (x2, HImode))
	{
	  ro[0] = x2;
	  goto L52;
	}
      break;
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L57;
	}
      break;
    case DFmode:
      if (general_operand (x2, DFmode))
	{
	  ro[0] = x2;
	  goto L70;
	}
      break;
    case SFmode:
      if (general_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L90;
	}
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L47:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 11;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L52:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 12;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L57:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 13;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L70:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    goto L76;
  x1 = XEXP (x0, 0);
  goto L1595;

  L76:
  if (pnum_clobbers != 0 && 1)
    {
      ro[1] = x2;
      if (TARGET_FPA)
	{
	  *pnum_clobbers = 1;
	  return 15;
	}
      }
  L77:
  ro[1] = x2;
  if (TARGET_68881)
    return 16;
  x1 = XEXP (x0, 0);
  goto L1595;

  L90:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    goto L96;
  x1 = XEXP (x0, 0);
  goto L1595;

  L96:
  if (pnum_clobbers != 0 && 1)
    {
      ro[1] = x2;
      if (TARGET_FPA)
	{
	  *pnum_clobbers = 1;
	  return 18;
	}
      }
  L97:
  ro[1] = x2;
  if (TARGET_68881)
    return 19;
  x1 = XEXP (x0, 0);
  goto L1595;

  L101:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L102;
	}
      break;
    case SImode:
      if (nonimmediate_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L110;
	}
    }
  goto L1262;

  L102:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 1 && 1)
    goto L103;
  goto L1262;

  L103:
  x2 = XEXP (x1, 2);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MINUS && 1)
    goto L104;
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      if (GET_CODE (operands[1]) == CONST_INT
   && (unsigned) INTVAL (operands[1]) < 8)
	return 24;
      }
  goto L1262;

  L104:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 7 && 1)
    goto L121;
  goto L1262;

  L121:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == AND && 1)
    goto L122;
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      return 20;
    }
  goto L1262;

  L122:
  x4 = XEXP (x3, 0);
  if (general_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L123;
    }
  goto L1262;

  L123:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 7 && 1)
    return 22;
  goto L1262;

  L110:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 1 && 1)
    goto L111;
  goto L1262;

  L111:
  x2 = XEXP (x1, 2);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MINUS && 1)
    goto L112;
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      if (GET_CODE (operands[1]) == CONST_INT)
	return 25;
      }
  goto L1262;

  L112:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 31 && 1)
    goto L131;
  goto L1262;

  L131:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == AND && 1)
    goto L132;
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      return 21;
    }
  goto L1262;

  L132:
  x4 = XEXP (x3, 0);
  if (general_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L133;
    }
  goto L1262;

  L133:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 31 && 1)
    return 23;
  goto L1262;

  L27:
  if (pnum_clobbers != 0 && 1)
    {
      ro[0] = x1;
      if (TARGET_FPA)
	{
	  *pnum_clobbers = 1;
	  return 6;
	}
      }
  L28:
  ro[0] = x1;
  if (TARGET_68881)
    return 7;
  goto L37;

  L41:
  if (pnum_clobbers != 0 && 1)
    {
      ro[0] = x1;
      if (TARGET_FPA)
	{
	  *pnum_clobbers = 1;
	  return 9;
	}
      }
  L42:
  ro[0] = x1;
  if (TARGET_68881)
    return 10;
  x1 = XEXP (x0, 0);
  goto L1595;

  L1263:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case QImode:
      if (memory_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L1264;
	}
      break;
    case SImode:
      if (nonimmediate_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L1270;
	}
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1264:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1265;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1265:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      if (TARGET_68020 && TARGET_BITFIELD
   && GET_CODE (operands[1]) == CONST_INT)
	return 246;
      }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1270:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1271;
    }
  x1 = XEXP (x0, 0);
  goto L1595;

  L1271:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      if (TARGET_68020 && TARGET_BITFIELD
   && GET_CODE (operands[1]) == CONST_INT)
	return 247;
      }
  x1 = XEXP (x0, 0);
  goto L1595;
 L160:
  tem = recog_4 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  goto L1595;

  L1324:
  x1 = XEXP (x0, 1);
  switch (GET_CODE (x1))
    {
    case IF_THEN_ELSE:
      goto L1325;
    case LABEL_REF:
      goto L1505;
    }
  x1 = XEXP (x0, 0);
  goto L1595;
 L1325:
  tem = recog_5 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L1595;

  L1505:
  x2 = XEXP (x1, 0);
  ro[0] = x2;
  return 278;

  L1596:
  x1 = XEXP (x0, 1);
  if (GET_CODE (x1) == CALL && 1)
    goto L1597;
  x1 = XEXP (x0, 0);
  goto L1612;

  L1597:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L1598;
    }
  x1 = XEXP (x0, 0);
  goto L1612;

  L1598:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    goto L1604;
  x1 = XEXP (x0, 0);
  goto L1612;

  L1604:
  ro[2] = x2;
  if (! flag_pic)
    return 290;
  L1605:
  ro[2] = x2;
  if (flag_pic)
    return 291;
  x1 = XEXP (x0, 0);
  goto L1612;

  L1613:
  x1 = XEXP (x0, 1);
  if (address_operand (x1, SImode))
    {
      ro[0] = x1;
      return 297;
    }
  goto ret0;

  L1703:
  x1 = XEXP (x0, 1);
  if (nonimmediate_operand (x1, XFmode))
    {
      ro[0] = x1;
      if (TARGET_68881)
	return 318;
      }
  if (GET_CODE (x1) == COMPARE && 1)
    goto L1707;
  goto ret0;

  L1707:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, XFmode))
    {
      ro[0] = x2;
      goto L1708;
    }
  goto ret0;

  L1708:
  x2 = XEXP (x1, 1);
  if (nonimmediate_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 320;
      }
  goto ret0;

  L1751:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == SImode && GET_CODE (x1) == FIX && 1)
    goto L1752;
  if (address_operand (x1, QImode))
    {
      ro[1] = x1;
      return 298;
    }
  goto ret0;

  L1752:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 331;
      }
  goto ret0;

  L1619:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != DFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1718;
    }
  switch (GET_CODE (x1))
    {
    case PLUS:
      goto L1620;
    case MINUS:
      goto L1648;
    case MULT:
      goto L1662;
    }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1620:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DFmode && GET_CODE (x2) == MULT && 1)
    goto L1621;
  x1 = XEXP (x0, 0);
  goto L1718;

  L1621:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L1622;
    }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1622:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, DFmode))
    {
      ro[2] = x3;
      goto L1623;
    }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1623:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    {
      ro[3] = x2;
      if (TARGET_FPA)
	return 306;
      }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1648:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DFmode && GET_CODE (x2) == MULT && 1)
    goto L1649;
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L1635;
    }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1649:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L1650;
    }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1650:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, DFmode))
    {
      ro[2] = x3;
      goto L1651;
    }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1651:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    {
      ro[3] = x2;
      if (TARGET_FPA)
	return 310;
      }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1635:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == DFmode && GET_CODE (x2) == MULT && 1)
    goto L1636;
  x1 = XEXP (x0, 0);
  goto L1718;

  L1636:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[2] = x3;
      goto L1637;
    }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1637:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, DFmode))
    {
      ro[3] = x3;
      if (TARGET_FPA)
	return 308;
      }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1662:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DFmode:
      switch (GET_CODE (x2))
	{
	case PLUS:
	  goto L1663;
	case MINUS:
	  goto L1677;
	}
    }
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L1684;
    }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1663:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L1664;
    }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1664:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, DFmode))
    {
      ro[2] = x3;
      goto L1665;
    }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1665:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    {
      ro[3] = x2;
      if (TARGET_FPA)
	return 312;
      }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1677:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L1678;
    }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1678:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, DFmode))
    {
      ro[2] = x3;
      goto L1679;
    }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1679:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    {
      ro[3] = x2;
      if (TARGET_FPA)
	return 314;
      }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1684:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == DFmode && GET_CODE (x2) == MINUS && 1)
    goto L1685;
  x1 = XEXP (x0, 0);
  goto L1718;

  L1685:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[2] = x3;
      goto L1686;
    }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1686:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, DFmode))
    {
      ro[3] = x3;
      if (TARGET_FPA)
	return 315;
      }
  x1 = XEXP (x0, 0);
  goto L1718;

  L1719:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == DFmode && GET_CODE (x1) == FLOAT_TRUNCATE && 1)
    goto L1720;
  goto ret0;

  L1720:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 323;
      }
  goto ret0;

  L1626:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != SFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1722;
    }
  switch (GET_CODE (x1))
    {
    case PLUS:
      goto L1627;
    case MINUS:
      goto L1655;
    case MULT:
      goto L1669;
    }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1627:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SFmode && GET_CODE (x2) == MULT && 1)
    goto L1628;
  x1 = XEXP (x0, 0);
  goto L1722;

  L1628:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L1629;
    }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1629:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SFmode))
    {
      ro[2] = x3;
      goto L1630;
    }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1630:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    {
      ro[3] = x2;
      if (TARGET_FPA)
	return 307;
      }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1655:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SFmode && GET_CODE (x2) == MULT && 1)
    goto L1656;
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L1642;
    }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1656:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L1657;
    }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1657:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SFmode))
    {
      ro[2] = x3;
      goto L1658;
    }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1658:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    {
      ro[3] = x2;
      if (TARGET_FPA)
	return 311;
      }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1642:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SFmode && GET_CODE (x2) == MULT && 1)
    goto L1643;
  x1 = XEXP (x0, 0);
  goto L1722;

  L1643:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[2] = x3;
      goto L1644;
    }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1644:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SFmode))
    {
      ro[3] = x3;
      if (TARGET_FPA)
	return 309;
      }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1669:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SFmode:
      switch (GET_CODE (x2))
	{
	case PLUS:
	  goto L1670;
	case MINUS:
	  goto L1691;
	}
    }
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L1698;
    }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1670:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L1671;
    }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1671:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SFmode))
    {
      ro[2] = x3;
      goto L1672;
    }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1672:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    {
      ro[3] = x2;
      if (TARGET_FPA)
	return 313;
      }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1691:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L1692;
    }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1692:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SFmode))
    {
      ro[2] = x3;
      goto L1693;
    }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1693:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    {
      ro[3] = x2;
      if (TARGET_FPA)
	return 316;
      }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1698:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SFmode && GET_CODE (x2) == MINUS && 1)
    goto L1699;
  x1 = XEXP (x0, 0);
  goto L1722;

  L1699:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[2] = x3;
      goto L1700;
    }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1700:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SFmode))
    {
      ro[3] = x3;
      if (TARGET_FPA)
	return 317;
      }
  x1 = XEXP (x0, 0);
  goto L1722;

  L1723:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == SFmode && GET_CODE (x1) == FLOAT_TRUNCATE && 1)
    goto L1724;
  goto ret0;

  L1724:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 324;
      }
  goto ret0;

  L1711:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != XFmode)
    goto ret0;
  switch (GET_CODE (x1))
    {
    case FLOAT_EXTEND:
      goto L1712;
    case FLOAT:
      goto L1728;
    case FIX:
      goto L1740;
    case PLUS:
      goto L1756;
    case MINUS:
      goto L1761;
    case MULT:
      goto L1766;
    case DIV:
      goto L1771;
    case NEG:
      goto L1776;
    case ABS:
      goto L1780;
    case SQRT:
      goto L1784;
    }
  goto ret0;

  L1712:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 321;
      }
  L1716:
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 322;
      }
  goto ret0;

  L1728:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 325;
      }
  L1732:
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 326;
      }
  L1736:
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 327;
      }
  goto ret0;

  L1740:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 328;
      }
  goto ret0;

  L1756:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, XFmode))
    {
      ro[1] = x2;
      goto L1757;
    }
  goto ret0;

  L1757:
  x2 = XEXP (x1, 1);
  if (nonimmediate_operand (x2, XFmode))
    {
      ro[2] = x2;
      if (TARGET_68881)
	return 333;
      }
  goto ret0;

  L1761:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, XFmode))
    {
      ro[1] = x2;
      goto L1762;
    }
  goto ret0;

  L1762:
  x2 = XEXP (x1, 1);
  if (nonimmediate_operand (x2, XFmode))
    {
      ro[2] = x2;
      if (TARGET_68881)
	return 335;
      }
  goto ret0;

  L1766:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, XFmode))
    {
      ro[1] = x2;
      goto L1767;
    }
  goto ret0;

  L1767:
  x2 = XEXP (x1, 1);
  if (nonimmediate_operand (x2, XFmode))
    {
      ro[2] = x2;
      if (TARGET_68881)
	return 337;
      }
  goto ret0;

  L1771:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, XFmode))
    {
      ro[1] = x2;
      goto L1772;
    }
  goto ret0;

  L1772:
  x2 = XEXP (x1, 1);
  if (nonimmediate_operand (x2, XFmode))
    {
      ro[2] = x2;
      if (TARGET_68881)
	return 339;
      }
  goto ret0;

  L1776:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 340;
      }
  goto ret0;

  L1780:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 341;
      }
  goto ret0;

  L1784:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 342;
      }
  goto ret0;

  L1743:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == QImode && GET_CODE (x1) == FIX && 1)
    goto L1744;
  goto ret0;

  L1744:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 329;
      }
  goto ret0;

  L1747:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == HImode && GET_CODE (x1) == FIX && 1)
    goto L1748;
  goto ret0;

  L1748:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_68881)
	return 330;
      }
  goto ret0;
 ret0: return -1;
}

int
recog_7 (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5, x6;
  int tem;

  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  switch (GET_MODE (x2))
    {
    case SImode:
      if (GET_CODE (x2) == PLUS && 1)
	goto L1517;
      if (register_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L1510;
	}
    }
  if (GET_CODE (x2) == IF_THEN_ELSE && 1)
    goto L1526;
  goto ret0;

  L1517:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == PC && 1)
    goto L1518;
  goto ret0;

  L1518:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, HImode))
    {
      ro[0] = x3;
      goto L1519;
    }
  goto ret0;

  L1519:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L1520;
  goto ret0;

  L1520:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1521;
  goto ret0;

  L1521:
  x3 = XEXP (x2, 0);
  ro[1] = x3;
  return 281;

  L1510:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L1511;
  goto ret0;

  L1511:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1512;
  goto ret0;

  L1512:
  x3 = XEXP (x2, 0);
  ro[1] = x3;
  return 280;

  L1526:
  x3 = XEXP (x2, 0);
  switch (GET_CODE (x3))
    {
    case NE:
      goto L1527;
    case GE:
      goto L1557;
    }
  goto ret0;

  L1527:
  x4 = XEXP (x3, 0);
  switch (GET_MODE (x4))
    {
    case HImode:
      if (general_operand (x4, HImode))
	{
	  ro[0] = x4;
	  goto L1528;
	}
      break;
    case SImode:
      if (general_operand (x4, SImode))
	{
	  ro[0] = x4;
	  goto L1543;
	}
    }
  goto ret0;

  L1528:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L1529;
  goto ret0;

  L1529:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == LABEL_REF && 1)
    goto L1530;
  goto ret0;

  L1530:
  x4 = XEXP (x3, 0);
  ro[1] = x4;
  goto L1531;

  L1531:
  x3 = XEXP (x2, 2);
  if (GET_CODE (x3) == PC && 1)
    goto L1532;
  goto ret0;

  L1532:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L1533;
  goto ret0;

  L1533:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1534;
  goto ret0;

  L1534:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == PLUS && 1)
    goto L1535;
  goto ret0;

  L1535:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[0]) && 1)
    goto L1536;
  goto ret0;

  L1536:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == -1 && 1)
    return 282;
  goto ret0;

  L1543:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L1544;
  goto ret0;

  L1544:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == LABEL_REF && 1)
    goto L1545;
  goto ret0;

  L1545:
  x4 = XEXP (x3, 0);
  ro[1] = x4;
  goto L1546;

  L1546:
  x3 = XEXP (x2, 2);
  if (GET_CODE (x3) == PC && 1)
    goto L1547;
  goto ret0;

  L1547:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L1548;
  goto ret0;

  L1548:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1549;
  goto ret0;

  L1549:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == PLUS && 1)
    goto L1550;
  goto ret0;

  L1550:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[0]) && 1)
    goto L1551;
  goto ret0;

  L1551:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == -1 && 1)
    return 283;
  goto ret0;

  L1557:
  x4 = XEXP (x3, 0);
  if (GET_CODE (x4) != PLUS)
    goto ret0;
  switch (GET_MODE (x4))
    {
    case HImode:
      goto L1558;
    case SImode:
      goto L1575;
    }
  goto ret0;

  L1558:
  x5 = XEXP (x4, 0);
  if (general_operand (x5, HImode))
    {
      ro[0] = x5;
      goto L1559;
    }
  goto ret0;

  L1559:
  x5 = XEXP (x4, 1);
  if (GET_CODE (x5) == CONST_INT && XWINT (x5, 0) == -1 && 1)
    goto L1560;
  goto ret0;

  L1560:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L1561;
  goto ret0;

  L1561:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == LABEL_REF && 1)
    goto L1562;
  goto ret0;

  L1562:
  x4 = XEXP (x3, 0);
  ro[1] = x4;
  goto L1563;

  L1563:
  x3 = XEXP (x2, 2);
  if (GET_CODE (x3) == PC && 1)
    goto L1564;
  goto ret0;

  L1564:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L1565;
  goto ret0;

  L1565:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1566;
  goto ret0;

  L1566:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == PLUS && 1)
    goto L1567;
  goto ret0;

  L1567:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[0]) && 1)
    goto L1568;
  goto ret0;

  L1568:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == -1 && 1)
    if (find_reg_note (insn, REG_NONNEG, 0))
      return 284;
  goto ret0;

  L1575:
  x5 = XEXP (x4, 0);
  if (general_operand (x5, SImode))
    {
      ro[0] = x5;
      goto L1576;
    }
  goto ret0;

  L1576:
  x5 = XEXP (x4, 1);
  if (GET_CODE (x5) == CONST_INT && XWINT (x5, 0) == -1 && 1)
    goto L1577;
  goto ret0;

  L1577:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 0 && 1)
    goto L1578;
  goto ret0;

  L1578:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == LABEL_REF && 1)
    goto L1579;
  goto ret0;

  L1579:
  x4 = XEXP (x3, 0);
  ro[1] = x4;
  goto L1580;

  L1580:
  x3 = XEXP (x2, 2);
  if (GET_CODE (x3) == PC && 1)
    goto L1581;
  goto ret0;

  L1581:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L1582;
  goto ret0;

  L1582:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1583;
  goto ret0;

  L1583:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == PLUS && 1)
    goto L1584;
  goto ret0;

  L1584:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[0]) && 1)
    goto L1585;
  goto ret0;

  L1585:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == -1 && 1)
    if (find_reg_note (insn, REG_NONNEG, 0))
      return 285;
  goto ret0;
 ret0: return -1;
}

int
recog_8 (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5, x6;
  int tem;

  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SImode:
      if (register_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L537;
	}
    L718:
      if (general_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L719;
	}
    }
  switch (GET_CODE (x2))
    {
    case CC0:
      goto L18;
    case PC:
      goto L1516;
    }
  goto ret0;

  L537:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MULT && 1)
    goto L538;
  x2 = XEXP (x1, 0);
  goto L718;

  L538:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L539;
    }
  x2 = XEXP (x1, 0);
  goto L718;

  L539:
  x3 = XEXP (x2, 1);
  if (nonimmediate_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L540;
    }
  if (GET_CODE (x3) == CONST_INT && 1)
    {
      ro[2] = x3;
      goto L556;
    }
  x2 = XEXP (x1, 0);
  goto L718;

  L540:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L541;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L541:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L542;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L542:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == TRUNCATE && 1)
    goto L543;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L543:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) != DImode)
    {
      x1 = XVECEXP (x0, 0, 0);
      x2 = XEXP (x1, 0);
      goto L718;
    }
  switch (GET_CODE (x3))
    {
    case LSHIFTRT:
      goto L544;
    case ASHIFT:
      goto L575;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L544:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) == DImode && GET_CODE (x4) == MULT && 1)
    goto L545;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L545:
  x5 = XEXP (x4, 0);
  if (GET_MODE (x5) == DImode && GET_CODE (x5) == ZERO_EXTEND && 1)
    goto L546;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L546:
  x6 = XEXP (x5, 0);
  if (rtx_equal_p (x6, ro[1]) && 1)
    goto L547;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L547:
  x5 = XEXP (x4, 1);
  if (GET_MODE (x5) == DImode && GET_CODE (x5) == ZERO_EXTEND && 1)
    goto L548;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L548:
  x6 = XEXP (x5, 0);
  if (rtx_equal_p (x6, ro[2]) && 1)
    goto L549;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L549:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 32 && 1)
    if (TARGET_68020)
      return 116;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L575:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) == DImode && GET_CODE (x4) == MULT && 1)
    goto L576;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L576:
  x5 = XEXP (x4, 0);
  if (GET_MODE (x5) == DImode && GET_CODE (x5) == SIGN_EXTEND && 1)
    goto L577;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L577:
  x6 = XEXP (x5, 0);
  if (rtx_equal_p (x6, ro[1]) && 1)
    goto L578;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L578:
  x5 = XEXP (x4, 1);
  if (GET_MODE (x5) == DImode && GET_CODE (x5) == SIGN_EXTEND && 1)
    goto L579;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L579:
  x6 = XEXP (x5, 0);
  if (rtx_equal_p (x6, ro[2]) && 1)
    goto L580;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L580:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 32 && 1)
    if (TARGET_68020)
      return 119;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L556:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L557;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L557:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L558;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L558:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == TRUNCATE && 1)
    goto L559;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L559:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) != DImode)
    {
      x1 = XVECEXP (x0, 0, 0);
      x2 = XEXP (x1, 0);
      goto L718;
    }
  switch (GET_CODE (x3))
    {
    case LSHIFTRT:
      goto L560;
    case ASHIFT:
      goto L591;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L560:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) == DImode && GET_CODE (x4) == MULT && 1)
    goto L561;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L561:
  x5 = XEXP (x4, 0);
  if (GET_MODE (x5) == DImode && GET_CODE (x5) == ZERO_EXTEND && 1)
    goto L562;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L562:
  x6 = XEXP (x5, 0);
  if (rtx_equal_p (x6, ro[1]) && 1)
    goto L563;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L563:
  x5 = XEXP (x4, 1);
  if (rtx_equal_p (x5, ro[2]) && 1)
    goto L564;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L564:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 32 && 1)
    if (TARGET_68020
   && (unsigned) INTVAL (operands[2]) <= 0x7fffffff)
      return 117;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L591:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) == DImode && GET_CODE (x4) == MULT && 1)
    goto L592;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L592:
  x5 = XEXP (x4, 0);
  if (GET_MODE (x5) == DImode && GET_CODE (x5) == SIGN_EXTEND && 1)
    goto L593;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L593:
  x6 = XEXP (x5, 0);
  if (rtx_equal_p (x6, ro[1]) && 1)
    goto L594;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L594:
  x5 = XEXP (x4, 1);
  if (rtx_equal_p (x5, ro[2]) && 1)
    goto L595;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L595:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 32 && 1)
    if (TARGET_68020
   /* This test is a noop on 32 bit machines,
      but important for a cross-compiler hosted on 64-bit machines.  */
   && INTVAL (operands[2]) <= 0x7fffffff
   && INTVAL (operands[2]) >= -0x80000000)
      return 120;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L718;

  L719:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != SImode)
    goto ret0;
  switch (GET_CODE (x2))
    {
    case DIV:
      goto L720;
    case UDIV:
      goto L731;
    }
  goto ret0;

  L720:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L721;
    }
  goto ret0;

  L721:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L722;
    }
  goto ret0;

  L722:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L723;
  goto ret0;

  L723:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && general_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L724;
    }
  goto ret0;

  L724:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MOD && 1)
    goto L725;
  goto ret0;

  L725:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    goto L726;
  goto ret0;

  L726:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[2]) && 1)
    if (TARGET_68020)
      return 145;
  goto ret0;

  L731:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L732;
    }
  goto ret0;

  L732:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L733;
    }
  goto ret0;

  L733:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L734;
  goto ret0;

  L734:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && general_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L735;
    }
  goto ret0;

  L735:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == UMOD && 1)
    goto L736;
  goto ret0;

  L736:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    goto L737;
  goto ret0;

  L737:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[2]) && 1)
    if (TARGET_68020)
      return 146;
  goto ret0;

  L18:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    {
      ro[0] = x2;
      goto L19;
    }
  L32:
  if (general_operand (x2, DFmode))
    {
      ro[0] = x2;
      goto L33;
    }
  if (GET_CODE (x2) == COMPARE && 1)
    goto L62;
  goto ret0;

  L19:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L20;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L32;

  L20:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[1] = x2;
      if (TARGET_FPA)
	return 6;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L32;

  L33:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L34;
  goto ret0;

  L34:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[1] = x2;
      if (TARGET_FPA)
	return 9;
      }
  goto ret0;

  L62:
  x3 = XEXP (x2, 0);
  switch (GET_MODE (x3))
    {
    case DFmode:
      if (general_operand (x3, DFmode))
	{
	  ro[0] = x3;
	  goto L63;
	}
      break;
    case SFmode:
      if (general_operand (x3, SFmode))
	{
	  ro[0] = x3;
	  goto L83;
	}
    }
  goto ret0;

  L63:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L64;
    }
  goto ret0;

  L64:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L65;
  goto ret0;

  L65:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      if (TARGET_FPA)
	return 15;
      }
  goto ret0;

  L83:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L84;
    }
  goto ret0;

  L84:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L85;
  goto ret0;

  L85:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      if (TARGET_FPA)
	return 18;
      }
  goto ret0;
 L1516:
  return recog_7 (x0, insn, pnum_clobbers);
 ret0: return -1;
}

int
recog (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5, x6;
  int tem;

  L1609:
  switch (GET_CODE (x0))
    {
    case REG:
      if (GET_MODE (x0) == SImode && XINT (x0, 0) == 15 && 1)
	if (NEED_PROBE)
	  return 295;
      break;
    case SET:
      goto L1;
    case PARALLEL:
      if (XVECLEN (x0, 0) == 2 && 1)
	goto L16;
      if (XVECLEN (x0, 0) == 3 && 1)
	goto L286;
      break;
    case CALL:
      goto L1587;
    case UNSPEC_VOLATILE:
      if (XINT (x0, 1) == 0 && XVECLEN (x0, 0) == 1 && 1)
	goto L1607;
      break;
    case CONST_INT:
      if (XWINT (x0, 0) == 0 && 1)
	return 294;
      break;
    case RETURN:
      if (USE_RETURN_INSN)
	return 296;
    }
  goto ret0;
 L1:
  return recog_6 (x0, insn, pnum_clobbers);

  L16:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == SET && 1)
    goto L536;
  goto ret0;
 L536:
  return recog_8 (x0, insn, pnum_clobbers);

  L286:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == SET && 1)
    goto L287;
  goto ret0;

  L287:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SImode:
      if (general_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L288;
	}
      break;
    case HImode:
      if (general_operand (x2, HImode))
	{
	  ro[0] = x2;
	  goto L303;
	}
      break;
    case QImode:
      if (general_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L318;
	}
    }
  goto ret0;

  L288:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == FIX && 1)
    goto L289;
  goto ret0;

  L289:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == DFmode && GET_CODE (x3) == FIX && 1)
    goto L290;
  goto ret0;

  L290:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, DFmode))
    {
      ro[1] = x4;
      goto L291;
    }
  goto ret0;

  L291:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L292;
  goto ret0;

  L292:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L293;
    }
  goto ret0;

  L293:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L294;
  goto ret0;

  L294:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_68040)
	return 70;
      }
  goto ret0;

  L303:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == FIX && 1)
    goto L304;
  goto ret0;

  L304:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == DFmode && GET_CODE (x3) == FIX && 1)
    goto L305;
  goto ret0;

  L305:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, DFmode))
    {
      ro[1] = x4;
      goto L306;
    }
  goto ret0;

  L306:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L307;
  goto ret0;

  L307:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L308;
    }
  goto ret0;

  L308:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L309;
  goto ret0;

  L309:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_68040)
	return 71;
      }
  goto ret0;

  L318:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == QImode && GET_CODE (x2) == FIX && 1)
    goto L319;
  goto ret0;

  L319:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == DFmode && GET_CODE (x3) == FIX && 1)
    goto L320;
  goto ret0;

  L320:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, DFmode))
    {
      ro[1] = x4;
      goto L321;
    }
  goto ret0;

  L321:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L322;
  goto ret0;

  L322:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L323;
    }
  goto ret0;

  L323:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L324;
  goto ret0;

  L324:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_68040)
	return 72;
      }
  goto ret0;

  L1587:
  x1 = XEXP (x0, 0);
  if (memory_operand (x1, QImode))
    {
      ro[0] = x1;
      goto L1588;
    }
  goto ret0;

  L1588:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SImode))
    goto L1592;
  goto ret0;

  L1592:
  ro[1] = x1;
  if (! flag_pic)
    return 287;
  L1593:
  ro[1] = x1;
  if (flag_pic)
    return 288;
  goto ret0;

  L1607:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == CONST_INT && XWINT (x1, 0) == 0 && 1)
    return 293;
  goto ret0;
 ret0: return -1;
}

rtx
split_insns (x0, insn)
     register rtx x0;
     rtx insn;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5, x6;
  rtx tem;

  goto ret0;
 ret0: return 0;
}


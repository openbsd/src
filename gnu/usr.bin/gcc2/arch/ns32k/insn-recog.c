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
  register rtx x1, x2, x3, x4;
  int tem;

  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case SImode:
      switch (GET_CODE (x1))
	{
	case SIGN_EXTEND:
	  goto L105;
	case ZERO_EXTEND:
	  goto L125;
	case FIX:
	  goto L167;
	case UNSIGNED_FIX:
	  goto L197;
	case PLUS:
	  goto L256;
	case MINUS:
	  goto L308;
	case MULT:
	  goto L345;
	case DIV:
	  goto L377;
	case MOD:
	  goto L410;
	case AND:
	  goto L458;
	case IOR:
	  goto L476;
	case XOR:
	  goto L697;
	case NEG:
	  goto L514;
	case NOT:
	  goto L526;
	case ASHIFT:
	  goto L538;
	case ASHIFTRT:
	  goto L553;
	case LSHIFT:
	  goto L586;
	case LSHIFTRT:
	  goto L601;
	case ROTATE:
	  goto L634;
	case ROTATERT:
	  goto L649;
	case ZERO_EXTRACT:
	  goto L718;
	}
    }
  if (general_operand (x1, SImode))
    {
      ro[1] = x1;
      return 15;
    }
  L681:
  if (address_operand (x1, QImode))
    {
      ro[1] = x1;
      return 150;
    }
  goto ret0;

  L105:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      if (nonimmediate_operand (x2, HImode))
	{
	  ro[1] = x2;
	  return 25;
	}
      break;
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[1] = x2;
	  return 27;
	}
    }
  goto ret0;

  L125:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      if (nonimmediate_operand (x2, HImode))
	{
	  ro[1] = x2;
	  return 30;
	}
      break;
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[1] = x2;
	  return 32;
	}
    }
  goto ret0;

  L167:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SFmode:
      switch (GET_CODE (x2))
	{
	case FIX:
	  goto L168;
	}
      break;
    case DFmode:
      if (GET_CODE (x2) == FIX && 1)
	goto L183;
    }
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 52;
      }
  L237:
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 55;
      }
  goto ret0;

  L168:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_32081)
	return 40;
      }
  goto ret0;

  L183:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      if (TARGET_32081)
	return 43;
      }
  goto ret0;

  L197:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) != FIX)
    goto ret0;
  switch (GET_MODE (x2))
    {
    case SFmode:
      goto L198;
    case DFmode:
      goto L213;
    }
  goto ret0;

  L198:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_32081)
	return 46;
      }
  goto ret0;

  L213:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      if (TARGET_32081)
	return 49;
      }
  goto ret0;

  L256:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SImode:
      switch (GET_CODE (x2))
	{
	case REG:
	  if (XINT (x2, 0) == 16 && 1)
	    goto L257;
	  if (XINT (x2, 0) == 17 && 1)
	    goto L262;
	}
    }
  L266:
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L267;
    }
  goto L681;

  L257:
  x2 = XEXP (x1, 1);
  if (immediate_operand (x2, SImode))
    {
      ro[1] = x2;
      if (GET_CODE (operands[1]) == CONST_INT)
	return 59;
      }
  x2 = XEXP (x1, 0);
  goto L266;

  L262:
  x2 = XEXP (x1, 1);
  if (immediate_operand (x2, SImode))
    {
      ro[1] = x2;
      if (GET_CODE (operands[1]) == CONST_INT)
	return 60;
      }
  x2 = XEXP (x1, 0);
  goto L266;

  L267:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 61;
    }
  goto L681;

  L308:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L309;
    }
  goto L681;

  L309:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 69;
    }
  goto L681;

  L345:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L346;
    }
  goto L681;

  L346:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 76;
    }
  goto L681;

  L377:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L378;
    }
  goto ret0;

  L378:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 82;
    }
  goto ret0;

  L410:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L411;
    }
  goto ret0;

  L411:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 88;
    }
  goto ret0;

  L458:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == NOT && 1)
    goto L459;
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L444;
    }
  goto ret0;

  L459:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L460;
    }
  goto ret0;

  L460:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 97;
    }
  goto ret0;

  L444:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 94;
    }
  goto ret0;

  L476:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L477;
    }
  goto ret0;

  L477:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 100;
    }
  goto ret0;

  L697:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == ASHIFT && 1)
    goto L698;
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L492;
    }
  goto ret0;

  L698:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 1 && 1)
    goto L699;
  goto ret0;

  L699:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L700;
    }
  goto ret0;

  L700:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 153;
  goto ret0;

  L492:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 103;
    }
  goto ret0;

  L514:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 108;
    }
  goto ret0;

  L526:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 111;
    }
  goto ret0;

  L538:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L539;
    }
  goto ret0;

  L539:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 114;
    }
  goto ret0;

  L553:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L559;
    }
  goto ret0;

  L559:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == NEG && 1)
    goto L560;
  if (immediate_operand (x2, SImode))
    {
      ro[2] = x2;
      return 118;
    }
  goto ret0;

  L560:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      return 119;
    }
  goto ret0;

  L586:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L587;
    }
  goto ret0;

  L587:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 126;
    }
  goto ret0;

  L601:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L607;
    }
  goto ret0;

  L607:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == NEG && 1)
    goto L608;
  if (immediate_operand (x2, SImode))
    {
      ro[2] = x2;
      return 130;
    }
  goto ret0;

  L608:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      return 131;
    }
  goto ret0;

  L634:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L635;
    }
  goto ret0;

  L635:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 138;
    }
  goto ret0;

  L649:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L655;
    }
  goto ret0;

  L655:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == NEG && 1)
    goto L656;
  if (immediate_operand (x2, SImode))
    {
      ro[2] = x2;
      return 142;
    }
  goto ret0;

  L656:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      return 143;
    }
  goto ret0;

  L718:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SImode:
      if (register_operand (x2, SImode))
	{
	  ro[1] = x2;
	  goto L719;
	}
      break;
    case QImode:
      if (general_operand (x2, QImode))
	{
	  ro[1] = x2;
	  goto L731;
	}
    }
  goto ret0;

  L719:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      goto L720;
    }
  goto ret0;

  L720:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[3] = x2;
      if ((INTVAL (operands[2]) == 8 || INTVAL (operands[2]) == 16)
   && (INTVAL (operands[3]) == 8 || INTVAL (operands[3]) == 16 || INTVAL (operands[3]) == 24))
	return 156;
      }
  L726:
  if (general_operand (x2, SImode))
    {
      ro[3] = x2;
      if (! TARGET_32532)
	return 157;
      }
  goto ret0;

  L731:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      goto L732;
    }
  goto ret0;

  L732:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[3] = x2;
      if (! TARGET_32532)
	return 158;
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
  register rtx x1, x2, x3, x4;
  int tem;

  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case HImode:
      switch (GET_CODE (x1))
	{
	case TRUNCATE:
	  goto L97;
	case SIGN_EXTEND:
	  goto L109;
	case ZERO_EXTEND:
	  goto L129;
	case FIX:
	  goto L162;
	case UNSIGNED_FIX:
	  goto L192;
	case PLUS:
	  goto L271;
	case MINUS:
	  goto L313;
	case MULT:
	  goto L350;
	case DIV:
	  goto L382;
	case MOD:
	  goto L415;
	case AND:
	  goto L464;
	case IOR:
	  goto L481;
	case XOR:
	  goto L496;
	case NEG:
	  goto L518;
	case NOT:
	  goto L530;
	case ASHIFT:
	  goto L543;
	case ASHIFTRT:
	  goto L564;
	case LSHIFT:
	  goto L591;
	case LSHIFTRT:
	  goto L612;
	case ROTATE:
	  goto L639;
	case ROTATERT:
	  goto L660;
	}
    }
  if (general_operand (x1, HImode))
    {
      ro[1] = x1;
      return 16;
    }
  goto ret0;

  L97:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, SImode))
    {
      ro[1] = x2;
      return 23;
    }
  goto ret0;

  L109:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, QImode))
    {
      ro[1] = x2;
      return 26;
    }
  goto ret0;

  L129:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, QImode))
    {
      ro[1] = x2;
      return 31;
    }
  goto ret0;

  L162:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SFmode:
      switch (GET_CODE (x2))
	{
	case FIX:
	  goto L163;
	}
      break;
    case DFmode:
      if (GET_CODE (x2) == FIX && 1)
	goto L178;
    }
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 51;
      }
  L233:
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 54;
      }
  goto ret0;

  L163:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_32081)
	return 39;
      }
  goto ret0;

  L178:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      if (TARGET_32081)
	return 42;
      }
  goto ret0;

  L192:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) != FIX)
    goto ret0;
  switch (GET_MODE (x2))
    {
    case SFmode:
      goto L193;
    case DFmode:
      goto L208;
    }
  goto ret0;

  L193:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_32081)
	return 45;
      }
  goto ret0;

  L208:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      if (TARGET_32081)
	return 48;
      }
  goto ret0;

  L271:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L272;
    }
  goto ret0;

  L272:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 62;
    }
  goto ret0;

  L313:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L314;
    }
  goto ret0;

  L314:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 70;
    }
  goto ret0;

  L350:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L351;
    }
  goto ret0;

  L351:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 77;
    }
  goto ret0;

  L382:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L383;
    }
  goto ret0;

  L383:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 83;
    }
  goto ret0;

  L415:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L416;
    }
  goto ret0;

  L416:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 89;
    }
  goto ret0;

  L464:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == NOT && 1)
    goto L465;
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L449;
    }
  goto ret0;

  L465:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, HImode))
    {
      ro[1] = x3;
      goto L466;
    }
  goto ret0;

  L466:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 98;
    }
  goto ret0;

  L449:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 95;
    }
  goto ret0;

  L481:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L482;
    }
  goto ret0;

  L482:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 101;
    }
  goto ret0;

  L496:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L497;
    }
  goto ret0;

  L497:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 104;
    }
  goto ret0;

  L518:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 109;
    }
  goto ret0;

  L530:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 112;
    }
  goto ret0;

  L543:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L544;
    }
  goto ret0;

  L544:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 115;
    }
  goto ret0;

  L564:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L570;
    }
  goto ret0;

  L570:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == NEG && 1)
    goto L571;
  if (immediate_operand (x2, SImode))
    {
      ro[2] = x2;
      return 121;
    }
  goto ret0;

  L571:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      return 122;
    }
  goto ret0;

  L591:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L592;
    }
  goto ret0;

  L592:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 127;
    }
  goto ret0;

  L612:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L618;
    }
  goto ret0;

  L618:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == NEG && 1)
    goto L619;
  if (immediate_operand (x2, SImode))
    {
      ro[2] = x2;
      return 133;
    }
  goto ret0;

  L619:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      return 134;
    }
  goto ret0;

  L639:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L640;
    }
  goto ret0;

  L640:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 139;
    }
  goto ret0;

  L660:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L666;
    }
  goto ret0;

  L666:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == NEG && 1)
    goto L667;
  if (immediate_operand (x2, SImode))
    {
      ro[2] = x2;
      return 145;
    }
  goto ret0;

  L667:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      return 146;
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
  register rtx x1, x2, x3, x4;
  int tem;

  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case QImode:
      switch (GET_CODE (x1))
	{
	case TRUNCATE:
	  goto L93;
	case FIX:
	  goto L157;
	case UNSIGNED_FIX:
	  goto L187;
	case PLUS:
	  goto L282;
	case MINUS:
	  goto L324;
	case MULT:
	  goto L355;
	case DIV:
	  goto L387;
	case MOD:
	  goto L420;
	case AND:
	  goto L470;
	case IOR:
	  goto L486;
	case XOR:
	  goto L704;
	case NEG:
	  goto L522;
	case NOT:
	  goto L534;
	case ASHIFT:
	  goto L548;
	case ASHIFTRT:
	  goto L575;
	case LSHIFT:
	  goto L596;
	case LSHIFTRT:
	  goto L623;
	case ROTATE:
	  goto L644;
	case ROTATERT:
	  goto L671;
	}
    }
  if (general_operand (x1, QImode))
    {
      ro[1] = x1;
      return 18;
    }
  goto ret0;

  L93:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SImode:
      if (nonimmediate_operand (x2, SImode))
	{
	  ro[1] = x2;
	  return 22;
	}
      break;
    case HImode:
      if (nonimmediate_operand (x2, HImode))
	{
	  ro[1] = x2;
	  return 24;
	}
    }
  goto ret0;

  L157:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SFmode:
      switch (GET_CODE (x2))
	{
	case FIX:
	  goto L158;
	}
      break;
    case DFmode:
      if (GET_CODE (x2) == FIX && 1)
	goto L173;
    }
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 50;
      }
  L229:
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 53;
      }
  goto ret0;

  L158:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_32081)
	return 38;
      }
  goto ret0;

  L173:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      if (TARGET_32081)
	return 41;
      }
  goto ret0;

  L187:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) != FIX)
    goto ret0;
  switch (GET_MODE (x2))
    {
    case SFmode:
      goto L188;
    case DFmode:
      goto L203;
    }
  goto ret0;

  L188:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_32081)
	return 44;
      }
  goto ret0;

  L203:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      if (TARGET_32081)
	return 47;
      }
  goto ret0;

  L282:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L283;
    }
  goto ret0;

  L283:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 64;
    }
  goto ret0;

  L324:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L325;
    }
  goto ret0;

  L325:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 72;
    }
  goto ret0;

  L355:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L356;
    }
  goto ret0;

  L356:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 78;
    }
  goto ret0;

  L387:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L388;
    }
  goto ret0;

  L388:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 84;
    }
  goto ret0;

  L420:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L421;
    }
  goto ret0;

  L421:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 90;
    }
  goto ret0;

  L470:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == QImode && GET_CODE (x2) == NOT && 1)
    goto L471;
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L454;
    }
  goto ret0;

  L471:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, QImode))
    {
      ro[1] = x3;
      goto L472;
    }
  goto ret0;

  L472:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 99;
    }
  goto ret0;

  L454:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 96;
    }
  goto ret0;

  L486:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L487;
    }
  goto ret0;

  L487:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 102;
    }
  goto ret0;

  L704:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == QImode && GET_CODE (x2) == SUBREG && XINT (x2, 1) == 0 && 1)
    goto L705;
  L501:
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L502;
    }
  goto ret0;

  L705:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == ASHIFT && 1)
    goto L706;
  goto L501;

  L706:
  x4 = XEXP (x3, 0);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 1 && 1)
    goto L707;
  goto L501;

  L707:
  x4 = XEXP (x3, 1);
  if (general_operand (x4, QImode))
    {
      ro[1] = x4;
      goto L708;
    }
  goto L501;

  L708:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 154;
  x2 = XEXP (x1, 0);
  goto L501;

  L502:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 105;
    }
  goto ret0;

  L522:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 110;
    }
  goto ret0;

  L534:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 113;
    }
  goto ret0;

  L548:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L549;
    }
  goto ret0;

  L549:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 116;
    }
  goto ret0;

  L575:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L581;
    }
  goto ret0;

  L581:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == NEG && 1)
    goto L582;
  if (immediate_operand (x2, SImode))
    {
      ro[2] = x2;
      return 124;
    }
  goto ret0;

  L582:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      return 125;
    }
  goto ret0;

  L596:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L597;
    }
  goto ret0;

  L597:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 128;
    }
  goto ret0;

  L623:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L629;
    }
  goto ret0;

  L629:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == NEG && 1)
    goto L630;
  if (immediate_operand (x2, SImode))
    {
      ro[2] = x2;
      return 136;
    }
  goto ret0;

  L630:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      return 137;
    }
  goto ret0;

  L644:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L645;
    }
  goto ret0;

  L645:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 140;
    }
  goto ret0;

  L671:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L677;
    }
  goto ret0;

  L677:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == NEG && 1)
    goto L678;
  if (immediate_operand (x2, SImode))
    {
      ro[2] = x2;
      return 148;
    }
  goto ret0;

  L678:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      return 149;
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
  register rtx x1, x2, x3, x4;
  int tem;

  x1 = XEXP (x0, 1);
  x2 = XEXP (x1, 0);
  switch (GET_CODE (x2))
    {
    case EQ:
      goto L759;
    case NE:
      goto L768;
    case GT:
      goto L777;
    case GTU:
      goto L786;
    case LT:
      goto L795;
    case LTU:
      goto L804;
    case GE:
      goto L813;
    case GEU:
      goto L822;
    case LE:
      goto L831;
    case LEU:
      goto L840;
    }
  goto ret0;

  L759:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L760;
  goto ret0;

  L760:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L761;
  goto ret0;

  L761:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L762;
    case PC:
      goto L852;
    }
  goto ret0;

  L762:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L763;

  L763:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 163;
  goto ret0;

  L852:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L853;
  goto ret0;

  L853:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 173;

  L768:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L769;
  goto ret0;

  L769:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L770;
  goto ret0;

  L770:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L771;
    case PC:
      goto L861;
    }
  goto ret0;

  L771:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L772;

  L772:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 164;
  goto ret0;

  L861:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L862;
  goto ret0;

  L862:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 174;

  L777:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L778;
  goto ret0;

  L778:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L779;
  goto ret0;

  L779:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L780;
    case PC:
      goto L870;
    }
  goto ret0;

  L780:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L781;

  L781:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 165;
  goto ret0;

  L870:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L871;
  goto ret0;

  L871:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 175;

  L786:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L787;
  goto ret0;

  L787:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L788;
  goto ret0;

  L788:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L789;
    case PC:
      goto L879;
    }
  goto ret0;

  L789:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L790;

  L790:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 166;
  goto ret0;

  L879:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L880;
  goto ret0;

  L880:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 176;

  L795:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L796;
  goto ret0;

  L796:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L797;
  goto ret0;

  L797:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L798;
    case PC:
      goto L888;
    }
  goto ret0;

  L798:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L799;

  L799:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 167;
  goto ret0;

  L888:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L889;
  goto ret0;

  L889:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 177;

  L804:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L805;
  goto ret0;

  L805:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L806;
  goto ret0;

  L806:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L807;
    case PC:
      goto L897;
    }
  goto ret0;

  L807:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L808;

  L808:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 168;
  goto ret0;

  L897:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L898;
  goto ret0;

  L898:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 178;

  L813:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L814;
  goto ret0;

  L814:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L815;
  goto ret0;

  L815:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L816;
    case PC:
      goto L906;
    }
  goto ret0;

  L816:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L817;

  L817:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 169;
  goto ret0;

  L906:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L907;
  goto ret0;

  L907:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 179;

  L822:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L823;
  goto ret0;

  L823:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L824;
  goto ret0;

  L824:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L825;
    case PC:
      goto L915;
    }
  goto ret0;

  L825:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L826;

  L826:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 170;
  goto ret0;

  L915:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L916;
  goto ret0;

  L916:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 180;

  L831:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L832;
  goto ret0;

  L832:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L833;
  goto ret0;

  L833:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L834;
    case PC:
      goto L924;
    }
  goto ret0;

  L834:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L835;

  L835:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 171;
  goto ret0;

  L924:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L925;
  goto ret0;

  L925:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 181;

  L840:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L841;
  goto ret0;

  L841:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L842;
  goto ret0;

  L842:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L843;
    case PC:
      goto L933;
    }
  goto ret0;

  L843:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L844;

  L844:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 172;
  goto ret0;

  L933:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L934;
  goto ret0;

  L934:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 182;
 ret0: return -1;
}

int
recog_5 (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4;
  int tem;

  x1 = XEXP (x0, 0);
  switch (GET_MODE (x1))
    {
    case DFmode:
      if (general_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L116;
	}
      break;
    case SFmode:
      if (general_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L120;
	}
      break;
    case TImode:
      if (memory_operand (x1, TImode))
	{
	  ro[0] = x1;
	  goto L48;
	}
      break;
    case DImode:
      if (general_operand (x1, DImode))
	{
	  ro[0] = x1;
	  goto L359;
	}
      break;
    case SImode:
      switch (GET_CODE (x1))
	{
	case REG:
	  if (XINT (x1, 0) == 17 && 1)
	    goto L250;
	  break;
	case ZERO_EXTRACT:
	  goto L684;
	}
    L56:
      if (general_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L104;
	}
    L390:
      if (register_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L391;
	}
      break;
    case HImode:
      if (general_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L96;
	}
    L396:
      if (register_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L397;
	}
      break;
    case QImode:
      if (general_operand (x1, QImode))
	{
	  ro[0] = x1;
	  goto L92;
	}
    L402:
      if (register_operand (x1, QImode))
	{
	  ro[0] = x1;
	  goto L403;
	}
    }
  switch (GET_CODE (x1))
    {
    case CC0:
      goto L2;
    case STRICT_LOW_PART:
      goto L63;
    case PC:
      goto L753;
    }
  L969:
  ro[0] = x1;
  goto L970;
  L977:
  switch (GET_MODE (x1))
    {
    case SFmode:
      if (general_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L978;
	}
      break;
    case DFmode:
      if (general_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L982;
	}
      break;
    case SImode:
      if (general_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L986;
	}
      break;
    case HImode:
      if (general_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L990;
	}
      break;
    case QImode:
      if (general_operand (x1, QImode))
	{
	  ro[0] = x1;
	  goto L994;
	}
    }
  if (GET_CODE (x1) == PC && 1)
    goto L999;
  goto ret0;

  L116:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case DFmode:
      switch (GET_CODE (x1))
	{
	case FLOAT_EXTEND:
	  goto L117;
	case FLOAT:
	  goto L141;
	case PLUS:
	  goto L241;
	case MINUS:
	  goto L293;
	case MULT:
	  goto L335;
	case DIV:
	  goto L367;
	case NEG:
	  goto L506;
	}
    }
  if (general_operand (x1, DFmode))
    {
      ro[1] = x1;
      return 10;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L117:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 28;
      }
  x1 = XEXP (x0, 0);
  goto L969;

  L141:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 34;
      }
  L149:
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 36;
      }
  x1 = XEXP (x0, 0);
  goto L969;

  L241:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L242;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L242:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    {
      ro[2] = x2;
      if (TARGET_32081)
	return 56;
      }
  x1 = XEXP (x0, 0);
  goto L969;

  L293:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L294;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L294:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    {
      ro[2] = x2;
      if (TARGET_32081)
	return 66;
      }
  x1 = XEXP (x0, 0);
  goto L969;

  L335:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L336;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L336:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    {
      ro[2] = x2;
      if (TARGET_32081)
	return 74;
      }
  x1 = XEXP (x0, 0);
  goto L969;

  L367:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L368;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L368:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    {
      ro[2] = x2;
      if (TARGET_32081)
	return 80;
      }
  x1 = XEXP (x0, 0);
  goto L969;

  L506:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 106;
      }
  x1 = XEXP (x0, 0);
  goto L969;

  L120:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case SFmode:
      switch (GET_CODE (x1))
	{
	case FLOAT_TRUNCATE:
	  goto L121;
	case FLOAT:
	  goto L137;
	case PLUS:
	  goto L246;
	case MINUS:
	  goto L298;
	case MULT:
	  goto L340;
	case DIV:
	  goto L372;
	case NEG:
	  goto L510;
	}
    }
  if (general_operand (x1, SFmode))
    {
      ro[1] = x1;
      return 11;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L121:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 29;
      }
  x1 = XEXP (x0, 0);
  goto L969;

  L137:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 33;
      }
  L145:
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 35;
      }
  L153:
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 37;
      }
  x1 = XEXP (x0, 0);
  goto L969;

  L246:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L247;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L247:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    {
      ro[2] = x2;
      if (TARGET_32081)
	return 57;
      }
  x1 = XEXP (x0, 0);
  goto L969;

  L298:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L299;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L299:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    {
      ro[2] = x2;
      if (TARGET_32081)
	return 67;
      }
  x1 = XEXP (x0, 0);
  goto L969;

  L340:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L341;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L341:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    {
      ro[2] = x2;
      if (TARGET_32081)
	return 75;
      }
  x1 = XEXP (x0, 0);
  goto L969;

  L372:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L373;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L373:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    {
      ro[2] = x2;
      if (TARGET_32081)
	return 81;
      }
  x1 = XEXP (x0, 0);
  goto L969;

  L510:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 107;
      }
  x1 = XEXP (x0, 0);
  goto L969;

  L48:
  x1 = XEXP (x0, 1);
  if (memory_operand (x1, TImode))
    {
      ro[1] = x1;
      return 12;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L359:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == DImode && GET_CODE (x1) == MULT && 1)
    goto L360;
  if (general_operand (x1, DImode))
    {
      ro[1] = x1;
      return 13;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L360:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == ZERO_EXTEND && 1)
    goto L361;
  x1 = XEXP (x0, 0);
  goto L969;

  L361:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L362;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L362:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == ZERO_EXTEND && 1)
    goto L363;
  x1 = XEXP (x0, 0);
  goto L969;

  L363:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, SImode))
    {
      ro[2] = x3;
      return 79;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L250:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case SImode:
      switch (GET_CODE (x1))
	{
	case PLUS:
	  goto L251;
	case MINUS:
	  goto L303;
	}
    }
  if (general_operand (x1, SImode))
    {
      ro[0] = x1;
      return 14;
    }
  x1 = XEXP (x0, 0);
  goto L56;

  L251:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 17 && 1)
    goto L252;
  x1 = XEXP (x0, 0);
  goto L56;

  L252:
  x2 = XEXP (x1, 1);
  if (immediate_operand (x2, SImode))
    {
      ro[0] = x2;
      if (GET_CODE (operands[0]) == CONST_INT)
	return 58;
      }
  x1 = XEXP (x0, 0);
  goto L56;

  L303:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 17 && 1)
    goto L304;
  x1 = XEXP (x0, 0);
  goto L56;

  L304:
  x2 = XEXP (x1, 1);
  if (immediate_operand (x2, SImode))
    {
      ro[0] = x2;
      if (GET_CODE (operands[0]) == CONST_INT)
	return 68;
      }
  x1 = XEXP (x0, 0);
  goto L56;

  L684:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SImode:
      if (general_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L685;
	}
    L735:
      if (memory_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L736;
	}
    L741:
      if (register_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L742;
	}
      break;
    case QImode:
      if (general_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L748;
	}
    }
  goto L969;

  L685:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 1 && 1)
    goto L686;
  x2 = XEXP (x1, 0);
  goto L735;

  L686:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L687;
    }
  x2 = XEXP (x1, 0);
  goto L735;

  L687:
  x1 = XEXP (x0, 1);
  if (GET_CODE (x1) != CONST_INT)
    {
      x1 = XEXP (x0, 0);
      x2 = XEXP (x1, 0);
    goto L735;
    }
  if (XWINT (x1, 0) == 1 && 1)
    return 151;
  if (XWINT (x1, 0) == 0 && 1)
    return 152;
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L735;

  L736:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[1] = x2;
      goto L737;
    }
  x2 = XEXP (x1, 0);
  goto L741;

  L737:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L738;
    }
  x2 = XEXP (x1, 0);
  goto L741;

  L738:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SImode))
    {
      ro[3] = x1;
      return 159;
    }
  x1 = XEXP (x0, 0);
  x2 = XEXP (x1, 0);
  goto L741;

  L742:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[1] = x2;
      goto L743;
    }
  goto L969;

  L743:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L744;
    }
  goto L969;

  L744:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SImode))
    {
      ro[3] = x1;
      return 160;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L748:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[1] = x2;
      goto L749;
    }
  goto L969;

  L749:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L750;
    }
  goto L969;

  L750:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SImode))
    {
      ro[3] = x1;
      return 161;
    }
  x1 = XEXP (x0, 0);
  goto L969;
 L104:
  tem = recog_1 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L390;

  L391:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != SImode)
    {
      x1 = XEXP (x0, 0);
      goto L969;
    }
  switch (GET_CODE (x1))
    {
    case UDIV:
      goto L392;
    case UMOD:
      goto L425;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L392:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == SUBREG && XINT (x2, 1) == 0 && 1)
    goto L393;
  x1 = XEXP (x0, 0);
  goto L969;

  L393:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == DImode && reg_or_mem_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L394;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L394:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 85;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L425:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == SUBREG && XINT (x2, 1) == 0 && 1)
    goto L426;
  x1 = XEXP (x0, 0);
  goto L969;

  L426:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == DImode && reg_or_mem_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L427;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L427:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 91;
    }
  x1 = XEXP (x0, 0);
  goto L969;
 L96:
  tem = recog_2 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L396;

  L397:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != HImode)
    {
      x1 = XEXP (x0, 0);
      goto L969;
    }
  switch (GET_CODE (x1))
    {
    case UDIV:
      goto L398;
    case UMOD:
      goto L431;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L398:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == SUBREG && XINT (x2, 1) == 0 && 1)
    goto L399;
  x1 = XEXP (x0, 0);
  goto L969;

  L399:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == DImode && reg_or_mem_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L400;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L400:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 86;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L431:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == SUBREG && XINT (x2, 1) == 0 && 1)
    goto L432;
  x1 = XEXP (x0, 0);
  goto L969;

  L432:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == DImode && reg_or_mem_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L433;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L433:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 92;
    }
  x1 = XEXP (x0, 0);
  goto L969;
 L92:
  tem = recog_3 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L402;

  L403:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != QImode)
    {
      x1 = XEXP (x0, 0);
      goto L969;
    }
  switch (GET_CODE (x1))
    {
    case UDIV:
      goto L404;
    case UMOD:
      goto L437;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L404:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == QImode && GET_CODE (x2) == SUBREG && XINT (x2, 1) == 0 && 1)
    goto L405;
  x1 = XEXP (x0, 0);
  goto L969;

  L405:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == DImode && reg_or_mem_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L406;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L406:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 87;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L437:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == QImode && GET_CODE (x2) == SUBREG && XINT (x2, 1) == 0 && 1)
    goto L438;
  x1 = XEXP (x0, 0);
  goto L969;

  L438:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == DImode && reg_or_mem_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L439;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L439:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 93;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L2:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case SImode:
      if (nonimmediate_operand (x1, SImode))
	{
	  ro[0] = x1;
	  return 0;
	}
      break;
    case HImode:
      if (nonimmediate_operand (x1, HImode))
	{
	  ro[0] = x1;
	  return 1;
	}
      break;
    case QImode:
      if (nonimmediate_operand (x1, QImode))
	{
	  ro[0] = x1;
	  return 2;
	}
    }
  switch (GET_CODE (x1))
    {
    case COMPARE:
      goto L18;
    case ZERO_EXTRACT:
      goto L712;
    case CONST_INT:
    case CONST_DOUBLE:
    case CONST:
    case SYMBOL_REF:
    case LABEL_REF:
    case SUBREG:
    case REG:
    case MEM:
    L11:
      if (general_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  if (TARGET_32081)
	    return 3;
	  }
    }
  L14:
  if (general_operand (x1, SFmode))
    {
      ro[0] = x1;
      if (TARGET_32081)
	return 4;
      }
  x1 = XEXP (x0, 0);
  goto L969;

  L18:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SImode:
      if (nonimmediate_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L19;
	}
      break;
    case HImode:
      if (nonimmediate_operand (x2, HImode))
	{
	  ro[0] = x2;
	  goto L24;
	}
      break;
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L29;
	}
      break;
    case DFmode:
      if (general_operand (x2, DFmode))
	{
	  ro[0] = x2;
	  goto L34;
	}
      break;
    case SFmode:
      if (general_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L39;
	}
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L19:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 5;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L24:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 6;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L29:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 7;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L34:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 8;
      }
  x1 = XEXP (x0, 0);
  goto L969;

  L39:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 9;
      }
  x1 = XEXP (x0, 0);
  goto L969;

  L712:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && general_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L713;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L713:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 1 && 1)
    goto L714;
  x1 = XEXP (x0, 0);
  goto L969;

  L714:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 155;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L63:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      if (general_operand (x2, HImode))
	{
	  ro[0] = x2;
	  goto L276;
	}
      break;
    case QImode:
      if (general_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L287;
	}
    }
  goto L969;

  L276:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case HImode:
      switch (GET_CODE (x1))
	{
	case PLUS:
	  goto L277;
	case MINUS:
	  goto L319;
	}
    }
  if (general_operand (x1, HImode))
    {
      ro[1] = x1;
      return 17;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L277:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L278;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L278:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 63;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L319:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L320;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L320:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 71;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L287:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case QImode:
      switch (GET_CODE (x1))
	{
	case PLUS:
	  goto L288;
	case MINUS:
	  goto L330;
	}
    }
  if (general_operand (x1, QImode))
    {
      ro[1] = x1;
      return 19;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L288:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L289;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L289:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 65;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L330:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L331;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L331:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 73;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L753:
  x1 = XEXP (x0, 1);
  switch (GET_CODE (x1))
    {
    case LABEL_REF:
      goto L754;
    case IF_THEN_ELSE:
      goto L758;
    }
  x1 = XEXP (x0, 0);
  goto L969;

  L754:
  x2 = XEXP (x1, 0);
  ro[0] = x2;
  return 162;
 L758:
  tem = recog_4 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L969;

  L970:
  x1 = XEXP (x0, 1);
  if (GET_CODE (x1) == CALL && 1)
    goto L971;
  x1 = XEXP (x0, 0);
  goto L977;

  L971:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L972;
    }
  x1 = XEXP (x0, 0);
  goto L977;

  L972:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 186;
    }
  x1 = XEXP (x0, 0);
  goto L977;

  L978:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == SFmode && GET_CODE (x1) == ABS && 1)
    goto L979;
  goto ret0;

  L979:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 190;
      }
  goto ret0;

  L982:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == DFmode && GET_CODE (x1) == ABS && 1)
    goto L983;
  goto ret0;

  L983:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_32081)
	return 191;
      }
  goto ret0;

  L986:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != SImode)
    goto ret0;
  switch (GET_CODE (x1))
    {
    case ABS:
      goto L987;
    case EQ:
      goto L1012;
    case NE:
      goto L1027;
    case GT:
      goto L1042;
    case GTU:
      goto L1057;
    case LT:
      goto L1072;
    case LTU:
      goto L1087;
    case GE:
      goto L1102;
    case GEU:
      goto L1117;
    case LE:
      goto L1132;
    case LEU:
      goto L1147;
    case FFS:
      goto L1170;
    }
  goto ret0;

  L987:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 192;
    }
  goto ret0;

  L1012:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1013;
  goto ret0;

  L1013:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 198;
  goto ret0;

  L1027:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1028;
  goto ret0;

  L1028:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 201;
  goto ret0;

  L1042:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1043;
  goto ret0;

  L1043:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 204;
  goto ret0;

  L1057:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1058;
  goto ret0;

  L1058:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 207;
  goto ret0;

  L1072:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1073;
  goto ret0;

  L1073:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 210;
  goto ret0;

  L1087:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1088;
  goto ret0;

  L1088:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 213;
  goto ret0;

  L1102:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1103;
  goto ret0;

  L1103:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 216;
  goto ret0;

  L1117:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1118;
  goto ret0;

  L1118:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 219;
  goto ret0;

  L1132:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1133;
  goto ret0;

  L1133:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 222;
  goto ret0;

  L1147:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1148;
  goto ret0;

  L1148:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 225;
  goto ret0;

  L1170:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 230;
    }
  goto ret0;

  L990:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != HImode)
    goto ret0;
  switch (GET_CODE (x1))
    {
    case ABS:
      goto L991;
    case EQ:
      goto L1017;
    case NE:
      goto L1032;
    case GT:
      goto L1047;
    case GTU:
      goto L1062;
    case LT:
      goto L1077;
    case LTU:
      goto L1092;
    case GE:
      goto L1107;
    case GEU:
      goto L1122;
    case LE:
      goto L1137;
    case LEU:
      goto L1152;
    case FFS:
      goto L1166;
    }
  goto ret0;

  L991:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 193;
    }
  goto ret0;

  L1017:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1018;
  goto ret0;

  L1018:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 199;
  goto ret0;

  L1032:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1033;
  goto ret0;

  L1033:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 202;
  goto ret0;

  L1047:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1048;
  goto ret0;

  L1048:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 205;
  goto ret0;

  L1062:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1063;
  goto ret0;

  L1063:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 208;
  goto ret0;

  L1077:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1078;
  goto ret0;

  L1078:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 211;
  goto ret0;

  L1092:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1093;
  goto ret0;

  L1093:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 214;
  goto ret0;

  L1107:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1108;
  goto ret0;

  L1108:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 217;
  goto ret0;

  L1122:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1123;
  goto ret0;

  L1123:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 220;
  goto ret0;

  L1137:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1138;
  goto ret0;

  L1138:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 223;
  goto ret0;

  L1152:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1153;
  goto ret0;

  L1153:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 226;
  goto ret0;

  L1166:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 229;
    }
  goto ret0;

  L994:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != QImode)
    goto ret0;
  switch (GET_CODE (x1))
    {
    case ABS:
      goto L995;
    case EQ:
      goto L1022;
    case NE:
      goto L1037;
    case GT:
      goto L1052;
    case GTU:
      goto L1067;
    case LT:
      goto L1082;
    case LTU:
      goto L1097;
    case GE:
      goto L1112;
    case GEU:
      goto L1127;
    case LE:
      goto L1142;
    case LEU:
      goto L1157;
    case FFS:
      goto L1162;
    }
  goto ret0;

  L995:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 194;
    }
  goto ret0;

  L1022:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1023;
  goto ret0;

  L1023:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 200;
  goto ret0;

  L1037:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1038;
  goto ret0;

  L1038:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 203;
  goto ret0;

  L1052:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1053;
  goto ret0;

  L1053:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 206;
  goto ret0;

  L1067:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1068;
  goto ret0;

  L1068:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 209;
  goto ret0;

  L1082:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1083;
  goto ret0;

  L1083:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 212;
  goto ret0;

  L1097:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1098;
  goto ret0;

  L1098:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 215;
  goto ret0;

  L1112:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1113;
  goto ret0;

  L1113:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 218;
  goto ret0;

  L1127:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1128;
  goto ret0;

  L1128:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 221;
  goto ret0;

  L1142:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1143;
  goto ret0;

  L1143:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 224;
  goto ret0;

  L1157:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1158;
  goto ret0;

  L1158:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 227;
  goto ret0;

  L1162:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 228;
    }
  goto ret0;

  L999:
  x1 = XEXP (x0, 1);
  if (register_operand (x1, SImode))
    {
      ro[0] = x1;
      return 196;
    }
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
  register rtx x1, x2, x3, x4;
  int tem;

  L0:
  switch (GET_CODE (x0))
    {
    case SET:
      goto L41;
    case PARALLEL:
      if (XVECLEN (x0, 0) == 5 && 1)
	goto L73;
      if (XVECLEN (x0, 0) == 2 && 1)
	goto L85;
      break;
    case CALL:
      goto L966;
    case UNSPEC_VOLATILE:
      if (XINT (x0, 1) == 0 && XVECLEN (x0, 0) == 1 && 1)
	goto L974;
      break;
    case RETURN:
      if (0)
	return 189;
      break;
    case CONST_INT:
      if (XWINT (x0, 0) == 0 && 1)
	return 195;
    }
  goto ret0;
 L41:
  return recog_5 (x0, insn, pnum_clobbers);

  L73:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == SET && 1)
    goto L74;
  goto ret0;

  L74:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == BLKmode && general_operand (x2, BLKmode))
    {
      ro[0] = x2;
      goto L75;
    }
  goto ret0;

  L75:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, BLKmode))
    {
      ro[1] = x2;
      goto L76;
    }
  goto ret0;

  L76:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L77;
  goto ret0;

  L77:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L78;
    }
  goto ret0;

  L78:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L79;
  goto ret0;

  L79:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 0 && 1)
    goto L80;
  goto ret0;

  L80:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L81;
  goto ret0;

  L81:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 1 && 1)
    goto L82;
  goto ret0;

  L82:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L83;
  goto ret0;

  L83:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 2 && 1)
    return 21;
  goto ret0;

  L85:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == SET && 1)
    goto L86;
  goto ret0;

  L86:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == BLKmode && general_operand (x2, BLKmode))
    {
      ro[0] = x2;
      goto L87;
    }
  if (GET_CODE (x2) == PC && 1)
    goto L1003;
  goto ret0;

  L87:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, BLKmode))
    {
      ro[1] = x2;
      goto L88;
    }
  goto ret0;

  L88:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L89;
  goto ret0;

  L89:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && general_operand (x2, SImode))
    {
      ro[2] = x2;
      *pnum_clobbers = 3;
      return 21;
    }
  goto ret0;

  L1003:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case PLUS:
      if (GET_MODE (x2) == SImode && 1)
	goto L1004;
      break;
    case IF_THEN_ELSE:
      goto L939;
    }
  goto ret0;

  L1004:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == PC && 1)
    goto L1005;
  goto ret0;

  L1005:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L1006;
    }
  goto ret0;

  L1006:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L1007;
  goto ret0;

  L1007:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1008;
  goto ret0;

  L1008:
  x3 = XEXP (x2, 0);
  ro[1] = x3;
  return 197;

  L939:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == NE && 1)
    goto L940;
  goto ret0;

  L940:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) == SImode && general_operand (x4, SImode))
    {
      ro[0] = x4;
      goto L941;
    }
  goto ret0;

  L941:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && 1)
    {
      ro[1] = x4;
      goto L942;
    }
  goto ret0;

  L942:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == LABEL_REF && 1)
    goto L943;
  goto ret0;

  L943:
  x4 = XEXP (x3, 0);
  ro[2] = x4;
  goto L944;

  L944:
  x3 = XEXP (x2, 2);
  if (GET_CODE (x3) == PC && 1)
    goto L945;
  goto ret0;

  L945:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L946;
  goto ret0;

  L946:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L947;
  goto ret0;

  L947:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != SImode)
    goto ret0;
  switch (GET_CODE (x2))
    {
    case MINUS:
      goto L948;
    case PLUS:
      goto L963;
    }
  goto ret0;

  L948:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[0]) && 1)
    goto L949;
  goto ret0;

  L949:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[1]) && 1)
    if (INTVAL (operands[1]) > -8 && INTVAL (operands[1]) <= 8)
      return 183;
  goto ret0;

  L963:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[0]) && 1)
    goto L964;
  goto ret0;

  L964:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && 1)
    {
      ro[3] = x3;
      if (INTVAL (operands[1]) == - INTVAL (operands[3])
   && INTVAL (operands[3]) >= -8 && INTVAL (operands[3]) < 8)
	return 184;
      }
  goto ret0;

  L966:
  x1 = XEXP (x0, 0);
  if (memory_operand (x1, QImode))
    {
      ro[0] = x1;
      goto L967;
    }
  goto ret0;

  L967:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, QImode))
    {
      ro[1] = x1;
      return 185;
    }
  goto ret0;

  L974:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == CONST_INT && XWINT (x1, 0) == 0 && 1)
    return 188;
  goto ret0;
 ret0: return -1;
}

rtx
split_insns (x0, insn)
     register rtx x0;
     rtx insn;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4;
  rtx tem;

  goto ret0;
 ret0: return 0;
}


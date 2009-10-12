/*    pp_hot.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
 *    2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * Then he heard Merry change the note, and up went the Horn-cry of Buckland,
 * shaking the air.
 *
 *                  Awake!  Awake!  Fear, Fire, Foes!  Awake!
 *                               Fire, Foes!  Awake!
 *
 *     [p.1007 of _The Lord of the Rings_, VI/viii: "The Scouring of the Shire"]
 */

/* This file contains 'hot' pp ("push/pop") functions that
 * execute the opcodes that make up a perl program. A typical pp function
 * expects to find its arguments on the stack, and usually pushes its
 * results onto the stack, hence the 'pp' terminology. Each OP structure
 * contains a pointer to the relevant pp_foo() function.
 *
 * By 'hot', we mean common ops whose execution speed is critical.
 * By gathering them together into a single file, we encourage
 * CPU cache hits on hot code. Also it could be taken as a warning not to
 * change any code in this file unless you're sure it won't affect
 * performance.
 */

#include "EXTERN.h"
#define PERL_IN_PP_HOT_C
#include "perl.h"

/* Hot code. */

PP(pp_const)
{
    dVAR;
    dSP;
    if ( PL_op->op_flags & OPf_SPECIAL )
        /* This is a const op added to hold the hints hash for
           pp_entereval. The hash can be modified by the code
           being eval'ed, so we return a copy instead. */
        mXPUSHs((SV*)Perl_hv_copy_hints_hv(aTHX_ (HV*)cSVOP_sv));
    else
        /* Normal const. */
        XPUSHs(cSVOP_sv);
    RETURN;
}

PP(pp_nextstate)
{
    dVAR;
    PL_curcop = (COP*)PL_op;
    TAINT_NOT;		/* Each statement is presumed innocent */
    PL_stack_sp = PL_stack_base + cxstack[cxstack_ix].blk_oldsp;
    FREETMPS;
    return NORMAL;
}

PP(pp_gvsv)
{
    dVAR;
    dSP;
    EXTEND(SP,1);
    if (PL_op->op_private & OPpLVAL_INTRO)
	PUSHs(save_scalar(cGVOP_gv));
    else
	PUSHs(GvSVn(cGVOP_gv));
    RETURN;
}

PP(pp_null)
{
    dVAR;
    return NORMAL;
}

PP(pp_setstate)
{
    dVAR;
    PL_curcop = (COP*)PL_op;
    return NORMAL;
}

PP(pp_pushmark)
{
    dVAR;
    PUSHMARK(PL_stack_sp);
    return NORMAL;
}

PP(pp_stringify)
{
    dVAR; dSP; dTARGET;
    sv_copypv(TARG,TOPs);
    SETTARG;
    RETURN;
}

PP(pp_gv)
{
    dVAR; dSP;
    XPUSHs(MUTABLE_SV(cGVOP_gv));
    RETURN;
}

PP(pp_and)
{
    dVAR; dSP;
    if (!SvTRUE(TOPs))
	RETURN;
    else {
        if (PL_op->op_type == OP_AND)
	    --SP;
	RETURNOP(cLOGOP->op_other);
    }
}

PP(pp_sassign)
{
    dVAR; dSP; dPOPTOPssrl;

    if (PL_op->op_private & OPpASSIGN_BACKWARDS) {
	SV * const temp = left;
	left = right; right = temp;
    }
    if (PL_tainting && PL_tainted && !SvTAINTED(left))
	TAINT_NOT;
    if (PL_op->op_private & OPpASSIGN_CV_TO_GV) {
	SV * const cv = SvRV(left);
	const U32 cv_type = SvTYPE(cv);
	const U32 gv_type = SvTYPE(right);
	const bool got_coderef = cv_type == SVt_PVCV || cv_type == SVt_PVFM;

	if (!got_coderef) {
	    assert(SvROK(cv));
	}

	/* Can do the optimisation if right (LVALUE) is not a typeglob,
	   left (RVALUE) is a reference to something, and we're in void
	   context. */
	if (!got_coderef && gv_type != SVt_PVGV && GIMME_V == G_VOID) {
	    /* Is the target symbol table currently empty?  */
	    GV * const gv = gv_fetchsv(right, GV_NOINIT, SVt_PVGV);
	    if (SvTYPE(gv) != SVt_PVGV && !SvOK(gv)) {
		/* Good. Create a new proxy constant subroutine in the target.
		   The gv becomes a(nother) reference to the constant.  */
		SV *const value = SvRV(cv);

		SvUPGRADE(MUTABLE_SV(gv), SVt_RV);
		SvPCS_IMPORTED_on(gv);
		SvRV_set(gv, value);
		SvREFCNT_inc_simple_void(value);
		SETs(right);
		RETURN;
	    }
	}

	/* Need to fix things up.  */
	if (gv_type != SVt_PVGV) {
	    /* Need to fix GV.  */
	    right = MUTABLE_SV(gv_fetchsv(right, GV_ADD, SVt_PVGV));
	}

	if (!got_coderef) {
	    /* We've been returned a constant rather than a full subroutine,
	       but they expect a subroutine reference to apply.  */
	    if (SvROK(cv)) {
		ENTER;
		SvREFCNT_inc_void(SvRV(cv));
		/* newCONSTSUB takes a reference count on the passed in SV
		   from us.  We set the name to NULL, otherwise we get into
		   all sorts of fun as the reference to our new sub is
		   donated to the GV that we're about to assign to.
		*/
		SvRV_set(left, MUTABLE_SV(newCONSTSUB(GvSTASH(right), NULL,
						      SvRV(cv))));
		SvREFCNT_dec(cv);
		LEAVE;
	    } else {
		/* What can happen for the corner case *{"BONK"} = \&{"BONK"};
		   is that
		   First:   ops for \&{"BONK"}; return us the constant in the
			    symbol table
		   Second:  ops for *{"BONK"} cause that symbol table entry
			    (and our reference to it) to be upgraded from RV
			    to typeblob)
		   Thirdly: We get here. cv is actually PVGV now, and its
			    GvCV() is actually the subroutine we're looking for

		   So change the reference so that it points to the subroutine
		   of that typeglob, as that's what they were after all along.
		*/
		GV *const upgraded = MUTABLE_GV(cv);
		CV *const source = GvCV(upgraded);

		assert(source);
		assert(CvFLAGS(source) & CVf_CONST);

		SvREFCNT_inc_void(source);
		SvREFCNT_dec(upgraded);
		SvRV_set(left, MUTABLE_SV(source));
	    }
	}

    }
    SvSetMagicSV(right, left);
    SETs(right);
    RETURN;
}

PP(pp_cond_expr)
{
    dVAR; dSP;
    if (SvTRUEx(POPs))
	RETURNOP(cLOGOP->op_other);
    else
	RETURNOP(cLOGOP->op_next);
}

PP(pp_unstack)
{
    dVAR;
    I32 oldsave;
    TAINT_NOT;		/* Each statement is presumed innocent */
    PL_stack_sp = PL_stack_base + cxstack[cxstack_ix].blk_oldsp;
    FREETMPS;
    oldsave = PL_scopestack[PL_scopestack_ix - 1];
    LEAVE_SCOPE(oldsave);
    return NORMAL;
}

PP(pp_concat)
{
  dVAR; dSP; dATARGET; tryAMAGICbin(concat,opASSIGN);
  {
    dPOPTOPssrl;
    bool lbyte;
    STRLEN rlen;
    const char *rpv = NULL;
    bool rbyte = FALSE;
    bool rcopied = FALSE;

    if (TARG == right && right != left) {
	/* mg_get(right) may happen here ... */
	rpv = SvPV_const(right, rlen);
	rbyte = !DO_UTF8(right);
	right = newSVpvn_flags(rpv, rlen, SVs_TEMP);
	rpv = SvPV_const(right, rlen);	/* no point setting UTF-8 here */
	rcopied = TRUE;
    }

    if (TARG != left) {
        STRLEN llen;
        const char* const lpv = SvPV_const(left, llen);	/* mg_get(left) may happen here */
	lbyte = !DO_UTF8(left);
	sv_setpvn(TARG, lpv, llen);
	if (!lbyte)
	    SvUTF8_on(TARG);
	else
	    SvUTF8_off(TARG);
    }
    else { /* TARG == left */
        STRLEN llen;
	SvGETMAGIC(left);		/* or mg_get(left) may happen here */
	if (!SvOK(TARG)) {
	    if (left == right && ckWARN(WARN_UNINITIALIZED))
		report_uninit(right);
	    sv_setpvs(left, "");
	}
	(void)SvPV_nomg_const(left, llen);    /* Needed to set UTF8 flag */
	lbyte = !DO_UTF8(left);
	if (IN_BYTES)
	    SvUTF8_off(TARG);
    }

    /* or mg_get(right) may happen here */
    if (!rcopied) {
	rpv = SvPV_const(right, rlen);
	rbyte = !DO_UTF8(right);
    }
    if (lbyte != rbyte) {
	if (lbyte)
	    sv_utf8_upgrade_nomg(TARG);
	else {
	    if (!rcopied)
		right = newSVpvn_flags(rpv, rlen, SVs_TEMP);
	    sv_utf8_upgrade_nomg(right);
	    rpv = SvPV_const(right, rlen);
	}
    }
    sv_catpvn_nomg(TARG, rpv, rlen);

    SETTARG;
    RETURN;
  }
}

PP(pp_padsv)
{
    dVAR; dSP; dTARGET;
    XPUSHs(TARG);
    if (PL_op->op_flags & OPf_MOD) {
	if (PL_op->op_private & OPpLVAL_INTRO)
	    if (!(PL_op->op_private & OPpPAD_STATE))
		SAVECLEARSV(PAD_SVl(PL_op->op_targ));
        if (PL_op->op_private & OPpDEREF) {
	    PUTBACK;
	    vivify_ref(PAD_SVl(PL_op->op_targ), PL_op->op_private & OPpDEREF);
	    SPAGAIN;
	}
    }
    RETURN;
}

PP(pp_readline)
{
    dVAR;
    tryAMAGICunTARGET(iter, 0);
    PL_last_in_gv = MUTABLE_GV(*PL_stack_sp--);
    if (!isGV_with_GP(PL_last_in_gv)) {
	if (SvROK(PL_last_in_gv) && isGV_with_GP(SvRV(PL_last_in_gv)))
	    PL_last_in_gv = MUTABLE_GV(SvRV(PL_last_in_gv));
	else {
	    dSP;
	    XPUSHs(MUTABLE_SV(PL_last_in_gv));
	    PUTBACK;
	    pp_rv2gv();
	    PL_last_in_gv = MUTABLE_GV(*PL_stack_sp--);
	}
    }
    return do_readline();
}

PP(pp_eq)
{
    dVAR; dSP; tryAMAGICbinSET(eq,0);
#ifndef NV_PRESERVES_UV
    if (SvROK(TOPs) && !SvAMAGIC(TOPs) && SvROK(TOPm1s) && !SvAMAGIC(TOPm1s)) {
        SP--;
	SETs(boolSV(SvRV(TOPs) == SvRV(TOPp1s)));
	RETURN;
    }
#endif
#ifdef PERL_PRESERVE_IVUV
    SvIV_please(TOPs);
    if (SvIOK(TOPs)) {
	/* Unless the left argument is integer in range we are going
	   to have to use NV maths. Hence only attempt to coerce the
	   right argument if we know the left is integer.  */
      SvIV_please(TOPm1s);
	if (SvIOK(TOPm1s)) {
	    const bool auvok = SvUOK(TOPm1s);
	    const bool buvok = SvUOK(TOPs);
	
	    if (auvok == buvok) { /* ## IV == IV or UV == UV ## */
                /* Casting IV to UV before comparison isn't going to matter
                   on 2s complement. On 1s complement or sign&magnitude
                   (if we have any of them) it could to make negative zero
                   differ from normal zero. As I understand it. (Need to
                   check - is negative zero implementation defined behaviour
                   anyway?). NWC  */
		const UV buv = SvUVX(POPs);
		const UV auv = SvUVX(TOPs);
		
		SETs(boolSV(auv == buv));
		RETURN;
	    }
	    {			/* ## Mixed IV,UV ## */
                SV *ivp, *uvp;
		IV iv;
		
		/* == is commutative so doesn't matter which is left or right */
		if (auvok) {
		    /* top of stack (b) is the iv */
                    ivp = *SP;
                    uvp = *--SP;
                } else {
                    uvp = *SP;
                    ivp = *--SP;
                }
                iv = SvIVX(ivp);
		if (iv < 0)
                    /* As uv is a UV, it's >0, so it cannot be == */
                    SETs(&PL_sv_no);
		else
		    /* we know iv is >= 0 */
		    SETs(boolSV((UV)iv == SvUVX(uvp)));
		RETURN;
	    }
	}
    }
#endif
    {
#if defined(NAN_COMPARE_BROKEN) && defined(Perl_isnan)
      dPOPTOPnnrl;
      if (Perl_isnan(left) || Perl_isnan(right))
	  RETSETNO;
      SETs(boolSV(left == right));
#else
      dPOPnv;
      SETs(boolSV(TOPn == value));
#endif
      RETURN;
    }
}

PP(pp_preinc)
{
    dVAR; dSP;
    if (SvTYPE(TOPs) >= SVt_PVAV || isGV_with_GP(TOPs))
	DIE(aTHX_ "%s", PL_no_modify);
    if (!SvREADONLY(TOPs) && SvIOK_notUV(TOPs) && !SvNOK(TOPs) && !SvPOK(TOPs)
        && SvIVX(TOPs) != IV_MAX)
    {
	SvIV_set(TOPs, SvIVX(TOPs) + 1);
	SvFLAGS(TOPs) &= ~(SVp_NOK|SVp_POK);
    }
    else /* Do all the PERL_PRESERVE_IVUV conditionals in sv_inc */
	sv_inc(TOPs);
    SvSETMAGIC(TOPs);
    return NORMAL;
}

PP(pp_or)
{
    dVAR; dSP;
    if (SvTRUE(TOPs))
	RETURN;
    else {
	if (PL_op->op_type == OP_OR)
            --SP;
	RETURNOP(cLOGOP->op_other);
    }
}

PP(pp_defined)
{
    dVAR; dSP;
    register SV* sv;
    bool defined;
    const int op_type = PL_op->op_type;
    const bool is_dor = (op_type == OP_DOR || op_type == OP_DORASSIGN);

    if (is_dor) {
        sv = TOPs;
        if (!sv || !SvANY(sv)) {
	    if (op_type == OP_DOR)
		--SP;
            RETURNOP(cLOGOP->op_other);
        }
    }
    else {
	/* OP_DEFINED */
        sv = POPs;
        if (!sv || !SvANY(sv))
            RETPUSHNO;
    }

    defined = FALSE;
    switch (SvTYPE(sv)) {
    case SVt_PVAV:
	if (AvMAX(sv) >= 0 || SvGMAGICAL(sv) || (SvRMAGICAL(sv) && mg_find(sv, PERL_MAGIC_tied)))
	    defined = TRUE;
	break;
    case SVt_PVHV:
	if (HvARRAY(sv) || SvGMAGICAL(sv) || (SvRMAGICAL(sv) && mg_find(sv, PERL_MAGIC_tied)))
	    defined = TRUE;
	break;
    case SVt_PVCV:
	if (CvROOT(sv) || CvXSUB(sv))
	    defined = TRUE;
	break;
    default:
	SvGETMAGIC(sv);
	if (SvOK(sv))
	    defined = TRUE;
	break;
    }

    if (is_dor) {
        if(defined) 
            RETURN; 
        if(op_type == OP_DOR)
            --SP;
        RETURNOP(cLOGOP->op_other);
    }
    /* assuming OP_DEFINED */
    if(defined) 
        RETPUSHYES;
    RETPUSHNO;
}

PP(pp_add)
{
    dVAR; dSP; dATARGET; bool useleft; SV *svl, *svr;
    tryAMAGICbin(add,opASSIGN);
    svl = sv_2num(TOPm1s);
    svr = sv_2num(TOPs);
    useleft = USE_LEFT(svl);
#ifdef PERL_PRESERVE_IVUV
    /* We must see if we can perform the addition with integers if possible,
       as the integer code detects overflow while the NV code doesn't.
       If either argument hasn't had a numeric conversion yet attempt to get
       the IV. It's important to do this now, rather than just assuming that
       it's not IOK as a PV of "9223372036854775806" may not take well to NV
       addition, and an SV which is NOK, NV=6.0 ought to be coerced to
       integer in case the second argument is IV=9223372036854775806
       We can (now) rely on sv_2iv to do the right thing, only setting the
       public IOK flag if the value in the NV (or PV) slot is truly integer.

       A side effect is that this also aggressively prefers integer maths over
       fp maths for integer values.

       How to detect overflow?

       C 99 section 6.2.6.1 says

       The range of nonnegative values of a signed integer type is a subrange
       of the corresponding unsigned integer type, and the representation of
       the same value in each type is the same. A computation involving
       unsigned operands can never overflow, because a result that cannot be
       represented by the resulting unsigned integer type is reduced modulo
       the number that is one greater than the largest value that can be
       represented by the resulting type.

       (the 9th paragraph)

       which I read as "unsigned ints wrap."

       signed integer overflow seems to be classed as "exception condition"

       If an exceptional condition occurs during the evaluation of an
       expression (that is, if the result is not mathematically defined or not
       in the range of representable values for its type), the behavior is
       undefined.

       (6.5, the 5th paragraph)

       I had assumed that on 2s complement machines signed arithmetic would
       wrap, hence coded pp_add and pp_subtract on the assumption that
       everything perl builds on would be happy.  After much wailing and
       gnashing of teeth it would seem that irix64 knows its ANSI spec well,
       knows that it doesn't need to, and doesn't.  Bah.  Anyway, the all-
       unsigned code below is actually shorter than the old code. :-)
    */

    SvIV_please(svr);
    if (SvIOK(svr)) {
	/* Unless the left argument is integer in range we are going to have to
	   use NV maths. Hence only attempt to coerce the right argument if
	   we know the left is integer.  */
	register UV auv = 0;
	bool auvok = FALSE;
	bool a_valid = 0;

	if (!useleft) {
	    auv = 0;
	    a_valid = auvok = 1;
	    /* left operand is undef, treat as zero. + 0 is identity,
	       Could SETi or SETu right now, but space optimise by not adding
	       lots of code to speed up what is probably a rarish case.  */
	} else {
	    /* Left operand is defined, so is it IV? */
	    SvIV_please(svl);
	    if (SvIOK(svl)) {
		if ((auvok = SvUOK(svl)))
		    auv = SvUVX(svl);
		else {
		    register const IV aiv = SvIVX(svl);
		    if (aiv >= 0) {
			auv = aiv;
			auvok = 1;	/* Now acting as a sign flag.  */
		    } else { /* 2s complement assumption for IV_MIN */
			auv = (UV)-aiv;
		    }
		}
		a_valid = 1;
	    }
	}
	if (a_valid) {
	    bool result_good = 0;
	    UV result;
	    register UV buv;
	    bool buvok = SvUOK(svr);
	
	    if (buvok)
		buv = SvUVX(svr);
	    else {
		register const IV biv = SvIVX(svr);
		if (biv >= 0) {
		    buv = biv;
		    buvok = 1;
		} else
		    buv = (UV)-biv;
	    }
	    /* ?uvok if value is >= 0. basically, flagged as UV if it's +ve,
	       else "IV" now, independent of how it came in.
	       if a, b represents positive, A, B negative, a maps to -A etc
	       a + b =>  (a + b)
	       A + b => -(a - b)
	       a + B =>  (a - b)
	       A + B => -(a + b)
	       all UV maths. negate result if A negative.
	       add if signs same, subtract if signs differ. */

	    if (auvok ^ buvok) {
		/* Signs differ.  */
		if (auv >= buv) {
		    result = auv - buv;
		    /* Must get smaller */
		    if (result <= auv)
			result_good = 1;
		} else {
		    result = buv - auv;
		    if (result <= buv) {
			/* result really should be -(auv-buv). as its negation
			   of true value, need to swap our result flag  */
			auvok = !auvok;
			result_good = 1;
		    }
		}
	    } else {
		/* Signs same */
		result = auv + buv;
		if (result >= auv)
		    result_good = 1;
	    }
	    if (result_good) {
		SP--;
		if (auvok)
		    SETu( result );
		else {
		    /* Negate result */
		    if (result <= (UV)IV_MIN)
			SETi( -(IV)result );
		    else {
			/* result valid, but out of range for IV.  */
			SETn( -(NV)result );
		    }
		}
		RETURN;
	    } /* Overflow, drop through to NVs.  */
	}
    }
#endif
    {
	NV value = SvNV(svr);
	(void)POPs;
	if (!useleft) {
	    /* left operand is undef, treat as zero. + 0.0 is identity. */
	    SETn(value);
	    RETURN;
	}
	SETn( value + SvNV(svl) );
	RETURN;
    }
}

PP(pp_aelemfast)
{
    dVAR; dSP;
    AV * const av = PL_op->op_flags & OPf_SPECIAL
	? MUTABLE_AV(PAD_SV(PL_op->op_targ)) : GvAV(cGVOP_gv);
    const U32 lval = PL_op->op_flags & OPf_MOD;
    SV** const svp = av_fetch(av, PL_op->op_private, lval);
    SV *sv = (svp ? *svp : &PL_sv_undef);
    EXTEND(SP, 1);
    if (!lval && SvGMAGICAL(sv))	/* see note in pp_helem() */
	sv = sv_mortalcopy(sv);
    PUSHs(sv);
    RETURN;
}

PP(pp_join)
{
    dVAR; dSP; dMARK; dTARGET;
    MARK++;
    do_join(TARG, *MARK, MARK, SP);
    SP = MARK;
    SETs(TARG);
    RETURN;
}

PP(pp_pushre)
{
    dVAR; dSP;
#ifdef DEBUGGING
    /*
     * We ass_u_me that LvTARGOFF() comes first, and that two STRLENs
     * will be enough to hold an OP*.
     */
    SV* const sv = sv_newmortal();
    sv_upgrade(sv, SVt_PVLV);
    LvTYPE(sv) = '/';
    Copy(&PL_op, &LvTARGOFF(sv), 1, OP*);
    XPUSHs(sv);
#else
    XPUSHs(MUTABLE_SV(PL_op));
#endif
    RETURN;
}

/* Oversized hot code. */

PP(pp_print)
{
    dVAR; dSP; dMARK; dORIGMARK;
    IO *io;
    register PerlIO *fp;
    MAGIC *mg;
    GV * const gv
	= (PL_op->op_flags & OPf_STACKED) ? MUTABLE_GV(*++MARK) : PL_defoutgv;

    if (gv && (io = GvIO(gv))
	&& (mg = SvTIED_mg((const SV *)io, PERL_MAGIC_tiedscalar)))
    {
      had_magic:
	if (MARK == ORIGMARK) {
	    /* If using default handle then we need to make space to
	     * pass object as 1st arg, so move other args up ...
	     */
	    MEXTEND(SP, 1);
	    ++MARK;
	    Move(MARK, MARK + 1, (SP - MARK) + 1, SV*);
	    ++SP;
	}
	PUSHMARK(MARK - 1);
	*MARK = SvTIED_obj(MUTABLE_SV(io), mg);
	PUTBACK;
	ENTER;
	if( PL_op->op_type == OP_SAY ) {
		/* local $\ = "\n" */
		SAVEGENERICSV(PL_ors_sv);
		PL_ors_sv = newSVpvs("\n");
	}
	call_method("PRINT", G_SCALAR);
	LEAVE;
	SPAGAIN;
	MARK = ORIGMARK + 1;
	*MARK = *SP;
	SP = MARK;
	RETURN;
    }
    if (!(io = GvIO(gv))) {
        if ((GvEGV(gv)) && (io = GvIO(GvEGV(gv)))
	    && (mg = SvTIED_mg((const SV *)io, PERL_MAGIC_tiedscalar)))
            goto had_magic;
	if (ckWARN2(WARN_UNOPENED,WARN_CLOSED))
	    report_evil_fh(gv, io, PL_op->op_type);
	SETERRNO(EBADF,RMS_IFI);
	goto just_say_no;
    }
    else if (!(fp = IoOFP(io))) {
	if (ckWARN2(WARN_CLOSED, WARN_IO))  {
	    if (IoIFP(io))
		report_evil_fh(gv, io, OP_phoney_INPUT_ONLY);
	    else if (ckWARN2(WARN_UNOPENED,WARN_CLOSED))
		report_evil_fh(gv, io, PL_op->op_type);
	}
	SETERRNO(EBADF,IoIFP(io)?RMS_FAC:RMS_IFI);
	goto just_say_no;
    }
    else {
	MARK++;
	if (PL_ofs_sv && SvOK(PL_ofs_sv)) {
	    while (MARK <= SP) {
		if (!do_print(*MARK, fp))
		    break;
		MARK++;
		if (MARK <= SP) {
		    if (!do_print(PL_ofs_sv, fp)) { /* $, */
			MARK--;
			break;
		    }
		}
	    }
	}
	else {
	    while (MARK <= SP) {
		if (!do_print(*MARK, fp))
		    break;
		MARK++;
	    }
	}
	if (MARK <= SP)
	    goto just_say_no;
	else {
	    if (PL_op->op_type == OP_SAY) {
		if (PerlIO_write(fp, "\n", 1) == 0 || PerlIO_error(fp))
		    goto just_say_no;
	    }
            else if (PL_ors_sv && SvOK(PL_ors_sv))
		if (!do_print(PL_ors_sv, fp)) /* $\ */
		    goto just_say_no;

	    if (IoFLAGS(io) & IOf_FLUSH)
		if (PerlIO_flush(fp) == EOF)
		    goto just_say_no;
	}
    }
    SP = ORIGMARK;
    XPUSHs(&PL_sv_yes);
    RETURN;

  just_say_no:
    SP = ORIGMARK;
    XPUSHs(&PL_sv_undef);
    RETURN;
}

PP(pp_rv2av)
{
    dVAR; dSP; dTOPss;
    const I32 gimme = GIMME_V;
    static const char an_array[] = "an ARRAY";
    static const char a_hash[] = "a HASH";
    const bool is_pp_rv2av = PL_op->op_type == OP_RV2AV;
    const svtype type = is_pp_rv2av ? SVt_PVAV : SVt_PVHV;

    if (SvROK(sv)) {
      wasref:
	tryAMAGICunDEREF_var(is_pp_rv2av ? to_av_amg : to_hv_amg);

	sv = SvRV(sv);
	if (SvTYPE(sv) != type)
	    DIE(aTHX_ "Not %s reference", is_pp_rv2av ? an_array : a_hash);
	if (PL_op->op_flags & OPf_REF) {
	    SETs(sv);
	    RETURN;
	}
	else if (LVRET) {
	    if (gimme != G_ARRAY)
		goto croak_cant_return;
	    SETs(sv);
	    RETURN;
	}
	else if (PL_op->op_flags & OPf_MOD
		&& PL_op->op_private & OPpLVAL_INTRO)
	    Perl_croak(aTHX_ "%s", PL_no_localize_ref);
    }
    else {
	if (SvTYPE(sv) == type) {
	    if (PL_op->op_flags & OPf_REF) {
		SETs(sv);
		RETURN;
	    }
	    else if (LVRET) {
		if (gimme != G_ARRAY)
		    goto croak_cant_return;
		SETs(sv);
		RETURN;
	    }
	}
	else {
	    GV *gv;
	
	    if (!isGV_with_GP(sv)) {
		if (SvGMAGICAL(sv)) {
		    mg_get(sv);
		    if (SvROK(sv))
			goto wasref;
		}
		gv = Perl_softref2xv(aTHX_ sv, is_pp_rv2av ? an_array : a_hash,
				     type, &sp);
		if (!gv)
		    RETURN;
	    }
	    else {
		gv = MUTABLE_GV(sv);
	    }
	    sv = is_pp_rv2av ? MUTABLE_SV(GvAVn(gv)) : MUTABLE_SV(GvHVn(gv));
	    if (PL_op->op_private & OPpLVAL_INTRO)
		sv = is_pp_rv2av ? MUTABLE_SV(save_ary(gv)) : MUTABLE_SV(save_hash(gv));
	    if (PL_op->op_flags & OPf_REF) {
		SETs(sv);
		RETURN;
	    }
	    else if (LVRET) {
		if (gimme != G_ARRAY)
		    goto croak_cant_return;
		SETs(sv);
		RETURN;
	    }
	}
    }

    if (is_pp_rv2av) {
	AV *const av = MUTABLE_AV(sv);
	/* The guts of pp_rv2av, with no intenting change to preserve history
	   (until such time as we get tools that can do blame annotation across
	   whitespace changes.  */
    if (gimme == G_ARRAY) {
	const I32 maxarg = AvFILL(av) + 1;
	(void)POPs;			/* XXXX May be optimized away? */
	EXTEND(SP, maxarg);
	if (SvRMAGICAL(av)) {
	    U32 i;
	    for (i=0; i < (U32)maxarg; i++) {
		SV ** const svp = av_fetch(av, i, FALSE);
		/* See note in pp_helem, and bug id #27839 */
		SP[i+1] = svp
		    ? SvGMAGICAL(*svp) ? sv_mortalcopy(*svp) : *svp
		    : &PL_sv_undef;
	    }
	}
	else {
	    Copy(AvARRAY(av), SP+1, maxarg, SV*);
	}
	SP += maxarg;
    }
    else if (gimme == G_SCALAR) {
	dTARGET;
	const I32 maxarg = AvFILL(av) + 1;
	SETi(maxarg);
    }
    } else {
	/* The guts of pp_rv2hv  */
    if (gimme == G_ARRAY) { /* array wanted */
	*PL_stack_sp = sv;
	return do_kv();
    }
    else if (gimme == G_SCALAR) {
	dTARGET;
    TARG = Perl_hv_scalar(aTHX_ MUTABLE_HV(sv));
	SPAGAIN;
	SETTARG;
    }
    }
    RETURN;

 croak_cant_return:
    Perl_croak(aTHX_ "Can't return %s to lvalue scalar context",
	       is_pp_rv2av ? "array" : "hash");
    RETURN;
}

STATIC void
S_do_oddball(pTHX_ HV *hash, SV **relem, SV **firstrelem)
{
    dVAR;

    PERL_ARGS_ASSERT_DO_ODDBALL;

    if (*relem) {
	SV *tmpstr;
        const HE *didstore;

        if (ckWARN(WARN_MISC)) {
	    const char *err;
	    if (relem == firstrelem &&
		SvROK(*relem) &&
		(SvTYPE(SvRV(*relem)) == SVt_PVAV ||
		 SvTYPE(SvRV(*relem)) == SVt_PVHV))
	    {
		err = "Reference found where even-sized list expected";
	    }
	    else
		err = "Odd number of elements in hash assignment";
	    Perl_warner(aTHX_ packWARN(WARN_MISC), "%s", err);
	}

        tmpstr = newSV(0);
        didstore = hv_store_ent(hash,*relem,tmpstr,0);
        if (SvMAGICAL(hash)) {
            if (SvSMAGICAL(tmpstr))
                mg_set(tmpstr);
            if (!didstore)
                sv_2mortal(tmpstr);
        }
        TAINT_NOT;
    }
}

PP(pp_aassign)
{
    dVAR; dSP;
    SV **lastlelem = PL_stack_sp;
    SV **lastrelem = PL_stack_base + POPMARK;
    SV **firstrelem = PL_stack_base + POPMARK + 1;
    SV **firstlelem = lastrelem + 1;

    register SV **relem;
    register SV **lelem;

    register SV *sv;
    register AV *ary;

    I32 gimme;
    HV *hash;
    I32 i;
    int magic;
    int duplicates = 0;
    SV **firsthashrelem = NULL;	/* "= 0" keeps gcc 2.95 quiet  */

    PL_delaymagic = DM_DELAY;		/* catch simultaneous items */
    gimme = GIMME_V;

    /* If there's a common identifier on both sides we have to take
     * special care that assigning the identifier on the left doesn't
     * clobber a value on the right that's used later in the list.
     */
    if (PL_op->op_private & (OPpASSIGN_COMMON)) {
	EXTEND_MORTAL(lastrelem - firstrelem + 1);
	for (relem = firstrelem; relem <= lastrelem; relem++) {
	    if ((sv = *relem)) {
		TAINT_NOT;	/* Each item is independent */
		*relem = sv_mortalcopy(sv);
	    }
	}
    }

    relem = firstrelem;
    lelem = firstlelem;
    ary = NULL;
    hash = NULL;

    while (lelem <= lastlelem) {
	TAINT_NOT;		/* Each item stands on its own, taintwise. */
	sv = *lelem++;
	switch (SvTYPE(sv)) {
	case SVt_PVAV:
	    ary = MUTABLE_AV(sv);
	    magic = SvMAGICAL(ary) != 0;
	    av_clear(ary);
	    av_extend(ary, lastrelem - relem);
	    i = 0;
	    while (relem <= lastrelem) {	/* gobble up all the rest */
		SV **didstore;
		assert(*relem);
		sv = newSVsv(*relem);
		*(relem++) = sv;
		didstore = av_store(ary,i++,sv);
		if (magic) {
		    if (SvSMAGICAL(sv)) {
			/* More magic can happen in the mg_set callback, so we
			 * backup the delaymagic for now. */
			U16 dmbak = PL_delaymagic;
			PL_delaymagic = 0;
			mg_set(sv);
			PL_delaymagic = dmbak;
		    }
		    if (!didstore)
			sv_2mortal(sv);
		}
		TAINT_NOT;
	    }
	    if (PL_delaymagic & DM_ARRAY)
		SvSETMAGIC(MUTABLE_SV(ary));
	    break;
	case SVt_PVHV: {				/* normal hash */
		SV *tmpstr;

		hash = MUTABLE_HV(sv);
		magic = SvMAGICAL(hash) != 0;
		hv_clear(hash);
		firsthashrelem = relem;

		while (relem < lastrelem) {	/* gobble up all the rest */
		    HE *didstore;
		    sv = *relem ? *relem : &PL_sv_no;
		    relem++;
		    tmpstr = newSV(0);
		    if (*relem)
			sv_setsv(tmpstr,*relem);	/* value */
		    *(relem++) = tmpstr;
		    if (gimme != G_VOID && hv_exists_ent(hash, sv, 0))
			/* key overwrites an existing entry */
			duplicates += 2;
		    didstore = hv_store_ent(hash,sv,tmpstr,0);
		    if (magic) {
			if (SvSMAGICAL(tmpstr)) {
			    U16 dmbak = PL_delaymagic;
			    PL_delaymagic = 0;
			    mg_set(tmpstr);
			    PL_delaymagic = dmbak;
			}
			if (!didstore)
			    sv_2mortal(tmpstr);
		    }
		    TAINT_NOT;
		}
		if (relem == lastrelem) {
		    do_oddball(hash, relem, firstrelem);
		    relem++;
		}
	    }
	    break;
	default:
	    if (SvIMMORTAL(sv)) {
		if (relem <= lastrelem)
		    relem++;
		break;
	    }
	    if (relem <= lastrelem) {
		sv_setsv(sv, *relem);
		*(relem++) = sv;
	    }
	    else
		sv_setsv(sv, &PL_sv_undef);

	    if (SvSMAGICAL(sv)) {
		U16 dmbak = PL_delaymagic;
		PL_delaymagic = 0;
		mg_set(sv);
		PL_delaymagic = dmbak;
	    }
	    break;
	}
    }
    if (PL_delaymagic & ~DM_DELAY) {
	if (PL_delaymagic & DM_UID) {
#ifdef HAS_SETRESUID
	    (void)setresuid((PL_delaymagic & DM_RUID) ? PL_uid  : (Uid_t)-1,
			    (PL_delaymagic & DM_EUID) ? PL_euid : (Uid_t)-1,
			    (Uid_t)-1);
#else
#  ifdef HAS_SETREUID
	    (void)setreuid((PL_delaymagic & DM_RUID) ? PL_uid  : (Uid_t)-1,
			   (PL_delaymagic & DM_EUID) ? PL_euid : (Uid_t)-1);
#  else
#    ifdef HAS_SETRUID
	    if ((PL_delaymagic & DM_UID) == DM_RUID) {
		(void)setruid(PL_uid);
		PL_delaymagic &= ~DM_RUID;
	    }
#    endif /* HAS_SETRUID */
#    ifdef HAS_SETEUID
	    if ((PL_delaymagic & DM_UID) == DM_EUID) {
		(void)seteuid(PL_euid);
		PL_delaymagic &= ~DM_EUID;
	    }
#    endif /* HAS_SETEUID */
	    if (PL_delaymagic & DM_UID) {
		if (PL_uid != PL_euid)
		    DIE(aTHX_ "No setreuid available");
		(void)PerlProc_setuid(PL_uid);
	    }
#  endif /* HAS_SETREUID */
#endif /* HAS_SETRESUID */
	    PL_uid = PerlProc_getuid();
	    PL_euid = PerlProc_geteuid();
	}
	if (PL_delaymagic & DM_GID) {
#ifdef HAS_SETRESGID
	    (void)setresgid((PL_delaymagic & DM_RGID) ? PL_gid  : (Gid_t)-1,
			    (PL_delaymagic & DM_EGID) ? PL_egid : (Gid_t)-1,
			    (Gid_t)-1);
#else
#  ifdef HAS_SETREGID
	    (void)setregid((PL_delaymagic & DM_RGID) ? PL_gid  : (Gid_t)-1,
			   (PL_delaymagic & DM_EGID) ? PL_egid : (Gid_t)-1);
#  else
#    ifdef HAS_SETRGID
	    if ((PL_delaymagic & DM_GID) == DM_RGID) {
		(void)setrgid(PL_gid);
		PL_delaymagic &= ~DM_RGID;
	    }
#    endif /* HAS_SETRGID */
#    ifdef HAS_SETEGID
	    if ((PL_delaymagic & DM_GID) == DM_EGID) {
		(void)setegid(PL_egid);
		PL_delaymagic &= ~DM_EGID;
	    }
#    endif /* HAS_SETEGID */
	    if (PL_delaymagic & DM_GID) {
		if (PL_gid != PL_egid)
		    DIE(aTHX_ "No setregid available");
		(void)PerlProc_setgid(PL_gid);
	    }
#  endif /* HAS_SETREGID */
#endif /* HAS_SETRESGID */
	    PL_gid = PerlProc_getgid();
	    PL_egid = PerlProc_getegid();
	}
	PL_tainting |= (PL_uid && (PL_euid != PL_uid || PL_egid != PL_gid));
    }
    PL_delaymagic = 0;

    if (gimme == G_VOID)
	SP = firstrelem - 1;
    else if (gimme == G_SCALAR) {
	dTARGET;
	SP = firstrelem;
	SETi(lastrelem - firstrelem + 1 - duplicates);
    }
    else {
	if (ary)
	    SP = lastrelem;
	else if (hash) {
	    if (duplicates) {
		/* Removes from the stack the entries which ended up as
		 * duplicated keys in the hash (fix for [perl #24380]) */
		Move(firsthashrelem + duplicates,
			firsthashrelem, duplicates, SV**);
		lastrelem -= duplicates;
	    }
	    SP = lastrelem;
	}
	else
	    SP = firstrelem + (lastlelem - firstlelem);
	lelem = firstlelem + (relem - firstrelem);
	while (relem <= SP)
	    *relem++ = (lelem <= lastlelem) ? *lelem++ : &PL_sv_undef;
    }

    RETURN;
}

PP(pp_qr)
{
    dVAR; dSP;
    register PMOP * const pm = cPMOP;
    REGEXP * rx = PM_GETRE(pm);
    SV * const pkg = CALLREG_PACKAGE(rx);
    SV * const rv = sv_newmortal();
    SV * const sv = newSVrv(rv, SvPV_nolen(pkg));
    SvREFCNT_dec(pkg);
    if (RX_EXTFLAGS(rx) & RXf_TAINTED)
        SvTAINTED_on(rv);
    sv_magic(sv,(SV*)ReREFCNT_inc(rx), PERL_MAGIC_qr,0,0);
    XPUSHs(rv);
    RETURN;
}

PP(pp_match)
{
    dVAR; dSP; dTARG;
    register PMOP *pm = cPMOP;
    PMOP *dynpm = pm;
    register const char *t;
    register const char *s;
    const char *strend;
    I32 global;
    I32 r_flags = REXEC_CHECKED;
    const char *truebase;			/* Start of string  */
    register REGEXP *rx = PM_GETRE(pm);
    bool rxtainted;
    const I32 gimme = GIMME;
    STRLEN len;
    I32 minmatch = 0;
    const I32 oldsave = PL_savestack_ix;
    I32 update_minmatch = 1;
    I32 had_zerolen = 0;
    U32 gpos = 0;

    if (PL_op->op_flags & OPf_STACKED)
	TARG = POPs;
    else if (PL_op->op_private & OPpTARGET_MY)
	GETTARGET;
    else {
	TARG = DEFSV;
	EXTEND(SP,1);
    }

    PUTBACK;				/* EVAL blocks need stack_sp. */
    s = SvPV_const(TARG, len);
    if (!s)
	DIE(aTHX_ "panic: pp_match");
    strend = s + len;
    rxtainted = ((RX_EXTFLAGS(rx) & RXf_TAINTED) ||
		 (PL_tainted && (pm->op_pmflags & PMf_RETAINT)));
    TAINT_NOT;

    RX_MATCH_UTF8_set(rx, DO_UTF8(TARG));

    /* PMdf_USED is set after a ?? matches once */
    if (
#ifdef USE_ITHREADS
        SvREADONLY(PL_regex_pad[pm->op_pmoffset])
#else
        pm->op_pmflags & PMf_USED
#endif
    ) {
      failure:
	if (gimme == G_ARRAY)
	    RETURN;
	RETPUSHNO;
    }



    /* empty pattern special-cased to use last successful pattern if possible */
    if (!RX_PRELEN(rx) && PL_curpm) {
	pm = PL_curpm;
	rx = PM_GETRE(pm);
    }

    if (RX_MINLEN(rx) > (I32)len)
	goto failure;

    truebase = t = s;

    /* XXXX What part of this is needed with true \G-support? */
    if ((global = dynpm->op_pmflags & PMf_GLOBAL)) {
	RX_OFFS(rx)[0].start = -1;
	if (SvTYPE(TARG) >= SVt_PVMG && SvMAGIC(TARG)) {
	    MAGIC* const mg = mg_find(TARG, PERL_MAGIC_regex_global);
	    if (mg && mg->mg_len >= 0) {
		if (!(RX_EXTFLAGS(rx) & RXf_GPOS_SEEN))
		    RX_OFFS(rx)[0].end = RX_OFFS(rx)[0].start = mg->mg_len;
		else if (RX_EXTFLAGS(rx) & RXf_ANCH_GPOS) {
		    r_flags |= REXEC_IGNOREPOS;
		    RX_OFFS(rx)[0].end = RX_OFFS(rx)[0].start = mg->mg_len;
		} else if (RX_EXTFLAGS(rx) & RXf_GPOS_FLOAT) 
		    gpos = mg->mg_len;
		else 
		    RX_OFFS(rx)[0].end = RX_OFFS(rx)[0].start = mg->mg_len;
		minmatch = (mg->mg_flags & MGf_MINMATCH) ? RX_GOFS(rx) + 1 : 0;
		update_minmatch = 0;
	    }
	}
    }
    /* XXX: comment out !global get safe $1 vars after a
       match, BUT be aware that this leads to dramatic slowdowns on
       /g matches against large strings.  So far a solution to this problem
       appears to be quite tricky.
       Test for the unsafe vars are TODO for now. */
    if ((  !global && RX_NPARENS(rx)) 
	    || SvTEMP(TARG) || PL_sawampersand ||
	    (RX_EXTFLAGS(rx) & (RXf_EVAL_SEEN|RXf_PMf_KEEPCOPY)))
	r_flags |= REXEC_COPY_STR;
    if (SvSCREAM(TARG))
	r_flags |= REXEC_SCREAM;

play_it_again:
    if (global && RX_OFFS(rx)[0].start != -1) {
	t = s = RX_OFFS(rx)[0].end + truebase - RX_GOFS(rx);
	if ((s + RX_MINLEN(rx)) > strend || s < truebase)
	    goto nope;
	if (update_minmatch++)
	    minmatch = had_zerolen;
    }
    if (RX_EXTFLAGS(rx) & RXf_USE_INTUIT &&
	DO_UTF8(TARG) == (RX_UTF8(rx) != 0)) {
	/* FIXME - can PL_bostr be made const char *?  */
	PL_bostr = (char *)truebase;
	s = CALLREG_INTUIT_START(rx, TARG, (char *)s, (char *)strend, r_flags, NULL);

	if (!s)
	    goto nope;
	if ( (RX_EXTFLAGS(rx) & RXf_CHECK_ALL)
	     && !PL_sawampersand
	     && !(RX_EXTFLAGS(rx) & RXf_PMf_KEEPCOPY)
	     && ((RX_EXTFLAGS(rx) & RXf_NOSCAN)
		 || !((RX_EXTFLAGS(rx) & RXf_INTUIT_TAIL)
		      && (r_flags & REXEC_SCREAM)))
	     && !SvROK(TARG))	/* Cannot trust since INTUIT cannot guess ^ */
	    goto yup;
    }
    if (CALLREGEXEC(rx, (char*)s, (char *)strend, (char*)truebase,
                    minmatch, TARG, NUM2PTR(void*, gpos), r_flags))
    {
	PL_curpm = pm;
	if (dynpm->op_pmflags & PMf_ONCE) {
#ifdef USE_ITHREADS
            SvREADONLY_on(PL_regex_pad[dynpm->op_pmoffset]);
#else
	    dynpm->op_pmflags |= PMf_USED;
#endif
        }
	goto gotcha;
    }
    else
	goto ret_no;
    /*NOTREACHED*/

  gotcha:
    if (rxtainted)
	RX_MATCH_TAINTED_on(rx);
    TAINT_IF(RX_MATCH_TAINTED(rx));
    if (gimme == G_ARRAY) {
	const I32 nparens = RX_NPARENS(rx);
	I32 i = (global && !nparens) ? 1 : 0;

	SPAGAIN;			/* EVAL blocks could move the stack. */
	EXTEND(SP, nparens + i);
	EXTEND_MORTAL(nparens + i);
	for (i = !i; i <= nparens; i++) {
	    PUSHs(sv_newmortal());
	    if ((RX_OFFS(rx)[i].start != -1) && RX_OFFS(rx)[i].end != -1 ) {
		const I32 len = RX_OFFS(rx)[i].end - RX_OFFS(rx)[i].start;
		s = RX_OFFS(rx)[i].start + truebase;
	        if (RX_OFFS(rx)[i].end < 0 || RX_OFFS(rx)[i].start < 0 ||
		    len < 0 || len > strend - s)
		    DIE(aTHX_ "panic: pp_match start/end pointers");
		sv_setpvn(*SP, s, len);
		if (DO_UTF8(TARG) && is_utf8_string((U8*)s, len))
		    SvUTF8_on(*SP);
	    }
	}
	if (global) {
	    if (dynpm->op_pmflags & PMf_CONTINUE) {
		MAGIC* mg = NULL;
		if (SvTYPE(TARG) >= SVt_PVMG && SvMAGIC(TARG))
		    mg = mg_find(TARG, PERL_MAGIC_regex_global);
		if (!mg) {
#ifdef PERL_OLD_COPY_ON_WRITE
		    if (SvIsCOW(TARG))
			sv_force_normal_flags(TARG, 0);
#endif
		    mg = sv_magicext(TARG, NULL, PERL_MAGIC_regex_global,
				     &PL_vtbl_mglob, NULL, 0);
		}
		if (RX_OFFS(rx)[0].start != -1) {
		    mg->mg_len = RX_OFFS(rx)[0].end;
		    if (RX_OFFS(rx)[0].start + RX_GOFS(rx) == (UV)RX_OFFS(rx)[0].end)
			mg->mg_flags |= MGf_MINMATCH;
		    else
			mg->mg_flags &= ~MGf_MINMATCH;
		}
	    }
	    had_zerolen = (RX_OFFS(rx)[0].start != -1
			   && (RX_OFFS(rx)[0].start + RX_GOFS(rx)
			       == (UV)RX_OFFS(rx)[0].end));
	    PUTBACK;			/* EVAL blocks may use stack */
	    r_flags |= REXEC_IGNOREPOS | REXEC_NOT_FIRST;
	    goto play_it_again;
	}
	else if (!nparens)
	    XPUSHs(&PL_sv_yes);
	LEAVE_SCOPE(oldsave);
	RETURN;
    }
    else {
	if (global) {
	    MAGIC* mg;
	    if (SvTYPE(TARG) >= SVt_PVMG && SvMAGIC(TARG))
		mg = mg_find(TARG, PERL_MAGIC_regex_global);
	    else
		mg = NULL;
	    if (!mg) {
#ifdef PERL_OLD_COPY_ON_WRITE
		if (SvIsCOW(TARG))
		    sv_force_normal_flags(TARG, 0);
#endif
		mg = sv_magicext(TARG, NULL, PERL_MAGIC_regex_global,
				 &PL_vtbl_mglob, NULL, 0);
	    }
	    if (RX_OFFS(rx)[0].start != -1) {
		mg->mg_len = RX_OFFS(rx)[0].end;
		if (RX_OFFS(rx)[0].start + RX_GOFS(rx) == (UV)RX_OFFS(rx)[0].end)
		    mg->mg_flags |= MGf_MINMATCH;
		else
		    mg->mg_flags &= ~MGf_MINMATCH;
	    }
	}
	LEAVE_SCOPE(oldsave);
	RETPUSHYES;
    }

yup:					/* Confirmed by INTUIT */
    if (rxtainted)
	RX_MATCH_TAINTED_on(rx);
    TAINT_IF(RX_MATCH_TAINTED(rx));
    PL_curpm = pm;
    if (dynpm->op_pmflags & PMf_ONCE) {
#ifdef USE_ITHREADS
        SvREADONLY_on(PL_regex_pad[dynpm->op_pmoffset]);
#else
        dynpm->op_pmflags |= PMf_USED;
#endif
    }
    if (RX_MATCH_COPIED(rx))
	Safefree(RX_SUBBEG(rx));
    RX_MATCH_COPIED_off(rx);
    RX_SUBBEG(rx) = NULL;
    if (global) {
	/* FIXME - should rx->subbeg be const char *?  */
	RX_SUBBEG(rx) = (char *) truebase;
	RX_OFFS(rx)[0].start = s - truebase;
	if (RX_MATCH_UTF8(rx)) {
	    char * const t = (char*)utf8_hop((U8*)s, RX_MINLENRET(rx));
	    RX_OFFS(rx)[0].end = t - truebase;
	}
	else {
	    RX_OFFS(rx)[0].end = s - truebase + RX_MINLENRET(rx);
	}
	RX_SUBLEN(rx) = strend - truebase;
	goto gotcha;
    }
    if (PL_sawampersand || RX_EXTFLAGS(rx) & RXf_PMf_KEEPCOPY) {
	I32 off;
#ifdef PERL_OLD_COPY_ON_WRITE
	if (SvIsCOW(TARG) || (SvFLAGS(TARG) & CAN_COW_MASK) == CAN_COW_FLAGS) {
	    if (DEBUG_C_TEST) {
		PerlIO_printf(Perl_debug_log,
			      "Copy on write: pp_match $& capture, type %d, truebase=%p, t=%p, difference %d\n",
			      (int) SvTYPE(TARG), (void*)truebase, (void*)t,
			      (int)(t-truebase));
	    }
	    rx->saved_copy = sv_setsv_cow(rx->saved_copy, TARG);
	    RX_SUBBEG(rx) = (char *) SvPVX_const(rx->saved_copy) + (t - truebase);
	    assert (SvPOKp(rx->saved_copy));
	} else
#endif
	{

	    RX_SUBBEG(rx) = savepvn(t, strend - t);
#ifdef PERL_OLD_COPY_ON_WRITE
	    rx->saved_copy = NULL;
#endif
	}
	RX_SUBLEN(rx) = strend - t;
	RX_MATCH_COPIED_on(rx);
	off = RX_OFFS(rx)[0].start = s - t;
	RX_OFFS(rx)[0].end = off + RX_MINLENRET(rx);
    }
    else {			/* startp/endp are used by @- @+. */
	RX_OFFS(rx)[0].start = s - truebase;
	RX_OFFS(rx)[0].end = s - truebase + RX_MINLENRET(rx);
    }
    /* including RX_NPARENS(rx) in the below code seems highly suspicious.
       -dmq */
    RX_NPARENS(rx) = RX_LASTPAREN(rx) = RX_LASTCLOSEPAREN(rx) = 0;	/* used by @-, @+, and $^N */
    LEAVE_SCOPE(oldsave);
    RETPUSHYES;

nope:
ret_no:
    if (global && !(dynpm->op_pmflags & PMf_CONTINUE)) {
	if (SvTYPE(TARG) >= SVt_PVMG && SvMAGIC(TARG)) {
	    MAGIC* const mg = mg_find(TARG, PERL_MAGIC_regex_global);
	    if (mg)
		mg->mg_len = -1;
	}
    }
    LEAVE_SCOPE(oldsave);
    if (gimme == G_ARRAY)
	RETURN;
    RETPUSHNO;
}

OP *
Perl_do_readline(pTHX)
{
    dVAR; dSP; dTARGETSTACKED;
    register SV *sv;
    STRLEN tmplen = 0;
    STRLEN offset;
    PerlIO *fp;
    register IO * const io = GvIO(PL_last_in_gv);
    register const I32 type = PL_op->op_type;
    const I32 gimme = GIMME_V;

    if (io) {
	MAGIC * const mg = SvTIED_mg((const SV *)io, PERL_MAGIC_tiedscalar);
	if (mg) {
	    PUSHMARK(SP);
	    XPUSHs(SvTIED_obj(MUTABLE_SV(io), mg));
	    PUTBACK;
	    ENTER;
	    call_method("READLINE", gimme);
	    LEAVE;
	    SPAGAIN;
	    if (gimme == G_SCALAR) {
		SV* const result = POPs;
		SvSetSV_nosteal(TARG, result);
		PUSHTARG;
	    }
	    RETURN;
	}
    }
    fp = NULL;
    if (io) {
	fp = IoIFP(io);
	if (!fp) {
	    if (IoFLAGS(io) & IOf_ARGV) {
		if (IoFLAGS(io) & IOf_START) {
		    IoLINES(io) = 0;
		    if (av_len(GvAVn(PL_last_in_gv)) < 0) {
			IoFLAGS(io) &= ~IOf_START;
			do_open(PL_last_in_gv,"-",1,FALSE,O_RDONLY,0,NULL);
			sv_setpvs(GvSVn(PL_last_in_gv), "-");
			SvSETMAGIC(GvSV(PL_last_in_gv));
			fp = IoIFP(io);
			goto have_fp;
		    }
		}
		fp = nextargv(PL_last_in_gv);
		if (!fp) { /* Note: fp != IoIFP(io) */
		    (void)do_close(PL_last_in_gv, FALSE); /* now it does*/
		}
	    }
	    else if (type == OP_GLOB)
		fp = Perl_start_glob(aTHX_ POPs, io);
	}
	else if (type == OP_GLOB)
	    SP--;
	else if (ckWARN(WARN_IO) && IoTYPE(io) == IoTYPE_WRONLY) {
	    report_evil_fh(PL_last_in_gv, io, OP_phoney_OUTPUT_ONLY);
	}
    }
    if (!fp) {
	if ((!io || !(IoFLAGS(io) & IOf_START))
	    && ckWARN2(WARN_GLOB, WARN_CLOSED))
	{
	    if (type == OP_GLOB)
		Perl_warner(aTHX_ packWARN(WARN_GLOB),
			    "glob failed (can't start child: %s)",
			    Strerror(errno));
	    else
		report_evil_fh(PL_last_in_gv, io, PL_op->op_type);
	}
	if (gimme == G_SCALAR) {
	    /* undef TARG, and push that undefined value */
	    if (type != OP_RCATLINE) {
		SV_CHECK_THINKFIRST_COW_DROP(TARG);
		SvOK_off(TARG);
	    }
	    PUSHTARG;
	}
	RETURN;
    }
  have_fp:
    if (gimme == G_SCALAR) {
	sv = TARG;
	if (type == OP_RCATLINE && SvGMAGICAL(sv))
	    mg_get(sv);
	if (SvROK(sv)) {
	    if (type == OP_RCATLINE)
		SvPV_force_nolen(sv);
	    else
		sv_unref(sv);
	}
	else if (isGV_with_GP(sv)) {
	    SvPV_force_nolen(sv);
	}
	SvUPGRADE(sv, SVt_PV);
	tmplen = SvLEN(sv);	/* remember if already alloced */
	if (!tmplen && !SvREADONLY(sv))
	    Sv_Grow(sv, 80);	/* try short-buffering it */
	offset = 0;
	if (type == OP_RCATLINE && SvOK(sv)) {
	    if (!SvPOK(sv)) {
		SvPV_force_nolen(sv);
	    }
	    offset = SvCUR(sv);
	}
    }
    else {
	sv = sv_2mortal(newSV(80));
	offset = 0;
    }

    /* This should not be marked tainted if the fp is marked clean */
#define MAYBE_TAINT_LINE(io, sv) \
    if (!(IoFLAGS(io) & IOf_UNTAINT)) { \
	TAINT;				\
	SvTAINTED_on(sv);		\
    }

/* delay EOF state for a snarfed empty file */
#define SNARF_EOF(gimme,rs,io,sv) \
    (gimme != G_SCALAR || SvCUR(sv)					\
     || (IoFLAGS(io) & IOf_NOLINE) || !RsSNARF(rs))

    for (;;) {
	PUTBACK;
	if (!sv_gets(sv, fp, offset)
	    && (type == OP_GLOB
		|| SNARF_EOF(gimme, PL_rs, io, sv)
		|| PerlIO_error(fp)))
	{
	    PerlIO_clearerr(fp);
	    if (IoFLAGS(io) & IOf_ARGV) {
		fp = nextargv(PL_last_in_gv);
		if (fp)
		    continue;
		(void)do_close(PL_last_in_gv, FALSE);
	    }
	    else if (type == OP_GLOB) {
		if (!do_close(PL_last_in_gv, FALSE) && ckWARN(WARN_GLOB)) {
		    Perl_warner(aTHX_ packWARN(WARN_GLOB),
			   "glob failed (child exited with status %d%s)",
			   (int)(STATUS_CURRENT >> 8),
			   (STATUS_CURRENT & 0x80) ? ", core dumped" : "");
		}
	    }
	    if (gimme == G_SCALAR) {
		if (type != OP_RCATLINE) {
		    SV_CHECK_THINKFIRST_COW_DROP(TARG);
		    SvOK_off(TARG);
		}
		SPAGAIN;
		PUSHTARG;
	    }
	    MAYBE_TAINT_LINE(io, sv);
	    RETURN;
	}
	MAYBE_TAINT_LINE(io, sv);
	IoLINES(io)++;
	IoFLAGS(io) |= IOf_NOLINE;
	SvSETMAGIC(sv);
	SPAGAIN;
	XPUSHs(sv);
	if (type == OP_GLOB) {
	    const char *t1;

	    if (SvCUR(sv) > 0 && SvCUR(PL_rs) > 0) {
		char * const tmps = SvEND(sv) - 1;
		if (*tmps == *SvPVX_const(PL_rs)) {
		    *tmps = '\0';
		    SvCUR_set(sv, SvCUR(sv) - 1);
		}
	    }
	    for (t1 = SvPVX_const(sv); *t1; t1++)
		if (!isALPHA(*t1) && !isDIGIT(*t1) &&
		    strchr("$&*(){}[]'\";\\|?<>~`", *t1))
			break;
	    if (*t1 && PerlLIO_lstat(SvPVX_const(sv), &PL_statbuf) < 0) {
		(void)POPs;		/* Unmatched wildcard?  Chuck it... */
		continue;
	    }
	} else if (SvUTF8(sv)) { /* OP_READLINE, OP_RCATLINE */
	     if (ckWARN(WARN_UTF8)) {
		const U8 * const s = (const U8*)SvPVX_const(sv) + offset;
		const STRLEN len = SvCUR(sv) - offset;
		const U8 *f;

		if (!is_utf8_string_loc(s, len, &f))
		    /* Emulate :encoding(utf8) warning in the same case. */
		    Perl_warner(aTHX_ packWARN(WARN_UTF8),
				"utf8 \"\\x%02X\" does not map to Unicode",
				f < (U8*)SvEND(sv) ? *f : 0);
	     }
	}
	if (gimme == G_ARRAY) {
	    if (SvLEN(sv) - SvCUR(sv) > 20) {
		SvPV_shrink_to_cur(sv);
	    }
	    sv = sv_2mortal(newSV(80));
	    continue;
	}
	else if (gimme == G_SCALAR && !tmplen && SvLEN(sv) - SvCUR(sv) > 80) {
	    /* try to reclaim a bit of scalar space (only on 1st alloc) */
	    const STRLEN new_len
		= SvCUR(sv) < 60 ? 80 : SvCUR(sv)+40; /* allow some slop */
	    SvPV_renew(sv, new_len);
	}
	RETURN;
    }
}

PP(pp_enter)
{
    dVAR; dSP;
    register PERL_CONTEXT *cx;
    I32 gimme = OP_GIMME(PL_op, -1);

    if (gimme == -1) {
	if (cxstack_ix >= 0)
	    gimme = cxstack[cxstack_ix].blk_gimme;
	else
	    gimme = G_SCALAR;
    }

    ENTER;

    SAVETMPS;
    PUSHBLOCK(cx, CXt_BLOCK, SP);

    RETURN;
}

PP(pp_helem)
{
    dVAR; dSP;
    HE* he;
    SV **svp;
    SV * const keysv = POPs;
    HV * const hv = MUTABLE_HV(POPs);
    const U32 lval = PL_op->op_flags & OPf_MOD || LVRET;
    const U32 defer = PL_op->op_private & OPpLVAL_DEFER;
    SV *sv;
    const U32 hash = (SvIsCOW_shared_hash(keysv)) ? SvSHARED_HASH(keysv) : 0;
    I32 preeminent = 0;

    if (SvTYPE(hv) != SVt_PVHV)
	RETPUSHUNDEF;

    if (PL_op->op_private & OPpLVAL_INTRO) {
	MAGIC *mg;
	HV *stash;
	/* does the element we're localizing already exist? */
	preeminent = /* can we determine whether it exists? */
	    (    !SvRMAGICAL(hv)
		|| mg_find((const SV *)hv, PERL_MAGIC_env)
		|| (     (mg = mg_find((const SV *)hv, PERL_MAGIC_tied))
			/* Try to preserve the existenceness of a tied hash
			* element by using EXISTS and DELETE if possible.
			* Fallback to FETCH and STORE otherwise */
		    && (stash = SvSTASH(SvRV(SvTIED_obj(MUTABLE_SV(hv), mg))))
		    && gv_fetchmethod_autoload(stash, "EXISTS", TRUE)
		    && gv_fetchmethod_autoload(stash, "DELETE", TRUE)
		)
	    ) ? hv_exists_ent(hv, keysv, 0) : 1;
    }
    he = hv_fetch_ent(hv, keysv, lval && !defer, hash);
    svp = he ? &HeVAL(he) : NULL;
    if (lval) {
	if (!svp || *svp == &PL_sv_undef) {
	    SV* lv;
	    SV* key2;
	    if (!defer) {
		DIE(aTHX_ PL_no_helem_sv, SVfARG(keysv));
	    }
	    lv = sv_newmortal();
	    sv_upgrade(lv, SVt_PVLV);
	    LvTYPE(lv) = 'y';
	    sv_magic(lv, key2 = newSVsv(keysv), PERL_MAGIC_defelem, NULL, 0);
	    SvREFCNT_dec(key2);	/* sv_magic() increments refcount */
	    LvTARG(lv) = SvREFCNT_inc_simple(hv);
	    LvTARGLEN(lv) = 1;
	    PUSHs(lv);
	    RETURN;
	}
	if (PL_op->op_private & OPpLVAL_INTRO) {
	    if (HvNAME_get(hv) && isGV(*svp))
		save_gp(MUTABLE_GV(*svp), !(PL_op->op_flags & OPf_SPECIAL));
	    else {
		if (!preeminent) {
		    STRLEN keylen;
		    const char * const key = SvPV_const(keysv, keylen);
		    SAVEDELETE(hv, savepvn(key,keylen),
			       SvUTF8(keysv) ? -(I32)keylen : (I32)keylen);
		} else
		    save_helem(hv, keysv, svp);
            }
	}
	else if (PL_op->op_private & OPpDEREF)
	    vivify_ref(*svp, PL_op->op_private & OPpDEREF);
    }
    sv = (svp ? *svp : &PL_sv_undef);
    /* This makes C<local $tied{foo} = $tied{foo}> possible.
     * Pushing the magical RHS on to the stack is useless, since
     * that magic is soon destined to be misled by the local(),
     * and thus the later pp_sassign() will fail to mg_get() the
     * old value.  This should also cure problems with delayed
     * mg_get()s.  GSAR 98-07-03 */
    if (!lval && SvGMAGICAL(sv))
	sv = sv_mortalcopy(sv);
    PUSHs(sv);
    RETURN;
}

PP(pp_leave)
{
    dVAR; dSP;
    register PERL_CONTEXT *cx;
    SV **newsp;
    PMOP *newpm;
    I32 gimme;

    if (PL_op->op_flags & OPf_SPECIAL) {
	cx = &cxstack[cxstack_ix];
	cx->blk_oldpm = PL_curpm;	/* fake block should preserve $1 et al */
    }

    POPBLOCK(cx,newpm);

    gimme = OP_GIMME(PL_op, -1);
    if (gimme == -1) {
	if (cxstack_ix >= 0)
	    gimme = cxstack[cxstack_ix].blk_gimme;
	else
	    gimme = G_SCALAR;
    }

    TAINT_NOT;
    if (gimme == G_VOID)
	SP = newsp;
    else if (gimme == G_SCALAR) {
	register SV **mark;
	MARK = newsp + 1;
	if (MARK <= SP) {
	    if (SvFLAGS(TOPs) & (SVs_PADTMP|SVs_TEMP))
		*MARK = TOPs;
	    else
		*MARK = sv_mortalcopy(TOPs);
	} else {
	    MEXTEND(mark,0);
	    *MARK = &PL_sv_undef;
	}
	SP = MARK;
    }
    else if (gimme == G_ARRAY) {
	/* in case LEAVE wipes old return values */
	register SV **mark;
	for (mark = newsp + 1; mark <= SP; mark++) {
	    if (!(SvFLAGS(*mark) & (SVs_PADTMP|SVs_TEMP))) {
		*mark = sv_mortalcopy(*mark);
		TAINT_NOT;	/* Each item is independent */
	    }
	}
    }
    PL_curpm = newpm;	/* Don't pop $1 et al till now */

    LEAVE;

    RETURN;
}

PP(pp_iter)
{
    dVAR; dSP;
    register PERL_CONTEXT *cx;
    SV *sv, *oldsv;
    AV* av;
    SV **itersvp;

    EXTEND(SP, 1);
    cx = &cxstack[cxstack_ix];
    if (CxTYPE(cx) != CXt_LOOP)
	DIE(aTHX_ "panic: pp_iter");

    itersvp = CxITERVAR(cx);
    av = cx->blk_loop.iterary;
    if (SvTYPE(av) != SVt_PVAV) {
	/* iterate ($min .. $max) */
	if (cx->blk_loop.iterlval) {
	    /* string increment */
	    register SV* cur = cx->blk_loop.iterlval;
	    /* If the maximum is !SvOK(), pp_enteriter substitutes PL_sv_no.
	       It has SvPVX of "" and SvCUR of 0, which is what we want.  */
	    STRLEN maxlen = 0;
	    const char *max = SvPV_const((SV*)av, maxlen);
	    if (!SvNIOK(cur) && SvCUR(cur) <= maxlen) {
		if (SvREFCNT(*itersvp) == 1 && !SvMAGICAL(*itersvp)) {
		    /* safe to reuse old SV */
		    sv_setsv(*itersvp, cur);
		}
		else
		{
		    /* we need a fresh SV every time so that loop body sees a
		     * completely new SV for closures/references to work as
		     * they used to */
		    oldsv = *itersvp;
		    *itersvp = newSVsv(cur);
		    SvREFCNT_dec(oldsv);
		}
		if (strEQ(SvPVX_const(cur), max))
		    sv_setiv(cur, 0); /* terminate next time */
		else
		    sv_inc(cur);
		RETPUSHYES;
	    }
	    RETPUSHNO;
	}
	/* integer increment */
	if (cx->blk_loop.iterix > cx->blk_loop.itermax)
	    RETPUSHNO;

	/* don't risk potential race */
	if (SvREFCNT(*itersvp) == 1 && !SvMAGICAL(*itersvp)) {
	    /* safe to reuse old SV */
	    sv_setiv(*itersvp, cx->blk_loop.iterix++);
	}
	else
	{
	    /* we need a fresh SV every time so that loop body sees a
	     * completely new SV for closures/references to work as they
	     * used to */
	    oldsv = *itersvp;
	    *itersvp = newSViv(cx->blk_loop.iterix++);
	    SvREFCNT_dec(oldsv);
	}

	/* Handle end of range at IV_MAX */
	if ((cx->blk_loop.iterix == IV_MIN) &&
	    (cx->blk_loop.itermax == IV_MAX))
	{
	    cx->blk_loop.iterix++;
	    cx->blk_loop.itermax++;
	}

	RETPUSHYES;
    }

    /* iterate array */
    if (PL_op->op_private & OPpITER_REVERSED) {
	/* In reverse, use itermax as the min :-)  */
	if (cx->blk_loop.iterix <= cx->blk_loop.itermax)
	    RETPUSHNO;

	if (SvMAGICAL(av) || AvREIFY(av)) {
	    SV * const * const svp = av_fetch(av, --cx->blk_loop.iterix, FALSE);
	    sv = svp ? *svp : NULL;
	}
	else {
	    sv = AvARRAY(av)[--cx->blk_loop.iterix];
	}
    }
    else {
	if (cx->blk_loop.iterix >= (av == PL_curstack ? cx->blk_oldsp :
				    AvFILL(av)))
	    RETPUSHNO;

	if (SvMAGICAL(av) || AvREIFY(av)) {
	    SV * const * const svp = av_fetch(av, ++cx->blk_loop.iterix, FALSE);
	    sv = svp ? *svp : NULL;
	}
	else {
	    sv = AvARRAY(av)[++cx->blk_loop.iterix];
	}
    }

    if (sv && SvIS_FREED(sv)) {
	*itersvp = NULL;
	Perl_croak(aTHX_ "Use of freed value in iteration");
    }

    if (sv)
	SvTEMP_off(sv);
    else
	sv = &PL_sv_undef;
    if (av != PL_curstack && sv == &PL_sv_undef) {
	SV *lv = cx->blk_loop.iterlval;
	if (lv && SvREFCNT(lv) > 1) {
	    SvREFCNT_dec(lv);
	    lv = NULL;
	}
	if (lv)
	    SvREFCNT_dec(LvTARG(lv));
	else {
	    lv = cx->blk_loop.iterlval = newSV_type(SVt_PVLV);
	    LvTYPE(lv) = 'y';
	    sv_magic(lv, NULL, PERL_MAGIC_defelem, NULL, 0);
	}
	LvTARG(lv) = SvREFCNT_inc_simple(av);
	LvTARGOFF(lv) = cx->blk_loop.iterix;
	LvTARGLEN(lv) = (STRLEN)UV_MAX;
	sv = (SV*)lv;
    }

    oldsv = *itersvp;
    *itersvp = SvREFCNT_inc_simple_NN(sv);
    SvREFCNT_dec(oldsv);

    RETPUSHYES;
}

PP(pp_subst)
{
    dVAR; dSP; dTARG;
    register PMOP *pm = cPMOP;
    PMOP *rpm = pm;
    register char *s;
    char *strend;
    register char *m;
    const char *c;
    register char *d;
    STRLEN clen;
    I32 iters = 0;
    I32 maxiters;
    register I32 i;
    bool once;
    U8 rxtainted;
    char *orig;
    I32 r_flags;
    register REGEXP *rx = PM_GETRE(pm);
    STRLEN len;
    int force_on_match = 0;
    const I32 oldsave = PL_savestack_ix;
    STRLEN slen;
    bool doutf8 = FALSE;
    I32 matched;
#ifdef PERL_OLD_COPY_ON_WRITE
    bool is_cow;
#endif
    SV *nsv = NULL;

    /* known replacement string? */
    register SV *dstr = (pm->op_pmflags & PMf_CONST) ? POPs : NULL;
    if (PL_op->op_flags & OPf_STACKED)
	TARG = POPs;
    else if (PL_op->op_private & OPpTARGET_MY)
	GETTARGET;
    else {
	TARG = DEFSV;
	EXTEND(SP,1);
    }

#ifdef PERL_OLD_COPY_ON_WRITE
    /* Awooga. Awooga. "bool" types that are actually char are dangerous,
       because they make integers such as 256 "false".  */
    is_cow = SvIsCOW(TARG) ? TRUE : FALSE;
#else
    if (SvIsCOW(TARG))
	sv_force_normal_flags(TARG,0);
#endif
    if (
#ifdef PERL_OLD_COPY_ON_WRITE
	!is_cow &&
#endif
	(SvREADONLY(TARG)
	 || ( ((SvTYPE(TARG) == SVt_PVGV && isGV_with_GP(TARG))
	       || SvTYPE(TARG) > SVt_PVLV)
	     && !(SvTYPE(TARG) == SVt_PVGV && SvFAKE(TARG)))))
	DIE(aTHX_ "%s", PL_no_modify);
    PUTBACK;

    s = SvPV_mutable(TARG, len);
    if (!SvPOKp(TARG) || SvTYPE(TARG) == SVt_PVGV)
	force_on_match = 1;
    rxtainted = ((RX_EXTFLAGS(rx) & RXf_TAINTED) ||
		 (PL_tainted && (pm->op_pmflags & PMf_RETAINT)));
    if (PL_tainted)
	rxtainted |= 2;
    TAINT_NOT;

    RX_MATCH_UTF8_set(rx, DO_UTF8(TARG));

  force_it:
    if (!pm || !s)
	DIE(aTHX_ "panic: pp_subst");

    strend = s + len;
    slen = RX_MATCH_UTF8(rx) ? utf8_length((U8*)s, (U8*)strend) : len;
    maxiters = 2 * slen + 10;	/* We can match twice at each
				   position, once with zero-length,
				   second time with non-zero. */

    if (!RX_PRELEN(rx) && PL_curpm) {
	pm = PL_curpm;
	rx = PM_GETRE(pm);
    }
    r_flags = (RX_NPARENS(rx) || SvTEMP(TARG) || PL_sawampersand
	    || (RX_EXTFLAGS(rx) & (RXf_EVAL_SEEN|RXf_PMf_KEEPCOPY)) )
	       ? REXEC_COPY_STR : 0;
    if (SvSCREAM(TARG))
	r_flags |= REXEC_SCREAM;

    orig = m = s;
    if (RX_EXTFLAGS(rx) & RXf_USE_INTUIT) {
	PL_bostr = orig;
	s = CALLREG_INTUIT_START(rx, TARG, s, strend, r_flags, NULL);

	if (!s)
	    goto nope;
	/* How to do it in subst? */
/*	if ( (RX_EXTFLAGS(rx) & RXf_CHECK_ALL)
	     && !PL_sawampersand
	     && !(RX_EXTFLAGS(rx) & RXf_KEEPCOPY)
	     && ((RX_EXTFLAGS(rx) & RXf_NOSCAN)
		 || !((RX_EXTFLAGS(rx) & RXf_INTUIT_TAIL)
		      && (r_flags & REXEC_SCREAM))))
	    goto yup;
*/
    }

    /* only replace once? */
    once = !(rpm->op_pmflags & PMf_GLOBAL);
    matched = CALLREGEXEC(rx, s, strend, orig, 0, TARG, NULL,
			 r_flags | REXEC_CHECKED);
    /* known replacement string? */
    if (dstr) {
	/* replacement needing upgrading? */
	if (DO_UTF8(TARG) && !doutf8) {
	     nsv = sv_newmortal();
	     SvSetSV(nsv, dstr);
	     if (PL_encoding)
		  sv_recode_to_utf8(nsv, PL_encoding);
	     else
		  sv_utf8_upgrade(nsv);
	     c = SvPV_const(nsv, clen);
	     doutf8 = TRUE;
	}
	else {
	    c = SvPV_const(dstr, clen);
	    doutf8 = DO_UTF8(dstr);
	}
    }
    else {
	c = NULL;
	doutf8 = FALSE;
    }
    
    /* can do inplace substitution? */
    if (c
#ifdef PERL_OLD_COPY_ON_WRITE
	&& !is_cow
#endif
	&& (I32)clen <= RX_MINLENRET(rx) && (once || !(r_flags & REXEC_COPY_STR))
	&& !(RX_EXTFLAGS(rx) & RXf_LOOKBEHIND_SEEN)
	&& (!doutf8 || SvUTF8(TARG))) {
	if (!matched)
	{
	    SPAGAIN;
	    PUSHs(&PL_sv_no);
	    LEAVE_SCOPE(oldsave);
	    RETURN;
	}
#ifdef PERL_OLD_COPY_ON_WRITE
	if (SvIsCOW(TARG)) {
	    assert (!force_on_match);
	    goto have_a_cow;
	}
#endif
	if (force_on_match) {
	    force_on_match = 0;
	    s = SvPV_force(TARG, len);
	    goto force_it;
	}
	d = s;
	PL_curpm = pm;
	SvSCREAM_off(TARG);	/* disable possible screamer */
	if (once) {
	    rxtainted |= RX_MATCH_TAINTED(rx);
	    m = orig + RX_OFFS(rx)[0].start;
	    d = orig + RX_OFFS(rx)[0].end;
	    s = orig;
	    if (m - s > strend - d) {  /* faster to shorten from end */
		if (clen) {
		    Copy(c, m, clen, char);
		    m += clen;
		}
		i = strend - d;
		if (i > 0) {
		    Move(d, m, i, char);
		    m += i;
		}
		*m = '\0';
		SvCUR_set(TARG, m - s);
	    }
	    else if ((i = m - s)) {	/* faster from front */
		d -= clen;
		m = d;
		Move(s, d - i, i, char);
		sv_chop(TARG, d-i);
		if (clen)
		    Copy(c, m, clen, char);
	    }
	    else if (clen) {
		d -= clen;
		sv_chop(TARG, d);
		Copy(c, d, clen, char);
	    }
	    else {
		sv_chop(TARG, d);
	    }
	    TAINT_IF(rxtainted & 1);
	    SPAGAIN;
	    PUSHs(&PL_sv_yes);
	}
	else {
	    do {
		if (iters++ > maxiters)
		    DIE(aTHX_ "Substitution loop");
		rxtainted |= RX_MATCH_TAINTED(rx);
		m = RX_OFFS(rx)[0].start + orig;
		if ((i = m - s)) {
		    if (s != d)
			Move(s, d, i, char);
		    d += i;
		}
		if (clen) {
		    Copy(c, d, clen, char);
		    d += clen;
		}
		s = RX_OFFS(rx)[0].end + orig;
	    } while (CALLREGEXEC(rx, s, strend, orig, s == m,
				 TARG, NULL,
				 /* don't match same null twice */
				 REXEC_NOT_FIRST|REXEC_IGNOREPOS));
	    if (s != d) {
		i = strend - s;
		SvCUR_set(TARG, d - SvPVX_const(TARG) + i);
		Move(s, d, i+1, char);		/* include the NUL */
	    }
	    TAINT_IF(rxtainted & 1);
	    SPAGAIN;
	    mPUSHi((I32)iters);
	}
	(void)SvPOK_only_UTF8(TARG);
	TAINT_IF(rxtainted);
	if (SvSMAGICAL(TARG)) {
	    PUTBACK;
	    mg_set(TARG);
	    SPAGAIN;
	}
	SvTAINT(TARG);
	if (doutf8)
	    SvUTF8_on(TARG);
	LEAVE_SCOPE(oldsave);
	RETURN;
    }

    if (matched)
    {
	if (force_on_match) {
	    force_on_match = 0;
	    s = SvPV_force(TARG, len);
	    goto force_it;
	}
#ifdef PERL_OLD_COPY_ON_WRITE
      have_a_cow:
#endif
	rxtainted |= RX_MATCH_TAINTED(rx);
	dstr = newSVpvn_utf8(m, s-m, DO_UTF8(TARG));
	SAVEFREESV(dstr);
	PL_curpm = pm;
	if (!c) {
	    register PERL_CONTEXT *cx;
	    SPAGAIN;
	    PUSHSUBST(cx);
	    RETURNOP(cPMOP->op_pmreplrootu.op_pmreplroot);
	}
	r_flags |= REXEC_IGNOREPOS | REXEC_NOT_FIRST;
	do {
	    if (iters++ > maxiters)
		DIE(aTHX_ "Substitution loop");
	    rxtainted |= RX_MATCH_TAINTED(rx);
	    if (RX_MATCH_COPIED(rx) && RX_SUBBEG(rx) != orig) {
		m = s;
		s = orig;
		orig = RX_SUBBEG(rx);
		s = orig + (m - s);
		strend = s + (strend - m);
	    }
	    m = RX_OFFS(rx)[0].start + orig;
	    if (doutf8 && !SvUTF8(dstr))
		sv_catpvn_utf8_upgrade(dstr, s, m - s, nsv);
            else
		sv_catpvn(dstr, s, m-s);
	    s = RX_OFFS(rx)[0].end + orig;
	    if (clen)
		sv_catpvn(dstr, c, clen);
	    if (once)
		break;
	} while (CALLREGEXEC(rx, s, strend, orig, s == m,
			     TARG, NULL, r_flags));
	if (doutf8 && !DO_UTF8(TARG))
	    sv_catpvn_utf8_upgrade(dstr, s, strend - s, nsv);
	else
	    sv_catpvn(dstr, s, strend - s);

#ifdef PERL_OLD_COPY_ON_WRITE
	/* The match may make the string COW. If so, brilliant, because that's
	   just saved us one malloc, copy and free - the regexp has donated
	   the old buffer, and we malloc an entirely new one, rather than the
	   regexp malloc()ing a buffer and copying our original, only for
	   us to throw it away here during the substitution.  */
	if (SvIsCOW(TARG)) {
	    sv_force_normal_flags(TARG, SV_COW_DROP_PV);
	} else
#endif
	{
	    SvPV_free(TARG);
	}
	SvPV_set(TARG, SvPVX(dstr));
	SvCUR_set(TARG, SvCUR(dstr));
	SvLEN_set(TARG, SvLEN(dstr));
	doutf8 |= DO_UTF8(dstr);
	SvPV_set(dstr, NULL);

	TAINT_IF(rxtainted & 1);
	SPAGAIN;
	mPUSHi((I32)iters);

	(void)SvPOK_only(TARG);
	if (doutf8)
	    SvUTF8_on(TARG);
	TAINT_IF(rxtainted);
	SvSETMAGIC(TARG);
	SvTAINT(TARG);
	LEAVE_SCOPE(oldsave);
	RETURN;
    }
    goto ret_no;

nope:
ret_no:
    SPAGAIN;
    PUSHs(&PL_sv_no);
    LEAVE_SCOPE(oldsave);
    RETURN;
}

PP(pp_grepwhile)
{
    dVAR; dSP;

    if (SvTRUEx(POPs))
	PL_stack_base[PL_markstack_ptr[-1]++] = PL_stack_base[*PL_markstack_ptr];
    ++*PL_markstack_ptr;
    LEAVE;					/* exit inner scope */

    /* All done yet? */
    if (PL_stack_base + *PL_markstack_ptr > SP) {
	I32 items;
	const I32 gimme = GIMME_V;

	LEAVE;					/* exit outer scope */
	(void)POPMARK;				/* pop src */
	items = --*PL_markstack_ptr - PL_markstack_ptr[-1];
	(void)POPMARK;				/* pop dst */
	SP = PL_stack_base + POPMARK;		/* pop original mark */
	if (gimme == G_SCALAR) {
	    if (PL_op->op_private & OPpGREP_LEX) {
		SV* const sv = sv_newmortal();
		sv_setiv(sv, items);
		PUSHs(sv);
	    }
	    else {
		dTARGET;
		XPUSHi(items);
	    }
	}
	else if (gimme == G_ARRAY)
	    SP += items;
	RETURN;
    }
    else {
	SV *src;

	ENTER;					/* enter inner scope */
	SAVEVPTR(PL_curpm);

	src = PL_stack_base[*PL_markstack_ptr];
	SvTEMP_off(src);
	if (PL_op->op_private & OPpGREP_LEX)
	    PAD_SVl(PL_op->op_targ) = src;
	else
	    DEFSV_set(src);

	RETURNOP(cLOGOP->op_other);
    }
}

PP(pp_leavesub)
{
    dVAR; dSP;
    SV **mark;
    SV **newsp;
    PMOP *newpm;
    I32 gimme;
    register PERL_CONTEXT *cx;
    SV *sv;

    if (CxMULTICALL(&cxstack[cxstack_ix]))
	return 0;

    POPBLOCK(cx,newpm);
    cxstack_ix++; /* temporarily protect top context */

    TAINT_NOT;
    if (gimme == G_SCALAR) {
	MARK = newsp + 1;
	if (MARK <= SP) {
	    if (cx->blk_sub.cv && CvDEPTH(cx->blk_sub.cv) > 1) {
		if (SvTEMP(TOPs)) {
		    *MARK = SvREFCNT_inc(TOPs);
		    FREETMPS;
		    sv_2mortal(*MARK);
		}
		else {
		    sv = SvREFCNT_inc(TOPs);	/* FREETMPS could clobber it */
		    FREETMPS;
		    *MARK = sv_mortalcopy(sv);
		    SvREFCNT_dec(sv);
		}
	    }
	    else
		*MARK = SvTEMP(TOPs) ? TOPs : sv_mortalcopy(TOPs);
	}
	else {
	    MEXTEND(MARK, 0);
	    *MARK = &PL_sv_undef;
	}
	SP = MARK;
    }
    else if (gimme == G_ARRAY) {
	for (MARK = newsp + 1; MARK <= SP; MARK++) {
	    if (!SvTEMP(*MARK)) {
		*MARK = sv_mortalcopy(*MARK);
		TAINT_NOT;	/* Each item is independent */
	    }
	}
    }
    PUTBACK;

    LEAVE;
    cxstack_ix--;
    POPSUB(cx,sv);	/* Stack values are safe: release CV and @_ ... */
    PL_curpm = newpm;	/* ... and pop $1 et al */

    LEAVESUB(sv);
    return cx->blk_sub.retop;
}

/* This duplicates the above code because the above code must not
 * get any slower by more conditions */
PP(pp_leavesublv)
{
    dVAR; dSP;
    SV **mark;
    SV **newsp;
    PMOP *newpm;
    I32 gimme;
    register PERL_CONTEXT *cx;
    SV *sv;

    if (CxMULTICALL(&cxstack[cxstack_ix]))
	return 0;

    POPBLOCK(cx,newpm);
    cxstack_ix++; /* temporarily protect top context */

    TAINT_NOT;

    if (CxLVAL(cx) & OPpENTERSUB_INARGS) {
	/* We are an argument to a function or grep().
	 * This kind of lvalueness was legal before lvalue
	 * subroutines too, so be backward compatible:
	 * cannot report errors.  */

	/* Scalar context *is* possible, on the LHS of -> only,
	 * as in f()->meth().  But this is not an lvalue. */
	if (gimme == G_SCALAR)
	    goto temporise;
	if (gimme == G_ARRAY) {
	    if (!CvLVALUE(cx->blk_sub.cv))
		goto temporise_array;
	    EXTEND_MORTAL(SP - newsp);
	    for (mark = newsp + 1; mark <= SP; mark++) {
		if (SvTEMP(*mark))
		    NOOP;
		else if (SvFLAGS(*mark) & (SVs_PADTMP | SVf_READONLY))
		    *mark = sv_mortalcopy(*mark);
		else {
		    /* Can be a localized value subject to deletion. */
		    PL_tmps_stack[++PL_tmps_ix] = *mark;
		    SvREFCNT_inc_void(*mark);
		}
	    }
	}
    }
    else if (CxLVAL(cx)) {     /* Leave it as it is if we can. */
	/* Here we go for robustness, not for speed, so we change all
	 * the refcounts so the caller gets a live guy. Cannot set
	 * TEMP, so sv_2mortal is out of question. */
	if (!CvLVALUE(cx->blk_sub.cv)) {
	    LEAVE;
	    cxstack_ix--;
	    POPSUB(cx,sv);
	    PL_curpm = newpm;
	    LEAVESUB(sv);
	    DIE(aTHX_ "Can't modify non-lvalue subroutine call");
	}
	if (gimme == G_SCALAR) {
	    MARK = newsp + 1;
	    EXTEND_MORTAL(1);
	    if (MARK == SP) {
		/* Temporaries are bad unless they happen to be elements
		 * of a tied hash or array */
		if (SvFLAGS(TOPs) & (SVs_TEMP | SVs_PADTMP | SVf_READONLY) &&
		    !(SvRMAGICAL(TOPs) && mg_find(TOPs, PERL_MAGIC_tiedelem))) {
		    LEAVE;
		    cxstack_ix--;
		    POPSUB(cx,sv);
		    PL_curpm = newpm;
		    LEAVESUB(sv);
		    DIE(aTHX_ "Can't return %s from lvalue subroutine",
			SvREADONLY(TOPs) ? (TOPs == &PL_sv_undef) ? "undef"
			: "a readonly value" : "a temporary");
		}
		else {                  /* Can be a localized value
					 * subject to deletion. */
		    PL_tmps_stack[++PL_tmps_ix] = *mark;
		    SvREFCNT_inc_void(*mark);
		}
	    }
	    else {			/* Should not happen? */
		LEAVE;
		cxstack_ix--;
		POPSUB(cx,sv);
		PL_curpm = newpm;
		LEAVESUB(sv);
		DIE(aTHX_ "%s returned from lvalue subroutine in scalar context",
		    (MARK > SP ? "Empty array" : "Array"));
	    }
	    SP = MARK;
	}
	else if (gimme == G_ARRAY) {
	    EXTEND_MORTAL(SP - newsp);
	    for (mark = newsp + 1; mark <= SP; mark++) {
		if (*mark != &PL_sv_undef
		    && SvFLAGS(*mark) & (SVs_TEMP | SVs_PADTMP | SVf_READONLY)) {
		    /* Might be flattened array after $#array =  */
		    PUTBACK;
		    LEAVE;
		    cxstack_ix--;
		    POPSUB(cx,sv);
		    PL_curpm = newpm;
		    LEAVESUB(sv);
		    DIE(aTHX_ "Can't return a %s from lvalue subroutine",
			SvREADONLY(TOPs) ? "readonly value" : "temporary");
		}
		else {
		    /* Can be a localized value subject to deletion. */
		    PL_tmps_stack[++PL_tmps_ix] = *mark;
		    SvREFCNT_inc_void(*mark);
		}
	    }
	}
    }
    else {
	if (gimme == G_SCALAR) {
	  temporise:
	    MARK = newsp + 1;
	    if (MARK <= SP) {
		if (cx->blk_sub.cv && CvDEPTH(cx->blk_sub.cv) > 1) {
		    if (SvTEMP(TOPs)) {
			*MARK = SvREFCNT_inc(TOPs);
			FREETMPS;
			sv_2mortal(*MARK);
		    }
		    else {
			sv = SvREFCNT_inc(TOPs); /* FREETMPS could clobber it */
			FREETMPS;
			*MARK = sv_mortalcopy(sv);
			SvREFCNT_dec(sv);
		    }
		}
		else
		    *MARK = SvTEMP(TOPs) ? TOPs : sv_mortalcopy(TOPs);
	    }
	    else {
		MEXTEND(MARK, 0);
		*MARK = &PL_sv_undef;
	    }
	    SP = MARK;
	}
	else if (gimme == G_ARRAY) {
	  temporise_array:
	    for (MARK = newsp + 1; MARK <= SP; MARK++) {
		if (!SvTEMP(*MARK)) {
		    *MARK = sv_mortalcopy(*MARK);
		    TAINT_NOT;  /* Each item is independent */
		}
	    }
	}
    }
    PUTBACK;

    LEAVE;
    cxstack_ix--;
    POPSUB(cx,sv);	/* Stack values are safe: release CV and @_ ... */
    PL_curpm = newpm;	/* ... and pop $1 et al */

    LEAVESUB(sv);
    return cx->blk_sub.retop;
}

PP(pp_entersub)
{
    dVAR; dSP; dPOPss;
    GV *gv;
    register CV *cv;
    register PERL_CONTEXT *cx;
    I32 gimme;
    const bool hasargs = (PL_op->op_flags & OPf_STACKED) != 0;

    if (!sv)
	DIE(aTHX_ "Not a CODE reference");
    switch (SvTYPE(sv)) {
	/* This is overwhelming the most common case:  */
    case SVt_PVGV:
	if (!isGV_with_GP(sv))
	    DIE(aTHX_ "Not a CODE reference");
	if (!(cv = GvCVu((const GV *)sv))) {
	    HV *stash;
	    cv = sv_2cv(sv, &stash, &gv, 0);
	}
	if (!cv) {
	    ENTER;
	    SAVETMPS;
	    goto try_autoload;
	}
	break;
    default:
	if (!SvROK(sv)) {
	    const char *sym;
	    STRLEN len;
	    if (sv == &PL_sv_yes) {		/* unfound import, ignore */
		if (hasargs)
		    SP = PL_stack_base + POPMARK;
		RETURN;
	    }
	    if (SvGMAGICAL(sv)) {
		mg_get(sv);
		if (SvROK(sv))
		    goto got_rv;
		if (SvPOKp(sv)) {
		    sym = SvPVX_const(sv);
		    len = SvCUR(sv);
		} else {
		    sym = NULL;
		    len = 0;
		}
	    }
	    else {
		sym = SvPV_const(sv, len);
            }
	    if (!sym)
		DIE(aTHX_ PL_no_usym, "a subroutine");
	    if (PL_op->op_private & HINT_STRICT_REFS)
		DIE(aTHX_ PL_no_symref, sym, "a subroutine");
	    cv = get_cvn_flags(sym, len, GV_ADD|SvUTF8(sv));
	    break;
	}
  got_rv:
	{
	    SV * const * sp = &sv;		/* Used in tryAMAGICunDEREF macro. */
	    tryAMAGICunDEREF(to_cv);
	}	
	cv = MUTABLE_CV(SvRV(sv));
	if (SvTYPE(cv) == SVt_PVCV)
	    break;
	/* FALL THROUGH */
    case SVt_PVHV:
    case SVt_PVAV:
	DIE(aTHX_ "Not a CODE reference");
	/* This is the second most common case:  */
    case SVt_PVCV:
	cv = MUTABLE_CV(sv);
	break;
    }

    ENTER;
    SAVETMPS;

  retry:
    if (!CvROOT(cv) && !CvXSUB(cv)) {
	GV* autogv;
	SV* sub_name;

	/* anonymous or undef'd function leaves us no recourse */
	if (CvANON(cv) || !(gv = CvGV(cv)))
	    DIE(aTHX_ "Undefined subroutine called");

	/* autoloaded stub? */
	if (cv != GvCV(gv)) {
	    cv = GvCV(gv);
	}
	/* should call AUTOLOAD now? */
	else {
try_autoload:
	    if ((autogv = gv_autoload4(GvSTASH(gv), GvNAME(gv), GvNAMELEN(gv),
				   FALSE)))
	    {
		cv = GvCV(autogv);
	    }
	    /* sorry */
	    else {
		sub_name = sv_newmortal();
		gv_efullname3(sub_name, gv, NULL);
		DIE(aTHX_ "Undefined subroutine &%"SVf" called", SVfARG(sub_name));
	    }
	}
	if (!cv)
	    DIE(aTHX_ "Not a CODE reference");
	goto retry;
    }

    gimme = GIMME_V;
    if ((PL_op->op_private & OPpENTERSUB_DB) && GvCV(PL_DBsub) && !CvNODEBUG(cv)) {
	 Perl_get_db_sub(aTHX_ &sv, cv);
	 if (CvISXSUB(cv))
	     PL_curcopdb = PL_curcop;
         if (CvLVALUE(cv)) {
             /* check for lsub that handles lvalue subroutines */
	     cv = GvCV(gv_HVadd(gv_fetchpv("DB::lsub", GV_ADDMULTI, SVt_PVHV)));
             /* if lsub not found then fall back to DB::sub */
	     if (!cv) cv = GvCV(PL_DBsub);
         } else {
             cv = GvCV(PL_DBsub);
         }

	if (!cv || (!CvXSUB(cv) && !CvSTART(cv)))
	    DIE(aTHX_ "No DB::sub routine defined");
    }

    if (!(CvISXSUB(cv))) {
	/* This path taken at least 75% of the time   */
	dMARK;
	register I32 items = SP - MARK;
	AV* const padlist = CvPADLIST(cv);
	PUSHBLOCK(cx, CXt_SUB, MARK);
	PUSHSUB(cx);
	cx->blk_sub.retop = PL_op->op_next;
	CvDEPTH(cv)++;
	/* XXX This would be a natural place to set C<PL_compcv = cv> so
	 * that eval'' ops within this sub know the correct lexical space.
	 * Owing the speed considerations, we choose instead to search for
	 * the cv using find_runcv() when calling doeval().
	 */
	if (CvDEPTH(cv) >= 2) {
	    PERL_STACK_OVERFLOW_CHECK();
	    pad_push(padlist, CvDEPTH(cv));
	}
	SAVECOMPPAD();
	PAD_SET_CUR_NOSAVE(padlist, CvDEPTH(cv));
	if (hasargs) {
	    AV *const av = MUTABLE_AV(PAD_SVl(0));
	    if (AvREAL(av)) {
		/* @_ is normally not REAL--this should only ever
		 * happen when DB::sub() calls things that modify @_ */
		av_clear(av);
		AvREAL_off(av);
		AvREIFY_on(av);
	    }
	    cx->blk_sub.savearray = GvAV(PL_defgv);
	    GvAV(PL_defgv) = MUTABLE_AV(SvREFCNT_inc_simple(av));
	    CX_CURPAD_SAVE(cx->blk_sub);
	    cx->blk_sub.argarray = av;
	    ++MARK;

	    if (items > AvMAX(av) + 1) {
		SV **ary = AvALLOC(av);
		if (AvARRAY(av) != ary) {
		    AvMAX(av) += AvARRAY(av) - AvALLOC(av);
		    AvARRAY(av) = ary;
		}
		if (items > AvMAX(av) + 1) {
		    AvMAX(av) = items - 1;
		    Renew(ary,items,SV*);
		    AvALLOC(av) = ary;
		    AvARRAY(av) = ary;
		}
	    }
	    Copy(MARK,AvARRAY(av),items,SV*);
	    AvFILLp(av) = items - 1;
	
	    while (items--) {
		if (*MARK)
		    SvTEMP_off(*MARK);
		MARK++;
	    }
	}
	/* warning must come *after* we fully set up the context
	 * stuff so that __WARN__ handlers can safely dounwind()
	 * if they want to
	 */
	if (CvDEPTH(cv) == PERL_SUB_DEPTH_WARN && ckWARN(WARN_RECURSION)
	    && !(PERLDB_SUB && cv == GvCV(PL_DBsub)))
	    sub_crush_depth(cv);
#if 0
	DEBUG_S(PerlIO_printf(Perl_debug_log,
			      "%p entersub returning %p\n", (void*)thr, (void*)CvSTART(cv)));
#endif
	RETURNOP(CvSTART(cv));
    }
    else {
	I32 markix = TOPMARK;

	PUTBACK;

	if (!hasargs) {
	    /* Need to copy @_ to stack. Alternative may be to
	     * switch stack to @_, and copy return values
	     * back. This would allow popping @_ in XSUB, e.g.. XXXX */
	    AV * const av = GvAV(PL_defgv);
	    const I32 items = AvFILLp(av) + 1;   /* @_ is not tieable */

	    if (items) {
		/* Mark is at the end of the stack. */
		EXTEND(SP, items);
		Copy(AvARRAY(av), SP + 1, items, SV*);
		SP += items;
		PUTBACK ;		
	    }
	}
	/* We assume first XSUB in &DB::sub is the called one. */
	if (PL_curcopdb) {
	    SAVEVPTR(PL_curcop);
	    PL_curcop = PL_curcopdb;
	    PL_curcopdb = NULL;
	}
	/* Do we need to open block here? XXXX */
	if (CvXSUB(cv)) /* XXX this is supposed to be true */
	    (void)(*CvXSUB(cv))(aTHX_ cv);

	/* Enforce some sanity in scalar context. */
	if (gimme == G_SCALAR && ++markix != PL_stack_sp - PL_stack_base ) {
	    if (markix > PL_stack_sp - PL_stack_base)
		*(PL_stack_base + markix) = &PL_sv_undef;
	    else
		*(PL_stack_base + markix) = *PL_stack_sp;
	    PL_stack_sp = PL_stack_base + markix;
	}
	LEAVE;
	return NORMAL;
    }
}

void
Perl_sub_crush_depth(pTHX_ CV *cv)
{
    PERL_ARGS_ASSERT_SUB_CRUSH_DEPTH;

    if (CvANON(cv))
	Perl_warner(aTHX_ packWARN(WARN_RECURSION), "Deep recursion on anonymous subroutine");
    else {
	SV* const tmpstr = sv_newmortal();
	gv_efullname3(tmpstr, CvGV(cv), NULL);
	Perl_warner(aTHX_ packWARN(WARN_RECURSION), "Deep recursion on subroutine \"%"SVf"\"",
		    SVfARG(tmpstr));
    }
}

PP(pp_aelem)
{
    dVAR; dSP;
    SV** svp;
    SV* const elemsv = POPs;
    IV elem = SvIV(elemsv);
    AV *const av = MUTABLE_AV(POPs);
    const U32 lval = PL_op->op_flags & OPf_MOD || LVRET;
    const U32 defer = (PL_op->op_private & OPpLVAL_DEFER) && (elem > av_len(av));
    SV *sv;

    if (SvROK(elemsv) && !SvGAMAGIC(elemsv) && ckWARN(WARN_MISC))
	Perl_warner(aTHX_ packWARN(WARN_MISC),
		    "Use of reference \"%"SVf"\" as array index",
		    SVfARG(elemsv));
    if (elem > 0)
	elem -= CopARYBASE_get(PL_curcop);
    if (SvTYPE(av) != SVt_PVAV)
	RETPUSHUNDEF;
    svp = av_fetch(av, elem, lval && !defer);
    if (lval) {
#ifdef PERL_MALLOC_WRAP
	 if (SvUOK(elemsv)) {
	      const UV uv = SvUV(elemsv);
	      elem = uv > IV_MAX ? IV_MAX : uv;
	 }
	 else if (SvNOK(elemsv))
	      elem = (IV)SvNV(elemsv);
	 if (elem > 0) {
	      static const char oom_array_extend[] =
		"Out of memory during array extend"; /* Duplicated in av.c */
	      MEM_WRAP_CHECK_1(elem,SV*,oom_array_extend);
	 }
#endif
	if (!svp || *svp == &PL_sv_undef) {
	    SV* lv;
	    if (!defer)
		DIE(aTHX_ PL_no_aelem, elem);
	    lv = sv_newmortal();
	    sv_upgrade(lv, SVt_PVLV);
	    LvTYPE(lv) = 'y';
	    sv_magic(lv, NULL, PERL_MAGIC_defelem, NULL, 0);
	    LvTARG(lv) = SvREFCNT_inc_simple(av);
	    LvTARGOFF(lv) = elem;
	    LvTARGLEN(lv) = 1;
	    PUSHs(lv);
	    RETURN;
	}
	if (PL_op->op_private & OPpLVAL_INTRO)
	    save_aelem(av, elem, svp);
	else if (PL_op->op_private & OPpDEREF)
	    vivify_ref(*svp, PL_op->op_private & OPpDEREF);
    }
    sv = (svp ? *svp : &PL_sv_undef);
    if (!lval && SvGMAGICAL(sv))	/* see note in pp_helem() */
	sv = sv_mortalcopy(sv);
    PUSHs(sv);
    RETURN;
}

void
Perl_vivify_ref(pTHX_ SV *sv, U32 to_what)
{
    PERL_ARGS_ASSERT_VIVIFY_REF;

    SvGETMAGIC(sv);
    if (!SvOK(sv)) {
	if (SvREADONLY(sv))
	    Perl_croak(aTHX_ "%s", PL_no_modify);
	prepare_SV_for_RV(sv);
	switch (to_what) {
	case OPpDEREF_SV:
	    SvRV_set(sv, newSV(0));
	    break;
	case OPpDEREF_AV:
	    SvRV_set(sv, MUTABLE_SV(newAV()));
	    break;
	case OPpDEREF_HV:
	    SvRV_set(sv, MUTABLE_SV(newHV()));
	    break;
	}
	SvROK_on(sv);
	SvSETMAGIC(sv);
    }
}

PP(pp_method)
{
    dVAR; dSP;
    SV* const sv = TOPs;

    if (SvROK(sv)) {
	SV* const rsv = SvRV(sv);
	if (SvTYPE(rsv) == SVt_PVCV) {
	    SETs(rsv);
	    RETURN;
	}
    }

    SETs(method_common(sv, NULL));
    RETURN;
}

PP(pp_method_named)
{
    dVAR; dSP;
    SV* const sv = cSVOP_sv;
    U32 hash = SvSHARED_HASH(sv);

    XPUSHs(method_common(sv, &hash));
    RETURN;
}

STATIC SV *
S_method_common(pTHX_ SV* meth, U32* hashp)
{
    dVAR;
    SV* ob;
    GV* gv;
    HV* stash;
    STRLEN namelen;
    const char* packname = NULL;
    SV *packsv = NULL;
    STRLEN packlen;
    const char * const name = SvPV_const(meth, namelen);
    SV * const sv = *(PL_stack_base + TOPMARK + 1);

    PERL_ARGS_ASSERT_METHOD_COMMON;

    if (!sv)
	Perl_croak(aTHX_ "Can't call method \"%s\" on an undefined value", name);

    SvGETMAGIC(sv);
    if (SvROK(sv))
	ob = MUTABLE_SV(SvRV(sv));
    else {
	GV* iogv;

	/* this isn't a reference */
        if(SvOK(sv) && (packname = SvPV_const(sv, packlen))) {
          const HE* const he = hv_fetch_ent(PL_stashcache, sv, 0, 0);
          if (he) { 
            stash = INT2PTR(HV*,SvIV(HeVAL(he)));
            goto fetch;
          }
        }

	if (!SvOK(sv) ||
	    !(packname) ||
	    !(iogv = gv_fetchsv(sv, 0, SVt_PVIO)) ||
	    !(ob=MUTABLE_SV(GvIO(iogv))))
	{
	    /* this isn't the name of a filehandle either */
	    if (!packname ||
		((UTF8_IS_START(*packname) && DO_UTF8(sv))
		    ? !isIDFIRST_utf8((U8*)packname)
		    : !isIDFIRST(*packname)
		))
	    {
		Perl_croak(aTHX_ "Can't call method \"%s\" %s", name,
			   SvOK(sv) ? "without a package or object reference"
				    : "on an undefined value");
	    }
	    /* assume it's a package name */
	    stash = gv_stashpvn(packname, packlen, 0);
	    if (!stash)
		packsv = sv;
            else {
	        SV* const ref = newSViv(PTR2IV(stash));
	        (void)hv_store(PL_stashcache, packname, packlen, ref, 0);
	    }
	    goto fetch;
	}
	/* it _is_ a filehandle name -- replace with a reference */
	*(PL_stack_base + TOPMARK + 1) = sv_2mortal(newRV(MUTABLE_SV(iogv)));
    }

    /* if we got here, ob should be a reference or a glob */
    if (!ob || !(SvOBJECT(ob)
		 || (SvTYPE(ob) == SVt_PVGV 
		     && isGV_with_GP(ob)
		     && (ob = MUTABLE_SV(GvIO((const GV *)ob)))
		     && SvOBJECT(ob))))
    {
	Perl_croak(aTHX_ "Can't call method \"%s\" on unblessed reference",
		   (SvSCREAM(meth) && strEQ(name,"isa")) ? "DOES" :
		   name);
    }

    stash = SvSTASH(ob);

  fetch:
    /* NOTE: stash may be null, hope hv_fetch_ent and
       gv_fetchmethod can cope (it seems they can) */

    /* shortcut for simple names */
    if (hashp) {
	const HE* const he = hv_fetch_ent(stash, meth, 0, *hashp);
	if (he) {
	    gv = MUTABLE_GV(HeVAL(he));
	    if (isGV(gv) && GvCV(gv) &&
		(!GvCVGEN(gv) || GvCVGEN(gv)
                  == (PL_sub_generation + HvMROMETA(stash)->cache_gen)))
		return MUTABLE_SV(GvCV(gv));
	}
    }

    gv = gv_fetchmethod(stash ? stash : MUTABLE_HV(packsv), name);

    if (!gv) {
	/* This code tries to figure out just what went wrong with
	   gv_fetchmethod.  It therefore needs to duplicate a lot of
	   the internals of that function.  We can't move it inside
	   Perl_gv_fetchmethod_autoload(), however, since that would
	   cause UNIVERSAL->can("NoSuchPackage::foo") to croak, and we
	   don't want that.
	*/
	const char* leaf = name;
	const char* sep = NULL;
	const char* p;

	for (p = name; *p; p++) {
	    if (*p == '\'')
		sep = p, leaf = p + 1;
	    else if (*p == ':' && *(p + 1) == ':')
		sep = p, leaf = p + 2;
	}
	if (!sep || ((sep - name) == 5 && strnEQ(name, "SUPER", 5))) {
	    /* the method name is unqualified or starts with SUPER:: */
#ifndef USE_ITHREADS
	    if (sep)
		stash = CopSTASH(PL_curcop);
#else
	    bool need_strlen = 1;
	    if (sep) {
		packname = CopSTASHPV(PL_curcop);
	    }
	    else
#endif
	    if (stash) {
		HEK * const packhek = HvNAME_HEK(stash);
		if (packhek) {
		    packname = HEK_KEY(packhek);
		    packlen = HEK_LEN(packhek);
#ifdef USE_ITHREADS
		    need_strlen = 0;
#endif
		} else {
		    goto croak;
		}
	    }

	    if (!packname) {
	    croak:
		Perl_croak(aTHX_
			   "Can't use anonymous symbol table for method lookup");
	    }
#ifdef USE_ITHREADS
	    if (need_strlen)
		packlen = strlen(packname);
#endif

	}
	else {
	    /* the method name is qualified */
	    packname = name;
	    packlen = sep - name;
	}
	
	/* we're relying on gv_fetchmethod not autovivifying the stash */
	if (gv_stashpvn(packname, packlen, 0)) {
	    Perl_croak(aTHX_
		       "Can't locate object method \"%s\" via package \"%.*s\"",
		       leaf, (int)packlen, packname);
	}
	else {
	    Perl_croak(aTHX_
		       "Can't locate object method \"%s\" via package \"%.*s\""
		       " (perhaps you forgot to load \"%.*s\"?)",
		       leaf, (int)packlen, packname, (int)packlen, packname);
	}
    }
    return isGV(gv) ? MUTABLE_SV(GvCV(gv)) : MUTABLE_SV(gv);
}

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */

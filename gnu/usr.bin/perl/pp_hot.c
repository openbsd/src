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
    dSP;
    XPUSHs(cSVOP_sv);
    RETURN;
}

PP(pp_nextstate)
{
    PL_curcop = (COP*)PL_op;
    TAINT_NOT;		/* Each statement is presumed innocent */
    PL_stack_sp = PL_stack_base + CX_CUR()->blk_oldsp;
    FREETMPS;
    PERL_ASYNC_CHECK();
    return NORMAL;
}

PP(pp_gvsv)
{
    dSP;
    EXTEND(SP,1);
    if (UNLIKELY(PL_op->op_private & OPpLVAL_INTRO))
	PUSHs(save_scalar(cGVOP_gv));
    else
	PUSHs(GvSVn(cGVOP_gv));
    RETURN;
}


/* also used for: pp_lineseq() pp_regcmaybe() pp_scalar() pp_scope() */

PP(pp_null)
{
    return NORMAL;
}

/* This is sometimes called directly by pp_coreargs, pp_grepstart and
   amagic_call. */
PP(pp_pushmark)
{
    PUSHMARK(PL_stack_sp);
    return NORMAL;
}

PP(pp_stringify)
{
    dSP; dTARGET;
    SV * const sv = TOPs;
    SETs(TARG);
    sv_copypv(TARG, sv);
    SvSETMAGIC(TARG);
    /* no PUTBACK, SETs doesn't inc/dec SP */
    return NORMAL;
}

PP(pp_gv)
{
    dSP;
    XPUSHs(MUTABLE_SV(cGVOP_gv));
    RETURN;
}


/* also used for: pp_andassign() */

PP(pp_and)
{
    PERL_ASYNC_CHECK();
    {
	/* SP is not used to remove a variable that is saved across the
	  sv_2bool_flags call in SvTRUE_NN, if a RISC/CISC or low/high machine
	  register or load/store vs direct mem ops macro is introduced, this
	  should be a define block between direct PL_stack_sp and dSP operations,
	  presently, using PL_stack_sp is bias towards CISC cpus */
	SV * const sv = *PL_stack_sp;
	if (!SvTRUE_NN(sv))
	    return NORMAL;
	else {
	    if (PL_op->op_type == OP_AND)
		--PL_stack_sp;
	    return cLOGOP->op_other;
	}
    }
}

PP(pp_sassign)
{
    dSP;
    /* sassign keeps its args in the optree traditionally backwards.
       So we pop them differently.
    */
    SV *left = POPs; SV *right = TOPs;

    if (PL_op->op_private & OPpASSIGN_BACKWARDS) {
	SV * const temp = left;
	left = right; right = temp;
    }
    assert(TAINTING_get || !TAINT_get);
    if (UNLIKELY(TAINT_get) && !SvTAINTED(right))
	TAINT_NOT;
    if (UNLIKELY(PL_op->op_private & OPpASSIGN_CV_TO_GV)) {
        /* *foo =\&bar */
	SV * const cv = SvRV(right);
	const U32 cv_type = SvTYPE(cv);
	const bool is_gv = isGV_with_GP(left);
	const bool got_coderef = cv_type == SVt_PVCV || cv_type == SVt_PVFM;

	if (!got_coderef) {
	    assert(SvROK(cv));
	}

	/* Can do the optimisation if left (LVALUE) is not a typeglob,
	   right (RVALUE) is a reference to something, and we're in void
	   context. */
	if (!got_coderef && !is_gv && GIMME_V == G_VOID) {
	    /* Is the target symbol table currently empty?  */
	    GV * const gv = gv_fetchsv_nomg(left, GV_NOINIT, SVt_PVGV);
	    if (SvTYPE(gv) != SVt_PVGV && !SvOK(gv)) {
		/* Good. Create a new proxy constant subroutine in the target.
		   The gv becomes a(nother) reference to the constant.  */
		SV *const value = SvRV(cv);

		SvUPGRADE(MUTABLE_SV(gv), SVt_IV);
		SvPCS_IMPORTED_on(gv);
		SvRV_set(gv, value);
		SvREFCNT_inc_simple_void(value);
		SETs(left);
		RETURN;
	    }
	}

	/* Need to fix things up.  */
	if (!is_gv) {
	    /* Need to fix GV.  */
	    left = MUTABLE_SV(gv_fetchsv_nomg(left,GV_ADD, SVt_PVGV));
	}

	if (!got_coderef) {
	    /* We've been returned a constant rather than a full subroutine,
	       but they expect a subroutine reference to apply.  */
	    if (SvROK(cv)) {
		ENTER_with_name("sassign_coderef");
		SvREFCNT_inc_void(SvRV(cv));
		/* newCONSTSUB takes a reference count on the passed in SV
		   from us.  We set the name to NULL, otherwise we get into
		   all sorts of fun as the reference to our new sub is
		   donated to the GV that we're about to assign to.
		*/
		SvRV_set(right, MUTABLE_SV(newCONSTSUB(GvSTASH(left), NULL,
						      SvRV(cv))));
		SvREFCNT_dec_NN(cv);
		LEAVE_with_name("sassign_coderef");
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

		SvREFCNT_inc_simple_void_NN(source);
		SvREFCNT_dec_NN(upgraded);
		SvRV_set(right, MUTABLE_SV(source));
	    }
	}

    }
    if (
      UNLIKELY(SvTEMP(left)) && !SvSMAGICAL(left) && SvREFCNT(left) == 1 &&
      (!isGV_with_GP(left) || SvFAKE(left)) && ckWARN(WARN_MISC)
    )
	Perl_warner(aTHX_
	    packWARN(WARN_MISC), "Useless assignment to a temporary"
	);
    SvSetMagicSV(left, right);
    SETs(left);
    RETURN;
}

PP(pp_cond_expr)
{
    dSP;
    PERL_ASYNC_CHECK();
    if (SvTRUEx(POPs))
	RETURNOP(cLOGOP->op_other);
    else
	RETURNOP(cLOGOP->op_next);
}

PP(pp_unstack)
{
    PERL_CONTEXT *cx;
    PERL_ASYNC_CHECK();
    TAINT_NOT;		/* Each statement is presumed innocent */
    cx  = CX_CUR();
    PL_stack_sp = PL_stack_base + cx->blk_oldsp;
    FREETMPS;
    if (!(PL_op->op_flags & OPf_SPECIAL)) {
        assert(CxTYPE(cx) == CXt_BLOCK || CxTYPE_is_LOOP(cx));
	CX_LEAVE_SCOPE(cx);
    }
    return NORMAL;
}

PP(pp_concat)
{
  dSP; dATARGET; tryAMAGICbin_MG(concat_amg, AMGf_assign);
  {
    dPOPTOPssrl;
    bool lbyte;
    STRLEN rlen;
    const char *rpv = NULL;
    bool rbyte = FALSE;
    bool rcopied = FALSE;

    if (TARG == right && right != left) { /* $r = $l.$r */
	rpv = SvPV_nomg_const(right, rlen);
	rbyte = !DO_UTF8(right);
	right = newSVpvn_flags(rpv, rlen, SVs_TEMP);
	rpv = SvPV_const(right, rlen);	/* no point setting UTF-8 here */
	rcopied = TRUE;
    }

    if (TARG != left) { /* not $l .= $r */
        STRLEN llen;
        const char* const lpv = SvPV_nomg_const(left, llen);
	lbyte = !DO_UTF8(left);
	sv_setpvn(TARG, lpv, llen);
	if (!lbyte)
	    SvUTF8_on(TARG);
	else
	    SvUTF8_off(TARG);
    }
    else { /* $l .= $r   and   left == TARG */
	if (!SvOK(left)) {
	    if (left == right && ckWARN(WARN_UNINITIALIZED)) /* $l .= $l */
		report_uninit(right);
	    sv_setpvs(left, "");
	}
        else {
            SvPV_force_nomg_nolen(left);
        }
	lbyte = !DO_UTF8(left);
	if (IN_BYTES)
	    SvUTF8_off(left);
    }

    if (!rcopied) {
	rpv = SvPV_nomg_const(right, rlen);
	rbyte = !DO_UTF8(right);
    }
    if (lbyte != rbyte) {
	if (lbyte)
	    sv_utf8_upgrade_nomg(TARG);
	else {
	    if (!rcopied)
		right = newSVpvn_flags(rpv, rlen, SVs_TEMP);
	    sv_utf8_upgrade_nomg(right);
	    rpv = SvPV_nomg_const(right, rlen);
	}
    }
    sv_catpvn_nomg(TARG, rpv, rlen);

    SETTARG;
    RETURN;
  }
}

/* push the elements of av onto the stack.
 * XXX Note that padav has similar code but without the mg_get().
 * I suspect that the mg_get is no longer needed, but while padav
 * differs, it can't share this function */

STATIC void
S_pushav(pTHX_ AV* const av)
{
    dSP;
    const SSize_t maxarg = AvFILL(av) + 1;
    EXTEND(SP, maxarg);
    if (UNLIKELY(SvRMAGICAL(av))) {
        PADOFFSET i;
        for (i=0; i < (PADOFFSET)maxarg; i++) {
            SV ** const svp = av_fetch(av, i, FALSE);
            /* See note in pp_helem, and bug id #27839 */
            SP[i+1] = svp
                ? SvGMAGICAL(*svp) ? (mg_get(*svp), *svp) : *svp
                : &PL_sv_undef;
        }
    }
    else {
        PADOFFSET i;
        for (i=0; i < (PADOFFSET)maxarg; i++) {
            SV * const sv = AvARRAY(av)[i];
            SP[i+1] = LIKELY(sv) ? sv : &PL_sv_undef;
        }
    }
    SP += maxarg;
    PUTBACK;
}


/* ($lex1,@lex2,...)   or my ($lex1,@lex2,...)  */

PP(pp_padrange)
{
    dSP;
    PADOFFSET base = PL_op->op_targ;
    int count = (int)(PL_op->op_private) & OPpPADRANGE_COUNTMASK;
    int i;
    if (PL_op->op_flags & OPf_SPECIAL) {
        /* fake the RHS of my ($x,$y,..) = @_ */
        PUSHMARK(SP);
        S_pushav(aTHX_ GvAVn(PL_defgv));
        SPAGAIN;
    }

    /* note, this is only skipped for compile-time-known void cxt */
    if ((PL_op->op_flags & OPf_WANT) != OPf_WANT_VOID) {
        EXTEND(SP, count);
        PUSHMARK(SP);
        for (i = 0; i <count; i++)
            *++SP = PAD_SV(base+i);
    }
    if (PL_op->op_private & OPpLVAL_INTRO) {
        SV **svp = &(PAD_SVl(base));
        const UV payload = (UV)(
                      (base << (OPpPADRANGE_COUNTSHIFT + SAVE_TIGHT_SHIFT))
                    | (count << SAVE_TIGHT_SHIFT)
                    | SAVEt_CLEARPADRANGE);
        STATIC_ASSERT_STMT(OPpPADRANGE_COUNTMASK + 1 == (1 << OPpPADRANGE_COUNTSHIFT));
        assert((payload >> (OPpPADRANGE_COUNTSHIFT+SAVE_TIGHT_SHIFT)) == base);
        {
            dSS_ADD;
            SS_ADD_UV(payload);
            SS_ADD_END(1);
        }

        for (i = 0; i <count; i++)
            SvPADSTALE_off(*svp++); /* mark lexical as active */
    }
    RETURN;
}


PP(pp_padsv)
{
    dSP;
    EXTEND(SP, 1);
    {
	OP * const op = PL_op;
	/* access PL_curpad once */
	SV ** const padentry = &(PAD_SVl(op->op_targ));
	{
	    dTARG;
	    TARG = *padentry;
	    PUSHs(TARG);
	    PUTBACK; /* no pop/push after this, TOPs ok */
	}
	if (op->op_flags & OPf_MOD) {
	    if (op->op_private & OPpLVAL_INTRO)
		if (!(op->op_private & OPpPAD_STATE))
		    save_clearsv(padentry);
	    if (op->op_private & OPpDEREF) {
		/* TOPs is equivalent to TARG here.  Using TOPs (SP) rather
		   than TARG reduces the scope of TARG, so it does not
		   span the call to save_clearsv, resulting in smaller
		   machine code. */
		TOPs = vivify_ref(TOPs, op->op_private & OPpDEREF);
	    }
	}
	return op->op_next;
    }
}

PP(pp_readline)
{
    dSP;
    if (TOPs) {
	SvGETMAGIC(TOPs);
	tryAMAGICunTARGETlist(iter_amg, 0);
	PL_last_in_gv = MUTABLE_GV(*PL_stack_sp--);
    }
    else PL_last_in_gv = PL_argvgv, PL_stack_sp--;
    if (!isGV_with_GP(PL_last_in_gv)) {
	if (SvROK(PL_last_in_gv) && isGV_with_GP(SvRV(PL_last_in_gv)))
	    PL_last_in_gv = MUTABLE_GV(SvRV(PL_last_in_gv));
	else {
	    dSP;
	    XPUSHs(MUTABLE_SV(PL_last_in_gv));
	    PUTBACK;
	    Perl_pp_rv2gv(aTHX);
	    PL_last_in_gv = MUTABLE_GV(*PL_stack_sp--);
	    if (PL_last_in_gv == (GV *)&PL_sv_undef)
		PL_last_in_gv = NULL;
	    else
		assert(isGV_with_GP(PL_last_in_gv));
	}
    }
    return do_readline();
}

PP(pp_eq)
{
    dSP;
    SV *left, *right;

    tryAMAGICbin_MG(eq_amg, AMGf_set|AMGf_numeric);
    right = POPs;
    left  = TOPs;
    SETs(boolSV(
	(SvIOK_notUV(left) && SvIOK_notUV(right))
	? (SvIVX(left) == SvIVX(right))
	: ( do_ncmp(left, right) == 0)
    ));
    RETURN;
}


/* also used for: pp_i_preinc() */

PP(pp_preinc)
{
    SV *sv = *PL_stack_sp;

    if (LIKELY(((sv->sv_flags &
                        (SVf_THINKFIRST|SVs_GMG|SVf_IVisUV|
                         SVf_IOK|SVf_NOK|SVf_POK|SVp_NOK|SVp_POK|SVf_ROK))
                == SVf_IOK))
        && SvIVX(sv) != IV_MAX)
    {
	SvIV_set(sv, SvIVX(sv) + 1);
    }
    else /* Do all the PERL_PRESERVE_IVUV and hard cases in sv_inc */
	sv_inc(sv);
    SvSETMAGIC(sv);
    return NORMAL;
}


/* also used for: pp_i_predec() */

PP(pp_predec)
{
    SV *sv = *PL_stack_sp;

    if (LIKELY(((sv->sv_flags &
                        (SVf_THINKFIRST|SVs_GMG|SVf_IVisUV|
                         SVf_IOK|SVf_NOK|SVf_POK|SVp_NOK|SVp_POK|SVf_ROK))
                == SVf_IOK))
        && SvIVX(sv) != IV_MIN)
    {
	SvIV_set(sv, SvIVX(sv) - 1);
    }
    else /* Do all the PERL_PRESERVE_IVUV and hard cases  in sv_dec */
	sv_dec(sv);
    SvSETMAGIC(sv);
    return NORMAL;
}


/* also used for: pp_orassign() */

PP(pp_or)
{
    dSP;
    PERL_ASYNC_CHECK();
    if (SvTRUE(TOPs))
	RETURN;
    else {
	if (PL_op->op_type == OP_OR)
            --SP;
	RETURNOP(cLOGOP->op_other);
    }
}


/* also used for: pp_dor() pp_dorassign() */

PP(pp_defined)
{
    dSP;
    SV* sv;
    bool defined;
    const int op_type = PL_op->op_type;
    const bool is_dor = (op_type == OP_DOR || op_type == OP_DORASSIGN);

    if (is_dor) {
	PERL_ASYNC_CHECK();
        sv = TOPs;
        if (UNLIKELY(!sv || !SvANY(sv))) {
	    if (op_type == OP_DOR)
		--SP;
            RETURNOP(cLOGOP->op_other);
        }
    }
    else {
	/* OP_DEFINED */
        sv = POPs;
        if (UNLIKELY(!sv || !SvANY(sv)))
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
    dSP; dATARGET; bool useleft; SV *svl, *svr;

    tryAMAGICbin_MG(add_amg, AMGf_assign|AMGf_numeric);
    svr = TOPs;
    svl = TOPm1s;

#ifdef PERL_PRESERVE_IVUV

    /* special-case some simple common cases */
    if (!((svl->sv_flags|svr->sv_flags) & (SVf_IVisUV|SVs_GMG))) {
        IV il, ir;
        U32 flags = (svl->sv_flags & svr->sv_flags);
        if (flags & SVf_IOK) {
            /* both args are simple IVs */
            UV topl, topr;
            il = SvIVX(svl);
            ir = SvIVX(svr);
          do_iv:
            topl = ((UV)il) >> (UVSIZE * 8 - 2);
            topr = ((UV)ir) >> (UVSIZE * 8 - 2);

            /* if both are in a range that can't under/overflow, do a
             * simple integer add: if the top of both numbers
             * are 00  or 11, then it's safe */
            if (!( ((topl+1) | (topr+1)) & 2)) {
                SP--;
                TARGi(il + ir, 0); /* args not GMG, so can't be tainted */
                SETs(TARG);
                RETURN;
            }
            goto generic;
        }
        else if (flags & SVf_NOK) {
            /* both args are NVs */
            NV nl = SvNVX(svl);
            NV nr = SvNVX(svr);

            if (
#if defined(NAN_COMPARE_BROKEN) && defined(Perl_isnan)
                !Perl_isnan(nl) && nl == (NV)(il = (IV)nl)
                && !Perl_isnan(nr) && nr == (NV)(ir = (IV)nr)
#else
                nl == (NV)(il = (IV)nl) && nr == (NV)(ir = (IV)nr)
#endif
                )
                /* nothing was lost by converting to IVs */
                goto do_iv;
            SP--;
            TARGn(nl + nr, 0); /* args not GMG, so can't be tainted */
            SETs(TARG);
            RETURN;
        }
    }

  generic:

    useleft = USE_LEFT(svl);
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

    if (SvIV_please_nomg(svr)) {
	/* Unless the left argument is integer in range we are going to have to
	   use NV maths. Hence only attempt to coerce the right argument if
	   we know the left is integer.  */
	UV auv = 0;
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
	    if (SvIV_please_nomg(svl)) {
		if ((auvok = SvUOK(svl)))
		    auv = SvUVX(svl);
		else {
		    const IV aiv = SvIVX(svl);
		    if (aiv >= 0) {
			auv = aiv;
			auvok = 1;	/* Now acting as a sign flag.  */
		    } else {
			auv = (aiv == IV_MIN) ? (UV)aiv : (UV)(-aiv);
		    }
		}
		a_valid = 1;
	    }
	}
	if (a_valid) {
	    bool result_good = 0;
	    UV result;
	    UV buv;
	    bool buvok = SvUOK(svr);
	
	    if (buvok)
		buv = SvUVX(svr);
	    else {
		const IV biv = SvIVX(svr);
		if (biv >= 0) {
		    buv = biv;
		    buvok = 1;
		} else
                    buv = (biv == IV_MIN) ? (UV)biv : (UV)(-biv);
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
                        SETi(result == (UV)IV_MIN
                                ? IV_MIN : -(IV)result);
		    else {
			/* result valid, but out of range for IV.  */
			SETn( -(NV)result );
		    }
		}
		RETURN;
	    } /* Overflow, drop through to NVs.  */
	}
    }

#else
    useleft = USE_LEFT(svl);
#endif

    {
	NV value = SvNV_nomg(svr);
	(void)POPs;
	if (!useleft) {
	    /* left operand is undef, treat as zero. + 0.0 is identity. */
	    SETn(value);
	    RETURN;
	}
	SETn( value + SvNV_nomg(svl) );
	RETURN;
    }
}


/* also used for: pp_aelemfast_lex() */

PP(pp_aelemfast)
{
    dSP;
    AV * const av = PL_op->op_type == OP_AELEMFAST_LEX
	? MUTABLE_AV(PAD_SV(PL_op->op_targ)) : GvAVn(cGVOP_gv);
    const U32 lval = PL_op->op_flags & OPf_MOD;
    SV** const svp = av_fetch(av, (I8)PL_op->op_private, lval);
    SV *sv = (svp ? *svp : &PL_sv_undef);

    if (UNLIKELY(!svp && lval))
        DIE(aTHX_ PL_no_aelem, (int)(I8)PL_op->op_private);

    EXTEND(SP, 1);
    if (!lval && SvRMAGICAL(av) && SvGMAGICAL(sv)) /* see note in pp_helem() */
	mg_get(sv);
    PUSHs(sv);
    RETURN;
}

PP(pp_join)
{
    dSP; dMARK; dTARGET;
    MARK++;
    do_join(TARG, *MARK, MARK, SP);
    SP = MARK;
    SETs(TARG);
    RETURN;
}

PP(pp_pushre)
{
    dSP;
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

/* also used for: pp_say() */

PP(pp_print)
{
    dSP; dMARK; dORIGMARK;
    PerlIO *fp;
    MAGIC *mg;
    GV * const gv
	= (PL_op->op_flags & OPf_STACKED) ? MUTABLE_GV(*++MARK) : PL_defoutgv;
    IO *io = GvIO(gv);

    if (io
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
	return Perl_tied_method(aTHX_ SV_CONST(PRINT), mark - 1, MUTABLE_SV(io),
				mg,
				(G_SCALAR | TIED_METHOD_ARGUMENTS_ON_STACK
				 | (PL_op->op_type == OP_SAY
				    ? TIED_METHOD_SAY : 0)), sp - mark);
    }
    if (!io) {
        if ( gv && GvEGVx(gv) && (io = GvIO(GvEGV(gv)))
	    && (mg = SvTIED_mg((const SV *)io, PERL_MAGIC_tiedscalar)))
            goto had_magic;
	report_evil_fh(gv);
	SETERRNO(EBADF,RMS_IFI);
	goto just_say_no;
    }
    else if (!(fp = IoOFP(io))) {
	if (IoIFP(io))
	    report_wrongway_fh(gv, '<');
	else
	    report_evil_fh(gv);
	SETERRNO(EBADF,IoIFP(io)?RMS_FAC:RMS_IFI);
	goto just_say_no;
    }
    else {
	SV * const ofs = GvSV(PL_ofsgv); /* $, */
	MARK++;
	if (ofs && (SvGMAGICAL(ofs) || SvOK(ofs))) {
	    while (MARK <= SP) {
		if (!do_print(*MARK, fp))
		    break;
		MARK++;
		if (MARK <= SP) {
		    /* don't use 'ofs' here - it may be invalidated by magic callbacks */
		    if (!do_print(GvSV(PL_ofsgv), fp)) {
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


/* also used for: pp_rv2hv() */
/* also called directly by pp_lvavref */

PP(pp_rv2av)
{
    dSP; dTOPss;
    const U8 gimme = GIMME_V;
    static const char an_array[] = "an ARRAY";
    static const char a_hash[] = "a HASH";
    const bool is_pp_rv2av = PL_op->op_type == OP_RV2AV
			  || PL_op->op_type == OP_LVAVREF;
    const svtype type = is_pp_rv2av ? SVt_PVAV : SVt_PVHV;

    SvGETMAGIC(sv);
    if (SvROK(sv)) {
	if (UNLIKELY(SvAMAGIC(sv))) {
	    sv = amagic_deref_call(sv, is_pp_rv2av ? to_av_amg : to_hv_amg);
	}
	sv = SvRV(sv);
	if (UNLIKELY(SvTYPE(sv) != type))
	    /* diag_listed_as: Not an ARRAY reference */
	    DIE(aTHX_ "Not %s reference", is_pp_rv2av ? an_array : a_hash);
	else if (UNLIKELY(PL_op->op_flags & OPf_MOD
		&& PL_op->op_private & OPpLVAL_INTRO))
	    Perl_croak(aTHX_ "%s", PL_no_localize_ref);
    }
    else if (UNLIKELY(SvTYPE(sv) != type)) {
	    GV *gv;
	
	    if (!isGV_with_GP(sv)) {
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
    }
    if (PL_op->op_flags & OPf_REF) {
		SETs(sv);
		RETURN;
    }
    else if (UNLIKELY(PL_op->op_private & OPpMAYBE_LVSUB)) {
	      const I32 flags = is_lvalue_sub();
	      if (flags && !(flags & OPpENTERSUB_INARGS)) {
		if (gimme != G_ARRAY)
		    goto croak_cant_return;
		SETs(sv);
		RETURN;
	      }
    }

    if (is_pp_rv2av) {
	AV *const av = MUTABLE_AV(sv);
	/* The guts of pp_rv2av  */
	if (gimme == G_ARRAY) {
            SP--;
            PUTBACK;
            S_pushav(aTHX_ av);
            SPAGAIN;
	}
	else if (gimme == G_SCALAR) {
	    dTARGET;
	    const SSize_t maxarg = AvFILL(av) + 1;
	    SETi(maxarg);
	}
    } else {
	/* The guts of pp_rv2hv  */
	if (gimme == G_ARRAY) { /* array wanted */
	    *PL_stack_sp = sv;
	    return Perl_do_kv(aTHX);
	}
	else if ((PL_op->op_private & OPpTRUEBOOL
	      || (  PL_op->op_private & OPpMAYBE_TRUEBOOL
		 && block_gimme() == G_VOID  ))
	      && (!SvRMAGICAL(sv) || !mg_find(sv, PERL_MAGIC_tied)))
	    SETs(HvUSEDKEYS(sv) ? &PL_sv_yes : sv_2mortal(newSViv(0)));
	else if (gimme == G_SCALAR) {
	    dTARG;
	    TARG = Perl_hv_scalar(aTHX_ MUTABLE_HV(sv));
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
S_do_oddball(pTHX_ SV **oddkey, SV **firstkey)
{
    PERL_ARGS_ASSERT_DO_ODDBALL;

    if (*oddkey) {
        if (ckWARN(WARN_MISC)) {
	    const char *err;
	    if (oddkey == firstkey &&
		SvROK(*oddkey) &&
		(SvTYPE(SvRV(*oddkey)) == SVt_PVAV ||
		 SvTYPE(SvRV(*oddkey)) == SVt_PVHV))
	    {
		err = "Reference found where even-sized list expected";
	    }
	    else
		err = "Odd number of elements in hash assignment";
	    Perl_warner(aTHX_ packWARN(WARN_MISC), "%s", err);
	}

    }
}


/* Do a mark and sweep with the SVf_BREAK flag to detect elements which
 * are common to both the LHS and RHS of an aassign, and replace them
 * with copies. All these copies are made before the actual list assign is
 * done.
 *
 * For example in ($a,$b) = ($b,$a), assigning the value of the first RHS
 * element ($b) to the first LH element ($a), modifies $a; when the
 * second assignment is done, the second RH element now has the wrong
 * value. So we initially replace the RHS with ($b, mortalcopy($a)).
 * Note that we don't need to make a mortal copy of $b.
 *
 * The algorithm below works by, for every RHS element, mark the
 * corresponding LHS target element with SVf_BREAK. Then if the RHS
 * element is found with SVf_BREAK set, it means it would have been
 * modified, so make a copy.
 * Note that by scanning both LHS and RHS in lockstep, we avoid
 * unnecessary copies (like $b above) compared with a naive
 * "mark all LHS; copy all marked RHS; unmark all LHS".
 *
 * If the LHS element is a 'my' declaration' and has a refcount of 1, then
 * it can't be common and can be skipped.
 *
 * On DEBUGGING builds it takes an extra boolean, fake. If true, it means
 * that we thought we didn't need to call S_aassign_copy_common(), but we
 * have anyway for sanity checking. If we find we need to copy, then panic.
 */

PERL_STATIC_INLINE void
S_aassign_copy_common(pTHX_ SV **firstlelem, SV **lastlelem,
        SV **firstrelem, SV **lastrelem
#ifdef DEBUGGING
        , bool fake
#endif
)
{
    dVAR;
    SV **relem;
    SV **lelem;
    SSize_t lcount = lastlelem - firstlelem + 1;
    bool marked = FALSE; /* have we marked any LHS with SVf_BREAK ? */
    bool const do_rc1 = cBOOL(PL_op->op_private & OPpASSIGN_COMMON_RC1);
    bool copy_all = FALSE;

    assert(!PL_in_clean_all); /* SVf_BREAK not already in use */
    assert(firstlelem < lastlelem); /* at least 2 LH elements */
    assert(firstrelem < lastrelem); /* at least 2 RH elements */


    lelem = firstlelem;
    /* we never have to copy the first RH element; it can't be corrupted
     * by assigning something to the corresponding first LH element.
     * So this scan does in a loop: mark LHS[N]; test RHS[N+1]
     */
    relem = firstrelem + 1;

    for (; relem <= lastrelem; relem++) {
        SV *svr;

        /* mark next LH element */

        if (--lcount >= 0) {
            SV *svl = *lelem++;

            if (UNLIKELY(!svl)) {/* skip AV alias marker */
                assert (lelem <= lastlelem);
                svl = *lelem++;
                lcount--;
            }

            assert(svl);
            if (SvSMAGICAL(svl)) {
                copy_all = TRUE;
            }
            if (SvTYPE(svl) == SVt_PVAV || SvTYPE(svl) == SVt_PVHV) {
                if (!marked)
                    return;
                /* this LH element will consume all further args;
                 * no need to mark any further LH elements (if any).
                 * But we still need to scan any remaining RHS elements;
                 * set lcount negative to distinguish from  lcount == 0,
                 * so the loop condition continues being true
                 */
                lcount = -1;
                lelem--; /* no need to unmark this element */
            }
            else if (!(do_rc1 && SvREFCNT(svl) == 1) && svl != &PL_sv_undef) {
                assert(!SvIMMORTAL(svl));
                SvFLAGS(svl) |= SVf_BREAK;
                marked = TRUE;
            }
            else if (!marked) {
                /* don't check RH element if no SVf_BREAK flags set yet */
                if (!lcount)
                    break;
                continue;
            }
        }

        /* see if corresponding RH element needs copying */

        assert(marked);
        svr = *relem;
        assert(svr);

        if (UNLIKELY(SvFLAGS(svr) & (SVf_BREAK|SVs_GMG) || copy_all)) {

#ifdef DEBUGGING
            if (fake) {
                /* op_dump(PL_op); */
                Perl_croak(aTHX_
                    "panic: aassign skipped needed copy of common RH elem %"
                        UVuf, (UV)(relem - firstrelem));
            }
#endif

            TAINT_NOT;	/* Each item is independent */

            /* Dear TODO test in t/op/sort.t, I love you.
               (It's relying on a panic, not a "semi-panic" from newSVsv()
               and then an assertion failure below.)  */
            if (UNLIKELY(SvIS_FREED(svr))) {
                Perl_croak(aTHX_ "panic: attempt to copy freed scalar %p",
                           (void*)svr);
            }
            /* avoid break flag while copying; otherwise COW etc
             * disabled... */
            SvFLAGS(svr) &= ~SVf_BREAK;
            /* Not newSVsv(), as it does not allow copy-on-write,
               resulting in wasteful copies.
               Also, we use SV_NOSTEAL in case the SV is used more than
               once, e.g.  (...) = (f())[0,0]
               Where the same SV appears twice on the RHS without a ref
               count bump.  (Although I suspect that the SV won't be
               stealable here anyway - DAPM).
               */
            *relem = sv_mortalcopy_flags(svr,
                                SV_GMAGIC|SV_DO_COW_SVSETSV|SV_NOSTEAL);
            /* ... but restore afterwards in case it's needed again,
             * e.g. ($a,$b,$c) = (1,$a,$a)
             */
            SvFLAGS(svr) |= SVf_BREAK;
        }

        if (!lcount)
            break;
    }

    if (!marked)
        return;

    /*unmark LHS */

    while (lelem > firstlelem) {
        SV * const svl = *(--lelem);
        if (svl)
            SvFLAGS(svl) &= ~SVf_BREAK;
    }
}



PP(pp_aassign)
{
    dVAR; dSP;
    SV **lastlelem = PL_stack_sp;
    SV **lastrelem = PL_stack_base + POPMARK;
    SV **firstrelem = PL_stack_base + POPMARK + 1;
    SV **firstlelem = lastrelem + 1;

    SV **relem;
    SV **lelem;

    SV *sv;
    AV *ary;

    U8 gimme;
    HV *hash;
    SSize_t i;
    int magic;
    U32 lval;
    /* PL_delaymagic is restored by JUMPENV_POP on dieing, so we
     * only need to save locally, not on the save stack */
    U16 old_delaymagic = PL_delaymagic;
#ifdef DEBUGGING
    bool fake = 0;
#endif

    PL_delaymagic = DM_DELAY;		/* catch simultaneous items */

    /* If there's a common identifier on both sides we have to take
     * special care that assigning the identifier on the left doesn't
     * clobber a value on the right that's used later in the list.
     */

    /* at least 2 LH and RH elements, or commonality isn't an issue */
    if (firstlelem < lastlelem && firstrelem < lastrelem) {
        for (relem = firstrelem+1; relem <= lastrelem; relem++) {
            if (SvGMAGICAL(*relem))
                goto do_scan;
        }
        for (lelem = firstlelem; lelem <= lastlelem; lelem++) {
            if (*lelem && SvSMAGICAL(*lelem))
                goto do_scan;
        }
        if ( PL_op->op_private & (OPpASSIGN_COMMON_SCALAR|OPpASSIGN_COMMON_RC1) ) {
            if (PL_op->op_private & OPpASSIGN_COMMON_RC1) {
                /* skip the scan if all scalars have a ref count of 1 */
                for (lelem = firstlelem; lelem <= lastlelem; lelem++) {
                    sv = *lelem;
                    if (!sv || SvREFCNT(sv) == 1)
                        continue;
                    if (SvTYPE(sv) != SVt_PVAV && SvTYPE(sv) != SVt_PVAV)
                        goto do_scan;
                    break;
                }
            }
            else {
            do_scan:
                S_aassign_copy_common(aTHX_
                                      firstlelem, lastlelem, firstrelem, lastrelem
#ifdef DEBUGGING
                    , fake
#endif
                );
            }
        }
    }
#ifdef DEBUGGING
    else {
        /* on debugging builds, do the scan even if we've concluded we
         * don't need to, then panic if we find commonality. Note that the
         * scanner assumes at least 2 elements */
        if (firstlelem < lastlelem && firstrelem < lastrelem) {
            fake = 1;
            goto do_scan;
        }
    }
#endif

    gimme = GIMME_V;
    lval = (gimme == G_ARRAY) ? (PL_op->op_flags & OPf_MOD || LVRET) : 0;

    relem = firstrelem;
    lelem = firstlelem;
    ary = NULL;
    hash = NULL;

    while (LIKELY(lelem <= lastlelem)) {
	bool alias = FALSE;
	TAINT_NOT;		/* Each item stands on its own, taintwise. */
	sv = *lelem++;
	if (UNLIKELY(!sv)) {
	    alias = TRUE;
	    sv = *lelem++;
	    ASSUME(SvTYPE(sv) == SVt_PVAV);
	}
	switch (SvTYPE(sv)) {
	case SVt_PVAV: {
            bool already_copied = FALSE;
	    ary = MUTABLE_AV(sv);
	    magic = SvMAGICAL(ary) != 0;
	    ENTER;
	    SAVEFREESV(SvREFCNT_inc_simple_NN(sv));

            /* We need to clear ary. The is a danger that if we do this,
             * elements on the RHS may be prematurely freed, e.g.
             *   @a = ($a[0]);
             * In the case of possible commonality, make a copy of each
             * RHS SV *before* clearing the array, and add a reference
             * from the tmps stack, so that it doesn't leak on death.
             * Otherwise, make a copy of each RHS SV only as we're storing
             * it into the array - that way we don't have to worry about
             * it being leaked if we die, but don't incur the cost of
             * mortalising everything.
             */

            if (   (PL_op->op_private & OPpASSIGN_COMMON_AGG)
                && (relem <= lastrelem)
                && (magic || AvFILL(ary) != -1))
            {
                SV **svp;
                EXTEND_MORTAL(lastrelem - relem + 1);
                for (svp = relem; svp <= lastrelem; svp++) {
                    /* see comment in S_aassign_copy_common about SV_NOSTEAL */
                    *svp = sv_mortalcopy_flags(*svp,
                            SV_GMAGIC|SV_DO_COW_SVSETSV|SV_NOSTEAL);
                    TAINT_NOT;
                }
                already_copied = TRUE;
            }

            av_clear(ary);
	    if (relem <= lastrelem)
                av_extend(ary, lastrelem - relem);

	    i = 0;
	    while (relem <= lastrelem) {	/* gobble up all the rest */
		SV **didstore;
		if (LIKELY(!alias)) {
                    if (already_copied)
                        sv = *relem;
                    else {
                        if (LIKELY(*relem))
                            /* before newSV, in case it dies */
                            SvGETMAGIC(*relem);
                        sv = newSV(0);
                        /* see comment in S_aassign_copy_common about
                         * SV_NOSTEAL */
                        sv_setsv_flags(sv, *relem,
                                    (SV_DO_COW_SVSETSV|SV_NOSTEAL));
                        *relem = sv;
                    }
		}
		else {
                    if (!already_copied)
                        SvGETMAGIC(*relem);
		    if (!SvROK(*relem))
			DIE(aTHX_ "Assigned value is not a reference");
		    if (SvTYPE(SvRV(*relem)) > SVt_PVLV)
		   /* diag_listed_as: Assigned value is not %s reference */
			DIE(aTHX_
			   "Assigned value is not a SCALAR reference");
		    if (lval && !already_copied)
			*relem = sv_mortalcopy(*relem);
		    /* XXX else check for weak refs?  */
		    sv = SvREFCNT_inc_NN(SvRV(*relem));
		}
		relem++;
                if (already_copied)
                    SvREFCNT_inc_simple_void_NN(sv); /* undo mortal free */
		didstore = av_store(ary,i++,sv);
		if (magic) {
		    if (!didstore)
			sv_2mortal(sv);
		    if (SvSMAGICAL(sv))
			mg_set(sv);
		}
		TAINT_NOT;
	    }
	    if (UNLIKELY(PL_delaymagic & DM_ARRAY_ISA))
		SvSETMAGIC(MUTABLE_SV(ary));
	    LEAVE;
	    break;
        }

	case SVt_PVHV: {				/* normal hash */
		SV *tmpstr;
                int odd;
                int duplicates = 0;
		SV** topelem = relem;
                SV **firsthashrelem = relem;
                bool already_copied = FALSE;

		hash = MUTABLE_HV(sv);
		magic = SvMAGICAL(hash) != 0;

                odd = ((lastrelem - firsthashrelem)&1)? 0 : 1;
                if (UNLIKELY(odd)) {
                    do_oddball(lastrelem, firsthashrelem);
                    /* we have firstlelem to reuse, it's not needed anymore
		     */
                    *(lastrelem+1) = &PL_sv_undef;
                }

		ENTER;
		SAVEFREESV(SvREFCNT_inc_simple_NN(sv));

                /* We need to clear hash. The is a danger that if we do this,
                 * elements on the RHS may be prematurely freed, e.g.
                 *   %h = (foo => $h{bar});
                 * In the case of possible commonality, make a copy of each
                 * RHS SV *before* clearing the hash, and add a reference
                 * from the tmps stack, so that it doesn't leak on death.
                 */

                if (   (PL_op->op_private & OPpASSIGN_COMMON_AGG)
                    && (relem <= lastrelem)
                    && (magic || HvUSEDKEYS(hash)))
                {
                    SV **svp;
                    EXTEND_MORTAL(lastrelem - relem + 1);
                    for (svp = relem; svp <= lastrelem; svp++) {
                        *svp = sv_mortalcopy_flags(*svp,
                                SV_GMAGIC|SV_DO_COW_SVSETSV|SV_NOSTEAL);
                        TAINT_NOT;
                    }
                    already_copied = TRUE;
                }

		hv_clear(hash);

		while (LIKELY(relem < lastrelem+odd)) {	/* gobble up all the rest */
		    HE *didstore;
                    assert(*relem);
		    /* Copy the key if aassign is called in lvalue context,
		       to avoid having the next op modify our rhs.  Copy
		       it also if it is gmagical, lest it make the
		       hv_store_ent call below croak, leaking the value. */
		    sv = (lval || SvGMAGICAL(*relem)) && !already_copied
			 ? sv_mortalcopy(*relem)
			 : *relem;
		    relem++;
                    assert(*relem);
                    if (already_copied)
                        tmpstr = *relem++;
                    else {
                        SvGETMAGIC(*relem);
                        tmpstr = newSV(0);
                        sv_setsv_nomg(tmpstr,*relem++);	/* value */
                    }

		    if (gimme == G_ARRAY) {
			if (hv_exists_ent(hash, sv, 0))
			    /* key overwrites an existing entry */
			    duplicates += 2;
			else {
			    /* copy element back: possibly to an earlier
			     * stack location if we encountered dups earlier,
			     * possibly to a later stack location if odd */
			    *topelem++ = sv;
			    *topelem++ = tmpstr;
			}
		    }
                    if (already_copied)
                        SvREFCNT_inc_simple_void_NN(tmpstr); /* undo mortal free */
		    didstore = hv_store_ent(hash,sv,tmpstr,0);
		    if (magic) {
			if (!didstore) sv_2mortal(tmpstr);
			SvSETMAGIC(tmpstr);
                    }
		    TAINT_NOT;
		}
		LEAVE;
                if (duplicates && gimme == G_ARRAY) {
                    /* at this point we have removed the duplicate key/value
                     * pairs from the stack, but the remaining values may be
                     * wrong; i.e. with (a 1 a 2 b 3) on the stack we've removed
                     * the (a 2), but the stack now probably contains
                     * (a <freed> b 3), because { hv_save(a,1); hv_save(a,2) }
                     * obliterates the earlier key. So refresh all values. */
                    lastrelem -= duplicates;
                    relem = firsthashrelem;
                    while (relem < lastrelem+odd) {
                        HE *he;
                        he = hv_fetch_ent(hash, *relem++, 0, 0);
                        *relem++ = (he ? HeVAL(he) : &PL_sv_undef);
                    }
                }
                if (odd && gimme == G_ARRAY) lastrelem++;
	    }
	    break;
	default:
	    if (SvIMMORTAL(sv)) {
		if (relem <= lastrelem)
		    relem++;
		break;
	    }
	    if (relem <= lastrelem) {
		if (UNLIKELY(
		  SvTEMP(sv) && !SvSMAGICAL(sv) && SvREFCNT(sv) == 1 &&
		  (!isGV_with_GP(sv) || SvFAKE(sv)) && ckWARN(WARN_MISC)
		))
		    Perl_warner(aTHX_
		       packWARN(WARN_MISC),
		      "Useless assignment to a temporary"
		    );
		sv_setsv(sv, *relem);
		*(relem++) = sv;
	    }
	    else
		sv_setsv(sv, &PL_sv_undef);
	    SvSETMAGIC(sv);
	    break;
	}
    }
    if (UNLIKELY(PL_delaymagic & ~DM_DELAY)) {
	/* Will be used to set PL_tainting below */
	Uid_t tmp_uid  = PerlProc_getuid();
	Uid_t tmp_euid = PerlProc_geteuid();
	Gid_t tmp_gid  = PerlProc_getgid();
	Gid_t tmp_egid = PerlProc_getegid();

        /* XXX $> et al currently silently ignore failures */
	if (PL_delaymagic & DM_UID) {
#ifdef HAS_SETRESUID
	    PERL_UNUSED_RESULT(
               setresuid((PL_delaymagic & DM_RUID) ? PL_delaymagic_uid  : (Uid_t)-1,
                         (PL_delaymagic & DM_EUID) ? PL_delaymagic_euid : (Uid_t)-1,
                         (Uid_t)-1));
#else
#  ifdef HAS_SETREUID
            PERL_UNUSED_RESULT(
                setreuid((PL_delaymagic & DM_RUID) ? PL_delaymagic_uid  : (Uid_t)-1,
                         (PL_delaymagic & DM_EUID) ? PL_delaymagic_euid : (Uid_t)-1));
#  else
#    ifdef HAS_SETRUID
	    if ((PL_delaymagic & DM_UID) == DM_RUID) {
		PERL_UNUSED_RESULT(setruid(PL_delaymagic_uid));
		PL_delaymagic &= ~DM_RUID;
	    }
#    endif /* HAS_SETRUID */
#    ifdef HAS_SETEUID
	    if ((PL_delaymagic & DM_UID) == DM_EUID) {
		PERL_UNUSED_RESULT(seteuid(PL_delaymagic_euid));
		PL_delaymagic &= ~DM_EUID;
	    }
#    endif /* HAS_SETEUID */
	    if (PL_delaymagic & DM_UID) {
		if (PL_delaymagic_uid != PL_delaymagic_euid)
		    DIE(aTHX_ "No setreuid available");
		PERL_UNUSED_RESULT(PerlProc_setuid(PL_delaymagic_uid));
	    }
#  endif /* HAS_SETREUID */
#endif /* HAS_SETRESUID */

	    tmp_uid  = PerlProc_getuid();
	    tmp_euid = PerlProc_geteuid();
	}
        /* XXX $> et al currently silently ignore failures */
	if (PL_delaymagic & DM_GID) {
#ifdef HAS_SETRESGID
	    PERL_UNUSED_RESULT(
                setresgid((PL_delaymagic & DM_RGID) ? PL_delaymagic_gid  : (Gid_t)-1,
                          (PL_delaymagic & DM_EGID) ? PL_delaymagic_egid : (Gid_t)-1,
                          (Gid_t)-1));
#else
#  ifdef HAS_SETREGID
	    PERL_UNUSED_RESULT(
                setregid((PL_delaymagic & DM_RGID) ? PL_delaymagic_gid  : (Gid_t)-1,
                         (PL_delaymagic & DM_EGID) ? PL_delaymagic_egid : (Gid_t)-1));
#  else
#    ifdef HAS_SETRGID
	    if ((PL_delaymagic & DM_GID) == DM_RGID) {
		PERL_UNUSED_RESULT(setrgid(PL_delaymagic_gid));
		PL_delaymagic &= ~DM_RGID;
	    }
#    endif /* HAS_SETRGID */
#    ifdef HAS_SETEGID
	    if ((PL_delaymagic & DM_GID) == DM_EGID) {
		PERL_UNUSED_RESULT(setegid(PL_delaymagic_egid));
		PL_delaymagic &= ~DM_EGID;
	    }
#    endif /* HAS_SETEGID */
	    if (PL_delaymagic & DM_GID) {
		if (PL_delaymagic_gid != PL_delaymagic_egid)
		    DIE(aTHX_ "No setregid available");
		PERL_UNUSED_RESULT(PerlProc_setgid(PL_delaymagic_gid));
	    }
#  endif /* HAS_SETREGID */
#endif /* HAS_SETRESGID */

	    tmp_gid  = PerlProc_getgid();
	    tmp_egid = PerlProc_getegid();
	}
	TAINTING_set( TAINTING_get | (tmp_uid && (tmp_euid != tmp_uid || tmp_egid != tmp_gid)) );
#ifdef NO_TAINT_SUPPORT
        PERL_UNUSED_VAR(tmp_uid);
        PERL_UNUSED_VAR(tmp_euid);
        PERL_UNUSED_VAR(tmp_gid);
        PERL_UNUSED_VAR(tmp_egid);
#endif
    }
    PL_delaymagic = old_delaymagic;

    if (gimme == G_VOID)
	SP = firstrelem - 1;
    else if (gimme == G_SCALAR) {
	dTARGET;
	SP = firstrelem;
	SETi(lastrelem - firstrelem + 1);
    }
    else {
	if (ary || hash)
	    /* note that in this case *firstlelem may have been overwritten
	       by sv_undef in the odd hash case */
	    SP = lastrelem;
	else {
	    SP = firstrelem + (lastlelem - firstlelem);
            lelem = firstlelem + (relem - firstrelem);
            while (relem <= SP)
                *relem++ = (lelem <= lastlelem) ? *lelem++ : &PL_sv_undef;
        }
    }

    RETURN;
}

PP(pp_qr)
{
    dSP;
    PMOP * const pm = cPMOP;
    REGEXP * rx = PM_GETRE(pm);
    SV * const pkg = rx ? CALLREG_PACKAGE(rx) : NULL;
    SV * const rv = sv_newmortal();
    CV **cvp;
    CV *cv;

    SvUPGRADE(rv, SVt_IV);
    /* For a subroutine describing itself as "This is a hacky workaround" I'm
       loathe to use it here, but it seems to be the right fix. Or close.
       The key part appears to be that it's essential for pp_qr to return a new
       object (SV), which implies that there needs to be an effective way to
       generate a new SV from the existing SV that is pre-compiled in the
       optree.  */
    SvRV_set(rv, MUTABLE_SV(reg_temp_copy(NULL, rx)));
    SvROK_on(rv);

    cvp = &( ReANY((REGEXP *)SvRV(rv))->qr_anoncv);
    if (UNLIKELY((cv = *cvp) && CvCLONE(*cvp))) {
	*cvp = cv_clone(cv);
	SvREFCNT_dec_NN(cv);
    }

    if (pkg) {
	HV *const stash = gv_stashsv(pkg, GV_ADD);
	SvREFCNT_dec_NN(pkg);
	(void)sv_bless(rv, stash);
    }

    if (UNLIKELY(RX_ISTAINTED(rx))) {
        SvTAINTED_on(rv);
        SvTAINTED_on(SvRV(rv));
    }
    XPUSHs(rv);
    RETURN;
}

PP(pp_match)
{
    dSP; dTARG;
    PMOP *pm = cPMOP;
    PMOP *dynpm = pm;
    const char *s;
    const char *strend;
    SSize_t curpos = 0; /* initial pos() or current $+[0] */
    I32 global;
    U8 r_flags = 0;
    const char *truebase;			/* Start of string  */
    REGEXP *rx = PM_GETRE(pm);
    bool rxtainted;
    const U8 gimme = GIMME_V;
    STRLEN len;
    const I32 oldsave = PL_savestack_ix;
    I32 had_zerolen = 0;
    MAGIC *mg = NULL;

    if (PL_op->op_flags & OPf_STACKED)
	TARG = POPs;
    else if (ARGTARG)
	GETTARGET;
    else {
	TARG = DEFSV;
	EXTEND(SP,1);
    }

    PUTBACK;				/* EVAL blocks need stack_sp. */
    /* Skip get-magic if this is a qr// clone, because regcomp has
       already done it. */
    truebase = ReANY(rx)->mother_re
	 ? SvPV_nomg_const(TARG, len)
	 : SvPV_const(TARG, len);
    if (!truebase)
	DIE(aTHX_ "panic: pp_match");
    strend = truebase + len;
    rxtainted = (RX_ISTAINTED(rx) ||
		 (TAINT_get && (pm->op_pmflags & PMf_RETAINT)));
    TAINT_NOT;

    /* We need to know this in case we fail out early - pos() must be reset */
    global = dynpm->op_pmflags & PMf_GLOBAL;

    /* PMdf_USED is set after a ?? matches once */
    if (
#ifdef USE_ITHREADS
        SvREADONLY(PL_regex_pad[pm->op_pmoffset])
#else
        pm->op_pmflags & PMf_USED
#endif
    ) {
        DEBUG_r(PerlIO_printf(Perl_debug_log, "?? already matched once"));
	goto nope;
    }

    /* empty pattern special-cased to use last successful pattern if
       possible, except for qr// */
    if (!ReANY(rx)->mother_re && !RX_PRELEN(rx)
     && PL_curpm) {
	pm = PL_curpm;
	rx = PM_GETRE(pm);
    }

    if (RX_MINLEN(rx) >= 0 && (STRLEN)RX_MINLEN(rx) > len) {
        DEBUG_r(PerlIO_printf(Perl_debug_log, "String shorter than min possible regex match (%"
                                              UVuf" < %"IVdf")\n",
                                              (UV)len, (IV)RX_MINLEN(rx)));
	goto nope;
    }

    /* get pos() if //g */
    if (global) {
        mg = mg_find_mglob(TARG);
        if (mg && mg->mg_len >= 0) {
            curpos = MgBYTEPOS(mg, TARG, truebase, len);
            /* last time pos() was set, it was zero-length match */
            if (mg->mg_flags & MGf_MINMATCH)
                had_zerolen = 1;
        }
    }

#ifdef PERL_SAWAMPERSAND
    if (       RX_NPARENS(rx)
            || PL_sawampersand
            || (RX_EXTFLAGS(rx) & (RXf_EVAL_SEEN|RXf_PMf_KEEPCOPY))
            || (dynpm->op_pmflags & PMf_KEEPCOPY)
    )
#endif
    {
	r_flags |= (REXEC_COPY_STR|REXEC_COPY_SKIP_PRE);
        /* in @a =~ /(.)/g, we iterate multiple times, but copy the buffer
         * only on the first iteration. Therefore we need to copy $' as well
         * as $&, to make the rest of the string available for captures in
         * subsequent iterations */
        if (! (global && gimme == G_ARRAY))
            r_flags |= REXEC_COPY_SKIP_POST;
    };
#ifdef PERL_SAWAMPERSAND
    if (dynpm->op_pmflags & PMf_KEEPCOPY)
        /* handle KEEPCOPY in pmop but not rx, eg $r=qr/a/; /$r/p */
        r_flags &= ~(REXEC_COPY_SKIP_PRE|REXEC_COPY_SKIP_POST);
#endif

    s = truebase;

  play_it_again:
    if (global)
	s = truebase + curpos;

    if (!CALLREGEXEC(rx, (char*)s, (char *)strend, (char*)truebase,
		     had_zerolen, TARG, NULL, r_flags))
	goto nope;

    PL_curpm = pm;
    if (dynpm->op_pmflags & PMf_ONCE)
#ifdef USE_ITHREADS
	SvREADONLY_on(PL_regex_pad[dynpm->op_pmoffset]);
#else
	dynpm->op_pmflags |= PMf_USED;
#endif

    if (rxtainted)
	RX_MATCH_TAINTED_on(rx);
    TAINT_IF(RX_MATCH_TAINTED(rx));

    /* update pos */

    if (global && (gimme != G_ARRAY || (dynpm->op_pmflags & PMf_CONTINUE))) {
        if (!mg)
            mg = sv_magicext_mglob(TARG);
        MgBYTEPOS_set(mg, TARG, truebase, RX_OFFS(rx)[0].end);
        if (RX_ZERO_LEN(rx))
            mg->mg_flags |= MGf_MINMATCH;
        else
            mg->mg_flags &= ~MGf_MINMATCH;
    }

    if ((!RX_NPARENS(rx) && !global) || gimme != G_ARRAY) {
	LEAVE_SCOPE(oldsave);
	RETPUSHYES;
    }

    /* push captures on stack */

    {
	const I32 nparens = RX_NPARENS(rx);
	I32 i = (global && !nparens) ? 1 : 0;

	SPAGAIN;			/* EVAL blocks could move the stack. */
	EXTEND(SP, nparens + i);
	EXTEND_MORTAL(nparens + i);
	for (i = !i; i <= nparens; i++) {
	    PUSHs(sv_newmortal());
	    if (LIKELY((RX_OFFS(rx)[i].start != -1)
                     && RX_OFFS(rx)[i].end   != -1 ))
            {
		const I32 len = RX_OFFS(rx)[i].end - RX_OFFS(rx)[i].start;
		const char * const s = RX_OFFS(rx)[i].start + truebase;
	        if (UNLIKELY(RX_OFFS(rx)[i].end < 0 || RX_OFFS(rx)[i].start < 0
                        || len < 0 || len > strend - s))
		    DIE(aTHX_ "panic: pp_match start/end pointers, i=%ld, "
			"start=%ld, end=%ld, s=%p, strend=%p, len=%"UVuf,
			(long) i, (long) RX_OFFS(rx)[i].start,
			(long)RX_OFFS(rx)[i].end, s, strend, (UV) len);
		sv_setpvn(*SP, s, len);
		if (DO_UTF8(TARG) && is_utf8_string((U8*)s, len))
		    SvUTF8_on(*SP);
	    }
	}
	if (global) {
            curpos = (UV)RX_OFFS(rx)[0].end;
	    had_zerolen = RX_ZERO_LEN(rx);
	    PUTBACK;			/* EVAL blocks may use stack */
	    r_flags |= REXEC_IGNOREPOS | REXEC_NOT_FIRST;
	    goto play_it_again;
	}
	LEAVE_SCOPE(oldsave);
	RETURN;
    }
    NOT_REACHED; /* NOTREACHED */

  nope:
    if (global && !(dynpm->op_pmflags & PMf_CONTINUE)) {
        if (!mg)
            mg = mg_find_mglob(TARG);
        if (mg)
            mg->mg_len = -1;
    }
    LEAVE_SCOPE(oldsave);
    if (gimme == G_ARRAY)
	RETURN;
    RETPUSHNO;
}

OP *
Perl_do_readline(pTHX)
{
    dSP; dTARGETSTACKED;
    SV *sv;
    STRLEN tmplen = 0;
    STRLEN offset;
    PerlIO *fp;
    IO * const io = GvIO(PL_last_in_gv);
    const I32 type = PL_op->op_type;
    const U8 gimme = GIMME_V;

    if (io) {
	const MAGIC *const mg = SvTIED_mg((const SV *)io, PERL_MAGIC_tiedscalar);
	if (mg) {
	    Perl_tied_method(aTHX_ SV_CONST(READLINE), SP, MUTABLE_SV(io), mg, gimme, 0);
	    if (gimme == G_SCALAR) {
		SPAGAIN;
		SvSetSV_nosteal(TARG, TOPs);
		SETTARG;
	    }
	    return NORMAL;
	}
    }
    fp = NULL;
    if (io) {
	fp = IoIFP(io);
	if (!fp) {
	    if (IoFLAGS(io) & IOf_ARGV) {
		if (IoFLAGS(io) & IOf_START) {
		    IoLINES(io) = 0;
		    if (av_tindex(GvAVn(PL_last_in_gv)) < 0) {
			IoFLAGS(io) &= ~IOf_START;
			do_open6(PL_last_in_gv, "-", 1, NULL, NULL, 0);
			SvTAINTED_off(GvSVn(PL_last_in_gv)); /* previous tainting irrelevant */
			sv_setpvs(GvSVn(PL_last_in_gv), "-");
			SvSETMAGIC(GvSV(PL_last_in_gv));
			fp = IoIFP(io);
			goto have_fp;
		    }
		}
		fp = nextargv(PL_last_in_gv, PL_op->op_flags & OPf_SPECIAL);
		if (!fp) { /* Note: fp != IoIFP(io) */
		    (void)do_close(PL_last_in_gv, FALSE); /* now it does*/
		}
	    }
	    else if (type == OP_GLOB)
		fp = Perl_start_glob(aTHX_ POPs, io);
	}
	else if (type == OP_GLOB)
	    SP--;
	else if (IoTYPE(io) == IoTYPE_WRONLY) {
	    report_wrongway_fh(PL_last_in_gv, '>');
	}
    }
    if (!fp) {
	if ((!io || !(IoFLAGS(io) & IOf_START))
	    && ckWARN(WARN_CLOSED)
            && type != OP_GLOB)
	{
	    report_evil_fh(PL_last_in_gv);
	}
	if (gimme == G_SCALAR) {
	    /* undef TARG, and push that undefined value */
	    if (type != OP_RCATLINE) {
		sv_setsv(TARG,NULL);
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
		SvPV_force_nomg_nolen(sv);
	    else
		sv_unref(sv);
	}
	else if (isGV_with_GP(sv)) {
	    SvPV_force_nomg_nolen(sv);
	}
	SvUPGRADE(sv, SVt_PV);
	tmplen = SvLEN(sv);	/* remember if already alloced */
	if (!tmplen && !SvREADONLY(sv) && !SvIsCOW(sv)) {
            /* try short-buffering it. Please update t/op/readline.t
	     * if you change the growth length.
	     */
	    Sv_Grow(sv, 80);
        }
	offset = 0;
	if (type == OP_RCATLINE && SvOK(sv)) {
	    if (!SvPOK(sv)) {
		SvPV_force_nomg_nolen(sv);
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
		fp = nextargv(PL_last_in_gv, PL_op->op_flags & OPf_SPECIAL);
		if (fp)
		    continue;
		(void)do_close(PL_last_in_gv, FALSE);
	    }
	    else if (type == OP_GLOB) {
		if (!do_close(PL_last_in_gv, FALSE)) {
		    Perl_ck_warner(aTHX_ packWARN(WARN_GLOB),
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
	    Stat_t statbuf;

	    if (SvCUR(sv) > 0 && SvCUR(PL_rs) > 0) {
		char * const tmps = SvEND(sv) - 1;
		if (*tmps == *SvPVX_const(PL_rs)) {
		    *tmps = '\0';
		    SvCUR_set(sv, SvCUR(sv) - 1);
		}
	    }
	    for (t1 = SvPVX_const(sv); *t1; t1++)
#ifdef __VMS
		if (strchr("*%?", *t1))
#else
		if (strchr("$&*(){}[]'\";\\|?<>~`", *t1))
#endif
			break;
	    if (*t1 && PerlLIO_lstat(SvPVX_const(sv), &statbuf) < 0) {
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

PP(pp_helem)
{
    dSP;
    HE* he;
    SV **svp;
    SV * const keysv = POPs;
    HV * const hv = MUTABLE_HV(POPs);
    const U32 lval = PL_op->op_flags & OPf_MOD || LVRET;
    const U32 defer = PL_op->op_private & OPpLVAL_DEFER;
    SV *sv;
    const bool localizing = PL_op->op_private & OPpLVAL_INTRO;
    bool preeminent = TRUE;

    if (SvTYPE(hv) != SVt_PVHV)
	RETPUSHUNDEF;

    if (localizing) {
	MAGIC *mg;
	HV *stash;

	/* If we can determine whether the element exist,
	 * Try to preserve the existenceness of a tied hash
	 * element by using EXISTS and DELETE if possible.
	 * Fallback to FETCH and STORE otherwise. */
	if (SvCANEXISTDELETE(hv))
	    preeminent = hv_exists_ent(hv, keysv, 0);
    }

    he = hv_fetch_ent(hv, keysv, lval && !defer, 0);
    svp = he ? &HeVAL(he) : NULL;
    if (lval) {
	if (!svp || !*svp || *svp == &PL_sv_undef) {
	    SV* lv;
	    SV* key2;
	    if (!defer) {
		DIE(aTHX_ PL_no_helem_sv, SVfARG(keysv));
	    }
	    lv = sv_newmortal();
	    sv_upgrade(lv, SVt_PVLV);
	    LvTYPE(lv) = 'y';
	    sv_magic(lv, key2 = newSVsv(keysv), PERL_MAGIC_defelem, NULL, 0);
	    SvREFCNT_dec_NN(key2);	/* sv_magic() increments refcount */
	    LvTARG(lv) = SvREFCNT_inc_simple_NN(hv);
	    LvTARGLEN(lv) = 1;
	    PUSHs(lv);
	    RETURN;
	}
	if (localizing) {
	    if (HvNAME_get(hv) && isGV(*svp))
		save_gp(MUTABLE_GV(*svp), !(PL_op->op_flags & OPf_SPECIAL));
	    else if (preeminent)
		save_helem_flags(hv, keysv, svp,
		     (PL_op->op_flags & OPf_SPECIAL) ? 0 : SAVEf_SETMAGIC);
	    else
		SAVEHDELETE(hv, keysv);
	}
	else if (PL_op->op_private & OPpDEREF) {
	    PUSHs(vivify_ref(*svp, PL_op->op_private & OPpDEREF));
	    RETURN;
	}
    }
    sv = (svp && *svp ? *svp : &PL_sv_undef);
    /* Originally this did a conditional C<sv = sv_mortalcopy(sv)>; this
     * was to make C<local $tied{foo} = $tied{foo}> possible.
     * However, it seems no longer to be needed for that purpose, and
     * introduced a new bug: stuff like C<while ($hash{taintedval} =~ /.../g>
     * would loop endlessly since the pos magic is getting set on the
     * mortal copy and lost. However, the copy has the effect of
     * triggering the get magic, and losing it altogether made things like
     * c<$tied{foo};> in void context no longer do get magic, which some
     * code relied on. Also, delayed triggering of magic on @+ and friends
     * meant the original regex may be out of scope by now. So as a
     * compromise, do the get magic here. (The MGf_GSKIP flag will stop it
     * being called too many times). */
    if (!lval && SvRMAGICAL(hv) && SvGMAGICAL(sv))
	mg_get(sv);
    PUSHs(sv);
    RETURN;
}


/* a stripped-down version of Perl_softref2xv() for use by
 * pp_multideref(), which doesn't use PL_op->op_flags */

STATIC GV *
S_softref2xv_lite(pTHX_ SV *const sv, const char *const what,
		const svtype type)
{
    if (PL_op->op_private & HINT_STRICT_REFS) {
	if (SvOK(sv))
	    Perl_die(aTHX_ PL_no_symref_sv, sv,
		     (SvPOKp(sv) && SvCUR(sv)>32 ? "..." : ""), what);
	else
	    Perl_die(aTHX_ PL_no_usym, what);
    }
    if (!SvOK(sv))
        Perl_die(aTHX_ PL_no_usym, what);
    return gv_fetchsv_nomg(sv, GV_ADD, type);
}


/* Handle one or more aggregate derefs and array/hash indexings, e.g.
 * $h->{foo}  or  $a[0]{$key}[$i]  or  f()->[1]
 *
 * op_aux points to an array of unions of UV / IV / SV* / PADOFFSET.
 * Each of these either contains a set of actions, or an argument, such as
 * an IV to use as an array index, or a lexical var to retrieve.
 * Several actions re stored per UV; we keep shifting new actions off the
 * one UV, and only reload when it becomes zero.
 */

PP(pp_multideref)
{
    SV *sv = NULL; /* init to avoid spurious 'may be used uninitialized' */
    UNOP_AUX_item *items = cUNOP_AUXx(PL_op)->op_aux;
    UV actions = items->uv;

    assert(actions);
    /* this tells find_uninit_var() where we're up to */
    PL_multideref_pc = items;

    while (1) {
        /* there are three main classes of action; the first retrieve
         * the initial AV or HV from a variable or the stack; the second
         * does the equivalent of an unrolled (/DREFAV, rv2av, aelem),
         * the third an unrolled (/DREFHV, rv2hv, helem).
         */
        switch (actions & MDEREF_ACTION_MASK) {

        case MDEREF_reload:
            actions = (++items)->uv;
            continue;

        case MDEREF_AV_padav_aelem:                 /* $lex[...] */
            sv = PAD_SVl((++items)->pad_offset);
            goto do_AV_aelem;

        case MDEREF_AV_gvav_aelem:                  /* $pkg[...] */
            sv = UNOP_AUX_item_sv(++items);
            assert(isGV_with_GP(sv));
            sv = (SV*)GvAVn((GV*)sv);
            goto do_AV_aelem;

        case MDEREF_AV_pop_rv2av_aelem:             /* expr->[...] */
            {
                dSP;
                sv = POPs;
                PUTBACK;
                goto do_AV_rv2av_aelem;
            }

        case MDEREF_AV_gvsv_vivify_rv2av_aelem:     /* $pkg->[...] */
            sv = UNOP_AUX_item_sv(++items);
            assert(isGV_with_GP(sv));
            sv = GvSVn((GV*)sv);
            goto do_AV_vivify_rv2av_aelem;

        case MDEREF_AV_padsv_vivify_rv2av_aelem:     /* $lex->[...] */
            sv = PAD_SVl((++items)->pad_offset);
            /* FALLTHROUGH */

        do_AV_vivify_rv2av_aelem:
        case MDEREF_AV_vivify_rv2av_aelem:           /* vivify, ->[...] */
            /* this is the OPpDEREF action normally found at the end of
             * ops like aelem, helem, rv2sv */
            sv = vivify_ref(sv, OPpDEREF_AV);
            /* FALLTHROUGH */

        do_AV_rv2av_aelem:
            /* this is basically a copy of pp_rv2av when it just has the
             * sKR/1 flags */
            SvGETMAGIC(sv);
            if (LIKELY(SvROK(sv))) {
                if (UNLIKELY(SvAMAGIC(sv))) {
                    sv = amagic_deref_call(sv, to_av_amg);
                }
                sv = SvRV(sv);
                if (UNLIKELY(SvTYPE(sv) != SVt_PVAV))
                    DIE(aTHX_ "Not an ARRAY reference");
            }
            else if (SvTYPE(sv) != SVt_PVAV) {
                if (!isGV_with_GP(sv))
                    sv = (SV*)S_softref2xv_lite(aTHX_ sv, "an ARRAY", SVt_PVAV);
                sv = MUTABLE_SV(GvAVn((GV*)sv));
            }
            /* FALLTHROUGH */

        do_AV_aelem:
            {
                /* retrieve the key; this may be either a lexical or package
                 * var (whose index/ptr is stored as an item) or a signed
                 * integer constant stored as an item.
                 */
                SV *elemsv;
                IV elem = 0; /* to shut up stupid compiler warnings */


                assert(SvTYPE(sv) == SVt_PVAV);

                switch (actions & MDEREF_INDEX_MASK) {
                case MDEREF_INDEX_none:
                    goto finish;
                case MDEREF_INDEX_const:
                    elem  = (++items)->iv;
                    break;
                case MDEREF_INDEX_padsv:
                    elemsv = PAD_SVl((++items)->pad_offset);
                    goto check_elem;
                case MDEREF_INDEX_gvsv:
                    elemsv = UNOP_AUX_item_sv(++items);
                    assert(isGV_with_GP(elemsv));
                    elemsv = GvSVn((GV*)elemsv);
                check_elem:
                    if (UNLIKELY(SvROK(elemsv) && !SvGAMAGIC(elemsv)
                                            && ckWARN(WARN_MISC)))
                        Perl_warner(aTHX_ packWARN(WARN_MISC),
                                "Use of reference \"%"SVf"\" as array index",
                                SVfARG(elemsv));
                    /* the only time that S_find_uninit_var() needs this
                     * is to determine which index value triggered the
                     * undef warning. So just update it here. Note that
                     * since we don't save and restore this var (e.g. for
                     * tie or overload execution), its value will be
                     * meaningless apart from just here */
                    PL_multideref_pc = items;
                    elem = SvIV(elemsv);
                    break;
                }


                /* this is basically a copy of pp_aelem with OPpDEREF skipped */

                if (!(actions & MDEREF_FLAG_last)) {
                    SV** svp = av_fetch((AV*)sv, elem, 1);
                    if (!svp || ! (sv=*svp))
                        DIE(aTHX_ PL_no_aelem, elem);
                    break;
                }

                if (PL_op->op_private &
                    (OPpMULTIDEREF_EXISTS|OPpMULTIDEREF_DELETE))
                {
                    if (PL_op->op_private & OPpMULTIDEREF_EXISTS) {
                        sv = av_exists((AV*)sv, elem) ? &PL_sv_yes : &PL_sv_no;
                    }
                    else {
                        I32 discard = (GIMME_V == G_VOID) ? G_DISCARD : 0;
                        sv = av_delete((AV*)sv, elem, discard);
                        if (discard)
                            return NORMAL;
                        if (!sv)
                            sv = &PL_sv_undef;
                    }
                }
                else {
                    const U32 lval = PL_op->op_flags & OPf_MOD || LVRET;
                    const U32 defer = PL_op->op_private & OPpLVAL_DEFER;
                    const bool localizing = PL_op->op_private & OPpLVAL_INTRO;
                    bool preeminent = TRUE;
                    AV *const av = (AV*)sv;
                    SV** svp;

                    if (UNLIKELY(localizing)) {
                        MAGIC *mg;
                        HV *stash;

                        /* If we can determine whether the element exist,
                         * Try to preserve the existenceness of a tied array
                         * element by using EXISTS and DELETE if possible.
                         * Fallback to FETCH and STORE otherwise. */
                        if (SvCANEXISTDELETE(av))
                            preeminent = av_exists(av, elem);
                    }

                    svp = av_fetch(av, elem, lval && !defer);

                    if (lval) {
                        if (!svp || !(sv = *svp)) {
                            IV len;
                            if (!defer)
                                DIE(aTHX_ PL_no_aelem, elem);
                            len = av_tindex(av);
                            sv = sv_2mortal(newSVavdefelem(av,
                            /* Resolve a negative index now, unless it points
                             * before the beginning of the array, in which
                             * case record it for error reporting in
                             * magic_setdefelem. */
                                elem < 0 && len + elem >= 0
                                    ? len + elem : elem, 1));
                        }
                        else {
                            if (UNLIKELY(localizing)) {
                                if (preeminent) {
                                    save_aelem(av, elem, svp);
                                    sv = *svp; /* may have changed */
                                }
                                else
                                    SAVEADELETE(av, elem);
                            }
                        }
                    }
                    else {
                        sv = (svp ? *svp : &PL_sv_undef);
                        /* see note in pp_helem() */
                        if (SvRMAGICAL(av) && SvGMAGICAL(sv))
                            mg_get(sv);
                    }
                }

            }
          finish:
            {
                dSP;
                XPUSHs(sv);
                RETURN;
            }
            /* NOTREACHED */




        case MDEREF_HV_padhv_helem:                 /* $lex{...} */
            sv = PAD_SVl((++items)->pad_offset);
            goto do_HV_helem;

        case MDEREF_HV_gvhv_helem:                  /* $pkg{...} */
            sv = UNOP_AUX_item_sv(++items);
            assert(isGV_with_GP(sv));
            sv = (SV*)GvHVn((GV*)sv);
            goto do_HV_helem;

        case MDEREF_HV_pop_rv2hv_helem:             /* expr->{...} */
            {
                dSP;
                sv = POPs;
                PUTBACK;
                goto do_HV_rv2hv_helem;
            }

        case MDEREF_HV_gvsv_vivify_rv2hv_helem:     /* $pkg->{...} */
            sv = UNOP_AUX_item_sv(++items);
            assert(isGV_with_GP(sv));
            sv = GvSVn((GV*)sv);
            goto do_HV_vivify_rv2hv_helem;

        case MDEREF_HV_padsv_vivify_rv2hv_helem:    /* $lex->{...} */
            sv = PAD_SVl((++items)->pad_offset);
            /* FALLTHROUGH */

        do_HV_vivify_rv2hv_helem:
        case MDEREF_HV_vivify_rv2hv_helem:           /* vivify, ->{...} */
            /* this is the OPpDEREF action normally found at the end of
             * ops like aelem, helem, rv2sv */
            sv = vivify_ref(sv, OPpDEREF_HV);
            /* FALLTHROUGH */

        do_HV_rv2hv_helem:
            /* this is basically a copy of pp_rv2hv when it just has the
             * sKR/1 flags (and pp_rv2hv is aliased to pp_rv2av) */

            SvGETMAGIC(sv);
            if (LIKELY(SvROK(sv))) {
                if (UNLIKELY(SvAMAGIC(sv))) {
                    sv = amagic_deref_call(sv, to_hv_amg);
                }
                sv = SvRV(sv);
                if (UNLIKELY(SvTYPE(sv) != SVt_PVHV))
                    DIE(aTHX_ "Not a HASH reference");
            }
            else if (SvTYPE(sv) != SVt_PVHV) {
                if (!isGV_with_GP(sv))
                    sv = (SV*)S_softref2xv_lite(aTHX_ sv, "a HASH", SVt_PVHV);
                sv = MUTABLE_SV(GvHVn((GV*)sv));
            }
            /* FALLTHROUGH */

        do_HV_helem:
            {
                /* retrieve the key; this may be either a lexical / package
                 * var or a string constant, whose index/ptr is stored as an
                 * item
                 */
                SV *keysv = NULL; /* to shut up stupid compiler warnings */

                assert(SvTYPE(sv) == SVt_PVHV);

                switch (actions & MDEREF_INDEX_MASK) {
                case MDEREF_INDEX_none:
                    goto finish;

                case MDEREF_INDEX_const:
                    keysv = UNOP_AUX_item_sv(++items);
                    break;

                case MDEREF_INDEX_padsv:
                    keysv = PAD_SVl((++items)->pad_offset);
                    break;

                case MDEREF_INDEX_gvsv:
                    keysv = UNOP_AUX_item_sv(++items);
                    keysv = GvSVn((GV*)keysv);
                    break;
                }

                /* see comment above about setting this var */
                PL_multideref_pc = items;


                /* ensure that candidate CONSTs have been HEKified */
                assert(   ((actions & MDEREF_INDEX_MASK) != MDEREF_INDEX_const)
                       || SvTYPE(keysv) >= SVt_PVMG
                       || !SvOK(keysv)
                       || SvROK(keysv)
                       || SvIsCOW_shared_hash(keysv));

                /* this is basically a copy of pp_helem with OPpDEREF skipped */

                if (!(actions & MDEREF_FLAG_last)) {
                    HE *he = hv_fetch_ent((HV*)sv, keysv, 1, 0);
                    if (!he || !(sv=HeVAL(he)) || sv == &PL_sv_undef)
                        DIE(aTHX_ PL_no_helem_sv, SVfARG(keysv));
                    break;
                }

                if (PL_op->op_private &
                    (OPpMULTIDEREF_EXISTS|OPpMULTIDEREF_DELETE))
                {
                    if (PL_op->op_private & OPpMULTIDEREF_EXISTS) {
                        sv = hv_exists_ent((HV*)sv, keysv, 0)
                                                ? &PL_sv_yes : &PL_sv_no;
                    }
                    else {
                        I32 discard = (GIMME_V == G_VOID) ? G_DISCARD : 0;
                        sv = hv_delete_ent((HV*)sv, keysv, discard, 0);
                        if (discard)
                            return NORMAL;
                        if (!sv)
                            sv = &PL_sv_undef;
                    }
                }
                else {
                    const U32 lval = PL_op->op_flags & OPf_MOD || LVRET;
                    const U32 defer = PL_op->op_private & OPpLVAL_DEFER;
                    const bool localizing = PL_op->op_private & OPpLVAL_INTRO;
                    bool preeminent = TRUE;
                    SV **svp;
                    HV * const hv = (HV*)sv;
                    HE* he;

                    if (UNLIKELY(localizing)) {
                        MAGIC *mg;
                        HV *stash;

                        /* If we can determine whether the element exist,
                         * Try to preserve the existenceness of a tied hash
                         * element by using EXISTS and DELETE if possible.
                         * Fallback to FETCH and STORE otherwise. */
                        if (SvCANEXISTDELETE(hv))
                            preeminent = hv_exists_ent(hv, keysv, 0);
                    }

                    he = hv_fetch_ent(hv, keysv, lval && !defer, 0);
                    svp = he ? &HeVAL(he) : NULL;


                    if (lval) {
                        if (!svp || !(sv = *svp) || sv == &PL_sv_undef) {
                            SV* lv;
                            SV* key2;
                            if (!defer)
                                DIE(aTHX_ PL_no_helem_sv, SVfARG(keysv));
                            lv = sv_newmortal();
                            sv_upgrade(lv, SVt_PVLV);
                            LvTYPE(lv) = 'y';
                            sv_magic(lv, key2 = newSVsv(keysv),
                                                PERL_MAGIC_defelem, NULL, 0);
                            /* sv_magic() increments refcount */
                            SvREFCNT_dec_NN(key2);
                            LvTARG(lv) = SvREFCNT_inc_simple_NN(hv);
                            LvTARGLEN(lv) = 1;
                            sv = lv;
                        }
                        else {
                            if (localizing) {
                                if (HvNAME_get(hv) && isGV(sv))
                                    save_gp(MUTABLE_GV(sv),
                                        !(PL_op->op_flags & OPf_SPECIAL));
                                else if (preeminent) {
                                    save_helem_flags(hv, keysv, svp,
                                         (PL_op->op_flags & OPf_SPECIAL)
                                            ? 0 : SAVEf_SETMAGIC);
                                    sv = *svp; /* may have changed */
                                }
                                else
                                    SAVEHDELETE(hv, keysv);
                            }
                        }
                    }
                    else {
                        sv = (svp && *svp ? *svp : &PL_sv_undef);
                        /* see note in pp_helem() */
                        if (SvRMAGICAL(hv) && SvGMAGICAL(sv))
                            mg_get(sv);
                    }
                }
                goto finish;
            }

        } /* switch */

        actions >>= MDEREF_SHIFT;
    } /* while */
    /* NOTREACHED */
}


PP(pp_iter)
{
    PERL_CONTEXT *cx;
    SV *oldsv;
    SV **itersvp;
    SV *retsv;

    SV *sv;
    AV *av;
    IV ix;
    IV inc;

    cx = CX_CUR();
    itersvp = CxITERVAR(cx);
    assert(itersvp);

    switch (CxTYPE(cx)) {

    case CXt_LOOP_LAZYSV: /* string increment */
    {
        SV* cur = cx->blk_loop.state_u.lazysv.cur;
        SV *end = cx->blk_loop.state_u.lazysv.end;
        /* If the maximum is !SvOK(), pp_enteriter substitutes PL_sv_no.
           It has SvPVX of "" and SvCUR of 0, which is what we want.  */
        STRLEN maxlen = 0;
        const char *max = SvPV_const(end, maxlen);
        if (UNLIKELY(SvNIOK(cur) || SvCUR(cur) > maxlen))
            goto retno;

        oldsv = *itersvp;
        /* NB: on the first iteration, oldsv will have a ref count of at
         * least 2 (one extra from blk_loop.itersave), so the GV or pad
         * slot will get localised; on subsequent iterations the RC==1
         * optimisation may kick in and the SV will be reused. */
         if (oldsv && LIKELY(SvREFCNT(oldsv) == 1 && !SvMAGICAL(oldsv))) {
            /* safe to reuse old SV */
            sv_setsv(oldsv, cur);
        }
        else
        {
            /* we need a fresh SV every time so that loop body sees a
             * completely new SV for closures/references to work as
             * they used to */
            *itersvp = newSVsv(cur);
            SvREFCNT_dec(oldsv);
        }
        if (strEQ(SvPVX_const(cur), max))
            sv_setiv(cur, 0); /* terminate next time */
        else
            sv_inc(cur);
        break;
    }

    case CXt_LOOP_LAZYIV: /* integer increment */
    {
        IV cur = cx->blk_loop.state_u.lazyiv.cur;
	if (UNLIKELY(cur > cx->blk_loop.state_u.lazyiv.end))
	    goto retno;

        oldsv = *itersvp;
	/* see NB comment above */
	if (oldsv && LIKELY(SvREFCNT(oldsv) == 1 && !SvMAGICAL(oldsv))) {
	    /* safe to reuse old SV */

            if (    (SvFLAGS(oldsv) & (SVTYPEMASK|SVf_THINKFIRST|SVf_IVisUV))
                 == SVt_IV)
            {
                /* Cheap SvIOK_only().
                 * Assert that flags which SvIOK_only() would test or
                 * clear can't be set, because we're SVt_IV */
                assert(!(SvFLAGS(oldsv) &
                    (SVf_OOK|SVf_UTF8|(SVf_OK & ~(SVf_IOK|SVp_IOK)))));
                SvFLAGS(oldsv) |= (SVf_IOK|SVp_IOK);
                /* SvIV_set() where sv_any points to head */
                oldsv->sv_u.svu_iv = cur;

            }
            else
                sv_setiv(oldsv, cur);
	}
	else
	{
	    /* we need a fresh SV every time so that loop body sees a
	     * completely new SV for closures/references to work as they
	     * used to */
	    *itersvp = newSViv(cur);
	    SvREFCNT_dec(oldsv);
	}

	if (UNLIKELY(cur == IV_MAX)) {
	    /* Handle end of range at IV_MAX */
	    cx->blk_loop.state_u.lazyiv.end = IV_MIN;
	} else
	    ++cx->blk_loop.state_u.lazyiv.cur;
        break;
    }

    case CXt_LOOP_LIST: /* for (1,2,3) */

        assert(OPpITER_REVERSED == 2); /* so inc becomes -1 or 1 */
        inc = 1 - (PL_op->op_private & OPpITER_REVERSED);
        ix = (cx->blk_loop.state_u.stack.ix += inc);
        if (UNLIKELY(inc > 0
                        ? ix > cx->blk_oldsp
                        : ix <= cx->blk_loop.state_u.stack.basesp)
        )
            goto retno;

        sv = PL_stack_base[ix];
        av = NULL;
        goto loop_ary_common;

    case CXt_LOOP_ARY: /* for (@ary) */

        av = cx->blk_loop.state_u.ary.ary;
        inc = 1 - (PL_op->op_private & OPpITER_REVERSED);
        ix = (cx->blk_loop.state_u.ary.ix += inc);
        if (UNLIKELY(inc > 0
                        ? ix > AvFILL(av)
                        : ix < 0)
        )
            goto retno;

        if (UNLIKELY(SvRMAGICAL(av))) {
            SV * const * const svp = av_fetch(av, ix, FALSE);
            sv = svp ? *svp : NULL;
        }
        else {
            sv = AvARRAY(av)[ix];
        }

      loop_ary_common:

        if (UNLIKELY(cx->cx_type & CXp_FOR_LVREF)) {
            SvSetMagicSV(*itersvp, sv);
            break;
        }

        if (LIKELY(sv)) {
            if (UNLIKELY(SvIS_FREED(sv))) {
                *itersvp = NULL;
                Perl_croak(aTHX_ "Use of freed value in iteration");
            }
            if (SvPADTMP(sv)) {
                sv = newSVsv(sv);
            }
            else {
                SvTEMP_off(sv);
                SvREFCNT_inc_simple_void_NN(sv);
            }
        }
        else if (av) {
            sv = newSVavdefelem(av, ix, 0);
        }
        else
            sv = &PL_sv_undef;

        oldsv = *itersvp;
        *itersvp = sv;
        SvREFCNT_dec(oldsv);
        break;

    default:
	DIE(aTHX_ "panic: pp_iter, type=%u", CxTYPE(cx));
    }

    retsv = &PL_sv_yes;
    if (0) {
      retno:
        retsv = &PL_sv_no;
    }
    /* pp_enteriter should have pre-extended the stack */
    assert(PL_stack_sp < PL_stack_max);
    *++PL_stack_sp =retsv;

    return PL_op->op_next;
}

/*
A description of how taint works in pattern matching and substitution.

This is all conditional on NO_TAINT_SUPPORT not being defined. Under
NO_TAINT_SUPPORT, taint-related operations should become no-ops.

While the pattern is being assembled/concatenated and then compiled,
PL_tainted will get set (via TAINT_set) if any component of the pattern
is tainted, e.g. /.*$tainted/.  At the end of pattern compilation,
the RXf_TAINTED flag is set on the pattern if PL_tainted is set (via
TAINT_get).  It will also be set if any component of the pattern matches
based on locale-dependent behavior.

When the pattern is copied, e.g. $r = qr/..../, the SV holding the ref to
the pattern is marked as tainted. This means that subsequent usage, such
as /x$r/, will set PL_tainted using TAINT_set, and thus RXf_TAINTED,
on the new pattern too.

RXf_TAINTED_SEEN is used post-execution by the get magic code
of $1 et al to indicate whether the returned value should be tainted.
It is the responsibility of the caller of the pattern (i.e. pp_match,
pp_subst etc) to set this flag for any other circumstances where $1 needs
to be tainted.

The taint behaviour of pp_subst (and pp_substcont) is quite complex.

There are three possible sources of taint
    * the source string
    * the pattern (both compile- and run-time, RXf_TAINTED / RXf_TAINTED_SEEN)
    * the replacement string (or expression under /e)
    
There are four destinations of taint and they are affected by the sources
according to the rules below:

    * the return value (not including /r):
	tainted by the source string and pattern, but only for the
	number-of-iterations case; boolean returns aren't tainted;
    * the modified string (or modified copy under /r):
	tainted by the source string, pattern, and replacement strings;
    * $1 et al:
	tainted by the pattern, and under 'use re "taint"', by the source
	string too;
    * PL_taint - i.e. whether subsequent code (e.g. in a /e block) is tainted:
	should always be unset before executing subsequent code.

The overall action of pp_subst is:

    * at the start, set bits in rxtainted indicating the taint status of
	the various sources.

    * After each pattern execution, update the SUBST_TAINT_PAT bit in
	rxtainted if RXf_TAINTED_SEEN has been set, to indicate that the
	pattern has subsequently become tainted via locale ops.

    * If control is being passed to pp_substcont to execute a /e block,
	save rxtainted in the CXt_SUBST block, for future use by
	pp_substcont.

    * Whenever control is being returned to perl code (either by falling
	off the "end" of pp_subst/pp_substcont, or by entering a /e block),
	use the flag bits in rxtainted to make all the appropriate types of
	destination taint visible; e.g. set RXf_TAINTED_SEEN so that $1
	et al will appear tainted.

pp_match is just a simpler version of the above.

*/

PP(pp_subst)
{
    dSP; dTARG;
    PMOP *pm = cPMOP;
    PMOP *rpm = pm;
    char *s;
    char *strend;
    const char *c;
    STRLEN clen;
    SSize_t iters = 0;
    SSize_t maxiters;
    bool once;
    U8 rxtainted = 0; /* holds various SUBST_TAINT_* flag bits.
			See "how taint works" above */
    char *orig;
    U8 r_flags;
    REGEXP *rx = PM_GETRE(pm);
    STRLEN len;
    int force_on_match = 0;
    const I32 oldsave = PL_savestack_ix;
    STRLEN slen;
    bool doutf8 = FALSE; /* whether replacement is in utf8 */
#ifdef PERL_ANY_COW
    bool was_cow;
#endif
    SV *nsv = NULL;
    /* known replacement string? */
    SV *dstr = (pm->op_pmflags & PMf_CONST) ? POPs : NULL;

    PERL_ASYNC_CHECK();

    if (PL_op->op_flags & OPf_STACKED)
	TARG = POPs;
    else if (ARGTARG)
	GETTARGET;
    else {
	TARG = DEFSV;
	EXTEND(SP,1);
    }

    SvGETMAGIC(TARG); /* must come before cow check */
#ifdef PERL_ANY_COW
    /* note that a string might get converted to COW during matching */
    was_cow = cBOOL(SvIsCOW(TARG));
#endif
    if (!(rpm->op_pmflags & PMf_NONDESTRUCT)) {
#ifndef PERL_ANY_COW
	if (SvIsCOW(TARG))
	    sv_force_normal_flags(TARG,0);
#endif
	if ((SvREADONLY(TARG)
		|| ( ((SvTYPE(TARG) == SVt_PVGV && isGV_with_GP(TARG))
		      || SvTYPE(TARG) > SVt_PVLV)
		     && !(SvTYPE(TARG) == SVt_PVGV && SvFAKE(TARG)))))
	    Perl_croak_no_modify();
    }
    PUTBACK;

    orig = SvPV_nomg(TARG, len);
    /* note we don't (yet) force the var into being a string; if we fail
     * to match, we leave as-is; on successful match however, we *will*
     * coerce into a string, then repeat the match */
    if (!SvPOKp(TARG) || SvTYPE(TARG) == SVt_PVGV || SvVOK(TARG))
	force_on_match = 1;

    /* only replace once? */
    once = !(rpm->op_pmflags & PMf_GLOBAL);

    /* See "how taint works" above */
    if (TAINTING_get) {
	rxtainted  = (
	    (SvTAINTED(TARG) ? SUBST_TAINT_STR : 0)
	  | (RX_ISTAINTED(rx) ? SUBST_TAINT_PAT : 0)
	  | ((pm->op_pmflags & PMf_RETAINT) ? SUBST_TAINT_RETAINT : 0)
	  | ((once && !(rpm->op_pmflags & PMf_NONDESTRUCT))
		? SUBST_TAINT_BOOLRET : 0));
	TAINT_NOT;
    }

  force_it:
    if (!pm || !orig)
	DIE(aTHX_ "panic: pp_subst, pm=%p, orig=%p", pm, orig);

    strend = orig + len;
    slen = DO_UTF8(TARG) ? utf8_length((U8*)orig, (U8*)strend) : len;
    maxiters = 2 * slen + 10;	/* We can match twice at each
				   position, once with zero-length,
				   second time with non-zero. */

    if (!RX_PRELEN(rx) && PL_curpm
     && !ReANY(rx)->mother_re) {
	pm = PL_curpm;
	rx = PM_GETRE(pm);
    }

#ifdef PERL_SAWAMPERSAND
    r_flags = (    RX_NPARENS(rx)
                || PL_sawampersand
                || (RX_EXTFLAGS(rx) & (RXf_EVAL_SEEN|RXf_PMf_KEEPCOPY))
                || (rpm->op_pmflags & PMf_KEEPCOPY)
              )
          ? REXEC_COPY_STR
          : 0;
#else
    r_flags = REXEC_COPY_STR;
#endif

    if (!CALLREGEXEC(rx, orig, strend, orig, 0, TARG, NULL, r_flags))
    {
	SPAGAIN;
	PUSHs(rpm->op_pmflags & PMf_NONDESTRUCT ? TARG : &PL_sv_no);
	LEAVE_SCOPE(oldsave);
	RETURN;
    }
    PL_curpm = pm;

    /* known replacement string? */
    if (dstr) {
	/* replacement needing upgrading? */
	if (DO_UTF8(TARG) && !doutf8) {
	     nsv = sv_newmortal();
	     SvSetSV(nsv, dstr);
	     if (IN_ENCODING)
		  sv_recode_to_utf8(nsv, _get_encoding());
	     else
		  sv_utf8_upgrade(nsv);
	     c = SvPV_const(nsv, clen);
	     doutf8 = TRUE;
	}
	else {
	    c = SvPV_const(dstr, clen);
	    doutf8 = DO_UTF8(dstr);
	}

	if (SvTAINTED(dstr))
	    rxtainted |= SUBST_TAINT_REPL;
    }
    else {
	c = NULL;
	doutf8 = FALSE;
    }
    
    /* can do inplace substitution? */
    if (c
#ifdef PERL_ANY_COW
	&& !was_cow
#endif
        && (I32)clen <= RX_MINLENRET(rx)
        && (  once
           || !(r_flags & REXEC_COPY_STR)
           || (!SvGMAGICAL(dstr) && !(RX_EXTFLAGS(rx) & RXf_EVAL_SEEN))
           )
        && !(RX_EXTFLAGS(rx) & RXf_NO_INPLACE_SUBST)
	&& (!doutf8 || SvUTF8(TARG))
	&& !(rpm->op_pmflags & PMf_NONDESTRUCT))
    {

#ifdef PERL_ANY_COW
        /* string might have got converted to COW since we set was_cow */
	if (SvIsCOW(TARG)) {
	  if (!force_on_match)
	    goto have_a_cow;
	  assert(SvVOK(TARG));
	}
#endif
	if (force_on_match) {
            /* redo the first match, this time with the orig var
             * forced into being a string */
	    force_on_match = 0;
	    orig = SvPV_force_nomg(TARG, len);
	    goto force_it;
	}

	if (once) {
            char *d, *m;
	    if (RX_MATCH_TAINTED(rx)) /* run time pattern taint, eg locale */
		rxtainted |= SUBST_TAINT_PAT;
	    m = orig + RX_OFFS(rx)[0].start;
	    d = orig + RX_OFFS(rx)[0].end;
	    s = orig;
	    if (m - s > strend - d) {  /* faster to shorten from end */
                I32 i;
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
	    else {	/* faster from front */
                I32 i = m - s;
		d -= clen;
                if (i > 0)
                    Move(s, d - i, i, char);
		sv_chop(TARG, d-i);
		if (clen)
		    Copy(c, d, clen, char);
	    }
	    SPAGAIN;
	    PUSHs(&PL_sv_yes);
	}
	else {
            char *d, *m;
            d = s = RX_OFFS(rx)[0].start + orig;
	    do {
                I32 i;
		if (UNLIKELY(iters++ > maxiters))
		    DIE(aTHX_ "Substitution loop");
		if (UNLIKELY(RX_MATCH_TAINTED(rx))) /* run time pattern taint, eg locale */
		    rxtainted |= SUBST_TAINT_PAT;
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
	    } while (CALLREGEXEC(rx, s, strend, orig,
				 s == m, /* don't match same null twice */
				 TARG, NULL,
                     REXEC_NOT_FIRST|REXEC_IGNOREPOS|REXEC_FAIL_ON_UNDERFLOW));
	    if (s != d) {
                I32 i = strend - s;
		SvCUR_set(TARG, d - SvPVX_const(TARG) + i);
		Move(s, d, i+1, char);		/* include the NUL */
	    }
	    SPAGAIN;
	    mPUSHi(iters);
	}
    }
    else {
	bool first;
        char *m;
	SV *repl;
	if (force_on_match) {
            /* redo the first match, this time with the orig var
             * forced into being a string */
	    force_on_match = 0;
	    if (rpm->op_pmflags & PMf_NONDESTRUCT) {
		/* I feel that it should be possible to avoid this mortal copy
		   given that the code below copies into a new destination.
		   However, I suspect it isn't worth the complexity of
		   unravelling the C<goto force_it> for the small number of
		   cases where it would be viable to drop into the copy code. */
		TARG = sv_2mortal(newSVsv(TARG));
	    }
	    orig = SvPV_force_nomg(TARG, len);
	    goto force_it;
	}
#ifdef PERL_ANY_COW
      have_a_cow:
#endif
	if (RX_MATCH_TAINTED(rx)) /* run time pattern taint, eg locale */
	    rxtainted |= SUBST_TAINT_PAT;
	repl = dstr;
        s = RX_OFFS(rx)[0].start + orig;
	dstr = newSVpvn_flags(orig, s-orig,
                    SVs_TEMP | (DO_UTF8(TARG) ? SVf_UTF8 : 0));
	if (!c) {
	    PERL_CONTEXT *cx;
	    SPAGAIN;
            m = orig;
	    /* note that a whole bunch of local vars are saved here for
	     * use by pp_substcont: here's a list of them in case you're
	     * searching for places in this sub that uses a particular var:
	     * iters maxiters r_flags oldsave rxtainted orig dstr targ
	     * s m strend rx once */
	    CX_PUSHSUBST(cx);
	    RETURNOP(cPMOP->op_pmreplrootu.op_pmreplroot);
	}
	first = TRUE;
	do {
	    if (UNLIKELY(iters++ > maxiters))
		DIE(aTHX_ "Substitution loop");
	    if (UNLIKELY(RX_MATCH_TAINTED(rx)))
		rxtainted |= SUBST_TAINT_PAT;
	    if (RX_MATCH_COPIED(rx) && RX_SUBBEG(rx) != orig) {
		char *old_s    = s;
		char *old_orig = orig;
                assert(RX_SUBOFFSET(rx) == 0);

		orig = RX_SUBBEG(rx);
		s = orig + (old_s - old_orig);
		strend = s + (strend - old_s);
	    }
	    m = RX_OFFS(rx)[0].start + orig;
	    sv_catpvn_nomg_maybeutf8(dstr, s, m - s, DO_UTF8(TARG));
	    s = RX_OFFS(rx)[0].end + orig;
	    if (first) {
		/* replacement already stringified */
	      if (clen)
		sv_catpvn_nomg_maybeutf8(dstr, c, clen, doutf8);
	      first = FALSE;
	    }
	    else {
		if (IN_ENCODING) {
		    if (!nsv) nsv = sv_newmortal();
		    sv_copypv(nsv, repl);
		    if (!DO_UTF8(nsv)) sv_recode_to_utf8(nsv, _get_encoding());
		    sv_catsv(dstr, nsv);
		}
		else sv_catsv(dstr, repl);
		if (UNLIKELY(SvTAINTED(repl)))
		    rxtainted |= SUBST_TAINT_REPL;
	    }
	    if (once)
		break;
	} while (CALLREGEXEC(rx, s, strend, orig,
                             s == m,    /* Yields minend of 0 or 1 */
			     TARG, NULL,
                    REXEC_NOT_FIRST|REXEC_IGNOREPOS|REXEC_FAIL_ON_UNDERFLOW));
        assert(strend >= s);
	sv_catpvn_nomg_maybeutf8(dstr, s, strend - s, DO_UTF8(TARG));

	if (rpm->op_pmflags & PMf_NONDESTRUCT) {
	    /* From here on down we're using the copy, and leaving the original
	       untouched.  */
	    TARG = dstr;
	    SPAGAIN;
	    PUSHs(dstr);
	} else {
#ifdef PERL_ANY_COW
	    /* The match may make the string COW. If so, brilliant, because
	       that's just saved us one malloc, copy and free - the regexp has
	       donated the old buffer, and we malloc an entirely new one, rather
	       than the regexp malloc()ing a buffer and copying our original,
	       only for us to throw it away here during the substitution.  */
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
	    SvFLAGS(TARG) |= SvUTF8(dstr);
	    SvPV_set(dstr, NULL);

	    SPAGAIN;
	    mPUSHi(iters);
	}
    }

    if (!(rpm->op_pmflags & PMf_NONDESTRUCT)) {
	(void)SvPOK_only_UTF8(TARG);
    }

    /* See "how taint works" above */
    if (TAINTING_get) {
	if ((rxtainted & SUBST_TAINT_PAT) ||
	    ((rxtainted & (SUBST_TAINT_STR|SUBST_TAINT_RETAINT)) ==
				(SUBST_TAINT_STR|SUBST_TAINT_RETAINT))
	)
	    (RX_MATCH_TAINTED_on(rx)); /* taint $1 et al */

	if (!(rxtainted & SUBST_TAINT_BOOLRET)
	    && (rxtainted & (SUBST_TAINT_STR|SUBST_TAINT_PAT))
	)
	    SvTAINTED_on(TOPs);  /* taint return value */
	else
	    SvTAINTED_off(TOPs);  /* may have got tainted earlier */

	/* needed for mg_set below */
	TAINT_set(
	  cBOOL(rxtainted & (SUBST_TAINT_STR|SUBST_TAINT_PAT|SUBST_TAINT_REPL))
        );
	SvTAINT(TARG);
    }
    SvSETMAGIC(TARG); /* PL_tainted must be correctly set for this mg_set */
    TAINT_NOT;
    LEAVE_SCOPE(oldsave);
    RETURN;
}

PP(pp_grepwhile)
{
    dSP;

    if (SvTRUEx(POPs))
	PL_stack_base[PL_markstack_ptr[-1]++] = PL_stack_base[*PL_markstack_ptr];
    ++*PL_markstack_ptr;
    FREETMPS;
    LEAVE_with_name("grep_item");					/* exit inner scope */

    /* All done yet? */
    if (UNLIKELY(PL_stack_base + *PL_markstack_ptr > SP)) {
	I32 items;
	const U8 gimme = GIMME_V;

	LEAVE_with_name("grep");					/* exit outer scope */
	(void)POPMARK;				/* pop src */
	items = --*PL_markstack_ptr - PL_markstack_ptr[-1];
	(void)POPMARK;				/* pop dst */
	SP = PL_stack_base + POPMARK;		/* pop original mark */
	if (gimme == G_SCALAR) {
		dTARGET;
		XPUSHi(items);
	}
	else if (gimme == G_ARRAY)
	    SP += items;
	RETURN;
    }
    else {
	SV *src;

	ENTER_with_name("grep_item");					/* enter inner scope */
	SAVEVPTR(PL_curpm);

	src = PL_stack_base[TOPMARK];
	if (SvPADTMP(src)) {
	    src = PL_stack_base[TOPMARK] = sv_mortalcopy(src);
	    PL_tmps_floor++;
	}
	SvTEMP_off(src);
	DEFSV_set(src);

	RETURNOP(cLOGOP->op_other);
    }
}

/* leave_adjust_stacks():
 *
 * Process a scope's return args (in the range from_sp+1 .. PL_stack_sp),
 * positioning them at to_sp+1 onwards, and do the equivalent of a
 * FREEMPS and TAINT_NOT.
 *
 * Not intended to be called in void context.
 *
 * When leaving a sub, eval, do{} or other scope, the things that need
 * doing to process the return args are:
 *    * in scalar context, only return the last arg (or PL_sv_undef if none);
 *    * for the types of return that return copies of their args (such
 *      as rvalue sub return), make a mortal copy of every return arg,
 *      except where we can optimise the copy away without it being
 *      semantically visible;
 *    * make sure that the arg isn't prematurely freed; in the case of an
 *      arg not copied, this may involve mortalising it. For example, in
 *      C<sub f { my $x = ...; $x }>, $x would be freed when we do
 *      CX_LEAVE_SCOPE(cx) unless it's protected or copied.
 *
 * What condition to use when deciding whether to pass the arg through
 * or make a copy, is determined by the 'pass' arg; its valid values are:
 *   0: rvalue sub/eval exit
 *   1: other rvalue scope exit
 *   2: :lvalue sub exit in rvalue context
 *   3: :lvalue sub exit in lvalue context and other lvalue scope exits
 *
 * There is a big issue with doing a FREETMPS. We would like to free any
 * temps created by the last statement which the sub executed, rather than
 * leaving them for the caller. In a situation where a sub call isn't
 * soon followed by a nextstate (e.g. nested recursive calls, a la
 * fibonacci()), temps can accumulate, causing memory and performance
 * issues.
 *
 * On the other hand, we don't want to free any TEMPs which are keeping
 * alive any return args that we skipped copying; nor do we wish to undo
 * any mortalising done here.
 *
 * The solution is to split the temps stack frame into two, with a cut
 * point delineating the two halves. We arrange that by the end of this
 * function, all the temps stack frame entries we wish to keep are in the
 * range  PL_tmps_floor+1.. tmps_base-1, while the ones to free now are in
 * the range  tmps_base .. PL_tmps_ix.  During the course of this
 * function, tmps_base starts off as PL_tmps_floor+1, then increases
 * whenever we find or create a temp that we know should be kept. In
 * general the stuff above tmps_base is undecided until we reach the end,
 * and we may need a sort stage for that.
 *
 * To determine whether a TEMP is keeping a return arg alive, every
 * arg that is kept rather than copied and which has the SvTEMP flag
 * set, has the flag temporarily unset, to mark it. At the end we scan
 * the temps stack frame above the cut for entries without SvTEMP and
 * keep them, while turning SvTEMP on again. Note that if we die before
 * the SvTEMPs flags are set again, its safe: at worst, subsequent use of
 * those SVs may be slightly less efficient.
 *
 * In practice various optimisations for some common cases mean we can
 * avoid most of the scanning and swapping about with the temps stack.
 */

void
Perl_leave_adjust_stacks(pTHX_ SV **from_sp, SV **to_sp, U8 gimme, int pass)
{
    dVAR;
    dSP;
    SSize_t tmps_base; /* lowest index into tmps stack that needs freeing now */
    SSize_t nargs;

    PERL_ARGS_ASSERT_LEAVE_ADJUST_STACKS;

    TAINT_NOT;

    if (gimme == G_ARRAY) {
        nargs = SP - from_sp;
        from_sp++;
    }
    else {
        assert(gimme == G_SCALAR);
        if (UNLIKELY(from_sp >= SP)) {
            /* no return args */
            assert(from_sp == SP);
            EXTEND(SP, 1);
            *++SP = &PL_sv_undef;
            to_sp = SP;
            nargs   = 0;
        }
        else {
            from_sp = SP;
            nargs   = 1;
        }
    }

    /* common code for G_SCALAR and G_ARRAY */

    tmps_base = PL_tmps_floor + 1;

    assert(nargs >= 0);
    if (nargs) {
        /* pointer version of tmps_base. Not safe across temp stack
         * reallocs. */
        SV **tmps_basep;

        EXTEND_MORTAL(nargs); /* one big extend for worst-case scenario */
        tmps_basep = PL_tmps_stack + tmps_base;

        /* process each return arg */

        do {
            SV *sv = *from_sp++;

            assert(PL_tmps_ix + nargs < PL_tmps_max);
#ifdef DEBUGGING
            /* PADTMPs with container set magic shouldn't appear in the
             * wild. This assert is more important for pp_leavesublv(),
             * but by testing for it here, we're more likely to catch
             * bad cases (what with :lvalue subs not being widely
             * deployed). The two issues are that for something like
             *     sub :lvalue { $tied{foo} }
             * or
             *     sub :lvalue { substr($foo,1,2) }
             * pp_leavesublv() will croak if the sub returns a PADTMP,
             * and currently functions like pp_substr() return a mortal
             * rather than using their PADTMP when returning a PVLV.
             * This is because the PVLV will hold a ref to $foo,
             * so $foo would get delayed in being freed while
             * the PADTMP SV remained in the PAD.
             * So if this assert fails it means either:
             *  1) there is pp code similar to pp_substr that is
             *     returning a PADTMP instead of a mortal, and probably
             *     needs fixing, or
             *  2) pp_leavesublv is making unwarranted assumptions
             *     about always croaking on a PADTMP
             */
            if (SvPADTMP(sv) && SvSMAGICAL(sv)) {
                MAGIC *mg;
                for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic) {
                    assert(PERL_MAGIC_TYPE_IS_VALUE_MAGIC(mg->mg_type));
                }
            }
#endif

            if (
               pass == 0 ? (SvTEMP(sv) && !SvMAGICAL(sv) && SvREFCNT(sv) == 1)
             : pass == 1 ? ((SvTEMP(sv) || SvPADTMP(sv)) && !SvMAGICAL(sv) && SvREFCNT(sv) == 1)
             : pass == 2 ? (!SvPADTMP(sv))
             : 1)
            {
                /* pass through: skip copy for logic or optimisation
                 * reasons; instead mortalise it, except that ... */
                *++to_sp = sv;

                if (SvTEMP(sv)) {
                    /* ... since this SV is an SvTEMP , we don't need to
                     * re-mortalise it; instead we just need to ensure
                     * that its existing entry in the temps stack frame
                     * ends up below the cut and so avoids being freed
                     * this time round. We mark it as needing to be kept
                     * by temporarily unsetting SvTEMP; then at the end,
                     * we shuffle any !SvTEMP entries on the tmps stack
                     * back below the cut.
                     * However, there's a significant chance that there's
                     * a 1:1 correspondence between the first few (or all)
                     * elements in the return args stack frame and those
                     * in the temps stack frame; e,g.:
                     *      sub f { ....; map {...} .... },
                     * or if we're exiting multiple scopes and one of the
                     * inner scopes has already made mortal copies of each
                     * return arg.
                     *
                     * If so, this arg sv will correspond to the next item
                     * on the tmps stack above the cut, and so can be kept
                     * merely by moving the cut boundary up one, rather
                     * than messing with SvTEMP.  If all args are 1:1 then
                     * we can avoid the sorting stage below completely.
                     *
                     * If there are no items above the cut on the tmps
                     * stack, then the SvTEMP must comne from an item
                     * below the cut, so there's nothing to do.
                     */
                    if (tmps_basep <= &PL_tmps_stack[PL_tmps_ix]) {
                        if (sv == *tmps_basep)
                            tmps_basep++;
                        else
                            SvTEMP_off(sv);
                    }
                }
                else if (!SvPADTMP(sv)) {
                    /* mortalise arg to avoid it being freed during save
                     * stack unwinding. Pad tmps don't need mortalising as
                     * they're never freed. This is the equivalent of
                     * sv_2mortal(SvREFCNT_inc(sv)), except that:
                     *  * it assumes that the temps stack has already been
                     *    extended;
                     *  * it puts the new item at the cut rather than at
                     *    ++PL_tmps_ix, moving the previous occupant there
                     *    instead.
                     */
                    if (!SvIMMORTAL(sv)) {
                        SvREFCNT_inc_simple_void_NN(sv);
                        SvTEMP_on(sv);
                        /* Note that if there's nothing above the cut,
                         * this copies the garbage one slot above
                         * PL_tmps_ix onto itself. This is harmless (the
                         * stack's already been extended), but might in
                         * theory trigger warnings from tools like ASan
                         */
                        PL_tmps_stack[++PL_tmps_ix] = *tmps_basep;
                        *tmps_basep++ = sv;
                    }
                }
            }
            else {
                /* Make a mortal copy of the SV.
                 * The following code is the equivalent of sv_mortalcopy()
                 * except that:
                 *  * it assumes the temps stack has already been extended;
                 *  * it optimises the copying for some simple SV types;
                 *  * it puts the new item at the cut rather than at
                 *    ++PL_tmps_ix, moving the previous occupant there
                 *    instead.
                 */
                SV *newsv = newSV(0);

                PL_tmps_stack[++PL_tmps_ix] = *tmps_basep;
                /* put it on the tmps stack early so it gets freed if we die */
                *tmps_basep++ = newsv;
                *++to_sp = newsv;

                if (SvTYPE(sv) <= SVt_IV) {
                    /* arg must be one of undef, IV/UV, or RV: skip
                     * sv_setsv_flags() and do the copy directly */
                    U32 dstflags;
                    U32 srcflags = SvFLAGS(sv);

                    assert(!SvGMAGICAL(sv));
                    if (srcflags & (SVf_IOK|SVf_ROK)) {
                        SET_SVANY_FOR_BODYLESS_IV(newsv);

                        if (srcflags & SVf_ROK) {
                            newsv->sv_u.svu_rv = SvREFCNT_inc(SvRV(sv));
                            /* SV type plus flags */
                            dstflags = (SVt_IV|SVf_ROK|SVs_TEMP);
                        }
                        else {
                            /* both src and dst are <= SVt_IV, so sv_any
                             * points to the head; so access the heads
                             * directly rather than going via sv_any.
                             */
                            assert(    &(sv->sv_u.svu_iv)
                                    == &(((XPVIV*) SvANY(sv))->xiv_iv));
                            assert(    &(newsv->sv_u.svu_iv)
                                    == &(((XPVIV*) SvANY(newsv))->xiv_iv));
                            newsv->sv_u.svu_iv = sv->sv_u.svu_iv;
                            /* SV type plus flags */
                            dstflags = (SVt_IV|SVf_IOK|SVp_IOK|SVs_TEMP
                                            |(srcflags & SVf_IVisUV));
                        }
                    }
                    else {
                        assert(!(srcflags & SVf_OK));
                        dstflags = (SVt_NULL|SVs_TEMP); /* SV type plus flags */
                    }
                    SvFLAGS(newsv) = dstflags;

                }
                else {
                    /* do the full sv_setsv() */
                    SSize_t old_base;

                    SvTEMP_on(newsv);
                    old_base = tmps_basep - PL_tmps_stack;
                    SvGETMAGIC(sv);
                    sv_setsv_flags(newsv, sv, SV_DO_COW_SVSETSV);
                    /* the mg_get or sv_setsv might have created new temps
                     * or realloced the tmps stack; regrow and reload */
                    EXTEND_MORTAL(nargs);
                    tmps_basep = PL_tmps_stack + old_base;
                    TAINT_NOT;	/* Each item is independent */
                }

            }
        } while (--nargs);

        /* If there are any temps left above the cut, we need to sort
         * them into those to keep and those to free. The only ones to
         * keep are those for which we've temporarily unset SvTEMP.
         * Work inwards from the two ends at tmps_basep .. PL_tmps_ix,
         * swapping pairs as necessary. Stop when we meet in the middle.
         */
        {
            SV **top = PL_tmps_stack + PL_tmps_ix;
            while (tmps_basep <= top) {
                SV *sv = *top;
                if (SvTEMP(sv))
                    top--;
                else {
                    SvTEMP_on(sv);
                    *top = *tmps_basep;
                    *tmps_basep = sv;
                    tmps_basep++;
                }
            }
        }

        tmps_base = tmps_basep - PL_tmps_stack;
    }

    PL_stack_sp = to_sp;

    /* unrolled FREETMPS() but using tmps_base-1 rather than PL_tmps_floor */
    while (PL_tmps_ix >= tmps_base) {
        SV* const sv = PL_tmps_stack[PL_tmps_ix--];
#ifdef PERL_POISON
        PoisonWith(PL_tmps_stack + PL_tmps_ix + 1, 1, SV *, 0xAB);
#endif
        if (LIKELY(sv)) {
            SvTEMP_off(sv);
            SvREFCNT_dec_NN(sv); /* note, can modify tmps_ix!!! */
        }
    }
}


/* also tail-called by pp_return */

PP(pp_leavesub)
{
    U8 gimme;
    PERL_CONTEXT *cx;
    SV **oldsp;
    OP *retop;

    cx = CX_CUR();
    assert(CxTYPE(cx) == CXt_SUB);

    if (CxMULTICALL(cx)) {
        /* entry zero of a stack is always PL_sv_undef, which
         * simplifies converting a '()' return into undef in scalar context */
        assert(PL_stack_sp > PL_stack_base || *PL_stack_base == &PL_sv_undef);
	return 0;
    }

    gimme = cx->blk_gimme;
    oldsp = PL_stack_base + cx->blk_oldsp; /* last arg of previous frame */

    if (gimme == G_VOID)
        PL_stack_sp = oldsp;
    else
        leave_adjust_stacks(oldsp, oldsp, gimme, 0);

    CX_LEAVE_SCOPE(cx);
    cx_popsub(cx);	/* Stack values are safe: release CV and @_ ... */
    cx_popblock(cx);
    retop = cx->blk_sub.retop;
    CX_POP(cx);

    return retop;
}


/* clear (if possible) or abandon the current @_. If 'abandon' is true,
 * forces an abandon */

void
Perl_clear_defarray(pTHX_ AV* av, bool abandon)
{
    const SSize_t fill = AvFILLp(av);

    PERL_ARGS_ASSERT_CLEAR_DEFARRAY;

    if (LIKELY(!abandon && SvREFCNT(av) == 1 && !SvMAGICAL(av))) {
        av_clear(av);
        AvREIFY_only(av);
    }
    else {
        AV *newav = newAV();
        av_extend(newav, fill);
        AvREIFY_only(newav);
        PAD_SVl(0) = MUTABLE_SV(newav);
        SvREFCNT_dec_NN(av);
    }
}


PP(pp_entersub)
{
    dSP; dPOPss;
    GV *gv;
    CV *cv;
    PERL_CONTEXT *cx;
    I32 old_savestack_ix;

    if (UNLIKELY(!sv))
	goto do_die;

    /* Locate the CV to call:
     * - most common case: RV->CV: f(), $ref->():
     *   note that if a sub is compiled before its caller is compiled,
     *   the stash entry will be a ref to a CV, rather than being a GV.
     * - second most common case: CV: $ref->method()
     */

    /* a non-magic-RV -> CV ? */
    if (LIKELY( (SvFLAGS(sv) & (SVf_ROK|SVs_GMG)) == SVf_ROK)) {
        cv = MUTABLE_CV(SvRV(sv));
        if (UNLIKELY(SvOBJECT(cv))) /* might be overloaded */
            goto do_ref;
    }
    else
        cv = MUTABLE_CV(sv);

    /* a CV ? */
    if (UNLIKELY(SvTYPE(cv) != SVt_PVCV)) {
        /* handle all the weird cases */
        switch (SvTYPE(sv)) {
        case SVt_PVLV:
            if (!isGV_with_GP(sv))
                goto do_default;
            /* FALLTHROUGH */
        case SVt_PVGV:
            cv = GvCVu((const GV *)sv);
            if (UNLIKELY(!cv)) {
                HV *stash;
                cv = sv_2cv(sv, &stash, &gv, 0);
                if (!cv) {
                    old_savestack_ix = PL_savestack_ix;
                    goto try_autoload;
                }
            }
            break;

        default:
          do_default:
            SvGETMAGIC(sv);
            if (SvROK(sv)) {
              do_ref:
                if (UNLIKELY(SvAMAGIC(sv))) {
                    sv = amagic_deref_call(sv, to_cv_amg);
                    /* Don't SPAGAIN here.  */
                }
            }
            else {
                const char *sym;
                STRLEN len;
                if (UNLIKELY(!SvOK(sv)))
                    DIE(aTHX_ PL_no_usym, "a subroutine");

                if (UNLIKELY(sv == &PL_sv_yes)) { /* unfound import, ignore */
                    if (PL_op->op_flags & OPf_STACKED) /* hasargs */
                        SP = PL_stack_base + POPMARK;
                    else
                        (void)POPMARK;
                    if (GIMME_V == G_SCALAR)
                        PUSHs(&PL_sv_undef);
                    RETURN;
                }

                sym = SvPV_nomg_const(sv, len);
                if (PL_op->op_private & HINT_STRICT_REFS)
                    DIE(aTHX_ "Can't use string (\"%" SVf32 "\"%s) as a subroutine ref while \"strict refs\" in use", sv, len>32 ? "..." : "");
                cv = get_cvn_flags(sym, len, GV_ADD|SvUTF8(sv));
                break;
            }
            cv = MUTABLE_CV(SvRV(sv));
            if (LIKELY(SvTYPE(cv) == SVt_PVCV))
                break;
            /* FALLTHROUGH */
        case SVt_PVHV:
        case SVt_PVAV:
          do_die:
            DIE(aTHX_ "Not a CODE reference");
        }
    }

    /* At this point we want to save PL_savestack_ix, either by doing a
     * cx_pushsub(), or for XS, doing an ENTER. But we don't yet know the final
     * CV we will be using (so we don't know whether its XS, so we can't
     * cx_pushsub() or ENTER yet), and determining cv may itself push stuff on
     * the save stack. So remember where we are currently on the save
     * stack, and later update the CX or scopestack entry accordingly. */
    old_savestack_ix = PL_savestack_ix;

    /* these two fields are in a union. If they ever become separate,
     * we have to test for both of them being null below */
    assert(cv);
    assert((void*)&CvROOT(cv) == (void*)&CvXSUB(cv));
    while (UNLIKELY(!CvROOT(cv))) {
	GV* autogv;
	SV* sub_name;

	/* anonymous or undef'd function leaves us no recourse */
	if (CvLEXICAL(cv) && CvHASGV(cv))
	    DIE(aTHX_ "Undefined subroutine &%"SVf" called",
		       SVfARG(cv_name(cv, NULL, 0)));
	if (CvANON(cv) || !CvHASGV(cv)) {
	    DIE(aTHX_ "Undefined subroutine called");
	}

	/* autoloaded stub? */
	if (cv != GvCV(gv = CvGV(cv))) {
	    cv = GvCV(gv);
	}
	/* should call AUTOLOAD now? */
	else {
          try_autoload:
	    autogv = gv_autoload_pvn(GvSTASH(gv), GvNAME(gv), GvNAMELEN(gv),
				   GvNAMEUTF8(gv) ? SVf_UTF8 : 0);
            cv = autogv ? GvCV(autogv) : NULL;
	}
	if (!cv) {
            sub_name = sv_newmortal();
            gv_efullname3(sub_name, gv, NULL);
            DIE(aTHX_ "Undefined subroutine &%"SVf" called", SVfARG(sub_name));
        }
    }

    /* unrolled "CvCLONE(cv) && ! CvCLONED(cv)" */
    if (UNLIKELY((CvFLAGS(cv) & (CVf_CLONE|CVf_CLONED)) == CVf_CLONE))
	DIE(aTHX_ "Closure prototype called");

    if (UNLIKELY((PL_op->op_private & OPpENTERSUB_DB) && GvCV(PL_DBsub)
            && !CvNODEBUG(cv)))
    {
	 Perl_get_db_sub(aTHX_ &sv, cv);
	 if (CvISXSUB(cv))
	     PL_curcopdb = PL_curcop;
         if (CvLVALUE(cv)) {
             /* check for lsub that handles lvalue subroutines */
	     cv = GvCV(gv_fetchpvs("DB::lsub", GV_ADDMULTI, SVt_PVCV));
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
	PADLIST *padlist;
        I32 depth;
        bool hasargs;
        U8 gimme;

        /* keep PADTMP args alive throughout the call (we need to do this
         * because @_ isn't refcounted). Note that we create the mortals
         * in the caller's tmps frame, so they won't be freed until after
         * we return from the sub.
         */
	{
            SV **svp = MARK;
            while (svp < SP) {
                SV *sv = *++svp;
                if (!sv)
                    continue;
                if (SvPADTMP(sv))
                    *svp = sv = sv_mortalcopy(sv);
                SvTEMP_off(sv);
	    }
        }

        gimme = GIMME_V;
	cx = cx_pushblock(CXt_SUB, gimme, MARK, old_savestack_ix);
        hasargs = cBOOL(PL_op->op_flags & OPf_STACKED);
	cx_pushsub(cx, cv, PL_op->op_next, hasargs);

	padlist = CvPADLIST(cv);
	if (UNLIKELY((depth = ++CvDEPTH(cv)) >= 2))
	    pad_push(padlist, depth);
	PAD_SET_CUR_NOSAVE(padlist, depth);
	if (LIKELY(hasargs)) {
	    AV *const av = MUTABLE_AV(PAD_SVl(0));
            SSize_t items;
            AV **defavp;

	    defavp = &GvAV(PL_defgv);
	    cx->blk_sub.savearray = *defavp;
	    *defavp = MUTABLE_AV(SvREFCNT_inc_simple_NN(av));

            /* it's the responsibility of whoever leaves a sub to ensure
             * that a clean, empty AV is left in pad[0]. This is normally
             * done by cx_popsub() */
            assert(!AvREAL(av) && AvFILLp(av) == -1);

            items = SP - MARK;
	    if (UNLIKELY(items - 1 > AvMAX(av))) {
                SV **ary = AvALLOC(av);
                AvMAX(av) = items - 1;
                Renew(ary, items, SV*);
                AvALLOC(av) = ary;
                AvARRAY(av) = ary;
            }

	    Copy(MARK+1,AvARRAY(av),items,SV*);
	    AvFILLp(av) = items - 1;
	}
	if (UNLIKELY((cx->blk_u16 & OPpENTERSUB_LVAL_MASK) == OPpLVAL_INTRO &&
	    !CvLVALUE(cv)))
            DIE(aTHX_ "Can't modify non-lvalue subroutine call of &%"SVf,
                SVfARG(cv_name(cv, NULL, 0)));
	/* warning must come *after* we fully set up the context
	 * stuff so that __WARN__ handlers can safely dounwind()
	 * if they want to
	 */
	if (UNLIKELY(depth == PERL_SUB_DEPTH_WARN
                && ckWARN(WARN_RECURSION)
                && !(PERLDB_SUB && cv == GvCV(PL_DBsub))))
	    sub_crush_depth(cv);
	RETURNOP(CvSTART(cv));
    }
    else {
	SSize_t markix = TOPMARK;
        bool is_scalar;

        ENTER;
        /* pretend we did the ENTER earlier */
	PL_scopestack[PL_scopestack_ix - 1] = old_savestack_ix;

	SAVETMPS;
	PUTBACK;

	if (UNLIKELY(((PL_op->op_private
	       & CX_PUSHSUB_GET_LVALUE_MASK(Perl_is_lvalue_sub)
             ) & OPpENTERSUB_LVAL_MASK) == OPpLVAL_INTRO &&
	    !CvLVALUE(cv)))
            DIE(aTHX_ "Can't modify non-lvalue subroutine call of &%"SVf,
                SVfARG(cv_name(cv, NULL, 0)));

	if (UNLIKELY(!(PL_op->op_flags & OPf_STACKED) && GvAV(PL_defgv))) {
	    /* Need to copy @_ to stack. Alternative may be to
	     * switch stack to @_, and copy return values
	     * back. This would allow popping @_ in XSUB, e.g.. XXXX */
	    AV * const av = GvAV(PL_defgv);
	    const SSize_t items = AvFILL(av) + 1;

	    if (items) {
		SSize_t i = 0;
		const bool m = cBOOL(SvRMAGICAL(av));
		/* Mark is at the end of the stack. */
		EXTEND(SP, items);
		for (; i < items; ++i)
		{
		    SV *sv;
		    if (m) {
			SV ** const svp = av_fetch(av, i, 0);
			sv = svp ? *svp : NULL;
		    }
		    else sv = AvARRAY(av)[i];
		    if (sv) SP[i+1] = sv;
		    else {
			SP[i+1] = newSVavdefelem(av, i, 1);
		    }
		}
		SP += items;
		PUTBACK ;		
	    }
	}
	else {
	    SV **mark = PL_stack_base + markix;
	    SSize_t items = SP - mark;
	    while (items--) {
		mark++;
		if (*mark && SvPADTMP(*mark)) {
		    *mark = sv_mortalcopy(*mark);
                }
	    }
	}
	/* We assume first XSUB in &DB::sub is the called one. */
	if (UNLIKELY(PL_curcopdb)) {
	    SAVEVPTR(PL_curcop);
	    PL_curcop = PL_curcopdb;
	    PL_curcopdb = NULL;
	}
	/* Do we need to open block here? XXXX */

        /* calculate gimme here as PL_op might get changed and then not
         * restored until the LEAVE further down */
        is_scalar = (GIMME_V == G_SCALAR);

	/* CvXSUB(cv) must not be NULL because newXS() refuses NULL xsub address */
	assert(CvXSUB(cv));
	CvXSUB(cv)(aTHX_ cv);

	/* Enforce some sanity in scalar context. */
	if (is_scalar) {
            SV **svp = PL_stack_base + markix + 1;
            if (svp != PL_stack_sp) {
                *svp = svp > PL_stack_sp ? &PL_sv_undef : *PL_stack_sp;
                PL_stack_sp = svp;
            }
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
	Perl_warner(aTHX_ packWARN(WARN_RECURSION), "Deep recursion on subroutine \"%"SVf"\"",
		    SVfARG(cv_name(cv,NULL,0)));
    }
}

PP(pp_aelem)
{
    dSP;
    SV** svp;
    SV* const elemsv = POPs;
    IV elem = SvIV(elemsv);
    AV *const av = MUTABLE_AV(POPs);
    const U32 lval = PL_op->op_flags & OPf_MOD || LVRET;
    const U32 defer = PL_op->op_private & OPpLVAL_DEFER;
    const bool localizing = PL_op->op_private & OPpLVAL_INTRO;
    bool preeminent = TRUE;
    SV *sv;

    if (UNLIKELY(SvROK(elemsv) && !SvGAMAGIC(elemsv) && ckWARN(WARN_MISC)))
	Perl_warner(aTHX_ packWARN(WARN_MISC),
		    "Use of reference \"%"SVf"\" as array index",
		    SVfARG(elemsv));
    if (UNLIKELY(SvTYPE(av) != SVt_PVAV))
	RETPUSHUNDEF;

    if (UNLIKELY(localizing)) {
	MAGIC *mg;
	HV *stash;

	/* If we can determine whether the element exist,
	 * Try to preserve the existenceness of a tied array
	 * element by using EXISTS and DELETE if possible.
	 * Fallback to FETCH and STORE otherwise. */
	if (SvCANEXISTDELETE(av))
	    preeminent = av_exists(av, elem);
    }

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
	if (!svp || !*svp) {
	    IV len;
	    if (!defer)
		DIE(aTHX_ PL_no_aelem, elem);
	    len = av_tindex(av);
	    mPUSHs(newSVavdefelem(av,
	    /* Resolve a negative index now, unless it points before the
	       beginning of the array, in which case record it for error
	       reporting in magic_setdefelem. */
		elem < 0 && len + elem >= 0 ? len + elem : elem,
		1));
	    RETURN;
	}
	if (UNLIKELY(localizing)) {
	    if (preeminent)
		save_aelem(av, elem, svp);
	    else
		SAVEADELETE(av, elem);
	}
	else if (PL_op->op_private & OPpDEREF) {
	    PUSHs(vivify_ref(*svp, PL_op->op_private & OPpDEREF));
	    RETURN;
	}
    }
    sv = (svp ? *svp : &PL_sv_undef);
    if (!lval && SvRMAGICAL(av) && SvGMAGICAL(sv)) /* see note in pp_helem() */
	mg_get(sv);
    PUSHs(sv);
    RETURN;
}

SV*
Perl_vivify_ref(pTHX_ SV *sv, U32 to_what)
{
    PERL_ARGS_ASSERT_VIVIFY_REF;

    SvGETMAGIC(sv);
    if (!SvOK(sv)) {
	if (SvREADONLY(sv))
	    Perl_croak_no_modify();
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
	SvGETMAGIC(sv);
    }
    if (SvGMAGICAL(sv)) {
	/* copy the sv without magic to prevent magic from being
	   executed twice */
	SV* msv = sv_newmortal();
	sv_setsv_nomg(msv, sv);
	return msv;
    }
    return sv;
}

PERL_STATIC_INLINE HV *
S_opmethod_stash(pTHX_ SV* meth)
{
    SV* ob;
    HV* stash;

    SV* const sv = PL_stack_base + TOPMARK == PL_stack_sp
	? (Perl_croak(aTHX_ "Can't call method \"%"SVf"\" without a "
			    "package or object reference", SVfARG(meth)),
	   (SV *)NULL)
	: *(PL_stack_base + TOPMARK + 1);

    PERL_ARGS_ASSERT_OPMETHOD_STASH;

    if (UNLIKELY(!sv))
       undefined:
	Perl_croak(aTHX_ "Can't call method \"%"SVf"\" on an undefined value",
		   SVfARG(meth));

    if (UNLIKELY(SvGMAGICAL(sv))) mg_get(sv);
    else if (SvIsCOW_shared_hash(sv)) { /* MyClass->meth() */
	stash = gv_stashsv(sv, GV_CACHE_ONLY);
	if (stash) return stash;
    }

    if (SvROK(sv))
	ob = MUTABLE_SV(SvRV(sv));
    else if (!SvOK(sv)) goto undefined;
    else if (isGV_with_GP(sv)) {
	if (!GvIO(sv))
	    Perl_croak(aTHX_ "Can't call method \"%"SVf"\" "
			     "without a package or object reference",
			      SVfARG(meth));
	ob = sv;
	if (SvTYPE(ob) == SVt_PVLV && LvTYPE(ob) == 'y') {
	    assert(!LvTARGLEN(ob));
	    ob = LvTARG(ob);
	    assert(ob);
	}
	*(PL_stack_base + TOPMARK + 1) = sv_2mortal(newRV(ob));
    }
    else {
	/* this isn't a reference */
	GV* iogv;
        STRLEN packlen;
        const char * const packname = SvPV_nomg_const(sv, packlen);
        const U32 packname_utf8 = SvUTF8(sv);
        stash = gv_stashpvn(packname, packlen, packname_utf8 | GV_CACHE_ONLY);
        if (stash) return stash;

	if (!(iogv = gv_fetchpvn_flags(
	        packname, packlen, packname_utf8, SVt_PVIO
	     )) ||
	    !(ob=MUTABLE_SV(GvIO(iogv))))
	{
	    /* this isn't the name of a filehandle either */
	    if (!packlen)
	    {
		Perl_croak(aTHX_ "Can't call method \"%"SVf"\" "
				 "without a package or object reference",
				  SVfARG(meth));
	    }
	    /* assume it's a package name */
	    stash = gv_stashpvn(packname, packlen, packname_utf8);
	    if (stash) return stash;
	    else return MUTABLE_HV(sv);
	}
	/* it _is_ a filehandle name -- replace with a reference */
	*(PL_stack_base + TOPMARK + 1) = sv_2mortal(newRV(MUTABLE_SV(iogv)));
    }

    /* if we got here, ob should be an object or a glob */
    if (!ob || !(SvOBJECT(ob)
		 || (isGV_with_GP(ob)
		     && (ob = MUTABLE_SV(GvIO((const GV *)ob)))
		     && SvOBJECT(ob))))
    {
	Perl_croak(aTHX_ "Can't call method \"%"SVf"\" on unblessed reference",
		   SVfARG((SvSCREAM(meth) && strEQ(SvPV_nolen_const(meth),"isa"))
                                        ? newSVpvs_flags("DOES", SVs_TEMP)
                                        : meth));
    }

    return SvSTASH(ob);
}

PP(pp_method)
{
    dSP;
    GV* gv;
    HV* stash;
    SV* const meth = TOPs;

    if (SvROK(meth)) {
        SV* const rmeth = SvRV(meth);
        if (SvTYPE(rmeth) == SVt_PVCV) {
            SETs(rmeth);
            RETURN;
        }
    }

    stash = opmethod_stash(meth);

    gv = gv_fetchmethod_sv_flags(stash, meth, GV_AUTOLOAD|GV_CROAK);
    assert(gv);

    SETs(isGV(gv) ? MUTABLE_SV(GvCV(gv)) : MUTABLE_SV(gv));
    RETURN;
}

#define METHOD_CHECK_CACHE(stash,cache,meth) 				\
    const HE* const he = hv_fetch_ent(cache, meth, 0, 0);		\
    if (he) {								\
        gv = MUTABLE_GV(HeVAL(he));					\
        if (isGV(gv) && GvCV(gv) && (!GvCVGEN(gv) || GvCVGEN(gv)	\
             == (PL_sub_generation + HvMROMETA(stash)->cache_gen)))	\
        {								\
            XPUSHs(MUTABLE_SV(GvCV(gv)));				\
            RETURN;							\
        }								\
    }									\

PP(pp_method_named)
{
    dSP;
    GV* gv;
    SV* const meth = cMETHOPx_meth(PL_op);
    HV* const stash = opmethod_stash(meth);

    if (LIKELY(SvTYPE(stash) == SVt_PVHV)) {
        METHOD_CHECK_CACHE(stash, stash, meth);
    }

    gv = gv_fetchmethod_sv_flags(stash, meth, GV_AUTOLOAD|GV_CROAK);
    assert(gv);

    XPUSHs(isGV(gv) ? MUTABLE_SV(GvCV(gv)) : MUTABLE_SV(gv));
    RETURN;
}

PP(pp_method_super)
{
    dSP;
    GV* gv;
    HV* cache;
    SV* const meth = cMETHOPx_meth(PL_op);
    HV* const stash = CopSTASH(PL_curcop);
    /* Actually, SUPER doesn't need real object's (or class') stash at all,
     * as it uses CopSTASH. However, we must ensure that object(class) is
     * correct (this check is done by S_opmethod_stash) */
    opmethod_stash(meth);

    if ((cache = HvMROMETA(stash)->super)) {
        METHOD_CHECK_CACHE(stash, cache, meth);
    }

    gv = gv_fetchmethod_sv_flags(stash, meth, GV_AUTOLOAD|GV_CROAK|GV_SUPER);
    assert(gv);

    XPUSHs(isGV(gv) ? MUTABLE_SV(GvCV(gv)) : MUTABLE_SV(gv));
    RETURN;
}

PP(pp_method_redir)
{
    dSP;
    GV* gv;
    SV* const meth = cMETHOPx_meth(PL_op);
    HV* stash = gv_stashsv(cMETHOPx_rclass(PL_op), 0);
    opmethod_stash(meth); /* not used but needed for error checks */

    if (stash) { METHOD_CHECK_CACHE(stash, stash, meth); }
    else stash = MUTABLE_HV(cMETHOPx_rclass(PL_op));

    gv = gv_fetchmethod_sv_flags(stash, meth, GV_AUTOLOAD|GV_CROAK);
    assert(gv);

    XPUSHs(isGV(gv) ? MUTABLE_SV(GvCV(gv)) : MUTABLE_SV(gv));
    RETURN;
}

PP(pp_method_redir_super)
{
    dSP;
    GV* gv;
    HV* cache;
    SV* const meth = cMETHOPx_meth(PL_op);
    HV* stash = gv_stashsv(cMETHOPx_rclass(PL_op), 0);
    opmethod_stash(meth); /* not used but needed for error checks */

    if (UNLIKELY(!stash)) stash = MUTABLE_HV(cMETHOPx_rclass(PL_op));
    else if ((cache = HvMROMETA(stash)->super)) {
         METHOD_CHECK_CACHE(stash, cache, meth);
    }

    gv = gv_fetchmethod_sv_flags(stash, meth, GV_AUTOLOAD|GV_CROAK|GV_SUPER);
    assert(gv);

    XPUSHs(isGV(gv) ? MUTABLE_SV(GvCV(gv)) : MUTABLE_SV(gv));
    RETURN;
}

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */

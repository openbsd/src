/*    pp.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
 *    2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * 'It's a big house this, and very peculiar.  Always a bit more
 *  to discover, and no knowing what you'll find round a corner.
 *  And Elves, sir!'                            --Samwise Gamgee
 *
 *     [p.225 of _The Lord of the Rings_, II/i: "Many Meetings"]
 */

/* This file contains general pp ("push/pop") functions that execute the
 * opcodes that make up a perl program. A typical pp function expects to
 * find its arguments on the stack, and usually pushes its results onto
 * the stack, hence the 'pp' terminology. Each OP structure contains
 * a pointer to the relevant pp_foo() function.
 */

#include "EXTERN.h"
#define PERL_IN_PP_C
#include "perl.h"
#include "keywords.h"

#include "reentr.h"
#include "regcharclass.h"

/* XXX I can't imagine anyone who doesn't have this actually _needs_
   it, since pid_t is an integral type.
   --AD  2/20/1998
*/
#ifdef NEED_GETPID_PROTO
extern Pid_t getpid (void);
#endif

/*
 * Some BSDs and Cygwin default to POSIX math instead of IEEE.
 * This switches them over to IEEE.
 */
#if defined(LIBM_LIB_VERSION)
    _LIB_VERSION_TYPE _LIB_VERSION = _IEEE_;
#endif

static const STRLEN small_mu_len = sizeof(GREEK_SMALL_LETTER_MU_UTF8) - 1;
static const STRLEN capital_iota_len = sizeof(GREEK_CAPITAL_LETTER_IOTA_UTF8) - 1;

/* variations on pp_null */

PP(pp_stub)
{
    dSP;
    if (GIMME_V == G_SCALAR)
	XPUSHs(&PL_sv_undef);
    RETURN;
}

/* Pushy stuff. */

/* This is also called directly by pp_lvavref.  */
PP(pp_padav)
{
    dSP; dTARGET;
    U8 gimme;
    assert(SvTYPE(TARG) == SVt_PVAV);
    if (UNLIKELY( PL_op->op_private & OPpLVAL_INTRO ))
	if (LIKELY( !(PL_op->op_private & OPpPAD_STATE) ))
	    SAVECLEARSV(PAD_SVl(PL_op->op_targ));
    EXTEND(SP, 1);

    if (PL_op->op_flags & OPf_REF) {
	PUSHs(TARG);
	RETURN;
    }
    else if (PL_op->op_private & OPpMAYBE_LVSUB) {
        const I32 flags = is_lvalue_sub();
        if (flags && !(flags & OPpENTERSUB_INARGS)) {
	    if (GIMME_V == G_SCALAR)
                /* diag_listed_as: Can't return %s to lvalue scalar context */
                Perl_croak(aTHX_ "Can't return array to lvalue scalar context");
            PUSHs(TARG);
            RETURN;
       }
    }

    gimme = GIMME_V;
    if (gimme == G_ARRAY) {
        /* XXX see also S_pushav in pp_hot.c */
	const SSize_t maxarg = AvFILL(MUTABLE_AV(TARG)) + 1;
	EXTEND(SP, maxarg);
	if (SvMAGICAL(TARG)) {
	    SSize_t i;
	    for (i=0; i < maxarg; i++) {
		SV * const * const svp = av_fetch(MUTABLE_AV(TARG), i, FALSE);
		SP[i+1] = (svp) ? *svp : &PL_sv_undef;
	    }
	}
	else {
	    SSize_t i;
	    for (i=0; i < maxarg; i++) {
		SV * const sv = AvARRAY((const AV *)TARG)[i];
		SP[i+1] = sv ? sv : &PL_sv_undef;
	    }
	}
	SP += maxarg;
    }
    else if (gimme == G_SCALAR) {
	SV* const sv = sv_newmortal();
	const SSize_t maxarg = AvFILL(MUTABLE_AV(TARG)) + 1;
	sv_setiv(sv, maxarg);
	PUSHs(sv);
    }
    RETURN;
}

PP(pp_padhv)
{
    dSP; dTARGET;
    U8 gimme;

    assert(SvTYPE(TARG) == SVt_PVHV);
    XPUSHs(TARG);
    if (UNLIKELY( PL_op->op_private & OPpLVAL_INTRO ))
	if (LIKELY( !(PL_op->op_private & OPpPAD_STATE) ))
	    SAVECLEARSV(PAD_SVl(PL_op->op_targ));

    if (PL_op->op_flags & OPf_REF)
	RETURN;
    else if (PL_op->op_private & OPpMAYBE_LVSUB) {
        const I32 flags = is_lvalue_sub();
        if (flags && !(flags & OPpENTERSUB_INARGS)) {
            if (GIMME_V == G_SCALAR)
                /* diag_listed_as: Can't return %s to lvalue scalar context */
                Perl_croak(aTHX_ "Can't return hash to lvalue scalar context");
            RETURN;
        }
    }

    gimme = GIMME_V;
    if (gimme == G_ARRAY) {
	RETURNOP(Perl_do_kv(aTHX));
    }
    else if ((PL_op->op_private & OPpTRUEBOOL
	  || (  PL_op->op_private & OPpMAYBE_TRUEBOOL
	     && block_gimme() == G_VOID  ))
	  && (!SvRMAGICAL(TARG) || !mg_find(TARG, PERL_MAGIC_tied))
    )
	SETs(HvUSEDKEYS(TARG) ? &PL_sv_yes : sv_2mortal(newSViv(0)));
    else if (gimme == G_SCALAR) {
	SV* const sv = Perl_hv_scalar(aTHX_ MUTABLE_HV(TARG));
	SETs(sv);
    }
    RETURN;
}

PP(pp_padcv)
{
    dSP; dTARGET;
    assert(SvTYPE(TARG) == SVt_PVCV);
    XPUSHs(TARG);
    RETURN;
}

PP(pp_introcv)
{
    dTARGET;
    SvPADSTALE_off(TARG);
    return NORMAL;
}

PP(pp_clonecv)
{
    dTARGET;
    CV * const protocv = PadnamePROTOCV(
	PadlistNAMESARRAY(CvPADLIST(find_runcv(NULL)))[ARGTARG]
    );
    assert(SvTYPE(TARG) == SVt_PVCV);
    assert(protocv);
    if (CvISXSUB(protocv)) { /* constant */
	/* XXX Should we clone it here? */
	/* If this changes to use SAVECLEARSV, we can move the SAVECLEARSV
	   to introcv and remove the SvPADSTALE_off. */
	SAVEPADSVANDMORTALIZE(ARGTARG);
	PAD_SVl(ARGTARG) = SvREFCNT_inc_simple_NN(protocv);
    }
    else {
	if (CvROOT(protocv)) {
	    assert(CvCLONE(protocv));
	    assert(!CvCLONED(protocv));
	}
	cv_clone_into(protocv,(CV *)TARG);
	SAVECLEARSV(PAD_SVl(ARGTARG));
    }
    return NORMAL;
}

/* Translations. */

/* In some cases this function inspects PL_op.  If this function is called
   for new op types, more bool parameters may need to be added in place of
   the checks.

   When noinit is true, the absence of a gv will cause a retval of undef.
   This is unrelated to the cv-to-gv assignment case.
*/

static SV *
S_rv2gv(pTHX_ SV *sv, const bool vivify_sv, const bool strict,
              const bool noinit)
{
    if (!isGV(sv) || SvFAKE(sv)) SvGETMAGIC(sv);
    if (SvROK(sv)) {
	if (SvAMAGIC(sv)) {
	    sv = amagic_deref_call(sv, to_gv_amg);
	}
      wasref:
	sv = SvRV(sv);
	if (SvTYPE(sv) == SVt_PVIO) {
	    GV * const gv = MUTABLE_GV(sv_newmortal());
	    gv_init(gv, 0, "__ANONIO__", 10, 0);
	    GvIOp(gv) = MUTABLE_IO(sv);
	    SvREFCNT_inc_void_NN(sv);
	    sv = MUTABLE_SV(gv);
	}
	else if (!isGV_with_GP(sv)) {
	    Perl_die(aTHX_ "Not a GLOB reference");
        }
    }
    else {
	if (!isGV_with_GP(sv)) {
	    if (!SvOK(sv)) {
		/* If this is a 'my' scalar and flag is set then vivify
		 * NI-S 1999/05/07
		 */
		if (vivify_sv && sv != &PL_sv_undef) {
		    GV *gv;
		    if (SvREADONLY(sv))
			Perl_croak_no_modify();
		    if (cUNOP->op_targ) {
			SV * const namesv = PAD_SV(cUNOP->op_targ);
			HV *stash = CopSTASH(PL_curcop);
			if (SvTYPE(stash) != SVt_PVHV) stash = NULL;
			gv = MUTABLE_GV(newSV(0));
			gv_init_sv(gv, stash, namesv, 0);
		    }
		    else {
			const char * const name = CopSTASHPV(PL_curcop);
			gv = newGVgen_flags(name,
                                HvNAMEUTF8(CopSTASH(PL_curcop)) ? SVf_UTF8 : 0 );
			SvREFCNT_inc_simple_void_NN(gv);
		    }
		    prepare_SV_for_RV(sv);
		    SvRV_set(sv, MUTABLE_SV(gv));
		    SvROK_on(sv);
		    SvSETMAGIC(sv);
		    goto wasref;
		}
		if (PL_op->op_flags & OPf_REF || strict) {
		    Perl_die(aTHX_ PL_no_usym, "a symbol");
                }
		if (ckWARN(WARN_UNINITIALIZED))
		    report_uninit(sv);
		return &PL_sv_undef;
	    }
	    if (noinit)
	    {
		if (!(sv = MUTABLE_SV(gv_fetchsv_nomg(
		           sv, GV_ADDMG, SVt_PVGV
		   ))))
		    return &PL_sv_undef;
	    }
	    else {
		if (strict) {
                    Perl_die(aTHX_
                             PL_no_symref_sv,
                             sv,
                             (SvPOKp(sv) && SvCUR(sv)>32 ? "..." : ""),
                             "a symbol"
                             );
                }
		if ((PL_op->op_private & (OPpLVAL_INTRO|OPpDONT_INIT_GV))
		    == OPpDONT_INIT_GV) {
		    /* We are the target of a coderef assignment.  Return
		       the scalar unchanged, and let pp_sasssign deal with
		       things.  */
		    return sv;
		}
		sv = MUTABLE_SV(gv_fetchsv_nomg(sv, GV_ADD, SVt_PVGV));
	    }
	    /* FAKE globs in the symbol table cause weird bugs (#77810) */
	    SvFAKE_off(sv);
	}
    }
    if (SvFAKE(sv) && !(PL_op->op_private & OPpALLOW_FAKE)) {
	SV *newsv = sv_newmortal();
	sv_setsv_flags(newsv, sv, 0);
	SvFAKE_off(newsv);
	sv = newsv;
    }
    return sv;
}

PP(pp_rv2gv)
{
    dSP; dTOPss;

    sv = S_rv2gv(aTHX_
          sv, PL_op->op_private & OPpDEREF,
          PL_op->op_private & HINT_STRICT_REFS,
          ((PL_op->op_flags & OPf_SPECIAL) && !(PL_op->op_flags & OPf_MOD))
             || PL_op->op_type == OP_READLINE
         );
    if (PL_op->op_private & OPpLVAL_INTRO)
	save_gp(MUTABLE_GV(sv), !(PL_op->op_flags & OPf_SPECIAL));
    SETs(sv);
    RETURN;
}

/* Helper function for pp_rv2sv and pp_rv2av  */
GV *
Perl_softref2xv(pTHX_ SV *const sv, const char *const what,
		const svtype type, SV ***spp)
{
    GV *gv;

    PERL_ARGS_ASSERT_SOFTREF2XV;

    if (PL_op->op_private & HINT_STRICT_REFS) {
	if (SvOK(sv))
	    Perl_die(aTHX_ PL_no_symref_sv, sv,
		     (SvPOKp(sv) && SvCUR(sv)>32 ? "..." : ""), what);
	else
	    Perl_die(aTHX_ PL_no_usym, what);
    }
    if (!SvOK(sv)) {
	if (
	  PL_op->op_flags & OPf_REF
	)
	    Perl_die(aTHX_ PL_no_usym, what);
	if (ckWARN(WARN_UNINITIALIZED))
	    report_uninit(sv);
	if (type != SVt_PV && GIMME_V == G_ARRAY) {
	    (*spp)--;
	    return NULL;
	}
	**spp = &PL_sv_undef;
	return NULL;
    }
    if ((PL_op->op_flags & OPf_SPECIAL) &&
	!(PL_op->op_flags & OPf_MOD))
	{
	    if (!(gv = gv_fetchsv_nomg(sv, GV_ADDMG, type)))
		{
		    **spp = &PL_sv_undef;
		    return NULL;
		}
	}
    else {
	gv = gv_fetchsv_nomg(sv, GV_ADD, type);
    }
    return gv;
}

PP(pp_rv2sv)
{
    dSP; dTOPss;
    GV *gv = NULL;

    SvGETMAGIC(sv);
    if (SvROK(sv)) {
	if (SvAMAGIC(sv)) {
	    sv = amagic_deref_call(sv, to_sv_amg);
	}

	sv = SvRV(sv);
	if (SvTYPE(sv) >= SVt_PVAV)
	    DIE(aTHX_ "Not a SCALAR reference");
    }
    else {
	gv = MUTABLE_GV(sv);

	if (!isGV_with_GP(gv)) {
	    gv = Perl_softref2xv(aTHX_ sv, "a SCALAR", SVt_PV, &sp);
	    if (!gv)
		RETURN;
	}
	sv = GvSVn(gv);
    }
    if (PL_op->op_flags & OPf_MOD) {
	if (PL_op->op_private & OPpLVAL_INTRO) {
	    if (cUNOP->op_first->op_type == OP_NULL)
		sv = save_scalar(MUTABLE_GV(TOPs));
	    else if (gv)
		sv = save_scalar(gv);
	    else
		Perl_croak(aTHX_ "%s", PL_no_localize_ref);
	}
	else if (PL_op->op_private & OPpDEREF)
	    sv = vivify_ref(sv, PL_op->op_private & OPpDEREF);
    }
    SETs(sv);
    RETURN;
}

PP(pp_av2arylen)
{
    dSP;
    AV * const av = MUTABLE_AV(TOPs);
    const I32 lvalue = PL_op->op_flags & OPf_MOD || LVRET;
    if (lvalue) {
	SV ** const svp = Perl_av_arylen_p(aTHX_ MUTABLE_AV(av));
	if (!*svp) {
	    *svp = newSV_type(SVt_PVMG);
	    sv_magic(*svp, MUTABLE_SV(av), PERL_MAGIC_arylen, NULL, 0);
	}
	SETs(*svp);
    } else {
	SETs(sv_2mortal(newSViv(AvFILL(MUTABLE_AV(av)))));
    }
    RETURN;
}

PP(pp_pos)
{
    dSP; dTOPss;

    if (PL_op->op_flags & OPf_MOD || LVRET) {
	SV * const ret = sv_2mortal(newSV_type(SVt_PVLV));/* Not TARG RT#67838 */
	sv_magic(ret, NULL, PERL_MAGIC_pos, NULL, 0);
	LvTYPE(ret) = '.';
	LvTARG(ret) = SvREFCNT_inc_simple(sv);
	SETs(ret);    /* no SvSETMAGIC */
    }
    else {
	    const MAGIC * const mg = mg_find_mglob(sv);
	    if (mg && mg->mg_len != -1) {
		dTARGET;
		STRLEN i = mg->mg_len;
		if (mg->mg_flags & MGf_BYTES && DO_UTF8(sv))
		    i = sv_pos_b2u_flags(sv, i, SV_GMAGIC|SV_CONST_RETURN);
		SETu(i);
		return NORMAL;
	    }
	    SETs(&PL_sv_undef);
    }
    return NORMAL;
}

PP(pp_rv2cv)
{
    dSP;
    GV *gv;
    HV *stash_unused;
    const I32 flags = (PL_op->op_flags & OPf_SPECIAL)
	? GV_ADDMG
	: ((PL_op->op_private & (OPpLVAL_INTRO|OPpMAY_RETURN_CONSTANT))
                                                    == OPpMAY_RETURN_CONSTANT)
	    ? GV_ADD|GV_NOEXPAND
	    : GV_ADD;
    /* We usually try to add a non-existent subroutine in case of AUTOLOAD. */
    /* (But not in defined().) */

    CV *cv = sv_2cv(TOPs, &stash_unused, &gv, flags);
    if (cv) NOOP;
    else if ((flags == (GV_ADD|GV_NOEXPAND)) && gv && SvROK(gv)) {
	cv = SvTYPE(SvRV(gv)) == SVt_PVCV
	    ? MUTABLE_CV(SvRV(gv))
	    : MUTABLE_CV(gv);
    }    
    else
	cv = MUTABLE_CV(&PL_sv_undef);
    SETs(MUTABLE_SV(cv));
    return NORMAL;
}

PP(pp_prototype)
{
    dSP;
    CV *cv;
    HV *stash;
    GV *gv;
    SV *ret = &PL_sv_undef;

    if (SvGMAGICAL(TOPs)) SETs(sv_mortalcopy(TOPs));
    if (SvPOK(TOPs) && SvCUR(TOPs) >= 7) {
	const char * s = SvPVX_const(TOPs);
	if (strnEQ(s, "CORE::", 6)) {
	    const int code = keyword(s + 6, SvCUR(TOPs) - 6, 1);
	    if (!code)
		DIE(aTHX_ "Can't find an opnumber for \"%"UTF8f"\"",
		   UTF8fARG(SvFLAGS(TOPs) & SVf_UTF8, SvCUR(TOPs)-6, s+6));
	    {
		SV * const sv = core_prototype(NULL, s + 6, code, NULL);
		if (sv) ret = sv;
	    }
	    goto set;
	}
    }
    cv = sv_2cv(TOPs, &stash, &gv, 0);
    if (cv && SvPOK(cv))
	ret = newSVpvn_flags(
	    CvPROTO(cv), CvPROTOLEN(cv), SVs_TEMP | SvUTF8(cv)
	);
  set:
    SETs(ret);
    RETURN;
}

PP(pp_anoncode)
{
    dSP;
    CV *cv = MUTABLE_CV(PAD_SV(PL_op->op_targ));
    if (CvCLONE(cv))
	cv = MUTABLE_CV(sv_2mortal(MUTABLE_SV(cv_clone(cv))));
    EXTEND(SP,1);
    PUSHs(MUTABLE_SV(cv));
    RETURN;
}

PP(pp_srefgen)
{
    dSP;
    *SP = refto(*SP);
    return NORMAL;
}

PP(pp_refgen)
{
    dSP; dMARK;
    if (GIMME_V != G_ARRAY) {
	if (++MARK <= SP)
	    *MARK = *SP;
	else
	{
	    MEXTEND(SP, 1);
	    *MARK = &PL_sv_undef;
	}
	*MARK = refto(*MARK);
	SP = MARK;
	RETURN;
    }
    EXTEND_MORTAL(SP - MARK);
    while (++MARK <= SP)
	*MARK = refto(*MARK);
    RETURN;
}

STATIC SV*
S_refto(pTHX_ SV *sv)
{
    SV* rv;

    PERL_ARGS_ASSERT_REFTO;

    if (SvTYPE(sv) == SVt_PVLV && LvTYPE(sv) == 'y') {
	if (LvTARGLEN(sv))
	    vivify_defelem(sv);
	if (!(sv = LvTARG(sv)))
	    sv = &PL_sv_undef;
	else
	    SvREFCNT_inc_void_NN(sv);
    }
    else if (SvTYPE(sv) == SVt_PVAV) {
	if (!AvREAL((const AV *)sv) && AvREIFY((const AV *)sv))
	    av_reify(MUTABLE_AV(sv));
	SvTEMP_off(sv);
	SvREFCNT_inc_void_NN(sv);
    }
    else if (SvPADTMP(sv)) {
        sv = newSVsv(sv);
    }
    else {
	SvTEMP_off(sv);
	SvREFCNT_inc_void_NN(sv);
    }
    rv = sv_newmortal();
    sv_upgrade(rv, SVt_IV);
    SvRV_set(rv, sv);
    SvROK_on(rv);
    return rv;
}

PP(pp_ref)
{
    dSP;
    SV * const sv = TOPs;

    SvGETMAGIC(sv);
    if (!SvROK(sv))
	SETs(&PL_sv_no);
    else {
	dTARGET;
	SETs(TARG);
	/* use the return value that is in a register, its the same as TARG */
	TARG = sv_ref(TARG,SvRV(sv),TRUE);
	SvSETMAGIC(TARG);
    }

    return NORMAL;
}

PP(pp_bless)
{
    dSP;
    HV *stash;

    if (MAXARG == 1)
    {
      curstash:
	stash = CopSTASH(PL_curcop);
	if (SvTYPE(stash) != SVt_PVHV)
	    Perl_croak(aTHX_ "Attempt to bless into a freed package");
    }
    else {
	SV * const ssv = POPs;
	STRLEN len;
	const char *ptr;

	if (!ssv) goto curstash;
	SvGETMAGIC(ssv);
	if (SvROK(ssv)) {
	  if (!SvAMAGIC(ssv)) {
	   frog:
	    Perl_croak(aTHX_ "Attempt to bless into a reference");
	  }
	  /* SvAMAGIC is on here, but it only means potentially overloaded,
	     so after stringification: */
	  ptr = SvPV_nomg_const(ssv,len);
	  /* We need to check the flag again: */
	  if (!SvAMAGIC(ssv)) goto frog;
	}
	else ptr = SvPV_nomg_const(ssv,len);
	if (len == 0)
	    Perl_ck_warner(aTHX_ packWARN(WARN_MISC),
			   "Explicit blessing to '' (assuming package main)");
	stash = gv_stashpvn(ptr, len, GV_ADD|SvUTF8(ssv));
    }

    (void)sv_bless(TOPs, stash);
    RETURN;
}

PP(pp_gelem)
{
    dSP;

    SV *sv = POPs;
    STRLEN len;
    const char * const elem = SvPV_const(sv, len);
    GV * const gv = MUTABLE_GV(TOPs);
    SV * tmpRef = NULL;

    sv = NULL;
    if (elem) {
	/* elem will always be NUL terminated.  */
	const char * const second_letter = elem + 1;
	switch (*elem) {
	case 'A':
	    if (len == 5 && strEQ(second_letter, "RRAY"))
	    {
		tmpRef = MUTABLE_SV(GvAV(gv));
		if (tmpRef && !AvREAL((const AV *)tmpRef)
		 && AvREIFY((const AV *)tmpRef))
		    av_reify(MUTABLE_AV(tmpRef));
	    }
	    break;
	case 'C':
	    if (len == 4 && strEQ(second_letter, "ODE"))
		tmpRef = MUTABLE_SV(GvCVu(gv));
	    break;
	case 'F':
	    if (len == 10 && strEQ(second_letter, "ILEHANDLE")) {
		tmpRef = MUTABLE_SV(GvIOp(gv));
	    }
	    else
		if (len == 6 && strEQ(second_letter, "ORMAT"))
		    tmpRef = MUTABLE_SV(GvFORM(gv));
	    break;
	case 'G':
	    if (len == 4 && strEQ(second_letter, "LOB"))
		tmpRef = MUTABLE_SV(gv);
	    break;
	case 'H':
	    if (len == 4 && strEQ(second_letter, "ASH"))
		tmpRef = MUTABLE_SV(GvHV(gv));
	    break;
	case 'I':
	    if (*second_letter == 'O' && !elem[2] && len == 2)
		tmpRef = MUTABLE_SV(GvIOp(gv));
	    break;
	case 'N':
	    if (len == 4 && strEQ(second_letter, "AME"))
		sv = newSVhek(GvNAME_HEK(gv));
	    break;
	case 'P':
	    if (len == 7 && strEQ(second_letter, "ACKAGE")) {
		const HV * const stash = GvSTASH(gv);
		const HEK * const hek = stash ? HvNAME_HEK(stash) : NULL;
		sv = hek ? newSVhek(hek) : newSVpvs("__ANON__");
	    }
	    break;
	case 'S':
	    if (len == 6 && strEQ(second_letter, "CALAR"))
		tmpRef = GvSVn(gv);
	    break;
	}
    }
    if (tmpRef)
	sv = newRV(tmpRef);
    if (sv)
	sv_2mortal(sv);
    else
	sv = &PL_sv_undef;
    SETs(sv);
    RETURN;
}

/* Pattern matching */

PP(pp_study)
{
    dSP; dTOPss;
    STRLEN len;

    (void)SvPV(sv, len);
    if (len == 0 || len > I32_MAX || !SvPOK(sv) || SvUTF8(sv) || SvVALID(sv)) {
	/* Historically, study was skipped in these cases. */
	SETs(&PL_sv_no);
	return NORMAL;
    }

    /* Make study a no-op. It's no longer useful and its existence
       complicates matters elsewhere. */
    SETs(&PL_sv_yes);
    return NORMAL;
}


/* also used for: pp_transr() */

PP(pp_trans)
{
    dSP; 
    SV *sv;

    if (PL_op->op_flags & OPf_STACKED)
	sv = POPs;
    else {
	EXTEND(SP,1);
	if (ARGTARG)
	    sv = PAD_SV(ARGTARG);
	else {
	    sv = DEFSV;
	}
    }
    if(PL_op->op_type == OP_TRANSR) {
	STRLEN len;
	const char * const pv = SvPV(sv,len);
	SV * const newsv = newSVpvn_flags(pv, len, SVs_TEMP|SvUTF8(sv));
	do_trans(newsv);
	PUSHs(newsv);
    }
    else {
	I32 i = do_trans(sv);
	mPUSHi(i);
    }
    RETURN;
}

/* Lvalue operators. */

static size_t
S_do_chomp(pTHX_ SV *retval, SV *sv, bool chomping)
{
    STRLEN len;
    char *s;
    size_t count = 0;

    PERL_ARGS_ASSERT_DO_CHOMP;

    if (chomping && (RsSNARF(PL_rs) || RsRECORD(PL_rs)))
	return 0;
    if (SvTYPE(sv) == SVt_PVAV) {
	I32 i;
	AV *const av = MUTABLE_AV(sv);
	const I32 max = AvFILL(av);

	for (i = 0; i <= max; i++) {
	    sv = MUTABLE_SV(av_fetch(av, i, FALSE));
	    if (sv && ((sv = *(SV**)sv), sv != &PL_sv_undef))
		count += do_chomp(retval, sv, chomping);
	}
        return count;
    }
    else if (SvTYPE(sv) == SVt_PVHV) {
	HV* const hv = MUTABLE_HV(sv);
	HE* entry;
        (void)hv_iterinit(hv);
        while ((entry = hv_iternext(hv)))
            count += do_chomp(retval, hv_iterval(hv,entry), chomping);
	return count;
    }
    else if (SvREADONLY(sv)) {
            Perl_croak_no_modify();
    }

    if (IN_ENCODING) {
	if (!SvUTF8(sv)) {
	    /* XXX, here sv is utf8-ized as a side-effect!
	       If encoding.pm is used properly, almost string-generating
	       operations, including literal strings, chr(), input data, etc.
	       should have been utf8-ized already, right?
	    */
	    sv_recode_to_utf8(sv, _get_encoding());
	}
    }

    s = SvPV(sv, len);
    if (chomping) {
	if (s && len) {
	    char *temp_buffer = NULL;
	    SV *svrecode = NULL;
	    s += --len;
	    if (RsPARA(PL_rs)) {
		if (*s != '\n')
		    goto nope_free_nothing;
		++count;
		while (len && s[-1] == '\n') {
		    --len;
		    --s;
		    ++count;
		}
	    }
	    else {
		STRLEN rslen, rs_charlen;
		const char *rsptr = SvPV_const(PL_rs, rslen);

		rs_charlen = SvUTF8(PL_rs)
		    ? sv_len_utf8(PL_rs)
		    : rslen;

		if (SvUTF8(PL_rs) != SvUTF8(sv)) {
		    /* Assumption is that rs is shorter than the scalar.  */
		    if (SvUTF8(PL_rs)) {
			/* RS is utf8, scalar is 8 bit.  */
			bool is_utf8 = TRUE;
			temp_buffer = (char*)bytes_from_utf8((U8*)rsptr,
							     &rslen, &is_utf8);
			if (is_utf8) {
			    /* Cannot downgrade, therefore cannot possibly match.
			       At this point, temp_buffer is not alloced, and
			       is the buffer inside PL_rs, so dont free it.
			     */
			    assert (temp_buffer == rsptr);
			    goto nope_free_sv;
			}
			rsptr = temp_buffer;
		    }
		    else if (IN_ENCODING) {
			/* RS is 8 bit, encoding.pm is used.
			 * Do not recode PL_rs as a side-effect. */
			svrecode = newSVpvn(rsptr, rslen);
			sv_recode_to_utf8(svrecode, _get_encoding());
			rsptr = SvPV_const(svrecode, rslen);
			rs_charlen = sv_len_utf8(svrecode);
		    }
		    else {
			/* RS is 8 bit, scalar is utf8.  */
			temp_buffer = (char*)bytes_to_utf8((U8*)rsptr, &rslen);
			rsptr = temp_buffer;
		    }
		}
		if (rslen == 1) {
		    if (*s != *rsptr)
			goto nope_free_all;
		    ++count;
		}
		else {
		    if (len < rslen - 1)
			goto nope_free_all;
		    len -= rslen - 1;
		    s -= rslen - 1;
		    if (memNE(s, rsptr, rslen))
			goto nope_free_all;
		    count += rs_charlen;
		}
	    }
	    SvPV_force_nomg_nolen(sv);
	    SvCUR_set(sv, len);
	    *SvEND(sv) = '\0';
	    SvNIOK_off(sv);
	    SvSETMAGIC(sv);

	    nope_free_all:
	    Safefree(temp_buffer);
	    nope_free_sv:
	    SvREFCNT_dec(svrecode);
	    nope_free_nothing: ;
	}
    } else {
	if (len && (!SvPOK(sv) || SvIsCOW(sv)))
	    s = SvPV_force_nomg(sv, len);
	if (DO_UTF8(sv)) {
	    if (s && len) {
		char * const send = s + len;
		char * const start = s;
		s = send - 1;
		while (s > start && UTF8_IS_CONTINUATION(*s))
		    s--;
		if (is_utf8_string((U8*)s, send - s)) {
		    sv_setpvn(retval, s, send - s);
		    *s = '\0';
		    SvCUR_set(sv, s - start);
		    SvNIOK_off(sv);
		    SvUTF8_on(retval);
		}
	    }
	    else
		sv_setpvs(retval, "");
	}
	else if (s && len) {
	    s += --len;
	    sv_setpvn(retval, s, 1);
	    *s = '\0';
	    SvCUR_set(sv, len);
	    SvUTF8_off(sv);
	    SvNIOK_off(sv);
	}
	else
	    sv_setpvs(retval, "");
	SvSETMAGIC(sv);
    }
    return count;
}


/* also used for: pp_schomp() */

PP(pp_schop)
{
    dSP; dTARGET;
    const bool chomping = PL_op->op_type == OP_SCHOMP;

    const size_t count = do_chomp(TARG, TOPs, chomping);
    if (chomping)
	sv_setiv(TARG, count);
    SETTARG;
    return NORMAL;
}


/* also used for: pp_chomp() */

PP(pp_chop)
{
    dSP; dMARK; dTARGET; dORIGMARK;
    const bool chomping = PL_op->op_type == OP_CHOMP;
    size_t count = 0;

    while (MARK < SP)
	count += do_chomp(TARG, *++MARK, chomping);
    if (chomping)
	sv_setiv(TARG, count);
    SP = ORIGMARK;
    XPUSHTARG;
    RETURN;
}

PP(pp_undef)
{
    dSP;
    SV *sv;

    if (!PL_op->op_private) {
	EXTEND(SP, 1);
	RETPUSHUNDEF;
    }

    sv = TOPs;
    if (!sv)
    {
	SETs(&PL_sv_undef);
	return NORMAL;
    }

    if (SvTHINKFIRST(sv))
	sv_force_normal_flags(sv, SV_COW_DROP_PV|SV_IMMEDIATE_UNREF);

    switch (SvTYPE(sv)) {
    case SVt_NULL:
	break;
    case SVt_PVAV:
	av_undef(MUTABLE_AV(sv));
	break;
    case SVt_PVHV:
	hv_undef(MUTABLE_HV(sv));
	break;
    case SVt_PVCV:
	if (cv_const_sv((const CV *)sv))
	    Perl_ck_warner(aTHX_ packWARN(WARN_MISC),
                          "Constant subroutine %"SVf" undefined",
			   SVfARG(CvANON((const CV *)sv)
                             ? newSVpvs_flags("(anonymous)", SVs_TEMP)
                             : sv_2mortal(newSVhek(
                                CvNAMED(sv)
                                 ? CvNAME_HEK((CV *)sv)
                                 : GvENAME_HEK(CvGV((const CV *)sv))
                               ))
                           ));
	/* FALLTHROUGH */
    case SVt_PVFM:
	    /* let user-undef'd sub keep its identity */
	cv_undef_flags(MUTABLE_CV(sv), CV_UNDEF_KEEP_NAME);
	break;
    case SVt_PVGV:
	assert(isGV_with_GP(sv));
	assert(!SvFAKE(sv));
	{
	    GP *gp;
            HV *stash;

            /* undef *Pkg::meth_name ... */
            bool method_changed
             =   GvCVu((const GV *)sv) && (stash = GvSTASH((const GV *)sv))
	      && HvENAME_get(stash);
            /* undef *Foo:: */
            if((stash = GvHV((const GV *)sv))) {
                if(HvENAME_get(stash))
                    SvREFCNT_inc_simple_void_NN(sv_2mortal((SV *)stash));
                else stash = NULL;
            }

	    SvREFCNT_inc_simple_void_NN(sv_2mortal(sv));
	    gp_free(MUTABLE_GV(sv));
	    Newxz(gp, 1, GP);
	    GvGP_set(sv, gp_ref(gp));
#ifndef PERL_DONT_CREATE_GVSV
	    GvSV(sv) = newSV(0);
#endif
	    GvLINE(sv) = CopLINE(PL_curcop);
	    GvEGV(sv) = MUTABLE_GV(sv);
	    GvMULTI_on(sv);

            if(stash)
                mro_package_moved(NULL, stash, (const GV *)sv, 0);
            stash = NULL;
            /* undef *Foo::ISA */
            if( strEQ(GvNAME((const GV *)sv), "ISA")
             && (stash = GvSTASH((const GV *)sv))
             && (method_changed || HvENAME(stash)) )
                mro_isa_changed_in(stash);
            else if(method_changed)
                mro_method_changed_in(
                 GvSTASH((const GV *)sv)
                );

	    break;
	}
    default:
	if (SvTYPE(sv) >= SVt_PV && SvPVX_const(sv) && SvLEN(sv)) {
	    SvPV_free(sv);
	    SvPV_set(sv, NULL);
	    SvLEN_set(sv, 0);
	}
	SvOK_off(sv);
	SvSETMAGIC(sv);
    }

    SETs(&PL_sv_undef);
    return NORMAL;
}


/* common "slow" code for pp_postinc and pp_postdec */

static OP *
S_postincdec_common(pTHX_ SV *sv, SV *targ)
{
    dSP;
    const bool inc =
	PL_op->op_type == OP_POSTINC || PL_op->op_type == OP_I_POSTINC;

    if (SvROK(sv))
	TARG = sv_newmortal();
    sv_setsv(TARG, sv);
    if (inc)
	sv_inc_nomg(sv);
    else
        sv_dec_nomg(sv);
    SvSETMAGIC(sv);
    /* special case for undef: see thread at 2003-03/msg00536.html in archive */
    if (inc && !SvOK(TARG))
	sv_setiv(TARG, 0);
    SETTARG;
    return NORMAL;
}


/* also used for: pp_i_postinc() */

PP(pp_postinc)
{
    dSP; dTARGET;
    SV *sv = TOPs;

    /* special-case sv being a simple integer */
    if (LIKELY(((sv->sv_flags &
                        (SVf_THINKFIRST|SVs_GMG|SVf_IVisUV|
                         SVf_IOK|SVf_NOK|SVf_POK|SVp_NOK|SVp_POK|SVf_ROK))
                == SVf_IOK))
        && SvIVX(sv) != IV_MAX)
    {
        IV iv = SvIVX(sv);
	SvIV_set(sv,  iv + 1);
        TARGi(iv, 0); /* arg not GMG, so can't be tainted */
        SETs(TARG);
        return NORMAL;
    }

    return S_postincdec_common(aTHX_ sv, TARG);
}


/* also used for: pp_i_postdec() */

PP(pp_postdec)
{
    dSP; dTARGET;
    SV *sv = TOPs;

    /* special-case sv being a simple integer */
    if (LIKELY(((sv->sv_flags &
                        (SVf_THINKFIRST|SVs_GMG|SVf_IVisUV|
                         SVf_IOK|SVf_NOK|SVf_POK|SVp_NOK|SVp_POK|SVf_ROK))
                == SVf_IOK))
        && SvIVX(sv) != IV_MIN)
    {
        IV iv = SvIVX(sv);
	SvIV_set(sv,  iv - 1);
        TARGi(iv, 0); /* arg not GMG, so can't be tainted */
        SETs(TARG);
        return NORMAL;
    }

    return S_postincdec_common(aTHX_ sv, TARG);
}


/* Ordinary operators. */

PP(pp_pow)
{
    dSP; dATARGET; SV *svl, *svr;
#ifdef PERL_PRESERVE_IVUV
    bool is_int = 0;
#endif
    tryAMAGICbin_MG(pow_amg, AMGf_assign|AMGf_numeric);
    svr = TOPs;
    svl = TOPm1s;
#ifdef PERL_PRESERVE_IVUV
    /* For integer to integer power, we do the calculation by hand wherever
       we're sure it is safe; otherwise we call pow() and try to convert to
       integer afterwards. */
    if (SvIV_please_nomg(svr) && SvIV_please_nomg(svl)) {
		UV power;
		bool baseuok;
		UV baseuv;

		if (SvUOK(svr)) {
		    power = SvUVX(svr);
		} else {
		    const IV iv = SvIVX(svr);
		    if (iv >= 0) {
			power = iv;
		    } else {
			goto float_it; /* Can't do negative powers this way.  */
		    }
		}

		baseuok = SvUOK(svl);
		if (baseuok) {
		    baseuv = SvUVX(svl);
		} else {
		    const IV iv = SvIVX(svl);
		    if (iv >= 0) {
			baseuv = iv;
			baseuok = TRUE; /* effectively it's a UV now */
		    } else {
			baseuv = -iv; /* abs, baseuok == false records sign */
		    }
		}
                /* now we have integer ** positive integer. */
                is_int = 1;

                /* foo & (foo - 1) is zero only for a power of 2.  */
                if (!(baseuv & (baseuv - 1))) {
                    /* We are raising power-of-2 to a positive integer.
                       The logic here will work for any base (even non-integer
                       bases) but it can be less accurate than
                       pow (base,power) or exp (power * log (base)) when the
                       intermediate values start to spill out of the mantissa.
                       With powers of 2 we know this can't happen.
                       And powers of 2 are the favourite thing for perl
                       programmers to notice ** not doing what they mean. */
                    NV result = 1.0;
                    NV base = baseuok ? baseuv : -(NV)baseuv;

		    if (power & 1) {
			result *= base;
		    }
		    while (power >>= 1) {
			base *= base;
			if (power & 1) {
			    result *= base;
			}
		    }
                    SP--;
                    SETn( result );
                    SvIV_please_nomg(svr);
                    RETURN;
		} else {
		    unsigned int highbit = 8 * sizeof(UV);
		    unsigned int diff = 8 * sizeof(UV);
		    while (diff >>= 1) {
			highbit -= diff;
			if (baseuv >> highbit) {
			    highbit += diff;
			}
		    }
		    /* we now have baseuv < 2 ** highbit */
		    if (power * highbit <= 8 * sizeof(UV)) {
			/* result will definitely fit in UV, so use UV math
			   on same algorithm as above */
			UV result = 1;
			UV base = baseuv;
			const bool odd_power = cBOOL(power & 1);
			if (odd_power) {
			    result *= base;
			}
			while (power >>= 1) {
			    base *= base;
			    if (power & 1) {
				result *= base;
			    }
			}
			SP--;
			if (baseuok || !odd_power)
			    /* answer is positive */
			    SETu( result );
			else if (result <= (UV)IV_MAX)
			    /* answer negative, fits in IV */
			    SETi( -(IV)result );
			else if (result == (UV)IV_MIN) 
			    /* 2's complement assumption: special case IV_MIN */
			    SETi( IV_MIN );
			else
			    /* answer negative, doesn't fit */
			    SETn( -(NV)result );
			RETURN;
		    } 
		}
    }
  float_it:
#endif    
    {
	NV right = SvNV_nomg(svr);
	NV left  = SvNV_nomg(svl);
	(void)POPs;

#if defined(USE_LONG_DOUBLE) && defined(HAS_AIX_POWL_NEG_BASE_BUG)
    /*
    We are building perl with long double support and are on an AIX OS
    afflicted with a powl() function that wrongly returns NaNQ for any
    negative base.  This was reported to IBM as PMR #23047-379 on
    03/06/2006.  The problem exists in at least the following versions
    of AIX and the libm fileset, and no doubt others as well:

	AIX 4.3.3-ML10      bos.adt.libm 4.3.3.50
	AIX 5.1.0-ML04      bos.adt.libm 5.1.0.29
	AIX 5.2.0           bos.adt.libm 5.2.0.85

    So, until IBM fixes powl(), we provide the following workaround to
    handle the problem ourselves.  Our logic is as follows: for
    negative bases (left), we use fmod(right, 2) to check if the
    exponent is an odd or even integer:

	- if odd,  powl(left, right) == -powl(-left, right)
	- if even, powl(left, right) ==  powl(-left, right)

    If the exponent is not an integer, the result is rightly NaNQ, so
    we just return that (as NV_NAN).
    */

	if (left < 0.0) {
	    NV mod2 = Perl_fmod( right, 2.0 );
	    if (mod2 == 1.0 || mod2 == -1.0) {	/* odd integer */
		SETn( -Perl_pow( -left, right) );
	    } else if (mod2 == 0.0) {		/* even integer */
		SETn( Perl_pow( -left, right) );
	    } else {				/* fractional power */
		SETn( NV_NAN );
	    }
	} else {
	    SETn( Perl_pow( left, right) );
	}
#else
	SETn( Perl_pow( left, right) );
#endif  /* HAS_AIX_POWL_NEG_BASE_BUG */

#ifdef PERL_PRESERVE_IVUV
	if (is_int)
	    SvIV_please_nomg(svr);
#endif
	RETURN;
    }
}

PP(pp_multiply)
{
    dSP; dATARGET; SV *svl, *svr;
    tryAMAGICbin_MG(mult_amg, AMGf_assign|AMGf_numeric);
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
            topl = ((UV)il) >> (UVSIZE * 4 - 1);
            topr = ((UV)ir) >> (UVSIZE * 4 - 1);

            /* if both are in a range that can't under/overflow, do a
             * simple integer multiply: if the top halves(*) of both numbers
             * are 00...00  or 11...11, then it's safe.
             * (*) for 32-bits, the "top half" is the top 17 bits,
             *     for 64-bits, its 33 bits */
            if (!(
                      ((topl+1) | (topr+1))
                    & ( (((UV)1) << (UVSIZE * 4 + 1)) - 2) /* 11..110 */
            )) {
                SP--;
                TARGi(il * ir, 0); /* args not GMG, so can't be tainted */
                SETs(TARG);
                RETURN;
            }
            goto generic;
        }
        else if (flags & SVf_NOK) {
            /* both args are NVs */
            NV nl = SvNVX(svl);
            NV nr = SvNVX(svr);
            NV result;

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
            result = nl * nr;
#  if defined(__sgi) && defined(USE_LONG_DOUBLE) && LONG_DOUBLEKIND == LONG_DOUBLE_IS_DOUBLEDOUBLE_128_BIT_BE_BE && NVSIZE == 16
            if (Perl_isinf(result)) {
                Zero((U8*)&result + 8, 8, U8);
            }
#  endif
            TARGn(result, 0); /* args not GMG, so can't be tainted */
            SETs(TARG);
            RETURN;
        }
    }

  generic:

    if (SvIV_please_nomg(svr)) {
	/* Unless the left argument is integer in range we are going to have to
	   use NV maths. Hence only attempt to coerce the right argument if
	   we know the left is integer.  */
	/* Left operand is defined, so is it IV? */
	if (SvIV_please_nomg(svl)) {
	    bool auvok = SvUOK(svl);
	    bool buvok = SvUOK(svr);
	    const UV topmask = (~ (UV)0) << (4 * sizeof (UV));
	    const UV botmask = ~((~ (UV)0) << (4 * sizeof (UV)));
	    UV alow;
	    UV ahigh;
	    UV blow;
	    UV bhigh;

	    if (auvok) {
		alow = SvUVX(svl);
	    } else {
		const IV aiv = SvIVX(svl);
		if (aiv >= 0) {
		    alow = aiv;
		    auvok = TRUE; /* effectively it's a UV now */
		} else {
                    /* abs, auvok == false records sign */
		    alow = (aiv == IV_MIN) ? (UV)aiv : (UV)(-aiv);
		}
	    }
	    if (buvok) {
		blow = SvUVX(svr);
	    } else {
		const IV biv = SvIVX(svr);
		if (biv >= 0) {
		    blow = biv;
		    buvok = TRUE; /* effectively it's a UV now */
		} else {
                    /* abs, buvok == false records sign */
		    blow = (biv == IV_MIN) ? (UV)biv : (UV)(-biv);
		}
	    }

	    /* If this does sign extension on unsigned it's time for plan B  */
	    ahigh = alow >> (4 * sizeof (UV));
	    alow &= botmask;
	    bhigh = blow >> (4 * sizeof (UV));
	    blow &= botmask;
	    if (ahigh && bhigh) {
		NOOP;
		/* eg 32 bit is at least 0x10000 * 0x10000 == 0x100000000
		   which is overflow. Drop to NVs below.  */
	    } else if (!ahigh && !bhigh) {
		/* eg 32 bit is at most 0xFFFF * 0xFFFF == 0xFFFE0001
		   so the unsigned multiply cannot overflow.  */
		const UV product = alow * blow;
		if (auvok == buvok) {
		    /* -ve * -ve or +ve * +ve gives a +ve result.  */
		    SP--;
		    SETu( product );
		    RETURN;
		} else if (product <= (UV)IV_MIN) {
		    /* 2s complement assumption that (UV)-IV_MIN is correct.  */
		    /* -ve result, which could overflow an IV  */
		    SP--;
                    /* can't negate IV_MIN, but there are aren't two
                     * integers such that !ahigh && !bhigh, where the
                     * product equals 0x800....000 */
                    assert(product != (UV)IV_MIN);
		    SETi( -(IV)product );
		    RETURN;
		} /* else drop to NVs below. */
	    } else {
		/* One operand is large, 1 small */
		UV product_middle;
		if (bhigh) {
		    /* swap the operands */
		    ahigh = bhigh;
		    bhigh = blow; /* bhigh now the temp var for the swap */
		    blow = alow;
		    alow = bhigh;
		}
		/* now, ((ahigh * blow) << half_UV_len) + (alow * blow)
		   multiplies can't overflow. shift can, add can, -ve can.  */
		product_middle = ahigh * blow;
		if (!(product_middle & topmask)) {
		    /* OK, (ahigh * blow) won't lose bits when we shift it.  */
		    UV product_low;
		    product_middle <<= (4 * sizeof (UV));
		    product_low = alow * blow;

		    /* as for pp_add, UV + something mustn't get smaller.
		       IIRC ANSI mandates this wrapping *behaviour* for
		       unsigned whatever the actual representation*/
		    product_low += product_middle;
		    if (product_low >= product_middle) {
			/* didn't overflow */
			if (auvok == buvok) {
			    /* -ve * -ve or +ve * +ve gives a +ve result.  */
			    SP--;
			    SETu( product_low );
			    RETURN;
			} else if (product_low <= (UV)IV_MIN) {
			    /* 2s complement assumption again  */
			    /* -ve result, which could overflow an IV  */
			    SP--;
			    SETi(product_low == (UV)IV_MIN
                                    ? IV_MIN : -(IV)product_low);
			    RETURN;
			} /* else drop to NVs below. */
		    }
		} /* product_middle too large */
	    } /* ahigh && bhigh */
	} /* SvIOK(svl) */
    } /* SvIOK(svr) */
#endif
    {
      NV right = SvNV_nomg(svr);
      NV left  = SvNV_nomg(svl);
      NV result = left * right;

      (void)POPs;
#if defined(__sgi) && defined(USE_LONG_DOUBLE) && LONG_DOUBLEKIND == LONG_DOUBLE_IS_DOUBLEDOUBLE_128_BIT_BE_BE && NVSIZE == 16
      if (Perl_isinf(result)) {
          Zero((U8*)&result + 8, 8, U8);
      }
#endif
      SETn(result);
      RETURN;
    }
}

PP(pp_divide)
{
    dSP; dATARGET; SV *svl, *svr;
    tryAMAGICbin_MG(div_amg, AMGf_assign|AMGf_numeric);
    svr = TOPs;
    svl = TOPm1s;
    /* Only try to do UV divide first
       if ((SLOPPYDIVIDE is true) or
           (PERL_PRESERVE_IVUV is true and one or both SV is a UV too large
            to preserve))
       The assumption is that it is better to use floating point divide
       whenever possible, only doing integer divide first if we can't be sure.
       If NV_PRESERVES_UV is true then we know at compile time that no UV
       can be too large to preserve, so don't need to compile the code to
       test the size of UVs.  */

#ifdef SLOPPYDIVIDE
#  define PERL_TRY_UV_DIVIDE
    /* ensure that 20./5. == 4. */
#else
#  ifdef PERL_PRESERVE_IVUV
#    ifndef NV_PRESERVES_UV
#      define PERL_TRY_UV_DIVIDE
#    endif
#  endif
#endif

#ifdef PERL_TRY_UV_DIVIDE
    if (SvIV_please_nomg(svr) && SvIV_please_nomg(svl)) {
            bool left_non_neg = SvUOK(svl);
            bool right_non_neg = SvUOK(svr);
            UV left;
            UV right;

            if (right_non_neg) {
                right = SvUVX(svr);
            }
	    else {
		const IV biv = SvIVX(svr);
                if (biv >= 0) {
                    right = biv;
                    right_non_neg = TRUE; /* effectively it's a UV now */
                }
		else {
                    right = (biv == IV_MIN) ? (UV)biv : (UV)(-biv);
                }
            }
            /* historically undef()/0 gives a "Use of uninitialized value"
               warning before dieing, hence this test goes here.
               If it were immediately before the second SvIV_please, then
               DIE() would be invoked before left was even inspected, so
               no inspection would give no warning.  */
            if (right == 0)
                DIE(aTHX_ "Illegal division by zero");

            if (left_non_neg) {
                left = SvUVX(svl);
            }
	    else {
		const IV aiv = SvIVX(svl);
                if (aiv >= 0) {
                    left = aiv;
                    left_non_neg = TRUE; /* effectively it's a UV now */
                }
		else {
                    left = (aiv == IV_MIN) ? (UV)aiv : (UV)(-aiv);
                }
            }

            if (left >= right
#ifdef SLOPPYDIVIDE
                /* For sloppy divide we always attempt integer division.  */
#else
                /* Otherwise we only attempt it if either or both operands
                   would not be preserved by an NV.  If both fit in NVs
                   we fall through to the NV divide code below.  However,
                   as left >= right to ensure integer result here, we know that
                   we can skip the test on the right operand - right big
                   enough not to be preserved can't get here unless left is
                   also too big.  */

                && (left > ((UV)1 << NV_PRESERVES_UV_BITS))
#endif
                ) {
                /* Integer division can't overflow, but it can be imprecise.  */
		const UV result = left / right;
                if (result * right == left) {
                    SP--; /* result is valid */
                    if (left_non_neg == right_non_neg) {
                        /* signs identical, result is positive.  */
                        SETu( result );
                        RETURN;
                    }
                    /* 2s complement assumption */
                    if (result <= (UV)IV_MIN)
                        SETi(result == (UV)IV_MIN ? IV_MIN : -(IV)result);
                    else {
                        /* It's exact but too negative for IV. */
                        SETn( -(NV)result );
                    }
                    RETURN;
                } /* tried integer divide but it was not an integer result */
            } /* else (PERL_ABS(result) < 1.0) or (both UVs in range for NV) */
    } /* one operand wasn't SvIOK */
#endif /* PERL_TRY_UV_DIVIDE */
    {
	NV right = SvNV_nomg(svr);
	NV left  = SvNV_nomg(svl);
	(void)POPs;(void)POPs;
#if defined(NAN_COMPARE_BROKEN) && defined(Perl_isnan)
	if (! Perl_isnan(right) && right == 0.0)
#else
	if (right == 0.0)
#endif
	    DIE(aTHX_ "Illegal division by zero");
	PUSHn( left / right );
	RETURN;
    }
}

PP(pp_modulo)
{
    dSP; dATARGET;
    tryAMAGICbin_MG(modulo_amg, AMGf_assign|AMGf_numeric);
    {
	UV left  = 0;
	UV right = 0;
	bool left_neg = FALSE;
	bool right_neg = FALSE;
	bool use_double = FALSE;
	bool dright_valid = FALSE;
	NV dright = 0.0;
	NV dleft  = 0.0;
	SV * const svr = TOPs;
	SV * const svl = TOPm1s;
        if (SvIV_please_nomg(svr)) {
            right_neg = !SvUOK(svr);
            if (!right_neg) {
                right = SvUVX(svr);
            } else {
		const IV biv = SvIVX(svr);
                if (biv >= 0) {
                    right = biv;
                    right_neg = FALSE; /* effectively it's a UV now */
                } else {
                    right = (biv == IV_MIN) ? (UV)biv : (UV)(-biv);
                }
            }
        }
        else {
	    dright = SvNV_nomg(svr);
	    right_neg = dright < 0;
	    if (right_neg)
		dright = -dright;
            if (dright < UV_MAX_P1) {
                right = U_V(dright);
                dright_valid = TRUE; /* In case we need to use double below.  */
            } else {
                use_double = TRUE;
            }
	}

        /* At this point use_double is only true if right is out of range for
           a UV.  In range NV has been rounded down to nearest UV and
           use_double false.  */
	if (!use_double && SvIV_please_nomg(svl)) {
                left_neg = !SvUOK(svl);
                if (!left_neg) {
                    left = SvUVX(svl);
                } else {
		    const IV aiv = SvIVX(svl);
                    if (aiv >= 0) {
                        left = aiv;
                        left_neg = FALSE; /* effectively it's a UV now */
                    } else {
                        left = (aiv == IV_MIN) ? (UV)aiv : (UV)(-aiv);
                    }
                }
        }
	else {
	    dleft = SvNV_nomg(svl);
	    left_neg = dleft < 0;
	    if (left_neg)
		dleft = -dleft;

            /* This should be exactly the 5.6 behaviour - if left and right are
               both in range for UV then use U_V() rather than floor.  */
	    if (!use_double) {
                if (dleft < UV_MAX_P1) {
                    /* right was in range, so is dleft, so use UVs not double.
                     */
                    left = U_V(dleft);
                }
                /* left is out of range for UV, right was in range, so promote
                   right (back) to double.  */
                else {
                    /* The +0.5 is used in 5.6 even though it is not strictly
                       consistent with the implicit +0 floor in the U_V()
                       inside the #if 1. */
                    dleft = Perl_floor(dleft + 0.5);
                    use_double = TRUE;
                    if (dright_valid)
                        dright = Perl_floor(dright + 0.5);
                    else
                        dright = right;
                }
            }
        }
	sp -= 2;
	if (use_double) {
	    NV dans;

	    if (!dright)
		DIE(aTHX_ "Illegal modulus zero");

	    dans = Perl_fmod(dleft, dright);
	    if ((left_neg != right_neg) && dans)
		dans = dright - dans;
	    if (right_neg)
		dans = -dans;
	    sv_setnv(TARG, dans);
	}
	else {
	    UV ans;

	    if (!right)
		DIE(aTHX_ "Illegal modulus zero");

	    ans = left % right;
	    if ((left_neg != right_neg) && ans)
		ans = right - ans;
	    if (right_neg) {
		/* XXX may warn: unary minus operator applied to unsigned type */
		/* could change -foo to be (~foo)+1 instead	*/
		if (ans <= ~((UV)IV_MAX)+1)
		    sv_setiv(TARG, ~ans+1);
		else
		    sv_setnv(TARG, -(NV)ans);
	    }
	    else
		sv_setuv(TARG, ans);
	}
	PUSHTARG;
	RETURN;
    }
}

PP(pp_repeat)
{
    dSP; dATARGET;
    IV count;
    SV *sv;
    bool infnan = FALSE;

    if (GIMME_V == G_ARRAY && PL_op->op_private & OPpREPEAT_DOLIST) {
	/* TODO: think of some way of doing list-repeat overloading ??? */
	sv = POPs;
	SvGETMAGIC(sv);
    }
    else {
	if (UNLIKELY(PL_op->op_private & OPpREPEAT_DOLIST)) {
	    /* The parser saw this as a list repeat, and there
	       are probably several items on the stack. But we're
	       in scalar/void context, and there's no pp_list to save us
	       now. So drop the rest of the items -- robin@kitsite.com
	     */
	    dMARK;
	    if (MARK + 1 < SP) {
		MARK[1] = TOPm1s;
		MARK[2] = TOPs;
	    }
	    else {
		dTOPss;
		ASSUME(MARK + 1 == SP);
		XPUSHs(sv);
		MARK[1] = &PL_sv_undef;
	    }
	    SP = MARK + 2;
	}
	tryAMAGICbin_MG(repeat_amg, AMGf_assign);
	sv = POPs;
    }

    if (SvIOKp(sv)) {
	 if (SvUOK(sv)) {
	      const UV uv = SvUV_nomg(sv);
	      if (uv > IV_MAX)
		   count = IV_MAX; /* The best we can do? */
	      else
		   count = uv;
	 } else {
	      count = SvIV_nomg(sv);
	 }
    }
    else if (SvNOKp(sv)) {
        const NV nv = SvNV_nomg(sv);
        infnan = Perl_isinfnan(nv);
        if (UNLIKELY(infnan)) {
            count = 0;
        } else {
            if (nv < 0.0)
                count = -1;   /* An arbitrary negative integer */
            else
                count = (IV)nv;
        }
    }
    else
	count = SvIV_nomg(sv);

    if (infnan) {
        Perl_ck_warner(aTHX_ packWARN(WARN_NUMERIC),
                       "Non-finite repeat count does nothing");
    } else if (count < 0) {
        count = 0;
        Perl_ck_warner(aTHX_ packWARN(WARN_NUMERIC),
                       "Negative repeat count does nothing");
    }

    if (GIMME_V == G_ARRAY && PL_op->op_private & OPpREPEAT_DOLIST) {
	dMARK;
	const SSize_t items = SP - MARK;
	const U8 mod = PL_op->op_flags & OPf_MOD;

	if (count > 1) {
	    SSize_t max;

            if (  items > SSize_t_MAX / count   /* max would overflow */
                                                /* repeatcpy would overflow */
               || items > I32_MAX / (I32)sizeof(SV *)
            )
               Perl_croak(aTHX_ "%s","Out of memory during list extend");
            max = items * count;
            MEXTEND(MARK, max);

	    while (SP > MARK) {
                if (*SP) {
                   if (mod && SvPADTMP(*SP)) {
                       *SP = sv_mortalcopy(*SP);
                   }
		   SvTEMP_off((*SP));
		}
		SP--;
	    }
	    MARK++;
	    repeatcpy((char*)(MARK + items), (char*)MARK,
		items * sizeof(const SV *), count - 1);
	    SP += max;
	}
	else if (count <= 0)
	    SP = MARK;
    }
    else {	/* Note: mark already snarfed by pp_list */
	SV * const tmpstr = POPs;
	STRLEN len;
	bool isutf;

	if (TARG != tmpstr)
	    sv_setsv_nomg(TARG, tmpstr);
	SvPV_force_nomg(TARG, len);
	isutf = DO_UTF8(TARG);
	if (count != 1) {
	    if (count < 1)
		SvCUR_set(TARG, 0);
	    else {
		STRLEN max;

		if (   len > (MEM_SIZE_MAX-1) / (UV)count /* max would overflow */
		    || len > (U32)I32_MAX  /* repeatcpy would overflow */
                )
		     Perl_croak(aTHX_ "%s",
                                        "Out of memory during string extend");
		max = (UV)count * len + 1;
		SvGROW(TARG, max);

		repeatcpy(SvPVX(TARG) + len, SvPVX(TARG), len, count - 1);
		SvCUR_set(TARG, SvCUR(TARG) * count);
	    }
	    *SvEND(TARG) = '\0';
	}
	if (isutf)
	    (void)SvPOK_only_UTF8(TARG);
	else
	    (void)SvPOK_only(TARG);

	PUSHTARG;
    }
    RETURN;
}

PP(pp_subtract)
{
    dSP; dATARGET; bool useleft; SV *svl, *svr;
    tryAMAGICbin_MG(subtr_amg, AMGf_assign|AMGf_numeric);
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
             * simple integer subtract: if the top of both numbers
             * are 00  or 11, then it's safe */
            if (!( ((topl+1) | (topr+1)) & 2)) {
                SP--;
                TARGi(il - ir, 0); /* args not GMG, so can't be tainted */
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
            TARGn(nl - nr, 0); /* args not GMG, so can't be tainted */
            SETs(TARG);
            RETURN;
        }
    }

  generic:

    useleft = USE_LEFT(svl);
    /* See comments in pp_add (in pp_hot.c) about Overflow, and how
       "bad things" happen if you rely on signed integers wrapping.  */
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
	    /* left operand is undef, treat as zero.  */
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
		    } else { /* 2s complement assumption for IV_MIN */
			auv = (aiv == IV_MIN) ? (UV)aiv : (UV)-aiv;
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
                    buv = (biv == IV_MIN) ? (UV)biv : (UV)-biv;
	    }
	    /* ?uvok if value is >= 0. basically, flagged as UV if it's +ve,
	       else "IV" now, independent of how it came in.
	       if a, b represents positive, A, B negative, a maps to -A etc
	       a - b =>  (a - b)
	       A - b => -(a + b)
	       a - B =>  (a + b)
	       A - B => -(a - b)
	       all UV maths. negate result if A negative.
	       subtract if signs same, add if signs differ. */

	    if (auvok ^ buvok) {
		/* Signs differ.  */
		result = auv + buv;
		if (result >= auv)
		    result_good = 1;
	    } else {
		/* Signs same */
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
	    /* left operand is undef, treat as zero - value */
	    SETn(-value);
	    RETURN;
	}
	SETn( SvNV_nomg(svl) - value );
	RETURN;
    }
}

#define IV_BITS (IVSIZE * 8)

static UV S_uv_shift(UV uv, int shift, bool left)
{
   if (shift < 0) {
       shift = -shift;
       left = !left;
   }
   if (shift >= IV_BITS) {
       return 0;
   }
   return left ? uv << shift : uv >> shift;
}

static IV S_iv_shift(IV iv, int shift, bool left)
{
   if (shift < 0) {
       shift = -shift;
       left = !left;
   }
   if (shift >= IV_BITS) {
       return iv < 0 && !left ? -1 : 0;
   }
   return left ? iv << shift : iv >> shift;
}

#define UV_LEFT_SHIFT(uv, shift) S_uv_shift(uv, shift, TRUE)
#define UV_RIGHT_SHIFT(uv, shift) S_uv_shift(uv, shift, FALSE)
#define IV_LEFT_SHIFT(iv, shift) S_iv_shift(iv, shift, TRUE)
#define IV_RIGHT_SHIFT(iv, shift) S_iv_shift(iv, shift, FALSE)

PP(pp_left_shift)
{
    dSP; dATARGET; SV *svl, *svr;
    tryAMAGICbin_MG(lshift_amg, AMGf_assign|AMGf_numeric);
    svr = POPs;
    svl = TOPs;
    {
      const IV shift = SvIV_nomg(svr);
      if (PL_op->op_private & HINT_INTEGER) {
          SETi(IV_LEFT_SHIFT(SvIV_nomg(svl), shift));
      }
      else {
	  SETu(UV_LEFT_SHIFT(SvUV_nomg(svl), shift));
      }
      RETURN;
    }
}

PP(pp_right_shift)
{
    dSP; dATARGET; SV *svl, *svr;
    tryAMAGICbin_MG(rshift_amg, AMGf_assign|AMGf_numeric);
    svr = POPs;
    svl = TOPs;
    {
      const IV shift = SvIV_nomg(svr);
      if (PL_op->op_private & HINT_INTEGER) {
	  SETi(IV_RIGHT_SHIFT(SvIV_nomg(svl), shift));
      }
      else {
          SETu(UV_RIGHT_SHIFT(SvUV_nomg(svl), shift));
      }
      RETURN;
    }
}

PP(pp_lt)
{
    dSP;
    SV *left, *right;

    tryAMAGICbin_MG(lt_amg, AMGf_set|AMGf_numeric);
    right = POPs;
    left  = TOPs;
    SETs(boolSV(
	(SvIOK_notUV(left) && SvIOK_notUV(right))
	? (SvIVX(left) < SvIVX(right))
	: (do_ncmp(left, right) == -1)
    ));
    RETURN;
}

PP(pp_gt)
{
    dSP;
    SV *left, *right;

    tryAMAGICbin_MG(gt_amg, AMGf_set|AMGf_numeric);
    right = POPs;
    left  = TOPs;
    SETs(boolSV(
	(SvIOK_notUV(left) && SvIOK_notUV(right))
	? (SvIVX(left) > SvIVX(right))
	: (do_ncmp(left, right) == 1)
    ));
    RETURN;
}

PP(pp_le)
{
    dSP;
    SV *left, *right;

    tryAMAGICbin_MG(le_amg, AMGf_set|AMGf_numeric);
    right = POPs;
    left  = TOPs;
    SETs(boolSV(
	(SvIOK_notUV(left) && SvIOK_notUV(right))
	? (SvIVX(left) <= SvIVX(right))
	: (do_ncmp(left, right) <= 0)
    ));
    RETURN;
}

PP(pp_ge)
{
    dSP;
    SV *left, *right;

    tryAMAGICbin_MG(ge_amg, AMGf_set|AMGf_numeric);
    right = POPs;
    left  = TOPs;
    SETs(boolSV(
	(SvIOK_notUV(left) && SvIOK_notUV(right))
	? (SvIVX(left) >= SvIVX(right))
	: ( (do_ncmp(left, right) & 2) == 0)
    ));
    RETURN;
}

PP(pp_ne)
{
    dSP;
    SV *left, *right;

    tryAMAGICbin_MG(ne_amg, AMGf_set|AMGf_numeric);
    right = POPs;
    left  = TOPs;
    SETs(boolSV(
	(SvIOK_notUV(left) && SvIOK_notUV(right))
	? (SvIVX(left) != SvIVX(right))
	: (do_ncmp(left, right) != 0)
    ));
    RETURN;
}

/* compare left and right SVs. Returns:
 * -1: <
 *  0: ==
 *  1: >
 *  2: left or right was a NaN
 */
I32
Perl_do_ncmp(pTHX_ SV* const left, SV * const right)
{
    PERL_ARGS_ASSERT_DO_NCMP;
#ifdef PERL_PRESERVE_IVUV
    /* Fortunately it seems NaN isn't IOK */
    if (SvIV_please_nomg(right) && SvIV_please_nomg(left)) {
	    if (!SvUOK(left)) {
		const IV leftiv = SvIVX(left);
		if (!SvUOK(right)) {
		    /* ## IV <=> IV ## */
		    const IV rightiv = SvIVX(right);
		    return (leftiv > rightiv) - (leftiv < rightiv);
		}
		/* ## IV <=> UV ## */
		if (leftiv < 0)
		    /* As (b) is a UV, it's >=0, so it must be < */
		    return -1;
		{
		    const UV rightuv = SvUVX(right);
		    return ((UV)leftiv > rightuv) - ((UV)leftiv < rightuv);
		}
	    }

	    if (SvUOK(right)) {
		/* ## UV <=> UV ## */
		const UV leftuv = SvUVX(left);
		const UV rightuv = SvUVX(right);
		return (leftuv > rightuv) - (leftuv < rightuv);
	    }
	    /* ## UV <=> IV ## */
	    {
		const IV rightiv = SvIVX(right);
		if (rightiv < 0)
		    /* As (a) is a UV, it's >=0, so it cannot be < */
		    return 1;
		{
		    const UV leftuv = SvUVX(left);
		    return (leftuv > (UV)rightiv) - (leftuv < (UV)rightiv);
		}
	    }
	    NOT_REACHED; /* NOTREACHED */
    }
#endif
    {
      NV const rnv = SvNV_nomg(right);
      NV const lnv = SvNV_nomg(left);

#if defined(NAN_COMPARE_BROKEN) && defined(Perl_isnan)
      if (Perl_isnan(lnv) || Perl_isnan(rnv)) {
	  return 2;
       }
      return (lnv > rnv) - (lnv < rnv);
#else
      if (lnv < rnv)
	return -1;
      if (lnv > rnv)
	return 1;
      if (lnv == rnv)
	return 0;
      return 2;
#endif
    }
}


PP(pp_ncmp)
{
    dSP;
    SV *left, *right;
    I32 value;
    tryAMAGICbin_MG(ncmp_amg, AMGf_numeric);
    right = POPs;
    left  = TOPs;
    value = do_ncmp(left, right);
    if (value == 2) {
	SETs(&PL_sv_undef);
    }
    else {
	dTARGET;
	SETi(value);
    }
    RETURN;
}


/* also used for: pp_sge() pp_sgt() pp_slt() */

PP(pp_sle)
{
    dSP;

    int amg_type = sle_amg;
    int multiplier = 1;
    int rhs = 1;

    switch (PL_op->op_type) {
    case OP_SLT:
	amg_type = slt_amg;
	/* cmp < 0 */
	rhs = 0;
	break;
    case OP_SGT:
	amg_type = sgt_amg;
	/* cmp > 0 */
	multiplier = -1;
	rhs = 0;
	break;
    case OP_SGE:
	amg_type = sge_amg;
	/* cmp >= 0 */
	multiplier = -1;
	break;
    }

    tryAMAGICbin_MG(amg_type, AMGf_set);
    {
      dPOPTOPssrl;
      const int cmp =
#ifdef USE_LOCALE_COLLATE
                      (IN_LC_RUNTIME(LC_COLLATE))
		      ? sv_cmp_locale_flags(left, right, 0)
                      :
#endif
		        sv_cmp_flags(left, right, 0);
      SETs(boolSV(cmp * multiplier < rhs));
      RETURN;
    }
}

PP(pp_seq)
{
    dSP;
    tryAMAGICbin_MG(seq_amg, AMGf_set);
    {
      dPOPTOPssrl;
      SETs(boolSV(sv_eq_flags(left, right, 0)));
      RETURN;
    }
}

PP(pp_sne)
{
    dSP;
    tryAMAGICbin_MG(sne_amg, AMGf_set);
    {
      dPOPTOPssrl;
      SETs(boolSV(!sv_eq_flags(left, right, 0)));
      RETURN;
    }
}

PP(pp_scmp)
{
    dSP; dTARGET;
    tryAMAGICbin_MG(scmp_amg, 0);
    {
      dPOPTOPssrl;
      const int cmp =
#ifdef USE_LOCALE_COLLATE
                      (IN_LC_RUNTIME(LC_COLLATE))
		      ? sv_cmp_locale_flags(left, right, 0)
		      :
#endif
                        sv_cmp_flags(left, right, 0);
      SETi( cmp );
      RETURN;
    }
}

PP(pp_bit_and)
{
    dSP; dATARGET;
    tryAMAGICbin_MG(band_amg, AMGf_assign);
    {
      dPOPTOPssrl;
      if (SvNIOKp(left) || SvNIOKp(right)) {
	const bool left_ro_nonnum  = !SvNIOKp(left) && SvREADONLY(left);
	const bool right_ro_nonnum = !SvNIOKp(right) && SvREADONLY(right);
	if (PL_op->op_private & HINT_INTEGER) {
	  const IV i = SvIV_nomg(left) & SvIV_nomg(right);
	  SETi(i);
	}
	else {
	  const UV u = SvUV_nomg(left) & SvUV_nomg(right);
	  SETu(u);
	}
	if (left_ro_nonnum && left != TARG) SvNIOK_off(left);
	if (right_ro_nonnum) SvNIOK_off(right);
      }
      else {
	do_vop(PL_op->op_type, TARG, left, right);
	SETTARG;
      }
      RETURN;
    }
}

PP(pp_nbit_and)
{
    dSP;
    tryAMAGICbin_MG(band_amg, AMGf_assign|AMGf_numarg);
    {
	dATARGET; dPOPTOPssrl;
	if (PL_op->op_private & HINT_INTEGER) {
	  const IV i = SvIV_nomg(left) & SvIV_nomg(right);
	  SETi(i);
	}
	else {
	  const UV u = SvUV_nomg(left) & SvUV_nomg(right);
	  SETu(u);
	}
    }
    RETURN;
}

PP(pp_sbit_and)
{
    dSP;
    tryAMAGICbin_MG(sband_amg, AMGf_assign);
    {
	dATARGET; dPOPTOPssrl;
	do_vop(OP_BIT_AND, TARG, left, right);
	RETSETTARG;
    }
}

/* also used for: pp_bit_xor() */

PP(pp_bit_or)
{
    dSP; dATARGET;
    const int op_type = PL_op->op_type;

    tryAMAGICbin_MG((op_type == OP_BIT_OR ? bor_amg : bxor_amg), AMGf_assign);
    {
      dPOPTOPssrl;
      if (SvNIOKp(left) || SvNIOKp(right)) {
	const bool left_ro_nonnum  = !SvNIOKp(left) && SvREADONLY(left);
	const bool right_ro_nonnum = !SvNIOKp(right) && SvREADONLY(right);
	if (PL_op->op_private & HINT_INTEGER) {
	  const IV l = (USE_LEFT(left) ? SvIV_nomg(left) : 0);
	  const IV r = SvIV_nomg(right);
	  const IV result = op_type == OP_BIT_OR ? (l | r) : (l ^ r);
	  SETi(result);
	}
	else {
	  const UV l = (USE_LEFT(left) ? SvUV_nomg(left) : 0);
	  const UV r = SvUV_nomg(right);
	  const UV result = op_type == OP_BIT_OR ? (l | r) : (l ^ r);
	  SETu(result);
	}
	if (left_ro_nonnum && left != TARG) SvNIOK_off(left);
	if (right_ro_nonnum) SvNIOK_off(right);
      }
      else {
	do_vop(op_type, TARG, left, right);
	SETTARG;
      }
      RETURN;
    }
}

/* also used for: pp_nbit_xor() */

PP(pp_nbit_or)
{
    dSP;
    const int op_type = PL_op->op_type;

    tryAMAGICbin_MG((op_type == OP_NBIT_OR ? bor_amg : bxor_amg),
		    AMGf_assign|AMGf_numarg);
    {
	dATARGET; dPOPTOPssrl;
	if (PL_op->op_private & HINT_INTEGER) {
	  const IV l = (USE_LEFT(left) ? SvIV_nomg(left) : 0);
	  const IV r = SvIV_nomg(right);
	  const IV result = op_type == OP_NBIT_OR ? (l | r) : (l ^ r);
	  SETi(result);
	}
	else {
	  const UV l = (USE_LEFT(left) ? SvUV_nomg(left) : 0);
	  const UV r = SvUV_nomg(right);
	  const UV result = op_type == OP_NBIT_OR ? (l | r) : (l ^ r);
	  SETu(result);
	}
    }
    RETURN;
}

/* also used for: pp_sbit_xor() */

PP(pp_sbit_or)
{
    dSP;
    const int op_type = PL_op->op_type;

    tryAMAGICbin_MG((op_type == OP_SBIT_OR ? sbor_amg : sbxor_amg),
		    AMGf_assign);
    {
	dATARGET; dPOPTOPssrl;
	do_vop(op_type == OP_SBIT_OR ? OP_BIT_OR : OP_BIT_XOR, TARG, left,
	       right);
	RETSETTARG;
    }
}

PERL_STATIC_INLINE bool
S_negate_string(pTHX)
{
    dTARGET; dSP;
    STRLEN len;
    const char *s;
    SV * const sv = TOPs;
    if (!SvPOKp(sv) || SvNIOK(sv) || (!SvPOK(sv) && SvNIOKp(sv)))
	return FALSE;
    s = SvPV_nomg_const(sv, len);
    if (isIDFIRST(*s)) {
	sv_setpvs(TARG, "-");
	sv_catsv(TARG, sv);
    }
    else if (*s == '+' || (*s == '-' && !looks_like_number(sv))) {
	sv_setsv_nomg(TARG, sv);
	*SvPV_force_nomg(TARG, len) = *s == '-' ? '+' : '-';
    }
    else return FALSE;
    SETTARG;
    return TRUE;
}

PP(pp_negate)
{
    dSP; dTARGET;
    tryAMAGICun_MG(neg_amg, AMGf_numeric);
    if (S_negate_string(aTHX)) return NORMAL;
    {
	SV * const sv = TOPs;

	if (SvIOK(sv)) {
	    /* It's publicly an integer */
	oops_its_an_int:
	    if (SvIsUV(sv)) {
		if (SvIVX(sv) == IV_MIN) {
		    /* 2s complement assumption. */
                    SETi(SvIVX(sv));	/* special case: -((UV)IV_MAX+1) ==
                                           IV_MIN */
                    return NORMAL;
		}
		else if (SvUVX(sv) <= IV_MAX) {
		    SETi(-SvIVX(sv));
		    return NORMAL;
		}
	    }
	    else if (SvIVX(sv) != IV_MIN) {
		SETi(-SvIVX(sv));
		return NORMAL;
	    }
#ifdef PERL_PRESERVE_IVUV
	    else {
		SETu((UV)IV_MIN);
		return NORMAL;
	    }
#endif
	}
	if (SvNIOKp(sv) && (SvNIOK(sv) || !SvPOK(sv)))
	    SETn(-SvNV_nomg(sv));
	else if (SvPOKp(sv) && SvIV_please_nomg(sv))
		  goto oops_its_an_int;
	else
	    SETn(-SvNV_nomg(sv));
    }
    return NORMAL;
}

PP(pp_not)
{
    dSP;
    tryAMAGICun_MG(not_amg, AMGf_set);
    *PL_stack_sp = boolSV(!SvTRUE_nomg(*PL_stack_sp));
    return NORMAL;
}

static void
S_scomplement(pTHX_ SV *targ, SV *sv)
{
	U8 *tmps;
	I32 anum;
	STRLEN len;

	sv_copypv_nomg(TARG, sv);
	tmps = (U8*)SvPV_nomg(TARG, len);
	anum = len;
	if (SvUTF8(TARG)) {
	  /* Calculate exact length, let's not estimate. */
	  STRLEN targlen = 0;
	  STRLEN l;
	  UV nchar = 0;
	  UV nwide = 0;
	  U8 * const send = tmps + len;
	  U8 * const origtmps = tmps;
	  const UV utf8flags = UTF8_ALLOW_ANYUV;

	  while (tmps < send) {
	    const UV c = utf8n_to_uvchr(tmps, send-tmps, &l, utf8flags);
	    tmps += l;
	    targlen += UVCHR_SKIP(~c);
	    nchar++;
	    if (c > 0xff)
		nwide++;
	  }

	  /* Now rewind strings and write them. */
	  tmps = origtmps;

	  if (nwide) {
	      U8 *result;
	      U8 *p;

              Perl_ck_warner_d(aTHX_ packWARN(WARN_DEPRECATED),
                        deprecated_above_ff_msg, PL_op_desc[PL_op->op_type]);
	      Newx(result, targlen + 1, U8);
	      p = result;
	      while (tmps < send) {
		  const UV c = utf8n_to_uvchr(tmps, send-tmps, &l, utf8flags);
		  tmps += l;
		  p = uvchr_to_utf8_flags(p, ~c, UNICODE_ALLOW_ANY);
	      }
	      *p = '\0';
	      sv_usepvn_flags(TARG, (char*)result, targlen,
			      SV_HAS_TRAILING_NUL);
	      SvUTF8_on(TARG);
	  }
	  else {
	      U8 *result;
	      U8 *p;

	      Newx(result, nchar + 1, U8);
	      p = result;
	      while (tmps < send) {
		  const U8 c = (U8)utf8n_to_uvchr(tmps, send-tmps, &l, utf8flags);
		  tmps += l;
		  *p++ = ~c;
	      }
	      *p = '\0';
	      sv_usepvn_flags(TARG, (char*)result, nchar, SV_HAS_TRAILING_NUL);
	      SvUTF8_off(TARG);
	  }
	  return;
	}
#ifdef LIBERAL
	{
	    long *tmpl;
	    for ( ; anum && (unsigned long)tmps % sizeof(long); anum--, tmps++)
		*tmps = ~*tmps;
	    tmpl = (long*)tmps;
	    for ( ; anum >= (I32)sizeof(long); anum -= (I32)sizeof(long), tmpl++)
		*tmpl = ~*tmpl;
	    tmps = (U8*)tmpl;
	}
#endif
	for ( ; anum > 0; anum--, tmps++)
	    *tmps = ~*tmps;
}

PP(pp_complement)
{
    dSP; dTARGET;
    tryAMAGICun_MG(compl_amg, AMGf_numeric);
    {
      dTOPss;
      if (SvNIOKp(sv)) {
	if (PL_op->op_private & HINT_INTEGER) {
	  const IV i = ~SvIV_nomg(sv);
	  SETi(i);
	}
	else {
	  const UV u = ~SvUV_nomg(sv);
	  SETu(u);
	}
      }
      else {
	S_scomplement(aTHX_ TARG, sv);
	SETTARG;
      }
      return NORMAL;
    }
}

PP(pp_ncomplement)
{
    dSP;
    tryAMAGICun_MG(compl_amg, AMGf_numeric|AMGf_numarg);
    {
	dTARGET; dTOPss;
	if (PL_op->op_private & HINT_INTEGER) {
	  const IV i = ~SvIV_nomg(sv);
	  SETi(i);
	}
	else {
	  const UV u = ~SvUV_nomg(sv);
	  SETu(u);
	}
    }
    return NORMAL;
}

PP(pp_scomplement)
{
    dSP;
    tryAMAGICun_MG(scompl_amg, AMGf_numeric);
    {
	dTARGET; dTOPss;
	S_scomplement(aTHX_ TARG, sv);
	SETTARG;
	return NORMAL;
    }
}

/* integer versions of some of the above */

PP(pp_i_multiply)
{
    dSP; dATARGET;
    tryAMAGICbin_MG(mult_amg, AMGf_assign);
    {
      dPOPTOPiirl_nomg;
      SETi( left * right );
      RETURN;
    }
}

PP(pp_i_divide)
{
    IV num;
    dSP; dATARGET;
    tryAMAGICbin_MG(div_amg, AMGf_assign);
    {
      dPOPTOPssrl;
      IV value = SvIV_nomg(right);
      if (value == 0)
	  DIE(aTHX_ "Illegal division by zero");
      num = SvIV_nomg(left);

      /* avoid FPE_INTOVF on some platforms when num is IV_MIN */
      if (value == -1)
          value = - num;
      else
          value = num / value;
      SETi(value);
      RETURN;
    }
}

#if defined(__GLIBC__) && IVSIZE == 8 && !defined(PERL_DEBUG_READONLY_OPS) \
    && ( __GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 8))
STATIC
PP(pp_i_modulo_0)
#else
PP(pp_i_modulo)
#endif
{
     /* This is the vanilla old i_modulo. */
     dSP; dATARGET;
     tryAMAGICbin_MG(modulo_amg, AMGf_assign);
     {
	  dPOPTOPiirl_nomg;
	  if (!right)
	       DIE(aTHX_ "Illegal modulus zero");
	  /* avoid FPE_INTOVF on some platforms when left is IV_MIN */
	  if (right == -1)
	      SETi( 0 );
	  else
	      SETi( left % right );
	  RETURN;
     }
}

#if defined(__GLIBC__) && IVSIZE == 8 && !defined(PERL_DEBUG_READONLY_OPS) \
    && ( __GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 8))
STATIC
PP(pp_i_modulo_1)

{
     /* This is the i_modulo with the workaround for the _moddi3 bug
      * in (at least) glibc 2.2.5 (the PERL_ABS() the workaround).
      * See below for pp_i_modulo. */
     dSP; dATARGET;
     tryAMAGICbin_MG(modulo_amg, AMGf_assign);
     {
	  dPOPTOPiirl_nomg;
	  if (!right)
	       DIE(aTHX_ "Illegal modulus zero");
	  /* avoid FPE_INTOVF on some platforms when left is IV_MIN */
	  if (right == -1)
	      SETi( 0 );
	  else
	      SETi( left % PERL_ABS(right) );
	  RETURN;
     }
}

PP(pp_i_modulo)
{
     dVAR; dSP; dATARGET;
     tryAMAGICbin_MG(modulo_amg, AMGf_assign);
     {
	  dPOPTOPiirl_nomg;
	  if (!right)
	       DIE(aTHX_ "Illegal modulus zero");
	  /* The assumption is to use hereafter the old vanilla version... */
	  PL_op->op_ppaddr =
	       PL_ppaddr[OP_I_MODULO] =
	           Perl_pp_i_modulo_0;
	  /* .. but if we have glibc, we might have a buggy _moddi3
	   * (at least glibc 2.2.5 is known to have this bug), in other
	   * words our integer modulus with negative quad as the second
	   * argument might be broken.  Test for this and re-patch the
	   * opcode dispatch table if that is the case, remembering to
	   * also apply the workaround so that this first round works
	   * right, too.  See [perl #9402] for more information. */
	  {
	       IV l =   3;
	       IV r = -10;
	       /* Cannot do this check with inlined IV constants since
		* that seems to work correctly even with the buggy glibc. */
	       if (l % r == -3) {
		    /* Yikes, we have the bug.
		     * Patch in the workaround version. */
		    PL_op->op_ppaddr =
			 PL_ppaddr[OP_I_MODULO] =
			     &Perl_pp_i_modulo_1;
		    /* Make certain we work right this time, too. */
		    right = PERL_ABS(right);
	       }
	  }
	  /* avoid FPE_INTOVF on some platforms when left is IV_MIN */
	  if (right == -1)
	      SETi( 0 );
	  else
	      SETi( left % right );
	  RETURN;
     }
}
#endif

PP(pp_i_add)
{
    dSP; dATARGET;
    tryAMAGICbin_MG(add_amg, AMGf_assign);
    {
      dPOPTOPiirl_ul_nomg;
      SETi( left + right );
      RETURN;
    }
}

PP(pp_i_subtract)
{
    dSP; dATARGET;
    tryAMAGICbin_MG(subtr_amg, AMGf_assign);
    {
      dPOPTOPiirl_ul_nomg;
      SETi( left - right );
      RETURN;
    }
}

PP(pp_i_lt)
{
    dSP;
    tryAMAGICbin_MG(lt_amg, AMGf_set);
    {
      dPOPTOPiirl_nomg;
      SETs(boolSV(left < right));
      RETURN;
    }
}

PP(pp_i_gt)
{
    dSP;
    tryAMAGICbin_MG(gt_amg, AMGf_set);
    {
      dPOPTOPiirl_nomg;
      SETs(boolSV(left > right));
      RETURN;
    }
}

PP(pp_i_le)
{
    dSP;
    tryAMAGICbin_MG(le_amg, AMGf_set);
    {
      dPOPTOPiirl_nomg;
      SETs(boolSV(left <= right));
      RETURN;
    }
}

PP(pp_i_ge)
{
    dSP;
    tryAMAGICbin_MG(ge_amg, AMGf_set);
    {
      dPOPTOPiirl_nomg;
      SETs(boolSV(left >= right));
      RETURN;
    }
}

PP(pp_i_eq)
{
    dSP;
    tryAMAGICbin_MG(eq_amg, AMGf_set);
    {
      dPOPTOPiirl_nomg;
      SETs(boolSV(left == right));
      RETURN;
    }
}

PP(pp_i_ne)
{
    dSP;
    tryAMAGICbin_MG(ne_amg, AMGf_set);
    {
      dPOPTOPiirl_nomg;
      SETs(boolSV(left != right));
      RETURN;
    }
}

PP(pp_i_ncmp)
{
    dSP; dTARGET;
    tryAMAGICbin_MG(ncmp_amg, 0);
    {
      dPOPTOPiirl_nomg;
      I32 value;

      if (left > right)
	value = 1;
      else if (left < right)
	value = -1;
      else
	value = 0;
      SETi(value);
      RETURN;
    }
}

PP(pp_i_negate)
{
    dSP; dTARGET;
    tryAMAGICun_MG(neg_amg, 0);
    if (S_negate_string(aTHX)) return NORMAL;
    {
	SV * const sv = TOPs;
	IV const i = SvIV_nomg(sv);
	SETi(-i);
	return NORMAL;
    }
}

/* High falutin' math. */

PP(pp_atan2)
{
    dSP; dTARGET;
    tryAMAGICbin_MG(atan2_amg, 0);
    {
      dPOPTOPnnrl_nomg;
      SETn(Perl_atan2(left, right));
      RETURN;
    }
}


/* also used for: pp_cos() pp_exp() pp_log() pp_sqrt() */

PP(pp_sin)
{
    dSP; dTARGET;
    int amg_type = fallback_amg;
    const char *neg_report = NULL;
    const int op_type = PL_op->op_type;

    switch (op_type) {
    case OP_SIN:  amg_type = sin_amg; break;
    case OP_COS:  amg_type = cos_amg; break;
    case OP_EXP:  amg_type = exp_amg; break;
    case OP_LOG:  amg_type = log_amg;  neg_report = "log";  break;
    case OP_SQRT: amg_type = sqrt_amg; neg_report = "sqrt"; break;
    }

    assert(amg_type != fallback_amg);

    tryAMAGICun_MG(amg_type, 0);
    {
      SV * const arg = TOPs;
      const NV value = SvNV_nomg(arg);
      NV result = NV_NAN;
      if (neg_report) { /* log or sqrt */
	  if (
#if defined(NAN_COMPARE_BROKEN) && defined(Perl_isnan)
	      ! Perl_isnan(value) &&
#endif
	      (op_type == OP_LOG ? (value <= 0.0) : (value < 0.0))) {
	      SET_NUMERIC_STANDARD();
	      /* diag_listed_as: Can't take log of %g */
	      DIE(aTHX_ "Can't take %s of %"NVgf, neg_report, value);
	  }
      }
      switch (op_type) {
      default:
      case OP_SIN:  result = Perl_sin(value);  break;
      case OP_COS:  result = Perl_cos(value);  break;
      case OP_EXP:  result = Perl_exp(value);  break;
      case OP_LOG:  result = Perl_log(value);  break;
      case OP_SQRT: result = Perl_sqrt(value); break;
      }
      SETn(result);
      return NORMAL;
    }
}

/* Support Configure command-line overrides for rand() functions.
   After 5.005, perhaps we should replace this by Configure support
   for drand48(), random(), or rand().  For 5.005, though, maintain
   compatibility by calling rand() but allow the user to override it.
   See INSTALL for details.  --Andy Dougherty  15 July 1998
*/
/* Now it's after 5.005, and Configure supports drand48() and random(),
   in addition to rand().  So the overrides should not be needed any more.
   --Jarkko Hietaniemi	27 September 1998
 */

PP(pp_rand)
{
    if (!PL_srand_called) {
	(void)seedDrand01((Rand_seed_t)seed());
	PL_srand_called = TRUE;
    }
    {
	dSP;
	NV value;
    
	if (MAXARG < 1)
	{
	    EXTEND(SP, 1);
	    value = 1.0;
	}
	else {
	    SV * const sv = POPs;
	    if(!sv)
		value = 1.0;
	    else
		value = SvNV(sv);
	}
    /* 1 of 2 things can be carried through SvNV, SP or TARG, SP was carried */
#if defined(NAN_COMPARE_BROKEN) && defined(Perl_isnan)
	if (! Perl_isnan(value) && value == 0.0)
#else
	if (value == 0.0)
#endif
	    value = 1.0;
	{
	    dTARGET;
	    PUSHs(TARG);
	    PUTBACK;
	    value *= Drand01();
	    sv_setnv_mg(TARG, value);
	}
    }
    return NORMAL;
}

PP(pp_srand)
{
    dSP; dTARGET;
    UV anum;

    if (MAXARG >= 1 && (TOPs || POPs)) {
        SV *top;
        char *pv;
        STRLEN len;
        int flags;

        top = POPs;
        pv = SvPV(top, len);
        flags = grok_number(pv, len, &anum);

        if (!(flags & IS_NUMBER_IN_UV)) {
            Perl_ck_warner_d(aTHX_ packWARN(WARN_OVERFLOW),
                             "Integer overflow in srand");
            anum = UV_MAX;
        }
        (void)srand48_deterministic((Rand_seed_t)anum);
    }
    else {
        anum = seed();
        (void)seedDrand01((Rand_seed_t)anum);
    }

    PL_srand_called = TRUE;
    if (anum)
	XPUSHu(anum);
    else {
	/* Historically srand always returned true. We can avoid breaking
	   that like this:  */
	sv_setpvs(TARG, "0 but true");
	XPUSHTARG;
    }
    RETURN;
}

PP(pp_int)
{
    dSP; dTARGET;
    tryAMAGICun_MG(int_amg, AMGf_numeric);
    {
      SV * const sv = TOPs;
      const IV iv = SvIV_nomg(sv);
      /* XXX it's arguable that compiler casting to IV might be subtly
	 different from modf (for numbers inside (IV_MIN,UV_MAX)) in which
	 else preferring IV has introduced a subtle behaviour change bug. OTOH
	 relying on floating point to be accurate is a bug.  */

      if (!SvOK(sv)) {
        SETu(0);
      }
      else if (SvIOK(sv)) {
	if (SvIsUV(sv))
	    SETu(SvUV_nomg(sv));
	else
	    SETi(iv);
      }
      else {
	  const NV value = SvNV_nomg(sv);
	  if (UNLIKELY(Perl_isinfnan(value)))
	      SETn(value);
	  else if (value >= 0.0) {
	      if (value < (NV)UV_MAX + 0.5) {
		  SETu(U_V(value));
	      } else {
		  SETn(Perl_floor(value));
	      }
	  }
	  else {
	      if (value > (NV)IV_MIN - 0.5) {
		  SETi(I_V(value));
	      } else {
		  SETn(Perl_ceil(value));
	      }
	  }
      }
    }
    return NORMAL;
}

PP(pp_abs)
{
    dSP; dTARGET;
    tryAMAGICun_MG(abs_amg, AMGf_numeric);
    {
      SV * const sv = TOPs;
      /* This will cache the NV value if string isn't actually integer  */
      const IV iv = SvIV_nomg(sv);

      if (!SvOK(sv)) {
        SETu(0);
      }
      else if (SvIOK(sv)) {
	/* IVX is precise  */
	if (SvIsUV(sv)) {
	  SETu(SvUV_nomg(sv));	/* force it to be numeric only */
	} else {
	  if (iv >= 0) {
	    SETi(iv);
	  } else {
	    if (iv != IV_MIN) {
	      SETi(-iv);
	    } else {
	      /* 2s complement assumption. Also, not really needed as
		 IV_MIN and -IV_MIN should both be %100...00 and NV-able  */
	      SETu((UV)IV_MIN);
	    }
	  }
	}
      } else{
	const NV value = SvNV_nomg(sv);
	if (value < 0.0)
	  SETn(-value);
	else
	  SETn(value);
      }
    }
    return NORMAL;
}


/* also used for: pp_hex() */

PP(pp_oct)
{
    dSP; dTARGET;
    const char *tmps;
    I32 flags = PERL_SCAN_ALLOW_UNDERSCORES;
    STRLEN len;
    NV result_nv;
    UV result_uv;
    SV* const sv = TOPs;

    tmps = (SvPV_const(sv, len));
    if (DO_UTF8(sv)) {
	 /* If Unicode, try to downgrade
	  * If not possible, croak. */
	 SV* const tsv = sv_2mortal(newSVsv(sv));
	
	 SvUTF8_on(tsv);
	 sv_utf8_downgrade(tsv, FALSE);
	 tmps = SvPV_const(tsv, len);
    }
    if (PL_op->op_type == OP_HEX)
	goto hex;

    while (*tmps && len && isSPACE(*tmps))
        tmps++, len--;
    if (*tmps == '0')
        tmps++, len--;
    if (isALPHA_FOLD_EQ(*tmps, 'x')) {
    hex:
        result_uv = grok_hex (tmps, &len, &flags, &result_nv);
    }
    else if (isALPHA_FOLD_EQ(*tmps, 'b'))
        result_uv = grok_bin (tmps, &len, &flags, &result_nv);
    else
        result_uv = grok_oct (tmps, &len, &flags, &result_nv);

    if (flags & PERL_SCAN_GREATER_THAN_UV_MAX) {
        SETn(result_nv);
    }
    else {
        SETu(result_uv);
    }
    return NORMAL;
}

/* String stuff. */

PP(pp_length)
{
    dSP; dTARGET;
    SV * const sv = TOPs;

    U32 in_bytes = IN_BYTES;
    /* simplest case shortcut */
    /* turn off SVf_UTF8 in tmp flags if HINT_BYTES on*/
    U32 svflags = (SvFLAGS(sv) ^ (in_bytes << 26)) & (SVf_POK|SVs_GMG|SVf_UTF8);
    STATIC_ASSERT_STMT(HINT_BYTES == 0x00000008 && SVf_UTF8 == 0x20000000 && (SVf_UTF8 == HINT_BYTES << 26));
    SETs(TARG);

    if(LIKELY(svflags == SVf_POK))
        goto simple_pv;
    if(svflags & SVs_GMG)
        mg_get(sv);
    if (SvOK(sv)) {
	if (!IN_BYTES) /* reread to avoid using an C auto/register */
	    sv_setiv(TARG, (IV)sv_len_utf8_nomg(sv));
	else
	{
	    STRLEN len;
            /* unrolled SvPV_nomg_const(sv,len) */
            if(SvPOK_nog(sv)){
                simple_pv:
                len = SvCUR(sv);
            } else  {
                (void)sv_2pv_flags(sv, &len, 0|SV_CONST_RETURN);
            }
	    sv_setiv(TARG, (IV)(len));
	}
    } else {
	if (!SvPADTMP(TARG)) {
	    sv_setsv_nomg(TARG, &PL_sv_undef);
	} else { /* TARG is on stack at this point and is overwriten by SETs.
                   This branch is the odd one out, so put TARG by default on
                   stack earlier to let local SP go out of liveness sooner */
            SETs(&PL_sv_undef);
            goto no_set_magic;
        }
    }
    SvSETMAGIC(TARG);
    no_set_magic:
    return NORMAL; /* no putback, SP didn't move in this opcode */
}

/* Returns false if substring is completely outside original string.
   No length is indicated by len_iv = 0 and len_is_uv = 0.  len_is_uv must
   always be true for an explicit 0.
*/
bool
Perl_translate_substr_offsets( STRLEN curlen, IV pos1_iv,
				bool pos1_is_uv, IV len_iv,
				bool len_is_uv, STRLEN *posp,
				STRLEN *lenp)
{
    IV pos2_iv;
    int    pos2_is_uv;

    PERL_ARGS_ASSERT_TRANSLATE_SUBSTR_OFFSETS;

    if (!pos1_is_uv && pos1_iv < 0 && curlen) {
	pos1_is_uv = curlen-1 > ~(UV)pos1_iv;
	pos1_iv += curlen;
    }
    if ((pos1_is_uv || pos1_iv > 0) && (UV)pos1_iv > curlen)
	return FALSE;

    if (len_iv || len_is_uv) {
	if (!len_is_uv && len_iv < 0) {
	    pos2_iv = curlen + len_iv;
	    if (curlen)
		pos2_is_uv = curlen-1 > ~(UV)len_iv;
	    else
		pos2_is_uv = 0;
	} else {  /* len_iv >= 0 */
	    if (!pos1_is_uv && pos1_iv < 0) {
		pos2_iv = pos1_iv + len_iv;
		pos2_is_uv = (UV)len_iv > (UV)IV_MAX;
	    } else {
		if ((UV)len_iv > curlen-(UV)pos1_iv)
		    pos2_iv = curlen;
		else
		    pos2_iv = pos1_iv+len_iv;
		pos2_is_uv = 1;
	    }
	}
    }
    else {
	pos2_iv = curlen;
	pos2_is_uv = 1;
    }

    if (!pos2_is_uv && pos2_iv < 0) {
	if (!pos1_is_uv && pos1_iv < 0)
	    return FALSE;
	pos2_iv = 0;
    }
    else if (!pos1_is_uv && pos1_iv < 0)
	pos1_iv = 0;

    if ((UV)pos2_iv < (UV)pos1_iv)
	pos2_iv = pos1_iv;
    if ((UV)pos2_iv > curlen)
	pos2_iv = curlen;

    /* pos1_iv and pos2_iv both in 0..curlen, so the cast is safe */
    *posp = (STRLEN)( (UV)pos1_iv );
    *lenp = (STRLEN)( (UV)pos2_iv - (UV)pos1_iv );

    return TRUE;
}

PP(pp_substr)
{
    dSP; dTARGET;
    SV *sv;
    STRLEN curlen;
    STRLEN utf8_curlen;
    SV *   pos_sv;
    IV     pos1_iv;
    int    pos1_is_uv;
    SV *   len_sv;
    IV     len_iv = 0;
    int    len_is_uv = 0;
    I32 lvalue = PL_op->op_flags & OPf_MOD || LVRET;
    const bool rvalue = (GIMME_V != G_VOID);
    const char *tmps;
    SV *repl_sv = NULL;
    const char *repl = NULL;
    STRLEN repl_len;
    int num_args = PL_op->op_private & 7;
    bool repl_need_utf8_upgrade = FALSE;

    if (num_args > 2) {
	if (num_args > 3) {
	  if(!(repl_sv = POPs)) num_args--;
	}
	if ((len_sv = POPs)) {
	    len_iv    = SvIV(len_sv);
	    len_is_uv = len_iv ? SvIOK_UV(len_sv) : 1;
	}
	else num_args--;
    }
    pos_sv     = POPs;
    pos1_iv    = SvIV(pos_sv);
    pos1_is_uv = SvIOK_UV(pos_sv);
    sv = POPs;
    if (PL_op->op_private & OPpSUBSTR_REPL_FIRST) {
	assert(!repl_sv);
	repl_sv = POPs;
    }
    if (lvalue && !repl_sv) {
	SV * ret;
	ret = sv_2mortal(newSV_type(SVt_PVLV));  /* Not TARG RT#67838 */
	sv_magic(ret, NULL, PERL_MAGIC_substr, NULL, 0);
	LvTYPE(ret) = 'x';
	LvTARG(ret) = SvREFCNT_inc_simple(sv);
	LvTARGOFF(ret) =
	    pos1_is_uv || pos1_iv >= 0
		? (STRLEN)(UV)pos1_iv
		: (LvFLAGS(ret) |= 1, (STRLEN)(UV)-pos1_iv);
	LvTARGLEN(ret) =
	    len_is_uv || len_iv > 0
		? (STRLEN)(UV)len_iv
		: (LvFLAGS(ret) |= 2, (STRLEN)(UV)-len_iv);

	PUSHs(ret);    /* avoid SvSETMAGIC here */
	RETURN;
    }
    if (repl_sv) {
	repl = SvPV_const(repl_sv, repl_len);
	SvGETMAGIC(sv);
	if (SvROK(sv))
	    Perl_ck_warner(aTHX_ packWARN(WARN_SUBSTR),
			    "Attempt to use reference as lvalue in substr"
	    );
	tmps = SvPV_force_nomg(sv, curlen);
	if (DO_UTF8(repl_sv) && repl_len) {
	    if (!DO_UTF8(sv)) {
		sv_utf8_upgrade_nomg(sv);
		curlen = SvCUR(sv);
	    }
	}
	else if (DO_UTF8(sv))
	    repl_need_utf8_upgrade = TRUE;
    }
    else tmps = SvPV_const(sv, curlen);
    if (DO_UTF8(sv)) {
        utf8_curlen = sv_or_pv_len_utf8(sv, tmps, curlen);
	if (utf8_curlen == curlen)
	    utf8_curlen = 0;
	else
	    curlen = utf8_curlen;
    }
    else
	utf8_curlen = 0;

    {
	STRLEN pos, len, byte_len, byte_pos;

	if (!translate_substr_offsets(
		curlen, pos1_iv, pos1_is_uv, len_iv, len_is_uv, &pos, &len
	)) goto bound_fail;

	byte_len = len;
	byte_pos = utf8_curlen
	    ? sv_or_pv_pos_u2b(sv, tmps, pos, &byte_len) : pos;

	tmps += byte_pos;

	if (rvalue) {
	    SvTAINTED_off(TARG);			/* decontaminate */
	    SvUTF8_off(TARG);			/* decontaminate */
	    sv_setpvn(TARG, tmps, byte_len);
#ifdef USE_LOCALE_COLLATE
	    sv_unmagic(TARG, PERL_MAGIC_collxfrm);
#endif
	    if (utf8_curlen)
		SvUTF8_on(TARG);
	}

	if (repl) {
	    SV* repl_sv_copy = NULL;

	    if (repl_need_utf8_upgrade) {
		repl_sv_copy = newSVsv(repl_sv);
		sv_utf8_upgrade(repl_sv_copy);
		repl = SvPV_const(repl_sv_copy, repl_len);
	    }
	    if (!SvOK(sv))
		sv_setpvs(sv, "");
	    sv_insert_flags(sv, byte_pos, byte_len, repl, repl_len, 0);
	    SvREFCNT_dec(repl_sv_copy);
	}
    }
    if (PL_op->op_private & OPpSUBSTR_REPL_FIRST)
	SP++;
    else if (rvalue) {
	SvSETMAGIC(TARG);
	PUSHs(TARG);
    }
    RETURN;

  bound_fail:
    if (repl)
	Perl_croak(aTHX_ "substr outside of string");
    Perl_ck_warner(aTHX_ packWARN(WARN_SUBSTR), "substr outside of string");
    RETPUSHUNDEF;
}

PP(pp_vec)
{
    dSP;
    const IV size   = POPi;
    const IV offset = POPi;
    SV * const src = POPs;
    const I32 lvalue = PL_op->op_flags & OPf_MOD || LVRET;
    SV * ret;

    if (lvalue) {			/* it's an lvalue! */
	ret = sv_2mortal(newSV_type(SVt_PVLV));  /* Not TARG RT#67838 */
	sv_magic(ret, NULL, PERL_MAGIC_vec, NULL, 0);
	LvTYPE(ret) = 'v';
	LvTARG(ret) = SvREFCNT_inc_simple(src);
	LvTARGOFF(ret) = offset;
	LvTARGLEN(ret) = size;
    }
    else {
	dTARGET;
	SvTAINTED_off(TARG);		/* decontaminate */
	ret = TARG;
    }

    sv_setuv(ret, do_vecget(src, offset, size));
    if (!lvalue)
	SvSETMAGIC(ret);
    PUSHs(ret);
    RETURN;
}


/* also used for: pp_rindex() */

PP(pp_index)
{
    dSP; dTARGET;
    SV *big;
    SV *little;
    SV *temp = NULL;
    STRLEN biglen;
    STRLEN llen = 0;
    SSize_t offset = 0;
    SSize_t retval;
    const char *big_p;
    const char *little_p;
    bool big_utf8;
    bool little_utf8;
    const bool is_index = PL_op->op_type == OP_INDEX;
    const bool threeargs = MAXARG >= 3 && (TOPs || ((void)POPs,0));

    if (threeargs)
	offset = POPi;
    little = POPs;
    big = POPs;
    big_p = SvPV_const(big, biglen);
    little_p = SvPV_const(little, llen);

    big_utf8 = DO_UTF8(big);
    little_utf8 = DO_UTF8(little);
    if (big_utf8 ^ little_utf8) {
	/* One needs to be upgraded.  */
	if (little_utf8 && !IN_ENCODING) {
	    /* Well, maybe instead we might be able to downgrade the small
	       string?  */
	    char * const pv = (char*)bytes_from_utf8((U8 *)little_p, &llen,
						     &little_utf8);
	    if (little_utf8) {
		/* If the large string is ISO-8859-1, and it's not possible to
		   convert the small string to ISO-8859-1, then there is no
		   way that it could be found anywhere by index.  */
		retval = -1;
		goto fail;
	    }

	    /* At this point, pv is a malloc()ed string. So donate it to temp
	       to ensure it will get free()d  */
	    little = temp = newSV(0);
	    sv_usepvn(temp, pv, llen);
	    little_p = SvPVX(little);
	} else {
	    temp = little_utf8
		? newSVpvn(big_p, biglen) : newSVpvn(little_p, llen);

	    if (IN_ENCODING) {
		sv_recode_to_utf8(temp, _get_encoding());
	    } else {
		sv_utf8_upgrade(temp);
	    }
	    if (little_utf8) {
		big = temp;
		big_utf8 = TRUE;
		big_p = SvPV_const(big, biglen);
	    } else {
		little = temp;
		little_p = SvPV_const(little, llen);
	    }
	}
    }
    if (SvGAMAGIC(big)) {
	/* Life just becomes a lot easier if I use a temporary here.
	   Otherwise I need to avoid calls to sv_pos_u2b(), which (dangerously)
	   will trigger magic and overloading again, as will fbm_instr()
	*/
	big = newSVpvn_flags(big_p, biglen,
			     SVs_TEMP | (big_utf8 ? SVf_UTF8 : 0));
	big_p = SvPVX(big);
    }
    if (SvGAMAGIC(little) || (is_index && !SvOK(little))) {
	/* index && SvOK() is a hack. fbm_instr() calls SvPV_const, which will
	   warn on undef, and we've already triggered a warning with the
	   SvPV_const some lines above. We can't remove that, as we need to
	   call some SvPV to trigger overloading early and find out if the
	   string is UTF-8.
	   This is all getting too messy. The API isn't quite clean enough,
	   because data access has side effects.
	*/
	little = newSVpvn_flags(little_p, llen,
				SVs_TEMP | (little_utf8 ? SVf_UTF8 : 0));
	little_p = SvPVX(little);
    }

    if (!threeargs)
	offset = is_index ? 0 : biglen;
    else {
	if (big_utf8 && offset > 0)
	    offset = sv_pos_u2b_flags(big, offset, 0, SV_CONST_RETURN);
	if (!is_index)
	    offset += llen;
    }
    if (offset < 0)
	offset = 0;
    else if (offset > (SSize_t)biglen)
	offset = biglen;
    if (!(little_p = is_index
	  ? fbm_instr((unsigned char*)big_p + offset,
		      (unsigned char*)big_p + biglen, little, 0)
	  : rninstr(big_p,  big_p  + offset,
		    little_p, little_p + llen)))
	retval = -1;
    else {
	retval = little_p - big_p;
	if (retval > 1 && big_utf8)
	    retval = sv_pos_b2u_flags(big, retval, SV_CONST_RETURN);
    }
    SvREFCNT_dec(temp);
 fail:
    PUSHi(retval);
    RETURN;
}

PP(pp_sprintf)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    SvTAINTED_off(TARG);
    do_sprintf(TARG, SP-MARK, MARK+1);
    TAINT_IF(SvTAINTED(TARG));
    SP = ORIGMARK;
    PUSHTARG;
    RETURN;
}

PP(pp_ord)
{
    dSP; dTARGET;

    SV *argsv = TOPs;
    STRLEN len;
    const U8 *s = (U8*)SvPV_const(argsv, len);

    if (IN_ENCODING && SvPOK(argsv) && !DO_UTF8(argsv)) {
        SV * const tmpsv = sv_2mortal(newSVsv(argsv));
        s = (U8*)sv_recode_to_utf8(tmpsv, _get_encoding());
        len = UTF8SKIP(s);  /* Should be well-formed; so this is its length */
        argsv = tmpsv;
    }

    SETu(DO_UTF8(argsv)
           ? utf8n_to_uvchr(s, len, 0, UTF8_ALLOW_ANYUV)
           : (UV)(*s));

    return NORMAL;
}

PP(pp_chr)
{
    dSP; dTARGET;
    char *tmps;
    UV value;
    SV *top = TOPs;

    SvGETMAGIC(top);
    if (UNLIKELY(SvAMAGIC(top)))
	top = sv_2num(top);
    if (UNLIKELY(isinfnansv(top)))
        Perl_croak(aTHX_ "Cannot chr %"NVgf, SvNV(top));
    else {
        if (!IN_BYTES /* under bytes, chr(-1) eq chr(0xff), etc. */
            && ((SvIOKp(top) && !SvIsUV(top) && SvIV_nomg(top) < 0)
                ||
                ((SvNOKp(top) || (SvOK(top) && !SvIsUV(top)))
                 && SvNV_nomg(top) < 0.0)))
        {
	    if (ckWARN(WARN_UTF8)) {
		if (SvGMAGICAL(top)) {
		    SV *top2 = sv_newmortal();
		    sv_setsv_nomg(top2, top);
		    top = top2;
		}
                Perl_warner(aTHX_ packWARN(WARN_UTF8),
                            "Invalid negative number (%"SVf") in chr", SVfARG(top));
            }
            value = UNICODE_REPLACEMENT;
        } else {
            value = SvUV_nomg(top);
        }
    }

    SvUPGRADE(TARG,SVt_PV);

    if (value > 255 && !IN_BYTES) {
	SvGROW(TARG, (STRLEN)UVCHR_SKIP(value)+1);
	tmps = (char*)uvchr_to_utf8_flags((U8*)SvPVX(TARG), value, 0);
	SvCUR_set(TARG, tmps - SvPVX_const(TARG));
	*tmps = '\0';
	(void)SvPOK_only(TARG);
	SvUTF8_on(TARG);
	SETTARG;
	return NORMAL;
    }

    SvGROW(TARG,2);
    SvCUR_set(TARG, 1);
    tmps = SvPVX(TARG);
    *tmps++ = (char)value;
    *tmps = '\0';
    (void)SvPOK_only(TARG);

    if (IN_ENCODING && !IN_BYTES) {
        sv_recode_to_utf8(TARG, _get_encoding());
	tmps = SvPVX(TARG);
	if (SvCUR(TARG) == 0
	    || ! is_utf8_string((U8*)tmps, SvCUR(TARG))
	    || UTF8_IS_REPLACEMENT((U8*) tmps, (U8*) tmps + SvCUR(TARG)))
	{
	    SvGROW(TARG, 2);
	    tmps = SvPVX(TARG);
	    SvCUR_set(TARG, 1);
	    *tmps++ = (char)value;
	    *tmps = '\0';
	    SvUTF8_off(TARG);
	}
    }

    SETTARG;
    return NORMAL;
}

PP(pp_crypt)
{
#ifdef HAS_CRYPT
    dSP; dTARGET;
    dPOPTOPssrl;
    STRLEN len;
    const char *tmps = SvPV_const(left, len);

    if (DO_UTF8(left)) {
         /* If Unicode, try to downgrade.
	  * If not possible, croak.
	  * Yes, we made this up.  */
	 SV* const tsv = newSVpvn_flags(tmps, len, SVf_UTF8|SVs_TEMP);

	 sv_utf8_downgrade(tsv, FALSE);
	 tmps = SvPV_const(tsv, len);
    }
#   ifdef USE_ITHREADS
#     ifdef HAS_CRYPT_R
    if (!PL_reentrant_buffer->_crypt_struct_buffer) {
      /* This should be threadsafe because in ithreads there is only
       * one thread per interpreter.  If this would not be true,
       * we would need a mutex to protect this malloc. */
        PL_reentrant_buffer->_crypt_struct_buffer =
	  (struct crypt_data *)safemalloc(sizeof(struct crypt_data));
#if defined(__GLIBC__) || defined(__EMX__)
	if (PL_reentrant_buffer->_crypt_struct_buffer) {
	    PL_reentrant_buffer->_crypt_struct_buffer->initialized = 0;
	    /* work around glibc-2.2.5 bug */
	    PL_reentrant_buffer->_crypt_struct_buffer->current_saltbits = 0;
	}
#endif
    }
#     endif /* HAS_CRYPT_R */
#   endif /* USE_ITHREADS */
#   ifdef FCRYPT
    sv_setpv(TARG, fcrypt(tmps, SvPV_nolen_const(right)));
#   else
    sv_setpv(TARG, PerlProc_crypt(tmps, SvPV_nolen_const(right)));
#   endif
    SvUTF8_off(TARG);
    SETTARG;
    RETURN;
#else
    DIE(aTHX_
      "The crypt() function is unimplemented due to excessive paranoia.");
#endif
}

/* Generally UTF-8 and UTF-EBCDIC are indistinguishable at this level.  So 
 * most comments below say UTF-8, when in fact they mean UTF-EBCDIC as well */


/* also used for: pp_lcfirst() */

PP(pp_ucfirst)
{
    /* Actually is both lcfirst() and ucfirst().  Only the first character
     * changes.  This means that possibly we can change in-place, ie., just
     * take the source and change that one character and store it back, but not
     * if read-only etc, or if the length changes */

    dSP;
    SV *source = TOPs;
    STRLEN slen; /* slen is the byte length of the whole SV. */
    STRLEN need;
    SV *dest;
    bool inplace;   /* ? Convert first char only, in-place */
    bool doing_utf8 = FALSE;		   /* ? using utf8 */
    bool convert_source_to_utf8 = FALSE;   /* ? need to convert */
    const int op_type = PL_op->op_type;
    const U8 *s;
    U8 *d;
    U8 tmpbuf[UTF8_MAXBYTES_CASE+1];
    STRLEN ulen;    /* ulen is the byte length of the original Unicode character
		     * stored as UTF-8 at s. */
    STRLEN tculen;  /* tculen is the byte length of the freshly titlecased (or
		     * lowercased) character stored in tmpbuf.  May be either
		     * UTF-8 or not, but in either case is the number of bytes */

    s = (const U8*)SvPV_const(source, slen);

    /* We may be able to get away with changing only the first character, in
     * place, but not if read-only, etc.  Later we may discover more reasons to
     * not convert in-place. */
    inplace = !SvREADONLY(source) && SvPADTMP(source);

    /* First calculate what the changed first character should be.  This affects
     * whether we can just swap it out, leaving the rest of the string unchanged,
     * or even if have to convert the dest to UTF-8 when the source isn't */

    if (! slen) {   /* If empty */
	need = 1; /* still need a trailing NUL */
	ulen = 0;
    }
    else if (DO_UTF8(source)) {	/* Is the source utf8? */
	doing_utf8 = TRUE;
        ulen = UTF8SKIP(s);
        if (op_type == OP_UCFIRST) {
#ifdef USE_LOCALE_CTYPE
	    _to_utf8_title_flags(s, tmpbuf, &tculen, IN_LC_RUNTIME(LC_CTYPE));
#else
	    _to_utf8_title_flags(s, tmpbuf, &tculen, 0);
#endif
	}
        else {
#ifdef USE_LOCALE_CTYPE
	    _to_utf8_lower_flags(s, tmpbuf, &tculen, IN_LC_RUNTIME(LC_CTYPE));
#else
	    _to_utf8_lower_flags(s, tmpbuf, &tculen, 0);
#endif
	}

        /* we can't do in-place if the length changes.  */
        if (ulen != tculen) inplace = FALSE;
        need = slen + 1 - ulen + tculen;
    }
    else { /* Non-zero length, non-UTF-8,  Need to consider locale and if
	    * latin1 is treated as caseless.  Note that a locale takes
	    * precedence */ 
	ulen = 1;	/* Original character is 1 byte */
	tculen = 1;	/* Most characters will require one byte, but this will
			 * need to be overridden for the tricky ones */
	need = slen + 1;

	if (op_type == OP_LCFIRST) {

	    /* lower case the first letter: no trickiness for any character */
#ifdef USE_LOCALE_CTYPE
            if (IN_LC_RUNTIME(LC_CTYPE)) {
                _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
                *tmpbuf = toLOWER_LC(*s);
            }
            else
#endif
            {
                *tmpbuf = (IN_UNI_8_BIT)
                          ? toLOWER_LATIN1(*s)
                          : toLOWER(*s);
            }
	}
#ifdef USE_LOCALE_CTYPE
	/* is ucfirst() */
	else if (IN_LC_RUNTIME(LC_CTYPE)) {
            if (IN_UTF8_CTYPE_LOCALE) {
                goto do_uni_rules;
            }

            _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
            *tmpbuf = (U8) toUPPER_LC(*s); /* This would be a bug if any
                                              locales have upper and title case
                                              different */
	}
#endif
	else if (! IN_UNI_8_BIT) {
	    *tmpbuf = toUPPER(*s);	/* Returns caseless for non-ascii, or
					 * on EBCDIC machines whatever the
					 * native function does */
	}
        else {
            /* Here, is ucfirst non-UTF-8, not in locale (unless that locale is
             * UTF-8, which we treat as not in locale), and cased latin1 */
	    UV title_ord;
#ifdef USE_LOCALE_CTYPE
      do_uni_rules:
#endif

	    title_ord = _to_upper_title_latin1(*s, tmpbuf, &tculen, 's');
	    if (tculen > 1) {
		assert(tculen == 2);

                /* If the result is an upper Latin1-range character, it can
                 * still be represented in one byte, which is its ordinal */
		if (UTF8_IS_DOWNGRADEABLE_START(*tmpbuf)) {
		    *tmpbuf = (U8) title_ord;
		    tculen = 1;
		}
		else {
                    /* Otherwise it became more than one ASCII character (in
                     * the case of LATIN_SMALL_LETTER_SHARP_S) or changed to
                     * beyond Latin1, so the number of bytes changed, so can't
                     * replace just the first character in place. */
		    inplace = FALSE;

                    /* If the result won't fit in a byte, the entire result
                     * will have to be in UTF-8.  Assume worst case sizing in
                     * conversion. (all latin1 characters occupy at most two
                     * bytes in utf8) */
		    if (title_ord > 255) {
			doing_utf8 = TRUE;
			convert_source_to_utf8 = TRUE;
			need = slen * 2 + 1;

                        /* The (converted) UTF-8 and UTF-EBCDIC lengths of all
                         * (both) characters whose title case is above 255 is
                         * 2. */
			ulen = 2;
		    }
                    else { /* LATIN_SMALL_LETTER_SHARP_S expands by 1 byte */
			need = slen + 1 + 1;
		    }
		}
	    }
	} /* End of use Unicode (Latin1) semantics */
    } /* End of changing the case of the first character */

    /* Here, have the first character's changed case stored in tmpbuf.  Ready to
     * generate the result */
    if (inplace) {

	/* We can convert in place.  This means we change just the first
	 * character without disturbing the rest; no need to grow */
	dest = source;
	s = d = (U8*)SvPV_force_nomg(source, slen);
    } else {
	dTARGET;

	dest = TARG;

	/* Here, we can't convert in place; we earlier calculated how much
	 * space we will need, so grow to accommodate that */
	SvUPGRADE(dest, SVt_PV);
	d = (U8*)SvGROW(dest, need);
	(void)SvPOK_only(dest);

	SETs(dest);
    }

    if (doing_utf8) {
	if (! inplace) {
	    if (! convert_source_to_utf8) {

		/* Here  both source and dest are in UTF-8, but have to create
		 * the entire output.  We initialize the result to be the
		 * title/lower cased first character, and then append the rest
		 * of the string. */
		sv_setpvn(dest, (char*)tmpbuf, tculen);
		if (slen > ulen) {
		    sv_catpvn(dest, (char*)(s + ulen), slen - ulen);
		}
	    }
	    else {
		const U8 *const send = s + slen;

		/* Here the dest needs to be in UTF-8, but the source isn't,
		 * except we earlier UTF-8'd the first character of the source
		 * into tmpbuf.  First put that into dest, and then append the
		 * rest of the source, converting it to UTF-8 as we go. */

		/* Assert tculen is 2 here because the only two characters that
		 * get to this part of the code have 2-byte UTF-8 equivalents */
		*d++ = *tmpbuf;
		*d++ = *(tmpbuf + 1);
		s++;	/* We have just processed the 1st char */

		for (; s < send; s++) {
		    d = uvchr_to_utf8(d, *s);
		}
		*d = '\0';
		SvCUR_set(dest, d - (U8*)SvPVX_const(dest));
	    }
	    SvUTF8_on(dest);
	}
	else {   /* in-place UTF-8.  Just overwrite the first character */
	    Copy(tmpbuf, d, tculen, U8);
	    SvCUR_set(dest, need - 1);
	}

    }
    else {  /* Neither source nor dest are in or need to be UTF-8 */
	if (slen) {
	    if (inplace) {  /* in-place, only need to change the 1st char */
		*d = *tmpbuf;
	    }
	    else {	/* Not in-place */

		/* Copy the case-changed character(s) from tmpbuf */
		Copy(tmpbuf, d, tculen, U8);
		d += tculen - 1; /* Code below expects d to point to final
				  * character stored */
	    }
	}
	else {	/* empty source */
	    /* See bug #39028: Don't taint if empty  */
	    *d = *s;
	}

	/* In a "use bytes" we don't treat the source as UTF-8, but, still want
	 * the destination to retain that flag */
	if (SvUTF8(source) && ! IN_BYTES)
	    SvUTF8_on(dest);

	if (!inplace) {	/* Finish the rest of the string, unchanged */
	    /* This will copy the trailing NUL  */
	    Copy(s + 1, d + 1, slen, U8);
	    SvCUR_set(dest, need - 1);
	}
    }
#ifdef USE_LOCALE_CTYPE
    if (IN_LC_RUNTIME(LC_CTYPE)) {
        TAINT;
        SvTAINTED_on(dest);
    }
#endif
    if (dest != source && SvTAINTED(source))
	SvTAINT(dest);
    SvSETMAGIC(dest);
    return NORMAL;
}

/* There's so much setup/teardown code common between uc and lc, I wonder if
   it would be worth merging the two, and just having a switch outside each
   of the three tight loops.  There is less and less commonality though */
PP(pp_uc)
{
    dSP;
    SV *source = TOPs;
    STRLEN len;
    STRLEN min;
    SV *dest;
    const U8 *s;
    U8 *d;

    SvGETMAGIC(source);

    if (   SvPADTMP(source)
	&& !SvREADONLY(source) && SvPOK(source)
	&& !DO_UTF8(source)
	&& (
#ifdef USE_LOCALE_CTYPE
            (IN_LC_RUNTIME(LC_CTYPE))
            ? ! IN_UTF8_CTYPE_LOCALE
            :
#endif
              ! IN_UNI_8_BIT))
    {

        /* We can convert in place.  The reason we can't if in UNI_8_BIT is to
         * make the loop tight, so we overwrite the source with the dest before
         * looking at it, and we need to look at the original source
         * afterwards.  There would also need to be code added to handle
         * switching to not in-place in midstream if we run into characters
         * that change the length.  Since being in locale overrides UNI_8_BIT,
         * that latter becomes irrelevant in the above test; instead for
         * locale, the size can't normally change, except if the locale is a
         * UTF-8 one */
	dest = source;
	s = d = (U8*)SvPV_force_nomg(source, len);
	min = len + 1;
    } else {
	dTARGET;

	dest = TARG;

	s = (const U8*)SvPV_nomg_const(source, len);
	min = len + 1;

	SvUPGRADE(dest, SVt_PV);
	d = (U8*)SvGROW(dest, min);
	(void)SvPOK_only(dest);

	SETs(dest);
    }

    /* Overloaded values may have toggled the UTF-8 flag on source, so we need
       to check DO_UTF8 again here.  */

    if (DO_UTF8(source)) {
	const U8 *const send = s + len;
	U8 tmpbuf[UTF8_MAXBYTES_CASE+1];

	/* All occurrences of these are to be moved to follow any other marks.
	 * This is context-dependent.  We may not be passed enough context to
	 * move the iota subscript beyond all of them, but we do the best we can
	 * with what we're given.  The result is always better than if we
	 * hadn't done this.  And, the problem would only arise if we are
	 * passed a character without all its combining marks, which would be
	 * the caller's mistake.  The information this is based on comes from a
	 * comment in Unicode SpecialCasing.txt, (and the Standard's text
	 * itself) and so can't be checked properly to see if it ever gets
	 * revised.  But the likelihood of it changing is remote */
	bool in_iota_subscript = FALSE;

	while (s < send) {
	    STRLEN u;
	    STRLEN ulen;
	    UV uv;
	    if (in_iota_subscript && ! _is_utf8_mark(s)) {

		/* A non-mark.  Time to output the iota subscript */
		Copy(GREEK_CAPITAL_LETTER_IOTA_UTF8, d, capital_iota_len, U8);
                d += capital_iota_len;
		in_iota_subscript = FALSE;
            }

            /* Then handle the current character.  Get the changed case value
             * and copy it to the output buffer */

            u = UTF8SKIP(s);
#ifdef USE_LOCALE_CTYPE
            uv = _to_utf8_upper_flags(s, tmpbuf, &ulen, IN_LC_RUNTIME(LC_CTYPE));
#else
            uv = _to_utf8_upper_flags(s, tmpbuf, &ulen, 0);
#endif
#define GREEK_CAPITAL_LETTER_IOTA 0x0399
#define COMBINING_GREEK_YPOGEGRAMMENI 0x0345
            if (uv == GREEK_CAPITAL_LETTER_IOTA
                && utf8_to_uvchr_buf(s, send, 0) == COMBINING_GREEK_YPOGEGRAMMENI)
            {
                in_iota_subscript = TRUE;
            }
            else {
                if (ulen > u && (SvLEN(dest) < (min += ulen - u))) {
                    /* If the eventually required minimum size outgrows the
                     * available space, we need to grow. */
                    const UV o = d - (U8*)SvPVX_const(dest);

                    /* If someone uppercases one million U+03B0s we SvGROW()
                     * one million times.  Or we could try guessing how much to
                     * allocate without allocating too much.  Such is life.
                     * See corresponding comment in lc code for another option
                     * */
                    SvGROW(dest, min);
                    d = (U8*)SvPVX(dest) + o;
                }
                Copy(tmpbuf, d, ulen, U8);
                d += ulen;
            }
            s += u;
	}
	if (in_iota_subscript) {
            Copy(GREEK_CAPITAL_LETTER_IOTA_UTF8, d, capital_iota_len, U8);
            d += capital_iota_len;
	}
	SvUTF8_on(dest);
	*d = '\0';

	SvCUR_set(dest, d - (U8*)SvPVX_const(dest));
    }
    else {	/* Not UTF-8 */
	if (len) {
	    const U8 *const send = s + len;

	    /* Use locale casing if in locale; regular style if not treating
	     * latin1 as having case; otherwise the latin1 casing.  Do the
	     * whole thing in a tight loop, for speed, */
#ifdef USE_LOCALE_CTYPE
	    if (IN_LC_RUNTIME(LC_CTYPE)) {
                if (IN_UTF8_CTYPE_LOCALE) {
                    goto do_uni_rules;
                }
                _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
		for (; s < send; d++, s++)
                    *d = (U8) toUPPER_LC(*s);
	    }
	    else
#endif
                 if (! IN_UNI_8_BIT) {
		for (; s < send; d++, s++) {
		    *d = toUPPER(*s);
		}
	    }
	    else {
#ifdef USE_LOCALE_CTYPE
          do_uni_rules:
#endif
		for (; s < send; d++, s++) {
		    *d = toUPPER_LATIN1_MOD(*s);
		    if (LIKELY(*d != LATIN_SMALL_LETTER_Y_WITH_DIAERESIS)) {
                        continue;
                    }

		    /* The mainstream case is the tight loop above.  To avoid
		     * extra tests in that, all three characters that require
		     * special handling are mapped by the MOD to the one tested
		     * just above.  
		     * Use the source to distinguish between the three cases */

#if    UNICODE_MAJOR_VERSION > 2                                        \
   || (UNICODE_MAJOR_VERSION == 2 && UNICODE_DOT_VERSION >= 1		\
                                  && UNICODE_DOT_DOT_VERSION >= 8)
		    if (*s == LATIN_SMALL_LETTER_SHARP_S) {

			/* uc() of this requires 2 characters, but they are
			 * ASCII.  If not enough room, grow the string */
			if (SvLEN(dest) < ++min) {	
			    const UV o = d - (U8*)SvPVX_const(dest);
			    SvGROW(dest, min);
			    d = (U8*)SvPVX(dest) + o;
			}
			*d++ = 'S'; *d = 'S'; /* upper case is 'SS' */
			continue;   /* Back to the tight loop; still in ASCII */
		    }
#endif

		    /* The other two special handling characters have their
		     * upper cases outside the latin1 range, hence need to be
		     * in UTF-8, so the whole result needs to be in UTF-8.  So,
		     * here we are somewhere in the middle of processing a
		     * non-UTF-8 string, and realize that we will have to convert
		     * the whole thing to UTF-8.  What to do?  There are
		     * several possibilities.  The simplest to code is to
		     * convert what we have so far, set a flag, and continue on
		     * in the loop.  The flag would be tested each time through
		     * the loop, and if set, the next character would be
		     * converted to UTF-8 and stored.  But, I (khw) didn't want
		     * to slow down the mainstream case at all for this fairly
		     * rare case, so I didn't want to add a test that didn't
		     * absolutely have to be there in the loop, besides the
		     * possibility that it would get too complicated for
		     * optimizers to deal with.  Another possibility is to just
		     * give up, convert the source to UTF-8, and restart the
		     * function that way.  Another possibility is to convert
		     * both what has already been processed and what is yet to
		     * come separately to UTF-8, then jump into the loop that
		     * handles UTF-8.  But the most efficient time-wise of the
		     * ones I could think of is what follows, and turned out to
		     * not require much extra code.  */

		    /* Convert what we have so far into UTF-8, telling the
		     * function that we know it should be converted, and to
		     * allow extra space for what we haven't processed yet.
		     * Assume the worst case space requirements for converting
		     * what we haven't processed so far: that it will require
		     * two bytes for each remaining source character, plus the
		     * NUL at the end.  This may cause the string pointer to
		     * move, so re-find it. */

		    len = d - (U8*)SvPVX_const(dest);
		    SvCUR_set(dest, len);
		    len = sv_utf8_upgrade_flags_grow(dest,
						SV_GMAGIC|SV_FORCE_UTF8_UPGRADE,
						(send -s) * 2 + 1);
		    d = (U8*)SvPVX(dest) + len;

		    /* Now process the remainder of the source, converting to
		     * upper and UTF-8.  If a resulting byte is invariant in
		     * UTF-8, output it as-is, otherwise convert to UTF-8 and
		     * append it to the output. */
		    for (; s < send; s++) {
			(void) _to_upper_title_latin1(*s, d, &len, 'S');
			d += len;
		    }

		    /* Here have processed the whole source; no need to continue
		     * with the outer loop.  Each character has been converted
		     * to upper case and converted to UTF-8 */

		    break;
		} /* End of processing all latin1-style chars */
	    } /* End of processing all chars */
	} /* End of source is not empty */

	if (source != dest) {
	    *d = '\0';  /* Here d points to 1 after last char, add NUL */
	    SvCUR_set(dest, d - (U8*)SvPVX_const(dest));
	}
    } /* End of isn't utf8 */
#ifdef USE_LOCALE_CTYPE
    if (IN_LC_RUNTIME(LC_CTYPE)) {
        TAINT;
        SvTAINTED_on(dest);
    }
#endif
    if (dest != source && SvTAINTED(source))
	SvTAINT(dest);
    SvSETMAGIC(dest);
    return NORMAL;
}

PP(pp_lc)
{
    dSP;
    SV *source = TOPs;
    STRLEN len;
    STRLEN min;
    SV *dest;
    const U8 *s;
    U8 *d;

    SvGETMAGIC(source);

    if (   SvPADTMP(source)
	&& !SvREADONLY(source) && SvPOK(source)
	&& !DO_UTF8(source)) {

	/* We can convert in place, as lowercasing anything in the latin1 range
	 * (or else DO_UTF8 would have been on) doesn't lengthen it */
	dest = source;
	s = d = (U8*)SvPV_force_nomg(source, len);
	min = len + 1;
    } else {
	dTARGET;

	dest = TARG;

	s = (const U8*)SvPV_nomg_const(source, len);
	min = len + 1;

	SvUPGRADE(dest, SVt_PV);
	d = (U8*)SvGROW(dest, min);
	(void)SvPOK_only(dest);

	SETs(dest);
    }

    /* Overloaded values may have toggled the UTF-8 flag on source, so we need
       to check DO_UTF8 again here.  */

    if (DO_UTF8(source)) {
	const U8 *const send = s + len;
	U8 tmpbuf[UTF8_MAXBYTES_CASE+1];

	while (s < send) {
	    const STRLEN u = UTF8SKIP(s);
	    STRLEN ulen;

#ifdef USE_LOCALE_CTYPE
	    _to_utf8_lower_flags(s, tmpbuf, &ulen, IN_LC_RUNTIME(LC_CTYPE));
#else
	    _to_utf8_lower_flags(s, tmpbuf, &ulen, 0);
#endif

	    /* Here is where we would do context-sensitive actions.  See the
	     * commit message for 86510fb15 for why there isn't any */

	    if (ulen > u && (SvLEN(dest) < (min += ulen - u))) {

		/* If the eventually required minimum size outgrows the
		 * available space, we need to grow. */
		const UV o = d - (U8*)SvPVX_const(dest);

		/* If someone lowercases one million U+0130s we SvGROW() one
		 * million times.  Or we could try guessing how much to
		 * allocate without allocating too much.  Such is life.
		 * Another option would be to grow an extra byte or two more
		 * each time we need to grow, which would cut down the million
		 * to 500K, with little waste */
		SvGROW(dest, min);
		d = (U8*)SvPVX(dest) + o;
	    }

	    /* Copy the newly lowercased letter to the output buffer we're
	     * building */
	    Copy(tmpbuf, d, ulen, U8);
	    d += ulen;
	    s += u;
	}   /* End of looping through the source string */
	SvUTF8_on(dest);
	*d = '\0';
	SvCUR_set(dest, d - (U8*)SvPVX_const(dest));
    } else {	/* Not utf8 */
	if (len) {
	    const U8 *const send = s + len;

	    /* Use locale casing if in locale; regular style if not treating
	     * latin1 as having case; otherwise the latin1 casing.  Do the
	     * whole thing in a tight loop, for speed, */
#ifdef USE_LOCALE_CTYPE
            if (IN_LC_RUNTIME(LC_CTYPE)) {
                _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
		for (; s < send; d++, s++)
		    *d = toLOWER_LC(*s);
            }
	    else
#endif
            if (! IN_UNI_8_BIT) {
		for (; s < send; d++, s++) {
		    *d = toLOWER(*s);
		}
	    }
	    else {
		for (; s < send; d++, s++) {
		    *d = toLOWER_LATIN1(*s);
		}
	    }
	}
	if (source != dest) {
	    *d = '\0';
	    SvCUR_set(dest, d - (U8*)SvPVX_const(dest));
	}
    }
#ifdef USE_LOCALE_CTYPE
    if (IN_LC_RUNTIME(LC_CTYPE)) {
        TAINT;
        SvTAINTED_on(dest);
    }
#endif
    if (dest != source && SvTAINTED(source))
	SvTAINT(dest);
    SvSETMAGIC(dest);
    return NORMAL;
}

PP(pp_quotemeta)
{
    dSP; dTARGET;
    SV * const sv = TOPs;
    STRLEN len;
    const char *s = SvPV_const(sv,len);

    SvUTF8_off(TARG);				/* decontaminate */
    if (len) {
	char *d;
	SvUPGRADE(TARG, SVt_PV);
	SvGROW(TARG, (len * 2) + 1);
	d = SvPVX(TARG);
	if (DO_UTF8(sv)) {
	    while (len) {
		STRLEN ulen = UTF8SKIP(s);
		bool to_quote = FALSE;

		if (UTF8_IS_INVARIANT(*s)) {
		    if (_isQUOTEMETA(*s)) {
			to_quote = TRUE;
		    }
		}
		else if (UTF8_IS_DOWNGRADEABLE_START(*s)) {
		    if (
#ifdef USE_LOCALE_CTYPE
		    /* In locale, we quote all non-ASCII Latin1 chars.
		     * Otherwise use the quoting rules */
		    
		    IN_LC_RUNTIME(LC_CTYPE)
			||
#endif
			_isQUOTEMETA(EIGHT_BIT_UTF8_TO_NATIVE(*s, *(s + 1))))
		    {
			to_quote = TRUE;
		    }
		}
		else if (is_QUOTEMETA_high(s)) {
		    to_quote = TRUE;
		}

		if (to_quote) {
		    *d++ = '\\';
		}
		if (ulen > len)
		    ulen = len;
		len -= ulen;
		while (ulen--)
		    *d++ = *s++;
	    }
	    SvUTF8_on(TARG);
	}
	else if (IN_UNI_8_BIT) {
	    while (len--) {
		if (_isQUOTEMETA(*s))
		    *d++ = '\\';
		*d++ = *s++;
	    }
	}
	else {
	    /* For non UNI_8_BIT (and hence in locale) just quote all \W
	     * including everything above ASCII */
	    while (len--) {
		if (!isWORDCHAR_A(*s))
		    *d++ = '\\';
		*d++ = *s++;
	    }
	}
	*d = '\0';
	SvCUR_set(TARG, d - SvPVX_const(TARG));
	(void)SvPOK_only_UTF8(TARG);
    }
    else
	sv_setpvn(TARG, s, len);
    SETTARG;
    return NORMAL;
}

PP(pp_fc)
{
    dTARGET;
    dSP;
    SV *source = TOPs;
    STRLEN len;
    STRLEN min;
    SV *dest;
    const U8 *s;
    const U8 *send;
    U8 *d;
    U8 tmpbuf[UTF8_MAXBYTES_CASE + 1];
#if    UNICODE_MAJOR_VERSION > 3 /* no multifolds in early Unicode */   \
   || (UNICODE_MAJOR_VERSION == 3 && (   UNICODE_DOT_VERSION > 0)       \
                                      || UNICODE_DOT_DOT_VERSION > 0)
    const bool full_folding = TRUE; /* This variable is here so we can easily
                                       move to more generality later */
#else
    const bool full_folding = FALSE;
#endif
    const U8 flags = ( full_folding      ? FOLD_FLAGS_FULL   : 0 )
#ifdef USE_LOCALE_CTYPE
                   | ( IN_LC_RUNTIME(LC_CTYPE) ? FOLD_FLAGS_LOCALE : 0 )
#endif
    ;

    /* This is a facsimile of pp_lc, but with a thousand bugs thanks to me.
     * You are welcome(?) -Hugmeir
     */

    SvGETMAGIC(source);

    dest = TARG;

    if (SvOK(source)) {
        s = (const U8*)SvPV_nomg_const(source, len);
    } else {
        if (ckWARN(WARN_UNINITIALIZED))
	    report_uninit(source);
	s = (const U8*)"";
	len = 0;
    }

    min = len + 1;

    SvUPGRADE(dest, SVt_PV);
    d = (U8*)SvGROW(dest, min);
    (void)SvPOK_only(dest);

    SETs(dest);

    send = s + len;
    if (DO_UTF8(source)) { /* UTF-8 flagged string. */
        while (s < send) {
            const STRLEN u = UTF8SKIP(s);
            STRLEN ulen;

            _to_utf8_fold_flags(s, tmpbuf, &ulen, flags);

            if (ulen > u && (SvLEN(dest) < (min += ulen - u))) {
                const UV o = d - (U8*)SvPVX_const(dest);
                SvGROW(dest, min);
                d = (U8*)SvPVX(dest) + o;
            }

            Copy(tmpbuf, d, ulen, U8);
            d += ulen;
            s += u;
        }
        SvUTF8_on(dest);
    } /* Unflagged string */
    else if (len) {
#ifdef USE_LOCALE_CTYPE
        if ( IN_LC_RUNTIME(LC_CTYPE) ) { /* Under locale */
            if (IN_UTF8_CTYPE_LOCALE) {
                goto do_uni_folding;
            }
            _CHECK_AND_WARN_PROBLEMATIC_LOCALE;
            for (; s < send; d++, s++)
                *d = (U8) toFOLD_LC(*s);
        }
        else
#endif
        if ( !IN_UNI_8_BIT ) { /* Under nothing, or bytes */
            for (; s < send; d++, s++)
                *d = toFOLD(*s);
        }
        else {
#ifdef USE_LOCALE_CTYPE
      do_uni_folding:
#endif
            /* For ASCII and the Latin-1 range, there's only two troublesome
             * folds, \x{DF} (\N{LATIN SMALL LETTER SHARP S}), which under full
             * casefolding becomes 'ss'; and \x{B5} (\N{MICRO SIGN}), which
             * under any fold becomes \x{3BC} (\N{GREEK SMALL LETTER MU}) --
             * For the rest, the casefold is their lowercase.  */
            for (; s < send; d++, s++) {
                if (*s == MICRO_SIGN) {
                    /* \N{MICRO SIGN}'s casefold is \N{GREEK SMALL LETTER MU},
                     * which is outside of the latin-1 range. There's a couple
                     * of ways to deal with this -- khw discusses them in
                     * pp_lc/uc, so go there :) What we do here is upgrade what
                     * we had already casefolded, then enter an inner loop that
                     * appends the rest of the characters as UTF-8. */
                    len = d - (U8*)SvPVX_const(dest);
                    SvCUR_set(dest, len);
                    len = sv_utf8_upgrade_flags_grow(dest,
                                                SV_GMAGIC|SV_FORCE_UTF8_UPGRADE,
						/* The max expansion for latin1
						 * chars is 1 byte becomes 2 */
                                                (send -s) * 2 + 1);
                    d = (U8*)SvPVX(dest) + len;

                    Copy(GREEK_SMALL_LETTER_MU_UTF8, d, small_mu_len, U8);
                    d += small_mu_len;
                    s++;
                    for (; s < send; s++) {
                        STRLEN ulen;
                        UV fc = _to_uni_fold_flags(*s, tmpbuf, &ulen, flags);
                        if UVCHR_IS_INVARIANT(fc) {
                            if (full_folding
                                && *s == LATIN_SMALL_LETTER_SHARP_S)
                            {
                                *d++ = 's';
                                *d++ = 's';
                            }
                            else
                                *d++ = (U8)fc;
                        }
                        else {
                            Copy(tmpbuf, d, ulen, U8);
                            d += ulen;
                        }
                    }
                    break;
                }
                else if (full_folding && *s == LATIN_SMALL_LETTER_SHARP_S) {
                    /* Under full casefolding, LATIN SMALL LETTER SHARP S
                     * becomes "ss", which may require growing the SV. */
                    if (SvLEN(dest) < ++min) {
                        const UV o = d - (U8*)SvPVX_const(dest);
                        SvGROW(dest, min);
                        d = (U8*)SvPVX(dest) + o;
                     }
                    *(d)++ = 's';
                    *d = 's';
                }
                else { /* If it's not one of those two, the fold is their lower
                          case */
                    *d = toLOWER_LATIN1(*s);
                }
             }
        }
    }
    *d = '\0';
    SvCUR_set(dest, d - (U8*)SvPVX_const(dest));

#ifdef USE_LOCALE_CTYPE
    if (IN_LC_RUNTIME(LC_CTYPE)) {
        TAINT;
        SvTAINTED_on(dest);
    }
#endif
    if (SvTAINTED(source))
	SvTAINT(dest);
    SvSETMAGIC(dest);
    RETURN;
}

/* Arrays. */

PP(pp_aslice)
{
    dSP; dMARK; dORIGMARK;
    AV *const av = MUTABLE_AV(POPs);
    const I32 lval = (PL_op->op_flags & OPf_MOD || LVRET);

    if (SvTYPE(av) == SVt_PVAV) {
	const bool localizing = PL_op->op_private & OPpLVAL_INTRO;
	bool can_preserve = FALSE;

	if (localizing) {
	    MAGIC *mg;
	    HV *stash;

	    can_preserve = SvCANEXISTDELETE(av);
	}

	if (lval && localizing) {
	    SV **svp;
	    SSize_t max = -1;
	    for (svp = MARK + 1; svp <= SP; svp++) {
		const SSize_t elem = SvIV(*svp);
		if (elem > max)
		    max = elem;
	    }
	    if (max > AvMAX(av))
		av_extend(av, max);
	}

	while (++MARK <= SP) {
	    SV **svp;
	    SSize_t elem = SvIV(*MARK);
	    bool preeminent = TRUE;

	    if (localizing && can_preserve) {
		/* If we can determine whether the element exist,
		 * Try to preserve the existenceness of a tied array
		 * element by using EXISTS and DELETE if possible.
		 * Fallback to FETCH and STORE otherwise. */
		preeminent = av_exists(av, elem);
	    }

	    svp = av_fetch(av, elem, lval);
	    if (lval) {
		if (!svp || !*svp)
		    DIE(aTHX_ PL_no_aelem, elem);
		if (localizing) {
		    if (preeminent)
			save_aelem(av, elem, svp);
		    else
			SAVEADELETE(av, elem);
		}
	    }
	    *MARK = svp ? *svp : &PL_sv_undef;
	}
    }
    if (GIMME_V != G_ARRAY) {
	MARK = ORIGMARK;
	*++MARK = SP > ORIGMARK ? *SP : &PL_sv_undef;
	SP = MARK;
    }
    RETURN;
}

PP(pp_kvaslice)
{
    dSP; dMARK;
    AV *const av = MUTABLE_AV(POPs);
    I32 lval = (PL_op->op_flags & OPf_MOD);
    SSize_t items = SP - MARK;

    if (PL_op->op_private & OPpMAYBE_LVSUB) {
       const I32 flags = is_lvalue_sub();
       if (flags) {
           if (!(flags & OPpENTERSUB_INARGS))
               /* diag_listed_as: Can't modify %s in %s */
	       Perl_croak(aTHX_ "Can't modify index/value array slice in list assignment");
	   lval = flags;
       }
    }

    MEXTEND(SP,items);
    while (items > 1) {
	*(MARK+items*2-1) = *(MARK+items);
	items--;
    }
    items = SP-MARK;
    SP += items;

    while (++MARK <= SP) {
        SV **svp;

	svp = av_fetch(av, SvIV(*MARK), lval);
        if (lval) {
            if (!svp || !*svp || *svp == &PL_sv_undef) {
                DIE(aTHX_ PL_no_aelem, SvIV(*MARK));
            }
	    *MARK = sv_mortalcopy(*MARK);
        }
	*++MARK = svp ? *svp : &PL_sv_undef;
    }
    if (GIMME_V != G_ARRAY) {
	MARK = SP - items*2;
	*++MARK = items > 0 ? *SP : &PL_sv_undef;
	SP = MARK;
    }
    RETURN;
}


PP(pp_aeach)
{
    dSP;
    AV *array = MUTABLE_AV(POPs);
    const U8 gimme = GIMME_V;
    IV *iterp = Perl_av_iter_p(aTHX_ array);
    const IV current = (*iterp)++;

    if (current > av_tindex(array)) {
	*iterp = 0;
	if (gimme == G_SCALAR)
	    RETPUSHUNDEF;
	else
	    RETURN;
    }

    EXTEND(SP, 2);
    mPUSHi(current);
    if (gimme == G_ARRAY) {
	SV **const element = av_fetch(array, current, 0);
        PUSHs(element ? *element : &PL_sv_undef);
    }
    RETURN;
}

/* also used for: pp_avalues()*/
PP(pp_akeys)
{
    dSP;
    AV *array = MUTABLE_AV(POPs);
    const U8 gimme = GIMME_V;

    *Perl_av_iter_p(aTHX_ array) = 0;

    if (gimme == G_SCALAR) {
	dTARGET;
	PUSHi(av_tindex(array) + 1);
    }
    else if (gimme == G_ARRAY) {
        IV n = Perl_av_len(aTHX_ array);
        IV i;

        EXTEND(SP, n + 1);

	if (PL_op->op_type == OP_AKEYS) {
	    for (i = 0;  i <= n;  i++) {
		mPUSHi(i);
	    }
	}
	else {
	    for (i = 0;  i <= n;  i++) {
		SV *const *const elem = Perl_av_fetch(aTHX_ array, i, 0);
		PUSHs(elem ? *elem : &PL_sv_undef);
	    }
	}
    }
    RETURN;
}

/* Associative arrays. */

PP(pp_each)
{
    dSP;
    HV * hash = MUTABLE_HV(POPs);
    HE *entry;
    const U8 gimme = GIMME_V;

    entry = hv_iternext(hash);

    EXTEND(SP, 2);
    if (entry) {
	SV* const sv = hv_iterkeysv(entry);
	PUSHs(sv);
	if (gimme == G_ARRAY) {
	    SV *val;
	    val = hv_iterval(hash, entry);
	    PUSHs(val);
	}
    }
    else if (gimme == G_SCALAR)
	RETPUSHUNDEF;

    RETURN;
}

STATIC OP *
S_do_delete_local(pTHX)
{
    dSP;
    const U8 gimme = GIMME_V;
    const MAGIC *mg;
    HV *stash;
    const bool sliced = !!(PL_op->op_private & OPpSLICE);
    SV **unsliced_keysv = sliced ? NULL : sp--;
    SV * const osv = POPs;
    SV **mark = sliced ? PL_stack_base + POPMARK : unsliced_keysv-1;
    dORIGMARK;
    const bool tied = SvRMAGICAL(osv)
			    && mg_find((const SV *)osv, PERL_MAGIC_tied);
    const bool can_preserve = SvCANEXISTDELETE(osv);
    const U32 type = SvTYPE(osv);
    SV ** const end = sliced ? SP : unsliced_keysv;

    if (type == SVt_PVHV) {			/* hash element */
	    HV * const hv = MUTABLE_HV(osv);
	    while (++MARK <= end) {
		SV * const keysv = *MARK;
		SV *sv = NULL;
		bool preeminent = TRUE;
		if (can_preserve)
		    preeminent = hv_exists_ent(hv, keysv, 0);
		if (tied) {
		    HE *he = hv_fetch_ent(hv, keysv, 1, 0);
		    if (he)
			sv = HeVAL(he);
		    else
			preeminent = FALSE;
		}
		else {
		    sv = hv_delete_ent(hv, keysv, 0, 0);
		    if (preeminent)
			SvREFCNT_inc_simple_void(sv); /* De-mortalize */
		}
		if (preeminent) {
		    if (!sv) DIE(aTHX_ PL_no_helem_sv, SVfARG(keysv));
		    save_helem_flags(hv, keysv, &sv, SAVEf_KEEPOLDELEM);
		    if (tied) {
			*MARK = sv_mortalcopy(sv);
			mg_clear(sv);
		    } else
			*MARK = sv;
		}
		else {
		    SAVEHDELETE(hv, keysv);
		    *MARK = &PL_sv_undef;
		}
	    }
    }
    else if (type == SVt_PVAV) {                  /* array element */
	    if (PL_op->op_flags & OPf_SPECIAL) {
		AV * const av = MUTABLE_AV(osv);
		while (++MARK <= end) {
		    SSize_t idx = SvIV(*MARK);
		    SV *sv = NULL;
		    bool preeminent = TRUE;
		    if (can_preserve)
			preeminent = av_exists(av, idx);
		    if (tied) {
			SV **svp = av_fetch(av, idx, 1);
			if (svp)
			    sv = *svp;
			else
			    preeminent = FALSE;
		    }
		    else {
			sv = av_delete(av, idx, 0);
			if (preeminent)
			   SvREFCNT_inc_simple_void(sv); /* De-mortalize */
		    }
		    if (preeminent) {
		        save_aelem_flags(av, idx, &sv, SAVEf_KEEPOLDELEM);
			if (tied) {
			    *MARK = sv_mortalcopy(sv);
			    mg_clear(sv);
			} else
			    *MARK = sv;
		    }
		    else {
		        SAVEADELETE(av, idx);
		        *MARK = &PL_sv_undef;
		    }
		}
	    }
	    else
		DIE(aTHX_ "panic: avhv_delete no longer supported");
    }
    else
	    DIE(aTHX_ "Not a HASH reference");
    if (sliced) {
	if (gimme == G_VOID)
	    SP = ORIGMARK;
	else if (gimme == G_SCALAR) {
	    MARK = ORIGMARK;
	    if (SP > MARK)
		*++MARK = *SP;
	    else
		*++MARK = &PL_sv_undef;
	    SP = MARK;
	}
    }
    else if (gimme != G_VOID)
	PUSHs(*unsliced_keysv);

    RETURN;
}

PP(pp_delete)
{
    dSP;
    U8 gimme;
    I32 discard;

    if (PL_op->op_private & OPpLVAL_INTRO)
	return do_delete_local();

    gimme = GIMME_V;
    discard = (gimme == G_VOID) ? G_DISCARD : 0;

    if (PL_op->op_private & OPpSLICE) {
	dMARK; dORIGMARK;
	HV * const hv = MUTABLE_HV(POPs);
	const U32 hvtype = SvTYPE(hv);
	if (hvtype == SVt_PVHV) {			/* hash element */
	    while (++MARK <= SP) {
		SV * const sv = hv_delete_ent(hv, *MARK, discard, 0);
		*MARK = sv ? sv : &PL_sv_undef;
	    }
	}
	else if (hvtype == SVt_PVAV) {                  /* array element */
            if (PL_op->op_flags & OPf_SPECIAL) {
                while (++MARK <= SP) {
                    SV * const sv = av_delete(MUTABLE_AV(hv), SvIV(*MARK), discard);
                    *MARK = sv ? sv : &PL_sv_undef;
                }
            }
	}
	else
	    DIE(aTHX_ "Not a HASH reference");
	if (discard)
	    SP = ORIGMARK;
	else if (gimme == G_SCALAR) {
	    MARK = ORIGMARK;
	    if (SP > MARK)
		*++MARK = *SP;
	    else
		*++MARK = &PL_sv_undef;
	    SP = MARK;
	}
    }
    else {
	SV *keysv = POPs;
	HV * const hv = MUTABLE_HV(POPs);
	SV *sv = NULL;
	if (SvTYPE(hv) == SVt_PVHV)
	    sv = hv_delete_ent(hv, keysv, discard, 0);
	else if (SvTYPE(hv) == SVt_PVAV) {
	    if (PL_op->op_flags & OPf_SPECIAL)
		sv = av_delete(MUTABLE_AV(hv), SvIV(keysv), discard);
	    else
		DIE(aTHX_ "panic: avhv_delete no longer supported");
	}
	else
	    DIE(aTHX_ "Not a HASH reference");
	if (!sv)
	    sv = &PL_sv_undef;
	if (!discard)
	    PUSHs(sv);
    }
    RETURN;
}

PP(pp_exists)
{
    dSP;
    SV *tmpsv;
    HV *hv;

    if (UNLIKELY( PL_op->op_private & OPpEXISTS_SUB )) {
	GV *gv;
	SV * const sv = POPs;
	CV * const cv = sv_2cv(sv, &hv, &gv, 0);
	if (cv)
	    RETPUSHYES;
	if (gv && isGV(gv) && GvCV(gv) && !GvCVGEN(gv))
	    RETPUSHYES;
	RETPUSHNO;
    }
    tmpsv = POPs;
    hv = MUTABLE_HV(POPs);
    if (LIKELY( SvTYPE(hv) == SVt_PVHV )) {
	if (hv_exists_ent(hv, tmpsv, 0))
	    RETPUSHYES;
    }
    else if (SvTYPE(hv) == SVt_PVAV) {
	if (PL_op->op_flags & OPf_SPECIAL) {		/* array element */
	    if (av_exists(MUTABLE_AV(hv), SvIV(tmpsv)))
		RETPUSHYES;
	}
    }
    else {
	DIE(aTHX_ "Not a HASH reference");
    }
    RETPUSHNO;
}

PP(pp_hslice)
{
    dSP; dMARK; dORIGMARK;
    HV * const hv = MUTABLE_HV(POPs);
    const I32 lval = (PL_op->op_flags & OPf_MOD || LVRET);
    const bool localizing = PL_op->op_private & OPpLVAL_INTRO;
    bool can_preserve = FALSE;

    if (localizing) {
        MAGIC *mg;
        HV *stash;

	if (SvCANEXISTDELETE(hv))
	    can_preserve = TRUE;
    }

    while (++MARK <= SP) {
        SV * const keysv = *MARK;
        SV **svp;
        HE *he;
        bool preeminent = TRUE;

        if (localizing && can_preserve) {
	    /* If we can determine whether the element exist,
             * try to preserve the existenceness of a tied hash
             * element by using EXISTS and DELETE if possible.
             * Fallback to FETCH and STORE otherwise. */
            preeminent = hv_exists_ent(hv, keysv, 0);
        }

        he = hv_fetch_ent(hv, keysv, lval, 0);
        svp = he ? &HeVAL(he) : NULL;

        if (lval) {
            if (!svp || !*svp || *svp == &PL_sv_undef) {
                DIE(aTHX_ PL_no_helem_sv, SVfARG(keysv));
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
        }
        *MARK = svp && *svp ? *svp : &PL_sv_undef;
    }
    if (GIMME_V != G_ARRAY) {
	MARK = ORIGMARK;
	*++MARK = SP > ORIGMARK ? *SP : &PL_sv_undef;
	SP = MARK;
    }
    RETURN;
}

PP(pp_kvhslice)
{
    dSP; dMARK;
    HV * const hv = MUTABLE_HV(POPs);
    I32 lval = (PL_op->op_flags & OPf_MOD);
    SSize_t items = SP - MARK;

    if (PL_op->op_private & OPpMAYBE_LVSUB) {
       const I32 flags = is_lvalue_sub();
       if (flags) {
           if (!(flags & OPpENTERSUB_INARGS))
               /* diag_listed_as: Can't modify %s in %s */
	       Perl_croak(aTHX_ "Can't modify key/value hash slice in list assignment");
	   lval = flags;
       }
    }

    MEXTEND(SP,items);
    while (items > 1) {
	*(MARK+items*2-1) = *(MARK+items);
	items--;
    }
    items = SP-MARK;
    SP += items;

    while (++MARK <= SP) {
        SV * const keysv = *MARK;
        SV **svp;
        HE *he;

        he = hv_fetch_ent(hv, keysv, lval, 0);
        svp = he ? &HeVAL(he) : NULL;

        if (lval) {
            if (!svp || !*svp || *svp == &PL_sv_undef) {
                DIE(aTHX_ PL_no_helem_sv, SVfARG(keysv));
            }
	    *MARK = sv_mortalcopy(*MARK);
        }
        *++MARK = svp && *svp ? *svp : &PL_sv_undef;
    }
    if (GIMME_V != G_ARRAY) {
	MARK = SP - items*2;
	*++MARK = items > 0 ? *SP : &PL_sv_undef;
	SP = MARK;
    }
    RETURN;
}

/* List operators. */

PP(pp_list)
{
    I32 markidx = POPMARK;
    if (GIMME_V != G_ARRAY) {
	SV **mark = PL_stack_base + markidx;
	dSP;
	if (++MARK <= SP)
	    *MARK = *SP;		/* unwanted list, return last item */
	else
	    *MARK = &PL_sv_undef;
	SP = MARK;
	PUTBACK;
    }
    return NORMAL;
}

PP(pp_lslice)
{
    dSP;
    SV ** const lastrelem = PL_stack_sp;
    SV ** const lastlelem = PL_stack_base + POPMARK;
    SV ** const firstlelem = PL_stack_base + POPMARK + 1;
    SV ** const firstrelem = lastlelem + 1;
    const U8 mod = PL_op->op_flags & OPf_MOD;

    const I32 max = lastrelem - lastlelem;
    SV **lelem;

    if (GIMME_V != G_ARRAY) {
        if (lastlelem < firstlelem) {
            *firstlelem = &PL_sv_undef;
        }
        else {
            I32 ix = SvIV(*lastlelem);
            if (ix < 0)
                ix += max;
            if (ix < 0 || ix >= max)
                *firstlelem = &PL_sv_undef;
            else
                *firstlelem = firstrelem[ix];
        }
        SP = firstlelem;
        RETURN;
    }

    if (max == 0) {
	SP = firstlelem - 1;
	RETURN;
    }

    for (lelem = firstlelem; lelem <= lastlelem; lelem++) {
	I32 ix = SvIV(*lelem);
	if (ix < 0)
	    ix += max;
	if (ix < 0 || ix >= max)
	    *lelem = &PL_sv_undef;
	else {
	    if (!(*lelem = firstrelem[ix]))
		*lelem = &PL_sv_undef;
	    else if (mod && SvPADTMP(*lelem)) {
		*lelem = firstrelem[ix] = sv_mortalcopy(*lelem);
            }
	}
    }
    SP = lastlelem;
    RETURN;
}

PP(pp_anonlist)
{
    dSP; dMARK;
    const I32 items = SP - MARK;
    SV * const av = MUTABLE_SV(av_make(items, MARK+1));
    SP = MARK;
    mXPUSHs((PL_op->op_flags & OPf_SPECIAL)
	    ? newRV_noinc(av) : av);
    RETURN;
}

PP(pp_anonhash)
{
    dSP; dMARK; dORIGMARK;
    HV* const hv = newHV();
    SV* const retval = sv_2mortal( PL_op->op_flags & OPf_SPECIAL
                                    ? newRV_noinc(MUTABLE_SV(hv))
                                    : MUTABLE_SV(hv) );

    while (MARK < SP) {
	SV * const key =
	    (MARK++, SvGMAGICAL(*MARK) ? sv_mortalcopy(*MARK) : *MARK);
	SV *val;
	if (MARK < SP)
	{
	    MARK++;
	    SvGETMAGIC(*MARK);
	    val = newSV(0);
	    sv_setsv_nomg(val, *MARK);
	}
	else
	{
	    Perl_ck_warner(aTHX_ packWARN(WARN_MISC), "Odd number of elements in anonymous hash");
	    val = newSV(0);
	}
	(void)hv_store_ent(hv,key,val,0);
    }
    SP = ORIGMARK;
    XPUSHs(retval);
    RETURN;
}

static AV *
S_deref_plain_array(pTHX_ AV *ary)
{
    if (SvTYPE(ary) == SVt_PVAV) return ary;
    SvGETMAGIC((SV *)ary);
    if (!SvROK(ary) || SvTYPE(SvRV(ary)) != SVt_PVAV)
	Perl_die(aTHX_ "Not an ARRAY reference");
    else if (SvOBJECT(SvRV(ary)))
	Perl_die(aTHX_ "Not an unblessed ARRAY reference");
    return (AV *)SvRV(ary);
}

#if defined(__GNUC__) && !defined(PERL_GCC_BRACE_GROUPS_FORBIDDEN)
# define DEREF_PLAIN_ARRAY(ary)       \
   ({                                  \
     AV *aRrRay = ary;                  \
     SvTYPE(aRrRay) == SVt_PVAV          \
      ? aRrRay                            \
      : S_deref_plain_array(aTHX_ aRrRay); \
   })
#else
# define DEREF_PLAIN_ARRAY(ary)            \
   (                                        \
     PL_Sv = (SV *)(ary),                    \
     SvTYPE(PL_Sv) == SVt_PVAV                \
      ? (AV *)PL_Sv                            \
      : S_deref_plain_array(aTHX_ (AV *)PL_Sv)  \
   )
#endif

PP(pp_splice)
{
    dSP; dMARK; dORIGMARK;
    int num_args = (SP - MARK);
    AV *ary = DEREF_PLAIN_ARRAY(MUTABLE_AV(*++MARK));
    SV **src;
    SV **dst;
    SSize_t i;
    SSize_t offset;
    SSize_t length;
    SSize_t newlen;
    SSize_t after;
    SSize_t diff;
    const MAGIC * const mg = SvTIED_mg((const SV *)ary, PERL_MAGIC_tied);

    if (mg) {
	return Perl_tied_method(aTHX_ SV_CONST(SPLICE), mark - 1, MUTABLE_SV(ary), mg,
				    GIMME_V | TIED_METHOD_ARGUMENTS_ON_STACK,
				    sp - mark);
    }

    SP++;

    if (++MARK < SP) {
	offset = i = SvIV(*MARK);
	if (offset < 0)
	    offset += AvFILLp(ary) + 1;
	if (offset < 0)
	    DIE(aTHX_ PL_no_aelem, i);
	if (++MARK < SP) {
	    length = SvIVx(*MARK++);
	    if (length < 0) {
		length += AvFILLp(ary) - offset + 1;
		if (length < 0)
		    length = 0;
	    }
	}
	else
	    length = AvMAX(ary) + 1;		/* close enough to infinity */
    }
    else {
	offset = 0;
	length = AvMAX(ary) + 1;
    }
    if (offset > AvFILLp(ary) + 1) {
	if (num_args > 2)
	    Perl_ck_warner(aTHX_ packWARN(WARN_MISC), "splice() offset past end of array" );
	offset = AvFILLp(ary) + 1;
    }
    after = AvFILLp(ary) + 1 - (offset + length);
    if (after < 0) {				/* not that much array */
	length += after;			/* offset+length now in array */
	after = 0;
	if (!AvALLOC(ary))
	    av_extend(ary, 0);
    }

    /* At this point, MARK .. SP-1 is our new LIST */

    newlen = SP - MARK;
    diff = newlen - length;
    if (newlen && !AvREAL(ary) && AvREIFY(ary))
	av_reify(ary);

    /* make new elements SVs now: avoid problems if they're from the array */
    for (dst = MARK, i = newlen; i; i--) {
        SV * const h = *dst;
	*dst++ = newSVsv(h);
    }

    if (diff < 0) {				/* shrinking the area */
	SV **tmparyval = NULL;
	if (newlen) {
	    Newx(tmparyval, newlen, SV*);	/* so remember insertion */
	    Copy(MARK, tmparyval, newlen, SV*);
	}

	MARK = ORIGMARK + 1;
	if (GIMME_V == G_ARRAY) {		/* copy return vals to stack */
	    const bool real = cBOOL(AvREAL(ary));
	    MEXTEND(MARK, length);
	    if (real)
		EXTEND_MORTAL(length);
	    for (i = 0, dst = MARK; i < length; i++) {
		if ((*dst = AvARRAY(ary)[i+offset])) {
		  if (real)
		    sv_2mortal(*dst);	/* free them eventually */
		}
		else
		    *dst = &PL_sv_undef;
		dst++;
	    }
	    MARK += length - 1;
	}
	else {
	    *MARK = AvARRAY(ary)[offset+length-1];
	    if (AvREAL(ary)) {
		sv_2mortal(*MARK);
		for (i = length - 1, dst = &AvARRAY(ary)[offset]; i > 0; i--)
		    SvREFCNT_dec(*dst++);	/* free them now */
	    }
	}
	AvFILLp(ary) += diff;

	/* pull up or down? */

	if (offset < after) {			/* easier to pull up */
	    if (offset) {			/* esp. if nothing to pull */
		src = &AvARRAY(ary)[offset-1];
		dst = src - diff;		/* diff is negative */
		for (i = offset; i > 0; i--)	/* can't trust Copy */
		    *dst-- = *src--;
	    }
	    dst = AvARRAY(ary);
	    AvARRAY(ary) = AvARRAY(ary) - diff; /* diff is negative */
	    AvMAX(ary) += diff;
	}
	else {
	    if (after) {			/* anything to pull down? */
		src = AvARRAY(ary) + offset + length;
		dst = src + diff;		/* diff is negative */
		Move(src, dst, after, SV*);
	    }
	    dst = &AvARRAY(ary)[AvFILLp(ary)+1];
						/* avoid later double free */
	}
	i = -diff;
	while (i)
	    dst[--i] = NULL;
	
	if (newlen) {
 	    Copy( tmparyval, AvARRAY(ary) + offset, newlen, SV* );
	    Safefree(tmparyval);
	}
    }
    else {					/* no, expanding (or same) */
	SV** tmparyval = NULL;
	if (length) {
	    Newx(tmparyval, length, SV*);	/* so remember deletion */
	    Copy(AvARRAY(ary)+offset, tmparyval, length, SV*);
	}

	if (diff > 0) {				/* expanding */
	    /* push up or down? */
	    if (offset < after && diff <= AvARRAY(ary) - AvALLOC(ary)) {
		if (offset) {
		    src = AvARRAY(ary);
		    dst = src - diff;
		    Move(src, dst, offset, SV*);
		}
		AvARRAY(ary) = AvARRAY(ary) - diff;/* diff is positive */
		AvMAX(ary) += diff;
		AvFILLp(ary) += diff;
	    }
	    else {
		if (AvFILLp(ary) + diff >= AvMAX(ary))	/* oh, well */
		    av_extend(ary, AvFILLp(ary) + diff);
		AvFILLp(ary) += diff;

		if (after) {
		    dst = AvARRAY(ary) + AvFILLp(ary);
		    src = dst - diff;
		    for (i = after; i; i--) {
			*dst-- = *src--;
		    }
		}
	    }
	}

	if (newlen) {
	    Copy( MARK, AvARRAY(ary) + offset, newlen, SV* );
	}

	MARK = ORIGMARK + 1;
	if (GIMME_V == G_ARRAY) {		/* copy return vals to stack */
	    if (length) {
		const bool real = cBOOL(AvREAL(ary));
		if (real)
		    EXTEND_MORTAL(length);
		for (i = 0, dst = MARK; i < length; i++) {
		    if ((*dst = tmparyval[i])) {
		      if (real)
			sv_2mortal(*dst);	/* free them eventually */
		    }
		    else *dst = &PL_sv_undef;
		    dst++;
		}
	    }
	    MARK += length - 1;
	}
	else if (length--) {
	    *MARK = tmparyval[length];
	    if (AvREAL(ary)) {
		sv_2mortal(*MARK);
		while (length-- > 0)
		    SvREFCNT_dec(tmparyval[length]);
	    }
	}
	else
	    *MARK = &PL_sv_undef;
	Safefree(tmparyval);
    }

    if (SvMAGICAL(ary))
	mg_set(MUTABLE_SV(ary));

    SP = MARK;
    RETURN;
}

PP(pp_push)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    AV * const ary = DEREF_PLAIN_ARRAY(MUTABLE_AV(*++MARK));
    const MAGIC * const mg = SvTIED_mg((const SV *)ary, PERL_MAGIC_tied);

    if (mg) {
	*MARK-- = SvTIED_obj(MUTABLE_SV(ary), mg);
	PUSHMARK(MARK);
	PUTBACK;
	ENTER_with_name("call_PUSH");
	call_sv(SV_CONST(PUSH),G_SCALAR|G_DISCARD|G_METHOD_NAMED);
	LEAVE_with_name("call_PUSH");
	/* SPAGAIN; not needed: SP is assigned to immediately below */
    }
    else {
        /* PL_delaymagic is restored by JUMPENV_POP on dieing, so we
         * only need to save locally, not on the save stack */
        U16 old_delaymagic = PL_delaymagic;

	if (SvREADONLY(ary) && MARK < SP) Perl_croak_no_modify();
	PL_delaymagic = DM_DELAY;
	for (++MARK; MARK <= SP; MARK++) {
	    SV *sv;
	    if (*MARK) SvGETMAGIC(*MARK);
	    sv = newSV(0);
	    if (*MARK)
		sv_setsv_nomg(sv, *MARK);
	    av_store(ary, AvFILLp(ary)+1, sv);
	}
	if (PL_delaymagic & DM_ARRAY_ISA)
	    mg_set(MUTABLE_SV(ary));
        PL_delaymagic = old_delaymagic;
    }
    SP = ORIGMARK;
    if (OP_GIMME(PL_op, 0) != G_VOID) {
	PUSHi( AvFILL(ary) + 1 );
    }
    RETURN;
}

/* also used for: pp_pop()*/
PP(pp_shift)
{
    dSP;
    AV * const av = PL_op->op_flags & OPf_SPECIAL
	? MUTABLE_AV(GvAV(PL_defgv)) : DEREF_PLAIN_ARRAY(MUTABLE_AV(POPs));
    SV * const sv = PL_op->op_type == OP_SHIFT ? av_shift(av) : av_pop(av);
    EXTEND(SP, 1);
    assert (sv);
    if (AvREAL(av))
	(void)sv_2mortal(sv);
    PUSHs(sv);
    RETURN;
}

PP(pp_unshift)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    AV *ary = DEREF_PLAIN_ARRAY(MUTABLE_AV(*++MARK));
    const MAGIC * const mg = SvTIED_mg((const SV *)ary, PERL_MAGIC_tied);

    if (mg) {
	*MARK-- = SvTIED_obj(MUTABLE_SV(ary), mg);
	PUSHMARK(MARK);
	PUTBACK;
	ENTER_with_name("call_UNSHIFT");
	call_sv(SV_CONST(UNSHIFT),G_SCALAR|G_DISCARD|G_METHOD_NAMED);
	LEAVE_with_name("call_UNSHIFT");
	/* SPAGAIN; not needed: SP is assigned to immediately below */
    }
    else {
        /* PL_delaymagic is restored by JUMPENV_POP on dieing, so we
         * only need to save locally, not on the save stack */
        U16 old_delaymagic = PL_delaymagic;
	SSize_t i = 0;

	av_unshift(ary, SP - MARK);
        PL_delaymagic = DM_DELAY;
	while (MARK < SP) {
	    SV * const sv = newSVsv(*++MARK);
	    (void)av_store(ary, i++, sv);
	}
        if (PL_delaymagic & DM_ARRAY_ISA)
            mg_set(MUTABLE_SV(ary));
        PL_delaymagic = old_delaymagic;
    }
    SP = ORIGMARK;
    if (OP_GIMME(PL_op, 0) != G_VOID) {
	PUSHi( AvFILL(ary) + 1 );
    }
    RETURN;
}

PP(pp_reverse)
{
    dSP; dMARK;

    if (GIMME_V == G_ARRAY) {
	if (PL_op->op_private & OPpREVERSE_INPLACE) {
	    AV *av;

	    /* See pp_sort() */
	    assert( MARK+1 == SP && *SP && SvTYPE(*SP) == SVt_PVAV);
	    (void)POPMARK; /* remove mark associated with ex-OP_AASSIGN */
	    av = MUTABLE_AV((*SP));
	    /* In-place reversing only happens in void context for the array
	     * assignment. We don't need to push anything on the stack. */
	    SP = MARK;

	    if (SvMAGICAL(av)) {
		SSize_t i, j;
		SV *tmp = sv_newmortal();
		/* For SvCANEXISTDELETE */
		HV *stash;
		const MAGIC *mg;
		bool can_preserve = SvCANEXISTDELETE(av);

		for (i = 0, j = av_tindex(av); i < j; ++i, --j) {
		    SV *begin, *end;

		    if (can_preserve) {
			if (!av_exists(av, i)) {
			    if (av_exists(av, j)) {
				SV *sv = av_delete(av, j, 0);
				begin = *av_fetch(av, i, TRUE);
				sv_setsv_mg(begin, sv);
			    }
			    continue;
			}
			else if (!av_exists(av, j)) {
			    SV *sv = av_delete(av, i, 0);
			    end = *av_fetch(av, j, TRUE);
			    sv_setsv_mg(end, sv);
			    continue;
			}
		    }

		    begin = *av_fetch(av, i, TRUE);
		    end   = *av_fetch(av, j, TRUE);
		    sv_setsv(tmp,      begin);
		    sv_setsv_mg(begin, end);
		    sv_setsv_mg(end,   tmp);
		}
	    }
	    else {
		SV **begin = AvARRAY(av);

		if (begin) {
		    SV **end   = begin + AvFILLp(av);

		    while (begin < end) {
			SV * const tmp = *begin;
			*begin++ = *end;
			*end--   = tmp;
		    }
		}
	    }
	}
	else {
	    SV **oldsp = SP;
	    MARK++;
	    while (MARK < SP) {
		SV * const tmp = *MARK;
		*MARK++ = *SP;
		*SP--   = tmp;
	    }
	    /* safe as long as stack cannot get extended in the above */
	    SP = oldsp;
	}
    }
    else {
	char *up;
	char *down;
	I32 tmp;
	dTARGET;
	STRLEN len;

	SvUTF8_off(TARG);				/* decontaminate */
	if (SP - MARK > 1)
	    do_join(TARG, &PL_sv_no, MARK, SP);
	else {
	    sv_setsv(TARG, SP > MARK ? *SP : DEFSV);
	}

	up = SvPV_force(TARG, len);
	if (len > 1) {
	    if (DO_UTF8(TARG)) {	/* first reverse each character */
		U8* s = (U8*)SvPVX(TARG);
		const U8* send = (U8*)(s + len);
		while (s < send) {
		    if (UTF8_IS_INVARIANT(*s)) {
			s++;
			continue;
		    }
		    else {
			if (!utf8_to_uvchr_buf(s, send, 0))
			    break;
			up = (char*)s;
			s += UTF8SKIP(s);
			down = (char*)(s - 1);
			/* reverse this character */
			while (down > up) {
			    tmp = *up;
			    *up++ = *down;
			    *down-- = (char)tmp;
			}
		    }
		}
		up = SvPVX(TARG);
	    }
	    down = SvPVX(TARG) + len - 1;
	    while (down > up) {
		tmp = *up;
		*up++ = *down;
		*down-- = (char)tmp;
	    }
	    (void)SvPOK_only_UTF8(TARG);
	}
	SP = MARK + 1;
	SETTARG;
    }
    RETURN;
}

PP(pp_split)
{
    dSP; dTARG;
    AV *ary = PL_op->op_flags & OPf_STACKED ? (AV *)POPs : NULL;
    IV limit = POPi;			/* note, negative is forever */
    SV * const sv = POPs;
    STRLEN len;
    const char *s = SvPV_const(sv, len);
    const bool do_utf8 = DO_UTF8(sv);
    const char *strend = s + len;
    PMOP *pm;
    REGEXP *rx;
    SV *dstr;
    const char *m;
    SSize_t iters = 0;
    const STRLEN slen = do_utf8
                        ? utf8_length((U8*)s, (U8*)strend)
                        : (STRLEN)(strend - s);
    SSize_t maxiters = slen + 10;
    I32 trailing_empty = 0;
    const char *orig;
    const IV origlimit = limit;
    I32 realarray = 0;
    I32 base;
    const U8 gimme = GIMME_V;
    bool gimme_scalar;
    const I32 oldsave = PL_savestack_ix;
    U32 make_mortal = SVs_TEMP;
    bool multiline = 0;
    MAGIC *mg = NULL;

#ifdef DEBUGGING
    Copy(&LvTARGOFF(POPs), &pm, 1, PMOP*);
#else
    pm = (PMOP*)POPs;
#endif
    if (!pm)
	DIE(aTHX_ "panic: pp_split, pm=%p, s=%p", pm, s);
    rx = PM_GETRE(pm);

    TAINT_IF(get_regex_charset(RX_EXTFLAGS(rx)) == REGEX_LOCALE_CHARSET &&
             (RX_EXTFLAGS(rx) & (RXf_WHITE | RXf_SKIPWHITE)));

#ifdef USE_ITHREADS
    if (pm->op_pmreplrootu.op_pmtargetoff) {
	ary = GvAVn(MUTABLE_GV(PAD_SVl(pm->op_pmreplrootu.op_pmtargetoff)));
	goto have_av;
    }
#else
    if (pm->op_pmreplrootu.op_pmtargetgv) {
	ary = GvAVn(pm->op_pmreplrootu.op_pmtargetgv);
	goto have_av;
    }
#endif
    else if (pm->op_targ)
	ary = (AV *)PAD_SVl(pm->op_targ);
    if (ary) {
	have_av:
	realarray = 1;
	PUTBACK;
	av_extend(ary,0);
	(void)sv_2mortal(SvREFCNT_inc_simple_NN(sv));
	av_clear(ary);
	SPAGAIN;
	if ((mg = SvTIED_mg((const SV *)ary, PERL_MAGIC_tied))) {
	    PUSHMARK(SP);
	    XPUSHs(SvTIED_obj(MUTABLE_SV(ary), mg));
	}
	else {
	    if (!AvREAL(ary)) {
		I32 i;
		AvREAL_on(ary);
		AvREIFY_off(ary);
		for (i = AvFILLp(ary); i >= 0; i--)
		    AvARRAY(ary)[i] = &PL_sv_undef; /* don't free mere refs */
	    }
	    /* temporarily switch stacks */
	    SAVESWITCHSTACK(PL_curstack, ary);
	    make_mortal = 0;
	}
    }
    base = SP - PL_stack_base;
    orig = s;
    if (RX_EXTFLAGS(rx) & RXf_SKIPWHITE) {
	if (do_utf8) {
	    while (isSPACE_utf8(s))
		s += UTF8SKIP(s);
	}
	else if (get_regex_charset(RX_EXTFLAGS(rx)) == REGEX_LOCALE_CHARSET) {
	    while (isSPACE_LC(*s))
		s++;
	}
	else {
	    while (isSPACE(*s))
		s++;
	}
    }
    if (RX_EXTFLAGS(rx) & RXf_PMf_MULTILINE) {
	multiline = 1;
    }

    gimme_scalar = gimme == G_SCALAR && !ary;

    if (!limit)
	limit = maxiters + 2;
    if (RX_EXTFLAGS(rx) & RXf_WHITE) {
	while (--limit) {
	    m = s;
	    /* this one uses 'm' and is a negative test */
	    if (do_utf8) {
		while (m < strend && ! isSPACE_utf8(m) ) {
		    const int t = UTF8SKIP(m);
		    /* isSPACE_utf8 returns FALSE for malform utf8 */
		    if (strend - m < t)
			m = strend;
		    else
			m += t;
		}
	    }
	    else if (get_regex_charset(RX_EXTFLAGS(rx)) == REGEX_LOCALE_CHARSET)
            {
	        while (m < strend && !isSPACE_LC(*m))
		    ++m;
            } else {
                while (m < strend && !isSPACE(*m))
                    ++m;
            }  
	    if (m >= strend)
		break;

	    if (gimme_scalar) {
		iters++;
		if (m-s == 0)
		    trailing_empty++;
		else
		    trailing_empty = 0;
	    } else {
		dstr = newSVpvn_flags(s, m-s,
				      (do_utf8 ? SVf_UTF8 : 0) | make_mortal);
		XPUSHs(dstr);
	    }

	    /* skip the whitespace found last */
	    if (do_utf8)
		s = m + UTF8SKIP(m);
	    else
		s = m + 1;

	    /* this one uses 's' and is a positive test */
	    if (do_utf8) {
		while (s < strend && isSPACE_utf8(s) )
	            s +=  UTF8SKIP(s);
	    }
	    else if (get_regex_charset(RX_EXTFLAGS(rx)) == REGEX_LOCALE_CHARSET)
            {
	        while (s < strend && isSPACE_LC(*s))
		    ++s;
            } else {
                while (s < strend && isSPACE(*s))
                    ++s;
            } 	    
	}
    }
    else if (RX_EXTFLAGS(rx) & RXf_START_ONLY) {
	while (--limit) {
	    for (m = s; m < strend && *m != '\n'; m++)
		;
	    m++;
	    if (m >= strend)
		break;

	    if (gimme_scalar) {
		iters++;
		if (m-s == 0)
		    trailing_empty++;
		else
		    trailing_empty = 0;
	    } else {
		dstr = newSVpvn_flags(s, m-s,
				      (do_utf8 ? SVf_UTF8 : 0) | make_mortal);
		XPUSHs(dstr);
	    }
	    s = m;
	}
    }
    else if (RX_EXTFLAGS(rx) & RXf_NULL && !(s >= strend)) {
        /*
          Pre-extend the stack, either the number of bytes or
          characters in the string or a limited amount, triggered by:

          my ($x, $y) = split //, $str;
            or
          split //, $str, $i;
        */
	if (!gimme_scalar) {
	    const IV items = limit - 1;
            /* setting it to -1 will trigger a panic in EXTEND() */
            const SSize_t sslen = slen > SSize_t_MAX ?  -1 : (SSize_t)slen;
	    if (items >=0 && items < sslen)
		EXTEND(SP, items);
	    else
		EXTEND(SP, sslen);
	}

        if (do_utf8) {
            while (--limit) {
                /* keep track of how many bytes we skip over */
                m = s;
                s += UTF8SKIP(s);
		if (gimme_scalar) {
		    iters++;
		    if (s-m == 0)
			trailing_empty++;
		    else
			trailing_empty = 0;
		} else {
		    dstr = newSVpvn_flags(m, s-m, SVf_UTF8 | make_mortal);

		    PUSHs(dstr);
		}

                if (s >= strend)
                    break;
            }
        } else {
            while (--limit) {
	        if (gimme_scalar) {
		    iters++;
		} else {
		    dstr = newSVpvn(s, 1);


		    if (make_mortal)
			sv_2mortal(dstr);

		    PUSHs(dstr);
		}

                s++;

                if (s >= strend)
                    break;
            }
        }
    }
    else if (do_utf8 == (RX_UTF8(rx) != 0) &&
	     (RX_EXTFLAGS(rx) & RXf_USE_INTUIT) && !RX_NPARENS(rx)
	     && (RX_EXTFLAGS(rx) & RXf_CHECK_ALL)
             && !(RX_EXTFLAGS(rx) & RXf_IS_ANCHORED)) {
	const int tail = (RX_EXTFLAGS(rx) & RXf_INTUIT_TAIL);
	SV * const csv = CALLREG_INTUIT_STRING(rx);

	len = RX_MINLENRET(rx);
	if (len == 1 && !RX_UTF8(rx) && !tail) {
	    const char c = *SvPV_nolen_const(csv);
	    while (--limit) {
		for (m = s; m < strend && *m != c; m++)
		    ;
		if (m >= strend)
		    break;
		if (gimme_scalar) {
		    iters++;
		    if (m-s == 0)
			trailing_empty++;
		    else
			trailing_empty = 0;
		} else {
		    dstr = newSVpvn_flags(s, m-s,
					 (do_utf8 ? SVf_UTF8 : 0) | make_mortal);
		    XPUSHs(dstr);
		}
		/* The rx->minlen is in characters but we want to step
		 * s ahead by bytes. */
 		if (do_utf8)
		    s = (char*)utf8_hop((U8*)m, len);
 		else
		    s = m + len; /* Fake \n at the end */
	    }
	}
	else {
	    while (s < strend && --limit &&
	      (m = fbm_instr((unsigned char*)s, (unsigned char*)strend,
			     csv, multiline ? FBMrf_MULTILINE : 0)) )
	    {
		if (gimme_scalar) {
		    iters++;
		    if (m-s == 0)
			trailing_empty++;
		    else
			trailing_empty = 0;
		} else {
		    dstr = newSVpvn_flags(s, m-s,
					 (do_utf8 ? SVf_UTF8 : 0) | make_mortal);
		    XPUSHs(dstr);
		}
		/* The rx->minlen is in characters but we want to step
		 * s ahead by bytes. */
 		if (do_utf8)
		    s = (char*)utf8_hop((U8*)m, len);
 		else
		    s = m + len; /* Fake \n at the end */
	    }
	}
    }
    else {
	maxiters += slen * RX_NPARENS(rx);
	while (s < strend && --limit)
	{
	    I32 rex_return;
	    PUTBACK;
	    rex_return = CALLREGEXEC(rx, (char*)s, (char*)strend, (char*)orig, 1,
				     sv, NULL, 0);
	    SPAGAIN;
	    if (rex_return == 0)
		break;
	    TAINT_IF(RX_MATCH_TAINTED(rx));
            /* we never pass the REXEC_COPY_STR flag, so it should
             * never get copied */
            assert(!RX_MATCH_COPIED(rx));
	    m = RX_OFFS(rx)[0].start + orig;

	    if (gimme_scalar) {
		iters++;
		if (m-s == 0)
		    trailing_empty++;
		else
		    trailing_empty = 0;
	    } else {
		dstr = newSVpvn_flags(s, m-s,
				      (do_utf8 ? SVf_UTF8 : 0) | make_mortal);
		XPUSHs(dstr);
	    }
	    if (RX_NPARENS(rx)) {
		I32 i;
		for (i = 1; i <= (I32)RX_NPARENS(rx); i++) {
		    s = RX_OFFS(rx)[i].start + orig;
		    m = RX_OFFS(rx)[i].end + orig;

		    /* japhy (07/27/01) -- the (m && s) test doesn't catch
		       parens that didn't match -- they should be set to
		       undef, not the empty string */
		    if (gimme_scalar) {
			iters++;
			if (m-s == 0)
			    trailing_empty++;
			else
			    trailing_empty = 0;
		    } else {
			if (m >= orig && s >= orig) {
			    dstr = newSVpvn_flags(s, m-s,
						 (do_utf8 ? SVf_UTF8 : 0)
						  | make_mortal);
			}
			else
			    dstr = &PL_sv_undef;  /* undef, not "" */
			XPUSHs(dstr);
		    }

		}
	    }
	    s = RX_OFFS(rx)[0].end + orig;
	}
    }

    if (!gimme_scalar) {
	iters = (SP - PL_stack_base) - base;
    }
    if (iters > maxiters)
	DIE(aTHX_ "Split loop");

    /* keep field after final delim? */
    if (s < strend || (iters && origlimit)) {
	if (!gimme_scalar) {
	    const STRLEN l = strend - s;
	    dstr = newSVpvn_flags(s, l, (do_utf8 ? SVf_UTF8 : 0) | make_mortal);
	    XPUSHs(dstr);
	}
	iters++;
    }
    else if (!origlimit) {
	if (gimme_scalar) {
	    iters -= trailing_empty;
	} else {
	    while (iters > 0 && (!TOPs || !SvANY(TOPs) || SvCUR(TOPs) == 0)) {
		if (TOPs && !make_mortal)
		    sv_2mortal(TOPs);
		*SP-- = &PL_sv_undef;
		iters--;
	    }
	}
    }

    PUTBACK;
    LEAVE_SCOPE(oldsave); /* may undo an earlier SWITCHSTACK */
    SPAGAIN;
    if (realarray) {
	if (!mg) {
	    if (SvSMAGICAL(ary)) {
		PUTBACK;
		mg_set(MUTABLE_SV(ary));
		SPAGAIN;
	    }
	    if (gimme == G_ARRAY) {
		EXTEND(SP, iters);
		Copy(AvARRAY(ary), SP + 1, iters, SV*);
		SP += iters;
		RETURN;
	    }
	}
	else {
	    PUTBACK;
	    ENTER_with_name("call_PUSH");
	    call_sv(SV_CONST(PUSH),G_SCALAR|G_DISCARD|G_METHOD_NAMED);
	    LEAVE_with_name("call_PUSH");
	    SPAGAIN;
	    if (gimme == G_ARRAY) {
		SSize_t i;
		/* EXTEND should not be needed - we just popped them */
		EXTEND(SP, iters);
		for (i=0; i < iters; i++) {
		    SV **svp = av_fetch(ary, i, FALSE);
		    PUSHs((svp) ? *svp : &PL_sv_undef);
		}
		RETURN;
	    }
	}
    }
    else {
	if (gimme == G_ARRAY)
	    RETURN;
    }

    GETTARGET;
    PUSHi(iters);
    RETURN;
}

PP(pp_once)
{
    dSP;
    SV *const sv = PAD_SVl(PL_op->op_targ);

    if (SvPADSTALE(sv)) {
	/* First time. */
	SvPADSTALE_off(sv);
	RETURNOP(cLOGOP->op_other);
    }
    RETURNOP(cLOGOP->op_next);
}

PP(pp_lock)
{
    dSP;
    dTOPss;
    SV *retsv = sv;
    SvLOCK(sv);
    if (SvTYPE(retsv) == SVt_PVAV || SvTYPE(retsv) == SVt_PVHV
     || SvTYPE(retsv) == SVt_PVCV) {
	retsv = refto(retsv);
    }
    SETs(retsv);
    RETURN;
}


/* used for: pp_padany(), pp_mapstart(), pp_custom(); plus any system ops
 * that aren't implemented on a particular platform */

PP(unimplemented_op)
{
    const Optype op_type = PL_op->op_type;
    /* Using OP_NAME() isn't going to be helpful here. Firstly, it doesn't cope
       with out of range op numbers - it only "special" cases op_custom.
       Secondly, as the three ops we "panic" on are padmy, mapstart and custom,
       if we get here for a custom op then that means that the custom op didn't
       have an implementation. Given that OP_NAME() looks up the custom op
       by its pp_addr, likely it will return NULL, unless someone (unhelpfully)
       registers &PL_unimplemented_op as the address of their custom op.
       NULL doesn't generate a useful error message. "custom" does. */
    const char *const name = op_type >= OP_max
	? "[out of range]" : PL_op_name[PL_op->op_type];
    if(OP_IS_SOCKET(op_type))
	DIE(aTHX_ PL_no_sock_func, name);
    DIE(aTHX_ "panic: unimplemented op %s (#%d) called", name,	op_type);
}

/* For sorting out arguments passed to a &CORE:: subroutine */
PP(pp_coreargs)
{
    dSP;
    int opnum = SvIOK(cSVOP_sv) ? (int)SvUV(cSVOP_sv) : 0;
    int defgv = PL_opargs[opnum] & OA_DEFGV ||opnum==OP_GLOB, whicharg = 0;
    AV * const at_ = GvAV(PL_defgv);
    SV **svp = at_ ? AvARRAY(at_) : NULL;
    I32 minargs = 0, maxargs = 0, numargs = at_ ? AvFILLp(at_)+1 : 0;
    I32 oa = opnum ? PL_opargs[opnum] >> OASHIFT : 0;
    bool seen_question = 0;
    const char *err = NULL;
    const bool pushmark = PL_op->op_private & OPpCOREARGS_PUSHMARK;

    /* Count how many args there are first, to get some idea how far to
       extend the stack. */
    while (oa) {
	if ((oa & 7) == OA_LIST) { maxargs = I32_MAX; break; }
	maxargs++;
	if (oa & OA_OPTIONAL) seen_question = 1;
	if (!seen_question) minargs++;
	oa >>= 4;
    }

    if(numargs < minargs) err = "Not enough";
    else if(numargs > maxargs) err = "Too many";
    if (err)
	/* diag_listed_as: Too many arguments for %s */
	Perl_croak(aTHX_
	  "%s arguments for %s", err,
	   opnum ? PL_op_desc[opnum] : SvPV_nolen_const(cSVOP_sv)
	);

    /* Reset the stack pointer.  Without this, we end up returning our own
       arguments in list context, in addition to the values we are supposed
       to return.  nextstate usually does this on sub entry, but we need
       to run the next op with the caller's hints, so we cannot have a
       nextstate. */
    SP = PL_stack_base + CX_CUR()->blk_oldsp;

    if(!maxargs) RETURN;

    /* We do this here, rather than with a separate pushmark op, as it has
       to come in between two things this function does (stack reset and
       arg pushing).  This seems the easiest way to do it. */
    if (pushmark) {
	PUTBACK;
	(void)Perl_pp_pushmark(aTHX);
    }

    EXTEND(SP, maxargs == I32_MAX ? numargs : maxargs);
    PUTBACK; /* The code below can die in various places. */

    oa = PL_opargs[opnum] >> OASHIFT;
    for (; oa&&(numargs||!pushmark); (void)(numargs&&(++svp,--numargs))) {
	whicharg++;
	switch (oa & 7) {
	case OA_SCALAR:
	  try_defsv:
	    if (!numargs && defgv && whicharg == minargs + 1) {
		PUSHs(DEFSV);
	    }
	    else PUSHs(numargs ? svp && *svp ? *svp : &PL_sv_undef : NULL);
	    break;
	case OA_LIST:
	    while (numargs--) {
		PUSHs(svp && *svp ? *svp : &PL_sv_undef);
		svp++;
	    }
	    RETURN;
	case OA_HVREF:
	    if (!svp || !*svp || !SvROK(*svp)
	     || SvTYPE(SvRV(*svp)) != SVt_PVHV)
		DIE(aTHX_
		/* diag_listed_as: Type of arg %d to &CORE::%s must be %s*/
		 "Type of arg %d to &CORE::%s must be hash reference",
		  whicharg, OP_DESC(PL_op->op_next)
		);
	    PUSHs(SvRV(*svp));
	    break;
	case OA_FILEREF:
	    if (!numargs) PUSHs(NULL);
	    else if(svp && *svp && SvROK(*svp) && isGV_with_GP(SvRV(*svp)))
		/* no magic here, as the prototype will have added an extra
		   refgen and we just want what was there before that */
		PUSHs(SvRV(*svp));
	    else {
		const bool constr = PL_op->op_private & whicharg;
		PUSHs(S_rv2gv(aTHX_
		    svp && *svp ? *svp : &PL_sv_undef,
		    constr, cBOOL(CopHINTS_get(PL_curcop) & HINT_STRICT_REFS),
		    !constr
		));
	    }
	    break;
	case OA_SCALARREF:
	  if (!numargs) goto try_defsv;
	  else {
	    const bool wantscalar =
		PL_op->op_private & OPpCOREARGS_SCALARMOD;
	    if (!svp || !*svp || !SvROK(*svp)
	        /* We have to permit globrefs even for the \$ proto, as
	           *foo is indistinguishable from ${\*foo}, and the proto-
	           type permits the latter. */
	     || SvTYPE(SvRV(*svp)) > (
	             wantscalar       ? SVt_PVLV
	           : opnum == OP_LOCK || opnum == OP_UNDEF
	                              ? SVt_PVCV
	           :                    SVt_PVHV
	        )
	       )
		DIE(aTHX_
		 "Type of arg %d to &CORE::%s must be %s",
		  whicharg, PL_op_name[opnum],
		  wantscalar
		    ? "scalar reference"
		    : opnum == OP_LOCK || opnum == OP_UNDEF
		       ? "reference to one of [$@%&*]"
		       : "reference to one of [$@%*]"
		);
	    PUSHs(SvRV(*svp));
	    if (opnum == OP_UNDEF && SvRV(*svp) == (SV *)PL_defgv
	     && CX_CUR()->cx_type & CXp_HASARGS) {
		/* Undo @_ localisation, so that sub exit does not undo
		   part of our undeffing. */
		PERL_CONTEXT *cx = CX_CUR();

                assert(CxHASARGS(cx));
                cx_popsub_args(cx);;
		cx->cx_type &= ~CXp_HASARGS;
	    }
	  }
	  break;
	default:
	    DIE(aTHX_ "panic: unknown OA_*: %x", (unsigned)(oa&7));
	}
	oa = oa >> 4;
    }

    RETURN;
}

PP(pp_runcv)
{
    dSP;
    CV *cv;
    if (PL_op->op_private & OPpOFFBYONE) {
	cv = find_runcv_where(FIND_RUNCV_level_eq, 1, NULL);
    }
    else cv = find_runcv(NULL);
    XPUSHs(CvEVAL(cv) ? &PL_sv_undef : sv_2mortal(newRV((SV *)cv)));
    RETURN;
}

static void
S_localise_aelem_lval(pTHX_ AV * const av, SV * const keysv,
			    const bool can_preserve)
{
    const SSize_t ix = SvIV(keysv);
    if (can_preserve ? av_exists(av, ix) : TRUE) {
	SV ** const svp = av_fetch(av, ix, 1);
	if (!svp || !*svp)
	    Perl_croak(aTHX_ PL_no_aelem, ix);
	save_aelem(av, ix, svp);
    }
    else
	SAVEADELETE(av, ix);
}

static void
S_localise_helem_lval(pTHX_ HV * const hv, SV * const keysv,
			    const bool can_preserve)
{
    if (can_preserve ? hv_exists_ent(hv, keysv, 0) : TRUE) {
	HE * const he = hv_fetch_ent(hv, keysv, 1, 0);
	SV ** const svp = he ? &HeVAL(he) : NULL;
	if (!svp || !*svp)
	    Perl_croak(aTHX_ PL_no_helem_sv, SVfARG(keysv));
	save_helem_flags(hv, keysv, svp, 0);
    }
    else
	SAVEHDELETE(hv, keysv);
}

static void
S_localise_gv_slot(pTHX_ GV *gv, U8 type)
{
    if (type == OPpLVREF_SV) {
	save_pushptrptr(gv, SvREFCNT_inc_simple(GvSV(gv)), SAVEt_GVSV);
	GvSV(gv) = 0;
    }
    else if (type == OPpLVREF_AV)
	/* XXX Inefficient, as it creates a new AV, which we are
	       about to clobber.  */
	save_ary(gv);
    else {
	assert(type == OPpLVREF_HV);
	/* XXX Likewise inefficient.  */
	save_hash(gv);
    }
}


PP(pp_refassign)
{
    dSP;
    SV * const key = PL_op->op_private & OPpLVREF_ELEM ? POPs : NULL;
    SV * const left = PL_op->op_flags & OPf_STACKED ? POPs : NULL;
    dTOPss;
    const char *bad = NULL;
    const U8 type = PL_op->op_private & OPpLVREF_TYPE;
    if (!SvROK(sv)) DIE(aTHX_ "Assigned value is not a reference");
    switch (type) {
    case OPpLVREF_SV:
	if (SvTYPE(SvRV(sv)) > SVt_PVLV)
	    bad = " SCALAR";
	break;
    case OPpLVREF_AV:
	if (SvTYPE(SvRV(sv)) != SVt_PVAV)
	    bad = "n ARRAY";
	break;
    case OPpLVREF_HV:
	if (SvTYPE(SvRV(sv)) != SVt_PVHV)
	    bad = " HASH";
	break;
    case OPpLVREF_CV:
	if (SvTYPE(SvRV(sv)) != SVt_PVCV)
	    bad = " CODE";
    }
    if (bad)
	/* diag_listed_as: Assigned value is not %s reference */
	DIE(aTHX_ "Assigned value is not a%s reference", bad);
    {
    MAGIC *mg;
    HV *stash;
    switch (left ? SvTYPE(left) : 0) {
    case 0:
    {
	SV * const old = PAD_SV(ARGTARG);
	PAD_SETSV(ARGTARG, SvREFCNT_inc_NN(SvRV(sv)));
	SvREFCNT_dec(old);
	if ((PL_op->op_private & (OPpLVAL_INTRO|OPpPAD_STATE))
		== OPpLVAL_INTRO)
	    SAVECLEARSV(PAD_SVl(ARGTARG));
	break;
    }
    case SVt_PVGV:
	if (PL_op->op_private & OPpLVAL_INTRO) {
	    S_localise_gv_slot(aTHX_ (GV *)left, type);
	}
	gv_setref(left, sv);
	SvSETMAGIC(left);
	break;
    case SVt_PVAV:
        assert(key);
	if (UNLIKELY(PL_op->op_private & OPpLVAL_INTRO)) {
	    S_localise_aelem_lval(aTHX_ (AV *)left, key,
					SvCANEXISTDELETE(left));
	}
	av_store((AV *)left, SvIV(key), SvREFCNT_inc_simple_NN(SvRV(sv)));
	break;
    case SVt_PVHV:
        if (UNLIKELY(PL_op->op_private & OPpLVAL_INTRO)) {
            assert(key);
	    S_localise_helem_lval(aTHX_ (HV *)left, key,
					SvCANEXISTDELETE(left));
        }
	(void)hv_store_ent((HV *)left, key, SvREFCNT_inc_simple_NN(SvRV(sv)), 0);
    }
    if (PL_op->op_flags & OPf_MOD)
	SETs(sv_2mortal(newSVsv(sv)));
    /* XXX else can weak references go stale before they are read, e.g.,
       in leavesub?  */
    RETURN;
    }
}

PP(pp_lvref)
{
    dSP;
    SV * const ret = sv_2mortal(newSV_type(SVt_PVMG));
    SV * const elem = PL_op->op_private & OPpLVREF_ELEM ? POPs : NULL;
    SV * const arg = PL_op->op_flags & OPf_STACKED ? POPs : NULL;
    MAGIC * const mg = sv_magicext(ret, arg, PERL_MAGIC_lvref,
				   &PL_vtbl_lvref, (char *)elem,
				   elem ? HEf_SVKEY : (I32)ARGTARG);
    mg->mg_private = PL_op->op_private;
    if (PL_op->op_private & OPpLVREF_ITER)
	mg->mg_flags |= MGf_PERSIST;
    if (UNLIKELY(PL_op->op_private & OPpLVAL_INTRO)) {
      if (elem) {
        MAGIC *mg;
        HV *stash;
        assert(arg);
        {
            const bool can_preserve = SvCANEXISTDELETE(arg);
            if (SvTYPE(arg) == SVt_PVAV)
              S_localise_aelem_lval(aTHX_ (AV *)arg, elem, can_preserve);
            else
              S_localise_helem_lval(aTHX_ (HV *)arg, elem, can_preserve);
        }
      }
      else if (arg) {
	S_localise_gv_slot(aTHX_ (GV *)arg, 
				 PL_op->op_private & OPpLVREF_TYPE);
      }
      else if (!(PL_op->op_private & OPpPAD_STATE))
	SAVECLEARSV(PAD_SVl(ARGTARG));
    }
    XPUSHs(ret);
    RETURN;
}

PP(pp_lvrefslice)
{
    dSP; dMARK;
    AV * const av = (AV *)POPs;
    const bool localizing = PL_op->op_private & OPpLVAL_INTRO;
    bool can_preserve = FALSE;

    if (UNLIKELY(localizing)) {
	MAGIC *mg;
	HV *stash;
	SV **svp;

	can_preserve = SvCANEXISTDELETE(av);

	if (SvTYPE(av) == SVt_PVAV) {
	    SSize_t max = -1;

	    for (svp = MARK + 1; svp <= SP; svp++) {
		const SSize_t elem = SvIV(*svp);
		if (elem > max)
		    max = elem;
	    }
	    if (max > AvMAX(av))
		av_extend(av, max);
	}
    }

    while (++MARK <= SP) {
	SV * const elemsv = *MARK;
	if (SvTYPE(av) == SVt_PVAV)
	    S_localise_aelem_lval(aTHX_ av, elemsv, can_preserve);
	else
	    S_localise_helem_lval(aTHX_ (HV *)av, elemsv, can_preserve);
	*MARK = sv_2mortal(newSV_type(SVt_PVMG));
	sv_magic(*MARK,(SV *)av,PERL_MAGIC_lvref,(char *)elemsv,HEf_SVKEY);
    }
    RETURN;
}

PP(pp_lvavref)
{
    if (PL_op->op_flags & OPf_STACKED)
	Perl_pp_rv2av(aTHX);
    else
	Perl_pp_padav(aTHX);
    {
	dSP;
	dTOPss;
	SETs(0); /* special alias marker that aassign recognises */
	XPUSHs(sv);
	RETURN;
    }
}

PP(pp_anonconst)
{
    dSP;
    dTOPss;
    SETs(sv_2mortal((SV *)newCONSTSUB(SvTYPE(CopSTASH(PL_curcop))==SVt_PVHV
					? CopSTASH(PL_curcop)
					: NULL,
				      NULL, SvREFCNT_inc_simple_NN(sv))));
    RETURN;
}

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */

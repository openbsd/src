/*    pp.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
 *    2000, 2001, 2002, 2003, 2004, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "It's a big house this, and very peculiar.  Always a bit more to discover,
 * and no knowing what you'll find around a corner.  And Elves, sir!" --Samwise
 */

#include "EXTERN.h"
#define PERL_IN_PP_C
#include "perl.h"
#include "keywords.h"

#include "reentr.h"

/* XXX I can't imagine anyone who doesn't have this actually _needs_
   it, since pid_t is an integral type.
   --AD  2/20/1998
*/
#ifdef NEED_GETPID_PROTO
extern Pid_t getpid (void);
#endif

/* variations on pp_null */

PP(pp_stub)
{
    dSP;
    if (GIMME_V == G_SCALAR)
	XPUSHs(&PL_sv_undef);
    RETURN;
}

PP(pp_scalar)
{
    return NORMAL;
}

/* Pushy stuff. */

PP(pp_padav)
{
    dSP; dTARGET;
    I32 gimme;
    if (PL_op->op_private & OPpLVAL_INTRO)
	SAVECLEARSV(PAD_SVl(PL_op->op_targ));
    EXTEND(SP, 1);
    if (PL_op->op_flags & OPf_REF) {
	PUSHs(TARG);
	RETURN;
    } else if (LVRET) {
	if (GIMME == G_SCALAR)
	    Perl_croak(aTHX_ "Can't return array to lvalue scalar context");
	PUSHs(TARG);
	RETURN;
    }
    gimme = GIMME_V;
    if (gimme == G_ARRAY) {
	I32 maxarg = AvFILL((AV*)TARG) + 1;
	EXTEND(SP, maxarg);
	if (SvMAGICAL(TARG)) {
	    U32 i;
	    for (i=0; i < (U32)maxarg; i++) {
		SV **svp = av_fetch((AV*)TARG, i, FALSE);
		SP[i+1] = (svp) ? *svp : &PL_sv_undef;
	    }
	}
	else {
	    Copy(AvARRAY((AV*)TARG), SP+1, maxarg, SV*);
	}
	SP += maxarg;
    }
    else if (gimme == G_SCALAR) {
	SV* sv = sv_newmortal();
	I32 maxarg = AvFILL((AV*)TARG) + 1;
	sv_setiv(sv, maxarg);
	PUSHs(sv);
    }
    RETURN;
}

PP(pp_padhv)
{
    dSP; dTARGET;
    I32 gimme;

    XPUSHs(TARG);
    if (PL_op->op_private & OPpLVAL_INTRO)
	SAVECLEARSV(PAD_SVl(PL_op->op_targ));
    if (PL_op->op_flags & OPf_REF)
	RETURN;
    else if (LVRET) {
	if (GIMME == G_SCALAR)
	    Perl_croak(aTHX_ "Can't return hash to lvalue scalar context");
	RETURN;
    }
    gimme = GIMME_V;
    if (gimme == G_ARRAY) {
	RETURNOP(do_kv());
    }
    else if (gimme == G_SCALAR) {
	SV* sv = Perl_hv_scalar(aTHX_ (HV*)TARG);
	SETs(sv);
    }
    RETURN;
}

PP(pp_padany)
{
    DIE(aTHX_ "NOT IMPL LINE %d",__LINE__);
}

/* Translations. */

PP(pp_rv2gv)
{
    dSP; dTOPss;

    if (SvROK(sv)) {
      wasref:
	tryAMAGICunDEREF(to_gv);

	sv = SvRV(sv);
	if (SvTYPE(sv) == SVt_PVIO) {
	    GV *gv = (GV*) sv_newmortal();
	    gv_init(gv, 0, "", 0, 0);
	    GvIOp(gv) = (IO *)sv;
	    (void)SvREFCNT_inc(sv);
	    sv = (SV*) gv;
	}
	else if (SvTYPE(sv) != SVt_PVGV)
	    DIE(aTHX_ "Not a GLOB reference");
    }
    else {
	if (SvTYPE(sv) != SVt_PVGV) {
	    char *sym;
	    STRLEN len;

	    if (SvGMAGICAL(sv)) {
		mg_get(sv);
		if (SvROK(sv))
		    goto wasref;
	    }
	    if (!SvOK(sv) && sv != &PL_sv_undef) {
		/* If this is a 'my' scalar and flag is set then vivify
		 * NI-S 1999/05/07
		 */
		if (PL_op->op_private & OPpDEREF) {
		    char *name;
		    GV *gv;
		    if (cUNOP->op_targ) {
			STRLEN len;
			SV *namesv = PAD_SV(cUNOP->op_targ);
			name = SvPV(namesv, len);
			gv = (GV*)NEWSV(0,0);
			gv_init(gv, CopSTASH(PL_curcop), name, len, 0);
		    }
		    else {
			name = CopSTASHPV(PL_curcop);
			gv = newGVgen(name);
		    }
		    if (SvTYPE(sv) < SVt_RV)
			sv_upgrade(sv, SVt_RV);
		    if (SvPVX(sv)) {
			(void)SvOOK_off(sv);		/* backoff */
			if (SvLEN(sv))
			    Safefree(SvPVX(sv));
			SvLEN(sv)=SvCUR(sv)=0;
		    }
		    SvRV(sv) = (SV*)gv;
		    SvROK_on(sv);
		    SvSETMAGIC(sv);
		    goto wasref;
		}
		if (PL_op->op_flags & OPf_REF ||
		    PL_op->op_private & HINT_STRICT_REFS)
		    DIE(aTHX_ PL_no_usym, "a symbol");
		if (ckWARN(WARN_UNINITIALIZED))
		    report_uninit();
		RETSETUNDEF;
	    }
	    sym = SvPV(sv,len);
	    if ((PL_op->op_flags & OPf_SPECIAL) &&
		!(PL_op->op_flags & OPf_MOD))
	    {
		sv = (SV*)gv_fetchpv(sym, FALSE, SVt_PVGV);
		if (!sv
		    && (!is_gv_magical(sym,len,0)
			|| !(sv = (SV*)gv_fetchpv(sym, TRUE, SVt_PVGV))))
		{
		    RETSETUNDEF;
		}
	    }
	    else {
		if (PL_op->op_private & HINT_STRICT_REFS)
		    DIE(aTHX_ PL_no_symref, sym, "a symbol");
		sv = (SV*)gv_fetchpv(sym, TRUE, SVt_PVGV);
	    }
	}
    }
    if (PL_op->op_private & OPpLVAL_INTRO)
	save_gp((GV*)sv, !(PL_op->op_flags & OPf_SPECIAL));
    SETs(sv);
    RETURN;
}

PP(pp_rv2sv)
{
    GV *gv = Nullgv;
    dSP; dTOPss;

    if (SvROK(sv)) {
      wasref:
	tryAMAGICunDEREF(to_sv);

	sv = SvRV(sv);
	switch (SvTYPE(sv)) {
	case SVt_PVAV:
	case SVt_PVHV:
	case SVt_PVCV:
	    DIE(aTHX_ "Not a SCALAR reference");
	}
    }
    else {
	char *sym;
	STRLEN len;
	gv = (GV*)sv;

	if (SvTYPE(gv) != SVt_PVGV) {
	    if (SvGMAGICAL(sv)) {
		mg_get(sv);
		if (SvROK(sv))
		    goto wasref;
	    }
	    if (!SvOK(sv)) {
		if (PL_op->op_flags & OPf_REF ||
		    PL_op->op_private & HINT_STRICT_REFS)
		    DIE(aTHX_ PL_no_usym, "a SCALAR");
		if (ckWARN(WARN_UNINITIALIZED))
		    report_uninit();
		RETSETUNDEF;
	    }
	    sym = SvPV(sv, len);
	    if ((PL_op->op_flags & OPf_SPECIAL) &&
		!(PL_op->op_flags & OPf_MOD))
	    {
		gv = (GV*)gv_fetchpv(sym, FALSE, SVt_PV);
		if (!gv
		    && (!is_gv_magical(sym,len,0)
			|| !(gv = (GV*)gv_fetchpv(sym, TRUE, SVt_PV))))
		{
		    RETSETUNDEF;
		}
	    }
	    else {
		if (PL_op->op_private & HINT_STRICT_REFS)
		    DIE(aTHX_ PL_no_symref, sym, "a SCALAR");
		gv = (GV*)gv_fetchpv(sym, TRUE, SVt_PV);
	    }
	}
	sv = GvSV(gv);
    }
    if (PL_op->op_flags & OPf_MOD) {
	if (PL_op->op_private & OPpLVAL_INTRO) {
	    if (cUNOP->op_first->op_type == OP_NULL)
		sv = save_scalar((GV*)TOPs);
	    else if (gv)
		sv = save_scalar(gv);
	    else
		Perl_croak(aTHX_ PL_no_localize_ref);
	}
	else if (PL_op->op_private & OPpDEREF)
	    vivify_ref(sv, PL_op->op_private & OPpDEREF);
    }
    SETs(sv);
    RETURN;
}

PP(pp_av2arylen)
{
    dSP;
    AV *av = (AV*)TOPs;
    SV *sv = AvARYLEN(av);
    if (!sv) {
	AvARYLEN(av) = sv = NEWSV(0,0);
	sv_upgrade(sv, SVt_IV);
	sv_magic(sv, (SV*)av, PERL_MAGIC_arylen, Nullch, 0);
    }
    SETs(sv);
    RETURN;
}

PP(pp_pos)
{
    dSP; dTARGET; dPOPss;

    if (PL_op->op_flags & OPf_MOD || LVRET) {
	if (SvTYPE(TARG) < SVt_PVLV) {
	    sv_upgrade(TARG, SVt_PVLV);
	    sv_magic(TARG, Nullsv, PERL_MAGIC_pos, Nullch, 0);
	}

	LvTYPE(TARG) = '.';
	if (LvTARG(TARG) != sv) {
	    if (LvTARG(TARG))
		SvREFCNT_dec(LvTARG(TARG));
	    LvTARG(TARG) = SvREFCNT_inc(sv);
	}
	PUSHs(TARG);	/* no SvSETMAGIC */
	RETURN;
    }
    else {
	MAGIC* mg;

	if (SvTYPE(sv) >= SVt_PVMG && SvMAGIC(sv)) {
	    mg = mg_find(sv, PERL_MAGIC_regex_global);
	    if (mg && mg->mg_len >= 0) {
		I32 i = mg->mg_len;
		if (DO_UTF8(sv))
		    sv_pos_b2u(sv, &i);
		PUSHi(i + PL_curcop->cop_arybase);
		RETURN;
	    }
	}
	RETPUSHUNDEF;
    }
}

PP(pp_rv2cv)
{
    dSP;
    GV *gv;
    HV *stash;

    /* We usually try to add a non-existent subroutine in case of AUTOLOAD. */
    /* (But not in defined().) */
    CV *cv = sv_2cv(TOPs, &stash, &gv, !(PL_op->op_flags & OPf_SPECIAL));
    if (cv) {
	if (CvCLONE(cv))
	    cv = (CV*)sv_2mortal((SV*)cv_clone(cv));
	if ((PL_op->op_private & OPpLVAL_INTRO)) {
	    if (gv && GvCV(gv) == cv && (gv = gv_autoload4(GvSTASH(gv), GvNAME(gv), GvNAMELEN(gv), FALSE)))
		cv = GvCV(gv);
	    if (!CvLVALUE(cv))
		DIE(aTHX_ "Can't modify non-lvalue subroutine call");
	}
    }
    else
	cv = (CV*)&PL_sv_undef;
    SETs((SV*)cv);
    RETURN;
}

PP(pp_prototype)
{
    dSP;
    CV *cv;
    HV *stash;
    GV *gv;
    SV *ret;

    ret = &PL_sv_undef;
    if (SvPOK(TOPs) && SvCUR(TOPs) >= 7) {
	char *s = SvPVX(TOPs);
	if (strnEQ(s, "CORE::", 6)) {
	    int code;
	
	    code = keyword(s + 6, SvCUR(TOPs) - 6);
	    if (code < 0) {	/* Overridable. */
#define MAX_ARGS_OP ((sizeof(I32) - 1) * 2)
		int i = 0, n = 0, seen_question = 0;
		I32 oa;
		char str[ MAX_ARGS_OP * 2 + 2 ]; /* One ';', one '\0' */

		if (code == -KEY_chop || code == -KEY_chomp)
		    goto set;
		while (i < MAXO) {	/* The slow way. */
		    if (strEQ(s + 6, PL_op_name[i])
			|| strEQ(s + 6, PL_op_desc[i]))
		    {
			goto found;
		    }
		    i++;
		}
		goto nonesuch;		/* Should not happen... */
	      found:
		oa = PL_opargs[i] >> OASHIFT;
		while (oa) {
		    if (oa & OA_OPTIONAL && !seen_question) {
			seen_question = 1;
			str[n++] = ';';
		    }
		    else if (n && str[0] == ';' && seen_question)
			goto set;	/* XXXX system, exec */
		    if ((oa & (OA_OPTIONAL - 1)) >= OA_AVREF
			&& (oa & (OA_OPTIONAL - 1)) <= OA_SCALARREF
			/* But globs are already references (kinda) */
			&& (oa & (OA_OPTIONAL - 1)) != OA_FILEREF
		    ) {
			str[n++] = '\\';
		    }
		    str[n++] = ("?$@@%&*$")[oa & (OA_OPTIONAL - 1)];
		    oa = oa >> 4;
		}
		str[n++] = '\0';
		ret = sv_2mortal(newSVpvn(str, n - 1));
	    }
	    else if (code)		/* Non-Overridable */
		goto set;
	    else {			/* None such */
	      nonesuch:
		DIE(aTHX_ "Can't find an opnumber for \"%s\"", s+6);
	    }
	}
    }
    cv = sv_2cv(TOPs, &stash, &gv, FALSE);
    if (cv && SvPOK(cv))
	ret = sv_2mortal(newSVpvn(SvPVX(cv), SvCUR(cv)));
  set:
    SETs(ret);
    RETURN;
}

PP(pp_anoncode)
{
    dSP;
    CV* cv = (CV*)PAD_SV(PL_op->op_targ);
    if (CvCLONE(cv))
	cv = (CV*)sv_2mortal((SV*)cv_clone(cv));
    EXTEND(SP,1);
    PUSHs((SV*)cv);
    RETURN;
}

PP(pp_srefgen)
{
    dSP;
    *SP = refto(*SP);
    RETURN;
}

PP(pp_refgen)
{
    dSP; dMARK;
    if (GIMME != G_ARRAY) {
	if (++MARK <= SP)
	    *MARK = *SP;
	else
	    *MARK = &PL_sv_undef;
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

    if (SvTYPE(sv) == SVt_PVLV && LvTYPE(sv) == 'y') {
	if (LvTARGLEN(sv))
	    vivify_defelem(sv);
	if (!(sv = LvTARG(sv)))
	    sv = &PL_sv_undef;
	else
	    (void)SvREFCNT_inc(sv);
    }
    else if (SvTYPE(sv) == SVt_PVAV) {
	if (!AvREAL((AV*)sv) && AvREIFY((AV*)sv))
	    av_reify((AV*)sv);
	SvTEMP_off(sv);
	(void)SvREFCNT_inc(sv);
    }
    else if (SvPADTMP(sv) && !IS_PADGV(sv))
        sv = newSVsv(sv);
    else {
	SvTEMP_off(sv);
	(void)SvREFCNT_inc(sv);
    }
    rv = sv_newmortal();
    sv_upgrade(rv, SVt_RV);
    SvRV(rv) = sv;
    SvROK_on(rv);
    return rv;
}

PP(pp_ref)
{
    dSP; dTARGET;
    SV *sv;
    char *pv;

    sv = POPs;

    if (sv && SvGMAGICAL(sv))
	mg_get(sv);

    if (!sv || !SvROK(sv))
	RETPUSHNO;

    sv = SvRV(sv);
    pv = sv_reftype(sv,TRUE);
    PUSHp(pv, strlen(pv));
    RETURN;
}

PP(pp_bless)
{
    dSP;
    HV *stash;

    if (MAXARG == 1)
	stash = CopSTASH(PL_curcop);
    else {
	SV *ssv = POPs;
	STRLEN len;
	char *ptr;

	if (ssv && !SvGMAGICAL(ssv) && !SvAMAGIC(ssv) && SvROK(ssv))
	    Perl_croak(aTHX_ "Attempt to bless into a reference");
	ptr = SvPV(ssv,len);
	if (ckWARN(WARN_MISC) && len == 0)
	    Perl_warner(aTHX_ packWARN(WARN_MISC),
		   "Explicit blessing to '' (assuming package main)");
	stash = gv_stashpvn(ptr, len, TRUE);
    }

    (void)sv_bless(TOPs, stash);
    RETURN;
}

PP(pp_gelem)
{
    GV *gv;
    SV *sv;
    SV *tmpRef;
    char *elem;
    dSP;
    STRLEN n_a;

    sv = POPs;
    elem = SvPV(sv, n_a);
    gv = (GV*)POPs;
    tmpRef = Nullsv;
    sv = Nullsv;
    switch (elem ? *elem : '\0')
    {
    case 'A':
	if (strEQ(elem, "ARRAY"))
	    tmpRef = (SV*)GvAV(gv);
	break;
    case 'C':
	if (strEQ(elem, "CODE"))
	    tmpRef = (SV*)GvCVu(gv);
	break;
    case 'F':
	if (strEQ(elem, "FILEHANDLE")) {
	    /* finally deprecated in 5.8.0 */
	    deprecate("*glob{FILEHANDLE}");
	    tmpRef = (SV*)GvIOp(gv);
	}
	else
	if (strEQ(elem, "FORMAT"))
	    tmpRef = (SV*)GvFORM(gv);
	break;
    case 'G':
	if (strEQ(elem, "GLOB"))
	    tmpRef = (SV*)gv;
	break;
    case 'H':
	if (strEQ(elem, "HASH"))
	    tmpRef = (SV*)GvHV(gv);
	break;
    case 'I':
	if (strEQ(elem, "IO"))
	    tmpRef = (SV*)GvIOp(gv);
	break;
    case 'N':
	if (strEQ(elem, "NAME"))
	    sv = newSVpvn(GvNAME(gv), GvNAMELEN(gv));
	break;
    case 'P':
	if (strEQ(elem, "PACKAGE"))
	    sv = newSVpv(HvNAME(GvSTASH(gv)), 0);
	break;
    case 'S':
	if (strEQ(elem, "SCALAR"))
	    tmpRef = GvSV(gv);
	break;
    }
    if (tmpRef)
	sv = newRV(tmpRef);
    if (sv)
	sv_2mortal(sv);
    else
	sv = &PL_sv_undef;
    XPUSHs(sv);
    RETURN;
}

/* Pattern matching */

PP(pp_study)
{
    dSP; dPOPss;
    register unsigned char *s;
    register I32 pos;
    register I32 ch;
    register I32 *sfirst;
    register I32 *snext;
    STRLEN len;

    if (sv == PL_lastscream) {
	if (SvSCREAM(sv))
	    RETPUSHYES;
    }
    else {
	if (PL_lastscream) {
	    SvSCREAM_off(PL_lastscream);
	    SvREFCNT_dec(PL_lastscream);
	}
	PL_lastscream = SvREFCNT_inc(sv);
    }

    s = (unsigned char*)(SvPV(sv, len));
    pos = len;
    if (pos <= 0)
	RETPUSHNO;
    if (pos > PL_maxscream) {
	if (PL_maxscream < 0) {
	    PL_maxscream = pos + 80;
	    New(301, PL_screamfirst, 256, I32);
	    New(302, PL_screamnext, PL_maxscream, I32);
	}
	else {
	    PL_maxscream = pos + pos / 4;
	    Renew(PL_screamnext, PL_maxscream, I32);
	}
    }

    sfirst = PL_screamfirst;
    snext = PL_screamnext;

    if (!sfirst || !snext)
	DIE(aTHX_ "do_study: out of memory");

    for (ch = 256; ch; --ch)
	*sfirst++ = -1;
    sfirst -= 256;

    while (--pos >= 0) {
	ch = s[pos];
	if (sfirst[ch] >= 0)
	    snext[pos] = sfirst[ch] - pos;
	else
	    snext[pos] = -pos;
	sfirst[ch] = pos;
    }

    SvSCREAM_on(sv);
    /* piggyback on m//g magic */
    sv_magic(sv, Nullsv, PERL_MAGIC_regex_global, Nullch, 0);
    RETPUSHYES;
}

PP(pp_trans)
{
    dSP; dTARG;
    SV *sv;

    if (PL_op->op_flags & OPf_STACKED)
	sv = POPs;
    else {
	sv = DEFSV;
	EXTEND(SP,1);
    }
    TARG = sv_newmortal();
    PUSHi(do_trans(sv));
    RETURN;
}

/* Lvalue operators. */

PP(pp_schop)
{
    dSP; dTARGET;
    do_chop(TARG, TOPs);
    SETTARG;
    RETURN;
}

PP(pp_chop)
{
    dSP; dMARK; dTARGET; dORIGMARK;
    while (MARK < SP)
	do_chop(TARG, *++MARK);
    SP = ORIGMARK;
    PUSHTARG;
    RETURN;
}

PP(pp_schomp)
{
    dSP; dTARGET;
    SETi(do_chomp(TOPs));
    RETURN;
}

PP(pp_chomp)
{
    dSP; dMARK; dTARGET;
    register I32 count = 0;

    while (SP > MARK)
	count += do_chomp(POPs);
    PUSHi(count);
    RETURN;
}

PP(pp_defined)
{
    dSP;
    register SV* sv;

    sv = POPs;
    if (!sv || !SvANY(sv))
	RETPUSHNO;
    switch (SvTYPE(sv)) {
    case SVt_PVAV:
	if (AvMAX(sv) >= 0 || SvGMAGICAL(sv)
		|| (SvRMAGICAL(sv) && mg_find(sv, PERL_MAGIC_tied)))
	    RETPUSHYES;
	break;
    case SVt_PVHV:
	if (HvARRAY(sv) || SvGMAGICAL(sv)
		|| (SvRMAGICAL(sv) && mg_find(sv, PERL_MAGIC_tied)))
	    RETPUSHYES;
	break;
    case SVt_PVCV:
	if (CvROOT(sv) || CvXSUB(sv))
	    RETPUSHYES;
	break;
    default:
	if (SvGMAGICAL(sv))
	    mg_get(sv);
	if (SvOK(sv))
	    RETPUSHYES;
    }
    RETPUSHNO;
}

PP(pp_undef)
{
    dSP;
    SV *sv;

    if (!PL_op->op_private) {
	EXTEND(SP, 1);
	RETPUSHUNDEF;
    }

    sv = POPs;
    if (!sv)
	RETPUSHUNDEF;

    if (SvTHINKFIRST(sv))
	sv_force_normal(sv);

    switch (SvTYPE(sv)) {
    case SVt_NULL:
	break;
    case SVt_PVAV:
	av_undef((AV*)sv);
	break;
    case SVt_PVHV:
	hv_undef((HV*)sv);
	break;
    case SVt_PVCV:
	if (ckWARN(WARN_MISC) && cv_const_sv((CV*)sv))
	    Perl_warner(aTHX_ packWARN(WARN_MISC), "Constant subroutine %s undefined",
		 CvANON((CV*)sv) ? "(anonymous)" : GvENAME(CvGV((CV*)sv)));
	/* FALL THROUGH */
    case SVt_PVFM:
	{
	    /* let user-undef'd sub keep its identity */
	    GV* gv = CvGV((CV*)sv);
	    cv_undef((CV*)sv);
	    CvGV((CV*)sv) = gv;
	}
	break;
    case SVt_PVGV:
	if (SvFAKE(sv))
	    SvSetMagicSV(sv, &PL_sv_undef);
	else {
	    GP *gp;
	    gp_free((GV*)sv);
	    Newz(602, gp, 1, GP);
	    GvGP(sv) = gp_ref(gp);
	    GvSV(sv) = NEWSV(72,0);
	    GvLINE(sv) = CopLINE(PL_curcop);
	    GvEGV(sv) = (GV*)sv;
	    GvMULTI_on(sv);
	}
	break;
    default:
	if (SvTYPE(sv) >= SVt_PV && SvPVX(sv) && SvLEN(sv)) {
	    (void)SvOOK_off(sv);
	    Safefree(SvPVX(sv));
	    SvPV_set(sv, Nullch);
	    SvLEN_set(sv, 0);
	}
	(void)SvOK_off(sv);
	SvSETMAGIC(sv);
    }

    RETPUSHUNDEF;
}

PP(pp_predec)
{
    dSP;
    if (SvTYPE(TOPs) > SVt_PVLV)
	DIE(aTHX_ PL_no_modify);
    if (!SvREADONLY(TOPs) && SvIOK_notUV(TOPs) && !SvNOK(TOPs) && !SvPOK(TOPs)
        && SvIVX(TOPs) != IV_MIN)
    {
	--SvIVX(TOPs);
	SvFLAGS(TOPs) &= ~(SVp_NOK|SVp_POK);
    }
    else
	sv_dec(TOPs);
    SvSETMAGIC(TOPs);
    return NORMAL;
}

PP(pp_postinc)
{
    dSP; dTARGET;
    if (SvTYPE(TOPs) > SVt_PVLV)
	DIE(aTHX_ PL_no_modify);
    sv_setsv(TARG, TOPs);
    if (!SvREADONLY(TOPs) && SvIOK_notUV(TOPs) && !SvNOK(TOPs) && !SvPOK(TOPs)
        && SvIVX(TOPs) != IV_MAX)
    {
	++SvIVX(TOPs);
	SvFLAGS(TOPs) &= ~(SVp_NOK|SVp_POK);
    }
    else
	sv_inc(TOPs);
    SvSETMAGIC(TOPs);
    /* special case for undef: see thread at 2003-03/msg00536.html in archive */
    if (!SvOK(TARG))
	sv_setiv(TARG, 0);
    SETs(TARG);
    return NORMAL;
}

PP(pp_postdec)
{
    dSP; dTARGET;
    if (SvTYPE(TOPs) > SVt_PVLV)
	DIE(aTHX_ PL_no_modify);
    sv_setsv(TARG, TOPs);
    if (!SvREADONLY(TOPs) && SvIOK_notUV(TOPs) && !SvNOK(TOPs) && !SvPOK(TOPs)
        && SvIVX(TOPs) != IV_MIN)
    {
	--SvIVX(TOPs);
	SvFLAGS(TOPs) &= ~(SVp_NOK|SVp_POK);
    }
    else
	sv_dec(TOPs);
    SvSETMAGIC(TOPs);
    SETs(TARG);
    return NORMAL;
}

/* Ordinary operators. */

PP(pp_pow)
{
    dSP; dATARGET;
#ifdef PERL_PRESERVE_IVUV
    bool is_int = 0;
#endif
    tryAMAGICbin(pow,opASSIGN);
#ifdef PERL_PRESERVE_IVUV
    /* For integer to integer power, we do the calculation by hand wherever
       we're sure it is safe; otherwise we call pow() and try to convert to
       integer afterwards. */
    {
        SvIV_please(TOPm1s);
        if (SvIOK(TOPm1s)) {
            bool baseuok = SvUOK(TOPm1s);
            UV baseuv;

            if (baseuok) {
                baseuv = SvUVX(TOPm1s);
            } else {
                IV iv = SvIVX(TOPm1s);
                if (iv >= 0) {
                    baseuv = iv;
                    baseuok = TRUE; /* effectively it's a UV now */
                } else {
                    baseuv = -iv; /* abs, baseuok == false records sign */
                }
            }
            SvIV_please(TOPs);
            if (SvIOK(TOPs)) {
                UV power;

                if (SvUOK(TOPs)) {
                    power = SvUVX(TOPs);
                } else {
                    IV iv = SvIVX(TOPs);
                    if (iv >= 0) {
                        power = iv;
                    } else {
                        goto float_it; /* Can't do negative powers this way.  */
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
                    int n = 0;

                    for (; power; base *= base, n++) {
                        /* Do I look like I trust gcc with long longs here?
                           Do I hell.  */
                        UV bit = (UV)1 << (UV)n;
                        if (power & bit) {
                            result *= base;
                            /* Only bother to clear the bit if it is set.  */
                            power -= bit;
                           /* Avoid squaring base again if we're done. */
                           if (power == 0) break;
                        }
                    }
                    SP--;
                    SETn( result );
                    SvIV_please(TOPs);
                    RETURN;
		} else {
		    register unsigned int highbit = 8 * sizeof(UV);
		    register unsigned int lowbit = 0;
		    register unsigned int diff;
		    bool odd_power = (bool)(power & 1);
		    while ((diff = (highbit - lowbit) >> 1)) {
			if (baseuv & ~((1 << (lowbit + diff)) - 1))
			    lowbit += diff;
			else 
			    highbit -= diff;
		    }
		    /* we now have baseuv < 2 ** highbit */
		    if (power * highbit <= 8 * sizeof(UV)) {
			/* result will definitely fit in UV, so use UV math
			   on same algorithm as above */
			register UV result = 1;
			register UV base = baseuv;
			register int n = 0;
			for (; power; base *= base, n++) {
			    register UV bit = (UV)1 << (UV)n;
			    if (power & bit) {
				result *= base;
				power -= bit;
				if (power == 0) break;
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
	}
    }
  float_it:
#endif    
    {
	dPOPTOPnnrl;
	SETn( Perl_pow( left, right) );
#ifdef PERL_PRESERVE_IVUV
	if (is_int)
	    SvIV_please(TOPs);
#endif
	RETURN;
    }
}

PP(pp_multiply)
{
    dSP; dATARGET; tryAMAGICbin(mult,opASSIGN);
#ifdef PERL_PRESERVE_IVUV
    SvIV_please(TOPs);
    if (SvIOK(TOPs)) {
	/* Unless the left argument is integer in range we are going to have to
	   use NV maths. Hence only attempt to coerce the right argument if
	   we know the left is integer.  */
	/* Left operand is defined, so is it IV? */
	SvIV_please(TOPm1s);
	if (SvIOK(TOPm1s)) {
	    bool auvok = SvUOK(TOPm1s);
	    bool buvok = SvUOK(TOPs);
	    const UV topmask = (~ (UV)0) << (4 * sizeof (UV));
	    const UV botmask = ~((~ (UV)0) << (4 * sizeof (UV)));
	    UV alow;
	    UV ahigh;
	    UV blow;
	    UV bhigh;

	    if (auvok) {
		alow = SvUVX(TOPm1s);
	    } else {
		IV aiv = SvIVX(TOPm1s);
		if (aiv >= 0) {
		    alow = aiv;
		    auvok = TRUE; /* effectively it's a UV now */
		} else {
		    alow = -aiv; /* abs, auvok == false records sign */
		}
	    }
	    if (buvok) {
		blow = SvUVX(TOPs);
	    } else {
		IV biv = SvIVX(TOPs);
		if (biv >= 0) {
		    blow = biv;
		    buvok = TRUE; /* effectively it's a UV now */
		} else {
		    blow = -biv; /* abs, buvok == false records sign */
		}
	    }

	    /* If this does sign extension on unsigned it's time for plan B  */
	    ahigh = alow >> (4 * sizeof (UV));
	    alow &= botmask;
	    bhigh = blow >> (4 * sizeof (UV));
	    blow &= botmask;
	    if (ahigh && bhigh) {
		/* eg 32 bit is at least 0x10000 * 0x10000 == 0x100000000
		   which is overflow. Drop to NVs below.  */
	    } else if (!ahigh && !bhigh) {
		/* eg 32 bit is at most 0xFFFF * 0xFFFF == 0xFFFE0001
		   so the unsigned multiply cannot overflow.  */
		UV product = alow * blow;
		if (auvok == buvok) {
		    /* -ve * -ve or +ve * +ve gives a +ve result.  */
		    SP--;
		    SETu( product );
		    RETURN;
		} else if (product <= (UV)IV_MIN) {
		    /* 2s complement assumption that (UV)-IV_MIN is correct.  */
		    /* -ve result, which could overflow an IV  */
		    SP--;
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
			    SETi( -(IV)product_low );
			    RETURN;
			} /* else drop to NVs below. */
		    }
		} /* product_middle too large */
	    } /* ahigh && bhigh */
	} /* SvIOK(TOPm1s) */
    } /* SvIOK(TOPs) */
#endif
    {
      dPOPTOPnnrl;
      SETn( left * right );
      RETURN;
    }
}

PP(pp_divide)
{
    dSP; dATARGET; tryAMAGICbin(div,opASSIGN);
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
    SvIV_please(TOPs);
    if (SvIOK(TOPs)) {
        SvIV_please(TOPm1s);
        if (SvIOK(TOPm1s)) {
            bool left_non_neg = SvUOK(TOPm1s);
            bool right_non_neg = SvUOK(TOPs);
            UV left;
            UV right;

            if (right_non_neg) {
                right = SvUVX(TOPs);
            }
	    else {
                IV biv = SvIVX(TOPs);
                if (biv >= 0) {
                    right = biv;
                    right_non_neg = TRUE; /* effectively it's a UV now */
                }
		else {
                    right = -biv;
                }
            }
            /* historically undef()/0 gives a "Use of uninitialized value"
               warning before dieing, hence this test goes here.
               If it were immediately before the second SvIV_please, then
               DIE() would be invoked before left was even inspected, so
               no inpsection would give no warning.  */
            if (right == 0)
                DIE(aTHX_ "Illegal division by zero");

            if (left_non_neg) {
                left = SvUVX(TOPm1s);
            }
	    else {
                IV aiv = SvIVX(TOPm1s);
                if (aiv >= 0) {
                    left = aiv;
                    left_non_neg = TRUE; /* effectively it's a UV now */
                }
		else {
                    left = -aiv;
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
                UV result = left / right;
                if (result * right == left) {
                    SP--; /* result is valid */
                    if (left_non_neg == right_non_neg) {
                        /* signs identical, result is positive.  */
                        SETu( result );
                        RETURN;
                    }
                    /* 2s complement assumption */
                    if (result <= (UV)IV_MIN)
                        SETi( -(IV)result );
                    else {
                        /* It's exact but too negative for IV. */
                        SETn( -(NV)result );
                    }
                    RETURN;
                } /* tried integer divide but it was not an integer result */
            } /* else (PERL_ABS(result) < 1.0) or (both UVs in range for NV) */
        } /* left wasn't SvIOK */
    } /* right wasn't SvIOK */
#endif /* PERL_TRY_UV_DIVIDE */
    {
	dPOPPOPnnrl;
	if (right == 0.0)
	    DIE(aTHX_ "Illegal division by zero");
	PUSHn( left / right );
	RETURN;
    }
}

PP(pp_modulo)
{
    dSP; dATARGET; tryAMAGICbin(modulo,opASSIGN);
    {
	UV left  = 0;
	UV right = 0;
	bool left_neg = FALSE;
	bool right_neg = FALSE;
	bool use_double = FALSE;
	bool dright_valid = FALSE;
	NV dright = 0.0;
	NV dleft  = 0.0;

        SvIV_please(TOPs);
        if (SvIOK(TOPs)) {
            right_neg = !SvUOK(TOPs);
            if (!right_neg) {
                right = SvUVX(POPs);
            } else {
                IV biv = SvIVX(POPs);
                if (biv >= 0) {
                    right = biv;
                    right_neg = FALSE; /* effectively it's a UV now */
                } else {
                    right = -biv;
                }
            }
        }
        else {
	    dright = POPn;
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
        SvIV_please(TOPs);
	if (!use_double && SvIOK(TOPs)) {
            if (SvIOK(TOPs)) {
                left_neg = !SvUOK(TOPs);
                if (!left_neg) {
                    left = SvUVX(POPs);
                } else {
                    IV aiv = SvIVX(POPs);
                    if (aiv >= 0) {
                        left = aiv;
                        left_neg = FALSE; /* effectively it's a UV now */
                    } else {
                        left = -aiv;
                    }
                }
            }
        }
	else {
	    dleft = POPn;
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
  dSP; dATARGET; tryAMAGICbin(repeat,opASSIGN);
  {
    register IV count;
    dPOPss;
    if (SvGMAGICAL(sv))
	 mg_get(sv);
    if (SvIOKp(sv)) {
	 if (SvUOK(sv)) {
	      UV uv = SvUV(sv);
	      if (uv > IV_MAX)
		   count = IV_MAX; /* The best we can do? */
	      else
		   count = uv;
	 } else {
	      IV iv = SvIV(sv);
	      if (iv < 0)
		   count = 0;
	      else
		   count = iv;
	 }
    }
    else if (SvNOKp(sv)) {
	 NV nv = SvNV(sv);
	 if (nv < 0.0)
	      count = 0;
	 else
	      count = (IV)nv;
    }
    else
	 count = SvIVx(sv);
    if (GIMME == G_ARRAY && PL_op->op_private & OPpREPEAT_DOLIST) {
	dMARK;
	I32 items = SP - MARK;
	I32 max;
	static const char oom_list_extend[] =
	  "Out of memory during list extend";

	max = items * count;
	MEM_WRAP_CHECK_1(max, SV*, oom_list_extend);
	/* Did the max computation overflow? */
	if (items > 0 && max > 0 && (max < items || max < count))
	   Perl_croak(aTHX_ oom_list_extend);
	MEXTEND(MARK, max);
	if (count > 1) {
	    while (SP > MARK) {
#if 0
	      /* This code was intended to fix 20010809.028:

	         $x = 'abcd';
		 for (($x =~ /./g) x 2) {
		     print chop; # "abcdabcd" expected as output.
		 }

	       * but that change (#11635) broke this code:

	       $x = [("foo")x2]; # only one "foo" ended up in the anonlist.

	       * I can't think of a better fix that doesn't introduce
	       * an efficiency hit by copying the SVs. The stack isn't
	       * refcounted, and mortalisation obviously doesn't
	       * Do The Right Thing when the stack has more than
	       * one pointer to the same mortal value.
	       * .robin.
	       */
		if (*SP) {
		    *SP = sv_2mortal(newSVsv(*SP));
		    SvREADONLY_on(*SP);
		}
#else
               if (*SP)
		   SvTEMP_off((*SP));
#endif
		SP--;
	    }
	    MARK++;
	    repeatcpy((char*)(MARK + items), (char*)MARK,
		items * sizeof(SV*), count - 1);
	    SP += max;
	}
	else if (count <= 0)
	    SP -= items;
    }
    else {	/* Note: mark already snarfed by pp_list */
	SV *tmpstr = POPs;
	STRLEN len;
	bool isutf;
	static const char oom_string_extend[] =
	  "Out of memory during string extend";

	SvSetSV(TARG, tmpstr);
	SvPV_force(TARG, len);
	isutf = DO_UTF8(TARG);
	if (count != 1) {
	    if (count < 1)
		SvCUR_set(TARG, 0);
	    else {
		IV max = count * len;
		if (len > ((MEM_SIZE)~0)/count)
		     Perl_croak(aTHX_ oom_string_extend);
	        MEM_WRAP_CHECK_1(max, char, oom_string_extend);
		SvGROW(TARG, (count * len) + 1);
		repeatcpy(SvPVX(TARG) + len, SvPVX(TARG), len, count - 1);
		SvCUR(TARG) *= count;
	    }
	    *SvEND(TARG) = '\0';
	}
	if (isutf)
	    (void)SvPOK_only_UTF8(TARG);
	else
	    (void)SvPOK_only(TARG);

	if (PL_op->op_private & OPpREPEAT_DOLIST) {
	    /* The parser saw this as a list repeat, and there
	       are probably several items on the stack. But we're
	       in scalar context, and there's no pp_list to save us
	       now. So drop the rest of the items -- robin@kitsite.com
	     */
	    dMARK;
	    SP = MARK;
	}
	PUSHTARG;
    }
    RETURN;
  }
}

PP(pp_subtract)
{
    dSP; dATARGET; bool useleft; tryAMAGICbin(subtr,opASSIGN);
    useleft = USE_LEFT(TOPm1s);
#ifdef PERL_PRESERVE_IVUV
    /* See comments in pp_add (in pp_hot.c) about Overflow, and how
       "bad things" happen if you rely on signed integers wrapping.  */
    SvIV_please(TOPs);
    if (SvIOK(TOPs)) {
	/* Unless the left argument is integer in range we are going to have to
	   use NV maths. Hence only attempt to coerce the right argument if
	   we know the left is integer.  */
	register UV auv = 0;
	bool auvok = FALSE;
	bool a_valid = 0;

	if (!useleft) {
	    auv = 0;
	    a_valid = auvok = 1;
	    /* left operand is undef, treat as zero.  */
	} else {
	    /* Left operand is defined, so is it IV? */
	    SvIV_please(TOPm1s);
	    if (SvIOK(TOPm1s)) {
		if ((auvok = SvUOK(TOPm1s)))
		    auv = SvUVX(TOPm1s);
		else {
		    register IV aiv = SvIVX(TOPm1s);
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
	    bool buvok = SvUOK(TOPs);
	
	    if (buvok)
		buv = SvUVX(TOPs);
	    else {
		register IV biv = SvIVX(TOPs);
		if (biv >= 0) {
		    buv = biv;
		    buvok = 1;
		} else
		    buv = (UV)-biv;
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
    useleft = USE_LEFT(TOPm1s);
    {
	dPOPnv;
	if (!useleft) {
	    /* left operand is undef, treat as zero - value */
	    SETn(-value);
	    RETURN;
	}
	SETn( TOPn - value );
	RETURN;
    }
}

PP(pp_left_shift)
{
    dSP; dATARGET; tryAMAGICbin(lshift,opASSIGN);
    {
      IV shift = POPi;
      if (PL_op->op_private & HINT_INTEGER) {
	IV i = TOPi;
	SETi(i << shift);
      }
      else {
	UV u = TOPu;
	SETu(u << shift);
      }
      RETURN;
    }
}

PP(pp_right_shift)
{
    dSP; dATARGET; tryAMAGICbin(rshift,opASSIGN);
    {
      IV shift = POPi;
      if (PL_op->op_private & HINT_INTEGER) {
	IV i = TOPi;
	SETi(i >> shift);
      }
      else {
	UV u = TOPu;
	SETu(u >> shift);
      }
      RETURN;
    }
}

PP(pp_lt)
{
    dSP; tryAMAGICbinSET(lt,0);
#ifdef PERL_PRESERVE_IVUV
    SvIV_please(TOPs);
    if (SvIOK(TOPs)) {
	SvIV_please(TOPm1s);
	if (SvIOK(TOPm1s)) {
	    bool auvok = SvUOK(TOPm1s);
	    bool buvok = SvUOK(TOPs);
	
	    if (!auvok && !buvok) { /* ## IV < IV ## */
		IV aiv = SvIVX(TOPm1s);
		IV biv = SvIVX(TOPs);
		
		SP--;
		SETs(boolSV(aiv < biv));
		RETURN;
	    }
	    if (auvok && buvok) { /* ## UV < UV ## */
		UV auv = SvUVX(TOPm1s);
		UV buv = SvUVX(TOPs);
		
		SP--;
		SETs(boolSV(auv < buv));
		RETURN;
	    }
	    if (auvok) { /* ## UV < IV ## */
		UV auv;
		IV biv;
		
		biv = SvIVX(TOPs);
		SP--;
		if (biv < 0) {
		    /* As (a) is a UV, it's >=0, so it cannot be < */
		    SETs(&PL_sv_no);
		    RETURN;
		}
		auv = SvUVX(TOPs);
		SETs(boolSV(auv < (UV)biv));
		RETURN;
	    }
	    { /* ## IV < UV ## */
		IV aiv;
		UV buv;
		
		aiv = SvIVX(TOPm1s);
		if (aiv < 0) {
		    /* As (b) is a UV, it's >=0, so it must be < */
		    SP--;
		    SETs(&PL_sv_yes);
		    RETURN;
		}
		buv = SvUVX(TOPs);
		SP--;
		SETs(boolSV((UV)aiv < buv));
		RETURN;
	    }
	}
    }
#endif
#ifndef NV_PRESERVES_UV
#ifdef PERL_PRESERVE_IVUV
    else
#endif
        if (SvROK(TOPs) && SvROK(TOPm1s)) {
            SP--;
            SETs(boolSV(SvRV(TOPs) < SvRV(TOPp1s)));
            RETURN;
        }
#endif
    {
      dPOPnv;
      SETs(boolSV(TOPn < value));
      RETURN;
    }
}

PP(pp_gt)
{
    dSP; tryAMAGICbinSET(gt,0);
#ifdef PERL_PRESERVE_IVUV
    SvIV_please(TOPs);
    if (SvIOK(TOPs)) {
	SvIV_please(TOPm1s);
	if (SvIOK(TOPm1s)) {
	    bool auvok = SvUOK(TOPm1s);
	    bool buvok = SvUOK(TOPs);
	
	    if (!auvok && !buvok) { /* ## IV > IV ## */
		IV aiv = SvIVX(TOPm1s);
		IV biv = SvIVX(TOPs);
		
		SP--;
		SETs(boolSV(aiv > biv));
		RETURN;
	    }
	    if (auvok && buvok) { /* ## UV > UV ## */
		UV auv = SvUVX(TOPm1s);
		UV buv = SvUVX(TOPs);
		
		SP--;
		SETs(boolSV(auv > buv));
		RETURN;
	    }
	    if (auvok) { /* ## UV > IV ## */
		UV auv;
		IV biv;
		
		biv = SvIVX(TOPs);
		SP--;
		if (biv < 0) {
		    /* As (a) is a UV, it's >=0, so it must be > */
		    SETs(&PL_sv_yes);
		    RETURN;
		}
		auv = SvUVX(TOPs);
		SETs(boolSV(auv > (UV)biv));
		RETURN;
	    }
	    { /* ## IV > UV ## */
		IV aiv;
		UV buv;
		
		aiv = SvIVX(TOPm1s);
		if (aiv < 0) {
		    /* As (b) is a UV, it's >=0, so it cannot be > */
		    SP--;
		    SETs(&PL_sv_no);
		    RETURN;
		}
		buv = SvUVX(TOPs);
		SP--;
		SETs(boolSV((UV)aiv > buv));
		RETURN;
	    }
	}
    }
#endif
#ifndef NV_PRESERVES_UV
#ifdef PERL_PRESERVE_IVUV
    else
#endif
        if (SvROK(TOPs) && SvROK(TOPm1s)) {
        SP--;
        SETs(boolSV(SvRV(TOPs) > SvRV(TOPp1s)));
        RETURN;
    }
#endif
    {
      dPOPnv;
      SETs(boolSV(TOPn > value));
      RETURN;
    }
}

PP(pp_le)
{
    dSP; tryAMAGICbinSET(le,0);
#ifdef PERL_PRESERVE_IVUV
    SvIV_please(TOPs);
    if (SvIOK(TOPs)) {
	SvIV_please(TOPm1s);
	if (SvIOK(TOPm1s)) {
	    bool auvok = SvUOK(TOPm1s);
	    bool buvok = SvUOK(TOPs);
	
	    if (!auvok && !buvok) { /* ## IV <= IV ## */
		IV aiv = SvIVX(TOPm1s);
		IV biv = SvIVX(TOPs);
		
		SP--;
		SETs(boolSV(aiv <= biv));
		RETURN;
	    }
	    if (auvok && buvok) { /* ## UV <= UV ## */
		UV auv = SvUVX(TOPm1s);
		UV buv = SvUVX(TOPs);
		
		SP--;
		SETs(boolSV(auv <= buv));
		RETURN;
	    }
	    if (auvok) { /* ## UV <= IV ## */
		UV auv;
		IV biv;
		
		biv = SvIVX(TOPs);
		SP--;
		if (biv < 0) {
		    /* As (a) is a UV, it's >=0, so a cannot be <= */
		    SETs(&PL_sv_no);
		    RETURN;
		}
		auv = SvUVX(TOPs);
		SETs(boolSV(auv <= (UV)biv));
		RETURN;
	    }
	    { /* ## IV <= UV ## */
		IV aiv;
		UV buv;
		
		aiv = SvIVX(TOPm1s);
		if (aiv < 0) {
		    /* As (b) is a UV, it's >=0, so a must be <= */
		    SP--;
		    SETs(&PL_sv_yes);
		    RETURN;
		}
		buv = SvUVX(TOPs);
		SP--;
		SETs(boolSV((UV)aiv <= buv));
		RETURN;
	    }
	}
    }
#endif
#ifndef NV_PRESERVES_UV
#ifdef PERL_PRESERVE_IVUV
    else
#endif
        if (SvROK(TOPs) && SvROK(TOPm1s)) {
        SP--;
        SETs(boolSV(SvRV(TOPs) <= SvRV(TOPp1s)));
        RETURN;
    }
#endif
    {
      dPOPnv;
      SETs(boolSV(TOPn <= value));
      RETURN;
    }
}

PP(pp_ge)
{
    dSP; tryAMAGICbinSET(ge,0);
#ifdef PERL_PRESERVE_IVUV
    SvIV_please(TOPs);
    if (SvIOK(TOPs)) {
	SvIV_please(TOPm1s);
	if (SvIOK(TOPm1s)) {
	    bool auvok = SvUOK(TOPm1s);
	    bool buvok = SvUOK(TOPs);
	
	    if (!auvok && !buvok) { /* ## IV >= IV ## */
		IV aiv = SvIVX(TOPm1s);
		IV biv = SvIVX(TOPs);
		
		SP--;
		SETs(boolSV(aiv >= biv));
		RETURN;
	    }
	    if (auvok && buvok) { /* ## UV >= UV ## */
		UV auv = SvUVX(TOPm1s);
		UV buv = SvUVX(TOPs);
		
		SP--;
		SETs(boolSV(auv >= buv));
		RETURN;
	    }
	    if (auvok) { /* ## UV >= IV ## */
		UV auv;
		IV biv;
		
		biv = SvIVX(TOPs);
		SP--;
		if (biv < 0) {
		    /* As (a) is a UV, it's >=0, so it must be >= */
		    SETs(&PL_sv_yes);
		    RETURN;
		}
		auv = SvUVX(TOPs);
		SETs(boolSV(auv >= (UV)biv));
		RETURN;
	    }
	    { /* ## IV >= UV ## */
		IV aiv;
		UV buv;
		
		aiv = SvIVX(TOPm1s);
		if (aiv < 0) {
		    /* As (b) is a UV, it's >=0, so a cannot be >= */
		    SP--;
		    SETs(&PL_sv_no);
		    RETURN;
		}
		buv = SvUVX(TOPs);
		SP--;
		SETs(boolSV((UV)aiv >= buv));
		RETURN;
	    }
	}
    }
#endif
#ifndef NV_PRESERVES_UV
#ifdef PERL_PRESERVE_IVUV
    else
#endif
        if (SvROK(TOPs) && SvROK(TOPm1s)) {
        SP--;
        SETs(boolSV(SvRV(TOPs) >= SvRV(TOPp1s)));
        RETURN;
    }
#endif
    {
      dPOPnv;
      SETs(boolSV(TOPn >= value));
      RETURN;
    }
}

PP(pp_ne)
{
    dSP; tryAMAGICbinSET(ne,0);
#ifndef NV_PRESERVES_UV
    if (SvROK(TOPs) && SvROK(TOPm1s)) {
        SP--;
	SETs(boolSV(SvRV(TOPs) != SvRV(TOPp1s)));
	RETURN;
    }
#endif
#ifdef PERL_PRESERVE_IVUV
    SvIV_please(TOPs);
    if (SvIOK(TOPs)) {
	SvIV_please(TOPm1s);
	if (SvIOK(TOPm1s)) {
	    bool auvok = SvUOK(TOPm1s);
	    bool buvok = SvUOK(TOPs);
	
	    if (auvok == buvok) { /* ## IV == IV or UV == UV ## */
                /* Casting IV to UV before comparison isn't going to matter
                   on 2s complement. On 1s complement or sign&magnitude
                   (if we have any of them) it could make negative zero
                   differ from normal zero. As I understand it. (Need to
                   check - is negative zero implementation defined behaviour
                   anyway?). NWC  */
		UV buv = SvUVX(POPs);
		UV auv = SvUVX(TOPs);
		
		SETs(boolSV(auv != buv));
		RETURN;
	    }
	    {			/* ## Mixed IV,UV ## */
		IV iv;
		UV uv;
		
		/* != is commutative so swap if needed (save code) */
		if (auvok) {
		    /* swap. top of stack (b) is the iv */
		    iv = SvIVX(TOPs);
		    SP--;
		    if (iv < 0) {
			/* As (a) is a UV, it's >0, so it cannot be == */
			SETs(&PL_sv_yes);
			RETURN;
		    }
		    uv = SvUVX(TOPs);
		} else {
		    iv = SvIVX(TOPm1s);
		    SP--;
		    if (iv < 0) {
			/* As (b) is a UV, it's >0, so it cannot be == */
			SETs(&PL_sv_yes);
			RETURN;
		    }
		    uv = SvUVX(*(SP+1)); /* Do I want TOPp1s() ? */
		}
		SETs(boolSV((UV)iv != uv));
		RETURN;
	    }
	}
    }
#endif
    {
      dPOPnv;
      SETs(boolSV(TOPn != value));
      RETURN;
    }
}

PP(pp_ncmp)
{
    dSP; dTARGET; tryAMAGICbin(ncmp,0);
#ifndef NV_PRESERVES_UV
    if (SvROK(TOPs) && SvROK(TOPm1s)) {
        UV right = PTR2UV(SvRV(POPs));
        UV left = PTR2UV(SvRV(TOPs));
	SETi((left > right) - (left < right));
	RETURN;
    }
#endif
#ifdef PERL_PRESERVE_IVUV
    /* Fortunately it seems NaN isn't IOK */
    SvIV_please(TOPs);
    if (SvIOK(TOPs)) {
	SvIV_please(TOPm1s);
	if (SvIOK(TOPm1s)) {
	    bool leftuvok = SvUOK(TOPm1s);
	    bool rightuvok = SvUOK(TOPs);
	    I32 value;
	    if (!leftuvok && !rightuvok) { /* ## IV <=> IV ## */
		IV leftiv = SvIVX(TOPm1s);
		IV rightiv = SvIVX(TOPs);
		
		if (leftiv > rightiv)
		    value = 1;
		else if (leftiv < rightiv)
		    value = -1;
		else
		    value = 0;
	    } else if (leftuvok && rightuvok) { /* ## UV <=> UV ## */
		UV leftuv = SvUVX(TOPm1s);
		UV rightuv = SvUVX(TOPs);
		
		if (leftuv > rightuv)
		    value = 1;
		else if (leftuv < rightuv)
		    value = -1;
		else
		    value = 0;
	    } else if (leftuvok) { /* ## UV <=> IV ## */
		UV leftuv;
		IV rightiv;
		
		rightiv = SvIVX(TOPs);
		if (rightiv < 0) {
		    /* As (a) is a UV, it's >=0, so it cannot be < */
		    value = 1;
		} else {
		    leftuv = SvUVX(TOPm1s);
		    if (leftuv > (UV)rightiv) {
			value = 1;
		    } else if (leftuv < (UV)rightiv) {
			value = -1;
		    } else {
			value = 0;
		    }
		}
	    } else { /* ## IV <=> UV ## */
		IV leftiv;
		UV rightuv;
		
		leftiv = SvIVX(TOPm1s);
		if (leftiv < 0) {
		    /* As (b) is a UV, it's >=0, so it must be < */
		    value = -1;
		} else {
		    rightuv = SvUVX(TOPs);
		    if ((UV)leftiv > rightuv) {
			value = 1;
		    } else if ((UV)leftiv < rightuv) {
			value = -1;
		    } else {
			value = 0;
		    }
		}
	    }
	    SP--;
	    SETi(value);
	    RETURN;
	}
    }
#endif
    {
      dPOPTOPnnrl;
      I32 value;

#ifdef Perl_isnan
      if (Perl_isnan(left) || Perl_isnan(right)) {
	  SETs(&PL_sv_undef);
	  RETURN;
       }
      value = (left > right) - (left < right);
#else
      if (left == right)
	value = 0;
      else if (left < right)
	value = -1;
      else if (left > right)
	value = 1;
      else {
	SETs(&PL_sv_undef);
	RETURN;
      }
#endif
      SETi(value);
      RETURN;
    }
}

PP(pp_slt)
{
    dSP; tryAMAGICbinSET(slt,0);
    {
      dPOPTOPssrl;
      int cmp = (IN_LOCALE_RUNTIME
		 ? sv_cmp_locale(left, right)
		 : sv_cmp(left, right));
      SETs(boolSV(cmp < 0));
      RETURN;
    }
}

PP(pp_sgt)
{
    dSP; tryAMAGICbinSET(sgt,0);
    {
      dPOPTOPssrl;
      int cmp = (IN_LOCALE_RUNTIME
		 ? sv_cmp_locale(left, right)
		 : sv_cmp(left, right));
      SETs(boolSV(cmp > 0));
      RETURN;
    }
}

PP(pp_sle)
{
    dSP; tryAMAGICbinSET(sle,0);
    {
      dPOPTOPssrl;
      int cmp = (IN_LOCALE_RUNTIME
		 ? sv_cmp_locale(left, right)
		 : sv_cmp(left, right));
      SETs(boolSV(cmp <= 0));
      RETURN;
    }
}

PP(pp_sge)
{
    dSP; tryAMAGICbinSET(sge,0);
    {
      dPOPTOPssrl;
      int cmp = (IN_LOCALE_RUNTIME
		 ? sv_cmp_locale(left, right)
		 : sv_cmp(left, right));
      SETs(boolSV(cmp >= 0));
      RETURN;
    }
}

PP(pp_seq)
{
    dSP; tryAMAGICbinSET(seq,0);
    {
      dPOPTOPssrl;
      SETs(boolSV(sv_eq(left, right)));
      RETURN;
    }
}

PP(pp_sne)
{
    dSP; tryAMAGICbinSET(sne,0);
    {
      dPOPTOPssrl;
      SETs(boolSV(!sv_eq(left, right)));
      RETURN;
    }
}

PP(pp_scmp)
{
    dSP; dTARGET;  tryAMAGICbin(scmp,0);
    {
      dPOPTOPssrl;
      int cmp = (IN_LOCALE_RUNTIME
		 ? sv_cmp_locale(left, right)
		 : sv_cmp(left, right));
      SETi( cmp );
      RETURN;
    }
}

PP(pp_bit_and)
{
    dSP; dATARGET; tryAMAGICbin(band,opASSIGN);
    {
      dPOPTOPssrl;
      if (SvNIOKp(left) || SvNIOKp(right)) {
	if (PL_op->op_private & HINT_INTEGER) {
	  IV i = SvIV(left) & SvIV(right);
	  SETi(i);
	}
	else {
	  UV u = SvUV(left) & SvUV(right);
	  SETu(u);
	}
      }
      else {
	do_vop(PL_op->op_type, TARG, left, right);
	SETTARG;
      }
      RETURN;
    }
}

PP(pp_bit_xor)
{
    dSP; dATARGET; tryAMAGICbin(bxor,opASSIGN);
    {
      dPOPTOPssrl;
      if (SvNIOKp(left) || SvNIOKp(right)) {
	if (PL_op->op_private & HINT_INTEGER) {
	  IV i = (USE_LEFT(left) ? SvIV(left) : 0) ^ SvIV(right);
	  SETi(i);
	}
	else {
	  UV u = (USE_LEFT(left) ? SvUV(left) : 0) ^ SvUV(right);
	  SETu(u);
	}
      }
      else {
	do_vop(PL_op->op_type, TARG, left, right);
	SETTARG;
      }
      RETURN;
    }
}

PP(pp_bit_or)
{
    dSP; dATARGET; tryAMAGICbin(bor,opASSIGN);
    {
      dPOPTOPssrl;
      if (SvNIOKp(left) || SvNIOKp(right)) {
	if (PL_op->op_private & HINT_INTEGER) {
	  IV i = (USE_LEFT(left) ? SvIV(left) : 0) | SvIV(right);
	  SETi(i);
	}
	else {
	  UV u = (USE_LEFT(left) ? SvUV(left) : 0) | SvUV(right);
	  SETu(u);
	}
      }
      else {
	do_vop(PL_op->op_type, TARG, left, right);
	SETTARG;
      }
      RETURN;
    }
}

PP(pp_negate)
{
    dSP; dTARGET; tryAMAGICun(neg);
    {
	dTOPss;
	int flags = SvFLAGS(sv);
	if (SvGMAGICAL(sv))
	    mg_get(sv);
	if ((flags & SVf_IOK) || ((flags & (SVp_IOK | SVp_NOK)) == SVp_IOK)) {
	    /* It's publicly an integer, or privately an integer-not-float */
	oops_its_an_int:
	    if (SvIsUV(sv)) {
		if (SvIVX(sv) == IV_MIN) {
		    /* 2s complement assumption. */
		    SETi(SvIVX(sv));	/* special case: -((UV)IV_MAX+1) == IV_MIN */
		    RETURN;
		}
		else if (SvUVX(sv) <= IV_MAX) {
		    SETi(-SvIVX(sv));
		    RETURN;
		}
	    }
	    else if (SvIVX(sv) != IV_MIN) {
		SETi(-SvIVX(sv));
		RETURN;
	    }
#ifdef PERL_PRESERVE_IVUV
	    else {
		SETu((UV)IV_MIN);
		RETURN;
	    }
#endif
	}
	if (SvNIOKp(sv))
	    SETn(-SvNV(sv));
	else if (SvPOKp(sv)) {
	    STRLEN len;
	    char *s = SvPV(sv, len);
	    if (isIDFIRST(*s)) {
		sv_setpvn(TARG, "-", 1);
		sv_catsv(TARG, sv);
	    }
	    else if (*s == '+' || *s == '-') {
		sv_setsv(TARG, sv);
		*SvPV_force(TARG, len) = *s == '-' ? '+' : '-';
	    }
	    else if (DO_UTF8(sv)) {
		SvIV_please(sv);
		if (SvIOK(sv))
		    goto oops_its_an_int;
		if (SvNOK(sv))
		    sv_setnv(TARG, -SvNV(sv));
		else {
		    sv_setpvn(TARG, "-", 1);
		    sv_catsv(TARG, sv);
		}
	    }
	    else {
		SvIV_please(sv);
		if (SvIOK(sv))
		  goto oops_its_an_int;
		sv_setnv(TARG, -SvNV(sv));
	    }
	    SETTARG;
	}
	else
	    SETn(-SvNV(sv));
    }
    RETURN;
}

PP(pp_not)
{
    dSP; tryAMAGICunSET(not);
    *PL_stack_sp = boolSV(!SvTRUE(*PL_stack_sp));
    return NORMAL;
}

PP(pp_complement)
{
    dSP; dTARGET; tryAMAGICun(compl);
    {
      dTOPss;
      if (SvNIOKp(sv)) {
	if (PL_op->op_private & HINT_INTEGER) {
	  IV i = ~SvIV(sv);
	  SETi(i);
	}
	else {
	  UV u = ~SvUV(sv);
	  SETu(u);
	}
      }
      else {
	register U8 *tmps;
	register I32 anum;
	STRLEN len;

	(void)SvPV_nomg(sv,len); /* force check for uninit var */
	SvSetSV(TARG, sv);
	tmps = (U8*)SvPV_force(TARG, len);
	anum = len;
	if (SvUTF8(TARG)) {
	  /* Calculate exact length, let's not estimate. */
	  STRLEN targlen = 0;
	  U8 *result;
	  U8 *send;
	  STRLEN l;
	  UV nchar = 0;
	  UV nwide = 0;

	  send = tmps + len;
	  while (tmps < send) {
	    UV c = utf8n_to_uvchr(tmps, send-tmps, &l, UTF8_ALLOW_ANYUV);
	    tmps += UTF8SKIP(tmps);
	    targlen += UNISKIP(~c);
	    nchar++;
	    if (c > 0xff)
		nwide++;
	  }

	  /* Now rewind strings and write them. */
	  tmps -= len;

	  if (nwide) {
	      Newz(0, result, targlen + 1, U8);
	      while (tmps < send) {
		  UV c = utf8n_to_uvchr(tmps, send-tmps, &l, UTF8_ALLOW_ANYUV);
		  tmps += UTF8SKIP(tmps);
		  result = uvchr_to_utf8_flags(result, ~c, UNICODE_ALLOW_ANY);
	      }
	      *result = '\0';
	      result -= targlen;
	      sv_setpvn(TARG, (char*)result, targlen);
	      SvUTF8_on(TARG);
	  }
	  else {
	      Newz(0, result, nchar + 1, U8);
	      while (tmps < send) {
		  U8 c = (U8)utf8n_to_uvchr(tmps, 0, &l, UTF8_ALLOW_ANY);
		  tmps += UTF8SKIP(tmps);
		  *result++ = ~c;
	      }
	      *result = '\0';
	      result -= nchar;
	      sv_setpvn(TARG, (char*)result, nchar);
	      SvUTF8_off(TARG);
	  }
	  Safefree(result);
	  SETs(TARG);
	  RETURN;
	}
#ifdef LIBERAL
	{
	    register long *tmpl;
	    for ( ; anum && (unsigned long)tmps % sizeof(long); anum--, tmps++)
		*tmps = ~*tmps;
	    tmpl = (long*)tmps;
	    for ( ; anum >= sizeof(long); anum -= sizeof(long), tmpl++)
		*tmpl = ~*tmpl;
	    tmps = (U8*)tmpl;
	}
#endif
	for ( ; anum > 0; anum--, tmps++)
	    *tmps = ~*tmps;

	SETs(TARG);
      }
      RETURN;
    }
}

/* integer versions of some of the above */

PP(pp_i_multiply)
{
    dSP; dATARGET; tryAMAGICbin(mult,opASSIGN);
    {
      dPOPTOPiirl;
      SETi( left * right );
      RETURN;
    }
}

PP(pp_i_divide)
{
    dSP; dATARGET; tryAMAGICbin(div,opASSIGN);
    {
      dPOPiv;
      if (value == 0)
	DIE(aTHX_ "Illegal division by zero");
      value = POPi / value;
      PUSHi( value );
      RETURN;
    }
}

STATIC
PP(pp_i_modulo_0)
{
     /* This is the vanilla old i_modulo. */
     dSP; dATARGET; tryAMAGICbin(modulo,opASSIGN);
     {
	  dPOPTOPiirl;
	  if (!right)
	       DIE(aTHX_ "Illegal modulus zero");
	  SETi( left % right );
	  RETURN;
     }
}

#if defined(__GLIBC__) && IVSIZE == 8
STATIC
PP(pp_i_modulo_1)
{
     /* This is the i_modulo with the workaround for the _moddi3 bug
      * in (at least) glibc 2.2.5 (the PERL_ABS() the workaround).
      * See below for pp_i_modulo. */
     dSP; dATARGET; tryAMAGICbin(modulo,opASSIGN);
     {
	  dPOPTOPiirl;
	  if (!right)
	       DIE(aTHX_ "Illegal modulus zero");
	  SETi( left % PERL_ABS(right) );
	  RETURN;
     }
}
#endif

PP(pp_i_modulo)
{
     dSP; dATARGET; tryAMAGICbin(modulo,opASSIGN);
     {
	  dPOPTOPiirl;
	  if (!right)
	       DIE(aTHX_ "Illegal modulus zero");
	  /* The assumption is to use hereafter the old vanilla version... */
	  PL_op->op_ppaddr =
	       PL_ppaddr[OP_I_MODULO] =
	           &Perl_pp_i_modulo_0;
	  /* .. but if we have glibc, we might have a buggy _moddi3
	   * (at least glicb 2.2.5 is known to have this bug), in other
	   * words our integer modulus with negative quad as the second
	   * argument might be broken.  Test for this and re-patch the
	   * opcode dispatch table if that is the case, remembering to
	   * also apply the workaround so that this first round works
	   * right, too.  See [perl #9402] for more information. */
#if defined(__GLIBC__) && IVSIZE == 8
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
#endif
	  SETi( left % right );
	  RETURN;
     }
}

PP(pp_i_add)
{
    dSP; dATARGET; tryAMAGICbin(add,opASSIGN);
    {
      dPOPTOPiirl_ul;
      SETi( left + right );
      RETURN;
    }
}

PP(pp_i_subtract)
{
    dSP; dATARGET; tryAMAGICbin(subtr,opASSIGN);
    {
      dPOPTOPiirl_ul;
      SETi( left - right );
      RETURN;
    }
}

PP(pp_i_lt)
{
    dSP; tryAMAGICbinSET(lt,0);
    {
      dPOPTOPiirl;
      SETs(boolSV(left < right));
      RETURN;
    }
}

PP(pp_i_gt)
{
    dSP; tryAMAGICbinSET(gt,0);
    {
      dPOPTOPiirl;
      SETs(boolSV(left > right));
      RETURN;
    }
}

PP(pp_i_le)
{
    dSP; tryAMAGICbinSET(le,0);
    {
      dPOPTOPiirl;
      SETs(boolSV(left <= right));
      RETURN;
    }
}

PP(pp_i_ge)
{
    dSP; tryAMAGICbinSET(ge,0);
    {
      dPOPTOPiirl;
      SETs(boolSV(left >= right));
      RETURN;
    }
}

PP(pp_i_eq)
{
    dSP; tryAMAGICbinSET(eq,0);
    {
      dPOPTOPiirl;
      SETs(boolSV(left == right));
      RETURN;
    }
}

PP(pp_i_ne)
{
    dSP; tryAMAGICbinSET(ne,0);
    {
      dPOPTOPiirl;
      SETs(boolSV(left != right));
      RETURN;
    }
}

PP(pp_i_ncmp)
{
    dSP; dTARGET; tryAMAGICbin(ncmp,0);
    {
      dPOPTOPiirl;
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
    dSP; dTARGET; tryAMAGICun(neg);
    SETi(-TOPi);
    RETURN;
}

/* High falutin' math. */

PP(pp_atan2)
{
    dSP; dTARGET; tryAMAGICbin(atan2,0);
    {
      dPOPTOPnnrl;
      SETn(Perl_atan2(left, right));
      RETURN;
    }
}

PP(pp_sin)
{
    dSP; dTARGET; tryAMAGICun(sin);
    {
      NV value;
      value = POPn;
      value = Perl_sin(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_cos)
{
    dSP; dTARGET; tryAMAGICun(cos);
    {
      NV value;
      value = POPn;
      value = Perl_cos(value);
      XPUSHn(value);
      RETURN;
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

#ifndef HAS_DRAND48_PROTO
extern double drand48 (void);
#endif

PP(pp_rand)
{
    dSP; dTARGET;
    NV value;
    if (MAXARG < 1)
	value = 1.0;
    else
	value = POPn;
    if (value == 0.0)
	value = 1.0;
    if (!PL_srand_called) {
	(void)seedDrand01((Rand_seed_t)seed());
	PL_srand_called = TRUE;
    }
    value *= Drand01();
    XPUSHn(value);
    RETURN;
}

PP(pp_srand)
{
    dSP;
    UV anum;
    if (MAXARG < 1)
	anum = seed();
    else
	anum = POPu;
    (void)seedDrand01((Rand_seed_t)anum);
    PL_srand_called = TRUE;
    EXTEND(SP, 1);
    RETPUSHYES;
}

PP(pp_exp)
{
    dSP; dTARGET; tryAMAGICun(exp);
    {
      NV value;
      value = POPn;
      value = Perl_exp(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_log)
{
    dSP; dTARGET; tryAMAGICun(log);
    {
      NV value;
      value = POPn;
      if (value <= 0.0) {
	SET_NUMERIC_STANDARD();
	DIE(aTHX_ "Can't take log of %"NVgf, value);
      }
      value = Perl_log(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_sqrt)
{
    dSP; dTARGET; tryAMAGICun(sqrt);
    {
      NV value;
      value = POPn;
      if (value < 0.0) {
	SET_NUMERIC_STANDARD();
	DIE(aTHX_ "Can't take sqrt of %"NVgf, value);
      }
      value = Perl_sqrt(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_int)
{
    dSP; dTARGET; tryAMAGICun(int);
    {
      NV value;
      IV iv = TOPi; /* attempt to convert to IV if possible. */
      /* XXX it's arguable that compiler casting to IV might be subtly
	 different from modf (for numbers inside (IV_MIN,UV_MAX)) in which
	 else preferring IV has introduced a subtle behaviour change bug. OTOH
	 relying on floating point to be accurate is a bug.  */

      if (!SvOK(TOPs))
        SETu(0);
      else if (SvIOK(TOPs)) {
	if (SvIsUV(TOPs)) {
	    UV uv = TOPu;
	    SETu(uv);
	} else
	    SETi(iv);
      } else {
	  value = TOPn;
	  if (value >= 0.0) {
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
    RETURN;
}

PP(pp_abs)
{
    dSP; dTARGET; tryAMAGICun(abs);
    {
      /* This will cache the NV value if string isn't actually integer  */
      IV iv = TOPi;

      if (!SvOK(TOPs))
        SETu(0);
      else if (SvIOK(TOPs)) {
	/* IVX is precise  */
	if (SvIsUV(TOPs)) {
	  SETu(TOPu);	/* force it to be numeric only */
	} else {
	  if (iv >= 0) {
	    SETi(iv);
	  } else {
	    if (iv != IV_MIN) {
	      SETi(-iv);
	    } else {
	      /* 2s complement assumption. Also, not really needed as
		 IV_MIN and -IV_MIN should both be %100...00 and NV-able  */
	      SETu(IV_MIN);
	    }
	  }
	}
      } else{
	NV value = TOPn;
	if (value < 0.0)
	  value = -value;
	SETn(value);
      }
    }
    RETURN;
}


PP(pp_hex)
{
    dSP; dTARGET;
    char *tmps;
    I32 flags = PERL_SCAN_ALLOW_UNDERSCORES;
    STRLEN len;
    NV result_nv;
    UV result_uv;
    SV* sv = POPs;

    tmps = (SvPVx(sv, len));
    if (DO_UTF8(sv)) {
	 /* If Unicode, try to downgrade
	  * If not possible, croak. */
         SV* tsv = sv_2mortal(newSVsv(sv));
	
	 SvUTF8_on(tsv);
	 sv_utf8_downgrade(tsv, FALSE);
	 tmps = SvPVX(tsv);
    }
    result_uv = grok_hex (tmps, &len, &flags, &result_nv);
    if (flags & PERL_SCAN_GREATER_THAN_UV_MAX) {
        XPUSHn(result_nv);
    }
    else {
        XPUSHu(result_uv);
    }
    RETURN;
}

PP(pp_oct)
{
    dSP; dTARGET;
    char *tmps;
    I32 flags = PERL_SCAN_ALLOW_UNDERSCORES;
    STRLEN len;
    NV result_nv;
    UV result_uv;
    SV* sv = POPs;

    tmps = (SvPVx(sv, len));
    if (DO_UTF8(sv)) {
	 /* If Unicode, try to downgrade
	  * If not possible, croak. */
         SV* tsv = sv_2mortal(newSVsv(sv));
	
	 SvUTF8_on(tsv);
	 sv_utf8_downgrade(tsv, FALSE);
	 tmps = SvPVX(tsv);
    }
    while (*tmps && len && isSPACE(*tmps))
        tmps++, len--;
    if (*tmps == '0')
        tmps++, len--;
    if (*tmps == 'x')
        result_uv = grok_hex (tmps, &len, &flags, &result_nv);
    else if (*tmps == 'b')
        result_uv = grok_bin (tmps, &len, &flags, &result_nv);
    else
        result_uv = grok_oct (tmps, &len, &flags, &result_nv);

    if (flags & PERL_SCAN_GREATER_THAN_UV_MAX) {
        XPUSHn(result_nv);
    }
    else {
        XPUSHu(result_uv);
    }
    RETURN;
}

/* String stuff. */

PP(pp_length)
{
    dSP; dTARGET;
    SV *sv = TOPs;

    if (DO_UTF8(sv))
	SETi(sv_len_utf8(sv));
    else
	SETi(sv_len(sv));
    RETURN;
}

PP(pp_substr)
{
    dSP; dTARGET;
    SV *sv;
    I32 len = 0;
    STRLEN curlen;
    STRLEN utf8_curlen;
    I32 pos;
    I32 rem;
    I32 fail;
    I32 lvalue = PL_op->op_flags & OPf_MOD || LVRET;
    char *tmps;
    I32 arybase = PL_curcop->cop_arybase;
    SV *repl_sv = NULL;
    char *repl = 0;
    STRLEN repl_len;
    int num_args = PL_op->op_private & 7;
    bool repl_need_utf8_upgrade = FALSE;
    bool repl_is_utf8 = FALSE;

    SvTAINTED_off(TARG);			/* decontaminate */
    SvUTF8_off(TARG);				/* decontaminate */
    if (num_args > 2) {
	if (num_args > 3) {
	    repl_sv = POPs;
	    repl = SvPV(repl_sv, repl_len);
	    repl_is_utf8 = DO_UTF8(repl_sv) && SvCUR(repl_sv);
	}
	len = POPi;
    }
    pos = POPi;
    sv = POPs;
    PUTBACK;
    if (repl_sv) {
	if (repl_is_utf8) {
	    if (!DO_UTF8(sv))
		sv_utf8_upgrade(sv);
	}
	else if (DO_UTF8(sv))
	    repl_need_utf8_upgrade = TRUE;
    }
    tmps = SvPV(sv, curlen);
    if (DO_UTF8(sv)) {
        utf8_curlen = sv_len_utf8(sv);
	if (utf8_curlen == curlen)
	    utf8_curlen = 0;
	else
	    curlen = utf8_curlen;
    }
    else
	utf8_curlen = 0;

    if (pos >= arybase) {
	pos -= arybase;
	rem = curlen-pos;
	fail = rem;
	if (num_args > 2) {
	    if (len < 0) {
		rem += len;
		if (rem < 0)
		    rem = 0;
	    }
	    else if (rem > len)
		     rem = len;
	}
    }
    else {
	pos += curlen;
	if (num_args < 3)
	    rem = curlen;
	else if (len >= 0) {
	    rem = pos+len;
	    if (rem > (I32)curlen)
		rem = curlen;
	}
	else {
	    rem = curlen+len;
	    if (rem < pos)
		rem = pos;
	}
	if (pos < 0)
	    pos = 0;
	fail = rem;
	rem -= pos;
    }
    if (fail < 0) {
	if (lvalue || repl)
	    Perl_croak(aTHX_ "substr outside of string");
	if (ckWARN(WARN_SUBSTR))
	    Perl_warner(aTHX_ packWARN(WARN_SUBSTR), "substr outside of string");
	RETPUSHUNDEF;
    }
    else {
	I32 upos = pos;
	I32 urem = rem;
	if (utf8_curlen)
	    sv_pos_u2b(sv, &pos, &rem);
	tmps += pos;
	/* we either return a PV or an LV. If the TARG hasn't been used
	 * before, or is of that type, reuse it; otherwise use a mortal
	 * instead. Note that LVs can have an extended lifetime, so also
	 * dont reuse if refcount > 1 (bug #20933) */
	if (SvTYPE(TARG) > SVt_NULL) {
	    if ( (SvTYPE(TARG) == SVt_PVLV)
		    ? (!lvalue || SvREFCNT(TARG) > 1)
		    : lvalue)
	    {
		TARG = sv_newmortal();
	    }
	}

	sv_setpvn(TARG, tmps, rem);
#ifdef USE_LOCALE_COLLATE
	sv_unmagic(TARG, PERL_MAGIC_collxfrm);
#endif
	if (utf8_curlen)
	    SvUTF8_on(TARG);
	if (repl) {
	    SV* repl_sv_copy = NULL;

	    if (repl_need_utf8_upgrade) {
		repl_sv_copy = newSVsv(repl_sv);
		sv_utf8_upgrade(repl_sv_copy);
		repl = SvPV(repl_sv_copy, repl_len);
		repl_is_utf8 = DO_UTF8(repl_sv_copy) && SvCUR(sv);
	    }
	    sv_insert(sv, pos, rem, repl, repl_len);
	    if (repl_is_utf8)
		SvUTF8_on(sv);
	    if (repl_sv_copy)
		SvREFCNT_dec(repl_sv_copy);
	}
	else if (lvalue) {		/* it's an lvalue! */
	    if (!SvGMAGICAL(sv)) {
		if (SvROK(sv)) {
		    STRLEN n_a;
		    SvPV_force(sv,n_a);
		    if (ckWARN(WARN_SUBSTR))
			Perl_warner(aTHX_ packWARN(WARN_SUBSTR),
				"Attempt to use reference as lvalue in substr");
		}
		if (SvOK(sv))		/* is it defined ? */
		    (void)SvPOK_only_UTF8(sv);
		else
		    sv_setpvn(sv,"",0);	/* avoid lexical reincarnation */
	    }

	    if (SvTYPE(TARG) < SVt_PVLV) {
		sv_upgrade(TARG, SVt_PVLV);
		sv_magic(TARG, Nullsv, PERL_MAGIC_substr, Nullch, 0);
	    }
	    else
		(void)SvOK_off(TARG);

	    LvTYPE(TARG) = 'x';
	    if (LvTARG(TARG) != sv) {
		if (LvTARG(TARG))
		    SvREFCNT_dec(LvTARG(TARG));
		LvTARG(TARG) = SvREFCNT_inc(sv);
	    }
	    LvTARGOFF(TARG) = upos;
	    LvTARGLEN(TARG) = urem;
	}
    }
    SPAGAIN;
    PUSHs(TARG);		/* avoid SvSETMAGIC here */
    RETURN;
}

PP(pp_vec)
{
    dSP; dTARGET;
    register IV size   = POPi;
    register IV offset = POPi;
    register SV *src = POPs;
    I32 lvalue = PL_op->op_flags & OPf_MOD || LVRET;

    SvTAINTED_off(TARG);		/* decontaminate */
    if (lvalue) {			/* it's an lvalue! */
	if (SvREFCNT(TARG) > 1)	/* don't share the TARG (#20933) */
	    TARG = sv_newmortal();
	if (SvTYPE(TARG) < SVt_PVLV) {
	    sv_upgrade(TARG, SVt_PVLV);
	    sv_magic(TARG, Nullsv, PERL_MAGIC_vec, Nullch, 0);
	}
	LvTYPE(TARG) = 'v';
	if (LvTARG(TARG) != src) {
	    if (LvTARG(TARG))
		SvREFCNT_dec(LvTARG(TARG));
	    LvTARG(TARG) = SvREFCNT_inc(src);
	}
	LvTARGOFF(TARG) = offset;
	LvTARGLEN(TARG) = size;
    }

    sv_setuv(TARG, do_vecget(src, offset, size));
    PUSHs(TARG);
    RETURN;
}

PP(pp_index)
{
    dSP; dTARGET;
    SV *big;
    SV *little;
    I32 offset;
    I32 retval;
    char *tmps;
    char *tmps2;
    STRLEN biglen;
    I32 arybase = PL_curcop->cop_arybase;

    if (MAXARG < 3)
	offset = 0;
    else
	offset = POPi - arybase;
    little = POPs;
    big = POPs;
    tmps = SvPV(big, biglen);
    if (offset > 0 && DO_UTF8(big))
	sv_pos_u2b(big, &offset, 0);
    if (offset < 0)
	offset = 0;
    else if (offset > (I32)biglen)
	offset = biglen;
    if (!(tmps2 = fbm_instr((unsigned char*)tmps + offset,
      (unsigned char*)tmps + biglen, little, 0)))
	retval = -1;
    else
	retval = tmps2 - tmps;
    if (retval > 0 && DO_UTF8(big))
	sv_pos_b2u(big, &retval);
    PUSHi(retval + arybase);
    RETURN;
}

PP(pp_rindex)
{
    dSP; dTARGET;
    SV *big;
    SV *little;
    STRLEN blen;
    STRLEN llen;
    I32 offset;
    I32 retval;
    char *tmps;
    char *tmps2;
    I32 arybase = PL_curcop->cop_arybase;

    if (MAXARG >= 3)
	offset = POPi;
    little = POPs;
    big = POPs;
    tmps2 = SvPV(little, llen);
    tmps = SvPV(big, blen);
    if (MAXARG < 3)
	offset = blen;
    else {
	if (offset > 0 && DO_UTF8(big))
	    sv_pos_u2b(big, &offset, 0);
	offset = offset - arybase + llen;
    }
    if (offset < 0)
	offset = 0;
    else if (offset > (I32)blen)
	offset = blen;
    if (!(tmps2 = rninstr(tmps,  tmps  + offset,
			  tmps2, tmps2 + llen)))
	retval = -1;
    else
	retval = tmps2 - tmps;
    if (retval > 0 && DO_UTF8(big))
	sv_pos_b2u(big, &retval);
    PUSHi(retval + arybase);
    RETURN;
}

PP(pp_sprintf)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    do_sprintf(TARG, SP-MARK, MARK+1);
    TAINT_IF(SvTAINTED(TARG));
    if (DO_UTF8(*(MARK+1)))
	SvUTF8_on(TARG);
    SP = ORIGMARK;
    PUSHTARG;
    RETURN;
}

PP(pp_ord)
{
    dSP; dTARGET;
    SV *argsv = POPs;
    STRLEN len;
    U8 *s = (U8*)SvPVx(argsv, len);
    SV *tmpsv;

    if (PL_encoding && SvPOK(argsv) && !DO_UTF8(argsv)) {
        tmpsv = sv_2mortal(newSVsv(argsv));
        s = (U8*)sv_recode_to_utf8(tmpsv, PL_encoding);
        argsv = tmpsv;
    }

    XPUSHu(DO_UTF8(argsv) ?
	   utf8n_to_uvchr(s, UTF8_MAXLEN, 0, UTF8_ALLOW_ANYUV) :
	   (*s & 0xff));

    RETURN;
}

PP(pp_chr)
{
    dSP; dTARGET;
    char *tmps;
    UV value = POPu;

    (void)SvUPGRADE(TARG,SVt_PV);

    if (value > 255 && !IN_BYTES) {
	SvGROW(TARG, (STRLEN)UNISKIP(value)+1);
	tmps = (char*)uvchr_to_utf8_flags((U8*)SvPVX(TARG), value, 0);
	SvCUR_set(TARG, tmps - SvPVX(TARG));
	*tmps = '\0';
	(void)SvPOK_only(TARG);
	SvUTF8_on(TARG);
	XPUSHs(TARG);
	RETURN;
    }

    SvGROW(TARG,2);
    SvCUR_set(TARG, 1);
    tmps = SvPVX(TARG);
    *tmps++ = (char)value;
    *tmps = '\0';
    (void)SvPOK_only(TARG);
    if (PL_encoding && !IN_BYTES) {
        sv_recode_to_utf8(TARG, PL_encoding);
	tmps = SvPVX(TARG);
	if (SvCUR(TARG) == 0 || !is_utf8_string((U8*)tmps, SvCUR(TARG)) ||
	    memEQ(tmps, "\xef\xbf\xbd\0", 4)) {
	    SvGROW(TARG, 3);
	    tmps = SvPVX(TARG);
	    SvCUR_set(TARG, 2);
	    *tmps++ = (U8)UTF8_EIGHT_BIT_HI(value);
	    *tmps++ = (U8)UTF8_EIGHT_BIT_LO(value);
	    *tmps = '\0';
	    SvUTF8_on(TARG);
	}
    }
    XPUSHs(TARG);
    RETURN;
}

PP(pp_crypt)
{
    dSP; dTARGET;
#ifdef HAS_CRYPT
    dPOPTOPssrl;
    STRLEN n_a;
    STRLEN len;
    char *tmps = SvPV(left, len);

    if (DO_UTF8(left)) {
         /* If Unicode, try to downgrade.
	  * If not possible, croak.
	  * Yes, we made this up.  */
         SV* tsv = sv_2mortal(newSVsv(left));

	 SvUTF8_on(tsv);
	 sv_utf8_downgrade(tsv, FALSE);
	 tmps = SvPVX(tsv);
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
    sv_setpv(TARG, fcrypt(tmps, SvPV(right, n_a)));
#   else
    sv_setpv(TARG, PerlProc_crypt(tmps, SvPV(right, n_a)));
#   endif
    SETs(TARG);
    RETURN;
#else
    DIE(aTHX_
      "The crypt() function is unimplemented due to excessive paranoia.");
#endif
}

PP(pp_ucfirst)
{
    dSP;
    SV *sv = TOPs;
    register U8 *s;
    STRLEN slen;

    SvGETMAGIC(sv);
    if (DO_UTF8(sv) &&
	(s = (U8*)SvPV_nomg(sv, slen)) && slen &&
	UTF8_IS_START(*s)) {
	U8 tmpbuf[UTF8_MAXLEN_UCLC+1];
	STRLEN ulen;
	STRLEN tculen;

	utf8_to_uvchr(s, &ulen);
	toTITLE_utf8(s, tmpbuf, &tculen);
	utf8_to_uvchr(tmpbuf, 0);

	if (!SvPADTMP(sv) || SvREADONLY(sv)) {
	    dTARGET;
	    /* slen is the byte length of the whole SV.
	     * ulen is the byte length of the original Unicode character
	     * stored as UTF-8 at s.
	     * tculen is the byte length of the freshly titlecased
	     * Unicode character stored as UTF-8 at tmpbuf.
	     * We first set the result to be the titlecased character,
	     * and then append the rest of the SV data. */
	    sv_setpvn(TARG, (char*)tmpbuf, tculen);
	    if (slen > ulen)
	        sv_catpvn(TARG, (char*)(s + ulen), slen - ulen);
	    SvUTF8_on(TARG);
	    SETs(TARG);
	}
	else {
	    s = (U8*)SvPV_force_nomg(sv, slen);
	    Copy(tmpbuf, s, tculen, U8);
	}
    }
    else {
	if (!SvPADTMP(sv) || SvREADONLY(sv)) {
	    dTARGET;
	    SvUTF8_off(TARG);				/* decontaminate */
	    sv_setsv_nomg(TARG, sv);
	    sv = TARG;
	    SETs(sv);
	}
	s = (U8*)SvPV_force_nomg(sv, slen);
	if (*s) {
	    if (IN_LOCALE_RUNTIME) {
		TAINT;
		SvTAINTED_on(sv);
		*s = toUPPER_LC(*s);
	    }
	    else
		*s = toUPPER(*s);
	}
    }
    SvSETMAGIC(sv);
    RETURN;
}

PP(pp_lcfirst)
{
    dSP;
    SV *sv = TOPs;
    register U8 *s;
    STRLEN slen;

    SvGETMAGIC(sv);
    if (DO_UTF8(sv) &&
	(s = (U8*)SvPV_nomg(sv, slen)) && slen &&
	UTF8_IS_START(*s)) {
	STRLEN ulen;
	U8 tmpbuf[UTF8_MAXLEN_UCLC+1];
	U8 *tend;
	UV uv;

	toLOWER_utf8(s, tmpbuf, &ulen);
	uv = utf8_to_uvchr(tmpbuf, 0);
	tend = uvchr_to_utf8(tmpbuf, uv);

	if (!SvPADTMP(sv) || (STRLEN)(tend - tmpbuf) != ulen || SvREADONLY(sv)) {
	    dTARGET;
	    sv_setpvn(TARG, (char*)tmpbuf, tend - tmpbuf);
	    if (slen > ulen)
	        sv_catpvn(TARG, (char*)(s + ulen), slen - ulen);
	    SvUTF8_on(TARG);
	    SETs(TARG);
	}
	else {
	    s = (U8*)SvPV_force_nomg(sv, slen);
	    Copy(tmpbuf, s, ulen, U8);
	}
    }
    else {
	if (!SvPADTMP(sv) || SvREADONLY(sv)) {
	    dTARGET;
	    SvUTF8_off(TARG);				/* decontaminate */
	    sv_setsv_nomg(TARG, sv);
	    sv = TARG;
	    SETs(sv);
	}
	s = (U8*)SvPV_force_nomg(sv, slen);
	if (*s) {
	    if (IN_LOCALE_RUNTIME) {
		TAINT;
		SvTAINTED_on(sv);
		*s = toLOWER_LC(*s);
	    }
	    else
		*s = toLOWER(*s);
	}
    }
    SvSETMAGIC(sv);
    RETURN;
}

PP(pp_uc)
{
    dSP;
    SV *sv = TOPs;
    register U8 *s;
    STRLEN len;

    SvGETMAGIC(sv);
    if (DO_UTF8(sv)) {
	dTARGET;
	STRLEN ulen;
	register U8 *d;
	U8 *send;
	U8 tmpbuf[UTF8_MAXLEN_UCLC+1];

	s = (U8*)SvPV_nomg(sv,len);
	if (!len) {
	    SvUTF8_off(TARG);				/* decontaminate */
	    sv_setpvn(TARG, "", 0);
	    SETs(TARG);
	}
	else {
	    STRLEN nchar = utf8_length(s, s + len);

	    (void)SvUPGRADE(TARG, SVt_PV);
	    SvGROW(TARG, (nchar * UTF8_MAXLEN_UCLC) + 1);
	    (void)SvPOK_only(TARG);
	    d = (U8*)SvPVX(TARG);
	    send = s + len;
	    while (s < send) {
		toUPPER_utf8(s, tmpbuf, &ulen);
		Copy(tmpbuf, d, ulen, U8);
		d += ulen;
		s += UTF8SKIP(s);
	    }
	    *d = '\0';
	    SvUTF8_on(TARG);
	    SvCUR_set(TARG, d - (U8*)SvPVX(TARG));
	    SETs(TARG);
	}
    }
    else {
	if (!SvPADTMP(sv) || SvREADONLY(sv)) {
	    dTARGET;
	    SvUTF8_off(TARG);				/* decontaminate */
	    sv_setsv_nomg(TARG, sv);
	    sv = TARG;
	    SETs(sv);
	}
	s = (U8*)SvPV_force_nomg(sv, len);
	if (len) {
	    register U8 *send = s + len;

	    if (IN_LOCALE_RUNTIME) {
		TAINT;
		SvTAINTED_on(sv);
		for (; s < send; s++)
		    *s = toUPPER_LC(*s);
	    }
	    else {
		for (; s < send; s++)
		    *s = toUPPER(*s);
	    }
	}
    }
    SvSETMAGIC(sv);
    RETURN;
}

PP(pp_lc)
{
    dSP;
    SV *sv = TOPs;
    register U8 *s;
    STRLEN len;

    SvGETMAGIC(sv);
    if (DO_UTF8(sv)) {
	dTARGET;
	STRLEN ulen;
	register U8 *d;
	U8 *send;
	U8 tmpbuf[UTF8_MAXLEN_UCLC+1];

	s = (U8*)SvPV_nomg(sv,len);
	if (!len) {
	    SvUTF8_off(TARG);				/* decontaminate */
	    sv_setpvn(TARG, "", 0);
	    SETs(TARG);
	}
	else {
	    STRLEN nchar = utf8_length(s, s + len);

	    (void)SvUPGRADE(TARG, SVt_PV);
	    SvGROW(TARG, (nchar * UTF8_MAXLEN_UCLC) + 1);
	    (void)SvPOK_only(TARG);
	    d = (U8*)SvPVX(TARG);
	    send = s + len;
	    while (s < send) {
		UV uv = toLOWER_utf8(s, tmpbuf, &ulen);
#define GREEK_CAPITAL_LETTER_SIGMA 0x03A3 /* Unicode */
		if (uv == GREEK_CAPITAL_LETTER_SIGMA) {
		     /*
		      * Now if the sigma is NOT followed by
		      * /$ignorable_sequence$cased_letter/;
		      * and it IS preceded by
		      * /$cased_letter$ignorable_sequence/;
		      * where $ignorable_sequence is
		      * [\x{2010}\x{AD}\p{Mn}]*
		      * and $cased_letter is
		      * [\p{Ll}\p{Lo}\p{Lt}]
		      * then it should be mapped to 0x03C2,
		      * (GREEK SMALL LETTER FINAL SIGMA),
		      * instead of staying 0x03A3.
		      * See lib/unicore/SpecCase.txt.
		      */
		}
		Copy(tmpbuf, d, ulen, U8);
		d += ulen;
		s += UTF8SKIP(s);
	    }
	    *d = '\0';
	    SvUTF8_on(TARG);
	    SvCUR_set(TARG, d - (U8*)SvPVX(TARG));
	    SETs(TARG);
	}
    }
    else {
	if (!SvPADTMP(sv) || SvREADONLY(sv)) {
	    dTARGET;
	    SvUTF8_off(TARG);				/* decontaminate */
	    sv_setsv_nomg(TARG, sv);
	    sv = TARG;
	    SETs(sv);
	}

	s = (U8*)SvPV_force_nomg(sv, len);
	if (len) {
	    register U8 *send = s + len;

	    if (IN_LOCALE_RUNTIME) {
		TAINT;
		SvTAINTED_on(sv);
		for (; s < send; s++)
		    *s = toLOWER_LC(*s);
	    }
	    else {
		for (; s < send; s++)
		    *s = toLOWER(*s);
	    }
	}
    }
    SvSETMAGIC(sv);
    RETURN;
}

PP(pp_quotemeta)
{
    dSP; dTARGET;
    SV *sv = TOPs;
    STRLEN len;
    register char *s = SvPV(sv,len);
    register char *d;

    SvUTF8_off(TARG);				/* decontaminate */
    if (len) {
	(void)SvUPGRADE(TARG, SVt_PV);
	SvGROW(TARG, (len * 2) + 1);
	d = SvPVX(TARG);
	if (DO_UTF8(sv)) {
	    while (len) {
		if (UTF8_IS_CONTINUED(*s)) {
		    STRLEN ulen = UTF8SKIP(s);
		    if (ulen > len)
			ulen = len;
		    len -= ulen;
		    while (ulen--)
			*d++ = *s++;
		}
		else {
		    if (!isALNUM(*s))
			*d++ = '\\';
		    *d++ = *s++;
		    len--;
		}
	    }
	    SvUTF8_on(TARG);
	}
	else {
	    while (len--) {
		if (!isALNUM(*s))
		    *d++ = '\\';
		*d++ = *s++;
	    }
	}
	*d = '\0';
	SvCUR_set(TARG, d - SvPVX(TARG));
	(void)SvPOK_only_UTF8(TARG);
    }
    else
	sv_setpvn(TARG, s, len);
    SETs(TARG);
    if (SvSMAGICAL(TARG))
	mg_set(TARG);
    RETURN;
}

/* Arrays. */

PP(pp_aslice)
{
    dSP; dMARK; dORIGMARK;
    register SV** svp;
    register AV* av = (AV*)POPs;
    register I32 lval = (PL_op->op_flags & OPf_MOD || LVRET);
    I32 arybase = PL_curcop->cop_arybase;
    I32 elem;

    if (SvTYPE(av) == SVt_PVAV) {
	if (lval && PL_op->op_private & OPpLVAL_INTRO) {
	    I32 max = -1;
	    for (svp = MARK + 1; svp <= SP; svp++) {
		elem = SvIVx(*svp);
		if (elem > max)
		    max = elem;
	    }
	    if (max > AvMAX(av))
		av_extend(av, max);
	}
	while (++MARK <= SP) {
	    elem = SvIVx(*MARK);

	    if (elem > 0)
		elem -= arybase;
	    svp = av_fetch(av, elem, lval);
	    if (lval) {
		if (!svp || *svp == &PL_sv_undef)
		    DIE(aTHX_ PL_no_aelem, elem);
		if (PL_op->op_private & OPpLVAL_INTRO)
		    save_aelem(av, elem, svp);
	    }
	    *MARK = svp ? *svp : &PL_sv_undef;
	}
    }
    if (GIMME != G_ARRAY) {
	MARK = ORIGMARK;
	*++MARK = *SP;
	SP = MARK;
    }
    RETURN;
}

/* Associative arrays. */

PP(pp_each)
{
    dSP;
    HV *hash = (HV*)POPs;
    HE *entry;
    I32 gimme = GIMME_V;
    I32 realhv = (SvTYPE(hash) == SVt_PVHV);

    PUTBACK;
    /* might clobber stack_sp */
    entry = realhv ? hv_iternext(hash) : avhv_iternext((AV*)hash);
    SPAGAIN;

    EXTEND(SP, 2);
    if (entry) {
        SV* sv = hv_iterkeysv(entry);
	PUSHs(sv);	/* won't clobber stack_sp */
	if (gimme == G_ARRAY) {
	    SV *val;
	    PUTBACK;
	    /* might clobber stack_sp */
	    val = realhv ?
		  hv_iterval(hash, entry) : avhv_iterval((AV*)hash, entry);
	    SPAGAIN;
	    PUSHs(val);
	}
    }
    else if (gimme == G_SCALAR)
	RETPUSHUNDEF;

    RETURN;
}

PP(pp_values)
{
    return do_kv();
}

PP(pp_keys)
{
    return do_kv();
}

PP(pp_delete)
{
    dSP;
    I32 gimme = GIMME_V;
    I32 discard = (gimme == G_VOID) ? G_DISCARD : 0;
    SV *sv;
    HV *hv;

    if (PL_op->op_private & OPpSLICE) {
	dMARK; dORIGMARK;
	U32 hvtype;
	hv = (HV*)POPs;
	hvtype = SvTYPE(hv);
	if (hvtype == SVt_PVHV) {			/* hash element */
	    while (++MARK <= SP) {
		sv = hv_delete_ent(hv, *MARK, discard, 0);
		*MARK = sv ? sv : &PL_sv_undef;
	    }
	}
	else if (hvtype == SVt_PVAV) {
	    if (PL_op->op_flags & OPf_SPECIAL) {	/* array element */
		while (++MARK <= SP) {
		    sv = av_delete((AV*)hv, SvIV(*MARK), discard);
		    *MARK = sv ? sv : &PL_sv_undef;
		}
	    }
	    else {					/* pseudo-hash element */
		while (++MARK <= SP) {
		    sv = avhv_delete_ent((AV*)hv, *MARK, discard, 0);
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
	hv = (HV*)POPs;
	if (SvTYPE(hv) == SVt_PVHV)
	    sv = hv_delete_ent(hv, keysv, discard, 0);
	else if (SvTYPE(hv) == SVt_PVAV) {
	    if (PL_op->op_flags & OPf_SPECIAL)
		sv = av_delete((AV*)hv, SvIV(keysv), discard);
	    else
		sv = avhv_delete_ent((AV*)hv, keysv, discard, 0);
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

    if (PL_op->op_private & OPpEXISTS_SUB) {
	GV *gv;
	CV *cv;
	SV *sv = POPs;
	cv = sv_2cv(sv, &hv, &gv, FALSE);
	if (cv)
	    RETPUSHYES;
	if (gv && isGV(gv) && GvCV(gv) && !GvCVGEN(gv))
	    RETPUSHYES;
	RETPUSHNO;
    }
    tmpsv = POPs;
    hv = (HV*)POPs;
    if (SvTYPE(hv) == SVt_PVHV) {
	if (hv_exists_ent(hv, tmpsv, 0))
	    RETPUSHYES;
    }
    else if (SvTYPE(hv) == SVt_PVAV) {
	if (PL_op->op_flags & OPf_SPECIAL) {		/* array element */
	    if (av_exists((AV*)hv, SvIV(tmpsv)))
		RETPUSHYES;
	}
	else if (avhv_exists_ent((AV*)hv, tmpsv, 0))	/* pseudo-hash element */
	    RETPUSHYES;
    }
    else {
	DIE(aTHX_ "Not a HASH reference");
    }
    RETPUSHNO;
}

PP(pp_hslice)
{
    dSP; dMARK; dORIGMARK;
    register HV *hv = (HV*)POPs;
    register I32 lval = (PL_op->op_flags & OPf_MOD || LVRET);
    I32 realhv = (SvTYPE(hv) == SVt_PVHV);
    bool localizing = PL_op->op_private & OPpLVAL_INTRO ? TRUE : FALSE;
    bool other_magic = FALSE;

    if (localizing) {
        MAGIC *mg;
        HV *stash;

        other_magic = mg_find((SV*)hv, PERL_MAGIC_env) ||
            ((mg = mg_find((SV*)hv, PERL_MAGIC_tied))
             /* Try to preserve the existenceness of a tied hash
              * element by using EXISTS and DELETE if possible.
              * Fallback to FETCH and STORE otherwise */
             && (stash = SvSTASH(SvRV(SvTIED_obj((SV*)hv, mg))))
             && gv_fetchmethod_autoload(stash, "EXISTS", TRUE)
             && gv_fetchmethod_autoload(stash, "DELETE", TRUE));
    }

    if (!realhv && localizing)
	DIE(aTHX_ "Can't localize pseudo-hash element");

    if (realhv || SvTYPE(hv) == SVt_PVAV) {
	while (++MARK <= SP) {
	    SV *keysv = *MARK;
	    SV **svp;
	    bool preeminent = FALSE;

            if (localizing) {
                preeminent = SvRMAGICAL(hv) && !other_magic ? 1 :
                    realhv ? hv_exists_ent(hv, keysv, 0)
                    : avhv_exists_ent((AV*)hv, keysv, 0);
            }

	    if (realhv) {
		HE *he = hv_fetch_ent(hv, keysv, lval, 0);
		svp = he ? &HeVAL(he) : 0;
	    }
	    else {
		svp = avhv_fetch_ent((AV*)hv, keysv, lval, 0);
	    }
	    if (lval) {
		if (!svp || *svp == &PL_sv_undef) {
		    STRLEN n_a;
		    DIE(aTHX_ PL_no_helem, SvPV(keysv, n_a));
		}
		if (localizing) {
		    if (preeminent)
		        save_helem(hv, keysv, svp);
		    else {
			STRLEN keylen;
			char *key = SvPV(keysv, keylen);
			SAVEDELETE(hv, savepvn(key,keylen), keylen);
		    }
                }
	    }
	    *MARK = svp ? *svp : &PL_sv_undef;
	}
    }
    if (GIMME != G_ARRAY) {
	MARK = ORIGMARK;
	*++MARK = *SP;
	SP = MARK;
    }
    RETURN;
}

/* List operators. */

PP(pp_list)
{
    dSP; dMARK;
    if (GIMME != G_ARRAY) {
	if (++MARK <= SP)
	    *MARK = *SP;		/* unwanted list, return last item */
	else
	    *MARK = &PL_sv_undef;
	SP = MARK;
    }
    RETURN;
}

PP(pp_lslice)
{
    dSP;
    SV **lastrelem = PL_stack_sp;
    SV **lastlelem = PL_stack_base + POPMARK;
    SV **firstlelem = PL_stack_base + POPMARK + 1;
    register SV **firstrelem = lastlelem + 1;
    I32 arybase = PL_curcop->cop_arybase;
    I32 lval = PL_op->op_flags & OPf_MOD;
    I32 is_something_there = lval;

    register I32 max = lastrelem - lastlelem;
    register SV **lelem;
    register I32 ix;

    if (GIMME != G_ARRAY) {
	ix = SvIVx(*lastlelem);
	if (ix < 0)
	    ix += max;
	else
	    ix -= arybase;
	if (ix < 0 || ix >= max)
	    *firstlelem = &PL_sv_undef;
	else
	    *firstlelem = firstrelem[ix];
	SP = firstlelem;
	RETURN;
    }

    if (max == 0) {
	SP = firstlelem - 1;
	RETURN;
    }

    for (lelem = firstlelem; lelem <= lastlelem; lelem++) {
	ix = SvIVx(*lelem);
	if (ix < 0)
	    ix += max;
	else
	    ix -= arybase;
	if (ix < 0 || ix >= max)
	    *lelem = &PL_sv_undef;
	else {
	    is_something_there = TRUE;
	    if (!(*lelem = firstrelem[ix]))
		*lelem = &PL_sv_undef;
	}
    }
    if (is_something_there)
	SP = lastlelem;
    else
	SP = firstlelem - 1;
    RETURN;
}

PP(pp_anonlist)
{
    dSP; dMARK; dORIGMARK;
    I32 items = SP - MARK;
    SV *av = sv_2mortal((SV*)av_make(items, MARK+1));
    SP = ORIGMARK;		/* av_make() might realloc stack_sp */
    XPUSHs(av);
    RETURN;
}

PP(pp_anonhash)
{
    dSP; dMARK; dORIGMARK;
    HV* hv = (HV*)sv_2mortal((SV*)newHV());

    while (MARK < SP) {
	SV* key = *++MARK;
	SV *val = NEWSV(46, 0);
	if (MARK < SP)
	    sv_setsv(val, *++MARK);
	else if (ckWARN(WARN_MISC))
	    Perl_warner(aTHX_ packWARN(WARN_MISC), "Odd number of elements in anonymous hash");
	(void)hv_store_ent(hv,key,val,0);
    }
    SP = ORIGMARK;
    XPUSHs((SV*)hv);
    RETURN;
}

PP(pp_splice)
{
    dSP; dMARK; dORIGMARK;
    register AV *ary = (AV*)*++MARK;
    register SV **src;
    register SV **dst;
    register I32 i;
    register I32 offset;
    register I32 length;
    I32 newlen;
    I32 after;
    I32 diff;
    SV **tmparyval = 0;
    MAGIC *mg;

    if ((mg = SvTIED_mg((SV*)ary, PERL_MAGIC_tied))) {
	*MARK-- = SvTIED_obj((SV*)ary, mg);
	PUSHMARK(MARK);
	PUTBACK;
	ENTER;
	call_method("SPLICE",GIMME_V);
	LEAVE;
	SPAGAIN;
	RETURN;
    }

    SP++;

    if (++MARK < SP) {
	offset = i = SvIVx(*MARK);
	if (offset < 0)
	    offset += AvFILLp(ary) + 1;
	else
	    offset -= PL_curcop->cop_arybase;
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
	if (ckWARN(WARN_MISC))
	    Perl_warner(aTHX_ packWARN(WARN_MISC), "splice() offset past end of array" );
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

    if (diff < 0) {				/* shrinking the area */
	if (newlen) {
	    New(451, tmparyval, newlen, SV*);	/* so remember insertion */
	    Copy(MARK, tmparyval, newlen, SV*);
	}

	MARK = ORIGMARK + 1;
	if (GIMME == G_ARRAY) {			/* copy return vals to stack */
	    MEXTEND(MARK, length);
	    Copy(AvARRAY(ary)+offset, MARK, length, SV*);
	    if (AvREAL(ary)) {
		EXTEND_MORTAL(length);
		for (i = length, dst = MARK; i; i--) {
		    sv_2mortal(*dst);	/* free them eventualy */
		    dst++;
		}
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
	    SvPVX(ary) = (char*)(AvARRAY(ary) - diff); /* diff is negative */
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
	    dst[--i] = &PL_sv_undef;
	
	if (newlen) {
	    for (src = tmparyval, dst = AvARRAY(ary) + offset;
	      newlen; newlen--) {
		*dst = NEWSV(46, 0);
		sv_setsv(*dst++, *src++);
	    }
	    Safefree(tmparyval);
	}
    }
    else {					/* no, expanding (or same) */
	if (length) {
	    New(452, tmparyval, length, SV*);	/* so remember deletion */
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
		SvPVX(ary) = (char*)(AvARRAY(ary) - diff);/* diff is positive */
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

	for (src = MARK, dst = AvARRAY(ary) + offset; newlen; newlen--) {
	    *dst = NEWSV(46, 0);
	    sv_setsv(*dst++, *src++);
	}
	MARK = ORIGMARK + 1;
	if (GIMME == G_ARRAY) {			/* copy return vals to stack */
	    if (length) {
		Copy(tmparyval, MARK, length, SV*);
		if (AvREAL(ary)) {
		    EXTEND_MORTAL(length);
		    for (i = length, dst = MARK; i; i--) {
			sv_2mortal(*dst);	/* free them eventualy */
			dst++;
		    }
		}
		Safefree(tmparyval);
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
	    Safefree(tmparyval);
	}
	else
	    *MARK = &PL_sv_undef;
    }
    SP = MARK;
    RETURN;
}

PP(pp_push)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    register AV *ary = (AV*)*++MARK;
    register SV *sv = &PL_sv_undef;
    MAGIC *mg;

    if ((mg = SvTIED_mg((SV*)ary, PERL_MAGIC_tied))) {
	*MARK-- = SvTIED_obj((SV*)ary, mg);
	PUSHMARK(MARK);
	PUTBACK;
	ENTER;
	call_method("PUSH",G_SCALAR|G_DISCARD);
	LEAVE;
	SPAGAIN;
    }
    else {
	/* Why no pre-extend of ary here ? */
	for (++MARK; MARK <= SP; MARK++) {
	    sv = NEWSV(51, 0);
	    if (*MARK)
		sv_setsv(sv, *MARK);
	    av_push(ary, sv);
	}
    }
    SP = ORIGMARK;
    PUSHi( AvFILL(ary) + 1 );
    RETURN;
}

PP(pp_pop)
{
    dSP;
    AV *av = (AV*)POPs;
    SV *sv = av_pop(av);
    if (AvREAL(av))
	(void)sv_2mortal(sv);
    PUSHs(sv);
    RETURN;
}

PP(pp_shift)
{
    dSP;
    AV *av = (AV*)POPs;
    SV *sv = av_shift(av);
    EXTEND(SP, 1);
    if (!sv)
	RETPUSHUNDEF;
    if (AvREAL(av))
	(void)sv_2mortal(sv);
    PUSHs(sv);
    RETURN;
}

PP(pp_unshift)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    register AV *ary = (AV*)*++MARK;
    register SV *sv;
    register I32 i = 0;
    MAGIC *mg;

    if ((mg = SvTIED_mg((SV*)ary, PERL_MAGIC_tied))) {
	*MARK-- = SvTIED_obj((SV*)ary, mg);
	PUSHMARK(MARK);
	PUTBACK;
	ENTER;
	call_method("UNSHIFT",G_SCALAR|G_DISCARD);
	LEAVE;
	SPAGAIN;
    }
    else {
	av_unshift(ary, SP - MARK);
	while (MARK < SP) {
	    sv = NEWSV(27, 0);
	    sv_setsv(sv, *++MARK);
	    (void)av_store(ary, i++, sv);
	}
    }
    SP = ORIGMARK;
    PUSHi( AvFILL(ary) + 1 );
    RETURN;
}

PP(pp_reverse)
{
    dSP; dMARK;
    register SV *tmp;
    SV **oldsp = SP;

    if (GIMME == G_ARRAY) {
	MARK++;
	while (MARK < SP) {
	    tmp = *MARK;
	    *MARK++ = *SP;
	    *SP-- = tmp;
	}
	/* safe as long as stack cannot get extended in the above */
	SP = oldsp;
    }
    else {
	register char *up;
	register char *down;
	register I32 tmp;
	dTARGET;
	STRLEN len;

	SvUTF8_off(TARG);				/* decontaminate */
	if (SP - MARK > 1)
	    do_join(TARG, &PL_sv_no, MARK, SP);
	else
	    sv_setsv(TARG, (SP > MARK) ? *SP : DEFSV);
	up = SvPV_force(TARG, len);
	if (len > 1) {
	    if (DO_UTF8(TARG)) {	/* first reverse each character */
		U8* s = (U8*)SvPVX(TARG);
		U8* send = (U8*)(s + len);
		while (s < send) {
		    if (UTF8_IS_INVARIANT(*s)) {
			s++;
			continue;
		    }
		    else {
			if (!utf8_to_uvchr(s, 0))
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
    AV *ary;
    register IV limit = POPi;			/* note, negative is forever */
    SV *sv = POPs;
    STRLEN len;
    register char *s = SvPV(sv, len);
    bool do_utf8 = DO_UTF8(sv);
    char *strend = s + len;
    register PMOP *pm;
    register REGEXP *rx;
    register SV *dstr;
    register char *m;
    I32 iters = 0;
    STRLEN slen = do_utf8 ? utf8_length((U8*)s, (U8*)strend) : (strend - s);
    I32 maxiters = slen + 10;
    I32 i;
    char *orig;
    I32 origlimit = limit;
    I32 realarray = 0;
    I32 base;
    AV *oldstack = PL_curstack;
    I32 gimme = GIMME_V;
    I32 oldsave = PL_savestack_ix;
    I32 make_mortal = 1;
    MAGIC *mg = (MAGIC *) NULL;

#ifdef DEBUGGING
    Copy(&LvTARGOFF(POPs), &pm, 1, PMOP*);
#else
    pm = (PMOP*)POPs;
#endif
    if (!pm || !s)
	DIE(aTHX_ "panic: pp_split");
    rx = PM_GETRE(pm);

    TAINT_IF((pm->op_pmflags & PMf_LOCALE) &&
	     (pm->op_pmflags & (PMf_WHITE | PMf_SKIPWHITE)));

    RX_MATCH_UTF8_set(rx, do_utf8);

    if (pm->op_pmreplroot) {
#ifdef USE_ITHREADS
	ary = GvAVn((GV*)PAD_SVl(INT2PTR(PADOFFSET, pm->op_pmreplroot)));
#else
	ary = GvAVn((GV*)pm->op_pmreplroot);
#endif
    }
    else if (gimme != G_ARRAY)
#ifdef USE_5005THREADS
	ary = (AV*)PAD_SVl(0);
#else
	ary = GvAVn(PL_defgv);
#endif /* USE_5005THREADS */
    else
	ary = Nullav;
    if (ary && (gimme != G_ARRAY || (pm->op_pmflags & PMf_ONCE))) {
	realarray = 1;
	PUTBACK;
	av_extend(ary,0);
	av_clear(ary);
	SPAGAIN;
	if ((mg = SvTIED_mg((SV*)ary, PERL_MAGIC_tied))) {
	    PUSHMARK(SP);
	    XPUSHs(SvTIED_obj((SV*)ary, mg));
	}
	else {
	    if (!AvREAL(ary)) {
		AvREAL_on(ary);
		AvREIFY_off(ary);
		for (i = AvFILLp(ary); i >= 0; i--)
		    AvARRAY(ary)[i] = &PL_sv_undef;	/* don't free mere refs */
	    }
	    /* temporarily switch stacks */
	    SWITCHSTACK(PL_curstack, ary);
	    PL_curstackinfo->si_stack = ary;
	    make_mortal = 0;
	}
    }
    base = SP - PL_stack_base;
    orig = s;
    if (pm->op_pmflags & PMf_SKIPWHITE) {
	if (pm->op_pmflags & PMf_LOCALE) {
	    while (isSPACE_LC(*s))
		s++;
	}
	else {
	    while (isSPACE(*s))
		s++;
	}
    }
    if (pm->op_pmflags & (PMf_MULTILINE|PMf_SINGLELINE)) {
	SAVEINT(PL_multiline);
	PL_multiline = pm->op_pmflags & PMf_MULTILINE;
    }

    if (!limit)
	limit = maxiters + 2;
    if (pm->op_pmflags & PMf_WHITE) {
	while (--limit) {
	    m = s;
	    while (m < strend &&
		   !((pm->op_pmflags & PMf_LOCALE)
		     ? isSPACE_LC(*m) : isSPACE(*m)))
		++m;
	    if (m >= strend)
		break;

	    dstr = NEWSV(30, m-s);
	    sv_setpvn(dstr, s, m-s);
	    if (make_mortal)
		sv_2mortal(dstr);
	    if (do_utf8)
		(void)SvUTF8_on(dstr);
	    XPUSHs(dstr);

	    s = m + 1;
	    while (s < strend &&
		   ((pm->op_pmflags & PMf_LOCALE)
		    ? isSPACE_LC(*s) : isSPACE(*s)))
		++s;
	}
    }
    else if (strEQ("^", rx->precomp)) {
	while (--limit) {
	    /*SUPPRESS 530*/
	    for (m = s; m < strend && *m != '\n'; m++) ;
	    m++;
	    if (m >= strend)
		break;
	    dstr = NEWSV(30, m-s);
	    sv_setpvn(dstr, s, m-s);
	    if (make_mortal)
		sv_2mortal(dstr);
	    if (do_utf8)
		(void)SvUTF8_on(dstr);
	    XPUSHs(dstr);
	    s = m;
	}
    }
    else if (do_utf8 == ((rx->reganch & ROPT_UTF8) != 0) &&
	     (rx->reganch & RE_USE_INTUIT) && !rx->nparens
	     && (rx->reganch & ROPT_CHECK_ALL)
	     && !(rx->reganch & ROPT_ANCH)) {
	int tail = (rx->reganch & RE_INTUIT_TAIL);
	SV *csv = CALLREG_INTUIT_STRING(aTHX_ rx);

	len = rx->minlen;
	if (len == 1 && !(rx->reganch & ROPT_UTF8) && !tail) {
	    STRLEN n_a;
	    char c = *SvPV(csv, n_a);
	    while (--limit) {
		/*SUPPRESS 530*/
		for (m = s; m < strend && *m != c; m++) ;
		if (m >= strend)
		    break;
		dstr = NEWSV(30, m-s);
		sv_setpvn(dstr, s, m-s);
		if (make_mortal)
		    sv_2mortal(dstr);
		if (do_utf8)
		    (void)SvUTF8_on(dstr);
		XPUSHs(dstr);
		/* The rx->minlen is in characters but we want to step
		 * s ahead by bytes. */
 		if (do_utf8)
		    s = (char*)utf8_hop((U8*)m, len);
 		else
		    s = m + len; /* Fake \n at the end */
	    }
	}
	else {
#ifndef lint
	    while (s < strend && --limit &&
	      (m = fbm_instr((unsigned char*)s, (unsigned char*)strend,
			     csv, PL_multiline ? FBMrf_MULTILINE : 0)) )
#endif
	    {
		dstr = NEWSV(31, m-s);
		sv_setpvn(dstr, s, m-s);
		if (make_mortal)
		    sv_2mortal(dstr);
		if (do_utf8)
		    (void)SvUTF8_on(dstr);
		XPUSHs(dstr);
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
	maxiters += slen * rx->nparens;
	while (s < strend && --limit)
	{
	    PUTBACK;
	    i = CALLREGEXEC(aTHX_ rx, s, strend, orig, 1 , sv, NULL, 0);
	    SPAGAIN;
	    if (i == 0)
		break;
	    TAINT_IF(RX_MATCH_TAINTED(rx));
	    if (RX_MATCH_COPIED(rx) && rx->subbeg != orig) {
		m = s;
		s = orig;
		orig = rx->subbeg;
		s = orig + (m - s);
		strend = s + (strend - m);
	    }
	    m = rx->startp[0] + orig;
	    dstr = NEWSV(32, m-s);
	    sv_setpvn(dstr, s, m-s);
	    if (make_mortal)
		sv_2mortal(dstr);
	    if (do_utf8)
		(void)SvUTF8_on(dstr);
	    XPUSHs(dstr);
	    if (rx->nparens) {
		for (i = 1; i <= (I32)rx->nparens; i++) {
		    s = rx->startp[i] + orig;
		    m = rx->endp[i] + orig;

		    /* japhy (07/27/01) -- the (m && s) test doesn't catch
		       parens that didn't match -- they should be set to
		       undef, not the empty string */
		    if (m >= orig && s >= orig) {
			dstr = NEWSV(33, m-s);
			sv_setpvn(dstr, s, m-s);
		    }
		    else
			dstr = &PL_sv_undef;  /* undef, not "" */
		    if (make_mortal)
			sv_2mortal(dstr);
		    if (do_utf8)
			(void)SvUTF8_on(dstr);
		    XPUSHs(dstr);
		}
	    }
	    s = rx->endp[0] + orig;
	}
    }

    LEAVE_SCOPE(oldsave);
    iters = (SP - PL_stack_base) - base;
    if (iters > maxiters)
	DIE(aTHX_ "Split loop");

    /* keep field after final delim? */
    if (s < strend || (iters && origlimit)) {
        STRLEN l = strend - s;
	dstr = NEWSV(34, l);
	sv_setpvn(dstr, s, l);
	if (make_mortal)
	    sv_2mortal(dstr);
	if (do_utf8)
	    (void)SvUTF8_on(dstr);
	XPUSHs(dstr);
	iters++;
    }
    else if (!origlimit) {
	while (iters > 0 && (!TOPs || !SvANY(TOPs) || SvCUR(TOPs) == 0)) {
	    if (TOPs && !make_mortal)
		sv_2mortal(TOPs);
	    iters--;
	    *SP-- = &PL_sv_undef;
	}
    }

    if (realarray) {
	if (!mg) {
	    SWITCHSTACK(ary, oldstack);
	    PL_curstackinfo->si_stack = oldstack;
	    if (SvSMAGICAL(ary)) {
		PUTBACK;
		mg_set((SV*)ary);
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
	    ENTER;
	    call_method("PUSH",G_SCALAR|G_DISCARD);
	    LEAVE;
	    SPAGAIN;
	    if (gimme == G_ARRAY) {
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

#ifdef USE_5005THREADS
void
Perl_unlock_condpair(pTHX_ void *svv)
{
    MAGIC *mg = mg_find((SV*)svv, PERL_MAGIC_mutex);

    if (!mg)
	Perl_croak(aTHX_ "panic: unlock_condpair unlocking non-mutex");
    MUTEX_LOCK(MgMUTEXP(mg));
    if (MgOWNER(mg) != thr)
	Perl_croak(aTHX_ "panic: unlock_condpair unlocking mutex that we don't own");
    MgOWNER(mg) = 0;
    COND_SIGNAL(MgOWNERCONDP(mg));
    DEBUG_S(PerlIO_printf(Perl_debug_log, "0x%"UVxf": unlock 0x%"UVxf"\n",
			  PTR2UV(thr), PTR2UV(svv)));
    MUTEX_UNLOCK(MgMUTEXP(mg));
}
#endif /* USE_5005THREADS */

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

PP(pp_threadsv)
{
#ifdef USE_5005THREADS
    dSP;
    EXTEND(SP, 1);
    if (PL_op->op_private & OPpLVAL_INTRO)
	PUSHs(*save_threadsv(PL_op->op_targ));
    else
	PUSHs(THREADSV(PL_op->op_targ));
    RETURN;
#else
    DIE(aTHX_ "tried to access per-thread data in non-threaded perl");
#endif /* USE_5005THREADS */
}

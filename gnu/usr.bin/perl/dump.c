/*    dump.c
 *
 *    Copyright (c) 1991-2002, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "'You have talked long in your sleep, Frodo,' said Gandalf gently, 'and
 * it has not been hard for me to read your mind and memory.'"
 */

#include "EXTERN.h"
#define PERL_IN_DUMP_C
#include "perl.h"
#include "regcomp.h"

void
Perl_dump_indent(pTHX_ I32 level, PerlIO *file, const char* pat, ...)
{
    va_list args;
    va_start(args, pat);
    dump_vindent(level, file, pat, &args);
    va_end(args);
}

void
Perl_dump_vindent(pTHX_ I32 level, PerlIO *file, const char* pat, va_list *args)
{
    PerlIO_printf(file, "%*s", (int)(level*PL_dumpindent), "");
    PerlIO_vprintf(file, pat, *args);
}

void
Perl_dump_all(pTHX)
{
    PerlIO_setlinebuf(Perl_debug_log);
    if (PL_main_root)
	op_dump(PL_main_root);
    dump_packsubs(PL_defstash);
}

void
Perl_dump_packsubs(pTHX_ HV *stash)
{
    I32	i;
    HE	*entry;

    if (!HvARRAY(stash))
	return;
    for (i = 0; i <= (I32) HvMAX(stash); i++) {
	for (entry = HvARRAY(stash)[i]; entry; entry = HeNEXT(entry)) {
	    GV *gv = (GV*)HeVAL(entry);
	    HV *hv;
	    if (SvTYPE(gv) != SVt_PVGV || !GvGP(gv))
		continue;
	    if (GvCVu(gv))
		dump_sub(gv);
	    if (GvFORM(gv))
		dump_form(gv);
	    if (HeKEY(entry)[HeKLEN(entry)-1] == ':'
		&& (hv = GvHV(gv)) && hv != PL_defstash)
		dump_packsubs(hv);		/* nested package */
	}
    }
}

void
Perl_dump_sub(pTHX_ GV *gv)
{
    SV *sv = sv_newmortal();

    gv_fullname3(sv, gv, Nullch);
    Perl_dump_indent(aTHX_ 0, Perl_debug_log, "\nSUB %s = ", SvPVX(sv));
    if (CvXSUB(GvCV(gv)))
	Perl_dump_indent(aTHX_ 0, Perl_debug_log, "(xsub 0x%"UVxf" %d)\n",
	    PTR2UV(CvXSUB(GvCV(gv))),
	    (int)CvXSUBANY(GvCV(gv)).any_i32);
    else if (CvROOT(GvCV(gv)))
	op_dump(CvROOT(GvCV(gv)));
    else
	Perl_dump_indent(aTHX_ 0, Perl_debug_log, "<undef>\n");
}

void
Perl_dump_form(pTHX_ GV *gv)
{
    SV *sv = sv_newmortal();

    gv_fullname3(sv, gv, Nullch);
    Perl_dump_indent(aTHX_ 0, Perl_debug_log, "\nFORMAT %s = ", SvPVX(sv));
    if (CvROOT(GvFORM(gv)))
	op_dump(CvROOT(GvFORM(gv)));
    else
	Perl_dump_indent(aTHX_ 0, Perl_debug_log, "<undef>\n");
}

void
Perl_dump_eval(pTHX)
{
    op_dump(PL_eval_root);
}

char *
Perl_pv_display(pTHX_ SV *dsv, char *pv, STRLEN cur, STRLEN len, STRLEN pvlim)
{
    int truncated = 0;
    int nul_terminated = len > cur && pv[cur] == '\0';

    sv_setpvn(dsv, "\"", 1);
    for (; cur--; pv++) {
	if (pvlim && SvCUR(dsv) >= pvlim) {
            truncated++;
	    break;
        }
        if (isPRINT(*pv)) {
            switch (*pv) {
	    case '\t': sv_catpvn(dsv, "\\t", 2);  break;
	    case '\n': sv_catpvn(dsv, "\\n", 2);  break;
	    case '\r': sv_catpvn(dsv, "\\r", 2);  break;
	    case '\f': sv_catpvn(dsv, "\\f", 2);  break;
	    case '"':  sv_catpvn(dsv, "\\\"", 2); break;
	    case '\\': sv_catpvn(dsv, "\\\\", 2); break;
	    default:   sv_catpvn(dsv, pv, 1);     break;
            }
        }
	else {
	    if (cur && isDIGIT(*(pv+1)))
		Perl_sv_catpvf(aTHX_ dsv, "\\%03o", (U8)*pv);
	    else
		Perl_sv_catpvf(aTHX_ dsv, "\\%o", (U8)*pv);
        }
    }
    sv_catpvn(dsv, "\"", 1);
    if (truncated)
	sv_catpvn(dsv, "...", 3);
    if (nul_terminated)
	sv_catpvn(dsv, "\\0", 2);

    return SvPVX(dsv);
}

char *
Perl_sv_peek(pTHX_ SV *sv)
{
    SV *t = sv_newmortal();
    STRLEN n_a;
    int unref = 0;

    sv_setpvn(t, "", 0);
  retry:
    if (!sv) {
	sv_catpv(t, "VOID");
	goto finish;
    }
    else if (sv == (SV*)0x55555555 || SvTYPE(sv) == 'U') {
	sv_catpv(t, "WILD");
	goto finish;
    }
    else if (sv == &PL_sv_undef || sv == &PL_sv_no || sv == &PL_sv_yes) {
	if (sv == &PL_sv_undef) {
	    sv_catpv(t, "SV_UNDEF");
	    if (!(SvFLAGS(sv) & (SVf_OK|SVf_OOK|SVs_OBJECT|
				 SVs_GMG|SVs_SMG|SVs_RMG)) &&
		SvREADONLY(sv))
		goto finish;
	}
	else if (sv == &PL_sv_no) {
	    sv_catpv(t, "SV_NO");
	    if (!(SvFLAGS(sv) & (SVf_ROK|SVf_OOK|SVs_OBJECT|
				 SVs_GMG|SVs_SMG|SVs_RMG)) &&
		!(~SvFLAGS(sv) & (SVf_POK|SVf_NOK|SVf_READONLY|
				  SVp_POK|SVp_NOK)) &&
		SvCUR(sv) == 0 &&
		SvNVX(sv) == 0.0)
		goto finish;
	}
	else {
	    sv_catpv(t, "SV_YES");
	    if (!(SvFLAGS(sv) & (SVf_ROK|SVf_OOK|SVs_OBJECT|
				 SVs_GMG|SVs_SMG|SVs_RMG)) &&
		!(~SvFLAGS(sv) & (SVf_POK|SVf_NOK|SVf_READONLY|
				  SVp_POK|SVp_NOK)) &&
		SvCUR(sv) == 1 &&
		SvPVX(sv) && *SvPVX(sv) == '1' &&
		SvNVX(sv) == 1.0)
		goto finish;
	}
	sv_catpv(t, ":");
    }
    else if (SvREFCNT(sv) == 0) {
	sv_catpv(t, "(");
	unref++;
    }
    else if (DEBUG_R_TEST_ && SvREFCNT(sv) > 1) {
	Perl_sv_catpvf(aTHX_ t, "<%"UVuf">", (UV)SvREFCNT(sv));
    }


    if (SvROK(sv)) {
	sv_catpv(t, "\\");
	if (SvCUR(t) + unref > 10) {
	    SvCUR(t) = unref + 3;
	    *SvEND(t) = '\0';
	    sv_catpv(t, "...");
	    goto finish;
	}
	sv = (SV*)SvRV(sv);
	goto retry;
    }
    switch (SvTYPE(sv)) {
    default:
	sv_catpv(t, "FREED");
	goto finish;

    case SVt_NULL:
	sv_catpv(t, "UNDEF");
	goto finish;
    case SVt_IV:
	sv_catpv(t, "IV");
	break;
    case SVt_NV:
	sv_catpv(t, "NV");
	break;
    case SVt_RV:
	sv_catpv(t, "RV");
	break;
    case SVt_PV:
	sv_catpv(t, "PV");
	break;
    case SVt_PVIV:
	sv_catpv(t, "PVIV");
	break;
    case SVt_PVNV:
	sv_catpv(t, "PVNV");
	break;
    case SVt_PVMG:
	sv_catpv(t, "PVMG");
	break;
    case SVt_PVLV:
	sv_catpv(t, "PVLV");
	break;
    case SVt_PVAV:
	sv_catpv(t, "AV");
	break;
    case SVt_PVHV:
	sv_catpv(t, "HV");
	break;
    case SVt_PVCV:
	if (CvGV(sv))
	    Perl_sv_catpvf(aTHX_ t, "CV(%s)", GvNAME(CvGV(sv)));
	else
	    sv_catpv(t, "CV()");
	goto finish;
    case SVt_PVGV:
	sv_catpv(t, "GV");
	break;
    case SVt_PVBM:
	sv_catpv(t, "BM");
	break;
    case SVt_PVFM:
	sv_catpv(t, "FM");
	break;
    case SVt_PVIO:
	sv_catpv(t, "IO");
	break;
    }

    if (SvPOKp(sv)) {
	if (!SvPVX(sv))
	    sv_catpv(t, "(null)");
	else {
	    SV *tmp = newSVpvn("", 0);
	    sv_catpv(t, "(");
	    if (SvOOK(sv))
		Perl_sv_catpvf(aTHX_ t, "[%s]", pv_display(tmp, SvPVX(sv)-SvIVX(sv), SvIVX(sv), 0, 127));
	    Perl_sv_catpvf(aTHX_ t, "%s)", pv_display(tmp, SvPVX(sv), SvCUR(sv), SvLEN(sv), 127));
	    if (SvUTF8(sv))
		Perl_sv_catpvf(aTHX_ t, " [UTF8 \"%s\"]",
			       sv_uni_display(tmp, sv, 8 * sv_len_utf8(sv),
					      UNI_DISPLAY_QQ));
	    SvREFCNT_dec(tmp);
	}
    }
    else if (SvNOKp(sv)) {
	STORE_NUMERIC_LOCAL_SET_STANDARD();
	Perl_sv_catpvf(aTHX_ t, "(%"NVgf")",SvNVX(sv));
	RESTORE_NUMERIC_LOCAL();
    }
    else if (SvIOKp(sv)) {
	if (SvIsUV(sv))
	    Perl_sv_catpvf(aTHX_ t, "(%"UVuf")", (UV)SvUVX(sv));
	else
            Perl_sv_catpvf(aTHX_ t, "(%"IVdf")", (IV)SvIVX(sv));
    }
    else
	sv_catpv(t, "()");

  finish:
    if (unref) {
	while (unref--)
	    sv_catpv(t, ")");
    }
    return SvPV(t, n_a);
}

void
Perl_do_pmop_dump(pTHX_ I32 level, PerlIO *file, PMOP *pm)
{
    char ch;

    if (!pm) {
	Perl_dump_indent(aTHX_ level, file, "{}\n");
	return;
    }
    Perl_dump_indent(aTHX_ level, file, "{\n");
    level++;
    if (pm->op_pmflags & PMf_ONCE)
	ch = '?';
    else
	ch = '/';
    if (PM_GETRE(pm))
	Perl_dump_indent(aTHX_ level, file, "PMf_PRE %c%s%c%s\n",
	     ch, PM_GETRE(pm)->precomp, ch,
	     (pm->op_private & OPpRUNTIME) ? " (RUNTIME)" : "");
    else
	Perl_dump_indent(aTHX_ level, file, "PMf_PRE (RUNTIME)\n");
    if (pm->op_type != OP_PUSHRE && pm->op_pmreplroot) {
	Perl_dump_indent(aTHX_ level, file, "PMf_REPL = ");
	op_dump(pm->op_pmreplroot);
    }
    if (pm->op_pmflags || (PM_GETRE(pm) && PM_GETRE(pm)->check_substr)) {
	SV *tmpsv = newSVpvn("", 0);
	if (pm->op_pmdynflags & PMdf_USED)
	    sv_catpv(tmpsv, ",USED");
	if (pm->op_pmdynflags & PMdf_TAINTED)
	    sv_catpv(tmpsv, ",TAINTED");
	if (pm->op_pmflags & PMf_ONCE)
	    sv_catpv(tmpsv, ",ONCE");
	if (PM_GETRE(pm) && PM_GETRE(pm)->check_substr
	    && !(PM_GETRE(pm)->reganch & ROPT_NOSCAN))
	    sv_catpv(tmpsv, ",SCANFIRST");
	if (PM_GETRE(pm) && PM_GETRE(pm)->check_substr
	    && PM_GETRE(pm)->reganch & ROPT_CHECK_ALL)
	    sv_catpv(tmpsv, ",ALL");
	if (pm->op_pmflags & PMf_SKIPWHITE)
	    sv_catpv(tmpsv, ",SKIPWHITE");
	if (pm->op_pmflags & PMf_CONST)
	    sv_catpv(tmpsv, ",CONST");
	if (pm->op_pmflags & PMf_KEEP)
	    sv_catpv(tmpsv, ",KEEP");
	if (pm->op_pmflags & PMf_GLOBAL)
	    sv_catpv(tmpsv, ",GLOBAL");
	if (pm->op_pmflags & PMf_CONTINUE)
	    sv_catpv(tmpsv, ",CONTINUE");
	if (pm->op_pmflags & PMf_RETAINT)
	    sv_catpv(tmpsv, ",RETAINT");
	if (pm->op_pmflags & PMf_EVAL)
	    sv_catpv(tmpsv, ",EVAL");
	Perl_dump_indent(aTHX_ level, file, "PMFLAGS = (%s)\n", SvCUR(tmpsv) ? SvPVX(tmpsv) + 1 : "");
	SvREFCNT_dec(tmpsv);
    }

    Perl_dump_indent(aTHX_ level-1, file, "}\n");
}

void
Perl_pmop_dump(pTHX_ PMOP *pm)
{
    do_pmop_dump(0, Perl_debug_log, pm);
}

void
Perl_do_op_dump(pTHX_ I32 level, PerlIO *file, OP *o)
{
    Perl_dump_indent(aTHX_ level, file, "{\n");
    level++;
    if (o->op_seq)
	PerlIO_printf(file, "%-4d", o->op_seq);
    else
	PerlIO_printf(file, "    ");
    PerlIO_printf(file,
		  "%*sTYPE = %s  ===> ",
		  (int)(PL_dumpindent*level-4), "", OP_NAME(o));
    if (o->op_next) {
	if (o->op_seq)
	    PerlIO_printf(file, "%d\n", o->op_next->op_seq);
	else
	    PerlIO_printf(file, "(%d)\n", o->op_next->op_seq);
    }
    else
	PerlIO_printf(file, "DONE\n");
    if (o->op_targ) {
	if (o->op_type == OP_NULL)
	{
	    Perl_dump_indent(aTHX_ level, file, "  (was %s)\n", PL_op_name[o->op_targ]);
	    if (o->op_targ == OP_NEXTSTATE)
	    {
		if (CopLINE(cCOPo))
		    Perl_dump_indent(aTHX_ level, file, "LINE = %d\n",CopLINE(cCOPo));
		if (CopSTASHPV(cCOPo))
		    Perl_dump_indent(aTHX_ level, file, "PACKAGE = \"%s\"\n",
				     CopSTASHPV(cCOPo));
		if (cCOPo->cop_label)
		    Perl_dump_indent(aTHX_ level, file, "LABEL = \"%s\"\n",
				     cCOPo->cop_label);
	    }
	}
	else
	    Perl_dump_indent(aTHX_ level, file, "TARG = %ld\n", (long)o->op_targ);
    }
#ifdef DUMPADDR
    Perl_dump_indent(aTHX_ level, file, "ADDR = 0x%"UVxf" => 0x%"UVxf"\n", (UV)o, (UV)o->op_next);
#endif
    if (o->op_flags) {
	SV *tmpsv = newSVpvn("", 0);
	switch (o->op_flags & OPf_WANT) {
	case OPf_WANT_VOID:
	    sv_catpv(tmpsv, ",VOID");
	    break;
	case OPf_WANT_SCALAR:
	    sv_catpv(tmpsv, ",SCALAR");
	    break;
	case OPf_WANT_LIST:
	    sv_catpv(tmpsv, ",LIST");
	    break;
	default:
	    sv_catpv(tmpsv, ",UNKNOWN");
	    break;
	}
	if (o->op_flags & OPf_KIDS)
	    sv_catpv(tmpsv, ",KIDS");
	if (o->op_flags & OPf_PARENS)
	    sv_catpv(tmpsv, ",PARENS");
	if (o->op_flags & OPf_STACKED)
	    sv_catpv(tmpsv, ",STACKED");
	if (o->op_flags & OPf_REF)
	    sv_catpv(tmpsv, ",REF");
	if (o->op_flags & OPf_MOD)
	    sv_catpv(tmpsv, ",MOD");
	if (o->op_flags & OPf_SPECIAL)
	    sv_catpv(tmpsv, ",SPECIAL");
	Perl_dump_indent(aTHX_ level, file, "FLAGS = (%s)\n", SvCUR(tmpsv) ? SvPVX(tmpsv) + 1 : "");
	SvREFCNT_dec(tmpsv);
    }
    if (o->op_private) {
	SV *tmpsv = newSVpvn("", 0);
	if (PL_opargs[o->op_type] & OA_TARGLEX) {
	    if (o->op_private & OPpTARGET_MY)
		sv_catpv(tmpsv, ",TARGET_MY");
	}
	else if (o->op_type == OP_LEAVESUB ||
		 o->op_type == OP_LEAVE ||
		 o->op_type == OP_LEAVESUBLV ||
		 o->op_type == OP_LEAVEWRITE) {
	    if (o->op_private & OPpREFCOUNTED)
		sv_catpv(tmpsv, ",REFCOUNTED");
	}
        else if (o->op_type == OP_AASSIGN) {
	    if (o->op_private & OPpASSIGN_COMMON)
		sv_catpv(tmpsv, ",COMMON");
	    if (o->op_private & OPpASSIGN_HASH)
		sv_catpv(tmpsv, ",HASH");
	}
	else if (o->op_type == OP_SASSIGN) {
	    if (o->op_private & OPpASSIGN_BACKWARDS)
		sv_catpv(tmpsv, ",BACKWARDS");
	}
	else if (o->op_type == OP_TRANS) {
	    if (o->op_private & OPpTRANS_SQUASH)
		sv_catpv(tmpsv, ",SQUASH");
	    if (o->op_private & OPpTRANS_DELETE)
		sv_catpv(tmpsv, ",DELETE");
	    if (o->op_private & OPpTRANS_COMPLEMENT)
		sv_catpv(tmpsv, ",COMPLEMENT");
	    if (o->op_private & OPpTRANS_IDENTICAL)
		sv_catpv(tmpsv, ",IDENTICAL");
	    if (o->op_private & OPpTRANS_GROWS)
		sv_catpv(tmpsv, ",GROWS");
	}
	else if (o->op_type == OP_REPEAT) {
	    if (o->op_private & OPpREPEAT_DOLIST)
		sv_catpv(tmpsv, ",DOLIST");
	}
	else if (o->op_type == OP_ENTERSUB ||
		 o->op_type == OP_RV2SV ||
		 o->op_type == OP_GVSV ||
		 o->op_type == OP_RV2AV ||
		 o->op_type == OP_RV2HV ||
		 o->op_type == OP_RV2GV ||
		 o->op_type == OP_AELEM ||
		 o->op_type == OP_HELEM )
	{
	    if (o->op_type == OP_ENTERSUB) {
		if (o->op_private & OPpENTERSUB_AMPER)
		    sv_catpv(tmpsv, ",AMPER");
		if (o->op_private & OPpENTERSUB_DB)
		    sv_catpv(tmpsv, ",DB");
		if (o->op_private & OPpENTERSUB_HASTARG)
		    sv_catpv(tmpsv, ",HASTARG");
		if (o->op_private & OPpENTERSUB_NOPAREN)
		    sv_catpv(tmpsv, ",NOPAREN");
		if (o->op_private & OPpENTERSUB_INARGS)
		    sv_catpv(tmpsv, ",INARGS");
		if (o->op_private & OPpENTERSUB_NOMOD)
		    sv_catpv(tmpsv, ",NOMOD");
	    }
	    else {
		switch (o->op_private & OPpDEREF) {
	    case OPpDEREF_SV:
		sv_catpv(tmpsv, ",SV");
		break;
	    case OPpDEREF_AV:
		sv_catpv(tmpsv, ",AV");
		break;
	    case OPpDEREF_HV:
		sv_catpv(tmpsv, ",HV");
		break;
	    }
		if (o->op_private & OPpMAYBE_LVSUB)
		    sv_catpv(tmpsv, ",MAYBE_LVSUB");
	    }
	    if (o->op_type == OP_AELEM || o->op_type == OP_HELEM) {
		if (o->op_private & OPpLVAL_DEFER)
		    sv_catpv(tmpsv, ",LVAL_DEFER");
	    }
	    else {
		if (o->op_private & HINT_STRICT_REFS)
		    sv_catpv(tmpsv, ",STRICT_REFS");
		if (o->op_private & OPpOUR_INTRO)
		    sv_catpv(tmpsv, ",OUR_INTRO");
	    }
	}
	else if (o->op_type == OP_CONST) {
	    if (o->op_private & OPpCONST_BARE)
		sv_catpv(tmpsv, ",BARE");
	    if (o->op_private & OPpCONST_STRICT)
		sv_catpv(tmpsv, ",STRICT");
	    if (o->op_private & OPpCONST_ARYBASE)
		sv_catpv(tmpsv, ",ARYBASE");
	    if (o->op_private & OPpCONST_WARNING)
		sv_catpv(tmpsv, ",WARNING");
	    if (o->op_private & OPpCONST_ENTERED)
		sv_catpv(tmpsv, ",ENTERED");
	}
	else if (o->op_type == OP_FLIP) {
	    if (o->op_private & OPpFLIP_LINENUM)
		sv_catpv(tmpsv, ",LINENUM");
	}
	else if (o->op_type == OP_FLOP) {
	    if (o->op_private & OPpFLIP_LINENUM)
		sv_catpv(tmpsv, ",LINENUM");
	}
	else if (o->op_type == OP_RV2CV) {
	    if (o->op_private & OPpLVAL_INTRO)
		sv_catpv(tmpsv, ",INTRO");
	}
	else if (o->op_type == OP_GV) {
	    if (o->op_private & OPpEARLY_CV)
		sv_catpv(tmpsv, ",EARLY_CV");
	}
	else if (o->op_type == OP_LIST) {
	    if (o->op_private & OPpLIST_GUESSED)
		sv_catpv(tmpsv, ",GUESSED");
	}
	else if (o->op_type == OP_DELETE) {
	    if (o->op_private & OPpSLICE)
		sv_catpv(tmpsv, ",SLICE");
	}
	else if (o->op_type == OP_EXISTS) {
	    if (o->op_private & OPpEXISTS_SUB)
		sv_catpv(tmpsv, ",EXISTS_SUB");
	}
	else if (o->op_type == OP_SORT) {
	    if (o->op_private & OPpSORT_NUMERIC)
		sv_catpv(tmpsv, ",NUMERIC");
	    if (o->op_private & OPpSORT_INTEGER)
		sv_catpv(tmpsv, ",INTEGER");
	    if (o->op_private & OPpSORT_REVERSE)
		sv_catpv(tmpsv, ",REVERSE");
	}
	else if (o->op_type == OP_THREADSV) {
	    if (o->op_private & OPpDONE_SVREF)
		sv_catpv(tmpsv, ",SVREF");
	}
	else if (o->op_type == OP_OPEN || o->op_type == OP_BACKTICK) {
	    if (o->op_private & OPpOPEN_IN_RAW)
		sv_catpv(tmpsv, ",IN_RAW");
	    if (o->op_private & OPpOPEN_IN_CRLF)
		sv_catpv(tmpsv, ",IN_CRLF");
	    if (o->op_private & OPpOPEN_OUT_RAW)
		sv_catpv(tmpsv, ",OUT_RAW");
	    if (o->op_private & OPpOPEN_OUT_CRLF)
		sv_catpv(tmpsv, ",OUT_CRLF");
	}
	else if (o->op_type == OP_EXIT) {
	    if (o->op_private & OPpEXIT_VMSISH)
		sv_catpv(tmpsv, ",EXIT_VMSISH");
	    if (o->op_private & OPpHUSH_VMSISH)
		sv_catpv(tmpsv, ",HUSH_VMSISH");
	}
	else if (o->op_type == OP_DIE) {
	    if (o->op_private & OPpHUSH_VMSISH)
		sv_catpv(tmpsv, ",HUSH_VMSISH");
	}
	if (o->op_flags & OPf_MOD && o->op_private & OPpLVAL_INTRO)
	    sv_catpv(tmpsv, ",INTRO");
	if (SvCUR(tmpsv))
	    Perl_dump_indent(aTHX_ level, file, "PRIVATE = (%s)\n", SvPVX(tmpsv) + 1);
	SvREFCNT_dec(tmpsv);
    }

    switch (o->op_type) {
    case OP_AELEMFAST:
    case OP_GVSV:
    case OP_GV:
#ifdef USE_ITHREADS
	Perl_dump_indent(aTHX_ level, file, "PADIX = %" IVdf "\n", (IV)cPADOPo->op_padix);
#else
	if (cSVOPo->op_sv) {
	    SV *tmpsv = NEWSV(0,0);
	    STRLEN n_a;
	    ENTER;
	    SAVEFREESV(tmpsv);
	    gv_fullname3(tmpsv, (GV*)cSVOPo->op_sv, Nullch);
	    Perl_dump_indent(aTHX_ level, file, "GV = %s\n", SvPV(tmpsv, n_a));
	    LEAVE;
	}
	else
	    Perl_dump_indent(aTHX_ level, file, "GV = NULL\n");
#endif
	break;
    case OP_CONST:
    case OP_METHOD_NAMED:
	Perl_dump_indent(aTHX_ level, file, "SV = %s\n", SvPEEK(cSVOPo->op_sv));
	break;
    case OP_SETSTATE:
    case OP_NEXTSTATE:
    case OP_DBSTATE:
	if (CopLINE(cCOPo))
	    Perl_dump_indent(aTHX_ level, file, "LINE = %d\n",CopLINE(cCOPo));
	if (CopSTASHPV(cCOPo))
	    Perl_dump_indent(aTHX_ level, file, "PACKAGE = \"%s\"\n",
			     CopSTASHPV(cCOPo));
	if (cCOPo->cop_label)
	    Perl_dump_indent(aTHX_ level, file, "LABEL = \"%s\"\n",
			     cCOPo->cop_label);
	break;
    case OP_ENTERLOOP:
	Perl_dump_indent(aTHX_ level, file, "REDO ===> ");
	if (cLOOPo->op_redoop)
	    PerlIO_printf(file, "%d\n", cLOOPo->op_redoop->op_seq);
	else
	    PerlIO_printf(file, "DONE\n");
	Perl_dump_indent(aTHX_ level, file, "NEXT ===> ");
	if (cLOOPo->op_nextop)
	    PerlIO_printf(file, "%d\n", cLOOPo->op_nextop->op_seq);
	else
	    PerlIO_printf(file, "DONE\n");
	Perl_dump_indent(aTHX_ level, file, "LAST ===> ");
	if (cLOOPo->op_lastop)
	    PerlIO_printf(file, "%d\n", cLOOPo->op_lastop->op_seq);
	else
	    PerlIO_printf(file, "DONE\n");
	break;
    case OP_COND_EXPR:
    case OP_RANGE:
    case OP_MAPWHILE:
    case OP_GREPWHILE:
    case OP_OR:
    case OP_AND:
	Perl_dump_indent(aTHX_ level, file, "OTHER ===> ");
	if (cLOGOPo->op_other)
	    PerlIO_printf(file, "%d\n", cLOGOPo->op_other->op_seq);
	else
	    PerlIO_printf(file, "DONE\n");
	break;
    case OP_PUSHRE:
    case OP_MATCH:
    case OP_QR:
    case OP_SUBST:
	do_pmop_dump(level, file, cPMOPo);
	break;
    case OP_LEAVE:
    case OP_LEAVEEVAL:
    case OP_LEAVESUB:
    case OP_LEAVESUBLV:
    case OP_LEAVEWRITE:
    case OP_SCOPE:
	if (o->op_private & OPpREFCOUNTED)
	    Perl_dump_indent(aTHX_ level, file, "REFCNT = %"UVuf"\n", (UV)o->op_targ);
	break;
    default:
	break;
    }
    if (o->op_flags & OPf_KIDS) {
	OP *kid;
	for (kid = cUNOPo->op_first; kid; kid = kid->op_sibling)
	    do_op_dump(level, file, kid);
    }
    Perl_dump_indent(aTHX_ level-1, file, "}\n");
}

void
Perl_op_dump(pTHX_ OP *o)
{
    do_op_dump(0, Perl_debug_log, o);
}

void
Perl_gv_dump(pTHX_ GV *gv)
{
    SV *sv;

    if (!gv) {
	PerlIO_printf(Perl_debug_log, "{}\n");
	return;
    }
    sv = sv_newmortal();
    PerlIO_printf(Perl_debug_log, "{\n");
    gv_fullname3(sv, gv, Nullch);
    Perl_dump_indent(aTHX_ 1, Perl_debug_log, "GV_NAME = %s", SvPVX(sv));
    if (gv != GvEGV(gv)) {
	gv_efullname3(sv, GvEGV(gv), Nullch);
	Perl_dump_indent(aTHX_ 1, Perl_debug_log, "-> %s", SvPVX(sv));
    }
    PerlIO_putc(Perl_debug_log, '\n');
    Perl_dump_indent(aTHX_ 0, Perl_debug_log, "}\n");
}


/* map magic types to the symbolic names
 * (with the PERL_MAGIC_ prefixed stripped)
 */

static struct { char type; char *name; } magic_names[] = {
	{ PERL_MAGIC_sv,             "sv(\\0)" },
	{ PERL_MAGIC_arylen,         "arylen(#)" },
	{ PERL_MAGIC_glob,           "glob(*)" },
	{ PERL_MAGIC_pos,            "pos(.)" },
	{ PERL_MAGIC_backref,        "backref(<)" },
	{ PERL_MAGIC_overload,       "overload(A)" },
	{ PERL_MAGIC_bm,             "bm(B)" },
	{ PERL_MAGIC_regdata,        "regdata(D)" },
	{ PERL_MAGIC_env,            "env(E)" },
	{ PERL_MAGIC_isa,            "isa(I)" },
	{ PERL_MAGIC_dbfile,         "dbfile(L)" },
	{ PERL_MAGIC_shared,         "shared(N)" },
	{ PERL_MAGIC_tied,           "tied(P)" },
	{ PERL_MAGIC_sig,            "sig(S)" },
	{ PERL_MAGIC_uvar,           "uvar(U)" },
	{ PERL_MAGIC_overload_elem,  "overload_elem(a)" },
	{ PERL_MAGIC_overload_table, "overload_table(c)" },
	{ PERL_MAGIC_regdatum,       "regdatum(d)" },
	{ PERL_MAGIC_envelem,        "envelem(e)" },
	{ PERL_MAGIC_fm,             "fm(f)" },
	{ PERL_MAGIC_regex_global,   "regex_global(g)" },
	{ PERL_MAGIC_isaelem,        "isaelem(i)" },
	{ PERL_MAGIC_nkeys,          "nkeys(k)" },
	{ PERL_MAGIC_dbline,         "dbline(l)" },
	{ PERL_MAGIC_mutex,          "mutex(m)" },
	{ PERL_MAGIC_shared_scalar,  "shared_scalar(n)" },
	{ PERL_MAGIC_collxfrm,       "collxfrm(o)" },
	{ PERL_MAGIC_tiedelem,       "tiedelem(p)" },
	{ PERL_MAGIC_tiedscalar,     "tiedscalar(q)" },
	{ PERL_MAGIC_qr,             "qr(r)" },
	{ PERL_MAGIC_sigelem,        "sigelem(s)" },
	{ PERL_MAGIC_taint,          "taint(t)" },
	{ PERL_MAGIC_uvar_elem,      "uvar_elem(v)" },
	{ PERL_MAGIC_vec,            "vec(v)" },
	{ PERL_MAGIC_substr,         "substr(x)" },
	{ PERL_MAGIC_defelem,        "defelem(y)" },
	{ PERL_MAGIC_ext,            "ext(~)" },
	/* this null string terminates the list */
	{ 0,                         0 },
};

void
Perl_do_magic_dump(pTHX_ I32 level, PerlIO *file, MAGIC *mg, I32 nest, I32 maxnest, bool dumpops, STRLEN pvlim)
{
    for (; mg; mg = mg->mg_moremagic) {
 	Perl_dump_indent(aTHX_ level, file,
			 "  MAGIC = 0x%"UVxf"\n", PTR2UV(mg));
 	if (mg->mg_virtual) {
            MGVTBL *v = mg->mg_virtual;
 	    char *s = 0;
 	    if      (v == &PL_vtbl_sv)         s = "sv";
            else if (v == &PL_vtbl_env)        s = "env";
            else if (v == &PL_vtbl_envelem)    s = "envelem";
            else if (v == &PL_vtbl_sig)        s = "sig";
            else if (v == &PL_vtbl_sigelem)    s = "sigelem";
            else if (v == &PL_vtbl_pack)       s = "pack";
            else if (v == &PL_vtbl_packelem)   s = "packelem";
            else if (v == &PL_vtbl_dbline)     s = "dbline";
            else if (v == &PL_vtbl_isa)        s = "isa";
            else if (v == &PL_vtbl_arylen)     s = "arylen";
            else if (v == &PL_vtbl_glob)       s = "glob";
            else if (v == &PL_vtbl_mglob)      s = "mglob";
            else if (v == &PL_vtbl_nkeys)      s = "nkeys";
            else if (v == &PL_vtbl_taint)      s = "taint";
            else if (v == &PL_vtbl_substr)     s = "substr";
            else if (v == &PL_vtbl_vec)        s = "vec";
            else if (v == &PL_vtbl_pos)        s = "pos";
            else if (v == &PL_vtbl_bm)         s = "bm";
            else if (v == &PL_vtbl_fm)         s = "fm";
            else if (v == &PL_vtbl_uvar)       s = "uvar";
            else if (v == &PL_vtbl_defelem)    s = "defelem";
#ifdef USE_LOCALE_COLLATE
	    else if (v == &PL_vtbl_collxfrm)   s = "collxfrm";
#endif
	    else if (v == &PL_vtbl_amagic)     s = "amagic";
	    else if (v == &PL_vtbl_amagicelem) s = "amagicelem";
	    else if (v == &PL_vtbl_backref)    s = "backref";
	    if (s)
	        Perl_dump_indent(aTHX_ level, file, "    MG_VIRTUAL = &PL_vtbl_%s\n", s);
	    else
	        Perl_dump_indent(aTHX_ level, file, "    MG_VIRTUAL = 0x%"UVxf"\n", PTR2UV(v));
        }
	else
	    Perl_dump_indent(aTHX_ level, file, "    MG_VIRTUAL = 0\n");

	if (mg->mg_private)
	    Perl_dump_indent(aTHX_ level, file, "    MG_PRIVATE = %d\n", mg->mg_private);

	{
	    int n;
	    char *name = 0;
	    for (n=0; magic_names[n].name; n++) {
		if (mg->mg_type == magic_names[n].type) {
		    name = magic_names[n].name;
		    break;
		}
	    }
	    if (name)
		Perl_dump_indent(aTHX_ level, file,
				"    MG_TYPE = PERL_MAGIC_%s\n", name);
	    else
		Perl_dump_indent(aTHX_ level, file,
				"    MG_TYPE = UNKNOWN(\\%o)\n", mg->mg_type);
	}

        if (mg->mg_flags) {
            Perl_dump_indent(aTHX_ level, file, "    MG_FLAGS = 0x%02X\n", mg->mg_flags);
	    if (mg->mg_flags & MGf_TAINTEDDIR)
	        Perl_dump_indent(aTHX_ level, file, "      TAINTEDDIR\n");
	    if (mg->mg_flags & MGf_REFCOUNTED)
	        Perl_dump_indent(aTHX_ level, file, "      REFCOUNTED\n");
            if (mg->mg_flags & MGf_GSKIP)
	        Perl_dump_indent(aTHX_ level, file, "      GSKIP\n");
	    if (mg->mg_flags & MGf_MINMATCH)
	        Perl_dump_indent(aTHX_ level, file, "      MINMATCH\n");
        }
	if (mg->mg_obj) {
	    Perl_dump_indent(aTHX_ level, file, "    MG_OBJ = 0x%"UVxf"\n", PTR2UV(mg->mg_obj));
	    if (mg->mg_flags & MGf_REFCOUNTED)
		do_sv_dump(level+2, file, mg->mg_obj, nest+1, maxnest, dumpops, pvlim); /* MG is already +1 */
	}
        if (mg->mg_len)
	    Perl_dump_indent(aTHX_ level, file, "    MG_LEN = %ld\n", (long)mg->mg_len);
        if (mg->mg_ptr) {
	    Perl_dump_indent(aTHX_ level, file, "    MG_PTR = 0x%"UVxf, PTR2UV(mg->mg_ptr));
	    if (mg->mg_len >= 0) {
		SV *sv = newSVpvn("", 0);
                PerlIO_printf(file, " %s", pv_display(sv, mg->mg_ptr, mg->mg_len, 0, pvlim));
		SvREFCNT_dec(sv);
            }
	    else if (mg->mg_len == HEf_SVKEY) {
		PerlIO_puts(file, " => HEf_SVKEY\n");
		do_sv_dump(level+2, file, (SV*)((mg)->mg_ptr), nest+1, maxnest, dumpops, pvlim); /* MG is already +1 */
		continue;
	    }
	    else
		PerlIO_puts(file, " ???? - please notify IZ");
            PerlIO_putc(file, '\n');
        }
    }
}

void
Perl_magic_dump(pTHX_ MAGIC *mg)
{
    do_magic_dump(0, Perl_debug_log, mg, 0, 0, 0, 0);
}

void
Perl_do_hv_dump(pTHX_ I32 level, PerlIO *file, char *name, HV *sv)
{
    Perl_dump_indent(aTHX_ level, file, "%s = 0x%"UVxf, name, PTR2UV(sv));
    if (sv && HvNAME(sv))
	PerlIO_printf(file, "\t\"%s\"\n", HvNAME(sv));
    else
	PerlIO_putc(file, '\n');
}

void
Perl_do_gv_dump(pTHX_ I32 level, PerlIO *file, char *name, GV *sv)
{
    Perl_dump_indent(aTHX_ level, file, "%s = 0x%"UVxf, name, PTR2UV(sv));
    if (sv && GvNAME(sv))
	PerlIO_printf(file, "\t\"%s\"\n", GvNAME(sv));
    else
	PerlIO_putc(file, '\n');
}

void
Perl_do_gvgv_dump(pTHX_ I32 level, PerlIO *file, char *name, GV *sv)
{
    Perl_dump_indent(aTHX_ level, file, "%s = 0x%"UVxf, name, PTR2UV(sv));
    if (sv && GvNAME(sv)) {
	PerlIO_printf(file, "\t\"");
	if (GvSTASH(sv))
	    PerlIO_printf(file, "%s\" :: \"", HvNAME(GvSTASH(sv)));
	PerlIO_printf(file, "%s\"\n", GvNAME(sv));
    }
    else
	PerlIO_putc(file, '\n');
}

void
Perl_do_sv_dump(pTHX_ I32 level, PerlIO *file, SV *sv, I32 nest, I32 maxnest, bool dumpops, STRLEN pvlim)
{
    SV *d;
    char *s;
    U32 flags;
    U32 type;
    STRLEN n_a;

    if (!sv) {
	Perl_dump_indent(aTHX_ level, file, "SV = 0\n");
	return;
    }

    flags = SvFLAGS(sv);
    type = SvTYPE(sv);

    d = Perl_newSVpvf(aTHX_
		   "(0x%"UVxf") at 0x%"UVxf"\n%*s  REFCNT = %"IVdf"\n%*s  FLAGS = (",
		   PTR2UV(SvANY(sv)), PTR2UV(sv),
		   (int)(PL_dumpindent*level), "", (IV)SvREFCNT(sv),
		   (int)(PL_dumpindent*level), "");

    if (flags & SVs_PADBUSY)	sv_catpv(d, "PADBUSY,");
    if (flags & SVs_PADTMP)	sv_catpv(d, "PADTMP,");
    if (flags & SVs_PADMY)	sv_catpv(d, "PADMY,");
    if (flags & SVs_TEMP)	sv_catpv(d, "TEMP,");
    if (flags & SVs_OBJECT)	sv_catpv(d, "OBJECT,");
    if (flags & SVs_GMG)	sv_catpv(d, "GMG,");
    if (flags & SVs_SMG)	sv_catpv(d, "SMG,");
    if (flags & SVs_RMG)	sv_catpv(d, "RMG,");

    if (flags & SVf_IOK)	sv_catpv(d, "IOK,");
    if (flags & SVf_NOK)	sv_catpv(d, "NOK,");
    if (flags & SVf_POK)	sv_catpv(d, "POK,");
    if (flags & SVf_ROK)  {	
    				sv_catpv(d, "ROK,");
	if (SvWEAKREF(sv))	sv_catpv(d, "WEAKREF,");
    }
    if (flags & SVf_OOK)	sv_catpv(d, "OOK,");
    if (flags & SVf_FAKE)	sv_catpv(d, "FAKE,");
    if (flags & SVf_READONLY)	sv_catpv(d, "READONLY,");

    if (flags & SVf_AMAGIC)	sv_catpv(d, "OVERLOAD,");
    if (flags & SVp_IOK)	sv_catpv(d, "pIOK,");
    if (flags & SVp_NOK)	sv_catpv(d, "pNOK,");
    if (flags & SVp_POK)	sv_catpv(d, "pPOK,");
    if (flags & SVp_SCREAM)	sv_catpv(d, "SCREAM,");

    switch (type) {
    case SVt_PVCV:
    case SVt_PVFM:
	if (CvANON(sv))		sv_catpv(d, "ANON,");
	if (CvUNIQUE(sv))	sv_catpv(d, "UNIQUE,");
	if (CvCLONE(sv))	sv_catpv(d, "CLONE,");
	if (CvCLONED(sv))	sv_catpv(d, "CLONED,");
	if (CvCONST(sv))	sv_catpv(d, "CONST,");
	if (CvNODEBUG(sv))	sv_catpv(d, "NODEBUG,");
	if (SvCOMPILED(sv))	sv_catpv(d, "COMPILED,");
	if (CvLVALUE(sv))	sv_catpv(d, "LVALUE,");
	if (CvMETHOD(sv))	sv_catpv(d, "METHOD,");
	if (CvLOCKED(sv))	sv_catpv(d, "LOCKED,");
	break;
    case SVt_PVHV:
	if (HvSHAREKEYS(sv))	sv_catpv(d, "SHAREKEYS,");
	if (HvLAZYDEL(sv))	sv_catpv(d, "LAZYDEL,");
	if (HvHASKFLAGS(sv))	sv_catpv(d, "HASKFLAGS,");
	break;
    case SVt_PVGV:
	if (GvINTRO(sv))	sv_catpv(d, "INTRO,");
	if (GvMULTI(sv))	sv_catpv(d, "MULTI,");
	if (GvUNIQUE(sv))       sv_catpv(d, "UNIQUE,");
	if (GvASSUMECV(sv))	sv_catpv(d, "ASSUMECV,");
	if (GvIN_PAD(sv))       sv_catpv(d, "IN_PAD,");
	if (flags & SVpad_OUR)	sv_catpv(d, "OUR,");
	if (GvIMPORTED(sv)) {
	    sv_catpv(d, "IMPORT");
	    if (GvIMPORTED(sv) == GVf_IMPORTED)
		sv_catpv(d, "ALL,");
	    else {
		sv_catpv(d, "(");
		if (GvIMPORTED_SV(sv))	sv_catpv(d, " SV");
		if (GvIMPORTED_AV(sv))	sv_catpv(d, " AV");
		if (GvIMPORTED_HV(sv))	sv_catpv(d, " HV");
		if (GvIMPORTED_CV(sv))	sv_catpv(d, " CV");
		sv_catpv(d, " ),");
	    }
	}
	/* FALL THROUGH */
    default:
	if (SvEVALED(sv))	sv_catpv(d, "EVALED,");
	if (SvIsUV(sv))		sv_catpv(d, "IsUV,");
	break;
    case SVt_PVBM:
	if (SvTAIL(sv))		sv_catpv(d, "TAIL,");
	if (SvVALID(sv))	sv_catpv(d, "VALID,");
	break;
    case SVt_PVMG:
	if (flags & SVpad_TYPED)
				sv_catpv(d, "TYPED,");
	break;
    }
    if (SvPOK(sv) && SvUTF8(sv))
        sv_catpv(d, "UTF8");

    if (*(SvEND(d) - 1) == ',')
	SvPVX(d)[--SvCUR(d)] = '\0';
    sv_catpv(d, ")");
    s = SvPVX(d);

    Perl_dump_indent(aTHX_ level, file, "SV = ");
    switch (type) {
    case SVt_NULL:
	PerlIO_printf(file, "NULL%s\n", s);
	SvREFCNT_dec(d);
	return;
    case SVt_IV:
	PerlIO_printf(file, "IV%s\n", s);
	break;
    case SVt_NV:
	PerlIO_printf(file, "NV%s\n", s);
	break;
    case SVt_RV:
	PerlIO_printf(file, "RV%s\n", s);
	break;
    case SVt_PV:
	PerlIO_printf(file, "PV%s\n", s);
	break;
    case SVt_PVIV:
	PerlIO_printf(file, "PVIV%s\n", s);
	break;
    case SVt_PVNV:
	PerlIO_printf(file, "PVNV%s\n", s);
	break;
    case SVt_PVBM:
	PerlIO_printf(file, "PVBM%s\n", s);
	break;
    case SVt_PVMG:
	PerlIO_printf(file, "PVMG%s\n", s);
	break;
    case SVt_PVLV:
	PerlIO_printf(file, "PVLV%s\n", s);
	break;
    case SVt_PVAV:
	PerlIO_printf(file, "PVAV%s\n", s);
	break;
    case SVt_PVHV:
	PerlIO_printf(file, "PVHV%s\n", s);
	break;
    case SVt_PVCV:
	PerlIO_printf(file, "PVCV%s\n", s);
	break;
    case SVt_PVGV:
	PerlIO_printf(file, "PVGV%s\n", s);
	break;
    case SVt_PVFM:
	PerlIO_printf(file, "PVFM%s\n", s);
	break;
    case SVt_PVIO:
	PerlIO_printf(file, "PVIO%s\n", s);
	break;
    default:
	PerlIO_printf(file, "UNKNOWN(0x%"UVxf") %s\n", (UV)type, s);
	SvREFCNT_dec(d);
	return;
    }
    if (type >= SVt_PVIV || type == SVt_IV) {
	if (SvIsUV(sv))
	    Perl_dump_indent(aTHX_ level, file, "  UV = %"UVuf, (UV)SvUVX(sv));
	else
	    Perl_dump_indent(aTHX_ level, file, "  IV = %"IVdf, (IV)SvIVX(sv));
	if (SvOOK(sv))
	    PerlIO_printf(file, "  (OFFSET)");
	PerlIO_putc(file, '\n');
    }
    if (type >= SVt_PVNV || type == SVt_NV) {
	STORE_NUMERIC_LOCAL_SET_STANDARD();
	/* %Vg doesn't work? --jhi */
#ifdef USE_LONG_DOUBLE
	Perl_dump_indent(aTHX_ level, file, "  NV = %.*" PERL_PRIgldbl "\n", LDBL_DIG, SvNVX(sv));
#else
	Perl_dump_indent(aTHX_ level, file, "  NV = %.*g\n", DBL_DIG, SvNVX(sv));
#endif
	RESTORE_NUMERIC_LOCAL();
    }
    if (SvROK(sv)) {
	Perl_dump_indent(aTHX_ level, file, "  RV = 0x%"UVxf"\n", PTR2UV(SvRV(sv)));
	if (nest < maxnest)
	    do_sv_dump(level+1, file, SvRV(sv), nest+1, maxnest, dumpops, pvlim);
	SvREFCNT_dec(d);
	return;
    }
    if (type < SVt_PV) {
	SvREFCNT_dec(d);
	return;
    }
    if (type <= SVt_PVLV) {
	if (SvPVX(sv)) {
	    Perl_dump_indent(aTHX_ level, file,"  PV = 0x%"UVxf" ", PTR2UV(SvPVX(sv)));
	    if (SvOOK(sv))
		PerlIO_printf(file, "( %s . ) ", pv_display(d, SvPVX(sv)-SvIVX(sv), SvIVX(sv), 0, pvlim));
	    PerlIO_printf(file, "%s", pv_display(d, SvPVX(sv), SvCUR(sv), SvLEN(sv), pvlim));
	    if (SvUTF8(sv)) /* the 8?  \x{....} */
	        PerlIO_printf(file, " [UTF8 \"%s\"]", sv_uni_display(d, sv, 8 * sv_len_utf8(sv), UNI_DISPLAY_QQ));
	    PerlIO_printf(file, "\n");
	    Perl_dump_indent(aTHX_ level, file, "  CUR = %"IVdf"\n", (IV)SvCUR(sv));
	    Perl_dump_indent(aTHX_ level, file, "  LEN = %"IVdf"\n", (IV)SvLEN(sv));
	}
	else
	    Perl_dump_indent(aTHX_ level, file, "  PV = 0\n");
    }
    if (type >= SVt_PVMG) {
	if (SvMAGIC(sv))
            do_magic_dump(level, file, SvMAGIC(sv), nest, maxnest, dumpops, pvlim);
	if (SvSTASH(sv))
	    do_hv_dump(level, file, "  STASH", SvSTASH(sv));
    }
    switch (type) {
    case SVt_PVLV:
	Perl_dump_indent(aTHX_ level, file, "  TYPE = %c\n", LvTYPE(sv));
	Perl_dump_indent(aTHX_ level, file, "  TARGOFF = %"IVdf"\n", (IV)LvTARGOFF(sv));
	Perl_dump_indent(aTHX_ level, file, "  TARGLEN = %"IVdf"\n", (IV)LvTARGLEN(sv));
	Perl_dump_indent(aTHX_ level, file, "  TARG = 0x%"UVxf"\n", PTR2UV(LvTARG(sv)));
	/* XXX level+1 ??? */
	do_sv_dump(level, file, LvTARG(sv), nest+1, maxnest, dumpops, pvlim);
	break;
    case SVt_PVAV:
	Perl_dump_indent(aTHX_ level, file, "  ARRAY = 0x%"UVxf, PTR2UV(AvARRAY(sv)));
	if (AvARRAY(sv) != AvALLOC(sv)) {
	    PerlIO_printf(file, " (offset=%"IVdf")\n", (IV)(AvARRAY(sv) - AvALLOC(sv)));
	    Perl_dump_indent(aTHX_ level, file, "  ALLOC = 0x%"UVxf"\n", PTR2UV(AvALLOC(sv)));
	}
	else
	    PerlIO_putc(file, '\n');
	Perl_dump_indent(aTHX_ level, file, "  FILL = %"IVdf"\n", (IV)AvFILLp(sv));
	Perl_dump_indent(aTHX_ level, file, "  MAX = %"IVdf"\n", (IV)AvMAX(sv));
	Perl_dump_indent(aTHX_ level, file, "  ARYLEN = 0x%"UVxf"\n", PTR2UV(AvARYLEN(sv)));
	flags = AvFLAGS(sv);
	sv_setpv(d, "");
	if (flags & AVf_REAL)	sv_catpv(d, ",REAL");
	if (flags & AVf_REIFY)	sv_catpv(d, ",REIFY");
	if (flags & AVf_REUSED)	sv_catpv(d, ",REUSED");
	Perl_dump_indent(aTHX_ level, file, "  FLAGS = (%s)\n", SvCUR(d) ? SvPVX(d) + 1 : "");
	if (nest < maxnest && av_len((AV*)sv) >= 0) {
	    int count;
	    for (count = 0; count <=  av_len((AV*)sv) && count < maxnest; count++) {
		SV** elt = av_fetch((AV*)sv,count,0);

		Perl_dump_indent(aTHX_ level + 1, file, "Elt No. %"IVdf"\n", (IV)count);
		if (elt)
		    do_sv_dump(level+1, file, *elt, nest+1, maxnest, dumpops, pvlim);
	    }
	}
	break;
    case SVt_PVHV:
	Perl_dump_indent(aTHX_ level, file, "  ARRAY = 0x%"UVxf, PTR2UV(HvARRAY(sv)));
	if (HvARRAY(sv) && HvKEYS(sv)) {
	    /* Show distribution of HEs in the ARRAY */
	    int freq[200];
#define FREQ_MAX (sizeof freq / sizeof freq[0] - 1)
	    int i;
	    int max = 0;
	    U32 pow2 = 2, keys = HvKEYS(sv);
	    NV theoret, sum = 0;

	    PerlIO_printf(file, "  (");
	    Zero(freq, FREQ_MAX + 1, int);
	    for (i = 0; (STRLEN)i <= HvMAX(sv); i++) {
		HE* h; int count = 0;
                for (h = HvARRAY(sv)[i]; h; h = HeNEXT(h))
		    count++;
		if (count > FREQ_MAX)
		    count = FREQ_MAX;
	        freq[count]++;
	        if (max < count)
		    max = count;
	    }
	    for (i = 0; i <= max; i++) {
		if (freq[i]) {
		    PerlIO_printf(file, "%d%s:%d", i,
				  (i == FREQ_MAX) ? "+" : "",
				  freq[i]);
		    if (i != max)
			PerlIO_printf(file, ", ");
		}
            }
	    PerlIO_putc(file, ')');
	    /* The "quality" of a hash is defined as the total number of
	       comparisons needed to access every element once, relative
	       to the expected number needed for a random hash.

	       The total number of comparisons is equal to the sum of
	       the squares of the number of entries in each bucket.
	       For a random hash of n keys into k buckets, the expected
	       value is
				n + n(n-1)/2k
	    */

	    for (i = max; i > 0; i--) { /* Precision: count down. */
		sum += freq[i] * i * i;
            }
	    while ((keys = keys >> 1))
		pow2 = pow2 << 1;
	    theoret = HvKEYS(sv);
	    theoret += theoret * (theoret-1)/pow2;
	    PerlIO_putc(file, '\n');
	    Perl_dump_indent(aTHX_ level, file, "  hash quality = %.1"NVff"%%", theoret/sum*100);
	}
	PerlIO_putc(file, '\n');
	Perl_dump_indent(aTHX_ level, file, "  KEYS = %"IVdf"\n", (IV)HvKEYS(sv));
	Perl_dump_indent(aTHX_ level, file, "  FILL = %"IVdf"\n", (IV)HvFILL(sv));
	Perl_dump_indent(aTHX_ level, file, "  MAX = %"IVdf"\n", (IV)HvMAX(sv));
	Perl_dump_indent(aTHX_ level, file, "  RITER = %"IVdf"\n", (IV)HvRITER(sv));
	Perl_dump_indent(aTHX_ level, file, "  EITER = 0x%"UVxf"\n", PTR2UV(HvEITER(sv)));
	if (HvPMROOT(sv))
	    Perl_dump_indent(aTHX_ level, file, "  PMROOT = 0x%"UVxf"\n", PTR2UV(HvPMROOT(sv)));
	if (HvNAME(sv))
	    Perl_dump_indent(aTHX_ level, file, "  NAME = \"%s\"\n", HvNAME(sv));
	if (nest < maxnest && !HvEITER(sv)) { /* Try to preserve iterator */
	    HE *he;
	    HV *hv = (HV*)sv;
	    int count = maxnest - nest;

	    hv_iterinit(hv);
	    while ((he = hv_iternext_flags(hv, HV_ITERNEXT_WANTPLACEHOLDERS))
                   && count--) {
		SV *elt, *keysv;
		char *keypv;
		STRLEN len;
		U32 hash = HeHASH(he);

		keysv = hv_iterkeysv(he);
		keypv = SvPV(keysv, len);
		elt = hv_iterval(hv, he);
		Perl_dump_indent(aTHX_ level+1, file, "Elt %s ", pv_display(d, keypv, len, 0, pvlim));
		if (SvUTF8(keysv))
		    PerlIO_printf(file, "[UTF8 \"%s\"] ", sv_uni_display(d, keysv, 8 * sv_len_utf8(keysv), UNI_DISPLAY_QQ));
		PerlIO_printf(file, "HASH = 0x%"UVxf"\n", (UV)hash);
		do_sv_dump(level+1, file, elt, nest+1, maxnest, dumpops, pvlim);
	    }
	    hv_iterinit(hv);		/* Return to status quo */
	}
	break;
    case SVt_PVCV:
	if (SvPOK(sv))
	    Perl_dump_indent(aTHX_ level, file, "  PROTOTYPE = \"%s\"\n", SvPV(sv,n_a));
	/* FALL THROUGH */
    case SVt_PVFM:
	do_hv_dump(level, file, "  COMP_STASH", CvSTASH(sv));
	if (CvSTART(sv))
	    Perl_dump_indent(aTHX_ level, file, "  START = 0x%"UVxf" ===> %"IVdf"\n", PTR2UV(CvSTART(sv)), (IV)CvSTART(sv)->op_seq);
	Perl_dump_indent(aTHX_ level, file, "  ROOT = 0x%"UVxf"\n", PTR2UV(CvROOT(sv)));
        if (CvROOT(sv) && dumpops)
	    do_op_dump(level+1, file, CvROOT(sv));
	Perl_dump_indent(aTHX_ level, file, "  XSUB = 0x%"UVxf"\n", PTR2UV(CvXSUB(sv)));
	Perl_dump_indent(aTHX_ level, file, "  XSUBANY = %"IVdf"\n", (IV)CvXSUBANY(sv).any_i32);
 	do_gvgv_dump(level, file, "  GVGV::GV", CvGV(sv));
	Perl_dump_indent(aTHX_ level, file, "  FILE = \"%s\"\n", CvFILE(sv));
	Perl_dump_indent(aTHX_ level, file, "  DEPTH = %"IVdf"\n", (IV)CvDEPTH(sv));
#ifdef USE_5005THREADS
	Perl_dump_indent(aTHX_ level, file, "  MUTEXP = 0x%"UVxf"\n", PTR2UV(CvMUTEXP(sv)));
	Perl_dump_indent(aTHX_ level, file, "  OWNER = 0x%"UVxf"\n",  PTR2UV(CvOWNER(sv)));
#endif /* USE_5005THREADS */
	Perl_dump_indent(aTHX_ level, file, "  FLAGS = 0x%"UVxf"\n", (UV)CvFLAGS(sv));
	if (type == SVt_PVFM)
	    Perl_dump_indent(aTHX_ level, file, "  LINES = %"IVdf"\n", (IV)FmLINES(sv));
	Perl_dump_indent(aTHX_ level, file, "  PADLIST = 0x%"UVxf"\n", PTR2UV(CvPADLIST(sv)));
	if (nest < maxnest && CvPADLIST(sv)) {
	    AV* padlist = CvPADLIST(sv);
	    AV* pad_name = (AV*)*av_fetch(padlist, 0, FALSE);
	    AV* pad = (AV*)*av_fetch(padlist, 1, FALSE);
	    SV** pname = AvARRAY(pad_name);
	    SV** ppad = AvARRAY(pad);
	    I32 ix;

	    for (ix = 1; ix <= AvFILL(pad_name); ix++) {
		if (SvPOK(pname[ix]))
		    Perl_dump_indent(aTHX_ level,
				/* %5d below is enough whitespace. */
				file,
				"%5d. 0x%"UVxf" (%s\"%s\" %"IVdf"-%"IVdf")\n",
				(int)ix, PTR2UV(ppad[ix]),
				SvFAKE(pname[ix]) ? "FAKE " : "",
				SvPVX(pname[ix]),
				(IV)SvNVX(pname[ix]),
				(IV)SvIVX(pname[ix]));
	    }
	}
	{
	    CV *outside = CvOUTSIDE(sv);
	    Perl_dump_indent(aTHX_ level, file, "  OUTSIDE = 0x%"UVxf" (%s)\n",
			PTR2UV(outside),
			(!outside ? "null"
			 : CvANON(outside) ? "ANON"
			 : (outside == PL_main_cv) ? "MAIN"
			 : CvUNIQUE(outside) ? "UNIQUE"
			 : CvGV(outside) ? GvNAME(CvGV(outside)) : "UNDEFINED"));
	}
	if (nest < maxnest && (CvCLONE(sv) || CvCLONED(sv)))
	    do_sv_dump(level+1, file, (SV*)CvOUTSIDE(sv), nest+1, maxnest, dumpops, pvlim);
	break;
    case SVt_PVGV:
	Perl_dump_indent(aTHX_ level, file, "  NAME = \"%s\"\n", GvNAME(sv));
	Perl_dump_indent(aTHX_ level, file, "  NAMELEN = %"IVdf"\n", (IV)GvNAMELEN(sv));
	do_hv_dump (level, file, "  GvSTASH", GvSTASH(sv));
	Perl_dump_indent(aTHX_ level, file, "  GP = 0x%"UVxf"\n", PTR2UV(GvGP(sv)));
	if (!GvGP(sv))
	    break;
	Perl_dump_indent(aTHX_ level, file, "    SV = 0x%"UVxf"\n", PTR2UV(GvSV(sv)));
	Perl_dump_indent(aTHX_ level, file, "    REFCNT = %"IVdf"\n", (IV)GvREFCNT(sv));
	Perl_dump_indent(aTHX_ level, file, "    IO = 0x%"UVxf"\n", PTR2UV(GvIOp(sv)));
	Perl_dump_indent(aTHX_ level, file, "    FORM = 0x%"UVxf"  \n", PTR2UV(GvFORM(sv)));
	Perl_dump_indent(aTHX_ level, file, "    AV = 0x%"UVxf"\n", PTR2UV(GvAV(sv)));
	Perl_dump_indent(aTHX_ level, file, "    HV = 0x%"UVxf"\n", PTR2UV(GvHV(sv)));
	Perl_dump_indent(aTHX_ level, file, "    CV = 0x%"UVxf"\n", PTR2UV(GvCV(sv)));
	Perl_dump_indent(aTHX_ level, file, "    CVGEN = 0x%"UVxf"\n", (UV)GvCVGEN(sv));
	Perl_dump_indent(aTHX_ level, file, "    GPFLAGS = 0x%"UVxf"\n", (UV)GvGPFLAGS(sv));
	Perl_dump_indent(aTHX_ level, file, "    LINE = %"IVdf"\n", (IV)GvLINE(sv));
	Perl_dump_indent(aTHX_ level, file, "    FILE = \"%s\"\n", GvFILE(sv));
	Perl_dump_indent(aTHX_ level, file, "    FLAGS = 0x%"UVxf"\n", (UV)GvFLAGS(sv));
	do_gv_dump (level, file, "    EGV", GvEGV(sv));
	break;
    case SVt_PVIO:
	Perl_dump_indent(aTHX_ level, file, "  IFP = 0x%"UVxf"\n", PTR2UV(IoIFP(sv)));
	Perl_dump_indent(aTHX_ level, file, "  OFP = 0x%"UVxf"\n", PTR2UV(IoOFP(sv)));
	Perl_dump_indent(aTHX_ level, file, "  DIRP = 0x%"UVxf"\n", PTR2UV(IoDIRP(sv)));
	Perl_dump_indent(aTHX_ level, file, "  LINES = %"IVdf"\n", (IV)IoLINES(sv));
	Perl_dump_indent(aTHX_ level, file, "  PAGE = %"IVdf"\n", (IV)IoPAGE(sv));
	Perl_dump_indent(aTHX_ level, file, "  PAGE_LEN = %"IVdf"\n", (IV)IoPAGE_LEN(sv));
	Perl_dump_indent(aTHX_ level, file, "  LINES_LEFT = %"IVdf"\n", (IV)IoLINES_LEFT(sv));
        if (IoTOP_NAME(sv))
            Perl_dump_indent(aTHX_ level, file, "  TOP_NAME = \"%s\"\n", IoTOP_NAME(sv));
	do_gv_dump (level, file, "  TOP_GV", IoTOP_GV(sv));
        if (IoFMT_NAME(sv))
            Perl_dump_indent(aTHX_ level, file, "  FMT_NAME = \"%s\"\n", IoFMT_NAME(sv));
	do_gv_dump (level, file, "  FMT_GV", IoFMT_GV(sv));
        if (IoBOTTOM_NAME(sv))
            Perl_dump_indent(aTHX_ level, file, "  BOTTOM_NAME = \"%s\"\n", IoBOTTOM_NAME(sv));
	do_gv_dump (level, file, "  BOTTOM_GV", IoBOTTOM_GV(sv));
	Perl_dump_indent(aTHX_ level, file, "  SUBPROCESS = %"IVdf"\n", (IV)IoSUBPROCESS(sv));
	if (isPRINT(IoTYPE(sv)))
            Perl_dump_indent(aTHX_ level, file, "  TYPE = '%c'\n", IoTYPE(sv));
	else
            Perl_dump_indent(aTHX_ level, file, "  TYPE = '\\%o'\n", IoTYPE(sv));
	Perl_dump_indent(aTHX_ level, file, "  FLAGS = 0x%"UVxf"\n", (UV)IoFLAGS(sv));
	break;
    }
    SvREFCNT_dec(d);
}

void
Perl_sv_dump(pTHX_ SV *sv)
{
    do_sv_dump(0, Perl_debug_log, sv, 0, 0, 0, 0);
}

int
Perl_runops_debug(pTHX)
{
    if (!PL_op) {
	if (ckWARN_d(WARN_DEBUGGING))
	    Perl_warner(aTHX_ packWARN(WARN_DEBUGGING), "NULL OP IN RUN");
	return 0;
    }

    do {
	PERL_ASYNC_CHECK();
	if (PL_debug) {
	    if (PL_watchaddr != 0 && *PL_watchaddr != PL_watchok)
		PerlIO_printf(Perl_debug_log,
			      "WARNING: %"UVxf" changed from %"UVxf" to %"UVxf"\n",
			      PTR2UV(PL_watchaddr), PTR2UV(PL_watchok),
			      PTR2UV(*PL_watchaddr));
	    if (DEBUG_s_TEST_) debstack();
	    if (DEBUG_t_TEST_) debop(PL_op);
	    if (DEBUG_P_TEST_) debprof(PL_op);
	}
    } while ((PL_op = CALL_FPTR(PL_op->op_ppaddr)(aTHX)));

    TAINT_NOT;
    return 0;
}

I32
Perl_debop(pTHX_ OP *o)
{
    AV *padlist, *comppad;
    CV *cv;
    SV *sv;
    STRLEN n_a;

    if (CopSTASH_eq(PL_curcop, PL_debstash) && !DEBUG_J_TEST_)
	return 0;

    Perl_deb(aTHX_ "%s", OP_NAME(o));
    switch (o->op_type) {
    case OP_CONST:
	PerlIO_printf(Perl_debug_log, "(%s)", SvPEEK(cSVOPo_sv));
	break;
    case OP_GVSV:
    case OP_GV:
	if (cGVOPo_gv) {
	    sv = NEWSV(0,0);
	    gv_fullname3(sv, cGVOPo_gv, Nullch);
	    PerlIO_printf(Perl_debug_log, "(%s)", SvPV(sv, n_a));
	    SvREFCNT_dec(sv);
	}
	else
	    PerlIO_printf(Perl_debug_log, "(NULL)");
	break;
    case OP_PADSV:
    case OP_PADAV:
    case OP_PADHV:
	/* print the lexical's name */
        cv = deb_curcv(cxstack_ix);
        if (cv) {
            padlist = CvPADLIST(cv);
            comppad = (AV*)(*av_fetch(padlist, 0, FALSE));
            sv = *av_fetch(comppad, o->op_targ, FALSE);
        } else
            sv = Nullsv;
        if (sv)
           PerlIO_printf(Perl_debug_log, "(%s)", SvPV_nolen(sv));
        else
           PerlIO_printf(Perl_debug_log, "[%"UVuf"]", (UV)o->op_targ);
        break;
    default:
	break;
    }
    PerlIO_printf(Perl_debug_log, "\n");
    return 0;
}

STATIC CV*
S_deb_curcv(pTHX_ I32 ix)
{
    PERL_CONTEXT *cx = &cxstack[ix];
    if (CxTYPE(cx) == CXt_SUB || CxTYPE(cx) == CXt_FORMAT)
        return cx->blk_sub.cv;
    else if (CxTYPE(cx) == CXt_EVAL && !CxTRYBLOCK(cx))
        return PL_compcv;
    else if (ix == 0 && PL_curstackinfo->si_type == PERLSI_MAIN)
        return PL_main_cv;
    else if (ix <= 0)
        return Nullcv;
    else
        return deb_curcv(ix - 1);
}

void
Perl_watch(pTHX_ char **addr)
{
    PL_watchaddr = addr;
    PL_watchok = *addr;
    PerlIO_printf(Perl_debug_log, "WATCHING, %"UVxf" is currently %"UVxf"\n",
	PTR2UV(PL_watchaddr), PTR2UV(PL_watchok));
}

STATIC void
S_debprof(pTHX_ OP *o)
{
    if (CopSTASH_eq(PL_curcop, PL_debstash) && !DEBUG_J_TEST_)
	return;
    if (!PL_profiledata)
	Newz(000, PL_profiledata, MAXO, U32);
    ++PL_profiledata[o->op_type];
}

void
Perl_debprofdump(pTHX)
{
    unsigned i;
    if (!PL_profiledata)
	return;
    for (i = 0; i < MAXO; i++) {
	if (PL_profiledata[i])
	    PerlIO_printf(Perl_debug_log,
			  "%5lu %s\n", (unsigned long)PL_profiledata[i],
                                       PL_op_name[i]);
    }
}

/*    dump.c
 *
 *    Copyright (c) 1991-1997, Larry Wall
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
#include "perl.h"

#ifndef DEBUGGING
void
dump_all()
{
}
#else  /* Rest of file is for DEBUGGING */

#ifdef I_STDARG
static void dump(char *pat, ...);
#else
static void dump();
#endif

void
dump_all()
{
    PerlIO_setlinebuf(Perl_debug_log);
    if (main_root)
	dump_op(main_root);
    dump_packsubs(defstash);
}

void
dump_packsubs(stash)
HV* stash;
{
    I32	i;
    HE	*entry;

    if (!HvARRAY(stash))
	return;
    for (i = 0; i <= (I32) HvMAX(stash); i++) {
	for (entry = HvARRAY(stash)[i]; entry; entry = HeNEXT(entry)) {
	    GV *gv = (GV*)HeVAL(entry);
	    HV *hv;
	    if (GvCVu(gv))
		dump_sub(gv);
	    if (GvFORM(gv))
		dump_form(gv);
	    if (HeKEY(entry)[HeKLEN(entry)-1] == ':' &&
	      (hv = GvHV(gv)) && HvNAME(hv) && hv != defstash)
		dump_packsubs(hv);		/* nested package */
	}
    }
}

void
dump_sub(gv)
GV* gv;
{
    SV *sv = sv_newmortal();

    gv_fullname3(sv, gv, Nullch);
    dump("\nSUB %s = ", SvPVX(sv));
    if (CvXSUB(GvCV(gv)))
	dump("(xsub 0x%x %d)\n",
	    (long)CvXSUB(GvCV(gv)),
	    CvXSUBANY(GvCV(gv)).any_i32);
    else if (CvROOT(GvCV(gv)))
	dump_op(CvROOT(GvCV(gv)));
    else
	dump("<undef>\n");
}

void
dump_form(gv)
GV* gv;
{
    SV *sv = sv_newmortal();

    gv_fullname3(sv, gv, Nullch);
    dump("\nFORMAT %s = ", SvPVX(sv));
    if (CvROOT(GvFORM(gv)))
	dump_op(CvROOT(GvFORM(gv)));
    else
	dump("<undef>\n");
}

void
dump_eval()
{
    dump_op(eval_root);
}

void
dump_op(op)
register OP *op;
{
    dump("{\n");
    if (op->op_seq)
	PerlIO_printf(Perl_debug_log, "%-4d", op->op_seq);
    else
	PerlIO_printf(Perl_debug_log, "    ");
    dump("TYPE = %s  ===> ", op_name[op->op_type]);
    if (op->op_next) {
	if (op->op_seq)
	    PerlIO_printf(Perl_debug_log, "%d\n", op->op_next->op_seq);
	else
	    PerlIO_printf(Perl_debug_log, "(%d)\n", op->op_next->op_seq);
    }
    else
	PerlIO_printf(Perl_debug_log, "DONE\n");
    dumplvl++;
    if (op->op_targ) {
	if (op->op_type == OP_NULL)
	    dump("  (was %s)\n", op_name[op->op_targ]);
	else
	    dump("TARG = %d\n", op->op_targ);
    }
#ifdef DUMPADDR
    dump("ADDR = 0x%lx => 0x%lx\n",op, op->op_next);
#endif
    if (op->op_flags) {
	SV *tmpsv = newSVpv("", 0);
	switch (op->op_flags & OPf_WANT) {
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
	if (op->op_flags & OPf_KIDS)
	    sv_catpv(tmpsv, ",KIDS");
	if (op->op_flags & OPf_PARENS)
	    sv_catpv(tmpsv, ",PARENS");
	if (op->op_flags & OPf_STACKED)
	    sv_catpv(tmpsv, ",STACKED");
	if (op->op_flags & OPf_REF)
	    sv_catpv(tmpsv, ",REF");
	if (op->op_flags & OPf_MOD)
	    sv_catpv(tmpsv, ",MOD");
	if (op->op_flags & OPf_SPECIAL)
	    sv_catpv(tmpsv, ",SPECIAL");
	dump("FLAGS = (%s)\n", SvCUR(tmpsv) ? SvPVX(tmpsv) + 1 : "");
	SvREFCNT_dec(tmpsv);
    }
    if (op->op_private) {
	SV *tmpsv = newSVpv("", 0);
	if (op->op_type == OP_AASSIGN) {
	    if (op->op_private & OPpASSIGN_COMMON)
		sv_catpv(tmpsv, ",COMMON");
	}
	else if (op->op_type == OP_SASSIGN) {
	    if (op->op_private & OPpASSIGN_BACKWARDS)
		sv_catpv(tmpsv, ",BACKWARDS");
	}
	else if (op->op_type == OP_TRANS) {
	    if (op->op_private & OPpTRANS_SQUASH)
		sv_catpv(tmpsv, ",SQUASH");
	    if (op->op_private & OPpTRANS_DELETE)
		sv_catpv(tmpsv, ",DELETE");
	    if (op->op_private & OPpTRANS_COMPLEMENT)
		sv_catpv(tmpsv, ",COMPLEMENT");
	}
	else if (op->op_type == OP_REPEAT) {
	    if (op->op_private & OPpREPEAT_DOLIST)
		sv_catpv(tmpsv, ",DOLIST");
	}
	else if (op->op_type == OP_ENTERSUB ||
		 op->op_type == OP_RV2SV ||
		 op->op_type == OP_RV2AV ||
		 op->op_type == OP_RV2HV ||
		 op->op_type == OP_RV2GV ||
		 op->op_type == OP_AELEM ||
		 op->op_type == OP_HELEM )
	{
	    if (op->op_type == OP_ENTERSUB) {
		if (op->op_private & OPpENTERSUB_AMPER)
		    sv_catpv(tmpsv, ",AMPER");
		if (op->op_private & OPpENTERSUB_DB)
		    sv_catpv(tmpsv, ",DB");
	    }
	    switch (op->op_private & OPpDEREF) {
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
	    if (op->op_type == OP_AELEM || op->op_type == OP_HELEM) {
		if (op->op_private & OPpLVAL_DEFER)
		    sv_catpv(tmpsv, ",LVAL_DEFER");
	    }
	    else {
		if (op->op_private & HINT_STRICT_REFS)
		    sv_catpv(tmpsv, ",STRICT_REFS");
	    }
	}
	else if (op->op_type == OP_CONST) {
	    if (op->op_private & OPpCONST_BARE)
		sv_catpv(tmpsv, ",BARE");
	}
	else if (op->op_type == OP_FLIP) {
	    if (op->op_private & OPpFLIP_LINENUM)
		sv_catpv(tmpsv, ",LINENUM");
	}
	else if (op->op_type == OP_FLOP) {
	    if (op->op_private & OPpFLIP_LINENUM)
		sv_catpv(tmpsv, ",LINENUM");
	}
	if (op->op_flags & OPf_MOD && op->op_private & OPpLVAL_INTRO)
	    sv_catpv(tmpsv, ",INTRO");
	if (SvCUR(tmpsv))
	    dump("PRIVATE = (%s)\n", SvPVX(tmpsv) + 1);
	SvREFCNT_dec(tmpsv);
    }

    switch (op->op_type) {
    case OP_GVSV:
    case OP_GV:
	if (cGVOP->op_gv) {
	    SV *tmpsv = NEWSV(0,0);
	    ENTER;
	    SAVEFREESV(tmpsv);
	    gv_fullname3(tmpsv, cGVOP->op_gv, Nullch);
	    dump("GV = %s\n", SvPV(tmpsv, na));
	    LEAVE;
	}
	else
	    dump("GV = NULL\n");
	break;
    case OP_CONST:
	dump("SV = %s\n", SvPEEK(cSVOP->op_sv));
	break;
    case OP_NEXTSTATE:
    case OP_DBSTATE:
	if (cCOP->cop_line)
	    dump("LINE = %d\n",cCOP->cop_line);
	if (cCOP->cop_label)
	    dump("LABEL = \"%s\"\n",cCOP->cop_label);
	break;
    case OP_ENTERLOOP:
	dump("REDO ===> ");
	if (cLOOP->op_redoop)
	    PerlIO_printf(Perl_debug_log, "%d\n", cLOOP->op_redoop->op_seq);
	else
	    PerlIO_printf(Perl_debug_log, "DONE\n");
	dump("NEXT ===> ");
	if (cLOOP->op_nextop)
	    PerlIO_printf(Perl_debug_log, "%d\n", cLOOP->op_nextop->op_seq);
	else
	    PerlIO_printf(Perl_debug_log, "DONE\n");
	dump("LAST ===> ");
	if (cLOOP->op_lastop)
	    PerlIO_printf(Perl_debug_log, "%d\n", cLOOP->op_lastop->op_seq);
	else
	    PerlIO_printf(Perl_debug_log, "DONE\n");
	break;
    case OP_COND_EXPR:
	dump("TRUE ===> ");
	if (cCONDOP->op_true)
	    PerlIO_printf(Perl_debug_log, "%d\n", cCONDOP->op_true->op_seq);
	else
	    PerlIO_printf(Perl_debug_log, "DONE\n");
	dump("FALSE ===> ");
	if (cCONDOP->op_false)
	    PerlIO_printf(Perl_debug_log, "%d\n", cCONDOP->op_false->op_seq);
	else
	    PerlIO_printf(Perl_debug_log, "DONE\n");
	break;
    case OP_MAPWHILE:
    case OP_GREPWHILE:
    case OP_OR:
    case OP_AND:
	dump("OTHER ===> ");
	if (cLOGOP->op_other)
	    PerlIO_printf(Perl_debug_log, "%d\n", cLOGOP->op_other->op_seq);
	else
	    PerlIO_printf(Perl_debug_log, "DONE\n");
	break;
    case OP_PUSHRE:
    case OP_MATCH:
    case OP_SUBST:
	dump_pm((PMOP*)op);
	break;
    default:
	break;
    }
    if (op->op_flags & OPf_KIDS) {
	OP *kid;
	for (kid = cUNOP->op_first; kid; kid = kid->op_sibling)
	    dump_op(kid);
    }
    dumplvl--;
    dump("}\n");
}

void
dump_gv(gv)
register GV *gv;
{
    SV *sv;

    if (!gv) {
	PerlIO_printf(Perl_debug_log, "{}\n");
	return;
    }
    sv = sv_newmortal();
    dumplvl++;
    PerlIO_printf(Perl_debug_log, "{\n");
    gv_fullname3(sv, gv, Nullch);
    dump("GV_NAME = %s", SvPVX(sv));
    if (gv != GvEGV(gv)) {
	gv_efullname3(sv, GvEGV(gv), Nullch);
	dump("-> %s", SvPVX(sv));
    }
    dump("\n");
    dumplvl--;
    dump("}\n");
}

void
dump_pm(pm)
register PMOP *pm;
{
    char ch;

    if (!pm) {
	dump("{}\n");
	return;
    }
    dump("{\n");
    dumplvl++;
    if (pm->op_pmflags & PMf_ONCE)
	ch = '?';
    else
	ch = '/';
    if (pm->op_pmregexp)
	dump("PMf_PRE %c%s%c%s\n",
	     ch, pm->op_pmregexp->precomp, ch,
	     (pm->op_private & OPpRUNTIME) ? " (RUNTIME)" : "");
    else
	dump("PMf_PRE (RUNTIME)\n");
    if (pm->op_type != OP_PUSHRE && pm->op_pmreplroot) {
	dump("PMf_REPL = ");
	dump_op(pm->op_pmreplroot);
    }
    if (pm->op_pmshort) {
	dump("PMf_SHORT = %s\n",SvPEEK(pm->op_pmshort));
    }
    if (pm->op_pmflags) {
	SV *tmpsv = newSVpv("", 0);
	if (pm->op_pmflags & PMf_USED)
	    sv_catpv(tmpsv, ",USED");
	if (pm->op_pmflags & PMf_ONCE)
	    sv_catpv(tmpsv, ",ONCE");
	if (pm->op_pmflags & PMf_SCANFIRST)
	    sv_catpv(tmpsv, ",SCANFIRST");
	if (pm->op_pmflags & PMf_ALL)
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
	if (pm->op_pmflags & PMf_EVAL)
	    sv_catpv(tmpsv, ",EVAL");
	dump("PMFLAGS = (%s)\n", SvCUR(tmpsv) ? SvPVX(tmpsv) + 1 : "");
	SvREFCNT_dec(tmpsv);
    }

    dumplvl--;
    dump("}\n");
}


#if !defined(I_STDARG) && !defined(I_VARARGS)
/* VARARGS1 */
static void dump(arg1,arg2,arg3,arg4,arg5)
char *arg1;
long arg2, arg3, arg4, arg5;
{
    I32 i;

    for (i = dumplvl*4; i; i--)
	(void)PerlIO_putc(Perl_debug_log,' ');
    PerlIO_printf(Perl_debug_log, arg1, arg2, arg3, arg4, arg5);
}

#else

#ifdef I_STDARG
static void
dump(char *pat,...)
#else
/*VARARGS0*/
static void
dump(pat,va_alist)
    char *pat;
    va_dcl
#endif
{
    I32 i;
    va_list args;

#ifdef I_STDARG
    va_start(args, pat);
#else
    va_start(args);
#endif
    for (i = dumplvl*4; i; i--)
	(void)PerlIO_putc(Perl_debug_log,' ');
    PerlIO_vprintf(Perl_debug_log,pat,args);
    va_end(args);
}
#endif

#endif

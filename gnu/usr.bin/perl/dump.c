/*    dump.c
 *
 *    Copyright (c) 1991-1994, Larry Wall
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

static void dump();

void
dump_all()
{
#ifdef HAS_SETLINEBUF
    setlinebuf(stderr);
#else
    setvbuf(stderr, Nullch, _IOLBF, 0);
#endif
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
	for (entry = HvARRAY(stash)[i]; entry; entry = entry->hent_next) {
	    GV *gv = (GV*)entry->hent_val;
	    HV *hv;
	    if (GvCV(gv))
		dump_sub(gv);
	    if (GvFORM(gv))
		dump_form(gv);
	    if (entry->hent_key[entry->hent_klen-1] == ':' &&
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

    gv_fullname(sv,gv);
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

    gv_fullname(sv,gv);
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
    SV *tmpsv;

    dump("{\n");
    if (op->op_seq)
	fprintf(stderr, "%-4d", op->op_seq);
    else
	fprintf(stderr, "    ");
    dump("TYPE = %s  ===> ", op_name[op->op_type]);
    if (op->op_next) {
	if (op->op_seq)
	    fprintf(stderr, "%d\n", op->op_next->op_seq);
	else
	    fprintf(stderr, "(%d)\n", op->op_next->op_seq);
    }
    else
	fprintf(stderr, "DONE\n");
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
	*buf = '\0';
	if (op->op_flags & OPf_KNOW) {
	    if (op->op_flags & OPf_LIST)
		(void)strcat(buf,"LIST,");
	    else
		(void)strcat(buf,"SCALAR,");
	}
	else
	    (void)strcat(buf,"UNKNOWN,");
	if (op->op_flags & OPf_KIDS)
	    (void)strcat(buf,"KIDS,");
	if (op->op_flags & OPf_PARENS)
	    (void)strcat(buf,"PARENS,");
	if (op->op_flags & OPf_STACKED)
	    (void)strcat(buf,"STACKED,");
	if (op->op_flags & OPf_REF)
	    (void)strcat(buf,"REF,");
	if (op->op_flags & OPf_MOD)
	    (void)strcat(buf,"MOD,");
	if (op->op_flags & OPf_SPECIAL)
	    (void)strcat(buf,"SPECIAL,");
	if (*buf)
	    buf[strlen(buf)-1] = '\0';
	dump("FLAGS = (%s)\n",buf);
    }
    if (op->op_private) {
	*buf = '\0';
	if (op->op_type == OP_AASSIGN) {
	    if (op->op_private & OPpASSIGN_COMMON)
		(void)strcat(buf,"COMMON,");
	}
	else if (op->op_type == OP_SASSIGN) {
	    if (op->op_private & OPpASSIGN_BACKWARDS)
		(void)strcat(buf,"BACKWARDS,");
	}
	else if (op->op_type == OP_TRANS) {
	    if (op->op_private & OPpTRANS_SQUASH)
		(void)strcat(buf,"SQUASH,");
	    if (op->op_private & OPpTRANS_DELETE)
		(void)strcat(buf,"DELETE,");
	    if (op->op_private & OPpTRANS_COMPLEMENT)
		(void)strcat(buf,"COMPLEMENT,");
	}
	else if (op->op_type == OP_REPEAT) {
	    if (op->op_private & OPpREPEAT_DOLIST)
		(void)strcat(buf,"DOLIST,");
	}
	else if (op->op_type == OP_ENTERSUB ||
		 op->op_type == OP_RV2SV ||
		 op->op_type == OP_RV2AV ||
		 op->op_type == OP_RV2HV ||
		 op->op_type == OP_RV2GV ||
		 op->op_type == OP_AELEM ||
		 op->op_type == OP_HELEM )
	{
	    if (op->op_private & OPpENTERSUB_AMPER)
		(void)strcat(buf,"AMPER,");
	    if (op->op_private & OPpENTERSUB_DB)
		(void)strcat(buf,"DB,");
	    if (op->op_private & OPpDEREF_AV)
		(void)strcat(buf,"AV,");
	    if (op->op_private & OPpDEREF_HV)
		(void)strcat(buf,"HV,");
	    if (op->op_private & HINT_STRICT_REFS)
		(void)strcat(buf,"STRICT_REFS,");
	}
	else if (op->op_type == OP_CONST) {
	    if (op->op_private & OPpCONST_BARE)
		(void)strcat(buf,"BARE,");
	}
	else if (op->op_type == OP_FLIP) {
	    if (op->op_private & OPpFLIP_LINENUM)
		(void)strcat(buf,"LINENUM,");
	}
	else if (op->op_type == OP_FLOP) {
	    if (op->op_private & OPpFLIP_LINENUM)
		(void)strcat(buf,"LINENUM,");
	}
	if (op->op_flags & OPf_MOD && op->op_private & OPpLVAL_INTRO)
	    (void)strcat(buf,"INTRO,");
	if (*buf) {
	    buf[strlen(buf)-1] = '\0';
	    dump("PRIVATE = (%s)\n",buf);
	}
    }

    switch (op->op_type) {
    case OP_GVSV:
    case OP_GV:
	if (cGVOP->op_gv) {
	    ENTER;
	    tmpsv = NEWSV(0,0);
	    SAVEFREESV(tmpsv);
	    gv_fullname(tmpsv,cGVOP->op_gv);
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
	    fprintf(stderr, "%d\n", cLOOP->op_redoop->op_seq);
	else
	    fprintf(stderr, "DONE\n");
	dump("NEXT ===> ");
	if (cLOOP->op_nextop)
	    fprintf(stderr, "%d\n", cLOOP->op_nextop->op_seq);
	else
	    fprintf(stderr, "DONE\n");
	dump("LAST ===> ");
	if (cLOOP->op_lastop)
	    fprintf(stderr, "%d\n", cLOOP->op_lastop->op_seq);
	else
	    fprintf(stderr, "DONE\n");
	break;
    case OP_COND_EXPR:
	dump("TRUE ===> ");
	if (cCONDOP->op_true)
	    fprintf(stderr, "%d\n", cCONDOP->op_true->op_seq);
	else
	    fprintf(stderr, "DONE\n");
	dump("FALSE ===> ");
	if (cCONDOP->op_false)
	    fprintf(stderr, "%d\n", cCONDOP->op_false->op_seq);
	else
	    fprintf(stderr, "DONE\n");
	break;
    case OP_MAPWHILE:
    case OP_GREPWHILE:
    case OP_OR:
    case OP_AND:
	dump("OTHER ===> ");
	if (cLOGOP->op_other)
	    fprintf(stderr, "%d\n", cLOGOP->op_other->op_seq);
	else
	    fprintf(stderr, "DONE\n");
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
	fprintf(stderr,"{}\n");
	return;
    }
    sv = sv_newmortal();
    dumplvl++;
    fprintf(stderr,"{\n");
    gv_fullname(sv,gv);
    dump("GV_NAME = %s", SvPVX(sv));
    if (gv != GvEGV(gv)) {
	gv_efullname(sv,GvEGV(gv));
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
	dump("PMf_PRE %c%s%c\n",ch,pm->op_pmregexp->precomp,ch);
    if (pm->op_type != OP_PUSHRE && pm->op_pmreplroot) {
	dump("PMf_REPL = ");
	dump_op(pm->op_pmreplroot);
    }
    if (pm->op_pmshort) {
	dump("PMf_SHORT = %s\n",SvPEEK(pm->op_pmshort));
    }
    if (pm->op_pmflags) {
	*buf = '\0';
	if (pm->op_pmflags & PMf_USED)
	    (void)strcat(buf,"USED,");
	if (pm->op_pmflags & PMf_ONCE)
	    (void)strcat(buf,"ONCE,");
	if (pm->op_pmflags & PMf_SCANFIRST)
	    (void)strcat(buf,"SCANFIRST,");
	if (pm->op_pmflags & PMf_ALL)
	    (void)strcat(buf,"ALL,");
	if (pm->op_pmflags & PMf_SKIPWHITE)
	    (void)strcat(buf,"SKIPWHITE,");
	if (pm->op_pmflags & PMf_FOLD)
	    (void)strcat(buf,"FOLD,");
	if (pm->op_pmflags & PMf_CONST)
	    (void)strcat(buf,"CONST,");
	if (pm->op_pmflags & PMf_KEEP)
	    (void)strcat(buf,"KEEP,");
	if (pm->op_pmflags & PMf_GLOBAL)
	    (void)strcat(buf,"GLOBAL,");
	if (pm->op_pmflags & PMf_RUNTIME)
	    (void)strcat(buf,"RUNTIME,");
	if (pm->op_pmflags & PMf_EVAL)
	    (void)strcat(buf,"EVAL,");
	if (*buf)
	    buf[strlen(buf)-1] = '\0';
	dump("PMFLAGS = (%s)\n",buf);
    }

    dumplvl--;
    dump("}\n");
}

/* VARARGS1 */
static void dump(arg1,arg2,arg3,arg4,arg5)
char *arg1;
long arg2, arg3, arg4, arg5;
{
    I32 i;

    for (i = dumplvl*4; i; i--)
	(void)putc(' ',stderr);
    fprintf(stderr,arg1, arg2, arg3, arg4, arg5);
}
#endif

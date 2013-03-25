/*	B.xs
 *
 *	Copyright (c) 1996 Malcolm Beattie
 *
 *	You may distribute under the terms of either the GNU General Public
 *	License or the Artistic License, as specified in the README file.
 *
 */

#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifdef PerlIO
typedef PerlIO * InputStream;
#else
typedef FILE * InputStream;
#endif


static const char* const svclassnames[] = {
    "B::NULL",
#if PERL_VERSION >= 9
    "B::BIND",
#endif
    "B::IV",
    "B::NV",
#if PERL_VERSION <= 10
    "B::RV",
#endif
    "B::PV",
    "B::PVIV",
    "B::PVNV",
    "B::PVMG",
#if PERL_VERSION <= 8
    "B::BM",
#endif
#if PERL_VERSION >= 11
    "B::REGEXP",
#endif
#if PERL_VERSION >= 9
    "B::GV",
#endif
    "B::PVLV",
    "B::AV",
    "B::HV",
    "B::CV",
#if PERL_VERSION <= 8
    "B::GV",
#endif
    "B::FM",
    "B::IO",
};

typedef enum {
    OPc_NULL,	/* 0 */
    OPc_BASEOP,	/* 1 */
    OPc_UNOP,	/* 2 */
    OPc_BINOP,	/* 3 */
    OPc_LOGOP,	/* 4 */
    OPc_LISTOP,	/* 5 */
    OPc_PMOP,	/* 6 */
    OPc_SVOP,	/* 7 */
    OPc_PADOP,	/* 8 */
    OPc_PVOP,	/* 9 */
    OPc_LOOP,	/* 10 */
    OPc_COP	/* 11 */
} opclass;

static const char* const opclassnames[] = {
    "B::NULL",
    "B::OP",
    "B::UNOP",
    "B::BINOP",
    "B::LOGOP",
    "B::LISTOP",
    "B::PMOP",
    "B::SVOP",
    "B::PADOP",
    "B::PVOP",
    "B::LOOP",
    "B::COP"	
};

static const size_t opsizes[] = {
    0,	
    sizeof(OP),
    sizeof(UNOP),
    sizeof(BINOP),
    sizeof(LOGOP),
    sizeof(LISTOP),
    sizeof(PMOP),
    sizeof(SVOP),
    sizeof(PADOP),
    sizeof(PVOP),
    sizeof(LOOP),
    sizeof(COP)	
};

#define MY_CXT_KEY "B::_guts" XS_VERSION

typedef struct {
    int		x_walkoptree_debug;	/* Flag for walkoptree debug hook */
    SV *	x_specialsv_list[7];
} my_cxt_t;

START_MY_CXT

#define walkoptree_debug	(MY_CXT.x_walkoptree_debug)
#define specialsv_list		(MY_CXT.x_specialsv_list)

static opclass
cc_opclass(pTHX_ const OP *o)
{
    bool custom = 0;

    if (!o)
	return OPc_NULL;

    if (o->op_type == 0)
	return (o->op_flags & OPf_KIDS) ? OPc_UNOP : OPc_BASEOP;

    if (o->op_type == OP_SASSIGN)
	return ((o->op_private & OPpASSIGN_BACKWARDS) ? OPc_UNOP : OPc_BINOP);

    if (o->op_type == OP_AELEMFAST) {
#if PERL_VERSION <= 14
	if (o->op_flags & OPf_SPECIAL)
	    return OPc_BASEOP;
	else
#endif
#ifdef USE_ITHREADS
	    return OPc_PADOP;
#else
	    return OPc_SVOP;
#endif
    }
    
#ifdef USE_ITHREADS
    if (o->op_type == OP_GV || o->op_type == OP_GVSV ||
	o->op_type == OP_RCATLINE)
	return OPc_PADOP;
#endif

    if (o->op_type == OP_CUSTOM)
        custom = 1;

    switch (OP_CLASS(o)) {
    case OA_BASEOP:
	return OPc_BASEOP;

    case OA_UNOP:
	return OPc_UNOP;

    case OA_BINOP:
	return OPc_BINOP;

    case OA_LOGOP:
	return OPc_LOGOP;

    case OA_LISTOP:
	return OPc_LISTOP;

    case OA_PMOP:
	return OPc_PMOP;

    case OA_SVOP:
	return OPc_SVOP;

    case OA_PADOP:
	return OPc_PADOP;

    case OA_PVOP_OR_SVOP:
        /*
         * Character translations (tr///) are usually a PVOP, keeping a 
         * pointer to a table of shorts used to look up translations.
         * Under utf8, however, a simple table isn't practical; instead,
         * the OP is an SVOP (or, under threads, a PADOP),
         * and the SV is a reference to a swash
         * (i.e., an RV pointing to an HV).
         */
	return (!custom &&
		   (o->op_private & (OPpTRANS_TO_UTF|OPpTRANS_FROM_UTF))
	       )
#if  defined(USE_ITHREADS) \
  && (PERL_VERSION > 8 || (PERL_VERSION == 8 && PERL_SUBVERSION >= 9))
		? OPc_PADOP : OPc_PVOP;
#else
		? OPc_SVOP : OPc_PVOP;
#endif

    case OA_LOOP:
	return OPc_LOOP;

    case OA_COP:
	return OPc_COP;

    case OA_BASEOP_OR_UNOP:
	/*
	 * UNI(OP_foo) in toke.c returns token UNI or FUNC1 depending on
	 * whether parens were seen. perly.y uses OPf_SPECIAL to
	 * signal whether a BASEOP had empty parens or none.
	 * Some other UNOPs are created later, though, so the best
	 * test is OPf_KIDS, which is set in newUNOP.
	 */
	return (o->op_flags & OPf_KIDS) ? OPc_UNOP : OPc_BASEOP;

    case OA_FILESTATOP:
	/*
	 * The file stat OPs are created via UNI(OP_foo) in toke.c but use
	 * the OPf_REF flag to distinguish between OP types instead of the
	 * usual OPf_SPECIAL flag. As usual, if OPf_KIDS is set, then we
	 * return OPc_UNOP so that walkoptree can find our children. If
	 * OPf_KIDS is not set then we check OPf_REF. Without OPf_REF set
	 * (no argument to the operator) it's an OP; with OPf_REF set it's
	 * an SVOP (and op_sv is the GV for the filehandle argument).
	 */
	return ((o->op_flags & OPf_KIDS) ? OPc_UNOP :
#ifdef USE_ITHREADS
		(o->op_flags & OPf_REF) ? OPc_PADOP : OPc_BASEOP);
#else
		(o->op_flags & OPf_REF) ? OPc_SVOP : OPc_BASEOP);
#endif
    case OA_LOOPEXOP:
	/*
	 * next, last, redo, dump and goto use OPf_SPECIAL to indicate that a
	 * label was omitted (in which case it's a BASEOP) or else a term was
	 * seen. In this last case, all except goto are definitely PVOP but
	 * goto is either a PVOP (with an ordinary constant label), an UNOP
	 * with OPf_STACKED (with a non-constant non-sub) or an UNOP for
	 * OP_REFGEN (with goto &sub) in which case OPf_STACKED also seems to
	 * get set.
	 */
	if (o->op_flags & OPf_STACKED)
	    return OPc_UNOP;
	else if (o->op_flags & OPf_SPECIAL)
	    return OPc_BASEOP;
	else
	    return OPc_PVOP;
    }
    warn("can't determine class of operator %s, assuming BASEOP\n",
	 OP_NAME(o));
    return OPc_BASEOP;
}

static SV *
make_op_object(pTHX_ const OP *o)
{
    SV *opsv = sv_newmortal();
    sv_setiv(newSVrv(opsv, opclassnames[cc_opclass(aTHX_ o)]), PTR2IV(o));
    return opsv;
}

static SV *
make_sv_object(pTHX_ SV *sv)
{
    SV *const arg = sv_newmortal();
    const char *type = 0;
    IV iv;
    dMY_CXT;

    for (iv = 0; iv < (IV)(sizeof(specialsv_list)/sizeof(SV*)); iv++) {
	if (sv == specialsv_list[iv]) {
	    type = "B::SPECIAL";
	    break;
	}
    }
    if (!type) {
	type = svclassnames[SvTYPE(sv)];
	iv = PTR2IV(sv);
    }
    sv_setiv(newSVrv(arg, type), iv);
    return arg;
}

#if PERL_VERSION >= 9
static SV *
make_temp_object(pTHX_ SV *temp)
{
    SV *target;
    SV *arg = sv_newmortal();
    const char *const type = svclassnames[SvTYPE(temp)];
    const IV iv = PTR2IV(temp);

    target = newSVrv(arg, type);
    sv_setiv(target, iv);

    /* Need to keep our "temp" around as long as the target exists.
       Simplest way seems to be to hang it from magic, and let that clear
       it up.  No vtable, so won't actually get in the way of anything.  */
    sv_magicext(target, temp, PERL_MAGIC_sv, NULL, NULL, 0);
    /* magic object has had its reference count increased, so we must drop
       our reference.  */
    SvREFCNT_dec(temp);
    return arg;
}

static SV *
make_warnings_object(pTHX_ const COP *const cop)
{
    const STRLEN *const warnings = cop->cop_warnings;
    const char *type = 0;
    dMY_CXT;
    IV iv = sizeof(specialsv_list)/sizeof(SV*);

    /* Counting down is deliberate. Before the split between make_sv_object
       and make_warnings_obj there appeared to be a bug - Nullsv and pWARN_STD
       were both 0, so you could never get a B::SPECIAL for pWARN_STD  */

    while (iv--) {
	if ((SV*)warnings == specialsv_list[iv]) {
	    type = "B::SPECIAL";
	    break;
	}
    }
    if (type) {
	SV *arg = sv_newmortal();
	sv_setiv(newSVrv(arg, type), iv);
	return arg;
    } else {
	/* B assumes that warnings are a regular SV. Seems easier to keep it
	   happy by making them into a regular SV.  */
	return make_temp_object(aTHX_ newSVpvn((char *)(warnings + 1), *warnings));
    }
}

static SV *
make_cop_io_object(pTHX_ COP *cop)
{
    SV *const value = newSV(0);

    Perl_emulate_cop_io(aTHX_ cop, value);

    if(SvOK(value)) {
	return make_sv_object(aTHX_ value);
    } else {
	SvREFCNT_dec(value);
	return make_sv_object(aTHX_ NULL);
    }
}
#endif

static SV *
make_mg_object(pTHX_ MAGIC *mg)
{
    SV *arg = sv_newmortal();
    sv_setiv(newSVrv(arg, "B::MAGIC"), PTR2IV(mg));
    return arg;
}

static SV *
cstring(pTHX_ SV *sv, bool perlstyle)
{
    SV *sstr;

    if (!SvOK(sv))
	return newSVpvs_flags("0", SVs_TEMP);

    sstr = newSVpvs_flags("\"", SVs_TEMP);

    if (perlstyle && SvUTF8(sv)) {
	SV *tmpsv = sv_newmortal(); /* Temporary SV to feed sv_uni_display */
	const STRLEN len = SvCUR(sv);
	const char *s = sv_uni_display(tmpsv, sv, 8*len, UNI_DISPLAY_QQ);
	while (*s)
	{
	    if (*s == '"')
		sv_catpvs(sstr, "\\\"");
	    else if (*s == '$')
		sv_catpvs(sstr, "\\$");
	    else if (*s == '@')
		sv_catpvs(sstr, "\\@");
	    else if (*s == '\\')
	    {
		if (strchr("nrftax\\",*(s+1)))
		    sv_catpvn(sstr, s++, 2);
		else
		    sv_catpvs(sstr, "\\\\");
	    }
	    else /* should always be printable */
		sv_catpvn(sstr, s, 1);
	    ++s;
	}
    }
    else
    {
	/* XXX Optimise? */
	STRLEN len;
	const char *s = SvPV(sv, len);
	for (; len; len--, s++)
	{
	    /* At least try a little for readability */
	    if (*s == '"')
		sv_catpvs(sstr, "\\\"");
	    else if (*s == '\\')
		sv_catpvs(sstr, "\\\\");
            /* trigraphs - bleagh */
            else if (!perlstyle && *s == '?' && len>=3 && s[1] == '?') {
                Perl_sv_catpvf(aTHX_ sstr, "\\%03o", '?');
            }
	    else if (perlstyle && *s == '$')
		sv_catpvs(sstr, "\\$");
	    else if (perlstyle && *s == '@')
		sv_catpvs(sstr, "\\@");
#ifdef EBCDIC
	    else if (isPRINT(*s))
#else
	    else if (*s >= ' ' && *s < 127)
#endif /* EBCDIC */
		sv_catpvn(sstr, s, 1);
	    else if (*s == '\n')
		sv_catpvs(sstr, "\\n");
	    else if (*s == '\r')
		sv_catpvs(sstr, "\\r");
	    else if (*s == '\t')
		sv_catpvs(sstr, "\\t");
	    else if (*s == '\a')
		sv_catpvs(sstr, "\\a");
	    else if (*s == '\b')
		sv_catpvs(sstr, "\\b");
	    else if (*s == '\f')
		sv_catpvs(sstr, "\\f");
	    else if (!perlstyle && *s == '\v')
		sv_catpvs(sstr, "\\v");
	    else
	    {
		/* Don't want promotion of a signed -1 char in sprintf args */
		const unsigned char c = (unsigned char) *s;
		Perl_sv_catpvf(aTHX_ sstr, "\\%03o", c);
	    }
	    /* XXX Add line breaks if string is long */
	}
    }
    sv_catpvs(sstr, "\"");
    return sstr;
}

static SV *
cchar(pTHX_ SV *sv)
{
    SV *sstr = newSVpvs_flags("'", SVs_TEMP);
    const char *s = SvPV_nolen(sv);
    /* Don't want promotion of a signed -1 char in sprintf args */
    const unsigned char c = (unsigned char) *s;

    if (c == '\'')
	sv_catpvs(sstr, "\\'");
    else if (c == '\\')
	sv_catpvs(sstr, "\\\\");
#ifdef EBCDIC
    else if (isPRINT(c))
#else
    else if (c >= ' ' && c < 127)
#endif /* EBCDIC */
	sv_catpvn(sstr, s, 1);
    else if (c == '\n')
	sv_catpvs(sstr, "\\n");
    else if (c == '\r')
	sv_catpvs(sstr, "\\r");
    else if (c == '\t')
	sv_catpvs(sstr, "\\t");
    else if (c == '\a')
	sv_catpvs(sstr, "\\a");
    else if (c == '\b')
	sv_catpvs(sstr, "\\b");
    else if (c == '\f')
	sv_catpvs(sstr, "\\f");
    else if (c == '\v')
	sv_catpvs(sstr, "\\v");
    else
	Perl_sv_catpvf(aTHX_ sstr, "\\%03o", c);
    sv_catpvs(sstr, "'");
    return sstr;
}

#if PERL_VERSION >= 9
#  define PMOP_pmreplstart(o)	o->op_pmstashstartu.op_pmreplstart
#  define PMOP_pmreplroot(o)	o->op_pmreplrootu.op_pmreplroot
#else
#  define PMOP_pmreplstart(o)	o->op_pmreplstart
#  define PMOP_pmreplroot(o)	o->op_pmreplroot
#  define PMOP_pmpermflags(o)	o->op_pmpermflags
#  define PMOP_pmdynflags(o)      o->op_pmdynflags
#endif

static SV *
walkoptree(pTHX_ OP *o, const char *method, SV *ref)
{
    dSP;
    OP *kid;
    SV *object;
    const char *const classname = opclassnames[cc_opclass(aTHX_ o)];
    dMY_CXT;

    /* Check that no-one has changed our reference, or is holding a reference
       to it.  */
    if (SvREFCNT(ref) == 1 && SvROK(ref) && SvTYPE(ref) == SVt_RV
	&& (object = SvRV(ref)) && SvREFCNT(object) == 1
	&& SvTYPE(object) == SVt_PVMG && SvIOK_only(object)
	&& !SvMAGICAL(object) && !SvMAGIC(object) && SvSTASH(object)) {
	/* Looks good, so rebless it for the class we need:  */
	sv_bless(ref, gv_stashpv(classname, GV_ADD));
    } else {
	/* Need to make a new one. */
	ref = sv_newmortal();
	object = newSVrv(ref, classname);
    }
    sv_setiv(object, PTR2IV(o));

    if (walkoptree_debug) {
	PUSHMARK(sp);
	XPUSHs(ref);
	PUTBACK;
	perl_call_method("walkoptree_debug", G_DISCARD);
    }
    PUSHMARK(sp);
    XPUSHs(ref);
    PUTBACK;
    perl_call_method(method, G_DISCARD);
    if (o && (o->op_flags & OPf_KIDS)) {
	for (kid = ((UNOP*)o)->op_first; kid; kid = kid->op_sibling) {
	    ref = walkoptree(aTHX_ kid, method, ref);
	}
    }
    if (o && (cc_opclass(aTHX_ o) == OPc_PMOP) && o->op_type != OP_PUSHRE
           && (kid = PMOP_pmreplroot(cPMOPo)))
    {
	ref = walkoptree(aTHX_ kid, method, ref);
    }
    return ref;
}

static SV **
oplist(pTHX_ OP *o, SV **SP)
{
    for(; o; o = o->op_next) {
#if PERL_VERSION >= 9
	if (o->op_opt == 0)
	    break;
	o->op_opt = 0;
#else
	if (o->op_seq == 0)
	    break;
	o->op_seq = 0;
#endif
	XPUSHs(make_op_object(aTHX_ o));
        switch (o->op_type) {
	case OP_SUBST:
            SP = oplist(aTHX_ PMOP_pmreplstart(cPMOPo), SP);
            continue;
	case OP_SORT:
	    if (o->op_flags & OPf_STACKED && o->op_flags & OPf_SPECIAL) {
		OP *kid = cLISTOPo->op_first->op_sibling;   /* pass pushmark */
		kid = kUNOP->op_first;                      /* pass rv2gv */
		kid = kUNOP->op_first;                      /* pass leave */
		SP = oplist(aTHX_ kid->op_next, SP);
	    }
	    continue;
        }
	switch (PL_opargs[o->op_type] & OA_CLASS_MASK) {
	case OA_LOGOP:
	    SP = oplist(aTHX_ cLOGOPo->op_other, SP);
	    break;
	case OA_LOOP:
	    SP = oplist(aTHX_ cLOOPo->op_lastop, SP);
	    SP = oplist(aTHX_ cLOOPo->op_nextop, SP);
	    SP = oplist(aTHX_ cLOOPo->op_redoop, SP);
	    break;
	}
    }
    return SP;
}

typedef OP	*B__OP;
typedef UNOP	*B__UNOP;
typedef BINOP	*B__BINOP;
typedef LOGOP	*B__LOGOP;
typedef LISTOP	*B__LISTOP;
typedef PMOP	*B__PMOP;
typedef SVOP	*B__SVOP;
typedef PADOP	*B__PADOP;
typedef PVOP	*B__PVOP;
typedef LOOP	*B__LOOP;
typedef COP	*B__COP;

typedef SV	*B__SV;
typedef SV	*B__IV;
typedef SV	*B__PV;
typedef SV	*B__NV;
typedef SV	*B__PVMG;
#if PERL_VERSION >= 11
typedef SV	*B__REGEXP;
#endif
typedef SV	*B__PVLV;
typedef SV	*B__BM;
typedef SV	*B__RV;
typedef SV	*B__FM;
typedef AV	*B__AV;
typedef HV	*B__HV;
typedef CV	*B__CV;
typedef GV	*B__GV;
typedef IO	*B__IO;

typedef MAGIC	*B__MAGIC;
typedef HE      *B__HE;
#if PERL_VERSION >= 9
typedef struct refcounted_he	*B__RHE;
#endif

#ifdef MULTIPLICITY
#  define ASSIGN_COMMON_ALIAS(prefix, var) \
    STMT_START { XSANY.any_i32 = offsetof(struct interpreter, prefix##var); } STMT_END
#else
#  define ASSIGN_COMMON_ALIAS(prefix, var) \
    STMT_START { XSANY.any_ptr = (void *)&PL_##var; } STMT_END
#endif

/* This needs to be ALIASed in a custom way, hence can't easily be defined as
   a regular XSUB.  */
static XSPROTO(intrpvar_sv_common); /* prototype to pass -Wmissing-prototypes */
static XSPROTO(intrpvar_sv_common)
{
    dVAR;
    dXSARGS;
    SV *ret;
    if (items != 0)
       croak_xs_usage(cv,  "");
#ifdef MULTIPLICITY
    ret = *(SV **)(XSANY.any_i32 + (char *)my_perl);
#else
    ret = *(SV **)(XSANY.any_ptr);
#endif
    ST(0) = make_sv_object(aTHX_ ret);
    XSRETURN(1);
}

#include "const-c.inc"

MODULE = B	PACKAGE = B

INCLUDE: const-xs.inc

PROTOTYPES: DISABLE

BOOT:
{
    CV *cv;
    const char *file = __FILE__;
    MY_CXT_INIT;
    specialsv_list[0] = Nullsv;
    specialsv_list[1] = &PL_sv_undef;
    specialsv_list[2] = &PL_sv_yes;
    specialsv_list[3] = &PL_sv_no;
    specialsv_list[4] = (SV *) pWARN_ALL;
    specialsv_list[5] = (SV *) pWARN_NONE;
    specialsv_list[6] = (SV *) pWARN_STD;
    
    cv = newXS("B::init_av", intrpvar_sv_common, file);
    ASSIGN_COMMON_ALIAS(I, initav);
    cv = newXS("B::check_av", intrpvar_sv_common, file);
    ASSIGN_COMMON_ALIAS(I, checkav_save);
#if PERL_VERSION >= 9
    cv = newXS("B::unitcheck_av", intrpvar_sv_common, file);
    ASSIGN_COMMON_ALIAS(I, unitcheckav_save);
#endif
    cv = newXS("B::begin_av", intrpvar_sv_common, file);
    ASSIGN_COMMON_ALIAS(I, beginav_save);
    cv = newXS("B::end_av", intrpvar_sv_common, file);
    ASSIGN_COMMON_ALIAS(I, endav);
    cv = newXS("B::main_cv", intrpvar_sv_common, file);
    ASSIGN_COMMON_ALIAS(I, main_cv);
    cv = newXS("B::inc_gv", intrpvar_sv_common, file);
    ASSIGN_COMMON_ALIAS(I, incgv);
    cv = newXS("B::defstash", intrpvar_sv_common, file);
    ASSIGN_COMMON_ALIAS(I, defstash);
    cv = newXS("B::curstash", intrpvar_sv_common, file);
    ASSIGN_COMMON_ALIAS(I, curstash);
    cv = newXS("B::formfeed", intrpvar_sv_common, file);
    ASSIGN_COMMON_ALIAS(I, formfeed);
#ifdef USE_ITHREADS
    cv = newXS("B::regex_padav", intrpvar_sv_common, file);
    ASSIGN_COMMON_ALIAS(I, regex_padav);
#endif
    cv = newXS("B::warnhook", intrpvar_sv_common, file);
    ASSIGN_COMMON_ALIAS(I, warnhook);
    cv = newXS("B::diehook", intrpvar_sv_common, file);
    ASSIGN_COMMON_ALIAS(I, diehook);
}

long 
amagic_generation()
    CODE:
	RETVAL = PL_amagic_generation;
    OUTPUT:
	RETVAL

void
comppadlist()
    PPCODE:
	PUSHs(make_sv_object(aTHX_ (SV *)(PL_main_cv ? CvPADLIST(PL_main_cv)
						     : CvPADLIST(PL_compcv))));

void
sv_undef()
    ALIAS:
	sv_no = 1
	sv_yes = 2
    PPCODE:
	PUSHs(make_sv_object(aTHX_ ix > 1 ? &PL_sv_yes
					  : ix < 1 ? &PL_sv_undef
						   : &PL_sv_no));

void
main_root()
    ALIAS:
	main_start = 1
    PPCODE:
	PUSHs(make_op_object(aTHX_ ix ? PL_main_start : PL_main_root));

UV
sub_generation()
    ALIAS:
	dowarn = 1
    CODE:
	RETVAL = ix ? PL_dowarn : PL_sub_generation;
    OUTPUT:
	RETVAL

void
walkoptree(op, method)
	B::OP op
	const char *	method
    CODE:
	(void) walkoptree(aTHX_ op, method, &PL_sv_undef);

int
walkoptree_debug(...)
    CODE:
	dMY_CXT;
	RETVAL = walkoptree_debug;
	if (items > 0 && SvTRUE(ST(1)))
	    walkoptree_debug = 1;
    OUTPUT:
	RETVAL

#define address(sv) PTR2IV(sv)

IV
address(sv)
	SV *	sv

void
svref_2object(sv)
	SV *	sv
    PPCODE:
	if (!SvROK(sv))
	    croak("argument is not a reference");
	PUSHs(make_sv_object(aTHX_ SvRV(sv)));

void
opnumber(name)
const char *	name
CODE:
{
 int i; 
 IV  result = -1;
 ST(0) = sv_newmortal();
 if (strncmp(name,"pp_",3) == 0)
   name += 3;
 for (i = 0; i < PL_maxo; i++)
  {
   if (strcmp(name, PL_op_name[i]) == 0)
    {
     result = i;
     break;
    }
  }
 sv_setiv(ST(0),result);
}

void
ppname(opnum)
	int	opnum
    CODE:
	ST(0) = sv_newmortal();
	if (opnum >= 0 && opnum < PL_maxo)
	    Perl_sv_setpvf(aTHX_ ST(0), "pp_%s", PL_op_name[opnum]);

void
hash(sv)
	SV *	sv
    CODE:
	STRLEN len;
	U32 hash = 0;
	const char *s = SvPVbyte(sv, len);
	PERL_HASH(hash, s, len);
	ST(0) = sv_2mortal(Perl_newSVpvf(aTHX_ "0x%"UVxf, (UV)hash));

#define cast_I32(foo) (I32)foo
IV
cast_I32(i)
	IV	i

void
minus_c()
    ALIAS:
	save_BEGINs = 1
    CODE:
	if (ix)
	    PL_savebegin = TRUE;
	else
	    PL_minus_c = TRUE;

void
cstring(sv)
	SV *	sv
    ALIAS:
	perlstring = 1
	cchar = 2
    PPCODE:
	PUSHs(ix == 2 ? cchar(aTHX_ sv) : cstring(aTHX_ sv, (bool)ix));

void
threadsv_names()
    PPCODE:
#if PERL_VERSION <= 8
# ifdef USE_5005THREADS
	int i;
	const STRLEN len = strlen(PL_threadsv_names);

	EXTEND(sp, len);
	for (i = 0; i < len; i++)
	    PUSHs(newSVpvn_flags(&PL_threadsv_names[i], 1, SVs_TEMP));
# endif
#endif

#define SVp		0x00000
#define U32p		0x10000
#define line_tp		0x20000
#define OPp		0x30000
#define PADOFFSETp	0x40000
#define U8p		0x50000
#define IVp		0x60000
#define char_pp		0x70000

#define OP_next_ix		OPp | offsetof(struct op, op_next)
#define OP_sibling_ix		OPp | offsetof(struct op, op_sibling)
#define UNOP_first_ix		OPp | offsetof(struct unop, op_first)
#define BINOP_last_ix		OPp | offsetof(struct binop, op_last)
#define LOGOP_other_ix		OPp | offsetof(struct logop, op_other)
#if PERL_VERSION >= 9
#  define PMOP_pmreplstart_ix \
		OPp | offsetof(struct pmop, op_pmstashstartu.op_pmreplstart)
#else
#  define PMOP_pmreplstart_ix	OPp | offsetof(struct pmop, op_pmreplstart)
#endif
#define LOOP_redoop_ix		OPp | offsetof(struct loop, op_redoop)
#define LOOP_nextop_ix		OPp | offsetof(struct loop, op_nextop)
#define LOOP_lastop_ix		OPp | offsetof(struct loop, op_lastop)

#define OP_targ_ix		PADOFFSETp | offsetof(struct op, op_targ)
#define OP_flags_ix		U8p | offsetof(struct op, op_flags)
#define OP_private_ix		U8p | offsetof(struct op, op_private)

#define PMOP_pmflags_ix		U32p | offsetof(struct pmop, op_pmflags)

#ifdef USE_ITHREADS
#define PMOP_pmoffset_ix	IVp | offsetof(struct pmop, op_pmoffset)
#endif

#  Yes, B::SV::sv and B::SV::gv really do end up generating identical code.
#define SVOP_sv_ix		SVp | offsetof(struct svop, op_sv)
#define SVOP_gv_ix		SVp | offsetof(struct svop, op_sv)

#define PADOP_padix_ix		PADOFFSETp | offsetof(struct padop, op_padix)

#define COP_seq_ix		U32p | offsetof(struct cop, cop_seq)
#define COP_line_ix		line_tp | offsetof(struct cop, cop_line)
#if PERL_VERSION >= 9
#define COP_hints_ix		U32p | offsetof(struct cop, cop_hints)
#else
#define COP_hints_ix		U8p | offsetof(struct cop, op_private)
#endif

#ifdef USE_ITHREADS
#define COP_stashpv_ix		char_pp | offsetof(struct cop, cop_stashpv)
#define COP_file_ix		char_pp | offsetof(struct cop, cop_file)
#else
#define COP_stash_ix		SVp | offsetof(struct cop, cop_stash)
#define COP_filegv_ix		SVp | offsetof(struct cop, cop_filegv)
#endif

MODULE = B	PACKAGE = B::OP

size_t
size(o)
	B::OP		o
    CODE:
	RETVAL = opsizes[cc_opclass(aTHX_ o)];
    OUTPUT:
	RETVAL

# The type checking code in B has always been identical for all OP types,
# irrespective of whether the action is actually defined on that OP.
# We should fix this
void
next(o)
	B::OP		o
    ALIAS:
	B::OP::next = OP_next_ix
	B::OP::sibling = OP_sibling_ix
	B::OP::targ = OP_targ_ix
	B::OP::flags = OP_flags_ix
	B::OP::private = OP_private_ix
	B::UNOP::first = UNOP_first_ix
	B::BINOP::last = BINOP_last_ix
	B::LOGOP::other = LOGOP_other_ix
	B::PMOP::pmreplstart = PMOP_pmreplstart_ix
	B::LOOP::redoop = LOOP_redoop_ix
	B::LOOP::nextop = LOOP_nextop_ix
	B::LOOP::lastop = LOOP_lastop_ix
	B::PMOP::pmflags = PMOP_pmflags_ix
	B::SVOP::sv = SVOP_sv_ix
	B::SVOP::gv = SVOP_gv_ix
	B::PADOP::padix = PADOP_padix_ix
	B::COP::cop_seq = COP_seq_ix
	B::COP::line = COP_line_ix
	B::COP::hints = COP_hints_ix
    PREINIT:
	char *ptr;
	SV *ret;
    PPCODE:
	ptr = (ix & 0xFFFF) + (char *)o;
	switch ((U8)(ix >> 16)) {
	case (U8)(OPp >> 16):
	    ret = make_op_object(aTHX_ *((OP **)ptr));
	    break;
	case (U8)(PADOFFSETp >> 16):
	    ret = sv_2mortal(newSVuv(*((PADOFFSET*)ptr)));
	    break;
	case (U8)(U8p >> 16):
	    ret = sv_2mortal(newSVuv(*((U8*)ptr)));
	    break;
	case (U8)(U32p >> 16):
	    ret = sv_2mortal(newSVuv(*((U32*)ptr)));
	    break;
	case (U8)(SVp >> 16):
	    ret = make_sv_object(aTHX_ *((SV **)ptr));
	    break;
	case (U8)(line_tp >> 16):
	    ret = sv_2mortal(newSVuv(*((line_t *)ptr)));
	    break;
#ifdef USE_ITHREADS
	case (U8)(IVp >> 16):
	    ret = sv_2mortal(newSViv(*((IV*)ptr)));
	    break;
	case (U8)(char_pp >> 16):
	    ret = sv_2mortal(newSVpv(*((char **)ptr), 0));
	    break;
#endif
	default:
	    croak("Illegal alias 0x%08x for B::*next", (unsigned)ix);

	}
	ST(0) = ret;
	XSRETURN(1);

char *
name(o)
	B::OP		o
    ALIAS:
	desc = 1
    CODE:
	RETVAL = (char *)(ix ? OP_DESC(o) : OP_NAME(o));
    OUTPUT:
	RETVAL

void
ppaddr(o)
	B::OP		o
    PREINIT:
	int i;
	SV *sv;
    CODE:
	sv = sv_2mortal(Perl_newSVpvf(aTHX_ "PL_ppaddr[OP_%s]",
				      PL_op_name[o->op_type]));
	for (i=13; (STRLEN)i < SvCUR(sv); ++i)
	    SvPVX(sv)[i] = toUPPER(SvPVX(sv)[i]);
	ST(0) = sv;

#if PERL_VERSION >= 9
#  These 3 are all bitfields, so we can't take their addresses.
UV
type(o)
	B::OP		o
    ALIAS:
	opt = 1
	spare = 2
    CODE:
	switch(ix) {
	  case 1:
	    RETVAL = o->op_opt;
	    break;
	  case 2:
	    RETVAL = o->op_spare;
	    break;
	  default:
	    RETVAL = o->op_type;
	}
    OUTPUT:
	RETVAL

#else

UV
type(o)
	B::OP		o
    ALIAS:
	seq = 1
    CODE:
	switch(ix) {
	  case 1:
	    RETVAL = o->op_seq;
	    break;
	  default:
	    RETVAL = o->op_type;
	}
    OUTPUT:
	RETVAL

#endif

void
oplist(o)
	B::OP		o
    PPCODE:
	SP = oplist(aTHX_ o, SP);

MODULE = B	PACKAGE = B::LISTOP

U32
children(o)
	B::LISTOP	o
	OP *		kid = NO_INIT
	int		i = NO_INIT
    CODE:
	i = 0;
	for (kid = o->op_first; kid; kid = kid->op_sibling)
	    i++;
	RETVAL = i;
    OUTPUT:
	RETVAL

MODULE = B	PACKAGE = B::PMOP		PREFIX = PMOP_

#if PERL_VERSION <= 8

void
PMOP_pmreplroot(o)
	B::PMOP		o
	OP *		root = NO_INIT
    CODE:
	root = o->op_pmreplroot;
	/* OP_PUSHRE stores an SV* instead of an OP* in op_pmreplroot */
	if (o->op_type == OP_PUSHRE) {
	    ST(0) = sv_newmortal();
#  ifdef USE_ITHREADS
            sv_setiv(ST(0), INT2PTR(PADOFFSET,root) );
#  else
	    sv_setiv(newSVrv(ST(0), root ?
			     svclassnames[SvTYPE((SV*)root)] : "B::SV"),
		     PTR2IV(root));
#  endif
	}
	else {
	    ST(0) = make_op_object(aTHX_ root);
	}

#else

void
PMOP_pmreplroot(o)
	B::PMOP		o
    CODE:
	if (o->op_type == OP_PUSHRE) {
#  ifdef USE_ITHREADS
	    ST(0) = sv_newmortal();
            sv_setiv(ST(0), o->op_pmreplrootu.op_pmtargetoff);
#  else
	    GV *const target = o->op_pmreplrootu.op_pmtargetgv;
	    ST(0) = sv_newmortal();
	    sv_setiv(newSVrv(ST(0), target ?
			     svclassnames[SvTYPE((SV*)target)] : "B::SV"),
		     PTR2IV(target));
#  endif
	}
	else {
	    OP *const root = o->op_pmreplrootu.op_pmreplroot; 
	    ST(0) = make_op_object(aTHX_ root);
	}

#endif

#ifdef USE_ITHREADS
#define PMOP_pmstashpv(o)	PmopSTASHPV(o);

char*
PMOP_pmstashpv(o)
	B::PMOP		o

#else

void
PMOP_pmstash(o)
	B::PMOP		o
    PPCODE:
	PUSHs(make_sv_object(aTHX_ (SV *) PmopSTASH(o)));

#endif

#if PERL_VERSION < 9

void
PMOP_pmnext(o)
	B::PMOP		o
    PPCODE:
	PUSHs(make_op_object(aTHX_ o->op_pmnext));

U32
PMOP_pmpermflags(o)
	B::PMOP		o

U8
PMOP_pmdynflags(o)
        B::PMOP         o

#endif

void
PMOP_precomp(o)
	B::PMOP		o
    PREINIT:
	dXSI32;
	REGEXP *rx;
    CODE:
	rx = PM_GETRE(o);
	ST(0) = sv_newmortal();
	if (rx) {
#if PERL_VERSION >= 9
	    if (ix) {
		sv_setuv(ST(0), RX_EXTFLAGS(rx));
	    } else
#endif
	    {
		sv_setpvn(ST(0), RX_PRECOMP(rx), RX_PRELEN(rx));
	    }
	}

BOOT:
{
	CV *cv;
#ifdef USE_ITHREADS
        cv = newXS("B::PMOP::pmoffset", XS_B__OP_next, __FILE__);
        XSANY.any_i32 = PMOP_pmoffset_ix;
        cv = newXS("B::COP::stashpv", XS_B__OP_next, __FILE__);
        XSANY.any_i32 = COP_stashpv_ix;
        cv = newXS("B::COP::file", XS_B__OP_next, __FILE__);
        XSANY.any_i32 = COP_file_ix;
#else
        cv = newXS("B::COP::stash", XS_B__OP_next, __FILE__);
        XSANY.any_i32 = COP_stash_ix;
        cv = newXS("B::COP::filegv", XS_B__OP_next, __FILE__);
        XSANY.any_i32 = COP_filegv_ix;
#endif
#if PERL_VERSION >= 9
        cv = newXS("B::PMOP::reflags", XS_B__PMOP_precomp, __FILE__);
        XSANY.any_i32 = 1;
#endif
}

MODULE = B	PACKAGE = B::PADOP

void
sv(o)
	B::PADOP o
    PREINIT:
	SV *ret;
    ALIAS:
	gv = 1
    PPCODE:
	/* It happens that the output typemaps for B::SV and B::GV are
	   identical. The "smarts" are in make_sv_object(), which determines
	   which class to use based on SvTYPE(), rather than anything baked in
	   at compile time.  */	   
	if (o->op_padix) {
	    ret = PAD_SVl(o->op_padix);
	    if (ix && SvTYPE(ret) != SVt_PVGV)
		ret = NULL;
	} else {
	    ret = NULL;
	}
	PUSHs(make_sv_object(aTHX_ ret));

MODULE = B	PACKAGE = B::PVOP

void
pv(o)
	B::PVOP	o
    CODE:
	/*
	 * OP_TRANS uses op_pv to point to a table of 256 or >=258 shorts
	 * whereas other PVOPs point to a null terminated string.
	 */
	if ((o->op_type == OP_TRANS || o->op_type == OP_TRANSR) &&
		(o->op_private & OPpTRANS_COMPLEMENT) &&
		!(o->op_private & OPpTRANS_DELETE))
	{
	    const short* const tbl = (short*)o->op_pv;
	    const short entries = 257 + tbl[256];
	    ST(0) = newSVpvn_flags(o->op_pv, entries * sizeof(short), SVs_TEMP);
	}
	else if (o->op_type == OP_TRANS || o->op_type == OP_TRANSR) {
	    ST(0) = newSVpvn_flags(o->op_pv, 256 * sizeof(short), SVs_TEMP);
	}
	else
	    ST(0) = newSVpvn_flags(o->op_pv, strlen(o->op_pv), SVs_TEMP);

#define COP_label(o)	CopLABEL(o)
#ifdef CopSTASH_len
#define COP_stashlen(o)	CopSTASH_len(o)
#endif

MODULE = B	PACKAGE = B::COP		PREFIX = COP_

const char *
COP_label(o)
	B::COP	o

# Both pairs of accessors are provided for both ithreads and not, but for each,
# one pair is direct structure access, and 1 pair "faked up" with a more complex
# macro. We implement the direct structure access pair using the common code
# above (B::OP::next)
 
#ifdef USE_ITHREADS

void
COP_stash(o)
	B::COP	o
    ALIAS:
	filegv = 1
    PPCODE:
	PUSHs(make_sv_object(aTHX_
			     ix ? (SV *)CopFILEGV(o) : (SV *)CopSTASH(o)));

#ifdef CopSTASH_len

U32
COP_stashlen(o)
	B::COP	o

#endif

#else

char *
COP_stashpv(o)
	B::COP	o
    ALIAS:
	file = 1
    CODE:
	RETVAL = ix ? CopFILE(o) : CopSTASHPV(o);
    OUTPUT:
	RETVAL

#endif

I32
COP_arybase(o)
	B::COP	o
    CODE:
	RETVAL = 0;
    OUTPUT:
	RETVAL

void
COP_warnings(o)
	B::COP	o
    ALIAS:
	io = 1
    PPCODE:
#if PERL_VERSION >= 9
	ST(0) = ix ? make_cop_io_object(aTHX_ o) : make_warnings_object(aTHX_ o);
#else
	ST(0) = make_sv_object(aTHX_ ix ? o->cop_io : o->cop_warnings);
#endif
	XSRETURN(1);

#if PERL_VERSION >= 9

B::RHE
COP_hints_hash(o)
	B::COP o
    CODE:
	RETVAL = CopHINTHASH_get(o);
    OUTPUT:
	RETVAL

#endif

MODULE = B	PACKAGE = B::SV

#define MAGICAL_FLAG_BITS (SVs_GMG|SVs_SMG|SVs_RMG)

U32
REFCNT(sv)
	B::SV	sv
    ALIAS:
	FLAGS = 0xFFFFFFFF
	SvTYPE = SVTYPEMASK
	POK = SVf_POK
	ROK = SVf_ROK
	MAGICAL = MAGICAL_FLAG_BITS
    CODE:
	RETVAL = ix ? (SvFLAGS(sv) & (U32)ix) : SvREFCNT(sv);
    OUTPUT:
	RETVAL

void
object_2svref(sv)
	B::SV	sv
    PPCODE:
	ST(0) = sv_2mortal(newRV(sv));
	XSRETURN(1);
	
MODULE = B	PACKAGE = B::IV		PREFIX = Sv

IV
SvIV(sv)
	B::IV	sv

MODULE = B	PACKAGE = B::IV

#define sv_SVp		0x00000
#define sv_IVp		0x10000
#define sv_UVp		0x20000
#define sv_STRLENp	0x30000
#define sv_U32p		0x40000
#define sv_U8p		0x50000
#define sv_char_pp	0x60000
#define sv_NVp		0x70000
#define sv_char_p	0x80000
#define sv_SSize_tp	0x90000
#define sv_I32p		0xA0000
#define sv_U16p		0xB0000

#define IV_ivx_ix	sv_IVp | offsetof(struct xpviv, xiv_iv)
#define IV_uvx_ix	sv_UVp | offsetof(struct xpvuv, xuv_uv)
#define NV_nvx_ix	sv_NVp | offsetof(struct xpvnv, xnv_u.xnv_nv)

#if PERL_VERSION >= 10
#define NV_cop_seq_range_low_ix \
			sv_U32p | offsetof(struct xpvnv, xnv_u.xpad_cop_seq.xlow)
#define NV_cop_seq_range_high_ix \
			sv_U32p | offsetof(struct xpvnv, xnv_u.xpad_cop_seq.xhigh)
#define NV_parent_pad_index_ix \
			sv_U32p | offsetof(struct xpvnv, xnv_u.xpad_cop_seq.xlow)
#define NV_parent_fakelex_flags_ix \
			sv_U32p | offsetof(struct xpvnv, xnv_u.xpad_cop_seq.xhigh)
#else
#define NV_cop_seq_range_low_ix \
			sv_NVp | offsetof(struct xpvnv, xnv_nv)
#define NV_cop_seq_range_high_ix \
			sv_UVp | offsetof(struct xpvnv, xuv_uv)
#define NV_parent_pad_index_ix \
			sv_NVp | offsetof(struct xpvnv, xnv_nv)
#define NV_parent_fakelex_flags_ix \
			sv_UVp | offsetof(struct xpvnv, xuv_uv)
#endif

#define PV_cur_ix	sv_STRLENp | offsetof(struct xpv, xpv_cur)
#define PV_len_ix	sv_STRLENp | offsetof(struct xpv, xpv_len)

#define PVMG_stash_ix	sv_SVp | offsetof(struct xpvmg, xmg_stash)

#if PERL_VERSION >= 10
#  if PERL_VERSION > 14
#    define PVBM_useful_ix	sv_I32p | offsetof(struct xpvgv, xnv_u.xbm_s.xbm_useful)
#    define PVBM_previous_ix	sv_UVp | offsetof(struct xpvuv, xuv_uv)
#  else
#define PVBM_useful_ix	sv_I32p | offsetof(struct xpvgv, xiv_u.xivu_i32)
#define PVBM_previous_ix    sv_U32p | offsetof(struct xpvgv, xnv_u.xbm_s.xbm_previous)
#  endif
#define PVBM_rare_ix	sv_U8p | offsetof(struct xpvgv, xnv_u.xbm_s.xbm_rare)
#else
#define PVBM_useful_ix	sv_I32p | offsetof(struct xpvbm, xbm_useful)
#define PVBM_previous_ix    sv_U16p | offsetof(struct xpvbm, xbm_previous)
#define PVBM_rare_ix	sv_U8p | offsetof(struct xpvbm, xbm_rare)
#endif

#define PVLV_targoff_ix	sv_U32p | offsetof(struct xpvlv, xlv_targoff)
#define PVLV_targlen_ix	sv_U32p | offsetof(struct xpvlv, xlv_targlen)
#define PVLV_targ_ix	sv_SVp | offsetof(struct xpvlv, xlv_targ)
#define PVLV_type_ix	sv_char_p | offsetof(struct xpvlv, xlv_type)

#if PERL_VERSION >= 10
#define PVGV_stash_ix	sv_SVp | offsetof(struct xpvgv, xnv_u.xgv_stash)
#define PVGV_flags_ix	sv_STRLENp | offsetof(struct xpvgv, xpv_cur)
#define PVIO_lines_ix	sv_IVp | offsetof(struct xpvio, xiv_iv)
#else
#define PVGV_stash_ix	sv_SVp | offsetof(struct xpvgv, xgv_stash)
#define PVGV_flags_ix	sv_U8p | offsetof(struct xpvgv, xgv_flags)
#define PVIO_lines_ix	sv_IVp | offsetof(struct xpvio, xio_lines)
#endif

#define PVIO_page_ix	    sv_IVp | offsetof(struct xpvio, xio_page)
#define PVIO_page_len_ix    sv_IVp | offsetof(struct xpvio, xio_page_len)
#define PVIO_lines_left_ix  sv_IVp | offsetof(struct xpvio, xio_lines_left)
#define PVIO_top_name_ix    sv_char_pp | offsetof(struct xpvio, xio_top_name)
#define PVIO_top_gv_ix	    sv_SVp | offsetof(struct xpvio, xio_top_gv)
#define PVIO_fmt_name_ix    sv_char_pp | offsetof(struct xpvio, xio_fmt_name)
#define PVIO_fmt_gv_ix	    sv_SVp | offsetof(struct xpvio, xio_fmt_gv)
#define PVIO_bottom_name_ix sv_char_pp | offsetof(struct xpvio, xio_bottom_name)
#define PVIO_bottom_gv_ix   sv_SVp | offsetof(struct xpvio, xio_bottom_gv)
#define PVIO_type_ix	    sv_char_p | offsetof(struct xpvio, xio_type)
#define PVIO_flags_ix	    sv_U8p | offsetof(struct xpvio, xio_flags)

#define PVAV_max_ix	sv_SSize_tp | offsetof(struct xpvav, xav_max)

#define PVFM_lines_ix	sv_IVp | offsetof(struct xpvfm, xfm_lines)

#define PVCV_stash_ix	sv_SVp | offsetof(struct xpvcv, xcv_stash) 
#define PVCV_gv_ix	sv_SVp | offsetof(struct xpvcv, xcv_gv)
#define PVCV_file_ix	sv_char_pp | offsetof(struct xpvcv, xcv_file)
#define PVCV_depth_ix	sv_I32p | offsetof(struct xpvcv, xcv_depth)
#define PVCV_padlist_ix	sv_SVp | offsetof(struct xpvcv, xcv_padlist)
#define PVCV_outside_ix	sv_SVp | offsetof(struct xpvcv, xcv_outside)
#define PVCV_outside_seq_ix sv_U32p | offsetof(struct xpvcv, xcv_outside_seq)
#define PVCV_flags_ix	sv_U16p | offsetof(struct xpvcv, xcv_flags)

#define PVHV_max_ix	sv_STRLENp | offsetof(struct xpvhv, xhv_max)

#if PERL_VERSION > 12
#define PVHV_keys_ix	sv_STRLENp | offsetof(struct xpvhv, xhv_keys)
#else
#define PVHV_keys_ix	sv_IVp | offsetof(struct xpvhv, xhv_keys)
#endif

# The type checking code in B has always been identical for all SV types,
# irrespective of whether the action is actually defined on that SV.
# We should fix this
void
IVX(sv)
	B::SV		sv
    ALIAS:
	B::IV::IVX = IV_ivx_ix
	B::IV::UVX = IV_uvx_ix
	B::NV::NVX = NV_nvx_ix
	B::NV::COP_SEQ_RANGE_LOW = NV_cop_seq_range_low_ix
	B::NV::COP_SEQ_RANGE_HIGH = NV_cop_seq_range_high_ix
	B::NV::PARENT_PAD_INDEX = NV_parent_pad_index_ix
	B::NV::PARENT_FAKELEX_FLAGS = NV_parent_fakelex_flags_ix
	B::PV::CUR = PV_cur_ix
	B::PV::LEN = PV_len_ix
	B::PVMG::SvSTASH = PVMG_stash_ix
	B::PVLV::TARGOFF = PVLV_targoff_ix
	B::PVLV::TARGLEN = PVLV_targlen_ix
	B::PVLV::TARG = PVLV_targ_ix
	B::PVLV::TYPE = PVLV_type_ix
	B::GV::STASH = PVGV_stash_ix
	B::GV::GvFLAGS = PVGV_flags_ix
	B::BM::USEFUL = PVBM_useful_ix
	B::BM::PREVIOUS = PVBM_previous_ix
	B::BM::RARE = PVBM_rare_ix
	B::IO::LINES =  PVIO_lines_ix
	B::IO::PAGE = PVIO_page_ix
	B::IO::PAGE_LEN = PVIO_page_len_ix
	B::IO::LINES_LEFT = PVIO_lines_left_ix
	B::IO::TOP_NAME = PVIO_top_name_ix
	B::IO::TOP_GV = PVIO_top_gv_ix
	B::IO::FMT_NAME = PVIO_fmt_name_ix
	B::IO::FMT_GV = PVIO_fmt_gv_ix
	B::IO::BOTTOM_NAME = PVIO_bottom_name_ix
	B::IO::BOTTOM_GV = PVIO_bottom_gv_ix
	B::IO::IoTYPE = PVIO_type_ix
	B::IO::IoFLAGS = PVIO_flags_ix
	B::AV::MAX = PVAV_max_ix
	B::FM::LINES = PVFM_lines_ix
	B::CV::STASH = PVCV_stash_ix
	B::CV::GV = PVCV_gv_ix
	B::CV::FILE = PVCV_file_ix
	B::CV::DEPTH = PVCV_depth_ix
	B::CV::PADLIST = PVCV_padlist_ix
	B::CV::OUTSIDE = PVCV_outside_ix
	B::CV::OUTSIDE_SEQ = PVCV_outside_seq_ix
	B::CV::CvFLAGS = PVCV_flags_ix
	B::HV::MAX = PVHV_max_ix
	B::HV::KEYS = PVHV_keys_ix
    PREINIT:
	char *ptr;
	SV *ret;
    PPCODE:
	ptr = (ix & 0xFFFF) + (char *)SvANY(sv);
	switch ((U8)(ix >> 16)) {
	case (U8)(sv_SVp >> 16):
	    ret = make_sv_object(aTHX_ *((SV **)ptr));
	    break;
	case (U8)(sv_IVp >> 16):
	    ret = sv_2mortal(newSViv(*((IV *)ptr)));
	    break;
	case (U8)(sv_UVp >> 16):
	    ret = sv_2mortal(newSVuv(*((UV *)ptr)));
	    break;
	case (U8)(sv_STRLENp >> 16):
	    ret = sv_2mortal(newSVuv(*((STRLEN *)ptr)));
	    break;
	case (U8)(sv_U32p >> 16):
	    ret = sv_2mortal(newSVuv(*((U32 *)ptr)));
	    break;
	case (U8)(sv_U8p >> 16):
	    ret = sv_2mortal(newSVuv(*((U8 *)ptr)));
	    break;
	case (U8)(sv_char_pp >> 16):
	    ret = sv_2mortal(newSVpv(*((char **)ptr), 0));
	    break;
	case (U8)(sv_NVp >> 16):
	    ret = sv_2mortal(newSVnv(*((NV *)ptr)));
	    break;
	case (U8)(sv_char_p >> 16):
	    ret = newSVpvn_flags((char *)ptr, 1, SVs_TEMP);
	    break;
	case (U8)(sv_SSize_tp >> 16):
	    ret = sv_2mortal(newSViv(*((SSize_t *)ptr)));
	    break;
	case (U8)(sv_I32p >> 16):
	    ret = sv_2mortal(newSVuv(*((I32 *)ptr)));
	    break;
	case (U8)(sv_U16p >> 16):
	    ret = sv_2mortal(newSVuv(*((U16 *)ptr)));
	    break;
	default:
	    croak("Illegal alias 0x%08x for B::*IVX", (unsigned)ix);
	}
	ST(0) = ret;
	XSRETURN(1);

void
packiv(sv)
	B::IV	sv
    ALIAS:
	needs64bits = 1
    CODE:
	if (ix) {
	    ST(0) = boolSV((I32)SvIVX(sv) != SvIVX(sv));
	} else if (sizeof(IV) == 8) {
	    U32 wp[2];
	    const IV iv = SvIVX(sv);
	    /*
	     * The following way of spelling 32 is to stop compilers on
	     * 32-bit architectures from moaning about the shift count
	     * being >= the width of the type. Such architectures don't
	     * reach this code anyway (unless sizeof(IV) > 8 but then
	     * everything else breaks too so I'm not fussed at the moment).
	     */
#ifdef UV_IS_QUAD
	    wp[0] = htonl(((UV)iv) >> (sizeof(UV)*4));
#else
	    wp[0] = htonl(((U32)iv) >> (sizeof(UV)*4));
#endif
	    wp[1] = htonl(iv & 0xffffffff);
	    ST(0) = newSVpvn_flags((char *)wp, 8, SVs_TEMP);
	} else {
	    U32 w = htonl((U32)SvIVX(sv));
	    ST(0) = newSVpvn_flags((char *)&w, 4, SVs_TEMP);
	}

MODULE = B	PACKAGE = B::NV		PREFIX = Sv

NV
SvNV(sv)
	B::NV	sv

#if PERL_VERSION < 11

MODULE = B	PACKAGE = B::RV		PREFIX = Sv

void
SvRV(sv)
	B::RV	sv
    PPCODE:
	PUSHs(make_sv_object(aTHX_ SvRV(sv)));

#else

MODULE = B	PACKAGE = B::REGEXP

void
REGEX(sv)
	B::REGEXP	sv
    ALIAS:
	precomp = 1
    PPCODE:
	if (ix) {
	    PUSHs(newSVpvn_flags(RX_PRECOMP(sv), RX_PRELEN(sv), SVs_TEMP));
	} else {
	    dXSTARG;
	    /* FIXME - can we code this method more efficiently?  */
	    PUSHi(PTR2IV(sv));
	}

#endif

MODULE = B	PACKAGE = B::PV

void
RV(sv)
        B::PV   sv
    PPCODE:
        if (!SvROK(sv))
            croak( "argument is not SvROK" );
	PUSHs(make_sv_object(aTHX_ SvRV(sv)));

void
PV(sv)
	B::PV	sv
    ALIAS:
	PVX = 1
	PVBM = 2
	B::BM::TABLE = 3
    PREINIT:
	const char *p;
	STRLEN len = 0;
	U32 utf8 = 0;
    CODE:
	if (ix == 3) {
#ifndef PERL_FBM_TABLE_OFFSET
	    const MAGIC *const mg = mg_find(sv, PERL_MAGIC_bm);

	    if (!mg)
                croak("argument to B::BM::TABLE is not a PVBM");
	    p = mg->mg_ptr;
	    len = mg->mg_len;
#else
	    p = SvPV(sv, len);
	    /* Boyer-Moore table is just after string and its safety-margin \0 */
	    p += len + PERL_FBM_TABLE_OFFSET;
	    len = 256;
#endif
	} else if (ix == 2) {
	    /* This used to read 257. I think that that was buggy - should have
	       been 258. (The "\0", the flags byte, and 256 for the table.)
	       The only user of this method is B::Bytecode in B::PV::bsave.
	       I'm guessing that nothing tested the runtime correctness of
	       output of bytecompiled string constant arguments to index (etc).

	       Note the start pointer is and has always been SvPVX(sv), not
	       SvPVX(sv) + SvCUR(sv) PVBM was added in 651aa52ea1faa806, and
	       first used by the compiler in 651aa52ea1faa806. It's used to
	       get a "complete" dump of the buffer at SvPVX(), not just the
	       PVBM table. This permits the generated bytecode to "load"
	       SvPVX in "one" hit.

	       5.15 and later store the BM table via MAGIC, so the compiler
	       should handle this just fine without changes if PVBM now
	       always returns the SvPVX() buffer.  */
	    p = SvPVX_const(sv);
#ifdef PERL_FBM_TABLE_OFFSET
	    len = SvCUR(sv) + (SvVALID(sv) ? 256 + PERL_FBM_TABLE_OFFSET : 0);
#else
	    len = SvCUR(sv);
#endif
	} else if (ix) {
	    p = SvPVX(sv);
	    len = strlen(p);
	} else if (SvPOK(sv)) {
	    len = SvCUR(sv);
	    p = SvPVX_const(sv);
	    utf8 = SvUTF8(sv);
#if PERL_VERSION < 10
	    /* Before 5.10 (well 931b58fb28fa5ca7), PAD_COMPNAME_GEN was stored
	       in SvCUR(), which meant we had to attempt this special casing
	       to avoid tripping up over variable names in the pads.  */
	    if((SvLEN(sv) && len >= SvLEN(sv))) {
		/* It claims to be longer than the space allocated for it -
		   presumably it's a variable name in the pad  */
		len = strlen(p);
	    }
#endif
        }
        else {
            /* XXX for backward compatibility, but should fail */
            /* croak( "argument is not SvPOK" ); */
	    p = NULL;
        }
	ST(0) = newSVpvn_flags(p, len, SVs_TEMP | utf8);

MODULE = B	PACKAGE = B::PVMG

void
MAGIC(sv)
	B::PVMG	sv
	MAGIC *	mg = NO_INIT
    PPCODE:
	for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic)
	    XPUSHs(make_mg_object(aTHX_ mg));

MODULE = B	PACKAGE = B::MAGIC

void
MOREMAGIC(mg)
	B::MAGIC	mg
    ALIAS:
	PRIVATE = 1
	TYPE = 2
	FLAGS = 3
	LENGTH = 4
	OBJ = 5
	PTR = 6
	REGEX = 7
	precomp = 8
    PPCODE:
	switch (ix) {
	case 0:
	    XPUSHs(mg->mg_moremagic ? make_mg_object(aTHX_ mg->mg_moremagic)
				    : &PL_sv_undef);
	    break;
	case 1:
	    mPUSHu(mg->mg_private);
	    break;
	case 2:
	    PUSHs(newSVpvn_flags(&(mg->mg_type), 1, SVs_TEMP));
	    break;
	case 3:
	    mPUSHu(mg->mg_flags);
	    break;
	case 4:
	    mPUSHi(mg->mg_len);
	    break;
	case 5:
	    PUSHs(make_sv_object(aTHX_ mg->mg_obj));
	    break;
	case 6:
	    if (mg->mg_ptr) {
		if (mg->mg_len >= 0) {
		    PUSHs(newSVpvn_flags(mg->mg_ptr, mg->mg_len, SVs_TEMP));
		} else if (mg->mg_len == HEf_SVKEY) {
		    PUSHs(make_sv_object(aTHX_ (SV*)mg->mg_ptr));
		} else
		    PUSHs(sv_newmortal());
	    } else
		PUSHs(sv_newmortal());
	    break;
	case 7:
	    if(mg->mg_type == PERL_MAGIC_qr) {
                mPUSHi(PTR2IV(mg->mg_obj));
	    } else {
		croak("REGEX is only meaningful on r-magic");
	    }
	    break;
	case 8:
	    if (mg->mg_type == PERL_MAGIC_qr) {
		REGEXP *rx = (REGEXP *)mg->mg_obj;
		PUSHs(newSVpvn_flags(rx ? RX_PRECOMP(rx) : NULL,
				     rx ? RX_PRELEN(rx) : 0, SVs_TEMP));
	    } else {
		croak( "precomp is only meaningful on r-magic" );
	    }
	    break;
	}

MODULE = B	PACKAGE = B::GV		PREFIX = Gv

void
GvNAME(gv)
	B::GV	gv
    ALIAS:
	FILE = 1
	B::HV::NAME = 2
    CODE:
#if PERL_VERSION >= 10
	ST(0) = sv_2mortal(newSVhek(!ix ? GvNAME_HEK(gv)
					: (ix == 1 ? GvFILE_HEK(gv)
						   : HvNAME_HEK((HV *)gv))));
#else
	ST(0) = !ix ? newSVpvn_flags(GvNAME(gv), GvNAMELEN(gv), SVs_TEMP)
		    : sv_2mortal(newSVpv(ix == 1 ? GvFILE(gv) : HvNAME((HV *)gv), 0))
#endif

bool
is_empty(gv)
        B::GV   gv
    ALIAS:
	isGV_with_GP = 1
    CODE:
	if (ix) {
#if PERL_VERSION >= 9
	    RETVAL = isGV_with_GP(gv) ? TRUE : FALSE;
#else
	    RETVAL = TRUE; /* In 5.8 and earlier they all are.  */
#endif
	} else {
            RETVAL = GvGP(gv) == Null(GP*);
	}
    OUTPUT:
        RETVAL

void*
GvGP(gv)
	B::GV	gv

#define GP_sv_ix	SVp | offsetof(struct gp, gp_sv)
#define GP_io_ix	SVp | offsetof(struct gp, gp_io)
#define GP_cv_ix	SVp | offsetof(struct gp, gp_cv)
#define GP_cvgen_ix	U32p | offsetof(struct gp, gp_cvgen)
#define GP_refcnt_ix	U32p | offsetof(struct gp, gp_refcnt)
#define GP_hv_ix	SVp | offsetof(struct gp, gp_hv)
#define GP_av_ix	SVp | offsetof(struct gp, gp_av)
#define GP_form_ix	SVp | offsetof(struct gp, gp_form)
#define GP_egv_ix	SVp | offsetof(struct gp, gp_egv)
#define GP_line_ix	line_tp | offsetof(struct gp, gp_line)

void
SV(gv)
	B::GV	gv
    ALIAS:
	SV = GP_sv_ix
	IO = GP_io_ix
	CV = GP_cv_ix
	CVGEN = GP_cvgen_ix
	GvREFCNT = GP_refcnt_ix
	HV = GP_hv_ix
	AV = GP_av_ix
	FORM = GP_form_ix
	EGV = GP_egv_ix
	LINE = GP_line_ix
    PREINIT:
	GP *gp;
	char *ptr;
	SV *ret;
    PPCODE:
	gp = GvGP(gv);
	if (!gp) {
	    const GV *const gv = CvGV(cv);
	    Perl_croak(aTHX_ "NULL gp in B::GV::%s", gv ? GvNAME(gv) : "???");
	}
	ptr = (ix & 0xFFFF) + (char *)gp;
	switch ((U8)(ix >> 16)) {
	case (U8)(SVp >> 16):
	    ret = make_sv_object(aTHX_ *((SV **)ptr));
	    break;
	case (U8)(U32p >> 16):
	    ret = sv_2mortal(newSVuv(*((U32*)ptr)));
	    break;
	case (U8)(line_tp >> 16):
	    ret = sv_2mortal(newSVuv(*((line_t *)ptr)));
	    break;
	default:
	    croak("Illegal alias 0x%08x for B::*SV", (unsigned)ix);
	}
	ST(0) = ret;
	XSRETURN(1);

void
FILEGV(gv)
	B::GV	gv
    PPCODE:
	PUSHs(make_sv_object(aTHX_ (SV *)GvFILEGV(gv)));

MODULE = B	PACKAGE = B::IO		PREFIX = Io

#if PERL_VERSION <= 8

short
IoSUBPROCESS(io)
	B::IO	io

#endif

bool
IsSTD(io,name)
	B::IO	io
	const char*	name
    PREINIT:
	PerlIO* handle = 0;
    CODE:
	if( strEQ( name, "stdin" ) ) {
	    handle = PerlIO_stdin();
	}
	else if( strEQ( name, "stdout" ) ) {
	    handle = PerlIO_stdout();
	}
	else if( strEQ( name, "stderr" ) ) {
	    handle = PerlIO_stderr();
	}
	else {
	    croak( "Invalid value '%s'", name );
	}
	RETVAL = handle == IoIFP(io);
    OUTPUT:
	RETVAL

MODULE = B	PACKAGE = B::AV		PREFIX = Av

SSize_t
AvFILL(av)
	B::AV	av

void
AvARRAY(av)
	B::AV	av
    PPCODE:
	if (AvFILL(av) >= 0) {
	    SV **svp = AvARRAY(av);
	    I32 i;
	    for (i = 0; i <= AvFILL(av); i++)
		XPUSHs(make_sv_object(aTHX_ svp[i]));
	}

void
AvARRAYelt(av, idx)
	B::AV	av
	int	idx
    PPCODE:
    	if (idx >= 0 && AvFILL(av) >= 0 && idx <= AvFILL(av))
	    XPUSHs(make_sv_object(aTHX_ (AvARRAY(av)[idx])));
	else
	    XPUSHs(make_sv_object(aTHX_ NULL));

#if PERL_VERSION < 9
				   
#define AvOFF(av) ((XPVAV*)SvANY(av))->xof_off

IV
AvOFF(av)
	B::AV	av

MODULE = B	PACKAGE = B::AV

U8
AvFLAGS(av)
	B::AV	av

#endif

MODULE = B	PACKAGE = B::CV		PREFIX = Cv

U32
CvCONST(cv)
	B::CV	cv

void
CvSTART(cv)
	B::CV	cv
    ALIAS:
	ROOT = 1
    PPCODE:
	PUSHs(make_op_object(aTHX_ CvISXSUB(cv) ? NULL
			     : ix ? CvROOT(cv) : CvSTART(cv)));

void
CvXSUB(cv)
	B::CV	cv
    ALIAS:
	XSUBANY = 1
    CODE:
	ST(0) = ix && CvCONST(cv)
	    ? make_sv_object(aTHX_ (SV *)CvXSUBANY(cv).any_ptr)
	    : sv_2mortal(newSViv(CvISXSUB(cv)
				 ? (ix ? CvXSUBANY(cv).any_iv
				       : PTR2IV(CvXSUB(cv)))
				 : 0));

void
const_sv(cv)
	B::CV	cv
    PPCODE:
	PUSHs(make_sv_object(aTHX_ (SV *)cv_const_sv(cv)));

MODULE = B	PACKAGE = B::HV		PREFIX = Hv

STRLEN
HvFILL(hv)
	B::HV	hv

I32
HvRITER(hv)
	B::HV	hv

#if PERL_VERSION < 9

B::PMOP
HvPMROOT(hv)
	B::HV	hv
    PPCODE:
	PUSHs(make_op_object(aTHX_ HvPMROOT(hv)));

#endif

void
HvARRAY(hv)
	B::HV	hv
    PPCODE:
	if (HvUSEDKEYS(hv) > 0) {
	    SV *sv;
	    char *key;
	    I32 len;
	    (void)hv_iterinit(hv);
	    EXTEND(sp, HvUSEDKEYS(hv) * 2);
	    while ((sv = hv_iternextsv(hv, &key, &len))) {
		mPUSHp(key, len);
		PUSHs(make_sv_object(aTHX_ sv));
	    }
	}

MODULE = B	PACKAGE = B::HE		PREFIX = He

void
HeVAL(he)
	B::HE he
    ALIAS:
	SVKEY_force = 1
    PPCODE:
	PUSHs(make_sv_object(aTHX_ ix ? HeSVKEY_force(he) : HeVAL(he)));

U32
HeHASH(he)
	B::HE he

MODULE = B	PACKAGE = B::RHE

#if PERL_VERSION >= 9

SV*
HASH(h)
	B::RHE h
    CODE:
	RETVAL = newRV( (SV*)cophh_2hv(h, 0) );
    OUTPUT:
	RETVAL

#endif

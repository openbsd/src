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


static char *svclassnames[] = {
    "B::NULL",
    "B::IV",
    "B::NV",
    "B::RV",
    "B::PV",
    "B::PVIV",
    "B::PVNV",
    "B::PVMG",
    "B::BM",
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

static char *opclassnames[] = {
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

static size_t opsizes[] = {
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
    if (!o)
	return OPc_NULL;

    if (o->op_type == 0)
	return (o->op_flags & OPf_KIDS) ? OPc_UNOP : OPc_BASEOP;

    if (o->op_type == OP_SASSIGN)
	return ((o->op_private & OPpASSIGN_BACKWARDS) ? OPc_UNOP : OPc_BINOP);

#ifdef USE_ITHREADS
    if (o->op_type == OP_GV || o->op_type == OP_GVSV ||
	o->op_type == OP_AELEMFAST || o->op_type == OP_RCATLINE)
	return OPc_PADOP;
#endif

    switch (PL_opargs[o->op_type] & OA_CLASS_MASK) {
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
         * the OP is an SVOP, and the SV is a reference to a swash
         * (i.e., an RV pointing to an HV).
         */
	return (o->op_private & (OPpTRANS_TO_UTF|OPpTRANS_FROM_UTF))
		? OPc_SVOP : OPc_PVOP;

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
	 PL_op_name[o->op_type]);
    return OPc_BASEOP;
}

static char *
cc_opclassname(pTHX_ const OP *o)
{
    return opclassnames[cc_opclass(aTHX_ o)];
}

static SV *
make_sv_object(pTHX_ SV *arg, SV *sv)
{
    char *type = 0;
    IV iv;
    dMY_CXT;
    
    for (iv = 0; iv < sizeof(specialsv_list)/sizeof(SV*); iv++) {
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

static SV *
make_mg_object(pTHX_ SV *arg, MAGIC *mg)
{
    sv_setiv(newSVrv(arg, "B::MAGIC"), PTR2IV(mg));
    return arg;
}

static SV *
cstring(pTHX_ SV *sv, bool perlstyle)
{
    SV *sstr = newSVpvn("", 0);

    if (!SvOK(sv))
	sv_setpvn(sstr, "0", 1);
    else if (perlstyle && SvUTF8(sv)) {
	SV *tmpsv = sv_newmortal(); /* Temporary SV to feed sv_uni_display */
	const STRLEN len = SvCUR(sv);
	const char *s = sv_uni_display(tmpsv, sv, 8*len, UNI_DISPLAY_QQ);
	sv_setpvn(sstr,"\"",1);
	while (*s)
	{
	    if (*s == '"')
		sv_catpvn(sstr, "\\\"", 2);
	    else if (*s == '$')
		sv_catpvn(sstr, "\\$", 2);
	    else if (*s == '@')
		sv_catpvn(sstr, "\\@", 2);
	    else if (*s == '\\')
	    {
		if (strchr("nrftax\\",*(s+1)))
		    sv_catpvn(sstr, s++, 2);
		else
		    sv_catpvn(sstr, "\\\\", 2);
	    }
	    else /* should always be printable */
		sv_catpvn(sstr, s, 1);
	    ++s;
	}
	sv_catpv(sstr, "\"");
	return sstr;
    }
    else
    {
	/* XXX Optimise? */
	STRLEN len;
	const char *s = SvPV(sv, len);
	sv_catpv(sstr, "\"");
	for (; len; len--, s++)
	{
	    /* At least try a little for readability */
	    if (*s == '"')
		sv_catpv(sstr, "\\\"");
	    else if (*s == '\\')
		sv_catpv(sstr, "\\\\");
            /* trigraphs - bleagh */
            else if (!perlstyle && *s == '?' && len>=3 && s[1] == '?') {
		char escbuff[5]; /* to fit backslash, 3 octals + trailing \0 */
                sprintf(escbuff, "\\%03o", '?');
                sv_catpv(sstr, escbuff);
            }
	    else if (perlstyle && *s == '$')
		sv_catpv(sstr, "\\$");
	    else if (perlstyle && *s == '@')
		sv_catpv(sstr, "\\@");
#ifdef EBCDIC
	    else if (isPRINT(*s))
#else
	    else if (*s >= ' ' && *s < 127)
#endif /* EBCDIC */
		sv_catpvn(sstr, s, 1);
	    else if (*s == '\n')
		sv_catpv(sstr, "\\n");
	    else if (*s == '\r')
		sv_catpv(sstr, "\\r");
	    else if (*s == '\t')
		sv_catpv(sstr, "\\t");
	    else if (*s == '\a')
		sv_catpv(sstr, "\\a");
	    else if (*s == '\b')
		sv_catpv(sstr, "\\b");
	    else if (*s == '\f')
		sv_catpv(sstr, "\\f");
	    else if (!perlstyle && *s == '\v')
		sv_catpv(sstr, "\\v");
	    else
	    {
		/* Don't want promotion of a signed -1 char in sprintf args */
		char escbuff[5]; /* to fit backslash, 3 octals + trailing \0 */
		const unsigned char c = (unsigned char) *s;
		sprintf(escbuff, "\\%03o", c);
		sv_catpv(sstr, escbuff);
	    }
	    /* XXX Add line breaks if string is long */
	}
	sv_catpv(sstr, "\"");
    }
    return sstr;
}

static SV *
cchar(pTHX_ SV *sv)
{
    SV *sstr = newSVpvn("'", 1);
    const char *s = SvPV_nolen(sv);

    if (*s == '\'')
	sv_catpvn(sstr, "\\'", 2);
    else if (*s == '\\')
	sv_catpvn(sstr, "\\\\", 2);
#ifdef EBCDIC
    else if (isPRINT(*s))
#else
    else if (*s >= ' ' && *s < 127)
#endif /* EBCDIC */
	sv_catpvn(sstr, s, 1);
    else if (*s == '\n')
	sv_catpvn(sstr, "\\n", 2);
    else if (*s == '\r')
	sv_catpvn(sstr, "\\r", 2);
    else if (*s == '\t')
	sv_catpvn(sstr, "\\t", 2);
    else if (*s == '\a')
	sv_catpvn(sstr, "\\a", 2);
    else if (*s == '\b')
	sv_catpvn(sstr, "\\b", 2);
    else if (*s == '\f')
	sv_catpvn(sstr, "\\f", 2);
    else if (*s == '\v')
	sv_catpvn(sstr, "\\v", 2);
    else
    {
	/* no trigraph support */
	char escbuff[5]; /* to fit backslash, 3 octals + trailing \0 */
	/* Don't want promotion of a signed -1 char in sprintf args */
	unsigned char c = (unsigned char) *s;
	sprintf(escbuff, "\\%03o", c);
	sv_catpv(sstr, escbuff);
    }
    sv_catpvn(sstr, "'", 1);
    return sstr;
}

static void
walkoptree(pTHX_ SV *opsv, const char *method)
{
    dSP;
    OP *o, *kid;
    dMY_CXT;

    if (!SvROK(opsv))
	croak("opsv is not a reference");
    opsv = sv_mortalcopy(opsv);
    o = INT2PTR(OP*,SvIV((SV*)SvRV(opsv)));
    if (walkoptree_debug) {
	PUSHMARK(sp);
	XPUSHs(opsv);
	PUTBACK;
	perl_call_method("walkoptree_debug", G_DISCARD);
    }
    PUSHMARK(sp);
    XPUSHs(opsv);
    PUTBACK;
    perl_call_method(method, G_DISCARD);
    if (o && (o->op_flags & OPf_KIDS)) {
	for (kid = ((UNOP*)o)->op_first; kid; kid = kid->op_sibling) {
	    /* Use the same opsv. Rely on methods not to mess it up. */
	    sv_setiv(newSVrv(opsv, cc_opclassname(aTHX_ kid)), PTR2IV(kid));
	    walkoptree(aTHX_ opsv, method);
	}
    }
    if (o && (cc_opclass(aTHX_ o) == OPc_PMOP) && o->op_type != OP_PUSHRE
	    && (kid = cPMOPo->op_pmreplroot))
    {
	sv_setiv(newSVrv(opsv, cc_opclassname(aTHX_ kid)), PTR2IV(kid));
	walkoptree(aTHX_ opsv, method);
    }
}

static SV **
oplist(pTHX_ OP *o, SV **SP)
{
    for(; o; o = o->op_next) {
	SV *opsv;
#if PERL_VERSION >= 9
	if (o->op_opt == 0)
	    break;
	o->op_opt = 0;
#else
	if (o->op_seq == 0)
	    break;
	o->op_seq = 0;
#endif
	opsv = sv_newmortal();
	sv_setiv(newSVrv(opsv, cc_opclassname(aTHX_ (OP*)o)), PTR2IV(o));
	XPUSHs(opsv);
        switch (o->op_type) {
	case OP_SUBST:
            SP = oplist(aTHX_ cPMOPo->op_pmreplstart, SP);
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

MODULE = B	PACKAGE = B	PREFIX = B_

PROTOTYPES: DISABLE

BOOT:
{
    HV *stash = gv_stashpvn("B", 1, TRUE);
    AV *export_ok = perl_get_av("B::EXPORT_OK",TRUE);
    MY_CXT_INIT;
    specialsv_list[0] = Nullsv;
    specialsv_list[1] = &PL_sv_undef;
    specialsv_list[2] = &PL_sv_yes;
    specialsv_list[3] = &PL_sv_no;
    specialsv_list[4] = pWARN_ALL;
    specialsv_list[5] = pWARN_NONE;
    specialsv_list[6] = pWARN_STD;
#if PERL_VERSION <= 8
#  define CVf_ASSERTION	0
#endif
#include "defsubs.h"
}

#define B_main_cv()	PL_main_cv
#define B_init_av()	PL_initav
#define B_inc_gv()	PL_incgv
#define B_check_av()	PL_checkav_save
#define B_begin_av()	PL_beginav_save
#define B_end_av()	PL_endav
#define B_main_root()	PL_main_root
#define B_main_start()	PL_main_start
#define B_amagic_generation()	PL_amagic_generation
#define B_defstash()	PL_defstash
#define B_curstash()	PL_curstash
#define B_dowarn()	PL_dowarn
#define B_comppadlist()	(PL_main_cv ? CvPADLIST(PL_main_cv) : CvPADLIST(PL_compcv))
#define B_sv_undef()	&PL_sv_undef
#define B_sv_yes()	&PL_sv_yes
#define B_sv_no()	&PL_sv_no
#define B_formfeed()	PL_formfeed
#ifdef USE_ITHREADS
#define B_regex_padav()	PL_regex_padav
#endif

B::AV
B_init_av()

B::AV
B_check_av()

B::AV
B_begin_av()

B::AV
B_end_av()

B::GV
B_inc_gv()

#ifdef USE_ITHREADS

B::AV
B_regex_padav()

#endif

B::CV
B_main_cv()

B::OP
B_main_root()

B::OP
B_main_start()

long 
B_amagic_generation()

B::AV
B_comppadlist()

B::SV
B_sv_undef()

B::SV
B_sv_yes()

B::SV
B_sv_no()

B::HV
B_curstash()

B::HV
B_defstash()

U8
B_dowarn()

B::SV
B_formfeed()

void
B_warnhook()
    CODE:
	ST(0) = make_sv_object(aTHX_ sv_newmortal(), PL_warnhook);

void
B_diehook()
    CODE:
	ST(0) = make_sv_object(aTHX_ sv_newmortal(), PL_diehook);

MODULE = B	PACKAGE = B

void
walkoptree(opsv, method)
	SV *	opsv
	const char *	method
    CODE:
	walkoptree(aTHX_ opsv, method);

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

B::SV
svref_2object(sv)
	SV *	sv
    CODE:
	if (!SvROK(sv))
	    croak("argument is not a reference");
	RETVAL = (SV*)SvRV(sv);
    OUTPUT:
	RETVAL              

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
	if (opnum >= 0 && opnum < PL_maxo) {
	    sv_setpvn(ST(0), "pp_", 3);
	    sv_catpv(ST(0), PL_op_name[opnum]);
	}

void
hash(sv)
	SV *	sv
    CODE:
	STRLEN len;
	U32 hash = 0;
	char hexhash[19]; /* must fit "0xffffffffffffffff" plus trailing \0 */
	const char *s = SvPV(sv, len);
	PERL_HASH(hash, s, len);
	sprintf(hexhash, "0x%"UVxf, (UV)hash);
	ST(0) = sv_2mortal(newSVpv(hexhash, 0));

#define cast_I32(foo) (I32)foo
IV
cast_I32(i)
	IV	i

void
minus_c()
    CODE:
	PL_minus_c = TRUE;

void
save_BEGINs()
    CODE:
	PL_savebegin = TRUE;

SV *
cstring(sv)
	SV *	sv
    CODE:
	RETVAL = cstring(aTHX_ sv, 0);
    OUTPUT:
	RETVAL

SV *
perlstring(sv)
	SV *	sv
    CODE:
	RETVAL = cstring(aTHX_ sv, 1);
    OUTPUT:
	RETVAL

SV *
cchar(sv)
	SV *	sv
    CODE:
	RETVAL = cchar(aTHX_ sv);
    OUTPUT:
	RETVAL

void
threadsv_names()
    PPCODE:
#if PERL_VERSION <= 8
# ifdef USE_5005THREADS
	int i;
	const STRLEN len = strlen(PL_threadsv_names);

	EXTEND(sp, len);
	for (i = 0; i < len; i++)
	    PUSHs(sv_2mortal(newSVpvn(&PL_threadsv_names[i], 1)));
# endif
#endif

#define OP_next(o)	o->op_next
#define OP_sibling(o)	o->op_sibling
#define OP_desc(o)	PL_op_desc[o->op_type]
#define OP_targ(o)	o->op_targ
#define OP_type(o)	o->op_type
#if PERL_VERSION >= 9
#  define OP_opt(o)	o->op_opt
#  define OP_static(o)	o->op_static
#else
#  define OP_seq(o)	o->op_seq
#endif
#define OP_flags(o)	o->op_flags
#define OP_private(o)	o->op_private
#define OP_spare(o)	o->op_spare

MODULE = B	PACKAGE = B::OP		PREFIX = OP_

size_t
OP_size(o)
	B::OP		o
    CODE:
	RETVAL = opsizes[cc_opclass(aTHX_ o)];
    OUTPUT:
	RETVAL

B::OP
OP_next(o)
	B::OP		o

B::OP
OP_sibling(o)
	B::OP		o

char *
OP_name(o)
	B::OP		o
    CODE:
	RETVAL = PL_op_name[o->op_type];
    OUTPUT:
	RETVAL


void
OP_ppaddr(o)
	B::OP		o
    PREINIT:
	int i;
	SV *sv = sv_newmortal();
    CODE:
	sv_setpvn(sv, "PL_ppaddr[OP_", 13);
	sv_catpv(sv, PL_op_name[o->op_type]);
	for (i=13; (STRLEN)i < SvCUR(sv); ++i)
	    SvPVX(sv)[i] = toUPPER(SvPVX(sv)[i]);
	sv_catpv(sv, "]");
	ST(0) = sv;

char *
OP_desc(o)
	B::OP		o

PADOFFSET
OP_targ(o)
	B::OP		o

U16
OP_type(o)
	B::OP		o

#if PERL_VERSION >= 9

U8
OP_opt(o)
	B::OP		o

U8
OP_static(o)
	B::OP		o

#else

U16
OP_seq(o)
	B::OP		o

#endif

U8
OP_flags(o)
	B::OP		o

U8
OP_private(o)
	B::OP		o

#if PERL_VERSION >= 9

U8
OP_spare(o)
	B::OP		o

#endif

void
OP_oplist(o)
	B::OP		o
    PPCODE:
	SP = oplist(aTHX_ o, SP);

#define UNOP_first(o)	o->op_first

MODULE = B	PACKAGE = B::UNOP		PREFIX = UNOP_

B::OP 
UNOP_first(o)
	B::UNOP	o

#define BINOP_last(o)	o->op_last

MODULE = B	PACKAGE = B::BINOP		PREFIX = BINOP_

B::OP
BINOP_last(o)
	B::BINOP	o

#define LOGOP_other(o)	o->op_other

MODULE = B	PACKAGE = B::LOGOP		PREFIX = LOGOP_

B::OP
LOGOP_other(o)
	B::LOGOP	o

MODULE = B	PACKAGE = B::LISTOP		PREFIX = LISTOP_

U32
LISTOP_children(o)
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

#define PMOP_pmreplroot(o)	o->op_pmreplroot
#define PMOP_pmreplstart(o)	o->op_pmreplstart
#define PMOP_pmnext(o)		o->op_pmnext
#define PMOP_pmregexp(o)	PM_GETRE(o)
#ifdef USE_ITHREADS
#define PMOP_pmoffset(o)	o->op_pmoffset
#define PMOP_pmstashpv(o)	o->op_pmstashpv
#else
#define PMOP_pmstash(o)		o->op_pmstash
#endif
#define PMOP_pmflags(o)		o->op_pmflags
#define PMOP_pmpermflags(o)	o->op_pmpermflags
#define PMOP_pmdynflags(o)      o->op_pmdynflags

MODULE = B	PACKAGE = B::PMOP		PREFIX = PMOP_

void
PMOP_pmreplroot(o)
	B::PMOP		o
	OP *		root = NO_INIT
    CODE:
	ST(0) = sv_newmortal();
	root = o->op_pmreplroot;
	/* OP_PUSHRE stores an SV* instead of an OP* in op_pmreplroot */
	if (o->op_type == OP_PUSHRE) {
#ifdef USE_ITHREADS
            sv_setiv(ST(0), INT2PTR(PADOFFSET,root) );
#else
	    sv_setiv(newSVrv(ST(0), root ?
			     svclassnames[SvTYPE((SV*)root)] : "B::SV"),
		     PTR2IV(root));
#endif
	}
	else {
	    sv_setiv(newSVrv(ST(0), cc_opclassname(aTHX_ root)), PTR2IV(root));
	}

B::OP
PMOP_pmreplstart(o)
	B::PMOP		o

B::PMOP
PMOP_pmnext(o)
	B::PMOP		o

#ifdef USE_ITHREADS

IV
PMOP_pmoffset(o)
	B::PMOP		o

char*
PMOP_pmstashpv(o)
	B::PMOP		o

#else

B::HV
PMOP_pmstash(o)
	B::PMOP		o

#endif

U32
PMOP_pmflags(o)
	B::PMOP		o

U32
PMOP_pmpermflags(o)
	B::PMOP		o

U8
PMOP_pmdynflags(o)
        B::PMOP         o

void
PMOP_precomp(o)
	B::PMOP		o
	REGEXP *	rx = NO_INIT
    CODE:
	ST(0) = sv_newmortal();
	rx = PM_GETRE(o);
	if (rx)
	    sv_setpvn(ST(0), rx->precomp, rx->prelen);

#define SVOP_sv(o)     cSVOPo->op_sv
#define SVOP_gv(o)     ((GV*)cSVOPo->op_sv)

MODULE = B	PACKAGE = B::SVOP		PREFIX = SVOP_

B::SV
SVOP_sv(o)
	B::SVOP	o

B::GV
SVOP_gv(o)
	B::SVOP	o

#define PADOP_padix(o)	o->op_padix
#define PADOP_sv(o)	(o->op_padix ? PAD_SVl(o->op_padix) : Nullsv)
#define PADOP_gv(o)	((o->op_padix \
			  && SvTYPE(PAD_SVl(o->op_padix)) == SVt_PVGV) \
			 ? (GV*)PAD_SVl(o->op_padix) : Nullgv)

MODULE = B	PACKAGE = B::PADOP		PREFIX = PADOP_

PADOFFSET
PADOP_padix(o)
	B::PADOP o

B::SV
PADOP_sv(o)
	B::PADOP o

B::GV
PADOP_gv(o)
	B::PADOP o

MODULE = B	PACKAGE = B::PVOP		PREFIX = PVOP_

void
PVOP_pv(o)
	B::PVOP	o
    CODE:
	/*
	 * OP_TRANS uses op_pv to point to a table of 256 or >=258 shorts
	 * whereas other PVOPs point to a null terminated string.
	 */
	if (o->op_type == OP_TRANS &&
		(o->op_private & OPpTRANS_COMPLEMENT) &&
		!(o->op_private & OPpTRANS_DELETE))
	{
	    const short* const tbl = (short*)o->op_pv;
	    const short entries = 257 + tbl[256];
	    ST(0) = sv_2mortal(newSVpv(o->op_pv, entries * sizeof(short)));
	}
	else if (o->op_type == OP_TRANS) {
	    ST(0) = sv_2mortal(newSVpv(o->op_pv, 256 * sizeof(short)));
	}
	else
	    ST(0) = sv_2mortal(newSVpv(o->op_pv, 0));

#define LOOP_redoop(o)	o->op_redoop
#define LOOP_nextop(o)	o->op_nextop
#define LOOP_lastop(o)	o->op_lastop

MODULE = B	PACKAGE = B::LOOP		PREFIX = LOOP_


B::OP
LOOP_redoop(o)
	B::LOOP	o

B::OP
LOOP_nextop(o)
	B::LOOP	o

B::OP
LOOP_lastop(o)
	B::LOOP	o

#define COP_label(o)	o->cop_label
#define COP_stashpv(o)	CopSTASHPV(o)
#define COP_stash(o)	CopSTASH(o)
#define COP_file(o)	CopFILE(o)
#define COP_filegv(o)	CopFILEGV(o)
#define COP_cop_seq(o)	o->cop_seq
#define COP_arybase(o)	o->cop_arybase
#define COP_line(o)	CopLINE(o)
#define COP_warnings(o)	o->cop_warnings
#define COP_io(o)	o->cop_io

MODULE = B	PACKAGE = B::COP		PREFIX = COP_

char *
COP_label(o)
	B::COP	o

char *
COP_stashpv(o)
	B::COP	o

B::HV
COP_stash(o)
	B::COP	o

char *
COP_file(o)
	B::COP	o

B::GV
COP_filegv(o)
       B::COP  o


U32
COP_cop_seq(o)
	B::COP	o

I32
COP_arybase(o)
	B::COP	o

U32
COP_line(o)
	B::COP	o

B::SV
COP_warnings(o)
	B::COP	o

B::SV
COP_io(o)
	B::COP	o

MODULE = B	PACKAGE = B::SV

U32
SvTYPE(sv)
	B::SV	sv

#define object_2svref(sv)	sv
#define SVREF SV *
	
SVREF
object_2svref(sv)
	B::SV	sv

MODULE = B	PACKAGE = B::SV		PREFIX = Sv

U32
SvREFCNT(sv)
	B::SV	sv

U32
SvFLAGS(sv)
	B::SV	sv

U32
SvPOK(sv)
	B::SV	sv

U32
SvROK(sv)
	B::SV	sv

U32
SvMAGICAL(sv)
	B::SV	sv

MODULE = B	PACKAGE = B::IV		PREFIX = Sv

IV
SvIV(sv)
	B::IV	sv

IV
SvIVX(sv)
	B::IV	sv

UV 
SvUVX(sv) 
	B::IV   sv
                      

MODULE = B	PACKAGE = B::IV

#define needs64bits(sv) ((I32)SvIVX(sv) != SvIVX(sv))

int
needs64bits(sv)
	B::IV	sv

void
packiv(sv)
	B::IV	sv
    CODE:
	if (sizeof(IV) == 8) {
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
	    ST(0) = sv_2mortal(newSVpvn((char *)wp, 8));
	} else {
	    U32 w = htonl((U32)SvIVX(sv));
	    ST(0) = sv_2mortal(newSVpvn((char *)&w, 4));
	}

MODULE = B	PACKAGE = B::NV		PREFIX = Sv

NV
SvNV(sv)
	B::NV	sv

NV
SvNVX(sv)
	B::NV	sv

MODULE = B	PACKAGE = B::RV		PREFIX = Sv

B::SV
SvRV(sv)
	B::RV	sv

MODULE = B	PACKAGE = B::PV		PREFIX = Sv

char*
SvPVX(sv)
	B::PV	sv

B::SV
SvRV(sv)
        B::PV   sv
    CODE:
        if( SvROK(sv) ) {
            RETVAL = SvRV(sv);
        }
        else {
            croak( "argument is not SvROK" );
        }
    OUTPUT:
        RETVAL

void
SvPV(sv)
	B::PV	sv
    CODE:
        ST(0) = sv_newmortal();
        if( SvPOK(sv) ) {
	    /* FIXME - we need a better way for B to identify PVs that are
	       in the pads as variable names.  */
	    if((SvLEN(sv) && SvCUR(sv) >= SvLEN(sv))) {
		/* It claims to be longer than the space allocated for it -
		   presuambly it's a variable name in the pad  */
		sv_setpv(ST(0), SvPV_nolen_const(sv));
	    } else {
		sv_setpvn(ST(0), SvPVX_const(sv), SvCUR(sv));
	    }
            SvFLAGS(ST(0)) |= SvUTF8(sv);
        }
        else {
            /* XXX for backward compatibility, but should fail */
            /* croak( "argument is not SvPOK" ); */
            sv_setpvn(ST(0), NULL, 0);
        }

void
SvPVBM(sv)
	B::PV	sv
    CODE:
        ST(0) = sv_newmortal();
	sv_setpvn(ST(0), SvPVX_const(sv),
	    SvCUR(sv) + (SvTYPE(sv) == SVt_PVBM ? 257 : 0));


STRLEN
SvLEN(sv)
	B::PV	sv

STRLEN
SvCUR(sv)
	B::PV	sv

MODULE = B	PACKAGE = B::PVMG	PREFIX = Sv

void
SvMAGIC(sv)
	B::PVMG	sv
	MAGIC *	mg = NO_INIT
    PPCODE:
	for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic)
	    XPUSHs(make_mg_object(aTHX_ sv_newmortal(), mg));

MODULE = B	PACKAGE = B::PVMG

B::HV
SvSTASH(sv)
	B::PVMG	sv

#define MgMOREMAGIC(mg) mg->mg_moremagic
#define MgPRIVATE(mg) mg->mg_private
#define MgTYPE(mg) mg->mg_type
#define MgFLAGS(mg) mg->mg_flags
#define MgOBJ(mg) mg->mg_obj
#define MgLENGTH(mg) mg->mg_len
#define MgREGEX(mg) PTR2IV(mg->mg_obj)

MODULE = B	PACKAGE = B::MAGIC	PREFIX = Mg	

B::MAGIC
MgMOREMAGIC(mg)
	B::MAGIC	mg
     CODE:
	if( MgMOREMAGIC(mg) ) {
	    RETVAL = MgMOREMAGIC(mg);
	}
	else {
	    XSRETURN_UNDEF;
	}
     OUTPUT:
	RETVAL

U16
MgPRIVATE(mg)
	B::MAGIC	mg

char
MgTYPE(mg)
	B::MAGIC	mg

U8
MgFLAGS(mg)
	B::MAGIC	mg

B::SV
MgOBJ(mg)
	B::MAGIC	mg

IV
MgREGEX(mg)
	B::MAGIC	mg
    CODE:
        if( mg->mg_type == 'r' ) {
            RETVAL = MgREGEX(mg);
        }
        else {
            croak( "REGEX is only meaningful on r-magic" );
        }
    OUTPUT:
        RETVAL

SV*
precomp(mg)
        B::MAGIC        mg
    CODE:
        if (mg->mg_type == 'r') {
            REGEXP* rx = (REGEXP*)mg->mg_obj;
            if( rx )
                RETVAL = newSVpvn( rx->precomp, rx->prelen );
        }
        else {
            croak( "precomp is only meaningful on r-magic" );
        }
    OUTPUT:
        RETVAL

I32 
MgLENGTH(mg)
	B::MAGIC	mg
 
void
MgPTR(mg)
	B::MAGIC	mg
    CODE:
	ST(0) = sv_newmortal();
 	if (mg->mg_ptr){
		if (mg->mg_len >= 0){
	    		sv_setpvn(ST(0), mg->mg_ptr, mg->mg_len);
		} else if (mg->mg_len == HEf_SVKEY) {
			ST(0) = make_sv_object(aTHX_
				    sv_newmortal(), (SV*)mg->mg_ptr);
		}
	}

MODULE = B	PACKAGE = B::PVLV	PREFIX = Lv

U32
LvTARGOFF(sv)
	B::PVLV	sv

U32
LvTARGLEN(sv)
	B::PVLV	sv

char
LvTYPE(sv)
	B::PVLV	sv

B::SV
LvTARG(sv)
	B::PVLV sv

MODULE = B	PACKAGE = B::BM		PREFIX = Bm

I32
BmUSEFUL(sv)
	B::BM	sv

U16
BmPREVIOUS(sv)
	B::BM	sv

U8
BmRARE(sv)
	B::BM	sv

void
BmTABLE(sv)
	B::BM	sv
	STRLEN	len = NO_INIT
	char *	str = NO_INIT
    CODE:
	str = SvPV(sv, len);
	/* Boyer-Moore table is just after string and its safety-margin \0 */
	ST(0) = sv_2mortal(newSVpvn(str + len + 1, 256));

MODULE = B	PACKAGE = B::GV		PREFIX = Gv

void
GvNAME(gv)
	B::GV	gv
    CODE:
	ST(0) = sv_2mortal(newSVpvn(GvNAME(gv), GvNAMELEN(gv)));

bool
is_empty(gv)
        B::GV   gv
    CODE:
        RETVAL = GvGP(gv) == Null(GP*);
    OUTPUT:
        RETVAL

void*
GvGP(gv)
	B::GV	gv

B::HV
GvSTASH(gv)
	B::GV	gv

B::SV
GvSV(gv)
	B::GV	gv

B::IO
GvIO(gv)
	B::GV	gv

B::FM
GvFORM(gv)
	B::GV	gv
    CODE:
	RETVAL = (SV*)GvFORM(gv);
    OUTPUT:
	RETVAL

B::AV
GvAV(gv)
	B::GV	gv

B::HV
GvHV(gv)
	B::GV	gv

B::GV
GvEGV(gv)
	B::GV	gv

B::CV
GvCV(gv)
	B::GV	gv

U32
GvCVGEN(gv)
	B::GV	gv

U32
GvLINE(gv)
	B::GV	gv

char *
GvFILE(gv)
	B::GV	gv

B::GV
GvFILEGV(gv)
	B::GV	gv

MODULE = B	PACKAGE = B::GV

U32
GvREFCNT(gv)
	B::GV	gv

U8
GvFLAGS(gv)
	B::GV	gv

MODULE = B	PACKAGE = B::IO		PREFIX = Io

long
IoLINES(io)
	B::IO	io

long
IoPAGE(io)
	B::IO	io

long
IoPAGE_LEN(io)
	B::IO	io

long
IoLINES_LEFT(io)
	B::IO	io

char *
IoTOP_NAME(io)
	B::IO	io

B::GV
IoTOP_GV(io)
	B::IO	io

char *
IoFMT_NAME(io)
	B::IO	io

B::GV
IoFMT_GV(io)
	B::IO	io

char *
IoBOTTOM_NAME(io)
	B::IO	io

B::GV
IoBOTTOM_GV(io)
	B::IO	io

short
IoSUBPROCESS(io)
	B::IO	io

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

MODULE = B	PACKAGE = B::IO

char
IoTYPE(io)
	B::IO	io

U8
IoFLAGS(io)
	B::IO	io

MODULE = B	PACKAGE = B::AV		PREFIX = Av

SSize_t
AvFILL(av)
	B::AV	av

SSize_t
AvMAX(av)
	B::AV	av

#define AvOFF(av) ((XPVAV*)SvANY(av))->xof_off

IV
AvOFF(av)
	B::AV	av

void
AvARRAY(av)
	B::AV	av
    PPCODE:
	if (AvFILL(av) >= 0) {
	    SV **svp = AvARRAY(av);
	    I32 i;
	    for (i = 0; i <= AvFILL(av); i++)
		XPUSHs(make_sv_object(aTHX_ sv_newmortal(), svp[i]));
	}

void
AvARRAYelt(av, idx)
	B::AV	av
	int	idx
    PPCODE:
    	if (idx >= 0 && AvFILL(av) >= 0 && idx <= AvFILL(av))
	    XPUSHs(make_sv_object(aTHX_ sv_newmortal(), (AvARRAY(av)[idx])));
	else
	    XPUSHs(make_sv_object(aTHX_ sv_newmortal(), NULL));

				   
MODULE = B	PACKAGE = B::AV

U8
AvFLAGS(av)
	B::AV	av

MODULE = B	PACKAGE = B::FM		PREFIX = Fm

IV
FmLINES(form)
	B::FM	form

MODULE = B	PACKAGE = B::CV		PREFIX = Cv

U32
CvCONST(cv)
	B::CV	cv

B::HV
CvSTASH(cv)
	B::CV	cv

B::OP
CvSTART(cv)
	B::CV	cv

B::OP
CvROOT(cv)
	B::CV	cv

B::GV
CvGV(cv)
	B::CV	cv

char *
CvFILE(cv)
	B::CV	cv

long
CvDEPTH(cv)
	B::CV	cv

B::AV
CvPADLIST(cv)
	B::CV	cv

B::CV
CvOUTSIDE(cv)
	B::CV	cv

U32
CvOUTSIDE_SEQ(cv)
	B::CV	cv

void
CvXSUB(cv)
	B::CV	cv
    CODE:
	ST(0) = sv_2mortal(newSViv(PTR2IV(CvXSUB(cv))));


void
CvXSUBANY(cv)
	B::CV	cv
    CODE:
	ST(0) = CvCONST(cv) ?
	    make_sv_object(aTHX_ sv_newmortal(),(SV *)CvXSUBANY(cv).any_ptr) :
	    sv_2mortal(newSViv(CvXSUBANY(cv).any_iv));

MODULE = B    PACKAGE = B::CV

U16
CvFLAGS(cv)
      B::CV   cv

MODULE = B	PACKAGE = B::CV		PREFIX = cv_

B::SV
cv_const_sv(cv)
	B::CV	cv


MODULE = B	PACKAGE = B::HV		PREFIX = Hv

STRLEN
HvFILL(hv)
	B::HV	hv

STRLEN
HvMAX(hv)
	B::HV	hv

I32
HvKEYS(hv)
	B::HV	hv

I32
HvRITER(hv)
	B::HV	hv

char *
HvNAME(hv)
	B::HV	hv

B::PMOP
HvPMROOT(hv)
	B::HV	hv

void
HvARRAY(hv)
	B::HV	hv
    PPCODE:
	if (HvKEYS(hv) > 0) {
	    SV *sv;
	    char *key;
	    I32 len;
	    (void)hv_iterinit(hv);
	    EXTEND(sp, HvKEYS(hv) * 2);
	    while ((sv = hv_iternextsv(hv, &key, &len))) {
		PUSHs(newSVpvn(key, len));
		PUSHs(make_sv_object(aTHX_ sv_newmortal(), sv));
	    }
	}

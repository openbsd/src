/*    toke.c
 *
 *    Copyright (c) 1991-1997, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 *   "It all comes from here, the stench and the peril."  --Frodo
 */

#include "EXTERN.h"
#include "perl.h"

static void check_uni _((void));
static void  force_next _((I32 type));
static char *force_version _((char *start));
static char *force_word _((char *start, int token, int check_keyword, int allow_pack, int allow_tick));
static SV *q _((SV *sv));
static char *scan_const _((char *start));
static char *scan_formline _((char *s));
static char *scan_heredoc _((char *s));
static char *scan_ident _((char *s, char *send, char *dest, STRLEN destlen,
			   I32 ck_uni));
static char *scan_inputsymbol _((char *start));
static char *scan_pat _((char *start));
static char *scan_str _((char *start));
static char *scan_subst _((char *start));
static char *scan_trans _((char *start));
static char *scan_word _((char *s, char *dest, STRLEN destlen,
			  int allow_package, STRLEN *slp));
static char *skipspace _((char *s));
static void checkcomma _((char *s, char *name, char *what));
static void force_ident _((char *s, int kind));
static void incline _((char *s));
static int intuit_method _((char *s, GV *gv));
static int intuit_more _((char *s));
static I32 lop _((I32 f, expectation x, char *s));
static void missingterm _((char *s));
static void no_op _((char *what, char *s));
static void set_csh _((void));
static I32 sublex_done _((void));
static I32 sublex_push _((void));
static I32 sublex_start _((void));
#ifdef CRIPPLED_CC
static int uni _((I32 f, char *s));
#endif
static char * filter_gets _((SV *sv, PerlIO *fp, STRLEN append));
static void restore_rsfp _((void *f));

static char ident_too_long[] = "Identifier too long";

static char *linestart;		/* beg. of most recently read line */

static char pending_ident;	/* pending identifier lookup */

static struct {
    I32 super_state;	/* lexer state to save */
    I32 sub_inwhat;	/* "lex_inwhat" to use */
    OP *sub_op;		/* "lex_op" to use */
} sublex_info;

/* The following are arranged oddly so that the guard on the switch statement
 * can get by with a single comparison (if the compiler is smart enough).
 */

/* #define LEX_NOTPARSING		11 is done in perl.h. */

#define LEX_NORMAL		10
#define LEX_INTERPNORMAL	 9
#define LEX_INTERPCASEMOD	 8
#define LEX_INTERPPUSH		 7
#define LEX_INTERPSTART		 6
#define LEX_INTERPEND		 5
#define LEX_INTERPENDMAYBE	 4
#define LEX_INTERPCONCAT	 3
#define LEX_INTERPCONST		 2
#define LEX_FORMLINE		 1
#define LEX_KNOWNEXT		 0

#ifdef I_FCNTL
#include <fcntl.h>
#endif
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif

/* XXX If this causes problems, set i_unistd=undef in the hint file.  */
#ifdef I_UNISTD
#  include <unistd.h> /* Needed for execv() */
#endif


#ifdef ff_next
#undef ff_next
#endif

#include "keywords.h"

#ifdef CLINE
#undef CLINE
#endif
#define CLINE (copline = (curcop->cop_line < copline ? curcop->cop_line : copline))

#define TOKEN(retval) return (bufptr = s,(int)retval)
#define OPERATOR(retval) return (expect = XTERM,bufptr = s,(int)retval)
#define AOPERATOR(retval) return ao((expect = XTERM,bufptr = s,(int)retval))
#define PREBLOCK(retval) return (expect = XBLOCK,bufptr = s,(int)retval)
#define PRETERMBLOCK(retval) return (expect = XTERMBLOCK,bufptr = s,(int)retval)
#define PREREF(retval) return (expect = XREF,bufptr = s,(int)retval)
#define TERM(retval) return (CLINE, expect = XOPERATOR,bufptr = s,(int)retval)
#define LOOPX(f) return(yylval.ival=f,expect = XTERM,bufptr = s,(int)LOOPEX)
#define FTST(f) return(yylval.ival=f,expect = XTERM,bufptr = s,(int)UNIOP)
#define FUN0(f) return(yylval.ival = f,expect = XOPERATOR,bufptr = s,(int)FUNC0)
#define FUN1(f) return(yylval.ival = f,expect = XOPERATOR,bufptr = s,(int)FUNC1)
#define BOop(f) return ao((yylval.ival=f,expect = XTERM,bufptr = s,(int)BITOROP))
#define BAop(f) return ao((yylval.ival=f,expect = XTERM,bufptr = s,(int)BITANDOP))
#define SHop(f) return ao((yylval.ival=f,expect = XTERM,bufptr = s,(int)SHIFTOP))
#define PWop(f) return ao((yylval.ival=f,expect = XTERM,bufptr = s,(int)POWOP))
#define PMop(f) return(yylval.ival=f,expect = XTERM,bufptr = s,(int)MATCHOP)
#define Aop(f) return ao((yylval.ival=f,expect = XTERM,bufptr = s,(int)ADDOP))
#define Mop(f) return ao((yylval.ival=f,expect = XTERM,bufptr = s,(int)MULOP))
#define Eop(f) return(yylval.ival=f,expect = XTERM,bufptr = s,(int)EQOP)
#define Rop(f) return(yylval.ival=f,expect = XTERM,bufptr = s,(int)RELOP)

/* This bit of chicanery makes a unary function followed by
 * a parenthesis into a function with one argument, highest precedence.
 */
#define UNI(f) return(yylval.ival = f, \
	expect = XTERM, \
	bufptr = s, \
	last_uni = oldbufptr, \
	last_lop_op = f, \
	(*s == '(' || (s = skipspace(s), *s == '(') ? (int)FUNC1 : (int)UNIOP) )

#define UNIBRACK(f) return(yylval.ival = f, \
	bufptr = s, \
	last_uni = oldbufptr, \
	(*s == '(' || (s = skipspace(s), *s == '(') ? (int)FUNC1 : (int)UNIOP) )

/* grandfather return to old style */
#define OLDLOP(f) return(yylval.ival=f,expect = XTERM,bufptr = s,(int)LSTOP)

static int
ao(toketype)
int toketype;
{
    if (*bufptr == '=') {
	bufptr++;
	if (toketype == ANDAND)
	    yylval.ival = OP_ANDASSIGN;
	else if (toketype == OROR)
	    yylval.ival = OP_ORASSIGN;
	toketype = ASSIGNOP;
    }
    return toketype;
}

static void
no_op(what, s)
char *what;
char *s;
{
    char *oldbp = bufptr;
    bool is_first = (oldbufptr == linestart);

    bufptr = s;
    yywarn(form("%s found where operator expected", what));
    if (is_first)
	warn("\t(Missing semicolon on previous line?)\n");
    else if (oldoldbufptr && isIDFIRST(*oldoldbufptr)) {
	char *t;
	for (t = oldoldbufptr; *t && (isALNUM(*t) || *t == ':'); t++) ;
	if (t < bufptr && isSPACE(*t))
	    warn("\t(Do you need to predeclare %.*s?)\n",
		t - oldoldbufptr, oldoldbufptr);

    }
    else
	warn("\t(Missing operator before %.*s?)\n", s - oldbp, oldbp);
    bufptr = oldbp;
}

static void
missingterm(s)
char *s;
{
    char tmpbuf[3];
    char q;
    if (s) {
	char *nl = strrchr(s,'\n');
	if (nl)
	    *nl = '\0';
    }
    else if (multi_close < 32 || multi_close == 127) {
	*tmpbuf = '^';
	tmpbuf[1] = toCTRL(multi_close);
	s = "\\n";
	tmpbuf[2] = '\0';
	s = tmpbuf;
    }
    else {
	*tmpbuf = multi_close;
	tmpbuf[1] = '\0';
	s = tmpbuf;
    }
    q = strchr(s,'"') ? '\'' : '"';
    croak("Can't find string terminator %c%s%c anywhere before EOF",q,s,q);
}

void
deprecate(s)
char *s;
{
    if (dowarn)
	warn("Use of %s is deprecated", s);
}

static void
depcom()
{
    deprecate("comma-less variable list");
}

void
lex_start(line)
SV *line;
{
    char *s;
    STRLEN len;

    SAVEI32(lex_dojoin);
    SAVEI32(lex_brackets);
    SAVEI32(lex_fakebrack);
    SAVEI32(lex_casemods);
    SAVEI32(lex_starts);
    SAVEI32(lex_state);
    SAVESPTR(lex_inpat);
    SAVEI32(lex_inwhat);
    SAVEI16(curcop->cop_line);
    SAVEPPTR(bufptr);
    SAVEPPTR(bufend);
    SAVEPPTR(oldbufptr);
    SAVEPPTR(oldoldbufptr);
    SAVEPPTR(linestart);
    SAVESPTR(linestr);
    SAVEPPTR(lex_brackstack);
    SAVEPPTR(lex_casestack);
    SAVEDESTRUCTOR(restore_rsfp, rsfp);

    lex_state = LEX_NORMAL;
    lex_defer = 0;
    expect = XSTATE;
    lex_brackets = 0;
    lex_fakebrack = 0;
    New(899, lex_brackstack, 120, char);
    New(899, lex_casestack, 12, char);
    SAVEFREEPV(lex_brackstack);
    SAVEFREEPV(lex_casestack);
    lex_casemods = 0;
    *lex_casestack = '\0';
    lex_dojoin = 0;
    lex_starts = 0;
    if (lex_stuff)
	SvREFCNT_dec(lex_stuff);
    lex_stuff = Nullsv;
    if (lex_repl)
	SvREFCNT_dec(lex_repl);
    lex_repl = Nullsv;
    lex_inpat = 0;
    lex_inwhat = 0;
    linestr = line;
    if (SvREADONLY(linestr))
	linestr = sv_2mortal(newSVsv(linestr));
    s = SvPV(linestr, len);
    if (len && s[len-1] != ';') {
	if (!(SvFLAGS(linestr) & SVs_TEMP))
	    linestr = sv_2mortal(newSVsv(linestr));
	sv_catpvn(linestr, "\n;", 2);
    }
    SvTEMP_off(linestr);
    oldoldbufptr = oldbufptr = bufptr = linestart = SvPVX(linestr);
    bufend = bufptr + SvCUR(linestr);
    SvREFCNT_dec(rs);
    rs = newSVpv("\n", 1);
    rsfp = 0;
}

void
lex_end()
{
    doextract = FALSE;
}

static void
restore_rsfp(f)
void *f;
{
    PerlIO *fp = (PerlIO*)f;

    if (rsfp == PerlIO_stdin())
	PerlIO_clearerr(rsfp);
    else if (rsfp && (rsfp != fp))
	PerlIO_close(rsfp);
    rsfp = fp;
}

static void
incline(s)
char *s;
{
    char *t;
    char *n;
    char ch;
    int sawline = 0;

    curcop->cop_line++;
    if (*s++ != '#')
	return;
    while (*s == ' ' || *s == '\t') s++;
    if (strnEQ(s, "line ", 5)) {
	s += 5;
	sawline = 1;
    }
    if (!isDIGIT(*s))
	return;
    n = s;
    while (isDIGIT(*s))
	s++;
    while (*s == ' ' || *s == '\t')
	s++;
    if (*s == '"' && (t = strchr(s+1, '"')))
	s++;
    else {
	if (!sawline)
	    return;		/* false alarm */
	for (t = s; !isSPACE(*t); t++) ;
    }
    ch = *t;
    *t = '\0';
    if (t - s > 0)
	curcop->cop_filegv = gv_fetchfile(s);
    else
	curcop->cop_filegv = gv_fetchfile(origfilename);
    *t = ch;
    curcop->cop_line = atoi(n)-1;
}

static char *
skipspace(s)
register char *s;
{
    if (lex_formbrack && lex_brackets <= lex_formbrack) {
	while (s < bufend && (*s == ' ' || *s == '\t'))
	    s++;
	return s;
    }
    for (;;) {
	STRLEN prevlen;
	while (s < bufend && isSPACE(*s))
	    s++;
	if (s < bufend && *s == '#') {
	    while (s < bufend && *s != '\n')
		s++;
	    if (s < bufend)
		s++;
	}
	if (s < bufend || !rsfp || lex_state != LEX_NORMAL)
	    return s;
	if ((s = filter_gets(linestr, rsfp, (prevlen = SvCUR(linestr)))) == Nullch) {
	    if (minus_n || minus_p) {
		sv_setpv(linestr,minus_p ?
			 ";}continue{print or die qq(-p destination: $!\\n)" :
			 "");
		sv_catpv(linestr,";}");
		minus_n = minus_p = 0;
	    }
	    else
		sv_setpv(linestr,";");
	    oldoldbufptr = oldbufptr = bufptr = s = linestart = SvPVX(linestr);
	    bufend = SvPVX(linestr) + SvCUR(linestr);
	    if (preprocess && !in_eval)
		(void)my_pclose(rsfp);
	    else if ((PerlIO*)rsfp == PerlIO_stdin())
		PerlIO_clearerr(rsfp);
	    else
		(void)PerlIO_close(rsfp);
	    if (e_fp == rsfp)
		e_fp = Nullfp;
	    rsfp = Nullfp;
	    return s;
	}
	linestart = bufptr = s + prevlen;
	bufend = s + SvCUR(linestr);
	s = bufptr;
	incline(s);
	if (PERLDB_LINE && curstash != debstash) {
	    SV *sv = NEWSV(85,0);

	    sv_upgrade(sv, SVt_PVMG);
	    sv_setpvn(sv,bufptr,bufend-bufptr);
	    av_store(GvAV(curcop->cop_filegv),(I32)curcop->cop_line,sv);
	}
    }
}

static void
check_uni() {
    char *s;
    char ch;
    char *t;

    if (oldoldbufptr != last_uni)
	return;
    while (isSPACE(*last_uni))
	last_uni++;
    for (s = last_uni; isALNUM(*s) || *s == '-'; s++) ;
    if ((t = strchr(s, '(')) && t < bufptr)
	return;
    ch = *s;
    *s = '\0';
    warn("Warning: Use of \"%s\" without parens is ambiguous", last_uni);
    *s = ch;
}

#ifdef CRIPPLED_CC

#undef UNI
#define UNI(f) return uni(f,s)

static int
uni(f,s)
I32 f;
char *s;
{
    yylval.ival = f;
    expect = XTERM;
    bufptr = s;
    last_uni = oldbufptr;
    last_lop_op = f;
    if (*s == '(')
	return FUNC1;
    s = skipspace(s);
    if (*s == '(')
	return FUNC1;
    else
	return UNIOP;
}

#endif /* CRIPPLED_CC */

#define LOP(f,x) return lop(f,x,s)

static I32
lop
#ifdef CAN_PROTOTYPE
   (I32 f, expectation x, char *s)
#else
   (f,x,s)
I32 f;
expectation x;
char *s;
#endif /* CAN_PROTOTYPE */
{
    yylval.ival = f;
    CLINE;
    expect = x;
    bufptr = s;
    last_lop = oldbufptr;
    last_lop_op = f;
    if (nexttoke)
	return LSTOP;
    if (*s == '(')
	return FUNC;
    s = skipspace(s);
    if (*s == '(')
	return FUNC;
    else
	return LSTOP;
}

static void 
force_next(type)
I32 type;
{
    nexttype[nexttoke] = type;
    nexttoke++;
    if (lex_state != LEX_KNOWNEXT) {
	lex_defer = lex_state;
	lex_expect = expect;
	lex_state = LEX_KNOWNEXT;
    }
}

static char *
force_word(start,token,check_keyword,allow_pack,allow_tick)
register char *start;
int token;
int check_keyword;
int allow_pack;
int allow_tick;
{
    register char *s;
    STRLEN len;
    
    start = skipspace(start);
    s = start;
    if (isIDFIRST(*s) ||
	(allow_pack && *s == ':') ||
	(allow_tick && *s == '\'') )
    {
	s = scan_word(s, tokenbuf, sizeof tokenbuf, allow_pack, &len);
	if (check_keyword && keyword(tokenbuf, len))
	    return start;
	if (token == METHOD) {
	    s = skipspace(s);
	    if (*s == '(')
		expect = XTERM;
	    else {
		expect = XOPERATOR;
		force_next(')');
		force_next('(');
	    }
	}
	nextval[nexttoke].opval = (OP*)newSVOP(OP_CONST,0, newSVpv(tokenbuf,0));
	nextval[nexttoke].opval->op_private |= OPpCONST_BARE;
	force_next(token);
    }
    return s;
}

static void
force_ident(s, kind)
register char *s;
int kind;
{
    if (s && *s) {
	OP* op = (OP*)newSVOP(OP_CONST, 0, newSVpv(s,0));
	nextval[nexttoke].opval = op;
	force_next(WORD);
	if (kind) {
	    op->op_private = OPpCONST_ENTERED;
	    /* XXX see note in pp_entereval() for why we forgo typo
	       warnings if the symbol must be introduced in an eval.
	       GSAR 96-10-12 */
	    gv_fetchpv(s, in_eval ? GV_ADDMULTI : TRUE,
		kind == '$' ? SVt_PV :
		kind == '@' ? SVt_PVAV :
		kind == '%' ? SVt_PVHV :
			      SVt_PVGV
		);
	}
    }
}

static char *
force_version(s)
char *s;
{
    OP *version = Nullop;

    s = skipspace(s);

    /* default VERSION number -- GBARR */

    if(isDIGIT(*s)) {
        char *d;
        int c;
        for( d=s, c = 1; isDIGIT(*d) || *d == '_' || (*d == '.' && c--); d++);
        if((*d == ';' || isSPACE(*d)) && *(skipspace(d)) != ',') {
            s = scan_num(s);
            /* real VERSION number -- GBARR */
            version = yylval.opval;
        }
    }

    /* NOTE: The parser sees the package name and the VERSION swapped */
    nextval[nexttoke].opval = version;
    force_next(WORD); 

    return (s);
}

static SV *
q(sv)
SV *sv;
{
    register char *s;
    register char *send;
    register char *d;
    STRLEN len;

    if (!SvLEN(sv))
	return sv;

    s = SvPV_force(sv, len);
    if (SvIVX(sv) == -1)
	return sv;
    send = s + len;
    while (s < send && *s != '\\')
	s++;
    if (s == send)
	return sv;
    d = s;
    while (s < send) {
	if (*s == '\\') {
	    if (s + 1 < send && (s[1] == '\\'))
		s++;		/* all that, just for this */
	}
	*d++ = *s++;
    }
    *d = '\0';
    SvCUR_set(sv, d - SvPVX(sv));

    return sv;
}

static I32
sublex_start()
{
    register I32 op_type = yylval.ival;

    if (op_type == OP_NULL) {
	yylval.opval = lex_op;
	lex_op = Nullop;
	return THING;
    }
    if (op_type == OP_CONST || op_type == OP_READLINE) {
	SV *sv = q(lex_stuff);
	STRLEN len;
	char *p = SvPV(sv, len);
	yylval.opval = (OP*)newSVOP(op_type, 0, newSVpv(p, len));
	SvREFCNT_dec(sv);
	lex_stuff = Nullsv;
	return THING;
    }

    sublex_info.super_state = lex_state;
    sublex_info.sub_inwhat = op_type;
    sublex_info.sub_op = lex_op;
    lex_state = LEX_INTERPPUSH;

    expect = XTERM;
    if (lex_op) {
	yylval.opval = lex_op;
	lex_op = Nullop;
	return PMFUNC;
    }
    else
	return FUNC;
}

static I32
sublex_push()
{
    push_scope();

    lex_state = sublex_info.super_state;
    SAVEI32(lex_dojoin);
    SAVEI32(lex_brackets);
    SAVEI32(lex_fakebrack);
    SAVEI32(lex_casemods);
    SAVEI32(lex_starts);
    SAVEI32(lex_state);
    SAVESPTR(lex_inpat);
    SAVEI32(lex_inwhat);
    SAVEI16(curcop->cop_line);
    SAVEPPTR(bufptr);
    SAVEPPTR(oldbufptr);
    SAVEPPTR(oldoldbufptr);
    SAVEPPTR(linestart);
    SAVESPTR(linestr);
    SAVEPPTR(lex_brackstack);
    SAVEPPTR(lex_casestack);

    linestr = lex_stuff;
    lex_stuff = Nullsv;

    bufend = bufptr = oldbufptr = oldoldbufptr = linestart = SvPVX(linestr);
    bufend += SvCUR(linestr);
    SAVEFREESV(linestr);

    lex_dojoin = FALSE;
    lex_brackets = 0;
    lex_fakebrack = 0;
    New(899, lex_brackstack, 120, char);
    New(899, lex_casestack, 12, char);
    SAVEFREEPV(lex_brackstack);
    SAVEFREEPV(lex_casestack);
    lex_casemods = 0;
    *lex_casestack = '\0';
    lex_starts = 0;
    lex_state = LEX_INTERPCONCAT;
    curcop->cop_line = multi_start;

    lex_inwhat = sublex_info.sub_inwhat;
    if (lex_inwhat == OP_MATCH || lex_inwhat == OP_SUBST)
	lex_inpat = sublex_info.sub_op;
    else
	lex_inpat = Nullop;

    return '(';
}

static I32
sublex_done()
{
    if (!lex_starts++) {
	expect = XOPERATOR;
	yylval.opval = (OP*)newSVOP(OP_CONST, 0, newSVpv("",0));
	return THING;
    }

    if (lex_casemods) {		/* oops, we've got some unbalanced parens */
	lex_state = LEX_INTERPCASEMOD;
	return yylex();
    }

    /* Is there a right-hand side to take care of? */
    if (lex_repl && (lex_inwhat == OP_SUBST || lex_inwhat == OP_TRANS)) {
	linestr = lex_repl;
	lex_inpat = 0;
	bufend = bufptr = oldbufptr = oldoldbufptr = linestart = SvPVX(linestr);
	bufend += SvCUR(linestr);
	SAVEFREESV(linestr);
	lex_dojoin = FALSE;
	lex_brackets = 0;
	lex_fakebrack = 0;
	lex_casemods = 0;
	*lex_casestack = '\0';
	lex_starts = 0;
	if (SvCOMPILED(lex_repl)) {
	    lex_state = LEX_INTERPNORMAL;
	    lex_starts++;
	}
	else
	    lex_state = LEX_INTERPCONCAT;
	lex_repl = Nullsv;
	return ',';
    }
    else {
	pop_scope();
	bufend = SvPVX(linestr);
	bufend += SvCUR(linestr);
	expect = XOPERATOR;
	return ')';
    }
}

static char *
scan_const(start)
char *start;
{
    register char *send = bufend;
    SV *sv = NEWSV(93, send - start);
    register char *s = start;
    register char *d = SvPVX(sv);
    bool dorange = FALSE;
    I32 len;
    char *leave =
	lex_inpat
	    ? "\\.^$@AGZdDwWsSbB+*?|()-nrtfeaxc0123456789[{]} \t\n\r\f\v#"
	    : (lex_inwhat & OP_TRANS)
		? ""
		: "";

    while (s < send || dorange) {
	if (lex_inwhat == OP_TRANS) {
	    if (dorange) {
		I32 i;
		I32 max;
		i = d - SvPVX(sv);
		SvGROW(sv, SvLEN(sv) + 256);
		d = SvPVX(sv) + i;
		d -= 2;
		max = (U8)d[1];
		for (i = (U8)*d; i <= max; i++)
		    *d++ = i;
		dorange = FALSE;
		continue;
	    }
	    else if (*s == '-' && s+1 < send  && s != start) {
		dorange = TRUE;
		s++;
	    }
	}
	else if (*s == '(' && lex_inpat && s[1] == '?' && s[2] == '#') {
	    while (s < send && *s != ')')
		*d++ = *s++;
	}
	else if (*s == '#' && lex_inpat &&
	  ((PMOP*)lex_inpat)->op_pmflags & PMf_EXTENDED) {
	    while (s+1 < send && *s != '\n')
		*d++ = *s++;
	}
	else if (*s == '@' && s[1] && (isALNUM(s[1]) || strchr(":'{$", s[1])))
	    break;
	else if (*s == '$') {
	    if (!lex_inpat)	/* not a regexp, so $ must be var */
		break;
	    if (s + 1 < send && !strchr(")| \n\t", s[1]))
		break;		/* in regexp, $ might be tail anchor */
	}
	if (*s == '\\' && s+1 < send) {
	    s++;
	    if (*s && strchr(leave, *s)) {
		*d++ = '\\';
		*d++ = *s++;
		continue;
	    }
	    if (lex_inwhat == OP_SUBST && !lex_inpat &&
		isDIGIT(*s) && *s != '0' && !isDIGIT(s[1]))
	    {
		if (dowarn)
		    warn("\\%c better written as $%c", *s, *s);
		*--s = '$';
		break;
	    }
	    if (lex_inwhat != OP_TRANS && *s && strchr("lLuUEQ", *s)) {
		--s;
		break;
	    }
	    switch (*s) {
	    case '-':
		if (lex_inwhat == OP_TRANS) {
		    *d++ = *s++;
		    continue;
		}
		/* FALL THROUGH */
	    default:
		*d++ = *s++;
		continue;
	    case '0': case '1': case '2': case '3':
	    case '4': case '5': case '6': case '7':
		*d++ = scan_oct(s, 3, &len);
		s += len;
		continue;
	    case 'x':
		*d++ = scan_hex(++s, 2, &len);
		s += len;
		continue;
	    case 'c':
		s++;
		len = *s++;
		*d++ = toCTRL(len);
		continue;
	    case 'b':
		*d++ = '\b';
		break;
	    case 'n':
		*d++ = '\n';
		break;
	    case 'r':
		*d++ = '\r';
		break;
	    case 'f':
		*d++ = '\f';
		break;
	    case 't':
		*d++ = '\t';
		break;
	    case 'e':
		*d++ = '\033';
		break;
	    case 'a':
		*d++ = '\007';
		break;
	    }
	    s++;
	    continue;
	}
	*d++ = *s++;
    }
    *d = '\0';
    SvCUR_set(sv, d - SvPVX(sv));
    SvPOK_on(sv);

    if (SvCUR(sv) + 5 < SvLEN(sv)) {
	SvLEN_set(sv, SvCUR(sv) + 1);
	Renew(SvPVX(sv), SvLEN(sv), char);
    }
    if (s > bufptr)
	yylval.opval = (OP*)newSVOP(OP_CONST, 0, sv);
    else
	SvREFCNT_dec(sv);
    return s;
}

/* This is the one truly awful dwimmer necessary to conflate C and sed. */
static int
intuit_more(s)
register char *s;
{
    if (lex_brackets)
	return TRUE;
    if (*s == '-' && s[1] == '>' && (s[2] == '[' || s[2] == '{'))
	return TRUE;
    if (*s != '{' && *s != '[')
	return FALSE;
    if (!lex_inpat)
	return TRUE;

    /* In a pattern, so maybe we have {n,m}. */
    if (*s == '{') {
	s++;
	if (!isDIGIT(*s))
	    return TRUE;
	while (isDIGIT(*s))
	    s++;
	if (*s == ',')
	    s++;
	while (isDIGIT(*s))
	    s++;
	if (*s == '}')
	    return FALSE;
	return TRUE;
	
    }

    /* On the other hand, maybe we have a character class */

    s++;
    if (*s == ']' || *s == '^')
	return FALSE;
    else {
	int weight = 2;		/* let's weigh the evidence */
	char seen[256];
	unsigned char un_char = 0, last_un_char;
	char *send = strchr(s,']');
	char tmpbuf[sizeof tokenbuf * 4];

	if (!send)		/* has to be an expression */
	    return TRUE;

	Zero(seen,256,char);
	if (*s == '$')
	    weight -= 3;
	else if (isDIGIT(*s)) {
	    if (s[1] != ']') {
		if (isDIGIT(s[1]) && s[2] == ']')
		    weight -= 10;
	    }
	    else
		weight -= 100;
	}
	for (; s < send; s++) {
	    last_un_char = un_char;
	    un_char = (unsigned char)*s;
	    switch (*s) {
	    case '@':
	    case '&':
	    case '$':
		weight -= seen[un_char] * 10;
		if (isALNUM(s[1])) {
		    scan_ident(s, send, tmpbuf, sizeof tmpbuf, FALSE);
		    if ((int)strlen(tmpbuf) > 1 && gv_fetchpv(tmpbuf,FALSE, SVt_PV))
			weight -= 100;
		    else
			weight -= 10;
		}
		else if (*s == '$' && s[1] &&
		  strchr("[#!%*<>()-=",s[1])) {
		    if (/*{*/ strchr("])} =",s[2]))
			weight -= 10;
		    else
			weight -= 1;
		}
		break;
	    case '\\':
		un_char = 254;
		if (s[1]) {
		    if (strchr("wds]",s[1]))
			weight += 100;
		    else if (seen['\''] || seen['"'])
			weight += 1;
		    else if (strchr("rnftbxcav",s[1]))
			weight += 40;
		    else if (isDIGIT(s[1])) {
			weight += 40;
			while (s[1] && isDIGIT(s[1]))
			    s++;
		    }
		}
		else
		    weight += 100;
		break;
	    case '-':
		if (s[1] == '\\')
		    weight += 50;
		if (strchr("aA01! ",last_un_char))
		    weight += 30;
		if (strchr("zZ79~",s[1]))
		    weight += 30;
		break;
	    default:
		if (!isALNUM(last_un_char) && !strchr("$@&",last_un_char) &&
			isALPHA(*s) && s[1] && isALPHA(s[1])) {
		    char *d = tmpbuf;
		    while (isALPHA(*s))
			*d++ = *s++;
		    *d = '\0';
		    if (keyword(tmpbuf, d - tmpbuf))
			weight -= 150;
		}
		if (un_char == last_un_char + 1)
		    weight += 5;
		weight -= seen[un_char];
		break;
	    }
	    seen[un_char]++;
	}
	if (weight >= 0)	/* probably a character class */
	    return FALSE;
    }

    return TRUE;
}

static int
intuit_method(start,gv)
char *start;
GV *gv;
{
    char *s = start + (*start == '$');
    char tmpbuf[sizeof tokenbuf];
    STRLEN len;
    GV* indirgv;

    if (gv) {
	if (GvIO(gv))
	    return 0;
	if (!GvCVu(gv))
	    gv = 0;
    }
    s = scan_word(s, tmpbuf, sizeof tmpbuf, TRUE, &len);
    if (*start == '$') {
	if (gv || last_lop_op == OP_PRINT || isUPPER(*tokenbuf))
	    return 0;
	s = skipspace(s);
	bufptr = start;
	expect = XREF;
	return *s == '(' ? FUNCMETH : METHOD;
    }
    if (!keyword(tmpbuf, len)) {
	indirgv = gv_fetchpv(tmpbuf,FALSE, SVt_PVCV);
	if (indirgv && GvCVu(indirgv))
	    return 0;
	/* filehandle or package name makes it a method */
	if (!gv || GvIO(indirgv) || gv_stashpvn(tmpbuf, len, FALSE)) {
	    s = skipspace(s);
	    if ((bufend - s) >= 2 && *s == '=' && *(s+1) == '>')
		return 0;	/* no assumptions -- "=>" quotes bearword */
	    nextval[nexttoke].opval =
		(OP*)newSVOP(OP_CONST, 0,
			    newSVpv(tmpbuf,0));
	    nextval[nexttoke].opval->op_private =
		OPpCONST_BARE;
	    expect = XTERM;
	    force_next(WORD);
	    bufptr = s;
	    return *s == '(' ? FUNCMETH : METHOD;
	}
    }
    return 0;
}

static char*
incl_perldb()
{
    if (perldb) {
	char *pdb = getenv("PERL5DB");

	if (pdb)
	    return pdb;
	return "BEGIN { require 'perl5db.pl' }";
    }
    return "";
}


/* Encoded script support. filter_add() effectively inserts a
 * 'pre-processing' function into the current source input stream. 
 * Note that the filter function only applies to the current source file
 * (e.g., it will not affect files 'require'd or 'use'd by this one).
 *
 * The datasv parameter (which may be NULL) can be used to pass
 * private data to this instance of the filter. The filter function
 * can recover the SV using the FILTER_DATA macro and use it to
 * store private buffers and state information.
 *
 * The supplied datasv parameter is upgraded to a PVIO type
 * and the IoDIRP field is used to store the function pointer.
 * Note that IoTOP_NAME, IoFMT_NAME, IoBOTTOM_NAME, if set for
 * private use must be set using malloc'd pointers.
 */
static int filter_debug = 0;

SV *
filter_add(funcp, datasv)
    filter_t funcp;
    SV *datasv;
{
    if (!funcp){ /* temporary handy debugging hack to be deleted */
	filter_debug = atoi((char*)datasv);
	return NULL;
    }
    if (!rsfp_filters)
	rsfp_filters = newAV();
    if (!datasv)
	datasv = newSV(0);
    if (!SvUPGRADE(datasv, SVt_PVIO))
        die("Can't upgrade filter_add data to SVt_PVIO");
    IoDIRP(datasv) = (DIR*)funcp; /* stash funcp into spare field */
    if (filter_debug)
	warn("filter_add func %p (%s)", funcp, SvPV(datasv,na));
    av_unshift(rsfp_filters, 1);
    av_store(rsfp_filters, 0, datasv) ;
    return(datasv);
}
 

/* Delete most recently added instance of this filter function.	*/
void
filter_del(funcp)
    filter_t funcp;
{
    if (filter_debug)
	warn("filter_del func %p", funcp);
    if (!rsfp_filters || AvFILL(rsfp_filters)<0)
	return;
    /* if filter is on top of stack (usual case) just pop it off */
    if (IoDIRP(FILTER_DATA(0)) == (void*)funcp){
	/* sv_free(av_pop(rsfp_filters)); */
	sv_free(av_shift(rsfp_filters));

        return;
    }
    /* we need to search for the correct entry and clear it	*/
    die("filter_del can only delete in reverse order (currently)");
}


/* Invoke the n'th filter function for the current rsfp.	 */
I32
filter_read(idx, buf_sv, maxlen)
    int idx;
    SV *buf_sv;
    int maxlen;		/* 0 = read one text line */
{
    filter_t funcp;
    SV *datasv = NULL;

    if (!rsfp_filters)
	return -1;
    if (idx > AvFILL(rsfp_filters)){       /* Any more filters?	*/
	/* Provide a default input filter to make life easy.	*/
	/* Note that we append to the line. This is handy.	*/
	if (filter_debug)
	    warn("filter_read %d: from rsfp\n", idx);
	if (maxlen) { 
 	    /* Want a block */
	    int len ;
	    int old_len = SvCUR(buf_sv) ;

	    /* ensure buf_sv is large enough */
	    SvGROW(buf_sv, old_len + maxlen) ;
	    if ((len = PerlIO_read(rsfp, SvPVX(buf_sv) + old_len, maxlen)) <= 0){
		if (PerlIO_error(rsfp))
	            return -1;		/* error */
	        else
		    return 0 ;		/* end of file */
	    }
	    SvCUR_set(buf_sv, old_len + len) ;
	} else {
	    /* Want a line */
            if (sv_gets(buf_sv, rsfp, SvCUR(buf_sv)) == NULL) {
		if (PerlIO_error(rsfp))
	            return -1;		/* error */
	        else
		    return 0 ;		/* end of file */
	    }
	}
	return SvCUR(buf_sv);
    }
    /* Skip this filter slot if filter has been deleted	*/
    if ( (datasv = FILTER_DATA(idx)) == &sv_undef){
	if (filter_debug)
	    warn("filter_read %d: skipped (filter deleted)\n", idx);
	return FILTER_READ(idx+1, buf_sv, maxlen); /* recurse */
    }
    /* Get function pointer hidden within datasv	*/
    funcp = (filter_t)IoDIRP(datasv);
    if (filter_debug)
	warn("filter_read %d: via function %p (%s)\n",
		idx, funcp, SvPV(datasv,na));
    /* Call function. The function is expected to 	*/
    /* call "FILTER_READ(idx+1, buf_sv)" first.		*/
    /* Return: <0:error, =0:eof, >0:not eof 		*/
    return (*funcp)(idx, buf_sv, maxlen);
}

static char *
filter_gets(sv,fp, append)
register SV *sv;
register PerlIO *fp;
STRLEN append;
{
    if (rsfp_filters) {

	if (!append)
            SvCUR_set(sv, 0);	/* start with empty line	*/
        if (FILTER_READ(0, sv, 0) > 0)
            return ( SvPVX(sv) ) ;
        else
	    return Nullch ;
    }
    else 
        return (sv_gets(sv, fp, append));
    
}


#ifdef DEBUGGING
    static char* exp_name[] =
	{ "OPERATOR", "TERM", "REF", "STATE", "BLOCK", "TERMBLOCK" };
#endif

EXT int yychar;		/* last token */

int
yylex()
{
    register char *s;
    register char *d;
    register I32 tmp;
    STRLEN len;

    if (pending_ident) {
	char pit = pending_ident;
	pending_ident = 0;

	if (in_my) {
	    if (strchr(tokenbuf,':'))
		croak(no_myglob,tokenbuf);
	    yylval.opval = newOP(OP_PADANY, 0);
	    yylval.opval->op_targ = pad_allocmy(tokenbuf);
	    return PRIVATEREF;
	}

	if (!strchr(tokenbuf,':') && (tmp = pad_findmy(tokenbuf))) {
	    if (last_lop_op == OP_SORT &&
		tokenbuf[0] == '$' &&
		(tokenbuf[1] == 'a' || tokenbuf[1] == 'b')
		&& !tokenbuf[2])
	    {
		for (d = in_eval ? oldoldbufptr : linestart;
		     d < bufend && *d != '\n';
		     d++)
		{
		    if (strnEQ(d,"<=>",3) || strnEQ(d,"cmp",3)) {
			croak("Can't use \"my %s\" in sort comparison",
			      tokenbuf);
		    }
		}
	    }

	    yylval.opval = newOP(OP_PADANY, 0);
	    yylval.opval->op_targ = tmp;
	    return PRIVATEREF;
	}

	/* Force them to make up their mind on "@foo". */
	if (pit == '@' && lex_state != LEX_NORMAL && !lex_brackets) {
	    GV *gv = gv_fetchpv(tokenbuf+1, FALSE, SVt_PVAV);
	    if (!gv || ((tokenbuf[0] == '@') ? !GvAV(gv) : !GvHV(gv)))
		yyerror(form("In string, %s now must be written as \\%s",
			     tokenbuf, tokenbuf));
	}

	yylval.opval = (OP*)newSVOP(OP_CONST, 0, newSVpv(tokenbuf+1, 0));
	yylval.opval->op_private = OPpCONST_ENTERED;
	gv_fetchpv(tokenbuf+1, in_eval ? GV_ADDMULTI : TRUE,
		   ((tokenbuf[0] == '$') ? SVt_PV
		    : (tokenbuf[0] == '@') ? SVt_PVAV
		    : SVt_PVHV));
	return WORD;
    }

    switch (lex_state) {
#ifdef COMMENTARY
    case LEX_NORMAL:		/* Some compilers will produce faster */
    case LEX_INTERPNORMAL:	/* code if we comment these out. */
	break;
#endif

    case LEX_KNOWNEXT:
	nexttoke--;
	yylval = nextval[nexttoke];
	if (!nexttoke) {
	    lex_state = lex_defer;
	    expect = lex_expect;
	    lex_defer = LEX_NORMAL;
	}
	return(nexttype[nexttoke]);

    case LEX_INTERPCASEMOD:
#ifdef DEBUGGING
	if (bufptr != bufend && *bufptr != '\\')
	    croak("panic: INTERPCASEMOD");
#endif
	if (bufptr == bufend || bufptr[1] == 'E') {
	    char oldmod;
	    if (lex_casemods) {
		oldmod = lex_casestack[--lex_casemods];
		lex_casestack[lex_casemods] = '\0';
		if (bufptr != bufend && strchr("LUQ", oldmod)) {
		    bufptr += 2;
		    lex_state = LEX_INTERPCONCAT;
		}
		return ')';
	    }
	    if (bufptr != bufend)
		bufptr += 2;
	    lex_state = LEX_INTERPCONCAT;
	    return yylex();
	}
	else {
	    s = bufptr + 1;
	    if (strnEQ(s, "L\\u", 3) || strnEQ(s, "U\\l", 3))
		tmp = *s, *s = s[2], s[2] = tmp;	/* misordered... */
	    if (strchr("LU", *s) &&
		(strchr(lex_casestack, 'L') || strchr(lex_casestack, 'U')))
	    {
		lex_casestack[--lex_casemods] = '\0';
		return ')';
	    }
	    if (lex_casemods > 10) {
		char* newlb = Renew(lex_casestack, lex_casemods + 2, char);
		if (newlb != lex_casestack) {
		    SAVEFREEPV(newlb);
		    lex_casestack = newlb;
		}
	    }
	    lex_casestack[lex_casemods++] = *s;
	    lex_casestack[lex_casemods] = '\0';
	    lex_state = LEX_INTERPCONCAT;
	    nextval[nexttoke].ival = 0;
	    force_next('(');
	    if (*s == 'l')
		nextval[nexttoke].ival = OP_LCFIRST;
	    else if (*s == 'u')
		nextval[nexttoke].ival = OP_UCFIRST;
	    else if (*s == 'L')
		nextval[nexttoke].ival = OP_LC;
	    else if (*s == 'U')
		nextval[nexttoke].ival = OP_UC;
	    else if (*s == 'Q')
		nextval[nexttoke].ival = OP_QUOTEMETA;
	    else
		croak("panic: yylex");
	    bufptr = s + 1;
	    force_next(FUNC);
	    if (lex_starts) {
		s = bufptr;
		lex_starts = 0;
		Aop(OP_CONCAT);
	    }
	    else
		return yylex();
	}

    case LEX_INTERPPUSH:
        return sublex_push();

    case LEX_INTERPSTART:
	if (bufptr == bufend)
	    return sublex_done();
	expect = XTERM;
	lex_dojoin = (*bufptr == '@');
	lex_state = LEX_INTERPNORMAL;
	if (lex_dojoin) {
	    nextval[nexttoke].ival = 0;
	    force_next(',');
	    force_ident("\"", '$');
	    nextval[nexttoke].ival = 0;
	    force_next('$');
	    nextval[nexttoke].ival = 0;
	    force_next('(');
	    nextval[nexttoke].ival = OP_JOIN;	/* emulate join($", ...) */
	    force_next(FUNC);
	}
	if (lex_starts++) {
	    s = bufptr;
	    Aop(OP_CONCAT);
	}
	return yylex();

    case LEX_INTERPENDMAYBE:
	if (intuit_more(bufptr)) {
	    lex_state = LEX_INTERPNORMAL;	/* false alarm, more expr */
	    break;
	}
	/* FALL THROUGH */

    case LEX_INTERPEND:
	if (lex_dojoin) {
	    lex_dojoin = FALSE;
	    lex_state = LEX_INTERPCONCAT;
	    return ')';
	}
	/* FALLTHROUGH */
    case LEX_INTERPCONCAT:
#ifdef DEBUGGING
	if (lex_brackets)
	    croak("panic: INTERPCONCAT");
#endif
	if (bufptr == bufend)
	    return sublex_done();

	if (SvIVX(linestr) == '\'') {
	    SV *sv = newSVsv(linestr);
	    if (!lex_inpat)
		sv = q(sv);
	    yylval.opval = (OP*)newSVOP(OP_CONST, 0, sv);
	    s = bufend;
	}
	else {
	    s = scan_const(bufptr);
	    if (*s == '\\')
		lex_state = LEX_INTERPCASEMOD;
	    else
		lex_state = LEX_INTERPSTART;
	}

	if (s != bufptr) {
	    nextval[nexttoke] = yylval;
	    expect = XTERM;
	    force_next(THING);
	    if (lex_starts++)
		Aop(OP_CONCAT);
	    else {
		bufptr = s;
		return yylex();
	    }
	}

	return yylex();
    case LEX_FORMLINE:
	lex_state = LEX_NORMAL;
	s = scan_formline(bufptr);
	if (!lex_formbrack)
	    goto rightbracket;
	OPERATOR(';');
    }

    s = bufptr;
    oldoldbufptr = oldbufptr;
    oldbufptr = s;
    DEBUG_p( {
	PerlIO_printf(PerlIO_stderr(), "### Tokener expecting %s at %s\n", exp_name[expect], s);
    } )

  retry:
    switch (*s) {
    default:
	croak("Unrecognized character \\%03o", *s & 255);
    case 4:
    case 26:
	goto fake_eof;			/* emulate EOF on ^D or ^Z */
    case 0:
	if (!rsfp) {
	    last_uni = 0;
	    last_lop = 0;
	    if (lex_brackets)
		yyerror("Missing right bracket");
	    TOKEN(0);
	}
	if (s++ < bufend)
	    goto retry;			/* ignore stray nulls */
	last_uni = 0;
	last_lop = 0;
	if (!in_eval && !preambled) {
	    preambled = TRUE;
	    sv_setpv(linestr,incl_perldb());
	    if (SvCUR(linestr))
		sv_catpv(linestr,";");
	    if (preambleav){
		while(AvFILL(preambleav) >= 0) {
		    SV *tmpsv = av_shift(preambleav);
		    sv_catsv(linestr, tmpsv);
		    sv_catpv(linestr, ";");
		    sv_free(tmpsv);
		}
		sv_free((SV*)preambleav);
		preambleav = NULL;
	    }
	    if (minus_n || minus_p) {
		sv_catpv(linestr, "LINE: while (<>) {");
		if (minus_l)
		    sv_catpv(linestr,"chomp;");
		if (minus_a) {
		    GV* gv = gv_fetchpv("::F", TRUE, SVt_PVAV);
		    if (gv)
			GvIMPORTED_AV_on(gv);
		    if (minus_F) {
			if (strchr("/'\"", *splitstr)
			      && strchr(splitstr + 1, *splitstr))
			    sv_catpvf(linestr, "@F=split(%s);", splitstr);
			else {
			    char delim;
			    s = "'~#\200\1'"; /* surely one char is unused...*/
			    while (s[1] && strchr(splitstr, *s))  s++;
			    delim = *s;
			    sv_catpvf(linestr, "@F=split(%s%c",
				      "q" + (delim == '\''), delim);
			    for (s = splitstr; *s; s++) {
				if (*s == '\\')
				    sv_catpvn(linestr, "\\", 1);
				sv_catpvn(linestr, s, 1);
			    }
			    sv_catpvf(linestr, "%c);", delim);
			}
		    }
		    else
		        sv_catpv(linestr,"@F=split(' ');");
		}
	    }
	    sv_catpv(linestr, "\n");
	    oldoldbufptr = oldbufptr = s = linestart = SvPVX(linestr);
	    bufend = SvPVX(linestr) + SvCUR(linestr);
	    if (PERLDB_LINE && curstash != debstash) {
		SV *sv = NEWSV(85,0);

		sv_upgrade(sv, SVt_PVMG);
		sv_setsv(sv,linestr);
		av_store(GvAV(curcop->cop_filegv),(I32)curcop->cop_line,sv);
	    }
	    goto retry;
	}
	do {
	    if ((s = filter_gets(linestr, rsfp, 0)) == Nullch) {
	      fake_eof:
		if (rsfp) {
		    if (preprocess && !in_eval)
			(void)my_pclose(rsfp);
		    else if ((PerlIO *)rsfp == PerlIO_stdin())
			PerlIO_clearerr(rsfp);
		    else
			(void)PerlIO_close(rsfp);
		    if (e_fp == rsfp)
			e_fp = Nullfp;
		    rsfp = Nullfp;
		}
		if (!in_eval && (minus_n || minus_p)) {
		    sv_setpv(linestr,minus_p ? ";}continue{print" : "");
		    sv_catpv(linestr,";}");
		    oldoldbufptr = oldbufptr = s = linestart = SvPVX(linestr);
		    bufend = SvPVX(linestr) + SvCUR(linestr);
		    minus_n = minus_p = 0;
		    goto retry;
		}
		oldoldbufptr = oldbufptr = s = linestart = SvPVX(linestr);
		sv_setpv(linestr,"");
		TOKEN(';');	/* not infinite loop because rsfp is NULL now */
	    }
	    if (doextract) {
		if (*s == '#' && s[1] == '!' && instr(s,"perl"))
		    doextract = FALSE;

		/* Incest with pod. */
		if (*s == '=' && strnEQ(s, "=cut", 4)) {
		    sv_setpv(linestr, "");
		    oldoldbufptr = oldbufptr = s = linestart = SvPVX(linestr);
		    bufend = SvPVX(linestr) + SvCUR(linestr);
		    doextract = FALSE;
		}
	    }
	    incline(s);
	} while (doextract);
	oldoldbufptr = oldbufptr = bufptr = linestart = s;
	if (PERLDB_LINE && curstash != debstash) {
	    SV *sv = NEWSV(85,0);

	    sv_upgrade(sv, SVt_PVMG);
	    sv_setsv(sv,linestr);
	    av_store(GvAV(curcop->cop_filegv),(I32)curcop->cop_line,sv);
	}
	bufend = SvPVX(linestr) + SvCUR(linestr);
	if (curcop->cop_line == 1) {
	    while (s < bufend && isSPACE(*s))
		s++;
	    if (*s == ':' && s[1] != ':') /* for csh execing sh scripts */
		s++;
	    d = Nullch;
	    if (!in_eval) {
		if (*s == '#' && *(s+1) == '!')
		    d = s + 2;
#ifdef ALTERNATE_SHEBANG
		else {
		    static char as[] = ALTERNATE_SHEBANG;
		    if (*s == as[0] && strnEQ(s, as, sizeof(as) - 1))
			d = s + (sizeof(as) - 1);
		}
#endif /* ALTERNATE_SHEBANG */
	    }
	    if (d) {
		char *ipath;
		char *ipathend;

		while (isSPACE(*d))
		    d++;
		ipath = d;
		while (*d && !isSPACE(*d))
		    d++;
		ipathend = d;

#ifdef ARG_ZERO_IS_SCRIPT
		if (ipathend > ipath) {
		    /*
		     * HP-UX (at least) sets argv[0] to the script name,
		     * which makes $^X incorrect.  And Digital UNIX and Linux,
		     * at least, set argv[0] to the basename of the Perl
		     * interpreter. So, having found "#!", we'll set it right.
		     */
		    SV *x = GvSV(gv_fetchpv("\030", TRUE, SVt_PV));
		    assert(SvPOK(x) || SvGMAGICAL(x));
		    if (sv_eq(x, GvSV(curcop->cop_filegv))) {
			sv_setpvn(x, ipath, ipathend - ipath);
			SvSETMAGIC(x);
		    }
		    TAINT_NOT;	/* $^X is always tainted, but that's OK */
		}
#endif /* ARG_ZERO_IS_SCRIPT */

		/*
		 * Look for options.
		 */
		d = instr(s,"perl -");
		if (!d)
		    d = instr(s,"perl");
#ifdef ALTERNATE_SHEBANG
		/*
		 * If the ALTERNATE_SHEBANG on this system starts with a
		 * character that can be part of a Perl expression, then if
		 * we see it but not "perl", we're probably looking at the
		 * start of Perl code, not a request to hand off to some
		 * other interpreter.  Similarly, if "perl" is there, but
		 * not in the first 'word' of the line, we assume the line
		 * contains the start of the Perl program.
		 */
		if (d && *s != '#') {
		    char *c = ipath;
		    while (*c && !strchr("; \t\r\n\f\v#", *c))
			c++;
		    if (c < d)
			d = Nullch;	/* "perl" not in first word; ignore */
		    else
			*s = '#';	/* Don't try to parse shebang line */
		}
#endif /* ALTERNATE_SHEBANG */
		if (!d &&
		    *s == '#' &&
		    ipathend > ipath &&
		    !minus_c &&
		    !instr(s,"indir") &&
		    instr(origargv[0],"perl"))
		{
		    char **newargv;

		    *ipathend = '\0';
		    s = ipathend + 1;
		    while (s < bufend && isSPACE(*s))
			s++;
		    if (s < bufend) {
			Newz(899,newargv,origargc+3,char*);
			newargv[1] = s;
			while (s < bufend && !isSPACE(*s))
			    s++;
			*s = '\0';
			Copy(origargv+1, newargv+2, origargc+1, char*);
		    }
		    else
			newargv = origargv;
		    newargv[0] = ipath;
		    execv(ipath, newargv);
		    croak("Can't exec %s", ipath);
		}
		if (d) {
		    U32 oldpdb = perldb;
		    bool oldn = minus_n;
		    bool oldp = minus_p;

		    while (*d && !isSPACE(*d)) d++;
		    while (*d == ' ' || *d == '\t') d++;

		    if (*d++ == '-') {
			do {
			    if (*d == 'M' || *d == 'm') {
				char *m = d;
				while (*d && !isSPACE(*d)) d++;
				croak("Too late for \"-%.*s\" option",
				      (int)(d - m), m);
			    }
			    d = moreswitches(d);
			} while (d);
			if (PERLDB_LINE && !oldpdb ||
			    ( minus_n || minus_p ) && !(oldn || oldp) )
			      /* if we have already added "LINE: while (<>) {",
			         we must not do it again */
			{
			    sv_setpv(linestr, "");
			    oldoldbufptr = oldbufptr = s = linestart = SvPVX(linestr);
			    bufend = SvPVX(linestr) + SvCUR(linestr);
			    preambled = FALSE;
			    if (PERLDB_LINE)
				(void)gv_fetchfile(origfilename);
			    goto retry;
			}
		    }
		}
	    }
	}
	if (lex_formbrack && lex_brackets <= lex_formbrack) {
	    bufptr = s;
	    lex_state = LEX_FORMLINE;
	    return yylex();
	}
	goto retry;
    case '\r':
	warn("Illegal character \\%03o (carriage return)", '\r');
	croak(
      "(Maybe you didn't strip carriage returns after a network transfer?)\n");
    case ' ': case '\t': case '\f': case 013:
	s++;
	goto retry;
    case '#':
    case '\n':
	if (lex_state != LEX_NORMAL || (in_eval && !rsfp)) {
	    d = bufend;
	    while (s < d && *s != '\n')
		s++;
	    if (s < d)
		s++;
	    incline(s);
	    if (lex_formbrack && lex_brackets <= lex_formbrack) {
		bufptr = s;
		lex_state = LEX_FORMLINE;
		return yylex();
	    }
	}
	else {
	    *s = '\0';
	    bufend = s;
	}
	goto retry;
    case '-':
	if (s[1] && isALPHA(s[1]) && !isALNUM(s[2])) {
	    s++;
	    bufptr = s;
	    tmp = *s++;

	    while (s < bufend && (*s == ' ' || *s == '\t'))
		s++;

	    if (strnEQ(s,"=>",2)) {
		if (dowarn)
		    warn("Ambiguous use of -%c => resolved to \"-%c\" =>",
			(int)tmp, (int)tmp);
		s = force_word(bufptr,WORD,FALSE,FALSE,FALSE);
		OPERATOR('-');		/* unary minus */
	    }
	    last_uni = oldbufptr;
	    last_lop_op = OP_FTEREAD;	/* good enough */
	    switch (tmp) {
	    case 'r': FTST(OP_FTEREAD);
	    case 'w': FTST(OP_FTEWRITE);
	    case 'x': FTST(OP_FTEEXEC);
	    case 'o': FTST(OP_FTEOWNED);
	    case 'R': FTST(OP_FTRREAD);
	    case 'W': FTST(OP_FTRWRITE);
	    case 'X': FTST(OP_FTREXEC);
	    case 'O': FTST(OP_FTROWNED);
	    case 'e': FTST(OP_FTIS);
	    case 'z': FTST(OP_FTZERO);
	    case 's': FTST(OP_FTSIZE);
	    case 'f': FTST(OP_FTFILE);
	    case 'd': FTST(OP_FTDIR);
	    case 'l': FTST(OP_FTLINK);
	    case 'p': FTST(OP_FTPIPE);
	    case 'S': FTST(OP_FTSOCK);
	    case 'u': FTST(OP_FTSUID);
	    case 'g': FTST(OP_FTSGID);
	    case 'k': FTST(OP_FTSVTX);
	    case 'b': FTST(OP_FTBLK);
	    case 'c': FTST(OP_FTCHR);
	    case 't': FTST(OP_FTTTY);
	    case 'T': FTST(OP_FTTEXT);
	    case 'B': FTST(OP_FTBINARY);
	    case 'M': gv_fetchpv("\024",TRUE, SVt_PV); FTST(OP_FTMTIME);
	    case 'A': gv_fetchpv("\024",TRUE, SVt_PV); FTST(OP_FTATIME);
	    case 'C': gv_fetchpv("\024",TRUE, SVt_PV); FTST(OP_FTCTIME);
	    default:
		croak("Unrecognized file test: -%c", (int)tmp);
		break;
	    }
	}
	tmp = *s++;
	if (*s == tmp) {
	    s++;
	    if (expect == XOPERATOR)
		TERM(POSTDEC);
	    else
		OPERATOR(PREDEC);
	}
	else if (*s == '>') {
	    s++;
	    s = skipspace(s);
	    if (isIDFIRST(*s)) {
		s = force_word(s,METHOD,FALSE,TRUE,FALSE);
		TOKEN(ARROW);
	    }
	    else if (*s == '$')
		OPERATOR(ARROW);
	    else
		TERM(ARROW);
	}
	if (expect == XOPERATOR)
	    Aop(OP_SUBTRACT);
	else {
	    if (isSPACE(*s) || !isSPACE(*bufptr))
		check_uni();
	    OPERATOR('-');		/* unary minus */
	}

    case '+':
	tmp = *s++;
	if (*s == tmp) {
	    s++;
	    if (expect == XOPERATOR)
		TERM(POSTINC);
	    else
		OPERATOR(PREINC);
	}
	if (expect == XOPERATOR)
	    Aop(OP_ADD);
	else {
	    if (isSPACE(*s) || !isSPACE(*bufptr))
		check_uni();
	    OPERATOR('+');
	}

    case '*':
	if (expect != XOPERATOR) {
	    s = scan_ident(s, bufend, tokenbuf, sizeof tokenbuf, TRUE);
	    expect = XOPERATOR;
	    force_ident(tokenbuf, '*');
	    if (!*tokenbuf)
		PREREF('*');
	    TERM('*');
	}
	s++;
	if (*s == '*') {
	    s++;
	    PWop(OP_POW);
	}
	Mop(OP_MULTIPLY);

    case '%':
	if (expect == XOPERATOR) {
	    ++s;
	    Mop(OP_MODULO);
	}
	tokenbuf[0] = '%';
	s = scan_ident(s, bufend, tokenbuf + 1, sizeof tokenbuf - 1, TRUE);
	if (!tokenbuf[1]) {
	    if (s == bufend)
		yyerror("Final % should be \\% or %name");
	    PREREF('%');
	}
	pending_ident = '%';
	TERM('%');

    case '^':
	s++;
	BOop(OP_BIT_XOR);
    case '[':
	lex_brackets++;
	/* FALL THROUGH */
    case '~':
    case ',':
	tmp = *s++;
	OPERATOR(tmp);
    case ':':
	if (s[1] == ':') {
	    len = 0;
	    goto just_a_word;
	}
	s++;
	OPERATOR(':');
    case '(':
	s++;
	if (last_lop == oldoldbufptr || last_uni == oldoldbufptr)
	    oldbufptr = oldoldbufptr;		/* allow print(STDOUT 123) */
	else
	    expect = XTERM;
	TOKEN('(');
    case ';':
	if (curcop->cop_line < copline)
	    copline = curcop->cop_line;
	tmp = *s++;
	OPERATOR(tmp);
    case ')':
	tmp = *s++;
	s = skipspace(s);
	if (*s == '{')
	    PREBLOCK(tmp);
	TERM(tmp);
    case ']':
	s++;
	if (lex_brackets <= 0)
	    yyerror("Unmatched right bracket");
	else
	    --lex_brackets;
	if (lex_state == LEX_INTERPNORMAL) {
	    if (lex_brackets == 0) {
		if (*s != '[' && *s != '{' && (*s != '-' || s[1] != '>'))
		    lex_state = LEX_INTERPEND;
	    }
	}
	TERM(']');
    case '{':
      leftbracket:
	s++;
	if (lex_brackets > 100) {
	    char* newlb = Renew(lex_brackstack, lex_brackets + 1, char);
	    if (newlb != lex_brackstack) {
		SAVEFREEPV(newlb);
		lex_brackstack = newlb;
	    }
	}
	switch (expect) {
	case XTERM:
	    if (lex_formbrack) {
		s--;
		PRETERMBLOCK(DO);
	    }
	    if (oldoldbufptr == last_lop)
		lex_brackstack[lex_brackets++] = XTERM;
	    else
		lex_brackstack[lex_brackets++] = XOPERATOR;
	    OPERATOR(HASHBRACK);
	case XOPERATOR:
	    while (s < bufend && (*s == ' ' || *s == '\t'))
		s++;
	    d = s;
	    tokenbuf[0] = '\0';
	    if (d < bufend && *d == '-') {
		tokenbuf[0] = '-';
		d++;
		while (d < bufend && (*d == ' ' || *d == '\t'))
		    d++;
	    }
	    if (d < bufend && isIDFIRST(*d)) {
		d = scan_word(d, tokenbuf + 1, sizeof tokenbuf - 1,
			      FALSE, &len);
		while (d < bufend && (*d == ' ' || *d == '\t'))
		    d++;
		if (*d == '}') {
		    char minus = (tokenbuf[0] == '-');
		    if (dowarn &&
			(keyword(tokenbuf + 1, len) ||
			 (minus && len == 1 && isALPHA(tokenbuf[1])) ||
			 perl_get_cv(tokenbuf + 1, FALSE) ))
			warn("Ambiguous use of {%s} resolved to {\"%s\"}",
			     tokenbuf + !minus, tokenbuf + !minus);
		    s = force_word(s + minus, WORD, FALSE, TRUE, FALSE);
		    if (minus)
			force_next('-');
		}
	    }
	    /* FALL THROUGH */
	case XBLOCK:
	    lex_brackstack[lex_brackets++] = XSTATE;
	    expect = XSTATE;
	    break;
	case XTERMBLOCK:
	    lex_brackstack[lex_brackets++] = XOPERATOR;
	    expect = XSTATE;
	    break;
	default: {
		char *t;
		if (oldoldbufptr == last_lop)
		    lex_brackstack[lex_brackets++] = XTERM;
		else
		    lex_brackstack[lex_brackets++] = XOPERATOR;
		s = skipspace(s);
		if (*s == '}')
		    OPERATOR(HASHBRACK);
		/* This hack serves to disambiguate a pair of curlies
		 * as being a block or an anon hash.  Normally, expectation
		 * determines that, but in cases where we're not in a
		 * position to expect anything in particular (like inside
		 * eval"") we have to resolve the ambiguity.  This code
		 * covers the case where the first term in the curlies is a
		 * quoted string.  Most other cases need to be explicitly
		 * disambiguated by prepending a `+' before the opening
		 * curly in order to force resolution as an anon hash.
		 *
		 * XXX should probably propagate the outer expectation
		 * into eval"" to rely less on this hack, but that could
		 * potentially break current behavior of eval"".
		 * GSAR 97-07-21
		 */
		t = s;
		if (*s == '\'' || *s == '"' || *s == '`') {
		    /* common case: get past first string, handling escapes */
		    for (t++; t < bufend && *t != *s;)
			if (*t++ == '\\' && (*t == '\\' || *t == *s))
			    t++;
		    t++;
		}
		else if (*s == 'q') {
		    if (++t < bufend
			&& (!isALNUM(*t)
			    || ((*t == 'q' || *t == 'x') && ++t < bufend
				&& !isALNUM(*t)))) {
			char *tmps;
			char open, close, term;
			I32 brackets = 1;

			while (t < bufend && isSPACE(*t))
			    t++;
			term = *t;
			open = term;
			if (term && (tmps = strchr("([{< )]}> )]}>",term)))
			    term = tmps[5];
			close = term;
			if (open == close)
			    for (t++; t < bufend; t++) {
				if (*t == '\\' && t+1 < bufend && open != '\\')
				    t++;
				else if (*t == open)
				    break;
			    }
			else
			    for (t++; t < bufend; t++) {
				if (*t == '\\' && t+1 < bufend)
				    t++;
				else if (*t == close && --brackets <= 0)
				    break;
				else if (*t == open)
				    brackets++;
			    }
		    }
		    t++;
		}
		else if (isALPHA(*s)) {
		    for (t++; t < bufend && isALNUM(*t); t++) ;
		}
		while (t < bufend && isSPACE(*t))
		    t++;
		/* if comma follows first term, call it an anon hash */
		/* XXX it could be a comma expression with loop modifiers */
		if (t < bufend && ((*t == ',' && (*s == 'q' || !isLOWER(*s)))
				   || (*t == '=' && t[1] == '>')))
		    OPERATOR(HASHBRACK);
		if (expect == XREF)
		    expect = XTERM;
		else {
		    lex_brackstack[lex_brackets-1] = XSTATE;
		    expect = XSTATE;
		}
	    }
	    break;
	}
	yylval.ival = curcop->cop_line;
	if (isSPACE(*s) || *s == '#')
	    copline = NOLINE;   /* invalidate current command line number */
	TOKEN('{');
    case '}':
      rightbracket:
	s++;
	if (lex_brackets <= 0)
	    yyerror("Unmatched right bracket");
	else
	    expect = (expectation)lex_brackstack[--lex_brackets];
	if (lex_brackets < lex_formbrack)
	    lex_formbrack = 0;
	if (lex_state == LEX_INTERPNORMAL) {
	    if (lex_brackets == 0) {
		if (lex_fakebrack) {
		    lex_state = LEX_INTERPEND;
		    bufptr = s;
		    return yylex();		/* ignore fake brackets */
		}
		if (*s == '-' && s[1] == '>')
		    lex_state = LEX_INTERPENDMAYBE;
		else if (*s != '[' && *s != '{')
		    lex_state = LEX_INTERPEND;
	    }
	}
	if (lex_brackets < lex_fakebrack) {
	    bufptr = s;
	    lex_fakebrack = 0;
	    return yylex();		/* ignore fake brackets */
	}
	force_next('}');
	TOKEN(';');
    case '&':
	s++;
	tmp = *s++;
	if (tmp == '&')
	    AOPERATOR(ANDAND);
	s--;
	if (expect == XOPERATOR) {
	    if (dowarn && isALPHA(*s) && bufptr == linestart) {
		curcop->cop_line--;
		warn(warn_nosemi);
		curcop->cop_line++;
	    }
	    BAop(OP_BIT_AND);
	}

	s = scan_ident(s - 1, bufend, tokenbuf, sizeof tokenbuf, TRUE);
	if (*tokenbuf) {
	    expect = XOPERATOR;
	    force_ident(tokenbuf, '&');
	}
	else
	    PREREF('&');
	yylval.ival = (OPpENTERSUB_AMPER<<8);
	TERM('&');

    case '|':
	s++;
	tmp = *s++;
	if (tmp == '|')
	    AOPERATOR(OROR);
	s--;
	BOop(OP_BIT_OR);
    case '=':
	s++;
	tmp = *s++;
	if (tmp == '=')
	    Eop(OP_EQ);
	if (tmp == '>')
	    OPERATOR(',');
	if (tmp == '~')
	    PMop(OP_MATCH);
	if (dowarn && tmp && isSPACE(*s) && strchr("+-*/%.^&|<",tmp))
	    warn("Reversed %c= operator",(int)tmp);
	s--;
	if (expect == XSTATE && isALPHA(tmp) &&
		(s == linestart+1 || s[-2] == '\n') )
	{
	    if (in_eval && !rsfp) {
		d = bufend;
		while (s < d) {
		    if (*s++ == '\n') {
			incline(s);
			if (strnEQ(s,"=cut",4)) {
			    s = strchr(s,'\n');
			    if (s)
				s++;
			    else
				s = d;
			    incline(s);
			    goto retry;
			}
		    }
		}
		goto retry;
	    }
	    s = bufend;
	    doextract = TRUE;
	    goto retry;
	}
	if (lex_brackets < lex_formbrack) {
	    char *t;
	    for (t = s; *t == ' ' || *t == '\t'; t++) ;
	    if (*t == '\n' || *t == '#') {
		s--;
		expect = XBLOCK;
		goto leftbracket;
	    }
	}
	yylval.ival = 0;
	OPERATOR(ASSIGNOP);
    case '!':
	s++;
	tmp = *s++;
	if (tmp == '=')
	    Eop(OP_NE);
	if (tmp == '~')
	    PMop(OP_NOT);
	s--;
	OPERATOR('!');
    case '<':
	if (expect != XOPERATOR) {
	    if (s[1] != '<' && !strchr(s,'>'))
		check_uni();
	    if (s[1] == '<')
		s = scan_heredoc(s);
	    else
		s = scan_inputsymbol(s);
	    TERM(sublex_start());
	}
	s++;
	tmp = *s++;
	if (tmp == '<')
	    SHop(OP_LEFT_SHIFT);
	if (tmp == '=') {
	    tmp = *s++;
	    if (tmp == '>')
		Eop(OP_NCMP);
	    s--;
	    Rop(OP_LE);
	}
	s--;
	Rop(OP_LT);
    case '>':
	s++;
	tmp = *s++;
	if (tmp == '>')
	    SHop(OP_RIGHT_SHIFT);
	if (tmp == '=')
	    Rop(OP_GE);
	s--;
	Rop(OP_GT);

    case '$':
	CLINE;

	if (expect == XOPERATOR) {
	    if (lex_formbrack && lex_brackets == lex_formbrack) {
		expect = XTERM;
		depcom();
		return ','; /* grandfather non-comma-format format */
	    }
	}

	if (s[1] == '#' && (isALPHA(s[2]) || strchr("_{$:", s[2]))) {
	    if (expect == XOPERATOR)
		no_op("Array length", bufptr);
	    tokenbuf[0] = '@';
	    s = scan_ident(s + 1, bufend, tokenbuf + 1, sizeof tokenbuf - 1,
			   FALSE);
	    if (!tokenbuf[1])
		PREREF(DOLSHARP);
	    expect = XOPERATOR;
	    pending_ident = '#';
	    TOKEN(DOLSHARP);
	}

	if (expect == XOPERATOR)
	    no_op("Scalar", bufptr);
	tokenbuf[0] = '$';
	s = scan_ident(s, bufend, tokenbuf + 1, sizeof tokenbuf - 1, FALSE);
	if (!tokenbuf[1]) {
	    if (s == bufend)
		yyerror("Final $ should be \\$ or $name");
	    PREREF('$');
	}

	/* This kludge not intended to be bulletproof. */
	if (tokenbuf[1] == '[' && !tokenbuf[2]) {
	    yylval.opval = newSVOP(OP_CONST, 0,
				   newSViv((IV)compiling.cop_arybase));
	    yylval.opval->op_private = OPpCONST_ARYBASE;
	    TERM(THING);
	}

	d = s;
	if (lex_state == LEX_NORMAL)
	    s = skipspace(s);

	if ((expect != XREF || oldoldbufptr == last_lop) && intuit_more(s)) {
	    char *t;
	    if (*s == '[') {
		tokenbuf[0] = '@';
		if (dowarn) {
		    for(t = s + 1;
			isSPACE(*t) || isALNUM(*t) || *t == '$';
			t++) ;
		    if (*t++ == ',') {
			bufptr = skipspace(bufptr);
			while (t < bufend && *t != ']')
			    t++;
			warn("Multidimensional syntax %.*s not supported",
			     (t - bufptr) + 1, bufptr);
		    }
		}
	    }
	    else if (*s == '{') {
		tokenbuf[0] = '%';
		if (dowarn && strEQ(tokenbuf+1, "SIG") &&
		    (t = strchr(s, '}')) && (t = strchr(t, '=')))
		{
		    char tmpbuf[sizeof tokenbuf];
		    STRLEN len;
		    for (t++; isSPACE(*t); t++) ;
		    if (isIDFIRST(*t)) {
			t = scan_word(t, tmpbuf, sizeof tmpbuf, TRUE, &len);
			if (*t != '(' && perl_get_cv(tmpbuf, FALSE))
			    warn("You need to quote \"%s\"", tmpbuf);
		    }
		}
	    }
	}

	expect = XOPERATOR;
	if (lex_state == LEX_NORMAL && isSPACE(*d)) {
	    bool islop = (last_lop == oldoldbufptr);
	    if (!islop || last_lop_op == OP_GREPSTART)
		expect = XOPERATOR;
	    else if (strchr("$@\"'`q", *s))
		expect = XTERM;		/* e.g. print $fh "foo" */
	    else if (strchr("&*<%", *s) && isIDFIRST(s[1]))
		expect = XTERM;		/* e.g. print $fh &sub */
	    else if (isIDFIRST(*s)) {
		char tmpbuf[sizeof tokenbuf];
		scan_word(s, tmpbuf, sizeof tmpbuf, TRUE, &len);
		if (tmp = keyword(tmpbuf, len)) {
		    /* binary operators exclude handle interpretations */
		    switch (tmp) {
		    case -KEY_x:
		    case -KEY_eq:
		    case -KEY_ne:
		    case -KEY_gt:
		    case -KEY_lt:
		    case -KEY_ge:
		    case -KEY_le:
		    case -KEY_cmp:
			break;
		    default:
			expect = XTERM;	/* e.g. print $fh length() */
			break;
		    }
		}
		else {
		    GV *gv = gv_fetchpv(tmpbuf, FALSE, SVt_PVCV);
		    if (gv && GvCVu(gv))
			expect = XTERM;	/* e.g. print $fh subr() */
		}
	    }
	    else if (isDIGIT(*s))
		expect = XTERM;		/* e.g. print $fh 3 */
	    else if (*s == '.' && isDIGIT(s[1]))
		expect = XTERM;		/* e.g. print $fh .3 */
	    else if (strchr("/?-+", *s) && !isSPACE(s[1]))
		expect = XTERM;		/* e.g. print $fh -1 */
	    else if (*s == '<' && s[1] == '<' && !isSPACE(s[2]))
		expect = XTERM;		/* print $fh <<"EOF" */
	}
	pending_ident = '$';
	TOKEN('$');

    case '@':
	if (expect == XOPERATOR)
	    no_op("Array", s);
	tokenbuf[0] = '@';
	s = scan_ident(s, bufend, tokenbuf + 1, sizeof tokenbuf - 1, FALSE);
	if (!tokenbuf[1]) {
	    if (s == bufend)
		yyerror("Final @ should be \\@ or @name");
	    PREREF('@');
	}
	if (lex_state == LEX_NORMAL)
	    s = skipspace(s);
	if ((expect != XREF || oldoldbufptr == last_lop) && intuit_more(s)) {
	    if (*s == '{')
		tokenbuf[0] = '%';

	    /* Warn about @ where they meant $. */
	    if (dowarn) {
		if (*s == '[' || *s == '{') {
		    char *t = s + 1;
		    while (*t && (isALNUM(*t) || strchr(" \t$#+-'\"", *t)))
			t++;
		    if (*t == '}' || *t == ']') {
			t++;
			bufptr = skipspace(bufptr);
			warn("Scalar value %.*s better written as $%.*s",
			    t-bufptr, bufptr, t-bufptr-1, bufptr+1);
		    }
		}
	    }
	}
	pending_ident = '@';
	TERM('@');

    case '/':			/* may either be division or pattern */
    case '?':			/* may either be conditional or pattern */
	if (expect != XOPERATOR) {
	    check_uni();
	    s = scan_pat(s);
	    TERM(sublex_start());
	}
	tmp = *s++;
	if (tmp == '/')
	    Mop(OP_DIVIDE);
	OPERATOR(tmp);

    case '.':
	if (lex_formbrack && lex_brackets == lex_formbrack && s[1] == '\n' &&
		(s == linestart || s[-1] == '\n') ) {
	    lex_formbrack = 0;
	    expect = XSTATE;
	    goto rightbracket;
	}
	if (expect == XOPERATOR || !isDIGIT(s[1])) {
	    tmp = *s++;
	    if (*s == tmp) {
		s++;
		if (*s == tmp) {
		    s++;
		    yylval.ival = OPf_SPECIAL;
		}
		else
		    yylval.ival = 0;
		OPERATOR(DOTDOT);
	    }
	    if (expect != XOPERATOR)
		check_uni();
	    Aop(OP_CONCAT);
	}
	/* FALL THROUGH */
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
	s = scan_num(s);
	if (expect == XOPERATOR)
	    no_op("Number",s);
	TERM(THING);

    case '\'':
	s = scan_str(s);
	if (expect == XOPERATOR) {
	    if (lex_formbrack && lex_brackets == lex_formbrack) {
		expect = XTERM;
		depcom();
		return ',';	/* grandfather non-comma-format format */
	    }
	    else
		no_op("String",s);
	}
	if (!s)
	    missingterm((char*)0);
	yylval.ival = OP_CONST;
	TERM(sublex_start());

    case '"':
	s = scan_str(s);
	if (expect == XOPERATOR) {
	    if (lex_formbrack && lex_brackets == lex_formbrack) {
		expect = XTERM;
		depcom();
		return ',';	/* grandfather non-comma-format format */
	    }
	    else
		no_op("String",s);
	}
	if (!s)
	    missingterm((char*)0);
	yylval.ival = OP_CONST;
	for (d = SvPV(lex_stuff, len); len; len--, d++) {
	    if (*d == '$' || *d == '@' || *d == '\\') {
		yylval.ival = OP_STRINGIFY;
		break;
	    }
	}
	TERM(sublex_start());

    case '`':
	s = scan_str(s);
	if (expect == XOPERATOR)
	    no_op("Backticks",s);
	if (!s)
	    missingterm((char*)0);
	yylval.ival = OP_BACKTICK;
	set_csh();
	TERM(sublex_start());

    case '\\':
	s++;
	if (dowarn && lex_inwhat && isDIGIT(*s))
	    warn("Can't use \\%c to mean $%c in expression", *s, *s);
	if (expect == XOPERATOR)
	    no_op("Backslash",s);
	OPERATOR(REFGEN);

    case 'x':
	if (isDIGIT(s[1]) && expect == XOPERATOR) {
	    s++;
	    Mop(OP_REPEAT);
	}
	goto keylookup;

    case '_':
    case 'a': case 'A':
    case 'b': case 'B':
    case 'c': case 'C':
    case 'd': case 'D':
    case 'e': case 'E':
    case 'f': case 'F':
    case 'g': case 'G':
    case 'h': case 'H':
    case 'i': case 'I':
    case 'j': case 'J':
    case 'k': case 'K':
    case 'l': case 'L':
    case 'm': case 'M':
    case 'n': case 'N':
    case 'o': case 'O':
    case 'p': case 'P':
    case 'q': case 'Q':
    case 'r': case 'R':
    case 's': case 'S':
    case 't': case 'T':
    case 'u': case 'U':
    case 'v': case 'V':
    case 'w': case 'W':
	      case 'X':
    case 'y': case 'Y':
    case 'z': case 'Z':

      keylookup:
	bufptr = s;
	s = scan_word(s, tokenbuf, sizeof tokenbuf, FALSE, &len);

	/* Some keywords can be followed by any delimiter, including ':' */
	tmp = (len == 1 && strchr("msyq", tokenbuf[0]) ||
	       len == 2 && ((tokenbuf[0] == 't' && tokenbuf[1] == 'r') ||
			    (tokenbuf[0] == 'q' &&
			     strchr("qwx", tokenbuf[1]))));

	/* x::* is just a word, unless x is "CORE" */
	if (!tmp && *s == ':' && s[1] == ':' && strNE(tokenbuf, "CORE"))
	    goto just_a_word;

	d = s;
	while (d < bufend && isSPACE(*d))
		d++;	/* no comments skipped here, or s### is misparsed */

	/* Is this a label? */
	if (!tmp && expect == XSTATE
	      && d < bufend && *d == ':' && *(d + 1) != ':') {
	    s = d + 1;
	    yylval.pval = savepv(tokenbuf);
	    CLINE;
	    TOKEN(LABEL);
	}

	/* Check for keywords */
	tmp = keyword(tokenbuf, len);

	/* Is this a word before a => operator? */
	if (strnEQ(d,"=>",2)) {
	    CLINE;
	    if (dowarn && (tmp || perl_get_cv(tokenbuf, FALSE)))
		warn("Ambiguous use of %s => resolved to \"%s\" =>",
			tokenbuf, tokenbuf);
	    yylval.opval = (OP*)newSVOP(OP_CONST, 0, newSVpv(tokenbuf,0));
	    yylval.opval->op_private = OPpCONST_BARE;
	    TERM(WORD);
	}

	if (tmp < 0) {			/* second-class keyword? */
	    GV* gv;
	    if (expect != XOPERATOR &&
		(*s != ':' || s[1] != ':') &&
		(gv = gv_fetchpv(tokenbuf, FALSE, SVt_PVCV)) &&
		GvIMPORTED_CV(gv))
	    {
		tmp = 0;
	    }
	    else
		tmp = -tmp;
	}

      reserved_word:
	switch (tmp) {

	default:			/* not a keyword */
	  just_a_word: {
		GV *gv;
		SV *sv;
		char lastchar = (bufptr == oldoldbufptr ? 0 : bufptr[-1]);

		/* Get the rest if it looks like a package qualifier */

		if (*s == '\'' || *s == ':' && s[1] == ':') {
		    s = scan_word(s, tokenbuf + len, sizeof tokenbuf - len,
				  TRUE, &len);
		    if (!len)
			croak("Bad name after %s::", tokenbuf);
		}

		if (expect == XOPERATOR) {
		    if (bufptr == linestart) {
			curcop->cop_line--;
			warn(warn_nosemi);
			curcop->cop_line++;
		    }
		    else
			no_op("Bareword",s);
		}

		/* Look for a subroutine with this name in current package. */

		gv = gv_fetchpv(tokenbuf,FALSE, SVt_PVCV);

		/* Presume this is going to be a bareword of some sort. */

		CLINE;
		yylval.opval = (OP*)newSVOP(OP_CONST, 0, newSVpv(tokenbuf,0));
		yylval.opval->op_private = OPpCONST_BARE;

		/* See if it's the indirect object for a list operator. */

		if (oldoldbufptr &&
		    oldoldbufptr < bufptr &&
		    (oldoldbufptr == last_lop || oldoldbufptr == last_uni) &&
		    /* NO SKIPSPACE BEFORE HERE! */
		    (expect == XREF ||
		     (opargs[last_lop_op] >> OASHIFT & 7) == OA_FILEREF) )
		{
		    bool immediate_paren = *s == '(';

		    /* (Now we can afford to cross potential line boundary.) */
		    s = skipspace(s);

		    /* Two barewords in a row may indicate method call. */

		    if ((isALPHA(*s) || *s == '$') && (tmp=intuit_method(s,gv)))
			return tmp;

		    /* If not a declared subroutine, it's an indirect object. */
		    /* (But it's an indir obj regardless for sort.) */

		    if ((last_lop_op == OP_SORT ||
                         (!immediate_paren && (!gv || !GvCVu(gv))) ) &&
                        (last_lop_op != OP_MAPSTART && last_lop_op != OP_GREPSTART)){
			expect = (last_lop == oldoldbufptr) ? XTERM : XOPERATOR;
			goto bareword;
		    }
		}

		/* If followed by a paren, it's certainly a subroutine. */

		expect = XOPERATOR;
		s = skipspace(s);
		if (*s == '(') {
		    CLINE;
		    if (gv && GvCVu(gv)) {
			for (d = s + 1; *d == ' ' || *d == '\t'; d++) ;
			if (*d == ')' && (sv = cv_const_sv(GvCV(gv)))) {
			    s = d + 1;
			    goto its_constant;
			}
		    }
		    nextval[nexttoke].opval = yylval.opval;
		    expect = XOPERATOR;
		    force_next(WORD);
		    yylval.ival = 0;
		    TOKEN('&');
		}

		/* If followed by var or block, call it a method (unless sub) */

		if ((*s == '$' || *s == '{') && (!gv || !GvCVu(gv))) {
		    last_lop = oldbufptr;
		    last_lop_op = OP_METHOD;
		    PREBLOCK(METHOD);
		}

		/* If followed by a bareword, see if it looks like indir obj. */

		if ((isALPHA(*s) || *s == '$') && (tmp = intuit_method(s,gv)))
		    return tmp;

		/* Not a method, so call it a subroutine (if defined) */

		if (gv && GvCVu(gv)) {
		    CV* cv;
		    if (lastchar == '-')
			warn("Ambiguous use of -%s resolved as -&%s()",
				tokenbuf, tokenbuf);
		    last_lop = oldbufptr;
		    last_lop_op = OP_ENTERSUB;
		    /* Check for a constant sub */
		    cv = GvCV(gv);
		    if ((sv = cv_const_sv(cv))) {
		  its_constant:
			SvREFCNT_dec(((SVOP*)yylval.opval)->op_sv);
			((SVOP*)yylval.opval)->op_sv = SvREFCNT_inc(sv);
			yylval.opval->op_private = 0;
			TOKEN(WORD);
		    }

		    /* Resolve to GV now. */
		    op_free(yylval.opval);
		    yylval.opval = newCVREF(0, newGVOP(OP_GV, 0, gv));
		    /* Is there a prototype? */
		    if (SvPOK(cv)) {
			STRLEN len;
			char *proto = SvPV((SV*)cv, len);
			if (!len)
			    TERM(FUNC0SUB);
			if (strEQ(proto, "$"))
			    OPERATOR(UNIOPSUB);
			if (*proto == '&' && *s == '{') {
			    sv_setpv(subname,"__ANON__");
			    PREBLOCK(LSTOPSUB);
			}
		    }
		    nextval[nexttoke].opval = yylval.opval;
		    expect = XTERM;
		    force_next(WORD);
		    TOKEN(NOAMP);
		}

		if (hints & HINT_STRICT_SUBS &&
		    lastchar != '-' &&
		    strnNE(s,"->",2) &&
		    last_lop_op != OP_TRUNCATE &&  /* S/F prototype in opcode.pl */
		    last_lop_op != OP_ACCEPT &&
		    last_lop_op != OP_PIPE_OP &&
		    last_lop_op != OP_SOCKPAIR)
		{
		    warn(
		     "Bareword \"%s\" not allowed while \"strict subs\" in use",
			tokenbuf);
		    ++error_count;
		}

		/* Call it a bare word */

	    bareword:
		if (dowarn) {
		    if (lastchar != '-') {
			for (d = tokenbuf; *d && isLOWER(*d); d++) ;
			if (!*d)
			    warn(warn_reserved, tokenbuf);
		    }
		}
		if (lastchar && strchr("*%&", lastchar)) {
		    warn("Operator or semicolon missing before %c%s",
			lastchar, tokenbuf);
		    warn("Ambiguous use of %c resolved as operator %c",
			lastchar, lastchar);
		}
		TOKEN(WORD);
	    }

	case KEY___FILE__:
	    yylval.opval = (OP*)newSVOP(OP_CONST, 0,
					newSVsv(GvSV(curcop->cop_filegv)));
	    TERM(THING);

	case KEY___LINE__:
	    yylval.opval = (OP*)newSVOP(OP_CONST, 0,
				    newSVpvf("%ld", (long)curcop->cop_line));
	    TERM(THING);

	case KEY___PACKAGE__:
	    yylval.opval = (OP*)newSVOP(OP_CONST, 0,
					(curstash
					 ? newSVsv(curstname)
					 : &sv_undef));
	    TERM(THING);

	case KEY___DATA__:
	case KEY___END__: {
	    GV *gv;

	    /*SUPPRESS 560*/
	    if (rsfp && (!in_eval || tokenbuf[2] == 'D')) {
		char *pname = "main";
		if (tokenbuf[2] == 'D')
		    pname = HvNAME(curstash ? curstash : defstash);
		gv = gv_fetchpv(form("%s::DATA", pname), TRUE, SVt_PVIO);
		GvMULTI_on(gv);
		if (!GvIO(gv))
		    GvIOp(gv) = newIO();
		IoIFP(GvIOp(gv)) = rsfp;
#if defined(HAS_FCNTL) && defined(F_SETFD)
		{
		    int fd = PerlIO_fileno(rsfp);
		    fcntl(fd,F_SETFD,fd >= 3);
		}
#endif
		/* Mark this internal pseudo-handle as clean */
		IoFLAGS(GvIOp(gv)) |= IOf_UNTAINT;
		if (preprocess)
		    IoTYPE(GvIOp(gv)) = '|';
		else if ((PerlIO*)rsfp == PerlIO_stdin())
		    IoTYPE(GvIOp(gv)) = '-';
		else
		    IoTYPE(GvIOp(gv)) = '<';
		rsfp = Nullfp;
	    }
	    goto fake_eof;
	}

	case KEY_AUTOLOAD:
	case KEY_DESTROY:
	case KEY_BEGIN:
	case KEY_END:
	    if (expect == XSTATE) {
		s = bufptr;
		goto really_sub;
	    }
	    goto just_a_word;

	case KEY_CORE:
	    if (*s == ':' && s[1] == ':') {
		s += 2;
		d = s;
		s = scan_word(s, tokenbuf, sizeof tokenbuf, FALSE, &len);
		tmp = keyword(tokenbuf, len);
		if (tmp < 0)
		    tmp = -tmp;
		goto reserved_word;
	    }
	    goto just_a_word;

	case KEY_abs:
	    UNI(OP_ABS);

	case KEY_alarm:
	    UNI(OP_ALARM);

	case KEY_accept:
	    LOP(OP_ACCEPT,XTERM);

	case KEY_and:
	    OPERATOR(ANDOP);

	case KEY_atan2:
	    LOP(OP_ATAN2,XTERM);

	case KEY_bind:
	    LOP(OP_BIND,XTERM);

	case KEY_binmode:
	    UNI(OP_BINMODE);

	case KEY_bless:
	    LOP(OP_BLESS,XTERM);

	case KEY_chop:
	    UNI(OP_CHOP);

	case KEY_continue:
	    PREBLOCK(CONTINUE);

	case KEY_chdir:
	    (void)gv_fetchpv("ENV",TRUE, SVt_PVHV);	/* may use HOME */
	    UNI(OP_CHDIR);

	case KEY_close:
	    UNI(OP_CLOSE);

	case KEY_closedir:
	    UNI(OP_CLOSEDIR);

	case KEY_cmp:
	    Eop(OP_SCMP);

	case KEY_caller:
	    UNI(OP_CALLER);

	case KEY_crypt:
#ifdef FCRYPT
	    if (!cryptseen++)
		init_des();
#endif
	    LOP(OP_CRYPT,XTERM);

	case KEY_chmod:
	    if (dowarn) {
		for (d = s; d < bufend && (isSPACE(*d) || *d == '('); d++) ;
		if (*d != '0' && isDIGIT(*d))
		    yywarn("chmod: mode argument is missing initial 0");
	    }
	    LOP(OP_CHMOD,XTERM);

	case KEY_chown:
	    LOP(OP_CHOWN,XTERM);

	case KEY_connect:
	    LOP(OP_CONNECT,XTERM);

	case KEY_chr:
	    UNI(OP_CHR);

	case KEY_cos:
	    UNI(OP_COS);

	case KEY_chroot:
	    UNI(OP_CHROOT);

	case KEY_do:
	    s = skipspace(s);
	    if (*s == '{')
		PRETERMBLOCK(DO);
	    if (*s != '\'')
		s = force_word(s,WORD,FALSE,TRUE,FALSE);
	    OPERATOR(DO);

	case KEY_die:
	    hints |= HINT_BLOCK_SCOPE;
	    LOP(OP_DIE,XTERM);

	case KEY_defined:
	    UNI(OP_DEFINED);

	case KEY_delete:
	    UNI(OP_DELETE);

	case KEY_dbmopen:
	    gv_fetchpv("AnyDBM_File::ISA", GV_ADDMULTI, SVt_PVAV);
	    LOP(OP_DBMOPEN,XTERM);

	case KEY_dbmclose:
	    UNI(OP_DBMCLOSE);

	case KEY_dump:
	    s = force_word(s,WORD,TRUE,FALSE,FALSE);
	    LOOPX(OP_DUMP);

	case KEY_else:
	    PREBLOCK(ELSE);

	case KEY_elsif:
	    yylval.ival = curcop->cop_line;
	    OPERATOR(ELSIF);

	case KEY_eq:
	    Eop(OP_SEQ);

	case KEY_exists:
	    UNI(OP_EXISTS);
	    
	case KEY_exit:
	    UNI(OP_EXIT);

	case KEY_eval:
	    s = skipspace(s);
	    expect = (*s == '{') ? XTERMBLOCK : XTERM;
	    UNIBRACK(OP_ENTEREVAL);

	case KEY_eof:
	    UNI(OP_EOF);

	case KEY_exp:
	    UNI(OP_EXP);

	case KEY_each:
	    UNI(OP_EACH);

	case KEY_exec:
	    set_csh();
	    LOP(OP_EXEC,XREF);

	case KEY_endhostent:
	    FUN0(OP_EHOSTENT);

	case KEY_endnetent:
	    FUN0(OP_ENETENT);

	case KEY_endservent:
	    FUN0(OP_ESERVENT);

	case KEY_endprotoent:
	    FUN0(OP_EPROTOENT);

	case KEY_endpwent:
	    FUN0(OP_EPWENT);

	case KEY_endgrent:
	    FUN0(OP_EGRENT);

	case KEY_for:
	case KEY_foreach:
	    yylval.ival = curcop->cop_line;
	    s = skipspace(s);
	    if (isIDFIRST(*s)) {
		char *p = s;
		if ((bufend - p) >= 3 &&
		    strnEQ(p, "my", 2) && isSPACE(*(p + 2)))
		    p += 2;
		p = skipspace(p);
		if (isIDFIRST(*p))
		    croak("Missing $ on loop variable");
	    }
	    OPERATOR(FOR);

	case KEY_formline:
	    LOP(OP_FORMLINE,XTERM);

	case KEY_fork:
	    FUN0(OP_FORK);

	case KEY_fcntl:
	    LOP(OP_FCNTL,XTERM);

	case KEY_fileno:
	    UNI(OP_FILENO);

	case KEY_flock:
	    LOP(OP_FLOCK,XTERM);

	case KEY_gt:
	    Rop(OP_SGT);

	case KEY_ge:
	    Rop(OP_SGE);

	case KEY_grep:
	    LOP(OP_GREPSTART, *s == '(' ? XTERM : XREF);

	case KEY_goto:
	    s = force_word(s,WORD,TRUE,FALSE,FALSE);
	    LOOPX(OP_GOTO);

	case KEY_gmtime:
	    UNI(OP_GMTIME);

	case KEY_getc:
	    UNI(OP_GETC);

	case KEY_getppid:
	    FUN0(OP_GETPPID);

	case KEY_getpgrp:
	    UNI(OP_GETPGRP);

	case KEY_getpriority:
	    LOP(OP_GETPRIORITY,XTERM);

	case KEY_getprotobyname:
	    UNI(OP_GPBYNAME);

	case KEY_getprotobynumber:
	    LOP(OP_GPBYNUMBER,XTERM);

	case KEY_getprotoent:
	    FUN0(OP_GPROTOENT);

	case KEY_getpwent:
	    FUN0(OP_GPWENT);

	case KEY_getpwnam:
	    UNI(OP_GPWNAM);

	case KEY_getpwuid:
	    UNI(OP_GPWUID);

	case KEY_getpeername:
	    UNI(OP_GETPEERNAME);

	case KEY_gethostbyname:
	    UNI(OP_GHBYNAME);

	case KEY_gethostbyaddr:
	    LOP(OP_GHBYADDR,XTERM);

	case KEY_gethostent:
	    FUN0(OP_GHOSTENT);

	case KEY_getnetbyname:
	    UNI(OP_GNBYNAME);

	case KEY_getnetbyaddr:
	    LOP(OP_GNBYADDR,XTERM);

	case KEY_getnetent:
	    FUN0(OP_GNETENT);

	case KEY_getservbyname:
	    LOP(OP_GSBYNAME,XTERM);

	case KEY_getservbyport:
	    LOP(OP_GSBYPORT,XTERM);

	case KEY_getservent:
	    FUN0(OP_GSERVENT);

	case KEY_getsockname:
	    UNI(OP_GETSOCKNAME);

	case KEY_getsockopt:
	    LOP(OP_GSOCKOPT,XTERM);

	case KEY_getgrent:
	    FUN0(OP_GGRENT);

	case KEY_getgrnam:
	    UNI(OP_GGRNAM);

	case KEY_getgrgid:
	    UNI(OP_GGRGID);

	case KEY_getlogin:
	    FUN0(OP_GETLOGIN);

	case KEY_glob:
	    set_csh();
	    LOP(OP_GLOB,XTERM);

	case KEY_hex:
	    UNI(OP_HEX);

	case KEY_if:
	    yylval.ival = curcop->cop_line;
	    OPERATOR(IF);

	case KEY_index:
	    LOP(OP_INDEX,XTERM);

	case KEY_int:
	    UNI(OP_INT);

	case KEY_ioctl:
	    LOP(OP_IOCTL,XTERM);

	case KEY_join:
	    LOP(OP_JOIN,XTERM);

	case KEY_keys:
	    UNI(OP_KEYS);

	case KEY_kill:
	    LOP(OP_KILL,XTERM);

	case KEY_last:
	    s = force_word(s,WORD,TRUE,FALSE,FALSE);
	    LOOPX(OP_LAST);
	    
	case KEY_lc:
	    UNI(OP_LC);

	case KEY_lcfirst:
	    UNI(OP_LCFIRST);

	case KEY_local:
	    OPERATOR(LOCAL);

	case KEY_length:
	    UNI(OP_LENGTH);

	case KEY_lt:
	    Rop(OP_SLT);

	case KEY_le:
	    Rop(OP_SLE);

	case KEY_localtime:
	    UNI(OP_LOCALTIME);

	case KEY_log:
	    UNI(OP_LOG);

	case KEY_link:
	    LOP(OP_LINK,XTERM);

	case KEY_listen:
	    LOP(OP_LISTEN,XTERM);

	case KEY_lstat:
	    UNI(OP_LSTAT);

	case KEY_m:
	    s = scan_pat(s);
	    TERM(sublex_start());

	case KEY_map:
	    LOP(OP_MAPSTART,XREF);
	    
	case KEY_mkdir:
	    LOP(OP_MKDIR,XTERM);

	case KEY_msgctl:
	    LOP(OP_MSGCTL,XTERM);

	case KEY_msgget:
	    LOP(OP_MSGGET,XTERM);

	case KEY_msgrcv:
	    LOP(OP_MSGRCV,XTERM);

	case KEY_msgsnd:
	    LOP(OP_MSGSND,XTERM);

	case KEY_my:
	    in_my = TRUE;
	    OPERATOR(MY);

	case KEY_next:
	    s = force_word(s,WORD,TRUE,FALSE,FALSE);
	    LOOPX(OP_NEXT);

	case KEY_ne:
	    Eop(OP_SNE);

	case KEY_no:
	    if (expect != XSTATE)
		yyerror("\"no\" not allowed in expression");
	    s = force_word(s,WORD,FALSE,TRUE,FALSE);
	    s = force_version(s);
	    yylval.ival = 0;
	    OPERATOR(USE);

	case KEY_not:
	    OPERATOR(NOTOP);

	case KEY_open:
	    s = skipspace(s);
	    if (isIDFIRST(*s)) {
		char *t;
		for (d = s; isALNUM(*d); d++) ;
		t = skipspace(d);
		if (strchr("|&*+-=!?:.", *t))
		    warn("Precedence problem: open %.*s should be open(%.*s)",
			d-s,s, d-s,s);
	    }
	    LOP(OP_OPEN,XTERM);

	case KEY_or:
	    yylval.ival = OP_OR;
	    OPERATOR(OROP);

	case KEY_ord:
	    UNI(OP_ORD);

	case KEY_oct:
	    UNI(OP_OCT);

	case KEY_opendir:
	    LOP(OP_OPEN_DIR,XTERM);

	case KEY_print:
	    checkcomma(s,tokenbuf,"filehandle");
	    LOP(OP_PRINT,XREF);

	case KEY_printf:
	    checkcomma(s,tokenbuf,"filehandle");
	    LOP(OP_PRTF,XREF);

	case KEY_prototype:
	    UNI(OP_PROTOTYPE);

	case KEY_push:
	    LOP(OP_PUSH,XTERM);

	case KEY_pop:
	    UNI(OP_POP);

	case KEY_pos:
	    UNI(OP_POS);
	    
	case KEY_pack:
	    LOP(OP_PACK,XTERM);

	case KEY_package:
	    s = force_word(s,WORD,FALSE,TRUE,FALSE);
	    OPERATOR(PACKAGE);

	case KEY_pipe:
	    LOP(OP_PIPE_OP,XTERM);

	case KEY_q:
	    s = scan_str(s);
	    if (!s)
		missingterm((char*)0);
	    yylval.ival = OP_CONST;
	    TERM(sublex_start());

	case KEY_quotemeta:
	    UNI(OP_QUOTEMETA);

	case KEY_qw:
	    s = scan_str(s);
	    if (!s)
		missingterm((char*)0);
	    if (dowarn && SvLEN(lex_stuff)) {
		d = SvPV_force(lex_stuff, len);
		for (; len; --len, ++d) {
		    if (*d == ',') {
			warn("Possible attempt to separate words with commas");
			break;
		    }
		    if (*d == '#') {
			warn("Possible attempt to put comments in qw() list");
			break;
		    }
		}
	    }
	    force_next(')');
	    nextval[nexttoke].opval = (OP*)newSVOP(OP_CONST, 0, q(lex_stuff));
	    lex_stuff = Nullsv;
	    force_next(THING);
	    force_next(',');
	    nextval[nexttoke].opval = (OP*)newSVOP(OP_CONST, 0, newSVpv(" ",1));
	    force_next(THING);
	    force_next('(');
	    yylval.ival = OP_SPLIT;
	    CLINE;
	    expect = XTERM;
	    bufptr = s;
	    last_lop = oldbufptr;
	    last_lop_op = OP_SPLIT;
	    return FUNC;

	case KEY_qq:
	    s = scan_str(s);
	    if (!s)
		missingterm((char*)0);
	    yylval.ival = OP_STRINGIFY;
	    if (SvIVX(lex_stuff) == '\'')
		SvIVX(lex_stuff) = 0;	/* qq'$foo' should intepolate */
	    TERM(sublex_start());

	case KEY_qx:
	    s = scan_str(s);
	    if (!s)
		missingterm((char*)0);
	    yylval.ival = OP_BACKTICK;
	    set_csh();
	    TERM(sublex_start());

	case KEY_return:
	    OLDLOP(OP_RETURN);

	case KEY_require:
	    *tokenbuf = '\0';
	    s = force_word(s,WORD,TRUE,TRUE,FALSE);
	    if (isIDFIRST(*tokenbuf))
		gv_stashpvn(tokenbuf, strlen(tokenbuf), TRUE);
	    else if (*s == '<')
		yyerror("<> should be quotes");
	    UNI(OP_REQUIRE);

	case KEY_reset:
	    UNI(OP_RESET);

	case KEY_redo:
	    s = force_word(s,WORD,TRUE,FALSE,FALSE);
	    LOOPX(OP_REDO);

	case KEY_rename:
	    LOP(OP_RENAME,XTERM);

	case KEY_rand:
	    UNI(OP_RAND);

	case KEY_rmdir:
	    UNI(OP_RMDIR);

	case KEY_rindex:
	    LOP(OP_RINDEX,XTERM);

	case KEY_read:
	    LOP(OP_READ,XTERM);

	case KEY_readdir:
	    UNI(OP_READDIR);

	case KEY_readline:
	    set_csh();
	    UNI(OP_READLINE);

	case KEY_readpipe:
	    set_csh();
	    UNI(OP_BACKTICK);

	case KEY_rewinddir:
	    UNI(OP_REWINDDIR);

	case KEY_recv:
	    LOP(OP_RECV,XTERM);

	case KEY_reverse:
	    LOP(OP_REVERSE,XTERM);

	case KEY_readlink:
	    UNI(OP_READLINK);

	case KEY_ref:
	    UNI(OP_REF);

	case KEY_s:
	    s = scan_subst(s);
	    if (yylval.opval)
		TERM(sublex_start());
	    else
		TOKEN(1);	/* force error */

	case KEY_chomp:
	    UNI(OP_CHOMP);
	    
	case KEY_scalar:
	    UNI(OP_SCALAR);

	case KEY_select:
	    LOP(OP_SELECT,XTERM);

	case KEY_seek:
	    LOP(OP_SEEK,XTERM);

	case KEY_semctl:
	    LOP(OP_SEMCTL,XTERM);

	case KEY_semget:
	    LOP(OP_SEMGET,XTERM);

	case KEY_semop:
	    LOP(OP_SEMOP,XTERM);

	case KEY_send:
	    LOP(OP_SEND,XTERM);

	case KEY_setpgrp:
	    LOP(OP_SETPGRP,XTERM);

	case KEY_setpriority:
	    LOP(OP_SETPRIORITY,XTERM);

	case KEY_sethostent:
	    UNI(OP_SHOSTENT);

	case KEY_setnetent:
	    UNI(OP_SNETENT);

	case KEY_setservent:
	    UNI(OP_SSERVENT);

	case KEY_setprotoent:
	    UNI(OP_SPROTOENT);

	case KEY_setpwent:
	    FUN0(OP_SPWENT);

	case KEY_setgrent:
	    FUN0(OP_SGRENT);

	case KEY_seekdir:
	    LOP(OP_SEEKDIR,XTERM);

	case KEY_setsockopt:
	    LOP(OP_SSOCKOPT,XTERM);

	case KEY_shift:
	    UNI(OP_SHIFT);

	case KEY_shmctl:
	    LOP(OP_SHMCTL,XTERM);

	case KEY_shmget:
	    LOP(OP_SHMGET,XTERM);

	case KEY_shmread:
	    LOP(OP_SHMREAD,XTERM);

	case KEY_shmwrite:
	    LOP(OP_SHMWRITE,XTERM);

	case KEY_shutdown:
	    LOP(OP_SHUTDOWN,XTERM);

	case KEY_sin:
	    UNI(OP_SIN);

	case KEY_sleep:
	    UNI(OP_SLEEP);

	case KEY_socket:
	    LOP(OP_SOCKET,XTERM);

	case KEY_socketpair:
	    LOP(OP_SOCKPAIR,XTERM);

	case KEY_sort:
	    checkcomma(s,tokenbuf,"subroutine name");
	    s = skipspace(s);
	    if (*s == ';' || *s == ')')		/* probably a close */
		croak("sort is now a reserved word");
	    expect = XTERM;
	    s = force_word(s,WORD,TRUE,TRUE,TRUE);
	    LOP(OP_SORT,XREF);

	case KEY_split:
	    LOP(OP_SPLIT,XTERM);

	case KEY_sprintf:
	    LOP(OP_SPRINTF,XTERM);

	case KEY_splice:
	    LOP(OP_SPLICE,XTERM);

	case KEY_sqrt:
	    UNI(OP_SQRT);

	case KEY_srand:
	    UNI(OP_SRAND);

	case KEY_stat:
	    UNI(OP_STAT);

	case KEY_study:
	    sawstudy++;
	    UNI(OP_STUDY);

	case KEY_substr:
	    LOP(OP_SUBSTR,XTERM);

	case KEY_format:
	case KEY_sub:
	  really_sub:
	    s = skipspace(s);

	    if (isIDFIRST(*s) || *s == '\'' || *s == ':') {
		char tmpbuf[sizeof tokenbuf];
		expect = XBLOCK;
		d = scan_word(s, tmpbuf, sizeof tmpbuf, TRUE, &len);
		if (strchr(tmpbuf, ':'))
		    sv_setpv(subname, tmpbuf);
		else {
		    sv_setsv(subname,curstname);
		    sv_catpvn(subname,"::",2);
		    sv_catpvn(subname,tmpbuf,len);
		}
		s = force_word(s,WORD,FALSE,TRUE,TRUE);
		s = skipspace(s);
	    }
	    else {
		expect = XTERMBLOCK;
		sv_setpv(subname,"?");
	    }

	    if (tmp == KEY_format) {
		s = skipspace(s);
		if (*s == '=')
		    lex_formbrack = lex_brackets + 1;
		OPERATOR(FORMAT);
	    }

	    /* Look for a prototype */
	    if (*s == '(') {
		char *p;

		s = scan_str(s);
		if (!s) {
		    if (lex_stuff)
			SvREFCNT_dec(lex_stuff);
		    lex_stuff = Nullsv;
		    croak("Prototype not terminated");
		}
		/* strip spaces */
		d = SvPVX(lex_stuff);
		tmp = 0;
		for (p = d; *p; ++p) {
		    if (!isSPACE(*p))
			d[tmp++] = *p;
		}
		d[tmp] = '\0';
		SvCUR(lex_stuff) = tmp;

		nexttoke++;
		nextval[1] = nextval[0];
		nexttype[1] = nexttype[0];
		nextval[0].opval = (OP*)newSVOP(OP_CONST, 0, lex_stuff);
		nexttype[0] = THING;
		if (nexttoke == 1) {
		    lex_defer = lex_state;
		    lex_expect = expect;
		    lex_state = LEX_KNOWNEXT;
		}
		lex_stuff = Nullsv;
	    }

	    if (*SvPV(subname,na) == '?') {
		sv_setpv(subname,"__ANON__");
		TOKEN(ANONSUB);
	    }
	    PREBLOCK(SUB);

	case KEY_system:
	    set_csh();
	    LOP(OP_SYSTEM,XREF);

	case KEY_symlink:
	    LOP(OP_SYMLINK,XTERM);

	case KEY_syscall:
	    LOP(OP_SYSCALL,XTERM);

	case KEY_sysopen:
	    LOP(OP_SYSOPEN,XTERM);

	case KEY_sysseek:
	    LOP(OP_SYSSEEK,XTERM);

	case KEY_sysread:
	    LOP(OP_SYSREAD,XTERM);

	case KEY_syswrite:
	    LOP(OP_SYSWRITE,XTERM);

	case KEY_tr:
	    s = scan_trans(s);
	    TERM(sublex_start());

	case KEY_tell:
	    UNI(OP_TELL);

	case KEY_telldir:
	    UNI(OP_TELLDIR);

	case KEY_tie:
	    LOP(OP_TIE,XTERM);

	case KEY_tied:
	    UNI(OP_TIED);

	case KEY_time:
	    FUN0(OP_TIME);

	case KEY_times:
	    FUN0(OP_TMS);

	case KEY_truncate:
	    LOP(OP_TRUNCATE,XTERM);

	case KEY_uc:
	    UNI(OP_UC);

	case KEY_ucfirst:
	    UNI(OP_UCFIRST);

	case KEY_untie:
	    UNI(OP_UNTIE);

	case KEY_until:
	    yylval.ival = curcop->cop_line;
	    OPERATOR(UNTIL);

	case KEY_unless:
	    yylval.ival = curcop->cop_line;
	    OPERATOR(UNLESS);

	case KEY_unlink:
	    LOP(OP_UNLINK,XTERM);

	case KEY_undef:
	    UNI(OP_UNDEF);

	case KEY_unpack:
	    LOP(OP_UNPACK,XTERM);

	case KEY_utime:
	    LOP(OP_UTIME,XTERM);

	case KEY_umask:
	    if (dowarn) {
		for (d = s; d < bufend && (isSPACE(*d) || *d == '('); d++) ;
		if (*d != '0' && isDIGIT(*d))
		    yywarn("umask: argument is missing initial 0");
	    }
	    UNI(OP_UMASK);

	case KEY_unshift:
	    LOP(OP_UNSHIFT,XTERM);

	case KEY_use:
	    if (expect != XSTATE)
		yyerror("\"use\" not allowed in expression");
	    s = skipspace(s);
	    if(isDIGIT(*s)) {
		s = force_version(s);
		if(*s == ';' || (s = skipspace(s), *s == ';')) {
		    nextval[nexttoke].opval = Nullop;
		    force_next(WORD);
		}
	    }
	    else {
		s = force_word(s,WORD,FALSE,TRUE,FALSE);
		s = force_version(s);
	    }
	    yylval.ival = 1;
	    OPERATOR(USE);

	case KEY_values:
	    UNI(OP_VALUES);

	case KEY_vec:
	    sawvec = TRUE;
	    LOP(OP_VEC,XTERM);

	case KEY_while:
	    yylval.ival = curcop->cop_line;
	    OPERATOR(WHILE);

	case KEY_warn:
	    hints |= HINT_BLOCK_SCOPE;
	    LOP(OP_WARN,XTERM);

	case KEY_wait:
	    FUN0(OP_WAIT);

	case KEY_waitpid:
	    LOP(OP_WAITPID,XTERM);

	case KEY_wantarray:
	    FUN0(OP_WANTARRAY);

	case KEY_write:
	    gv_fetchpv("\f",TRUE, SVt_PV);	/* Make sure $^L is defined */
	    UNI(OP_ENTERWRITE);

	case KEY_x:
	    if (expect == XOPERATOR)
		Mop(OP_REPEAT);
	    check_uni();
	    goto just_a_word;

	case KEY_xor:
	    yylval.ival = OP_XOR;
	    OPERATOR(OROP);

	case KEY_y:
	    s = scan_trans(s);
	    TERM(sublex_start());
	}
    }
}

I32
keyword(d, len)
register char *d;
I32 len;
{
    switch (*d) {
    case '_':
	if (d[1] == '_') {
	    if (strEQ(d,"__FILE__"))		return -KEY___FILE__;
	    if (strEQ(d,"__LINE__"))		return -KEY___LINE__;
	    if (strEQ(d,"__PACKAGE__"))		return -KEY___PACKAGE__;
	    if (strEQ(d,"__DATA__"))		return KEY___DATA__;
	    if (strEQ(d,"__END__"))		return KEY___END__;
	}
	break;
    case 'A':
	if (strEQ(d,"AUTOLOAD"))		return KEY_AUTOLOAD;
	break;
    case 'a':
	switch (len) {
	case 3:
	    if (strEQ(d,"and"))			return -KEY_and;
	    if (strEQ(d,"abs"))			return -KEY_abs;
	    break;
	case 5:
	    if (strEQ(d,"alarm"))		return -KEY_alarm;
	    if (strEQ(d,"atan2"))		return -KEY_atan2;
	    break;
	case 6:
	    if (strEQ(d,"accept"))		return -KEY_accept;
	    break;
	}
	break;
    case 'B':
	if (strEQ(d,"BEGIN"))			return KEY_BEGIN;
	break;
    case 'b':
	if (strEQ(d,"bless"))			return -KEY_bless;
	if (strEQ(d,"bind"))			return -KEY_bind;
	if (strEQ(d,"binmode"))			return -KEY_binmode;
	break;
    case 'C':
	if (strEQ(d,"CORE"))			return -KEY_CORE;
	break;
    case 'c':
	switch (len) {
	case 3:
	    if (strEQ(d,"cmp"))			return -KEY_cmp;
	    if (strEQ(d,"chr"))			return -KEY_chr;
	    if (strEQ(d,"cos"))			return -KEY_cos;
	    break;
	case 4:
	    if (strEQ(d,"chop"))		return KEY_chop;
	    break;
	case 5:
	    if (strEQ(d,"close"))		return -KEY_close;
	    if (strEQ(d,"chdir"))		return -KEY_chdir;
	    if (strEQ(d,"chomp"))		return KEY_chomp;
	    if (strEQ(d,"chmod"))		return -KEY_chmod;
	    if (strEQ(d,"chown"))		return -KEY_chown;
	    if (strEQ(d,"crypt"))		return -KEY_crypt;
	    break;
	case 6:
	    if (strEQ(d,"chroot"))		return -KEY_chroot;
	    if (strEQ(d,"caller"))		return -KEY_caller;
	    break;
	case 7:
	    if (strEQ(d,"connect"))		return -KEY_connect;
	    break;
	case 8:
	    if (strEQ(d,"closedir"))		return -KEY_closedir;
	    if (strEQ(d,"continue"))		return -KEY_continue;
	    break;
	}
	break;
    case 'D':
	if (strEQ(d,"DESTROY"))			return KEY_DESTROY;
	break;
    case 'd':
	switch (len) {
	case 2:
	    if (strEQ(d,"do"))			return KEY_do;
	    break;
	case 3:
	    if (strEQ(d,"die"))			return -KEY_die;
	    break;
	case 4:
	    if (strEQ(d,"dump"))		return -KEY_dump;
	    break;
	case 6:
	    if (strEQ(d,"delete"))		return KEY_delete;
	    break;
	case 7:
	    if (strEQ(d,"defined"))		return KEY_defined;
	    if (strEQ(d,"dbmopen"))		return -KEY_dbmopen;
	    break;
	case 8:
	    if (strEQ(d,"dbmclose"))		return -KEY_dbmclose;
	    break;
	}
	break;
    case 'E':
	if (strEQ(d,"EQ")) { deprecate(d);	return -KEY_eq;}
	if (strEQ(d,"END"))			return KEY_END;
	break;
    case 'e':
	switch (len) {
	case 2:
	    if (strEQ(d,"eq"))			return -KEY_eq;
	    break;
	case 3:
	    if (strEQ(d,"eof"))			return -KEY_eof;
	    if (strEQ(d,"exp"))			return -KEY_exp;
	    break;
	case 4:
	    if (strEQ(d,"else"))		return KEY_else;
	    if (strEQ(d,"exit"))		return -KEY_exit;
	    if (strEQ(d,"eval"))		return KEY_eval;
	    if (strEQ(d,"exec"))		return -KEY_exec;
	    if (strEQ(d,"each"))		return KEY_each;
	    break;
	case 5:
	    if (strEQ(d,"elsif"))		return KEY_elsif;
	    break;
	case 6:
	    if (strEQ(d,"exists"))		return KEY_exists;
	    if (strEQ(d,"elseif")) warn("elseif should be elsif");
	    break;
	case 8:
	    if (strEQ(d,"endgrent"))		return -KEY_endgrent;
	    if (strEQ(d,"endpwent"))		return -KEY_endpwent;
	    break;
	case 9:
	    if (strEQ(d,"endnetent"))		return -KEY_endnetent;
	    break;
	case 10:
	    if (strEQ(d,"endhostent"))		return -KEY_endhostent;
	    if (strEQ(d,"endservent"))		return -KEY_endservent;
	    break;
	case 11:
	    if (strEQ(d,"endprotoent"))		return -KEY_endprotoent;
	    break;
	}
	break;
    case 'f':
	switch (len) {
	case 3:
	    if (strEQ(d,"for"))			return KEY_for;
	    break;
	case 4:
	    if (strEQ(d,"fork"))		return -KEY_fork;
	    break;
	case 5:
	    if (strEQ(d,"fcntl"))		return -KEY_fcntl;
	    if (strEQ(d,"flock"))		return -KEY_flock;
	    break;
	case 6:
	    if (strEQ(d,"format"))		return KEY_format;
	    if (strEQ(d,"fileno"))		return -KEY_fileno;
	    break;
	case 7:
	    if (strEQ(d,"foreach"))		return KEY_foreach;
	    break;
	case 8:
	    if (strEQ(d,"formline"))		return -KEY_formline;
	    break;
	}
	break;
    case 'G':
	if (len == 2) {
	    if (strEQ(d,"GT")) { deprecate(d);	return -KEY_gt;}
	    if (strEQ(d,"GE")) { deprecate(d);	return -KEY_ge;}
	}
	break;
    case 'g':
	if (strnEQ(d,"get",3)) {
	    d += 3;
	    if (*d == 'p') {
		switch (len) {
		case 7:
		    if (strEQ(d,"ppid"))	return -KEY_getppid;
		    if (strEQ(d,"pgrp"))	return -KEY_getpgrp;
		    break;
		case 8:
		    if (strEQ(d,"pwent"))	return -KEY_getpwent;
		    if (strEQ(d,"pwnam"))	return -KEY_getpwnam;
		    if (strEQ(d,"pwuid"))	return -KEY_getpwuid;
		    break;
		case 11:
		    if (strEQ(d,"peername"))	return -KEY_getpeername;
		    if (strEQ(d,"protoent"))	return -KEY_getprotoent;
		    if (strEQ(d,"priority"))	return -KEY_getpriority;
		    break;
		case 14:
		    if (strEQ(d,"protobyname"))	return -KEY_getprotobyname;
		    break;
		case 16:
		    if (strEQ(d,"protobynumber"))return -KEY_getprotobynumber;
		    break;
		}
	    }
	    else if (*d == 'h') {
		if (strEQ(d,"hostbyname"))	return -KEY_gethostbyname;
		if (strEQ(d,"hostbyaddr"))	return -KEY_gethostbyaddr;
		if (strEQ(d,"hostent"))		return -KEY_gethostent;
	    }
	    else if (*d == 'n') {
		if (strEQ(d,"netbyname"))	return -KEY_getnetbyname;
		if (strEQ(d,"netbyaddr"))	return -KEY_getnetbyaddr;
		if (strEQ(d,"netent"))		return -KEY_getnetent;
	    }
	    else if (*d == 's') {
		if (strEQ(d,"servbyname"))	return -KEY_getservbyname;
		if (strEQ(d,"servbyport"))	return -KEY_getservbyport;
		if (strEQ(d,"servent"))		return -KEY_getservent;
		if (strEQ(d,"sockname"))	return -KEY_getsockname;
		if (strEQ(d,"sockopt"))		return -KEY_getsockopt;
	    }
	    else if (*d == 'g') {
		if (strEQ(d,"grent"))		return -KEY_getgrent;
		if (strEQ(d,"grnam"))		return -KEY_getgrnam;
		if (strEQ(d,"grgid"))		return -KEY_getgrgid;
	    }
	    else if (*d == 'l') {
		if (strEQ(d,"login"))		return -KEY_getlogin;
	    }
	    else if (strEQ(d,"c"))		return -KEY_getc;
	    break;
	}
	switch (len) {
	case 2:
	    if (strEQ(d,"gt"))			return -KEY_gt;
	    if (strEQ(d,"ge"))			return -KEY_ge;
	    break;
	case 4:
	    if (strEQ(d,"grep"))		return KEY_grep;
	    if (strEQ(d,"goto"))		return KEY_goto;
	    if (strEQ(d,"glob"))		return KEY_glob;
	    break;
	case 6:
	    if (strEQ(d,"gmtime"))		return -KEY_gmtime;
	    break;
	}
	break;
    case 'h':
	if (strEQ(d,"hex"))			return -KEY_hex;
	break;
    case 'i':
	switch (len) {
	case 2:
	    if (strEQ(d,"if"))			return KEY_if;
	    break;
	case 3:
	    if (strEQ(d,"int"))			return -KEY_int;
	    break;
	case 5:
	    if (strEQ(d,"index"))		return -KEY_index;
	    if (strEQ(d,"ioctl"))		return -KEY_ioctl;
	    break;
	}
	break;
    case 'j':
	if (strEQ(d,"join"))			return -KEY_join;
	break;
    case 'k':
	if (len == 4) {
	    if (strEQ(d,"keys"))		return KEY_keys;
	    if (strEQ(d,"kill"))		return -KEY_kill;
	}
	break;
    case 'L':
	if (len == 2) {
	    if (strEQ(d,"LT")) { deprecate(d);	return -KEY_lt;}
	    if (strEQ(d,"LE")) { deprecate(d);	return -KEY_le;}
	}
	break;
    case 'l':
	switch (len) {
	case 2:
	    if (strEQ(d,"lt"))			return -KEY_lt;
	    if (strEQ(d,"le"))			return -KEY_le;
	    if (strEQ(d,"lc"))			return -KEY_lc;
	    break;
	case 3:
	    if (strEQ(d,"log"))			return -KEY_log;
	    break;
	case 4:
	    if (strEQ(d,"last"))		return KEY_last;
	    if (strEQ(d,"link"))		return -KEY_link;
	    break;
	case 5:
	    if (strEQ(d,"local"))		return KEY_local;
	    if (strEQ(d,"lstat"))		return -KEY_lstat;
	    break;
	case 6:
	    if (strEQ(d,"length"))		return -KEY_length;
	    if (strEQ(d,"listen"))		return -KEY_listen;
	    break;
	case 7:
	    if (strEQ(d,"lcfirst"))		return -KEY_lcfirst;
	    break;
	case 9:
	    if (strEQ(d,"localtime"))		return -KEY_localtime;
	    break;
	}
	break;
    case 'm':
	switch (len) {
	case 1:					return KEY_m;
	case 2:
	    if (strEQ(d,"my"))			return KEY_my;
	    break;
	case 3:
	    if (strEQ(d,"map"))			return KEY_map;
	    break;
	case 5:
	    if (strEQ(d,"mkdir"))		return -KEY_mkdir;
	    break;
	case 6:
	    if (strEQ(d,"msgctl"))		return -KEY_msgctl;
	    if (strEQ(d,"msgget"))		return -KEY_msgget;
	    if (strEQ(d,"msgrcv"))		return -KEY_msgrcv;
	    if (strEQ(d,"msgsnd"))		return -KEY_msgsnd;
	    break;
	}
	break;
    case 'N':
	if (strEQ(d,"NE")) { deprecate(d);	return -KEY_ne;}
	break;
    case 'n':
	if (strEQ(d,"next"))			return KEY_next;
	if (strEQ(d,"ne"))			return -KEY_ne;
	if (strEQ(d,"not"))			return -KEY_not;
	if (strEQ(d,"no"))			return KEY_no;
	break;
    case 'o':
	switch (len) {
	case 2:
	    if (strEQ(d,"or"))			return -KEY_or;
	    break;
	case 3:
	    if (strEQ(d,"ord"))			return -KEY_ord;
	    if (strEQ(d,"oct"))			return -KEY_oct;
	    break;
	case 4:
	    if (strEQ(d,"open"))		return -KEY_open;
	    break;
	case 7:
	    if (strEQ(d,"opendir"))		return -KEY_opendir;
	    break;
	}
	break;
    case 'p':
	switch (len) {
	case 3:
	    if (strEQ(d,"pop"))			return KEY_pop;
	    if (strEQ(d,"pos"))			return KEY_pos;
	    break;
	case 4:
	    if (strEQ(d,"push"))		return KEY_push;
	    if (strEQ(d,"pack"))		return -KEY_pack;
	    if (strEQ(d,"pipe"))		return -KEY_pipe;
	    break;
	case 5:
	    if (strEQ(d,"print"))		return KEY_print;
	    break;
	case 6:
	    if (strEQ(d,"printf"))		return KEY_printf;
	    break;
	case 7:
	    if (strEQ(d,"package"))		return KEY_package;
	    break;
	case 9:
	    if (strEQ(d,"prototype"))		return KEY_prototype;
	}
	break;
    case 'q':
	if (len <= 2) {
	    if (strEQ(d,"q"))			return KEY_q;
	    if (strEQ(d,"qq"))			return KEY_qq;
	    if (strEQ(d,"qw"))			return KEY_qw;
	    if (strEQ(d,"qx"))			return KEY_qx;
	}
	else if (strEQ(d,"quotemeta"))		return -KEY_quotemeta;
	break;
    case 'r':
	switch (len) {
	case 3:
	    if (strEQ(d,"ref"))			return -KEY_ref;
	    break;
	case 4:
	    if (strEQ(d,"read"))		return -KEY_read;
	    if (strEQ(d,"rand"))		return -KEY_rand;
	    if (strEQ(d,"recv"))		return -KEY_recv;
	    if (strEQ(d,"redo"))		return KEY_redo;
	    break;
	case 5:
	    if (strEQ(d,"rmdir"))		return -KEY_rmdir;
	    if (strEQ(d,"reset"))		return -KEY_reset;
	    break;
	case 6:
	    if (strEQ(d,"return"))		return KEY_return;
	    if (strEQ(d,"rename"))		return -KEY_rename;
	    if (strEQ(d,"rindex"))		return -KEY_rindex;
	    break;
	case 7:
	    if (strEQ(d,"require"))		return -KEY_require;
	    if (strEQ(d,"reverse"))		return -KEY_reverse;
	    if (strEQ(d,"readdir"))		return -KEY_readdir;
	    break;
	case 8:
	    if (strEQ(d,"readlink"))		return -KEY_readlink;
	    if (strEQ(d,"readline"))		return -KEY_readline;
	    if (strEQ(d,"readpipe"))		return -KEY_readpipe;
	    break;
	case 9:
	    if (strEQ(d,"rewinddir"))		return -KEY_rewinddir;
	    break;
	}
	break;
    case 's':
	switch (d[1]) {
	case 0:					return KEY_s;
	case 'c':
	    if (strEQ(d,"scalar"))		return KEY_scalar;
	    break;
	case 'e':
	    switch (len) {
	    case 4:
		if (strEQ(d,"seek"))		return -KEY_seek;
		if (strEQ(d,"send"))		return -KEY_send;
		break;
	    case 5:
		if (strEQ(d,"semop"))		return -KEY_semop;
		break;
	    case 6:
		if (strEQ(d,"select"))		return -KEY_select;
		if (strEQ(d,"semctl"))		return -KEY_semctl;
		if (strEQ(d,"semget"))		return -KEY_semget;
		break;
	    case 7:
		if (strEQ(d,"setpgrp"))		return -KEY_setpgrp;
		if (strEQ(d,"seekdir"))		return -KEY_seekdir;
		break;
	    case 8:
		if (strEQ(d,"setpwent"))	return -KEY_setpwent;
		if (strEQ(d,"setgrent"))	return -KEY_setgrent;
		break;
	    case 9:
		if (strEQ(d,"setnetent"))	return -KEY_setnetent;
		break;
	    case 10:
		if (strEQ(d,"setsockopt"))	return -KEY_setsockopt;
		if (strEQ(d,"sethostent"))	return -KEY_sethostent;
		if (strEQ(d,"setservent"))	return -KEY_setservent;
		break;
	    case 11:
		if (strEQ(d,"setpriority"))	return -KEY_setpriority;
		if (strEQ(d,"setprotoent"))	return -KEY_setprotoent;
		break;
	    }
	    break;
	case 'h':
	    switch (len) {
	    case 5:
		if (strEQ(d,"shift"))		return KEY_shift;
		break;
	    case 6:
		if (strEQ(d,"shmctl"))		return -KEY_shmctl;
		if (strEQ(d,"shmget"))		return -KEY_shmget;
		break;
	    case 7:
		if (strEQ(d,"shmread"))		return -KEY_shmread;
		break;
	    case 8:
		if (strEQ(d,"shmwrite"))	return -KEY_shmwrite;
		if (strEQ(d,"shutdown"))	return -KEY_shutdown;
		break;
	    }
	    break;
	case 'i':
	    if (strEQ(d,"sin"))			return -KEY_sin;
	    break;
	case 'l':
	    if (strEQ(d,"sleep"))		return -KEY_sleep;
	    break;
	case 'o':
	    if (strEQ(d,"sort"))		return KEY_sort;
	    if (strEQ(d,"socket"))		return -KEY_socket;
	    if (strEQ(d,"socketpair"))		return -KEY_socketpair;
	    break;
	case 'p':
	    if (strEQ(d,"split"))		return KEY_split;
	    if (strEQ(d,"sprintf"))		return -KEY_sprintf;
	    if (strEQ(d,"splice"))		return KEY_splice;
	    break;
	case 'q':
	    if (strEQ(d,"sqrt"))		return -KEY_sqrt;
	    break;
	case 'r':
	    if (strEQ(d,"srand"))		return -KEY_srand;
	    break;
	case 't':
	    if (strEQ(d,"stat"))		return -KEY_stat;
	    if (strEQ(d,"study"))		return KEY_study;
	    break;
	case 'u':
	    if (strEQ(d,"substr"))		return -KEY_substr;
	    if (strEQ(d,"sub"))			return KEY_sub;
	    break;
	case 'y':
	    switch (len) {
	    case 6:
		if (strEQ(d,"system"))		return -KEY_system;
		break;
	    case 7:
		if (strEQ(d,"symlink"))		return -KEY_symlink;
		if (strEQ(d,"syscall"))		return -KEY_syscall;
		if (strEQ(d,"sysopen"))		return -KEY_sysopen;
		if (strEQ(d,"sysread"))		return -KEY_sysread;
		if (strEQ(d,"sysseek"))		return -KEY_sysseek;
		break;
	    case 8:
		if (strEQ(d,"syswrite"))	return -KEY_syswrite;
		break;
	    }
	    break;
	}
	break;
    case 't':
	switch (len) {
	case 2:
	    if (strEQ(d,"tr"))			return KEY_tr;
	    break;
	case 3:
	    if (strEQ(d,"tie"))			return KEY_tie;
	    break;
	case 4:
	    if (strEQ(d,"tell"))		return -KEY_tell;
	    if (strEQ(d,"tied"))		return KEY_tied;
	    if (strEQ(d,"time"))		return -KEY_time;
	    break;
	case 5:
	    if (strEQ(d,"times"))		return -KEY_times;
	    break;
	case 7:
	    if (strEQ(d,"telldir"))		return -KEY_telldir;
	    break;
	case 8:
	    if (strEQ(d,"truncate"))		return -KEY_truncate;
	    break;
	}
	break;
    case 'u':
	switch (len) {
	case 2:
	    if (strEQ(d,"uc"))			return -KEY_uc;
	    break;
	case 3:
	    if (strEQ(d,"use"))			return KEY_use;
	    break;
	case 5:
	    if (strEQ(d,"undef"))		return KEY_undef;
	    if (strEQ(d,"until"))		return KEY_until;
	    if (strEQ(d,"untie"))		return KEY_untie;
	    if (strEQ(d,"utime"))		return -KEY_utime;
	    if (strEQ(d,"umask"))		return -KEY_umask;
	    break;
	case 6:
	    if (strEQ(d,"unless"))		return KEY_unless;
	    if (strEQ(d,"unpack"))		return -KEY_unpack;
	    if (strEQ(d,"unlink"))		return -KEY_unlink;
	    break;
	case 7:
	    if (strEQ(d,"unshift"))		return KEY_unshift;
	    if (strEQ(d,"ucfirst"))		return -KEY_ucfirst;
	    break;
	}
	break;
    case 'v':
	if (strEQ(d,"values"))			return -KEY_values;
	if (strEQ(d,"vec"))			return -KEY_vec;
	break;
    case 'w':
	switch (len) {
	case 4:
	    if (strEQ(d,"warn"))		return -KEY_warn;
	    if (strEQ(d,"wait"))		return -KEY_wait;
	    break;
	case 5:
	    if (strEQ(d,"while"))		return KEY_while;
	    if (strEQ(d,"write"))		return -KEY_write;
	    break;
	case 7:
	    if (strEQ(d,"waitpid"))		return -KEY_waitpid;
	    break;
	case 9:
	    if (strEQ(d,"wantarray"))		return -KEY_wantarray;
	    break;
	}
	break;
    case 'x':
	if (len == 1)				return -KEY_x;
	if (strEQ(d,"xor"))			return -KEY_xor;
	break;
    case 'y':
	if (len == 1)				return KEY_y;
	break;
    case 'z':
	break;
    }
    return 0;
}

static void
checkcomma(s,name,what)
register char *s;
char *name;
char *what;
{
    char *w;

    if (dowarn && *s == ' ' && s[1] == '(') {	/* XXX gotta be a better way */
	int level = 1;
	for (w = s+2; *w && level; w++) {
	    if (*w == '(')
		++level;
	    else if (*w == ')')
		--level;
	}
	if (*w)
	    for (; *w && isSPACE(*w); w++) ;
	if (!*w || !strchr(";|})]oaiuw!=", *w))	/* an advisory hack only... */
	    warn("%s (...) interpreted as function",name);
    }
    while (s < bufend && isSPACE(*s))
	s++;
    if (*s == '(')
	s++;
    while (s < bufend && isSPACE(*s))
	s++;
    if (isIDFIRST(*s)) {
	w = s++;
	while (isALNUM(*s))
	    s++;
	while (s < bufend && isSPACE(*s))
	    s++;
	if (*s == ',') {
	    int kw;
	    *s = '\0';
	    kw = keyword(w, s - w) || perl_get_cv(w, FALSE) != 0;
	    *s = ',';
	    if (kw)
		return;
	    croak("No comma allowed after %s", what);
	}
    }
}

static char *
scan_word(s, dest, destlen, allow_package, slp)
register char *s;
char *dest;
STRLEN destlen;
int allow_package;
STRLEN *slp;
{
    register char *d = dest;
    register char *e = d + destlen - 3;  /* two-character token, ending NUL */
    for (;;) {
	if (d >= e)
	    croak(ident_too_long);
	if (isALNUM(*s))
	    *d++ = *s++;
	else if (*s == '\'' && allow_package && isIDFIRST(s[1])) {
	    *d++ = ':';
	    *d++ = ':';
	    s++;
	}
	else if (*s == ':' && s[1] == ':' && allow_package && isIDFIRST(s[2])) {
	    *d++ = *s++;
	    *d++ = *s++;
	}
	else {
	    *d = '\0';
	    *slp = d - dest;
	    return s;
	}
    }
}

static char *
scan_ident(s, send, dest, destlen, ck_uni)
register char *s;
register char *send;
char *dest;
STRLEN destlen;
I32 ck_uni;
{
    register char *d;
    register char *e;
    char *bracket = 0;
    char funny = *s++;

    if (lex_brackets == 0)
	lex_fakebrack = 0;
    if (isSPACE(*s))
	s = skipspace(s);
    d = dest;
    e = d + destlen - 3;	/* two-character token, ending NUL */
    if (isDIGIT(*s)) {
	while (isDIGIT(*s)) {
	    if (d >= e)
		croak(ident_too_long);
	    *d++ = *s++;
	}
    }
    else {
	for (;;) {
	    if (d >= e)
		croak(ident_too_long);
	    if (isALNUM(*s))
		*d++ = *s++;
	    else if (*s == '\'' && isIDFIRST(s[1])) {
		*d++ = ':';
		*d++ = ':';
		s++;
	    }
	    else if (*s == ':' && s[1] == ':') {
		*d++ = *s++;
		*d++ = *s++;
	    }
	    else
		break;
	}
    }
    *d = '\0';
    d = dest;
    if (*d) {
	if (lex_state != LEX_NORMAL)
	    lex_state = LEX_INTERPENDMAYBE;
	return s;
    }
    if (*s == '$' && s[1] &&
      (isALNUM(s[1]) || strchr("${", s[1]) || strnEQ(s+1,"::",2)) )
    {
	if (isDIGIT(s[1]) && lex_state == LEX_INTERPNORMAL)
	    deprecate("\"$$<digit>\" to mean \"${$}<digit>\"");
	else
	    return s;
    }
    if (*s == '{') {
	bracket = s;
	s++;
    }
    else if (ck_uni)
	check_uni();
    if (s < send)
	*d = *s++;
    d[1] = '\0';
    if (*d == '^' && *s && (isUPPER(*s) || strchr("[\\]^_?", *s))) {
	*d = toCTRL(*s);
	s++;
    }
    if (bracket) {
	if (isSPACE(s[-1])) {
	    while (s < send) {
		char ch = *s++;
		if (ch != ' ' && ch != '\t') {
		    *d = ch;
		    break;
		}
	    }
	}
	if (isIDFIRST(*d)) {
	    d++;
	    while (isALNUM(*s) || *s == ':')
		*d++ = *s++;
	    *d = '\0';
	    while (s < send && (*s == ' ' || *s == '\t')) s++;
	    if ((*s == '[' || (*s == '{' && strNE(dest, "sub")))) {
		if (dowarn && keyword(dest, d - dest)) {
		    char *brack = *s == '[' ? "[...]" : "{...}";
		    warn("Ambiguous use of %c{%s%s} resolved to %c%s%s",
			funny, dest, brack, funny, dest, brack);
		}
		lex_fakebrack = lex_brackets+1;
		bracket++;
		lex_brackstack[lex_brackets++] = XOPERATOR;
		return s;
	    }
	}
	if (*s == '}') {
	    s++;
	    if (lex_state == LEX_INTERPNORMAL && !lex_brackets)
		lex_state = LEX_INTERPEND;
	    if (funny == '#')
		funny = '@';
	    if (dowarn && lex_state == LEX_NORMAL &&
	      (keyword(dest, d - dest) || perl_get_cv(dest, FALSE)))
		warn("Ambiguous use of %c{%s} resolved to %c%s",
		    funny, dest, funny, dest);
	}
	else {
	    s = bracket;		/* let the parser handle it */
	    *dest = '\0';
	}
    }
    else if (lex_state == LEX_INTERPNORMAL && !lex_brackets && !intuit_more(s))
	lex_state = LEX_INTERPEND;
    return s;
}

void pmflag(pmfl,ch)
U16* pmfl;
int ch;
{
    if (ch == 'i')
	*pmfl |= PMf_FOLD;
    else if (ch == 'g')
	*pmfl |= PMf_GLOBAL;
    else if (ch == 'c')
	*pmfl |= PMf_CONTINUE;
    else if (ch == 'o')
	*pmfl |= PMf_KEEP;
    else if (ch == 'm')
	*pmfl |= PMf_MULTILINE;
    else if (ch == 's')
	*pmfl |= PMf_SINGLELINE;
    else if (ch == 'x')
	*pmfl |= PMf_EXTENDED;
}

static char *
scan_pat(start)
char *start;
{
    PMOP *pm;
    char *s;

    s = scan_str(start);
    if (!s) {
	if (lex_stuff)
	    SvREFCNT_dec(lex_stuff);
	lex_stuff = Nullsv;
	croak("Search pattern not terminated");
    }

    pm = (PMOP*)newPMOP(OP_MATCH, 0);
    if (multi_open == '?')
	pm->op_pmflags |= PMf_ONCE;
    while (*s && strchr("iogcmsx", *s))
	pmflag(&pm->op_pmflags,*s++);
    pm->op_pmpermflags = pm->op_pmflags;

    lex_op = (OP*)pm;
    yylval.ival = OP_MATCH;
    return s;
}

static char *
scan_subst(start)
char *start;
{
    register char *s;
    register PMOP *pm;
    I32 first_start;
    I32 es = 0;

    yylval.ival = OP_NULL;

    s = scan_str(start);

    if (!s) {
	if (lex_stuff)
	    SvREFCNT_dec(lex_stuff);
	lex_stuff = Nullsv;
	croak("Substitution pattern not terminated");
    }

    if (s[-1] == multi_open)
	s--;

    first_start = multi_start;
    s = scan_str(s);
    if (!s) {
	if (lex_stuff)
	    SvREFCNT_dec(lex_stuff);
	lex_stuff = Nullsv;
	if (lex_repl)
	    SvREFCNT_dec(lex_repl);
	lex_repl = Nullsv;
	croak("Substitution replacement not terminated");
    }
    multi_start = first_start;	/* so whole substitution is taken together */

    pm = (PMOP*)newPMOP(OP_SUBST, 0);
    while (*s && strchr("iogcmsex", *s)) {
	if (*s == 'e') {
	    s++;
	    es++;
	}
	else
	    pmflag(&pm->op_pmflags,*s++);
    }

    if (es) {
	SV *repl;
	pm->op_pmflags |= PMf_EVAL;
	repl = newSVpv("",0);
	while (es-- > 0)
	    sv_catpv(repl, es ? "eval " : "do ");
	sv_catpvn(repl, "{ ", 2);
	sv_catsv(repl, lex_repl);
	sv_catpvn(repl, " };", 2);
	SvCOMPILED_on(repl);
	SvREFCNT_dec(lex_repl);
	lex_repl = repl;
    }

    pm->op_pmpermflags = pm->op_pmflags;
    lex_op = (OP*)pm;
    yylval.ival = OP_SUBST;
    return s;
}

void
hoistmust(pm)
register PMOP *pm;
{
    if (!pm->op_pmshort && pm->op_pmregexp->regstart &&
	(!pm->op_pmregexp->regmust || pm->op_pmregexp->reganch & ROPT_ANCH)
       ) {
	if (!(pm->op_pmregexp->reganch & ROPT_ANCH))
	    pm->op_pmflags |= PMf_SCANFIRST;
	pm->op_pmshort = SvREFCNT_inc(pm->op_pmregexp->regstart);
	pm->op_pmslen = SvCUR(pm->op_pmshort);
    }
    else if (pm->op_pmregexp->regmust) {/* is there a better short-circuit? */
	if (pm->op_pmshort &&
	  sv_eq(pm->op_pmshort,pm->op_pmregexp->regmust))
	{
	    if (pm->op_pmflags & PMf_SCANFIRST) {
		SvREFCNT_dec(pm->op_pmshort);
		pm->op_pmshort = Nullsv;
	    }
	    else {
		SvREFCNT_dec(pm->op_pmregexp->regmust);
		pm->op_pmregexp->regmust = Nullsv;
		return;
	    }
	}
	/* promote the better string */
	if ((!pm->op_pmshort &&
	     !(pm->op_pmregexp->reganch & ROPT_ANCH_GPOS)) ||
	    ((pm->op_pmflags & PMf_SCANFIRST) &&
	     (SvCUR(pm->op_pmshort) < SvCUR(pm->op_pmregexp->regmust)))) {
	    SvREFCNT_dec(pm->op_pmshort);		/* ok if null */
	    pm->op_pmshort = pm->op_pmregexp->regmust;
	    pm->op_pmslen = SvCUR(pm->op_pmshort);
	    pm->op_pmregexp->regmust = Nullsv;
	    pm->op_pmflags |= PMf_SCANFIRST;
	}
    }
}

static char *
scan_trans(start)
char *start;
{
    register char* s;
    OP *op;
    short *tbl;
    I32 squash;
    I32 delete;
    I32 complement;

    yylval.ival = OP_NULL;

    s = scan_str(start);
    if (!s) {
	if (lex_stuff)
	    SvREFCNT_dec(lex_stuff);
	lex_stuff = Nullsv;
	croak("Translation pattern not terminated");
    }
    if (s[-1] == multi_open)
	s--;

    s = scan_str(s);
    if (!s) {
	if (lex_stuff)
	    SvREFCNT_dec(lex_stuff);
	lex_stuff = Nullsv;
	if (lex_repl)
	    SvREFCNT_dec(lex_repl);
	lex_repl = Nullsv;
	croak("Translation replacement not terminated");
    }

    New(803,tbl,256,short);
    op = newPVOP(OP_TRANS, 0, (char*)tbl);

    complement = delete = squash = 0;
    while (*s == 'c' || *s == 'd' || *s == 's') {
	if (*s == 'c')
	    complement = OPpTRANS_COMPLEMENT;
	else if (*s == 'd')
	    delete = OPpTRANS_DELETE;
	else
	    squash = OPpTRANS_SQUASH;
	s++;
    }
    op->op_private = delete|squash|complement;

    lex_op = op;
    yylval.ival = OP_TRANS;
    return s;
}

static char *
scan_heredoc(s)
register char *s;
{
    SV *herewas;
    I32 op_type = OP_SCALAR;
    I32 len;
    SV *tmpstr;
    char term;
    register char *d;
    register char *e;
    char *peek;
    int outer = (rsfp && !lex_inwhat);

    s += 2;
    d = tokenbuf;
    e = tokenbuf + sizeof tokenbuf - 1;
    if (!outer)
	*d++ = '\n';
    for (peek = s; *peek == ' ' || *peek == '\t'; peek++) ;
    if (*peek && strchr("`'\"",*peek)) {
	s = peek;
	term = *s++;
	s = delimcpy(d, e, s, bufend, term, &len);
	d += len;
	if (s < bufend)
	    s++;
    }
    else {
	if (*s == '\\')
	    s++, term = '\'';
	else
	    term = '"';
	if (!isALNUM(*s))
	    deprecate("bare << to mean <<\"\"");
	for (; isALNUM(*s); s++) {
	    if (d < e)
		*d++ = *s;
	}
    }
    if (d >= tokenbuf + sizeof tokenbuf - 1)
	croak("Delimiter for here document is too long");
    *d++ = '\n';
    *d = '\0';
    len = d - tokenbuf;
    d = "\n";
    if (outer || !(d=ninstr(s,bufend,d,d+1)))
	herewas = newSVpv(s,bufend-s);
    else
	s--, herewas = newSVpv(s,d-s);
    s += SvCUR(herewas);

    tmpstr = NEWSV(87,80);
    sv_upgrade(tmpstr, SVt_PVIV);
    if (term == '\'') {
	op_type = OP_CONST;
	SvIVX(tmpstr) = -1;
    }
    else if (term == '`') {
	op_type = OP_BACKTICK;
	SvIVX(tmpstr) = '\\';
    }

    CLINE;
    multi_start = curcop->cop_line;
    multi_open = multi_close = '<';
    term = *tokenbuf;
    if (!outer) {
	d = s;
	while (s < bufend &&
	  (*s != term || memNE(s,tokenbuf,len)) ) {
	    if (*s++ == '\n')
		curcop->cop_line++;
	}
	if (s >= bufend) {
	    curcop->cop_line = multi_start;
	    missingterm(tokenbuf);
	}
	sv_setpvn(tmpstr,d+1,s-d);
	s += len - 1;
	sv_catpvn(herewas,s,bufend-s);
	sv_setsv(linestr,herewas);
	oldoldbufptr = oldbufptr = bufptr = s = linestart = SvPVX(linestr);
	bufend = SvPVX(linestr) + SvCUR(linestr);
    }
    else
	sv_setpvn(tmpstr,"",0);   /* avoid "uninitialized" warning */
    while (s >= bufend) {	/* multiple line string? */
	if (!outer ||
	 !(oldoldbufptr = oldbufptr = s = linestart = filter_gets(linestr, rsfp, 0))) {
	    curcop->cop_line = multi_start;
	    missingterm(tokenbuf);
	}
	curcop->cop_line++;
	if (PERLDB_LINE && curstash != debstash) {
	    SV *sv = NEWSV(88,0);

	    sv_upgrade(sv, SVt_PVMG);
	    sv_setsv(sv,linestr);
	    av_store(GvAV(curcop->cop_filegv),
	      (I32)curcop->cop_line,sv);
	}
	bufend = SvPVX(linestr) + SvCUR(linestr);
	if (*s == term && memEQ(s,tokenbuf,len)) {
	    s = bufend - 1;
	    *s = ' ';
	    sv_catsv(linestr,herewas);
	    bufend = SvPVX(linestr) + SvCUR(linestr);
	}
	else {
	    s = bufend;
	    sv_catsv(tmpstr,linestr);
	}
    }
    multi_end = curcop->cop_line;
    s++;
    if (SvCUR(tmpstr) + 5 < SvLEN(tmpstr)) {
	SvLEN_set(tmpstr, SvCUR(tmpstr) + 1);
	Renew(SvPVX(tmpstr), SvLEN(tmpstr), char);
    }
    SvREFCNT_dec(herewas);
    lex_stuff = tmpstr;
    yylval.ival = op_type;
    return s;
}

static char *
scan_inputsymbol(start)
char *start;
{
    register char *s = start;
    register char *d;
    register char *e;
    I32 len;

    d = tokenbuf;
    e = tokenbuf + sizeof tokenbuf;
    s = delimcpy(d, e, s + 1, bufend, '>', &len);
    if (len >= sizeof tokenbuf)
	croak("Excessively long <> operator");
    if (s >= bufend)
	croak("Unterminated <> operator");
    s++;
    if (*d == '$' && d[1]) d++;
    while (*d && (isALNUM(*d) || *d == '\'' || *d == ':'))
	d++;
    if (d - tokenbuf != len) {
	yylval.ival = OP_GLOB;
	set_csh();
	s = scan_str(start);
	if (!s)
	    croak("Glob not terminated");
	return s;
    }
    else {
	d = tokenbuf;
	if (!len)
	    (void)strcpy(d,"ARGV");
	if (*d == '$') {
	    I32 tmp;
	    if (tmp = pad_findmy(d)) {
		OP *op = newOP(OP_PADSV, 0);
		op->op_targ = tmp;
		lex_op = (OP*)newUNOP(OP_READLINE, 0, newUNOP(OP_RV2GV, 0, op));
	    }
	    else {
		GV *gv = gv_fetchpv(d+1,TRUE, SVt_PV);
		lex_op = (OP*)newUNOP(OP_READLINE, 0,
					newUNOP(OP_RV2GV, 0,
					    newUNOP(OP_RV2SV, 0,
						newGVOP(OP_GV, 0, gv))));
	    }
	    yylval.ival = OP_NULL;
	}
	else {
	    GV *gv = gv_fetchpv(d,TRUE, SVt_PVIO);
	    lex_op = (OP*)newUNOP(OP_READLINE, 0, newGVOP(OP_GV, 0, gv));
	    yylval.ival = OP_NULL;
	}
    }
    return s;
}

static char *
scan_str(start)
char *start;
{
    SV *sv;
    char *tmps;
    register char *s = start;
    register char term;
    register char *to;
    I32 brackets = 1;

    if (isSPACE(*s))
	s = skipspace(s);
    CLINE;
    term = *s;
    multi_start = curcop->cop_line;
    multi_open = term;
    if (term && (tmps = strchr("([{< )]}> )]}>",term)))
	term = tmps[5];
    multi_close = term;

    sv = NEWSV(87,80);
    sv_upgrade(sv, SVt_PVIV);
    SvIVX(sv) = term;
    (void)SvPOK_only(sv);		/* validate pointer */
    s++;
    for (;;) {
	SvGROW(sv, SvCUR(sv) + (bufend - s) + 1);
	to = SvPVX(sv)+SvCUR(sv);
	if (multi_open == multi_close) {
	    for (; s < bufend; s++,to++) {
		if (*s == '\n' && !rsfp)
		    curcop->cop_line++;
		if (*s == '\\' && s+1 < bufend && term != '\\') {
		    if (s[1] == term)
			s++;
		    else
			*to++ = *s++;
		}
		else if (*s == term)
		    break;
		*to = *s;
	    }
	}
	else {
	    for (; s < bufend; s++,to++) {
		if (*s == '\n' && !rsfp)
		    curcop->cop_line++;
		if (*s == '\\' && s+1 < bufend) {
		    if ((s[1] == multi_open) || (s[1] == multi_close))
			s++;
		    else
			*to++ = *s++;
		}
		else if (*s == multi_close && --brackets <= 0)
		    break;
		else if (*s == multi_open)
		    brackets++;
		*to = *s;
	    }
	}
	*to = '\0';
	SvCUR_set(sv, to - SvPVX(sv));

    if (s < bufend) break;	/* string ends on this line? */

	if (!rsfp ||
	 !(oldoldbufptr = oldbufptr = s = linestart = filter_gets(linestr, rsfp, 0))) {
	    sv_free(sv);
	    curcop->cop_line = multi_start;
	    return Nullch;
	}
	curcop->cop_line++;
	if (PERLDB_LINE && curstash != debstash) {
	    SV *sv = NEWSV(88,0);

	    sv_upgrade(sv, SVt_PVMG);
	    sv_setsv(sv,linestr);
	    av_store(GvAV(curcop->cop_filegv),
	      (I32)curcop->cop_line, sv);
	}
	bufend = SvPVX(linestr) + SvCUR(linestr);
    }
    multi_end = curcop->cop_line;
    s++;
    if (SvCUR(sv) + 5 < SvLEN(sv)) {
	SvLEN_set(sv, SvCUR(sv) + 1);
	Renew(SvPVX(sv), SvLEN(sv), char);
    }
    if (lex_stuff)
	lex_repl = sv;
    else
	lex_stuff = sv;
    return s;
}

char *
scan_num(start)
char *start;
{
    register char *s = start;
    register char *d;
    register char *e;
    I32 tryiv;
    double value;
    SV *sv;
    I32 floatit;
    char *lastub = 0;
    static char number_too_long[] = "Number too long";

    switch (*s) {
    default:
	croak("panic: scan_num");
    case '0':
	{
	    UV u;
	    I32 shift;
	    bool overflowed = FALSE;

	    if (s[1] == 'x') {
		shift = 4;
		s += 2;
	    }
	    else if (s[1] == '.')
		goto decimal;
	    else
		shift = 3;
	    u = 0;
	    for (;;) {
		UV n, b;

		switch (*s) {
		default:
		    goto out;
		case '_':
		    s++;
		    break;
		case '8': case '9':
		    if (shift != 4)
			yyerror("Illegal octal digit");
		    /* FALL THROUGH */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7':
		    b = *s++ & 15;
		    goto digit;
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		    if (shift != 4)
			goto out;
		    b = (*s++ & 7) + 9;
		  digit:
		    n = u << shift;
		    if (!overflowed && (n >> shift) != u) {
			warn("Integer overflow in %s number",
			     (shift == 4) ? "hex" : "octal");
			overflowed = TRUE;
		    }
		    u = n | b;
		    break;
		}
	    }
	  out:
	    sv = NEWSV(92,0);
	    sv_setuv(sv, u);
	}
	break;
    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9': case '.':
      decimal:
	d = tokenbuf;
	e = tokenbuf + sizeof tokenbuf - 6; /* room for various punctuation */
	floatit = FALSE;
	while (isDIGIT(*s) || *s == '_') {
	    if (*s == '_') {
		if (dowarn && lastub && s - lastub != 3)
		    warn("Misplaced _ in number");
		lastub = ++s;
	    }
	    else {
		if (d >= e)
		    croak(number_too_long);
		*d++ = *s++;
	    }
	}
	if (dowarn && lastub && s - lastub != 3)
	    warn("Misplaced _ in number");
	if (*s == '.' && s[1] != '.') {
	    floatit = TRUE;
	    *d++ = *s++;
	    for (; isDIGIT(*s) || *s == '_'; s++) {
		if (d >= e)
		    croak(number_too_long);
		if (*s != '_')
		    *d++ = *s;
	    }
	}
	if (*s && strchr("eE",*s) && strchr("+-0123456789",s[1])) {
	    floatit = TRUE;
	    s++;
	    *d++ = 'e';		/* At least some Mach atof()s don't grok 'E' */
	    if (*s == '+' || *s == '-')
		*d++ = *s++;
	    while (isDIGIT(*s)) {
		if (d >= e)
		    croak(number_too_long);
		*d++ = *s++;
	    }
	}
	*d = '\0';
	sv = NEWSV(92,0);
	SET_NUMERIC_STANDARD();
	value = atof(tokenbuf);
	tryiv = I_V(value);
	if (!floatit && (double)tryiv == value)
	    sv_setiv(sv, tryiv);
	else
	    sv_setnv(sv, value);
	break;
    }

    yylval.opval = newSVOP(OP_CONST, 0, sv);

    return s;
}

static char *
scan_formline(s)
register char *s;
{
    register char *eol;
    register char *t;
    SV *stuff = newSVpv("",0);
    bool needargs = FALSE;

    while (!needargs) {
	if (*s == '.' || *s == '}') {
	    /*SUPPRESS 530*/
	    for (t = s+1; *t == ' ' || *t == '\t'; t++) ;
	    if (*t == '\n')
		break;
	}
	if (in_eval && !rsfp) {
	    eol = strchr(s,'\n');
	    if (!eol++)
		eol = bufend;
	}
	else
	    eol = bufend = SvPVX(linestr) + SvCUR(linestr);
	if (*s != '#') {
	    for (t = s; t < eol; t++) {
		if (*t == '~' && t[1] == '~' && SvCUR(stuff)) {
		    needargs = FALSE;
		    goto enough;	/* ~~ must be first line in formline */
		}
		if (*t == '@' || *t == '^')
		    needargs = TRUE;
	    }
	    sv_catpvn(stuff, s, eol-s);
	}
	s = eol;
	if (rsfp) {
	    s = filter_gets(linestr, rsfp, 0);
	    oldoldbufptr = oldbufptr = bufptr = linestart = SvPVX(linestr);
	    bufend = bufptr + SvCUR(linestr);
	    if (!s) {
		s = bufptr;
		yyerror("Format not terminated");
		break;
	    }
	}
	incline(s);
    }
  enough:
    if (SvCUR(stuff)) {
	expect = XTERM;
	if (needargs) {
	    lex_state = LEX_NORMAL;
	    nextval[nexttoke].ival = 0;
	    force_next(',');
	}
	else
	    lex_state = LEX_FORMLINE;
	nextval[nexttoke].opval = (OP*)newSVOP(OP_CONST, 0, stuff);
	force_next(THING);
	nextval[nexttoke].ival = OP_FORMLINE;
	force_next(LSTOP);
    }
    else {
	SvREFCNT_dec(stuff);
	lex_formbrack = 0;
	bufptr = s;
    }
    return s;
}

static void
set_csh()
{
#ifdef CSH
    if (!cshlen)
	cshlen = strlen(cshname);
#endif
}

I32
start_subparse(is_format, flags)
I32 is_format;
U32 flags;
{
    I32 oldsavestack_ix = savestack_ix;
    CV* outsidecv = compcv;
    AV* comppadlist;

    if (compcv) {
	assert(SvTYPE(compcv) == SVt_PVCV);
    }
    save_I32(&subline);
    save_item(subname);
    SAVEI32(padix);
    SAVESPTR(curpad);
    SAVESPTR(comppad);
    SAVESPTR(comppad_name);
    SAVESPTR(compcv);
    SAVEI32(comppad_name_fill);
    SAVEI32(min_intro_pending);
    SAVEI32(max_intro_pending);
    SAVEI32(pad_reset_pending);

    compcv = (CV*)NEWSV(1104,0);
    sv_upgrade((SV *)compcv, is_format ? SVt_PVFM : SVt_PVCV);
    CvFLAGS(compcv) |= flags;

    comppad = newAV();
    comppad_name = newAV();
    comppad_name_fill = 0;
    min_intro_pending = 0;
    av_push(comppad, Nullsv);
    curpad = AvARRAY(comppad);
    padix = 0;
    subline = curcop->cop_line;

    comppadlist = newAV();
    AvREAL_off(comppadlist);
    av_store(comppadlist, 0, (SV*)comppad_name);
    av_store(comppadlist, 1, (SV*)comppad);

    CvPADLIST(compcv) = comppadlist;
    CvOUTSIDE(compcv) = (CV*)SvREFCNT_inc((SV*)outsidecv);

    return oldsavestack_ix;
}

int
yywarn(s)
char *s;
{
    --error_count;
    in_eval |= 2;
    yyerror(s);
    in_eval &= ~2;
    return 0;
}

int
yyerror(s)
char *s;
{
    char *where = NULL;
    char *context = NULL;
    int contlen = -1;
    SV *msg;

    if (!yychar || (yychar == ';' && !rsfp))
	where = "at EOF";
    else if (bufptr > oldoldbufptr && bufptr - oldoldbufptr < 200 &&
      oldoldbufptr != oldbufptr && oldbufptr != bufptr) {
	while (isSPACE(*oldoldbufptr))
	    oldoldbufptr++;
	context = oldoldbufptr;
	contlen = bufptr - oldoldbufptr;
    }
    else if (bufptr > oldbufptr && bufptr - oldbufptr < 200 &&
      oldbufptr != bufptr) {
	while (isSPACE(*oldbufptr))
	    oldbufptr++;
	context = oldbufptr;
	contlen = bufptr - oldbufptr;
    }
    else if (yychar > 255)
	where = "next token ???";
    else if ((yychar & 127) == 127) {
	if (lex_state == LEX_NORMAL ||
	   (lex_state == LEX_KNOWNEXT && lex_defer == LEX_NORMAL))
	    where = "at end of line";
	else if (lex_inpat)
	    where = "within pattern";
	else
	    where = "within string";
    }
    else {
	SV *where_sv = sv_2mortal(newSVpv("next char ", 0));
	if (yychar < 32)
	    sv_catpvf(where_sv, "^%c", toCTRL(yychar));
	else if (isPRINT_LC(yychar))
	    sv_catpvf(where_sv, "%c", yychar);
	else
	    sv_catpvf(where_sv, "\\%03o", yychar & 255);
	where = SvPVX(where_sv);
    }
    msg = sv_2mortal(newSVpv(s, 0));
    sv_catpvf(msg, " at %_ line %ld, ",
	      GvSV(curcop->cop_filegv), (long)curcop->cop_line);
    if (context)
	sv_catpvf(msg, "near \"%.*s\"\n", contlen, context);
    else
	sv_catpvf(msg, "%s\n", where);
    if (multi_start < multi_end && (U32)(curcop->cop_line - multi_end) <= 1) {
	sv_catpvf(msg,
	"  (Might be a runaway multi-line %c%c string starting on line %ld)\n",
		(int)multi_open,(int)multi_close,(long)multi_start);
        multi_end = 0;
    }
    if (in_eval & 2)
	warn("%_", msg);
    else if (in_eval)
	sv_catsv(GvSV(errgv), msg);
    else
	PerlIO_write(PerlIO_stderr(), SvPVX(msg), SvCUR(msg));
    if (++error_count >= 10)
	croak("%_ has too many errors.\n", GvSV(curcop->cop_filegv));
    in_my = 0;
    return 0;
}

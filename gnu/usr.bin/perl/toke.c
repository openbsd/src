/*    toke.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
 *    2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 *  'It all comes from here, the stench and the peril.'    --Frodo
 *
 *     [p.719 of _The Lord of the Rings_, IV/ix: "Shelob's Lair"]
 */

/*
 * This file is the lexer for Perl.  It's closely linked to the
 * parser, perly.y.
 *
 * The main routine is yylex(), which returns the next token.
 */

/*
=head1 Lexer interface
This is the lower layer of the Perl parser, managing characters and tokens.

=for apidoc AmU|yy_parser *|PL_parser

Pointer to a structure encapsulating the state of the parsing operation
currently in progress.  The pointer can be locally changed to perform
a nested parse without interfering with the state of an outer parse.
Individual members of C<PL_parser> have their own documentation.

=cut
*/

#include "EXTERN.h"
#define PERL_IN_TOKE_C
#include "perl.h"
#include "dquote_inline.h"
#include "invlist_inline.h"

#define new_constant(a,b,c,d,e,f,g)	\
	S_new_constant(aTHX_ a,b,STR_WITH_LEN(c),d,e,f, g)

#define pl_yylval	(PL_parser->yylval)

/* XXX temporary backwards compatibility */
#define PL_lex_brackets		(PL_parser->lex_brackets)
#define PL_lex_allbrackets	(PL_parser->lex_allbrackets)
#define PL_lex_fakeeof		(PL_parser->lex_fakeeof)
#define PL_lex_brackstack	(PL_parser->lex_brackstack)
#define PL_lex_casemods		(PL_parser->lex_casemods)
#define PL_lex_casestack        (PL_parser->lex_casestack)
#define PL_lex_dojoin		(PL_parser->lex_dojoin)
#define PL_lex_formbrack        (PL_parser->lex_formbrack)
#define PL_lex_inpat		(PL_parser->lex_inpat)
#define PL_lex_inwhat		(PL_parser->lex_inwhat)
#define PL_lex_op		(PL_parser->lex_op)
#define PL_lex_repl		(PL_parser->lex_repl)
#define PL_lex_starts		(PL_parser->lex_starts)
#define PL_lex_stuff		(PL_parser->lex_stuff)
#define PL_multi_start		(PL_parser->multi_start)
#define PL_multi_open		(PL_parser->multi_open)
#define PL_multi_close		(PL_parser->multi_close)
#define PL_preambled		(PL_parser->preambled)
#define PL_linestr		(PL_parser->linestr)
#define PL_expect		(PL_parser->expect)
#define PL_copline		(PL_parser->copline)
#define PL_bufptr		(PL_parser->bufptr)
#define PL_oldbufptr		(PL_parser->oldbufptr)
#define PL_oldoldbufptr		(PL_parser->oldoldbufptr)
#define PL_linestart		(PL_parser->linestart)
#define PL_bufend		(PL_parser->bufend)
#define PL_last_uni		(PL_parser->last_uni)
#define PL_last_lop		(PL_parser->last_lop)
#define PL_last_lop_op		(PL_parser->last_lop_op)
#define PL_lex_state		(PL_parser->lex_state)
#define PL_rsfp			(PL_parser->rsfp)
#define PL_rsfp_filters		(PL_parser->rsfp_filters)
#define PL_in_my		(PL_parser->in_my)
#define PL_in_my_stash		(PL_parser->in_my_stash)
#define PL_tokenbuf		(PL_parser->tokenbuf)
#define PL_multi_end		(PL_parser->multi_end)
#define PL_error_count		(PL_parser->error_count)

#  define PL_nexttoke		(PL_parser->nexttoke)
#  define PL_nexttype		(PL_parser->nexttype)
#  define PL_nextval		(PL_parser->nextval)


#define SvEVALED(sv) \
    (SvTYPE(sv) >= SVt_PVNV \
    && ((XPVIV*)SvANY(sv))->xiv_u.xivu_eval_seen)

static const char* const ident_too_long = "Identifier too long";

#  define NEXTVAL_NEXTTOKE PL_nextval[PL_nexttoke]

#define XENUMMASK  0x3f
#define XFAKEEOF   0x40
#define XFAKEBRACK 0x80

#ifdef USE_UTF8_SCRIPTS
#   define UTF cBOOL(!IN_BYTES)
#else
#   define UTF cBOOL((PL_linestr && DO_UTF8(PL_linestr)) || ( !(PL_parser->lex_flags & LEX_IGNORE_UTF8_HINTS) && (PL_hints & HINT_UTF8)))
#endif

/* The maximum number of characters preceding the unrecognized one to display */
#define UNRECOGNIZED_PRECEDE_COUNT 10

/* In variables named $^X, these are the legal values for X.
 * 1999-02-27 mjd-perl-patch@plover.com */
#define isCONTROLVAR(x) (isUPPER(x) || strchr("[\\]^_?", (x)))

#define SPACE_OR_TAB(c) isBLANK_A(c)

#define HEXFP_PEEK(s)     \
    (((s[0] == '.') && \
      (isXDIGIT(s[1]) || isALPHA_FOLD_EQ(s[1], 'p'))) || \
     isALPHA_FOLD_EQ(s[0], 'p'))

/* LEX_* are values for PL_lex_state, the state of the lexer.
 * They are arranged oddly so that the guard on the switch statement
 * can get by with a single comparison (if the compiler is smart enough).
 *
 * These values refer to the various states within a sublex parse,
 * i.e. within a double quotish string
 */

/* #define LEX_NOTPARSING		11 is done in perl.h. */

#define LEX_NORMAL		10 /* normal code (ie not within "...")     */
#define LEX_INTERPNORMAL	 9 /* code within a string, eg "$foo[$x+1]" */
#define LEX_INTERPCASEMOD	 8 /* expecting a \U, \Q or \E etc          */
#define LEX_INTERPPUSH		 7 /* starting a new sublex parse level     */
#define LEX_INTERPSTART		 6 /* expecting the start of a $var         */

				   /* at end of code, eg "$x" followed by:  */
#define LEX_INTERPEND		 5 /* ... eg not one of [, { or ->          */
#define LEX_INTERPENDMAYBE	 4 /* ... eg one of [, { or ->              */

#define LEX_INTERPCONCAT	 3 /* expecting anything, eg at start of
				        string or after \E, $foo, etc       */
#define LEX_INTERPCONST		 2 /* NOT USED */
#define LEX_FORMLINE		 1 /* expecting a format line               */


#ifdef DEBUGGING
static const char* const lex_state_names[] = {
    "KNOWNEXT",
    "FORMLINE",
    "INTERPCONST",
    "INTERPCONCAT",
    "INTERPENDMAYBE",
    "INTERPEND",
    "INTERPSTART",
    "INTERPPUSH",
    "INTERPCASEMOD",
    "INTERPNORMAL",
    "NORMAL"
};
#endif

#include "keywords.h"

/* CLINE is a macro that ensures PL_copline has a sane value */

#define CLINE (PL_copline = (CopLINE(PL_curcop) < PL_copline ? CopLINE(PL_curcop) : PL_copline))

/*
 * Convenience functions to return different tokens and prime the
 * lexer for the next token.  They all take an argument.
 *
 * TOKEN        : generic token (used for '(', DOLSHARP, etc)
 * OPERATOR     : generic operator
 * AOPERATOR    : assignment operator
 * PREBLOCK     : beginning the block after an if, while, foreach, ...
 * PRETERMBLOCK : beginning a non-code-defining {} block (eg, hash ref)
 * PREREF       : *EXPR where EXPR is not a simple identifier
 * TERM         : expression term
 * POSTDEREF    : postfix dereference (->$* ->@[...] etc.)
 * LOOPX        : loop exiting command (goto, last, dump, etc)
 * FTST         : file test operator
 * FUN0         : zero-argument function
 * FUN0OP       : zero-argument function, with its op created in this file
 * FUN1         : not used, except for not, which isn't a UNIOP
 * BOop         : bitwise or or xor
 * BAop         : bitwise and
 * BCop         : bitwise complement
 * SHop         : shift operator
 * PWop         : power operator
 * PMop         : pattern-matching operator
 * Aop          : addition-level operator
 * AopNOASSIGN  : addition-level operator that is never part of .=
 * Mop          : multiplication-level operator
 * Eop          : equality-testing operator
 * Rop          : relational operator <= != gt
 *
 * Also see LOP and lop() below.
 */

#ifdef DEBUGGING /* Serve -DT. */
#   define REPORT(retval) tokereport((I32)retval, &pl_yylval)
#else
#   define REPORT(retval) (retval)
#endif

#define TOKEN(retval) return ( PL_bufptr = s, REPORT(retval))
#define OPERATOR(retval) return (PL_expect = XTERM, PL_bufptr = s, REPORT(retval))
#define AOPERATOR(retval) return ao((PL_expect = XTERM, PL_bufptr = s, retval))
#define PREBLOCK(retval) return (PL_expect = XBLOCK,PL_bufptr = s, REPORT(retval))
#define PRETERMBLOCK(retval) return (PL_expect = XTERMBLOCK,PL_bufptr = s, REPORT(retval))
#define PREREF(retval) return (PL_expect = XREF,PL_bufptr = s, REPORT(retval))
#define TERM(retval) return (CLINE, PL_expect = XOPERATOR, PL_bufptr = s, REPORT(retval))
#define POSTDEREF(f) return (PL_bufptr = s, S_postderef(aTHX_ REPORT(f),s[1]))
#define LOOPX(f) return (PL_bufptr = force_word(s,BAREWORD,TRUE,FALSE), \
			 pl_yylval.ival=f, \
			 PL_expect = PL_nexttoke ? XOPERATOR : XTERM, \
			 REPORT((int)LOOPEX))
#define FTST(f)  return (pl_yylval.ival=f, PL_expect=XTERMORDORDOR, PL_bufptr=s, REPORT((int)UNIOP))
#define FUN0(f)  return (pl_yylval.ival=f, PL_expect=XOPERATOR, PL_bufptr=s, REPORT((int)FUNC0))
#define FUN0OP(f)  return (pl_yylval.opval=f, CLINE, PL_expect=XOPERATOR, PL_bufptr=s, REPORT((int)FUNC0OP))
#define FUN1(f)  return (pl_yylval.ival=f, PL_expect=XOPERATOR, PL_bufptr=s, REPORT((int)FUNC1))
#define BOop(f)  return ao((pl_yylval.ival=f, PL_expect=XTERM, PL_bufptr=s, (int)BITOROP))
#define BAop(f)  return ao((pl_yylval.ival=f, PL_expect=XTERM, PL_bufptr=s, (int)BITANDOP))
#define BCop(f) return pl_yylval.ival=f, PL_expect=XTERM, PL_bufptr = s, \
		       REPORT('~')
#define SHop(f)  return ao((pl_yylval.ival=f, PL_expect=XTERM, PL_bufptr=s, (int)SHIFTOP))
#define PWop(f)  return ao((pl_yylval.ival=f, PL_expect=XTERM, PL_bufptr=s, (int)POWOP))
#define PMop(f)  return(pl_yylval.ival=f, PL_expect=XTERM, PL_bufptr=s, REPORT((int)MATCHOP))
#define Aop(f)   return ao((pl_yylval.ival=f, PL_expect=XTERM, PL_bufptr=s, (int)ADDOP))
#define AopNOASSIGN(f) return (pl_yylval.ival=f, PL_bufptr=s, REPORT((int)ADDOP))
#define Mop(f)   return ao((pl_yylval.ival=f, PL_expect=XTERM, PL_bufptr=s, (int)MULOP))
#define Eop(f)   return (pl_yylval.ival=f, PL_expect=XTERM, PL_bufptr=s, REPORT((int)EQOP))
#define Rop(f)   return (pl_yylval.ival=f, PL_expect=XTERM, PL_bufptr=s, REPORT((int)RELOP))

/* This bit of chicanery makes a unary function followed by
 * a parenthesis into a function with one argument, highest precedence.
 * The UNIDOR macro is for unary functions that can be followed by the //
 * operator (such as C<shift // 0>).
 */
#define UNI3(f,x,have_x) { \
	pl_yylval.ival = f; \
	if (have_x) PL_expect = x; \
	PL_bufptr = s; \
	PL_last_uni = PL_oldbufptr; \
	PL_last_lop_op = (f) < 0 ? -(f) : (f); \
	if (*s == '(') \
	    return REPORT( (int)FUNC1 ); \
	s = skipspace(s); \
	return REPORT( *s=='(' ? (int)FUNC1 : (int)UNIOP ); \
	}
#define UNI(f)    UNI3(f,XTERM,1)
#define UNIDOR(f) UNI3(f,XTERMORDORDOR,1)
#define UNIPROTO(f,optional) { \
	if (optional) PL_last_uni = PL_oldbufptr; \
	OPERATOR(f); \
	}

#define UNIBRACK(f) UNI3(f,0,0)

/* grandfather return to old style */
#define OLDLOP(f) \
	do { \
	    if (!PL_lex_allbrackets && PL_lex_fakeeof > LEX_FAKEEOF_LOWLOGIC) \
		PL_lex_fakeeof = LEX_FAKEEOF_LOWLOGIC; \
	    pl_yylval.ival = (f); \
	    PL_expect = XTERM; \
	    PL_bufptr = s; \
	    return (int)LSTOP; \
	} while(0)

#define COPLINE_INC_WITH_HERELINES		    \
    STMT_START {				     \
	CopLINE_inc(PL_curcop);			      \
	if (PL_parser->herelines)		       \
	    CopLINE(PL_curcop) += PL_parser->herelines, \
	    PL_parser->herelines = 0;			 \
    } STMT_END
/* Called after scan_str to update CopLINE(PL_curcop), but only when there
 * is no sublex_push to follow. */
#define COPLINE_SET_FROM_MULTI_END	      \
    STMT_START {			       \
	CopLINE_set(PL_curcop, PL_multi_end);	\
	if (PL_multi_end != PL_multi_start)	 \
	    PL_parser->herelines = 0;		  \
    } STMT_END


#ifdef DEBUGGING

/* how to interpret the pl_yylval associated with the token */
enum token_type {
    TOKENTYPE_NONE,
    TOKENTYPE_IVAL,
    TOKENTYPE_OPNUM, /* pl_yylval.ival contains an opcode number */
    TOKENTYPE_PVAL,
    TOKENTYPE_OPVAL
};

static struct debug_tokens {
    const int token;
    enum token_type type;
    const char *name;
} const debug_tokens[] =
{
    { ADDOP,		TOKENTYPE_OPNUM,	"ADDOP" },
    { ANDAND,		TOKENTYPE_NONE,		"ANDAND" },
    { ANDOP,		TOKENTYPE_NONE,		"ANDOP" },
    { ANONSUB,		TOKENTYPE_IVAL,		"ANONSUB" },
    { ANON_SIGSUB,	TOKENTYPE_IVAL,		"ANON_SIGSUB" },
    { ARROW,		TOKENTYPE_NONE,		"ARROW" },
    { ASSIGNOP,		TOKENTYPE_OPNUM,	"ASSIGNOP" },
    { BITANDOP,		TOKENTYPE_OPNUM,	"BITANDOP" },
    { BITOROP,		TOKENTYPE_OPNUM,	"BITOROP" },
    { COLONATTR,	TOKENTYPE_NONE,		"COLONATTR" },
    { CONTINUE,		TOKENTYPE_NONE,		"CONTINUE" },
    { DEFAULT,		TOKENTYPE_NONE,		"DEFAULT" },
    { DO,		TOKENTYPE_NONE,		"DO" },
    { DOLSHARP,		TOKENTYPE_NONE,		"DOLSHARP" },
    { DORDOR,		TOKENTYPE_NONE,		"DORDOR" },
    { DOROP,		TOKENTYPE_OPNUM,	"DOROP" },
    { DOTDOT,		TOKENTYPE_IVAL,		"DOTDOT" },
    { ELSE,		TOKENTYPE_NONE,		"ELSE" },
    { ELSIF,		TOKENTYPE_IVAL,		"ELSIF" },
    { EQOP,		TOKENTYPE_OPNUM,	"EQOP" },
    { FOR,		TOKENTYPE_IVAL,		"FOR" },
    { FORMAT,		TOKENTYPE_NONE,		"FORMAT" },
    { FORMLBRACK,	TOKENTYPE_NONE,		"FORMLBRACK" },
    { FORMRBRACK,	TOKENTYPE_NONE,		"FORMRBRACK" },
    { FUNC,		TOKENTYPE_OPNUM,	"FUNC" },
    { FUNC0,		TOKENTYPE_OPNUM,	"FUNC0" },
    { FUNC0OP,		TOKENTYPE_OPVAL,	"FUNC0OP" },
    { FUNC0SUB,		TOKENTYPE_OPVAL,	"FUNC0SUB" },
    { FUNC1,		TOKENTYPE_OPNUM,	"FUNC1" },
    { FUNCMETH,		TOKENTYPE_OPVAL,	"FUNCMETH" },
    { GIVEN,		TOKENTYPE_IVAL,		"GIVEN" },
    { HASHBRACK,	TOKENTYPE_NONE,		"HASHBRACK" },
    { IF,		TOKENTYPE_IVAL,		"IF" },
    { LABEL,		TOKENTYPE_PVAL,		"LABEL" },
    { LOCAL,		TOKENTYPE_IVAL,		"LOCAL" },
    { LOOPEX,		TOKENTYPE_OPNUM,	"LOOPEX" },
    { LSTOP,		TOKENTYPE_OPNUM,	"LSTOP" },
    { LSTOPSUB,		TOKENTYPE_OPVAL,	"LSTOPSUB" },
    { MATCHOP,		TOKENTYPE_OPNUM,	"MATCHOP" },
    { METHOD,		TOKENTYPE_OPVAL,	"METHOD" },
    { MULOP,		TOKENTYPE_OPNUM,	"MULOP" },
    { MY,		TOKENTYPE_IVAL,		"MY" },
    { NOAMP,		TOKENTYPE_NONE,		"NOAMP" },
    { NOTOP,		TOKENTYPE_NONE,		"NOTOP" },
    { OROP,		TOKENTYPE_IVAL,		"OROP" },
    { OROR,		TOKENTYPE_NONE,		"OROR" },
    { PACKAGE,		TOKENTYPE_NONE,		"PACKAGE" },
    { PLUGEXPR,		TOKENTYPE_OPVAL,	"PLUGEXPR" },
    { PLUGSTMT,		TOKENTYPE_OPVAL,	"PLUGSTMT" },
    { PMFUNC,		TOKENTYPE_OPVAL,	"PMFUNC" },
    { POSTJOIN,		TOKENTYPE_NONE,		"POSTJOIN" },
    { POSTDEC,		TOKENTYPE_NONE,		"POSTDEC" },
    { POSTINC,		TOKENTYPE_NONE,		"POSTINC" },
    { POWOP,		TOKENTYPE_OPNUM,	"POWOP" },
    { PREDEC,		TOKENTYPE_NONE,		"PREDEC" },
    { PREINC,		TOKENTYPE_NONE,		"PREINC" },
    { PRIVATEREF,	TOKENTYPE_OPVAL,	"PRIVATEREF" },
    { QWLIST,		TOKENTYPE_OPVAL,	"QWLIST" },
    { REFGEN,		TOKENTYPE_NONE,		"REFGEN" },
    { RELOP,		TOKENTYPE_OPNUM,	"RELOP" },
    { REQUIRE,		TOKENTYPE_NONE,		"REQUIRE" },
    { SHIFTOP,		TOKENTYPE_OPNUM,	"SHIFTOP" },
    { SIGSUB,		TOKENTYPE_NONE,		"SIGSUB" },
    { SUB,		TOKENTYPE_NONE,		"SUB" },
    { THING,		TOKENTYPE_OPVAL,	"THING" },
    { UMINUS,		TOKENTYPE_NONE,		"UMINUS" },
    { UNIOP,		TOKENTYPE_OPNUM,	"UNIOP" },
    { UNIOPSUB,		TOKENTYPE_OPVAL,	"UNIOPSUB" },
    { UNLESS,		TOKENTYPE_IVAL,		"UNLESS" },
    { UNTIL,		TOKENTYPE_IVAL,		"UNTIL" },
    { USE,		TOKENTYPE_IVAL,		"USE" },
    { WHEN,		TOKENTYPE_IVAL,		"WHEN" },
    { WHILE,		TOKENTYPE_IVAL,		"WHILE" },
    { BAREWORD,		TOKENTYPE_OPVAL,	"BAREWORD" },
    { YADAYADA,		TOKENTYPE_IVAL,		"YADAYADA" },
    { 0,		TOKENTYPE_NONE,		NULL }
};

/* dump the returned token in rv, plus any optional arg in pl_yylval */

STATIC int
S_tokereport(pTHX_ I32 rv, const YYSTYPE* lvalp)
{
    PERL_ARGS_ASSERT_TOKEREPORT;

    if (DEBUG_T_TEST) {
	const char *name = NULL;
	enum token_type type = TOKENTYPE_NONE;
	const struct debug_tokens *p;
	SV* const report = newSVpvs("<== ");

	for (p = debug_tokens; p->token; p++) {
	    if (p->token == (int)rv) {
		name = p->name;
		type = p->type;
		break;
	    }
	}
	if (name)
	    Perl_sv_catpv(aTHX_ report, name);
	else if (isGRAPH(rv))
	{
	    Perl_sv_catpvf(aTHX_ report, "'%c'", (char)rv);
	    if ((char)rv == 'p')
		sv_catpvs(report, " (pending identifier)");
	}
	else if (!rv)
	    sv_catpvs(report, "EOF");
	else
	    Perl_sv_catpvf(aTHX_ report, "?? %" IVdf, (IV)rv);
	switch (type) {
	case TOKENTYPE_NONE:
	    break;
	case TOKENTYPE_IVAL:
	    Perl_sv_catpvf(aTHX_ report, "(ival=%" IVdf ")", (IV)lvalp->ival);
	    break;
	case TOKENTYPE_OPNUM:
	    Perl_sv_catpvf(aTHX_ report, "(ival=op_%s)",
				    PL_op_name[lvalp->ival]);
	    break;
	case TOKENTYPE_PVAL:
	    Perl_sv_catpvf(aTHX_ report, "(pval=\"%s\")", lvalp->pval);
	    break;
	case TOKENTYPE_OPVAL:
	    if (lvalp->opval) {
		Perl_sv_catpvf(aTHX_ report, "(opval=op_%s)",
				    PL_op_name[lvalp->opval->op_type]);
		if (lvalp->opval->op_type == OP_CONST) {
		    Perl_sv_catpvf(aTHX_ report, " %s",
			SvPEEK(cSVOPx_sv(lvalp->opval)));
		}

	    }
	    else
		sv_catpvs(report, "(opval=null)");
	    break;
	}
        PerlIO_printf(Perl_debug_log, "### %s\n\n", SvPV_nolen_const(report));
    };
    return (int)rv;
}


/* print the buffer with suitable escapes */

STATIC void
S_printbuf(pTHX_ const char *const fmt, const char *const s)
{
    SV* const tmp = newSVpvs("");

    PERL_ARGS_ASSERT_PRINTBUF;

    GCC_DIAG_IGNORE_STMT(-Wformat-nonliteral); /* fmt checked by caller */
    PerlIO_printf(Perl_debug_log, fmt, pv_display(tmp, s, strlen(s), 0, 60));
    GCC_DIAG_RESTORE_STMT;
    SvREFCNT_dec(tmp);
}

#endif

/*
 * S_ao
 *
 * This subroutine looks for an '=' next to the operator that has just been
 * parsed and turns it into an ASSIGNOP if it finds one.
 */

STATIC int
S_ao(pTHX_ int toketype)
{
    if (*PL_bufptr == '=') {
	PL_bufptr++;
	if (toketype == ANDAND)
	    pl_yylval.ival = OP_ANDASSIGN;
	else if (toketype == OROR)
	    pl_yylval.ival = OP_ORASSIGN;
	else if (toketype == DORDOR)
	    pl_yylval.ival = OP_DORASSIGN;
	toketype = ASSIGNOP;
    }
    return REPORT(toketype);
}

/*
 * S_no_op
 * When Perl expects an operator and finds something else, no_op
 * prints the warning.  It always prints "<something> found where
 * operator expected.  It prints "Missing semicolon on previous line?"
 * if the surprise occurs at the start of the line.  "do you need to
 * predeclare ..." is printed out for code like "sub bar; foo bar $x"
 * where the compiler doesn't know if foo is a method call or a function.
 * It prints "Missing operator before end of line" if there's nothing
 * after the missing operator, or "... before <...>" if there is something
 * after the missing operator.
 *
 * PL_bufptr is expected to point to the start of the thing that was found,
 * and s after the next token or partial token.
 */

STATIC void
S_no_op(pTHX_ const char *const what, char *s)
{
    char * const oldbp = PL_bufptr;
    const bool is_first = (PL_oldbufptr == PL_linestart);

    PERL_ARGS_ASSERT_NO_OP;

    if (!s)
	s = oldbp;
    else
	PL_bufptr = s;
    yywarn(Perl_form(aTHX_ "%s found where operator expected", what), UTF ? SVf_UTF8 : 0);
    if (ckWARN_d(WARN_SYNTAX)) {
	if (is_first)
	    Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
		    "\t(Missing semicolon on previous line?)\n");
        else if (PL_oldoldbufptr && isIDFIRST_lazy_if_safe(PL_oldoldbufptr,
                                                           PL_bufend,
                                                           UTF))
        {
	    const char *t;
	    for (t = PL_oldoldbufptr;
                 (isWORDCHAR_lazy_if_safe(t, PL_bufend, UTF) || *t == ':');
                 t += UTF ? UTF8SKIP(t) : 1)
            {
		NOOP;
            }
	    if (t < PL_bufptr && isSPACE(*t))
		Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
			"\t(Do you need to predeclare %" UTF8f "?)\n",
		      UTF8fARG(UTF, t - PL_oldoldbufptr, PL_oldoldbufptr));
	}
	else {
	    assert(s >= oldbp);
	    Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
		    "\t(Missing operator before %" UTF8f "?)\n",
		     UTF8fARG(UTF, s - oldbp, oldbp));
	}
    }
    PL_bufptr = oldbp;
}

/*
 * S_missingterm
 * Complain about missing quote/regexp/heredoc terminator.
 * If it's called with NULL then it cauterizes the line buffer.
 * If we're in a delimited string and the delimiter is a control
 * character, it's reformatted into a two-char sequence like ^C.
 * This is fatal.
 */

STATIC void
S_missingterm(pTHX_ char *s, STRLEN len)
{
    char tmpbuf[UTF8_MAXBYTES + 1];
    char q;
    bool uni = FALSE;
    SV *sv;
    if (s) {
	char * const nl = (char *) my_memrchr(s, '\n', len);
        if (nl) {
            *nl = '\0';
            len = nl - s;
        }
	uni = UTF;
    }
    else if (PL_multi_close < 32) {
	*tmpbuf = '^';
	tmpbuf[1] = (char)toCTRL(PL_multi_close);
	tmpbuf[2] = '\0';
	s = tmpbuf;
        len = 2;
    }
    else {
	if (LIKELY(PL_multi_close < 256)) {
	    *tmpbuf = (char)PL_multi_close;
	    tmpbuf[1] = '\0';
            len = 1;
	}
	else {
            char *end = (char *)uvchr_to_utf8((U8 *)tmpbuf, PL_multi_close);
            *end = '\0';
            len = end - tmpbuf;
	    uni = TRUE;
	}
	s = tmpbuf;
    }
    q = memchr(s, '"', len) ? '\'' : '"';
    sv = sv_2mortal(newSVpvn(s, len));
    if (uni)
	SvUTF8_on(sv);
    Perl_croak(aTHX_ "Can't find string terminator %c%" SVf "%c"
                     " anywhere before EOF", q, SVfARG(sv), q);
}

#include "feature.h"

/*
 * Check whether the named feature is enabled.
 */
bool
Perl_feature_is_enabled(pTHX_ const char *const name, STRLEN namelen)
{
    char he_name[8 + MAX_FEATURE_LEN] = "feature_";

    PERL_ARGS_ASSERT_FEATURE_IS_ENABLED;

    assert(CURRENT_FEATURE_BUNDLE == FEATURE_BUNDLE_CUSTOM);

    if (namelen > MAX_FEATURE_LEN)
	return FALSE;
    memcpy(&he_name[8], name, namelen);

    return cBOOL(cop_hints_fetch_pvn(PL_curcop, he_name, 8 + namelen, 0,
				     REFCOUNTED_HE_EXISTS));
}

/*
 * experimental text filters for win32 carriage-returns, utf16-to-utf8 and
 * utf16-to-utf8-reversed.
 */

#ifdef PERL_CR_FILTER
static void
strip_return(SV *sv)
{
    const char *s = SvPVX_const(sv);
    const char * const e = s + SvCUR(sv);

    PERL_ARGS_ASSERT_STRIP_RETURN;

    /* outer loop optimized to do nothing if there are no CR-LFs */
    while (s < e) {
	if (*s++ == '\r' && *s == '\n') {
	    /* hit a CR-LF, need to copy the rest */
	    char *d = s - 1;
	    *d++ = *s++;
	    while (s < e) {
		if (*s == '\r' && s[1] == '\n')
		    s++;
		*d++ = *s++;
	    }
	    SvCUR(sv) -= s - d;
	    return;
	}
    }
}

STATIC I32
S_cr_textfilter(pTHX_ int idx, SV *sv, int maxlen)
{
    const I32 count = FILTER_READ(idx+1, sv, maxlen);
    if (count > 0 && !maxlen)
	strip_return(sv);
    return count;
}
#endif

/*
=for apidoc Amx|void|lex_start|SV *line|PerlIO *rsfp|U32 flags

Creates and initialises a new lexer/parser state object, supplying
a context in which to lex and parse from a new source of Perl code.
A pointer to the new state object is placed in L</PL_parser>.  An entry
is made on the save stack so that upon unwinding, the new state object
will be destroyed and the former value of L</PL_parser> will be restored.
Nothing else need be done to clean up the parsing context.

The code to be parsed comes from C<line> and C<rsfp>.  C<line>, if
non-null, provides a string (in SV form) containing code to be parsed.
A copy of the string is made, so subsequent modification of C<line>
does not affect parsing.  C<rsfp>, if non-null, provides an input stream
from which code will be read to be parsed.  If both are non-null, the
code in C<line> comes first and must consist of complete lines of input,
and C<rsfp> supplies the remainder of the source.

The C<flags> parameter is reserved for future use.  Currently it is only
used by perl internally, so extensions should always pass zero.

=cut
*/

/* LEX_START_SAME_FILTER indicates that this is not a new file, so it
   can share filters with the current parser.
   LEX_START_DONT_CLOSE indicates that the file handle wasn't opened by the
   caller, hence isn't owned by the parser, so shouldn't be closed on parser
   destruction. This is used to handle the case of defaulting to reading the
   script from the standard input because no filename was given on the command
   line (without getting confused by situation where STDIN has been closed, so
   the script handle is opened on fd 0)  */

void
Perl_lex_start(pTHX_ SV *line, PerlIO *rsfp, U32 flags)
{
    const char *s = NULL;
    yy_parser *parser, *oparser;

    if (flags && flags & ~LEX_START_FLAGS)
	Perl_croak(aTHX_ "Lexing code internal error (%s)", "lex_start");

    /* create and initialise a parser */

    Newxz(parser, 1, yy_parser);
    parser->old_parser = oparser = PL_parser;
    PL_parser = parser;

    parser->stack = NULL;
    parser->stack_max1 = NULL;
    parser->ps = NULL;

    /* on scope exit, free this parser and restore any outer one */
    SAVEPARSER(parser);
    parser->saved_curcop = PL_curcop;

    /* initialise lexer state */

    parser->nexttoke = 0;
    parser->error_count = oparser ? oparser->error_count : 0;
    parser->copline = parser->preambling = NOLINE;
    parser->lex_state = LEX_NORMAL;
    parser->expect = XSTATE;
    parser->rsfp = rsfp;
    parser->recheck_utf8_validity = FALSE;
    parser->rsfp_filters =
      !(flags & LEX_START_SAME_FILTER) || !oparser
        ? NULL
        : MUTABLE_AV(SvREFCNT_inc(
            oparser->rsfp_filters
             ? oparser->rsfp_filters
             : (oparser->rsfp_filters = newAV())
          ));

    Newx(parser->lex_brackstack, 120, char);
    Newx(parser->lex_casestack, 12, char);
    *parser->lex_casestack = '\0';
    Newxz(parser->lex_shared, 1, LEXSHARED);

    if (line) {
	STRLEN len;
        const U8* first_bad_char_loc;

	s = SvPV_const(line, len);

        if (   SvUTF8(line)
            && UNLIKELY(! is_utf8_string_loc((U8 *) s,
                                             SvCUR(line),
                                             &first_bad_char_loc)))
        {
            _force_out_malformed_utf8_message(first_bad_char_loc,
                                              (U8 *) s + SvCUR(line),
                                              0,
                                              1 /* 1 means die */ );
            NOT_REACHED; /* NOTREACHED */
        }

	parser->linestr = flags & LEX_START_COPIED
			    ? SvREFCNT_inc_simple_NN(line)
			    : newSVpvn_flags(s, len, SvUTF8(line));
	if (!rsfp)
	    sv_catpvs(parser->linestr, "\n;");
    } else {
	parser->linestr = newSVpvn("\n;", rsfp ? 1 : 2);
    }

    parser->oldoldbufptr =
	parser->oldbufptr =
	parser->bufptr =
	parser->linestart = SvPVX(parser->linestr);
    parser->bufend = parser->bufptr + SvCUR(parser->linestr);
    parser->last_lop = parser->last_uni = NULL;

    STATIC_ASSERT_STMT(FITS_IN_8_BITS(LEX_IGNORE_UTF8_HINTS|LEX_EVALBYTES
                                                        |LEX_DONT_CLOSE_RSFP));
    parser->lex_flags = (U8) (flags & (LEX_IGNORE_UTF8_HINTS|LEX_EVALBYTES
                                                        |LEX_DONT_CLOSE_RSFP));

    parser->in_pod = parser->filtered = 0;
}


/* delete a parser object */

void
Perl_parser_free(pTHX_  const yy_parser *parser)
{
    PERL_ARGS_ASSERT_PARSER_FREE;

    PL_curcop = parser->saved_curcop;
    SvREFCNT_dec(parser->linestr);

    if (PL_parser->lex_flags & LEX_DONT_CLOSE_RSFP)
	PerlIO_clearerr(parser->rsfp);
    else if (parser->rsfp && (!parser->old_parser
          || (parser->old_parser && parser->rsfp != parser->old_parser->rsfp)))
	PerlIO_close(parser->rsfp);
    SvREFCNT_dec(parser->rsfp_filters);
    SvREFCNT_dec(parser->lex_stuff);
    SvREFCNT_dec(parser->lex_sub_repl);

    Safefree(parser->lex_brackstack);
    Safefree(parser->lex_casestack);
    Safefree(parser->lex_shared);
    PL_parser = parser->old_parser;
    Safefree(parser);
}

void
Perl_parser_free_nexttoke_ops(pTHX_  yy_parser *parser, OPSLAB *slab)
{
    I32 nexttoke = parser->nexttoke;
    PERL_ARGS_ASSERT_PARSER_FREE_NEXTTOKE_OPS;
    while (nexttoke--) {
	if (S_is_opval_token(parser->nexttype[nexttoke] & 0xffff)
	 && parser->nextval[nexttoke].opval
	 && parser->nextval[nexttoke].opval->op_slabbed
	 && OpSLAB(parser->nextval[nexttoke].opval) == slab) {
	    op_free(parser->nextval[nexttoke].opval);
	    parser->nextval[nexttoke].opval = NULL;
	}
    }
}


/*
=for apidoc AmxU|SV *|PL_parser-E<gt>linestr

Buffer scalar containing the chunk currently under consideration of the
text currently being lexed.  This is always a plain string scalar (for
which C<SvPOK> is true).  It is not intended to be used as a scalar by
normal scalar means; instead refer to the buffer directly by the pointer
variables described below.

The lexer maintains various C<char*> pointers to things in the
C<PL_parser-E<gt>linestr> buffer.  If C<PL_parser-E<gt>linestr> is ever
reallocated, all of these pointers must be updated.  Don't attempt to
do this manually, but rather use L</lex_grow_linestr> if you need to
reallocate the buffer.

The content of the text chunk in the buffer is commonly exactly one
complete line of input, up to and including a newline terminator,
but there are situations where it is otherwise.  The octets of the
buffer may be intended to be interpreted as either UTF-8 or Latin-1.
The function L</lex_bufutf8> tells you which.  Do not use the C<SvUTF8>
flag on this scalar, which may disagree with it.

For direct examination of the buffer, the variable
L</PL_parser-E<gt>bufend> points to the end of the buffer.  The current
lexing position is pointed to by L</PL_parser-E<gt>bufptr>.  Direct use
of these pointers is usually preferable to examination of the scalar
through normal scalar means.

=for apidoc AmxU|char *|PL_parser-E<gt>bufend

Direct pointer to the end of the chunk of text currently being lexed, the
end of the lexer buffer.  This is equal to C<SvPVX(PL_parser-E<gt>linestr)
+ SvCUR(PL_parser-E<gt>linestr)>.  A C<NUL> character (zero octet) is
always located at the end of the buffer, and does not count as part of
the buffer's contents.

=for apidoc AmxU|char *|PL_parser-E<gt>bufptr

Points to the current position of lexing inside the lexer buffer.
Characters around this point may be freely examined, within
the range delimited by C<SvPVX(L</PL_parser-E<gt>linestr>)> and
L</PL_parser-E<gt>bufend>.  The octets of the buffer may be intended to be
interpreted as either UTF-8 or Latin-1, as indicated by L</lex_bufutf8>.

Lexing code (whether in the Perl core or not) moves this pointer past
the characters that it consumes.  It is also expected to perform some
bookkeeping whenever a newline character is consumed.  This movement
can be more conveniently performed by the function L</lex_read_to>,
which handles newlines appropriately.

Interpretation of the buffer's octets can be abstracted out by
using the slightly higher-level functions L</lex_peek_unichar> and
L</lex_read_unichar>.

=for apidoc AmxU|char *|PL_parser-E<gt>linestart

Points to the start of the current line inside the lexer buffer.
This is useful for indicating at which column an error occurred, and
not much else.  This must be updated by any lexing code that consumes
a newline; the function L</lex_read_to> handles this detail.

=cut
*/

/*
=for apidoc Amx|bool|lex_bufutf8

Indicates whether the octets in the lexer buffer
(L</PL_parser-E<gt>linestr>) should be interpreted as the UTF-8 encoding
of Unicode characters.  If not, they should be interpreted as Latin-1
characters.  This is analogous to the C<SvUTF8> flag for scalars.

In UTF-8 mode, it is not guaranteed that the lexer buffer actually
contains valid UTF-8.  Lexing code must be robust in the face of invalid
encoding.

The actual C<SvUTF8> flag of the L</PL_parser-E<gt>linestr> scalar
is significant, but not the whole story regarding the input character
encoding.  Normally, when a file is being read, the scalar contains octets
and its C<SvUTF8> flag is off, but the octets should be interpreted as
UTF-8 if the C<use utf8> pragma is in effect.  During a string eval,
however, the scalar may have the C<SvUTF8> flag on, and in this case its
octets should be interpreted as UTF-8 unless the C<use bytes> pragma
is in effect.  This logic may change in the future; use this function
instead of implementing the logic yourself.

=cut
*/

bool
Perl_lex_bufutf8(pTHX)
{
    return UTF;
}

/*
=for apidoc Amx|char *|lex_grow_linestr|STRLEN len

Reallocates the lexer buffer (L</PL_parser-E<gt>linestr>) to accommodate
at least C<len> octets (including terminating C<NUL>).  Returns a
pointer to the reallocated buffer.  This is necessary before making
any direct modification of the buffer that would increase its length.
L</lex_stuff_pvn> provides a more convenient way to insert text into
the buffer.

Do not use C<SvGROW> or C<sv_grow> directly on C<PL_parser-E<gt>linestr>;
this function updates all of the lexer's variables that point directly
into the buffer.

=cut
*/

char *
Perl_lex_grow_linestr(pTHX_ STRLEN len)
{
    SV *linestr;
    char *buf;
    STRLEN bufend_pos, bufptr_pos, oldbufptr_pos, oldoldbufptr_pos;
    STRLEN linestart_pos, last_uni_pos, last_lop_pos, re_eval_start_pos;
    bool current;

    linestr = PL_parser->linestr;
    buf = SvPVX(linestr);
    if (len <= SvLEN(linestr))
	return buf;

    /* Is the lex_shared linestr SV the same as the current linestr SV?
     * Only in this case does re_eval_start need adjusting, since it
     * points within lex_shared->ls_linestr's buffer */
    current = (   !PL_parser->lex_shared->ls_linestr
               || linestr == PL_parser->lex_shared->ls_linestr);

    bufend_pos = PL_parser->bufend - buf;
    bufptr_pos = PL_parser->bufptr - buf;
    oldbufptr_pos = PL_parser->oldbufptr - buf;
    oldoldbufptr_pos = PL_parser->oldoldbufptr - buf;
    linestart_pos = PL_parser->linestart - buf;
    last_uni_pos = PL_parser->last_uni ? PL_parser->last_uni - buf : 0;
    last_lop_pos = PL_parser->last_lop ? PL_parser->last_lop - buf : 0;
    re_eval_start_pos = (current && PL_parser->lex_shared->re_eval_start) ?
                            PL_parser->lex_shared->re_eval_start - buf : 0;

    buf = sv_grow(linestr, len);

    PL_parser->bufend = buf + bufend_pos;
    PL_parser->bufptr = buf + bufptr_pos;
    PL_parser->oldbufptr = buf + oldbufptr_pos;
    PL_parser->oldoldbufptr = buf + oldoldbufptr_pos;
    PL_parser->linestart = buf + linestart_pos;
    if (PL_parser->last_uni)
	PL_parser->last_uni = buf + last_uni_pos;
    if (PL_parser->last_lop)
	PL_parser->last_lop = buf + last_lop_pos;
    if (current && PL_parser->lex_shared->re_eval_start)
        PL_parser->lex_shared->re_eval_start  = buf + re_eval_start_pos;
    return buf;
}

/*
=for apidoc Amx|void|lex_stuff_pvn|const char *pv|STRLEN len|U32 flags

Insert characters into the lexer buffer (L</PL_parser-E<gt>linestr>),
immediately after the current lexing point (L</PL_parser-E<gt>bufptr>),
reallocating the buffer if necessary.  This means that lexing code that
runs later will see the characters as if they had appeared in the input.
It is not recommended to do this as part of normal parsing, and most
uses of this facility run the risk of the inserted characters being
interpreted in an unintended manner.

The string to be inserted is represented by C<len> octets starting
at C<pv>.  These octets are interpreted as either UTF-8 or Latin-1,
according to whether the C<LEX_STUFF_UTF8> flag is set in C<flags>.
The characters are recoded for the lexer buffer, according to how the
buffer is currently being interpreted (L</lex_bufutf8>).  If a string
to be inserted is available as a Perl scalar, the L</lex_stuff_sv>
function is more convenient.

=cut
*/

void
Perl_lex_stuff_pvn(pTHX_ const char *pv, STRLEN len, U32 flags)
{
    dVAR;
    char *bufptr;
    PERL_ARGS_ASSERT_LEX_STUFF_PVN;
    if (flags & ~(LEX_STUFF_UTF8))
	Perl_croak(aTHX_ "Lexing code internal error (%s)", "lex_stuff_pvn");
    if (UTF) {
	if (flags & LEX_STUFF_UTF8) {
	    goto plain_copy;
	} else {
	    STRLEN highhalf = 0;    /* Count of variants */
	    const char *p, *e = pv+len;
	    for (p = pv; p != e; p++) {
		if (! UTF8_IS_INVARIANT(*p)) {
                    highhalf++;
                }
            }
	    if (!highhalf)
		goto plain_copy;
	    lex_grow_linestr(SvCUR(PL_parser->linestr)+1+len+highhalf);
	    bufptr = PL_parser->bufptr;
	    Move(bufptr, bufptr+len+highhalf, PL_parser->bufend+1-bufptr, char);
	    SvCUR_set(PL_parser->linestr,
	    	SvCUR(PL_parser->linestr) + len+highhalf);
	    PL_parser->bufend += len+highhalf;
	    for (p = pv; p != e; p++) {
                append_utf8_from_native_byte(*p, (U8 **) &bufptr);
	    }
	}
    } else {
	if (flags & LEX_STUFF_UTF8) {
	    STRLEN highhalf = 0;
	    const char *p, *e = pv+len;
	    for (p = pv; p != e; p++) {
		U8 c = (U8)*p;
		if (UTF8_IS_ABOVE_LATIN1(c)) {
		    Perl_croak(aTHX_ "Lexing code attempted to stuff "
				"non-Latin-1 character into Latin-1 input");
		} else if (UTF8_IS_NEXT_CHAR_DOWNGRADEABLE(p, e)) {
		    p++;
		    highhalf++;
                } else assert(UTF8_IS_INVARIANT(c));
	    }
	    if (!highhalf)
		goto plain_copy;
	    lex_grow_linestr(SvCUR(PL_parser->linestr)+1+len-highhalf);
	    bufptr = PL_parser->bufptr;
	    Move(bufptr, bufptr+len-highhalf, PL_parser->bufend+1-bufptr, char);
	    SvCUR_set(PL_parser->linestr,
	    	SvCUR(PL_parser->linestr) + len-highhalf);
	    PL_parser->bufend += len-highhalf;
	    p = pv;
	    while (p < e) {
		if (UTF8_IS_INVARIANT(*p)) {
		    *bufptr++ = *p;
                    p++;
		}
		else {
                    assert(p < e -1 );
		    *bufptr++ = EIGHT_BIT_UTF8_TO_NATIVE(*p, *(p+1));
		    p += 2;
                }
	    }
	} else {
	  plain_copy:
	    lex_grow_linestr(SvCUR(PL_parser->linestr)+1+len);
	    bufptr = PL_parser->bufptr;
	    Move(bufptr, bufptr+len, PL_parser->bufend+1-bufptr, char);
	    SvCUR_set(PL_parser->linestr, SvCUR(PL_parser->linestr) + len);
	    PL_parser->bufend += len;
	    Copy(pv, bufptr, len, char);
	}
    }
}

/*
=for apidoc Amx|void|lex_stuff_pv|const char *pv|U32 flags

Insert characters into the lexer buffer (L</PL_parser-E<gt>linestr>),
immediately after the current lexing point (L</PL_parser-E<gt>bufptr>),
reallocating the buffer if necessary.  This means that lexing code that
runs later will see the characters as if they had appeared in the input.
It is not recommended to do this as part of normal parsing, and most
uses of this facility run the risk of the inserted characters being
interpreted in an unintended manner.

The string to be inserted is represented by octets starting at C<pv>
and continuing to the first nul.  These octets are interpreted as either
UTF-8 or Latin-1, according to whether the C<LEX_STUFF_UTF8> flag is set
in C<flags>.  The characters are recoded for the lexer buffer, according
to how the buffer is currently being interpreted (L</lex_bufutf8>).
If it is not convenient to nul-terminate a string to be inserted, the
L</lex_stuff_pvn> function is more appropriate.

=cut
*/

void
Perl_lex_stuff_pv(pTHX_ const char *pv, U32 flags)
{
    PERL_ARGS_ASSERT_LEX_STUFF_PV;
    lex_stuff_pvn(pv, strlen(pv), flags);
}

/*
=for apidoc Amx|void|lex_stuff_sv|SV *sv|U32 flags

Insert characters into the lexer buffer (L</PL_parser-E<gt>linestr>),
immediately after the current lexing point (L</PL_parser-E<gt>bufptr>),
reallocating the buffer if necessary.  This means that lexing code that
runs later will see the characters as if they had appeared in the input.
It is not recommended to do this as part of normal parsing, and most
uses of this facility run the risk of the inserted characters being
interpreted in an unintended manner.

The string to be inserted is the string value of C<sv>.  The characters
are recoded for the lexer buffer, according to how the buffer is currently
being interpreted (L</lex_bufutf8>).  If a string to be inserted is
not already a Perl scalar, the L</lex_stuff_pvn> function avoids the
need to construct a scalar.

=cut
*/

void
Perl_lex_stuff_sv(pTHX_ SV *sv, U32 flags)
{
    char *pv;
    STRLEN len;
    PERL_ARGS_ASSERT_LEX_STUFF_SV;
    if (flags)
	Perl_croak(aTHX_ "Lexing code internal error (%s)", "lex_stuff_sv");
    pv = SvPV(sv, len);
    lex_stuff_pvn(pv, len, flags | (SvUTF8(sv) ? LEX_STUFF_UTF8 : 0));
}

/*
=for apidoc Amx|void|lex_unstuff|char *ptr

Discards text about to be lexed, from L</PL_parser-E<gt>bufptr> up to
C<ptr>.  Text following C<ptr> will be moved, and the buffer shortened.
This hides the discarded text from any lexing code that runs later,
as if the text had never appeared.

This is not the normal way to consume lexed text.  For that, use
L</lex_read_to>.

=cut
*/

void
Perl_lex_unstuff(pTHX_ char *ptr)
{
    char *buf, *bufend;
    STRLEN unstuff_len;
    PERL_ARGS_ASSERT_LEX_UNSTUFF;
    buf = PL_parser->bufptr;
    if (ptr < buf)
	Perl_croak(aTHX_ "Lexing code internal error (%s)", "lex_unstuff");
    if (ptr == buf)
	return;
    bufend = PL_parser->bufend;
    if (ptr > bufend)
	Perl_croak(aTHX_ "Lexing code internal error (%s)", "lex_unstuff");
    unstuff_len = ptr - buf;
    Move(ptr, buf, bufend+1-ptr, char);
    SvCUR_set(PL_parser->linestr, SvCUR(PL_parser->linestr) - unstuff_len);
    PL_parser->bufend = bufend - unstuff_len;
}

/*
=for apidoc Amx|void|lex_read_to|char *ptr

Consume text in the lexer buffer, from L</PL_parser-E<gt>bufptr> up
to C<ptr>.  This advances L</PL_parser-E<gt>bufptr> to match C<ptr>,
performing the correct bookkeeping whenever a newline character is passed.
This is the normal way to consume lexed text.

Interpretation of the buffer's octets can be abstracted out by
using the slightly higher-level functions L</lex_peek_unichar> and
L</lex_read_unichar>.

=cut
*/

void
Perl_lex_read_to(pTHX_ char *ptr)
{
    char *s;
    PERL_ARGS_ASSERT_LEX_READ_TO;
    s = PL_parser->bufptr;
    if (ptr < s || ptr > PL_parser->bufend)
	Perl_croak(aTHX_ "Lexing code internal error (%s)", "lex_read_to");
    for (; s != ptr; s++)
	if (*s == '\n') {
	    COPLINE_INC_WITH_HERELINES;
	    PL_parser->linestart = s+1;
	}
    PL_parser->bufptr = ptr;
}

/*
=for apidoc Amx|void|lex_discard_to|char *ptr

Discards the first part of the L</PL_parser-E<gt>linestr> buffer,
up to C<ptr>.  The remaining content of the buffer will be moved, and
all pointers into the buffer updated appropriately.  C<ptr> must not
be later in the buffer than the position of L</PL_parser-E<gt>bufptr>:
it is not permitted to discard text that has yet to be lexed.

Normally it is not necessarily to do this directly, because it suffices to
use the implicit discarding behaviour of L</lex_next_chunk> and things
based on it.  However, if a token stretches across multiple lines,
and the lexing code has kept multiple lines of text in the buffer for
that purpose, then after completion of the token it would be wise to
explicitly discard the now-unneeded earlier lines, to avoid future
multi-line tokens growing the buffer without bound.

=cut
*/

void
Perl_lex_discard_to(pTHX_ char *ptr)
{
    char *buf;
    STRLEN discard_len;
    PERL_ARGS_ASSERT_LEX_DISCARD_TO;
    buf = SvPVX(PL_parser->linestr);
    if (ptr < buf)
	Perl_croak(aTHX_ "Lexing code internal error (%s)", "lex_discard_to");
    if (ptr == buf)
	return;
    if (ptr > PL_parser->bufptr)
	Perl_croak(aTHX_ "Lexing code internal error (%s)", "lex_discard_to");
    discard_len = ptr - buf;
    if (PL_parser->oldbufptr < ptr)
	PL_parser->oldbufptr = ptr;
    if (PL_parser->oldoldbufptr < ptr)
	PL_parser->oldoldbufptr = ptr;
    if (PL_parser->last_uni && PL_parser->last_uni < ptr)
	PL_parser->last_uni = NULL;
    if (PL_parser->last_lop && PL_parser->last_lop < ptr)
	PL_parser->last_lop = NULL;
    Move(ptr, buf, PL_parser->bufend+1-ptr, char);
    SvCUR_set(PL_parser->linestr, SvCUR(PL_parser->linestr) - discard_len);
    PL_parser->bufend -= discard_len;
    PL_parser->bufptr -= discard_len;
    PL_parser->oldbufptr -= discard_len;
    PL_parser->oldoldbufptr -= discard_len;
    if (PL_parser->last_uni)
	PL_parser->last_uni -= discard_len;
    if (PL_parser->last_lop)
	PL_parser->last_lop -= discard_len;
}

void
Perl_notify_parser_that_changed_to_utf8(pTHX)
{
    /* Called when $^H is changed to indicate that HINT_UTF8 has changed from
     * off to on.  At compile time, this has the effect of entering a 'use
     * utf8' section.  This means that any input was not previously checked for
     * UTF-8 (because it was off), but now we do need to check it, or our
     * assumptions about the input being sane could be wrong, and we could
     * segfault.  This routine just sets a flag so that the next time we look
     * at the input we do the well-formed UTF-8 check.  If we aren't in the
     * proper phase, there may not be a parser object, but if there is, setting
     * the flag is harmless */

    if (PL_parser) {
        PL_parser->recheck_utf8_validity = TRUE;
    }
}

/*
=for apidoc Amx|bool|lex_next_chunk|U32 flags

Reads in the next chunk of text to be lexed, appending it to
L</PL_parser-E<gt>linestr>.  This should be called when lexing code has
looked to the end of the current chunk and wants to know more.  It is
usual, but not necessary, for lexing to have consumed the entirety of
the current chunk at this time.

If L</PL_parser-E<gt>bufptr> is pointing to the very end of the current
chunk (i.e., the current chunk has been entirely consumed), normally the
current chunk will be discarded at the same time that the new chunk is
read in.  If C<flags> has the C<LEX_KEEP_PREVIOUS> bit set, the current chunk
will not be discarded.  If the current chunk has not been entirely
consumed, then it will not be discarded regardless of the flag.

Returns true if some new text was added to the buffer, or false if the
buffer has reached the end of the input text.

=cut
*/

#define LEX_FAKE_EOF 0x80000000
#define LEX_NO_TERM  0x40000000 /* here-doc */

bool
Perl_lex_next_chunk(pTHX_ U32 flags)
{
    SV *linestr;
    char *buf;
    STRLEN old_bufend_pos, new_bufend_pos;
    STRLEN bufptr_pos, oldbufptr_pos, oldoldbufptr_pos;
    STRLEN linestart_pos, last_uni_pos, last_lop_pos;
    bool got_some_for_debugger = 0;
    bool got_some;

    if (flags & ~(LEX_KEEP_PREVIOUS|LEX_FAKE_EOF|LEX_NO_TERM))
	Perl_croak(aTHX_ "Lexing code internal error (%s)", "lex_next_chunk");
    if (!(flags & LEX_NO_TERM) && PL_lex_inwhat)
	return FALSE;
    linestr = PL_parser->linestr;
    buf = SvPVX(linestr);
    if (!(flags & LEX_KEEP_PREVIOUS)
          && PL_parser->bufptr == PL_parser->bufend)
    {
	old_bufend_pos = bufptr_pos = oldbufptr_pos = oldoldbufptr_pos = 0;
	linestart_pos = 0;
	if (PL_parser->last_uni != PL_parser->bufend)
	    PL_parser->last_uni = NULL;
	if (PL_parser->last_lop != PL_parser->bufend)
	    PL_parser->last_lop = NULL;
	last_uni_pos = last_lop_pos = 0;
	*buf = 0;
	SvCUR(linestr) = 0;
    } else {
	old_bufend_pos = PL_parser->bufend - buf;
	bufptr_pos = PL_parser->bufptr - buf;
	oldbufptr_pos = PL_parser->oldbufptr - buf;
	oldoldbufptr_pos = PL_parser->oldoldbufptr - buf;
	linestart_pos = PL_parser->linestart - buf;
	last_uni_pos = PL_parser->last_uni ? PL_parser->last_uni - buf : 0;
	last_lop_pos = PL_parser->last_lop ? PL_parser->last_lop - buf : 0;
    }
    if (flags & LEX_FAKE_EOF) {
	goto eof;
    } else if (!PL_parser->rsfp && !PL_parser->filtered) {
	got_some = 0;
    } else if (filter_gets(linestr, old_bufend_pos)) {
	got_some = 1;
	got_some_for_debugger = 1;
    } else if (flags & LEX_NO_TERM) {
	got_some = 0;
    } else {
	if (!SvPOK(linestr))   /* can get undefined by filter_gets */
            SvPVCLEAR(linestr);
	eof:
	/* End of real input.  Close filehandle (unless it was STDIN),
	 * then add implicit termination.
	 */
	if (PL_parser->lex_flags & LEX_DONT_CLOSE_RSFP)
	    PerlIO_clearerr(PL_parser->rsfp);
	else if (PL_parser->rsfp)
	    (void)PerlIO_close(PL_parser->rsfp);
	PL_parser->rsfp = NULL;
	PL_parser->in_pod = PL_parser->filtered = 0;
	if (!PL_in_eval && PL_minus_p) {
	    sv_catpvs(linestr,
		/*{*/";}continue{print or die qq(-p destination: $!\\n);}");
	    PL_minus_n = PL_minus_p = 0;
	} else if (!PL_in_eval && PL_minus_n) {
	    sv_catpvs(linestr, /*{*/";}");
	    PL_minus_n = 0;
	} else
	    sv_catpvs(linestr, ";");
	got_some = 1;
    }
    buf = SvPVX(linestr);
    new_bufend_pos = SvCUR(linestr);
    PL_parser->bufend = buf + new_bufend_pos;
    PL_parser->bufptr = buf + bufptr_pos;

    if (UTF) {
        const U8* first_bad_char_loc;
        if (UNLIKELY(! is_utf8_string_loc(
                            (U8 *) PL_parser->bufptr,
                                   PL_parser->bufend - PL_parser->bufptr,
                                   &first_bad_char_loc)))
        {
            _force_out_malformed_utf8_message(first_bad_char_loc,
                                              (U8 *) PL_parser->bufend,
                                              0,
                                              1 /* 1 means die */ );
            NOT_REACHED; /* NOTREACHED */
        }
    }

    PL_parser->oldbufptr = buf + oldbufptr_pos;
    PL_parser->oldoldbufptr = buf + oldoldbufptr_pos;
    PL_parser->linestart = buf + linestart_pos;
    if (PL_parser->last_uni)
	PL_parser->last_uni = buf + last_uni_pos;
    if (PL_parser->last_lop)
	PL_parser->last_lop = buf + last_lop_pos;
    if (PL_parser->preambling != NOLINE) {
	CopLINE_set(PL_curcop, PL_parser->preambling + 1);
	PL_parser->preambling = NOLINE;
    }
    if (   got_some_for_debugger
        && PERLDB_LINE_OR_SAVESRC
        && PL_curstash != PL_debstash)
    {
	/* debugger active and we're not compiling the debugger code,
	 * so store the line into the debugger's array of lines
	 */
	update_debugger_info(NULL, buf+old_bufend_pos,
	    new_bufend_pos-old_bufend_pos);
    }
    return got_some;
}

/*
=for apidoc Amx|I32|lex_peek_unichar|U32 flags

Looks ahead one (Unicode) character in the text currently being lexed.
Returns the codepoint (unsigned integer value) of the next character,
or -1 if lexing has reached the end of the input text.  To consume the
peeked character, use L</lex_read_unichar>.

If the next character is in (or extends into) the next chunk of input
text, the next chunk will be read in.  Normally the current chunk will be
discarded at the same time, but if C<flags> has the C<LEX_KEEP_PREVIOUS>
bit set, then the current chunk will not be discarded.

If the input is being interpreted as UTF-8 and a UTF-8 encoding error
is encountered, an exception is generated.

=cut
*/

I32
Perl_lex_peek_unichar(pTHX_ U32 flags)
{
    dVAR;
    char *s, *bufend;
    if (flags & ~(LEX_KEEP_PREVIOUS))
	Perl_croak(aTHX_ "Lexing code internal error (%s)", "lex_peek_unichar");
    s = PL_parser->bufptr;
    bufend = PL_parser->bufend;
    if (UTF) {
	U8 head;
	I32 unichar;
	STRLEN len, retlen;
	if (s == bufend) {
	    if (!lex_next_chunk(flags))
		return -1;
	    s = PL_parser->bufptr;
	    bufend = PL_parser->bufend;
	}
	head = (U8)*s;
	if (UTF8_IS_INVARIANT(head))
	    return head;
	if (UTF8_IS_START(head)) {
	    len = UTF8SKIP(&head);
	    while ((STRLEN)(bufend-s) < len) {
		if (!lex_next_chunk(flags | LEX_KEEP_PREVIOUS))
		    break;
		s = PL_parser->bufptr;
		bufend = PL_parser->bufend;
	    }
	}
	unichar = utf8n_to_uvchr((U8*)s, bufend-s, &retlen, UTF8_CHECK_ONLY);
	if (retlen == (STRLEN)-1) {
            _force_out_malformed_utf8_message((U8 *) s,
                                              (U8 *) bufend,
                                              0,
                                              1 /* 1 means die */ );
            NOT_REACHED; /* NOTREACHED */
	}
	return unichar;
    } else {
	if (s == bufend) {
	    if (!lex_next_chunk(flags))
		return -1;
	    s = PL_parser->bufptr;
	}
	return (U8)*s;
    }
}

/*
=for apidoc Amx|I32|lex_read_unichar|U32 flags

Reads the next (Unicode) character in the text currently being lexed.
Returns the codepoint (unsigned integer value) of the character read,
and moves L</PL_parser-E<gt>bufptr> past the character, or returns -1
if lexing has reached the end of the input text.  To non-destructively
examine the next character, use L</lex_peek_unichar> instead.

If the next character is in (or extends into) the next chunk of input
text, the next chunk will be read in.  Normally the current chunk will be
discarded at the same time, but if C<flags> has the C<LEX_KEEP_PREVIOUS>
bit set, then the current chunk will not be discarded.

If the input is being interpreted as UTF-8 and a UTF-8 encoding error
is encountered, an exception is generated.

=cut
*/

I32
Perl_lex_read_unichar(pTHX_ U32 flags)
{
    I32 c;
    if (flags & ~(LEX_KEEP_PREVIOUS))
	Perl_croak(aTHX_ "Lexing code internal error (%s)", "lex_read_unichar");
    c = lex_peek_unichar(flags);
    if (c != -1) {
	if (c == '\n')
	    COPLINE_INC_WITH_HERELINES;
	if (UTF)
	    PL_parser->bufptr += UTF8SKIP(PL_parser->bufptr);
	else
	    ++(PL_parser->bufptr);
    }
    return c;
}

/*
=for apidoc Amx|void|lex_read_space|U32 flags

Reads optional spaces, in Perl style, in the text currently being
lexed.  The spaces may include ordinary whitespace characters and
Perl-style comments.  C<#line> directives are processed if encountered.
L</PL_parser-E<gt>bufptr> is moved past the spaces, so that it points
at a non-space character (or the end of the input text).

If spaces extend into the next chunk of input text, the next chunk will
be read in.  Normally the current chunk will be discarded at the same
time, but if C<flags> has the C<LEX_KEEP_PREVIOUS> bit set, then the current
chunk will not be discarded.

=cut
*/

#define LEX_NO_INCLINE    0x40000000
#define LEX_NO_NEXT_CHUNK 0x80000000

void
Perl_lex_read_space(pTHX_ U32 flags)
{
    char *s, *bufend;
    const bool can_incline = !(flags & LEX_NO_INCLINE);
    bool need_incline = 0;
    if (flags & ~(LEX_KEEP_PREVIOUS|LEX_NO_NEXT_CHUNK|LEX_NO_INCLINE))
	Perl_croak(aTHX_ "Lexing code internal error (%s)", "lex_read_space");
    s = PL_parser->bufptr;
    bufend = PL_parser->bufend;
    while (1) {
	char c = *s;
	if (c == '#') {
	    do {
		c = *++s;
	    } while (!(c == '\n' || (c == 0 && s == bufend)));
	} else if (c == '\n') {
	    s++;
	    if (can_incline) {
		PL_parser->linestart = s;
		if (s == bufend)
		    need_incline = 1;
		else
		    incline(s, bufend);
	    }
	} else if (isSPACE(c)) {
	    s++;
	} else if (c == 0 && s == bufend) {
	    bool got_more;
	    line_t l;
	    if (flags & LEX_NO_NEXT_CHUNK)
		break;
	    PL_parser->bufptr = s;
	    l = CopLINE(PL_curcop);
	    CopLINE(PL_curcop) += PL_parser->herelines + 1;
	    got_more = lex_next_chunk(flags);
	    CopLINE_set(PL_curcop, l);
	    s = PL_parser->bufptr;
	    bufend = PL_parser->bufend;
	    if (!got_more)
		break;
	    if (can_incline && need_incline && PL_parser->rsfp) {
		incline(s, bufend);
		need_incline = 0;
	    }
	} else if (!c) {
	    s++;
	} else {
	    break;
	}
    }
    PL_parser->bufptr = s;
}

/*

=for apidoc EXMp|bool|validate_proto|SV *name|SV *proto|bool warn

This function performs syntax checking on a prototype, C<proto>.
If C<warn> is true, any illegal characters or mismatched brackets
will trigger illegalproto warnings, declaring that they were
detected in the prototype for C<name>.

The return value is C<true> if this is a valid prototype, and
C<false> if it is not, regardless of whether C<warn> was C<true> or
C<false>.

Note that C<NULL> is a valid C<proto> and will always return C<true>.

=cut

 */

bool
Perl_validate_proto(pTHX_ SV *name, SV *proto, bool warn, bool curstash)
{
    STRLEN len, origlen;
    char *p;
    bool bad_proto = FALSE;
    bool in_brackets = FALSE;
    bool after_slash = FALSE;
    char greedy_proto = ' ';
    bool proto_after_greedy_proto = FALSE;
    bool must_be_last = FALSE;
    bool underscore = FALSE;
    bool bad_proto_after_underscore = FALSE;

    PERL_ARGS_ASSERT_VALIDATE_PROTO;

    if (!proto)
	return TRUE;

    p = SvPV(proto, len);
    origlen = len;
    for (; len--; p++) {
	if (!isSPACE(*p)) {
	    if (must_be_last)
		proto_after_greedy_proto = TRUE;
	    if (underscore) {
		if (!strchr(";@%", *p))
		    bad_proto_after_underscore = TRUE;
		underscore = FALSE;
	    }
	    if (!strchr("$@%*;[]&\\_+", *p) || *p == '\0') {
		bad_proto = TRUE;
	    }
	    else {
		if (*p == '[')
		    in_brackets = TRUE;
		else if (*p == ']')
		    in_brackets = FALSE;
		else if ((*p == '@' || *p == '%')
                         && !after_slash
                         && !in_brackets )
                {
		    must_be_last = TRUE;
		    greedy_proto = *p;
		}
		else if (*p == '_')
		    underscore = TRUE;
	    }
	    if (*p == '\\')
		after_slash = TRUE;
	    else
		after_slash = FALSE;
	}
    }

    if (warn) {
	SV *tmpsv = newSVpvs_flags("", SVs_TEMP);
	p -= origlen;
	p = SvUTF8(proto)
	    ? sv_uni_display(tmpsv, newSVpvn_flags(p, origlen, SVs_TEMP | SVf_UTF8),
	                     origlen, UNI_DISPLAY_ISPRINT)
	    : pv_pretty(tmpsv, p, origlen, 60, NULL, NULL, PERL_PV_ESCAPE_NONASCII);

	if (curstash && !memchr(SvPVX(name), ':', SvCUR(name))) {
	    SV *name2 = sv_2mortal(newSVsv(PL_curstname));
	    sv_catpvs(name2, "::");
	    sv_catsv(name2, (SV *)name);
	    name = name2;
	}

	if (proto_after_greedy_proto)
	    Perl_warner(aTHX_ packWARN(WARN_ILLEGALPROTO),
			"Prototype after '%c' for %" SVf " : %s",
			greedy_proto, SVfARG(name), p);
	if (in_brackets)
	    Perl_warner(aTHX_ packWARN(WARN_ILLEGALPROTO),
			"Missing ']' in prototype for %" SVf " : %s",
			SVfARG(name), p);
	if (bad_proto)
	    Perl_warner(aTHX_ packWARN(WARN_ILLEGALPROTO),
			"Illegal character in prototype for %" SVf " : %s",
			SVfARG(name), p);
	if (bad_proto_after_underscore)
	    Perl_warner(aTHX_ packWARN(WARN_ILLEGALPROTO),
			"Illegal character after '_' in prototype for %" SVf " : %s",
			SVfARG(name), p);
    }

    return (! (proto_after_greedy_proto || bad_proto) );
}

/*
 * S_incline
 * This subroutine has nothing to do with tilting, whether at windmills
 * or pinball tables.  Its name is short for "increment line".  It
 * increments the current line number in CopLINE(PL_curcop) and checks
 * to see whether the line starts with a comment of the form
 *    # line 500 "foo.pm"
 * If so, it sets the current line number and file to the values in the comment.
 */

STATIC void
S_incline(pTHX_ const char *s, const char *end)
{
    const char *t;
    const char *n;
    const char *e;
    line_t line_num;
    UV uv;

    PERL_ARGS_ASSERT_INCLINE;

    assert(end >= s);

    COPLINE_INC_WITH_HERELINES;
    if (!PL_rsfp && !PL_parser->filtered && PL_lex_state == LEX_NORMAL
     && s+1 == PL_bufend && *s == ';') {
	/* fake newline in string eval */
	CopLINE_dec(PL_curcop);
	return;
    }
    if (*s++ != '#')
	return;
    while (SPACE_OR_TAB(*s))
	s++;
    if (memBEGINs(s, (STRLEN) (end - s), "line"))
	s += sizeof("line") - 1;
    else
	return;
    if (SPACE_OR_TAB(*s))
	s++;
    else
	return;
    while (SPACE_OR_TAB(*s))
	s++;
    if (!isDIGIT(*s))
	return;

    n = s;
    while (isDIGIT(*s))
	s++;
    if (!SPACE_OR_TAB(*s) && *s != '\r' && *s != '\n' && *s != '\0')
	return;
    while (SPACE_OR_TAB(*s))
	s++;
    if (*s == '"' && (t = (char *) memchr(s+1, '"', end - s))) {
	s++;
	e = t + 1;
    }
    else {
	t = s;
	while (*t && !isSPACE(*t))
	    t++;
	e = t;
    }
    while (SPACE_OR_TAB(*e) || *e == '\r' || *e == '\f')
	e++;
    if (*e != '\n' && *e != '\0')
	return;		/* false alarm */

    if (!grok_atoUV(n, &uv, &e))
        return;
    line_num = ((line_t)uv) - 1;

    if (t - s > 0) {
	const STRLEN len = t - s;

	if (!PL_rsfp && !PL_parser->filtered) {
	    /* must copy *{"::_<(eval N)[oldfilename:L]"}
	     * to *{"::_<newfilename"} */
	    /* However, the long form of evals is only turned on by the
	       debugger - usually they're "(eval %lu)" */
	    GV * const cfgv = CopFILEGV(PL_curcop);
	    if (cfgv) {
		char smallbuf[128];
		STRLEN tmplen2 = len;
		char *tmpbuf2;
		GV *gv2;

		if (tmplen2 + 2 <= sizeof smallbuf)
		    tmpbuf2 = smallbuf;
		else
		    Newx(tmpbuf2, tmplen2 + 2, char);

		tmpbuf2[0] = '_';
		tmpbuf2[1] = '<';

		memcpy(tmpbuf2 + 2, s, tmplen2);
		tmplen2 += 2;

		gv2 = *(GV**)hv_fetch(PL_defstash, tmpbuf2, tmplen2, TRUE);
		if (!isGV(gv2)) {
		    gv_init(gv2, PL_defstash, tmpbuf2, tmplen2, FALSE);
		    /* adjust ${"::_<newfilename"} to store the new file name */
		    GvSV(gv2) = newSVpvn(tmpbuf2 + 2, tmplen2 - 2);
		    /* The line number may differ. If that is the case,
		       alias the saved lines that are in the array.
		       Otherwise alias the whole array. */
		    if (CopLINE(PL_curcop) == line_num) {
			GvHV(gv2) = MUTABLE_HV(SvREFCNT_inc(GvHV(cfgv)));
			GvAV(gv2) = MUTABLE_AV(SvREFCNT_inc(GvAV(cfgv)));
		    }
		    else if (GvAV(cfgv)) {
			AV * const av = GvAV(cfgv);
			const line_t start = CopLINE(PL_curcop)+1;
			SSize_t items = AvFILLp(av) - start;
			if (items > 0) {
			    AV * const av2 = GvAVn(gv2);
			    SV **svp = AvARRAY(av) + start;
			    Size_t l = line_num+1;
			    while (items-- && l < SSize_t_MAX && l == (line_t)l)
				av_store(av2, (SSize_t)l++, SvREFCNT_inc(*svp++));
			}
		    }
		}

		if (tmpbuf2 != smallbuf) Safefree(tmpbuf2);
	    }
	}
	CopFILE_free(PL_curcop);
	CopFILE_setn(PL_curcop, s, len);
    }
    CopLINE_set(PL_curcop, line_num);
}

STATIC void
S_update_debugger_info(pTHX_ SV *orig_sv, const char *const buf, STRLEN len)
{
    AV *av = CopFILEAVx(PL_curcop);
    if (av) {
	SV * sv;
	if (PL_parser->preambling == NOLINE) sv = newSV_type(SVt_PVMG);
	else {
	    sv = *av_fetch(av, 0, 1);
	    SvUPGRADE(sv, SVt_PVMG);
	}
        if (!SvPOK(sv)) SvPVCLEAR(sv);
	if (orig_sv)
	    sv_catsv(sv, orig_sv);
	else
	    sv_catpvn(sv, buf, len);
	if (!SvIOK(sv)) {
	    (void)SvIOK_on(sv);
	    SvIV_set(sv, 0);
	}
	if (PL_parser->preambling == NOLINE)
	    av_store(av, CopLINE(PL_curcop), sv);
    }
}

/*
 * skipspace
 * Called to gobble the appropriate amount and type of whitespace.
 * Skips comments as well.
 * Returns the next character after the whitespace that is skipped.
 *
 * peekspace
 * Same thing, but look ahead without incrementing line numbers or
 * adjusting PL_linestart.
 */

#define skipspace(s) skipspace_flags(s, 0)
#define peekspace(s) skipspace_flags(s, LEX_NO_INCLINE)

STATIC char *
S_skipspace_flags(pTHX_ char *s, U32 flags)
{
    PERL_ARGS_ASSERT_SKIPSPACE_FLAGS;
    if (PL_lex_formbrack && PL_lex_brackets <= PL_lex_formbrack) {
	while (s < PL_bufend && (SPACE_OR_TAB(*s) || !*s))
	    s++;
    } else {
	STRLEN bufptr_pos = PL_bufptr - SvPVX(PL_linestr);
	PL_bufptr = s;
	lex_read_space(flags | LEX_KEEP_PREVIOUS |
		(PL_lex_inwhat || PL_lex_state == LEX_FORMLINE ?
		    LEX_NO_NEXT_CHUNK : 0));
	s = PL_bufptr;
	PL_bufptr = SvPVX(PL_linestr) + bufptr_pos;
	if (PL_linestart > PL_bufptr)
	    PL_bufptr = PL_linestart;
	return s;
    }
    return s;
}

/*
 * S_check_uni
 * Check the unary operators to ensure there's no ambiguity in how they're
 * used.  An ambiguous piece of code would be:
 *     rand + 5
 * This doesn't mean rand() + 5.  Because rand() is a unary operator,
 * the +5 is its argument.
 */

STATIC void
S_check_uni(pTHX)
{
    const char *s;

    if (PL_oldoldbufptr != PL_last_uni)
	return;
    while (isSPACE(*PL_last_uni))
	PL_last_uni++;
    s = PL_last_uni;
    while (isWORDCHAR_lazy_if_safe(s, PL_bufend, UTF) || *s == '-')
	s += UTF ? UTF8SKIP(s) : 1;
    if (s < PL_bufptr && memchr(s, '(', PL_bufptr - s))
	return;

    Perl_ck_warner_d(aTHX_ packWARN(WARN_AMBIGUOUS),
		     "Warning: Use of \"%" UTF8f "\" without parentheses is ambiguous",
		     UTF8fARG(UTF, (int)(s - PL_last_uni), PL_last_uni));
}

/*
 * LOP : macro to build a list operator.  Its behaviour has been replaced
 * with a subroutine, S_lop() for which LOP is just another name.
 */

#define LOP(f,x) return lop(f,x,s)

/*
 * S_lop
 * Build a list operator (or something that might be one).  The rules:
 *  - if we have a next token, then it's a list operator (no parens) for
 *    which the next token has already been parsed; e.g.,
 *       sort foo @args
 *       sort foo (@args)
 *  - if the next thing is an opening paren, then it's a function
 *  - else it's a list operator
 */

STATIC I32
S_lop(pTHX_ I32 f, U8 x, char *s)
{
    PERL_ARGS_ASSERT_LOP;

    pl_yylval.ival = f;
    CLINE;
    PL_bufptr = s;
    PL_last_lop = PL_oldbufptr;
    PL_last_lop_op = (OPCODE)f;
    if (PL_nexttoke)
	goto lstop;
    PL_expect = x;
    if (*s == '(')
	return REPORT(FUNC);
    s = skipspace(s);
    if (*s == '(')
	return REPORT(FUNC);
    else {
	lstop:
	if (!PL_lex_allbrackets && PL_lex_fakeeof > LEX_FAKEEOF_LOWLOGIC)
	    PL_lex_fakeeof = LEX_FAKEEOF_LOWLOGIC;
	return REPORT(LSTOP);
    }
}

/*
 * S_force_next
 * When the lexer realizes it knows the next token (for instance,
 * it is reordering tokens for the parser) then it can call S_force_next
 * to know what token to return the next time the lexer is called.  Caller
 * will need to set PL_nextval[] and possibly PL_expect to ensure
 * the lexer handles the token correctly.
 */

STATIC void
S_force_next(pTHX_ I32 type)
{
#ifdef DEBUGGING
    if (DEBUG_T_TEST) {
        PerlIO_printf(Perl_debug_log, "### forced token:\n");
	tokereport(type, &NEXTVAL_NEXTTOKE);
    }
#endif
    assert(PL_nexttoke < C_ARRAY_LENGTH(PL_nexttype));
    PL_nexttype[PL_nexttoke] = type;
    PL_nexttoke++;
}

/*
 * S_postderef
 *
 * This subroutine handles postfix deref syntax after the arrow has already
 * been emitted.  @* $* etc. are emitted as two separate tokens right here.
 * @[ @{ %[ %{ *{ are emitted also as two tokens, but this function emits
 * only the first, leaving yylex to find the next.
 */

static int
S_postderef(pTHX_ int const funny, char const next)
{
    assert(funny == DOLSHARP || strchr("$@%&*", funny));
    if (next == '*') {
	PL_expect = XOPERATOR;
	if (PL_lex_state == LEX_INTERPNORMAL && !PL_lex_brackets) {
	    assert('@' == funny || '$' == funny || DOLSHARP == funny);
	    PL_lex_state = LEX_INTERPEND;
	    if ('@' == funny)
		force_next(POSTJOIN);
	}
	force_next(next);
	PL_bufptr+=2;
    }
    else {
	if ('@' == funny && PL_lex_state == LEX_INTERPNORMAL
	 && !PL_lex_brackets)
	    PL_lex_dojoin = 2;
	PL_expect = XOPERATOR;
	PL_bufptr++;
    }
    return funny;
}

void
Perl_yyunlex(pTHX)
{
    int yyc = PL_parser->yychar;
    if (yyc != YYEMPTY) {
	if (yyc) {
	    NEXTVAL_NEXTTOKE = PL_parser->yylval;
	    if (yyc == '{'/*}*/ || yyc == HASHBRACK || yyc == '['/*]*/) {
		PL_lex_allbrackets--;
		PL_lex_brackets--;
		yyc |= (3<<24) | (PL_lex_brackstack[PL_lex_brackets] << 16);
	    } else if (yyc == '('/*)*/) {
		PL_lex_allbrackets--;
		yyc |= (2<<24);
	    }
	    force_next(yyc);
	}
	PL_parser->yychar = YYEMPTY;
    }
}

STATIC SV *
S_newSV_maybe_utf8(pTHX_ const char *const start, STRLEN len)
{
    SV * const sv = newSVpvn_utf8(start, len,
                    ! IN_BYTES
                  &&  UTF
                  &&  len != 0
                  &&  is_utf8_non_invariant_string((const U8*)start, len));
    return sv;
}

/*
 * S_force_word
 * When the lexer knows the next thing is a word (for instance, it has
 * just seen -> and it knows that the next char is a word char, then
 * it calls S_force_word to stick the next word into the PL_nexttoke/val
 * lookahead.
 *
 * Arguments:
 *   char *start : buffer position (must be within PL_linestr)
 *   int token   : PL_next* will be this type of bare word
 *                 (e.g., METHOD,BAREWORD)
 *   int check_keyword : if true, Perl checks to make sure the word isn't
 *       a keyword (do this if the word is a label, e.g. goto FOO)
 *   int allow_pack : if true, : characters will also be allowed (require,
 *       use, etc. do this)
 */

STATIC char *
S_force_word(pTHX_ char *start, int token, int check_keyword, int allow_pack)
{
    char *s;
    STRLEN len;

    PERL_ARGS_ASSERT_FORCE_WORD;

    start = skipspace(start);
    s = start;
    if (   isIDFIRST_lazy_if_safe(s, PL_bufend, UTF)
        || (allow_pack && *s == ':' && s[1] == ':') )
    {
	s = scan_word(s, PL_tokenbuf, sizeof PL_tokenbuf, allow_pack, &len);
	if (check_keyword) {
	  char *s2 = PL_tokenbuf;
	  STRLEN len2 = len;
	  if (allow_pack && memBEGINPs(s2, len, "CORE::")) {
	    s2 += sizeof("CORE::") - 1;
            len2 -= sizeof("CORE::") - 1;
          }
	  if (keyword(s2, len2, 0))
	    return start;
	}
	if (token == METHOD) {
	    s = skipspace(s);
	    if (*s == '(')
		PL_expect = XTERM;
	    else {
		PL_expect = XOPERATOR;
	    }
	}
	NEXTVAL_NEXTTOKE.opval
            = newSVOP(OP_CONST,0,
			   S_newSV_maybe_utf8(aTHX_ PL_tokenbuf, len));
	NEXTVAL_NEXTTOKE.opval->op_private |= OPpCONST_BARE;
	force_next(token);
    }
    return s;
}

/*
 * S_force_ident
 * Called when the lexer wants $foo *foo &foo etc, but the program
 * text only contains the "foo" portion.  The first argument is a pointer
 * to the "foo", and the second argument is the type symbol to prefix.
 * Forces the next token to be a "BAREWORD".
 * Creates the symbol if it didn't already exist (via gv_fetchpv()).
 */

STATIC void
S_force_ident(pTHX_ const char *s, int kind)
{
    PERL_ARGS_ASSERT_FORCE_IDENT;

    if (s[0]) {
	const STRLEN len = s[1] ? strlen(s) : 1; /* s = "\"" see yylex */
        OP* const o = newSVOP(OP_CONST, 0, newSVpvn_flags(s, len,
                                                                UTF ? SVf_UTF8 : 0));
	NEXTVAL_NEXTTOKE.opval = o;
	force_next(BAREWORD);
	if (kind) {
	    o->op_private = OPpCONST_ENTERED;
	    /* XXX see note in pp_entereval() for why we forgo typo
	       warnings if the symbol must be introduced in an eval.
	       GSAR 96-10-12 */
	    gv_fetchpvn_flags(s, len,
			      (PL_in_eval ? GV_ADDMULTI
			      : GV_ADD) | ( UTF ? SVf_UTF8 : 0 ),
			      kind == '$' ? SVt_PV :
			      kind == '@' ? SVt_PVAV :
			      kind == '%' ? SVt_PVHV :
			      SVt_PVGV
			      );
	}
    }
}

static void
S_force_ident_maybe_lex(pTHX_ char pit)
{
    NEXTVAL_NEXTTOKE.ival = pit;
    force_next('p');
}

NV
Perl_str_to_version(pTHX_ SV *sv)
{
    NV retval = 0.0;
    NV nshift = 1.0;
    STRLEN len;
    const char *start = SvPV_const(sv,len);
    const char * const end = start + len;
    const bool utf = cBOOL(SvUTF8(sv));

    PERL_ARGS_ASSERT_STR_TO_VERSION;

    while (start < end) {
	STRLEN skip;
	UV n;
	if (utf)
	    n = utf8n_to_uvchr((U8*)start, len, &skip, 0);
	else {
	    n = *(U8*)start;
	    skip = 1;
	}
	retval += ((NV)n)/nshift;
	start += skip;
	nshift *= 1000;
    }
    return retval;
}

/*
 * S_force_version
 * Forces the next token to be a version number.
 * If the next token appears to be an invalid version number, (e.g. "v2b"),
 * and if "guessing" is TRUE, then no new token is created (and the caller
 * must use an alternative parsing method).
 */

STATIC char *
S_force_version(pTHX_ char *s, int guessing)
{
    OP *version = NULL;
    char *d;

    PERL_ARGS_ASSERT_FORCE_VERSION;

    s = skipspace(s);

    d = s;
    if (*d == 'v')
	d++;
    if (isDIGIT(*d)) {
	while (isDIGIT(*d) || *d == '_' || *d == '.')
	    d++;
        if (*d == ';' || isSPACE(*d) || *d == '{' || *d == '}' || !*d) {
	    SV *ver;
            s = scan_num(s, &pl_yylval);
            version = pl_yylval.opval;
	    ver = cSVOPx(version)->op_sv;
	    if (SvPOK(ver) && !SvNIOK(ver)) {
		SvUPGRADE(ver, SVt_PVNV);
		SvNV_set(ver, str_to_version(ver));
		SvNOK_on(ver);		/* hint that it is a version */
	    }
        }
	else if (guessing) {
	    return s;
	}
    }

    /* NOTE: The parser sees the package name and the VERSION swapped */
    NEXTVAL_NEXTTOKE.opval = version;
    force_next(BAREWORD);

    return s;
}

/*
 * S_force_strict_version
 * Forces the next token to be a version number using strict syntax rules.
 */

STATIC char *
S_force_strict_version(pTHX_ char *s)
{
    OP *version = NULL;
    const char *errstr = NULL;

    PERL_ARGS_ASSERT_FORCE_STRICT_VERSION;

    while (isSPACE(*s)) /* leading whitespace */
	s++;

    if (is_STRICT_VERSION(s,&errstr)) {
	SV *ver = newSV(0);
	s = (char *)scan_version(s, ver, 0);
	version = newSVOP(OP_CONST, 0, ver);
    }
    else if ((*s != ';' && *s != '{' && *s != '}' )
             && (s = skipspace(s), (*s != ';' && *s != '{' && *s != '}' )))
    {
	PL_bufptr = s;
	if (errstr)
	    yyerror(errstr); /* version required */
	return s;
    }

    /* NOTE: The parser sees the package name and the VERSION swapped */
    NEXTVAL_NEXTTOKE.opval = version;
    force_next(BAREWORD);

    return s;
}

/*
 * S_tokeq
 * Turns any \\ into \ in a quoted string passed in in 'sv', returning 'sv',
 * modified as necessary.  However, if HINT_NEW_STRING is on, 'sv' is
 * unchanged, and a new SV containing the modified input is returned.
 */

STATIC SV *
S_tokeq(pTHX_ SV *sv)
{
    char *s;
    char *send;
    char *d;
    SV *pv = sv;

    PERL_ARGS_ASSERT_TOKEQ;

    assert (SvPOK(sv));
    assert (SvLEN(sv));
    assert (!SvIsCOW(sv));
    if (SvTYPE(sv) >= SVt_PVIV && SvIVX(sv) == -1) /* <<'heredoc' */
	goto finish;
    s = SvPVX(sv);
    send = SvEND(sv);
    /* This is relying on the SV being "well formed" with a trailing '\0'  */
    while (s < send && !(*s == '\\' && s[1] == '\\'))
	s++;
    if (s == send)
	goto finish;
    d = s;
    if ( PL_hints & HINT_NEW_STRING ) {
	pv = newSVpvn_flags(SvPVX_const(pv), SvCUR(sv),
			    SVs_TEMP | SvUTF8(sv));
    }
    while (s < send) {
	if (*s == '\\') {
	    if (s + 1 < send && (s[1] == '\\'))
		s++;		/* all that, just for this */
	}
	*d++ = *s++;
    }
    *d = '\0';
    SvCUR_set(sv, d - SvPVX_const(sv));
  finish:
    if ( PL_hints & HINT_NEW_STRING )
       return new_constant(NULL, 0, "q", sv, pv, "q", 1);
    return sv;
}

/*
 * Now come three functions related to double-quote context,
 * S_sublex_start, S_sublex_push, and S_sublex_done.  They're used when
 * converting things like "\u\Lgnat" into ucfirst(lc("gnat")).  They
 * interact with PL_lex_state, and create fake ( ... ) argument lists
 * to handle functions and concatenation.
 * For example,
 *   "foo\lbar"
 * is tokenised as
 *    stringify ( const[foo] concat lcfirst ( const[bar] ) )
 */

/*
 * S_sublex_start
 * Assumes that pl_yylval.ival is the op we're creating (e.g. OP_LCFIRST).
 *
 * Pattern matching will set PL_lex_op to the pattern-matching op to
 * make (we return THING if pl_yylval.ival is OP_NULL, PMFUNC otherwise).
 *
 * OP_CONST is easy--just make the new op and return.
 *
 * Everything else becomes a FUNC.
 *
 * Sets PL_lex_state to LEX_INTERPPUSH unless ival was OP_NULL or we
 * had an OP_CONST.  This just sets us up for a
 * call to S_sublex_push().
 */

STATIC I32
S_sublex_start(pTHX)
{
    const I32 op_type = pl_yylval.ival;

    if (op_type == OP_NULL) {
	pl_yylval.opval = PL_lex_op;
	PL_lex_op = NULL;
	return THING;
    }
    if (op_type == OP_CONST) {
	SV *sv = PL_lex_stuff;
	PL_lex_stuff = NULL;
	sv = tokeq(sv);

	if (SvTYPE(sv) == SVt_PVIV) {
	    /* Overloaded constants, nothing fancy: Convert to SVt_PV: */
	    STRLEN len;
	    const char * const p = SvPV_const(sv, len);
	    SV * const nsv = newSVpvn_flags(p, len, SvUTF8(sv));
	    SvREFCNT_dec(sv);
	    sv = nsv;
	}
        pl_yylval.opval = newSVOP(op_type, 0, sv);
	return THING;
    }

    PL_parser->lex_super_state = PL_lex_state;
    PL_parser->lex_sub_inwhat = (U16)op_type;
    PL_parser->lex_sub_op = PL_lex_op;
    PL_parser->sub_no_recover = FALSE;
    PL_parser->sub_error_count = PL_error_count;
    PL_lex_state = LEX_INTERPPUSH;

    PL_expect = XTERM;
    if (PL_lex_op) {
	pl_yylval.opval = PL_lex_op;
	PL_lex_op = NULL;
	return PMFUNC;
    }
    else
	return FUNC;
}

/*
 * S_sublex_push
 * Create a new scope to save the lexing state.  The scope will be
 * ended in S_sublex_done.  Returns a '(', starting the function arguments
 * to the uc, lc, etc. found before.
 * Sets PL_lex_state to LEX_INTERPCONCAT.
 */

STATIC I32
S_sublex_push(pTHX)
{
    LEXSHARED *shared;
    const bool is_heredoc = PL_multi_close == '<';
    ENTER;

    PL_lex_state = PL_parser->lex_super_state;
    SAVEI8(PL_lex_dojoin);
    SAVEI32(PL_lex_brackets);
    SAVEI32(PL_lex_allbrackets);
    SAVEI32(PL_lex_formbrack);
    SAVEI8(PL_lex_fakeeof);
    SAVEI32(PL_lex_casemods);
    SAVEI32(PL_lex_starts);
    SAVEI8(PL_lex_state);
    SAVESPTR(PL_lex_repl);
    SAVEVPTR(PL_lex_inpat);
    SAVEI16(PL_lex_inwhat);
    if (is_heredoc)
    {
	SAVECOPLINE(PL_curcop);
	SAVEI32(PL_multi_end);
	SAVEI32(PL_parser->herelines);
	PL_parser->herelines = 0;
    }
    SAVEIV(PL_multi_close);
    SAVEPPTR(PL_bufptr);
    SAVEPPTR(PL_bufend);
    SAVEPPTR(PL_oldbufptr);
    SAVEPPTR(PL_oldoldbufptr);
    SAVEPPTR(PL_last_lop);
    SAVEPPTR(PL_last_uni);
    SAVEPPTR(PL_linestart);
    SAVESPTR(PL_linestr);
    SAVEGENERICPV(PL_lex_brackstack);
    SAVEGENERICPV(PL_lex_casestack);
    SAVEGENERICPV(PL_parser->lex_shared);
    SAVEBOOL(PL_parser->lex_re_reparsing);
    SAVEI32(PL_copline);

    /* The here-doc parser needs to be able to peek into outer lexing
       scopes to find the body of the here-doc.  So we put PL_linestr and
       PL_bufptr into lex_shared, to ‘share’ those values.
     */
    PL_parser->lex_shared->ls_linestr = PL_linestr;
    PL_parser->lex_shared->ls_bufptr  = PL_bufptr;

    PL_linestr = PL_lex_stuff;
    PL_lex_repl = PL_parser->lex_sub_repl;
    PL_lex_stuff = NULL;
    PL_parser->lex_sub_repl = NULL;

    /* Arrange for PL_lex_stuff to be freed on scope exit, in case it gets
       set for an inner quote-like operator and then an error causes scope-
       popping.  We must not have a PL_lex_stuff value left dangling, as
       that breaks assumptions elsewhere.  See bug #123617.  */
    SAVEGENERICSV(PL_lex_stuff);
    SAVEGENERICSV(PL_parser->lex_sub_repl);

    PL_bufend = PL_bufptr = PL_oldbufptr = PL_oldoldbufptr = PL_linestart
	= SvPVX(PL_linestr);
    PL_bufend += SvCUR(PL_linestr);
    PL_last_lop = PL_last_uni = NULL;
    SAVEFREESV(PL_linestr);
    if (PL_lex_repl) SAVEFREESV(PL_lex_repl);

    PL_lex_dojoin = FALSE;
    PL_lex_brackets = PL_lex_formbrack = 0;
    PL_lex_allbrackets = 0;
    PL_lex_fakeeof = LEX_FAKEEOF_NEVER;
    Newx(PL_lex_brackstack, 120, char);
    Newx(PL_lex_casestack, 12, char);
    PL_lex_casemods = 0;
    *PL_lex_casestack = '\0';
    PL_lex_starts = 0;
    PL_lex_state = LEX_INTERPCONCAT;
    if (is_heredoc)
	CopLINE_set(PL_curcop, (line_t)PL_multi_start);
    PL_copline = NOLINE;

    Newxz(shared, 1, LEXSHARED);
    shared->ls_prev = PL_parser->lex_shared;
    PL_parser->lex_shared = shared;

    PL_lex_inwhat = PL_parser->lex_sub_inwhat;
    if (PL_lex_inwhat == OP_TRANSR) PL_lex_inwhat = OP_TRANS;
    if (PL_lex_inwhat == OP_MATCH || PL_lex_inwhat == OP_QR || PL_lex_inwhat == OP_SUBST)
	PL_lex_inpat = PL_parser->lex_sub_op;
    else
	PL_lex_inpat = NULL;

    PL_parser->lex_re_reparsing = cBOOL(PL_in_eval & EVAL_RE_REPARSING);
    PL_in_eval &= ~EVAL_RE_REPARSING;

    return '(';
}

/*
 * S_sublex_done
 * Restores lexer state after a S_sublex_push.
 */

STATIC I32
S_sublex_done(pTHX)
{
    if (!PL_lex_starts++) {
	SV * const sv = newSVpvs("");
	if (SvUTF8(PL_linestr))
	    SvUTF8_on(sv);
	PL_expect = XOPERATOR;
        pl_yylval.opval = newSVOP(OP_CONST, 0, sv);
	return THING;
    }

    if (PL_lex_casemods) {		/* oops, we've got some unbalanced parens */
	PL_lex_state = LEX_INTERPCASEMOD;
	return yylex();
    }

    /* Is there a right-hand side to take care of? (s//RHS/ or tr//RHS/) */
    assert(PL_lex_inwhat != OP_TRANSR);
    if (PL_lex_repl) {
	assert (PL_lex_inwhat == OP_SUBST || PL_lex_inwhat == OP_TRANS);
	PL_linestr = PL_lex_repl;
	PL_lex_inpat = 0;
	PL_bufend = PL_bufptr = PL_oldbufptr = PL_oldoldbufptr = PL_linestart = SvPVX(PL_linestr);
	PL_bufend += SvCUR(PL_linestr);
	PL_last_lop = PL_last_uni = NULL;
	PL_lex_dojoin = FALSE;
	PL_lex_brackets = 0;
	PL_lex_allbrackets = 0;
	PL_lex_fakeeof = LEX_FAKEEOF_NEVER;
	PL_lex_casemods = 0;
	*PL_lex_casestack = '\0';
	PL_lex_starts = 0;
	if (SvEVALED(PL_lex_repl)) {
	    PL_lex_state = LEX_INTERPNORMAL;
	    PL_lex_starts++;
	    /*	we don't clear PL_lex_repl here, so that we can check later
		whether this is an evalled subst; that means we rely on the
		logic to ensure sublex_done() is called again only via the
		branch (in yylex()) that clears PL_lex_repl, else we'll loop */
	}
	else {
	    PL_lex_state = LEX_INTERPCONCAT;
	    PL_lex_repl = NULL;
	}
	if (SvTYPE(PL_linestr) >= SVt_PVNV) {
	    CopLINE(PL_curcop) +=
		((XPVNV*)SvANY(PL_linestr))->xnv_u.xnv_lines
		 + PL_parser->herelines;
	    PL_parser->herelines = 0;
	}
	return '/';
    }
    else {
	const line_t l = CopLINE(PL_curcop);
	LEAVE;
        if (PL_parser->sub_error_count != PL_error_count) {
            const char * const name = OutCopFILE(PL_curcop);
            if (PL_parser->sub_no_recover) {
                const char * msg = "";
                if (PL_in_eval) {
                    SV *errsv = ERRSV;
                    if (SvCUR(ERRSV)) {
                        msg = Perl_form(aTHX_ "%" SVf, SVfARG(errsv));
                    }
                }
                abort_execution(msg, name);
                NOT_REACHED;
            }
        }
	if (PL_multi_close == '<')
	    PL_parser->herelines += l - PL_multi_end;
	PL_bufend = SvPVX(PL_linestr);
	PL_bufend += SvCUR(PL_linestr);
	PL_expect = XOPERATOR;
	return ')';
    }
}

STATIC SV*
S_get_and_check_backslash_N_name(pTHX_ const char* s, const char* const e)
{
    /* <s> points to first character of interior of \N{}, <e> to one beyond the
     * interior, hence to the "}".  Finds what the name resolves to, returning
     * an SV* containing it; NULL if no valid one found */

    SV* res = newSVpvn_flags(s, e - s, UTF ? SVf_UTF8 : 0);

    HV * table;
    SV **cvp;
    SV *cv;
    SV *rv;
    HV *stash;
    const char* backslash_ptr = s - 3; /* Points to the <\> of \N{... */

    PERL_ARGS_ASSERT_GET_AND_CHECK_BACKSLASH_N_NAME;

    if (!SvCUR(res)) {
        SvREFCNT_dec_NN(res);
        /* diag_listed_as: Unknown charname '%s' */
        yyerror("Unknown charname ''");
        return NULL;
    }

    res = new_constant( NULL, 0, "charnames", res, NULL, backslash_ptr,
                        /* include the <}> */
                        e - backslash_ptr + 1);
    if (! SvPOK(res)) {
        SvREFCNT_dec_NN(res);
        return NULL;
    }

    /* See if the charnames handler is the Perl core's, and if so, we can skip
     * the validation needed for a user-supplied one, as Perl's does its own
     * validation. */
    table = GvHV(PL_hintgv);		 /* ^H */
    cvp = hv_fetchs(table, "charnames", FALSE);
    if (cvp && (cv = *cvp) && SvROK(cv) && (rv = SvRV(cv),
        SvTYPE(rv) == SVt_PVCV) && ((stash = CvSTASH(rv)) != NULL))
    {
        const char * const name = HvNAME(stash);
         if (memEQs(name, HvNAMELEN(stash), "_charnames")) {
           return res;
       }
    }

    /* Here, it isn't Perl's charname handler.  We can't rely on a
     * user-supplied handler to validate the input name.  For non-ut8 input,
     * look to see that the first character is legal.  Then loop through the
     * rest checking that each is a continuation */

    /* This code makes the reasonable assumption that the only Latin1-range
     * characters that begin a character name alias are alphabetic, otherwise
     * would have to create a isCHARNAME_BEGIN macro */

    if (! UTF) {
        if (! isALPHAU(*s)) {
            goto bad_charname;
        }
        s++;
        while (s < e) {
            if (! isCHARNAME_CONT(*s)) {
                goto bad_charname;
            }
	    if (*s == ' ' && *(s-1) == ' ') {
                goto multi_spaces;
            }
            s++;
        }
    }
    else {
        /* Similarly for utf8.  For invariants can check directly; for other
         * Latin1, can calculate their code point and check; otherwise  use a
         * swash */
        if (UTF8_IS_INVARIANT(*s)) {
            if (! isALPHAU(*s)) {
                goto bad_charname;
            }
            s++;
        } else if (UTF8_IS_DOWNGRADEABLE_START(*s)) {
            if (! isALPHAU(EIGHT_BIT_UTF8_TO_NATIVE(*s, *(s+1)))) {
                goto bad_charname;
            }
            s += 2;
        }
        else {
            if (! _invlist_contains_cp(PL_utf8_charname_begin,
                                       utf8_to_uvchr_buf((U8 *) s,
                                                         (U8 *) e,
                                                         NULL)))
            {
                goto bad_charname;
            }
            s += UTF8SKIP(s);
        }

        while (s < e) {
            if (UTF8_IS_INVARIANT(*s)) {
                if (! isCHARNAME_CONT(*s)) {
                    goto bad_charname;
                }
                if (*s == ' ' && *(s-1) == ' ') {
                    goto multi_spaces;
                }
                s++;
            }
            else if (UTF8_IS_DOWNGRADEABLE_START(*s)) {
                if (! isCHARNAME_CONT(EIGHT_BIT_UTF8_TO_NATIVE(*s, *(s+1))))
                {
                    goto bad_charname;
                }
                s += 2;
            }
            else {
                if (! _invlist_contains_cp(PL_utf8_charname_continue,
                                           utf8_to_uvchr_buf((U8 *) s,
                                                             (U8 *) e,
                                                             NULL)))
                {
                    goto bad_charname;
                }
                s += UTF8SKIP(s);
            }
        }
    }
    if (*(s-1) == ' ') {
        /* diag_listed_as: charnames alias definitions may not contain
                           trailing white-space; marked by <-- HERE in %s
         */
        yyerror_pv(
            Perl_form(aTHX_
            "charnames alias definitions may not contain trailing "
            "white-space; marked by <-- HERE in %.*s<-- HERE %.*s",
            (int)(s - backslash_ptr + 1), backslash_ptr,
            (int)(e - s + 1), s + 1
            ),
        UTF ? SVf_UTF8 : 0);
        return NULL;
    }

    if (SvUTF8(res)) { /* Don't accept malformed input */
        const U8* first_bad_char_loc;
        STRLEN len;
        const char* const str = SvPV_const(res, len);
        if (UNLIKELY(! is_utf8_string_loc((U8 *) str, len,
                                          &first_bad_char_loc)))
        {
            _force_out_malformed_utf8_message(first_bad_char_loc,
                                              (U8 *) PL_parser->bufend,
                                              0,
                                              0 /* 0 means don't die */ );
            /* diag_listed_as: Malformed UTF-8 returned by \N{%s}
                               immediately after '%s' */
            yyerror_pv(
              Perl_form(aTHX_
                "Malformed UTF-8 returned by %.*s immediately after '%.*s'",
                 (int) (e - backslash_ptr + 1), backslash_ptr,
                 (int) ((char *) first_bad_char_loc - str), str
              ),
              SVf_UTF8);
            return NULL;
        }
    }

    return res;

  bad_charname: {

        /* The final %.*s makes sure that should the trailing NUL be missing
         * that this print won't run off the end of the string */
        /* diag_listed_as: Invalid character in \N{...}; marked by <-- HERE
                           in \N{%s} */
        yyerror_pv(
          Perl_form(aTHX_
            "Invalid character in \\N{...}; marked by <-- HERE in %.*s<-- HERE %.*s",
            (int)(s - backslash_ptr + 1), backslash_ptr,
            (int)(e - s + 1), s + 1
          ),
          UTF ? SVf_UTF8 : 0);
        return NULL;
    }

  multi_spaces:
        /* diag_listed_as: charnames alias definitions may not contain a
                           sequence of multiple spaces; marked by <-- HERE
                           in %s */
        yyerror_pv(
          Perl_form(aTHX_
            "charnames alias definitions may not contain a sequence of "
            "multiple spaces; marked by <-- HERE in %.*s<-- HERE %.*s",
            (int)(s - backslash_ptr + 1), backslash_ptr,
            (int)(e - s + 1), s + 1
          ),
          UTF ? SVf_UTF8 : 0);
        return NULL;
}

/*
  scan_const

  Extracts the next constant part of a pattern, double-quoted string,
  or transliteration.  This is terrifying code.

  For example, in parsing the double-quoted string "ab\x63$d", it would
  stop at the '$' and return an OP_CONST containing 'abc'.

  It looks at PL_lex_inwhat and PL_lex_inpat to find out whether it's
  processing a pattern (PL_lex_inpat is true), a transliteration
  (PL_lex_inwhat == OP_TRANS is true), or a double-quoted string.

  Returns a pointer to the character scanned up to. If this is
  advanced from the start pointer supplied (i.e. if anything was
  successfully parsed), will leave an OP_CONST for the substring scanned
  in pl_yylval. Caller must intuit reason for not parsing further
  by looking at the next characters herself.

  In patterns:
    expand:
      \N{FOO}  => \N{U+hex_for_character_FOO}
      (if FOO expands to multiple characters, expands to \N{U+xx.XX.yy ...})

    pass through:
	all other \-char, including \N and \N{ apart from \N{ABC}

    stops on:
	@ and $ where it appears to be a var, but not for $ as tail anchor
        \l \L \u \U \Q \E
	(?{  or  (??{

  In transliterations:
    characters are VERY literal, except for - not at the start or end
    of the string, which indicates a range.  However some backslash sequences
    are recognized: \r, \n, and the like
                    \007 \o{}, \x{}, \N{}
    If all elements in the transliteration are below 256,
    scan_const expands the range to the full set of intermediate
    characters. If the range is in utf8, the hyphen is replaced with
    a certain range mark which will be handled by pmtrans() in op.c.

  In double-quoted strings:
    backslashes:
      all those recognized in transliterations
      deprecated backrefs: \1 (in substitution replacements)
      case and quoting: \U \Q \E
    stops on @ and $

  scan_const does *not* construct ops to handle interpolated strings.
  It stops processing as soon as it finds an embedded $ or @ variable
  and leaves it to the caller to work out what's going on.

  embedded arrays (whether in pattern or not) could be:
      @foo, @::foo, @'foo, @{foo}, @$foo, @+, @-.

  $ in double-quoted strings must be the symbol of an embedded scalar.

  $ in pattern could be $foo or could be tail anchor.  Assumption:
  it's a tail anchor if $ is the last thing in the string, or if it's
  followed by one of "()| \r\n\t"

  \1 (backreferences) are turned into $1 in substitutions

  The structure of the code is
      while (there's a character to process) {
	  handle transliteration ranges
	  skip regexp comments /(?#comment)/ and codes /(?{code})/
	  skip #-initiated comments in //x patterns
	  check for embedded arrays
	  check for embedded scalars
	  if (backslash) {
	      deprecate \1 in substitution replacements
	      handle string-changing backslashes \l \U \Q \E, etc.
	      switch (what was escaped) {
		  handle \- in a transliteration (becomes a literal -)
		  if a pattern and not \N{, go treat as regular character
		  handle \132 (octal characters)
		  handle \x15 and \x{1234} (hex characters)
		  handle \N{name} (named characters, also \N{3,5} in a pattern)
		  handle \cV (control characters)
		  handle printf-style backslashes (\f, \r, \n, etc)
	      } (end switch)
	      continue
	  } (end if backslash)
          handle regular character
    } (end while character to read)

*/

STATIC char *
S_scan_const(pTHX_ char *start)
{
    char *send = PL_bufend;		/* end of the constant */
    SV *sv = newSV(send - start);       /* sv for the constant.  See note below
                                           on sizing. */
    char *s = start;			/* start of the constant */
    char *d = SvPVX(sv);		/* destination for copies */
    bool dorange = FALSE;               /* are we in a translit range? */
    bool didrange = FALSE;              /* did we just finish a range? */
    bool in_charclass = FALSE;          /* within /[...]/ */
    bool has_utf8 = FALSE;              /* Output constant is UTF8 */
    bool  this_utf8 = cBOOL(UTF);       /* Is the source string assumed to be
                                           UTF8?  But, this can show as true
                                           when the source isn't utf8, as for
                                           example when it is entirely composed
                                           of hex constants */
    STRLEN utf8_variant_count = 0;      /* When not in UTF-8, this counts the
                                           number of characters found so far
                                           that will expand (into 2 bytes)
                                           should we have to convert to
                                           UTF-8) */
    SV *res;		                /* result from charnames */
    STRLEN offset_to_max = 0;   /* The offset in the output to where the range
                                   high-end character is temporarily placed */

    /* Does something require special handling in tr/// ?  This avoids extra
     * work in a less likely case.  As such, khw didn't feel it was worth
     * adding any branches to the more mainline code to handle this, which
     * means that this doesn't get set in some circumstances when things like
     * \x{100} get expanded out.  As a result there needs to be extra testing
     * done in the tr code */
    bool has_above_latin1 = FALSE;

    /* Note on sizing:  The scanned constant is placed into sv, which is
     * initialized by newSV() assuming one byte of output for every byte of
     * input.  This routine expects newSV() to allocate an extra byte for a
     * trailing NUL, which this routine will append if it gets to the end of
     * the input.  There may be more bytes of input than output (eg., \N{LATIN
     * CAPITAL LETTER A}), or more output than input if the constant ends up
     * recoded to utf8, but each time a construct is found that might increase
     * the needed size, SvGROW() is called.  Its size parameter each time is
     * based on the best guess estimate at the time, namely the length used so
     * far, plus the length the current construct will occupy, plus room for
     * the trailing NUL, plus one byte for every input byte still unscanned */

    UV uv = UV_MAX; /* Initialize to weird value to try to catch any uses
                       before set */
#ifdef EBCDIC
    int backslash_N = 0;            /* ? was the character from \N{} */
    int non_portable_endpoint = 0;  /* ? In a range is an endpoint
                                       platform-specific like \x65 */
#endif

    PERL_ARGS_ASSERT_SCAN_CONST;

    assert(PL_lex_inwhat != OP_TRANSR);
    if (PL_lex_inwhat == OP_TRANS && PL_parser->lex_sub_op) {
	/* If we are doing a trans and we know we want UTF8 set expectation */
	has_utf8   = PL_parser->lex_sub_op->op_private & (OPpTRANS_FROM_UTF|OPpTRANS_TO_UTF);
	this_utf8  = PL_parser->lex_sub_op->op_private & (PL_lex_repl ? OPpTRANS_FROM_UTF : OPpTRANS_TO_UTF);
    }

    /* Protect sv from errors and fatal warnings. */
    ENTER_with_name("scan_const");
    SAVEFREESV(sv);

    while (s < send
           || dorange   /* Handle tr/// range at right edge of input */
    ) {

        /* get transliterations out of the way (they're most literal) */
	if (PL_lex_inwhat == OP_TRANS) {

            /* But there isn't any special handling necessary unless there is a
             * range, so for most cases we just drop down and handle the value
             * as any other.  There are two exceptions.
             *
             * 1.  A hyphen indicates that we are actually going to have a
             *     range.  In this case, skip the '-', set a flag, then drop
             *     down to handle what should be the end range value.
             * 2.  After we've handled that value, the next time through, that
             *     flag is set and we fix up the range.
             *
             * Ranges entirely within Latin1 are expanded out entirely, in
             * order to make the transliteration a simple table look-up.
             * Ranges that extend above Latin1 have to be done differently, so
             * there is no advantage to expanding them here, so they are
             * stored here as Min, ILLEGAL_UTF8_BYTE, Max.  The illegal byte
             * signifies a hyphen without any possible ambiguity.  On EBCDIC
             * machines, if the range is expressed as Unicode, the Latin1
             * portion is expanded out even if the range extends above
             * Latin1.  This is because each code point in it has to be
             * processed here individually to get its native translation */

	    if (! dorange) {

                /* Here, we don't think we're in a range.  If the new character
                 * is not a hyphen; or if it is a hyphen, but it's too close to
                 * either edge to indicate a range, or if we haven't output any
                 * characters yet then it's a regular character. */
                if (*s != '-' || s >= send - 1 || s == start || d == SvPVX(sv)) {

                    /* A regular character.  Process like any other, but first
                     * clear any flags */
                    didrange = FALSE;
                    dorange = FALSE;
#ifdef EBCDIC
                    non_portable_endpoint = 0;
                    backslash_N = 0;
#endif
                    /* The tests here for being above Latin1 and similar ones
                     * in the following 'else' suffice to find all such
                     * occurences in the constant, except those added by a
                     * backslash escape sequence, like \x{100}.  Mostly, those
                     * set 'has_above_latin1' as appropriate */
                    if (this_utf8 && UTF8_IS_ABOVE_LATIN1(*s)) {
                        has_above_latin1 = TRUE;
                    }

                    /* Drops down to generic code to process current byte */
                }
                else {  /* Is a '-' in the context where it means a range */
                    if (didrange) { /* Something like y/A-C-Z// */
                        Perl_croak(aTHX_ "Ambiguous range in transliteration"
                                         " operator");
                    }

                    dorange = TRUE;

                    s++;    /* Skip past the hyphen */

                    /* d now points to where the end-range character will be
                     * placed.  Save it so won't have to go finding it later,
                     * and drop down to get that character.  (Actually we
                     * instead save the offset, to handle the case where a
                     * realloc in the meantime could change the actual
                     * pointer).  We'll finish processing the range the next
                     * time through the loop */
                    offset_to_max = d - SvPVX_const(sv);

                    if (this_utf8 && UTF8_IS_ABOVE_LATIN1(*s)) {
                        has_above_latin1 = TRUE;
                    }

                    /* Drops down to generic code to process current byte */
                }
            }  /* End of not a range */
            else {
                /* Here we have parsed a range.  Now must handle it.  At this
                 * point:
                 * 'sv' is a SV* that contains the output string we are
                 *      constructing.  The final two characters in that string
                 *      are the range start and range end, in order.
                 * 'd'  points to just beyond the range end in the 'sv' string,
                 *      where we would next place something
                 * 'offset_to_max' is the offset in 'sv' at which the character
                 *      (the range's maximum end point) before 'd'  begins.
                 */
                char * max_ptr = SvPVX(sv) + offset_to_max;
                char * min_ptr;
                IV range_min;
		IV range_max;	/* last character in range */
                STRLEN grow;
                Size_t offset_to_min = 0;
                Size_t extras = 0;
#ifdef EBCDIC
                bool convert_unicode;
                IV real_range_max = 0;
#endif
                /* Get the code point values of the range ends. */
                if (has_utf8) {
                    /* We know the utf8 is valid, because we just constructed
                     * it ourselves in previous loop iterations */
                    min_ptr = (char*) utf8_hop( (U8*) max_ptr, -1);
                    range_min = valid_utf8_to_uvchr( (U8*) min_ptr, NULL);
                    range_max = valid_utf8_to_uvchr( (U8*) max_ptr, NULL);

                    /* This compensates for not all code setting
                     * 'has_above_latin1', so that we don't skip stuff that
                     * should be executed */
                    if (range_max > 255) {
                        has_above_latin1 = TRUE;
                    }
                }
                else {
                    min_ptr = max_ptr - 1;
                    range_min = * (U8*) min_ptr;
                    range_max = * (U8*) max_ptr;
                }

                /* If the range is just a single code point, like tr/a-a/.../,
                 * that code point is already in the output, twice.  We can
                 * just back up over the second instance and avoid all the rest
                 * of the work.  But if it is a variant character, it's been
                 * counted twice, so decrement.  (This unlikely scenario is
                 * special cased, like the one for a range of 2 code points
                 * below, only because the main-line code below needs a range
                 * of 3 or more to work without special casing.  Might as well
                 * get it out of the way now.) */
                if (UNLIKELY(range_max == range_min)) {
                    d = max_ptr;
                    if (! has_utf8 && ! UVCHR_IS_INVARIANT(range_max)) {
                        utf8_variant_count--;
                    }
                    goto range_done;
                }

#ifdef EBCDIC
                /* On EBCDIC platforms, we may have to deal with portable
                 * ranges.  These happen if at least one range endpoint is a
                 * Unicode value (\N{...}), or if the range is a subset of
                 * [A-Z] or [a-z], and both ends are literal characters,
                 * like 'A', and not like \x{C1} */
                convert_unicode =
                               cBOOL(backslash_N)   /* \N{} forces Unicode,
                                                       hence portable range */
                    || (     ! non_portable_endpoint
                        && ((  isLOWER_A(range_min) && isLOWER_A(range_max))
                           || (isUPPER_A(range_min) && isUPPER_A(range_max))));
                if (convert_unicode) {

                    /* Special handling is needed for these portable ranges.
                     * They are defined to be in Unicode terms, which includes
                     * all the Unicode code points between the end points.
                     * Convert to Unicode to get the Unicode range.  Later we
                     * will convert each code point in the range back to
                     * native.  */
                    range_min = NATIVE_TO_UNI(range_min);
                    range_max = NATIVE_TO_UNI(range_max);
                }
#endif

                if (range_min > range_max) {
#ifdef EBCDIC
                    if (convert_unicode) {
                        /* Need to convert back to native for meaningful
                         * messages for this platform */
                        range_min = UNI_TO_NATIVE(range_min);
                        range_max = UNI_TO_NATIVE(range_max);
                    }
#endif
                    /* Use the characters themselves for the error message if
                     * ASCII printables; otherwise some visible representation
                     * of them */
                    if (isPRINT_A(range_min) && isPRINT_A(range_max)) {
                        Perl_croak(aTHX_
			 "Invalid range \"%c-%c\" in transliteration operator",
			 (char)range_min, (char)range_max);
                    }
#ifdef EBCDIC
                    else if (convert_unicode) {
        /* diag_listed_as: Invalid range "%s" in transliteration operator */
                        Perl_croak(aTHX_
                           "Invalid range \"\\N{U+%04" UVXf "}-\\N{U+%04"
                           UVXf "}\" in transliteration operator",
                           range_min, range_max);
                    }
#endif
                    else {
        /* diag_listed_as: Invalid range "%s" in transliteration operator */
                        Perl_croak(aTHX_
                           "Invalid range \"\\x{%04" UVXf "}-\\x{%04" UVXf "}\""
                           " in transliteration operator",
                           range_min, range_max);
                    }
                }

                /* If the range is exactly two code points long, they are
                 * already both in the output */
                if (UNLIKELY(range_min + 1 == range_max)) {
                    goto range_done;
                }

                /* Here the range contains at least 3 code points */

		if (has_utf8) {

                    /* If everything in the transliteration is below 256, we
                     * can avoid special handling later.  A translation table
                     * for each of those bytes is created by op.c.  So we
                     * expand out all ranges to their constituent code points.
                     * But if we've encountered something above 255, the
                     * expanding won't help, so skip doing that.  But if it's
                     * EBCDIC, we may have to look at each character below 256
                     * if we have to convert to/from Unicode values */
                    if (   has_above_latin1
#ifdef EBCDIC
		        && (range_min > 255 || ! convert_unicode)
#endif
                    ) {
                        /* Move the high character one byte to the right; then
                         * insert between it and the range begin, an illegal
                         * byte which serves to indicate this is a range (using
                         * a '-' would be ambiguous). */
                        char *e = d++;
                        while (e-- > max_ptr) {
                            *(e + 1) = *e;
                        }
                        *(e + 1) = (char) ILLEGAL_UTF8_BYTE;
                        goto range_done;
                    }

                    /* Here, we're going to expand out the range.  For EBCDIC
                     * the range can extend above 255 (not so in ASCII), so
                     * for EBCDIC, split it into the parts above and below
                     * 255/256 */
#ifdef EBCDIC
                    if (range_max > 255) {
                        real_range_max = range_max;
                        range_max = 255;
                    }
#endif
		}

                /* Here we need to expand out the string to contain each
                 * character in the range.  Grow the output to handle this.
                 * For non-UTF8, we need a byte for each code point in the
                 * range, minus the three that we've already allocated for: the
                 * hyphen, the min, and the max.  For UTF-8, we need this
                 * plus an extra byte for each code point that occupies two
                 * bytes (is variant) when in UTF-8 (except we've already
                 * allocated for the end points, including if they are
                 * variants).  For ASCII platforms and Unicode ranges on EBCDIC
                 * platforms, it's easy to calculate a precise number.  To
                 * start, we count the variants in the range, which we need
                 * elsewhere in this function anyway.  (For the case where it
                 * isn't easy to calculate, 'extras' has been initialized to 0,
                 * and the calculation is done in a loop further down.) */
#ifdef EBCDIC
                if (convert_unicode)
#endif
                {
                    /* This is executed unconditionally on ASCII, and for
                     * Unicode ranges on EBCDIC.  Under these conditions, all
                     * code points above a certain value are variant; and none
                     * under that value are.  We just need to find out how much
                     * of the range is above that value.  We don't count the
                     * end points here, as they will already have been counted
                     * as they were parsed. */
                    if (range_min >= UTF_CONTINUATION_MARK) {

                        /* The whole range is made up of variants */
                        extras = (range_max - 1) - (range_min + 1) + 1;
                    }
                    else if (range_max >= UTF_CONTINUATION_MARK) {

                        /* Only the higher portion of the range is variants */
                        extras = (range_max - 1) - UTF_CONTINUATION_MARK + 1;
                    }

                    utf8_variant_count += extras;
                }

                /* The base growth is the number of code points in the range,
                 * not including the endpoints, which have already been sized
                 * for (and output).  We don't subtract for the hyphen, as it
                 * has been parsed but not output, and the SvGROW below is
                 * based only on what's been output plus what's left to parse.
                 * */
                grow = (range_max - 1) - (range_min + 1) + 1;

                if (has_utf8) {
#ifdef EBCDIC
                    /* In some cases in EBCDIC, we haven't yet calculated a
                     * precise amount needed for the UTF-8 variants.  Just
                     * assume the worst case, that everything will expand by a
                     * byte */
                    if (! convert_unicode) {
                        grow *= 2;
                    }
                    else
#endif
                    {
                        /* Otherwise we know exactly how many variants there
                         * are in the range. */
                        grow += extras;
                    }
                }

                /* Grow, but position the output to overwrite the range min end
                 * point, because in some cases we overwrite that */
                SvCUR_set(sv, d - SvPVX_const(sv));
                offset_to_min = min_ptr - SvPVX_const(sv);

                /* See Note on sizing above. */
                d = offset_to_min + SvGROW(sv, SvCUR(sv)
                                             + (send - s)
                                             + grow
                                             + 1 /* Trailing NUL */ );

                /* Now, we can expand out the range. */
#ifdef EBCDIC
                if (convert_unicode) {
                    SSize_t i;

                    /* Recall that the min and max are now in Unicode terms, so
                     * we have to convert each character to its native
                     * equivalent */
                    if (has_utf8) {
                        for (i = range_min; i <= range_max; i++) {
                            append_utf8_from_native_byte(
                                                    LATIN1_TO_NATIVE((U8) i),
                                                    (U8 **) &d);
                        }
                    }
                    else {
                        for (i = range_min; i <= range_max; i++) {
                            *d++ = (char)LATIN1_TO_NATIVE((U8) i);
                        }
		    }
		}
                else
#endif
                /* Always gets run for ASCII, and sometimes for EBCDIC. */
                {
                    /* Here, no conversions are necessary, which means that the
                     * first character in the range is already in 'd' and
                     * valid, so we can skip overwriting it */
                    if (has_utf8) {
                        SSize_t i;
                        d += UTF8SKIP(d);
                        for (i = range_min + 1; i <= range_max; i++) {
                            append_utf8_from_native_byte((U8) i, (U8 **) &d);
                        }
                    }
                    else {
                        SSize_t i;
                        d++;
                        assert(range_min + 1 <= range_max);
                        for (i = range_min + 1; i < range_max; i++) {
#ifdef EBCDIC
                            /* In this case on EBCDIC, we haven't calculated
                             * the variants.  Do it here, as we go along */
                            if (! UVCHR_IS_INVARIANT(i)) {
                                utf8_variant_count++;
                            }
#endif
                            *d++ = (char)i;
                        }

                        /* The range_max is done outside the loop so as to
                         * avoid having to special case not incrementing
                         * 'utf8_variant_count' on EBCDIC (it's already been
                         * counted when originally parsed) */
                        *d++ = (char) range_max;
		    }
		}

#ifdef EBCDIC
                /* If the original range extended above 255, add in that
                 * portion. */
                if (real_range_max) {
                    *d++ = (char) UTF8_TWO_BYTE_HI(0x100);
                    *d++ = (char) UTF8_TWO_BYTE_LO(0x100);
                    if (real_range_max > 0x100) {
                        if (real_range_max > 0x101) {
                            *d++ = (char) ILLEGAL_UTF8_BYTE;
                        }
                        d = (char*)uvchr_to_utf8((U8*)d, real_range_max);
                    }
                }
#endif

              range_done:
		/* mark the range as done, and continue */
		didrange = TRUE;
		dorange = FALSE;
#ifdef EBCDIC
		non_portable_endpoint = 0;
                backslash_N = 0;
#endif
		continue;
	    } /* End of is a range */
        } /* End of transliteration.  Joins main code after these else's */
	else if (*s == '[' && PL_lex_inpat && !in_charclass) {
	    char *s1 = s-1;
	    int esc = 0;
	    while (s1 >= start && *s1-- == '\\')
		esc = !esc;
	    if (!esc)
		in_charclass = TRUE;
	}
	else if (*s == ']' && PL_lex_inpat && in_charclass) {
	    char *s1 = s-1;
	    int esc = 0;
	    while (s1 >= start && *s1-- == '\\')
		esc = !esc;
	    if (!esc)
		in_charclass = FALSE;
	}
            /* skip for regexp comments /(?#comment)/, except for the last
             * char, which will be done separately.  Stop on (?{..}) and
             * friends */
	else if (*s == '(' && PL_lex_inpat && s[1] == '?' && !in_charclass) {
	    if (s[2] == '#') {
		while (s+1 < send && *s != ')')
		    *d++ = *s++;
	    }
	    else if (!PL_lex_casemods
                     && (    s[2] == '{' /* This should match regcomp.c */
		         || (s[2] == '?' && s[3] == '{')))
	    {
		break;
	    }
	}
            /* likewise skip #-initiated comments in //x patterns */
	else if (*s == '#'
                 && PL_lex_inpat
                 && !in_charclass
                 && ((PMOP*)PL_lex_inpat)->op_pmflags & RXf_PMf_EXTENDED)
        {
	    while (s < send && *s != '\n')
		*d++ = *s++;
	}
            /* no further processing of single-quoted regex */
	else if (PL_lex_inpat && SvIVX(PL_linestr) == '\'')
	    goto default_action;

            /* check for embedded arrays
             * (@foo, @::foo, @'foo, @{foo}, @$foo, @+, @-)
             */
	else if (*s == '@' && s[1]) {
	    if (UTF
               ? isIDFIRST_utf8_safe(s+1, send)
               : isWORDCHAR_A(s[1]))
            {
		break;
            }
	    if (strchr(":'{$", s[1]))
		break;
	    if (!PL_lex_inpat && (s[1] == '+' || s[1] == '-'))
		break; /* in regexp, neither @+ nor @- are interpolated */
	}
            /* check for embedded scalars.  only stop if we're sure it's a
             * variable.  */
	else if (*s == '$') {
	    if (!PL_lex_inpat)	/* not a regexp, so $ must be var */
		break;
	    if (s + 1 < send && !strchr("()| \r\n\t", s[1])) {
		if (s[1] == '\\') {
		    Perl_ck_warner(aTHX_ packWARN(WARN_AMBIGUOUS),
				   "Possible unintended interpolation of $\\ in regex");
		}
		break;		/* in regexp, $ might be tail anchor */
            }
	}

	/* End of else if chain - OP_TRANS rejoin rest */

        if (UNLIKELY(s >= send)) {
            assert(s == send);
            break;
        }

	/* backslashes */
	if (*s == '\\' && s+1 < send) {
	    char* e;	/* Can be used for ending '}', etc. */

	    s++;

	    /* warn on \1 - \9 in substitution replacements, but note that \11
	     * is an octal; and \19 is \1 followed by '9' */
	    if (PL_lex_inwhat == OP_SUBST
                && !PL_lex_inpat
                && isDIGIT(*s)
                && *s != '0'
                && !isDIGIT(s[1]))
	    {
		/* diag_listed_as: \%d better written as $%d */
		Perl_ck_warner(aTHX_ packWARN(WARN_SYNTAX), "\\%c better written as $%c", *s, *s);
		*--s = '$';
		break;
	    }

	    /* string-change backslash escapes */
	    if (PL_lex_inwhat != OP_TRANS && *s && strchr("lLuUEQF", *s)) {
		--s;
		break;
	    }
	    /* In a pattern, process \N, but skip any other backslash escapes.
	     * This is because we don't want to translate an escape sequence
	     * into a meta symbol and have the regex compiler use the meta
	     * symbol meaning, e.g. \x{2E} would be confused with a dot.  But
	     * in spite of this, we do have to process \N here while the proper
	     * charnames handler is in scope.  See bugs #56444 and #62056.
             *
	     * There is a complication because \N in a pattern may also stand
	     * for 'match a non-nl', and not mean a charname, in which case its
	     * processing should be deferred to the regex compiler.  To be a
	     * charname it must be followed immediately by a '{', and not look
	     * like \N followed by a curly quantifier, i.e., not something like
	     * \N{3,}.  regcurly returns a boolean indicating if it is a legal
	     * quantifier */
	    else if (PL_lex_inpat
		    && (*s != 'N'
			|| s[1] != '{'
			|| regcurly(s + 1)))
	    {
		*d++ = '\\';
		goto default_action;
	    }

	    switch (*s) {
	    default:
	        {
		    if ((isALPHANUMERIC(*s)))
			Perl_ck_warner(aTHX_ packWARN(WARN_MISC),
				       "Unrecognized escape \\%c passed through",
				       *s);
		    /* default action is to copy the quoted character */
		    goto default_action;
		}

	    /* eg. \132 indicates the octal constant 0132 */
	    case '0': case '1': case '2': case '3':
	    case '4': case '5': case '6': case '7':
		{
                    I32 flags = PERL_SCAN_SILENT_ILLDIGIT;
                    STRLEN len = 3;
		    uv = grok_oct(s, &len, &flags, NULL);
		    s += len;
                    if (len < 3 && s < send && isDIGIT(*s)
                        && ckWARN(WARN_MISC))
                    {
                        Perl_warner(aTHX_ packWARN(WARN_MISC),
                                    "%s", form_short_octal_warning(s, len));
                    }
		}
		goto NUM_ESCAPE_INSERT;

	    /* eg. \o{24} indicates the octal constant \024 */
	    case 'o':
		{
		    const char* error;

		    bool valid = grok_bslash_o(&s, PL_bufend,
                                               &uv, &error,
                                               TRUE, /* Output warning */
                                               FALSE, /* Not strict */
                                               TRUE, /* Output warnings for
                                                         non-portables */
                                               UTF);
		    if (! valid) {
			yyerror(error);
			uv = 0; /* drop through to ensure range ends are set */
		    }
		    goto NUM_ESCAPE_INSERT;
		}

	    /* eg. \x24 indicates the hex constant 0x24 */
	    case 'x':
		{
		    const char* error;

		    bool valid = grok_bslash_x(&s, PL_bufend,
                                               &uv, &error,
                                               TRUE, /* Output warning */
                                               FALSE, /* Not strict */
                                               TRUE,  /* Output warnings for
                                                         non-portables */
                                               UTF);
		    if (! valid) {
			yyerror(error);
			uv = 0; /* drop through to ensure range ends are set */
		    }
		}

	      NUM_ESCAPE_INSERT:
		/* Insert oct or hex escaped character. */

		/* Here uv is the ordinal of the next character being added */
		if (UVCHR_IS_INVARIANT(uv)) {
		    *d++ = (char) uv;
		}
		else {
		    if (!has_utf8 && uv > 255) {

                        /* Here, 'uv' won't fit unless we convert to UTF-8.
                         * If we've only seen invariants so far, all we have to
                         * do is turn on the flag */
                        if (utf8_variant_count == 0) {
                            SvUTF8_on(sv);
                        }
                        else {
                            SvCUR_set(sv, d - SvPVX_const(sv));
                            SvPOK_on(sv);
                            *d = '\0';

                            sv_utf8_upgrade_flags_grow(
                                           sv,
                                           SV_GMAGIC|SV_FORCE_UTF8_UPGRADE,

                                           /* Since we're having to grow here,
                                            * make sure we have enough room for
                                            * this escape and a NUL, so the
                                            * code immediately below won't have
                                            * to actually grow again */
                                          UVCHR_SKIP(uv)
                                        + (STRLEN)(send - s) + 1);
                            d = SvPVX(sv) + SvCUR(sv);
                        }

                        has_above_latin1 = TRUE;
                        has_utf8 = TRUE;
                    }

                    if (! has_utf8) {
		        *d++ = (char)uv;
                        utf8_variant_count++;
                    }
		    else {
                       /* Usually, there will already be enough room in 'sv'
                        * since such escapes are likely longer than any UTF-8
                        * sequence they can end up as.  This isn't the case on
                        * EBCDIC where \x{40000000} contains 12 bytes, and the
                        * UTF-8 for it contains 14.  And, we have to allow for
                        * a trailing NUL.  It probably can't happen on ASCII
                        * platforms, but be safe.  See Note on sizing above. */
                        const STRLEN needed = d - SvPVX(sv)
                                            + UVCHR_SKIP(uv)
                                            + (send - s)
                                            + 1;
                        if (UNLIKELY(needed > SvLEN(sv))) {
                            SvCUR_set(sv, d - SvPVX_const(sv));
                            d = SvCUR(sv) + SvGROW(sv, needed);
                        }

		        d = (char*)uvchr_to_utf8((U8*)d, uv);
			if (PL_lex_inwhat == OP_TRANS
                            && PL_parser->lex_sub_op)
                        {
			    PL_parser->lex_sub_op->op_private |=
				(PL_lex_repl ? OPpTRANS_FROM_UTF
					     : OPpTRANS_TO_UTF);
			}
		    }
		}
#ifdef EBCDIC
                non_portable_endpoint++;
#endif
		continue;

 	    case 'N':
                /* In a non-pattern \N must be like \N{U+0041}, or it can be a
                 * named character, like \N{LATIN SMALL LETTER A}, or a named
                 * sequence, like \N{LATIN CAPITAL LETTER A WITH MACRON AND
                 * GRAVE} (except y/// can't handle the latter, croaking).  For
                 * convenience all three forms are referred to as "named
                 * characters" below.
                 *
                 * For patterns, \N also can mean to match a non-newline.  Code
                 * before this 'switch' statement should already have handled
                 * this situation, and hence this code only has to deal with
                 * the named character cases.
                 *
                 * For non-patterns, the named characters are converted to
                 * their string equivalents.  In patterns, named characters are
                 * not converted to their ultimate forms for the same reasons
                 * that other escapes aren't (mainly that the ultimate
                 * character could be considered a meta-symbol by the regex
                 * compiler).  Instead, they are converted to the \N{U+...}
                 * form to get the value from the charnames that is in effect
                 * right now, while preserving the fact that it was a named
                 * character, so that the regex compiler knows this.
                 *
		 * The structure of this section of code (besides checking for
		 * errors and upgrading to utf8) is:
                 *    If the named character is of the form \N{U+...}, pass it
                 *      through if a pattern; otherwise convert the code point
                 *      to utf8
                 *    Otherwise must be some \N{NAME}: convert to
                 *      \N{U+c1.c2...} if a pattern; otherwise convert to utf8
                 *
                 * Transliteration is an exception.  The conversion to utf8 is
                 * only done if the code point requires it to be representable.
                 *
                 * Here, 's' points to the 'N'; the test below is guaranteed to
		 * succeed if we are being called on a pattern, as we already
                 * know from a test above that the next character is a '{'.  A
                 * non-pattern \N must mean 'named character', which requires
                 * braces */
		s++;
		if (*s != '{') {
		    yyerror("Missing braces on \\N{}");
                    *d++ = '\0';
		    continue;
		}
		s++;

		/* If there is no matching '}', it is an error. */
		if (! (e = (char *) memchr(s, '}', send - s))) {
		    if (! PL_lex_inpat) {
			yyerror("Missing right brace on \\N{}");
		    } else {
			yyerror("Missing right brace on \\N{} or unescaped left brace after \\N");
		    }
                    yyquit(); /* Have exhausted the input. */
		}

		/* Here it looks like a named character */

		if (*s == 'U' && s[1] == '+') { /* \N{U+...} */
		    s += 2;	    /* Skip to next char after the 'U+' */
		    if (PL_lex_inpat) {

                        /* In patterns, we can have \N{U+xxxx.yyyy.zzzz...} */
                        /* Check the syntax.  */
                        const char *orig_s;
                        orig_s = s - 5;
                        if (!isXDIGIT(*s)) {
                          bad_NU:
                            yyerror(
                                "Invalid hexadecimal number in \\N{U+...}"
                            );
                            s = e + 1;
                            *d++ = '\0';
                            continue;
                        }
                        while (++s < e) {
                            if (isXDIGIT(*s))
                                continue;
                            else if ((*s == '.' || *s == '_')
                                  && isXDIGIT(s[1]))
                                continue;
                            goto bad_NU;
                        }

                        /* Pass everything through unchanged.
                         * +1 is for the '}' */
                        Copy(orig_s, d, e - orig_s + 1, char);
                        d += e - orig_s + 1;
		    }
		    else {  /* Not a pattern: convert the hex to string */
                        I32 flags = PERL_SCAN_ALLOW_UNDERSCORES
				| PERL_SCAN_SILENT_ILLDIGIT
				| PERL_SCAN_DISALLOW_PREFIX;
                        STRLEN len = e - s;
                        uv = grok_hex(s, &len, &flags, NULL);
                        if (len == 0 || (len != (STRLEN)(e - s)))
                            goto bad_NU;

                         /* For non-tr///, if the destination is not in utf8,
                          * unconditionally recode it to be so.  This is
                          * because \N{} implies Unicode semantics, and scalars
                          * have to be in utf8 to guarantee those semantics.
                          * tr/// doesn't care about Unicode rules, so no need
                          * there to upgrade to UTF-8 for small enough code
                          * points */
			if (! has_utf8 && (   uv > 0xFF
                                           || PL_lex_inwhat != OP_TRANS))
                        {
			    /* See Note on sizing above.  */
                            const STRLEN extra = OFFUNISKIP(uv) + (send - e) + 1;

			    SvCUR_set(sv, d - SvPVX_const(sv));
			    SvPOK_on(sv);
			    *d = '\0';

                            if (utf8_variant_count == 0) {
                                SvUTF8_on(sv);
                                d = SvCUR(sv) + SvGROW(sv, SvCUR(sv) + extra);
                            }
                            else {
                                sv_utf8_upgrade_flags_grow(
                                               sv,
                                               SV_GMAGIC|SV_FORCE_UTF8_UPGRADE,
                                               extra);
                                d = SvPVX(sv) + SvCUR(sv);
                            }

			    has_utf8 = TRUE;
                            has_above_latin1 = TRUE;
			}

                        /* Add the (Unicode) code point to the output. */
			if (! has_utf8 || OFFUNI_IS_INVARIANT(uv)) {
			    *d++ = (char) LATIN1_TO_NATIVE(uv);
			}
			else {
                            d = (char*) uvoffuni_to_utf8_flags((U8*)d, uv, 0);
                        }
		    }
		}
		else /* Here is \N{NAME} but not \N{U+...}. */
                     if ((res = get_and_check_backslash_N_name(s, e)))
                {
                    STRLEN len;
                    const char *str = SvPV_const(res, len);
                    if (PL_lex_inpat) {

			if (! len) { /* The name resolved to an empty string */
			    Copy("\\N{}", d, 4, char);
			    d += 4;
			}
			else {
			    /* In order to not lose information for the regex
			    * compiler, pass the result in the specially made
			    * syntax: \N{U+c1.c2.c3...}, where c1 etc. are
			    * the code points in hex of each character
			    * returned by charnames */

			    const char *str_end = str + len;
			    const STRLEN off = d - SvPVX_const(sv);

                            if (! SvUTF8(res)) {
                                /* For the non-UTF-8 case, we can determine the
                                 * exact length needed without having to parse
                                 * through the string.  Each character takes up
                                 * 2 hex digits plus either a trailing dot or
                                 * the "}" */
                                const char initial_text[] = "\\N{U+";
                                const STRLEN initial_len = sizeof(initial_text)
                                                           - 1;
                                d = off + SvGROW(sv, off
                                                    + 3 * len

                                                    /* +1 for trailing NUL */
                                                    + initial_len + 1

                                                    + (STRLEN)(send - e));
                                Copy(initial_text, d, initial_len, char);
                                d += initial_len;
                                while (str < str_end) {
                                    char hex_string[4];
                                    int len =
                                        my_snprintf(hex_string,
                                                  sizeof(hex_string),
                                                  "%02X.",

                                                  /* The regex compiler is
                                                   * expecting Unicode, not
                                                   * native */
                                                  NATIVE_TO_LATIN1(*str));
                                    PERL_MY_SNPRINTF_POST_GUARD(len,
                                                           sizeof(hex_string));
                                    Copy(hex_string, d, 3, char);
                                    d += 3;
                                    str++;
                                }
                                d--;    /* Below, we will overwrite the final
                                           dot with a right brace */
                            }
                            else {
                                STRLEN char_length; /* cur char's byte length */

                                /* and the number of bytes after this is
                                 * translated into hex digits */
                                STRLEN output_length;

                                /* 2 hex per byte; 2 chars for '\N'; 2 chars
                                 * for max('U+', '.'); and 1 for NUL */
                                char hex_string[2 * UTF8_MAXBYTES + 5];

                                /* Get the first character of the result. */
                                U32 uv = utf8n_to_uvchr((U8 *) str,
                                                        len,
                                                        &char_length,
                                                        UTF8_ALLOW_ANYUV);
                                /* Convert first code point to Unicode hex,
                                 * including the boiler plate before it. */
                                output_length =
                                    my_snprintf(hex_string, sizeof(hex_string),
                                             "\\N{U+%X",
                                             (unsigned int) NATIVE_TO_UNI(uv));

                                /* Make sure there is enough space to hold it */
                                d = off + SvGROW(sv, off
                                                    + output_length
                                                    + (STRLEN)(send - e)
                                                    + 2);	/* '}' + NUL */
                                /* And output it */
                                Copy(hex_string, d, output_length, char);
                                d += output_length;

                                /* For each subsequent character, append dot and
                                * its Unicode code point in hex */
                                while ((str += char_length) < str_end) {
                                    const STRLEN off = d - SvPVX_const(sv);
                                    U32 uv = utf8n_to_uvchr((U8 *) str,
                                                            str_end - str,
                                                            &char_length,
                                                            UTF8_ALLOW_ANYUV);
                                    output_length =
                                        my_snprintf(hex_string,
                                             sizeof(hex_string),
                                             ".%X",
                                             (unsigned int) NATIVE_TO_UNI(uv));

                                    d = off + SvGROW(sv, off
                                                        + output_length
                                                        + (STRLEN)(send - e)
                                                        + 2);	/* '}' +  NUL */
                                    Copy(hex_string, d, output_length, char);
                                    d += output_length;
                                }
			    }

			    *d++ = '}';	/* Done.  Add the trailing brace */
			}
		    }
		    else { /* Here, not in a pattern.  Convert the name to a
			    * string. */

                        if (PL_lex_inwhat == OP_TRANS) {
                            str = SvPV_const(res, len);
                            if (len > ((SvUTF8(res))
                                       ? UTF8SKIP(str)
                                       : 1U))
                            {
                                yyerror(Perl_form(aTHX_
                                    "%.*s must not be a named sequence"
                                    " in transliteration operator",
                                        /*  +1 to include the "}" */
                                    (int) (e + 1 - start), start));
                                *d++ = '\0';
                                goto end_backslash_N;
                            }

                            if (SvUTF8(res) && UTF8_IS_ABOVE_LATIN1(*str)) {
                                has_above_latin1 = TRUE;
                            }

                        }
                        else if (! SvUTF8(res)) {
                            /* Make sure \N{} return is UTF-8.  This is because
                             * \N{} implies Unicode semantics, and scalars have
                             * to be in utf8 to guarantee those semantics; but
                             * not needed in tr/// */
                            sv_utf8_upgrade_flags(res, 0);
                            str = SvPV_const(res, len);
                        }

                         /* Upgrade destination to be utf8 if this new
                          * component is */
			if (! has_utf8 && SvUTF8(res)) {
			    /* See Note on sizing above.  */
                            const STRLEN extra = len + (send - s) + 1;

			    SvCUR_set(sv, d - SvPVX_const(sv));
			    SvPOK_on(sv);
			    *d = '\0';

                            if (utf8_variant_count == 0) {
                                SvUTF8_on(sv);
                                d = SvCUR(sv) + SvGROW(sv, SvCUR(sv) + extra);
                            }
                            else {
                                sv_utf8_upgrade_flags_grow(sv,
						SV_GMAGIC|SV_FORCE_UTF8_UPGRADE,
						extra);
                                d = SvPVX(sv) + SvCUR(sv);
                            }
			    has_utf8 = TRUE;
			} else if (len > (STRLEN)(e - s + 4)) { /* I _guess_ 4 is \N{} --jhi */

			    /* See Note on sizing above.  (NOTE: SvCUR() is not
			     * set correctly here). */
                            const STRLEN extra = len + (send - e) + 1;
			    const STRLEN off = d - SvPVX_const(sv);
			    d = off + SvGROW(sv, off + extra);
			}
			Copy(str, d, len, char);
			d += len;
		    }

		    SvREFCNT_dec(res);

		} /* End \N{NAME} */

              end_backslash_N:
#ifdef EBCDIC
                backslash_N++; /* \N{} is defined to be Unicode */
#endif
		s = e + 1;  /* Point to just after the '}' */
		continue;

	    /* \c is a control character */
	    case 'c':
		s++;
		if (s < send) {
		    *d++ = grok_bslash_c(*s, 1);
		}
		else {
		    yyerror("Missing control char name in \\c");
		    yyquit();   /* Are at end of input, no sense continuing */
		}
#ifdef EBCDIC
                non_portable_endpoint++;
#endif
                break;

	    /* printf-style backslashes, formfeeds, newlines, etc */
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
		*d++ = ESC_NATIVE;
		break;
	    case 'a':
		*d++ = '\a';
		break;
	    } /* end switch */

	    s++;
	    continue;
	} /* end if (backslash) */

    default_action:
        /* Just copy the input to the output, though we may have to convert
         * to/from UTF-8.
         *
         * If the input has the same representation in UTF-8 as not, it will be
         * a single byte, and we don't care about UTF8ness; just copy the byte */
        if (NATIVE_BYTE_IS_INVARIANT((U8)(*s))) {
	    *d++ = *s++;
        }
        else if (! this_utf8 && ! has_utf8) {
            /* If neither source nor output is UTF-8, is also a single byte,
             * just copy it; but this byte counts should we later have to
             * convert to UTF-8 */
	    *d++ = *s++;
            utf8_variant_count++;
        }
        else if (this_utf8 && has_utf8) {   /* Both UTF-8, can just copy */
	    const STRLEN len = UTF8SKIP(s);

            /* We expect the source to have already been checked for
             * malformedness */
            assert(isUTF8_CHAR((U8 *) s, (U8 *) send));

            Copy(s, d, len, U8);
            d += len;
            s += len;
        }
        else { /* UTF8ness matters and doesn't match, need to convert */
	    STRLEN len = 1;
	    const UV nextuv   = (this_utf8)
                                ? utf8n_to_uvchr((U8*)s, send - s, &len, 0)
                                : (UV) ((U8) *s);
	    STRLEN need = UVCHR_SKIP(nextuv);

	    if (!has_utf8) {
		SvCUR_set(sv, d - SvPVX_const(sv));
		SvPOK_on(sv);
		*d = '\0';

                /* See Note on sizing above. */
                need += (STRLEN)(send - s) + 1;

                if (utf8_variant_count == 0) {
                    SvUTF8_on(sv);
                    d = SvCUR(sv) + SvGROW(sv, SvCUR(sv) + need);
                }
                else {
                    sv_utf8_upgrade_flags_grow(sv,
                                               SV_GMAGIC|SV_FORCE_UTF8_UPGRADE,
                                               need);
                    d = SvPVX(sv) + SvCUR(sv);
                }
		has_utf8 = TRUE;
	    } else if (need > len) {
		/* encoded value larger than old, may need extra space (NOTE:
		 * SvCUR() is not set correctly here).   See Note on sizing
		 * above.  */
                const STRLEN extra = need + (send - s) + 1;
		const STRLEN off = d - SvPVX_const(sv);
		d = off + SvGROW(sv, off + extra);
	    }
	    s += len;

	    d = (char*)uvchr_to_utf8((U8*)d, nextuv);
	}
    } /* while loop to process each character */

    /* terminate the string and set up the sv */
    *d = '\0';
    SvCUR_set(sv, d - SvPVX_const(sv));
    if (SvCUR(sv) >= SvLEN(sv))
	Perl_croak(aTHX_ "panic: constant overflowed allocated space, %" UVuf
		   " >= %" UVuf, (UV)SvCUR(sv), (UV)SvLEN(sv));

    SvPOK_on(sv);
    if (has_utf8) {
	SvUTF8_on(sv);
	if (PL_lex_inwhat == OP_TRANS && PL_parser->lex_sub_op) {
	    PL_parser->lex_sub_op->op_private |=
		    (PL_lex_repl ? OPpTRANS_FROM_UTF : OPpTRANS_TO_UTF);
	}
    }

    /* shrink the sv if we allocated more than we used */
    if (SvCUR(sv) + 5 < SvLEN(sv)) {
	SvPV_shrink_to_cur(sv);
    }

    /* return the substring (via pl_yylval) only if we parsed anything */
    if (s > start) {
	char *s2 = start;
	for (; s2 < s; s2++) {
	    if (*s2 == '\n')
		COPLINE_INC_WITH_HERELINES;
	}
	SvREFCNT_inc_simple_void_NN(sv);
	if (   (PL_hints & ( PL_lex_inpat ? HINT_NEW_RE : HINT_NEW_STRING ))
            && ! PL_parser->lex_re_reparsing)
        {
	    const char *const key = PL_lex_inpat ? "qr" : "q";
	    const STRLEN keylen = PL_lex_inpat ? 2 : 1;
	    const char *type;
	    STRLEN typelen;

	    if (PL_lex_inwhat == OP_TRANS) {
		type = "tr";
		typelen = 2;
	    } else if (PL_lex_inwhat == OP_SUBST && !PL_lex_inpat) {
		type = "s";
		typelen = 1;
	    } else if (PL_lex_inpat && SvIVX(PL_linestr) == '\'') {
		type = "q";
		typelen = 1;
	    } else  {
		type = "qq";
		typelen = 2;
	    }

	    sv = S_new_constant(aTHX_ start, s - start, key, keylen, sv, NULL,
				type, typelen);
	}
        pl_yylval.opval = newSVOP(OP_CONST, 0, sv);
    }
    LEAVE_with_name("scan_const");
    return s;
}

/* S_intuit_more
 * Returns TRUE if there's more to the expression (e.g., a subscript),
 * FALSE otherwise.
 *
 * It deals with "$foo[3]" and /$foo[3]/ and /$foo[0123456789$]+/
 *
 * ->[ and ->{ return TRUE
 * ->$* ->$#* ->@* ->@[ ->@{ return TRUE if postderef_qq is enabled
 * { and [ outside a pattern are always subscripts, so return TRUE
 * if we're outside a pattern and it's not { or [, then return FALSE
 * if we're in a pattern and the first char is a {
 *   {4,5} (any digits around the comma) returns FALSE
 * if we're in a pattern and the first char is a [
 *   [] returns FALSE
 *   [SOMETHING] has a funky algorithm to decide whether it's a
 *      character class or not.  It has to deal with things like
 *      /$foo[-3]/ and /$foo[$bar]/ as well as /$foo[$\d]+/
 * anything else returns TRUE
 */

/* This is the one truly awful dwimmer necessary to conflate C and sed. */

STATIC int
S_intuit_more(pTHX_ char *s, char *e)
{
    PERL_ARGS_ASSERT_INTUIT_MORE;

    if (PL_lex_brackets)
	return TRUE;
    if (*s == '-' && s[1] == '>' && (s[2] == '[' || s[2] == '{'))
	return TRUE;
    if (*s == '-' && s[1] == '>'
     && FEATURE_POSTDEREF_QQ_IS_ENABLED
     && ( (s[2] == '$' && (s[3] == '*' || (s[3] == '#' && s[4] == '*')))
	||(s[2] == '@' && strchr("*[{",s[3])) ))
	return TRUE;
    if (*s != '{' && *s != '[')
	return FALSE;
    PL_parser->sub_no_recover = TRUE;
    if (!PL_lex_inpat)
	return TRUE;

    /* In a pattern, so maybe we have {n,m}. */
    if (*s == '{') {
	if (regcurly(s)) {
	    return FALSE;
	}
	return TRUE;
    }

    /* On the other hand, maybe we have a character class */

    s++;
    if (*s == ']' || *s == '^')
	return FALSE;
    else {
        /* this is terrifying, and it works */
	int weight;
	char seen[256];
	const char * const send = (char *) memchr(s, ']', e - s);
	unsigned char un_char, last_un_char;
	char tmpbuf[sizeof PL_tokenbuf * 4];

	if (!send)		/* has to be an expression */
	    return TRUE;
	weight = 2;		/* let's weigh the evidence */

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
	Zero(seen,256,char);
	un_char = 255;
	for (; s < send; s++) {
	    last_un_char = un_char;
	    un_char = (unsigned char)*s;
	    switch (*s) {
	    case '@':
	    case '&':
	    case '$':
		weight -= seen[un_char] * 10;
	        if (isWORDCHAR_lazy_if_safe(s+1, PL_bufend, UTF)) {
		    int len;
		    scan_ident(s, tmpbuf, sizeof tmpbuf, FALSE);
		    len = (int)strlen(tmpbuf);
		    if (len > 1 && gv_fetchpvn_flags(tmpbuf, len,
                                                    UTF ? SVf_UTF8 : 0, SVt_PV))
			weight -= 100;
		    else
			weight -= 10;
		}
		else if (*s == '$'
                         && s[1]
                         && strchr("[#!%*<>()-=",s[1]))
                {
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
		    else if (seen[(U8)'\''] || seen[(U8)'"'])
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
		if (last_un_char == 255 && (isDIGIT(s[1]) || s[1] == '$'))
		    weight -= 5;	/* cope with negative subscript */
		break;
	    default:
		if (!isWORDCHAR(last_un_char)
		    && !(last_un_char == '$' || last_un_char == '@'
			 || last_un_char == '&')
		    && isALPHA(*s) && s[1] && isALPHA(s[1])) {
		    char *d = s;
		    while (isALPHA(*s))
			s++;
		    if (keyword(d, s - d, 0))
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

/*
 * S_intuit_method
 *
 * Does all the checking to disambiguate
 *   foo bar
 * between foo(bar) and bar->foo.  Returns 0 if not a method, otherwise
 * FUNCMETH (bar->foo(args)) or METHOD (bar->foo args).
 *
 * First argument is the stuff after the first token, e.g. "bar".
 *
 * Not a method if foo is a filehandle.
 * Not a method if foo is a subroutine prototyped to take a filehandle.
 * Not a method if it's really "Foo $bar"
 * Method if it's "foo $bar"
 * Not a method if it's really "print foo $bar"
 * Method if it's really "foo package::" (interpreted as package->foo)
 * Not a method if bar is known to be a subroutine ("sub bar; foo bar")
 * Not a method if bar is a filehandle or package, but is quoted with
 *   =>
 */

STATIC int
S_intuit_method(pTHX_ char *start, SV *ioname, CV *cv)
{
    char *s = start + (*start == '$');
    char tmpbuf[sizeof PL_tokenbuf];
    STRLEN len;
    GV* indirgv;
	/* Mustn't actually add anything to a symbol table.
	   But also don't want to "initialise" any placeholder
	   constants that might already be there into full
	   blown PVGVs with attached PVCV.  */
    GV * const gv =
	ioname ? gv_fetchsv(ioname, GV_NOADD_NOINIT, SVt_PVCV) : NULL;

    PERL_ARGS_ASSERT_INTUIT_METHOD;

    if (gv && SvTYPE(gv) == SVt_PVGV && GvIO(gv))
	    return 0;
    if (cv && SvPOK(cv)) {
	const char *proto = CvPROTO(cv);
	if (proto) {
	    while (*proto && (isSPACE(*proto) || *proto == ';'))
		proto++;
	    if (*proto == '*')
		return 0;
	}
    }

    if (*start == '$') {
        SSize_t start_off = start - SvPVX(PL_linestr);
	if (cv || PL_last_lop_op == OP_PRINT || PL_last_lop_op == OP_SAY
            || isUPPER(*PL_tokenbuf))
	    return 0;
        /* this could be $# */
        if (isSPACE(*s))
            s = skipspace(s);
	PL_bufptr = SvPVX(PL_linestr) + start_off;
	PL_expect = XREF;
	return *s == '(' ? FUNCMETH : METHOD;
    }

    s = scan_word(s, tmpbuf, sizeof tmpbuf, TRUE, &len);
    /* start is the beginning of the possible filehandle/object,
     * and s is the end of it
     * tmpbuf is a copy of it (but with single quotes as double colons)
     */

    if (!keyword(tmpbuf, len, 0)) {
	if (len > 2 && tmpbuf[len - 2] == ':' && tmpbuf[len - 1] == ':') {
	    len -= 2;
	    tmpbuf[len] = '\0';
	    goto bare_package;
	}
	indirgv = gv_fetchpvn_flags(tmpbuf, len,
				    GV_NOADD_NOINIT|( UTF ? SVf_UTF8 : 0 ),
				    SVt_PVCV);
	if (indirgv && SvTYPE(indirgv) != SVt_NULL
	 && (!isGV(indirgv) || GvCVu(indirgv)))
	    return 0;
	/* filehandle or package name makes it a method */
	if (!cv || GvIO(indirgv) || gv_stashpvn(tmpbuf, len, UTF ? SVf_UTF8 : 0)) {
	    s = skipspace(s);
	    if ((PL_bufend - s) >= 2 && *s == '=' && *(s+1) == '>')
		return 0;	/* no assumptions -- "=>" quotes bareword */
      bare_package:
            NEXTVAL_NEXTTOKE.opval = newSVOP(OP_CONST, 0,
						  S_newSV_maybe_utf8(aTHX_ tmpbuf, len));
	    NEXTVAL_NEXTTOKE.opval->op_private = OPpCONST_BARE;
	    PL_expect = XTERM;
	    force_next(BAREWORD);
	    PL_bufptr = s;
	    return *s == '(' ? FUNCMETH : METHOD;
	}
    }
    return 0;
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
 * and the IoDIRP/IoANY field is used to store the function pointer,
 * and IOf_FAKE_DIRP is enabled on datasv to mark this as such.
 * Note that IoTOP_NAME, IoFMT_NAME, IoBOTTOM_NAME, if set for
 * private use must be set using malloc'd pointers.
 */

SV *
Perl_filter_add(pTHX_ filter_t funcp, SV *datasv)
{
    if (!funcp)
	return NULL;

    if (!PL_parser)
	return NULL;

    if (PL_parser->lex_flags & LEX_IGNORE_UTF8_HINTS)
	Perl_croak(aTHX_ "Source filters apply only to byte streams");

    if (!PL_rsfp_filters)
	PL_rsfp_filters = newAV();
    if (!datasv)
	datasv = newSV(0);
    SvUPGRADE(datasv, SVt_PVIO);
    IoANY(datasv) = FPTR2DPTR(void *, funcp); /* stash funcp into spare field */
    IoFLAGS(datasv) |= IOf_FAKE_DIRP;
    DEBUG_P(PerlIO_printf(Perl_debug_log, "filter_add func %p (%s)\n",
			  FPTR2DPTR(void *, IoANY(datasv)),
			  SvPV_nolen(datasv)));
    av_unshift(PL_rsfp_filters, 1);
    av_store(PL_rsfp_filters, 0, datasv) ;
    if (
	!PL_parser->filtered
     && PL_parser->lex_flags & LEX_EVALBYTES
     && PL_bufptr < PL_bufend
    ) {
	const char *s = PL_bufptr;
	while (s < PL_bufend) {
	    if (*s == '\n') {
		SV *linestr = PL_parser->linestr;
		char *buf = SvPVX(linestr);
		STRLEN const bufptr_pos = PL_parser->bufptr - buf;
		STRLEN const oldbufptr_pos = PL_parser->oldbufptr - buf;
		STRLEN const oldoldbufptr_pos=PL_parser->oldoldbufptr-buf;
		STRLEN const linestart_pos = PL_parser->linestart - buf;
		STRLEN const last_uni_pos =
		    PL_parser->last_uni ? PL_parser->last_uni - buf : 0;
		STRLEN const last_lop_pos =
		    PL_parser->last_lop ? PL_parser->last_lop - buf : 0;
		av_push(PL_rsfp_filters, linestr);
		PL_parser->linestr =
		    newSVpvn(SvPVX(linestr), ++s-SvPVX(linestr));
		buf = SvPVX(PL_parser->linestr);
		PL_parser->bufend = buf + SvCUR(PL_parser->linestr);
		PL_parser->bufptr = buf + bufptr_pos;
		PL_parser->oldbufptr = buf + oldbufptr_pos;
		PL_parser->oldoldbufptr = buf + oldoldbufptr_pos;
		PL_parser->linestart = buf + linestart_pos;
		if (PL_parser->last_uni)
		    PL_parser->last_uni = buf + last_uni_pos;
		if (PL_parser->last_lop)
		    PL_parser->last_lop = buf + last_lop_pos;
		SvLEN_set(linestr, SvCUR(linestr));
		SvCUR_set(linestr, s - SvPVX(linestr));
		PL_parser->filtered = 1;
		break;
	    }
	    s++;
	}
    }
    return(datasv);
}


/* Delete most recently added instance of this filter function.	*/
void
Perl_filter_del(pTHX_ filter_t funcp)
{
    SV *datasv;

    PERL_ARGS_ASSERT_FILTER_DEL;

#ifdef DEBUGGING
    DEBUG_P(PerlIO_printf(Perl_debug_log, "filter_del func %p",
			  FPTR2DPTR(void*, funcp)));
#endif
    if (!PL_parser || !PL_rsfp_filters || AvFILLp(PL_rsfp_filters)<0)
	return;
    /* if filter is on top of stack (usual case) just pop it off */
    datasv = FILTER_DATA(AvFILLp(PL_rsfp_filters));
    if (IoANY(datasv) == FPTR2DPTR(void *, funcp)) {
	sv_free(av_pop(PL_rsfp_filters));

        return;
    }
    /* we need to search for the correct entry and clear it	*/
    Perl_die(aTHX_ "filter_del can only delete in reverse order (currently)");
}


/* Invoke the idxth filter function for the current rsfp.	 */
/* maxlen 0 = read one text line */
I32
Perl_filter_read(pTHX_ int idx, SV *buf_sv, int maxlen)
{
    filter_t funcp;
    I32 ret;
    SV *datasv = NULL;
    /* This API is bad. It should have been using unsigned int for maxlen.
       Not sure if we want to change the API, but if not we should sanity
       check the value here.  */
    unsigned int correct_length = maxlen < 0 ?  PERL_INT_MAX : maxlen;

    PERL_ARGS_ASSERT_FILTER_READ;

    if (!PL_parser || !PL_rsfp_filters)
	return -1;
    if (idx > AvFILLp(PL_rsfp_filters)) {       /* Any more filters?	*/
	/* Provide a default input filter to make life easy.	*/
	/* Note that we append to the line. This is handy.	*/
	DEBUG_P(PerlIO_printf(Perl_debug_log,
			      "filter_read %d: from rsfp\n", idx));
	if (correct_length) {
 	    /* Want a block */
	    int len ;
	    const int old_len = SvCUR(buf_sv);

	    /* ensure buf_sv is large enough */
	    SvGROW(buf_sv, (STRLEN)(old_len + correct_length + 1)) ;
	    if ((len = PerlIO_read(PL_rsfp, SvPVX(buf_sv) + old_len,
				   correct_length)) <= 0) {
		if (PerlIO_error(PL_rsfp))
	            return -1;		/* error */
	        else
		    return 0 ;		/* end of file */
	    }
	    SvCUR_set(buf_sv, old_len + len) ;
	    SvPVX(buf_sv)[old_len + len] = '\0';
	} else {
	    /* Want a line */
            if (sv_gets(buf_sv, PL_rsfp, SvCUR(buf_sv)) == NULL) {
		if (PerlIO_error(PL_rsfp))
	            return -1;		/* error */
	        else
		    return 0 ;		/* end of file */
	    }
	}
	return SvCUR(buf_sv);
    }
    /* Skip this filter slot if filter has been deleted	*/
    if ( (datasv = FILTER_DATA(idx)) == &PL_sv_undef) {
	DEBUG_P(PerlIO_printf(Perl_debug_log,
			      "filter_read %d: skipped (filter deleted)\n",
			      idx));
	return FILTER_READ(idx+1, buf_sv, correct_length); /* recurse */
    }
    if (SvTYPE(datasv) != SVt_PVIO) {
	if (correct_length) {
 	    /* Want a block */
	    const STRLEN remainder = SvLEN(datasv) - SvCUR(datasv);
	    if (!remainder) return 0; /* eof */
	    if (correct_length > remainder) correct_length = remainder;
	    sv_catpvn(buf_sv, SvEND(datasv), correct_length);
	    SvCUR_set(datasv, SvCUR(datasv) + correct_length);
	} else {
	    /* Want a line */
	    const char *s = SvEND(datasv);
	    const char *send = SvPVX(datasv) + SvLEN(datasv);
	    while (s < send) {
		if (*s == '\n') {
		    s++;
		    break;
		}
		s++;
	    }
	    if (s == send) return 0; /* eof */
	    sv_catpvn(buf_sv, SvEND(datasv), s-SvEND(datasv));
	    SvCUR_set(datasv, s-SvPVX(datasv));
	}
	return SvCUR(buf_sv);
    }
    /* Get function pointer hidden within datasv	*/
    funcp = DPTR2FPTR(filter_t, IoANY(datasv));
    DEBUG_P(PerlIO_printf(Perl_debug_log,
			  "filter_read %d: via function %p (%s)\n",
			  idx, (void*)datasv, SvPV_nolen_const(datasv)));
    /* Call function. The function is expected to 	*/
    /* call "FILTER_READ(idx+1, buf_sv)" first.		*/
    /* Return: <0:error, =0:eof, >0:not eof 		*/
    ENTER;
    save_scalar(PL_errgv);
    ret = (*funcp)(aTHX_ idx, buf_sv, correct_length);
    LEAVE;
    return ret;
}

STATIC char *
S_filter_gets(pTHX_ SV *sv, STRLEN append)
{
    PERL_ARGS_ASSERT_FILTER_GETS;

#ifdef PERL_CR_FILTER
    if (!PL_rsfp_filters) {
	filter_add(S_cr_textfilter,NULL);
    }
#endif
    if (PL_rsfp_filters) {
	if (!append)
            SvCUR_set(sv, 0);	/* start with empty line	*/
        if (FILTER_READ(0, sv, 0) > 0)
            return ( SvPVX(sv) ) ;
        else
	    return NULL ;
    }
    else
        return (sv_gets(sv, PL_rsfp, append));
}

STATIC HV *
S_find_in_my_stash(pTHX_ const char *pkgname, STRLEN len)
{
    GV *gv;

    PERL_ARGS_ASSERT_FIND_IN_MY_STASH;

    if (memEQs(pkgname, len, "__PACKAGE__"))
        return PL_curstash;

    if (len > 2
        && (pkgname[len - 2] == ':' && pkgname[len - 1] == ':')
        && (gv = gv_fetchpvn_flags(pkgname,
                                   len,
                                   ( UTF ? SVf_UTF8 : 0 ), SVt_PVHV)))
    {
        return GvHV(gv);			/* Foo:: */
    }

    /* use constant CLASS => 'MyClass' */
    gv = gv_fetchpvn_flags(pkgname, len, UTF ? SVf_UTF8 : 0, SVt_PVCV);
    if (gv && GvCV(gv)) {
	SV * const sv = cv_const_sv(GvCV(gv));
	if (sv)
	    return gv_stashsv(sv, 0);
    }

    return gv_stashpvn(pkgname, len, UTF ? SVf_UTF8 : 0);
}


STATIC char *
S_tokenize_use(pTHX_ int is_use, char *s) {
    PERL_ARGS_ASSERT_TOKENIZE_USE;

    if (PL_expect != XSTATE)
	/* diag_listed_as: "use" not allowed in expression */
	yyerror(Perl_form(aTHX_ "\"%s\" not allowed in expression",
		    is_use ? "use" : "no"));
    PL_expect = XTERM;
    s = skipspace(s);
    if (isDIGIT(*s) || (*s == 'v' && isDIGIT(s[1]))) {
	s = force_version(s, TRUE);
	if (*s == ';' || *s == '}'
		|| (s = skipspace(s), (*s == ';' || *s == '}'))) {
	    NEXTVAL_NEXTTOKE.opval = NULL;
	    force_next(BAREWORD);
	}
	else if (*s == 'v') {
	    s = force_word(s,BAREWORD,FALSE,TRUE);
	    s = force_version(s, FALSE);
	}
    }
    else {
	s = force_word(s,BAREWORD,FALSE,TRUE);
	s = force_version(s, FALSE);
    }
    pl_yylval.ival = is_use;
    return s;
}
#ifdef DEBUGGING
    static const char* const exp_name[] =
	{ "OPERATOR", "TERM", "REF", "STATE", "BLOCK", "ATTRBLOCK",
	  "ATTRTERM", "TERMBLOCK", "XBLOCKTERM", "POSTDEREF",
	  "SIGVAR", "TERMORDORDOR"
	};
#endif

#define word_takes_any_delimiter(p,l) S_word_takes_any_delimiter(p,l)
STATIC bool
S_word_takes_any_delimiter(char *p, STRLEN len)
{
    return (len == 1 && strchr("msyq", p[0]))
            || (len == 2
                && ((p[0] == 't' && p[1] == 'r')
                    || (p[0] == 'q' && strchr("qwxr", p[1]))));
}

static void
S_check_scalar_slice(pTHX_ char *s)
{
    s++;
    while (SPACE_OR_TAB(*s)) s++;
    if (*s == 'q' && s[1] == 'w' && !isWORDCHAR_lazy_if_safe(s+2,
                                                             PL_bufend,
                                                             UTF))
    {
	return;
    }
    while (    isWORDCHAR_lazy_if_safe(s, PL_bufend, UTF)
           || (*s && strchr(" \t$#+-'\"", *s)))
    {
        s += UTF ? UTF8SKIP(s) : 1;
    }
    if (*s == '}' || *s == ']')
	pl_yylval.ival = OPpSLICEWARNING;
}

#define lex_token_boundary() S_lex_token_boundary(aTHX)
static void
S_lex_token_boundary(pTHX)
{
    PL_oldoldbufptr = PL_oldbufptr;
    PL_oldbufptr = PL_bufptr;
}

#define vcs_conflict_marker(s) S_vcs_conflict_marker(aTHX_ s)
static char *
S_vcs_conflict_marker(pTHX_ char *s)
{
    lex_token_boundary();
    PL_bufptr = s;
    yyerror("Version control conflict marker");
    while (s < PL_bufend && *s != '\n')
	s++;
    return s;
}

/*
  yylex

  Works out what to call the token just pulled out of the input
  stream.  The yacc parser takes care of taking the ops we return and
  stitching them into a tree.

  Returns:
    The type of the next token

  Structure:
      Check if we have already built the token; if so, use it.
      Switch based on the current state:
	  - if we have a case modifier in a string, deal with that
	  - handle other cases of interpolation inside a string
	  - scan the next line if we are inside a format
      In the normal state, switch on the next character:
	  - default:
	    if alphabetic, go to key lookup
	    unrecognized character - croak
	  - 0/4/26: handle end-of-line or EOF
	  - cases for whitespace
	  - \n and #: handle comments and line numbers
	  - various operators, brackets and sigils
	  - numbers
	  - quotes
	  - 'v': vstrings (or go to key lookup)
	  - 'x' repetition operator (or go to key lookup)
	  - other ASCII alphanumerics (key lookup begins here):
	      word before => ?
	      keyword plugin
	      scan built-in keyword (but do nothing with it yet)
	      check for statement label
	      check for lexical subs
		  goto just_a_word if there is one
	      see whether built-in keyword is overridden
	      switch on keyword number:
		  - default: just_a_word:
		      not a built-in keyword; handle bareword lookup
		      disambiguate between method and sub call
		      fall back to bareword
		  - cases for built-in keywords
*/


int
Perl_yylex(pTHX)
{
    dVAR;
    char *s = PL_bufptr;
    char *d;
    STRLEN len;
    bool bof = FALSE;
    const bool saw_infix_sigil = cBOOL(PL_parser->saw_infix_sigil);
    U8 formbrack = 0;
    U32 fake_eof = 0;

    /* orig_keyword, gvp, and gv are initialized here because
     * jump to the label just_a_word_zero can bypass their
     * initialization later. */
    I32 orig_keyword = 0;
    GV *gv = NULL;
    GV **gvp = NULL;

    if (UNLIKELY(PL_parser->recheck_utf8_validity)) {
        const U8* first_bad_char_loc;
        if (UTF && UNLIKELY(! is_utf8_string_loc((U8 *) PL_bufptr,
                                                        PL_bufend - PL_bufptr,
                                                        &first_bad_char_loc)))
        {
            _force_out_malformed_utf8_message(first_bad_char_loc,
                                              (U8 *) PL_bufend,
                                              0,
                                              1 /* 1 means die */ );
            NOT_REACHED; /* NOTREACHED */
        }
        PL_parser->recheck_utf8_validity = FALSE;
    }
    DEBUG_T( {
	SV* tmp = newSVpvs("");
	PerlIO_printf(Perl_debug_log, "### %" IVdf ":LEX_%s/X%s %s\n",
	    (IV)CopLINE(PL_curcop),
	    lex_state_names[PL_lex_state],
	    exp_name[PL_expect],
	    pv_display(tmp, s, strlen(s), 0, 60));
	SvREFCNT_dec(tmp);
    } );

    /* when we've already built the next token, just pull it out of the queue */
    if (PL_nexttoke) {
	PL_nexttoke--;
	pl_yylval = PL_nextval[PL_nexttoke];
	{
	    I32 next_type;
	    next_type = PL_nexttype[PL_nexttoke];
	    if (next_type & (7<<24)) {
		if (next_type & (1<<24)) {
		    if (PL_lex_brackets > 100)
			Renew(PL_lex_brackstack, PL_lex_brackets + 10, char);
		    PL_lex_brackstack[PL_lex_brackets++] =
			(char) ((next_type >> 16) & 0xff);
		}
		if (next_type & (2<<24))
		    PL_lex_allbrackets++;
		if (next_type & (4<<24))
		    PL_lex_allbrackets--;
		next_type &= 0xffff;
	    }
	    return REPORT(next_type == 'p' ? pending_ident() : next_type);
	}
    }

    switch (PL_lex_state) {
    case LEX_NORMAL:
    case LEX_INTERPNORMAL:
	break;

    /* interpolated case modifiers like \L \U, including \Q and \E.
       when we get here, PL_bufptr is at the \
    */
    case LEX_INTERPCASEMOD:
#ifdef DEBUGGING
	if (PL_bufptr != PL_bufend && *PL_bufptr != '\\')
	    Perl_croak(aTHX_
		       "panic: INTERPCASEMOD bufptr=%p, bufend=%p, *bufptr=%u",
		       PL_bufptr, PL_bufend, *PL_bufptr);
#endif
	/* handle \E or end of string */
       	if (PL_bufptr == PL_bufend || PL_bufptr[1] == 'E') {
	    /* if at a \E */
	    if (PL_lex_casemods) {
		const char oldmod = PL_lex_casestack[--PL_lex_casemods];
		PL_lex_casestack[PL_lex_casemods] = '\0';

		if (PL_bufptr != PL_bufend
		    && (oldmod == 'L' || oldmod == 'U' || oldmod == 'Q'
                        || oldmod == 'F')) {
		    PL_bufptr += 2;
		    PL_lex_state = LEX_INTERPCONCAT;
		}
		PL_lex_allbrackets--;
		return REPORT(')');
	    }
            else if ( PL_bufptr != PL_bufend && PL_bufptr[1] == 'E' ) {
               /* Got an unpaired \E */
               Perl_ck_warner(aTHX_ packWARN(WARN_MISC),
                        "Useless use of \\E");
            }
	    if (PL_bufptr != PL_bufend)
		PL_bufptr += 2;
	    PL_lex_state = LEX_INTERPCONCAT;
	    return yylex();
	}
	else {
	    DEBUG_T({ PerlIO_printf(Perl_debug_log,
              "### Saw case modifier\n"); });
	    s = PL_bufptr + 1;
	    if (s[1] == '\\' && s[2] == 'E') {
	        PL_bufptr = s + 3;
		PL_lex_state = LEX_INTERPCONCAT;
		return yylex();
	    }
	    else {
		I32 tmp;
                if (   memBEGINs(s, (STRLEN) (PL_bufend - s), "L\\u")
                    || memBEGINs(s, (STRLEN) (PL_bufend - s), "U\\l"))
                {
                    tmp = *s, *s = s[2], s[2] = (char)tmp;	/* misordered... */
                }
		if ((*s == 'L' || *s == 'U' || *s == 'F')
                    && (strpbrk(PL_lex_casestack, "LUF")))
                {
		    PL_lex_casestack[--PL_lex_casemods] = '\0';
		    PL_lex_allbrackets--;
		    return REPORT(')');
		}
		if (PL_lex_casemods > 10)
		    Renew(PL_lex_casestack, PL_lex_casemods + 2, char);
		PL_lex_casestack[PL_lex_casemods++] = *s;
		PL_lex_casestack[PL_lex_casemods] = '\0';
		PL_lex_state = LEX_INTERPCONCAT;
		NEXTVAL_NEXTTOKE.ival = 0;
		force_next((2<<24)|'(');
		if (*s == 'l')
		    NEXTVAL_NEXTTOKE.ival = OP_LCFIRST;
		else if (*s == 'u')
		    NEXTVAL_NEXTTOKE.ival = OP_UCFIRST;
		else if (*s == 'L')
		    NEXTVAL_NEXTTOKE.ival = OP_LC;
		else if (*s == 'U')
		    NEXTVAL_NEXTTOKE.ival = OP_UC;
		else if (*s == 'Q')
		    NEXTVAL_NEXTTOKE.ival = OP_QUOTEMETA;
                else if (*s == 'F')
		    NEXTVAL_NEXTTOKE.ival = OP_FC;
		else
		    Perl_croak(aTHX_ "panic: yylex, *s=%u", *s);
		PL_bufptr = s + 1;
	    }
	    force_next(FUNC);
	    if (PL_lex_starts) {
		s = PL_bufptr;
		PL_lex_starts = 0;
		/* commas only at base level: /$a\Ub$c/ => ($a,uc(b.$c)) */
		if (PL_lex_casemods == 1 && PL_lex_inpat)
		    TOKEN(',');
		else
		    AopNOASSIGN(OP_CONCAT);
	    }
	    else
		return yylex();
	}

    case LEX_INTERPPUSH:
        return REPORT(sublex_push());

    case LEX_INTERPSTART:
	if (PL_bufptr == PL_bufend)
	    return REPORT(sublex_done());
	DEBUG_T({ if(*PL_bufptr != '(') PerlIO_printf(Perl_debug_log,
              "### Interpolated variable\n"); });
	PL_expect = XTERM;
        /* for /@a/, we leave the joining for the regex engine to do
         * (unless we're within \Q etc) */
	PL_lex_dojoin = (*PL_bufptr == '@'
                            && (!PL_lex_inpat || PL_lex_casemods));
	PL_lex_state = LEX_INTERPNORMAL;
	if (PL_lex_dojoin) {
	    NEXTVAL_NEXTTOKE.ival = 0;
	    force_next(',');
	    force_ident("\"", '$');
	    NEXTVAL_NEXTTOKE.ival = 0;
	    force_next('$');
	    NEXTVAL_NEXTTOKE.ival = 0;
	    force_next((2<<24)|'(');
	    NEXTVAL_NEXTTOKE.ival = OP_JOIN;	/* emulate join($", ...) */
	    force_next(FUNC);
	}
	/* Convert (?{...}) and friends to 'do {...}' */
	if (PL_lex_inpat && *PL_bufptr == '(') {
	    PL_parser->lex_shared->re_eval_start = PL_bufptr;
	    PL_bufptr += 2;
	    if (*PL_bufptr != '{')
		PL_bufptr++;
	    PL_expect = XTERMBLOCK;
	    force_next(DO);
	}

	if (PL_lex_starts++) {
	    s = PL_bufptr;
	    /* commas only at base level: /$a\Ub$c/ => ($a,uc(b.$c)) */
	    if (!PL_lex_casemods && PL_lex_inpat)
		TOKEN(',');
	    else
		AopNOASSIGN(OP_CONCAT);
	}
	return yylex();

    case LEX_INTERPENDMAYBE:
	if (intuit_more(PL_bufptr, PL_bufend)) {
	    PL_lex_state = LEX_INTERPNORMAL;	/* false alarm, more expr */
	    break;
	}
	/* FALLTHROUGH */

    case LEX_INTERPEND:
	if (PL_lex_dojoin) {
	    const U8 dojoin_was = PL_lex_dojoin;
	    PL_lex_dojoin = FALSE;
	    PL_lex_state = LEX_INTERPCONCAT;
	    PL_lex_allbrackets--;
	    return REPORT(dojoin_was == 1 ? (int)')' : (int)POSTJOIN);
	}
	if (PL_lex_inwhat == OP_SUBST && PL_linestr == PL_lex_repl
	    && SvEVALED(PL_lex_repl))
	{
	    if (PL_bufptr != PL_bufend)
		Perl_croak(aTHX_ "Bad evalled substitution pattern");
	    PL_lex_repl = NULL;
	}
	/* Paranoia.  re_eval_start is adjusted when S_scan_heredoc sets
	   re_eval_str.  If the here-doc body’s length equals the previous
	   value of re_eval_start, re_eval_start will now be null.  So
	   check re_eval_str as well. */
	if (PL_parser->lex_shared->re_eval_start
	 || PL_parser->lex_shared->re_eval_str) {
	    SV *sv;
	    if (*PL_bufptr != ')')
		Perl_croak(aTHX_ "Sequence (?{...}) not terminated with ')'");
	    PL_bufptr++;
	    /* having compiled a (?{..}) expression, return the original
	     * text too, as a const */
	    if (PL_parser->lex_shared->re_eval_str) {
		sv = PL_parser->lex_shared->re_eval_str;
		PL_parser->lex_shared->re_eval_str = NULL;
		SvCUR_set(sv,
			 PL_bufptr - PL_parser->lex_shared->re_eval_start);
		SvPV_shrink_to_cur(sv);
	    }
	    else sv = newSVpvn(PL_parser->lex_shared->re_eval_start,
			 PL_bufptr - PL_parser->lex_shared->re_eval_start);
	    NEXTVAL_NEXTTOKE.opval =
                    newSVOP(OP_CONST, 0,
				 sv);
	    force_next(THING);
	    PL_parser->lex_shared->re_eval_start = NULL;
	    PL_expect = XTERM;
	    return REPORT(',');
	}

	/* FALLTHROUGH */
    case LEX_INTERPCONCAT:
#ifdef DEBUGGING
	if (PL_lex_brackets)
	    Perl_croak(aTHX_ "panic: INTERPCONCAT, lex_brackets=%ld",
		       (long) PL_lex_brackets);
#endif
	if (PL_bufptr == PL_bufend)
	    return REPORT(sublex_done());

	/* m'foo' still needs to be parsed for possible (?{...}) */
	if (SvIVX(PL_linestr) == '\'' && !PL_lex_inpat) {
	    SV *sv = newSVsv(PL_linestr);
	    sv = tokeq(sv);
            pl_yylval.opval = newSVOP(OP_CONST, 0, sv);
	    s = PL_bufend;
	}
	else {
            int save_error_count = PL_error_count;

	    s = scan_const(PL_bufptr);

            /* Set flag if this was a pattern and there were errors.  op.c will
             * refuse to compile a pattern with this flag set.  Otherwise, we
             * could get segfaults, etc. */
            if (PL_lex_inpat && PL_error_count > save_error_count) {
                ((PMOP*)PL_lex_inpat)->op_pmflags |= PMf_HAS_ERROR;
            }
	    if (*s == '\\')
		PL_lex_state = LEX_INTERPCASEMOD;
	    else
		PL_lex_state = LEX_INTERPSTART;
	}

	if (s != PL_bufptr) {
	    NEXTVAL_NEXTTOKE = pl_yylval;
	    PL_expect = XTERM;
	    force_next(THING);
	    if (PL_lex_starts++) {
		/* commas only at base level: /$a\Ub$c/ => ($a,uc(b.$c)) */
		if (!PL_lex_casemods && PL_lex_inpat)
		    TOKEN(',');
		else
		    AopNOASSIGN(OP_CONCAT);
	    }
	    else {
		PL_bufptr = s;
		return yylex();
	    }
	}

	return yylex();
    case LEX_FORMLINE:
        if (PL_parser->sub_error_count != PL_error_count) {
            /* There was an error parsing a formline, which tends to
               mess up the parser.
               Unlike interpolated sub-parsing, we can't treat any of
               these as recoverable, so no need to check sub_no_recover.
            */
            yyquit();
        }
	assert(PL_lex_formbrack);
	s = scan_formline(PL_bufptr);
	if (!PL_lex_formbrack)
	{
	    formbrack = 1;
	    goto rightbracket;
	}
	PL_bufptr = s;
	return yylex();
    }

    /* We really do *not* want PL_linestr ever becoming a COW. */
    assert (!SvIsCOW(PL_linestr));
    s = PL_bufptr;
    PL_oldoldbufptr = PL_oldbufptr;
    PL_oldbufptr = s;
    PL_parser->saw_infix_sigil = 0;

    if (PL_in_my == KEY_sigvar) {
        /* we expect the sigil and optional var name part of a
         * signature element here. Since a '$' is not necessarily
         * followed by a var name, handle it specially here; the general
         * yylex code would otherwise try to interpret whatever follows
         * as a var; e.g. ($, ...) would be seen as the var '$,'
         */

        U8 sigil;

        s = skipspace(s);
        sigil = *s++;
        PL_bufptr = s; /* for error reporting */
        switch (sigil) {
        case '$':
        case '@':
        case '%':
            /* spot stuff that looks like an prototype */
            if (strchr("$:@%&*;\\[]", *s)) {
                yyerror("Illegal character following sigil in a subroutine signature");
                break;
            }
            /* '$#' is banned, while '$ # comment' isn't */
            if (*s == '#') {
                yyerror("'#' not allowed immediately following a sigil in a subroutine signature");
                break;
            }
            s = skipspace(s);
            if (isIDFIRST_lazy_if_safe(s, PL_bufend, UTF)) {
                char *dest = PL_tokenbuf + 1;
                /* read var name, including sigil, into PL_tokenbuf */
                PL_tokenbuf[0] = sigil;
                parse_ident(&s, &dest, dest + sizeof(PL_tokenbuf) - 1,
                    0, cBOOL(UTF), FALSE, FALSE);
                *dest = '\0';
                assert(PL_tokenbuf[1]); /* we have a variable name */
            }
            else {
                *PL_tokenbuf = 0;
                PL_in_my = 0;
            }

            s = skipspace(s);
            /* parse the = for the default ourselves to avoid '+=' etc being accepted here
             * as the ASSIGNOP, and exclude other tokens that start with =
             */
            if (*s == '=' && (!s[1] || strchr("=~>", s[1]) == 0)) {
                /* save now to report with the same context as we did when
                 * all ASSIGNOPS were accepted */
                PL_oldbufptr = s;

                ++s;
                NEXTVAL_NEXTTOKE.ival = 0;
                force_next(ASSIGNOP);
                PL_expect = XTERM;
            }
            else if (*s == ',' || *s == ')') {
                PL_expect = XOPERATOR;
            }
            else {
                /* make sure the context shows the unexpected character and
                 * hopefully a bit more */
                if (*s) ++s;
                while (*s && *s != '$' && *s != '@' && *s != '%' && *s != ')')
                    s++;
                PL_bufptr = s; /* for error reporting */
                yyerror("Illegal operator following parameter in a subroutine signature");
                PL_in_my = 0;
            }
            if (*PL_tokenbuf) {
                NEXTVAL_NEXTTOKE.ival = sigil;
                force_next('p'); /* force a signature pending identifier */
            }
            break;

        case ')':
            PL_expect = XBLOCK;
            break;
        case ',': /* handle ($a,,$b) */
            break;

        default:
            PL_in_my = 0;
            yyerror("A signature parameter must start with '$', '@' or '%'");
            /* very crude error recovery: skip to likely next signature
             * element */
            while (*s && *s != '$' && *s != '@' && *s != '%' && *s != ')')
                s++;
            break;
        }
        TOKEN(sigil);
    }

  retry:
    switch (*s) {
    default:
	if (UTF) {
            if (isIDFIRST_utf8_safe(s, PL_bufend)) {
                goto keylookup;
            }
        }
        else if (isALNUMC(*s)) {
	    goto keylookup;
	}
    {
        SV *dsv = newSVpvs_flags("", SVs_TEMP);
        const char *c;
        if (UTF) {
            STRLEN skiplen = UTF8SKIP(s);
            STRLEN stravail = PL_bufend - s;
            c = sv_uni_display(dsv, newSVpvn_flags(s,
                                                   skiplen > stravail ? stravail : skiplen,
                                                   SVs_TEMP | SVf_UTF8),
                               10, UNI_DISPLAY_ISPRINT);
        }
        else {
            c = Perl_form(aTHX_ "\\x%02X", (unsigned char)*s);
        }

        if (s >= PL_linestart) {
            d = PL_linestart;
        }
        else {
            /* somehow (probably due to a parse failure), PL_linestart has advanced
             * pass PL_bufptr, get a reasonable beginning of line
             */
            d = s;
            while (d > SvPVX(PL_linestr) && d[-1] && d[-1] != '\n')
                --d;
        }
        len = UTF ? Perl_utf8_length(aTHX_ (U8 *) d, (U8 *) s) : (STRLEN) (s - d);
        if (len > UNRECOGNIZED_PRECEDE_COUNT) {
            d = UTF ? (char *) utf8_hop_back((U8 *) s, -UNRECOGNIZED_PRECEDE_COUNT, (U8 *)d) : s - UNRECOGNIZED_PRECEDE_COUNT;
        }

        Perl_croak(aTHX_  "Unrecognized character %s; marked by <-- HERE after %" UTF8f "<-- HERE near column %d", c,
                          UTF8fARG(UTF, (s - d), d),
                         (int) len + 1);
    }
    case 4:
    case 26:
	goto fake_eof;			/* emulate EOF on ^D or ^Z */
    case 0:
	if ((!PL_rsfp || PL_lex_inwhat)
	 && (!PL_parser->filtered || s+1 < PL_bufend)) {
	    PL_last_uni = 0;
	    PL_last_lop = 0;
	    if (PL_lex_brackets
                && PL_lex_brackstack[PL_lex_brackets-1] != XFAKEEOF)
            {
		yyerror((const char *)
			(PL_lex_formbrack
			 ? "Format not terminated"
			 : "Missing right curly or square bracket"));
	    }
            DEBUG_T( { PerlIO_printf(Perl_debug_log,
                        "### Tokener got EOF\n");
            } );
	    TOKEN(0);
	}
	if (s++ < PL_bufend)
	    goto retry;			/* ignore stray nulls */
	PL_last_uni = 0;
	PL_last_lop = 0;
	if (!PL_in_eval && !PL_preambled) {
	    PL_preambled = TRUE;
	    if (PL_perldb) {
		/* Generate a string of Perl code to load the debugger.
		 * If PERL5DB is set, it will return the contents of that,
		 * otherwise a compile-time require of perl5db.pl.  */

		const char * const pdb = PerlEnv_getenv("PERL5DB");

		if (pdb) {
		    sv_setpv(PL_linestr, pdb);
		    sv_catpvs(PL_linestr,";");
		} else {
		    SETERRNO(0,SS_NORMAL);
		    sv_setpvs(PL_linestr, "BEGIN { require 'perl5db.pl' };");
		}
		PL_parser->preambling = CopLINE(PL_curcop);
	    } else
                SvPVCLEAR(PL_linestr);
	    if (PL_preambleav) {
		SV **svp = AvARRAY(PL_preambleav);
		SV **const end = svp + AvFILLp(PL_preambleav);
		while(svp <= end) {
		    sv_catsv(PL_linestr, *svp);
		    ++svp;
		    sv_catpvs(PL_linestr, ";");
		}
		sv_free(MUTABLE_SV(PL_preambleav));
		PL_preambleav = NULL;
	    }
	    if (PL_minus_E)
		sv_catpvs(PL_linestr,
			  "use feature ':5." STRINGIFY(PERL_VERSION) "';");
	    if (PL_minus_n || PL_minus_p) {
		sv_catpvs(PL_linestr, "LINE: while (<>) {"/*}*/);
		if (PL_minus_l)
		    sv_catpvs(PL_linestr,"chomp;");
		if (PL_minus_a) {
		    if (PL_minus_F) {
                        if (   (   *PL_splitstr == '/'
                                || *PL_splitstr == '\''
                                || *PL_splitstr == '"')
                            && strchr(PL_splitstr + 1, *PL_splitstr))
                        {
                            /* strchr is ok, because -F pattern can't contain
                             * embeddded NULs */
			    Perl_sv_catpvf(aTHX_ PL_linestr, "our @F=split(%s);", PL_splitstr);
                        }
			else {
			    /* "q\0${splitstr}\0" is legal perl. Yes, even NUL
			       bytes can be used as quoting characters.  :-) */
			    const char *splits = PL_splitstr;
			    sv_catpvs(PL_linestr, "our @F=split(q\0");
			    do {
				/* Need to \ \s  */
				if (*splits == '\\')
				    sv_catpvn(PL_linestr, splits, 1);
				sv_catpvn(PL_linestr, splits, 1);
			    } while (*splits++);
			    /* This loop will embed the trailing NUL of
			       PL_linestr as the last thing it does before
			       terminating.  */
			    sv_catpvs(PL_linestr, ");");
			}
		    }
		    else
		        sv_catpvs(PL_linestr,"our @F=split(' ');");
		}
	    }
	    sv_catpvs(PL_linestr, "\n");
	    PL_oldoldbufptr = PL_oldbufptr = s = PL_linestart = SvPVX(PL_linestr);
	    PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
	    PL_last_lop = PL_last_uni = NULL;
	    if (PERLDB_LINE_OR_SAVESRC && PL_curstash != PL_debstash)
		update_debugger_info(PL_linestr, NULL, 0);
	    goto retry;
	}
	do {
	    fake_eof = 0;
	    bof = cBOOL(PL_rsfp);
	    if (0) {
	      fake_eof:
		fake_eof = LEX_FAKE_EOF;
	    }
	    PL_bufptr = PL_bufend;
	    COPLINE_INC_WITH_HERELINES;
	    if (!lex_next_chunk(fake_eof)) {
		CopLINE_dec(PL_curcop);
		s = PL_bufptr;
		TOKEN(';');	/* not infinite loop because rsfp is NULL now */
	    }
	    CopLINE_dec(PL_curcop);
	    s = PL_bufptr;
	    /* If it looks like the start of a BOM or raw UTF-16,
	     * check if it in fact is. */
	    if (bof && PL_rsfp
                && (   *s == 0
                    || *(U8*)s == BOM_UTF8_FIRST_BYTE
                    || *(U8*)s >= 0xFE
                    || s[1] == 0))
            {
		Off_t offset = (IV)PerlIO_tell(PL_rsfp);
		bof = (offset == (Off_t)SvCUR(PL_linestr));
#if defined(PERLIO_USING_CRLF) && defined(PERL_TEXTMODE_SCRIPTS)
		/* offset may include swallowed CR */
		if (!bof)
		    bof = (offset == (Off_t)SvCUR(PL_linestr)+1);
#endif
		if (bof) {
		    PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
		    s = swallow_bom((U8*)s);
		}
	    }
	    if (PL_parser->in_pod) {
		/* Incest with pod. */
                if (    memBEGINPs(s, (STRLEN) (PL_bufend - s), "=cut")
                    && !isALPHA(s[4]))
                {
                    SvPVCLEAR(PL_linestr);
		    PL_oldoldbufptr = PL_oldbufptr = s = PL_linestart = SvPVX(PL_linestr);
		    PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
		    PL_last_lop = PL_last_uni = NULL;
		    PL_parser->in_pod = 0;
		}
	    }
	    if (PL_rsfp || PL_parser->filtered)
		incline(s, PL_bufend);
	} while (PL_parser->in_pod);
	PL_oldoldbufptr = PL_oldbufptr = PL_bufptr = PL_linestart = s;
	PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
	PL_last_lop = PL_last_uni = NULL;
	if (CopLINE(PL_curcop) == 1) {
	    while (s < PL_bufend && isSPACE(*s))
		s++;
	    if (*s == ':' && s[1] != ':') /* for csh execing sh scripts */
		s++;
	    d = NULL;
	    if (!PL_in_eval) {
		if (*s == '#' && *(s+1) == '!')
		    d = s + 2;
#ifdef ALTERNATE_SHEBANG
		else {
		    static char const as[] = ALTERNATE_SHEBANG;
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
                    SV* copfilesv = CopFILESV(PL_curcop);
                    if (copfilesv) {
                        SV * const x =
                            GvSV(gv_fetchpvs("\030", GV_ADD|GV_NOTQUAL,
                                             SVt_PV)); /* $^X */
                        assert(SvPOK(x) || SvGMAGICAL(x));
                        if (sv_eq(x, copfilesv)) {
                            sv_setpvn(x, ipath, ipathend - ipath);
                            SvSETMAGIC(x);
                        }
                        else {
                            STRLEN blen;
                            STRLEN llen;
                            const char *bstart = SvPV_const(copfilesv, blen);
                            const char * const lstart = SvPV_const(x, llen);
                            if (llen < blen) {
                                bstart += blen - llen;
                                if (strnEQ(bstart, lstart, llen) &&	bstart[-1] == '/') {
                                    sv_setpvn(x, ipath, ipathend - ipath);
                                    SvSETMAGIC(x);
                                }
                            }
			}
                    }
                    else {
                        /* Anything to do if no copfilesv? */
		    }
		    TAINT_NOT;	/* $^X is always tainted, but that's OK */
		}
#endif /* ARG_ZERO_IS_SCRIPT */

		/*
		 * Look for options.
		 */
		d = instr(s,"perl -");
		if (!d) {
		    d = instr(s,"perl");
#if defined(DOSISH)
		    /* avoid getting into infinite loops when shebang
		     * line contains "Perl" rather than "perl" */
		    if (!d) {
			for (d = ipathend-4; d >= ipath; --d) {
			    if (isALPHA_FOLD_EQ(*d, 'p')
				&& !ibcmp(d, "perl", 4))
			    {
				break;
			    }
			}
			if (d < ipath)
			    d = NULL;
		    }
#endif
		}
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
		    const char *c = ipath;
		    while (*c && !strchr("; \t\r\n\f\v#", *c))
			c++;
		    if (c < d)
			d = NULL;	/* "perl" not in first word; ignore */
		    else
			*s = '#';	/* Don't try to parse shebang line */
		}
#endif /* ALTERNATE_SHEBANG */
		if (!d
                    && *s == '#'
                    && ipathend > ipath
                    && !PL_minus_c
                    && !instr(s,"indir")
                    && instr(PL_origargv[0],"perl"))
		{
		    dVAR;
		    char **newargv;

		    *ipathend = '\0';
		    s = ipathend + 1;
		    while (s < PL_bufend && isSPACE(*s))
			s++;
		    if (s < PL_bufend) {
			Newx(newargv,PL_origargc+3,char*);
			newargv[1] = s;
			while (s < PL_bufend && !isSPACE(*s))
			    s++;
			*s = '\0';
			Copy(PL_origargv+1, newargv+2, PL_origargc+1, char*);
		    }
		    else
			newargv = PL_origargv;
		    newargv[0] = ipath;
		    PERL_FPU_PRE_EXEC
		    PerlProc_execv(ipath, EXEC_ARGV_CAST(newargv));
		    PERL_FPU_POST_EXEC
		    Perl_croak(aTHX_ "Can't exec %s", ipath);
		}
		if (d) {
		    while (*d && !isSPACE(*d))
			d++;
		    while (SPACE_OR_TAB(*d))
			d++;

		    if (*d++ == '-') {
			const bool switches_done = PL_doswitches;
			const U32 oldpdb = PL_perldb;
			const bool oldn = PL_minus_n;
			const bool oldp = PL_minus_p;
			const char *d1 = d;

			do {
			    bool baduni = FALSE;
			    if (*d1 == 'C') {
				const char *d2 = d1 + 1;
				if (parse_unicode_opts((const char **)&d2)
				    != PL_unicode)
				    baduni = TRUE;
			    }
			    if (baduni || isALPHA_FOLD_EQ(*d1, 'M')) {
				const char * const m = d1;
				while (*d1 && !isSPACE(*d1))
				    d1++;
				Perl_croak(aTHX_ "Too late for \"-%.*s\" option",
				      (int)(d1 - m), m);
			    }
			    d1 = moreswitches(d1);
			} while (d1);
			if (PL_doswitches && !switches_done) {
			    int argc = PL_origargc;
			    char **argv = PL_origargv;
			    do {
				argc--,argv++;
			    } while (argc && argv[0][0] == '-' && argv[0][1]);
			    init_argv_symbols(argc,argv);
			}
			if (   (PERLDB_LINE_OR_SAVESRC && !oldpdb)
                            || ((PL_minus_n || PL_minus_p) && !(oldn || oldp)))
			      /* if we have already added "LINE: while (<>) {",
			         we must not do it again */
			{
                            SvPVCLEAR(PL_linestr);
			    PL_oldoldbufptr = PL_oldbufptr = s = PL_linestart = SvPVX(PL_linestr);
			    PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
			    PL_last_lop = PL_last_uni = NULL;
			    PL_preambled = FALSE;
			    if (PERLDB_LINE_OR_SAVESRC)
				(void)gv_fetchfile(PL_origfilename);
			    goto retry;
			}
		    }
		}
	    }
	}
	if (PL_lex_formbrack && PL_lex_brackets <= PL_lex_formbrack) {
	    PL_lex_state = LEX_FORMLINE;
	    force_next(FORMRBRACK);
	    TOKEN(';');
	}
	goto retry;
    case '\r':
#ifdef PERL_STRICT_CR
	Perl_warn(aTHX_ "Illegal character \\%03o (carriage return)", '\r');
	Perl_croak(aTHX_
      "\t(Maybe you didn't strip carriage returns after a network transfer?)\n");
#endif
    case ' ': case '\t': case '\f': case '\v':
	s++;
	goto retry;
    case '#':
    case '\n':
	if (PL_lex_state != LEX_NORMAL
            || (PL_in_eval && !PL_rsfp && !PL_parser->filtered))
        {
            const bool in_comment = *s == '#';
	    if (*s == '#' && s == PL_linestart && PL_in_eval
	     && !PL_rsfp && !PL_parser->filtered) {
		/* handle eval qq[#line 1 "foo"\n ...] */
		CopLINE_dec(PL_curcop);
		incline(s, PL_bufend);
	    }
            d = s;
            while (d < PL_bufend && *d != '\n')
                d++;
            if (d < PL_bufend)
                d++;
            s = d;
            if (in_comment && d == PL_bufend
                && PL_lex_state == LEX_INTERPNORMAL
                && PL_lex_inwhat == OP_SUBST && PL_lex_repl == PL_linestr
                && SvEVALED(PL_lex_repl) && d[-1] == '}') s--;
            else
                incline(s, PL_bufend);
	    if (PL_lex_formbrack && PL_lex_brackets <= PL_lex_formbrack) {
		PL_lex_state = LEX_FORMLINE;
		force_next(FORMRBRACK);
		TOKEN(';');
	    }
	}
	else {
            while (s < PL_bufend && *s != '\n')
                s++;
            if (s < PL_bufend)
                {
                    s++;
                    if (s < PL_bufend)
                        incline(s, PL_bufend);
                }
	}
	goto retry;
    case '-':
	if (s[1] && isALPHA(s[1]) && !isWORDCHAR(s[2])) {
	    I32 ftst = 0;
	    char tmp;

	    s++;
	    PL_bufptr = s;
	    tmp = *s++;

	    while (s < PL_bufend && SPACE_OR_TAB(*s))
		s++;

	    if (memBEGINs(s, (STRLEN) (PL_bufend - s), "=>")) {
		s = force_word(PL_bufptr,BAREWORD,FALSE,FALSE);
		DEBUG_T( { printbuf("### Saw unary minus before =>, forcing word %s\n", s); } );
		OPERATOR('-');		/* unary minus */
	    }
	    switch (tmp) {
	    case 'r': ftst = OP_FTEREAD;	break;
	    case 'w': ftst = OP_FTEWRITE;	break;
	    case 'x': ftst = OP_FTEEXEC;	break;
	    case 'o': ftst = OP_FTEOWNED;	break;
	    case 'R': ftst = OP_FTRREAD;	break;
	    case 'W': ftst = OP_FTRWRITE;	break;
	    case 'X': ftst = OP_FTREXEC;	break;
	    case 'O': ftst = OP_FTROWNED;	break;
	    case 'e': ftst = OP_FTIS;		break;
	    case 'z': ftst = OP_FTZERO;		break;
	    case 's': ftst = OP_FTSIZE;		break;
	    case 'f': ftst = OP_FTFILE;		break;
	    case 'd': ftst = OP_FTDIR;		break;
	    case 'l': ftst = OP_FTLINK;		break;
	    case 'p': ftst = OP_FTPIPE;		break;
	    case 'S': ftst = OP_FTSOCK;		break;
	    case 'u': ftst = OP_FTSUID;		break;
	    case 'g': ftst = OP_FTSGID;		break;
	    case 'k': ftst = OP_FTSVTX;		break;
	    case 'b': ftst = OP_FTBLK;		break;
	    case 'c': ftst = OP_FTCHR;		break;
	    case 't': ftst = OP_FTTTY;		break;
	    case 'T': ftst = OP_FTTEXT;		break;
	    case 'B': ftst = OP_FTBINARY;	break;
	    case 'M': case 'A': case 'C':
		gv_fetchpvs("\024", GV_ADD|GV_NOTQUAL, SVt_PV);
		switch (tmp) {
		case 'M': ftst = OP_FTMTIME;	break;
		case 'A': ftst = OP_FTATIME;	break;
		case 'C': ftst = OP_FTCTIME;	break;
		default:			break;
		}
		break;
	    default:
		break;
	    }
	    if (ftst) {
                PL_last_uni = PL_oldbufptr;
		PL_last_lop_op = (OPCODE)ftst;
		DEBUG_T( { PerlIO_printf(Perl_debug_log,
                        "### Saw file test %c\n", (int)tmp);
		} );
		FTST(ftst);
	    }
	    else {
		/* Assume it was a minus followed by a one-letter named
		 * subroutine call (or a -bareword), then. */
		DEBUG_T( { PerlIO_printf(Perl_debug_log,
			"### '-%c' looked like a file test but was not\n",
			(int) tmp);
		} );
		s = --PL_bufptr;
	    }
	}
	{
	    const char tmp = *s++;
	    if (*s == tmp) {
		s++;
		if (PL_expect == XOPERATOR)
		    TERM(POSTDEC);
		else
		    OPERATOR(PREDEC);
	    }
	    else if (*s == '>') {
		s++;
		s = skipspace(s);
		if (((*s == '$' || *s == '&') && s[1] == '*')
		  ||(*s == '$' && s[1] == '#' && s[2] == '*')
		  ||((*s == '@' || *s == '%') && strchr("*[{", s[1]))
		  ||(*s == '*' && (s[1] == '*' || s[1] == '{'))
		 )
		{
		    PL_expect = XPOSTDEREF;
		    TOKEN(ARROW);
		}
	        if (isIDFIRST_lazy_if_safe(s, PL_bufend, UTF)) {
		    s = force_word(s,METHOD,FALSE,TRUE);
		    TOKEN(ARROW);
		}
		else if (*s == '$')
		    OPERATOR(ARROW);
		else
		    TERM(ARROW);
	    }
	    if (PL_expect == XOPERATOR) {
		if (*s == '='
                    && !PL_lex_allbrackets
                    && PL_lex_fakeeof >= LEX_FAKEEOF_ASSIGN)
                {
		    s--;
		    TOKEN(0);
		}
		Aop(OP_SUBTRACT);
	    }
	    else {
		if (isSPACE(*s) || !isSPACE(*PL_bufptr))
		    check_uni();
		OPERATOR('-');		/* unary minus */
	    }
	}

    case '+':
	{
	    const char tmp = *s++;
	    if (*s == tmp) {
		s++;
		if (PL_expect == XOPERATOR)
		    TERM(POSTINC);
		else
		    OPERATOR(PREINC);
	    }
	    if (PL_expect == XOPERATOR) {
		if (*s == '='
                    && !PL_lex_allbrackets
                    && PL_lex_fakeeof >= LEX_FAKEEOF_ASSIGN)
                {
		    s--;
		    TOKEN(0);
		}
		Aop(OP_ADD);
	    }
	    else {
		if (isSPACE(*s) || !isSPACE(*PL_bufptr))
		    check_uni();
		OPERATOR('+');
	    }
	}

    case '*':
	if (PL_expect == XPOSTDEREF) POSTDEREF('*');
	if (PL_expect != XOPERATOR) {
	    s = scan_ident(s, PL_tokenbuf, sizeof PL_tokenbuf, TRUE);
	    PL_expect = XOPERATOR;
	    force_ident(PL_tokenbuf, '*');
	    if (!*PL_tokenbuf)
		PREREF('*');
	    TERM('*');
	}
	s++;
	if (*s == '*') {
	    s++;
	    if (*s == '=' && !PL_lex_allbrackets
                && PL_lex_fakeeof >= LEX_FAKEEOF_ASSIGN)
            {
		s -= 2;
		TOKEN(0);
	    }
	    PWop(OP_POW);
	}
	if (*s == '='
            && !PL_lex_allbrackets
            && PL_lex_fakeeof >= LEX_FAKEEOF_ASSIGN)
        {
	    s--;
	    TOKEN(0);
	}
	PL_parser->saw_infix_sigil = 1;
	Mop(OP_MULTIPLY);

    case '%':
    {
	if (PL_expect == XOPERATOR) {
	    if (s[1] == '='
                && !PL_lex_allbrackets
                && PL_lex_fakeeof >= LEX_FAKEEOF_ASSIGN)
            {
		TOKEN(0);
            }
	    ++s;
	    PL_parser->saw_infix_sigil = 1;
	    Mop(OP_MODULO);
	}
	else if (PL_expect == XPOSTDEREF) POSTDEREF('%');
	PL_tokenbuf[0] = '%';
	s = scan_ident(s, PL_tokenbuf + 1, sizeof PL_tokenbuf - 1, FALSE);
	pl_yylval.ival = 0;
	if (!PL_tokenbuf[1]) {
	    PREREF('%');
	}
        if (   (PL_expect != XREF || PL_oldoldbufptr == PL_last_lop)
            && intuit_more(s, PL_bufend)) {
	    if (*s == '[')
		PL_tokenbuf[0] = '@';
	}
	PL_expect = XOPERATOR;
	force_ident_maybe_lex('%');
	TERM('%');
    }
    case '^':
	d = s;
	bof = FEATURE_BITWISE_IS_ENABLED;
	if (bof && s[1] == '.')
	    s++;
	if (!PL_lex_allbrackets && PL_lex_fakeeof >=
		(s[1] == '=' ? LEX_FAKEEOF_ASSIGN : LEX_FAKEEOF_BITWISE))
	{
	    s = d;
	    TOKEN(0);
	}
	s++;
	BOop(bof ? d == s-2 ? OP_SBIT_XOR : OP_NBIT_XOR : OP_BIT_XOR);
    case '[':
	if (PL_lex_brackets > 100)
	    Renew(PL_lex_brackstack, PL_lex_brackets + 10, char);
	PL_lex_brackstack[PL_lex_brackets++] = 0;
	PL_lex_allbrackets++;
	{
	    const char tmp = *s++;
	    OPERATOR(tmp);
	}
    case '~':
	if (s[1] == '~'
	    && (PL_expect == XOPERATOR || PL_expect == XTERMORDORDOR))
	{
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_COMPARE)
		TOKEN(0);
	    s += 2;
            Perl_ck_warner_d(aTHX_
                packWARN(WARN_EXPERIMENTAL__SMARTMATCH),
                "Smartmatch is experimental");
	    Eop(OP_SMARTMATCH);
	}
	s++;
	if ((bof = FEATURE_BITWISE_IS_ENABLED) && *s == '.') {
	    s++;
	    BCop(OP_SCOMPLEMENT);
	}
	BCop(bof ? OP_NCOMPLEMENT : OP_COMPLEMENT);
    case ',':
	if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_COMMA)
	    TOKEN(0);
	s++;
	OPERATOR(',');
    case ':':
	if (s[1] == ':') {
	    len = 0;
	    goto just_a_word_zero_gv;
	}
	s++;
        {
        OP *attrs;

	switch (PL_expect) {
	case XOPERATOR:
	    if (!PL_in_my || PL_lex_state != LEX_NORMAL)
		break;
	    PL_bufptr = s;	/* update in case we back off */
	    if (*s == '=') {
		Perl_croak(aTHX_
			   "Use of := for an empty attribute list is not allowed");
	    }
	    goto grabattrs;
	case XATTRBLOCK:
	    PL_expect = XBLOCK;
	    goto grabattrs;
	case XATTRTERM:
	    PL_expect = XTERMBLOCK;
	 grabattrs:
            /* NB: as well as parsing normal attributes, we also end up
             * here if there is something looking like attributes
             * following a signature (which is illegal, but used to be
             * legal in 5.20..5.26). If the latter, we still parse the
             * attributes so that error messages(s) are less confusing,
             * but ignore them (parser->sig_seen).
             */
	    s = skipspace(s);
	    attrs = NULL;
            while (isIDFIRST_lazy_if_safe(s, PL_bufend, UTF)) {
                bool sig = PL_parser->sig_seen;
		I32 tmp;
		SV *sv;
		d = scan_word(s, PL_tokenbuf, sizeof PL_tokenbuf, FALSE, &len);
		if (isLOWER(*s) && (tmp = keyword(PL_tokenbuf, len, 0))) {
		    if (tmp < 0) tmp = -tmp;
		    switch (tmp) {
		    case KEY_or:
		    case KEY_and:
		    case KEY_for:
		    case KEY_foreach:
		    case KEY_unless:
		    case KEY_if:
		    case KEY_while:
		    case KEY_until:
			goto got_attrs;
		    default:
			break;
		    }
		}
		sv = newSVpvn_flags(s, len, UTF ? SVf_UTF8 : 0);
		if (*d == '(') {
		    d = scan_str(d,TRUE,TRUE,FALSE,NULL);
		    if (!d) {
			if (attrs)
			    op_free(attrs);
			sv_free(sv);
                        Perl_croak(aTHX_ "Unterminated attribute parameter in attribute list");
		    }
		    COPLINE_SET_FROM_MULTI_END;
		}
		if (PL_lex_stuff) {
		    sv_catsv(sv, PL_lex_stuff);
		    attrs = op_append_elem(OP_LIST, attrs,
					newSVOP(OP_CONST, 0, sv));
		    SvREFCNT_dec_NN(PL_lex_stuff);
		    PL_lex_stuff = NULL;
		}
		else {
		    /* NOTE: any CV attrs applied here need to be part of
		       the CVf_BUILTIN_ATTRS define in cv.h! */
		    if (!PL_in_my && memEQs(SvPVX(sv), len, "lvalue")) {
			sv_free(sv);
			if (!sig)
                            CvLVALUE_on(PL_compcv);
		    }
		    else if (!PL_in_my && memEQs(SvPVX(sv), len, "method")) {
			sv_free(sv);
			if (!sig)
                            CvMETHOD_on(PL_compcv);
		    }
		    else if (!PL_in_my && memEQs(SvPVX(sv), len, "const"))
		    {
			sv_free(sv);
                        if (!sig) {
                            Perl_ck_warner_d(aTHX_
                                packWARN(WARN_EXPERIMENTAL__CONST_ATTR),
                               ":const is experimental"
                            );
                            CvANONCONST_on(PL_compcv);
                            if (!CvANON(PL_compcv))
                                yyerror(":const is not permitted on named "
                                        "subroutines");
                        }
		    }
		    /* After we've set the flags, it could be argued that
		       we don't need to do the attributes.pm-based setting
		       process, and shouldn't bother appending recognized
		       flags.  To experiment with that, uncomment the
		       following "else".  (Note that's already been
		       uncommented.  That keeps the above-applied built-in
		       attributes from being intercepted (and possibly
		       rejected) by a package's attribute routines, but is
		       justified by the performance win for the common case
		       of applying only built-in attributes.) */
		    else
		        attrs = op_append_elem(OP_LIST, attrs,
					    newSVOP(OP_CONST, 0,
					      	    sv));
		}
		s = skipspace(d);
		if (*s == ':' && s[1] != ':')
		    s = skipspace(s+1);
		else if (s == d)
		    break;	/* require real whitespace or :'s */
		/* XXX losing whitespace on sequential attributes here */
	    }
	    {
		if (*s != ';'
                    && *s != '}'
                    && !(PL_expect == XOPERATOR
			 ? (*s == '=' ||  *s == ')')
			 : (*s == '{' ||  *s == '(')))
                {
		    const char q = ((*s == '\'') ? '"' : '\'');
		    /* If here for an expression, and parsed no attrs, back
		       off. */
		    if (PL_expect == XOPERATOR && !attrs) {
			s = PL_bufptr;
			break;
		    }
		    /* MUST advance bufptr here to avoid bogus "at end of line"
		       context messages from yyerror().
		    */
		    PL_bufptr = s;
		    yyerror( (const char *)
			     (*s
			      ? Perl_form(aTHX_ "Invalid separator character "
					  "%c%c%c in attribute list", q, *s, q)
			      : "Unterminated attribute list" ) );
		    if (attrs)
			op_free(attrs);
		    OPERATOR(':');
		}
	    }
	got_attrs:
            if (PL_parser->sig_seen) {
                /* see comment about about sig_seen and parser error
                 * handling */
                if (attrs)
                    op_free(attrs);
                Perl_croak(aTHX_ "Subroutine attributes must come "
                                 "before the signature");
                }
	    if (attrs) {
		NEXTVAL_NEXTTOKE.opval = attrs;
		force_next(THING);
	    }
	    TOKEN(COLONATTR);
	}
	}
	if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_CLOSING) {
	    s--;
	    TOKEN(0);
	}
	PL_lex_allbrackets--;
	OPERATOR(':');
    case '(':
	s++;
	if (PL_last_lop == PL_oldoldbufptr || PL_last_uni == PL_oldoldbufptr)
	    PL_oldbufptr = PL_oldoldbufptr;		/* allow print(STDOUT 123) */
	else
	    PL_expect = XTERM;
	s = skipspace(s);
	PL_lex_allbrackets++;
	TOKEN('(');
    case ';':
	if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_NONEXPR)
	    TOKEN(0);
	CLINE;
	s++;
	PL_expect = XSTATE;
	TOKEN(';');
    case ')':
	if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_CLOSING)
	    TOKEN(0);
	s++;
	PL_lex_allbrackets--;
	s = skipspace(s);
	if (*s == '{')
	    PREBLOCK(')');
	TERM(')');
    case ']':
	if (PL_lex_brackets && PL_lex_brackstack[PL_lex_brackets-1] == XFAKEEOF)
	    TOKEN(0);
	s++;
	if (PL_lex_brackets <= 0)
	    /* diag_listed_as: Unmatched right %s bracket */
	    yyerror("Unmatched right square bracket");
	else
	    --PL_lex_brackets;
	PL_lex_allbrackets--;
	if (PL_lex_state == LEX_INTERPNORMAL) {
	    if (PL_lex_brackets == 0) {
		if (*s == '-' && s[1] == '>')
		    PL_lex_state = LEX_INTERPENDMAYBE;
		else if (*s != '[' && *s != '{')
		    PL_lex_state = LEX_INTERPEND;
	    }
	}
	TERM(']');
    case '{':
	s++;
      leftbracket:
	if (PL_lex_brackets > 100) {
	    Renew(PL_lex_brackstack, PL_lex_brackets + 10, char);
	}
	switch (PL_expect) {
	case XTERM:
	case XTERMORDORDOR:
	    PL_lex_brackstack[PL_lex_brackets++] = XOPERATOR;
	    PL_lex_allbrackets++;
	    OPERATOR(HASHBRACK);
	case XOPERATOR:
	    while (s < PL_bufend && SPACE_OR_TAB(*s))
		s++;
	    d = s;
	    PL_tokenbuf[0] = '\0';
	    if (d < PL_bufend && *d == '-') {
		PL_tokenbuf[0] = '-';
		d++;
		while (d < PL_bufend && SPACE_OR_TAB(*d))
		    d++;
	    }
            if (d < PL_bufend && isIDFIRST_lazy_if_safe(d, PL_bufend, UTF)) {
		d = scan_word(d, PL_tokenbuf + 1, sizeof PL_tokenbuf - 1,
			      FALSE, &len);
		while (d < PL_bufend && SPACE_OR_TAB(*d))
		    d++;
		if (*d == '}') {
		    const char minus = (PL_tokenbuf[0] == '-');
		    s = force_word(s + minus, BAREWORD, FALSE, TRUE);
		    if (minus)
			force_next('-');
		}
	    }
	    /* FALLTHROUGH */
	case XATTRTERM:
	case XTERMBLOCK:
	    PL_lex_brackstack[PL_lex_brackets++] = XOPERATOR;
	    PL_lex_allbrackets++;
	    PL_expect = XSTATE;
	    break;
	case XATTRBLOCK:
	case XBLOCK:
	    PL_lex_brackstack[PL_lex_brackets++] = XSTATE;
	    PL_lex_allbrackets++;
	    PL_expect = XSTATE;
	    break;
	case XBLOCKTERM:
	    PL_lex_brackstack[PL_lex_brackets++] = XTERM;
	    PL_lex_allbrackets++;
	    PL_expect = XSTATE;
	    break;
	default: {
		const char *t;
		if (PL_oldoldbufptr == PL_last_lop)
		    PL_lex_brackstack[PL_lex_brackets++] = XTERM;
		else
		    PL_lex_brackstack[PL_lex_brackets++] = XOPERATOR;
		PL_lex_allbrackets++;
		s = skipspace(s);
		if (*s == '}') {
		    if (PL_expect == XREF && PL_lex_state == LEX_INTERPNORMAL) {
			PL_expect = XTERM;
			/* This hack is to get the ${} in the message. */
			PL_bufptr = s+1;
			yyerror("syntax error");
			break;
		    }
		    OPERATOR(HASHBRACK);
		}
		if (PL_expect == XREF && PL_oldoldbufptr != PL_last_lop) {
		    /* ${...} or @{...} etc., but not print {...}
		     * Skip the disambiguation and treat this as a block.
		     */
		    goto block_expectation;
		}
		/* This hack serves to disambiguate a pair of curlies
		 * as being a block or an anon hash.  Normally, expectation
		 * determines that, but in cases where we're not in a
		 * position to expect anything in particular (like inside
		 * eval"") we have to resolve the ambiguity.  This code
		 * covers the case where the first term in the curlies is a
		 * quoted string.  Most other cases need to be explicitly
		 * disambiguated by prepending a "+" before the opening
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
		    for (t++; t < PL_bufend && *t != *s;)
			if (*t++ == '\\')
			    t++;
		    t++;
		}
		else if (*s == 'q') {
		    if (++t < PL_bufend
			&& (!isWORDCHAR(*t)
			    || ((*t == 'q' || *t == 'x') && ++t < PL_bufend
				&& !isWORDCHAR(*t))))
		    {
			/* skip q//-like construct */
			const char *tmps;
			char open, close, term;
			I32 brackets = 1;

			while (t < PL_bufend && isSPACE(*t))
			    t++;
			/* check for q => */
			if (t+1 < PL_bufend && t[0] == '=' && t[1] == '>') {
			    OPERATOR(HASHBRACK);
			}
			term = *t;
			open = term;
			if (term && (tmps = strchr("([{< )]}> )]}>",term)))
			    term = tmps[5];
			close = term;
			if (open == close)
			    for (t++; t < PL_bufend; t++) {
				if (*t == '\\' && t+1 < PL_bufend && open != '\\')
				    t++;
				else if (*t == open)
				    break;
			    }
			else {
			    for (t++; t < PL_bufend; t++) {
				if (*t == '\\' && t+1 < PL_bufend)
				    t++;
				else if (*t == close && --brackets <= 0)
				    break;
				else if (*t == open)
				    brackets++;
			    }
			}
			t++;
		    }
		    else
			/* skip plain q word */
			while (   t < PL_bufend
                               && isWORDCHAR_lazy_if_safe(t, PL_bufend, UTF))
                        {
			    t += UTF ? UTF8SKIP(t) : 1;
                        }
		}
	        else if (isWORDCHAR_lazy_if_safe(t, PL_bufend, UTF)) {
		    t += UTF ? UTF8SKIP(t) : 1;
		    while (   t < PL_bufend
                           && isWORDCHAR_lazy_if_safe(t, PL_bufend, UTF))
                    {
			t += UTF ? UTF8SKIP(t) : 1;
                    }
		}
		while (t < PL_bufend && isSPACE(*t))
		    t++;
		/* if comma follows first term, call it an anon hash */
		/* XXX it could be a comma expression with loop modifiers */
		if (t < PL_bufend && ((*t == ',' && (*s == 'q' || !isLOWER(*s)))
				   || (*t == '=' && t[1] == '>')))
		    OPERATOR(HASHBRACK);
		if (PL_expect == XREF)
		{
		  block_expectation:
		    /* If there is an opening brace or 'sub:', treat it
		       as a term to make ${{...}}{k} and &{sub:attr...}
		       dwim.  Otherwise, treat it as a statement, so
		       map {no strict; ...} works.
		     */
		    s = skipspace(s);
		    if (*s == '{') {
			PL_expect = XTERM;
			break;
		    }
		    if (memBEGINs(s, (STRLEN) (PL_bufend - s), "sub")) {
                        PL_bufptr = s;
			d = s + 3;
			d = skipspace(d);
                        s = PL_bufptr;
			if (*d == ':') {
			    PL_expect = XTERM;
			    break;
			}
		    }
		    PL_expect = XSTATE;
		}
		else {
		    PL_lex_brackstack[PL_lex_brackets-1] = XSTATE;
		    PL_expect = XSTATE;
		}
	    }
	    break;
	}
	pl_yylval.ival = CopLINE(PL_curcop);
	PL_copline = NOLINE;   /* invalidate current command line number */
	TOKEN(formbrack ? '=' : '{');
    case '}':
	if (PL_lex_brackets && PL_lex_brackstack[PL_lex_brackets-1] == XFAKEEOF)
	    TOKEN(0);
      rightbracket:
	assert(s != PL_bufend);
	s++;
	if (PL_lex_brackets <= 0)
	    /* diag_listed_as: Unmatched right %s bracket */
	    yyerror("Unmatched right curly bracket");
	else
	    PL_expect = (expectation)PL_lex_brackstack[--PL_lex_brackets];
	PL_lex_allbrackets--;
	if (PL_lex_state == LEX_INTERPNORMAL) {
	    if (PL_lex_brackets == 0) {
		if (PL_expect & XFAKEBRACK) {
		    PL_expect &= XENUMMASK;
		    PL_lex_state = LEX_INTERPEND;
		    PL_bufptr = s;
		    return yylex();	/* ignore fake brackets */
		}
		if (PL_lex_inwhat == OP_SUBST && PL_lex_repl == PL_linestr
		 && SvEVALED(PL_lex_repl))
		    PL_lex_state = LEX_INTERPEND;
		else if (*s == '-' && s[1] == '>')
		    PL_lex_state = LEX_INTERPENDMAYBE;
		else if (*s != '[' && *s != '{')
		    PL_lex_state = LEX_INTERPEND;
	    }
	}
	if (PL_expect & XFAKEBRACK) {
	    PL_expect &= XENUMMASK;
	    PL_bufptr = s;
	    return yylex();		/* ignore fake brackets */
	}
	force_next(formbrack ? '.' : '}');
	if (formbrack) LEAVE_with_name("lex_format");
	if (formbrack == 2) { /* means . where arguments were expected */
	    force_next(';');
	    TOKEN(FORMRBRACK);
	}
	TOKEN(';');
    case '&':
	if (PL_expect == XPOSTDEREF) POSTDEREF('&');
	s++;
	if (*s++ == '&') {
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >=
		    (*s == '=' ? LEX_FAKEEOF_ASSIGN : LEX_FAKEEOF_LOGIC)) {
		s -= 2;
		TOKEN(0);
	    }
	    AOPERATOR(ANDAND);
	}
	s--;
	if (PL_expect == XOPERATOR) {
	    if (   PL_bufptr == PL_linestart
                && ckWARN(WARN_SEMICOLON)
	        && isIDFIRST_lazy_if_safe(s, PL_bufend, UTF))
	    {
		CopLINE_dec(PL_curcop);
		Perl_warner(aTHX_ packWARN(WARN_SEMICOLON), "%s", PL_warn_nosemi);
		CopLINE_inc(PL_curcop);
	    }
	    d = s;
	    if ((bof = FEATURE_BITWISE_IS_ENABLED) && *s == '.')
		s++;
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >=
		    (*s == '=' ? LEX_FAKEEOF_ASSIGN : LEX_FAKEEOF_BITWISE)) {
		s = d;
		s--;
		TOKEN(0);
	    }
	    if (d == s) {
		PL_parser->saw_infix_sigil = 1;
		BAop(bof ? OP_NBIT_AND : OP_BIT_AND);
	    }
	    else
		BAop(OP_SBIT_AND);
	}

	PL_tokenbuf[0] = '&';
	s = scan_ident(s - 1, PL_tokenbuf + 1, sizeof PL_tokenbuf - 1, TRUE);
	pl_yylval.ival = (OPpENTERSUB_AMPER<<8);
	if (PL_tokenbuf[1]) {
	    force_ident_maybe_lex('&');
	}
	else
	    PREREF('&');
	TERM('&');

    case '|':
	s++;
	if (*s++ == '|') {
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >=
		    (*s == '=' ? LEX_FAKEEOF_ASSIGN : LEX_FAKEEOF_LOGIC)) {
		s -= 2;
		TOKEN(0);
	    }
	    AOPERATOR(OROR);
	}
	s--;
	d = s;
	if ((bof = FEATURE_BITWISE_IS_ENABLED) && *s == '.')
	    s++;
	if (!PL_lex_allbrackets && PL_lex_fakeeof >=
		(*s == '=' ? LEX_FAKEEOF_ASSIGN : LEX_FAKEEOF_BITWISE)) {
	    s = d - 1;
	    TOKEN(0);
	}
	BOop(bof ? s == d ? OP_NBIT_OR : OP_SBIT_OR : OP_BIT_OR);
    case '=':
	s++;
	{
	    const char tmp = *s++;
	    if (tmp == '=') {
                if (   (s == PL_linestart+2 || s[-3] == '\n')
                    && memBEGINs(s, (STRLEN) (PL_bufend - s), "====="))
                {
	            s = vcs_conflict_marker(s + 5);
	            goto retry;
	        }
		if (!PL_lex_allbrackets
                    && PL_lex_fakeeof >= LEX_FAKEEOF_COMPARE)
                {
		    s -= 2;
		    TOKEN(0);
		}
		Eop(OP_EQ);
	    }
	    if (tmp == '>') {
		if (!PL_lex_allbrackets
                    && PL_lex_fakeeof >= LEX_FAKEEOF_COMMA)
                {
		    s -= 2;
		    TOKEN(0);
		}
		OPERATOR(',');
	    }
	    if (tmp == '~')
		PMop(OP_MATCH);
	    if (tmp && isSPACE(*s) && ckWARN(WARN_SYNTAX)
		&& strchr("+-*/%.^&|<",tmp))
		Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
			    "Reversed %c= operator",(int)tmp);
	    s--;
	    if (PL_expect == XSTATE
                && isALPHA(tmp)
                && (s == PL_linestart+1 || s[-2] == '\n') )
            {
                if (   (PL_in_eval && !PL_rsfp && !PL_parser->filtered)
                    || PL_lex_state != LEX_NORMAL)
                {
                    d = PL_bufend;
                    while (s < d) {
                        if (*s++ == '\n') {
                            incline(s, PL_bufend);
                            if (memBEGINs(s, (STRLEN) (PL_bufend - s), "=cut"))
                            {
                                s = (char *) memchr(s,'\n', d - s);
                                if (s)
                                    s++;
                                else
                                    s = d;
                                incline(s, PL_bufend);
                                goto retry;
                            }
                        }
                    }
                    goto retry;
                }
                s = PL_bufend;
                PL_parser->in_pod = 1;
                goto retry;
            }
	}
	if (PL_expect == XBLOCK) {
	    const char *t = s;
#ifdef PERL_STRICT_CR
	    while (SPACE_OR_TAB(*t))
#else
	    while (SPACE_OR_TAB(*t) || *t == '\r')
#endif
		t++;
	    if (*t == '\n' || *t == '#') {
		formbrack = 1;
		ENTER_with_name("lex_format");
		SAVEI8(PL_parser->form_lex_state);
		SAVEI32(PL_lex_formbrack);
		PL_parser->form_lex_state = PL_lex_state;
		PL_lex_formbrack = PL_lex_brackets + 1;
                PL_parser->sub_error_count = PL_error_count;
		goto leftbracket;
	    }
	}
	if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_ASSIGN) {
	    s--;
	    TOKEN(0);
	}
	pl_yylval.ival = 0;
	OPERATOR(ASSIGNOP);
    case '!':
	s++;
	{
	    const char tmp = *s++;
	    if (tmp == '=') {
		/* was this !=~ where !~ was meant?
		 * warn on m:!=~\s+([/?]|[msy]\W|tr\W): */

		if (*s == '~' && ckWARN(WARN_SYNTAX)) {
		    const char *t = s+1;

		    while (t < PL_bufend && isSPACE(*t))
			++t;

		    if (*t == '/' || *t == '?'
                        || ((*t == 'm' || *t == 's' || *t == 'y')
			    && !isWORDCHAR(t[1]))
                        || (*t == 't' && t[1] == 'r' && !isWORDCHAR(t[2])))
			Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
				    "!=~ should be !~");
		}
		if (!PL_lex_allbrackets
                    && PL_lex_fakeeof >= LEX_FAKEEOF_COMPARE)
                {
		    s -= 2;
		    TOKEN(0);
		}
		Eop(OP_NE);
	    }
	    if (tmp == '~')
		PMop(OP_NOT);
	}
	s--;
	OPERATOR('!');
    case '<':
	if (PL_expect != XOPERATOR) {
	    if (s[1] != '<' && !memchr(s,'>', PL_bufend - s))
		check_uni();
	    if (s[1] == '<' && s[2] != '>') {
                if (   (s == PL_linestart || s[-1] == '\n')
                    && memBEGINs(s+2, (STRLEN) (PL_bufend - (s+2)), "<<<<<"))
                {
	            s = vcs_conflict_marker(s + 7);
	            goto retry;
	        }
		s = scan_heredoc(s);
	    }
	    else
		s = scan_inputsymbol(s);
	    PL_expect = XOPERATOR;
	    TOKEN(sublex_start());
	}
	s++;
	{
	    char tmp = *s++;
	    if (tmp == '<') {
                if (   (s == PL_linestart+2 || s[-3] == '\n')
                    && memBEGINs(s, (STRLEN) (PL_bufend - s), "<<<<<"))
                {
                    s = vcs_conflict_marker(s + 5);
	            goto retry;
	        }
		if (*s == '=' && !PL_lex_allbrackets
                    && PL_lex_fakeeof >= LEX_FAKEEOF_ASSIGN)
                {
		    s -= 2;
		    TOKEN(0);
		}
		SHop(OP_LEFT_SHIFT);
	    }
	    if (tmp == '=') {
		tmp = *s++;
		if (tmp == '>') {
		    if (!PL_lex_allbrackets
                        && PL_lex_fakeeof >= LEX_FAKEEOF_COMPARE)
                    {
			s -= 3;
			TOKEN(0);
		    }
		    Eop(OP_NCMP);
		}
		s--;
		if (!PL_lex_allbrackets
                    && PL_lex_fakeeof >= LEX_FAKEEOF_COMPARE)
                {
		    s -= 2;
		    TOKEN(0);
		}
		Rop(OP_LE);
	    }
	}
	s--;
	if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_COMPARE) {
	    s--;
	    TOKEN(0);
	}
	Rop(OP_LT);
    case '>':
	s++;
	{
	    const char tmp = *s++;
	    if (tmp == '>') {
	        if (   (s == PL_linestart+2 || s[-3] == '\n')
                    && memBEGINs(s, (STRLEN) (PL_bufend - s), ">>>>>"))
                {
	            s = vcs_conflict_marker(s + 5);
	            goto retry;
	        }
		if (*s == '=' && !PL_lex_allbrackets
                    && PL_lex_fakeeof >= LEX_FAKEEOF_ASSIGN)
                {
		    s -= 2;
		    TOKEN(0);
		}
		SHop(OP_RIGHT_SHIFT);
	    }
	    else if (tmp == '=') {
		if (!PL_lex_allbrackets
                    && PL_lex_fakeeof >= LEX_FAKEEOF_COMPARE)
                {
		    s -= 2;
		    TOKEN(0);
		}
		Rop(OP_GE);
	    }
	}
	s--;
	if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_COMPARE) {
	    s--;
	    TOKEN(0);
	}
	Rop(OP_GT);

    case '$':
	CLINE;

        if (PL_expect == XPOSTDEREF) {
	    if (s[1] == '#') {
		s++;
		POSTDEREF(DOLSHARP);
	    }
	    POSTDEREF('$');
	}

	if (   s[1] == '#'
            && (   isIDFIRST_lazy_if_safe(s+2, PL_bufend, UTF)
                || strchr("{$:+-@", s[2])))
        {
	    PL_tokenbuf[0] = '@';
	    s = scan_ident(s + 1, PL_tokenbuf + 1,
			   sizeof PL_tokenbuf - 1, FALSE);
            if (PL_expect == XOPERATOR) {
                d = s;
                if (PL_bufptr > s) {
                    d = PL_bufptr-1;
                    PL_bufptr = PL_oldbufptr;
                }
		no_op("Array length", d);
            }
	    if (!PL_tokenbuf[1])
		PREREF(DOLSHARP);
	    PL_expect = XOPERATOR;
	    force_ident_maybe_lex('#');
	    TOKEN(DOLSHARP);
	}

	PL_tokenbuf[0] = '$';
	s = scan_ident(s, PL_tokenbuf + 1, sizeof PL_tokenbuf - 1, FALSE);
	if (PL_expect == XOPERATOR) {
	    d = s;
	    if (PL_bufptr > s) {
		d = PL_bufptr-1;
		PL_bufptr = PL_oldbufptr;
	    }
	    no_op("Scalar", d);
	}
	if (!PL_tokenbuf[1]) {
	    if (s == PL_bufend)
		yyerror("Final $ should be \\$ or $name");
	    PREREF('$');
	}

	d = s;
	{
	    const char tmp = *s;
	    if (PL_lex_state == LEX_NORMAL || PL_lex_brackets)
		s = skipspace(s);

	    if (   (PL_expect != XREF || PL_oldoldbufptr == PL_last_lop)
		&& intuit_more(s, PL_bufend)) {
		if (*s == '[') {
		    PL_tokenbuf[0] = '@';
		    if (ckWARN(WARN_SYNTAX)) {
			char *t = s+1;

                        while (   isSPACE(*t)
                               || isWORDCHAR_lazy_if_safe(t, PL_bufend, UTF)
                               || *t == '$')
                        {
			    t += UTF ? UTF8SKIP(t) : 1;
                        }
			if (*t++ == ',') {
			    PL_bufptr = skipspace(PL_bufptr); /* XXX can realloc */
			    while (t < PL_bufend && *t != ']')
				t++;
			    Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
					"Multidimensional syntax %" UTF8f " not supported",
                                        UTF8fARG(UTF,(int)((t - PL_bufptr) + 1), PL_bufptr));
			}
		    }
		}
		else if (*s == '{') {
		    char *t;
		    PL_tokenbuf[0] = '%';
                    if (    strEQ(PL_tokenbuf+1, "SIG")
                        && ckWARN(WARN_SYNTAX)
                        && (t = (char *) memchr(s, '}', PL_bufend - s))
                        && (t = (char *) memchr(t, '=', PL_bufend - t)))
                    {
                        char tmpbuf[sizeof PL_tokenbuf];
                        do {
                            t++;
                        } while (isSPACE(*t));
                        if (isIDFIRST_lazy_if_safe(t, PL_bufend, UTF)) {
                            STRLEN len;
                            t = scan_word(t, tmpbuf, sizeof tmpbuf, TRUE,
                                            &len);
                            while (isSPACE(*t))
                                t++;
                            if (  *t == ';'
                                && get_cvn_flags(tmpbuf, len, UTF
                                                                ? SVf_UTF8
                                                                : 0))
                            {
                                Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
                                    "You need to quote \"%" UTF8f "\"",
                                        UTF8fARG(UTF, len, tmpbuf));
                            }
                        }
                    }
		}
	    }

	    PL_expect = XOPERATOR;
	    if (PL_lex_state == LEX_NORMAL && isSPACE((char)tmp)) {
		const bool islop = (PL_last_lop == PL_oldoldbufptr);
		if (!islop || PL_last_lop_op == OP_GREPSTART)
		    PL_expect = XOPERATOR;
		else if (strchr("$@\"'`q", *s))
		    PL_expect = XTERM;		/* e.g. print $fh "foo" */
		else if (   strchr("&*<%", *s)
                         && isIDFIRST_lazy_if_safe(s+1, PL_bufend, UTF))
                {
		    PL_expect = XTERM;		/* e.g. print $fh &sub */
                }
	        else if (isIDFIRST_lazy_if_safe(s, PL_bufend, UTF)) {
		    char tmpbuf[sizeof PL_tokenbuf];
		    int t2;
		    scan_word(s, tmpbuf, sizeof tmpbuf, TRUE, &len);
		    if ((t2 = keyword(tmpbuf, len, 0))) {
			/* binary operators exclude handle interpretations */
			switch (t2) {
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
			    PL_expect = XTERM;	/* e.g. print $fh length() */
			    break;
			}
		    }
		    else {
			PL_expect = XTERM;	/* e.g. print $fh subr() */
		    }
		}
		else if (isDIGIT(*s))
		    PL_expect = XTERM;		/* e.g. print $fh 3 */
		else if (*s == '.' && isDIGIT(s[1]))
		    PL_expect = XTERM;		/* e.g. print $fh .3 */
		else if ((*s == '?' || *s == '-' || *s == '+')
			 && !isSPACE(s[1]) && s[1] != '=')
		    PL_expect = XTERM;		/* e.g. print $fh -1 */
		else if (*s == '/' && !isSPACE(s[1]) && s[1] != '='
			 && s[1] != '/')
		    PL_expect = XTERM;		/* e.g. print $fh /.../
						   XXX except DORDOR operator
						*/
		else if (*s == '<' && s[1] == '<' && !isSPACE(s[2])
			 && s[2] != '=')
		    PL_expect = XTERM;		/* print $fh <<"EOF" */
	    }
	}
	force_ident_maybe_lex('$');
	TOKEN('$');

    case '@':
        if (PL_expect == XPOSTDEREF)
            POSTDEREF('@');
	PL_tokenbuf[0] = '@';
	s = scan_ident(s, PL_tokenbuf + 1, sizeof PL_tokenbuf - 1, FALSE);
	if (PL_expect == XOPERATOR) {
            d = s;
            if (PL_bufptr > s) {
                d = PL_bufptr-1;
                PL_bufptr = PL_oldbufptr;
            }
	    no_op("Array", d);
        }
	pl_yylval.ival = 0;
	if (!PL_tokenbuf[1]) {
	    PREREF('@');
	}
	if (PL_lex_state == LEX_NORMAL)
	    s = skipspace(s);
	if (   (PL_expect != XREF || PL_oldoldbufptr == PL_last_lop)
            && intuit_more(s, PL_bufend))
        {
	    if (*s == '{')
		PL_tokenbuf[0] = '%';

	    /* Warn about @ where they meant $. */
	    if (*s == '[' || *s == '{') {
		if (ckWARN(WARN_SYNTAX)) {
		    S_check_scalar_slice(aTHX_ s);
		}
	    }
	}
	PL_expect = XOPERATOR;
	force_ident_maybe_lex('@');
	TERM('@');

     case '/':			/* may be division, defined-or, or pattern */
	if ((PL_expect == XOPERATOR || PL_expect == XTERMORDORDOR) && s[1] == '/') {
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >=
		    (s[2] == '=' ? LEX_FAKEEOF_ASSIGN : LEX_FAKEEOF_LOGIC))
		TOKEN(0);
	    s += 2;
	    AOPERATOR(DORDOR);
	}
	else if (PL_expect == XOPERATOR) {
	    s++;
	    if (*s == '=' && !PL_lex_allbrackets
                && PL_lex_fakeeof >= LEX_FAKEEOF_ASSIGN)
            {
		s--;
		TOKEN(0);
	    }
	    Mop(OP_DIVIDE);
        }
	else {
	    /* Disable warning on "study /blah/" */
	    if (    PL_oldoldbufptr == PL_last_uni
                && (   *PL_last_uni != 's' || s - PL_last_uni < 5
                    || memNE(PL_last_uni, "study", 5)
                    || isWORDCHAR_lazy_if_safe(PL_last_uni+5, PL_bufend, UTF)
	     ))
	        check_uni();
	    s = scan_pat(s,OP_MATCH);
	    TERM(sublex_start());
	}

     case '?':			/* conditional */
	s++;
	if (!PL_lex_allbrackets
            && PL_lex_fakeeof >= LEX_FAKEEOF_IFELSE)
        {
	    s--;
	    TOKEN(0);
	}
	PL_lex_allbrackets++;
	OPERATOR('?');

    case '.':
	if (PL_lex_formbrack && PL_lex_brackets == PL_lex_formbrack
#ifdef PERL_STRICT_CR
	    && s[1] == '\n'
#else
	    && (s[1] == '\n' || (s[1] == '\r' && s[2] == '\n'))
#endif
	    && (s == PL_linestart || s[-1] == '\n') )
	{
	    PL_expect = XSTATE;
	    formbrack = 2; /* dot seen where arguments expected */
	    goto rightbracket;
	}
	if (PL_expect == XSTATE && s[1] == '.' && s[2] == '.') {
	    s += 3;
	    OPERATOR(YADAYADA);
	}
	if (PL_expect == XOPERATOR || !isDIGIT(s[1])) {
	    char tmp = *s++;
	    if (*s == tmp) {
		if (!PL_lex_allbrackets
                    && PL_lex_fakeeof >= LEX_FAKEEOF_RANGE)
                {
		    s--;
		    TOKEN(0);
		}
		s++;
		if (*s == tmp) {
		    s++;
		    pl_yylval.ival = OPf_SPECIAL;
		}
		else
		    pl_yylval.ival = 0;
		OPERATOR(DOTDOT);
	    }
	    if (*s == '=' && !PL_lex_allbrackets
                && PL_lex_fakeeof >= LEX_FAKEEOF_ASSIGN)
            {
		s--;
		TOKEN(0);
	    }
	    Aop(OP_CONCAT);
	}
	/* FALLTHROUGH */
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
	s = scan_num(s, &pl_yylval);
	DEBUG_T( { printbuf("### Saw number in %s\n", s); } );
	if (PL_expect == XOPERATOR)
	    no_op("Number",s);
	TERM(THING);

    case '\'':
	s = scan_str(s,FALSE,FALSE,FALSE,NULL);
	if (!s)
	    missingterm(NULL, 0);
	COPLINE_SET_FROM_MULTI_END;
	DEBUG_T( { printbuf("### Saw string before %s\n", s); } );
	if (PL_expect == XOPERATOR) {
            no_op("String",s);
	}
	pl_yylval.ival = OP_CONST;
	TERM(sublex_start());

    case '"':
	s = scan_str(s,FALSE,FALSE,FALSE,NULL);
	DEBUG_T( {
	    if (s)
		printbuf("### Saw string before %s\n", s);
	    else
		PerlIO_printf(Perl_debug_log,
			     "### Saw unterminated string\n");
	} );
	if (PL_expect == XOPERATOR) {
		no_op("String",s);
	}
	if (!s)
	    missingterm(NULL, 0);
	pl_yylval.ival = OP_CONST;
	/* FIXME. I think that this can be const if char *d is replaced by
	   more localised variables.  */
	for (d = SvPV(PL_lex_stuff, len); len; len--, d++) {
	    if (*d == '$' || *d == '@' || *d == '\\' || !UTF8_IS_INVARIANT((U8)*d)) {
		pl_yylval.ival = OP_STRINGIFY;
		break;
	    }
	}
	if (pl_yylval.ival == OP_CONST)
	    COPLINE_SET_FROM_MULTI_END;
	TERM(sublex_start());

    case '`':
	s = scan_str(s,FALSE,FALSE,FALSE,NULL);
	DEBUG_T( {
            if (s)
                printbuf("### Saw backtick string before %s\n", s);
            else
		PerlIO_printf(Perl_debug_log,
			     "### Saw unterminated backtick string\n");
        } );
	if (PL_expect == XOPERATOR)
	    no_op("Backticks",s);
	if (!s)
	    missingterm(NULL, 0);
	pl_yylval.ival = OP_BACKTICK;
	TERM(sublex_start());

    case '\\':
	s++;
	if (PL_lex_inwhat == OP_SUBST && PL_lex_repl == PL_linestr
	 && isDIGIT(*s))
	    Perl_ck_warner(aTHX_ packWARN(WARN_SYNTAX),"Can't use \\%c to mean $%c in expression",
			   *s, *s);
	if (PL_expect == XOPERATOR)
	    no_op("Backslash",s);
	OPERATOR(REFGEN);

    case 'v':
	if (isDIGIT(s[1]) && PL_expect != XOPERATOR) {
	    char *start = s + 2;
	    while (isDIGIT(*start) || *start == '_')
		start++;
	    if (*start == '.' && isDIGIT(start[1])) {
		s = scan_num(s, &pl_yylval);
		TERM(THING);
	    }
	    else if ((*start == ':' && start[1] == ':')
		  || (PL_expect == XSTATE && *start == ':'))
		goto keylookup;
	    else if (PL_expect == XSTATE) {
		d = start;
		while (d < PL_bufend && isSPACE(*d)) d++;
		if (*d == ':') goto keylookup;
	    }
	    /* avoid v123abc() or $h{v1}, allow C<print v10;> */
	    if (!isALPHA(*start) && (PL_expect == XTERM
			|| PL_expect == XREF || PL_expect == XSTATE
			|| PL_expect == XTERMORDORDOR)) {
		GV *const gv = gv_fetchpvn_flags(s, start - s,
                                                    UTF ? SVf_UTF8 : 0, SVt_PVCV);
		if (!gv) {
		    s = scan_num(s, &pl_yylval);
		    TERM(THING);
		}
	    }
	}
	goto keylookup;
    case 'x':
	if (isDIGIT(s[1]) && PL_expect == XOPERATOR) {
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
	      case 'V':
    case 'w': case 'W':
	      case 'X':
    case 'y': case 'Y':
    case 'z': case 'Z':

      keylookup: {
	bool anydelim;
	bool lex;
	I32 tmp;
	SV *sv;
	CV *cv;
	PADOFFSET off;
	OP *rv2cv_op;

	lex = FALSE;
	orig_keyword = 0;
	off = 0;
	sv = NULL;
	cv = NULL;
	gv = NULL;
	gvp = NULL;
	rv2cv_op = NULL;

	PL_bufptr = s;
	s = scan_word(s, PL_tokenbuf, sizeof PL_tokenbuf, FALSE, &len);

	/* Some keywords can be followed by any delimiter, including ':' */
	anydelim = word_takes_any_delimiter(PL_tokenbuf, len);

	/* x::* is just a word, unless x is "CORE" */
	if (!anydelim && *s == ':' && s[1] == ':') {
	    if (memEQs(PL_tokenbuf, len, "CORE")) goto case_KEY_CORE;
	    goto just_a_word;
	}

	d = s;
	while (d < PL_bufend && isSPACE(*d))
		d++;	/* no comments skipped here, or s### is misparsed */

	/* Is this a word before a => operator? */
	if (*d == '=' && d[1] == '>') {
	  fat_arrow:
	    CLINE;
	    pl_yylval.opval
                = newSVOP(OP_CONST, 0,
			       S_newSV_maybe_utf8(aTHX_ PL_tokenbuf, len));
	    pl_yylval.opval->op_private = OPpCONST_BARE;
	    TERM(BAREWORD);
	}

	/* Check for plugged-in keyword */
	{
	    OP *o;
	    int result;
	    char *saved_bufptr = PL_bufptr;
	    PL_bufptr = s;
	    result = PL_keyword_plugin(aTHX_ PL_tokenbuf, len, &o);
	    s = PL_bufptr;
	    if (result == KEYWORD_PLUGIN_DECLINE) {
		/* not a plugged-in keyword */
		PL_bufptr = saved_bufptr;
	    } else if (result == KEYWORD_PLUGIN_STMT) {
		pl_yylval.opval = o;
		CLINE;
		if (!PL_nexttoke) PL_expect = XSTATE;
		return REPORT(PLUGSTMT);
	    } else if (result == KEYWORD_PLUGIN_EXPR) {
		pl_yylval.opval = o;
		CLINE;
		if (!PL_nexttoke) PL_expect = XOPERATOR;
		return REPORT(PLUGEXPR);
	    } else {
		Perl_croak(aTHX_ "Bad plugin affecting keyword '%s'",
					PL_tokenbuf);
	    }
	}

	/* Check for built-in keyword */
	tmp = keyword(PL_tokenbuf, len, 0);

	/* Is this a label? */
	if (!anydelim && PL_expect == XSTATE
	      && d < PL_bufend && *d == ':' && *(d + 1) != ':') {
	    s = d + 1;
	    pl_yylval.pval = savepvn(PL_tokenbuf, len+1);
	    pl_yylval.pval[len] = '\0';
	    pl_yylval.pval[len+1] = UTF ? 1 : 0;
	    CLINE;
	    TOKEN(LABEL);
	}

	/* Check for lexical sub */
	if (PL_expect != XOPERATOR) {
	    char tmpbuf[sizeof PL_tokenbuf + 1];
	    *tmpbuf = '&';
	    Copy(PL_tokenbuf, tmpbuf+1, len, char);
	    off = pad_findmy_pvn(tmpbuf, len+1, 0);
	    if (off != NOT_IN_PAD) {
		assert(off); /* we assume this is boolean-true below */
		if (PAD_COMPNAME_FLAGS_isOUR(off)) {
		    HV *  const stash = PAD_COMPNAME_OURSTASH(off);
		    HEK * const stashname = HvNAME_HEK(stash);
		    sv = newSVhek(stashname);
                    sv_catpvs(sv, "::");
                    sv_catpvn_flags(sv, PL_tokenbuf, len,
				    (UTF ? SV_CATUTF8 : SV_CATBYTES));
		    gv = gv_fetchsv(sv, GV_NOADD_NOINIT | SvUTF8(sv),
				    SVt_PVCV);
		    off = 0;
		    if (!gv) {
			sv_free(sv);
			sv = NULL;
			goto just_a_word;
		    }
		}
		else {
		    rv2cv_op = newOP(OP_PADANY, 0);
		    rv2cv_op->op_targ = off;
		    cv = find_lexical_cv(off);
		}
		lex = TRUE;
		goto just_a_word;
	    }
	    off = 0;
	}

	if (tmp < 0) {			/* second-class keyword? */
	    GV *ogv = NULL;	/* override (winner) */
	    GV *hgv = NULL;	/* hidden (loser) */
	    if (PL_expect != XOPERATOR && (*s != ':' || s[1] != ':')) {
		CV *cv;
		if ((gv = gv_fetchpvn_flags(PL_tokenbuf, len,
					    (UTF ? SVf_UTF8 : 0)|GV_NOTQUAL,
					    SVt_PVCV))
                    && (cv = GvCVu(gv)))
		{
		    if (GvIMPORTED_CV(gv))
			ogv = gv;
		    else if (! CvMETHOD(cv))
			hgv = gv;
		}
		if (!ogv
                    && (gvp = (GV**)hv_fetch(PL_globalstash, PL_tokenbuf,
                                                              len, FALSE))
                    && (gv = *gvp)
                    && (isGV_with_GP(gv)
			? GvCVu(gv) && GvIMPORTED_CV(gv)
			:   SvPCS_IMPORTED(gv)
			&& (gv_init(gv, PL_globalstash, PL_tokenbuf,
                                                                 len, 0), 1)))
		{
		    ogv = gv;
		}
	    }
	    if (ogv) {
		orig_keyword = tmp;
		tmp = 0;		/* overridden by import or by GLOBAL */
	    }
	    else if (gv && !gvp
		     && -tmp==KEY_lock	/* XXX generalizable kludge */
		     && GvCVu(gv))
	    {
		tmp = 0;		/* any sub overrides "weak" keyword */
	    }
	    else {			/* no override */
		tmp = -tmp;
		if (tmp == KEY_dump) {
		    Perl_ck_warner_d(aTHX_ packWARN2(WARN_MISC,WARN_DEPRECATED),
				     "dump() better written as CORE::dump(). "
                                     "dump() will no longer be available "
                                     "in Perl 5.30");
		}
		gv = NULL;
		gvp = 0;
		if (hgv && tmp != KEY_x)	/* never ambiguous */
		    Perl_ck_warner(aTHX_ packWARN(WARN_AMBIGUOUS),
				   "Ambiguous call resolved as CORE::%s(), "
				   "qualify as such or use &",
				   GvENAME(hgv));
	    }
	}

	if (tmp && tmp != KEY___DATA__ && tmp != KEY___END__
	 && (!anydelim || *s != '#')) {
	    /* no override, and not s### either; skipspace is safe here
	     * check for => on following line */
	    bool arrow;
	    STRLEN bufoff = PL_bufptr - SvPVX(PL_linestr);
	    STRLEN   soff = s         - SvPVX(PL_linestr);
	    s = peekspace(s);
	    arrow = *s == '=' && s[1] == '>';
	    PL_bufptr = SvPVX(PL_linestr) + bufoff;
	    s         = SvPVX(PL_linestr) +   soff;
	    if (arrow)
		goto fat_arrow;
	}

      reserved_word:
	switch (tmp) {

	    /* Trade off - by using this evil construction we can pull the
	       variable gv into the block labelled keylookup. If not, then
	       we have to give it function scope so that the goto from the
	       earlier ':' case doesn't bypass the initialisation.  */
	    just_a_word_zero_gv:
		sv = NULL;
		cv = NULL;
		gv = NULL;
		gvp = NULL;
		rv2cv_op = NULL;
		orig_keyword = 0;
		lex = 0;
		off = 0;
            /* FALLTHROUGH */
	default:			/* not a keyword */
	  just_a_word: {
		int pkgname = 0;
		const char lastchar = (PL_bufptr == PL_oldoldbufptr ? 0 : PL_bufptr[-1]);
		bool safebw;
		bool no_op_error = FALSE;

		if (PL_expect == XOPERATOR) {
		    if (PL_bufptr == PL_linestart) {
			CopLINE_dec(PL_curcop);
			Perl_warner(aTHX_ packWARN(WARN_SEMICOLON), "%s", PL_warn_nosemi);
			CopLINE_inc(PL_curcop);
		    }
		    else
			/* We want to call no_op with s pointing after the
			   bareword, so defer it.  But we want it to come
			   before the Bad name croak.  */
			no_op_error = TRUE;
		}

		/* Get the rest if it looks like a package qualifier */

		if (*s == '\'' || (*s == ':' && s[1] == ':')) {
		    STRLEN morelen;
		    s = scan_word(s, PL_tokenbuf + len, sizeof PL_tokenbuf - len,
				  TRUE, &morelen);
		    if (no_op_error) {
			no_op("Bareword",s);
			no_op_error = FALSE;
		    }
		    if (!morelen)
			Perl_croak(aTHX_ "Bad name after %" UTF8f "%s",
				UTF8fARG(UTF, len, PL_tokenbuf),
				*s == '\'' ? "'" : "::");
		    len += morelen;
		    pkgname = 1;
		}

		if (no_op_error)
			no_op("Bareword",s);

		/* See if the name is "Foo::",
		   in which case Foo is a bareword
		   (and a package name). */

		if (len > 2
                    && PL_tokenbuf[len - 2] == ':'
                    && PL_tokenbuf[len - 1] == ':')
		{
		    if (ckWARN(WARN_BAREWORD)
			&& ! gv_fetchpvn_flags(PL_tokenbuf, len, UTF ? SVf_UTF8 : 0, SVt_PVHV))
			Perl_warner(aTHX_ packWARN(WARN_BAREWORD),
                                    "Bareword \"%" UTF8f
                                    "\" refers to nonexistent package",
                                    UTF8fARG(UTF, len, PL_tokenbuf));
		    len -= 2;
		    PL_tokenbuf[len] = '\0';
		    gv = NULL;
		    gvp = 0;
		    safebw = TRUE;
		}
		else {
		    safebw = FALSE;
		}

		/* if we saw a global override before, get the right name */

		if (!sv)
		  sv = S_newSV_maybe_utf8(aTHX_ PL_tokenbuf,
						len);
		if (gvp) {
		    SV * const tmp_sv = sv;
		    sv = newSVpvs("CORE::GLOBAL::");
		    sv_catsv(sv, tmp_sv);
		    SvREFCNT_dec(tmp_sv);
		}


		/* Presume this is going to be a bareword of some sort. */
		CLINE;
                pl_yylval.opval = newSVOP(OP_CONST, 0, sv);
		pl_yylval.opval->op_private = OPpCONST_BARE;

		/* And if "Foo::", then that's what it certainly is. */
		if (safebw)
		    goto safe_bareword;

		if (!off)
		{
		    OP *const_op = newSVOP(OP_CONST, 0, SvREFCNT_inc_NN(sv));
		    const_op->op_private = OPpCONST_BARE;
		    rv2cv_op =
			newCVREF(OPpMAY_RETURN_CONSTANT<<8, const_op);
		    cv = lex
			? isGV(gv)
			    ? GvCV(gv)
			    : SvROK(gv) && SvTYPE(SvRV(gv)) == SVt_PVCV
				? (CV *)SvRV(gv)
				: ((CV *)gv)
			: rv2cv_op_cv(rv2cv_op, RV2CVOPCV_RETURN_STUB);
		}

		/* Use this var to track whether intuit_method has been
		   called.  intuit_method returns 0 or > 255.  */
		tmp = 1;

		/* See if it's the indirect object for a list operator. */

		if (PL_oldoldbufptr
                    && PL_oldoldbufptr < PL_bufptr
                    && (PL_oldoldbufptr == PL_last_lop
		        || PL_oldoldbufptr == PL_last_uni)
                    && /* NO SKIPSPACE BEFORE HERE! */
		       (PL_expect == XREF
                        || ((PL_opargs[PL_last_lop_op] >> OASHIFT)& 7)
                                                               == OA_FILEREF))
		{
		    bool immediate_paren = *s == '(';
                    SSize_t s_off;

		    /* (Now we can afford to cross potential line boundary.) */
		    s = skipspace(s);

                    /* intuit_method() can indirectly call lex_next_chunk(),
                     * invalidating s
                     */
                    s_off = s - SvPVX(PL_linestr);
		    /* Two barewords in a row may indicate method call. */
	            if (   (   isIDFIRST_lazy_if_safe(s, PL_bufend, UTF)
                            || *s == '$')
                        && (tmp = intuit_method(s, lex ? NULL : sv, cv)))
                    {
                        /* the code at method: doesn't use s */
			goto method;
		    }
                    s = SvPVX(PL_linestr) + s_off;

		    /* If not a declared subroutine, it's an indirect object. */
		    /* (But it's an indir obj regardless for sort.) */
		    /* Also, if "_" follows a filetest operator, it's a bareword */

		    if (
			( !immediate_paren && (PL_last_lop_op == OP_SORT
                         || (!cv
                             && (PL_last_lop_op != OP_MAPSTART
                                 && PL_last_lop_op != OP_GREPSTART))))
		       || (PL_tokenbuf[0] == '_' && PL_tokenbuf[1] == '\0'
			    && ((PL_opargs[PL_last_lop_op] & OA_CLASS_MASK)
                                                            == OA_FILESTATOP))
		       )
		    {
			PL_expect = (PL_last_lop == PL_oldoldbufptr) ? XTERM : XOPERATOR;
			goto bareword;
		    }
		}

		PL_expect = XOPERATOR;
		s = skipspace(s);

		/* Is this a word before a => operator? */
		if (*s == '=' && s[1] == '>' && !pkgname) {
		    op_free(rv2cv_op);
		    CLINE;
		    if (gvp || (lex && !off)) {
			assert (cSVOPx(pl_yylval.opval)->op_sv == sv);
			/* This is our own scalar, created a few lines
			   above, so this is safe. */
			SvREADONLY_off(sv);
			sv_setpv(sv, PL_tokenbuf);
			if (UTF && !IN_BYTES
			 && is_utf8_string((U8*)PL_tokenbuf, len))
			      SvUTF8_on(sv);
			SvREADONLY_on(sv);
		    }
		    TERM(BAREWORD);
		}

		/* If followed by a paren, it's certainly a subroutine. */
		if (*s == '(') {
		    CLINE;
		    if (cv) {
			d = s + 1;
			while (SPACE_OR_TAB(*d))
			    d++;
			if (*d == ')' && (sv = cv_const_sv_or_av(cv))) {
			    s = d + 1;
			    goto its_constant;
			}
		    }
		    NEXTVAL_NEXTTOKE.opval =
			off ? rv2cv_op : pl_yylval.opval;
		    if (off)
			 op_free(pl_yylval.opval), force_next(PRIVATEREF);
		    else op_free(rv2cv_op),	   force_next(BAREWORD);
		    pl_yylval.ival = 0;
		    TOKEN('&');
		}

		/* If followed by var or block, call it a method (unless sub) */

		if ((*s == '$' || *s == '{') && !cv) {
		    op_free(rv2cv_op);
		    PL_last_lop = PL_oldbufptr;
		    PL_last_lop_op = OP_METHOD;
		    if (!PL_lex_allbrackets
                        && PL_lex_fakeeof > LEX_FAKEEOF_LOWLOGIC)
                    {
			PL_lex_fakeeof = LEX_FAKEEOF_LOWLOGIC;
                    }
		    PL_expect = XBLOCKTERM;
		    PL_bufptr = s;
		    return REPORT(METHOD);
		}

		/* If followed by a bareword, see if it looks like indir obj. */

		if (   tmp == 1
                    && !orig_keyword
                    && (isIDFIRST_lazy_if_safe(s, PL_bufend, UTF) || *s == '$')
                    && (tmp = intuit_method(s, lex ? NULL : sv, cv)))
                {
		  method:
		    if (lex && !off) {
			assert(cSVOPx(pl_yylval.opval)->op_sv == sv);
			SvREADONLY_off(sv);
			sv_setpvn(sv, PL_tokenbuf, len);
			if (UTF && !IN_BYTES
			 && is_utf8_string((U8*)PL_tokenbuf, len))
			    SvUTF8_on (sv);
			else SvUTF8_off(sv);
		    }
		    op_free(rv2cv_op);
		    if (tmp == METHOD && !PL_lex_allbrackets
                        && PL_lex_fakeeof > LEX_FAKEEOF_LOWLOGIC)
                    {
			PL_lex_fakeeof = LEX_FAKEEOF_LOWLOGIC;
                    }
		    return REPORT(tmp);
		}

		/* Not a method, so call it a subroutine (if defined) */

		if (cv) {
		    /* Check for a constant sub */
		    if ((sv = cv_const_sv_or_av(cv))) {
		  its_constant:
			op_free(rv2cv_op);
			SvREFCNT_dec(((SVOP*)pl_yylval.opval)->op_sv);
			((SVOP*)pl_yylval.opval)->op_sv = SvREFCNT_inc_simple(sv);
			if (SvTYPE(sv) == SVt_PVAV)
			    pl_yylval.opval = newUNOP(OP_RV2AV, OPf_PARENS,
						      pl_yylval.opval);
			else {
			    pl_yylval.opval->op_private = 0;
			    pl_yylval.opval->op_folded = 1;
			    pl_yylval.opval->op_flags |= OPf_SPECIAL;
			}
			TOKEN(BAREWORD);
		    }

		    op_free(pl_yylval.opval);
		    pl_yylval.opval =
                        off ? newCVREF(0, rv2cv_op) : rv2cv_op;
		    pl_yylval.opval->op_private |= OPpENTERSUB_NOPAREN;
		    PL_last_lop = PL_oldbufptr;
		    PL_last_lop_op = OP_ENTERSUB;
		    /* Is there a prototype? */
		    if (
			SvPOK(cv))
		    {
			STRLEN protolen = CvPROTOLEN(cv);
			const char *proto = CvPROTO(cv);
			bool optional;
			proto = S_strip_spaces(aTHX_ proto, &protolen);
			if (!protolen)
			    TERM(FUNC0SUB);
			if ((optional = *proto == ';'))
			  do
			    proto++;
			  while (*proto == ';');
			if (
			    (
			        (
			            *proto == '$' || *proto == '_'
			         || *proto == '*' || *proto == '+'
			        )
			     && proto[1] == '\0'
			    )
			 || (
			     *proto == '\\' && proto[1] && proto[2] == '\0'
			    )
			)
			    UNIPROTO(UNIOPSUB,optional);
			if (*proto == '\\' && proto[1] == '[') {
			    const char *p = proto + 2;
			    while(*p && *p != ']')
				++p;
			    if(*p == ']' && !p[1])
				UNIPROTO(UNIOPSUB,optional);
			}
			if (*proto == '&' && *s == '{') {
			    if (PL_curstash)
				sv_setpvs(PL_subname, "__ANON__");
			    else
				sv_setpvs(PL_subname, "__ANON__::__ANON__");
			    if (!PL_lex_allbrackets
                                && PL_lex_fakeeof > LEX_FAKEEOF_LOWLOGIC)
                            {
				PL_lex_fakeeof = LEX_FAKEEOF_LOWLOGIC;
                            }
			    PREBLOCK(LSTOPSUB);
			}
		    }
		    NEXTVAL_NEXTTOKE.opval = pl_yylval.opval;
		    PL_expect = XTERM;
		    force_next(off ? PRIVATEREF : BAREWORD);
		    if (!PL_lex_allbrackets
                        && PL_lex_fakeeof > LEX_FAKEEOF_LOWLOGIC)
                    {
			PL_lex_fakeeof = LEX_FAKEEOF_LOWLOGIC;
                    }
		    TOKEN(NOAMP);
		}

		/* Call it a bare word */

		if (PL_hints & HINT_STRICT_SUBS)
		    pl_yylval.opval->op_private |= OPpCONST_STRICT;
		else {
		bareword:
		    /* after "print" and similar functions (corresponding to
		     * "F? L" in opcode.pl), whatever wasn't already parsed as
		     * a filehandle should be subject to "strict subs".
		     * Likewise for the optional indirect-object argument to system
		     * or exec, which can't be a bareword */
		    if ((PL_last_lop_op == OP_PRINT
			    || PL_last_lop_op == OP_PRTF
			    || PL_last_lop_op == OP_SAY
			    || PL_last_lop_op == OP_SYSTEM
			    || PL_last_lop_op == OP_EXEC)
			    && (PL_hints & HINT_STRICT_SUBS))
			pl_yylval.opval->op_private |= OPpCONST_STRICT;
		    if (lastchar != '-') {
			if (ckWARN(WARN_RESERVED)) {
			    d = PL_tokenbuf;
			    while (isLOWER(*d))
				d++;
			    if (!*d && !gv_stashpv(PL_tokenbuf, UTF ? SVf_UTF8 : 0))
                            {
                                /* PL_warn_reserved is constant */
                                GCC_DIAG_IGNORE_STMT(-Wformat-nonliteral);
				Perl_warner(aTHX_ packWARN(WARN_RESERVED), PL_warn_reserved,
				       PL_tokenbuf);
                                GCC_DIAG_RESTORE_STMT;
                            }
			}
		    }
		}
		op_free(rv2cv_op);

	    safe_bareword:
		if ((lastchar == '*' || lastchar == '%' || lastchar == '&')
		 && saw_infix_sigil) {
		    Perl_ck_warner_d(aTHX_ packWARN(WARN_AMBIGUOUS),
				     "Operator or semicolon missing before %c%" UTF8f,
				     lastchar,
				     UTF8fARG(UTF, strlen(PL_tokenbuf),
					      PL_tokenbuf));
		    Perl_ck_warner_d(aTHX_ packWARN(WARN_AMBIGUOUS),
				     "Ambiguous use of %c resolved as operator %c",
				     lastchar, lastchar);
		}
		TOKEN(BAREWORD);
	    }

	case KEY___FILE__:
	    FUN0OP(
                newSVOP(OP_CONST, 0, newSVpv(CopFILE(PL_curcop),0))
	    );

	case KEY___LINE__:
	    FUN0OP(
                newSVOP(OP_CONST, 0,
		    Perl_newSVpvf(aTHX_ "%" IVdf, (IV)CopLINE(PL_curcop)))
	    );

	case KEY___PACKAGE__:
	    FUN0OP(
                newSVOP(OP_CONST, 0,
					(PL_curstash
					 ? newSVhek(HvNAME_HEK(PL_curstash))
					 : &PL_sv_undef))
	    );

	case KEY___DATA__:
	case KEY___END__: {
	    GV *gv;
	    if (PL_rsfp && (!PL_in_eval || PL_tokenbuf[2] == 'D')) {
		HV * const stash = PL_tokenbuf[2] == 'D' && PL_curstash
					? PL_curstash
					: PL_defstash;
		gv = (GV *)*hv_fetchs(stash, "DATA", 1);
		if (!isGV(gv))
		    gv_init(gv,stash,"DATA",4,0);
		GvMULTI_on(gv);
		if (!GvIO(gv))
		    GvIOp(gv) = newIO();
		IoIFP(GvIOp(gv)) = PL_rsfp;
		/* Mark this internal pseudo-handle as clean */
		IoFLAGS(GvIOp(gv)) |= IOf_UNTAINT;
		if ((PerlIO*)PL_rsfp == PerlIO_stdin())
		    IoTYPE(GvIOp(gv)) = IoTYPE_STD;
		else
		    IoTYPE(GvIOp(gv)) = IoTYPE_RDONLY;
#if defined(WIN32) && !defined(PERL_TEXTMODE_SCRIPTS)
		/* if the script was opened in binmode, we need to revert
		 * it to text mode for compatibility; but only iff it has CRs
		 * XXX this is a questionable hack at best. */
		if (PL_bufend-PL_bufptr > 2
		    && PL_bufend[-1] == '\n' && PL_bufend[-2] == '\r')
		{
		    Off_t loc = 0;
		    if (IoTYPE(GvIOp(gv)) == IoTYPE_RDONLY) {
			loc = PerlIO_tell(PL_rsfp);
			(void)PerlIO_seek(PL_rsfp, 0L, 0);
		    }
#ifdef NETWARE
			if (PerlLIO_setmode(PL_rsfp, O_TEXT) != -1) {
#else
		    if (PerlLIO_setmode(PerlIO_fileno(PL_rsfp), O_TEXT) != -1) {
#endif	/* NETWARE */
			if (loc > 0)
			    PerlIO_seek(PL_rsfp, loc, 0);
		    }
		}
#endif
#ifdef PERLIO_LAYERS
		if (!IN_BYTES) {
		    if (UTF)
			PerlIO_apply_layers(aTHX_ PL_rsfp, NULL, ":utf8");
		}
#endif
		PL_rsfp = NULL;
	    }
	    goto fake_eof;
	}

	case KEY___SUB__:
	    FUN0OP(CvCLONE(PL_compcv)
			? newOP(OP_RUNCV, 0)
			: newPVOP(OP_RUNCV,0,NULL));

	case KEY_AUTOLOAD:
	case KEY_DESTROY:
	case KEY_BEGIN:
	case KEY_UNITCHECK:
	case KEY_CHECK:
	case KEY_INIT:
	case KEY_END:
	    if (PL_expect == XSTATE) {
		s = PL_bufptr;
		goto really_sub;
	    }
	    goto just_a_word;

	case_KEY_CORE:
	    {
		STRLEN olen = len;
		d = s;
		s += 2;
		s = scan_word(s, PL_tokenbuf, sizeof PL_tokenbuf, FALSE, &len);
		if ((*s == ':' && s[1] == ':')
		 || (!(tmp = keyword(PL_tokenbuf, len, 1)) && *s == '\''))
		{
		    s = d;
		    len = olen;
		    Copy(PL_bufptr, PL_tokenbuf, olen, char);
		    goto just_a_word;
		}
		if (!tmp)
		    Perl_croak(aTHX_ "CORE::%" UTF8f " is not a keyword",
				      UTF8fARG(UTF, len, PL_tokenbuf));
		if (tmp < 0)
		    tmp = -tmp;
		else if (tmp == KEY_require || tmp == KEY_do
		      || tmp == KEY_glob)
		    /* that's a way to remember we saw "CORE::" */
		    orig_keyword = tmp;
		goto reserved_word;
	    }

	case KEY_abs:
	    UNI(OP_ABS);

	case KEY_alarm:
	    UNI(OP_ALARM);

	case KEY_accept:
	    LOP(OP_ACCEPT,XTERM);

	case KEY_and:
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_LOWLOGIC)
		return REPORT(0);
	    OPERATOR(ANDOP);

	case KEY_atan2:
	    LOP(OP_ATAN2,XTERM);

	case KEY_bind:
	    LOP(OP_BIND,XTERM);

	case KEY_binmode:
	    LOP(OP_BINMODE,XTERM);

	case KEY_bless:
	    LOP(OP_BLESS,XTERM);

	case KEY_break:
	    FUN0(OP_BREAK);

	case KEY_chop:
	    UNI(OP_CHOP);

	case KEY_continue:
		    /* We have to disambiguate the two senses of
		      "continue". If the next token is a '{' then
		      treat it as the start of a continue block;
		      otherwise treat it as a control operator.
		     */
		    s = skipspace(s);
		    if (*s == '{')
	    PREBLOCK(CONTINUE);
		    else
			FUN0(OP_CONTINUE);

	case KEY_chdir:
	    /* may use HOME */
	    (void)gv_fetchpvs("ENV", GV_ADD|GV_NOTQUAL, SVt_PVHV);
	    UNI(OP_CHDIR);

	case KEY_close:
	    UNI(OP_CLOSE);

	case KEY_closedir:
	    UNI(OP_CLOSEDIR);

	case KEY_cmp:
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_COMPARE)
		return REPORT(0);
	    Eop(OP_SCMP);

	case KEY_caller:
	    UNI(OP_CALLER);

	case KEY_crypt:
#ifdef FCRYPT
	    if (!PL_cryptseen) {
		PL_cryptseen = TRUE;
		init_des();
	    }
#endif
	    LOP(OP_CRYPT,XTERM);

	case KEY_chmod:
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

	case KEY_default:
	    PREBLOCK(DEFAULT);

	case KEY_do:
	    s = skipspace(s);
	    if (*s == '{')
		PRETERMBLOCK(DO);
	    if (*s != '\'') {
		*PL_tokenbuf = '&';
		d = scan_word(s, PL_tokenbuf + 1, sizeof PL_tokenbuf - 1,
			      1, &len);
		if (len && memNEs(PL_tokenbuf+1, len, "CORE")
		 && !keyword(PL_tokenbuf + 1, len, 0)) {
                    SSize_t off = s-SvPVX(PL_linestr);
		    d = skipspace(d);
                    s = SvPVX(PL_linestr)+off;
		    if (*d == '(') {
			force_ident_maybe_lex('&');
			s = d;
		    }
		}
	    }
	    if (orig_keyword == KEY_do) {
		orig_keyword = 0;
		pl_yylval.ival = 1;
	    }
	    else
		pl_yylval.ival = 0;
	    OPERATOR(DO);

	case KEY_die:
	    PL_hints |= HINT_BLOCK_SCOPE;
	    LOP(OP_DIE,XTERM);

	case KEY_defined:
	    UNI(OP_DEFINED);

	case KEY_delete:
	    UNI(OP_DELETE);

	case KEY_dbmopen:
	    Perl_populate_isa(aTHX_ STR_WITH_LEN("AnyDBM_File::ISA"),
			      STR_WITH_LEN("NDBM_File::"),
			      STR_WITH_LEN("DB_File::"),
			      STR_WITH_LEN("GDBM_File::"),
			      STR_WITH_LEN("SDBM_File::"),
			      STR_WITH_LEN("ODBM_File::"),
			      NULL);
	    LOP(OP_DBMOPEN,XTERM);

	case KEY_dbmclose:
	    UNI(OP_DBMCLOSE);

	case KEY_dump:
	    LOOPX(OP_DUMP);

	case KEY_else:
	    PREBLOCK(ELSE);

	case KEY_elsif:
	    pl_yylval.ival = CopLINE(PL_curcop);
	    OPERATOR(ELSIF);

	case KEY_eq:
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_COMPARE)
		return REPORT(0);
	    Eop(OP_SEQ);

	case KEY_exists:
	    UNI(OP_EXISTS);

	case KEY_exit:
	    UNI(OP_EXIT);

	case KEY_eval:
	    s = skipspace(s);
	    if (*s == '{') { /* block eval */
		PL_expect = XTERMBLOCK;
		UNIBRACK(OP_ENTERTRY);
	    }
	    else { /* string eval */
		PL_expect = XTERM;
		UNIBRACK(OP_ENTEREVAL);
	    }

	case KEY_evalbytes:
	    PL_expect = XTERM;
	    UNIBRACK(-OP_ENTEREVAL);

	case KEY_eof:
	    UNI(OP_EOF);

	case KEY_exp:
	    UNI(OP_EXP);

	case KEY_each:
	    UNI(OP_EACH);

	case KEY_exec:
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
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_NONEXPR)
		return REPORT(0);
	    pl_yylval.ival = CopLINE(PL_curcop);
	    s = skipspace(s);
            if (   PL_expect == XSTATE
                && isIDFIRST_lazy_if_safe(s, PL_bufend, UTF))
            {
		char *p = s;
                SSize_t s_off = s - SvPVX(PL_linestr);

                if (   memBEGINPs(p, (STRLEN) (PL_bufend - p), "my")
                    && isSPACE(*(p + 2)))
                {
                    p += 2;
                }
                else if (   memBEGINPs(p, (STRLEN) (PL_bufend - p), "our")
                         && isSPACE(*(p + 3)))
                {
                    p += 3;
                }

		p = skipspace(p);
                /* skip optional package name, as in "for my abc $x (..)" */
	        if (isIDFIRST_lazy_if_safe(p, PL_bufend, UTF)) {
		    p = scan_word(p, PL_tokenbuf, sizeof PL_tokenbuf, TRUE, &len);
		    p = skipspace(p);
		}
		if (*p != '$' && *p != '\\')
		    Perl_croak(aTHX_ "Missing $ on loop variable");

                /* The buffer may have been reallocated, update s */
                s = SvPVX(PL_linestr) + s_off;
	    }
	    OPERATOR(FOR);

	case KEY_formline:
	    LOP(OP_FORMLINE,XTERM);

	case KEY_fork:
	    FUN0(OP_FORK);

	case KEY_fc:
	    UNI(OP_FC);

	case KEY_fcntl:
	    LOP(OP_FCNTL,XTERM);

	case KEY_fileno:
	    UNI(OP_FILENO);

	case KEY_flock:
	    LOP(OP_FLOCK,XTERM);

	case KEY_gt:
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_COMPARE)
		return REPORT(0);
	    Rop(OP_SGT);

	case KEY_ge:
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_COMPARE)
		return REPORT(0);
	    Rop(OP_SGE);

	case KEY_grep:
	    LOP(OP_GREPSTART, XREF);

	case KEY_goto:
	    LOOPX(OP_GOTO);

	case KEY_gmtime:
	    UNI(OP_GMTIME);

	case KEY_getc:
	    UNIDOR(OP_GETC);

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

	case KEY_given:
	    pl_yylval.ival = CopLINE(PL_curcop);
            Perl_ck_warner_d(aTHX_
                packWARN(WARN_EXPERIMENTAL__SMARTMATCH),
                "given is experimental");
	    OPERATOR(GIVEN);

	case KEY_glob:
	    LOP(
	     orig_keyword==KEY_glob ? -OP_GLOB : OP_GLOB,
	     XTERM
	    );

	case KEY_hex:
	    UNI(OP_HEX);

	case KEY_if:
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_NONEXPR)
		return REPORT(0);
	    pl_yylval.ival = CopLINE(PL_curcop);
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
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_COMPARE)
		return REPORT(0);
	    Rop(OP_SLT);

	case KEY_le:
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_COMPARE)
		return REPORT(0);
	    Rop(OP_SLE);

	case KEY_localtime:
	    UNI(OP_LOCALTIME);

	case KEY_log:
	    UNI(OP_LOG);

	case KEY_link:
	    LOP(OP_LINK,XTERM);

	case KEY_listen:
	    LOP(OP_LISTEN,XTERM);

	case KEY_lock:
	    UNI(OP_LOCK);

	case KEY_lstat:
	    UNI(OP_LSTAT);

	case KEY_m:
	    s = scan_pat(s,OP_MATCH);
	    TERM(sublex_start());

	case KEY_map:
	    LOP(OP_MAPSTART, XREF);

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

	case KEY_our:
	case KEY_my:
	case KEY_state:
	    if (PL_in_my) {
	        PL_bufptr = s;
	        yyerror(Perl_form(aTHX_
	                          "Can't redeclare \"%s\" in \"%s\"",
	                           tmp      == KEY_my    ? "my" :
	                           tmp      == KEY_state ? "state" : "our",
	                           PL_in_my == KEY_my    ? "my" :
	                           PL_in_my == KEY_state ? "state" : "our"));
	    }
	    PL_in_my = (U16)tmp;
	    s = skipspace(s);
            if (isIDFIRST_lazy_if_safe(s, PL_bufend, UTF)) {
		s = scan_word(s, PL_tokenbuf, sizeof PL_tokenbuf, TRUE, &len);
		if (memEQs(PL_tokenbuf, len, "sub"))
		    goto really_sub;
		PL_in_my_stash = find_in_my_stash(PL_tokenbuf, len);
		if (!PL_in_my_stash) {
		    char tmpbuf[1024];
                    int len;
		    PL_bufptr = s;
		    len = my_snprintf(tmpbuf, sizeof(tmpbuf), "No such class %.1000s", PL_tokenbuf);
                    PERL_MY_SNPRINTF_POST_GUARD(len, sizeof(tmpbuf));
		    yyerror_pv(tmpbuf, UTF ? SVf_UTF8 : 0);
		}
	    }
	    else if (*s == '\\') {
		if (!FEATURE_MYREF_IS_ENABLED)
		    Perl_croak(aTHX_ "The experimental declared_refs "
				     "feature is not enabled");
		Perl_ck_warner_d(aTHX_
		     packWARN(WARN_EXPERIMENTAL__DECLARED_REFS),
		    "Declaring references is experimental");
	    }
	    OPERATOR(MY);

	case KEY_next:
	    LOOPX(OP_NEXT);

	case KEY_ne:
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_COMPARE)
		return REPORT(0);
	    Eop(OP_SNE);

	case KEY_no:
	    s = tokenize_use(0, s);
	    TOKEN(USE);

	case KEY_not:
	    if (*s == '(' || (s = skipspace(s), *s == '('))
		FUN1(OP_NOT);
	    else {
		if (!PL_lex_allbrackets
                    && PL_lex_fakeeof > LEX_FAKEEOF_LOWLOGIC)
                {
		    PL_lex_fakeeof = LEX_FAKEEOF_LOWLOGIC;
                }
		OPERATOR(NOTOP);
	    }

	case KEY_open:
	    s = skipspace(s);
            if (isIDFIRST_lazy_if_safe(s, PL_bufend, UTF)) {
                const char *t;
                d = scan_word(s, PL_tokenbuf, sizeof PL_tokenbuf, FALSE,
                              &len);
		for (t=d; isSPACE(*t);)
		    t++;
		if ( *t && strchr("|&*+-=!?:.", *t) && ckWARN_d(WARN_PRECEDENCE)
		    /* [perl #16184] */
		    && !(t[0] == '=' && t[1] == '>')
		    && !(t[0] == ':' && t[1] == ':')
		    && !keyword(s, d-s, 0)
		) {
		    Perl_warner(aTHX_ packWARN(WARN_PRECEDENCE),
		       "Precedence problem: open %" UTF8f " should be open(%" UTF8f ")",
			UTF8fARG(UTF, d-s, s), UTF8fARG(UTF, d-s, s));
		}
	    }
	    LOP(OP_OPEN,XTERM);

	case KEY_or:
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_LOWLOGIC)
		return REPORT(0);
	    pl_yylval.ival = OP_OR;
	    OPERATOR(OROP);

	case KEY_ord:
	    UNI(OP_ORD);

	case KEY_oct:
	    UNI(OP_OCT);

	case KEY_opendir:
	    LOP(OP_OPEN_DIR,XTERM);

	case KEY_print:
	    checkcomma(s,PL_tokenbuf,"filehandle");
	    LOP(OP_PRINT,XREF);

	case KEY_printf:
	    checkcomma(s,PL_tokenbuf,"filehandle");
	    LOP(OP_PRTF,XREF);

	case KEY_prototype:
	    UNI(OP_PROTOTYPE);

	case KEY_push:
	    LOP(OP_PUSH,XTERM);

	case KEY_pop:
	    UNIDOR(OP_POP);

	case KEY_pos:
	    UNIDOR(OP_POS);

	case KEY_pack:
	    LOP(OP_PACK,XTERM);

	case KEY_package:
	    s = force_word(s,BAREWORD,FALSE,TRUE);
	    s = skipspace(s);
	    s = force_strict_version(s);
	    PREBLOCK(PACKAGE);

	case KEY_pipe:
	    LOP(OP_PIPE_OP,XTERM);

	case KEY_q:
	    s = scan_str(s,FALSE,FALSE,FALSE,NULL);
	    if (!s)
		missingterm(NULL, 0);
	    COPLINE_SET_FROM_MULTI_END;
	    pl_yylval.ival = OP_CONST;
	    TERM(sublex_start());

	case KEY_quotemeta:
	    UNI(OP_QUOTEMETA);

	case KEY_qw: {
	    OP *words = NULL;
	    s = scan_str(s,FALSE,FALSE,FALSE,NULL);
	    if (!s)
		missingterm(NULL, 0);
	    COPLINE_SET_FROM_MULTI_END;
	    PL_expect = XOPERATOR;
	    if (SvCUR(PL_lex_stuff)) {
		int warned_comma = !ckWARN(WARN_QW);
		int warned_comment = warned_comma;
		d = SvPV_force(PL_lex_stuff, len);
		while (len) {
		    for (; isSPACE(*d) && len; --len, ++d)
			/**/;
		    if (len) {
			SV *sv;
			const char *b = d;
			if (!warned_comma || !warned_comment) {
			    for (; !isSPACE(*d) && len; --len, ++d) {
				if (!warned_comma && *d == ',') {
				    Perl_warner(aTHX_ packWARN(WARN_QW),
					"Possible attempt to separate words with commas");
				    ++warned_comma;
				}
				else if (!warned_comment && *d == '#') {
				    Perl_warner(aTHX_ packWARN(WARN_QW),
					"Possible attempt to put comments in qw() list");
				    ++warned_comment;
				}
			    }
			}
			else {
			    for (; !isSPACE(*d) && len; --len, ++d)
				/**/;
			}
			sv = newSVpvn_utf8(b, d-b, DO_UTF8(PL_lex_stuff));
			words = op_append_elem(OP_LIST, words,
					    newSVOP(OP_CONST, 0, tokeq(sv)));
		    }
		}
	    }
	    if (!words)
		words = newNULLLIST();
	    SvREFCNT_dec_NN(PL_lex_stuff);
	    PL_lex_stuff = NULL;
	    PL_expect = XOPERATOR;
	    pl_yylval.opval = sawparens(words);
	    TOKEN(QWLIST);
	}

	case KEY_qq:
	    s = scan_str(s,FALSE,FALSE,FALSE,NULL);
	    if (!s)
		missingterm(NULL, 0);
	    pl_yylval.ival = OP_STRINGIFY;
	    if (SvIVX(PL_lex_stuff) == '\'')
		SvIV_set(PL_lex_stuff, 0);	/* qq'$foo' should interpolate */
	    TERM(sublex_start());

	case KEY_qr:
	    s = scan_pat(s,OP_QR);
	    TERM(sublex_start());

	case KEY_qx:
	    s = scan_str(s,FALSE,FALSE,FALSE,NULL);
	    if (!s)
		missingterm(NULL, 0);
	    pl_yylval.ival = OP_BACKTICK;
	    TERM(sublex_start());

	case KEY_return:
	    OLDLOP(OP_RETURN);

	case KEY_require:
	    s = skipspace(s);
	    if (isDIGIT(*s)) {
		s = force_version(s, FALSE);
	    }
	    else if (*s != 'v' || !isDIGIT(s[1])
		    || (s = force_version(s, TRUE), *s == 'v'))
	    {
		*PL_tokenbuf = '\0';
		s = force_word(s,BAREWORD,TRUE,TRUE);
                if (isIDFIRST_lazy_if_safe(PL_tokenbuf,
                                           PL_tokenbuf + sizeof(PL_tokenbuf),
                                           UTF))
                {
		    gv_stashpvn(PL_tokenbuf, strlen(PL_tokenbuf),
                                GV_ADD | (UTF ? SVf_UTF8 : 0));
                }
		else if (*s == '<')
		    yyerror("<> at require-statement should be quotes");
	    }
	    if (orig_keyword == KEY_require) {
		orig_keyword = 0;
		pl_yylval.ival = 1;
	    }
	    else
		pl_yylval.ival = 0;
	    PL_expect = PL_nexttoke ? XOPERATOR : XTERM;
	    PL_bufptr = s;
	    PL_last_uni = PL_oldbufptr;
	    PL_last_lop_op = OP_REQUIRE;
	    s = skipspace(s);
	    return REPORT( (int)REQUIRE );

	case KEY_reset:
	    UNI(OP_RESET);

	case KEY_redo:
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
	    UNIDOR(OP_READLINE);

	case KEY_readpipe:
	    UNIDOR(OP_BACKTICK);

	case KEY_rewinddir:
	    UNI(OP_REWINDDIR);

	case KEY_recv:
	    LOP(OP_RECV,XTERM);

	case KEY_reverse:
	    LOP(OP_REVERSE,XTERM);

	case KEY_readlink:
	    UNIDOR(OP_READLINK);

	case KEY_ref:
	    UNI(OP_REF);

	case KEY_s:
	    s = scan_subst(s);
	    if (pl_yylval.opval)
		TERM(sublex_start());
	    else
		TOKEN(1);	/* force error */

	case KEY_say:
	    checkcomma(s,PL_tokenbuf,"filehandle");
	    LOP(OP_SAY,XREF);

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
	    UNIDOR(OP_SHIFT);

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
	    checkcomma(s,PL_tokenbuf,"subroutine name");
	    s = skipspace(s);
	    PL_expect = XTERM;
	    s = force_word(s,BAREWORD,TRUE,TRUE);
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
	    UNI(OP_STUDY);

	case KEY_substr:
	    LOP(OP_SUBSTR,XTERM);

	case KEY_format:
	case KEY_sub:
	  really_sub:
	    {
		char * const tmpbuf = PL_tokenbuf + 1;
		bool have_name, have_proto;
		const int key = tmp;
                SV *format_name = NULL;
                bool is_sigsub = FEATURE_SIGNATURES_IS_ENABLED;

                SSize_t off = s-SvPVX(PL_linestr);
		s = skipspace(s);
                d = SvPVX(PL_linestr)+off;

                SAVEBOOL(PL_parser->sig_seen);
                PL_parser->sig_seen = FALSE;

                if (   isIDFIRST_lazy_if_safe(s, PL_bufend, UTF)
                    || *s == '\''
                    || (*s == ':' && s[1] == ':'))
		{

		    PL_expect = XATTRBLOCK;
		    d = scan_word(s, tmpbuf, sizeof PL_tokenbuf - 1, TRUE,
				  &len);
                    if (key == KEY_format)
			format_name = S_newSV_maybe_utf8(aTHX_ s, d - s);
		    *PL_tokenbuf = '&';
		    if (memchr(tmpbuf, ':', len) || key != KEY_sub
		     || pad_findmy_pvn(
			    PL_tokenbuf, len + 1, 0
			) != NOT_IN_PAD)
			sv_setpvn(PL_subname, tmpbuf, len);
		    else {
			sv_setsv(PL_subname,PL_curstname);
			sv_catpvs(PL_subname,"::");
			sv_catpvn(PL_subname,tmpbuf,len);
		    }
                    if (SvUTF8(PL_linestr))
                        SvUTF8_on(PL_subname);
		    have_name = TRUE;


		    s = skipspace(d);
		}
		else {
		    if (key == KEY_my || key == KEY_our || key==KEY_state)
		    {
			*d = '\0';
			/* diag_listed_as: Missing name in "%s sub" */
			Perl_croak(aTHX_
				  "Missing name in \"%s\"", PL_bufptr);
		    }
		    PL_expect = XATTRTERM;
		    sv_setpvs(PL_subname,"?");
		    have_name = FALSE;
		}

		if (key == KEY_format) {
		    if (format_name) {
                        NEXTVAL_NEXTTOKE.opval
                            = newSVOP(OP_CONST,0, format_name);
                        NEXTVAL_NEXTTOKE.opval->op_private |= OPpCONST_BARE;
                        force_next(BAREWORD);
                    }
		    PREBLOCK(FORMAT);
		}

		/* Look for a prototype */
		if (*s == '(' && !is_sigsub) {
		    s = scan_str(s,FALSE,FALSE,FALSE,NULL);
		    COPLINE_SET_FROM_MULTI_END;
		    if (!s)
			Perl_croak(aTHX_ "Prototype not terminated");
		    (void)validate_proto(PL_subname, PL_lex_stuff,
					 ckWARN(WARN_ILLEGALPROTO), 0);
		    have_proto = TRUE;

		    s = skipspace(s);
		}
		else
		    have_proto = FALSE;

		if (  !(*s == ':' && s[1] != ':')
                    && (*s != '{' && *s != '(') && key != KEY_format)
                {
                    assert(key == KEY_sub || key == KEY_AUTOLOAD ||
                           key == KEY_DESTROY || key == KEY_BEGIN ||
                           key == KEY_UNITCHECK || key == KEY_CHECK ||
                           key == KEY_INIT || key == KEY_END ||
                           key == KEY_my || key == KEY_state ||
                           key == KEY_our);
		    if (!have_name)
			Perl_croak(aTHX_ "Illegal declaration of anonymous subroutine");
		    else if (*s != ';' && *s != '}')
			Perl_croak(aTHX_ "Illegal declaration of subroutine %" SVf, SVfARG(PL_subname));
		}

		if (have_proto) {
		    NEXTVAL_NEXTTOKE.opval =
                        newSVOP(OP_CONST, 0, PL_lex_stuff);
		    PL_lex_stuff = NULL;
		    force_next(THING);
		}
		if (!have_name) {
		    if (PL_curstash)
			sv_setpvs(PL_subname, "__ANON__");
		    else
			sv_setpvs(PL_subname, "__ANON__::__ANON__");
                    if (is_sigsub)
                        TOKEN(ANON_SIGSUB);
                    else
                        TOKEN(ANONSUB);
		}
		force_ident_maybe_lex('&');
                if (is_sigsub)
                    TOKEN(SIGSUB);
                else
                    TOKEN(SUB);
	    }

	case KEY_system:
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
	case KEY_y:
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
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_NONEXPR)
		return REPORT(0);
	    pl_yylval.ival = CopLINE(PL_curcop);
	    OPERATOR(UNTIL);

	case KEY_unless:
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_NONEXPR)
		return REPORT(0);
	    pl_yylval.ival = CopLINE(PL_curcop);
	    OPERATOR(UNLESS);

	case KEY_unlink:
	    LOP(OP_UNLINK,XTERM);

	case KEY_undef:
	    UNIDOR(OP_UNDEF);

	case KEY_unpack:
	    LOP(OP_UNPACK,XTERM);

	case KEY_utime:
	    LOP(OP_UTIME,XTERM);

	case KEY_umask:
	    UNIDOR(OP_UMASK);

	case KEY_unshift:
	    LOP(OP_UNSHIFT,XTERM);

	case KEY_use:
	    s = tokenize_use(1, s);
	    TOKEN(USE);

	case KEY_values:
	    UNI(OP_VALUES);

	case KEY_vec:
	    LOP(OP_VEC,XTERM);

	case KEY_when:
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_NONEXPR)
		return REPORT(0);
	    pl_yylval.ival = CopLINE(PL_curcop);
            Perl_ck_warner_d(aTHX_
                packWARN(WARN_EXPERIMENTAL__SMARTMATCH),
                "when is experimental");
	    OPERATOR(WHEN);

	case KEY_while:
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_NONEXPR)
		return REPORT(0);
	    pl_yylval.ival = CopLINE(PL_curcop);
	    OPERATOR(WHILE);

	case KEY_warn:
	    PL_hints |= HINT_BLOCK_SCOPE;
	    LOP(OP_WARN,XTERM);

	case KEY_wait:
	    FUN0(OP_WAIT);

	case KEY_waitpid:
	    LOP(OP_WAITPID,XTERM);

	case KEY_wantarray:
	    FUN0(OP_WANTARRAY);

	case KEY_write:
            /* Make sure $^L is defined. 0x0C is CTRL-L on ASCII platforms, and
             * we use the same number on EBCDIC */
	    gv_fetchpvs("\x0C", GV_ADD|GV_NOTQUAL, SVt_PV);
	    UNI(OP_ENTERWRITE);

	case KEY_x:
	    if (PL_expect == XOPERATOR) {
		if (*s == '=' && !PL_lex_allbrackets
                    && PL_lex_fakeeof >= LEX_FAKEEOF_ASSIGN)
                {
		    return REPORT(0);
                }
		Mop(OP_REPEAT);
	    }
	    check_uni();
	    goto just_a_word;

	case KEY_xor:
	    if (!PL_lex_allbrackets && PL_lex_fakeeof >= LEX_FAKEEOF_LOWLOGIC)
		return REPORT(0);
	    pl_yylval.ival = OP_XOR;
	    OPERATOR(OROP);
	}
    }}
}

/*
  S_pending_ident

  Looks up an identifier in the pad or in a package

  is_sig indicates that this is a subroutine signature variable
  rather than a plain pad var.

  Returns:
    PRIVATEREF if this is a lexical name.
    BAREWORD   if this belongs to a package.

  Structure:
      if we're in a my declaration
	  croak if they tried to say my($foo::bar)
	  build the ops for a my() declaration
      if it's an access to a my() variable
	  build ops for access to a my() variable
      if in a dq string, and they've said @foo and we can't find @foo
	  warn
      build ops for a bareword
*/

static int
S_pending_ident(pTHX)
{
    PADOFFSET tmp = 0;
    const char pit = (char)pl_yylval.ival;
    const STRLEN tokenbuf_len = strlen(PL_tokenbuf);
    /* All routes through this function want to know if there is a colon.  */
    const char *const has_colon = (const char*) memchr (PL_tokenbuf, ':', tokenbuf_len);

    DEBUG_T({ PerlIO_printf(Perl_debug_log,
          "### Pending identifier '%s'\n", PL_tokenbuf); });
    assert(tokenbuf_len >= 2);

    /* if we're in a my(), we can't allow dynamics here.
       $foo'bar has already been turned into $foo::bar, so
       just check for colons.

       if it's a legal name, the OP is a PADANY.
    */
    if (PL_in_my) {
        if (PL_in_my == KEY_our) {	/* "our" is merely analogous to "my" */
            if (has_colon)
                /* diag_listed_as: No package name allowed for variable %s
                                   in "our" */
                yyerror_pv(Perl_form(aTHX_ "No package name allowed for "
                                  "%se %s in \"our\"",
                                  *PL_tokenbuf=='&' ?"subroutin":"variabl",
                                  PL_tokenbuf), UTF ? SVf_UTF8 : 0);
            tmp = allocmy(PL_tokenbuf, tokenbuf_len, UTF ? SVf_UTF8 : 0);
        }
        else {
            OP *o;
            if (has_colon) {
                /* "my" variable %s can't be in a package */
                /* PL_no_myglob is constant */
                GCC_DIAG_IGNORE_STMT(-Wformat-nonliteral);
                yyerror_pv(Perl_form(aTHX_ PL_no_myglob,
                            PL_in_my == KEY_my ? "my" : "state",
                            *PL_tokenbuf == '&' ? "subroutin" : "variabl",
                            PL_tokenbuf),
                            UTF ? SVf_UTF8 : 0);
                GCC_DIAG_RESTORE_STMT;
            }

            if (PL_in_my == KEY_sigvar) {
                /* A signature 'padop' needs in addition, an op_first to
                 * point to a child sigdefelem, and an extra field to hold
                 * the signature index. We can achieve both by using an
                 * UNOP_AUX and (ab)using the op_aux field to hold the
                 * index. If we ever need more fields, use a real malloced
                 * aux strut instead.
                 */
                o = newUNOP_AUX(OP_ARGELEM, 0, NULL,
                                    INT2PTR(UNOP_AUX_item *,
                                        (PL_parser->sig_elems)));
                o->op_private |= (  PL_tokenbuf[0] == '$' ? OPpARGELEM_SV
                                  : PL_tokenbuf[0] == '@' ? OPpARGELEM_AV
                                  :                         OPpARGELEM_HV);
            }
            else
                o = newOP(OP_PADANY, 0);
            o->op_targ = allocmy(PL_tokenbuf, tokenbuf_len,
                                                        UTF ? SVf_UTF8 : 0);
            if (PL_in_my == KEY_sigvar)
                PL_in_my = 0;

            pl_yylval.opval = o;
	    return PRIVATEREF;
        }
    }

    /*
       build the ops for accesses to a my() variable.
    */

    if (!has_colon) {
	if (!PL_in_my)
	    tmp = pad_findmy_pvn(PL_tokenbuf, tokenbuf_len,
                                 0);
        if (tmp != NOT_IN_PAD) {
            /* might be an "our" variable" */
            if (PAD_COMPNAME_FLAGS_isOUR(tmp)) {
                /* build ops for a bareword */
		HV *  const stash = PAD_COMPNAME_OURSTASH(tmp);
		HEK * const stashname = HvNAME_HEK(stash);
		SV *  const sym = newSVhek(stashname);
                sv_catpvs(sym, "::");
                sv_catpvn_flags(sym, PL_tokenbuf+1, tokenbuf_len > 0 ? tokenbuf_len - 1 : 0, (UTF ? SV_CATUTF8 : SV_CATBYTES ));
                pl_yylval.opval = newSVOP(OP_CONST, 0, sym);
                pl_yylval.opval->op_private = OPpCONST_ENTERED;
                if (pit != '&')
                  gv_fetchsv(sym,
                    GV_ADDMULTI,
                    ((PL_tokenbuf[0] == '$') ? SVt_PV
                     : (PL_tokenbuf[0] == '@') ? SVt_PVAV
                     : SVt_PVHV));
                return BAREWORD;
            }

            pl_yylval.opval = newOP(OP_PADANY, 0);
            pl_yylval.opval->op_targ = tmp;
            return PRIVATEREF;
        }
    }

    /*
       Whine if they've said @foo or @foo{key} in a doublequoted string,
       and @foo (or %foo) isn't a variable we can find in the symbol
       table.
    */
    if (ckWARN(WARN_AMBIGUOUS)
        && pit == '@'
        && PL_lex_state != LEX_NORMAL
        && !PL_lex_brackets)
    {
        GV *const gv = gv_fetchpvn_flags(PL_tokenbuf + 1, tokenbuf_len > 0 ? tokenbuf_len - 1 : 0,
                                         ( UTF ? SVf_UTF8 : 0 ) | GV_ADDMG,
                                         SVt_PVAV);
        if ((!gv || ((PL_tokenbuf[0] == '@') ? !GvAV(gv) : !GvHV(gv)))
	   )
        {
            /* Downgraded from fatal to warning 20000522 mjd */
            Perl_warner(aTHX_ packWARN(WARN_AMBIGUOUS),
			"Possible unintended interpolation of %" UTF8f
			" in string",
			UTF8fARG(UTF, tokenbuf_len, PL_tokenbuf));
        }
    }

    /* build ops for a bareword */
    pl_yylval.opval = newSVOP(OP_CONST, 0,
				   newSVpvn_flags(PL_tokenbuf + 1,
                                                      tokenbuf_len > 0 ? tokenbuf_len - 1 : 0,
                                                      UTF ? SVf_UTF8 : 0 ));
    pl_yylval.opval->op_private = OPpCONST_ENTERED;
    if (pit != '&')
        gv_fetchpvn_flags(PL_tokenbuf+1, tokenbuf_len > 0 ? tokenbuf_len - 1 : 0,
		     (PL_in_eval ? GV_ADDMULTI : GV_ADD)
                     | ( UTF ? SVf_UTF8 : 0 ),
		     ((PL_tokenbuf[0] == '$') ? SVt_PV
		      : (PL_tokenbuf[0] == '@') ? SVt_PVAV
		      : SVt_PVHV));
    return BAREWORD;
}

STATIC void
S_checkcomma(pTHX_ const char *s, const char *name, const char *what)
{
    PERL_ARGS_ASSERT_CHECKCOMMA;

    if (*s == ' ' && s[1] == '(') {	/* XXX gotta be a better way */
	if (ckWARN(WARN_SYNTAX)) {
	    int level = 1;
	    const char *w;
	    for (w = s+2; *w && level; w++) {
		if (*w == '(')
		    ++level;
		else if (*w == ')')
		    --level;
	    }
	    while (isSPACE(*w))
		++w;
	    /* the list of chars below is for end of statements or
	     * block / parens, boolean operators (&&, ||, //) and branch
	     * constructs (or, and, if, until, unless, while, err, for).
	     * Not a very solid hack... */
	    if (!*w || !strchr(";&/|})]oaiuwef!=", *w))
		Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
			    "%s (...) interpreted as function",name);
	}
    }
    while (s < PL_bufend && isSPACE(*s))
	s++;
    if (*s == '(')
	s++;
    while (s < PL_bufend && isSPACE(*s))
	s++;
    if (isIDFIRST_lazy_if_safe(s, PL_bufend, UTF)) {
	const char * const w = s;
        s += UTF ? UTF8SKIP(s) : 1;
	while (isWORDCHAR_lazy_if_safe(s, PL_bufend, UTF))
	    s += UTF ? UTF8SKIP(s) : 1;
	while (s < PL_bufend && isSPACE(*s))
	    s++;
	if (*s == ',') {
	    GV* gv;
	    if (keyword(w, s - w, 0))
		return;

	    gv = gv_fetchpvn_flags(w, s - w, ( UTF ? SVf_UTF8 : 0 ), SVt_PVCV);
	    if (gv && GvCVu(gv))
		return;
	    if (s - w <= 254) {
                PADOFFSET off;
		char tmpbuf[256];
		Copy(w, tmpbuf+1, s - w, char);
		*tmpbuf = '&';
		off = pad_findmy_pvn(tmpbuf, s-w+1, 0);
		if (off != NOT_IN_PAD) return;
	    }
	    Perl_croak(aTHX_ "No comma allowed after %s", what);
	}
    }
}

/* S_new_constant(): do any overload::constant lookup.

   Either returns sv, or mortalizes/frees sv and returns a new SV*.
   Best used as sv=new_constant(..., sv, ...).
   If s, pv are NULL, calls subroutine with one argument,
   and <type> is used with error messages only.
   <type> is assumed to be well formed UTF-8 */

STATIC SV *
S_new_constant(pTHX_ const char *s, STRLEN len, const char *key, STRLEN keylen,
	       SV *sv, SV *pv, const char *type, STRLEN typelen)
{
    dSP;
    HV * table = GvHV(PL_hintgv);		 /* ^H */
    SV *res;
    SV *errsv = NULL;
    SV **cvp;
    SV *cv, *typesv;
    const char *why1 = "", *why2 = "", *why3 = "";

    PERL_ARGS_ASSERT_NEW_CONSTANT;
    /* We assume that this is true: */
    if (*key == 'c') { assert (strEQ(key, "charnames")); }
    assert(type || s);

    /* charnames doesn't work well if there have been errors found */
    if (PL_error_count > 0 && *key == 'c')
    {
	SvREFCNT_dec_NN(sv);
	return &PL_sv_undef;
    }

    sv_2mortal(sv);			/* Parent created it permanently */
    if (!table
	|| ! (PL_hints & HINT_LOCALIZE_HH)
	|| ! (cvp = hv_fetch(table, key, keylen, FALSE))
	|| ! SvOK(*cvp))
    {
	char *msg;

	/* Here haven't found what we're looking for.  If it is charnames,
	 * perhaps it needs to be loaded.  Try doing that before giving up */
	if (*key == 'c') {
	    Perl_load_module(aTHX_
		            0,
			    newSVpvs("_charnames"),
			     /* version parameter; no need to specify it, as if
			      * we get too early a version, will fail anyway,
			      * not being able to find '_charnames' */
			    NULL,
			    newSVpvs(":full"),
			    newSVpvs(":short"),
			    NULL);
            assert(sp == PL_stack_sp);
	    table = GvHV(PL_hintgv);
	    if (table
		&& (PL_hints & HINT_LOCALIZE_HH)
		&& (cvp = hv_fetch(table, key, keylen, FALSE))
		&& SvOK(*cvp))
	    {
		goto now_ok;
	    }
	}
	if (!table || !(PL_hints & HINT_LOCALIZE_HH)) {
	    msg = Perl_form(aTHX_
			       "Constant(%.*s) unknown",
				(int)(type ? typelen : len),
				(type ? type: s));
	}
	else {
            why1 = "$^H{";
            why2 = key;
            why3 = "} is not defined";
        report:
            if (*key == 'c') {
                msg = Perl_form(aTHX_
                            /* The +3 is for '\N{'; -4 for that, plus '}' */
                            "Unknown charname '%.*s'", (int)typelen - 4, type + 3
                      );
            }
            else {
                msg = Perl_form(aTHX_ "Constant(%.*s): %s%s%s",
                                    (int)(type ? typelen : len),
                                    (type ? type: s), why1, why2, why3);
            }
        }
	yyerror_pv(msg, UTF ? SVf_UTF8 : 0);
  	return SvREFCNT_inc_simple_NN(sv);
    }
  now_ok:
    cv = *cvp;
    if (!pv && s)
  	pv = newSVpvn_flags(s, len, SVs_TEMP);
    if (type && pv)
  	typesv = newSVpvn_flags(type, typelen, SVs_TEMP);
    else
  	typesv = &PL_sv_undef;

    PUSHSTACKi(PERLSI_OVERLOAD);
    ENTER ;
    SAVETMPS;

    PUSHMARK(SP) ;
    EXTEND(sp, 3);
    if (pv)
 	PUSHs(pv);
    PUSHs(sv);
    if (pv)
 	PUSHs(typesv);
    PUTBACK;
    call_sv(cv, G_SCALAR | ( PL_in_eval ? 0 : G_EVAL));

    SPAGAIN ;

    /* Check the eval first */
    if (!PL_in_eval && ((errsv = ERRSV), SvTRUE_NN(errsv))) {
	STRLEN errlen;
	const char * errstr;
	sv_catpvs(errsv, "Propagated");
	errstr = SvPV_const(errsv, errlen);
	yyerror_pvn(errstr, errlen, 0); /* Duplicates the message inside eval */
	(void)POPs;
	res = SvREFCNT_inc_simple_NN(sv);
    }
    else {
 	res = POPs;
	SvREFCNT_inc_simple_void_NN(res);
    }

    PUTBACK ;
    FREETMPS ;
    LEAVE ;
    POPSTACK;

    if (!SvOK(res)) {
 	why1 = "Call to &{$^H{";
 	why2 = key;
 	why3 = "}} did not return a defined value";
 	sv = res;
	(void)sv_2mortal(sv);
 	goto report;
    }

    return res;
}

PERL_STATIC_INLINE void
S_parse_ident(pTHX_ char **s, char **d, char * const e, int allow_package,
                    bool is_utf8, bool check_dollar, bool tick_warn)
{
    int saw_tick = 0;
    const char *olds = *s;
    PERL_ARGS_ASSERT_PARSE_IDENT;

    while (*s < PL_bufend) {
        if (*d >= e)
            Perl_croak(aTHX_ "%s", ident_too_long);
        if (is_utf8 && isIDFIRST_utf8_safe(*s, PL_bufend)) {
             /* The UTF-8 case must come first, otherwise things
             * like c\N{COMBINING TILDE} would start failing, as the
             * isWORDCHAR_A case below would gobble the 'c' up.
             */

            char *t = *s + UTF8SKIP(*s);
            while (isIDCONT_utf8_safe((const U8*) t, (const U8*) PL_bufend)) {
                t += UTF8SKIP(t);
            }
            if (*d + (t - *s) > e)
                Perl_croak(aTHX_ "%s", ident_too_long);
            Copy(*s, *d, t - *s, char);
            *d += t - *s;
            *s = t;
        }
        else if ( isWORDCHAR_A(**s) ) {
            do {
                *(*d)++ = *(*s)++;
            } while (isWORDCHAR_A(**s) && *d < e);
        }
        else if (   allow_package
                 && **s == '\''
                 && isIDFIRST_lazy_if_safe((*s)+1, PL_bufend, is_utf8))
        {
            *(*d)++ = ':';
            *(*d)++ = ':';
            (*s)++;
            saw_tick++;
        }
        else if (allow_package && **s == ':' && (*s)[1] == ':'
           /* Disallow things like Foo::$bar. For the curious, this is
            * the code path that triggers the "Bad name after" warning
            * when looking for barewords.
            */
           && !(check_dollar && (*s)[2] == '$')) {
            *(*d)++ = *(*s)++;
            *(*d)++ = *(*s)++;
        }
        else
            break;
    }
    if (UNLIKELY(tick_warn && saw_tick && PL_lex_state == LEX_INTERPNORMAL
              && !PL_lex_brackets && ckWARN(WARN_SYNTAX))) {
        char *d;
	char *d2;
        Newx(d, *s - olds + saw_tick + 2, char); /* +2 for $# */
        d2 = d;
        SAVEFREEPV(d);
        Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
                         "Old package separator used in string");
        if (olds[-1] == '#')
            *d2++ = olds[-2];
        *d2++ = olds[-1];
        while (olds < *s) {
            if (*olds == '\'') {
                *d2++ = '\\';
                *d2++ = *olds++;
            }
	    else
                *d2++ = *olds++;
        }
        Perl_warner(aTHX_ packWARN(WARN_SYNTAX),
                         "\t(Did you mean \"%" UTF8f "\" instead?)\n",
                          UTF8fARG(is_utf8, d2-d, d));
    }
    return;
}

/* Returns a NUL terminated string, with the length of the string written to
   *slp
   */
STATIC char *
S_scan_word(pTHX_ char *s, char *dest, STRLEN destlen, int allow_package, STRLEN *slp)
{
    char *d = dest;
    char * const e = d + destlen - 3;  /* two-character token, ending NUL */
    bool is_utf8 = cBOOL(UTF);

    PERL_ARGS_ASSERT_SCAN_WORD;

    parse_ident(&s, &d, e, allow_package, is_utf8, TRUE, FALSE);
    *d = '\0';
    *slp = d - dest;
    return s;
}

/* Is the byte 'd' a legal single character identifier name?  'u' is true
 * iff Unicode semantics are to be used.  The legal ones are any of:
 *  a) all ASCII characters except:
 *          1) control and space-type ones, like NUL, SOH, \t, and SPACE;
 *          2) '{'
 *     The final case currently doesn't get this far in the program, so we
 *     don't test for it.  If that were to change, it would be ok to allow it.
 *  b) When not under Unicode rules, any upper Latin1 character
 *  c) Otherwise, when unicode rules are used, all XIDS characters.
 *
 *      Because all ASCII characters have the same representation whether
 *      encoded in UTF-8 or not, we can use the foo_A macros below and '\0' and
 *      '{' without knowing if is UTF-8 or not. */
#define VALID_LEN_ONE_IDENT(s, e, is_utf8)                                  \
    (isGRAPH_A(*(s)) || ((is_utf8)                                          \
                         ? isIDFIRST_utf8_safe(s, e)                        \
                         : (isGRAPH_L1(*s)                                  \
                            && LIKELY((U8) *(s) != LATIN1_TO_NATIVE(0xAD)))))

STATIC char *
S_scan_ident(pTHX_ char *s, char *dest, STRLEN destlen, I32 ck_uni)
{
    I32 herelines = PL_parser->herelines;
    SSize_t bracket = -1;
    char funny = *s++;
    char *d = dest;
    char * const e = d + destlen - 3;    /* two-character token, ending NUL */
    bool is_utf8 = cBOOL(UTF);
    I32 orig_copline = 0, tmp_copline = 0;

    PERL_ARGS_ASSERT_SCAN_IDENT;

    if (isSPACE(*s) || !*s)
	s = skipspace(s);
    if (isDIGIT(*s)) {
	while (isDIGIT(*s)) {
	    if (d >= e)
		Perl_croak(aTHX_ "%s", ident_too_long);
	    *d++ = *s++;
	}
    }
    else {  /* See if it is a "normal" identifier */
        parse_ident(&s, &d, e, 1, is_utf8, FALSE, TRUE);
    }
    *d = '\0';
    d = dest;
    if (*d) {
        /* Either a digit variable, or parse_ident() found an identifier
           (anything valid as a bareword), so job done and return.  */
	if (PL_lex_state != LEX_NORMAL)
	    PL_lex_state = LEX_INTERPENDMAYBE;
	return s;
    }

    /* Here, it is not a run-of-the-mill identifier name */

    if (*s == '$' && s[1]
        && (   isIDFIRST_lazy_if_safe(s+1, PL_bufend, is_utf8)
            || isDIGIT_A((U8)s[1])
            || s[1] == '$'
            || s[1] == '{'
            || memBEGINs(s+1, (STRLEN) (PL_bufend - (s+1)), "::")) )
    {
        /* Dereferencing a value in a scalar variable.
           The alternatives are different syntaxes for a scalar variable.
           Using ' as a leading package separator isn't allowed. :: is.   */
	return s;
    }
    /* Handle the opening { of @{...}, &{...}, *{...}, %{...}, ${...}  */
    if (*s == '{') {
	bracket = s - SvPVX(PL_linestr);
	s++;
	orig_copline = CopLINE(PL_curcop);
        if (s < PL_bufend && isSPACE(*s)) {
            s = skipspace(s);
        }
    }
    if ((s <= PL_bufend - (is_utf8)
                          ? UTF8SKIP(s)
                          : 1)
        && VALID_LEN_ONE_IDENT(s, PL_bufend, is_utf8))
    {
        if (is_utf8) {
            const STRLEN skip = UTF8SKIP(s);
            STRLEN i;
            d[skip] = '\0';
            for ( i = 0; i < skip; i++ )
                d[i] = *s++;
        }
        else {
            *d = *s++;
            d[1] = '\0';
        }
    }
    /* Convert $^F, ${^F} and the ^F of ${^FOO} to control characters */
    if (*d == '^' && *s && isCONTROLVAR(*s)) {
	*d = toCTRL(*s);
	s++;
    }
    /* Warn about ambiguous code after unary operators if {...} notation isn't
       used.  There's no difference in ambiguity; it's merely a heuristic
       about when not to warn.  */
    else if (ck_uni && bracket == -1)
	check_uni();
    if (bracket != -1) {
        bool skip;
        char *s2;
        /* If we were processing {...} notation then...  */
        if (isIDFIRST_lazy_if_safe(d, e, is_utf8)
            || (!isPRINT(*d) /* isCNTRL(d), plus all non-ASCII */
                 && isWORDCHAR(*s))
        ) {
            /* note we have to check for a normal identifier first,
             * as it handles utf8 symbols, and only after that has
             * been ruled out can we look at the caret words */
            if (isIDFIRST_lazy_if_safe(d, e, is_utf8) ) {
                /* if it starts as a valid identifier, assume that it is one.
                   (the later check for } being at the expected point will trap
                   cases where this doesn't pan out.)  */
                d += is_utf8 ? UTF8SKIP(d) : 1;
                parse_ident(&s, &d, e, 1, is_utf8, TRUE, TRUE);
                *d = '\0';
            }
            else { /* caret word: ${^Foo} ${^CAPTURE[0]} */
                d++;
                while (isWORDCHAR(*s) && d < e) {
                    *d++ = *s++;
                }
                if (d >= e)
                    Perl_croak(aTHX_ "%s", ident_too_long);
                *d = '\0';
            }
            tmp_copline = CopLINE(PL_curcop);
            if (s < PL_bufend && isSPACE(*s)) {
                s = skipspace(s);
            }
	    if ((*s == '[' || (*s == '{' && strNE(dest, "sub")))) {
                /* ${foo[0]} and ${foo{bar}} and ${^CAPTURE[0]} notation.  */
		if (ckWARN(WARN_AMBIGUOUS) && keyword(dest, d - dest, 0)) {
		    const char * const brack =
			(const char *)
			((*s == '[') ? "[...]" : "{...}");
                    orig_copline = CopLINE(PL_curcop);
                    CopLINE_set(PL_curcop, tmp_copline);
   /* diag_listed_as: Ambiguous use of %c{%s[...]} resolved to %c%s[...] */
		    Perl_warner(aTHX_ packWARN(WARN_AMBIGUOUS),
			"Ambiguous use of %c{%s%s} resolved to %c%s%s",
			funny, dest, brack, funny, dest, brack);
                    CopLINE_set(PL_curcop, orig_copline);
		}
		bracket++;
		PL_lex_brackstack[PL_lex_brackets++] = (char)(XOPERATOR | XFAKEBRACK);
		PL_lex_allbrackets++;
		return s;
	    }
	}

        if ( !tmp_copline )
            tmp_copline = CopLINE(PL_curcop);
        if ((skip = s < PL_bufend && isSPACE(*s))) {
            /* Avoid incrementing line numbers or resetting PL_linestart,
               in case we have to back up.  */
            STRLEN s_off = s - SvPVX(PL_linestr);
            s2 = peekspace(s);
            s = SvPVX(PL_linestr) + s_off;
        }
        else
            s2 = s;

        /* Expect to find a closing } after consuming any trailing whitespace.
         */
        if (*s2 == '}') {
            /* Now increment line numbers if applicable.  */
            if (skip)
                s = skipspace(s);
	    s++;
	    if (PL_lex_state == LEX_INTERPNORMAL && !PL_lex_brackets) {
		PL_lex_state = LEX_INTERPEND;
		PL_expect = XREF;
	    }
	    if (PL_lex_state == LEX_NORMAL) {
		if (ckWARN(WARN_AMBIGUOUS)
                    && (keyword(dest, d - dest, 0)
		        || get_cvn_flags(dest, d - dest, is_utf8
                           ? SVf_UTF8
                           : 0)))
		{
                    SV *tmp = newSVpvn_flags( dest, d - dest,
                                        SVs_TEMP | (is_utf8 ? SVf_UTF8 : 0) );
		    if (funny == '#')
			funny = '@';
                    orig_copline = CopLINE(PL_curcop);
                    CopLINE_set(PL_curcop, tmp_copline);
		    Perl_warner(aTHX_ packWARN(WARN_AMBIGUOUS),
			"Ambiguous use of %c{%" SVf "} resolved to %c%" SVf,
			funny, SVfARG(tmp), funny, SVfARG(tmp));
                    CopLINE_set(PL_curcop, orig_copline);
		}
	    }
	}
	else {
            /* Didn't find the closing } at the point we expected, so restore
               state such that the next thing to process is the opening { and */
	    s = SvPVX(PL_linestr) + bracket; /* let the parser handle it */
            CopLINE_set(PL_curcop, orig_copline);
            PL_parser->herelines = herelines;
	    *dest = '\0';
            PL_parser->sub_no_recover = TRUE;
	}
    }
    else if (   PL_lex_state == LEX_INTERPNORMAL
             && !PL_lex_brackets
             && !intuit_more(s, PL_bufend))
	PL_lex_state = LEX_INTERPEND;
    return s;
}

static bool
S_pmflag(pTHX_ const char* const valid_flags, U32 * pmfl, char** s, char* charset, unsigned int * x_mod_count) {

    /* Adds, subtracts to/from 'pmfl' based on the next regex modifier flag
     * found in the parse starting at 's', based on the subset that are valid
     * in this context input to this routine in 'valid_flags'. Advances s.
     * Returns TRUE if the input should be treated as a valid flag, so the next
     * char may be as well; otherwise FALSE. 'charset' should point to a NUL
     * upon first call on the current regex.  This routine will set it to any
     * charset modifier found.  The caller shouldn't change it.  This way,
     * another charset modifier encountered in the parse can be detected as an
     * error, as we have decided to allow only one */

    const char c = **s;
    STRLEN charlen = UTF ? UTF8SKIP(*s) : 1;

    if ( charlen != 1 || ! strchr(valid_flags, c) ) {
        if (isWORDCHAR_lazy_if_safe( *s, PL_bufend, UTF)) {
            yyerror_pv(Perl_form(aTHX_ "Unknown regexp modifier \"/%.*s\"", (int)charlen, *s),
                       UTF ? SVf_UTF8 : 0);
            (*s) += charlen;
            /* Pretend that it worked, so will continue processing before
             * dieing */
            return TRUE;
        }
        return FALSE;
    }

    switch (c) {

        CASE_STD_PMMOD_FLAGS_PARSE_SET(pmfl, *x_mod_count);
        case GLOBAL_PAT_MOD:      *pmfl |= PMf_GLOBAL; break;
        case CONTINUE_PAT_MOD:    *pmfl |= PMf_CONTINUE; break;
        case ONCE_PAT_MOD:        *pmfl |= PMf_KEEP; break;
        case KEEPCOPY_PAT_MOD:    *pmfl |= RXf_PMf_KEEPCOPY; break;
        case NONDESTRUCT_PAT_MOD: *pmfl |= PMf_NONDESTRUCT; break;
	case LOCALE_PAT_MOD:
	    if (*charset) {
		goto multiple_charsets;
	    }
	    set_regex_charset(pmfl, REGEX_LOCALE_CHARSET);
	    *charset = c;
	    break;
	case UNICODE_PAT_MOD:
	    if (*charset) {
		goto multiple_charsets;
	    }
	    set_regex_charset(pmfl, REGEX_UNICODE_CHARSET);
	    *charset = c;
	    break;
	case ASCII_RESTRICT_PAT_MOD:
	    if (! *charset) {
		set_regex_charset(pmfl, REGEX_ASCII_RESTRICTED_CHARSET);
	    }
	    else {

		/* Error if previous modifier wasn't an 'a', but if it was, see
		 * if, and accept, a second occurrence (only) */
		if (*charset != 'a'
		    || get_regex_charset(*pmfl)
			!= REGEX_ASCII_RESTRICTED_CHARSET)
		{
			goto multiple_charsets;
		}
		set_regex_charset(pmfl, REGEX_ASCII_MORE_RESTRICTED_CHARSET);
	    }
	    *charset = c;
	    break;
	case DEPENDS_PAT_MOD:
	    if (*charset) {
		goto multiple_charsets;
	    }
	    set_regex_charset(pmfl, REGEX_DEPENDS_CHARSET);
	    *charset = c;
	    break;
    }

    (*s)++;
    return TRUE;

    multiple_charsets:
	if (*charset != c) {
	    yyerror(Perl_form(aTHX_ "Regexp modifiers \"/%c\" and \"/%c\" are mutually exclusive", *charset, c));
	}
	else if (c == 'a') {
  /* diag_listed_as: Regexp modifier "/%c" may appear a maximum of twice */
	    yyerror("Regexp modifier \"/a\" may appear a maximum of twice");
	}
	else {
	    yyerror(Perl_form(aTHX_ "Regexp modifier \"/%c\" may not appear twice", c));
	}

	/* Pretend that it worked, so will continue processing before dieing */
	(*s)++;
	return TRUE;
}

STATIC char *
S_scan_pat(pTHX_ char *start, I32 type)
{
    PMOP *pm;
    char *s;
    const char * const valid_flags =
	(const char *)((type == OP_QR) ? QR_PAT_MODS : M_PAT_MODS);
    char charset = '\0';    /* character set modifier */
    unsigned int x_mod_count = 0;

    PERL_ARGS_ASSERT_SCAN_PAT;

    s = scan_str(start,TRUE,FALSE, (PL_in_eval & EVAL_RE_REPARSING), NULL);
    if (!s)
	Perl_croak(aTHX_ "Search pattern not terminated");

    pm = (PMOP*)newPMOP(type, 0);
    if (PL_multi_open == '?') {
	/* This is the only point in the code that sets PMf_ONCE:  */
	pm->op_pmflags |= PMf_ONCE;

	/* Hence it's safe to do this bit of PMOP book-keeping here, which
	   allows us to restrict the list needed by reset to just the ??
	   matches.  */
	assert(type != OP_TRANS);
	if (PL_curstash) {
	    MAGIC *mg = mg_find((const SV *)PL_curstash, PERL_MAGIC_symtab);
	    U32 elements;
	    if (!mg) {
		mg = sv_magicext(MUTABLE_SV(PL_curstash), 0, PERL_MAGIC_symtab, 0, 0,
				 0);
	    }
	    elements = mg->mg_len / sizeof(PMOP**);
	    Renewc(mg->mg_ptr, elements + 1, PMOP*, char);
	    ((PMOP**)mg->mg_ptr) [elements++] = pm;
	    mg->mg_len = elements * sizeof(PMOP**);
	    PmopSTASH_set(pm,PL_curstash);
	}
    }

    /* if qr/...(?{..}).../, then need to parse the pattern within a new
     * anon CV. False positives like qr/[(?{]/ are harmless */

    if (type == OP_QR) {
	STRLEN len;
	char *e, *p = SvPV(PL_lex_stuff, len);
	e = p + len;
	for (; p < e; p++) {
	    if (p[0] == '(' && p[1] == '?'
		&& (p[2] == '{' || (p[2] == '?' && p[3] == '{')))
	    {
		pm->op_pmflags |= PMf_HAS_CV;
		break;
	    }
	}
	pm->op_pmflags |= PMf_IS_QR;
    }

    while (*s && S_pmflag(aTHX_ valid_flags, &(pm->op_pmflags),
                                &s, &charset, &x_mod_count))
    {};
    /* issue a warning if /c is specified,but /g is not */
    if ((pm->op_pmflags & PMf_CONTINUE) && !(pm->op_pmflags & PMf_GLOBAL))
    {
        Perl_ck_warner(aTHX_ packWARN(WARN_REGEXP),
		       "Use of /c modifier is meaningless without /g" );
    }

    PL_lex_op = (OP*)pm;
    pl_yylval.ival = OP_MATCH;
    return s;
}

STATIC char *
S_scan_subst(pTHX_ char *start)
{
    char *s;
    PMOP *pm;
    I32 first_start;
    line_t first_line;
    line_t linediff = 0;
    I32 es = 0;
    char charset = '\0';    /* character set modifier */
    unsigned int x_mod_count = 0;
    char *t;

    PERL_ARGS_ASSERT_SCAN_SUBST;

    pl_yylval.ival = OP_NULL;

    s = scan_str(start, TRUE, FALSE, FALSE, &t);

    if (!s)
	Perl_croak(aTHX_ "Substitution pattern not terminated");

    s = t;

    first_start = PL_multi_start;
    first_line = CopLINE(PL_curcop);
    s = scan_str(s,FALSE,FALSE,FALSE,NULL);
    if (!s) {
	SvREFCNT_dec_NN(PL_lex_stuff);
	PL_lex_stuff = NULL;
	Perl_croak(aTHX_ "Substitution replacement not terminated");
    }
    PL_multi_start = first_start;	/* so whole substitution is taken together */

    pm = (PMOP*)newPMOP(OP_SUBST, 0);


    while (*s) {
	if (*s == EXEC_PAT_MOD) {
	    s++;
	    es++;
	}
	else if (! S_pmflag(aTHX_ S_PAT_MODS, &(pm->op_pmflags),
                                  &s, &charset, &x_mod_count))
	{
	    break;
	}
    }

    if ((pm->op_pmflags & PMf_CONTINUE)) {
        Perl_ck_warner(aTHX_ packWARN(WARN_REGEXP), "Use of /c modifier is meaningless in s///" );
    }

    if (es) {
	SV * const repl = newSVpvs("");

	PL_multi_end = 0;
	pm->op_pmflags |= PMf_EVAL;
        for (; es > 1; es--) {
            sv_catpvs(repl, "eval ");
        }
        sv_catpvs(repl, "do {");
	sv_catsv(repl, PL_parser->lex_sub_repl);
	sv_catpvs(repl, "}");
	SvREFCNT_dec(PL_parser->lex_sub_repl);
	PL_parser->lex_sub_repl = repl;
    }


    linediff = CopLINE(PL_curcop) - first_line;
    if (linediff)
	CopLINE_set(PL_curcop, first_line);

    if (linediff || es) {
        /* the IVX field indicates that the replacement string is a s///e;
         * the NVX field indicates how many src code lines the replacement
         * spreads over */
        sv_upgrade(PL_parser->lex_sub_repl, SVt_PVNV);
        ((XPVNV*)SvANY(PL_parser->lex_sub_repl))->xnv_u.xnv_lines = linediff;
        ((XPVIV*)SvANY(PL_parser->lex_sub_repl))->xiv_u.xivu_eval_seen =
                                                                    cBOOL(es);
    }

    PL_lex_op = (OP*)pm;
    pl_yylval.ival = OP_SUBST;
    return s;
}

STATIC char *
S_scan_trans(pTHX_ char *start)
{
    char* s;
    OP *o;
    U8 squash;
    U8 del;
    U8 complement;
    bool nondestruct = 0;
    char *t;

    PERL_ARGS_ASSERT_SCAN_TRANS;

    pl_yylval.ival = OP_NULL;

    s = scan_str(start,FALSE,FALSE,FALSE,&t);
    if (!s)
	Perl_croak(aTHX_ "Transliteration pattern not terminated");

    s = t;

    s = scan_str(s,FALSE,FALSE,FALSE,NULL);
    if (!s) {
	SvREFCNT_dec_NN(PL_lex_stuff);
	PL_lex_stuff = NULL;
	Perl_croak(aTHX_ "Transliteration replacement not terminated");
    }

    complement = del = squash = 0;
    while (1) {
	switch (*s) {
	case 'c':
	    complement = OPpTRANS_COMPLEMENT;
	    break;
	case 'd':
	    del = OPpTRANS_DELETE;
	    break;
	case 's':
	    squash = OPpTRANS_SQUASH;
	    break;
	case 'r':
	    nondestruct = 1;
	    break;
	default:
	    goto no_more;
	}
	s++;
    }
  no_more:

    o = newPVOP(nondestruct ? OP_TRANSR : OP_TRANS, 0, (char*)NULL);
    o->op_private &= ~OPpTRANS_ALL;
    o->op_private |= del|squash|complement|
      (DO_UTF8(PL_lex_stuff)? OPpTRANS_FROM_UTF : 0)|
      (DO_UTF8(PL_parser->lex_sub_repl) ? OPpTRANS_TO_UTF   : 0);

    PL_lex_op = o;
    pl_yylval.ival = nondestruct ? OP_TRANSR : OP_TRANS;


    return s;
}

/* scan_heredoc
   Takes a pointer to the first < in <<FOO.
   Returns a pointer to the byte following <<FOO.

   This function scans a heredoc, which involves different methods
   depending on whether we are in a string eval, quoted construct, etc.
   This is because PL_linestr could containing a single line of input, or
   a whole string being evalled, or the contents of the current quote-
   like operator.

   The two basic methods are:
    - Steal lines from the input stream
    - Scan the heredoc in PL_linestr and remove it therefrom

   In a file scope or filtered eval, the first method is used; in a
   string eval, the second.

   In a quote-like operator, we have to choose between the two,
   depending on where we can find a newline.  We peek into outer lex-
   ing scopes until we find one with a newline in it.  If we reach the
   outermost lexing scope and it is a file, we use the stream method.
   Otherwise it is treated as an eval.
*/

STATIC char *
S_scan_heredoc(pTHX_ char *s)
{
    I32 op_type = OP_SCALAR;
    I32 len;
    SV *tmpstr;
    char term;
    char *d;
    char *e;
    char *peek;
    char *indent = 0;
    I32 indent_len = 0;
    bool indented = FALSE;
    const bool infile = PL_rsfp || PL_parser->filtered;
    const line_t origline = CopLINE(PL_curcop);
    LEXSHARED *shared = PL_parser->lex_shared;

    PERL_ARGS_ASSERT_SCAN_HEREDOC;

    s += 2;
    d = PL_tokenbuf + 1;
    e = PL_tokenbuf + sizeof PL_tokenbuf - 1;
    *PL_tokenbuf = '\n';
    peek = s;
    if (*peek == '~') {
	indented = TRUE;
	peek++; s++;
    }
    while (SPACE_OR_TAB(*peek))
	peek++;
    if (*peek == '`' || *peek == '\'' || *peek =='"') {
	s = peek;
	term = *s++;
	s = delimcpy(d, e, s, PL_bufend, term, &len);
	if (s == PL_bufend)
	    Perl_croak(aTHX_ "Unterminated delimiter for here document");
	d += len;
	s++;
    }
    else {
	if (*s == '\\')
            /* <<\FOO is equivalent to <<'FOO' */
	    s++, term = '\'';
	else
	    term = '"';
	if (! isWORDCHAR_lazy_if_safe(s, PL_bufend, UTF))
	    Perl_croak(aTHX_ "Use of bare << to mean <<\"\" is forbidden");
	peek = s;
        while (isWORDCHAR_lazy_if_safe(peek, PL_bufend, UTF)) {
	    peek += UTF ? UTF8SKIP(peek) : 1;
	}
	len = (peek - s >= e - d) ? (e - d) : (peek - s);
	Copy(s, d, len, char);
	s += len;
	d += len;
    }
    if (d >= PL_tokenbuf + sizeof PL_tokenbuf - 1)
	Perl_croak(aTHX_ "Delimiter for here document is too long");
    *d++ = '\n';
    *d = '\0';
    len = d - PL_tokenbuf;

#ifndef PERL_STRICT_CR
    d = (char *) memchr(s, '\r', PL_bufend - s);
    if (d) {
	char * const olds = s;
	s = d;
	while (s < PL_bufend) {
	    if (*s == '\r') {
		*d++ = '\n';
		if (*++s == '\n')
		    s++;
	    }
	    else if (*s == '\n' && s[1] == '\r') {	/* \015\013 on a mac? */
		*d++ = *s++;
		s++;
	    }
	    else
		*d++ = *s++;
	}
	*d = '\0';
	PL_bufend = d;
	SvCUR_set(PL_linestr, PL_bufend - SvPVX_const(PL_linestr));
	s = olds;
    }
#endif

    tmpstr = newSV_type(SVt_PVIV);
    SvGROW(tmpstr, 80);
    if (term == '\'') {
	op_type = OP_CONST;
	SvIV_set(tmpstr, -1);
    }
    else if (term == '`') {
	op_type = OP_BACKTICK;
	SvIV_set(tmpstr, '\\');
    }

    PL_multi_start = origline + 1 + PL_parser->herelines;
    PL_multi_open = PL_multi_close = '<';
    /* inside a string eval or quote-like operator */
    if (!infile || PL_lex_inwhat) {
	SV *linestr;
	char *bufend;
	char * const olds = s;
	PERL_CONTEXT * const cx = CX_CUR();
	/* These two fields are not set until an inner lexing scope is
	   entered.  But we need them set here. */
	shared->ls_bufptr  = s;
	shared->ls_linestr = PL_linestr;
	if (PL_lex_inwhat)
	  /* Look for a newline.  If the current buffer does not have one,
	     peek into the line buffer of the parent lexing scope, going
 	     up as many levels as necessary to find one with a newline
	     after bufptr.
	   */
	  while (!(s = (char *)memchr(
		    (void *)shared->ls_bufptr, '\n',
		    SvEND(shared->ls_linestr)-shared->ls_bufptr
		))) {
	    shared = shared->ls_prev;
	    /* shared is only null if we have gone beyond the outermost
	       lexing scope.  In a file, we will have broken out of the
	       loop in the previous iteration.  In an eval, the string buf-
	       fer ends with "\n;", so the while condition above will have
	       evaluated to false.  So shared can never be null.  Or so you
	       might think.  Odd syntax errors like s;@{<<; can gobble up
	       the implicit semicolon at the end of a flie, causing the
	       file handle to be closed even when we are not in a string
	       eval.  So shared may be null in that case.
               (Closing '}' here to balance the earlier open brace for
               editors that look for matched pairs.) */
	    if (UNLIKELY(!shared))
		goto interminable;
	    /* A LEXSHARED struct with a null ls_prev pointer is the outer-
	       most lexing scope.  In a file, shared->ls_linestr at that
	       level is just one line, so there is no body to steal. */
	    if (infile && !shared->ls_prev) {
		s = olds;
		goto streaming;
	    }
	  }
	else {	/* eval or we've already hit EOF */
	    s = (char*)memchr((void*)s, '\n', PL_bufend - s);
	    if (!s)
                goto interminable;
	}
	linestr = shared->ls_linestr;
	bufend = SvEND(linestr);
	d = s;
	if (indented) {
	    char *myolds = s;

	    while (s < bufend - len + 1) {
		if (*s++ == '\n')
		    ++PL_parser->herelines;

		if (memEQ(s, PL_tokenbuf + 1, len - 1)) {
		    char *backup = s;
		    indent_len = 0;

		    /* Only valid if it's preceded by whitespace only */
		    while (backup != myolds && --backup >= myolds) {
			if (! SPACE_OR_TAB(*backup)) {
			    break;
			}

			indent_len++;
		    }

		    /* No whitespace or all! */
		    if (backup == s || *backup == '\n') {
			Newx(indent, indent_len + 1, char);
			memcpy(indent, backup + 1, indent_len);
			indent[indent_len] = 0;
			s--; /* before our delimiter */
			PL_parser->herelines--; /* this line doesn't count */
			break;
		    }
		}
	    }
	} else {
	    while (s < bufend - len + 1
	           && memNE(s,PL_tokenbuf,len) )
	    {
		if (*s++ == '\n')
		    ++PL_parser->herelines;
	    }
	}

	if (s >= bufend - len + 1) {
	    goto interminable;
	}
	sv_setpvn(tmpstr,d+1,s-d);
	s += len - 1;
	/* the preceding stmt passes a newline */
	PL_parser->herelines++;

	/* s now points to the newline after the heredoc terminator.
	   d points to the newline before the body of the heredoc.
	 */

	/* We are going to modify linestr in place here, so set
	   aside copies of the string if necessary for re-evals or
	   (caller $n)[6]. */
	/* See the Paranoia note in case LEX_INTERPEND in yylex, for why we
	   check shared->re_eval_str. */
	if (shared->re_eval_start || shared->re_eval_str) {
	    /* Set aside the rest of the regexp */
	    if (!shared->re_eval_str)
		shared->re_eval_str =
		       newSVpvn(shared->re_eval_start,
				bufend - shared->re_eval_start);
	    shared->re_eval_start -= s-d;
	}
	if (cxstack_ix >= 0
            && CxTYPE(cx) == CXt_EVAL
            && CxOLD_OP_TYPE(cx) == OP_ENTEREVAL
            && cx->blk_eval.cur_text == linestr)
        {
	    cx->blk_eval.cur_text = newSVsv(linestr);
	    cx->blk_u16 |= 0x40; /* indicate cur_text is ref counted */
	}
	/* Copy everything from s onwards back to d. */
	Move(s,d,bufend-s + 1,char);
	SvCUR_set(linestr, SvCUR(linestr) - (s-d));
	/* Setting PL_bufend only applies when we have not dug deeper
	   into other scopes, because sublex_done sets PL_bufend to
	   SvEND(PL_linestr). */
	if (shared == PL_parser->lex_shared) PL_bufend = SvEND(linestr);
	s = olds;
    }
    else
    {
      SV *linestr_save;
      char *oldbufptr_save;
      char *oldoldbufptr_save;
     streaming:
      SvPVCLEAR(tmpstr);   /* avoid "uninitialized" warning */
      term = PL_tokenbuf[1];
      len--;
      linestr_save = PL_linestr; /* must restore this afterwards */
      d = s;			 /* and this */
      oldbufptr_save = PL_oldbufptr;
      oldoldbufptr_save = PL_oldoldbufptr;
      PL_linestr = newSVpvs("");
      PL_bufend = SvPVX(PL_linestr);
      while (1) {
	PL_bufptr = PL_bufend;
	CopLINE_set(PL_curcop,
		    origline + 1 + PL_parser->herelines);
	if (!lex_next_chunk(LEX_NO_TERM)
	 && (!SvCUR(tmpstr) || SvEND(tmpstr)[-1] != '\n')) {
	    /* Simply freeing linestr_save might seem simpler here, as it
	       does not matter what PL_linestr points to, since we are
	       about to croak; but in a quote-like op, linestr_save
	       will have been prospectively freed already, via
	       SAVEFREESV(PL_linestr) in sublex_push, so it’s easier to
	       restore PL_linestr. */
	    SvREFCNT_dec_NN(PL_linestr);
	    PL_linestr = linestr_save;
            PL_oldbufptr = oldbufptr_save;
            PL_oldoldbufptr = oldoldbufptr_save;
	    goto interminable;
	}
	CopLINE_set(PL_curcop, origline);
	if (!SvCUR(PL_linestr) || PL_bufend[-1] != '\n') {
            s = lex_grow_linestr(SvLEN(PL_linestr) + 3);
            /* ^That should be enough to avoid this needing to grow:  */
	    sv_catpvs(PL_linestr, "\n\0");
            assert(s == SvPVX(PL_linestr));
            PL_bufend = SvEND(PL_linestr);
	}
	s = PL_bufptr;
	PL_parser->herelines++;
	PL_last_lop = PL_last_uni = NULL;
#ifndef PERL_STRICT_CR
	if (PL_bufend - PL_linestart >= 2) {
	    if (   (PL_bufend[-2] == '\r' && PL_bufend[-1] == '\n')
                || (PL_bufend[-2] == '\n' && PL_bufend[-1] == '\r'))
	    {
		PL_bufend[-2] = '\n';
		PL_bufend--;
		SvCUR_set(PL_linestr, PL_bufend - SvPVX_const(PL_linestr));
	    }
	    else if (PL_bufend[-1] == '\r')
		PL_bufend[-1] = '\n';
	}
	else if (PL_bufend - PL_linestart == 1 && PL_bufend[-1] == '\r')
	    PL_bufend[-1] = '\n';
#endif
	if (indented && (PL_bufend-s) >= len) {
	    char * found = ninstr(s, PL_bufend, (PL_tokenbuf + 1), (PL_tokenbuf +1 + len));

	    if (found) {
		char *backup = found;
		indent_len = 0;

		/* Only valid if it's preceded by whitespace only */
		while (backup != s && --backup >= s) {
		    if (! SPACE_OR_TAB(*backup)) {
			break;
		    }
		    indent_len++;
		}

		/* All whitespace or none! */
		if (backup == found || SPACE_OR_TAB(*backup)) {
		    Newx(indent, indent_len + 1, char);
		    memcpy(indent, backup, indent_len);
		    indent[indent_len] = 0;
		    SvREFCNT_dec(PL_linestr);
		    PL_linestr = linestr_save;
		    PL_linestart = SvPVX(linestr_save);
		    PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
		    PL_oldbufptr = oldbufptr_save;
		    PL_oldoldbufptr = oldoldbufptr_save;
		    s = d;
		    break;
		}
	    }

	    /* Didn't find it */
	    sv_catsv(tmpstr,PL_linestr);
	} else {
	    if (*s == term && PL_bufend-s >= len
	        && memEQ(s,PL_tokenbuf + 1,len))
	    {
		SvREFCNT_dec(PL_linestr);
		PL_linestr = linestr_save;
		PL_linestart = SvPVX(linestr_save);
		PL_bufend = SvPVX(PL_linestr) + SvCUR(PL_linestr);
		PL_oldbufptr = oldbufptr_save;
		PL_oldoldbufptr = oldoldbufptr_save;
		s = d;
		break;
	    } else {
		sv_catsv(tmpstr,PL_linestr);
	    }
	}
      }
    }
    PL_multi_end = origline + PL_parser->herelines;
    if (indented && indent) {
	STRLEN linecount = 1;
	STRLEN herelen = SvCUR(tmpstr);
	char *ss = SvPVX(tmpstr);
	char *se = ss + herelen;
        SV *newstr = newSV(herelen+1);
        SvPOK_on(newstr);

	/* Trim leading whitespace */
	while (ss < se) {
	    /* newline only? Copy and move on */
	    if (*ss == '\n') {
		sv_catpv(newstr,"\n");
		ss++;
		linecount++;

	    /* Found our indentation? Strip it */
	    } else if (se - ss >= indent_len
	               && memEQ(ss, indent, indent_len))
	    {
		STRLEN le = 0;

		ss += indent_len;

		while ((ss + le) < se && *(ss + le) != '\n')
		    le++;

		sv_catpvn(newstr, ss, le);

		ss += le;

	    /* Line doesn't begin with our indentation? Croak */
	    } else {
		Perl_croak(aTHX_
		    "Indentation on line %d of here-doc doesn't match delimiter",
		    (int)linecount
		);
	    }
	}
        /* avoid sv_setsv() as we dont wan't to COW here */
        sv_setpvn(tmpstr,SvPVX(newstr),SvCUR(newstr));
	Safefree(indent);
	SvREFCNT_dec_NN(newstr);
    }
    if (SvCUR(tmpstr) + 5 < SvLEN(tmpstr)) {
	SvPV_shrink_to_cur(tmpstr);
    }
    if (!IN_BYTES) {
	if (UTF && is_utf8_string((U8*)SvPVX_const(tmpstr), SvCUR(tmpstr)))
	    SvUTF8_on(tmpstr);
    }
    PL_lex_stuff = tmpstr;
    pl_yylval.ival = op_type;
    return s;

  interminable:
    SvREFCNT_dec(tmpstr);
    CopLINE_set(PL_curcop, origline);
    missingterm(PL_tokenbuf + 1, sizeof(PL_tokenbuf) - 1);
}

/* scan_inputsymbol
   takes: position of first '<' in input buffer
   returns: position of first char following the matching '>' in
	    input buffer
   side-effects: pl_yylval and lex_op are set.

   This code handles:

   <>		read from ARGV
   <<>>		read from ARGV without magic open
   <FH> 	read from filehandle
   <pkg::FH>	read from package qualified filehandle
   <pkg'FH>	read from package qualified filehandle
   <$fh>	read from filehandle in $fh
   <*.h>	filename glob

*/

STATIC char *
S_scan_inputsymbol(pTHX_ char *start)
{
    char *s = start;		/* current position in buffer */
    char *end;
    I32 len;
    bool nomagicopen = FALSE;
    char *d = PL_tokenbuf;					/* start of temp holding space */
    const char * const e = PL_tokenbuf + sizeof PL_tokenbuf;	/* end of temp holding space */

    PERL_ARGS_ASSERT_SCAN_INPUTSYMBOL;

    end = (char *) memchr(s, '\n', PL_bufend - s);
    if (!end)
	end = PL_bufend;
    if (s[1] == '<' && s[2] == '>' && s[3] == '>') {
        nomagicopen = TRUE;
        *d = '\0';
        len = 0;
        s += 3;
    }
    else
        s = delimcpy(d, e, s + 1, end, '>', &len);	/* extract until > */

    /* die if we didn't have space for the contents of the <>,
       or if it didn't end, or if we see a newline
    */

    if (len >= (I32)sizeof PL_tokenbuf)
	Perl_croak(aTHX_ "Excessively long <> operator");
    if (s >= end)
	Perl_croak(aTHX_ "Unterminated <> operator");

    s++;

    /* check for <$fh>
       Remember, only scalar variables are interpreted as filehandles by
       this code.  Anything more complex (e.g., <$fh{$num}>) will be
       treated as a glob() call.
       This code makes use of the fact that except for the $ at the front,
       a scalar variable and a filehandle look the same.
    */
    if (*d == '$' && d[1]) d++;

    /* allow <Pkg'VALUE> or <Pkg::VALUE> */
    while (isWORDCHAR_lazy_if_safe(d, e, UTF) || *d == '\'' || *d == ':') {
	d += UTF ? UTF8SKIP(d) : 1;
    }

    /* If we've tried to read what we allow filehandles to look like, and
       there's still text left, then it must be a glob() and not a getline.
       Use scan_str to pull out the stuff between the <> and treat it
       as nothing more than a string.
    */

    if (d - PL_tokenbuf != len) {
	pl_yylval.ival = OP_GLOB;
	s = scan_str(start,FALSE,FALSE,FALSE,NULL);
	if (!s)
	   Perl_croak(aTHX_ "Glob not terminated");
	return s;
    }
    else {
	bool readline_overriden = FALSE;
	GV *gv_readline;
    	/* we're in a filehandle read situation */
	d = PL_tokenbuf;

	/* turn <> into <ARGV> */
	if (!len)
	    Copy("ARGV",d,5,char);

	/* Check whether readline() is overriden */
	if ((gv_readline = gv_override("readline",8)))
	    readline_overriden = TRUE;

	/* if <$fh>, create the ops to turn the variable into a
	   filehandle
	*/
	if (*d == '$') {
	    /* try to find it in the pad for this block, otherwise find
	       add symbol table ops
	    */
	    const PADOFFSET tmp = pad_findmy_pvn(d, len, 0);
	    if (tmp != NOT_IN_PAD) {
		if (PAD_COMPNAME_FLAGS_isOUR(tmp)) {
		    HV * const stash = PAD_COMPNAME_OURSTASH(tmp);
		    HEK * const stashname = HvNAME_HEK(stash);
		    SV * const sym = sv_2mortal(newSVhek(stashname));
		    sv_catpvs(sym, "::");
		    sv_catpv(sym, d+1);
		    d = SvPVX(sym);
		    goto intro_sym;
		}
		else {
		    OP * const o = newOP(OP_PADSV, 0);
		    o->op_targ = tmp;
		    PL_lex_op = readline_overriden
                        ? newUNOP(OP_ENTERSUB, OPf_STACKED,
				op_append_elem(OP_LIST, o,
				    newCVREF(0, newGVOP(OP_GV,0,gv_readline))))
                        : newUNOP(OP_READLINE, 0, o);
		}
	    }
	    else {
		GV *gv;
		++d;
              intro_sym:
		gv = gv_fetchpv(d,
				GV_ADDMULTI | ( UTF ? SVf_UTF8 : 0 ),
				SVt_PV);
		PL_lex_op = readline_overriden
                    ? newUNOP(OP_ENTERSUB, OPf_STACKED,
			    op_append_elem(OP_LIST,
				newUNOP(OP_RV2SV, 0, newGVOP(OP_GV, 0, gv)),
				newCVREF(0, newGVOP(OP_GV, 0, gv_readline))))
                    : newUNOP(OP_READLINE, 0,
			    newUNOP(OP_RV2SV, 0,
				newGVOP(OP_GV, 0, gv)));
	    }
	    /* we created the ops in PL_lex_op, so make pl_yylval.ival a null op */
	    pl_yylval.ival = OP_NULL;
	}

	/* If it's none of the above, it must be a literal filehandle
	   (<Foo::BAR> or <FOO>) so build a simple readline OP */
	else {
	    GV * const gv = gv_fetchpv(d, GV_ADD | ( UTF ? SVf_UTF8 : 0 ), SVt_PVIO);
	    PL_lex_op = readline_overriden
                ? newUNOP(OP_ENTERSUB, OPf_STACKED,
			op_append_elem(OP_LIST,
			    newGVOP(OP_GV, 0, gv),
			    newCVREF(0, newGVOP(OP_GV, 0, gv_readline))))
                : newUNOP(OP_READLINE, nomagicopen ? OPf_SPECIAL : 0, newGVOP(OP_GV, 0, gv));
	    pl_yylval.ival = OP_NULL;
	}
    }

    return s;
}


/* scan_str
   takes:
	start			position in buffer
        keep_bracketed_quoted   preserve \ quoting of embedded delimiters, but
                                only if they are of the open/close form
	keep_delims		preserve the delimiters around the string
	re_reparse		compiling a run-time /(?{})/:
				   collapse // to /,  and skip encoding src
	delimp			if non-null, this is set to the position of
				the closing delimiter, or just after it if
				the closing and opening delimiters differ
				(i.e., the opening delimiter of a substitu-
				tion replacement)
   returns: position to continue reading from buffer
   side-effects: multi_start, multi_close, lex_repl or lex_stuff, and
   	updates the read buffer.

   This subroutine pulls a string out of the input.  It is called for:
   	q		single quotes		q(literal text)
	'		single quotes		'literal text'
	qq		double quotes		qq(interpolate $here please)
	"		double quotes		"interpolate $here please"
	qx		backticks		qx(/bin/ls -l)
	`		backticks		`/bin/ls -l`
	qw		quote words		@EXPORT_OK = qw( func() $spam )
	m//		regexp match		m/this/
	s///		regexp substitute	s/this/that/
	tr///		string transliterate	tr/this/that/
	y///		string transliterate	y/this/that/
	($*@)		sub prototypes		sub foo ($)
	(stuff)		sub attr parameters	sub foo : attr(stuff)
	<>		readline or globs	<FOO>, <>, <$fh>, or <*.c>

   In most of these cases (all but <>, patterns and transliterate)
   yylex() calls scan_str().  m// makes yylex() call scan_pat() which
   calls scan_str().  s/// makes yylex() call scan_subst() which calls
   scan_str().  tr/// and y/// make yylex() call scan_trans() which
   calls scan_str().

   It skips whitespace before the string starts, and treats the first
   character as the delimiter.  If the delimiter is one of ([{< then
   the corresponding "close" character )]}> is used as the closing
   delimiter.  It allows quoting of delimiters, and if the string has
   balanced delimiters ([{<>}]) it allows nesting.

   On success, the SV with the resulting string is put into lex_stuff or,
   if that is already non-NULL, into lex_repl. The second case occurs only
   when parsing the RHS of the special constructs s/// and tr/// (y///).
   For convenience, the terminating delimiter character is stuffed into
   SvIVX of the SV.
*/

STATIC char *
S_scan_str(pTHX_ char *start, int keep_bracketed_quoted, int keep_delims, int re_reparse,
		 char **delimp
    )
{
    SV *sv;			/* scalar value: string */
    const char *tmps;		/* temp string, used for delimiter matching */
    char *s = start;		/* current position in the buffer */
    char term;			/* terminating character */
    char *to;			/* current position in the sv's data */
    I32 brackets = 1;		/* bracket nesting level */
    bool has_utf8 = FALSE;	/* is there any utf8 content? */
    IV termcode;		/* terminating char. code */
    U8 termstr[UTF8_MAXBYTES+1]; /* terminating string */
    STRLEN termlen;		/* length of terminating string */
    line_t herelines;

    /* The delimiters that have a mirror-image closing one */
    const char * opening_delims = "([{<";
    const char * closing_delims = ")]}>";

    const char * non_grapheme_msg = "Use of unassigned code point or"
                                    " non-standalone grapheme for a delimiter"
                                    " will be a fatal error starting in Perl"
                                    " 5.30";
    /* The only non-UTF character that isn't a stand alone grapheme is
     * white-space, hence can't be a delimiter.  So can skip for non-UTF-8 */
    bool check_grapheme = UTF && ckWARN_d(WARN_DEPRECATED);

    PERL_ARGS_ASSERT_SCAN_STR;

    /* skip space before the delimiter */
    if (isSPACE(*s)) {
	s = skipspace(s);
    }

    /* mark where we are, in case we need to report errors */
    CLINE;

    /* after skipping whitespace, the next character is the terminator */
    term = *s;
    if (!UTF || UTF8_IS_INVARIANT(term)) {
	termcode = termstr[0] = term;
	termlen = 1;
    }
    else {
	termcode = utf8_to_uvchr_buf((U8*)s, (U8*)PL_bufend, &termlen);
        if (check_grapheme) {
            if (   UNLIKELY(UNICODE_IS_SUPER(termcode))
                || UNLIKELY(UNICODE_IS_NONCHAR(termcode)))
            {
                /* These are considered graphemes, and since the ending
                 * delimiter will be the same, we don't have to check the other
                 * end */
                check_grapheme = FALSE;
            }
            else if (UNLIKELY(! _is_grapheme((U8 *) start,
                                             (U8 *) s,
                                             (U8 *) PL_bufend,
                                             termcode)))
            {
                Perl_warner(aTHX_ packWARN(WARN_DEPRECATED), "%s", non_grapheme_msg);

                /* Don't have to check the other end, as have already warned at
                 * this one */
                check_grapheme = FALSE;
            }
        }

	Copy(s, termstr, termlen, U8);
    }

    /* mark where we are */
    PL_multi_start = CopLINE(PL_curcop);
    PL_multi_open = termcode;
    herelines = PL_parser->herelines;

    /* If the delimiter has a mirror-image closing one, get it */
    if (term && (tmps = strchr(opening_delims, term))) {
        termcode = termstr[0] = term = closing_delims[tmps - opening_delims];
    }

    PL_multi_close = termcode;

    if (PL_multi_open == PL_multi_close) {
        keep_bracketed_quoted = FALSE;
    }

    /* create a new SV to hold the contents.  79 is the SV's initial length.
       What a random number. */
    sv = newSV_type(SVt_PVIV);
    SvGROW(sv, 80);
    SvIV_set(sv, termcode);
    (void)SvPOK_only(sv);		/* validate pointer */

    /* move past delimiter and try to read a complete string */
    if (keep_delims)
	sv_catpvn(sv, s, termlen);
    s += termlen;
    for (;;) {
    	/* extend sv if need be */
	SvGROW(sv, SvCUR(sv) + (PL_bufend - s) + 1);
	/* set 'to' to the next character in the sv's string */
	to = SvPVX(sv)+SvCUR(sv);

	/* if open delimiter is the close delimiter read unbridle */
	if (PL_multi_open == PL_multi_close) {
	    for (; s < PL_bufend; s++,to++) {
	    	/* embedded newlines increment the current line number */
		if (*s == '\n' && !PL_rsfp && !PL_parser->filtered)
		    COPLINE_INC_WITH_HERELINES;
		/* handle quoted delimiters */
		if (*s == '\\' && s+1 < PL_bufend && term != '\\') {
		    if (!keep_bracketed_quoted
		        && (s[1] == term
			    || (re_reparse && s[1] == '\\'))
		    )
			s++;
		    else /* any other quotes are simply copied straight through */
			*to++ = *s++;
		}
		/* terminate when run out of buffer (the for() condition), or
		   have found the terminator */
		else if (*s == term) {  /* First byte of terminator matches */
		    if (termlen == 1)   /* If is the only byte, are done */
			break;

                    /* If the remainder of the terminator matches, also are
                     * done, after checking that is a separate grapheme */
                    if (   s + termlen <= PL_bufend
                        && memEQ(s + 1, (char*)termstr + 1, termlen - 1))
                    {
                        if (   check_grapheme
                            && UNLIKELY(! _is_grapheme((U8 *) start,
                                                              (U8 *) s,
                                                              (U8 *) PL_bufend,
                                                              termcode)))
                        {
                            Perl_warner(aTHX_ packWARN(WARN_DEPRECATED),
                                        "%s", non_grapheme_msg);
                        }
			break;
                    }
		}
		else if (!has_utf8 && !UTF8_IS_INVARIANT((U8)*s) && UTF) {
		    has_utf8 = TRUE;
                }

		*to = *s;
	    }
	}

	/* if the terminator isn't the same as the start character (e.g.,
	   matched brackets), we have to allow more in the quoting, and
	   be prepared for nested brackets.
	*/
	else {
	    /* read until we run out of string, or we find the terminator */
	    for (; s < PL_bufend; s++,to++) {
	    	/* embedded newlines increment the line count */
		if (*s == '\n' && !PL_rsfp && !PL_parser->filtered)
		    COPLINE_INC_WITH_HERELINES;
		/* backslashes can escape the open or closing characters */
		if (*s == '\\' && s+1 < PL_bufend) {
		    if (!keep_bracketed_quoted
                       && ( ((UV)s[1] == PL_multi_open)
                         || ((UV)s[1] == PL_multi_close) ))
                    {
			s++;
                    }
		    else
			*to++ = *s++;
                }
		/* allow nested opens and closes */
		else if ((UV)*s == PL_multi_close && --brackets <= 0)
		    break;
		else if ((UV)*s == PL_multi_open)
		    brackets++;
		else if (!has_utf8 && !UTF8_IS_INVARIANT((U8)*s) && UTF)
		    has_utf8 = TRUE;
		*to = *s;
	    }
	}
	/* terminate the copied string and update the sv's end-of-string */
	*to = '\0';
	SvCUR_set(sv, to - SvPVX_const(sv));

	/*
	 * this next chunk reads more into the buffer if we're not done yet
	 */

  	if (s < PL_bufend)
	    break;		/* handle case where we are done yet :-) */

#ifndef PERL_STRICT_CR
	if (to - SvPVX_const(sv) >= 2) {
	    if (   (to[-2] == '\r' && to[-1] == '\n')
                || (to[-2] == '\n' && to[-1] == '\r'))
	    {
		to[-2] = '\n';
		to--;
		SvCUR_set(sv, to - SvPVX_const(sv));
	    }
	    else if (to[-1] == '\r')
		to[-1] = '\n';
	}
	else if (to - SvPVX_const(sv) == 1 && to[-1] == '\r')
	    to[-1] = '\n';
#endif

	/* if we're out of file, or a read fails, bail and reset the current
	   line marker so we can report where the unterminated string began
	*/
	COPLINE_INC_WITH_HERELINES;
	PL_bufptr = PL_bufend;
	if (!lex_next_chunk(0)) {
	    sv_free(sv);
	    CopLINE_set(PL_curcop, (line_t)PL_multi_start);
	    return NULL;
	}
	s = start = PL_bufptr;
    }

    /* at this point, we have successfully read the delimited string */

    if (keep_delims)
	    sv_catpvn(sv, s, termlen);
    s += termlen;

    if (has_utf8)
	SvUTF8_on(sv);

    PL_multi_end = CopLINE(PL_curcop);
    CopLINE_set(PL_curcop, PL_multi_start);
    PL_parser->herelines = herelines;

    /* if we allocated too much space, give some back */
    if (SvCUR(sv) + 5 < SvLEN(sv)) {
	SvLEN_set(sv, SvCUR(sv) + 1);
	SvPV_renew(sv, SvLEN(sv));
    }

    /* decide whether this is the first or second quoted string we've read
       for this op
    */

    if (PL_lex_stuff)
	PL_parser->lex_sub_repl = sv;
    else
	PL_lex_stuff = sv;
    if (delimp) *delimp = PL_multi_open == PL_multi_close ? s-termlen : s;
    return s;
}

/*
  scan_num
  takes: pointer to position in buffer
  returns: pointer to new position in buffer
  side-effects: builds ops for the constant in pl_yylval.op

  Read a number in any of the formats that Perl accepts:

  \d(_?\d)*(\.(\d(_?\d)*)?)?[Ee][\+\-]?(\d(_?\d)*)	12 12.34 12.
  \.\d(_?\d)*[Ee][\+\-]?(\d(_?\d)*)			.34
  0b[01](_?[01])*                                       binary integers
  0[0-7](_?[0-7])*                                      octal integers
  0x[0-9A-Fa-f](_?[0-9A-Fa-f])*                         hexadecimal integers
  0x[0-9A-Fa-f](_?[0-9A-Fa-f])*(?:\.\d*)?p[+-]?[0-9]+   hexadecimal floats

  Like most scan_ routines, it uses the PL_tokenbuf buffer to hold the
  thing it reads.

  If it reads a number without a decimal point or an exponent, it will
  try converting the number to an integer and see if it can do so
  without loss of precision.
*/

char *
Perl_scan_num(pTHX_ const char *start, YYSTYPE* lvalp)
{
    const char *s = start;	/* current position in buffer */
    char *d;			/* destination in temp buffer */
    char *e;			/* end of temp buffer */
    NV nv;				/* number read, as a double */
    SV *sv = NULL;			/* place to put the converted number */
    bool floatit;			/* boolean: int or float? */
    const char *lastub = NULL;		/* position of last underbar */
    static const char* const number_too_long = "Number too long";
    bool warned_about_underscore = 0;
#define WARN_ABOUT_UNDERSCORE() \
	do { \
	    if (!warned_about_underscore) { \
		warned_about_underscore = 1; \
		Perl_ck_warner(aTHX_ packWARN(WARN_SYNTAX), \
			       "Misplaced _ in number"); \
	    } \
	} while(0)
    /* Hexadecimal floating point.
     *
     * In many places (where we have quads and NV is IEEE 754 double)
     * we can fit the mantissa bits of a NV into an unsigned quad.
     * (Note that UVs might not be quads even when we have quads.)
     * This will not work everywhere, though (either no quads, or
     * using long doubles), in which case we have to resort to NV,
     * which will probably mean horrible loss of precision due to
     * multiple fp operations. */
    bool hexfp = FALSE;
    int total_bits = 0;
    int significant_bits = 0;
#if NVSIZE == 8 && defined(HAS_QUAD) && defined(Uquad_t)
#  define HEXFP_UQUAD
    Uquad_t hexfp_uquad = 0;
    int hexfp_frac_bits = 0;
#else
#  define HEXFP_NV
    NV hexfp_nv = 0.0;
#endif
    NV hexfp_mult = 1.0;
    UV high_non_zero = 0; /* highest digit */
    int non_zero_integer_digits = 0;

    PERL_ARGS_ASSERT_SCAN_NUM;

    /* We use the first character to decide what type of number this is */

    switch (*s) {
    default:
	Perl_croak(aTHX_ "panic: scan_num, *s=%d", *s);

    /* if it starts with a 0, it could be an octal number, a decimal in
       0.13 disguise, or a hexadecimal number, or a binary number. */
    case '0':
	{
	  /* variables:
	     u		holds the "number so far"
	     shift	the power of 2 of the base
			(hex == 4, octal == 3, binary == 1)
	     overflowed	was the number more than we can hold?

	     Shift is used when we add a digit.  It also serves as an "are
	     we in octal/hex/binary?" indicator to disallow hex characters
	     when in octal mode.
	   */
	    NV n = 0.0;
	    UV u = 0;
	    I32 shift;
	    bool overflowed = FALSE;
	    bool just_zero  = TRUE;	/* just plain 0 or binary number? */
	    static const NV nvshift[5] = { 1.0, 2.0, 4.0, 8.0, 16.0 };
	    static const char* const bases[5] =
	      { "", "binary", "", "octal", "hexadecimal" };
	    static const char* const Bases[5] =
	      { "", "Binary", "", "Octal", "Hexadecimal" };
	    static const char* const maxima[5] =
	      { "",
		"0b11111111111111111111111111111111",
		"",
		"037777777777",
		"0xffffffff" };
	    const char *base, *Base, *max;

	    /* check for hex */
	    if (isALPHA_FOLD_EQ(s[1], 'x')) {
		shift = 4;
		s += 2;
		just_zero = FALSE;
	    } else if (isALPHA_FOLD_EQ(s[1], 'b')) {
		shift = 1;
		s += 2;
		just_zero = FALSE;
	    }
	    /* check for a decimal in disguise */
	    else if (s[1] == '.' || isALPHA_FOLD_EQ(s[1], 'e'))
		goto decimal;
	    /* so it must be octal */
	    else {
		shift = 3;
		s++;
	    }

	    if (*s == '_') {
		WARN_ABOUT_UNDERSCORE();
	       lastub = s++;
	    }

	    base = bases[shift];
	    Base = Bases[shift];
	    max  = maxima[shift];

	    /* read the rest of the number */
	    for (;;) {
		/* x is used in the overflow test,
		   b is the digit we're adding on. */
		UV x, b;

		switch (*s) {

		/* if we don't mention it, we're done */
		default:
		    goto out;

		/* _ are ignored -- but warned about if consecutive */
		case '_':
		    if (lastub && s == lastub + 1)
			WARN_ABOUT_UNDERSCORE();
		    lastub = s++;
		    break;

		/* 8 and 9 are not octal */
		case '8': case '9':
		    if (shift == 3)
			yyerror(Perl_form(aTHX_ "Illegal octal digit '%c'", *s));
		    /* FALLTHROUGH */

	        /* octal digits */
		case '2': case '3': case '4':
		case '5': case '6': case '7':
		    if (shift == 1)
			yyerror(Perl_form(aTHX_ "Illegal binary digit '%c'", *s));
		    /* FALLTHROUGH */

		case '0': case '1':
		    b = *s++ & 15;		/* ASCII digit -> value of digit */
		    goto digit;

	        /* hex digits */
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		    /* make sure they said 0x */
		    if (shift != 4)
			goto out;
		    b = (*s++ & 7) + 9;

		    /* Prepare to put the digit we have onto the end
		       of the number so far.  We check for overflows.
		    */

		  digit:
		    just_zero = FALSE;
		    if (!overflowed) {
			assert(shift >= 0);
			x = u << shift;	/* make room for the digit */

                        total_bits += shift;

			if ((x >> shift) != u
			    && !(PL_hints & HINT_NEW_BINARY)) {
			    overflowed = TRUE;
			    n = (NV) u;
			    Perl_ck_warner_d(aTHX_ packWARN(WARN_OVERFLOW),
					     "Integer overflow in %s number",
					     base);
			} else
			    u = x | b;		/* add the digit to the end */
		    }
		    if (overflowed) {
			n *= nvshift[shift];
			/* If an NV has not enough bits in its
			 * mantissa to represent an UV this summing of
			 * small low-order numbers is a waste of time
			 * (because the NV cannot preserve the
			 * low-order bits anyway): we could just
			 * remember when did we overflow and in the
			 * end just multiply n by the right
			 * amount. */
			n += (NV) b;
		    }

                    if (high_non_zero == 0 && b > 0)
                        high_non_zero = b;

                    if (high_non_zero)
                        non_zero_integer_digits++;

                    /* this could be hexfp, but peek ahead
                     * to avoid matching ".." */
                    if (UNLIKELY(HEXFP_PEEK(s))) {
                        goto out;
                    }

		    break;
		}
	    }

	  /* if we get here, we had success: make a scalar value from
	     the number.
	  */
	  out:

	    /* final misplaced underbar check */
	    if (s[-1] == '_')
		WARN_ABOUT_UNDERSCORE();

            if (UNLIKELY(HEXFP_PEEK(s))) {
                /* Do sloppy (on the underbars) but quick detection
                 * (and value construction) for hexfp, the decimal
                 * detection will shortly be more thorough with the
                 * underbar checks. */
                const char* h = s;
                significant_bits = non_zero_integer_digits * shift;
#ifdef HEXFP_UQUAD
                hexfp_uquad = u;
#else /* HEXFP_NV */
                hexfp_nv = u;
#endif
                /* Ignore the leading zero bits of
                 * the high (first) non-zero digit. */
                if (high_non_zero) {
                    if (high_non_zero < 0x8)
                        significant_bits--;
                    if (high_non_zero < 0x4)
                        significant_bits--;
                    if (high_non_zero < 0x2)
                        significant_bits--;
                }

                if (*h == '.') {
#ifdef HEXFP_NV
                    NV nv_mult = 1.0;
#endif
                    bool accumulate = TRUE;
                    U8 b;
                    int lim = 1 << shift;
                    for (h++; ((isXDIGIT(*h) && (b = XDIGIT_VALUE(*h)) < lim) ||
                               *h == '_'); h++) {
                        if (isXDIGIT(*h)) {
                            significant_bits += shift;
#ifdef HEXFP_UQUAD
                            if (accumulate) {
                                if (significant_bits < NV_MANT_DIG) {
                                    /* We are in the long "run" of xdigits,
                                     * accumulate the full four bits. */
				    assert(shift >= 0);
                                    hexfp_uquad <<= shift;
                                    hexfp_uquad |= b;
                                    hexfp_frac_bits += shift;
                                } else if (significant_bits - shift < NV_MANT_DIG) {
                                    /* We are at a hexdigit either at,
                                     * or straddling, the edge of mantissa.
                                     * We will try grabbing as many as
                                     * possible bits. */
                                    int tail =
                                      significant_bits - NV_MANT_DIG;
                                    if (tail <= 0)
                                       tail += shift;
				    assert(tail >= 0);
                                    hexfp_uquad <<= tail;
				    assert((shift - tail) >= 0);
                                    hexfp_uquad |= b >> (shift - tail);
                                    hexfp_frac_bits += tail;

                                    /* Ignore the trailing zero bits
                                     * of the last non-zero xdigit.
                                     *
                                     * The assumption here is that if
                                     * one has input of e.g. the xdigit
                                     * eight (0x8), there is only one
                                     * bit being input, not the full
                                     * four bits.  Conversely, if one
                                     * specifies a zero xdigit, the
                                     * assumption is that one really
                                     * wants all those bits to be zero. */
                                    if (b) {
                                        if ((b & 0x1) == 0x0) {
                                            significant_bits--;
                                            if ((b & 0x2) == 0x0) {
                                                significant_bits--;
                                                if ((b & 0x4) == 0x0) {
                                                    significant_bits--;
                                                }
                                            }
                                        }
                                    }

                                    accumulate = FALSE;
                                }
                            } else {
                                /* Keep skipping the xdigits, and
                                 * accumulating the significant bits,
                                 * but do not shift the uquad
                                 * (which would catastrophically drop
                                 * high-order bits) or accumulate the
                                 * xdigits anymore. */
                            }
#else /* HEXFP_NV */
                            if (accumulate) {
                                nv_mult /= nvshift[shift];
                                if (nv_mult > 0.0)
                                    hexfp_nv += b * nv_mult;
                                else
                                    accumulate = FALSE;
                            }
#endif
                        }
                        if (significant_bits >= NV_MANT_DIG)
                            accumulate = FALSE;
                    }
                }

                if ((total_bits > 0 || significant_bits > 0) &&
                    isALPHA_FOLD_EQ(*h, 'p')) {
                    bool negexp = FALSE;
                    h++;
                    if (*h == '+')
                        h++;
                    else if (*h == '-') {
                        negexp = TRUE;
                        h++;
                    }
                    if (isDIGIT(*h)) {
                        I32 hexfp_exp = 0;
                        while (isDIGIT(*h) || *h == '_') {
                            if (isDIGIT(*h)) {
                                hexfp_exp *= 10;
                                hexfp_exp += *h - '0';
#ifdef NV_MIN_EXP
                                if (negexp
                                    && -hexfp_exp < NV_MIN_EXP - 1) {
                                    /* NOTE: this means that the exponent
                                     * underflow warning happens for
                                     * the IEEE 754 subnormals (denormals),
                                     * because DBL_MIN_EXP etc are the lowest
                                     * possible binary (or, rather, DBL_RADIX-base)
                                     * exponent for normals, not subnormals.
                                     *
                                     * This may or may not be a good thing. */
                                    Perl_ck_warner(aTHX_ packWARN(WARN_OVERFLOW),
                                                   "Hexadecimal float: exponent underflow");
                                    break;
                                }
#endif
#ifdef NV_MAX_EXP
                                if (!negexp
                                    && hexfp_exp > NV_MAX_EXP - 1) {
                                    Perl_ck_warner(aTHX_ packWARN(WARN_OVERFLOW),
                                                   "Hexadecimal float: exponent overflow");
                                    break;
                                }
#endif
                            }
                            h++;
                        }
                        if (negexp)
                            hexfp_exp = -hexfp_exp;
#ifdef HEXFP_UQUAD
                        hexfp_exp -= hexfp_frac_bits;
#endif
                        hexfp_mult = Perl_pow(2.0, hexfp_exp);
                        hexfp = TRUE;
                        goto decimal;
                    }
                }
            }

	    if (overflowed) {
		if (n > 4294967295.0)
		    Perl_ck_warner(aTHX_ packWARN(WARN_PORTABLE),
				   "%s number > %s non-portable",
				   Base, max);
		sv = newSVnv(n);
	    }
	    else {
#if UVSIZE > 4
		if (u > 0xffffffff)
		    Perl_ck_warner(aTHX_ packWARN(WARN_PORTABLE),
				   "%s number > %s non-portable",
				   Base, max);
#endif
		sv = newSVuv(u);
	    }
	    if (just_zero && (PL_hints & HINT_NEW_INTEGER))
		sv = new_constant(start, s - start, "integer",
				  sv, NULL, NULL, 0);
	    else if (PL_hints & HINT_NEW_BINARY)
		sv = new_constant(start, s - start, "binary", sv, NULL, NULL, 0);
	}
	break;

    /*
      handle decimal numbers.
      we're also sent here when we read a 0 as the first digit
    */
    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9': case '.':
      decimal:
	d = PL_tokenbuf;
	e = PL_tokenbuf + sizeof PL_tokenbuf - 6; /* room for various punctuation */
        floatit = FALSE;
        if (hexfp) {
            floatit = TRUE;
            *d++ = '0';
            *d++ = 'x';
            s = start + 2;
        }

	/* read next group of digits and _ and copy into d */
	while (isDIGIT(*s)
               || *s == '_'
               || UNLIKELY(hexfp && isXDIGIT(*s)))
        {
	    /* skip underscores, checking for misplaced ones
	       if -w is on
	    */
	    if (*s == '_') {
		if (lastub && s == lastub + 1)
		    WARN_ABOUT_UNDERSCORE();
		lastub = s++;
	    }
	    else {
	        /* check for end of fixed-length buffer */
		if (d >= e)
		    Perl_croak(aTHX_ "%s", number_too_long);
		/* if we're ok, copy the character */
		*d++ = *s++;
	    }
	}

	/* final misplaced underbar check */
	if (lastub && s == lastub + 1)
	    WARN_ABOUT_UNDERSCORE();

	/* read a decimal portion if there is one.  avoid
	   3..5 being interpreted as the number 3. followed
	   by .5
	*/
	if (*s == '.' && s[1] != '.') {
	    floatit = TRUE;
	    *d++ = *s++;

	    if (*s == '_') {
		WARN_ABOUT_UNDERSCORE();
		lastub = s;
	    }

	    /* copy, ignoring underbars, until we run out of digits.
	    */
	    for (; isDIGIT(*s)
                   || *s == '_'
                   || UNLIKELY(hexfp && isXDIGIT(*s));
                 s++)
            {
	        /* fixed length buffer check */
		if (d >= e)
		    Perl_croak(aTHX_ "%s", number_too_long);
		if (*s == '_') {
		   if (lastub && s == lastub + 1)
			WARN_ABOUT_UNDERSCORE();
		   lastub = s;
		}
		else
		    *d++ = *s;
	    }
	    /* fractional part ending in underbar? */
	    if (s[-1] == '_')
		WARN_ABOUT_UNDERSCORE();
	    if (*s == '.' && isDIGIT(s[1])) {
		/* oops, it's really a v-string, but without the "v" */
		s = start;
		goto vstring;
	    }
	}

	/* read exponent part, if present */
	if ((isALPHA_FOLD_EQ(*s, 'e')
              || UNLIKELY(hexfp && isALPHA_FOLD_EQ(*s, 'p')))
            && strchr("+-0123456789_", s[1]))
        {
            int exp_digits = 0;
            const char *save_s = s;
            char * save_d = d;

            /* regardless of whether user said 3E5 or 3e5, use lower 'e',
               ditto for p (hexfloats) */
            if ((isALPHA_FOLD_EQ(*s, 'e'))) {
		/* At least some Mach atof()s don't grok 'E' */
                *d++ = 'e';
            }
            else if (UNLIKELY(hexfp && (isALPHA_FOLD_EQ(*s, 'p')))) {
                *d++ = 'p';
            }

	    s++;


	    /* stray preinitial _ */
	    if (*s == '_') {
		WARN_ABOUT_UNDERSCORE();
	        lastub = s++;
	    }

	    /* allow positive or negative exponent */
	    if (*s == '+' || *s == '-')
		*d++ = *s++;

	    /* stray initial _ */
	    if (*s == '_') {
		WARN_ABOUT_UNDERSCORE();
	        lastub = s++;
	    }

	    /* read digits of exponent */
	    while (isDIGIT(*s) || *s == '_') {
	        if (isDIGIT(*s)) {
                    ++exp_digits;
		    if (d >= e)
		        Perl_croak(aTHX_ "%s", number_too_long);
		    *d++ = *s++;
		}
		else {
		   if (((lastub && s == lastub + 1)
                        || (!isDIGIT(s[1]) && s[1] != '_')))
			WARN_ABOUT_UNDERSCORE();
		   lastub = s++;
		}
	    }

            if (!exp_digits) {
                /* no exponent digits, the [eEpP] could be for something else,
                 * though in practice we don't get here for p since that's preparsed
                 * earlier, and results in only the 0xX being consumed, so behave similarly
                 * for decimal floats and consume only the D.DD, leaving the [eE] to the
                 * next token.
                 */
                s = save_s;
                d = save_d;
            }
            else {
                floatit = TRUE;
            }
	}


	/*
           We try to do an integer conversion first if no characters
           indicating "float" have been found.
	 */

	if (!floatit) {
    	    UV uv;
	    const int flags = grok_number (PL_tokenbuf, d - PL_tokenbuf, &uv);

            if (flags == IS_NUMBER_IN_UV) {
              if (uv <= IV_MAX)
		sv = newSViv(uv); /* Prefer IVs over UVs. */
              else
	    	sv = newSVuv(uv);
            } else if (flags == (IS_NUMBER_IN_UV | IS_NUMBER_NEG)) {
              if (uv <= (UV) IV_MIN)
                sv = newSViv(-(IV)uv);
              else
	    	floatit = TRUE;
            } else
              floatit = TRUE;
        }
	if (floatit) {
	    /* terminate the string */
	    *d = '\0';
            if (UNLIKELY(hexfp)) {
#  ifdef NV_MANT_DIG
                if (significant_bits > NV_MANT_DIG)
                    Perl_ck_warner(aTHX_ packWARN(WARN_OVERFLOW),
                                   "Hexadecimal float: mantissa overflow");
#  endif
#ifdef HEXFP_UQUAD
                nv = hexfp_uquad * hexfp_mult;
#else /* HEXFP_NV */
                nv = hexfp_nv * hexfp_mult;
#endif
            } else {
                nv = Atof(PL_tokenbuf);
            }
            sv = newSVnv(nv);
	}

	if ( floatit
	     ? (PL_hints & HINT_NEW_FLOAT) : (PL_hints & HINT_NEW_INTEGER) ) {
	    const char *const key = floatit ? "float" : "integer";
	    const STRLEN keylen = floatit ? 5 : 7;
	    sv = S_new_constant(aTHX_ PL_tokenbuf, d - PL_tokenbuf,
				key, keylen, sv, NULL, NULL, 0);
	}
	break;

    /* if it starts with a v, it could be a v-string */
    case 'v':
    vstring:
		sv = newSV(5); /* preallocate storage space */
		ENTER_with_name("scan_vstring");
		SAVEFREESV(sv);
		s = scan_vstring(s, PL_bufend, sv);
		SvREFCNT_inc_simple_void_NN(sv);
		LEAVE_with_name("scan_vstring");
	break;
    }

    /* make the op for the constant and return */

    if (sv)
	lvalp->opval = newSVOP(OP_CONST, 0, sv);
    else
	lvalp->opval = NULL;

    return (char *)s;
}

STATIC char *
S_scan_formline(pTHX_ char *s)
{
    SV * const stuff = newSVpvs("");
    bool needargs = FALSE;
    bool eofmt = FALSE;

    PERL_ARGS_ASSERT_SCAN_FORMLINE;

    while (!needargs) {
        char *eol;
	if (*s == '.') {
            char *t = s+1;
#ifdef PERL_STRICT_CR
	    while (SPACE_OR_TAB(*t))
		t++;
#else
	    while (SPACE_OR_TAB(*t) || *t == '\r')
		t++;
#endif
	    if (*t == '\n' || t == PL_bufend) {
	        eofmt = TRUE;
		break;
            }
	}
	eol = (char *) memchr(s,'\n',PL_bufend-s);
	if (!eol++)
		eol = PL_bufend;
	if (*s != '#') {
            char *t;
	    for (t = s; t < eol; t++) {
		if (*t == '~' && t[1] == '~' && SvCUR(stuff)) {
		    needargs = FALSE;
		    goto enough;	/* ~~ must be first line in formline */
		}
		if (*t == '@' || *t == '^')
		    needargs = TRUE;
	    }
	    if (eol > s) {
	        sv_catpvn(stuff, s, eol-s);
#ifndef PERL_STRICT_CR
		if (eol-s > 1 && eol[-2] == '\r' && eol[-1] == '\n') {
		    char *end = SvPVX(stuff) + SvCUR(stuff);
		    end[-2] = '\n';
		    end[-1] = '\0';
		    SvCUR_set(stuff, SvCUR(stuff) - 1);
		}
#endif
	    }
	    else
	      break;
	}
	s = (char*)eol;
	if ((PL_rsfp || PL_parser->filtered)
	 && PL_parser->form_lex_state == LEX_NORMAL) {
	    bool got_some;
	    PL_bufptr = PL_bufend;
	    COPLINE_INC_WITH_HERELINES;
	    got_some = lex_next_chunk(0);
	    CopLINE_dec(PL_curcop);
	    s = PL_bufptr;
	    if (!got_some)
		break;
	}
	incline(s, PL_bufend);
    }
  enough:
    if (!SvCUR(stuff) || needargs)
	PL_lex_state = PL_parser->form_lex_state;
    if (SvCUR(stuff)) {
	PL_expect = XSTATE;
	if (needargs) {
	    const char *s2 = s;
	    while (isSPACE(*s2) && *s2 != '\n')
		s2++;
	    if (*s2 == '{') {
		PL_expect = XTERMBLOCK;
		NEXTVAL_NEXTTOKE.ival = 0;
		force_next(DO);
	    }
	    NEXTVAL_NEXTTOKE.ival = 0;
	    force_next(FORMLBRACK);
	}
	if (!IN_BYTES) {
	    if (UTF && is_utf8_string((U8*)SvPVX_const(stuff), SvCUR(stuff)))
		SvUTF8_on(stuff);
	}
        NEXTVAL_NEXTTOKE.opval = newSVOP(OP_CONST, 0, stuff);
	force_next(THING);
    }
    else {
	SvREFCNT_dec(stuff);
	if (eofmt)
	    PL_lex_formbrack = 0;
    }
    return s;
}

I32
Perl_start_subparse(pTHX_ I32 is_format, U32 flags)
{
    const I32 oldsavestack_ix = PL_savestack_ix;
    CV* const outsidecv = PL_compcv;

    SAVEI32(PL_subline);
    save_item(PL_subname);
    SAVESPTR(PL_compcv);

    PL_compcv = MUTABLE_CV(newSV_type(is_format ? SVt_PVFM : SVt_PVCV));
    CvFLAGS(PL_compcv) |= flags;

    PL_subline = CopLINE(PL_curcop);
    CvPADLIST(PL_compcv) = pad_new(padnew_SAVE|padnew_SAVESUB);
    CvOUTSIDE(PL_compcv) = MUTABLE_CV(SvREFCNT_inc_simple(outsidecv));
    CvOUTSIDE_SEQ(PL_compcv) = PL_cop_seqmax;
    if (outsidecv && CvPADLIST(outsidecv))
	CvPADLIST(PL_compcv)->xpadl_outid = CvPADLIST(outsidecv)->xpadl_id;

    return oldsavestack_ix;
}


/* Do extra initialisation of a CV (typically one just created by
 * start_subparse()) if that CV is for a named sub
 */

void
Perl_init_named_cv(pTHX_ CV *cv, OP *nameop)
{
    PERL_ARGS_ASSERT_INIT_NAMED_CV;

    if (nameop->op_type == OP_CONST) {
        const char *const name = SvPV_nolen_const(((SVOP*)nameop)->op_sv);
        if (   strEQ(name, "BEGIN")
            || strEQ(name, "END")
            || strEQ(name, "INIT")
            || strEQ(name, "CHECK")
            || strEQ(name, "UNITCHECK")
        )
          CvSPECIAL_on(cv);
    }
    else
    /* State subs inside anonymous subs need to be
     clonable themselves. */
    if (   CvANON(CvOUTSIDE(cv))
        || CvCLONE(CvOUTSIDE(cv))
        || !PadnameIsSTATE(PadlistNAMESARRAY(CvPADLIST(
                        CvOUTSIDE(cv)
                     ))[nameop->op_targ])
    )
      CvCLONE_on(cv);
}


static int
S_yywarn(pTHX_ const char *const s, U32 flags)
{
    PERL_ARGS_ASSERT_YYWARN;

    PL_in_eval |= EVAL_WARNONLY;
    yyerror_pv(s, flags);
    return 0;
}

void
Perl_abort_execution(pTHX_ const char * const msg, const char * const name)
{
    PERL_ARGS_ASSERT_ABORT_EXECUTION;

    if (PL_minus_c)
        Perl_croak(aTHX_ "%s%s had compilation errors.\n", msg, name);
    else {
        Perl_croak(aTHX_
                "%sExecution of %s aborted due to compilation errors.\n", msg, name);
    }
    NOT_REACHED; /* NOTREACHED */
}

void
Perl_yyquit(pTHX)
{
    /* Called, after at least one error has been found, to abort the parse now,
     * instead of trying to forge ahead */

    yyerror_pvn(NULL, 0, 0);
}

int
Perl_yyerror(pTHX_ const char *const s)
{
    PERL_ARGS_ASSERT_YYERROR;
    return yyerror_pvn(s, strlen(s), 0);
}

int
Perl_yyerror_pv(pTHX_ const char *const s, U32 flags)
{
    PERL_ARGS_ASSERT_YYERROR_PV;
    return yyerror_pvn(s, strlen(s), flags);
}

int
Perl_yyerror_pvn(pTHX_ const char *const s, STRLEN len, U32 flags)
{
    const char *context = NULL;
    int contlen = -1;
    SV *msg;
    SV * const where_sv = newSVpvs_flags("", SVs_TEMP);
    int yychar  = PL_parser->yychar;

    /* Output error message 's' with length 'len'.  'flags' are SV flags that
     * apply.  If the number of errors found is large enough, it abandons
     * parsing.  If 's' is NULL, there is no message, and it abandons
     * processing unconditionally */

    if (s != NULL) {
        if (!yychar || (yychar == ';' && !PL_rsfp))
            sv_catpvs(where_sv, "at EOF");
        else if (   PL_oldoldbufptr
                 && PL_bufptr > PL_oldoldbufptr
                 && PL_bufptr - PL_oldoldbufptr < 200
                 && PL_oldoldbufptr != PL_oldbufptr
                 && PL_oldbufptr != PL_bufptr)
        {
            /*
                    Only for NetWare:
                    The code below is removed for NetWare because it
                    abends/crashes on NetWare when the script has error such as
                    not having the closing quotes like:
                        if ($var eq "value)
                    Checking of white spaces is anyway done in NetWare code.
            */
#ifndef NETWARE
            while (isSPACE(*PL_oldoldbufptr))
                PL_oldoldbufptr++;
#endif
            context = PL_oldoldbufptr;
            contlen = PL_bufptr - PL_oldoldbufptr;
        }
        else if (  PL_oldbufptr
                && PL_bufptr > PL_oldbufptr
                && PL_bufptr - PL_oldbufptr < 200
                && PL_oldbufptr != PL_bufptr) {
            /*
                    Only for NetWare:
                    The code below is removed for NetWare because it
                    abends/crashes on NetWare when the script has error such as
                    not having the closing quotes like:
                        if ($var eq "value)
                    Checking of white spaces is anyway done in NetWare code.
            */
#ifndef NETWARE
            while (isSPACE(*PL_oldbufptr))
                PL_oldbufptr++;
#endif
            context = PL_oldbufptr;
            contlen = PL_bufptr - PL_oldbufptr;
        }
        else if (yychar > 255)
            sv_catpvs(where_sv, "next token ???");
        else if (yychar == YYEMPTY) {
            if (PL_lex_state == LEX_NORMAL)
                sv_catpvs(where_sv, "at end of line");
            else if (PL_lex_inpat)
                sv_catpvs(where_sv, "within pattern");
            else
                sv_catpvs(where_sv, "within string");
        }
        else {
            sv_catpvs(where_sv, "next char ");
            if (yychar < 32)
                Perl_sv_catpvf(aTHX_ where_sv, "^%c", toCTRL(yychar));
            else if (isPRINT_LC(yychar)) {
                const char string = yychar;
                sv_catpvn(where_sv, &string, 1);
            }
            else
                Perl_sv_catpvf(aTHX_ where_sv, "\\%03o", yychar & 255);
        }
        msg = newSVpvn_flags(s, len, (flags & SVf_UTF8) | SVs_TEMP);
        Perl_sv_catpvf(aTHX_ msg, " at %s line %" IVdf ", ",
            OutCopFILE(PL_curcop),
            (IV)(PL_parser->preambling == NOLINE
                   ? CopLINE(PL_curcop)
                   : PL_parser->preambling));
        if (context)
            Perl_sv_catpvf(aTHX_ msg, "near \"%" UTF8f "\"\n",
                                 UTF8fARG(UTF, contlen, context));
        else
            Perl_sv_catpvf(aTHX_ msg, "%" SVf "\n", SVfARG(where_sv));
        if (   PL_multi_start < PL_multi_end
            && (U32)(CopLINE(PL_curcop) - PL_multi_end) <= 1)
        {
            Perl_sv_catpvf(aTHX_ msg,
            "  (Might be a runaway multi-line %c%c string starting on"
            " line %" IVdf ")\n",
                    (int)PL_multi_open,(int)PL_multi_close,(IV)PL_multi_start);
            PL_multi_end = 0;
        }
        if (PL_in_eval & EVAL_WARNONLY) {
            PL_in_eval &= ~EVAL_WARNONLY;
            Perl_ck_warner_d(aTHX_ packWARN(WARN_SYNTAX), "%" SVf, SVfARG(msg));
        }
        else {
            qerror(msg);
        }
    }
    if (s == NULL || PL_error_count >= 10) {
        const char * msg = "";
        const char * const name = OutCopFILE(PL_curcop);

	if (PL_in_eval) {
            SV * errsv = ERRSV;
            if (SvCUR(errsv)) {
                msg = Perl_form(aTHX_ "%" SVf, SVfARG(errsv));
            }
        }

        if (s == NULL) {
            abort_execution(msg, name);
        }
        else {
            Perl_croak(aTHX_ "%s%s has too many errors.\n", msg, name);
        }
    }
    PL_in_my = 0;
    PL_in_my_stash = NULL;
    return 0;
}

STATIC char*
S_swallow_bom(pTHX_ U8 *s)
{
    const STRLEN slen = SvCUR(PL_linestr);

    PERL_ARGS_ASSERT_SWALLOW_BOM;

    switch (s[0]) {
    case 0xFF:
	if (s[1] == 0xFE) {
	    /* UTF-16 little-endian? (or UTF-32LE?) */
	    if (s[2] == 0 && s[3] == 0)  /* UTF-32 little-endian */
		/* diag_listed_as: Unsupported script encoding %s */
		Perl_croak(aTHX_ "Unsupported script encoding UTF-32LE");
#ifndef PERL_NO_UTF16_FILTER
#ifdef DEBUGGING
	    if (DEBUG_p_TEST || DEBUG_T_TEST) PerlIO_printf(Perl_debug_log, "UTF-16LE script encoding (BOM)\n");
#endif
	    s += 2;
	    if (PL_bufend > (char*)s) {
		s = add_utf16_textfilter(s, TRUE);
	    }
#else
	    /* diag_listed_as: Unsupported script encoding %s */
	    Perl_croak(aTHX_ "Unsupported script encoding UTF-16LE");
#endif
	}
	break;
    case 0xFE:
	if (s[1] == 0xFF) {   /* UTF-16 big-endian? */
#ifndef PERL_NO_UTF16_FILTER
#ifdef DEBUGGING
	    if (DEBUG_p_TEST || DEBUG_T_TEST) PerlIO_printf(Perl_debug_log, "UTF-16BE script encoding (BOM)\n");
#endif
	    s += 2;
	    if (PL_bufend > (char *)s) {
		s = add_utf16_textfilter(s, FALSE);
	    }
#else
	    /* diag_listed_as: Unsupported script encoding %s */
	    Perl_croak(aTHX_ "Unsupported script encoding UTF-16BE");
#endif
	}
	break;
    case BOM_UTF8_FIRST_BYTE: {
        if (memBEGINs(s+1, slen - 1, BOM_UTF8_TAIL)) {
#ifdef DEBUGGING
            if (DEBUG_p_TEST || DEBUG_T_TEST) PerlIO_printf(Perl_debug_log, "UTF-8 script encoding (BOM)\n");
#endif
            s += sizeof(BOM_UTF8) - 1;                     /* UTF-8 */
        }
        break;
    }
    case 0:
	if (slen > 3) {
	     if (s[1] == 0) {
		  if (s[2] == 0xFE && s[3] == 0xFF) {
		       /* UTF-32 big-endian */
		       /* diag_listed_as: Unsupported script encoding %s */
		       Perl_croak(aTHX_ "Unsupported script encoding UTF-32BE");
		  }
	     }
	     else if (s[2] == 0 && s[3] != 0) {
		  /* Leading bytes
		   * 00 xx 00 xx
		   * are a good indicator of UTF-16BE. */
#ifndef PERL_NO_UTF16_FILTER
#ifdef DEBUGGING
		  if (DEBUG_p_TEST || DEBUG_T_TEST) PerlIO_printf(Perl_debug_log, "UTF-16BE script encoding (no BOM)\n");
#endif
		  s = add_utf16_textfilter(s, FALSE);
#else
		  /* diag_listed_as: Unsupported script encoding %s */
		  Perl_croak(aTHX_ "Unsupported script encoding UTF-16BE");
#endif
	     }
	}
        break;

    default:
	 if (slen > 3 && s[1] == 0 && s[2] != 0 && s[3] == 0) {
		  /* Leading bytes
		   * xx 00 xx 00
		   * are a good indicator of UTF-16LE. */
#ifndef PERL_NO_UTF16_FILTER
#ifdef DEBUGGING
	      if (DEBUG_p_TEST || DEBUG_T_TEST) PerlIO_printf(Perl_debug_log, "UTF-16LE script encoding (no BOM)\n");
#endif
	      s = add_utf16_textfilter(s, TRUE);
#else
	      /* diag_listed_as: Unsupported script encoding %s */
	      Perl_croak(aTHX_ "Unsupported script encoding UTF-16LE");
#endif
	 }
    }
    return (char*)s;
}


#ifndef PERL_NO_UTF16_FILTER
static I32
S_utf16_textfilter(pTHX_ int idx, SV *sv, int maxlen)
{
    SV *const filter = FILTER_DATA(idx);
    /* We re-use this each time round, throwing the contents away before we
       return.  */
    SV *const utf16_buffer = MUTABLE_SV(IoTOP_GV(filter));
    SV *const utf8_buffer = filter;
    IV status = IoPAGE(filter);
    const bool reverse = cBOOL(IoLINES(filter));
    I32 retval;

    PERL_ARGS_ASSERT_UTF16_TEXTFILTER;

    /* As we're automatically added, at the lowest level, and hence only called
       from this file, we can be sure that we're not called in block mode. Hence
       don't bother writing code to deal with block mode.  */
    if (maxlen) {
	Perl_croak(aTHX_ "panic: utf16_textfilter called in block mode (for %d characters)", maxlen);
    }
    if (status < 0) {
	Perl_croak(aTHX_ "panic: utf16_textfilter called after error (status=%" IVdf ")", status);
    }
    DEBUG_P(PerlIO_printf(Perl_debug_log,
			  "utf16_textfilter(%p,%ce): idx=%d maxlen=%d status=%" IVdf " utf16=%" UVuf " utf8=%" UVuf "\n",
			  FPTR2DPTR(void *, S_utf16_textfilter),
			  reverse ? 'l' : 'b', idx, maxlen, status,
			  (UV)SvCUR(utf16_buffer), (UV)SvCUR(utf8_buffer)));

    while (1) {
	STRLEN chars;
	STRLEN have;
	I32 newlen;
	U8 *end;
	/* First, look in our buffer of existing UTF-8 data:  */
	char *nl = (char *)memchr(SvPVX(utf8_buffer), '\n', SvCUR(utf8_buffer));

	if (nl) {
	    ++nl;
	} else if (status == 0) {
	    /* EOF */
	    IoPAGE(filter) = 0;
	    nl = SvEND(utf8_buffer);
	}
	if (nl) {
	    STRLEN got = nl - SvPVX(utf8_buffer);
	    /* Did we have anything to append?  */
	    retval = got != 0;
	    sv_catpvn(sv, SvPVX(utf8_buffer), got);
	    /* Everything else in this code works just fine if SVp_POK isn't
	       set.  This, however, needs it, and we need it to work, else
	       we loop infinitely because the buffer is never consumed.  */
	    sv_chop(utf8_buffer, nl);
	    break;
	}

	/* OK, not a complete line there, so need to read some more UTF-16.
	   Read an extra octect if the buffer currently has an odd number. */
	while (1) {
	    if (status <= 0)
		break;
	    if (SvCUR(utf16_buffer) >= 2) {
		/* Location of the high octet of the last complete code point.
		   Gosh, UTF-16 is a pain. All the benefits of variable length,
		   *coupled* with all the benefits of partial reads and
		   endianness.  */
		const U8 *const last_hi = (U8*)SvPVX(utf16_buffer)
		    + ((SvCUR(utf16_buffer) & ~1) - (reverse ? 1 : 2));

		if (*last_hi < 0xd8 || *last_hi > 0xdb) {
		    break;
		}

		/* We have the first half of a surrogate. Read more.  */
		DEBUG_P(PerlIO_printf(Perl_debug_log, "utf16_textfilter partial surrogate detected at %p\n", last_hi));
	    }

	    status = FILTER_READ(idx + 1, utf16_buffer,
				 160 + (SvCUR(utf16_buffer) & 1));
	    DEBUG_P(PerlIO_printf(Perl_debug_log, "utf16_textfilter status=%" IVdf " SvCUR(sv)=%" UVuf "\n", status, (UV)SvCUR(utf16_buffer)));
	    DEBUG_P({ sv_dump(utf16_buffer); sv_dump(utf8_buffer);});
	    if (status < 0) {
		/* Error */
		IoPAGE(filter) = status;
		return status;
	    }
	}

        /* 'chars' isn't quite the right name, as code points above 0xFFFF
         * require 4 bytes per char */
	chars = SvCUR(utf16_buffer) >> 1;
	have = SvCUR(utf8_buffer);

        /* Assume the worst case size as noted by the functions: twice the
         * number of input bytes */
	SvGROW(utf8_buffer, have + chars * 4 + 1);

	if (reverse) {
	    end = utf16_to_utf8_reversed((U8*)SvPVX(utf16_buffer),
					 (U8*)SvPVX_const(utf8_buffer) + have,
					 chars * 2, &newlen);
	} else {
	    end = utf16_to_utf8((U8*)SvPVX(utf16_buffer),
				(U8*)SvPVX_const(utf8_buffer) + have,
				chars * 2, &newlen);
	}
	SvCUR_set(utf8_buffer, have + newlen);
	*end = '\0';

	/* No need to keep this SV "well-formed" with a '\0' after the end, as
	   it's private to us, and utf16_to_utf8{,reversed} take a
	   (pointer,length) pair, rather than a NUL-terminated string.  */
	if(SvCUR(utf16_buffer) & 1) {
	    *SvPVX(utf16_buffer) = SvEND(utf16_buffer)[-1];
	    SvCUR_set(utf16_buffer, 1);
	} else {
	    SvCUR_set(utf16_buffer, 0);
	}
    }
    DEBUG_P(PerlIO_printf(Perl_debug_log,
			  "utf16_textfilter: returns, status=%" IVdf " utf16=%" UVuf " utf8=%" UVuf "\n",
			  status,
			  (UV)SvCUR(utf16_buffer), (UV)SvCUR(utf8_buffer)));
    DEBUG_P({ sv_dump(utf8_buffer); sv_dump(sv);});
    return retval;
}

static U8 *
S_add_utf16_textfilter(pTHX_ U8 *const s, bool reversed)
{
    SV *filter = filter_add(S_utf16_textfilter, NULL);

    PERL_ARGS_ASSERT_ADD_UTF16_TEXTFILTER;

    IoTOP_GV(filter) = MUTABLE_GV(newSVpvn((char *)s, PL_bufend - (char*)s));
    SvPVCLEAR(filter);
    IoLINES(filter) = reversed;
    IoPAGE(filter) = 1; /* Not EOF */

    /* Sadly, we have to return a valid pointer, come what may, so we have to
       ignore any error return from this.  */
    SvCUR_set(PL_linestr, 0);
    if (FILTER_READ(0, PL_linestr, 0)) {
	SvUTF8_on(PL_linestr);
    } else {
	SvUTF8_on(PL_linestr);
    }
    PL_bufend = SvEND(PL_linestr);
    return (U8*)SvPVX(PL_linestr);
}
#endif

/*
Returns a pointer to the next character after the parsed
vstring, as well as updating the passed in sv.

Function must be called like

	sv = sv_2mortal(newSV(5));
	s = scan_vstring(s,e,sv);

where s and e are the start and end of the string.
The sv should already be large enough to store the vstring
passed in, for performance reasons.

This function may croak if fatal warnings are enabled in the
calling scope, hence the sv_2mortal in the example (to prevent
a leak).  Make sure to do SvREFCNT_inc afterwards if you use
sv_2mortal.

*/

char *
Perl_scan_vstring(pTHX_ const char *s, const char *const e, SV *sv)
{
    const char *pos = s;
    const char *start = s;

    PERL_ARGS_ASSERT_SCAN_VSTRING;

    if (*pos == 'v') pos++;  /* get past 'v' */
    while (pos < e && (isDIGIT(*pos) || *pos == '_'))
	pos++;
    if ( *pos != '.') {
	/* this may not be a v-string if followed by => */
	const char *next = pos;
	while (next < e && isSPACE(*next))
	    ++next;
	if ((e - next) >= 2 && *next == '=' && next[1] == '>' ) {
	    /* return string not v-string */
	    sv_setpvn(sv,(char *)s,pos-s);
	    return (char *)pos;
	}
    }

    if (!isALPHA(*pos)) {
	U8 tmpbuf[UTF8_MAXBYTES+1];

	if (*s == 'v')
	    s++;  /* get past 'v' */

        SvPVCLEAR(sv);

	for (;;) {
	    /* this is atoi() that tolerates underscores */
	    U8 *tmpend;
	    UV rev = 0;
	    const char *end = pos;
	    UV mult = 1;
	    while (--end >= s) {
		if (*end != '_') {
		    const UV orev = rev;
		    rev += (*end - '0') * mult;
		    mult *= 10;
		    if (orev > rev)
			/* diag_listed_as: Integer overflow in %s number */
			Perl_ck_warner_d(aTHX_ packWARN(WARN_OVERFLOW),
					 "Integer overflow in decimal number");
		}
	    }

	    /* Append native character for the rev point */
	    tmpend = uvchr_to_utf8(tmpbuf, rev);
	    sv_catpvn(sv, (const char*)tmpbuf, tmpend - tmpbuf);
	    if (!UVCHR_IS_INVARIANT(rev))
		 SvUTF8_on(sv);
	    if (pos + 1 < e && *pos == '.' && isDIGIT(pos[1]))
		 s = ++pos;
	    else {
		 s = pos;
		 break;
	    }
	    while (pos < e && (isDIGIT(*pos) || *pos == '_'))
		 pos++;
	}
	SvPOK_on(sv);
	sv_magic(sv,NULL,PERL_MAGIC_vstring,(const char*)start, pos-start);
	SvRMAGICAL_on(sv);
    }
    return (char *)s;
}

int
Perl_keyword_plugin_standard(pTHX_
	char *keyword_ptr, STRLEN keyword_len, OP **op_ptr)
{
    PERL_ARGS_ASSERT_KEYWORD_PLUGIN_STANDARD;
    PERL_UNUSED_CONTEXT;
    PERL_UNUSED_ARG(keyword_ptr);
    PERL_UNUSED_ARG(keyword_len);
    PERL_UNUSED_ARG(op_ptr);
    return KEYWORD_PLUGIN_DECLINE;
}

/*
=for apidoc Amx|void|wrap_keyword_plugin|Perl_keyword_plugin_t new_plugin|Perl_keyword_plugin_t *old_plugin_p

Puts a C function into the chain of keyword plugins.  This is the
preferred way to manipulate the L</PL_keyword_plugin> variable.
C<new_plugin> is a pointer to the C function that is to be added to the
keyword plugin chain, and C<old_plugin_p> points to the storage location
where a pointer to the next function in the chain will be stored.  The
value of C<new_plugin> is written into the L</PL_keyword_plugin> variable,
while the value previously stored there is written to C<*old_plugin_p>.

L</PL_keyword_plugin> is global to an entire process, and a module wishing
to hook keyword parsing may find itself invoked more than once per
process, typically in different threads.  To handle that situation, this
function is idempotent.  The location C<*old_plugin_p> must initially
(once per process) contain a null pointer.  A C variable of static
duration (declared at file scope, typically also marked C<static> to give
it internal linkage) will be implicitly initialised appropriately, if it
does not have an explicit initialiser.  This function will only actually
modify the plugin chain if it finds C<*old_plugin_p> to be null.  This
function is also thread safe on the small scale.  It uses appropriate
locking to avoid race conditions in accessing L</PL_keyword_plugin>.

When this function is called, the function referenced by C<new_plugin>
must be ready to be called, except for C<*old_plugin_p> being unfilled.
In a threading situation, C<new_plugin> may be called immediately, even
before this function has returned.  C<*old_plugin_p> will always be
appropriately set before C<new_plugin> is called.  If C<new_plugin>
decides not to do anything special with the identifier that it is given
(which is the usual case for most calls to a keyword plugin), it must
chain the plugin function referenced by C<*old_plugin_p>.

Taken all together, XS code to install a keyword plugin should typically
look something like this:

    static Perl_keyword_plugin_t next_keyword_plugin;
    static OP *my_keyword_plugin(pTHX_
        char *keyword_plugin, STRLEN keyword_len, OP **op_ptr)
    {
        if (memEQs(keyword_ptr, keyword_len,
                   "my_new_keyword")) {
            ...
        } else {
            return next_keyword_plugin(aTHX_
                keyword_ptr, keyword_len, op_ptr);
        }
    }
    BOOT:
        wrap_keyword_plugin(my_keyword_plugin,
                            &next_keyword_plugin);

Direct access to L</PL_keyword_plugin> should be avoided.

=cut
*/

void
Perl_wrap_keyword_plugin(pTHX_
    Perl_keyword_plugin_t new_plugin, Perl_keyword_plugin_t *old_plugin_p)
{
    dVAR;

    PERL_UNUSED_CONTEXT;
    PERL_ARGS_ASSERT_WRAP_KEYWORD_PLUGIN;
    if (*old_plugin_p) return;
    KEYWORD_PLUGIN_MUTEX_LOCK;
    if (!*old_plugin_p) {
        *old_plugin_p = PL_keyword_plugin;
        PL_keyword_plugin = new_plugin;
    }
    KEYWORD_PLUGIN_MUTEX_UNLOCK;
}

#define parse_recdescent(g,p) S_parse_recdescent(aTHX_ g,p)
static void
S_parse_recdescent(pTHX_ int gramtype, I32 fakeeof)
{
    SAVEI32(PL_lex_brackets);
    if (PL_lex_brackets > 100)
	Renew(PL_lex_brackstack, PL_lex_brackets + 10, char);
    PL_lex_brackstack[PL_lex_brackets++] = XFAKEEOF;
    SAVEI32(PL_lex_allbrackets);
    PL_lex_allbrackets = 0;
    SAVEI8(PL_lex_fakeeof);
    PL_lex_fakeeof = (U8)fakeeof;
    if(yyparse(gramtype) && !PL_parser->error_count)
	qerror(Perl_mess(aTHX_ "Parse error"));
}

#define parse_recdescent_for_op(g,p) S_parse_recdescent_for_op(aTHX_ g,p)
static OP *
S_parse_recdescent_for_op(pTHX_ int gramtype, I32 fakeeof)
{
    OP *o;
    ENTER;
    SAVEVPTR(PL_eval_root);
    PL_eval_root = NULL;
    parse_recdescent(gramtype, fakeeof);
    o = PL_eval_root;
    LEAVE;
    return o;
}

#define parse_expr(p,f) S_parse_expr(aTHX_ p,f)
static OP *
S_parse_expr(pTHX_ I32 fakeeof, U32 flags)
{
    OP *exprop;
    if (flags & ~PARSE_OPTIONAL)
	Perl_croak(aTHX_ "Parsing code internal error (%s)", "parse_expr");
    exprop = parse_recdescent_for_op(GRAMEXPR, fakeeof);
    if (!exprop && !(flags & PARSE_OPTIONAL)) {
	if (!PL_parser->error_count)
	    qerror(Perl_mess(aTHX_ "Parse error"));
	exprop = newOP(OP_NULL, 0);
    }
    return exprop;
}

/*
=for apidoc Amx|OP *|parse_arithexpr|U32 flags

Parse a Perl arithmetic expression.  This may contain operators of precedence
down to the bit shift operators.  The expression must be followed (and thus
terminated) either by a comparison or lower-precedence operator or by
something that would normally terminate an expression such as semicolon.
If C<flags> has the C<PARSE_OPTIONAL> bit set, then the expression is optional,
otherwise it is mandatory.  It is up to the caller to ensure that the
dynamic parser state (L</PL_parser> et al) is correctly set to reflect
the source of the code to be parsed and the lexical context for the
expression.

The op tree representing the expression is returned.  If an optional
expression is absent, a null pointer is returned, otherwise the pointer
will be non-null.

If an error occurs in parsing or compilation, in most cases a valid op
tree is returned anyway.  The error is reflected in the parser state,
normally resulting in a single exception at the top level of parsing
which covers all the compilation errors that occurred.  Some compilation
errors, however, will throw an exception immediately.

=cut
*/

OP *
Perl_parse_arithexpr(pTHX_ U32 flags)
{
    return parse_expr(LEX_FAKEEOF_COMPARE, flags);
}

/*
=for apidoc Amx|OP *|parse_termexpr|U32 flags

Parse a Perl term expression.  This may contain operators of precedence
down to the assignment operators.  The expression must be followed (and thus
terminated) either by a comma or lower-precedence operator or by
something that would normally terminate an expression such as semicolon.
If C<flags> has the C<PARSE_OPTIONAL> bit set, then the expression is optional,
otherwise it is mandatory.  It is up to the caller to ensure that the
dynamic parser state (L</PL_parser> et al) is correctly set to reflect
the source of the code to be parsed and the lexical context for the
expression.

The op tree representing the expression is returned.  If an optional
expression is absent, a null pointer is returned, otherwise the pointer
will be non-null.

If an error occurs in parsing or compilation, in most cases a valid op
tree is returned anyway.  The error is reflected in the parser state,
normally resulting in a single exception at the top level of parsing
which covers all the compilation errors that occurred.  Some compilation
errors, however, will throw an exception immediately.

=cut
*/

OP *
Perl_parse_termexpr(pTHX_ U32 flags)
{
    return parse_expr(LEX_FAKEEOF_COMMA, flags);
}

/*
=for apidoc Amx|OP *|parse_listexpr|U32 flags

Parse a Perl list expression.  This may contain operators of precedence
down to the comma operator.  The expression must be followed (and thus
terminated) either by a low-precedence logic operator such as C<or> or by
something that would normally terminate an expression such as semicolon.
If C<flags> has the C<PARSE_OPTIONAL> bit set, then the expression is optional,
otherwise it is mandatory.  It is up to the caller to ensure that the
dynamic parser state (L</PL_parser> et al) is correctly set to reflect
the source of the code to be parsed and the lexical context for the
expression.

The op tree representing the expression is returned.  If an optional
expression is absent, a null pointer is returned, otherwise the pointer
will be non-null.

If an error occurs in parsing or compilation, in most cases a valid op
tree is returned anyway.  The error is reflected in the parser state,
normally resulting in a single exception at the top level of parsing
which covers all the compilation errors that occurred.  Some compilation
errors, however, will throw an exception immediately.

=cut
*/

OP *
Perl_parse_listexpr(pTHX_ U32 flags)
{
    return parse_expr(LEX_FAKEEOF_LOWLOGIC, flags);
}

/*
=for apidoc Amx|OP *|parse_fullexpr|U32 flags

Parse a single complete Perl expression.  This allows the full
expression grammar, including the lowest-precedence operators such
as C<or>.  The expression must be followed (and thus terminated) by a
token that an expression would normally be terminated by: end-of-file,
closing bracketing punctuation, semicolon, or one of the keywords that
signals a postfix expression-statement modifier.  If C<flags> has the
C<PARSE_OPTIONAL> bit set, then the expression is optional, otherwise it is
mandatory.  It is up to the caller to ensure that the dynamic parser
state (L</PL_parser> et al) is correctly set to reflect the source of
the code to be parsed and the lexical context for the expression.

The op tree representing the expression is returned.  If an optional
expression is absent, a null pointer is returned, otherwise the pointer
will be non-null.

If an error occurs in parsing or compilation, in most cases a valid op
tree is returned anyway.  The error is reflected in the parser state,
normally resulting in a single exception at the top level of parsing
which covers all the compilation errors that occurred.  Some compilation
errors, however, will throw an exception immediately.

=cut
*/

OP *
Perl_parse_fullexpr(pTHX_ U32 flags)
{
    return parse_expr(LEX_FAKEEOF_NONEXPR, flags);
}

/*
=for apidoc Amx|OP *|parse_block|U32 flags

Parse a single complete Perl code block.  This consists of an opening
brace, a sequence of statements, and a closing brace.  The block
constitutes a lexical scope, so C<my> variables and various compile-time
effects can be contained within it.  It is up to the caller to ensure
that the dynamic parser state (L</PL_parser> et al) is correctly set to
reflect the source of the code to be parsed and the lexical context for
the statement.

The op tree representing the code block is returned.  This is always a
real op, never a null pointer.  It will normally be a C<lineseq> list,
including C<nextstate> or equivalent ops.  No ops to construct any kind
of runtime scope are included by virtue of it being a block.

If an error occurs in parsing or compilation, in most cases a valid op
tree (most likely null) is returned anyway.  The error is reflected in
the parser state, normally resulting in a single exception at the top
level of parsing which covers all the compilation errors that occurred.
Some compilation errors, however, will throw an exception immediately.

The C<flags> parameter is reserved for future use, and must always
be zero.

=cut
*/

OP *
Perl_parse_block(pTHX_ U32 flags)
{
    if (flags)
	Perl_croak(aTHX_ "Parsing code internal error (%s)", "parse_block");
    return parse_recdescent_for_op(GRAMBLOCK, LEX_FAKEEOF_NEVER);
}

/*
=for apidoc Amx|OP *|parse_barestmt|U32 flags

Parse a single unadorned Perl statement.  This may be a normal imperative
statement or a declaration that has compile-time effect.  It does not
include any label or other affixture.  It is up to the caller to ensure
that the dynamic parser state (L</PL_parser> et al) is correctly set to
reflect the source of the code to be parsed and the lexical context for
the statement.

The op tree representing the statement is returned.  This may be a
null pointer if the statement is null, for example if it was actually
a subroutine definition (which has compile-time side effects).  If not
null, it will be ops directly implementing the statement, suitable to
pass to L</newSTATEOP>.  It will not normally include a C<nextstate> or
equivalent op (except for those embedded in a scope contained entirely
within the statement).

If an error occurs in parsing or compilation, in most cases a valid op
tree (most likely null) is returned anyway.  The error is reflected in
the parser state, normally resulting in a single exception at the top
level of parsing which covers all the compilation errors that occurred.
Some compilation errors, however, will throw an exception immediately.

The C<flags> parameter is reserved for future use, and must always
be zero.

=cut
*/

OP *
Perl_parse_barestmt(pTHX_ U32 flags)
{
    if (flags)
	Perl_croak(aTHX_ "Parsing code internal error (%s)", "parse_barestmt");
    return parse_recdescent_for_op(GRAMBARESTMT, LEX_FAKEEOF_NEVER);
}

/*
=for apidoc Amx|SV *|parse_label|U32 flags

Parse a single label, possibly optional, of the type that may prefix a
Perl statement.  It is up to the caller to ensure that the dynamic parser
state (L</PL_parser> et al) is correctly set to reflect the source of
the code to be parsed.  If C<flags> has the C<PARSE_OPTIONAL> bit set, then the
label is optional, otherwise it is mandatory.

The name of the label is returned in the form of a fresh scalar.  If an
optional label is absent, a null pointer is returned.

If an error occurs in parsing, which can only occur if the label is
mandatory, a valid label is returned anyway.  The error is reflected in
the parser state, normally resulting in a single exception at the top
level of parsing which covers all the compilation errors that occurred.

=cut
*/

SV *
Perl_parse_label(pTHX_ U32 flags)
{
    if (flags & ~PARSE_OPTIONAL)
	Perl_croak(aTHX_ "Parsing code internal error (%s)", "parse_label");
    if (PL_nexttoke) {
	PL_parser->yychar = yylex();
	if (PL_parser->yychar == LABEL) {
	    char * const lpv = pl_yylval.pval;
	    STRLEN llen = strlen(lpv);
	    PL_parser->yychar = YYEMPTY;
	    return newSVpvn_flags(lpv, llen, lpv[llen+1] ? SVf_UTF8 : 0);
	} else {
	    yyunlex();
	    goto no_label;
	}
    } else {
	char *s, *t;
	STRLEN wlen, bufptr_pos;
	lex_read_space(0);
	t = s = PL_bufptr;
        if (!isIDFIRST_lazy_if_safe(s, PL_bufend, UTF))
	    goto no_label;
	t = scan_word(s, PL_tokenbuf, sizeof PL_tokenbuf, FALSE, &wlen);
	if (word_takes_any_delimiter(s, wlen))
	    goto no_label;
	bufptr_pos = s - SvPVX(PL_linestr);
	PL_bufptr = t;
	lex_read_space(LEX_KEEP_PREVIOUS);
	t = PL_bufptr;
	s = SvPVX(PL_linestr) + bufptr_pos;
	if (t[0] == ':' && t[1] != ':') {
	    PL_oldoldbufptr = PL_oldbufptr;
	    PL_oldbufptr = s;
	    PL_bufptr = t+1;
	    return newSVpvn_flags(s, wlen, UTF ? SVf_UTF8 : 0);
	} else {
	    PL_bufptr = s;
	    no_label:
	    if (flags & PARSE_OPTIONAL) {
		return NULL;
	    } else {
		qerror(Perl_mess(aTHX_ "Parse error"));
		return newSVpvs("x");
	    }
	}
    }
}

/*
=for apidoc Amx|OP *|parse_fullstmt|U32 flags

Parse a single complete Perl statement.  This may be a normal imperative
statement or a declaration that has compile-time effect, and may include
optional labels.  It is up to the caller to ensure that the dynamic
parser state (L</PL_parser> et al) is correctly set to reflect the source
of the code to be parsed and the lexical context for the statement.

The op tree representing the statement is returned.  This may be a
null pointer if the statement is null, for example if it was actually
a subroutine definition (which has compile-time side effects).  If not
null, it will be the result of a L</newSTATEOP> call, normally including
a C<nextstate> or equivalent op.

If an error occurs in parsing or compilation, in most cases a valid op
tree (most likely null) is returned anyway.  The error is reflected in
the parser state, normally resulting in a single exception at the top
level of parsing which covers all the compilation errors that occurred.
Some compilation errors, however, will throw an exception immediately.

The C<flags> parameter is reserved for future use, and must always
be zero.

=cut
*/

OP *
Perl_parse_fullstmt(pTHX_ U32 flags)
{
    if (flags)
	Perl_croak(aTHX_ "Parsing code internal error (%s)", "parse_fullstmt");
    return parse_recdescent_for_op(GRAMFULLSTMT, LEX_FAKEEOF_NEVER);
}

/*
=for apidoc Amx|OP *|parse_stmtseq|U32 flags

Parse a sequence of zero or more Perl statements.  These may be normal
imperative statements, including optional labels, or declarations
that have compile-time effect, or any mixture thereof.  The statement
sequence ends when a closing brace or end-of-file is encountered in a
place where a new statement could have validly started.  It is up to
the caller to ensure that the dynamic parser state (L</PL_parser> et al)
is correctly set to reflect the source of the code to be parsed and the
lexical context for the statements.

The op tree representing the statement sequence is returned.  This may
be a null pointer if the statements were all null, for example if there
were no statements or if there were only subroutine definitions (which
have compile-time side effects).  If not null, it will be a C<lineseq>
list, normally including C<nextstate> or equivalent ops.

If an error occurs in parsing or compilation, in most cases a valid op
tree is returned anyway.  The error is reflected in the parser state,
normally resulting in a single exception at the top level of parsing
which covers all the compilation errors that occurred.  Some compilation
errors, however, will throw an exception immediately.

The C<flags> parameter is reserved for future use, and must always
be zero.

=cut
*/

OP *
Perl_parse_stmtseq(pTHX_ U32 flags)
{
    OP *stmtseqop;
    I32 c;
    if (flags)
	Perl_croak(aTHX_ "Parsing code internal error (%s)", "parse_stmtseq");
    stmtseqop = parse_recdescent_for_op(GRAMSTMTSEQ, LEX_FAKEEOF_CLOSING);
    c = lex_peek_unichar(0);
    if (c != -1 && c != /*{*/'}')
	qerror(Perl_mess(aTHX_ "Parse error"));
    return stmtseqop;
}

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */

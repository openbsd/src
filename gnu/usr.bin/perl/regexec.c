/*    regexec.c
 */

/*
 * "One Ring to rule them all, One Ring to find them..."
 */

/* NOTE: this is derived from Henry Spencer's regexp code, and should not
 * confused with the original package (see point 3 below).  Thanks, Henry!
 */

/* Additional note: this code is very heavily munged from Henry's version
 * in places.  In some spots I've traded clarity for efficiency, so don't
 * blame Henry for some of the lack of readability.
 */

/* The names of the functions have been changed from regcomp and
 * regexec to  pregcomp and pregexec in order to avoid conflicts
 * with the POSIX routines of the same names.
*/

/*SUPPRESS 112*/
/*
 * pregcomp and pregexec -- regsub and regerror are not used in perl
 *
 *	Copyright (c) 1986 by University of Toronto.
 *	Written by Henry Spencer.  Not derived from licensed software.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to redistribute it freely,
 *	subject to the following restrictions:
 *
 *	1. The author is not responsible for the consequences of use of
 *		this software, no matter how awful, even if they arise
 *		from defects in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *		by explicit claim or by omission.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *		be misrepresented as being the original software.
 *
 ****    Alterations to Henry's code are...
 ****
 ****    Copyright (c) 1991-1997, Larry Wall
 ****
 ****    You may distribute under the terms of either the GNU General Public
 ****    License or the Artistic License, as specified in the README file.
 *
 * Beware that some of this code is subtly aware of the way operator
 * precedence is structured in regular expressions.  Serious changes in
 * regular-expression syntax might require a total rethink.
 */
#include "EXTERN.h"
#include "perl.h"
#include "regcomp.h"

#ifndef STATIC
#define	STATIC	static
#endif

#ifdef DEBUGGING
static I32 regnarrate = 0;
static char* regprogram = 0;
#endif

/* Current curly descriptor */
typedef struct curcur CURCUR;
struct curcur {
    int		parenfloor;	/* how far back to strip paren data */
    int		cur;		/* how many instances of scan we've matched */
    int		min;		/* the minimal number of scans to match */
    int		max;		/* the maximal number of scans to match */
    int		minmod;		/* whether to work our way up or down */
    char *	scan;		/* the thing to match */
    char *	next;		/* what has to match after it */
    char *	lastloc;	/* where we started matching this scan */
    CURCUR *	oldcc;		/* current curly before we started this one */
};

static CURCUR* regcc;

typedef I32 CHECKPOINT;

static CHECKPOINT regcppush _((I32 parenfloor));
static char * regcppop _((void));

static CHECKPOINT
regcppush(parenfloor)
I32 parenfloor;
{
    int retval = savestack_ix;
    int i = (regsize - parenfloor) * 3;
    int p;

    SSCHECK(i + 5);
    for (p = regsize; p > parenfloor; p--) {
	SSPUSHPTR(regendp[p]);
	SSPUSHPTR(regstartp[p]);
	SSPUSHINT(p);
    }
    SSPUSHINT(regsize);
    SSPUSHINT(*reglastparen);
    SSPUSHPTR(reginput);
    SSPUSHINT(i + 3);
    SSPUSHINT(SAVEt_REGCONTEXT);
    return retval;
}

static char *
regcppop()
{
    I32 i = SSPOPINT;
    U32 paren = 0;
    char *input;
    char *tmps;
    assert(i == SAVEt_REGCONTEXT);
    i = SSPOPINT;
    input = (char *) SSPOPPTR;
    *reglastparen = SSPOPINT;
    regsize = SSPOPINT;
    for (i -= 3; i > 0; i -= 3) {
	paren = (U32)SSPOPINT;
	regstartp[paren] = (char *) SSPOPPTR;
	tmps = (char*)SSPOPPTR;
	if (paren <= *reglastparen)
	    regendp[paren] = tmps;
    }
    for (paren = *reglastparen + 1; paren <= regnpar; paren++) {
	if (paren > regsize)
	    regstartp[paren] = Nullch;
	regendp[paren] = Nullch;
    }
    return input;
}

/* After a successful match in WHILEM, we want to restore paren matches
 * that have been overwritten by a failed match attempt in the process
 * of reaching this success. We do this by restoring regstartp[i]
 * wherever regendp[i] has not changed; if OPEN is changed to modify
 * regendp[], the '== endp' test below should be changed to match.
 * This corrects the error of:
 *	0 > length [ "foobar" =~ / ( (foo) | (bar) )* /x ]->[1]
 */
static void
regcppartblow(base)
I32 base;
{
    I32 i = SSPOPINT;
    U32 paren;
    char *startp;
    char *endp;
    assert(i == SAVEt_REGCONTEXT);
    i = SSPOPINT;
    /* input, lastparen, size */
    SSPOPPTR; SSPOPINT; SSPOPINT;
    for (i -= 3; i > 0; i -= 3) {
	paren = (U32)SSPOPINT;
	startp = (char *) SSPOPPTR;
	endp = (char *) SSPOPPTR;
	if (paren <= *reglastparen && regendp[paren] == endp)
	    regstartp[paren] = startp;
    }
    assert(savestack_ix == base);
}

#define regcpblow(cp) leave_scope(cp)

/*
 * pregexec and friends
 */

/*
 * Forwards.
 */

static I32 regmatch _((char *prog));
static I32 regrepeat _((char *p, I32 max));
static I32 regtry _((regexp *prog, char *startpos));
static bool reginclass _((char *p, I32 c));

static bool regtainted;		/* tainted information used? */

/*
 - pregexec - match a regexp against a string
 */
I32
pregexec(prog, stringarg, strend, strbeg, minend, screamer, safebase)
register regexp *prog;
char *stringarg;
register char *strend;	/* pointer to null at end of string */
char *strbeg;	/* real beginning of string */
I32 minend;	/* end of match must be at least minend after stringarg */
SV *screamer;
I32 safebase;	/* no need to remember string in subbase */
{
    register char *s;
    register char *c;
    register char *startpos = stringarg;
    register I32 tmp;
    I32 minlen = 0;		/* must match at least this many chars */
    I32 dontbother = 0;	/* how many characters not to try at end */
    CURCUR cc;

    cc.cur = 0;
    cc.oldcc = 0;
    regcc = &cc;

#ifdef DEBUGGING
    regnarrate = debug & 512;
    regprogram = prog->program;
#endif

    /* Be paranoid... */
    if (prog == NULL || startpos == NULL) {
	croak("NULL regexp parameter");
	return 0;
    }

    if (startpos == strbeg)	/* is ^ valid at stringarg? */
	regprev = '\n';
    else {
	regprev = stringarg[-1];
	if (!multiline && regprev == '\n')
	    regprev = '\0';		/* force ^ to NOT match */
    }

    regprecomp = prog->precomp;
    /* Check validity of program. */
    if (UCHARAT(prog->program) != MAGIC) {
	FAIL("corrupted regexp program");
    }

    regnpar = prog->nparens;
    regtainted = FALSE;

    /* If there is a "must appear" string, look for it. */
    s = startpos;
    if (prog->regmust != Nullsv &&
	!(prog->reganch & ROPT_ANCH_GPOS) &&
	(!(prog->reganch & ROPT_ANCH_BOL)
	 || (multiline && prog->regback >= 0)) )
    {
	if (stringarg == strbeg && screamer) {
	    if (screamfirst[BmRARE(prog->regmust)] >= 0)
		    s = screaminstr(screamer,prog->regmust);
	    else
		    s = Nullch;
	}
	else
	    s = fbm_instr((unsigned char*)s, (unsigned char*)strend,
		prog->regmust);
	if (!s) {
	    ++BmUSEFUL(prog->regmust);	/* hooray */
	    goto phooey;	/* not present */
	}
	else if (prog->regback >= 0) {
	    s -= prog->regback;
	    if (s < startpos)
		s = startpos;
	    minlen = prog->regback + SvCUR(prog->regmust);
	}
	else if (!prog->naughty && --BmUSEFUL(prog->regmust) < 0) { /* boo */
	    SvREFCNT_dec(prog->regmust);
	    prog->regmust = Nullsv;	/* disable regmust */
	    s = startpos;
	}
	else {
	    s = startpos;
	    minlen = SvCUR(prog->regmust);
	}
    }

    /* Mark beginning of line for ^ . */
    regbol = startpos;

    /* Mark end of line for $ (and such) */
    regeol = strend;

    /* see how far we have to get to not match where we matched before */
    regtill = startpos+minend;

    /* Simplest case:  anchored match need be tried only once. */
    /*  [unless only anchor is BOL and multiline is set] */
    if (prog->reganch & ROPT_ANCH) {
	if (regtry(prog, startpos))
	    goto got_it;
	else if (!(prog->reganch & ROPT_ANCH_GPOS) &&
		 (multiline || (prog->reganch & ROPT_IMPLICIT)))
	{
	    if (minlen)
		dontbother = minlen - 1;
	    strend -= dontbother;
	    /* for multiline we only have to try after newlines */
	    if (s > startpos)
		s--;
	    while (s < strend) {
		if (*s++ == '\n') {
		    if (s < strend && regtry(prog, s))
			goto got_it;
		}
	    }
	}
	goto phooey;
    }

    /* Messy cases:  unanchored match. */
    if (prog->regstart) {
	if (prog->reganch & ROPT_SKIP) {  /* we have /x+whatever/ */
	    /* it must be a one character string */
	    char ch = SvPVX(prog->regstart)[0];
	    while (s < strend) {
		if (*s == ch) {
		    if (regtry(prog, s))
			goto got_it;
		    s++;
		    while (s < strend && *s == ch)
			s++;
		}
		s++;
	    }
	}
	else if (SvTYPE(prog->regstart) == SVt_PVBM) {
	    /* We know what string it must start with. */
	    while ((s = fbm_instr((unsigned char*)s,
	      (unsigned char*)strend, prog->regstart)) != NULL)
	    {
		if (regtry(prog, s))
		    goto got_it;
		s++;
	    }
	}
	else {				/* Optimized fbm_instr: */
	    c = SvPVX(prog->regstart);
	    while ((s = ninstr(s, strend, c, c + SvCUR(prog->regstart))) != NULL)
	    {
		if (regtry(prog, s))
		    goto got_it;
		s++;
	    }
	}
	goto phooey;
    }
    /*SUPPRESS 560*/
    if (c = prog->regstclass) {
	I32 doevery = (prog->reganch & ROPT_SKIP) == 0;

	if (minlen)
	    dontbother = minlen - 1;
	strend -= dontbother;	/* don't bother with what can't match */
	tmp = 1;
	/* We know what class it must start with. */
	switch (OP(c)) {
	case ANYOF:
	    c = OPERAND(c);
	    while (s < strend) {
		if (reginclass(c, *s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case BOUNDL:
	    regtainted = TRUE;
	    /* FALL THROUGH */
	case BOUND:
	    if (minlen)
		dontbother++,strend--;
	    tmp = (s != startpos) ? UCHARAT(s - 1) : regprev;
	    tmp = ((OP(c) == BOUND ? isALNUM(tmp) : isALNUM_LC(tmp)) != 0);
	    while (s < strend) {
		if (tmp == !(OP(c) == BOUND ? isALNUM(*s) : isALNUM_LC(*s))) {
		    tmp = !tmp;
		    if (regtry(prog, s))
			goto got_it;
		}
		s++;
	    }
	    if ((minlen || tmp) && regtry(prog,s))
		goto got_it;
	    break;
	case NBOUNDL:
	    regtainted = TRUE;
	    /* FALL THROUGH */
	case NBOUND:
	    if (minlen)
		dontbother++,strend--;
	    tmp = (s != startpos) ? UCHARAT(s - 1) : regprev;
	    tmp = ((OP(c) == NBOUND ? isALNUM(tmp) : isALNUM_LC(tmp)) != 0);
	    while (s < strend) {
		if (tmp == !(OP(c) == NBOUND ? isALNUM(*s) : isALNUM_LC(*s)))
		    tmp = !tmp;
		else if (regtry(prog, s))
		    goto got_it;
		s++;
	    }
	    if ((minlen || !tmp) && regtry(prog,s))
		goto got_it;
	    break;
	case ALNUM:
	    while (s < strend) {
		if (isALNUM(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case ALNUML:
	    regtainted = TRUE;
	    while (s < strend) {
		if (isALNUM_LC(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case NALNUM:
	    while (s < strend) {
		if (!isALNUM(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case NALNUML:
	    regtainted = TRUE;
	    while (s < strend) {
		if (!isALNUM_LC(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case SPACE:
	    while (s < strend) {
		if (isSPACE(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case SPACEL:
	    regtainted = TRUE;
	    while (s < strend) {
		if (isSPACE_LC(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case NSPACE:
	    while (s < strend) {
		if (!isSPACE(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case NSPACEL:
	    regtainted = TRUE;
	    while (s < strend) {
		if (!isSPACE_LC(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case DIGIT:
	    while (s < strend) {
		if (isDIGIT(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	case NDIGIT:
	    while (s < strend) {
		if (!isDIGIT(*s)) {
		    if (tmp && regtry(prog, s))
			goto got_it;
		    else
			tmp = doevery;
		}
		else
		    tmp = 1;
		s++;
	    }
	    break;
	}
    }
    else {
	if (minlen)
	    dontbother = minlen - 1;
	strend -= dontbother;
	/* We don't know much -- general case. */
	do {
	    if (regtry(prog, s))
		goto got_it;
	} while (s++ < strend);
    }

    /* Failure. */
    goto phooey;

got_it:
    strend += dontbother;	/* uncheat */
    prog->subbeg = strbeg;
    prog->subend = strend;
    prog->exec_tainted = regtainted;

    /* make sure $`, $&, $', and $digit will work later */
    if (strbeg != prog->subbase) {
	if (safebase) {
	    if (prog->subbase) {
		Safefree(prog->subbase);
		prog->subbase = Nullch;
	    }
	}
	else {
	    I32 i = strend - startpos + (stringarg - strbeg);
	    s = savepvn(strbeg, i);
	    Safefree(prog->subbase);
	    prog->subbase = s;
	    prog->subbeg = prog->subbase;
	    prog->subend = prog->subbase + i;
	    s = prog->subbase + (stringarg - strbeg);
	    for (i = 0; i <= prog->nparens; i++) {
		if (prog->endp[i]) {
		    prog->startp[i] = s + (prog->startp[i] - startpos);
		    prog->endp[i] = s + (prog->endp[i] - startpos);
		}
	    }
	}
    }
    return 1;

phooey:
    return 0;
}

/*
 - regtry - try match at specific point
 */
static I32			/* 0 failure, 1 success */
regtry(prog, startpos)
regexp *prog;
char *startpos;
{
    register I32 i;
    register char **sp;
    register char **ep;

    reginput = startpos;
    regstartp = prog->startp;
    regendp = prog->endp;
    reglastparen = &prog->lastparen;
    prog->lastparen = 0;
    regsize = 0;

    sp = prog->startp;
    ep = prog->endp;
    if (prog->nparens) {
	for (i = prog->nparens; i >= 0; i--) {
	    *sp++ = NULL;
	    *ep++ = NULL;
	}
    }
    if (regmatch(prog->program + 1) && reginput >= regtill) {
	prog->startp[0] = startpos;
	prog->endp[0] = reginput;
	return 1;
    }
    else
	return 0;
}

/*
 - regmatch - main matching routine
 *
 * Conceptually the strategy is simple:  check to see whether the current
 * node matches, call self recursively to see whether the rest matches,
 * and then act accordingly.  In practice we make some effort to avoid
 * recursion, in particular by going through "ordinary" nodes (that don't
 * need to know whether the rest of the match failed) by a loop instead of
 * by recursion.
 */
/* [lwall] I've hoisted the register declarations to the outer block in order to
 * maybe save a little bit of pushing and popping on the stack.  It also takes
 * advantage of machines that use a register save mask on subroutine entry.
 */
static I32			/* 0 failure, 1 success */
regmatch(prog)
char *prog;
{
    register char *scan;	/* Current node. */
    char *next;			/* Next node. */
    register I32 nextchar;
    register I32 n;		/* no or next */
    register I32 ln;		/* len or last */
    register char *s;		/* operand or save */
    register char *locinput = reginput;
    register I32 c1, c2;	/* case fold search */
    int minmod = 0;
#ifdef DEBUGGING
    static int regindent = 0;
    regindent++;
#endif

    nextchar = UCHARAT(locinput);
    scan = prog;
    while (scan != NULL) {
#ifdef DEBUGGING
#define sayYES goto yes
#define sayNO goto no
#define saySAME(x) if (x) goto yes; else goto no
	if (regnarrate) {
	    SV *prop = sv_newmortal();
	    regprop(prop, scan);
	    PerlIO_printf(Perl_debug_log, "%*s%2ld%-8.8s\t<%.10s>\n",
			  regindent*2, "", (long)(scan - regprogram),
			  SvPVX(prop), locinput);
	}
#else
#define sayYES return 1
#define sayNO return 0
#define saySAME(x) return x
#endif

#ifdef REGALIGN
	next = scan + NEXT(scan);
	if (next == scan)
	    next = NULL;
#else
	next = regnext(scan);
#endif

	switch (OP(scan)) {
	case BOL:
	    if (locinput == regbol
		? regprev == '\n'
		: ((nextchar || locinput < regeol) && locinput[-1] == '\n') )
	    {
		/* regtill = regbol; */
		break;
	    }
	    sayNO;
	case MBOL:
	    if (locinput == regbol
		? regprev == '\n'
		: ((nextchar || locinput < regeol) && locinput[-1] == '\n') )
	    {
		break;
	    }
	    sayNO;
	case SBOL:
	    if (locinput == regbol && regprev == '\n')
		break;
	    sayNO;
	case GPOS:
	    if (locinput == regbol)
		break;
	    sayNO;
	case EOL:
	    if (multiline)
		goto meol;
	    else
		goto seol;
	case MEOL:
	  meol:
	    if ((nextchar || locinput < regeol) && nextchar != '\n')
		sayNO;
	    break;
	case SEOL:
	  seol:
	    if ((nextchar || locinput < regeol) && nextchar != '\n')
		sayNO;
	    if (regeol - locinput > 1)
		sayNO;
	    break;
	case SANY:
	    if (!nextchar && locinput >= regeol)
		sayNO;
	    nextchar = UCHARAT(++locinput);
	    break;
	case ANY:
	    if (!nextchar && locinput >= regeol || nextchar == '\n')
		sayNO;
	    nextchar = UCHARAT(++locinput);
	    break;
	case EXACT:
	    s = OPERAND(scan);
	    ln = *s++;
	    /* Inline the first character, for speed. */
	    if (UCHARAT(s) != nextchar)
		sayNO;
	    if (regeol - locinput < ln)
		sayNO;
	    if (ln > 1 && memNE(s, locinput, ln))
		sayNO;
	    locinput += ln;
	    nextchar = UCHARAT(locinput);
	    break;
	case EXACTFL:
	    regtainted = TRUE;
	    /* FALL THROUGH */
	case EXACTF:
	    s = OPERAND(scan);
	    ln = *s++;
	    /* Inline the first character, for speed. */
	    if (UCHARAT(s) != nextchar &&
		UCHARAT(s) != ((OP(scan) == EXACTF)
			       ? fold : fold_locale)[nextchar])
		sayNO;
	    if (regeol - locinput < ln)
		sayNO;
	    if (ln > 1 && (OP(scan) == EXACTF
			   ? ibcmp(s, locinput, ln)
			   : ibcmp_locale(s, locinput, ln)))
		sayNO;
	    locinput += ln;
	    nextchar = UCHARAT(locinput);
	    break;
	case ANYOF:
	    s = OPERAND(scan);
	    if (nextchar < 0)
		nextchar = UCHARAT(locinput);
	    if (!reginclass(s, nextchar))
		sayNO;
	    if (!nextchar && locinput >= regeol)
		sayNO;
	    nextchar = UCHARAT(++locinput);
	    break;
	case ALNUML:
	    regtainted = TRUE;
	    /* FALL THROUGH */
	case ALNUM:
	    if (!nextchar)
		sayNO;
	    if (!(OP(scan) == ALNUM
		  ? isALNUM(nextchar) : isALNUM_LC(nextchar)))
		sayNO;
	    nextchar = UCHARAT(++locinput);
	    break;
	case NALNUML:
	    regtainted = TRUE;
	    /* FALL THROUGH */
	case NALNUM:
	    if (!nextchar && locinput >= regeol)
		sayNO;
	    if (OP(scan) == NALNUM
		? isALNUM(nextchar) : isALNUM_LC(nextchar))
		sayNO;
	    nextchar = UCHARAT(++locinput);
	    break;
	case BOUNDL:
	case NBOUNDL:
	    regtainted = TRUE;
	    /* FALL THROUGH */
	case BOUND:
	case NBOUND:
	    /* was last char in word? */
	    ln = (locinput != regbol) ? UCHARAT(locinput - 1) : regprev;
	    if (OP(scan) == BOUND || OP(scan) == NBOUND) {
		ln = isALNUM(ln);
		n = isALNUM(nextchar);
	    }
	    else {
		ln = isALNUM_LC(ln);
		n = isALNUM_LC(nextchar);
	    }
	    if (((!ln) == (!n)) == (OP(scan) == BOUND || OP(scan) == BOUNDL))
		sayNO;
	    break;
	case SPACEL:
	    regtainted = TRUE;
	    /* FALL THROUGH */
	case SPACE:
	    if (!nextchar && locinput >= regeol)
		sayNO;
	    if (!(OP(scan) == SPACE
		  ? isSPACE(nextchar) : isSPACE_LC(nextchar)))
		sayNO;
	    nextchar = UCHARAT(++locinput);
	    break;
	case NSPACEL:
	    regtainted = TRUE;
	    /* FALL THROUGH */
	case NSPACE:
	    if (!nextchar)
		sayNO;
	    if (OP(scan) == SPACE
		? isSPACE(nextchar) : isSPACE_LC(nextchar))
		sayNO;
	    nextchar = UCHARAT(++locinput);
	    break;
	case DIGIT:
	    if (!isDIGIT(nextchar))
		sayNO;
	    nextchar = UCHARAT(++locinput);
	    break;
	case NDIGIT:
	    if (!nextchar && locinput >= regeol)
		sayNO;
	    if (isDIGIT(nextchar))
		sayNO;
	    nextchar = UCHARAT(++locinput);
	    break;
	case REFFL:
	    regtainted = TRUE;
	    /* FALL THROUGH */
	case REF:
	case REFF:
	    n = ARG1(scan);  /* which paren pair */
	    s = regstartp[n];
	    if (!s)
		sayNO;
	    if (!regendp[n])
		sayNO;
	    if (s == regendp[n])
		break;
	    /* Inline the first character, for speed. */
	    if (UCHARAT(s) != nextchar &&
		(OP(scan) == REF ||
		 (UCHARAT(s) != ((OP(scan) == REFF
				 ? fold : fold_locale)[nextchar]))))
		sayNO;
	    ln = regendp[n] - s;
	    if (locinput + ln > regeol)
		sayNO;
	    if (ln > 1 && (OP(scan) == REF
			   ? memNE(s, locinput, ln)
			   : (OP(scan) == REFF
			      ? ibcmp(s, locinput, ln)
			      : ibcmp_locale(s, locinput, ln))))
		sayNO;
	    locinput += ln;
	    nextchar = UCHARAT(locinput);
	    break;

	case NOTHING:
	    break;
	case BACK:
	    break;
	case OPEN:
	    n = ARG1(scan);  /* which paren pair */
	    regstartp[n] = locinput;
	    if (n > regsize)
		regsize = n;
	    break;
	case CLOSE:
	    n = ARG1(scan);  /* which paren pair */
	    regendp[n] = locinput;
	    if (n > *reglastparen)
		*reglastparen = n;
	    break;
	case CURLYX: {
		CURCUR cc;
		CHECKPOINT cp = savestack_ix;
		cc.oldcc = regcc;
		regcc = &cc;
		cc.parenfloor = *reglastparen;
		cc.cur = -1;
		cc.min = ARG1(scan);
		cc.max  = ARG2(scan);
		cc.scan = NEXTOPER(scan) + 4;
		cc.next = next;
		cc.minmod = minmod;
		cc.lastloc = 0;
		reginput = locinput;
		n = regmatch(PREVOPER(next));	/* start on the WHILEM */
		regcpblow(cp);
		regcc = cc.oldcc;
		saySAME(n);
	    }
	    /* NOT REACHED */
	case WHILEM: {
		/*
		 * This is really hard to understand, because after we match
		 * what we're trying to match, we must make sure the rest of
		 * the RE is going to match for sure, and to do that we have
		 * to go back UP the parse tree by recursing ever deeper.  And
		 * if it fails, we have to reset our parent's current state
		 * that we can try again after backing off.
		 */

		CHECKPOINT cp;
		CURCUR* cc = regcc;
		n = cc->cur + 1;	/* how many we know we matched */
		reginput = locinput;

#ifdef DEBUGGING
		if (regnarrate)
		    PerlIO_printf(Perl_debug_log, "%*s  %ld  %lx\n", regindent*2, "",
			(long)n, (long)cc);
#endif

		/* If degenerate scan matches "", assume scan done. */

		if (locinput == cc->lastloc && n >= cc->min) {
		    regcc = cc->oldcc;
		    ln = regcc->cur;
		    if (regmatch(cc->next))
			sayYES;
		    regcc->cur = ln;
		    regcc = cc;
		    sayNO;
		}

		/* First just match a string of min scans. */

		if (n < cc->min) {
		    cc->cur = n;
		    cc->lastloc = locinput;
		    if (regmatch(cc->scan))
			sayYES;
		    cc->cur = n - 1;
		    sayNO;
		}

		/* Prefer next over scan for minimal matching. */

		if (cc->minmod) {
		    regcc = cc->oldcc;
		    ln = regcc->cur;
		    cp = regcppush(cc->parenfloor);
		    if (regmatch(cc->next)) {
			regcppartblow(cp);
			sayYES;	/* All done. */
		    }
		    regcppop();
		    regcc->cur = ln;
		    regcc = cc;

		    if (n >= cc->max)	/* Maximum greed exceeded? */
			sayNO;

		    /* Try scanning more and see if it helps. */
		    reginput = locinput;
		    cc->cur = n;
		    cc->lastloc = locinput;
		    cp = regcppush(cc->parenfloor);
		    if (regmatch(cc->scan)) {
			regcppartblow(cp);
			sayYES;
		    }
		    regcppop();
		    cc->cur = n - 1;
		    sayNO;
		}

		/* Prefer scan over next for maximal matching. */

		if (n < cc->max) {	/* More greed allowed? */
		    cp = regcppush(cc->parenfloor);
		    cc->cur = n;
		    cc->lastloc = locinput;
		    if (regmatch(cc->scan)) {
			regcppartblow(cp);
			sayYES;
		    }
		    regcppop();		/* Restore some previous $<digit>s? */
		    reginput = locinput;
		}

		/* Failed deeper matches of scan, so see if this one works. */
		regcc = cc->oldcc;
		ln = regcc->cur;
		if (regmatch(cc->next))
		    sayYES;
		regcc->cur = ln;
		regcc = cc;
		cc->cur = n - 1;
		sayNO;
	    }
	    /* NOT REACHED */
	case BRANCH: {
		if (OP(next) != BRANCH)	  /* No choice. */
		    next = NEXTOPER(scan);/* Avoid recursion. */
		else {
		    int lastparen = *reglastparen;
		    do {
			reginput = locinput;
			if (regmatch(NEXTOPER(scan)))
			    sayYES;
			for (n = *reglastparen; n > lastparen; n--)
			    regendp[n] = 0;
			*reglastparen = n;
			    
#ifdef REGALIGN
			/*SUPPRESS 560*/
			if (n = NEXT(scan))
			    scan += n;
			else
			    scan = NULL;
#else
			scan = regnext(scan);
#endif
		    } while (scan != NULL && OP(scan) == BRANCH);
		    sayNO;
		    /* NOTREACHED */
		}
	    }
	    break;
	case MINMOD:
	    minmod = 1;
	    break;
	case CURLY:
	    ln = ARG1(scan);  /* min to match */
	    n  = ARG2(scan);  /* max to match */
	    scan = NEXTOPER(scan) + 4;
	    goto repeat;
	case STAR:
	    ln = 0;
	    n = 32767;
	    scan = NEXTOPER(scan);
	    goto repeat;
	case PLUS:
	    /*
	    * Lookahead to avoid useless match attempts
	    * when we know what character comes next.
	    */
	    ln = 1;
	    n = 32767;
	    scan = NEXTOPER(scan);
	  repeat:
	    if (regkind[(U8)OP(next)] == EXACT) {
		c1 = UCHARAT(OPERAND(next) + 1);
		if (OP(next) == EXACTF)
		    c2 = fold[c1];
		else if (OP(next) == EXACTFL)
		    c2 = fold_locale[c1];
		else
		    c2 = c1;
	    }
	    else
		c1 = c2 = -1000;
	    reginput = locinput;
	    if (minmod) {
		minmod = 0;
		if (ln && regrepeat(scan, ln) < ln)
		    sayNO;
		while (n >= ln || (n == 32767 && ln > 0)) { /* ln overflow ? */
		    /* If it could work, try it. */
		    if (c1 == -1000 ||
			UCHARAT(reginput) == c1 ||
			UCHARAT(reginput) == c2)
		    {
			if (regmatch(next))
			    sayYES;
		    }
		    /* Couldn't or didn't -- back up. */
		    reginput = locinput + ln;
		    if (regrepeat(scan, 1)) {
			ln++;
			reginput = locinput + ln;
		    }
		    else
			sayNO;
		}
	    }
	    else {
		n = regrepeat(scan, n);
		if (ln < n && regkind[(U8)OP(next)] == EOL &&
		    (!multiline || OP(next) == SEOL))
		    ln = n;			/* why back off? */
		while (n >= ln) {
		    /* If it could work, try it. */
		    if (c1 == -1000 ||
			UCHARAT(reginput) == c1 ||
			UCHARAT(reginput) == c2)
		    {
			if (regmatch(next))
			    sayYES;
		    }
		    /* Couldn't or didn't -- back up. */
		    n--;
		    reginput = locinput + n;
		}
	    }
	    sayNO;
	case SUCCEED:
	case END:
	    reginput = locinput;	/* put where regtry can find it */
	    sayYES;			/* Success! */
	case IFMATCH:
	    reginput = locinput;
	    scan = NEXTOPER(scan);
	    if (!regmatch(scan))
		sayNO;
	    break;
	case UNLESSM:
	    reginput = locinput;
	    scan = NEXTOPER(scan);
	    if (regmatch(scan))
		sayNO;
	    break;
	default:
	    PerlIO_printf(PerlIO_stderr(), "%lx %d\n",
			  (unsigned long)scan, scan[1]);
	    FAIL("regexp memory corruption");
	}
	scan = next;
    }

    /*
    * We get here only if there's trouble -- normally "case END" is
    * the terminating point.
    */
    FAIL("corrupted regexp pointers");
    /*NOTREACHED*/
    sayNO;

yes:
#ifdef DEBUGGING
    regindent--;
#endif
    return 1;

no:
#ifdef DEBUGGING
    regindent--;
#endif
    return 0;
}

/*
 - regrepeat - repeatedly match something simple, report how many
 */
/*
 * [This routine now assumes that it will only match on things of length 1.
 * That was true before, but now we assume scan - reginput is the count,
 * rather than incrementing count on every character.]
 */
static I32
regrepeat(p, max)
char *p;
I32 max;
{
    register char *scan;
    register char *opnd;
    register I32 c;
    register char *loceol = regeol;

    scan = reginput;
    if (max != 32767 && max < loceol - scan)
      loceol = scan + max;
    opnd = OPERAND(p);
    switch (OP(p)) {
    case ANY:
	while (scan < loceol && *scan != '\n')
	    scan++;
	break;
    case SANY:
	scan = loceol;
	break;
    case EXACT:		/* length of string is 1 */
	c = UCHARAT(++opnd);
	while (scan < loceol && UCHARAT(scan) == c)
	    scan++;
	break;
    case EXACTF:	/* length of string is 1 */
	c = UCHARAT(++opnd);
	while (scan < loceol &&
	       (UCHARAT(scan) == c || UCHARAT(scan) == fold[c]))
	    scan++;
	break;
    case EXACTFL:	/* length of string is 1 */
	regtainted = TRUE;
	c = UCHARAT(++opnd);
	while (scan < loceol &&
	       (UCHARAT(scan) == c || UCHARAT(scan) == fold_locale[c]))
	    scan++;
	break;
    case ANYOF:
	while (scan < loceol && reginclass(opnd, *scan))
	    scan++;
	break;
    case ALNUM:
	while (scan < loceol && isALNUM(*scan))
	    scan++;
	break;
    case ALNUML:
	regtainted = TRUE;
	while (scan < loceol && isALNUM_LC(*scan))
	    scan++;
	break;
    case NALNUM:
	while (scan < loceol && !isALNUM(*scan))
	    scan++;
	break;
    case NALNUML:
	regtainted = TRUE;
	while (scan < loceol && !isALNUM_LC(*scan))
	    scan++;
	break;
    case SPACE:
	while (scan < loceol && isSPACE(*scan))
	    scan++;
	break;
    case SPACEL:
	regtainted = TRUE;
	while (scan < loceol && isSPACE_LC(*scan))
	    scan++;
	break;
    case NSPACE:
	while (scan < loceol && !isSPACE(*scan))
	    scan++;
	break;
    case NSPACEL:
	regtainted = TRUE;
	while (scan < loceol && !isSPACE_LC(*scan))
	    scan++;
	break;
    case DIGIT:
	while (scan < loceol && isDIGIT(*scan))
	    scan++;
	break;
    case NDIGIT:
	while (scan < loceol && !isDIGIT(*scan))
	    scan++;
	break;
    default:		/* Called on something of 0 width. */
	break;		/* So match right here or not at all. */
    }

    c = scan - reginput;
    reginput = scan;

    return(c);
}

/*
 - regclass - determine if a character falls into a character class
 */

static bool
reginclass(p, c)
register char *p;
register I32 c;
{
    char flags = *p;
    bool match = FALSE;

    c &= 0xFF;
    if (p[1 + (c >> 3)] & (1 << (c & 7)))
	match = TRUE;
    else if (flags & ANYOF_FOLD) {
	I32 cf;
	if (flags & ANYOF_LOCALE) {
	    regtainted = TRUE;
	    cf = fold_locale[c];
	}
	else
	    cf = fold[c];
	if (p[1 + (cf >> 3)] & (1 << (cf & 7)))
	    match = TRUE;
    }

    if (!match && (flags & ANYOF_ISA)) {
	regtainted = TRUE;

	if (((flags & ANYOF_ALNUML)  && isALNUM_LC(c))  ||
	    ((flags & ANYOF_NALNUML) && !isALNUM_LC(c)) ||
	    ((flags & ANYOF_SPACEL)  && isSPACE_LC(c))  ||
	    ((flags & ANYOF_NSPACEL) && !isSPACE_LC(c)))
	{
	    match = TRUE;
	}
    }

    return match ^ ((flags & ANYOF_INVERT) != 0);
}

/*
 - regnext - dig the "next" pointer out of a node
 *
 * [Note, when REGALIGN is defined there are two places in regmatch()
 * that bypass this code for speed.]
 */
char *
regnext(p)
register char *p;
{
    register I32 offset;

    if (p == &regdummy)
	return(NULL);

    offset = NEXT(p);
    if (offset == 0)
	return(NULL);

#ifdef REGALIGN
    return(p+offset);
#else
    if (OP(p) == BACK)
	return(p-offset);
    else
	return(p+offset);
#endif
}

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
 ****    Copyright (c) 1991-1994, Larry Wall
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

CHECKPOINT regcppush _((I32 parenfloor));
char * regcppop _((void));

CHECKPOINT
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

char*
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
    register I32 i;
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
    regnpar = prog->nparens;
    /* Check validity of program. */
    if (UCHARAT(prog->program) != MAGIC) {
	FAIL("corrupted regexp program");
    }

    if (prog->do_folding) {
	i = strend - startpos;
	New(1101,c,i+1,char);
	Copy(startpos, c, i+1, char);
	startpos = c;
	strend = startpos + i;
	for (s = startpos; s < strend; s++)
	    if (isUPPER(*s))
		*s = toLOWER(*s);
    }

    /* If there is a "must appear" string, look for it. */
    s = startpos;
    if (prog->regmust != Nullsv &&
	(!(prog->reganch & ROPT_ANCH)
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
    /*  [unless multiline is set] */
    if (prog->reganch & ROPT_ANCH) {
	if (regtry(prog, startpos))
	    goto got_it;
	else if (multiline || (prog->reganch & ROPT_IMPLICIT)) {
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
	    i = SvPVX(prog->regstart)[0];
	    while (s < strend) {
		if (*s == i) {
		    if (regtry(prog, s))
			goto got_it;
		    s++;
		    while (s < strend && *s == i)
			s++;
		}
		s++;
	    }
	}
	else if (SvPOK(prog->regstart) == 3) {
	    /* We know what string it must start with. */
	    while ((s = fbm_instr((unsigned char*)s,
	      (unsigned char*)strend, prog->regstart)) != NULL)
	    {
		if (regtry(prog, s))
		    goto got_it;
		s++;
	    }
	}
	else {
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
		i = UCHARAT(s);
		if (!(c[i >> 3] & (1 << (i&7)))) {
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
	case BOUND:
	    if (minlen)
		dontbother++,strend--;
	    if (s != startpos) {
		i = s[-1];
		tmp = isALNUM(i);
	    }
	    else
		tmp = isALNUM(regprev);	/* assume not alphanumeric */
	    while (s < strend) {
		i = *s;
		if (tmp != isALNUM(i)) {
		    tmp = !tmp;
		    if (regtry(prog, s))
			goto got_it;
		}
		s++;
	    }
	    if ((minlen || tmp) && regtry(prog,s))
		goto got_it;
	    break;
	case NBOUND:
	    if (minlen)
		dontbother++,strend--;
	    if (s != startpos) {
		i = s[-1];
		tmp = isALNUM(i);
	    }
	    else
		tmp = isALNUM(regprev);	/* assume not alphanumeric */
	    while (s < strend) {
		i = *s;
		if (tmp != isALNUM(i))
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
		i = *s;
		if (isALNUM(i)) {
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
		i = *s;
		if (!isALNUM(i)) {
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
    if ((!safebase && (prog->nparens || sawampersand)) || prog->do_folding) {
	i = strend - startpos + (stringarg - strbeg);
	if (safebase) {			/* no need for $digit later */
	    s = strbeg;
	    prog->subend = s+i;
	}
	else if (strbeg != prog->subbase) {
	    s = savepvn(strbeg,i);	/* so $digit will work later */
	    if (prog->subbase)
		Safefree(prog->subbase);
	    prog->subbeg = prog->subbase = s;
	    prog->subend = s+i;
	}
	else {
	    prog->subbeg = s = prog->subbase;
	    prog->subend = s+i;
	}
	s += (stringarg - strbeg);
	for (i = 0; i <= prog->nparens; i++) {
	    if (prog->endp[i]) {
		prog->startp[i] = s + (prog->startp[i] - startpos);
		prog->endp[i] = s + (prog->endp[i] - startpos);
	    }
	}
	if (prog->do_folding)
	    Safefree(startpos);
    }
    return 1;

phooey:
    if (prog->do_folding)
	Safefree(startpos);
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
    int minmod = 0;
#ifdef DEBUGGING
    static int regindent = 0;
    regindent++;
#endif

    nextchar = *locinput;
    scan = prog;
    while (scan != NULL) {
#ifdef DEBUGGING
#define sayYES goto yes
#define sayNO goto no
#define saySAME(x) if (x) goto yes; else goto no
	if (regnarrate) {
	    fprintf(stderr, "%*s%2d%-8.8s\t<%.10s>\n", regindent*2, "",
		scan - regprogram, regprop(scan), locinput);
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
	case GBOL:
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
	    nextchar = *++locinput;
	    break;
	case ANY:
	    if (!nextchar && locinput >= regeol || nextchar == '\n')
		sayNO;
	    nextchar = *++locinput;
	    break;
	case EXACTLY:
	    s = OPERAND(scan);
	    ln = *s++;
	    /* Inline the first character, for speed. */
	    if (*s != nextchar)
		sayNO;
	    if (regeol - locinput < ln)
		sayNO;
	    if (ln > 1 && bcmp(s, locinput, ln) != 0)
		sayNO;
	    locinput += ln;
	    nextchar = *locinput;
	    break;
	case ANYOF:
	    s = OPERAND(scan);
	    if (nextchar < 0)
		nextchar = UCHARAT(locinput);
	    if (s[nextchar >> 3] & (1 << (nextchar&7)))
		sayNO;
	    if (!nextchar && locinput >= regeol)
		sayNO;
	    nextchar = *++locinput;
	    break;
	case ALNUM:
	    if (!nextchar)
		sayNO;
	    if (!isALNUM(nextchar))
		sayNO;
	    nextchar = *++locinput;
	    break;
	case NALNUM:
	    if (!nextchar && locinput >= regeol)
		sayNO;
	    if (isALNUM(nextchar))
		sayNO;
	    nextchar = *++locinput;
	    break;
	case NBOUND:
	case BOUND:
	    if (locinput == regbol)	/* was last char in word? */
		ln = isALNUM(regprev);
	    else 
		ln = isALNUM(locinput[-1]);
	    n = isALNUM(nextchar); /* is next char in word? */
	    if ((ln == n) == (OP(scan) == BOUND))
		sayNO;
	    break;
	case SPACE:
	    if (!nextchar && locinput >= regeol)
		sayNO;
	    if (!isSPACE(nextchar))
		sayNO;
	    nextchar = *++locinput;
	    break;
	case NSPACE:
	    if (!nextchar)
		sayNO;
	    if (isSPACE(nextchar))
		sayNO;
	    nextchar = *++locinput;
	    break;
	case DIGIT:
	    if (!isDIGIT(nextchar))
		sayNO;
	    nextchar = *++locinput;
	    break;
	case NDIGIT:
	    if (!nextchar && locinput >= regeol)
		sayNO;
	    if (isDIGIT(nextchar))
		sayNO;
	    nextchar = *++locinput;
	    break;
	case REF:
	    n = ARG1(scan);  /* which paren pair */
	    s = regstartp[n];
	    if (!s)
		sayNO;
	    if (!regendp[n])
		sayNO;
	    if (s == regendp[n])
		break;
	    /* Inline the first character, for speed. */
	    if (*s != nextchar)
		sayNO;
	    ln = regendp[n] - s;
	    if (locinput + ln > regeol)
		sayNO;
	    if (ln > 1 && bcmp(s, locinput, ln) != 0)
		sayNO;
	    locinput += ln;
	    nextchar = *locinput;
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

		CURCUR* cc = regcc;
		n = cc->cur + 1;	/* how many we know we matched */
		reginput = locinput;

#ifdef DEBUGGING
		if (regnarrate)
		    fprintf(stderr, "%*s  %d  %lx\n", regindent*2, "",
			n, (long)cc);
#endif

		/* If degenerate scan matches "", assume scan done. */

		if (locinput == cc->lastloc) {
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
		    if (regmatch(cc->next))
			sayYES;	/* All done. */
		    regcc->cur = ln;
		    regcc = cc;

		    if (n >= cc->max)	/* Maximum greed exceeded? */
			sayNO;

		    /* Try scanning more and see if it helps. */
		    reginput = locinput;
		    cc->cur = n;
		    cc->lastloc = locinput;
		    if (regmatch(cc->scan))
			sayYES;
		    cc->cur = n - 1;
		    sayNO;
		}

		/* Prefer scan over next for maximal matching. */

		if (n < cc->max) {	/* More greed allowed? */
		    regcppush(cc->parenfloor);
		    cc->cur = n;
		    cc->lastloc = locinput;
		    if (regmatch(cc->scan))
			sayYES;
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
	    if (OP(next) == EXACTLY)
		nextchar = *(OPERAND(next)+1);
	    else
		nextchar = -1000;
	    reginput = locinput;
	    if (minmod) {
		minmod = 0;
		if (ln && regrepeat(scan, ln) < ln)
		    sayNO;
		while (n >= ln || (n == 32767 && ln > 0)) { /* ln overflow ? */
		    /* If it could work, try it. */
		    if (nextchar == -1000 || *reginput == nextchar)
			if (regmatch(next))
			    sayYES;
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
		    if (nextchar == -1000 || *reginput == nextchar)
			if (regmatch(next))
			    sayYES;
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
	    fprintf(stderr, "%x %d\n",(unsigned)scan,scan[1]);
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
    case EXACTLY:		/* length of string is 1 */
	opnd++;
	while (scan < loceol && *opnd == *scan)
	    scan++;
	break;
    case ANYOF:
	c = UCHARAT(scan);
	while (scan < loceol && !(opnd[c >> 3] & (1 << (c & 7)))) {
	    scan++;
	    c = UCHARAT(scan);
	}
	break;
    case ALNUM:
	while (scan < loceol && isALNUM(*scan))
	    scan++;
	break;
    case NALNUM:
	while (scan < loceol && !isALNUM(*scan))
	    scan++;
	break;
    case SPACE:
	while (scan < loceol && isSPACE(*scan))
	    scan++;
	break;
    case NSPACE:
	while (scan < loceol && !isSPACE(*scan))
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

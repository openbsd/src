/*    regcomp.c
 */

/*
 * "A fair jaw-cracker dwarf-language must be."  --Samwise Gamgee
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
#include "INTERN.h"
#include "regcomp.h"

#ifdef MSDOS
# if defined(BUGGY_MSC6)
 /* MSC 6.00A breaks on op/regexp.t test 85 unless we turn this off */
 # pragma optimize("a",off)
 /* But MSC 6.00A is happy with 'w', for aliases only across function calls*/
 # pragma optimize("w",on )
# endif /* BUGGY_MSC6 */
#endif /* MSDOS */

#ifndef STATIC
#define	STATIC	static
#endif

#define	ISMULT1(c)	((c) == '*' || (c) == '+' || (c) == '?')
#define	ISMULT2(s)	((*s) == '*' || (*s) == '+' || (*s) == '?' || \
	((*s) == '{' && regcurly(s)))
#ifdef atarist
#define	PERL_META	"^$.[()|?+*\\"
#else
#define	META	"^$.[()|?+*\\"
#endif

#ifdef SPSTART
#undef SPSTART		/* dratted cpp namespace... */
#endif
/*
 * Flags to be passed up and down.
 */
#define	WORST		0	/* Worst case. */
#define	HASWIDTH	0x1	/* Known never to match null string. */
#define	SIMPLE		0x2	/* Simple enough to be STAR/PLUS operand. */
#define	SPSTART		0x4	/* Starts with * or +. */
#define TRYAGAIN	0x8	/* Weeded out a declaration. */

/*
 * Forward declarations for pregcomp()'s friends.
 */

static char *reg _((I32, I32 *));
static char *reganode _((char, unsigned short));
static char *regatom _((I32 *));
static char *regbranch _((I32 *));
static void regc _((char));
static char *regclass _((void));
STATIC I32 regcurly _((char *));
static char *regnode _((char));
static char *regpiece _((I32 *));
static void reginsert _((char, char *));
static void regoptail _((char *, char *));
static void regset _((char *, I32));
static void regtail _((char *, char *));
static char* regwhite _((char *, char *));
static char* nextchar _((void));

/*
 - pregcomp - compile a regular expression into internal code
 *
 * We can't allocate space until we know how big the compiled form will be,
 * but we can't compile it (and thus know how big it is) until we've got a
 * place to put the code.  So we cheat:  we compile it twice, once with code
 * generation turned off and size counting turned on, and once "for real".
 * This also means that we don't allocate space until we are sure that the
 * thing really will compile successfully, and we never have to move the
 * code and thus invalidate pointers into it.  (Note that it has to be in
 * one piece because free() must be able to free it all.) [NB: not true in perl]
 *
 * Beware that the optimization-preparation code in here knows about some
 * of the structure of the compiled regexp.  [I'll say.]
 */
regexp *
pregcomp(exp,xend,pm)
char* exp;
char* xend;
PMOP* pm;
{
    register regexp *r;
    register char *scan;
    register SV *longish;
    SV *longest;
    register I32 len;
    register char *first;
    I32 flags;
    I32 backish;
    I32 backest;
    I32 curback;
    I32 minlen = 0;
    I32 sawplus = 0;
    I32 sawopen = 0;
#define MAX_REPEAT_DEPTH 12
    struct {
	char *opcode;
	I32 count;
    } repeat_stack[MAX_REPEAT_DEPTH];
    I32 repeat_depth = 0;
    I32 repeat_count = 1;	/* We start unmultiplied. */

    if (exp == NULL)
	croak("NULL regexp argument");

    regprecomp = savepvn(exp, xend - exp);
    regflags = pm->op_pmflags;
    regsawback = 0;

    /* First pass: determine size, legality. */
    regparse = exp;
    regxend = xend;
    regnaughty = 0;
    regnpar = 1;
    regsize = 0L;
    regcode = &regdummy;
    regc((char)MAGIC);
    if (reg(0, &flags) == NULL) {
	Safefree(regprecomp);
	regprecomp = Nullch;
	return(NULL);
    }

    /* Small enough for pointer-storage convention? */
    if (regsize >= 32767L)		/* Probably could be 65535L. */
	FAIL("regexp too big");

    /* Allocate space and initialize. */
    Newc(1001, r, sizeof(regexp) + (unsigned)regsize, char, regexp);
    if (r == NULL)
	FAIL("regexp out of space");
    r->prelen = xend - exp;
    r->precomp = regprecomp;
    r->subbeg = r->subbase = NULL;

    /* Second pass: emit code. */
    regparse = exp;
    regxend = xend;
    regnaughty = 0;
    regnpar = 1;
    regcode = r->program;
    regc((char)MAGIC);
    if (reg(0, &flags) == NULL)
	return(NULL);

    /* Dig out information for optimizations. */
    pm->op_pmflags = regflags;
    r->regstart = Nullsv;	/* Worst-case defaults. */
    r->reganch = 0;
    r->regmust = Nullsv;
    r->regback = -1;
    r->regstclass = Nullch;
    r->naughty = regnaughty >= 10;	/* Probably an expensive pattern. */
    scan = r->program+1;			/* First BRANCH. */
    if (OP(regnext(scan)) == END) {/* Only one top-level choice. */
	scan = NEXTOPER(scan);

	first = scan;
	while ((OP(first) == OPEN && (sawopen = 1)) ||
	    (OP(first) == BRANCH && OP(regnext(first)) != BRANCH) ||
	    (OP(first) == PLUS) ||
	    (OP(first) == MINMOD) ||
	    (regkind[(U8)OP(first)] == CURLY && ARG1(first) > 0) ) {
		if (OP(first) == PLUS)
		    sawplus = 1;
		else
		    first += regarglen[(U8)OP(first)];
		first = NEXTOPER(first);
	}

	/* Starting-point info. */
      again:
	if (OP(first) == EXACT) {
	    r->regstart = newSVpv(OPERAND(first)+1,*OPERAND(first));
	    if (SvCUR(r->regstart) > !sawstudy)
		fbm_compile(r->regstart);
	    (void)SvUPGRADE(r->regstart, SVt_PVBM);
	}
	else if (strchr(simple+2,OP(first)))
	    r->regstclass = first;
	else if (regkind[(U8)OP(first)] == BOUND ||
		 regkind[(U8)OP(first)] == NBOUND)
	    r->regstclass = first;
	else if (regkind[(U8)OP(first)] == BOL) {
	    r->reganch |= ROPT_ANCH_BOL;
	    first = NEXTOPER(first);
	    goto again;
	}
	else if (OP(first) == GPOS) {
	    r->reganch |= ROPT_ANCH_GPOS;
	    first = NEXTOPER(first);
	    goto again;
	}
	else if ((OP(first) == STAR &&
	    regkind[(U8)OP(NEXTOPER(first))] == ANY) &&
	    !(r->reganch & ROPT_ANCH) )
	{
	    /* turn .* into ^.* with an implied $*=1 */
	    r->reganch |= ROPT_ANCH_BOL | ROPT_IMPLICIT;
	    first = NEXTOPER(first);
	    goto again;
	}
	if (sawplus && (!sawopen || !regsawback))
	    r->reganch |= ROPT_SKIP;	/* x+ must match 1st of run */

	DEBUG_r(PerlIO_printf(Perl_debug_log, "first %d next %d offset %ld\n",
	   OP(first), OP(NEXTOPER(first)), (long)(first - scan)));
	/*
	* If there's something expensive in the r.e., find the
	* longest literal string that must appear and make it the
	* regmust.  Resolve ties in favor of later strings, since
	* the regstart check works with the beginning of the r.e.
	* and avoiding duplication strengthens checking.  Not a
	* strong reason, but sufficient in the absence of others.
	* [Now we resolve ties in favor of the earlier string if
	* it happens that curback has been invalidated, since the
	* earlier string may buy us something the later one won't.]
	*/
	longish = newSVpv("",0);
	longest = newSVpv("",0);
	len = 0;
	minlen = 0;
	curback = 0;
	backish = 0;
	backest = 0;
	while (OP(scan) != END) {
	    if (OP(scan) == BRANCH) {
		if (OP(regnext(scan)) == BRANCH) {
		    curback = -30000;
		    while (OP(scan) == BRANCH)
			scan = regnext(scan);
		}
		else	/* single branch is ok */
		    scan = NEXTOPER(scan);
		continue;
	    }
	    if (OP(scan) == UNLESSM) {
		curback = -30000;
		scan = regnext(scan);
		continue;
	    }
	    if (OP(scan) == EXACT) {
		char *t;

		first = scan;
		while ((t = regnext(scan)) && OP(t) == CLOSE)
		    scan = t;
		minlen += *OPERAND(first) * repeat_count;
		if (curback - backish == len) {
		    sv_catpvn(longish, OPERAND(first)+1,
			*OPERAND(first));
		    len += *OPERAND(first);
		    curback += *OPERAND(first);
		    first = regnext(scan);
		}
		else if (*OPERAND(first) >= len + (curback >= 0)) {
		    len = *OPERAND(first);
		    sv_setpvn(longish, OPERAND(first)+1,len);
		    backish = curback;
		    curback += len;
		    first = regnext(scan);
		}
		else
		    curback += *OPERAND(first);
	    }
	    else if (strchr(varies,OP(scan))) {
		int tcount;
		char *next;

		if (repeat_depth < MAX_REPEAT_DEPTH
		    && ((OP(scan) == PLUS
			 && (tcount = 1)
			 && (next = NEXTOPER(scan)))
			|| (regkind[(U8)OP(scan)] == CURLY
			    && (tcount = ARG1(scan))
			    && (next = NEXTOPER(scan)+4))))
		{
		    /* We treat (abc)+ as (abc)(abc)*. */

		    /* Mark the place to return back. */
		    repeat_stack[repeat_depth].opcode = regnext(scan);
		    repeat_stack[repeat_depth].count = repeat_count;
		    repeat_depth++;
		    repeat_count *= tcount;

		    /* Go deeper: */
		    scan = next;
		    continue;
		}
		else {
		    curback = -30000;
		    len = 0;
		    if (SvCUR(longish) > SvCUR(longest)) {
			sv_setsv(longest,longish);
			backest = backish;
		    }
		    sv_setpvn(longish,"",0);
		}
	    }
	    else if (strchr(simple,OP(scan))) {
		curback++;
		minlen += repeat_count;
		len = 0;
		if (SvCUR(longish) > SvCUR(longest)) {
		    sv_setsv(longest,longish);
		    backest = backish;
		}
		sv_setpvn(longish,"",0);
	    }
	    scan = regnext(scan);
	    if (!scan) {		/* Go up PLUS or CURLY. */
		if (!repeat_depth--)
		    croak("panic: re scan");
		scan = repeat_stack[repeat_depth].opcode;
		repeat_count = repeat_stack[repeat_depth].count;
		/* Need to submit the longest string found: */
		curback = -30000;
		len = 0;
		if (SvCUR(longish) > SvCUR(longest)) {
		    sv_setsv(longest,longish);
		    backest = backish;
		}
		sv_setpvn(longish,"",0);
	    }
	}

	/* Prefer earlier on tie, unless we can tail match latter */

	if (SvCUR(longish) + (first && regkind[(U8)OP(first)] == EOL)
		> SvCUR(longest))
	{
	    sv_setsv(longest,longish);
	    backest = backish;
	}
	else
	    sv_setpvn(longish,"",0);
	if (SvCUR(longest)
	    && (!r->regstart
		|| !fbm_instr((unsigned char*) SvPVX(r->regstart),
			      (unsigned char *) (SvPVX(r->regstart)
						 + SvCUR(r->regstart)),
			      longest)))
	{
	    r->regmust = longest;
	    if (backest < 0)
		backest = -1;
	    r->regback = backest;
	    if (SvCUR(longest) > !(sawstudy || 
				   (first && regkind[(U8)OP(first)] == EOL)))
		fbm_compile(r->regmust);
	    (void)SvUPGRADE(r->regmust, SVt_PVBM);
	    BmUSEFUL(r->regmust) = 100;
	    if (first && regkind[(U8)OP(first)] == EOL && SvCUR(longish))
		SvTAIL_on(r->regmust);
	}
	else {
	    SvREFCNT_dec(longest);
	    longest = Nullsv;
	}
	SvREFCNT_dec(longish);
    }

    r->nparens = regnpar - 1;
    r->minlen = minlen;
    Newz(1002, r->startp, regnpar, char*);
    Newz(1002, r->endp, regnpar, char*);
    DEBUG_r(regdump(r));
    return(r);
}

/*
 - reg - regular expression, i.e. main body or parenthesized thing
 *
 * Caller must absorb opening parenthesis.
 *
 * Combining parenthesis handling with the base level of regular expression
 * is a trifle forced, but the need to tie the tails of the branches to what
 * follows makes it hard to avoid.
 */
static char *
reg(paren, flagp)
I32 paren;			/* Parenthesized? */
I32 *flagp;
{
    register char *ret;
    register char *br;
    register char *ender = 0;
    register I32 parno = 0;
    I32 flags;

    *flagp = HASWIDTH;	/* Tentatively. */

    /* Make an OPEN node, if parenthesized. */
    if (paren) {
	if (*regparse == '?') {
	    regparse++;
	    paren = *regparse++;
	    ret = NULL;
	    switch (paren) {
	    case ':':
	    case '=':
	    case '!':
		break;
	    case '$':
	    case '@':
		croak("Sequence (?%c...) not implemented", (int)paren);
		break;
	    case '#':
		while (*regparse && *regparse != ')')
		    regparse++;
		if (*regparse != ')')
		    croak("Sequence (?#... not terminated");
		nextchar();
		*flagp = TRYAGAIN;
		return NULL;
            case 0:
                croak("Sequence (? incomplete");
                break;
	    default:
		--regparse;
		while (*regparse && strchr("iogcmsx", *regparse))
		    pmflag(&regflags, *regparse++);
		if (*regparse != ')')
		    croak("Sequence (?%c...) not recognized", *regparse);
		nextchar();
		*flagp = TRYAGAIN;
		return NULL;
	    }
	}
	else {
	    parno = regnpar;
	    regnpar++;
	    ret = reganode(OPEN, parno);
	}
    } else
	ret = NULL;

    /* Pick up the branches, linking them together. */
    br = regbranch(&flags);
    if (br == NULL)
	return(NULL);
    if (ret != NULL)
	regtail(ret, br);	/* OPEN -> first. */
    else
	ret = br;
    if (!(flags&HASWIDTH))
	*flagp &= ~HASWIDTH;
    *flagp |= flags&SPSTART;
    while (*regparse == '|') {
	nextchar();
	br = regbranch(&flags);
	if (br == NULL)
	    return(NULL);
	regtail(ret, br);	/* BRANCH -> BRANCH. */
	if (!(flags&HASWIDTH))
	    *flagp &= ~HASWIDTH;
	*flagp |= flags&SPSTART;
    }

    /* Make a closing node, and hook it on the end. */
    switch (paren) {
    case ':':
	ender = regnode(NOTHING);
	break;
    case 1:
	ender = reganode(CLOSE, parno);
	break;
    case '=':
    case '!':
	ender = regnode(SUCCEED);
	*flagp &= ~HASWIDTH;
	break;
    case 0:
	ender = regnode(END);
	break;
    }
    regtail(ret, ender);

    /* Hook the tails of the branches to the closing node. */
    for (br = ret; br != NULL; br = regnext(br))
	regoptail(br, ender);

    if (paren == '=') {
	reginsert(IFMATCH,ret);
	regtail(ret, regnode(NOTHING));
    }
    else if (paren == '!') {
	reginsert(UNLESSM,ret);
	regtail(ret, regnode(NOTHING));
    }

    /* Check for proper termination. */
    if (paren && (regparse >= regxend || *nextchar() != ')')) {
	FAIL("unmatched () in regexp");
    } else if (!paren && regparse < regxend) {
	if (*regparse == ')') {
	    FAIL("unmatched () in regexp");
	} else
	    FAIL("junk on end of regexp");	/* "Can't happen". */
	/* NOTREACHED */
    }

    return(ret);
}

/*
 - regbranch - one alternative of an | operator
 *
 * Implements the concatenation operator.
 */
static char *
regbranch(flagp)
I32 *flagp;
{
    register char *ret;
    register char *chain;
    register char *latest;
    I32 flags = 0;

    *flagp = WORST;		/* Tentatively. */

    ret = regnode(BRANCH);
    chain = NULL;
    regparse--;
    nextchar();
    while (regparse < regxend && *regparse != '|' && *regparse != ')') {
	flags &= ~TRYAGAIN;
	latest = regpiece(&flags);
	if (latest == NULL) {
	    if (flags & TRYAGAIN)
		continue;
	    return(NULL);
	}
	*flagp |= flags&HASWIDTH;
	if (chain == NULL)	/* First piece. */
	    *flagp |= flags&SPSTART;
	else {
	    regnaughty++;
	    regtail(chain, latest);
	}
	chain = latest;
    }
    if (chain == NULL)	/* Loop ran zero times. */
	(void) regnode(NOTHING);

    return(ret);
}

/*
 - regpiece - something followed by possible [*+?]
 *
 * Note that the branching code sequences used for ? and the general cases
 * of * and + are somewhat optimized:  they use the same NOTHING node as
 * both the endmarker for their branch list and the body of the last branch.
 * It might seem that this node could be dispensed with entirely, but the
 * endmarker role is not redundant.
 */
static char *
regpiece(flagp)
I32 *flagp;
{
    register char *ret;
    register char op;
    register char *next;
    I32 flags;
    char *origparse = regparse;
    char *maxpos;
    I32 min;
    I32 max = 32767;

    ret = regatom(&flags);
    if (ret == NULL) {
	if (flags & TRYAGAIN)
	    *flagp |= TRYAGAIN;
	return(NULL);
    }

    op = *regparse;
    if (op == '(' && regparse[1] == '?' && regparse[2] == '#') {
	while (op && op != ')')
	    op = *++regparse;
	if (op) {
	    nextchar();
	    op = *regparse;
	}
    }

    if (op == '{' && regcurly(regparse)) {
	next = regparse + 1;
	maxpos = Nullch;
	while (isDIGIT(*next) || *next == ',') {
	    if (*next == ',') {
		if (maxpos)
		    break;
		else
		    maxpos = next;
	    }
	    next++;
	}
	if (*next == '}') {		/* got one */
	    if (!maxpos)
		maxpos = next;
	    regparse++;
	    min = atoi(regparse);
	    if (*maxpos == ',')
		maxpos++;
	    else
		maxpos = regparse;
	    max = atoi(maxpos);
	    if (!max && *maxpos != '0')
		max = 32767;		/* meaning "infinity" */
	    regparse = next;
	    nextchar();

	do_curly:
	    if ((flags&SIMPLE)) {
		regnaughty += 2 + regnaughty / 2;
		reginsert(CURLY, ret);
	    }
	    else {
		regnaughty += 4 + regnaughty;	/* compound interest */
		regtail(ret, regnode(WHILEM));
		reginsert(CURLYX,ret);
		regtail(ret, regnode(NOTHING));
	    }

	    if (min > 0)
		*flagp = (WORST|HASWIDTH);
	    if (max && max < min)
		croak("Can't do {n,m} with n > m");
	    if (regcode != &regdummy) {
#ifdef REGALIGN
		*(unsigned short *)(ret+3) = min;
		*(unsigned short *)(ret+5) = max;
#else
		ret[3] = min >> 8; ret[4] = min & 0377;
		ret[5] = max  >> 8; ret[6] = max  & 0377;
#endif
	    }

	    goto nest_check;
	}
    }

    if (!ISMULT1(op)) {
	*flagp = flags;
	return(ret);
    }

    if (!(flags&HASWIDTH) && op != '?')
      FAIL("regexp *+ operand could be empty"); /* else may core dump */

    nextchar();

    *flagp = (op != '+') ? (WORST|SPSTART) : (WORST|HASWIDTH);

    if (op == '*' && (flags&SIMPLE)) {
	reginsert(STAR, ret);
	regnaughty += 4;
    }
    else if (op == '*') {
	min = 0;
	goto do_curly;
    } else if (op == '+' && (flags&SIMPLE)) {
	reginsert(PLUS, ret);
	regnaughty += 3;
    }
    else if (op == '+') {
	min = 1;
	goto do_curly;
    } else if (op == '?') {
	min = 0; max = 1;
	goto do_curly;
    }
  nest_check:
    if (dowarn && regcode != &regdummy && !(flags&HASWIDTH) && max > 10000) {
	warn("%.*s matches null string many times",
	    regparse - origparse, origparse);
    }

    if (*regparse == '?') {
	nextchar();
	reginsert(MINMOD, ret);
#ifdef REGALIGN
	regtail(ret, ret + 4);
#else
	regtail(ret, ret + 3);
#endif
    }
    if (ISMULT2(regparse))
	FAIL("nested *?+ in regexp");

    return(ret);
}

/*
 - regatom - the lowest level
 *
 * Optimization:  gobbles an entire sequence of ordinary characters so that
 * it can turn them into a single node, which is smaller to store and
 * faster to run.  Backslashed characters are exceptions, each becoming a
 * separate node; the code is simpler that way and it's not worth fixing.
 *
 * [Yes, it is worth fixing, some scripts can run twice the speed.]
 */
static char *
regatom(flagp)
I32 *flagp;
{
    register char *ret = 0;
    I32 flags;

    *flagp = WORST;		/* Tentatively. */

tryagain:
    switch (*regparse) {
    case '^':
	nextchar();
	if (regflags & PMf_MULTILINE)
	    ret = regnode(MBOL);
	else if (regflags & PMf_SINGLELINE)
	    ret = regnode(SBOL);
	else
	    ret = regnode(BOL);
	break;
    case '$':
	nextchar();
	if (regflags & PMf_MULTILINE)
	    ret = regnode(MEOL);
	else if (regflags & PMf_SINGLELINE)
	    ret = regnode(SEOL);
	else
	    ret = regnode(EOL);
	break;
    case '.':
	nextchar();
	if (regflags & PMf_SINGLELINE)
	    ret = regnode(SANY);
	else
	    ret = regnode(ANY);
	regnaughty++;
	*flagp |= HASWIDTH|SIMPLE;
	break;
    case '[':
	regparse++;
	ret = regclass();
	*flagp |= HASWIDTH|SIMPLE;
	break;
    case '(':
	nextchar();
	ret = reg(1, &flags);
	if (ret == NULL) {
		if (flags & TRYAGAIN)
		    goto tryagain;
		return(NULL);
	}
	*flagp |= flags&(HASWIDTH|SPSTART);
	break;
    case '|':
    case ')':
	if (flags & TRYAGAIN) {
	    *flagp |= TRYAGAIN;
	    return NULL;
	}
	croak("internal urp in regexp at /%s/", regparse);
				/* Supposed to be caught earlier. */
	break;
    case '{':
	if (!regcurly(regparse)) {
	    regparse++;
	    goto defchar;
	}
	/* FALL THROUGH */
    case '?':
    case '+':
    case '*':
	FAIL("?+*{} follows nothing in regexp");
	break;
    case '\\':
	switch (*++regparse) {
	case 'A':
	    ret = regnode(SBOL);
	    *flagp |= SIMPLE;
	    nextchar();
	    break;
	case 'G':
	    ret = regnode(GPOS);
	    *flagp |= SIMPLE;
	    nextchar();
	    break;
	case 'Z':
	    ret = regnode(SEOL);
	    *flagp |= SIMPLE;
	    nextchar();
	    break;
	case 'w':
	    ret = regnode((regflags & PMf_LOCALE) ? ALNUML : ALNUM);
	    *flagp |= HASWIDTH|SIMPLE;
	    nextchar();
	    break;
	case 'W':
	    ret = regnode((regflags & PMf_LOCALE) ? NALNUML : NALNUM);
	    *flagp |= HASWIDTH|SIMPLE;
	    nextchar();
	    break;
	case 'b':
	    ret = regnode((regflags & PMf_LOCALE) ? BOUNDL : BOUND);
	    *flagp |= SIMPLE;
	    nextchar();
	    break;
	case 'B':
	    ret = regnode((regflags & PMf_LOCALE) ? NBOUNDL : NBOUND);
	    *flagp |= SIMPLE;
	    nextchar();
	    break;
	case 's':
	    ret = regnode((regflags & PMf_LOCALE) ? SPACEL : SPACE);
	    *flagp |= HASWIDTH|SIMPLE;
	    nextchar();
	    break;
	case 'S':
	    ret = regnode((regflags & PMf_LOCALE) ? NSPACEL : NSPACE);
	    *flagp |= HASWIDTH|SIMPLE;
	    nextchar();
	    break;
	case 'd':
	    ret = regnode(DIGIT);
	    *flagp |= HASWIDTH|SIMPLE;
	    nextchar();
	    break;
	case 'D':
	    ret = regnode(NDIGIT);
	    *flagp |= HASWIDTH|SIMPLE;
	    nextchar();
	    break;
	case 'n':
	case 'r':
	case 't':
	case 'f':
	case 'e':
	case 'a':
	case 'x':
	case 'c':
	case '0':
	    goto defchar;
	case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	    {
		I32 num = atoi(regparse);

		if (num > 9 && num >= regnpar)
		    goto defchar;
		else {
		    regsawback = 1;
		    ret = reganode((regflags & PMf_FOLD)
				   ? ((regflags & PMf_LOCALE) ? REFFL : REFF)
				   : REF, num);
		    *flagp |= HASWIDTH;
		    while (isDIGIT(*regparse))
			regparse++;
		    regparse--;
		    nextchar();
		}
	    }
	    break;
	case '\0':
	    if (regparse >= regxend)
		FAIL("trailing \\ in regexp");
	    /* FALL THROUGH */
	default:
	    goto defchar;
	}
	break;

    case '#':
	if (regflags & PMf_EXTENDED) {
	    while (regparse < regxend && *regparse != '\n') regparse++;
	    if (regparse < regxend)
		goto tryagain;
	}
	/* FALL THROUGH */

    default: {
	    register I32 len;
	    register char ender;
	    register char *p;
	    char *oldp;
	    I32 numlen;

	    regparse++;

	defchar:
	    ret = regnode((regflags & PMf_FOLD)
			  ? ((regflags & PMf_LOCALE) ? EXACTFL : EXACTF)
			  : EXACT);
	    regc(0);		/* save spot for len */
	    for (len = 0, p = regparse - 1;
	      len < 127 && p < regxend;
	      len++)
	    {
		oldp = p;

		if (regflags & PMf_EXTENDED)
		    p = regwhite(p, regxend);
		switch (*p) {
		case '^':
		case '$':
		case '.':
		case '[':
		case '(':
		case ')':
		case '|':
		    goto loopdone;
		case '\\':
		    switch (*++p) {
		    case 'A':
		    case 'G':
		    case 'Z':
		    case 'w':
		    case 'W':
		    case 'b':
		    case 'B':
		    case 's':
		    case 'S':
		    case 'd':
		    case 'D':
			--p;
			goto loopdone;
		    case 'n':
			ender = '\n';
			p++;
			break;
		    case 'r':
			ender = '\r';
			p++;
			break;
		    case 't':
			ender = '\t';
			p++;
			break;
		    case 'f':
			ender = '\f';
			p++;
			break;
		    case 'e':
			ender = '\033';
			p++;
			break;
		    case 'a':
			ender = '\007';
			p++;
			break;
		    case 'x':
			ender = scan_hex(++p, 2, &numlen);
			p += numlen;
			break;
		    case 'c':
			p++;
			ender = UCHARAT(p++);
			ender = toCTRL(ender);
			break;
		    case '0': case '1': case '2': case '3':case '4':
		    case '5': case '6': case '7': case '8':case '9':
			if (*p == '0' ||
			  (isDIGIT(p[1]) && atoi(p) >= regnpar) ) {
			    ender = scan_oct(p, 3, &numlen);
			    p += numlen;
			}
			else {
			    --p;
			    goto loopdone;
			}
			break;
		    case '\0':
			if (p >= regxend)
			    FAIL("trailing \\ in regexp");
			/* FALL THROUGH */
		    default:
			ender = *p++;
			break;
		    }
		    break;
		default:
		    ender = *p++;
		    break;
		}
		if (regflags & PMf_EXTENDED)
		    p = regwhite(p, regxend);
		if (ISMULT2(p)) { /* Back off on ?+*. */
		    if (len)
			p = oldp;
		    else {
			len++;
			regc(ender);
		    }
		    break;
		}
		regc(ender);
	    }
	loopdone:
	    regparse = p - 1;
	    nextchar();
	    if (len < 0)
		FAIL("internal disaster in regexp");
	    if (len > 0)
		*flagp |= HASWIDTH;
	    if (len == 1)
		*flagp |= SIMPLE;
	    if (regcode != &regdummy)
		*OPERAND(ret) = len;
	    regc('\0');
	}
	break;
    }

    return(ret);
}

static char *
regwhite(p, e)
char *p;
char *e;
{
    while (p < e) {
	if (isSPACE(*p))
	    ++p;
	else if (*p == '#') {
	    do {
		p++;
	    } while (p < e && *p != '\n');
	}
	else
	    break;
    }
    return p;
}

static void
regset(opnd, c)
char *opnd;
register I32 c;
{
    if (opnd == &regdummy)
	return;
    c &= 0xFF;
    opnd[1 + (c >> 3)] |= (1 << (c & 7));
}

static char *
regclass()
{
    register char *opnd;
    register I32 class;
    register I32 lastclass = 1234;
    register I32 range = 0;
    register char *ret;
    register I32 def;
    I32 numlen;

    ret = regnode(ANYOF);
    opnd = regcode;
    for (class = 0; class < 33; class++)
	regc(0);
    if (*regparse == '^') {	/* Complement of range. */
	regnaughty++;
	regparse++;
	if (opnd != &regdummy)
	    *opnd |= ANYOF_INVERT;
    }
    if (opnd != &regdummy) {
	if (regflags & PMf_FOLD)
	    *opnd |= ANYOF_FOLD;
	if (regflags & PMf_LOCALE)
	    *opnd |= ANYOF_LOCALE;
    }
    if (*regparse == ']' || *regparse == '-')
	goto skipcond;		/* allow 1st char to be ] or - */
    while (regparse < regxend && *regparse != ']') {
       skipcond:
	class = UCHARAT(regparse++);
	if (class == '\\') {
	    class = UCHARAT(regparse++);
	    switch (class) {
	    case 'w':
		if (regflags & PMf_LOCALE) {
		    if (opnd != &regdummy)
			*opnd |= ANYOF_ALNUML;
		}
		else {
		    for (class = 0; class < 256; class++)
			if (isALNUM(class))
			    regset(opnd, class);
		}
		lastclass = 1234;
		continue;
	    case 'W':
		if (regflags & PMf_LOCALE) {
		    if (opnd != &regdummy)
			*opnd |= ANYOF_NALNUML;
		}
		else {
		    for (class = 0; class < 256; class++)
			if (!isALNUM(class))
			    regset(opnd, class);
		}
		lastclass = 1234;
		continue;
	    case 's':
		if (regflags & PMf_LOCALE) {
		    if (opnd != &regdummy)
			*opnd |= ANYOF_SPACEL;
		}
		else {
		    for (class = 0; class < 256; class++)
			if (isSPACE(class))
			    regset(opnd, class);
		}
		lastclass = 1234;
		continue;
	    case 'S':
		if (regflags & PMf_LOCALE) {
		    if (opnd != &regdummy)
			*opnd |= ANYOF_NSPACEL;
		}
		else {
		    for (class = 0; class < 256; class++)
			if (!isSPACE(class))
			    regset(opnd, class);
		}
		lastclass = 1234;
		continue;
	    case 'd':
		for (class = '0'; class <= '9'; class++)
		    regset(opnd, class);
		lastclass = 1234;
		continue;
	    case 'D':
		for (class = 0; class < '0'; class++)
		    regset(opnd, class);
		for (class = '9' + 1; class < 256; class++)
		    regset(opnd, class);
		lastclass = 1234;
		continue;
	    case 'n':
		class = '\n';
		break;
	    case 'r':
		class = '\r';
		break;
	    case 't':
		class = '\t';
		break;
	    case 'f':
		class = '\f';
		break;
	    case 'b':
		class = '\b';
		break;
	    case 'e':
		class = '\033';
		break;
	    case 'a':
		class = '\007';
		break;
	    case 'x':
		class = scan_hex(regparse, 2, &numlen);
		regparse += numlen;
		break;
	    case 'c':
		class = UCHARAT(regparse++);
		class = toCTRL(class);
		break;
	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9':
		class = scan_oct(--regparse, 3, &numlen);
		regparse += numlen;
		break;
	    }
	}
	if (range) {
	    if (lastclass > class)
		FAIL("invalid [] range in regexp");
	    range = 0;
	}
	else {
	    lastclass = class;
	    if (*regparse == '-' && regparse+1 < regxend &&
	      regparse[1] != ']') {
		regparse++;
		range = 1;
		continue;	/* do it next time */
	    }
	}
	for ( ; lastclass <= class; lastclass++)
	    regset(opnd, lastclass);
	lastclass = class;
    }
    if (*regparse != ']')
	FAIL("unmatched [] in regexp");
    nextchar();
    return ret;
}

static char*
nextchar()
{
    char* retval = regparse++;

    for (;;) {
	if (*regparse == '(' && regparse[1] == '?' &&
		regparse[2] == '#') {
	    while (*regparse && *regparse != ')')
		regparse++;
	    regparse++;
	    continue;
	}
	if (regflags & PMf_EXTENDED) {
	    if (isSPACE(*regparse)) {
		regparse++;
		continue;
	    }
	    else if (*regparse == '#') {
		while (*regparse && *regparse != '\n')
		    regparse++;
		regparse++;
		continue;
	    }
	}
	return retval;
    }
}

/*
- regnode - emit a node
*/
#ifdef CAN_PROTOTYPE
static char *			/* Location. */
regnode(char op)
#else
static char *			/* Location. */
regnode(op)
char op;
#endif
{
    register char *ret;
    register char *ptr;

    ret = regcode;
    if (ret == &regdummy) {
#ifdef REGALIGN
	if (!(regsize & 1))
	    regsize++;
#endif
	regsize += 3;
	return(ret);
    }

#ifdef REGALIGN
#ifndef lint
    if (!((long)ret & 1))
      *ret++ = 127;
#endif
#endif
    ptr = ret;
    *ptr++ = op;
    *ptr++ = '\0';		/* Null "next" pointer. */
    *ptr++ = '\0';
    regcode = ptr;

    return(ret);
}

/*
- reganode - emit a node with an argument
*/
#ifdef CAN_PROTOTYPE
static char *			/* Location. */
reganode(char op, unsigned short arg)
#else
static char *			/* Location. */
reganode(op, arg)
char op;
unsigned short arg;
#endif
{
    register char *ret;
    register char *ptr;

    ret = regcode;
    if (ret == &regdummy) {
#ifdef REGALIGN
	if (!(regsize & 1))
	    regsize++;
#endif
	regsize += 5;
	return(ret);
    }

#ifdef REGALIGN
#ifndef lint
    if (!((long)ret & 1))
      *ret++ = 127;
#endif
#endif
    ptr = ret;
    *ptr++ = op;
    *ptr++ = '\0';		/* Null "next" pointer. */
    *ptr++ = '\0';
#ifdef REGALIGN
    *(unsigned short *)(ret+3) = arg;
#else
    ret[3] = arg >> 8; ret[4] = arg & 0377;
#endif
    ptr += 2;
    regcode = ptr;

    return(ret);
}

/*
- regc - emit (if appropriate) a byte of code
*/
#ifdef CAN_PROTOTYPE
static void
regc(char b)
#else
static void
regc(b)
char b;
#endif
{
    if (regcode != &regdummy)
	*regcode++ = b;
    else
	regsize++;
}

/*
- reginsert - insert an operator in front of already-emitted operand
*
* Means relocating the operand.
*/
#ifdef CAN_PROTOTYPE
static void
reginsert(char op, char *opnd)
#else
static void
reginsert(op, opnd)
char op;
char *opnd;
#endif
{
    register char *src;
    register char *dst;
    register char *place;
    register int offset = (regkind[(U8)op] == CURLY ? 4 : 0);

    if (regcode == &regdummy) {
#ifdef REGALIGN
	regsize += 4 + offset;
#else
	regsize += 3 + offset;
#endif
	return;
    }

    src = regcode;
#ifdef REGALIGN
    regcode += 4 + offset;
#else
    regcode += 3 + offset;
#endif
    dst = regcode;
    while (src > opnd)
	*--dst = *--src;

    place = opnd;		/* Op node, where operand used to be. */
    *place++ = op;
    *place++ = '\0';
    *place++ = '\0';
    while (offset-- > 0)
	*place++ = '\0';
#ifdef REGALIGN
    *place++ = '\177';
#endif
}

/*
- regtail - set the next-pointer at the end of a node chain
*/
static void
regtail(p, val)
char *p;
char *val;
{
    register char *scan;
    register char *temp;
    register I32 offset;

    if (p == &regdummy)
	return;

    /* Find last node. */
    scan = p;
    for (;;) {
	temp = regnext(scan);
	if (temp == NULL)
	    break;
	scan = temp;
    }

#ifdef REGALIGN
    offset = val - scan;
#ifndef lint
    *(short*)(scan+1) = offset;
#else
    offset = offset;
#endif
#else
    if (OP(scan) == BACK)
	offset = scan - val;
    else
	offset = val - scan;
    *(scan+1) = (offset>>8)&0377;
    *(scan+2) = offset&0377;
#endif
}

/*
- regoptail - regtail on operand of first argument; nop if operandless
*/
static void
regoptail(p, val)
char *p;
char *val;
{
    /* "Operandless" and "op != BRANCH" are synonymous in practice. */
    if (p == NULL || p == &regdummy || regkind[(U8)OP(p)] != BRANCH)
	return;
    regtail(NEXTOPER(p), val);
}

/*
 - regcurly - a little FSA that accepts {\d+,?\d*}
 */
STATIC I32
regcurly(s)
register char *s;
{
    if (*s++ != '{')
	return FALSE;
    if (!isDIGIT(*s))
	return FALSE;
    while (isDIGIT(*s))
	s++;
    if (*s == ',')
	s++;
    while (isDIGIT(*s))
	s++;
    if (*s != '}')
	return FALSE;
    return TRUE;
}

#ifdef DEBUGGING

/*
 - regdump - dump a regexp onto Perl_debug_log in vaguely comprehensible form
 */
void
regdump(r)
regexp *r;
{
    register char *s;
    register char op = EXACT;	/* Arbitrary non-END op. */
    register char *next;
    SV *sv = sv_newmortal();

    s = r->program + 1;
    while (op != END) {	/* While that wasn't END last time... */
#ifdef REGALIGN
	if (!((long)s & 1))
	    s++;
#endif
	op = OP(s);
	/* where, what */
	regprop(sv, s);
	PerlIO_printf(Perl_debug_log, "%2ld%s", (long)(s - r->program), SvPVX(sv));
	next = regnext(s);
	s += regarglen[(U8)op];
	if (next == NULL)		/* Next ptr. */
	    PerlIO_printf(Perl_debug_log, "(0)");
	else 
	    PerlIO_printf(Perl_debug_log, "(%ld)", (long)(s-r->program)+(next-s));
	s += 3;
	if (op == ANYOF) {
	    s += 33;
	}
	if (regkind[(U8)op] == EXACT) {
	    /* Literal string, where present. */
	    s++;
	    (void)PerlIO_putc(Perl_debug_log, ' ');
	    (void)PerlIO_putc(Perl_debug_log, '<');
	    while (*s != '\0') {
		(void)PerlIO_putc(Perl_debug_log,*s);
		s++;
	    }
	    (void)PerlIO_putc(Perl_debug_log, '>');
	    s++;
	}
	(void)PerlIO_putc(Perl_debug_log, '\n');
    }

    /* Header fields of interest. */
    if (r->regstart)
	PerlIO_printf(Perl_debug_log, "start `%s' ", SvPVX(r->regstart));
    if (r->regstclass) {
	regprop(sv, r->regstclass);
	PerlIO_printf(Perl_debug_log, "stclass `%s' ", SvPVX(sv));
    }
    if (r->reganch & ROPT_ANCH) {
	PerlIO_printf(Perl_debug_log, "anchored");
	if (r->reganch & ROPT_ANCH_BOL)
	    PerlIO_printf(Perl_debug_log, "(BOL)");
	if (r->reganch & ROPT_ANCH_GPOS)
	    PerlIO_printf(Perl_debug_log, "(GPOS)");
	PerlIO_putc(Perl_debug_log, ' ');
    }
    if (r->reganch & ROPT_SKIP)
	PerlIO_printf(Perl_debug_log, "plus ");
    if (r->reganch & ROPT_IMPLICIT)
	PerlIO_printf(Perl_debug_log, "implicit ");
    if (r->regmust != NULL)
	PerlIO_printf(Perl_debug_log, "must have \"%s\" back %ld ", SvPVX(r->regmust),
	 (long) r->regback);
    PerlIO_printf(Perl_debug_log, "minlen %ld ", (long) r->minlen);
    PerlIO_printf(Perl_debug_log, "\n");
}

/*
- regprop - printable representation of opcode
*/
void
regprop(sv, op)
SV *sv;
char *op;
{
    register char *p = 0;

    sv_setpv(sv, ":");
    switch (OP(op)) {
    case BOL:
	p = "BOL";
	break;
    case MBOL:
	p = "MBOL";
	break;
    case SBOL:
	p = "SBOL";
	break;
    case EOL:
	p = "EOL";
	break;
    case MEOL:
	p = "MEOL";
	break;
    case SEOL:
	p = "SEOL";
	break;
    case ANY:
	p = "ANY";
	break;
    case SANY:
	p = "SANY";
	break;
    case ANYOF:
	p = "ANYOF";
	break;
    case BRANCH:
	p = "BRANCH";
	break;
    case EXACT:
	p = "EXACT";
	break;
    case EXACTF:
	p = "EXACTF";
	break;
    case EXACTFL:
	p = "EXACTFL";
	break;
    case NOTHING:
	p = "NOTHING";
	break;
    case BACK:
	p = "BACK";
	break;
    case END:
	p = "END";
	break;
    case BOUND:
	p = "BOUND";
	break;
    case BOUNDL:
	p = "BOUNDL";
	break;
    case NBOUND:
	p = "NBOUND";
	break;
    case NBOUNDL:
	p = "NBOUNDL";
	break;
    case CURLY:
	sv_catpvf(sv, "CURLY {%d,%d}", ARG1(op), ARG2(op));
	break;
    case CURLYX:
	sv_catpvf(sv, "CURLYX {%d,%d}", ARG1(op), ARG2(op));
	break;
    case REF:
	sv_catpvf(sv, "REF%d", ARG1(op));
	break;
    case REFF:
	sv_catpvf(sv, "REFF%d", ARG1(op));
	break;
    case REFFL:
	sv_catpvf(sv, "REFFL%d", ARG1(op));
	break;
    case OPEN:
	sv_catpvf(sv, "OPEN%d", ARG1(op));
	break;
    case CLOSE:
	sv_catpvf(sv, "CLOSE%d", ARG1(op));
	p = NULL;
	break;
    case STAR:
	p = "STAR";
	break;
    case PLUS:
	p = "PLUS";
	break;
    case MINMOD:
	p = "MINMOD";
	break;
    case GPOS:
	p = "GPOS";
	break;
    case UNLESSM:
	p = "UNLESSM";
	break;
    case IFMATCH:
	p = "IFMATCH";
	break;
    case SUCCEED:
	p = "SUCCEED";
	break;
    case WHILEM:
	p = "WHILEM";
	break;
    case DIGIT:
	p = "DIGIT";
	break;
    case NDIGIT:
	p = "NDIGIT";
	break;
    case ALNUM:
	p = "ALNUM";
	break;
    case NALNUM:
	p = "NALNUM";
	break;
    case SPACE:
	p = "SPACE";
	break;
    case NSPACE:
	p = "NSPACE";
	break;
    case ALNUML:
	p = "ALNUML";
	break;
    case NALNUML:
	p = "NALNUML";
	break;
    case SPACEL:
	p = "SPACEL";
	break;
    case NSPACEL:
	p = "NSPACEL";
	break;
    default:
	FAIL("corrupted regexp opcode");
    }
    if (p)
	sv_catpv(sv, p);
}
#endif /* DEBUGGING */

void
pregfree(r)
struct regexp *r;
{
    if (!r)
	return;
    if (r->precomp) {
	Safefree(r->precomp);
	r->precomp = Nullch;
    }
    if (r->subbase) {
	Safefree(r->subbase);
	r->subbase = Nullch;
    }
    if (r->regmust) {
	SvREFCNT_dec(r->regmust);
	r->regmust = Nullsv;
    }
    if (r->regstart) {
	SvREFCNT_dec(r->regstart);
	r->regstart = Nullsv;
    }
    Safefree(r->startp);
    Safefree(r->endp);
    Safefree(r);
}

/*	$OpenBSD: regexp.c,v 1.2 1996/09/21 06:23:17 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 * NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE
 *
 * This is NOT the original regular expression code as written by
 * Henry Spencer. This code has been modified specifically for use
 * with the VIM editor, and should not be used apart from compiling
 * VIM. If you want a good regular expression library, get the
 * original code. The copyright notice that follows is from the
 * original.
 *
 * NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE
 *
 *
 * vim_regcomp and vim_regexec -- vim_regsub and regerror are elsewhere
 *
 *		Copyright (c) 1986 by University of Toronto.
 *		Written by Henry Spencer.  Not derived from licensed software.
 *
 *		Permission is granted to anyone to use this software for any
 *		purpose on any computer system, and to redistribute it freely,
 *		subject to the following restrictions:
 *
 *		1. The author is not responsible for the consequences of use of
 *				this software, no matter how awful, even if they arise
 *				from defects in it.
 *
 *		2. The origin of this software must not be misrepresented, either
 *				by explicit claim or by omission.
 *
 *		3. Altered versions must be plainly marked as such, and must not
 *				be misrepresented as being the original software.
 *
 * Beware that some of this code is subtly aware of the way operator
 * precedence is structured in regular expressions.  Serious changes in
 * regular-expression syntax might require a total rethink.
 *
 * $Log: regexp.c,v $
 * Revision 1.2  1996/09/21 06:23:17  downsj
 * update to vim 4.4beta
 *
 * Revision 1.1.1.1  1996/09/07 21:40:25  downsj
 * Initial import of vim 4.2.
 *
 * This is meant to replace nvi in the tree.  Vim, in general, works better,
 * provides more features, and does not suffer from the license problems
 * being imposed upon nvi.
 *
 * On the other hand, vim lacks a non-visual ex mode, in addition to open mode.
 *
 * This includes the GUI (X11) code, but doesn't try to compile it.
 *
 * Revision 1.2  88/04/28  08:09:45  tony
 * First modification of the regexp library. Added an external variable
 * 'reg_ic' which can be set to indicate that case should be ignored.
 * Added a new parameter to vim_regexec() to indicate that the given string
 * comes from the beginning of a line and is thus eligible to match
 * 'beginning-of-line'.
 *
 * Revisions by Olaf 'Rhialto' Seibert, rhialto@mbfys.kun.nl:
 * Changes for vi: (the semantics of several things were rather different)
 * - Added lexical analyzer, because in vi magicness of characters
 *   is rather difficult, and may change over time.
 * - Added support for \< \> \1-\9 and ~
 * - Left some magic stuff in, but only backslashed: \| \+
 * - * and \+ still work after \) even though they shouldn't.
 */
#include "vim.h"
#include "globals.h"
#include "proto.h"
#undef DEBUG

#include <stdio.h>
#include "regexp.h"
#include "option.h"

/*
 * The "internal use only" fields in regexp.h are present to pass info from
 * compile to execute that permits the execute phase to run lots faster on
 * simple cases.  They are:
 *
 * regstart 	char that must begin a match; '\0' if none obvious
 * reganch		is the match anchored (at beginning-of-line only)?
 * regmust		string (pointer into program) that match must include, or NULL
 * regmlen		length of regmust string
 *
 * Regstart and reganch permit very fast decisions on suitable starting points
 * for a match, cutting down the work a lot.  Regmust permits fast rejection
 * of lines that cannot possibly match.  The regmust tests are costly enough
 * that vim_regcomp() supplies a regmust only if the r.e. contains something
 * potentially expensive (at present, the only such thing detected is * or +
 * at the start of the r.e., which can involve a lot of backup).  Regmlen is
 * supplied because the test in vim_regexec() needs it and vim_regcomp() is
 * computing it anyway.
 */

/*
 * Structure for regexp "program".	This is essentially a linear encoding
 * of a nondeterministic finite-state machine (aka syntax charts or
 * "railroad normal form" in parsing technology).  Each node is an opcode
 * plus a "next" pointer, possibly plus an operand.  "Next" pointers of
 * all nodes except BRANCH implement concatenation; a "next" pointer with
 * a BRANCH on both ends of it is connecting two alternatives.	(Here we
 * have one of the subtle syntax dependencies:	an individual BRANCH (as
 * opposed to a collection of them) is never concatenated with anything
 * because of operator precedence.)  The operand of some types of node is
 * a literal string; for others, it is a node leading into a sub-FSM.  In
 * particular, the operand of a BRANCH node is the first node of the branch.
 * (NB this is *not* a tree structure:	the tail of the branch connects
 * to the thing following the set of BRANCHes.)  The opcodes are:
 */

/* definition	number			   opnd?	meaning */
#define END 	0				/* no	End of program. */
#define BOL 	1				/* no	Match "" at beginning of line. */
#define EOL 	2				/* no	Match "" at end of line. */
#define ANY 	3				/* no	Match any one character. */
#define ANYOF	4				/* str	Match any character in this string. */
#define ANYBUT	5				/* str	Match any character not in this
								 *		string. */
#define BRANCH	6				/* node Match this alternative, or the
								 *		next... */
#define BACK	7				/* no	Match "", "next" ptr points backward. */
#define EXACTLY 8				/* str	Match this string. */
#define NOTHING 9				/* no	Match empty string. */
#define STAR	10				/* node Match this (simple) thing 0 or more
								 *		times. */
#define PLUS	11				/* node Match this (simple) thing 1 or more
								 *		times. */
#define BOW		12				/* no	Match "" after [^a-zA-Z0-9_] */
#define EOW		13				/* no	Match "" at    [^a-zA-Z0-9_] */
#define IDENT	14				/* no	Match identifier char */
#define WORD	15				/* no	Match keyword char */
#define FNAME   16				/* no	Match file name char */
#define PRINT	17				/* no	Match printable char */
#define SIDENT	18				/* no	Match identifier char but no digit */
#define SWORD	19				/* no	Match word char but no digit */
#define SFNAME	20				/* no	Match file name char but no digit */
#define SPRINT	21				/* no	Match printable char but no digit */
#define MOPEN	30				/* no	Mark this point in input as start of
								 *		#n. */
 /* MOPEN+1 is number 1, etc. */
#define MCLOSE	40				/* no	Analogous to MOPEN. */
#define BACKREF	50				/* node Match same string again \1-\9 */

#define Magic(x)	((x)|('\\'<<8))

/*
 * Opcode notes:
 *
 * BRANCH		The set of branches constituting a single choice are hooked
 *				together with their "next" pointers, since precedence prevents
 *				anything being concatenated to any individual branch.  The
 *				"next" pointer of the last BRANCH in a choice points to the
 *				thing following the whole choice.  This is also where the
 *				final "next" pointer of each individual branch points; each
 *				branch starts with the operand node of a BRANCH node.
 *
 * BACK 		Normal "next" pointers all implicitly point forward; BACK
 *				exists to make loop structures possible.
 *
 * STAR,PLUS	'=', and complex '*' and '+', are implemented as circular
 *				BRANCH structures using BACK.  Simple cases (one character
 *				per match) are implemented with STAR and PLUS for speed
 *				and to minimize recursive plunges.
 *				Note: We would like to use "\?" instead of "\=", but a "\?"
 *				can be part of a pattern to escape the special meaning of '?'
 *				at the end of the pattern in "?pattern?e".
 *
 * MOPEN,MCLOSE	...are numbered at compile time.
 */

/*
 * A node is one char of opcode followed by two chars of "next" pointer.
 * "Next" pointers are stored as two 8-bit pieces, high order first.  The
 * value is a positive offset from the opcode of the node containing it.
 * An operand, if any, simply follows the node.  (Note that much of the
 * code generation knows about this implicit relationship.)
 *
 * Using two bytes for the "next" pointer is vast overkill for most things,
 * but allows patterns to get big without disasters.
 */
#define OP(p)	(*(p))
#define NEXT(p) (((*((p)+1)&0377)<<8) + (*((p)+2)&0377))
#define OPERAND(p)		((p) + 3)

/*
 * Utility definitions.
 */
#ifndef CHARBITS
#define UCHARAT(p)		((int)*(unsigned char *)(p))
#else
#define UCHARAT(p)		((int)*(p)&CHARBITS)
#endif

#define EMSG_RETURN(m) { emsg(m); rc_did_emsg = TRUE; return NULL; }

static int ismult __ARGS((int));
static char_u *cstrchr __ARGS((char_u *, int));

	static int
ismult(c)
	int c;
{
	return (c == Magic('*') || c == Magic('+') || c == Magic('='));
}

/*
 * Flags to be passed up and down.
 */
#define HASWIDTH		01		/* Known never to match null string. */
#define SIMPLE			02		/* Simple enough to be STAR/PLUS operand. */
#define SPSTART 		04		/* Starts with * or +. */
#define WORST			0		/* Worst case. */

/*
 * The following allows empty REs, meaning "the same as the previous RE".
 * per the ed(1) manual.
 */
/* #define EMPTY_RE */			/* this is done outside of regexp */
#ifdef EMPTY_RE
char_u		   *reg_prev_re;
#endif

#define TILDE
#ifdef TILDE
char_u		   *reg_prev_sub;
#endif

/*
 * Global work variables for vim_regcomp().
 */

static char_u    *regparse;	/* Input-scan pointer. */
static int		regnpar;		/* () count. */
static char_u 	regdummy;
static char_u   *regcode;		/* Code-emit pointer; &regdummy = don't. */
static long 	regsize;		/* Code size. */
static char_u   **regendp;		/* Ditto for endp. */

/*
 * META contains all characters that may be magic, except '^' and '$'.
 * This depends on the configuration options TILDE, BACKREF.
 * (could be done simpler for compilers that know string concatenation)
 */

#ifdef TILDE
# ifdef BACKREF
       static char_u META[] = ".[()|=+*<>iIkKfFpP~123456789";
# else
       static char_u META[] = ".[()|=+*<>iIkKfFpP~";
# endif
#else
# ifdef BACKREF
       static char_u META[] = ".[()|=+*<>iIkKfFpP123456789";
# else
       static char_u META[] = ".[()|=+*<>iIkKfFpP";
# endif
#endif

/*
 * REGEXP_ABBR contains all characters which act as abbreviations after '\'.
 * These are:
 *	\r	- New line (CR).
 *	\t	- Tab (TAB).
 *	\e	- Escape (ESC).
 *	\b	- Backspace (Ctrl('H')).
 */
static char_u REGEXP_ABBR[] = "rteb";

/*
 * Forward declarations for vim_regcomp()'s friends.
 */
static void		initchr __ARGS((char_u *));
static int		getchr __ARGS((void));
static int		peekchr __ARGS((void));
#define PeekChr() curchr	/* shortcut only when last action was peekchr() */
static int 		curchr;
static void		skipchr __ARGS((void));
static void		ungetchr __ARGS((void));
static char_u    *reg __ARGS((int, int *));
static char_u    *regbranch __ARGS((int *));
static char_u    *regpiece __ARGS((int *));
static char_u    *regatom __ARGS((int *));
static char_u    *regnode __ARGS((int));
static char_u    *regnext __ARGS((char_u *));
static void 	regc __ARGS((int));
static void 	unregc __ARGS((void));
static void 	reginsert __ARGS((int, char_u *));
static void 	regtail __ARGS((char_u *, char_u *));
static void 	regoptail __ARGS((char_u *, char_u *));

#ifndef HAVE_STRCSPN
static size_t	strcspn __ARGS((const char_u *, const char_u *));
#endif

/*
 * skip past regular expression
 * stop at end of 'p' of where 'dirc' is found ('/', '?', etc)
 * take care of characters with a backslash in front of it
 * skip strings inside [ and ].
 */
	char_u *
skip_regexp(p, dirc)
	char_u	*p;
	int		dirc;
{
	int		in_range = FALSE;

	for (; p[0] != NUL; ++p)
	{
		if (p[0] == dirc && !in_range)		/* found end of regexp */
			break;
		if (!in_range && ((p[0] == '[' && p_magic) ||
								   (p[0] == '\\' && p[1] == '[' && !p_magic)))
		{
			in_range = TRUE;
			if (p[0] == '\\')
				++p;
								/* "[^]" and "[]" are not the end of a range */
			if (p[1] == '^')
				++p;
			if (p[1] == ']')
				++p;
		}
		else if (p[0] == '\\' && p[1] != NUL)
			++p;	/* skip next character */
		else if (p[0] == ']')
			in_range = FALSE;
	}
	return p;
}

/*
 - vim_regcomp - compile a regular expression into internal code
 *
 * We can't allocate space until we know how big the compiled form will be,
 * but we can't compile it (and thus know how big it is) until we've got a
 * place to put the code.  So we cheat:  we compile it twice, once with code
 * generation turned off and size counting turned on, and once "for real".
 * This also means that we don't allocate space until we are sure that the
 * thing really will compile successfully, and we never have to move the
 * code and thus invalidate pointers into it.  (Note that it has to be in
 * one piece because vim_free() must be able to free it all.)
 *
 * Beware that the optimization-preparation code in here knows about some
 * of the structure of the compiled regexp.
 */
	regexp		   *
vim_regcomp(exp)
	char_u		   *exp;
{
	register regexp *r;
	register char_u  *scan;
	register char_u  *longest;
	register int	len;
	int 			flags;
/*	extern char    *malloc();*/

	if (exp == NULL)
		EMSG_RETURN(e_null);

#ifdef EMPTY_RE			/* this is done outside of regexp */
	if (*exp == '\0')
	{
		if (reg_prev_re)
			exp = reg_prev_re;
		else
			EMSG_RETURN(e_noprevre);
	}
#endif

	/* First pass: determine size, legality. */
	initchr((char_u *)exp);
	regnpar = 1;
	regsize = 0L;
	regcode = &regdummy;
	regendp = NULL;
	regc(MAGIC);
	if (reg(0, &flags) == NULL)
		return NULL;

	/* Small enough for pointer-storage convention? */
	if (regsize >= 32767L)		/* Probably could be 65535L. */
		EMSG_RETURN(e_toolong);

	/* Allocate space. */
/*	r = (regexp *) malloc((unsigned) (sizeof(regexp) + regsize));*/
	r = (regexp *) alloc((unsigned) (sizeof(regexp) + regsize));
	if (r == NULL)
		return NULL;

#ifdef EMPTY_RE			/* this is done outside of regexp */
	if (exp != reg_prev_re) {
		vim_free(reg_prev_re);
		if (reg_prev_re = alloc(STRLEN(exp) + 1))
			STRCPY(reg_prev_re, exp);
	}
#endif

	/* Second pass: emit code. */
	initchr((char_u *)exp);
	regnpar = 1;
	regcode = r->program;
	regendp = r->endp;
	regc(MAGIC);
	if (reg(0, &flags) == NULL) {
		vim_free(r);
		return NULL;
	}

	/* Dig out information for optimizations. */
	r->regstart = '\0'; 		/* Worst-case defaults. */
	r->reganch = 0;
	r->regmust = NULL;
	r->regmlen = 0;
	scan = r->program + 1;		/* First BRANCH. */
	if (OP(regnext(scan)) == END) { 	/* Only one top-level choice. */
		scan = OPERAND(scan);

		/* Starting-point info. */
		if (OP(scan) == EXACTLY)
			r->regstart = *OPERAND(scan);
		else if (OP(scan) == BOL)
			r->reganch++;

		/*
		 * If there's something expensive in the r.e., find the longest
		 * literal string that must appear and make it the regmust.  Resolve
		 * ties in favor of later strings, since the regstart check works
		 * with the beginning of the r.e. and avoiding duplication
		 * strengthens checking.  Not a strong reason, but sufficient in the
		 * absence of others.
		 */
		/*
		 * When the r.e. starts with BOW, it is faster to look for a regmust
		 * first. Used a lot for "#" and "*" commands. (Added by mool).
		 */
		if (flags & SPSTART || OP(scan) == BOW) {
			longest = NULL;
			len = 0;
			for (; scan != NULL; scan = regnext(scan))
				if (OP(scan) == EXACTLY && STRLEN(OPERAND(scan)) >= (size_t)len) {
					longest = OPERAND(scan);
					len = STRLEN(OPERAND(scan));
				}
			r->regmust = longest;
			r->regmlen = len;
		}
	}
#ifdef DEBUG
	regdump(r);
#endif
	return r;
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
	static char_u *
reg(paren, flagp)
	int 			paren;		/* Parenthesized? */
	int 		   *flagp;
{
	register char_u  *ret;
	register char_u  *br;
	register char_u  *ender;
	register int	parno = 0;
	int 			flags;

	*flagp = HASWIDTH;			/* Tentatively. */

	/* Make an MOPEN node, if parenthesized. */
	if (paren) {
		if (regnpar >= NSUBEXP)
			EMSG_RETURN(e_toombra);
		parno = regnpar;
		regnpar++;
		ret = regnode(MOPEN + parno);
		if (regendp)
			regendp[parno] = NULL;	/* haven't seen the close paren yet */
	} else
		ret = NULL;

	/* Pick up the branches, linking them together. */
	br = regbranch(&flags);
	if (br == NULL)
		return NULL;
	if (ret != NULL)
		regtail(ret, br);		/* MOPEN -> first. */
	else
		ret = br;
	if (!(flags & HASWIDTH))
		*flagp &= ~HASWIDTH;
	*flagp |= flags & SPSTART;
	while (peekchr() == Magic('|')) {
		skipchr();
		br = regbranch(&flags);
		if (br == NULL)
			return NULL;
		regtail(ret, br);		/* BRANCH -> BRANCH. */
		if (!(flags & HASWIDTH))
			*flagp &= ~HASWIDTH;
		*flagp |= flags & SPSTART;
	}

	/* Make a closing node, and hook it on the end. */
	ender = regnode((paren) ? MCLOSE + parno : END);
	regtail(ret, ender);

	/* Hook the tails of the branches to the closing node. */
	for (br = ret; br != NULL; br = regnext(br))
		regoptail(br, ender);

	/* Check for proper termination. */
	if (paren && getchr() != Magic(')'))
		EMSG_RETURN(e_toombra)
	else if (!paren && peekchr() != '\0')
	{
		if (PeekChr() == Magic(')'))
			EMSG_RETURN(e_toomket)
		else
			EMSG_RETURN(e_trailing)		/* "Can't happen". */
		/* NOTREACHED */
	}
	/*
	 * Here we set the flag allowing back references to this set of
	 * parentheses.
	 */
	if (paren && regendp)
		regendp[parno] = ender;	/* have seen the close paren */
	return ret;
}

/*
 - regbranch - one alternative of an | operator
 *
 * Implements the concatenation operator.
 */
	static char_u    *
regbranch(flagp)
	int 		   *flagp;
{
	register char_u  *ret;
	register char_u  *chain;
	register char_u  *latest;
	int 			flags;

	*flagp = WORST; 			/* Tentatively. */

	ret = regnode(BRANCH);
	chain = NULL;
	while (peekchr() != '\0' && PeekChr() != Magic('|') && PeekChr() != Magic(')')) {
		latest = regpiece(&flags);
		if (latest == NULL)
			return NULL;
		*flagp |= flags & HASWIDTH;
		if (chain == NULL)		/* First piece. */
			*flagp |= flags & SPSTART;
		else
			regtail(chain, latest);
		chain = latest;
	}
	if (chain == NULL)			/* Loop ran zero times. */
		(void) regnode(NOTHING);

	return ret;
}

/*
 - regpiece - something followed by possible [*+=]
 *
 * Note that the branching code sequences used for = and the general cases
 * of * and + are somewhat optimized:  they use the same NOTHING node as
 * both the endmarker for their branch list and the body of the last branch.
 * It might seem that this node could be dispensed with entirely, but the
 * endmarker role is not redundant.
 */
static char_u    *
regpiece(flagp)
	int 		   *flagp;
{
	register char_u  *ret;
	register int	op;
	register char_u  *next;
	int 			flags;

	ret = regatom(&flags);
	if (ret == NULL)
		return NULL;

	op = peekchr();
	if (!ismult(op)) {
		*flagp = flags;
		return ret;
	}
	if (!(flags & HASWIDTH) && op != Magic('='))
		EMSG_RETURN((char_u *)"*+ operand could be empty");
	*flagp = (op != Magic('+')) ? (WORST | SPSTART) : (WORST | HASWIDTH);

	if (op == Magic('*') && (flags & SIMPLE))
		reginsert(STAR, ret);
	else if (op == Magic('*')) {
		/* Emit x* as (x&|), where & means "self". */
		reginsert(BRANCH, ret); /* Either x */
		regoptail(ret, regnode(BACK));	/* and loop */
		regoptail(ret, ret);	/* back */
		regtail(ret, regnode(BRANCH));	/* or */
		regtail(ret, regnode(NOTHING)); /* null. */
	} else if (op == Magic('+') && (flags & SIMPLE))
		reginsert(PLUS, ret);
	else if (op == Magic('+')) {
		/* Emit x+ as x(&|), where & means "self". */
		next = regnode(BRANCH); /* Either */
		regtail(ret, next);
		regtail(regnode(BACK), ret);	/* loop back */
		regtail(next, regnode(BRANCH)); /* or */
		regtail(ret, regnode(NOTHING)); /* null. */
	} else if (op == Magic('=')) {
		/* Emit x= as (x|) */
		reginsert(BRANCH, ret); /* Either x */
		regtail(ret, regnode(BRANCH));	/* or */
		next = regnode(NOTHING);/* null. */
		regtail(ret, next);
		regoptail(ret, next);
	}
	skipchr();
	if (ismult(peekchr()))
		EMSG_RETURN((char_u *)"Nested *=+");

	return ret;
}

/*
 - regatom - the lowest level
 *
 * Optimization:  gobbles an entire sequence of ordinary characters so that
 * it can turn them into a single node, which is smaller to store and
 * faster to run.
 */
static char_u    *
regatom(flagp)
	int 		   *flagp;
{
	register char_u  *ret;
	int 			flags;

	*flagp = WORST; 			/* Tentatively. */

	switch (getchr()) {
	  case Magic('^'):
		ret = regnode(BOL);
		break;
	  case Magic('$'):
		ret = regnode(EOL);
		break;
	  case Magic('<'):
		ret = regnode(BOW);
		break;
	  case Magic('>'):
		ret = regnode(EOW);
		break;
	  case Magic('.'):
		ret = regnode(ANY);
		*flagp |= HASWIDTH | SIMPLE;
		break;
	  case Magic('i'):
	  	ret = regnode(IDENT);
		*flagp |= HASWIDTH | SIMPLE;
		break;
	  case Magic('k'):
	  	ret = regnode(WORD);
		*flagp |= HASWIDTH | SIMPLE;
		break;
	  case Magic('I'):
	  	ret = regnode(SIDENT);
		*flagp |= HASWIDTH | SIMPLE;
		break;
	  case Magic('K'):
	  	ret = regnode(SWORD);
		*flagp |= HASWIDTH | SIMPLE;
		break;
	  case Magic('f'):
	  	ret = regnode(FNAME);
		*flagp |= HASWIDTH | SIMPLE;
		break;
	  case Magic('F'):
	  	ret = regnode(SFNAME);
		*flagp |= HASWIDTH | SIMPLE;
		break;
	  case Magic('p'):
	  	ret = regnode(PRINT);
		*flagp |= HASWIDTH | SIMPLE;
		break;
	  case Magic('P'):
	  	ret = regnode(SPRINT);
		*flagp |= HASWIDTH | SIMPLE;
		break;
	  case Magic('('):
		ret = reg(1, &flags);
		if (ret == NULL)
			return NULL;
		*flagp |= flags & (HASWIDTH | SPSTART);
		break;
	  case '\0':
	  case Magic('|'):
	  case Magic(')'):
		EMSG_RETURN(e_internal)		/* Supposed to be caught earlier. */
		/* NOTREACHED */
	  case Magic('='):
		EMSG_RETURN((char_u *)"\\= follows nothing")
		/* NOTREACHED */
	  case Magic('+'):
		EMSG_RETURN((char_u *)"\\+ follows nothing")
		/* NOTREACHED */
	  case Magic('*'):
		if (reg_magic)
			EMSG_RETURN((char_u *)"* follows nothing")
		else
			EMSG_RETURN((char_u *)"\\* follows nothing")
		/* break; Not Reached */
#ifdef TILDE
	  case Magic('~'):			/* previous substitute pattern */
			if (reg_prev_sub) {
				register char_u *p;

				ret = regnode(EXACTLY);
				p = reg_prev_sub;
				while (*p) {
					regc(*p++);
				}
				regc('\0');
				if (p - reg_prev_sub) {
					*flagp |= HASWIDTH;
					if ((p - reg_prev_sub) == 1)
						*flagp |= SIMPLE;
				}
	  		} else
				EMSG_RETURN(e_nopresub);
			break;
#endif
#ifdef BACKREF
	  case Magic('1'):
	  case Magic('2'):
	  case Magic('3'):
	  case Magic('4'):
	  case Magic('5'):
	  case Magic('6'):
	  case Magic('7'):
	  case Magic('8'):
	  case Magic('9'): {
			int				refnum;

			ungetchr();
			refnum = getchr() - Magic('0');
			/*
			 * Check if the back reference is legal. We use the parentheses
			 * pointers to mark encountered close parentheses, but this
			 * is only available in the second pass. Checking opens is
			 * always possible.
			 * Should also check that we don't refer to something that
			 * is repeated (+*=): what instance of the repetition should
			 * we match? TODO.
			 */
			if (refnum < regnpar &&
				(regendp == NULL || regendp[refnum] != NULL))
				ret = regnode(BACKREF + refnum);
			else
				EMSG_RETURN((char_u *)"Illegal back reference");
		}
		break;
#endif
	  case Magic('['):
	    {
			char_u 		*p;

			/*
			 * If there is no matching ']', we assume the '[' is a normal
			 * character. This makes ":help [" work.
			 */
			p = regparse;
			if (*p == '^') 	/* Complement of range. */
				++p;
			if (*p == ']' || *p == '-')
				++p;
			while (*p != '\0' && *p != ']')
			{
				if (*p == '-')
				{
					++p;
					if (*p != ']' && *p != '\0')
						++p;
				}
				else if (*p == '\\' && p[1] != '\0')
					p += 2;
				else
					++p;
			}
			if (*p == ']')		/* there is a matching ']' */
			{
				/*
				 * In a character class, different parsing rules apply.
				 * Not even \ is special anymore, nothing is.
				 */
				if (*regparse == '^') { 	/* Complement of range. */
					ret = regnode(ANYBUT);
					regparse++;
				} else
					ret = regnode(ANYOF);
				if (*regparse == ']' || *regparse == '-')
					regc(*regparse++);
				while (*regparse != '\0' && *regparse != ']') {
					if (*regparse == '-') {
						regparse++;
						if (*regparse == ']' || *regparse == '\0')
							regc('-');
						else {
							register int	cclass;
							register int	cclassend;

							cclass = UCHARAT(regparse - 2) + 1;
							cclassend = UCHARAT(regparse);
							if (cclass > cclassend + 1)
								EMSG_RETURN(e_invrange);
							for (; cclass <= cclassend; cclass++)
								regc(cclass);
							regparse++;
						}
					} else if (*regparse == '\\' && regparse[1]) {
						regparse++;
						regc(*regparse++);
					} else
						regc(*regparse++);
				}
				regc('\0');
				if (*regparse != ']')
					EMSG_RETURN(e_toomsbra);
				skipchr();			/* let's be friends with the lexer again */
				*flagp |= HASWIDTH | SIMPLE;
				break;
			}
		}
		/* FALLTHROUGH */

	  default:
	    {
			register int	len;
			int				chr;

			ungetchr();
			len = 0;
			ret = regnode(EXACTLY);
			/*
			 * Always take at least one character, for '[' without matching
			 * ']'.
			 */
			while ((chr = peekchr()) != '\0' && (chr < Magic(0) || len == 0))
			{
				regc(chr);
				skipchr();
				len++;
			}
#ifdef DEBUG
			if (len == 0)
				 EMSG_RETURN((char_u *)"Unexpected magic character; check META.");
#endif
			/*
			 * If there is a following *, \+ or \= we need the character
			 * in front of it as a single character operand
			 */
			if (len > 1 && ismult(chr))
			{
				unregc();			/* Back off of *+= operand */
				ungetchr();			/* and put it back for next time */
				--len;
			}
			regc('\0');
			*flagp |= HASWIDTH;
			if (len == 1)
				*flagp |= SIMPLE;
		}
		break;
	}

	return ret;
}

/*
 - regnode - emit a node
 */
static char_u    *				/* Location. */
regnode(op)
	int			op;
{
	register char_u  *ret;
	register char_u  *ptr;

	ret = regcode;
	if (ret == &regdummy) {
		regsize += 3;
		return ret;
	}
	ptr = ret;
	*ptr++ = op;
	*ptr++ = '\0';				/* Null "next" pointer. */
	*ptr++ = '\0';
	regcode = ptr;

	return ret;
}

/*
 - regc - emit (if appropriate) a byte of code
 */
static void
regc(b)
	int			b;
{
	if (regcode != &regdummy)
		*regcode++ = b;
	else
		regsize++;
}

/*
 - unregc - take back (if appropriate) a byte of code
 */
static void
unregc()
{
	if (regcode != &regdummy)
		regcode--;
	else
		regsize--;
}

/*
 - reginsert - insert an operator in front of already-emitted operand
 *
 * Means relocating the operand.
 */
static void
reginsert(op, opnd)
	int			op;
	char_u		   *opnd;
{
	register char_u  *src;
	register char_u  *dst;
	register char_u  *place;

	if (regcode == &regdummy) {
		regsize += 3;
		return;
	}
	src = regcode;
	regcode += 3;
	dst = regcode;
	while (src > opnd)
		*--dst = *--src;

	place = opnd;				/* Op node, where operand used to be. */
	*place++ = op;
	*place++ = '\0';
	*place = '\0';
}

/*
 - regtail - set the next-pointer at the end of a node chain
 */
static void
regtail(p, val)
	char_u		   *p;
	char_u		   *val;
{
	register char_u  *scan;
	register char_u  *temp;
	register int	offset;

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

	if (OP(scan) == BACK)
		offset = (int)(scan - val);
	else
		offset = (int)(val - scan);
	*(scan + 1) = (char_u) ((offset >> 8) & 0377);
	*(scan + 2) = (char_u) (offset & 0377);
}

/*
 - regoptail - regtail on operand of first argument; nop if operandless
 */
static void
regoptail(p, val)
	char_u		   *p;
	char_u		   *val;
{
	/* "Operandless" and "op != BRANCH" are synonymous in practice. */
	if (p == NULL || p == &regdummy || OP(p) != BRANCH)
		return;
	regtail(OPERAND(p), val);
}

/*
 - getchr() - get the next character from the pattern. We know about
 * magic and such, so therefore we need a lexical analyzer.
 */

/* static int		curchr; */
static int		prevchr;
static int		nextchr;	/* used for ungetchr() */
/*
 * Note: prevchr is sometimes -1 when we are not at the start,
 * eg in /[ ^I]^ the pattern was never found even if it existed, because ^ was
 * taken to be magic -- webb
 */
static int		at_start;	/* True when we are on the first character */

static void
initchr(str)
char_u *str;
{
	regparse = str;
	curchr = prevchr = nextchr = -1;
	at_start = TRUE;
}

static int
peekchr()
{
	if (curchr < 0) {
		switch (curchr = regparse[0]) {
		case '.':
	/*	case '+':*/
	/*	case '=':*/
		case '[':
		case '~':
			if (reg_magic)
				curchr = Magic(curchr);
			break;
		case '*':
			/* * is not magic as the very first character, eg "?*ptr" */
			if (reg_magic && !at_start)
				curchr = Magic('*');
			break;
		case '^':
			/* ^ is only magic as the very first character */
			if (at_start)
				curchr = Magic('^');
			break;
		case '$':
			/* $ is only magic as the very last character and in front of '\|' */
			if (regparse[1] == NUL || (regparse[1] == '\\' && regparse[2] == '|'))
				curchr = Magic('$');
			break;
		case '\\':
			regparse++;
			if (regparse[0] == NUL)
				curchr = '\\';	/* trailing '\' */
			else if (vim_strchr(META, regparse[0]))
			{
				/*
				 * META contains everything that may be magic sometimes, except
				 * ^ and $ ("\^" and "\$" are never magic).
				 * We now fetch the next character and toggle its magicness.
				 * Therefore, \ is so meta-magic that it is not in META.
				 */
				curchr = -1;
				at_start = FALSE;			/* We still want to be able to say "/\*ptr" */
				peekchr();
				curchr ^= Magic(0);
			}
			else if (vim_strchr(REGEXP_ABBR, regparse[0]))
			{
				/*
				 * Handle abbreviations, like "\t" for TAB -- webb
				 */
				switch (regparse[0])
				{
					case 'r':	curchr = CR;		break;
					case 't':	curchr = TAB;		break;
					case 'e':	curchr = ESC;		break;
					case 'b':	curchr = Ctrl('H');	break;
				}
			}
			else
			{
				/*
				 * Next character can never be (made) magic?
				 * Then backslashing it won't do anything.
				 */
				curchr = regparse[0];
			}
			break;
		}
	}

	return curchr;
}

static void
skipchr()
{
	regparse++;
	at_start = FALSE;
	prevchr = curchr;
	curchr = nextchr;		/* use previously unget char, or -1 */
	nextchr = -1;
}

static int
getchr()
{
	int chr;

	chr = peekchr();
	skipchr();

	return chr;
}

/*
 * put character back. Works only once!
 */
static void
ungetchr()
{
	nextchr = curchr;
	curchr = prevchr;
	/*
	 * Backup regparse as well; not because we will use what it points at,
	 * but because skipchr() will bump it again.
	 */
	regparse--;
}

/*
 * vim_regexec and friends
 */

/*
 * Global work variables for vim_regexec().
 */
static char_u    *reginput;		/* String-input pointer. */
static char_u    *regbol; 		/* Beginning of input, for ^ check. */
static char_u   **regstartp;		/* Pointer to startp array. */
/* static char_u   **regendp;	*/	/* Ditto for endp. */

/*
 * Forwards.
 */
static int		regtry __ARGS((regexp *, char_u *));
static int		regmatch __ARGS((char_u *));
static int		regrepeat __ARGS((char_u *));

#ifdef DEBUG
int 			regnarrate = 1;
void			regdump __ARGS((regexp *));
static char_u    *regprop __ARGS((char_u *));
#endif

/*
 * vim_regexec - match a regexp against a string
 * Return non-zero if there is a match.
 */
int
vim_regexec(prog, string, at_bol)
	register regexp *prog;
	register char_u  *string;
	int 			at_bol;
{
	register char_u  *s;

	/* Be paranoid... */
	if (prog == NULL || string == NULL) {
		emsg(e_null);
		rc_did_emsg = TRUE;
		return 0;
	}
	/* Check validity of program. */
	if (UCHARAT(prog->program) != MAGIC) {
		emsg(e_re_corr);
		rc_did_emsg = TRUE;
		return 0;
	}
	/* If there is a "must appear" string, look for it. */
	if (prog->regmust != NULL) {
		s = string;
		while ((s = cstrchr(s, prog->regmust[0])) != NULL) {
			if (cstrncmp(s, prog->regmust, prog->regmlen) == 0)
				break;			/* Found it. */
			s++;
		}
		if (s == NULL)			/* Not present. */
			return 0;
	}
	/* Mark beginning of line for ^ . */
	if (at_bol)
		regbol = string;		/* is possible to match bol */
	else
		regbol = NULL;			/* we aren't there, so don't match it */

	/* Simplest case:  anchored match need be tried only once. */
	if (prog->reganch)
		return regtry(prog, string);

	/* Messy cases:  unanchored match. */
	s = string;
	if (prog->regstart != '\0')
		/* We know what char it must start with. */
		while ((s = cstrchr(s, prog->regstart)) != NULL) {
			if (regtry(prog, s))
				return 1;
			s++;
		}
	else
		/* We don't -- general case. */
		do {
			if (regtry(prog, s))
				return 1;
		} while (*s++ != '\0');

	/* Failure. */
	return 0;
}

/*
 - regtry - try match at specific point
 */
static int						/* 0 failure, 1 success */
regtry(prog, string)
	regexp		   *prog;
	char_u		   *string;
{
	register int	i;
	register char_u **sp;
	register char_u **ep;

	reginput = string;
	regstartp = prog->startp;
	regendp = prog->endp;

	sp = prog->startp;
	ep = prog->endp;
	for (i = NSUBEXP; i > 0; i--) {
		*sp++ = NULL;
		*ep++ = NULL;
	}
	if (regmatch(prog->program + 1)) {
		prog->startp[0] = string;
		prog->endp[0] = reginput;
		return 1;
	} else
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
static int						/* 0 failure, 1 success */
regmatch(prog)
	char_u		   *prog;
{
	register char_u  *scan;		/* Current node. */
	char_u		   *next;		/* Next node. */

	scan = prog;
#ifdef DEBUG
	if (scan != NULL && regnarrate)
		fprintf(stderr, "%s(\n", regprop(scan));
#endif
	while (scan != NULL) {
#ifdef DEBUG
		if (regnarrate) {
			fprintf(stderr, "%s...\n", regprop(scan));
		}
#endif
		next = regnext(scan);
		switch (OP(scan)) {
		  case BOL:
			if (reginput != regbol)
				return 0;
			break;
		  case EOL:
			if (*reginput != '\0')
				return 0;
			break;
		  case BOW:		/* \<word; reginput points to w */
		  	if (reginput != regbol && iswordchar(reginput[-1]))
				return 0;
		  	if (!reginput[0] || !iswordchar(reginput[0]))
				return 0;
			break;
		  case EOW:		/* word\>; reginput points after d */
		  	if (reginput == regbol || !iswordchar(reginput[-1]))
				return 0;
		  	if (reginput[0] && iswordchar(reginput[0]))
				return 0;
			break;
		  case ANY:
			if (*reginput == '\0')
				return 0;
			reginput++;
			break;
		  case IDENT:
		  	if (!isidchar(*reginput))
				return 0;
			reginput++;
		    break;
		  case WORD:
		  	if (!iswordchar(*reginput))
				return 0;
			reginput++;
		    break;
		  case FNAME:
		  	if (!isfilechar(*reginput))
				return 0;
			reginput++;
		    break;
		  case PRINT:
		  	if (charsize(*reginput) != 1)
				return 0;
			reginput++;
		    break;
		  case SIDENT:
		  	if (isdigit(*reginput) || !isidchar(*reginput))
				return 0;
			reginput++;
		    break;
		  case SWORD:
		  	if (isdigit(*reginput) || !iswordchar(*reginput))
				return 0;
			reginput++;
		    break;
		  case SFNAME:
		  	if (isdigit(*reginput) || !isfilechar(*reginput))
				return 0;
			reginput++;
		    break;
		  case SPRINT:
		  	if (isdigit(*reginput) || charsize(*reginput) != 1)
				return 0;
			reginput++;
		    break;
		  case EXACTLY:{
				register int	len;
				register char_u  *opnd;

				opnd = OPERAND(scan);
				/* Inline the first character, for speed. */
				if (*opnd != *reginput && (!reg_ic || TO_UPPER(*opnd) != TO_UPPER(*reginput)))
					return 0;
				len = STRLEN(opnd);
				if (len > 1 && cstrncmp(opnd, reginput, len) != 0)
					return 0;
				reginput += len;
			}
			break;
		  case ANYOF:
			if (*reginput == '\0' || cstrchr(OPERAND(scan), *reginput) == NULL)
				return 0;
			reginput++;
			break;
		  case ANYBUT:
			if (*reginput == '\0' || cstrchr(OPERAND(scan), *reginput) != NULL)
				return 0;
			reginput++;
			break;
		  case NOTHING:
			break;
		  case BACK:
			break;
		  case MOPEN + 1:
		  case MOPEN + 2:
		  case MOPEN + 3:
		  case MOPEN + 4:
		  case MOPEN + 5:
		  case MOPEN + 6:
		  case MOPEN + 7:
		  case MOPEN + 8:
		  case MOPEN + 9:{
				register int	no;
				register char_u  *save;

				no = OP(scan) - MOPEN;
				save = regstartp[no];
				regstartp[no] = reginput; /* Tentatively */
#ifdef DEBUG
				printf("MOPEN  %d pre  @'%s' ('%s' )'%s'\n",
					no, save,
					regstartp[no] ? regstartp[no] : "NULL",
					regendp[no] ? regendp[no] : "NULL");
#endif

				if (regmatch(next)) {
#ifdef DEBUG
				printf("MOPEN  %d post @'%s' ('%s' )'%s'\n",
					no, save,
					regstartp[no] ? regstartp[no] : "NULL",
					regendp[no] ? regendp[no] : "NULL");
#endif
					return 1;
				}
				regstartp[no] = save;		/* We were wrong... */
				return 0;
			}
			/* break; Not Reached */
		  case MCLOSE + 1:
		  case MCLOSE + 2:
		  case MCLOSE + 3:
		  case MCLOSE + 4:
		  case MCLOSE + 5:
		  case MCLOSE + 6:
		  case MCLOSE + 7:
		  case MCLOSE + 8:
		  case MCLOSE + 9:{
				register int	no;
				register char_u  *save;

				no = OP(scan) - MCLOSE;
				save = regendp[no];
				regendp[no] = reginput; /* Tentatively */
#ifdef DEBUG
				printf("MCLOSE %d pre  @'%s' ('%s' )'%s'\n",
					no, save,
					regstartp[no] ? regstartp[no] : "NULL",
					regendp[no] ? regendp[no] : "NULL");
#endif

				if (regmatch(next)) {
#ifdef DEBUG
				printf("MCLOSE %d post @'%s' ('%s' )'%s'\n",
					no, save,
					regstartp[no] ? regstartp[no] : "NULL",
					regendp[no] ? regendp[no] : "NULL");
#endif
					return 1;
				}
				regendp[no] = save;		/* We were wrong... */
				return 0;
			}
			/* break; Not Reached */
#ifdef BACKREF
		  case BACKREF + 1:
		  case BACKREF + 2:
		  case BACKREF + 3:
		  case BACKREF + 4:
		  case BACKREF + 5:
		  case BACKREF + 6:
		  case BACKREF + 7:
		  case BACKREF + 8:
		  case BACKREF + 9:{
				register int	no;
				int				len;

				no = OP(scan) - BACKREF;
				if (regendp[no] != NULL) {
					len = (int)(regendp[no] - regstartp[no]);
					if (cstrncmp(regstartp[no], reginput, len) != 0)
						return 0;
					reginput += len;
				} else {
					/*emsg("backref to 0-repeat");*/
					/*return 0;*/
				}
		  	}
			break;
#endif
		  case BRANCH:{
				register char_u  *save;

				if (OP(next) != BRANCH) /* No choice. */
					next = OPERAND(scan);		/* Avoid recursion. */
				else {
					do {
						save = reginput;
						if (regmatch(OPERAND(scan)))
							return 1;
						reginput = save;
						scan = regnext(scan);
					} while (scan != NULL && OP(scan) == BRANCH);
					return 0;
					/* NOTREACHED */
				}
			}
			break;
		  case STAR:
		  case PLUS:{
				register int	nextch;
				register int	no;
				register char_u  *save;
				register int	min;

				/*
				 * Lookahead to avoid useless match attempts when we know
				 * what character comes next.
				 */
				nextch = '\0';
				if (OP(next) == EXACTLY)
				{
					nextch = *OPERAND(next);
					if (reg_ic)
						nextch = TO_UPPER(nextch);
				}
				min = (OP(scan) == STAR) ? 0 : 1;
				save = reginput;
				no = regrepeat(OPERAND(scan));
				while (no >= min)
				{
					/* If it could work, try it. */
					if (nextch == '\0' || (*reginput == nextch ||
									(reg_ic && TO_UPPER(*reginput) == nextch)))
						if (regmatch(next))
							return 1;
					/* Couldn't or didn't -- back up. */
					no--;
					reginput = save + no;
				}
				return 0;
			}
			/* break; Not Reached */
		  case END:
			return 1; 		/* Success! */
			/* break; Not Reached */
		  default:
			emsg(e_re_corr);
			return 0;
			/* break; Not Reached */
		}

		scan = next;
	}

	/*
	 * We get here only if there's trouble -- normally "case END" is the
	 * terminating point.
	 */
	emsg(e_re_corr);
	return 0;
}

/*
 - regrepeat - repeatedly match something simple, report how many
 */
static int
regrepeat(p)
	char_u		   *p;
{
	register int	count = 0;
	register char_u  *scan;
	register char_u  *opnd;

	scan = reginput;
	opnd = OPERAND(p);
	switch (OP(p)) {
	  case ANY:
		count = STRLEN(scan);
		scan += count;
		break;
	  case IDENT:
	    for (count = 0; isidchar(*scan); ++count)
			++scan;
		break;
	  case WORD:
	    for (count = 0; iswordchar(*scan); ++count)
			++scan;
		break;
	  case FNAME:
	    for (count = 0; isfilechar(*scan); ++count)
			++scan;
		break;
	  case PRINT:
	    for (count = 0; charsize(*scan) == 1; ++count)
			++scan;
		break;
	  case SIDENT:
	    for (count = 0; !isdigit(*scan) && isidchar(*scan); ++count)
			++scan;
		break;
	  case SWORD:
	    for (count = 0; !isdigit(*scan) && iswordchar(*scan); ++count)
			++scan;
		break;
	  case SFNAME:
	    for (count = 0; !isdigit(*scan) && isfilechar(*scan); ++count)
			++scan;
		break;
	  case SPRINT:
	    for (count = 0; !isdigit(*scan) && charsize(*scan) == 1; ++count)
			++scan;
		break;
	  case EXACTLY:
		while (*opnd == *scan || (reg_ic && TO_UPPER(*opnd) == TO_UPPER(*scan)))
		{
			count++;
			scan++;
		}
		break;
	  case ANYOF:
		while (*scan != '\0' && cstrchr(opnd, *scan) != NULL)
		{
			count++;
			scan++;
		}
		break;
	  case ANYBUT:
		while (*scan != '\0' && cstrchr(opnd, *scan) == NULL) {
			count++;
			scan++;
		}
		break;
	  default:					/* Oh dear.  Called inappropriately. */
		emsg(e_re_corr);
		count = 0;				/* Best compromise. */
		break;
	}
	reginput = scan;

	return count;
}

/*
 - regnext - dig the "next" pointer out of a node
 */
static char_u    *
regnext(p)
	register char_u  *p;
{
	register int	offset;

	if (p == &regdummy)
		return NULL;

	offset = NEXT(p);
	if (offset == 0)
		return NULL;

	if (OP(p) == BACK)
		return p - offset;
	else
		return p + offset;
}

#ifdef DEBUG

/*
 - regdump - dump a regexp onto stdout in vaguely comprehensible form
 */
void
regdump(r)
	regexp		   *r;
{
	register char_u  *s;
	register int	op = EXACTLY;		/* Arbitrary non-END op. */
	register char_u  *next;


	s = r->program + 1;
	while (op != END) { 		/* While that wasn't END last time... */
		op = OP(s);
		printf("%2d%s", s - r->program, regprop(s));	/* Where, what. */
		next = regnext(s);
		if (next == NULL)		/* Next ptr. */
			printf("(0)");
		else
			printf("(%d)", (s - r->program) + (next - s));
		s += 3;
		if (op == ANYOF || op == ANYBUT || op == EXACTLY) {
			/* Literal string, where present. */
			while (*s != '\0') {
				putchar(*s);
				s++;
			}
			s++;
		}
		putchar('\n');
	}

	/* Header fields of interest. */
	if (r->regstart != '\0')
		printf("start `%c' ", r->regstart);
	if (r->reganch)
		printf("anchored ");
	if (r->regmust != NULL)
		printf("must have \"%s\"", r->regmust);
	printf("\n");
}

/*
 - regprop - printable representation of opcode
 */
static char_u    *
regprop(op)
	char_u		   *op;
{
	register char_u  *p;
	static char_u 	buf[50];

	(void) strcpy(buf, ":");

	switch (OP(op)) {
	  case BOL:
		p = "BOL";
		break;
	  case EOL:
		p = "EOL";
		break;
	  case ANY:
		p = "ANY";
		break;
	  case IDENT:
	  	p = "IDENT";
		break;
	  case WORD:
	  	p = "WORD";
		break;
	  case FNAME:
	  	p = "FNAME";
		break;
	  case PRINT:
	  	p = "PRINT";
		break;
	  case SIDENT:
	  	p = "SIDENT";
		break;
	  case SWORD:
	  	p = "SWORD";
		break;
	  case SFNAME:
	  	p = "SFNAME";
		break;
	  case SPRINT:
	  	p = "SPRINT";
		break;
	  case ANYOF:
		p = "ANYOF";
		break;
	  case ANYBUT:
		p = "ANYBUT";
		break;
	  case BRANCH:
		p = "BRANCH";
		break;
	  case EXACTLY:
		p = "EXACTLY";
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
	  case MOPEN + 1:
	  case MOPEN + 2:
	  case MOPEN + 3:
	  case MOPEN + 4:
	  case MOPEN + 5:
	  case MOPEN + 6:
	  case MOPEN + 7:
	  case MOPEN + 8:
	  case MOPEN + 9:
		sprintf(buf + STRLEN(buf), "MOPEN%d", OP(op) - MOPEN);
		p = NULL;
		break;
	  case MCLOSE + 1:
	  case MCLOSE + 2:
	  case MCLOSE + 3:
	  case MCLOSE + 4:
	  case MCLOSE + 5:
	  case MCLOSE + 6:
	  case MCLOSE + 7:
	  case MCLOSE + 8:
	  case MCLOSE + 9:
		sprintf(buf + STRLEN(buf), "MCLOSE%d", OP(op) - MCLOSE);
		p = NULL;
		break;
	  case BACKREF + 1:
	  case BACKREF + 2:
	  case BACKREF + 3:
	  case BACKREF + 4:
	  case BACKREF + 5:
	  case BACKREF + 6:
	  case BACKREF + 7:
	  case BACKREF + 8:
	  case BACKREF + 9:
		sprintf(buf + STRLEN(buf), "BACKREF%d", OP(op) - BACKREF);
		p = NULL;
		break;
	  case STAR:
		p = "STAR";
		break;
	  case PLUS:
		p = "PLUS";
		break;
	  default:
		sprintf(buf + STRLEN(buf), "corrupt %d", OP(op));
		p = NULL;
		break;
	}
	if (p != NULL)
		(void) strcat(buf, p);
	return buf;
}
#endif

/*
 * The following is provided for those people who do not have strcspn() in
 * their C libraries.  They should get off their butts and do something
 * about it; at least one public-domain implementation of those (highly
 * useful) string routines has been published on Usenet.
 */
#ifndef HAVE_STRCSPN
/*
 * strcspn - find length of initial segment of s1 consisting entirely
 * of characters not from s2
 */

	static size_t
strcspn(s1, s2)
	const char_u		   *s1;
	const char_u		   *s2;
{
	register char_u  *scan1;
	register char_u  *scan2;
	register int	count;

	count = 0;
	for (scan1 = s1; *scan1 != '\0'; scan1++) {
		for (scan2 = s2; *scan2 != '\0';)		/* ++ moved down. */
			if (*scan1 == *scan2++)
				return count;
		count++;
	}
	return count;
}
#endif

/*
 * Compare two strings, ignore case if reg_ic set.
 * Return 0 if strings match, non-zero otherwise.
 */
	int
cstrncmp(s1, s2, n)
	char_u		   *s1, *s2;
	int 			n;
{
	if (!reg_ic)
		return STRNCMP(s1, s2, (size_t)n);

	return vim_strnicmp(s1, s2, (size_t)n);
}

/*
 * cstrchr: This function is used a lot for simple searches, keep it fast!
 */
	static char_u *
cstrchr(s, c)
	char_u		   *s;
	register int	c;
{
	register char_u		   *p;

	if (!reg_ic)
		return vim_strchr(s, c);

	c = TO_UPPER(c);

	for (p = s; *p; p++)
	{
		if (TO_UPPER(*p) == c)
			return p;
	}
	return NULL;
}

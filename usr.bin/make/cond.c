/*	$OpenPackages$ */
/*	$OpenBSD: cond.c,v 1.24 2001/05/03 13:41:02 espie Exp $	*/
/*	$NetBSD: cond.c,v 1.7 1996/11/06 17:59:02 christos Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * cond.c --
 *	Functions to handle conditionals in a makefile.
 *
 * Interface:
 *	Cond_Eval	Evaluate the conditional in the passed line.
 *
 */

#include    <ctype.h>
#include    <math.h>
#include    <stddef.h>
#include    "make.h"
#include    "ohash.h"
#include    "dir.h"
#include    "buf.h"

#ifndef lint
#if 0
static char sccsid[] = "@(#)cond.c	8.2 (Berkeley) 1/2/94";
#else
UNUSED
static char rcsid[] = "$OpenBSD: cond.c,v 1.24 2001/05/03 13:41:02 espie Exp $";
#endif
#endif /* not lint */


/* The parsing of conditional expressions is based on this grammar:
 *	E -> F || E
 *	E -> F
 *	F -> T && F
 *	F -> T
 *	T -> defined(variable)
 *	T -> make(target)
 *	T -> exists(file)
 *	T -> empty(varspec)
 *	T -> target(name)
 *	T -> symbol
 *	T -> $(varspec) op value
 *	T -> $(varspec) == "string"
 *	T -> $(varspec) != "string"
 *	T -> ( E )
 *	T -> ! T
 *	op -> == | != | > | < | >= | <=
 *
 * 'symbol' is some other symbol to which the default function (condDefProc)
 * is applied.
 *
 * Tokens are scanned from the 'condExpr' string. The scanner (CondToken)
 * will return And for '&' and '&&', Or for '|' and '||', Not for '!',
 * LParen for '(', RParen for ')' and will evaluate the other terminal
 * symbols, using either the default function or the function given in the
 * terminal, and return the result as either True or False.
 *
 * All Non-Terminal functions (CondE, CondF and CondT) return Err on error.  */
typedef enum {
    False = 0, True = 1, And, Or, Not, LParen, RParen, EndOfFile, None, Err
} Token;

/*-
 * Structures to handle elegantly the different forms of #if's. The
 * last two fields are stored in condInvert and condDefProc, respectively.
 */
static Boolean CondGetArg(const char **, struct Name *,
    const char *, Boolean);
static Boolean CondDoDefined(struct Name *);
static Boolean CondDoMake(struct Name *);
static Boolean CondDoExists(struct Name *);
static Boolean CondDoTarget(struct Name *);
static Boolean CondCvtArg(const char *, double *);
static Token CondToken(Boolean);
static Token CondT(Boolean);
static Token CondF(Boolean);
static Token CondE(Boolean);
static Token CondHandleVarSpec(Boolean);
static Token CondHandleDefault(Boolean);
static const char *find_cond(const char *);


static struct If {
    char	*form;		/* Form of if */
    int 	formlen;	/* Length of form */
    Boolean	doNot;		/* TRUE if default function should be negated */
    Boolean	(*defProc)(struct Name *);
				/* Default function to apply */
} ifs[] = {
    { "ifdef",	  5,	  FALSE,  CondDoDefined },
    { "ifndef",   6,	  TRUE,   CondDoDefined },
    { "ifmake",   6,	  FALSE,  CondDoMake },
    { "ifnmake",  7,	  TRUE,   CondDoMake },
    { "if",	  2,	  FALSE,  CondDoDefined },
    { NULL,	  0,	  FALSE,  NULL }
};

static Boolean	  condInvert;		/* Invert the default function */
static Boolean	  (*condDefProc)	/* Default function to apply */
		   (struct Name *);
static const char *condExpr;		/* The expression to parse */
static Token	  condPushBack=None;	/* Single push-back token used in
					 * parsing */

#define MAXIF		30	  /* greatest depth of #if'ing */

static struct {
	Boolean 	value;
	unsigned long	lineno;
	const char	*filename;
} condStack[MAXIF];			/* Stack of conditionals */
static int	  condTop = MAXIF;	/* Top-most conditional */
static int	  skipIfLevel=0;	/* Depth of skipped conditionals */
static Boolean	  skipLine = FALSE;	/* Whether the parse module is skipping
					 * lines */

static const char *
find_cond(p)
    const char *p;
{
    for (;;p++) {
	if (strchr(" \t)&|$", *p) != NULL)
	    return p;
    }
}


/*-
 *-----------------------------------------------------------------------
 * CondGetArg --
 *	Find the argument of a built-in function.
 *
 * Results:
 *	TRUE if evaluation went okay
 *
 * Side Effects:
 *	The line pointer is set to point to the closing parenthesis of the
 *	function call. The argument is filled.
 *-----------------------------------------------------------------------
 */
static Boolean
CondGetArg(linePtr, arg, func, parens)
    const char 		**linePtr;
    struct Name	  	*arg;
    const char	  	*func;
    Boolean	  	parens;	/* TRUE if arg should be bounded by parens */
{
    const char	  	*cp;

    cp = *linePtr;
    if (parens) {
	while (*cp != '(' && *cp != '\0')
	    cp++;
	if (*cp == '(')
	    cp++;
    }

    if (*cp == '\0') {
	/* No arguments whatsoever. Because 'make' and 'defined' aren't really
	 * "reserved words", we don't print a message. I think this is better
	 * than hitting the user with a warning message every time s/he uses
	 * the word 'make' or 'defined' at the beginning of a symbol...  */
	arg->s = cp;
	arg->e = cp;
	arg->tofree = FALSE;
	return FALSE;
    }

    while (*cp == ' ' || *cp == '\t')
	cp++;


    cp = Var_Name_Get(cp, arg, NULL, TRUE, find_cond);

    while (*cp == ' ' || *cp == '\t')
	cp++;
    if (parens && *cp != ')') {
	Parse_Error(PARSE_WARNING, "Missing closing parenthesis for %s()",
		     func);
	return FALSE;
    } else if (parens)
	/* Advance pointer past close parenthesis.  */
	cp++;

    *linePtr = cp;
    return TRUE;
}

/*-
 *-----------------------------------------------------------------------
 * CondDoDefined --
 *	Handle the 'defined' function for conditionals.
 *
 * Results:
 *	TRUE if the given variable is defined.
 *-----------------------------------------------------------------------
 */
static Boolean
CondDoDefined(arg)
    struct Name	*arg;
{
    if (Var_Value_interval(arg->s, arg->e) != NULL)
	return TRUE;
    else
	return FALSE;
}

/*-
 *-----------------------------------------------------------------------
 * CondDoMake --
 *	Handle the 'make' function for conditionals.
 *
 * Results:
 *	TRUE if the given target is being made.
 *-----------------------------------------------------------------------
 */
static Boolean
CondDoMake(arg)
    struct Name	*arg;
{
    LstNode ln;

    for (ln = Lst_First(&create); ln != NULL; ln = Lst_Adv(ln)) {
	if (Str_Matchi((char *)Lst_Datum(ln), arg->s, arg->e))
	    return TRUE;
    }

    return FALSE;
}

/*-
 *-----------------------------------------------------------------------
 * CondDoExists --
 *	See if the given file exists.
 *
 * Results:
 *	TRUE if the file exists and FALSE if it does not.
 *-----------------------------------------------------------------------
 */
static Boolean
CondDoExists(arg)
    struct Name *arg;
{
    Boolean result;
    char    *path;

    path = Dir_FindFilei(arg->s, arg->e, &dirSearchPath);
    if (path != NULL) {
	result = TRUE;
	free(path);
    } else {
	result = FALSE;
    }
    return result;
}

/*-
 *-----------------------------------------------------------------------
 * CondDoTarget --
 *	See if the given node exists and is an actual target.
 *
 * Results:
 *	TRUE if the node exists as a target and FALSE if it does not.
 *-----------------------------------------------------------------------
 */
static Boolean
CondDoTarget(arg)
    struct Name	*arg;
{
    GNode   *gn;

    gn = Targ_FindNode(arg->s, arg->e, TARG_NOCREATE);
    if (gn != NULL && !OP_NOP(gn->type))
	return TRUE;
    else
	return FALSE;
}


/*-
 *-----------------------------------------------------------------------
 * CondCvtArg --
 *	Convert the given number into a double. If the number begins
 *	with 0x, it is interpreted as a hexadecimal integer
 *	and converted to a double from there. All other strings just have
 *	strtod called on them.
 *
 * Results:
 *	Sets 'value' to double value of string.
 *	Returns true if the string was a valid number, false o.w.
 *
 * Side Effects:
 *	Can change 'value' even if string is not a valid number.
 *-----------------------------------------------------------------------
 */
static Boolean
CondCvtArg(str, value)
    const char		*str;
    double		*value;
{
    if (*str == '0' && str[1] == 'x') {
	long i;

	for (str += 2, i = 0; *str; str++) {
	    int x;
	    if (isdigit(*str))
		x  = *str - '0';
	    else if (isxdigit(*str))
		x = 10 + *str - isupper(*str) ? 'A' : 'a';
	    else
		return FALSE;
	    i = (i << 4) + x;
	}
	*value = (double) i;
	return TRUE;
    }
    else {
	char *eptr;
	*value = strtod(str, &eptr);
	return *eptr == '\0';
    }
}


static Token
CondHandleVarSpec(doEval)
    Boolean doEval;
{
    Token	t;
    char	*lhs;
    const char	*rhs;
    const char	*op;
    size_t	varSpecLen;
    Boolean	doFree;

    /* Parse the variable spec and skip over it, saving its
     * value in lhs.  */
    t = Err;
    lhs = Var_Parse(condExpr, NULL, doEval,&varSpecLen,&doFree);
    if (lhs == var_Error)
	/* Even if !doEval, we still report syntax errors, which
	 * is what getting var_Error back with !doEval means.  */
	return Err;
    condExpr += varSpecLen;

    if (!isspace(*condExpr) &&
	strchr("!=><", *condExpr) == NULL) {
	BUFFER buf;

	Buf_Init(&buf, 0);

	Buf_AddString(&buf, lhs);

	if (doFree)
	    free(lhs);

	for (;*condExpr && !isspace(*condExpr); condExpr++)
	    Buf_AddChar(&buf, *condExpr);

	lhs = Buf_Retrieve(&buf);

	doFree = TRUE;
    }

    /* Skip whitespace to get to the operator.	*/
    while (isspace(*condExpr))
	condExpr++;

    /* Make sure the operator is a valid one. If it isn't a
     * known relational operator, pretend we got a
     * != 0 comparison.  */
    op = condExpr;
    switch (*condExpr) {
	case '!':
	case '=':
	case '<':
	case '>':
	    if (condExpr[1] == '=')
		condExpr += 2;
	    else
		condExpr += 1;
	    break;
	default:
	    op = "!=";
	    rhs = "0";

	    goto do_compare;
    }
    while (isspace(*condExpr))
	condExpr++;
    if (*condExpr == '\0') {
	Parse_Error(PARSE_WARNING,
		    "Missing right-hand-side of operator");
	goto error;
    }
    rhs = condExpr;
do_compare:
    if (*rhs == '"') {
	/* Doing a string comparison. Only allow == and != for
	 * operators.  */
	char	*string;
	const char *cp;
	int	    qt;
	BUFFER	buf;

do_string_compare:
	if ((*op != '!' && *op != '=') || op[1] != '=') {
	    Parse_Error(PARSE_WARNING,
    "String comparison operator should be either == or !=");
	    goto error;
	}

	Buf_Init(&buf, 0);
	qt = *rhs == '"' ? 1 : 0;

	for (cp = &rhs[qt];
	     ((qt && *cp != '"') ||
	      (!qt && strchr(" \t)", *cp) == NULL)) &&
	     *cp != '\0';) {
	    if (*cp == '$') {
		size_t	len;

		if (Var_ParseBuffer(&buf, cp, NULL, doEval, &len)
		    == SUCCESS) {
		    cp += len;
		    continue;
		}
	    } else if (*cp == '\\' && cp[1] != '\0')
		/* Backslash escapes things -- skip over next
		 * character, if it exists.  */
		cp++;
	    Buf_AddChar(&buf, *cp++);
	}

	string = Buf_Retrieve(&buf);

	if (DEBUG(COND))
	    printf("lhs = \"%s\", rhs = \"%s\", op = %.2s\n",
		   lhs, string, op);
	/* Null-terminate rhs and perform the comparison.
	 * t is set to the result.  */
	if (*op == '=')
	    t = strcmp(lhs, string) ? False : True;
	else
	    t = strcmp(lhs, string) ? True : False;
	free(string);
	if (rhs == condExpr) {
	    if (!qt && *cp == ')')
		condExpr = cp;
	    else if (*cp == '\0')
		condExpr = cp;
	    else
		condExpr = cp + 1;
	}
    } else {
	/* rhs is either a float or an integer. Convert both the
	 * lhs and the rhs to a double and compare the two.  */
	double		left, right;
	char		*string;

	if (!CondCvtArg(lhs, &left))
	    goto do_string_compare;
	if (*rhs == '$') {
	    size_t	len;
	    Boolean	freeIt;

	    string = Var_Parse(rhs, NULL, doEval,&len,&freeIt);
	    if (string == var_Error)
		right = 0.0;
	    else {
		if (!CondCvtArg(string, &right)) {
		    if (freeIt)
			free(string);
		    goto do_string_compare;
		}
		if (freeIt)
		    free(string);
		if (rhs == condExpr)
		    condExpr += len;
	    }
	} else {
	    if (!CondCvtArg(rhs, &right))
		goto do_string_compare;
	    if (rhs == condExpr) {
		/* Skip over the right-hand side.  */
		while (!isspace(*condExpr) &&
		      *condExpr != '\0')
		    condExpr++;

	    }
	}

	if (DEBUG(COND))
	    printf("left = %f, right = %f, op = %.2s\n", left,
		   right, op);
	switch (op[0]) {
	case '!':
	    if (op[1] != '=') {
		Parse_Error(PARSE_WARNING,
			    "Unknown operator");
		goto error;
	    }
	    t = left != right ? True : False;
	    break;
	case '=':
	    if (op[1] != '=') {
		Parse_Error(PARSE_WARNING,
			    "Unknown operator");
		goto error;
	    }
	    t = left == right ? True : False;
	    break;
	case '<':
	    if (op[1] == '=')
		t = left <= right ? True : False;
	    else
		t = left < right ? True : False;
	    break;
	case '>':
	    if (op[1] == '=')
		t = left >= right ? True : False;
	    else
		t = left > right ? True : False;
	    break;
	}
    }
error:
    if (doFree)
	free(lhs);
    return t;
}

#define S(s)	s, sizeof(s)-1
static struct operator {
    const char *s;
    size_t len;
    Boolean (*proc)(struct Name *);
} ops[] = {
    {S("defined"), CondDoDefined},
    {S("make"), CondDoMake},
    {S("exists"), CondDoExists},
    {S("target"), CondDoTarget},
    {NULL, 0, NULL}
};
static Token
CondHandleDefault(doEval)
    Boolean	doEval;
{
    Boolean	t;
    Boolean	(*evalProc)(struct Name *);
    Boolean	invert = FALSE;
    struct Name	arg;
    size_t arglen;

    evalProc = NULL;
    if (strncmp(condExpr, "empty", 5) == 0) {
	/* Use Var_Parse to parse the spec in parens and return
	 * True if the resulting string is empty.  */
	size_t	 length;
	Boolean doFree;
	char	*val;

	condExpr += 5;

	for (arglen = 0; condExpr[arglen] != '(' && condExpr[arglen] != '\0';)
	     arglen++;

	if (condExpr[arglen] != '\0') {
	    val = Var_Parse(&condExpr[arglen - 1], NULL,
			    doEval, &length, &doFree);
	    if (val == var_Error)
		t = Err;
	    else {
		/* A variable is empty when it just contains
		 * spaces... 4/15/92, christos */
		char *p;
		for (p = val; *p && isspace(*p); p++)
		    continue;
		t = *p == '\0' ? True : False;
	    }
	    if (doFree)
		free(val);
	    /* Advance condExpr to beyond the closing ). Note that
	     * we subtract one from arglen + length b/c length
	     * is calculated from condExpr[arglen - 1].  */
	    condExpr += arglen + length - 1;
	    return t;
	} else
	    condExpr -= 5;
    } else {
	struct operator *op;

	for (op = ops; op != NULL; op++)
	    if (strncmp(condExpr, op->s, op->len) == 0) {
		condExpr += op->len;
		if (CondGetArg(&condExpr, &arg, op->s, TRUE))
		    evalProc = op->proc;
		else
		    condExpr -= op->len;
		break;
	    }
    }
    if (evalProc == NULL) {
	/* The symbol is itself the argument to the default
	 * function. We advance condExpr to the end of the symbol
	 * by hand (the next whitespace, closing paren or
	 * binary operator) and set to invert the evaluation
	 * function if condInvert is TRUE.  */
	invert = condInvert;
	evalProc = condDefProc;
	/* XXX should we ignore problems now ? */
	CondGetArg(&condExpr, &arg, "", FALSE);
    }

    /* Evaluate the argument using the set function. If invert
     * is TRUE, we invert the sense of the function.  */
    t = (!doEval || (*evalProc)(&arg) ?
	 (invert ? False : True) :
	 (invert ? True : False));
    Var_Name_Free(&arg);
    return t;
}

/*-
 *-----------------------------------------------------------------------
 * CondToken --
 *	Return the next token from the input.
 *
 * Results:
 *	A Token for the next lexical token in the stream.
 *
 * Side Effects:
 *	condPushback will be set back to None if it is used.
 *-----------------------------------------------------------------------
 */
static Token
CondToken(doEval)
    Boolean doEval;
{

    if (condPushBack != None) {
	Token	  t;

	t = condPushBack;
	condPushBack = None;
	return t;
    }

    while (*condExpr == ' ' || *condExpr == '\t')
	condExpr++;
    switch (*condExpr) {
	case '(':
	    condExpr++;
	    return LParen;
	case ')':
	    condExpr++;
	    return RParen;
	case '|':
	    if (condExpr[1] == '|')
		condExpr++;
	    condExpr++;
	    return Or;
	case '&':
	    if (condExpr[1] == '&')
		condExpr++;
	    condExpr++;
	    return And;
	case '!':
	    condExpr++;
	    return Not;
	case '\n':
	case '\0':
	    return EndOfFile;
	case '$':
	    return CondHandleVarSpec(doEval);
	default:
	    return CondHandleDefault(doEval);
    }
}

/*-
 *-----------------------------------------------------------------------
 * CondT --
 *	Parse a single term in the expression. This consists of a terminal
 *	symbol or Not and a terminal symbol (not including the binary
 *	operators):
 *	    T -> defined(variable) | make(target) | exists(file) | symbol
 *	    T -> ! T | ( E )
 *
 * Results:
 *	True, False or Err.
 *
 * Side Effects:
 *	Tokens are consumed.
 *-----------------------------------------------------------------------
 */
static Token
CondT(doEval)
    Boolean doEval;
{
    Token   t;

    t = CondToken(doEval);

    if (t == EndOfFile)
	/* If we reached the end of the expression, the expression
	 * is malformed...  */
	t = Err;
    else if (t == LParen) {
	/* T -> ( E ).	*/
	t = CondE(doEval);
	if (t != Err)
	    if (CondToken(doEval) != RParen)
		t = Err;
    } else if (t == Not) {
	t = CondT(doEval);
	if (t == True)
	    t = False;
	else if (t == False)
	    t = True;
    }
    return t;
}

/*-
 *-----------------------------------------------------------------------
 * CondF --
 *	Parse a conjunctive factor (nice name, wot?)
 *	    F -> T && F | T
 *
 * Results:
 *	True, False or Err
 *
 * Side Effects:
 *	Tokens are consumed.
 *-----------------------------------------------------------------------
 */
static Token
CondF(doEval)
    Boolean doEval;
{
    Token   l, o;

    l = CondT(doEval);
    if (l != Err) {
	o = CondToken(doEval);

	if (o == And) {
	    /* F -> T && F
	     *
	     * If T is False, the whole thing will be False, but we have to
	     * parse the r.h.s. anyway (to throw it away).
	     * If T is True, the result is the r.h.s., be it an Err or no.  */
	    if (l == True)
		l = CondF(doEval);
	    else
		(void)CondF(FALSE);
	} else
	    /* F -> T.	*/
	    condPushBack = o;
    }
    return l;
}

/*-
 *-----------------------------------------------------------------------
 * CondE --
 *	Main expression production.
 *	    E -> F || E | F
 *
 * Results:
 *	True, False or Err.
 *
 * Side Effects:
 *	Tokens are, of course, consumed.
 *-----------------------------------------------------------------------
 */
static Token
CondE(doEval)
    Boolean doEval;
{
    Token   l, o;

    l = CondF(doEval);
    if (l != Err) {
	o = CondToken(doEval);

	if (o == Or) {
	    /* E -> F || E
	     *
	     * A similar thing occurs for ||, except that here we make sure
	     * the l.h.s. is False before we bother to evaluate the r.h.s.
	     * Once again, if l is False, the result is the r.h.s. and once
	     * again if l is True, we parse the r.h.s. to throw it away.  */
	    if (l == False)
		l = CondE(doEval);
	    else
		(void)CondE(FALSE);
	} else
	    /* E -> F.	*/
	    condPushBack = o;
    }
    return l;
}

/*-
 *-----------------------------------------------------------------------
 * Cond_Eval --
 *	Evaluate the conditional in the passed fragment. The fragment
 *	looks like this:
 *	    <cond-type> <expr>
 *	where <cond-type> is any of if, ifmake, ifnmake, ifdef,
 *	ifndef, elif, elifmake, elifnmake, elifdef, elifndef
 *	and <expr> consists of &&, ||, !, make(target), defined(variable)
 *	and parenthetical groupings thereof.
 *
 * Results:
 *	COND_PARSE	if should parse lines after the conditional
 *	COND_SKIP	if should skip lines after the conditional
 *	COND_INVALID	if not a valid conditional.
 *-----------------------------------------------------------------------
 */
int
Cond_Eval(line)
    char	    *line;    /* Line to parse */
{
    struct If	    *ifp;
    Boolean	    isElse;
    Boolean	    value = FALSE;
    int 	    level;	/* Level at which to report errors. */

    level = PARSE_FATAL;

    /* Stuff we are looking for can be if*, elif*, else, or endif.
     * otherwise, this is not our turf.  */

    /* Find what type of if we're dealing with. The result is left
     * in ifp and isElse is set TRUE if it's an elif line.  */
    if (line[0] == 'e' && line[1] == 'l') {
	line += 2;
	isElse = TRUE;
    } else if (strncmp(line, "endif", 5) == 0) {
	/* End of a conditional section. If skipIfLevel is non-zero, that
	 * conditional was skipped, so lines following it should also be
	 * skipped. Hence, we return COND_SKIP. Otherwise, the conditional
	 * was read so succeeding lines should be parsed (think about it...)
	 * so we return COND_PARSE, unless this endif isn't paired with
	 * a decent if.  */
	if (skipIfLevel != 0) {
	    skipIfLevel -= 1;
	    return COND_SKIP;
	} else {
	    if (condTop == MAXIF) {
		Parse_Error(level, "if-less endif");
		return COND_INVALID;
	    } else {
		skipLine = FALSE;
		condTop += 1;
		return COND_PARSE;
	    }
	}
    } else
	isElse = FALSE;

    /* Figure out what sort of conditional it is -- what its default
     * function is, etc. -- by looking in the table of valid "ifs" */
    for (ifp = ifs; ifp->form != NULL; ifp++) {
	if (strncmp(ifp->form, line, ifp->formlen) == 0)
	    break;
    }

    if (ifp->form == NULL) {
	/* Nothing fits. If the first word on the line is actually
	 * "else", it's a valid conditional whose value is the inverse
	 * of the previous if we parsed.  */
	if (isElse && line[0] == 's' && line[1] == 'e') {
	    if (condTop == MAXIF) {
		Parse_Error(level, "if-less else");
		return COND_INVALID;
	    } else if (skipIfLevel == 0)
		value = !condStack[condTop].value;
	    else
		return COND_SKIP;
	} else
	    /* Not a valid conditional type. No error...  */
	    return COND_INVALID;
    } else {
	if (isElse) {
	    if (condTop == MAXIF) {
		Parse_Error(level, "if-less elif");
		return COND_INVALID;
	    } else if (skipIfLevel != 0) {
		/* If skipping this conditional, just ignore the whole thing.
		 * If we don't, the user might be employing a variable that's
		 * undefined, for which there's an enclosing ifdef that
		 * we're skipping...  */
		return COND_SKIP;
	    }
	} else if (skipLine) {
	    /* Don't even try to evaluate a conditional that's not an else if
	     * we're skipping things...  */
	    skipIfLevel += 1;
	    return COND_SKIP;
	}

	/* Initialize file-global variables for parsing.  */
	condDefProc = ifp->defProc;
	condInvert = ifp->doNot;

	line += ifp->formlen;

	while (*line == ' ' || *line == '\t')
	    line++;

	condExpr = line;
	condPushBack = None;

	switch (CondE(TRUE)) {
	    case True:
		if (CondToken(TRUE) == EndOfFile) {
		    value = TRUE;
		    break;
		}
		goto err;
		/* FALLTHROUGH */
	    case False:
		if (CondToken(TRUE) == EndOfFile) {
		    value = FALSE;
		    break;
		}
		/* FALLTHROUGH */
	    case Err:
	    err:
		Parse_Error(level, "Malformed conditional (%s)", line);
		return COND_INVALID;
	    default:
		break;
	}
    }
    if (!isElse)
	condTop -= 1;
    else if (skipIfLevel != 0 || condStack[condTop].value) {
	/* If this is an else-type conditional, it should only take effect
	 * if its corresponding if was evaluated and FALSE. If its if was
	 * TRUE or skipped, we return COND_SKIP (and start skipping in case
	 * we weren't already), leaving the stack unmolested so later elif's
	 * don't screw up...  */
	skipLine = TRUE;
	return COND_SKIP;
    }

    if (condTop < 0) {
	/* This is the one case where we can definitely proclaim a fatal
	 * error. If we don't, we're hosed.  */
	Parse_Error(PARSE_FATAL, "Too many nested if's. %d max.", MAXIF);
	return COND_INVALID;
    } else {
	condStack[condTop].value = value;
	condStack[condTop].lineno = Parse_Getlineno();
	condStack[condTop].filename = Parse_Getfilename();
	skipLine = !value;
	return value ? COND_PARSE : COND_SKIP;
    }
}

/*-
 *-----------------------------------------------------------------------
 * Cond_End --
 *	Make sure everything's clean at the end of a makefile.
 *
 * Side Effects:
 *	Parse_Error will be called if open conditionals are around.
 *-----------------------------------------------------------------------
 */
void
Cond_End()
{
    int i;

    if (condTop != MAXIF) {
	Parse_Error(PARSE_FATAL, "%d open conditional%s", MAXIF-condTop,
		    MAXIF-condTop == 1 ? "" : "s");
	for (i = MAXIF-1; i >= condTop; i--) {
	    fprintf(stderr, "\t at line %lu of %s\n", condStack[i].lineno,
		condStack[i].filename);
	}
    }
    condTop = MAXIF;
}

/*	$OpenPackages$ */
/*	$OpenBSD: cond.c,v 1.42 2010/04/25 13:59:53 espie Exp $	*/
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "defines.h"
#include "dir.h"
#include "buf.h"
#include "cond.h"
#include "cond_int.h"
#include "condhashconsts.h"
#include "error.h"
#include "var.h"
#include "varname.h"
#include "targ.h"
#include "lowparse.h"
#include "str.h"
#include "main.h"
#include "gnode.h"
#include "lst.h"
#include "ohash.h"


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
 *	T -> "string" == "string"
 *	T -> "string" != "string"
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
 * terminal, and return the result as either true or False.
 *
 * All Non-Terminal functions (CondE, CondF and CondT) return Err on error.  */
typedef enum {
	False = 0, True = 1, And, Or, Not, LParen, RParen, EndOfFile, None, Err
} Token;

/*-
 * Structures to handle elegantly the different forms of #if's. The
 * last two fields are stored in condInvert and condDefProc, respectively.
 */
static bool CondGetArg(const char **, struct Name *,
    const char *, bool);
static bool CondDoDefined(struct Name *);
static bool CondDoMake(struct Name *);
static bool CondDoExists(struct Name *);
static bool CondDoTarget(struct Name *);
static bool CondCvtArg(const char *, double *);
static Token CondToken(bool);
static Token CondT(bool);
static Token CondF(bool);
static Token CondE(bool);
static Token CondHandleVarSpec(bool);
static Token CondHandleDefault(bool);
static Token CondHandleComparison(char *, bool, bool);
static Token CondHandleString(bool);
static const char *find_cond(const char *);


struct If {
	bool isElse;			/* true for else forms */
	bool doNot;			/* true for embedded negation */
	bool (*defProc)(struct Name *); /* function to apply */
};

static struct If ifs[] = {
	{ false,false,	CondDoDefined },	/* if, ifdef */
	{ false,true,	CondDoDefined },	/* ifndef */
	{ false,false,	CondDoMake },		/* ifmake */
	{ false,true,	CondDoMake },		/* ifnmake */
	{ true,	false,	CondDoDefined },	/* elif, elifdef */
	{ true,	true,	CondDoDefined },	/* elifndef */
	{ true,	false,	CondDoMake },		/* elifmake */
	{ true,	true,	CondDoMake },		/* elifnmake */
	{ true,	false,	NULL }
};

#define COND_IF_INDEX		0
#define COND_IFDEF_INDEX	0
#define COND_IFNDEF_INDEX	1
#define COND_IFMAKE_INDEX	2
#define COND_IFNMAKE_INDEX	3
#define COND_ELIF_INDEX		4
#define COND_ELIFDEF_INDEX	4
#define COND_ELIFNDEF_INDEX	5
#define COND_ELIFMAKE_INDEX	6
#define COND_ELIFNMAKE_INDEX	7
#define COND_ELSE_INDEX		8

static bool condInvert;		/* Invert the default function */
static bool (*condDefProc)(struct Name *);
				/* Default function to apply */
static const char *condExpr;	/* The expression to parse */
static Token condPushBack=None;	/* Single push-back token used in parsing */

#define MAXIF 30		/* greatest depth of #if'ing */

static struct {
	bool 	value;
	unsigned long	lineno;
	const char	*filename;
} condStack[MAXIF];		/* Stack of conditionals */

static int condTop = MAXIF;	/* Top-most conditional */
static int skipIfLevel=0;	/* Depth of skipped conditionals */
static bool skipLine = false;	/* Whether the parse module is skipping lines */

static const char *
find_cond(const char *p)
{
	for (;;p++) {
		/* XXX: when *p == '\0', strchr() returns !NULL */
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
 *	true if evaluation went okay
 *
 * Side Effects:
 *	The line pointer is set to point to the closing parenthesis of the
 *	function call. The argument is filled.
 *-----------------------------------------------------------------------
 */
static bool
CondGetArg(const char **linePtr, struct Name *arg, const char *func,
    bool parens) /* true if arg should be bounded by parens */
{
	const char *cp;

	cp = *linePtr;
	if (parens) {
		while (*cp != '(' && *cp != '\0')
			cp++;
		if (*cp == '(')
			cp++;
	}

	if (*cp == '\0') {
		/* No arguments whatsoever. Because 'make' and 'defined' aren't
		 * really "reserved words", we don't print a message. I think
		 * this is better than hitting the user with a warning message
		 * every time s/he uses the word 'make' or 'defined' at the
		 * beginning of a symbol...  */
		arg->s = cp;
		arg->e = cp;
		arg->tofree = false;
		return false;
	}

	while (*cp == ' ' || *cp == '\t')
		cp++;


	cp = VarName_Get(cp, arg, NULL, true, find_cond);

	while (*cp == ' ' || *cp == '\t')
		cp++;
	if (parens && *cp != ')') {
		Parse_Error(PARSE_WARNING,
		    "Missing closing parenthesis for %s()", func);
	    return false;
	} else if (parens)
		/* Advance pointer past close parenthesis.  */
		cp++;

	*linePtr = cp;
	return true;
}

/*-
 *-----------------------------------------------------------------------
 * CondDoDefined --
 *	Handle the 'defined' function for conditionals.
 *
 * Results:
 *	true if the given variable is defined.
 *-----------------------------------------------------------------------
 */
static bool
CondDoDefined(struct Name *arg)
{
	return Var_Definedi(arg->s, arg->e);
}

/*-
 *-----------------------------------------------------------------------
 * CondDoMake --
 *	Handle the 'make' function for conditionals.
 *
 * Results:
 *	true if the given target is being made.
 *-----------------------------------------------------------------------
 */
static bool
CondDoMake(struct Name *arg)
{
	LstNode ln;

	for (ln = Lst_First(create); ln != NULL; ln = Lst_Adv(ln)) {
		char *s = (char *)Lst_Datum(ln);
		if (Str_Matchi(s, strchr(s, '\0'), arg->s, arg->e))
			return true;
	}

	return false;
}

/*-
 *-----------------------------------------------------------------------
 * CondDoExists --
 *	See if the given file exists.
 *
 * Results:
 *	true if the file exists and false if it does not.
 *-----------------------------------------------------------------------
 */
static bool
CondDoExists(struct Name *arg)
{
	bool result;
	char *path;

	path = Dir_FindFilei(arg->s, arg->e, defaultPath);
	if (path != NULL) {
		result = true;
		free(path);
	} else {
		result = false;
	}
	return result;
}

/*-
 *-----------------------------------------------------------------------
 * CondDoTarget --
 *	See if the given node exists and is an actual target.
 *
 * Results:
 *	true if the node exists as a target and false if it does not.
 *-----------------------------------------------------------------------
 */
static bool
CondDoTarget(struct Name *arg)
{
    GNode *gn;

	gn = Targ_FindNodei(arg->s, arg->e, TARG_NOCREATE);
	if (gn != NULL && !OP_NOP(gn->type))
		return true;
	else
		return false;
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
static bool
CondCvtArg(const char *str, double *value)
{
	if (*str == '0' && str[1] == 'x') {
		long i;

		for (str += 2, i = 0; *str; str++) {
			int x;
			if (isdigit(*str))
				x  = *str - '0';
			else if (isxdigit(*str))
				x = 10 + *str - (isupper(*str) ? 'A' : 'a');
			else
				return false;
			i = (i << 4) + x;
		}
		*value = (double) i;
		return true;
	}
	else {
		char *eptr;
		*value = strtod(str, &eptr);
		return *eptr == '\0';
	}
}


static Token
CondHandleVarSpec(bool doEval)
{
	char *lhs;
	size_t varSpecLen;
	bool doFree;

	/* Parse the variable spec and skip over it, saving its
	 * value in lhs.  */
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

		lhs = Var_Subst(Buf_Retrieve(&buf), NULL, doEval);
		Buf_Destroy(&buf);
		doFree = true;
	}

	return CondHandleComparison(lhs, doFree, doEval);
}

static Token
CondHandleString(bool doEval)
{
	char *lhs;
	const char *begin;
	BUFFER buf;

	/* find the extent of the string */
	begin = ++condExpr;
	while (*condExpr && *condExpr != '"') {
		condExpr++;
	}

	Buf_Init(&buf, 0);
	Buf_Addi(&buf, begin, condExpr);
	if (*condExpr == '"')
		condExpr++;
	lhs = Var_Subst(Buf_Retrieve(&buf), NULL, doEval);
	Buf_Destroy(&buf);
	return CondHandleComparison(lhs, true, doEval);
}

static Token
CondHandleComparison(char *lhs, bool doFree, bool doEval)
{
	Token t;
	const char *rhs;
	const char *op;

	t = Err;
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
		char *string;
		const char *cp;
		int qt;
		BUFFER buf;

do_string_compare:
		if ((*op != '!' && *op != '=') || op[1] != '=') {
			Parse_Error(PARSE_WARNING,
			    "String comparison operator should be either == or !=");
			goto error;
		}

		Buf_Init(&buf, 0);
		qt = *rhs == '"' ? 1 : 0;

		for (cp = &rhs[qt]; ((qt && *cp != '"') ||
		    (!qt && strchr(" \t)", *cp) == NULL)) && *cp != '\0';) {
			if (*cp == '$') {
				size_t len;

				if (Var_ParseBuffer(&buf, cp, NULL, doEval,
				    &len)) {
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
		double left, right;
		char *string;

		if (!CondCvtArg(lhs, &left))
			goto do_string_compare;
		if (*rhs == '$') {
			size_t len;
			bool freeIt;

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
				while (!isspace(*condExpr) && *condExpr != '\0')
					condExpr++;
			}
		}

		if (DEBUG(COND))
			printf("left = %f, right = %f, op = %.2s\n", left,
			    right, op);
		switch (op[0]) {
		case '!':
			if (op[1] != '=') {
				Parse_Error(PARSE_WARNING, "Unknown operator");
				goto error;
			}
			t = left != right ? True : False;
			break;
		case '=':
			if (op[1] != '=') {
				Parse_Error(PARSE_WARNING, "Unknown operator");
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
	bool (*proc)(struct Name *);
} ops[] = {
	{S("defined"), CondDoDefined},
	{S("make"), CondDoMake},
	{S("exists"), CondDoExists},
	{S("target"), CondDoTarget},
	{NULL, 0, NULL}
};

static Token
CondHandleDefault(bool doEval)
{
	bool t;
	bool (*evalProc)(struct Name *);
	bool invert = false;
	struct Name arg;
	size_t arglen;

	evalProc = NULL;
	if (strncmp(condExpr, "empty", 5) == 0) {
		/* Use Var_Parse to parse the spec in parens and return
		 * True if the resulting string is empty.  */
		size_t length;
		bool doFree;
		char *val;

		condExpr += 5;

		for (arglen = 0; condExpr[arglen] != '(' &&
		    condExpr[arglen] != '\0';)
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
				for (p = val; isspace(*p); p++)
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
				if (CondGetArg(&condExpr, &arg, op->s, true))
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
		 * function if condInvert is true.  */
		invert = condInvert;
		evalProc = condDefProc;
		/* XXX should we ignore problems now ? */
		CondGetArg(&condExpr, &arg, "", false);
	}

	/* Evaluate the argument using the set function. If invert
	 * is true, we invert the sense of the function.  */
	t = (!doEval || (*evalProc)(&arg) ?
	     (invert ? False : True) :
	     (invert ? True : False));
	VarName_Free(&arg);
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
CondToken(bool doEval)
{

	if (condPushBack != None) {
		Token t;

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
	case '"':
		return CondHandleString(doEval);
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
CondT(bool doEval)
{
	Token t;

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
CondF(bool doEval)
{
	Token l, o;

	l = CondT(doEval);
	if (l != Err) {
		o = CondToken(doEval);

		if (o == And) {
		    /* F -> T && F
		     *
		     * If T is False, the whole thing will be False, but we
		     * have to parse the r.h.s. anyway (to throw it away).  If
		     * T is True, the result is the r.h.s., be it an Err or no.
		     * */
		    if (l == True)
			    l = CondF(doEval);
		    else
			    (void)CondF(false);
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
CondE(bool doEval)
{
	Token l, o;

	l = CondF(doEval);
	if (l != Err) {
		o = CondToken(doEval);

		if (o == Or) {
			/* E -> F || E
			 *
			 * A similar thing occurs for ||, except that here we
			 * make sure the l.h.s. is False before we bother to
			 * evaluate the r.h.s.  Once again, if l is False, the
			 * result is the r.h.s. and once again if l is True, we
			 * parse the r.h.s. to throw it away.  */
			if (l == False)
				l = CondE(doEval);
			else
				(void)CondE(false);
		} else
			/* E -> F.	*/
			condPushBack = o;
	}
	return l;
}

/* Evaluate conditional in line.
 * returns COND_SKIP, COND_PARSE, COND_INVALID, COND_ISFOR, COND_ISINCLUDE,
 * COND_ISUNDEF.
 * A conditional line looks like this:
 *	<cond-type> <expr>
 *	where <cond-type> is any of if, ifmake, ifnmake, ifdef,
 *	ifndef, elif, elifmake, elifnmake, elifdef, elifndef
 *	and <expr> consists of &&, ||, !, make(target), defined(variable)
 *	and parenthetical groupings thereof.
 */
int
Cond_Eval(const char *line)
{
	/* find end of keyword */
	const char *end;
	uint32_t k;
	size_t len;
	struct If *ifp;
	bool value = false;
	int level;	/* Level at which to report errors. */

	level = PARSE_FATAL;

	for (end = line; islower(*end); end++)
		;
	/* quick path: recognize special targets early on */
	if (*end == '.' || *end == ':')
		return COND_INVALID;
	len = end - line;
	k = ohash_interval(line, &end);
	switch(k % MAGICSLOTS2) {
	case K_COND_IF % MAGICSLOTS2:
		if (k == K_COND_IF && len == strlen(COND_IF) &&
		    strncmp(line, COND_IF, len) == 0) {
			ifp = ifs + COND_IF_INDEX;
		} else
			return COND_INVALID;
		break;
	case K_COND_IFDEF % MAGICSLOTS2:
		if (k == K_COND_IFDEF && len == strlen(COND_IFDEF) &&
		    strncmp(line, COND_IFDEF, len) == 0) {
			ifp = ifs + COND_IFDEF_INDEX;
		} else
			return COND_INVALID;
		break;
	case K_COND_IFNDEF % MAGICSLOTS2:
		if (k == K_COND_IFNDEF && len == strlen(COND_IFNDEF) &&
		    strncmp(line, COND_IFNDEF, len) == 0) {
			ifp = ifs + COND_IFNDEF_INDEX;
		} else
			return COND_INVALID;
		break;
	case K_COND_IFMAKE % MAGICSLOTS2:
		if (k == K_COND_IFMAKE && len == strlen(COND_IFMAKE) &&
		    strncmp(line, COND_IFMAKE, len) == 0) {
			ifp = ifs + COND_IFMAKE_INDEX;
		} else
			return COND_INVALID;
		break;
	case K_COND_IFNMAKE % MAGICSLOTS2:
		if (k == K_COND_IFNMAKE && len == strlen(COND_IFNMAKE) &&
		    strncmp(line, COND_IFNMAKE, len) == 0) {
			ifp = ifs + COND_IFNMAKE_INDEX;
		} else
			return COND_INVALID;
		break;
	case K_COND_ELIF % MAGICSLOTS2:
		if (k == K_COND_ELIF && len == strlen(COND_ELIF) &&
		    strncmp(line, COND_ELIF, len) == 0) {
			ifp = ifs + COND_ELIF_INDEX;
		} else
			return COND_INVALID;
		break;
	case K_COND_ELIFDEF % MAGICSLOTS2:
		if (k == K_COND_ELIFDEF && len == strlen(COND_ELIFDEF) &&
		    strncmp(line, COND_ELIFDEF, len) == 0) {
			ifp = ifs + COND_ELIFDEF_INDEX;
		} else
			return COND_INVALID;
		break;
	case K_COND_ELIFNDEF % MAGICSLOTS2:
		if (k == K_COND_ELIFNDEF && len == strlen(COND_ELIFNDEF) &&
		    strncmp(line, COND_ELIFNDEF, len) == 0) {
			ifp = ifs + COND_ELIFNDEF_INDEX;
		} else
			return COND_INVALID;
		break;
	case K_COND_ELIFMAKE % MAGICSLOTS2:
		if (k == K_COND_ELIFMAKE && len == strlen(COND_ELIFMAKE) &&
		    strncmp(line, COND_ELIFMAKE, len) == 0) {
			ifp = ifs + COND_ELIFMAKE_INDEX;
		} else
			return COND_INVALID;
		break;
	case K_COND_ELIFNMAKE % MAGICSLOTS2:
		if (k == K_COND_ELIFNMAKE && len == strlen(COND_ELIFNMAKE) &&
		    strncmp(line, COND_ELIFNMAKE, len) == 0) {
			ifp = ifs + COND_ELIFNMAKE_INDEX;
		} else
			return COND_INVALID;
		break;
	case K_COND_ELSE % MAGICSLOTS2:
		/* valid conditional whose value is the inverse
		 * of the previous if we parsed.  */
		if (k == K_COND_ELSE && len == strlen(COND_ELSE) &&
		    strncmp(line, COND_ELSE, len) == 0) {
			if (condTop == MAXIF) {
				Parse_Error(level, "if-less else");
				return COND_INVALID;
			} else if (skipIfLevel == 0) {
				value = !condStack[condTop].value;
				ifp = ifs + COND_ELSE_INDEX;
			} else
				return COND_SKIP;
		} else
			return COND_INVALID;
		break;
	case K_COND_ENDIF % MAGICSLOTS2:
		if (k == K_COND_ENDIF && len == strlen(COND_ENDIF) &&
		    strncmp(line, COND_ENDIF, len) == 0) {
			/* End of a conditional section. If skipIfLevel is
			 * non-zero, that conditional was skipped, so lines
			 * following it should also be skipped. Hence, we
			 * return COND_SKIP. Otherwise, the conditional was
			 * read so succeeding lines should be parsed (think
			 * about it...) so we return COND_PARSE, unless this
			 * endif isn't paired with a decent if.  */
			if (skipIfLevel != 0) {
				skipIfLevel--;
				return COND_SKIP;
			} else {
				if (condTop == MAXIF) {
					Parse_Error(level, "if-less endif");
					return COND_INVALID;
				} else {
					skipLine = false;
					condTop++;
					return COND_PARSE;
				}
			}
		} else
			return COND_INVALID;
		break;

	/* Recognize other keywords there, to simplify parser's task */
	case K_COND_FOR % MAGICSLOTS2:
		if (k == K_COND_FOR && len == strlen(COND_FOR) &&
		    strncmp(line, COND_FOR, len) == 0)
			return COND_ISFOR;
		else
			return COND_INVALID;
	case K_COND_UNDEF % MAGICSLOTS2:
		if (k == K_COND_UNDEF && len == strlen(COND_UNDEF) &&
		    strncmp(line, COND_UNDEF, len) == 0)
			return COND_ISUNDEF;
		else
			return COND_INVALID;
	case K_COND_POISON % MAGICSLOTS2:
		if (k == K_COND_POISON && len == strlen(COND_POISON) &&
		    strncmp(line, COND_POISON, len) == 0)
			return COND_ISPOISON;
		else
			return COND_INVALID;
	case K_COND_INCLUDE % MAGICSLOTS2:
		if (k == K_COND_INCLUDE && len == strlen(COND_INCLUDE) &&
		    strncmp(line, COND_INCLUDE, len) == 0)
			return COND_ISINCLUDE;
		else
			return COND_INVALID;
	default:
		/* Not a valid conditional type. No error...  */
		return COND_INVALID;
	}

	if (ifp->isElse) {
		if (condTop == MAXIF) {
			Parse_Error(level, "if-less elif");
			return COND_INVALID;
		} else if (skipIfLevel != 0 || condStack[condTop].value) {
			/*
			 * Skip if we're meant to or is an else-type
			 * conditional and previous corresponding one was
			 * evaluated to true.
			 */
			skipLine = true;
			return COND_SKIP;
		}
	} else if (skipLine) {
		/* Don't even try to evaluate a conditional that's not an else
		 * if we're skipping things...  */
		skipIfLevel++;
		return COND_SKIP;
	} else
		condTop--;

	if (condTop < 0) {
		/* This is the one case where we can definitely proclaim a fatal
		 * error. If we don't, we're hosed.  */
		Parse_Error(PARSE_FATAL, "Too many nested if's. %d max.",
		    MAXIF);
		condTop = 0;
		return COND_INVALID;
	}

	if (ifp->defProc) {
		/* Initialize file-global variables for parsing.  */
		condDefProc = ifp->defProc;
		condInvert = ifp->doNot;

		line += len;

		while (*line == ' ' || *line == '\t')
			line++;

		condExpr = line;
		condPushBack = None;

		switch (CondE(true)) {
		case True:
			if (CondToken(true) == EndOfFile) {
				value = true;
				break;
			}
			goto err;
			/* FALLTHROUGH */
		case False:
			if (CondToken(true) == EndOfFile) {
				value = false;
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

	condStack[condTop].value = value;
	condStack[condTop].lineno = Parse_Getlineno();
	condStack[condTop].filename = Parse_Getfilename();
	skipLine = !value;
	return value ? COND_PARSE : COND_SKIP;
}

void
Cond_End(void)
{
	int i;

	if (condTop != MAXIF) {
		Parse_Error(PARSE_FATAL, "%s%d open conditional%s",
		    condTop == 0 ? "at least ": "", MAXIF-condTop,
		    MAXIF-condTop == 1 ? "" : "s");
		for (i = MAXIF-1; i >= condTop; i--) {
			fprintf(stderr, "\t at line %lu of %s\n",
			    condStack[i].lineno, condStack[i].filename);
		}
	}
	condTop = MAXIF;
}

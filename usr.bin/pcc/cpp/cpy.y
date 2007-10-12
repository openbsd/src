/*	$OpenBSD: cpy.y,v 1.2 2007/10/12 07:22:44 otto Exp $	*/

/*
 * Copyright (c) 2004 Anders Magnusson (ragge@ludd.luth.se).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright(C) Caldera International Inc. 2001-2002. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code and documentation must retain the above
 * copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 * 	This product includes software developed or owned by Caldera
 *	International, Inc.
 * Neither the name of Caldera International, Inc. nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OFLIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

%{
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "cpp.h"

void yyerror(char *);
int yylex(void);
struct val eval(struct nd *);
%}

%term stop
%term EQ NE LE GE LS RS
%term ANDAND OROR IDENT NUMBER
/*
 * The following terminals are not used in the yacc code.
 */
%term STRING FPOINT WSPACE VA_ARGS CONCAT MKSTR ELLIPS

%left ','
%right '='
%right '?' ':'
%left OROR
%left ANDAND
%left '|' '^'
%left '&'
%binary EQ NE
%binary '<' '>' LE GE
%left LS RS
%left '+' '-'
%left '*' '/' '%'
%right '!' '~' UMINUS
%left '(' '.'

%union {
	struct val val;
	struct nd *node;
}

%type <val>	NUMBER
%type <node>	term e

%%
S:	e '\n'	{ return(eval($1).v.val != 0);}


e:	  e '*' e
		{$$ = mknode('*', $1, $3);}
	| e '/' e
		{$$ = mknode('/', $1, $3);}
	| e '%' e
		{$$ = mknode('%', $1, $3);}
	| e '+' e
		{$$ = mknode('+', $1, $3);}
	| e '-' e
		{$$ = mknode('-', $1, $3);}
	| e LS e
		{$$ = mknode(LS, $1, $3);}
	| e RS e
		{$$ = mknode(RS, $1, $3);}
	| e '<' e
		{$$ = mknode('<', $1, $3);}
	| e '>' e
		{$$ = mknode('>', $1, $3);}
	| e LE e
		{$$ = mknode(LE, $1, $3);}
	| e GE e
		{$$ = mknode(GE, $1, $3);}
	| e EQ e
		{$$ = mknode(EQ, $1, $3);}
	| e NE e
		{$$ = mknode(NE, $1, $3);}
	| e '&' e
		{$$ = mknode('&', $1, $3);}
	| e '^' e
		{$$ = mknode('^', $1, $3);}
	| e '|' e
		{$$ = mknode('|', $1, $3);}
	| e ANDAND e
		{$$ = mknode(ANDAND, $1, $3);}
	| e OROR e
		{$$ = mknode(OROR, $1, $3);}
	| e '?' e ':' e {
		struct nd *n = mknode(':', $3, $5);
		$$ = mknode('?', $1, n);}
	| e ',' e
		{$$ = mknode(',', $1, $3);}
	| term
		{$$ = $1;}
term:
	  '-' term %prec UMINUS
		{$$ = mknode(UMINUS, $2, NULL);}
	| '+' term %prec UMINUS
		{$$ = $2;}
	| '!' term
		{$$ = mknode('!', $2, NULL);}
	| '~' term
		{$$ = mknode('~', $2, NULL);}
	| '(' e ')'
		{$$ = $2;}
	| NUMBER
		{$$= mknum($1);}
%%

void
yyerror(char *err)
{
	error(err);
}

struct nd *
mknode(int op, struct nd *left, struct nd *right)
{
	struct nd *r = malloc(sizeof(*r));
	if (r == NULL) 
		error("out of mem");

	r->op = op;
	r->nd_left = left;
	r->nd_right = right;

	return r;
}

struct nd *
mknum(struct val val)
{
	struct nd *r = malloc(sizeof(*r));
	if (r == NULL) 
		error("out of mem");

	r->op = NUMBER;
	r->n.v = val;
	return r;
}

#define EVALUNARY(tok, op, t, x)				\
	case (tok): if (t) ret.v.uval = op x.v.uval;		\
		else ret.v.val = op x.v.val;			\
		ret.type = t;					\
		break;

#define EVALBIN(tok, op, t, x, y, r) 				\
	case (tok): if (t) ret.v.uval = x.v.uval op y.v.uval;	\
		else ret.v.val = x.v.val op y.v.val;		\
		ret.type = r;					\
		break;

struct val
eval(struct nd *tree)
{
	struct val ret, l, r;
	int t;

	switch (tree->op) {
	case NUMBER:
		ret.type = tree->nd_type;
		if (ret.type)
			ret.v.uval = tree->nd_uval;
		else
			ret.v.val = tree->nd_val;
		goto out;
	case LS:
	case RS:
		r = eval(tree->nd_right);
		/* FALLTHROUGH */
	case UMINUS:
	case '~':
	case '!':
		l = eval(tree->nd_left);
		switch (tree->op) {
			EVALBIN(LS, <<, l.type, l, r, l.type);
			EVALBIN(RS, >>, l.type, l, r, l.type);
			EVALUNARY(UMINUS, -, l.type, l);
			EVALUNARY('~', ~, l.type, l);
			EVALUNARY('!', !, 0, l);
		}
		goto out;
	case '?':
		l = eval(tree->nd_left);
		// XXX mem leak
		if (l.v.val)
			ret = eval(tree->nd_right->nd_left);
		else
			ret = eval(tree->nd_right->nd_right);
		goto out;
	case OROR:
		l = eval(tree->nd_left);
		// XXX mem leak
		if (l.v.val)
			ret = l;
		else
			ret = eval(tree->nd_right);
		ret.type = 0;
		goto out;
	case ANDAND:
		l = eval(tree->nd_left);
		// XXX mem leak
		if (l.v.val)
			ret = eval(tree->nd_right);
		else
			ret = l;
		ret.type = 0;
		goto out;
	case ',':
		// XXX mem leak
		ret = eval(tree->nd_right);
		goto out;
	}

	l = eval(tree->nd_left);
	r = eval(tree->nd_right);
	t = l.type || r.type;
	switch (tree->op) {
		EVALBIN(EQ, ==, t, l, r, 0);
		EVALBIN(NE, !=, t, l, r, 0);
		EVALBIN('<', <, t, l, r, 0);
		EVALBIN('>', >, t, l, r, 0);
		EVALBIN(GE, >=, t, l, r, 0);
		EVALBIN(LE, <=, t, l, r, 0);
		EVALBIN('+', +, t, l, r, t);
		EVALBIN('-', -, t, l, r, t);
		EVALBIN('*', *, t, l, r, t);
		// XXX check /,% by zero
		EVALBIN('/', /, t, l, r, t);
		EVALBIN('%', %, t, l, r, t);
		EVALBIN('&', &, t, l, r, t);
		EVALBIN('|', |, t, l, r, t);
		EVALBIN('^', ^, t, l, r, t);
	}
out:
	free(tree);
	return ret;
}

/*	$OpenBSD: cpy.y,v 1.4 2007/10/21 18:58:02 otto Exp $	*/

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

#include "cpp.h"

void yyerror(char *);
int yylex(void);
int setd(int l, int r);

#define	EVALUNARY(tok, l, r) l.nd_val = tok r.nd_val; l.op = r.op
#define	EVALBIN(tok, d, l, r)	\
	d.op = setd(l.op, r.op); d.nd_val = l.nd_val tok r.nd_val
#define	EVALUBIN(tok, d, l, r, t)				\
	d.op = setd(l.op, r.op);				\
	if (d.op == NUMBER) d.nd_val = l.nd_val tok r.nd_val;	\
	else d.nd_uval = l.nd_uval tok r.nd_uval;		\
	if (t && d.op) d.op = NUMBER
#define	XEVALUBIN(tok, d, l, r)					\
	if (r.nd_val) { EVALUBIN(tok, d, l, r, 0); } else d.op = 0
%}

%term stop
%term EQ NE LE GE LS RS
%term ANDAND OROR IDENT NUMBER UNUMBER
/*
 * The following terminals are not used in the yacc code.
 */
%term STRING FPOINT WSPACE VA_ARGS CONCAT MKSTR ELLIPS

%left ','
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
%left '('

%union {
	struct nd node;
}

%type <node>	term e NUMBER UNUMBER

%%
S:	e '\n'	{ 
		if ($1.op == 0)
			error("division by zero");
		return $1.nd_val;
	}

e:	  e '*' e
		{ EVALUBIN(*, $$, $1, $3, 0); }
	| e '/' e
		{ XEVALUBIN(/, $$, $1, $3); }
	| e '%' e
		{ XEVALUBIN(%, $$, $1, $3); }
	| e '+' e
		{ EVALBIN(+, $$, $1, $3); }
	| e '-' e
		{ EVALBIN(-, $$, $1, $3); }
	| e LS e
		{ EVALBIN(<<, $$, $1, $3); }
	| e RS e
		{ EVALUBIN(>>, $$, $1, $3, 0); }
	| e '<' e
		{ EVALUBIN(<, $$, $1, $3, 1); }
	| e '>' e
		{ EVALUBIN(>, $$, $1, $3, 1); }
	| e LE e
		{ EVALUBIN(<=, $$, $1, $3, 1); }
	| e GE e
		{ EVALUBIN(>=, $$, $1, $3, 1); }
	| e EQ e
		{ EVALUBIN(==, $$, $1, $3, 1); }
	| e NE e
		{ EVALUBIN(!=, $$, $1, $3, 1); }
	| e '&' e
		{ EVALBIN(&, $$, $1, $3); }
	| e '^' e
		{ EVALBIN(^, $$, $1, $3); }
	| e '|' e
		{ EVALBIN(|, $$, $1, $3); }
	| e ANDAND e {
		$$ = $1;
		if ($1.nd_val) {
			$$.op = setd($1.op, $3.op);
			$$.nd_val = ($3.nd_val != 0);
		}
		if ($$.op == UNUMBER) $$.op = NUMBER;
	}
	| e OROR e {
		if ($1.nd_val != 0) {
			$$.nd_val = ($1.nd_val != 0);
			$$.op = $1.op;
		} else {
			$$.nd_val = ($3.nd_val != 0);
			$$.op = setd($1.op, $3.op);
		}
		if ($$.op == UNUMBER) $$.op = NUMBER;
	}
	| e '?' e ':' e {
		if ($1.op == 0)
			$$ = $1;
		else if ($1.nd_val)
			$$ = $3;
		else
			$$ = $5;
	}
	| e ',' e {
		$$.op = setd($1.op, $3.op);
		$$.nd_val = $3.nd_val;
		if ($$.op) $$.op =  $3.op;
	}
	| term
		{$$ = $1;}
term:
	  '-' term %prec UMINUS
		{ EVALUNARY(-, $$, $2); }
	| '+' term %prec UMINUS
		{$$ = $2;}
	| '!' term
		{ $$.nd_val = ! $2.nd_val; $$.op = $2.op ? NUMBER : 0; }
	| '~' term
		{ EVALUNARY(~, $$, $2); }
	| '(' e ')'
		{$$ = $2;}
	| NUMBER
		{$$ = $1;}
%%

void
yyerror(char *err)
{
	error(err);
}

/*
 * Set return type of an expression.
 */
int
setd(int l, int r)
{
	if (!l || !r)
		return 0; /* div by zero involved */
	if (l == UNUMBER || r == UNUMBER)
		return UNUMBER;
	return NUMBER;
}


/*
 * Yacc syntax for GSP assembler
 *
 * Copyright (c) 1993 Paul Mackerras.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Mackerras.
 * 4. The name of the author may not be used to endorse or promote products
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

/* declarations */

%{
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "gsp_ass.h"
%}

%union {
	long		y_int;
	char		*y_id;
	expr		y_expr;
	operand		y_opnd;
}

%token LEFT_SHIFT, RIGHT_SHIFT

%term <y_id>	ID, STRING
%term <y_int>	NUMBER, REGISTER
%term UMINUS

/* operator precedences - lowest to highest */
%nonassoc ':'		/* concatenate y:x */
%left LEFT_SHIFT, RIGHT_SHIFT
%left '^'		/* EXCLUSIVE OR operator */
%left '|'		/* OR operator */
%left '&'		/* AND operator */
%left '+', '-'
%left '*', '/'
%nonassoc '~', UMINUS	/* NOT operator */

%start line

/* types of non-terminals */
%type <y_expr>		expr
%type <y_opnd>		operand, operands, ea

%%
/* rules */

line		: label ID operands	{ statement($2, $3); }
		| ID operands		{ statement($1, $2); }
		| label
		| ID '=' expr		{ do_asg($1, $3, 0); }
		| /* empty */
		| error
		;

label		: ID ':'		{ set_label($1); }
		| NUMBER ':'		{ set_numeric_label($1); }
		;

operands	: /* empty */		{ $$ = NULL; }
		| operand		{ $$ = $1; }
		| operands ',' operand	{ $$ = add_operand($1, $3); }
		;

operand		: REGISTER		{ $$ = reg_op($1); }
		| ea			{ $$ = $1; }
		| expr			{ $$ = expr_op($1); }
		| STRING		{ $$ = string_op($1); }
		;

ea		: '@' expr		{ $$ = abs_adr($2); }
		| '*' REGISTER		{ $$ = reg_ind($2, M_IND); }
		| '*' REGISTER ID	{ $$ = reg_indxy($2, $3); }
		| '*' REGISTER '+'	{ $$ = reg_ind($2, M_POSTINC); }
		| '*' '-' REGISTER	{ $$ = reg_ind($3, M_PREDEC); }
		| '-' '*' REGISTER	{ $$ = reg_ind($3, M_PREDEC); }
		| '*' REGISTER '(' expr ')'
					{ $$ = reg_index($2, $4); }
		;

expr		: ID			{ $$ = id_expr($1); }
		| NUMBER		{ $$ = num_expr($1); }
		| '.'			{ $$ = here_expr(); }
		| '(' expr ')'		{ $$ = $2; }
		| '~' expr		{ $$ = bexpr('~', $2, NULL); }
		| '-' expr			%prec UMINUS
					{ $$ = bexpr(NEG, $2, NULL); }
		| expr '*' expr		{ $$ = bexpr('*', $1, $3); }
		| expr '/' expr		{ $$ = bexpr('/', $1, $3); }
		| expr '+' expr		{ $$ = bexpr('+', $1, $3); }
		| expr '-' expr		{ $$ = bexpr('-', $1, $3); }
		| expr '&' expr		{ $$ = bexpr('&', $1, $3); }
		| expr '|' expr		{ $$ = bexpr('|', $1, $3); }
		| expr '^' expr		{ $$ = bexpr('^', $1, $3); }
		| expr ':' expr		{ $$ = bexpr(':', $1, $3); }
		| expr LEFT_SHIFT expr	{ $$ = bexpr(LEFT_SHIFT, $1, $3); }
		| expr RIGHT_SHIFT expr	{ $$ = bexpr(RIGHT_SHIFT, $1, $3); }
		;


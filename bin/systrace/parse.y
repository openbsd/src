/*	$OpenBSD: parse.y,v 1.6 2002/07/19 14:38:58 itojun Exp $	*/

/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
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
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
%{
#include <sys/types.h>

#include <sys/time.h>
#include <sys/tree.h>

#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <stdarg.h>
#include <string.h>

#include "intercept.h"
#include "systrace.h"
#include "systrace-errno.h"

void yyrestart(FILE *);

int filter_fnmatch(struct intercept_translate *, struct logic *);
int filter_stringmatch(struct intercept_translate *, struct logic *);
int filter_negstringmatch(struct intercept_translate *, struct logic *);
int filter_substrmatch(struct intercept_translate *, struct logic *);
int filter_negsubstrmatch(struct intercept_translate *, struct logic *);
int filter_inpath(struct intercept_translate *, struct logic *);
int filter_true(struct intercept_translate *, struct logic *);

struct logic *parse_newsymbol(char *, int, char *);

int yylex(void);
int yyparse(void);

int errors = 0;
struct filter *myfilter;

%}

%token	AND OR NOT LBRACE RBRACE LSQBRACE RSQBRACE THEN MATCH PERMIT DENY
%token	EQ NEQ TRUE SUB NSUB INPATH
%token	<string> STRING
%token	<string> CMDSTRING
%token	<number> NUMBER
%type	<logic> expression
%type	<logic> symbol
%type	<action> action
%type	<number> typeoff
%type	<string> errorcode
%union {
	int number;
	char *string;
	short action;
	struct logic *logic;
}
%%

filter		: fullexpression
		;

fullexpression	: expression THEN action errorcode
	{
		int flags = 0, errorcode = SYSTRACE_EPERM;

		switch ($3) {
		case ICPOLICY_NEVER:
			if ($4 == NULL)
				break;
			errorcode = systrace_error_translate($4);
			if (errorcode == -1)
				yyerror("Unknown error code: %s", $4);
			break;
		case ICPOLICY_PERMIT:
			if ($4 == NULL)
				break;
			if (!strcasecmp($4, "inherit"))
				flags = PROCESS_INHERIT_POLICY;
			else if (!strcasecmp($4, "detach"))
				flags = PROCESS_DETACH;
			else
				yyerror("Unknown flag: %s", $4);
			break;
		}

		if ($4 != NULL)
			free($4);

		myfilter = calloc(1, sizeof(struct filter));
		if (myfilter == NULL) {
			yyerror("calloc");
			break;
		}
		myfilter->logicroot = $1;
		myfilter->match_action = $3;
		myfilter->match_error = errorcode;
		myfilter->match_flags = flags;
	}
;

errorcode	: /* Empty */
{
	$$ = NULL;
}
		| LSQBRACE STRING RSQBRACE
{
	$$ = $2;
}
;

expression	: symbol
{
	$$ = $1;
}
		| NOT expression
{
	struct logic *node;
	node = calloc(1, sizeof(struct logic));
	if (node == NULL) {
		yyerror("calloc");
		break;
	}
	node->op = LOGIC_NOT;
	node->left = $2;
	$$ = node;
}
		| LBRACE expression RBRACE
{
	$$ = $2;
}
		| expression AND expression
{
	struct logic *node;
	node = calloc(1, sizeof(struct logic));
	if (node == NULL) {
		yyerror("calloc");
		break;
	}
	node->op = LOGIC_AND;
	node->left = $1;
	node->right = $3;
	$$ = node;
}
		| expression OR expression
{
	struct logic *node;
	node = calloc(1, sizeof(struct logic));
	if (node == NULL) {
		yyerror("calloc");
		break;
	}
	node->op = LOGIC_OR;
	node->left = $1;
	node->right = $3;
	$$ = node;
}
;

symbol		: STRING typeoff MATCH CMDSTRING
{
	struct logic *node;

	if ((node = parse_newsymbol($1, $2, $4)) == NULL)
		break;

	node->filter_match = filter_fnmatch;
	$$ = node;
}
		| STRING typeoff EQ CMDSTRING
{
	struct logic *node;

	if ((node = parse_newsymbol($1, $2, $4)) == NULL)
		break;

	node->filter_match = filter_stringmatch;
	$$ = node;
}
		| STRING typeoff NEQ CMDSTRING
{
	struct logic *node;

	if ((node = parse_newsymbol($1, $2, $4)) == NULL)
		break;

	node->filter_match = filter_negstringmatch;
	$$ = node;
}
		| STRING typeoff SUB CMDSTRING
{
	struct logic *node;

	if ((node = parse_newsymbol($1, $2, $4)) == NULL)
		break;

	node->filter_match = filter_substrmatch;
	$$ = node;
}
		| STRING typeoff NSUB CMDSTRING
{
	struct logic *node;

	if ((node = parse_newsymbol($1, $2, $4)) == NULL)
		break;

	node->filter_match = filter_negsubstrmatch;
	$$ = node;
}
		| STRING typeoff INPATH CMDSTRING
{
	struct logic *node;

	if ((node = parse_newsymbol($1, $2, $4)) == NULL)
		break;

	node->filter_match = filter_inpath;
	$$ = node;
}
		| TRUE
{
	struct logic *node;

	if ((node = parse_newsymbol(NULL, -1, NULL)) == NULL)
		break;

	node->filter_match = filter_true;
	$$ = node;
}
;

typeoff		: /* empty */
{
	$$ = -1;
}
		| LSQBRACE NUMBER RSQBRACE
{
	if ($2 < 0 || $2 >= INTERCEPT_MAXSYSCALLARGS) {
		yyerror("Bad offset: %d", $2);
		break;
	}
	$$ = $2;
}
;
action		: PERMIT
{
	$$ = ICPOLICY_PERMIT;
}
		| DENY
{
	$$ = ICPOLICY_NEVER;
}
%%

int yyerror(char *, ...);

int
yyerror(char *fmt, ...)
{
	va_list ap;
	errors = 1;

	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	fprintf(stdout, "\n");
	va_end(ap);
	return (0);
}

struct logic *
parse_newsymbol(char *type, int typeoff, char *data)
{
	struct logic *node;
	node = calloc(1, sizeof(struct logic));

	if (node == NULL) {
		yyerror("calloc");
		return (NULL);
	}
	node->op = LOGIC_SINGLE;
	node->type = type;
	node->typeoff = typeoff;
	if (data) {
		node->filterdata = strdup(filter_expand(data));
		free(data);
		if (node->filterdata == NULL) {
			yyerror("strdup");
			return (NULL);
		}
		node->filterlen = strlen(node->filterdata) + 1;
	}

	return (node);
}

int
parse_filter(char *name, struct filter **pfilter)
{
	extern char *mystring;
	extern int myoff;
	extern struct filter *myfilter;

	errors = 0;

	myfilter = NULL;
	mystring = name;
	myoff = 0;

	yyparse();
	yyrestart(NULL);
	*pfilter = myfilter;
	return (errors ? -1 : 0);
}

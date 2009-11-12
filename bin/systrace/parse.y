/*	$OpenBSD: parse.y,v 1.18 2009/11/12 20:07:46 millert Exp $	*/

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
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <regex.h>

#include "intercept.h"
#include "systrace.h"
#include "systrace-errno.h"
#include "filter.h"

void yyrestart(FILE *);

struct logic *parse_newsymbol(char *, int, char *);

int yylex(void);
int yyparse(void);
int yyerror(const char *, ...);

int errors = 0;
struct filter *myfilter;
extern char *mystring;
extern int myoff;
extern int iamroot;

%}

%token	AND OR NOT LBRACE RBRACE LSQBRACE RSQBRACE THEN MATCH PERMIT DENY ASK
%token	EQ NEQ TRUE SUB NSUB INPATH LOG COMMA IF USER GROUP EQUAL NEQUAL AS
%token	COLON RE LESSER GREATER
%token	<string> STRING
%token	<string> CMDSTRING
%token	<number> NUMBER
%type	<logic> expression
%type	<logic> symbol
%type	<action> action
%type	<number> typeoff
%type	<number> logcode
%type	<uid> uid
%type	<gid> gid
%type	<string> errorcode
%type	<predicate> predicate
%type	<elevate> elevate;
%union {
	int number;
	char *string;
	short action;
	struct logic *logic;
	struct predicate predicate;
	struct elevate elevate;
	uid_t uid;
	gid_t gid;
}
%%

fullexpression	: expression THEN action errorcode logcode elevate predicate
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

		if ($5)
			flags |= SYSCALL_LOG;

		/* Special policy that allows only yes or no */
		if ($3 == ICPOLICY_ASK)
			flags |= PROCESS_PROMPT;

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
		myfilter->match_predicate = $7;
		myfilter->elevate = $6;
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

logcode	: /* Empty */
{
	$$ = 0;
}
		| LOG
{
	$$ = 1;
}
;


uid		: NUMBER
{
	$$ = $1;
} 
		| STRING
{
	struct passwd *pw;
	if ((pw = getpwnam($1)) == NULL) {
		yyerror("Unknown user %s", $1);
		break;
	}

	$$ = pw->pw_uid;
}

gid		: NUMBER
{
	$$ = $1;
}
		| STRING
{
	struct group *gr;
	if ((gr = getgrnam($1)) == NULL) {
		yyerror("Unknown group %s", $1);
		break;
	}

	$$ = gr->gr_gid;
}

elevate: /* Empty */
{
	memset(&$$, 0, sizeof($$));
}
		| AS uid
{
	if (!iamroot) {
		yyerror("Privilege elevation not allowed.");
		break;
	}

	$$.e_flags = ELEVATE_UID;
	$$.e_uid = $2;
}
		| AS uid COLON gid
{
	if (!iamroot) {
		yyerror("Privilege elevation not allowed.");
		break;
	}

	$$.e_flags = ELEVATE_UID|ELEVATE_GID;
	$$.e_uid = $2;
	$$.e_gid = $4;
}
		| AS COLON gid
{
	if (!iamroot) {
		yyerror("Privilege elevation not allowed.");
		break;
	}

	$$.e_flags = ELEVATE_GID;
	$$.e_gid = $3;
}

predicate : /* Empty */
{
	memset(&$$, 0, sizeof($$));
}
		| COMMA IF USER EQUAL uid
{
	memset(&$$, 0, sizeof($$));
	$$.p_uid = $5;
	$$.p_flags = PREDIC_UID;
}
		| COMMA IF USER NEQUAL uid
{
	memset(&$$, 0, sizeof($$));
	$$.p_uid = $5;
	$$.p_flags = PREDIC_UID | PREDIC_NEGATIVE;
}
		| COMMA IF USER LESSER uid
{
	memset(&$$, 0, sizeof($$));
	$$.p_uid = $5;
	$$.p_flags = PREDIC_UID | PREDIC_LESSER;
}
		| COMMA IF USER GREATER uid
{
	memset(&$$, 0, sizeof($$));
	$$.p_uid = $5;
	$$.p_flags = PREDIC_UID | PREDIC_GREATER;
}
		| COMMA IF GROUP EQUAL gid
{
	memset(&$$, 0, sizeof($$));
	$$.p_gid = $5;
	$$.p_flags = PREDIC_GID;
}
		| COMMA IF GROUP NEQUAL gid
{
	memset(&$$, 0, sizeof($$));
	$$.p_gid = $5;
	$$.p_flags = PREDIC_GID | PREDIC_NEGATIVE;
}
		| COMMA IF GROUP LESSER gid
{
	memset(&$$, 0, sizeof($$));
	$$.p_gid = $5;
	$$.p_flags = PREDIC_GID | PREDIC_LESSER;
}
		| COMMA IF GROUP GREATER gid
{
	memset(&$$, 0, sizeof($$));
	$$.p_gid = $5;
	$$.p_flags = PREDIC_GID | PREDIC_GREATER;
}

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
		| STRING typeoff RE CMDSTRING
{
	struct logic *node;
	regex_t re;

	if ((node = parse_newsymbol($1, $2, $4)) == NULL)
		break;

	/* Precompute regexp here, otherwise we need to compute it
	 * on the fly which is fairly expensive.
	 */
	if (!(node->flags & LOGIC_NEEDEXPAND)) {
		if (regcomp(&re, node->filterdata,
			REG_EXTENDED | REG_NOSUB) != 0) {
			yyerror("Invalid regular expression: %s",
			    node->filterdata);
			break;
		}
		if ((node->filterarg = malloc(sizeof(re))) == NULL) {
			yyerror("malloc");
			break;
		}
		memcpy(node->filterarg, &re, sizeof(re));
	} else
		node->filterarg = NULL;

	node->filter_match = filter_regex;
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
		| ASK
{
	$$ = ICPOLICY_ASK;
}
		| DENY
{
	$$ = ICPOLICY_NEVER;
}
%%

int
yyerror(const char *fmt, ...)
{
	va_list ap;
	errors = 1;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
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
		/* For the root user, variable expansion may change */
		if (iamroot) {
			node->filterdata = data;
			if (filter_needexpand(data))
				node->flags |= LOGIC_NEEDEXPAND;
		} else {
			node->filterdata = strdup(filter_expand(data));
			free(data);
		}
		if (node->filterdata == NULL) {
			yyerror("strdup");
			free(node);
			return (NULL);
		}
		node->filterlen = strlen(node->filterdata) + 1;
	}

	return (node);
}

int
parse_filter(char *name, struct filter **pfilter)
{

	errors = 0;

	myfilter = NULL;
	mystring = name;
	myoff = 0;

	yyparse();
	yyrestart(NULL);
	*pfilter = myfilter;
	return (errors ? -1 : 0);
}

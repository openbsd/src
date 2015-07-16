/* $OpenBSD: parse.y,v 1.1 2015/07/16 20:44:21 tedu Exp $ */
/*
 * Copyright (c) 2015 Ted Unangst <tedu@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

%{
#include <sys/types.h>
#include <ctype.h>
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <err.h>

#include "doas.h"

typedef struct {
	union {
		struct {
			int action;
			int options;
			const char **envlist;
		};
		const char *str;
	};
} yystype;
#define YYSTYPE yystype

FILE *yyfp;

struct rule **rules;
int nrules, maxrules;

%}

%token TPERMIT TDENY TAS TCMD
%token TNOPASS TKEEPENV
%token TSTRING

%%

grammar:	/* empty */
		| grammar '\n'
		| grammar rule '\n'
		;

rule:		action ident target cmd {
			struct rule *r;
			r = calloc(1, sizeof(*r));
			r->action = $1.action;
			r->options = $1.options;
			r->envlist = $1.envlist;
			r->ident = $2.str;
			r->target = $3.str;
			r->cmd = $4.str;
			if (nrules == maxrules) {
				if (maxrules == 0)
					maxrules = 63;
				else
					maxrules *= 2;
				if (!(rules = reallocarray(rules, maxrules, sizeof(*rules))))
					errx(1, "can't allocate rules");
			}
			rules[nrules++] = r;
		} ;

action:		TPERMIT options {
			$$.action = PERMIT;
			$$.options = $2.options;
			$$.envlist = $2.envlist;
		} | TDENY {
			$$.action = DENY;
		} ;

options:	/* none */
		| options option {
			$$.options = $1.options | $2.options;
			$$.envlist = $1.envlist;
			if ($2.envlist) {
				if ($$.envlist)
					errx(1, "can't have two keepenv sections");
				else
					$$.envlist = $2.envlist;
			}
		} ;
option:		TNOPASS {
			$$.options = NOPASS;
		} | TKEEPENV {
			$$.options = KEEPENV;
		} | TKEEPENV '{' envlist '}' {
			$$.options = KEEPENV;
			$$.envlist = $3.envlist;
		} ;

envlist:	/* empty */ {
			if (!($$.envlist = calloc(1, sizeof(char *))))
				errx(1, "can't allocate envlist");
		} | envlist TSTRING {
			int nenv = arraylen($1.envlist);
			if (!($$.envlist = reallocarray($1.envlist, nenv + 2, sizeof(char *))))
				errx(1, "can't allocate envlist");
			$$.envlist[nenv] = $2.str;
			$$.envlist[nenv + 1] = NULL;
		}


ident:		TSTRING {
			$$.str = $1.str;
		} ;

target:		/* optional */ {
			$$.str = NULL;
		} | TAS TSTRING {
			$$.str = $2.str;
		} ;

cmd:		/* optional */ {
			$$.str = NULL;
		} | TCMD TSTRING {
			$$.str = $2.str;
		} ;

%%

void
yyerror(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	fprintf(stderr, "doas: ");
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	va_end(va);
	exit(1);
}

struct keyword {
	const char *word;
	int token;
} keywords[] = {
	{ "deny", TDENY },
	{ "permit", TPERMIT },
	{ "as", TAS },
	{ "cmd", TCMD },
	{ "nopass", TNOPASS },
	{ "keepenv", TKEEPENV },
};

int
yylex(void)
{
	char buf[1024], *ebuf, *p, *str;
	int i, c;

	p = buf;
	ebuf = buf + sizeof(buf);
	while ((c = getc(yyfp)) == ' ' || c == '\t')
		; /* skip spaces */
	switch (c) {
		case '\n':
		case '{':
		case '}':
			return c;
		case '#':
			while ((c = getc(yyfp)) != '\n' && c != EOF)
				; /* skip comments */
			if (c == EOF)
				return 0;
			return c;
		case EOF:
			return 0;
		case ':':
			*p++ = c;
			c = getc(yyfp);
			break;
		default:
			break;
	}
	while (isalnum(c)) {
		*p++ = c;
		if (p == ebuf)
			yyerror("too much stuff");
		c = getc(yyfp);
	}
	*p = 0;
	if (c != EOF)
		ungetc(c, yyfp);
	for (i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
		if (strcmp(buf, keywords[i].word) == 0)
			return keywords[i].token;
	}
	if ((str = strdup(buf)) == NULL)
		err(1, "strdup");
	yylval.str = str;
	return TSTRING;
}

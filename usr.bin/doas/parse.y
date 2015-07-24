/* $OpenBSD: parse.y,v 1.10 2015/07/24 06:36:42 zhuk Exp $ */
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
			const char *cmd;
			const char **cmdargs;
			const char **envlist;
		};
		const char *str;
	};
	int lineno;
	int colno;
} yystype;
#define YYSTYPE yystype

FILE *yyfp;

struct rule **rules;
int nrules, maxrules;
int parse_errors = 0;

void yyerror(const char *, ...);
int yylex(void);
int yyparse(void);

%}

%token TPERMIT TDENY TAS TCMD TARGS
%token TNOPASS TKEEPENV
%token TSTRING

%%

grammar:	/* empty */
		| grammar '\n'
		| grammar rule '\n'
		| error '\n'
		;

rule:		action ident target cmd {
			struct rule *r;
			r = calloc(1, sizeof(*r));
			if (!r)
				errx(1, "can't allocate rule");
			r->action = $1.action;
			r->options = $1.options;
			r->envlist = $1.envlist;
			r->ident = $2.str;
			r->target = $3.str;
			r->cmd = $4.cmd;
			r->cmdargs = $4.cmdargs;
			if (nrules == maxrules) {
				if (maxrules == 0)
					maxrules = 63;
				else
					maxrules *= 2;
				if (!(rules = reallocarray(rules, maxrules,
				    sizeof(*rules))))
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
				if ($$.envlist) {
					yyerror("can't have two keepenv sections");
					YYERROR;
				} else
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
			if (!($$.envlist = reallocarray($1.envlist, nenv + 2,
			    sizeof(char *))))
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
			$$.cmd = NULL;
			$$.cmdargs = NULL;
		} | TCMD TSTRING args {
			$$.cmd = $2.str;
			$$.cmdargs = $3.cmdargs;
		} ;

args:		/* empty */ {
			$$.cmdargs = NULL;
		} | TARGS argslist {
			$$.cmdargs = $2.cmdargs;
		} ;

argslist:	/* empty */ {
			if (!($$.cmdargs = calloc(1, sizeof(char *))))
				errx(1, "can't allocate args");
		} | argslist TSTRING {
			int nargs = arraylen($1.cmdargs);
			if (!($$.cmdargs = reallocarray($1.cmdargs, nargs + 2, sizeof(char *))))
				errx(1, "can't allocate args");
			$$.cmdargs[nargs] = $2.str;
			$$.cmdargs[nargs + 1] = NULL;
		} ;

%%

void
yyerror(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
	fprintf(stderr, " at line %d\n", yylval.lineno + 1);
	parse_errors++;
}

struct keyword {
	const char *word;
	int token;
} keywords[] = {
	{ "deny", TDENY },
	{ "permit", TPERMIT },
	{ "as", TAS },
	{ "cmd", TCMD },
	{ "args", TARGS },
	{ "nopass", TNOPASS },
	{ "keepenv", TKEEPENV },
};

int
yylex(void)
{
	char buf[1024], *ebuf, *p, *str;
	int i, c, quotes = 0, escape = 0, qpos = -1, nonkw = 0;

	p = buf;
	ebuf = buf + sizeof(buf);

repeat:
	/* skip whitespace first */
	for (c = getc(yyfp); c == ' ' || c == '\t'; c = getc(yyfp))
		yylval.colno++;

	/* check for special one-character constructions */
	switch (c) {
		case '\n':
			yylval.colno = 0;
			yylval.lineno++;
			/* FALLTHROUGH */
		case '{':
		case '}':
			return c;
		case '#':
			/* skip comments; NUL is allowed; no continuation */
			while ((c = getc(yyfp)) != '\n')
				if (c == EOF)
					return 0;
			yylval.colno = 0;
			yylval.lineno++;
			return c;
		case EOF:
			return 0;
	}

	/* parsing next word */
	for (;; c = getc(yyfp), yylval.colno++) {
		switch (c) {
		case '\0':
			yyerror("unallowed character NUL in column %d", yylval.colno + 1);
			escape = 0;
			continue;
		case '\\':
			escape = !escape;
			if (escape)
				continue;
			break;
		case '\n':
			if (quotes)
				yyerror("unterminated quotes in column %d",
				    qpos + 1);
			if (escape) {
				nonkw = 1;
				escape = 0;
				continue;
			}
			goto eow;
		case EOF:
			if (escape)
				yyerror("unterminated escape in column %d",
				    yylval.colno);
			if (quotes)
				yyerror("unterminated quotes in column %d",
				    qpos + 1);
			goto eow;
			/* FALLTHROUGH */
		case '{':
		case '}':
		case '#':
		case ' ':
		case '\t':
			if (!escape && !quotes)
				goto eow;
			break;
		case '"':
			if (!escape) {
				quotes = !quotes;
				if (quotes) {
					nonkw = 1;
					qpos = yylval.colno;
				}
				continue;
			}
		}
		*p++ = c;
		if (p == ebuf)
			yyerror("too long line");
		escape = 0;
	}

eow:
	*p = 0;
	if (c != EOF)
		ungetc(c, yyfp);
	if (p == buf) {
		/*
		 * There could be a number of reasons for empty buffer, and we handle
		 * all of them here, to avoid cluttering the main loop.
		 */
		if (c == EOF)
			return 0;
		else if (qpos == -1)    /* accept, e.g., empty args: cmd foo args "" */
			goto repeat;
	}
	if (!nonkw) {
		for (i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
			if (strcmp(buf, keywords[i].word) == 0)
				return keywords[i].token;
		}
	}
	if ((str = strdup(buf)) == NULL)
		err(1, "strdup");
	yylval.str = str;
	return TSTRING;
}

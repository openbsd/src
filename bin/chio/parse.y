/*	$OpenBSD: parse.y,v 1.7 2007/10/11 14:39:15 deraadt Exp $ */

/*
 * Copyright (c) 2006 Bob Beck <beck@openbsd.org>
 * Copyright (c) 2002-2006 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2001 Daniel Hartmeier.  All rights reserved.
 * Copyright (c) 2001 Theo de Raadt.  All rights reserved.
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
#include <sys/socket.h>
#include <sys/queue.h>

#include <ctype.h>
#include <err.h>
#include <libgen.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

struct changer {
	TAILQ_ENTRY(changer)	  entry;
	char			 *name;
	char			**drives;
	u_int			  drivecnt;
};
TAILQ_HEAD(changers, changer)	 changers;
struct changer			*curchanger;

static FILE			*fin = NULL;
static int			 lineno = 1;
static int			 errors = 0;
const char			*infile;

int	 yyerror(const char *, ...);
int	 yyparse(void);
int	 kw_cmp(const void *, const void *);
int	 lookup(char *);
int	 lgetc(int);
int	 lungetc(int);
int	 findeol(void);
int	 yylex(void);

typedef struct {
	union {
		int64_t			 number;
		char			*string;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	CHANGER
%token	DRIVE
%token	ERROR
%token	<v.string>		STRING
%token	<v.number>		NUMBER
%%

grammar		: /* empty */
		| grammar '\n'
		| grammar changer '\n'
		| grammar error '\n'		{ errors++; }
		;

optnl		: '\n' optnl
		|
		;

nl		: '\n' optnl
		;

changer		: CHANGER STRING optnl '{' optnl {
			curchanger = new_changer($2);
		}
		    changeropts_l '}' {
			TAILQ_INSERT_TAIL(&changers, curchanger, entry);
			curchanger = NULL;
		}
		;

changeropts_l	: changeropts_l changeroptsl
		| changeroptsl
		;

changeroptsl	: changeropts nl
		| error nl
		;

changeropts	: DRIVE STRING	{
			void *newp;

			if ((newp = realloc(curchanger->drives,
			    (curchanger->drivecnt + 1) *
			    sizeof(curchanger->drives))) == NULL)
				err(1, NULL);
			curchanger->drives = newp;
			if ((curchanger->drives[curchanger->drivecnt] =
			    strdup($2)) == NULL)
				err(1, NULL);
			curchanger->drivecnt++;
			free($2);
		}
		;

%%

struct keywords {
	const char	*k_name;
	int		 k_val;
};

int
yyerror(const char *fmt, ...)
{
	va_list		 ap;
	char		*nfmt;

	errors = 1;
	va_start(ap, fmt);
	if (asprintf(&nfmt, "%s:%d: %s", infile, yylval.lineno, fmt) == -1)
		err(1, "yyerror asprintf");
	err(1, nfmt, ap);
	va_end(ap);
	free(nfmt);
	return (0);
}

int
kw_cmp(const void *k, const void *e)
{
	return (strcmp(k, ((const struct keywords *)e)->k_name));
}

int
lookup(char *s)
{
	/* this has to be sorted always */
	static const struct keywords keywords[] = {
		{ "changer",		CHANGER},
		{ "drive",		DRIVE}
	};
	const struct keywords	*p;

	p = bsearch(s, keywords, sizeof(keywords)/sizeof(keywords[0]),
	    sizeof(keywords[0]), kw_cmp);

	if (p)
		return (p->k_val);
	else
		return (STRING);
}

#define MAXPUSHBACK	128

char	*parsebuf;
int	 parseindex;
char	 pushback_buffer[MAXPUSHBACK];
int	 pushback_index = 0;

int
lgetc(int inquot)
{
	int	c, next;
	FILE *f = fin;

	if (parsebuf) {
		/* Read character from the parsebuffer instead of input. */
		if (parseindex >= 0) {
			c = parsebuf[parseindex++];
			if (c != '\0')
				return (c);
			parsebuf = NULL;
		} else
			parseindex++;
	}

	if (pushback_index)
		return (pushback_buffer[--pushback_index]);

	if (inquot) {
		c = getc(f);
		return (c);
	}

	while ((c = getc(f)) == '\\') {
		next = getc(f);
		if (next != '\n') {
			c = next;
			break;
		}
		yylval.lineno = lineno;
		lineno++;
	}
	if (c == '\t' || c == ' ') {
		/* Compress blanks to a single space. */
		do {
			c = getc(f);
		} while (c == '\t' || c == ' ');
		ungetc(c, f);
		c = ' ';
	}

	return (c);
}

int
lungetc(int c)
{
	if (c == EOF)
		return (EOF);
	if (parsebuf) {
		parseindex--;
		if (parseindex >= 0)
			return (c);
	}
	if (pushback_index < MAXPUSHBACK-1)
		return (pushback_buffer[pushback_index++] = c);
	else
		return (EOF);
}

int
findeol(void)
{
	int	c;

	parsebuf = NULL;
	pushback_index = 0;

	/* skip to either EOF or the first real EOL */
	while (1) {
		c = lgetc(0);
		if (c == '\n') {
			lineno++;
			break;
		}
		if (c == EOF)
			break;
	}
	return (ERROR);
}

int
yylex(void)
{
	char	 buf[8096];
	char	*p;
	int	 endc, next, c;
	int	 token;

	p = buf;
	while ((c = lgetc(0)) == ' ')
		; /* nothing */

	yylval.lineno = lineno;
	if (c == '#')
		while ((c = lgetc(0)) != '\n' && c != EOF)
			; /* nothing */

	switch (c) {
	case '\'':
	case '"':
		endc = c;
		while (1) {
			if ((c = lgetc(1)) == EOF)
				return (0);
			if (c == '\n') {
				lineno++;
				continue;
			} else if (c == '\\') {
				if ((next = lgetc(1)) == EOF)
					return (0);
				if (next == endc)
					c = next;
				else
					lungetc(next);
			} else if (c == endc) {
				*p = '\0';
				break;
			}
			if (p + 1 >= buf + sizeof(buf) - 1) {
				yyerror("string too long");
				return (findeol());
			}
			*p++ = (char)c;
		}
		yylval.v.string = strdup(buf);
		if (yylval.v.string == NULL)
			err(1, "yylex: strdup");
		return (STRING);
	}

#define allowed_to_end_number(x) \
	(isspace(x) || x == ')' || x ==',' || x == '/' || x == '}')

	if (c == '-' || isdigit(c)) {
		do {
			*p++ = c;
			if ((unsigned)(p-buf) >= sizeof(buf)) {
				yyerror("string too long");
				return (findeol());
			}
		} while ((c = lgetc(0)) != EOF && isdigit(c));
		lungetc(c);
		if (p == buf + 1 && buf[0] == '-')
			goto nodigits;
		if (c == EOF || allowed_to_end_number(c)) {
			const char *errstr = NULL;

			*p = '\0';
			yylval.v.number = strtonum(buf, LLONG_MIN,
			    LLONG_MAX, &errstr);
			if (errstr) {
				yyerror("\"%s\" invalid number: %s",
				    buf, errstr);
				return (findeol());
			}
			return (NUMBER);
		} else {
nodigits:
			while (p > buf + 1)
				lungetc(*--p);
			c = *--p;
			if (c == '-')
				return (c);
		}
	}

#define allowed_in_string(x) \
	(isalnum(x) || (ispunct(x) && x != '(' && x != ')' && \
	x != '{' && x != '}' && x != '<' && x != '>' && \
	x != '!' && x != '=' && x != '/' && x != '#' && \
	x != ','))

	if (isalnum(c) || c == ':' || c == '_' || c == '*') {
		do {
			*p++ = c;
			if ((unsigned)(p-buf) >= sizeof(buf)) {
				yyerror("string too long");
				return (findeol());
			}
		} while ((c = lgetc(0)) != EOF && (allowed_in_string(c)));
		lungetc(c);
		*p = '\0';
		if ((token = lookup(buf)) == STRING)
			if ((yylval.v.string = strdup(buf)) == NULL)
				err(1, "yylex: strdup");
		return (token);
	}
	if (c == '\n') {
		yylval.lineno = lineno;
		lineno++;
	}
	if (c == EOF)
		return (0);
	return (c);
}

char *
parse_tapedev(const char *filename, const char *changer, int drive)
{
	struct changer	*p;
	char *tapedev = NULL;

	lineno = 1;
	errors = 0;
	TAILQ_INIT(&changers);

	if ((fin = fopen(filename, "r")) == NULL)
		goto guess;

	infile = filename;

	yyparse();

	fclose(fin);

	TAILQ_FOREACH(p, &changers, entry) {
		if (strcmp(basename(changer), p->name) == 0) {
			if (drive >= 0 && drive < p->drivecnt) {
				if (asprintf(&tapedev, "/dev/%s",
				     p->drives[drive]) == -1)
					errx(1, "malloc failed");
			} else
				tapedev = NULL;
		}
	}

guess:
	/* if no device found, do the default of /dev/rstX */
	if (tapedev == NULL)
		if (asprintf(&tapedev, "/dev/rst%d", drive) == -1)
			errx(1, "malloc failed");
	return (tapedev);
}

struct changer *
new_changer(char *name)
{
	struct changer	*p;

	if ((p = calloc(1, sizeof(*p))) == NULL)
		err(1, NULL);

	if ((p->name = strdup(name)) == NULL)
		err(1, NULL);

	return (p);
}



/*	$OpenBSD: parse.y,v 1.2 2005/04/13 18:28:45 henning Exp $	*/

/*
 * Copyright (c) 2004, 2005 Reyk Floeter <reyk@vantronix.net>
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
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/if_llc.h>
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "hostapd.h"

extern struct hostapd_config hostapd_cfg;

static FILE *fin = NULL;
static int lineno = 1;
static int errors = 0;
char *infile;

TAILQ_HEAD(symhead, sym)	 symhead = TAILQ_HEAD_INITIALIZER(symhead);
struct sym {
	TAILQ_ENTRY(sym)	 entry;
	int			 used;
	int			 persist;
	char			*nam;
	char			*val;
};

int	 yyerror(const char *, ...);
int	 yyparse(void);
int	 kw_cmp(const void *, const void *);
int	 lookup(char *);
int	 lgetc(FILE *);
int	 lungetc(int);
int	 findeol(void);
int	 yylex(void);
int	 symset(const char *, const char *, int);
char	*symget(const char *);

typedef struct {
	union {
		char	*string;
		int	 val;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	MODE INTERFACE IAPP HOSTAP MULTICAST BROADCAST SET
%token	ERROR
%token	<v.string>	STRING
%token	<v.val>		VALUE
%type	<v.string>	string

%%

/*
 * Configuration grammar
 */

grammar		: /* empty */
		| grammar '\n'
		| grammar option '\n'
		| grammar varset '\n'
		| grammar error '\n'		{ errors++; }
		;

option		: SET HOSTAP INTERFACE STRING
		{
			strlcpy(hostapd_cfg.c_apme_iface, $4,
			    sizeof(hostapd_cfg.c_apme_iface));

			hostapd_cfg.c_flags |= HOSTAPD_CFG_F_APME;

			hostapd_log(HOSTAPD_LOG_DEBUG,
			    "parse %s: Host AP interface %s\n",
			    hostapd_cfg.c_config, $4);

			free($4);
		}
		| SET IAPP INTERFACE STRING
		{
			strlcpy(hostapd_cfg.c_iapp_iface, $4,
			    sizeof(hostapd_cfg.c_iapp_iface));

			hostapd_cfg.c_flags |= HOSTAPD_CFG_F_IAPP;

			hostapd_log(HOSTAPD_LOG_DEBUG, "parse %s: "
			    "IAPP interface %s\n", hostapd_cfg.c_config, $4);

			free($4);
		}
		| SET IAPP MODE MULTICAST
		{
			hostapd_cfg.c_flags &= ~HOSTAPD_CFG_F_BRDCAST;
		}
		| SET IAPP MODE BROADCAST
		{
			hostapd_cfg.c_flags |= HOSTAPD_CFG_F_BRDCAST;
		}
		;

string		: string STRING				{
			if (asprintf(&$$, "%s %s", $1, $2) == -1)
				hostapd_fatal("string: asprintf");
			free($1);
			free($2);
		}
		| STRING
		;

varset		: STRING '=' string
		{
			if (symset($1, $3, 0) == -1)
				hostapd_fatal("cannot store variable");
			free($1);
			free($3);
		}
		;

%%

/*
 * Parser and lexer
 */

struct keywords {
	char *k_name;
	int k_val;
};

int
kw_cmp(const void *a, const void *b)
{
	return strcmp(a, ((const struct keywords *)b)->k_name);
}

int
lookup(char *token)
{
	/* Keep this list sorted */
	static const struct keywords keywords[] = {
		{ "broadcast",	BROADCAST },
		{ "hostap",	HOSTAP },
		{ "iapp",	IAPP },
		{ "interface",	INTERFACE },
		{ "mode",	MODE },
		{ "multicast",	MULTICAST },
		{ "set",	SET },
	};
	const struct keywords *p;

	p = bsearch(token, keywords, sizeof(keywords) / sizeof(keywords[0]),
	    sizeof(keywords[0]), kw_cmp);

	return (p == NULL ? STRING : p->k_val);
}

#define MAXPUSHBACK	128

char	*parsebuf;
int	 parseindex;
char	 pushback_buffer[MAXPUSHBACK];
int	 pushback_index = 0;

int
lgetc(FILE *f)
{
	int	c, next;

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

	while ((c = getc(f)) == '\\') {
		next = getc(f);
		if (next != '\n') {
			if (isspace(next))
				yyerror("whitespace after \\");
			ungetc(next, f);
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
		c = lgetc(fin);
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
	char	*p, *val;
	int	 endc, c;
	int	 token;

top:
	p = buf;
	while ((c = lgetc(fin)) == ' ')
		; /* nothing */

	yylval.lineno = lineno;
	if (c == '#')
		while ((c = lgetc(fin)) != '\n' && c != EOF)
			; /* nothing */
	if (c == '$' && parsebuf == NULL) {
		while (1) {
			if ((c = lgetc(fin)) == EOF)
				return (0);

			if (p + 1 >= buf + sizeof(buf) - 1) {
				yyerror("string too long");
				return (findeol());
			}
			if (isalnum(c) || c == '_') {
				*p++ = (char)c;
				continue;
			}
			*p = '\0';
			lungetc(c);
			break;
		}
		val = symget(buf);
		if (val == NULL) {
			yyerror("macro \"%s\" not defined", buf);
			return (findeol());
		}
		parsebuf = val;
		parseindex = 0;
		goto top;
	}

	switch (c) {
	case '\'':
	case '"':
		endc = c;
		while (1) {
			if ((c = lgetc(fin)) == EOF)
				return (0);
			if (c == endc) {
				*p = '\0';
				break;
			}
			if (c == '\n') {
				lineno++;
				continue;
			}
			if (p + 1 >= buf + sizeof(buf) - 1) {
				yyerror("string too long");
				return (findeol());
			}
			*p++ = (char)c;
		}
		yylval.v.string = strdup(buf);
		if (yylval.v.string == NULL)
			hostapd_fatal("yylex: strdup");
		return (STRING);
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
		} while ((c = lgetc(fin)) != EOF && (allowed_in_string(c)));
		lungetc(c);
		*p = '\0';
		if ((token = lookup(buf)) == STRING)
			if ((yylval.v.string = strdup(buf)) == NULL)
				hostapd_fatal("yylex: strdup");
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

int
symset(const char *nam, const char *val, int persist)
{
	struct sym	*sym;

	for (sym = TAILQ_FIRST(&symhead); sym && strcmp(nam, sym->nam);
	    sym = TAILQ_NEXT(sym, entry))
		;	/* nothing */

	if (sym != NULL) {
		if (sym->persist == 1)
			return (0);
		else {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
			free(sym);
		}
	}
	if ((sym = calloc(1, sizeof(*sym))) == NULL)
		return (-1);

	sym->nam = strdup(nam);
	if (sym->nam == NULL) {
		free(sym);
		return (-1);
	}
	sym->val = strdup(val);
	if (sym->val == NULL) {
		free(sym->nam);
		free(sym);
		return (-1);
	}
	sym->used = 0;
	sym->persist = persist;
	TAILQ_INSERT_TAIL(&symhead, sym, entry);

	hostapd_log(HOSTAPD_LOG_DEBUG, "%s = \"%s\"\n", sym->nam, sym->val);

	return (0);
}

int
hostapd_parse_symset(char *s)
{
	char	*sym, *val;
	int	ret;
	size_t	len;

	if ((val = strrchr(s, '=')) == NULL)
		return (-1);

	len = strlen(s) - strlen(val) + 1;
	if ((sym = malloc(len)) == NULL)
		hostapd_fatal("cmdline_symset: malloc");

	strlcpy(sym, s, len);

	ret = symset(sym, val + 1, 1);

	free(sym);

	return (ret);
}

char *
symget(const char *nam)
{
	struct sym	*sym;

	TAILQ_FOREACH(sym, &symhead, entry)
		if (strcmp(nam, sym->nam) == 0) {
			sym->used = 1;
			return (sym->val);
		}
	return (NULL);
}

#if 0
int
yylex(void)
{
	char *p;
	int v;

	/* Locate next token */
	if (confptr == NULL) {
		confptr = confbuf;
	} else {
		for (p = confptr; *p && p < confbuf + conflen; p++)
			;
		*p++;
		if (!*p)
			return 0;
		confptr = p;
	}

	/* Numerical token? */
	if (isdigit(*confptr)) {
		for (p = confptr; *p; p++)
			if (*p == '.') /* IP-address, or bad input */
				goto is_string;
		v = (int)strtol(confptr, (char **)NULL, 10);
		yylval.val = v;
		return VALUE;
	}

 is_string:
	if ((v = lookup(confptr)) == STRING) {
		yylval.string = strdup(confptr);
		if (yylval.string == NULL)
			hostapd_fatal("yylex: strdup()");
	}
	return v;
}
#endif

int
hostapd_parse_file(struct hostapd_config *cfg)
{
	struct sym *sym, *next;
	int ret;

	if ((fin = fopen(cfg->c_config, "r")) == NULL) {
		hostapd_log(HOSTAPD_LOG, "unable to find %s, using defaults\n",
		    cfg->c_config);
		return (0);
	}
	infile = cfg->c_config;

	if (hostapd_check_file_secrecy(fileno(fin), cfg->c_config)) {
		hostapd_fatal("invalid permissions for %s\n", cfg->c_config);
		fclose(fin);
		return (-1);
	}

	lineno = 1;
	errors = 0;

	ret = yyparse();

	fclose(fin);

	/* Free macros and check which have not been used. */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entry);
		if (!sym->used)
			hostapd_log(HOSTAPD_LOG_VERBOSE,
			    "warning: macro \"%s\" not used\n", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
			free(sym);
		}
	}

	return (ret);
}

int
yyerror(const char *fmt, ...)
{
	va_list ap;
	char *nfmt;

	errors = 1;

	va_start(ap, fmt);
	if (asprintf(&nfmt, "%s:%d: %s\n", infile, yylval.lineno, fmt) == -1)
		hostapd_fatal("yyerror asprintf");
	hostapd_log(HOSTAPD_LOG, nfmt, ap);
	va_end(ap);
	free(nfmt);

	return (0);
}


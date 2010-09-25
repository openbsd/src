/*	$OpenBSD: parse.y,v 1.2 2010/09/25 16:23:01 sobrado Exp $ */

/*
 * Copyright (c) 2010 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2004, 2005 Esben Norby <norby@openbsd.org>
 * Copyright (c) 2004 Ryan McBride <mcbride@openbsd.org>
 * Copyright (c) 2002, 2003, 2004 Henning Brauer <henning@openbsd.org>
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
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <limits.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "iscsid.h"
#include "iscsictl.h"

TAILQ_HEAD(files, file)		 files = TAILQ_HEAD_INITIALIZER(files);
static struct file {
	TAILQ_ENTRY(file)	 entry;
	FILE			*stream;
	char			*name;
	int			 lineno;
	int			 errors;
} *file, *topfile;
struct file	*pushfile(const char *, int);
int		 popfile(void);
int		 yyparse(void);
int		 yylex(void);
int		 yyerror(const char *, ...);
int		 kw_cmp(const void *, const void *);
int		 lookup(char *);
int		 lgetc(int);
int		 lungetc(int);
int		 findeol(void);

void		 clear_config(struct iscsi_config *);

TAILQ_HEAD(symhead, sym)	 symhead = TAILQ_HEAD_INITIALIZER(symhead);
struct sym {
	TAILQ_ENTRY(sym)	 entry;
	int			 used;
	int			 persist;
	char			*nam;
	char			*val;
};
int		 symset(const char *, const char *, int);
char		*symget(const char *);

static int			 errors;
static struct iscsi_config	*conf;
static struct session_config	*session;

struct addrinfo_opts {
	int	af;
	char	*port;
} addrinfo_opts;

typedef struct {
	union {
		int			 i;
		int64_t			 number;
		char			*string;
		struct addrinfo_opts	 addrinfo_opts;
		struct addrinfo		*addrinfo;
	} v;
	int lineno;
} YYSTYPE;

%}

%token  TARGET TARGETNAME TARGETADDR
%token	INITIATORNAME INITIATORADDR
%token	ENABLED DISABLED NORMAL DISCOVERY
%token  ADDRESS INET INET6 PORT
%token	INCLUDE
%token	ERROR
%token	<v.string>		STRING
%token	<v.number>		NUMBER
%type	<v.i>			af state type
%type	<v.string>		port
%type	<v.addrinfo>		addrinfo
%type	<v.addrinfo_opts>	addrinfo_opts addrinfo_opts_l addrinfo_opt
%type	<v.string>		string

%%

grammar		: /* empty */
		| grammar '\n'
		| grammar include '\n'
		| grammar varset '\n'
		| grammar target '\n'
		| grammar error '\n'		{ file->errors++; }
		;

include		: INCLUDE STRING		{
			struct file	*nfile;

			if ((nfile = pushfile($2, 1)) == NULL) {
				yyerror("failed to include file %s", $2);
				free($2);
				YYERROR;
			}
			free($2);

			file = nfile;
			lungetc('\n');
		}
		;

string		: string STRING	{
			if (asprintf(&$$, "%s %s", $1, $2) == -1) {
				free($1);
				free($2);
				yyerror("string: asprintf");
				YYERROR;
			}
			free($1);
			free($2);
		}
		| STRING
		;

varset		: STRING '=' string		{
			if (symset($1, $3, 0) == -1)
				err(1, "cannot store variable");
			free($1);
			free($3);
		}
		;

optnl		: '\n' optnl
		|
		;

nl		: '\n' optnl		/* one or more newlines */
		;

target		: TARGET STRING {
			struct session_ctlcfg *scelm;

			scelm = calloc(1, sizeof(*scelm));
			session = &scelm->session;
			if (strlcpy(session->SessionName, $2,
			    sizeof(session->SessionName)) >=
			    sizeof(session->SessionName)) {
				free($2);
				free(scelm);
				yyerror("target name \"%s\" too long");
				YYERROR;
			}
			free($2);
			SIMPLEQ_INSERT_TAIL(&conf->sessions, scelm, entry);
		} '{' optnl targetopts_l '}'
		;

targetopts_l	: targetopts_l targetoptsl nl
		| targetoptsl optnl
		;

targetoptsl	: state			{ session->disabled = $1; }
		| type			{ session->SessionType = $1; }
		| TARGETNAME STRING	{ session->TargetName = $2; }
		| INITIATORNAME STRING	{ session->InitiatorName = $2; }
		| TARGETADDR addrinfo	{
			bcopy($2->ai_addr, &session->connection.TargetAddr,
			    $2->ai_addr->sa_len);
			freeaddrinfo($2);
		}
		| INITIATORADDR addrinfo {
			bcopy($2->ai_addr, &session->connection.LocalAddr,
			    $2->ai_addr->sa_len);
			freeaddrinfo($2);
		}
		;

addrinfo	: STRING addrinfo_opts {
			struct addrinfo hints;
			char *hostname;
			int error;

			$$ = NULL;

			if ($2.port == NULL) {
				if (($2.port = strdup("iscsi")) == NULL) {
					free($1);
					yyerror("port strdup");
					YYERROR;
				}
			}

			memset(&hints, 0, sizeof(hints));
			hints.ai_family = $2.af;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;

			if (strcmp($1, "*") == 0) {
				hostname = NULL;
				hints.ai_flags = AI_PASSIVE;
			} else
				hostname = $1;

			error = getaddrinfo(hostname, $2.port, &hints, &$$);
			if (error) {
				yyerror("%s (%s %s)", gai_strerror(error),
				    $1, $2.port);
				free($1);
				free($2.port);
				YYERROR;
			}

			free($1);
			free($2.port);
		}
		;

addrinfo_opts	: {
			addrinfo_opts.port = NULL;
			addrinfo_opts.af = PF_UNSPEC;
		}
			addrinfo_opts_l { $$ = addrinfo_opts; }
		| /* empty */ {
			addrinfo_opts.port = NULL;
			addrinfo_opts.af = PF_UNSPEC;
			$$ = addrinfo_opts;
		}
		;

addrinfo_opts_l	: addrinfo_opts_l addrinfo_opt
		| addrinfo_opt
		;

addrinfo_opt	: port {
			if (addrinfo_opts.port != NULL) {
				yyerror("port cannot be redefined");
				YYERROR;
			}
			addrinfo_opts.port = $1;
		}
		| af {
			if (addrinfo_opts.af != PF_UNSPEC) {
				yyerror("address family cannot be redefined");
				YYERROR;
			}
			addrinfo_opts.af = $1;
		}
		;

port		: PORT STRING			{ $$ = $2; }
		;

af		: INET				{ $$ = PF_INET; }
		| INET6				{ $$ = PF_INET6; }
		;

state		: ENABLED			{ $$ = 0; }
		| DISABLED			{ $$ = 1; }
		;

type		: NORMAL			{ $$ = SESSION_TYPE_NORMAL; }
		| DISCOVERY			{ $$ = SESSION_TYPE_DISCOVERY; }
		;


%%

struct keywords {
	const char	*k_name;
	int		 k_val;
};

int
yyerror(const char *fmt, ...)
{
	va_list	ap;

	file->errors++;
	va_start(ap, fmt);
	fprintf(stderr, "%s:%d: ", file->name, yylval.lineno);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
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
		{"address",		ADDRESS},
		{"disabled",		DISABLED},
		{"discovery",		DISCOVERY},
		{"enabled",		ENABLED},
		{"include",		INCLUDE},
		{"inet",		INET},
		{"inet4",		INET},
		{"inet6",		INET6},
		{"initiatoraddr",	INITIATORADDR},
		{"initiatorname",	INITIATORNAME},
		{"normal",		NORMAL},
		{"port",		PORT},
		{"target",		TARGET},
		{"targetaddr",		TARGETADDR},
		{"targetname",		TARGETNAME}
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
int	 pushback_index;

int
lgetc(int quotec)
{
	int		c, next;

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

	if (quotec) {
		if ((c = getc(file->stream)) == EOF) {
			yyerror("reached end of file while parsing "
			    "quoted string");
			if (file == topfile || popfile() == EOF)
				return (EOF);
			return (quotec);
		}
		return (c);
	}

	while ((c = getc(file->stream)) == '\\') {
		next = getc(file->stream);
		if (next != '\n') {
			c = next;
			break;
		}
		yylval.lineno = file->lineno;
		file->lineno++;
	}

	while (c == EOF) {
		if (file == topfile || popfile() == EOF)
			return (EOF);
		c = getc(file->stream);
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

	/* skip to either EOF or the first real EOL */
	while (1) {
		if (pushback_index)
			c = pushback_buffer[--pushback_index];
		else
			c = lgetc(0);
		if (c == '\n') {
			file->lineno++;
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
	int	 quotec, next, c;
	int	 token;

top:
	p = buf;
	while ((c = lgetc(0)) == ' ' || c == '\t')
		; /* nothing */

	yylval.lineno = file->lineno;
	if (c == '#')
		while ((c = lgetc(0)) != '\n' && c != EOF)
			; /* nothing */
	if (c == '$' && parsebuf == NULL) {
		while (1) {
			if ((c = lgetc(0)) == EOF)
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
			yyerror("macro '%s' not defined", buf);
			return (findeol());
		}
		parsebuf = val;
		parseindex = 0;
		goto top;
	}

	switch (c) {
	case '\'':
	case '"':
		quotec = c;
		while (1) {
			if ((c = lgetc(quotec)) == EOF)
				return (0);
			if (c == '\n') {
				file->lineno++;
				continue;
			} else if (c == '\\') {
				if ((next = lgetc(quotec)) == EOF)
					return (0);
				if (next == quotec || c == ' ' || c == '\t')
					c = next;
				else if (next == '\n') {
					file->lineno++;
					continue;
				} else
					lungetc(next);
			} else if (c == quotec) {
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
	(isspace(x) || x == ')' || x ==',' || x == '/' || x == '}' || x == '=')

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
	x != '{' && x != '}' && \
	x != '!' && x != '=' && x != '#' && \
	x != ','))

	if (isalnum(c) || c == ':' || c == '_') {
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
		yylval.lineno = file->lineno;
		file->lineno++;
	}
	if (c == EOF)
		return (0);
	return (c);
}

struct file *
pushfile(const char *name, int secret)
{
	struct file	*nfile;

	if ((nfile = calloc(1, sizeof(struct file))) == NULL) {
		warn("malloc");
		return (NULL);
	}
	if ((nfile->name = strdup(name)) == NULL) {
		warn("malloc");
		free(nfile);
		return (NULL);
	}
	if ((nfile->stream = fopen(nfile->name, "r")) == NULL) {
		warn("%s", nfile->name);
		free(nfile->name);
		free(nfile);
		return (NULL);
	}
	nfile->lineno = 1;
	TAILQ_INSERT_TAIL(&files, nfile, entry);
	return (nfile);
}

int
popfile(void)
{
	struct file	*prev;

	if ((prev = TAILQ_PREV(file, files, entry)) != NULL)
		prev->errors += file->errors;

	TAILQ_REMOVE(&files, file, entry);
	fclose(file->stream);
	free(file->name);
	free(file);
	file = prev;
	return (file ? 0 : EOF);
}

struct iscsi_config *
parse_config(char *filename)
{
	struct sym	*sym, *next;

	file = pushfile(filename, 1);
	if (file == NULL)
		return (NULL);
	topfile = file;

	conf = calloc(1, sizeof(struct iscsi_config));
	if (conf == NULL)
		return (NULL);
	SIMPLEQ_INIT(&conf->sessions);

	yyparse();
	errors = file->errors;
	popfile();

	/* Free macros and check which have not been used. */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entry);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
			free(sym);
		}
	}

	if (errors) {
		clear_config(conf);
		return (NULL);
	}

	return (conf);
}

int
cmdline_symset(char *s)
{
	char	*sym, *val;
	int	ret;
	size_t	len;

	if ((val = strrchr(s, '=')) == NULL)
		return (-1);

	len = strlen(s) - strlen(val) + 1;
	if ((sym = malloc(len)) == NULL)
		errx(1, "cmdline_symset: malloc");

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
	return (0);
}

void
clear_config(struct iscsi_config *c)
{
	struct session_ctlcfg *s;

	while ((s = SIMPLEQ_FIRST(&c->sessions))) {
		SIMPLEQ_REMOVE_HEAD(&c->sessions, entry);
		free(s->session.TargetName);
		free(s->session.InitiatorName);
		free(s);
	}

	free(c);
}

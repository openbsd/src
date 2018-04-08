/*	$OpenBSD: parse.y,v 1.20 2018/04/08 19:10:12 florian Exp $ */

/*
 * Copyright (c) 2016 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2016 Sebastian Benoit <benno@openbsd.org>
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
#include <sys/stat.h>
#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parse.h"

TAILQ_HEAD(files, file)		 files = TAILQ_HEAD_INITIALIZER(files);
static struct file {
	TAILQ_ENTRY(file)	 entry;
	FILE			*stream;
	char			*name;
	int			 lineno;
	int			 errors;
} *file, *topfile;
struct file	*pushfile(const char *);
int		 popfile(void);
int		 yyparse(void);
int		 yylex(void);
int		 yyerror(const char *, ...)
    __attribute__((__format__ (printf, 1, 2)))
    __attribute__((__nonnull__ (1)));
int		 kw_cmp(const void *, const void *);
int		 lookup(char *);
int		 lgetc(int);
int		 lungetc(int);
int		 findeol(void);

struct authority_c	*conf_new_authority(struct acme_conf *, char *);
struct domain_c		*conf_new_domain(struct acme_conf *, char *);
struct keyfile		*conf_new_keyfile(struct acme_conf *, char *);
void			 clear_config(struct acme_conf *);
void			 print_config(struct acme_conf *);
int			 conf_check_file(char *, int);

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

static struct acme_conf		*conf;
static struct authority_c	*auth;
static struct domain_c		*domain;
static int			 errors = 0;

typedef struct {
	union {
		int64_t		 number;
		char		*string;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	AUTHORITY URL API ACCOUNT
%token	DOMAIN ALTERNATIVE NAMES CERT FULL CHAIN KEY SIGN WITH CHALLENGEDIR
%token	YES NO
%token	INCLUDE
%token	ERROR
%token	<v.string>	STRING
%token	<v.number>	NUMBER
%type	<v.string>	string

%%

grammar		: /* empty */
		| grammar include '\n'
		| grammar varset '\n'
		| grammar '\n'
		| grammar authority '\n'
		| grammar domain '\n'
		| grammar error '\n'		{ file->errors++; }
		;

include		: INCLUDE STRING		{
			struct file	*nfile;

			if ((nfile = pushfile($2)) == NULL) {
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
			char *s = $1;
			if (conf->opts & ACME_OPT_VERBOSE)
				printf("%s = \"%s\"\n", $1, $3);
			while (*s++) {
				if (isspace((unsigned char)*s)) {
					yyerror("macro name cannot contain "
					    "whitespace");
					YYERROR;
				}
			}
			if (symset($1, $3, 0) == -1)
				errx(EXIT_FAILURE, "cannot store variable");
			free($1);
			free($3);
		}
		;

optnl		: '\n' optnl
		|
		;

nl		: '\n' optnl		/* one newline or more */
		;

comma		: ','
		| /*empty*/
		;

authority	: AUTHORITY STRING {
			char *s;
			if ((s = strdup($2)) == NULL)
				err(EXIT_FAILURE, "strdup");
			if ((auth = conf_new_authority(conf, s)) == NULL) {
				free(s);
				yyerror("authority already defined");
				YYERROR;
			}
		} '{' optnl authorityopts_l '}' {
			/* XXX enforce minimum config here */
			auth = NULL;
		}
		;

authorityopts_l	: authorityopts_l authorityoptsl nl
		| authorityoptsl optnl
		;

authorityoptsl	: API URL STRING {
			char *s;
			if (auth->api != NULL) {
				yyerror("duplicate api");
				YYERROR;
			}
			if ((s = strdup($3)) == NULL)
				err(EXIT_FAILURE, "strdup");
			auth->api = s;
		}
		| ACCOUNT KEY STRING {
			char *s;
			if (auth->account != NULL) {
				yyerror("duplicate account");
				YYERROR;
			}
			if ((s = strdup($3)) == NULL)
				err(EXIT_FAILURE, "strdup");
			auth->account = s;
		}
		;

domain		: DOMAIN STRING {
			char *s;
			if ((s = strdup($2)) == NULL)
				err(EXIT_FAILURE, "strdup");
			if (!domain_valid(s)) {
				yyerror("%s: bad domain syntax", s);
				free(s);
				YYERROR;
			}
			if ((domain = conf_new_domain(conf, s)) == NULL) {
				free(s);
				yyerror("domain already defined");
				YYERROR;
			}
		} '{' optnl domainopts_l '}' {
			/* enforce minimum config here */
			if (domain->key == NULL) {
				yyerror("no domain key file specified for "
				    "domain %s", domain->domain);
				YYERROR;
			}
			if (domain->cert == NULL && domain->fullchain == NULL) {
				yyerror("at least certificate file or full "
				    "certificate chain file must be specified "
				    "for domain %s", domain->domain);
				YYERROR;
			}
			domain = NULL;
		}
		;

domainopts_l	: domainopts_l domainoptsl nl
		| domainoptsl optnl
		;

domainoptsl	: ALTERNATIVE NAMES '{' altname_l '}'
		| DOMAIN KEY STRING {
			char *s;
			if (domain->key != NULL) {
				yyerror("duplicate key");
				YYERROR;
			}
			if ((s = strdup($3)) == NULL)
				err(EXIT_FAILURE, "strdup");
			if (!conf_check_file(s,
			    (conf->opts & ACME_OPT_NEWDKEY))) {
				free(s);
				YYERROR;
			}
			if ((conf_new_keyfile(conf, s)) == NULL) {
				free(s);
				yyerror("domain key file already used");
				YYERROR;
			}
			domain->key = s;
		}
		| DOMAIN CERT STRING {
			char *s;
			if (domain->cert != NULL) {
				yyerror("duplicate cert");
				YYERROR;
			}
			if ((s = strdup($3)) == NULL)
				err(EXIT_FAILURE, "strdup");
			if (s[0] != '/') {
				free(s);
				yyerror("not an absolute path");
				YYERROR;
			}
			if ((conf_new_keyfile(conf, s)) == NULL) {
				free(s);
				yyerror("domain cert file already used");
				YYERROR;
			}
			domain->cert = s;
		}
		| DOMAIN CHAIN CERT STRING {
			char *s;
			if (domain->chain != NULL) {
				yyerror("duplicate chain");
				YYERROR;
			}
			if ((s = strdup($4)) == NULL)
				err(EXIT_FAILURE, "strdup");
			if ((conf_new_keyfile(conf, s)) == NULL) {
				free(s);
				yyerror("domain chain file already used");
				YYERROR;
			}
			domain->chain = s;
		}
		| DOMAIN FULL CHAIN CERT STRING {
			char *s;
			if (domain->fullchain != NULL) {
				yyerror("duplicate chain");
				YYERROR;
			}
			if ((s = strdup($5)) == NULL)
				err(EXIT_FAILURE, "strdup");
			if ((conf_new_keyfile(conf, s)) == NULL) {
				free(s);
				yyerror("domain full chain file already used");
				YYERROR;
			}
			domain->fullchain = s;
		}
		| SIGN WITH STRING {
			char *s;
			if (domain->auth != NULL) {
				yyerror("duplicate use");
				YYERROR;
			}
			if ((s = strdup($3)) == NULL)
				err(EXIT_FAILURE, "strdup");
			if (authority_find(conf, s) == NULL) {
				yyerror("use: unknown authority");
				free(s);
				YYERROR;
			}
			domain->auth = s;
		}
		| CHALLENGEDIR STRING {
			char *s;
			if (domain->challengedir != NULL) {
				yyerror("duplicate challengedir");
				YYERROR;
			}
			if ((s = strdup($2)) == NULL)
				err(EXIT_FAILURE, "strdup");
			domain->challengedir = s;
		}
		;

altname_l	: altname comma altname_l
		| altname
		;

altname		: STRING {
			char			*s;
			struct altname_c	*ac;
			if (!domain_valid($1)) {
				yyerror("bad domain syntax");
				YYERROR;
			}
			if ((ac = calloc(1, sizeof(struct altname_c))) == NULL)
				err(EXIT_FAILURE, "calloc");
			if ((s = strdup($1)) == NULL) {
				free(ac);
				err(EXIT_FAILURE, "strdup");
			}
			ac->domain = s;
			TAILQ_INSERT_TAIL(&domain->altname_list, ac, entry);
			domain->altname_count++;
			/*
			 * XXX we could check if altname is duplicate
			 * or identical to domain->domain
			*/
		}

%%

struct keywords {
	const char	*k_name;
	int		 k_val;
};

int
yyerror(const char *fmt, ...)
{
	va_list		 ap;
	char		*msg;

	file->errors++;
	va_start(ap, fmt);
	if (vasprintf(&msg, fmt, ap) == -1)
		err(EXIT_FAILURE, "yyerror vasprintf");
	va_end(ap);
	fprintf(stderr, "%s:%d: %s\n", file->name, yylval.lineno, msg);
	free(msg);
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
		{"account",		ACCOUNT},
		{"alternative",		ALTERNATIVE},
		{"api",			API},
		{"authority",		AUTHORITY},
		{"certificate",		CERT},
		{"chain",		CHAIN},
		{"challengedir",	CHALLENGEDIR},
		{"domain",		DOMAIN},
		{"full",		FULL},
		{"include",		INCLUDE},
		{"key",			KEY},
		{"names",		NAMES},
		{"sign",		SIGN},
		{"url",			URL},
		{"with",		WITH},
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

u_char	*parsebuf;
int	 parseindex;
u_char	 pushback_buffer[MAXPUSHBACK];
int	 pushback_index = 0;

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
	u_char	 buf[8096];
	u_char	*p, *val;
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
				*p++ = c;
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
			} else if (c == '\0') {
				yyerror("syntax error");
				return (findeol());
			}
			if (p + 1 >= buf + sizeof(buf) - 1) {
				yyerror("string too long");
				return (findeol());
			}
			*p++ = c;
		}
		yylval.v.string = strdup(buf);
		if (yylval.v.string == NULL)
			err(EXIT_FAILURE, "yylex: strdup");
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
		if ((token = lookup(buf)) == STRING) {
			if ((yylval.v.string = strdup(buf)) == NULL)
				err(EXIT_FAILURE, "yylex: strdup");
		}
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
pushfile(const char *name)
{
	struct file	*nfile;

	if ((nfile = calloc(1, sizeof(struct file))) == NULL) {
		warn("malloc");
		return (NULL);
	}
	if ((nfile->name = strdup(name)) == NULL) {
		warn("strdup");
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

struct acme_conf *
parse_config(const char *filename, int opts)
{
	struct sym	*sym, *next;

	if ((conf = calloc(1, sizeof(struct acme_conf))) == NULL)
		err(EXIT_FAILURE, "parse_config");
	conf->opts = opts;

	if ((file = pushfile(filename)) == NULL) {
		free(conf);
		return (NULL);
	}
	topfile = file;

	TAILQ_INIT(&conf->authority_list);
	TAILQ_INIT(&conf->domain_list);

	yyparse();
	errors = file->errors;
	popfile();

	/* Free macros and check which have not been used. */
	TAILQ_FOREACH_SAFE(sym, &symhead, entry, next) {
		if ((conf->opts & ACME_OPT_VERBOSE) && !sym->used)
			fprintf(stderr, "warning: macro '%s' not "
			    "used\n", sym->nam);
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

	if (opts & ACME_OPT_CHECK)
		print_config(conf);

	return (conf);
}

int
symset(const char *nam, const char *val, int persist)
{
	struct sym	*sym;

	TAILQ_FOREACH(sym, &symhead, entry) {
		if (strcmp(nam, sym->nam) == 0)
			break;
	}

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
		errx(EXIT_FAILURE, "cmdline_symset: malloc");

	strlcpy(sym, s, len);

	ret = symset(sym, val + 1, 1);
	free(sym);

	return (ret);
}

char *
symget(const char *nam)
{
	struct sym	*sym;

	TAILQ_FOREACH(sym, &symhead, entry) {
		if (strcmp(nam, sym->nam) == 0) {
			sym->used = 1;
			return (sym->val);
		}
	}
	return (NULL);
}

struct authority_c *
conf_new_authority(struct acme_conf *c, char *s)
{
	struct authority_c *a;

	a = authority_find(c, s);
	if (a)
		return (NULL);
	if ((a = calloc(1, sizeof(struct authority_c))) == NULL)
		err(EXIT_FAILURE, "calloc");
	TAILQ_INSERT_TAIL(&c->authority_list, a, entry);

	a->name = s;
	return (a);
}

struct authority_c *
authority_find(struct acme_conf *c, char *s)
{
	struct authority_c	*a;

	TAILQ_FOREACH(a, &c->authority_list, entry) {
		if (strncmp(a->name, s, AUTH_MAXLEN) == 0) {
			return (a);
		}
	}
	return (NULL);
}

struct authority_c *
authority_find0(struct acme_conf *c)
{
	return (TAILQ_FIRST(&c->authority_list));
}

struct domain_c *
conf_new_domain(struct acme_conf *c, char *s)
{
	struct domain_c *d;

	d = domain_find(c, s);
	if (d)
		return (NULL);
	if ((d = calloc(1, sizeof(struct domain_c))) == NULL)
		err(EXIT_FAILURE, "calloc");
	TAILQ_INSERT_TAIL(&c->domain_list, d, entry);

	d->domain = s;
	TAILQ_INIT(&d->altname_list);

	return (d);
}

struct domain_c *
domain_find(struct acme_conf *c, char *s)
{
	struct domain_c	*d;

	TAILQ_FOREACH(d, &c->domain_list, entry) {
		if (strncmp(d->domain, s, DOMAIN_MAXLEN) == 0) {
			return (d);
		}
	}
	return (NULL);
}

struct keyfile *
conf_new_keyfile(struct acme_conf *c, char *s)
{
	struct keyfile *k;

	LIST_FOREACH(k, &c->used_key_list, entry) {
		if (strncmp(k->name, s, PATH_MAX) == 0) {
			return (NULL);
		}
	}

	if ((k = calloc(1, sizeof(struct keyfile))) == NULL)
		err(EXIT_FAILURE, "calloc");
	LIST_INSERT_HEAD(&c->used_key_list, k, entry);

	k->name = s;
	return (k);
}

void
clear_config(struct acme_conf *xconf)
{
	struct authority_c	*a;
	struct domain_c		*d;
	struct altname_c	*ac;

	while ((a = TAILQ_FIRST(&xconf->authority_list)) != NULL) {
		TAILQ_REMOVE(&xconf->authority_list, a, entry);
		free(a);
	}
	while ((d = TAILQ_FIRST(&xconf->domain_list)) != NULL) {
		while ((ac = TAILQ_FIRST(&d->altname_list)) != NULL) {
			TAILQ_REMOVE(&d->altname_list, ac, entry);
			free(ac);
		}
		TAILQ_REMOVE(&xconf->domain_list, d, entry);
		free(d);
	}
	free(xconf);
}

void
print_config(struct acme_conf *xconf)
{
	struct authority_c	*a;
	struct domain_c		*d;
	struct altname_c	*ac;
	int			 f;

	TAILQ_FOREACH(a, &xconf->authority_list, entry) {
		printf("authority %s {\n", a->name);
		if (a->api != NULL)
			printf("\tapi url \"%s\"\n", a->api);
		if (a->account != NULL)
			printf("\taccount key \"%s\"\n", a->account);
		printf("}\n\n");
	}
	TAILQ_FOREACH(d, &xconf->domain_list, entry) {
		f = 0;
		printf("domain %s {\n", d->domain);
		TAILQ_FOREACH(ac, &d->altname_list, entry) {
			if (!f)
				printf("\talternative names { ");
			if (ac->domain != NULL) {
				printf("%s%s", f ? ", " : " ", ac->domain);
				f = 1;
			}
		}
		if (f)
			printf("}\n");
		if (d->key != NULL)
			printf("\tdomain key \"%s\"\n", d->key);
		if (d->cert != NULL)
			printf("\tdomain certificate \"%s\"\n", d->cert);
		if (d->chain != NULL)
			printf("\tdomain chain certificate \"%s\"\n", d->chain);
		if (d->fullchain != NULL)
			printf("\tdomain full chain certificate \"%s\"\n",
			    d->fullchain);
		if (d->auth != NULL)
			printf("\tsign with \"%s\"\n", d->auth);
		if (d->challengedir != NULL)
			printf("\tchallengedir \"%s\"\n", d->challengedir);
		printf("}\n\n");
	}
}

/*
 * This isn't RFC1035 compliant, but does the bare minimum in making
 * sure that we don't get bogus domain names on the command line, which
 * might otherwise screw up our directory structure.
 * Returns zero on failure, non-zero on success.
 */
int
domain_valid(const char *cp)
{

	for ( ; *cp != '\0'; cp++)
		if (!(*cp == '.' || *cp == '-' ||
		    *cp == '_' || isalnum((int)*cp)))
			return (0);
	return (1);
}

int
conf_check_file(char *s, int dontstat)
{
	struct stat st;

	if (s[0] != '/') {
		warnx("%s: not an absolute path", s);
		return (0);
	}
	if (dontstat)
		return (1);
	if (stat(s, &st)) {
		warn("cannot stat %s", s);
		return (0);
	}
	if (st.st_mode & (S_IRWXG | S_IRWXO)) {
		warnx("%s: group read/writable or world read/writable", s);
		return (0);
	}
	return (1);
}

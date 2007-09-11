/*	$OpenBSD: parse.y,v 1.8 2007/09/11 23:06:37 deraadt Exp $ */

/*
 * Copyright (c) 2006 Michele Marchetto <mydecay@openbeer.it>
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <ifaddrs.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ripd.h"
#include "rip.h"
#include "ripe.h"
#include "log.h"

static struct ripd_conf		*conf;
static FILE			*fin = NULL;
static int			 lineno = 1;
static int			 errors = 0;
char				*infile;
char				*start_state;

struct iface	*iface = NULL;

int		yyerror(const char *, ...);
int		yyparse(void);
int		kw_cmp(const void *, const void *);
int		lookup(char *);
int		lgetc(FILE *);
int		lungetc(int);
int		findeol(void);
int		yylex(void);
void		clear_config(struct ripd_conf *);
int		check_file_secrecy(int, const char *);
u_int32_t	get_rtr_id(void);
int		host(const char *, struct in_addr *, struct in_addr *);

static struct {
	char			 auth_key[MAX_SIMPLE_AUTH_LEN];
	struct auth_md_head	 md_list;
	enum auth_type		 auth_type;
	u_int8_t		 auth_keyid;
	u_int8_t		 cost;
} *defs, globaldefs, ifacedefs;

TAILQ_HEAD(symhead, sym)	 symhead = TAILQ_HEAD_INITIALIZER(symhead);
struct sym {
	TAILQ_ENTRY(sym)	 entries;
	int			 used;
	int			 persist;
	char			*nam;
	char			*val;
};

int			 symset(const char *, const char *, int);
char			*symget(const char *);
struct iface		*conf_get_if(struct kif *);

typedef struct {
	union {
		int64_t		 number;
		char		*string;
	} v;
	int lineno;
} YYSTYPE;

%}

%token  SPLIT_HORIZON TRIGGERED_UPDATES FIBUPDATE REDISTRIBUTE
%token	AUTHKEY AUTHTYPE AUTHMD AUTHMDKEYID
%token  INTERFACE RTLABEL
%token	COST PASSIVE
%token	YES NO
%token  ERROR
%token  <v.string>	STRING
%token  <v.number>	NUMBER
%type   <v.number>	yesno no
%type   <v.string>	string

%%

grammar		: /* empty */
		| grammar '\n'
		| grammar conf_main '\n'
		| grammar varset '\n'
		| grammar interface '\n'
		;

string		: string STRING {
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

yesno		: YES	{ $$ = 1; }
		| NO	{ $$ = 0; }
		;

no		: /* empty */	{ $$ = 0; }
		| NO		{ $$ = 1; }

varset		: STRING '=' string {
			if (conf->opts & RIPD_OPT_VERBOSE)
				printf("%s = \"%s\"\n", $1, $3);
			if (symset($1, $3, 0) == -1)
				fatal("cannot store variable");
			free($1);
			free($3);
		}
		;

conf_main	: SPLIT_HORIZON STRING {
			/* clean flags first */
			conf->options &= ~(OPT_SPLIT_HORIZON |
				    OPT_SPLIT_POISONED);
			if (!strcmp($2, "none"))
				/* nothing */ ;
			else if (!strcmp($2, "default"))
				conf->options |= OPT_SPLIT_HORIZON;
			else if (!strcmp($2, "poisoned"))
				conf->options |= OPT_SPLIT_POISONED;
			else {
				yyerror("unknon split horizon type");
				free($2);
				YYERROR;
			}
			free($2);
		}
		| TRIGGERED_UPDATES yesno {
			if ($2 == 1)
				conf->options |= OPT_TRIGGERED_UPDATES;
			else
				conf->options &= ~OPT_TRIGGERED_UPDATES;
		}
		| FIBUPDATE yesno {
			if ($2 == 0)
				conf->flags |= RIPD_FLAG_NO_FIB_UPDATE;
			else
				conf->flags &= ~RIPD_FLAG_NO_FIB_UPDATE;
		}
		| no REDISTRIBUTE STRING {
			struct redistribute	*r;

			if (!strcmp($3, "default")) {
				if (!$1)
					conf->redistribute |=
					    REDISTRIBUTE_DEFAULT;
				else
					conf->redistribute &=
					    ~REDISTRIBUTE_DEFAULT;
			} else {
				if ((r = calloc(1, sizeof(*r))) == NULL)
					fatal(NULL);
				if (!strcmp($3, "static"))
					r->type = REDIST_STATIC;
				else if (!strcmp($3, "connected"))
					r->type = REDIST_CONNECTED;
				else if (host($3, &r->addr, &r->mask))
					r->type = REDIST_ADDR;
				else {
					yyerror("unknown redistribute type");
					free($3);
					free(r);
					YYERROR;
				}

				if ($1)
					r->type |= REDIST_NO;

				SIMPLEQ_INSERT_TAIL(&conf->redist_list, r,
				    entry);
			}
			conf->redistribute |= REDISTRIBUTE_ON;
			free($3);
		}
		| no REDISTRIBUTE RTLABEL STRING {
			struct redistribute	*r;

			if ((r = calloc(1, sizeof(*r))) == NULL)
				fatal(NULL);
			r->type = REDIST_LABEL;
			r->label = rtlabel_name2id($4);
			if ($1)
				r->type |= REDIST_NO;
			free($4);

			SIMPLEQ_INSERT_TAIL(&conf->redist_list, r, entry);
			conf->redistribute |= REDISTRIBUTE_ON;
		}
		| defaults
		;

authmd		: AUTHMD NUMBER STRING {
			if ($2 < MIN_MD_ID || $2 > MAX_MD_ID) {
				yyerror("auth-md key-id out of range "
				    "(%d-%d)", MIN_MD_ID, MAX_MD_ID);
				free($3);
				YYERROR;
			}
			if (strlen($3) > MD5_DIGEST_LENGTH) {
				yyerror("auth-md key length out of range "
				    "(max length %d)",
				    MD5_DIGEST_LENGTH);
				free($3);
				YYERROR;
			}
			md_list_add(&defs->md_list, $2, $3);
			free($3);
		}

authmdkeyid	: AUTHMDKEYID NUMBER {
			if ($2 < MIN_MD_ID || $2 > MAX_MD_ID) {
				yyerror("auth-md-keyid out of range "
				    "(%d-%d)", MIN_MD_ID, MAX_MD_ID);
				YYERROR;
			}
			defs->auth_keyid = $2;
		}

authtype	: AUTHTYPE STRING {
			enum auth_type	type;

			if (!strcmp($2, "none"))
				type = AUTH_NONE;
			else if (!strcmp($2, "simple"))
				type = AUTH_SIMPLE;
			else if (!strcmp($2, "crypt"))
				type = AUTH_CRYPT;
			else {
				yyerror("unknown auth-type");
				free($2);
				YYERROR;
			}
			free($2);
			defs->auth_type = type;
		}
		;

authkey		: AUTHKEY STRING {
			if (strlen($2) > MAX_SIMPLE_AUTH_LEN) {
				yyerror("auth-key too long (max length %d)",
				    MAX_SIMPLE_AUTH_LEN);
				free($2);
				YYERROR;
			}
			strncpy(defs->auth_key, $2,
			    sizeof(defs->auth_key));
			free($2);
		}
		;

defaults	: COST NUMBER {
			if ($2 < 1 || $2 > INFINITY) {
				yyerror("cost out of range (%d-%d)", 1,
				    INFINITY);
				YYERROR;
			}
			defs->cost = $2;
		}
		| authtype
		| authkey
		| authmdkeyid
		| authmd
		;

optnl		: '\n' optnl
		|
		;

nl		: '\n' optnl
		;

interface       : INTERFACE STRING {
			struct kif *kif;

			if ((kif = kif_findname($2)) == NULL) {
				yyerror("unknown interface %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
			iface = conf_get_if(kif);
			if (iface == NULL)
				YYERROR;
			LIST_INSERT_HEAD(&conf->iface_list, iface, entry);
			memcpy(&ifacedefs, defs, sizeof(ifacedefs));
			md_list_copy(&ifacedefs.md_list, &defs->md_list);
			defs = &ifacedefs;
		} interface_block {
			iface->cost = defs->cost;
			iface->auth_type = defs->auth_type;
			iface->auth_keyid = defs->auth_keyid;
			memcpy(iface->auth_key, defs->auth_key,
			    sizeof(iface->auth_key));
			md_list_copy(&iface->auth_md_list, &defs->md_list);
			md_list_clr(&defs->md_list);
			defs = &globaldefs;
		}
		;

interface_block : '{' optnl interfaceopts_l '}'
		| '{' optnl '}'
		;

interfaceopts_l : interfaceopts_l interfaceoptsl
		| interfaceoptsl
		;

interfaceoptsl	: PASSIVE nl		{ iface->passive = 1; }
		| defaults nl
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

	errors = 1;
	va_start(ap, fmt);
	fprintf(stderr, "%s:%d: ", infile, yylval.lineno);
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
	    {"auth-key",		AUTHKEY},
	    {"auth-md",			AUTHMD},
	    {"auth-md-keyid",		AUTHMDKEYID},
	    {"auth-type",		AUTHTYPE},
	    {"cost",			COST},
	    {"fib-update",		FIBUPDATE},
	    {"interface",		INTERFACE},
	    {"no",			NO},
	    {"passive",			PASSIVE},
	    {"redistribute",		REDISTRIBUTE},
	    {"rtlabel",			RTLABEL},
	    {"split-horizon",		SPLIT_HORIZON},
	    {"triggered-updates",	TRIGGERED_UPDATES},
	    {"yes",			YES}
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
			errx(1, "yylex: strdup");
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
		} while ((c = lgetc(fin)) != EOF && isdigit(c));
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
		} while ((c = lgetc(fin)) != EOF && (allowed_in_string(c)));
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

struct ripd_conf *
parse_config(char *filename, int opts)
{
	struct sym	*sym, *next;

	if ((conf = calloc(1, sizeof(struct ripd_conf))) == NULL)
		fatal("parse_config");

	bzero(&globaldefs, sizeof(globaldefs));
	defs = &globaldefs;
	TAILQ_INIT(&defs->md_list);
	defs->cost = DEFAULT_COST;
	defs->auth_type = AUTH_NONE;

	if ((fin = fopen(filename, "r")) == NULL) {
		warn("%s", filename);
		free(conf);
		return (NULL);
	}
	infile = filename;

	conf->opts = opts;
	SIMPLEQ_INIT(&conf->redist_list);

	if (!(conf->opts & RIPD_OPT_NOACTION))
		if (check_file_secrecy(fileno(fin), filename)) {
			fclose(fin);
			free(conf);
			return (NULL);
		}

	yyparse();

	fclose(fin);

	/* Free macros and check which have not been used. */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entries);
		if ((conf->opts & RIPD_OPT_VERBOSE2) && !sym->used)
			fprintf(stderr, "warning: macro '%s' not "
			    "used\n", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entries);
			free(sym);
		}
	}

	/* free global config defaults */
	md_list_clr(&globaldefs.md_list);

	if (errors) {
		clear_config(conf);
		return (NULL);
	}

	return (conf);
}

int
symset(const char *nam, const char *val, int persist)
{
	struct sym	*sym;

	for (sym = TAILQ_FIRST(&symhead); sym && strcmp(nam, sym->nam);
	    sym = TAILQ_NEXT(sym, entries))
		;	/* nothing */

	if (sym != NULL) {
		if (sym->persist == 1)
			return (0);
		else {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entries);
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
	TAILQ_INSERT_TAIL(&symhead, sym, entries);
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

	TAILQ_FOREACH(sym, &symhead, entries)
		if (strcmp(nam, sym->nam) == 0) {
			sym->used = 1;
			return (sym->val);
		}
	return (NULL);
}

struct iface *
conf_get_if(struct kif *kif)
{
	struct iface    *i;

	LIST_FOREACH(i, &conf->iface_list, entry)
		if (i->ifindex == kif->ifindex) {
			yyerror("interface %s already configured",
			    kif->ifname);
			return (NULL);
		}

	i = if_new(kif);
	i->auth_keyid = 1;
	i->passive = 0;

	return (i);
}

void
clear_config(struct ripd_conf *xconf)
{
	struct iface	*i;

	while ((i = LIST_FIRST(&conf->iface_list)) != NULL) {
		LIST_REMOVE(i, entry);
		if_del(i);
	}

	free(xconf);
}

int
host(const char *s, struct in_addr *addr, struct in_addr *mask)
{
	struct in_addr		 ina;
	int			 bits = 32;

	bzero(&ina, sizeof(struct in_addr));
	if (strrchr(s, '/') != NULL) {
		if ((bits = inet_net_pton(AF_INET, s, &ina, sizeof(ina))) == -1)
			return (0);
	} else {
		if (inet_pton(AF_INET, s, &ina) != 1)
			return (0);
	}

	addr->s_addr = ina.s_addr;
	mask->s_addr = prefixlen2mask(bits);

	return (1);
}

/*	$OpenBSD: parse.y,v 1.23 2006/01/05 15:53:36 claudio Exp $ */

/*
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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ospf.h"
#include "ospfd.h"
#include "ospfe.h"
#include "log.h"

static struct ospfd_conf	*conf;
static FILE			*fin = NULL;
static int			 lineno = 1;
static int			 errors = 0;
static int			 pdebug = 1;
char				*infile;
char				*start_state;

struct area	*area = NULL;
struct iface	*iface = NULL;

int	 yyerror(const char *, ...);
int	 yyparse(void);
int	 kw_cmp(const void *, const void *);
int	 lookup(char *);
int	 lgetc(FILE *);
int	 lungetc(int);
int	 findeol(void);
int	 yylex(void);
void	 clear_config(struct ospfd_conf *xconf);
int	 check_file_secrecy(int fd, const char *fname);
u_int32_t	get_rtr_id(void);

static struct {
	u_int32_t	dead_interval;
	u_int16_t	transmit_delay;
	u_int16_t	hello_interval;
	u_int16_t	rxmt_interval;
	u_int16_t	metric;
	u_int8_t	priority;
} defaults;

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
int			 atoul(char *, u_long *);
struct area		*conf_get_area(struct in_addr);
struct iface		*conf_get_if(struct kif *);

typedef struct {
	union {
		u_int32_t	 number;
		char		*string;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	AREA INTERFACE ROUTERID FIBUPDATE REDISTRIBUTE
%token	RFC1583COMPAT SPFDELAY SPFHOLDTIME
%token	AUTHKEY AUTHTYPE AUTHMD AUTHMDKEYID
%token	METRIC PASSIVE
%token	HELLOINTERVAL TRANSMITDELAY
%token	RETRANSMITINTERVAL ROUTERDEADTIME ROUTERPRIORITY
%token	ERROR
%token	<v.string>	STRING
%type	<v.number>	number yesno
%type	<v.string>	string

%%

grammar		: /* empty */
		| grammar '\n'
		| grammar conf_main '\n'
		| grammar varset '\n'
		| grammar area '\n'
		| grammar error '\n'		{ errors++; }
		;

number		: STRING {
			u_long	ulval;

			if (atoul($1, &ulval) == -1) {
				yyerror("%s is not a number", $1);
				free($1);
				YYERROR;
			} else
				$$ = ulval;
			free($1);
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

yesno		: STRING {
			if (!strcmp($1, "yes"))
				$$ = 1;
			else if (!strcmp($1, "no"))
				$$ = 0;
			else {
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

varset		: STRING '=' string		{
			if (conf->opts & OSPFD_OPT_VERBOSE)
				printf("%s = \"%s\"\n", $1, $3);
			if (symset($1, $3, 0) == -1)
				fatal("cannot store variable");
			free($1);
			free($3);
		}
		;

conf_main	: METRIC number {
			if ($2 < MIN_METRIC || $2 > MAX_METRIC) {
				yyerror("metric out of range (%d-%d)",
				    MIN_METRIC, MAX_METRIC);
				YYERROR;
			}
			defaults.metric = $2;
		}
		| ROUTERPRIORITY number {
			if ($2 < MIN_PRIORITY || $2 > MAX_PRIORITY) {
				yyerror("router-priority out of range (%d-%d)",
				    MIN_PRIORITY, MAX_PRIORITY);
				YYERROR;
			}
			defaults.priority = $2;
		}
		| ROUTERDEADTIME number {
			if ($2 < MIN_RTR_DEAD_TIME || $2 > MAX_RTR_DEAD_TIME) {
				yyerror("router-dead-time out of range (%d-%d)",
				    MIN_RTR_DEAD_TIME, MAX_RTR_DEAD_TIME);
				YYERROR;
			}
			defaults.dead_interval = $2;
		}
		| TRANSMITDELAY number {
			if ($2 < MIN_TRANSMIT_DELAY ||
			    $2 > MAX_TRANSMIT_DELAY) {
				yyerror("transmit-delay out of range (%d-%d)",
				    MIN_TRANSMIT_DELAY, MAX_TRANSMIT_DELAY);
				YYERROR;
			}
			defaults.transmit_delay = $2;
		}
		| HELLOINTERVAL number {
			if ($2 < MIN_HELLO_INTERVAL ||
			    $2 > MAX_HELLO_INTERVAL) {
				yyerror("hello-interval out of range (%d-%d)",
				    MIN_HELLO_INTERVAL, MAX_HELLO_INTERVAL);
				YYERROR;
			}
			defaults.hello_interval = $2;
		}
		| RETRANSMITINTERVAL number {
			if ($2 < MIN_RXMT_INTERVAL || $2 > MAX_RXMT_INTERVAL) {
				yyerror("retransmit-interval out of range "
				    "(%d-%d)", MIN_RXMT_INTERVAL,
				    MAX_RXMT_INTERVAL);
				YYERROR;
			}
			defaults.rxmt_interval = $2;
		}
		| ROUTERID STRING {
			if (!inet_aton($2, &conf->rtr_id)) {
				yyerror("error parsing router-id");
				free($2);
				YYERROR;
			}
			free($2);
		}
		| FIBUPDATE yesno {
			if ($2 == 0)
				conf->flags |= OSPFD_FLAG_NO_FIB_UPDATE;
			else
				conf->flags &= ~OSPFD_FLAG_NO_FIB_UPDATE;
		}
		| REDISTRIBUTE STRING {
			if (!strcmp($2, "static"))
				conf->redistribute_flags |=
				    REDISTRIBUTE_STATIC;
			else if (!strcmp($2, "connected"))
				conf->redistribute_flags |=
				    REDISTRIBUTE_CONNECTED;
			else if (!strcmp($2, "default"))
				conf->redistribute_flags |=
				    REDISTRIBUTE_DEFAULT;
			else if (!strcmp($2, "none"))
				conf->redistribute_flags = 0;
			else {
				yyerror("unknown redistribute type");
				free($2);
				YYERROR;
			}
			free($2);
		}
		| RFC1583COMPAT yesno {
			conf->rfc1583compat = $2;
		}
		| SPFDELAY number {
			if ($2 < MIN_SPF_DELAY || $2 > MAX_SPF_DELAY) {
				yyerror("spf-delay out of range "
				    "(%d-%d)", MIN_SPF_DELAY,
				    MAX_SPF_DELAY);
				YYERROR;
			}
			conf->spf_delay = $2;
		}
		| SPFHOLDTIME number {
			if ($2 < MIN_SPF_HOLDTIME || $2 > MAX_SPF_HOLDTIME) {
				yyerror("spf-holdtime out of range "
				    "(%d-%d)", MIN_SPF_HOLDTIME,
				    MAX_SPF_HOLDTIME);
				YYERROR;
			}
			conf->spf_hold_time = $2;
		}
		;

authmd		: AUTHMD number STRING {
			if (iface != NULL) {
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
				md_list_add(iface, $2, $3);
			}
			free($3);
		}

authmdkeyid	: AUTHMDKEYID number {
			if (iface != NULL) {
				if ($2 < MIN_MD_ID || $2 > MAX_MD_ID) {
					yyerror("auth-md-keyid out of range "
					    "(%d-%d)", MIN_MD_ID, MAX_MD_ID);
					YYERROR;
				}
				iface->auth_keyid = $2;
			}
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
			iface->auth_type = type;
		}
		;

authkey		: AUTHKEY STRING {
			if (iface != NULL) {
				if (strlen($2) > MAX_SIMPLE_AUTH_LEN) {
					yyerror("auth-key length out of range "
					    "(max length %d)", MAX_SIMPLE_AUTH_LEN);
					free($2);
					YYERROR;
				}
				iface->auth_key = $2;
			} else
				free($2);
		}
		;

optnl		: '\n' optnl
		|
		;

nl		: '\n' optnl		/* one newline or more */
		;

area		: AREA STRING {
			struct in_addr	id;
			if (inet_aton($2, &id) == 0) {
				yyerror("error parsing area");
				free($2);
				YYERROR;
			}
			free($2);
			area = conf_get_area(id);
		} '{' optnl areaopts_l '}' {
			area = NULL;
		}
		;

areaopts_l	: areaopts_l areaoptsl
		| areaoptsl
		;

areaoptsl	: interface nl
		| METRIC number nl {
			if ($2 < MIN_METRIC || $2 > MAX_METRIC) {
				yyerror("metric out of range (%d-%d)",
				    MIN_METRIC, MAX_METRIC);
				YYERROR;
			}
			area->metric = $2;
		}
		| ROUTERPRIORITY number nl {
			if ($2 < MIN_PRIORITY || $2 > MAX_PRIORITY) {
				yyerror("router-priority out of range (%d-%d)",
				    MIN_PRIORITY, MAX_PRIORITY);
				YYERROR;
			}
			area->priority = $2;
		}
		| ROUTERDEADTIME number nl {
			if ($2 < MIN_RTR_DEAD_TIME || $2 > MAX_RTR_DEAD_TIME) {
				yyerror("router-dead-time out of range (%d-%d)",
				    MIN_RTR_DEAD_TIME, MAX_RTR_DEAD_TIME);
				YYERROR;
			}
			area->dead_interval = $2;
		}
		| TRANSMITDELAY number nl {
			if ($2 < MIN_TRANSMIT_DELAY ||
			    $2 > MAX_TRANSMIT_DELAY) {
				yyerror("transmit-delay out of range (%d-%d)",
				    MIN_TRANSMIT_DELAY, MAX_TRANSMIT_DELAY);
				YYERROR;
			}
			area->transmit_delay = $2;
		}
		| HELLOINTERVAL number nl {
			if ($2 < MIN_HELLO_INTERVAL ||
			    $2 > MAX_HELLO_INTERVAL) {
				yyerror("hello-interval out of range (%d-%d)",
				    MIN_HELLO_INTERVAL, MAX_HELLO_INTERVAL);
				YYERROR;
			}
			area->hello_interval = $2;
		}
		| RETRANSMITINTERVAL number nl {
			if ($2 < MIN_RXMT_INTERVAL || $2 > MAX_RXMT_INTERVAL) {
				yyerror("retransmit-interval out of range "
				    "(%d-%d)", MIN_RXMT_INTERVAL,
				    MAX_RXMT_INTERVAL);
				YYERROR;
			}
			area->rxmt_interval = $2;
		}
		;

interface	: INTERFACE STRING	{
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
			iface->area = area;
			LIST_INSERT_HEAD(&area->iface_list,
			    iface, entry);
		} interface_block {
			iface = NULL;
		}
		;

interface_block	: '{' optnl interfaceopts_l '}'
		| '{' optnl '}'
		|
		;

interfaceopts_l	: interfaceopts_l interfaceoptsl
		| interfaceoptsl
		;

interfaceoptsl	: authmd nl
		| authkey nl
		| authmdkeyid nl
		| authtype nl
		| PASSIVE nl		{ iface->passive = 1; }
		| METRIC number nl {
			if ($2 < MIN_METRIC || $2 > MAX_METRIC) {
				yyerror("metric out of range (%d-%d)",
				    MIN_METRIC, MAX_METRIC);
				YYERROR;
			}
			iface->metric = $2;
		}
		| ROUTERPRIORITY number nl {
			if ($2 < MIN_PRIORITY || $2 > MAX_PRIORITY) {
				yyerror("router-priority out of range (%d-%d)",
				    MIN_PRIORITY, MAX_PRIORITY);
				YYERROR;
			}
			iface->priority = $2;
		}
		| ROUTERDEADTIME number nl {
			if ($2 < MIN_RTR_DEAD_TIME || $2 > MAX_RTR_DEAD_TIME) {
				yyerror("router-dead-time out of range (%d-%d)",
				    MIN_RTR_DEAD_TIME, MAX_RTR_DEAD_TIME);
				YYERROR;
			}
			iface->dead_interval = $2;
		}
		| TRANSMITDELAY number nl {
			if ($2 < MIN_TRANSMIT_DELAY ||
			    $2 > MAX_TRANSMIT_DELAY) {
				yyerror("transmit-delay out of range (%d-%d)",
				    MIN_TRANSMIT_DELAY, MAX_TRANSMIT_DELAY);
				YYERROR;
			}
			iface->transmit_delay = $2;
		}
		| HELLOINTERVAL number nl {
			if ($2 < MIN_HELLO_INTERVAL ||
			    $2 > MAX_HELLO_INTERVAL) {
				yyerror("hello-interval out of range (%d-%d)",
				    MIN_HELLO_INTERVAL, MAX_HELLO_INTERVAL);
				YYERROR;
			}
			iface->hello_interval = $2;
		}
		| RETRANSMITINTERVAL number nl {
			if ($2 < MIN_RXMT_INTERVAL || $2 > MAX_RXMT_INTERVAL) {
				yyerror("retransmit-interval out of range "
				    "(%d-%d)", MIN_RXMT_INTERVAL,
				    MAX_RXMT_INTERVAL);
				YYERROR;
			}
			iface->rxmt_interval = $2;
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
		{"area",		AREA},
		{"auth-key",		AUTHKEY},
		{"auth-md",		AUTHMD},
		{"auth-md-keyid",	AUTHMDKEYID},
		{"auth-type",		AUTHTYPE},
		{"fib-update",		FIBUPDATE},
		{"hello-interval",	HELLOINTERVAL},
		{"interface",		INTERFACE},
		{"metric",		METRIC},
		{"passive",		PASSIVE},
		{"redistribute",	REDISTRIBUTE},
		{"retransmit-interval",	RETRANSMITINTERVAL},
		{"rfc1583compat",	RFC1583COMPAT},
		{"router-dead-time",	ROUTERDEADTIME},
		{"router-id",		ROUTERID},
		{"router-priority",	ROUTERPRIORITY},
		{"spf-delay",		SPFDELAY},
		{"spf-holdtime",	SPFHOLDTIME},
		{"transmit-delay",	TRANSMITDELAY}
	};
	const struct keywords	*p;

	p = bsearch(s, keywords, sizeof(keywords)/sizeof(keywords[0]),
	    sizeof(keywords[0]), kw_cmp);

	if (p) {
		if (pdebug > 1)
			fprintf(stderr, "%s: %d\n", s, p->k_val);
		return (p->k_val);
	} else {
		if (pdebug > 1)
			fprintf(stderr, "string: %s\n", s);
		return (STRING);
	}
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

struct ospfd_conf *
parse_config(char *filename, int opts)
{
	struct sym	*sym, *next;

	if ((conf = calloc(1, sizeof(struct ospfd_conf))) == NULL) {
		errx(1, "parse_config calloc");
		return (NULL);
	}

	defaults.dead_interval = DEFAULT_RTR_DEAD_TIME;
	defaults.transmit_delay = DEFAULT_TRANSMIT_DELAY;
	defaults.hello_interval = DEFAULT_HELLO_INTERVAL;
	defaults.rxmt_interval = DEFAULT_RXMT_INTERVAL;
	defaults.metric = DEFAULT_METRIC;
	defaults.priority = DEFAULT_PRIORITY;

	conf->options = OSPF_OPTION_E;
	conf->spf_delay = DEFAULT_SPF_DELAY;
	conf->spf_hold_time = DEFAULT_SPF_HOLDTIME;
	conf->spf_state = SPF_IDLE;

	if ((fin = fopen(filename, "r")) == NULL) {
		warn("%s", filename);
		free(conf);
		return (NULL);
	}
	infile = filename;

	conf->opts = opts;
	LIST_INIT(&conf->area_list);

	if (!(conf->opts & OSPFD_OPT_NOACTION))
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
		if ((conf->opts & OSPFD_OPT_VERBOSE2) && !sym->used)
			fprintf(stderr, "warning: macro '%s' not "
			    "used\n", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entries);
			free(sym);
		}
	}

	if (errors) {
		clear_config(conf);
		return (NULL);
	}

	if (conf->rtr_id.s_addr == 0)
		conf->rtr_id.s_addr = get_rtr_id();
	
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

int
atoul(char *s, u_long *ulvalp)
{
	u_long	 ulval;
	char	*ep;

	errno = 0;
	ulval = strtoul(s, &ep, 0);
	if (s[0] == '\0' || *ep != '\0')
		return (-1);
	if (errno == ERANGE && ulval == ULONG_MAX)
		return (-1);
	*ulvalp = ulval;
	return (0);
}

struct area *
conf_get_area(struct in_addr id)
{
	struct area	*a;

	a = area_find(conf, id);
	if (a)
		return (a);
	a = area_new();
	LIST_INSERT_HEAD(&conf->area_list, a, entry);

	a->dead_interval = defaults.dead_interval;
	a->transmit_delay = defaults.transmit_delay;
	a->hello_interval = defaults.hello_interval;
	a->rxmt_interval = defaults.rxmt_interval;
	a->metric = defaults.metric;
	a->priority = defaults.priority;

	a->id.s_addr = id.s_addr;

	return (a);
}

struct iface *
conf_get_if(struct kif *kif)
{
	struct area	*a;
	struct iface	*i;

	LIST_FOREACH(a, &conf->area_list, entry)
		LIST_FOREACH(i, &a->iface_list, entry)
			if (i->ifindex == kif->ifindex) {
				yyerror("interface %s already configured",
				    kif->ifname);
				return (NULL);
			}

	i = if_new(kif);
	i->dead_interval = area->dead_interval;
	i->transmit_delay = area->transmit_delay;
	i->hello_interval = area->hello_interval;
	i->rxmt_interval = area->rxmt_interval;
	i->metric = area->metric;
	i->priority = area->priority;
	i->auth_keyid = 1;

	return (i);
}

void
clear_config(struct ospfd_conf *xconf)
{
	struct area	*a;

	while ((a = LIST_FIRST(&xconf->area_list)) != NULL) {
		LIST_REMOVE(a, entry);
		area_del(a);
	}

	free(xconf);
}

u_int32_t
get_rtr_id(void)
{
	struct ifaddrs		*ifap, *ifa;
	u_int32_t		 ip = 0, cur, localnet;

	localnet = htonl(INADDR_LOOPBACK & IN_CLASSA_NET);

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;
		cur = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
		if ((cur & localnet) == localnet)	/* skip 127/8 */
			continue;
		if (cur > ip || ip == 0)
			ip = cur;
	}
	freeifaddrs(ifap);
	
	if (ip == 0)
		fatal("router-id is 0.0.0.0");

	return (ip);
}

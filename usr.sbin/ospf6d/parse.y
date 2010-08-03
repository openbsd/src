/*	$OpenBSD: parse.y,v 1.18 2010/08/03 18:42:41 henning Exp $ */

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
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <limits.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ospf6.h"
#include "ospf6d.h"
#include "ospfe.h"
#include "log.h"

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
int		 check_file_secrecy(int, const char *);
int		 yyparse(void);
int		 yylex(void);
int		 yyerror(const char *, ...);
int		 kw_cmp(const void *, const void *);
int		 lookup(char *);
int		 lgetc(int);
int		 lungetc(int);
int		 findeol(void);

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

void		 clear_config(struct ospfd_conf *xconf);
u_int32_t	 get_rtr_id(void);
int	 host(const char *, struct in6_addr *);
int	 prefix(const char *, struct in6_addr *, u_int8_t *);

static struct ospfd_conf	*conf;
static int			 errors = 0;

struct area	*area = NULL;
struct iface	*iface = NULL;

struct config_defaults {
	u_int16_t	dead_interval;
	u_int16_t	transmit_delay;
	u_int16_t	hello_interval;
	u_int16_t	rxmt_interval;
	u_int16_t	metric;
	u_int8_t	priority;
};

struct config_defaults	 globaldefs;
struct config_defaults	 areadefs;
struct config_defaults	 ifacedefs;
struct config_defaults	*defs;

struct area	*conf_get_area(struct in_addr);

typedef struct {
	union {
		int64_t		 number;
		char		*string;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	AREA INTERFACE ROUTERID FIBUPDATE REDISTRIBUTE RTLABEL
%token	STUB ROUTER SPFDELAY SPFHOLDTIME EXTTAG
%token	METRIC PASSIVE
%token	HELLOINTERVAL TRANSMITDELAY
%token	RETRANSMITINTERVAL ROUTERDEADTIME ROUTERPRIORITY
%token	SET TYPE
%token	YES NO
%token	DEMOTE
%token	ERROR
%token	<v.string>	STRING
%token	<v.number>	NUMBER
%type	<v.number>	yesno no optlist, optlist_l option demotecount
%type	<v.string>	string

%%

grammar		: /* empty */
		| grammar '\n'
		| grammar conf_main '\n'
		| grammar varset '\n'
		| grammar area '\n'
		| grammar error '\n'		{ file->errors++; }
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

yesno		: YES	{ $$ = 1; }
		| NO	{ $$ = 0; }
		;

no		: /* empty */	{ $$ = 0; }
		| NO		{ $$ = 1; }

varset		: STRING '=' string		{
			if (conf->opts & OSPFD_OPT_VERBOSE)
				printf("%s = \"%s\"\n", $1, $3);
			if (symset($1, $3, 0) == -1)
				fatal("cannot store variable");
			free($1);
			free($3);
		}
		;

conf_main	: ROUTERID STRING {
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
		| no REDISTRIBUTE STRING optlist {
			struct redistribute	*r;

			if (!strcmp($3, "default")) {
				if (!$1)
					conf->redistribute |=
					    REDISTRIBUTE_DEFAULT;
				else
					conf->redistribute &=
					    ~REDISTRIBUTE_DEFAULT;
				conf->defaultmetric = $4;
			} else {
				if ((r = calloc(1, sizeof(*r))) == NULL)
					fatal(NULL);
				if (!strcmp($3, "static"))
					r->type = REDIST_STATIC;
				else if (!strcmp($3, "connected"))
					r->type = REDIST_CONNECTED;
				else if (prefix($3, &r->addr, &r->prefixlen))
					r->type = REDIST_ADDR;
				else {
					yyerror("unknown redistribute type");
					free($3);
					free(r);
					YYERROR;
				}

				if ($1)
					r->type |= REDIST_NO;
				r->metric = $4;

				SIMPLEQ_INSERT_TAIL(&conf->redist_list, r,
				    entry);
			}
			conf->redistribute |= REDISTRIBUTE_ON;
			free($3);
		}
		| no REDISTRIBUTE RTLABEL STRING optlist {
			struct redistribute	*r;

			if ((r = calloc(1, sizeof(*r))) == NULL)
				fatal(NULL);
			r->type = REDIST_LABEL;
			r->label = rtlabel_name2id($4);
			if ($1)
				r->type |= REDIST_NO;
			r->metric = $5;
			free($4);

			SIMPLEQ_INSERT_TAIL(&conf->redist_list, r, entry);
			conf->redistribute |= REDISTRIBUTE_ON;
		}
		| RTLABEL STRING EXTTAG NUMBER {
			if ($4 < 0 || $4 > UINT_MAX) {
				yyerror("invalid external route tag");
				free($2);
				YYERROR;
			}
			rtlabel_tag(rtlabel_name2id($2), $4);
			free($2);
		}
		| SPFDELAY NUMBER {
			if ($2 < MIN_SPF_DELAY || $2 > MAX_SPF_DELAY) {
				yyerror("spf-delay out of range "
				    "(%d-%d)", MIN_SPF_DELAY,
				    MAX_SPF_DELAY);
				YYERROR;
			}
			conf->spf_delay = $2;
		}
		| SPFHOLDTIME NUMBER {
			if ($2 < MIN_SPF_HOLDTIME || $2 > MAX_SPF_HOLDTIME) {
				yyerror("spf-holdtime out of range "
				    "(%d-%d)", MIN_SPF_HOLDTIME,
				    MAX_SPF_HOLDTIME);
				YYERROR;
			}
			conf->spf_hold_time = $2;
		}
		| STUB ROUTER yesno {
			if ($3)
				conf->flags |= OSPFD_FLAG_STUB_ROUTER;
			else
				/* allow to force non stub mode */
				conf->flags &= ~OSPFD_FLAG_STUB_ROUTER;
		}
		| defaults
		;

optlist		: /* empty */			{ $$ = DEFAULT_REDIST_METRIC; }
		| SET option			{
			$$ = $2;
			if (($$ & LSA_METRIC_MASK) == 0)
				$$ |= DEFAULT_REDIST_METRIC;
		}
		| SET optnl '{' optnl optlist_l optnl '}'	{
			$$ = $5;
			if (($$ & LSA_METRIC_MASK) == 0)
				$$ |= DEFAULT_REDIST_METRIC;
		}
		;

optlist_l	: optlist_l comma option {
			if ($1 & LSA_ASEXT_E_FLAG && $3 & LSA_ASEXT_E_FLAG) {
				yyerror("redistribute type already defined");
				YYERROR;
			}
			if ($1 & LSA_METRIC_MASK && $3 & LSA_METRIC_MASK) {
				yyerror("redistribute metric already defined");
				YYERROR;
			}
			$$ = $1 | $3;
		}
		| option { $$ = $1; }
		;

option		: METRIC NUMBER {
			if ($2 == 0 || $2 > MAX_METRIC) {
				yyerror("invalid redistribute metric");
				YYERROR;
			}
			$$ = $2;
		}
		| TYPE NUMBER {
			switch ($2) {
			case 1:
				$$ = 0;
				break;
			case 2:
				$$ = LSA_ASEXT_E_FLAG;
				break;
			default:
				yyerror("only external type 1 and 2 allowed");
				YYERROR;
			}
		}
		;

defaults	: METRIC NUMBER {
			if ($2 < MIN_METRIC || $2 > MAX_METRIC) {
				yyerror("metric out of range (%d-%d)",
				    MIN_METRIC, MAX_METRIC);
				YYERROR;
			}
			defs->metric = $2;
		}
		| ROUTERPRIORITY NUMBER {
			if ($2 < MIN_PRIORITY || $2 > MAX_PRIORITY) {
				yyerror("router-priority out of range (%d-%d)",
				    MIN_PRIORITY, MAX_PRIORITY);
				YYERROR;
			}
			defs->priority = $2;
		}
		| ROUTERDEADTIME NUMBER {
			if ($2 < MIN_RTR_DEAD_TIME || $2 > MAX_RTR_DEAD_TIME) {
				yyerror("router-dead-time out of range (%d-%d)",
				    MIN_RTR_DEAD_TIME, MAX_RTR_DEAD_TIME);
				YYERROR;
			}
			defs->dead_interval = $2;
		}
		| TRANSMITDELAY NUMBER {
			if ($2 < MIN_TRANSMIT_DELAY ||
			    $2 > MAX_TRANSMIT_DELAY) {
				yyerror("transmit-delay out of range (%d-%d)",
				    MIN_TRANSMIT_DELAY, MAX_TRANSMIT_DELAY);
				YYERROR;
			}
			defs->transmit_delay = $2;
		}
		| HELLOINTERVAL NUMBER {
			if ($2 < MIN_HELLO_INTERVAL ||
			    $2 > MAX_HELLO_INTERVAL) {
				yyerror("hello-interval out of range (%d-%d)",
				    MIN_HELLO_INTERVAL, MAX_HELLO_INTERVAL);
				YYERROR;
			}
			defs->hello_interval = $2;
		}
		| RETRANSMITINTERVAL NUMBER {
			if ($2 < MIN_RXMT_INTERVAL || $2 > MAX_RXMT_INTERVAL) {
				yyerror("retransmit-interval out of range "
				    "(%d-%d)", MIN_RXMT_INTERVAL,
				    MAX_RXMT_INTERVAL);
				YYERROR;
			}
			defs->rxmt_interval = $2;
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

area		: AREA STRING {
			struct in_addr	id;
			if (inet_aton($2, &id) == 0) {
				yyerror("error parsing area");
				free($2);
				YYERROR;
			}
			free($2);
			area = conf_get_area(id);

			memcpy(&areadefs, defs, sizeof(areadefs));
			defs = &areadefs;
		} '{' optnl areaopts_l '}' {
			area = NULL;
			defs = &globaldefs;
		}
		;

demotecount	: NUMBER	{ $$ = $1; }
		| /*empty*/	{ $$ = 1; }
		;

areaopts_l	: areaopts_l areaoptsl nl
		| areaoptsl optnl
		;

areaoptsl	: interface
		| DEMOTE STRING	demotecount {
			if ($3 < 1 || $3 > 255) {
				yyerror("demote count out of range (1-255)");
				free($2);
				YYERROR;
			}
			area->demote_level = $3;
			if (strlcpy(area->demote_group, $2,
			    sizeof(area->demote_group)) >=
			    sizeof(area->demote_group)) {
				yyerror("demote group name \"%s\" too long");
				free($2);
				YYERROR;
			}
			free($2);
			if (carp_demote_init(area->demote_group,
			    conf->opts & OSPFD_OPT_FORCE_DEMOTE) == -1) {
				yyerror("error initializing group \"%s\"",
				    area->demote_group);
				YYERROR;
			}
		}
		| defaults
		;

interface	: INTERFACE STRING	{
			if ((iface = if_findname($2)) == NULL) {
				yyerror("unknown interface %s", $2);
				free($2);
				YYERROR;
			}
			if (IN6_IS_ADDR_UNSPECIFIED(&iface->addr)) {
				yyerror("unnumbered interface %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
			iface->area_id.s_addr = area->id.s_addr;
			LIST_INSERT_HEAD(&area->iface_list, iface, entry);

			memcpy(&ifacedefs, defs, sizeof(ifacedefs));
			defs = &ifacedefs;
		} interface_block {
			iface->dead_interval = defs->dead_interval;
			iface->transmit_delay = defs->transmit_delay;
			iface->hello_interval = defs->hello_interval;
			iface->rxmt_interval = defs->rxmt_interval;
			iface->metric = defs->metric;
			iface->priority = defs->priority;
			iface->cflags |= F_IFACE_CONFIGURED;
			iface = NULL;
			/* interface is always part of an area */
			defs = &areadefs;
		}
		;

interface_block	: '{' optnl interfaceopts_l '}'
		| '{' optnl '}'
		|
		;

interfaceopts_l	: interfaceopts_l interfaceoptsl nl
		| interfaceoptsl optnl
		;

interfaceoptsl	: PASSIVE		{ iface->cflags |= F_IFACE_PASSIVE; }
		| DEMOTE STRING		{
			if (strlcpy(iface->demote_group, $2,
			    sizeof(iface->demote_group)) >=
			    sizeof(iface->demote_group)) {
				yyerror("demote group name \"%s\" too long");
				free($2);
				YYERROR;
			}
			free($2);
			if (carp_demote_init(iface->demote_group,
			    conf->opts & OSPFD_OPT_FORCE_DEMOTE) == -1) {
				yyerror("error initializing group \"%s\"",
				    iface->demote_group);
				YYERROR;
			}
		}
		| defaults
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
		{"area",		AREA},
		{"demote",		DEMOTE},
		{"external-tag",	EXTTAG},
		{"fib-update",		FIBUPDATE},
		{"hello-interval",	HELLOINTERVAL},
		{"interface",		INTERFACE},
		{"metric",		METRIC},
		{"no",			NO},
		{"passive",		PASSIVE},
		{"redistribute",	REDISTRIBUTE},
		{"retransmit-interval",	RETRANSMITINTERVAL},
		{"router",		ROUTER},
		{"router-dead-time",	ROUTERDEADTIME},
		{"router-id",		ROUTERID},
		{"router-priority",	ROUTERPRIORITY},
		{"rtlabel",		RTLABEL},
		{"set",			SET},
		{"spf-delay",		SPFDELAY},
		{"spf-holdtime",	SPFHOLDTIME},
		{"stub",		STUB},
		{"transmit-delay",	TRANSMITDELAY},
		{"type",		TYPE},
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

int
check_file_secrecy(int fd, const char *fname)
{
	struct stat	st;

	if (fstat(fd, &st)) {
		log_warn("cannot stat %s", fname);
		return (-1);
	}
	if (st.st_uid != 0 && st.st_uid != getuid()) {
		log_warnx("%s: owner not root or current user", fname);
		return (-1);
	}
	if (st.st_mode & (S_IRWXG | S_IRWXO)) {
		log_warnx("%s: group/world readable/writeable", fname);
		return (-1);
	}
	return (0);
}

struct file *
pushfile(const char *name, int secret)
{
	struct file	*nfile;

	if ((nfile = calloc(1, sizeof(struct file))) == NULL) {
		log_warn("malloc");
		return (NULL);
	}
	if ((nfile->name = strdup(name)) == NULL) {
		log_warn("malloc");
		free(nfile);
		return (NULL);
	}
	if ((nfile->stream = fopen(nfile->name, "r")) == NULL) {
		log_warn("%s", nfile->name);
		free(nfile->name);
		free(nfile);
		return (NULL);
	} else if (secret &&
	    check_file_secrecy(fileno(nfile->stream), nfile->name)) {
		fclose(nfile->stream);
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

struct ospfd_conf *
parse_config(char *filename, int opts)
{
	struct sym	*sym, *next;

	if ((conf = calloc(1, sizeof(struct ospfd_conf))) == NULL)
		fatal("parse_config");
	conf->opts = opts;
	if (conf->opts & OSPFD_OPT_STUB_ROUTER)
		conf->flags |= OSPFD_FLAG_STUB_ROUTER;

	bzero(&globaldefs, sizeof(globaldefs));
	defs = &globaldefs;
	defs->dead_interval = DEFAULT_RTR_DEAD_TIME;
	defs->transmit_delay = DEFAULT_TRANSMIT_DELAY;
	defs->hello_interval = DEFAULT_HELLO_INTERVAL;
	defs->rxmt_interval = DEFAULT_RXMT_INTERVAL;
	defs->metric = DEFAULT_METRIC;
	defs->priority = DEFAULT_PRIORITY;

	conf->spf_delay = DEFAULT_SPF_DELAY;
	conf->spf_hold_time = DEFAULT_SPF_HOLDTIME;
	conf->spf_state = SPF_IDLE;

	if ((file = pushfile(filename, !(conf->opts & OSPFD_OPT_NOACTION))) == NULL) {
		free(conf);
		return (NULL);
	}
	topfile = file;

	LIST_INIT(&conf->area_list);
	LIST_INIT(&conf->cand_list);
	SIMPLEQ_INIT(&conf->redist_list);

	yyparse();
	errors = file->errors;
	popfile();

	/* Free macros and check which have not been used. */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entry);
		if ((conf->opts & OSPFD_OPT_VERBOSE2) && !sym->used)
			fprintf(stderr, "warning: macro '%s' not "
			    "used\n", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
			free(sym);
		}
	}

	/* free global config defaults */
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

struct area *
conf_get_area(struct in_addr id)
{
	struct area	*a;

	a = area_find(conf, id);
	if (a)
		return (a);
	a = area_new();
	LIST_INSERT_HEAD(&conf->area_list, a, entry);

	a->id.s_addr = id.s_addr;

	return (a);
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
		if (ntohl(cur) < ntohl(ip) || ip == 0)
			ip = cur;
	}
	freeifaddrs(ifap);

	if (ip == 0)
		fatal("router-id is 0.0.0.0");

	return (ip);
}

int
host(const char *s, struct in6_addr *addr)
{
	struct addrinfo	hints, *r;

	if (s == NULL)
		return (0);

	bzero(addr, sizeof(struct in6_addr));
	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM; /*dummy*/
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(s, "0", &hints, &r) == 0) {
		*addr = ((struct sockaddr_in6 *)r->ai_addr)->sin6_addr;
		/* XXX address scope !!! */
		/* ((struct sockaddr_in6 *)r->ai_addr)->sin6_scope_id */
		freeaddrinfo(r);
		return (1);
	}
	return (0);
}

int
prefix(const char *s, struct in6_addr *addr, u_int8_t *plen)
{
	char		*p, *ps;
	const char	*errstr;
	int		 mask;

	if (s == NULL)
		return (0);

	if ((p = strrchr(s, '/')) != NULL) {
		mask = strtonum(p + 1, 0, 128, &errstr);
		if (errstr)
			errx(1, "invalid netmask: %s", errstr);

		if ((ps = malloc(strlen(s) - strlen(p) + 1)) == NULL)
			err(1, "parse_prefix: malloc");
		strlcpy(ps, s, strlen(s) - strlen(p) + 1);

		if (host(ps, addr) == 0) {
			free(ps);
			return (0);
		}

		inet6applymask(addr, addr, mask);
		*plen = mask;
		return (1);
	}
	*plen = 128;
	return (host(s, addr));
}

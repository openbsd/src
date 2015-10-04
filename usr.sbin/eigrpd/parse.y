/*	$OpenBSD: parse.y,v 1.2 2015/10/04 22:54:38 renato Exp $ */

/*
 * Copyright (c) 2015 Renato Westphal <renato@openbsd.org>
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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "eigrp.h"
#include "eigrpd.h"
#include "eigrpe.h"
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
int		 yyerror(const char *, ...)
    __attribute__((__format__ (printf, 1, 2)))
    __attribute__((__nonnull__ (1)));
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

void		 clear_config(struct eigrpd_conf *xconf);
uint32_t	 get_rtr_id(void);
int		 host(const char *, union eigrpd_addr *, uint8_t *);

static struct eigrpd_conf	*conf;
static int			 errors = 0;

int			 af = AF_UNSPEC;
struct eigrp		*eigrp = NULL;
struct eigrp_iface	*ei = NULL;

struct config_defaults {
	uint8_t		kvalues[6];
	uint16_t	active_timeout;
	uint8_t		maximum_hops;
	uint8_t		maximum_paths;
	uint8_t		variance;
	struct redist_metric *dflt_metric;
	uint16_t	hello_interval;
	uint16_t	hello_holdtime;
	uint32_t	delay;
	uint32_t	bandwidth;
	uint8_t		splithorizon;
};

struct config_defaults	 globaldefs;
struct config_defaults	 afdefs;
struct config_defaults	 asdefs;
struct config_defaults	 ifacedefs;
struct config_defaults	*defs;

struct eigrp		*conf_get_instance(uint16_t);
struct eigrp_iface	*conf_get_if(struct kif *);

typedef struct {
	union {
		int64_t			 number;
		char			*string;
		struct redistribute	*redist;
		struct redist_metric	*redist_metric;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	ROUTERID AS FIBUPDATE RDOMAIN REDISTRIBUTE METRIC DFLTMETRIC
%token	MAXHOPS MAXPATHS VARIANCE FIBPRIORITY_INT FIBPRIORITY_EXT
%token	AF IPV4 IPV6 HELLOINTERVAL HOLDTIME KVALUES ACTIVETIMEOUT
%token	INTERFACE PASSIVE DELAY BANDWIDTH SPLITHORIZON
%token	YES NO
%token	INCLUDE
%token	ERROR
%token	<v.string>	STRING
%token	<v.number>	NUMBER
%type	<v.number>	yesno no
%type	<v.string>	string
%type	<v.number>	eigrp_af
%type	<v.redist>	redistribute
%type	<v.redist_metric> redist_metric opt_red_metric

%%

grammar		: /* empty */
		| grammar include '\n'
		| grammar '\n'
		| grammar conf_main '\n'
		| grammar varset '\n'
		| grammar af '\n'
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

yesno		: YES	{ $$ = 1; }
		| NO	{ $$ = 0; }
		;

no		: /* empty */	{ $$ = 0; }
		| NO		{ $$ = 1; }
		;

eigrp_af	: IPV4	{ $$ = AF_INET; }
		| IPV6	{ $$ = AF_INET6; }
		;

varset		: STRING '=' string		{
			if (conf->opts & EIGRPD_OPT_VERBOSE)
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
				conf->flags |= EIGRPD_FLAG_NO_FIB_UPDATE;
			else
				conf->flags &= ~EIGRPD_FLAG_NO_FIB_UPDATE;
		}
		| RDOMAIN NUMBER {
			if ($2 < 0 || $2 > RT_TABLEID_MAX) {
				yyerror("invalid rdomain");
				YYERROR;
			}
			conf->rdomain = $2;
		}
		| FIBPRIORITY_INT NUMBER {
			if ($2 <= RTP_NONE || $2 > RTP_MAX) {
				yyerror("invalid fib-priority");
				YYERROR;
			}
			conf->fib_priority_internal = $2;
		}
		| FIBPRIORITY_EXT NUMBER {
			if ($2 <= RTP_NONE || $2 > RTP_MAX) {
				yyerror("invalid fib-priority");
				YYERROR;
			}
			conf->fib_priority_external = $2;
		}
		| defaults
		;

redistribute	: no REDISTRIBUTE STRING opt_red_metric {
			struct redistribute	*r;

			if ((r = calloc(1, sizeof(*r))) == NULL)
				fatal(NULL);
			if (!strcmp($3, "default"))
				r->type = REDIST_DEFAULT;
			else if (!strcmp($3, "static"))
				r->type = REDIST_STATIC;
			else if (!strcmp($3, "rip"))
				r->type = REDIST_RIP;
			else if (!strcmp($3, "ospf"))
				r->type = REDIST_OSPF;
			else if (!strcmp($3, "connected"))
				r->type = REDIST_CONNECTED;
			else if (host($3, &r->addr, &r->prefixlen) >= 0)
				r->type = REDIST_ADDR;
			else {
				yyerror("invalid redistribute");
				free($3);
				free(r);
				YYERROR;
			}

			r->af = af;
			if ($1)
				r->type |= REDIST_NO;
			r->metric = $4;
			free($3);
			$$ = r;
		}
		;

redist_metric	: NUMBER NUMBER NUMBER NUMBER NUMBER {
			struct redist_metric	*m;

			if ($1 < MIN_BANDWIDTH || $1 > MAX_BANDWIDTH) {
				yyerror("bandwidth out of range (%d-%d)",
				    MIN_BANDWIDTH, MAX_BANDWIDTH);
				YYERROR;
			}
			if ($2 < MIN_DELAY || $2 > MAX_DELAY) {
				yyerror("delay out of range (%d-%d)",
				    MIN_DELAY, MAX_DELAY);
				YYERROR;
			}
			if ($3 < MIN_RELIABILITY || $3 > MAX_RELIABILITY) {
				yyerror("reliability out of range (%d-%d)",
				    MIN_RELIABILITY, MAX_RELIABILITY);
				YYERROR;
			}
			if ($4 < MIN_LOAD || $4 > MAX_LOAD) {
				yyerror("load out of range (%d-%d)",
				    MIN_LOAD, MAX_LOAD);
				YYERROR;
			}
			if ($5 < MIN_MTU || $5 > MAX_MTU) {
				yyerror("mtu out of range (%d-%d)",
				    MIN_MTU, MAX_MTU);
				YYERROR;
			}

			if ((m = calloc(1, sizeof(*m))) == NULL)
				fatal(NULL);
			m->bandwidth = $1;
			m->delay = $2;
			m->reliability = $3;
			m->load = $4;
			m->mtu = $5;

			$$ = m;
		}
		;

opt_red_metric	: /* empty */			{ $$ = NULL; }
		| METRIC redist_metric 		{ $$ = $2; }
		;

defaults	: KVALUES NUMBER NUMBER NUMBER NUMBER NUMBER NUMBER {
			if ($2 < MIN_KVALUE || $2 > MAX_KVALUE ||
			    $3 < MIN_KVALUE || $3 > MAX_KVALUE ||
			    $4 < MIN_KVALUE || $4 > MAX_KVALUE ||
			    $5 < MIN_KVALUE || $5 > MAX_KVALUE ||
			    $6 < MIN_KVALUE || $6 > MAX_KVALUE ||
			    $7 < MIN_KVALUE || $7 > MAX_KVALUE) {
				yyerror("k-value out of range (%d-%d)",
				    MIN_KVALUE, MAX_KVALUE);
				YYERROR;
			}
			defs->kvalues[0] = $2;
			defs->kvalues[1] = $3;
			defs->kvalues[2] = $4;
			defs->kvalues[3] = $5;
			defs->kvalues[4] = $6;
			defs->kvalues[5] = $7;
		}
		| ACTIVETIMEOUT NUMBER {
			if ($2 < MIN_ACTIVE_TIMEOUT ||
			    $2 > MAX_ACTIVE_TIMEOUT) {
				yyerror("active-timeout out of range (%d-%d)",
				    MIN_ACTIVE_TIMEOUT, MAX_ACTIVE_TIMEOUT);
				YYERROR;
			}
			defs->active_timeout = $2;
		}
		| MAXHOPS NUMBER {
			if ($2 < MIN_MAXIMUM_HOPS ||
			    $2 > MAX_MAXIMUM_HOPS) {
				yyerror("maximum-hops out of range (%d-%d)",
				    MIN_MAXIMUM_HOPS, MAX_MAXIMUM_HOPS);
				YYERROR;
			}
			defs->maximum_hops = $2;
		}
		| MAXPATHS NUMBER {
			if ($2 < MIN_MAXIMUM_PATHS ||
			    $2 > MAX_MAXIMUM_PATHS) {
				yyerror("maximum-paths out of range (%d-%d)",
				    MIN_MAXIMUM_PATHS, MAX_MAXIMUM_PATHS);
				YYERROR;
			}
			defs->maximum_paths = $2;
		}
		| VARIANCE NUMBER {
			if ($2 < MIN_VARIANCE ||
			    $2 > MAX_VARIANCE) {
				yyerror("variance out of range (%d-%d)",
				    MIN_VARIANCE, MAX_VARIANCE);
				YYERROR;
			}
			defs->variance = $2;
		}
		| DFLTMETRIC redist_metric {
			defs->dflt_metric = $2;
		}
		| iface_defaults
		;

iface_defaults	: HELLOINTERVAL NUMBER {
			if ($2 < MIN_HELLO_INTERVAL ||
			    $2 > MAX_HELLO_INTERVAL) {
				yyerror("hello-interval out of range (%d-%d)",
				    MIN_HELLO_INTERVAL, MAX_HELLO_INTERVAL);
				YYERROR;
			}
			defs->hello_interval = $2;
		}
		| HOLDTIME NUMBER {
			if ($2 < MIN_HELLO_HOLDTIME ||
			    $2 > MAX_HELLO_HOLDTIME) {
				yyerror("hold-timel out of range (%d-%d)",
				    MIN_HELLO_HOLDTIME,
				    MAX_HELLO_HOLDTIME);
				YYERROR;
			}
			defs->hello_holdtime = $2;
		}
		| DELAY NUMBER {
			if ($2 < MIN_DELAY || $2 > MAX_DELAY) {
				yyerror("delay out of range (%d-%d)",
				    MIN_DELAY, MAX_DELAY);
				YYERROR;
			}
			defs->delay = $2;
		}
		| BANDWIDTH NUMBER {
			if ($2 < MIN_BANDWIDTH || $2 > MAX_BANDWIDTH) {
				yyerror("bandwidth out of range (%d-%d)",
				    MIN_BANDWIDTH, MAX_BANDWIDTH);
				YYERROR;
			}
			defs->bandwidth = $2;
		}
		| SPLITHORIZON yesno {
			defs->splithorizon = $2;
		}
		;

optnl		: '\n' optnl
		|
		;

nl		: '\n' optnl		/* one newline or more */
		;

af		: AF eigrp_af {
			af = $2;
			memcpy(&afdefs, defs, sizeof(afdefs));
			defs = &afdefs;
		} af_block {
			af = AF_UNSPEC;
			defs = &globaldefs;
		}
		;

af_block	: '{' optnl afopts_l '}'
		| '{' optnl '}'
		|
		;

afopts_l	: afopts_l afoptsl nl
		| afoptsl optnl
		;

afoptsl		: as
		| defaults
		;

as		: AS NUMBER {
			if ($2 < EIGRP_MIN_AS || $2 > EIGRP_MAX_AS) {
				yyerror("invalid autonomous-system");
				YYERROR;
			}
			eigrp = conf_get_instance($2);
			if (eigrp == NULL)
				YYERROR;
			TAILQ_INSERT_TAIL(&conf->instances, eigrp, entry);

			memcpy(&asdefs, defs, sizeof(asdefs));
			defs = &asdefs;
		} as_block {
			memcpy(eigrp->kvalues, defs->kvalues,
			    sizeof(eigrp->kvalues));
			eigrp->active_timeout = defs->active_timeout;
			eigrp->maximum_hops = defs->maximum_hops;
			eigrp->maximum_paths = defs->maximum_paths;
			eigrp->variance = defs->variance;
			eigrp->dflt_metric = defs->dflt_metric;
			eigrp = NULL;
			defs = &afdefs;
		}
		;

as_block	: '{' optnl asopts_l '}'
		| '{' optnl '}'
		|
		;

asopts_l	: asopts_l asoptsl nl
		| asoptsl optnl
		;

asoptsl		: interface
		| redistribute {
			SIMPLEQ_INSERT_TAIL(&eigrp->redist_list, $1, entry);
		}
		| defaults
		;

interface	: INTERFACE STRING	{
			struct kif	*kif;

			if ((kif = kif_findname($2)) == NULL) {
				yyerror("unknown interface %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
			ei = conf_get_if(kif);
			if (ei == NULL)
				YYERROR;

			memcpy(&ifacedefs, defs, sizeof(ifacedefs));
			defs = &ifacedefs;
		} interface_block {
			ei->hello_holdtime = defs->hello_holdtime;
			ei->hello_interval = defs->hello_interval;
			ei->delay = defs->delay;
			ei->bandwidth = defs->bandwidth;
			ei->splithorizon = defs->splithorizon;
			ei = NULL;
			defs = &asdefs;
		}
		;

interface_block	: '{' optnl interfaceopts_l '}'
		| '{' optnl '}'
		|
		;

interfaceopts_l	: interfaceopts_l interfaceoptsl nl
		| interfaceoptsl optnl
		;

interfaceoptsl	: PASSIVE		{ ei->passive = 1; }
		| iface_defaults
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
	char		*msg;

	file->errors++;
	va_start(ap, fmt);
	if (vasprintf(&msg, fmt, ap) == -1)
		fatalx("yyerror vasprintf");
	va_end(ap);
	logit(LOG_CRIT, "%s:%d: %s", file->name, yylval.lineno, msg);
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
		{"active-timeout",		ACTIVETIMEOUT},
		{"address-family",		AF},
		{"autonomous-system",		AS},
		{"bandwidth",			BANDWIDTH},
		{"default-metric",		DFLTMETRIC},
		{"delay",			DELAY},
		{"fib-priority-external",	FIBPRIORITY_EXT},
		{"fib-priority-internal",	FIBPRIORITY_INT},
		{"fib-update",			FIBUPDATE},
		{"hello-interval",		HELLOINTERVAL},
		{"holdtime",			HOLDTIME},
		{"include",			INCLUDE},
		{"interface",			INTERFACE},
		{"ipv4",			IPV4},
		{"ipv6",			IPV6},
		{"k-values",			KVALUES},
		{"maximum-hops",		MAXHOPS},
		{"maximum-paths",		MAXPATHS},
		{"metric",			METRIC},
		{"no",				NO},
		{"passive",			PASSIVE},
		{"rdomain",			RDOMAIN},
		{"redistribute",		REDISTRIBUTE},
		{"router-id",			ROUTERID},
		{"split-horizon",		SPLITHORIZON},
		{"variance",			VARIANCE},
		{"yes",				YES}
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

unsigned char	*parsebuf;
int		 parseindex;
unsigned char	 pushback_buffer[MAXPUSHBACK];
int		 pushback_index = 0;

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
	unsigned char	 buf[8096];
	unsigned char	*p, *val;
	int		 quotec, next, c;
	int		 token;

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
	if (st.st_mode & (S_IWGRP | S_IXGRP | S_IRWXO)) {
		log_warnx("%s: group writable or world read/writable", fname);
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

struct eigrpd_conf *
parse_config(char *filename, int opts)
{
	struct sym	*sym, *next;

	if ((conf = calloc(1, sizeof(struct eigrpd_conf))) == NULL)
		fatal("parse_config");
	conf->opts = opts;
	conf->rdomain = 0;
	conf->fib_priority_internal = RTP_EIGRP;
	conf->fib_priority_external = RTP_EIGRP;

	memset(&globaldefs, 0, sizeof(globaldefs));
	defs = &globaldefs;
	defs->kvalues[0] = defs->kvalues[2] = 1;
	defs->active_timeout = DEFAULT_ACTIVE_TIMEOUT;
	defs->maximum_hops = DEFAULT_MAXIMUM_HOPS;
	defs->maximum_paths = DEFAULT_MAXIMUM_PATHS;
	defs->variance = DEFAULT_VARIANCE;
	defs->hello_holdtime = DEFAULT_HELLO_HOLDTIME;
	defs->hello_interval = DEFAULT_HELLO_INTERVAL;
	defs->delay = DEFAULT_DELAY;
	defs->bandwidth = DEFAULT_BANDWIDTH;
	defs->splithorizon = 1;

	if ((file = pushfile(filename, !(conf->opts & EIGRPD_OPT_NOACTION))) == NULL) {
		free(conf);
		return (NULL);
	}
	topfile = file;

	TAILQ_INIT(&conf->iface_list);
	TAILQ_INIT(&conf->instances);

	yyparse();
	errors = file->errors;
	popfile();

	/* Free macros and check which have not been used. */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entry);
		if ((conf->opts & EIGRPD_OPT_VERBOSE2) && !sym->used)
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

struct eigrp *
conf_get_instance(uint16_t as)
{
	struct eigrp	*e;

	if (eigrp_find(conf, af, as)) {
		yyerror("autonomous-system %u already configured"
		    "for address-family %s", as, af_name(af));
		return (NULL);
	}

	e = calloc(1, sizeof(struct eigrp));
	e->af = af;
	e->as = as;
	SIMPLEQ_INIT(&e->redist_list);
	TAILQ_INIT(&e->ei_list);
	RB_INIT(&e->nbrs);
	RB_INIT(&e->topology);

	/* start local sequence number used by RTP */
	e->seq_num = 1;

	return (e);
}

struct eigrp_iface *
conf_get_if(struct kif *kif)
{
	struct eigrp_iface	*e;

	TAILQ_FOREACH(e, &eigrp->ei_list, e_entry)
		if (e->iface->ifindex == kif->ifindex) {
			yyerror("interface %s already configured "
			    "for address-family %s and "
			    "autonomous-system %u", kif->ifname,
			    af_name(af), eigrp->as);
			return (NULL);
		}

	e = eigrp_if_new(conf, eigrp, kif);

	return (e);
}

void
clear_config(struct eigrpd_conf *xconf)
{
	free(xconf);
}

uint32_t
get_rtr_id(void)
{
	struct ifaddrs		*ifap, *ifa;
	uint32_t		 ip = 0, cur, localnet;

	localnet = htonl(INADDR_LOOPBACK & IN_CLASSA_NET);

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (strncmp(ifa->ifa_name, "carp", 4) == 0)
			continue;
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
host(const char *s, union eigrpd_addr *addr, uint8_t *plen)
{
	char			*p, *ps;
	const char		*errstr;
	int			 maxplen;

	switch (af) {
	case AF_INET:
		maxplen = 32;
		break;
	case AF_INET6:
		maxplen = 128;
		break;
	default:
		return (-1);
	}

	if ((p = strrchr(s, '/')) != NULL) {
		*plen = strtonum(p + 1, 0, maxplen, &errstr);
		if (errstr) {
			log_warnx("prefixlen is %s: %s", errstr, p + 1);
			return (-1);
		}
		if ((ps = malloc(strlen(s) - strlen(p) + 1)) == NULL)
			fatal("host: malloc");
		strlcpy(ps, s, strlen(s) - strlen(p) + 1);
	} else {
		if ((ps = strdup(s)) == NULL)
			fatal("host: strdup");
		*plen = maxplen;
	}

	memset(addr, 0, sizeof(union eigrpd_addr));
	switch (af) {
	case AF_INET:
		if (inet_pton(AF_INET, ps, &addr->v4) != 1)
			return (-1);
		break;
	case AF_INET6:
		if (inet_pton(AF_INET6, ps, &addr->v6) != 1)
			return (-1);
		break;
	default:
		return (-1);
	}
	eigrp_applymask(af, addr, addr, *plen);
	free(ps);

	return (0);
}

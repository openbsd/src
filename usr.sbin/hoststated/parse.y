/*	$OpenBSD: parse.y,v 1.10 2007/01/08 14:30:31 reyk Exp $	*/

/*
 * Copyright (c) 2006 Pierre-Yves Ritschard <pyr@spootnik.org>
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
#include <sys/queue.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <regex.h>

#include "hostated.h"

struct hostated			*conf = NULL;
static FILE			*fin = NULL;
static int			 lineno = 1;
static int			 errors = 0;
const char			*infile;
char				*start_state;
objid_t				 last_service_id = 0;
objid_t				 last_table_id = 0;
objid_t				 last_host_id = 0;

static struct service		*service = NULL;
static struct table		*table = NULL;

int	 yyerror(const char *, ...);
int	 yyparse(void);
int	 kw_cmp(const void *, const void *);
int	 lookup(char *);
int	 lgetc(FILE *);
int	 lungetc(int);
int	 findeol(void);
int	 yylex(void);

TAILQ_HEAD(symhead, sym)	 symhead = TAILQ_HEAD_INITIALIZER(symhead);
struct sym {
	TAILQ_ENTRY(sym)	 entries;
	int			 used;
	int			 persist;
	char			*nam;
	char			*val;
};

int		 symset(const char *, const char *, int);
char		*symget(const char *);
int		 cmdline_symset(char *);

struct address	*host_v4(const char *);
struct address	*host_v6(const char *);
int		 host_dns(const char *, struct addresslist *,
		    int, in_port_t, const char *);
int		 host(const char *, struct addresslist *,
		    int, in_port_t, const char *);

typedef struct {
	union {
		u_int32_t	 number;
		char		*string;
		struct host	*host;
		struct timeval	 tv;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	SERVICE TABLE BACKUP HOST REAL
%token  CHECK HTTP TCP ICMP EXTERNAL
%token  TIMEOUT CODE DIGEST PORT TAG INTERFACE
%token	VIRTUAL IP INTERVAL DISABLE STICKYADDR
%token	SEND EXPECT NOTHING
%token	ERROR
%token	<v.string>	STRING
%type	<v.string>	interface
%type	<v.number>	number
%type	<v.host>	host
%type	<v.tv>		timeout

%%

grammar		: /* empty */
		| grammar '\n'
		| grammar varset '\n'
		| grammar main '\n'
		| grammar service '\n'
		| grammar table '\n'
		| grammar error '\n'		{ errors++; }
		;

number		: STRING	{
			const char	*estr;

			$$ = strtonum($1, 0, UINT_MAX, &estr);
			if (estr) {
				yyerror("cannot parse number %s : %s",
				    $1, estr);
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

varset		: STRING '=' STRING	{
			if (symset($1, $3, 0) == -1)
				fatal("cannot store variable");
			free($1);
			free($3);
		}
		;

sendbuf		: NOTHING		{
			bzero(table->sendbuf, sizeof(table->sendbuf));
		}
		| STRING		{
			if (strlcpy(table->sendbuf, $1, sizeof(table->sendbuf))
			    >= sizeof(table->sendbuf)) {
				yyerror("yyparse: send buffer truncated");
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

main		: INTERVAL number	{ conf->interval = $2; }
		| TIMEOUT timeout	{
			bcopy(&$2, &conf->timeout, sizeof(struct timeval));
		}
		;

service		: SERVICE STRING	{
			struct service *srv;

			TAILQ_FOREACH(srv, &conf->services, entry)
				if (!strcmp(srv->name, $2))
					break;
			if (srv != NULL) {
				yyerror("service %s defined twice", $2);
				free($2);
				YYERROR;
			}
			if ((srv = calloc(1, sizeof (*srv))) == NULL)
				fatal("out of memory");

			if (strlcpy(srv->name, $2, sizeof(srv->name)) >=
			    sizeof(srv->name)) {
				yyerror("service name truncated");
				YYERROR;
			}
			free($2);
			srv->id = last_service_id++;
			if (last_service_id == UINT_MAX) {
				yyerror("too many services defined");
				YYERROR;
			}
			service = srv;
		} '{' optnl serviceopts_l '}'	{
			if (service->table == NULL) {
				yyerror("service %s has no table",
				    service->name);
				YYERROR;
			}
			if (TAILQ_EMPTY(&service->virts)) {
				yyerror("service %s has no virtual ip",
				    service->name);
				YYERROR;
			}
			conf->servicecount++;
			if (service->backup == NULL)
				service->backup = &conf->empty_table;
			else if (service->backup->port !=
			    service->table->port) {
				yyerror("service %s uses two different ports "
				    "for its table and backup table",
				    service->name);
				YYERROR;
			}

			if (!(service->flags & F_DISABLE))
				service->flags |= F_ADD;
			TAILQ_INSERT_HEAD(&conf->services, service, entry);
		}
		;

serviceopts_l	: serviceopts_l serviceoptsl nl
		| serviceoptsl optnl
		;

serviceoptsl	: TABLE STRING	{
			struct table *tb;

			TAILQ_FOREACH(tb, &conf->tables, entry)
				if (!strcmp(tb->name, $2))
					break;
			if (tb == NULL) {
				yyerror("no such table: %s", $2);
				free($2);
				YYERROR;
			} else {
				service->table = tb;
				service->table->serviceid = service->id;
				service->table->flags |= F_USED;
				free($2);
			}
		}
		| BACKUP TABLE STRING	{
			struct table *tb;

			if (service->backup) {
				yyerror("backup already specified");
				free($3);
				YYERROR;
			}

			TAILQ_FOREACH(tb, &conf->tables, entry)
				if (!strcmp(tb->name, $3))
					break;

			if (tb == NULL) {
				yyerror("no such table: %s", $3);
				free($3);
				YYERROR;
			} else {
				service->backup = tb;
				service->backup->serviceid = service->id;
				service->backup->flags |= (F_USED|F_BACKUP);
				free($3);
			}
		}
		| VIRTUAL IP STRING PORT number	interface {
			if ($5 < 1 || $5 > USHRT_MAX) {
				yyerror("invalid port number: %d", $5);
				free($3);
				free($6);
				YYERROR;
			}
			if (host($3, &service->virts,
				 SRV_MAX_VIRTS, htons($5), $6) <= 0) {
				yyerror("invalid virtual ip: %s", $3);
				free($3);
				free($6);
				YYERROR;
			}
			free($3);
			free($6);
		}
		| DISABLE			{ service->flags |= F_DISABLE; }
		| STICKYADDR			{ service->flags |= F_STICKY; }
		| TAG STRING {
			if (strlcpy(service->tag, $2, sizeof(service->tag)) >=
			    sizeof(service->tag)) {
				yyerror("service tag name truncated");
				free($2);
				YYERROR;
			}
			free($2);
		}
		;

table		: TABLE STRING	{
			struct table *tb;

			TAILQ_FOREACH(tb, &conf->tables, entry)
				if (!strcmp(tb->name, $2))
					break;
			if (tb != NULL) {
				yyerror("table %s defined twice");
				free($2);
				YYERROR;
			}

			if ((tb = calloc(1, sizeof (*tb))) == NULL)
				fatal("out of memory");

			if (strlcpy(tb->name, $2, sizeof(tb->name)) >=
			    sizeof(tb->name)) {
				yyerror("table name truncated");
				YYERROR;
			}
			tb->id = last_table_id++;
			bcopy(&conf->timeout, &tb->timeout,
			    sizeof(struct timeval));
			if (last_table_id == UINT_MAX) {
				yyerror("too many tables defined");
				YYERROR;
			}
			free($2);
			table = tb;
		} '{' optnl tableopts_l '}'	{
			if (table->port == 0) {
				yyerror("table %s has no port", table->name);
				YYERROR;
			}
			if (TAILQ_EMPTY(&table->hosts)) {
				yyerror("table %s has no hosts", table->name);
				YYERROR;
			}
			if (table->check == CHECK_NOCHECK) {
				yyerror("table %s has no check", table->name);
				YYERROR;
			}
			conf->tablecount++;
			TAILQ_INSERT_HEAD(&conf->tables, table, entry);
		}
		;

tableopts_l	: tableopts_l tableoptsl nl
		| tableoptsl optnl
		;

tableoptsl	: host			{
			$1->tableid = table->id;
			$1->tablename = table->name;
			TAILQ_INSERT_HEAD(&table->hosts, $1, entry);
		}
		| TIMEOUT timeout	{
			bcopy(&$2, &table->timeout, sizeof(struct timeval));
		}
		| CHECK ICMP		{
			table->check = CHECK_ICMP;
		}
		| CHECK TCP		{
			table->check = CHECK_TCP;
		}
		| CHECK HTTP STRING CODE number {
			table->check = CHECK_HTTP_CODE;
			table->retcode = $5;
			if (strlcpy(table->path, $3, sizeof(table->path)) >=
			    sizeof(table->path)) {
				yyerror("http path truncated");
				free($3);
				YYERROR;
			}
		}
		| CHECK HTTP STRING DIGEST STRING {
			table->check = CHECK_HTTP_DIGEST;
			if (strlcpy(table->path, $3, sizeof(table->path)) >=
			    sizeof(table->path)) {
				yyerror("http path truncated");
				free($3);
				free($5);
				YYERROR;
			}
			if (strlcpy(table->digest, $5,
			    sizeof(table->digest)) >= sizeof(table->digest)) {
				yyerror("http digest truncated");
				free($3);
				free($5);
				YYERROR;
			}
			free($3);
			free($5);
		}
		| CHECK SEND sendbuf EXPECT STRING {
			int	ret;
			char	ebuf[32];

			table->check = CHECK_SEND_EXPECT;
			ret = regcomp(&table->regx, $5, REG_EXTENDED|REG_NOSUB);
			if (ret != 0) {
				regerror(ret, &table->regx, ebuf, sizeof(ebuf));
				yyerror("cannot compile expect regexp: %s",
				    ebuf);
				free($5);
				YYERROR;
			}
			free($5);
		}
		| REAL PORT number {
			if ($3 < 1 || $3 >= USHRT_MAX) {
				yyerror("invalid port number: %d", $3);
				YYERROR;
			}
			table->port = $3;
		}
		| DISABLE			{ table->flags |= F_DISABLE; }
		;

interface	: /*empty*/		{ $$ = NULL; }
		| INTERFACE STRING	{ $$ = $2; }
		;

host		: HOST STRING {
			struct host *r;
			struct address *a;
			struct addresslist al;

			if ((r = calloc(1, sizeof(*r))) == NULL)
				fatal("out of memory");

			TAILQ_INIT(&al);
			if (host($2, &al, 1, 0, NULL) <= 0) {
				yyerror("invalid host %s", $2);
				free($2);
				YYERROR;
			}
			a = TAILQ_FIRST(&al);
			memcpy(&r->ss, &a->ss, sizeof(r->ss));
			free(a);

			if (strlcpy(r->name, $2, sizeof(r->name)) >=
			    sizeof(r->name)) {
				yyerror("host name truncated");
				free($2);
				YYERROR;
			} else {
				r->id = last_host_id++;
				if (last_host_id == UINT_MAX) {
					yyerror("too many hosts defined");
					YYERROR;
				}
				free($2);
				$$ = r;
			}
		}
		;

timeout		: number
		{
			$$.tv_sec = $1 / 1000;
			$$.tv_usec = ($1 % 1000) * 1000;
		}
		;

optnl		: '\n' optnl
		|
		;

nl		: '\n' optnl
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
		{ "backup",		BACKUP },
		{ "check",		CHECK },
		{ "code",		CODE },
		{ "digest",		DIGEST },
		{ "disable",		DISABLE },
		{ "expect",		EXPECT },
		{ "external",		EXTERNAL },
		{ "host",		HOST },
		{ "http",		HTTP },
		{ "icmp",		ICMP },
		{ "interface",		INTERFACE },
		{ "interval",		INTERVAL },
		{ "ip",			IP },
		{ "nothing",		NOTHING },
		{ "port",		PORT },
		{ "real",		REAL },
		{ "send",		SEND },
		{ "service",		SERVICE },
		{ "sticky-address",	STICKYADDR },
		{ "table",		TABLE },
		{ "tag",		TAG },
		{ "tcp",		TCP },
		{ "timeout",		TIMEOUT },
		{ "virtual",		VIRTUAL }
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
		if (next == 'n') {
			c = '\n';
			break;
		} else if (next == 'r') {
			c = '\r';
			break;
		} else if (next != '\n') {
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

int
parse_config(struct hostated *x_conf, const char *filename, int opts)
{
	struct sym	*sym, *next;

	conf = x_conf;

	TAILQ_INIT(&conf->services);
	TAILQ_INIT(&conf->tables);
	memset(&conf->empty_table, 0, sizeof(conf->empty_table));
	conf->empty_table.id = EMPTY_TABLE;
	conf->empty_table.flags |= F_DISABLE;
	(void)strlcpy(conf->empty_table.name, "empty",
	    sizeof(conf->empty_table.name));

	conf->timeout.tv_sec = CHECK_TIMEOUT / 1000;
	conf->timeout.tv_usec = (CHECK_TIMEOUT % 1000) * 1000;
	conf->interval = CHECK_INTERVAL;
	conf->opts = opts;

	if ((fin = fopen(filename, "r")) == NULL) {
		warn("%s", filename);
		return (NULL);
	}
	infile = filename;
	yyparse();
	fclose(fin);

	/* Free macros and check which have not been used. */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entries);
		if ((conf->opts & HOSTATED_OPT_VERBOSE) && !sym->used)
			fprintf(stderr, "warning: macro '%s' not "
			    "used\n", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entries);
			free(sym);
		}
	}

	if (TAILQ_EMPTY(&conf->services)) {
		log_warnx("no services, nothing to do");
		errors++;
	}

	/* Verify that every table is used */
	TAILQ_FOREACH(table, &conf->tables, entry)
		if (!(table->flags & F_USED)) {
			log_warnx("unused table: %s", table->name);
			errors++;
		}

	if (errors) {
		bzero(&conf, sizeof (*conf));
		return (-1);
	}

	return (0);
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

struct address *
host_v4(const char *s)
{
	struct in_addr		 ina;
	struct sockaddr_in	*sain;
	struct address		*h;

	bzero(&ina, sizeof(ina));
	if (inet_pton(AF_INET, s, &ina) != 1)
		return (NULL);

	if ((h = calloc(1, sizeof(*h))) == NULL)
		fatal(NULL);
	sain = (struct sockaddr_in *)&h->ss;
	sain->sin_len = sizeof(struct sockaddr_in);
	sain->sin_family = AF_INET;
	sain->sin_addr.s_addr = ina.s_addr;

	return (h);
}

struct address *
host_v6(const char *s)
{
	struct in6_addr		 ina6;
	struct sockaddr_in6	*sin6;
	struct address		*h;

	bzero(&ina6, sizeof(ina6));
	if (inet_pton(AF_INET6, s, &ina6) != 1)
		return (NULL);

	if ((h = calloc(1, sizeof(*h))) == NULL)
		fatal(NULL);
	sin6 = (struct sockaddr_in6 *)&h->ss;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	sin6->sin6_family = AF_INET6;
	memcpy(&sin6->sin6_addr, &ina6, sizeof(ina6));

	return (h);
}

int
host_dns(const char *s, struct addresslist *al, int max,
	 in_port_t port, const char *ifname)
{
	struct addrinfo		 hints, *res0, *res;
	int			 error, cnt = 0;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct address		*h;

	bzero(&hints, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM; /* DUMMY */
	error = getaddrinfo(s, NULL, &hints, &res0);
	if (error == EAI_AGAIN || error == EAI_NODATA || error == EAI_NONAME)
		return (0);
	if (error) {
		log_warnx("host_dns: could not parse \"%s\": %s", s,
		    gai_strerror(error));
		return (-1);
	}

	for (res = res0; res && cnt < max; res = res->ai_next) {
		if (res->ai_family != AF_INET &&
		    res->ai_family != AF_INET6)
			continue;
		if ((h = calloc(1, sizeof(*h))) == NULL)
			fatal(NULL);

		h->port = port;
		if (ifname != NULL) {
			if (strlcpy(h->ifname, ifname, sizeof(h->ifname)) >=
			    sizeof(h->ifname))
				log_warnx("host_dns: interface name truncated");
			return (-1);
		}
		h->ss.ss_family = res->ai_family;
		if (res->ai_family == AF_INET) {
			sain = (struct sockaddr_in *)&h->ss;
			sain->sin_len = sizeof(struct sockaddr_in);
			sain->sin_addr.s_addr = ((struct sockaddr_in *)
			    res->ai_addr)->sin_addr.s_addr;
		} else {
			sin6 = (struct sockaddr_in6 *)&h->ss;
			sin6->sin6_len = sizeof(struct sockaddr_in6);
			memcpy(&sin6->sin6_addr, &((struct sockaddr_in6 *)
			    res->ai_addr)->sin6_addr, sizeof(struct in6_addr));
		}

		TAILQ_INSERT_HEAD(al, h, entry);
		cnt++;
	}
	if (cnt == max && res) {
		log_warnx("host_dns: %s resolves to more than %d hosts",
		    s, max);
	}
	freeaddrinfo(res0);
	return (cnt);
}

int
host(const char *s, struct addresslist *al, int max,
    in_port_t port, const char *ifname)
{
	struct address *h;

	h = host_v4(s);

	/* IPv6 address? */
	if (h == NULL)
		h = host_v6(s);

	if (h != NULL) {
		h->port = port;
		if (ifname != NULL) {
			if (strlcpy(h->ifname, ifname, sizeof(h->ifname)) >=
			    sizeof(h->ifname)) {
				log_warnx("host: interface name truncated");
				return (-1);
			}
		}

		TAILQ_INSERT_HEAD(al, h, entry);
		return (1);
	}

	return (host_dns(s, al, max, port, ifname));
}

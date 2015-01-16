/*	$OpenBSD: parse.y,v 1.54 2015/01/16 06:40:17 deraadt Exp $	*/

/*
 * Copyright (c) 2007 - 2015 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2006 Pierre-Yves Ritschard <pyr@openbsd.org>
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
#include <sys/queue.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/pfvar.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/route.h>

#include <ctype.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <limits.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <ifaddrs.h>
#include <syslog.h>

#include "httpd.h"
#include "http.h"

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

struct httpd		*conf = NULL;
static int		 errors = 0;
static int		 loadcfg = 0;
uint32_t		 last_server_id = 0;

static struct server	*srv = NULL, *parentsrv = NULL;
static struct server_config *srv_conf = NULL;
struct serverlist	 servers;
struct media_type	 media;

struct address	*host_v4(const char *);
struct address	*host_v6(const char *);
int		 host_dns(const char *, struct addresslist *,
		    int, struct portrange *, const char *, int);
int		 host_if(const char *, struct addresslist *,
		    int, struct portrange *, const char *, int);
int		 host(const char *, struct addresslist *,
		    int, struct portrange *, const char *, int);
void		 host_free(struct addresslist *);
struct server	*server_inherit(struct server *, const char *,
		    struct server_config *);
int		 getservice(char *);
int		 is_if_in_group(const char *, const char *);

typedef struct {
	union {
		int64_t			 number;
		char			*string;
		struct timeval		 tv;
		struct portrange	 port;
		struct {
			struct sockaddr_storage	 ss;
			char			 name[HOST_NAME_MAX+1];
		}			 addr;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	ACCESS ALIAS AUTO BACKLOG BODY BUFFER CERTIFICATE CHROOT CIPHERS COMMON
%token	COMBINED CONNECTION DIRECTORY ERR FCGI INDEX IP KEY LISTEN LOCATION
%token	LOG LOGDIR MAXIMUM NO NODELAY ON PORT PREFORK REQUEST REQUESTS ROOT
%token	SACK SERVER SOCKET STRIP STYLE SYSLOG TCP TIMEOUT TLS TYPES 
%token	ERROR INCLUDE
%token	<v.string>	STRING
%token  <v.number>	NUMBER
%type	<v.port>	port
%type	<v.number>	opttls
%type	<v.tv>		timeout
%type	<v.string>	numberstring

%%

grammar		: /* empty */
		| grammar include '\n'
		| grammar '\n'
		| grammar varset '\n'
		| grammar main '\n'
		| grammar server '\n'
		| grammar types '\n'
		| grammar error '\n'		{ file->errors++; }
		;

include		: INCLUDE STRING		{
			struct file	*nfile;

			if ((nfile = pushfile($2, 0)) == NULL) {
				yyerror("failed to include file %s", $2);
				free($2);
				YYERROR;
			}
			free($2);

			file = nfile;
			lungetc('\n');
		}
		;

varset		: STRING '=' STRING	{
			if (symset($1, $3, 0) == -1)
				fatal("cannot store variable");
			free($1);
			free($3);
		}
		;

opttls		: /*empty*/	{ $$ = 0; }
		| TLS		{ $$ = 1; }
		;

main		: PREFORK NUMBER	{
			if (loadcfg)
				break;
			if ($2 <= 0 || $2 > SERVER_MAXPROC) {
				yyerror("invalid number of preforked "
				    "servers: %lld", $2);
				YYERROR;
			}
			conf->sc_prefork_server = $2;
		}
		| CHROOT STRING		{
			conf->sc_chroot = $2;
		}
		| LOGDIR STRING		{
			conf->sc_logdir = $2;
		}
		;

server		: SERVER STRING		{
			struct server	*s;

			if (!loadcfg) {
				free($2);
				YYACCEPT;
			}

			if ((s = calloc(1, sizeof (*s))) == NULL)
				fatal("out of memory");

			if (strlcpy(s->srv_conf.name, $2,
			    sizeof(s->srv_conf.name)) >=
			    sizeof(s->srv_conf.name)) {
				yyerror("server name truncated");
				free($2);
				free(s);
				YYERROR;
			}
			free($2);

			strlcpy(s->srv_conf.root, HTTPD_DOCROOT,
			    sizeof(s->srv_conf.root));
			strlcpy(s->srv_conf.index, HTTPD_INDEX,
			    sizeof(s->srv_conf.index));
			strlcpy(s->srv_conf.accesslog, HTTPD_ACCESS_LOG,
			    sizeof(s->srv_conf.accesslog));
			strlcpy(s->srv_conf.errorlog, HTTPD_ERROR_LOG,
			    sizeof(s->srv_conf.errorlog));
			s->srv_conf.id = ++last_server_id;
			s->srv_s = -1;
			s->srv_conf.timeout.tv_sec = SERVER_TIMEOUT;
			s->srv_conf.maxrequests = SERVER_MAXREQUESTS;
			s->srv_conf.maxrequestbody = SERVER_MAXREQUESTBODY;
			s->srv_conf.flags |= SRVFLAG_LOG;
			s->srv_conf.logformat = LOG_FORMAT_COMMON;
			if ((s->srv_conf.tls_cert_file =
			    strdup(HTTPD_TLS_CERT)) == NULL)
				fatal("out of memory");
			if ((s->srv_conf.tls_key_file =
			    strdup(HTTPD_TLS_KEY)) == NULL)
				fatal("out of memory");
			strlcpy(s->srv_conf.tls_ciphers, HTTPD_TLS_CIPHERS,
			    sizeof(s->srv_conf.tls_ciphers));

			if (last_server_id == INT_MAX) {
				yyerror("too many servers defined");
				free(s);
				YYERROR;
			}
			srv = s;
			srv_conf = &srv->srv_conf;

			SPLAY_INIT(&srv->srv_clients);
			TAILQ_INIT(&srv->srv_hosts);

			TAILQ_INSERT_TAIL(&srv->srv_hosts, srv_conf, entry);
		} '{' optnl serveropts_l '}'	{
			struct server		*s = NULL, *sn;
			struct server_config	*a, *b;

			srv_conf = &srv->srv_conf;

			TAILQ_FOREACH(s, conf->sc_servers, srv_entry) {
				if ((s->srv_conf.flags &
				    SRVFLAG_LOCATION) == 0 &&
				    strcmp(s->srv_conf.name,
				    srv->srv_conf.name) == 0 &&
				    s->srv_conf.port == srv->srv_conf.port &&
				    sockaddr_cmp(
				    (struct sockaddr *)&s->srv_conf.ss,
				    (struct sockaddr *)&srv->srv_conf.ss,
				    s->srv_conf.prefixlen) == 0)
					break;
			}
			if (s != NULL) {
				yyerror("server \"%s\" defined twice",
				    srv->srv_conf.name);
				serverconfig_free(srv_conf);
				free(srv);
				YYABORT;
			}

			if (srv->srv_conf.ss.ss_family == AF_UNSPEC) {
				yyerror("listen address not specified");
				serverconfig_free(srv_conf);
				free(srv);
				YYERROR;
			}

			if (server_tls_load_keypair(srv) == -1) {
				yyerror("failed to load public/private keys "
				    "for server %s", srv->srv_conf.name);
				serverconfig_free(srv_conf);
				free(srv);
				YYERROR;
			}

			DPRINTF("adding server \"%s[%u]\"",
			    srv->srv_conf.name, srv->srv_conf.id);

			TAILQ_INSERT_TAIL(conf->sc_servers, srv, srv_entry);

			/*
			 * Add aliases and additional listen addresses as
			 * individual servers.
			 */
			TAILQ_FOREACH(a, &srv->srv_hosts, entry) {
				/* listen address */
				if (a->ss.ss_family == AF_UNSPEC)
					continue;
				TAILQ_FOREACH(b, &srv->srv_hosts, entry) {
					/* alias name */
					if (*b->name == '\0' ||
					    (b == &srv->srv_conf && b == a))
						continue;

					if ((sn = server_inherit(srv,
					    b->name, a)) == NULL) {
						serverconfig_free(srv_conf);
						free(srv);
						YYABORT;
					}

					DPRINTF("adding server \"%s[%u]\"",
					    sn->srv_conf.name, sn->srv_conf.id);

					TAILQ_INSERT_TAIL(conf->sc_servers,
					    sn, srv_entry);
				}
			}

			/* Remove temporary aliases */
			TAILQ_FOREACH_SAFE(a, &srv->srv_hosts, entry, b) {
				TAILQ_REMOVE(&srv->srv_hosts, a, entry);
				if (a == &srv->srv_conf)
					continue;
				serverconfig_free(a);
				free(a);
			}

			srv = NULL;
			srv_conf = NULL;
		}
		;

serveropts_l	: serveropts_l serveroptsl nl
		| serveroptsl optnl
		;

serveroptsl	: LISTEN ON STRING opttls port {
			struct addresslist	 al;
			struct address		*h;
			struct server_config	*s_conf, *alias = NULL;

			if (parentsrv != NULL) {
				yyerror("listen %s inside location", $3);
				free($3);
				YYERROR;
			}

			if (srv->srv_conf.ss.ss_family != AF_UNSPEC) {
				if ((alias = calloc(1,
				    sizeof(*alias))) == NULL)
					fatal("out of memory");

				/* Add as an alias */
				s_conf = alias;
			} else
				s_conf = &srv->srv_conf;
			if ($5.op != PF_OP_EQ) {
				yyerror("invalid port");
				free($3);
				YYERROR;
			}

			TAILQ_INIT(&al);
			if (host($3, &al, 1, &$5, NULL, -1) <= 0) {
				yyerror("invalid listen ip: %s", $3);
				free($3);
				YYERROR;
			}
			free($3);
			h = TAILQ_FIRST(&al);
			memcpy(&s_conf->ss, &h->ss, sizeof(s_conf->ss));
			s_conf->port = h->port.val[0];
			s_conf->prefixlen = h->prefixlen;
			host_free(&al);

			if ($4) {
				s_conf->flags |= SRVFLAG_TLS;
			}

			if (alias != NULL) {
				TAILQ_INSERT_TAIL(&srv->srv_hosts,
				    alias, entry);
			}
		}
		| ALIAS STRING		{
			struct server_config	*alias;

			if (parentsrv != NULL) {
				yyerror("alias inside location");
				free($2);
				YYERROR;
			}

			if ((alias = calloc(1, sizeof(*alias))) == NULL)
				fatal("out of memory");

			if (strlcpy(alias->name, $2, sizeof(alias->name)) >=
			    sizeof(alias->name)) {
				yyerror("server alias truncated");
				free($2);
				free(alias);
				YYERROR;
			}
			free($2);

			TAILQ_INSERT_TAIL(&srv->srv_hosts, alias, entry);
		}
		| tcpip			{
			if (parentsrv != NULL) {
				yyerror("tcp flags inside location");
				YYERROR;
			}
		}
		| connection		{
			if (parentsrv != NULL) {
				yyerror("connection options inside location");
				YYERROR;
			}
		}
		| tls			{
			if (parentsrv != NULL) {
				yyerror("tls configuration inside location");
				YYERROR;
			}
		}
		| root
		| directory
		| logformat
		| fastcgi
		| LOCATION STRING		{
			struct server	*s;

			if (srv->srv_conf.ss.ss_family == AF_UNSPEC) {
				yyerror("listen address not specified");
				free($2);
				YYERROR;
			}

			if (parentsrv != NULL) {
				yyerror("location %s inside location", $2);
				free($2);
				YYERROR;
			}

			if (!loadcfg) {
				free($2);
				YYACCEPT;
			}

			if ((s = calloc(1, sizeof (*s))) == NULL)
				fatal("out of memory");

			if (strlcpy(s->srv_conf.location, $2,
			    sizeof(s->srv_conf.location)) >=
			    sizeof(s->srv_conf.location)) {
				yyerror("server location truncated");
				free($2);
				free(s);
				YYERROR;
			}
			free($2);

			if (strlcpy(s->srv_conf.name, srv->srv_conf.name,
			    sizeof(s->srv_conf.name)) >=
			    sizeof(s->srv_conf.name)) {
				yyerror("server name truncated");
				free(s);
				YYERROR;
			}

			/* A location entry uses the parent id */
			s->srv_conf.id = srv->srv_conf.id;
			s->srv_conf.flags = SRVFLAG_LOCATION;
			s->srv_s = -1;
			memcpy(&s->srv_conf.ss, &srv->srv_conf.ss,
			    sizeof(s->srv_conf.ss));
			s->srv_conf.port = srv->srv_conf.port;
			s->srv_conf.prefixlen = srv->srv_conf.prefixlen;

			if (last_server_id == INT_MAX) {
				yyerror("too many servers/locations defined");
				free(s);
				YYERROR;
			}
			parentsrv = srv;
			srv = s;
			srv_conf = &srv->srv_conf;
			SPLAY_INIT(&srv->srv_clients);
		} '{' optnl serveropts_l '}'	{
			struct server	*s = NULL;

			TAILQ_FOREACH(s, conf->sc_servers, srv_entry) {
				if ((s->srv_conf.flags & SRVFLAG_LOCATION) &&
				    s->srv_conf.id == srv_conf->id &&
				    strcmp(s->srv_conf.location,
				    srv_conf->location) == 0)
					break;
			}
			if (s != NULL) {
				yyerror("location \"%s\" defined twice",
				    srv->srv_conf.location);
				serverconfig_free(srv_conf);
				free(srv);
				YYABORT;
			}

			DPRINTF("adding location \"%s\" for \"%s[%u]\"",
			    srv->srv_conf.location,
			    srv->srv_conf.name, srv->srv_conf.id);

			TAILQ_INSERT_TAIL(conf->sc_servers, srv, srv_entry);

			srv = parentsrv;
			srv_conf = &parentsrv->srv_conf;
			parentsrv = NULL;
		}
		| include
		;

fastcgi		: NO FCGI		{
			srv_conf->flags &= ~SRVFLAG_FCGI;
			srv_conf->flags |= SRVFLAG_NO_FCGI;
		}
		| FCGI			{
			srv_conf->flags &= ~SRVFLAG_NO_FCGI;
			srv_conf->flags |= SRVFLAG_FCGI;
		}
		| FCGI			{
			srv_conf->flags &= ~SRVFLAG_NO_FCGI;
			srv_conf->flags |= SRVFLAG_FCGI;
		} '{' optnl fcgiflags_l '}'
		| FCGI			{
			srv_conf->flags &= ~SRVFLAG_NO_FCGI;
			srv_conf->flags |= SRVFLAG_FCGI;
		} fcgiflags
		;

fcgiflags_l	: fcgiflags optcommanl fcgiflags_l
		| fcgiflags optnl
		;

fcgiflags	: SOCKET STRING		{
			if (strlcpy(srv_conf->socket, $2,
			    sizeof(srv_conf->socket)) >=
			    sizeof(srv_conf->socket)) {
				yyerror("fastcgi socket too long");
				free($2);
				YYERROR;
			}
			free($2);
			srv_conf->flags |= SRVFLAG_SOCKET;
		}
		;

connection	: CONNECTION '{' optnl conflags_l '}'
		| CONNECTION conflags
		;

conflags_l	: conflags optcommanl conflags_l
		| conflags optnl
		;

conflags	: TIMEOUT timeout		{
			memcpy(&srv_conf->timeout, &$2,
			    sizeof(struct timeval));
		}
		| MAXIMUM REQUESTS NUMBER	{
			srv_conf->maxrequests = $3;
		}
		| MAXIMUM REQUEST BODY NUMBER	{
			srv_conf->maxrequestbody = $4;
		}
		;

tls		: TLS '{' optnl tlsopts_l '}'
		| TLS tlsopts
		;

tlsopts_l	: tlsopts optcommanl tlsopts_l
		| tlsopts optnl
		;

tlsopts		: CERTIFICATE STRING		{
			free(srv_conf->tls_cert_file);
			if ((srv_conf->tls_cert_file = strdup($2)) == NULL)
				fatal("out of memory");
			free($2);
		}
		| KEY STRING			{
			free(srv_conf->tls_key_file);
			if ((srv_conf->tls_key_file = strdup($2)) == NULL)
				fatal("out of memory");
			free($2);
		}
		| CIPHERS STRING		{
			if (strlcpy(srv_conf->tls_ciphers, $2,
			    sizeof(srv_conf->tls_ciphers)) >=
			    sizeof(srv_conf->tls_ciphers)) {
				yyerror("ciphers too long");
				free($2);
				YYERROR;
			}
			free($2);
		}
		;

root		: ROOT rootflags
		| ROOT '{' optnl rootflags_l '}'
		;

rootflags_l	: rootflags optcommanl rootflags_l
		| rootflags optnl
		;

rootflags	: STRING		{
			if (strlcpy(srv->srv_conf.root, $1,
			    sizeof(srv->srv_conf.root)) >=
			    sizeof(srv->srv_conf.root)) {
				yyerror("document root too long");
				free($1);
				YYERROR;
			}
			free($1);
			srv->srv_conf.flags |= SRVFLAG_ROOT;
		}
		| STRIP NUMBER		{
			if ($2 < 0 || $2 > INT_MAX) {
				yyerror("invalid strip number");
				YYERROR;
			}
			srv->srv_conf.strip = $2;
		}
		;

directory	: DIRECTORY dirflags
		| DIRECTORY '{' optnl dirflags_l '}'
		;

dirflags_l	: dirflags optcommanl dirflags_l
		| dirflags optnl
		;

dirflags	: INDEX STRING		{
			if (strlcpy(srv_conf->index, $2,
			    sizeof(srv_conf->index)) >=
			    sizeof(srv_conf->index)) {
				yyerror("index file too long");
				free($2);
				YYERROR;
			}
			srv_conf->flags &= ~SRVFLAG_NO_INDEX;
			srv_conf->flags |= SRVFLAG_INDEX;
			free($2);
		}
		| NO INDEX		{
			srv_conf->flags &= ~SRVFLAG_INDEX;
			srv_conf->flags |= SRVFLAG_NO_INDEX;
		}
		| AUTO INDEX		{
			srv_conf->flags &= ~SRVFLAG_NO_AUTO_INDEX;
			srv_conf->flags |= SRVFLAG_AUTO_INDEX;
		}
		| NO AUTO INDEX		{
			srv_conf->flags &= ~SRVFLAG_AUTO_INDEX;
			srv_conf->flags |= SRVFLAG_NO_AUTO_INDEX;
		}
		;


logformat	: LOG logflags
		| LOG '{' optnl logflags_l '}'
		| NO LOG		{
			srv_conf->flags &= ~SRVFLAG_LOG;
			srv_conf->flags |= SRVFLAG_NO_LOG;
		}
		;

logflags_l	: logflags optcommanl logflags_l
		| logflags optnl
		;

logflags	: STYLE logstyle
		| SYSLOG		{
			srv_conf->flags &= ~SRVFLAG_NO_SYSLOG;
			srv_conf->flags |= SRVFLAG_SYSLOG;
		}
		| NO SYSLOG		{
			srv_conf->flags &= ~SRVFLAG_SYSLOG;
			srv_conf->flags |= SRVFLAG_NO_SYSLOG;
		}
		| ACCESS STRING		{
			if (strlcpy(srv_conf->accesslog, $2,
			    sizeof(srv_conf->accesslog)) >=
			    sizeof(srv_conf->accesslog)) {
				yyerror("access log name too long");
				free($2);
				YYERROR;
			}
			free($2);
			srv_conf->flags |= SRVFLAG_ACCESS_LOG;
		}
		| ERR STRING		{
			if (strlcpy(srv_conf->errorlog, $2,
			    sizeof(srv_conf->errorlog)) >=
			    sizeof(srv_conf->errorlog)) {
				yyerror("error log name too long");
				free($2);
				YYERROR;
			}
			free($2);
			srv_conf->flags |= SRVFLAG_ERROR_LOG;
		}
		;

logstyle	: COMMON		{
			srv_conf->flags &= ~SRVFLAG_NO_LOG;
			srv_conf->flags |= SRVFLAG_LOG;
			srv_conf->logformat = LOG_FORMAT_COMMON;
		}
		| COMBINED		{
			srv_conf->flags &= ~SRVFLAG_NO_LOG;
			srv_conf->flags |= SRVFLAG_LOG;
			srv_conf->logformat = LOG_FORMAT_COMBINED;
		}
		| CONNECTION		{
			srv_conf->flags &= ~SRVFLAG_NO_LOG;
			srv_conf->flags |= SRVFLAG_LOG;
			srv_conf->logformat = LOG_FORMAT_CONNECTION;
		}
		;

tcpip		: TCP '{' optnl tcpflags_l '}'
		| TCP tcpflags
		;

tcpflags_l	: tcpflags optcommanl tcpflags_l
		| tcpflags optnl
		;

tcpflags	: SACK			{ srv_conf->tcpflags |= TCPFLAG_SACK; }
		| NO SACK		{ srv_conf->tcpflags |= TCPFLAG_NSACK; }
		| NODELAY		{
			srv_conf->tcpflags |= TCPFLAG_NODELAY;
		}
		| NO NODELAY		{
			srv_conf->tcpflags |= TCPFLAG_NNODELAY;
		}
		| BACKLOG NUMBER	{
			if ($2 < 0 || $2 > SERVER_MAX_CLIENTS) {
				yyerror("invalid backlog: %lld", $2);
				YYERROR;
			}
			srv_conf->tcpbacklog = $2;
		}
		| SOCKET BUFFER NUMBER	{
			srv_conf->tcpflags |= TCPFLAG_BUFSIZ;
			if ((srv_conf->tcpbufsiz = $3) < 0) {
				yyerror("invalid socket buffer size: %lld", $3);
				YYERROR;
			}
		}
		| IP STRING NUMBER	{
			if ($3 < 0) {
				yyerror("invalid ttl: %lld", $3);
				free($2);
				YYERROR;
			}
			if (strcasecmp("ttl", $2) == 0) {
				srv_conf->tcpflags |= TCPFLAG_IPTTL;
				srv_conf->tcpipttl = $3;
			} else if (strcasecmp("minttl", $2) == 0) {
				srv_conf->tcpflags |= TCPFLAG_IPMINTTL;
				srv_conf->tcpipminttl = $3;
			} else {
				yyerror("invalid TCP/IP flag: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
		}
		;

types		: TYPES	'{' optnl mediaopts_l '}'
		;

mediaopts_l	: mediaopts_l mediaoptsl nl
		| mediaoptsl nl
		;

mediaoptsl	: STRING '/' STRING	{
			if (strlcpy(media.media_type, $1,
			    sizeof(media.media_type)) >=
			    sizeof(media.media_type) ||
			    strlcpy(media.media_subtype, $3,
			    sizeof(media.media_subtype)) >=
			    sizeof(media.media_subtype)) {
				yyerror("media type too long");
				free($1);
				free($3);
				YYERROR;
			}
			free($1);
			free($3);
		} medianames_l optsemicolon
		| include
		;

medianames_l	: medianames_l medianamesl
		| medianamesl
		;

medianamesl	: numberstring				{
			if (strlcpy(media.media_name, $1,
			    sizeof(media.media_name)) >=
			    sizeof(media.media_name)) {
				yyerror("media name too long");
				free($1);
				YYERROR;
			}
			free($1);

			if (!loadcfg)
				break;

			if (media_add(conf->sc_mediatypes, &media) == NULL) {
				yyerror("failed to add media type");
				YYERROR;
			}
		}
		;

port		: PORT STRING {
			char		*a, *b;
			int		 p[2];

			p[0] = p[1] = 0;

			a = $2;
			b = strchr($2, ':');
			if (b == NULL)
				$$.op = PF_OP_EQ;
			else {
				*b++ = '\0';
				if ((p[1] = getservice(b)) == -1) {
					free($2);
					YYERROR;
				}
				$$.op = PF_OP_RRG;
			}
			if ((p[0] = getservice(a)) == -1) {
				free($2);
				YYERROR;
			}
			$$.val[0] = p[0];
			$$.val[1] = p[1];
			free($2);
		}
		| PORT NUMBER {
			if ($2 <= 0 || $2 >= (int)USHRT_MAX) {
				yyerror("invalid port: %lld", $2);
				YYERROR;
			}
			$$.val[0] = htons($2);
			$$.op = PF_OP_EQ;
		}
		;

timeout		: NUMBER
		{
			if ($1 < 0) {
				yyerror("invalid timeout: %lld", $1);
				YYERROR;
			}
			$$.tv_sec = $1;
			$$.tv_usec = 0;
		}
		;

numberstring	: NUMBER		{
			char *s;
			if (asprintf(&s, "%lld", $1) == -1) {
				yyerror("asprintf: number");
				YYERROR;
			}
			$$ = s;
		}
		| STRING
		;

optsemicolon	: ';'
		|
		;

optnl		: '\n' optnl
		|
		;

optcommanl	: ',' optnl
		| nl
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
		{ "access",		ACCESS },
		{ "alias",		ALIAS },
		{ "auto",		AUTO },
		{ "backlog",		BACKLOG },
		{ "body",		BODY },
		{ "buffer",		BUFFER },
		{ "certificate",	CERTIFICATE },
		{ "chroot",		CHROOT },
		{ "ciphers",		CIPHERS },
		{ "combined",		COMBINED },
		{ "common",		COMMON },
		{ "connection",		CONNECTION },
		{ "directory",		DIRECTORY },
		{ "error",		ERR },
		{ "fastcgi",		FCGI },
		{ "include",		INCLUDE },
		{ "index",		INDEX },
		{ "ip",			IP },
		{ "key",		KEY },
		{ "listen",		LISTEN },
		{ "location",		LOCATION },
		{ "log",		LOG },
		{ "logdir",		LOGDIR },
		{ "max",		MAXIMUM },
		{ "no",			NO },
		{ "nodelay",		NODELAY },
		{ "on",			ON },
		{ "port",		PORT },
		{ "prefork",		PREFORK },
		{ "request",		REQUEST },
		{ "requests",		REQUESTS },
		{ "root",		ROOT },
		{ "sack",		SACK },
		{ "server",		SERVER },
		{ "socket",		SOCKET },
		{ "strip",		STRIP },
		{ "style",		STYLE },
		{ "syslog",		SYSLOG },
		{ "tcp",		TCP },
		{ "timeout",		TIMEOUT },
		{ "tls",		TLS },
		{ "types",		TYPES }
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
	x != '{' && x != '}' && x != '<' && x != '>' && \
	x != '!' && x != '=' && x != '#' && \
	x != ',' && x != ';' && x != '/'))

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
		log_warn("%s: malloc", __func__);
		return (NULL);
	}
	if ((nfile->name = strdup(name)) == NULL) {
		log_warn("%s: malloc", __func__);
		free(nfile);
		return (NULL);
	}
	if ((nfile->stream = fopen(nfile->name, "r")) == NULL) {
		log_warn("%s: %s", __func__, nfile->name);
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

int
parse_config(const char *filename, struct httpd *x_conf)
{
	struct sym	*sym, *next;

	conf = x_conf;
	if (config_init(conf) == -1) {
		log_warn("%s: cannot initialize configuration", __func__);
		return (-1);
	}

	errors = 0;

	if ((file = pushfile(filename, 0)) == NULL)
		return (-1);

	topfile = file;
	setservent(1);

	yyparse();
	errors = file->errors;
	popfile();

	endservent();
	endprotoent();

	/* Free macros */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entry);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
			free(sym);
		}
	}

	return (errors ? -1 : 0);
}

int
load_config(const char *filename, struct httpd *x_conf)
{
	struct sym		*sym, *next;
	struct http_mediatype	 mediatypes[] = MEDIA_TYPES;
	struct media_type	 m;
	int			 i;

	conf = x_conf;
	conf->sc_flags = 0;

	loadcfg = 1;
	errors = 0;
	last_server_id = 0;

	srv = NULL;

	if ((file = pushfile(filename, 0)) == NULL)
		return (-1);

	topfile = file;
	setservent(1);

	yyparse();
	errors = file->errors;
	popfile();

	endservent();
	endprotoent();

	/* Free macros and check which have not been used. */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entry);
		if ((conf->sc_opts & HTTPD_OPT_VERBOSE) && !sym->used)
			fprintf(stderr, "warning: macro '%s' not "
			    "used\n", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
			free(sym);
		}
	}

	if (TAILQ_EMPTY(conf->sc_servers)) {
		log_warnx("no actions, nothing to do");
		errors++;
	}

	if (RB_EMPTY(conf->sc_mediatypes)) {
		/* Add default media types */
		for (i = 0; mediatypes[i].media_name != NULL; i++) {
			(void)strlcpy(m.media_name, mediatypes[i].media_name,
			    sizeof(m.media_name));
			(void)strlcpy(m.media_type, mediatypes[i].media_type,
			    sizeof(m.media_type));
			(void)strlcpy(m.media_subtype,
			    mediatypes[i].media_subtype,
			    sizeof(m.media_subtype));
			m.media_encoding = NULL;

			if (media_add(conf->sc_mediatypes, &m) == NULL) {
				log_warnx("failed to add default media \"%s\"",
				    m.media_name);
				errors++;
			}
		}
	}

	return (errors ? -1 : 0);
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

	(void)strlcpy(sym, s, len);

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

struct address *
host_v4(const char *s)
{
	struct in_addr		 ina;
	struct sockaddr_in	*sain;
	struct address		*h;

	memset(&ina, 0, sizeof(ina));
	if (inet_pton(AF_INET, s, &ina) != 1)
		return (NULL);

	if ((h = calloc(1, sizeof(*h))) == NULL)
		fatal(NULL);
	sain = (struct sockaddr_in *)&h->ss;
	sain->sin_len = sizeof(struct sockaddr_in);
	sain->sin_family = AF_INET;
	sain->sin_addr.s_addr = ina.s_addr;
	if (sain->sin_addr.s_addr == INADDR_ANY)
		h->prefixlen = 0; /* 0.0.0.0 address */
	else
		h->prefixlen = -1; /* host address */
	return (h);
}

struct address *
host_v6(const char *s)
{
	struct addrinfo		 hints, *res;
	struct sockaddr_in6	*sa_in6;
	struct address		*h = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM; /* dummy */
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(s, "0", &hints, &res) == 0) {
		if ((h = calloc(1, sizeof(*h))) == NULL)
			fatal(NULL);
		sa_in6 = (struct sockaddr_in6 *)&h->ss;
		sa_in6->sin6_len = sizeof(struct sockaddr_in6);
		sa_in6->sin6_family = AF_INET6;
		memcpy(&sa_in6->sin6_addr,
		    &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr,
		    sizeof(sa_in6->sin6_addr));
		sa_in6->sin6_scope_id =
		    ((struct sockaddr_in6 *)res->ai_addr)->sin6_scope_id;
		if (memcmp(&sa_in6->sin6_addr, &in6addr_any,
		    sizeof(sa_in6->sin6_addr)) == 0)
			h->prefixlen = 0; /* any address */
		else
			h->prefixlen = -1; /* host address */
		freeaddrinfo(res);
	}

	return (h);
}

int
host_dns(const char *s, struct addresslist *al, int max,
    struct portrange *port, const char *ifname, int ipproto)
{
	struct addrinfo		 hints, *res0, *res;
	int			 error, cnt = 0;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct address		*h;

	if ((cnt = host_if(s, al, max, port, ifname, ipproto)) != 0)
		return (cnt);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM; /* DUMMY */
	error = getaddrinfo(s, NULL, &hints, &res0);
	if (error == EAI_AGAIN || error == EAI_NODATA || error == EAI_NONAME)
		return (0);
	if (error) {
		log_warnx("%s: could not parse \"%s\": %s", __func__, s,
		    gai_strerror(error));
		return (-1);
	}

	for (res = res0; res && cnt < max; res = res->ai_next) {
		if (res->ai_family != AF_INET &&
		    res->ai_family != AF_INET6)
			continue;
		if ((h = calloc(1, sizeof(*h))) == NULL)
			fatal(NULL);

		if (port != NULL)
			memcpy(&h->port, port, sizeof(h->port));
		if (ifname != NULL) {
			if (strlcpy(h->ifname, ifname, sizeof(h->ifname)) >=
			    sizeof(h->ifname))
				log_warnx("%s: interface name truncated",
				    __func__);
			freeaddrinfo(res0);
			free(h);
			return (-1);
		}
		if (ipproto != -1)
			h->ipproto = ipproto;
		h->ss.ss_family = res->ai_family;
		h->prefixlen = -1; /* host address */

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
		log_warnx("%s: %s resolves to more than %d hosts", __func__,
		    s, max);
	}
	freeaddrinfo(res0);
	return (cnt);
}

int
host_if(const char *s, struct addresslist *al, int max,
    struct portrange *port, const char *ifname, int ipproto)
{
	struct ifaddrs		*ifap, *p;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct address		*h;
	int			 cnt = 0, af;

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	/* First search for IPv4 addresses */
	af = AF_INET;

 nextaf:
	for (p = ifap; p != NULL && cnt < max; p = p->ifa_next) {
		if (p->ifa_addr->sa_family != af ||
		    (strcmp(s, p->ifa_name) != 0 &&
		    !is_if_in_group(p->ifa_name, s)))
			continue;
		if ((h = calloc(1, sizeof(*h))) == NULL)
			fatal("calloc");

		if (port != NULL)
			memcpy(&h->port, port, sizeof(h->port));
		if (ifname != NULL) {
			if (strlcpy(h->ifname, ifname, sizeof(h->ifname)) >=
			    sizeof(h->ifname))
				log_warnx("%s: interface name truncated",
				    __func__);
			freeifaddrs(ifap);
			return (-1);
		}
		if (ipproto != -1)
			h->ipproto = ipproto;
		h->ss.ss_family = af;
		h->prefixlen = -1; /* host address */

		if (af == AF_INET) {
			sain = (struct sockaddr_in *)&h->ss;
			sain->sin_len = sizeof(struct sockaddr_in);
			sain->sin_addr.s_addr = ((struct sockaddr_in *)
			    p->ifa_addr)->sin_addr.s_addr;
		} else {
			sin6 = (struct sockaddr_in6 *)&h->ss;
			sin6->sin6_len = sizeof(struct sockaddr_in6);
			memcpy(&sin6->sin6_addr, &((struct sockaddr_in6 *)
			    p->ifa_addr)->sin6_addr, sizeof(struct in6_addr));
			sin6->sin6_scope_id = ((struct sockaddr_in6 *)
			    p->ifa_addr)->sin6_scope_id;
		}

		TAILQ_INSERT_HEAD(al, h, entry);
		cnt++;
	}
	if (af == AF_INET) {
		/* Next search for IPv6 addresses */
		af = AF_INET6;
		goto nextaf;
	}

	if (cnt > max) {
		log_warnx("%s: %s resolves to more than %d hosts", __func__,
		    s, max);
	}
	freeifaddrs(ifap);
	return (cnt);
}

int
host(const char *s, struct addresslist *al, int max,
    struct portrange *port, const char *ifname, int ipproto)
{
	struct address *h;

	if (strcmp("*", s) == 0)
		s = "0.0.0.0";

	h = host_v4(s);

	/* IPv6 address? */
	if (h == NULL)
		h = host_v6(s);

	if (h != NULL) {
		if (port != NULL)
			memcpy(&h->port, port, sizeof(h->port));
		if (ifname != NULL) {
			if (strlcpy(h->ifname, ifname, sizeof(h->ifname)) >=
			    sizeof(h->ifname)) {
				log_warnx("%s: interface name truncated",
				    __func__);
				free(h);
				return (-1);
			}
		}
		if (ipproto != -1)
			h->ipproto = ipproto;

		TAILQ_INSERT_HEAD(al, h, entry);
		return (1);
	}

	return (host_dns(s, al, max, port, ifname, ipproto));
}

void
host_free(struct addresslist *al)
{
	struct address	 *h;

	while ((h = TAILQ_FIRST(al)) != NULL) {
		TAILQ_REMOVE(al, h, entry);
		free(h);
	}
}

struct server *
server_inherit(struct server *src, const char *name,
    struct server_config *addr)
{
	struct server	*dst, *s, *dstl;

	if ((dst = calloc(1, sizeof(*dst))) == NULL)
		fatal("out of memory");

	/* Copy the source server and assign a new Id */
	memcpy(&dst->srv_conf, &src->srv_conf, sizeof(dst->srv_conf));
	if ((dst->srv_conf.tls_cert_file =
	    strdup(src->srv_conf.tls_cert_file)) == NULL)
		fatal("out of memory");
	if ((dst->srv_conf.tls_key_file =
	    strdup(src->srv_conf.tls_key_file)) == NULL)
		fatal("out of memory");
	dst->srv_conf.tls_cert = NULL;
	dst->srv_conf.tls_key = NULL;

	dst->srv_conf.id = ++last_server_id;
	dst->srv_s = -1;

	if (last_server_id == INT_MAX) {
		yyerror("too many servers defined");
		serverconfig_free(&dst->srv_conf);
		free(dst);
		return (NULL);
	}

	/* Now set alias and listen address */
	strlcpy(dst->srv_conf.name, name, sizeof(dst->srv_conf.name));
	memcpy(&dst->srv_conf.ss, &addr->ss, sizeof(dst->srv_conf.ss));
	dst->srv_conf.port = addr->port;
	dst->srv_conf.prefixlen = addr->prefixlen;
	if (addr->flags & SRVFLAG_TLS)
		dst->srv_conf.flags |= SRVFLAG_TLS;
	else
		dst->srv_conf.flags &= ~SRVFLAG_TLS;

	if (server_tls_load_keypair(dst) == -1) {
		yyerror("failed to load public/private keys "
		    "for server %s", dst->srv_conf.name);
		serverconfig_free(&dst->srv_conf);
		free(dst);
		return (NULL);
	}

	/* Check if the new server already exists */
	TAILQ_FOREACH(s, conf->sc_servers, srv_entry) {
		if ((s->srv_conf.flags &
		    SRVFLAG_LOCATION) == 0 &&
		    strcmp(s->srv_conf.name,
		    dst->srv_conf.name) == 0 &&
		    s->srv_conf.port == dst->srv_conf.port &&
		    sockaddr_cmp(
		    (struct sockaddr *)&s->srv_conf.ss,
		    (struct sockaddr *)&dst->srv_conf.ss,
		    s->srv_conf.prefixlen) == 0)
			break;
	}
	if (s != NULL) {
		yyerror("server \"%s\" defined twice",
		    dst->srv_conf.name);
		serverconfig_free(&dst->srv_conf);
		free(dst);
		return (NULL);
	}

	/* Copy all the locations of the source server */
	TAILQ_FOREACH(s, conf->sc_servers, srv_entry) {
		if (!(s->srv_conf.flags & SRVFLAG_LOCATION &&
		    s->srv_conf.id == src->srv_conf.id))
			continue;

		if ((dstl = calloc(1, sizeof(*dstl))) == NULL)
			fatal("out of memory");

		memcpy(&dstl->srv_conf, &s->srv_conf, sizeof(dstl->srv_conf));
		strlcpy(dstl->srv_conf.name, name, sizeof(dstl->srv_conf.name));

		/* Copy the new Id and listen address */
		dstl->srv_conf.id = dst->srv_conf.id;
		memcpy(&dstl->srv_conf.ss, &addr->ss,
		    sizeof(dstl->srv_conf.ss));
		dstl->srv_conf.port = addr->port;
		dstl->srv_conf.prefixlen = addr->prefixlen;
		dstl->srv_s = -1;

		DPRINTF("adding location \"%s\" for \"%s[%u]\"",
		    dstl->srv_conf.location,
		    dstl->srv_conf.name, dstl->srv_conf.id);

		TAILQ_INSERT_TAIL(conf->sc_servers, dstl, srv_entry);
	}

	return (dst);
}

int
getservice(char *n)
{
	struct servent	*s;
	const char	*errstr;
	long long	 llval;

	llval = strtonum(n, 0, UINT16_MAX, &errstr);
	if (errstr) {
		s = getservbyname(n, "tcp");
		if (s == NULL)
			s = getservbyname(n, "udp");
		if (s == NULL) {
			yyerror("unknown port %s", n);
			return (-1);
		}
		return (s->s_port);
	}

	return (htons((u_short)llval));
}

int
is_if_in_group(const char *ifname, const char *groupname)
{
	unsigned int		 len;
	struct ifgroupreq	 ifgr;
	struct ifg_req		*ifg;
	int			 s;
	int			 ret = 0;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");

	memset(&ifgr, 0, sizeof(ifgr));
	if (strlcpy(ifgr.ifgr_name, ifname, IFNAMSIZ) >= IFNAMSIZ)
		err(1, "IFNAMSIZ");
	if (ioctl(s, SIOCGIFGROUP, (caddr_t)&ifgr) == -1) {
		if (errno == EINVAL || errno == ENOTTY)
			goto end;
		err(1, "SIOCGIFGROUP");
	}

	len = ifgr.ifgr_len;
	ifgr.ifgr_groups =
	    (struct ifg_req *)calloc(len / sizeof(struct ifg_req),
		sizeof(struct ifg_req));
	if (ifgr.ifgr_groups == NULL)
		err(1, "getifgroups");
	if (ioctl(s, SIOCGIFGROUP, (caddr_t)&ifgr) == -1)
		err(1, "SIOCGIFGROUP");

	ifg = ifgr.ifgr_groups;
	for (; ifg && len >= sizeof(struct ifg_req); ifg++) {
		len -= sizeof(struct ifg_req);
		if (strcmp(ifg->ifgrq_group, groupname) == 0) {
			ret = 1;
			break;
		}
	}
	free(ifgr.ifgr_groups);

end:
	close(s);
	return (ret);
}

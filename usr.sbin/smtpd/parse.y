/*	$OpenBSD: parse.y,v 1.59 2010/05/27 15:36:04 gilles Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
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
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <ifaddrs.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "smtpd.h"

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
int		 kw_cmp(const void *, const void *);
int		 lookup(char *);
int		 lgetc(int);
int		 lungetc(int);
int		 findeol(void);
int		 yyerror(const char *, ...)
    __attribute__ ((format (printf, 1, 2)));

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

struct smtpd		*conf = NULL;
static int		 errors = 0;

objid_t			 last_map_id = 1;
struct map		*map = NULL;
struct rule		*rule = NULL;
TAILQ_HEAD(condlist, cond) *conditions = NULL;
struct mapel_list	*contents = NULL;

struct listener	*host_v4(const char *, in_port_t);
struct listener	*host_v6(const char *, in_port_t);
int		 host_dns(const char *, const char *, const char *,
		    struct listenerlist *, int, in_port_t, u_int8_t);
int		 host(const char *, const char *, const char *,
		    struct listenerlist *, int, in_port_t, u_int8_t);
int		 interface(const char *, const char *, const char *,
		    struct listenerlist *, int, in_port_t, u_int8_t);
void		 set_localaddrs(void);

typedef struct {
	union {
		int64_t		 number;
		objid_t		 object;
		struct timeval	 tv;
		struct cond	*cond;
		char		*string;
		struct host	*host;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	QUEUE INTERVAL SIZE LISTEN ON ALL PORT
%token	MAP TYPE HASH LIST SINGLE SSL SMTPS CERTIFICATE
%token	DNS DB PLAIN EXTERNAL DOMAIN CONFIG SOURCE
%token  RELAY VIA DELIVER TO MAILDIR MBOX HOSTNAME
%token	ACCEPT REJECT INCLUDE NETWORK ERROR MDA FROM FOR
%token	ARROW ENABLE AUTH TLS LOCAL VIRTUAL USER TAG ALIAS
%token	<v.string>	STRING
%token  <v.number>	NUMBER
%type	<v.map>		map
%type	<v.number>	quantifier decision port from auth ssl size
%type	<v.cond>	condition
%type	<v.tv>		interval
%type	<v.object>	mapref
%type	<v.string>	certname user tag on alias

%%

grammar		: /* empty */
		| grammar '\n'
		| grammar include '\n'
		| grammar varset '\n'
		| grammar main '\n'
		| grammar map '\n'
		| grammar rule '\n'
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

varset		: STRING '=' STRING		{
			if (symset($1, $3, 0) == -1)
				fatal("cannot store variable");
			free($1);
			free($3);
		}
		;

comma		: ','
		| nl
		| /* empty */
		;

optnl		: '\n' optnl
		|
		;

nl		: '\n' optnl
		;

quantifier	: /* empty */			{ $$ = 1; }
		| 'm'				{ $$ = 60; }
		| 'h'				{ $$ = 3600; }
		| 'd'				{ $$ = 86400; }
		;

interval	: NUMBER quantifier		{
			if ($1 < 0) {
				yyerror("invalid interval: %lld", $1);
				YYERROR;
			}
			$$.tv_usec = 0;
			$$.tv_sec = $1 * $2;
		}

size		: NUMBER			{
			if ($1 < 0) {
				yyerror("invalid size: %lld", $1);
				YYERROR;
			}
			$$ = $1;
		}
		| STRING			{
			long long result;

			if (scan_scaled($1, &result) == -1 || result < 0) {
				yyerror("invalid size: %s", $1);
				YYERROR;
			}
			free($1);

			$$ = result;
		}
		;

port		: PORT STRING			{
			struct servent	*servent;

			servent = getservbyname($2, "tcp");
			if (servent == NULL) {
				yyerror("port %s is invalid", $2);
				free($2);
				YYERROR;
			}
			$$ = servent->s_port;
			free($2);
		}
		| PORT NUMBER			{
			if ($2 <= 0 || $2 >= (int)USHRT_MAX) {
				yyerror("invalid port: %lld", $2);
				YYERROR;
			}
			$$ = htons($2);
		}
		| /* empty */			{
			$$ = 0;
		}
		;

certname	: CERTIFICATE STRING	{
			if (($$ = strdup($2)) == NULL)
				fatal(NULL);
			free($2);
		}
		| /* empty */			{ $$ = NULL; }
		;

ssl		: SMTPS				{ $$ = F_SMTPS; }
		| TLS				{ $$ = F_STARTTLS; }
		| SSL				{ $$ = F_SSL; }
		| /* empty */			{ $$ = 0; }

auth		: ENABLE AUTH  			{ $$ = 1; }
		| /* empty */			{ $$ = 0; }
		;

tag		: TAG STRING			{
       			if (strlen($2) >= MAX_TAG_SIZE) {
       				yyerror("tag name too long");
				free($2);
				YYERROR;
			}

			$$ = $2;
		}
		| /* empty */			{ $$ = NULL; }
		;

main		: QUEUE INTERVAL interval	{
			conf->sc_qintval = $3;
		}
	       	| SIZE size {
       			conf->sc_maxsize = $2;
		}
		| LISTEN ON STRING port ssl certname auth tag {
			char		*cert;
			char		*tag;
			u_int8_t	 flags;

			if ($5 == F_SSL) {
				yyerror("syntax error");
				free($8);
				free($6);
				free($3);
				YYERROR;
			}

			if ($5 == 0 && ($6 != NULL || $7)) {
				yyerror("error: must specify tls or smtps");
				free($8);
				free($6);
				free($3);
				YYERROR;
			}

			if ($4 == 0) {
				if ($5 == F_SMTPS)
					$4 = htons(465);
				else
					$4 = htons(25);
			}

			cert = ($6 != NULL) ? $6 : $3;
			flags = $5;

			if ($7)
				flags |= F_AUTH;

			if ($5 && ssl_load_certfile(conf, cert, F_SCERT) < 0) {
				yyerror("cannot load certificate: %s", cert);
				free($8);
				free($6);
				free($3);
				YYERROR;
			}

			tag = $3;
			if ($8 != NULL)
				tag = $8;

			if (! interface($3, tag, cert, conf->sc_listeners,
				MAX_LISTEN, $4, flags)) {
				if (host($3, tag, cert, conf->sc_listeners,
					MAX_LISTEN, $4, flags) <= 0) {
					yyerror("invalid virtual ip or interface: %s", $3);
					free($8);
					free($6);
					free($3);
					YYERROR;
				}
			}
			free($8);
			free($6);
			free($3);
		}
		| HOSTNAME STRING		{
			if (strlcpy(conf->sc_hostname, $2,
			    sizeof(conf->sc_hostname)) >=
			    sizeof(conf->sc_hostname)) {
				yyerror("hostname truncated");
				free($2);
				YYERROR;
			}
			free($2);
		}
		;

maptype		: SINGLE			{ map->m_type = T_SINGLE; }
		| LIST				{ map->m_type = T_LIST; }
		| HASH				{ map->m_type = T_HASH; }
		;

mapsource	: DNS				{ map->m_src = S_DNS; }
		| PLAIN STRING			{
			map->m_src = S_PLAIN;
			if (strlcpy(map->m_config, $2, sizeof(map->m_config))
			    >= sizeof(map->m_config))
				err(1, "pathname too long");
		}
		| DB STRING			{
			map->m_src = S_DB;
			if (strlcpy(map->m_config, $2, sizeof(map->m_config))
			    >= sizeof(map->m_config))
				err(1, "pathname too long");
		}
		| EXTERNAL			{ map->m_src = S_EXT; }
		;

mapopt		: TYPE maptype
		| SOURCE mapsource
		| CONFIG STRING			{
		}
		;

mapopts_l	: mapopts_l mapopt nl
		| mapopt optnl
		;

map		: MAP STRING			{
			struct map	*m;

			TAILQ_FOREACH(m, conf->sc_maps, m_entry)
				if (strcmp(m->m_name, $2) == 0)
					break;

			if (m != NULL) {
				yyerror("map %s defined twice", $2);
				free($2);
				YYERROR;
			}
			if ((m = calloc(1, sizeof(*m))) == NULL)
				fatal("out of memory");
			if (strlcpy(m->m_name, $2, sizeof(m->m_name)) >=
			    sizeof(m->m_name)) {
				yyerror("map name truncated");
				free(m);
				free($2);
				YYERROR;
			}

			m->m_id = last_map_id++;
			m->m_type = T_SINGLE;

			if (m->m_id == INT_MAX) {
				yyerror("too many maps defined");
				free($2);
				free(m);
				YYERROR;
			}
			map = m;
		} '{' optnl mapopts_l '}'	{
			if (map->m_src == S_NONE) {
				yyerror("map %s has no source defined", $2);
				free(map);
				map = NULL;
				YYERROR;
			}
			TAILQ_INSERT_TAIL(conf->sc_maps, map, m_entry);
			map = NULL;
		}
		;

keyval		: STRING ARROW STRING		{
			struct mapel	*me;

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");

			if (strlcpy(me->me_key.med_string, $1,
			    sizeof(me->me_key.med_string)) >=
			    sizeof(me->me_key.med_string) ||
			    strlcpy(me->me_val.med_string, $3,
			    sizeof(me->me_val.med_string)) >=
			    sizeof(me->me_val.med_string)) {
				yyerror("map elements too long: %s, %s",
				    $1, $3);
				free(me);
				free($1);
				free($3);
				YYERROR;
			}
			free($1);
			free($3);

			TAILQ_INSERT_TAIL(contents, me, me_entry);
		}

keyval_list	: keyval
		| keyval comma keyval_list
		;

stringel	: STRING			{
			struct mapel	*me;
			int bits;
			struct sockaddr_in ssin;
			struct sockaddr_in6 ssin6;

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");

			/* Attempt detection of $1 format */
			if (strchr($1, '/') != NULL) {
				/* Dealing with a netmask */
				bzero(&ssin, sizeof(struct sockaddr_in));
				bits = inet_net_pton(AF_INET, $1, &ssin.sin_addr, sizeof(struct in_addr));
				if (bits != -1) {
					ssin.sin_family = AF_INET;
					me->me_key.med_addr.bits = bits;
					memcpy(&me->me_key.med_addr.ss, &ssin, sizeof(ssin));
					me->me_key.med_addr.ss.ss_len = sizeof(struct sockaddr_in);
				}
				else {
					bzero(&ssin6, sizeof(struct sockaddr_in6));
					bits = inet_net_pton(AF_INET6, $1, &ssin6.sin6_addr, sizeof(struct in6_addr));
					if (bits == -1)
						err(1, "inet_net_pton");
					ssin6.sin6_family = AF_INET6;
					me->me_key.med_addr.bits = bits;
					memcpy(&me->me_key.med_addr.ss, &ssin6, sizeof(ssin6));
					me->me_key.med_addr.ss.ss_len = sizeof(struct sockaddr_in6);
				}
			}
			else {
				/* IP address ? */
				if (inet_pton(AF_INET, $1, &ssin.sin_addr) == 1) {
					ssin.sin_family = AF_INET;
					me->me_key.med_addr.bits = 32;
					memcpy(&me->me_key.med_addr.ss, &ssin, sizeof(ssin));
					me->me_key.med_addr.ss.ss_len = sizeof(struct sockaddr_in);
				}
				else if (inet_pton(AF_INET6, $1, &ssin6.sin6_addr) == 1) {
					ssin6.sin6_family = AF_INET6;
					me->me_key.med_addr.bits = 128;
					memcpy(&me->me_key.med_addr.ss, &ssin6, sizeof(ssin6));
					me->me_key.med_addr.ss.ss_len = sizeof(struct sockaddr_in6);
				}
				else {
					/* either a hostname or a value unrelated to network */
					if (strlcpy(me->me_key.med_string, $1,
						sizeof(me->me_key.med_string)) >=
					    sizeof(me->me_key.med_string)) {
						yyerror("map element too long: %s", $1);
						free(me);
						free($1);
						YYERROR;
					}
				}
			}
			free($1);
			TAILQ_INSERT_TAIL(contents, me, me_entry);
		}
		;

string_list	: stringel
		| stringel comma string_list
		;

mapref		: STRING			{
			struct map	*m;
			struct mapel	*me;
			int bits;
			struct sockaddr_in ssin;
			struct sockaddr_in6 ssin6;

			if ((m = calloc(1, sizeof(*m))) == NULL)
				fatal("out of memory");
			m->m_id = last_map_id++;
			if (m->m_id == INT_MAX) {
				yyerror("too many maps defined");
				free(m);
				YYERROR;
			}
			if (! bsnprintf(m->m_name, sizeof(m->m_name),
				"<dynamic(%u)>", m->m_id))
				fatal("snprintf");
			m->m_flags |= F_DYNAMIC|F_USED;
			m->m_type = T_SINGLE;
			m->m_src = S_NONE;

			TAILQ_INIT(&m->m_contents);

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");

			/* Attempt detection of $1 format */
			if (strchr($1, '/') != NULL) {
				/* Dealing with a netmask */
				bzero(&ssin, sizeof(struct sockaddr_in));
				bits = inet_net_pton(AF_INET, $1, &ssin.sin_addr, sizeof(struct in_addr));
				if (bits != -1) {
					ssin.sin_family = AF_INET;
					me->me_key.med_addr.bits = bits;
					memcpy(&me->me_key.med_addr.ss, &ssin, sizeof(ssin));
					me->me_key.med_addr.ss.ss_len = sizeof(struct sockaddr_in);
				}
				else {
					bzero(&ssin6, sizeof(struct sockaddr_in6));
					bits = inet_net_pton(AF_INET6, $1, &ssin6.sin6_addr, sizeof(struct in6_addr));
					if (bits == -1)
						err(1, "inet_net_pton");
					ssin6.sin6_family = AF_INET6;
					me->me_key.med_addr.bits = bits;
					memcpy(&me->me_key.med_addr.ss, &ssin6, sizeof(ssin6));
					me->me_key.med_addr.ss.ss_len = sizeof(struct sockaddr_in6);
				}
			}
			else {
				/* IP address ? */
				if (inet_pton(AF_INET, $1, &ssin.sin_addr) == 1) {
					ssin.sin_family = AF_INET;
					me->me_key.med_addr.bits = 32;
					memcpy(&me->me_key.med_addr.ss, &ssin, sizeof(ssin));
					me->me_key.med_addr.ss.ss_len = sizeof(struct sockaddr_in);
				}
				else if (inet_pton(AF_INET6, $1, &ssin6.sin6_addr) == 1) {
					ssin6.sin6_family = AF_INET6;
					me->me_key.med_addr.bits = 128;
					memcpy(&me->me_key.med_addr.ss, &ssin6, sizeof(ssin6));
					me->me_key.med_addr.ss.ss_len = sizeof(struct sockaddr_in6);
				}
				else {
					/* either a hostname or a value unrelated to network */
					if (strlcpy(me->me_key.med_string, $1,
						sizeof(me->me_key.med_string)) >=
					    sizeof(me->me_key.med_string)) {
						yyerror("map element too long: %s", $1);
						free(me);
						free(m);
						free($1);
						YYERROR;
					}
				}
			}
			free($1);

			TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);
			TAILQ_INSERT_TAIL(conf->sc_maps, m, m_entry);
			$$ = m->m_id;
		}
		| '('				{
			struct map	*m;

			if ((m = calloc(1, sizeof(*m))) == NULL)
				fatal("out of memory");

			m->m_id = last_map_id++;
			if (m->m_id == INT_MAX) {
				yyerror("too many maps defined");
				free(m);
				YYERROR;
			}
			if (! bsnprintf(m->m_name, sizeof(m->m_name),
				"<dynamic(%u)>", m->m_id))
				fatal("snprintf");
			m->m_flags |= F_DYNAMIC|F_USED;
			m->m_type = T_LIST;

			TAILQ_INIT(&m->m_contents);
			contents = &m->m_contents;
			map = m;

		} string_list ')'		{
			TAILQ_INSERT_TAIL(conf->sc_maps, map, m_entry);
			$$ = map->m_id;
		}
		| '{'				{
			struct map	*m;

			if ((m = calloc(1, sizeof(*m))) == NULL)
				fatal("out of memory");

			m->m_id = last_map_id++;
			if (m->m_id == INT_MAX) {
				yyerror("too many maps defined");
				free(m);
				YYERROR;
			}
			if (! bsnprintf(m->m_name, sizeof(m->m_name),
				"<dynamic(%u)>", m->m_id))
				fatal("snprintf");
			m->m_flags |= F_DYNAMIC|F_USED;
			m->m_type = T_HASH;

			TAILQ_INIT(&m->m_contents);
			contents = &m->m_contents;
			map = m;

		} keyval_list '}'		{
			TAILQ_INSERT_TAIL(conf->sc_maps, map, m_entry);
			$$ = map->m_id;
		}
		| MAP STRING			{
			struct map	*m;

			if ((m = map_findbyname(conf, $2)) == NULL) {
				yyerror("no such map: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
			m->m_flags |= F_USED;
			$$ = m->m_id;
		}
		;

decision	: ACCEPT			{ $$ = 1; }
		| REJECT			{ $$ = 0; }
		;

alias		: ALIAS STRING			{ $$ = $2; }
		| /* empty */			{ $$ = NULL; }
		;

condition	: NETWORK mapref		{
			struct cond	*c;

			if ((c = calloc(1, sizeof *c)) == NULL)
				fatal("out of memory");
			c->c_type = C_NET;
			c->c_map = $2;
			$$ = c;
		}
		| DOMAIN mapref	alias		{
			struct cond	*c;
			struct map	*m;

			if ($3) {
				if ((m = map_findbyname(conf, $3)) == NULL) {
					yyerror("no such map: %s", $3);
					free($3);
					YYERROR;
				}
				rule->r_amap = m->m_id;
			}

			if ((c = calloc(1, sizeof *c)) == NULL)
				fatal("out of memory");
			c->c_type = C_DOM;
			c->c_map = $2;
			$$ = c;
		}
		| VIRTUAL STRING		{
			struct cond	*c;
			struct map	*m;

			if ((m = map_findbyname(conf, $2)) == NULL) {
				yyerror("no such map: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
			m->m_flags |= F_USED;

			if ((c = calloc(1, sizeof *c)) == NULL)
				fatal("out of memory");
			c->c_type = C_VDOM;
			c->c_map = m->m_id;
			$$ = c;
		}
		| LOCAL alias {
			struct cond	*c;
			struct map	*m;
			struct mapel	*me;

			if ($2) {
				if ((m = map_findbyname(conf, $2)) == NULL) {
					yyerror("no such map: %s", $2);
					free($2);
					YYERROR;
				}
				rule->r_amap = m->m_id;
			}

			if ((m = calloc(1, sizeof(*m))) == NULL)
				fatal("out of memory");
			m->m_id = last_map_id++;
			if (m->m_id == INT_MAX) {
				yyerror("too many maps defined");
				free(m);
				YYERROR;
			}
			if (! bsnprintf(m->m_name, sizeof(m->m_name),
				"<dynamic(%u)>", m->m_id))
				fatal("snprintf");
			m->m_flags |= F_DYNAMIC|F_USED;
			m->m_type = T_SINGLE;

			TAILQ_INIT(&m->m_contents);

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");

			(void)strlcpy(me->me_key.med_string, "localhost",
			    sizeof(me->me_key.med_string));
			TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");

			if (gethostname(me->me_key.med_string,
				sizeof(me->me_key.med_string)) == -1) {
				yyerror("gethostname() failed");
				free(me);
				free(m);
				YYERROR;
			}
			TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);

			TAILQ_INSERT_TAIL(conf->sc_maps, m, m_entry);

			if ((c = calloc(1, sizeof *c)) == NULL)
				fatal("out of memory");
			c->c_type = C_DOM;
			c->c_map = m->m_id;

			$$ = c;
		}
		| ALL				{
			struct cond	*c;

			if ((c = calloc(1, sizeof *c)) == NULL)
				fatal("out of memory");
			c->c_type = C_ALL;
			$$ = c;
		}
		;

condition_list	: condition comma condition_list	{
			TAILQ_INSERT_TAIL(conditions, $1, c_entry);
		}
		| condition	{
			TAILQ_INSERT_TAIL(conditions, $1, c_entry);
		}
		;

conditions	: condition				{
			TAILQ_INSERT_TAIL(conditions, $1, c_entry);
		}
		| '{' condition_list '}'
		;

user		: USER STRING		{
			struct passwd *pw;

			pw = getpwnam($2);
			if (pw == NULL) {
				yyerror("user '%s' does not exist.", $2);
				free($2);
				YYERROR;
			}
			$$ = $2;
		}
		| /* empty */		{ $$ = NULL; }
		;

action		: DELIVER TO MAILDIR user		{
			rule->r_user = $4;
			rule->r_action = A_MAILDIR;
			if (strlcpy(rule->r_value.path, "~/Maildir",
			    sizeof(rule->r_value.path)) >=
			    sizeof(rule->r_value.path))
				fatal("pathname too long");
		}
		| DELIVER TO MAILDIR STRING user	{
			rule->r_user = $5;
			rule->r_action = A_MAILDIR;
			if (strlcpy(rule->r_value.path, $4,
			    sizeof(rule->r_value.path)) >=
			    sizeof(rule->r_value.path))
				fatal("pathname too long");
			free($4);
		}
		| DELIVER TO MBOX			{
			rule->r_action = A_MBOX;
			if (strlcpy(rule->r_value.path, _PATH_MAILDIR "/%u",
			    sizeof(rule->r_value.path))
			    >= sizeof(rule->r_value.path))
				fatal("pathname too long");
		}
		| DELIVER TO MDA STRING user		{
			rule->r_user = $5;
			rule->r_action = A_EXT;
			if (strlcpy(rule->r_value.command, $4,
			    sizeof(rule->r_value.command))
			    >= sizeof(rule->r_value.command))
				fatal("command too long");
			free($4);
		}
		| RELAY				{
			rule->r_action = A_RELAY;
		}
		| RELAY VIA STRING port ssl certname auth {
			rule->r_action = A_RELAYVIA;

			if ($5 == 0 && ($6 != NULL || $7)) {
				yyerror("error: must specify tls, smtps, or ssl");
				free($6);
				free($3);
				YYERROR;
			}

			if (strlcpy(rule->r_value.relayhost.hostname, $3,
			    sizeof(rule->r_value.relayhost.hostname))
			    >= sizeof(rule->r_value.relayhost.hostname))
				fatal("hostname too long");

			rule->r_value.relayhost.port = $4;
			rule->r_value.relayhost.flags |= $5;

			if ($7)
				rule->r_value.relayhost.flags |= F_AUTH;

			if ($6 != NULL) {
				if (ssl_load_certfile(conf, $6, F_CCERT) < 0) {
					yyerror("cannot load certificate: %s",
					    $6);
					free($6);
					free($3);
					YYERROR;
				}
				if (strlcpy(rule->r_value.relayhost.cert, $6,
					sizeof(rule->r_value.relayhost.cert))
				    >= sizeof(rule->r_value.relayhost.cert))
					fatal("certificate path too long");
			}

			free($3);
			free($6);
		}
		;

from		: FROM mapref			{
			$$ = $2;
		}
		| FROM ALL			{
			struct map	*m;
			struct mapel	*me;
			struct sockaddr_in *ssin;
			struct sockaddr_in6 *ssin6;

			if ((m = calloc(1, sizeof(*m))) == NULL)
				fatal("out of memory");
			m->m_id = last_map_id++;
			if (m->m_id == INT_MAX) {
				yyerror("too many maps defined");
				free(m);
				YYERROR;
			}
			if (! bsnprintf(m->m_name, sizeof(m->m_name),
				"<dynamic(%u)>", m->m_id))
				fatal("snprintf");
			m->m_flags |= F_DYNAMIC|F_USED;
			m->m_type = T_SINGLE;

			TAILQ_INIT(&m->m_contents);

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");
			me->me_key.med_addr.bits = 0;
			me->me_key.med_addr.ss.ss_len = sizeof(struct sockaddr_in);
			ssin = (struct sockaddr_in *)&me->me_key.med_addr.ss;
			ssin->sin_family = AF_INET;
			if (inet_pton(AF_INET, "0.0.0.0", &ssin->sin_addr) != 1) {
				free(me);
				free(m);
				YYERROR;
			}
			TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");
			me->me_key.med_addr.bits = 0;
			me->me_key.med_addr.ss.ss_len = sizeof(struct sockaddr_in6);
			ssin6 = (struct sockaddr_in6 *)&me->me_key.med_addr.ss;
			ssin6->sin6_family = AF_INET6;
			if (inet_pton(AF_INET6, "::", &ssin6->sin6_addr) != 1) {
				free(me);
				free(m);
				YYERROR;
			}
			TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);

			TAILQ_INSERT_TAIL(conf->sc_maps, m, m_entry);
			$$ = m->m_id;
		}
		| FROM LOCAL			{
			struct map	*m;

			m = map_findbyname(conf, "localhost");
			$$ = m->m_id;
		}
		| /* empty */			{
			struct map	*m;

			m = map_findbyname(conf, "localhost");
			$$ = m->m_id;
		}
		;

on		: ON STRING	{
       			if (strlen($2) >= MAX_TAG_SIZE) {
       				yyerror("interface, address or tag name too long");
				free($2);
				YYERROR;
			}

			$$ = $2;
		}
		| /* empty */	{ $$ = NULL; }

rule		: decision on from			{

			if ((rule = calloc(1, sizeof(*rule))) == NULL)
				fatal("out of memory");
			rule->r_sources = map_find(conf, $3);


			if ((conditions = calloc(1, sizeof(*conditions))) == NULL)
				fatal("out of memory");

			if ($2)
				(void)strlcpy(rule->r_tag, $2, sizeof(rule->r_tag));
			free($2);


			TAILQ_INIT(conditions);

		} FOR conditions action	tag {
			struct rule	*subr;
			struct cond	*cond;

			while ((cond = TAILQ_FIRST(conditions)) != NULL) {

				if ((subr = calloc(1, sizeof(*subr))) == NULL)
					fatal("out of memory");

				*subr = *rule;

				subr->r_condition = *cond;
				
				TAILQ_REMOVE(conditions, cond, c_entry);
				TAILQ_INSERT_TAIL(conf->sc_rules, subr, r_entry);

				free(cond);
			}

			free(conditions);
			free(rule);
			conditions = NULL;
			rule = NULL;
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
		{ "accept",		ACCEPT },
		{ "alias",		ALIAS },
		{ "all",		ALL },
		{ "auth",		AUTH },
		{ "certificate",	CERTIFICATE },
		{ "config",		CONFIG },
		{ "db",			DB },
		{ "deliver",		DELIVER },
		{ "dns",		DNS },
		{ "domain",		DOMAIN },
		{ "enable",		ENABLE },
		{ "external",		EXTERNAL },
		{ "for",		FOR },
		{ "from",		FROM },
		{ "hash",		HASH },
		{ "hostname",		HOSTNAME },
		{ "include",		INCLUDE },
		{ "interval",		INTERVAL },
		{ "list",		LIST },
		{ "listen",		LISTEN },
		{ "local",		LOCAL },
		{ "maildir",		MAILDIR },
		{ "map",		MAP },
		{ "mbox",		MBOX },
		{ "mda",		MDA },
		{ "network",		NETWORK },
		{ "on",			ON },
		{ "plain",		PLAIN },
		{ "port",		PORT },
		{ "queue",		QUEUE },
		{ "reject",		REJECT },
		{ "relay",		RELAY },
		{ "single",		SINGLE },
		{ "size",		SIZE },
		{ "smtps",		SMTPS },
		{ "source",		SOURCE },
		{ "ssl",		SSL },
		{ "tag",		TAG },
		{ "tls",		TLS },
		{ "to",			TO },
		{ "type",		TYPE },
		{ "user",		USER },
		{ "via",		VIA },
		{ "virtual",		VIRTUAL },
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
	pushback_index = 0;

	/* skip to either EOF or the first real EOL */
	while (1) {
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
				else if (next == '\n')
					continue;
				else
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

	if (c == '=') {
		if ((c = lgetc(0)) != EOF && c == '>')
			return (ARROW);
		lungetc(c);
		c = '=';
	}

#define allowed_in_string(x) \
	(isalnum(x) || (ispunct(x) && x != '(' && x != ')' && \
	x != '{' && x != '}' && x != '<' && x != '>' && \
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

int
parse_config(struct smtpd *x_conf, const char *filename, int opts)
{
	struct sym	*sym, *next;
	struct map	*m;

	conf = x_conf;
	bzero(conf, sizeof(*conf));

	conf->sc_maxsize = SIZE_MAX;

	if ((conf->sc_maps = calloc(1, sizeof(*conf->sc_maps))) == NULL) {
		log_warn("cannot allocate memory");
		return (-1);
	}
	if ((conf->sc_rules = calloc(1, sizeof(*conf->sc_rules))) == NULL) {
		log_warn("cannot allocate memory");
		free(conf->sc_maps);
		return (-1);
	}
	if ((conf->sc_listeners = calloc(1, sizeof(*conf->sc_listeners))) == NULL) {
		log_warn("cannot allocate memory");
		free(conf->sc_maps);
		free(conf->sc_rules);
		return (-1);
	}
	if ((conf->sc_ssl = calloc(1, sizeof(*conf->sc_ssl))) == NULL) {
		log_warn("cannot allocate memory");
		free(conf->sc_maps);
		free(conf->sc_rules);
		free(conf->sc_listeners);
		return (-1);
	}
	if ((m = calloc(1, sizeof(*m))) == NULL) {
		log_warn("cannot allocate memory");
		free(conf->sc_maps);
		free(conf->sc_rules);
		free(conf->sc_listeners);
		free(conf->sc_ssl);
		return (-1);
	}

	errors = 0;
	last_map_id = 1;

	map = NULL;
	rule = NULL;

	TAILQ_INIT(conf->sc_listeners);
	TAILQ_INIT(conf->sc_maps);
	TAILQ_INIT(conf->sc_rules);
	SPLAY_INIT(conf->sc_ssl);
	SPLAY_INIT(&conf->sc_sessions);

	conf->sc_qintval.tv_sec = SMTPD_QUEUE_INTERVAL;
	conf->sc_qintval.tv_usec = 0;
	conf->sc_opts = opts;

	if ((file = pushfile(filename, 0)) == NULL) {
		purge_config(conf, PURGE_EVERYTHING);
		free(m);
		return (-1);
	}
	topfile = file;

	/*
	 * declare special "local" map
	 */
	m->m_id = last_map_id++;
	if (strlcpy(m->m_name, "localhost", sizeof(m->m_name))
	    >= sizeof(m->m_name))
		fatal("strlcpy");
	m->m_type = T_LIST;
	TAILQ_INIT(&m->m_contents);
	TAILQ_INSERT_TAIL(conf->sc_maps, m, m_entry);
	set_localaddrs();

	/*
	 * parse configuration
	 */
	setservent(1);
	yyparse();
	errors = file->errors;
	popfile();
	endservent();

	/* Free macros and check which have not been used. */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entry);
		if ((conf->sc_opts & SMTPD_OPT_VERBOSE) && !sym->used)
			fprintf(stderr, "warning: macro '%s' not "
			    "used\n", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
			free(sym);
		}
	}

	if (TAILQ_EMPTY(conf->sc_rules)) {
		log_warnx("no rules, nothing to do");
		errors++;
	}

	if (strlen(conf->sc_hostname) == 0)
		if (gethostname(conf->sc_hostname,
		    sizeof(conf->sc_hostname)) == -1) {
			log_warn("could not determine host name");
			bzero(conf->sc_hostname, sizeof(conf->sc_hostname));
			errors++;
		}

	if (errors) {
		purge_config(conf, PURGE_EVERYTHING);
		return (-1);
	}

	return (0);
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

struct listener *
host_v4(const char *s, in_port_t port)
{
	struct in_addr		 ina;
	struct sockaddr_in	*sain;
	struct listener		*h;

	bzero(&ina, sizeof(ina));
	if (inet_pton(AF_INET, s, &ina) != 1)
		return (NULL);

	if ((h = calloc(1, sizeof(*h))) == NULL)
		fatal(NULL);
	sain = (struct sockaddr_in *)&h->ss;
	sain->sin_len = sizeof(struct sockaddr_in);
	sain->sin_family = AF_INET;
	sain->sin_addr.s_addr = ina.s_addr;
	sain->sin_port = port;

	return (h);
}

struct listener *
host_v6(const char *s, in_port_t port)
{
	struct in6_addr		 ina6;
	struct sockaddr_in6	*sin6;
	struct listener		*h;

	bzero(&ina6, sizeof(ina6));
	if (inet_pton(AF_INET6, s, &ina6) != 1)
		return (NULL);

	if ((h = calloc(1, sizeof(*h))) == NULL)
		fatal(NULL);
	sin6 = (struct sockaddr_in6 *)&h->ss;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_port = port;
	memcpy(&sin6->sin6_addr, &ina6, sizeof(ina6));

	return (h);
}

int
host_dns(const char *s, const char *tag, const char *cert,
    struct listenerlist *al, int max, in_port_t port, u_int8_t flags)
{
	struct addrinfo		 hints, *res0, *res;
	int			 error, cnt = 0;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct listener		*h;

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
		h->flags = flags;
		h->ss.ss_family = res->ai_family;
		h->ssl = NULL;
		h->ssl_cert_name[0] = '\0';
		if (cert != NULL)
			(void)strlcpy(h->ssl_cert_name, cert, sizeof(h->ssl_cert_name));
		if (tag != NULL)
			(void)strlcpy(h->tag, tag, sizeof(h->tag));

		if (res->ai_family == AF_INET) {
			sain = (struct sockaddr_in *)&h->ss;
			sain->sin_len = sizeof(struct sockaddr_in);
			sain->sin_addr.s_addr = ((struct sockaddr_in *)
			    res->ai_addr)->sin_addr.s_addr;
			sain->sin_port = port;
		} else {
			sin6 = (struct sockaddr_in6 *)&h->ss;
			sin6->sin6_len = sizeof(struct sockaddr_in6);
			memcpy(&sin6->sin6_addr, &((struct sockaddr_in6 *)
			    res->ai_addr)->sin6_addr, sizeof(struct in6_addr));
			sin6->sin6_port = port;
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
host(const char *s, const char *tag, const char *cert, struct listenerlist *al,
    int max, in_port_t port, u_int8_t flags)
{
	struct listener *h;

	h = host_v4(s, port);

	/* IPv6 address? */
	if (h == NULL)
		h = host_v6(s, port);

	if (h != NULL) {
		h->port = port;
		h->flags = flags;
		h->ssl = NULL;
		h->ssl_cert_name[0] = '\0';
		if (cert != NULL)
			(void)strlcpy(h->ssl_cert_name, cert, sizeof(h->ssl_cert_name));
		if (tag != NULL)
			(void)strlcpy(h->tag, tag, sizeof(h->tag));

		TAILQ_INSERT_HEAD(al, h, entry);
		return (1);
	}

	return (host_dns(s, tag, cert, al, max, port, flags));
}

int
interface(const char *s, const char *tag, const char *cert,
    struct listenerlist *al, int max, in_port_t port, u_int8_t flags)
{
	struct ifaddrs *ifap, *p;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct listener		*h;
	int ret = 0;

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	for (p = ifap; p != NULL; p = p->ifa_next) {
		if (strcmp(s, p->ifa_name) != 0)
			continue;

		if ((h = calloc(1, sizeof(*h))) == NULL)
			fatal(NULL);

		switch (p->ifa_addr->sa_family) {
		case AF_INET:
			sain = (struct sockaddr_in *)&h->ss;
			*sain = *(struct sockaddr_in *)p->ifa_addr;
			sain->sin_len = sizeof(struct sockaddr_in);
			sain->sin_port = port;
			break;

		case AF_INET6:
			sin6 = (struct sockaddr_in6 *)&h->ss;
			*sin6 = *(struct sockaddr_in6 *)p->ifa_addr;
			sin6->sin6_len = sizeof(struct sockaddr_in6);
			sin6->sin6_port = port;
			break;

		default:
			free(h);
			continue;
		}

		h->fd = -1;
		h->port = port;
		h->flags = flags;
		h->ssl = NULL;
		h->ssl_cert_name[0] = '\0';
		if (cert != NULL)
			(void)strlcpy(h->ssl_cert_name, cert, sizeof(h->ssl_cert_name));
		if (tag != NULL)
			(void)strlcpy(h->tag, tag, sizeof(h->tag));

		ret = 1;
		TAILQ_INSERT_HEAD(al, h, entry);
	}

	freeifaddrs(ifap);

	return ret;
}

void
set_localaddrs(void)
{
	struct ifaddrs *ifap, *p;
	struct sockaddr_storage ss;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct map		*m;
	struct mapel		*me;

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	m = map_findbyname(conf, "localhost");

	for (p = ifap; p != NULL; p = p->ifa_next) {
		switch (p->ifa_addr->sa_family) {
		case AF_INET:
			sain = (struct sockaddr_in *)&ss;
			*sain = *(struct sockaddr_in *)p->ifa_addr;
			sain->sin_len = sizeof(struct sockaddr_in);

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");
			me->me_key.med_addr.bits = 32;
			me->me_key.med_addr.ss = *(struct sockaddr_storage *)sain;
			TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);

			break;

		case AF_INET6:
			sin6 = (struct sockaddr_in6 *)&ss;
			*sin6 = *(struct sockaddr_in6 *)p->ifa_addr;
			sin6->sin6_len = sizeof(struct sockaddr_in6);

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");
			me->me_key.med_addr.bits = 128;
			me->me_key.med_addr.ss = *(struct sockaddr_storage *)sin6;
			TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);

			break;
		}
	}

	freeifaddrs(ifap);
}

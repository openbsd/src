/*	$OpenBSD: parse.y,v 1.112 2013/01/28 15:14:02 gilles Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@poolp.org>
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
#include <sys/ioctl.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <ifaddrs.h>
#include <imsg.h>
#include <inttypes.h>
#include <netdb.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "smtpd.h"
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

struct table		*table = NULL;
struct rule		*rule = NULL;
struct listener		 l;

struct listener	*host_v4(const char *, in_port_t);
struct listener	*host_v6(const char *, in_port_t);
int		 host_dns(const char *, const char *, const char *,
		    struct listenerlist *, int, in_port_t, uint8_t);
int		 host(const char *, const char *, const char *,
    struct listenerlist *, int, in_port_t, const char *, uint8_t, const char *);
int		 interface(const char *, const char *, const char *,
    struct listenerlist *, int, in_port_t, const char *, uint8_t, const char *);
void		 set_localaddrs(void);
int		 delaytonum(char *);
int		 is_if_in_group(const char *, const char *);

typedef struct {
	union {
		int64_t		 number;
		objid_t		 object;
		char		*string;
		struct host	*host;
		struct mailaddr	*maddr;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	AS QUEUE COMPRESSION MAXMESSAGESIZE LISTEN ON ANY PORT EXPIRE
%token	TABLE SSL SMTPS CERTIFICATE DOMAIN BOUNCEWARN
%token  RELAY BACKUP VIA DELIVER TO MAILDIR MBOX HOSTNAME HELO
%token	ACCEPT REJECT INCLUDE ERROR MDA FROM FOR SOURCE
%token	ARROW AUTH TLS LOCAL VIRTUAL TAG TAGGED ALIAS FILTER KEY
%token	AUTH_OPTIONAL TLS_REQUIRE USERS SENDER
%token	<v.string>	STRING
%token  <v.number>	NUMBER
%type	<v.table>	table
%type	<v.number>	port from auth ssl size expire sender
%type	<v.object>	tables tablenew tableref destination alias virtual usermapping userbase credentials
%type	<v.maddr>	relay_as
%type	<v.string>	certificate tag tagged compression relay_source listen_helo relay_helo relay_backup
%%

grammar		: /* empty */
		| grammar '\n'
		| grammar include '\n'
		| grammar varset '\n'
		| grammar main '\n'
		| grammar table '\n'
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

size		: NUMBER		{
			if ($1 < 0) {
				yyerror("invalid size: %" PRId64, $1);
				YYERROR;
			}
			$$ = $1;
		}
		| STRING			{
			long long result;

			if (scan_scaled($1, &result) == -1 || result < 0) {
				yyerror("invalid size: %s", $1);
				free($1);
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
				yyerror("invalid port: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
			$$ = ntohs(servent->s_port);
		}
		| PORT NUMBER			{
			if ($2 <= 0 || $2 >= (int)USHRT_MAX) {
				yyerror("invalid port: %" PRId64, $2);
				YYERROR;
			}
			$$ = $2;
		}
		| /* empty */			{
			$$ = 0;
		}
		;

certificate	: CERTIFICATE STRING	{
			if (($$ = strdup($2)) == NULL) {
				yyerror("strdup");
				free($2);
				YYERROR;
			}
			free($2);
		}
		| /* empty */			{ $$ = NULL; }
		;

ssl		: SMTPS				{ $$ = F_SMTPS; }
		| TLS				{ $$ = F_STARTTLS; }
		| SSL				{ $$ = F_SSL; }
		| TLS_REQUIRE			{ $$ = F_STARTTLS|F_STARTTLS_REQUIRE; }
		| /* Empty */			{ $$ = 0; }
		;

auth		: AUTH				{
			$$ = F_AUTH|F_AUTH_REQUIRE;
		}
		| AUTH_OPTIONAL			{
			$$ = F_AUTH;
		}
		| AUTH tables  			{
			strlcpy(l.authtable, table_find($2)->t_name, sizeof l.authtable);
			$$ = F_AUTH|F_AUTH_REQUIRE;
		}
		| AUTH_OPTIONAL tables 		{
			strlcpy(l.authtable, table_find($2)->t_name, sizeof l.authtable);
			$$ = F_AUTH;
		}
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

tagged		: TAGGED STRING			{
			if (($$ = strdup($2)) == NULL) {
       				yyerror("strdup");
				free($2);
				YYERROR;
			}
			free($2);
		}
		| /* empty */			{ $$ = NULL; }
		;

expire		: EXPIRE STRING {
			$$ = delaytonum($2);
			if ($$ == -1) {
				yyerror("invalid expire delay: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
		}
		| /* empty */	{ $$ = conf->sc_qexpire; }
		;

bouncedelay	: STRING {
			time_t	d;
			int	i;

			d = delaytonum($1);
			if (d < 0) {
				yyerror("invalid bounce delay: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
			for (i = 0; i < MAX_BOUNCE_WARN; i++) {
				if (conf->sc_bounce_warn[i] != 0)
					continue;
				conf->sc_bounce_warn[i] = d;
				break;
			}
		}

bouncedelays	: bouncedelays ',' bouncedelay
		| bouncedelay
		| /* EMPTY */
		;

credentials	: AUTH tables	{
			struct table   *t = table_find($2);

			if (! table_check_use(t, T_DYNAMIC|T_HASH, K_CREDENTIALS)) {
				yyerror("invalid use of table \"%s\" as AUTH parameter",
				    t->t_name);
				YYERROR;
			}

			$$ = t->t_id;
		}
		| /* empty */	{ $$ = 0; }
		;

compression	: COMPRESSION		{
			$$ = strdup("gzip");
			if ($$ == NULL) {
				yyerror("strdup");
				YYERROR;
			}
		}
		| COMPRESSION STRING	{ $$ = $2; }
		;

listen_helo	: HOSTNAME STRING	{ $$ = $2; }
		| /* empty */		{ $$ = NULL; }

main		: QUEUE compression {
			conf->sc_queue_compress_algo = strdup($2);
			if (conf->sc_queue_compress_algo == NULL) {
				yyerror("strdup");
				free($2);
				YYERROR;
			}
			conf->sc_queue_flags |= QUEUE_COMPRESS;
			free($2);
		}
		| BOUNCEWARN {
			bzero(conf->sc_bounce_warn, sizeof conf->sc_bounce_warn);
		} bouncedelays
		| EXPIRE STRING {
			conf->sc_qexpire = delaytonum($2);
			if (conf->sc_qexpire == -1) {
				yyerror("invalid expire delay: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
		}
		| MAXMESSAGESIZE size {
			conf->sc_maxsize = $2;
		}
		| LISTEN {
			bzero(&l, sizeof l);
		} ON STRING port ssl certificate auth tag listen_helo {
			char	       *ifx  = $4;
			in_port_t	port = $5;
			uint8_t		ssl  = $6;
			char	       *cert = $7;
			uint8_t		auth = $8;
			char	       *tag  = $9;
			char	       *helo = $10;

			if (port != 0 && ssl == F_SSL) {
				yyerror("invalid listen option: tls/smtps on same port");
				YYERROR;
			}

			if (auth != 0 && !ssl) {
				yyerror("invalid listen option: auth requires tls/smtps");
				YYERROR;
			}

			if (port == 0) {
				if (ssl & F_SMTPS) {
					if (! interface(ifx, tag, cert, conf->sc_listeners,
						MAX_LISTEN, 465, l.authtable, F_SMTPS|auth, helo)) {
						if (host(ifx, tag, cert, conf->sc_listeners,
							MAX_LISTEN, 465, l.authtable, ssl|auth, helo) <= 0) {
							yyerror("invalid virtual ip or interface: %s", ifx);
							YYERROR;
						}
					}
				}
				if (! ssl || (ssl & ~F_SMTPS)) {
					if (! interface(ifx, tag, cert, conf->sc_listeners,
						MAX_LISTEN, 25, l.authtable, (ssl&~F_SMTPS)|auth, helo)) {
						if (host(ifx, tag, cert, conf->sc_listeners,
							MAX_LISTEN, 25, l.authtable, ssl|auth, helo) <= 0) {
							yyerror("invalid virtual ip or interface: %s", ifx);
							YYERROR;
						}
					}
				}
			}
			else {
				if (! interface(ifx, tag, cert, conf->sc_listeners,
					MAX_LISTEN, port, l.authtable, ssl|auth, helo)) {
					if (host(ifx, tag, cert, conf->sc_listeners,
						MAX_LISTEN, port, l.authtable, ssl|auth, helo) <= 0) {
						yyerror("invalid virtual ip or interface: %s", ifx);
						YYERROR;
					}
				}
			}
		}
		| FILTER STRING			{
			struct filter *filter;
			struct filter *tmp;

			filter = xcalloc(1, sizeof *filter, "parse condition: FILTER");
			if (strlcpy(filter->name, $2, sizeof (filter->name))
			    >= sizeof (filter->name)) {
       				yyerror("Filter name too long: %s", filter->name);
				free($2);
				YYERROR;
				
			}
			(void)snprintf(filter->path, sizeof filter->path,
			    PATH_FILTERS "/%s", filter->name);

			tmp = dict_get(&conf->sc_filters, filter->name);
			if (tmp == NULL)
				dict_set(&conf->sc_filters, filter->name, filter);
			else {
       				yyerror("ambiguous filter name: %s", filter->name);
				free($2);
				YYERROR;
			}
			free($2);
		}
		| FILTER STRING STRING		{
			struct filter *filter;
			struct filter *tmp;

			filter = calloc(1, sizeof (*filter));
			if (filter == NULL ||
			    strlcpy(filter->name, $2, sizeof (filter->name))
			    >= sizeof (filter->name) ||
			    strlcpy(filter->path, $3, sizeof (filter->path))
			    >= sizeof (filter->path)) {
				free(filter);
				free($2);
				free($3);
				YYERROR;
			}

			tmp = dict_get(&conf->sc_filters, filter->name);
			if (tmp == NULL)
				dict_set(&conf->sc_filters, filter->name, filter);
			else {
       				yyerror("ambiguous filter name: %s", filter->name);
				free($2);
				free($3);
				YYERROR;
			}
			free($2);
			free($3);
		}
		;

table		: TABLE STRING STRING	{
			char *p, *backend, *config;

			p = $3;
			if (*p == '/') {
				backend = "static";
				config = $3;
			}
			else {
				backend = $3;
				config = NULL;
				for (p = $3; *p && *p != ':'; p++)
					;
				if (*p == ':') {
					*p = '\0';
					backend = $3;
					config  = p+1;
				}
			}
			if (config != NULL && *config != '/') {
				yyerror("invalid backend parameter for table: %s",
				    $2);
				free($2);
				free($3);
				YYERROR;
			}
			table = table_create(backend, $2, config);
			if (! table->t_backend->config(table, config)) {
				yyerror("invalid backend configuration for table %s",
				    table->t_name);
				free($2);
				free($3);
				YYERROR;
			}
			free($2);
			free($3);
		}
		| TABLE STRING {
			table = table_create("static", $2, NULL);
			free($2);
		} '{' tableval_list '}' {
			table = NULL;
		}
		;

keyval		: STRING ARROW STRING		{
			table->t_type = T_HASH;
			table_add(table, $1, $3);
			free($1);
			free($3);
		}
		;

keyval_list	: keyval
		| keyval comma keyval_list
		;

stringel	: STRING			{
			table->t_type = T_LIST;
			table_add(table, $1, NULL);
			free($1);
		}
		;

string_list	: stringel
		| stringel comma string_list
		;

tableval_list	: string_list			{ }
		| keyval_list			{ }
		;

tablenew	: STRING			{
			struct table	*t;

			t = table_create("static", NULL, NULL);
			t->t_type = T_LIST;
			table_add(t, $1, NULL);
			free($1);
			$$ = t->t_id;
			table = table_create("static", NULL, NULL);
		}
		| '{'				{
			table = table_create("static", NULL, NULL);
		} tableval_list '}'		{
			$$ = table->t_id;
		}
		;

tableref       	: '<' STRING '>'       		{
			struct table	*t;

			if ((t = table_findbyname($2)) == NULL) {
				yyerror("no such table: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
			$$ = t->t_id;
		}
		;

tables		: tablenew			{ $$ = $1; }
		| tableref			{ $$ = $1; }
		;

alias		: ALIAS tables			{
			struct table   *t = table_find($2);

			if (! table_check_use(t, T_DYNAMIC|T_HASH, K_ALIAS)) {
				yyerror("invalid use of table \"%s\" as ALIAS parameter",
				    t->t_name);
				YYERROR;
			}

			$$ = t->t_id;
		}
		;

virtual		: VIRTUAL tables		{
			struct table   *t = table_find($2);

			if (! table_check_service(t, K_ALIAS)) {
				yyerror("invalid use of table \"%s\" as VIRTUAL parameter",
				    t->t_name);
				YYERROR;
			}

			$$ = t->t_id;
		}
		;

usermapping	: alias		{
			rule->r_desttype = DEST_DOM;
			$$ = $1;
		}
		| virtual	{
			rule->r_desttype = DEST_VDOM;
			$$ = $1;
		}
		| /**/		{
			rule->r_desttype = DEST_DOM;
			$$ = 0;
		}
		;

userbase	: USERS tables	{
			struct table   *t = table_find($2);

			if (! table_check_use(t, T_DYNAMIC|T_HASH, K_USERINFO)) {
				yyerror("invalid use of table \"%s\" as USERS parameter",
				    t->t_name);
				YYERROR;
			}

			$$ = t->t_id;
		}
		| /**/	{ $$ = table_findbyname("<getpwnam>")->t_id; }
		;

		


destination	: DOMAIN tables			{
			struct table   *t = table_find($2);

			if (! table_check_use(t, T_DYNAMIC|T_LIST, K_DOMAIN)) {
				yyerror("invalid use of table \"%s\" as DOMAIN parameter",
				    t->t_name);
				YYERROR;
			}

			$$ = t->t_id;
		}
		| LOCAL		{ $$ = table_findbyname("<localnames>")->t_id; }
		| ANY		{ $$ = 0; }
		;

relay_source	: SOURCE tables			{
			struct table	*t = table_find($2);
			if (! table_check_use(t, T_DYNAMIC|T_LIST, K_SOURCE)) {
				yyerror("invalid use of table \"%s\" as "
				    "SOURCE parameter", t->t_name);
				YYERROR;
			}
			$$ = t->t_name;
		}
		| { $$ = NULL; }
		;

relay_helo	: HELO tables			{
			struct table	*t = table_find($2);
			if (! table_check_use(t, T_DYNAMIC|T_HASH, K_ADDRNAME)) {
				yyerror("invalid use of table \"%s\" as "
				    "HELO parameter", t->t_name);
				YYERROR;
			}
			$$ = t->t_name;
		}
		| { $$ = NULL; }
		;

relay_backup	: BACKUP STRING			{ $$ = $2; }
		| BACKUP       			{ $$ = NULL; }
		;

relay_as     	: AS STRING		{
			struct mailaddr maddr, *maddrp;

			if (! text_to_mailaddr(&maddr, $2)) {
				yyerror("invalid parameter to AS: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);

			if (maddr.user[0] == '\0' && maddr.domain[0] == '\0') {
				yyerror("invalid empty parameter to AS");
				YYERROR;
			}
			else if (maddr.domain[0] == '\0') {
				if (strlcpy(maddr.domain, conf->sc_hostname,
					sizeof (maddr.domain))
				    >= sizeof (maddr.domain)) {
					yyerror("hostname too long for AS parameter: %s",
					    conf->sc_hostname);
					YYERROR;
				}
			}
			$$ = xmemdup(&maddr, sizeof (*maddrp), "parse relay_as: AS");
		}
		| /* empty */		{ $$ = NULL; }
		;

action		: userbase DELIVER TO MAILDIR			{
			rule->r_users = table_find($1);
			rule->r_action = A_MAILDIR;
			if (strlcpy(rule->r_value.buffer, "~/Maildir",
			    sizeof(rule->r_value.buffer)) >=
			    sizeof(rule->r_value.buffer))
				fatal("pathname too long");
		}
		| userbase DELIVER TO MAILDIR STRING		{
			rule->r_users = table_find($1);
			rule->r_action = A_MAILDIR;
			if (strlcpy(rule->r_value.buffer, $5,
			    sizeof(rule->r_value.buffer)) >=
			    sizeof(rule->r_value.buffer))
				fatal("pathname too long");
			free($5);
		}
		| userbase DELIVER TO MBOX			{
			rule->r_users = table_find($1);
			rule->r_action = A_MBOX;
			if (strlcpy(rule->r_value.buffer, _PATH_MAILDIR "/%u",
			    sizeof(rule->r_value.buffer))
			    >= sizeof(rule->r_value.buffer))
				fatal("pathname too long");
		}
		| userbase DELIVER TO MDA STRING	       	{
			rule->r_users = table_find($1);
			rule->r_action = A_MDA;
			if (strlcpy(rule->r_value.buffer, $5,
			    sizeof(rule->r_value.buffer))
			    >= sizeof(rule->r_value.buffer))
				fatal("command too long");
			free($5);
		}
		| RELAY relay_as relay_source relay_helo       	{
			rule->r_action = A_RELAY;
			rule->r_as = $2;
			if ($4 != NULL && $3 == NULL) {
				yyerror("HELO can only be used with SOURCE");
				YYERROR;
			}
			if ($3)
				strlcpy(rule->r_value.relayhost.sourcetable, $3,
				    sizeof rule->r_value.relayhost.sourcetable);
			if ($4)
				strlcpy(rule->r_value.relayhost.helotable, $4,
				    sizeof rule->r_value.relayhost.helotable);
		}
		| RELAY relay_backup relay_as relay_source relay_helo	{
			rule->r_action = A_RELAY;
			rule->r_as = $3;
			rule->r_value.relayhost.flags |= F_BACKUP;

			if ($2)
				strlcpy(rule->r_value.relayhost.hostname, $2,
				    sizeof (rule->r_value.relayhost.hostname));
			else
				strlcpy(rule->r_value.relayhost.hostname, env->sc_hostname,
				    sizeof (rule->r_value.relayhost.hostname));
			free($2);

			if ($5 != NULL && $4 == NULL) {
				yyerror("HELO can only be used with SOURCE");
				YYERROR;
			}
			if ($4)
				strlcpy(rule->r_value.relayhost.sourcetable, $4,
				    sizeof rule->r_value.relayhost.sourcetable);
			if ($5)
				strlcpy(rule->r_value.relayhost.helotable, $5,
				    sizeof rule->r_value.relayhost.helotable);
		}
		| RELAY VIA STRING certificate credentials relay_as relay_source relay_helo {
			struct table	*t;

			rule->r_action = A_RELAYVIA;
			rule->r_as = $6;

			if (! text_to_relayhost(&rule->r_value.relayhost, $3)) {
				yyerror("error: invalid url: %s", $3);
				free($3);
				free($4);
				free($6);
				YYERROR;
			}
			free($3);

			/* no worries, F_AUTH cant be set without SSL */
			if (rule->r_value.relayhost.flags & F_AUTH) {
				if (! $5) {
					yyerror("error: auth without auth table");
					free($4);
					free($6);
					YYERROR;
				}
				t = table_find($5);
				strlcpy(rule->r_value.relayhost.authtable, t->t_name,
				    sizeof(rule->r_value.relayhost.authtable));
			}

			if ($4 != NULL) {
				if (strlcpy(rule->r_value.relayhost.cert, $4,
					sizeof(rule->r_value.relayhost.cert))
				    >= sizeof(rule->r_value.relayhost.cert))
					fatal("certificate path too long");
			}
			free($4);
			if ($8 != NULL && $7 == NULL) {
				yyerror("HELO can only be used with SOURCE");
				YYERROR;
			}
			if ($7)
				strlcpy(rule->r_value.relayhost.sourcetable, $7,
				    sizeof rule->r_value.relayhost.sourcetable);
			if ($8)
				strlcpy(rule->r_value.relayhost.helotable, $8,
				    sizeof rule->r_value.relayhost.helotable);
		}
		;

from		: FROM tables			{
			struct table   *t = table_find($2);

			if (! table_check_use(t, T_DYNAMIC|T_LIST, K_NETADDR)) {
				yyerror("invalid use of table \"%s\" as FROM parameter",
				    t->t_name);
				YYERROR;
			}

			$$ = t->t_id;
		}
		| FROM ANY			{
			$$ = table_findbyname("<anyhost>")->t_id;
		}
		| FROM LOCAL			{
			$$ = table_findbyname("<localhost>")->t_id;
		}
		| /* empty */			{
			$$ = table_findbyname("<localhost>")->t_id;
		}
		;

sender		: SENDER tables			{
			struct table   *t = table_find($2);

			if (! table_check_use(t, T_DYNAMIC|T_LIST, K_MAILADDR)) {
				yyerror("invalid use of table \"%s\" as SENDER parameter",
				    t->t_name);
				YYERROR;
			}

			$$ = t->t_id;
		}
		| /* empty */			{ $$ = 0; }
		;

rule		: ACCEPT {
			rule = xcalloc(1, sizeof(*rule), "parse rule: ACCEPT");
		 } tagged from sender FOR destination usermapping action expire {

			rule->r_decision = R_ACCEPT;
			rule->r_sources = table_find($4);
			rule->r_senders = table_find($5);
			rule->r_destination = table_find($7);
			rule->r_mapping = table_find($8);
			if ($3) {
				if (strlcpy(rule->r_tag, $3, sizeof rule->r_tag)
				    >= sizeof rule->r_tag) {
					yyerror("tag name too long: %s", $3);
					free($3);
					YYERROR;
				}
				free($3);
			}
			rule->r_qexpire = $10;

			if (rule->r_mapping && rule->r_desttype == DEST_VDOM) {
				enum table_type type;

				switch (rule->r_action) {
				case A_RELAY:
				case A_RELAYVIA:
					type = T_LIST;
					break;
				default:
					type = T_HASH;
					break;
				}
				if (! table_check_service(rule->r_mapping, K_ALIAS) &&
				    ! table_check_type(rule->r_mapping, type)) {
					yyerror("invalid use of table \"%s\" as VIRTUAL parameter",
					    rule->r_mapping->t_name);
					YYERROR;
				}
			}

			TAILQ_INSERT_TAIL(conf->sc_rules, rule, r_entry);

			rule = NULL;
		}
		| REJECT {
			rule = xcalloc(1, sizeof(*rule), "parse rule: REJECT");
		} tagged from sender FOR destination usermapping {
			rule->r_decision = R_REJECT;
			rule->r_sources = table_find($4);
			rule->r_sources = table_find($5);
			rule->r_destination = table_find($7);
			rule->r_mapping = table_find($8);
			if ($3) {
				if (strlcpy(rule->r_tag, $3, sizeof rule->r_tag)
				    >= sizeof rule->r_tag) {
					yyerror("tag name too long: %s", $3);
					free($3);
					YYERROR;
				}
				free($3);
			}
			TAILQ_INSERT_TAIL(conf->sc_rules, rule, r_entry);
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
		{ "any",		ANY },
		{ "as",			AS },
		{ "auth",		AUTH },
		{ "auth-optional",     	AUTH_OPTIONAL },
		{ "backup",		BACKUP },
		{ "bounce-warn",	BOUNCEWARN },
		{ "certificate",	CERTIFICATE },
		{ "compression",       	COMPRESSION },
		{ "deliver",		DELIVER },
		{ "domain",		DOMAIN },
		{ "expire",		EXPIRE },
		{ "filter",		FILTER },
		{ "for",		FOR },
		{ "from",		FROM },
		{ "helo",		HELO },
		{ "hostname",		HOSTNAME },
		{ "include",		INCLUDE },
		{ "key",		KEY },
		{ "listen",		LISTEN },
		{ "local",		LOCAL },
		{ "maildir",		MAILDIR },
		{ "max-message-size",  	MAXMESSAGESIZE },
		{ "mbox",		MBOX },
		{ "mda",		MDA },
		{ "on",			ON },
		{ "port",		PORT },
		{ "queue",		QUEUE },
		{ "reject",		REJECT },
		{ "relay",		RELAY },
		{ "sender",    		SENDER },
		{ "smtps",		SMTPS },
		{ "source",		SOURCE },
		{ "ssl",		SSL },
		{ "table",		TABLE },
		{ "tag",		TAG },
		{ "tagged",		TAGGED },
		{ "tls",		TLS },
		{ "tls-require",       	TLS_REQUIRE },
		{ "to",			TO },
		{ "users",     		USERS },
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
		log_warn("warn: cannot stat %s", fname);
		return (-1);
	}
	if (st.st_uid != 0 && st.st_uid != getuid()) {
		log_warnx("warn: %s: owner not root or current user", fname);
		return (-1);
	}
	if (st.st_mode & (S_IRWXG | S_IRWXO)) {
		log_warnx("warn: %s: group/world readable/writeable", fname);
		return (-1);
	}
	return (0);
}

struct file *
pushfile(const char *name, int secret)
{
	struct file	*nfile;

	if ((nfile = calloc(1, sizeof(struct file))) == NULL) {
		log_warn("warn: malloc");
		return (NULL);
	}
	if ((nfile->name = strdup(name)) == NULL) {
		log_warn("warn: malloc");
		free(nfile);
		return (NULL);
	}
	if ((nfile->stream = fopen(nfile->name, "r")) == NULL) {
		log_warn("warn: %s", nfile->name);
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
	struct sym     *sym, *next;
	struct table   *t;
	char		hostname[MAXHOSTNAMELEN];

	if (gethostname(hostname, sizeof hostname) == -1) {
		fprintf(stderr, "invalid hostname: gethostname() failed\n");
		return (-1);
	}

	conf = x_conf;
	bzero(conf, sizeof(*conf));

	conf->sc_maxsize = DEFAULT_MAX_BODY_SIZE;

	conf->sc_tables_dict = calloc(1, sizeof(*conf->sc_tables_dict));
	conf->sc_tables_tree = calloc(1, sizeof(*conf->sc_tables_tree));
	conf->sc_rules = calloc(1, sizeof(*conf->sc_rules));
	conf->sc_listeners = calloc(1, sizeof(*conf->sc_listeners));
	conf->sc_ssl_dict = calloc(1, sizeof(*conf->sc_ssl_dict));

	/* Report mails delayed for more than 4 hours */
	conf->sc_bounce_warn[0] = 3600 * 4;

	if (conf->sc_tables_dict == NULL	||
	    conf->sc_tables_tree == NULL	||
	    conf->sc_rules == NULL		||
	    conf->sc_listeners == NULL		||
	    conf->sc_ssl_dict == NULL) {
		log_warn("warn: cannot allocate memory");
		free(conf->sc_tables_dict);
		free(conf->sc_tables_tree);
		free(conf->sc_rules);
		free(conf->sc_listeners);
		free(conf->sc_ssl_dict);
		return (-1);
	}

	errors = 0;

	table = NULL;
	rule = NULL;

	dict_init(&conf->sc_filters);

	dict_init(conf->sc_ssl_dict);
	dict_init(conf->sc_tables_dict);
	tree_init(conf->sc_tables_tree);

	TAILQ_INIT(conf->sc_listeners);
	TAILQ_INIT(conf->sc_rules);

	conf->sc_qexpire = SMTPD_QUEUE_EXPIRY;
	conf->sc_opts = opts;

	if ((file = pushfile(filename, 0)) == NULL) {
		purge_config(PURGE_EVERYTHING);
		return (-1);
	}
	topfile = file;

	/*
	 * declare special "localhost", "anyhost" and "localnames" tables
	 */
	set_localaddrs();

	t = table_create("static", "<localnames>", NULL);
	t->t_type = T_LIST;
	table_add(t, "localhost", NULL);
	table_add(t, hostname, NULL);

	table_create("getpwnam", "<getpwnam>", NULL);

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
		log_warnx("warn: no rules, nothing to do");
		errors++;
	}

	if (strlen(conf->sc_hostname) == 0)
		if (gethostname(conf->sc_hostname,
		    sizeof(conf->sc_hostname)) == -1) {
			log_warn("warn: could not determine host name");
			bzero(conf->sc_hostname, sizeof(conf->sc_hostname));
			errors++;
		}

	if (errors) {
		purge_config(PURGE_EVERYTHING);
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

	h = xcalloc(1, sizeof(*h), "host_v4");
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

	h = xcalloc(1, sizeof(*h), "host_v6");
	sin6 = (struct sockaddr_in6 *)&h->ss;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_port = port;
	memcpy(&sin6->sin6_addr, &ina6, sizeof(ina6));

	return (h);
}

int
host_dns(const char *s, const char *tag, const char *cert,
    struct listenerlist *al, int max, in_port_t port, uint8_t flags)
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
		log_warnx("warn: host_dns: could not parse \"%s\": %s", s,
		    gai_strerror(error));
		return (-1);
	}

	for (res = res0; res && cnt < max; res = res->ai_next) {
		if (res->ai_family != AF_INET &&
		    res->ai_family != AF_INET6)
			continue;
		h = xcalloc(1, sizeof(*h), "host_dns");

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
		log_warnx("warn: host_dns: %s resolves to more than %d hosts",
		    s, max);
	}
	freeaddrinfo(res0);
	return (cnt);
}

int
host(const char *s, const char *tag, const char *cert, struct listenerlist *al,
    int max, in_port_t port, const char *authtable, uint8_t flags,
    const char *helo)
{
	struct listener *h;

	port = htons(port);

	h = host_v4(s, port);

	/* IPv6 address? */
	if (h == NULL)
		h = host_v6(s, port);

	if (h != NULL) {
		h->port = port;
		h->flags = flags;
		if (h->flags & F_SSL)
			if (cert == NULL)
				cert = s;
		h->ssl = NULL;
		h->ssl_cert_name[0] = '\0';
		if (authtable != NULL)
			(void)strlcpy(h->authtable, authtable, sizeof(h->authtable));
		if (cert != NULL)
			(void)strlcpy(h->ssl_cert_name, cert, sizeof(h->ssl_cert_name));
		if (tag != NULL)
			(void)strlcpy(h->tag, tag, sizeof(h->tag));
		if (helo != NULL)
			(void)strlcpy(h->helo, helo, sizeof(h->helo));

		TAILQ_INSERT_HEAD(al, h, entry);
		return (1);
	}

	return (host_dns(s, tag, cert, al, max, port, flags));
}

int
interface(const char *s, const char *tag, const char *cert,
    struct listenerlist *al, int max, in_port_t port, const char *authtable, uint8_t flags,
    const char *helo)
{
	struct ifaddrs *ifap, *p;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct listener		*h;
	int ret = 0;

	port = htons(port);

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	for (p = ifap; p != NULL; p = p->ifa_next) {
		if (p->ifa_addr == NULL)
			continue;
		if (strcmp(p->ifa_name, s) != 0 &&
		    ! is_if_in_group(p->ifa_name, s))
			continue;

		h = xcalloc(1, sizeof(*h), "interface");

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
		if (h->flags & F_SSL)
			if (cert == NULL)
				cert = s;
		h->ssl = NULL;
		h->ssl_cert_name[0] = '\0';
		if (authtable != NULL)
			(void)strlcpy(h->authtable, authtable, sizeof(h->authtable));
		if (cert != NULL)
			(void)strlcpy(h->ssl_cert_name, cert, sizeof(h->ssl_cert_name));
		if (tag != NULL)
			(void)strlcpy(h->tag, tag, sizeof(h->tag));
		if (helo != NULL)
			(void)strlcpy(h->helo, helo, sizeof(h->helo));
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
	struct table		*t;

	t = table_create("static", "<anyhost>", NULL);
	table_add(t, "local", NULL);
	table_add(t, "0.0.0.0/0", NULL);
	table_add(t, "::/0", NULL);

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	t = table_create("static", "<localhost>", NULL);
	table_add(t, "local", NULL);

	for (p = ifap; p != NULL; p = p->ifa_next) {
		if (p->ifa_addr == NULL)
			continue;
		switch (p->ifa_addr->sa_family) {
		case AF_INET:
			sain = (struct sockaddr_in *)&ss;
			*sain = *(struct sockaddr_in *)p->ifa_addr;
			sain->sin_len = sizeof(struct sockaddr_in);
			table_add(t, ss_to_text(&ss), NULL);
			break;

		case AF_INET6:
			sin6 = (struct sockaddr_in6 *)&ss;
			*sin6 = *(struct sockaddr_in6 *)p->ifa_addr;
			sin6->sin6_len = sizeof(struct sockaddr_in6);
			table_add(t, ss_to_text(&ss), NULL);
			break;
		}
	}

	freeifaddrs(ifap);
}

int
delaytonum(char *str)
{
	unsigned int     factor;
	size_t           len;
	const char      *errstr = NULL;
	int              delay;
  	
	/* we need at least 1 digit and 1 unit */
	len = strlen(str);
	if (len < 2)
		goto bad;
	
	switch(str[len - 1]) {
		
	case 's':
		factor = 1;
		break;
		
	case 'm':
		factor = 60;
		break;
		
	case 'h':
		factor = 60 * 60;
		break;
		
	case 'd':
		factor = 24 * 60 * 60;
		break;
		
	default:
		goto bad;
	}
  	
	str[len - 1] = '\0';
	delay = strtonum(str, 1, INT_MAX / factor, &errstr);
	if (errstr)
		goto bad;
	
	return (delay * factor);
  	
bad:
	return (-1);
}

int
is_if_in_group(const char *ifname, const char *groupname)
{
        unsigned int		 len;
        struct ifgroupreq        ifgr;
        struct ifg_req          *ifg;
	int			 s;
	int			 ret = 0;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");

        memset(&ifgr, 0, sizeof(ifgr));
        strlcpy(ifgr.ifgr_name, ifname, IFNAMSIZ);
        if (ioctl(s, SIOCGIFGROUP, (caddr_t)&ifgr) == -1) {
                if (errno == EINVAL || errno == ENOTTY)
			goto end;
		err(1, "SIOCGIFGROUP");
        }

        len = ifgr.ifgr_len;
        ifgr.ifgr_groups =
            (struct ifg_req *)xcalloc(len/sizeof(struct ifg_req),
		sizeof(struct ifg_req), "is_if_in_group");
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
	return ret;
}

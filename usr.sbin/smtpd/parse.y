/*	$OpenBSD: parse.y,v 1.148 2014/11/16 19:07:50 bluhm Exp $	*/

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
#include <syslog.h>
#include <unistd.h>
#include <util.h>

#include <openssl/ssl.h>

#include "smtpd.h"
#include "ssl.h"
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
    __attribute__((__format__ (printf, 1, 2)))
    __attribute__((__nonnull__ (1)));

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

struct filter_conf	*filter = NULL;
struct table		*table = NULL;
struct rule		*rule = NULL;
struct listener		 l;
struct mta_limits	*limits;
static struct pki	*pki;

enum listen_options {
	LO_FAMILY	= 0x01,
	LO_PORT		= 0x02,
	LO_SSL		= 0x04,
	LO_FILTER      	= 0x08,
	LO_PKI      	= 0x10,
	LO_AUTH      	= 0x20,
	LO_TAG      	= 0x40,
	LO_HOSTNAME   	= 0x80,
	LO_HOSTNAMES   	= 0x100,
	LO_MASKSOURCE  	= 0x200,
	LO_NODSN	= 0x400,
};

static struct listen_opts {
	char	       *ifx;
	int		family;
	in_port_t	port;
	uint16_t	ssl;
	char	       *filtername;
	char	       *pki;
	uint16_t       	auth;
	struct table   *authtable;
	char	       *tag;
	char	       *hostname;
	struct table   *hostnametable;
	uint16_t	flags;

	uint16_t       	options;
} listen_opts;

static void	create_listener(struct listenerlist *,  struct listen_opts *);
static void	config_listener(struct listener *,  struct listen_opts *);

struct listener	*host_v4(const char *, in_port_t);
struct listener	*host_v6(const char *, in_port_t);
int		 host_dns(struct listenerlist *, struct listen_opts *);
int		 host(struct listenerlist *, struct listen_opts *);
int		 interface(struct listenerlist *, struct listen_opts *);
void		 set_local(const char *);
void		 set_localaddrs(struct table *);
int		 delaytonum(char *);
int		 is_if_in_group(const char *, const char *);

static struct filter_conf *create_filter_proc(char *, char *);
static struct filter_conf *create_filter_chain(char *);
static int add_filter_arg(struct filter_conf *, char *);

typedef struct {
	union {
		int64_t		 number;
		struct table	*table;
		char		*string;
		struct host	*host;
		struct mailaddr	*maddr;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	AS QUEUE COMPRESSION ENCRYPTION MAXMESSAGESIZE MAXMTADEFERRED LISTEN ON ANY PORT EXPIRE
%token	TABLE SECURE SMTPS CERTIFICATE DOMAIN BOUNCEWARN LIMIT INET4 INET6 NODSN
%token  RELAY BACKUP VIA DELIVER TO LMTP MAILDIR MBOX HOSTNAME HOSTNAMES
%token	ACCEPT REJECT INCLUDE ERROR MDA FROM FOR SOURCE MTA PKI SCHEDULER
%token	ARROW AUTH TLS LOCAL VIRTUAL TAG TAGGED ALIAS FILTER KEY CA DHPARAMS
%token	AUTH_OPTIONAL TLS_REQUIRE USERBASE SENDER MASK_SOURCE VERIFY FORWARDONLY RECIPIENT
%token	<v.string>	STRING
%token  <v.number>	NUMBER
%type	<v.table>	table
%type	<v.number>	size negation
%type	<v.table>	tables tablenew tableref alias virtual userbase
%type	<v.string>	tagged
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

tagged		: TAGGED negation STRING       		{
			if (strlcpy(rule->r_tag, $3, sizeof rule->r_tag)
			    >= sizeof rule->r_tag) {
				yyerror("tag name too long: %s", $3);
				free($3);
				YYERROR;
			}
			free($3);
			rule->r_nottag = $2;
		}
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

opt_limit_mda	: STRING NUMBER {
			if (!strcmp($1, "max-session")) {
				conf->sc_mda_max_session = $2;
			}
			else if (!strcmp($1, "max-session-per-user")) {
				conf->sc_mda_max_user_session = $2;
			}
			else if (!strcmp($1, "task-lowat")) {
				conf->sc_mda_task_lowat = $2;
			}
			else if (!strcmp($1, "task-hiwat")) {
				conf->sc_mda_task_hiwat = $2;
			}
			else if (!strcmp($1, "task-release")) {
				conf->sc_mda_task_release = $2;
			}
			else {
				yyerror("invalid scheduler limit keyword: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

limits_mda	: opt_limit_mda limits_mda
		| /* empty */
		;

opt_limit_mta	: INET4 {
			limits->family = AF_INET;
		}
		| INET6 {
			limits->family = AF_INET6;
		}
		| STRING NUMBER {
			if (!limit_mta_set(limits, $1, $2)) {
				yyerror("invalid mta limit keyword: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

limits_mta	: opt_limit_mta limits_mta
		| /* empty */
		;

opt_limit_scheduler : STRING NUMBER {
			if (!strcmp($1, "max-inflight")) {
				conf->sc_scheduler_max_inflight = $2;
			}
			else if (!strcmp($1, "max-evp-batch-size")) {
				conf->sc_scheduler_max_evp_batch_size = $2;
			}
			else if (!strcmp($1, "max-msg-batch-size")) {
				conf->sc_scheduler_max_msg_batch_size = $2;
			}
			else if (!strcmp($1, "max-schedule")) {
				conf->sc_scheduler_max_schedule = $2;
			}
			else {
				yyerror("invalid scheduler limit keyword: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

limits_scheduler: opt_limit_scheduler limits_scheduler
		| /* empty */
		;

opt_pki		: CERTIFICATE STRING {
			pki->pki_cert_file = $2;
		}
		| KEY STRING {
			pki->pki_key_file = $2;
		}
		| CA STRING {
			pki->pki_ca_file = $2;
		}
		| DHPARAMS STRING {
			pki->pki_dhparams_file = $2;
		}
		;

pki		: opt_pki pki
		| /* empty */
		;

opt_listen     	: INET4			{
			if (listen_opts.options & LO_FAMILY) {
				yyerror("address family already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_FAMILY;
			listen_opts.family = AF_INET;
		}
		| INET6			{
			if (listen_opts.options & LO_FAMILY) {
				yyerror("address family already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_FAMILY;
			listen_opts.family = AF_INET6;
		}
		| PORT STRING			{
			struct servent	*servent;

			if (listen_opts.options & LO_PORT) {
				yyerror("port already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_PORT;

			servent = getservbyname($2, "tcp");
			if (servent == NULL) {
				yyerror("invalid port: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
			listen_opts.port = ntohs(servent->s_port);
		}
		| PORT NUMBER			{
			if (listen_opts.options & LO_PORT) {
				yyerror("port already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_PORT;

			if ($2 <= 0 || $2 >= (int)USHRT_MAX) {
				yyerror("invalid port: %" PRId64, $2);
				YYERROR;
			}
			listen_opts.port = $2;
		}
		| FILTER STRING			{
			if (listen_opts.options & LO_FILTER) {
				yyerror("filter already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_FILTER;
			listen_opts.filtername = $2;
		}
		| SMTPS				{
			if (listen_opts.options & LO_SSL) {
				yyerror("TLS mode already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_SSL;
			listen_opts.ssl = F_SMTPS;
		}
		| SMTPS VERIFY 			{
			if (listen_opts.options & LO_SSL) {
				yyerror("TLS mode already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_SSL;
			listen_opts.ssl = F_SMTPS|F_TLS_VERIFY;
		}
		| TLS				{
			if (listen_opts.options & LO_SSL) {
				yyerror("TLS mode already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_SSL;
			listen_opts.ssl = F_STARTTLS;
		}
		| SECURE       			{
			if (listen_opts.options & LO_SSL) {
				yyerror("TLS mode already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_SSL;
			listen_opts.ssl = F_SSL;
		}
		| TLS_REQUIRE			{
			if (listen_opts.options & LO_SSL) {
				yyerror("TLS mode already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_SSL;
			listen_opts.ssl = F_STARTTLS|F_STARTTLS_REQUIRE;
		}
		| TLS_REQUIRE VERIFY   		{
			if (listen_opts.options & LO_SSL) {
				yyerror("TLS mode already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_SSL;
			listen_opts.ssl = F_STARTTLS|F_STARTTLS_REQUIRE|F_TLS_VERIFY;
		}
		| PKI STRING			{
			if (listen_opts.options & LO_PKI) {
				yyerror("pki already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_PKI;
			listen_opts.pki = $2;
		}
		| AUTH				{
			if (listen_opts.options & LO_AUTH) {
				yyerror("auth already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_AUTH;
			listen_opts.auth = F_AUTH|F_AUTH_REQUIRE;
		}
		| AUTH_OPTIONAL			{
			if (listen_opts.options & LO_AUTH) {
				yyerror("auth already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_AUTH;
			listen_opts.auth = F_AUTH;
		}
		| AUTH tables  			{
			if (listen_opts.options & LO_AUTH) {
				yyerror("auth already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_AUTH;
			listen_opts.authtable = $2;
			listen_opts.auth = F_AUTH|F_AUTH_REQUIRE;
		}
		| AUTH_OPTIONAL tables 		{
			if (listen_opts.options & LO_AUTH) {
				yyerror("auth already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_AUTH;
			listen_opts.authtable = $2;
			listen_opts.auth = F_AUTH;
		}
		| TAG STRING			{
			if (listen_opts.options & LO_TAG) {
				yyerror("tag already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_TAG;

       			if (strlen($2) >= MAX_TAG_SIZE) {
       				yyerror("tag name too long");
				free($2);
				YYERROR;
			}
			listen_opts.tag = $2;
		}
		| HOSTNAME STRING	{
			if (listen_opts.options & LO_HOSTNAME) {
				yyerror("hostname already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_HOSTNAME;

			listen_opts.hostname = $2;
		}
		| HOSTNAMES tables	{
			struct table	*t = $2;

			if (listen_opts.options & LO_HOSTNAMES) {
				yyerror("hostnames already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_HOSTNAMES;

			if (! table_check_use(t, T_DYNAMIC|T_HASH, K_ADDRNAME)) {
				yyerror("invalid use of table \"%s\" as "
				    "HOSTNAMES parameter", t->t_name);
				YYERROR;
			}
			listen_opts.hostnametable = t;
		}
		| MASK_SOURCE	{
			if (listen_opts.options & LO_MASKSOURCE) {
				yyerror("mask-source already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_MASKSOURCE;
			listen_opts.flags |= F_MASK_SOURCE;
		}
		| NODSN	{
			if (listen_opts.options & LO_NODSN) {
				yyerror("no-dsn already specified");
				YYERROR;	
			}
			listen_opts.options |= LO_NODSN;
			listen_opts.flags &= ~F_EXT_DSN;
		}
		;

listen		: opt_listen listen
		| /* empty */
		;

opt_relay_common: AS STRING	{
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
			rule->r_as = xmemdup(&maddr, sizeof (*maddrp), "parse relay_as: AS");
		}
		| SOURCE tables			{
			struct table	*t = $2;
			if (! table_check_use(t, T_DYNAMIC|T_LIST, K_SOURCE)) {
				yyerror("invalid use of table \"%s\" as "
				    "SOURCE parameter", t->t_name);
				YYERROR;
			}
			(void)strlcpy(rule->r_value.relayhost.sourcetable, t->t_name,
			    sizeof rule->r_value.relayhost.sourcetable);
		}
		| HOSTNAME STRING {
			(void)strlcpy(rule->r_value.relayhost.heloname, $2,
			    sizeof rule->r_value.relayhost.heloname);
			free($2);
		}
		| HOSTNAMES tables		{
			struct table	*t = $2;
			if (! table_check_use(t, T_DYNAMIC|T_HASH, K_ADDRNAME)) {
				yyerror("invalid use of table \"%s\" as "
				    "HOSTNAMES parameter", t->t_name);
				YYERROR;
			}
			(void)strlcpy(rule->r_value.relayhost.helotable, t->t_name,
			    sizeof rule->r_value.relayhost.helotable);
		}
		| PKI STRING {
			if (! lowercase(rule->r_value.relayhost.pki_name, $2,
				sizeof(rule->r_value.relayhost.pki_name))) {
				yyerror("pki name too long: %s", $2);
				free($2);
				YYERROR;
			}
			if (dict_get(conf->sc_pki_dict,
			    rule->r_value.relayhost.pki_name) == NULL) {
				log_warnx("pki name not found: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
		}
		;

opt_relay	: BACKUP STRING			{
			rule->r_value.relayhost.flags |= F_BACKUP;
			if (strlcpy(rule->r_value.relayhost.hostname, $2,
				sizeof (rule->r_value.relayhost.hostname))
			    >= sizeof (rule->r_value.relayhost.hostname)) {
				log_warnx("hostname too long: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
		}
		| BACKUP       			{
			rule->r_value.relayhost.flags |= F_BACKUP;
			(void)strlcpy(rule->r_value.relayhost.hostname,
			    conf->sc_hostname,
			    sizeof (rule->r_value.relayhost.hostname));
		}
		| TLS       			{
			rule->r_value.relayhost.flags |= F_STARTTLS;
		}
		| TLS VERIFY			{
			rule->r_value.relayhost.flags |= F_STARTTLS|F_TLS_VERIFY;
		}
		;

relay		: opt_relay_common relay
		| opt_relay relay
		| /* empty */
		;

opt_relay_via	: AUTH tables {
			struct table   *t = $2;

			if (! table_check_use(t, T_DYNAMIC|T_HASH, K_CREDENTIALS)) {
				yyerror("invalid use of table \"%s\" as AUTH parameter",
				    t->t_name);
				YYERROR;
			}
			(void)strlcpy(rule->r_value.relayhost.authtable, t->t_name,
			    sizeof(rule->r_value.relayhost.authtable));
		}
		| VERIFY {
			if (!(rule->r_value.relayhost.flags & F_SSL)) {
				yyerror("cannot \"verify\" with insecure protocol");
				YYERROR;
			}
			rule->r_value.relayhost.flags |= F_TLS_VERIFY;
		}
		;

relay_via	: opt_relay_common relay_via
		| opt_relay_via relay_via
		| /* empty */
		;

main		: BOUNCEWARN {
			memset(conf->sc_bounce_warn, 0, sizeof conf->sc_bounce_warn);
		} bouncedelays
		| QUEUE COMPRESSION {
			conf->sc_queue_flags |= QUEUE_COMPRESSION;
		}
		| QUEUE ENCRYPTION {
			conf->sc_queue_flags |= QUEUE_ENCRYPTION;
		}
		| QUEUE ENCRYPTION KEY STRING {
			if (strcasecmp($4, "stdin") == 0 || strcasecmp($4, "-") == 0) {
				conf->sc_queue_key = "stdin";
				free($4);
			}
			else
				conf->sc_queue_key = $4;
			conf->sc_queue_flags |= QUEUE_ENCRYPTION;
		}
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
		| MAXMTADEFERRED NUMBER  {
			conf->sc_mta_max_deferred = $2;
		}
		| LIMIT MDA limits_mda
		| LIMIT MTA FOR DOMAIN STRING {
			struct mta_limits	*d;

			limits = dict_get(conf->sc_limits_dict, $5);
			if (limits == NULL) {
				limits = xcalloc(1, sizeof(*limits), "mta_limits");
				dict_xset(conf->sc_limits_dict, $5, limits);
				d = dict_xget(conf->sc_limits_dict, "default");
				memmove(limits, d, sizeof(*limits));
			}
			free($5);
		} limits_mta
		| LIMIT MTA {
			limits = dict_get(conf->sc_limits_dict, "default");
		} limits_mta
		| LIMIT SCHEDULER limits_scheduler
		| LISTEN {
			memset(&l, 0, sizeof l);
			memset(&listen_opts, 0, sizeof listen_opts);
			listen_opts.family = AF_UNSPEC;
			listen_opts.flags |= F_EXT_DSN;
		} ON STRING listen {
			listen_opts.ifx = $4;
			create_listener(conf->sc_listeners, &listen_opts);
		}
		| FILTER STRING STRING {
			if (!strcmp($3, "chain")) {
				free($3);
				if ((filter = create_filter_chain($2)) == NULL) {
					free($2);
					YYERROR;
				}
			}
			else {
				if ((filter = create_filter_proc($2, $3)) == NULL) {
					free($2);
					free($3);
					YYERROR;
				}
			}
		} filter_args;
		| PKI STRING	{
			char buf[MAXHOSTNAMELEN];
			xlowercase(buf, $2, sizeof(buf));
			free($2);
			pki = dict_get(conf->sc_pki_dict, buf);
			if (pki == NULL) {
				pki = xcalloc(1, sizeof *pki, "parse:pki");
				(void)strlcpy(pki->pki_name, buf, sizeof(pki->pki_name));
				dict_set(conf->sc_pki_dict, pki->pki_name, pki);
			}
		} pki
		;

filter_args	:
		| STRING {
			if (!add_filter_arg(filter, $1)) {
				free($1);
				YYERROR;
			}
		} filter_args
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
			table = table_create(backend, $2, NULL, config);
			if (!table_config(table)) {
				yyerror("invalid configuration file %s for table %s",
				    config, table->t_name);
				free($2);
				free($3);
				YYERROR;
			}
			free($2);
			free($3);
		}
		| TABLE STRING {
			table = table_create("static", $2, NULL, NULL);
			free($2);
		} '{' tableval_list '}' {
			table = NULL;
		}
		;

assign		: '=' | ARROW;

keyval		: STRING assign STRING		{
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

			t = table_create("static", NULL, NULL, NULL);
			t->t_type = T_LIST;
			table_add(t, $1, NULL);
			free($1);
			$$ = t;
		}
		| '{'				{
			table = table_create("static", NULL, NULL, NULL);
		} tableval_list '}'		{
			$$ = table;
		}
		;

tableref       	: '<' STRING '>'       		{
			struct table	*t;

			if ((t = table_find($2, NULL)) == NULL) {
				yyerror("no such table: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
			$$ = t;
		}
		;

tables		: tablenew			{ $$ = $1; }
		| tableref			{ $$ = $1; }
		;

alias		: ALIAS tables			{
			struct table   *t = $2;

			if (! table_check_use(t, T_DYNAMIC|T_HASH, K_ALIAS)) {
				yyerror("invalid use of table \"%s\" as ALIAS parameter",
				    t->t_name);
				YYERROR;
			}

			$$ = t;
		}
		;

virtual		: VIRTUAL tables		{
			struct table   *t = $2;

			if (! table_check_use(t, T_DYNAMIC|T_HASH, K_ALIAS)) {
				yyerror("invalid use of table \"%s\" as VIRTUAL parameter",
				    t->t_name);
				YYERROR;
			}
			$$ = t;
		}
		;

usermapping	: alias		{
			if (rule->r_mapping) {
				yyerror("alias specified multiple times");
				YYERROR;
			}
			rule->r_desttype = DEST_DOM;
			rule->r_mapping = $1;
		}
		| virtual	{
			if (rule->r_mapping) {
				yyerror("virtual specified multiple times");
				YYERROR;
			}
			rule->r_desttype = DEST_VDOM;
			rule->r_mapping = $1;
		}
		;

userbase	: USERBASE tables	{
			struct table   *t = $2;

			if (rule->r_userbase) {
				yyerror("userbase specified multiple times");
				YYERROR;
			}
			if (! table_check_use(t, T_DYNAMIC|T_HASH, K_USERINFO)) {
				yyerror("invalid use of table \"%s\" as USERBASE parameter",
				    t->t_name);
				YYERROR;
			}
			rule->r_userbase = t;
		}
		;

deliver_action	: DELIVER TO MAILDIR			{
			rule->r_action = A_MAILDIR;
			if (strlcpy(rule->r_value.buffer, "~/Maildir",
			    sizeof(rule->r_value.buffer)) >=
			    sizeof(rule->r_value.buffer))
				fatal("pathname too long");
		}
		| DELIVER TO MAILDIR STRING		{
			rule->r_action = A_MAILDIR;
			if (strlcpy(rule->r_value.buffer, $4,
			    sizeof(rule->r_value.buffer)) >=
			    sizeof(rule->r_value.buffer))
				fatal("pathname too long");
			free($4);
		}
		| DELIVER TO MBOX			{
			rule->r_action = A_MBOX;
			if (strlcpy(rule->r_value.buffer, _PATH_MAILDIR "/%u",
			    sizeof(rule->r_value.buffer))
			    >= sizeof(rule->r_value.buffer))
				fatal("pathname too long");
		}
		| DELIVER TO LMTP STRING		{
			rule->r_action = A_LMTP;
			if (strchr($4, ':') || $4[0] == '/') {
				if (strlcpy(rule->r_value.buffer, $4,
					sizeof(rule->r_value.buffer))
					>= sizeof(rule->r_value.buffer))
					fatal("lmtp destination too long");
			} else
				fatal("invalid lmtp destination");
			free($4);
		}
		| DELIVER TO MDA STRING			{
			rule->r_action = A_MDA;
			if (strlcpy(rule->r_value.buffer, $4,
			    sizeof(rule->r_value.buffer))
			    >= sizeof(rule->r_value.buffer))
				fatal("command too long");
			free($4);
		}
		;

relay_action   	: RELAY relay {
			rule->r_action = A_RELAY;
		}
		| RELAY VIA STRING {
			rule->r_action = A_RELAYVIA;
			if (! text_to_relayhost(&rule->r_value.relayhost, $3)) {
				yyerror("error: invalid url: %s", $3);
				free($3);
				YYERROR;
			}
			free($3);
		} relay_via {
			/* no worries, F_AUTH cant be set without SSL */
			if (rule->r_value.relayhost.flags & F_AUTH) {
				if (rule->r_value.relayhost.authtable[0] == '\0') {
					yyerror("error: auth without auth table");
					YYERROR;
				}
			}
		}
		;

negation	: '!'		{ $$ = 1; }
		| /* empty */	{ $$ = 0; }
		;

from		: FROM negation SOURCE tables       		{
			struct table   *t = $4;

			if (rule->r_sources) {
				yyerror("from specified multiple times");
				YYERROR;
			}
			if (! table_check_use(t, T_DYNAMIC|T_LIST, K_NETADDR)) {
				yyerror("invalid use of table \"%s\" as FROM parameter",
				    t->t_name);
				YYERROR;
			}
			rule->r_notsources = $2;
			rule->r_sources = t;
		}
		| FROM negation ANY    		{
			if (rule->r_sources) {
				yyerror("from specified multiple times");
				YYERROR;
			}
			rule->r_sources = table_find("<anyhost>", NULL);
			rule->r_notsources = $2;
		}
		| FROM negation LOCAL  		{
			if (rule->r_sources) {
				yyerror("from specified multiple times");
				YYERROR;
			}
			rule->r_sources = table_find("<localhost>", NULL);
			rule->r_notsources = $2;
		}
		;

for		: FOR negation DOMAIN tables {
			struct table   *t = $4;

			if (rule->r_destination) {
				yyerror("for specified multiple times");
				YYERROR;
			}
			if (! table_check_use(t, T_DYNAMIC|T_LIST, K_DOMAIN)) {
				yyerror("invalid use of table \"%s\" as DOMAIN parameter",
				    t->t_name);
				YYERROR;
			}
			rule->r_notdestination = $2;
			rule->r_destination = t;
		}
		| FOR negation ANY    		{
			if (rule->r_destination) {
				yyerror("for specified multiple times");
				YYERROR;
			}
			rule->r_notdestination = $2;
			rule->r_destination = table_find("<anydestination>", NULL);
		}
		| FOR negation LOCAL  		{
			if (rule->r_destination) {
				yyerror("for specified multiple times");
				YYERROR;
			}
			rule->r_notdestination = $2;
			rule->r_destination = table_find("<localnames>", NULL);
		}
		;

sender		: SENDER negation tables			{
			struct table   *t = $3;

			if (rule->r_senders) {
				yyerror("sender specified multiple times");
				YYERROR;
			}

			if (! table_check_use(t, T_DYNAMIC|T_LIST, K_MAILADDR)) {
				yyerror("invalid use of table \"%s\" as SENDER parameter",
				    t->t_name);
				YYERROR;
			}
			rule->r_notsenders = $2;
			rule->r_senders = t;
		}
		;

recipient      	: RECIPIENT negation tables			{
			struct table   *t = $3;

			if (rule->r_recipients) {
				yyerror("recipient specified multiple times");
				YYERROR;
			}

			if (! table_check_use(t, T_DYNAMIC|T_LIST, K_MAILADDR)) {
				yyerror("invalid use of table \"%s\" as RECIPIENT parameter",
				    t->t_name);
				YYERROR;
			}
			rule->r_notrecipients = $2;
			rule->r_recipients = t;
		}
		;

forwardonly	: FORWARDONLY {
			if (rule->r_forwardonly) {
				yyerror("forward-only specified multiple times");
				YYERROR;
			}
			rule->r_forwardonly = 1;
		}
		;

expire		: EXPIRE STRING {
			if (rule->r_qexpire != -1) {
				yyerror("expire specified multiple times");
				YYERROR;
			}
			rule->r_qexpire = delaytonum($2);
			if (rule->r_qexpire == -1) {
				yyerror("invalid expire delay: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
		}
		;

opt_decision	: sender
		| recipient
		| from
		| for
		| tagged
		;
decision	: opt_decision decision
		|
		;

opt_lookup	: userbase
		| usermapping
		;
lookup		: opt_lookup lookup
		|
		;

action		: deliver_action
		| relay_action
		|
		;

opt_accept	: expire
		| forwardonly
		;

accept_params	: opt_accept accept_params
		|
		;

rule		: ACCEPT {
			rule = xcalloc(1, sizeof(*rule), "parse rule: ACCEPT");
			rule->r_action = A_NONE;
			rule->r_decision = R_ACCEPT;
			rule->r_desttype = DEST_DOM;
			rule->r_qexpire = -1;
		} decision lookup action accept_params {
			if (! rule->r_sources)
				rule->r_sources = table_find("<localhost>", NULL);			
			if (! rule->r_destination)
			 	rule->r_destination = table_find("<localnames>", NULL);
			if (! rule->r_userbase)
				rule->r_userbase = table_find("<getpwnam>", NULL);
			if (rule->r_qexpire == -1)
				rule->r_qexpire = conf->sc_qexpire;
			if (rule->r_action == A_RELAY || rule->r_action == A_RELAYVIA) {
				if (rule->r_userbase != table_find("<getpwnam>", NULL)) {
					yyerror("userbase may not be used with a relay rule");
					YYERROR;
				}
				if (rule->r_mapping) {
					yyerror("aliases/virtual may not be used with a relay rule");
					YYERROR;
				}
			}
			if (rule->r_forwardonly && rule->r_action != A_NONE) {
				yyerror("forward-only may not be used with a default action");
				YYERROR;
			}
			TAILQ_INSERT_TAIL(conf->sc_rules, rule, r_entry);
			rule = NULL;
		}
		| REJECT {
			rule = xcalloc(1, sizeof(*rule), "parse rule: REJECT");
			rule->r_decision = R_REJECT;
			rule->r_desttype = DEST_DOM;
		} decision {
			if (! rule->r_sources)
				rule->r_sources = table_find("<localhost>", NULL);
			if (! rule->r_destination)
				rule->r_destination = table_find("<localnames>", NULL);
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
		{ "accept",		ACCEPT },
		{ "alias",		ALIAS },
		{ "any",		ANY },
		{ "as",			AS },
		{ "auth",		AUTH },
		{ "auth-optional",     	AUTH_OPTIONAL },
		{ "backup",		BACKUP },
		{ "bounce-warn",	BOUNCEWARN },
		{ "ca",			CA },
		{ "certificate",	CERTIFICATE },
		{ "compression",	COMPRESSION },
		{ "deliver",		DELIVER },
		{ "dhparams",		DHPARAMS },
		{ "domain",		DOMAIN },
		{ "encryption",		ENCRYPTION },
		{ "expire",		EXPIRE },
		{ "filter",		FILTER },
		{ "for",		FOR },
		{ "forward-only",      	FORWARDONLY },
		{ "from",		FROM },
		{ "hostname",		HOSTNAME },
		{ "hostnames",		HOSTNAMES },
		{ "include",		INCLUDE },
		{ "inet4",		INET4 },
		{ "inet6",		INET6 },
		{ "key",		KEY },
		{ "limit",		LIMIT },
		{ "listen",		LISTEN },
		{ "lmtp",		LMTP },
		{ "local",		LOCAL },
		{ "maildir",		MAILDIR },
		{ "mask-source",	MASK_SOURCE },
		{ "max-message-size",  	MAXMESSAGESIZE },
		{ "max-mta-deferred",  	MAXMTADEFERRED },
		{ "mbox",		MBOX },
		{ "mda",		MDA },
		{ "mta",		MTA },
		{ "no-dsn",		NODSN },
		{ "on",			ON },
		{ "pki",		PKI },
		{ "port",		PORT },
		{ "queue",		QUEUE },
		{ "recipient",		RECIPIENT },
		{ "reject",		REJECT },
		{ "relay",		RELAY },
		{ "scheduler",		SCHEDULER },
		{ "secure",		SECURE },
		{ "sender",    		SENDER },
		{ "smtps",		SMTPS },
		{ "source",		SOURCE },
		{ "table",		TABLE },
		{ "tag",		TAG },
		{ "tagged",		TAGGED },
		{ "tls",		TLS },
		{ "tls-require",       	TLS_REQUIRE },
		{ "to",			TO },
		{ "userbase",		USERBASE },
		{ "verify",		VERIFY },
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
	if (st.st_mode & (S_IWGRP | S_IXGRP | S_IRWXO)) {
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
	char		hostname[SMTPD_MAXHOSTNAMELEN];
	char		hostname_copy[SMTPD_MAXHOSTNAMELEN];

	if (! getmailname(hostname, sizeof hostname))
		return (-1);

	conf = x_conf;
	memset(conf, 0, sizeof(*conf));

	(void)strlcpy(conf->sc_hostname, hostname, sizeof(conf->sc_hostname));

	conf->sc_maxsize = DEFAULT_MAX_BODY_SIZE;

	conf->sc_tables_dict = calloc(1, sizeof(*conf->sc_tables_dict));
	conf->sc_rules = calloc(1, sizeof(*conf->sc_rules));
	conf->sc_listeners = calloc(1, sizeof(*conf->sc_listeners));
	conf->sc_pki_dict = calloc(1, sizeof(*conf->sc_pki_dict));
	conf->sc_ssl_dict = calloc(1, sizeof(*conf->sc_ssl_dict));
	conf->sc_limits_dict = calloc(1, sizeof(*conf->sc_limits_dict));

	/* Report mails delayed for more than 4 hours */
	conf->sc_bounce_warn[0] = 3600 * 4;

	if (conf->sc_tables_dict == NULL	||
	    conf->sc_rules == NULL		||
	    conf->sc_listeners == NULL		||
	    conf->sc_pki_dict == NULL		||
	    conf->sc_limits_dict == NULL) {
		log_warn("warn: cannot allocate memory");
		free(conf->sc_tables_dict);
		free(conf->sc_rules);
		free(conf->sc_listeners);
		free(conf->sc_pki_dict);
		free(conf->sc_ssl_dict);
		free(conf->sc_limits_dict);
		return (-1);
	}

	errors = 0;

	table = NULL;
	rule = NULL;

	dict_init(&conf->sc_filters);

	dict_init(conf->sc_pki_dict);
	dict_init(conf->sc_ssl_dict);
	dict_init(conf->sc_tables_dict);

	dict_init(conf->sc_limits_dict);
	limits = xcalloc(1, sizeof(*limits), "mta_limits");
	limit_mta_set_defaults(limits);
	dict_xset(conf->sc_limits_dict, "default", limits);

	TAILQ_INIT(conf->sc_listeners);
	TAILQ_INIT(conf->sc_rules);

	conf->sc_qexpire = SMTPD_QUEUE_EXPIRY;
	conf->sc_opts = opts;

	conf->sc_mta_max_deferred = 100;
	conf->sc_scheduler_max_inflight = 5000;
	conf->sc_scheduler_max_schedule = 10;
	conf->sc_scheduler_max_evp_batch_size = 256;
	conf->sc_scheduler_max_msg_batch_size = 1024;

	conf->sc_mda_max_session = 50;
	conf->sc_mda_max_user_session = 7;
	conf->sc_mda_task_hiwat = 50;
	conf->sc_mda_task_lowat = 30;
	conf->sc_mda_task_release = 10;

	if ((file = pushfile(filename, 0)) == NULL) {
		purge_config(PURGE_EVERYTHING);
		return (-1);
	}
	topfile = file;

	/*
	 * declare special "localhost", "anyhost" and "localnames" tables
	 */
	set_local(hostname);

	t = table_create("static", "<anydestination>", NULL, NULL);
	t->t_type = T_LIST;
	table_add(t, "*", NULL);

	/* can't truncate here */
	(void)strlcpy(hostname_copy, hostname, sizeof hostname_copy);

	hostname_copy[strcspn(hostname_copy, ".")] = '\0';
	if (strcmp(hostname, hostname_copy) != 0)
		table_add(t, hostname_copy, NULL);

	table_create("getpwnam", "<getpwnam>", NULL, NULL);

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

static void
create_listener(struct listenerlist *ll,  struct listen_opts *lo)
{
	uint16_t	flags;

	if (lo->port != 0 && lo->ssl == F_SSL)
		errx(1, "invalid listen option: tls/smtps on same port");
	
	if (lo->auth != 0 && !lo->ssl)
		errx(1, "invalid listen option: auth requires tls/smtps");
	
	if (lo->pki && !lo->ssl)
		errx(1, "invalid listen option: pki requires tls/smtps");
	
	flags = lo->flags;

	if (lo->port) {
		lo->flags = lo->ssl|lo->auth|flags;
		lo->port = htons(lo->port);
		if (! interface(ll, lo))
			if (host(ll, lo) <= 0)
				errx(1, "invalid virtual ip or interface: %s", lo->ifx);
	}
	else {
		if (lo->ssl & F_SMTPS) {
			lo->port = htons(465);
			lo->flags = F_SMTPS|lo->auth|flags;
			if (! interface(ll, lo))
				if (host(ll, lo) <= 0)
					errx(1, "invalid virtual ip or interface: %s", lo->ifx);
		}

		if (! lo->ssl || (lo->ssl & F_STARTTLS)) {
			lo->port = htons(25);
			lo->flags = lo->auth|flags;
			if (lo->ssl & F_STARTTLS)
				lo->flags |= F_STARTTLS;
			if (! interface(ll, lo))
				if (host(ll, lo) <= 0)
					errx(1, "invalid virtual ip or interface: %s", lo->ifx);
		}
	}
}

static void
config_listener(struct listener *h,  struct listen_opts *lo)
{
	h->fd = -1;
	h->port = lo->port;
	h->flags = lo->flags;

	if (lo->hostname == NULL)
		lo->hostname = conf->sc_hostname;

	if (lo->filtername) {
		if (dict_get(&conf->sc_filters, lo->filtername) == NULL) {
			log_warnx("undefined filter: %s", lo->filtername);
			fatalx(NULL);
		}
		(void)strlcpy(h->filter, lo->filtername, sizeof(h->filter));
	}

	h->pki_name[0] = '\0';

	if (lo->authtable != NULL)
		(void)strlcpy(h->authtable, lo->authtable->t_name, sizeof(h->authtable));
	if (lo->pki != NULL) {
		if (! lowercase(h->pki_name, lo->pki, sizeof(h->pki_name))) {
			log_warnx("pki name too long: %s", lo->pki);
			fatalx(NULL);
		}
		if (dict_get(conf->sc_pki_dict, h->pki_name) == NULL) {
			log_warnx("pki name not found: %s", lo->pki);
			fatalx(NULL);
		}
	}
	if (lo->tag != NULL)
		(void)strlcpy(h->tag, lo->tag, sizeof(h->tag));

	(void)strlcpy(h->hostname, lo->hostname, sizeof(h->hostname));
	if (lo->hostnametable)
		(void)strlcpy(h->hostnametable, lo->hostnametable->t_name, sizeof(h->hostnametable));

	if (lo->ssl & F_TLS_VERIFY)
		h->flags |= F_TLS_VERIFY;
}

struct listener *
host_v4(const char *s, in_port_t port)
{
	struct in_addr		 ina;
	struct sockaddr_in	*sain;
	struct listener		*h;

	memset(&ina, 0, sizeof(ina));
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

	memset(&ina6, 0, sizeof(ina6));
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
host_dns(struct listenerlist *al, struct listen_opts *lo)
{
	struct addrinfo		 hints, *res0, *res;
	int			 error, cnt = 0;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct listener		*h;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(lo->ifx, NULL, &hints, &res0);
	if (error == EAI_AGAIN || error == EAI_NODATA || error == EAI_NONAME)
		return (0);
	if (error) {
		log_warnx("warn: host_dns: could not parse \"%s\": %s", lo->ifx,
		    gai_strerror(error));
		return (-1);
	}

	for (res = res0; res; res = res->ai_next) {
		if (res->ai_family != AF_INET &&
		    res->ai_family != AF_INET6)
			continue;
		h = xcalloc(1, sizeof(*h), "host_dns");

		h->ss.ss_family = res->ai_family;
		if (res->ai_family == AF_INET) {
			sain = (struct sockaddr_in *)&h->ss;
			sain->sin_len = sizeof(struct sockaddr_in);
			sain->sin_addr.s_addr = ((struct sockaddr_in *)
			    res->ai_addr)->sin_addr.s_addr;
			sain->sin_port = lo->port;
		} else {
			sin6 = (struct sockaddr_in6 *)&h->ss;
			sin6->sin6_len = sizeof(struct sockaddr_in6);
			memcpy(&sin6->sin6_addr, &((struct sockaddr_in6 *)
			    res->ai_addr)->sin6_addr, sizeof(struct in6_addr));
			sin6->sin6_port = lo->port;
		}

		config_listener(h, lo);

		TAILQ_INSERT_HEAD(al, h, entry);
		cnt++;
	}

	freeaddrinfo(res0);
	return (cnt);
}

int
host(struct listenerlist *al, struct listen_opts *lo)
{
	struct listener *h;

	h = host_v4(lo->ifx, lo->port);

	/* IPv6 address? */
	if (h == NULL)
		h = host_v6(lo->ifx, lo->port);

	if (h != NULL) {
		config_listener(h, lo);
		TAILQ_INSERT_HEAD(al, h, entry);
		return (1);
	}

	return (host_dns(al, lo));
}

int
interface(struct listenerlist *al, struct listen_opts *lo)
{
	struct ifaddrs *ifap, *p;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct listener		*h;
	int			ret = 0;

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	for (p = ifap; p != NULL; p = p->ifa_next) {
		if (p->ifa_addr == NULL)
			continue;
		if (strcmp(p->ifa_name, lo->ifx) != 0 &&
		    ! is_if_in_group(p->ifa_name, lo->ifx))
			continue;
		if (lo->family != AF_UNSPEC && lo->family != p->ifa_addr->sa_family)
			continue;

		h = xcalloc(1, sizeof(*h), "interface");

		switch (p->ifa_addr->sa_family) {
		case AF_INET:
			sain = (struct sockaddr_in *)&h->ss;
			*sain = *(struct sockaddr_in *)p->ifa_addr;
			sain->sin_len = sizeof(struct sockaddr_in);
			sain->sin_port = lo->port;
			break;

		case AF_INET6:
			sin6 = (struct sockaddr_in6 *)&h->ss;
			*sin6 = *(struct sockaddr_in6 *)p->ifa_addr;
			sin6->sin6_len = sizeof(struct sockaddr_in6);
			sin6->sin6_port = lo->port;
			break;

		default:
			free(h);
			continue;
		}

		config_listener(h, lo);
		ret = 1;
		TAILQ_INSERT_HEAD(al, h, entry);
	}

	freeifaddrs(ifap);

	return ret;
}

void
set_local(const char *hostname)
{
	struct table	*t;

	t = table_create("static", "<localnames>", NULL, NULL);
	t->t_type = T_LIST;
	table_add(t, "localhost", NULL);
	table_add(t, hostname, NULL);

	set_localaddrs(t);
}

void
set_localaddrs(struct table *localnames)
{
	struct ifaddrs *ifap, *p;
	struct sockaddr_storage ss;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct table		*t;
	char buf[NI_MAXHOST + 5];

	t = table_create("static", "<anyhost>", NULL, NULL);
	table_add(t, "local", NULL);
	table_add(t, "0.0.0.0/0", NULL);
	table_add(t, "::/0", NULL);

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	t = table_create("static", "<localhost>", NULL, NULL);
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
			table_add(localnames, ss_to_text(&ss), NULL);
			(void)snprintf(buf, sizeof buf, "[%s]", ss_to_text(&ss));
			table_add(localnames, buf, NULL);
			break;

		case AF_INET6:
			sin6 = (struct sockaddr_in6 *)&ss;
			*sin6 = *(struct sockaddr_in6 *)p->ifa_addr;
			sin6->sin6_len = sizeof(struct sockaddr_in6);
			table_add(t, ss_to_text(&ss), NULL);
			table_add(localnames, ss_to_text(&ss), NULL);
			(void)snprintf(buf, sizeof buf, "[%s]", ss_to_text(&ss));
			table_add(localnames, buf, NULL);
			(void)snprintf(buf, sizeof buf, "[ipv6:%s]", ss_to_text(&ss));
			table_add(localnames, buf, NULL);
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
        if (strlcpy(ifgr.ifgr_name, ifname, IFNAMSIZ) >= IFNAMSIZ)
		errx(1, "interface name too large");

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

static struct filter_conf *
create_filter_proc(char *name, char *prog)
{
	struct filter_conf	*f;
	char			*path;

	if (dict_get(&conf->sc_filters, name)) {
		yyerror("filter \"%s\" already defined", name);
		return (NULL);
	}

	if (asprintf(&path, "%s/filter-%s", PATH_LIBEXEC, prog) == -1) {
		yyerror("filter \"%s\" asprintf failed", name);
		return (0);
	}

	f = xcalloc(1, sizeof(*f), "create_filter");
	f->path = path;
	f->name = name;
	f->argv[f->argc++] = name;

	dict_xset(&conf->sc_filters, name, f);

	return (f);
}

static struct filter_conf *
create_filter_chain(char *name)
{
	struct filter_conf	*f;

	if (dict_get(&conf->sc_filters, name)) {
		yyerror("filter \"%s\" already defined", name);
		return (NULL);
	}

	f = xcalloc(1, sizeof(*f), "create_filter_chain");
	f->chain = 1;
	f->name = name;

	dict_xset(&conf->sc_filters, name, f);

	return (f);
}

static int
add_filter_arg(struct filter_conf *f, char *arg)
{
	if (f->argc == MAX_FILTER_ARGS) {
		yyerror("filter \"%s\" is full", f->name);
		return (0);
	}

	if (f->chain) {
		if (dict_get(&conf->sc_filters, arg) == NULL) {
			yyerror("undefined filter \"%s\"", arg);
			return (0);
		}
		if (dict_get(&conf->sc_filters, arg) == f) {
			yyerror("filter chain cannot contain itself");
			return (0);
		}
	}

	f->argv[f->argc++] = arg;

	return (1);
}

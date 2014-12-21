/*	$OpenBSD: parse.y,v 1.198 2014/12/21 00:54:49 guenther Exp $	*/

/*
 * Copyright (c) 2007 - 2014 Reyk Floeter <reyk@openbsd.org>
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
#include <md5.h>

#include <openssl/ssl.h>

#include "relayd.h"
#include "http.h"
#include "snmp.h"

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

struct relayd		*conf = NULL;
static int		 errors = 0;
static int		 loadcfg = 0;
objid_t			 last_rdr_id = 0;
objid_t			 last_table_id = 0;
objid_t			 last_host_id = 0;
objid_t			 last_relay_id = 0;
objid_t			 last_proto_id = 0;
objid_t			 last_rt_id = 0;
objid_t			 last_nr_id = 0;
objid_t			 last_key_id = 0;

static struct rdr	*rdr = NULL;
static struct table	*table = NULL;
static struct relay	*rlay = NULL;
static struct host	*hst = NULL;
struct relaylist	 relays;
static struct protocol	*proto = NULL;
static struct relay_rule *rule = NULL;
static struct router	*router = NULL;
static int		 label = 0;
static int		 tagged = 0;
static int		 tag = 0;
static in_port_t	 tableport = 0;
static int		 dstmode;
static enum key_type	 keytype = KEY_TYPE_NONE;
static enum direction	 dir = RELAY_DIR_ANY;
static char		*rulefile = NULL;
static union hashkey	*hashkey = NULL;

struct address	*host_v4(const char *);
struct address	*host_v6(const char *);
int		 host_dns(const char *, struct addresslist *,
		    int, struct portrange *, const char *, int);
int		 host_if(const char *, struct addresslist *,
		    int, struct portrange *, const char *, int);
int		 host(const char *, struct addresslist *,
		    int, struct portrange *, const char *, int);
void		 host_free(struct addresslist *);

struct table	*table_inherit(struct table *);
int		 relay_id(struct relay *);
struct relay	*relay_inherit(struct relay *, struct relay *);
int		 getservice(char *);
int		 is_if_in_group(const char *, const char *);

typedef struct {
	union {
		int64_t			 number;
		char			*string;
		struct host		*host;
		struct timeval		 tv;
		struct table		*table;
		struct portrange	 port;
		struct {
			union hashkey	 key;
			int		 keyset;
		}			 key;
		enum direction		 dir;
		struct {
			struct sockaddr_storage	 ss;
			char			 name[MAXHOSTNAMELEN];
		}			 addr;
		struct {
			enum digest_type type;
			char		*digest;
		}			 digest;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	ALL APPEND BACKLOG BACKUP BUFFER CA CACHE SET CHECK CIPHERS CODE
%token	COOKIE DEMOTE DIGEST DISABLE ERROR EXPECT PASS BLOCK EXTERNAL FILENAME
%token	FORWARD FROM HASH HEADER HOST ICMP INCLUDE INET INET6 INTERFACE
%token	INTERVAL IP LABEL LISTEN VALUE LOADBALANCE LOG LOOKUP METHOD MODE NAT
%token	NO DESTINATION NODELAY NOTHING ON PARENT PATH PFTAG PORT PREFORK
%token	PRIORITY PROTO QUERYSTR REAL REDIRECT RELAY REMOVE REQUEST RESPONSE
%token	RETRY QUICK RETURN ROUNDROBIN ROUTE SACK SCRIPT SEND SESSION SNMP
%token	SOCKET SPLICE SSL STICKYADDR STYLE TABLE TAG TAGGED TCP TIMEOUT TLS TO
%token	ROUTER RTLABEL TRANSPARENT TRAP UPDATES URL VIRTUAL WITH TTL RTABLE
%token	MATCH PARAMS RANDOM LEASTSTATES SRCHASH KEY CERTIFICATE PASSWORD ECDH
%token	EDH CURVE
%token	<v.string>	STRING
%token  <v.number>	NUMBER
%type	<v.string>	hostname interface table value optstring
%type	<v.number>	http_type loglevel quick trap
%type	<v.number>	dstmode flag forwardmode retry
%type	<v.number>	opttls opttlsclient tlscache
%type	<v.number>	redirect_proto relay_proto match
%type	<v.number>	action ruleaf key_option
%type	<v.number>	tlsdhparams tlsecdhcurve
%type	<v.port>	port
%type	<v.host>	host
%type	<v.addr>	address
%type	<v.tv>		timeout
%type	<v.digest>	digest optdigest
%type	<v.table>	tablespec
%type	<v.dir>		dir
%type	<v.key>		hashkey

%%

grammar		: /* empty */
		| grammar include '\n'
		| grammar '\n'
		| grammar varset '\n'
		| grammar main '\n'
		| grammar rdr '\n'
		| grammar tabledef '\n'
		| grammar relay '\n'
		| grammar proto '\n'
		| grammar router '\n'
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

ssltls		: SSL		{
			log_warnx("%s:%d: %s",
			    file->name, yylval.lineno,
			    "please use the \"tls\" keyword"
			    " instead of \"ssl\"");
		}
		| TLS
		;

opttls		: /*empty*/	{ $$ = 0; }
		| ssltls	{ $$ = 1; }
		;

opttlsclient	: /*empty*/	{ $$ = 0; }
		| WITH ssltls	{ $$ = 1; }
		;

http_type	: STRING	{
			if (strcmp("https", $1) == 0) {
				$$ = 1;
			} else if (strcmp("http", $1) == 0) {
				$$ = 0;
			} else {
				yyerror("invalid check type: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

hostname	: /* empty */		{
			$$ = strdup("");
			if ($$ == NULL)
				fatal("calloc");
		}
		| HOST STRING	{
			if (asprintf(&$$, "Host: %s\r\nConnection: close\r\n",
			    $2) == -1)
				fatal("asprintf");
		}
		;

relay_proto	: /* empty */			{ $$ = RELAY_PROTO_TCP; }
		| TCP				{ $$ = RELAY_PROTO_TCP; }
		| STRING			{
			if (strcmp("http", $1) == 0) {
				$$ = RELAY_PROTO_HTTP;
			} else if (strcmp("dns", $1) == 0) {
				$$ = RELAY_PROTO_DNS;
			} else {
				yyerror("invalid protocol type: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

redirect_proto	: /* empty */			{ $$ = IPPROTO_TCP; }
		| TCP				{ $$ = IPPROTO_TCP; }
		| STRING			{
			struct protoent	*p;

			if ((p = getprotobyname($1)) == NULL) {
				yyerror("invalid protocol: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);

			$$ = p->p_proto;
		}
		;

eflags_l	: eflags comma eflags_l
		| eflags
		;

opteflags	: /* nothing */
		| eflags
		;

eflags		: STYLE STRING
		{
			if ((proto->style = strdup($2)) == NULL)
				fatal("out of memory");
			free($2);
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
				yyerror("invalid port: %d", $2);
				YYERROR;
			}
			$$.val[0] = htons($2);
			$$.op = PF_OP_EQ;
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
			table->sendbuf = NULL;
		}
		| STRING		{
			table->sendbuf = strdup($1);
			if (table->sendbuf == NULL)
				fatal("out of memory");
			free($1);
		}
		;

main		: INTERVAL NUMBER	{
			if (loadcfg)
				break;
			if ((conf->sc_interval.tv_sec = $2) < 0) {
				yyerror("invalid interval: %d", $2);
				YYERROR;
			}
		}
		| LOG loglevel		{
			if (loadcfg)
				break;
			conf->sc_opts |= $2;
		}
		| TIMEOUT timeout	{
			if (loadcfg)
				break;
			bcopy(&$2, &conf->sc_timeout, sizeof(struct timeval));
		}
		| PREFORK NUMBER	{
			if (loadcfg)
				break;
			if ($2 <= 0 || $2 > RELAY_MAXPROC) {
				yyerror("invalid number of preforked "
				    "relays: %d", $2);
				YYERROR;
			}
			conf->sc_prefork_relay = $2;
		}
		| SNMP trap optstring	{
			if (loadcfg)
				break;
			conf->sc_flags |= F_SNMP;
			if ($2)
				conf->sc_snmp_flags |= FSNMP_TRAPONLY;
			if ($3)
				conf->sc_snmp_path = $3;
			else
				conf->sc_snmp_path = strdup(AGENTX_SOCKET);
			if (conf->sc_snmp_path == NULL)
				fatal("out of memory");
		}
		;

trap		: /* nothing */		{ $$ = 0; }
		| TRAP			{ $$ = 1; }

loglevel	: UPDATES		{ $$ = RELAYD_OPT_LOGUPDATE; }
		| ALL			{ $$ = RELAYD_OPT_LOGALL; }
		;

rdr		: REDIRECT STRING	{
			struct rdr *srv;

			conf->sc_flags |= F_NEEDPF;

			if (!loadcfg) {
				free($2);
				YYACCEPT;
			}

			TAILQ_FOREACH(srv, conf->sc_rdrs, entry)
				if (!strcmp(srv->conf.name, $2))
					break;
			if (srv != NULL) {
				yyerror("redirection %s defined twice", $2);
				free($2);
				YYERROR;
			}
			if ((srv = calloc(1, sizeof (*srv))) == NULL)
				fatal("out of memory");

			if (strlcpy(srv->conf.name, $2,
			    sizeof(srv->conf.name)) >=
			    sizeof(srv->conf.name)) {
				yyerror("redirection name truncated");
				free($2);
				free(srv);
				YYERROR;
			}
			free($2);
			srv->conf.id = ++last_rdr_id;
			srv->conf.timeout.tv_sec = RELAY_TIMEOUT;
			if (last_rdr_id == INT_MAX) {
				yyerror("too many redirections defined");
				free(srv);
				YYERROR;
			}
			rdr = srv;
		} '{' optnl rdropts_l '}'	{
			if (rdr->table == NULL) {
				yyerror("redirection %s has no table",
				    rdr->conf.name);
				YYERROR;
			}
			if (TAILQ_EMPTY(&rdr->virts)) {
				yyerror("redirection %s has no virtual ip",
				    rdr->conf.name);
				YYERROR;
			}
			conf->sc_rdrcount++;
			if (rdr->backup == NULL) {
				rdr->conf.backup_id =
				    conf->sc_empty_table.conf.id;
				rdr->backup = &conf->sc_empty_table;
			} else if (rdr->backup->conf.port !=
			    rdr->table->conf.port) {
				yyerror("redirection %s uses two different "
				    "ports for its table and backup table",
				    rdr->conf.name);
				YYERROR;
			}
			if (!(rdr->conf.flags & F_DISABLE))
				rdr->conf.flags |= F_ADD;
			TAILQ_INSERT_TAIL(conf->sc_rdrs, rdr, entry);
			tableport = 0;
			rdr = NULL;
		}
		;

rdropts_l	: rdropts_l rdroptsl nl
		| rdroptsl optnl
		;

rdroptsl	: forwardmode TO tablespec interface	{
			if (hashkey != NULL) {
				free(hashkey);
				hashkey = NULL;
			}

			switch ($1) {
			case FWD_NORMAL:
				if ($4 == NULL)
					break;
				yyerror("superfluous interface");
				free($4);
				YYERROR;
			case FWD_ROUTE:
				if ($4 != NULL)
					break;
				yyerror("missing interface to route to");
				free($4);
				YYERROR;
			case FWD_TRANS:
				yyerror("no transparent forward here");
				if ($4 != NULL)
					free($4);
				YYERROR;
			}
			if ($4 != NULL) {
				if (strlcpy($3->conf.ifname, $4,
				    sizeof($3->conf.ifname)) >=
				    sizeof($3->conf.ifname)) {
					yyerror("interface name truncated");
					free($4);
					YYERROR;
				}
				free($4);
			}

			if ($3->conf.check == CHECK_NOCHECK) {
				yyerror("table %s has no check", $3->conf.name);
				purge_table(conf->sc_tables, $3);
				YYERROR;
			}
			if (rdr->backup) {
				yyerror("only one backup table is allowed");
				purge_table(conf->sc_tables, $3);
				YYERROR;
			}
			if (rdr->table) {
				rdr->backup = $3;
				rdr->conf.backup_id = $3->conf.id;
				if (dstmode != rdr->conf.mode) {
					yyerror("backup table for %s with "
					    "different mode", rdr->conf.name);
					YYERROR;
				}
			} else {
				rdr->table = $3;
				rdr->conf.table_id = $3->conf.id;
				rdr->conf.mode = dstmode;
			}
			$3->conf.fwdmode = $1;
			$3->conf.rdrid = rdr->conf.id;
			$3->conf.flags |= F_USED;
		}
		| LISTEN ON STRING redirect_proto port interface {
			if (host($3, &rdr->virts,
			    SRV_MAX_VIRTS, &$5, $6, $4) <= 0) {
				yyerror("invalid virtual ip: %s", $3);
				free($3);
				free($6);
				YYERROR;
			}
			free($3);
			free($6);
			if (rdr->conf.port == 0)
				rdr->conf.port = $5.val[0];
			tableport = rdr->conf.port;
		}
		| DISABLE		{ rdr->conf.flags |= F_DISABLE; }
		| STICKYADDR		{ rdr->conf.flags |= F_STICKY; }
		| match PFTAG STRING {
			conf->sc_flags |= F_NEEDPF;
			if (strlcpy(rdr->conf.tag, $3,
			    sizeof(rdr->conf.tag)) >=
			    sizeof(rdr->conf.tag)) {
				yyerror("redirection tag name truncated");
				free($3);
				YYERROR;
			}
			if ($1)
				rdr->conf.flags |= F_MATCH;
			free($3);
		}
		| SESSION TIMEOUT NUMBER		{
			if ((rdr->conf.timeout.tv_sec = $3) < 0) {
				yyerror("invalid timeout: %lld", $3);
				YYERROR;
			}
			if (rdr->conf.timeout.tv_sec > INT_MAX) {
				yyerror("timeout too large: %lld", $3);
				YYERROR;
			}
		}
		| include
		;

match		: /* empty */		{ $$ = 0; }
		| MATCH			{ $$ = 1; }
		;

forwardmode	: FORWARD		{ $$ = FWD_NORMAL; }
		| ROUTE			{ $$ = FWD_ROUTE; }
		| TRANSPARENT FORWARD	{ $$ = FWD_TRANS; }
		;

table		: '<' STRING '>'	{
			if (strlen($2) >= TABLE_NAME_SIZE) {
				yyerror("invalid table name");
				free($2);
				YYERROR;
			}
			$$ = $2;
		}
		;

tabledef	: TABLE table		{
			struct table *tb;

			if (!loadcfg) {
				free($2);
				YYACCEPT;
			}

			TAILQ_FOREACH(tb, conf->sc_tables, entry)
				if (!strcmp(tb->conf.name, $2))
					break;
			if (tb != NULL) {
				yyerror("table %s defined twice", $2);
				free($2);
				YYERROR;
			}

			if ((tb = calloc(1, sizeof (*tb))) == NULL)
				fatal("out of memory");

			if (strlcpy(tb->conf.name, $2,
			    sizeof(tb->conf.name)) >= sizeof(tb->conf.name)) {
				yyerror("table name truncated");
				free($2);
				YYERROR;
			}
			free($2);

			tb->conf.id = 0; /* will be set later */
			bcopy(&conf->sc_timeout, &tb->conf.timeout,
			    sizeof(struct timeval));
			TAILQ_INIT(&tb->hosts);
			table = tb;
			dstmode = RELAY_DSTMODE_DEFAULT;
		} tabledefopts_l	{
			if (TAILQ_EMPTY(&table->hosts)) {
				yyerror("table %s has no hosts",
				    table->conf.name);
				YYERROR;
			}
			conf->sc_tablecount++;
			TAILQ_INSERT_TAIL(conf->sc_tables, table, entry);
		}
		;

tabledefopts_l	: tabledefopts_l tabledefopts
		| tabledefopts
		;

tabledefopts	: DISABLE		{ table->conf.flags |= F_DISABLE; }
		| '{' optnl tablelist_l '}'
		;

tablelist_l	: tablelist comma tablelist_l
		| tablelist optnl
		;

tablelist	: host			{
			$1->conf.tableid = table->conf.id;
			$1->tablename = table->conf.name;
			TAILQ_INSERT_TAIL(&table->hosts, $1, entry);
		}
		| include
		;

tablespec	: table			{
			struct table	*tb;
			if ((tb = calloc(1, sizeof (*tb))) == NULL)
				fatal("out of memory");
			if (strlcpy(tb->conf.name, $1,
			    sizeof(tb->conf.name)) >= sizeof(tb->conf.name)) {
				yyerror("table name truncated");
				free($1);
				YYERROR;
			}
			free($1);
			table = tb;
			dstmode = RELAY_DSTMODE_DEFAULT;
			hashkey = NULL;
		} tableopts_l		{
			struct table	*tb;
			if (table->conf.port == 0)
				table->conf.port = tableport;
			else
				table->conf.flags |= F_PORT;
			if ((tb = table_inherit(table)) == NULL)
				YYERROR;
			$$ = tb;
		}
		;

tableopts_l	: tableopts tableopts_l
		| tableopts
		;

tableopts	: CHECK tablecheck
		| port			{
			if ($1.op != PF_OP_EQ) {
				yyerror("invalid port");
				YYERROR;
			}
			table->conf.port = $1.val[0];
		}
		| TIMEOUT timeout	{
			bcopy(&$2, &table->conf.timeout,
			    sizeof(struct timeval));
		}
		| DEMOTE STRING		{
			table->conf.flags |= F_DEMOTE;
			if (strlcpy(table->conf.demote_group, $2,
			    sizeof(table->conf.demote_group))
			    >= sizeof(table->conf.demote_group)) {
				yyerror("yyparse: demote group name too long");
				free($2);
				YYERROR;
			}
			free($2);
			if (carp_demote_init(table->conf.demote_group, 1)
			    == -1) {
				yyerror("yyparse: error initializing group "
				    "'%s'", table->conf.demote_group);
				YYERROR;
			}
		}
		| INTERVAL NUMBER	{
			if ($2 < conf->sc_interval.tv_sec ||
			    $2 % conf->sc_interval.tv_sec) {
				yyerror("table interval must be "
				    "divisible by global interval");
				YYERROR;
			}
			table->conf.skip_cnt =
			    ($2 / conf->sc_interval.tv_sec) - 1;
		}
		| MODE dstmode hashkey	{
			switch ($2) {
			case RELAY_DSTMODE_LOADBALANCE:
			case RELAY_DSTMODE_HASH:
			case RELAY_DSTMODE_SRCHASH:
				if (hashkey != NULL) {
					yyerror("key already specified");
					free(hashkey);
					YYERROR;
				}
				if ((hashkey = calloc(1,
				    sizeof(*hashkey))) == NULL)
					fatal("out of memory");
				memcpy(hashkey, &$3.key, sizeof(*hashkey));
				break;
			default:
				if ($3.keyset) {
					yyerror("key not supported by mode");
					YYERROR;
				}
				hashkey = NULL;
				break;
			}

			switch ($2) {
			case RELAY_DSTMODE_LOADBALANCE:
			case RELAY_DSTMODE_HASH:
			case RELAY_DSTMODE_RANDOM:
			case RELAY_DSTMODE_SRCHASH:
				if (rdr != NULL) {
					yyerror("mode not supported "
					    "for redirections");
					YYERROR;
				}
				/* FALLTHROUGH */
			case RELAY_DSTMODE_ROUNDROBIN:
				dstmode = $2;
				break;
			case RELAY_DSTMODE_LEASTSTATES:
				if (rdr == NULL) {
					yyerror("mode not supported "
					    "for relays");
					YYERROR;
				}
				dstmode = $2;
				break;
			}
		}
		;

/* should be in sync with sbin/pfctl/parse.y's hashkey */
hashkey		: /* empty */		{
			$$.keyset = 0;
			$$.key.data[0] = arc4random();
			$$.key.data[1] = arc4random();
			$$.key.data[2] = arc4random();
			$$.key.data[3] = arc4random();
		}
		| STRING		{
			/* manual key configuration */
			$$.keyset = 1;

			if (!strncmp($1, "0x", 2)) {
				if (strlen($1) != 34) {
					free($1);
					yyerror("hex key must be 128 bits "
					    "(32 hex digits) long");
					YYERROR;
				}

				if (sscanf($1, "0x%8x%8x%8x%8x",
				    &$$.key.data[0], &$$.key.data[1],
				    &$$.key.data[2], &$$.key.data[3]) != 4) {
					free($1);
					yyerror("invalid hex key");
					YYERROR;
				}
			} else {
				MD5_CTX	context;

				MD5Init(&context);
				MD5Update(&context, (unsigned char *)$1,
				    strlen($1));
				MD5Final((unsigned char *)$$.key.data,
				    &context);
				HTONL($$.key.data[0]);
				HTONL($$.key.data[1]);
				HTONL($$.key.data[2]);
				HTONL($$.key.data[3]);
			}
			free($1);
		}
		;

tablecheck	: ICMP			{ table->conf.check = CHECK_ICMP; }
		| TCP			{ table->conf.check = CHECK_TCP; }
		| ssltls		{
			table->conf.check = CHECK_TCP;
			conf->sc_flags |= F_TLS;
			table->conf.flags |= F_TLS;
		}
		| http_type STRING hostname CODE NUMBER {
			if ($1) {
				conf->sc_flags |= F_TLS;
				table->conf.flags |= F_TLS;
			}
			table->conf.check = CHECK_HTTP_CODE;
			if ((table->conf.retcode = $5) <= 0) {
				yyerror("invalid HTTP code: %d", $5);
				free($2);
				free($3);
				YYERROR;
			}
			if (asprintf(&table->sendbuf,
			    "HEAD %s HTTP/1.%c\r\n%s\r\n",
			    $2, strlen($3) ? '1' : '0', $3) == -1)
				fatal("asprintf");
			free($2);
			free($3);
			if (table->sendbuf == NULL)
				fatal("out of memory");
		}
		| http_type STRING hostname digest {
			if ($1) {
				conf->sc_flags |= F_TLS;
				table->conf.flags |= F_TLS;
			}
			table->conf.check = CHECK_HTTP_DIGEST;
			if (asprintf(&table->sendbuf,
			    "GET %s HTTP/1.%c\r\n%s\r\n",
			    $2, strlen($3) ? '1' : '0', $3) == -1)
				fatal("asprintf");
			free($2);
			free($3);
			if (table->sendbuf == NULL)
				fatal("out of memory");
			if (strlcpy(table->conf.digest, $4.digest,
			    sizeof(table->conf.digest)) >=
			    sizeof(table->conf.digest)) {
				yyerror("digest truncated");
				free($4.digest);
				YYERROR;
			}
			table->conf.digest_type = $4.type;
			free($4.digest);
		}
		| SEND sendbuf EXPECT STRING opttls {
			table->conf.check = CHECK_SEND_EXPECT;
			if ($5) {
				conf->sc_flags |= F_TLS;
				table->conf.flags |= F_TLS;
			}
			if (strlcpy(table->conf.exbuf, $4,
			    sizeof(table->conf.exbuf))
			    >= sizeof(table->conf.exbuf)) {
				yyerror("yyparse: expect buffer truncated");
				free($4);
				YYERROR;
			}
			translate_string(table->conf.exbuf);
			free($4);
		}
		| SCRIPT STRING {
			table->conf.check = CHECK_SCRIPT;
			if (strlcpy(table->conf.path, $2,
			    sizeof(table->conf.path)) >=
			    sizeof(table->conf.path)) {
				yyerror("script path truncated");
				free($2);
				YYERROR;
			}
			conf->sc_flags |= F_SCRIPT;
			free($2);
		}
		;

digest		: DIGEST STRING
		{
			switch (strlen($2)) {
			case 40:
				$$.type = DIGEST_SHA1;
				break;
			case 32:
				$$.type = DIGEST_MD5;
				break;
			default:
				yyerror("invalid http digest");
				free($2);
				YYERROR;
			}
			$$.digest = $2;
		}
		;

optdigest	: digest			{
			$$.digest = $1.digest;
			$$.type = $1.type;
		}
		| STRING			{
			$$.digest = $1;
			$$.type = DIGEST_NONE;
		}
		;

proto		: relay_proto PROTO STRING	{
			struct protocol *p;

			if (!loadcfg) {
				free($3);
				YYACCEPT;
			}

			if (strcmp($3, "default") == 0) {
				p = &conf->sc_proto_default;
			} else {
				TAILQ_FOREACH(p, conf->sc_protos, entry)
					if (!strcmp(p->name, $3))
						break;
			}
			if (p != NULL) {
				yyerror("protocol %s defined twice", $3);
				free($3);
				YYERROR;
			}
			if ((p = calloc(1, sizeof (*p))) == NULL)
				fatal("out of memory");

			if (strlcpy(p->name, $3, sizeof(p->name)) >=
			    sizeof(p->name)) {
				yyerror("protocol name truncated");
				free($3);
				free(p);
				YYERROR;
			}
			free($3);
			p->id = ++last_proto_id;
			p->type = $1;
			p->cache = RELAY_CACHESIZE;
			p->tcpflags = TCPFLAG_DEFAULT;
			p->tlsflags = TLSFLAG_DEFAULT;
			p->tcpbacklog = RELAY_BACKLOG;
			TAILQ_INIT(&p->rules);
			(void)strlcpy(p->tlsciphers, TLSCIPHERS_DEFAULT,
			    sizeof(p->tlsciphers));
			p->tlsdhparams = TLSDHPARAMS_DEFAULT;
			p->tlsecdhcurve = TLSECDHCURVE_DEFAULT;
			if (last_proto_id == INT_MAX) {
				yyerror("too many protocols defined");
				free(p);
				YYERROR;
			}
			proto = p;
		} protopts_n			{
			conf->sc_protocount++;

			if ((proto->tlsflags & TLSFLAG_VERSION) == 0) {
				yyerror("invalid TLS protocol");
				YYERROR;
			}

			TAILQ_INSERT_TAIL(conf->sc_protos, proto, entry);
		}
		;

protopts_n	: /* empty */
		| '{' '}'
		| '{' optnl protopts_l '}'
		;

protopts_l	: protopts_l protoptsl nl
		| protoptsl optnl
		;

protoptsl	: ssltls tlsflags
		| ssltls '{' tlsflags_l '}'
		| TCP tcpflags
		| TCP '{' tcpflags_l '}'
		| RETURN ERROR opteflags	{ proto->flags |= F_RETURN; }
		| RETURN ERROR '{' eflags_l '}'	{ proto->flags |= F_RETURN; }
		| filterrule
		| include
		;

tcpflags_l	: tcpflags comma tcpflags_l
		| tcpflags
		;

tcpflags	: SACK			{ proto->tcpflags |= TCPFLAG_SACK; }
		| NO SACK		{ proto->tcpflags |= TCPFLAG_NSACK; }
		| NODELAY		{ proto->tcpflags |= TCPFLAG_NODELAY; }
		| NO NODELAY		{ proto->tcpflags |= TCPFLAG_NNODELAY; }
		| SPLICE		{ /* default */ }
		| NO SPLICE		{ proto->tcpflags |= TCPFLAG_NSPLICE; }
		| BACKLOG NUMBER	{
			if ($2 < 0 || $2 > RELAY_MAX_SESSIONS) {
				yyerror("invalid backlog: %d", $2);
				YYERROR;
			}
			proto->tcpbacklog = $2;
		}
		| SOCKET BUFFER NUMBER	{
			proto->tcpflags |= TCPFLAG_BUFSIZ;
			if ((proto->tcpbufsiz = $3) < 0) {
				yyerror("invalid socket buffer size: %d", $3);
				YYERROR;
			}
		}
		| IP STRING NUMBER	{
			if ($3 < 0) {
				yyerror("invalid ttl: %d", $3);
				free($2);
				YYERROR;
			}
			if (strcasecmp("ttl", $2) == 0) {
				proto->tcpflags |= TCPFLAG_IPTTL;
				proto->tcpipttl = $3;
			} else if (strcasecmp("minttl", $2) == 0) {
				proto->tcpflags |= TCPFLAG_IPMINTTL;
				proto->tcpipminttl = $3;
			} else {
				yyerror("invalid TCP/IP flag: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
		}
		;

tlsflags_l	: tlsflags comma tlsflags_l
		| tlsflags
		;

tlsflags	: SESSION CACHE tlscache	{ proto->cache = $3; }
		| CIPHERS STRING		{
			if (strlcpy(proto->tlsciphers, $2,
			    sizeof(proto->tlsciphers)) >=
			    sizeof(proto->tlsciphers)) {
				yyerror("tlsciphers truncated");
				free($2);
				YYERROR;
			}
			free($2);
		}
		| NO EDH			{
			proto->tlsdhparams = TLSDHPARAMS_NONE;
		}
		| EDH tlsdhparams		{
			proto->tlsdhparams = $2;
		}
		| NO ECDH			{
			proto->tlsecdhcurve = 0;
		}
		| ECDH tlsecdhcurve		{
			proto->tlsecdhcurve = $2;
		}
		| CA FILENAME STRING		{
			if (strlcpy(proto->tlsca, $3,
			    sizeof(proto->tlsca)) >=
			    sizeof(proto->tlsca)) {
				yyerror("tlsca truncated");
				free($3);
				YYERROR;
			}
			free($3);
		}
		| CA KEY STRING PASSWORD STRING	{
			if (strlcpy(proto->tlscakey, $3,
			    sizeof(proto->tlscakey)) >=
			    sizeof(proto->tlscakey)) {
				yyerror("tlscakey truncated");
				free($3);
				free($5);
				YYERROR;
			}
			if ((proto->tlscapass = strdup($5)) == NULL) {
				yyerror("tlscapass");
				free($3);
				free($5);
				YYERROR;
			}
			free($3);
			free($5);
		}
		| CA CERTIFICATE STRING		{
			if (strlcpy(proto->tlscacert, $3,
			    sizeof(proto->tlscacert)) >=
			    sizeof(proto->tlscacert)) {
				yyerror("tlscacert truncated");
				free($3);
				YYERROR;
			}
			free($3);
		}
		| NO flag			{ proto->tlsflags &= ~($2); }
		| flag				{ proto->tlsflags |= $1; }
		;

flag		: STRING			{
			if (strcmp("sslv3", $1) == 0)
				$$ = TLSFLAG_SSLV3;
			else if (strcmp("tlsv1", $1) == 0)
				$$ = TLSFLAG_TLSV1;
			else if (strcmp("tlsv1.0", $1) == 0)
				$$ = TLSFLAG_TLSV1_0;
			else if (strcmp("tlsv1.1", $1) == 0)
				$$ = TLSFLAG_TLSV1_1;
			else if (strcmp("tlsv1.2", $1) == 0)
				$$ = TLSFLAG_TLSV1_2;
			else if (strcmp("cipher-server-preference", $1) == 0)
				$$ = TLSFLAG_CIPHER_SERVER_PREF;
			else if (strcmp("client-renegotiation", $1) == 0)
				$$ = TLSFLAG_CLIENT_RENEG;
			else {
				yyerror("invalid TLS flag: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

tlscache	: NUMBER			{
			if ($1 < 0) {
				yyerror("invalid tlscache value: %d", $1);
				YYERROR;
			}
			$$ = $1;
		}
		| DISABLE			{ $$ = -2; }
		;

filterrule	: action dir quick ruleaf rulesrc ruledst {
			if ((rule = calloc(1, sizeof(*rule))) == NULL)
				fatal("out of memory");

			rule->rule_action = $1;
			rule->rule_proto = proto->type;
			rule->rule_dir = $2;
			rule->rule_flags |= $3;
			rule->rule_af = $4;

			rulefile = NULL;
		} ruleopts_l {
			if (rule_add(proto, rule, rulefile) == -1) {
				if (rulefile == NULL) {
					yyerror("failed to load rule");
				} else {
					yyerror("failed to load rules from %s",
					    rulefile);
					free(rulefile);
				}
				rule_free(rule);
				free(rule);
				YYERROR;
			}
			if (rulefile)
				free(rulefile);
			rulefile = NULL;
			rule = NULL;
			keytype = KEY_TYPE_NONE;
		}
		;

action		: PASS				{ $$ = RULE_ACTION_PASS; }
		| BLOCK				{ $$ = RULE_ACTION_BLOCK; }
		| MATCH				{ $$ = RULE_ACTION_MATCH; }
		;

dir		: /* empty */			{
			$$ = dir = RELAY_DIR_REQUEST;
		}
		| REQUEST			{
			$$ = dir = RELAY_DIR_REQUEST;
		}
		| RESPONSE			{
			$$ = dir = RELAY_DIR_RESPONSE;
		}
		;

quick		: /* empty */			{ $$ = 0; }
		| QUICK				{ $$ = RULE_FLAG_QUICK; }
		;

ruleaf		: /* empty */			{ $$ = AF_UNSPEC; }
		| INET6				{ $$ = AF_INET6; }
		| INET				{ $$ = AF_INET; }
		;

rulesrc		: /* XXX */
		;

ruledst		: /* XXX */
		;

ruleopts_l	: /* empty */
		| ruleopts_t
		;

ruleopts_t	: ruleopts ruleopts_t
		| ruleopts
		;

ruleopts	: METHOD STRING					{
			u_int	id;
			if ((id = relay_httpmethod_byname($2)) ==
			    HTTP_METHOD_NONE) {
				yyerror("unknown HTTP method currently not "
				    "supported");
				free($2);
				YYERROR;
			}
			rule->rule_method = id;
			free($2);
		}
		| COOKIE key_option STRING value		{
			keytype = KEY_TYPE_COOKIE;
			rule->rule_kv[keytype].kv_key = strdup($3);
			rule->rule_kv[keytype].kv_option = $2;
			rule->rule_kv[keytype].kv_value = (($4 != NULL) ?
			    strdup($4) : strdup("*"));
			if (rule->rule_kv[keytype].kv_key == NULL ||
			    rule->rule_kv[keytype].kv_value == NULL)
				fatal("out of memory");
			free($3);
			if ($4)
				free($4);
			rule->rule_kv[keytype].kv_type = keytype;
		}
		| COOKIE key_option				{
			keytype = KEY_TYPE_COOKIE;
			rule->rule_kv[keytype].kv_option = $2;
			rule->rule_kv[keytype].kv_type = keytype;
		}
		| HEADER key_option STRING value		{
			keytype = KEY_TYPE_HEADER;
			memset(&rule->rule_kv[keytype], 0,
			    sizeof(rule->rule_kv[keytype]));
			rule->rule_kv[keytype].kv_option = $2;
			rule->rule_kv[keytype].kv_key = strdup($3);
			rule->rule_kv[keytype].kv_value = (($4 != NULL) ?
			    strdup($4) : strdup("*"));
			if (rule->rule_kv[keytype].kv_key == NULL ||
			    rule->rule_kv[keytype].kv_value == NULL)
				fatal("out of memory");
			free($3);
			if ($4)
				free($4);
			rule->rule_kv[keytype].kv_type = keytype;
		}
		| HEADER key_option				{
			keytype = KEY_TYPE_HEADER;
			rule->rule_kv[keytype].kv_option = $2;
			rule->rule_kv[keytype].kv_type = keytype;
		}
		| PATH key_option STRING value			{
			keytype = KEY_TYPE_PATH;
			rule->rule_kv[keytype].kv_option = $2;
			rule->rule_kv[keytype].kv_key = strdup($3);
			rule->rule_kv[keytype].kv_value = (($4 != NULL) ?
			    strdup($4) : strdup("*"));
			if (rule->rule_kv[keytype].kv_key == NULL ||
			    rule->rule_kv[keytype].kv_value == NULL)
				fatal("out of memory");
			free($3);
			if ($4)
				free($4);
			rule->rule_kv[keytype].kv_type = keytype;
		}
		| PATH key_option				{
			keytype = KEY_TYPE_PATH;
			rule->rule_kv[keytype].kv_option = $2;
			rule->rule_kv[keytype].kv_type = keytype;
		}
		| QUERYSTR key_option STRING value		{
			switch ($2) {
			case KEY_OPTION_APPEND:
			case KEY_OPTION_SET:
			case KEY_OPTION_REMOVE:
				yyerror("combining query type and the given "
				    "option is not supported");
				free($3);
				if ($4)
					free($4);
				YYERROR;
				break;
			}
			keytype = KEY_TYPE_QUERY;
			rule->rule_kv[keytype].kv_option = $2;
			rule->rule_kv[keytype].kv_key = strdup($3);
			rule->rule_kv[keytype].kv_value = (($4 != NULL) ?
			    strdup($4) : strdup("*"));
			if (rule->rule_kv[keytype].kv_key == NULL ||
			    rule->rule_kv[keytype].kv_value == NULL)
				fatal("out of memory");
			free($3);
			if ($4)
				free($4);
			rule->rule_kv[keytype].kv_type = keytype;
		}
		| QUERYSTR key_option				{
			switch ($2) {
			case KEY_OPTION_APPEND:
			case KEY_OPTION_SET:
			case KEY_OPTION_REMOVE:
				yyerror("combining query type and the given "
				    "option is not supported");
				YYERROR;
				break;
			}
			keytype = KEY_TYPE_QUERY;
			rule->rule_kv[keytype].kv_option = $2;
			rule->rule_kv[keytype].kv_type = keytype;
		}
		| URL key_option optdigest value			{
			switch ($2) {
			case KEY_OPTION_APPEND:
			case KEY_OPTION_SET:
			case KEY_OPTION_REMOVE:
				yyerror("combining url type and the given "
				"option is not supported");
				free($3.digest);
				free($4);
				YYERROR;
				break;
			}
			keytype = KEY_TYPE_URL;
			rule->rule_kv[keytype].kv_option = $2;
			rule->rule_kv[keytype].kv_key = strdup($3.digest);
			rule->rule_kv[keytype].kv_digest = $3.type;
			rule->rule_kv[keytype].kv_value = (($4 != NULL) ?
			    strdup($4) : strdup("*"));
			if (rule->rule_kv[keytype].kv_key == NULL ||
			    rule->rule_kv[keytype].kv_value == NULL)
				fatal("out of memory");
			free($3.digest);
			if ($4)
				free($4);
			rule->rule_kv[keytype].kv_type = keytype;
		}
		| URL key_option					{
			switch ($2) {
			case KEY_OPTION_APPEND:
			case KEY_OPTION_SET:
			case KEY_OPTION_REMOVE:
				yyerror("combining url type and the given "
				    "option is not supported");
				YYERROR;
				break;
			}
			keytype = KEY_TYPE_URL;
			rule->rule_kv[keytype].kv_option = $2;
			rule->rule_kv[keytype].kv_type = keytype;
		}
		| FORWARD TO table				{
			if (table_findbyname(conf, $3) == NULL) {
				yyerror("undefined forward table");
				free($3);
				YYERROR;
			}
			if (strlcpy(rule->rule_tablename, $3,
			    sizeof(rule->rule_tablename)) >=
			    sizeof(rule->rule_tablename)) {
				yyerror("invalid forward table name");
				free($3);
				YYERROR;
			}
			free($3);
		}
		| TAG STRING					{
			tag = tag_name2id($2);
			if (rule->rule_tag) {
				yyerror("tag already defined");
				free($2);
				rule_free(rule);
				free(rule);
				YYERROR;
			}
			if (tag == 0) {
				yyerror("invalid tag");
				free($2);
				rule_free(rule);
				free(rule);
				YYERROR;
			}
			rule->rule_tag = tag;
			if (strlcpy(rule->rule_tagname, $2,
			    sizeof(rule->rule_tagname)) >=
			    sizeof(rule->rule_tagname)) {
				yyerror("tag truncated");
				free($2);
				rule_free(rule);
				free(rule);
				YYERROR;
			}
			free($2);
		}
		| NO TAG					{
			if (tag == 0) {
				yyerror("no tag defined");
				YYERROR;
			}
			rule->rule_tag = -1;
			memset(rule->rule_tagname, 0,
			    sizeof(rule->rule_tagname));
		}
		| TAGGED STRING					{
			tagged = tag_name2id($2);
			if (rule->rule_tagged) {
				yyerror("tagged already defined");
				free($2);
				rule_free(rule);
				free(rule);
				YYERROR;
			}
			if (tagged == 0) {
				yyerror("invalid tag");
				free($2);
				rule_free(rule);
				free(rule);
				YYERROR;
			}
			rule->rule_tagged = tagged;
			if (strlcpy(rule->rule_taggedname, $2,
			    sizeof(rule->rule_taggedname)) >=
			    sizeof(rule->rule_taggedname)) {
				yyerror("tagged truncated");
				free($2);
				rule_free(rule);
				free(rule);
				YYERROR;
			}
			free($2);
		}
		| LABEL STRING					{
			label = label_name2id($2);
			if (rule->rule_label) {
				yyerror("label already defined");
				free($2);
				rule_free(rule);
				free(rule);
				YYERROR;
			}
			if (label == 0) {
				yyerror("invalid label");
				free($2);
				rule_free(rule);
				free(rule);
				YYERROR;
			}
			rule->rule_label = label;
			if (strlcpy(rule->rule_labelname, $2,
			    sizeof(rule->rule_labelname)) >=
			    sizeof(rule->rule_labelname)) {
				yyerror("label truncated");
				free($2);
				rule_free(rule);
				free(rule);
				YYERROR;
			}
			free($2);
		}
		| NO LABEL					{
			if (label == 0) {
				yyerror("no label defined");
				YYERROR;
			}
			rule->rule_label = -1;
			memset(rule->rule_labelname, 0,
			    sizeof(rule->rule_labelname));
		}
		| FILENAME STRING value				{
			if (rulefile != NULL) {
				yyerror("only one file per rule supported");
				free($2);
				free($3);
				rule_free(rule);
				free(rule);
				YYERROR;
			}
			if ($3) {
				if ((rule->rule_kv[keytype].kv_value =
				    strdup($3)) == NULL)
					fatal("out of memory");
				free($3);
			} else
				rule->rule_kv[keytype].kv_value = NULL;
			rulefile = $2;
		}
		;

value		: /* empty */		{ $$ = NULL; }
		| VALUE STRING		{ $$ = $2; }
		;

key_option	: /* empty */		{ $$ = KEY_OPTION_NONE; }
		| APPEND		{ $$ = KEY_OPTION_APPEND; }
		| SET			{ $$ = KEY_OPTION_SET; }
		| REMOVE		{ $$ = KEY_OPTION_REMOVE; }
		| HASH			{ $$ = KEY_OPTION_HASH; }
		| LOG			{ $$ = KEY_OPTION_LOG; }
		;

tlsdhparams	: /* empty */		{ $$ = TLSDHPARAMS_MIN; }
		| PARAMS NUMBER		{
			if ($2 < TLSDHPARAMS_MIN) {
				yyerror("EDH params not supported: %d", $2);
				YYERROR;
			}
			$$ = $2;
		}
		;

tlsecdhcurve	: /* empty */		{ $$ = TLSECDHCURVE_DEFAULT; }
		| CURVE STRING		{
			if (strcmp("none", $2) == 0)
				$$ = 0;
			else if ((proto->tlsecdhcurve = OBJ_sn2nid($2)) == 0) {
				yyerror("ECDH curve not supported");
				free($2);
				YYERROR;
			}
			free($2);
		}
		;

relay		: RELAY STRING	{
			struct relay *r;

			if (!loadcfg) {
				free($2);
				YYACCEPT;
			}

			TAILQ_FOREACH(r, conf->sc_relays, rl_entry)
				if (!strcmp(r->rl_conf.name, $2))
					break;
			if (r != NULL) {
				yyerror("relay %s defined twice", $2);
				free($2);
				YYERROR;
			}
			TAILQ_INIT(&relays);

			if ((r = calloc(1, sizeof (*r))) == NULL)
				fatal("out of memory");

			if (strlcpy(r->rl_conf.name, $2,
			    sizeof(r->rl_conf.name)) >=
			    sizeof(r->rl_conf.name)) {
				yyerror("relay name truncated");
				free($2);
				free(r);
				YYERROR;
			}
			free($2);
			if (relay_id(r) == -1) {
				yyerror("too many relays defined");
				free(r);
				YYERROR;
			}
			r->rl_conf.timeout.tv_sec = RELAY_TIMEOUT;
			r->rl_proto = NULL;
			r->rl_conf.proto = EMPTY_ID;
			r->rl_conf.dstretry = 0;
			TAILQ_INIT(&r->rl_tables);
			if (last_relay_id == INT_MAX) {
				yyerror("too many relays defined");
				free(r);
				YYERROR;
			}
			dstmode = RELAY_DSTMODE_DEFAULT;
			rlay = r;
		} '{' optnl relayopts_l '}'	{
			struct relay	*r;

			if (rlay->rl_conf.ss.ss_family == AF_UNSPEC) {
				yyerror("relay %s has no listener",
				    rlay->rl_conf.name);
				YYERROR;
			}
			if ((rlay->rl_conf.flags & (F_NATLOOK|F_DIVERT)) ==
			    (F_NATLOOK|F_DIVERT)) {
				yyerror("relay %s with conflicting nat lookup "
				    "and peer options", rlay->rl_conf.name);
				YYERROR;
			}
			if ((rlay->rl_conf.flags & (F_NATLOOK|F_DIVERT)) == 0 &&
			    rlay->rl_conf.dstss.ss_family == AF_UNSPEC &&
			    TAILQ_EMPTY(&rlay->rl_tables)) {
				yyerror("relay %s has no target, rdr, "
				    "or table", rlay->rl_conf.name);
				YYERROR;
			}
			if (rlay->rl_conf.proto == EMPTY_ID) {
				rlay->rl_proto = &conf->sc_proto_default;
				rlay->rl_conf.proto = conf->sc_proto_default.id;
			}
			if (relay_load_certfiles(rlay) == -1) {
				yyerror("cannot load certificates for relay %s",
				    rlay->rl_conf.name);
				YYERROR;
			}
			conf->sc_relaycount++;
			SPLAY_INIT(&rlay->rl_sessions);
			TAILQ_INSERT_TAIL(conf->sc_relays, rlay, rl_entry);

			tableport = 0;

			while ((r = TAILQ_FIRST(&relays)) != NULL) {
				TAILQ_REMOVE(&relays, r, rl_entry);
				if (relay_inherit(rlay, r) == NULL) {
					YYERROR;
				}
			}
			rlay = NULL;
		}
		;

relayopts_l	: relayopts_l relayoptsl nl
		| relayoptsl optnl
		;

relayoptsl	: LISTEN ON STRING port opttls {
			struct addresslist	 al;
			struct address		*h;
			struct relay		*r;

			if (rlay->rl_conf.ss.ss_family != AF_UNSPEC) {
				if ((r = calloc(1, sizeof (*r))) == NULL)
					fatal("out of memory");
				TAILQ_INSERT_TAIL(&relays, r, rl_entry);
			} else
				r = rlay;
			if ($4.op != PF_OP_EQ) {
				yyerror("invalid port");
				free($3);
				YYERROR;
			}

			TAILQ_INIT(&al);
			if (host($3, &al, 1, &$4, NULL, -1) <= 0) {
				yyerror("invalid listen ip: %s", $3);
				free($3);
				YYERROR;
			}
			free($3);
			h = TAILQ_FIRST(&al);
			bcopy(&h->ss, &r->rl_conf.ss, sizeof(r->rl_conf.ss));
			r->rl_conf.port = h->port.val[0];
			if ($5) {
				r->rl_conf.flags |= F_TLS;
				conf->sc_flags |= F_TLS;
			}
			tableport = h->port.val[0];
			host_free(&al);
		}
		| forwardmode opttlsclient TO forwardspec dstaf {
			rlay->rl_conf.fwdmode = $1;
			if ($1 == FWD_ROUTE) {
				yyerror("no route for relays");
				YYERROR;
			}
			if ($2) {
				rlay->rl_conf.flags |= F_TLSCLIENT;
				conf->sc_flags |= F_TLSCLIENT;
			}
		}
		| SESSION TIMEOUT NUMBER		{
			if ((rlay->rl_conf.timeout.tv_sec = $3) < 0) {
				yyerror("invalid timeout: %lld", $3);
				YYERROR;
			}
			if (rlay->rl_conf.timeout.tv_sec > INT_MAX) {
				yyerror("timeout too large: %lld", $3);
				YYERROR;
			}
		}
		| PROTO STRING			{
			struct protocol *p;

			TAILQ_FOREACH(p, conf->sc_protos, entry)
				if (!strcmp(p->name, $2))
					break;
			if (p == NULL) {
				yyerror("no such protocol: %s", $2);
				free($2);
				YYERROR;
			}
			p->flags |= F_USED;
			rlay->rl_conf.proto = p->id;
			rlay->rl_proto = p;
			free($2);
		}
		| DISABLE		{ rlay->rl_conf.flags |= F_DISABLE; }
		| include
		;

forwardspec	: STRING port retry	{
			struct addresslist	 al;
			struct address		*h;

			if (rlay->rl_conf.dstss.ss_family != AF_UNSPEC) {
				yyerror("relay %s target or redirection "
				    "already specified", rlay->rl_conf.name);
				free($1);
				YYERROR;
			}
			if ($2.op != PF_OP_EQ) {
				yyerror("invalid port");
				free($1);
				YYERROR;
			}

			TAILQ_INIT(&al);
			if (host($1, &al, 1, &$2, NULL, -1) <= 0) {
				yyerror("invalid listen ip: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
			h = TAILQ_FIRST(&al);
			bcopy(&h->ss, &rlay->rl_conf.dstss,
			    sizeof(rlay->rl_conf.dstss));
			rlay->rl_conf.dstport = h->port.val[0];
			rlay->rl_conf.dstretry = $3;
			host_free(&al);
		}
		| NAT LOOKUP retry	{
			conf->sc_flags |= F_NEEDPF;
			rlay->rl_conf.flags |= F_NATLOOK;
			rlay->rl_conf.dstretry = $3;
		}
		| DESTINATION retry		{
			conf->sc_flags |= F_NEEDPF;
			rlay->rl_conf.flags |= F_DIVERT;
			rlay->rl_conf.dstretry = $2;
		}
		| tablespec	{
			struct relay_table	*rlt;

			if ((rlt = calloc(1, sizeof(*rlt))) == NULL) {
				yyerror("failed to allocate table reference");
				YYERROR;
			}

			rlt->rlt_table = $1;
			rlt->rlt_table->conf.flags |= F_USED;
			rlt->rlt_mode = dstmode;
			rlt->rlt_flags = F_USED;
			if (!TAILQ_EMPTY(&rlay->rl_tables))
				rlt->rlt_flags |= F_BACKUP;

			if (hashkey != NULL &&
			    (rlay->rl_conf.flags & F_HASHKEY) == 0) {
				memcpy(&rlay->rl_conf.hashkey,
				    hashkey, sizeof(rlay->rl_conf.hashkey));
				rlay->rl_conf.flags |= F_HASHKEY;
			}
			free(hashkey);
			hashkey = NULL;

			TAILQ_INSERT_TAIL(&rlay->rl_tables, rlt, rlt_entry);
		}
		;

dstmode		: /* empty */		{ $$ = RELAY_DSTMODE_DEFAULT; }
		| LOADBALANCE		{ $$ = RELAY_DSTMODE_LOADBALANCE; }
		| ROUNDROBIN		{ $$ = RELAY_DSTMODE_ROUNDROBIN; }
		| HASH			{ $$ = RELAY_DSTMODE_HASH; }
		| LEASTSTATES		{ $$ = RELAY_DSTMODE_LEASTSTATES; }
		| SRCHASH		{ $$ = RELAY_DSTMODE_SRCHASH; }
		| RANDOM		{ $$ = RELAY_DSTMODE_RANDOM; }
		;

router		: ROUTER STRING		{
			struct router *rt = NULL;

			if (!loadcfg) {
				free($2);
				YYACCEPT;
			}

			conf->sc_flags |= F_NEEDRT;
			TAILQ_FOREACH(rt, conf->sc_rts, rt_entry)
				if (!strcmp(rt->rt_conf.name, $2))
					break;
			if (rt != NULL) {
				yyerror("router %s defined twice", $2);
				free($2);
				YYERROR;
			}

			if ((rt = calloc(1, sizeof (*rt))) == NULL)
				fatal("out of memory");

			if (strlcpy(rt->rt_conf.name, $2,
			    sizeof(rt->rt_conf.name)) >=
			    sizeof(rt->rt_conf.name)) {
				yyerror("router name truncated");
				free(rt);
				YYERROR;
			}
			free($2);
			rt->rt_conf.id = ++last_rt_id;
			if (last_rt_id == INT_MAX) {
				yyerror("too many routers defined");
				free(rt);
				YYERROR;
			}
			TAILQ_INIT(&rt->rt_netroutes);
			router = rt;

			tableport = -1;
		} '{' optnl routeopts_l '}'	{
			if (!router->rt_conf.nroutes) {
				yyerror("router %s without routes",
				    router->rt_conf.name);
				free(router);
				router = NULL;
				YYERROR;
			}

			conf->sc_routercount++;
			TAILQ_INSERT_TAIL(conf->sc_rts, router, rt_entry);
			router = NULL;

			tableport = 0;
		}
		;

routeopts_l	: routeopts_l routeoptsl nl
		| routeoptsl optnl
		;

routeoptsl	: ROUTE address '/' NUMBER {
			struct netroute	*nr;

			if (router->rt_conf.af == AF_UNSPEC)
				router->rt_conf.af = $2.ss.ss_family;
			else if (router->rt_conf.af != $2.ss.ss_family) {
				yyerror("router %s address family mismatch",
				    router->rt_conf.name);
				YYERROR;
			}

			if ((router->rt_conf.af == AF_INET &&
			    ($4 > 32 || $4 < 0)) ||
			    (router->rt_conf.af == AF_INET6 &&
			    ($4 > 128 || $4 < 0))) {
				yyerror("invalid prefixlen %d", $4);
				YYERROR;
			}

			if ((nr = calloc(1, sizeof(*nr))) == NULL)
				fatal("out of memory");

			nr->nr_conf.id = ++last_nr_id;
			if (last_nr_id == INT_MAX) {
				yyerror("too many routes defined");
				free(nr);
				YYERROR;
			}
			nr->nr_conf.prefixlen = $4;
			nr->nr_conf.routerid = router->rt_conf.id;
			nr->nr_router = router;
			bcopy(&$2.ss, &nr->nr_conf.ss, sizeof($2.ss));

			router->rt_conf.nroutes++;
			conf->sc_routecount++;
			TAILQ_INSERT_TAIL(&router->rt_netroutes, nr, nr_entry);
			TAILQ_INSERT_TAIL(conf->sc_routes, nr, nr_route);
		}
		| FORWARD TO tablespec {
			free(hashkey);
			hashkey = NULL;

			if (router->rt_gwtable) {
				yyerror("router %s table already specified",
				    router->rt_conf.name);
				purge_table(conf->sc_tables, $3);
				YYERROR;
			}
			router->rt_gwtable = $3;
			router->rt_gwtable->conf.flags |= F_USED;
			router->rt_conf.gwtable = $3->conf.id;
			router->rt_conf.gwport = $3->conf.port;
		}
		| RTABLE NUMBER {
			if (router->rt_conf.rtable) {
				yyerror("router %s rtable already specified",
				    router->rt_conf.name);
				YYERROR;
			}
			if ($2 < 0 || $2 > RT_TABLEID_MAX) {
				yyerror("invalid rtable id %d", $2);
				YYERROR;
			}
			router->rt_conf.rtable = $2;
		}
		| RTLABEL STRING {
			if (strlcpy(router->rt_conf.label, $2,
			    sizeof(router->rt_conf.label)) >=
			    sizeof(router->rt_conf.label)) {
				yyerror("route label truncated");
				free($2);
				YYERROR;
			}
			free($2);
		}
		| DISABLE		{ rlay->rl_conf.flags |= F_DISABLE; }
		| include
		;

dstaf		: /* empty */		{
			rlay->rl_conf.dstaf.ss_family = AF_UNSPEC;
		}
		| INET			{
			rlay->rl_conf.dstaf.ss_family = AF_INET;
		}
		| INET6	STRING		{
			struct sockaddr_in6	*sin6;

			sin6 = (struct sockaddr_in6 *)&rlay->rl_conf.dstaf;
			if (inet_pton(AF_INET6, $2, &sin6->sin6_addr) == -1) {
				yyerror("invalid ipv6 address %s", $2);
				free($2);
				YYERROR;
			}
			free($2);

			sin6->sin6_family = AF_INET6;
			sin6->sin6_len = sizeof(*sin6);
		}
		;

interface	: /* empty */		{ $$ = NULL; }
		| INTERFACE STRING	{ $$ = $2; }
		;

host		: address	{
			if ((hst = calloc(1, sizeof(*(hst)))) == NULL)
				fatal("out of memory");

			if (strlcpy(hst->conf.name, $1.name,
			    sizeof(hst->conf.name)) >= sizeof(hst->conf.name)) {
				yyerror("host name truncated");
				free(hst);
				YYERROR;
			}
			bcopy(&$1.ss, &hst->conf.ss, sizeof($1.ss));
			hst->conf.id = 0; /* will be set later */
			SLIST_INIT(&hst->children);
		} opthostflags {
			$$ = hst;
			hst = NULL;
		}
		;

opthostflags	: /* empty */
		| hostflags_l
		;

hostflags_l	: hostflags hostflags_l
		| hostflags
		;

hostflags	: RETRY NUMBER		{
			if (hst->conf.retry) {
				yyerror("retry value already set");
				YYERROR;
			}
			if ($2 < 0) {
				yyerror("invalid retry value: %d\n", $2);
				YYERROR;
			}
			hst->conf.retry = $2;
		}
		| PARENT NUMBER		{
			if (hst->conf.parentid) {
				yyerror("parent value already set");
				YYERROR;
			}
			if ($2 < 0) {
				yyerror("invalid parent value: %d\n", $2);
				YYERROR;
			}
			hst->conf.parentid = $2;
		}
		| PRIORITY NUMBER		{
			if (hst->conf.priority) {
				yyerror("priority already set");
				YYERROR;
			}
			if ($2 < 0 || $2 > RTP_MAX) {
				yyerror("invalid priority value: %d\n", $2);
				YYERROR;
			}
			hst->conf.priority = $2;
		}
		| IP TTL NUMBER		{
			if (hst->conf.ttl) {
				yyerror("ttl value already set");
				YYERROR;
			}
			if ($3 < 0) {
				yyerror("invalid ttl value: %d\n", $3);
				YYERROR;
			}
			hst->conf.ttl = $3;
		}
		;

address		: STRING	{
			struct address *h;
			struct addresslist al;

			if (strlcpy($$.name, $1,
			    sizeof($$.name)) >= sizeof($$.name)) {
				yyerror("host name truncated");
				free($1);
				YYERROR;
			}

			TAILQ_INIT(&al);
			if (host($1, &al, 1, NULL, NULL, -1) <= 0) {
				yyerror("invalid host %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
			h = TAILQ_FIRST(&al);
			memcpy(&$$.ss, &h->ss, sizeof($$.ss));
			host_free(&al);
		}
		;

retry		: /* empty */		{ $$ = 0; }
		| RETRY NUMBER		{
			if (($$ = $2) < 0) {
				yyerror("invalid retry value: %d\n", $2);
				YYERROR;
			}
		}
		;

timeout		: NUMBER
		{
			if ($1 < 0) {
				yyerror("invalid timeout: %d\n", $1);
				YYERROR;
			}
			$$.tv_sec = $1 / 1000;
			$$.tv_usec = ($1 % 1000) * 1000;
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

optstring	: STRING		{ $$ = $1; }
		| /* nothing */		{ $$ = NULL; }
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
		{ "all",		ALL },
		{ "append",		APPEND },
		{ "backlog",		BACKLOG },
		{ "backup",		BACKUP },
		{ "block",		BLOCK },
		{ "buffer",		BUFFER },
		{ "ca",			CA },
		{ "cache",		CACHE },
		{ "cert",		CERTIFICATE },
		{ "check",		CHECK },
		{ "ciphers",		CIPHERS },
		{ "code",		CODE },
		{ "cookie",		COOKIE },
		{ "curve",		CURVE },
		{ "demote",		DEMOTE },
		{ "destination",	DESTINATION },
		{ "digest",		DIGEST },
		{ "disable",		DISABLE },
		{ "ecdh",		ECDH },
		{ "edh",		EDH },
		{ "error",		ERROR },
		{ "expect",		EXPECT },
		{ "external",		EXTERNAL },
		{ "file",		FILENAME },
		{ "forward",		FORWARD },
		{ "from",		FROM },
		{ "hash",		HASH },
		{ "header",		HEADER },
		{ "host",		HOST },
		{ "icmp",		ICMP },
		{ "include",		INCLUDE },
		{ "inet",		INET },
		{ "inet6",		INET6 },
		{ "interface",		INTERFACE },
		{ "interval",		INTERVAL },
		{ "ip",			IP },
		{ "key",		KEY },
		{ "label",		LABEL },
		{ "least-states",	LEASTSTATES },
		{ "listen",		LISTEN },
		{ "loadbalance",	LOADBALANCE },
		{ "log",		LOG },
		{ "lookup",		LOOKUP },
		{ "match",		MATCH },
		{ "method",		METHOD },
		{ "mode",		MODE },
		{ "nat",		NAT },
		{ "no",			NO },
		{ "nodelay",		NODELAY },
		{ "nothing",		NOTHING },
		{ "on",			ON },
		{ "params",		PARAMS },
		{ "parent",		PARENT },
		{ "pass",		PASS },
		{ "password",		PASSWORD },
		{ "path",		PATH },
		{ "pftag",		PFTAG },
		{ "port",		PORT },
		{ "prefork",		PREFORK },
		{ "priority",		PRIORITY },
		{ "protocol",		PROTO },
		{ "query",		QUERYSTR },
		{ "quick",		QUICK },
		{ "random",		RANDOM },
		{ "real",		REAL },
		{ "redirect",		REDIRECT },
		{ "relay",		RELAY },
		{ "remove",		REMOVE },
		{ "request",		REQUEST },
		{ "response",		RESPONSE },
		{ "retry",		RETRY },
		{ "return",		RETURN },
		{ "roundrobin",		ROUNDROBIN },
		{ "route",		ROUTE },
		{ "router",		ROUTER },
		{ "rtable",		RTABLE },
		{ "rtlabel",		RTLABEL },
		{ "sack",		SACK },
		{ "script",		SCRIPT },
		{ "send",		SEND },
		{ "session",		SESSION },
		{ "set",		SET },
		{ "snmp",		SNMP },
		{ "socket",		SOCKET },
		{ "source-hash",	SRCHASH },
		{ "splice",		SPLICE },
		{ "ssl",		SSL },
		{ "sticky-address",	STICKYADDR },
		{ "style",		STYLE },
		{ "table",		TABLE },
		{ "tag",		TAG },
		{ "tagged",		TAGGED },
		{ "tcp",		TCP },
		{ "timeout",		TIMEOUT },
		{ "tls",		TLS },
		{ "to",			TO },
		{ "transparent",	TRANSPARENT },
		{ "trap",		TRAP },
		{ "ttl",		TTL },
		{ "updates",		UPDATES },
		{ "url",		URL },
		{ "value",		VALUE },
		{ "virtual",		VIRTUAL },
		{ "with",		WITH }
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
	x != ',' && x != '/'))

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
parse_config(const char *filename, struct relayd *x_conf)
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
load_config(const char *filename, struct relayd *x_conf)
{
	struct sym		*sym, *next;
	struct table		*nexttb;
	struct host		*h, *ph;
	struct relay_table	*rlt;

	conf = x_conf;
	conf->sc_flags = 0;

	loadcfg = 1;
	errors = 0;
	last_host_id = last_table_id = last_rdr_id = last_proto_id =
	    last_relay_id = last_rt_id = last_nr_id = 0;

	rdr = NULL;
	table = NULL;
	rlay = NULL;
	proto = NULL;
	router = NULL;

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
		if ((conf->sc_opts & RELAYD_OPT_VERBOSE) && !sym->used)
			fprintf(stderr, "warning: macro '%s' not "
			    "used\n", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
			free(sym);
		}
	}

	if (TAILQ_EMPTY(conf->sc_rdrs) &&
	    TAILQ_EMPTY(conf->sc_relays) &&
	    TAILQ_EMPTY(conf->sc_rts)) {
		log_warnx("no actions, nothing to do");
		errors++;
	}

	/* Cleanup relay list to inherit */
	while ((rlay = TAILQ_FIRST(&relays)) != NULL) {
		TAILQ_REMOVE(&relays, rlay, rl_entry);
		while ((rlt = TAILQ_FIRST(&rlay->rl_tables))) {
			TAILQ_REMOVE(&rlay->rl_tables, rlt, rlt_entry);
			free(rlt);
		}
		free(rlay);
	}

	if (timercmp(&conf->sc_timeout, &conf->sc_interval, >=)) {
		log_warnx("global timeout exceeds interval");
		errors++;
	}

	/* Verify that every table is used */
	for (table = TAILQ_FIRST(conf->sc_tables); table != NULL;
	     table = nexttb) {
		nexttb = TAILQ_NEXT(table, entry);
		if (table->conf.port == 0) {
			TAILQ_REMOVE(conf->sc_tables, table, entry);
			while ((h = TAILQ_FIRST(&table->hosts)) != NULL) {
				TAILQ_REMOVE(&table->hosts, h, entry);
				free(h);
			}
			if (table->sendbuf != NULL)
				free(table->sendbuf);
			free(table);
			continue;
		}

		TAILQ_FOREACH(h, &table->hosts, entry) {
			if (h->conf.parentid) {
				ph = host_find(conf, h->conf.parentid);

				/* Validate the parent id */
				if (h->conf.id == h->conf.parentid ||
				    ph == NULL || ph->conf.parentid)
					ph = NULL;

				if (ph == NULL) {
					log_warnx("host parent id %d invalid",
					    h->conf.parentid);
					errors++;
				} else
					SLIST_INSERT_HEAD(&ph->children,
					    h, child);
			}
		}

		if (!(table->conf.flags & F_USED)) {
			log_warnx("unused table: %s", table->conf.name);
			errors++;
		}
		if (timercmp(&table->conf.timeout, &conf->sc_interval, >=)) {
			log_warnx("table timeout exceeds interval: %s",
			    table->conf.name);
			errors++;
		}
	}

	/* Verify that every non-default protocol is used */
	TAILQ_FOREACH(proto, conf->sc_protos, entry) {
		if (!(proto->flags & F_USED)) {
			log_warnx("unused protocol: %s", proto->name);
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
	struct addrinfo		 hints, *res;
	struct sockaddr_in6	*sa_in6;
	struct address		*h = NULL;

	bzero(&hints, sizeof(hints));
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

	bzero(&hints, sizeof(hints));
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
			bcopy(port, &h->port, sizeof(h->port));
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
			bcopy(port, &h->port, sizeof(h->port));
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

	h = host_v4(s);

	/* IPv6 address? */
	if (h == NULL)
		h = host_v6(s);

	if (h != NULL) {
		if (port != NULL)
			bcopy(port, &h->port, sizeof(h->port));
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

struct table *
table_inherit(struct table *tb)
{
	char		pname[TABLE_NAME_SIZE + 6];
	struct host	*h, *dsth;
	struct table	*dsttb, *oldtb;

	/* Get the table or table template */
	if ((dsttb = table_findbyname(conf, tb->conf.name)) == NULL) {
		yyerror("unknown table %s", tb->conf.name);
		goto fail;
	}
	if (dsttb->conf.port != 0)
		fatal("invalid table");	/* should not happen */

	if (tb->conf.port == 0) {
		yyerror("invalid port");
		goto fail;
	}

	/* Check if a matching table already exists */
	if (snprintf(pname, sizeof(pname), "%s:%u",
	    tb->conf.name, ntohs(tb->conf.port)) >= (int)sizeof(pname)) {
		yyerror("invalid table name");
		goto fail;
	}
	if (strlcpy(tb->conf.name, pname, sizeof(tb->conf.name)) >=
	    sizeof(tb->conf.name)) {
		yyerror("invalid table mame");
		goto fail;
	}
	if ((oldtb = table_findbyconf(conf, tb)) != NULL) {
		purge_table(NULL, tb);
		return (oldtb);
	}

	/* Create a new table */
	tb->conf.id = ++last_table_id;
	if (last_table_id == INT_MAX) {
		yyerror("too many tables defined");
		goto fail;
	}
	tb->conf.flags |= dsttb->conf.flags;

	/* Inherit global table options */
	if (tb->conf.timeout.tv_sec == 0 && tb->conf.timeout.tv_usec == 0)
		bcopy(&dsttb->conf.timeout, &tb->conf.timeout,
		    sizeof(struct timeval));

	/* Copy the associated hosts */
	TAILQ_INIT(&tb->hosts);
	TAILQ_FOREACH(dsth, &dsttb->hosts, entry) {
		if ((h = (struct host *)
		    calloc(1, sizeof (*h))) == NULL)
			fatal("out of memory");
		bcopy(dsth, h, sizeof(*h));
		h->conf.id = ++last_host_id;
		if (last_host_id == INT_MAX) {
			yyerror("too many hosts defined");
			free(h);
			goto fail;
		}
		h->conf.tableid = tb->conf.id;
		h->tablename = tb->conf.name;
		SLIST_INIT(&h->children);
		TAILQ_INSERT_TAIL(&tb->hosts, h, entry);
		TAILQ_INSERT_TAIL(&conf->sc_hosts, h, globalentry);
	}

	conf->sc_tablecount++;
	TAILQ_INSERT_TAIL(conf->sc_tables, tb, entry);

	return (tb);

 fail:
	purge_table(NULL, tb);
	return (NULL);
}

int
relay_id(struct relay *rl)
{
	rl->rl_conf.id = ++last_relay_id;
	rl->rl_conf.tls_keyid = ++last_key_id;
	rl->rl_conf.tls_cakeyid = ++last_key_id;

	if (last_relay_id == INT_MAX || last_key_id == INT_MAX)
		return (-1);

	return (0);
}

struct relay *
relay_inherit(struct relay *ra, struct relay *rb)
{
	struct relay_config	 rc;
	struct relay_table	*rta, *rtb;

	bcopy(&rb->rl_conf, &rc, sizeof(rc));
	bcopy(ra, rb, sizeof(*rb));

	bcopy(&rc.ss, &rb->rl_conf.ss, sizeof(rb->rl_conf.ss));
	rb->rl_conf.port = rc.port;
	rb->rl_conf.flags =
	    (ra->rl_conf.flags & ~F_TLS) | (rc.flags & F_TLS);
	if (!(rb->rl_conf.flags & F_TLS)) {
		rb->rl_tls_cert = NULL;
		rb->rl_conf.tls_cert_len = 0;
		rb->rl_tls_key = NULL;
		rb->rl_conf.tls_key_len = 0;
	}
	TAILQ_INIT(&rb->rl_tables);

	if (relay_id(rb) == -1) {
		yyerror("too many relays defined");
		goto err;
	}

	if (snprintf(rb->rl_conf.name, sizeof(rb->rl_conf.name), "%s%u:%u",
	    ra->rl_conf.name, rb->rl_conf.id, ntohs(rc.port)) >=
	    (int)sizeof(rb->rl_conf.name)) {
		yyerror("invalid relay name");
		goto err;
	}

	if (relay_findbyname(conf, rb->rl_conf.name) != NULL ||
	    relay_findbyaddr(conf, &rb->rl_conf) != NULL) {
		yyerror("relay %s defined twice", rb->rl_conf.name);
		goto err;
	}
	if (relay_load_certfiles(rb) == -1) {
		yyerror("cannot load certificates for relay %s",
		    rb->rl_conf.name);
		goto err;
	}

	TAILQ_FOREACH(rta, &ra->rl_tables, rlt_entry) {
		if ((rtb = calloc(1, sizeof(*rtb))) == NULL) {
			yyerror("cannot allocate relay table");
			goto err;
		}
		rtb->rlt_table = rta->rlt_table;
		rtb->rlt_mode = rta->rlt_mode;
		rtb->rlt_flags = rta->rlt_flags;

		TAILQ_INSERT_TAIL(&rb->rl_tables, rtb, rlt_entry);
	}

	conf->sc_relaycount++;
	SPLAY_INIT(&rlay->rl_sessions);
	TAILQ_INSERT_TAIL(conf->sc_relays, rb, rl_entry);

	return (rb);

 err:
	while ((rtb = TAILQ_FIRST(&rb->rl_tables))) {
		TAILQ_REMOVE(&rb->rl_tables, rtb, rlt_entry);
		free(rtb);
	}
	free(rb);
	return (NULL);
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

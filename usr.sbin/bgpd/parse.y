/*	$OpenBSD: parse.y,v 1.84 2004/04/26 08:51:10 henning Exp $ */

/*
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
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "bgpd.h"
#include "mrt.h"
#include "session.h"

static struct bgpd_config	*conf;
static struct mrt_head		*mrtconf;
static struct network_head	*netconf;
static struct peer		*peer_l, *peer_l_old;
static struct peer		*curpeer;
static struct peer		*curgroup;
static struct filter_head	*filter_l;
static FILE			*fin = NULL;
static int			 lineno = 1;
static int			 errors = 0;
static int			 pdebug = 1;
static u_int32_t		 id;
char				*infile;

int	 yyerror(const char *, ...);
int	 yyparse(void);
int	 kw_cmp(const void *, const void *);
int	 lookup(char *);
int	 lgetc(FILE *);
int	 lungetc(int);
int	 findeol(void);
int	 yylex(void);

struct peer	*alloc_peer(void);
struct peer	*new_peer(void);
struct peer	*new_group(void);
int		 add_mrtconfig(enum mrt_type, char *, time_t, struct peer *);
int		 get_id(struct peer *);
int		 expand_rule(struct filter_rule *, struct filter_peers *,
		    struct filter_match *, struct filter_set *);

TAILQ_HEAD(symhead, sym)	 symhead = TAILQ_HEAD_INITIALIZER(symhead);
struct sym {
	TAILQ_ENTRY(sym)	 entries;
	int			 used;
	int			 persist;
	char			*nam;
	char			*val;
};

int	 symset(const char *, const char *, int);
char	*symget(const char *);
int	 atoul(char *, u_long *);
int	 getcommunity(char *);

typedef struct {
	union {
		u_int32_t		 number;
		char			*string;
		struct bgpd_addr	 addr;
		u_int8_t		 u8;
		struct filter_peers	 filter_peers;
		struct filter_match	 filter_match;
		struct filter_set	 filter_set;
		struct {
			struct bgpd_addr	prefix;
			u_int8_t		len;
		}			prefix;
		struct {
			u_int8_t		enc_alg;
			char			enc_key[IPSEC_ENC_KEY_LEN];
			u_int8_t		enc_key_len;
		}			encspec;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	AS ROUTERID HOLDTIME YMIN LISTEN ON FIBUPDATE
%token	GROUP NEIGHBOR NETWORK
%token	REMOTEAS DESCR LOCALADDR MULTIHOP PASSIVE MAXPREFIX ANNOUNCE
%token	ENFORCE NEIGHBORAS CAPABILITIES
%token	DUMP TABLE IN OUT
%token	LOG ROUTECOLL
%token	TCP MD5SIG PASSWORD KEY
%token	ALLOW DENY MATCH
%token	QUICK
%token	FROM TO ANY
%token	PREFIX PREFIXLEN SOURCEAS TRANSITAS COMMUNITY
%token	SET LOCALPREF MED NEXTHOP PREPEND
%token	ERROR
%token	IPSEC ESP AH SPI
%token	<v.string>		STRING
%type	<v.number>		number asnumber optnumber yesno inout
%type	<v.string>		string
%type	<v.addr>		address
%type	<v.prefix>		prefix addrspec
%type	<v.u8>			action quick direction
%type	<v.filter_peers>	filter_peer
%type	<v.filter_match>	filter_match prefixlenop
%type	<v.filter_set>		filter_set filter_set_l filter_set_opt
%type	<v.u8>			unaryop binaryop filter_as
%type	<v.encspec>		encspec;
%%

grammar		: /* empty */
		| grammar '\n'
		| grammar conf_main '\n'
		| grammar varset '\n'
		| grammar neighbor '\n'
		| grammar group '\n'
		| grammar filterrule '\n'
		| grammar error '\n'		{ errors++; }
		;

number		: STRING			{
			u_long	ulval;

			if (atoul($1, &ulval) == -1) {
				yyerror("\"%s\" is not a number", $1);
				free($1);
				YYERROR;
			} else
				$$ = ulval;
			free($1);
		}
		;

asnumber	: number			{
			if ($1 > USHRT_MAX) {
				yyerror("AS too big: max %u", USHRT_MAX);
				YYERROR;
			}
		}

string		: string STRING				{
			if (asprintf(&$$, "%s %s", $1, $2) == -1)
				fatal("string: asprintf");
			free($1);
			free($2);
		}
		| STRING
		;

yesno		:  STRING		{
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
			if (conf->opts & BGPD_OPT_VERBOSE)
				printf("%s = \"%s\"\n", $1, $3);
			if (symset($1, $3, 0) == -1)
				fatal("cannot store variable");
			free($1);
			free($3);
		}
		;

conf_main	: AS asnumber		{
			conf->as = $2;
		}
		| ROUTERID address		{
			if ($2.af != AF_INET) {
				yyerror("router-id must be an IPv4 address");
				YYERROR;
			}
			conf->bgpid = $2.v4.s_addr;
		}
		| HOLDTIME number	{
			if ($2 < MIN_HOLDTIME) {
				yyerror("holdtime must be at least %u",
				    MIN_HOLDTIME);
				YYERROR;
			}
			conf->holdtime = $2;
		}
		| HOLDTIME YMIN number	{
			if ($3 < MIN_HOLDTIME) {
				yyerror("holdtime min must be at least %u",
				    MIN_HOLDTIME);
				YYERROR;
			}
			conf->min_holdtime = $3;
		}
		| LISTEN ON address	{
			switch ($3.af) {
			case AF_INET:
				conf->listen_addr.sin_addr.s_addr =
				    $3.v4.s_addr;
				break;
			case AF_INET6:
				memcpy(&conf->listen6_addr.sin6_addr, &$3.v6,
				    sizeof(conf->listen6_addr.sin6_addr));
				break;
			default:
				yyerror("king bula does not like family %u",
				    $3.af);
				YYERROR;
			}
		}
		| FIBUPDATE yesno		{
			if ($2 == 0)
				conf->flags |= BGPD_FLAG_NO_FIB_UPDATE;
			else
				conf->flags &= ~BGPD_FLAG_NO_FIB_UPDATE;
		}
		| ROUTECOLL yesno	{
			if ($2 == 1)
				conf->flags |= BGPD_FLAG_NO_EVALUATE;
			else
				conf->flags &= ~BGPD_FLAG_NO_EVALUATE;
		}
		| LOG string		{
			if (!strcmp($2, "updates"))
				conf->log |= BGPD_LOG_UPDATES;
			else {
				free($2);
				YYERROR;
			}
			free($2);
		}
		| NETWORK prefix filter_set	{
			struct network	*n;

			if ($2.prefix.af != AF_INET) {
				yyerror("king bula sez: AF_INET only for now");
				YYERROR;
			}
			if ((n = calloc(1, sizeof(struct network))) == NULL)
				fatal("new_network");
			memcpy(&n->net.prefix, &$2.prefix,
			    sizeof(n->net.prefix));
			n->net.prefixlen = $2.len;
			memcpy(&n->net.attrset, &$3,
			    sizeof(n->net.attrset));

			TAILQ_INSERT_TAIL(netconf, n, network_l);
		}
		| DUMP TABLE STRING optnumber		{
			if (add_mrtconfig(MRT_TABLE_DUMP, $3, $4, NULL) == -1) {
				free($3);
				YYERROR;
			}
			free($3);
		}
		| mrtdump
		;

mrtdump		: DUMP STRING inout STRING optnumber	{
			int action;

			if (!strcmp($2, "all"))
				action = $3 ? MRT_ALL_IN : MRT_ALL_OUT;
			else if (!strcmp($2, "updates"))
				action = $3 ? MRT_UPDATE_IN : MRT_UPDATE_OUT;
			else {
				yyerror("unknown mrt msg dump type");
				free($2);
				free($4);
				YYERROR;
			}
			if (add_mrtconfig(action, $4, $5, curpeer) == -1) {
				free($2);
				free($4);
				YYERROR;
			}
			free($2);
			free($4);
		}
		;

inout		: IN		{ $$ = 1; }
		| OUT		{ $$ = 0; }
		;

address		: STRING		{
			u_int8_t	len;

			if (!host($1, &$$, &len)) {
				yyerror("could not parse address spec \"%s\"",
				     $1);
				free($1);
				YYERROR;
			}
			free($1);

			if (($$.af == AF_INET && len != 32) ||
			    ($$.af == AF_INET6 && len != 128)) {
				/* unreachable */
				yyerror("got prefixlen %u, expected %u",
				    len, $$.af == AF_INET ? 32 : 128);
				YYERROR;
			}
		}
		;

prefix		: STRING '/' number	{
			char	*s;

			if (asprintf(&s, "%s/%u", $1, $3) == -1)
				fatal(NULL);

			if (!host(s, &$$.prefix, &$$.len)) {
				yyerror("could not parse address \"%s/%u\"",
				     $1, $3);
				free(s);
				free($1);
				YYERROR;
			}
			free($1);
			free(s);
		}
		;

addrspec	: address	{
			memcpy(&$$.prefix, &$1, sizeof(struct bgpd_addr));
			if ($$.prefix.af == AF_INET)
				$$.len = 32;
			else
				$$.len = 128;
		}
		| prefix
		;

optnl		: '\n' optnl
		|
		;

nl		: '\n' optnl		/* one newline or more */
		;

optnumber	: /* empty */		{ $$ = 0; }
		| number
		;

neighbor	: {	curpeer = new_peer(); }
		    NEIGHBOR addrspec optnl '{' optnl {
			memcpy(&curpeer->conf.remote_addr, &$3.prefix,
			    sizeof(curpeer->conf.remote_addr));
			curpeer->conf.remote_masklen = $3.len;
			if (($3.prefix.af == AF_INET && $3.len != 32) ||
			    ($3.prefix.af == AF_INET6 && $3.len != 128))
				curpeer->conf.template = 1;
			if (get_id(curpeer)) {
				yyerror("get_id failed");
				YYERROR;
			}
		}
		    peeropts_l '}' {
			if (curpeer->conf.local_addr.af &&
			    curpeer->conf.local_addr.af !=
			    curpeer->conf.remote_addr.af) {
				yyerror("local-address and neighbor address "
				    "must be of the same address family");
				YYERROR;
			}

			curpeer->next = peer_l;
			peer_l = curpeer;
			curpeer = NULL;
		}
		;

group		: GROUP string optnl '{' optnl {
			curgroup = curpeer = new_group();
			if (strlcpy(curgroup->conf.group, $2,
			    sizeof(curgroup->conf.group)) >=
			    sizeof(curgroup->conf.group)) {
				yyerror("group name \"%s\" too long: max %u",
				    $2, sizeof(curgroup->conf.group) - 1);
				free($2);
				YYERROR;
			}
			free($2);
			if (get_id(curgroup)) {
				yyerror("get_id failed");
				YYERROR;
			}
		}
		    groupopts_l '}' {
			free(curgroup);
			curgroup = NULL;
		}
		;

groupopts_l	: groupopts_l groupoptsl
		| groupoptsl
		;

groupoptsl	: peeropts nl
		| neighbor nl
		| error nl
		;

peeropts_l	: peeropts_l peeroptsl
		| peeroptsl
		;

peeroptsl	: peeropts nl
		| error nl
		;

peeropts	: REMOTEAS asnumber	{
			curpeer->conf.remote_as = $2;
		}
		| DESCR string		{
			if (strlcpy(curpeer->conf.descr, $2,
			    sizeof(curpeer->conf.descr)) >=
			    sizeof(curpeer->conf.descr)) {
				yyerror("descr \"%s\" too long: max %u",
				    $2, sizeof(curpeer->conf.descr) - 1);
				free($2);
				YYERROR;
			}
			free($2);
		}
		| LOCALADDR address	{
			memcpy(&curpeer->conf.local_addr, &$2,
			    sizeof(curpeer->conf.local_addr));
		}
		| MULTIHOP number	{
			if ($2 < 2 || $2 > 255) {
				yyerror("invalid multihop distance %d", $2);
				YYERROR;
			}
			curpeer->conf.distance = $2;
		}
		| PASSIVE		{
			curpeer->conf.passive = 1;
		}
		| HOLDTIME number	{
			if ($2 < MIN_HOLDTIME) {
				yyerror("holdtime must be at least %u",
				    MIN_HOLDTIME);
				YYERROR;
			}
			curpeer->conf.holdtime = $2;
		}
		| HOLDTIME YMIN number	{
			if ($3 < MIN_HOLDTIME) {
				yyerror("holdtime min must be at least %u",
				    MIN_HOLDTIME);
				YYERROR;
			}
			curpeer->conf.min_holdtime = $3;
		}
		| ANNOUNCE STRING {
			if (!strcmp($2, "self"))
				curpeer->conf.announce_type = ANNOUNCE_SELF;
			else if (!strcmp($2, "none"))
				curpeer->conf.announce_type = ANNOUNCE_NONE;
			else if (!strcmp($2, "all"))
				curpeer->conf.announce_type = ANNOUNCE_ALL;
			else {
				free($2);
				YYERROR;
			}
			free($2);
		}
		| ENFORCE NEIGHBORAS yesno {
			if ($3)
				curpeer->conf.enforce_as = ENFORCE_AS_ON;
			else
				curpeer->conf.enforce_as = ENFORCE_AS_OFF;
		}
		| MAXPREFIX number {
			curpeer->conf.max_prefix = $2;
		}
		| TCP MD5SIG PASSWORD string {
			if (strlcpy(curpeer->conf.tcp_md5_key, $4,
			    sizeof(curpeer->conf.tcp_md5_key)) >=
			    sizeof(curpeer->conf.tcp_md5_key)) {
				yyerror("tcp md5sig password too long: max %u",
				    sizeof(curpeer->conf.tcp_md5_key) - 1);
				free($4);
				YYERROR;
			}
			free($4);
		}
		| TCP MD5SIG KEY string {
			unsigned	i;
			char		s[3];

			if (strlen($4) / 2 >=
			    sizeof(curpeer->conf.tcp_md5_key)) {
				yyerror("key too long");
				free($4);
				YYERROR;
			}

			if (strlen($4) % 2) {
				yyerror("key must be of even length");
				free($4);
				YYERROR;
			}

			for (i = 0; i < strlen($4) / 2; i++) {
				s[0] = $4[2*i];
				s[1] = $4[2*i + 1];
				s[2] = 0;
				if (!isxdigit(s[0]) || !isxdigit(s[1])) {
					yyerror("key must be specified in hex");
					free($4);
					YYERROR;
				}
				curpeer->conf.tcp_md5_key[i] =
				    strtoul(s, NULL, 16);
			}
			free($4);
		}
		| IPSEC ESP inout SPI number STRING STRING encspec {
			unsigned	i;
			char		s[3];
			u_int32_t	auth_alg;

			if (strlen($7) / 2 >
			    sizeof(curpeer->conf.ipsec.auth_key_in)) {
				yyerror("auth key too long");
				free($7);
				free($6);
				YYERROR;
			}

			if (strlen($7) % 2) {
				yyerror("auth key must be of even length");
				free($6);
				free($7);
				YYERROR;
			}

			for (i = 0; i < strlen($7) / 2; i++) {
				s[0] = $7[2*i];
				s[1] = $7[2*i + 1];
				s[2] = 0;
				if (!isxdigit(s[0]) || !isxdigit(s[1])) {
					yyerror("key must be specified in hex");
					free($7);
					YYERROR;
				}
				if ($3 == 1)
					curpeer->conf.ipsec.auth_key_in[i] =
					    strtoul(s, NULL, 16);
				else
					curpeer->conf.ipsec.auth_key_out[i] =
					    strtoul(s, NULL, 16);
			}
			free($7);

			if (!strcmp($6, "sha1"))
				auth_alg = SADB_AALG_SHA1HMAC;
			else if (!strcmp($6, "md5"))
				auth_alg = SADB_AALG_MD5HMAC;
			else {
				yyerror("unknown auth algorithm \"%s\"", $6);
				free($6);
				YYERROR;
			}
			free($6);

			if ($3 == 1) {
				curpeer->conf.ipsec.spi_in = $5;
				curpeer->conf.ipsec.auth_alg_in = auth_alg;
				curpeer->conf.ipsec.enc_alg_in = $8.enc_alg;
				memcpy(&curpeer->conf.ipsec.enc_key_in,
				    &$8.enc_key,
				    sizeof(curpeer->conf.ipsec.enc_key_in));
				curpeer->conf.ipsec.enc_keylen_in =
				    $8.enc_key_len;
				curpeer->conf.ipsec.auth_keylen_in =
				    strlen($7) / 2;
			} else {
				curpeer->conf.ipsec.spi_out = $5;
				curpeer->conf.ipsec.auth_alg_out = auth_alg;
				curpeer->conf.ipsec.enc_alg_out = $8.enc_alg;
				memcpy(&curpeer->conf.ipsec.enc_key_out,
				    &$8.enc_key,
				    sizeof(curpeer->conf.ipsec.enc_key_out));
				curpeer->conf.ipsec.enc_keylen_out =
				    $8.enc_key_len;
				curpeer->conf.ipsec.auth_keylen_out =
				    strlen($7) / 2;
			}
		}
		| ANNOUNCE CAPABILITIES yesno {
			curpeer->conf.capabilities = $3;
		}
		| SET filter_set_opt	{
			memcpy(&curpeer->conf.attrset, &$2,
			    sizeof(curpeer->conf.attrset));
		}
		| SET optnl "{" optnl filter_set_l optnl "}"	{
			memcpy(&curpeer->conf.attrset, &$5,
			    sizeof(curpeer->conf.attrset));
		}
		| mrtdump
		;

encspec		: /* nada */	{
			bzero(&$$, sizeof($$));
		}
		| STRING STRING {
			unsigned	i;
			char		s[3];

			bzero(&$$, sizeof($$));
			if (!strcmp($1, "3des") || !strcmp($1, "3des-cbc"))
				$$.enc_alg = SADB_EALG_3DESCBC;
			else if (!strcmp($1, "aes") ||
			    !strcmp($1, "aes-128-cbc"))
				$$.enc_alg = SADB_X_EALG_AES;
			else {
				yyerror("unknown enc algorithm \"%s\"", $1);
				free($1);
				free($2);
				YYERROR;
			}
			free($1);

			if (strlen($2) / 2 >
			    sizeof(curpeer->conf.ipsec.enc_key_in)) {
				yyerror("enc key too long");
				free($2);
				YYERROR;
			}

			if (strlen($2) % 2) {
				yyerror("key must be of even length");
				free($2);
				YYERROR;
			}

			for (i = 0; i < strlen($2) / 2; i++) {
				s[0] = $2[2*i];
				s[1] = $2[2*i + 1];
				s[2] = 0;
				if (!isxdigit(s[0]) || !isxdigit(s[1])) {
					yyerror("key must be specified in hex");
					free($2);
					YYERROR;
				}
				$$.enc_key[i] = strtoul(s, NULL, 16);
			}
			free($2);
			$$.enc_key_len = strlen($2) / 2;
		}
		;

filterrule	: action quick direction filter_peer filter_match filter_set
		{
			struct filter_rule	r;

			if ($5.prefix.addr.af && $5.prefix.addr.af != AF_INET) {
				yyerror("king bula sez: AF_INET only for now");
				YYERROR;
			}

			bzero(&r, sizeof(r));
			r.action = $1;
			r.quick = $2;
			r.dir = $3;

			if (expand_rule(&r, &$4, &$5, &$6) == -1)
				YYERROR;
		}
		;

action		: ALLOW		{ $$ = ACTION_ALLOW; }
		| DENY		{ $$ = ACTION_DENY; }
		| MATCH		{ $$ = ACTION_NONE; }
		;

quick		: /* empty */	{ $$ = 0; }
		| QUICK		{ $$ = 1; }
		;

direction	: FROM		{ $$ = DIR_IN; }
		| TO		{ $$ = DIR_OUT; }
		;

filter_peer	: ANY		{ $$.peerid = $$.groupid = 0; }
		| address	{
			struct peer *p;

			$$.groupid = $$.peerid = 0;
			for (p = peer_l; p != NULL; p = p->next)
				if (!memcmp(&p->conf.remote_addr,
				    &$1, sizeof(p->conf.remote_addr))) {
					$$.peerid = p->conf.id;
					break;
				}
			if ($$.peerid == 0) {
				yyerror("no such peer: %s", log_addr(&$1));
				YYERROR;
			}
		}
		| GROUP string	{
			struct peer *p;

			$$.peerid = 0;
			for (p = peer_l; p != NULL; p = p->next)
				if (!strcmp(p->conf.group, $2)) {
					$$.groupid = p->conf.groupid;
					break;
				}
			if ($$.groupid == 0) {
				yyerror("no such group: \"%s\"", $2);
				free($2);
				YYERROR;
			}
			free($2);
		}
		;

filter_match	: /* empty */			{ bzero(&$$, sizeof($$)); }
		| PREFIX prefix			{
			bzero(&$$, sizeof($$));
			memcpy(&$$.prefix.addr, &$2.prefix,
			    sizeof($$.prefix.addr));
			$$.prefix.len = $2.len;
		}
		| PREFIX prefix PREFIXLEN prefixlenop	{
			bzero(&$$, sizeof($$));
			memcpy(&$$.prefix.addr, &$2.prefix,
			    sizeof($$.prefix.addr));
			$$.prefix.len = $2.len;
			$$.prefixlen = $4.prefixlen;
			$$.prefixlen.af = $2.prefix.af;
			if ($$.prefixlen.af == AF_INET)
				if ($$.prefixlen.len_max > 32 ||
				    $$.prefixlen.len_min > 32) {
					yyerror("prefixlen must be <= 32");
					YYERROR;
				}
			if ($$.prefixlen.af == AF_INET6)
				if ($$.prefixlen.len_max > 128 ||
				    $$.prefixlen.len_min > 128) {
					yyerror("prefixlen must be <= 128");
					YYERROR;
				}
		}
		| PREFIXLEN prefixlenop		{
			bzero(&$$, sizeof($$));
			$$.prefixlen = $2.prefixlen;
			$$.prefixlen.af = AF_INET;
		}
		| filter_as asnumber		{
			bzero(&$$, sizeof($$));
			$$.as.as = $2;
			$$.as.type = $1;
		}
		| COMMUNITY STRING	{
			char	*p;
			int	 i;

			bzero(&$$, sizeof($$));
			if ((p = strchr($2, ':')) == NULL) {
				yyerror("Bad community syntax");
				YYERROR;
			}
			*p++ = 0;
			if ((i = getcommunity($2)) == COMMUNITY_ERROR)
				YYERROR;
			if (i == 0 || i == USHRT_MAX) {
				yyerror("Bad community as number");
				YYERROR;
			}
			$$.community.as = i;
			if ((i = getcommunity(p)) == COMMUNITY_ERROR)
				YYERROR;
			$$.community.type = i;
		}
		;

prefixlenop	: unaryop number		{
			bzero(&$$, sizeof($$));
			if ($2 > 128) {
				yyerror("prefixlen must be < 128");
				YYERROR;
			}
			$$.prefixlen.op = $1;
			$$.prefixlen.len_min = $2;
		}
		| number binaryop number	{
			bzero(&$$, sizeof($$));
			if ($1 > 128 || $3 > 128) {
				yyerror("prefixlen must be < 128");
				YYERROR;
			}
			if ($1 >= $3) {
				yyerror("start prefixlen is bigger that end");
				YYERROR;
			}
			$$.prefixlen.op = $2;
			$$.prefixlen.len_min = $1;
			$$.prefixlen.len_max = $3;
		}
		;

filter_as	: AS		{ $$ = AS_ALL; }
		| SOURCEAS	{ $$ = AS_SOURCE; }
		| TRANSITAS	{ $$ = AS_TRANSIT; }
		;

filter_set	: /* empty */					{
			bzero(&$$, sizeof($$));
		}
		| SET filter_set_opt				{ $$ = $2; }
		| SET optnl "{" optnl filter_set_l optnl "}"	{ $$ = $5; }
		;

filter_set_l	: filter_set_l comma filter_set_opt	{
			$$ = $1;
			if ($$.flags & $3.flags) {
				yyerror("redefining set shitz is not fluffy");
				YYERROR;
			}
			$$.flags |= $3.flags;
			if ($3.flags & SET_LOCALPREF)
				$$.localpref = $3.localpref;
			if ($3.flags & SET_MED)
				$$.med = $3.med;
			if ($3.flags & SET_NEXTHOP)
				memcpy(&$$.nexthop, &$3.nexthop,
				    sizeof($$.nexthop));
			if ($3.flags & SET_PREPEND)
				$$.prepend = $3.prepend;
		}
		| filter_set_opt
		;

filter_set_opt	: LOCALPREF number		{
			$$.flags = SET_LOCALPREF;
			$$.localpref = $2;
		}
		| MED number			{
			$$.flags = SET_MED;
			$$.med = $2;
		}
		| NEXTHOP address		{
			if ($2.af == AF_INET) {
				$$.flags = SET_NEXTHOP;
				$$.nexthop.s_addr = $2.v4.s_addr;
			} else {
				yyerror("king bula sez: AF_INET only for now");
				YYERROR;
			}
		}
		| PREPEND number		{
			$$.flags = SET_PREPEND;
			$$.prepend = $2;
		}
		;

comma		: ","
		| /* empty */
		;

unaryop		: '='		{ $$ = OP_EQ; }
		| '!' '='	{ $$ = OP_NE; }
		| '<' '='	{ $$ = OP_LE; }
		| '<'		{ $$ = OP_LT; }
		| '>' '='	{ $$ = OP_GE; }
		| '>'		{ $$ = OP_GT; }
		;

binaryop	: '-'		{ $$ = OP_RANGE; }
		| '>' '<'	{ $$ = OP_XRANGE; }
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
	char		*nfmt;

	errors = 1;
	va_start(ap, fmt);
	if (asprintf(&nfmt, "%s:%d: %s", infile, yylval.lineno, fmt) == -1)
		fatalx("yyerror asprintf");
	vlog(LOG_CRIT, nfmt, ap);
	va_end(ap);
	free(nfmt);
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
		{ "AS",			AS},
		{ "ah",			AH},
		{ "allow",		ALLOW},
		{ "announce",		ANNOUNCE},
		{ "any",		ANY},
		{ "capabilities",	CAPABILITIES},
		{ "community",		COMMUNITY},
		{ "deny",		DENY},
		{ "descr",		DESCR},
		{ "dump",		DUMP},
		{ "enforce",		ENFORCE},
		{ "esp",		ESP},
		{ "fib-update",		FIBUPDATE},
		{ "from",		FROM},
		{ "group",		GROUP},
		{ "holdtime",		HOLDTIME},
		{ "in",			IN},
		{ "ipsec",		IPSEC},
		{ "key",		KEY},
		{ "listen",		LISTEN},
		{ "local-address",	LOCALADDR},
		{ "localpref",		LOCALPREF},
		{ "log",		LOG},
		{ "match",		MATCH},
		{ "max-prefix",		MAXPREFIX},
		{ "md5sig",		MD5SIG},
		{ "med",		MED},
		{ "min",		YMIN},
		{ "multihop",		MULTIHOP},
		{ "neighbor",		NEIGHBOR},
		{ "neighbor-as",	NEIGHBORAS},
		{ "network",		NETWORK},
		{ "nexthop",		NEXTHOP},
		{ "on",			ON},
		{ "out",		OUT},
		{ "passive",		PASSIVE},
		{ "password",		PASSWORD},
		{ "prefix",		PREFIX},
		{ "prefixlen",		PREFIXLEN},
		{ "prepend-self",	PREPEND},
		{ "quick",		QUICK},
		{ "remote-as",		REMOTEAS},
		{ "route-collector",	ROUTECOLL},
		{ "router-id",		ROUTERID},
		{ "set",		SET},
		{ "source-AS",		SOURCEAS},
		{ "spi",		SPI},
		{ "table",		TABLE},
		{ "tcp",		TCP},
		{ "to",			TO},
		{ "transit-AS",		TRANSITAS}
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
			yyerror("macro \"%s\" not defined", buf);
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
			fatal("yylex: strdup");
		return (STRING);
	}

#define allowed_in_string(x) \
	(isalnum(x) || (ispunct(x) && x != '(' && x != ')' && \
	x != '{' && x != '}' && x != '<' && x != '>' && \
	x != '!' && x != '=' && x != '/' && x != '#' && \
	x != ','))

	if (isalnum(c) || c == ':' || c == '_' || c == '*') {
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
				fatal("yylex: strdup");
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
parse_config(char *filename, struct bgpd_config *xconf,
    struct mrt_head *xmconf, struct peer **xpeers, struct network_head *nc,
    struct filter_head *xfilter_l)
{
	struct sym		*sym, *next;
	struct peer		*p, *pnext;

	if ((conf = calloc(1, sizeof(struct bgpd_config))) == NULL)
		fatal(NULL);
	if ((mrtconf = calloc(1, sizeof(struct mrt_head))) == NULL)
		fatal(NULL);
	LIST_INIT(mrtconf);
	netconf = nc;
	TAILQ_INIT(netconf);

	peer_l = NULL;
	peer_l_old = *xpeers;
	curpeer = NULL;
	curgroup = NULL;
	lineno = 1;
	errors = 0;
	id = 1;
	filter_l = xfilter_l;
	TAILQ_INIT(filter_l);

	conf->listen_addr.sin_len = sizeof(conf->listen_addr);
	conf->listen_addr.sin_family = AF_INET;
	conf->listen_addr.sin_addr.s_addr = INADDR_ANY;
	conf->listen_addr.sin_port = htons(BGP_PORT);

	bzero(&conf->listen6_addr, sizeof(conf->listen6_addr));
	conf->listen6_addr.sin6_len = sizeof(conf->listen6_addr);
	conf->listen6_addr.sin6_family = AF_INET6;
	conf->listen6_addr.sin6_port = htons(BGP_PORT);

	if ((fin = fopen(filename, "r")) == NULL) {
		warn("%s", filename);
		free(conf);
		free(mrtconf);
		return (-1);
	}
	infile = filename;

	if (check_file_secrecy(fileno(fin), filename)) {
		free(conf);
		free(mrtconf);
		return (-1);
	}

	yyparse();

	fclose(fin);

	/* Free macros and check which have not been used. */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entries);
		if ((conf->opts & BGPD_OPT_VERBOSE2) && !sym->used)
			fprintf(stderr, "warning: macro \"%s\" not "
			    "used\n", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entries);
			free(sym);
		}
	}

	errors += merge_config(xconf, conf, peer_l);
	errors += mrt_mergeconfig(xmconf, mrtconf);
	*xpeers = peer_l;

	for (p = peer_l_old; p != NULL; p = pnext) {
		pnext = p->next;
		free(p);
	}

	free(conf);
	free(mrtconf);

	return (errors ? -1 : 0);
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
		fatal("cmdline_symset: malloc");

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

int
getcommunity(char *s)
{
	u_long	ulval;

	if (strcmp(s, "*") == 0)
		return (COMMUNITY_ANY);
	if (atoul(s, &ulval) == -1) {
		yyerror("\"%s\" is not a number", s);
		return (COMMUNITY_ERROR);
	}
	if (ulval > USHRT_MAX) {
		yyerror("Community too big: max %u", USHRT_MAX);
		return (COMMUNITY_ERROR);
	}
	return (ulval);
}

struct peer *
alloc_peer(void)
{
	struct peer	*p;

	if ((p = calloc(1, sizeof(struct peer))) == NULL)
		fatal("new_peer");

	/* some sane defaults */
	p->state = STATE_NONE;
	p->next = NULL;
	p->conf.distance = 1;
	p->conf.announce_type = ANNOUNCE_UNDEF;
	p->conf.capabilities = 1;

	return (p);
}

struct peer *
new_peer(void)
{
	struct peer	*p;

	p = alloc_peer();

	if (curgroup != NULL) {
		memcpy(p, curgroup, sizeof(struct peer));
		if (strlcpy(p->conf.group, curgroup->conf.group,
		    sizeof(p->conf.group)) >= sizeof(p->conf.group))
			fatalx("new_peer group strlcpy");
		if (strlcpy(p->conf.descr, curgroup->conf.descr,
		    sizeof(p->conf.descr)) >= sizeof(p->conf.descr))
			fatalx("new_peer descr strlcpy");
		p->conf.groupid = curgroup->conf.id;
	}
	p->next = NULL;

	return (p);
}

struct peer *
new_group(void)
{
	return (alloc_peer());
}

int
add_mrtconfig(enum mrt_type type, char *name, time_t timeout, struct peer *p)
{
	struct mrt	*m, *n;

	LIST_FOREACH(m, mrtconf, list) {
		if (p == NULL) {
			if (m->conf.peer_id != 0 || m->conf.group_id != 0)
				continue;
		} else {
			if (m->conf.peer_id != p->conf.id ||
			    m->conf.group_id != p->conf.groupid)
				continue;
		}
		if (m->conf.type == type) {
			yyerror("only one mrtdump per type allowed.");
			return (-1);
		}
	}

	if ((n = calloc(1, sizeof(struct mrt))) == NULL)
		fatal("add_mrtconfig");

	n->conf.type = type;
	n->msgbuf.sock = -1;
	if (strlcpy(n->name, name, sizeof(n->name)) >= sizeof(n->name)) {
		yyerror("filename \"%s\" too long: max %u",
		    name, sizeof(n->name) - 1);
		free(n);
		return (-1);
	}
	n->ReopenTimerInterval = timeout;
	if (p != NULL) {
		if (curgroup == p) {
			n->conf.peer_id = 0;
			n->conf.group_id = p->conf.id;
		} else {
			n->conf.peer_id = p->conf.id;
			n->conf.group_id = 0;
		}
	}

	LIST_INSERT_HEAD(mrtconf, n, list);

	return (0);
}

int
get_id(struct peer *newpeer)
{
	struct peer	*p;

	if (newpeer->conf.remote_addr.af)
		for (p = peer_l_old; p != NULL; p = p->next)
			if (!memcmp(&p->conf.remote_addr,
			    &newpeer->conf.remote_addr,
			    sizeof(p->conf.remote_addr))) {
				newpeer->conf.id = p->conf.id;
				return (0);
			}

	/* new one */
	for (; id < UINT_MAX / 2; id++) {
		for (p = peer_l_old; p != NULL && p->conf.id != id; p = p->next)
			;	/* nothing */
		if (p == NULL) {	/* we found a free id */
			newpeer->conf.id = id++;
			return (0);
		}
	}

	return (-1);
}

int
expand_rule(struct filter_rule *rule, struct filter_peers *peer,
    struct filter_match *match, struct filter_set *set)
{
	struct filter_rule	*r;

	if ((r = calloc(1, sizeof(struct filter_rule))) == NULL) {
		log_warn("expand_rule");
		return (-1);
	}

	memcpy(r, rule, sizeof(struct filter_rule));
	memcpy(&r->peer, peer, sizeof(struct filter_peers));
	memcpy(&r->match, match, sizeof(struct filter_match));
	memcpy(&r->set, set, sizeof(struct filter_set));

	TAILQ_INSERT_TAIL(filter_l, r, entries);

	return (0);
}

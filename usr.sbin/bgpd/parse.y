/*	$OpenBSD: parse.y,v 1.258 2010/09/02 14:03:21 sobrado Exp $ */

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
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netmpls/mpls.h>

#include <ctype.h>
#include <err.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include "bgpd.h"
#include "mrt.h"
#include "session.h"
#include "rde.h"

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

static struct bgpd_config	*conf;
static struct mrt_head		*mrtconf;
static struct network_head	*netconf, *gnetconf;
static struct peer		*peer_l, *peer_l_old;
static struct peer		*curpeer;
static struct peer		*curgroup;
static struct rdomain		*currdom;
static struct rdomain_head	*rdom_l;
static struct filter_head	*filter_l;
static struct filter_head	*peerfilter_l;
static struct filter_head	*groupfilter_l;
static struct filter_rule	*curpeer_filter[2];
static struct filter_rule	*curgroup_filter[2];
static struct listen_addrs	*listen_addrs;
static u_int32_t		 id;

struct filter_peers_l {
	struct filter_peers_l	*next;
	struct filter_peers	 p;
};

struct filter_prefix_l {
	struct filter_prefix_l	*next;
	struct filter_prefix	 p;
};

struct filter_as_l {
	struct filter_as_l	*next;
	struct filter_as	 a;
};

struct filter_match_l {
	struct filter_match	 m;
	struct filter_prefix_l	*prefix_l;
	struct filter_as_l	*as_l;
	u_int8_t		 aid;
} fmopts;

struct peer	*alloc_peer(void);
struct peer	*new_peer(void);
struct peer	*new_group(void);
int		 add_mrtconfig(enum mrt_type, char *, time_t, struct peer *,
		    char *);
int		 add_rib(char *, u_int, u_int16_t);
struct rde_rib	*find_rib(char *);
int		 get_id(struct peer *);
int		 expand_rule(struct filter_rule *, struct filter_peers_l *,
		    struct filter_match_l *, struct filter_set_head *);
int		 str2key(char *, char *, size_t);
int		 neighbor_consistent(struct peer *);
int		 merge_filterset(struct filter_set_head *, struct filter_set *);
void		 copy_filterset(struct filter_set_head *,
		    struct filter_set_head *);
void		 move_filterset(struct filter_set_head *,
		    struct filter_set_head *);
struct filter_rule	*get_rule(enum action_types);

int		 getcommunity(char *);
int		 parsecommunity(struct filter_community *, char *);
int		 parsesubtype(char *);
int		 parseextvalue(char *, u_int32_t *);
int		 parseextcommunity(struct filter_extcommunity *, char *,
		    char *);

typedef struct {
	union {
		int64_t			 number;
		char			*string;
		struct bgpd_addr	 addr;
		u_int8_t		 u8;
		struct filter_peers_l	*filter_peers;
		struct filter_match_l	 filter_match;
		struct filter_prefix_l	*filter_prefix;
		struct filter_as_l	*filter_as;
		struct filter_prefixlen	 prefixlen;
		struct filter_set	*filter_set;
		struct filter_set_head	*filter_set_head;
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

%token	AS ROUTERID HOLDTIME YMIN LISTEN ON FIBUPDATE RTABLE
%token	RDOMAIN RD EXPORTTRGT IMPORTTRGT
%token	RDE RIB EVALUATE IGNORE COMPARE
%token	GROUP NEIGHBOR NETWORK
%token	REMOTEAS DESCR LOCALADDR MULTIHOP PASSIVE MAXPREFIX RESTART
%token	ANNOUNCE CAPABILITIES REFRESH AS4BYTE CONNECTRETRY
%token	DEMOTE ENFORCE NEIGHBORAS REFLECTOR DEPEND DOWN SOFTRECONFIG
%token	DUMP IN OUT SOCKET RESTRICTED
%token	LOG ROUTECOLL TRANSPARENT
%token	TCP MD5SIG PASSWORD KEY TTLSECURITY
%token	ALLOW DENY MATCH
%token	QUICK
%token	FROM TO ANY
%token	CONNECTED STATIC
%token	COMMUNITY EXTCOMMUNITY
%token	PREFIX PREFIXLEN SOURCEAS TRANSITAS PEERAS DELETE MAXASLEN MAXASSEQ
%token	SET LOCALPREF MED METRIC NEXTHOP REJECT BLACKHOLE NOMODIFY SELF
%token	PREPEND_SELF PREPEND_PEER PFTABLE WEIGHT RTLABEL ORIGIN
%token	ERROR INCLUDE
%token	IPSEC ESP AH SPI IKE
%token	IPV4 IPV6
%token	QUALIFY VIA
%token	NE LE GE XRANGE
%token	<v.string>		STRING
%token	<v.number>		NUMBER
%type	<v.number>		asnumber as4number optnumber
%type	<v.number>		espah family restart origincode nettype
%type	<v.number>		yesno inout restricted
%type	<v.string>		string filter_rib
%type	<v.addr>		address
%type	<v.prefix>		prefix addrspec
%type	<v.u8>			action quick direction delete
%type	<v.filter_peers>	filter_peer filter_peer_l filter_peer_h
%type	<v.filter_match>	filter_match filter_elm filter_match_h
%type	<v.filter_as>		filter_as filter_as_l filter_as_h
%type	<v.filter_as>		filter_as_t filter_as_t_l filter_as_l_h
%type	<v.prefixlen>		prefixlenop
%type	<v.filter_set>		filter_set_opt
%type	<v.filter_set_head>	filter_set filter_set_l
%type	<v.filter_prefix>	filter_prefix filter_prefix_l
%type	<v.filter_prefix>	filter_prefix_h filter_prefix_m
%type	<v.u8>			unaryop binaryop filter_as_type
%type	<v.encspec>		encspec
%%

grammar		: /* empty */
		| grammar '\n'
		| grammar include '\n'
		| grammar conf_main '\n'
		| grammar varset '\n'
		| grammar rdomain '\n'
		| grammar neighbor '\n'
		| grammar group '\n'
		| grammar filterrule '\n'
		| grammar error '\n'		{ file->errors++; }
		;

asnumber	: NUMBER			{
			/*
			 * Accroding to iana 65535 and 4294967295 are reserved
			 * but enforcing this is not duty of the parser.
			 */
			if ($1 < 0 || $1 > UINT_MAX) {
				yyerror("AS too big: max %u", UINT_MAX);
				YYERROR;
			}
		}

as4number	: STRING			{
			const char	*errstr;
			char		*dot;
			u_int32_t	 uvalh = 0, uval;

			if ((dot = strchr($1,'.')) != NULL) {
				*dot++ = '\0';
				uvalh = strtonum($1, 0, USHRT_MAX, &errstr);
				if (errstr) {
					yyerror("number %s is %s", $1, errstr);
					free($1);
					YYERROR;
				}
				uval = strtonum(dot, 0, USHRT_MAX, &errstr);
				if (errstr) {
					yyerror("number %s is %s", dot, errstr);
					free($1);
					YYERROR;
				}
				free($1);
			} else {
				yyerror("AS %s is bad", $1);
				free($1);
				YYERROR;
			}
			if (uvalh == 0 && uval == AS_TRANS) {
				yyerror("AS %u is reserved and may not be used",
				    AS_TRANS);
				YYERROR;
			}
			$$ = uval | (uvalh << 16);
		}
		| asnumber {
			if ($1 == AS_TRANS) {
				yyerror("AS %u is reserved and may not be used",
				    AS_TRANS);
				YYERROR;
			}
			$$ = $1;
		}
		;

string		: string STRING			{
			if (asprintf(&$$, "%s %s", $1, $2) == -1)
				fatal("string: asprintf");
			free($1);
			free($2);
		}
		| STRING
		;

yesno		:  STRING			{
			if (!strcmp($1, "yes"))
				$$ = 1;
			else if (!strcmp($1, "no"))
				$$ = 0;
			else {
				yyerror("syntax error, "
				    "either yes or no expected");
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

conf_main	: AS as4number		{
			conf->as = $2;
			if ($2 > USHRT_MAX)
				conf->short_as = AS_TRANS;
			else
				conf->short_as = $2;
		}
		| AS as4number asnumber {
			conf->as = $2;
			conf->short_as = $3;
		}
		| ROUTERID address		{
			if ($2.aid != AID_INET) {
				yyerror("router-id must be an IPv4 address");
				YYERROR;
			}
			conf->bgpid = $2.v4.s_addr;
		}
		| HOLDTIME NUMBER	{
			if ($2 < MIN_HOLDTIME || $2 > USHRT_MAX) {
				yyerror("holdtime must be between %u and %u",
				    MIN_HOLDTIME, USHRT_MAX);
				YYERROR;
			}
			conf->holdtime = $2;
		}
		| HOLDTIME YMIN NUMBER	{
			if ($3 < MIN_HOLDTIME || $3 > USHRT_MAX) {
				yyerror("holdtime must be between %u and %u",
				    MIN_HOLDTIME, USHRT_MAX);
				YYERROR;
			}
			conf->min_holdtime = $3;
		}
		| LISTEN ON address	{
			struct listen_addr	*la;

			if ((la = calloc(1, sizeof(struct listen_addr))) ==
			    NULL)
				fatal("parse conf_main listen on calloc");

			la->fd = -1;
			memcpy(&la->sa, addr2sa(&$3, BGP_PORT), sizeof(la->sa));
			TAILQ_INSERT_TAIL(listen_addrs, la, entry);
		}
		| FIBUPDATE yesno		{
			struct rde_rib *rr;
			rr = find_rib("Loc-RIB");
			if (rr == NULL)
				fatalx("RTABLE can not find the main RIB!");

			if ($2 == 0)
				rr->flags |= F_RIB_NOFIBSYNC;
			else
				rr->flags &= ~F_RIB_NOFIBSYNC;
		}
		| ROUTECOLL yesno	{
			if ($2 == 1)
				conf->flags |= BGPD_FLAG_NO_EVALUATE;
			else
				conf->flags &= ~BGPD_FLAG_NO_EVALUATE;
		}
		| RDE RIB STRING {
			if (add_rib($3, 0, F_RIB_NOFIB)) {
				free($3);
				YYERROR;
			}
			free($3);
		}
		| RDE RIB STRING yesno EVALUATE {
			if ($4) {
				free($3);
				yyerror("bad rde rib definition");
				YYERROR;
			}
			if (add_rib($3, 0, F_RIB_NOFIB | F_RIB_NOEVALUATE)) {
				free($3);
				YYERROR;
			}
			free($3);
		}
		| RDE RIB STRING RTABLE NUMBER {
			if (add_rib($3, $5, 0)) {
				free($3);
				YYERROR;
			}
			free($3);
		}
		| RDE RIB STRING RTABLE NUMBER FIBUPDATE yesno {
			int	flags = 0;
			if ($7 == 0)
				flags = F_RIB_NOFIBSYNC;
			if (add_rib($3, $5, flags)) {
				free($3);
				YYERROR;
			}
			free($3);
		}
		| TRANSPARENT yesno	{
			if ($2 == 1)
				conf->flags |= BGPD_FLAG_DECISION_TRANS_AS;
			else
				conf->flags &= ~BGPD_FLAG_DECISION_TRANS_AS;
		}
		| LOG STRING		{
			if (!strcmp($2, "updates"))
				conf->log |= BGPD_LOG_UPDATES;
			else {
				free($2);
				YYERROR;
			}
			free($2);
		}
		| network
		| DUMP STRING STRING optnumber		{
			int action;

			if ($4 < 0 || $4 > UINT_MAX) {
				yyerror("bad timeout");
				free($2);
				free($3);
				YYERROR;
			}
			if (!strcmp($2, "table"))
				action = MRT_TABLE_DUMP;
			else if (!strcmp($2, "table-mp"))
				action = MRT_TABLE_DUMP_MP;
			else {
				yyerror("unknown mrt dump type");
				free($2);
				free($3);
				YYERROR;
			}
			free($2);
			if (add_mrtconfig(action, $3, $4, NULL, NULL) == -1) {
				free($3);
				YYERROR;
			}
			free($3);
		}
		| DUMP RIB STRING STRING STRING optnumber		{
			int action;

			if ($6 < 0 || $6 > UINT_MAX) {
				yyerror("bad timeout");
				free($3);
				free($4);
				free($5);
				YYERROR;
			}
			if (!strcmp($4, "table"))
				action = MRT_TABLE_DUMP;
			else if (!strcmp($4, "table-mp"))
				action = MRT_TABLE_DUMP_MP;
			else {
				yyerror("unknown mrt dump type");
				free($3);
				free($4);
				free($5);
				YYERROR;
			}
			free($4);
			if (add_mrtconfig(action, $5, $6, NULL, $3) == -1) {
				free($3);
				free($5);
				YYERROR;
			}
			free($3);
			free($5);
		}
		| mrtdump
		| RDE STRING EVALUATE		{
			if (!strcmp($2, "route-age"))
				conf->flags |= BGPD_FLAG_DECISION_ROUTEAGE;
			else {
				yyerror("unknown route decision type");
				free($2);
				YYERROR;
			}
			free($2);
		}
		| RDE STRING IGNORE		{
			if (!strcmp($2, "route-age"))
				conf->flags &= ~BGPD_FLAG_DECISION_ROUTEAGE;
			else {
				yyerror("unknown route decision type");
				free($2);
				YYERROR;
			}
			free($2);
		}
		| RDE MED COMPARE STRING	{
			if (!strcmp($4, "always"))
				conf->flags |= BGPD_FLAG_DECISION_MED_ALWAYS;
			else if (!strcmp($4, "strict"))
				conf->flags &= ~BGPD_FLAG_DECISION_MED_ALWAYS;
			else {
				yyerror("rde med compare: "
				    "unknown setting \"%s\"", $4);
				free($4);
				YYERROR;
			}
			free($4);
		}
		| NEXTHOP QUALIFY VIA STRING	{
			if (!strcmp($4, "bgp"))
				conf->flags |= BGPD_FLAG_NEXTHOP_BGP;
			else if (!strcmp($4, "default"))
				conf->flags |= BGPD_FLAG_NEXTHOP_DEFAULT;
			else {
				yyerror("nexthop depend on: "
				    "unknown setting \"%s\"", $4);
				free($4);
				YYERROR;
			}
			free($4);
		}
		| RTABLE NUMBER {
			struct rde_rib *rr;
			if (ktable_exists($2, NULL) != 1) {
				yyerror("rtable id %lld does not exist", $2);
				YYERROR;
			}
			rr = find_rib("Loc-RIB");
			if (rr == NULL)
				fatalx("RTABLE can not find the main RIB!");
			rr->rtableid = $2;
		}
		| CONNECTRETRY NUMBER {
			if ($2 > USHRT_MAX || $2 < 1) {
				yyerror("invalid connect-retry");
				YYERROR;
			}
			conf->connectretry = $2;
		}
		| SOCKET STRING	restricted {
			if ($3) {
				free(conf->rcsock);
				conf->rcsock = $2;
			} else {
				free(conf->csock);
				conf->csock = $2;
			}
		}
		;

mrtdump		: DUMP STRING inout STRING optnumber	{
			int action;

			if ($5 < 0 || $5 > UINT_MAX) {
				yyerror("bad timeout");
				free($2);
				free($4);
				YYERROR;
			}
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
			if (add_mrtconfig(action, $4, $5, curpeer, NULL) ==
			    -1) {
				free($2);
				free($4);
				YYERROR;
			}
			free($2);
			free($4);
		}
		;

network		: NETWORK prefix filter_set	{
			struct network	*n;

			if ((n = calloc(1, sizeof(struct network))) == NULL)
				fatal("new_network");
			memcpy(&n->net.prefix, &$2.prefix,
			    sizeof(n->net.prefix));
			n->net.prefixlen = $2.len;
			move_filterset($3, &n->net.attrset);
			free($3);

			TAILQ_INSERT_TAIL(netconf, n, entry);
		}
		| NETWORK family nettype filter_set	{
			struct network	*n;

			if ((n = calloc(1, sizeof(struct network))) == NULL)
				fatal("new_network");
			if (afi2aid($2, SAFI_UNICAST, &n->net.prefix.aid) ==
			    -1) {
				yyerror("unknown family");
				filterset_free($4);
				free($4);
				YYERROR;
			}
			n->net.type = $3 ? NETWORK_STATIC : NETWORK_CONNECTED;
			move_filterset($4, &n->net.attrset);
			free($4);

			TAILQ_INSERT_TAIL(netconf, n, entry);
		}
		;

inout		: IN		{ $$ = 1; }
		| OUT		{ $$ = 0; }
		;

restricted	: RESTRICTED	{ $$ = 1; }
		| /* nothing */	{ $$ = 0; }
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

			if (($$.aid == AID_INET && len != 32) ||
			    ($$.aid == AID_INET6 && len != 128)) {
				/* unreachable */
				yyerror("got prefixlen %u, expected %u",
				    len, $$.aid == AID_INET ? 32 : 128);
				YYERROR;
			}
		}
		;

prefix		: STRING '/' NUMBER	{
			char	*s;

			if ($3 < 0 || $3 > 128) {
				yyerror("bad prefixlen %lld", $3);
				free($1);
				YYERROR;
			}
			if (asprintf(&s, "%s/%lld", $1, $3) == -1)
				fatal(NULL);
			free($1);

			if (!host(s, &$$.prefix, &$$.len)) {
				yyerror("could not parse address \"%s\"", s);
				free(s);
				YYERROR;
			}
			free(s);
		}
		| NUMBER '/' NUMBER	{
			char	*s;

			/* does not match IPv6 */
			if ($1 < 0 || $1 > 255 || $3 < 0 || $3 > 32) {
				yyerror("bad prefix %lld/%lld", $1, $3);
				YYERROR;
			}
			if (asprintf(&s, "%lld/%lld", $1, $3) == -1)
				fatal(NULL);

			if (!host(s, &$$.prefix, &$$.len)) {
				yyerror("could not parse address \"%s\"", s);
				free(s);
				YYERROR;
			}
			free(s);
		}
		;

addrspec	: address	{
			memcpy(&$$.prefix, &$1, sizeof(struct bgpd_addr));
			if ($$.prefix.aid == AID_INET)
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
		| NUMBER
		;

rdomain		: RDOMAIN NUMBER optnl '{' optnl	{
			if (ktable_exists($2, NULL) != 1) {
				yyerror("rdomain %lld does not exist", $2);
				YYERROR;
			}
			if (!(currdom = calloc(1, sizeof(struct rdomain))))
				fatal(NULL);
			currdom->rtableid = $2;
			TAILQ_INIT(&currdom->import);
			TAILQ_INIT(&currdom->export);
			TAILQ_INIT(&currdom->net_l);
			netconf = &currdom->net_l;
		}
		    rdomainopts_l '}' {
			/* insert into list */
			SIMPLEQ_INSERT_TAIL(rdom_l, currdom, entry);
			currdom = NULL;
			netconf = gnetconf;
		}

rdomainopts_l	: rdomainopts_l rdomainoptsl
		| rdomainoptsl
		;

rdomainoptsl	: rdomainopts nl
		;

rdomainopts	: RD STRING {
			struct filter_extcommunity	ext;
			u_int64_t			rd;

			if (parseextcommunity(&ext, "rt", $2) == -1) {
				free($2);
				YYERROR;
			}
			free($2);
			/*
			 * RD is almost encode like an ext-community,
			 * but only almost so convert here.
			 */
			if (community_ext_conv(&ext, 0, &rd)) {
				yyerror("bad encoding of rd");
				YYERROR;
			}
			rd = betoh64(rd) & 0xffffffffffffULL;
			switch (ext.type) {
			case EXT_COMMUNITY_TWO_AS:
				rd |= (0ULL << 48);
				break;
			case EXT_COMMUNITY_IPV4:
				rd |= (1ULL << 48);
				break;
			case EXT_COMMUNITY_FOUR_AS:
				rd |= (2ULL << 48);
				break;
			default:
				yyerror("bad encoding of rd");
				YYERROR;
			}
			currdom->rd = htobe64(rd);
		}
		| EXPORTTRGT STRING STRING	{
			struct filter_set	*set;

			if ((set = calloc(1, sizeof(struct filter_set))) ==
			    NULL)
				fatal(NULL);
			set->type = ACTION_SET_EXT_COMMUNITY;
			if (parseextcommunity(&set->action.ext_community,
			    $2, $3) == -1) {
				free($3);
				free($2);
				free(set);
				YYERROR;
			}
			free($3);
			free($2);
			TAILQ_INSERT_TAIL(&currdom->export, set, entry);
		}
		| IMPORTTRGT STRING STRING	{
			struct filter_set	*set;

			if ((set = calloc(1, sizeof(struct filter_set))) ==
			    NULL)
				fatal(NULL);
			set->type = ACTION_SET_EXT_COMMUNITY;
			if (parseextcommunity(&set->action.ext_community,
			    $2, $3) == -1) {
				free($3);
				free($2);
				free(set);
				YYERROR;
			}
			free($3);
			free($2);
			TAILQ_INSERT_TAIL(&currdom->import, set, entry);
		}
		| DESCR string		{
			if (strlcpy(currdom->descr, $2,
			    sizeof(currdom->descr)) >=
			    sizeof(currdom->descr)) {
				yyerror("descr \"%s\" too long: max %u",
				    $2, sizeof(currdom->descr) - 1);
				free($2);
				YYERROR;
			}
			free($2);
		}
		| FIBUPDATE yesno		{
			if ($2 == 0)
				currdom->flags |= F_RIB_NOFIBSYNC;
			else
				currdom->flags &= ~F_RIB_NOFIBSYNC;
		}
		| network
		| DEPEND ON STRING	{
			/* XXX this is a hack */
			if (if_nametoindex($3) == 0) {
				yyerror("interface %s does not exist", $3);
				free($3);
				YYERROR;
			}
			strlcpy(currdom->ifmpe, $3, IFNAMSIZ);
			free($3);
			if (get_mpe_label(currdom)) {
				yyerror("failed to get mpls label from %s",
				    currdom->ifmpe);
				YYERROR;
			}
		}
		;

neighbor	: {	curpeer = new_peer(); }
		    NEIGHBOR addrspec {
			memcpy(&curpeer->conf.remote_addr, &$3.prefix,
			    sizeof(curpeer->conf.remote_addr));
			curpeer->conf.remote_masklen = $3.len;
			if (($3.prefix.aid == AID_INET && $3.len != 32) ||
			    ($3.prefix.aid == AID_INET6 && $3.len != 128))
				curpeer->conf.template = 1;
			if (curpeer->conf.capabilities.mp[
			    curpeer->conf.remote_addr.aid] == -1)
				curpeer->conf.capabilities.mp[
				    curpeer->conf.remote_addr.aid] = 1;
			if (get_id(curpeer)) {
				yyerror("get_id failed");
				YYERROR;
			}
		}
		    peeropts_h {
			if (curpeer_filter[0] != NULL)
				TAILQ_INSERT_TAIL(peerfilter_l,
				    curpeer_filter[0], entry);
			if (curpeer_filter[1] != NULL)
				TAILQ_INSERT_TAIL(peerfilter_l,
				    curpeer_filter[1], entry);
			curpeer_filter[0] = NULL;
			curpeer_filter[1] = NULL;

			if (neighbor_consistent(curpeer) == -1)
				YYERROR;
			curpeer->next = peer_l;
			peer_l = curpeer;
			curpeer = curgroup;
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
			if (curgroup_filter[0] != NULL)
				TAILQ_INSERT_TAIL(groupfilter_l,
				    curgroup_filter[0], entry);
			if (curgroup_filter[1] != NULL)
				TAILQ_INSERT_TAIL(groupfilter_l,
				    curgroup_filter[1], entry);
			curgroup_filter[0] = NULL;
			curgroup_filter[1] = NULL;

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

peeropts_h	: '{' optnl peeropts_l '}'
		| /* empty */
		;

peeropts_l	: peeropts_l peeroptsl
		| peeroptsl
		;

peeroptsl	: peeropts nl
		| error nl
		;

peeropts	: REMOTEAS as4number	{
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
		| MULTIHOP NUMBER	{
			if ($2 < 2 || $2 > 255) {
				yyerror("invalid multihop distance %d", $2);
				YYERROR;
			}
			curpeer->conf.distance = $2;
		}
		| PASSIVE		{
			curpeer->conf.passive = 1;
		}
		| DOWN		{
			curpeer->conf.down = 1;
		}
		| RIB STRING	{
			if (!find_rib($2)) {
				yyerror("rib \"%s\" does not exist.", $2);
				free($2);
				YYERROR;
			}
			if (strlcpy(curpeer->conf.rib, $2,
			    sizeof(curpeer->conf.rib)) >=
			    sizeof(curpeer->conf.rib)) {
				yyerror("rib name \"%s\" too long: max %u",
				   $2, sizeof(curpeer->conf.rib) - 1);
				free($2);
				YYERROR;
			}
			free($2);
		}
		| HOLDTIME NUMBER	{
			if ($2 < MIN_HOLDTIME || $2 > USHRT_MAX) {
				yyerror("holdtime must be between %u and %u",
				    MIN_HOLDTIME, USHRT_MAX);
				YYERROR;
			}
			curpeer->conf.holdtime = $2;
		}
		| HOLDTIME YMIN NUMBER	{
			if ($3 < MIN_HOLDTIME || $3 > USHRT_MAX) {
				yyerror("holdtime must be between %u and %u",
				    MIN_HOLDTIME, USHRT_MAX);
				YYERROR;
			}
			curpeer->conf.min_holdtime = $3;
		}
		| ANNOUNCE family STRING {
			u_int8_t	aid, safi;
			int8_t		val = 1;

			if (!strcmp($3, "none")) {
				safi = SAFI_UNICAST;
				val = 0;
			} else if (!strcmp($3, "unicast")) {
				safi = SAFI_UNICAST;
			} else if (!strcmp($3, "vpn")) {
				safi = SAFI_MPLSVPN;
			} else {
				yyerror("unknown/unsupported SAFI \"%s\"",
				    $3);
				free($3);
				YYERROR;
			}
			free($3);

			if (afi2aid($2, safi, &aid) == -1) {
				yyerror("unknown AFI/SAFI pair");
				YYERROR;
			}
			curpeer->conf.capabilities.mp[aid] = val;
		}
		| ANNOUNCE CAPABILITIES yesno {
			curpeer->conf.announce_capa = $3;
		}
		| ANNOUNCE REFRESH yesno {
			curpeer->conf.capabilities.refresh = $3;
		}
		| ANNOUNCE RESTART yesno {
			curpeer->conf.capabilities.restart = $3;
		}
		| ANNOUNCE AS4BYTE yesno {
			curpeer->conf.capabilities.as4byte = $3;
		}
		| ANNOUNCE SELF {
			curpeer->conf.announce_type = ANNOUNCE_SELF;
		}
		| ANNOUNCE STRING {
			if (!strcmp($2, "self"))
				curpeer->conf.announce_type = ANNOUNCE_SELF;
			else if (!strcmp($2, "none"))
				curpeer->conf.announce_type = ANNOUNCE_NONE;
			else if (!strcmp($2, "all"))
				curpeer->conf.announce_type = ANNOUNCE_ALL;
			else if (!strcmp($2, "default-route"))
				curpeer->conf.announce_type =
				    ANNOUNCE_DEFAULT_ROUTE;
			else {
				yyerror("invalid announce type");
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
		| MAXPREFIX NUMBER restart {
			if ($2 < 0 || $2 > UINT_MAX) {
				yyerror("bad maximum number of prefixes");
				YYERROR;
			}
			curpeer->conf.max_prefix = $2;
			curpeer->conf.max_prefix_restart = $3;
		}
		| TCP MD5SIG PASSWORD string {
			if (curpeer->conf.auth.method) {
				yyerror("auth method cannot be redefined");
				free($4);
				YYERROR;
			}
			if (strlcpy(curpeer->conf.auth.md5key, $4,
			    sizeof(curpeer->conf.auth.md5key)) >=
			    sizeof(curpeer->conf.auth.md5key)) {
				yyerror("tcp md5sig password too long: max %u",
				    sizeof(curpeer->conf.auth.md5key) - 1);
				free($4);
				YYERROR;
			}
			curpeer->conf.auth.method = AUTH_MD5SIG;
			curpeer->conf.auth.md5key_len = strlen($4);
			free($4);
		}
		| TCP MD5SIG KEY string {
			if (curpeer->conf.auth.method) {
				yyerror("auth method cannot be redefined");
				free($4);
				YYERROR;
			}

			if (str2key($4, curpeer->conf.auth.md5key,
			    sizeof(curpeer->conf.auth.md5key)) == -1) {
				free($4);
				YYERROR;
			}
			curpeer->conf.auth.method = AUTH_MD5SIG;
			curpeer->conf.auth.md5key_len = strlen($4) / 2;
			free($4);
		}
		| IPSEC espah IKE {
			if (curpeer->conf.auth.method) {
				yyerror("auth method cannot be redefined");
				YYERROR;
			}
			if ($2)
				curpeer->conf.auth.method = AUTH_IPSEC_IKE_ESP;
			else
				curpeer->conf.auth.method = AUTH_IPSEC_IKE_AH;
		}
		| IPSEC espah inout SPI NUMBER STRING STRING encspec {
			u_int32_t	auth_alg;
			u_int8_t	keylen;

			if (curpeer->conf.auth.method &&
			    (((curpeer->conf.auth.spi_in && $3 == 1) ||
			    (curpeer->conf.auth.spi_out && $3 == 0)) ||
			    ($2 == 1 && curpeer->conf.auth.method !=
			    AUTH_IPSEC_MANUAL_ESP) ||
			    ($2 == 0 && curpeer->conf.auth.method !=
			    AUTH_IPSEC_MANUAL_AH))) {
				yyerror("auth method cannot be redefined");
				free($6);
				free($7);
				YYERROR;
			}

			if (!strcmp($6, "sha1")) {
				auth_alg = SADB_AALG_SHA1HMAC;
				keylen = 20;
			} else if (!strcmp($6, "md5")) {
				auth_alg = SADB_AALG_MD5HMAC;
				keylen = 16;
			} else {
				yyerror("unknown auth algorithm \"%s\"", $6);
				free($6);
				free($7);
				YYERROR;
			}
			free($6);

			if (strlen($7) / 2 != keylen) {
				yyerror("auth key len: must be %u bytes, "
				    "is %u bytes", keylen, strlen($7) / 2);
				free($7);
				YYERROR;
			}

			if ($2)
				curpeer->conf.auth.method =
				    AUTH_IPSEC_MANUAL_ESP;
			else {
				if ($8.enc_alg) {
					yyerror("\"ipsec ah\" doesn't take "
					    "encryption keys");
					free($7);
					YYERROR;
				}
				curpeer->conf.auth.method =
				    AUTH_IPSEC_MANUAL_AH;
			}

			if ($5 < 0 || $5 > UINT_MAX) {
				yyerror("bad spi number %lld", $5);
				free($7);
				YYERROR;
			}

			if ($3 == 1) {
				if (str2key($7, curpeer->conf.auth.auth_key_in,
				    sizeof(curpeer->conf.auth.auth_key_in)) ==
				    -1) {
					free($7);
					YYERROR;
				}
				curpeer->conf.auth.spi_in = $5;
				curpeer->conf.auth.auth_alg_in = auth_alg;
				curpeer->conf.auth.enc_alg_in = $8.enc_alg;
				memcpy(&curpeer->conf.auth.enc_key_in,
				    &$8.enc_key,
				    sizeof(curpeer->conf.auth.enc_key_in));
				curpeer->conf.auth.enc_keylen_in =
				    $8.enc_key_len;
				curpeer->conf.auth.auth_keylen_in = keylen;
			} else {
				if (str2key($7, curpeer->conf.auth.auth_key_out,
				    sizeof(curpeer->conf.auth.auth_key_out)) ==
				    -1) {
					free($7);
					YYERROR;
				}
				curpeer->conf.auth.spi_out = $5;
				curpeer->conf.auth.auth_alg_out = auth_alg;
				curpeer->conf.auth.enc_alg_out = $8.enc_alg;
				memcpy(&curpeer->conf.auth.enc_key_out,
				    &$8.enc_key,
				    sizeof(curpeer->conf.auth.enc_key_out));
				curpeer->conf.auth.enc_keylen_out =
				    $8.enc_key_len;
				curpeer->conf.auth.auth_keylen_out = keylen;
			}
			free($7);
		}
		| TTLSECURITY yesno	{
			curpeer->conf.ttlsec = $2;
		}
		| SET filter_set_opt	{
			struct filter_rule	*r;

			r = get_rule($2->type);
			if (merge_filterset(&r->set, $2) == -1)
				YYERROR;
		}
		| SET optnl "{" optnl filter_set_l optnl "}"	{
			struct filter_rule	*r;
			struct filter_set	*s;

			while ((s = TAILQ_FIRST($5)) != NULL) {
				TAILQ_REMOVE($5, s, entry);
				r = get_rule(s->type);
				if (merge_filterset(&r->set, s) == -1)
					YYERROR;
			}
			free($5);
		}
		| mrtdump
		| REFLECTOR		{
			if ((conf->flags & BGPD_FLAG_REFLECTOR) &&
			    conf->clusterid != 0) {
				yyerror("only one route reflector "
				    "cluster allowed");
				YYERROR;
			}
			conf->flags |= BGPD_FLAG_REFLECTOR;
			curpeer->conf.reflector_client = 1;
		}
		| REFLECTOR address	{
			if ($2.aid != AID_INET) {
				yyerror("route reflector cluster-id must be "
				    "an IPv4 address");
				YYERROR;
			}
			if ((conf->flags & BGPD_FLAG_REFLECTOR) &&
			    conf->clusterid != $2.v4.s_addr) {
				yyerror("only one route reflector "
				    "cluster allowed");
				YYERROR;
			}
			conf->flags |= BGPD_FLAG_REFLECTOR;
			curpeer->conf.reflector_client = 1;
			conf->clusterid = $2.v4.s_addr;
		}
		| DEPEND ON STRING	{
			if (strlcpy(curpeer->conf.if_depend, $3,
			    sizeof(curpeer->conf.if_depend)) >=
			    sizeof(curpeer->conf.if_depend)) {
				yyerror("interface name \"%s\" too long: "
				    "max %u", $3,
				    sizeof(curpeer->conf.if_depend) - 1);
				free($3);
				YYERROR;
			}
			free($3);
		}
		| DEMOTE STRING		{
			if (strlcpy(curpeer->conf.demote_group, $2,
			    sizeof(curpeer->conf.demote_group)) >=
			    sizeof(curpeer->conf.demote_group)) {
				yyerror("demote group name \"%s\" too long: "
				    "max %u", $2,
				    sizeof(curpeer->conf.demote_group) - 1);
				free($2);
				YYERROR;
			}
			free($2);
			if (carp_demote_init(curpeer->conf.demote_group,
			    conf->opts & BGPD_OPT_FORCE_DEMOTE) == -1) {
				yyerror("error initializing group \"%s\"",
				    curpeer->conf.demote_group);
				YYERROR;
			}
		}
		| SOFTRECONFIG inout yesno {
			if ($2)
				curpeer->conf.softreconfig_in = $3;
			else
				curpeer->conf.softreconfig_out = $3;
		}
		| TRANSPARENT yesno	{
			if ($2 == 1)
				curpeer->conf.flags |= PEERFLAG_TRANS_AS;
			else
				curpeer->conf.flags &= ~PEERFLAG_TRANS_AS;
		}
		;

restart		: /* nada */		{ $$ = 0; }
		| RESTART NUMBER	{
			if ($2 < 1 || $2 > USHRT_MAX) {
				yyerror("restart out of range. 1 to %u minutes",
				    USHRT_MAX);
				YYERROR;
			}
			$$ = $2;
		}
		;

family		: IPV4	{ $$ = AFI_IPv4; }
		| IPV6	{ $$ = AFI_IPv6; }
		;

nettype		: STATIC { $$ = 1; },
		| CONNECTED { $$ = 0; }
		;

espah		: ESP		{ $$ = 1; }
		| AH		{ $$ = 0; }
		;

encspec		: /* nada */	{
			bzero(&$$, sizeof($$));
		}
		| STRING STRING {
			bzero(&$$, sizeof($$));
			if (!strcmp($1, "3des") || !strcmp($1, "3des-cbc")) {
				$$.enc_alg = SADB_EALG_3DESCBC;
				$$.enc_key_len = 21; /* XXX verify */
			} else if (!strcmp($1, "aes") ||
			    !strcmp($1, "aes-128-cbc")) {
				$$.enc_alg = SADB_X_EALG_AES;
				$$.enc_key_len = 16;
			} else {
				yyerror("unknown enc algorithm \"%s\"", $1);
				free($1);
				free($2);
				YYERROR;
			}
			free($1);

			if (strlen($2) / 2 != $$.enc_key_len) {
				yyerror("enc key length wrong: should be %u "
				    "bytes, is %u bytes",
				    $$.enc_key_len * 2, strlen($2));
				free($2);
				YYERROR;
			}

			if (str2key($2, $$.enc_key, sizeof($$.enc_key)) == -1) {
				free($2);
				YYERROR;
			}
			free($2);
		}
		;

filterrule	: action quick filter_rib direction filter_peer_h filter_match_h filter_set
		{
			struct filter_rule	 r;

			bzero(&r, sizeof(r));
			r.action = $1;
			r.quick = $2;
			r.dir = $4;
			if ($3) {
				if (r.dir != DIR_IN) {
					yyerror("rib only allowed on \"from\" "
					    "rules.");
					free($3);
					YYERROR;
				}
				if (!find_rib($3)) {
					yyerror("rib \"%s\" does not exist.",
					    $3);
					free($3);
					YYERROR;
				}
				if (strlcpy(r.rib, $3, sizeof(r.rib)) >=
				    sizeof(r.rib)) {
					yyerror("rib name \"%s\" too long: "
					    "max %u", $3, sizeof(r.rib) - 1);
					free($3);
					YYERROR;
				}
				free($3);
			}
			if (expand_rule(&r, $5, &$6, $7) == -1)
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

filter_rib	: /* empty */	{ $$ = NULL; }
		| RIB STRING	{ $$ = $2; }

filter_peer_h	: filter_peer
		| '{' filter_peer_l '}'		{ $$ = $2; }
		;

filter_peer_l	: filter_peer				{ $$ = $1; }
		| filter_peer_l comma filter_peer	{
			$3->next = $1;
			$$ = $3;
		}
		;

filter_peer	: ANY		{
			if (($$ = calloc(1, sizeof(struct filter_peers_l))) ==
			    NULL)
				fatal(NULL);
			$$->p.peerid = $$->p.groupid = 0;
			$$->next = NULL;
		}
		| address	{
			struct peer *p;

			if (($$ = calloc(1, sizeof(struct filter_peers_l))) ==
			    NULL)
				fatal(NULL);
			$$->p.groupid = $$->p.peerid = 0;
			$$->next = NULL;
			for (p = peer_l; p != NULL; p = p->next)
				if (!memcmp(&p->conf.remote_addr,
				    &$1, sizeof(p->conf.remote_addr))) {
					$$->p.peerid = p->conf.id;
					break;
				}
			if ($$->p.peerid == 0) {
				yyerror("no such peer: %s", log_addr(&$1));
				free($$);
				YYERROR;
			}
		}
		| GROUP STRING	{
			struct peer *p;

			if (($$ = calloc(1, sizeof(struct filter_peers_l))) ==
			    NULL)
				fatal(NULL);
			$$->p.peerid = 0;
			$$->next = NULL;
			for (p = peer_l; p != NULL; p = p->next)
				if (!strcmp(p->conf.group, $2)) {
					$$->p.groupid = p->conf.groupid;
					break;
				}
			if ($$->p.groupid == 0) {
				yyerror("no such group: \"%s\"", $2);
				free($2);
				free($$);
				YYERROR;
			}
			free($2);
		}
		;

filter_prefix_h	: PREFIX filter_prefix			{ $$ = $2; }
		| PREFIX '{' filter_prefix_m '}'	{ $$ = $3; }
		;

filter_prefix_m	: filter_prefix_l
		| '{' filter_prefix_l '}'		{ $$ = $2; }
		| '{' filter_prefix_l '}' filter_prefix_m
		{
			struct filter_prefix_l	*p;

			/* merge, both can be lists */
			for (p = $2; p != NULL && p->next != NULL; p = p->next)
				;	/* nothing */
			if (p != NULL)
				p->next = $4;
			$$ = $2;
		}
		;

filter_prefix_l	: filter_prefix				{ $$ = $1; }
		| filter_prefix_l comma filter_prefix	{
			$3->next = $1;
			$$ = $3;
		}
		;

filter_prefix	: prefix				{
			if (fmopts.aid && fmopts.aid != $1.prefix.aid) {
				yyerror("rules with mixed address families "
				    "are not allowed");
				YYERROR;
			} else
				fmopts.aid = $1.prefix.aid;
			if (($$ = calloc(1, sizeof(struct filter_prefix_l))) ==
			    NULL)
				fatal(NULL);
			memcpy(&$$->p.addr, &$1.prefix,
			    sizeof($$->p.addr));
			$$->p.len = $1.len;
			$$->next = NULL;
		}
		;

filter_as_h	: filter_as_t
		| '{' filter_as_t_l '}'		{ $$ = $2; }
		;

filter_as_t_l	: filter_as_t
		| filter_as_t_l comma filter_as_t		{
			struct filter_as_l	*a;

			/* merge, both can be lists */
			for (a = $1; a != NULL && a->next != NULL; a = a->next)
				;	/* nothing */
			if (a != NULL)
				a->next = $3;
			$$ = $1;
		}
		;

filter_as_t	: filter_as_type filter_as			{
			$$ = $2;
			$$->a.type = $1;
		}
		| filter_as_type '{' filter_as_l_h '}'	{
			struct filter_as_l	*a;

			$$ = $3;
			for (a = $$; a != NULL; a = a->next)
				a->a.type = $1;
		}
		;

filter_as_l_h	: filter_as_l
		| '{' filter_as_l '}'			{ $$ = $2; }
		| '{' filter_as_l '}' filter_as_l_h
		{
			struct filter_as_l	*a;

			/* merge, both can be lists */
			for (a = $2; a != NULL && a->next != NULL; a = a->next)
				;	/* nothing */
			if (a != NULL)
				a->next = $4;
			$$ = $2;
		}
		;

filter_as_l	: filter_as
		| filter_as_l comma filter_as	{
			$3->next = $1;
			$$ = $3;
		}
		;

filter_as	: as4number		{
			if (($$ = calloc(1, sizeof(struct filter_as_l))) ==
			    NULL)
				fatal(NULL);
			$$->a.as = $1;
		}
		| NEIGHBORAS		{
			if (($$ = calloc(1, sizeof(struct filter_as_l))) ==
			    NULL)
				fatal(NULL);
			$$->a.flags = AS_FLAG_NEIGHBORAS;
		}
		;

filter_match_h	: /* empty */			{
			bzero(&$$, sizeof($$));
			$$.m.community.as = COMMUNITY_UNSET;
		}
		| {
			bzero(&fmopts, sizeof(fmopts));
			fmopts.m.community.as = COMMUNITY_UNSET;
		}
		    filter_match		{
			memcpy(&$$, &fmopts, sizeof($$));
		}
		;

filter_match	: filter_elm
		| filter_match filter_elm
		;

filter_elm	: filter_prefix_h	{
			if (fmopts.prefix_l != NULL) {
				yyerror("\"prefix\" already specified");
				YYERROR;
			}
			fmopts.prefix_l = $1;
		}
		| PREFIXLEN prefixlenop		{
			if (fmopts.aid == 0) {
				yyerror("address family needs to be specified "
				    "before \"prefixlen\"");
				YYERROR;
			}
			if (fmopts.m.prefixlen.aid) {
				yyerror("\"prefixlen\" already specified");
				YYERROR;
			}
			memcpy(&fmopts.m.prefixlen, &$2,
			    sizeof(fmopts.m.prefixlen));
			fmopts.m.prefixlen.aid = fmopts.aid;
		}
		| filter_as_h		{
			if (fmopts.as_l != NULL) {
				yyerror("AS filters already specified");
				YYERROR;
			}
			fmopts.as_l = $1;
		}
		| MAXASLEN NUMBER	{
			if (fmopts.m.aslen.type != ASLEN_NONE) {
				yyerror("AS length filters already specified");
				YYERROR;
			}
			if ($2 < 0 || $2 > UINT_MAX) {
				yyerror("bad max-as-len %lld", $2);
				YYERROR;
			}
			fmopts.m.aslen.type = ASLEN_MAX;
			fmopts.m.aslen.aslen = $2;
		}
		| MAXASSEQ NUMBER	{
			if (fmopts.m.aslen.type != ASLEN_NONE) {
				yyerror("AS length filters already specified");
				YYERROR;
			}
			if ($2 < 0 || $2 > UINT_MAX) {
				yyerror("bad max-as-seq %lld", $2);
				YYERROR;
			}
			fmopts.m.aslen.type = ASLEN_SEQ;
			fmopts.m.aslen.aslen = $2;
		}
		| COMMUNITY STRING	{
			if (fmopts.m.community.as != COMMUNITY_UNSET) {
				yyerror("\"community\" already specified");
				free($2);
				YYERROR;
			}
			if (parsecommunity(&fmopts.m.community, $2) == -1) {
				free($2);
				YYERROR;
			}
			free($2);
		}
		| EXTCOMMUNITY STRING STRING {
			if (fmopts.m.ext_community.flags &
			    EXT_COMMUNITY_FLAG_VALID) {
				yyerror("\"ext-community\" already specified");
				free($2);
				free($3);
				YYERROR;
			}

			if (parseextcommunity(&fmopts.m.ext_community,
			    $2, $3) == -1) {
				free($2);
				free($3);
				YYERROR;
			}
			free($2);
			free($3);
		}
		| IPV4			{
			if (fmopts.aid) {
				yyerror("address family already specified");
				YYERROR;
			}
			fmopts.aid = AID_INET;
		}
		| IPV6			{
			if (fmopts.aid) {
				yyerror("address family already specified");
				YYERROR;
			}
			fmopts.aid = AID_INET6;
		}
		;

prefixlenop	: unaryop NUMBER		{
			bzero(&$$, sizeof($$));
			if ($2 < 0 || $2 > 128) {
				yyerror("prefixlen must be < 128");
				YYERROR;
			}
			$$.op = $1;
			$$.len_min = $2;
		}
		| NUMBER binaryop NUMBER	{
			bzero(&$$, sizeof($$));
			if ($1 < 0 || $1 > 128 || $3 < 0 || $3 > 128) {
				yyerror("prefixlen must be < 128");
				YYERROR;
			}
			if ($1 >= $3) {
				yyerror("start prefixlen is bigger than end");
				YYERROR;
			}
			$$.op = $2;
			$$.len_min = $1;
			$$.len_max = $3;
		}
		;

filter_as_type	: AS		{ $$ = AS_ALL; }
		| SOURCEAS	{ $$ = AS_SOURCE; }
		| TRANSITAS	{ $$ = AS_TRANSIT; }
		| PEERAS	{ $$ = AS_PEER; }
		;

filter_set	: /* empty */					{ $$ = NULL; }
		| SET filter_set_opt				{
			if (($$ = calloc(1, sizeof(struct filter_set_head))) ==
			    NULL)
				fatal(NULL);
			TAILQ_INIT($$);
			TAILQ_INSERT_TAIL($$, $2, entry);
		}
		| SET optnl "{" optnl filter_set_l optnl "}"	{ $$ = $5; }
		;

filter_set_l	: filter_set_l comma filter_set_opt	{
			$$ = $1;
			if (merge_filterset($$, $3) == 1)
				YYERROR;
		}
		| filter_set_opt {
			if (($$ = calloc(1, sizeof(struct filter_set_head))) ==
			    NULL)
				fatal(NULL);
			TAILQ_INIT($$);
			TAILQ_INSERT_TAIL($$, $1, entry);
		}
		;

delete		: /* empty */	{ $$ = 0; }
		| DELETE	{ $$ = 1; }
		;

filter_set_opt	: LOCALPREF NUMBER		{
			if ($2 < -INT_MAX || $2 > UINT_MAX) {
				yyerror("bad localpref %lld", $2);
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			if ($2 > 0) {
				$$->type = ACTION_SET_LOCALPREF;
				$$->action.metric = $2;
			} else {
				$$->type = ACTION_SET_RELATIVE_LOCALPREF;
				$$->action.relative = $2;
			}
		}
		| LOCALPREF '+' NUMBER		{
			if ($3 < 0 || $3 > INT_MAX) {
				yyerror("bad localpref +%lld", $3);
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_SET_RELATIVE_LOCALPREF;
			$$->action.relative = $3;
		}
		| LOCALPREF '-' NUMBER		{
			if ($3 < 0 || $3 > INT_MAX) {
				yyerror("bad localpref -%lld", $3);
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_SET_RELATIVE_LOCALPREF;
			$$->action.relative = -$3;
		}
		| MED NUMBER			{
			if ($2 < -INT_MAX || $2 > UINT_MAX) {
				yyerror("bad metric %lld", $2);
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			if ($2 > 0) {
				$$->type = ACTION_SET_MED;
				$$->action.metric = $2;
			} else {
				$$->type = ACTION_SET_RELATIVE_MED;
				$$->action.relative = $2;
			}
		}
		| MED '+' NUMBER			{
			if ($3 < 0 || $3 > INT_MAX) {
				yyerror("bad metric +%lld", $3);
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_SET_RELATIVE_MED;
			$$->action.relative = $3;
		}
		| MED '-' NUMBER			{
			if ($3 < 0 || $3 > INT_MAX) {
				yyerror("bad metric -%lld", $3);
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_SET_RELATIVE_MED;
			$$->action.relative = -$3;
		}
		| METRIC NUMBER			{	/* alias for MED */
			if ($2 < -INT_MAX || $2 > UINT_MAX) {
				yyerror("bad metric %lld", $2);
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			if ($2 > 0) {
				$$->type = ACTION_SET_MED;
				$$->action.metric = $2;
			} else {
				$$->type = ACTION_SET_RELATIVE_MED;
				$$->action.relative = $2;
			}
		}
		| METRIC '+' NUMBER			{
			if ($3 < 0 || $3 > INT_MAX) {
				yyerror("bad metric +%lld", $3);
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_SET_RELATIVE_MED;
			$$->action.metric = $3;
		}
		| METRIC '-' NUMBER			{
			if ($3 < 0 || $3 > INT_MAX) {
				yyerror("bad metric -%lld", $3);
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_SET_RELATIVE_MED;
			$$->action.relative = -$3;
		}
		| WEIGHT NUMBER				{
			if ($2 < -INT_MAX || $2 > UINT_MAX) {
				yyerror("bad weight %lld", $2);
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			if ($2 > 0) {
				$$->type = ACTION_SET_WEIGHT;
				$$->action.metric = $2;
			} else {
				$$->type = ACTION_SET_RELATIVE_WEIGHT;
				$$->action.relative = $2;
			}
		}
		| WEIGHT '+' NUMBER			{
			if ($3 < 0 || $3 > INT_MAX) {
				yyerror("bad weight +%lld", $3);
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_SET_RELATIVE_WEIGHT;
			$$->action.relative = $3;
		}
		| WEIGHT '-' NUMBER			{
			if ($3 < 0 || $3 > INT_MAX) {
				yyerror("bad weight -%lld", $3);
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_SET_RELATIVE_WEIGHT;
			$$->action.relative = -$3;
		}
		| NEXTHOP address		{
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_SET_NEXTHOP;
			memcpy(&$$->action.nexthop, &$2,
			    sizeof($$->action.nexthop));
		}
		| NEXTHOP BLACKHOLE		{
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_SET_NEXTHOP_BLACKHOLE;
		}
		| NEXTHOP REJECT		{
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_SET_NEXTHOP_REJECT;
		}
		| NEXTHOP NOMODIFY		{
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_SET_NEXTHOP_NOMODIFY;
		}
		| NEXTHOP SELF		{
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_SET_NEXTHOP_SELF;
		}
		| PREPEND_SELF NUMBER		{
			if ($2 < 0 || $2 > 128) {
				yyerror("bad number of prepends");
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_SET_PREPEND_SELF;
			$$->action.prepend = $2;
		}
		| PREPEND_PEER NUMBER		{
			if ($2 < 0 || $2 > 128) {
				yyerror("bad number of prepends");
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_SET_PREPEND_PEER;
			$$->action.prepend = $2;
		}
		| PFTABLE STRING		{
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_PFTABLE;
			if (!(conf->opts & BGPD_OPT_NOACTION) &&
			    pftable_exists($2) != 0) {
				yyerror("pftable name does not exist");
				free($2);
				free($$);
				YYERROR;
			}
			if (strlcpy($$->action.pftable, $2,
			    sizeof($$->action.pftable)) >=
			    sizeof($$->action.pftable)) {
				yyerror("pftable name too long");
				free($2);
				free($$);
				YYERROR;
			}
			if (pftable_add($2) != 0) {
				yyerror("Couldn't register table");
				free($2);
				free($$);
				YYERROR;
			}
			free($2);
		}
		| RTLABEL STRING		{
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_RTLABEL;
			if (strlcpy($$->action.rtlabel, $2,
			    sizeof($$->action.rtlabel)) >=
			    sizeof($$->action.rtlabel)) {
				yyerror("rtlabel name too long");
				free($2);
				free($$);
				YYERROR;
			}
			free($2);
		}
		| COMMUNITY delete STRING	{
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			if ($2)
				$$->type = ACTION_DEL_COMMUNITY;
			else
				$$->type = ACTION_SET_COMMUNITY;

			if (parsecommunity(&$$->action.community, $3) == -1) {
				free($3);
				free($$);
				YYERROR;
			}
			free($3);
			/* Don't allow setting of any match */
			if (!$2 && ($$->action.community.as == COMMUNITY_ANY ||
			    $$->action.community.type == COMMUNITY_ANY)) {
				yyerror("'*' is not allowed in set community");
				free($$);
				YYERROR;
			}
		}
		| EXTCOMMUNITY delete STRING STRING {
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			if ($2)
				$$->type = ACTION_DEL_EXT_COMMUNITY;
			else
				$$->type = ACTION_SET_EXT_COMMUNITY;

			if (parseextcommunity(&$$->action.ext_community,
			    $3, $4) == -1) {
				free($3);
				free($4);
				free($$);
				YYERROR;
			}
			free($3);
			free($4);
		}
		| ORIGIN origincode {
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_SET_ORIGIN;
			$$->action.origin = $2;
		}
		;

origincode	: string {
			if (!strcmp($1, "egp"))
				$$ = ORIGIN_EGP;
			else if (!strcmp($1, "igp"))
				$$ = ORIGIN_IGP;
			else if (!strcmp($1, "incomplete"))
				$$ = ORIGIN_INCOMPLETE;
			else {
				yyerror("unknown origin \"%s\"", $1);
				free($1);
				YYERROR;
			}
			free($1);
		};

comma		: ","
		| /* empty */
		;

unaryop		: '='		{ $$ = OP_EQ; }
		| NE		{ $$ = OP_NE; }
		| LE		{ $$ = OP_LE; }
		| '<'		{ $$ = OP_LT; }
		| GE		{ $$ = OP_GE; }
		| '>'		{ $$ = OP_GT; }
		;

binaryop	: '-'		{ $$ = OP_RANGE; }
		| XRANGE	{ $$ = OP_XRANGE; }
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

	file->errors++;
	va_start(ap, fmt);
	if (asprintf(&nfmt, "%s:%d: %s", file->name, yylval.lineno, fmt) == -1)
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
		{ "IPv4",		IPV4},
		{ "IPv6",		IPV6},
		{ "ah",			AH},
		{ "allow",		ALLOW},
		{ "announce",		ANNOUNCE},
		{ "any",		ANY},
		{ "as-4byte",		AS4BYTE },
		{ "blackhole",		BLACKHOLE},
		{ "capabilities",	CAPABILITIES},
		{ "community",		COMMUNITY},
		{ "compare",		COMPARE},
		{ "connect-retry",	CONNECTRETRY},
		{ "connected",		CONNECTED},
		{ "delete",		DELETE},
		{ "demote",		DEMOTE},
		{ "deny",		DENY},
		{ "depend",		DEPEND},
		{ "descr",		DESCR},
		{ "down",		DOWN},
		{ "dump",		DUMP},
		{ "enforce",		ENFORCE},
		{ "esp",		ESP},
		{ "evaluate",		EVALUATE},
		{ "export-target",	EXPORTTRGT},
		{ "ext-community",	EXTCOMMUNITY},
		{ "fib-update",		FIBUPDATE},
		{ "from",		FROM},
		{ "group",		GROUP},
		{ "holdtime",		HOLDTIME},
		{ "ignore",		IGNORE},
		{ "ike",		IKE},
		{ "import-target",	IMPORTTRGT},
		{ "in",			IN},
		{ "include",		INCLUDE},
		{ "inet",		IPV4},
		{ "inet6",		IPV6},
		{ "ipsec",		IPSEC},
		{ "key",		KEY},
		{ "listen",		LISTEN},
		{ "local-address",	LOCALADDR},
		{ "localpref",		LOCALPREF},
		{ "log",		LOG},
		{ "match",		MATCH},
		{ "max-as-len",		MAXASLEN},
		{ "max-as-seq",		MAXASSEQ},
		{ "max-prefix",		MAXPREFIX},
		{ "md5sig",		MD5SIG},
		{ "med",		MED},
		{ "metric",		METRIC},
		{ "min",		YMIN},
		{ "multihop",		MULTIHOP},
		{ "neighbor",		NEIGHBOR},
		{ "neighbor-as",	NEIGHBORAS},
		{ "network",		NETWORK},
		{ "nexthop",		NEXTHOP},
		{ "no-modify",		NOMODIFY},
		{ "on",			ON},
		{ "origin",		ORIGIN},
		{ "out",		OUT},
		{ "passive",		PASSIVE},
		{ "password",		PASSWORD},
		{ "peer-as",		PEERAS},
		{ "pftable",		PFTABLE},
		{ "prefix",		PREFIX},
		{ "prefixlen",		PREFIXLEN},
		{ "prepend-neighbor",	PREPEND_PEER},
		{ "prepend-self",	PREPEND_SELF},
		{ "qualify",		QUALIFY},
		{ "quick",		QUICK},
		{ "rd",			RD},
		{ "rde",		RDE},
		{ "rdomain",		RDOMAIN},
		{ "refresh",		REFRESH },
		{ "reject",		REJECT},
		{ "remote-as",		REMOTEAS},
		{ "restart",		RESTART},
		{ "restricted",		RESTRICTED},
		{ "rib",		RIB},
		{ "route-collector",	ROUTECOLL},
		{ "route-reflector",	REFLECTOR},
		{ "router-id",		ROUTERID},
		{ "rtable",		RTABLE},
		{ "rtlabel",		RTLABEL},
		{ "self",		SELF},
		{ "set",		SET},
		{ "socket",		SOCKET },
		{ "softreconfig",	SOFTRECONFIG},
		{ "source-as",		SOURCEAS},
		{ "spi",		SPI},
		{ "static",		STATIC},
		{ "tcp",		TCP},
		{ "to",			TO},
		{ "transit-as",		TRANSITAS},
		{ "transparent-as",	TRANSPARENT},
		{ "ttl-security",	TTLSECURITY},
		{ "via",		VIA},
		{ "weight",		WEIGHT}
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
			fatal("yylex: strdup");
		return (STRING);
	case '!':
		next = lgetc(0);
		if (next == '=')
			return (NE);
		lungetc(next);
		break;
	case '<':
		next = lgetc(0);
		if (next == '=')
			return (LE);
		lungetc(next);
		break;
	case '>':
		next = lgetc(0);
		if (next == '<')
			return (XRANGE);
		else if (next == '=')
			return (GE);
		lungetc(next);
		break;
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
	x != '!' && x != '=' && x != '/' && x != '#' && \
	x != ','))

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
				fatal("yylex: strdup");
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
	}
	if (secret &&
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
parse_config(char *filename, struct bgpd_config *xconf,
    struct mrt_head *xmconf, struct peer **xpeers, struct network_head *nc,
    struct filter_head *xfilter_l, struct rdomain_head *xrdom_l)
{
	struct sym		*sym, *next;
	struct peer		*p, *pnext;
	struct listen_addr	*la;
	struct network		*n;
	struct filter_rule	*r;
	struct rde_rib		*rr;
	struct rdomain		*rd;
	int			 errors = 0;

	if ((conf = calloc(1, sizeof(struct bgpd_config))) == NULL)
		fatal(NULL);
	conf->opts = xconf->opts;
	conf->csock = strdup(SOCKET_NAME);

	if ((file = pushfile(filename, 1)) == NULL) {
		free(conf);
		return (-1);
	}
	topfile = file;

	if ((mrtconf = calloc(1, sizeof(struct mrt_head))) == NULL)
		fatal(NULL);
	if ((listen_addrs = calloc(1, sizeof(struct listen_addrs))) == NULL)
		fatal(NULL);
	if ((filter_l = calloc(1, sizeof(struct filter_head))) == NULL)
		fatal(NULL);
	if ((peerfilter_l = calloc(1, sizeof(struct filter_head))) == NULL)
		fatal(NULL);
	if ((groupfilter_l = calloc(1, sizeof(struct filter_head))) == NULL)
		fatal(NULL);
	LIST_INIT(mrtconf);
	TAILQ_INIT(listen_addrs);
	TAILQ_INIT(filter_l);
	TAILQ_INIT(peerfilter_l);
	TAILQ_INIT(groupfilter_l);

	peer_l = NULL;
	peer_l_old = *xpeers;
	curpeer = NULL;
	curgroup = NULL;
	id = 1;

	/* network list is always empty in the parent */
	gnetconf = netconf = nc;
	TAILQ_INIT(netconf);
	/* init the empty filter list for later */
	TAILQ_INIT(xfilter_l);
	SIMPLEQ_INIT(xrdom_l);
	rdom_l = xrdom_l;

	add_rib("Adj-RIB-In", 0, F_RIB_NOFIB | F_RIB_NOEVALUATE);
	add_rib("Loc-RIB", 0, 0);

	yyparse();
	errors = file->errors;
	popfile();

	/* Free macros and check which have not been used. */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entry);
		if ((conf->opts & BGPD_OPT_VERBOSE2) && !sym->used)
			fprintf(stderr, "warning: macro \"%s\" not "
			    "used\n", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
			free(sym);
		}
	}

	if (errors) {
		/* XXX more leaks in this case */
		free(conf->csock);
		free(conf->rcsock);

		while ((la = TAILQ_FIRST(listen_addrs)) != NULL) {
			TAILQ_REMOVE(listen_addrs, la, entry);
			free(la);
		}
		free(listen_addrs);

		for (p = peer_l; p != NULL; p = pnext) {
			pnext = p->next;
			free(p);
		}

		while ((n = TAILQ_FIRST(netconf)) != NULL) {
			TAILQ_REMOVE(netconf, n, entry);
			filterset_free(&n->net.attrset);
			free(n);
		}

		while ((r = TAILQ_FIRST(filter_l)) != NULL) {
			TAILQ_REMOVE(filter_l, r, entry);
			filterset_free(&r->set);
			free(r);
		}

		while ((r = TAILQ_FIRST(peerfilter_l)) != NULL) {
			TAILQ_REMOVE(peerfilter_l, r, entry);
			filterset_free(&r->set);
			free(r);
		}

		while ((r = TAILQ_FIRST(groupfilter_l)) != NULL) {
			TAILQ_REMOVE(groupfilter_l, r, entry);
			filterset_free(&r->set);
			free(r);
		}
		while ((rr = SIMPLEQ_FIRST(&ribnames)) != NULL) {
			SIMPLEQ_REMOVE_HEAD(&ribnames, entry);
			free(rr);
		}
		while ((rd = SIMPLEQ_FIRST(rdom_l)) != NULL) {
			SIMPLEQ_REMOVE_HEAD(rdom_l, entry);
			filterset_free(&rd->export);
			filterset_free(&rd->import);

			while ((n = TAILQ_FIRST(&rd->net_l)) != NULL) {
				TAILQ_REMOVE(&rd->net_l, n, entry);
				filterset_free(&n->net.attrset);
				free(n);
			}

			free(rd);
		}
	} else {
		errors += merge_config(xconf, conf, peer_l, listen_addrs);
		errors += mrt_mergeconfig(xmconf, mrtconf);
		*xpeers = peer_l;

		for (p = peer_l_old; p != NULL; p = pnext) {
			pnext = p->next;
			free(p);
		}

		/*
		 * Move filter list and static group and peer filtersets
		 * together. Static group sets come first then peer sets
		 * last normal filter rules.
		 */
		while ((r = TAILQ_FIRST(groupfilter_l)) != NULL) {
			TAILQ_REMOVE(groupfilter_l, r, entry);
			TAILQ_INSERT_TAIL(xfilter_l, r, entry);
		}
		while ((r = TAILQ_FIRST(peerfilter_l)) != NULL) {
			TAILQ_REMOVE(peerfilter_l, r, entry);
			TAILQ_INSERT_TAIL(xfilter_l, r, entry);
		}
		while ((r = TAILQ_FIRST(filter_l)) != NULL) {
			TAILQ_REMOVE(filter_l, r, entry);
			TAILQ_INSERT_TAIL(xfilter_l, r, entry);
		}
	}

	free(conf);
	free(mrtconf);
	free(filter_l);
	free(peerfilter_l);
	free(groupfilter_l);

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

	TAILQ_FOREACH(sym, &symhead, entry)
		if (strcmp(nam, sym->nam) == 0) {
			sym->used = 1;
			return (sym->val);
		}
	return (NULL);
}

int
getcommunity(char *s)
{
	int		 val;
	const char	*errstr;

	if (strcmp(s, "*") == 0)
		return (COMMUNITY_ANY);
	if (strcmp(s, "neighbor-as") == 0)
		return (COMMUNITY_NEIGHBOR_AS);
	val = strtonum(s, 0, USHRT_MAX, &errstr);
	if (errstr) {
		yyerror("Community %s is %s (max: %u)", s, errstr, USHRT_MAX);
		return (COMMUNITY_ERROR);
	}
	return (val);
}

int
parsecommunity(struct filter_community *c, char *s)
{
	char *p;
	int i, as;

	/* Well-known communities */
	if (strcasecmp(s, "NO_EXPORT") == 0) {
		c->as = COMMUNITY_WELLKNOWN;
		c->type = COMMUNITY_NO_EXPORT;
		return (0);
	} else if (strcasecmp(s, "NO_ADVERTISE") == 0) {
		c->as = COMMUNITY_WELLKNOWN;
		c->type = COMMUNITY_NO_ADVERTISE;
		return (0);
	} else if (strcasecmp(s, "NO_EXPORT_SUBCONFED") == 0) {
		c->as = COMMUNITY_WELLKNOWN;
		c->type = COMMUNITY_NO_EXPSUBCONFED;
		return (0);
	} else if (strcasecmp(s, "NO_PEER") == 0) {
		c->as = COMMUNITY_WELLKNOWN;
		c->type = COMMUNITY_NO_PEER;
		return (0);
	}

	if ((p = strchr(s, ':')) == NULL) {
		yyerror("Bad community syntax");
		return (-1);
	}
	*p++ = 0;

	if ((i = getcommunity(s)) == COMMUNITY_ERROR)
		return (-1);
	if (i == COMMUNITY_WELLKNOWN) {
		yyerror("Bad community AS number");
		return (-1);
	}
	as = i;

	if ((i = getcommunity(p)) == COMMUNITY_ERROR)
		return (-1);
	c->as = as;
	c->type = i;

	return (0);
}

int
parsesubtype(char *type)
{
	/* this has to be sorted always */
	static const struct keywords keywords[] = {
		{ "bdc",	EXT_COMMUNITY_BGP_COLLECT },
		{ "odi",	EXT_COMMUNITY_OSPF_DOM_ID },
		{ "ori",	EXT_COMMUNITY_OSPF_RTR_ID },
		{ "ort",	EXT_COMMUNITY_OSPF_RTR_TYPE },
		{ "rt",		EXT_COMMUNITY_ROUTE_TGT },
		{ "soo",	EXT_CUMMUNITY_ROUTE_ORIG }
	};
	const struct keywords	*p;

	p = bsearch(type, keywords, sizeof(keywords)/sizeof(keywords[0]),
	    sizeof(keywords[0]), kw_cmp);

	if (p)
		return (p->k_val);
	else
		return (-1);
}

int
parseextvalue(char *s, u_int32_t *v)
{
	const char 	*errstr;
	char		*p;
	struct in_addr	 ip;
	u_int32_t	 uvalh = 0, uval;

	if ((p = strchr(s, '.')) == NULL) {
		/* AS_PLAIN number (4 or 2 byte) */
		uval = strtonum(s, 0, UINT_MAX, &errstr);
		if (errstr) {
			yyerror("Bad ext-community %s is %s", s, errstr);
			return (-1);
		}
		*v = uval;
		if (uval > USHRT_MAX)
			return (EXT_COMMUNITY_FOUR_AS);
		else
			return (EXT_COMMUNITY_TWO_AS);
	} else if (strchr(p + 1, '.') == NULL) {
		/* AS_DOT number (4-byte) */
		*p++ = '\0';
		uvalh = strtonum(s, 0, USHRT_MAX, &errstr);
		if (errstr) {
			yyerror("Bad ext-community %s is %s", s, errstr);
			return (-1);
		}
		uval = strtonum(p, 0, USHRT_MAX, &errstr);
		if (errstr) {
			yyerror("Bad ext-community %s is %s", p, errstr);
			return (-1);
		}
		*v = uval | (uvalh << 16);
		return (EXT_COMMUNITY_FOUR_AS);
	} else {
		/* more then one dot -> IP address */
		if (inet_aton(s, &ip) == 0) {
			yyerror("Bad ext-community %s not parseable", s);
			return (-1);
		}
		*v = ip.s_addr;
		return (EXT_COMMUNITY_IPV4);
	}
	return (-1);
}

int
parseextcommunity(struct filter_extcommunity *c, char *t, char *s)
{
	const struct ext_comm_pairs	 iana[] = IANA_EXT_COMMUNITIES;
	const char 	*errstr;
	u_int64_t	 ullval;
	u_int32_t	 uval;
	char		*p, *ep;
	unsigned int	 i;
	int		 type, subtype;

	if ((subtype = parsesubtype(t)) == -1) {
		yyerror("Bad ext-community unknown type");
		return (-1);
	}

	if ((p = strchr(s, ':')) == NULL) {
		type = EXT_COMMUNITY_OPAQUE,
		errno = 0;
		ullval = strtoull(s, &ep, 0);
		if (s[0] == '\0' || *ep != '\0') {
			yyerror("Bad ext-community bad value");
			return (-1);
		}
		if (errno == ERANGE && ullval > EXT_COMMUNITY_OPAQUE_MAX) {
			yyerror("Bad ext-community value to big");
			return (-1);
		}
		c->data.ext_opaq = ullval;
	} else {
		*p++ = '\0';
		if ((type = parseextvalue(s, &uval)) == -1)
			return (-1);
		switch (type) {
		case EXT_COMMUNITY_TWO_AS:
			ullval = strtonum(p, 0, UINT_MAX, &errstr);
			break;
		case EXT_COMMUNITY_IPV4:
		case EXT_COMMUNITY_FOUR_AS:
			ullval = strtonum(p, 0, USHRT_MAX, &errstr);
			break;
		default:
			fatalx("parseextcommunity: unexpected result");
		}
		if (errstr) {
			yyerror("Bad ext-community %s is %s", p,
			    errstr);
			return (-1);
		}
		switch (type) {
		case EXT_COMMUNITY_TWO_AS:
			c->data.ext_as.as = uval;
			c->data.ext_as.val = ullval;
			break;
		case EXT_COMMUNITY_IPV4:
			c->data.ext_ip.addr.s_addr = uval;
			c->data.ext_ip.val = ullval;
			break;
		case EXT_COMMUNITY_FOUR_AS:
			c->data.ext_as4.as4 = uval;
			c->data.ext_as4.val = ullval;
			break;
		}
	}
	c->type = type;
	c->subtype = subtype;

	/* verify type/subtype combo */
	for (i = 0; i < sizeof(iana)/sizeof(iana[0]); i++) {
		if (iana[i].type == type && iana[i].subtype == subtype) {
			if (iana[i].transitive)
				c->type |= EXT_COMMUNITY_TRANSITIVE;
			c->flags |= EXT_COMMUNITY_FLAG_VALID;
			return (0);
		}
	}

	yyerror("Bad ext-community bad format for type");
	return (-1);
}

struct peer *
alloc_peer(void)
{
	struct peer	*p;
	u_int8_t	 i;

	if ((p = calloc(1, sizeof(struct peer))) == NULL)
		fatal("new_peer");

	/* some sane defaults */
	p->state = STATE_NONE;
	p->next = NULL;
	p->conf.distance = 1;
	p->conf.announce_type = ANNOUNCE_UNDEF;
	p->conf.announce_capa = 1;
	for (i = 0; i < AID_MAX; i++)
		p->conf.capabilities.mp[i] = -1;
	p->conf.capabilities.refresh = 1;
	p->conf.capabilities.restart = 0;
	p->conf.capabilities.as4byte = 1;
	p->conf.local_as = conf->as;
	p->conf.local_short_as = conf->short_as;
	p->conf.softreconfig_in = 1;
	p->conf.softreconfig_out = 1;

	return (p);
}

struct peer *
new_peer(void)
{
	struct peer		*p;

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
		p->conf.local_as = curgroup->conf.local_as;
		p->conf.local_short_as = curgroup->conf.local_short_as;
	}
	p->next = NULL;
	if (conf->flags & BGPD_FLAG_DECISION_TRANS_AS)
		p->conf.flags |= PEERFLAG_TRANS_AS;
	return (p);
}

struct peer *
new_group(void)
{
	return (alloc_peer());
}

int
add_mrtconfig(enum mrt_type type, char *name, time_t timeout, struct peer *p,
    char *rib)
{
	struct mrt	*m, *n;

	LIST_FOREACH(m, mrtconf, entry) {
		if ((rib && strcmp(rib, m->rib)) ||
		    (!rib && *m->rib))
			continue;
		if (p == NULL) {
			if (m->peer_id != 0 || m->group_id != 0)
				continue;
		} else {
			if (m->peer_id != p->conf.id ||
			    m->group_id != p->conf.groupid)
				continue;
		}
		if (m->type == type) {
			yyerror("only one mrtdump per type allowed.");
			return (-1);
		}
	}

	if ((n = calloc(1, sizeof(struct mrt_config))) == NULL)
		fatal("add_mrtconfig");

	n->type = type;
	if (strlcpy(MRT2MC(n)->name, name, sizeof(MRT2MC(n)->name)) >=
	    sizeof(MRT2MC(n)->name)) {
		yyerror("filename \"%s\" too long: max %u",
		    name, sizeof(MRT2MC(n)->name) - 1);
		free(n);
		return (-1);
	}
	MRT2MC(n)->ReopenTimerInterval = timeout;
	if (p != NULL) {
		if (curgroup == p) {
			n->peer_id = 0;
			n->group_id = p->conf.id;
		} else {
			n->peer_id = p->conf.id;
			n->group_id = 0;
		}
	}
	if (rib) {
		if (!find_rib(rib)) {
			yyerror("rib \"%s\" does not exist.", rib);
			free(n);
			return (-1);
		}
		if (strlcpy(n->rib, rib, sizeof(n->rib)) >=
		    sizeof(n->rib)) {
			yyerror("rib name \"%s\" too long: max %u",
			    name, sizeof(n->rib) - 1);
			free(n);
			return (-1);
		}
	}

	LIST_INSERT_HEAD(mrtconf, n, entry);

	return (0);
}

int
add_rib(char *name, u_int rtableid, u_int16_t flags)
{
	struct rde_rib	*rr;
	u_int		 rdom;

	if ((rr = find_rib(name)) == NULL) {
		if ((rr = calloc(1, sizeof(*rr))) == NULL) {
			log_warn("add_rib");
			return (-1);
		}
	}
	if (strlcpy(rr->name, name, sizeof(rr->name)) >= sizeof(rr->name)) {
		yyerror("rib name \"%s\" too long: max %u",
		   name, sizeof(rr->name) - 1);
		free(rr);
		return (-1);
	}
	rr->flags |= flags;
	if ((rr->flags & F_RIB_HASNOFIB) == 0) {
		if (ktable_exists(rtableid, &rdom) != 1) {
			yyerror("rtable id %lld does not exist", rtableid);
			free(rr);
			return (-1);
		}
		if (rdom != 0) {
			yyerror("rtable %lld does not belong to rdomain 0",
			    rtableid);
			free(rr);
			return (-1);
		}
		rr->rtableid = rtableid;
	}
	SIMPLEQ_INSERT_TAIL(&ribnames, rr, entry);
	return (0);
}

struct rde_rib *
find_rib(char *name)
{
	struct rde_rib	*rr;

	SIMPLEQ_FOREACH(rr, &ribnames, entry) {
		if (!strcmp(rr->name, name))
			return (rr);
	}
	return (NULL);
}

int
get_id(struct peer *newpeer)
{
	struct peer	*p;

	for (p = peer_l_old; p != NULL; p = p->next)
		if (newpeer->conf.remote_addr.aid) {
			if (!memcmp(&p->conf.remote_addr,
			    &newpeer->conf.remote_addr,
			    sizeof(p->conf.remote_addr))) {
				newpeer->conf.id = p->conf.id;
				return (0);
			}
		} else {	/* newpeer is a group */
			if (strcmp(newpeer->conf.group, p->conf.group) == 0) {
				newpeer->conf.id = p->conf.groupid;
				return (0);
			}
		}

	/* new one */
	for (; id < UINT_MAX / 2; id++) {
		for (p = peer_l_old; p != NULL &&
		    p->conf.id != id && p->conf.groupid != id; p = p->next)
			;	/* nothing */
		if (p == NULL) {	/* we found a free id */
			newpeer->conf.id = id++;
			return (0);
		}
	}

	return (-1);
}

int
expand_rule(struct filter_rule *rule, struct filter_peers_l *peer,
    struct filter_match_l *match, struct filter_set_head *set)
{
	struct filter_rule	*r;
	struct filter_peers_l	*p, *pnext;
	struct filter_prefix_l	*prefix, *prefix_next;
	struct filter_as_l	*a, *anext;
	struct filter_set	*s;

	p = peer;
	do {
		prefix = match->prefix_l;
		do {
			a = match->as_l;
			do {
				if ((r = calloc(1,
				    sizeof(struct filter_rule))) == NULL) {
					log_warn("expand_rule");
					return (-1);
				}

				memcpy(r, rule, sizeof(struct filter_rule));
				memcpy(&r->match, match,
				    sizeof(struct filter_match));
				TAILQ_INIT(&r->set);
				copy_filterset(set, &r->set);

				if (p != NULL)
					memcpy(&r->peer, &p->p,
					    sizeof(struct filter_peers));

				if (prefix != NULL)
					memcpy(&r->match.prefix, &prefix->p,
					    sizeof(r->match.prefix));

				if (a != NULL)
					memcpy(&r->match.as, &a->a,
					    sizeof(struct filter_as));

				TAILQ_INSERT_TAIL(filter_l, r, entry);

				if (a != NULL)
					a = a->next;
			} while (a != NULL);

			if (prefix != NULL)
				prefix = prefix->next;
		} while (prefix != NULL);

		if (p != NULL)
			p = p->next;
	} while (p != NULL);

	for (p = peer; p != NULL; p = pnext) {
		pnext = p->next;
		free(p);
	}

	for (prefix = match->prefix_l; prefix != NULL; prefix = prefix_next) {
		prefix_next = prefix->next;
		free(prefix);
	}

	for (a = match->as_l; a != NULL; a = anext) {
		anext = a->next;
		free(a);
	}

	if (set != NULL) {
		while ((s = TAILQ_FIRST(set)) != NULL) {
			TAILQ_REMOVE(set, s, entry);
			free(s);
		}
		free(set);
	}

	return (0);
}

int
str2key(char *s, char *dest, size_t max_len)
{
	unsigned	i;
	char		t[3];

	if (strlen(s) / 2 > max_len) {
		yyerror("key too long");
		return (-1);
	}

	if (strlen(s) % 2) {
		yyerror("key must be of even length");
		return (-1);
	}

	for (i = 0; i < strlen(s) / 2; i++) {
		t[0] = s[2*i];
		t[1] = s[2*i + 1];
		t[2] = 0;
		if (!isxdigit(t[0]) || !isxdigit(t[1])) {
			yyerror("key must be specified in hex");
			return (-1);
		}
		dest[i] = strtoul(t, NULL, 16);
	}

	return (0);
}

int
neighbor_consistent(struct peer *p)
{
	u_int8_t	i;

	/* local-address and peer's address: same address family */
	if (p->conf.local_addr.aid &&
	    p->conf.local_addr.aid != p->conf.remote_addr.aid) {
		yyerror("local-address and neighbor address "
		    "must be of the same address family");
		return (-1);
	}

	/* with any form of ipsec local-address is required */
	if ((p->conf.auth.method == AUTH_IPSEC_IKE_ESP ||
	    p->conf.auth.method == AUTH_IPSEC_IKE_AH ||
	    p->conf.auth.method == AUTH_IPSEC_MANUAL_ESP ||
	    p->conf.auth.method == AUTH_IPSEC_MANUAL_AH) &&
	    !p->conf.local_addr.aid) {
		yyerror("neighbors with any form of IPsec configured "
		    "need local-address to be specified");
		return (-1);
	}

	/* with static keying we need both directions */
	if ((p->conf.auth.method == AUTH_IPSEC_MANUAL_ESP ||
	    p->conf.auth.method == AUTH_IPSEC_MANUAL_AH) &&
	    (!p->conf.auth.spi_in || !p->conf.auth.spi_out)) {
		yyerror("with manual keyed IPsec, SPIs and keys "
		    "for both directions are required");
		return (-1);
	}

	if (!conf->as) {
		yyerror("AS needs to be given before neighbor definitions");
		return (-1);
	}

	/* set default values if they where undefined */
	p->conf.ebgp = (p->conf.remote_as != conf->as);
	if (p->conf.announce_type == ANNOUNCE_UNDEF)
		p->conf.announce_type = p->conf.ebgp == 0 ?
		    ANNOUNCE_ALL : ANNOUNCE_SELF;
	if (p->conf.enforce_as == ENFORCE_AS_UNDEF)
		p->conf.enforce_as = p->conf.ebgp == 0 ?
		    ENFORCE_AS_OFF : ENFORCE_AS_ON;

	/* EBGP neighbors are not allowed in route reflector clusters */
	if (p->conf.reflector_client && p->conf.ebgp) {
		yyerror("EBGP neighbors are not allowed in route "
		    "reflector clusters");
		return (-1);
	}

	/* the default MP capability is NONE */
	for (i = 0; i < AID_MAX; i++)
		if (p->conf.capabilities.mp[i] == -1)
			p->conf.capabilities.mp[i] = 0;

	return (0);
}

int
merge_filterset(struct filter_set_head *sh, struct filter_set *s)
{
	struct filter_set	*t;

	TAILQ_FOREACH(t, sh, entry) {
		/*
		 * need to cycle across the full list because even
		 * if types are not equal filterset_cmp() may return 0.
		 */
		if (filterset_cmp(s, t) == 0) {
			if (s->type == ACTION_SET_COMMUNITY)
				yyerror("community is already set");
			else if (s->type == ACTION_DEL_COMMUNITY)
				yyerror("community will already be deleted");
			else if (s->type == ACTION_SET_EXT_COMMUNITY)
				yyerror("ext-community is already set");
			else if (s->type == ACTION_DEL_EXT_COMMUNITY)
				yyerror(
				    "ext-community will already be deleted");
			else
				yyerror("redefining set parameter %s",
				    filterset_name(s->type));
			return (-1);
		}
	}

	TAILQ_FOREACH(t, sh, entry) {
		if (s->type < t->type) {
			TAILQ_INSERT_BEFORE(t, s, entry);
			return (0);
		}
		if (s->type == t->type)
			switch (s->type) {
			case ACTION_SET_COMMUNITY:
			case ACTION_DEL_COMMUNITY:
				if (s->action.community.as <
				    t->action.community.as ||
				    (s->action.community.as ==
				    t->action.community.as &&
				    s->action.community.type <
				    t->action.community.type)) {
					TAILQ_INSERT_BEFORE(t, s, entry);
					return (0);
				}
				break;
			case ACTION_SET_EXT_COMMUNITY:
			case ACTION_DEL_EXT_COMMUNITY:
				if (memcmp(&s->action.ext_community,
				    &t->action.ext_community,
				    sizeof(s->action.ext_community)) < 0) {
					TAILQ_INSERT_BEFORE(t, s, entry);
					return (0);
				}
				break;
			case ACTION_SET_NEXTHOP:
				if (s->action.nexthop.aid <
				    t->action.nexthop.aid) {
					TAILQ_INSERT_BEFORE(t, s, entry);
					return (0);
				}
				break;
			default:
				break;
			}
	}

	TAILQ_INSERT_TAIL(sh, s, entry);
	return (0);
}

void
copy_filterset(struct filter_set_head *source, struct filter_set_head *dest)
{
	struct filter_set	*s, *t;

	if (source == NULL)
		return;

	TAILQ_FOREACH(s, source, entry) {
		if ((t = calloc(1, sizeof(struct filter_set))) == NULL)
			fatal(NULL);
		memcpy(t, s, sizeof(struct filter_set));
		TAILQ_INSERT_TAIL(dest, t, entry);
	}
}

void
move_filterset(struct filter_set_head *source, struct filter_set_head *dest)
{
	struct filter_set	*s;

	TAILQ_INIT(dest);

	if (source == NULL)
		return;

	while ((s = TAILQ_FIRST(source)) != NULL) {
		TAILQ_REMOVE(source, s, entry);
		TAILQ_INSERT_TAIL(dest, s, entry);
	}
}

struct filter_rule *
get_rule(enum action_types type)
{
	struct filter_rule	*r;
	int			 out;

	switch (type) {
	case ACTION_SET_PREPEND_SELF:
	case ACTION_SET_NEXTHOP_NOMODIFY:
	case ACTION_SET_NEXTHOP_SELF:
		out = 1;
		break;
	default:
		out = 0;
		break;
	}
	r = (curpeer == curgroup) ? curgroup_filter[out] : curpeer_filter[out];
	if (r == NULL) {
		if ((r = calloc(1, sizeof(struct filter_rule))) == NULL)
			fatal(NULL);
		r->quick = 0;
		r->dir = out ? DIR_OUT : DIR_IN;
		r->action = ACTION_NONE;
		r->match.community.as = COMMUNITY_UNSET;
		TAILQ_INIT(&r->set);
		if (curpeer == curgroup) {
			/* group */
			r->peer.groupid = curgroup->conf.id;
			curgroup_filter[out] = r;
		} else {
			/* peer */
			r->peer.peerid = curpeer->conf.id;
			curpeer_filter[out] = r;
		}
	}
	return (r);
}

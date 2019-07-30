/*	$OpenBSD: parse.y,v 1.319 2018/02/10 01:24:28 benno Exp $ */

/*
 * Copyright (c) 2002, 2003, 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2001 Daniel Hartmeier.  All rights reserved.
 * Copyright (c) 2001 Theo de Raadt.  All rights reserved.
 * Copyright (c) 2016, 2017 Job Snijders <job@openbsd.org>
 * Copyright (c) 2016 Peter Hessler <phessler@openbsd.org>
 * Copyright (c) 2017, 2018 Sebastian Benoit <benno@openbsd.org>
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
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/ip_ipsp.h>
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

static struct bgpd_config	*conf;
static struct network_head	*netconf;
static struct peer		*peer_l, *peer_l_old;
static struct peer		*curpeer;
static struct peer		*curgroup;
static struct rdomain		*currdom;
static struct filter_head	*filter_l;
static struct filter_head	*peerfilter_l;
static struct filter_head	*groupfilter_l;
static struct filter_rule	*curpeer_filter[2];
static struct filter_rule	*curgroup_filter[2];
static u_int32_t		 id;

struct filter_rib_l {
	struct filter_rib_l	*next;
	char			 name[PEER_DESCR_LEN];
};

struct filter_peers_l {
	struct filter_peers_l	*next;
	struct filter_peers	 p;
};

struct filter_prefix_l {
	struct filter_prefix_l	*next;
	struct filter_prefix	 p;
};

struct filter_prefixlen {
	enum comp_ops		op;
	int			len_min;
	int			len_max;
};

struct filter_as_l {
	struct filter_as_l	*next;
	struct filter_as	 a;
};

struct filter_match_l {
	struct filter_match	 m;
	struct filter_prefix_l	*prefix_l;
	struct filter_as_l	*as_l;
	struct filter_prefixset	*prefixset;
} fmopts;

struct peer	*alloc_peer(void);
struct peer	*new_peer(void);
struct peer	*new_group(void);
int		 add_mrtconfig(enum mrt_type, char *, int, struct peer *,
		    char *);
int		 add_rib(char *, u_int, u_int16_t);
struct rde_rib	*find_rib(char *);
int		 get_id(struct peer *);
int		 merge_prefixspec(struct filter_prefix_l *,
		    struct filter_prefixlen *);
int		 expand_rule(struct filter_rule *, struct filter_rib_l *,
		    struct filter_peers_l *, struct filter_match_l *,
		    struct filter_set_head *);
int		 str2key(char *, char *, size_t);
int		 neighbor_consistent(struct peer *);
int		 merge_filterset(struct filter_set_head *, struct filter_set *);
void		 copy_filterset(struct filter_set_head *,
		    struct filter_set_head *);
void		 merge_filter_lists(struct filter_head *, struct filter_head *);
struct filter_rule	*get_rule(enum action_types);

int		 getcommunity(char *);
int		 parsecommunity(struct filter_community *, char *);
int64_t 	 getlargecommunity(char *);
int		 parselargecommunity(struct filter_largecommunity *, char *);
int		 parsesubtype(char *, int *, int *);
int		 parseextvalue(char *, u_int32_t *);
int		 parseextcommunity(struct filter_extcommunity *, char *,
		    char *);

typedef struct {
	union {
		int64_t			 number;
		char			*string;
		struct bgpd_addr	 addr;
		u_int8_t		 u8;
		struct filter_rib_l	*filter_rib;
		struct filter_peers_l	*filter_peers;
		struct filter_match_l	 filter_match;
		struct filter_prefixset	*filter_prefixset;
		struct filter_prefix_l	*filter_prefix;
		struct filter_as_l	*filter_as;
		struct filter_set	*filter_set;
		struct filter_set_head	*filter_set_head;
		struct {
			struct bgpd_addr	prefix;
			u_int8_t		len;
		}			prefix;
		struct filter_prefixlen	prefixlen;
		struct prefixset	*prefixset;
		struct {
			u_int8_t		enc_alg;
			char			enc_key[IPSEC_ENC_KEY_LEN];
			u_int8_t		enc_key_len;
		}			encspec;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	AS ROUTERID HOLDTIME YMIN LISTEN ON FIBUPDATE FIBPRIORITY RTABLE
%token	RDOMAIN RD EXPORTTRGT IMPORTTRGT
%token	RDE RIB EVALUATE IGNORE COMPARE
%token	GROUP NEIGHBOR NETWORK
%token	EBGP IBGP
%token	LOCALAS REMOTEAS DESCR LOCALADDR MULTIHOP PASSIVE MAXPREFIX RESTART
%token	ANNOUNCE CAPABILITIES REFRESH AS4BYTE CONNECTRETRY
%token	DEMOTE ENFORCE NEIGHBORAS REFLECTOR DEPEND DOWN
%token	DUMP IN OUT SOCKET RESTRICTED
%token	LOG ROUTECOLL TRANSPARENT
%token	TCP MD5SIG PASSWORD KEY TTLSECURITY
%token	ALLOW DENY MATCH
%token	QUICK
%token	FROM TO ANY
%token	CONNECTED STATIC
%token	COMMUNITY EXTCOMMUNITY LARGECOMMUNITY
%token	PREFIX PREFIXLEN PREFIXSET SOURCEAS TRANSITAS PEERAS DELETE MAXASLEN
%token	MAXASSEQ SET LOCALPREF MED METRIC NEXTHOP REJECT BLACKHOLE NOMODIFY SELF
%token	PREPEND_SELF PREPEND_PEER PFTABLE WEIGHT RTLABEL ORIGIN
%token	ERROR INCLUDE
%token	IPSEC ESP AH SPI IKE
%token	IPV4 IPV6
%token	QUALIFY VIA
%token	NE LE GE XRANGE LONGER
%token	<v.string>		STRING
%token	<v.number>		NUMBER
%type	<v.number>		asnumber as4number as4number_any optnumber
%type	<v.number>		espah family restart origincode nettype
%type	<v.number>		yesno inout restricted
%type	<v.string>		string
%type	<v.addr>		address
%type	<v.prefix>		prefix addrspec
%type	<v.prefixset>		prefixset
%type	<v.u8>			action quick direction delete
%type	<v.filter_rib>		filter_rib_h filter_rib_l filter_rib
%type	<v.filter_peers>	filter_peer filter_peer_l filter_peer_h
%type	<v.filter_match>	filter_match filter_elm filter_match_h
%type	<v.filter_as>		filter_as filter_as_l filter_as_h
%type	<v.filter_as>		filter_as_t filter_as_t_l filter_as_l_h
%type	<v.prefixlen>		prefixlenop
%type	<v.filter_set>		filter_set_opt
%type	<v.filter_set_head>	filter_set filter_set_l
%type	<v.filter_prefix>	filter_prefix filter_prefix_l filter_prefix_h
%type	<v.filter_prefix>	filter_prefix_m
%type	<v.u8>			unaryop equalityop binaryop filter_as_type
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
			 * According to iana 65535 and 4294967295 are reserved
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

as4number_any	: STRING			{
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
			$$ = uval | (uvalh << 16);
		}
		| asnumber {
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
			char *s = $1;
			if (cmd_opts & BGPD_OPT_VERBOSE)
				printf("%s = \"%s\"\n", $1, $3);
			while (*s++) {
				if (isspace((unsigned char)*s)) {
					yyerror("macro name cannot contain "
					    "whitespace");
					YYERROR;
				}
			}
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
			TAILQ_INSERT_TAIL(conf->listen_addrs, la, entry);
		}
		| FIBPRIORITY NUMBER		{
			if ($2 <= RTP_NONE || $2 > RTP_MAX) {
				yyerror("invalid fib-priority");
				YYERROR;
			}
			conf->fib_priority = $2;
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
			if (add_rib($3, conf->default_tableid, F_RIB_NOFIB)) {
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
			if (add_rib($3, conf->default_tableid,
			    F_RIB_NOFIB | F_RIB_NOEVALUATE)) {
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
		| prefixset		{
			SIMPLEQ_INSERT_TAIL(conf->prefixsets, $1, entry);
		}
		| DUMP STRING STRING optnumber		{
			int action;

			if ($4 < 0 || $4 > INT_MAX) {
				yyerror("bad timeout");
				free($2);
				free($3);
				YYERROR;
			}
			if (!strcmp($2, "table"))
				action = MRT_TABLE_DUMP;
			else if (!strcmp($2, "table-mp"))
				action = MRT_TABLE_DUMP_MP;
			else if (!strcmp($2, "table-v2"))
				action = MRT_TABLE_DUMP_V2;
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

			if ($6 < 0 || $6 > INT_MAX) {
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
			else if (!strcmp($4, "table-v2"))
				action = MRT_TABLE_DUMP_V2;
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
			if (strlen($2) >=
			    sizeof(((struct sockaddr_un *)0)->sun_path)) {
				yyerror("socket path too long");
				YYERROR;
			}
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

			if ($5 < 0 || $5 > INT_MAX) {
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

prefixset	: PREFIXSET STRING '{' filter_prefix_l '}'	{
			struct filter_prefix_l	*n, *p;
			struct prefixset_item	*pi;
			if (find_prefixset($2, conf->prefixsets) != NULL)  {
				yyerror("duplicate prefixset %s", $2);
				free($2);
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(*$$)))
			    == NULL)
				fatal("prefixset");
			if (strlcpy($$->name, $2,
			    sizeof($$->name)) >=
			    sizeof($$->name)) {
				yyerror("prefix-set \"%s\" too long: max %zu",
				    $2, sizeof($$->name) - 1);
				free($2);
				YYERROR;
			}
			SIMPLEQ_INIT(&$$->psitems);
			n = $4;
			while (n != NULL) {
				if ((pi = calloc(1, sizeof(*pi))) == NULL)
					fatal("prefixset item");
				pi->p = n->p;
				SIMPLEQ_INSERT_TAIL(&$$->psitems, pi, entry);
				p = n;
				n = n->next;
				free(p);
			}
		}

network		: NETWORK prefix filter_set	{
			struct network	*n, *m;

			if ((n = calloc(1, sizeof(struct network))) == NULL)
				fatal("new_network");
			memcpy(&n->net.prefix, &$2.prefix,
			    sizeof(n->net.prefix));
			n->net.prefixlen = $2.len;
			filterset_move($3, &n->net.attrset);
			free($3);
			TAILQ_FOREACH(m, netconf, entry) {
				if (n->net.prefixlen == m->net.prefixlen &&
				    prefix_compare(&n->net.prefix,
				    &m->net.prefix, n->net.prefixlen) == 0)
					yyerror("duplicate prefix "
					    "in network statement");
			}

			TAILQ_INSERT_TAIL(netconf, n, entry);
		}
		| NETWORK PREFIXSET STRING filter_set	{
			if ((find_prefixset($3, conf->prefixsets)) == NULL) {
				yyerror("prefix-set not defined");
				free($3);
				free($4);
				YYERROR;
			}
			/*
			 * XXX not implemented
			 */
			yyerror("network prefix-set not implemented.");
			free($3);
			free($4);
			YYERROR;
		}
		| NETWORK family RTLABEL STRING filter_set	{
			struct network	*n;

			if ((n = calloc(1, sizeof(struct network))) == NULL)
				fatal("new_network");
			if (afi2aid($2, SAFI_UNICAST, &n->net.prefix.aid) ==
			    -1) {
				yyerror("unknown family");
				filterset_free($5);
				free($5);
				YYERROR;
			}
			n->net.type = NETWORK_RTLABEL;
			n->net.rtlabel = rtlabel_name2id($4);
			filterset_move($5, &n->net.attrset);
			free($5);

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
			filterset_move($4, &n->net.attrset);
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
			SIMPLEQ_INSERT_TAIL(&conf->rdomains, currdom, entry);
			currdom = NULL;
			netconf = &conf->networks;
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
			case EXT_COMMUNITY_TRANS_TWO_AS:
				rd |= (0ULL << 48);
				break;
			case EXT_COMMUNITY_TRANS_IPV4:
				rd |= (1ULL << 48);
				break;
			case EXT_COMMUNITY_TRANS_FOUR_AS:
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
				yyerror("descr \"%s\" too long: max %zu",
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
				yyerror("group name \"%s\" too long: max %zu",
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
		;

peeropts	: REMOTEAS as4number	{
			curpeer->conf.remote_as = $2;
		}
		| LOCALAS as4number	{
			curpeer->conf.local_as = $2;
			if ($2 > USHRT_MAX)
				curpeer->conf.local_short_as = AS_TRANS;
			else
				curpeer->conf.local_short_as = $2;
		}
		| LOCALAS as4number asnumber {
			curpeer->conf.local_as = $2;
			curpeer->conf.local_short_as = $3;
		}
		| DESCR string		{
			if (strlcpy(curpeer->conf.descr, $2,
			    sizeof(curpeer->conf.descr)) >=
			    sizeof(curpeer->conf.descr)) {
				yyerror("descr \"%s\" too long: max %zu",
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
				yyerror("invalid multihop distance %lld", $2);
				YYERROR;
			}
			curpeer->conf.distance = $2;
		}
		| PASSIVE		{
			curpeer->conf.passive = 1;
		}
		| DOWN			{
			curpeer->conf.down = 1;
		}
		| DOWN STRING		{
			curpeer->conf.down = 1;
			if (strlcpy(curpeer->conf.shutcomm, $2,
				sizeof(curpeer->conf.shutcomm)) >=
				sizeof(curpeer->conf.shutcomm)) {
				    yyerror("shutdown reason too long");
				    free($2);
				    YYERROR;
			}
			free($2);
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
				yyerror("rib name \"%s\" too long: max %zu",
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
			curpeer->conf.capabilities.grestart.restart = $3;
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
		| ENFORCE LOCALAS yesno {
			if ($3)
				curpeer->conf.enforce_local_as = ENFORCE_AS_ON;
			else
				curpeer->conf.enforce_local_as = ENFORCE_AS_OFF;
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
				yyerror("tcp md5sig password too long: max %zu",
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
				    "is %zu bytes", keylen, strlen($7) / 2);
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

			if ($5 <= SPI_RESERVED_MAX || $5 > UINT_MAX) {
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
				    "max %zu", $3,
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
				    "max %zu", $2,
				    sizeof(curpeer->conf.demote_group) - 1);
				free($2);
				YYERROR;
			}
			free($2);
			if (carp_demote_init(curpeer->conf.demote_group,
			    cmd_opts & BGPD_OPT_FORCE_DEMOTE) == -1) {
				yyerror("error initializing group \"%s\"",
				    curpeer->conf.demote_group);
				YYERROR;
			}
		}
		| TRANSPARENT yesno	{
			if ($2 == 1)
				curpeer->conf.flags |= PEERFLAG_TRANS_AS;
			else
				curpeer->conf.flags &= ~PEERFLAG_TRANS_AS;
		}
		| LOG STRING		{
			if (!strcmp($2, "updates"))
				curpeer->conf.flags |= PEERFLAG_LOG_UPDATES;
			else if (!strcmp($2, "no"))
				curpeer->conf.flags &= ~PEERFLAG_LOG_UPDATES;
			else {
				free($2);
				YYERROR;
			}
			free($2);
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
				    "bytes, is %zu bytes",
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

filterrule	: action quick filter_rib_h direction filter_peer_h
				filter_match_h filter_set
		{
			struct filter_rule	 r;
			struct filter_rib_l	 *rb, *rbnext;

			bzero(&r, sizeof(r));
			r.action = $1;
			r.quick = $2;
			r.dir = $4;
			if ($3) {
				if (r.dir != DIR_IN) {
					yyerror("rib only allowed on \"from\" "
					    "rules.");

					for (rb = $3; rb != NULL; rb = rbnext) {
						rbnext = rb->next;
						free(rb);
					}
					YYERROR;
				}
			}
			if (expand_rule(&r, $3, $5, &$6, $7) == -1)
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

filter_rib_h	: /* empty */			{ $$ = NULL; }
		| RIB filter_rib		{ $$ = $2; }
		| RIB '{' filter_rib_l '}'	{ $$ = $3; }

filter_rib_l	: filter_rib			{ $$ = $1; }
		| filter_rib_l comma filter_rib	{
			$3->next = $1;
			$$ = $3;
		}
		;

filter_rib	: STRING	{
			if (!find_rib($1)) {
				yyerror("rib \"%s\" does not exist.", $1);
				free($1);
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(struct filter_rib_l))) ==
			    NULL)
				fatal(NULL);
			$$->next = NULL;
			if (strlcpy($$->name, $1, sizeof($$->name)) >=
			    sizeof($$->name)) {
				yyerror("rib name \"%s\" too long: "
				    "max %zu", $1, sizeof($$->name) - 1);
				free($1);
				free($$);
				YYERROR;
			}
			free($1);
		}
		;

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
			$$->p.remote_as = $$->p.groupid = $$->p.peerid = 0;
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
 		| AS as4number	{
			if (($$ = calloc(1, sizeof(struct filter_peers_l))) ==
			    NULL)
				fatal(NULL);
			$$->p.groupid = $$->p.peerid = 0;
			$$->p.remote_as = $2;
		}
		| GROUP STRING	{
			struct peer *p;

			if (($$ = calloc(1, sizeof(struct filter_peers_l))) ==
			    NULL)
				fatal(NULL);
			$$->p.remote_as = $$->p.peerid = 0;
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
		| EBGP {
			if (($$ = calloc(1, sizeof(struct filter_peers_l))) ==
			    NULL)
				fatal(NULL);
			$$->p.ebgp = 1;
		}
		| IBGP {
			if (($$ = calloc(1, sizeof(struct filter_peers_l))) ==
			    NULL)
				fatal(NULL);
			$$->p.ibgp = 1;
		}
		;

filter_prefix_h	: IPV4 prefixlenop			 {
			if ($2.op == OP_NONE)
				$2.op = OP_GE;
			if (($$ = calloc(1, sizeof(struct filter_prefix_l))) ==
			    NULL)
				fatal(NULL);
			$$->p.addr.aid = AID_INET;
			if (merge_prefixspec($$, &$2) == -1) {
				free($$);
				YYERROR;
			}
		}
		| IPV6 prefixlenop			{
			if ($2.op == OP_NONE)
				$2.op = OP_GE;
			if (($$ = calloc(1, sizeof(struct filter_prefix_l))) ==
			    NULL)
				fatal(NULL);
			$$->p.addr.aid = AID_INET6;
			if (merge_prefixspec($$, &$2) == -1) {
				free($$);
				YYERROR;
			}
		}
		| PREFIX filter_prefix			{ $$ = $2; }
		| PREFIX '{' filter_prefix_m '}'	{ $$ = $3; }
		;

filter_prefix_m	: filter_prefix_l
		| '{' filter_prefix_l '}'		{ $$ = $2; }
		| '{' filter_prefix_l '}' filter_prefix_m
		{
			struct filter_prefix_l  *p;

			/* merge, both can be lists */
			for (p = $2; p != NULL && p->next != NULL; p = p->next)
				;       /* nothing */
			if (p != NULL)
				p->next = $4;
			$$ = $2;
		}

filter_prefix_l	: filter_prefix			{ $$ = $1; }
		| filter_prefix_l comma filter_prefix	{
			$3->next = $1;
			$$ = $3;
		}
		;

filter_prefix	: prefix prefixlenop			{
			if (($$ = calloc(1, sizeof(struct filter_prefix_l))) ==
			    NULL)
				fatal(NULL);
			memcpy(&$$->p.addr, &$1.prefix,
			    sizeof($$->p.addr));
			$$->p.len = $1.len;

			if (merge_prefixspec($$, &$2) == -1) {
				free($$);
				YYERROR;
			}
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

filter_as	: as4number_any		{
			if (($$ = calloc(1, sizeof(struct filter_as_l))) ==
			    NULL)
				fatal(NULL);
			$$->a.as = $1;
			$$->a.op = OP_EQ;
		}
		| NEIGHBORAS		{
			if (($$ = calloc(1, sizeof(struct filter_as_l))) ==
			    NULL)
				fatal(NULL);
			$$->a.flags = AS_FLAG_NEIGHBORAS;
		}
		| equalityop as4number_any	{
			if (($$ = calloc(1, sizeof(struct filter_as_l))) ==
			    NULL)
				fatal(NULL);
			$$->a.op = $1;
			$$->a.as = $2;
		}
		| as4number_any binaryop as4number_any {
			if (($$ = calloc(1, sizeof(struct filter_as_l))) ==
			    NULL)
				fatal(NULL);
			if ($1 >= $3) {
				yyerror("start AS is bigger than end");
				YYERROR;
			}
			$$->a.op = $2;
			$$->a.as_min = $1;
			$$->a.as_max = $3;
		}
		;

filter_match_h	: /* empty */			{
			bzero(&$$, sizeof($$));
			$$.m.community.as = COMMUNITY_UNSET;
			$$.m.large_community.as = COMMUNITY_UNSET;
		}
		| {
			bzero(&fmopts, sizeof(fmopts));
			fmopts.m.community.as = COMMUNITY_UNSET;
			fmopts.m.large_community.as = COMMUNITY_UNSET;
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
			if (fmopts.m.prefixset.name[0] != '\0') {
				yyerror("\"prefix-set\" already specified, "
				    "cannot be used with \"prefix\" in the "
				    "same filter rule");
				YYERROR;
			}
			fmopts.prefix_l = $1;
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
		| LARGECOMMUNITY STRING	{
			if (fmopts.m.large_community.as != COMMUNITY_UNSET) {
				yyerror("\"large-community\" already specified");
				free($2);
				YYERROR;
			}
			if (parselargecommunity(&fmopts.m.large_community, $2) == -1) {
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
		| NEXTHOP address 	{
			if (fmopts.m.nexthop.flags) {
				yyerror("nexthop already specified");
				YYERROR;
			}
			fmopts.m.nexthop.addr = $2;
			fmopts.m.nexthop.flags = FILTER_NEXTHOP_ADDR;
		}
		| NEXTHOP NEIGHBOR 	{
			if (fmopts.m.nexthop.flags) {
				yyerror("nexthop already specified");
				YYERROR;
			}
			fmopts.m.nexthop.flags = FILTER_NEXTHOP_NEIGHBOR;
		}
		| PREFIXSET STRING	{
			if (fmopts.prefix_l != NULL) {
				yyerror("\"prefix\" already specified, cannot "
				    "be used with \"prefix-set\" in the same "
				    "filter rule");
				free($2);
				YYERROR;
			}
			if (fmopts.m.prefixset.name[0] != '\0') {
				yyerror("prefix-set filters already specified");
				free($2);
				YYERROR;
			}
			if ((find_prefixset($2, conf->prefixsets)) == NULL) {
				yyerror("prefix-set not defined");
				free($2);
				YYERROR;
			}
			if (strlcpy(fmopts.m.prefixset.name, $2,
			    sizeof(fmopts.m.prefixset.name)) >=
			    sizeof(fmopts.m.prefixset.name)) {
				yyerror("prefix-set name too long");
				free($2);
				YYERROR;
			}
			fmopts.m.prefixset.flags |= PREFIXSET_FLAG_FILTER;
			fmopts.m.prefixset.ps = NULL;
			free($2);
		}
		;

prefixlenop	: /* empty */			{ bzero(&$$, sizeof($$)); }
		| LONGER				{
			bzero(&$$, sizeof($$));
			$$.op = OP_GE;
			$$.len_min = -1;
		}
		| PREFIXLEN unaryop NUMBER		{
			bzero(&$$, sizeof($$));
			if ($3 < 0 || $3 > 128) {
				yyerror("prefixlen must be >= 0 and <= 128");
				YYERROR;
			}
			if ($2 == OP_GT && $3 == 0) {
				yyerror("prefixlen must be > 0");
				YYERROR;
			}
			$$.op = $2;
			$$.len_min = $3;
		}
		| PREFIXLEN NUMBER binaryop NUMBER	{
			bzero(&$$, sizeof($$));
			if ($2 < 0 || $2 > 128 || $4 < 0 || $4 > 128) {
				yyerror("prefixlen must be < 128");
				YYERROR;
			}
			if ($2 >= $4) {
				yyerror("start prefixlen is bigger than end");
				YYERROR;
			}
			$$.op = $3;
			$$.len_min = $2;
			$$.len_max = $4;
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
			if ($2 >= 0) {
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
			if ($2 >= 0) {
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
			if ($2 >= 0) {
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
			if (!(cmd_opts & BGPD_OPT_NOACTION) &&
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
		| LARGECOMMUNITY delete STRING	{
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			if ($2)
				$$->type = ACTION_DEL_LARGE_COMMUNITY;
			else
				$$->type = ACTION_SET_LARGE_COMMUNITY;

			if (parselargecommunity(&$$->action.large_community,
			    $3) == -1) {
				free($3);
				free($$);
				YYERROR;
			}
			free($3);
			/* Don't allow setting of any match */
			if (!$2 &&
			    ($$->action.large_community.as == COMMUNITY_ANY ||
			    $$->action.large_community.ld1 == COMMUNITY_ANY ||
			    $$->action.large_community.ld2 == COMMUNITY_ANY)) {
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

equalityop	: '='		{ $$ = OP_EQ; }
		| NE		{ $$ = OP_NE; }
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
		{ "ebgp",		EBGP},
		{ "enforce",		ENFORCE},
		{ "esp",		ESP},
		{ "evaluate",		EVALUATE},
		{ "export-target",	EXPORTTRGT},
		{ "ext-community",	EXTCOMMUNITY},
		{ "fib-priority",	FIBPRIORITY},
		{ "fib-update",		FIBUPDATE},
		{ "from",		FROM},
		{ "group",		GROUP},
		{ "holdtime",		HOLDTIME},
		{ "ibgp",		IBGP},
		{ "ignore",		IGNORE},
		{ "ike",		IKE},
		{ "import-target",	IMPORTTRGT},
		{ "in",			IN},
		{ "include",		INCLUDE},
		{ "inet",		IPV4},
		{ "inet6",		IPV6},
		{ "ipsec",		IPSEC},
		{ "key",		KEY},
		{ "large-community",	LARGECOMMUNITY},
		{ "listen",		LISTEN},
		{ "local-address",	LOCALADDR},
		{ "local-as",		LOCALAS},
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
		{ "or-longer",		LONGER},
		{ "origin",		ORIGIN},
		{ "out",		OUT},
		{ "passive",		PASSIVE},
		{ "password",		PASSWORD},
		{ "peer-as",		PEERAS},
		{ "pftable",		PFTABLE},
		{ "prefix",		PREFIX},
		{ "prefix-set",		PREFIXSET},
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
parse_config(char *filename, struct bgpd_config *xconf, struct peer **xpeers)
{
	struct sym		*sym, *next;
	struct peer		*p, *pnext;
	struct rde_rib		*rr;
	int			 errors = 0;

	conf = new_config();

	if ((filter_l = calloc(1, sizeof(struct filter_head))) == NULL)
		fatal(NULL);
	if ((peerfilter_l = calloc(1, sizeof(struct filter_head))) == NULL)
		fatal(NULL);
	if ((groupfilter_l = calloc(1, sizeof(struct filter_head))) == NULL)
		fatal(NULL);
	TAILQ_INIT(filter_l);
	TAILQ_INIT(peerfilter_l);
	TAILQ_INIT(groupfilter_l);

	peer_l = NULL;
	peer_l_old = *xpeers;
	curpeer = NULL;
	curgroup = NULL;
	id = 1;

	netconf = &conf->networks;

	/* the Adj-RIB-In/Out have no fib so no need to set the tableid */
	add_rib("Adj-RIB-In", 0, F_RIB_NOFIB | F_RIB_NOEVALUATE);
	add_rib("Adj-RIB-Out", 0, F_RIB_NOFIB | F_RIB_NOEVALUATE);
	add_rib("Loc-RIB", conf->default_tableid, F_RIB_LOCAL);

	if ((file = pushfile(filename, 1)) == NULL) {
		free(conf);
		return (-1);
	}
	topfile = file;

	yyparse();
	errors = file->errors;
	popfile();

	/* Free macros and check which have not been used. */
	TAILQ_FOREACH_SAFE(sym, &symhead, entry, next) {
		if ((cmd_opts & BGPD_OPT_VERBOSE2) && !sym->used)
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
		for (p = peer_l; p != NULL; p = pnext) {
			pnext = p->next;
			free(p);
		}

		while ((rr = SIMPLEQ_FIRST(&ribnames)) != NULL) {
			SIMPLEQ_REMOVE_HEAD(&ribnames, entry);
			free(rr);
		}

		filterlist_free(filter_l);
		filterlist_free(peerfilter_l);
		filterlist_free(groupfilter_l);

		free_config(conf);
	} else {
		/*
		 * Move filter list and static group and peer filtersets
		 * together. Static group sets come first then peer sets
		 * last normal filter rules.
		 */
		merge_filter_lists(conf->filters, groupfilter_l);
		merge_filter_lists(conf->filters, peerfilter_l);
		merge_filter_lists(conf->filters, filter_l);

		errors += mrt_mergeconfig(xconf->mrt, conf->mrt);
		errors += merge_config(xconf, conf, peer_l);
		*xpeers = peer_l;

		for (p = peer_l_old; p != NULL; p = pnext) {
			pnext = p->next;
			free(p);
		}

		free(filter_l);
		free(peerfilter_l);
		free(groupfilter_l);
	}

	return (errors ? -1 : 0);
}

int
symset(const char *nam, const char *val, int persist)
{
	struct sym	*sym;

	TAILQ_FOREACH(sym, &symhead, entry) {
		if (strcmp(nam, sym->nam) == 0)
			break;
	}

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

	TAILQ_FOREACH(sym, &symhead, entry) {
		if (strcmp(nam, sym->nam) == 0) {
			sym->used = 1;
			return (sym->val);
		}
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
	if (strcmp(s, "local-as") == 0)
		return (COMMUNITY_LOCAL_AS);
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
	if (strcasecmp(s, "GRACEFUL_SHUTDOWN") == 0) {
		c->as = COMMUNITY_WELLKNOWN;
		c->type = COMMUNITY_GRACEFUL_SHUTDOWN;
		return (0);
	} else if (strcasecmp(s, "NO_EXPORT") == 0) {
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
	} else if (strcasecmp(s, "BLACKHOLE") == 0) {
		c->as = COMMUNITY_WELLKNOWN;
		c->type = COMMUNITY_BLACKHOLE;
		return (0);
	}

	if ((p = strchr(s, ':')) == NULL) {
		yyerror("Bad community syntax");
		return (-1);
	}
	*p++ = 0;

	if ((i = getcommunity(s)) == COMMUNITY_ERROR)
		return (-1);
	as = i;

	if ((i = getcommunity(p)) == COMMUNITY_ERROR)
		return (-1);
	c->as = as;
	c->type = i;

	return (0);
}

int64_t
getlargecommunity(char *s)
{
	u_int		 val;
	const char	*errstr;

	if (strcmp(s, "*") == 0)
		return (COMMUNITY_ANY);
	if (strcmp(s, "neighbor-as") == 0)
		return (COMMUNITY_NEIGHBOR_AS);
	if (strcmp(s, "local-as") == 0)
		return (COMMUNITY_LOCAL_AS);
	val = strtonum(s, 0, UINT_MAX, &errstr);
	if (errstr) {
		yyerror("Large Community %s is %s (max: %u)",
		    s, errstr, UINT_MAX);
		return (COMMUNITY_ERROR);
	}
	return (val);
}

int
parselargecommunity(struct filter_largecommunity *c, char *s)
{
	char *p, *q;
	int64_t as, ld1, ld2;

	if ((p = strchr(s, ':')) == NULL) {
		yyerror("Bad community syntax");
		return (-1);
	}
	*p++ = 0;

	if ((q = strchr(p, ':')) == NULL) {
		yyerror("Bad community syntax");
		return (-1);
	}
	*q++ = 0;

	if ((as = getlargecommunity(s)) == COMMUNITY_ERROR)
		return (-1);

	if ((ld1 = getlargecommunity(p)) == COMMUNITY_ERROR)
		return (-1);

	if ((ld2 = getlargecommunity(q)) == COMMUNITY_ERROR)
		return (-1);

	c->as = as;
	c->ld1 = ld1;
	c->ld2 = ld2;

	return (0);
}

int
parsesubtype(char *name, int *type, int *subtype)
{
	const struct ext_comm_pairs *cp;
	int found = 0;

	for (cp = iana_ext_comms; cp->subname != NULL; cp++) {
		if (strcmp(name, cp->subname) == 0) {
			if (found == 0) {
				*type = cp->type;
				*subtype = cp->subtype;
			}
			found++;
		}
	}
	if (found > 1)
		*type = -1;
	return (found);
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
		if (uval <= USHRT_MAX)
			return (EXT_COMMUNITY_TRANS_TWO_AS);
		else
			return (EXT_COMMUNITY_TRANS_FOUR_AS);
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
		return (EXT_COMMUNITY_TRANS_FOUR_AS);
	} else {
		/* more than one dot -> IP address */
		if (inet_aton(s, &ip) == 0) {
			yyerror("Bad ext-community %s not parseable", s);
			return (-1);
		}
		*v = ip.s_addr;
		return (EXT_COMMUNITY_TRANS_IPV4);
	}
	return (-1);
}

int
parseextcommunity(struct filter_extcommunity *c, char *t, char *s)
{
	const struct ext_comm_pairs *cp;
	const char 	*errstr;
	u_int64_t	 ullval;
	u_int32_t	 uval;
	char		*p, *ep;
	int		 type, subtype;

	if (parsesubtype(t, &type, &subtype) == 0) {
		yyerror("Bad ext-community unknown type");
		return (-1);
	}

	switch (type) {
	case -1:
		if ((p = strchr(s, ':')) == NULL) {
			yyerror("Bad ext-community %s", s);
			return (-1);
		}
		*p++ = '\0';
		if ((type = parseextvalue(s, &uval)) == -1)
			return (-1);
		switch (type) {
		case EXT_COMMUNITY_TRANS_TWO_AS:
			ullval = strtonum(p, 0, UINT_MAX, &errstr);
			break;
		case EXT_COMMUNITY_TRANS_IPV4:
		case EXT_COMMUNITY_TRANS_FOUR_AS:
			ullval = strtonum(p, 0, USHRT_MAX, &errstr);
			break;
		default:
			fatalx("parseextcommunity: unexpected result");
		}
		if (errstr) {
			yyerror("Bad ext-community %s is %s", p, errstr);
			return (-1);
		}
		switch (type) {
		case EXT_COMMUNITY_TRANS_TWO_AS:
			c->data.ext_as.as = uval;
			c->data.ext_as.val = ullval;
			break;
		case EXT_COMMUNITY_TRANS_IPV4:
			c->data.ext_ip.addr.s_addr = uval;
			c->data.ext_ip.val = ullval;
			break;
		case EXT_COMMUNITY_TRANS_FOUR_AS:
			c->data.ext_as4.as4 = uval;
			c->data.ext_as4.val = ullval;
			break;
		}
		break;
	case EXT_COMMUNITY_TRANS_OPAQUE:
	case EXT_COMMUNITY_TRANS_EVPN:
		errno = 0;
		ullval = strtoull(s, &ep, 0);
		if (s[0] == '\0' || *ep != '\0') {
			yyerror("Bad ext-community bad value");
			return (-1);
		}
		if (errno == ERANGE && ullval > EXT_COMMUNITY_OPAQUE_MAX) {
			yyerror("Bad ext-community value too big");
			return (-1);
		}
		c->data.ext_opaq = ullval;
		break;
	case EXT_COMMUNITY_NON_TRANS_OPAQUE:
		if (strcmp(s, "valid") == 0)
			c->data.ext_opaq = EXT_COMMUNITY_OVS_VALID;
		else if (strcmp(s, "invalid") == 0)
			c->data.ext_opaq = EXT_COMMUNITY_OVS_INVALID;
		else if (strcmp(s, "not-found") == 0)
			c->data.ext_opaq = EXT_COMMUNITY_OVS_NOTFOUND;
		else {
			yyerror("Bad ext-community %s", s);
			return (-1);
		}
		break;
	}
	c->type = type;
	c->subtype = subtype;

	/* verify type/subtype combo */
	for (cp = iana_ext_comms; cp->subname != NULL; cp++) {
		if (cp->type == type && cp->subtype == subtype) {
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
	p->conf.capabilities.grestart.restart = 1;
	p->conf.capabilities.as4byte = 1;
	p->conf.local_as = conf->as;
	p->conf.local_short_as = conf->short_as;

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
add_mrtconfig(enum mrt_type type, char *name, int timeout, struct peer *p,
    char *rib)
{
	struct mrt	*m, *n;

	LIST_FOREACH(m, conf->mrt, entry) {
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
		yyerror("filename \"%s\" too long: max %zu",
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
			yyerror("rib name \"%s\" too long: max %zu",
			    name, sizeof(n->rib) - 1);
			free(n);
			return (-1);
		}
	}

	LIST_INSERT_HEAD(conf->mrt, n, entry);

	return (0);
}

int
add_rib(char *name, u_int rtableid, u_int16_t flags)
{
	struct rde_rib	*rr;
	u_int		 rdom, default_rdom;

	if ((rr = find_rib(name)) == NULL) {
		if ((rr = calloc(1, sizeof(*rr))) == NULL) {
			log_warn("add_rib");
			return (-1);
		}
	}
	if (strlcpy(rr->name, name, sizeof(rr->name)) >= sizeof(rr->name)) {
		yyerror("rib name \"%s\" too long: max %zu",
		   name, sizeof(rr->name) - 1);
		free(rr);
		return (-1);
	}
	rr->flags |= flags;
	if ((rr->flags & F_RIB_HASNOFIB) == 0) {
		if (ktable_exists(rtableid, &rdom) != 1) {
			yyerror("rtable id %u does not exist", rtableid);
			free(rr);
			return (-1);
		}
		if (ktable_exists(conf->default_tableid, &default_rdom) != 1)
			fatal("default rtable %u does not exist",
			    conf->default_tableid);
		if (rdom != default_rdom) {
			log_warnx("rtable %u does not belong to rdomain %u",
			    rtableid, default_rdom);
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

struct prefixset *
find_prefixset(char *name, struct prefixset_head *p)
{
	struct prefixset *ps;

	SIMPLEQ_FOREACH(ps, p, entry) {
		if (!strcmp(ps->name, name))
			return (ps);
	}
	return (NULL);
}

/* returns the prefixset_item from psitems that matches i */
struct prefixset_item *
find_prefixsetitem(struct prefixset_item *i, struct prefixset_items_h *psitems)
{
	struct prefixset_item *psi;

	SIMPLEQ_FOREACH(psi, psitems, entry) {
		if (memcmp(&i->p, &psi->p, sizeof(psi->p)) == 0)
			return(psi);
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
merge_prefixspec(struct filter_prefix_l *p, struct filter_prefixlen *pl)
{
	u_int8_t max_len = 0;

	switch (p->p.addr.aid) {
	case AID_INET:
	case AID_VPN_IPv4:
		max_len = 32;
		break;
	case AID_INET6:
		max_len = 128;
		break;
	}

	switch (pl->op) {
	case OP_NONE:
		return (0);
	case OP_RANGE:
	case OP_XRANGE:
		if (pl->len_min > max_len || pl->len_max > max_len) {
			yyerror("prefixlen %d too big for AF, limit %d",
			    pl->len_min > max_len ? pl->len_min : pl->len_max,
			    max_len);
			return (-1);
		}
		if (pl->len_min < p->p.len) {
			yyerror("prefixlen %d smaller than prefix, limit %d",
			    pl->len_min, p->p.len);
			return (-1);
		}
		p->p.len_max = pl->len_max;
		break;
	case OP_GE:
		/* fix up the "or-longer" case */
		if (pl->len_min == -1)
			pl->len_min = p->p.len;
		/* FALLTHROUGH */
	case OP_EQ:
	case OP_NE:
	case OP_LE:
	case OP_GT:
		if (pl->len_min > max_len) {
			yyerror("prefixlen %d too big for AF, limit %d",
			    pl->len_min, max_len);
			return (-1);
		}
		if (pl->len_min < p->p.len) {
			yyerror("prefixlen %d smaller than prefix, limit %d",
			    pl->len_min, p->p.len);
			return (-1);
		}
		break;
	case OP_LT:
		if (pl->len_min > max_len - 1) {
			yyerror("prefixlen %d too big for AF, limit %d",
			    pl->len_min, max_len - 1);
			return (-1);
		}
		if (pl->len_min < p->p.len + 1) {
			yyerror("prefixlen %d too small for prefix, limit %d",
			    pl->len_min, p->p.len + 1);
			return (-1);
		}
		break;
	}

	p->p.op = pl->op;
	p->p.len_min = pl->len_min;
	return (0);
}

int
expand_rule(struct filter_rule *rule, struct filter_rib_l *rib,
    struct filter_peers_l *peer, struct filter_match_l *match,
    struct filter_set_head *set)
{
	struct filter_rule	*r;
	struct filter_rib_l	*rb, *rbnext;
	struct filter_peers_l	*p, *pnext;
	struct filter_prefix_l	*prefix, *prefix_next;
	struct filter_as_l	*a, *anext;
	struct filter_set	*s;

	rb = rib;
	do {
		p = peer;
		do {
			a = match->as_l;
			do {
				prefix = match->prefix_l;
				do {
					if ((r = calloc(1,
					    sizeof(struct filter_rule))) ==
						 NULL) {
						log_warn("expand_rule");
						return (-1);
					}

					memcpy(r, rule, sizeof(struct filter_rule));
					memcpy(&r->match, match,
					    sizeof(struct filter_match));
					TAILQ_INIT(&r->set);
					copy_filterset(set, &r->set);

					if (rb != NULL)
						strlcpy(r->rib, rb->name,
						     sizeof(r->rib));

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

					if (prefix != NULL)
						prefix = prefix->next;
				} while (prefix != NULL);

				if (a != NULL)
					a = a->next;
			} while (a != NULL);

			if (p != NULL)
				p = p->next;
		} while (p != NULL);

		if (rb != NULL)
			rb = rb->next;
	} while (rb != NULL);

	for (rb = rib; rb != NULL; rb = rbnext) {
		rbnext = rb->next;
		free(rb);
	}

	for (p = peer; p != NULL; p = pnext) {
		pnext = p->next;
		free(p);
	}

	for (a = match->as_l; a != NULL; a = anext) {
		anext = a->next;
		free(a);
	}

	for (prefix = match->prefix_l; prefix != NULL; prefix = prefix_next) {
		prefix_next = prefix->next;
		free(prefix);
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
		p->conf.announce_type = p->conf.ebgp ?
		    ANNOUNCE_SELF : ANNOUNCE_ALL;
	if (p->conf.enforce_as == ENFORCE_AS_UNDEF)
		p->conf.enforce_as = p->conf.ebgp ?
		    ENFORCE_AS_ON : ENFORCE_AS_OFF;
	if (p->conf.enforce_local_as == ENFORCE_AS_UNDEF)
		p->conf.enforce_local_as = ENFORCE_AS_ON;

	if (p->conf.remote_as == 0 && p->conf.enforce_as != ENFORCE_AS_OFF) {
		yyerror("peer AS may not be zero");
		return (-1);
	}

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
			else if (s->type == ACTION_SET_LARGE_COMMUNITY)
				yyerror("large-community is already set");
			else if (s->type == ACTION_DEL_LARGE_COMMUNITY)
				yyerror("large-community will already be deleted");
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
			case ACTION_SET_LARGE_COMMUNITY:
			case ACTION_DEL_LARGE_COMMUNITY:
				if (s->action.large_community.as <
				    t->action.large_community.as ||
				    (s->action.large_community.as ==
				    t->action.large_community.as &&
				    s->action.large_community.ld1 <
				    t->action.large_community.ld2 )) {
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
		if ((t = malloc(sizeof(struct filter_set))) == NULL)
			fatal(NULL);
		memcpy(t, s, sizeof(struct filter_set));
		TAILQ_INSERT_TAIL(dest, t, entry);
	}
}

void
merge_filter_lists(struct filter_head *dst, struct filter_head *src)
{
	struct filter_rule *r;

	while ((r = TAILQ_FIRST(src)) != NULL) {
		TAILQ_REMOVE(src, r, entry);
		TAILQ_INSERT_TAIL(dst, r, entry);
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
		r->match.large_community.as = COMMUNITY_UNSET;
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

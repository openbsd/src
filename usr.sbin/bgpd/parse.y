/*	$OpenBSD: parse.y,v 1.368 2018/12/30 13:53:07 denis Exp $ */

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
	size_t	 		 ungetpos;
	size_t			 ungetsize;
	u_char			*ungetbuf;
	int			 eof_reached;
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
int		 igetc(void);
int		 lgetc(int);
void		 lungetc(int);
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
static struct prefixset		*curpset, *curoset;
static struct prefixset_tree	*curpsitree;
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
int		 merge_prefixspec(struct filter_prefix *,
		    struct filter_prefixlen *);
int		 expand_rule(struct filter_rule *, struct filter_rib_l *,
		    struct filter_peers_l *, struct filter_match_l *,
		    struct filter_set_head *);
int		 str2key(char *, char *, size_t);
int		 neighbor_consistent(struct peer *);
int		 merge_filterset(struct filter_set_head *, struct filter_set *);
void		 optimize_filters(struct filter_head *);
struct filter_rule	*get_rule(enum action_types);

int		 parsecommunity(struct filter_community *, int, char *);
int		 parsesubtype(char *, int *, int *);
int		 parseextcommunity(struct filter_community *, char *,
		    char *);
static int	 new_as_set(char *);
static void	 add_as_set(u_int32_t);
static void	 done_as_set(void);
static struct prefixset	*new_prefix_set(char *, int);
static void	 add_roa_set(struct prefixset_item *, u_int32_t, u_int8_t);

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
		struct prefixset_item	*prefixset_item;
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
%token	RDOMAIN RD EXPORT EXPORTTRGT IMPORTTRGT
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
%token	COMMUNITY EXTCOMMUNITY LARGECOMMUNITY DELETE
%token	PREFIX PREFIXLEN PREFIXSET
%token	ROASET ORIGINSET OVS
%token	ASSET SOURCEAS TRANSITAS PEERAS MAXASLEN MAXASSEQ
%token	SET LOCALPREF MED METRIC NEXTHOP REJECT BLACKHOLE NOMODIFY SELF
%token	PREPEND_SELF PREPEND_PEER PFTABLE WEIGHT RTLABEL ORIGIN PRIORITY
%token	ERROR INCLUDE
%token	IPSEC ESP AH SPI IKE
%token	IPV4 IPV6
%token	QUALIFY VIA
%token	NE LE GE XRANGE LONGER MAXLEN
%token	<v.string>		STRING
%token	<v.number>		NUMBER
%type	<v.number>		asnumber as4number as4number_any optnumber
%type	<v.number>		espah family restart origincode nettype
%type	<v.number>		yesno inout restricted validity
%type	<v.string>		string
%type	<v.addr>		address
%type	<v.prefix>		prefix addrspec
%type	<v.prefixset_item>	prefixset_item
%type	<v.u8>			action quick direction delete community
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
		| grammar varset '\n'
		| grammar include '\n'
		| grammar as_set '\n'
		| grammar prefixset '\n'
		| grammar roa_set '\n'
		| grammar origin_set '\n'
		| grammar conf_main '\n'
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
			if (uvalh == 0 && (uval == AS_TRANS || uval == 0)) {
				yyerror("AS %u is reserved and may not be used",
				    uval);
				YYERROR;
			}
			$$ = uval | (uvalh << 16);
		}
		| asnumber {
			if ($1 == AS_TRANS || $1 == 0) {
				yyerror("AS %u is reserved and may not be used",
				    (u_int32_t)$1);
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
					free($1);
					free($3);
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

as_set		: ASSET STRING '{' optnl	{
			if (new_as_set($2) != 0) {
				free($2);
				YYERROR;
			}
			free($2);
		} as_set_l optnl '}' {
			done_as_set();
		}
		| ASSET STRING '{' optnl '}'	{
			if (new_as_set($2) != 0) {
				free($2);
				YYERROR;
			}
			free($2);
		}

as_set_l	: as4number_any			{ add_as_set($1); }
		| as_set_l comma as4number_any	{ add_as_set($3); }

prefixset	: PREFIXSET STRING '{' optnl		{
			if ((curpset = new_prefix_set($2, 0)) == NULL) {
				free($2);
				YYERROR;
			}
			free($2);
		} prefixset_l optnl '}'			{
			SIMPLEQ_INSERT_TAIL(&conf->prefixsets, curpset, entry);
			curpset = NULL;
		}
		| PREFIXSET STRING '{' optnl '}'	{
			if ((curpset = new_prefix_set($2, 0)) == NULL) {
				free($2);
				YYERROR;
			}
			free($2);
			SIMPLEQ_INSERT_TAIL(&conf->prefixsets, curpset, entry);
			curpset = NULL;
		}

prefixset_l	: prefixset_item			{
			struct prefixset_item	*psi;
			if ($1->p.op != OP_NONE)
				curpset->sflags |= PREFIXSET_FLAG_OPS;
			psi = RB_INSERT(prefixset_tree, &curpset->psitems, $1);
			if (psi != NULL) {
				if (cmd_opts & BGPD_OPT_VERBOSE2)
					log_warnx("warning: duplicate entry in "
					    "prefixset \"%s\" for %s/%u",
					    curpset->name,
					    log_addr(&$1->p.addr), $1->p.len);
				free($1);
			}
		}
		| prefixset_l comma prefixset_item	{
			struct prefixset_item	*psi;
			if ($3->p.op != OP_NONE)
				curpset->sflags |= PREFIXSET_FLAG_OPS;
			psi = RB_INSERT(prefixset_tree, &curpset->psitems, $3);
			if (psi != NULL) {
				if (cmd_opts & BGPD_OPT_VERBOSE2)
					log_warnx("warning: duplicate entry in "
					    "prefixset \"%s\" for %s/%u",
					    curpset->name,
					    log_addr(&$3->p.addr), $3->p.len);
				free($3);
			}
		}
		;

prefixset_item	: prefix prefixlenop			{
			if ($2.op != OP_NONE && $2.op != OP_RANGE) {
				yyerror("unsupported prefixlen operation in "
				    "prefix-set");
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(*$$))) == NULL)
				fatal(NULL);
			memcpy(&$$->p.addr, &$1.prefix, sizeof($$->p.addr));
			$$->p.len = $1.len;
			if (merge_prefixspec(&$$->p, &$2) == -1) {
				free($$);
				YYERROR;
			}
		}
		;

roa_set		: ROASET '{' optnl		{
			curpsitree = &conf->roa;
		} roa_set_l optnl '}'			{
			curpsitree = NULL;
		}
		| ROASET '{' optnl '}'		/* nothing */
		;

origin_set	: ORIGINSET STRING '{' optnl		{
			if ((curoset = new_prefix_set($2, 1)) == NULL) {
				free($2);
				YYERROR;
			}
			curpsitree = &curoset->psitems;
			free($2);
		} roa_set_l optnl '}'			{
			SIMPLEQ_INSERT_TAIL(&conf->originsets, curoset, entry);
			curoset = NULL;
			curpsitree = NULL;
		}
		| ORIGINSET STRING '{' optnl '}'		{
			if ((curoset = new_prefix_set($2, 1)) == NULL) {
				free($2);
				YYERROR;
			}
			free($2);
			SIMPLEQ_INSERT_TAIL(&conf->originsets, curoset, entry);
			curoset = NULL;
			curpsitree = NULL;
		}
		;

roa_set_l	: prefixset_item SOURCEAS as4number_any			{
			if ($1->p.len_min != $1->p.len) {
				yyerror("unsupported prefixlen operation in "
				    "roa-set");
				free($1);
				YYERROR;
			}
			add_roa_set($1, $3, $1->p.len_max);
		}
		| roa_set_l comma prefixset_item SOURCEAS as4number_any	{
			if ($3->p.len_min != $3->p.len) {
				yyerror("unsupported prefixlen operation in "
				    "roa-set");
				free($3);
				YYERROR;
			}
			add_roa_set($3, $5, $3->p.len_max);
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
			if ($5 > RT_TABLEID_MAX) {
				yyerror("rtable %llu too big: max %u", $5,
				    RT_TABLEID_MAX);
				YYERROR;
			}
			if (add_rib($3, $5, 0)) {
				free($3);
				YYERROR;
			}
			free($3);
		}
		| RDE RIB STRING RTABLE NUMBER FIBUPDATE yesno {
			int	flags = 0;
			if ($5 > RT_TABLEID_MAX) {
				yyerror("rtable %llu too big: max %u", $5,
				    RT_TABLEID_MAX);
				YYERROR;
			}
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
			if ($2 > RT_TABLEID_MAX) {
				yyerror("rtable %llu too big: max %u", $2,
				    RT_TABLEID_MAX);
				YYERROR;
			}
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
			struct prefixset *ps;
			struct network	*n;
			if ((ps = find_prefixset($3, &conf->prefixsets))
			    == NULL) {
				yyerror("prefix-set '%s' not defined", $3);
				free($3);
				filterset_free($4);
				free($4);
				YYERROR;
			}
			if (ps->sflags & PREFIXSET_FLAG_OPS) {
				yyerror("prefix-set %s has prefixlen operators "
				    "and cannot be used in network statements.",
				    ps->name);
				free($3);
				filterset_free($4);
				free($4);
				YYERROR;
			}
			if ((n = calloc(1, sizeof(struct network))) == NULL)
				fatal("new_network");
			strlcpy(n->net.psname, ps->name, sizeof(n->net.psname));
			filterset_move($4, &n->net.attrset);
			n->net.type = NETWORK_PREFIXSET;
			TAILQ_INSERT_TAIL(netconf, n, entry);
			free($3);
			free($4);
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
		| NETWORK family PRIORITY NUMBER filter_set	{
			struct network	*n;
			if ($4 < RTP_LOCAL && $4 > RTP_MAX) {
				yyerror("priority %lld > max %d or < min %d", $4,
				    RTP_MAX, RTP_LOCAL);
				YYERROR;
			}

			if ((n = calloc(1, sizeof(struct network))) == NULL)
				fatal("new_network");
			if (afi2aid($2, SAFI_UNICAST, &n->net.prefix.aid) ==
			    -1) {
				yyerror("unknown family");
				filterset_free($5);
				free($5);
				YYERROR;
			}
			n->net.type = NETWORK_PRIORITY;
			n->net.priority = $4;
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

optnumber	: /* empty */		{ $$ = 0; }
		| NUMBER
		;

rdomain		: RDOMAIN NUMBER			{
			if ($2 > RT_TABLEID_MAX) {
				yyerror("rtable %llu too big: max %u", $2,
				    RT_TABLEID_MAX);
				YYERROR;
			}
			if ((cmd_opts & BGPD_OPT_NOACTION) == 0 &&
			    ktable_exists($2, NULL) != 1) {
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
		} '{' rdomainopts_l '}'	{
			/* insert into list */
			SIMPLEQ_INSERT_TAIL(&conf->rdomains, currdom, entry);
			currdom = NULL;
			netconf = &conf->networks;
		}
		;

rdomainopts_l	: /* empty */
		| rdomainopts_l '\n'
		| rdomainopts_l rdomainopts '\n'
		| rdomainopts_l error '\n'
		;

rdomainopts	: RD STRING {
			struct filter_community	ext;
			u_int64_t		rd;

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
			switch (ext.c.e.type) {
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
			set->type = ACTION_SET_COMMUNITY;
			if (parseextcommunity(&set->action.community,
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
			set->type = ACTION_SET_COMMUNITY;
			if (parseextcommunity(&set->action.community,
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
			if ((cmd_opts & BGPD_OPT_NOACTION) == 0 &&
			    if_nametoindex($3) == 0) {
				yyerror("interface %s does not exist", $3);
				free($3);
				YYERROR;
			}
			strlcpy(currdom->ifmpe, $3, IFNAMSIZ);
			free($3);
			/* XXX this is in the wrong place */
			if ((cmd_opts & BGPD_OPT_NOACTION) == 0 &&
			    get_mpe_label(currdom)) {
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

group		: GROUP string 			{
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
		} '{' groupopts_l '}'		{
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

groupopts_l	: /* empty */
		| groupopts_l '\n'
		| groupopts_l peeropts '\n'
		| groupopts_l neighbor '\n'
		| groupopts_l error '\n'
		;

peeropts_h	: '{' '\n' peeropts_l '}'
		| '{' peeropts '}'
		| /* empty */
		;

peeropts_l	: /* empty */
		| peeropts_l '\n'
		| peeropts_l peeropts '\n'
		| peeropts_l error '\n'
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
			yyerror("support for the 'announce self' directive has"
			    " been removed. Urgent configuration review "
			    "required!");
			YYERROR;
		}
		| ANNOUNCE STRING {
			if (!strcmp($2, "all"))
				logit(LOG_ERR, "%s:%d: %s", file->name,
				    yylval.lineno,
				    "warning: 'announce all' is deprecated");
			else if (!strcmp($2, "none")) {
				logit(LOG_ERR, "%s:%d: %s", file->name,
				    yylval.lineno,
				    "warning: 'announce none' is deprecated, "
				    "use 'export none' instead");
				curpeer->conf.export_type = EXPORT_NONE;
			} else if (!strcmp($2, "default-route")) {
				logit(LOG_ERR, "%s:%d: %s", file->name,
				    yylval.lineno,
				    "warning: 'announce default-route' is "
				    "deprecated, use 'export default-route' "
				    "instead");
				curpeer->conf.export_type =
				    EXPORT_DEFAULT_ROUTE;
			} else {
				yyerror("syntax error: unknown '%s'", $2);
				free($2);
				YYERROR;
			}
			free($2);
		}
		| EXPORT STRING {
			if (!strcmp($2, "none"))
				curpeer->conf.export_type = EXPORT_NONE;
			else if (!strcmp($2, "default-route"))
				curpeer->conf.export_type =
				    EXPORT_DEFAULT_ROUTE;
			else {
				yyerror("invalid export type");
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
		| SET "{" optnl filter_set_l optnl "}"	{
			struct filter_rule	*r;
			struct filter_set	*s;

			while ((s = TAILQ_FIRST($4)) != NULL) {
				TAILQ_REMOVE($4, s, entry);
				r = get_rule(s->type);
				if (merge_filterset(&r->set, s) == -1)
					YYERROR;
			}
			free($4);
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
		| RIB '{' optnl filter_rib_l optnl '}'	{ $$ = $4; }

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
		| '{' optnl filter_peer_l optnl '}'	{ $$ = $3; }
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
			if ($2.op == OP_NONE) {
				$2.op = OP_RANGE;
				$2.len_min = 0;
				$2.len_max = -1;
			}
			if (($$ = calloc(1, sizeof(struct filter_prefix_l))) ==
			    NULL)
				fatal(NULL);
			$$->p.addr.aid = AID_INET;
			if (merge_prefixspec(&$$->p, &$2) == -1) {
				free($$);
				YYERROR;
			}
		}
		| IPV6 prefixlenop			{
			if ($2.op == OP_NONE) {
				$2.op = OP_RANGE;
				$2.len_min = 0;
				$2.len_max = -1;
			}
			if (($$ = calloc(1, sizeof(struct filter_prefix_l))) ==
			    NULL)
				fatal(NULL);
			$$->p.addr.aid = AID_INET6;
			if (merge_prefixspec(&$$->p, &$2) == -1) {
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

			if (merge_prefixspec(&$$->p, &$2) == -1) {
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
		| filter_as_type ASSET STRING {
			if (as_sets_lookup(conf->as_sets, $3) == NULL) {
				yyerror("as-set \"%s\" not defined", $3);
				free($3);
				YYERROR;
			}
			if (($$ = calloc(1, sizeof(struct filter_as_l))) ==
			    NULL)
				fatal(NULL);
			$$->a.type = $1;
			$$->a.flags = AS_FLAG_AS_SET_NAME;
			if (strlcpy($$->a.name, $3, sizeof($$->a.name)) >=
			    sizeof($$->a.name)) {
				yyerror("as-set name \"%s\" too long: "
				    "max %zu", $3, sizeof($$->a.name) - 1);
				free($3);
				free($$);
				YYERROR;
			}
			free($3);
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
			$$->a.as_min = $1;
			$$->a.as_max = $1;
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
			$$->a.as_min = $2;
			$$->a.as_max = $2;
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
		}
		| {
			bzero(&fmopts, sizeof(fmopts));
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
		| community STRING	{
			int i;
			for (i = 0; i < MAX_COMM_MATCH; i++) {
				if (fmopts.m.community[i].type ==
				    COMMUNITY_TYPE_NONE)
					break;
			}
			if (i >= MAX_COMM_MATCH) {
				yyerror("too many \"community\" filters "
				    "specified");
				free($2);
				YYERROR;
			}
			if (parsecommunity(&fmopts.m.community[i], $1, $2) == -1) {
				free($2);
				YYERROR;
			}
			free($2);
		}
		| EXTCOMMUNITY STRING STRING {
			int i;
			for (i = 0; i < MAX_COMM_MATCH; i++) {
				if (fmopts.m.community[i].type ==
				    COMMUNITY_TYPE_NONE)
					break;
			}
			if (i >= MAX_COMM_MATCH) {
				yyerror("too many \"community\" filters "
				    "specified");
				free($2);
				free($3);
				YYERROR;
			}
			if (parseextcommunity(&fmopts.m.community[i],
			    $2, $3) == -1) {
				free($2);
				free($3);
				YYERROR;
			}
			free($2);
			free($3);
		}
		| EXTCOMMUNITY OVS STRING {
			int i;
			for (i = 0; i < MAX_COMM_MATCH; i++) {
				if (fmopts.m.community[i].type ==
				    COMMUNITY_TYPE_NONE)
					break;
			}
			if (i >= MAX_COMM_MATCH) {
				yyerror("too many \"community\" filters "
				    "specified");
				free($3);
				YYERROR;
			}
			if (parseextcommunity(&fmopts.m.community[i],
			    "ovs", $3) == -1) {
				free($3);
				YYERROR;
			}
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
		| PREFIXSET STRING prefixlenop {
			struct prefixset *ps;
			if (fmopts.prefix_l != NULL) {
				yyerror("\"prefix\" already specified, cannot "
				    "be used with \"prefix-set\" in the same "
				    "filter rule");
				free($2);
				YYERROR;
			}
			if (fmopts.m.prefixset.name[0] != '\0') {
				yyerror("prefix-set filter already specified");
				free($2);
				YYERROR;
			}
			if ((ps = find_prefixset($2, &conf->prefixsets))
			    == NULL) {
				yyerror("prefix-set '%s' not defined", $2);
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
			if (!($3.op == OP_NONE ||
			    ($3.op == OP_RANGE &&
			     $3.len_min == -1 && $3.len_max == -1))) {
				yyerror("prefix-sets can only use option "
				    "or-longer");
				free($2);
				YYERROR;
			}
			if ($3.op == OP_RANGE && ps->sflags & PREFIXSET_FLAG_OPS) {
				yyerror("prefix-set %s contains prefixlen "
				    "operators and cannot be used with an "
				    "or-longer filter", $2);
				free($2);
				YYERROR;
			}
			if ($3.op == OP_RANGE && $3.len_min == -1 &&
			    $3.len_min == -1)
				fmopts.m.prefixset.flags |=
				    PREFIXSET_FLAG_LONGER;
			fmopts.m.prefixset.flags |= PREFIXSET_FLAG_FILTER;
			free($2);
		}
		| ORIGINSET STRING {
			if (fmopts.m.originset.name[0] != '\0') {
				yyerror("origin-set filter already specified");
				free($2);
				YYERROR;
			}
			if (find_prefixset($2, &conf->originsets) == NULL) {
				yyerror("origin-set '%s' not defined", $2);
				free($2);
				YYERROR;
			}
			if (strlcpy(fmopts.m.originset.name, $2,
			    sizeof(fmopts.m.originset.name)) >=
			    sizeof(fmopts.m.originset.name)) {
				yyerror("origin-set name too long");
				free($2);
				YYERROR;
			}
			free($2);
		}
		| OVS validity		{
			if (fmopts.m.ovs.is_set) {
				yyerror("ovs filter already specified");
				YYERROR;
			}
			fmopts.m.ovs.validity = $2;
			fmopts.m.ovs.is_set = 1;
		}
		;

prefixlenop	: /* empty */			{ bzero(&$$, sizeof($$)); }
		| LONGER				{
			bzero(&$$, sizeof($$));
			$$.op = OP_RANGE;
			$$.len_min = -1;
			$$.len_max = -1;
		}
		| MAXLEN NUMBER				{
			bzero(&$$, sizeof($$));
			if ($2 < 0 || $2 > 128) {
				yyerror("prefixlen must be >= 0 and <= 128");
				YYERROR;
			}

			$$.op = OP_RANGE;
			$$.len_min = -1;
			$$.len_max = $2;
		}
		| PREFIXLEN unaryop NUMBER		{
			int min, max;

			bzero(&$$, sizeof($$));
			if ($3 < 0 || $3 > 128) {
				yyerror("prefixlen must be >= 0 and <= 128");
				YYERROR;
			}
			/*
			 * convert the unary operation into the equivalent
			 * range check
			 */
			$$.op = OP_RANGE;

			switch ($2) {
			case OP_NE:
				$$.op = $2;
			case OP_EQ:
				min = max = $3;
				break;
			case OP_LT:
				if ($3 == 0) {
					yyerror("prefixlen must be > 0");
					YYERROR;
				}
				$3 -= 1;
			case OP_LE:
				min = -1;
				max = $3;
				break;
			case OP_GT:
				$3 += 1;
			case OP_GE:
				min = $3;
				max = -1;
				break;
			default:
				yyerror("unknown prefixlen operation");
				YYERROR;
			}
			$$.len_min = min;
			$$.len_max = max;
		}
		| PREFIXLEN NUMBER binaryop NUMBER	{
			bzero(&$$, sizeof($$));
			if ($2 < 0 || $2 > 128 || $4 < 0 || $4 > 128) {
				yyerror("prefixlen must be < 128");
				YYERROR;
			}
			if ($2 > $4) {
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
		| SET "{" optnl filter_set_l optnl "}"	{ $$ = $4; }
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

community	: COMMUNITY		{ $$ = COMMUNITY_TYPE_BASIC; }
		| LARGECOMMUNITY	{ $$ = COMMUNITY_TYPE_LARGE; }
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
		| community delete STRING	{
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			if ($2)
				$$->type = ACTION_DEL_COMMUNITY;
			else
				$$->type = ACTION_SET_COMMUNITY;

			if (parsecommunity(&$$->action.community, $1, $3) ==
			    -1) {
				free($3);
				free($$);
				YYERROR;
			}
			free($3);
			/* Don't allow setting of any match */
			if (!$2 &&
			    ($$->action.community.dflag1 == COMMUNITY_ANY ||
			    $$->action.community.dflag2 == COMMUNITY_ANY ||
			    $$->action.community.dflag3 == COMMUNITY_ANY)) {
				yyerror("'*' is not allowed in set community");
				free($$);
				YYERROR;
			}
		}
		| EXTCOMMUNITY delete STRING STRING {
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			if ($2)
				$$->type = ACTION_DEL_COMMUNITY;
			else
				$$->type = ACTION_SET_COMMUNITY;

			if (parseextcommunity(&$$->action.community,
			    $3, $4) == -1) {
				free($3);
				free($4);
				free($$);
				YYERROR;
			}
			free($3);
			free($4);
		}
		| EXTCOMMUNITY delete OVS STRING {
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			if ($2)
				$$->type = ACTION_DEL_COMMUNITY;
			else
				$$->type = ACTION_SET_COMMUNITY;

			if (parseextcommunity(&$$->action.community,
			    "ovs", $4) == -1) {
				free($4);
				free($$);
				YYERROR;
			}
			free($4);
		}
		| ORIGIN origincode {
			if (($$ = calloc(1, sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			$$->type = ACTION_SET_ORIGIN;
			$$->action.origin = $2;
		}
		;

origincode	: STRING	{
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

validity	: STRING	{
			if (!strcmp($1, "not-found"))
				$$ = ROA_NOTFOUND;
			else if (!strcmp($1, "invalid"))
				$$ = ROA_INVALID;
			else if (!strcmp($1, "valid"))
				$$ = ROA_VALID;
			else {
				yyerror("unknown validity \"%s\"", $1);
				free($1);
				YYERROR;
			}
			free($1);
		};

optnl		: /* empty */
		| '\n' optnl
		;

comma		: /* empty */
		| ','
		| '\n' optnl
		| ',' '\n' optnl
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
		{ "as-set",		ASSET },
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
		{ "export",		EXPORT},
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
		{ "maxlen",		MAXLEN},
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
		{ "origin-set",		ORIGINSET},
		{ "out",		OUT},
		{ "ovs",		OVS},
		{ "passive",		PASSIVE},
		{ "password",		PASSWORD},
		{ "peer-as",		PEERAS},
		{ "pftable",		PFTABLE},
		{ "prefix",		PREFIX},
		{ "prefix-set",		PREFIXSET},
		{ "prefixlen",		PREFIXLEN},
		{ "prepend-neighbor",	PREPEND_PEER},
		{ "prepend-self",	PREPEND_SELF},
		{ "priority",		PRIORITY},
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
		{ "roa-set",		ROASET },
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

#define START_EXPAND	1
#define DONE_EXPAND	2

static int	expanding;

int
igetc(void)
{
	int	c;

	while (1) {
		if (file->ungetpos > 0)
			c = file->ungetbuf[--file->ungetpos];
		else
			c = getc(file->stream);

		if (c == START_EXPAND)
			expanding = 1;
		else if (c == DONE_EXPAND)
			expanding = 0;
		else
			break;
	}
	return (c);
}

int
lgetc(int quotec)
{
	int		c, next;

	if (quotec) {
		if ((c = igetc()) == EOF) {
			yyerror("reached end of file while parsing "
			    "quoted string");
			if (file == topfile || popfile() == EOF)
				return (EOF);
			return (quotec);
		}
		return (c);
	}

	while ((c = igetc()) == '\\') {
		next = igetc();
		if (next != '\n') {
			c = next;
			break;
		}
		yylval.lineno = file->lineno;
		file->lineno++;
	}

	if (c == EOF) {
		/*
		 * Fake EOL when hit EOF for the first time. This gets line
		 * count right if last line in included file is syntactically
		 * invalid and has no newline.
		 */
		if (file->eof_reached == 0) {
			file->eof_reached = 1;
			return ('\n');
		}
		while (c == EOF) {
			if (file == topfile || popfile() == EOF)
				return (EOF);
			c = igetc();
		}
	}
	return (c);
}

void
lungetc(int c)
{
	if (c == EOF)
		return;

	if (file->ungetpos >= file->ungetsize) {
		void *p = reallocarray(file->ungetbuf, file->ungetsize, 2);
		if (p == NULL)
			err(1, "lungetc");
		file->ungetbuf = p;
		file->ungetsize *= 2;
	}
	file->ungetbuf[file->ungetpos++] = c;
}

int
findeol(void)
{
	int	c;

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
	if (c == '$' && !expanding) {
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
		p = val + strlen(val) - 1;
		lungetc(DONE_EXPAND);
		while (p >= val) {
			lungetc(*p);
			p--;
		}
		lungetc(START_EXPAND);
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
				if (next == quotec || next == ' ' ||
				    next == '\t')
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
				yyerror("syntax error: unterminated quote");
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
		log_warn("%s", __func__);
		return (NULL);
	}
	if ((nfile->name = strdup(name)) == NULL) {
		log_warn("%s", __func__);
		free(nfile);
		return (NULL);
	}
	if ((nfile->stream = fopen(nfile->name, "r")) == NULL) {
		log_warn("%s: %s", __func__, nfile->name);
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
	nfile->lineno = TAILQ_EMPTY(&files) ? 1 : 0;
	nfile->ungetsize = 16;
	nfile->ungetbuf = malloc(nfile->ungetsize);
	if (nfile->ungetbuf == NULL) {
		log_warn("%s", __func__);
		fclose(nfile->stream);
		free(nfile->name);
		free(nfile);
		return (NULL);
	}
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
	free(file->ungetbuf);
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
	struct network	       	*n;
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

	add_rib("Adj-RIB-In", conf->default_tableid,
	    F_RIB_NOFIB | F_RIB_NOEVALUATE);
	add_rib("Adj-RIB-Out", conf->default_tableid,
	    F_RIB_NOFIB | F_RIB_NOEVALUATE);
	add_rib("Loc-RIB", conf->default_tableid, F_RIB_LOCAL);

	if ((file = pushfile(filename, 1)) == NULL) {
		free(conf);
		return (-1);
	}
	topfile = file;

	yyparse();
	errors = file->errors;
	popfile();

	/* check that we dont try to announce our own routes */
	TAILQ_FOREACH(n, netconf, entry)
	    if (n->net.priority == conf->fib_priority) {
		    errors++;
		    logit(LOG_CRIT, "network priority %d == fib-priority "
			"%d is not allowed.",
			n->net.priority, conf->fib_priority);
	    }
	
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
		 * Concatenate filter list and static group and peer filtersets
		 * together. Static group sets come first then peer sets
		 * last normal filter rules.
		 */
		TAILQ_CONCAT(conf->filters, groupfilter_l, entry);
		TAILQ_CONCAT(conf->filters, peerfilter_l, entry);
		TAILQ_CONCAT(conf->filters, filter_l, entry);

		optimize_filters(conf->filters);

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

	if ((val = strrchr(s, '=')) == NULL)
		return (-1);
	sym = strndup(s, val - s);
	if (sym == NULL)
		fatal("%s: strndup", __func__);
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

static int
getcommunity(char *s, int large, u_int32_t *val, u_int8_t *flag)
{
	int64_t		 max = USHRT_MAX;
	const char	*errstr;

	*flag = 0;
	*val = 0;
	if (strcmp(s, "*") == 0) {
		*flag = COMMUNITY_ANY;
		return 0;
	} else if (strcmp(s, "neighbor-as") == 0) {
		*flag = COMMUNITY_NEIGHBOR_AS;
		return 0;
	} else if (strcmp(s, "local-as") == 0) {
		*flag =  COMMUNITY_LOCAL_AS;
		return 0;
	}
	if (large)
		max = UINT_MAX;
	*val = strtonum(s, 0, max, &errstr);
	if (errstr) {
		yyerror("Community %s is %s (max: %llu)", s, errstr, max);
		return -1;
	}
	return 0;
}

static void
setcommunity(struct filter_community *c, u_int32_t as, u_int32_t data,
    u_int8_t asflag, u_int8_t dataflag)
{
	memset(c, 0, sizeof(*c));
	c->type = COMMUNITY_TYPE_BASIC;
	c->dflag1 = asflag;
	c->dflag2 = dataflag;
	c->c.b.data1 = as;
	c->c.b.data2 = data;
}

static int
parselargecommunity(struct filter_community *c, char *s)
{
	char *p, *q;

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

	if (getcommunity(s, 1, &c->c.l.data1, &c->dflag1) == -1 ||
	    getcommunity(p, 1, &c->c.l.data2, &c->dflag2) == -1 ||
	    getcommunity(q, 1, &c->c.l.data3, &c->dflag3) == -1)
		return (-1);
	c->type = COMMUNITY_TYPE_LARGE;
	return (0);
}

int
parsecommunity(struct filter_community *c, int type, char *s)
{
	char *p;
	u_int32_t as, data;
	u_int8_t asflag, dataflag;

	if (type == COMMUNITY_TYPE_LARGE)
		return parselargecommunity(c, s);

	/* Well-known communities */
	if (strcasecmp(s, "GRACEFUL_SHUTDOWN") == 0) {
		setcommunity(c, COMMUNITY_WELLKNOWN,
		    COMMUNITY_GRACEFUL_SHUTDOWN, 0, 0);
		return (0);
	} else if (strcasecmp(s, "NO_EXPORT") == 0) {
		setcommunity(c, COMMUNITY_WELLKNOWN,
		    COMMUNITY_NO_EXPORT, 0, 0);
		return (0);
	} else if (strcasecmp(s, "NO_ADVERTISE") == 0) {
		setcommunity(c, COMMUNITY_WELLKNOWN,
		    COMMUNITY_NO_ADVERTISE, 0, 0);
		return (0);
	} else if (strcasecmp(s, "NO_EXPORT_SUBCONFED") == 0) {
		setcommunity(c, COMMUNITY_WELLKNOWN,
		    COMMUNITY_NO_EXPSUBCONFED, 0, 0);
		return (0);
	} else if (strcasecmp(s, "NO_PEER") == 0) {
		setcommunity(c, COMMUNITY_WELLKNOWN,
		    COMMUNITY_NO_PEER, 0, 0);
		return (0);
	} else if (strcasecmp(s, "BLACKHOLE") == 0) {
		setcommunity(c, COMMUNITY_WELLKNOWN,
		    COMMUNITY_BLACKHOLE, 0, 0);
		return (0);
	}

	if ((p = strchr(s, ':')) == NULL) {
		yyerror("Bad community syntax");
		return (-1);
	}
	*p++ = 0;

	if (getcommunity(s, 0, &as, &asflag) == -1 ||
	    getcommunity(p, 0, &data, &dataflag) == -1)
		return (-1);
	setcommunity(c, as, data, asflag, dataflag);
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

static int
parseextvalue(int type, char *s, u_int32_t *v)
{
	const char 	*errstr;
	char		*p;
	struct in_addr	 ip;
	u_int32_t	 uvalh, uval;

	if (type != -1) {
		/* nothing */
	} else if ((p = strchr(s, '.')) == NULL) {
		/* AS_PLAIN number (4 or 2 byte) */
		strtonum(s, 0, USHRT_MAX, &errstr);
		if (errstr == NULL)
			type = EXT_COMMUNITY_TRANS_TWO_AS;
		else
			type = EXT_COMMUNITY_TRANS_FOUR_AS;
	} else if (strchr(p + 1, '.') == NULL) {
		/* AS_DOT number (4-byte) */
		type = EXT_COMMUNITY_TRANS_FOUR_AS;
	} else {
		/* more than one dot -> IP address */
		type = EXT_COMMUNITY_TRANS_IPV4;
	}

	switch (type) {
	case EXT_COMMUNITY_TRANS_TWO_AS:
		uval = strtonum(s, 0, USHRT_MAX, &errstr);
		if (errstr) {
			yyerror("Bad ext-community %s is %s", s, errstr);
			return (-1);
		}
		*v = uval;
		break;
	case EXT_COMMUNITY_TRANS_FOUR_AS:
		if ((p = strchr(s, '.')) == NULL) {
			uval = strtonum(s, 0, UINT_MAX, &errstr);
			if (errstr) {
				yyerror("Bad ext-community %s is %s", s,
				    errstr);
				return (-1);
			}
			*v = uval;
			break;
		}
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
		break;
	case EXT_COMMUNITY_TRANS_IPV4:
		if (inet_aton(s, &ip) == 0) {
			yyerror("Bad ext-community %s not parseable", s);
			return (-1);
		}
		*v = ntohl(ip.s_addr);
		break;
	default:
		fatalx("%s: unexpected type %d", __func__, type);
	}
	return (type);
}

int
parseextcommunity(struct filter_community *c, char *t, char *s)
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
	case EXT_COMMUNITY_TRANS_TWO_AS:
	case EXT_COMMUNITY_TRANS_FOUR_AS:
	case EXT_COMMUNITY_TRANS_IPV4:
	case -1:
		if ((p = strchr(s, ':')) == NULL) {
			yyerror("Bad ext-community %s", s);
			return (-1);
		}
		*p++ = '\0';
		if ((type = parseextvalue(type, s, &uval)) == -1)
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
		c->c.e.data1 = uval;
		c->c.e.data2 = ullval;
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
		c->c.e.data2 = ullval;
		break;
	case EXT_COMMUNITY_NON_TRANS_OPAQUE:
		if (strcmp(s, "valid") == 0)
			c->c.e.data2 = EXT_COMMUNITY_OVS_VALID;
		else if (strcmp(s, "invalid") == 0)
			c->c.e.data2 = EXT_COMMUNITY_OVS_INVALID;
		else if (strcmp(s, "not-found") == 0)
			c->c.e.data2 = EXT_COMMUNITY_OVS_NOTFOUND;
		else {
			yyerror("Bad ext-community %s", s);
			return (-1);
		}
		break;
	}
	c->c.e.type = type;
	c->c.e.subtype = subtype;

	/* verify type/subtype combo */
	for (cp = iana_ext_comms; cp->subname != NULL; cp++) {
		if (cp->type == type && cp->subtype == subtype) {
			c->type = COMMUNITY_TYPE_EXT;
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
	p->conf.export_type = EXPORT_UNSET;
	p->conf.announce_capa = 1;
	for (i = 0; i < AID_MAX; i++)
		p->conf.capabilities.mp[i] = 0;
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
	}
	rr->rtableid = rtableid;
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
merge_prefixspec(struct filter_prefix *p, struct filter_prefixlen *pl)
{
	u_int8_t max_len = 0;

	switch (p->addr.aid) {
	case AID_INET:
	case AID_VPN_IPv4:
		max_len = 32;
		break;
	case AID_INET6:
	case AID_VPN_IPv6:
		max_len = 128;
		break;
	}

	if (pl->op == OP_NONE) {
		p->len_min = p->len_max = p->len;
		return (0);
	}

	if (pl->len_min == -1)
		pl->len_min = p->len;
	if (pl->len_max == -1)
		pl->len_max = max_len;

	if (pl->len_max > max_len) {
		yyerror("prefixlen %d too big, limit %d",
		    pl->len_max, max_len);
		return (-1);
	}
	if (pl->len_min > pl->len_max) {
		yyerror("prefixlen %d too big, limit %d",
		    pl->len_min, pl->len_max);
		return (-1);
	}
	if (pl->len_min < p->len) {
		yyerror("prefixlen %d smaller than prefix, limit %d",
		    pl->len_min, p->len);
		return (-1);
	}

	p->op = pl->op;
	p->len_min = pl->len_min;
	p->len_max = pl->len_max;
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

	return (0);
}

static void
filterset_add(struct filter_set_head *sh, struct filter_set *s)
{
	struct filter_set	*t;

	TAILQ_FOREACH(t, sh, entry) {
		if (s->type < t->type) {
			TAILQ_INSERT_BEFORE(t, s, entry);
			return;
		}
		if (s->type == t->type) {
			switch (s->type) {
			case ACTION_SET_COMMUNITY:
			case ACTION_DEL_COMMUNITY:
				if (memcmp(&s->action.community,
				    &t->action.community,
				    sizeof(s->action.community)) < 0) {
					TAILQ_INSERT_BEFORE(t, s, entry);
					return;
				} else if (memcmp(&s->action.community,
				    &t->action.community,
				    sizeof(s->action.community)) == 0)
					break;
				continue;
			case ACTION_SET_NEXTHOP:
				/* only last nexthop per AF matters */
				if (s->action.nexthop.aid <
				    t->action.nexthop.aid) {
					TAILQ_INSERT_BEFORE(t, s, entry);
					return;
				} else if (s->action.nexthop.aid ==
				    t->action.nexthop.aid) {
					t->action.nexthop = s->action.nexthop;
					break;
				}
				continue;
			case ACTION_SET_NEXTHOP_BLACKHOLE:
			case ACTION_SET_NEXTHOP_REJECT:
			case ACTION_SET_NEXTHOP_NOMODIFY:
			case ACTION_SET_NEXTHOP_SELF:
				/* set it only once */
				break;
			case ACTION_SET_LOCALPREF:
			case ACTION_SET_MED:
			case ACTION_SET_WEIGHT:
				/* only last set matters */
				t->action.metric = s->action.metric;
				break;
			case ACTION_SET_RELATIVE_LOCALPREF:
			case ACTION_SET_RELATIVE_MED:
			case ACTION_SET_RELATIVE_WEIGHT:
				/* sum all relative numbers */
				t->action.relative += s->action.relative;
				break;
			case ACTION_SET_ORIGIN:
				/* only last set matters */
				t->action.origin = s->action.origin;
				break;
			case ACTION_PFTABLE:
				/* only last set matters */
				strlcpy(t->action.pftable, s->action.pftable,
				    sizeof(t->action.pftable));
				break;
			case ACTION_RTLABEL:
				/* only last set matters */
				strlcpy(t->action.rtlabel, s->action.rtlabel,
				    sizeof(t->action.rtlabel));
				break;
			default:
				break;
			}
			free(s);
			return;
		}
	}

	TAILQ_INSERT_TAIL(sh, s, entry);
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
			else
				yyerror("redefining set parameter %s",
				    filterset_name(s->type));
			return (-1);
		}
	}

	filterset_add(sh, s);
	return (0);
}

static int
filter_equal(struct filter_rule *fa, struct filter_rule *fb)
{
	if (fa == NULL || fb == NULL)
		return 0;
	if (fa->action != fb->action || fa->quick != fb->quick ||
	    fa->dir != fb->dir)
		return 0;
	if (memcmp(&fa->peer, &fb->peer, sizeof(fa->peer)))
		return 0;
	if (memcmp(&fa->match, &fb->match, sizeof(fa->match)))
		return 0;

	return 1;
}

/* do a basic optimization by folding equal rules together */
void
optimize_filters(struct filter_head *fh)
{
	struct filter_rule *r, *nr;

	TAILQ_FOREACH_SAFE(r, fh, entry, nr) {
		while (filter_equal(r, nr)) {
			struct filter_set	*t;

			while((t = TAILQ_FIRST(&nr->set)) != NULL) {
				TAILQ_REMOVE(&nr->set, t, entry);
				filterset_add(&r->set, t);
			}

			TAILQ_REMOVE(fh, nr, entry);
			free(nr);
			nr = TAILQ_NEXT(r, entry);
		}
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

struct set_table *curset;
static int
new_as_set(char *name)
{
	struct as_set *aset;

	if (as_sets_lookup(conf->as_sets, name) != NULL) {
		yyerror("as-set \"%s\" already exists", name);
		return -1;
	}

	aset = as_sets_new(conf->as_sets, name, 0, sizeof(u_int32_t));
	if (aset == NULL)
		fatal(NULL);

	curset = aset->set;
	return 0;
}

static void
add_as_set(u_int32_t as)
{
	if (curset == NULL)
		fatalx("%s: bad mojo jojo", __func__);

	if (set_add(curset, &as, 1) != 0)
		fatal(NULL);
}

static void
done_as_set(void)
{
	curset = NULL;
}

static struct prefixset *
new_prefix_set(char *name, int is_roa)
{
	const char *type = "prefix-set";
	struct prefixset_head *sets = &conf->prefixsets;
	struct prefixset *pset;

	if (is_roa) {
		type = "roa-set";
		sets = &conf->originsets;
	}

	if (find_prefixset(name, sets) != NULL)  {
		yyerror("%s \"%s\" already exists", type, name);
		return NULL;
	}
	if ((pset = calloc(1, sizeof(*pset))) == NULL)
		fatal("prefixset");
	if (strlcpy(pset->name, name, sizeof(pset->name)) >=
	    sizeof(pset->name)) {
		yyerror("%s \"%s\" too long: max %zu", type,
		    name, sizeof(pset->name) - 1);
		free(pset);
		return NULL;
	}
	RB_INIT(&pset->psitems);
	return pset;
}

static void
add_roa_set(struct prefixset_item *npsi, u_int32_t as, u_int8_t max)
{
	struct prefixset_item	*psi;
	struct roa_set rs;

	/* no prefixlen option in this tree */
	npsi->p.op = OP_NONE;
	npsi->p.len_max = npsi->p.len_min = npsi->p.len;
	psi = RB_INSERT(prefixset_tree, curpsitree, npsi);
	if (psi == NULL)
		psi = npsi;

	if (psi->set == NULL)
		if ((psi->set = set_new(1, sizeof(rs))) == NULL)
			fatal("set_new");
	rs.as = as;
	rs.maxlen = max;
	if (set_add(psi->set, &rs, 1) != 0)
		fatal("as_set_new");
}

/*	$OpenBSD: parse.y,v 1.199 2002/11/23 05:22:24 mcbride Exp $	*/

/*
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2001 Daniel Hartmeier.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
%{
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <net/pfvar.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <err.h>
#include <pwd.h>
#include <grp.h>
#include <md5.h>

#include "pfctl_parser.h"
#include "pfctl_altq.h"

static struct pfctl *pf = NULL;
static FILE *fin = NULL;
static int debug = 0;
static int lineno = 1;
static int errors = 0;
static int rulestate = 0;
static u_int16_t returnicmpdefault =  (ICMP_UNREACH << 8) | ICMP_UNREACH_PORT;
static u_int16_t returnicmp6default = (ICMP6_DST_UNREACH << 8) |
    ICMP6_DST_UNREACH_NOPORT;
static int blockpolicy = PFRULE_DROP;
static int require_order = 1;

enum {
	PFCTL_STATE_NONE = 0,
	PFCTL_STATE_OPTION = 1,
	PFCTL_STATE_SCRUB = 2,
	PFCTL_STATE_NAT = 3,
	PFCTL_STATE_FILTER = 4
};

enum pfctl_iflookup_mode {
	PFCTL_IFLOOKUP_HOST = 0,
	PFCTL_IFLOOKUP_NET = 1,
	PFCTL_IFLOOKUP_BCAST = 2
};

struct node_if {
	char			 ifname[IFNAMSIZ];
	u_int8_t		 not;
	u_int			 ifa_flags;
	struct node_if		*next;
	struct node_if		*tail;
};

struct node_proto {
	u_int8_t		 proto;
	struct node_proto	*next;
	struct node_proto	*tail;
};

struct node_host {
	struct pf_addr_wrap	 addr;
	struct pf_addr		 bcast;
	sa_family_t		 af;
	u_int8_t		 not;
	u_int8_t		 noroute;
	u_int32_t		 ifindex;	/* link-local IPv6 addrs */
	char			*ifname;
	u_int			 ifa_flags;
	struct node_host	*next;
	struct node_host	*tail;
};

struct node_port {
	u_int16_t		 port[2];
	u_int8_t		 op;
	struct node_port	*next;
	struct node_port	*tail;
};

struct node_uid {
	uid_t			 uid[2];
	u_int8_t		 op;
	struct node_uid		*next;
	struct node_uid		*tail;
};

struct node_gid {
	gid_t			 gid[2];
	u_int8_t		 op;
	struct node_gid		*next;
	struct node_gid		*tail;
};

struct node_icmp {
	u_int8_t		 code;
	u_int8_t		 type;
	u_int8_t		 proto;
	struct node_icmp	*next;
	struct node_icmp	*tail;
};

enum	{ PF_STATE_OPT_MAX=0, PF_STATE_OPT_TIMEOUT=1 };
struct node_state_opt {
	int			 type;
	union {
		u_int32_t	 max_states;
		struct {
			int		number;
			u_int32_t	seconds;
		}		 timeout;
	}			 data;
	struct node_state_opt	*next;
	struct node_state_opt	*tail;
};

struct peer {
	struct node_host	*host;
	struct node_port	*port;
};

struct node_queue {
	char			 queue[PF_QNAME_SIZE];
	char			 parent[PF_QNAME_SIZE];
	char			 ifname[IFNAMSIZ];
	struct node_queue	*next;
	struct node_queue	*tail;
}	*queues = NULL;

struct node_queue_opt {
	int			 qtype;
	union {	/* options for other schedulers will follow */
		struct cbq_opts		 cbq_opts;
	}			 data;
};

struct node_queue_bw {
	u_int32_t	bw_absolute;
	u_int16_t	bw_percent;
};

int	yyerror(char *, ...);
int	rule_consistent(struct pf_rule *);
int	nat_consistent(struct pf_nat *);
int	rdr_consistent(struct pf_rdr *);
int	yyparse(void);
void	set_ipmask(struct node_host *, u_int8_t);
void	expand_rdr(struct pf_rdr *, struct node_if *, struct node_proto *,
	    struct node_host *, struct node_host *, struct node_host *);
void	expand_nat(struct pf_nat *, struct node_if *, struct node_proto *,
	    struct node_host *, struct node_port *,
	    struct node_host *, struct node_port *, struct node_host *);
void	expand_label_if(const char *, char *, const char *);
void	expand_label_addr(const char *, char *, u_int8_t, struct node_host *);
void	expand_label_port(const char *, char *, struct node_port *);
void	expand_label_proto(const char *, char *, u_int8_t);
void	expand_label_nr(const char *, char *);
void	expand_label(char *, const char *, u_int8_t, struct node_host *,
	    struct node_port *, struct node_host *, struct node_port *,
	    u_int8_t);
void  expand_rule(struct pf_rule *, struct node_if *, struct node_host *,
	    struct node_proto *, struct node_host *, struct node_port *,
	    struct node_host *, struct node_port *, struct node_uid *,
	    struct node_gid *, struct node_icmp *);
void	expand_altq(struct pf_altq *, struct node_if *, struct node_queue *);
int	expand_queue(struct pf_altq *, struct node_queue *,
	    struct node_queue_bw);
int	check_rulestate(int);
int	kw_cmp(const void *, const void *);
int	lookup(char *);
int	lgetc(FILE *);
int	lungetc(int, FILE *);
int	findeol(void);
int	yylex(void);
struct	node_host *host(char *, int);
int	atoul(char *, u_long *);
int	getservice(char *);

struct sym {
	struct sym *next;
	int used;
	char *nam;
	char *val;
};
struct sym *symhead = NULL;

int	symset(const char *, const char *);
char *	symget(const char *);

void	ifa_load(void);
struct	node_host *ifa_exists(char *);
struct	node_host *ifa_lookup(char *, enum pfctl_iflookup_mode);
struct	node_host *ifa_pick_ips(struct node_host *, sa_family_t);
u_int16_t	parseicmpspec(char *, sa_family_t);

typedef struct {
	union {
		u_int32_t		number;
		int			i;
		char			*string;
		struct {
			u_int8_t	b1;
			u_int8_t	b2;
			u_int16_t	w;
			u_int16_t	w2;
		}			b;
		struct range {
			int		a;
			int		b;
			int		t;
		}			range;
		struct node_if		*interface;
		struct node_proto	*proto;
		struct node_icmp	*icmp;
		struct node_host	*host;
		struct node_port	*port;
		struct node_uid		*uid;
		struct node_gid		*gid;
		struct node_state_opt	*state_opt;
		struct peer		peer;
		struct {
			struct peer	src, dst;
		}			fromto;
		struct {
			struct node_host	*host;
			u_int8_t	rt;
			u_int8_t	pool_opts;
			sa_family_t	af;
			struct pf_poolhashkey	*key;
		}			route;
		struct redirection {
			struct node_host	*host;
			struct range		 rport;
		}			*redirection;
		struct {
			int		type;
			struct pf_poolhashkey	*key;
		} 			pooltype;
		struct {
			int			 action;
			struct node_state_opt	*options;
		}			keep_state;
		struct {
			u_int8_t	log;
			u_int8_t	quick;
		}			logquick;
		struct node_queue	*queue;
		struct node_queue_opt	queue_options;
		struct node_queue_bw	queue_bwspec;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	PASS BLOCK SCRUB RETURN IN OUT LOG LOGALL QUICK ON FROM TO FLAGS
%token	RETURNRST RETURNICMP RETURNICMP6 PROTO INET INET6 ALL ANY ICMPTYPE
%token	ICMP6TYPE CODE KEEP MODULATE STATE PORT RDR NAT BINAT ARROW NODF
%token	MINTTL ERROR ALLOWOPTS FASTROUTE ROUTETO DUPTO REPLYTO NO LABEL
%token	NOROUTE FRAGMENT USER GROUP MAXMSS MAXIMUM TTL TOS DROP
%token	FRAGNORM FRAGDROP FRAGCROP
%token	SET OPTIMIZATION TIMEOUT LIMIT LOGINTERFACE BLOCKPOLICY
%token	REQUIREORDER YES
%token	ANTISPOOF FOR
%token	BITMASK RANDOM SOURCEHASH ROUNDROBIN KEY STATICPORT
%token	ALTQ SCHEDULER CBQ BANDWIDTH TBRSIZE
%token	QUEUE PRIORITY QLIMIT
%token	DEFAULT CONTROL BORROW RED ECN RIO
%token	<v.string> STRING
%token	<v.i>	PORTUNARY PORTBINARY
%type	<v.interface>	interface if_list if_item_not if_item
%type	<v.number>	number port icmptype icmp6type minttl uid gid maxmss
%type	<v.number>	tos
%type	<v.i>	no dir log af nodf allowopts fragment fragcache
%type	<v.i>	staticport
%type	<v.b>	action flag flags blockspec
%type	<v.range>	dport rport
%type	<v.pooltype>	pooltype
%type	<v.proto>	proto proto_list proto_item
%type	<v.icmp>	icmpspec icmp_list icmp6_list icmp_item icmp6_item
%type	<v.fromto>	fromto
%type	<v.peer>	ipportspec
%type	<v.host>	ipspec xhost host address host_list 
%type	<v.host>	redir_host_list redirspec
%type	<v.host>	route_host route_host_list routespec
%type	<v.port>	portspec port_list port_item
%type	<v.uid>		uids uid_list uid_item
%type	<v.gid>		gids gid_list gid_item
%type	<v.route>	route
%type	<v.redirection>	redirection redirpool
%type	<v.string>	label string
%type	<v.keep_state>	keep
%type	<v.state_opt>	state_opt_spec state_opt_list state_opt_item
%type	<v.logquick>	logquick
%type	<v.interface>	antispoof_ifspc antispoof_iflst
%type	<v.number>	priority qlimit tbrsize
%type	<v.string>	qname
%type	<v.queue>	qassign qassign_list qassign_item
%type	<v.queue_options>	schedtype
%type	<v.number>	cbqflags_list cbqflags_item
%type	<v.queue_bwspec>	bandwidth
%%

ruleset		: /* empty */
		| ruleset '\n'
		| ruleset option '\n'
		| ruleset scrubrule '\n'
		| ruleset natrule '\n'
		| ruleset binatrule '\n'
		| ruleset rdrrule '\n'
		| ruleset pfrule '\n'
		| ruleset altqif '\n'
		| ruleset queuespec '\n'
		| ruleset varset '\n'
		| ruleset antispoof '\n'
		| ruleset error '\n'		{ errors++; }
		;

option		: SET OPTIMIZATION STRING		{
			if (pf->opts & PF_OPT_VERBOSE)
				printf("set optimization %s\n", $3);
			if (check_rulestate(PFCTL_STATE_OPTION))
				YYERROR;
			if (pfctl_set_optimization(pf, $3) != 0) {
				yyerror("unknown optimization %s", $3);
				YYERROR;
			}
		}
		| SET TIMEOUT timeout_spec
		| SET TIMEOUT '{' timeout_list '}'
		| SET LIMIT limit_spec
		| SET LIMIT '{' limit_list '}'
		| SET LOGINTERFACE STRING		{
			if (pf->opts & PF_OPT_VERBOSE)
				printf("set loginterface %s\n", $3);
			if (check_rulestate(PFCTL_STATE_OPTION))
				YYERROR;
			if (pfctl_set_logif(pf, $3) != 0) {
				yyerror("error setting loginterface %s", $3);
				YYERROR;
			}
		}
		| SET BLOCKPOLICY DROP	{
			if (pf->opts & PF_OPT_VERBOSE)
				printf("set block-policy drop\n");
			if (check_rulestate(PFCTL_STATE_OPTION))
				YYERROR;
			blockpolicy = PFRULE_DROP;
		}
		| SET BLOCKPOLICY RETURN {
			if (pf->opts & PF_OPT_VERBOSE)
				printf("set block-policy return\n");
			if (check_rulestate(PFCTL_STATE_OPTION))
				YYERROR;
			blockpolicy = PFRULE_RETURN;
		}
		| SET REQUIREORDER YES {
			if (pf->opts & PF_OPT_VERBOSE)
				printf("set require-order yes\n");
			require_order = 1;
		}
		| SET REQUIREORDER NO {
			if (pf->opts & PF_OPT_VERBOSE)
				printf("set require-order no\n");
			require_order = 0;
		}
		;

string		: string STRING				{
			if (asprintf(&$$, "%s %s", $1, $2) == -1) {
				yyerror("malloc failed");
				YYERROR;
			}
			free($1);
			free($2);
		}
		| STRING
		;

varset		: STRING PORTUNARY string		{
			if (pf->opts & PF_OPT_VERBOSE)
				printf("%s = %s\n", $1, $3);
			if (symset($1, $3) == -1) {
				yyerror("cannot store variable %s", $1);
				YYERROR;
			}
		}
		;

scrubrule	: SCRUB dir interface af fromto nodf minttl maxmss fragcache
		{
			struct pf_rule r;

			if (check_rulestate(PFCTL_STATE_SCRUB))
				YYERROR;

			memset(&r, 0, sizeof(r));

			r.action = PF_SCRUB;
			r.direction = $2;

			if ($3) {
				if ($3->not) {
					yyerror("scrub rules don't support "
					    "'! <if>'");
					YYERROR;
				}
			}
			r.af = $4;
			if ($6)
				r.rule_flag |= PFRULE_NODF;
			if ($7)
				r.min_ttl = $7;
			if ($8)
				r.max_mss = $8;
			if ($9)
				r.rule_flag |= $9;

			expand_rule(&r, $3, NULL, NULL,
			    $5.src.host, $5.src.port, $5.dst.host, $5.dst.port,
			    NULL, NULL, NULL); 
		}
		;

antispoof	: ANTISPOOF logquick antispoof_ifspc af {
			struct pf_rule r;
			struct node_host *h = NULL;
			struct node_if *i, *j;

			if (check_rulestate(PFCTL_STATE_FILTER))
				YYERROR;

			for (i = $3; i; i = i->next) {
				memset(&r, 0, sizeof(r));

				r.action = PF_DROP;
				r.direction = PF_IN;
				r.log = $2.log;
				r.quick = $2.quick;
				r.af = $4;

				j = calloc(1, sizeof(struct node_if));
				if (j == NULL)
					errx(1, "antispoof: calloc");
				strlcpy(j->ifname, i->ifname, IFNAMSIZ);
				j->not = 1;
				h = ifa_lookup(j->ifname, PFCTL_IFLOOKUP_NET);

				expand_rule(&r, j, NULL, NULL, h, NULL, NULL,
				    NULL, NULL, NULL, NULL);

				if ((i->ifa_flags & IFF_LOOPBACK) == 0) {
					memset(&r, 0, sizeof(r));

					r.action = PF_DROP;
					r.direction = PF_IN;
					r.log = $2.log;
					r.quick = $2.quick;
					r.af = $4;

					h = ifa_lookup(i->ifname,
					    PFCTL_IFLOOKUP_HOST);

					expand_rule(&r, NULL, NULL, NULL, h,
					    NULL, NULL, NULL, NULL, NULL, NULL);
				}
			}
		}
		;

antispoof_ifspc	: FOR if_item			{ $$ = $2; }
		| FOR '{' antispoof_iflst '}'	{ $$ = $3; }
		;

antispoof_iflst	: if_item			{ $$ = $1; }
		| antispoof_iflst comma if_item	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;


/* altq stuff */

altqif		: ALTQ interface SCHEDULER schedtype bandwidth tbrsize
		  QUEUE qassign	{
			struct	pf_altq a;

			memset(&a, 0, sizeof(a));
			if ($4.qtype == ALTQT_NONE) {
				yyerror("no scheduler specified!");
				YYERROR;
			}
			a.scheduler = $4.qtype;
			a.pq_u.cbq_opts.flags = $4.data.cbq_opts.flags;
			if ((a.ifbandwidth = $5.bw_absolute) == 0) {
				yyerror("interface bandwidth must be absolute");
				YYERROR;
			}
			a.tbrsize = $6;
			if ($8 == NULL) {
				yyerror("no child queues?");
				YYERROR;
			}
			expand_altq(&a,	$2, $8);
		}
		;

qassign		: /* empty */		{ $$ = NULL; }
		| qassign_item		{ $$ = $1; }
		| '{' qassign_list '}'	{ $$ = $2; }
		;

qassign_list	: qassign_item			{ $$ = $1; }
		| qassign_list comma qassign_item	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

qassign_item	: STRING			{
			$$ = malloc(sizeof(struct node_queue));
			if ($$ == NULL)
				err(1, "queue_item: malloc");
			strlcpy($$->queue, $1, PF_QNAME_SIZE);
			$$->next = NULL;
			$$->tail = $$;
		}
		;

queuespec	: QUEUE STRING bandwidth priority qlimit schedtype qassign {
			struct	pf_altq a;

			memset(&a, 0, sizeof(a));

			if (strlcpy(a.qname, $2, sizeof(a.qname)) >=
			    PF_QNAME_SIZE) {
				yyerror("queue name too long (max "
				    "%d chars)", PF_QNAME_SIZE-1);
				YYERROR;
			}
			if ($4 > 255) {
				yyerror("priority out of range: max 255");
				YYERROR;
			}
			a.priority = $4;
			a.qlimit = $5;
			a.scheduler = $6.qtype;
			switch (a.scheduler) {
			case ALTQT_CBQ:
				a.pq_u.cbq_opts.flags = $6.data.cbq_opts.flags;
			}
			if (expand_queue(&a, $7, $3))
				YYERROR;

		}
		;

schedtype	: /* empty */			{ $$.qtype = ALTQT_NONE; }
		| CBQ				{ $$.qtype = ALTQT_CBQ; }
		| CBQ '(' cbqflags_list ')'	{
			$$.qtype = ALTQT_CBQ;
			$$.data.cbq_opts.flags = $3;
		}
		;

cbqflags_list	: cbqflags_item				{ $$ |= $1; }
		| cbqflags_list comma cbqflags_item	{ $$ |= $3; }
		;


cbqflags_item	: DEFAULT	{ $$ = CBQCLF_DEFCLASS; }
		| CONTROL	{ $$ = CBQCLF_CTLCLASS; }
		| BORROW	{ $$ = CBQCLF_BORROW; }
		| RED		{ $$ = CBQCLF_RED; }
		| ECN		{ $$ = CBQCLF_RED|CBQCLF_ECN; }
		| RIO		{ $$ = CBQCLF_RIO; }
		;

bandwidth	: /* empty */		{
			$$.bw_absolute = 0;
			$$.bw_percent = 0;
		}
		| BANDWIDTH STRING {
			double bps;
			char *cp;

			$$.bw_percent = 0;

			bps = strtod($2, &cp);
			if (cp != NULL) {
				if (!strcmp(cp, "b"))
					;
				else if (!strcmp(cp, "Kb"))
					bps *= 1024;
				else if (!strcmp(cp, "Mb"))
					bps *= 1024 * 1024;
				else if (!strcmp(cp, "Gb"))
					bps *= 1024 * 1024 * 1024;
				else if (!strcmp(cp, "%")) {
					if (bps < 0 || bps > 100) {
						yyerror("bandwidth spec "
						    "out of range");
						YYERROR;
					}
					$$.bw_percent = bps;
					bps = 0;
				} else {
					yyerror("unknown unit %s", cp);
					YYERROR;
				}
			}
			$$.bw_absolute = (u_int32_t)bps;
		}
		;

priority	: /* empty */		{ $$ = 1; }
		| PRIORITY number	{
			if ($2 > 255) {
				yyerror("priority out of range: max 255");
				YYERROR;
			}
			$$ = $2;
		}
		;

qlimit		: /* empty */		{ $$ = 0; }
		| QLIMIT number		{
			if ($2 > 65535) {
				yyerror("qlimit out of range: max 65535");
				YYERROR;
			}
			$$ = $2;
		}
		;


tbrsize		: /* empty */		{ $$ = 0; }
		| number		{
			if ($1 > 65535) {
				yyerror("tbrsize too big: max 65535");
				YYERROR;
			}
			$$ = $1;
		}
		;

pfrule		: action dir logquick interface route af proto fromto
		  uids gids flags icmpspec tos keep fragment allowopts label
		  qname
		{
			struct pf_rule r;
			struct node_state_opt *o;
			struct node_proto *proto;

			if (check_rulestate(PFCTL_STATE_FILTER))
				YYERROR;

			memset(&r, 0, sizeof(r));

			r.action = $1.b1;
			switch ($1.b2) {
			case PFRULE_RETURNRST:
				r.rule_flag |= PFRULE_RETURNRST;
				r.return_ttl = $1.w;
				break;
			case PFRULE_RETURNICMP:
				r.rule_flag |= PFRULE_RETURNICMP;
				r.return_icmp = $1.w;
				r.return_icmp6 = $1.w2;
				break;
			case PFRULE_RETURN:
				r.rule_flag |= PFRULE_RETURN;
				r.return_icmp = $1.w;
				r.return_icmp6 = $1.w2;
				break;
			}
			r.direction = $2;
			r.log = $3.log;
			r.quick = $3.quick;

			r.af = $6;
			r.flags = $11.b1;
			r.flagset = $11.b2;

			if ($11.b1 || $11.b2) {
				for (proto = $7; proto != NULL &&
				    proto->proto != IPPROTO_TCP;
				    proto = proto->next)
					;	/* nothing */
				if (proto == NULL && $7 != NULL) {
					yyerror("flags only apply to tcp");
					YYERROR;
				}
			}

			r.tos = $13;
			r.keep_state = $14.action;
			o = $14.options;
			while (o) {
				struct node_state_opt *p = o;

				switch (o->type) {
				case PF_STATE_OPT_MAX:
					if (r.max_states) {
						yyerror("state option 'max' "
						    "multiple definitions");
						YYERROR;
					}
					r.max_states = o->data.max_states;
					break;
				case PF_STATE_OPT_TIMEOUT:
					if (r.timeout[o->data.timeout.number]) {
						yyerror("state timeout %s "
						    "multiple definitions",
						    pf_timeouts[o->data.
						    timeout.number].name);
						YYERROR;
					}
					r.timeout[o->data.timeout.number] =
					    o->data.timeout.seconds;
				}
				o = o->next;
				free(p);
			}

			if ($15)
				r.rule_flag |= PFRULE_FRAGMENT;
			r.allow_opts = $16;

			if ($5.rt) {
				r.rt = $5.rt;
				r.rt_pool.opts = $5.pool_opts;
			}

			if ($17) {
				if (strlcpy(r.label, $17, sizeof(r.label)) >=
				    PF_RULE_LABEL_SIZE) {
					yyerror("rule label too long (max "
					    "%d chars)", PF_RULE_LABEL_SIZE-1);
					YYERROR;
				}
				free($17);
			}

			if ($18) {
				if (strlcpy(r.qname, $18, sizeof(r.qname)) >=
				    PF_QNAME_SIZE) {
					yyerror("rule qname too long (max "
					    "%d chars)", PF_QNAME_SIZE-1);
					YYERROR;
				}
				free($18);
			}

			expand_rule(&r, $4, $5.host, $7,
			    $8.src.host, $8.src.port, $8.dst.host, $8.dst.port, 
			    $9, $10, $12);
		}
		;

action		: PASS			{ $$.b1 = PF_PASS; $$.b2 = $$.w = 0; }
		| BLOCK blockspec	{ $$ = $2; $$.b1 = PF_DROP; }
		;

blockspec	: /* empty */		{
			$$.b2 = blockpolicy;
			$$.w = returnicmpdefault;
			$$.w2 = returnicmp6default;
		}
		| DROP			{
			$$.b2 = PFRULE_DROP;
			$$.w = 0;
			$$.w2 = 0;
		}
		| RETURNRST		{
			$$.b2 = PFRULE_RETURNRST;
			$$.w = 0;
			$$.w2 = 0;
		}
		| RETURNRST '(' TTL number ')'	{
			$$.b2 = PFRULE_RETURNRST;
			$$.w = $4;
			$$.w2 = 0;
		}
		| RETURNICMP		{
			$$.b2 = PFRULE_RETURNICMP;
			$$.w = returnicmpdefault;
			$$.w2 = returnicmp6default;
		}
		| RETURNICMP6		{
			$$.b2 = PFRULE_RETURNICMP;
			$$.w = returnicmpdefault;
			$$.w2 = returnicmp6default;
		}
		| RETURNICMP '(' STRING ')'	{
			$$.b2 = PFRULE_RETURNICMP;
			if (!($$.w = parseicmpspec($3, AF_INET)))
				YYERROR;
			$$.w2 = returnicmp6default;
		}
		| RETURNICMP6 '(' STRING ')'	{
			$$.b2 = PFRULE_RETURNICMP;
			$$.w = returnicmpdefault;
			if (!($$.w2 = parseicmpspec($3, AF_INET6)))
				YYERROR;
		}
		| RETURNICMP '(' STRING comma STRING ')' {
			$$.b2 = PFRULE_RETURNICMP;
			if (!($$.w = parseicmpspec($3, AF_INET)))
				YYERROR;
			if (!($$.w2 = parseicmpspec($5, AF_INET6)));
		}
		| RETURN {
			$$.b2 = PFRULE_RETURN;
			$$.w = returnicmpdefault;
			$$.w2 = returnicmp6default;
		}
		;

fragcache	: /* empty */		{ $$ = 0; }
		| fragment FRAGNORM	{ $$ = 0; /* default */ }
		| fragment FRAGCROP	{ $$ = PFRULE_FRAGCROP; }
		| fragment FRAGDROP	{ $$ = PFRULE_FRAGDROP; }
		;


dir		: IN				{ $$ = PF_IN; }
		| OUT				{ $$ = PF_OUT; }
		;

logquick	: /* empty */			{ $$.log = 0; $$.quick = 0; }
		| log				{ $$.log = $1; $$.quick = 0; }
		| QUICK				{ $$.log = 0; $$.quick = 1; }
		| log QUICK			{ $$.log = $1; $$.quick = 1; }
		| QUICK log			{ $$.log = $2; $$.quick = 1; }
		;

log		: LOG				{ $$ = 1; }
		| LOGALL			{ $$ = 2; }
		;

interface	: /* empty */			{ $$ = NULL; }
		| ON if_item_not		{ $$ = $2; }
		| ON '{' if_list '}'		{ $$ = $3; }
		;

if_list		: if_item_not			{ $$ = $1; }
		| if_list comma if_item_not	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

if_item_not	: '!' if_item			{ $$ = $2; $$->not = 1; }
		| if_item			{ $$ = $1; }

if_item		: STRING			{
			struct node_host *n;

			if ((n = ifa_exists($1)) == NULL) {
				yyerror("unknown interface %s", $1);
				YYERROR;
			}
			$$ = malloc(sizeof(struct node_if));
			if ($$ == NULL)
				err(1, "if_item: malloc");
			strlcpy($$->ifname, $1, IFNAMSIZ);
			$$->ifa_flags = n->ifa_flags;
			$$->not = 0;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

af		: /* empty */			{ $$ = 0; }
		| INET				{ $$ = AF_INET; }
		| INET6				{ $$ = AF_INET6; }

proto		: /* empty */			{ $$ = NULL; }
		| PROTO proto_item		{ $$ = $2; }
		| PROTO '{' proto_list '}'	{ $$ = $3; }
		;

proto_list	: proto_item			{ $$ = $1; }
		| proto_list comma proto_item	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

proto_item	: STRING			{
			u_int8_t pr;
			u_long ulval;

			if (atoul($1, &ulval) == 0) {
				if (ulval > 255) {
					yyerror("protocol outside range");
					YYERROR;
				}
				pr = (u_int8_t)ulval;
			} else {
				struct protoent *p;

				p = getprotobyname($1);
				if (p == NULL) {
					yyerror("unknown protocol %s", $1);
					YYERROR;
				}
				pr = p->p_proto;
			}
			if (pr == 0) {
				yyerror("proto 0 cannot be used");
				YYERROR;
			}
			$$ = malloc(sizeof(struct node_proto));
			if ($$ == NULL)
				err(1, "proto_item: malloc");
			$$->proto = pr;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

fromto		: ALL				{
			$$.src.host = NULL;
			$$.src.port = NULL;
			$$.dst.host = NULL;
			$$.dst.port = NULL;
		}
		| FROM ipportspec TO ipportspec	{
			$$.src = $2;
			$$.dst = $4;
		}
		;

ipportspec	: ipspec			{ $$.host = $1; $$.port = NULL; }
		| ipspec PORT portspec		{
			$$.host = $1;
			$$.port = $3;
		}
		;

ipspec		: ANY				{ $$ = NULL; }
		| xhost				{ $$ = $1; }
		| '{' host_list '}'		{ $$ = $2; }
		;

host_list	: xhost				{ $$ = $1; }
		| host_list comma xhost		{
			/* $3 may be a list, so use its tail pointer */
			$1->tail->next = $3->tail;
			$1->tail = $3->tail;
			$$ = $1;
		}
		;

xhost		: '!' host			{
			struct node_host *h;
			for (h = $2; h; h = h->next)
				h->not = 1;
			$$ = $2;
		}
		| host				{ $$ = $1; }
		| NOROUTE			{
			$$ = calloc(1, sizeof(struct node_host));
			if ($$ == NULL)
				err(1, "xhost: calloc");
			$$->noroute = 1;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

host		: address
		| STRING '/' number		{ $$ = host($1, $3); }
		;

number		: STRING			{
			u_long ulval;

			if (atoul($1, &ulval) == -1) {
				yyerror("%s is not a number", $1);
				YYERROR;
			} else
				$$ = ulval;
		}
		;

address		: '(' STRING ')'		{
			$$ = calloc(1, sizeof(struct node_host));
			if ($$ == NULL)
				err(1, "address: calloc");
			$$->af = 0;
			$$->ifname[0] = 0;
			set_ipmask($$, 128);
			$$->addr.addr_dyn = (struct pf_addr_dyn *)1;
			strncpy($$->addr.addr.pfa.ifname, $2,
			    sizeof($$->addr.addr.pfa.ifname));
			$$->next = NULL;
			$$->tail = $$;
		}
		| STRING			{ $$ = host($1, -1); }
		;

portspec	: port_item			{ $$ = $1; }
		| '{' port_list '}'		{ $$ = $2; }
		;

port_list	: port_item			{ $$ = $1; }
		| port_list comma port_item	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

port_item	: port				{
			$$ = malloc(sizeof(struct node_port));
			if ($$ == NULL)
				err(1, "port_item: malloc");
			$$->port[0] = $1;
			$$->port[1] = $1;
			$$->op = PF_OP_EQ;
			$$->next = NULL;
			$$->tail = $$;
		}
		| PORTUNARY port		{
			$$ = malloc(sizeof(struct node_port));
			if ($$ == NULL)
				err(1, "port_item: malloc");
			$$->port[0] = $2;
			$$->port[1] = $2;
			$$->op = $1;
			$$->next = NULL;
			$$->tail = $$;
		}
		| port PORTBINARY port		{
			$$ = malloc(sizeof(struct node_port));
			if ($$ == NULL)
				err(1, "port_item: malloc");
			$$->port[0] = $1;
			$$->port[1] = $3;
			$$->op = $2;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

port		: STRING			{
			struct servent *s = NULL;
			u_long ulval;

			if (atoul($1, &ulval) == 0) {
				if (ulval > 65535) {
					yyerror("illegal port value %d", ulval);
					YYERROR;
				}
				$$ = htons(ulval);
			} else {
				s = getservbyname($1, "tcp");
				if (s == NULL)
					s = getservbyname($1, "udp");
				if (s == NULL) {
					yyerror("unknown port %s", $1);
					YYERROR;
				}
				$$ = s->s_port;
			}
		}
		;

uids		: /* empty */			{ $$ = NULL; }
		| USER uid_item			{ $$ = $2; }
		| USER '{' uid_list '}'		{ $$ = $3; }
		;

uid_list	: uid_item			{ $$ = $1; }
		| uid_list comma uid_item	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

uid_item	: uid				{
			$$ = malloc(sizeof(struct node_uid));
			if ($$ == NULL)
				err(1, "uid_item: malloc");
			$$->uid[0] = $1;
			$$->uid[1] = $1;
			$$->op = PF_OP_EQ;
			$$->next = NULL;
			$$->tail = $$;
		}
		| PORTUNARY uid			{
			if ($2 == UID_MAX && $1 != PF_OP_EQ && $1 != PF_OP_NE) {
				yyerror("user unknown requires operator = or !=");
				YYERROR;
			}
			$$ = malloc(sizeof(struct node_uid));
			if ($$ == NULL)
				err(1, "uid_item: malloc");
			$$->uid[0] = $2;
			$$->uid[1] = $2;
			$$->op = $1;
			$$->next = NULL;
			$$->tail = $$;
		}
		| uid PORTBINARY uid		{
			if ($1 == UID_MAX || $3 == UID_MAX) {
				yyerror("user unknown requires operator = or !=");
				YYERROR;
			}
			$$ = malloc(sizeof(struct node_uid));
			if ($$ == NULL)
				err(1, "uid_item: malloc");
			$$->uid[0] = $1;
			$$->uid[1] = $3;
			$$->op = $2;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

uid		: STRING			{
			u_long ulval;

			if (atoul($1, &ulval) == -1) {
				if (!strcmp($1, "unknown"))
					$$ = UID_MAX;
				else {
					struct passwd *pw;

					if ((pw = getpwnam($1)) == NULL) {
						yyerror("unknown user %s", $1);
						YYERROR;
					}
					$$ = pw->pw_uid;
				}
			} else {
				if (ulval >= UID_MAX) {
					yyerror("illegal uid value %lu", ulval);
					YYERROR;
				}
				$$ = ulval;
			}
		}
		;

gids		: /* empty */			{ $$ = NULL; }
		| GROUP gid_item		{ $$ = $2; }
		| GROUP '{' gid_list '}'	{ $$ = $3; }
		;

gid_list	: gid_item			{ $$ = $1; }
		| gid_list comma gid_item	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

gid_item	: gid				{
			$$ = malloc(sizeof(struct node_gid));
			if ($$ == NULL)
				err(1, "gid_item: malloc");
			$$->gid[0] = $1;
			$$->gid[1] = $1;
			$$->op = PF_OP_EQ;
			$$->next = NULL;
			$$->tail = $$;
		}
		| PORTUNARY gid			{
			if ($2 == GID_MAX && $1 != PF_OP_EQ && $1 != PF_OP_NE) {
				yyerror("group unknown requires operator = or !=");
				YYERROR;
			}
			$$ = malloc(sizeof(struct node_gid));
			if ($$ == NULL)
				err(1, "gid_item: malloc");
			$$->gid[0] = $2;
			$$->gid[1] = $2;
			$$->op = $1;
			$$->next = NULL;
			$$->tail = $$;
		}
		| gid PORTBINARY gid		{
			if ($1 == GID_MAX || $3 == GID_MAX) {
				yyerror("group unknown requires operator = or !=");
				YYERROR;
			}
			$$ = malloc(sizeof(struct node_gid));
			if ($$ == NULL)
				err(1, "gid_item: malloc");
			$$->gid[0] = $1;
			$$->gid[1] = $3;
			$$->op = $2;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

gid		: STRING			{
			u_long ulval;

			if (atoul($1, &ulval) == -1) {
				if (!strcmp($1, "unknown"))
					$$ = GID_MAX;
				else {
					struct group *grp;

					if ((grp = getgrnam($1)) == NULL) {
						yyerror("unknown group %s", $1);
						YYERROR;
					}
					$$ = grp->gr_gid;
				}
			} else {
				if (ulval >= GID_MAX) {
					yyerror("illegal gid value %lu", ulval);
					YYERROR;
				}
				$$ = ulval;
			}
		}
		;

flag		: STRING			{
			int f;

			if ((f = parse_flags($1)) < 0) {
				yyerror("bad flags %s", $1);
				YYERROR;
			}
			$$.b1 = f;
		}
		;

flags		: /* empty */			{ $$.b1 = 0; $$.b2 = 0; }
		| FLAGS flag "/" flag		{ $$.b1 = $2.b1; $$.b2 = $4.b1; }
		| FLAGS "/" flag		{ $$.b1 = 0; $$.b2 = $3.b1; }
		;

icmpspec	: /* empty */			{ $$ = NULL; }
		| ICMPTYPE icmp_item		{ $$ = $2; }
		| ICMPTYPE '{' icmp_list '}'	{ $$ = $3; }
		| ICMP6TYPE icmp6_item		{ $$ = $2; }
		| ICMP6TYPE '{' icmp6_list '}'	{ $$ = $3; }
		;

icmp_list	: icmp_item			{ $$ = $1; }
		| icmp_list comma icmp_item	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

icmp6_list	: icmp6_item			{ $$ = $1; }
		| icmp6_list comma icmp6_item	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

icmp_item	: icmptype		{
			$$ = malloc(sizeof(struct node_icmp));
			if ($$ == NULL)
				err(1, "icmp_item: malloc");
			$$->type = $1;
			$$->code = 0;
			$$->proto = IPPROTO_ICMP;
			$$->next = NULL;
			$$->tail = $$;
		}
		| icmptype CODE STRING	{
			const struct icmpcodeent *p;
			u_long ulval;

			if (atoul($3, &ulval) == 0) {
				if (ulval > 255) {
					yyerror("illegal icmp-code %d", ulval);
					YYERROR;
				}
			} else {
				if ((p = geticmpcodebyname($1-1, $3,
				    AF_INET)) == NULL) {
					yyerror("unknown icmp-code %s", $3);
					YYERROR;
				}
				ulval = p->code;
			}
			$$ = malloc(sizeof(struct node_icmp));
			if ($$ == NULL)
				err(1, "icmp_item: malloc");
			$$->type = $1;
			$$->code = ulval + 1;
			$$->proto = IPPROTO_ICMP;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

icmp6_item	: icmp6type		{
			$$ = malloc(sizeof(struct node_icmp));
			if ($$ == NULL)
				err(1, "icmp_item: malloc");
			$$->type = $1;
			$$->code = 0;
			$$->proto = IPPROTO_ICMPV6;
			$$->next = NULL;
			$$->tail = $$;
		}
		| icmp6type CODE STRING	{
			const struct icmpcodeent *p;
			u_long ulval;

			if (atoul($3, &ulval) == 0) {
				if (ulval > 255) {
					yyerror("illegal icmp6-code %ld", ulval);
					YYERROR;
				}
			} else {
				if ((p = geticmpcodebyname($1-1, $3,
				    AF_INET6)) == NULL) {
					yyerror("unknown icmp6-code %s", $3);
					YYERROR;
				}
				ulval = p->code;
			}
			$$ = malloc(sizeof(struct node_icmp));
			if ($$ == NULL)
				err(1, "icmp_item: malloc");
			$$->type = $1;
			$$->code = ulval + 1;
			$$->proto = IPPROTO_ICMPV6;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

icmptype	: STRING			{
			const struct icmptypeent *p;
			u_long ulval;

			if (atoul($1, &ulval) == 0) {
				if (ulval > 255) {
					yyerror("illegal icmp-type %d", ulval);
					YYERROR;
				}
				$$ = ulval + 1;
			} else {
				if ((p = geticmptypebyname($1, AF_INET)) == NULL) {
					yyerror("unknown icmp-type %s", $1);
					YYERROR;
				}
				$$ = p->type + 1;
			}
		}
		;

icmp6type	: STRING			{
			const struct icmptypeent *p;
			u_long ulval;

			if (atoul($1, &ulval) == 0) {
				if (ulval > 255) {
					yyerror("illegal icmp6-type %d", ulval);
					YYERROR;
				}
				$$ = ulval + 1;
			} else {
				if ((p = geticmptypebyname($1, AF_INET6)) == NULL) {
					yyerror("unknown ipv6-icmp-type %s", $1);
					YYERROR;
				}
				$$ = p->type + 1;
			}
		}
		;

tos		: /* empty */			{ $$ = 0; }
		| TOS STRING			{
			if (!strcmp($2, "lowdelay"))
				$$ = IPTOS_LOWDELAY;
			else if (!strcmp($2, "throughput"))
				$$ = IPTOS_THROUGHPUT;
			else if (!strcmp($2, "reliability"))
				$$ = IPTOS_RELIABILITY;
			else if ($2[0] == '0' && $2[1] == 'x')
				$$ = strtoul($2, NULL, 16);
			else
				$$ = strtoul($2, NULL, 10);
			if (!$$ || $$ > 255) {
				yyerror("illegal tos value %s", $2);
				YYERROR;
			}
		}
		;

keep		: /* empty */			{
			$$.action = 0;
			$$.options = NULL;
		}
		| KEEP STATE state_opt_spec	{
			$$.action = PF_STATE_NORMAL;
			$$.options = $3;
		}
		| MODULATE STATE state_opt_spec	{
			$$.action = PF_STATE_MODULATE;
			$$.options = $3;
		}
		;

state_opt_spec	: /* empty */			{ $$ = NULL; }
		| '(' state_opt_list ')'	{ $$ = $2; }
		;

state_opt_list	: state_opt_item		{ $$ = $1; }
		| state_opt_list comma state_opt_item {
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

state_opt_item	: MAXIMUM number		{
			if ($2 <= 0) {
				yyerror("illegal states max value %d", $2);
				YYERROR;
			}
			$$ = calloc(1, sizeof(struct node_state_opt));
			if ($$ == NULL)
				err(1, "state_opt_item: calloc");
			$$->type = PF_STATE_OPT_MAX;
			$$->data.max_states = $2;
			$$->next = NULL;
			$$->tail = $$;
		}
		| STRING number			{
			int i;

			for (i = 0; pf_timeouts[i].name &&
			    strcmp(pf_timeouts[i].name, $1); ++i)
				;	/* nothing */
			if (!pf_timeouts[i].name) {
				yyerror("illegal timeout name %s", $1);
				YYERROR;
			}
			if (strchr(pf_timeouts[i].name, '.') == NULL) {
				yyerror("illegal state timeout %s", $1);
				YYERROR;
			}
			$$ = calloc(1, sizeof(struct node_state_opt));
			if ($$ == NULL)
				err(1, "state_opt_item: calloc");
			$$->type = PF_STATE_OPT_TIMEOUT;
			$$->data.timeout.number = pf_timeouts[i].timeout;
			$$->data.timeout.seconds = $2;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

fragment	: /* empty */			{ $$ = 0; }
		| FRAGMENT			{ $$ = 1; }

minttl		: /* empty */			{ $$ = 0; }
		| MINTTL number			{
			if ($2 > 255) {
				yyerror("illegal min-ttl value %d", $2);
				YYERROR;
			}
			$$ = $2;
		}
		;

nodf		: /* empty */			{ $$ = 0; }
		| NODF				{ $$ = 1; }
		;

maxmss		: /* empty */			{ $$ = 0; }
		| MAXMSS number			{ $$ = $2; }
		;

allowopts	: /* empty */			{ $$ = 0; }
		| ALLOWOPTS			{ $$ = 1; }

label		: /* empty */			{ $$ = NULL; }
		| LABEL STRING			{
			if (($$ = strdup($2)) == NULL) {
				yyerror("rule label strdup() failed");
				YYERROR;
			}
		}
		;
qname		: /* empty */			{ $$ = NULL; }
		| QUEUE STRING			{
			if (($$ = strdup($2)) == NULL) {
				yyerror("qname strdup() failed");
				YYERROR;
			}
		}
		;

no		: /* empty */			{ $$ = 0; }
		| NO				{ $$ = 1; }
		;

rport		: STRING			{
			char *p = strchr($1, ':');

			if (p == NULL) {
				if (($$.a = getservice($1)) == -1)
					YYERROR;
				$$.b = $$.t = 0;
			} else if (!strcmp(p+1, "*")) {
				*p = 0;
				if (($$.a = getservice($1)) == -1)
					YYERROR;
				$$.b = 0;
				$$.t = PF_RPORT_RANGE;
			} else {
				*p++ = 0;
				if (($$.a = getservice($1)) == -1 ||
				    ($$.b = getservice(p)) == -1)
					YYERROR;
				$$.t = PF_RPORT_RANGE;
			}
		}
		;

redirspec	: host				{ $$ = $1; }
		| '{' redir_host_list '}'	{ $$ = $2; }
		;

redir_host_list	: host				{ $$ = $1; }
		| redir_host_list comma host	{
			/* $3 may be a list, so use its tail pointer */
			$1->tail->next = $3->tail;
			$1->tail = $3->tail;
			$$ = $1;
		}
		;

redirpool	: /* empty */			{ $$ = NULL; }
		| ARROW redirspec		{
			$$ = malloc(sizeof(struct redirection));
			if ($$ == NULL)
				err(1, "redirection: malloc");
			$$->host = $2;
			$$->rport.a = $$->rport.b = $$->rport.t = 0;
		}
		| ARROW redirspec PORT rport	{
			$$ = malloc(sizeof(struct redirection));
			if ($$ == NULL)
				err(1, "redirection: malloc");
			$$->host = $2;
			$$->rport = $4;
		}
		;

pooltype	: /* empty */			{ $$.type = PF_POOL_NONE; }
		| BITMASK			{ $$.type = PF_POOL_BITMASK; }
		| RANDOM			{ $$.type = PF_POOL_RANDOM; }
		| SOURCEHASH			{ $$.type = PF_POOL_SRCHASH; }
		| SOURCEHASH RANDOM
		{
			$$.key = malloc(sizeof(struct pf_poolhashkey));
			if ($$.key == NULL)
				err(1, "pooltype: malloc");
			$$.type = PF_POOL_SRCKEYHASH;
			$$.key->key32[0] = arc4random();
			$$.key->key32[1] = arc4random();
			$$.key->key32[2] = arc4random();
			$$.key->key32[3] = arc4random();
		}
		| SOURCEHASH KEY string		
		{
			MD5_CTX context;
			
			$$.key = malloc(sizeof(struct pf_poolhashkey));
			if ($$.key == NULL)
				err(1, "pooltype: malloc");
			$$.type = PF_POOL_SRCKEYHASH;
			MD5Init(&context);
			MD5Update(&context, $3, strlen($3));
			MD5Final((unsigned char *)$$.key, &context);
		}
		| ROUNDROBIN 			{ $$.type = PF_POOL_ROUNDROBIN; }
		;

staticport	: /* empty */			{ $$ = 0; }
		| STATICPORT			{ $$ = PF_POOL_STATICPORT; }
		;

redirection	: /* empty */			{ $$ = NULL; }
		| ARROW host			{
			$$ = malloc(sizeof(struct redirection));
			if ($$ == NULL)
				err(1, "redirection: malloc");
			$$->host = $2;
			$$->rport.a = $$->rport.b = $$->rport.t = 0;
		}
		| ARROW host PORT rport	{
			$$ = malloc(sizeof(struct redirection));
			if ($$ == NULL)
				err(1, "redirection: malloc");
			$$->host = $2;
			$$->rport = $4;
		}
		;

natrule		: no NAT interface af proto fromto redirpool pooltype staticport
		{
			struct pf_nat nat;

			if (check_rulestate(PFCTL_STATE_NAT))
				YYERROR;

			memset(&nat, 0, sizeof(nat));

			nat.no = $1;
			nat.af = $4;

			if (!nat.af) {
				if ($6.src.host && $6.src.host->af &&
				    !$6.src.host->ifindex)
					nat.af = $6.src.host->af;
				else if ($6.dst.host && $6.dst.host->af &&
				    !$6.dst.host->ifindex)
					nat.af = $6.dst.host->af;
			}

			if (nat.no) {
				if ($7 != NULL) {
					yyerror("'no nat' rule does not need "
					    "'->'");
					YYERROR;
				}
			} else {
				struct node_host *n;

				if ($7 == NULL || $7->host == NULL) {
					yyerror("'nat' rule requires '-> "
					    "address'");
					YYERROR;
				}
				if (!nat.af && !$7->host->ifindex)
					nat.af = $7->host->af;

				n = ifa_pick_ips($7->host, nat.af);
				if (n == NULL)
					YYERROR;
				if (!nat.af)
					nat.af = n->af;
				nat.proxy_port[0] = ntohs($7->rport.a);
				nat.proxy_port[1] = ntohs($7->rport.b);
				if (!nat.proxy_port[0] && !nat.proxy_port[1]) {
					nat.proxy_port[0] =
					    PF_NAT_PROXY_PORT_LOW;
					nat.proxy_port[1] =
					    PF_NAT_PROXY_PORT_HIGH;
				} else if (!nat.proxy_port[1])
					nat.proxy_port[1] = nat.proxy_port[0];
			}

			if ($7->host->next) {
				nat.rpool.opts = $8.type;
				if (nat.rpool.opts == PF_POOL_NONE) 
					nat.rpool.opts = PF_POOL_ROUNDROBIN;
				if (nat.rpool.opts != PF_POOL_ROUNDROBIN) {
					yyerror("nat: only round-robin valid "
					    "for multiple redirection "
					    "addresses");
					YYERROR;
				} 
			} else {
				if ((nat.af == AF_INET &&
				    unmask(&$7->host->addr.mask,
				    nat.af) == 32) ||
				    (nat.af == AF_INET6 &&
				    unmask(&$7->host->addr.mask,
				    nat.af) == 128)) {
					nat.rpool.opts = PF_POOL_NONE;
				} else {
					if ($8.type == PF_POOL_NONE) 
						nat.rpool.opts =
						    PF_POOL_ROUNDROBIN;
					else
						nat.rpool.opts = $8.type;
				}
			}
			if ((nat.rpool.opts & PF_POOL_TYPEMASK) ==
			    PF_POOL_SRCKEYHASH) {
				memcpy(&nat.rpool.key, $8.key,
				    sizeof(struct pf_poolhashkey));
			}

			expand_nat(&nat, $3, $5, $6.src.host, $6.src.port,
			    $6.dst.host, $6.dst.port, $7->host);
			free($7);
		}
		;

binatrule	: no BINAT interface af proto FROM host TO ipspec redirection
		{
			struct pf_binat binat;

			if (check_rulestate(PFCTL_STATE_NAT))
				YYERROR;

			memset(&binat, 0, sizeof(binat));

			binat.no = $1;
			if ($3 != NULL) {
				memcpy(binat.ifname, $3->ifname,
				    sizeof(binat.ifname));
				free($3);
			}
			binat.af = $4;
			if ($5 != NULL) {
				binat.proto = $5->proto;
				free($5);
			}
			if ($7 != NULL && $9 != NULL && $7->af != $9->af) {
				yyerror("binat ip versions must match");
				YYERROR;
			}
			if ($7 != NULL) {
				if ($7->next) {
					yyerror("multiple binat ip addresses");
					YYERROR;
				}
				if ($7->addr.addr_dyn != NULL) {
					if (!binat.af) {
						yyerror("address family (inet/"
						    "inet6) undefined");
						YYERROR;
					}
					$7->af = binat.af;
				}
				if (binat.af && $7->af != binat.af) {
					yyerror("binat ip versions must match");
					YYERROR;
				}
				binat.af = $7->af;
				memcpy(&binat.saddr.addr, &$7->addr.addr,
				    sizeof(binat.saddr.addr));
				memcpy(&binat.saddr.mask, &$7->addr.mask,
				    sizeof(binat.saddr.mask));
				free($7);
			}
			if ($9 != NULL) {
				if ($9->next) {
					yyerror("multiple binat ip addresses");
					YYERROR;
				}
				if ($9->addr.addr_dyn != NULL) {
					if (!binat.af) {
						yyerror("address family (inet/"
						    "inet6) undefined");
						YYERROR;
					}
					$9->af = binat.af;
				}
				if (binat.af && $9->af != binat.af) {
					yyerror("binat ip versions must match");
					YYERROR;
				}
				binat.af = $9->af;
				memcpy(&binat.daddr.addr, &$9->addr.addr,
				    sizeof(binat.daddr.addr));
				memcpy(&binat.daddr.mask, &$9->addr.mask,
				    sizeof(binat.daddr.mask));
				binat.dnot  = $9->not;
				free($9);
			}

			if (binat.no) {
				if ($10 != NULL) {
					yyerror("'no binat' rule does not need"
					    " '->'");
					YYERROR;
				}
			} else {
				struct node_host *n;

				if ($10 == NULL || $10->host == NULL) {
					yyerror("'binat' rule requires"
					    " '-> address'");
					YYERROR;
				}
				n = ifa_pick_ips($10->host, binat.af);
				if (n == NULL)
					YYERROR;
				if (n->next != NULL) {
					yyerror("multiple addresses in '-> "
					    "address'");
					YYERROR;
				}
				if (n->addr.addr_dyn != NULL) {
					if (!binat.af) {
						yyerror("address family (inet/"
						    "inet6) undefined");
						YYERROR;
					}
					n->af = binat.af;
				}
				if (binat.af && n->af != binat.af) {
					yyerror("binat ip versions must match");
					YYERROR;
				}
				binat.af = n->af;
				memcpy(&binat.raddr.addr, &n->addr.addr,
				    sizeof(binat.raddr.addr));
				memcpy(&binat.raddr.mask, &n->addr.mask,
				    sizeof(binat.raddr.mask));
				if (!PF_AZERO(&binat.saddr.mask, binat.af) &&
				    !PF_AEQ(&binat.saddr.mask,
				    &binat.raddr.mask, binat.af)) {
					yyerror("'binat' source mask and "
					    "redirect mask must be the same");
					YYERROR;
				} 
				free($10);
			}

			pfctl_add_binat(pf, &binat);
		}
		;

rdrrule		: no RDR interface af proto FROM ipspec TO ipspec dport redirpool pooltype
		{
			struct pf_rdr rdr;

			if (check_rulestate(PFCTL_STATE_NAT))
				YYERROR;

			memset(&rdr, 0, sizeof(rdr));

			rdr.no = $1;
			rdr.af = $4;
			if ($7 != NULL) {
				memcpy(&rdr.saddr.addr, &$7->addr.addr,
				    sizeof(rdr.saddr.addr));
				memcpy(&rdr.saddr.mask, &$7->addr.mask,
				    sizeof(rdr.saddr.mask));
				rdr.snot  = $7->not;
				if (!rdr.af && !$7->ifindex)
					rdr.af = $7->af;
			}
			if ($9 != NULL) {
				memcpy(&rdr.daddr.addr, &$9->addr.addr,
				    sizeof(rdr.daddr.addr));
				memcpy(&rdr.daddr.mask, &$9->addr.mask,
				    sizeof(rdr.daddr.mask));
				rdr.dnot  = $9->not;
				if (!rdr.af && !$9->ifindex)
					rdr.af = $9->af;
			}

			rdr.dport  = $10.a;
			rdr.dport2 = $10.b;
			rdr.opts  |= $10.t;

			if ($12.type == PF_POOL_NONE) 
				rdr.rpool.opts = PF_POOL_RANDOM;
			else
				rdr.rpool.opts = $12.type;

			if (rdr.no) {
				if ($11 != NULL) {
					yyerror("'no rdr' rule does not need '->'");
					YYERROR;
				}
			} else {
				struct node_host *n;

				if ($11 == NULL || $11->host == NULL) {
					yyerror("'rdr' rule requires '-> "
					    "address'");
					YYERROR;
				} 
				if (!rdr.af && !$11->host->ifindex)
					rdr.af = $11->host->af;

				n = ifa_pick_ips($11->host, rdr.af);
				if (n == NULL)
					YYERROR;
				if (!rdr.af)
					rdr.af = n->af;
				rdr.rport  = $11->rport.a;
				rdr.opts  |= $11->rport.t;
			}
			if ($11->host->next) {
				rdr.rpool.opts = $12.type;
				if (rdr.rpool.opts == PF_POOL_NONE) 
					rdr.rpool.opts = PF_POOL_ROUNDROBIN;
				if (rdr.rpool.opts != PF_POOL_ROUNDROBIN) {
					yyerror("nat: only round-robin valid "
					    "for multiple redirection "
					    "addresses");
					YYERROR;
				} 
			} else {
				if ((rdr.af == AF_INET &&
				    unmask(&$11->host->addr.mask, rdr.af) == 32) ||
				    (rdr.af == AF_INET6 &&
				    unmask(&$11->host->addr.mask, rdr.af) == 128)) {
					rdr.rpool.opts = PF_POOL_NONE;
				} else {
					if ($12.type == PF_POOL_NONE) 
						rdr.rpool.opts =
						    PF_POOL_ROUNDROBIN;
					else
						rdr.rpool.opts = $12.type;
				}
			}
			if ((rdr.rpool.opts & PF_POOL_TYPEMASK) ==
			    PF_POOL_SRCKEYHASH) {
				memcpy(&rdr.rpool.key, $12.key,
				    sizeof(struct pf_poolhashkey));
			}

			expand_rdr(&rdr, $3, $5, $7, $9, $11->host);
		}
		;

dport		: /* empty */			{
			$$.a = $$.b = $$.t = 0;
		}
		| PORT STRING			{
			char *p = strchr($2, ':');

			if (p == NULL) {
				if (($$.a = getservice($2)) == -1)
					YYERROR;
				$$.b = $$.t = 0;
			} else {
				*p++ = 0;
				if (($$.a = getservice($2)) == -1 ||
				    ($$.b = getservice(p)) == -1)
					YYERROR;
				$$.t = PF_DPORT_RANGE;
			}
		}
		;

route_host	: '(' STRING ')'		{ 
			if (($$->ifname = strdup($2)) == NULL) {
				yyerror("routeto: strdup");
				YYERROR;
			}
			if (ifa_exists($$->ifname) == NULL) {
				yyerror("routeto: unknown interface %s",
				    $$->ifname);
				YYERROR;
			}
		}
		| '(' STRING host ')'		{ 
			$$ = $3;
			if (($$->ifname = strdup($2)) == NULL) {
				yyerror("routeto: strdup");
				YYERROR;
			}
			if (ifa_exists($$->ifname) == NULL) {
				yyerror("routeto: unknown interface %s",
				    $$->ifname);
				YYERROR;
			}
		}
		;

route_host_list	: route_host				{ $$ = $1; }
		| route_host_list comma route_host	{
			if ($1->af == 0) 
				$1->af = $3->af;
			if ($1->af != $3->af) {
				yyerror("all pool addresses must be in the "
				    "same address family");
				YYERROR;
			} 
			/* $3 may be a list, so use its tail pointer */
			$1->tail->next = $3->tail;
			$1->tail = $3->tail;
			$$ = $1;
		}
		;

routespec	: route_host			{ $$ = $1; }
		| '{' route_host_list '}'	{ $$ = $2; }
		;


route		: /* empty */			{
			$$.host = NULL;
			$$.rt = 0;
			$$.pool_opts = 0;
		}
		| FASTROUTE {
			$$.host = NULL;
			$$.rt = PF_FASTROUTE;
			$$.pool_opts = 0;
		}
		| ROUTETO routespec pooltype {
			$$.host = $2;
			$$.rt = PF_ROUTETO;
			
			if ($2->next) {
				$$.pool_opts = $3.type;
				if ($$.pool_opts == PF_POOL_NONE) 
					$$.pool_opts = PF_POOL_ROUNDROBIN;
				if ($$.pool_opts != PF_POOL_ROUNDROBIN) {
					yyerror("nat: only round-robin valid "
					    "for multiple redirection "
					    "addresses");
					YYERROR;
				} 
			} else {
				if (($2->af == AF_INET &&
				    unmask(&$2->addr.mask, $2->af) == 32) ||
				    ($2->af == AF_INET6 &&
				    unmask(&$2->addr.mask, $2->af) == 128)) {
					$$.pool_opts = PF_POOL_NONE;
				} else {
					if ($3.type == PF_POOL_NONE) 
						$$.pool_opts =
						    PF_POOL_ROUNDROBIN;
					else
						$$.pool_opts = $3.type;
				}
			}
			if (($$.pool_opts & PF_POOL_TYPEMASK) ==
			    PF_POOL_SRCKEYHASH) {
				$$.key = $3.key;
			}
		}
		| REPLYTO routespec pooltype {
			$$.host = $2;
			$$.rt = PF_REPLYTO;
			
			if ($2->next) {
				$$.pool_opts = $3.type;
				if ($$.pool_opts == PF_POOL_NONE) 
					$$.pool_opts = PF_POOL_ROUNDROBIN;
				if ($$.pool_opts != PF_POOL_ROUNDROBIN) {
					yyerror("nat: only round-robin valid "
					    "for multiple redirection "
					    "addresses");
					YYERROR;
				} 
			} else {
				if (($2->af == AF_INET &&
				    unmask(&$2->addr.mask, $2->af) == 32) ||
				    ($2->af == AF_INET6 &&
				    unmask(&$2->addr.mask, $2->af) == 128)) {
					$$.pool_opts = PF_POOL_NONE;
				} else {
					if ($3.type == PF_POOL_NONE) 
						$$.pool_opts =
						    PF_POOL_ROUNDROBIN;
					else
						$$.pool_opts = $3.type;
				}
			}
			if (($$.pool_opts & PF_POOL_TYPEMASK) ==
			    PF_POOL_SRCKEYHASH) {
				$$.key = $3.key;
			}
		}
		| DUPTO routespec pooltype {
			$$.host = $2;
			$$.rt = PF_DUPTO;
			
			if ($2->next) {
				$$.pool_opts = $3.type;
				if ($$.pool_opts == PF_POOL_NONE) 
					$$.pool_opts = PF_POOL_ROUNDROBIN;
				if ($$.pool_opts != PF_POOL_ROUNDROBIN) {
					yyerror("nat: only round-robin valid "
					    "for multiple redirection "
					    "addresses");
					YYERROR;
				} 
			} else {
				if (($2->af == AF_INET &&
				    unmask(&$2->addr.mask, $2->af) == 32) ||
				    ($2->af == AF_INET6 &&
				    unmask(&$2->addr.mask, $2->af) == 128)) {
					$$.pool_opts = PF_POOL_NONE;
				} else {
					if ($3.type == PF_POOL_NONE) 
						$$.pool_opts =
						    PF_POOL_ROUNDROBIN;
					else
						$$.pool_opts = $3.type;
				}
			}
			if (($$.pool_opts & PF_POOL_TYPEMASK) ==
			    PF_POOL_SRCKEYHASH) {
				$$.key = $3.key;
			}
		}
		;

timeout_spec	: STRING number
		{
			if (pf->opts & PF_OPT_VERBOSE)
				printf("set timeout %s %us\n", $1, $2);
			if (check_rulestate(PFCTL_STATE_OPTION))
				YYERROR;
			if (pfctl_set_timeout(pf, $1, $2) != 0) {
				yyerror("unknown timeout %s", $1);
				YYERROR;
			}
		}
		;

timeout_list	: timeout_list comma timeout_spec
		| timeout_spec
		;

limit_spec	: STRING number
		{
			if (pf->opts & PF_OPT_VERBOSE)
				printf("set limit %s %u\n", $1, $2);
			if (check_rulestate(PFCTL_STATE_OPTION))
				YYERROR;
			if (pfctl_set_limit(pf, $1, $2) != 0) {
				yyerror("unable to set limit %s %u", $1, $2);
				YYERROR;
			}
		}

limit_list	: limit_list comma limit_spec
		| limit_spec
		;

comma		: ','
		| /* empty */
		;

%%

int
yyerror(char *fmt, ...)
{
	va_list ap;
	extern char *infile;
	errors = 1;

	va_start(ap, fmt);
	fprintf(stderr, "%s:%d: ", infile, yylval.lineno);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	return (0);
}

int
rule_consistent(struct pf_rule *r)
{
	int problems = 0;

	if (r->proto != IPPROTO_TCP && r->proto != IPPROTO_UDP &&
	    (r->src.port_op || r->dst.port_op)) {
		yyerror("port only applies to tcp/udp");
		problems++;
	}
	if (r->proto != IPPROTO_ICMP && r->proto != IPPROTO_ICMPV6 &&
	    (r->type || r->code)) {
		yyerror("icmp-type/code only applies to icmp");
		problems++;
	}
	if (!r->af && (r->type || r->code)) {
		yyerror("must indicate address family with icmp-type/code");
		problems++;
	}
	if ((r->proto == IPPROTO_ICMP && r->af == AF_INET6) ||
	    (r->proto == IPPROTO_ICMPV6 && r->af == AF_INET)) {
		yyerror("icmp version does not match address family");
		problems++;
	}
	if (r->keep_state == PF_STATE_MODULATE && r->proto &&
	    r->proto != IPPROTO_TCP) {
		yyerror("modulate state can only be applied to TCP rules");
		problems++;
	}
	if (r->allow_opts && r->action != PF_PASS) {
		yyerror("allow-opts can only be specified for pass rules");
		problems++;
	}
	if (!r->af && (r->src.addr.addr_dyn != NULL ||
	    r->dst.addr.addr_dyn != NULL)) {
		yyerror("dynamic addresses require address family (inet/inet6)");
		problems++;
	}
	if (r->rule_flag & PFRULE_FRAGMENT && (r->src.port_op ||
	    r->dst.port_op || r->flagset || r->type || r->code)) {
		yyerror("fragments can be filtered only on IP header fields");
		problems++;
	}
	if (r->rule_flag & PFRULE_RETURNRST && r->proto != IPPROTO_TCP) {
		yyerror("return-rst can only be applied to TCP rules");
		problems++;
	}
	if (r->action == PF_DROP && r->keep_state) {
		yyerror("keep state on block rules doesn't make sense");
		problems++;
	}
	return (-problems);
}

int
nat_consistent(struct pf_nat *r)
{
	int problems = 0;
	struct pf_pooladdr *pa;

	if (!r->af) { 
		TAILQ_FOREACH(pa, &r->rpool.list, entries) {
			if (pa->addr.addr_dyn != NULL) {
				yyerror("dynamic addresses require "
				    "address family (inet/inet6)");
				problems++;
				break;
			}
		}
	}
	return (-problems);
}

int
rdr_consistent(struct pf_rdr *r)
{
	int problems = 0;
	struct pf_pooladdr *pa;

	if (r->proto != IPPROTO_TCP && r->proto != IPPROTO_UDP &&
	    (r->dport || r->dport2 || r->rport)) {
		yyerror("port only applies to tcp/udp");
		problems++;
	}
	if (!r->af) {
		if (r->saddr.addr_dyn != NULL || r->daddr.addr_dyn != NULL) {
			yyerror("dynamic addresses require address family "
			     "(inet/inet6)");
			problems++;
		} else {
			TAILQ_FOREACH(pa, &r->rpool.list, entries) {
				if (pa->addr.addr_dyn != NULL) {
					yyerror("dynamic addresses require "
					    "address family (inet/inet6)");
					problems++;
					break;
				}
			}
		}
	}
	return (-problems);
}

struct keywords {
	const char	*k_name;
	int	 k_val;
};

/* macro gore, but you should've seen the prior indentation nightmare... */

#define FREE_LIST(T,r) \
	do { \
		T *p, *n = r; \
		while (n != NULL) { \
			p = n; \
			n = n->next; \
			free(p); \
		} \
	} while (0)

#define LOOP_THROUGH(T,n,r,C) \
	do { \
		T *n; \
		if (r == NULL) { \
			r = calloc(1, sizeof(T)); \
			if (r == NULL) \
				err(1, "LOOP: calloc"); \
			r->next = NULL; \
		} \
		n = r; \
		while (n != NULL) { \
			do { \
				C; \
			} while (0); \
			n = n->next; \
		} \
	} while (0)

void
expand_label_if(const char *name, char *label, const char *ifname)
{
	char tmp[PF_RULE_LABEL_SIZE];
	char *p;

	while ((p = strstr(label, name)) != NULL) {
		tmp[0] = 0;
		strlcat(tmp, label, p-label+1);
		if (!*ifname)
			strlcat(tmp, "any", PF_RULE_LABEL_SIZE);
		else
			strlcat(tmp, ifname, PF_RULE_LABEL_SIZE);
		strlcat(tmp, p+strlen(name), PF_RULE_LABEL_SIZE);
		strncpy(label, tmp, PF_RULE_LABEL_SIZE);
	}
}

void
expand_label_addr(const char *name, char *label, sa_family_t af,
    struct node_host *host)
{
	char tmp[PF_RULE_LABEL_SIZE];
	char *p;

	while ((p = strstr(label, name)) != NULL) {
		tmp[0] = 0;

		strlcat(tmp, label, p-label+1);

		if (host->not)
			strlcat(tmp, "! ", PF_RULE_LABEL_SIZE);
		if (host->addr.addr_dyn != NULL) {
			strlcat(tmp, "(", PF_RULE_LABEL_SIZE);
			strlcat(tmp, host->addr.addr.pfa.ifname,
			    PF_RULE_LABEL_SIZE);
			strlcat(tmp, ")", PF_RULE_LABEL_SIZE);
		} else if (!af || (PF_AZERO(&host->addr.addr, af) &&
		    PF_AZERO(&host->addr.mask, af)))
			strlcat(tmp, "any", PF_RULE_LABEL_SIZE);
		else {
			char a[48];
			int bits;

			if (inet_ntop(af, &host->addr.addr, a,
			    sizeof(a)) == NULL)
				strlcat(a, "?", sizeof(a));
			strlcat(tmp, a, PF_RULE_LABEL_SIZE);
			bits = unmask(&host->addr.mask, af);
			a[0] = 0;
			if ((af == AF_INET && bits < 32) ||
			    (af == AF_INET6 && bits < 128))
				snprintf(a, sizeof(a), "/%d", bits);
			strlcat(tmp, a, PF_RULE_LABEL_SIZE);
		}
		strlcat(tmp, p+strlen(name), PF_RULE_LABEL_SIZE);
		strncpy(label, tmp, PF_RULE_LABEL_SIZE);
	}
}

void
expand_label_port(const char *name, char *label, struct node_port *port)
{
	char tmp[PF_RULE_LABEL_SIZE];
	char *p;
	char a1[6], a2[6], op[13];

	while ((p = strstr(label, name)) != NULL) {
		tmp[0] = 0;

		strlcat(tmp, label, p-label+1);

		snprintf(a1, sizeof(a1), "%u", ntohs(port->port[0]));
		snprintf(a2, sizeof(a2), "%u", ntohs(port->port[1]));
		if (!port->op)
			op[0] = 0;
		else if (port->op == PF_OP_IRG)
			snprintf(op, sizeof(op), "%s><%s", a1, a2);
		else if (port->op == PF_OP_XRG)
			snprintf(op, sizeof(op), "%s<>%s", a1, a2);
		else if (port->op == PF_OP_EQ)
			snprintf(op, sizeof(op), "%s", a1);
		else if (port->op == PF_OP_NE)
			snprintf(op, sizeof(op), "!=%s", a1);
		else if (port->op == PF_OP_LT)
			snprintf(op, sizeof(op), "<%s", a1);
		else if (port->op == PF_OP_LE)
			snprintf(op, sizeof(op), "<=%s", a1);
		else if (port->op == PF_OP_GT)
			snprintf(op, sizeof(op), ">%s", a1);
		else if (port->op == PF_OP_GE)
			snprintf(op, sizeof(op), ">=%s", a1);
		strlcat(tmp, op, PF_RULE_LABEL_SIZE);
		strlcat(tmp, p+strlen(name), PF_RULE_LABEL_SIZE);
		strncpy(label, tmp, PF_RULE_LABEL_SIZE);
	}
}

void
expand_label_proto(const char *name, char *label, u_int8_t proto)
{
	char tmp[PF_RULE_LABEL_SIZE];
	char *p;
	struct protoent *pe;

	while ((p = strstr(label, name)) != NULL) {
		tmp[0] = 0;
		strlcat(tmp, label, p-label+1);
		pe = getprotobynumber(proto);
		if (pe != NULL)
		    strlcat(tmp, pe->p_name, PF_RULE_LABEL_SIZE);
		else
		    snprintf(tmp+strlen(tmp), PF_RULE_LABEL_SIZE-strlen(tmp),
			"%u", proto);
		strlcat(tmp, p+strlen(name), PF_RULE_LABEL_SIZE);
		strncpy(label, tmp, PF_RULE_LABEL_SIZE);
	}
}

void
expand_label_nr(const char *name, char *label)
{
	char tmp[PF_RULE_LABEL_SIZE];
	char *p;

	while ((p = strstr(label, name)) != NULL) {
		tmp[0] = 0;
		strlcat(tmp, label, p-label+1);
		snprintf(tmp+strlen(tmp), PF_RULE_LABEL_SIZE-strlen(tmp),
		    "%u", pf->rule_nr);
		strlcat(tmp, p+strlen(name), PF_RULE_LABEL_SIZE);
		strncpy(label, tmp, PF_RULE_LABEL_SIZE);
	}
}

void
expand_label(char *label, const char *ifname, sa_family_t af,
    struct node_host *src_host, struct node_port *src_port,
    struct node_host *dst_host, struct node_port *dst_port,
    u_int8_t proto)
{
	expand_label_if("$if", label, ifname);
	expand_label_addr("$srcaddr", label, af, src_host);
	expand_label_addr("$dstaddr", label, af, dst_host);
	expand_label_port("$srcport", label, src_port);
	expand_label_port("$dstport", label, dst_port);
	expand_label_proto("$proto", label, proto);
	expand_label_nr("$nr", label);
}

void
expand_altq(struct pf_altq *a, struct node_if *interfaces,
    struct node_queue *nqueues)
{
	struct	pf_altq pa, pb;
	char	qname[PF_QNAME_SIZE];
	struct	node_queue *n;

	LOOP_THROUGH(struct node_if, interface, interfaces,
		memcpy(&pa, a, sizeof(struct pf_altq));
		strlcpy(pa.ifname, interface->ifname, IFNAMSIZ);

		if (interface->not)
			yyerror("altq on ! <interface> is not supported");
		else {
			eval_pfaltq(pf, &pa);
			pfctl_add_altq(pf, &pa);
			if (pf->opts & PF_OPT_VERBOSE) {
				print_altq(&pf->paltq->altq, 0);
				if (nqueues && nqueues->tail) {
					printf(" queue { ");
					LOOP_THROUGH(struct node_queue, queue,
					    nqueues,
						printf("%s ", queue->queue);
					);
					printf("}");
				}
				printf("\n");
			}

			/* now create a root queue */
			memset(&pb, 0, sizeof(struct pf_altq));
			strlcpy(qname, "root_", sizeof(qname));
			strlcat(qname, interface->ifname, sizeof(qname));
			strlcpy(pb.qname, qname, PF_QNAME_SIZE);
			strlcpy(pb.ifname, interface->ifname, IFNAMSIZ);
			pb.qlimit = pa.qlimit;
			pb.scheduler = pa.scheduler;
			pb.pq_u.cbq_opts.flags = pa.pq_u.cbq_opts.flags;
			eval_pfqueue(pf, &pb, pa.ifbandwidth, 0);
			pfctl_add_altq(pf, &pb);

			LOOP_THROUGH(struct node_queue, queue, nqueues,
				n = calloc(1, sizeof(struct node_queue));
				if (n == NULL)
					err(1, "expand_altq: malloc");
				strlcpy(n->parent, qname, PF_QNAME_SIZE);
				strlcpy(n->queue, queue->queue, PF_QNAME_SIZE);
				strlcpy(n->ifname, interface->ifname, IFNAMSIZ);
				n->next = NULL;
				n->tail = n;
				if (queues == NULL)
					queues = n;
				else {
					queues->tail->next = n;
					queues->tail = n;
				}
			);
		}
	);
	FREE_LIST(struct node_if, interfaces);
	FREE_LIST(struct node_queue, nqueues);
}

int
expand_queue(struct pf_altq *a, struct node_queue *nqueues,
    struct node_queue_bw bwspec)
{
	struct	node_queue *n;
	u_int8_t	added = 0;

	LOOP_THROUGH(struct node_queue, tqueue, queues,
		if (!strncmp(a->qname, tqueue->queue, PF_QNAME_SIZE)) {
			/* found ourselve in queues */
			LOOP_THROUGH(struct node_queue, queue, nqueues,
				n = malloc(sizeof(struct node_queue));
				if (n == NULL)
					err(1, "expand_queue: malloc");
				strlcpy(n->parent, a->qname, PF_QNAME_SIZE);
				strlcpy(n->queue, queue->queue, PF_QNAME_SIZE);
				strlcpy(n->ifname, tqueue->ifname, IFNAMSIZ);
				n->next = NULL;
				n->tail = n;
				if (queues == NULL)
					queues = n;
				else {
					queues->tail->next = n;
					queues->tail = n;
				}
			);
			strlcpy(a->ifname, tqueue->ifname, IFNAMSIZ);
			strlcpy(a->parent, tqueue->parent, PF_QNAME_SIZE);

			eval_pfqueue(pf, a, bwspec.bw_absolute,
			    bwspec.bw_percent);
			pfctl_add_altq(pf, a);
			if (pf->opts & PF_OPT_VERBOSE) {
				print_altq(&pf->paltq->altq, 0);
				if (nqueues && nqueues->tail) {
					printf(" { ");
					LOOP_THROUGH(struct node_queue, queue,
					    nqueues,
						printf("%s ", queue->queue);
					);
					printf("}");
				}
				printf("\n");
			}
			added++;
			FREE_LIST(struct node_queue, nqueues);
		}
	);

	if (!added) {
		yyerror("queue has no parent");
		return (1);
	} else
		return (0);
}

void
expand_rule(struct pf_rule *r,
    struct node_if *interfaces, struct node_host *rt_pool_hosts,
    struct node_proto *protos, struct node_host *src_hosts,
    struct node_port *src_ports, struct node_host *dst_hosts,
    struct node_port *dst_ports, struct node_uid *uids,
    struct node_gid *gids, struct node_icmp *icmp_types)
{
	sa_family_t	af = r->af;
	int added = 0, error = 0;
	char	ifname[IF_NAMESIZE];
	char	label[PF_RULE_LABEL_SIZE];
	struct pf_pooladdr *pa;
	struct node_host *h;
	char	qname[PF_QNAME_SIZE];
	u_int8_t flags, flagset;

	strlcpy(label, r->label, sizeof(label));
	strlcpy(qname, r->qname, sizeof(qname));
	flags = r->flags;
	flagset = r->flagset;

	LOOP_THROUGH(struct node_if, interface, interfaces,
	LOOP_THROUGH(struct node_proto, proto, protos,
	LOOP_THROUGH(struct node_icmp, icmp_type, icmp_types,
	LOOP_THROUGH(struct node_host, src_host, src_hosts,
	LOOP_THROUGH(struct node_port, src_port, src_ports,
	LOOP_THROUGH(struct node_host, dst_host, dst_hosts,
	LOOP_THROUGH(struct node_port, dst_port, dst_ports,
	LOOP_THROUGH(struct node_uid, uid, uids,
	LOOP_THROUGH(struct node_gid, gid, gids,

		r->af = af;
		/* for link-local IPv6 address, interface must match up */
		if ((r->af && src_host->af && r->af != src_host->af) ||
		    (r->af && dst_host->af && r->af != dst_host->af) ||
		    (src_host->af && dst_host->af &&
		    src_host->af != dst_host->af) ||
		    (src_host->ifindex && dst_host->ifindex &&
		    src_host->ifindex != dst_host->ifindex) ||
		    (src_host->ifindex && if_nametoindex(interface->ifname) &&
		    src_host->ifindex != if_nametoindex(interface->ifname)) ||
		    (dst_host->ifindex && if_nametoindex(interface->ifname) &&
		    dst_host->ifindex != if_nametoindex(interface->ifname)))
			continue;
		if (!r->af && src_host->af)
			r->af = src_host->af;
		else if (!r->af && dst_host->af)
			r->af = dst_host->af;

		if (if_indextoname(src_host->ifindex, ifname))
			memcpy(r->ifname, ifname, sizeof(r->ifname));
		else if (if_indextoname(dst_host->ifindex, ifname))
			memcpy(r->ifname, ifname, sizeof(r->ifname));
		else
			memcpy(r->ifname, interface->ifname, sizeof(r->ifname));

		strlcpy(r->label, label, PF_RULE_LABEL_SIZE);
		expand_label(r->label, r->ifname, r->af, src_host, src_port,
		    dst_host, dst_port, proto->proto);
		strlcpy(r->qname, qname, PF_QNAME_SIZE);
		r->qid = qname_to_qid(qname, r->ifname);
		r->ifnot = interface->not;
		r->proto = proto->proto;
		r->src.addr = src_host->addr;
		r->src.noroute = src_host->noroute;
		r->src.not = src_host->not;
		r->src.port[0] = src_port->port[0];
		r->src.port[1] = src_port->port[1];
		r->src.port_op = src_port->op;
		r->dst.addr = dst_host->addr;
		r->dst.noroute = dst_host->noroute;
		r->dst.not = dst_host->not;
		r->dst.port[0] = dst_port->port[0];
		r->dst.port[1] = dst_port->port[1];
		r->dst.port_op = dst_port->op;
		r->uid.op = uid->op;
		r->uid.uid[0] = uid->uid[0];
		r->uid.uid[1] = uid->uid[1];
		r->gid.op = gid->op;
		r->gid.gid[0] = gid->gid[0];
		r->gid.gid[1] = gid->gid[1];
		r->type = icmp_type->type;
		r->code = icmp_type->code;

		if (r->proto && r->proto != IPPROTO_TCP) {
			r->flags = 0;
			r->flagset = 0;
		} else {
			r->flags = flags;
			r->flagset = flagset;
		}
		if (icmp_type->proto && r->proto != icmp_type->proto) {
			yyerror("icmp-type mismatch");
			error++;
		}

		TAILQ_INIT(&r->rt_pool.list);
		for (h = rt_pool_hosts; h; h = h->next) {
			pa = malloc(sizeof(struct pf_pooladdr));
			if (pa == NULL) {
				yyerror("malloc");
				error++;
			}
			pa->addr = h->addr;
			if (h->ifname != NULL)
				strncpy(pa->ifname, h->ifname, IFNAMSIZ);
			else
				pa->ifname[0] = 0;
			TAILQ_INSERT_TAIL(&r->rt_pool.list, pa, entries);
		}

		if (rule_consistent(r) < 0 || error)
			yyerror("skipping filter rule due to errors");
		else {
			r->nr = pf->rule_nr++;
			pfctl_add_rule(pf, r);
			added++;
		}

	)))))))));

	FREE_LIST(struct node_if, interfaces);
	FREE_LIST(struct node_proto, protos);
	FREE_LIST(struct node_host, src_hosts);
	FREE_LIST(struct node_port, src_ports);
	FREE_LIST(struct node_host, dst_hosts);
	FREE_LIST(struct node_port, dst_ports);
	FREE_LIST(struct node_uid, uids);
	FREE_LIST(struct node_gid, gids);
	FREE_LIST(struct node_icmp, icmp_types);
	FREE_LIST(struct node_host, rt_pool_hosts);

	if (!added)
		yyerror("rule expands to no valid combination");
}

void
expand_nat(struct pf_nat *n,
    struct node_if *interfaces, struct node_proto *protos,
    struct node_host *src_hosts, struct node_port *src_ports,
    struct node_host *dst_hosts, struct node_port *dst_ports,
    struct node_host *rpool_hosts)
{
	char ifname[IF_NAMESIZE];
	struct pf_pooladdr *pa;
	struct node_host *h;
	sa_family_t af = n->af;
	int added = 0, error = 0;

	LOOP_THROUGH(struct node_if, interface, interfaces,
	LOOP_THROUGH(struct node_proto, proto, protos,
	LOOP_THROUGH(struct node_host, src_host, src_hosts,
	LOOP_THROUGH(struct node_port, src_port, src_ports,
	LOOP_THROUGH(struct node_host, dst_host, dst_hosts,
	LOOP_THROUGH(struct node_port, dst_port, dst_ports,

		n->af = af;
		/* for link-local IPv6 address, interface must match up */
		if ((n->af && src_host->af && n->af != src_host->af) ||
		    (n->af && dst_host->af && n->af != dst_host->af) ||
		    (src_host->af && dst_host->af &&
		    src_host->af != dst_host->af) ||
		    (src_host->ifindex && dst_host->ifindex &&
		    src_host->ifindex != dst_host->ifindex) ||
		    (src_host->ifindex && if_nametoindex(interface->ifname) &&
		    src_host->ifindex != if_nametoindex(interface->ifname)) ||
		    (dst_host->ifindex && if_nametoindex(interface->ifname) &&
		    dst_host->ifindex != if_nametoindex(interface->ifname)))
			continue;
		if (!n->af && src_host->af)
			n->af = src_host->af;
		else if (!n->af && dst_host->af)
			n->af = dst_host->af;

		if (if_indextoname(src_host->ifindex, ifname))
			memcpy(n->ifname, ifname, sizeof(n->ifname));
		else if (if_indextoname(dst_host->ifindex, ifname))
			memcpy(n->ifname, ifname, sizeof(n->ifname));
		else
			memcpy(n->ifname, interface->ifname, sizeof(n->ifname));

		n->ifnot = interface->not;
		n->proto = proto->proto;
		n->src.addr = src_host->addr;
		n->src.noroute = src_host->noroute;
		n->src.not = src_host->not;
		n->src.port[0] = src_port->port[0];
		n->src.port[1] = src_port->port[1];
		n->src.port_op = src_port->op;
		n->dst.addr = dst_host->addr;
		n->dst.noroute = dst_host->noroute;
		n->dst.not = dst_host->not;
		n->dst.port[0] = dst_port->port[0];
		n->dst.port[1] = dst_port->port[1];
		n->dst.port_op = dst_port->op;

		TAILQ_INIT(&n->rpool.list);
		for (h = rpool_hosts; h; h = h->next) {
			pa = malloc(sizeof(struct pf_pooladdr));
			if (pa == NULL) {
				yyerror("malloc");
				error++;
			}
			pa->addr = h->addr;
			pa->ifname[0] = 0;
			TAILQ_INSERT_TAIL(&n->rpool.list, pa, entries);
		}

		TAILQ_INIT(&n->rpool.list);
		for (h = rpool_hosts; h; h = h->next) {
			pa = malloc(sizeof(struct pf_pooladdr));
			if (pa == NULL) {
				yyerror("malloc");
				error++;
			}
			pa->addr = h->addr;
			pa->ifname[0] = 0;
			TAILQ_INSERT_TAIL(&n->rpool.list, pa, entries);
		}

		if (nat_consistent(n) < 0 || error)
			yyerror("skipping nat rule due to errors");
		else {  
			pfctl_add_nat(pf, n);
			added++;
		}

	))))));

	FREE_LIST(struct node_if, interfaces);
	FREE_LIST(struct node_proto, protos);
	FREE_LIST(struct node_host, src_hosts);
	FREE_LIST(struct node_port, src_ports);
	FREE_LIST(struct node_host, dst_hosts);
	FREE_LIST(struct node_port, dst_ports);
	FREE_LIST(struct node_host, rpool_hosts);

	if (!added)
		yyerror("nat rule expands to no valid combinations");
}

void
expand_rdr(struct pf_rdr *r, struct node_if *interfaces,
    struct node_proto *protos, struct node_host *src_hosts,
    struct node_host *dst_hosts, struct node_host *rpool_hosts)
{
	sa_family_t af = r->af;
	int added = 0, error = 0;
	char ifname[IF_NAMESIZE];
	struct pf_pooladdr *pa;
	struct node_host *h;

	LOOP_THROUGH(struct node_if, interface, interfaces,
	LOOP_THROUGH(struct node_proto, proto, protos,
	LOOP_THROUGH(struct node_host, src_host, src_hosts,
	LOOP_THROUGH(struct node_host, dst_host, dst_hosts,

		r->af = af;
		if ((r->af && src_host->af && r->af != src_host->af) ||
		    (r->af && dst_host->af && r->af != dst_host->af) ||
		    (src_host->af && dst_host->af &&
		    src_host->af != dst_host->af) ||
		    (src_host->ifindex && dst_host->ifindex &&
		    src_host->ifindex != dst_host->ifindex) ||
		    (src_host->ifindex && if_nametoindex(interface->ifname) &&
		    src_host->ifindex != if_nametoindex(interface->ifname)) ||
		    (dst_host->ifindex && if_nametoindex(interface->ifname) &&
		    dst_host->ifindex != if_nametoindex(interface->ifname)))
			continue;

		if (!r->af && src_host->af)
			r->af = src_host->af;
		else if (!r->af && dst_host->af)
			r->af = dst_host->af;

		if (if_indextoname(src_host->ifindex, ifname))
			memcpy(r->ifname, ifname, sizeof(r->ifname));
		else if (if_indextoname(dst_host->ifindex, ifname))
			memcpy(r->ifname, ifname, sizeof(r->ifname));
		else
			memcpy(r->ifname, interface->ifname, sizeof(r->ifname));

		r->proto = proto->proto;
		r->ifnot = interface->not;
		r->saddr = src_host->addr;
		r->daddr = dst_host->addr;

		TAILQ_INIT(&r->rpool.list);
		for (h = rpool_hosts; h; h = h->next) {
			pa = malloc(sizeof(struct pf_pooladdr));
			if (pa == NULL) {
				yyerror("malloc");
				error++;
			}
			pa->addr = h->addr;
			pa->ifname[0] = 0;
			TAILQ_INSERT_TAIL(&r->rpool.list, pa, entries);
		}

		if (rdr_consistent(r) < 0 || error)
			yyerror("skipping rdr rule due to errors");
		else {
			pfctl_add_rdr(pf, r);
			added++;
		}

	))));

	FREE_LIST(struct node_if, interfaces);
	FREE_LIST(struct node_proto, protos);
	FREE_LIST(struct node_host, src_hosts);
	FREE_LIST(struct node_host, dst_hosts);
	FREE_LIST(struct node_host, rpool_hosts);

	if (!added)
		yyerror("rdr rule expands to no valid combination");
}

#undef FREE_LIST
#undef LOOP_THROUGH

int
check_rulestate(int desired_state)
{
	if (require_order && (rulestate > desired_state)) {
		yyerror("Rules must be in order: options, normalization, "
		    "translation, filter");
		return (1);
	}
	rulestate = desired_state;
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
		{ "all",	ALL},
		{ "allow-opts",	ALLOWOPTS},
		{ "altq",	ALTQ},
		{ "antispoof",	ANTISPOOF},
		{ "any",	ANY},
		{ "bandwidth",	BANDWIDTH},
		{ "binat",	BINAT},
		{ "bitmask", 	BITMASK},
		{ "block",	BLOCK},
		{ "block-policy", BLOCKPOLICY},
		{ "borrow",	BORROW},
		{ "cbq",	CBQ},
		{ "code",	CODE},
		{ "control",	CONTROL},
		{ "crop",	FRAGCROP},
		{ "default",	DEFAULT},
		{ "drop",	DROP},
		{ "drop-ovl",	FRAGDROP},
		{ "dup-to",	DUPTO},
		{ "ecn",	ECN},
		{ "fastroute",	FASTROUTE},
		{ "flags",	FLAGS},
		{ "for",	FOR},
		{ "fragment",	FRAGMENT},
		{ "from",	FROM},
		{ "group",	GROUP},
		{ "icmp-type",	ICMPTYPE},
		{ "in",		IN},
		{ "inet",	INET},
		{ "inet6",	INET6},
		{ "ipv6-icmp-type", ICMP6TYPE},
		{ "keep",	KEEP},
		{ "key",	KEY},
		{ "label",	LABEL},
		{ "limit",	LIMIT},
		{ "log",	LOG},
		{ "log-all",	LOGALL},
		{ "loginterface", LOGINTERFACE},
		{ "max",	MAXIMUM},
		{ "max-mss",	MAXMSS},
		{ "min-ttl",	MINTTL},
		{ "modulate",	MODULATE},
		{ "nat",	NAT},
		{ "no",		NO},
		{ "no-df",	NODF},
		{ "no-route",	NOROUTE},
		{ "on",		ON},
		{ "optimization", OPTIMIZATION},
		{ "out",	OUT},
		{ "pass",	PASS},
		{ "port",	PORT},
		{ "priority",	PRIORITY},
		{ "proto",	PROTO},
		{ "qlimit",	QLIMIT},
		{ "queue",	QUEUE},
		{ "quick",	QUICK},
		{ "random",	RANDOM},
		{ "rdr",	RDR},
		{ "reassemble",	FRAGNORM},
		{ "red",	RED},
		{ "reply-to",	REPLYTO},
		{ "require-order", REQUIREORDER},
		{ "return",	RETURN},
		{ "return-icmp",RETURNICMP},
		{ "return-icmp6",RETURNICMP6},
		{ "return-rst",	RETURNRST},
		{ "rio",	RIO},
		{ "round-robin",ROUNDROBIN},
		{ "route-to",	ROUTETO},
		{ "scheduler",	SCHEDULER},
		{ "scrub",	SCRUB},
		{ "set",	SET},
		{ "source-hash",SOURCEHASH},
		{ "state",	STATE},
		{ "tbrsize",	TBRSIZE},
		{ "timeout",	TIMEOUT},
		{ "to",		TO},
		{ "tos",	TOS},
		{ "ttl",	TTL},
		{ "user",	USER},
		{ "yes",	YES},
	};
	const struct keywords *p;

	p = bsearch(s, keywords, sizeof(keywords)/sizeof(keywords[0]),
	    sizeof(keywords[0]), kw_cmp);

	if (p) {
		if (debug > 1)
			fprintf(stderr, "%s: %d\n", s, p->k_val);
		return (p->k_val);
	} else {
		if (debug > 1)
			fprintf(stderr, "string: %s\n", s);
		return (STRING);
	}
}

#define MAXPUSHBACK	128

char	*parsebuf;
int	parseindex;
char	pushback_buffer[MAXPUSHBACK];
int	pushback_index = 0;

int
lgetc(FILE *fin)
{
	int c, next;

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

	while ((c = getc(fin)) == '\\') {
		next = getc(fin);
		if (next != '\n') {
			if (isspace(next))
				yyerror("whitespace after \\");
			ungetc(next, fin);
			break;
		}
		yylval.lineno = lineno;
		lineno++;
	}
	if (c == '\t' || c == ' ') {
		/* Compress blanks to a single space. */
		do {
			c = getc(fin);
		} while (c == '\t' || c == ' ');
		ungetc(c, fin);
		c = ' ';
	}

	return (c);
}

int
lungetc(int c, FILE *fin)
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
	int c;

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
	char buf[8096], *p, *val;
	int endc, c, next;
	int token;

top:
	p = buf;
	while ((c = lgetc(fin)) == ' ')
		;

	yylval.lineno = lineno;
	if (c == '#')
		while ((c = lgetc(fin)) != '\n' && c != EOF)
			;
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
			lungetc(c, fin);
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
			err(1, "yylex: strdup");
		return (STRING);
	case '=':
		yylval.v.i = PF_OP_EQ;
		return (PORTUNARY);
	case '!':
		next = lgetc(fin);
		if (next == '=') {
			yylval.v.i = PF_OP_NE;
			return (PORTUNARY);
		}
		lungetc(next, fin);
		break;
	case '<':
		next = lgetc(fin);
		if (next == '>') {
			yylval.v.i = PF_OP_XRG;
			return (PORTBINARY);
		} else  if (next == '=') {
			yylval.v.i = PF_OP_LE;
		} else {
			yylval.v.i = PF_OP_LT;
			lungetc(next, fin);
		}
		return (PORTUNARY);
		break;
	case '>':
		next = lgetc(fin);
		if (next == '<') {
			yylval.v.i = PF_OP_IRG;
			return (PORTBINARY);
		} else  if (next == '=') {
			yylval.v.i = PF_OP_GE;
		} else {
			yylval.v.i = PF_OP_GT;
			lungetc(next, fin);
		}
		return (PORTUNARY);
		break;
	case '-':
		next = lgetc(fin);
		if (next == '>')
			return (ARROW);
		lungetc(next, fin);
		break;
	}

#define allowed_in_string(x) \
	(isalnum(x) || (ispunct(x) && x != '(' && x != ')' && \
	x != '{' && x != '}' && x != '<' && x != '>' && \
	x != '!' && x != '=' && x != '/' && x != '#' && \
	x != ','))

	if (isalnum(c) || c == ':') {
		do {
			*p++ = c;
			if ((unsigned)(p-buf) >= sizeof(buf)) {
				yyerror("string too long");
				return (findeol());
			}
		} while ((c = lgetc(fin)) != EOF && (allowed_in_string(c)));
		lungetc(c, fin);
		*p = '\0';
		token = lookup(buf);
		yylval.v.string = strdup(buf);
		if (yylval.v.string == NULL)
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
parse_rules(FILE *input, struct pfctl *xpf)
{
	struct sym *sym;

	fin = input;
	pf = xpf;
	lineno = 1;
	errors = 0;
	rulestate = PFCTL_STATE_NONE;
	yyparse();

	/* Check which macros have not been used. */
	for (sym = symhead; sym; sym = sym->next)
		if (!sym->used)
			fprintf(stderr, "warning: macro '%s' not used\n",
			    sym->nam);

	return (errors ? -1 : 0);
}

void
set_ipmask(struct node_host *h, u_int8_t b)
{
	struct pf_addr *m, *n;
	int i, j = 0;

	m = &h->addr.mask;

	for (i = 0; i < 4; i++)
		m->addr32[i] = 0;

	while (b >= 32) {
		m->addr32[j++] = 0xffffffff;
		b -= 32;
	}
	for (i = 31; i > 31-b; --i)
		m->addr32[j] |= (1 << i);
	if (b)
		m->addr32[j] = htonl(m->addr32[j]);

	/* Mask off bits of the address that will never be used. */
	n = &h->addr.addr;
	for (i = 0; i < 4; i++)
		n->addr32[i] = n->addr32[i] & m->addr32[i];
}

/*
 * Over-designed efficiency is a French and German concept, so how about
 * we wait until they discover this ugliness and make it all fancy.
 */
int
symset(const char *nam, const char *val)
{
	struct sym *sym;

	sym = calloc(1, sizeof(*sym));
	if (sym == NULL)
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
	sym->next = symhead;
	sym->used = 0;
	symhead = sym;
	return (0);
}

char *
symget(const char *nam)
{
	struct sym *sym;

	for (sym = symhead; sym; sym = sym->next)
		if (strcmp(nam, sym->nam) == 0) {
			sym->used = 1;
			return (sym->val);
		}
	return (NULL);
}

/* interface lookup routines */

struct node_host *iftab;

void
ifa_load(void)
{
	struct ifaddrs *ifap, *ifa;
	struct node_host *n = NULL, *h = NULL;

	if (getifaddrs(&ifap) < 0)
		err(1, "getifaddrs");

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (!(ifa->ifa_addr->sa_family == AF_INET ||
		    ifa->ifa_addr->sa_family == AF_INET6 ||
		    ifa->ifa_addr->sa_family == AF_LINK))
				continue;
		n = calloc(1, sizeof(struct node_host));
		if (n == NULL)
			err(1, "address: calloc");
		n->af = ifa->ifa_addr->sa_family;
		n->addr.addr_dyn = NULL;
		n->ifa_flags = ifa->ifa_flags;
#ifdef __KAME__
		if (n->af == AF_INET6 &&
		    IN6_IS_ADDR_LINKLOCAL(&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr) &&
		    ((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_scope_id == 0) {
			struct sockaddr_in6 *sin6;

			sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
			sin6->sin6_scope_id = sin6->sin6_addr.s6_addr[2] << 8 |
			    sin6->sin6_addr.s6_addr[3];
			sin6->sin6_addr.s6_addr[2] = 0;
			sin6->sin6_addr.s6_addr[3] = 0;
		}
#endif
		n->ifindex = 0;
		if (n->af == AF_INET) {
			memcpy(&n->addr.addr, &((struct sockaddr_in *)
			    ifa->ifa_addr)->sin_addr.s_addr,
			    sizeof(struct in_addr));
			memcpy(&n->addr.mask, &((struct sockaddr_in *)
			    ifa->ifa_netmask)->sin_addr.s_addr,
			    sizeof(struct in_addr));
			if (ifa->ifa_broadaddr != NULL)
				memcpy(&n->bcast, &((struct sockaddr_in *)
				    ifa->ifa_broadaddr)->sin_addr.s_addr,
				    sizeof(struct in_addr));
		} else if (n->af == AF_INET6) {
			memcpy(&n->addr.addr, &((struct sockaddr_in6 *)
			    ifa->ifa_addr)->sin6_addr.s6_addr,
			    sizeof(struct in6_addr));
			memcpy(&n->addr.mask, &((struct sockaddr_in6 *)
			    ifa->ifa_netmask)->sin6_addr.s6_addr,
			    sizeof(struct in6_addr));
			if (ifa->ifa_broadaddr != NULL)
				memcpy(&n->bcast, &((struct sockaddr_in6 *)
				    ifa->ifa_broadaddr)->sin6_addr.s6_addr,
				    sizeof(struct in6_addr));
			n->ifindex = ((struct sockaddr_in6 *)
			    ifa->ifa_addr)->sin6_scope_id;
		}
		if ((n->ifname = strdup(ifa->ifa_name)) == NULL) {
			yyerror("malloc failed");
			exit(1);
		}
		n->next = NULL;
		n->tail = n;
		if (h == NULL)
			h = n;
		else {
			h->tail->next = n;
			h->tail = n;
		}
	}
	iftab = h;
	freeifaddrs(ifap);
}

struct node_host *
ifa_exists(char *ifa_name)
{
	struct node_host *n;

	if (iftab == NULL)
		ifa_load();

	for (n = iftab; n; n = n->next) {
		if (n->af == AF_LINK && !strncmp(n->ifname, ifa_name, IFNAMSIZ))
			return (n);
	}
	return (NULL);
}

struct node_host *
ifa_lookup(char *ifa_name, enum pfctl_iflookup_mode mode)
{
	struct node_host *p = NULL, *h = NULL, *n = NULL;
	int return_all = 0;

	if (!strncmp(ifa_name, "self", IFNAMSIZ))
		return_all = 1;

	if (iftab == NULL)
		ifa_load();

	for (p = iftab; p; p = p->next) {
		if (!((p->af == AF_INET || p->af == AF_INET6) &&
		    (!strncmp(p->ifname, ifa_name, IFNAMSIZ) || return_all)))
			continue;
		if (mode == PFCTL_IFLOOKUP_BCAST && p->af != AF_INET)
			continue;
		if (mode == PFCTL_IFLOOKUP_NET && p->ifindex > 0)
			continue;
		n = calloc(1, sizeof(struct node_host));
		if (n == NULL)
			err(1, "address: calloc");
		n->af = p->af;
		n->addr.addr_dyn = NULL;
		if (mode == PFCTL_IFLOOKUP_BCAST) {
				memcpy(&n->addr.addr, &p->bcast,
				    sizeof(struct pf_addr));
		} else
			memcpy(&n->addr.addr, &p->addr.addr,
			    sizeof(struct pf_addr));
		if (mode == PFCTL_IFLOOKUP_NET)
			memcpy(&n->addr.mask, &p->addr.mask,
			    sizeof(struct pf_addr));
		else {
			if (n->af == AF_INET)
				set_ipmask(n, 32);
			else
				set_ipmask(n, 128);
		}
		n->ifindex = p->ifindex;

		n->next = NULL;
		n->tail = n;
		if (h == NULL)
			h = n;
		else {
			h->tail->next = n;
			h->tail = n;
		}
	}
	if (h == NULL && mode == PFCTL_IFLOOKUP_HOST) {
		yyerror("no IP address found for %s", ifa_name);
	}
	return (h);
}

struct node_host *
ifa_pick_ips(struct node_host *nh, sa_family_t af)
{
	struct node_host *d, *h = nh, *n = NULL, *ip = NULL, *ip6 = NULL;

	while (h != NULL) {
		switch (h->af) {
		case AF_INET:
			if (ip == NULL) {
				ip = h;
				ip->tail = ip;
			} else {
				ip->tail->next = h;
				ip->tail = h;
			}
			h = h->next;
			ip->tail->next = NULL;
			n = ip;
			break;
		case AF_INET6:
			if (ip6 == NULL) {
				ip6 = h;
				ip6->tail = ip6;
			} else {
				ip6->tail->next = h;
				ip6->tail = h;
			}
			h = h->next;
			ip6->tail->next = NULL;
			n = ip6;
			break;
		}
	}

	switch (af) {
	case AF_INET:
		n = ip;
		h = ip6;
		break;
	case AF_INET6:
		n = ip6;
		h = ip;
		break;
	default:
		if (ip && ip6) {
			yyerror("address family not given and translation "
			    "address expands to multiple address families");
			return (NULL);
		} 
		h = NULL;
		break;
	}
			
	if (n == NULL)
		yyerror("no translation address with matching address family "
		    "found.");

	while (h != NULL) {  
		d = h;
		h = h->next;
		free(d);
	}
	return (n);
}

struct node_host *
host(char *s, int mask)
{
	struct node_host *h = NULL, *n;
	struct in_addr ina;
	struct addrinfo hints, *res0, *res;
	int bits, error, v4mask, v6mask;
	char *buf = NULL;

	if (ifa_exists(s) || !strncmp(s, "self", IFNAMSIZ)) {
		/* interface with this name exists */
		h = ifa_lookup(s, PFCTL_IFLOOKUP_HOST);
		if (h != NULL && mask > -1)
			set_ipmask(h, mask);
		return (h);
	}

	if (mask == -1) {
		if (asprintf(&buf, "%s", s) == -1)
			err(1, "host: malloc");
		v4mask = 32;
		v6mask = 128;
	} else if (mask <= 128) {
		if (asprintf(&buf, "%s/%d", s, mask) == -1)
			err(1, "host: malloc");
		v4mask = v6mask = mask;
	} else {
		yyerror("illegal mask");
		return (NULL);
	}

	memset(&ina, 0, sizeof(struct in_addr));
	if ((bits = inet_net_pton(AF_INET, buf, &ina, sizeof(&ina))) > -1) {
		h = calloc(1, sizeof(struct node_host));
		if (h == NULL)
			err(1, "address: calloc");
		h->ifname = NULL;
		h->af = AF_INET;
		h->addr.addr_dyn = NULL;
		h->addr.addr.addr32[0] = ina.s_addr;
		set_ipmask(h, bits);
		h->next = NULL;
		h->tail = h;
		free(buf);
		return (h);
	}
	free(buf);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM; /*dummy*/
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(s, "0", &hints, &res) == 0) {
		n = calloc(1, sizeof(struct node_host));
		if (n == NULL)
			err(1, "address: calloc");
		n->ifname = NULL;
		n->af = AF_INET6;
		n->addr.addr_dyn = NULL;
		memcpy(&n->addr.addr,
		    &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr,
		    sizeof(n->addr.addr));
		n->ifindex = ((struct sockaddr_in6 *)res->ai_addr)->sin6_scope_id;
		set_ipmask(n, v6mask);
		freeaddrinfo(res);
		n->next = NULL;
		n->tail = n;
		return (n);
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM; /* DUMMY */
	error = getaddrinfo(s, NULL, &hints, &res0);
	if (error) {
		yyerror("cannot resolve %s: %s",
		    s, gai_strerror(error));
		return (NULL);
	}
	for (res = res0; res; res = res->ai_next) {
		if (res->ai_family != AF_INET &&
		    res->ai_family != AF_INET6)
			continue;
		n = calloc(1, sizeof(struct node_host));
		if (n == NULL)
			err(1, "address: calloc");
		n->ifname = NULL;
		n->af = res->ai_family;
		n->addr.addr_dyn = NULL;
		if (res->ai_family == AF_INET) {
			memcpy(&n->addr.addr,
			    &((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr,
			    sizeof(struct in_addr));
			set_ipmask(n, v4mask);
		} else {
			memcpy(&n->addr.addr,
			    &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr.s6_addr,
			    sizeof(struct in6_addr));
			n->ifindex =
			    ((struct sockaddr_in6 *)res->ai_addr)->sin6_scope_id;
			set_ipmask(n, v6mask);
		}
		n->next = NULL;
		n->tail = n;
		if (h == NULL)
			h = n;
		else {
			h->tail->next = n;
			h->tail = n;
		}
	}
	freeaddrinfo(res0);
	if (h == NULL) {
		yyerror("no IP address found for %s", s);
		return (NULL);
	}
	return (h);
}

int
atoul(char *s, u_long *ulvalp)
{
	u_long ulval;
	char *ep;

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
getservice(char *n)
{
	struct servent *s;
	u_long ulval;

	if (atoul(n, &ulval) == 0) {
		if (ulval > 65535) {
			yyerror("illegal port value %d", ulval);
			return (-1);
		}
		return (htons(ulval));
	} else {
		s = getservbyname(n, "tcp");
		if (s == NULL)
			s = getservbyname(n, "udp");
		if (s == NULL) {
			yyerror("unknown port %s", n);
			return (-1);
		}
		return (s->s_port);
	}
}

u_int16_t
parseicmpspec(char *w, sa_family_t af)
{
	const struct icmpcodeent *p;
	u_long ulval;
	u_int8_t icmptype;

	if (af == AF_INET)
		icmptype = returnicmpdefault >> 8;
	else
		icmptype = returnicmp6default >> 8;

	if (atoul(w, &ulval) == -1) {
		if ((p = geticmpcodebyname(icmptype, w, af)) == NULL) {
			yyerror("unknown icmp code %s", w);
			return (0);
		}
		ulval = p->code;
	}
	return (icmptype << 8 | ulval);
}

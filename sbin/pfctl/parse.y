/*	$OpenBSD: parse.y,v 1.122 2002/07/17 08:32:20 henning Exp $	*/

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

#include "pfctl_parser.h"

static struct pfctl *pf = NULL;
static FILE *fin = NULL;
static int debug = 0;
static int lineno = 1;
static int errors = 0;
static int rulestate = 0;

enum {PFCTL_STATE_NONE=0, PFCTL_STATE_OPTION=1,
      PFCTL_STATE_SCRUB=2, PFCTL_STATE_NAT=3,
      PFCTL_STATE_FILTER=4};

struct node_if {
	char			 ifname[IFNAMSIZ];
	u_int8_t		 not;
	struct node_if		*next;
};

struct node_proto {
	u_int8_t		 proto;
	struct node_proto	*next;
};

struct node_host {
	struct pf_addr_wrap	 addr;
	struct pf_addr		 mask;
	u_int8_t		 af;
	u_int8_t		 not;
	u_int8_t		 noroute;
	struct node_host	*next;
	u_int32_t		 ifindex;	/* link-local IPv6 addrs */
};

struct node_port {
	u_int16_t		 port[2];
	u_int8_t		 op;
	struct node_port	*next;
};

struct node_uid {
	uid_t			 uid[2];
	u_int8_t		 op;
	struct node_uid		*next;
};

struct node_gid {
	gid_t			 gid[2];
	u_int8_t		 op;
	struct node_gid		*next;
};

struct node_icmp {
	u_int8_t		 code;
	u_int8_t		 type;
	u_int8_t		 proto;
	struct node_icmp	*next;
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
};

struct peer {
	struct node_host	*host;
	struct node_port	*port;
};

int	rule_consistent(struct pf_rule *);
int	nat_consistent(struct pf_nat *);
int	rdr_consistent(struct pf_rdr *);
int	yyparse(void);
void	ipmask(struct pf_addr *, u_int8_t);
void	expand_rdr(struct pf_rdr *, struct node_if *, struct node_proto *,
    struct node_host *, struct node_host *);
void	expand_nat(struct pf_nat *, struct node_if *, struct node_proto *, 
    struct node_host *, struct node_port *,
    struct node_host *, struct node_port *);
void	expand_label_addr(const char *, char *, u_int8_t, struct node_host *);
void	expand_label_port(const char *, char *, struct node_port *);
void	expand_label_proto(const char *, char *, u_int8_t);
void	expand_label_nr(const char *, char *);
void	expand_label(char *, u_int8_t, struct node_host *, struct node_port *,
    struct node_host *, struct node_port *, u_int8_t);
void	expand_rule(struct pf_rule *, struct node_if *, struct node_proto *,
    struct node_host *, struct node_port *, struct node_host *,
    struct node_port *, struct node_uid *, struct node_gid *,
    struct node_icmp *);
int	check_rulestate(int);

struct sym {
	struct sym *next;
	char *nam;
	char *val;
};
struct sym *symhead = NULL;

int	symset(const char *, const char *);
char *	symget(const char *);

void	ifa_load();
int	ifa_exists(char *);
struct node_host    *ifa_lookup(char *);
struct node_host    *ifa_pick_ip(struct node_host *, u_int8_t);

typedef struct {
	union {
		u_int32_t		number;
		int			i;
		char			*string;
		struct {
			u_int8_t	b1;
			u_int8_t	b2;
			u_int16_t	w;
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
			char		*string;
			struct pf_addr	*addr;
			u_int8_t	rt;
			u_int8_t	af;
		}			route;
		struct redirection {
			struct node_host	*address;
			struct range		 rport;
		}			*redirection;
		struct {
			int			 action;
			struct node_state_opt	*options;
		}			keep_state;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	PASS BLOCK SCRUB RETURN IN OUT LOG LOGALL QUICK ON FROM TO FLAGS
%token	RETURNRST RETURNICMP RETURNICMP6 PROTO INET INET6 ALL ANY ICMPTYPE
%token	ICMP6TYPE CODE KEEP MODULATE STATE PORT RDR NAT BINAT ARROW NODF
%token	MINTTL IPV6ADDR ERROR ALLOWOPTS FASTROUTE ROUTETO DUPTO NO LABEL
%token	NOROUTE FRAGMENT USER GROUP MAXMSS MAXIMUM TTL SELF
%token	FRAGNORM FRAGDROP FRAGCROP
%token	SET OPTIMIZATION TIMEOUT LIMIT LOGINTERFACE
%token	<v.string> STRING
%token	<v.number> NUMBER
%token	<v.i>	PORTUNARY PORTBINARY
%type	<v.interface>	interface if_list if_item_not if_item
%type	<v.number>	port icmptype icmp6type minttl uid gid maxmss
%type	<v.i>	no dir log quick af nodf allowopts fragment fragcache
%type	<v.b>	action flag flags blockspec
%type	<v.range>	dport rport
%type	<v.proto>	proto proto_list proto_item
%type	<v.icmp>	icmpspec icmp_list icmp6_list icmp_item icmp6_item
%type	<v.fromto>	fromto
%type	<v.peer>	ipportspec
%type	<v.host>	ipspec xhost host address host_list IPV6ADDR
%type	<v.port>	portspec port_list port_item
%type	<v.uid>		uids uid_list uid_item
%type	<v.gid>		gids gid_list gid_item
%type	<v.route>	route
%type	<v.redirection>	redirection
%type	<v.string>	label
%type	<v.keep_state>	keep
%type	<v.state_opt>	state_opt_spec state_opt_list state_opt_item
%type	<v.timeout_spec>	timeout_spec timeout_list
%type	<v.limit_spec>	limit_spec limit_list
%%

ruleset		: /* empty */
		| ruleset '\n'
		| ruleset option '\n'
		| ruleset scrubrule '\n'
		| ruleset natrule '\n'
		| ruleset binatrule '\n'
		| ruleset rdrrule '\n'
		| ruleset pfrule '\n'
		| ruleset varset '\n'
		| ruleset error '\n'		{ errors++; }
		;

option		: SET OPTIMIZATION STRING
		{
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
		| SET LOGINTERFACE STRING
		{
			if (pf->opts & PF_OPT_VERBOSE)
				printf("set loginterface %s\n", $3);
			if (check_rulestate(PFCTL_STATE_OPTION))
				YYERROR;
			if (pfctl_set_logif(pf, $3) != 0) {
				yyerror("error setting loginterface %s", $3);
				YYERROR;
			}
		}
		;

varset		: STRING PORTUNARY STRING
		{
			if (pf->opts & PF_OPT_VERBOSE)
				printf("%s = %s\n", $1, $3);
			if (symset($1, $3) == -1) {
				yyerror("cannot store variable %s", $1);
				YYERROR;
			}
		}
		;

scrubrule	: SCRUB dir interface fromto nodf minttl maxmss fragcache
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
				} else if ($3->next) {
					yyerror("scrub rules don't support "
					    "{} expansion");
					YYERROR;
				}
				memcpy(r.ifname, $3->ifname,
				    sizeof(r.ifname));
			}
			if ($5)
				r.rule_flag |= PFRULE_NODF;
			if ($6)
				r.min_ttl = $6;
			if ($7)
				r.max_mss = $7;
			if ($8)
				r.rule_flag |= $8;

			r.nr = pf->rule_nr++;
			if (rule_consistent(&r) < 0)
				yyerror("skipping scrub rule due to errors");
			else
				pfctl_add_rule(pf, &r);

		}
		;

pfrule		: action dir log quick interface route af proto fromto
		  uids gids flags icmpspec keep fragment allowopts label
		{
			struct pf_rule r;
			struct node_state_opt *o;

			if (check_rulestate(PFCTL_STATE_FILTER))
				YYERROR;

			memset(&r, 0, sizeof(r));

			r.action = $1.b1;
			if ($1.b2) {
				r.rule_flag |= PFRULE_RETURNRST;
				r.return_ttl = $1.w;
			} else
				r.return_icmp = $1.w;
			r.direction = $2;
			r.log = $3;
			r.quick = $4;

			r.af = $7;
			r.flags = $12.b1;
			r.flagset = $12.b2;

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

			if ($6.rt) {
				r.rt = $6.rt;
				if ($6.string) {
					memcpy(r.rt_ifname, $6.string,
					    sizeof(r.rt_ifname));
					free($6.string);
				}
				if ($6.addr) {
					if (!r.af)
						r.af = $6.af;
					else if (r.af != $6.af) {
						yyerror("address family"
						    " mismatch");
						YYERROR;
					}
					memcpy(&r.rt_addr, $6.addr,
					    sizeof(r.rt_addr));
					free($6.addr);
				}
			}

			if ($17) {
				if (strlen($17) >= PF_RULE_LABEL_SIZE) {
					yyerror("rule label too long (max "
					    "%d chars)", PF_RULE_LABEL_SIZE-1);
					YYERROR;
				}
				strlcpy(r.label, $17, sizeof(r.label));
				free($17);
			}

			expand_rule(&r, $5, $8, $9.src.host, $9.src.port,
			    $9.dst.host, $9.dst.port, $10, $11, $13);
		}
		;

action		: PASS			{ $$.b1 = PF_PASS; $$.b2 = $$.w = 0; }
		| BLOCK blockspec	{ $$ = $2; $$.b1 = PF_DROP; }
		;

blockspec	: /* empty */		{ $$.b2 = 0; $$.w = 0; }
		| RETURNRST		{ $$.b2 = 1; $$.w = 0; }
		| RETURNRST '(' TTL NUMBER ')'	{
			$$.w = $4;
			$$.b2 = 1;
		}
		| RETURNICMP		{
			$$.b2 = 0;
			$$.w = (ICMP_UNREACH << 8) | ICMP_UNREACH_PORT;
		}
		| RETURNICMP6		{
			$$.b2 = 0;
			$$.w = (ICMP6_DST_UNREACH << 8) |
			    ICMP6_DST_UNREACH_NOPORT;
		}
		| RETURNICMP '(' NUMBER ')'	{
			$$.w = (ICMP_UNREACH << 8) | $3;
			$$.b2 = 0;
		}
		| RETURNICMP '(' STRING ')'	{
			const struct icmpcodeent *p;

			if ((p = geticmpcodebyname(ICMP_UNREACH, $3,
			    AF_INET)) == NULL) {
				yyerror("unknown icmp code %s", $3);
				YYERROR;
			}
			$$.w = (p->type << 8) | p->code;
			$$.b2 = 0;
		}
		| RETURNICMP6 '(' NUMBER ')'	{
			$$.w = (ICMP6_DST_UNREACH << 8) | $3;
			$$.b2 = 0;
		}
		| RETURNICMP6 '(' STRING ')'	{
			const struct icmpcodeent *p;

			if ((p = geticmpcodebyname(ICMP6_DST_UNREACH, $3,
			    AF_INET6)) == NULL) {
				yyerror("unknown icmp code %s", $3);
				YYERROR;
			}
			$$.w = (p->type << 8) | p->code;
			$$.b2 = 0;
		}
		;

fragcache	: /* empty */		{ $$ = 0; }
		| fragment FRAGNORM	{ $$ = 0; /* default */ }
		| fragment FRAGCROP	{ $$ = PFRULE_FRAGCROP; }
		| fragment FRAGDROP	{ $$ = PFRULE_FRAGDROP; }
		;


dir		: IN			{ $$ = PF_IN; }
		| OUT				{ $$ = PF_OUT; }
		;

log		: /* empty */			{ $$ = 0; }
		| LOG				{ $$ = 1; }
		| LOGALL			{ $$ = 2; }
		;

quick		: /* empty */			{ $$ = 0; }
		| QUICK				{ $$ = 1; }
		;

interface	: /* empty */			{ $$ = NULL; }
		| ON if_item_not		{ $$ = $2; }
		| ON '{' if_list '}'		{ $$ = $3; }
		;

if_list		: if_item_not			{ $$ = $1; }
		| if_list ',' if_item_not	{ $3->next = $1; $$ = $3; }
		;

if_item_not	: '!' if_item			{ $$ = $2; $$->not = 1; }
		| if_item			{ $$ = $1; }

if_item		: STRING			{
			if (!ifa_exists($1)) {
				yyerror("unknown interface %s", $1);
				YYERROR;
			}
			$$ = malloc(sizeof(struct node_if));
			if ($$ == NULL)
				err(1, "if_item: malloc");
			strlcpy($$->ifname, $1, IFNAMSIZ);
			$$->not = 0;
			$$->next = NULL;
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
		| proto_list ',' proto_item	{ $3->next = $1; $$ = $3; }
		;

proto_item	: NUMBER			{
			struct protoent *p;

			if ((p = getprotobynumber($1)) == NULL) {
				yyerror("unknown protocol %d", $1);
				YYERROR;
			}
			$$ = malloc(sizeof(struct node_proto));
			if ($$ == NULL)
				err(1, "proto_item: malloc");
			$$->proto = p->p_proto;
			$$->next = NULL;
		}
		| STRING			{
			struct protoent *p;

			if ((p = getprotobyname($1)) == NULL) {
				yyerror("unknown protocol %s", $1);
				YYERROR;
			}
			$$ = malloc(sizeof(struct node_proto));
			if ($$ == NULL)
				err(1, "proto_item: malloc");
			$$->proto = p->p_proto;
			$$->next = NULL;
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
		| host_list ',' xhost		{
			/* both $1 and $3 may be lists, so join them */
			$$ = $3;
			while ($3->next)
				$3 = $3->next;
			$3->next = $1;
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
		}
		;

host		: address			{
			struct node_host *n;
			for (n = $1; n; n = n->next)
				if (n->af == AF_INET)
					ipmask(&n->mask, 32);
				else
					ipmask(&n->mask, 128);
			$$ = $1;
		}
		| address '/' NUMBER		{
			struct node_host *n;
			for (n = $1; n; n = n->next) {
				if ($1->af == AF_INET) {
					if ($3 < 0 || $3 > 32) {
						yyerror(
						    "illegal netmask value /%d",
						    $3);
						YYERROR;
					}
				} else {
					if ($3 < 0 || $3 > 128) {
						yyerror(
						    "illegal netmask value /%d",
						    $3);
						YYERROR;
					}
				}
				ipmask(&n->mask, $3);
			}
			$$ = $1;
		}
		;

address		: '(' STRING ')'		{
			$$ = calloc(1, sizeof(struct node_host));
			if ($$ == NULL)
				err(1, "address: calloc");
			$$->af = 0;
			$$->addr.addr_dyn = (struct pf_addr_dyn *)1;
			strncpy($$->addr.addr.pfa.ifname, $2,
			    sizeof($$->addr.addr.pfa.ifname));
		}
		| SELF				{
			struct node_host *h = NULL;
				if ((h = ifa_lookup("all")) == NULL)
					YYERROR;
				else
					$$ = h;
		}
		| STRING			{
			if (ifa_exists($1)) {
				struct node_host *h = NULL;

				/* interface with this name exists */
				if ((h = ifa_lookup($1)) == NULL)
					YYERROR;
				else
					$$ = h;
			} else {
				struct node_host *h = NULL, *n;
				struct addrinfo hints, *res0, *res;
				int error;

				memset(&hints, 0, sizeof(hints));
				hints.ai_family = PF_UNSPEC;
				hints.ai_socktype = SOCK_STREAM; /* DUMMY */
				error = getaddrinfo($1, NULL, &hints, &res0);
				if (error) {
					yyerror("cannot resolve %s: %s",
					    $1, gai_strerror(error));
					YYERROR;
				}
				for (res = res0; res; res = res->ai_next) {
					if (res->ai_family != AF_INET &&
					    res->ai_family != AF_INET6)
						continue;
					n = calloc(1, sizeof(struct node_host));
					if (n == NULL)
						err(1, "address: calloc");
					n->af = res->ai_family;
					n->addr.addr_dyn = NULL;
					if (res->ai_family == AF_INET)
						memcpy(&n->addr.addr,
						&((struct sockaddr_in *)
						    res->ai_addr)
						    ->sin_addr.s_addr,
						sizeof(struct in_addr));
					else {
						memcpy(&n->addr.addr,
						&((struct sockaddr_in6 *)
						    res->ai_addr)
						    ->sin6_addr.s6_addr,
						sizeof(struct in6_addr));
						n->ifindex =
						    ((struct sockaddr_in6 *)
						    res->ai_addr)
						    ->sin6_scope_id;
					}
					n->next = h;
					h = n;
				}
				freeaddrinfo(res0);
				if (h == NULL) {
					yyerror("no IP address found for %s", $1);
					YYERROR;
				}
				$$ = h;
			}
		}
		| NUMBER '.' NUMBER '.' NUMBER '.' NUMBER {
			if ($1 < 0 || $3 < 0 || $5 < 0 || $7 < 0 ||
			    $1 > 255 || $3 > 255 || $5 > 255 || $7 > 255) {
				yyerror("illegal ip address %d.%d.%d.%d",
				    $1, $3, $5, $7);
				YYERROR;
			}
			$$ = calloc(1, sizeof(struct node_host));
			if ($$ == NULL)
				err(1, "address: calloc");
			$$->af = AF_INET;
			$$->addr.addr_dyn = NULL;
			$$->addr.addr.addr32[0] = htonl(($1 << 24) |
			    ($3 << 16) | ($5 << 8) | $7);
		}
		| IPV6ADDR			{ $$ = $1; }
		;

portspec	: port_item			{ $$ = $1; }
		| '{' port_list '}'		{ $$ = $2; }
		;

port_list	: port_item			{ $$ = $1; }
		| port_list ',' port_item	{ $3->next = $1; $$ = $3; }
		;

port_item	: port				{
			$$ = malloc(sizeof(struct node_port));
			if ($$ == NULL)
				err(1, "port_item: malloc");
			$$->port[0] = $1;
			$$->port[1] = $1;
			$$->op = PF_OP_EQ;
			$$->next = NULL;
		}
		| PORTUNARY port		{
			$$ = malloc(sizeof(struct node_port));
			if ($$ == NULL)
				err(1, "port_item: malloc");
			$$->port[0] = $2;
			$$->port[1] = $2;
			$$->op = $1;
			$$->next = NULL;
		}
		| port PORTBINARY port		{
			$$ = malloc(sizeof(struct node_port));
			if ($$ == NULL)
				err(1, "port_item: malloc");
			$$->port[0] = $1;
			$$->port[1] = $3;
			$$->op = $2;
			$$->next = NULL;
		}
		;

port		: NUMBER			{
			if ($1 < 0 || $1 > 65535) {
				yyerror("illegal port value %d", $1);
				YYERROR;
			}
			$$ = htons($1);
		}
		| STRING			{
			struct servent *s = NULL;

			s = getservbyname($1, "tcp");
			if (s == NULL)
				s = getservbyname($1, "udp");
			if (s == NULL) {
				yyerror("unknown protocol %s", $1);
				YYERROR;
			}
			$$ = s->s_port;
		}
		;

uids		: /* empty */			{ $$ = NULL; }
		| USER uid_item			{ $$ = $2; }
		| USER '{' uid_list '}'		{ $$ = $3; }
		;

uid_list	: uid_item			{ $$ = $1; }
		| uid_list ',' uid_item		{ $3->next = $1; $$ = $3; }
		;

uid_item	: uid				{
			$$ = malloc(sizeof(struct node_uid));
			if ($$ == NULL)
				err(1, "uid_item: malloc");
			$$->uid[0] = $1;
			$$->uid[1] = $1;
			$$->op = PF_OP_EQ;
			$$->next = NULL;
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
		}
		;

uid		: NUMBER			{
			if ($1 < 0 || $1 >= UID_MAX) {
				yyerror("illegal uid value %u", $1);
				YYERROR;
			}
			$$ = $1;
		}
		| STRING			{
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
		}
		;

gids		: /* empty */			{ $$ = NULL; }
		| GROUP gid_item		{ $$ = $2; }
		| GROUP '{' gid_list '}'	{ $$ = $3; }
		;

gid_list	: gid_item			{ $$ = $1; }
		| gid_list ',' gid_item		{ $3->next = $1; $$ = $3; }
		;

gid_item	: gid				{
			$$ = malloc(sizeof(struct node_gid));
			if ($$ == NULL)
				err(1, "gid_item: malloc");
			$$->gid[0] = $1;
			$$->gid[1] = $1;
			$$->op = PF_OP_EQ;
			$$->next = NULL;
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
		}
		;

gid		: NUMBER			{
			if ($1 < 0 || $1 >= GID_MAX) {
				yyerror("illegal gid value %u", $1);
				YYERROR;
			}
			$$ = $1;
		}
		| STRING			{
			if (!strcmp($1, "unknown"))
				$$ = GID_MAX;
			else {
				struct passwd *pw;

				if ((pw = getpwnam($1)) == NULL) {
					yyerror("unknown group %s", $1);
					YYERROR;
				}
				$$ = pw->pw_uid;
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
		| FLAGS flag			{ $$.b1 = $2.b1; $$.b2 = PF_TH_ALL; }
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
		| icmp_list ',' icmp_item	{ $3->next = $1; $$ = $3; }
		;

icmp6_list	: icmp6_item			{ $$ = $1; }
		| icmp6_list ',' icmp6_item	{ $3->next = $1; $$ = $3; }
		;

icmp_item	: icmptype		{
			$$ = malloc(sizeof(struct node_icmp));
			if ($$ == NULL)
				err(1, "icmp_item: malloc");
			$$->type = $1;
			$$->code = 0;
			$$->proto = IPPROTO_ICMP;
			$$->next = NULL;
		}
		| icmptype CODE NUMBER	{
			$$ = malloc(sizeof(struct node_icmp));
			if ($$ == NULL)
				err(1, "icmp_item: malloc");
			if ($3 < 0 || $3 > 255) {
				yyerror("illegal icmp-code %d", $3);
				YYERROR;
			}
			$$->type = $1;
			$$->code = $3 + 1;
			$$->proto = IPPROTO_ICMP;
			$$->next = NULL;
		}
		| icmptype CODE STRING	{
			const struct icmpcodeent *p;

			$$ = malloc(sizeof(struct node_icmp));
			if ($$ == NULL)
				err(1, "icmp_item: malloc");
			$$->type = $1;
			if ((p = geticmpcodebyname($1, $3,
			    AF_INET)) == NULL) {
				yyerror("unknown icmp-code %s", $3);
				YYERROR;
			}
			$$->code = p->code + 1;
			$$->proto = IPPROTO_ICMP;
			$$->next = NULL;
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
		}
		| icmp6type CODE NUMBER	{
			$$ = malloc(sizeof(struct node_icmp));
			if ($$ == NULL)
				err(1, "icmp_item: malloc");
			if ($3 < 0 || $3 > 255) {
				yyerror("illegal icmp6-code %d", $3);
				YYERROR;
			}
			$$->type = $1;
			$$->code = $3 + 1;
			$$->proto = IPPROTO_ICMPV6;
			$$->next = NULL;
		}
		| icmp6type CODE STRING	{
			const struct icmpcodeent *p;

			$$ = malloc(sizeof(struct node_icmp));
			if ($$ == NULL)
				err(1, "icmp_item: malloc");
			$$->type = $1;
			if ((p = geticmpcodebyname($1, $3,
			    AF_INET6)) == NULL) {
				yyerror("unknown icmp6-code %s", $3);
				YYERROR;
			}
			$$->code = p->code + 1;
			$$->proto = IPPROTO_ICMPV6;
			$$->next = NULL;
		}
		;

icmptype	: STRING			{
			const struct icmptypeent *p;

			if ((p = geticmptypebyname($1, AF_INET)) == NULL) {
				yyerror("unknown icmp-type %s", $1);
				YYERROR;
			}
			$$ = p->type + 1;
		}
		| NUMBER			{
			if ($1 < 0 || $1 > 255) {
				yyerror("illegal icmp-type %d", $1);
				YYERROR;
			}
			$$ = $1 + 1;
		}
		;

icmp6type	: STRING			{
			const struct icmptypeent *p;

			if ((p = geticmptypebyname($1, AF_INET6)) == NULL) {
				yyerror("unknown ipv6-icmp-type %s", $1);
				YYERROR;
			}
			$$ = p->type + 1;
		}
		| NUMBER			{
			if ($1 < 0 || $1 > 255) {
				yyerror("illegal icmp6-type %d", $1);
				YYERROR;
			}
			$$ = $1 + 1;
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
		| state_opt_list ',' state_opt_item {
			$$ = $1;
			while ($1->next)
				$1 = $1->next;
			$1->next = $3;
		}
		;

state_opt_item	: MAXIMUM NUMBER		{
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
		}
		| STRING NUMBER			{
			int i;

			for (i = 0; pf_timeouts[i].name &&
			    strcmp(pf_timeouts[i].name, $1); ++i);
			if (!pf_timeouts[i].name) {
				yyerror("illegal timeout name %s", $1);
				YYERROR;
			}
			if (strchr(pf_timeouts[i].name, '.') == NULL) {
				yyerror("illegal state timeout %s", $1);
				YYERROR;
			}
			if ($2 < 0) {
				yyerror("illegal timeout value %d", $2);
				YYERROR;
			}
			$$ = calloc(1, sizeof(struct node_state_opt));
			if ($$ == NULL)
				err(1, "state_opt_item: calloc");
			$$->type = PF_STATE_OPT_TIMEOUT;
			$$->data.timeout.number = pf_timeouts[i].timeout;
			$$->data.timeout.seconds = $2;
			$$->next = NULL;
		}
		;

fragment	: /* empty */			{ $$ = 0; }
		| FRAGMENT			{ $$ = 1; }

minttl		: /* empty */			{ $$ = 0; }
		| MINTTL NUMBER			{
			if ($2 < 0 || $2 > 255) {
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
		| MAXMSS NUMBER			{
			if ($2 < 0) {
				yyerror("illegal max-mss value %d", $2);
				YYERROR;
			}
			$$ = $2;
		}
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

no		: /* empty */			{ $$ = 0; }
		| NO				{ $$ = 1; }
		;

rport		: port				{
			$$.a = $1;
			$$.b = $$.t = 0;
		}
		| port ':' port			{
			$$.a = $1;
			$$.b = $3;
			$$.t = PF_RPORT_RANGE;
		}
		| port ':' '*'			{
			$$.a = $1;
			$$.b = 0;
			$$.t = PF_RPORT_RANGE;
		}
		;

redirection	: /* empty */			{ $$ = NULL; }
		| ARROW address			{
			$$ = malloc(sizeof(struct redirection));
			if ($$ == NULL)
				err(1, "redirection: malloc");
			$$->address = $2;
			$$->rport.a = $$->rport.b = $$->rport.t = 0;
		}
		| ARROW address PORT rport	{
			$$ = malloc(sizeof(struct redirection));
			if ($$ == NULL)
				err(1, "redirection: malloc");
			$$->address = $2;
			$$->rport = $4;
		}
		;

natrule		: no NAT interface af proto fromto redirection
		{
			struct pf_nat nat;

			if (check_rulestate(PFCTL_STATE_NAT))
				YYERROR;

			memset(&nat, 0, sizeof(nat));

			nat.no = $1;
			nat.af = $4;
			if (nat.no) {
				if ($7 != NULL) {
					yyerror("'no nat' rule does not need "
					    "'->'");
					YYERROR;
				}
			} else {
				struct node_host *n;

				if ($7 == NULL || $7->address == NULL) {
					yyerror("'nat' rule requires '-> "
					    "address'");
					YYERROR;
				}
				n = ifa_pick_ip($7->address, nat.af);
				if (n == NULL)
					YYERROR;
				if (!nat.af)
					nat.af = n->af;
				memcpy(&nat.raddr, &n->addr,
				    sizeof(nat.raddr));
				nat.proxy_port[0] = ntohs($7->rport.a);
				nat.proxy_port[1] = ntohs($7->rport.b);
				if (!nat.proxy_port[0] && !nat.proxy_port[1]) {
					nat.proxy_port[0] =
					    PF_NAT_PROXY_PORT_LOW;
					nat.proxy_port[1] =
					    PF_NAT_PROXY_PORT_HIGH;
				} else if (!nat.proxy_port[1])
					nat.proxy_port[1] = nat.proxy_port[0];
				free($7->address);
				free($7);
			}

			expand_nat(&nat, $3, $5, $6.src.host, $6.src.port,
			    $6.dst.host, $6.dst.port);
		}
		;

binatrule	: no BINAT interface af proto FROM address TO ipspec redirection
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
				memcpy(&binat.saddr, &$7->addr,
				    sizeof(binat.saddr));
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
				memcpy(&binat.daddr, &$9->addr,
				    sizeof(binat.daddr));
				memcpy(&binat.dmask, &$9->mask,
				    sizeof(binat.dmask));
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

				if ($10 == NULL || $10->address == NULL) {
					yyerror("'binat' rule requires"
					    " '-> address'");
					YYERROR;
				}
				n = ifa_pick_ip($10->address, binat.af);
				if (n == NULL)
					YYERROR;
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
				memcpy(&binat.raddr, &n->addr,
				    sizeof(binat.raddr));
				free($10->address);
				free($10);
			}

			pfctl_add_binat(pf, &binat);
		}

rdrrule		: no RDR interface af proto FROM ipspec TO ipspec dport redirection
		{
			struct pf_rdr rdr;

			if (check_rulestate(PFCTL_STATE_NAT))
				YYERROR;

			memset(&rdr, 0, sizeof(rdr));

			rdr.no = $1;
			rdr.af = $4;
			if ($7 != NULL) {
				memcpy(&rdr.saddr, &$7->addr,
				    sizeof(rdr.saddr));
				memcpy(&rdr.smask, &$7->mask,
				    sizeof(rdr.smask));
				rdr.snot  = $7->not;
			}
			if ($9 != NULL) {
				memcpy(&rdr.daddr, &$9->addr,
				    sizeof(rdr.daddr));
				memcpy(&rdr.dmask, &$9->mask,
				    sizeof(rdr.dmask));
				rdr.dnot  = $9->not;
			}

			rdr.dport  = $10.a;
			rdr.dport2 = $10.b;
			rdr.opts  |= $10.t;

			if (rdr.no) {
				if ($11 != NULL) {
					yyerror("'no rdr' rule does not need '->'");
					YYERROR;
				}
			} else {
				struct node_host *n;

				if ($11 == NULL || $11->address == NULL) {
					yyerror("'rdr' rule requires '-> "
					    "address'");
					YYERROR;
				}
				n = ifa_pick_ip($11->address, rdr.af);
				if (n == NULL)
					YYERROR;
				if (!rdr.af)
					rdr.af = n->af;
				memcpy(&rdr.raddr, &n->addr,
				    sizeof(rdr.raddr));
				free($11->address);
				rdr.rport  = $11->rport.a;
				rdr.opts  |= $11->rport.t;
				free($11);
			}

			expand_rdr(&rdr, $3, $5, $7, $9);
		}
		;

dport		: /* empty */			{
			$$.a = $$.b = $$.t = 0;
		}
		| PORT port			{
			$$.a = $2;
			$$.b = $$.t = 0;
		}
		| PORT port ':' port		{
			$$.a = $2;
			$$.b = $4;
			$$.t = PF_DPORT_RANGE;
		}
		;

route		: /* empty */			{
			$$.string = NULL;
			$$.rt = 0;
			$$.addr = NULL;
			$$.af = 0;
		}
		| FASTROUTE {
			$$.string = NULL;
			$$.rt = PF_FASTROUTE;
			$$.addr = NULL;
		}
		| ROUTETO STRING ':' address {
			$$.string = strdup($2);
			$$.rt = PF_ROUTETO;
			if ($4->addr.addr_dyn != NULL) {
				yyerror("route-to does not support"
				    " dynamic addresses");
				YYERROR;
			}
			if ($4->next) {
				yyerror("multiple route-to ip addresses");
				YYERROR;
			}
			$$.addr = &$4->addr.addr;
			$$.af = $4->af;
		}
		| ROUTETO STRING {
			$$.string = strdup($2);
			$$.rt = PF_ROUTETO;
			$$.addr = NULL;
		}
		| DUPTO STRING ':' address {
			$$.string = strdup($2);
			$$.rt = PF_DUPTO;
			if ($4->addr.addr_dyn != NULL) {
				yyerror("dup-to does not support"
				    " dynamic addresses");
				YYERROR;
			}
			if ($4->next) {
				yyerror("multiple dup-to ip addresses");
				YYERROR;
			}
			$$.addr = &$4->addr.addr;
			$$.af = $4->af;
		}
		| DUPTO STRING {
			$$.string = strdup($2);
			$$.rt = PF_DUPTO;
			$$.addr = NULL;
		}
		;

timeout_spec	: STRING NUMBER
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

timeout_list	: timeout_list ',' timeout_spec
		| timeout_spec
		;

limit_spec	: STRING NUMBER
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

limit_list	: limit_list ',' limit_spec
		| limit_spec
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
	if (r->proto != IPPROTO_TCP && (r->flags || r->flagset)) {
		yyerror("flags only applies to tcp");
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
	if (!(r->rule_flag & PFRULE_RETURNRST) && r->return_icmp &&
	    ((r->af != AF_INET6  &&  (r->return_icmp>>8) != ICMP_UNREACH) ||
	    (r->af == AF_INET6 && (r->return_icmp>>8) != ICMP6_DST_UNREACH))) {
		yyerror("return-icmp version does not match address family");
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

	if (!r->af && (r->raddr.addr_dyn != NULL)) {
		yyerror("dynamic addresses require address family (inet/inet6)");
		problems++;
	}
	return (-problems);
}

int
rdr_consistent(struct pf_rdr *r)
{
	int problems = 0;

	if (r->proto != IPPROTO_TCP && r->proto != IPPROTO_UDP &&
	    (r->dport || r->dport2 || r->rport)) {
		yyerror("port only applies to tcp/udp");
		problems++;
	}
	if (!r->af && (r->saddr.addr_dyn != NULL ||
	    r->daddr.addr_dyn != NULL || r->raddr.addr_dyn != NULL)) {
		yyerror("dynamic addresses require address family (inet/inet6)");
		problems++;
	}
	return (-problems);
}

struct keywords {
	const char	*k_name;
	int	 k_val;
};

/* macro gore, but you should've seen the prior indentation nightmare... */

#define CHECK_ROOT(T,r) \
	do { \
		if (r == NULL) { \
			r = malloc(sizeof(T)); \
			if (r == NULL) \
				err(1, "malloc"); \
			memset(r, 0, sizeof(T)); \
		} \
	} while (0)

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
		T *n = r; \
		while (n != NULL) { \
			do { \
				C; \
			} while (0); \
			n = n->next; \
		} \
	} while (0)

void
expand_label_addr(const char *name, char *label, u_int8_t af,
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
		    PF_AZERO(&host->mask, af)))
			strlcat(tmp, "any", PF_RULE_LABEL_SIZE);
		else {
			char a[48];
			int bits;

			if (inet_ntop(af, &host->addr.addr, a,
			    sizeof(a)) == NULL)
				strlcat(a, "?", sizeof(a));
			strlcat(tmp, a, PF_RULE_LABEL_SIZE);
			bits = unmask(&host->mask, af);
			a[0] = 0;
			if ((af == AF_INET && bits < 32) ||
			    (af == AF_INET6 && bits < 128))
				snprintf(a, sizeof(a), "/%u", bits);
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
expand_label(char *label, u_int8_t af,
    struct node_host *src_host, struct node_port *src_port,
    struct node_host *dst_host, struct node_port *dst_port,
    u_int8_t proto)
{
	expand_label_addr("$srcaddr", label, af, src_host);
	expand_label_addr("$dstaddr", label, af, dst_host);
	expand_label_port("$srcport", label, src_port);
	expand_label_port("$dstport", label, dst_port);
	expand_label_proto("$proto", label, proto);
	expand_label_nr("$nr", label);
}

void
expand_rule(struct pf_rule *r,
    struct node_if *interfaces, struct node_proto *protos,
    struct node_host *src_hosts, struct node_port *src_ports,
    struct node_host *dst_hosts, struct node_port *dst_ports,
    struct node_uid *uids, struct node_gid *gids,
    struct node_icmp *icmp_types)
{
	int af = r->af, nomatch = 0, added = 0;
	char ifname[IF_NAMESIZE];
	char label[PF_RULE_LABEL_SIZE];

	strlcpy(label, r->label, sizeof(label));

	CHECK_ROOT(struct node_if, interfaces);
	CHECK_ROOT(struct node_proto, protos);
	CHECK_ROOT(struct node_host, src_hosts);
	CHECK_ROOT(struct node_port, src_ports);
	CHECK_ROOT(struct node_host, dst_hosts);
	CHECK_ROOT(struct node_port, dst_ports);
	CHECK_ROOT(struct node_uid, uids);
	CHECK_ROOT(struct node_gid, gids);
	CHECK_ROOT(struct node_icmp, icmp_types);

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
		expand_label(r->label, r->af, src_host, src_port,
		    dst_host, dst_port, proto->proto);
		r->ifnot = interface->not;
		r->proto = proto->proto;
		r->src.addr = src_host->addr;
		r->src.mask = src_host->mask;
		r->src.noroute = src_host->noroute;
		r->src.not = src_host->not;
		r->src.port[0] = src_port->port[0];
		r->src.port[1] = src_port->port[1];
		r->src.port_op = src_port->op;
		r->dst.addr = dst_host->addr;
		r->dst.mask = dst_host->mask;
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

		if (icmp_type->proto && r->proto != icmp_type->proto) {
			yyerror("icmp-type mismatch");
			nomatch++;
		}

		if (rule_consistent(r) < 0 || nomatch)
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

	if (!added)
		yyerror("rule expands to no valid combination");
}

void
expand_nat(struct pf_nat *n,
    struct node_if *interfaces, struct node_proto *protos,
    struct node_host *src_hosts, struct node_port *src_ports,
    struct node_host *dst_hosts, struct node_port *dst_ports)
{
	char ifname[IF_NAMESIZE];
	int af = n->af, added = 0;

	CHECK_ROOT(struct node_if, interfaces);
	CHECK_ROOT(struct node_proto, protos);
	CHECK_ROOT(struct node_host, src_hosts);
	CHECK_ROOT(struct node_port, src_ports);
	CHECK_ROOT(struct node_host, dst_hosts);
	CHECK_ROOT(struct node_port, dst_ports);

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

		if (!n->af && n->raddr.addr_dyn != NULL) {
			yyerror("address family (inet/inet6) undefined");
			continue;
		}

		n->ifnot = interface->not;
		n->proto = proto->proto;
		n->src.addr = src_host->addr;
		n->src.mask = src_host->mask;
		n->src.noroute = src_host->noroute;
		n->src.not = src_host->not;
		n->src.port[0] = src_port->port[0];
		n->src.port[1] = src_port->port[1];
		n->src.port_op = src_port->op;
		n->dst.addr = dst_host->addr;
		n->dst.mask = dst_host->mask;
		n->dst.noroute = dst_host->noroute;
		n->dst.not = dst_host->not;
		n->dst.port[0] = dst_port->port[0];
		n->dst.port[1] = dst_port->port[1];
		n->dst.port_op = dst_port->op;

		if (nat_consistent(n) < 0)
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

	if (!added)
		yyerror("nat rule expands to no valid combinations");
}

void
expand_rdr(struct pf_rdr *r, struct node_if *interfaces,
    struct node_proto *protos, struct node_host *src_hosts,
    struct node_host *dst_hosts)
{
	int af = r->af, added = 0;
	char ifname[IF_NAMESIZE];

	CHECK_ROOT(struct node_if, interfaces);
	CHECK_ROOT(struct node_proto, protos);
	CHECK_ROOT(struct node_host, src_hosts);
	CHECK_ROOT(struct node_host, dst_hosts);

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
		if (!r->af && (r->saddr.addr_dyn != NULL ||
		    r->daddr.addr_dyn != NULL || r->raddr.addr_dyn)) {
			yyerror("address family (inet/inet6) undefined");
			continue;
		}

		if (if_indextoname(src_host->ifindex, ifname))
			memcpy(r->ifname, ifname, sizeof(r->ifname));
		else if (if_indextoname(dst_host->ifindex, ifname))
			memcpy(r->ifname, ifname, sizeof(r->ifname));
		else
			memcpy(r->ifname, interface->ifname, sizeof(r->ifname));

		r->proto = proto->proto;
		r->ifnot = interface->not;
		r->saddr = src_host->addr;
		r->smask = src_host->mask;
		r->daddr = dst_host->addr;
		r->dmask = dst_host->mask;

		if (rdr_consistent(r) < 0)
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

	if (!added)
		yyerror("rdr rule expands to no valid combination");
}

#undef FREE_LIST
#undef CHECK_ROOT
#undef LOOP_THROUGH

int
check_rulestate(int desired_state)
{
	if (rulestate > desired_state) {
		yyerror("Rules must be in order: options, normalization, "
		    "translation, filter");
		return (1);
	}
	rulestate = desired_state;
	return (0);
}

int
kw_cmp(k, e)
	const void *k, *e;
{
	return (strcmp(k, ((struct keywords *)e)->k_name));
}

int
lookup(char *s)
{
	/* this has to be sorted always */
	static const struct keywords keywords[] = {
		{ "all",	ALL},
		{ "allow-opts",	ALLOWOPTS},
		{ "any",	ANY},
		{ "binat",	BINAT},
		{ "block",	BLOCK},
		{ "code",	CODE},
		{ "crop",	FRAGCROP},
		{ "drop-ovl",	FRAGDROP},
		{ "dup-to",	DUPTO},
		{ "fastroute",	FASTROUTE},
		{ "flags",	FLAGS},
		{ "fragment",	FRAGMENT},
		{ "from",	FROM},
		{ "group",	GROUP},
		{ "icmp-type",	ICMPTYPE},
		{ "in",		IN},
		{ "inet",	INET},
		{ "inet6",	INET6},
		{ "ipv6-icmp-type", ICMP6TYPE},
		{ "keep",	KEEP},
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
		{ "proto",	PROTO},
		{ "quick",	QUICK},
		{ "rdr",	RDR},
		{ "reassemble",	FRAGNORM},
		{ "return",	RETURN},
		{ "return-icmp",RETURNICMP},
		{ "return-icmp6",RETURNICMP6},
		{ "return-rst",	RETURNRST},
		{ "route-to",	ROUTETO},
		{ "scrub",	SCRUB},
		{ "self",	SELF},
		{ "set",	SET},
		{ "state",	STATE},
		{ "timeout",	TIMEOUT},
		{ "to",		TO},
		{ "ttl",	TTL},
		{ "user",	USER},
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
		/* Reading characters from the parse buffer, instead of input */
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
			ungetc(next, fin);
			break;
		}
		yylval.lineno = lineno;
		lineno++;
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
findeol()
{
	int c;

	parsebuf = NULL;
	pushback_index = 0;

	/* skip to either EOF or the first real EOL */
	while (1) {
		c = lgetc(fin);
		if (c == '\\') {
			c = lgetc(fin);
			if (c == '\n')
				continue;
		}
		if (c == EOF || c == '\n')
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
	while ((c = lgetc(fin)) == ' ' || c == '\t')
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
		if (val == NULL)
			return (ERROR);
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
			if (c == '\n')
				continue;
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

	/* Need to parse v6 addresses before tokenizing numbers. ick */
	if (isxdigit(c) || c == ':') {
		struct node_host *node = NULL;
		u_int32_t addr[4];
		char lookahead[46];
		int i = 0;
		struct addrinfo hints, *res;

		lookahead[i] = c;

		while (i < sizeof(lookahead) &&
		    (isalnum(c) || c == ':' || c == '.' || c == '%')) {
			lookahead[++i] = c = lgetc(fin);
		}

		/* quick check avoids calling inet_pton too often */
		lungetc(lookahead[i], fin);
		lookahead[i] = '\0';

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET6;
		hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
		hints.ai_flags = AI_NUMERICHOST;
		if (getaddrinfo(lookahead, "0", &hints, &res) == 0) {
			node = calloc(1, sizeof(struct node_host));
			if (node == NULL)
				err(1, "yylex: calloc");
			node->af = AF_INET6;
			node->addr.addr_dyn = NULL;
			memcpy(&node->addr.addr,
			    &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr,
			    sizeof(addr));
			node->ifindex = ((struct sockaddr_in6 *)res->ai_addr)
			    ->sin6_scope_id;
			yylval.v.host = node;
			return IPV6ADDR;
			freeaddrinfo(res);
		} else {
			free(node);
			while (i > 1)
				lungetc(lookahead[--i], fin);
			c = lookahead[--i];
		}
	}

	if (isdigit(c)) {
		int index = 0, base = 10;
		u_int64_t n = 0;

		yylval.v.number = 0;
		while (1) {
			if (base == 10) {
				if (!isdigit(c))
					break;
				c -= '0';
			} else if (base == 16) {
				if (isdigit(c))
					c -= '0';
				else if (c >= 'a' && c <= 'f')
					c -= 'a' - 10;
				else if (c >= 'A' && c <= 'F')
					c -= 'A' - 10;
				else
					break;
			}
			n = n * base + c;

			if (n > UINT_MAX) {
				yyerror("number is too large");
				return (ERROR);
			}
			c = lgetc(fin);
			if (c == EOF)
				break;
			if (index++ == 0 && n == 0 && c == 'x') {
				base = 16;
				c = lgetc(fin);
				if (c == EOF)
					break;
			}
		}
		yylval.v.number = (u_int32_t)n;

		if (c != EOF)
			lungetc(c, fin);
		if (debug > 1)
			fprintf(stderr, "number: %d\n", yylval.v.number);
		return (NUMBER);
	}

#define allowed_in_string(x) \
	(isalnum(x) || (ispunct(x) && x != '(' && x != ')' && \
	x != '{' && x != '}' && x != '<' && x != '>' && \
	x != '!' && x != '=' && x != '/' && x != '#' && \
	x != ',' && x != ':' && x != '(' && x != ')'))

	if (isalnum(c)) {
		do {
			*p++ = c;
			if (p-buf >= sizeof buf) {
				yyerror("string too long");
				return (ERROR);
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
	fin = input;
	pf = xpf;
	lineno = 1;
	errors = 0;
	rulestate = PFCTL_STATE_NONE;
	yyparse();
	return (errors ? -1 : 0);
}

void
ipmask(struct pf_addr *m, u_int8_t b)
{
	int i, j = 0;

	while (b >= 32) {
		m->addr32[j++] = 0xffffffff;
		b -= 32;
	}
	for (i = 31; i > 31-b; --i)
		m->addr32[j] |= (1 << i);
	if (b)
		m->addr32[j] = htonl(m->addr32[j]);
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
	symhead = sym;
	return (0);
}

char *
symget(const char *nam)
{
	struct sym *sym;

	for (sym = symhead; sym; sym = sym->next)
		if (strcmp(nam, sym->nam) == 0)
			return (sym->val);
	return (NULL);
}

struct ifaddrs **ifatab, **ifalist;
int ifatablen, ifalistlen;
int
ifa_comp(const void *p1, const void *p2)
{
	struct ifaddrs *ifa1 = *(struct ifaddrs **)p1;
	struct ifaddrs *ifa2 = *(struct ifaddrs **)p2;

	return strcmp(ifa1->ifa_name, ifa2->ifa_name);
}

void
ifa_load(void)
{
	struct ifaddrs *ifap, *ifa;
	void *p;
	int load_ifalen = 0;

	if (getifaddrs(&ifap) < 0)
		err(1, "getifaddrs");
	for (ifa = ifap; ifa; ifa = ifa->ifa_next)
		load_ifalen++;
	/* (over-)allocate tables */
	ifatab = malloc(load_ifalen * sizeof(void *));
	ifalist = malloc(load_ifalen * sizeof(void *));
	if (!ifatab || !ifalist)
		err(1, "malloc");
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family == AF_LINK) {
			if (bsearch(&ifa, ifalist, ifalistlen, sizeof(void *),
			    ifa_comp))
				continue; /* take only the first LINK address */
			ifalist[ifalistlen++] = ifa;
			qsort(ifalist, ifalistlen, sizeof(void *), ifa_comp);
		}
#ifdef __KAME__
		if (ifa->ifa_addr->sa_family == AF_INET6 &&
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
		if (ifa->ifa_addr->sa_family == AF_INET ||
		    ifa->ifa_addr->sa_family == AF_INET6) {
			ifatab[ifatablen++] = ifa;
		}
	}
	/* shrink tables */
	if ((p = realloc(ifatab, ifatablen * sizeof(void *))) == NULL) {
		free(ifatab);
		ifatab = NULL;
	} else
		ifatab = p;
	if ((p = realloc(ifalist, ifalistlen * sizeof(void *))) == NULL) {
		free(ifalist);
		ifalist = NULL;
	} else
		ifalist = p;
	if (!ifatab || !ifalist)
		err(1, "realloc");

}

int
ifa_exists(char *ifa_name)
{
	struct ifaddrs ifa, *ifp = &ifa, **ifpp;

	if (!ifalist)
		ifa_load();
	ifa.ifa_name = ifa_name;
	ifpp = bsearch(&ifp, ifalist, ifalistlen, sizeof(void *), ifa_comp);
	if (ifpp == NULL)
		return(0);
	else
		return(1);
}

struct node_host *
ifa_lookup(char *ifa_name)
{
	struct node_host *h = NULL, *n = NULL;
	struct ifaddrs *ifa;
	int return_all = 0;

	if (strncmp(ifa_name, "all", IFNAMSIZ) == 0)
		return_all = 1;

	if (!ifatab)
		ifa_load();
	for (ifa = *ifatab; ifa; ifa = ifa->ifa_next) {
		if (strncmp(ifa->ifa_name, ifa_name, IFNAMSIZ) == 0 || 
		    return_all) {
			if (!(ifa->ifa_addr->sa_family == AF_INET ||
			    ifa->ifa_addr->sa_family == AF_INET6))
				continue;
			n = calloc(1, sizeof(struct node_host));
			if (n == NULL)
				err(1, "address: calloc");
			n->af = ifa->ifa_addr->sa_family;
			n->addr.addr_dyn = NULL;
			if (ifa->ifa_addr->sa_family == AF_INET)
				memcpy(&n->addr.addr, &((struct sockaddr_in *)
				    ifa->ifa_addr)->sin_addr.s_addr,
				    sizeof(struct in_addr));
			else {
				memcpy(&n->addr.addr, &((struct sockaddr_in6 *)
				    ifa->ifa_addr)->sin6_addr.s6_addr,
				    sizeof(struct in6_addr));
				n->ifindex = ((struct sockaddr_in6 *)
				    ifa->ifa_addr)->sin6_scope_id;
			}
			n->next = h;
			h = n;
		}
	}
	if (h == NULL) {
		yyerror("no IP address found for %s", ifa_name);
	}
	return (h);
}

struct node_host *
ifa_pick_ip(struct node_host *nh, u_int8_t af)
{
	struct node_host *h, *n = NULL;

	if (af == 0 && nh->next) {
		yyerror("address family not given and interface has multiple "
		    "IPs");
		return(NULL);
	}
	for(h = nh; h; h = h->next) {
		if (h->af == af || h->af == 0 || af == 0) {
			if (n != NULL) {
				yyerror("interface has multiple IPs of "
				    "this address family");
				return(NULL);
			}
			n = h;
		}
	}
	return n;
}


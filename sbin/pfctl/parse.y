/*	$OpenBSD: parse.y,v 1.20 2001/08/19 16:16:41 dhartmei Exp $	*/

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
#include <net/pfvar.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <netdb.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <err.h>

#include "pfctl_parser.h"

static struct pfctl *pf = NULL;
static FILE *fin = NULL;
static int debug = 0;
static int lineno = 1;
static int errors = 0;
static int natmode = 0;

struct node_proto {
	u_int8_t		 proto;
	struct node_proto	*next;
};

struct node_host {
	u_int32_t		 addr;
	u_int32_t		 mask;
	u_int8_t		 not;
	struct node_host	*next;
};

struct node_port {
	u_int16_t		 port[2];
	u_int8_t		 op;
	struct node_port	*next;
};

struct peer {
	struct node_host	*host;
	struct node_port	*port;
};

int			 rule_consistent(struct pf_rule *);
int			 yyparse(void);
struct pf_rule_addr	*new_addr(void);
u_int32_t		 ipmask(u_int8_t);
void			 expand_rule(struct pf_rule *, struct node_proto *,
			    struct node_host *, struct node_port *,
			    struct node_host *, struct node_port *);

typedef struct {
	union {
		u_int32_t		number;
		int			i;
		char			*string;
		struct {
			char		*string;
			int		not;
		}			iface;
		struct {
			u_int8_t	b1;
			u_int8_t	b2;
			u_int16_t	w;
		}			b;
		struct {
			int		a;
			int		b;
			int		t;
		}			range;
		struct node_proto	*proto;
		struct node_host	*host;
		struct node_port	*port;
		struct peer		peer;
		struct {
			struct peer	src, dst;
		}			fromto;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	PASS BLOCK SCRUB RETURN IN OUT LOG LOGALL QUICK ON FROM TO FLAGS
%token	RETURNRST RETURNICMP PROTO ALL ANY ICMPTYPE CODE KEEP STATE PORT
%token	RDR NAT ARROW NODF MINTTL ERROR
%token	<v.string> STRING
%token	<v.number> NUMBER
%token	<v.i>	PORTUNARY PORTBINARY
%type	<v.iface> iface natiface
%type	<v.number> port icmptype minttl
%type	<v.i>	direction log quick keep nodf
%type	<v.b>	action icmpspec flag flags blockspec
%type	<v.range>	dport rport
%type	<v.proto>	proto proto_list proto_item
%type	<v.fromto>	fromto
%type	<v.peer>	ipportspec
%type	<v.host>	ipspec host address host_list
%type	<v.port>	portspec port_list port_item
%%

ruleset:	/* empty */
		| ruleset '\n'
		| ruleset pfrule '\n'
		| ruleset natrule '\n'
		| ruleset rdrrule '\n'
		| ruleset error '\n'		{ errors++; }
		;

pfrule:		action direction log quick iface proto fromto flags icmpspec keep nodf minttl
		{
			struct pf_rule r;

			if (natmode) {
				yyerror("filter rule not permitted in nat mode");
				YYERROR;
			}
			memset(&r, 0, sizeof(r));

			r.action = $1.b1;
			if ($1.b2)
				r.rule_flag |= PFRULE_RETURNRST;
			else
				r.return_icmp = $1.w;
			r.direction = $2;
			r.log = $3;
			r.quick = $4;
			if ($5.string)
				memcpy(r.ifname, $5.string, sizeof(r.ifname));

			r.flags = $8.b1;
			r.flagset = $8.b2;
			r.type = $9.b1;
			r.code = $9.b2;
			r.keep_state = $10;

			if ($11)
				r.rule_flag |= PFRULE_NODF;
			if ($12)
				r.min_ttl = $12;

			expand_rule(&r, $6, $7.src.host, $7.src.port,
			    $7.dst.host, $7.dst.port);
		}
		;

action:		PASS			{ $$.b1 = PF_PASS; }
		| BLOCK blockspec	{ $$ = $2; $$.b1 = PF_DROP; }
		| SCRUB			{ $$.b1 = PF_SCRUB; }
		;

blockspec:				{ $$.b2 = 0; $$.w = 0; }
		| RETURNRST		{ $$.b2 = 1; $$.w = 0;}
		| RETURNICMP		{
			$$.b2 = 0;
			$$.w = (ICMP_UNREACH << 8) | ICMP_UNREACH_PORT;
		}
		| RETURNICMP '(' STRING ')'	{
			struct icmpcodeent *p;

			if ((p = geticmpcodebyname(ICMP_UNREACH, $3)) == NULL) {
				yyerror("unknown icmp code %s", $3);
				YYERROR;
			}
			$$.w = (p->type << 8) | p->code;
			$$.b2 = 0;
		}
		;

direction:	IN			{ $$ = PF_IN; }
		| OUT			{ $$ = PF_OUT; }
		;

log:					{ $$ = 0; }
		| LOG			{ $$ = 1; }
		| LOGALL		{ $$ = 2; }
		;

quick:					{ $$ = 0; }
		| QUICK			{ $$ = 1; }
		;

natiface:	iface
		| ON '!' STRING		{
			$$.string = strdup($3); $$.not = 1;
		}
		;
iface:					{ $$.string = NULL; }
		| ON STRING		{ $$.string = strdup($2); }
		;

proto:						{ $$ = NULL; }
		| PROTO proto_item		{ $$ = $2; }
		| PROTO '{' proto_list '}'	{ $$ = $3; }
		;

proto_list:	proto_item			{ $$ = $1; }
		| proto_list ',' proto_item	{ $3->next = $1; $$ = $3; }
		;

proto_item:	NUMBER			{
			struct protoent *p;

			if ((p = getprotobynumber($1)) == NULL) {
				yyerror("unknown protocol %d", $1);
				YYERROR;
			}
			$$ = malloc(sizeof(struct node_proto));
			$$->proto = p->p_proto;
			$$->next = NULL;
		}
		| STRING		{
			struct protoent *p;

			if ((p = getprotobyname($1)) == NULL) {
				yyerror("unknown protocol %s", $1);
				YYERROR;
			}
			$$ = malloc(sizeof(struct node_proto));
			$$->proto = p->p_proto;
			$$->next = NULL;
		}
		;

fromto:		ALL			{
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

ipportspec:	ipspec			{ $$.host = $1; $$.port = NULL; }
		| ipspec PORT portspec	{
			$$.host = $1;
			$$.port = $3;
		}
		;

ipspec:		ANY			{ $$ = NULL; }
		| '!' host		{ $$ = $2; $$->not = 1; }
		| host			{ $$ = $1; $$->not = 0; }
		| '{' host_list '}'	{ $$ = $2; }
		;

host_list:	host			{ $$ = $1; }
		| host_list ',' host	{ $3->next = $1; $$ = $3; }

host:		address			{
			$$ = $1;
			$$->mask = 0xffffffff;
		}
		| address '/' NUMBER	{
			if ($3 < 0 || $3 > 32) {
				yyerror("illegal netmask value %d", $3);
				YYERROR;
			}
			$$ = $1;
			$$->mask = ipmask($3);
		}
		;

address:	STRING {
			struct hostent *hp;

			if ((hp = gethostbyname($1)) == NULL) {
				yyerror("cannot resolve %s", $1);
				YYERROR;
			}
			$$ = malloc(sizeof(struct node_host));
			memcpy(&$$->addr, hp->h_addr, sizeof(u_int32_t));
		}
		| NUMBER '.' NUMBER '.' NUMBER '.' NUMBER {
			if ($1 < 0 || $3 < 0 || $5 < 0 || $7 < 0 ||
			    $1 > 255 || $3 > 255 || $5 > 255 || $7 > 255) {
				yyerror("illegal ip address %d.%d.%d.%d",
				    $1, $3, $5, $7);
				YYERROR;
			}
			$$ = malloc(sizeof(struct node_host));
			$$->addr = htonl(($1 << 24) | ($3 << 16) | ($5 << 8) | $7);
		}
		;

portspec:	port_item			{ $$ = $1; }
		| '{' port_list '}'		{ $$ = $2; }
		;

port_list:	port_item			{ $$ = $1; }
		| port_list ',' port_item	{ $3->next = $1; $$ = $3; }
		;

port_item:	port				{
			$$ = malloc(sizeof(struct node_port));
			$$->port[0] = $1;
			$$->port[1] = $1;
			$$->op = PF_OP_EQ;
		}
		| PORTUNARY port		{
			$$ = malloc(sizeof(struct node_port));
			$$->port[0] = $2;
			$$->port[1] = $2;
			$$->op = $1;
		}
		| port PORTBINARY port		{
			$$ = malloc(sizeof(struct node_port));
			$$->port[0] = $1;
			$$->port[1] = $3;
			$$->op = $2;
		}
		;

port:		NUMBER				{
			if (0 > $1 || $1 > 65535) {
				yyerror("illegal port value %d", $1);
				YYERROR;
			}
			$$ = htons($1);
		}
		| STRING		{
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

flag:		STRING			{
			int f;

			if ((f = parse_flags($1)) < 0) {
				yyerror("bad flags %s", $1);
				YYERROR;
			}
			$$.b1 = f;
		}
		;

flags:					{ $$.b1 = 0; $$.b2 = 0; }
		| FLAGS flag		{ $$.b1 = $2.b1; $$.b2 = 63; }
		| FLAGS flag "/" flag	{ $$.b1 = $2.b1; $$.b2 = $4.b1; }
		| FLAGS "/" flag	{ $$.b1 = 0; $$.b2 = $3.b1; }
		;

icmpspec:				{ $$.b1 = 0; $$.b2 = 0; }
		| ICMPTYPE icmptype	{ $$.b1 = $2; $$.b2 = 0; }
		| ICMPTYPE icmptype CODE NUMBER	{
			if ($4 < 0 || $4 > 255) {
				yyerror("illegal icmp code %d", $4);
				YYERROR;
			}
			$$.b1 = $2;
			$$.b2 = $4 + 1;
		}
		| ICMPTYPE icmptype CODE STRING	{
			struct icmpcodeent *p;

			$$.b1 = $2;
			if ((p = geticmpcodebyname($2, $4)) == NULL) {
				yyerror("unknown icmp-code %s", $4);
				YYERROR;
			}
			$$.b2 = p->code + 1;
		}
		;

icmptype:	STRING			{
			struct icmptypeent *p;

			if ((p = geticmptypebyname($1)) == NULL) {
				yyerror("unknown icmp-type %s", $1);
				YYERROR;
			}
			$$ = p->type + 1;
		}
		| NUMBER		{
			if ($1 < 0 || $1 > 255) {
				yyerror("illegal icmp type %d", $1);
				YYERROR;
			}
			$$ = $1 + 1;
		}
		;


keep:					{ $$ = 0; }
		| KEEP STATE		{ $$ = 1; }
		;

minttl:					{ $$ = 0; }
		| MINTTL NUMBER		{
			if ($2 < 0 || $2 > 255) {
				yyerror("illegal min-ttl value %d", $2);
				YYERROR;
			}
			$$ = $2;
		}
		;

nodf:					{ $$ = 0; }
		| NODF			{ $$ = 1; }
		;

natrule:	NAT natiface proto FROM ipspec TO ipspec ARROW address
		{
			struct pf_nat nat;

			if (!natmode) {
				yyerror("nat rule not permitted in filter mode");
				YYERROR;
			}

			memset(&nat, 0, sizeof(nat));

			if ($2.string) {
				memcpy(nat.ifname, $2.string,
				    sizeof(nat.ifname));
				nat.ifnot = $2.not;
			}
			if ($3 != NULL) {
				nat.proto = $3->proto;
				free($3);
			}
			if ($5 != NULL) {
				nat.saddr = $5->addr;
				nat.smask = $5->mask;
				nat.snot  = $5->not;
				free($5);
			}
			if ($7 != NULL) {
				nat.daddr = $7->addr;
				nat.dmask = $7->mask;
				nat.dnot  = $7->not;
				free($7);
			}

			if ($9 == NULL) {
				yyerror("nat rule requires redirection address");
				YYERROR;
			}
			nat.raddr = $9->addr;
			free($9);

			pfctl_add_nat(pf, &nat);
		}
		;

rdrrule:	RDR natiface proto FROM ipspec TO ipspec dport ARROW address rport
		{
			struct pf_rdr rdr;

			if (!natmode) {
				yyerror("rdr rule not permitted in filter mode");
				YYERROR;
			}

			memset(&rdr, 0, sizeof(rdr));

			if ($2.string) {
				memcpy(rdr.ifname, $2.string,
				    sizeof(rdr.ifname));
				rdr.ifnot = $2.not;
			}
			if ($3 != NULL) {
				rdr.proto = $3->proto;
				free($3);
			}
			if ($5 != NULL) {
				rdr.saddr = $5->addr;
				rdr.smask = $5->mask;
				rdr.snot  = $5->not;
				free($5);
			}
			if ($7 != NULL) {
				rdr.daddr = $7->addr;
				rdr.dmask = $7->mask;
				rdr.dnot  = $7->not;
				free($7);
			}

			rdr.dport  = $8.a;
			rdr.dport2 = $8.b;
			rdr.opts  |= $8.t;

			if ($10 == NULL) {
				yyerror("rdr rule requires redirection address");
				YYERROR;
			}
			rdr.raddr = $10->addr;
			free($10);

			rdr.rport  = $11.a;
			rdr.opts  |= $11.t;

			pfctl_add_rdr(pf, &rdr);
		}
		;

dport:		PORT port			{
			$$.a = $2;
			$$.b = $$.t = 0;
		}
		| PORT port ':' port		{
			$$.a = $2;
			$$.b = $4;
			$$.t = PF_DPORT_RANGE;
		}
		;

rport:		PORT port			{
			$$.a = $2;
			$$.b = $$.t = 0;
		}
		| PORT port ':' '*'		{
			$$.a = $2;
			$$.b = 0;
			$$.t = PF_RPORT_RANGE;
		}
		;

%%

int
yyerror(char *fmt, ...)
{
	va_list ap;
	extern char *infile;
	errors = 1;

	va_start(ap, fmt);
	fprintf(stderr, "%s:%d: ", infile, yyval.lineno);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	return (0);
}

int
rule_consistent(struct pf_rule *r)
{
	int problems = 0;

	if (r->action == PF_SCRUB) {
		if (r->quick) {
			yyerror("quick does not apply to scrub");
			problems++;
		}
		if (r->keep_state) {
			yyerror("keep state does not apply to scrub");
			problems++;
		}
		if (r->src.port_op) {
			yyerror("src port does not apply to scrub");
			problems++;
		}
		if (r->dst.port_op) {
			yyerror("dst port does not apply to scrub");
			problems++;
		}
		if (r->type || r->code) {
			yyerror("icmp-type/code does not apply to scrub");
			problems++;
		}
	} else {
		if (r->rule_flag & PFRULE_NODF) {
			yyerror("nodf only applies to scrub");
			problems++;
		}
		if (r->min_ttl) {
			yyerror("min-ttl only applies to scrub");
			problems++;
		}
	}
	if (r->proto != IPPROTO_TCP && r->proto != IPPROTO_UDP &&
	    (r->src.port_op || r->dst.port_op)) {
		yyerror("port only applies to tcp/udp");
		problems++;
	}
	if (r->proto != IPPROTO_ICMP && (r->type || r->code)) {
		yyerror("icmp-type/code only applies to icmp");
		problems++;
	}
	return (-problems);
}

#define CHECK_ROOT(T,r) \
	do { \
		if (r == NULL) { \
			r = malloc(sizeof(T)); \
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

void
expand_rule(struct pf_rule *r, struct node_proto *protos,
    struct node_host *src_hosts, struct node_port *src_ports,
    struct node_host *dst_hosts, struct node_port *dst_ports)
{
	struct node_proto *proto;
	struct node_host *src_host, *dst_host;
	struct node_port *src_port, *dst_port;

	CHECK_ROOT(struct node_proto, protos);
	CHECK_ROOT(struct node_host, src_hosts);
	CHECK_ROOT(struct node_port, src_ports);
	CHECK_ROOT(struct node_host, dst_hosts);
	CHECK_ROOT(struct node_port, dst_ports);

	proto = protos;
	while (proto != NULL) {
		src_host = src_hosts;
		while (src_host != NULL) {
			src_port = src_ports;
			while (src_port != NULL) {
				dst_host = dst_hosts;
				while (dst_host != NULL) {
					dst_port = dst_ports;
					while (dst_port != NULL) {
						r->proto = proto->proto;
						r->src.addr = src_host->addr;
						r->src.mask = src_host->mask;
						r->src.not = src_host->not;
						r->src.port[0] = src_port->port[0];
						r->src.port[1] = src_port->port[1];
						r->src.port_op = src_port->op;
						r->dst.addr = dst_host->addr;
						r->dst.mask = dst_host->mask;
						r->dst.not = dst_host->not;
						r->dst.port[0] = dst_port->port[0];
						r->dst.port[1] = dst_port->port[1];
						r->dst.port_op = dst_port->op;
						if (rule_consistent(r) < 0)
							yyerror("skipping rule "
							    "due to errors");
						else
							pfctl_add_rule(pf, r);
						dst_port = dst_port->next;
					}
					dst_host = dst_host->next;
				}
				src_port = src_port->next;
			}
			src_host = src_host->next;
		}
		proto = proto->next;
	}

	FREE_LIST(struct node_proto, protos);
	FREE_LIST(struct node_host, src_hosts);
	FREE_LIST(struct node_port, src_ports);
	FREE_LIST(struct node_host, dst_hosts);
	FREE_LIST(struct node_port, dst_ports);
}

#undef FREE_LIST
#undef CHECK_ROOT

int
lookup(char *s)
{
	int i;
	struct keywords {
		char	*k_name;
		int	 k_val;
	} keywords[] = {
		{ "all",	ALL},
		{ "any",	ANY},
		{ "block",	BLOCK},
		{ "code",	CODE},
		{ "flags",	FLAGS},
		{ "from",	FROM},
		{ "icmp-type",	ICMPTYPE},
		{ "in",		IN},
		{ "keep",	KEEP},
		{ "log",	LOG},
		{ "log-all",	LOGALL},
		{ "min-ttl",	MINTTL},
		{ "nat",	NAT},
		{ "no-df",	NODF},
		{ "on",		ON},
		{ "out",	OUT},
		{ "pass",	PASS},
		{ "port",	PORT},
		{ "proto",	PROTO},
		{ "quick",	QUICK},
		{ "rdr",	RDR},
		{ "return",	RETURN},
		{ "return-icmp",RETURNICMP},
		{ "return-rst",	RETURNRST},
		{ "scrub",	SCRUB},
		{ "state",	STATE},
		{ "to",		TO},
		{ NULL,		0 },
	};

	for (i = 0; keywords[i].k_name != NULL; i++) {
		if (strcmp(s, keywords[i].k_name) == 0) {
			if (debug > 1)
				fprintf(stderr, "%s: %d\n", s,
				    keywords[i].k_val);
			return (keywords[i].k_val);
		}
	}
	if (debug > 1)
		fprintf(stderr, "string: %s\n", s);
	return (STRING);
}

int
lgetc(FILE *fin)
{
	int c, next;

restart:
	c = getc(fin);
	if (c == '\\') {
		next = getc(fin);
		if (next != '\n') {
			ungetc(next, fin);
			return (c);
		}
		yylval.lineno = lineno;
		lineno++;
		goto restart;
	}
	return (c);
}

int
yylex(void)
{
	char *p, buf[8096];
	int c, next;
	int token;

	while ((c = lgetc(fin)) == ' ' || c == '\t')
		;
	
	yylval.lineno = lineno;
	if (c == '#')
		while ((c = lgetc(fin)) != '\n' && c != EOF)
			;
	if (c == '-') {
		next = lgetc(fin);
		if (next == '>')
			return (ARROW);
		ungetc(next, fin);
	}
	switch (c) {
	case '=':
		yylval.v.i = PF_OP_EQ;
		return (PORTUNARY);
	case '!':
		next = lgetc(fin);
		if (next == '=') {
			yylval.v.i = PF_OP_NE;
			return (PORTUNARY);
		}
		ungetc(next, fin);
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
			ungetc(next, fin);
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
			ungetc(next, fin);
		}
		return (PORTUNARY);
		break;
	}
	if (isdigit(c)) {
		yylval.v.number = 0;
		do {
			u_int64_t n = (u_int64_t)yylval.v.number * 10 + c - '0';
			if (n > 0xffffffff) {
				yyerror("number is too large");
				return (ERROR);
			}
			yylval.v.number = (u_int32_t)n;
		} while ((c = lgetc(fin)) != EOF && isdigit(c));
		ungetc(c, fin);
		if (debug > 1)
			fprintf(stderr, "number: %d\n", yylval.v.number);
		return (NUMBER);
	}

#define allowed_in_string(x) \
	isalnum(x) || (ispunct(x) && x != '(' && x != ')' && x != '<' \
	&& x != '>' && x != '!' && x != '=' && x != '/' && x != '#' && x != ',')

	if (isalnum(c)) {
		p = buf;
		do {
			*p++ = c;
			if (p-buf >= sizeof buf) {
				yyerror("string too long");
				return (ERROR);
			}
		} while ((c = lgetc(fin)) != EOF && (allowed_in_string(c)));
		ungetc(c, fin);
		*p = '\0';
		token = lookup(buf);
		yylval.v.string = strdup(buf);
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
	natmode = 0;
	fin = input;
	pf = xpf;
	errors = 0;
	yyparse();
	return (errors ? -1 : 0);
}

int
parse_nat(FILE *input, struct pfctl *xpf)
{
	natmode = 1;
	fin = input;
	pf = xpf;
	errors = 0;
	yyparse();
	return (errors ? -1 : 0);
}

u_int32_t
ipmask(u_int8_t b)
{
	u_int32_t m = 0;
	int i;

	for (i = 31; i > 31-b; --i)
		m |= (1 << i);
	return (htonl(m));
}

struct pf_rule_addr *
new_addr(void)
{
	struct pf_rule_addr *ra;

	ra = malloc(sizeof(struct pf_rule_addr));
	if (ra == NULL)
		err(1, "new_addr: malloc failed");
	memset(ra, 0, sizeof(*ra));
	return (ra);
}

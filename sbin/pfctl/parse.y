/*	$OpenBSD: parse.y,v 1.11 2001/07/18 00:41:48 mickey Exp $	*/

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

static int proto = 0;	/* this is a synthesysed attribute */

int			 rule_consistent(struct pf_rule *);
int			 yyparse(void);
struct pf_rule_addr	*new_addr(void);
u_int32_t		 ipmask(u_int8_t);

%}
%union {
	u_int32_t		number;
	int			i;
	char			*string;
	struct pf_rule_addr	*addr;
	struct {
		struct pf_rule_addr	*src, *dst;
	}			addr2;
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
}
%token	PASS BLOCK SCRUB RETURN IN OUT LOG LOGALL QUICK ON FROM TO FLAGS
%token	RETURNRST RETURNICMP PROTO ALL ANY ICMPTYPE CODE KEEP STATE PORT
%token	RDR NAT ARROW NODF MINTTL
%token	<string> STRING
%token	<number> NUMBER
%token	<i>	PORTUNARY PORTBINARY
%type	<addr>	ipportspec ipspec host portspec
%type	<addr2>	fromto
%type	<iface> iface
%type	<number> address port icmptype minttl
%type	<i>	direction log quick keep proto nodf
%type	<b>	action icmpspec flags blockspec
%type	<range>	dport rport
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

			if (natmode)
				errx(1, "line %d: filter rule in nat mode",
				    lineno);

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
			r.proto = $6;
			proto = 0;	/* reset syntesysed attribute */

			memcpy(&r.src, $7.src, sizeof(r.src));
			free($7.src);
			memcpy(&r.dst, $7.dst, sizeof(r.dst));
			free($7.dst);

			r.flags = $8.b1;
			r.flagset = $8.b2;
			r.type = $9.b1;
			r.code = $9.b2;
			r.keep_state = $10;

			if ($11)
				r.rule_flag |= PFRULE_NODF;
			if ($12)
				r.min_ttl = $12;

			if (rule_consistent(&r) < 0)
				warnx("skipping rule due to errors");
			else
				pfctl_add_rule(pf, &r);
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
				warnx("line %d: unknown icmp code %s",
				    lineno, $3);
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

iface:					{ $$.string = NULL; }
		| ON STRING		{ $$.string = strdup($2); }
		| ON '!' STRING		{
			if (! natmode) {
				warnx("can't '!' interface in pf rule");
				YYERROR;
			}
			$$.string = strdup($3); $$.not = 1;
		}
		;

proto:					{ $$ = proto; }
		| PROTO NUMBER		{
			struct protoent *p;

			if ((p = getprotobynumber($2)) == NULL) {
				warnx("line %d: unknown protocol %d", lineno,
				    $2);
				YYERROR;
			}
			proto = $$ = p->p_proto;
		}
		| PROTO STRING		{
			struct protoent *p;

			if ((p = getprotobyname($2)) == NULL) {
				warnx("line %d: unknown protocol %s", lineno,
				    $2);
				YYERROR;
			}
			proto = $$ = p->p_proto;
		}
		;

fromto:		ALL			{
			$$.src = new_addr();
			$$.dst = new_addr();
		}
		| FROM ipportspec TO ipportspec {
			$$.src = $2;
			$$.dst = $4;
		}
		;

ipportspec:	ipspec			{ $$ = $1; }
		| ipspec portspec		{
			$$ = $1;
			if ($2) {
				$$->port[0] = $2->port[0];
				$$->port[1] = $2->port[1];
				$$->port_op = $2->port_op;
				free($2);
			}
		}
		;

ipspec:		ANY			{ $$ = new_addr(); }
		| '!' host		{ $$ = $2; $$->not = 1; }
		| host			{ $$ = $1; }
		;

host:		address		{
			$$ = new_addr();
			$$->addr = $1;
			$$->mask = 0xffffffff;
		}
		|
		address '/' NUMBER	{
			if ($3 < 0 || $3 > 32) {
				warnx("illegal netmask value %d", $3);
				YYERROR;
			}
			$$ = new_addr();
			$$->addr = $1;
			$$->mask = ipmask($3);
		}
		;

address:	STRING {
			struct hostent *hp;

			if (inet_pton(AF_INET, $1, &$$) != 1) {
				if ((hp = gethostbyname($1)) == NULL) {
					warnx("line %d: cannot resolve %s",
					    lineno, $1);
					YYERROR;
				}
				memcpy(&$$, hp->h_addr, sizeof(u_int32_t));
			}
		}
		| NUMBER '.' NUMBER '.' NUMBER '.' NUMBER {
			if ($1 < 0 || $3 < 0 || $5 < 0 || $7 < 0 ||
			    $1 > 255 || $3 > 255 || $5 > 255 || $7 > 255) {
				warnx("illegal ip address %d.%d.%d.%d",
				    $1, $3, $5, $7);
				YYERROR;
			}
			$$ = (htonl(($1 << 24) | ($3 << 16) | ($5 << 8) | $7));
		}
		;

portspec:	PORT PORTUNARY port	{
			$$ = new_addr();
			$$->port_op = $2;
			$$->port[0] = $3;
			$$->port[1] = $3;
		}
		| PORT port PORTBINARY port	{
			$$ = new_addr();
			$$->port[0] = $2;
			$$->port_op = $3;
			$$->port[1] = $4;
		}
		;

port:		NUMBER			{
			if (0 > $1 || $1 > 65535) {
				warnx("illegal port value %d", $1);
				YYERROR;
			}
			$$ = htons($1);
		}
		| STRING		{
			struct servent *s = NULL;

			/* use synthesysed attribute */
			if (proto) {
				s = getservbyname($1,
				    proto == IPPROTO_TCP ? "tcp" : "udp");
				if (s == NULL) {
					warnx("line %d: unknown protocol %s",
					    lineno, $1);
					YYERROR;
				}
				$$ = s->s_port;
			} else {
				$$ = 0;
			}
		}
		;

flags:					{ $$.b1 = 0; $$.b2 = 0; }
		| FLAGS STRING		{
			int f;

			if ((f = parse_flags($2)) < 0) {
				warnx("line %d: bad flags %s", lineno, $2);
				YYERROR;
			}
			$$.b1 = f;
			$$.b2 = 63;
		}
		| FLAGS STRING "/" STRING	{
			int f;

			if ((f = parse_flags($2)) < 0) {
				warnx("line %d: bad flags %s", lineno, $2);
				YYERROR;
			}
			$$.b1 = f;
			if ((f = parse_flags($4)) < 0) {
				warnx("line %d: bad flags %s", lineno, $4);
				YYERROR;
			}
			$$.b2 = f;
		}
		;

icmpspec:				{ $$.b1 = 0; $$.b2 = 0; }
		| ICMPTYPE icmptype	{ $$.b1 = $2; $$.b2 = 0; }
		| ICMPTYPE icmptype CODE NUMBER	{
			if ($4 < 0 || $4 > 255) {
				warnx("illegal icmp code %d", $4);
				YYERROR;
			}
			$$.b1 = $2;
			$$.b2 = $4 + 1;
		}
		| ICMPTYPE icmptype CODE STRING	{
			struct icmpcodeent *p;

			$$.b1 = $2;
			if ((p = geticmpcodebyname($2, $4)) == NULL) {
				warnx("line %d: unknown icmp-code %s",
				    lineno, $4);
				YYERROR;
			}
			$$.b2 = p->code + 1;
		}
		;

icmptype:	STRING			{
			struct icmptypeent *p;

			if ((p = geticmptypebyname($1)) == NULL) {
				warnx("line %d: unknown icmp-type %s",
				    lineno, $1);
				YYERROR;
			}
			$$ = p->type + 1;
		}
		| NUMBER		{
			if ($1 < 0 || $1 > 255) {
				warnx("illegal icmp type %d", $1);
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
				warnx("illegal min-ttl value %d", $2);
				YYERROR;
			}
			$$ = $2;
		}
		;

nodf:					{ $$ = 0; }
		| NODF			{ $$ = 1; }
		;

natrule:	NAT iface proto FROM ipspec TO ipspec ARROW address
		{
			struct pf_nat nat;

			if (!natmode)
				errx(1, "line %d: nat rule in filter mode",
				    lineno);

			memset(&nat, 0, sizeof(nat));

			if ($2.string) {
				memcpy(nat.ifname, $2.string,
				    sizeof(nat.ifname));
				nat.ifnot = $2.not;
			}
			nat.proto = $3;
			proto = 0;	/* reset syntesysed attribute */

			nat.saddr = $5->addr;
			nat.smask = $5->mask;
			nat.snot  = $5->not;
			free($5);

			nat.daddr = $7->addr;
			nat.dmask = $7->mask;
			nat.dnot  = $7->not;
			free($7);

			nat.raddr = $9;

			pfctl_add_nat(pf, &nat);
		}
		;

rdrrule:	RDR { proto = IPPROTO_TCP; } iface proto FROM ipspec TO ipspec dport ARROW address rport
		{
			struct pf_rdr rdr;

			if (!natmode) {
				errx(1, "line %d: nat rule in filter mode",
				    lineno);
			}

			memset(&rdr, 0, sizeof(rdr));

			if ($3.string) {
				memcpy(rdr.ifname, $3.string,
				    sizeof(rdr.ifname));
				rdr.ifnot = $3.not;
			}
			rdr.proto = $4;
			proto = 0;	/* reset syntesysed attribute */

			rdr.saddr = $6->addr;
			rdr.smask = $6->mask;
			rdr.snot  = $6->not;
			free($6);

			rdr.daddr = $8->addr;
			rdr.dmask = $8->mask;
			rdr.dnot  = $8->not;
			free($8);

			rdr.dport  = $9.a;
			rdr.dport2 = $9.b;
			rdr.opts  |= $9.t;

			rdr.raddr = $11;

			rdr.rport  = $12.a;
			rdr.opts  |= $12.t;

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
yyerror(char *s)
{
	errors = 1;
	warnx("%s near line %d", s, lineno);
	return 0;
}

int
rule_consistent(struct pf_rule *r)
{
	int problems = 0;

	if (r->action == PF_SCRUB) {
		if (r->quick) {
			warnx("quick does not apply to scrub");
			problems++;
		}
		if (r->keep_state) {
			warnx("keep state does not apply to scrub");
			problems++;
		}
		if (r->src.port_op) {
			warnx("src port does not apply to scrub");
			problems++;
		}
		if (r->dst.port_op) {
			warnx("dst port does not apply to scrub");
			problems++;
		}
		if (r->type || r->code) {
			warnx("icmp-type/code does not apply to scrub");
			problems++;
		}
	} else {
		if (r->rule_flag & PFRULE_NODF) {
			warnx("nodf applies only to scrub");
			problems++;
		}
		if (r->min_ttl) {
			warnx("min-ttl applies only to scrub");
			problems++;
		}
	}
	if (r->proto != IPPROTO_TCP && r->proto != IPPROTO_UDP &&
	    (r->src.port_op || r->dst.port_op)) {
		warnx("ports do only apply to tcp/udp");
		problems++;
	}
	if (r->proto != IPPROTO_ICMP && (r->type || r->code)) {
		warnx("icmp-type/code does only apply to icmp");
		problems++;
	}
	return -problems;
}

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
			return keywords[i].k_val;
		}
	}
	if (debug > 1)
		fprintf(stderr, "string: %s\n", s);
	return STRING;
}

int
yylex(void)
{
	char *p, buf[8096];
	int c, next;
	int token;

	while ((c = getc(fin)) == ' ' || c == '\t')
		;
	if (c == '#')
		while ((c = getc(fin)) != '\n' && c != EOF)
			;
	if (c == '-') {
		next = getc(fin);
		if (next == '>')
			return ARROW;
		ungetc(next, fin);
	}
	switch (c) {
	case '=':
		yylval.i = PF_OP_EQ;
		return PORTUNARY;
	case '!':
		next = getc(fin);
		if (next == '=') {
			yylval.i = PF_OP_NE;
			return PORTUNARY;
		}
		ungetc(next, fin);
		break;
	case '<':
		next = getc(fin);
		if (next == '>') {
			yylval.i = PF_OP_GL;
			return PORTBINARY;
		} else  if (next == '=') {
			yylval.i = PF_OP_LE;
		} else {
			yylval.i = PF_OP_LT;
			ungetc(next, fin);
		}
		return PORTUNARY;
		break;
	case '>':
		next = getc(fin);
		if (next == '<') {
			yylval.i = PF_OP_GL;
			return PORTBINARY;
		} else  if (next == '=') {
			yylval.i = PF_OP_GE;
		} else {
			yylval.i = PF_OP_GT;
			ungetc(next, fin);
		}
		return PORTUNARY;
		break;
	}
	if (isdigit(c)) {
		yylval.number = 0;
		do {
			yylval.number *= 10;
			yylval.number += c - '0';
		} while ((c = getc(fin)) != EOF && isdigit(c));
		ungetc(c, fin);
		if (debug > 1)
			fprintf(stderr, "number: %d\n", yylval.number);
		return NUMBER;
	}

#define allowed_in_string(x) \
	isalnum(x) || \
	( ispunct(x) && \
	x != '(' && \
	x != ')' && \
	x != '<' && \
	x != '>' && \
	x != '!' && \
	x != '=' && \
	x != '/' && \
	x != '#' )

	if (isalnum(c)) {
		p = buf;
		do {
			*p++ = c;
			if (p-buf >= sizeof buf)
				errx(1, "line %d: string too long", lineno);
		} while ((c = getc(fin)) != EOF && (allowed_in_string(c)));
		ungetc(c, fin);
		*p = '\0';
		token = lookup(buf);
		yylval.string = strdup(buf);
		return token;
	}
	if (c == '\n')
		lineno++;
	if (c == EOF)
		return 0;
	return c;
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

/*	$OpenBSD: pfctl_parser.c,v 1.138 2003/02/02 18:32:35 henning Exp $ */

/*
 * Copyright (c) 2001 Daniel Hartmeier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
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
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <stdarg.h>
#include <errno.h>
#include <err.h>
#include <ifaddrs.h>

#include "pfctl_parser.h"
#include "pfctl.h"

void		 print_op (u_int8_t, const char *, const char *);
void		 print_port (u_int8_t, u_int16_t, u_int16_t, char *);
void		 print_uid (u_int8_t, uid_t, uid_t, const char *);
void		 print_gid (u_int8_t, gid_t, gid_t, const char *);
void		 print_flags (u_int8_t);
void		 print_fromto(struct pf_rule_addr *, struct pf_rule_addr *,
		    u_int8_t, u_int8_t, int);

struct node_host	*host_if(const char *, const int);
struct node_host	*host_v4(const char *);
struct node_host	*host_v6(const char *, const int);
struct node_host	*host_dns(const char *, const int, const int);

char *tcpflags = "FSRPAUEW";

static const struct icmptypeent icmp_type[] = {
	{ "echoreq",	ICMP_ECHO },
	{ "echorep",	ICMP_ECHOREPLY },
	{ "unreach",	ICMP_UNREACH },
	{ "squench",	ICMP_SOURCEQUENCH },
	{ "redir",	ICMP_REDIRECT },
	{ "althost",	ICMP_ALTHOSTADDR },
	{ "routeradv",	ICMP_ROUTERADVERT },
	{ "routersol",	ICMP_ROUTERSOLICIT },
	{ "timex",	ICMP_TIMXCEED },
	{ "paramprob",	ICMP_PARAMPROB },
	{ "timereq",	ICMP_TSTAMP },
	{ "timerep",	ICMP_TSTAMPREPLY },
	{ "inforeq",	ICMP_IREQ },
	{ "inforep",	ICMP_IREQREPLY },
	{ "maskreq",	ICMP_MASKREQ },
	{ "maskrep",	ICMP_MASKREPLY },
	{ "trace",	ICMP_TRACEROUTE },
	{ "dataconv",	ICMP_DATACONVERR },
	{ "mobredir",	ICMP_MOBILE_REDIRECT },
	{ "ipv6-where",	ICMP_IPV6_WHEREAREYOU },
	{ "ipv6-here",	ICMP_IPV6_IAMHERE },
	{ "mobregreq",	ICMP_MOBILE_REGREQUEST },
	{ "mobregrep",	ICMP_MOBILE_REGREPLY },
	{ "skip",	ICMP_SKIP },
	{ "photuris",	ICMP_PHOTURIS }
};

static const struct icmptypeent icmp6_type[] = {
	{ "unreach",	ICMP6_DST_UNREACH },
	{ "toobig",	ICMP6_PACKET_TOO_BIG },
	{ "timex",	ICMP6_TIME_EXCEEDED },
	{ "paramprob",	ICMP6_PARAM_PROB },
	{ "echoreq",	ICMP6_ECHO_REQUEST },
	{ "echorep",	ICMP6_ECHO_REPLY },
	{ "groupqry",	ICMP6_MEMBERSHIP_QUERY },
	{ "listqry",	MLD6_LISTENER_QUERY },
	{ "grouprep",	ICMP6_MEMBERSHIP_REPORT },
	{ "listenrep",	MLD6_LISTENER_REPORT },
	{ "groupterm",	ICMP6_MEMBERSHIP_REDUCTION },
	{ "listendone", MLD6_LISTENER_DONE },
	{ "routersol",	ND_ROUTER_SOLICIT },
	{ "routeradv",	ND_ROUTER_ADVERT },
	{ "neighbrsol", ND_NEIGHBOR_SOLICIT },
	{ "neighbradv", ND_NEIGHBOR_ADVERT },
	{ "redir",	ND_REDIRECT },
	{ "routrrenum", ICMP6_ROUTER_RENUMBERING },
	{ "wrureq",	ICMP6_WRUREQUEST },
	{ "wrurep",	ICMP6_WRUREPLY },
	{ "fqdnreq",	ICMP6_FQDN_QUERY },
	{ "fqdnrep",	ICMP6_FQDN_REPLY },
	{ "niqry",	ICMP6_NI_QUERY },
	{ "nirep",	ICMP6_NI_REPLY },
	{ "mtraceresp",	MLD6_MTRACE_RESP },
	{ "mtrace",	MLD6_MTRACE }
};

static const struct icmpcodeent icmp_code[] = {
	{ "net-unr",		ICMP_UNREACH,	ICMP_UNREACH_NET },
	{ "host-unr",		ICMP_UNREACH,	ICMP_UNREACH_HOST },
	{ "proto-unr",		ICMP_UNREACH,	ICMP_UNREACH_PROTOCOL },
	{ "port-unr",		ICMP_UNREACH,	ICMP_UNREACH_PORT },
	{ "needfrag",		ICMP_UNREACH,	ICMP_UNREACH_NEEDFRAG },
	{ "srcfail",		ICMP_UNREACH,	ICMP_UNREACH_SRCFAIL },
	{ "net-unk",		ICMP_UNREACH,	ICMP_UNREACH_NET_UNKNOWN },
	{ "host-unk",		ICMP_UNREACH,	ICMP_UNREACH_HOST_UNKNOWN },
	{ "isolate",		ICMP_UNREACH,	ICMP_UNREACH_ISOLATED },
	{ "net-prohib",		ICMP_UNREACH,	ICMP_UNREACH_NET_PROHIB },
	{ "host-prohib",	ICMP_UNREACH,	ICMP_UNREACH_HOST_PROHIB },
	{ "net-tos",		ICMP_UNREACH,	ICMP_UNREACH_TOSNET },
	{ "host-tos",		ICMP_UNREACH,	ICMP_UNREACH_TOSHOST },
	{ "filter-prohib",	ICMP_UNREACH,	ICMP_UNREACH_FILTER_PROHIB },
	{ "host-preced",	ICMP_UNREACH,	ICMP_UNREACH_HOST_PRECEDENCE },
	{ "cutoff-preced",	ICMP_UNREACH,	ICMP_UNREACH_PRECEDENCE_CUTOFF },
	{ "redir-net",		ICMP_REDIRECT,	ICMP_REDIRECT_NET },
	{ "redir-host",		ICMP_REDIRECT,	ICMP_REDIRECT_HOST },
	{ "redir-tos-net",	ICMP_REDIRECT,	ICMP_REDIRECT_TOSNET },
	{ "redir-tos-host",	ICMP_REDIRECT,	ICMP_REDIRECT_TOSHOST },
	{ "normal-adv",		ICMP_ROUTERADVERT, ICMP_ROUTERADVERT_NORMAL },
	{ "common-adv",		ICMP_ROUTERADVERT, ICMP_ROUTERADVERT_NOROUTE_COMMON },
	{ "transit",		ICMP_TIMXCEED,	ICMP_TIMXCEED_INTRANS },
	{ "reassemb",		ICMP_TIMXCEED,	ICMP_TIMXCEED_REASS },
	{ "badhead",		ICMP_PARAMPROB,	ICMP_PARAMPROB_ERRATPTR },
	{ "optmiss",		ICMP_PARAMPROB,	ICMP_PARAMPROB_OPTABSENT },
	{ "badlen",		ICMP_PARAMPROB,	ICMP_PARAMPROB_LENGTH },
	{ "unknown-ind",	ICMP_PHOTURIS,	ICMP_PHOTURIS_UNKNOWN_INDEX },
	{ "auth-fail",		ICMP_PHOTURIS,	ICMP_PHOTURIS_AUTH_FAILED },
	{ "decrypt-fail",	ICMP_PHOTURIS,	ICMP_PHOTURIS_DECRYPT_FAILED }
};

static const struct icmpcodeent icmp6_code[] = {
	{ "admin-unr", ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_ADMIN },
	{ "noroute-unr", ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_NOROUTE },
	{ "notnbr-unr",	ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_NOTNEIGHBOR },
	{ "beyond-unr", ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_BEYONDSCOPE },
	{ "addr-unr", ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_ADDR },
	{ "port-unr", ICMP6_DST_UNREACH, ICMP6_DST_UNREACH_NOPORT },
	{ "transit", ICMP6_TIME_EXCEEDED, ICMP6_TIME_EXCEED_TRANSIT },
	{ "reassemb", ICMP6_TIME_EXCEEDED, ICMP6_TIME_EXCEED_REASSEMBLY },
	{ "badhead", ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER },
	{ "nxthdr", ICMP6_PARAM_PROB, ICMP6_PARAMPROB_NEXTHEADER },
	{ "redironlink", ND_REDIRECT, ND_REDIRECT_ONLINK },
	{ "redirrouter", ND_REDIRECT, ND_REDIRECT_ROUTER }
};

const struct pf_timeout pf_timeouts[] = {
	{ "tcp.first",		PFTM_TCP_FIRST_PACKET },
	{ "tcp.opening",	PFTM_TCP_OPENING },
	{ "tcp.established",	PFTM_TCP_ESTABLISHED },
	{ "tcp.closing",	PFTM_TCP_CLOSING },
	{ "tcp.finwait",	PFTM_TCP_FIN_WAIT },
	{ "tcp.closed",		PFTM_TCP_CLOSED },
	{ "udp.first",		PFTM_UDP_FIRST_PACKET },
	{ "udp.single",		PFTM_UDP_SINGLE },
	{ "udp.multiple",	PFTM_UDP_MULTIPLE },
	{ "icmp.first",		PFTM_ICMP_FIRST_PACKET },
	{ "icmp.error",		PFTM_ICMP_ERROR_REPLY },
	{ "other.first",	PFTM_OTHER_FIRST_PACKET },
	{ "other.single",	PFTM_OTHER_SINGLE },
	{ "other.multiple",	PFTM_OTHER_MULTIPLE },
	{ "frag",		PFTM_FRAG },
	{ "interval",		PFTM_INTERVAL },
	{ NULL,			0 }
};

const struct icmptypeent *
geticmptypebynumber(u_int8_t type, sa_family_t af)
{
	unsigned int	i;

	if (af != AF_INET6) {
		for (i=0; i < (sizeof (icmp_type) / sizeof(icmp_type[0]));
		    i++) {
			if (type == icmp_type[i].type)
				return (&icmp_type[i]);
		}
	} else {
		for (i=0; i < (sizeof (icmp6_type) /
		    sizeof(icmp6_type[0])); i++) {
			if (type == icmp6_type[i].type)
				 return (&icmp6_type[i]);
		}
	}
	return (NULL);
}

const struct icmptypeent *
geticmptypebyname(char *w, sa_family_t af)
{
	unsigned int	i;

	if (af != AF_INET6) {
		for (i=0; i < (sizeof (icmp_type) / sizeof(icmp_type[0]));
		    i++) {
			if (!strcmp(w, icmp_type[i].name))
				return (&icmp_type[i]);
		}
	} else {
		for (i=0; i < (sizeof (icmp6_type) /
		    sizeof(icmp6_type[0])); i++) {
			if (!strcmp(w, icmp6_type[i].name))
				return (&icmp6_type[i]);
		}
	}
	return (NULL);
}

const struct icmpcodeent *
geticmpcodebynumber(u_int8_t type, u_int8_t code, sa_family_t af)
{
	unsigned int	i;

	if (af != AF_INET6) {
		for (i=0; i < (sizeof (icmp_code) / sizeof(icmp_code[0]));
		    i++) {
			if (type == icmp_code[i].type &&
			    code == icmp_code[i].code)
				return (&icmp_code[i]);
		}
	} else {
		for (i=0; i < (sizeof (icmp6_code) /
		   sizeof(icmp6_code[0])); i++) {
			if (type == icmp6_code[i].type &&
			    code == icmp6_code[i].code)
				return (&icmp6_code[i]);
		}
	}
	return (NULL);
}

const struct icmpcodeent *
geticmpcodebyname(u_long type, char *w, sa_family_t af)
{
	unsigned int	i;

	if (af != AF_INET6) {
		for (i=0; i < (sizeof (icmp_code) / sizeof(icmp_code[0]));
		    i++) {
			if (type == icmp_code[i].type &&
			    !strcmp(w, icmp_code[i].name))
				return (&icmp_code[i]);
		}
	} else {
		for (i=0; i < (sizeof (icmp6_code) /
		    sizeof(icmp6_code[0])); i++) {
			if (type == icmp6_code[i].type &&
			    !strcmp(w, icmp6_code[i].name))
				return (&icmp6_code[i]);
		}
	}
	return (NULL);
}

void
print_op(u_int8_t op, const char *a1, const char *a2)
{
	if (op == PF_OP_IRG)
		printf("%s >< %s ", a1, a2);
	else if (op == PF_OP_XRG)
		printf("%s <> %s ", a1, a2);
	else if (op == PF_OP_EQ)
		printf("= %s ", a1);
	else if (op == PF_OP_NE)
		printf("!= %s ", a1);
	else if (op == PF_OP_LT)
		printf("< %s ", a1);
	else if (op == PF_OP_LE)
		printf("<= %s ", a1);
	else if (op == PF_OP_GT)
		printf("> %s ", a1);
	else if (op == PF_OP_GE)
		printf(">= %s ", a1);
}

void
print_port(u_int8_t op, u_int16_t p1, u_int16_t p2, char *proto)
{
	char		 a1[6], a2[6];
	struct servent	*s;

	s = getservbyport(p1, proto);
	p1 = ntohs(p1);
	p2 = ntohs(p2);
	snprintf(a1, sizeof(a1), "%u", p1);
	snprintf(a2, sizeof(a2), "%u", p2);
	printf("port ");
	if (s != NULL && (op == PF_OP_EQ || op == PF_OP_NE))
		print_op(op, s->s_name, a2);
	else
		print_op(op, a1, a2);
}

void
print_uid(u_int8_t op, uid_t u1, uid_t u2, const char *t)
{
	char	a1[11], a2[11];

	snprintf(a1, sizeof(a1), "%u", u1);
	snprintf(a2, sizeof(a2), "%u", u2);
	printf("%s ", t);
	if (u1 == UID_MAX && (op == PF_OP_EQ || op == PF_OP_NE))
		print_op(op, "unknown", a2);
	else
		print_op(op, a1, a2);
}

void
print_gid(u_int8_t op, gid_t g1, gid_t g2, const char *t)
{
	char	a1[11], a2[11];

	snprintf(a1, sizeof(a1), "%u", g1);
	snprintf(a2, sizeof(a2), "%u", g2);
	printf("%s ", t);
	if (g1 == GID_MAX && (op == PF_OP_EQ || op == PF_OP_NE))
		print_op(op, "unknown", a2);
	else
		print_op(op, a1, a2);
}

void
print_flags(u_int8_t f)
{
	int	i;

	for (i = 0; tcpflags[i]; ++i)
		if (f & (1 << i))
			printf("%c", tcpflags[i]);
}

void
print_fromto(struct pf_rule_addr *src, struct pf_rule_addr *dst,
    sa_family_t af, u_int8_t proto, int verbose)
{
	if (src->addr.type != PF_ADDR_NOROUTE &&
	    dst->addr.type != PF_ADDR_NOROUTE &&
	    PF_AZERO(&src->addr.v.a.addr, AF_INET6) &&
	    PF_AZERO(&src->addr.v.a.mask, AF_INET6) &&
	    !src->port_op && PF_AZERO(&dst->addr.v.a.addr, AF_INET6) &&
	    PF_AZERO(&dst->addr.v.a.mask, AF_INET6) && !dst->port_op)
		printf("all ");
	else {
		printf("from ");
		if (src->addr.type == PF_ADDR_NOROUTE)
			printf("no-route ");
		else if (PF_AZERO(&src->addr.v.a.addr, AF_INET6) &&
		    PF_AZERO(&src->addr.v.a.mask, AF_INET6))
			printf("any ");
		else {
			if (src->not)
				printf("! ");
			print_addr(&src->addr, af, verbose);
			printf(" ");
		}
		if (src->port_op)
			print_port(src->port_op, src->port[0],
			    src->port[1],
			    proto == IPPROTO_TCP ? "tcp" : "udp");

		printf("to ");
		if (dst->addr.type == PF_ADDR_NOROUTE)
			printf("no-route ");
		else if (PF_AZERO(&dst->addr.v.a.addr, AF_INET6) &&
		    PF_AZERO(&dst->addr.v.a.mask, AF_INET6))
			printf("any ");
		else {
			if (dst->not)
				printf("! ");
			print_addr(&dst->addr, af, verbose);
			printf(" ");
		}
		if (dst->port_op)
			print_port(dst->port_op, dst->port[0],
			    dst->port[1],
			    proto == IPPROTO_TCP ? "tcp" : "udp");
	}
}

void
print_rule(struct pf_rule *r, int verbose)
{
	switch (r->action) {
	case PF_NAT:
	case PF_NONAT:
		print_nat(r, verbose);
		break;
	case PF_BINAT:
	case PF_NOBINAT:
		print_binat(r, verbose);
		break;
	case PF_RDR:
	case PF_NORDR:
		print_rdr(r, verbose);
		break;
	default:
	case PF_PASS:
	case PF_DROP:
	case PF_SCRUB:
		print_filter(r, verbose);
		break;
	}
}

void
print_pool(struct pf_pool *pool, u_int16_t p1, u_int16_t p2,
    sa_family_t af, int id)
{
	struct pf_pooladdr	*pooladdr;

	if ((TAILQ_FIRST(&pool->list) != NULL) &&
	    TAILQ_NEXT(TAILQ_FIRST(&pool->list), entries) != NULL)
		printf("{ ");
	TAILQ_FOREACH(pooladdr, &pool->list, entries){
		switch (id) {
		case PF_NAT:
		case PF_RDR:
		case PF_BINAT:
			print_addr(&pooladdr->addr.addr, af, 0);
			break;
		case PF_PASS:
			if (PF_AZERO(&pooladdr->addr.addr.v.a.addr, af))
				printf("%s", pooladdr->ifname);
			else {
				printf("(%s ", pooladdr->ifname);
				print_addr(&pooladdr->addr.addr, af, 0);
				printf(")");
			}
			break;
		default:
			break;
		}
		if (TAILQ_NEXT(pooladdr, entries) != NULL)
			printf(", ");
		else if (TAILQ_NEXT(TAILQ_FIRST(&pool->list), entries) != NULL)
			printf(" }");
	}
	switch (id) {
	case PF_NAT:
		if (p1 != PF_NAT_PROXY_PORT_LOW ||
		    p2 != PF_NAT_PROXY_PORT_HIGH) {
			if (p1 == p2)
				printf(" port %u", p1);
			else
				printf(" port %u:%u", p1, p2);
		}
		break;
	case PF_RDR:
		if (p1) {
			printf(" port %u", ntohs(p1));
			if (p2)
				printf(":%u", ntohs(p2));
		}
		break;
	default:
		break;
	}
	switch (pool->opts & PF_POOL_TYPEMASK) {
	case PF_POOL_NONE:
		break;
	case PF_POOL_BITMASK:
		printf(" bitmask");
		break;
	case PF_POOL_RANDOM:
		printf(" random");
		break;
	case PF_POOL_SRCHASH:
		printf(" source-hash 0x%08x%08x%08x%08x",
		    pool->key.key32[0], pool->key.key32[1],
		    pool->key.key32[2], pool->key.key32[3]);
		break;
	case PF_POOL_ROUNDROBIN:
		printf(" round-robin");
		break;
	}
	if (pool->opts & PF_POOL_STATICPORT)
		printf(" static-port");
}

void
print_nat(struct pf_rule *n, int verbose)
{
	if (verbose)
		printf("@%d ", n->nr);
	if (n->anchorname[0])
		printf("nat-anchor %s ", n->anchorname);
	else {
		if (n->action == PF_NONAT)
			printf("no ");
		printf("nat ");
	}
	if (n->ifname[0]) {
		printf("on ");
		if (n->ifnot)
			printf("! ");
		printf("%s ", n->ifname);
	}
	if (n->af) {
		if (n->af == AF_INET)
			printf("inet ");
		else
			printf("inet6 ");
	}
	if (n->proto) {
		struct protoent	*p;

		if ((p = getprotobynumber(n->proto)) != NULL)
			printf("proto %s ", p->p_name);
		else
			printf("proto %u ", n->proto);
	}
	print_fromto(&n->src, &n->dst, n->af, n->proto, verbose);
	if (!n->anchorname[0] && (n->action == PF_NAT)) {
		printf("-> ");
		print_pool(&n->rpool, n->rpool.proxy_port[0],
		    n->rpool.proxy_port[1], n->af, PF_NAT);
	}
	printf("\n");
}

void
print_binat(struct pf_rule *b, int verbose)
{
	if (verbose)
		printf("@%d ", b->nr);
	if (b->anchorname[0])
		printf("binat-anchor %s ", b->anchorname);
	else {
		if (b->action == PF_NOBINAT)
			printf("no ");
		printf("binat ");
	}
	if (b->ifname[0]) {
		printf("on ");
		printf("%s ", b->ifname);
	}
	if (b->af) {
		if (b->af == AF_INET)
			printf("inet ");
		else
			printf("inet6 ");
	}
	if (b->proto) {
		struct protoent	*p;

		if ((p = getprotobynumber(b->proto)) != NULL)
			printf("proto %s ", p->p_name);
		else
			printf("proto %u ", b->proto);
	}
	printf("from ");
	if (!PF_AZERO(&b->src.addr.v.a.addr, b->af) ||
	    !PF_AZERO(&b->src.addr.v.a.mask, b->af)) {
		print_addr(&b->src.addr, b->af, verbose);
		printf(" ");
	} else
		printf("any ");
	printf("to ");
	if (!PF_AZERO(&b->dst.addr.v.a.addr, b->af) ||
	    !PF_AZERO(&b->dst.addr.v.a.mask, b->af)) {
		if (b->dst.not)
			printf("! ");
		print_addr(&b->dst.addr, b->af, verbose);
		printf(" ");
	} else
		printf("any ");
	if (!b->anchorname[0] && (b->action == PF_BINAT)) {
		printf("-> ");
		print_pool(&b->rpool, 0, 0, b->af, PF_BINAT);
	}
	printf("\n");
}

void
print_rdr(struct pf_rule *r, int verbose)
{
	if (verbose)
		printf("@%d ", r->nr);
	if (r->anchorname[0])
		printf("rdr-anchor %s ", r->anchorname);
	else {
		if (r->action == PF_NORDR)
			printf("no ");
		printf("rdr ");
	}
	if (r->ifname[0]) {
		printf("on ");
		if (r->ifnot)
			printf("! ");
		printf("%s ", r->ifname);
	}
	if (r->af) {
		if (r->af == AF_INET)
			printf("inet ");
		else
			printf("inet6 ");
	}
	if (r->proto) {
		struct protoent	*p;

		if ((p = getprotobynumber(r->proto)) != NULL)
			printf("proto %s ", p->p_name);
		else
			printf("proto %u ", r->proto);
	}
	printf("from ");
	if (!PF_AZERO(&r->src.addr.v.a.addr, r->af) ||
	    !PF_AZERO(&r->src.addr.v.a.mask, r->af)) {
		if (r->src.not)
			printf("! ");
		print_addr(&r->src.addr, r->af, verbose);
		printf(" ");
	} else
		printf("any ");
	printf("to ");
	if (!PF_AZERO(&r->dst.addr.v.a.addr, r->af) ||
	    !PF_AZERO(&r->dst.addr.v.a.mask, r->af)) {
		if (r->dst.not)
			printf("! ");
		print_addr(&r->dst.addr, r->af, verbose);
		printf(" ");
	} else
		printf("any ");
	if (r->dst.port[0]) {
		printf("port %u", ntohs(r->dst.port[0]));
		if (r->dst.port_op & PF_OP_RRG)
			printf(":%u", ntohs(r->dst.port[1]));
		printf(" ");
	}
	if (!r->anchorname[0] && (r->action == PF_RDR)) {
		printf("-> ");
		print_pool(&r->rpool, r->rpool.proxy_port[0],
		    r->rpool.proxy_port[1], r->af, PF_RDR);
	}
	printf("\n");
}

char	*pf_reasons[PFRES_MAX+1] = PFRES_NAMES;
char	*pf_fcounters[FCNT_MAX+1] = FCNT_NAMES;

void
print_status(struct pf_status *s)
{
	char	statline[80];
	time_t	runtime;
	int	i;

	runtime = time(NULL) - s->since;

	if (s->running) {
		unsigned	sec, min, hrs, day = runtime;

		sec = day % 60;
		day /= 60;
		min = day % 60;
		day /= 60;
		hrs = day % 24;
		day /= 24;
		snprintf(statline, sizeof(statline),
		    "Status: Enabled for %u days %.2u:%.2u:%.2u",
		    day, hrs, min, sec);
	} else
		snprintf(statline, sizeof(statline), "Status: Disabled");
	printf("%-44s", statline);
	switch (s->debug) {
	case 0:
		printf("%15s\n\n", "Debug: None");
		break;
	case 1:
		printf("%15s\n\n", "Debug: Urgent");
		break;
	case 2:
		printf("%15s\n\n", "Debug: Misc");
		break;
	}
	if (s->ifname[0] != 0) {
		printf("Interface Stats for %-16s %5s %16s\n",
		    s->ifname, "IPv4", "IPv6");
		printf("  %-25s %14llu %16llu\n", "Bytes In",
		    s->bcounters[0][0], s->bcounters[1][0]);
		printf("  %-25s %14llu %16llu\n", "Bytes Out",
		    s->bcounters[0][1], s->bcounters[1][1]);
		printf("  Packets In\n");
		printf("    %-23s %14llu %16llu\n", "Passed",
		    s->pcounters[0][0][PF_PASS],
		    s->pcounters[1][0][PF_PASS]);
		printf("    %-23s %14llu %16llu\n", "Blocked",
		    s->pcounters[0][0][PF_DROP],
		    s->pcounters[1][0][PF_DROP]);
		printf("  Packets Out\n");
		printf("    %-23s %14llu %16llu\n", "Passed",
		    s->pcounters[0][1][PF_PASS],
		    s->pcounters[1][1][PF_PASS]);
		printf("    %-23s %14llu %16llu\n\n", "Blocked",
		    s->pcounters[0][1][PF_DROP],
		    s->pcounters[1][1][PF_DROP]);
	}
	printf("%-27s %14s %16s\n", "State Table", "Total", "Rate");
	printf("  %-25s %14u %14s\n", "current entries", s->states, "");
	for (i = 0; i < FCNT_MAX; i++) {
		printf("  %-25s %14lld ", pf_fcounters[i],
			    s->fcounters[i]);
		if (runtime > 0)
			printf("%14.1f/s\n",
			    (double)s->fcounters[i] / (double)runtime);
		else
			printf("%14s\n", "");
	}
	printf("Counters\n");
	for (i = 0; i < PFRES_MAX; i++) {
		printf("  %-25s %14lld ", pf_reasons[i],
		    s->counters[i]);
		if (runtime > 0)
			printf("%14.1f/s\n",
			    (double)s->counters[i] / (double)runtime);
		else
			printf("%14s\n", "");
	}
}

void
print_filter(struct pf_rule *r, int verbose)
{
	int	i, opts;

	if (verbose)
		printf("@%d ", r->nr);
	if (r->anchorname[0])
		printf("anchor %s ", r->anchorname);
	else if (r->action == PF_PASS)
		printf("pass ");
	else if (r->action == PF_DROP) {
		printf("block ");
		if (r->rule_flag & PFRULE_RETURN)
			printf("return ");
		else if (r->rule_flag & PFRULE_RETURNRST) {
			if (!r->return_ttl)
				printf("return-rst ");
			else
				printf("return-rst(ttl %d) ", r->return_ttl);
		} else if (r->rule_flag & PFRULE_RETURNICMP) {
			const struct icmpcodeent	*ic, *ic6;

			ic = geticmpcodebynumber(r->return_icmp >> 8,
			    r->return_icmp & 255, AF_INET);
			ic6 = geticmpcodebynumber(r->return_icmp6 >> 8,
			    r->return_icmp6 & 255, AF_INET6);

			switch(r->af) {
			case AF_INET:
				printf("return-icmp");
				if (ic == NULL)
					printf("(%u) ", r->return_icmp & 255);
				else
					printf("(%s) ", ic->name);
				break;
			case AF_INET6:
				printf("return-icmp6");
				if (ic6 == NULL)
					printf("(%u) ", r->return_icmp6 & 255);
				else
					printf("(%s) ", ic6->name);
				break;
			default:
				printf("return-icmp");
				if (ic == NULL)
					printf("(%u, ", r->return_icmp & 255);
				else
					printf("(%s, ", ic->name);
				if (ic6 == NULL)
					printf("%u) ", r->return_icmp6 & 255);
				else
					printf("%s) ", ic6->name);
				break;
			}
		} else
			printf("drop ");
	} else
		printf("scrub ");
	if (r->direction == PF_IN)
		printf("in ");
	else if (r->direction == PF_OUT)
		printf("out ");
	if (r->log == 1)
		printf("log ");
	else if (r->log == 2)
		printf("log-all ");
	if (r->quick)
		printf("quick ");
	if (r->ifname[0]) {
		if (r->ifnot)
			printf("on ! %s ", r->ifname);
		else
			printf("on %s ", r->ifname);
	}
	if (r->rt) {
		if (r->rt == PF_ROUTETO)
			printf("route-to ");
		else if (r->rt == PF_REPLYTO)
			printf("reply-to ");
		else if (r->rt == PF_DUPTO)
			printf("dup-to ");
		else if (r->rt == PF_FASTROUTE)
			printf("fastroute ");
		if (r->rt != PF_FASTROUTE) {
			print_pool(&r->rpool, 0, 0, r->af, PF_PASS);
			printf(" ");
		}
	}
	if (r->af) {
		if (r->af == AF_INET)
			printf("inet ");
		else
			printf("inet6 ");
	}
	if (r->proto) {
		struct protoent	*p;

		if ((p = getprotobynumber(r->proto)) != NULL)
			printf("proto %s ", p->p_name);
		else
			printf("proto %u ", r->proto);
	}
	print_fromto(&r->src, &r->dst, r->af, r->proto, verbose);
	if (r->uid.op)
		print_uid(r->uid.op, r->uid.uid[0], r->uid.uid[1], "user");
	if (r->gid.op)
		print_gid(r->gid.op, r->gid.gid[0], r->gid.gid[1], "group");
	if (r->flags || r->flagset) {
		printf("flags ");
		print_flags(r->flags);
		printf("/");
		print_flags(r->flagset);
		printf(" ");
	}
	if (r->type) {
		const struct icmptypeent	*it;

		it = geticmptypebynumber(r->type-1, r->af);
		if (r->af != AF_INET6)
			printf("icmp-type");
		else
			printf("icmp6-type");
		if (it != NULL)
			printf(" %s ", it->name);
		else
			printf(" %u ", r->type-1);
		if (r->code) {
			const struct icmpcodeent	*ic;

			ic = geticmpcodebynumber(r->type-1, r->code-1, r->af);
			if (ic != NULL)
				printf("code %s ", ic->name);
			else
				printf("code %u ", r->code-1);
		}
	}
	if (r->tos)
		printf("tos 0x%2.2x ", r->tos);
	if (r->keep_state == PF_STATE_NORMAL)
		printf("keep state ");
	else if (r->keep_state == PF_STATE_MODULATE)
		printf("modulate state ");
	opts = 0;
	if (r->max_states)
		opts = 1;
	for (i = 0; !opts && i < PFTM_MAX; ++i)
		if (r->timeout[i])
			opts = 1;
	if (opts) {
		printf("(");
		if (r->max_states) {
			printf("max %u", r->max_states);
			opts = 0;
		}
		for (i = 0; i < PFTM_MAX; ++i)
			if (r->timeout[i]) {
				if (!opts)
					printf(", ");
				opts = 0;
				printf("%s %u", pf_timeouts[i].name,
				    r->timeout[i]);
			}
		printf(") ");
	}
	if (r->rule_flag & PFRULE_FRAGMENT)
		printf("fragment ");
	if (r->rule_flag & PFRULE_NODF)
		printf("no-df ");
	if (r->min_ttl)
		printf("min-ttl %d ", r->min_ttl);
	if (r->max_mss)
		printf("max-mss %d ", r->max_mss);
	if (r->allow_opts)
		printf("allow-opts ");
	if (r->action == PF_SCRUB) {
		if (r->rule_flag & PFRULE_FRAGDROP)
			printf("fragment drop-ovl ");
		else if (r->rule_flag & PFRULE_FRAGCROP)
			printf("fragment crop ");
		else
			printf("fragment reassemble ");
	}
	if (r->label[0])
		printf("label \"%s\" ", r->label);
	if (r->qname[0] && r->pqname[0])
		printf("queue(%s, %s) ", r->qname, r->pqname);
	else if (r->qname[0])
		printf("queue %s ", r->qname);

	printf("\n");
}

int
parse_flags(char *s)
{
	char		*p, *q;
	u_int8_t	 f = 0;

	for (p = s; *p; p++) {
		if ((q = strchr(tcpflags, *p)) == NULL)
			return -1;
		else
			f |= 1 << (q - tcpflags);
	}
	return (f ? f : PF_TH_ALL);
}

void
set_ipmask(struct node_host *h, u_int8_t b)
{
	struct pf_addr	*m, *n;
	int		 i, j = 0;

	m = &h->addr.v.a.mask;

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
	n = &h->addr.v.a.addr;
	for (i = 0; i < 4; i++)
		n->addr32[i] = n->addr32[i] & m->addr32[i];
}

/* interface lookup routines */

struct node_host	*iftab;

void
ifa_load(void)
{
	struct ifaddrs		*ifap, *ifa;
	struct node_host	*n = NULL, *h = NULL;

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
		n->ifa_flags = ifa->ifa_flags;
#ifdef __KAME__
		if (n->af == AF_INET6 &&
		    IN6_IS_ADDR_LINKLOCAL(&((struct sockaddr_in6 *)
		    ifa->ifa_addr)->sin6_addr) &&
		    ((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_scope_id == 0) {
			struct sockaddr_in6	*sin6;

			sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
			sin6->sin6_scope_id = sin6->sin6_addr.s6_addr[2] << 8 |
			    sin6->sin6_addr.s6_addr[3];
			sin6->sin6_addr.s6_addr[2] = 0;
			sin6->sin6_addr.s6_addr[3] = 0;
		}
#endif
		n->ifindex = 0;
		if (n->af == AF_INET) {
			memcpy(&n->addr.v.a.addr, &((struct sockaddr_in *)
			    ifa->ifa_addr)->sin_addr.s_addr,
			    sizeof(struct in_addr));
			memcpy(&n->addr.v.a.mask, &((struct sockaddr_in *)
			    ifa->ifa_netmask)->sin_addr.s_addr,
			    sizeof(struct in_addr));
			if (ifa->ifa_broadaddr != NULL)
				memcpy(&n->bcast, &((struct sockaddr_in *)
				    ifa->ifa_broadaddr)->sin_addr.s_addr,
				    sizeof(struct in_addr));
		} else if (n->af == AF_INET6) {
			memcpy(&n->addr.v.a.addr, &((struct sockaddr_in6 *)
			    ifa->ifa_addr)->sin6_addr.s6_addr,
			    sizeof(struct in6_addr));
			memcpy(&n->addr.v.a.mask, &((struct sockaddr_in6 *)
			    ifa->ifa_netmask)->sin6_addr.s6_addr,
			    sizeof(struct in6_addr));
			if (ifa->ifa_broadaddr != NULL)
				memcpy(&n->bcast, &((struct sockaddr_in6 *)
				    ifa->ifa_broadaddr)->sin6_addr.s6_addr,
				    sizeof(struct in6_addr));
			n->ifindex = ((struct sockaddr_in6 *)
			    ifa->ifa_addr)->sin6_scope_id;
		}
		if ((n->ifname = strdup(ifa->ifa_name)) == NULL)
			err(1, "ifa_load: strdup");
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
ifa_exists(const char *ifa_name)
{
	struct node_host	*n;

	if (iftab == NULL)
		ifa_load();

	for (n = iftab; n; n = n->next) {
		if (n->af == AF_LINK && !strncmp(n->ifname, ifa_name, IFNAMSIZ))
			return (n);
	}
	return (NULL);
}

struct node_host *
ifa_lookup(const char *ifa_name, enum pfctl_iflookup_mode mode)
{
	struct node_host	*p = NULL, *h = NULL, *n = NULL;
	int			 return_all = 0;

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
		if (mode == PFCTL_IFLOOKUP_BCAST)
			memcpy(&n->addr.v.a.addr, &p->bcast,
			    sizeof(struct pf_addr));
		else
			memcpy(&n->addr.v.a.addr, &p->addr.v.a.addr,
			    sizeof(struct pf_addr));
		if (mode == PFCTL_IFLOOKUP_NET)
			set_ipmask(n, unmask(&p->addr.v.a.mask, n->af));
		else {
			if (n->af == AF_INET) {
				if (p->ifa_flags & IFF_LOOPBACK &&
				    p->ifa_flags & IFF_LINK1)
					memcpy(&n->addr.v.a.mask,
					    &p->addr.v.a.mask,
					    sizeof(struct pf_addr));
				else
					set_ipmask(n, 32);
			} else
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
		fprintf(stderr, "no IP address found for %s\n", ifa_name);
	}
	return (h);
}

struct node_host *
host(const char *s, int mask)
{
	struct node_host	*h = NULL;
	int			 v4mask, v6mask, cont = 1;
	char			*buf = NULL, *p, *q, *ps;

	if ((p = strrchr(s, '/')) != NULL) {
		if (mask != -1) {
			fprintf(stderr, "address with netmask specified"
			    "and extra netmask supplied\n");
			return (NULL);
		}
		mask = strtol(p+1, &q, 0);
		if (!q || *q) {
			fprintf(stderr, "invalid netmask\n");
			return (NULL);
		}
		if ((buf = strdup(s)) == NULL)
			err(1, "host: strdup");
		if ((ps = malloc(strlen(s) - strlen(p) + 1)) == NULL)
			err(1, "host: malloc");
		strlcpy(ps, s, strlen(s) - strlen(p) + 1);
		v4mask = v6mask = mask;
	} else {
		if (asprintf(&ps, "%s", s) == -1)
			err(1, "host: asprintf");
		if (mask == -1) {
			if (asprintf(&buf, "%s", s) == -1)
				err(1, "host: asprintf");
			v4mask = 32;
			v6mask = 128;
		} else if (mask <= 128) {
			if (asprintf(&buf, "%s/%d", s, mask) == -1)
				err(1, "host: asprintf");
			v4mask = v6mask = mask;
		} else {
			fprintf(stderr, "illegal mask\n");
			return (NULL);
		}
	}

	/* interface with this name exists? */
	if (cont && (h = host_if(ps, mask)) != NULL)
		cont = 0;

	/* IPv4 address? */
	if (cont && (h = host_v4(buf)) != NULL)
		cont = 0;
	free(buf);

	/* IPv6 address? */
	if (cont && (h = host_v6(ps, v6mask)) != NULL)
		cont = 0;

	/* dns lookup */
	if (cont && (h = host_dns(ps, v4mask, v6mask)) != NULL)
		cont = 0;
	free(ps);

	if (h == NULL || cont == 1) {
		fprintf(stderr, "no IP address found for %s\n", s);
		return (NULL);
	}
	return (h);
}

struct node_host *
host_if(const char *s, const int mask)
{
	struct node_host	*n, *h = NULL;
	char			*p, *ps;
	int			 mode = PFCTL_IFLOOKUP_HOST;

	if ((p = strrchr(s, ':')) != NULL &&
	    (!strcmp(p+1, "network") || !strcmp(p+1, "broadcast"))) {
		if (!strcmp(p+1, "network"))
			mode = PFCTL_IFLOOKUP_NET;
		if (!strcmp(p+1, "broadcast"))
			mode = PFCTL_IFLOOKUP_BCAST;
		if (mask > -1) {
			fprintf(stderr, "network or broadcast lookup, but "
			    "extra netmask given\n");
			return (NULL);
		}
		if ((ps = malloc(strlen(s) - strlen(p) + 1)) == NULL)
			err(1, "host: malloc");
		strlcpy(ps, s, strlen(s) - strlen(p) + 1);
	} else
		if ((ps = strdup(s)) == NULL)
			err(1, "host_if: strdup");

	if (ifa_exists(ps) || !strncmp(ps, "self", IFNAMSIZ)) {
		/* interface with this name exists */
		h = ifa_lookup(ps, mode);
		for (n = h; n != NULL && mask > -1; n = n->next)
			set_ipmask(n, mask);
	}

	free(ps);
	return (h);
}

struct node_host *
host_v4(const char *s)
{
	struct node_host	*h = NULL;
	struct in_addr		 ina;
	int			 bits;

	memset(&ina, 0, sizeof(struct in_addr));
	if ((bits = inet_net_pton(AF_INET, s, &ina, sizeof(&ina))) > -1) {
		h = calloc(1, sizeof(struct node_host));
		if (h == NULL)
			err(1, "address: calloc");
		h->ifname = NULL;
		h->af = AF_INET;
		h->addr.v.a.addr.addr32[0] = ina.s_addr;
		set_ipmask(h, bits);
		h->next = NULL;
		h->tail = h;
	}

	return (h);
}

struct node_host *
host_v6(const char *s, const int mask)
{
	struct addrinfo		 hints, *res;
	struct node_host	*h = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM; /*dummy*/
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(s, "0", &hints, &res) == 0) {
		h = calloc(1, sizeof(struct node_host));
		if (h == NULL)
			err(1, "address: calloc");
		h->ifname = NULL;
		h->af = AF_INET6;
		memcpy(&h->addr.v.a.addr,
		    &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr,
		    sizeof(h->addr.v.a.addr));
		h->ifindex =
		    ((struct sockaddr_in6 *)res->ai_addr)->sin6_scope_id;
		set_ipmask(h, mask);
		freeaddrinfo(res);
		h->next = NULL;
		h->tail = h;
	}

	return (h);
}

struct node_host *
host_dns(const char *s, const int v4mask, const int v6mask)
{
	struct addrinfo		 hints, *res0, *res;
	struct node_host	*n, *h = NULL;
	int			 error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM; /* DUMMY */
	error = getaddrinfo(s, NULL, &hints, &res0);
	if (error)
		return (h);

	for (res = res0; res; res = res->ai_next) {
		if (res->ai_family != AF_INET &&
		    res->ai_family != AF_INET6)
			continue;
		n = calloc(1, sizeof(struct node_host));
		if (n == NULL)
			err(1, "host_dns: calloc");
		n->ifname = NULL;
		n->af = res->ai_family;
		if (res->ai_family == AF_INET) {
			memcpy(&n->addr.v.a.addr,
			    &((struct sockaddr_in *)
			    res->ai_addr)->sin_addr.s_addr,
			    sizeof(struct in_addr));
			set_ipmask(n, v4mask);
		} else {
			memcpy(&n->addr.v.a.addr,
			    &((struct sockaddr_in6 *)
			    res->ai_addr)->sin6_addr.s6_addr,
			    sizeof(struct in6_addr));
			n->ifindex =
			    ((struct sockaddr_in6 *)
			    res->ai_addr)->sin6_scope_id;
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

	return (h);
}

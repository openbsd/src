/*	$OpenBSD: pfctl_parser.c,v 1.100 2002/10/14 12:58:28 henning Exp $ */

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

#include "pfctl_parser.h"
#include "pf_print_state.h"

void		 print_op (u_int8_t, const char *, const char *);
void		 print_port (u_int8_t, u_int16_t, u_int16_t, char *);
void		 print_uid (u_int8_t, uid_t, uid_t, const char *);
void		 print_gid (u_int8_t, gid_t, gid_t, const char *);
void		 print_flags (u_int8_t);
void		 print_fromto(struct pf_rule_addr *, struct pf_rule_addr *,
		    u_int8_t, u_int8_t);

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
geticmptypebynumber(u_int8_t type, u_int8_t af)
{
	unsigned int i;

	if (af != AF_INET6) {
		for (i=0; i < (sizeof (icmp_type) / sizeof(icmp_type[0])); i++) {
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
geticmptypebyname(char *w, u_int8_t af)
{
	unsigned int i;

	if (af != AF_INET6) {
		for (i=0; i < (sizeof (icmp_type) / sizeof(icmp_type[0])); i++) {
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
geticmpcodebynumber(u_int8_t type, u_int8_t code, u_int8_t af)
{
	unsigned int i;

	if (af != AF_INET6) {
		for (i=0; i < (sizeof (icmp_code) / sizeof(icmp_code[0])); i++) {
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
geticmpcodebyname(u_long type, char *w, u_int8_t af)
{
	unsigned int i;

	if (af != AF_INET6) {
		for (i=0; i < (sizeof (icmp_code) / sizeof(icmp_code[0])); i++) {
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
	char a1[6], a2[6];
	struct servent *s = getservbyport(p1, proto);

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
	char a1[11], a2[11];

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
	char a1[11], a2[11];

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
	int i;

	for (i = 0; tcpflags[i]; ++i)
		if (f & (1 << i))
			printf("%c", tcpflags[i]);
}

void
print_fromto(struct pf_rule_addr *src, struct pf_rule_addr *dst,
    u_int8_t af, u_int8_t proto)
{
	if (PF_AZERO(&src->addr.addr, AF_INET6) &&
	    PF_AZERO(&src->mask, AF_INET6) &&
	    !src->noroute && !dst->noroute &&
	    !src->port_op && PF_AZERO(&dst->addr.addr, AF_INET6) &&
	    PF_AZERO(&dst->mask, AF_INET6) && !dst->port_op)
		printf("all ");
	else {
		printf("from ");
		if (src->noroute)
			printf("no-route ");
		else if (PF_AZERO(&src->addr.addr, AF_INET6) &&
		    PF_AZERO(&src->mask, AF_INET6))
			printf("any ");
		else {
			if (src->not)
				printf("! ");
			print_addr(&src->addr, &src->mask, af);
			printf(" ");
		}
		if (src->port_op)
			print_port(src->port_op, src->port[0],
			    src->port[1],
			    proto == IPPROTO_TCP ? "tcp" : "udp");

		printf("to ");
		if (dst->noroute)
			printf("no-route ");
		else if (PF_AZERO(&dst->addr.addr, AF_INET6) &&
		    PF_AZERO(&dst->mask, AF_INET6))
			printf("any ");
		else {
			if (dst->not)
				printf("! ");
			print_addr(&dst->addr, &dst->mask, af);
			printf(" ");
		}
		if (dst->port_op)
			print_port(dst->port_op, dst->port[0],
			    dst->port[1],
			    proto == IPPROTO_TCP ? "tcp" : "udp");
	}
}

void
print_nat(struct pf_nat *n)
{
	if (n->no)
		printf("no ");
	printf("nat ");
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
		struct protoent *p = getprotobynumber(n->proto);

		if (p != NULL)
			printf("proto %s ", p->p_name);
		else
			printf("proto %u ", n->proto);
	}
	print_fromto(&n->src, &n->dst, n->af, n->proto);
	if (!n->no) {
		printf("-> ");
		print_addr(&n->raddr, NULL, n->af);
		if (n->proxy_port[0] != PF_NAT_PROXY_PORT_LOW ||
		    n->proxy_port[1] != PF_NAT_PROXY_PORT_HIGH) {
			if (n->proxy_port[0] == n->proxy_port[1])
				printf(" port %u", n->proxy_port[0]);
			else
				printf(" port %u:%u", n->proxy_port[0],
				    n->proxy_port[1]);
		}
	}
	printf("\n");
}

void
print_binat(struct pf_binat *b)
{
	if (b->no)
		printf("no ");
	printf("binat ");
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
		struct protoent *p = getprotobynumber(b->proto);

		if (p != NULL)
			printf("proto %s ", p->p_name);
		else
			printf("proto %u ", b->proto);
	}
	printf("from ");
	print_addr(&b->saddr, &b->smask, b->af);
	printf(" ");
	printf("to ");
	if (!PF_AZERO(&b->daddr.addr, b->af) || !PF_AZERO(&b->dmask, b->af)) {
		if (b->dnot)
			printf("! ");
		print_addr(&b->daddr, &b->dmask, b->af);
		printf(" ");
	} else
		printf("any ");
	if (!b->no) {
		printf("-> ");
		print_addr(&b->raddr, &b->rmask, b->af);
	}
	printf("\n");
}

void
print_rdr(struct pf_rdr *r)
{
	if (r->no)
		printf("no ");
	printf("rdr ");
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
		struct protoent *p = getprotobynumber(r->proto);

		if (p != NULL)
			printf("proto %s ", p->p_name);
		else
			printf("proto %u ", r->proto);
	}
	printf("from ");
	if (!PF_AZERO(&r->saddr.addr, r->af) || !PF_AZERO(&r->smask, r->af)) {
		if (r->snot)
			printf("! ");
		print_addr(&r->saddr, &r->smask, r->af);
		printf(" ");
	} else
		printf("any ");
	printf("to ");
	if (!PF_AZERO(&r->daddr.addr, r->af) || !PF_AZERO(&r->dmask, r->af)) {
		if (r->dnot)
			printf("! ");
		print_addr(&r->daddr, &r->dmask, r->af);
		printf(" ");
	} else
		printf("any ");
	if (r->dport) {
		printf("port %u", ntohs(r->dport));
		if (r->opts & PF_DPORT_RANGE)
			printf(":%u", ntohs(r->dport2));
	}
	if (!r->no) {
		printf(" -> ");
		print_addr(&r->raddr, NULL, r->af);
		printf(" ");
		if (r->rport) {
			printf("port %u", ntohs(r->rport));
			if (r->opts & PF_RPORT_RANGE)
				printf(":*");
		}
	}
	printf("\n");
}

char *pf_reasons[PFRES_MAX+1] = PFRES_NAMES;
char *pf_fcounters[FCNT_MAX+1] = FCNT_NAMES;

void
print_status(struct pf_status *s)
{
	time_t runtime;
	int i;
	char statline[80];

	runtime = time(NULL) - s->since;

	if (s->running) {
		unsigned sec, min, hrs, day = runtime;

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
		    s->bcounters[0][PF_IN], s->bcounters[1][PF_IN]);
		printf("  %-25s %14llu %16llu\n", "Bytes Out",
		    s->bcounters[0][PF_OUT], s->bcounters[1][PF_OUT]);
		printf("  Packets In\n");
		printf("    %-23s %14llu %16llu\n", "Passed",
		    s->pcounters[0][PF_IN][PF_PASS],
		    s->pcounters[1][PF_IN][PF_PASS]);
		printf("    %-23s %14llu %16llu\n", "Blocked",
		    s->pcounters[0][PF_IN][PF_DROP],
		    s->pcounters[1][PF_IN][PF_DROP]);
		printf("  Packets Out\n");
		printf("    %-23s %14llu %16llu\n", "Passed",
		    s->pcounters[0][PF_OUT][PF_PASS],
		    s->pcounters[1][PF_OUT][PF_PASS]);
		printf("    %-23s %14llu %16llu\n\n", "Blocked",
		    s->pcounters[0][PF_OUT][PF_DROP],
		    s->pcounters[1][PF_OUT][PF_DROP]);
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
print_rule(struct pf_rule *r)
{
	int i, opts;

	printf("@%d ", r->nr);
	if (r->action == PF_PASS)
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
			const struct icmpcodeent *ic, *ic6;

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
		}
	} else {
		printf("scrub ");
	}
	if (r->direction == 0)
		printf("in ");
	else
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
			printf("fastroute");
		if (r->af && !PF_AZERO(&r->rt_addr, r->af)) {
			struct pf_addr_wrap aw;

			printf("(%s ", r->rt_ifname);
			aw.addr = r->rt_addr;
			aw.addr_dyn = NULL;
			print_addr(&aw, NULL, r->af);
			printf(")");
		} else if (r->rt_ifname[0])
			printf("%s", r->rt_ifname);
		printf(" ");
	}
	if (r->af) {
		if (r->af == AF_INET)
			printf("inet ");
		else
			printf("inet6 ");
	}
	if (r->proto) {
		struct protoent *p = getprotobynumber(r->proto);

		if (p != NULL)
			printf("proto %s ", p->p_name);
		else
			printf("proto %u ", r->proto);
	}
	print_fromto(&r->src, &r->dst, r->af, r->proto);
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
		const struct icmptypeent *p;

		p = geticmptypebynumber(r->type-1, r->af);
		if (r->af != AF_INET6)
			printf("icmp-type");
		else
			printf("ipv6-icmp-type");
		if (p != NULL)
			printf(" %s ", p->name);
		else
			printf(" %u ", r->type-1);
		if (r->code) {
			const struct icmpcodeent *p;

			p = geticmpcodebynumber(r->type-1, r->code-1, r->af);
			if (p != NULL)
				printf("code %s ", p->name);
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
		printf("label %s", r->label);

	printf("\n");
}

int
parse_flags(char *s)
{
	char *p, *q;
	u_int8_t f = 0;

	for (p = s; *p; p++) {
		if ((q = strchr(tcpflags, *p)) == NULL)
			return -1;
		else
			f |= 1 << (q - tcpflags);
	}
	return (f ? f : PF_TH_ALL);
}

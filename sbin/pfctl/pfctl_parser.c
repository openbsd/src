/*	$OpenBSD: pfctl_parser.c,v 1.43 2001/08/19 17:03:00 frantzen Exp $ */

/*
 * Copyright (c) 2001, Daniel Hartmeier
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
#define TCPSTATES
#include <netinet/tcp_fsm.h>
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

void		 print_addr (u_int32_t);
void		 print_host (struct pf_state_host *);
void		 print_seq (struct pf_state_peer *);
void		 print_port (u_int8_t, u_int16_t, u_int16_t, char *);
void		 print_flags (u_int8_t);

char *tcpflags = "FSRPAU";

struct icmptypeent icmp_type[] = {
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

struct icmpcodeent icmp_code[] = {
	{ "net-unr",		ICMP_UNREACH, 	ICMP_UNREACH_NET },
	{ "host-unr",		ICMP_UNREACH, 	ICMP_UNREACH_HOST },
	{ "proto-unr",		ICMP_UNREACH, 	ICMP_UNREACH_PROTOCOL },
	{ "port-unr",		ICMP_UNREACH, 	ICMP_UNREACH_PORT },
	{ "needfrag",		ICMP_UNREACH, 	ICMP_UNREACH_NEEDFRAG },
	{ "srcfail",		ICMP_UNREACH, 	ICMP_UNREACH_SRCFAIL },
	{ "net-unk",		ICMP_UNREACH, 	ICMP_UNREACH_NET_UNKNOWN },
	{ "host-unk",		ICMP_UNREACH, 	ICMP_UNREACH_HOST_UNKNOWN },
	{ "isolate",		ICMP_UNREACH, 	ICMP_UNREACH_ISOLATED },
	{ "net-prohib",		ICMP_UNREACH, 	ICMP_UNREACH_NET_PROHIB },
	{ "host-prohib",	ICMP_UNREACH, 	ICMP_UNREACH_HOST_PROHIB },
	{ "net-tos",		ICMP_UNREACH, 	ICMP_UNREACH_TOSNET },
	{ "host-tos",		ICMP_UNREACH, 	ICMP_UNREACH_TOSHOST },
	{ "filter-prohib",	ICMP_UNREACH, 	ICMP_UNREACH_FILTER_PROHIB },
	{ "host-preced",	ICMP_UNREACH, 	ICMP_UNREACH_HOST_PRECEDENCE },
	{ "cutoff-preced",	ICMP_UNREACH, 	ICMP_UNREACH_PRECEDENCE_CUTOFF },
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

struct icmptypeent *
geticmptypebynumber(u_int8_t type)
{
	unsigned i;

	for(i=0; i < (sizeof (icmp_type) / sizeof(icmp_type[0])); i++) {
		if(type == icmp_type[i].type)
			return (&icmp_type[i]);
	}
	return (0);
}

struct icmptypeent *
geticmptypebyname(char *w)
{
	unsigned i;

	for(i=0; i < (sizeof (icmp_type) / sizeof(icmp_type[0])); i++) {
		if(!strcmp(w, icmp_type[i].name))
			return (&icmp_type[i]);
	}
	return (0);
}

struct icmpcodeent *
geticmpcodebynumber(u_int8_t type, u_int8_t code)
{
	unsigned i;

	for(i=0; i < (sizeof (icmp_code) / sizeof(icmp_code[0])); i++) {
		if (type == icmp_code[i].type && code == icmp_code[i].code)
			return (&icmp_code[i]);
	}
	return (0);
}

struct icmpcodeent *
geticmpcodebyname(u_long type, char *w)
{
	unsigned i;

	for(i=0; i < (sizeof (icmp_code) / sizeof(icmp_code[0])); i++) {
		if (type == icmp_code[i].type && !strcmp(w, icmp_code[i].name))
			return (&icmp_code[i]);
	}
	return (0);
}

void
print_addr(u_int32_t a)
{
	a = ntohl(a);
	printf("%u.%u.%u.%u", (a>>24)&255, (a>>16)&255, (a>>8)&255, a&255);
}

void
print_host(struct pf_state_host *h)
{
	u_int32_t a = ntohl(h->addr);
	u_int16_t p = ntohs(h->port);

	printf("%u.%u.%u.%u:%u", (a>>24)&255, (a>>16)&255, (a>>8)&255, a&255, p);
}

void
print_seq(struct pf_state_peer *p)
{
	printf("[%u + %u]", p->seqlo, p->seqhi - p->seqlo);
}

void
print_port(u_int8_t op, u_int16_t p1, u_int16_t p2, char *proto)
{
	struct servent *s = getservbyport(p1, proto);

	p1 = ntohs(p1);
	p2 = ntohs(p2);
	printf("port ");
	if (op == PF_OP_IRG)
		printf("%u >< %u ", p1, p2);
	else if (op == PF_OP_XRG)
		printf("%u <> %u ", p1, p2);
	else if (op == PF_OP_EQ) {
		if (s != NULL)
			printf("= %s ", s->s_name);
		else
			printf("= %u ", p1);
	} else if (op == PF_OP_NE) {
		if (s != NULL)
			printf("!= %s ", s->s_name);
		else
			printf("!= %u ", p1);
	} else if (op == PF_OP_LT)
		printf("< %u ", p1);
	else if (op == PF_OP_LE)
		printf("<= %u ", p1);
	else if (op == PF_OP_GT)
		printf("> %u ", p1);
	else if (op == PF_OP_GE)
		printf(">= %u ", p1);
}

void
print_flags(u_int8_t f)
{
	int i;

	for (i = 0; i < 6; ++i)
		if (f & (1 << i))
			printf("%c", tcpflags[i]);
}

void
print_nat(struct pf_nat *n)
{
	printf("@nat ");
	if (n->ifname[0]) {
		printf("on ");
		if (n->ifnot)
			printf("! ");
		printf("%s ", n->ifname);
	}
	printf("from ");
	if (n->saddr || n->smask) {
		if (n->snot)
			printf("! ");
		print_addr(n->saddr);
		if (n->smask != 0xFFFFFFFF) {
			printf("/");
			print_addr(n->smask);
		}
		printf(" ");
	} else
		printf("any ");
	printf("to ");
	if (n->daddr || n->dmask) {
		if (n->dnot)
			printf("! ");
		print_addr(n->daddr);
		if (n->dmask != 0xFFFFFFFF) {
			printf("/");
			print_addr(n->dmask);
		}
		printf(" ");
	} else
		printf("any ");
	printf("-> ");
	print_addr(n->raddr);
	printf(" ");
	switch (n->proto) {
	case IPPROTO_TCP:
		printf("proto tcp");
		break;
	case IPPROTO_UDP:
		printf("proto udp");
		break;
	case IPPROTO_ICMP:
		printf("proto icmp");
		break;
	}
	printf("\n");
}

void
print_rdr(struct pf_rdr *r)
{
	printf("@rdr ");
	if (r->ifname[0]) {
		printf("on ");
		if (r->ifnot)
			printf("! ");
		printf("%s ", r->ifname);
	}
	switch (r->proto) {
	case IPPROTO_TCP:
		printf("proto tcp ");
		break;
	case IPPROTO_UDP:
		printf("proto udp ");
		break;
	}
	printf("from ");
	if (r->saddr || r->smask) {
		if (r->snot)
			printf("! ");
		print_addr(r->saddr);
		if (r->smask != 0xFFFFFFFF) {
			printf("/");
			print_addr(r->smask);
		}
		printf(" ");
	} else
		printf("any ");
	printf("to ");
	if (r->daddr || r->dmask) {
		if (r->dnot)
			printf("! ");
		print_addr(r->daddr);
		if (r->dmask != 0xFFFFFFFF) {
			printf("/");
			print_addr(r->dmask);
		}
		printf(" ");
	} else
		printf("any ");
	printf("port %u", ntohs(r->dport));
	if (r->opts & PF_DPORT_RANGE)
		printf(":%u", ntohs(r->dport2));
	printf(" -> ");
	print_addr(r->raddr);
	printf(" ");
	printf("port %u", ntohs(r->rport));
	if (r->opts & PF_RPORT_RANGE)
		printf(":*");
	printf("\n");
}

char *pf_reasons[PFRES_MAX+1] = PFRES_NAMES;
char *pf_fcounters[FCNT_MAX+1] = FCNT_NAMES;

void
print_status(struct pf_status *s)
{

	time_t t = time(NULL);
	int i;

	printf("Status: %s  Time: %u  Since: %u  Debug: ",
	    s->running ? "Enabled" : "Disabled",
	    t, s->since);
	switch (s->debug) {
		case 0:
			printf("None");
			break;
		case 1:
			printf("Urgent");
			break;
		case 2:
			printf("Misc");
			break;
	}
	printf("\nBytes In: %-10llu  Bytes Out: %-10llu\n",
	    s->bcounters[PF_IN], s->bcounters[PF_OUT]);
	printf("Inbound Packets:  Passed: %-10llu  Dropped: %-10llu\n",
	    s->pcounters[PF_IN][PF_PASS],
	    s->pcounters[PF_IN][PF_DROP]);
	printf("Outbound Packets: Passed: %-10llu  Dropped: %-10llu\n",
	    s->pcounters[PF_OUT][PF_PASS],
	    s->pcounters[PF_OUT][PF_DROP]);
	printf("States: %u\n", s->states);
	printf("pf Counters\n");
	for (i = 0; i < FCNT_MAX; i++)
		printf("%-25s %-8lld\n", pf_fcounters[i],
		    s->fcounters[i]);
	printf("Counters\n");
	for (i = 0; i < PFRES_MAX; i++)
		printf("%-25s %-8lld\n", pf_reasons[i],
		    s->counters[i]);
}

void
print_state(struct pf_state *s)
{
	struct pf_state_peer *src, *dst;
	u_int8_t hrs, min, sec;

	if (s->direction == PF_OUT) {
		src = &s->src;
		dst = &s->dst;
	} else {
		src = &s->dst;
		dst = &s->src;
	}
	switch (s->proto) {
	case IPPROTO_TCP:
		printf("TCP  ");
		break;
	case IPPROTO_UDP:
		printf("UDP  ");
		break;
	case IPPROTO_ICMP:
		printf("ICMP ");
		break;
	default:
		printf("???? ");
		break;
	}
	if ((s->lan.addr != s->gwy.addr) || (s->lan.port != s->gwy.port)) {
		print_host(&s->lan);
		if (s->direction == PF_OUT)
			printf(" -> ");
		else
			printf(" <- ");
	}
	print_host(&s->gwy);
	if (s->direction == PF_OUT)
		printf(" -> ");
	else
		printf(" <- ");
	print_host(&s->ext);
	printf("\n");

	if (s->proto == IPPROTO_TCP) {
		printf("   %s:%s  ", tcpstates[src->state],
			tcpstates[dst->state]);
		print_seq(src);
		printf("    ");
		print_seq(dst);
		printf("\n");
	} else {
		printf("   %u:%u  ", src->state, dst->state);
	}

	sec = s->creation % 60;
	s->creation /= 60;
	min = s->creation % 60;
	s->creation /= 60;
	hrs = s->creation;
	printf("   age %.2u:%.2u:%.2u", hrs, min, sec);
	sec = s->expire % 60;
	s->expire /= 60;
	min = s->expire % 60;
	s->expire /= 60;
	hrs = s->expire;
	printf(", expires in %.2u:%.2u:%.2u", hrs, min, sec);
	printf(", %u pkts, %u bytes\n", s->packets, s->bytes);
}

void
print_rule(struct pf_rule *r)
{
	printf("@%d ", r->nr + 1);
	if (r->action == PF_PASS)
		printf("pass ");
	else if (r->action == PF_DROP) {
		printf("block ");
		if (r->rule_flag & PFRULE_RETURNRST)
			printf("return-rst ");
		else if (r->return_icmp) {
			struct icmpcodeent *ic;

			printf("return-icmp");
			ic = geticmpcodebynumber(r->return_icmp >> 8,
			    r->return_icmp & 255);
			if ((ic == NULL) || (ic->type != ICMP_UNREACH))
				printf("(%u,%u) ", r->return_icmp >> 8,
				    r->return_icmp & 255);
			else if (ic->code != ICMP_UNREACH_PORT)
				printf("(%s) ", ic->name);
			else
				printf(" ");
		}
	} else
		printf("scrub ");
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
	if (r->ifname[0])
		printf("on %s ", r->ifname);
	if (r->proto) {
		struct protoent *p = getprotobynumber(r->proto);
		if (p != NULL)
			printf("proto %s ", p->p_name);
		else
			printf("proto %u ", r->proto);
	}
	if (!r->src.addr && !r->src.mask && !r->src.port_op && !r->dst.addr && ! r->dst.mask && !r->dst.port_op)
		printf("all ");
	else {
		printf("from ");
		if (!r->src.addr && !r->src.mask)
			printf("any ");
		else {
			if (r->src.not)
				printf("! ");
			print_addr(r->src.addr);
			if (r->src.mask != 0xFFFFFFFF) {
				printf("/");
				print_addr(r->src.mask);
			}
			printf(" ");
		}
		if (r->src.port_op)
			print_port(r->src.port_op, r->src.port[0],
			    r->src.port[1],
			    r->proto == IPPROTO_TCP ? "tcp" : "udp");

		printf("to ");
		if (!r->dst.addr && !r->dst.mask)
			printf("any ");
		else {
			if (r->dst.not)
				printf("! ");
			print_addr(r->dst.addr);
			if (r->dst.mask != 0xFFFFFFFF) {
				printf("/");
				print_addr(r->dst.mask);
			}
			printf(" ");
		}
		if (r->dst.port_op)
			print_port(r->dst.port_op, r->dst.port[0],
			    r->dst.port[1],
			    r->proto == IPPROTO_TCP ? "tcp" : "udp");
	}
	if (r->flags || r->flagset) {
		printf("flags ");
		print_flags(r->flags);
		printf("/");
		print_flags(r->flagset);
		printf(" ");
	}
	if (r->type) {
		struct icmptypeent *p;

		p = geticmptypebynumber(r->type-1);
		if (p != NULL)
			printf("icmp-type %s ", p->name);
		else
			printf("icmp-type %u ", r->type-1);
		if (r->code) {
			struct icmpcodeent *p;
			
			p = geticmpcodebynumber(r->type-1, r->code-1);
			if (p != NULL)
				printf("code %s ", p->name);
			else
				printf("code %u ", r->code-1);
		}
	}
	if (r->keep_state)
		printf("keep state ");
	if (r->rule_flag & PFRULE_NODF)
		printf("no-df ");
	if (r->min_ttl)
		printf("min-ttl %d ", r->min_ttl);
	
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
        return (f ? f : 63);
}

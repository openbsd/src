/*	$OpenBSD: pf_print_state.c,v 1.12 2002/11/29 18:24:29 mickey Exp $	*/

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
#include "pf_print_state.h"

void	print_name(struct pf_addr *, struct pf_addr *, sa_family_t);

void
print_addr(struct pf_addr_wrap *addr, sa_family_t af)
{
	char buf[48];

	if (addr->addr_dyn != NULL)
		printf("(%s)", addr->addr.pfa.ifname);
	else {
		if (inet_ntop(af, &addr->addr, buf, sizeof(buf)) == NULL)
			printf("?");
		else
			printf("%s", buf);
	}
	if (! PF_AZERO(&addr->mask, af)) {
		int bits = unmask(&addr->mask, af);

		if (bits != (af == AF_INET ? 32 : 128))
			printf("/%d", bits);
	}
}

void
print_name(struct pf_addr *addr, struct pf_addr *mask, sa_family_t af)
{
	char host[NI_MAXHOST];

	strlcpy(host, "?", sizeof(host));
	switch (af) {
	case AF_INET: {
		struct sockaddr_in sin;

		memset(&sin, 0, sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;
		sin.sin_addr = addr->v4;
		getnameinfo((struct sockaddr *)&sin, sin.sin_len,
		    host, sizeof(host), NULL, 0, NI_NOFQDN);
		break;
	}
	case AF_INET6: {
		struct sockaddr_in6 sin6;

		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_family = AF_INET6;
		sin6.sin6_addr = addr->v6;
		getnameinfo((struct sockaddr *)&sin6, sin6.sin6_len,
		    host, sizeof(host), NULL, 0, NI_NOFQDN);
		break;
	}
	}
	printf("%s", host);
}

void
print_host(struct pf_state_host *h, sa_family_t af, int opts)
{
	u_int16_t p = ntohs(h->port);

	if (opts & PF_OPT_USEDNS)
		print_name(&h->addr, NULL, af);
	else {
		struct pf_addr_wrap aw;

		aw.addr = h->addr;
		memset(&aw.mask, 0xff, sizeof(aw.mask));
		aw.addr_dyn = NULL;
		print_addr(&aw, af);
	}

	if (p) {
		if (af == AF_INET)
			printf(":%u", p);
		else
			printf("[%u]", p);
	}
}

void
print_seq(struct pf_state_peer *p)
{
	if (p->seqdiff)
		printf("[%u + %u](+%u)", p->seqlo, p->seqhi - p->seqlo,
		    p->seqdiff);
	else
		printf("[%u + %u]", p->seqlo, p->seqhi - p->seqlo);
}

void
print_state(struct pf_state *s, int opts)
{
	struct pf_state_peer *src, *dst;
	struct protoent *p;
	int min, sec;

	if (s->direction == PF_OUT) {
		src = &s->src;
		dst = &s->dst;
	} else {
		src = &s->dst;
		dst = &s->src;
	}
	if ((p = getprotobynumber(s->proto)) != NULL)
		printf("%s ", p->p_name);
	else
		printf("%u ", s->proto);
	if (PF_ANEQ(&s->lan.addr, &s->gwy.addr, s->af) ||
	    (s->lan.port != s->gwy.port)) {
		print_host(&s->lan, s->af, opts);
		if (s->direction == PF_OUT)
			printf(" -> ");
		else
			printf(" <- ");
	}
	print_host(&s->gwy, s->af, opts);
	if (s->direction == PF_OUT)
		printf(" -> ");
	else
		printf(" <- ");
	print_host(&s->ext, s->af, opts);

	printf("    ");
	if (s->proto == IPPROTO_TCP) {
		if (src->state <= TCPS_TIME_WAIT &&
		    dst->state <= TCPS_TIME_WAIT) {
			printf("   %s:%s\n", tcpstates[src->state],
			    tcpstates[dst->state]);
		} else {
			printf("   <BAD STATE LEVELS>\n");
		}
		if (opts & PF_OPT_VERBOSE) {
			printf("   ");
			print_seq(src);
			printf("  ");
			print_seq(dst);
			printf("\n");
		}
	} else if (s->proto == IPPROTO_UDP && src->state < PFUDPS_NSTATES &&
	    dst->state < PFUDPS_NSTATES) {
		const char *states[] = PFUDPS_NAMES;
		printf("   %s:%s\n", states[src->state], states[dst->state]);
	} else if (s->proto != IPPROTO_ICMP && src->state < PFOTHERS_NSTATES &&
	    dst->state < PFOTHERS_NSTATES) {
		/* XXX ICMP doesn't really have state levels */
		const char *states[] = PFOTHERS_NAMES;
		printf("   %s:%s\n", states[src->state], states[dst->state]);
	} else {
		printf("   %u:%u\n", src->state, dst->state);
	}

	if (opts & PF_OPT_VERBOSE) {
		sec = s->creation % 60;
		s->creation /= 60;
		min = s->creation % 60;
		s->creation /= 60;
		printf("   age %.2u:%.2u:%.2u", s->creation, min, sec);
		sec = s->expire % 60;
		s->expire /= 60;
		min = s->expire % 60;
		s->expire /= 60;
		printf(", expires in %.2u:%.2u:%.2u", s->expire, min, sec);
		printf(", %u pkts, %u bytes", s->packets, s->bytes);
		if (s->rule.nr != USHRT_MAX)
			printf(", rule %u", s->rule.nr);
		printf("\n");
	}
}

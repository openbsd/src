/*	$OpenBSD: pfctl_parser.c,v 1.7 2001/06/25 10:14:07 deraadt Exp $ */

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
#include <net/pfvar.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <stdarg.h>
#include <errno.h>

#include "pfctl_parser.h"

void		 print_addr (u_int32_t);
void		 print_host (struct state_host *);
void		 print_seq (struct state_peer *);
void		 print_port (u_int8_t, u_int16_t, u_int16_t, char *);
void		 print_flags (u_int8_t);
char		*next_word (char **);
u_int16_t	 next_number (char **);
u_int32_t	 next_addr (char **);
u_int8_t	 next_flags (char **);
u_int16_t	 rule_port (char *, u_int8_t);
u_int32_t	 rule_mask (u_int8_t);

char *tcpflags = "FSRPAU";

int
error(int n, char *fmt, ...)
{
	extern char *__progname;
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s: line %d", __progname, n);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	return (0);
}

void
print_addr(u_int32_t a)
{
	a = ntohl(a);
	printf("%u.%u.%u.%u", (a>>24)&255, (a>>16)&255, (a>>8)&255, a&255);
}

void
print_host(struct state_host *h)
{
	u_int32_t a = ntohl(h->addr);
	u_int16_t p = ntohs(h->port);

	printf("%u.%u.%u.%u:%u", (a>>24)&255, (a>>16)&255, (a>>8)&255, a&255, p);
}

void
print_seq(struct state_peer *p)
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
	if (op == 1)
		printf("%u >< %u ", p1, p2);
	else if (op == 2) {
		if (s != NULL)
			printf("= %s ", s->s_name);
		else
			printf("= %u ", p1);
	} else if (op == 3) {
		if (s != NULL)
			printf("!= %s ", s->s_name);
		else
			printf("!= %u ", p1);
	} else if (op == 4)
		printf("< %u ", p1);
	else if (op == 5)
		printf("<= %u ", p1);
	else if (op == 6)
		printf("> %u ", p1);
	else
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
	printf("nat %s ", n->ifname);
	if (n->not)
		printf("! ");
	print_addr(n->saddr);
	if (n->smask != 0xFFFFFFFF) {
		printf("/");
		print_addr(n->smask);
	}
	printf(" -> ");
	print_addr(n->daddr);
	switch (n->proto) {
	case IPPROTO_TCP:
		printf(" proto tcp");
		break;
	case IPPROTO_UDP:
		printf(" proto udp");
		break;
	case IPPROTO_ICMP:
		printf(" proto icmp");
		break;
	}
	printf("\n");
}

void
print_rdr(struct pf_rdr *r)
{
	printf("rdr %s ", r->ifname);
	if (r->not)
		printf("! ");
	print_addr(r->daddr);
	if (r->dmask != 0xFFFFFFFF) {
		printf("/");
		print_addr(r->dmask);
	}
	printf(" port %u -> ", ntohs(r->dport));
	print_addr(r->raddr);
	printf(" port %u", ntohs(r->rport));
	switch (r->proto) {
	case IPPROTO_TCP:
		printf(" proto tcp");
		break;
	case IPPROTO_UDP:
		printf(" proto udp");
		break;
	}
	printf("\n");
}

void
print_status(struct pf_status *s)
{
	time_t t = time(NULL);

	printf("%u %u %u", t, s->since, s->running);
	if (s->running) {
		printf(" %u %u", s->bytes[0], s->bytes[1]);
		printf(" %u %u", s->packets[0][0], s->packets[0][1]);
		printf(" %u %u", s->packets[1][0], s->packets[1][1]);
		printf(" %u %u %u %u", s->states, s->state_inserts,
		    s->state_removals, s->state_searches);
	}
	printf("\n");
}

void
print_state(struct pf_state *s)
{
	struct state_peer *src, *dst;
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

	printf("%u:%u  ", src->state, dst->state);
	if (s->proto == IPPROTO_TCP) {
		print_seq(src);
		printf("    ");
		print_seq(dst);
		printf("\n     ");
	}

	sec = s->creation % 60;
	s->creation /= 60;
	min = s->creation % 60;
	s->creation /= 60;
	hrs = s->creation;
	printf("age %.2u:%.2u:%.2u", hrs, min, sec);
	sec = s->expire % 60;
	s->expire /= 60;
	min = s->expire % 60;
	s->expire /= 60;
	hrs = s->expire;
	printf(", expires in %.2u:%.2u:%.2u", hrs, min, sec);
	printf(", %u pkts, %u bytes\n", s->packets, s->bytes);
	printf("\n");
}

void
print_rule(struct pf_rule *r)
{
	if (r->action == 0)
		printf("pass ");
	else
		printf("block ");
	if (r->action == 2)
		printf("return-rst ");
	if (r->direction == 0)
		printf("in ");
	else
		printf("out ");
	if (r->log)
		printf("log ");
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
	if (!r->src.addr && !r->src.port_op && !r->dst.addr && !r->dst.port_op)
		printf("all ");
	else {
		printf("from ");
		if (!r->src.addr)
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
		if (!r->dst.addr)
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
	if (r->type)
		printf("icmp-type %u ", r->type-1);
	if (r->code)
		printf("code %u ", r->code-1);
	if (r->keep_state)
		printf("keep state ");
	printf("\n");
}

char *
next_line(char **s)
{
	char *l = *s;

	while (**s && (**s != '\n'))
		(*s)++;
	if (**s) {
		**s = 0;
		(*s)++;
	}
	return (l);
}

char *
next_word(char **s)
{
	char *w;

	while ((**s == ' ') || (**s == '\t') || (**s == '\n'))
		(*s)++;
	w = *s;
	while (**s && (**s != ' ') && (**s != '\t') && (**s != '\n'))
		(*s)++;
	if (**s) {
		**s = 0;
		(*s)++;
	}
	return (w);
}

u_int16_t
next_number(char **s)
{
	u_int16_t n = 0;

	while (**s && !isdigit(**s))
		(*s)++;
	while (**s && isdigit(**s)) {
		n *= 10;
		n += **s - '0';
		(*s)++;
	}
	return (n);
}

u_int32_t
next_addr(char **w)
{
	u_int8_t a, b, c, d;
	a = next_number(w);
	b = next_number(w);
	c = next_number(w);
	d = next_number(w);
	return (htonl((a << 24) | (b << 16) | (c << 8) | d));
}

u_int8_t
next_flags(char **s)
{
	u_int8_t f = 0;
	char *p;

	while (**s && !strchr(tcpflags, **s))
		(*s)++;
	while (**s && ((p = strchr(tcpflags, **s)) != NULL)) {
		f |= 1 << (p-tcpflags);
		(*s)++;
	}
	return (f ? f : 63);
}

u_int16_t
rule_port(char *w, u_int8_t p)
{
	struct servent *s;
	u_long ul;
	char *ep;

	errno = 0;
	ul = strtoul(w, &ep, 10);
	if (*w == '\0' || *ep != '\0') {
		s = getservbyname(w, p == IPPROTO_TCP ? "tcp" : "udp");
		if (s == NULL)
			return (0);
		return (s->s_port);
	}
	if (errno == ERANGE && ul == ULONG_MAX)
		return (0);
	if (ul > USHRT_MAX) {
		errno = ERANGE;
		return (0);
	}
	return (htons(ul));
}

u_int32_t
rule_mask(u_int8_t b)
{
	u_int32_t m = 0;
	int i;

	for (i = 31; i > 31-b; --i)
		m |= (1 << i);
	return (htonl(m));
}

int
parse_rule(int n, char *l, struct pf_rule *r)
{
	char *w;
	memset(r, 0, sizeof(struct pf_rule));
	w = next_word(&l);

	/* pass / block */
	if (!strcmp(w, "pass" ))
		r->action = 0;
	else if (!strcmp(w, "block"))
		r->action = 1;
	else {
		error(n, "expected pass/block, got %s\n", w);
		return (0);
	}
	w = next_word(&l);

	/* return-rst */
	if ((r->action == 1) && !strcmp(w, "return-rst")) {
		r->action = 2;
		w = next_word(&l);
	}

	/* in / out */
	if (!strcmp(w, "in" ))
		r->direction = 0;
	else if (!strcmp(w, "out"))
		r->direction = 1;
	else {
		error(n, "expected in/out, got %s\n", w);
		return (0);
	}
	w = next_word(&l);

	/* log */
	if (!strcmp(w, "log")) {
		r->log = 1;
		w = next_word(&l);
	}

	/* quick */
	if (!strcmp(w, "quick")) {
		r->quick = 1;
		w = next_word(&l);
	}

	/* on <if> */
	if (!strcmp(w, "on")) {
		w = next_word(&l);
		strncpy(r->ifname, w, 16);
		w = next_word(&l);
	}

	/* proto tcp/udp/icmp */
	if (!strcmp(w, "proto")) {
		struct protoent *p;
		w = next_word(&l);
		p = getprotobyname(w);
		if (p == NULL) {
			error(n, "unknown protocol %s\n", w);
			return (0);
		}
		r->proto = p->p_proto;
		w = next_word(&l);
	}

	/* all / from src to dst */
	if (!strcmp(w, "all" ))
		w = next_word(&l);
	else if (!strcmp(w, "from")) {
		w = next_word(&l);

		/* source address */
		if (!strcmp(w, "any"))
			w = next_word(&l);
		else {
			if (!strcmp(w, "!")) {
				r->src.not = 1;
				w = next_word(&l);
			}
			r->src.addr = next_addr(&w);
			if (!*w)
				r->src.mask = 0xFFFFFFFF;
			else if (*w == '/')
				r->src.mask = rule_mask(next_number(&w));
			else {
				error(n, "expected /, got '%c'\n", *w);
				return (0);
			}
			w = next_word(&l);
		}

		/* source port */
		if (((r->proto == IPPROTO_TCP) || (r->proto == IPPROTO_UDP)) &&
		    !strcmp(w, "port")) {
			w = next_word(&l);
			if (!strcmp(w, "=" ))
				r->src.port_op = 2;
			else if (!strcmp(w, "!="))
				r->src.port_op = 3;
			else if (!strcmp(w, "<" ))
				r->src.port_op = 4;
			else if (!strcmp(w, "<="))
				r->src.port_op = 5;
			else if (!strcmp(w, ">" ))
				r->src.port_op = 6;
			else if (!strcmp(w, ">="))
				r->src.port_op = 7;
			else
				r->src.port_op = 1;
			if (r->src.port_op != 1)
				w = next_word(&l);
			r->src.port[0] = rule_port(w, r->proto);
			w = next_word(&l);
			if (r->src.port_op == 1) {
				if (strcmp(w, "<>") && strcmp(w, "><")) {
					error(n, "expected <>/><, got %s\n",
					    w);
					return (0);
				}
				w = next_word(&l);
				r->src.port[1] = rule_port(w, r->proto);
				w = next_word(&l);
			}
		}

		/* destination address */
		if (strcmp(w, "to")) {
			error(n, "expected to, got %s\n", w);
			return (0);
		}
		w = next_word(&l);
		if (!strcmp(w, "any"))
			w = next_word(&l);
		else {
			if (!strcmp(w, "!")) {
				r->dst.not = 1;
				w = next_word(&l);
			}
			r->dst.addr = next_addr(&w);
			if (!*w)
				r->dst.mask = 0xFFFFFFFF;
			else if (*w == '/')
				r->dst.mask = rule_mask(next_number(&w));
			else {
				error(n, "expected /, got '%c'\n", *w);
				return (0);
			}
			w = next_word(&l);
		}

		/* destination port */
		if (((r->proto == IPPROTO_TCP) || (r->proto == IPPROTO_UDP)) &&
		    !strcmp(w, "port")) {
			w = next_word(&l);
			if (!strcmp(w, "=" ))
				r->dst.port_op = 2;
			else if (!strcmp(w, "!="))
				r->dst.port_op = 3;
			else if (!strcmp(w, "<" ))
				r->dst.port_op = 4;
			else if (!strcmp(w, "<="))
				r->dst.port_op = 5;
			else if (!strcmp(w, ">" ))
				r->dst.port_op = 6;
			else if (!strcmp(w, ">="))
				r->dst.port_op = 7;
			else
				r->dst.port_op = 1;
			if (r->dst.port_op != 1)
				w = next_word(&l);
			r->dst.port[0] = rule_port(w, r->proto);
			w = next_word(&l);
			if (r->dst.port_op == 1) {
				if (strcmp(w, "<>") && strcmp(w, "><")) {
					error(n, "expected <>/><, got %s\n",
					    w);
					return (0);
				}
				w = next_word(&l);
				r->dst.port[1] = rule_port(w, r->proto);
				w = next_word(&l);
			}
		}

	} else {
		error(n, "expected all/from, got %s\n", w);
		return (0);
	}

	/* flags */
	if (!strcmp(w, "flags")) {
		if (r->proto != IPPROTO_TCP) {
			error(n, "flags only valid for proto tcp\n");
			return (0);
		} else {
			w = next_word(&l);
			r->flags = next_flags(&w);
			r->flagset = next_flags(&w);
			w = next_word(&l);
		}
	}

	/* icmp type/code */
	if (!strcmp(w, "icmp-type")) {
		if (r->proto != IPPROTO_ICMP) {
			error(n, "icmp-type only valid for proto icmp\n");
			return (0);
		} else {
			u_long ul;
			char *ep;

			w = next_word(&l);

			errno = 0;
			ul = strtoul(w, &ep, 10);
			if (w[0] == '\0' || *ep != '\0' ||
			    (errno == ERANGE && ul == ULONG_MAX) ||
			    ul > ICMP_MAXTYPE) {
				error(n, "icmp-type type wrong\n");
				return (0);
			}
			r->type = ul+1;

			w = next_word(&l);
			if (!strcmp(w, "code")) {
				w = next_word(&l);

				errno = 0;
				ul = strtoul(w, &ep, 10);
				if ((w[0] == '\0' || *ep != '\0') ||
				    (errno == ERANGE && ul == ULONG_MAX) ||
				    ul > 255) {
					error(n, "icmp-type code wrong\n");
					return (0);
				}

				r->code = ul + 1;
				w = next_word(&l);
			}
		}
	}

	/* keep */
	if (!strcmp(w, "keep")) {
		w = next_word(&l);
		if (!strcmp(w, "state")) {
			w = next_word(&l);
			r->keep_state = 1;
		} else {
			error(n, "expected state, got %s\n", w);
			return (0);
		}
	}

	/* no further options expected */
	while (*w) {
		error(n, "unexpected %s\n", w);
		w = next_word(&l);
	}

	return (1);
}

int
parse_nat(int n, char *l, struct pf_nat *nat)
{
	char *w;

	memset(nat, 0, sizeof(struct pf_nat));
	w = next_word(&l);

	/* nat */
	if (strcmp(w, "nat" )) {
		error(n, "expected nat, got %s\n", w);
		return (0);
	}
	w = next_word(&l);

	/* if */
	strncpy(nat->ifname, w, 16);
	w = next_word(&l);

	/* internal addr/mask */
	if (!strcmp(w, "!")) {
		nat->not = 1;
		w = next_word(&l);
	}
	nat->saddr = next_addr(&w);
	if (!*w)
		nat->smask = 0xFFFFFFFF;
	else if (*w == '/')
		nat->smask = rule_mask(next_number(&w));
	else {
		error(n, "expected /, got '%c'\n", *w);
		return (0);
	}
	w = next_word(&l);

	/* -> */
	if (strcmp(w, "->")) {
		error(n, "expected ->, got %s\n", w);
		return (0);
	}
	w = next_word(&l);

	/* external addr */
	nat->daddr = next_addr(&w);
	w = next_word(&l);

	/* proto */
	if (!strcmp(w, "proto")) {
		w = next_word(&l);
		if (!strcmp(w, "tcp"))
			nat->proto = IPPROTO_TCP;
		else if (!strcmp(w, "udp"))
			nat->proto = IPPROTO_UDP;
		else if (!strcmp(w, "icmp"))
			nat->proto = IPPROTO_ICMP;
		else {
			error(n, "expected tcp/udp/icmp, got %s\n", w);
			return (0);
		}
		w = next_word(&l);
	}

	/* no further options expected */
	while (*w) {
		error(n, "unexpected %s\n", w);
		w = next_word(&l);
	}

	return (1);
}

int
parse_rdr(int n, char *l, struct pf_rdr *rdr)
{
	char *w;

	memset(rdr, 0, sizeof(struct pf_rdr));
	w = next_word(&l);

	/* rdr */
	if (strcmp(w, "rdr" )) {
		error(n, "expected rdr, got %s\n", w);
		return (0);
	}
	w = next_word(&l);

	/* if */
	strncpy(rdr->ifname, w, 16);
	w = next_word(&l);

	/* external addr/mask */
	if (!strcmp(w, "!")) {
		rdr->not = 1;
		w = next_word(&l);
	}
	rdr->daddr = next_addr(&w);
	if (!*w)
		rdr->dmask = 0xFFFFFFFF;
	else if (*w == '/')
		rdr->dmask = rule_mask(next_number(&w));
	else {
		error(n, "expected /, got '%c'\n", *w);
		return (0);
	}
	w = next_word(&l);

	/* external port */
	if (strcmp(w, "port")) {
		error(n, "expected port, got %s\n", w);
		return (0);
	}
	w = next_word(&l);
	rdr->dport = htons(next_number(&w));
	w = next_word(&l);

	/* -> */
	if (strcmp(w, "->")) {
		error(n, "expected ->, got %s\n", w);
		return (0);
	}
	w = next_word(&l);

	/* internal addr */
	rdr->raddr = next_addr(&w);
	w = next_word(&l);

	/* internal port */
	if (strcmp(w, "port")) {
		error(n, "expected port, got %s\n", w);
		return (0);
	}
	w = next_word(&l);
	rdr->rport = htons(next_number(&w));
	w = next_word(&l);

	/* proto */
	if (!strcmp(w, "proto")) {
		w = next_word(&l);
		if (!strcmp(w, "tcp"))
			rdr->proto = IPPROTO_TCP;
		else if (!strcmp(w, "udp"))
			rdr->proto = IPPROTO_UDP;
		else {
			error(n, "expected tcp/udp, got %s\n", w);
			return (0);
		}
		w = next_word(&l);
	}

	/* no further options expected */
	while (*w) {
		error(n, "unexpected %s\n", w);
		w = next_word(&l);
	}

	return (1);
}

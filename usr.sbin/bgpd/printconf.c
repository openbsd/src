/*	$OpenBSD: printconf.c,v 1.5 2004/02/24 15:43:03 claudio Exp $	*/

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA, PROFITS OR MIND, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>

#include "bgpd.h"
#include "session.h"

void		 print_op(enum comp_ops);
void		 print_mainconf(struct bgpd_config *);
void		 print_network(struct network_config *);
void		 print_peer(struct peer_config *);
void		 print_rule(struct peer *, struct filter_rule *);

void
print_op(enum comp_ops op)
{
	switch (op) {
	case OP_RANGE:
		printf("-");
		break;
	case OP_XRANGE:
		printf("><");
		break;
	case OP_EQ:
		printf("=");
		break;
	case OP_NE:
		printf("!=");
		break;
	case OP_LE:
		printf("<=");
		break;
	case OP_LT:
		printf("<");
		break;
	case OP_GE:
		printf(">=");
		break;
	case OP_GT:
		printf(">");
		break;
	default:
		printf("?");
		break;
	}
}

void
print_mainconf(struct bgpd_config *conf)
{
	struct in_addr	ina;

	printf("AS %u\n", conf->as);
	ina.s_addr = conf->bgpid;
	printf("router-id %s\n", inet_ntoa(ina));
	if (conf->holdtime)
		printf("holdtime %u\n", conf->holdtime);
	if (conf->min_holdtime)
		printf("holdtime min %u\n", conf->min_holdtime);

	if (conf->flags & BGPD_FLAG_NO_FIB_UPDATE)
		printf("fib-update no\n");
	else
		printf("fib-update yes\n");

	if (conf->log & BGPD_LOG_UPDATES)
		printf("log updates\n");

	if (conf->listen_addr.sin_addr.s_addr != INADDR_ANY)
		printf("listen-on %s\n", inet_ntoa(conf->listen_addr.sin_addr));
}

void
print_network(struct network_config *n)
{
	printf("network %s/%u\n", log_addr(&n->prefix), n->prefixlen);
}

void
print_peer(struct peer_config *p)
{
	const char	*tab	= "\t";
	const char	*nada	= "";
	const char	*c;

	if (p->group[0]) {
		printf("group \"%s\" {\n", p->group);
		c = tab;
	} else
		c = nada;

	printf("%sneighbor %s {\n", c, log_addr(&p->remote_addr));
	printf("%s\tdescr \"%s\"\n", c, p->descr);
	printf("%s\tremote-as %u\n", c, p->remote_as);
	if (p->distance > 1)
		printf("%s\tmultihop %u\n", c, p->distance);
	if (p->passive)
		printf("%s\tpassive\n", c);
	if (p->local_addr.af)
		printf("%s\tlocal-address %s\n", c, log_addr(&p->local_addr));
	if (p->max_prefix)
		printf("%s\tmax-prefix %u\n", c, p->max_prefix);
	if (p->holdtime)
		printf("%s\tholdtime %u\n", c, p->holdtime);
	if (p->min_holdtime)
		printf("%s\tholdtime min %u\n", c, p->min_holdtime);
	if (p->announce_type == ANNOUNCE_SELF)
		printf("%s\tannounce self\n", c);
	else if (p->announce_type == ANNOUNCE_NONE)
		printf("%s\tannounce none\n", c);
	else if (p->announce_type == ANNOUNCE_ALL)
		printf("%s\tannounce all\n", c);
	else
		printf("%s\tannounce ???\n", c);
	if (p->tcp_md5_key[0])
		printf("%s\ttcp md5sig\n", c);
	if (p->attrset.flags) {
		printf("%s\tset {\n", c);
		if (p->attrset.flags & SET_LOCALPREF)
			printf("%s\t\tlocalpref %u\n", c, p->attrset.localpref);
		if (p->attrset.flags & SET_MED)
			printf("%s\t\tmed %u\n", c, p->attrset.med);
		if (p->attrset.flags & SET_NEXTHOP)
			printf("%s\t\tnexthop %s\n",
			    c, inet_ntoa(p->attrset.nexthop));
		if (p->attrset.flags & SET_PREPEND)
			printf("%s\t\tprepend-self %u\n",
			    c, p->attrset.prepend);
		printf("%s\t}\n", c);
	}
	printf("%s}\n", c);
	if (p->group[0])
		printf("}\n");
}

void
print_rule(struct peer *peer_l, struct filter_rule *r)
{
	struct peer	*p;

	if (r->action == ACTION_ALLOW)
		printf("allow ");
	else if (r->action == ACTION_DENY)
		printf("deny ");
	else
		printf("match ");

	if (r->quick)
		printf("quick ");

	if (r->dir == DIR_IN)
		printf("from ");
	else if (r->dir == DIR_OUT)
		printf("to ");
	else
		printf("eeeeeeeps. ");

	if (r->peer.peerid) {
		for (p = peer_l; p != NULL && p->conf.id != r->peer.peerid;
		    p = p->next)
			;	/* nothing */
		if (p == NULL)
			printf("?");
		else
			printf("%s ", log_addr(&p->conf.remote_addr));
	} else if (r->peer.groupid) {
		for (p = peer_l; p != NULL &&
		    p->conf.groupid != r->peer.groupid; p = p->next)
			;	/* nothing */
		if (p == NULL)
			printf("group ? ");
		else
			printf("group %s ", p->conf.group);
	} else
		printf("any ");

	if (r->match.prefix.addr.af)
		printf("prefix %s/%u ", log_addr(&r->match.prefix.addr),
		    r->match.prefix.len);

	if (r->match.prefixlen.op) {
		if (r->match.prefixlen.op == OP_RANGE ||
		    r->match.prefixlen.op == OP_XRANGE) {
			printf("prefixlen %u ", r->match.prefixlen.len_min);
			print_op(r->match.prefixlen.op);
			printf(" %u ", r->match.prefixlen.len_max);
		} else {
			printf("prefixlen ");
			print_op(r->match.prefixlen.op);
			printf(" %u ", r->match.prefixlen.len_min);
		}
	}

	if (r->match.as.type) {
		if (r->match.as.type == AS_ALL)
			printf("AS %u ", r->match.as.as);
		else if (r->match.as.type == AS_SOURCE)
			printf("source-AS %u ", r->match.as.as);
		else if (r->match.as.type == AS_TRANSIT)
			printf("transit-AS %u ", r->match.as.as);
		else
			printf("unfluffy-AS %u ", r->match.as.as);
	}

	if (r->set.flags) {
		printf("set { ");
		if (r->set.flags & SET_LOCALPREF)
			printf("localpref %u ", r->set.localpref);
		if (r->set.flags & SET_MED)
			printf("med %u ", r->set.med);
		if (r->set.flags & SET_NEXTHOP)
			printf("nexthop %s ", inet_ntoa(r->set.nexthop));
		if (r->set.flags & SET_PREPEND)
			printf("prepend-self %u ", r->set.prepend);


		printf("}");
	}

	printf("\n");
}

void
print_config(struct bgpd_config *conf, struct network_head *net_l,
    struct peer *peer_l, struct filter_head *rules_l)
{
	struct peer		*p;
	struct filter_rule	*r;
	struct network		*n;

	print_mainconf(conf);
	printf("\n");
	TAILQ_FOREACH(n, net_l, network_l)
		print_network(&n->net);
	printf("\n");
	for (p = peer_l; p != NULL; p = p->next)
		print_peer(&p->conf);
	printf("\n");
	TAILQ_FOREACH(r, rules_l, entries)
		print_rule(peer_l, r);
}

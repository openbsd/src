/*	$OpenBSD: printconf.c,v 1.18 2004/05/08 17:40:53 henning Exp $	*/

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
#include "mrt.h"
#include "session.h"

void		 print_op(enum comp_ops);
void		 print_set(struct filter_set *);
void		 print_mainconf(struct bgpd_config *);
void		 print_network(struct network_config *);
void		 print_peer(struct peer_config *);
const char	*print_auth_alg(u_int8_t);
const char	*print_enc_alg(u_int8_t);
void		 print_rule(struct peer *, struct filter_rule *);
const char *	 mrt_type(enum mrt_type);
void		 print_mrt(u_int32_t, u_int32_t, const char *);

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
print_set(struct filter_set *set)
{
	if (set->flags) {
		printf("set { ");
		if (set->flags & SET_LOCALPREF)
			printf("localpref %u ", set->localpref);
		if (set->flags & SET_MED)
			printf("med %u ", set->med);
		if (set->flags & SET_NEXTHOP)
			printf("nexthop %s ", inet_ntoa(set->nexthop));
		if (set->flags & SET_PREPEND)
			printf("prepend-self %u ", set->prepend);
		printf("}");
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

	if (conf->flags & BGPD_FLAG_NO_EVALUATE)
		printf("route-collector yes\n");

	if (conf->log & BGPD_LOG_UPDATES)
		printf("log updates\n");

	if (conf->listen_addr.sin_addr.s_addr != INADDR_ANY)
		printf("listen-on %s\n", inet_ntoa(conf->listen_addr.sin_addr));
}

void
print_network(struct network_config *n)
{
	printf("network %s/%u", log_addr(&n->prefix), n->prefixlen);
	if (n->attrset.flags)
		printf(" ");
	print_set(&n->attrset);
	printf("\n");
}

void
print_peer(struct peer_config *p)
{
	const char	*tab	= "\t";
	const char	*nada	= "";
	const char	*c;
	char		*method;

	if (p->group[0]) {
		printf("group \"%s\" {\n", p->group);
		c = tab;
	} else
		c = nada;

	if ((p->remote_addr.af == AF_INET && p->remote_masklen != 32) ||
	    (p->remote_addr.af == AF_INET6 && p->remote_masklen != 128))
		printf("%sneighbor %s/%u {\n", c, log_addr(&p->remote_addr),
		    p->remote_masklen);
	else
		printf("%sneighbor %s {\n", c, log_addr(&p->remote_addr));
	if (p->descr[0])
		printf("%s\tdescr \"%s\"\n", c, p->descr);
	if (p->remote_as)
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
	if (p->capabilities == 0)
		printf("%s\tannounce capabilities no\n", c);
	if (p->announce_type == ANNOUNCE_SELF)
		printf("%s\tannounce self\n", c);
	else if (p->announce_type == ANNOUNCE_NONE)
		printf("%s\tannounce none\n", c);
	else if (p->announce_type == ANNOUNCE_ALL)
		printf("%s\tannounce all\n", c);
	else
		printf("%s\tannounce ???\n", c);

	if (p->auth.method == AUTH_MD5SIG)
		printf("%s\ttcp md5sig\n", c);
	else if (p->auth.method == AUTH_IPSEC_MANUAL_ESP || p->auth.method == AUTH_IPSEC_MANUAL_AH) {
		if (p->auth.method == AUTH_IPSEC_MANUAL_ESP)
			method = "esp";
		else
			method = "ah";

		printf("%s\tipsec %s in spi %u %s XXXXXX", c, method, p->auth.spi_in,
		    print_auth_alg(p->auth.auth_alg_in));
		if (p->auth.enc_alg_in)
			printf(" %s XXXXXX", print_enc_alg(p->auth.enc_alg_in));
		printf("\n");

		printf("%s\tipsec %s out spi %u %s XXXXXX", c, method, p->auth.spi_out,
		    print_auth_alg(p->auth.auth_alg_out));
		if (p->auth.enc_alg_out)
			printf(" %s XXXXXX",
			    print_enc_alg(p->auth.enc_alg_out));
		printf("\n");
	} else if (p->auth.method == AUTH_IPSEC_IKE_AH)
		printf("%s\tipsec ah ike\n", c);
	else if (p->auth.method == AUTH_IPSEC_IKE_ESP)
		printf("%s\tipsec esp ike\n", c);

	if (p->attrset.flags)
		printf("%s\t", c);
	print_set(&p->attrset);
	if (p->attrset.flags)
		printf("\n");

	print_mrt(p->id, p->groupid, c == nada ? "\t" : "\t\t");

	printf("%s}\n", c);
	if (p->group[0])
		printf("}\n");
}

const char *
print_auth_alg(u_int8_t alg)
{
	switch (alg) {
	case SADB_AALG_SHA1HMAC:
		return ("sha1");
	case SADB_AALG_MD5HMAC:
		return ("md5");
	default:
		return ("???");
	}
}

const char *
print_enc_alg(u_int8_t alg)
{
	switch (alg) {
	case SADB_EALG_3DESCBC:
		return ("3des");
	case SADB_X_EALG_AES:
		return ("aes");
	default:
		return ("???");
	}
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

	if (r->match.community.as != 0) {
		if (r->match.community.as == COMMUNITY_ANY)
			printf("*:");
		else
			printf("%d:", r->match.community.as);

		if (r->match.community.type == COMMUNITY_ANY)
			printf("* ");
		else
			printf("%d ", r->match.community.type);
	}

	print_set(&r->set);

	printf("\n");
}

const char *
mrt_type(enum mrt_type t)
{
	switch (t) {
	case MRT_NONE:
		return "unfluffy MRT";
	case MRT_TABLE_DUMP:
		return "table";
	case MRT_ALL_IN:
		return "all in";
	case MRT_ALL_OUT:
		return "all out";
	case MRT_UPDATE_IN:
		return "updates in";
	case MRT_UPDATE_OUT:
		return "updates out";
	}
	return "unfluffy MRT";
}

struct mrt_head	*xmrt_l = NULL;

void
print_mrt(u_int32_t pid, u_int32_t gid, const char *prep)
{
	struct mrt	*m;

	if (xmrt_l == NULL)
		return;

	LIST_FOREACH(m, xmrt_l, list)
		if ((gid != 0 && m->conf.group_id == gid) ||
		    (m->conf.peer_id == pid && m->conf.group_id == gid)) {
			if (m->ReopenTimerInterval == 0)
				printf("%sdump %s %s\n", prep,
				    mrt_type(m->conf.type), m->name);
			else
				printf("%sdump %s %s %d\n", prep,
				    mrt_type(m->conf.type),
				    m->name, m->ReopenTimerInterval);
		}
}

void
print_config(struct bgpd_config *conf, struct network_head *net_l,
    struct peer *peer_l, struct filter_head *rules_l, struct mrt_head *mrt_l)
{
	struct peer		*p;
	struct filter_rule	*r;
	struct network		*n;

	xmrt_l = mrt_l;
	print_mainconf(conf);
	printf("\n");
	print_mrt(0, 0, "");
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

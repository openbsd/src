/*	$OpenBSD: printconf.c,v 1.34 2004/11/18 17:07:38 henning Exp $	*/

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
#include <stdlib.h>
#include <string.h>

#include "bgpd.h"
#include "mrt.h"
#include "session.h"

void		 print_op(enum comp_ops);
void		 print_set(struct filter_set *);
void		 print_mainconf(struct bgpd_config *);
void		 print_network(struct network_config *);
void		 print_peer(struct peer_config *, struct bgpd_config *,
		    const char *);
const char	*print_auth_alg(u_int8_t);
const char	*print_enc_alg(u_int8_t);
void		 print_rule(struct peer *, struct filter_rule *);
const char *	 mrt_type(enum mrt_type);
void		 print_mrt(u_int32_t, u_int32_t, const char *, const char *);
void		 print_groups(struct bgpd_config *, struct peer *);
int		 peer_compare(const void *, const void *);

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
			printf("nexthop %s ", log_addr(&set->nexthop));
		if (set->flags & SET_NEXTHOP_REJECT)
			printf("nexthop reject ");
		if (set->flags & SET_NEXTHOP_BLACKHOLE)
			printf("nexthop blackhole ");
		if (set->flags & SET_PREPEND_SELF)
			printf("prepend-self %u ", set->prepend_self);
		if (set->flags & SET_PREPEND_PEER)
			printf("prepend-neighbor %u ", set->prepend_peer);
		printf("}");
	}
}

void
print_mainconf(struct bgpd_config *conf)
{
	struct in_addr		 ina;
	struct listen_addr	*la;

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

	if (conf->flags & BGPD_FLAG_DECISION_ROUTEAGE)
		printf("rde route-age evaluate\n");

	if (conf->log & BGPD_LOG_UPDATES)
		printf("log updates\n");

	TAILQ_FOREACH(la, conf->listen_addrs, entry)
		printf("listen on %s\n",
		    log_sockaddr((struct sockaddr *)&la->sa));
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
print_peer(struct peer_config *p, struct bgpd_config *conf, const char *c)
{
	char		*method;
	struct in_addr	 ina;

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
	if (p->announce_capa == 0)
		printf("%s\tannounce capabilities no\n", c);
	if (p->announce_type == ANNOUNCE_SELF)
		printf("%s\tannounce self\n", c);
	else if (p->announce_type == ANNOUNCE_NONE)
		printf("%s\tannounce none\n", c);
	else if (p->announce_type == ANNOUNCE_ALL)
		printf("%s\tannounce all\n", c);
	else if (p->announce_type == ANNOUNCE_DEFAULT_ROUTE)
		printf("%s\tannounce default-route\n", c);
	else
		printf("%s\tannounce ???\n", c);
	if (p->enforce_as == ENFORCE_AS_ON)
		printf("%s\tenforce neighbor-as yes\n", c);
	else
		printf("%s\tenforce neighbor-as no\n", c);
	if (p->reflector_client) {
		if (conf->clusterid == 0)
			printf("%s\troute-reflector\n", c);
		else {
			ina.s_addr = conf->clusterid;
			printf("%s\troute-reflector %s\n", c,
			    inet_ntoa(ina));
		}
	}
	if (p->if_depend[0])
		printf("%s\tdepend on \"%s\"\n", c, p->if_depend);

	if (p->auth.method == AUTH_MD5SIG)
		printf("%s\ttcp md5sig\n", c);
	else if (p->auth.method == AUTH_IPSEC_MANUAL_ESP ||
	    p->auth.method == AUTH_IPSEC_MANUAL_AH) {
		if (p->auth.method == AUTH_IPSEC_MANUAL_ESP)
			method = "esp";
		else
			method = "ah";

		printf("%s\tipsec %s in spi %u %s XXXXXX", c, method,
		    p->auth.spi_in, print_auth_alg(p->auth.auth_alg_in));
		if (p->auth.enc_alg_in)
			printf(" %s XXXXXX", print_enc_alg(p->auth.enc_alg_in));
		printf("\n");

		printf("%s\tipsec %s out spi %u %s XXXXXX", c, method,
		    p->auth.spi_out, print_auth_alg(p->auth.auth_alg_out));
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

	print_mrt(p->id, p->groupid, c, "\t");

	printf("%s}\n", c);
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
			printf("source-as %u ", r->match.as.as);
		else if (r->match.as.type == AS_TRANSIT)
			printf("transit-as %u ", r->match.as.as);
		else
			printf("unfluffy-as %u ", r->match.as.as);
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
		break;
	case MRT_TABLE_DUMP:
		return "table";
	case MRT_TABLE_DUMP_MP:
		return "table-mp";
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
print_mrt(u_int32_t pid, u_int32_t gid, const char *prep, const char *prep2)
{
	struct mrt	*m;

	if (xmrt_l == NULL)
		return;

	LIST_FOREACH(m, xmrt_l, entry)
		if ((gid != 0 && m->group_id == gid) ||
		    (m->peer_id == pid && m->group_id == gid)) {
			if (MRT2MC(m)->ReopenTimerInterval == 0)
				printf("%s%sdump %s %s\n", prep, prep2,
				    mrt_type(m->type), MRT2MC(m)->name);
			else
				printf("%s%sdump %s %s %d\n", prep, prep2,
				    mrt_type(m->type),
				    MRT2MC(m)->name,
				    MRT2MC(m)->ReopenTimerInterval);
		}
}

void
print_groups(struct bgpd_config *conf, struct peer *peer_l)
{
	struct peer_config	**peerlist;
	struct peer		 *p;
	u_int			  peer_cnt, i;
	u_int32_t		  prev_groupid;
	const char		 *tab	= "\t";
	const char		 *nada	= "";
	const char		 *c;

	peer_cnt = 0;
	for (p = peer_l; p != NULL; p = p->next)
		peer_cnt++;

	if ((peerlist = calloc(peer_cnt, sizeof(struct peer_config *))) == NULL)
		fatal("print_groups calloc");

	i = 0;
	for (p = peer_l; p != NULL; p = p->next)
		peerlist[i++] = &p->conf;

	qsort(peerlist, peer_cnt, sizeof(struct peer_config *), peer_compare);

	prev_groupid = 0;
	for (i = 0; i < peer_cnt; i++) {
		if (peerlist[i]->groupid) {
			c = tab;
			if (peerlist[i]->groupid != prev_groupid) {
				if (prev_groupid)
					printf("}\n\n");
				printf("group \"%s\" {\n", peerlist[i]->group);
				prev_groupid = peerlist[i]->groupid;
			}
		} else
			c = nada;

		print_peer(peerlist[i], conf, c);
	}

	if (prev_groupid)
		printf("}\n\n");

	free(peerlist);
}

int
peer_compare(const void *aa, const void *bb)
{
	const struct peer_config * const *a;
	const struct peer_config * const *b;

	a = aa;
	b = bb;

	return ((*a)->groupid - (*b)->groupid);
}

void
print_config(struct bgpd_config *conf, struct network_head *net_l,
    struct peer *peer_l, struct filter_head *rules_l, struct mrt_head *mrt_l)
{
	struct filter_rule	*r;
	struct network		*n;

	xmrt_l = mrt_l;
	print_mainconf(conf);
	printf("\n");
	print_mrt(0, 0, "", "");
	printf("\n");
	TAILQ_FOREACH(n, net_l, entry)
		print_network(&n->net);
	printf("\n");
	print_groups(conf, peer_l);
	printf("\n");
	TAILQ_FOREACH(r, rules_l, entry)
		print_rule(peer_l, r);
}

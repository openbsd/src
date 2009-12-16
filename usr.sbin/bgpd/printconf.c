/*	$OpenBSD: printconf.c,v 1.76 2009/12/16 15:40:55 claudio Exp $	*/

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
#include "rde.h"

void		 print_op(enum comp_ops);
void		 print_community(int, int);
void		 print_extcommunity(struct filter_extcommunity *);
void		 print_origin(u_int8_t);
void		 print_set(struct filter_set_head *);
void		 print_mainconf(struct bgpd_config *);
void		 print_network(struct network_config *);
void		 print_peer(struct peer_config *, struct bgpd_config *,
		    const char *);
const char	*print_auth_alg(u_int8_t);
const char	*print_enc_alg(u_int8_t);
void		 print_announce(struct peer_config *, const char *);
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
print_community(int as, int type)
{
	if (as == COMMUNITY_ANY)
		printf("*:");
	else if (as == COMMUNITY_NEIGHBOR_AS)
		printf("neighbor-as:");
	else
		printf("%d:", as);

	if (type == COMMUNITY_ANY)
		printf("* ");
	else if (type == COMMUNITY_NEIGHBOR_AS)
		printf("neighbor-as ");
	else
		printf("%d ", type);
}

void
print_extcommunity(struct filter_extcommunity *c)
{
	switch (c->type & EXT_COMMUNITY_VALUE) {
	case EXT_COMMUNITY_TWO_AS:
		printf("%s %i:%i", log_ext_subtype(c->subtype),
		    c->data.ext_as.as, c->data.ext_as.val);
		break;
	case EXT_COMMUNITY_IPV4:
		printf("%s %s:%i", log_ext_subtype(c->subtype),
		    inet_ntoa(c->data.ext_ip.addr), c->data.ext_ip.val);
		break;
	case EXT_COMMUNITY_FOUR_AS:
		printf("%s %s:%i", log_ext_subtype(c->subtype),
		    log_as(c->data.ext_as4.as4), c->data.ext_as.val);
		break;
	case EXT_COMMUNITY_OPAQUE:
		printf("%s 0x%x", log_ext_subtype(c->subtype),
		    c->data.ext_opaq);
		break;
	default:
		printf("0x%x 0x%llx", c->type, c->data.ext_opaq);
		break;
	}
}

void
print_origin(u_int8_t o)
{
	if (o == ORIGIN_IGP)
		printf("igp ");
	else if (o == ORIGIN_EGP)
		printf("egp ");
	else if (o == ORIGIN_INCOMPLETE)
		printf("incomplete ");
	else
		printf("%u ", o);
}

void
print_set(struct filter_set_head *set)
{
	struct filter_set	*s;

	if (TAILQ_EMPTY(set))
		return;

	printf("set { ");
	TAILQ_FOREACH(s, set, entry) {
		switch (s->type) {
		case ACTION_SET_LOCALPREF:
			printf("localpref %u ", s->action.metric);
			break;
		case ACTION_SET_RELATIVE_LOCALPREF:
			printf("localpref %+d ", s->action.relative);
			break;
		case ACTION_SET_MED:
			printf("metric %u ", s->action.metric);
			break;
		case ACTION_SET_RELATIVE_MED:
			printf("metric %+d ", s->action.relative);
			break;
		case ACTION_SET_WEIGHT:
			printf("weight %u ", s->action.metric);
			break;
		case ACTION_SET_RELATIVE_WEIGHT:
			printf("weight %+d ", s->action.relative);
			break;
		case ACTION_SET_NEXTHOP:
			printf("nexthop %s ", log_addr(&s->action.nexthop));
			break;
		case ACTION_SET_NEXTHOP_REJECT:
			printf("nexthop reject ");
			break;
		case ACTION_SET_NEXTHOP_BLACKHOLE:
			printf("nexthop blackhole ");
			break;
		case ACTION_SET_NEXTHOP_NOMODIFY:
			printf("nexthop no-modify ");
			break;
		case ACTION_SET_NEXTHOP_SELF:
			printf("nexthop self ");
			break;
		case ACTION_SET_PREPEND_SELF:
			printf("prepend-self %u ", s->action.prepend);
			break;
		case ACTION_SET_PREPEND_PEER:
			printf("prepend-neighbor %u ", s->action.prepend);
			break;
		case ACTION_DEL_COMMUNITY:
			printf("community delete ");
			print_community(s->action.community.as,
			    s->action.community.type);
			printf(" ");
			break;
		case ACTION_SET_COMMUNITY:
			printf("community ");
			print_community(s->action.community.as,
			    s->action.community.type);
			printf(" ");
			break;
		case ACTION_PFTABLE:
			printf("pftable %s ", s->action.pftable);
			break;
		case ACTION_RTLABEL:
			printf("rtlabel %s ", s->action.rtlabel);
			break;
		case ACTION_SET_ORIGIN:
			printf("origin ");
			print_origin(s->action.origin);
			break;
		case ACTION_RTLABEL_ID:
		case ACTION_PFTABLE_ID:
			/* not possible */
			printf("king bula saiz: config broken");
			break;
		case ACTION_SET_EXT_COMMUNITY:
			printf("ext-community ");
			print_extcommunity(&s->action.ext_community);
			printf(" ");
			break;
		case ACTION_DEL_EXT_COMMUNITY:
			printf("ext-community delete ");
			print_extcommunity(&s->action.ext_community);
			printf(" ");
			break;
		}
	}
	printf("}");
}

void
print_mainconf(struct bgpd_config *conf)
{
	struct in_addr		 ina;
	struct listen_addr	*la;

	printf("AS %s", log_as(conf->as));
	if (conf->as > USHRT_MAX && conf->short_as != AS_TRANS)
		printf(" %u", conf->short_as);
	ina.s_addr = conf->bgpid;
	printf("\nrouter-id %s\n", inet_ntoa(ina));
	if (conf->holdtime)
		printf("holdtime %u\n", conf->holdtime);
	if (conf->min_holdtime)
		printf("holdtime min %u\n", conf->min_holdtime);
	if (conf->connectretry)
		printf("connect-retry %u\n", conf->connectretry);

	if (conf->flags & BGPD_FLAG_NO_FIB_UPDATE)
		printf("fib-update no\n");
	else
		printf("fib-update yes\n");

	if (conf->flags & BGPD_FLAG_NO_EVALUATE)
		printf("route-collector yes\n");

	if (conf->flags & BGPD_FLAG_DECISION_ROUTEAGE)
		printf("rde route-age evaluate\n");

	if (conf->flags & BGPD_FLAG_DECISION_MED_ALWAYS)
		printf("rde med compare always\n");

	if (conf->log & BGPD_LOG_UPDATES)
		printf("log updates\n");

	TAILQ_FOREACH(la, conf->listen_addrs, entry)
		printf("listen on %s\n",
		    log_sockaddr((struct sockaddr *)&la->sa));

	if (conf->flags & BGPD_FLAG_NEXTHOP_BGP)
		printf("nexthop qualify via bgp\n");
	if (conf->flags & BGPD_FLAG_NEXTHOP_DEFAULT)
		printf("nexthop qualify via default\n");

	if (conf->flags & BGPD_FLAG_REDIST_CONNECTED) {
		printf("network inet connected");
		if (!TAILQ_EMPTY(&conf->connectset))
			printf(" ");
		print_set(&conf->connectset);
		printf("\n");
	}
	if (conf->flags & BGPD_FLAG_REDIST_STATIC) {
		printf("network inet static");
		if (!TAILQ_EMPTY(&conf->staticset))
			printf(" ");
		print_set(&conf->staticset);
		printf("\n");
	}
	if (conf->flags & BGPD_FLAG_REDIST6_CONNECTED) {
		printf("network inet6 connected");
		if (!TAILQ_EMPTY(&conf->connectset6))
			printf(" ");
		print_set(&conf->connectset6);
		printf("\n");
	}
	if (conf->flags & BGPD_FLAG_REDIST_STATIC) {
		printf("network inet6 static");
		if (!TAILQ_EMPTY(&conf->staticset6))
			printf(" ");
		print_set(&conf->staticset6);
		printf("\n");
	}
	if (conf->rtableid)
		printf("rtable %u\n", conf->rtableid);
}

void
print_network(struct network_config *n)
{
	printf("network %s/%u", log_addr(&n->prefix), n->prefixlen);
	if (!TAILQ_EMPTY(&n->attrset))
		printf(" ");
	print_set(&n->attrset);
	printf("\n");
}

void
print_peer(struct peer_config *p, struct bgpd_config *conf, const char *c)
{
	char		*method;
	struct in_addr	 ina;

	if ((p->remote_addr.aid == AID_INET && p->remote_masklen != 32) ||
	    (p->remote_addr.aid == AID_INET6 && p->remote_masklen != 128))
		printf("%sneighbor %s/%u {\n", c, log_addr(&p->remote_addr),
		    p->remote_masklen);
	else
		printf("%sneighbor %s {\n", c, log_addr(&p->remote_addr));
	if (p->descr[0])
		printf("%s\tdescr \"%s\"\n", c, p->descr);
	if (p->rib[0])
		printf("%s\trib \"%s\"\n", c, p->rib);
	if (p->remote_as)
		printf("%s\tremote-as %s\n", c, log_as(p->remote_as));
	if (p->down)
		printf("%s\tdown\n", c);
	if (p->distance > 1)
		printf("%s\tmultihop %u\n", c, p->distance);
	if (p->passive)
		printf("%s\tpassive\n", c);
	if (p->local_addr.aid)
		printf("%s\tlocal-address %s\n", c, log_addr(&p->local_addr));
	if (p->max_prefix) {
		printf("%s\tmax-prefix %u", c, p->max_prefix);
		if (p->max_prefix_restart)
			printf(" restart %u", p->max_prefix_restart);
		printf("\n");
	}
	if (p->holdtime)
		printf("%s\tholdtime %u\n", c, p->holdtime);
	if (p->min_holdtime)
		printf("%s\tholdtime min %u\n", c, p->min_holdtime);
	if (p->announce_capa == 0)
		printf("%s\tannounce capabilities no\n", c);
	if (p->capabilities.refresh == 0)
		printf("%s\tannounce refresh no\n", c);
	if (p->capabilities.restart == 1)
		printf("%s\tannounce restart yes\n", c);
	if (p->capabilities.as4byte == 0)
		printf("%s\tannounce as4byte no\n", c);
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
	if (p->demote_group[0])
		printf("%s\tdemote %s\n", c, p->demote_group);
	if (p->if_depend[0])
		printf("%s\tdepend on \"%s\"\n", c, p->if_depend);
	if (p->flags & PEERFLAG_TRANS_AS)
		printf("%s\ttransparent-as yes\n", c);

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

	if (p->ttlsec)
		printf("%s\tttl-security yes\n", c);

	print_announce(p, c);

	if (p->softreconfig_in == 1)
		printf("%s\tsoftreconfig in yes\n", c);
	else
		printf("%s\tsoftreconfig in no\n", c);

	if (p->softreconfig_out == 1)
		printf("%s\tsoftreconfig out yes\n", c);
	else
		printf("%s\tsoftreconfig out no\n", c);


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
print_announce(struct peer_config *p, const char *c)
{
	u_int8_t	aid;

	for (aid = 0; aid < AID_MAX; aid++)
		if (p->capabilities.mp[aid])
			printf("%s\tannounce %s\n", c, aid2str(aid));
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

	if (r->rib[0])
		printf("rib %s ", r->rib);

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
			printf("? ");
		else
			printf("%s ", log_addr(&p->conf.remote_addr));
	} else if (r->peer.groupid) {
		for (p = peer_l; p != NULL &&
		    p->conf.groupid != r->peer.groupid; p = p->next)
			;	/* nothing */
		if (p == NULL)
			printf("group ? ");
		else
			printf("group \"%s\" ", p->conf.group);
	} else
		printf("any ");

	if (r->match.prefix.addr.aid)
		printf("prefix %s/%u ", log_addr(&r->match.prefix.addr),
		    r->match.prefix.len);

	if (r->match.prefix.addr.aid == 0 && r->match.prefixlen.aid) {
		if (r->match.prefixlen.aid == AID_INET)
			printf("inet ");
		if (r->match.prefixlen.aid == AID_INET6)
			printf("inet6 ");
	}

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
			printf("AS %s ", log_as(r->match.as.as));
		else if (r->match.as.type == AS_SOURCE)
			printf("source-as %s ", log_as(r->match.as.as));
		else if (r->match.as.type == AS_TRANSIT)
			printf("transit-as %s ", log_as(r->match.as.as));
		else if (r->match.as.type == AS_PEER)
			printf("peer-as %s ", log_as(r->match.as.as));
		else
			printf("unfluffy-as %s ", log_as(r->match.as.as));
	}

	if (r->match.community.as != COMMUNITY_UNSET) {
		printf("community ");
		print_community(r->match.community.as,
		    r->match.community.type);
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
			printf("%s%sdump ", prep, prep2);
			if (m->rib[0])
				printf("rib %s ", m->rib);
			if (MRT2MC(m)->ReopenTimerInterval == 0)
				printf("%s %s\n", mrt_type(m->type),
				    MRT2MC(m)->name);
			else
				printf("%s %s %d\n", mrt_type(m->type),
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
print_config(struct bgpd_config *conf, struct rib_names *rib_l,
    struct network_head *net_l, struct peer *peer_l,
    struct filter_head *rules_l, struct mrt_head *mrt_l)
{
	struct filter_rule	*r;
	struct network		*n;
	struct rde_rib		*rr;

	xmrt_l = mrt_l;
	printf("\n");
	print_mainconf(conf);
	printf("\n");
	SIMPLEQ_FOREACH(rr, rib_l, entry) {
		if (rr->flags & F_RIB_NOEVALUATE)
			printf("rde rib %s no evaluate\n", rr->name);
		else
			printf("rde rib %s\n", rr->name);
	}
	printf("\n");
	TAILQ_FOREACH(n, net_l, entry)
		print_network(&n->net);
	printf("\n");
	print_mrt(0, 0, "", "");
	printf("\n");
	print_groups(conf, peer_l);
	printf("\n");
	TAILQ_FOREACH(r, rules_l, entry)
		print_rule(peer_l, r);
}

/*	$OpenBSD: printconf.c,v 1.126 2018/12/30 13:53:07 denis Exp $	*/

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2016 Job Snijders <job@instituut.net>
 * Copyright (c) 2016 Peter Hessler <phessler@openbsd.org>
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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bgpd.h"
#include "mrt.h"
#include "session.h"
#include "rde.h"
#include "log.h"

void		 print_prefix(struct filter_prefix *p);
const char	*community_type(struct filter_community *c);
void		 print_community(struct filter_community *c);
void		 print_origin(u_int8_t);
void		 print_set(struct filter_set_head *);
void		 print_mainconf(struct bgpd_config *);
void		 print_rdomain_targets(struct filter_set_head *, const char *);
void		 print_rdomain(struct rdomain *);
const char	*print_af(u_int8_t);
void		 print_network(struct network_config *, const char *);
void		 print_as_sets(struct as_set_head *);
void		 print_prefixsets(struct prefixset_head *);
void		 print_originsets(struct prefixset_head *);
void		 print_roa(struct prefixset_tree *p);
void		 print_peer(struct peer_config *, struct bgpd_config *,
		    const char *);
const char	*print_auth_alg(u_int8_t);
const char	*print_enc_alg(u_int8_t);
void		 print_announce(struct peer_config *, const char *);
void		 print_as(struct filter_rule *);
void		 print_rule(struct peer *, struct filter_rule *);
const char	*mrt_type(enum mrt_type);
void		 print_mrt(struct bgpd_config *, u_int32_t, u_int32_t,
		    const char *, const char *);
void		 print_groups(struct bgpd_config *, struct peer *);
int		 peer_compare(const void *, const void *);

void
print_prefix(struct filter_prefix *p)
{
	u_int8_t max_len = 0;

	switch (p->addr.aid) {
	case AID_INET:
	case AID_VPN_IPv4:
		max_len = 32;
		break;
	case AID_INET6:
	case AID_VPN_IPv6:
		max_len = 128;
		break;
	case AID_UNSPEC:
		/* no prefix to print */
		return;
	}

	printf("%s/%u", log_addr(&p->addr), p->len);

	switch (p->op) {
	case OP_NONE:
		break;
	case OP_NE:
		printf(" prefixlen != %u", p->len_min);
		break;
	case OP_XRANGE:
		printf(" prefixlen %u >< %u ", p->len_min, p->len_max);
		break;
	case OP_RANGE:
		if (p->len_min == p->len_max && p->len != p->len_min)
			printf(" prefixlen = %u", p->len_min);
		else if (p->len == p->len_min && p->len_max == max_len)
			printf(" or-longer");
		else if (p->len == p->len_min && p->len != p->len_max)
			printf(" maxlen %u", p->len_max);
		else if (p->len_max == max_len)
			printf(" prefixlen >= %u", p->len_min);
		else
			printf(" prefixlen %u - %u", p->len_min, p->len_max);
		break;
	default:
		printf(" prefixlen %u ??? %u", p->len_min, p->len_max);
		break;
	}
}

const char *
community_type(struct filter_community *c)
{
	switch (c->type) {
	case COMMUNITY_TYPE_BASIC:
		return "community";
	case COMMUNITY_TYPE_LARGE:
		return "large-community";
	case COMMUNITY_TYPE_EXT:
		return "ext-community";
	default:
		return "???";
	}
}

void
print_community(struct filter_community *c)
{
	struct in_addr addr;

	switch (c->type) {
	case COMMUNITY_TYPE_BASIC:
		switch (c->dflag1) {
		case COMMUNITY_ANY:
			printf("*:");
			break;
		case COMMUNITY_NEIGHBOR_AS:
			printf("neighbor-as:");
			break;
		case COMMUNITY_LOCAL_AS:
			printf("local-as:");
			break;
		default:
			printf("%u:", c->c.b.data1);
			break;
		}
		switch (c->dflag2) {
		case COMMUNITY_ANY:
			printf("* ");
			break;
		case COMMUNITY_NEIGHBOR_AS:
			printf("neighbor-as ");
			break;
		case COMMUNITY_LOCAL_AS:
			printf("local-as ");
			break;
		default:
			printf("%u ", c->c.b.data2);
			break;
		}
		break;
	case COMMUNITY_TYPE_LARGE:
		switch (c->dflag1) {
		case COMMUNITY_ANY:
			printf("*:");
			break;
		case COMMUNITY_NEIGHBOR_AS:
			printf("neighbor-as:");
			break;
		case COMMUNITY_LOCAL_AS:
			printf("local-as:");
			break;
		default:
			printf("%u:", c->c.l.data1);
			break;
		}
		switch (c->dflag2) {
		case COMMUNITY_ANY:
			printf("*:");
			break;
		case COMMUNITY_NEIGHBOR_AS:
			printf("neighbor-as:");
			break;
		case COMMUNITY_LOCAL_AS:
			printf("local-as:");
			break;
		default:
			printf("%u:", c->c.l.data2);
			break;
		}
		switch (c->dflag3) {
		case COMMUNITY_ANY:
			printf("* ");
			break;
		case COMMUNITY_NEIGHBOR_AS:
			printf("neighbor-as ");
			break;
		case COMMUNITY_LOCAL_AS:
			printf("local-as ");
			break;
		default:
			printf("%u ", c->c.l.data3);
			break;
		}
		break;
	case COMMUNITY_TYPE_EXT:
		printf("%s ", log_ext_subtype(c->c.e.type, c->c.e.subtype));
		switch (c->c.e.type) {
		case EXT_COMMUNITY_TRANS_TWO_AS:
		case EXT_COMMUNITY_TRANS_FOUR_AS:
			printf("%s:%llu ", log_as(c->c.e.data1),
			    c->c.e.data2);
			break;
		case EXT_COMMUNITY_TRANS_IPV4:
			addr.s_addr = htonl(c->c.e.data1);
			printf("%s:%llu ", inet_ntoa(addr),
			    c->c.e.data2);
			break;
		case EXT_COMMUNITY_TRANS_OPAQUE:
		case EXT_COMMUNITY_TRANS_EVPN:
			printf("0x%llx ", c->c.e.data2);
			break;
		case EXT_COMMUNITY_NON_TRANS_OPAQUE:
			switch (c->c.e.data2) {
			case EXT_COMMUNITY_OVS_VALID:
				printf("valid ");
				break;
			case EXT_COMMUNITY_OVS_NOTFOUND:
				printf("not-found ");
				break;
			case EXT_COMMUNITY_OVS_INVALID:
				printf("invalid ");
				break;
			}
			break;
		default:
			printf("0x%llx ", c->c.e.data2);
			break;
		}
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
			printf("%s delete ",
			    community_type(&s->action.community));
			print_community(&s->action.community);
			break;
		case ACTION_SET_COMMUNITY:
			printf("%s ", community_type(&s->action.community));
			print_community(&s->action.community);
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

	printf("socket \"%s\"\n", conf->csock);
	if (conf->rcsock)
		printf("socket \"%s\" restricted\n", conf->rcsock);
	if (conf->holdtime)
		printf("holdtime %u\n", conf->holdtime);
	if (conf->min_holdtime)
		printf("holdtime min %u\n", conf->min_holdtime);
	if (conf->connectretry)
		printf("connect-retry %u\n", conf->connectretry);

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
	printf("fib-priority %hhu", conf->fib_priority);
	printf("\n\n");
}

void
print_rdomain_targets(struct filter_set_head *set, const char *tgt)
{
	struct filter_set	*s;
	TAILQ_FOREACH(s, set, entry) {
		printf("\t%s ", tgt);
		print_community(&s->action.community);
		printf("\n");
	}
}

void
print_rdomain(struct rdomain *r)
{
	struct network *n;

	printf("rdomain %u {\n", r->rtableid);
	if (*r->descr)
		printf("\tdescr \"%s\"\n", r->descr);
	if (r->flags & F_RIB_NOFIBSYNC)
		printf("\tfib-update no\n");
	else
		printf("\tfib-update yes\n");
	printf("\tdepend on %s\n", r->ifmpe);

	TAILQ_FOREACH(n, &r->net_l, entry)
		print_network(&n->net, "\t");

	printf("\n\t%s\n", log_rd(r->rd));

	print_rdomain_targets(&r->export, "export-target");
	print_rdomain_targets(&r->import, "import-target");

	printf("}\n");
}

const char *
print_af(u_int8_t aid)
{
	/*
	 * Hack around the fact that aid2str() will return "IPv4 unicast"
	 * for AID_INET. AID_INET and AID_INET6 need special handling and
	 * the other AID should never end up here (at least for now).
	 */
	if (aid == AID_INET)
		return ("inet");
	if (aid == AID_INET6)
		return ("inet6");
	return (aid2str(aid));
}

void
print_network(struct network_config *n, const char *c)
{
	switch (n->type) {
	case NETWORK_STATIC:
		printf("%snetwork %s static", c, print_af(n->prefix.aid));
		break;
	case NETWORK_CONNECTED:
		printf("%snetwork %s connected", c, print_af(n->prefix.aid));
		break;
	case NETWORK_RTLABEL:
		printf("%snetwork %s rtlabel \"%s\"", c,
		    print_af(n->prefix.aid), rtlabel_id2name(n->rtlabel));
		break;
	case NETWORK_PRIORITY:
		printf("%snetwork %s priority %d", c,
		    print_af(n->prefix.aid), n->priority);
		break;
	case NETWORK_PREFIXSET:
		printf("%snetwork prefix-set %s", c, n->psname);
		break;
	default:
		printf("%snetwork %s/%u", c, log_addr(&n->prefix),
		    n->prefixlen);
		break;
	}
	if (!TAILQ_EMPTY(&n->attrset))
		printf(" ");
	print_set(&n->attrset);
	printf("\n");
}

void
print_as_sets(struct as_set_head *as_sets)
{
	struct as_set *aset;
	u_int32_t *as;
	size_t i, n;
	int len;

	SIMPLEQ_FOREACH(aset, as_sets, entry) {
		printf("as-set \"%s\" {\n\t", aset->name);
		as = set_get(aset->set, &n);
		for (i = 0, len = 8; i < n; i++) {
			if (len > 72) {
				printf("\n\t");
				len = 8;
			}
			len += printf("%u ", as[i]);
		}
		printf("\n}\n\n");
	}
}

void
print_prefixsets(struct prefixset_head *psh)
{
	struct prefixset	*ps;
	struct prefixset_item	*psi;

	SIMPLEQ_FOREACH(ps, psh, entry) {
		int count = 0;
		printf("prefix-set \"%s\" {", ps->name);
		RB_FOREACH(psi, prefixset_tree, &ps->psitems) {
			if (count++ % 2 == 0)
				printf("\n\t");
			else
				printf(", ");
			print_prefix(&psi->p);
		}
		printf("\n}\n\n");
	}
}

void
print_originsets(struct prefixset_head *psh)
{
	struct prefixset	*ps;
	struct prefixset_item	*psi;
	struct roa_set		*rs;
	size_t			 i, n;

	SIMPLEQ_FOREACH(ps, psh, entry) {
		printf("origin-set \"%s\" {", ps->name);
		RB_FOREACH(psi, prefixset_tree, &ps->psitems) {
			rs = set_get(psi->set, &n);
			for (i = 0; i < n; i++) {
				printf("\n\t");
				print_prefix(&psi->p);
				if (psi->p.len != rs[i].maxlen)
					printf(" maxlen %u", rs[i].maxlen);
				printf(" source-as %u", rs[i].as);
			}
		}
		printf("\n}\n\n");
	}
}

void
print_roa(struct prefixset_tree *p)
{
	struct prefixset_item	*psi;
	struct roa_set		*rs;
	size_t			 i, n;

	if (RB_EMPTY(p))
		return;

	printf("roa-set {");
	RB_FOREACH(psi, prefixset_tree, p) {
		rs = set_get(psi->set, &n);
		for (i = 0; i < n; i++) {
			printf("\n\t");
			print_prefix(&psi->p);
			if (psi->p.len != rs[i].maxlen)
				printf(" maxlen %u", rs[i].maxlen);
			printf(" source-as %u", rs[i].as);
		}
	}
	printf("\n}\n\n");
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
	if (p->local_as != conf->as) {
		printf("%s\tlocal-as %s", c, log_as(p->local_as));
		if (p->local_as > USHRT_MAX && p->local_short_as != AS_TRANS)
			printf(" %u", p->local_short_as);
		printf("\n");
	}
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
	if (p->capabilities.grestart.restart == 0)
		printf("%s\tannounce restart no\n", c);
	if (p->capabilities.as4byte == 0)
		printf("%s\tannounce as4byte no\n", c);
	if (p->export_type == EXPORT_NONE)
		printf("%s\texport none\n", c);
	else if (p->export_type == EXPORT_DEFAULT_ROUTE)
		printf("%s\texport default-route\n", c);
	if (p->enforce_as == ENFORCE_AS_ON)
		printf("%s\tenforce neighbor-as yes\n", c);
	else
		printf("%s\tenforce neighbor-as no\n", c);
	if (p->enforce_local_as == ENFORCE_AS_ON)
		printf("%s\tenforce local-as yes\n", c);
	else
		printf("%s\tenforce local-as no\n", c);
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

	if (p->flags & PEERFLAG_LOG_UPDATES)
		printf("%s\tlog updates\n", c);

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

	print_mrt(conf, p->id, p->groupid, c, "\t");

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

void print_as(struct filter_rule *r)
{
	if (r->match.as.flags & AS_FLAG_AS_SET_NAME) {
		printf("as-set \"%s\" ", r->match.as.name);
		return;
	}
	switch(r->match.as.op) {
	case OP_RANGE:
		printf("%s - ", log_as(r->match.as.as_min));
		printf("%s ", log_as(r->match.as.as_max));
		break;
	case OP_XRANGE:
		printf("%s >< ", log_as(r->match.as.as_min));
		printf("%s ", log_as(r->match.as.as_max));
		break;
	case OP_NE:
		printf("!= %s ", log_as(r->match.as.as_min));
		break;
	default:
		printf("%s ", log_as(r->match.as.as_min));
		break;
	}
}

void
print_rule(struct peer *peer_l, struct filter_rule *r)
{
	struct peer *p;
	int i;

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
	} else if (r->peer.remote_as) {
		printf("AS %s ", log_as(r->peer.remote_as));
	} else if (r->peer.ebgp) {
		printf("ebgp ");
	} else if (r->peer.ibgp) {
		printf("ibgp ");
	} else
		printf("any ");

	if (r->match.ovs.is_set) {
		switch (r->match.ovs.validity) {
		case ROA_VALID:
			printf("ovs valid ");
			break;
		case ROA_INVALID:
			printf("ovs invalid ");
			break;
		case ROA_NOTFOUND:
			printf("ovs not-found ");
			break;
		default:
			printf("ovs ??? %d ??? ", r->match.ovs.validity);
		}
	}

	if (r->match.prefix.addr.aid != AID_UNSPEC) {
		printf("prefix ");
		print_prefix(&r->match.prefix);
		printf(" ");
	}

	if (r->match.prefixset.name[0] != '\0')
		printf("prefix-set \"%s\" ", r->match.prefixset.name);
	if (r->match.prefixset.flags & PREFIXSET_FLAG_LONGER)
		printf("or-longer ");

	if (r->match.originset.name[0] != '\0')
		printf("origin-set \"%s\" ", r->match.originset.name);

	if (r->match.nexthop.flags) {
		if (r->match.nexthop.flags == FILTER_NEXTHOP_NEIGHBOR)
			printf("nexthop neighbor ");
		else
			printf("nexthop %s ", log_addr(&r->match.nexthop.addr));
	}

	if (r->match.as.type) {
		if (r->match.as.type == AS_ALL)
			printf("AS ");
		else if (r->match.as.type == AS_SOURCE)
			printf("source-as ");
		else if (r->match.as.type == AS_TRANSIT)
			printf("transit-as ");
		else if (r->match.as.type == AS_PEER)
			printf("peer-as ");
		else
			printf("unfluffy-as ");
		print_as(r);
	}

	if (r->match.aslen.type) {
		printf("%s %u ", r->match.aslen.type == ASLEN_MAX ?
		    "max-as-len" : "max-as-seq", r->match.aslen.aslen);
	}

	for (i = 0; i < MAX_COMM_MATCH; i++) {
		struct filter_community *c = &r->match.community[i];
		if (c->type != COMMUNITY_TYPE_NONE) {
			printf("%s ", community_type(c));
			print_community(c);
		}
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
	case MRT_TABLE_DUMP_V2:
		return "table-v2";
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

void
print_mrt(struct bgpd_config *conf, u_int32_t pid, u_int32_t gid,
    const char *prep, const char *prep2)
{
	struct mrt	*m;

	if (conf->mrt == NULL)
		return;

	LIST_FOREACH(m, conf->mrt, entry)
		if ((gid != 0 && m->group_id == gid) ||
		    (m->peer_id == pid && m->group_id == gid)) {
			printf("%s%sdump ", prep, prep2);
			if (m->rib[0])
				printf("rib %s ", m->rib);
			printf("%s \"%s\"", mrt_type(m->type),
			    MRT2MC(m)->name);
			if (MRT2MC(m)->ReopenTimerInterval == 0)
				printf("\n");
			else
				printf(" %d\n", MRT2MC(m)->ReopenTimerInterval);
		}
	if (!LIST_EMPTY(conf->mrt))
		printf("\n");
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
    struct filter_head *rules_l, struct mrt_head *mrt_l,
    struct rdomain_head *rdom_l)
{
	struct filter_rule	*r;
	struct network		*n;
	struct rde_rib		*rr;
	struct rdomain		*rd;

	print_mainconf(conf);
	print_roa(&conf->roa);
	print_as_sets(conf->as_sets);
	print_prefixsets(&conf->prefixsets);
	print_originsets(&conf->originsets);
	TAILQ_FOREACH(n, net_l, entry)
		print_network(&n->net, "");
	if (!SIMPLEQ_EMPTY(rdom_l))
		printf("\n");
	SIMPLEQ_FOREACH(rd, rdom_l, entry)
		print_rdomain(rd);
	printf("\n");
	SIMPLEQ_FOREACH(rr, rib_l, entry) {
		if (rr->flags & F_RIB_NOEVALUATE)
			printf("rde rib %s no evaluate\n", rr->name);
		else if (rr->flags & F_RIB_NOFIB)
			printf("rde rib %s\n", rr->name);
		else
			printf("rde rib %s rtable %u fib-update %s\n", rr->name,
			    rr->rtableid, rr->flags & F_RIB_NOFIBSYNC ?
			    "no" : "yes");
	}
	printf("\n");
	print_mrt(conf, 0, 0, "", "");
	print_groups(conf, peer_l);
	TAILQ_FOREACH(r, rules_l, entry)
		print_rule(peer_l, r);
}

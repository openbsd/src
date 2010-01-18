/*	$OpenBSD: pf_lb.c,v 1.11 2010/01/18 23:52:46 mcbride Exp $ */

/*
 * Copyright (c) 2001 Daniel Hartmeier
 * Copyright (c) 2002 - 2008 Henning Brauer
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
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#include "bpfilter.h"
#include "pflog.h"
#include "pfsync.h"
#include "pflow.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/filio.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/syslog.h>

#include <crypto/md5.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/route.h>
#include <net/radix_mpath.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/udp_var.h>
#include <netinet/icmp_var.h>
#include <netinet/if_ether.h>

#include <dev/rndvar.h>
#include <net/pfvar.h>
#include <net/if_pflog.h>
#include <net/if_pflow.h>

#if NPFSYNC > 0
#include <net/if_pfsync.h>
#endif /* NPFSYNC > 0 */

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet/in_pcb.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>
#endif /* INET6 */


/*
 * Global variables
 */

void			 pf_hash(struct pf_addr *, struct pf_addr *,
			    struct pf_poolhashkey *, sa_family_t);
int			 pf_get_sport(sa_family_t, u_int8_t, struct pf_rule *,
			    struct pf_addr *, struct pf_addr *, u_int16_t,
			    struct pf_addr *, u_int16_t *, u_int16_t, u_int16_t,
			    struct pf_src_node **, int);

#define mix(a,b,c) \
	do {					\
		a -= b; a -= c; a ^= (c >> 13);	\
		b -= c; b -= a; b ^= (a << 8);	\
		c -= a; c -= b; c ^= (b >> 13);	\
		a -= b; a -= c; a ^= (c >> 12);	\
		b -= c; b -= a; b ^= (a << 16);	\
		c -= a; c -= b; c ^= (b >> 5);	\
		a -= b; a -= c; a ^= (c >> 3);	\
		b -= c; b -= a; b ^= (a << 10);	\
		c -= a; c -= b; c ^= (b >> 15);	\
	} while (0)

/*
 * hash function based on bridge_hash in if_bridge.c
 */
void
pf_hash(struct pf_addr *inaddr, struct pf_addr *hash,
    struct pf_poolhashkey *key, sa_family_t af)
{
	u_int32_t	a = 0x9e3779b9, b = 0x9e3779b9, c = key->key32[0];

	switch (af) {
#ifdef INET
	case AF_INET:
		a += inaddr->addr32[0];
		b += key->key32[1];
		mix(a, b, c);
		hash->addr32[0] = c + key->key32[2];
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		a += inaddr->addr32[0];
		b += inaddr->addr32[2];
		mix(a, b, c);
		hash->addr32[0] = c;
		a += inaddr->addr32[1];
		b += inaddr->addr32[3];
		c += key->key32[1];
		mix(a, b, c);
		hash->addr32[1] = c;
		a += inaddr->addr32[2];
		b += inaddr->addr32[1];
		c += key->key32[2];
		mix(a, b, c);
		hash->addr32[2] = c;
		a += inaddr->addr32[3];
		b += inaddr->addr32[0];
		c += key->key32[3];
		mix(a, b, c);
		hash->addr32[3] = c;
		break;
#endif /* INET6 */
	}
}

int
pf_get_sport(sa_family_t af, u_int8_t proto, struct pf_rule *r,
    struct pf_addr *saddr, struct pf_addr *daddr, u_int16_t dport,
    struct pf_addr *naddr, u_int16_t *nport, u_int16_t low, u_int16_t high,
    struct pf_src_node **sn, int rdomain)
{
	struct pf_state_key_cmp	key;
	struct pf_addr		init_addr;
	u_int16_t		cut;

	bzero(&init_addr, sizeof(init_addr));
	if (pf_map_addr(af, r, saddr, naddr, &init_addr, sn, &r->nat,
	    PF_SN_NAT))
		return (1);

	if (proto == IPPROTO_ICMP || proto == IPPROTO_ICMPV6) {
		if (dport == htons(ICMP6_ECHO_REQUEST) ||
		    dport == htons(ICMP_ECHO)) {
			low = 1;
			high = 65535;
		} else
			return (0);	/* Don't try to modify non-echo ICMP */
	}

	do {
		key.af = af;
		key.proto = proto;
		key.rdomain = rdomain;
		PF_ACPY(&key.addr[1], daddr, key.af);
		PF_ACPY(&key.addr[0], naddr, key.af);
		key.port[1] = dport;

		/*
		 * port search; start random, step;
		 * similar 2 portloop in in_pcbbind
		 */
		if (!(proto == IPPROTO_TCP || proto == IPPROTO_UDP ||
		    proto == IPPROTO_ICMP)) {
			/* XXX bug icmp states dont use the id on both sides */
			key.port[0] = dport;
			if (pf_find_state_all(&key, PF_IN, NULL) == NULL)
				return (0);
		} else if (low == 0 && high == 0) {
			key.port[0] = *nport;
			if (pf_find_state_all(&key, PF_IN, NULL) == NULL)
				return (0);
		} else if (low == high) {
			key.port[0] = htons(low);
			if (pf_find_state_all(&key, PF_IN, NULL) == NULL) {
				*nport = htons(low);
				return (0);
			}
		} else {
			u_int16_t tmp;

			if (low > high) {
				tmp = low;
				low = high;
				high = tmp;
			}
			/* low < high */
			cut = arc4random_uniform(1 + high - low) + low;
			/* low <= cut <= high */
			for (tmp = cut; tmp <= high; ++(tmp)) {
				key.port[0] = htons(tmp);
				if (pf_find_state_all(&key, PF_IN, NULL) ==
				    NULL && !in_baddynamic(tmp, proto)) {
					*nport = htons(tmp);
					return (0);
				}
			}
			for (tmp = cut - 1; tmp >= low; --(tmp)) {
				key.port[0] = htons(tmp);
				if (pf_find_state_all(&key, PF_IN, NULL) ==
				    NULL && !in_baddynamic(tmp, proto)) {
					*nport = htons(tmp);
					return (0);
				}
			}
		}

		switch (r->nat.opts & PF_POOL_TYPEMASK) {
		case PF_POOL_RANDOM:
		case PF_POOL_ROUNDROBIN:
			if (pf_map_addr(af, r, saddr, naddr, &init_addr, sn,
			    &r->nat, PF_SN_NAT))
				return (1);
			break;
		case PF_POOL_NONE:
		case PF_POOL_SRCHASH:
		case PF_POOL_BITMASK:
		default:
			return (1);
		}
	} while (! PF_AEQ(&init_addr, naddr, af) );
	return (1);					/* none available */
}

int
pf_map_addr(sa_family_t af, struct pf_rule *r, struct pf_addr *saddr,
    struct pf_addr *naddr, struct pf_addr *init_addr, struct pf_src_node **sns,
    struct pf_pool *rpool, enum pf_sn_types type)
{
	unsigned char		 hash[16];
	struct pf_addr		*raddr = &rpool->addr.v.a.addr;
	struct pf_addr		*rmask = &rpool->addr.v.a.mask;
	struct pf_src_node	 k;

	if (sns[type] == NULL && rpool->opts & PF_POOL_STICKYADDR &&
	    (rpool->opts & PF_POOL_TYPEMASK) != PF_POOL_NONE) {
		k.af = af;
		k.type = type;
		PF_ACPY(&k.addr, saddr, af);
		k.rule.ptr = r;
		pf_status.scounters[SCNT_SRC_NODE_SEARCH]++;
		sns[type] = RB_FIND(pf_src_tree, &tree_src_tracking, &k);
		if (sns[type] != NULL) {
			if (!PF_AZERO(&(sns[type])->raddr, af))
				PF_ACPY(naddr, &(sns[type])->raddr, af);
			if (pf_status.debug >= LOG_DEBUG) {
				log(LOG_DEBUG, "pf: pf_map_addr: "
				    "src tracking (%u) maps ", type);
				pf_print_host(&k.addr, 0, af);
				addlog(" to ");
				pf_print_host(naddr, 0, af);
				addlog("\n");
			}
			return (0);
		}
	}

	if (rpool->addr.type == PF_ADDR_NOROUTE)
		return (1);
	if (rpool->addr.type == PF_ADDR_DYNIFTL) {
		switch (af) {
#ifdef INET
		case AF_INET:
			if (rpool->addr.p.dyn->pfid_acnt4 < 1 &&
			    (rpool->opts & PF_POOL_TYPEMASK) !=
			    PF_POOL_ROUNDROBIN)
				return (1);
			 raddr = &rpool->addr.p.dyn->pfid_addr4;
			 rmask = &rpool->addr.p.dyn->pfid_mask4;
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			if (rpool->addr.p.dyn->pfid_acnt6 < 1 &&
			    (rpool->opts & PF_POOL_TYPEMASK) !=
			    PF_POOL_ROUNDROBIN)
				return (1);
			raddr = &rpool->addr.p.dyn->pfid_addr6;
			rmask = &rpool->addr.p.dyn->pfid_mask6;
			break;
#endif /* INET6 */
		}
	} else if (rpool->addr.type == PF_ADDR_TABLE) {
		if ((rpool->opts & PF_POOL_TYPEMASK) != PF_POOL_ROUNDROBIN)
			return (1); /* unsupported */
	} else {
		raddr = &rpool->addr.v.a.addr;
		rmask = &rpool->addr.v.a.mask;
	}

	switch (rpool->opts & PF_POOL_TYPEMASK) {
	case PF_POOL_NONE:
		PF_ACPY(naddr, raddr, af);
		break;
	case PF_POOL_BITMASK:
		PF_POOLMASK(naddr, raddr, rmask, saddr, af);
		break;
	case PF_POOL_RANDOM:
		if (init_addr != NULL && PF_AZERO(init_addr, af)) {
			switch (af) {
#ifdef INET
			case AF_INET:
				rpool->counter.addr32[0] = htonl(arc4random());
				break;
#endif /* INET */
#ifdef INET6
			case AF_INET6:
				if (rmask->addr32[3] != 0xffffffff)
					rpool->counter.addr32[3] =
					    htonl(arc4random());
				else
					break;
				if (rmask->addr32[2] != 0xffffffff)
					rpool->counter.addr32[2] =
					    htonl(arc4random());
				else
					break;
				if (rmask->addr32[1] != 0xffffffff)
					rpool->counter.addr32[1] =
					    htonl(arc4random());
				else
					break;
				if (rmask->addr32[0] != 0xffffffff)
					rpool->counter.addr32[0] =
					    htonl(arc4random());
				break;
#endif /* INET6 */
			}
			PF_POOLMASK(naddr, raddr, rmask, &rpool->counter, af);
			PF_ACPY(init_addr, naddr, af);

		} else {
			PF_AINC(&rpool->counter, af);
			PF_POOLMASK(naddr, raddr, rmask, &rpool->counter, af);
		}
		break;
	case PF_POOL_SRCHASH:
		pf_hash(saddr, (struct pf_addr *)&hash, &rpool->key, af);
		PF_POOLMASK(naddr, raddr, rmask, (struct pf_addr *)&hash, af);
		break;
	case PF_POOL_ROUNDROBIN:
		if (rpool->addr.type == PF_ADDR_TABLE) {
			if (pfr_pool_get(rpool->addr.p.tbl,
			    &rpool->tblidx, &rpool->counter,
			    &raddr, &rmask, &rpool->kif, af))
				return (1);
		} else if (rpool->addr.type == PF_ADDR_DYNIFTL) {
			if (pfr_pool_get(rpool->addr.p.dyn->pfid_kt,
			    &rpool->tblidx, &rpool->counter,
			    &raddr, &rmask, &rpool->kif, af))
				return (1);
		} else if (pf_match_addr(0, raddr, rmask, &rpool->counter, af))
			return (1);

		PF_ACPY(naddr, &rpool->counter, af);
		if (init_addr != NULL && PF_AZERO(init_addr, af))
			PF_ACPY(init_addr, naddr, af);
		PF_AINC(&rpool->counter, af);
		break;
	}

	if (rpool->opts & PF_POOL_STICKYADDR) {
		if (sns[type] != NULL) {
			pf_remove_src_node(sns[type]);
			sns[type] = NULL;
		}
		if (pf_insert_src_node(&sns[type], r, type, af, saddr, naddr,
		    0))
			return (1);
	}

	if (pf_status.debug >= LOG_NOTICE &&
	    (rpool->opts & PF_POOL_TYPEMASK) != PF_POOL_NONE) {
		log(LOG_NOTICE, "pf: pf_map_addr: selected address ");
		pf_print_host(naddr, 0, af);
		addlog("\n");
	}

	return (0);
}

int
pf_get_transaddr(struct pf_rule *r, struct pf_pdesc *pd, struct pf_addr *saddr,
    u_int16_t *sport, struct pf_addr *daddr, u_int16_t *dport,
    struct pf_src_node **sns)
{
	struct pf_addr	naddr;
	u_int16_t	nport = 0;

	if (r->nat.addr.type != PF_ADDR_NONE) {
		/* XXX is this right? what if rtable is changed at the same
		 * XXX time? where do I need to figure out the sport? */
		if (pf_get_sport(pd->af, pd->proto, r, saddr,
		    daddr, *dport, &naddr, &nport, r->nat.proxy_port[0],
		    r->nat.proxy_port[1], sns, pd->rdomain)) {
			DPFPRINTF(LOG_NOTICE,
			    "pf: NAT proxy port allocation (%u-%u) failed",
			    r->nat.proxy_port[0],
			    r->nat.proxy_port[1]);
			return (-1);
		}
		PF_ACPY(saddr, &naddr, pd->af);
		if (nport)
			*sport = nport;
	}
	if (r->rdr.addr.type != PF_ADDR_NONE) {
		if (pf_map_addr(pd->af, r, saddr, &naddr, NULL, sns, &r->rdr,
		    PF_SN_RDR))
			return (-1);
		if ((r->rdr.opts & PF_POOL_TYPEMASK) == PF_POOL_BITMASK)
			PF_POOLMASK(&naddr, &naddr,  &r->rdr.addr.v.a.mask,
			    daddr, pd->af);

			if (r->rdr.proxy_port[1]) {
				u_int32_t	tmp_nport;

				tmp_nport = ((ntohs(*dport) -
				    ntohs(r->dst.port[0])) %
				    (r->rdr.proxy_port[1] -
				    r->rdr.proxy_port[0] + 1)) +
				    r->rdr.proxy_port[0];

				/* wrap around if necessary */
				if (tmp_nport > 65535)
					tmp_nport -= 65535;
				nport = htons((u_int16_t)tmp_nport);
			} else if (r->rdr.proxy_port[0])
				nport = htons(r->rdr.proxy_port[0]);

		PF_ACPY(daddr, &naddr, pd->af);
		if (nport)
			*dport = nport;
	}

	return (0);
}

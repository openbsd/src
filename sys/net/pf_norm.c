/*	$OpenBSD: pf_norm.c,v 1.10 2001/09/08 02:10:33 provos Exp $ */

/*
 * Copyright 2001 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/time.h>
#include <sys/pool.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/route.h>
#include <net/if_pflog.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>

#include <net/pfvar.h>

#include "pflog.h"

struct pf_frent {
	LIST_ENTRY(pf_frent) fr_next;
	struct ip *fr_ip;
	struct mbuf *fr_m;
};

#define PFFRAG_SEENLAST	0x0001		/* Seen the last fragment for this */

struct pf_fragment {
	TAILQ_ENTRY(pf_fragment) frag_next;
	struct in_addr	fr_src;
	struct in_addr	fr_dst;
	u_int8_t	fr_p;		/* protocol of this fragment */
	u_int8_t	fr_flags;	/* status flags */
	u_int16_t	fr_id;		/* fragment id for reassemble */
	u_int16_t	fr_max;		/* fragment data max */
	struct timeval	fr_timeout;
	LIST_HEAD(pf_fragq, pf_frent) fr_queue;
};

TAILQ_HEAD(pf_fragqueue, pf_fragment)	pf_fragqueue;

/* Private prototypes */
void			 pf_ip2key(struct pf_tree_key *, struct ip *);
void			 pf_remove_fragment(struct pf_fragment *);
void			 pf_flush_fragments(void);
void			 pf_free_fragment(struct pf_fragment *);
struct pf_fragment	*pf_find_fragment(struct ip *);
struct mbuf		*pf_reassemble(struct mbuf **, struct pf_fragment *,
			    struct pf_frent *, int);
u_int16_t		 pf_cksum_fixup(u_int16_t, u_int16_t, u_int16_t);
int			 pf_normalize_tcp(int, struct ifnet *, struct mbuf *,
			    int, int, struct ip *, struct tcphdr *);

#define PFFRAG_FRENT_HIWAT	5000	/* Number of fragment entries */
#define PFFRAG_FRAG_HIWAT	1000	/* Number of fragmented packets */

#define DPFPRINTF(x)		if (pf_status.debug) printf x

#if NPFLOG > 0
#define		 PFLOG_PACKET(x,a,b,c,d,e) \
		do { \
			HTONS((x)->ip_len); \
			HTONS((x)->ip_off); \
			pflog_packet(a,b,c,d,e); \
			NTOHS((x)->ip_len); \
			NTOHS((x)->ip_off); \
		} while (0)
#else
#define		 PFLOG_PACKET(x,a,b,c,d,e)	((void)0)
#endif

/* Globals */
struct pf_tree_node	*tree_fragment;
struct pool		 pf_frent_pl, pf_frag_pl;
int			 pf_nfrents;
extern int		 pftm_frag;	/* Fragment expire timeout */

void
pf_normalize_init(void)
{
	pool_init(&pf_frent_pl, sizeof(struct pf_frent), 0, 0, 0, "pffrent",
	    0, NULL, NULL, 0);
	pool_init(&pf_frag_pl, sizeof(struct pf_fragment), 0, 0, 0, "pffrag",
	    0, NULL, NULL, 0);

	pool_sethiwat(&pf_frag_pl, PFFRAG_FRAG_HIWAT);
	pool_sethardlimit(&pf_frent_pl, PFFRAG_FRENT_HIWAT, NULL, 0);

	TAILQ_INIT(&pf_fragqueue);
}

void
pf_purge_expired_fragments(void)
{
	struct pf_fragment *frag;
	struct timeval now, expire;

	microtime(&now);

	timerclear(&expire);
	expire.tv_sec = pftm_frag;
	timersub(&now, &expire, &expire);

	while ((frag = TAILQ_LAST(&pf_fragqueue, pf_fragqueue)) != NULL) {
		if (timercmp(&frag->fr_timeout, &expire, >))
			break;

		DPFPRINTF((__FUNCTION__": expiring %p\n", frag));
		pf_free_fragment(frag);
	}
}

/*
 *  Try to flush old fragments to make space for new ones
 */

void
pf_flush_fragments(void)
{
	struct pf_fragment *frag;
	int goal = pf_nfrents * 9 / 10;

	DPFPRINTF((__FUNCTION__": trying to free > %d frents\n",
		   pf_nfrents - goal));

	while (goal < pf_nfrents) {
		frag = TAILQ_LAST(&pf_fragqueue, pf_fragqueue);
		if (frag == NULL)
			break;
		pf_free_fragment(frag);
	}
}

/* Frees the fragments and all associated entries */

void
pf_free_fragment(struct pf_fragment *frag)
{
	struct pf_frent *frent;

	/* Free all fragments */
	for (frent = LIST_FIRST(&frag->fr_queue); frent;
	    frent = LIST_FIRST(&frag->fr_queue)) {
		LIST_REMOVE(frent, fr_next);

		m_freem(frent->fr_m);
		pool_put(&pf_frent_pl, frent);
		pf_nfrents--;
	}

	pf_remove_fragment(frag);
}

void
pf_ip2key(struct pf_tree_key *key, struct ip *ip)
{
	key->proto = ip->ip_p;
	key->addr[0] = ip->ip_src;
	key->addr[1] = ip->ip_dst;
	key->port[0] = ip->ip_id;
	key->port[1] = 0;
}

struct pf_fragment *
pf_find_fragment(struct ip *ip)
{
	struct pf_tree_key key;
	struct pf_fragment *frag;

	pf_ip2key(&key, ip);

	frag = (struct pf_fragment *)pf_find_state(tree_fragment, &key);

	if (frag != NULL) {
		microtime(&frag->fr_timeout);
		TAILQ_REMOVE(&pf_fragqueue, frag, frag_next);
		TAILQ_INSERT_HEAD(&pf_fragqueue, frag, frag_next);
	}

	return (frag);
}

/* Removes a fragment from the fragment queue and frees the fragment */

void
pf_remove_fragment(struct pf_fragment *frag)
{
	struct pf_tree_key key;

	key.proto = frag->fr_p;
	key.addr[0] = frag->fr_src;
	key.addr[1] = frag->fr_dst;
	key.port[0] = frag->fr_id;
	key.port[1] = 0;

	pf_tree_remove(&tree_fragment, NULL, &key);
	TAILQ_REMOVE(&pf_fragqueue, frag, frag_next);

	pool_put(&pf_frag_pl, frag);
}

struct mbuf *
pf_reassemble(struct mbuf **m0, struct pf_fragment *frag,
    struct pf_frent *frent, int mff)
{
	struct mbuf *m = *m0, *m2;
	struct pf_frent *frea, *next;
	struct pf_frent *frep = NULL;
	struct ip *ip = frent->fr_ip;
	int hlen = ip->ip_hl << 2;
	u_int16_t off = ip->ip_off;
	u_int16_t max = ip->ip_len + off;

	/* Strip off ip header */
	m->m_data += hlen;
	m->m_len -= hlen;

	/* Create a new reassembly queue for this packet */
	if (frag == NULL) {
		struct pf_tree_key key;

		frag = pool_get(&pf_frag_pl, M_NOWAIT);
		if (frag == NULL) {
			pf_flush_fragments();
			frag = pool_get(&pf_frag_pl, M_NOWAIT);
			if (frag == NULL)
				goto drop_fragment;
		}

		frag->fr_flags = 0;
		frag->fr_max = 0;
		frag->fr_src = frent->fr_ip->ip_src;
		frag->fr_dst = frent->fr_ip->ip_dst;
		frag->fr_p = frent->fr_ip->ip_p;
		frag->fr_id = frent->fr_ip->ip_id;
		LIST_INIT(&frag->fr_queue);

		pf_ip2key(&key, frent->fr_ip);

		pf_tree_insert(&tree_fragment, NULL, &key,
		    (struct pf_state *)frag);
		TAILQ_INSERT_HEAD(&pf_fragqueue, frag, frag_next);

		/* We do not have a previous fragment */
		frep = NULL;
		goto insert;
	}

	/*
	 * Find a fragment after the current one:
	 *  - off contains the real shifted offset.
	 */
	LIST_FOREACH(frea, &frag->fr_queue, fr_next) {
		if (frea->fr_ip->ip_off > off)
			break;
		frep = frea;
	}

	KASSERT(frep != NULL || frea != NULL);

	if (frep != NULL) {
		u_int16_t precut;

		precut = frep->fr_ip->ip_off + frep->fr_ip->ip_len - off;
		if (precut >= ip->ip_len)
			goto drop_fragment;
		if (precut) {
			m_adj(frent->fr_m, precut);

			DPFPRINTF((__FUNCTION__": overlap -%d\n", precut));
			/* Enforce 8 byte boundaries */
			off = ip->ip_off += precut;
			ip->ip_len -= precut;
		}
	}

	for (; frea != NULL && ip->ip_len + off > frea->fr_ip->ip_off;
	    frea = next) {
		u_int16_t aftercut;

		aftercut = (ip->ip_len + off) - frea->fr_ip->ip_off;
		DPFPRINTF((__FUNCTION__": adjust overlap %d\n", aftercut));
		if (aftercut < frea->fr_ip->ip_len) {
			frea->fr_ip->ip_len -= aftercut;
			frea->fr_ip->ip_off += aftercut;
			m_adj(frea->fr_m, aftercut);
			break;
		}

		/* This fragment is completely overlapped, loose it */
		next = LIST_NEXT(frea, fr_next);
		m_freem(frea->fr_m);
		LIST_REMOVE(frea, fr_next);
		pool_put(&pf_frent_pl, frea);
		pf_nfrents--;
	}

 insert:
	/* Update maxmimum data size */
	if (frag->fr_max < max)
		frag->fr_max = max;
	/* This is the last segment */
	if (!mff)
		frag->fr_flags |= PFFRAG_SEENLAST;

	if (frep == NULL)
		LIST_INSERT_HEAD(&frag->fr_queue, frent, fr_next);
	else
		LIST_INSERT_AFTER(frep, frent, fr_next);

	/* Check if we are completely reassembled */
	if (!(frag->fr_flags & PFFRAG_SEENLAST))
		return (NULL);

	/* Check if we have all the data */
	off = 0;
	for (frep = LIST_FIRST(&frag->fr_queue); frep; frep = next) {
		next = LIST_NEXT(frep, fr_next);

		off += frep->fr_ip->ip_len;
		if (off < frag->fr_max &&
		    (next == NULL || next->fr_ip->ip_off != off)) {
			DPFPRINTF((__FUNCTION__
			    ": missing fragment at %d, next %d, max %d\n",
			    off, next == NULL ? -1 : next->fr_ip->ip_off,
			    frag->fr_max));
			return (NULL);
		}
	}
	DPFPRINTF((__FUNCTION__": %d < %d?\n", off, frag->fr_max));
	if (off < frag->fr_max)
		return (NULL);

	/* We have all the data */
	frent = LIST_FIRST(&frag->fr_queue);
	KASSERT(frent != NULL);
	if ((frent->fr_ip->ip_hl << 2) + off > IP_MAXPACKET) {
		DPFPRINTF((__FUNCTION__": drop: too big: %d\n", off));
		pf_free_fragment(frag);
		return (NULL);
	}
	next = LIST_NEXT(frent, fr_next);

	/* Magic from ip_input */
	ip = frent->fr_ip;
	m = frent->fr_m;
	m2 = m->m_next;
	m->m_next = NULL;
	m_cat(m, m2);
	pool_put(&pf_frent_pl, frent);
	pf_nfrents--;
	for (frent = next; frent != NULL; frent = next) {
		next = LIST_NEXT(frent, fr_next);

		m2 = frent->fr_m;
		pool_put(&pf_frent_pl, frent);
		pf_nfrents--;
		m_cat(m, m2);
	}

	ip->ip_src = frag->fr_src;
	ip->ip_dst = frag->fr_dst;

	/* Remove from fragment queue */
	pf_remove_fragment(frag);

	hlen = ip->ip_hl << 2;
	ip->ip_len = off + hlen;
	m->m_len += hlen;
	m->m_data -= hlen;

	/* some debugging cruft by sklower, below, will go away soon */
	/* XXX this should be done elsewhere */
	if (m->m_flags & M_PKTHDR) {
		int plen = 0;
		for (m2 = m; m2; m2 = m2->m_next)
			plen += m2->m_len;
		m->m_pkthdr.len = plen;
	}

	DPFPRINTF((__FUNCTION__": complete: %p(%d)\n", m, ip->ip_len));
	return (m);

 drop_fragment:
	/* Oops - fail safe - drop packet */
	m_freem(m);
	return (NULL);
}

int
pf_normalize_ip(struct mbuf **m0, int dir, struct ifnet *ifp, u_short *reason)
{
	struct mbuf *m = *m0;
	struct pf_rule *r;
	struct pf_frent *frent;
	struct pf_fragment *frag;
	struct ip *h = mtod(m, struct ip *);
	int mff = (h->ip_off & IP_MF), hlen = h->ip_hl << 2;
	u_int16_t fragoff = (h->ip_off & IP_OFFMASK) << 3;
	u_int16_t max;

	TAILQ_FOREACH(r, pf_rules_active, entries) {
		if ((r->action == PF_SCRUB) &&
		    MATCH_TUPLE(h, r, dir, ifp))
			break;
	}

	if (r == NULL)
		return (PF_PASS);

	/* Check for illegal packets */
	if (hlen < sizeof(struct ip))
		goto drop;

	if (hlen > h->ip_len)
		goto drop;

	/* We will need other tests here */
	if (!fragoff && !mff)
		goto no_fragment;

	/* Now we are dealing with a fragmented packet */
	frag = pf_find_fragment(h);

	/* This can not happen */
	if (h->ip_off & IP_DF) {
		DPFPRINTF((__FUNCTION__": IP_DF\n"));
		goto bad;
	}

	h->ip_len -= hlen;
	h->ip_off <<= 3;

	/* All fragments are 8 byte aligned */
	if (mff && (h->ip_len & 0x7)) {
		DPFPRINTF((__FUNCTION__": mff and %d\n", h->ip_len));
		goto bad;
	}

	max = fragoff + h->ip_len;
	/* Respect maximum length */
	if (max > IP_MAXPACKET) {
		DPFPRINTF((__FUNCTION__": max packet %d\n", max));
		goto bad;
	}
	/* Check if we saw the last fragment already */
	if (frag != NULL && (frag->fr_flags & PFFRAG_SEENLAST) &&
	    max > frag->fr_max)
		goto bad;

	/* Get an entry for the fragment queue */
	frent = pool_get(&pf_frent_pl, PR_NOWAIT);
	if (frent == NULL) {
		/* Try to clean up old fragments */
		pf_flush_fragments();
		frent = pool_get(&pf_frent_pl, PR_NOWAIT);
		if (frent == NULL) {
			REASON_SET(reason, PFRES_MEMORY);
			return (PF_DROP);
		}
	}
	pf_nfrents++;
	frent->fr_ip = h;
	frent->fr_m = m;

	/* Might return a completely reassembled mbuf, or NULL */
	DPFPRINTF((__FUNCTION__": reass frag %d @ %d\n", h->ip_id, fragoff));
	*m0 = m = pf_reassemble(m0, frag, frent, mff);

	if (m == NULL)
		return (PF_DROP);

	h = mtod(m, struct ip *);

 no_fragment:
	if (dir != PF_OUT)
		return (PF_PASS);

	/* At this point, only IP_DF is allowed in ip_off */
	if (r->rule_flag & PFRULE_NODF)
		h->ip_off = 0;
	else
		h->ip_off &= IP_DF;

	/* Enforce a minimum ttl, may cause endless packet loops */
	if (r->min_ttl && h->ip_ttl < r->min_ttl)
		h->ip_ttl = r->min_ttl;

	return (PF_PASS);

 drop:
	REASON_SET(reason, PFRES_NORM);
	if (r != NULL && r->log)
		PFLOG_PACKET(h, m, AF_INET, dir, *reason, r);
	return (PF_DROP);

 bad:
	DPFPRINTF((__FUNCTION__": dropping bad fragment\n"));

	/* Free assoicated fragments */
	if (frag != NULL)
		pf_free_fragment(frag);

	REASON_SET(reason, PFRES_FRAG);
	if (r != NULL && r->log)
		PFLOG_PACKET(h, m, AF_INET, dir, *reason, r);

	return (PF_DROP);
}

int
pf_normalize_tcp(int dir, struct ifnet *ifp, struct mbuf *m, int ipoff,
    int off, struct ip *h, struct tcphdr *th)
{
	struct pf_rule *r, *rm = NULL;
	int rewrite = 0, reason;
	u_int8_t flags;

	r = TAILQ_FIRST(pf_rules_active);
	while (r != NULL) {
		if (r->action != PF_SCRUB) {
			r = TAILQ_NEXT(r, entries);
			continue;
		}
		if (r->ifp != NULL && r->ifp != ifp)
			r = r->skip[0];
		else if (r->proto && r->proto != h->ip_p)
			r = r->skip[1];
		else if (r->src.mask && !pf_match_addr(r->src.not,
			    r->src.addr, r->src.mask, h->ip_src.s_addr))
			r = r->skip[2];
		else if (r->src.port_op && !pf_match_port(r->src.port_op,
			    r->src.port[0], r->src.port[1], th->th_sport))
			r = r->skip[3];
		else if (r->dst.mask && !pf_match_addr(r->dst.not,
			    r->dst.addr, r->dst.mask, h->ip_dst.s_addr))
			r = r->skip[4];
		else if (r->dst.port_op && !pf_match_port(r->dst.port_op,
			    r->dst.port[0], r->dst.port[1], th->th_dport))
			r = r->skip[5];
		else if (r->direction != dir)
			r = TAILQ_NEXT(r, entries);
		else if (r->ifp != NULL && r->ifp != ifp)
			r = TAILQ_NEXT(r, entries);
		else {
			rm = r;
			break;
		}
	}

	if (rm == NULL)
		return (PF_PASS);

	flags = th->th_flags;
	if (flags & TH_SYN) {
		/* Illegal packet */
		if (flags & TH_RST)
			goto tcp_drop;

		if (flags & TH_FIN)
			flags &= ~TH_FIN;
	} else {
		/* Illegal packet */
		if (!(flags & (TH_ACK|TH_RST)))
			goto tcp_drop;
	}

	if (!(flags & TH_ACK)) {
		/* These flags are only valid if ACK is set */
		if ((flags & TH_FIN) || (flags & TH_PUSH) || (flags & TH_URG))
			goto tcp_drop;
	}

	/* Check for illegal header length */
	if (th->th_off < (sizeof(struct tcphdr) >> 2))
		goto tcp_drop;

	/* If flags changed, or reserved data set, then adjust */
	if (flags != th->th_flags || th->th_x2 != 0) {
		u_int16_t ov, nv;

		ov = *(u_int16_t *)(&th->th_ack + 1);
		th->th_flags = flags;
		th->th_x2 = 0;
		nv = *(u_int16_t *)(&th->th_ack + 1);

		th->th_sum = pf_cksum_fixup(th->th_sum, ov, nv);
		rewrite = 1;
	}

	/* Remove urgent pointer, if TH_URG is not set */
	if (!(flags & TH_URG) && th->th_urp) {
		th->th_sum = pf_cksum_fixup(th->th_sum, th->th_urp, 0);
		th->th_urp = 0;
		rewrite = 1;
	}

	/* copy back packet headers if we sanitized */
	if (rewrite)
		m_copyback(m, off, sizeof(*th), (caddr_t)th);

	return (PF_PASS);

 tcp_drop:
	REASON_SET(&reason, PFRES_NORM);
	if (rm != NULL && rm->log)
		PFLOG_PACKET(h, m, AF_INET, dir, reason, rm);
	return (PF_DROP);
}

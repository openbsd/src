/*	$OpenBSD: pf_norm.c,v 1.176 2015/04/17 16:42:50 bluhm Exp $ */

/*
 * Copyright 2001 Niels Provos <provos@citi.umich.edu>
 * Copyright 2009 Henning Brauer <henning@openbsd.org>
 * Copyright 2011 Alexander Bluhm <bluhm@openbsd.org>
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

#include "pflog.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/pool.h>
#include <sys/syslog.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_fsm.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_pflog.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif /* INET6 */

#include <net/pfvar.h>

struct pf_frent {
	TAILQ_ENTRY(pf_frent) fr_next;
	struct mbuf	*fe_m;
	u_int16_t	 fe_hdrlen;	/* ipv4 header length with ip options
					   ipv6, extension, fragment header */
	u_int16_t	 fe_extoff;	/* last extension header offset or 0 */
	u_int16_t	 fe_len;	/* fragment length */
	u_int16_t	 fe_off;	/* fragment offset */
	u_int16_t	 fe_mff;	/* more fragment flag */
};

/* keep synced with struct pf_fragment, used in RB_FIND */
struct pf_fragment_cmp {
	struct pf_addr	fr_src;
	struct pf_addr	fr_dst;
	u_int32_t	fr_id;
	sa_family_t	fr_af;
	u_int8_t	fr_proto;
	u_int8_t	fr_direction;
};

struct pf_fragment {
	struct pf_addr	fr_src;		/* ip source address */
	struct pf_addr	fr_dst;		/* ip destination address */
	u_int32_t	fr_id;		/* fragment id for reassemble */
	sa_family_t	fr_af;		/* address family */
	u_int8_t	fr_proto;	/* protocol of this fragment */
	u_int8_t	fr_direction;	/* pf packet direction */

	RB_ENTRY(pf_fragment) fr_entry;
	TAILQ_ENTRY(pf_fragment) frag_next;
	TAILQ_HEAD(pf_fragq, pf_frent) fr_queue;
	int32_t		fr_timeout;
	u_int16_t	fr_maxlen;	/* maximum length of single fragment */
};

struct pf_fragment_tag {
	u_int16_t	 ft_hdrlen;	/* header length of reassembled pkt */
	u_int16_t	 ft_extoff;	/* last extension header offset or 0 */
	u_int16_t	 ft_maxlen;	/* maximum fragment payload length */
};

TAILQ_HEAD(pf_fragqueue, pf_fragment)	pf_fragqueue;

static __inline int	 pf_frag_compare(struct pf_fragment *,
			    struct pf_fragment *);
RB_HEAD(pf_frag_tree, pf_fragment)	pf_frag_tree, pf_cache_tree;
RB_PROTOTYPE(pf_frag_tree, pf_fragment, fr_entry, pf_frag_compare);
RB_GENERATE(pf_frag_tree, pf_fragment, fr_entry, pf_frag_compare);

/* Private prototypes */
void			 pf_flush_fragments(void);
void			 pf_free_fragment(struct pf_fragment *);
struct pf_fragment	*pf_find_fragment(struct pf_fragment_cmp *,
			    struct pf_frag_tree *);
struct pf_frent		*pf_create_fragment(u_short *);
struct pf_fragment	*pf_fillup_fragment(struct pf_fragment_cmp *,
			    struct pf_frent *, u_short *);
int			 pf_isfull_fragment(struct pf_fragment *);
struct mbuf		*pf_join_fragment(struct pf_fragment *);
int			 pf_reassemble(struct mbuf **, int, u_short *);
#ifdef INET6
int			 pf_reassemble6(struct mbuf **, struct ip6_frag *,
			    u_int16_t, u_int16_t, int, u_short *);
#endif /* INET6 */

/* Globals */
struct pool		 pf_frent_pl, pf_frag_pl;
struct pool		 pf_state_scrub_pl;
int			 pf_nfrents;

void
pf_normalize_init(void)
{
	pool_init(&pf_frent_pl, sizeof(struct pf_frent), 0, 0, 0, "pffrent",
	    NULL);
	pool_init(&pf_frag_pl, sizeof(struct pf_fragment), 0, 0, 0, "pffrag",
	    NULL);
	pool_init(&pf_state_scrub_pl, sizeof(struct pf_state_scrub), 0, 0, 0,
	    "pfstscr", NULL);

	pool_sethiwat(&pf_frag_pl, PFFRAG_FRAG_HIWAT);
	pool_sethardlimit(&pf_frent_pl, PFFRAG_FRENT_HIWAT, NULL, 0);

	TAILQ_INIT(&pf_fragqueue);
}

static __inline int
pf_frag_compare(struct pf_fragment *a, struct pf_fragment *b)
{
	int	diff;

	if ((diff = a->fr_id - b->fr_id) != 0)
		return (diff);
	if ((diff = a->fr_proto - b->fr_proto) != 0)
		return (diff);
	if ((diff = a->fr_af - b->fr_af) != 0)
		return (diff);
	if ((diff = pf_addr_compare(&a->fr_src, &b->fr_src, a->fr_af)) != 0)
		return (diff);
	if ((diff = pf_addr_compare(&a->fr_dst, &b->fr_dst, a->fr_af)) != 0)
		return (diff);

	return (0);
}

void
pf_purge_expired_fragments(void)
{
	struct pf_fragment	*frag;
	int32_t			 expire;

	expire = time_uptime - pf_default_rule.timeout[PFTM_FRAG];
	while ((frag = TAILQ_LAST(&pf_fragqueue, pf_fragqueue)) != NULL) {
		if (frag->fr_timeout > expire)
			break;
		DPFPRINTF(LOG_NOTICE, "expiring %d(%p)", frag->fr_id, frag);
		pf_free_fragment(frag);
	}
}

/*
 * Try to flush old fragments to make space for new ones
 */
void
pf_flush_fragments(void)
{
	struct pf_fragment	*frag;
	int			 goal;

	goal = pf_nfrents * 9 / 10;
	DPFPRINTF(LOG_NOTICE, "trying to free > %d frents", pf_nfrents - goal);
	while (goal < pf_nfrents) {
		if ((frag = TAILQ_LAST(&pf_fragqueue, pf_fragqueue)) == NULL)
			break;
		pf_free_fragment(frag);
	}
}

/*
 * Remove a fragment from the fragment queue, free its fragment entries,
 * and free the fragment itself.
 */
void
pf_free_fragment(struct pf_fragment *frag)
{
	struct pf_frent		*frent;

	RB_REMOVE(pf_frag_tree, &pf_frag_tree, frag);
	TAILQ_REMOVE(&pf_fragqueue, frag, frag_next);

	/* Free all fragment entries */
	while ((frent = TAILQ_FIRST(&frag->fr_queue)) != NULL) {
		TAILQ_REMOVE(&frag->fr_queue, frent, fr_next);
		m_freem(frent->fe_m);
		pool_put(&pf_frent_pl, frent);
		pf_nfrents--;
	}
	pool_put(&pf_frag_pl, frag);
}

struct pf_fragment *
pf_find_fragment(struct pf_fragment_cmp *key, struct pf_frag_tree *tree)
{
	struct pf_fragment	*frag;

	frag = RB_FIND(pf_frag_tree, tree, (struct pf_fragment *)key);
	if (frag != NULL) {
		TAILQ_REMOVE(&pf_fragqueue, frag, frag_next);
		TAILQ_INSERT_HEAD(&pf_fragqueue, frag, frag_next);
	}

	return (frag);
}

struct pf_frent *
pf_create_fragment(u_short *reason)
{
	struct pf_frent	*frent;

	frent = pool_get(&pf_frent_pl, PR_NOWAIT);
	if (frent == NULL) {
		pf_flush_fragments();
		frent = pool_get(&pf_frent_pl, PR_NOWAIT);
		if (frent == NULL) {
			REASON_SET(reason, PFRES_MEMORY);
			return (NULL);
		}
	}
	pf_nfrents++;

	return (frent);
}

struct pf_fragment *
pf_fillup_fragment(struct pf_fragment_cmp *key, struct pf_frent *frent,
    u_short *reason)
{
	struct pf_frent		*after, *next, *prev;
	struct pf_fragment	*frag;
	u_int16_t		 total;

	/* No empty fragments */
	if (frent->fe_len == 0) {
		DPFPRINTF(LOG_NOTICE, "bad fragment: len 0");
		goto bad_fragment;
	}

	/* All fragments are 8 byte aligned */
	if (frent->fe_mff && (frent->fe_len & 0x7)) {
		DPFPRINTF(LOG_NOTICE, "bad fragment: mff and len %d",
		    frent->fe_len);
		goto bad_fragment;
	}

	/* Respect maximum length, IP_MAXPACKET == IPV6_MAXPACKET */
	if (frent->fe_off + frent->fe_len > IP_MAXPACKET) {
		DPFPRINTF(LOG_NOTICE, "bad fragment: max packet %d",
		    frent->fe_off + frent->fe_len);
		goto bad_fragment;
	}

	DPFPRINTF(LOG_NOTICE, key->fr_af == AF_INET ?
	    "reass frag %d @ %d-%d" : "reass frag %#08x @ %d-%d",
	    key->fr_id, frent->fe_off, frent->fe_off + frent->fe_len);

	/* Fully buffer all of the fragments in this fragment queue */
	frag = pf_find_fragment(key, &pf_frag_tree);

	/* Create a new reassembly queue for this packet */
	if (frag == NULL) {
		frag = pool_get(&pf_frag_pl, PR_NOWAIT);
		if (frag == NULL) {
			pf_flush_fragments();
			frag = pool_get(&pf_frag_pl, PR_NOWAIT);
			if (frag == NULL) {
				REASON_SET(reason, PFRES_MEMORY);
				goto drop_fragment;
			}
		}

		*(struct pf_fragment_cmp *)frag = *key;
		TAILQ_INIT(&frag->fr_queue);
		frag->fr_timeout = time_uptime;
		frag->fr_maxlen = frent->fe_len;

		RB_INSERT(pf_frag_tree, &pf_frag_tree, frag);
		TAILQ_INSERT_HEAD(&pf_fragqueue, frag, frag_next);

		/* We do not have a previous fragment */
		TAILQ_INSERT_HEAD(&frag->fr_queue, frent, fr_next);

		return (frag);
	}

	KASSERT(!TAILQ_EMPTY(&frag->fr_queue));

	/* Remember maximum fragment len for refragmentation */
	if (frent->fe_len > frag->fr_maxlen)
		frag->fr_maxlen = frent->fe_len;

	/* Maximum data we have seen already */
	total = TAILQ_LAST(&frag->fr_queue, pf_fragq)->fe_off +
	    TAILQ_LAST(&frag->fr_queue, pf_fragq)->fe_len;

	/* Non terminal fragments must have more fragments flag */
	if (frent->fe_off + frent->fe_len < total && !frent->fe_mff)
		goto bad_fragment;

	/* Check if we saw the last fragment already */
	if (!TAILQ_LAST(&frag->fr_queue, pf_fragq)->fe_mff) {
		if (frent->fe_off + frent->fe_len > total ||
		    (frent->fe_off + frent->fe_len == total && frent->fe_mff))
			goto bad_fragment;
	} else {
		if (frent->fe_off + frent->fe_len == total && !frent->fe_mff)
			goto bad_fragment;
	}

	/* Find a fragment after the current one */
	prev = NULL;
	TAILQ_FOREACH(after, &frag->fr_queue, fr_next) {
		if (after->fe_off > frent->fe_off)
			break;
		prev = after;
	}

	KASSERT(prev != NULL || after != NULL);

	if (prev != NULL && prev->fe_off + prev->fe_len > frent->fe_off) {
		u_int16_t	precut;

#ifdef INET6
		if (frag->fr_af == AF_INET6)
			goto free_fragment;
#endif /* INET6 */

		precut = prev->fe_off + prev->fe_len - frent->fe_off;
		if (precut >= frent->fe_len) {
			DPFPRINTF(LOG_NOTICE, "new frag overlapped");
			goto drop_fragment;
		}
		DPFPRINTF(LOG_NOTICE, "frag head overlap %d", precut);
		m_adj(frent->fe_m, precut);
		frent->fe_off += precut;
		frent->fe_len -= precut;
	}

	for (; after != NULL && frent->fe_off + frent->fe_len > after->fe_off;
	    after = next) {
		u_int16_t	aftercut;

#ifdef INET6
		if (frag->fr_af == AF_INET6)
			goto free_fragment;
#endif /* INET6 */

		aftercut = frent->fe_off + frent->fe_len - after->fe_off;
		if (aftercut < after->fe_len) {
			DPFPRINTF(LOG_NOTICE, "frag tail overlap %d", aftercut);
			m_adj(after->fe_m, aftercut);
			after->fe_off += aftercut;
			after->fe_len -= aftercut;
			break;
		}

		/* This fragment is completely overlapped, lose it */
		DPFPRINTF(LOG_NOTICE, "old frag overlapped");
		next = TAILQ_NEXT(after, fr_next);
		TAILQ_REMOVE(&frag->fr_queue, after, fr_next);
		m_freem(after->fe_m);
		pool_put(&pf_frent_pl, after);
		pf_nfrents--;
	}

	if (prev == NULL)
		TAILQ_INSERT_HEAD(&frag->fr_queue, frent, fr_next);
	else
		TAILQ_INSERT_AFTER(&frag->fr_queue, prev, frent, fr_next);

	return (frag);

#ifdef INET6
free_fragment:
	/*
	 * RFC 5722, Errata 3089:  When reassembling an IPv6 datagram, if one
	 * or more its constituent fragments is determined to be an overlapping
	 * fragment, the entire datagram (and any constituent fragments) MUST
	 * be silently discarded.
	 */
	DPFPRINTF(LOG_NOTICE, "flush overlapping fragments");
	pf_free_fragment(frag);
#endif /* INET6 */
bad_fragment:
	REASON_SET(reason, PFRES_FRAG);
drop_fragment:
	pool_put(&pf_frent_pl, frent);
	pf_nfrents--;
	return (NULL);
}

int
pf_isfull_fragment(struct pf_fragment *frag)
{
	struct pf_frent		*frent, *next;
	u_int16_t		 off, total;

	KASSERT(!TAILQ_EMPTY(&frag->fr_queue));

	/* Check if we are completely reassembled */
	if (TAILQ_LAST(&frag->fr_queue, pf_fragq)->fe_mff)
		return (0);

	/* Maximum data we have seen already */
	total = TAILQ_LAST(&frag->fr_queue, pf_fragq)->fe_off +
	    TAILQ_LAST(&frag->fr_queue, pf_fragq)->fe_len;

	/* Check if we have all the data */
	off = 0;
	for (frent = TAILQ_FIRST(&frag->fr_queue); frent; frent = next) {
		next = TAILQ_NEXT(frent, fr_next);
		off += frent->fe_len;
		if (off < total && (next == NULL || next->fe_off != off)) {
			DPFPRINTF(LOG_NOTICE,
			    "missing fragment at %d, next %d, total %d",
			    off, next == NULL ? -1 : next->fe_off, total);
			return (0);
		}
	}
	DPFPRINTF(LOG_NOTICE, "%d < %d?", off, total);
	if (off < total)
		return (0);
	KASSERT(off == total);

	return (1);
}

struct mbuf *
pf_join_fragment(struct pf_fragment *frag)
{
	struct mbuf		*m, *m2;
	struct pf_frent		*frent;

	frent = TAILQ_FIRST(&frag->fr_queue);
	TAILQ_REMOVE(&frag->fr_queue, frent, fr_next);

	m = frent->fe_m;
	/* Strip off any trailing bytes */
	m_adj(m, (frent->fe_hdrlen + frent->fe_len) - m->m_pkthdr.len);
	/* Magic from ip_input */
	m2 = m->m_next;
	m->m_next = NULL;
	m_cat(m, m2);
	pool_put(&pf_frent_pl, frent);
	pf_nfrents--;

	while ((frent = TAILQ_FIRST(&frag->fr_queue)) != NULL) {
		TAILQ_REMOVE(&frag->fr_queue, frent, fr_next);
		m2 = frent->fe_m;
		/* Strip off ip header */
		m_adj(m2, frent->fe_hdrlen);
		/* Strip off any trailing bytes */
		m_adj(m2, frent->fe_len - m2->m_pkthdr.len);
		pool_put(&pf_frent_pl, frent);
		pf_nfrents--;
		m_cat(m, m2);
	}

	/* Remove from fragment queue */
	pf_free_fragment(frag);

	return (m);
}

int
pf_reassemble(struct mbuf **m0, int dir, u_short *reason)
{
	struct mbuf		*m = *m0;
	struct ip		*ip = mtod(m, struct ip *);
	struct pf_frent		*frent;
	struct pf_fragment	*frag;
	struct pf_fragment_cmp	 key;
	u_int16_t		 total, hdrlen;

	/* Get an entry for the fragment queue */
	if ((frent = pf_create_fragment(reason)) == NULL)
		return (PF_DROP);

	frent->fe_m = m;
	frent->fe_hdrlen = ip->ip_hl << 2;
	frent->fe_extoff = 0;
	frent->fe_len = ntohs(ip->ip_len) - (ip->ip_hl << 2);
	frent->fe_off = (ntohs(ip->ip_off) & IP_OFFMASK) << 3;
	frent->fe_mff = ntohs(ip->ip_off) & IP_MF;

	key.fr_src.v4 = ip->ip_src;
	key.fr_dst.v4 = ip->ip_dst;
	key.fr_af = AF_INET;
	key.fr_proto = ip->ip_p;
	key.fr_id = ip->ip_id;
	key.fr_direction = dir;

	if ((frag = pf_fillup_fragment(&key, frent, reason)) == NULL)
		return (PF_DROP);

	/* The mbuf is part of the fragment entry, no direct free or access */
	m = *m0 = NULL;

	if (!pf_isfull_fragment(frag))
		return (PF_PASS);  /* drop because *m0 is NULL, no error */

	/* We have all the data */
	frent = TAILQ_FIRST(&frag->fr_queue);
	KASSERT(frent != NULL);
	total = TAILQ_LAST(&frag->fr_queue, pf_fragq)->fe_off +
	    TAILQ_LAST(&frag->fr_queue, pf_fragq)->fe_len;
	hdrlen = frent->fe_hdrlen;
	m = *m0 = pf_join_fragment(frag);
	frag = NULL;

	if (m->m_flags & M_PKTHDR) {
		int plen = 0;
		for (m = *m0; m; m = m->m_next)
			plen += m->m_len;
		m = *m0;
		m->m_pkthdr.len = plen;
	}

	ip = mtod(m, struct ip *);
	ip->ip_len = htons(hdrlen + total);
	ip->ip_off &= ~(IP_MF|IP_OFFMASK);

	if (hdrlen + total > IP_MAXPACKET) {
		DPFPRINTF(LOG_NOTICE, "drop: too big: %d", total);
		ip->ip_len = 0;
		REASON_SET(reason, PFRES_SHORT);
		/* PF_DROP requires a valid mbuf *m0 in pf_test() */
		return (PF_DROP);
	}

	DPFPRINTF(LOG_NOTICE, "complete: %p(%d)", m, ntohs(ip->ip_len));
	return (PF_PASS);
}

#ifdef INET6
int
pf_reassemble6(struct mbuf **m0, struct ip6_frag *fraghdr,
    u_int16_t hdrlen, u_int16_t extoff, int dir, u_short *reason)
{
	struct mbuf		*m = *m0;
	struct ip6_hdr		*ip6 = mtod(m, struct ip6_hdr *);
	struct m_tag		*mtag;
	struct pf_fragment_tag	*ftag;
	struct pf_frent		*frent;
	struct pf_fragment	*frag;
	struct pf_fragment_cmp	 key;
	int			 off;
	u_int16_t		 total, maxlen;
	u_int8_t		 proto;

	/* Get an entry for the fragment queue */
	if ((frent = pf_create_fragment(reason)) == NULL)
		return (PF_DROP);

	frent->fe_m = m;
	frent->fe_hdrlen = hdrlen;
	frent->fe_extoff = extoff;
	frent->fe_len = sizeof(struct ip6_hdr) + ntohs(ip6->ip6_plen) - hdrlen;
	frent->fe_off = ntohs(fraghdr->ip6f_offlg & IP6F_OFF_MASK);
	frent->fe_mff = fraghdr->ip6f_offlg & IP6F_MORE_FRAG;

	key.fr_src.v6 = ip6->ip6_src;
	key.fr_dst.v6 = ip6->ip6_dst;
	key.fr_af = AF_INET6;
	/* Only the first fragment's protocol is relevant */
	key.fr_proto = 0;
	key.fr_id = fraghdr->ip6f_ident;
	key.fr_direction = dir;

	if ((frag = pf_fillup_fragment(&key, frent, reason)) == NULL)
		return (PF_DROP);

	/* The mbuf is part of the fragment entry, no direct free or access */
	m = *m0 = NULL;

	if (!pf_isfull_fragment(frag))
		return (PF_PASS);  /* drop because *m0 is NULL, no error */

	/* We have all the data */
	extoff = frent->fe_extoff;
	maxlen = frag->fr_maxlen;
	frent = TAILQ_FIRST(&frag->fr_queue);
	KASSERT(frent != NULL);
	total = TAILQ_LAST(&frag->fr_queue, pf_fragq)->fe_off +
	    TAILQ_LAST(&frag->fr_queue, pf_fragq)->fe_len;
	hdrlen = frent->fe_hdrlen - sizeof(struct ip6_frag);
	m = *m0 = pf_join_fragment(frag);
	frag = NULL;

	/* Take protocol from first fragment header */
	if ((m = m_getptr(m, hdrlen + offsetof(struct ip6_frag, ip6f_nxt),
	    &off)) == NULL)
		panic("pf_reassemble6: short mbuf chain");
	proto = *(mtod(m, caddr_t) + off);
	m = *m0;

	/* Delete frag6 header */
	if (frag6_deletefraghdr(m, hdrlen) != 0)
		goto fail;

	if (m->m_flags & M_PKTHDR) {
		int plen = 0;
		for (m = *m0; m; m = m->m_next)
			plen += m->m_len;
		m = *m0;
		m->m_pkthdr.len = plen;
	}

	if ((mtag = m_tag_get(PACKET_TAG_PF_REASSEMBLED, sizeof(struct
	    pf_fragment_tag), M_NOWAIT)) == NULL)
		goto fail;
	ftag = (struct pf_fragment_tag *)(mtag + 1);
	ftag->ft_hdrlen = hdrlen;
	ftag->ft_extoff = extoff;
	ftag->ft_maxlen = maxlen;
	m_tag_prepend(m, mtag);

	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_plen = htons(hdrlen - sizeof(struct ip6_hdr) + total);
	if (extoff) {
		/* Write protocol into next field of last extension header */
		if ((m = m_getptr(m, extoff + offsetof(struct ip6_ext,
		    ip6e_nxt), &off)) == NULL)
			panic("pf_reassemble6: short mbuf chain");
		*(mtod(m, caddr_t) + off) = proto;
		m = *m0;
	} else
		ip6->ip6_nxt = proto;

	if (hdrlen - sizeof(struct ip6_hdr) + total > IPV6_MAXPACKET) {
		DPFPRINTF(LOG_NOTICE, "drop: too big: %d", total);
		ip6->ip6_plen = 0;
		REASON_SET(reason, PFRES_SHORT);
		/* PF_DROP requires a valid mbuf *m0 in pf_test6() */
		return (PF_DROP);
	}

	DPFPRINTF(LOG_NOTICE, "complete: %p(%d)", m, ntohs(ip6->ip6_plen));
	return (PF_PASS);

fail:
	REASON_SET(reason, PFRES_MEMORY);
	/* PF_DROP requires a valid mbuf *m0 in pf_test6(), will free later */
	return (PF_DROP);
}

int
pf_refragment6(struct mbuf **m0, struct m_tag *mtag, int dir)
{
	struct mbuf		*m = *m0, *t;
	struct pf_fragment_tag	*ftag = (struct pf_fragment_tag *)(mtag + 1);
	u_int32_t		 mtu;
	u_int16_t		 hdrlen, extoff, maxlen;
	u_int8_t		 proto;
	int			 error, action;

	hdrlen = ftag->ft_hdrlen;
	extoff = ftag->ft_extoff;
	maxlen = ftag->ft_maxlen;
	m_tag_delete(m, mtag);
	mtag = NULL;
	ftag = NULL;

	/* Checksum must be calculated for the whole packet */
	in6_proto_cksum_out(m, NULL);

	if (extoff) {
		int off;

		/* Use protocol from next field of last extension header */
		if ((m = m_getptr(m, extoff + offsetof(struct ip6_ext,
		    ip6e_nxt), &off)) == NULL)
			panic("pf_refragment6: short mbuf chain");
		proto = *(mtod(m, caddr_t) + off);
		*(mtod(m, caddr_t) + off) = IPPROTO_FRAGMENT;
		m = *m0;
	} else {
		struct ip6_hdr *hdr;

		hdr = mtod(m, struct ip6_hdr *);
		proto = hdr->ip6_nxt;
		hdr->ip6_nxt = IPPROTO_FRAGMENT;
	}

	/*
	 * Maxlen may be less than 8 iff there was only a single
	 * fragment.  As it was fragmented before, add a fragment
	 * header also for a single fragment.  If total or maxlen
	 * is less than 8, ip6_fragment() will return EMSGSIZE and
	 * we drop the packet.
	 */
	mtu = hdrlen + sizeof(struct ip6_frag) + maxlen;
	error = ip6_fragment(m, hdrlen, proto, mtu);

	m = (*m0)->m_nextpkt;
	(*m0)->m_nextpkt = NULL;
	if (error == 0) {
		/* The first mbuf contains the unfragmented packet */
		m_freem(*m0);
		*m0 = NULL;
		action = PF_PASS;
	} else {
		/* Drop expects an mbuf to free */
		DPFPRINTF(LOG_NOTICE, "refragment error %d", error);
		action = PF_DROP;
	}
	for (t = m; m; m = t) {
		t = m->m_nextpkt;
		m->m_nextpkt = NULL;
		m->m_pkthdr.pf.flags |= PF_TAG_REFRAGMENTED;
		if (error == 0)
			ip6_forward(m, 0);
		else
			m_freem(m);
	}

	return (action);
}
#endif /* INET6 */

int
pf_normalize_ip(struct pf_pdesc *pd, u_short *reason)
{
	struct ip	*h = mtod(pd->m, struct ip *);
	u_int16_t	 fragoff = (ntohs(h->ip_off) & IP_OFFMASK) << 3;
	u_int16_t	 mff = (ntohs(h->ip_off) & IP_MF);

	if (!fragoff && !mff)
		goto no_fragment;

	/* Clear IP_DF if we're in no-df mode */
	if (pf_status.reass & PF_REASS_NODF && h->ip_off & htons(IP_DF))
		h->ip_off &= htons(~IP_DF);

	/* We're dealing with a fragment now. Don't allow fragments
	 * with IP_DF to enter the cache. If the flag was cleared by
	 * no-df above, fine. Otherwise drop it.
	 */
	if (h->ip_off & htons(IP_DF)) {
		DPFPRINTF(LOG_NOTICE, "bad fragment: IP_DF");
		REASON_SET(reason, PFRES_FRAG);
		return (PF_DROP);
	}

	if (!pf_status.reass)
		return (PF_PASS);	/* no reassembly */

	/* Returns PF_DROP or m is NULL or completely reassembled mbuf */
	if (pf_reassemble(&pd->m, pd->dir, reason) != PF_PASS)
		return (PF_DROP);
	if (pd->m == NULL)
		return (PF_PASS);  /* packet has been reassembled, no error */

	h = mtod(pd->m, struct ip *);

no_fragment:
	/* At this point, only IP_DF is allowed in ip_off */
	if (h->ip_off & ~htons(IP_DF))
		h->ip_off &= htons(IP_DF);

	return (PF_PASS);
}

#ifdef INET6
int
pf_normalize_ip6(struct pf_pdesc *pd, u_short *reason)
{
	struct ip6_frag		 frag;

	if (pd->fragoff == 0)
		goto no_fragment;

	if (!pf_pull_hdr(pd->m, pd->fragoff, &frag, sizeof(frag), NULL, reason,
	    AF_INET6))
		return (PF_DROP);

	if (!pf_status.reass)
		return (PF_PASS);	/* no reassembly */

	/* Returns PF_DROP or m is NULL or completely reassembled mbuf */
	if (pf_reassemble6(&pd->m, &frag, pd->fragoff + sizeof(frag),
	    pd->extoff, pd->dir, reason) != PF_PASS)
		return (PF_DROP);
	if (pd->m == NULL)
		return (PF_PASS);  /* packet has been reassembled, no error */

no_fragment:
	return (PF_PASS);
}
#endif /* INET6 */

int
pf_normalize_tcp(struct pf_pdesc *pd)
{
	struct tcphdr	*th = pd->hdr.tcp;
	u_short		 reason;
	u_int8_t	 flags;
	u_int		 rewrite = 0;

	if (pd->csum_status == PF_CSUM_UNKNOWN)
		pf_check_proto_cksum(pd, pd->off, pd->tot_len - pd->off,
		    pd->proto, pd->af);

	flags = th->th_flags;
	if (flags & TH_SYN) {
		/* Illegal packet */
		if (flags & TH_RST)
			goto tcp_drop;

		if (flags & TH_FIN)	/* XXX why clear instead of drop? */
			flags &= ~TH_FIN;
	} else {
		/* Illegal packet */
		if (!(flags & (TH_ACK|TH_RST)))
			goto tcp_drop;
	}

	if (!(flags & TH_ACK)) {
		/* These flags are only valid if ACK is set */
		if (flags & (TH_FIN|TH_PUSH|TH_URG))
			goto tcp_drop;
	}

	/* If flags changed, or reserved data set, then adjust */
	if (flags != th->th_flags || th->th_x2 != 0) {
		th->th_flags = flags;
		th->th_x2 = 0;
		rewrite = 1;
	}

	/* Remove urgent pointer, if TH_URG is not set */
	if (!(flags & TH_URG) && th->th_urp) {
		th->th_urp = 0;
		rewrite = 1;
	}

	/* copy back packet headers if we sanitized */
	if (rewrite) {
		pf_cksum(pd, pd->m);
		m_copyback(pd->m, pd->off, sizeof(*th), th, M_NOWAIT);
	}

	return (PF_PASS);

tcp_drop:
	REASON_SET(&reason, PFRES_NORM);
	return (PF_DROP);
}

int
pf_normalize_tcp_init(struct pf_pdesc *pd, struct pf_state_peer *src,
    struct pf_state_peer *dst)
{
	struct tcphdr	*th = pd->hdr.tcp;
	u_int32_t	 tsval, tsecr;
	u_int8_t	 hdr[60];
	u_int8_t	*opt;

	KASSERT(src->scrub == NULL);

	src->scrub = pool_get(&pf_state_scrub_pl, PR_NOWAIT);
	if (src->scrub == NULL)
		return (1);
	bzero(src->scrub, sizeof(*src->scrub));

	switch (pd->af) {
	case AF_INET: {
		struct ip *h = mtod(pd->m, struct ip *);
		src->scrub->pfss_ttl = h->ip_ttl;
		break;
	}
#ifdef INET6
	case AF_INET6: {
		struct ip6_hdr *h = mtod(pd->m, struct ip6_hdr *);
		src->scrub->pfss_ttl = h->ip6_hlim;
		break;
	}
#endif /* INET6 */
	}

	/*
	 * All normalizations below are only begun if we see the start of
	 * the connections.  They must all set an enabled bit in pfss_flags
	 */
	if ((th->th_flags & TH_SYN) == 0)
		return (0);

	if (th->th_off > (sizeof(struct tcphdr) >> 2) && src->scrub &&
	    pf_pull_hdr(pd->m, pd->off, hdr, th->th_off << 2, NULL, NULL,
	    pd->af)) {
		/* Diddle with TCP options */
		int	hlen;

		opt = hdr + sizeof(struct tcphdr);
		hlen = (th->th_off << 2) - sizeof(struct tcphdr);
		while (hlen >= TCPOLEN_TIMESTAMP) {
			switch (*opt) {
			case TCPOPT_EOL:	/* FALLTHROUGH */
			case TCPOPT_NOP:
				opt++;
				hlen--;
				break;
			case TCPOPT_TIMESTAMP:
				if (opt[1] >= TCPOLEN_TIMESTAMP) {
					src->scrub->pfss_flags |=
					    PFSS_TIMESTAMP;
					src->scrub->pfss_ts_mod =
					    htonl(arc4random());

					/* note PFSS_PAWS not set yet */
					memcpy(&tsval, &opt[2],
					    sizeof(u_int32_t));
					memcpy(&tsecr, &opt[6],
					    sizeof(u_int32_t));
					src->scrub->pfss_tsval0 = ntohl(tsval);
					src->scrub->pfss_tsval = ntohl(tsval);
					src->scrub->pfss_tsecr = ntohl(tsecr);
					getmicrouptime(&src->scrub->pfss_last);
				}
				/* FALLTHROUGH */
			default:
				hlen -= MAX(opt[1], 2);
				opt += MAX(opt[1], 2);
				break;
			}
		}
	}

	return (0);
}

void
pf_normalize_tcp_cleanup(struct pf_state *state)
{
	if (state->src.scrub)
		pool_put(&pf_state_scrub_pl, state->src.scrub);
	if (state->dst.scrub)
		pool_put(&pf_state_scrub_pl, state->dst.scrub);

	/* Someday... flush the TCP segment reassembly descriptors. */
}

int
pf_normalize_tcp_stateful(struct pf_pdesc *pd, u_short *reason,
    struct pf_state *state, struct pf_state_peer *src,
    struct pf_state_peer *dst, int *writeback)
{
	struct tcphdr	*th = pd->hdr.tcp;
	struct timeval	 uptime;
	u_int32_t	 tsval, tsecr;
	u_int		 tsval_from_last;
	u_int8_t	 hdr[60];
	u_int8_t	*opt;
	int		 copyback = 0;
	int		 got_ts = 0;

	KASSERT(src->scrub || dst->scrub);

	/*
	 * Enforce the minimum TTL seen for this connection.  Negate a common
	 * technique to evade an intrusion detection system and confuse
	 * firewall state code.
	 */
	switch (pd->af) {
	case AF_INET:
		if (src->scrub) {
			struct ip *h = mtod(pd->m, struct ip *);
			if (h->ip_ttl > src->scrub->pfss_ttl)
				src->scrub->pfss_ttl = h->ip_ttl;
			h->ip_ttl = src->scrub->pfss_ttl;
		}
		break;
#ifdef INET6
	case AF_INET6:
		if (src->scrub) {
			struct ip6_hdr *h = mtod(pd->m, struct ip6_hdr *);
			if (h->ip6_hlim > src->scrub->pfss_ttl)
				src->scrub->pfss_ttl = h->ip6_hlim;
			h->ip6_hlim = src->scrub->pfss_ttl;
		}
		break;
#endif /* INET6 */
	}

	if (th->th_off > (sizeof(struct tcphdr) >> 2) &&
	    ((src->scrub && (src->scrub->pfss_flags & PFSS_TIMESTAMP)) ||
	    (dst->scrub && (dst->scrub->pfss_flags & PFSS_TIMESTAMP))) &&
	    pf_pull_hdr(pd->m, pd->off, hdr, th->th_off << 2, NULL, NULL,
	    pd->af)) {
		/* Diddle with TCP options */
		int hlen;
		opt = hdr + sizeof(struct tcphdr);
		hlen = (th->th_off << 2) - sizeof(struct tcphdr);
		while (hlen >= TCPOLEN_TIMESTAMP) {
			switch (*opt) {
			case TCPOPT_EOL:	/* FALLTHROUGH */
			case TCPOPT_NOP:
				opt++;
				hlen--;
				break;
			case TCPOPT_TIMESTAMP:
				/* Modulate the timestamps.  Can be used for
				 * NAT detection, OS uptime determination or
				 * reboot detection.
				 */

				if (got_ts) {
					/* Huh?  Multiple timestamps!? */
					if (pf_status.debug >= LOG_NOTICE) {
						log(LOG_NOTICE,
						    "pf: %s: multiple TS??",
						    __func__);
						pf_print_state(state);
						addlog("\n");
					}
					REASON_SET(reason, PFRES_TS);
					return (PF_DROP);
				}
				if (opt[1] >= TCPOLEN_TIMESTAMP) {
					memcpy(&tsval, &opt[2],
					    sizeof(u_int32_t));
					if (tsval && src->scrub &&
					    (src->scrub->pfss_flags &
					    PFSS_TIMESTAMP)) {
						tsval = ntohl(tsval);
						pf_change_a(pd, &opt[2],
						    htonl(tsval +
						    src->scrub->pfss_ts_mod));
						copyback = 1;
					}

					/* Modulate TS reply iff valid (!0) */
					memcpy(&tsecr, &opt[6],
					    sizeof(u_int32_t));
					if (tsecr && dst->scrub &&
					    (dst->scrub->pfss_flags &
					    PFSS_TIMESTAMP)) {
						tsecr = ntohl(tsecr)
						    - dst->scrub->pfss_ts_mod;
						pf_change_a(pd, &opt[6],
						    htonl(tsecr));
						copyback = 1;
					}
					got_ts = 1;
				}
				/* FALLTHROUGH */
			default:
				hlen -= MAX(opt[1], 2);
				opt += MAX(opt[1], 2);
				break;
			}
		}
		if (copyback) {
			/* Copyback the options, caller copys back header */
			*writeback = 1;
			m_copyback(pd->m, pd->off + sizeof(struct tcphdr),
			    (th->th_off << 2) - sizeof(struct tcphdr), hdr +
			    sizeof(struct tcphdr), M_NOWAIT);
		}
	}


	/*
	 * Must invalidate PAWS checks on connections idle for too long.
	 * The fastest allowed timestamp clock is 1ms.  That turns out to
	 * be about 24 days before it wraps.  XXX Right now our lowerbound
	 * TS echo check only works for the first 12 days of a connection
	 * when the TS has exhausted half its 32bit space
	 */
#define TS_MAX_IDLE	(24*24*60*60)
#define TS_MAX_CONN	(12*24*60*60)	/* XXX remove when better tsecr check */

	getmicrouptime(&uptime);
	if (src->scrub && (src->scrub->pfss_flags & PFSS_PAWS) &&
	    (uptime.tv_sec - src->scrub->pfss_last.tv_sec > TS_MAX_IDLE ||
	    time_uptime - state->creation > TS_MAX_CONN))  {
		if (pf_status.debug >= LOG_NOTICE) {
			log(LOG_NOTICE, "pf: src idled out of PAWS ");
			pf_print_state(state);
			addlog("\n");
		}
		src->scrub->pfss_flags =
		    (src->scrub->pfss_flags & ~PFSS_PAWS) | PFSS_PAWS_IDLED;
	}
	if (dst->scrub && (dst->scrub->pfss_flags & PFSS_PAWS) &&
	    uptime.tv_sec - dst->scrub->pfss_last.tv_sec > TS_MAX_IDLE) {
		if (pf_status.debug >= LOG_NOTICE) {
			log(LOG_NOTICE, "pf: dst idled out of PAWS ");
			pf_print_state(state);
			addlog("\n");
		}
		dst->scrub->pfss_flags =
		    (dst->scrub->pfss_flags & ~PFSS_PAWS) | PFSS_PAWS_IDLED;
	}

	if (got_ts && src->scrub && dst->scrub &&
	    (src->scrub->pfss_flags & PFSS_PAWS) &&
	    (dst->scrub->pfss_flags & PFSS_PAWS)) {
		/* Validate that the timestamps are "in-window".
		 * RFC1323 describes TCP Timestamp options that allow
		 * measurement of RTT (round trip time) and PAWS
		 * (protection against wrapped sequence numbers).  PAWS
		 * gives us a set of rules for rejecting packets on
		 * long fat pipes (packets that were somehow delayed
		 * in transit longer than the time it took to send the
		 * full TCP sequence space of 4Gb).  We can use these
		 * rules and infer a few others that will let us treat
		 * the 32bit timestamp and the 32bit echoed timestamp
		 * as sequence numbers to prevent a blind attacker from
		 * inserting packets into a connection.
		 *
		 * RFC1323 tells us:
		 *  - The timestamp on this packet must be greater than
		 *    or equal to the last value echoed by the other
		 *    endpoint.  The RFC says those will be discarded
		 *    since it is a dup that has already been acked.
		 *    This gives us a lowerbound on the timestamp.
		 *        timestamp >= other last echoed timestamp
		 *  - The timestamp will be less than or equal to
		 *    the last timestamp plus the time between the
		 *    last packet and now.  The RFC defines the max
		 *    clock rate as 1ms.  We will allow clocks to be
		 *    up to 10% fast and will allow a total difference
		 *    or 30 seconds due to a route change.  And this
		 *    gives us an upperbound on the timestamp.
		 *        timestamp <= last timestamp + max ticks
		 *    We have to be careful here.  Windows will send an
		 *    initial timestamp of zero and then initialize it
		 *    to a random value after the 3whs; presumably to
		 *    avoid a DoS by having to call an expensive RNG
		 *    during a SYN flood.  Proof MS has at least one
		 *    good security geek.
		 *
		 *  - The TCP timestamp option must also echo the other
		 *    endpoints timestamp.  The timestamp echoed is the
		 *    one carried on the earliest unacknowledged segment
		 *    on the left edge of the sequence window.  The RFC
		 *    states that the host will reject any echoed
		 *    timestamps that were larger than any ever sent.
		 *    This gives us an upperbound on the TS echo.
		 *        tescr <= largest_tsval
		 *  - The lowerbound on the TS echo is a little more
		 *    tricky to determine.  The other endpoint's echoed
		 *    values will not decrease.  But there may be
		 *    network conditions that re-order packets and
		 *    cause our view of them to decrease.  For now the
		 *    only lowerbound we can safely determine is that
		 *    the TS echo will never be less than the original
		 *    TS.  XXX There is probably a better lowerbound.
		 *    Remove TS_MAX_CONN with better lowerbound check.
		 *        tescr >= other original TS
		 *
		 * It is also important to note that the fastest
		 * timestamp clock of 1ms will wrap its 32bit space in
		 * 24 days.  So we just disable TS checking after 24
		 * days of idle time.  We actually must use a 12d
		 * connection limit until we can come up with a better
		 * lowerbound to the TS echo check.
		 */
		struct timeval	delta_ts;
		int		ts_fudge;

		/*
		 * PFTM_TS_DIFF is how many seconds of leeway to allow
		 * a host's timestamp.  This can happen if the previous
		 * packet got delayed in transit for much longer than
		 * this packet.
		 */
		if ((ts_fudge = state->rule.ptr->timeout[PFTM_TS_DIFF]) == 0)
			ts_fudge = pf_default_rule.timeout[PFTM_TS_DIFF];

		/* Calculate max ticks since the last timestamp */
#define TS_MAXFREQ	1100		/* RFC max TS freq of 1Khz + 10% skew */
#define TS_MICROSECS	1000000		/* microseconds per second */
		timersub(&uptime, &src->scrub->pfss_last, &delta_ts);
		tsval_from_last = (delta_ts.tv_sec + ts_fudge) * TS_MAXFREQ;
		tsval_from_last += delta_ts.tv_usec / (TS_MICROSECS/TS_MAXFREQ);

		if ((src->state >= TCPS_ESTABLISHED &&
		    dst->state >= TCPS_ESTABLISHED) &&
		    (SEQ_LT(tsval, dst->scrub->pfss_tsecr) ||
		    SEQ_GT(tsval, src->scrub->pfss_tsval + tsval_from_last) ||
		    (tsecr && (SEQ_GT(tsecr, dst->scrub->pfss_tsval) ||
		    SEQ_LT(tsecr, dst->scrub->pfss_tsval0))))) {
			/* Bad RFC1323 implementation or an insertion attack.
			 *
			 * - Solaris 2.6 and 2.7 are known to send another ACK
			 *   after the FIN,FIN|ACK,ACK closing that carries
			 *   an old timestamp.
			 */

			DPFPRINTF(LOG_NOTICE, "Timestamp failed %c%c%c%c",
			    SEQ_LT(tsval, dst->scrub->pfss_tsecr) ? '0' : ' ',
			    SEQ_GT(tsval, src->scrub->pfss_tsval +
			    tsval_from_last) ? '1' : ' ',
			    SEQ_GT(tsecr, dst->scrub->pfss_tsval) ? '2' : ' ',
			    SEQ_LT(tsecr, dst->scrub->pfss_tsval0)? '3' : ' ');
			DPFPRINTF(LOG_NOTICE, " tsval: %u  tsecr: %u  "
			    "+ticks: %u  idle: %llu.%06lus", tsval, tsecr,
			    tsval_from_last, (long long)delta_ts.tv_sec,
			    delta_ts.tv_usec);
			DPFPRINTF(LOG_NOTICE, " src->tsval: %u  tsecr: %u",
			    src->scrub->pfss_tsval, src->scrub->pfss_tsecr);
			DPFPRINTF(LOG_NOTICE, " dst->tsval: %u  tsecr: %u  "
			    "tsval0: %u", dst->scrub->pfss_tsval,
			    dst->scrub->pfss_tsecr, dst->scrub->pfss_tsval0);
			if (pf_status.debug >= LOG_NOTICE) {
				log(LOG_NOTICE, "pf: ");
				pf_print_state(state);
				pf_print_flags(th->th_flags);
				addlog("\n");
			}
			REASON_SET(reason, PFRES_TS);
			return (PF_DROP);
		}
		/* XXX I'd really like to require tsecr but it's optional */
	} else if (!got_ts && (th->th_flags & TH_RST) == 0 &&
	    ((src->state == TCPS_ESTABLISHED && dst->state == TCPS_ESTABLISHED)
	    || pd->p_len > 0 || (th->th_flags & TH_SYN)) &&
	    src->scrub && dst->scrub &&
	    (src->scrub->pfss_flags & PFSS_PAWS) &&
	    (dst->scrub->pfss_flags & PFSS_PAWS)) {
		/* Didn't send a timestamp.  Timestamps aren't really useful
		 * when:
		 *  - connection opening or closing (often not even sent).
		 *    but we must not let an attacker to put a FIN on a
		 *    data packet to sneak it through our ESTABLISHED check.
		 *  - on a TCP reset.  RFC suggests not even looking at TS.
		 *  - on an empty ACK.  The TS will not be echoed so it will
		 *    probably not help keep the RTT calculation in sync and
		 *    there isn't as much danger when the sequence numbers
		 *    got wrapped.  So some stacks don't include TS on empty
		 *    ACKs :-(
		 *
		 * To minimize the disruption to mostly RFC1323 conformant
		 * stacks, we will only require timestamps on data packets.
		 *
		 * And what do ya know, we cannot require timestamps on data
		 * packets.  There appear to be devices that do legitimate
		 * TCP connection hijacking.  There are HTTP devices that allow
		 * a 3whs (with timestamps) and then buffer the HTTP request.
		 * If the intermediate device has the HTTP response cache, it
		 * will spoof the response but not bother timestamping its
		 * packets.  So we can look for the presence of a timestamp in
		 * the first data packet and if there, require it in all future
		 * packets.
		 */

		if (pd->p_len > 0 && (src->scrub->pfss_flags & PFSS_DATA_TS)) {
			/*
			 * Hey!  Someone tried to sneak a packet in.  Or the
			 * stack changed its RFC1323 behavior?!?!
			 */
			if (pf_status.debug >= LOG_NOTICE) {
				log(LOG_NOTICE,
				    "pf: did not receive expected RFC1323 "
				    "timestamp");
				pf_print_state(state);
				pf_print_flags(th->th_flags);
				addlog("\n");
			}
			REASON_SET(reason, PFRES_TS);
			return (PF_DROP);
		}
	}

	/*
	 * We will note if a host sends his data packets with or without
	 * timestamps.  And require all data packets to contain a timestamp
	 * if the first does.  PAWS implicitly requires that all data packets be
	 * timestamped.  But I think there are middle-man devices that hijack
	 * TCP streams immediately after the 3whs and don't timestamp their
	 * packets (seen in a WWW accelerator or cache).
	 */
	if (pd->p_len > 0 && src->scrub && (src->scrub->pfss_flags &
	    (PFSS_TIMESTAMP|PFSS_DATA_TS|PFSS_DATA_NOTS)) == PFSS_TIMESTAMP) {
		if (got_ts)
			src->scrub->pfss_flags |= PFSS_DATA_TS;
		else {
			src->scrub->pfss_flags |= PFSS_DATA_NOTS;
			if (pf_status.debug >= LOG_NOTICE && dst->scrub &&
			    (dst->scrub->pfss_flags & PFSS_TIMESTAMP)) {
				/* Don't warn if other host rejected RFC1323 */
				log(LOG_NOTICE,
				    "pf: broken RFC1323 stack did not "
				    "timestamp data packet. Disabled PAWS "
				    "security.");
				pf_print_state(state);
				pf_print_flags(th->th_flags);
				addlog("\n");
			}
		}
	}

	/*
	 * Update PAWS values
	 */
	if (got_ts && src->scrub && PFSS_TIMESTAMP == (src->scrub->pfss_flags &
	    (PFSS_PAWS_IDLED|PFSS_TIMESTAMP))) {
		getmicrouptime(&src->scrub->pfss_last);
		if (SEQ_GEQ(tsval, src->scrub->pfss_tsval) ||
		    (src->scrub->pfss_flags & PFSS_PAWS) == 0)
			src->scrub->pfss_tsval = tsval;

		if (tsecr) {
			if (SEQ_GEQ(tsecr, src->scrub->pfss_tsecr) ||
			    (src->scrub->pfss_flags & PFSS_PAWS) == 0)
				src->scrub->pfss_tsecr = tsecr;

			if ((src->scrub->pfss_flags & PFSS_PAWS) == 0 &&
			    (SEQ_LT(tsval, src->scrub->pfss_tsval0) ||
			    src->scrub->pfss_tsval0 == 0)) {
				/* tsval0 MUST be the lowest timestamp */
				src->scrub->pfss_tsval0 = tsval;
			}

			/* Only fully initialized after a TS gets echoed */
			if ((src->scrub->pfss_flags & PFSS_PAWS) == 0)
				src->scrub->pfss_flags |= PFSS_PAWS;
		}
	}

	/* I have a dream....  TCP segment reassembly.... */
	return (0);
}

int
pf_normalize_mss(struct pf_pdesc *pd, u_int16_t maxmss)
{
	struct tcphdr	*th = pd->hdr.tcp;
	u_int16_t	 mss;
	int		 thoff;
	int		 opt, cnt, optlen = 0;
	u_char		 opts[MAX_TCPOPTLEN];
	u_char		*optp = opts;

	if (pd->csum_status == PF_CSUM_UNKNOWN)
		pf_check_proto_cksum(pd, pd->off, pd->tot_len - pd->off,
		    pd->proto, pd->af);

	thoff = th->th_off << 2;
	cnt = thoff - sizeof(struct tcphdr);

	if (cnt <= 0 || cnt > MAX_TCPOPTLEN || !pf_pull_hdr(pd->m,
	    pd->off + sizeof(*th), opts, cnt, NULL, NULL, pd->af))
		return (0);

	for (; cnt > 0; cnt -= optlen, optp += optlen) {
		opt = optp[0];
		if (opt == TCPOPT_EOL)
			break;
		if (opt == TCPOPT_NOP)
			optlen = 1;
		else {
			if (cnt < 2)
				break;
			optlen = optp[1];
			if (optlen < 2 || optlen > cnt)
				break;
		}
		if (opt == TCPOPT_MAXSEG) {
			memcpy(&mss, (optp + 2), 2);
			if (ntohs(mss) > maxmss) {
				mss = htons(maxmss);
				m_copyback(pd->m,
				    pd->off + sizeof(*th) + optp + 2 - opts,
				    2, &mss, M_NOWAIT);
				pf_cksum(pd, pd->m);
				m_copyback(pd->m, pd->off, sizeof(*th), th,
				    M_NOWAIT);
			}
		}
	}

	return (0);
}

void
pf_scrub(struct mbuf *m, u_int16_t flags, sa_family_t af, u_int8_t min_ttl,
    u_int8_t tos)
{
	struct ip		*h = mtod(m, struct ip *);
#ifdef INET6
	struct ip6_hdr		*h6 = mtod(m, struct ip6_hdr *);
#endif

	/* Clear IP_DF if no-df was requested */
	if (flags & PFSTATE_NODF && af == AF_INET && h->ip_off & htons(IP_DF))
		h->ip_off &= htons(~IP_DF);

	/* Enforce a minimum ttl, may cause endless packet loops */
	if (min_ttl && af == AF_INET && h->ip_ttl < min_ttl)
		h->ip_ttl = min_ttl;
#ifdef INET6
	if (min_ttl && af == AF_INET6 && h6->ip6_hlim < min_ttl)
		h6->ip6_hlim = min_ttl;
#endif

	/* Enforce tos */
	if (flags & PFSTATE_SETTOS) {
		if (af == AF_INET)
			h->ip_tos = tos | (h->ip_tos & IPTOS_ECN_MASK);
#ifdef INET6
		if (af == AF_INET6) {
			/* drugs are unable to explain such idiocy */
			h6->ip6_flow &= ~htonl(0x0fc00000);
			h6->ip6_flow |= htonl(((u_int32_t)tos) << 20);
		}
#endif
	}

	/* random-id, but not for fragments */
	if (flags & PFSTATE_RANDOMID && af == AF_INET &&
	    !(h->ip_off & ~htons(IP_DF)))
		h->ip_id = htons(ip_randomid());
}

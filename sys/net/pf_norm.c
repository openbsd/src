/*	$OpenBSD: pf_norm.c,v 1.38 2002/10/29 19:51:04 mickey Exp $ */

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

struct pf_frent {
	LIST_ENTRY(pf_frent) fr_next;
	struct ip *fr_ip;
	struct mbuf *fr_m;
};

struct pf_frcache {
	LIST_ENTRY(pf_frcache) fr_next;
	uint16_t	fr_off;
	uint16_t	fr_end;
};

#define PFFRAG_SEENLAST	0x0001		/* Seen the last fragment for this */
#define PFFRAG_NOBUFFER	0x0002		/* Non-buffering fragment cache */
#define PFFRAG_DROP	0x0004		/* Drop all fragments */
#define BUFFER_FRAGMENTS(fr)	(!((fr)->fr_flags & PFFRAG_NOBUFFER))

struct pf_fragment {
	RB_ENTRY(pf_fragment) fr_entry;
	TAILQ_ENTRY(pf_fragment) frag_next;
	struct in_addr	fr_src;
	struct in_addr	fr_dst;
	u_int8_t	fr_p;		/* protocol of this fragment */
	u_int8_t	fr_flags;	/* status flags */
	u_int16_t	fr_id;		/* fragment id for reassemble */
	u_int16_t	fr_max;		/* fragment data max */
	u_int32_t	fr_timeout;
#define fr_queue	fr_u.fru_queue
#define fr_cache	fr_u.fru_cache
	union {
		LIST_HEAD(pf_fragq, pf_frent) fru_queue;	/* buffering */
		LIST_HEAD(pf_cacheq, pf_frcache) fru_cache;	/* non-buf */
	} fr_u;
};

TAILQ_HEAD(pf_fragqueue, pf_fragment)	pf_fragqueue;
TAILQ_HEAD(pf_cachequeue, pf_fragment)	pf_cachequeue;

static __inline int	 pf_frag_compare(struct pf_fragment *,
			    struct pf_fragment *);
RB_HEAD(pf_frag_tree, pf_fragment)	pf_frag_tree, pf_cache_tree;
RB_PROTOTYPE(pf_frag_tree, pf_fragment, fr_entry, pf_frag_compare);
RB_GENERATE(pf_frag_tree, pf_fragment, fr_entry, pf_frag_compare);

/* Private prototypes */
void			 pf_ip2key(struct pf_fragment *, struct ip *);
void			 pf_remove_fragment(struct pf_fragment *);
void			 pf_flush_fragments(void);
void			 pf_free_fragment(struct pf_fragment *);
struct pf_fragment	*pf_find_fragment(struct ip *, struct pf_frag_tree *);
struct mbuf		*pf_reassemble(struct mbuf **, struct pf_fragment *,
			    struct pf_frent *, int);
struct mbuf		*pf_fragcache(struct mbuf **, struct ip*,
			    struct pf_fragment *, int, int, int *);
u_int16_t		 pf_cksum_fixup(u_int16_t, u_int16_t, u_int16_t);
int			 pf_normalize_tcp(int, struct ifnet *, struct mbuf *,
			    int, int, void *, struct pf_pdesc *);
int			 pf_normalize_tcpopt(struct pf_rule *, struct mbuf *,
			    struct tcphdr *, int);

#define	DPFPRINTF(x)	if (pf_status.debug >= PF_DEBUG_MISC) \
			    { printf("%s: ", __func__); printf x ;}

/* Globals */
struct pool		 pf_frent_pl, pf_frag_pl, pf_cache_pl, pf_cent_pl;
int			 pf_nfrents, pf_ncache;
extern int		 pftm_frag;	/* Fragment expire timeout */

void
pf_normalize_init(void)
{
	pool_init(&pf_frent_pl, sizeof(struct pf_frent), 0, 0, 0, "pffrent",
	    NULL);
	pool_init(&pf_frag_pl, sizeof(struct pf_fragment), 0, 0, 0, "pffrag",
	    NULL);
	pool_init(&pf_cache_pl, sizeof(struct pf_fragment), 0, 0, 0,
	    "pffrcache", NULL);
	pool_init(&pf_cent_pl, sizeof(struct pf_frcache), 0, 0, 0, "pffrcent",
	    NULL);

	pool_sethiwat(&pf_frag_pl, PFFRAG_FRAG_HIWAT);
	pool_sethardlimit(&pf_frent_pl, PFFRAG_FRENT_HIWAT, NULL, 0);
	pool_sethardlimit(&pf_cache_pl, PFFRAG_FRCACHE_HIWAT, NULL, 0);
	pool_sethardlimit(&pf_cent_pl, PFFRAG_FRCENT_HIWAT, NULL, 0);

	TAILQ_INIT(&pf_fragqueue);
	TAILQ_INIT(&pf_cachequeue);
}

static __inline int
pf_frag_compare(struct pf_fragment *a, struct pf_fragment *b)
{
	int diff;

	if ((diff = a->fr_id - b->fr_id))
		return (diff);
	else if ((diff = a->fr_p - b->fr_p))
		return (diff);
	else if (a->fr_src.s_addr < b->fr_src.s_addr)
		return (-1);
	else if (a->fr_src.s_addr > b->fr_src.s_addr)
		return (1);
	else if (a->fr_dst.s_addr < b->fr_dst.s_addr)
		return (-1);
	else if (a->fr_dst.s_addr > b->fr_dst.s_addr)
		return (1);
	return (0);
}

void
pf_purge_expired_fragments(void)
{
	struct pf_fragment *frag;
	u_int32_t expire = time.tv_sec - pftm_frag;

	while ((frag = TAILQ_LAST(&pf_fragqueue, pf_fragqueue)) != NULL) {
		KASSERT(BUFFER_FRAGMENTS(frag));
		if (frag->fr_timeout > expire)
			break;

		DPFPRINTF(("expiring %d(%p)\n", frag->fr_id, frag));
		pf_free_fragment(frag);
	}

	while ((frag = TAILQ_LAST(&pf_cachequeue, pf_cachequeue)) != NULL) {
		KASSERT(!BUFFER_FRAGMENTS(frag));
		if (frag->fr_timeout > expire)
			break;

		DPFPRINTF(("expiring %d(%p)\n", frag->fr_id, frag));
		pf_free_fragment(frag);
		KASSERT(TAILQ_EMPTY(&pf_cachequeue) ||
		    TAILQ_LAST(&pf_cachequeue, pf_cachequeue) != frag);
	}
}

/*
 *  Try to flush old fragments to make space for new ones
 */

void
pf_flush_fragments(void)
{
	struct pf_fragment *frag;
	int goal;

	goal = pf_nfrents * 9 / 10;
	DPFPRINTF(("trying to free > %d frents\n",
		   pf_nfrents - goal));
	while (goal < pf_nfrents) {
		frag = TAILQ_LAST(&pf_fragqueue, pf_fragqueue);
		if (frag == NULL)
			break;
		pf_free_fragment(frag);
	}


	goal = pf_ncache * 9 / 10;
	DPFPRINTF(("trying to free > %d cache entries\n",
		   pf_ncache - goal));
	while (goal < pf_ncache) {
		frag = TAILQ_LAST(&pf_cachequeue, pf_cachequeue);
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
	struct pf_frcache *frcache;

	/* Free all fragments */
	if (BUFFER_FRAGMENTS(frag)) {
		for (frent = LIST_FIRST(&frag->fr_queue); frent;
		    frent = LIST_FIRST(&frag->fr_queue)) {
			LIST_REMOVE(frent, fr_next);

			m_freem(frent->fr_m);
			pool_put(&pf_frent_pl, frent);
			pf_nfrents--;
		}
	} else {
		for (frcache = LIST_FIRST(&frag->fr_cache); frcache;
		    frcache = LIST_FIRST(&frag->fr_cache)) {
			LIST_REMOVE(frcache, fr_next);

			KASSERT(LIST_EMPTY(&frag->fr_cache) ||
			    LIST_FIRST(&frag->fr_cache)->fr_off >
			    frcache->fr_end);

			pool_put(&pf_cent_pl, frcache);
			pf_ncache--;
		}
	}

	pf_remove_fragment(frag);
}

void
pf_ip2key(struct pf_fragment *key, struct ip *ip)
{
	key->fr_p = ip->ip_p;
	key->fr_id = ip->ip_id;
	key->fr_src.s_addr = ip->ip_src.s_addr;
	key->fr_dst.s_addr = ip->ip_dst.s_addr;
}

struct pf_fragment *
pf_find_fragment(struct ip *ip, struct pf_frag_tree *tree)
{
	struct pf_fragment key;
	struct pf_fragment *frag;

	pf_ip2key(&key, ip);

	frag = RB_FIND(pf_frag_tree, tree, &key);
	if (frag != NULL) {
		/* XXX Are we sure we want to update the timeout? */
		frag->fr_timeout = time.tv_sec;
		if (BUFFER_FRAGMENTS(frag)) {
			TAILQ_REMOVE(&pf_fragqueue, frag, frag_next);
			TAILQ_INSERT_HEAD(&pf_fragqueue, frag, frag_next);
		} else {
			TAILQ_REMOVE(&pf_cachequeue, frag, frag_next);
			TAILQ_INSERT_HEAD(&pf_cachequeue, frag, frag_next);
		}
	}

	return (frag);
}

/* Removes a fragment from the fragment queue and frees the fragment */

void
pf_remove_fragment(struct pf_fragment *frag)
{
	if (BUFFER_FRAGMENTS(frag)) {
		RB_REMOVE(pf_frag_tree, &pf_frag_tree, frag);
		TAILQ_REMOVE(&pf_fragqueue, frag, frag_next);
		pool_put(&pf_frag_pl, frag);
	} else {
		RB_REMOVE(pf_frag_tree, &pf_cache_tree, frag);
		TAILQ_REMOVE(&pf_cachequeue, frag, frag_next);
		pool_put(&pf_cache_pl, frag);
	}
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

	KASSERT(frag == NULL || BUFFER_FRAGMENTS(frag));

	/* Strip off ip header */
	m->m_data += hlen;
	m->m_len -= hlen;

	/* Create a new reassembly queue for this packet */
	if (frag == NULL) {
		frag = pool_get(&pf_frag_pl, PR_NOWAIT);
		if (frag == NULL) {
			pf_flush_fragments();
			frag = pool_get(&pf_frag_pl, PR_NOWAIT);
			if (frag == NULL)
				goto drop_fragment;
		}

		frag->fr_flags = 0;
		frag->fr_max = 0;
		frag->fr_src = frent->fr_ip->ip_src;
		frag->fr_dst = frent->fr_ip->ip_dst;
		frag->fr_p = frent->fr_ip->ip_p;
		frag->fr_id = frent->fr_ip->ip_id;
		frag->fr_timeout = time.tv_sec;
		LIST_INIT(&frag->fr_queue);

		RB_INSERT(pf_frag_tree, &pf_frag_tree, frag);
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

			DPFPRINTF(("overlap -%d\n", precut));
			/* Enforce 8 byte boundaries */
			off = ip->ip_off += precut;
			ip->ip_len -= precut;
		}
	}

	for (; frea != NULL && ip->ip_len + off > frea->fr_ip->ip_off;
	    frea = next) {
		u_int16_t aftercut;

		aftercut = (ip->ip_len + off) - frea->fr_ip->ip_off;
		DPFPRINTF(("adjust overlap %d\n", aftercut));
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
	/* Update maximum data size */
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
			DPFPRINTF(("missing fragment at %d, next %d, max %d\n",
			    off, next == NULL ? -1 : next->fr_ip->ip_off,
			    frag->fr_max));
			return (NULL);
		}
	}
	DPFPRINTF(("%d < %d?\n", off, frag->fr_max));
	if (off < frag->fr_max)
		return (NULL);

	/* We have all the data */
	frent = LIST_FIRST(&frag->fr_queue);
	KASSERT(frent != NULL);
	if ((frent->fr_ip->ip_hl << 2) + off > IP_MAXPACKET) {
		DPFPRINTF(("drop: too big: %d\n", off));
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

	DPFPRINTF(("complete: %p(%d)\n", m, ip->ip_len));
	return (m);

 drop_fragment:
	/* Oops - fail safe - drop packet */
	pool_put(&pf_frent_pl, frent);
	pf_nfrents--;
	m_freem(m);
	return (NULL);
}

struct mbuf *
pf_fragcache(struct mbuf **m0, struct ip *h, struct pf_fragment *frag, int mff,
    int drop, int *nomem)
{
	struct mbuf *m = *m0;
	struct pf_frcache *frp, *fra, *cur = NULL;
	int ip_len = h->ip_len - (h->ip_hl << 2);
	u_int16_t off = h->ip_off << 3;
	u_int16_t max = ip_len + off;
	int hosed = 0;

	KASSERT(frag == NULL || !BUFFER_FRAGMENTS(frag));

	/* Create a new range queue for this packet */
	if (frag == NULL) {
		frag = pool_get(&pf_cache_pl, PR_NOWAIT);
		if (frag == NULL) {
			pf_flush_fragments();
			frag = pool_get(&pf_cache_pl, PR_NOWAIT);
			if (frag == NULL)
				goto no_mem;
		}

		/* Get an entry for the queue */
		cur = pool_get(&pf_cent_pl, PR_NOWAIT);
		if (cur == NULL) {
			pool_put(&pf_cache_pl, frag);
			goto no_mem;
		}
		pf_ncache++;

		frag->fr_flags = PFFRAG_NOBUFFER;
		frag->fr_max = 0;
		frag->fr_src = h->ip_src;
		frag->fr_dst = h->ip_dst;
		frag->fr_p = h->ip_p;
		frag->fr_id = h->ip_id;
		frag->fr_timeout = time.tv_sec;

		cur->fr_off = off;
		cur->fr_end = max;
		LIST_INIT(&frag->fr_cache);
		LIST_INSERT_HEAD(&frag->fr_cache, cur, fr_next);

		RB_INSERT(pf_frag_tree, &pf_cache_tree, frag);
		TAILQ_INSERT_HEAD(&pf_cachequeue, frag, frag_next);

		DPFPRINTF(("fragcache[%d]: new %d-%d\n", h->ip_id, off, max));

		goto pass;
	}

	/*
	 * Find a fragment after the current one:
	 *  - off contains the real shifted offset.
	 */
	frp = NULL;
	LIST_FOREACH(fra, &frag->fr_cache, fr_next) {
		if (fra->fr_off > off)
			break;
		frp = fra;
	}

	KASSERT(frp != NULL || fra != NULL);

	if (frp != NULL) {
		int precut;

		precut = frp->fr_end - off;
		if (precut >= ip_len) {
			/* Fragment is entirely a duplicate */
			DPFPRINTF(("fragcache[%d]: dead (%d-%d) %d-%d\n",
			    h->ip_id, frp->fr_off, frp->fr_end, off, max));
			goto drop_fragment;
		}
		if (precut == 0) {
			/* They are adjacent.  Fixup cache entry */
			DPFPRINTF(("fragcache[%d]: adjacent (%d-%d) %d-%d\n",
			    h->ip_id, frp->fr_off, frp->fr_end, off, max));
			frp->fr_end = max;
		} else if (precut > 0) {
			/* The first part of this payload overlaps with a
			 * fragment that has already been passed.
			 * Need to trim off the first part of the payload.
			 * But to do so easily, we need to create another
			 * mbuf to throw the original header into.
			 */

			DPFPRINTF(("fragcache[%d]: chop %d (%d-%d) %d-%d\n",
			    h->ip_id, precut, frp->fr_off, frp->fr_end, off,
			    max));

			off += precut;
			max -= precut;
			/* Update the previous frag to encompas this one */
			frp->fr_end = max;

			if (!drop) {
				/* XXX Optimization opportunity
				 * This is a very heavy way to trim the payload.
				 * we could do it much faster by diddling mbuf
				 * internals but that would be even less legible
				 * than this mbuf magic.  For my next trick,
				 * I'll pull a rabbit out of my laptop.
				 */
				*m0 = m_copym2(m, 0, h->ip_hl << 2, M_NOWAIT);
				if (*m0 == NULL)
					goto no_mem;
				KASSERT((*m0)->m_next == NULL);
				m_adj(m, precut + (h->ip_hl << 2));
				m_cat(*m0, m);
				m = *m0;
				if (m->m_flags & M_PKTHDR) {
					int plen = 0;
					struct mbuf *t;
					for (t = m; t; t = t->m_next)
						plen += t->m_len;
					m->m_pkthdr.len = plen;
				}


				h = mtod(m, struct ip *);

				KASSERT((int)m->m_len == h->ip_len - precut);

				h->ip_off += precut >> 3;
				h->ip_len -= precut;
			} else {
				hosed++;
			}
		} else {
			/* There is a gap between fragments */

			DPFPRINTF(("fragcache[%d]: gap %d (%d-%d) %d-%d\n",
			    h->ip_id, -precut, frp->fr_off, frp->fr_end, off,
			    max));

			cur = pool_get(&pf_cent_pl, PR_NOWAIT);
			if (cur == NULL)
				goto no_mem;
			pf_ncache++;

			cur->fr_off = off;
			cur->fr_end = max;
			LIST_INSERT_AFTER(frp, cur, fr_next);
		}
	}

	if (fra != NULL) {
		int aftercut;
		int merge = 0;

		aftercut = max - fra->fr_off;
		if (aftercut == 0) {
			/* Adjacent fragments */
			DPFPRINTF(("fragcache[%d]: adjacent %d-%d (%d-%d)\n",
			    h->ip_id, off, max, fra->fr_off, fra->fr_end));
			fra->fr_off = off;
			merge = 1;
		} else if (aftercut > 0) {
			/* Need to chop off the tail of this fragment */
			DPFPRINTF(("fragcache[%d]: chop %d %d-%d (%d-%d)\n",
			    h->ip_id, aftercut, off, max, fra->fr_off,
			    fra->fr_end));
			fra->fr_off = off;
			max -= aftercut;

			merge = 1;

			if (!drop) {
				m_adj(m, -aftercut);
				if (m->m_flags & M_PKTHDR) {
					int plen = 0;
					struct mbuf *t;
					for (t = m; t; t = t->m_next)
						plen += t->m_len;
					m->m_pkthdr.len = plen;
				}
				h = mtod(m, struct ip *);
				KASSERT((int)m->m_len == h->ip_len - aftercut);
				h->ip_len -= aftercut;
			} else {
				hosed++;
			}
		} else {
			/* There is a gap between fragments */
			DPFPRINTF(("fragcache[%d]: gap %d %d-%d (%d-%d)\n",
			    h->ip_id, -aftercut, off, max, fra->fr_off,
			    fra->fr_end));

			cur = pool_get(&pf_cent_pl, PR_NOWAIT);
			if (cur == NULL)
				goto no_mem;
			pf_ncache++;

			cur->fr_off = off;
			cur->fr_end = max;
			LIST_INSERT_BEFORE(fra, cur, fr_next);
		}


		/* Need to glue together two seperate fragment descriptors */
		if (merge) {
			if (cur && fra->fr_off <= cur->fr_end) {
				/* Need to merge in a previous 'cur' */
				DPFPRINTF(("fragcache[%d]: adjacent(merge %d-%d) %d-%d (%d-%d)\n",
				    h->ip_id, cur->fr_off, cur->fr_end, off,
				    max, fra->fr_off, fra->fr_end));
				fra->fr_off = cur->fr_off;
				LIST_REMOVE(cur, fr_next);
				pool_put(&pf_cent_pl, cur);
				pf_ncache--;
				cur = NULL;

			} else if (frp && fra->fr_off <= frp->fr_end) {
				/* Need to merge in a modified 'frp' */
				KASSERT(cur == NULL);
				DPFPRINTF(("fragcache[%d]: adjacent(merge %d-%d) %d-%d (%d-%d)\n",
				    h->ip_id, frp->fr_off, frp->fr_end, off,
				    max, fra->fr_off, fra->fr_end));
				fra->fr_off = frp->fr_off;
				LIST_REMOVE(frp, fr_next);
				pool_put(&pf_cent_pl, frp);
				pf_ncache--;
				frp = NULL;

			}
		}
	}

	if (hosed) {
		/*
		 * We must keep tracking the overall fragment even when
		 * we're going to drop it anyway so that we know when to
		 * free the overall descriptor.  Thus we drop the frag late.
		 */
		goto drop_fragment;
	}


 pass:
	/* Update maximum data size */
	if (frag->fr_max < max)
		frag->fr_max = max;

	/* This is the last segment */
	if (!mff)
		frag->fr_flags |= PFFRAG_SEENLAST;

	/* Check if we are completely reassembled */
	if ((frag->fr_flags & PFFRAG_SEENLAST) &&
	    LIST_FIRST(&frag->fr_cache)->fr_off == 0 &&
	    LIST_FIRST(&frag->fr_cache)->fr_end == frag->fr_max) {
		/* Remove from fragment queue */
		DPFPRINTF(("fragcache[%d]: done 0-%d\n", h->ip_id,
		    frag->fr_max));
		pf_free_fragment(frag);
	}

	return (m);

 no_mem:
	*nomem = 1;

	/* Still need to pay attention to !IP_MF */
	if (!mff && frag)
		frag->fr_flags |= PFFRAG_SEENLAST;

	m_freem(m);
	return (NULL);

 drop_fragment:

	/* Still need to pay attention to !IP_MF */
	if (!mff && frag)
		frag->fr_flags |= PFFRAG_SEENLAST;

	if (drop) {
		/* This fragment has been deemed bad.  Don't reass */
		if ((frag->fr_flags & PFFRAG_DROP) == 0)
			DPFPRINTF(("fragcache[%d]: dropping overall fragment\n",
			    h->ip_id));
		frag->fr_flags |= PFFRAG_DROP;
	}

	m_freem(m);
	return (NULL);
}

int
pf_normalize_ip(struct mbuf **m0, int dir, struct ifnet *ifp, u_short *reason)
{
	struct mbuf *m = *m0;
	struct pf_rule *r;
	struct pf_frent *frent;
	struct pf_fragment *frag = NULL;
	struct ip *h = mtod(m, struct ip *);
	int mff = (h->ip_off & IP_MF), hlen = h->ip_hl << 2;
	u_int16_t fragoff = (h->ip_off & IP_OFFMASK) << 3;
	u_int16_t max;
	int ip_len;
	int ip_off;

	r = TAILQ_FIRST(pf_rules_active);
	while (r != NULL) {
		if (r->action != PF_SCRUB)
			r = r->skip[PF_SKIP_ACTION];
		else if (r->ifp != NULL && r->ifp != ifp)
			r = r->skip[PF_SKIP_IFP];
		else if (r->direction != dir)
			r = r->skip[PF_SKIP_DIR];
		else if (r->af && r->af != AF_INET)
			r = r->skip[PF_SKIP_AF];
		else if (r->proto && r->proto != h->ip_p)
			r = r->skip[PF_SKIP_PROTO];
		else if (!PF_AZERO(&r->src.mask, AF_INET) &&
		    !PF_MATCHA(r->src.not, &r->src.addr.addr, &r->src.mask,
		    (struct pf_addr *)&h->ip_src.s_addr, AF_INET))
			r = r->skip[PF_SKIP_SRC_ADDR];
		else if (!PF_AZERO(&r->dst.mask, AF_INET) &&
		    !PF_MATCHA(r->dst.not, &r->dst.addr.addr, &r->dst.mask,
		    (struct pf_addr *)&h->ip_dst.s_addr, AF_INET))
			r = r->skip[PF_SKIP_DST_ADDR];
		else
			break;
	}

	if (r == NULL)
		return (PF_PASS);

	/* Check for illegal packets */
	if (hlen < (int)sizeof(struct ip))
		goto drop;

	if (hlen > h->ip_len)
		goto drop;

	/* We will need other tests here */
	if (!fragoff && !mff)
		goto no_fragment;

	/* This can not happen */
	if (h->ip_off & IP_DF) {
		DPFPRINTF(("IP_DF\n"));
		goto bad;
	}

	ip_len = h->ip_len - hlen;
	ip_off = h->ip_off << 3;

	/* All fragments are 8 byte aligned */
	if (mff && (ip_len & 0x7)) {
		DPFPRINTF(("mff and %d\n", ip_len));
		goto bad;
	}

	max = fragoff + ip_len;
	/* Respect maximum length */
	if (max > IP_MAXPACKET) {
		DPFPRINTF(("max packet %d\n", max));
		goto bad;
	}

	if ((r->rule_flag & (PFRULE_FRAGCROP|PFRULE_FRAGDROP)) == 0) {
		/* Fully buffer all of the fragments */

		h->ip_len = ip_len;	/* logic need muddled off/len */
		h->ip_off = ip_off;
		frag = pf_find_fragment(h, &pf_frag_tree);

		/* Check if we saw the last fragment already */
		if (frag != NULL && (frag->fr_flags & PFFRAG_SEENLAST) &&
		    max > frag->fr_max)
			goto bad;

		/* Get an entry for the fragment queue */
		frent = pool_get(&pf_frent_pl, PR_NOWAIT);
		if (frent == NULL) {
			REASON_SET(reason, PFRES_MEMORY);
			return (PF_DROP);
		}
		pf_nfrents++;
		frent->fr_ip = h;
		frent->fr_m = m;

		/* Might return a completely reassembled mbuf, or NULL */
		DPFPRINTF(("reass frag %d @ %d-%d\n", h->ip_id, fragoff, max));
		*m0 = m = pf_reassemble(m0, frag, frent, mff);

		if (m == NULL)
			return (PF_DROP);

		if (frag && (frag->fr_flags & PFFRAG_DROP))
			goto drop;

		h = mtod(m, struct ip *);
	} else {
		/* non-buffering fragment cache (drops or masks overlaps) */
		int nomem = 0;

		if (dir == PF_OUT) {
			if (m_tag_find(m, PACKET_TAG_PF_FRAGCACHE, NULL) !=
			    NULL) {
				/* Already passed the fragment cache in the
				 * input direction.  If we continued, it would
				 * appear to be a dup and would be dropped.
				 */
				goto fragment_pass;
			}
		}

		frag = pf_find_fragment(h, &pf_cache_tree);

		/* Check if we saw the last fragment already */
		if (frag != NULL && (frag->fr_flags & PFFRAG_SEENLAST) &&
		    max > frag->fr_max) {
			if (r->rule_flag & PFRULE_FRAGDROP)
				frag->fr_flags |= PFFRAG_DROP;
			goto bad;
		}

		*m0 = m = pf_fragcache(m0, h, frag, mff,
		    (r->rule_flag & PFRULE_FRAGDROP) ? 1 : 0, &nomem);
		if (m == NULL) {
			if (nomem)
				goto no_mem;
			goto drop;
		}

		if (dir == PF_IN) {
			struct m_tag *mtag;
			mtag = m_tag_get(PACKET_TAG_PF_FRAGCACHE, 0, M_NOWAIT);
			if (mtag == NULL)
				goto no_mem;
			m_tag_prepend(m, mtag);
		}
		if (frag && (frag->fr_flags & PFFRAG_DROP))
			goto drop;
		goto fragment_pass;
	}

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

 fragment_pass:
	if (dir != PF_OUT)
		return (PF_PASS);

	/* Enforce a minimum ttl, may cause endless packet loops */
	if (r->min_ttl && h->ip_ttl < r->min_ttl)
		h->ip_ttl = r->min_ttl;

	return (PF_PASS);

 no_mem:
	REASON_SET(reason, PFRES_MEMORY);
	if (r != NULL && r->log)
		PFLOG_PACKET(ifp, h, m, AF_INET, dir, *reason, r);
	return (PF_DROP);

 drop:
	REASON_SET(reason, PFRES_NORM);
	if (r != NULL && r->log)
		PFLOG_PACKET(ifp, h, m, AF_INET, dir, *reason, r);
	return (PF_DROP);

 bad:
	DPFPRINTF(("dropping bad fragment\n"));

	/* Free assoicated fragments */
	if (frag != NULL)
		pf_free_fragment(frag);

	REASON_SET(reason, PFRES_FRAG);
	if (r != NULL && r->log)
		PFLOG_PACKET(ifp, h, m, AF_INET, dir, *reason, r);

	return (PF_DROP);
}

int
pf_normalize_tcp(int dir, struct ifnet *ifp, struct mbuf *m, int ipoff,
    int off, void *h, struct pf_pdesc *pd)
{
	struct pf_rule *r, *rm = NULL;
	struct tcphdr *th = pd->hdr.tcp;
	int rewrite = 0;
	u_short reason;
	u_int8_t flags;
	sa_family_t af = pd->af;

	r = TAILQ_FIRST(pf_rules_active);
	while (r != NULL) {
		if (r->action != PF_SCRUB)
			r = r->skip[PF_SKIP_ACTION];
		else if (r->ifp != NULL && r->ifp != ifp)
			r = r->skip[PF_SKIP_IFP];
		else if (r->direction != dir)
			r = r->skip[PF_SKIP_DIR];
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF];
		else if (r->proto && r->proto != pd->proto)
			r = r->skip[PF_SKIP_PROTO];
		else if (r->src.noroute && pf_routable(pd->src, af))
			r = TAILQ_NEXT(r, entries);
		else if (!r->src.noroute && !PF_AZERO(&r->src.mask, af) &&
		    !PF_MATCHA(r->src.not, &r->src.addr.addr, &r->src.mask,
			    pd->src, af))
			r = r->skip[PF_SKIP_SRC_ADDR];
		else if (r->src.port_op && !pf_match_port(r->src.port_op,
			    r->src.port[0], r->src.port[1], th->th_sport))
			r = r->skip[PF_SKIP_SRC_PORT];
		else if (r->dst.noroute && pf_routable(pd->dst, af))
			r = TAILQ_NEXT(r, entries);
		else if (!r->dst.noroute && !PF_AZERO(&r->dst.mask, af) &&
		    !PF_MATCHA(r->dst.not, &r->dst.addr.addr, &r->dst.mask,
			    pd->dst, af))
			r = r->skip[PF_SKIP_DST_ADDR];
		else if (r->dst.port_op && !pf_match_port(r->dst.port_op,
			    r->dst.port[0], r->dst.port[1], th->th_dport))
			r = r->skip[PF_SKIP_DST_PORT];
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

	/* Process options */
	if (r->max_mss && pf_normalize_tcpopt(r, m, th, off))
		rewrite = 1;

	/* copy back packet headers if we sanitized */
	if (rewrite)
		m_copyback(m, off, sizeof(*th), (caddr_t)th);

	return (PF_PASS);

 tcp_drop:
	REASON_SET(&reason, PFRES_NORM);
	if (rm != NULL && rm->log)
		PFLOG_PACKET(ifp, h, m, AF_INET, dir, reason, rm);
	return (PF_DROP);
}

int
pf_normalize_tcpopt(struct pf_rule *r, struct mbuf *m, struct tcphdr *th,
    int off)
{
	u_int16_t *mss;
	int thoff;
	int opt, cnt, optlen = 0;
	int rewrite = 0;
	u_char *optp;

	thoff = th->th_off << 2;
	cnt = thoff - sizeof(struct tcphdr);
	optp = mtod(m, caddr_t) + off + sizeof(struct tcphdr);

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
		switch (opt) {
		case TCPOPT_MAXSEG:
			mss = (u_int16_t *)(optp + 2);
			if ((ntohs(*mss)) > r->max_mss) {
				th->th_sum = pf_cksum_fixup(th->th_sum,
				    *mss, htons(r->max_mss));
				*mss = htons(r->max_mss);
				rewrite = 1;
			}
			break;
		default:
			break;
		}
	}

	return (rewrite);
}

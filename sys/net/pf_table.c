/*      $OpenBSD: pf_table.c,v 1.4 2002/12/30 15:39:18 cedric Exp $ */

/*
 * Copyright (c) 2002 Cedric Berger
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip_ipsp.h>
#include <net/pfvar.h>

#include <crypto/sha1.h>

#define ACCEPT_FLAGS(oklist)			\
	do {					\
		if ((flags & ~(oklist)) &	\
		   PFR_FLAG_ALLMASK)		\
			return (EINVAL);	\
	} while(0)

#define	FILLIN_SIN(sin, addr)			\
	do {					\
		(sin).sin_len = sizeof(sin);	\
		(sin).sin_family = AF_INET;	\
		(sin).sin_addr = (addr);	\
	} while (0)

#define	FILLIN_SIN6(sin6, addr)			\
	do {					\
		(sin6).sin6_len = sizeof(sin6);	\
		(sin6).sin6_family = AF_INET6;	\
		(sin6).sin6_addr = (addr);	\
	} while (0)

#define	AF_BITS(af)		(((af)==AF_INET)?32:128)
#define	ADDR_NETWORK(ad)	((ad)->pfra_net < AF_BITS((ad)->pfra_af))
#define	KENTRY_NETWORK(ke)	((ke)->pfrke_net < AF_BITS((ke)->pfrke_af))

struct pfr_walktree {
	enum pfrw_op {
		PFRW_MARK,
		PFRW_SWEEP,
		PFRW_ENQUEUE,
		PFRW_GET_ADDRS,
		PFRW_GET_ASTATS
	}	 pfrw_op;
	union {
		struct pfr_addr		*pfrw1_addr;
		struct pfr_astats	*pfrw1_astats;
		struct pfr_kentryworkq	*pfrw1_workq;
	}	 pfrw_1;
	int	 pfrw_free;
};
#define pfrw_addr	pfrw_1.pfrw1_addr
#define pfrw_astats	pfrw_1.pfrw1_astats
#define pfrw_workq	pfrw_1.pfrw1_workq
#define pfrw_cnt	pfrw_free

#define	PFR_HASH_BUCKETS	1024
#define	PFR_HASH_BUCKET(hash)	((hash).pfrh_int32[1] & (PFR_HASH_BUCKETS-1))

#define senderr(e)	do { rv = (e); goto _bad; } while(0)

struct pool		 pfr_ktable_pl;
struct pool		 pfr_kentry_pl;
struct sockaddr_in	 pfr_sin = { sizeof(pfr_sin), AF_INET };
struct sockaddr_in6	 pfr_sin6 = { sizeof(pfr_sin6), AF_INET6 };

int			 pfr_validate_addr(struct pfr_addr *);
int			 pfr_enqueue_addrs(struct pfr_ktable *,
			    struct pfr_kentryworkq *, int *);
struct pfr_kentry	*pfr_lookup_addr(struct pfr_ktable *,
			    struct pfr_addr *, int);
struct pfr_kentry	*pfr_create_kentry(struct pfr_addr *, long);
void			 pfr_destroy_kentry(struct pfr_kentry *);
void			 pfr_destroy_kentries(struct pfr_kentryworkq *);
int			 pfr_insert_kentries(struct pfr_ktable *,
			    struct pfr_kentryworkq *);
void			 pfr_remove_kentries(struct pfr_ktable *,
			    struct pfr_kentryworkq *);
void			 pfr_clstats_kentries(struct pfr_kentryworkq *, long);
void			 pfr_reset_feedback(struct pfr_addr *, int);
void			 pfr_prepare_network(union sockaddr_union *, int, int);
int			 pfr_route_kentry(struct pfr_ktable *,
			    struct pfr_kentry *);
int			 pfr_unroute_kentry(struct pfr_ktable *,
			    struct pfr_kentry *);
void			 pfr_copyout_addr(struct pfr_addr *,
			    struct pfr_kentry *);
int			 pfr_walktree(struct radix_node *, void *);
void			 pfr_insert_ktables(struct pfr_ktableworkq *);
void			 pfr_remove_ktables(struct pfr_ktableworkq *);
void			 pfr_clstats_ktables(struct pfr_ktableworkq *, long,
			    int);
struct pfr_ktable	*pfr_create_ktable(struct pfr_table *, long);
void			 pfr_destroy_ktable(struct pfr_ktable *);
void			 pfr_destroy_ktables(struct pfr_ktableworkq *);
int			 pfr_ktable_compare(struct pfr_ktable *,
			    struct pfr_ktable *);
struct pfr_ktable	*pfr_lookup_hash(union pfr_hash *);
struct pfr_ktable	*pfr_lookup_table(struct pfr_table *);
int			 pfr_match_addr(struct pf_addr *, struct pf_addr *,
			    struct pf_addr *, sa_family_t);
void			 pfr_update_stats(struct pf_addr *, struct pf_addr *,
			    struct pf_addr *, sa_family_t, u_int64_t, int, int,
			    int);

RB_PROTOTYPE(pfr_ktablehead, pfr_ktable, pfrkt_tree, pfr_ktable_compare);
RB_GENERATE(pfr_ktablehead, pfr_ktable, pfrkt_tree, pfr_ktable_compare);

struct pfr_ktablehashq	 pfr_ktablehash[PFR_HASH_BUCKETS];
struct pfr_ktablehead	 pfr_ktables;
int			 pfr_ktable_cnt;


int
pfr_clr_addrs(struct pfr_table *tbl, int *ndel, int flags)
{
	struct pfr_ktable	*kt;
	struct pfr_kentryworkq	 workq;
	int			 s, rv;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_DUMMY);
	kt = pfr_lookup_table(tbl);
	if (kt == NULL)
		return (ESRCH);
	rv = pfr_enqueue_addrs(kt, &workq, ndel);
	if (rv)
		return rv;

	if (!(flags & PFR_FLAG_DUMMY)) {
		if (flags & PFR_FLAG_ATOMIC)
			s = splsoftnet();
		pfr_remove_kentries(kt, &workq);
		if (flags & PFR_FLAG_ATOMIC)
			splx(s);
		if (kt->pfrkt_cnt) {
			printf("pfr_clr_addrs: corruption detected.");
			kt->pfrkt_cnt = 0;
		}
	}
	return (0);
}

int
pfr_add_addrs(struct pfr_table *tbl, struct pfr_addr *addr, int size,
    int *nadd, int flags)
{
	struct pfr_ktable	*kt;
	struct pfr_kentryworkq	 workq;
	struct pfr_kentry	*p;
	struct pfr_addr		 ad;
	int			 i, rv, s, xadd = 0;
	long			 tzero = time.tv_sec;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_DUMMY+PFR_FLAG_FEEDBACK);
	kt = pfr_lookup_table(tbl);
	if (kt == NULL)
		return (ESRCH);
	SLIST_INIT(&workq);
	for (i = 0; i < size; i++) {
		if (copyin(addr+i, &ad, sizeof(ad)))
			senderr(EFAULT);
		if (pfr_validate_addr(&ad))
			senderr(EFAULT);
		p = pfr_lookup_addr(kt, &ad, 1);
		if (flags & PFR_FLAG_FEEDBACK) {
			ad.pfra_fback = (p == NULL) ?
				PFR_FB_ADDED : PFR_FB_NONE;
			if (copyout(&ad, addr+i, sizeof(ad)))
				senderr(EFAULT);
		}
		if (p == NULL) {
			if (!(flags & PFR_FLAG_DUMMY)) {
				p = pfr_create_kentry(&ad, tzero);
				if (p == NULL)
					senderr(ENOMEM);
				SLIST_INSERT_HEAD(&workq, p, pfrke_workq);
			}
			xadd++;
		} else if (p->pfrke_not != ad.pfra_not)
			senderr(EEXIST);
	}
	if (!(flags & PFR_FLAG_DUMMY)) {
		if (flags & PFR_FLAG_ATOMIC)
			s = splsoftnet();
		if (pfr_insert_kentries(kt, &workq)) {
			splx(s);
			senderr(ENOMEM);
		}
		if (flags & PFR_FLAG_ATOMIC)
			splx(s);
	}
	if (nadd != NULL)
		*nadd = xadd;
	return (0);
_bad:
	pfr_destroy_kentries(&workq);
	if (flags & PFR_FLAG_FEEDBACK)
		pfr_reset_feedback(addr, size);
	return (rv);
}

int
pfr_del_addrs(struct pfr_table *tbl, struct pfr_addr *addr, int size,
    int *ndel, int flags)
{
	struct pfr_ktable	*kt;
	struct pfr_kentryworkq	 workq;
	struct pfr_kentry	*p;
	struct pfr_addr		 ad;
	int			 i, rv, s, xdel = 0;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_DUMMY+PFR_FLAG_FEEDBACK);
	kt = pfr_lookup_table(tbl);
	if (kt == NULL)
		return (ESRCH);
	SLIST_INIT(&workq);
	for (i = 0; i < size; i++) {
		if (copyin(addr+i, &ad, sizeof(ad)))
			senderr(EFAULT);
		if (pfr_validate_addr(&ad))
			senderr(EINVAL);
		p = pfr_lookup_addr(kt, &ad, 1);
		if (flags & PFR_FLAG_FEEDBACK) {
			ad.pfra_fback = (p != NULL) ?
				PFR_FB_DELETED : PFR_FB_NONE;
			if (copyout(&ad, addr+i, sizeof(ad)))
				senderr(EFAULT);
                }
		if (p != NULL) {
			SLIST_INSERT_HEAD(&workq, p, pfrke_workq);
			xdel++;
		}
	}
	if (!(flags & PFR_FLAG_DUMMY)) {
		if (flags & PFR_FLAG_ATOMIC)
			s = splsoftnet();
		pfr_remove_kentries(kt, &workq);
		if (flags & PFR_FLAG_ATOMIC)
			splx(s);
	}
	if (ndel != NULL)
		*ndel = xdel;
	return (0);
_bad:
	if (flags & PFR_FLAG_FEEDBACK)
		pfr_reset_feedback(addr, size);
	return (rv);
}

int
pfr_set_addrs(struct pfr_table *tbl, struct pfr_addr *addr, int size,
    int *size2, int *nadd, int *ndel, int *nchange, int flags)
{
	struct pfr_ktable	*kt;
	struct pfr_kentryworkq	 addq, delq, changeq;
	struct pfr_walktree	 w;
	struct pfr_kentry	*p;
	struct pfr_addr		 ad;
	int			 i, rv, s, xadd = 0, xdel = 0, xchange = 0;
	long			 tzero = time.tv_sec;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_DUMMY+PFR_FLAG_FEEDBACK);
	kt = pfr_lookup_table(tbl);
	if (kt == NULL)
		return (ESRCH);

	bzero(&w, sizeof(w));
	w.pfrw_op = PFRW_MARK;
	rv = rn_walktree(kt->pfrkt_ip4, pfr_walktree, &w);
	if (!rv)
		rv = rn_walktree(kt->pfrkt_ip6, pfr_walktree, &w);
	if (rv)
		return (rv);

	SLIST_INIT(&addq);
	SLIST_INIT(&delq);
	SLIST_INIT(&changeq);
	for (i = 0; i < size; i++) {
		if (copyin(addr+i, &ad, sizeof(ad)))
			senderr(EFAULT);
		if (pfr_validate_addr(&ad))
			senderr(EINVAL);
		p = pfr_lookup_addr(kt, &ad, 1);
		if (p != NULL) {
			p->pfrke_mark = 1;
			if (p->pfrke_not != ad.pfra_not) {
				SLIST_INSERT_HEAD(&changeq, p, pfrke_workq);
				ad.pfra_fback = PFR_FB_CHANGED;
				xchange++;
			} else
				ad.pfra_fback = PFR_FB_NONE;
		} else {
			if (!(flags & PFR_FLAG_DUMMY)) {
				p = pfr_create_kentry(&ad, tzero);
				if (p == NULL)
					senderr(ENOMEM);
				SLIST_INSERT_HEAD(&addq, p, pfrke_workq);
			}
			ad.pfra_fback = PFR_FB_ADDED;
			xadd++;
		}
		if (flags & PFR_FLAG_FEEDBACK)
			if (copyout(&ad, addr+i, sizeof(ad)))
				senderr(EFAULT);
	}
	w.pfrw_op = PFRW_SWEEP;
	w.pfrw_workq = &delq;
	rv = rn_walktree(kt->pfrkt_ip4, pfr_walktree, &w);
	if (!rv)
		rv = rn_walktree(kt->pfrkt_ip6, pfr_walktree, &w);
	if (rv)
		senderr(rv);
	xdel = w.pfrw_cnt;
	if ((flags & PFR_FLAG_FEEDBACK) && *size2) {
		if (*size2 < size+xdel) {
			*size2 = size+xdel;
			senderr(0);
		}
		i = 0;
		SLIST_FOREACH(p, &delq, pfrke_workq) {
			pfr_copyout_addr(&ad, p);
			ad.pfra_fback = PFR_FB_DELETED;
			if (copyout(&ad, addr+size+i, sizeof(ad)))
				senderr(EFAULT);
			i++;
		}
	}
	if (!(flags & PFR_FLAG_DUMMY)) {
		if (flags & PFR_FLAG_ATOMIC)
			s = splsoftnet();
		if (pfr_insert_kentries(kt, &addq)) {
			if (flags & PFR_FLAG_ATOMIC)
				splx(s);
			senderr(ENOMEM);
		}
		pfr_remove_kentries(kt, &delq);
		SLIST_FOREACH(p, &changeq, pfrke_workq)
			p->pfrke_not ^= 1;
		pfr_clstats_kentries(&changeq, time.tv_sec);
		if (flags & PFR_FLAG_ATOMIC)
			splx(s);
	}
	if (nadd != NULL)
		*nadd = xadd;
	if (ndel != NULL)
		*ndel = xdel;
	if (nchange != NULL)
		*nchange = xchange;
	if ((flags & PFR_FLAG_FEEDBACK) && *size2)
		*size2 = size+xdel;
	return (0);
_bad:
	pfr_destroy_kentries(&addq);
	if (flags & PFR_FLAG_FEEDBACK)
		pfr_reset_feedback(addr, size);
	return (rv);
}

int
pfr_tst_addrs(struct pfr_table *tbl, struct pfr_addr *addr, int size,
	int flags)
{
	struct pfr_ktable	*kt;
	struct pfr_kentry	*p;
	struct pfr_addr		 ad;
	int			 i;

	ACCEPT_FLAGS(0);
	kt = pfr_lookup_table(tbl);
	if (kt == NULL)
		return (ESRCH);

	for (i = 0; i < size; i++) {
		if (copyin(addr+i, &ad, sizeof(ad)))
			return (EFAULT);
		if (pfr_validate_addr(&ad))
			return (EINVAL);
		if (ADDR_NETWORK(&ad))
			return (EINVAL);
		p = pfr_lookup_addr(kt, &ad, 0);
		ad.pfra_fback = (p != NULL && !p->pfrke_not) ?
			PFR_FB_MATCH : PFR_FB_NONE;
		if (copyout(&ad, addr+i, sizeof(ad)))
			return (EFAULT);
	}
	return (0);
}

int
pfr_get_addrs(struct pfr_table *tbl, struct pfr_addr *addr, int *size,
	int flags)
{
	struct pfr_ktable	*kt;
	struct pfr_walktree	 w;
	int			 rv;

	ACCEPT_FLAGS(0);
	kt = pfr_lookup_table(tbl);
	if (kt == NULL)
		return (ESRCH);
	if (kt->pfrkt_cnt > *size) {
		*size = kt->pfrkt_cnt;
		return (0);
	}

	bzero(&w, sizeof(w));
	w.pfrw_op = PFRW_GET_ADDRS;
	w.pfrw_addr = addr;
	w.pfrw_free = kt->pfrkt_cnt;
	rv = rn_walktree(kt->pfrkt_ip4, pfr_walktree, &w);
	if (!rv)
		rv = rn_walktree(kt->pfrkt_ip6, pfr_walktree, &w);
	if (rv)
		return (rv);

	if (w.pfrw_free) {
		printf("pfr_get_addrs: corruption detected.");
		return (ENOTTY);
	}
	*size = kt->pfrkt_cnt;
	return (0);
}

int
pfr_get_astats(struct pfr_table *tbl, struct pfr_astats *addr, int *size,
	int flags)
{
	struct pfr_ktable	*kt;
	struct pfr_walktree	 w;
	struct pfr_kentryworkq	 workq;
	int			 rv, s;
	long			 tzero = time.tv_sec;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_CLSTATS);
	kt = pfr_lookup_table(tbl);
	if (kt == NULL)
		return (ESRCH);
	if (kt->pfrkt_cnt > *size) {
		*size = kt->pfrkt_cnt;
		return (0);
	}

	bzero(&w, sizeof(w));
	w.pfrw_op = PFRW_GET_ASTATS;
	w.pfrw_astats = addr;
	w.pfrw_free = kt->pfrkt_cnt;
	if (flags & PFR_FLAG_ATOMIC)
		s = splsoftnet();
	rv = rn_walktree(kt->pfrkt_ip4, pfr_walktree, &w);
	if (!rv)
		rv = rn_walktree(kt->pfrkt_ip6, pfr_walktree, &w);
	if (!rv && (flags & PFR_FLAG_CLSTATS)) {
		rv = pfr_enqueue_addrs(kt, &workq, NULL);
		if (rv)
			return rv;
		pfr_clstats_kentries(&workq, tzero);
	}
	if (flags & PFR_FLAG_ATOMIC)
		splx(s);
	if (rv)
		return (rv);

	if (w.pfrw_free) {
		printf("pfr_get_astats: corruption detected.");
		return (ENOTTY);
	}
	*size = kt->pfrkt_cnt;
	return (0);
}

int
pfr_clr_astats(struct pfr_table *tbl, struct pfr_addr *addr, int size,
    int *nzero, int flags)
{
	struct pfr_ktable	*kt;
	struct pfr_kentryworkq	 workq;
	struct pfr_kentry	*p;
	struct pfr_addr		 ad;
	int			 i, rv, s, xzero = 0;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_DUMMY+PFR_FLAG_FEEDBACK);
	kt = pfr_lookup_table(tbl);
	if (kt == NULL)
		return (ESRCH);
	SLIST_INIT(&workq);
	for (i = 0; i < size; i++) {
		if (copyin(addr+i, &ad, sizeof(ad)))
			senderr(EFAULT);
		if (pfr_validate_addr(&ad))
			senderr(EINVAL);
		p = pfr_lookup_addr(kt, &ad, 1);
		if (flags & PFR_FLAG_FEEDBACK) {
			ad.pfra_fback = (p != NULL) ?
				PFR_FB_CLEARED : PFR_FB_NONE;
			if (copyout(&ad, addr+i, sizeof(ad)))
				senderr(EFAULT);
                }
		if (p != NULL) {
			SLIST_INSERT_HEAD(&workq, p, pfrke_workq);
			xzero++;
		}
	}

	if (!(flags & PFR_FLAG_DUMMY)) {
		if (flags & PFR_FLAG_ATOMIC)
			s = splsoftnet();
		pfr_clstats_kentries(&workq, 0);
		if (flags & PFR_FLAG_ATOMIC)
			splx(s);
	}
	if (nzero != NULL)
		*nzero = xzero;
	return (0);
_bad:
	if (flags & PFR_FLAG_FEEDBACK)
		pfr_reset_feedback(addr, size);
	return (rv);
}


int
pfr_validate_addr(struct pfr_addr *ad)
{
	switch (ad->pfra_af) {
	case AF_INET:
		if (ad->pfra_af > 32)
			return (-1);
		return (0);
	case AF_INET6:
		if (ad->pfra_af > 128)
			return (-1);
		return (0);
	default:
		return (-1);
	}
}

int
pfr_enqueue_addrs(struct pfr_ktable *kt, struct pfr_kentryworkq *workq,
	int *naddr)
{
	struct pfr_walktree	w;
	int			rv;

	SLIST_INIT(workq);
	bzero(&w, sizeof(w));
	w.pfrw_op = PFRW_ENQUEUE;
	w.pfrw_workq = workq;
	rv = rn_walktree(kt->pfrkt_ip4, pfr_walktree, &w);
	if (rv)
		return (rv);
	rv = rn_walktree(kt->pfrkt_ip6, pfr_walktree, &w);
	if (rv)
		return (rv);
	if (naddr != NULL)
		*naddr = w.pfrw_cnt;
	return (0);
}

struct pfr_kentry *
pfr_lookup_addr(struct pfr_ktable *kt, struct pfr_addr *ad, int exact)
{
	union sockaddr_union	 sa, mask;
	struct radix_node_head	*head;
	struct pfr_kentry	*ke;

	bzero(&sa, sizeof(sa));
	if (ad->pfra_af == AF_INET) {
		FILLIN_SIN(sa.sin, ad->pfra_ip4addr);
		head = kt->pfrkt_ip4;
	} else {
		FILLIN_SIN6(sa.sin6, ad->pfra_ip6addr);
		head = kt->pfrkt_ip6;
	}
	if (ADDR_NETWORK(ad)) {
		pfr_prepare_network(&mask, ad->pfra_af, ad->pfra_net);
		ke = (struct pfr_kentry *)rn_lookup(&sa, &mask, head);
	} else {
		ke = (struct pfr_kentry *)rn_match(&sa, head);
		if (exact && ke && KENTRY_NETWORK(ke))
			ke = NULL;
	}
	return (ke);
}

struct pfr_kentry *
pfr_create_kentry(struct pfr_addr *ad, long tzero)
{
	struct pfr_kentry	*ke;

	ke = pool_get(&pfr_kentry_pl, PR_NOWAIT);
	if (ke == NULL)
		return (NULL);
	bzero(ke, sizeof(*ke));

	if (ad->pfra_af == AF_INET)
		FILLIN_SIN(ke->pfrke_sa.sin, ad->pfra_ip4addr);
	else
		FILLIN_SIN6(ke->pfrke_sa.sin6, ad->pfra_ip6addr);
	ke->pfrke_af = ad->pfra_af;
	ke->pfrke_net = ad->pfra_net;
	ke->pfrke_not = ad->pfra_not;
	ke->pfrke_tzero = tzero;
	return (ke);
}

void
pfr_destroy_kentry(struct pfr_kentry *ke)
{
	if (ke != NULL)
		pool_put(&pfr_kentry_pl, ke);
}

void
pfr_destroy_kentries(struct pfr_kentryworkq *workq)
{
	struct pfr_kentry	*p, *q;

	for (p = SLIST_FIRST(workq); p != NULL; p = q) {
		q = SLIST_NEXT(p, pfrke_workq);
		pfr_destroy_kentry(p);
	}
}

int
pfr_insert_kentries(struct pfr_ktable *kt,
    struct pfr_kentryworkq *workq)
{
	struct pfr_kentry	*p, *q;
	int			 n = 0;

	SLIST_FOREACH(p, workq, pfrke_workq) {
		if (pfr_route_kentry(kt, p)) {
			/* bad luck - no memory for netmask */
			SLIST_FOREACH(q, workq, pfrke_workq) {
				if (q == p)
					break;
				pfr_unroute_kentry(kt, q);
			}
			return (-1);
		}
		n++;
	}
	kt->pfrkt_cnt += n;
	return (0);
}

void
pfr_remove_kentries(struct pfr_ktable *kt,
    struct pfr_kentryworkq *workq)
{
	struct pfr_kentry	*p;
	int			 n = 0;

	SLIST_FOREACH(p, workq, pfrke_workq) {
		pfr_unroute_kentry(kt, p);
		n++;
	}
	kt->pfrkt_cnt -= n;
	pfr_destroy_kentries(workq);
}

void
pfr_clstats_kentries(struct pfr_kentryworkq *workq, long tzero)
{
	struct pfr_kentry	*p;
	int			 s, n = 0;

	SLIST_FOREACH(p, workq, pfrke_workq) {
		s = splsoftnet();
		bzero(p->pfrke_packets, sizeof(p->pfrke_packets));
		bzero(p->pfrke_bytes, sizeof(p->pfrke_bytes));
		splx(s);
		p->pfrke_tzero = tzero;
		n++;
	}
}

void
pfr_reset_feedback(struct pfr_addr *addr, int size)
{
	struct pfr_addr	ad;
	int		i;

        for (i = 0; i < size; i++) {
                if (copyin(addr+i, &ad, sizeof(ad)))
                        break;
                ad.pfra_fback = PFR_FB_NONE;
                if (copyout(&ad, addr+i, sizeof(ad)))
			break;
        }
}

void
pfr_prepare_network(union sockaddr_union *sa, int af, int net)
{
	int	i;

	bzero(sa, sizeof(*sa));
	if (af == AF_INET) {
		sa->sin.sin_len = sizeof(sa->sin);
		sa->sin.sin_family = AF_INET;
		sa->sin.sin_addr.s_addr = htonl(-1 << (32-net));
	} else {
		sa->sin6.sin6_len = sizeof(sa->sin6);
		sa->sin6.sin6_family = AF_INET;
		for (i = 0; i < 4; i++) {
			if (net <= 32) {
				sa->sin6.sin6_addr.s6_addr32[i] =
					htonl(-1 << (32-net));
				break;
			}
			sa->sin6.sin6_addr.s6_addr32[i] = 0xFFFFFFFF;
			net -= 32;
		}
	}
}

int
pfr_route_kentry(struct pfr_ktable *kt, struct pfr_kentry *ke)
{
	union sockaddr_union	 mask;
	struct radix_node	*rn;
	struct radix_node_head	*head;
	int			 s;

	if (ke->pfrke_af == AF_INET)
		head = kt->pfrkt_ip4;
	else
		head = kt->pfrkt_ip6;

	s = splsoftnet();
	if (KENTRY_NETWORK(ke)) {
		pfr_prepare_network(&mask, ke->pfrke_af, ke->pfrke_net);
		rn = rn_addroute(&ke->pfrke_sa, &mask, head, ke->pfrke_node);
	} else
		rn = rn_addroute(&ke->pfrke_sa, NULL, head, ke->pfrke_node);
	splx(s);

	if (rn == NULL) {
		printf("pfr_route_kentry: no memory for mask\n");
		return (-1);
	}
	return (0);
}

int
pfr_unroute_kentry(struct pfr_ktable *kt, struct pfr_kentry *ke)
{
	union sockaddr_union	 mask;
	struct radix_node	*rn;
	struct radix_node_head	*head;
	int			 s;

	if (ke->pfrke_af == AF_INET)
		head = kt->pfrkt_ip4;
	else
		head = kt->pfrkt_ip6;

	s = splsoftnet();
	if (KENTRY_NETWORK(ke)) {
		pfr_prepare_network(&mask, ke->pfrke_af, ke->pfrke_net);
		rn = rn_delete(&ke->pfrke_sa, &mask, head);
	} else
		rn = rn_delete(&ke->pfrke_sa, NULL, head);
	splx(s);

	if (rn == NULL) {
		printf("pfr_unroute_kentry: delete failed\n");
		return (-1);
	}
	return (0);
}

void
pfr_copyout_addr(struct pfr_addr *ad, struct pfr_kentry *ke)
{
	bzero(ad, sizeof(*ad));
	ad->pfra_af = ke->pfrke_af;
	ad->pfra_net = ke->pfrke_net;
	ad->pfra_not = ke->pfrke_not;
	if (ad->pfra_af == AF_INET)
		ad->pfra_ip4addr = ke->pfrke_sa.sin.sin_addr;
	else
		ad->pfra_ip6addr = ke->pfrke_sa.sin6.sin6_addr;
}

int
pfr_walktree(struct radix_node *rn, void *arg)
{
	struct pfr_kentry	*ke = (struct pfr_kentry *)rn;
	struct pfr_walktree	*w = arg;
	int			 s;

	switch (w->pfrw_op) {
	case PFRW_MARK:
		ke->pfrke_mark = 0;
		break;
	case PFRW_SWEEP:
		if (ke->pfrke_mark)
			break;
		/* fall trough */
	case PFRW_ENQUEUE:
		SLIST_INSERT_HEAD(w->pfrw_workq, ke, pfrke_workq);
		w->pfrw_cnt++;
		break;
	case PFRW_GET_ADDRS:
		if (w->pfrw_free-- > 0) {
			struct pfr_addr ad;

			pfr_copyout_addr(&ad, ke);
			if (copyout(&ad, w->pfrw_addr, sizeof(ad)))
				return (EFAULT);
			w->pfrw_addr++;
		}
		break;
	case PFRW_GET_ASTATS:
		if (w->pfrw_free-- > 0) {
			struct pfr_astats as;

			pfr_copyout_addr(&as.pfras_a, ke);

			s = splsoftnet();
			bcopy(ke->pfrke_packets, as.pfras_packets,
				sizeof(as.pfras_packets));
			bcopy(ke->pfrke_bytes, as.pfras_bytes,
				sizeof(as.pfras_bytes));
			splx(s);
			as.pfras_tzero = ke->pfrke_tzero;

			if (copyout(&as, w->pfrw_astats, sizeof(as)))
				return (EFAULT);
			w->pfrw_astats++;
		}
		break;
	}
	return (0);
}


int
pfr_clr_tables(int *ndel, int flags)
{
	struct pfr_ktableworkq	 workq;
	struct pfr_ktable	*p;
	int			 s, xdel = 0;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_DUMMY);
	SLIST_INIT(&workq);
	RB_FOREACH(p, pfr_ktablehead, &pfr_ktables) {
		SLIST_INSERT_HEAD(&workq, p, pfrkt_workq);
		xdel++;
	}

	if (!(flags & PFR_FLAG_DUMMY)) {
		if (flags & PFR_FLAG_ATOMIC)
			s = splsoftnet();
		pfr_remove_ktables(&workq);
		if (flags & PFR_FLAG_ATOMIC)
			splx(s);
	}
	if (ndel != NULL)
		*ndel = xdel;
	return (0);
}

int
pfr_add_tables(struct pfr_table *tbl, int size, int *nadd, int flags)
{
	struct pfr_ktableworkq	 workq;
	struct pfr_ktable	*p, key;
	int			 i, s, xadd = 0;
	long			 tzero = time.tv_sec;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_DUMMY);
	SLIST_INIT(&workq);
	for(i = 0; i < size; i++) {
		if (copyin(tbl+i, &key.pfrkt_t, sizeof(key.pfrkt_t))) {
			pfr_destroy_ktables(&workq);
			return (EFAULT);
		}
		if (key.pfrkt_name[PF_TABLE_NAME_SIZE-1])
			return (EINVAL);
		p = RB_FIND(pfr_ktablehead, &pfr_ktables, &key);
		if (p == NULL) {
			if (!(flags & PFR_FLAG_DUMMY)) {
				p = pfr_create_ktable(&key.pfrkt_t, tzero);
				if (p == NULL) {
					pfr_destroy_ktables(&workq);
					return (ENOMEM);
				}
				SLIST_INSERT_HEAD(&workq, p, pfrkt_workq);
				/* XXX move the following out of the if */
				if (pfr_lookup_hash(&p->pfrkt_hash)) {
					printf(
					    "pfr_add_tables: sha collision\n");
					pfr_destroy_ktables(&workq);
					return (EEXIST);
				}
			}
			xadd++;
		}
	}
	if (!(flags & PFR_FLAG_DUMMY)) {
		if (flags & PFR_FLAG_ATOMIC)
			s = splsoftnet();
		pfr_insert_ktables(&workq);
		if (flags & PFR_FLAG_ATOMIC)
			splx(s);
	}
	if (nadd != NULL)
		*nadd = xadd;
	return (0);
}

int
pfr_del_tables(struct pfr_table *tbl, int size, int *ndel, int flags)
{
	struct pfr_ktableworkq	 workq;
	struct pfr_ktable	*p, key;
	int			 i, s, xdel = 0;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_DUMMY);
	SLIST_INIT(&workq);
	for(i = 0; i < size; i++) {
		if (copyin(tbl+i, &key.pfrkt_t, sizeof(key.pfrkt_t)))
			return (EFAULT);
		p = RB_FIND(pfr_ktablehead, &pfr_ktables, &key);
		if (p != NULL) {
			SLIST_INSERT_HEAD(&workq, p, pfrkt_workq);
			xdel++;
		}
	}

	if (!(flags & PFR_FLAG_DUMMY)) {
		if (flags & PFR_FLAG_ATOMIC)
			s = splsoftnet();
		pfr_remove_ktables(&workq);
		if (flags & PFR_FLAG_ATOMIC)
			splx(s);
	}
	if (ndel != NULL)
		*ndel = xdel;
	return (0);
}

int
pfr_get_tables(struct pfr_table *tbl, int *size, int flags)
{
	struct pfr_ktable	*p;
	int			 n = pfr_ktable_cnt;

	ACCEPT_FLAGS(0);
	if (n > *size) {
		*size = n;
		return (0);
	}
	RB_FOREACH(p, pfr_ktablehead, &pfr_ktables) {
		if (n-- <= 0)
			continue;
		if (copyout(&p->pfrkt_t, tbl++, sizeof(*tbl)))
			return (EFAULT);
	}
	if (n) {
		printf("pfr_get_tables: corruption detected.");
		return (ENOTTY);
	}
	*size = pfr_ktable_cnt;
	return (0);
}

int
pfr_get_tstats(struct pfr_tstats *tbl, int *size, int flags)
{
	struct pfr_ktable	*p;
	struct pfr_ktableworkq	 workq;
	int			 s, n = pfr_ktable_cnt;
	long			 tzero = time.tv_sec;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_CLSTATS+PFR_FLAG_RECURSE);
	if (n > *size) {
		*size = n;
		return (0);
	}
	SLIST_INIT(&workq);
	if (flags & PFR_FLAG_ATOMIC)
		s = splsoftnet();
	RB_FOREACH(p, pfr_ktablehead, &pfr_ktables) {
		if (n-- <= 0)
			continue;
		if (!(flags & PFR_FLAG_ATOMIC))
			s = splsoftnet();
		if (copyout(&p->pfrkt_ts, tbl++, sizeof(*tbl))) {
			splx(s);
			return (EFAULT);
		}
		if (!(flags & PFR_FLAG_ATOMIC))
			splx(s);
		SLIST_INSERT_HEAD(&workq, p, pfrkt_workq);
	}
	if (flags & PFR_FLAG_CLSTATS)
		pfr_clstats_ktables(&workq, tzero,
		    flags & PFR_FLAG_RECURSE);
	if (flags & PFR_FLAG_ATOMIC)
		splx(s);
	if (n) {
		printf("pfr_get_tstats: corruption detected.");
		return (ENOTTY);
	}
	*size = pfr_ktable_cnt;
	return (0);
}

int
pfr_clr_tstats(struct pfr_table *tbl, int size, int *nzero, int flags)
{
	struct pfr_ktableworkq	 workq;
	struct pfr_ktable	*p, key;
	int			 i, s, xzero = 0;
	long			 tzero = time.tv_sec;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_CLSTATS+PFR_FLAG_RECURSE);
	SLIST_INIT(&workq);
	for(i = 0; i < size; i++) {
		if (copyin(tbl+i, &key.pfrkt_t, sizeof(key.pfrkt_t)))
			return (EFAULT);
		p = RB_FIND(pfr_ktablehead, &pfr_ktables, &key);
		if (p != NULL) {
			SLIST_INSERT_HEAD(&workq, p, pfrkt_workq);
			xzero++;
		}
	}

	if (!(flags & PFR_FLAG_DUMMY)) {
		if (flags & PFR_FLAG_ATOMIC)
			s = splsoftnet();
		pfr_clstats_ktables(&workq, tzero, flags & PFR_FLAG_RECURSE);
		if (flags & PFR_FLAG_ATOMIC)
			splx(s);
	}
	if (nzero != NULL)
		*nzero = xzero;
        return (0);
}

int
pfr_wrap_table(struct pfr_table *tbl, struct pf_addr_wrap *wrap,
    int *exists, int flags)
{
	union pfr_hash		hash;
	struct pf_addr_wrap	w;
	SHA1_CTX		sha1;

	ACCEPT_FLAGS(0);
	if (!*tbl->pfrt_name || tbl->pfrt_name[PF_TABLE_NAME_SIZE-1])
		return (EINVAL);
	SHA1Init(&sha1);
	SHA1Update(&sha1, tbl->pfrt_name, strlen(tbl->pfrt_name));
	SHA1Final(hash.pfrh_sha1, &sha1);

	bzero(&w, sizeof(w));
	bcopy(&hash, &w.addr, sizeof(w.addr));
	w.mask.addr32[0] = PF_TABLE_MASK;
	w.mask.addr32[1] = hash.pfrh_int32[4];
	if (copyout(&w, wrap, sizeof(*wrap)))
		return (EFAULT);

	if (exists != NULL)
		*exists = pfr_lookup_table(tbl) != NULL;
	return (0);
}

int
pfr_unwrap_table(struct pfr_table *tbl, struct pf_addr_wrap *wrap, int flags)
{
	union pfr_hash		 hash;
	struct pf_addr_wrap	 w;
	struct pfr_ktable	*kt;

	ACCEPT_FLAGS(0);
	if (copyin(wrap, &w, sizeof(w)))
		return (EFAULT);

	if (w.mask.addr32[0] != PF_TABLE_MASK || w.mask.addr32[2] ||
	    w.mask.addr32[3])
		return (EINVAL);

	bcopy(&w.addr, &hash, 16);
	hash.pfrh_int32[4] = w.mask.addr32[1];
	kt = pfr_lookup_hash(&hash);
	if (kt == NULL)
		return (ENOENT);
	*tbl = kt->pfrkt_t;
	return (0);
}

void
pfr_insert_ktables(struct pfr_ktableworkq *workq)
{
	struct pfr_ktable	*p;
	int			 s, n = 0;

	/* insert into tree */
	SLIST_FOREACH(p, workq, pfrkt_workq) {
		RB_INSERT(pfr_ktablehead, &pfr_ktables, p);
		n++;
	}
	pfr_ktable_cnt += n;

	SLIST_FOREACH(p, workq, pfrkt_workq) {
		s = splsoftnet();
		SLIST_INSERT_HEAD(pfr_ktablehash +
		    PFR_HASH_BUCKET(p->pfrkt_hash),
		    p, pfrkt_hashq);
		splx(s);
	}
}

void
pfr_remove_ktables(struct pfr_ktableworkq *workq)
{
	struct pfr_kentryworkq	 addrq;
	struct pfr_ktable	*p;
	int			 s, n = 0;

	SLIST_FOREACH(p, workq, pfrkt_workq) {
		s = splsoftnet();
		SLIST_REMOVE(pfr_ktablehash + PFR_HASH_BUCKET(p->pfrkt_hash),
		    p, pfr_ktable, pfrkt_hashq);
		splx(s);
	}

	SLIST_FOREACH(p, workq, pfrkt_workq) {
		RB_REMOVE(pfr_ktablehead, &pfr_ktables, p);
		if (pfr_enqueue_addrs(p, &addrq, NULL))
			printf("pfr_remove_ktables: enqueue failed");
		pfr_destroy_kentries(&addrq);
		n++;
	}
	pfr_ktable_cnt -= n;
	pfr_destroy_ktables(workq);
}

void
pfr_clstats_ktables(struct pfr_ktableworkq *workq, long tzero, int recurse)
{
	struct pfr_kentryworkq	 addrq;
	struct pfr_ktable	*p;
	int			 s;

	SLIST_FOREACH(p, workq, pfrkt_workq) {
		if (recurse) {
			if (pfr_enqueue_addrs(p, &addrq, NULL))
				printf("pfr_clr_tstats: enqueue failed");
			pfr_clstats_kentries(&addrq, tzero);
		}
		s = splsoftnet();
		bzero(p->pfrkt_packets, sizeof(p->pfrkt_packets));
		bzero(p->pfrkt_bytes, sizeof(p->pfrkt_bytes));
		p->pfrkt_match = p->pfrkt_nomatch = 0;
		splx(s);
		p->pfrkt_tzero = tzero;
	}
}

struct pfr_ktable *
pfr_create_ktable(struct pfr_table *tbl, long tzero)
{
	struct pfr_ktable	*kt;
	SHA1_CTX		 sha1;

	kt = pool_get(&pfr_ktable_pl, PR_NOWAIT);
	if (kt == NULL)
		return (NULL);
	bzero(kt, sizeof(*kt));
	kt->pfrkt_t = *tbl;

	/* compute secure hash */
	SHA1Init(&sha1);
	SHA1Update(&sha1, kt->pfrkt_name, strlen(kt->pfrkt_name));
	SHA1Final(kt->pfrkt_hash.pfrh_sha1, &sha1);

	if (!rn_inithead((void **)&kt->pfrkt_ip4,
	    8 * offsetof(struct sockaddr_in, sin_addr)) ||
	    !rn_inithead((void **)&kt->pfrkt_ip6,
	    8 * offsetof(struct sockaddr_in6, sin6_addr))) {
		pfr_destroy_ktable(kt);
		return (NULL);
	}
	kt->pfrkt_tzero = tzero;

	return (kt);
}

void
pfr_destroy_ktable(struct pfr_ktable *kt)
{
	if (kt == NULL)
		return;
	if (kt->pfrkt_ip4 != NULL)
		free((caddr_t)kt->pfrkt_ip4, M_RTABLE);
	if (kt->pfrkt_ip6 != NULL)
		free((caddr_t)kt->pfrkt_ip6, M_RTABLE);
	pool_put(&pfr_ktable_pl, kt);
}

void
pfr_destroy_ktables(struct pfr_ktableworkq *workq)
{
	struct pfr_ktable	*p, *q;

	for(p = SLIST_FIRST(workq); p; p = q) {
		q = SLIST_NEXT(p, pfrkt_workq);
		pfr_destroy_ktable(p);
	}
}

int
pfr_ktable_compare(struct pfr_ktable *p, struct pfr_ktable *q)
{
	return (strncmp(p->pfrkt_name, q->pfrkt_name, PF_TABLE_NAME_SIZE));
}

struct pfr_ktable *
pfr_lookup_hash(union pfr_hash *hash)
{
	struct pfr_ktable	*p;

	SLIST_FOREACH(p, pfr_ktablehash+PFR_HASH_BUCKET(*hash), pfrkt_hashq)
		if (!memcmp(p->pfrkt_hash.pfrh_sha1, hash->pfrh_sha1, 20))
			return (p);
	return (NULL);
}

struct pfr_ktable *
pfr_lookup_table(struct pfr_table *tbl)
{
	/* struct pfr_ktable start like a struct pfr_table */
	return RB_FIND(pfr_ktablehead, &pfr_ktables, (struct pfr_ktable *)tbl);
}


/*
 * Return 1 if the addresses a and b match (with mask m), otherwise return 0.
 * If n is 0, they match if they are equal. If n is != 0, they match if they
 * are different.
 */
int
pfr_match_addr(struct pf_addr *a, struct pf_addr *m,
    struct pf_addr *b, sa_family_t af)
{
	union pfr_hash		 hash;
	struct pfr_ktable	*kt;
	struct pfr_kentry	*ke = NULL;
	int			 match;

	bcopy(a, &hash, 16);
	hash.pfrh_int32[4] = m->addr32[1];
	kt = pfr_lookup_hash(&hash);
	if (kt == NULL)
		return (0);
	switch (af) {
	case AF_INET:
		pfr_sin.sin_addr.s_addr = b->addr32[0];
		ke = (struct pfr_kentry *)rn_match(&pfr_sin, kt->pfrkt_ip4);
		break;
	case AF_INET6:
		bcopy(&b, &pfr_sin6.sin6_addr, sizeof(pfr_sin6.sin6_addr));
		ke = (struct pfr_kentry *)rn_match(&pfr_sin6, kt->pfrkt_ip6);
		break;
	}
	match = (ke && !ke->pfrke_not);
	if (match)
		kt->pfrkt_match++;
	else
		kt->pfrkt_nomatch++;
	return (match);
}

void
pfr_update_stats(struct pf_addr *a, struct pf_addr *m,
    struct pf_addr *b, sa_family_t af, u_int64_t len,
    int dir_out, int op_pass, int notrule)
{
	union pfr_hash		 hash;
	struct pfr_ktable	*kt;
	struct pfr_kentry	*ke = NULL;

	bcopy(a, &hash, 16);
	hash.pfrh_int32[4] = m->addr32[1];
	kt = pfr_lookup_hash(&hash);
	if (kt == NULL)
		return;

	switch (af) {
	case AF_INET:
		pfr_sin.sin_addr.s_addr = b->addr32[0];
		ke = (struct pfr_kentry *)rn_match(&pfr_sin, kt->pfrkt_ip4);
		break;
	case AF_INET6:
		bcopy(&b, &pfr_sin6.sin6_addr, sizeof(pfr_sin6.sin6_addr));
		ke = (struct pfr_kentry *)rn_match(&pfr_sin6, kt->pfrkt_ip6);
		break;
	}
	if (ke == NULL || ke->pfrke_not != notrule) {
		if (op_pass != PFR_OP_PASS)
			printf("pfr_update_stats: assertion failed.");
		op_pass = PFR_OP_XPASS;
	}
	kt->pfrkt_packets[dir_out][op_pass]++;
	kt->pfrkt_bytes[dir_out][op_pass] += len;
	if (op_pass != PFR_OP_XPASS) {
		ke->pfrke_packets[dir_out][op_pass]++;
		ke->pfrke_bytes[dir_out][op_pass] += len;
	}
}

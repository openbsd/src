/*	$OpenBSD: pf_table.c,v 1.22 2003/01/15 10:42:48 cedric Exp $	*/

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

#define ACCEPT_FLAGS(oklist)			\
	do {					\
		if ((flags & ~(oklist)) &	\
		    PFR_FLAG_ALLMASK)		\
			return (EINVAL);	\
	} while (0)

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

#define SWAP(type, a1, a2)			\
	do {					\
		type tmp = a1;			\
		a1 = a2;			\
		a2 = tmp;			\
	} while (0)

#define	AF_BITS(af)		(((af)==AF_INET)?32:128)
#define	ADDR_NETWORK(ad)	((ad)->pfra_net < AF_BITS((ad)->pfra_af))
#define	KENTRY_NETWORK(ke)	((ke)->pfrke_net < AF_BITS((ke)->pfrke_af))

#define NO_ADDRESSES		(-1)
#define ENQUEUE_UNMARKED_ONLY	(1)
#define INVERT_NEG_FLAG		(1)

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

#define senderr(e)	do { rv = (e); goto _bad; } while (0)

struct pool		 pfr_ktable_pl;
struct pool		 pfr_kentry_pl;
struct sockaddr_in	 pfr_sin;
struct sockaddr_in6	 pfr_sin6;

void			 pfr_copyout_addr(struct pfr_addr *,
			    struct pfr_kentry *ke);
int			 pfr_validate_addr(struct pfr_addr *);
void			 pfr_enqueue_addrs(struct pfr_ktable *,
			    struct pfr_kentryworkq *, int *, int);
void			 pfr_mark_addrs(struct pfr_ktable *);
struct pfr_kentry	*pfr_lookup_addr(struct pfr_ktable *,
			    struct pfr_addr *, int);
struct pfr_kentry	*pfr_create_kentry(struct pfr_addr *);
void			 pfr_destroy_kentries(struct pfr_kentryworkq *);
void			 pfr_destroy_kentry(struct pfr_kentry *);
void			 pfr_insert_kentries(struct pfr_ktable *,
			    struct pfr_kentryworkq *, long);
void			 pfr_remove_kentries(struct pfr_ktable *,
			    struct pfr_kentryworkq *);
void			 pfr_clstats_kentries(struct pfr_kentryworkq *, long,
			    int);
void			 pfr_reset_feedback(struct pfr_addr *, int);
void			 pfr_prepare_network(union sockaddr_union *, int, int);
int			 pfr_route_kentry(struct pfr_ktable *,
			    struct pfr_kentry *);
int			 pfr_unroute_kentry(struct pfr_ktable *,
			    struct pfr_kentry *);
int			 pfr_walktree(struct radix_node *, void *);
int			 pfr_validate_table(struct pfr_table *, int);
void			 pfr_commit_ktable(struct pfr_ktable *, long);
void			 pfr_insert_ktables(struct pfr_ktableworkq *);
void			 pfr_insert_ktable(struct pfr_ktable *);
void			 pfr_setflags_ktables(struct pfr_ktableworkq *, int,
			    int);
void			 pfr_setflags_ktable(struct pfr_ktable *, int, int);
void			 pfr_clstats_ktables(struct pfr_ktableworkq *, long,
			    int);
void			 pfr_clstats_ktable(struct pfr_ktable *, long, int);
struct pfr_ktable	*pfr_create_ktable(struct pfr_table *, long);
void			 pfr_destroy_ktables(struct pfr_ktableworkq *, int);
void			 pfr_destroy_ktable(struct pfr_ktable *, int);
int			 pfr_ktable_compare(struct pfr_ktable *,
			    struct pfr_ktable *);
struct pfr_ktable	*pfr_lookup_table(struct pfr_table *);

RB_PROTOTYPE(pfr_ktablehead, pfr_ktable, pfrkt_tree, pfr_ktable_compare);
RB_GENERATE(pfr_ktablehead, pfr_ktable, pfrkt_tree, pfr_ktable_compare);

struct pfr_ktablehead	 pfr_ktables;
struct pfr_table	 pfr_nulltable;
int			 pfr_ktable_cnt;
int			 pfr_ticket;

void
pfr_initialize(void)
{
	pool_init(&pfr_ktable_pl, sizeof(struct pfr_ktable), 0, 0, 0,
	    "pfrktable", NULL);
	pool_init(&pfr_kentry_pl, sizeof(struct pfr_kentry), 0, 0, 0,
	    "pfrkentry", NULL);

	pfr_sin.sin_len = sizeof(pfr_sin);
	pfr_sin.sin_family = AF_INET;
	pfr_sin6.sin6_len = sizeof(pfr_sin6);
	pfr_sin6.sin6_family = AF_INET6;

	pfr_ticket = 100;
}

int
pfr_clr_addrs(struct pfr_table *tbl, int *ndel, int flags)
{
	struct pfr_ktable	*kt;
	struct pfr_kentryworkq	 workq;
	int			 s;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_DUMMY);
	if (pfr_validate_table(tbl, 0))
		return (EINVAL);
	kt = pfr_lookup_table(tbl);
	if (kt == NULL || !(kt->pfrkt_flags & PFR_TFLAG_ACTIVE))
		return (ESRCH);
	if (kt->pfrkt_flags & PFR_TFLAG_CONST)
		return (EPERM);
	pfr_enqueue_addrs(kt, &workq, ndel, 0);

	if (!(flags & PFR_FLAG_DUMMY)) {
		if (flags & PFR_FLAG_ATOMIC)
			s = splsoftnet();
		pfr_remove_kentries(kt, &workq);
		if (flags & PFR_FLAG_ATOMIC)
			splx(s);
		if (kt->pfrkt_cnt) {
			printf("pfr_clr_addrs: corruption detected (%d).",
			    kt->pfrkt_cnt);
			kt->pfrkt_cnt = 0;
		}
	}
	return (0);
}

int
pfr_add_addrs(struct pfr_table *tbl, struct pfr_addr *addr, int size,
    int *nadd, int flags)
{
	struct pfr_ktable	*kt, *tmpkt;
	struct pfr_kentryworkq	 workq;
	struct pfr_kentry	*p, *q;
	struct pfr_addr		 ad;
	int			 i, rv, s, xadd = 0;
	long			 tzero = time.tv_sec;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_DUMMY+PFR_FLAG_FEEDBACK);
	if (pfr_validate_table(tbl, 0))
		return (EINVAL);
	kt = pfr_lookup_table(tbl);
	if (kt == NULL || !(kt->pfrkt_flags & PFR_TFLAG_ACTIVE))
		return (ESRCH);
	if (kt->pfrkt_flags & PFR_TFLAG_CONST)
		return (EPERM);
	tmpkt = pfr_create_ktable(&pfr_nulltable, 0);
	if (tmpkt == NULL)
		return (ENOMEM);
	SLIST_INIT(&workq);
	for (i = 0; i < size; i++) {
		if (copyin(addr+i, &ad, sizeof(ad)))
			senderr(EFAULT);
		if (pfr_validate_addr(&ad))
			senderr(EFAULT);
		p = pfr_lookup_addr(kt, &ad, 1);
		q = pfr_lookup_addr(tmpkt, &ad, 1);
		if (flags & PFR_FLAG_FEEDBACK) {
			if (q != NULL)
				ad.pfra_fback = PFR_FB_DUPLICATE;
			else if (p == NULL)
				ad.pfra_fback = PFR_FB_ADDED;
			else if (p->pfrke_not != ad.pfra_not)
				ad.pfra_fback = PFR_FB_CONFLICT;
			else
				ad.pfra_fback = PFR_FB_NONE;
		}
		if (p == NULL && q == NULL) {
			p = pfr_create_kentry(&ad);
			if (p == NULL)
				senderr(ENOMEM);
			if (pfr_route_kentry(tmpkt, p)) {
				pfr_destroy_kentry(p);
				ad.pfra_fback = PFR_FB_NONE;
			} else {
				SLIST_INSERT_HEAD(&workq, p, pfrke_workq);
				xadd++;
			}
		}
		if (flags & PFR_FLAG_FEEDBACK)
			if (copyout(&ad, addr+i, sizeof(ad)))
				senderr(EFAULT);
	}
	if (!(flags & PFR_FLAG_DUMMY)) {
		if (flags & PFR_FLAG_ATOMIC)
			s = splsoftnet();
		pfr_insert_kentries(kt, &workq, tzero);
		if (flags & PFR_FLAG_ATOMIC)
			splx(s);
	} else
		pfr_destroy_kentries(&workq);
	if (nadd != NULL)
		*nadd = xadd;
	pfr_destroy_ktable(tmpkt, 0);
	return (0);
_bad:
	pfr_destroy_kentries(&workq);
	if (flags & PFR_FLAG_FEEDBACK)
		pfr_reset_feedback(addr, size);
	pfr_destroy_ktable(tmpkt, 0);
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
	if (pfr_validate_table(tbl, 0))
		return (EINVAL);
	kt = pfr_lookup_table(tbl);
	if (kt == NULL || !(kt->pfrkt_flags & PFR_TFLAG_ACTIVE))
		return (ESRCH);
	if (kt->pfrkt_flags & PFR_TFLAG_CONST)
		return (EPERM);
	pfr_mark_addrs(kt);
	SLIST_INIT(&workq);
	for (i = 0; i < size; i++) {
		if (copyin(addr+i, &ad, sizeof(ad)))
			senderr(EFAULT);
		if (pfr_validate_addr(&ad))
			senderr(EINVAL);
		p = pfr_lookup_addr(kt, &ad, 1);
		if (flags & PFR_FLAG_FEEDBACK) {
			if (p == NULL)
				ad.pfra_fback = PFR_FB_NONE;
			else if (p->pfrke_not != ad.pfra_not)
				ad.pfra_fback = PFR_FB_CONFLICT;
			else if (p->pfrke_mark)
				ad.pfra_fback = PFR_FB_DUPLICATE;
			else
				ad.pfra_fback = PFR_FB_DELETED;
		}
		if (p != NULL && p->pfrke_not == ad.pfra_not &&
		    !p->pfrke_mark) {
			p->pfrke_mark = 1;
			SLIST_INSERT_HEAD(&workq, p, pfrke_workq);
			xdel++;
		}
		if (flags & PFR_FLAG_FEEDBACK)
			if (copyout(&ad, addr+i, sizeof(ad)))
				senderr(EFAULT);
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
	struct pfr_ktable	*kt, *tmpkt;
	struct pfr_kentryworkq	 addq, delq, changeq;
	struct pfr_kentry	*p, *q;
	struct pfr_addr		 ad;
	int			 i, rv, s, xadd = 0, xdel = 0, xchange = 0;
	long			 tzero = time.tv_sec;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_DUMMY+PFR_FLAG_FEEDBACK);
	if (pfr_validate_table(tbl, 0))
		return (EINVAL);
	kt = pfr_lookup_table(tbl);
	if (kt == NULL || !(kt->pfrkt_flags & PFR_TFLAG_ACTIVE))
		return (ESRCH);
	if (kt->pfrkt_flags & PFR_TFLAG_CONST)
		return (EPERM);
	tmpkt = pfr_create_ktable(&pfr_nulltable, 0);
	if (tmpkt == NULL)
		return (ENOMEM);
	pfr_mark_addrs(kt);
	SLIST_INIT(&addq);
	SLIST_INIT(&delq);
	SLIST_INIT(&changeq);
	for (i = 0; i < size; i++) {
		if (copyin(addr+i, &ad, sizeof(ad)))
			senderr(EFAULT);
		if (pfr_validate_addr(&ad))
			senderr(EINVAL);
		ad.pfra_fback = PFR_FB_NONE;
		p = pfr_lookup_addr(kt, &ad, 1);
		if (p != NULL) {
			if (p->pfrke_mark) {
				ad.pfra_fback = PFR_FB_DUPLICATE;
				goto _skip;
			}
			p->pfrke_mark = 1;
			if (p->pfrke_not != ad.pfra_not) {
				SLIST_INSERT_HEAD(&changeq, p, pfrke_workq);
				ad.pfra_fback = PFR_FB_CHANGED;
				xchange++;
			}
		} else {
			q = pfr_lookup_addr(tmpkt, &ad, 1);
			if (q != NULL) {
				ad.pfra_fback = PFR_FB_DUPLICATE;
				goto _skip;
			}
			p = pfr_create_kentry(&ad);
			if (p == NULL)
				senderr(ENOMEM);
			if (pfr_route_kentry(tmpkt, p)) {
				pfr_destroy_kentry(p);
				ad.pfra_fback = PFR_FB_NONE;
			} else {
				SLIST_INSERT_HEAD(&addq, p, pfrke_workq);
				ad.pfra_fback = PFR_FB_ADDED;
				xadd++;
			}
		}
_skip:
		if (flags & PFR_FLAG_FEEDBACK)
			if (copyout(&ad, addr+i, sizeof(ad)))
				senderr(EFAULT);
	}
	pfr_enqueue_addrs(kt, &delq, &xdel, ENQUEUE_UNMARKED_ONLY);
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
		pfr_insert_kentries(kt, &addq, tzero);
		pfr_remove_kentries(kt, &delq);
		pfr_clstats_kentries(&changeq, tzero, INVERT_NEG_FLAG);
		if (flags & PFR_FLAG_ATOMIC)
			splx(s);
	} else
		pfr_destroy_kentries(&addq);
	if (nadd != NULL)
		*nadd = xadd;
	if (ndel != NULL)
		*ndel = xdel;
	if (nchange != NULL)
		*nchange = xchange;
	if ((flags & PFR_FLAG_FEEDBACK) && *size2)
		*size2 = size+xdel;
	pfr_destroy_ktable(tmpkt, 0);
	return (0);
_bad:
	pfr_destroy_kentries(&addq);
	if (flags & PFR_FLAG_FEEDBACK)
		pfr_reset_feedback(addr, size);
	pfr_destroy_ktable(tmpkt, 0);
	return (rv);
}

int
pfr_tst_addrs(struct pfr_table *tbl, struct pfr_addr *addr, int size,
	int *nmatch, int flags)
{
	struct pfr_ktable	*kt;
	struct pfr_kentry	*p;
	struct pfr_addr		 ad;
	int			 i, xmatch = 0;

	ACCEPT_FLAGS(PFR_FLAG_REPLACE);
	if (pfr_validate_table(tbl, 0))
		return (EINVAL);
	kt = pfr_lookup_table(tbl);
	if (kt == NULL || !(kt->pfrkt_flags & PFR_TFLAG_ACTIVE))
		return (ESRCH);

	for (i = 0; i < size; i++) {
		if (copyin(addr+i, &ad, sizeof(ad)))
			return (EFAULT);
		if (pfr_validate_addr(&ad))
			return (EINVAL);
		if (ADDR_NETWORK(&ad))
			return (EINVAL);
		p = pfr_lookup_addr(kt, &ad, 0);
		if (flags & PFR_FLAG_REPLACE)
			pfr_copyout_addr(&ad, p);
		ad.pfra_fback = (p == NULL) ? PFR_FB_NONE :
		    (p->pfrke_not ? PFR_FB_NOTMATCH : PFR_FB_MATCH);
		if (p != NULL && !p->pfrke_not)
			xmatch++;
		if (copyout(&ad, addr+i, sizeof(ad)))
			return (EFAULT);
	}
	if (nmatch != NULL)
		*nmatch = xmatch;
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
	if (pfr_validate_table(tbl, 0))
		return (EINVAL);
	kt = pfr_lookup_table(tbl);
	if (kt == NULL || !(kt->pfrkt_flags & PFR_TFLAG_ACTIVE))
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
		printf("pfr_get_addrs: corruption detected (%d).",
		    w.pfrw_free);
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

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC); /* XXX PFR_FLAG_CLSTATS disabled */
	if (pfr_validate_table(tbl, 0))
		return (EINVAL);
	kt = pfr_lookup_table(tbl);
	if (kt == NULL || !(kt->pfrkt_flags & PFR_TFLAG_ACTIVE))
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
		pfr_enqueue_addrs(kt, &workq, NULL, 0);
		pfr_clstats_kentries(&workq, tzero, 0);
	}
	if (flags & PFR_FLAG_ATOMIC)
		splx(s);
	if (rv)
		return (rv);

	if (w.pfrw_free) {
		printf("pfr_get_astats: corruption detected (%d).",
		    w.pfrw_free);
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
	if (pfr_validate_table(tbl, 0))
		return (EINVAL);
	kt = pfr_lookup_table(tbl);
	if (kt == NULL || !(kt->pfrkt_flags & PFR_TFLAG_ACTIVE))
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
		pfr_clstats_kentries(&workq, 0, 0);
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
	int i;

	switch (ad->pfra_af) {
	case AF_INET:
		if (ad->pfra_net > 32)
			return (-1);
		break;
	case AF_INET6:
		if (ad->pfra_net > 128)
			return (-1);
		break;
	default:
		return (-1);
	}
	if (ad->pfra_net < 128 &&
		(((caddr_t)ad)[ad->pfra_net/8] & (0xFF >> (ad->pfra_net%8))))
			return (-1);
	for (i = (ad->pfra_net+7)/8; i < sizeof(ad->pfra_u); i++)
		if (((caddr_t)ad)[i])
			return (-1);
	if (ad->pfra_not && ad->pfra_not != 1)
		return (-1);
	if (ad->pfra_fback)
		return (-1);
	return (0);
}

void
pfr_enqueue_addrs(struct pfr_ktable *kt, struct pfr_kentryworkq *workq,
	int *naddr, int sweep)
{
	struct pfr_walktree	w;

	SLIST_INIT(workq);
	bzero(&w, sizeof(w));
	w.pfrw_op = sweep ? PFRW_SWEEP : PFRW_ENQUEUE;
	w.pfrw_workq = workq;
	if (kt->pfrkt_ip4 != NULL)
		if (rn_walktree(kt->pfrkt_ip4, pfr_walktree, &w))
			printf("pfr_enqueue_addrs: IPv4 walktree failed.");
	if (kt->pfrkt_ip6 != NULL)
		if (rn_walktree(kt->pfrkt_ip6, pfr_walktree, &w))
			printf("pfr_enqueue_addrs: IPv6 walktree failed.");
	if (naddr != NULL)
		*naddr = w.pfrw_cnt;
}

void
pfr_mark_addrs(struct pfr_ktable *kt)
{
	struct pfr_walktree	w;

	bzero(&w, sizeof(w));
	w.pfrw_op = PFRW_MARK;
	if (rn_walktree(kt->pfrkt_ip4, pfr_walktree, &w))
		printf("pfr_mark_addrs: IPv4 walktree failed.");
	if (rn_walktree(kt->pfrkt_ip6, pfr_walktree, &w))
		printf("pfr_mark_addrs: IPv6 walktree failed.");
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
pfr_create_kentry(struct pfr_addr *ad)
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
	return (ke);
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

void
pfr_destroy_kentry(struct pfr_kentry *ke)
{
	pool_put(&pfr_kentry_pl, ke);
}

void
pfr_insert_kentries(struct pfr_ktable *kt,
    struct pfr_kentryworkq *workq, long tzero)
{
	struct pfr_kentry	*p;
	int			 rv, n = 0;

	SLIST_FOREACH(p, workq, pfrke_workq) {
		rv = pfr_route_kentry(kt, p);
		if (rv) {
			printf("pfr_insert_kentries: cannot route entry "
			    "(code=%d).\n", rv);
			break;
		}
		p->pfrke_tzero = tzero;
		n++;
	}
	kt->pfrkt_cnt += n;
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
pfr_clstats_kentries(struct pfr_kentryworkq *workq, long tzero, int negchange)
{
	struct pfr_kentry	*p;
	int			 s, n = 0;

	SLIST_FOREACH(p, workq, pfrke_workq) {
		s = splsoftnet();
		if (negchange)
			p->pfrke_not = !p->pfrke_not;
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

	bzero(ke->pfrke_node, sizeof(ke->pfrke_node));
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

	return (rn == NULL ? -1 : 0);
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
	if (ke == NULL)
		return;
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
		if (!(p->pfrkt_flags & PFR_TFLAG_ACTIVE))
			continue;
		SLIST_INSERT_HEAD(&workq, p, pfrkt_workq);
		xdel++;
	}
	if (!(flags & PFR_FLAG_DUMMY)) {
		if (flags & PFR_FLAG_ATOMIC)
			s = splsoftnet();
		pfr_setflags_ktables(&workq, 0, PFR_TFLAG_ACTIVE);
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
	struct pfr_ktableworkq	 addq, changeq;
	struct pfr_ktable	*p, *q, key;
	int			 i, rv, s, xadd = 0;
	long			 tzero = time.tv_sec;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_DUMMY);
	SLIST_INIT(&addq);
	SLIST_INIT(&changeq);
	for (i = 0; i < size; i++) {
		if (copyin(tbl+i, &key.pfrkt_t, sizeof(key.pfrkt_t)))
			senderr(EFAULT);
		if (pfr_validate_table(&key.pfrkt_t, PFR_TFLAG_USRMASK))
			senderr(EINVAL);
		key.pfrkt_flags |= PFR_TFLAG_ACTIVE;
		p = RB_FIND(pfr_ktablehead, &pfr_ktables, &key);
		if (p == NULL) {
			p = pfr_create_ktable(&key.pfrkt_t, tzero);
			if (p == NULL)
				senderr(ENOMEM);
			SLIST_FOREACH(q, &addq, pfrkt_workq) {
				if (!strcmp(p->pfrkt_name, q->pfrkt_name))
					goto _skip;
			}
			SLIST_INSERT_HEAD(&addq, p, pfrkt_workq);
			xadd++;
		} else if (!(p->pfrkt_flags & PFR_TFLAG_ACTIVE)) {
			SLIST_FOREACH(q, &changeq, pfrkt_workq)
				if (!strcmp(key.pfrkt_name, q->pfrkt_name))
					goto _skip;
			SLIST_INSERT_HEAD(&changeq, p, pfrkt_workq);
			xadd++;
		}
_skip:
	}
	if (!(flags & PFR_FLAG_DUMMY)) {
		if (flags & PFR_FLAG_ATOMIC)
			s = splsoftnet();
		pfr_insert_ktables(&addq);
		pfr_setflags_ktables(&changeq, PFR_TFLAG_ACTIVE, 0);
		if (flags & PFR_FLAG_ATOMIC)
			splx(s);
	} else
		 pfr_destroy_ktables(&addq, 0);
	if (nadd != NULL)
		*nadd = xadd;
	return (0);
_bad:
	pfr_destroy_ktables(&addq, 0);
	return (rv);
}

int
pfr_del_tables(struct pfr_table *tbl, int size, int *ndel, int flags)
{
	struct pfr_ktableworkq	 workq;
	struct pfr_ktable	*p, *q, key;
	int			 i, s, xdel = 0;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_DUMMY);
	SLIST_INIT(&workq);
	for (i = 0; i < size; i++) {
		if (copyin(tbl+i, &key.pfrkt_t, sizeof(key.pfrkt_t)))
			return (EFAULT);
		if (pfr_validate_table(&key.pfrkt_t, 0))
			return (EINVAL);
		p = RB_FIND(pfr_ktablehead, &pfr_ktables, &key);
		if (p != NULL && (p->pfrkt_flags & PFR_TFLAG_ACTIVE)) {
			SLIST_FOREACH(q, &workq, pfrkt_workq)
				if (!strcmp(p->pfrkt_name, q->pfrkt_name))
					goto _skip;
			SLIST_INSERT_HEAD(&workq, p, pfrkt_workq);
			xdel++;
		}
_skip:
	}

	if (!(flags & PFR_FLAG_DUMMY)) {
		if (flags & PFR_FLAG_ATOMIC)
			s = splsoftnet();
		pfr_setflags_ktables(&workq, 0, PFR_TFLAG_ACTIVE);
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
		printf("pfr_get_tables: corruption detected (%d).", n);
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

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC); /* XXX PFR_FLAG_CLSTATS disabled */
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
		    flags & PFR_FLAG_ADDRSTOO);
	if (flags & PFR_FLAG_ATOMIC)
		splx(s);
	if (n) {
		printf("pfr_get_tstats: corruption detected (%d).", n);
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

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_CLSTATS+PFR_FLAG_ADDRSTOO);
	SLIST_INIT(&workq);
	for (i = 0; i < size; i++) {
		if (copyin(tbl+i, &key.pfrkt_t, sizeof(key.pfrkt_t)))
			return (EFAULT);
		if (pfr_validate_table(&key.pfrkt_t, 0))
			return (EINVAL);
		p = RB_FIND(pfr_ktablehead, &pfr_ktables, &key);
		if (p != NULL) {
			SLIST_INSERT_HEAD(&workq, p, pfrkt_workq);
			xzero++;
		}
	}
	if (!(flags & PFR_FLAG_DUMMY)) {
		if (flags & PFR_FLAG_ATOMIC)
			s = splsoftnet();
		pfr_clstats_ktables(&workq, tzero, flags & PFR_FLAG_ADDRSTOO);
		if (flags & PFR_FLAG_ATOMIC)
			splx(s);
	}
	if (nzero != NULL)
		*nzero = xzero;
	return (0);
}

int
pfr_set_tflags(struct pfr_table *tbl, int size, int setflag, int clrflag,
	int *nchange, int *ndel, int flags)
{
	struct pfr_ktableworkq	 workq;
	struct pfr_ktable	*p, *q, key;
	int			 i, s, xchange = 0, xdel = 0;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_DUMMY);
	if ((setflag & ~PFR_TFLAG_USRMASK) ||
	    (clrflag & ~PFR_TFLAG_USRMASK) ||
	    (setflag & clrflag))
		return (EINVAL);
	SLIST_INIT(&workq);
	for (i = 0; i < size; i++) {
		if (copyin(tbl+i, &key.pfrkt_t, sizeof(key.pfrkt_t)))
			return (EFAULT);
		if (pfr_validate_table(&key.pfrkt_t, 0))
			return (EINVAL);
		p = RB_FIND(pfr_ktablehead, &pfr_ktables, &key);
		if (p != NULL && (p->pfrkt_flags & PFR_TFLAG_ACTIVE)) {
			if (((p->pfrkt_flags & setflag) == setflag) &&
			    !(p->pfrkt_flags & clrflag))
				goto _skip;
			SLIST_FOREACH(q, &workq, pfrkt_workq)
				if (!strcmp(p->pfrkt_name, q->pfrkt_name))
					goto _skip;
			SLIST_INSERT_HEAD(&workq, p, pfrkt_workq);
			if ((p->pfrkt_flags & PFR_TFLAG_PERSIST) &&
			    (clrflag & PFR_TFLAG_PERSIST) &&
			    !(p->pfrkt_flags & PFR_TFLAG_REFERENCED))
				xdel++;
			else
				xchange++;
		}
_skip:
	}
	if (!(flags & PFR_FLAG_DUMMY)) {
		if (flags & PFR_FLAG_ATOMIC)
			s = splsoftnet();
		pfr_setflags_ktables(&workq, setflag, clrflag);
		if (flags & PFR_FLAG_ATOMIC)
			splx(s);
	}
	if (nchange != NULL)
		*nchange = xchange;
	if (ndel != NULL)
		*ndel = xdel;
	return (0);
}

int
pfr_ina_begin(int *ticket, int *ndel, int flags)
{
	struct pfr_ktableworkq	 workq;
	struct pfr_ktable	*p;
	int			 xdel = 0;

	ACCEPT_FLAGS(PFR_FLAG_DUMMY);
	SLIST_INIT(&workq);
	RB_FOREACH(p, pfr_ktablehead, &pfr_ktables) {
		if (!(p->pfrkt_flags & PFR_TFLAG_INACTIVE))
			continue;
		SLIST_INSERT_HEAD(&workq, p, pfrkt_workq);
		xdel++;
	}
	if (!(flags & PFR_FLAG_DUMMY))
		pfr_setflags_ktables(&workq, 0, PFR_TFLAG_INACTIVE);
	if (ndel != NULL)
		*ndel = xdel;
	if (ticket != NULL && !(flags & PFR_FLAG_DUMMY))
		*ticket = ++pfr_ticket;
	return (0);
}

int
pfr_ina_define(struct pfr_table *tbl, struct pfr_addr *addr, int size,
    int *nadd, int *naddr, int ticket, int flags)
{
	struct pfr_ktableworkq	 tableq;
	struct pfr_kentryworkq	 addrq;
	struct pfr_ktable	*kt, *shadow;
	struct pfr_kentry	*p;
	struct pfr_addr		 ad;
	int			 i, rv, xadd = 0, xaddr = 0;

	ACCEPT_FLAGS(PFR_FLAG_DUMMY|PFR_FLAG_ADDRSTOO);
	if (ticket != pfr_ticket)
		return (EBUSY);
	if (size && !(flags & PFR_FLAG_ADDRSTOO))
		return (EINVAL);
	if (pfr_validate_table(tbl, PFR_TFLAG_USRMASK))
		return (EINVAL);
	tbl->pfrt_flags |= PFR_TFLAG_INACTIVE;
	SLIST_INIT(&tableq);
	kt = RB_FIND(pfr_ktablehead, &pfr_ktables, (struct pfr_ktable *)tbl);
	if (kt == NULL) {
		kt = pfr_create_ktable(tbl, 0);
		if (kt == NULL)
			return (ENOMEM);
		SLIST_INSERT_HEAD(&tableq, kt, pfrkt_workq);
		xadd++;
	} else if (!(kt->pfrkt_flags & PFR_TFLAG_INACTIVE))
		xadd++;
	shadow = pfr_create_ktable(tbl, 0);
	if (shadow == NULL)
		return (ENOMEM);
	SLIST_INIT(&addrq);
	for (i = 0; i < size; i++) {
		if (copyin(addr+i, &ad, sizeof(ad)))
			senderr(EFAULT);
		if (pfr_validate_addr(&ad))
			senderr(EFAULT);
		if (pfr_lookup_addr(shadow, &ad, 1) != NULL)
			continue;
		p = pfr_create_kentry(&ad);
		if (p == NULL)
			senderr(ENOMEM);
		SLIST_INSERT_HEAD(&addrq, p, pfrke_workq);
		xaddr++;
	}
	if (!(flags & PFR_FLAG_ADDRSTOO))
		shadow->pfrkt_cnt = NO_ADDRESSES;
	if (!(flags & PFR_FLAG_DUMMY)) {
		if (kt->pfrkt_shadow != NULL)
			pfr_destroy_ktable(kt->pfrkt_shadow, 1);
		kt->pfrkt_flags |= PFR_TFLAG_INACTIVE;
		pfr_insert_ktables(&tableq);
		kt->pfrkt_shadow = shadow;
		pfr_insert_kentries(shadow, &addrq, 0);
	} else {
		pfr_destroy_ktable(shadow, 0);
		pfr_destroy_ktables(&tableq, 0);
		pfr_destroy_kentries(&addrq);
	}
	if (nadd != NULL)
		*nadd = xadd;
	if (naddr != NULL)
		*naddr = xaddr;
	return (0);
_bad:
	pfr_destroy_ktable(shadow, 0);
	pfr_destroy_ktables(&tableq, 0);
	pfr_destroy_kentries(&addrq);
	return (rv);
}

int
pfr_ina_commit(int ticket, int *nadd, int *nchange, int flags)
{
	struct pfr_ktable	*p;
	struct pfr_ktableworkq	 workq;
	int			 s, xadd = 0, xchange = 0;
	long			 tzero = time.tv_sec;

	ACCEPT_FLAGS(PFR_FLAG_ATOMIC+PFR_FLAG_DUMMY);
	if (ticket != pfr_ticket)
		return (EBUSY);
	pfr_ticket++;

	SLIST_INIT(&workq);
	RB_FOREACH(p, pfr_ktablehead, &pfr_ktables) {
		if (!(p->pfrkt_flags & PFR_TFLAG_INACTIVE))
			continue;
		SLIST_INSERT_HEAD(&workq, p, pfrkt_workq);
		if (p->pfrkt_flags & PFR_TFLAG_ACTIVE)
			xchange++;
		else
			xadd++;
	}

	if (!(flags & PFR_FLAG_DUMMY)) {
		if (flags & PFR_FLAG_ATOMIC)
			s = splsoftnet();
		SLIST_FOREACH(p, &workq, pfrkt_workq)
			pfr_commit_ktable(p, tzero);
		if (flags & PFR_FLAG_ATOMIC)
			splx(s);
	}
	if (nadd != NULL)
		*nadd = xadd;
	if (nchange != NULL)
		*nchange = xchange;

	return (0);
}

void
pfr_commit_ktable(struct pfr_ktable *kt, long tzero)
{
	struct pfr_ktable	*shadow = kt->pfrkt_shadow;
	int			 setflag, clrflag;

	if (shadow->pfrkt_cnt == NO_ADDRESSES) {
		if (!(kt->pfrkt_flags & PFR_TFLAG_ACTIVE))
			pfr_clstats_ktable(kt, tzero, 1);
	} else if (kt->pfrkt_flags & PFR_TFLAG_ACTIVE) {
		/* kt might contain addresses */
		struct pfr_kentryworkq	 addrq, addq, changeq, delq, garbageq;
		struct pfr_kentry	*p, *q, *next;
		struct pfr_addr		 ad;

		pfr_enqueue_addrs(shadow, &addrq, NULL, 0);
		pfr_mark_addrs(kt);
		SLIST_INIT(&addq);
		SLIST_INIT(&changeq);
		SLIST_INIT(&delq);
		SLIST_INIT(&garbageq);
		for (p = SLIST_FIRST(&addrq); p != NULL; p = next) {
			next = SLIST_NEXT(p, pfrke_workq);	/* XXX */
			pfr_copyout_addr(&ad, p);
			q = pfr_lookup_addr(kt, &ad, 1);
			if (q != NULL) {
				if (q->pfrke_not != p->pfrke_not)
					SLIST_INSERT_HEAD(&changeq, q,
					    pfrke_workq);
				q->pfrke_mark = 1;
				SLIST_INSERT_HEAD(&garbageq, p, pfrke_workq);
			} else {
				p->pfrke_tzero = tzero;
				SLIST_INSERT_HEAD(&addq, p, pfrke_workq);
			}
		}
		pfr_enqueue_addrs(kt, &delq, NULL, ENQUEUE_UNMARKED_ONLY);
		pfr_insert_kentries(kt, &addq, tzero);
		pfr_remove_kentries(kt, &delq);
		pfr_clstats_kentries(&changeq, tzero, INVERT_NEG_FLAG);
		pfr_destroy_kentries(&garbageq);
	} else {
		/* kt cannot contain addresses */
		SWAP(struct radix_node_head *, kt->pfrkt_ip4,
		    shadow->pfrkt_ip4);
		SWAP(struct radix_node_head *, kt->pfrkt_ip6,
		    shadow->pfrkt_ip6);
		SWAP(int, kt->pfrkt_cnt, shadow->pfrkt_cnt);
		pfr_clstats_ktable(kt, tzero, 1);
	}
	setflag = shadow->pfrkt_flags & PFR_TFLAG_USRMASK;
	clrflag = (kt->pfrkt_flags & ~setflag) & PFR_TFLAG_USRMASK;
	setflag |= PFR_TFLAG_ACTIVE;
	clrflag |= PFR_TFLAG_INACTIVE;
	pfr_destroy_ktable(shadow, 0);
	kt->pfrkt_shadow = NULL;
	pfr_setflags_ktable(kt, setflag, clrflag);
}

int
pfr_validate_table(struct pfr_table *tbl, int allowedflags)
{
	int i;

	if (!tbl->pfrt_name[0])
		return (-1);
	if (tbl->pfrt_name[PF_TABLE_NAME_SIZE-1])
		return (-1);
	for (i = strlen(tbl->pfrt_name); i < PF_TABLE_NAME_SIZE; i++)
		if (tbl->pfrt_name[i])
			return (-1);
	if (tbl->pfrt_flags & ~allowedflags)
		return (-1);
	return (0);
}

void
pfr_insert_ktables(struct pfr_ktableworkq *workq)
{
	struct pfr_ktable	*p;

	SLIST_FOREACH(p, workq, pfrkt_workq)
		pfr_insert_ktable(p);
}

void
pfr_insert_ktable(struct pfr_ktable *kt)
{
	RB_INSERT(pfr_ktablehead, &pfr_ktables, kt);
	pfr_ktable_cnt++;
}

void
pfr_setflags_ktables(struct pfr_ktableworkq *workq, int setflag, int clrflag)
{
	struct pfr_ktable	*p;

	SLIST_FOREACH(p, workq, pfrkt_workq)
		pfr_setflags_ktable(p, setflag, clrflag);
}

void
pfr_setflags_ktable(struct pfr_ktable *kt, int setflag, int clrflag)
{
	struct pfr_kentryworkq	addrq;
	int			oldf = kt->pfrkt_flags;
	int			newf = (oldf | setflag) & ~clrflag;

	if (!(newf & PFR_TFLAG_REFERENCED) &&
	    !(newf & PFR_TFLAG_PERSIST))
		newf &= ~PFR_TFLAG_ACTIVE;
	if (!(newf & PFR_TFLAG_ACTIVE))
		newf &= ~PFR_TFLAG_USRMASK;
	if (!(newf & PFR_TFLAG_SETMASK)) {
		RB_REMOVE(pfr_ktablehead, &pfr_ktables, kt);
		pfr_destroy_ktable(kt, 1);
		pfr_ktable_cnt--;
		return;
	}
	if (!(newf & PFR_TFLAG_ACTIVE) && kt->pfrkt_cnt) {
		pfr_enqueue_addrs(kt, &addrq, NULL, 0);
		pfr_remove_kentries(kt, &addrq);
	}
	if (!(newf & PFR_TFLAG_INACTIVE) && kt->pfrkt_shadow != NULL) {
		pfr_destroy_ktable(kt->pfrkt_shadow, 1);
		kt->pfrkt_shadow = NULL;
	}
	kt->pfrkt_flags = newf;
}

void
pfr_clstats_ktables(struct pfr_ktableworkq *workq, long tzero, int recurse)
{
	struct pfr_ktable	*p;

	SLIST_FOREACH(p, workq, pfrkt_workq)
		pfr_clstats_ktable(p, tzero, recurse);
}

void
pfr_clstats_ktable(struct pfr_ktable *kt, long tzero, int recurse)
{
	struct pfr_kentryworkq	 addrq;
	int			 s;

	if (recurse) {
		pfr_enqueue_addrs(kt, &addrq, NULL, 0);
		pfr_clstats_kentries(&addrq, tzero, 0);
	}
	s = splsoftnet();
	bzero(kt->pfrkt_packets, sizeof(kt->pfrkt_packets));
	bzero(kt->pfrkt_bytes, sizeof(kt->pfrkt_bytes));
	kt->pfrkt_match = kt->pfrkt_nomatch = 0;
	splx(s);
	kt->pfrkt_tzero = tzero;
}

struct pfr_ktable *
pfr_create_ktable(struct pfr_table *tbl, long tzero)
{
	struct pfr_ktable	*kt;

	kt = pool_get(&pfr_ktable_pl, PR_NOWAIT);
	if (kt == NULL)
		return (NULL);
	bzero(kt, sizeof(*kt));
	kt->pfrkt_t = *tbl;

	if (!rn_inithead((void **)&kt->pfrkt_ip4,
	    offsetof(struct sockaddr_in, sin_addr) * 8) ||
	    !rn_inithead((void **)&kt->pfrkt_ip6,
	    offsetof(struct sockaddr_in6, sin6_addr) * 8)) {
		pfr_destroy_ktable(kt, 0);
		return (NULL);
	}
	kt->pfrkt_tzero = tzero;

	return (kt);
}

void
pfr_destroy_ktables(struct pfr_ktableworkq *workq, int flushaddr)
{
	struct pfr_ktable	*p, *q;

	for (p = SLIST_FIRST(workq); p; p = q) {
		q = SLIST_NEXT(p, pfrkt_workq);
		pfr_destroy_ktable(p, flushaddr);
	}
}

void
pfr_destroy_ktable(struct pfr_ktable *kt, int flushaddr)
{
	struct pfr_kentryworkq	 addrq;

	if (flushaddr) {
		pfr_enqueue_addrs(kt, &addrq, NULL, 0);
		pfr_destroy_kentries(&addrq);
	}
	if (kt->pfrkt_ip4 != NULL)
		free((caddr_t)kt->pfrkt_ip4, M_RTABLE);
	if (kt->pfrkt_ip6 != NULL)
		free((caddr_t)kt->pfrkt_ip6, M_RTABLE);
	if (kt->pfrkt_shadow != NULL)
		pfr_destroy_ktable(kt->pfrkt_shadow, flushaddr);
	pool_put(&pfr_ktable_pl, kt);
}

int
pfr_ktable_compare(struct pfr_ktable *p, struct pfr_ktable *q)
{
	return (strncmp(p->pfrkt_name, q->pfrkt_name, PF_TABLE_NAME_SIZE));
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
pfr_match_addr(struct pfr_ktable *kt, struct pf_addr *a, sa_family_t af)
{
	struct pfr_kentry	*ke = NULL;
	int			 match;

	switch (af) {
	case AF_INET:
		pfr_sin.sin_addr.s_addr = a->addr32[0];
		ke = (struct pfr_kentry *)rn_match(&pfr_sin, kt->pfrkt_ip4);
		break;
	case AF_INET6:
		bcopy(&a, &pfr_sin6.sin6_addr, sizeof(pfr_sin6.sin6_addr));
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
pfr_update_stats(struct pfr_ktable *kt, struct pf_addr *a, sa_family_t af,
    u_int64_t len, int dir_out, int op_pass, int notrule)
{
	struct pfr_kentry	*ke = NULL;

	switch (af) {
	case AF_INET:
		pfr_sin.sin_addr.s_addr = a->addr32[0];
		ke = (struct pfr_kentry *)rn_match(&pfr_sin, kt->pfrkt_ip4);
		break;
	case AF_INET6:
		bcopy(&a, &pfr_sin6.sin6_addr, sizeof(pfr_sin6.sin6_addr));
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

struct pfr_ktable *
pfr_attach_table(char *name)
{
	struct pfr_ktable	*kt;
	struct pfr_table	 tbl;

	bzero(&tbl, sizeof(tbl));
	strlcpy(tbl.pfrt_name, name, sizeof(tbl.pfrt_name));
	kt = pfr_lookup_table(&tbl);
	if (kt == NULL) {
		kt = pfr_create_ktable(&tbl, time.tv_sec);
		if (kt == NULL)
			return NULL;
		pfr_insert_ktable(kt);
	}
	if (!kt->pfrkt_refcnt++)
		pfr_setflags_ktable(kt, PFR_TFLAG_REFERENCED, 0);
	return kt;
}

void
pfr_detach_table(struct pfr_ktable *kt)
{
	if (kt->pfrkt_refcnt <= 0)
		printf("pfr_detach_table: refcount = %d\n",
		    kt->pfrkt_refcnt);
	else if (!--kt->pfrkt_refcnt)
		pfr_setflags_ktable(kt, 0, PFR_TFLAG_REFERENCED);
}

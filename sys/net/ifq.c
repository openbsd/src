/*	$OpenBSD: ifq.c,v 1.1 2015/12/08 10:06:12 dlg Exp $ */

/*
 * Copyright (c) 2015 David Gwynne <dlg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/atomic.h>

#include <net/if.h>
#include <net/if_var.h>

/*
 * priq glue
 */
void		*priq_alloc(void *);
void		 priq_free(void *);
int		 priq_enq(struct ifqueue *, struct mbuf *);
struct mbuf	*priq_deq_begin(struct ifqueue *, void **);
void		 priq_deq_commit(struct ifqueue *, struct mbuf *, void *);
void		 priq_purge(struct ifqueue *, struct mbuf_list *);

const struct ifq_ops priq_ops = {
	priq_alloc,
	priq_free,
	priq_enq,
	priq_deq_begin,
	priq_deq_commit,
	priq_purge,
};

const struct ifq_ops * const ifq_priq_ops = &priq_ops;

/*
 * priq internal structures
 */

struct priq_list {
	struct mbuf		*head;
	struct mbuf		*tail;
};

struct priq {
	struct priq_list	 pq_lists[IFQ_NQUEUES];
};

/*
 * ifqueue serialiser
 */

static inline unsigned int
ifq_enter(struct ifqueue *ifq)
{
	return (atomic_inc_int_nv(&ifq->ifq_serializer) == 1);
}

static inline unsigned int
ifq_leave(struct ifqueue *ifq)
{
	if (atomic_cas_uint(&ifq->ifq_serializer, 1, 0) == 1)
		return (1);

	ifq->ifq_serializer = 1;

	return (0);
}

void
if_start_mpsafe(struct ifnet *ifp)
{
	struct ifqueue *ifq = &ifp->if_snd;

	if (!ifq_enter(ifq))
		return;

	do {
		if (__predict_false(!ISSET(ifp->if_flags, IFF_RUNNING))) {
			ifq->ifq_serializer = 0;
			wakeup_one(&ifq->ifq_serializer);
			return;
		}

		if (ifq_empty(ifq) || ifq_is_oactive(ifq))
			continue;

		ifp->if_start(ifp);

	} while (!ifq_leave(ifq));
}

void
if_start_barrier(struct ifnet *ifp)
{
	struct sleep_state sls;
	struct ifqueue *ifq = &ifp->if_snd;

	/* this should only be called from converted drivers */
	KASSERT(ISSET(ifp->if_xflags, IFXF_MPSAFE));

	/* drivers should only call this on the way down */
	KASSERT(!ISSET(ifp->if_flags, IFF_RUNNING));

	if (ifq->ifq_serializer == 0)
		return;

	if_start_mpsafe(ifp); /* spin the wheel to guarantee a wakeup */
	do {
		sleep_setup(&sls, &ifq->ifq_serializer, PWAIT, "ifbar");
		sleep_finish(&sls, ifq->ifq_serializer != 0);
	} while (ifq->ifq_serializer != 0);
}

/*
 * ifqueue mbuf queue API
 */

void
ifq_init(struct ifqueue *ifq)
{
	mtx_init(&ifq->ifq_mtx, IPL_NET);
	ifq->ifq_drops = 0;

	/* default to priq */
	ifq->ifq_ops = &priq_ops;
	ifq->ifq_q = priq_ops.ifqop_alloc(NULL);

	ifq->ifq_serializer = 0;
	ifq->ifq_len = 0;

	if (ifq->ifq_maxlen == 0)
		ifq_set_maxlen(ifq, IFQ_MAXLEN);
}

void
ifq_attach(struct ifqueue *ifq, const struct ifq_ops *newops, void *opsarg)
{
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct mbuf_list free_ml = MBUF_LIST_INITIALIZER();
	struct mbuf *m;
	const struct ifq_ops *oldops;
	void *newq, *oldq;

	newq = newops->ifqop_alloc(opsarg);

	mtx_enter(&ifq->ifq_mtx);
	ifq->ifq_ops->ifqop_purge(ifq, &ml);
	ifq->ifq_len = 0;

	oldops = ifq->ifq_ops;
	oldq = ifq->ifq_q;

	ifq->ifq_ops = newops;
	ifq->ifq_q = newq;

	while ((m = ml_dequeue(&ml)) != NULL) {
		if (ifq->ifq_ops->ifqop_enq(ifq, m) != 0) {
			ifq->ifq_drops++;
			ml_enqueue(&free_ml, m);
		} else
			ifq->ifq_len++;
	}
	mtx_leave(&ifq->ifq_mtx);

	oldops->ifqop_free(oldq);

	ml_purge(&free_ml);
}

void
ifq_destroy(struct ifqueue *ifq)
{
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();

	/* don't need to lock because this is the last use of the ifq */

	ifq->ifq_ops->ifqop_purge(ifq, &ml);
	ifq->ifq_ops->ifqop_free(ifq->ifq_q);

	ml_purge(&ml);
}

int
ifq_enqueue_try(struct ifqueue *ifq, struct mbuf *m)
{
	int rv;

	mtx_enter(&ifq->ifq_mtx);
	rv = ifq->ifq_ops->ifqop_enq(ifq, m);
	if (rv == 0)
		ifq->ifq_len++;
	else
		ifq->ifq_drops++;
	mtx_leave(&ifq->ifq_mtx);

	return (rv);
}

int
ifq_enqueue(struct ifqueue *ifq, struct mbuf *m)
{
	int err;

	err = ifq_enqueue_try(ifq, m);
	if (err != 0)
		m_freem(m);

	return (err);
}

struct mbuf *
ifq_deq_begin(struct ifqueue *ifq)
{
	struct mbuf *m = NULL;
	void *cookie;

	mtx_enter(&ifq->ifq_mtx);
	if (ifq->ifq_len == 0 ||
	    (m = ifq->ifq_ops->ifqop_deq_begin(ifq, &cookie)) == NULL) {
		mtx_leave(&ifq->ifq_mtx);
		return (NULL);
	}

	m->m_pkthdr.ph_cookie = cookie;

	return (m);
}

void
ifq_deq_commit(struct ifqueue *ifq, struct mbuf *m)
{
	void *cookie;

	KASSERT(m != NULL);
	cookie = m->m_pkthdr.ph_cookie;

	ifq->ifq_ops->ifqop_deq_commit(ifq, m, cookie);
	ifq->ifq_len--;
	mtx_leave(&ifq->ifq_mtx);
}

void
ifq_deq_rollback(struct ifqueue *ifq, struct mbuf *m)
{
	KASSERT(m != NULL);

	mtx_leave(&ifq->ifq_mtx);
}

struct mbuf *
ifq_dequeue(struct ifqueue *ifq)
{
	struct mbuf *m;

	m = ifq_deq_begin(ifq);
	if (m == NULL)
		return (NULL);

	ifq_deq_commit(ifq, m);

	return (m);
}

unsigned int
ifq_purge(struct ifqueue *ifq)
{
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	unsigned int rv;

	mtx_enter(&ifq->ifq_mtx);
	ifq->ifq_ops->ifqop_purge(ifq, &ml);
	rv = ifq->ifq_len;
	ifq->ifq_len = 0;
	ifq->ifq_drops += rv;
	mtx_leave(&ifq->ifq_mtx);

	KASSERT(rv == ml_len(&ml));

	ml_purge(&ml);

	return (rv);
}

void *
ifq_q_enter(struct ifqueue *ifq, const struct ifq_ops *ops)
{
	mtx_enter(&ifq->ifq_mtx);
	if (ifq->ifq_ops == ops)
		return (ifq->ifq_q);

	mtx_leave(&ifq->ifq_mtx);

	return (NULL);
}

void
ifq_q_leave(struct ifqueue *ifq, void *q)
{
	KASSERT(q == ifq->ifq_q);
	mtx_leave(&ifq->ifq_mtx);
}

/*
 * priq implementation
 */

void *
priq_alloc(void *null)
{
	return (malloc(sizeof(struct priq), M_DEVBUF, M_WAITOK | M_ZERO));
}

void
priq_free(void *pq)
{
	free(pq, M_DEVBUF, sizeof(struct priq));
}

int
priq_enq(struct ifqueue *ifq, struct mbuf *m)
{
	struct priq *pq;
	struct priq_list *pl;

	if (ifq_len(ifq) >= ifq->ifq_maxlen)
		return (ENOBUFS);

	pq = ifq->ifq_q;
	KASSERT(m->m_pkthdr.pf.prio <= IFQ_MAXPRIO);
	pl = &pq->pq_lists[m->m_pkthdr.pf.prio];

	m->m_nextpkt = NULL;
	if (pl->tail == NULL)
		pl->head = m;
	else
		pl->tail->m_nextpkt = m;
	pl->tail = m;

	return (0);
}

struct mbuf *
priq_deq_begin(struct ifqueue *ifq, void **cookiep)
{
	struct priq *pq = ifq->ifq_q;
	struct priq_list *pl;
	unsigned int prio = nitems(pq->pq_lists);
	struct mbuf *m;

	do {
		pl = &pq->pq_lists[--prio];
		m = pl->head;
		if (m != NULL) {
			*cookiep = pl;
			return (m);
		}
	} while (prio > 0);

	return (NULL);
}

void
priq_deq_commit(struct ifqueue *ifq, struct mbuf *m, void *cookie)
{
	struct priq_list *pl = cookie;

	KASSERT(pl->head == m);

	pl->head = m->m_nextpkt;
	m->m_nextpkt = NULL;

	if (pl->head == NULL)
		pl->tail = NULL;
}

void
priq_purge(struct ifqueue *ifq, struct mbuf_list *ml)
{
	struct priq *pq = ifq->ifq_q;
	struct priq_list *pl;
	unsigned int prio = nitems(pq->pq_lists);
	struct mbuf *m, *n;

	do {
		pl = &pq->pq_lists[--prio];

		for (m = pl->head; m != NULL; m = n) {
			n = m->m_nextpkt;
			ml_enqueue(ml, m);
		}

		pl->head = pl->tail = NULL;
	} while (prio > 0);
}

/*	$OpenBSD: ifq.c,v 1.11 2017/05/03 20:55:29 mikeb Exp $ */

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

#include <net/if.h>
#include <net/if_var.h>

/*
 * priq glue
 */
unsigned int	 priq_idx(unsigned int, const struct mbuf *);
struct mbuf	*priq_enq(struct ifqueue *, struct mbuf *);
struct mbuf	*priq_deq_begin(struct ifqueue *, void **);
void		 priq_deq_commit(struct ifqueue *, struct mbuf *, void *);
void		 priq_purge(struct ifqueue *, struct mbuf_list *);

void		*priq_alloc(unsigned int, void *);
void		 priq_free(unsigned int, void *);

const struct ifq_ops priq_ops = {
	priq_idx,
	priq_enq,
	priq_deq_begin,
	priq_deq_commit,
	priq_purge,
	priq_alloc,
	priq_free,
};

const struct ifq_ops * const ifq_priq_ops = &priq_ops;

/*
 * priq internal structures
 */

struct priq {
	struct mbuf_list	 pq_lists[IFQ_NQUEUES];
};

/*
 * ifqueue serialiser
 */

void	ifq_start_task(void *);
void	ifq_restart_task(void *);
void	ifq_barrier_task(void *);

#define TASK_ONQUEUE 0x1

void
ifq_serialize(struct ifqueue *ifq, struct task *t)
{
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct task work;

	if (ISSET(t->t_flags, TASK_ONQUEUE))
		return;

	mtx_enter(&ifq->ifq_task_mtx);
	if (!ISSET(t->t_flags, TASK_ONQUEUE)) {
		SET(t->t_flags, TASK_ONQUEUE);
		TAILQ_INSERT_TAIL(&ifq->ifq_task_list, t, t_entry);
	}

	if (ifq->ifq_serializer == NULL) {
		ifq->ifq_serializer = curcpu();

		while ((t = TAILQ_FIRST(&ifq->ifq_task_list)) != NULL) {
			TAILQ_REMOVE(&ifq->ifq_task_list, t, t_entry);
			CLR(t->t_flags, TASK_ONQUEUE);
			work = *t; /* copy to caller to avoid races */

			mtx_leave(&ifq->ifq_task_mtx);

			(*work.t_func)(work.t_arg);

			mtx_enter(&ifq->ifq_task_mtx);
		}

		/*
		 * ifq->ifq_free is only modified by dequeue, which
		 * is only called from within this serialization
		 * context. it is therefore safe to access and modify
		 * here without taking ifq->ifq_mtx.
		 */
		ml = ifq->ifq_free;
		ml_init(&ifq->ifq_free);

		ifq->ifq_serializer = NULL;
	}
	mtx_leave(&ifq->ifq_task_mtx);

	ml_purge(&ml);
}

int
ifq_is_serialized(struct ifqueue *ifq)
{
	return (ifq->ifq_serializer == curcpu());
}

void
ifq_start_task(void *p)
{
	struct ifqueue *ifq = p;
	struct ifnet *ifp = ifq->ifq_if;

	if (!ISSET(ifp->if_flags, IFF_RUNNING) ||
	    ifq_empty(ifq) || ifq_is_oactive(ifq))
		return;

	ifp->if_qstart(ifq);
}

void
ifq_restart_task(void *p)
{
	struct ifqueue *ifq = p;
	struct ifnet *ifp = ifq->ifq_if;

	ifq_clr_oactive(ifq);
	ifp->if_qstart(ifq);
}

void
ifq_barrier(struct ifqueue *ifq)
{
	struct sleep_state sls;
	unsigned int notdone = 1;
	struct task t = TASK_INITIALIZER(ifq_barrier_task, &notdone);

	/* this should only be called from converted drivers */
	KASSERT(ISSET(ifq->ifq_if->if_xflags, IFXF_MPSAFE));

	if (ifq->ifq_serializer == NULL)
		return;

	ifq_serialize(ifq, &t);

	while (notdone) {
		sleep_setup(&sls, &notdone, PWAIT, "ifqbar");
		sleep_finish(&sls, notdone);
	}
}

void
ifq_barrier_task(void *p)
{
	unsigned int *notdone = p;

	*notdone = 0;
	wakeup_one(notdone);
}

/*
 * ifqueue mbuf queue API
 */

void
ifq_init(struct ifqueue *ifq, struct ifnet *ifp, unsigned int idx)
{
	ifq->ifq_if = ifp;
	ifq->ifq_softc = NULL;

	mtx_init(&ifq->ifq_mtx, IPL_NET);
	ifq->ifq_qdrops = 0;

	/* default to priq */
	ifq->ifq_ops = &priq_ops;
	ifq->ifq_q = priq_ops.ifqop_alloc(idx, NULL);

	ml_init(&ifq->ifq_free);
	ifq->ifq_len = 0;

	ifq->ifq_packets = 0;
	ifq->ifq_bytes = 0;
	ifq->ifq_qdrops = 0;
	ifq->ifq_errors = 0;
	ifq->ifq_mcasts = 0;

	mtx_init(&ifq->ifq_task_mtx, IPL_NET);
	TAILQ_INIT(&ifq->ifq_task_list);
	ifq->ifq_serializer = NULL;

	task_set(&ifq->ifq_start, ifq_start_task, ifq);
	task_set(&ifq->ifq_restart, ifq_restart_task, ifq);

	if (ifq->ifq_maxlen == 0)
		ifq_set_maxlen(ifq, IFQ_MAXLEN);

	ifq->ifq_idx = idx;
}

void
ifq_attach(struct ifqueue *ifq, const struct ifq_ops *newops, void *opsarg)
{
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct mbuf_list free_ml = MBUF_LIST_INITIALIZER();
	struct mbuf *m;
	const struct ifq_ops *oldops;
	void *newq, *oldq;

	newq = newops->ifqop_alloc(ifq->ifq_idx, opsarg);

	mtx_enter(&ifq->ifq_mtx);
	ifq->ifq_ops->ifqop_purge(ifq, &ml);
	ifq->ifq_len = 0;

	oldops = ifq->ifq_ops;
	oldq = ifq->ifq_q;

	ifq->ifq_ops = newops;
	ifq->ifq_q = newq;

	while ((m = ml_dequeue(&ml)) != NULL) {
		m = ifq->ifq_ops->ifqop_enq(ifq, m);
		if (m != NULL) {
			ifq->ifq_qdrops++;
			ml_enqueue(&free_ml, m);
		} else
			ifq->ifq_len++;
	}
	mtx_leave(&ifq->ifq_mtx);

	oldops->ifqop_free(ifq->ifq_idx, oldq);

	ml_purge(&free_ml);
}

void
ifq_destroy(struct ifqueue *ifq)
{
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();

	/* don't need to lock because this is the last use of the ifq */

	ifq->ifq_ops->ifqop_purge(ifq, &ml);
	ifq->ifq_ops->ifqop_free(ifq->ifq_idx, ifq->ifq_q);

	ml_purge(&ml);
}

int
ifq_enqueue(struct ifqueue *ifq, struct mbuf *m)
{
	struct mbuf *dm;

	mtx_enter(&ifq->ifq_mtx);
	dm = ifq->ifq_ops->ifqop_enq(ifq, m);
	if (dm != m) {
		ifq->ifq_packets++;
		ifq->ifq_bytes += m->m_pkthdr.len;
		if (ISSET(m->m_flags, M_MCAST))
			ifq->ifq_mcasts++;
	}

	if (dm == NULL)
		ifq->ifq_len++;
	else
		ifq->ifq_qdrops++;
	mtx_leave(&ifq->ifq_mtx);

	if (dm != NULL)
		m_freem(dm);

	return (dm == m ? ENOBUFS : 0);
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
	ifq->ifq_qdrops += rv;
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

void
ifq_mfreem(struct ifqueue *ifq, struct mbuf *m)
{
	IFQ_ASSERT_SERIALIZED(ifq);

	ifq->ifq_len--;
	ifq->ifq_qdrops++;
	ml_enqueue(&ifq->ifq_free, m);
}

void
ifq_mfreeml(struct ifqueue *ifq, struct mbuf_list *ml)
{
	IFQ_ASSERT_SERIALIZED(ifq);

	ifq->ifq_len -= ml_len(ml);
	ifq->ifq_qdrops += ml_len(ml);
	ml_enlist(&ifq->ifq_free, ml);
}

/*
 * priq implementation
 */

unsigned int
priq_idx(unsigned int nqueues, const struct mbuf *m)
{
	unsigned int flow = 0;

	if (ISSET(m->m_pkthdr.ph_flowid, M_FLOWID_VALID))
		flow = m->m_pkthdr.ph_flowid & M_FLOWID_MASK;

	return (flow % nqueues);
}

void *
priq_alloc(unsigned int idx, void *null)
{
	struct priq *pq;
	int i;

	pq = malloc(sizeof(struct priq), M_DEVBUF, M_WAITOK);
	for (i = 0; i < IFQ_NQUEUES; i++)
		ml_init(&pq->pq_lists[i]);
	return (pq);
}

void
priq_free(unsigned int idx, void *pq)
{
	free(pq, M_DEVBUF, sizeof(struct priq));
}

struct mbuf *
priq_enq(struct ifqueue *ifq, struct mbuf *m)
{
	struct priq *pq;
	struct mbuf_list *pl;
	struct mbuf *n = NULL;
	unsigned int prio;

	pq = ifq->ifq_q;
	KASSERT(m->m_pkthdr.pf.prio <= IFQ_MAXPRIO);

	/* Find a lower priority queue to drop from */
	if (ifq_len(ifq) >= ifq->ifq_maxlen) {
		for (prio = 0; prio < m->m_pkthdr.pf.prio; prio++) {
			pl = &pq->pq_lists[prio];
			if (ml_len(pl) > 0) {
				n = ml_dequeue(pl);
				goto enqueue;
			}
		}
		/*
		 * There's no lower priority queue that we can
		 * drop from so don't enqueue this one.
		 */
		return (m);
	}

 enqueue:
	pl = &pq->pq_lists[m->m_pkthdr.pf.prio];
	ml_enqueue(pl, m);

	return (n);
}

struct mbuf *
priq_deq_begin(struct ifqueue *ifq, void **cookiep)
{
	struct priq *pq = ifq->ifq_q;
	struct mbuf_list *pl;
	unsigned int prio = nitems(pq->pq_lists);
	struct mbuf *m;

	do {
		pl = &pq->pq_lists[--prio];
		m = MBUF_LIST_FIRST(pl);
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
	struct mbuf_list *pl = cookie;

	KASSERT(MBUF_LIST_FIRST(pl) == m);

	ml_dequeue(pl);
}

void
priq_purge(struct ifqueue *ifq, struct mbuf_list *ml)
{
	struct priq *pq = ifq->ifq_q;
	struct mbuf_list *pl;
	unsigned int prio = nitems(pq->pq_lists);

	do {
		pl = &pq->pq_lists[--prio];
		ml_enlist(ml, pl);
	} while (prio > 0);
}

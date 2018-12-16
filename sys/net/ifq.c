/*	$OpenBSD: ifq.c,v 1.25 2018/12/16 03:36:02 dlg Exp $ */

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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_var.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

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

void
ifq_serialize(struct ifqueue *ifq, struct task *t)
{
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

		ifq->ifq_serializer = NULL;
	}
	mtx_leave(&ifq->ifq_task_mtx);
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
	struct cond c = COND_INITIALIZER();
	struct task t = TASK_INITIALIZER(ifq_barrier_task, &c);

	if (ifq->ifq_serializer == NULL)
		return;

	ifq_serialize(ifq, &t);

	cond_wait(&c, "ifqbar");
}

void
ifq_barrier_task(void *p)
{
	struct cond *c = p;

	cond_signal(c);
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

void
ifq_add_data(struct ifqueue *ifq, struct if_data *data)
{
	mtx_enter(&ifq->ifq_mtx);
	data->ifi_opackets += ifq->ifq_packets;
	data->ifi_obytes += ifq->ifq_bytes;
	data->ifi_oqdrops += ifq->ifq_qdrops;
	data->ifi_omcasts += ifq->ifq_mcasts;
	/* ifp->if_data.ifi_oerrors */
	mtx_leave(&ifq->ifq_mtx);
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

static inline void
ifq_deq_enter(struct ifqueue *ifq)
{
	mtx_enter(&ifq->ifq_mtx);
}

static inline void
ifq_deq_leave(struct ifqueue *ifq)
{
	struct mbuf_list ml;

	ml = ifq->ifq_free;
	ml_init(&ifq->ifq_free);

	mtx_leave(&ifq->ifq_mtx);

	if (!ml_empty(&ml))
		ml_purge(&ml);
}

struct mbuf *
ifq_deq_begin(struct ifqueue *ifq)
{
	struct mbuf *m = NULL;
	void *cookie;

	ifq_deq_enter(ifq);
	if (ifq->ifq_len == 0 ||
	    (m = ifq->ifq_ops->ifqop_deq_begin(ifq, &cookie)) == NULL) {
		ifq_deq_leave(ifq);
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
	ifq_deq_leave(ifq);
}

void
ifq_deq_rollback(struct ifqueue *ifq, struct mbuf *m)
{
	KASSERT(m != NULL);

	ifq_deq_leave(ifq);
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

int
ifq_hdatalen(struct ifqueue *ifq)
{
	struct mbuf *m;
	int len = 0;

	m = ifq_deq_begin(ifq);
	if (m != NULL) {
		len = m->m_pkthdr.len;
		ifq_deq_commit(ifq, m);
	}

	return (len);
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
	MUTEX_ASSERT_LOCKED(&ifq->ifq_mtx);

	ifq->ifq_len--;
	ifq->ifq_qdrops++;
	ml_enqueue(&ifq->ifq_free, m);
}

void
ifq_mfreeml(struct ifqueue *ifq, struct mbuf_list *ml)
{
	MUTEX_ASSERT_LOCKED(&ifq->ifq_mtx);

	ifq->ifq_len -= ml_len(ml);
	ifq->ifq_qdrops += ml_len(ml);
	ml_enlist(&ifq->ifq_free, ml);
}

/*
 * ifiq
 */

static void	ifiq_process(void *);

void
ifiq_init(struct ifiqueue *ifiq, struct ifnet *ifp, unsigned int idx)
{
	ifiq->ifiq_if = ifp;
	ifiq->ifiq_softnet = net_tq(ifp->if_index); /* + idx */
	ifiq->ifiq_softc = NULL;

	mtx_init(&ifiq->ifiq_mtx, IPL_NET);
	ml_init(&ifiq->ifiq_ml);
	task_set(&ifiq->ifiq_task, ifiq_process, ifiq);

	ifiq->ifiq_qdrops = 0;
	ifiq->ifiq_packets = 0;
	ifiq->ifiq_bytes = 0;
	ifiq->ifiq_qdrops = 0;
	ifiq->ifiq_errors = 0;

	ifiq->ifiq_idx = idx;
}

void
ifiq_destroy(struct ifiqueue *ifiq)
{
	if (!task_del(ifiq->ifiq_softnet, &ifiq->ifiq_task)) {
		NET_ASSERT_UNLOCKED();
		taskq_barrier(ifiq->ifiq_softnet);
	}

	/* don't need to lock because this is the last use of the ifiq */
	ml_purge(&ifiq->ifiq_ml);
}

int
ifiq_input(struct ifiqueue *ifiq, struct mbuf_list *ml, unsigned int cwm)
{
	struct ifnet *ifp = ifiq->ifiq_if;
	struct mbuf *m;
	uint64_t packets;
	uint64_t bytes = 0;
#if NBPFILTER > 0
	caddr_t if_bpf;
#endif
	int rv = 1;

	if (ml_empty(ml))
		return (0);

	MBUF_LIST_FOREACH(ml, m) {
		m->m_pkthdr.ph_ifidx = ifp->if_index;
		m->m_pkthdr.ph_rtableid = ifp->if_rdomain;
		bytes += m->m_pkthdr.len;
	}
	packets = ml_len(ml);

#if NBPFILTER > 0
	if_bpf = ifp->if_bpf;
	if (if_bpf) {
		struct mbuf_list ml0 = *ml;

		ml_init(ml);

		while ((m = ml_dequeue(&ml0)) != NULL) {
			if (bpf_mtap_ether(if_bpf, m, BPF_DIRECTION_IN))
				m_freem(m);
			else
				ml_enqueue(ml, m);
		}

		if (ml_empty(ml)) {
			mtx_enter(&ifiq->ifiq_mtx);
			ifiq->ifiq_packets += packets;
			ifiq->ifiq_bytes += bytes;
			mtx_leave(&ifiq->ifiq_mtx);

			return (0);
		}
	}
#endif

	mtx_enter(&ifiq->ifiq_mtx);
	ifiq->ifiq_packets += packets;
	ifiq->ifiq_bytes += bytes;

	if (ifiq_len(ifiq) >= cwm * 5)
		ifiq->ifiq_qdrops += ml_len(ml);
	else {
		rv = (ifiq_len(ifiq) >= cwm * 3);
		ml_enlist(&ifiq->ifiq_ml, ml);
	}
	mtx_leave(&ifiq->ifiq_mtx);

	if (ml_empty(ml))
		task_add(ifiq->ifiq_softnet, &ifiq->ifiq_task);
	else
		ml_purge(ml);

	return (rv);
}

void
ifiq_add_data(struct ifiqueue *ifiq, struct if_data *data)
{
	mtx_enter(&ifiq->ifiq_mtx);
	data->ifi_ipackets += ifiq->ifiq_packets;
	data->ifi_ibytes += ifiq->ifiq_bytes;
	data->ifi_iqdrops += ifiq->ifiq_qdrops;
	mtx_leave(&ifiq->ifiq_mtx);
}

void
ifiq_barrier(struct ifiqueue *ifiq)
{
	if (!task_del(ifiq->ifiq_softnet, &ifiq->ifiq_task))
		taskq_barrier(ifiq->ifiq_softnet);
}

int
ifiq_enqueue(struct ifiqueue *ifiq, struct mbuf *m)
{
	mtx_enter(&ifiq->ifiq_mtx);
	ml_enqueue(&ifiq->ifiq_ml, m);
	mtx_leave(&ifiq->ifiq_mtx);

	task_add(ifiq->ifiq_softnet, &ifiq->ifiq_task);

	return (0);
}

static void
ifiq_process(void *arg)
{
	struct ifiqueue *ifiq = arg;
	struct mbuf_list ml;

	if (ifiq_empty(ifiq))
		return;

	mtx_enter(&ifiq->ifiq_mtx);
	ml = ifiq->ifiq_ml;
	ml_init(&ifiq->ifiq_ml);
	mtx_leave(&ifiq->ifiq_mtx);

	if_input_process(ifiq->ifiq_if, &ml);
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

/*	$OpenBSD: altq_wfq.c,v 1.4 2002/11/26 01:03:34 henning Exp $	*/
/*	$KAME: altq_wfq.c,v 1.7 2000/12/14 08:12:46 thorpej Exp $	*/

/*
 * Copyright (C) 1997-2000
 *	Sony Computer Science Laboratories Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 *  March 27, 1997.  Written by Hiroshi Kyusojin of Keio University
 *  (kyu@mt.cs.keio.ac.jp).
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_types.h>
#include <netinet/in.h>

#include <altq/altq.h>
#include <altq/altq_conf.h>
#include <altq/altq_wfq.h>

/*
#define	WFQ_DEBUG
*/

static int		wfq_setenable(struct wfq_interface *, int);
static int		wfq_ifattach(struct wfq_interface *);
static int		wfq_ifdetach(struct wfq_interface *);
static int		wfq_ifenqueue(struct ifaltq *, struct mbuf *,
				      struct altq_pktattr *);
static u_long		wfq_hash(struct flowinfo *, int);
static __inline u_long	wfq_hashbydstaddr(struct flowinfo *, int);
static __inline u_long	wfq_hashbysrcport(struct flowinfo *, int);
static wfq		*wfq_maxqueue(wfq_state_t *);
static struct mbuf	*wfq_ifdequeue(struct ifaltq *, int);
static int		wfq_getqid(struct wfq_getqid *);
static int		wfq_setweight(struct wfq_setweight *);
static int		wfq_getstats(struct wfq_getstats *);
static int		wfq_config(struct wfq_conf *);
static int		wfq_request(struct ifaltq *, int, void *);
static int		wfq_flush(struct ifaltq *);
static void		*wfq_classify(void *, struct mbuf *, int);

/* global value : pointer to wfq queue list */
static wfq_state_t *wfq_list = NULL;

static int
wfq_setenable(ifacep, flag)
	struct wfq_interface *ifacep;
	int flag;
{
	wfq_state_t *wfqp;
	int error = 0;

	if ((wfqp = altq_lookup(ifacep->wfq_ifacename, ALTQT_WFQ)) == NULL)
		return (EBADF);

	switch(flag){
	case ENABLE:
		error = altq_enable(wfqp->ifq);
		break;
	case DISABLE:
		error = altq_disable(wfqp->ifq);
		break;
	}
	return error;
}


static int
wfq_ifattach(ifacep)
	struct wfq_interface *ifacep;
{
	int error = 0, i;
	struct ifnet *ifp;
	wfq_state_t *new_wfqp;
	wfq *queue;

	if ((ifp = ifunit(ifacep->wfq_ifacename)) == NULL) {
#ifdef WFQ_DEBUG
		printf("wfq_ifattach()...no ifp found\n");
#endif
		return (ENXIO);
	}

	if (!ALTQ_IS_READY(&ifp->if_snd)) {
#ifdef WFQ_DEBUG
		printf("wfq_ifattach()...altq is not ready\n");
#endif
		return (ENXIO);
	}

	/* allocate and initialize wfq_state_t */
	MALLOC(new_wfqp, wfq_state_t *, sizeof(wfq_state_t),
	       M_DEVBUF, M_WAITOK);
	if (new_wfqp == NULL)
		return (ENOMEM);
	bzero(new_wfqp, sizeof(wfq_state_t));
	MALLOC(queue, wfq *, sizeof(wfq) * DEFAULT_QSIZE,
	       M_DEVBUF, M_WAITOK);
	if (queue == NULL) {
		FREE(new_wfqp, M_DEVBUF);
		return (ENOMEM);
	}
	bzero(queue, sizeof(wfq) * DEFAULT_QSIZE);

	/* keep the ifq */
	new_wfqp->ifq = &ifp->if_snd;
	new_wfqp->nums = DEFAULT_QSIZE;
	new_wfqp->hwm = HWM;
	new_wfqp->bytes = 0;
	new_wfqp->rrp = NULL;
	new_wfqp->queue = queue;
	new_wfqp->hash_func = wfq_hashbydstaddr;
	new_wfqp->fbmask = FIMB4_DADDR;

	for (i = 0; i < new_wfqp->nums; i++, queue++) {
		queue->next = queue->prev = NULL;
		queue->head = queue->tail = NULL;
		queue->bytes = queue->quota = 0;
		queue->weight = 100;
	}

	/*
	 * set WFQ to this ifnet structure.
	 */
	if ((error = altq_attach(&ifp->if_snd, ALTQT_WFQ, new_wfqp,
				 wfq_ifenqueue, wfq_ifdequeue, wfq_request,
				 new_wfqp, wfq_classify)) != 0) {
		FREE(queue, M_DEVBUF);
		FREE(new_wfqp, M_DEVBUF);
		return (error);
	}

	new_wfqp->next = wfq_list;
	wfq_list = new_wfqp;

	return (error);
}


static int
wfq_ifdetach(ifacep)
	struct wfq_interface *ifacep;
{
	int		error = 0;
	wfq_state_t	*wfqp;

	if ((wfqp = altq_lookup(ifacep->wfq_ifacename, ALTQT_WFQ)) == NULL)
		return (EBADF);

	/* free queued mbuf */
	wfq_flush(wfqp->ifq);

	/* remove WFQ from the ifnet structure. */
	(void)altq_disable(wfqp->ifq);
	(void)altq_detach(wfqp->ifq);

	/* remove from the wfqstate list */
	if (wfq_list == wfqp)
		wfq_list = wfqp->next;
	else {
		wfq_state_t *wp = wfq_list;
		do {
			if (wp->next == wfqp) {
				wp->next = wfqp->next;
				break;
			}
		} while ((wp = wp->next) != NULL);
	}

	/* deallocate wfq_state_t */
	FREE(wfqp->queue, M_DEVBUF);
	FREE(wfqp, M_DEVBUF);
	return (error);
}

static int
wfq_request(ifq, req, arg)
	struct ifaltq *ifq;
	int req;
	void *arg;
{
	wfq_state_t *wfqp = (wfq_state_t *)ifq->altq_disc;

	switch (req) {
	case ALTRQ_PURGE:
		wfq_flush(wfqp->ifq);
		break;
	}
	return (0);
}


static int
wfq_flush(ifq)
	struct ifaltq *ifq;
{
	struct mbuf *mp;

	while ((mp = wfq_ifdequeue(ifq, ALTDQ_REMOVE)) != NULL)
		m_freem(mp);
	if (ALTQ_IS_ENABLED(ifq))
		ifq->ifq_len = 0;
	return 0;
}

static void *
wfq_classify(clfier, m, af)
	void *clfier;
	struct mbuf *m;
	int af;
{
	wfq_state_t *wfqp = (wfq_state_t *)clfier;
	struct flowinfo flow;

	altq_extractflow(m, af, &flow, wfqp->fbmask);
	return (&wfqp->queue[(*wfqp->hash_func)(&flow, wfqp->nums)]);
}

static int
wfq_ifenqueue(ifq, mp, pktattr)
	struct ifaltq *ifq;
	struct mbuf *mp;
	struct altq_pktattr *pktattr;
{
	wfq_state_t *wfqp;
	wfq *queue;
	int byte, error = 0;

	wfqp = (wfq_state_t *)ifq->altq_disc;
	mp->m_nextpkt = NULL;

	/* grab a queue selected by classifier */
	if (pktattr == NULL || (queue = pktattr->pattr_class) == NULL)
		queue = &wfqp->queue[0];

	if (queue->tail == NULL)
		queue->head = mp;
	else
		queue->tail->m_nextpkt = mp;
	queue->tail = mp;
	byte = mp->m_pkthdr.len;
	queue->bytes += byte;
	wfqp->bytes += byte;
	ifq->ifq_len++;

	if (queue->next == NULL) {
		/* this queue gets active. add the queue to the active list */
		if (wfqp->rrp == NULL){
			/* no queue in the active list */
			queue->next = queue->prev = queue;
			wfqp->rrp = queue;
			WFQ_ADDQUOTA(queue);
		} else {
			/* insert the queue at the tail of the active list */
			queue->prev = wfqp->rrp->prev;
			wfqp->rrp->prev->next = queue;
			wfqp->rrp->prev = queue;
			queue->next = wfqp->rrp;
			queue->quota = 0;
		}
	}

	/* check overflow. if the total size exceeds the high water mark,
	   drop packets from the longest queue. */
	while (wfqp->bytes > wfqp->hwm) {
		wfq *drop_queue = wfq_maxqueue(wfqp);

		/* drop the packet at the head. */
		mp = drop_queue->head;
		if ((drop_queue->head = mp->m_nextpkt) == NULL)
			drop_queue->tail = NULL;
		mp->m_nextpkt = NULL;
		byte = mp->m_pkthdr.len;
		drop_queue->bytes -= byte;
		PKTCNTR_ADD(&drop_queue->drop_cnt, byte);
		wfqp->bytes -= byte;
		m_freem(mp);
		ifq->ifq_len--;
		if(drop_queue == queue)
			/* the queue for this flow is selected to drop */
			error = ENOBUFS;
	}
	return error;
}


static u_long wfq_hash(flow, n)
	struct flowinfo *flow;
	int n;
{
	u_long val = 0;

	if (flow != NULL) {
		if (flow->fi_family == AF_INET) {
			struct flowinfo_in *fp = (struct flowinfo_in *)flow;
			u_long val2;

			val = fp->fi_dst.s_addr ^ fp->fi_src.s_addr;
			val = val ^ (val >> 8) ^ (val >> 16) ^ (val >> 24);
			val2 = fp->fi_dport ^ fp->fi_sport ^ fp->fi_proto;
			val2 = val2 ^ (val2 >> 8);
			val = val ^ val2;
		}
#ifdef INET6
		else if (flow->fi_family == AF_INET6) {
			struct flowinfo_in6 *fp6 = (struct flowinfo_in6 *)flow;

			val = ntohl(fp6->fi6_flowlabel);
		}
#endif
	}

	return (val % n);
}


static __inline u_long wfq_hashbydstaddr(flow, n)
	struct flowinfo *flow;
	int n;
{
	u_long val = 0;

	if (flow != NULL) {
		if (flow->fi_family == AF_INET) {
			struct flowinfo_in *fp = (struct flowinfo_in *)flow;

			val = fp->fi_dst.s_addr;
			val = val ^ (val >> 8) ^ (val >> 16) ^ (val >> 24);
		}
#ifdef INET6
		else if (flow->fi_family == AF_INET6) {
			struct flowinfo_in6 *fp6 = (struct flowinfo_in6 *)flow;

			val = ntohl(fp6->fi6_flowlabel);
		}
#endif
	}

	return (val % n);
}

static __inline u_long wfq_hashbysrcport(flow, n)
	struct flowinfo *flow;
	int n;
{
	u_long val = 0;

	if (flow != NULL) {
		if (flow->fi_family == AF_INET) {
			struct flowinfo_in *fp = (struct flowinfo_in *)flow;

			val = fp->fi_sport;
		}
#ifdef INET6
		else if (flow->fi_family == AF_INET6) {
			struct flowinfo_in6 *fp6 = (struct flowinfo_in6 *)flow;

			val = fp6->fi6_sport;
		}
#endif
	}
	val = val ^ (val >> 8);

	return (val % n);
}

static wfq *wfq_maxqueue(wfqp)
	wfq_state_t *wfqp;
{
	int byte, max_byte = 0;
	wfq *queue, *max_queue = NULL;

	if((queue = wfqp->rrp) == NULL)
		/* never happens */
		return NULL;
	do{
		if ((byte = queue->bytes * 100 / queue->weight) > max_byte) {
			max_queue = queue;
			max_byte = byte;
		}
	} while ((queue = queue->next) != wfqp->rrp);

	return max_queue;
}


static struct mbuf *
wfq_ifdequeue(ifq, op)
	struct ifaltq *ifq;
	int op;
{
	wfq_state_t *wfqp;
	wfq *queue;
	struct mbuf *mp;
	int byte;

	wfqp = (wfq_state_t *)ifq->altq_disc;

	if ((wfqp->bytes == 0) || ((queue = wfqp->rrp) == NULL))
		/* no packet in the queues */
		return NULL;

	while (1) {
		if (queue->quota > 0) {
			if (queue->bytes <= 0) {
				/* this queue no longer has packet.
				   remove the queue from the active list. */
				if (queue->next == queue){
					/* no other active queue
					   -- this case never happens in
					   this algorithm. */
					queue->next = queue->prev = NULL;
					wfqp->rrp = NULL;
					return NULL;
				} else {
					queue->prev->next = queue->next;
					queue->next->prev = queue->prev;
					/* the round-robin pointer points
					   to this queue, advance the rrp */
					wfqp->rrp = queue->next;
					queue->next = queue->prev = NULL;
					queue = wfqp->rrp;
					WFQ_ADDQUOTA(queue);
					continue;
				}
			}

			/* dequeue a packet from this queue */
			mp = queue->head;
			if (op == ALTDQ_REMOVE) {
				if((queue->head = mp->m_nextpkt) == NULL)
					queue->tail = NULL;
				byte = mp->m_pkthdr.len;
				mp->m_nextpkt = NULL;
				queue->quota -= byte;
				queue->bytes -= byte;
				PKTCNTR_ADD(&queue->xmit_cnt, byte);
				wfqp->bytes -= byte;
				if (ALTQ_IS_ENABLED(ifq))
					ifq->ifq_len--;
			}
			return mp;

			/* if the queue gets empty by this dequeueing,
			   the queue will be removed from the active list
			   at the next round */
		}

		/* advance the round-robin pointer */
		queue = wfqp->rrp = queue->next;
		WFQ_ADDQUOTA(queue);
	}
}

static int
wfq_getqid(gqidp)
	struct wfq_getqid *gqidp;
{
	wfq_state_t *wfqp;

	if ((wfqp = altq_lookup(gqidp->iface.wfq_ifacename, ALTQT_WFQ))
	    == NULL)
		return (EBADF);

	gqidp->qid = (*wfqp->hash_func)(&gqidp->flow, wfqp->nums);
	return 0;
}

static int
wfq_setweight(swp)
	struct wfq_setweight *swp;
{
	wfq_state_t	*wfqp;
	wfq *queue;
	int old;

	if (swp->weight < 0) {
		printf("set weight in natural number\n");
		return (EINVAL);
	}

	if ((wfqp = altq_lookup(swp->iface.wfq_ifacename, ALTQT_WFQ)) == NULL)
		return (EBADF);

	queue = &wfqp->queue[swp->qid];
	old = queue->weight;
	queue->weight = swp->weight;
	swp->weight = old;
	return 0;
}


static int
wfq_getstats(gsp)
	struct wfq_getstats *gsp;
{
	wfq_state_t	*wfqp;
	wfq *queue;
	queue_stats *stats;

	if ((wfqp = altq_lookup(gsp->iface.wfq_ifacename, ALTQT_WFQ)) == NULL)
		return (EBADF);

	if (gsp->qid >= wfqp->nums)
		return (EINVAL);

	queue = &wfqp->queue[gsp->qid];
	stats = &gsp->stats;

	stats->bytes		= queue->bytes;
	stats->weight		= queue->weight;
	stats->xmit_cnt		= queue->xmit_cnt;
	stats->drop_cnt		= queue->drop_cnt;

	return 0;
}


static int
wfq_config(cf)
	struct wfq_conf *cf;
{
	wfq_state_t	*wfqp;
	wfq		*queue;
	int		i, error = 0;

	if ((wfqp = altq_lookup(cf->iface.wfq_ifacename, ALTQT_WFQ)) == NULL)
		return (EBADF);

	if(cf->nqueues <= 0 ||  MAX_QSIZE < cf->nqueues)
		cf->nqueues = DEFAULT_QSIZE;

	if (cf->nqueues != wfqp->nums) {
		/* free queued mbuf */
		wfq_flush(wfqp->ifq);
		FREE(wfqp->queue, M_DEVBUF);

		MALLOC(queue, wfq *, sizeof(wfq) * cf->nqueues,
		       M_DEVBUF, M_WAITOK);
		if (queue == NULL)
			return (ENOMEM);
		bzero(queue, sizeof(wfq) * cf->nqueues);

		wfqp->nums = cf->nqueues;
		wfqp->bytes = 0;
		wfqp->rrp = NULL;
		wfqp->queue = queue;
		for (i = 0; i < wfqp->nums; i++, queue++) {
			queue->next = queue->prev = NULL;
			queue->head = queue->tail = NULL;
			queue->bytes = queue->quota = 0;
			queue->weight = 100;
		}
	}

	if (cf->qlimit != 0)
		wfqp->hwm = cf->qlimit;

	switch (cf->hash_policy) {
	case WFQ_HASH_DSTADDR:
		wfqp->hash_func = wfq_hashbydstaddr;
		wfqp->fbmask = FIMB4_DADDR;
#ifdef INET6
		wfqp->fbmask |= FIMB6_FLABEL;	/* use flowlabel for ipv6 */
#endif
		break;
	case WFQ_HASH_SRCPORT:
		wfqp->hash_func = wfq_hashbysrcport;
		wfqp->fbmask = FIMB4_SPORT;
#ifdef INET6
		wfqp->fbmask |= FIMB6_SPORT;
#endif
		break;
	case WFQ_HASH_FULL:
		wfqp->hash_func = wfq_hash;
		wfqp->fbmask = FIMB4_ALL;
#ifdef INET6
		wfqp->fbmask |= FIMB6_FLABEL;	/* use flowlabel for ipv6 */
#endif
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
}

/*
 * wfq device interface
 */

altqdev_decl(wfq);

int
wfqopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	return 0;
}

int
wfqclose(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct ifnet *ifp;
	struct wfq_interface iface;
	wfq_state_t *wfqp;
	int s;

	s = splimp();
	while ((wfqp = wfq_list) != NULL) {
		ifp = wfqp->ifq->altq_ifp;
#if defined(__NetBSD__) || defined(__OpenBSD__)
		sprintf(iface.wfq_ifacename, "%s", ifp->if_xname);
#else
		sprintf(iface.wfq_ifacename, "%s%d",
			ifp->if_name, ifp->if_unit);
#endif
		wfq_ifdetach(&iface);
	}
	splx(s);
	return 0;
}

int
wfqioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	ioctlcmd_t cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	int	error = 0;
	int 	s;

	/* check cmd for superuser only */
	switch (cmd) {
	case WFQ_GET_QID:
	case WFQ_GET_STATS:
		break;
	default:
#if (__FreeBSD_version > 400000)
		if ((error = suser(p)) != 0)
#else
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
#endif
			return (error);
		break;
	}

	s = splimp();
	switch (cmd) {

	case WFQ_ENABLE:
		error = wfq_setenable((struct wfq_interface *)addr, ENABLE);
		break;

	case WFQ_DISABLE:
		error = wfq_setenable((struct wfq_interface *)addr, DISABLE);
		break;

	case WFQ_IF_ATTACH:
		error = wfq_ifattach((struct wfq_interface *)addr);
		break;

	case WFQ_IF_DETACH:
		error = wfq_ifdetach((struct wfq_interface *)addr);
		break;

	case WFQ_GET_QID:
		error = wfq_getqid((struct wfq_getqid *)addr);
		break;

	case WFQ_SET_WEIGHT:
		error = wfq_setweight((struct wfq_setweight *)addr);
		break;

	case WFQ_GET_STATS:
		error = wfq_getstats((struct wfq_getstats *)addr);
		break;

	case WFQ_CONFIG:
		error = wfq_config((struct wfq_conf *)addr);
		break;

	default:
		error = EINVAL;
		break;
	}
	splx(s);
	return error;
}

#ifdef KLD_MODULE

static struct altqsw wfq_sw =
	{"wfq", wfqopen, wfqclose, wfqioctl};

ALTQ_MODULE(altq_wfq, ALTQT_WFQ, &wfq_sw);

#endif /* KLD_MODULE */

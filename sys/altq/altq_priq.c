/*	$OpenBSD: altq_priq.c,v 1.7 2002/12/16 09:18:05 kjc Exp $	*/
/*	$KAME: altq_priq.c,v 1.1 2000/10/18 09:15:23 kjc Exp $	*/
/*
 * Copyright (C) 2000
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
 * priority queue
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/queue.h>

#include <net/if.h>
#include <netinet/in.h>

#include <net/pfvar.h>
#include <altq/altq.h>
#include <altq/altq_priq.h>

/*
 * function prototypes
 */
static int priq_clear_interface(struct priq_if *);
static int priq_request(struct ifaltq *, int, void *);
static void priq_purge(struct priq_if *);
static struct priq_class *priq_class_create(struct priq_if *, int, int, int);
static int priq_class_destroy(struct priq_class *);
static int priq_enqueue(struct ifaltq *, struct mbuf *,
			     struct altq_pktattr *);
static struct mbuf *priq_dequeue(struct ifaltq *, int);

static int priq_addq(struct priq_class *, struct mbuf *);
static struct mbuf *priq_getq(struct priq_class *);
static struct mbuf *priq_pollq(struct priq_class *);
static void priq_purgeq(struct priq_class *);

static void get_class_stats(struct priq_classstats *, struct priq_class *);
static struct priq_class *clh_to_clp(struct priq_if *, u_int32_t);

int
priq_pfattach(struct pf_altq *a)
{
	struct ifnet *ifp;
	int s, error;

	if ((ifp = ifunit(a->ifname)) == NULL || a->altq_disc == NULL)
		return (EINVAL);
	s = splimp();
	error = altq_attach(&ifp->if_snd, ALTQT_PRIQ, a->altq_disc,
	    priq_enqueue, priq_dequeue, priq_request, NULL, NULL);
	splx(s);
	return (error);
}

int
priq_add_altq(struct pf_altq *a)
{
	struct priq_if *pif;
	struct ifnet 	*ifp;

	if ((ifp = ifunit(a->ifname)) == NULL)
		return (EINVAL);
	if (!ALTQ_IS_READY(&ifp->if_snd))
		return (ENODEV);

	MALLOC(pif, struct priq_if *, sizeof(struct priq_if),
	    M_DEVBUF, M_WAITOK);
	if (pif == NULL)
		return (ENOMEM);
	bzero(pif, sizeof(struct priq_if));
	pif->pif_bandwidth = a->ifbandwidth;
	pif->pif_maxpri = -1;
	pif->pif_ifq = &ifp->if_snd;

	/* keep the state in pf_altq */
	a->altq_disc = pif;

	return (0);
}

int
priq_remove_altq(struct pf_altq *a)
{
	struct priq_if *pif;

	if ((pif = a->altq_disc) == NULL)
		return (EINVAL);
	a->altq_disc = NULL;

	(void)priq_clear_interface(pif);

	FREE(pif, M_DEVBUF);
	return (0);
}

int
priq_add_queue(struct pf_altq *a)
{
	struct priq_if *pif;
	struct priq_class *cl;

	if ((pif = a->altq_disc) == NULL)
		return (EINVAL);

	/* check parameters */
	if (a->priority >= PRIQ_MAXPRI)
		return (EINVAL);

	cl = priq_class_create(pif, a->priority, a->qlimit,
	    a->pq_u.priq_opts.flags);
	if (cl == NULL)
		return (ENOMEM);

	/* return handle to user space. */
	a->qid = cl->cl_handle;

	return (0);
}

int
priq_remove_queue(struct pf_altq *a)
{
	struct priq_if *pif;
	struct priq_class *cl;

	if ((pif = a->altq_disc) == NULL)
		return (EINVAL);

	if ((cl = clh_to_clp(pif, a->qid)) == NULL)
		return (EINVAL);

	return (priq_class_destroy(cl));
}

int
priq_getqstats(struct pf_altq *a, void *ubuf, int *nbytes)
{
	struct priq_if *pif;
	struct priq_class *cl;
	struct priq_classstats stats;
	int error = 0;

	if ((pif = altq_lookup(a->ifname, ALTQT_PRIQ)) == NULL)
		return (EBADF);

	if ((cl = clh_to_clp(pif, a->qid)) == NULL)
		return (EINVAL);

	if (*nbytes < sizeof(stats))
		return (EINVAL);

	get_class_stats(&stats, cl);
#if 0
	stats.handle = a->qid;
#endif

	if ((error = copyout((caddr_t)&stats, ubuf, sizeof(stats))) != 0)
		return (error);
	*nbytes = sizeof(stats);
	return (0);
}

/*
 * bring the interface back to the initial state by discarding
 * all the filters and classes.
 */
static int
priq_clear_interface(pif)
	struct priq_if *pif;
{
	struct priq_class	*cl;
	int pri;

	/* clear out the classes */
	for (pri = 0; pri <= pif->pif_maxpri; pri++)
		if ((cl = pif->pif_classes[pri]) != NULL)
			priq_class_destroy(cl);

	return (0);
}

static int
priq_request(ifq, req, arg)
	struct ifaltq *ifq;
	int req;
	void *arg;
{
	struct priq_if	*pif = (struct priq_if *)ifq->altq_disc;

	switch (req) {
	case ALTRQ_PURGE:
		priq_purge(pif);
		break;
	}
	return (0);
}

/* discard all the queued packets on the interface */
static void
priq_purge(pif)
	struct priq_if *pif;
{
	struct priq_class *cl;
	int pri;

	for (pri = 0; pri <= pif->pif_maxpri; pri++) {
		if ((cl = pif->pif_classes[pri]) != NULL && !qempty(cl->cl_q))
			priq_purgeq(cl);
	}
	if (ALTQ_IS_ENABLED(pif->pif_ifq))
		pif->pif_ifq->ifq_len = 0;
}

static struct priq_class *
priq_class_create(pif, pri, qlimit, flags)
	struct priq_if *pif;
	int pri, qlimit, flags;
{
	struct priq_class *cl;
	int s;

#ifndef ALTQ_RED
	if (flags & PRCF_RED) {
#ifdef ALTQ_DEBUG
		printf("priq_class_create: RED not configured for PRIQ!\n");
#endif
		return (NULL);
	}
#endif

	if ((cl = pif->pif_classes[pri]) != NULL) {
		/* modify the class instead of creating a new one */
		s = splimp();
		if (!qempty(cl->cl_q))
			priq_purgeq(cl);
		splx(s);
#ifdef ALTQ_RIO
		if (q_is_rio(cl->cl_q))
			rio_destroy((rio_t *)cl->cl_red);
#endif
#ifdef ALTQ_RED
		if (q_is_red(cl->cl_q))
			red_destroy(cl->cl_red);
#endif
	} else {
		MALLOC(cl, struct priq_class *, sizeof(struct priq_class),
		       M_DEVBUF, M_WAITOK);
		if (cl == NULL)
			return (NULL);
		bzero(cl, sizeof(struct priq_class));

		MALLOC(cl->cl_q, class_queue_t *, sizeof(class_queue_t),
		       M_DEVBUF, M_WAITOK);
		if (cl->cl_q == NULL)
			goto err_ret;
		bzero(cl->cl_q, sizeof(class_queue_t));
	}

	pif->pif_classes[pri] = cl;
	if (flags & PRCF_DEFAULTCLASS)
		pif->pif_default = cl;
	if (qlimit == 0)
		qlimit = 50;  /* use default */
	qlimit(cl->cl_q) = qlimit;
	qtype(cl->cl_q) = Q_DROPTAIL;
	qlen(cl->cl_q) = 0;
	cl->cl_flags = flags;
	cl->cl_pri = pri;
	if (pri > pif->pif_maxpri)
		pif->pif_maxpri = pri;
	cl->cl_pif = pif;
	cl->cl_handle = pri + 1;

#ifdef ALTQ_RED
	if (flags & (PRCF_RED|PRCF_RIO)) {
		int red_flags, red_pkttime;

		red_flags = 0;
		if (flags & PRCF_ECN)
			red_flags |= REDF_ECN;
#ifdef ALTQ_RIO
		if (flags & PRCF_CLEARDSCP)
			red_flags |= RIOF_CLEARDSCP;
#endif
		if (pif->pif_bandwidth < 8)
			red_pkttime = 1000 * 1000 * 1000; /* 1 sec */
		else
			red_pkttime = (int64_t)pif->pif_ifq->altq_ifp->if_mtu
			  * 1000 * 1000 * 1000 / (pif->pif_bandwidth / 8);
#ifdef ALTQ_RIO
		if (flags & PRCF_RIO) {
			cl->cl_red = (red_t *)rio_alloc(0, NULL,
						red_flags, red_pkttime);
			if (cl->cl_red != NULL)
				qtype(cl->cl_q) = Q_RIO;
		} else
#endif
		if (flags & PRCF_RED) {
			cl->cl_red = red_alloc(0, 0, 0, 0,
					       red_flags, red_pkttime);
			if (cl->cl_red != NULL)
				qtype(cl->cl_q) = Q_RED;
		}
	}
#endif /* ALTQ_RED */

	return (cl);

 err_ret:
	if (cl->cl_red != NULL) {
#ifdef ALTQ_RIO
		if (q_is_rio(cl->cl_q))
			rio_destroy((rio_t *)cl->cl_red);
#endif
#ifdef ALTQ_RED
		if (q_is_red(cl->cl_q))
			red_destroy(cl->cl_red);
#endif
	}
	if (cl->cl_q != NULL)
		FREE(cl->cl_q, M_DEVBUF);
	FREE(cl, M_DEVBUF);
	return (NULL);
}

static int
priq_class_destroy(cl)
	struct priq_class *cl;
{
	struct priq_if *pif;
	int s, pri;

	s = splimp();

	if (!qempty(cl->cl_q))
		priq_purgeq(cl);

	pif = cl->cl_pif;
	pif->pif_classes[cl->cl_pri] = NULL;
	if (pif->pif_maxpri == cl->cl_pri) {
		for (pri = cl->cl_pri; pri >= 0; pri--)
			if (pif->pif_classes[pri] != NULL) {
				pif->pif_maxpri = pri;
				break;
			}
		if (pri < 0)
			pif->pif_maxpri = -1;
	}
	splx(s);

	if (cl->cl_red != NULL) {
#ifdef ALTQ_RIO
		if (q_is_rio(cl->cl_q))
			rio_destroy((rio_t *)cl->cl_red);
#endif
#ifdef ALTQ_RED
		if (q_is_red(cl->cl_q))
			red_destroy(cl->cl_red);
#endif
	}
	FREE(cl->cl_q, M_DEVBUF);
	FREE(cl, M_DEVBUF);
	return (0);
}

/*
 * priq_enqueue is an enqueue function to be registered to
 * (*altq_enqueue) in struct ifaltq.
 */
static int
priq_enqueue(ifq, m, pktattr)
	struct ifaltq *ifq;
	struct mbuf *m;
	struct altq_pktattr *pktattr;
{
	struct priq_if	*pif = (struct priq_if *)ifq->altq_disc;
	struct priq_class *cl;
	struct m_tag *t;
	int len;

	/* grab class set by classifier */
	t = m_tag_find(m, PACKET_TAG_PF_QID, NULL);
	if (t == NULL ||
	    (cl = clh_to_clp(pif, ((struct altq_tag *)(t+1))->qid)) == NULL) {
		cl = pif->pif_default;
		if (cl == NULL) {
			m_freem(m);
			return (ENOBUFS);
		}
		cl->cl_pktattr = NULL;
	}

	len = m_pktlen(m);
	if (priq_addq(cl, m) != 0) {
		/* drop occurred.  mbuf was freed in priq_addq. */
		PKTCNTR_ADD(&cl->cl_dropcnt, len);
		return (ENOBUFS);
	}
	IFQ_INC_LEN(ifq);

	/* successfully queued. */
	return (0);
}

/*
 * priq_dequeue is a dequeue function to be registered to
 * (*altq_dequeue) in struct ifaltq.
 *
 * note: ALTDQ_POLL returns the next packet without removing the packet
 *	from the queue.  ALTDQ_REMOVE is a normal dequeue operation.
 *	ALTDQ_REMOVE must return the same packet if called immediately
 *	after ALTDQ_POLL.
 */
static struct mbuf *
priq_dequeue(ifq, op)
	struct ifaltq	*ifq;
	int		op;
{
	struct priq_if	*pif = (struct priq_if *)ifq->altq_disc;
	struct priq_class *cl;
	struct mbuf *m;
	int pri;

	if (IFQ_IS_EMPTY(ifq))
		/* no packet in the queue */
		return (NULL);

	for (pri = pif->pif_maxpri;  pri >= 0; pri--) {
		if ((cl = pif->pif_classes[pri]) != NULL &&
		    !qempty(cl->cl_q)) {
			if (op == ALTDQ_POLL)
				return (priq_pollq(cl));

			m = priq_getq(cl);
			if (m != NULL) {
				IFQ_DEC_LEN(ifq);
				if (qempty(cl->cl_q))
					cl->cl_period++;
				PKTCNTR_ADD(&cl->cl_xmitcnt, m_pktlen(m));
			}
			return (m);
		}
	}
	return (NULL);
}

static int
priq_addq(cl, m)
	struct priq_class *cl;
	struct mbuf *m;
{

#ifdef ALTQ_RIO
	if (q_is_rio(cl->cl_q))
		return rio_addq((rio_t *)cl->cl_red, cl->cl_q, m,
				cl->cl_pktattr);
#endif
#ifdef ALTQ_RED
	if (q_is_red(cl->cl_q))
		return red_addq(cl->cl_red, cl->cl_q, m, cl->cl_pktattr);
#endif
	if (qlen(cl->cl_q) >= qlimit(cl->cl_q)) {
		m_freem(m);
		return (-1);
	}

	if (cl->cl_flags & PRCF_CLEARDSCP)
		write_dsfield(m, cl->cl_pktattr, 0);

	_addq(cl->cl_q, m);

	return (0);
}

static struct mbuf *
priq_getq(cl)
	struct priq_class *cl;
{
#ifdef ALTQ_RIO
	if (q_is_rio(cl->cl_q))
		return rio_getq((rio_t *)cl->cl_red, cl->cl_q);
#endif
#ifdef ALTQ_RED
	if (q_is_red(cl->cl_q))
		return red_getq(cl->cl_red, cl->cl_q);
#endif
	return _getq(cl->cl_q);
}

static struct mbuf *
priq_pollq(cl)
	struct priq_class *cl;
{
	return qhead(cl->cl_q);
}

static void
priq_purgeq(cl)
	struct priq_class *cl;
{
	struct mbuf *m;

	if (qempty(cl->cl_q))
		return;

	while ((m = _getq(cl->cl_q)) != NULL) {
		PKTCNTR_ADD(&cl->cl_dropcnt, m_pktlen(m));
		m_freem(m);
	}
	ASSERT(qlen(cl->cl_q) == 0);
}

static void
get_class_stats(sp, cl)
	struct priq_classstats *sp;
	struct priq_class *cl;
{
	sp->class_handle = cl->cl_handle;
	sp->qlength = qlen(cl->cl_q);
	sp->qlimit = qlimit(cl->cl_q);
	sp->period = cl->cl_period;
	sp->xmitcnt = cl->cl_xmitcnt;
	sp->dropcnt = cl->cl_dropcnt;

	sp->qtype = qtype(cl->cl_q);
#ifdef ALTQ_RED
	if (q_is_red(cl->cl_q))
		red_getstats(cl->cl_red, &sp->red[0]);
#endif
#ifdef ALTQ_RIO
	if (q_is_rio(cl->cl_q))
		rio_getstats((rio_t *)cl->cl_red, &sp->red[0]);
#endif

}

/* convert a class handle to the corresponding class pointer */
static struct priq_class *
clh_to_clp(pif, chandle)
	struct priq_if *pif;
	u_int32_t chandle;
{
	int idx;

	if (chandle == 0)
		return (NULL);
	idx = chandle - 1;
	if (idx >= PRIQ_MAXPRI)
		return (NULL);
	return (pif->pif_classes[idx]);
}

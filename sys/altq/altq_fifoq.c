/*	$OpenBSD: altq_fifoq.c,v 1.5 2002/11/26 01:03:34 henning Exp $	*/
/*	$KAME: altq_fifoq.c,v 1.7 2000/12/14 08:12:45 thorpej Exp $	*/

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
 * FIFOQ is an altq sample implementation.  There will be little
 * need to use FIFOQ as an alternative queueing scheme.
 * But this code is provided as a template for those who want to
 * write their own queueing schemes.
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_types.h>
#include <netinet/in.h>

#include <altq/altq.h>
#include <altq/altq_conf.h>
#include <altq/altq_fifoq.h>

#define	FIFOQ_STATS	/* collect statistics */

/* fifoq_list keeps all fifoq_state_t's allocated. */
static fifoq_state_t *fifoq_list = NULL;

/* internal function prototypes */
static int		fifoq_enqueue(struct ifaltq *, struct mbuf *,
				      struct altq_pktattr *);
static struct mbuf 	*fifoq_dequeue(struct ifaltq *, int);
static int 		fifoq_detach(fifoq_state_t *);
static int		fifoq_request(struct ifaltq *, int, void *);
static void 		fifoq_purge(fifoq_state_t *);

/*
 * fifoq device interface
 */
altqdev_decl(fifoq);

int
fifoqopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	/* everything will be done when the queueing scheme is attached. */
	return 0;
}

/*
 * there are 2 ways to act on close.
 *   detach-all-on-close:
 *	use for the daemon style approach.  if the daemon dies, all the
 *	resource will be released.
 *   no-action-on-close:
 *	use for the command style approach.  (e.g.  fifoq on/off)
 *
 * note: close is called not on every close but when the last reference
 *       is removed (only once with multiple simultaneous references.)
 */
int
fifoqclose(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	fifoq_state_t *q;
	int err, error = 0;

	while ((q = fifoq_list) != NULL) {
		/* destroy all */
		err = fifoq_detach(q);
		if (err != 0 && error == 0)
			error = err;
	}

	return error;
}

int
fifoqioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	ioctlcmd_t cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	fifoq_state_t *q;
	struct fifoq_interface *ifacep;
	struct ifnet *ifp;
	int	error = 0;

	/* check super-user privilege */
	switch (cmd) {
	case FIFOQ_GETSTATS:
		break;
	default:
#if (__FreeBSD_version > 400000)
		if ((error = suser(p)) != 0)
			return (error);
#else
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return (error);
#endif
		break;
	}

	switch (cmd) {
	case FIFOQ_ENABLE:
		ifacep = (struct fifoq_interface *)addr;
		if ((q = altq_lookup(ifacep->fifoq_ifname, ALTQT_FIFOQ))
		    == NULL) {
			error = EBADF;
			break;
		}
		error = altq_enable(q->q_ifq);
		break;

	case FIFOQ_DISABLE:
		ifacep = (struct fifoq_interface *)addr;
		if ((q = altq_lookup(ifacep->fifoq_ifname, ALTQT_FIFOQ))
		    == NULL) {
			error = EBADF;
			break;
		}
		error = altq_disable(q->q_ifq);
		break;

	case FIFOQ_IF_ATTACH:
		ifp = ifunit(((struct fifoq_interface *)addr)->fifoq_ifname);
		if (ifp == NULL) {
			error = ENXIO;
			break;
		}

		/* allocate and initialize fifoq_state_t */
		MALLOC(q, fifoq_state_t *, sizeof(fifoq_state_t),
		       M_DEVBUF, M_WAITOK);
		if (q == NULL) {
			error = ENOMEM;
			break;
		}
		bzero(q, sizeof(fifoq_state_t));

		q->q_ifq = &ifp->if_snd;
		q->q_head = q->q_tail = NULL;
		q->q_len = 0;
		q->q_limit = FIFOQ_LIMIT;

		/*
		 * set FIFOQ to this ifnet structure.
		 */
		error = altq_attach(q->q_ifq, ALTQT_FIFOQ, q,
				    fifoq_enqueue, fifoq_dequeue, fifoq_request,
				    NULL, NULL);
		if (error) {
			FREE(q, M_DEVBUF);
			break;
		}

		/* add this state to the fifoq list */
		q->q_next = fifoq_list;
		fifoq_list = q;
		break;

	case FIFOQ_IF_DETACH:
		ifacep = (struct fifoq_interface *)addr;
		if ((q = altq_lookup(ifacep->fifoq_ifname, ALTQT_FIFOQ))
		    == NULL) {
			error = EBADF;
			break;
		}
		error = fifoq_detach(q);
		break;

	case FIFOQ_GETSTATS:
		do {
			struct fifoq_getstats *q_stats;

			q_stats = (struct fifoq_getstats *)addr;
			if ((q = altq_lookup(q_stats->iface.fifoq_ifname,
					     ALTQT_FIFOQ)) == NULL) {
				error = EBADF;
				break;
			}

			q_stats->q_len		= q->q_len;
			q_stats->q_limit 	= q->q_limit;
			q_stats->xmit_cnt	= q->q_stats.xmit_cnt;
			q_stats->drop_cnt 	= q->q_stats.drop_cnt;
			q_stats->period   	= q->q_stats.period;
		} while (0);
		break;

	case FIFOQ_CONFIG:
		do {
			struct fifoq_conf *fc;
			int limit;

			fc = (struct fifoq_conf *)addr;
			if ((q = altq_lookup(fc->iface.fifoq_ifname,
					     ALTQT_FIFOQ)) == NULL) {
				error = EBADF;
				break;
			}
			limit = fc->fifoq_limit;
			if (limit < 0)
				limit = 0;
			q->q_limit = limit;
			fc->fifoq_limit = limit;
		} while (0);
		break;

	default:
		error = EINVAL;
		break;
	}
	return error;
}

/*
 * fifoq support routines
 */

/*
 * enqueue routine:
 *
 *	returns: 0 when successfully queued.
 *		 ENOBUFS when drop occurs.
 */
static int
fifoq_enqueue(ifq, m, pktattr)
	struct ifaltq *ifq;
	struct mbuf *m;
	struct altq_pktattr *pktattr;
{
	fifoq_state_t *q = (fifoq_state_t *)ifq->altq_disc;

	/* if the queue is full, drop the incoming packet(drop-tail) */
	if (q->q_len >= q->q_limit) {
#ifdef FIFOQ_STATS
		PKTCNTR_ADD(&q->q_stats.drop_cnt, m_pktlen(m));
#endif
		m_freem(m);
		return (ENOBUFS);
	}

	/* enqueue the packet at the taile of the queue */
	m->m_nextpkt = NULL;
	if (q->q_tail == NULL)
		q->q_head = m;
	else
		q->q_tail->m_nextpkt = m;
	q->q_tail = m;
	q->q_len++;
	ifq->ifq_len++;
	return 0;
}

/*
 * dequeue routine:
 *	must be called in splimp.
 *
 *	returns: mbuf dequeued.
 *		 NULL when no packet is available in the queue.
 */
/*
 * ALTDQ_PEEK is provided for drivers which need to know the next packet
 * to send in advance.
 * when ALTDQ_PEEK is specified, the next packet to be dequeued is
 * returned without dequeueing the packet.
 * when ALTDQ_DEQUEUE is called *immediately after* an ALTDQ_PEEK
 * operation, the same packet should be returned.
 */
static struct mbuf *
fifoq_dequeue(ifq, op)
	struct ifaltq *ifq;
	int op;
{
	fifoq_state_t *q = (fifoq_state_t *)ifq->altq_disc;
	struct mbuf *m = NULL;

	if (op == ALTDQ_POLL)
		return (q->q_head);

	if ((m = q->q_head) == NULL)
		return (NULL);

	if ((q->q_head = m->m_nextpkt) == NULL)
		q->q_tail = NULL;
	m->m_nextpkt = NULL;
	q->q_len--;
	ifq->ifq_len--;
#ifdef FIFOQ_STATS
	PKTCNTR_ADD(&q->q_stats.xmit_cnt, m_pktlen(m));
	if (q->q_len == 0)
		q->q_stats.period++;
#endif
	return (m);
}

static int
fifoq_request(ifq, req, arg)
	struct ifaltq *ifq;
	int req;
	void *arg;
{
	fifoq_state_t *q = (fifoq_state_t *)ifq->altq_disc;

	switch (req) {
	case ALTRQ_PURGE:
		fifoq_purge(q);
		break;
	}
	return (0);
}


static int fifoq_detach(q)
	fifoq_state_t *q;
{
	fifoq_state_t *tmp;
	int error = 0;

	if (ALTQ_IS_ENABLED(q->q_ifq))
		altq_disable(q->q_ifq);

	fifoq_purge(q);

	if ((error = altq_detach(q->q_ifq)))
		return (error);

	if (fifoq_list == q)
		fifoq_list = q->q_next;
	else {
		for (tmp = fifoq_list; tmp != NULL; tmp = tmp->q_next)
			if (tmp->q_next == q) {
				tmp->q_next = q->q_next;
				break;
			}
		if (tmp == NULL)
			printf("fifoq_detach: no state in fifoq_list!\n");
	}

	FREE(q, M_DEVBUF);
	return (error);
}

/*
 * fifoq_purge
 * should be called in splimp or after disabling the fifoq.
 */
static void fifoq_purge(q)
	fifoq_state_t *q;
{
	struct mbuf *m;

	while ((m = q->q_head) != NULL) {
		q->q_head = m->m_nextpkt;
		m_freem(m);
	}
	q->q_tail = NULL;
	q->q_len = 0;
	if (ALTQ_IS_ENABLED(q->q_ifq))
		q->q_ifq->ifq_len = 0;
}

#ifdef KLD_MODULE

static struct altqsw fifoq_sw =
	{"fifoq", fifoqopen, fifoqclose, fifoqioctl};

ALTQ_MODULE(altq_fifoq, ALTQT_FIFOQ, &fifoq_sw);

#endif /* KLD_MODULE */

/*	$OpenBSD: altq_hfsc.c,v 1.6 2002/11/26 01:03:34 henning Exp $	*/
/*	$KAME: altq_hfsc.c,v 1.13 2002/05/16 11:02:58 kjc Exp $	*/

/*
 * Copyright (c) 1997-1999 Carnegie Mellon University. All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation is hereby granted (including for commercial or
 * for-profit use), provided that both the copyright notice and this
 * permission notice appear in all copies of the software, derivative
 * works, or modified versions, and any portions thereof, and that
 * both notices appear in supporting documentation, and that credit
 * is given to Carnegie Mellon University in all publications reporting
 * on direct or indirect use of this code or its derivatives.
 *
 * THIS SOFTWARE IS EXPERIMENTAL AND IS KNOWN TO HAVE BUGS, SOME OF
 * WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON PROVIDES THIS
 * SOFTWARE IN ITS ``AS IS'' CONDITION, AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Carnegie Mellon encourages (but does not require) users of this
 * software to return any improvements or extensions that they make,
 * and to grant Carnegie Mellon the rights to redistribute these
 * changes without encumbrance.
 */
/*
 * H-FSC is described in Proceedings of SIGCOMM'97,
 * "A Hierarchical Fair Service Curve Algorithm for Link-Sharing,
 * Real-Time and Priority Service"
 * by Ion Stoica, Hui Zhang, and T. S. Eugene Ng.
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
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_types.h>

#include <altq/altq.h>
#include <altq/altq_conf.h>
#include <altq/altq_hfsc.h>

/*
 * function prototypes
 */
static struct hfsc_if *hfsc_attach(struct ifaltq *, u_int);
static int hfsc_detach(struct hfsc_if *);
static int hfsc_clear_interface(struct hfsc_if *);
static int hfsc_request(struct ifaltq *, int, void *);
static void hfsc_purge(struct hfsc_if *);
static struct hfsc_class *hfsc_class_create(struct hfsc_if *,
		 struct service_curve *, struct hfsc_class *, int, int);
static int hfsc_class_destroy(struct hfsc_class *);
static int hfsc_class_modify(struct hfsc_class *,
			     struct service_curve *, struct service_curve *);
static struct hfsc_class *hfsc_nextclass(struct hfsc_class *);

static int hfsc_enqueue(struct ifaltq *, struct mbuf *, struct altq_pktattr *);
static struct mbuf *hfsc_dequeue(struct ifaltq *, int);

static int hfsc_addq(struct hfsc_class *, struct mbuf *);
static struct mbuf *hfsc_getq(struct hfsc_class *);
static struct mbuf *hfsc_pollq(struct hfsc_class *);
static void hfsc_purgeq(struct hfsc_class *);

static void set_active(struct hfsc_class *, int);
static void set_passive(struct hfsc_class *);

static void init_ed(struct hfsc_class *, int);
static void update_ed(struct hfsc_class *, int);
static void update_d(struct hfsc_class *, int);
static void init_v(struct hfsc_class *, int);
static void update_v(struct hfsc_class *, int);
static ellist_t *ellist_alloc(void);
static void ellist_destroy(ellist_t *);
static void ellist_insert(struct hfsc_class *);
static void ellist_remove(struct hfsc_class *);
static void ellist_update(struct hfsc_class *);
struct hfsc_class *ellist_get_mindl(ellist_t *);
static actlist_t *actlist_alloc(void);
static void actlist_destroy(actlist_t *);
static void actlist_insert(struct hfsc_class *);
static void actlist_remove(struct hfsc_class *);
static void actlist_update(struct hfsc_class *);

static __inline u_int64_t seg_x2y(u_int64_t, u_int64_t);
static __inline u_int64_t seg_y2x(u_int64_t, u_int64_t);
static __inline u_int64_t m2sm(u_int);
static __inline u_int64_t m2ism(u_int);
static __inline u_int64_t d2dx(u_int);
static u_int sm2m(u_int64_t);
static u_int dx2d(u_int64_t);

static void sc2isc(struct service_curve *, struct internal_sc *);
static void rtsc_init(struct runtime_sc *, struct internal_sc *,
		      u_int64_t, u_int64_t);
static u_int64_t rtsc_y2x(struct runtime_sc *, u_int64_t);
static u_int64_t rtsc_x2y(struct runtime_sc *, u_int64_t);
static void rtsc_min(struct runtime_sc *, struct internal_sc *,
		     u_int64_t, u_int64_t);

int hfscopen(dev_t, int, int, struct proc *);
int hfscclose(dev_t, int, int, struct proc *);
int hfscioctl(dev_t, ioctlcmd_t, caddr_t, int, struct proc *);
static int hfsccmd_if_attach(struct hfsc_attach *);
static int hfsccmd_if_detach(struct hfsc_interface *);
static int hfsccmd_add_class(struct hfsc_add_class *);
static int hfsccmd_delete_class(struct hfsc_delete_class *);
static int hfsccmd_modify_class(struct hfsc_modify_class *);
static int hfsccmd_add_filter(struct hfsc_add_filter *);
static int hfsccmd_delete_filter(struct hfsc_delete_filter *);
static int hfsccmd_class_stats(struct hfsc_class_stats *);
static void get_class_stats(struct class_stats *, struct hfsc_class *);
static struct hfsc_class *clh_to_clp(struct hfsc_if *, u_long);
static u_long clp_to_clh(struct hfsc_class *);

/*
 * macros
 */
#define	is_a_parent_class(cl)	((cl)->cl_children != NULL)

/* hif_list keeps all hfsc_if's allocated. */
static struct hfsc_if *hif_list = NULL;

static struct hfsc_if *
hfsc_attach(ifq, bandwidth)
	struct ifaltq *ifq;
	u_int bandwidth;
{
	struct hfsc_if *hif;
	struct service_curve root_sc;

	MALLOC(hif, struct hfsc_if *, sizeof(struct hfsc_if),
	       M_DEVBUF, M_WAITOK);
	if (hif == NULL)
		return (NULL);
	bzero(hif, sizeof(struct hfsc_if));

	hif->hif_eligible = ellist_alloc();
	if (hif->hif_eligible == NULL) {
		FREE(hif, M_DEVBUF);
		return NULL;
	}

	hif->hif_ifq = ifq;

	/*
	 * create root class
	 */
	root_sc.m1 = bandwidth;
	root_sc.d = 0;
	root_sc.m2 = bandwidth;
	if ((hif->hif_rootclass =
	     hfsc_class_create(hif, &root_sc, NULL, 0, 0)) == NULL) {
		FREE(hif, M_DEVBUF);
		return (NULL);
	}

	/* add this state to the hfsc list */
	hif->hif_next = hif_list;
	hif_list = hif;

	return (hif);
}

static int
hfsc_detach(hif)
	struct hfsc_if *hif;
{
	(void)hfsc_clear_interface(hif);
	(void)hfsc_class_destroy(hif->hif_rootclass);

	/* remove this interface from the hif list */
	if (hif_list == hif)
		hif_list = hif->hif_next;
	else {
		struct hfsc_if *h;

		for (h = hif_list; h != NULL; h = h->hif_next)
			if (h->hif_next == hif) {
				h->hif_next = hif->hif_next;
				break;
			}
		ASSERT(h != NULL);
	}

	ellist_destroy(hif->hif_eligible);

	FREE(hif, M_DEVBUF);

	return (0);
}

/*
 * bring the interface back to the initial state by discarding
 * all the filters and classes except the root class.
 */
static int
hfsc_clear_interface(hif)
	struct hfsc_if *hif;
{
	struct hfsc_class	*cl;

	/* free the filters for this interface */
	acc_discard_filters(&hif->hif_classifier, NULL, 1);

	/* clear out the classes */
	while ((cl = hif->hif_rootclass->cl_children) != NULL) {
		/*
		 * remove the first leaf class found in the hierarchy
		 * then start over
		 */
		for (; cl != NULL; cl = hfsc_nextclass(cl)) {
			if (!is_a_parent_class(cl)) {
				(void)hfsc_class_destroy(cl);
				break;
			}
		}
	}

	return (0);
}

static int
hfsc_request(ifq, req, arg)
	struct ifaltq *ifq;
	int req;
	void *arg;
{
	struct hfsc_if	*hif = (struct hfsc_if *)ifq->altq_disc;

	switch (req) {
	case ALTRQ_PURGE:
		hfsc_purge(hif);
		break;
	}
	return (0);
}

/* discard all the queued packets on the interface */
static void
hfsc_purge(hif)
	struct hfsc_if *hif;
{
	struct hfsc_class *cl;

	for (cl = hif->hif_rootclass; cl != NULL; cl = hfsc_nextclass(cl))
		if (!qempty(cl->cl_q))
			hfsc_purgeq(cl);
	if (ALTQ_IS_ENABLED(hif->hif_ifq))
		hif->hif_ifq->ifq_len = 0;
}

struct hfsc_class *
hfsc_class_create(hif, sc, parent, qlimit, flags)
	struct hfsc_if *hif;
	struct service_curve *sc;
	struct hfsc_class *parent;
	int qlimit, flags;
{
	struct hfsc_class *cl, *p;
	int s;

#ifndef ALTQ_RED
	if (flags & HFCF_RED) {
		printf("hfsc_class_create: RED not configured for HFSC!\n");
		return (NULL);
	}
#endif

	MALLOC(cl, struct hfsc_class *, sizeof(struct hfsc_class),
	       M_DEVBUF, M_WAITOK);
	if (cl == NULL)
		return (NULL);
	bzero(cl, sizeof(struct hfsc_class));

	MALLOC(cl->cl_q, class_queue_t *, sizeof(class_queue_t),
	       M_DEVBUF, M_WAITOK);
	if (cl->cl_q == NULL)
		goto err_ret;
	bzero(cl->cl_q, sizeof(class_queue_t));

	cl->cl_actc = actlist_alloc();
	if (cl->cl_actc == NULL)
		goto err_ret;

	if (qlimit == 0)
		qlimit = 50;  /* use default */
	qlimit(cl->cl_q) = qlimit;
	qtype(cl->cl_q) = Q_DROPTAIL;
	qlen(cl->cl_q) = 0;
	cl->cl_flags = flags;
#ifdef ALTQ_RED
	if (flags & (HFCF_RED|HFCF_RIO)) {
		int red_flags, red_pkttime;

		red_flags = 0;
		if (flags & HFCF_ECN)
			red_flags |= REDF_ECN;
#ifdef ALTQ_RIO
		if (flags & HFCF_CLEARDSCP)
			red_flags |= RIOF_CLEARDSCP;
#endif
		if (sc->m2 < 8)
			red_pkttime = 1000 * 1000 * 1000; /* 1 sec */
		else
			red_pkttime = (int64_t)hif->hif_ifq->altq_ifp->if_mtu
				* 1000 * 1000 * 1000 / (sc->m2 / 8);
		if (flags & HFCF_RED) {
			cl->cl_red = red_alloc(0, 0, 0, 0,
					       red_flags, red_pkttime);
			if (cl->cl_red != NULL)
				qtype(cl->cl_q) = Q_RED;
		}
#ifdef ALTQ_RIO
		else {
			cl->cl_red = (red_t *)rio_alloc(0, NULL,
						      red_flags, red_pkttime);
			if (cl->cl_red != NULL)
				qtype(cl->cl_q) = Q_RIO;
		}
#endif
	}
#endif /* ALTQ_RED */

	if (sc != NULL && (sc->m1 != 0 || sc->m2 != 0)) {
		MALLOC(cl->cl_rsc, struct internal_sc *,
		       sizeof(struct internal_sc), M_DEVBUF, M_WAITOK);
		if (cl->cl_rsc == NULL)
			goto err_ret;
		bzero(cl->cl_rsc, sizeof(struct internal_sc));
		sc2isc(sc, cl->cl_rsc);
		rtsc_init(&cl->cl_deadline, cl->cl_rsc, 0, 0);
		rtsc_init(&cl->cl_eligible, cl->cl_rsc, 0, 0);

		MALLOC(cl->cl_fsc, struct internal_sc *,
		       sizeof(struct internal_sc), M_DEVBUF, M_WAITOK);
		if (cl->cl_fsc == NULL)
			goto err_ret;
		bzero(cl->cl_fsc, sizeof(struct internal_sc));
		sc2isc(sc, cl->cl_fsc);
		rtsc_init(&cl->cl_virtual, cl->cl_fsc, 0, 0);
	}

	cl->cl_id = hif->hif_classid++;
	cl->cl_handle = (u_long)cl;  /* XXX: just a pointer to this class */
	cl->cl_hif = hif;
	cl->cl_parent = parent;

	s = splimp();
	hif->hif_classes++;
	if (flags & HFCF_DEFAULTCLASS)
		hif->hif_defaultclass = cl;

	/* add this class to the children list of the parent */
	if (parent == NULL) {
		/* this is root class */
	}
	else if ((p = parent->cl_children) == NULL)
		parent->cl_children = cl;
	else {
		while (p->cl_siblings != NULL)
			p = p->cl_siblings;
		p->cl_siblings = cl;
	}
	splx(s);

	return (cl);

 err_ret:
	if (cl->cl_actc != NULL)
		actlist_destroy(cl->cl_actc);
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
	if (cl->cl_fsc != NULL)
		FREE(cl->cl_fsc, M_DEVBUF);
	if (cl->cl_rsc != NULL)
		FREE(cl->cl_rsc, M_DEVBUF);
	if (cl->cl_q != NULL)
		FREE(cl->cl_q, M_DEVBUF);
	FREE(cl, M_DEVBUF);
	return (NULL);
}

static int
hfsc_class_destroy(cl)
	struct hfsc_class *cl;
{
	int s;

	if (is_a_parent_class(cl))
		return (EBUSY);

	s = splimp();

	/* delete filters referencing to this class */
	acc_discard_filters(&cl->cl_hif->hif_classifier, cl, 0);

	if (!qempty(cl->cl_q))
		hfsc_purgeq(cl);

	if (cl->cl_parent == NULL) {
		/* this is root class */
	} else {
		struct hfsc_class *p = cl->cl_parent->cl_children;

		if (p == cl)
			cl->cl_parent->cl_children = cl->cl_siblings;
		else do {
			if (p->cl_siblings == cl) {
				p->cl_siblings = cl->cl_siblings;
				break;
			}
		} while ((p = p->cl_siblings) != NULL);
		ASSERT(p != NULL);
	}
	cl->cl_hif->hif_classes--;
	splx(s);

	actlist_destroy(cl->cl_actc);

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
	if (cl->cl_fsc != NULL)
		FREE(cl->cl_fsc, M_DEVBUF);
	if (cl->cl_rsc != NULL)
		FREE(cl->cl_rsc, M_DEVBUF);
	FREE(cl->cl_q, M_DEVBUF);
	FREE(cl, M_DEVBUF);

	return (0);
}

static int
hfsc_class_modify(cl, rsc, fsc)
	struct hfsc_class *cl;
	struct service_curve *rsc, *fsc;
{
	struct internal_sc *rsc_tmp, *fsc_tmp;
	int s;

	if (rsc != NULL && (rsc->m1 != 0 || rsc->m2 != 0) &&
	    cl->cl_rsc == NULL) {
		MALLOC(rsc_tmp, struct internal_sc *,
		       sizeof(struct internal_sc), M_DEVBUF, M_WAITOK);
		if (rsc_tmp == NULL)
			return (ENOMEM);
	}
	if (fsc != NULL && (fsc->m1 != 0 || fsc->m2 != 0) &&
	    cl->cl_fsc == NULL) {
		MALLOC(fsc_tmp, struct internal_sc *,
		       sizeof(struct internal_sc), M_DEVBUF, M_WAITOK);
		if (fsc_tmp == NULL)
			return (ENOMEM);
	}

	s = splimp();
	if (!qempty(cl->cl_q))
		hfsc_purgeq(cl);

	if (rsc != NULL) {
		if (rsc->m1 == 0 && rsc->m2 == 0) {
			if (cl->cl_rsc != NULL) {
				FREE(cl->cl_rsc, M_DEVBUF);
				cl->cl_rsc = NULL;
			}
		} else {
			if (cl->cl_rsc == NULL)
				cl->cl_rsc = rsc_tmp;
			bzero(cl->cl_rsc, sizeof(struct internal_sc));
			sc2isc(rsc, cl->cl_rsc);
			rtsc_init(&cl->cl_deadline, cl->cl_rsc, 0, 0);
			rtsc_init(&cl->cl_eligible, cl->cl_rsc, 0, 0);
		}
	}

	if (fsc != NULL) {
		if (fsc->m1 == 0 && fsc->m2 == 0) {
			if (cl->cl_fsc != NULL) {
				FREE(cl->cl_fsc, M_DEVBUF);
				cl->cl_fsc = NULL;
			}
		} else {
			if (cl->cl_fsc == NULL)
				cl->cl_fsc = fsc_tmp;
			bzero(cl->cl_fsc, sizeof(struct internal_sc));
			sc2isc(fsc, cl->cl_fsc);
			rtsc_init(&cl->cl_virtual, cl->cl_fsc, 0, 0);
		}
	}
	splx(s);

	return (0);
}

/*
 * hfsc_nextclass returns the next class in the tree.
 *   usage:
 * 	for (cl = hif->hif_rootclass; cl != NULL; cl = hfsc_nextclass(cl))
 *		do_something;
 */
static struct hfsc_class *
hfsc_nextclass(cl)
	struct hfsc_class *cl;
{
	if (cl->cl_children != NULL)
		cl = cl->cl_children;
	else if (cl->cl_siblings != NULL)
		cl = cl->cl_siblings;
	else {
		while ((cl = cl->cl_parent) != NULL)
			if (cl->cl_siblings) {
				cl = cl->cl_siblings;
				break;
			}
	}

	return (cl);
}

/*
 * hfsc_enqueue is an enqueue function to be registered to
 * (*altq_enqueue) in struct ifaltq.
 */
static int
hfsc_enqueue(ifq, m, pktattr)
	struct ifaltq *ifq;
	struct mbuf *m;
	struct altq_pktattr *pktattr;
{
	struct hfsc_if	*hif = (struct hfsc_if *)ifq->altq_disc;
	struct hfsc_class *cl;
	int len;

	/* grab class set by classifier */
	if (pktattr == NULL || (cl = pktattr->pattr_class) == NULL)
		cl = hif->hif_defaultclass;
	cl->cl_pktattr = pktattr;  /* save proto hdr used by ECN */

	len = m_pktlen(m);
	if (hfsc_addq(cl, m) != 0) {
		/* drop occurred.  mbuf was freed in hfsc_addq. */
		PKTCNTR_ADD(&cl->cl_stats.drop_cnt, len);
		return (ENOBUFS);
	}
	IFQ_INC_LEN(ifq);
	cl->cl_hif->hif_packets++;

	/* successfully queued. */
	if (qlen(cl->cl_q) == 1)
		set_active(cl, m_pktlen(m));

#ifdef HFSC_PKTLOG
	/* put the logging_hook here */
#endif
	return (0);
}

/*
 * hfsc_dequeue is a dequeue function to be registered to
 * (*altq_dequeue) in struct ifaltq.
 *
 * note: ALTDQ_POLL returns the next packet without removing the packet
 *	from the queue.  ALTDQ_REMOVE is a normal dequeue operation.
 *	ALTDQ_REMOVE must return the same packet if called immediately
 *	after ALTDQ_POLL.
 */
static struct mbuf *
hfsc_dequeue(ifq, op)
	struct ifaltq	*ifq;
	int		op;
{
	struct hfsc_if	*hif = (struct hfsc_if *)ifq->altq_disc;
	struct hfsc_class *cl;
	struct mbuf *m;
	int len, next_len;
	int realtime = 0;

	if (hif->hif_packets == 0)
		/* no packet in the tree */
		return (NULL);

	if (op == ALTDQ_REMOVE && hif->hif_pollcache != NULL) {
		u_int64_t cur_time;

		cl = hif->hif_pollcache;
		hif->hif_pollcache = NULL;
		/* check if the class was scheduled by real-time criteria */
		if (cl->cl_rsc != NULL) {
			cur_time = read_machclk();
			realtime = (cl->cl_e <= cur_time);
		}
	} else {
		/*
		 * if there are eligible classes, use real-time criteria.
		 * find the class with the minimum deadline among
		 * the eligible classes.
		 */
		if ((cl = ellist_get_mindl(hif->hif_eligible)) != NULL) {
			realtime = 1;
		} else {
			/*
			 * use link-sharing criteria
			 * get the class with the minimum vt in the hierarchy
			 */
			cl = hif->hif_rootclass;
			while (is_a_parent_class(cl)) {
				cl = actlist_first(cl->cl_actc);
				if (cl == NULL)
					return (NULL);
			}
		}

		if (op == ALTDQ_POLL) {
			hif->hif_pollcache = cl;
			m = hfsc_pollq(cl);
			return (m);
		}
	}

	m = hfsc_getq(cl);
	len = m_pktlen(m);
	cl->cl_hif->hif_packets--;
	IFQ_DEC_LEN(ifq);
	PKTCNTR_ADD(&cl->cl_stats.xmit_cnt, len);

	update_v(cl, len);
	if (realtime)
		cl->cl_cumul += len;

	if (!qempty(cl->cl_q)) {
		if (cl->cl_rsc != NULL) {
			/* update ed */
			next_len = m_pktlen(qhead(cl->cl_q));

			if (realtime)
				update_ed(cl, next_len);
			else
				update_d(cl, next_len);
		}
	} else {
		/* the class becomes passive */
		set_passive(cl);
	}

#ifdef HFSC_PKTLOG
	/* put the logging_hook here */
#endif

	return (m);
}

static int
hfsc_addq(cl, m)
	struct hfsc_class *cl;
	struct mbuf *m;
{

#ifdef ALTQ_RIO
	if (q_is_rio(cl->cl_q))
		return rio_addq((rio_t *)cl->cl_red, cl->cl_q,
				m, cl->cl_pktattr);
#endif
#ifdef ALTQ_RED
	if (q_is_red(cl->cl_q))
		return red_addq(cl->cl_red, cl->cl_q, m, cl->cl_pktattr);
#endif
	if (qlen(cl->cl_q) >= qlimit(cl->cl_q)) {
		m_freem(m);
		return (-1);
	}

	if (cl->cl_flags & HFCF_CLEARDSCP)
		write_dsfield(m, cl->cl_pktattr, 0);

	_addq(cl->cl_q, m);

	return (0);
}

static struct mbuf *
hfsc_getq(cl)
	struct hfsc_class *cl;
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
hfsc_pollq(cl)
	struct hfsc_class *cl;
{
	return qhead(cl->cl_q);
}

static void
hfsc_purgeq(cl)
	struct hfsc_class *cl;
{
	struct mbuf *m;

	if (qempty(cl->cl_q))
		return;

	while ((m = _getq(cl->cl_q)) != NULL) {
		PKTCNTR_ADD(&cl->cl_stats.drop_cnt, m_pktlen(m));
		m_freem(m);
	}
	ASSERT(qlen(cl->cl_q) == 0);

	set_passive(cl);
}

static void
set_active(cl, len)
	struct hfsc_class *cl;
	int len;
{
	if (cl->cl_rsc != NULL)
		init_ed(cl, len);
	if (cl->cl_fsc != NULL)
		init_v(cl, len);

	cl->cl_stats.period++;
}

static void
set_passive(cl)
	struct hfsc_class *cl;
{
	if (cl->cl_rsc != NULL)
		ellist_remove(cl);

	if (cl->cl_fsc != NULL) {
		while (cl->cl_parent != NULL) {
			if (--cl->cl_nactive == 0) {
				/* remove this class from the vt list */
				actlist_remove(cl);
			} else
				/* still has active children */
				break;

			/* go up to the parent class */
			cl = cl->cl_parent;
		}
	}
}

static void
init_ed(cl, next_len)
	struct hfsc_class *cl;
	int next_len;
{
	u_int64_t cur_time;

	cur_time = read_machclk();

	/* update the deadline curve */
	rtsc_min(&cl->cl_deadline, cl->cl_rsc, cur_time, cl->cl_cumul);

	/*
	 * update the eligible curve.
	 * for concave, it is equal to the deadline curve.
	 * for convex, it is a linear curve with slope m2.
	 */
	cl->cl_eligible = cl->cl_deadline;
	if (cl->cl_rsc->sm1 <= cl->cl_rsc->sm2) {
		cl->cl_eligible.dx = 0;
		cl->cl_eligible.dy = 0;
	}

	/* compute e and d */
	cl->cl_e = rtsc_y2x(&cl->cl_eligible, cl->cl_cumul);
	cl->cl_d = rtsc_y2x(&cl->cl_deadline, cl->cl_cumul + next_len);

	ellist_insert(cl);
}

static void
update_ed(cl, next_len)
	struct hfsc_class *cl;
	int next_len;
{
	cl->cl_e = rtsc_y2x(&cl->cl_eligible, cl->cl_cumul);
	cl->cl_d = rtsc_y2x(&cl->cl_deadline, cl->cl_cumul + next_len);

	ellist_update(cl);
}

static void
update_d(cl, next_len)
	struct hfsc_class *cl;
	int next_len;
{
	cl->cl_d = rtsc_y2x(&cl->cl_deadline, cl->cl_cumul + next_len);
}

static void
init_v(cl, len)
	struct hfsc_class *cl;
	int len;
{
	struct hfsc_class *min_cl, *max_cl;

	while (cl->cl_parent != NULL) {

		if (cl->cl_nactive++ > 0)
			/* already active */
			break;

		/*
		 * if parent became idle while this class was idle.
		 * reset vt and the runtime service curve.
		 */
		if (cl->cl_parent->cl_nactive == 0 ||
		    cl->cl_parent->cl_vtperiod != cl->cl_parentperiod) {
			cl->cl_vt = 0;
			rtsc_init(&cl->cl_virtual, cl->cl_fsc,
				  0, cl->cl_total);
		}
		min_cl = actlist_first(cl->cl_parent->cl_actc);
		if (min_cl != NULL) {
			u_int64_t vt;

			/*
			 * set vt to the average of the min and max classes.
			 * if the parent's period didn't change,
			 * don't decrease vt of the class.
			 */
			max_cl = actlist_last(cl->cl_parent->cl_actc);
			vt = (min_cl->cl_vt + max_cl->cl_vt) / 2;
			if (cl->cl_parent->cl_vtperiod != cl->cl_parentperiod
			    || vt > cl->cl_vt)
				cl->cl_vt = vt;
		}

		/* update the virtual curve */
		rtsc_min(&cl->cl_virtual, cl->cl_fsc, cl->cl_vt, cl->cl_total);

		cl->cl_vtperiod++;  /* increment vt period */
		cl->cl_parentperiod = cl->cl_parent->cl_vtperiod;
		if (cl->cl_parent->cl_nactive == 0)
			cl->cl_parentperiod++;

		actlist_insert(cl);

		/* go up to the parent class */
		cl = cl->cl_parent;
	}
}

static void
update_v(cl, len)
	struct hfsc_class *cl;
	int len;
{
	while (cl->cl_parent != NULL) {

		cl->cl_total += len;

		if (cl->cl_fsc != NULL) {
			cl->cl_vt = rtsc_y2x(&cl->cl_virtual, cl->cl_total);

			/* update the vt list */
			actlist_update(cl);
		}

		/* go up to the parent class */
		cl = cl->cl_parent;
	}
}

/*
 * TAILQ based ellist and actlist implementation
 * (ion wanted to make a calendar queue based implementation)
 */
/*
 * eligible list holds backlogged classes being sorted by their eligible times.
 * there is one eligible list per interface.
 */

static ellist_t *
ellist_alloc()
{
	ellist_t *head;

	MALLOC(head, ellist_t *, sizeof(ellist_t), M_DEVBUF, M_WAITOK);
	TAILQ_INIT(head);
	return (head);
}

static void
ellist_destroy(head)
	ellist_t *head;
{
	FREE(head, M_DEVBUF);
}

static void
ellist_insert(cl)
	struct hfsc_class *cl;
{
	struct hfsc_if	*hif = cl->cl_hif;
	struct hfsc_class *p;

	/* check the last entry first */
	if ((p = TAILQ_LAST(hif->hif_eligible, _eligible)) == NULL ||
	    p->cl_e <= cl->cl_e) {
		TAILQ_INSERT_TAIL(hif->hif_eligible, cl, cl_ellist);
		return;
	}

	TAILQ_FOREACH(p, hif->hif_eligible, cl_ellist) {
		if (cl->cl_e < p->cl_e) {
			TAILQ_INSERT_BEFORE(p, cl, cl_ellist);
			return;
		}
	}
	ASSERT(0); /* should not reach here */
}

static void
ellist_remove(cl)
	struct hfsc_class *cl;
{
	struct hfsc_if	*hif = cl->cl_hif;

	TAILQ_REMOVE(hif->hif_eligible, cl, cl_ellist);
}

static void
ellist_update(cl)
	struct hfsc_class *cl;
{
	struct hfsc_if	*hif = cl->cl_hif;
	struct hfsc_class *p, *last;

	/*
	 * the eligible time of a class increases monotonically.
	 * if the next entry has a larger eligible time, nothing to do.
	 */
	p = TAILQ_NEXT(cl, cl_ellist);
	if (p == NULL || cl->cl_e <= p->cl_e)
		return;

	/* check the last entry */
	last = TAILQ_LAST(hif->hif_eligible, _eligible);
	ASSERT(last != NULL);
	if (last->cl_e <= cl->cl_e) {
		TAILQ_REMOVE(hif->hif_eligible, cl, cl_ellist);
		TAILQ_INSERT_TAIL(hif->hif_eligible, cl, cl_ellist);
		return;
	}

	/*
	 * the new position must be between the next entry
	 * and the last entry
	 */
	while ((p = TAILQ_NEXT(p, cl_ellist)) != NULL) {
		if (cl->cl_e < p->cl_e) {
			TAILQ_REMOVE(hif->hif_eligible, cl, cl_ellist);
			TAILQ_INSERT_BEFORE(p, cl, cl_ellist);
			return;
		}
	}
	ASSERT(0); /* should not reach here */
}

/* find the class with the minimum deadline among the eligible classes */
struct hfsc_class *
ellist_get_mindl(head)
	ellist_t *head;
{
	struct hfsc_class *p, *cl = NULL;
	u_int64_t cur_time;

	cur_time = read_machclk();

	TAILQ_FOREACH(p, head, cl_ellist) {
		if (p->cl_e > cur_time)
			break;
		if (cl == NULL || p->cl_d < cl->cl_d)
			cl = p;
	}
	return (cl);
}

/*
 * active children list holds backlogged child classes being sorted
 * by their virtual time.
 * each intermediate class has one active children list.
 */
static actlist_t *
actlist_alloc()
{
	actlist_t *head;

	MALLOC(head, actlist_t *, sizeof(actlist_t), M_DEVBUF, M_WAITOK);
	TAILQ_INIT(head);
	return (head);
}

static void
actlist_destroy(head)
	actlist_t *head;
{
	FREE(head, M_DEVBUF);
}
static void
actlist_insert(cl)
	struct hfsc_class *cl;
{
	struct hfsc_class *p;

	/* check the last entry first */
	if ((p = TAILQ_LAST(cl->cl_parent->cl_actc, _active)) == NULL
	    || p->cl_vt <= cl->cl_vt) {
		TAILQ_INSERT_TAIL(cl->cl_parent->cl_actc, cl, cl_actlist);
		return;
	}

	TAILQ_FOREACH(p, cl->cl_parent->cl_actc, cl_actlist) {
		if (cl->cl_vt < p->cl_vt) {
			TAILQ_INSERT_BEFORE(p, cl, cl_actlist);
			return;
		}
	}
	ASSERT(0); /* should not reach here */
}

static void
actlist_remove(cl)
	struct hfsc_class *cl;
{
	TAILQ_REMOVE(cl->cl_parent->cl_actc, cl, cl_actlist);
}

static void
actlist_update(cl)
	struct hfsc_class *cl;
{
	struct hfsc_class *p, *last;

	/*
	 * the virtual time of a class increases monotonically during its
	 * backlogged period.
	 * if the next entry has a larger virtual time, nothing to do.
	 */
	p = TAILQ_NEXT(cl, cl_actlist);
	if (p == NULL || cl->cl_vt <= p->cl_vt)
		return;

	/* check the last entry */
	last = TAILQ_LAST(cl->cl_parent->cl_actc, _active);
	ASSERT(last != NULL);
	if (last->cl_vt <= cl->cl_vt) {
		TAILQ_REMOVE(cl->cl_parent->cl_actc, cl, cl_actlist);
		TAILQ_INSERT_TAIL(cl->cl_parent->cl_actc, cl, cl_actlist);
		return;
	}

	/*
	 * the new position must be between the next entry
	 * and the last entry
	 */
	while ((p = TAILQ_NEXT(p, cl_actlist)) != NULL) {
		if (cl->cl_vt < p->cl_vt) {
			TAILQ_REMOVE(cl->cl_parent->cl_actc, cl, cl_actlist);
			TAILQ_INSERT_BEFORE(p, cl, cl_actlist);
			return;
		}
	}
	ASSERT(0); /* should not reach here */
}

/*
 * service curve support functions
 *
 *  external service curve parameters
 *	m: bits/sec
 *	d: msec
 *  internal service curve parameters
 *	sm: (bytes/tsc_interval) << SM_SHIFT
 *	ism: (tsc_count/byte) << ISM_SHIFT
 *	dx: tsc_count
 *
 * SM_SHIFT and ISM_SHIFT are scaled in order to keep effective digits.
 * we should be able to handle 100K-1Gbps linkspeed with 200Hz-1GHz CPU
 * speed.  SM_SHIFT and ISM_SHIFT are selected to have at least 3 effective
 * digits in decimal using the following table.
 *
 *  bits/set    100Kbps     1Mbps     10Mbps     100Mbps    1Gbps
 *  ----------+-------------------------------------------------------
 *  bytes/nsec  12.5e-6    125e-6     1250e-6    12500e-6   125000e-6
 *  sm(500MHz)  25.0e-6    250e-6     2500e-6    25000e-6   250000e-6
 *  sm(200MHz)  62.5e-6    625e-6     6250e-6    62500e-6   625000e-6
 *
 *  nsec/byte   80000      8000       800        80         8
 *  ism(500MHz) 40000      4000       400        40         4
 *  ism(200MHz) 16000      1600       160        16         1.6
 */
#define	SM_SHIFT	24
#define	ISM_SHIFT	10

#define	SC_LARGEVAL	(1LL << 32)
#define	SC_INFINITY	0xffffffffffffffffLL

static __inline u_int64_t
seg_x2y(x, sm)
	u_int64_t x;
	u_int64_t sm;
{
	u_int64_t y;

	if (x < SC_LARGEVAL)
		y = x * sm >> SM_SHIFT;
	else
		y = (x >> SM_SHIFT) * sm;
	return (y);
}

static __inline u_int64_t
seg_y2x(y, ism)
	u_int64_t y;
	u_int64_t ism;
{
	u_int64_t x;

	if (y == 0)
		x = 0;
	else if (ism == SC_INFINITY)
		x = SC_INFINITY;
	else if (y < SC_LARGEVAL)
		x = y * ism >> ISM_SHIFT;
	else
		x = (y >> ISM_SHIFT) * ism;
	return (x);
}

static __inline u_int64_t
m2sm(m)
	u_int m;
{
	u_int64_t sm;

	sm = ((u_int64_t)m << SM_SHIFT) / 8 / machclk_freq;
	return (sm);
}

static __inline u_int64_t
m2ism(m)
	u_int m;
{
	u_int64_t ism;

	if (m == 0)
		ism = SC_INFINITY;
	else
		ism = ((u_int64_t)machclk_freq << ISM_SHIFT) * 8 / m;
	return (ism);
}

static __inline u_int64_t
d2dx(d)
	u_int	d;
{
	u_int64_t dx;

	dx = ((u_int64_t)d * machclk_freq) / 1000;
	return (dx);
}

static u_int
sm2m(sm)
	u_int64_t sm;
{
	u_int64_t m;

	m = (sm * 8 * machclk_freq) >> SM_SHIFT;
	return ((u_int)m);
}

static u_int
dx2d(dx)
	u_int64_t dx;
{
	u_int64_t d;

	d = dx * 1000 / machclk_freq;
	return ((u_int)d);
}

static void
sc2isc(sc, isc)
	struct service_curve	*sc;
	struct internal_sc	*isc;
{
	isc->sm1 = m2sm(sc->m1);
	isc->ism1 = m2ism(sc->m1);
	isc->dx = d2dx(sc->d);
	isc->dy = seg_x2y(isc->dx, isc->sm1);
	isc->sm2 = m2sm(sc->m2);
	isc->ism2 = m2ism(sc->m2);
}

/*
 * initialize the runtime service curve with the given internal
 * service curve starting at (x, y).
 */
static void
rtsc_init(rtsc, isc, x, y)
	struct runtime_sc	*rtsc;
	struct internal_sc	*isc;
	u_int64_t		x, y;
{
	rtsc->x =	x;
	rtsc->y =	y;
	rtsc->sm1 =	isc->sm1;
	rtsc->ism1 =	isc->ism1;
	rtsc->dx =	isc->dx;
	rtsc->dy =	isc->dy;
	rtsc->sm2 =	isc->sm2;
	rtsc->ism2 =	isc->ism2;
}

/*
 * calculate the y-projection of the runtime service curve by the
 * given x-projection value
 */
static u_int64_t
rtsc_y2x(rtsc, y)
	struct runtime_sc	*rtsc;
	u_int64_t		y;
{
	u_int64_t	x;

	if (y < rtsc->y)
		x = rtsc->x;
	else if (y <= rtsc->y + rtsc->dy) {
		/* x belongs to the 1st segment */
		if (rtsc->dy == 0)
			x = rtsc->x + rtsc->dx;
		else
			x = rtsc->x + seg_y2x(y - rtsc->y, rtsc->ism1);
	} else {
		/* x belongs to the 2nd segment */
		x = rtsc->x + rtsc->dx
		    + seg_y2x(y - rtsc->y - rtsc->dy, rtsc->ism2);
	}
	return (x);
}

static u_int64_t
rtsc_x2y(rtsc, x)
	struct runtime_sc	*rtsc;
	u_int64_t		x;
{
	u_int64_t	y;

	if (x <= rtsc->x)
		y = rtsc->y;
	else if (x <= rtsc->x + rtsc->dx)
		/* y belongs to the 1st segment */
		y = rtsc->y + seg_x2y(x - rtsc->x, rtsc->sm1);
	else
		/* y belongs to the 2nd segment */
		y = rtsc->y + rtsc->dy
		    + seg_x2y(x - rtsc->x - rtsc->dx, rtsc->sm2);
	return (y);
}

/*
 * update the runtime service curve by taking the minimum of the current
 * runtime service curve and the service curve starting at (x, y).
 */
static void
rtsc_min(rtsc, isc, x, y)
	struct runtime_sc	*rtsc;
	struct internal_sc	*isc;
	u_int64_t		x, y;
{
	u_int64_t	y1, y2, dx, dy;

	if (isc->sm1 <= isc->sm2) {
		/* service curve is convex */
		y1 = rtsc_x2y(rtsc, x);
		if (y1 < y)
			/* the current rtsc is smaller */
			return;
		rtsc->x = x;
		rtsc->y = y;
		return;
	}

	/*
	 * service curve is concave
	 * compute the two y values of the current rtsc
	 *	y1: at x
	 *	y2: at (x + dx)
	 */
	y1 = rtsc_x2y(rtsc, x);
	if (y1 <= y) {
		/* rtsc is below isc, no change to rtsc */
		return;
	}

	y2 = rtsc_x2y(rtsc, x + isc->dx);
	if (y2 >= y + isc->dy) {
		/* rtsc is above isc, replace rtsc by isc */
		rtsc->x = x;
		rtsc->y = y;
		rtsc->dx = isc->dx;
		rtsc->dy = isc->dy;
		return;
	}

	/*
	 * the two curves intersect
	 * compute the offsets (dx, dy) using the reverse
	 * function of seg_x2y()
	 *	seg_x2y(dx, sm1) == seg_x2y(dx, sm2) + (y1 - y)
	 */
	dx = ((y1 - y) << SM_SHIFT) / (isc->sm1 - isc->sm2);
	/*
	 * check if (x, y1) belongs to the 1st segment of rtsc.
	 * if so, add the offset.
	 */
	if (rtsc->x + rtsc->dx > x)
		dx += rtsc->x + rtsc->dx - x;
	dy = seg_x2y(dx, isc->sm1);

	rtsc->x = x;
	rtsc->y = y;
	rtsc->dx = dx;
	rtsc->dy = dy;
	return;
}

/*
 * hfsc device interface
 */
int
hfscopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	if (machclk_freq == 0)
		init_machclk();

	if (machclk_freq == 0) {
		printf("hfsc: no cpu clock available!\n");
		return (ENXIO);
	}

	/* everything will be done when the queueing scheme is attached. */
	return 0;
}

int
hfscclose(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct hfsc_if *hif;
	int err, error = 0;

	while ((hif = hif_list) != NULL) {
		/* destroy all */
		if (ALTQ_IS_ENABLED(hif->hif_ifq))
			altq_disable(hif->hif_ifq);

		err = altq_detach(hif->hif_ifq);
		if (err == 0)
			err = hfsc_detach(hif);
		if (err != 0 && error == 0)
			error = err;
	}

	return error;
}

int
hfscioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	ioctlcmd_t cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct hfsc_if *hif;
	struct hfsc_interface *ifacep;
	int	error = 0;

	/* check super-user privilege */
	switch (cmd) {
	case HFSC_GETSTATS:
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

	case HFSC_IF_ATTACH:
		error = hfsccmd_if_attach((struct hfsc_attach *)addr);
		break;

	case HFSC_IF_DETACH:
		error = hfsccmd_if_detach((struct hfsc_interface *)addr);
		break;

	case HFSC_ENABLE:
	case HFSC_DISABLE:
	case HFSC_CLEAR_HIERARCHY:
		ifacep = (struct hfsc_interface *)addr;
		if ((hif = altq_lookup(ifacep->hfsc_ifname,
				       ALTQT_HFSC)) == NULL) {
			error = EBADF;
			break;
		}

		switch (cmd) {

		case HFSC_ENABLE:
			if (hif->hif_defaultclass == NULL) {
#if 1
				printf("hfsc: no default class\n");
#endif
				error = EINVAL;
				break;
			}
			error = altq_enable(hif->hif_ifq);
			break;

		case HFSC_DISABLE:
			error = altq_disable(hif->hif_ifq);
			break;

		case HFSC_CLEAR_HIERARCHY:
			hfsc_clear_interface(hif);
			break;
		}
		break;

	case HFSC_ADD_CLASS:
		error = hfsccmd_add_class((struct hfsc_add_class *)addr);
		break;

	case HFSC_DEL_CLASS:
		error = hfsccmd_delete_class((struct hfsc_delete_class *)addr);
		break;

	case HFSC_MOD_CLASS:
		error = hfsccmd_modify_class((struct hfsc_modify_class *)addr);
		break;

	case HFSC_ADD_FILTER:
		error = hfsccmd_add_filter((struct hfsc_add_filter *)addr);
		break;

	case HFSC_DEL_FILTER:
		error = hfsccmd_delete_filter((struct hfsc_delete_filter *)addr);
		break;

	case HFSC_GETSTATS:
		error = hfsccmd_class_stats((struct hfsc_class_stats *)addr);
		break;

	default:
		error = EINVAL;
		break;
	}
	return error;
}

static int
hfsccmd_if_attach(ap)
	struct hfsc_attach *ap;
{
	struct hfsc_if *hif;
	struct ifnet *ifp;
	int error;

	if ((ifp = ifunit(ap->iface.hfsc_ifname)) == NULL)
		return (ENXIO);

	if ((hif = hfsc_attach(&ifp->if_snd, ap->bandwidth)) == NULL)
		return (ENOMEM);

	/*
	 * set HFSC to this ifnet structure.
	 */
	if ((error = altq_attach(&ifp->if_snd, ALTQT_HFSC, hif,
				 hfsc_enqueue, hfsc_dequeue, hfsc_request,
				 &hif->hif_classifier, acc_classify)) != 0)
		(void)hfsc_detach(hif);

	return (error);
}

static int
hfsccmd_if_detach(ap)
	struct hfsc_interface *ap;
{
	struct hfsc_if *hif;
	int error;

	if ((hif = altq_lookup(ap->hfsc_ifname, ALTQT_HFSC)) == NULL)
		return (EBADF);

	if (ALTQ_IS_ENABLED(hif->hif_ifq))
		altq_disable(hif->hif_ifq);

	if ((error = altq_detach(hif->hif_ifq)))
		return (error);

	return hfsc_detach(hif);
}

static int
hfsccmd_add_class(ap)
	struct hfsc_add_class *ap;
{
	struct hfsc_if *hif;
	struct hfsc_class *cl, *parent;

	if ((hif = altq_lookup(ap->iface.hfsc_ifname, ALTQT_HFSC)) == NULL)
		return (EBADF);

	if ((parent = clh_to_clp(hif, ap->parent_handle)) == NULL) {
		if (ap->parent_handle == HFSC_ROOTCLASS_HANDLE)
			parent = hif->hif_rootclass;
		else
			return (EINVAL);
	}

	if ((cl = hfsc_class_create(hif, &ap->service_curve, parent,
				    ap->qlimit, ap->flags)) == NULL)
		return (ENOMEM);

	/* return a class handle to the user */
	ap->class_handle = clp_to_clh(cl);
	return (0);
}

static int
hfsccmd_delete_class(ap)
	struct hfsc_delete_class *ap;
{
	struct hfsc_if *hif;
	struct hfsc_class *cl;

	if ((hif = altq_lookup(ap->iface.hfsc_ifname, ALTQT_HFSC)) == NULL)
		return (EBADF);

	if ((cl = clh_to_clp(hif, ap->class_handle)) == NULL)
		return (EINVAL);

	return hfsc_class_destroy(cl);
}

static int
hfsccmd_modify_class(ap)
	struct hfsc_modify_class *ap;
{
	struct hfsc_if *hif;
	struct hfsc_class *cl;
	struct service_curve *rsc = NULL;
	struct service_curve *fsc = NULL;

	if ((hif = altq_lookup(ap->iface.hfsc_ifname, ALTQT_HFSC)) == NULL)
		return (EBADF);

	if ((cl = clh_to_clp(hif, ap->class_handle)) == NULL)
		return (EINVAL);

	if (ap->sctype & HFSC_REALTIMESC)
		rsc = &ap->service_curve;
	if (ap->sctype & HFSC_LINKSHARINGSC)
		fsc = &ap->service_curve;

	return hfsc_class_modify(cl, rsc, fsc);
}

static int
hfsccmd_add_filter(ap)
	struct hfsc_add_filter *ap;
{
	struct hfsc_if *hif;
	struct hfsc_class *cl;

	if ((hif = altq_lookup(ap->iface.hfsc_ifname, ALTQT_HFSC)) == NULL)
		return (EBADF);

	if ((cl = clh_to_clp(hif, ap->class_handle)) == NULL)
		return (EINVAL);

	if (is_a_parent_class(cl)) {
#if 1
		printf("hfsccmd_add_filter: not a leaf class!\n");
#endif
		return (EINVAL);
	}

	return acc_add_filter(&hif->hif_classifier, &ap->filter,
			      cl, &ap->filter_handle);
}

static int
hfsccmd_delete_filter(ap)
	struct hfsc_delete_filter *ap;
{
	struct hfsc_if *hif;

	if ((hif = altq_lookup(ap->iface.hfsc_ifname, ALTQT_HFSC)) == NULL)
		return (EBADF);

	return acc_delete_filter(&hif->hif_classifier,
				 ap->filter_handle);
}

static int
hfsccmd_class_stats(ap)
	struct hfsc_class_stats *ap;
{
	struct hfsc_if *hif;
	struct hfsc_class *cl;
	struct class_stats stats, *usp;
	int	n, nclasses, error;

	if ((hif = altq_lookup(ap->iface.hfsc_ifname, ALTQT_HFSC)) == NULL)
		return (EBADF);

	ap->cur_time = read_machclk();
	ap->hif_classes = hif->hif_classes;
	ap->hif_packets = hif->hif_packets;

	/* skip the first N classes in the tree */
	nclasses = ap->nskip;
	for (cl = hif->hif_rootclass, n = 0; cl != NULL && n < nclasses;
	     cl = hfsc_nextclass(cl), n++)
		;
	if (n != nclasses)
		return (EINVAL);

	/* then, read the next N classes in the tree */
	nclasses = ap->nclasses;
	usp = ap->stats;
	for (n = 0; cl != NULL && n < nclasses; cl = hfsc_nextclass(cl), n++) {

		get_class_stats(&stats, cl);

		if ((error = copyout((caddr_t)&stats, (caddr_t)usp++,
				     sizeof(stats))) != 0)
			return (error);
	}

	ap->nclasses = n;

	return (0);
}

static void get_class_stats(sp, cl)
	struct class_stats *sp;
	struct hfsc_class *cl;
{
	sp->class_id = cl->cl_id;
	sp->class_handle = clp_to_clh(cl);

	if (cl->cl_rsc != NULL) {
		sp->rsc.m1 = sm2m(cl->cl_rsc->sm1);
		sp->rsc.d = dx2d(cl->cl_rsc->dx);
		sp->rsc.m2 = sm2m(cl->cl_rsc->sm2);
	} else {
		sp->rsc.m1 = 0;
		sp->rsc.d = 0;
		sp->rsc.m2 = 0;
	}
	if (cl->cl_fsc != NULL) {
		sp->fsc.m1 = sm2m(cl->cl_fsc->sm1);
		sp->fsc.d = dx2d(cl->cl_fsc->dx);
		sp->fsc.m2 = sm2m(cl->cl_fsc->sm2);
	} else {
		sp->fsc.m1 = 0;
		sp->fsc.d = 0;
		sp->fsc.m2 = 0;
	}

	sp->total = cl->cl_total;
	sp->cumul = cl->cl_cumul;

	sp->d = cl->cl_d;
	sp->e = cl->cl_e;
	sp->vt = cl->cl_vt;

	sp->qlength = qlen(cl->cl_q);
	sp->xmit_cnt = cl->cl_stats.xmit_cnt;
	sp->drop_cnt = cl->cl_stats.drop_cnt;
	sp->period = cl->cl_stats.period;

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
static struct hfsc_class *
clh_to_clp(hif, chandle)
	struct hfsc_if *hif;
	u_long chandle;
{
	struct hfsc_class *cl;

	cl = (struct hfsc_class *)chandle;
	if (chandle != ALIGN(cl)) {
#if 1
		printf("clh_to_cl: unaligned pointer %p\n", cl);
#endif
		return (NULL);
	}

	if (cl == NULL || cl->cl_handle != chandle || cl->cl_hif != hif)
		return (NULL);

	return (cl);
}

/* convert a class pointer to the corresponding class handle */
static u_long
clp_to_clh(cl)
	struct hfsc_class *cl;
{
	if (cl->cl_parent == NULL)
		return (HFSC_ROOTCLASS_HANDLE);  /* XXX */
	return (cl->cl_handle);
}

#ifdef KLD_MODULE

static struct altqsw hfsc_sw =
	{"hfsc", hfscopen, hfscclose, hfscioctl};

ALTQ_MODULE(altq_hfsc, ALTQT_HFSC, &hfsc_sw);

#endif /* KLD_MODULE */

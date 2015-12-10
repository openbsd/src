/*	$OpenBSD: ifq.h,v 1.3 2015/12/10 03:05:17 dlg Exp $ */

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

#ifndef _NET_IFQ_H_
#define _NET_IFQ_H_

struct ifnet;

struct ifq_ops;

struct ifqueue {
	struct ifnet		*ifq_if;

	/* mbuf handling */
	struct mutex		 ifq_mtx;
	uint64_t		 ifq_drops;
	const struct ifq_ops	*ifq_ops;
	void			*ifq_q;
	unsigned int		 ifq_len;
	unsigned int		 ifq_oactive;

	/* work serialisation */
	struct mutex		 ifq_task_mtx;
	struct task_list	 ifq_task_list;
	unsigned int		 ifq_serializer;

	/* work to be serialised */
	struct task		 ifq_start;
	struct task		 ifq_restart;

	unsigned int		 ifq_maxlen;
};

#ifdef _KERNEL

#define IFQ_MAXLEN		256

/*
 * 
 * Interface Send Queues
 * 
 * struct ifqueue sits between the network stack and a drivers
 * transmission of packets. The high level view is that when the stack
 * has finished generating a packet it hands it to a driver for
 * transmission. It does this by queueing the packet on an ifqueue and
 * notifying the driver to start transmission of the queued packets.
 * 
 * struct ifqueue also provides the point where conditioning of
 * traffic (ie, priq and hfsc) is implemented, and provides some
 * infrastructure to assist in the implementation of network drivers.
 * 
 * = ifq API
 * 
 * The ifq API provides functions for three distinct consumers:
 * 
 * 1. The network stack
 * 2. Traffic QoS/conditioning implementations
 * 3. Network drivers
 * 
 * == Network Stack API
 * 
 * The network stack is responsible for initialising and destroying
 * the ifqueue structure, changing the traffic conditioner on an
 * interface queue, enqueuing packets for transmission, and notifying
 * the driver to start transmission.
 * 
 * === ifq_init()
 * 
 * During if_attach(), the network stack calls ifq_init to initialise
 * the ifqueue structure. By default it configures the priq traffic
 * conditioner.
 * 
 * === ifq_destroy()
 * 
 * The network stack calls ifq_destroy() during if_detach to tear down
 * the ifqueue structure. It frees the traffic conditioner state, and
 * frees any mbufs that were left queued.
 * 
 * === ifq_attach()
 * 
 * ifq_attach() is used to replace the current traffic conditioner on
 * the ifqueue. All the pending mbufs are removed from the previous
 * conditioner and requeued on the new.
 * 
 * === ifq_enqueue() and ifq_enqueue_try()
 * 
 * ifq_enqueue() and ifq_enqueue_try() attempt to fit an mbuf onto the
 * ifqueue. If the current traffic conditioner rejects the packet it
 * wont be queued and will be counted as a drop. ifq_enqueue() will
 * free the mbuf on the callers behalf if the packet is rejected.
 * ifq_enqueue_try() does not free the mbuf, allowing the caller to
 * reuse it.
 * 
 * === ifq_start()
 * 
 * Once a packet has been successfully queued with ifq_enqueue() or
 * ifq_enqueue_try(), the network card is notified with a call to
 * if_start(). If an interface is marked with IFXF_MPSAFE in its
 * if_xflags field, if_start() calls ifq_start() to dispatch the
 * interfaces start routine. Calls to ifq_start() run in the ifqueue
 * serialisation context, guaranteeing that only one instance of
 * ifp->if_start() will be running in the system at any point in time.
 * 
 * 
 * == Traffic conditioners API
 * 
 * The majority of interaction between struct ifqueue and a traffic
 * conditioner occurs via the callbacks a traffic conditioner provides
 * in an instance of struct ifq_ops.
 * 
 * XXX document ifqop_*
 * 
 * The ifqueue API implements the locking on behalf of the conditioning
 * implementations so conditioners only have to reject or keep mbufs.
 * If something needs to inspect a conditioners internals, the queue lock
 * needs to be taken to allow for a consistent or safe view. The queue
 * lock may be taken and released with ifq_q_enter() and ifq_q_leave().
 * 
 * === ifq_q_enter()
 * 
 * Code wishing to access a conditioners internals may take the queue
 * lock with ifq_q_enter(). The caller must pass a reference to the
 * conditioners ifq_ops structure so the infrastructure can ensure the
 * caller is able to understand the internals. ifq_q_enter() returns
 * a pointer to the conditions internal structures, or NULL if the
 * ifq_ops did not match the current conditioner.
 * 
 * === ifq_q_leave()
 * 
 * The queue lock acquired with ifq_q_enter() is released with
 * ifq_q_leave().
 * 
 * 
 * == Network Driver API
 * 
 * The API used by network drivers is mostly documented in the
 * ifq_dequeue(9) manpage except for ifq_serialize().
 * 
 * === ifq_serialize()
 * 
 * A driver may run arbitrary work in the ifqueue serialiser context
 * via ifq_serialize(). The work to be done is represented by a task
 * that has been prepared with task_set.
 * 
 * The work will be run in series with any other work dispatched by
 * ifq_start(), ifq_restart(), or other ifq_serialize() calls.
 * 
 * Because the work may be run on another CPU, the lifetime of the
 * task and the work it represents can extend beyond the end of the
 * call to ifq_serialize() that dispatched it.
 * 
 * 
 * = ifqueue work serialisation
 * 
 * ifqueues provide a mechanism to dispatch work to be run in a single
 * context. Work in this mechanism is represtented by task structures.
 * 
 * The tasks are run in a context similar to a taskq serviced by a
 * single kernel thread, except the work is run immediately by the
 * first CPU that dispatches work. If a second CPU attempts to dispatch
 * additional tasks while the first is still running, it will be queued
 * to be run by the first CPU. The second CPU will return immediately.
 * 
 * = MP Safe Network Drivers
 * 
 * An MP safe network driver is one in which its start routine can be
 * called by the network stack without holding the big kernel lock.
 * 
 * == Attach
 * 
 * A driver advertises it's ability to run its start routine by setting
 * the IFXF_MPSAFE flag in ifp->if_xflags before calling if_attach():
 * 
 * 	ifp->if_xflags = IFXF_MPSAFE;
 * 	ifp->if_start = drv_start;
 * 	if_attach(ifp);
 * 
 * The network stack will then wrap its calls to ifp->if_start with
 * ifq_start() to guarantee there is only one instance of that function
 * running in the system and to serialise it with other work the driver
 * may provide.
 * 
 * == Initialise
 * 
 * When the stack requests an interface be brought up (ie, drv_ioctl()
 * is called to handle SIOCSIFFLAGS with IFF_UP set in ifp->if_flags)
 * drivers should set IFF_RUNNING in ifp->if_flags and call
 * ifq_clr_oactive().
 * 
 * == if_start
 * 
 * ifq_start() checks that IFF_RUNNING is set in ifp->if_flags, that
 * ifq_is_oactive() does not return true, and that there are pending
 * packets to transmit via a call to ifq_len(). Therefore, drivers are
 * no longer responsible for doing this themselves.
 * 
 * If a driver should not transmit packets while its link is down, use
 * ifq_purge() to flush pending packets from the transmit queue.
 * 
 * Drivers for hardware should use the following pattern to transmit
 * packets:
 * 
 * 	void
 * 	drv_start(struct ifnet *ifp)
 * 	{
 * 		struct drv_softc *sc = ifp->if_softc;
 * 		struct mbuf *m;
 * 		int kick = 0;
 * 
 * 		if (NO_LINK) {
 * 			ifq_purge(&ifp->if_snd);
 * 			return;
 * 		}
 * 
 * 		for (;;) {
 * 			if (NO_SPACE) {
 * 				ifq_set_oactive(&ifp->if_snd);
 * 				break;
 * 			}
 * 
 * 			m = ifq_dequeue(&ifp->if_snd);
 * 			if (m == NULL)
 * 				break;
 * 
 * 			if (drv_encap(sc, m) != 0) { // map and fill ring
 * 				m_freem(m);
 * 				continue;
 * 			}
 * 
 * 			bpf_mtap();
 * 		}
 *  
 *  		drv_kick(sc); // notify hw of new descriptors on the ring
 * 	 }
 * 
 * == Transmission completion
 * 
 * The following pattern should be used for transmit queue interrupt
 * processing:
 * 
 * 	void
 * 	drv_txeof(struct drv_softc *sc)
 * 	{
 * 		struct ifnet *ifp = &sc->sc_if;
 * 
 * 		while (COMPLETED_PKTS) {
 * 			// unmap packets, m_freem() the mbufs.
 * 		}
 * 
 * 		if (ifq_is_oactive(&ifp->if_snd))
 * 			ifq_restart(&ifp->if_snd);
 * 	}
 * 
 * == Stop
 * 
 * Bringing an interface down (ie, IFF_UP was cleared in ifp->if_flags)
 * should clear IFF_RUNNING in ifp->if_flags, and guarantee the start
 * routine is not running before freeing any resources it uses:
 * 
 * 	void
 * 	drv_down(struct drv_softc *sc)
 * 	{
 * 		struct ifnet *ifp = &sc->sc_if;
 * 
 * 		CLR(ifp->if_flags, IFF_RUNNING);
 * 		DISABLE_INTERRUPTS();
 * 
 * 		ifq_barrier(&ifp->if_snd);
 * 		intr_barrier(sc->sc_ih);
 * 
 * 		FREE_RESOURCES();
 * 
 * 		ifq_clr_oactive();
 * 	}
 * 
 */

struct ifq_ops {
	void			*(*ifqop_alloc)(void *);
	void			 (*ifqop_free)(void *);
	int			 (*ifqop_enq)(struct ifqueue *, struct mbuf *);
	struct mbuf 		*(*ifqop_deq_begin)(struct ifqueue *, void **);
	void			 (*ifqop_deq_commit)(struct ifqueue *,
				    struct mbuf *, void *);
	void	 		 (*ifqop_purge)(struct ifqueue *,
				    struct mbuf_list *);
};

/*
 * Interface send queues.
 */

void		 ifq_init(struct ifqueue *, struct ifnet *);
void		 ifq_attach(struct ifqueue *, const struct ifq_ops *, void *);
void		 ifq_destroy(struct ifqueue *);
int		 ifq_enqueue_try(struct ifqueue *, struct mbuf *);
int		 ifq_enqueue(struct ifqueue *, struct mbuf *);
struct mbuf	*ifq_deq_begin(struct ifqueue *);
void		 ifq_deq_commit(struct ifqueue *, struct mbuf *);
void		 ifq_deq_rollback(struct ifqueue *, struct mbuf *);
struct mbuf	*ifq_dequeue(struct ifqueue *);
unsigned int	 ifq_purge(struct ifqueue *);
void		*ifq_q_enter(struct ifqueue *, const struct ifq_ops *);
void		 ifq_q_leave(struct ifqueue *, void *);
void		 ifq_serialize(struct ifqueue *, struct task *);
void		 ifq_barrier(struct ifqueue *);

#define	ifq_len(_ifq)			((_ifq)->ifq_len)
#define	ifq_empty(_ifq)			(ifq_len(_ifq) == 0)
#define	ifq_set_maxlen(_ifq, _l)	((_ifq)->ifq_maxlen = (_l))

static inline void
ifq_set_oactive(struct ifqueue *ifq)
{
	ifq->ifq_oactive = 1;
}

static inline void
ifq_clr_oactive(struct ifqueue *ifq)
{
	ifq->ifq_oactive = 0;
}

static inline unsigned int
ifq_is_oactive(struct ifqueue *ifq)
{
	return (ifq->ifq_oactive);
}

static inline void
ifq_start(struct ifqueue *ifq)
{
	ifq_serialize(ifq, &ifq->ifq_start);
}

static inline void
ifq_restart(struct ifqueue *ifq)
{
	ifq_serialize(ifq, &ifq->ifq_restart);
}

extern const struct ifq_ops * const ifq_priq_ops;

#endif /* _KERNEL */

#endif /* _NET_IFQ_H_ */

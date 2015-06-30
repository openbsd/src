/*	$OpenBSD: if_var.h,v 1.33 2015/06/30 13:54:42 mpi Exp $	*/
/*	$NetBSD: if.h,v 1.23 1996/05/07 02:40:27 thorpej Exp $	*/

/*
 * Copyright (c) 2012-2013 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)if.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NET_IF_VAR_H_
#define _NET_IF_VAR_H_

#include <sys/queue.h>
#include <sys/mbuf.h>
#ifdef _KERNEL
#include <net/hfsc.h>
#endif

/*
 * Structures defining a network interface, providing a packet
 * transport mechanism (ala level 0 of the PUP protocols).
 *
 * Each interface accepts output datagrams of a specified maximum
 * length, and provides higher level routines with input datagrams
 * received from its medium.
 *
 * Output occurs when the routine if_output is called, with four parameters:
 *	(*ifp->if_output)(ifp, m, dst, rt)
 * Here m is the mbuf chain to be sent and dst is the destination address.
 * The output routine encapsulates the supplied datagram if necessary,
 * and then transmits it on its medium.
 *
 * On input, each interface unwraps the data received by it, and either
 * places it on the input queue of an internetwork datagram routine
 * and posts the associated software interrupt, or passes the datagram to a raw
 * packet input routine.
 *
 * Routines exist for locating interfaces by their addresses
 * or for locating an interface on a certain network, as well as more general
 * routing and gateway routines maintaining information used to locate
 * interfaces.  These routines live in the files if.c and route.c
 */

#include <sys/time.h>

struct proc;
struct rtentry;
struct socket;
struct timeout;
struct ether_header;
struct arpcom;
struct rt_addrinfo;
struct ifnet;
struct hfsc_if;
struct task;

/*
 * Structure describing a `cloning' interface.
 */
struct if_clone {
	LIST_ENTRY(if_clone)	 ifc_list;	/* on list of cloners */
	const char		*ifc_name;	/* name of device, e.g. `gif' */
	size_t			 ifc_namelen;	/* length of name */

	int			(*ifc_create)(struct if_clone *, int);
	int			(*ifc_destroy)(struct ifnet *);
};

#define	IF_CLONE_INITIALIZER(name, create, destroy)			\
	{ { 0 }, name, sizeof(name) - 1, create, destroy }

/*
 * Structure defining a queue for a network interface.
 */
struct	ifqueue {
	struct {
		struct	mbuf *head;
		struct	mbuf *tail;
	}			 ifq_q[IFQ_NQUEUES];
	int			 ifq_len;
	int			 ifq_maxlen;
	int			 ifq_drops;
	struct hfsc_if		*ifq_hfsc;
};

/*
 * Interface input hooks.
 */
struct ifih {
	SLIST_ENTRY(ifih) ifih_next;
	int		(*ifih_input)(struct mbuf *);
	int		  ifih_refcnt;
};

/*
 * Structure defining a queue for a network interface.
 *
 * (Would like to call this struct ``if'', but C isn't PL/1.)
 */
TAILQ_HEAD(ifnet_head, ifnet);		/* the actual queue head */

struct ifnet {				/* and the entries */
	void	*if_softc;		/* lower-level data for this if */
	TAILQ_ENTRY(ifnet) if_list;	/* all struct ifnets are chained */
	TAILQ_ENTRY(ifnet) if_txlist;	/* list of ifnets ready to tx */
	TAILQ_HEAD(, ifaddr) if_addrlist; /* linked list of addresses per if */
	TAILQ_HEAD(, ifmaddr) if_maddrlist; /* list of multicast records */
	TAILQ_HEAD(, ifg_list) if_groups; /* linked list of groups per if */
	struct hook_desc_head *if_addrhooks; /* address change callbacks */
	struct hook_desc_head *if_linkstatehooks; /* link change callbacks */
	struct hook_desc_head *if_detachhooks; /* detach callbacks */
	char	if_xname[IFNAMSIZ];	/* external name (name + unit) */
	int	if_pcount;		/* number of promiscuous listeners */
	caddr_t	if_bpf;			/* packet filter structure */
	caddr_t if_bridgeport;		/* used by bridge ports */
	caddr_t	if_tp;			/* used by trunk ports */
	caddr_t	if_pf_kif;		/* pf interface abstraction */
	union {
		caddr_t	carp_s;		/* carp structure (used by !carp ifs) */
		struct ifnet *carp_d;	/* ptr to carpdev (used by carp ifs) */
	} if_carp_ptr;
#define if_carp		if_carp_ptr.carp_s
#define if_carpdev	if_carp_ptr.carp_d
	u_short	if_index;		/* numeric abbreviation for this if */
	short	if_timer;		/* time 'til if_watchdog called */
	short	if_flags;		/* up/down, broadcast, etc. */
	int	if_xflags;		/* extra softnet flags */
	struct	if_data if_data;	/* stats and other data about if */
	u_int32_t if_hardmtu;		/* maximum MTU device supports */
	char	if_description[IFDESCRSIZE]; /* interface description */
	u_short	if_rtlabelid;		/* next route label */
	u_int8_t if_priority;
	struct	timeout *if_slowtimo;	/* watchdog timeout */
	struct	task *if_linkstatetask; /* task to do route updates */

	/* procedure handles */
	SLIST_HEAD(, ifih) if_inputs;	/* input routines (dequeue) */

					/* output routine (enqueue) */
	int	(*if_output)(struct ifnet *, struct mbuf *, struct sockaddr *,
		     struct rtentry *);

					/* link level output function */
	int	(*if_ll_output)(struct ifnet *, struct mbuf *,
		    struct sockaddr *, struct rtentry *);
					/* initiate output routine */
	void	(*if_start)(struct ifnet *);
					/* ioctl routine */
	int	(*if_ioctl)(struct ifnet *, u_long, caddr_t);
					/* stop routine */
	int	(*if_stop)(struct ifnet *, int);
					/* timer routine */
	void	(*if_watchdog)(struct ifnet *);
	int	(*if_wol)(struct ifnet *, int);
	struct	ifqueue if_snd;		/* output queue */
	struct sockaddr_dl *if_sadl;	/* pointer to our sockaddr_dl */

	void	*if_afdata[AF_MAX];
};
#define	if_mtu		if_data.ifi_mtu
#define	if_type		if_data.ifi_type
#define	if_addrlen	if_data.ifi_addrlen
#define	if_hdrlen	if_data.ifi_hdrlen
#define	if_metric	if_data.ifi_metric
#define	if_link_state	if_data.ifi_link_state
#define	if_baudrate	if_data.ifi_baudrate
#define	if_ipackets	if_data.ifi_ipackets
#define	if_ierrors	if_data.ifi_ierrors
#define	if_opackets	if_data.ifi_opackets
#define	if_oerrors	if_data.ifi_oerrors
#define	if_collisions	if_data.ifi_collisions
#define	if_ibytes	if_data.ifi_ibytes
#define	if_obytes	if_data.ifi_obytes
#define	if_imcasts	if_data.ifi_imcasts
#define	if_omcasts	if_data.ifi_omcasts
#define	if_iqdrops	if_data.ifi_iqdrops
#define	if_noproto	if_data.ifi_noproto
#define	if_lastchange	if_data.ifi_lastchange
#define	if_capabilities	if_data.ifi_capabilities
#define	if_rdomain	if_data.ifi_rdomain

/*
 * The ifaddr structure contains information about one address
 * of an interface.  They are maintained by the different address families,
 * are allocated and attached when an address is set, and are linked
 * together so all addresses for an interface can be located.
 */
struct ifaddr {
	struct	sockaddr *ifa_addr;	/* address of interface */
	struct	sockaddr *ifa_dstaddr;	/* other end of p-to-p link */
#define	ifa_broadaddr	ifa_dstaddr	/* broadcast address interface */
	struct	sockaddr *ifa_netmask;	/* used to determine subnet */
	struct	ifnet *ifa_ifp;		/* back-pointer to interface */
	TAILQ_ENTRY(ifaddr) ifa_list;	/* list of addresses for interface */
					/* check or clean routes (+ or -)'d */
	void	(*ifa_rtrequest)(int, struct rtentry *);
	u_int	ifa_flags;		/* interface flags, see below */
	u_int	ifa_refcnt;		/* number of `rt_ifa` references */
	int	ifa_metric;		/* cost of going out this interface */
};

#define	IFA_ROUTE		0x01	/* Auto-magically installed route */

/*
 * Interface multicast address.
 */
struct ifmaddr {
	struct sockaddr		*ifma_addr;	/* Protocol address */
	unsigned short		 ifma_ifidx;	/* Index of the interface */
	unsigned int		 ifma_refcnt;	/* Count of references */
	TAILQ_ENTRY(ifmaddr)	 ifma_list;	/* Per-interface list */
};

/*
 * interface groups
 */

struct ifg_group {
	char			 ifg_group[IFNAMSIZ];
	u_int			 ifg_refcnt;
	caddr_t			 ifg_pf_kif;
	int			 ifg_carp_demoted;
	TAILQ_HEAD(, ifg_member) ifg_members;
	TAILQ_ENTRY(ifg_group)	 ifg_next;
};

struct ifg_member {
	TAILQ_ENTRY(ifg_member)	 ifgm_next;
	struct ifnet		*ifgm_ifp;
};

struct ifg_list {
	struct ifg_group	*ifgl_group;
	TAILQ_ENTRY(ifg_list)	 ifgl_next;
};

#ifdef _KERNEL
#define	IFQ_MAXLEN	256
#define	IFNET_SLOWHZ	1		/* granularity is 1 second */

/*
 * Output queues (ifp->if_snd) and internetwork datagram level (pup level 1)
 * input routines have queues of messages stored on ifqueue structures
 * (defined above).  Entries are added to and deleted from these structures
 * by these macros, which should be called with ipl raised to splnet().
 */
#define	IF_QFULL(ifq)		((ifq)->ifq_len >= (ifq)->ifq_maxlen)
#define	IF_DROP(ifq)		((ifq)->ifq_drops++)
#define	IF_ENQUEUE(ifq, m)						\
do {									\
	(m)->m_nextpkt = NULL;						\
	if ((ifq)->ifq_q[(m)->m_pkthdr.pf.prio].tail == NULL)		\
		(ifq)->ifq_q[(m)->m_pkthdr.pf.prio].head = m;		\
	else								\
		(ifq)->ifq_q[(m)->m_pkthdr.pf.prio].tail->m_nextpkt = m; \
	(ifq)->ifq_q[(m)->m_pkthdr.pf.prio].tail = m;			\
	(ifq)->ifq_len++;						\
} while (/* CONSTCOND */0)
#define	IF_PREPEND(ifq, m)						\
do {									\
	(m)->m_nextpkt = (ifq)->ifq_q[(m)->m_pkthdr.pf.prio].head;	\
	if ((ifq)->ifq_q[(m)->m_pkthdr.pf.prio].tail == NULL)		\
		(ifq)->ifq_q[(m)->m_pkthdr.pf.prio].tail = (m);		\
	(ifq)->ifq_q[(m)->m_pkthdr.pf.prio].head = (m);			\
	(ifq)->ifq_len++;						\
} while (/* CONSTCOND */0)

#define	IF_POLL(ifq, m)							\
do {									\
	int	if_dequeue_prio = IFQ_MAXPRIO;				\
	do {								\
		(m) = (ifq)->ifq_q[if_dequeue_prio].head;		\
	} while (!(m) && --if_dequeue_prio >= 0); 			\
} while (/* CONSTCOND */0)

#define	IF_DEQUEUE(ifq, m)						\
do {									\
	int	if_dequeue_prio = IFQ_MAXPRIO;				\
	do {								\
		(m) = (ifq)->ifq_q[if_dequeue_prio].head;		\
		if (m) {						\
			if (((ifq)->ifq_q[if_dequeue_prio].head =	\
			    (m)->m_nextpkt) == NULL)			\
				(ifq)->ifq_q[if_dequeue_prio].tail = NULL; \
			(m)->m_nextpkt = NULL;				\
			(ifq)->ifq_len--;				\
		}							\
	} while (!(m) && --if_dequeue_prio >= 0);			\
} while (/* CONSTCOND */0)

#define	IF_PURGE(ifq)							\
do {									\
	struct mbuf *__m0;						\
									\
	for (;;) {							\
		IF_DEQUEUE((ifq), __m0);				\
		if (__m0 == NULL)					\
			break;						\
		else							\
			m_freem(__m0);					\
	}								\
} while (/* CONSTCOND */0)
#define	IF_LEN(ifq)		((ifq)->ifq_len)
#define	IF_IS_EMPTY(ifq)	((ifq)->ifq_len == 0)

/* XXX pattr unused */
#define	IFQ_ENQUEUE(ifq, m, pattr, err)					\
do {									\
	if (HFSC_ENABLED(ifq))						\
		(err) = hfsc_enqueue(((struct ifqueue *)(ifq)), m);	\
	else {								\
		if (IF_QFULL((ifq))) {					\
			m_freem((m));					\
			(err) = ENOBUFS;				\
		} else {						\
			IF_ENQUEUE((ifq), (m));				\
			(err) = 0;					\
		}							\
	}								\
	if ((err))							\
		(ifq)->ifq_drops++;					\
} while (/* CONSTCOND */0)

#define	IFQ_DEQUEUE(ifq, m)						\
do {									\
	if (HFSC_ENABLED((ifq)))					\
		(m) = hfsc_dequeue(((struct ifqueue *)(ifq)), 1);	\
	else								\
		IF_DEQUEUE((ifq), (m));					\
} while (/* CONSTCOND */0)

#define	IFQ_POLL(ifq, m)						\
do {									\
	if (HFSC_ENABLED((ifq)))					\
		(m) = hfsc_dequeue(((struct ifqueue *)(ifq)), 0);	\
	else								\
		IF_POLL((ifq), (m));					\
} while (/* CONSTCOND */0)

#define	IFQ_PURGE(ifq)							\
do {									\
	if (HFSC_ENABLED((ifq)))					\
		hfsc_purge(((struct ifqueue *)(ifq)));			\
	else								\
		IF_PURGE((ifq));					\
} while (/* CONSTCOND */0)

#define	IFQ_SET_READY(ifq)	/* nothing */

#define	IFQ_LEN(ifq)			IF_LEN(ifq)
#define	IFQ_IS_EMPTY(ifq)		((ifq)->ifq_len == 0)
#define	IFQ_SET_MAXLEN(ifq, len)	((ifq)->ifq_maxlen = (len))

/* default interface priorities */
#define IF_WIRED_DEFAULT_PRIORITY	0
#define IF_WIRELESS_DEFAULT_PRIORITY	4
#define IF_CARP_DEFAULT_PRIORITY	15

/*
 * Network stack input queues.
 */
struct	niqueue {
	struct mbuf_queue	ni_q;
	u_int			ni_isr;
};

#define NIQUEUE_INITIALIZER(_len, _isr) \
    { MBUF_QUEUE_INITIALIZER((_len), IPL_NET), (_isr) }

void		niq_init(struct niqueue *, u_int, u_int);
int		niq_enqueue(struct niqueue *, struct mbuf *);
int		niq_enlist(struct niqueue *, struct mbuf_list *);

#define niq_dequeue(_q)			mq_dequeue(&(_q)->ni_q)
#define niq_dechain(_q)			mq_dechain(&(_q)->ni_q)
#define niq_delist(_q, _ml)		mq_delist(&(_q)->ni_q, (_ml))
#define niq_filter(_q, _f, _c)		mq_filter(&(_q)->ni_q, (_f), (_c))
#define niq_len(_q)			mq_len(&(_q)->ni_q)
#define niq_drops(_q)			mq_drops(&(_q)->ni_q)
#define sysctl_niq(_n, _l, _op, _olp, _np, _nl, _niq) \
    sysctl_mq((_n), (_l), (_op), (_olp), (_np), (_nl), &(_niq)->ni_q)

extern struct ifnet_head ifnet;
extern struct ifnet *lo0ifp;

void	if_start(struct ifnet *);
int	if_enqueue(struct ifnet *, struct mbuf *);
void	if_input(struct ifnet *, struct mbuf_list *);

void	ether_ifattach(struct ifnet *);
void	ether_ifdetach(struct ifnet *);
int	ether_ioctl(struct ifnet *, struct arpcom *, u_long, caddr_t);
int	ether_input(struct mbuf *);
int	ether_output(struct ifnet *,
	    struct mbuf *, struct sockaddr *, struct rtentry *);
char	*ether_sprintf(u_char *);

struct	ifaddr *ifa_ifwithaddr(struct sockaddr *, u_int);
struct	ifaddr *ifa_ifwithdstaddr(struct sockaddr *, u_int);
struct	ifaddr *ifa_ifwithnet(struct sockaddr *, u_int);
struct	ifaddr *ifaof_ifpforaddr(struct sockaddr *, struct ifnet *);
void	ifafree(struct ifaddr *);
void	link_rtrequest(int, struct rtentry *);
void	p2p_rtrequest(int, struct rtentry *);

void	if_clone_attach(struct if_clone *);
void	if_clone_detach(struct if_clone *);

int	if_clone_create(const char *);
int	if_clone_destroy(const char *);

int     sysctl_mq(int *, u_int, void *, size_t *, void *, size_t,
	    struct mbuf_queue *);

int	loioctl(struct ifnet *, u_long, caddr_t);
void	loopattach(int);
int	looutput(struct ifnet *,
	    struct mbuf *, struct sockaddr *, struct rtentry *);
void	lortrequest(int, struct rtentry *);
void	ifa_add(struct ifnet *, struct ifaddr *);
void	ifa_del(struct ifnet *, struct ifaddr *);
void	ifa_update_broadaddr(struct ifnet *, struct ifaddr *,
	    struct sockaddr *);

void	if_rxr_init(struct if_rxring *, u_int, u_int);
u_int	if_rxr_get(struct if_rxring *, u_int);

#define if_rxr_put(_r, _c)	do { (_r)->rxr_alive -= (_c); } while (0)
#define if_rxr_inuse(_r)	((_r)->rxr_alive)

int	if_rxr_info_ioctl(struct if_rxrinfo *, u_int, struct if_rxring_info *);
int	if_rxr_ioctl(struct if_rxrinfo *, const char *, u_int,
	    struct if_rxring *);

#endif /* _KERNEL */

#endif /* _NET_IF_VAR_H_ */

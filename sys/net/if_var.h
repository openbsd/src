/*	$OpenBSD: if_var.h,v 1.93 2018/12/20 10:26:36 claudio Exp $	*/
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

#ifdef _KERNEL

#include <sys/queue.h>
#include <sys/mbuf.h>
#include <sys/srp.h>
#include <sys/refcnt.h>
#include <sys/task.h>
#include <sys/time.h>
#include <sys/timeout.h>

#include <net/ifq.h>

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

struct rtentry;
struct ifnet;
struct task;
struct cpumem;

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
{									\
  .ifc_list	= { NULL, NULL },					\
  .ifc_name	= name,							\
  .ifc_namelen	= sizeof(name) - 1,					\
  .ifc_create	= create,						\
  .ifc_destroy	= destroy,						\
}

/*
 *  Locks used to protect struct members in this file:
 *	I	immutable after creation
 *	d	protection left do the driver
 *	c	only used in ioctl or routing socket contexts (kernel lock)
 *	k	kernel lock
 *	N	net lock
 */
/*
 * Structure defining a queue for a network interface.
 *
 * (Would like to call this struct ``if'', but C isn't PL/1.)
 */
TAILQ_HEAD(ifnet_head, ifnet);		/* the actual queue head */

struct ifnet {				/* and the entries */
	void	*if_softc;		/* lower-level data for this if */
	struct	refcnt if_refcnt;
	TAILQ_ENTRY(ifnet) if_list;	/* [k] all struct ifnets are chained */
	TAILQ_HEAD(, ifaddr) if_addrlist; /* [N] list of addresses per if */
	TAILQ_HEAD(, ifmaddr) if_maddrlist; /* [N] list of multicast records */
	TAILQ_HEAD(, ifg_list) if_groups; /* [N] list of groups per if */
	struct hook_desc_head *if_addrhooks; /* [I] address change callbacks */
	struct hook_desc_head *if_linkstatehooks; /* [I] link change callbacks*/
	struct hook_desc_head *if_detachhooks; /* [I] detach callbacks */
				/* [I] check or clean routes (+ or -)'d */
	void	(*if_rtrequest)(struct ifnet *, int, struct rtentry *);
	char	if_xname[IFNAMSIZ];	/* [I] external name (name + unit) */
	int	if_pcount;		/* [k] # of promiscuous listeners */
	caddr_t	if_bpf;			/* packet filter structure */
	caddr_t if_bridgeport;		/* used by bridge ports */
	caddr_t if_switchport;		/* used by switch ports */
	caddr_t if_mcast;		/* used by multicast code */
	caddr_t if_mcast6;		/* used by IPv6 multicast code */
	caddr_t	if_pf_kif;		/* pf interface abstraction */
	union {
		struct srpl carp_s;	/* carp if list (used by !carp ifs) */
		struct ifnet *carp_d;	/* ptr to carpdev (used by carp ifs) */
	} if_carp_ptr;
#define if_carp		if_carp_ptr.carp_s
#define if_carpdev	if_carp_ptr.carp_d
	unsigned int if_index;		/* [I] unique index for this if */
	short	if_timer;		/* time 'til if_watchdog called */
	unsigned short if_flags;	/* [N] up/down, broadcast, etc. */
	int	if_xflags;		/* [N] extra softnet flags */
	struct	if_data if_data;	/* stats and other data about if */
	struct	cpumem *if_counters;	/* per cpu stats */
	uint32_t if_hardmtu;		/* [d] maximum MTU device supports */
	char	if_description[IFDESCRSIZE]; /* [c] interface description */
	u_short	if_rtlabelid;		/* [c] next route label */
	uint8_t if_priority;		/* [c] route priority offset */
	uint8_t if_llprio;		/* [N] link layer priority */
	struct	timeout if_slowtimo;	/* [I] watchdog timeout */
	struct	task if_watchdogtask;	/* [I] watchdog task */
	struct	task if_linkstatetask;	/* [I] task to do route updates */

	/* procedure handles */
	SRPL_HEAD(, ifih) if_inputs;	/* input routines (dequeue) */
	int	(*if_output)(struct ifnet *, struct mbuf *, struct sockaddr *,
		     struct rtentry *);	/* output routine (enqueue) */
					/* link level output function */
	int	(*if_ll_output)(struct ifnet *, struct mbuf *,
		    struct sockaddr *, struct rtentry *);
	void	(*if_start)(struct ifnet *);	/* initiate output */
	int	(*if_ioctl)(struct ifnet *, u_long, caddr_t); /* ioctl hook */
	void	(*if_watchdog)(struct ifnet *);	/* timer routine */
	int	(*if_wol)(struct ifnet *, int);	/* WoL routine **/

	/* queues */
	struct	ifqueue if_snd;		/* transmit queue */
	struct	ifqueue **if_ifqs;	/* [I] pointer to an array of sndqs */
	void	(*if_qstart)(struct ifqueue *);
	unsigned int if_nifqs;		/* [I] number of output queues */

	struct	ifiqueue if_rcv;	/* rx/input queue */
	struct	ifiqueue **if_iqs;	/* [I] pointer to the array of iqs */
	unsigned int if_niqs;		/* [I] number of input queues */

	struct sockaddr_dl *if_sadl;	/* [N] pointer to our sockaddr_dl */

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
#define	if_oqdrops	if_data.ifi_oqdrops
#define	if_noproto	if_data.ifi_noproto
#define	if_lastchange	if_data.ifi_lastchange	/* [c] last op. state change */
#define	if_capabilities	if_data.ifi_capabilities
#define	if_rdomain	if_data.ifi_rdomain

enum if_counters {
	ifc_ipackets,		/* packets received on interface */
	ifc_ierrors,		/* input errors on interface */
	ifc_opackets,		/* packets sent on interface */
	ifc_oerrors,		/* output errors on interface */
	ifc_collisions,		/* collisions on csma interfaces */
	ifc_ibytes,		/* total number of octets received */
	ifc_obytes,		/* total number of octets sent */
	ifc_imcasts,		/* packets received via multicast */
	ifc_omcasts,		/* packets sent via multicast */
	ifc_iqdrops,		/* dropped on input, this interface */
	ifc_oqdrops,		/* dropped on output, this interface */
	ifc_noproto,		/* destined for unsupported protocol */

	ifc_ncounters
};

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
	unsigned int		 ifma_ifidx;	/* Index of the interface */
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

#define	IFNET_SLOWTIMO	1		/* granularity is 1 second */

/*
 * IFQ compat on ifq API
 */

#define	IFQ_ENQUEUE(ifq, m, err)					\
do {									\
	(err) = ifq_enqueue((ifq), (m));				\
} while (/* CONSTCOND */0)

#define	IFQ_DEQUEUE(ifq, m)						\
do {									\
	(m) = ifq_dequeue(ifq);						\
} while (/* CONSTCOND */0)

#define	IFQ_PURGE(ifq)							\
do {									\
	(void)ifq_purge(ifq);						\
} while (/* CONSTCOND */0)

#define	IFQ_LEN(ifq)			ifq_len(ifq)
#define	IFQ_IS_EMPTY(ifq)		ifq_empty(ifq)
#define	IFQ_SET_MAXLEN(ifq, len)	ifq_set_maxlen(ifq, len)

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
#define niq_len(_q)			mq_len(&(_q)->ni_q)
#define niq_drops(_q)			mq_drops(&(_q)->ni_q)
#define sysctl_niq(_n, _l, _op, _olp, _np, _nl, _niq) \
    sysctl_mq((_n), (_l), (_op), (_olp), (_np), (_nl), &(_niq)->ni_q)

extern struct ifnet_head ifnet;

void	if_start(struct ifnet *);
int	if_enqueue(struct ifnet *, struct mbuf *);
void	if_input(struct ifnet *, struct mbuf_list *);
void	if_input_process(struct ifnet *, struct mbuf_list *);
int	if_input_local(struct ifnet *, struct mbuf *, sa_family_t);
int	if_output_local(struct ifnet *, struct mbuf *, sa_family_t);
void	if_rtrequest_dummy(struct ifnet *, int, struct rtentry *);
void	p2p_rtrequest(struct ifnet *, int, struct rtentry *);

struct	ifaddr *ifa_ifwithaddr(struct sockaddr *, u_int);
struct	ifaddr *ifa_ifwithdstaddr(struct sockaddr *, u_int);
struct	ifaddr *ifaof_ifpforaddr(struct sockaddr *, struct ifnet *);
void	ifafree(struct ifaddr *);

int	if_isconnected(const struct ifnet *, unsigned int);

void	if_clone_attach(struct if_clone *);

int	if_clone_create(const char *, int);
int	if_clone_destroy(const char *);

struct if_clone *
	if_clone_lookup(const char *, int *);

void	ifa_add(struct ifnet *, struct ifaddr *);
void	ifa_del(struct ifnet *, struct ifaddr *);
void	ifa_update_broadaddr(struct ifnet *, struct ifaddr *,
	    struct sockaddr *);

void	if_ih_insert(struct ifnet *, int (*)(struct ifnet *, struct mbuf *,
	    void *), void *);
void	if_ih_remove(struct ifnet *, int (*)(struct ifnet *, struct mbuf *,
	    void *), void *);

void	if_rxr_livelocked(struct if_rxring *);
void	if_rxr_init(struct if_rxring *, u_int, u_int);
u_int	if_rxr_get(struct if_rxring *, u_int);

#define if_rxr_put(_r, _c)	do { (_r)->rxr_alive -= (_c); } while (0)
#define if_rxr_inuse(_r)	((_r)->rxr_alive)
#define if_rxr_cwm(_r)		((_r)->rxr_cwm)

int	if_rxr_info_ioctl(struct if_rxrinfo *, u_int, struct if_rxring_info *);
int	if_rxr_ioctl(struct if_rxrinfo *, const char *, u_int,
	    struct if_rxring *);

void	if_counters_alloc(struct ifnet *);
void	if_counters_free(struct ifnet *);

#endif /* _KERNEL */

#endif /* _NET_IF_VAR_H_ */

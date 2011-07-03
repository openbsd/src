/*	$OpenBSD: if.h,v 1.124 2011/07/03 23:05:35 henning Exp $	*/
/*	$NetBSD: if.h,v 1.23 1996/05/07 02:40:27 thorpej Exp $	*/

/*
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

#ifndef _NET_IF_H_
#define _NET_IF_H_

#include <sys/queue.h>
#include <sys/tree.h>

/*
 * Always include ALTQ glue here -- we use the ALTQ interface queue
 * structure even when ALTQ is not configured into the kernel so that
 * the size of struct ifnet does not changed based on the option.  The
 * ALTQ queue structure is API-compatible with the legacy ifqueue.
 */
#include <altq/if_altq.h>

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
/*  XXX fast fix for SNMP, going away soon */
#include <sys/time.h>

struct mbuf;
struct proc;
struct rtentry;
struct socket;
struct ether_header;
struct arpcom;
struct rt_addrinfo;

/*
 * Structure describing a `cloning' interface.
 */
struct if_clone {
	LIST_ENTRY(if_clone) ifc_list;	/* on list of cloners */
	const char *ifc_name;		/* name of device, e.g. `gif' */
	size_t ifc_namelen;		/* length of name */

	int	(*ifc_create)(struct if_clone *, int);
	int	(*ifc_destroy)(struct ifnet *);
};

#define	IF_CLONE_INITIALIZER(name, create, destroy)			\
	{ { 0 }, name, sizeof(name) - 1, create, destroy }

/*
 * Structure used to query names of interface cloners.
 */
struct if_clonereq {
	int	ifcr_total;		/* total cloners (out) */
	int	ifcr_count;		/* room for this many in user buffer */
	char	*ifcr_buffer;		/* buffer for cloner names */
};

#define MCLPOOLS	7		/* number of cluster pools */

struct mclpool {
	u_int	mcl_grown;
	u_short	mcl_alive;
	u_short mcl_hwm;
	u_short mcl_cwm;
	u_short mcl_lwm;
};

/*
 * Structure defining statistics and other data kept regarding a network
 * interface.
 */
struct	if_data {
	/* generic interface information */
	u_char		ifi_type;		/* ethernet, tokenring, etc. */
	u_char		ifi_addrlen;		/* media address length */
	u_char		ifi_hdrlen;		/* media header length */
	u_char		ifi_link_state;		/* current link state */
	u_int32_t	ifi_mtu;		/* maximum transmission unit */
	u_int32_t	ifi_metric;		/* routing metric (external only) */
	u_int32_t	ifi_pad;
	u_int64_t	ifi_baudrate;		/* linespeed */
	/* volatile statistics */
	u_int64_t	ifi_ipackets;		/* packets received on interface */
	u_int64_t	ifi_ierrors;		/* input errors on interface */
	u_int64_t	ifi_opackets;		/* packets sent on interface */
	u_int64_t	ifi_oerrors;		/* output errors on interface */
	u_int64_t	ifi_collisions;		/* collisions on csma interfaces */
	u_int64_t	ifi_ibytes;		/* total number of octets received */
	u_int64_t	ifi_obytes;		/* total number of octets sent */
	u_int64_t	ifi_imcasts;		/* packets received via multicast */
	u_int64_t	ifi_omcasts;		/* packets sent via multicast */
	u_int64_t	ifi_iqdrops;		/* dropped on input, this interface */
	u_int64_t	ifi_noproto;		/* destined for unsupported protocol */
	struct	timeval ifi_lastchange;	/* last operational state change */

	struct mclpool	ifi_mclpool[MCLPOOLS];
};

/*
 * Structure defining a queue for a network interface.
 * XXX keep in sync with struct ifaltq.
 */
struct	ifqueue {
	struct	mbuf *ifq_head;
	struct	mbuf *ifq_tail;
	int	ifq_len;
	int	ifq_maxlen;
	int	ifq_drops;
	struct	timeout *ifq_congestion;
};

/*
 * Values for if_link_state.
 */
#define LINK_STATE_UNKNOWN	0	/* link unknown */
#define LINK_STATE_INVALID	1	/* link invalid */
#define LINK_STATE_DOWN		2	/* link is down */
#define LINK_STATE_KALIVE_DOWN	3	/* keepalive reports down */
#define LINK_STATE_UP		4	/* link is up */
#define LINK_STATE_HALF_DUPLEX	5	/* link is up and half duplex */
#define LINK_STATE_FULL_DUPLEX	6	/* link is up and full duplex */

#define LINK_STATE_IS_UP(_s)	\
		((_s) >= LINK_STATE_UP || (_s) == LINK_STATE_UNKNOWN)

/*
 * Status bit descriptions for the various interface types.
 */
struct if_status_description {
	u_char	ifs_type;
	u_char	ifs_state;
	const char *ifs_string;
};

#define LINK_STATE_DESC_MATCH(_ifs, _t, _s)				\
	(((_ifs)->ifs_type == (_t) || (_ifs)->ifs_type == 0) &&		\
	    (_ifs)->ifs_state == (_s))
		
	

#define LINK_STATE_DESCRIPTIONS {					\
	{ IFT_ETHER, LINK_STATE_DOWN, "no carrier" },			\
									\
	{ IFT_IEEE80211, LINK_STATE_DOWN, "no network" },		\
									\
	{ IFT_PPP, LINK_STATE_DOWN, "no carrier" },			\
									\
	{ IFT_CARP, LINK_STATE_DOWN, "backup" },			\
	{ IFT_CARP, LINK_STATE_UP, "master" },				\
	{ IFT_CARP, LINK_STATE_HALF_DUPLEX, "master" },			\
	{ IFT_CARP, LINK_STATE_FULL_DUPLEX, "master" },			\
									\
	{ 0, LINK_STATE_UP, "active" },					\
	{ 0, LINK_STATE_HALF_DUPLEX, "active" },			\
	{ 0, LINK_STATE_FULL_DUPLEX, "active" },			\
									\
	{ 0, LINK_STATE_UNKNOWN, "unknown" },				\
	{ 0, LINK_STATE_INVALID, "invalid" },				\
	{ 0, LINK_STATE_DOWN, "down" },					\
	{ 0, LINK_STATE_KALIVE_DOWN, "keepalive down" },		\
	{ 0, 0, NULL }							\
}

/*
 * Structure defining a queue for a network interface.
 *
 * (Would like to call this struct ``if'', but C isn't PL/1.)
 */
TAILQ_HEAD(ifnet_head, ifnet);		/* the actual queue head */

/*
 * Length of interface external name, including terminating '\0'.
 * Note: this is the same size as a generic device's external name.
 */
#define	IFNAMSIZ	16
#define	IF_NAMESIZE	IFNAMSIZ

/*
 * Length of interface description, including terminating '\0'.
 */
#define	IFDESCRSIZE	64

struct ifnet {				/* and the entries */
	void	*if_softc;		/* lower-level data for this if */
	TAILQ_ENTRY(ifnet) if_list;	/* all struct ifnets are chained */
	TAILQ_ENTRY(ifnet) if_txlist;	/* list of ifnets ready to tx */
	TAILQ_HEAD(, ifaddr) if_addrlist; /* linked list of addresses per if */
	TAILQ_HEAD(, ifg_list) if_groups; /* linked list of groups per if */
	struct hook_desc_head *if_addrhooks; /* address change callbacks */
	struct hook_desc_head *if_linkstatehooks; /* link change callbacks */
	struct hook_desc_head *if_detachhooks; /* detach callbacks */
	char	if_xname[IFNAMSIZ];	/* external name (name + unit) */
	int	if_pcount;		/* number of promiscuous listeners */
	caddr_t	if_bpf;			/* packet filter structure */
	caddr_t	if_bridge;		/* bridge structure */
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
	int	if_capabilities;	/* interface capabilities */
	u_int	if_rdomain;		/* routing instance */
	char	if_description[IFDESCRSIZE]; /* interface description */
	u_short	if_rtlabelid;		/* next route label */
	u_int8_t if_priority;

	/* procedure handles */
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
	struct	ifaltq if_snd;		/* output queue (includes altq) */
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

#define	IFF_UP		0x1		/* interface is up */
#define	IFF_BROADCAST	0x2		/* broadcast address valid */
#define	IFF_DEBUG	0x4		/* turn on debugging */
#define	IFF_LOOPBACK	0x8		/* is a loopback net */
#define	IFF_POINTOPOINT	0x10		/* interface is point-to-point link */
#define	IFF_NOTRAILERS	0x20		/* avoid use of trailers */
#define	IFF_RUNNING	0x40		/* resources allocated */
#define	IFF_NOARP	0x80		/* no address resolution protocol */
#define	IFF_PROMISC	0x100		/* receive all packets */
#define	IFF_ALLMULTI	0x200		/* receive all multicast packets */
#define	IFF_OACTIVE	0x400		/* transmission in progress */
#define	IFF_SIMPLEX	0x800		/* can't hear own transmissions */
#define	IFF_LINK0	0x1000		/* per link layer defined bit */
#define	IFF_LINK1	0x2000		/* per link layer defined bit */
#define	IFF_LINK2	0x4000		/* per link layer defined bit */
#define	IFF_MULTICAST	0x8000		/* supports multicast */

/* flags set internally only: */
#define	IFF_CANTCHANGE \
	(IFF_BROADCAST|IFF_POINTOPOINT|IFF_RUNNING|IFF_OACTIVE|\
	    IFF_SIMPLEX|IFF_MULTICAST|IFF_ALLMULTI)

#define IFXF_TXREADY		0x1		/* interface is ready to tx */
#define	IFXF_NOINET6		0x2		/* don't do inet6 */
#define	IFXF_INET6_PRIVACY	0x4		/* autoconf privacy extension */
#define	IFXF_MPLS		0x8		/* supports MPLS */
#define	IFXF_WOL		0x10		/* wake on lan enabled */

#define	IFXF_CANTCHANGE \
	(IFXF_TXREADY)

/*
 * Some convenience macros used for setting ifi_baudrate.
 */
#define	IF_Kbps(x)	((x) * 1000ULL)			/* kilobits/sec. */
#define	IF_Mbps(x)	(IF_Kbps((x) * 1000ULL))	/* megabits/sec. */
#define	IF_Gbps(x)	(IF_Mbps((x) * 1000ULL))	/* gigabits/sec. */

/* Capabilities that interfaces can advertise. */
#define	IFCAP_CSUM_IPv4		0x00000001	/* can do IPv4 header csum */
#define	IFCAP_CSUM_TCPv4	0x00000002	/* can do IPv4/TCP csum */
#define	IFCAP_CSUM_UDPv4	0x00000004	/* can do IPv4/UDP csum */
#define	IFCAP_IPSEC		0x00000008	/* can do IPsec */
#define	IFCAP_VLAN_MTU		0x00000010	/* VLAN-compatible MTU */
#define	IFCAP_VLAN_HWTAGGING	0x00000020	/* hardware VLAN tag support */
#define	IFCAP_IPCOMP		0x00000040	/* can do IPcomp */
#define	IFCAP_CSUM_TCPv6	0x00000080	/* can do IPv6/TCP checksums */
#define	IFCAP_CSUM_UDPv6	0x00000100	/* can do IPv6/UDP checksums */
#define	IFCAP_CSUM_TCPv4_Rx	0x00000200	/* can do IPv4/TCP (Rx only) */
#define	IFCAP_CSUM_UDPv4_Rx	0x00000400	/* can do IPv4/UDP (Rx only) */
#define	IFCAP_WOL		0x00008000	/* can do wake on lan */

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
	(m)->m_nextpkt = 0;						\
	if ((ifq)->ifq_tail == 0)					\
		(ifq)->ifq_head = m;					\
	else								\
		(ifq)->ifq_tail->m_nextpkt = m;				\
	(ifq)->ifq_tail = m;						\
	(ifq)->ifq_len++;						\
} while (/* CONSTCOND */0)
#define	IF_PREPEND(ifq, m)						\
do {									\
	(m)->m_nextpkt = (ifq)->ifq_head;				\
	if ((ifq)->ifq_tail == 0)					\
		(ifq)->ifq_tail = (m);					\
	(ifq)->ifq_head = (m);						\
	(ifq)->ifq_len++;						\
} while (/* CONSTCOND */0)
#define	IF_DEQUEUE(ifq, m)						\
do {									\
	(m) = (ifq)->ifq_head;						\
	if (m) {							\
		if (((ifq)->ifq_head = (m)->m_nextpkt) == 0)		\
			(ifq)->ifq_tail = 0;				\
		(m)->m_nextpkt = 0;					\
		(ifq)->ifq_len--;					\
	}								\
} while (/* CONSTCOND */0)

#define	IF_INPUT_ENQUEUE(ifq, m)					\
do {									\
	if (IF_QFULL(ifq)) {						\
		IF_DROP(ifq);						\
		m_freem(m);						\
		if (!(ifq)->ifq_congestion)				\
			if_congestion(ifq);				\
	} else								\
		IF_ENQUEUE(ifq, m);					\
} while (/* CONSTCOND */0)

#define	IF_POLL(ifq, m)		((m) = (ifq)->ifq_head)
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
#define	IF_IS_EMPTY(ifq)	((ifq)->ifq_len == 0)

#define	IFQ_MAXLEN	256
#define	IFNET_SLOWHZ	1		/* granularity is 1 second */

/* symbolic names for terminal (per-protocol) CTL_IFQ_ nodes */
#define IFQCTL_LEN 1
#define IFQCTL_MAXLEN 2
#define IFQCTL_DROPS 3
#define IFQCTL_CONGESTION 4
#define IFQCTL_MAXID 5

/* sysctl for ifq (per-protocol packet input queue variant of ifqueue) */
#define CTL_IFQ_NAMES  { \
	{ 0, 0 }, \
	{ "len", CTLTYPE_INT }, \
	{ "maxlen", CTLTYPE_INT }, \
	{ "drops", CTLTYPE_INT }, \
	{ "congestion", CTLTYPE_INT }, \
}

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
	void	(*ifa_rtrequest)(int, struct rtentry *, struct rt_addrinfo *);
	u_int	ifa_flags;		/* mostly rt_flags for cloning */
	u_int	ifa_refcnt;		/* count of references */
	int	ifa_metric;		/* cost of going out this interface */
};
#define	IFA_ROUTE	RTF_UP		/* route installed */

struct ifaddr_item {
	RB_ENTRY(ifaddr_item)	 ifai_entry;
	struct sockaddr		*ifai_addr;
	struct ifaddr		*ifai_ifa;
	struct ifaddr_item	*ifai_next;
	u_int			 ifai_rdomain;
};

/*
 * Message format for use in obtaining information about interfaces
 * from sysctl and the routing socket.
 */
struct if_msghdr {
	u_short	ifm_msglen;	/* to skip over non-understood messages */
	u_char	ifm_version;	/* future binary compatibility */
	u_char	ifm_type;	/* message type */
	u_short ifm_hdrlen;	/* sizeof(if_msghdr) to skip over the header */
	u_short	ifm_index;	/* index for associated ifp */
	u_short	ifm_tableid;	/* routing table id */
	u_char	ifm_pad1;
	u_char	ifm_pad2;
	int	ifm_addrs;	/* like rtm_addrs */
	int	ifm_flags;	/* value of if_flags */
	int	ifm_xflags;
	struct	if_data ifm_data;/* statistics and other data about if */
};

/*
 * Message format for use in obtaining information about interface addresses
 * from sysctl and the routing socket.
 */
struct ifa_msghdr {
	u_short	ifam_msglen;	/* to skip over non-understood messages */
	u_char	ifam_version;	/* future binary compatibility */
	u_char	ifam_type;	/* message type */
	u_short ifam_hdrlen;	/* sizeof(ifa_msghdr) to skip over the header */
	u_short	ifam_index;	/* index for associated ifp */
	u_short	ifam_tableid;	/* routing table id */
	u_char	ifam_pad1;
	u_char	ifam_pad2;
	int	ifam_addrs;	/* like rtm_addrs */
	int	ifam_flags;	/* value of ifa_flags */
	int	ifam_metric;	/* value of ifa_metric */
};


/*
 * Message format announcing the arrival or departure of a network interface.
 */
struct if_announcemsghdr {
	u_short	ifan_msglen;	/* to skip over non-understood messages */
	u_char	ifan_version;	/* future binary compatibility */
	u_char	ifan_type;	/* message type */
	u_short ifan_hdrlen;	/* sizeof(ifa_msghdr) to skip over the header */
	u_short	ifan_index;	/* index for associated ifp */
	u_short	ifan_what;	/* what type of announcement */
	char	ifan_name[IFNAMSIZ];	/* if name, e.g. "en0" */
};

#define IFAN_ARRIVAL	0	/* interface arrival */
#define IFAN_DEPARTURE	1	/* interface departure */

/*
 * interface groups
 */

#define	IFG_ALL		"all"		/* group contains all interfaces */
#define	IFG_EGRESS	"egress"	/* if(s) default route(s) point to */

struct ifg_group {
	char				 ifg_group[IFNAMSIZ];
	u_int				 ifg_refcnt;
	caddr_t				 ifg_pf_kif;
	int				 ifg_carp_demoted;
	TAILQ_HEAD(, ifg_member)	 ifg_members;
	TAILQ_ENTRY(ifg_group)		 ifg_next;
};

struct ifg_member {
	TAILQ_ENTRY(ifg_member)	 ifgm_next;
	struct ifnet		*ifgm_ifp;
};

struct ifg_list {
	struct ifg_group	*ifgl_group;
	TAILQ_ENTRY(ifg_list)	 ifgl_next;
};

struct ifg_req {
	union {
		char			 ifgrqu_group[IFNAMSIZ];
		char			 ifgrqu_member[IFNAMSIZ];
	} ifgrq_ifgrqu;
#define	ifgrq_group	ifgrq_ifgrqu.ifgrqu_group
#define	ifgrq_member	ifgrq_ifgrqu.ifgrqu_member
};

struct ifg_attrib {
	int	ifg_carp_demoted;
};

/*
 * Used to lookup groups for an interface
 */
struct ifgroupreq {
	char	ifgr_name[IFNAMSIZ];
	u_int	ifgr_len;
	union {
		char			 ifgru_group[IFNAMSIZ];
		struct	ifg_req		*ifgru_groups;
		struct	ifg_attrib	 ifgru_attrib;
	} ifgr_ifgru;
#define ifgr_group	ifgr_ifgru.ifgru_group
#define ifgr_groups	ifgr_ifgru.ifgru_groups
#define ifgr_attrib	ifgr_ifgru.ifgru_attrib
};

/*
 * Interface request structure used for socket
 * ioctl's.  All interface ioctl's must have parameter
 * definitions which begin with ifr_name.  The
 * remainder may be interface specific.
 */
struct	ifreq {
	char	ifr_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	union {
		struct	sockaddr ifru_addr;
		struct	sockaddr ifru_dstaddr;
		struct	sockaddr ifru_broadaddr;
		short	ifru_flags;
		int	ifru_metric;
		caddr_t	ifru_data;
	} ifr_ifru;
#define	ifr_addr	ifr_ifru.ifru_addr	/* address */
#define	ifr_dstaddr	ifr_ifru.ifru_dstaddr	/* other end of p-to-p link */
#define	ifr_broadaddr	ifr_ifru.ifru_broadaddr	/* broadcast address */
#define	ifr_flags	ifr_ifru.ifru_flags	/* flags */
#define	ifr_metric	ifr_ifru.ifru_metric	/* metric */
#define	ifr_mtu		ifr_ifru.ifru_metric	/* mtu (overload) */
#define	ifr_media	ifr_ifru.ifru_metric	/* media options (overload) */
#define	ifr_rdomainid	ifr_ifru.ifru_metric	/* VRF instance (overload) */
#define	ifr_data	ifr_ifru.ifru_data	/* for use by interface */
};

struct ifaliasreq {
	char	ifra_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	struct	sockaddr ifra_addr;
	struct	sockaddr ifra_dstaddr;
#define	ifra_broadaddr	ifra_dstaddr
	struct	sockaddr ifra_mask;
};

struct ifmediareq {
	char	ifm_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	int	ifm_current;			/* current media options */
	int	ifm_mask;			/* don't care mask */
	int	ifm_status;			/* media status */
	int	ifm_active;			/* active options */ 
	int	ifm_count;			/* # entries in ifm_ulist
							array */
	int	*ifm_ulist;			/* media words */
};

struct ifkalivereq {
	char	ikar_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	int	ikar_timeo;
	int	ikar_cnt;
};

/*
 * Structure used in SIOCGIFCONF request.
 * Used to retrieve interface configuration
 * for machine (useful for programs which
 * must know all networks accessible).
 */
struct	ifconf {
	int	ifc_len;		/* size of associated buffer */
	union {
		caddr_t	ifcu_buf;
		struct	ifreq *ifcu_req;
	} ifc_ifcu;
#define	ifc_buf	ifc_ifcu.ifcu_buf	/* buffer address */
#define	ifc_req	ifc_ifcu.ifcu_req	/* array of structures returned */
};

/*
 * Structure for SIOC[AGD]LIFADDR
 */
struct if_laddrreq {
	char iflr_name[IFNAMSIZ];
	unsigned int flags;
#define IFLR_PREFIX	0x8000	/* in: prefix given  out: kernel fills id */
	unsigned int prefixlen;		/* in/out */
	struct sockaddr_storage addr;	/* in/out */
	struct sockaddr_storage dstaddr; /* out */
};

struct if_nameindex {
	unsigned int	if_index;
	char		*if_name;
};

#ifndef _KERNEL
__BEGIN_DECLS
unsigned int if_nametoindex(const char *);
char	*if_indextoname(unsigned int, char *);
struct	if_nameindex *if_nameindex(void);
void	if_freenameindex(struct if_nameindex *);
__END_DECLS
#endif

#include <net/if_arp.h>

#ifdef _KERNEL
#define	IFAFREE(ifa) \
do { \
	if ((ifa)->ifa_refcnt <= 0) \
		ifafree(ifa); \
	else \
		(ifa)->ifa_refcnt--; \
} while (/* CONSTCOND */0)

#ifdef ALTQ

#define	IFQ_ENQUEUE(ifq, m, pattr, err)					\
do {									\
	if (ALTQ_IS_ENABLED((ifq)))					\
		ALTQ_ENQUEUE((ifq), (m), (pattr), (err));		\
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
	if (TBR_IS_ENABLED((ifq)))					\
		(m) = tbr_dequeue((ifq), ALTDQ_REMOVE);			\
	else if (ALTQ_IS_ENABLED((ifq)))				\
		ALTQ_DEQUEUE((ifq), (m));				\
	else								\
		IF_DEQUEUE((ifq), (m));					\
} while (/* CONSTCOND */0)

#define	IFQ_POLL(ifq, m)						\
do {									\
	if (TBR_IS_ENABLED((ifq)))					\
		(m) = tbr_dequeue((ifq), ALTDQ_POLL);			\
	else if (ALTQ_IS_ENABLED((ifq)))				\
		ALTQ_POLL((ifq), (m));					\
	else								\
		IF_POLL((ifq), (m));					\
} while (/* CONSTCOND */0)

#define	IFQ_PURGE(ifq)							\
do {									\
	if (ALTQ_IS_ENABLED((ifq)))					\
		ALTQ_PURGE((ifq));					\
	else								\
		IF_PURGE((ifq));					\
} while (/* CONSTCOND */0)

#define	IFQ_SET_READY(ifq)						\
do {									\
	((ifq)->altq_flags |= ALTQF_READY);				\
} while (/* CONSTCOND */0)

#define	IFQ_CLASSIFY(ifq, m, af, pa)					\
do {									\
	if (ALTQ_IS_ENABLED((ifq))) {					\
		if (ALTQ_NEEDS_CLASSIFY((ifq)))				\
			(pa)->pattr_class = (*(ifq)->altq_classify)	\
				((ifq)->altq_clfier, (m), (af));	\
		(pa)->pattr_af = (af);					\
		(pa)->pattr_hdr = mtod((m), caddr_t);			\
	}								\
} while (/* CONSTCOND */0)

#else /* !ALTQ */

#define	IFQ_ENQUEUE(ifq, m, pattr, err)					\
do {									\
	if (IF_QFULL((ifq))) {						\
		m_freem((m));						\
		(err) = ENOBUFS;					\
	} else {							\
		IF_ENQUEUE((ifq), (m));					\
		(err) = 0;						\
	}								\
	if ((err))							\
		(ifq)->ifq_drops++;					\
} while (/* CONSTCOND */0)

#define	IFQ_DEQUEUE(ifq, m)	IF_DEQUEUE((ifq), (m))

#define	IFQ_POLL(ifq, m)	IF_POLL((ifq), (m))

#define	IFQ_PURGE(ifq)		IF_PURGE((ifq))

#define	IFQ_SET_READY(ifq)	/* nothing */

#define	IFQ_CLASSIFY(ifq, m, af, pa) /* nothing */

#endif /* ALTQ */

#define	IFQ_IS_EMPTY(ifq)		((ifq)->ifq_len == 0)
#define	IFQ_INC_LEN(ifq)		((ifq)->ifq_len++)
#define	IFQ_DEC_LEN(ifq)		(--(ifq)->ifq_len)
#define	IFQ_INC_DROPS(ifq)		((ifq)->ifq_drops++)
#define	IFQ_SET_MAXLEN(ifq, len)	((ifq)->ifq_maxlen = (len))

/* default interface priorities */
#define IF_WIRED_DEFAULT_PRIORITY 0
#define IF_WIRELESS_DEFAULT_PRIORITY 4

extern int ifqmaxlen;
extern struct ifnet_head ifnet;
extern struct ifnet **ifindex2ifnet;
extern struct ifnet *lo0ifp;
extern int if_indexlim;

#define	ether_input_mbuf(ifp, m)        ether_input((ifp), NULL, (m))

void	ether_ifattach(struct ifnet *);
void	ether_ifdetach(struct ifnet *);
int	ether_ioctl(struct ifnet *, struct arpcom *, u_long, caddr_t);
void	ether_input(struct ifnet *, struct ether_header *, struct mbuf *);
int	ether_output(struct ifnet *,
	    struct mbuf *, struct sockaddr *, struct rtentry *);
char	*ether_sprintf(u_char *);

void	if_alloc_sadl(struct ifnet *);
void	if_free_sadl(struct ifnet *);
void	if_attach(struct ifnet *);
void	if_attachdomain(void);
void	if_attachtail(struct ifnet *);
void	if_attachhead(struct ifnet *);
void	if_detach(struct ifnet *);
void	if_down(struct ifnet *);
void	if_downall(void);
void	if_link_state_change(struct ifnet *);
void	if_qflush(struct ifqueue *);
void	if_slowtimo(void *);
void	if_up(struct ifnet *);
int	ifconf(u_long, caddr_t);
void	ifinit(void);
int	ifioctl(struct socket *, u_long, caddr_t, struct proc *);
int	ifpromisc(struct ifnet *, int);
struct	ifg_group *if_creategroup(const char *);
int	if_addgroup(struct ifnet *, const char *);
int	if_delgroup(struct ifnet *, const char *);
void	if_group_routechange(struct sockaddr *, struct sockaddr *);
struct	ifnet *ifunit(const char *);
void	if_start(struct ifnet *);
void	ifnewlladdr(struct ifnet *);

struct	ifaddr *ifa_ifwithaddr(struct sockaddr *, u_int);
struct	ifaddr *ifa_ifwithaf(int, u_int);
struct	ifaddr *ifa_ifwithdstaddr(struct sockaddr *, u_int);
struct	ifaddr *ifa_ifwithnet(struct sockaddr *, u_int);
struct	ifaddr *ifa_ifwithroute(int, struct sockaddr *,
					struct sockaddr *, u_int);
struct	ifaddr *ifaof_ifpforaddr(struct sockaddr *, struct ifnet *);
void	ifafree(struct ifaddr *);
void	link_rtrequest(int, struct rtentry *, struct rt_addrinfo *);

void	if_clone_attach(struct if_clone *);
void	if_clone_detach(struct if_clone *);

int	if_clone_create(const char *);
int	if_clone_destroy(const char *);

void	if_congestion(struct ifqueue *);
int     sysctl_ifq(int *, u_int, void *, size_t *, void *, size_t,
	    struct ifqueue *);

int	loioctl(struct ifnet *, u_long, caddr_t);
void	loopattach(int);
int	looutput(struct ifnet *,
	    struct mbuf *, struct sockaddr *, struct rtentry *);
void	lortrequest(int, struct rtentry *, struct rt_addrinfo *);
void	ifa_add(struct ifnet *, struct ifaddr *);
void	ifa_del(struct ifnet *, struct ifaddr *);
void	ifa_update_broadaddr(struct ifnet *, struct ifaddr *,
	    struct sockaddr *);
#endif /* _KERNEL */
#endif /* _NET_IF_H_ */

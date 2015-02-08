/*	$OpenBSD: if.h,v 1.160 2015/02/08 06:00:52 mpi Exp $	*/
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

#include <sys/cdefs.h>

/*
 * Length of interface external name, including terminating '\0'.
 * Note: this is the same size as a generic device's external name.
 */
#define	IF_NAMESIZE	16

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

#if __BSD_VISIBLE

/*
 * Structure used to query names of interface cloners.
 */
struct if_clonereq {
	int	ifcr_total;		/* total cloners (out) */
	int	ifcr_count;		/* room for this many in user buffer */
	char	*ifcr_buffer;		/* buffer for cloner names */
};

#define MCLPOOLS	7		/* number of cluster pools */

struct if_rxring {
	int     rxr_adjusted;
	u_int	rxr_alive;
	u_int	rxr_cwm;
	u_int	rxr_lwm;
	u_int	rxr_hwm;
};

struct if_rxring_info {
	char	ifr_name[16];		/* name of the ring */
	u_int	ifr_size;		/* size of the packets on the ring */
	struct if_rxring ifr_info;
};

/* Structure used in SIOCGIFRXR request. */
struct if_rxrinfo {
	u_int	ifri_total;
	struct if_rxring_info *ifri_entries;
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
	u_int32_t	ifi_capabilities;	/* interface capabilities */
	struct	timeval ifi_lastchange;	/* last operational state change */
};

#define IFQ_NQUEUES	8
#define IFQ_MAXPRIO	IFQ_NQUEUES - 1
#define IFQ_DEFPRIO	3

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

/* Traditional BSD name for length of interface external name. */
#define	IFNAMSIZ	IF_NAMESIZE

/*
 * Length of interface description, including terminating '\0'.
 */
#define	IFDESCRSIZE	64

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
#define	IFXF_INET6_NOPRIVACY	0x4		/* don't autoconf privacy */
#define	IFXF_MPLS		0x8		/* supports MPLS */
#define	IFXF_WOL		0x10		/* wake on lan enabled */
#define	IFXF_AUTOCONF6		0x20		/* v6 autoconf enabled */

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
#define	IFCAP_VLAN_MTU		0x00000010	/* VLAN-compatible MTU */
#define	IFCAP_VLAN_HWTAGGING	0x00000020	/* hardware VLAN tag support */
#define	IFCAP_CSUM_TCPv6	0x00000080	/* can do IPv6/TCP checksums */
#define	IFCAP_CSUM_UDPv6	0x00000100	/* can do IPv6/UDP checksums */
#define	IFCAP_WOL		0x00008000	/* can do wake on lan */

#define IFCAP_CSUM_MASK		(IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 | \
    IFCAP_CSUM_UDPv4 | IFCAP_CSUM_TCPv6 | IFCAP_CSUM_UDPv6)

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
#define	ifr_hardmtu	ifr_ifru.ifru_metric	/* hardmtu (overload) */
#define	ifr_media	ifr_ifru.ifru_metric	/* media options (overload) */
#define	ifr_rdomainid	ifr_ifru.ifru_metric	/* VRF instance (overload) */
#define ifr_vnetid	ifr_ifru.ifru_metric	/* Virtual Net Id (overload) */
#define ifr_ttl		ifr_ifru.ifru_metric	/* tunnel TTL (overload) */
#define	ifr_data	ifr_ifru.ifru_data	/* for use by interface */
};

struct ifaliasreq {
	char	ifra_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	union {
		struct	sockaddr ifrau_addr;
		int	ifrau_align;
	 } ifra_ifrau;
#ifndef ifra_addr
#define ifra_addr	ifra_ifrau.ifrau_addr
#endif
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

/* SIOCIFAFDETACH */
struct if_afreq {
	char		ifar_name[IFNAMSIZ];
	sa_family_t	ifar_af;
};

#include <net/if_arp.h>

#ifdef _KERNEL
struct socket;
struct ifnet;

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
struct	ifnet *if_get(unsigned int);
void	ifnewlladdr(struct ifnet *);

#endif /* _KERNEL */

#endif /* __BSD_VISIBLE */

#endif /* _NET_IF_H_ */

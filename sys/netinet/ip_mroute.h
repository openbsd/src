/*	$OpenBSD: ip_mroute.h,v 1.11 2004/11/24 01:25:42 mcbride Exp $	*/
/*	$NetBSD: ip_mroute.h,v 1.23 2004/04/21 17:49:46 itojun Exp $	*/

#ifndef _NETINET_IP_MROUTE_H_
#define _NETINET_IP_MROUTE_H_

/*
 * Definitions for IP multicast forwarding.
 *
 * Written by David Waitzman, BBN Labs, August 1988.
 * Modified by Steve Deering, Stanford, February 1989.
 * Modified by Ajit Thyagarajan, PARC, August 1993.
 * Modified by Ajit Thyagarajan, PARC, August 1994.
 *
 * MROUTING Revision: 1.2
 */

#include <sys/queue.h>
#include <sys/timeout.h>

/*
 * Multicast Routing set/getsockopt commands.
 */
#define	MRT_INIT		100	/* initialize forwarder */
#define	MRT_DONE		101	/* shut down forwarder */
#define	MRT_ADD_VIF		102	/* create virtual interface */
#define	MRT_DEL_VIF		103	/* delete virtual interface */
#define	MRT_ADD_MFC		104	/* insert forwarding cache entry */
#define	MRT_DEL_MFC		105	/* delete forwarding cache entry */
#define	MRT_VERSION		106	/* get kernel version number */
#define	MRT_ASSERT		107	/* enable assert processing */


/*
 * Types and macros for handling bitmaps with one bit per virtual interface.
 */
#define	MAXVIFS 32
typedef u_int32_t vifbitmap_t;
typedef u_int16_t vifi_t;		/* type of a vif index */

#define	VIFM_SET(n, m)			((m) |= (1 << (n)))
#define	VIFM_CLR(n, m)			((m) &= ~(1 << (n)))
#define	VIFM_ISSET(n, m)		((m) & (1 << (n)))
#define	VIFM_SETALL(m)			((m) = 0xffffffff)
#define	VIFM_CLRALL(m)			((m) = 0x00000000)
#define	VIFM_COPY(mfrom, mto)		((mto) = (mfrom))
#define	VIFM_SAME(m1, m2)		((m1) == (m2))

#define	VIFF_TUNNEL	0x1		/* vif represents a tunnel end-point */
#define	VIFF_SRCRT	0x2		/* tunnel uses IP src routing */

/*
 * Argument structure for MRT_ADD_VIF.
 * (MRT_DEL_VIF takes a single vifi_t argument.)
 */
struct vifctl {
	vifi_t	  vifc_vifi;	    	/* the index of the vif to be added */
	u_int8_t  vifc_flags;     	/* VIFF_ flags defined below */
	u_int8_t  vifc_threshold; 	/* min ttl required to forward on vif */
	u_int32_t vifc_rate_limit;	/* max rate */
	struct	  in_addr vifc_lcl_addr;/* local interface address */
	struct	  in_addr vifc_rmt_addr;/* remote address (tunnels only) */
};

/*
 * Argument structure for MRT_ADD_MFC and MRT_DEL_MFC.
 */
struct mfcctl {
	struct	 in_addr mfcc_origin;	/* ip origin of mcasts */
	struct	 in_addr mfcc_mcastgrp;	/* multicast group associated */
	vifi_t	 mfcc_parent;		/* incoming vif */
	u_int8_t mfcc_ttls[MAXVIFS];	/* forwarding ttls on vifs */
};

/*
 * Argument structure used by mrouted to get src-grp pkt counts.
 */
struct sioc_sg_req {
	struct	in_addr src;
	struct	in_addr grp;
	u_long	pktcnt;
	u_long	bytecnt;
	u_long	wrong_if;
};

/*
 * Argument structure used by mrouted to get vif pkt counts.
 */
struct sioc_vif_req {
	vifi_t	vifi;			/* vif number */
	u_long	icount;			/* input packet count on vif */
	u_long	ocount;			/* output packet count on vif */
	u_long	ibytes;			/* input byte count on vif */
	u_long	obytes;			/* output byte count on vif */
};


/*
 * The kernel's multicast routing statistics.
 */
struct mrtstat {
	u_long	mrts_mfc_lookups;	/* # forw. cache hash table hits */
	u_long	mrts_mfc_misses;	/* # forw. cache hash table misses */
	u_long	mrts_upcalls;		/* # calls to mrouted */
	u_long	mrts_no_route;		/* no route for packet's origin */
	u_long	mrts_bad_tunnel;	/* malformed tunnel options */
	u_long	mrts_cant_tunnel;	/* no room for tunnel options */
	u_long	mrts_wrong_if;		/* arrived on wrong interface */
	u_long	mrts_upq_ovflw;		/* upcall Q overflow */
	u_long	mrts_cache_cleanups;	/* # entries with no upcalls */
	u_long	mrts_drop_sel;     	/* pkts dropped selectively */
	u_long	mrts_q_overflow;    	/* pkts dropped - Q overflow */
	u_long	mrts_pkt2large;     	/* pkts dropped - size > BKT SIZE */
	u_long	mrts_upq_sockfull;	/* upcalls dropped - socket full */
};


#ifdef _KERNEL

/*
 * The kernel's virtual-interface structure.
 */
struct vif {
	struct	  mbuf *tbf_q, **tbf_t;	/* packet queue */
	struct	  timeval tbf_last_pkt_t; /* arr. time of last pkt */
	u_int32_t tbf_n_tok;		/* no of tokens in bucket */
	u_int32_t tbf_q_len;		/* length of queue at this vif */
	u_int32_t tbf_max_q_len;	/* max. queue length */

	u_int8_t  v_flags;		/* VIFF_ flags defined above */
	u_int8_t  v_threshold;		/* min ttl required to forward on vif */
	u_int32_t v_rate_limit;		/* max rate */
	struct	  in_addr v_lcl_addr;	/* local interface address */
	struct	  in_addr v_rmt_addr;	/* remote address (tunnels only) */
	struct	  ifnet *v_ifp;		/* pointer to interface */
	u_long	  v_pkt_in;		/* # pkts in on interface */
	u_long	  v_pkt_out;		/* # pkts out on interface */
	u_long	  v_bytes_in;		/* # bytes in on interface */
	u_long	  v_bytes_out;		/* # bytes out on interface */
	struct	  route v_route;	/* cached route if this is a tunnel */
	struct	  timeout v_repq_ch;	/* for tbf_reprocess_q() */
#ifdef RSVP_ISI
	int	  v_rsvp_on;		/* # RSVP listening on this vif */
	struct	  socket *v_rsvpd;	/* # RSVPD daemon */
#endif /* RSVP_ISI */
};

/*
 * The kernel's multicast forwarding cache entry structure.
 * (A field for the type of service (mfc_tos) is to be added
 * at a future point.)
 */
struct mfc {
	LIST_ENTRY(mfc) mfc_hash;
	struct	 in_addr mfc_origin;	 	/* ip origin of mcasts */
	struct	 in_addr mfc_mcastgrp;  	/* multicast group associated */
	vifi_t	 mfc_parent;			/* incoming vif */
	u_int8_t mfc_ttls[MAXVIFS]; 		/* forwarding ttls on vifs */
	u_long	 mfc_pkt_cnt;			/* pkt count for src-grp */
	u_long	 mfc_byte_cnt;			/* byte count for src-grp */
	u_long	 mfc_wrong_if;			/* wrong if for src-grp	*/
	int	 mfc_expire;			/* time to clean entry up */
	struct	 timeval mfc_last_assert;	/* last time I sent an assert */
	struct	 rtdetq *mfc_stall;		/* pkts waiting for route */
};

/*
 * Structure used to communicate from kernel to multicast router.
 * (Note the convenient similarity to an IP packet.)
 */
struct igmpmsg {
	u_int32_t unused1;
	u_int32_t unused2;
	u_int8_t  im_msgtype;		/* what type of message */
#define IGMPMSG_NOCACHE		1	/* no MFC in the kernel		    */
#define IGMPMSG_WRONGVIF	2	/* packet came from wrong interface */
	u_int8_t  im_mbz;		/* must be zero */
	u_int8_t  im_vif;		/* vif rec'd on */
	u_int8_t  unused3;
	struct	  in_addr im_src, im_dst;
};

/*
 * Argument structure used for pkt info. while upcall is made.
 */
struct rtdetq {
	struct	mbuf *m;		/* a copy of the packet */
	struct	ifnet *ifp;		/* interface pkt came in on */
#ifdef UPCALL_TIMING
	struct	timeval t;		/* timestamp */
#endif /* UPCALL_TIMING */
	struct	rtdetq *next;
};

#define	MFCTBLSIZ	256
#define	MAX_UPQ		4		/* max. no of pkts in upcall Q */

/*
 * Token bucket filter code
 */
#define	MAX_BKT_SIZE    10000		/* 10K bytes size */
#define	MAXQSIZE        10		/* max. no of pkts in token queue */

int	ip_mrouter_set(struct socket *, int, struct mbuf **);
int	ip_mrouter_get(struct socket *, int, struct mbuf **);
int	mrt_ioctl(struct socket *, u_long, caddr_t);
int	ip_mrouter_done(void);
void	ip_mrouter_detach(struct ifnet *);
void	reset_vif(struct vif *);
void	vif_delete(struct ifnet *);
#ifdef RSVP_ISI
int	ip_mforward(struct mbuf *, struct ifnet *, struct ip_moptions *);
int	legal_vif_num(int);
int	ip_rsvp_vif_init(struct socket *, struct mbuf *);
int	ip_rsvp_vif_done(struct socket *, struct mbuf *);
void	ip_rsvp_force_done(struct socket *);
void	rsvp_input(struct mbuf *, int, int);
#else
int	ip_mforward(struct mbuf *, struct ifnet *);
#endif /* RSVP_ISI */

void	ipip_mroute_input(struct mbuf *, ...);

#endif /* _KERNEL */
#endif /* _NETINET_IP_MROUTE_H_ */

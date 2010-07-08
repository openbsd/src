/*	$OpenBSD: ip6_var.h,v 1.37 2010/07/08 19:42:46 jsg Exp $	*/
/*	$KAME: ip6_var.h,v 1.33 2000/06/11 14:59:20 jinmei Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)ip_var.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NETINET6_IP6_VAR_H_
#define _NETINET6_IP6_VAR_H_

/*
 * IP6 reassembly queue structure.  Each fragment
 * being reassembled is attached to one of these structures.
 */
struct	ip6q {
	u_int32_t	ip6q_head;
	u_int16_t	ip6q_len;
	u_int8_t	ip6q_nxt;	/* ip6f_nxt in first fragment */
	u_int8_t	ip6q_hlim;
	struct ip6asfrag *ip6q_down;
	struct ip6asfrag *ip6q_up;
	u_int32_t	ip6q_ident;
	u_int8_t	ip6q_arrive;
	u_int8_t	ip6q_ttl;
	struct in6_addr	ip6q_src, ip6q_dst;
	struct ip6q	*ip6q_next;
	struct ip6q	*ip6q_prev;
	int		ip6q_unfrglen;	/* len of unfragmentable part */
#ifdef notyet
	u_char		*ip6q_nxtp;
#endif
	int		ip6q_nfrag;	/* # of fragments */
};

struct	ip6asfrag {
	u_int32_t	ip6af_head;
	u_int16_t	ip6af_len;
	u_int8_t	ip6af_nxt;
	u_int8_t	ip6af_hlim;
	/* must not override the above members during reassembling */
	struct ip6asfrag *ip6af_down;
	struct ip6asfrag *ip6af_up;
	struct mbuf	*ip6af_m;
	int		ip6af_offset;	/* offset in ip6af_m to next header */
	int		ip6af_frglen;	/* fragmentable part length */
	int		ip6af_off;	/* fragment offset */
	u_int16_t	ip6af_mff;	/* more fragment bit in frag off */
};

#define IP6_REASS_MBUF(ip6af) ((ip6af)->ip6af_m)

struct	ip6_moptions {
	struct	ifnet *im6o_multicast_ifp; /* ifp for outgoing multicasts */
	u_char	im6o_multicast_hlim;	/* hoplimit for outgoing multicasts */
	u_char	im6o_multicast_loop;	/* 1 >= hear sends if a member */
	LIST_HEAD(, in6_multi_mship) im6o_memberships;
};

/*
 * Control options for outgoing packets
 */

/* Routing header related info */
struct	ip6po_rhinfo {
	struct	ip6_rthdr *ip6po_rhi_rthdr; /* Routing header */
	struct	route_in6 ip6po_rhi_route; /* Route to the 1st hop */
};
#define ip6po_rthdr	ip6po_rhinfo.ip6po_rhi_rthdr
#define ip6po_route	ip6po_rhinfo.ip6po_rhi_route

/* Nexthop related info */
struct	ip6po_nhinfo {
	struct sockaddr *ip6po_nhi_nexthop;
	struct route_in6 ip6po_nhi_route;
};
#define ip6po_nexthop	ip6po_nhinfo.ip6po_nhi_nexthop
#define ip6po_nextroute	ip6po_nhinfo.ip6po_nhi_route

struct	ip6_pktopts {
	/* Hoplimit for outgoing packets */
	int	ip6po_hlim;

	/* Outgoing IF/address information */
	struct in6_pktinfo *ip6po_pktinfo;

	/* Next-hop address information */
	struct	ip6po_nhinfo ip6po_nhinfo;

	/* Hop-by-Hop options header */
	struct	ip6_hbh *ip6po_hbh;

	/* Destination options header (before a routing header) */
	struct	ip6_dest *ip6po_dest1;

	/* Routing header related info. */
	struct	ip6po_rhinfo ip6po_rhinfo;

	/* Destination options header (after a routing header) */
	struct	ip6_dest *ip6po_dest2;

	/* traffic class */
	int	ip6po_tclass;

	/* fragment vs PMTU discovery policy */
	int	ip6po_minmtu;
#define IP6PO_MINMTU_MCASTONLY	-1 /* default: send at min MTU for multicast */
#define IP6PO_MINMTU_DISABLE	0  /* always perform pmtu disc */
#define IP6PO_MINMTU_ALL	1  /* always send at min MTU */

	int	ip6po_flags;
#define	IP6PO_DONTFRAG	0x04	/* disable fragmentation (IPV6_DONTFRAG) */
};

struct	ip6stat {
	u_int64_t ip6s_total;		/* total packets received */
	u_int64_t ip6s_tooshort;	/* packet too short */
	u_int64_t ip6s_toosmall;	/* not enough data */
	u_int64_t ip6s_fragments;	/* fragments received */
	u_int64_t ip6s_fragdropped;	/* frags dropped(dups, out of space) */
	u_int64_t ip6s_fragtimeout;	/* fragments timed out */
	u_int64_t ip6s_fragoverflow;	/* fragments that exceeded limit */
	u_int64_t ip6s_forward;		/* packets forwarded */
	u_int64_t ip6s_cantforward;	/* packets rcvd for unreachable dest */
	u_int64_t ip6s_redirectsent;	/* packets forwarded on same net */
	u_int64_t ip6s_delivered;	/* datagrams delivered to upper level*/
	u_int64_t ip6s_localout;	/* total ip packets generated here */
	u_int64_t ip6s_odropped;	/* lost packets due to nobufs, etc. */
	u_int64_t ip6s_reassembled;	/* total packets reassembled ok */
	u_int64_t ip6s_fragmented;	/* datagrams successfully fragmented */
	u_int64_t ip6s_ofragments;	/* output fragments created */
	u_int64_t ip6s_cantfrag;	/* don't fragment flag was set, etc. */
	u_int64_t ip6s_badoptions;	/* error in option processing */
	u_int64_t ip6s_noroute;		/* packets discarded due to no route */
	u_int64_t ip6s_badvers;		/* ip6 version != 6 */
	u_int64_t ip6s_rawout;		/* total raw ip packets generated */
	u_int64_t ip6s_badscope;	/* scope error */
	u_int64_t ip6s_notmember;	/* don't join this multicast group */
	u_int64_t ip6s_nxthist[256];	/* next header history */
	u_int64_t ip6s_m1;		/* one mbuf */
	u_int64_t ip6s_m2m[32];		/* two or more mbuf */
	u_int64_t ip6s_mext1;		/* one ext mbuf */
	u_int64_t ip6s_mext2m;		/* two or more ext mbuf */
	u_int64_t ip6s_exthdrtoolong;	/* ext hdr are not continuous */
	u_int64_t ip6s_nogif;		/* no match gif found */
	u_int64_t ip6s_toomanyhdr;	/* discarded due to too many headers */

	/*
	 * statistics for improvement of the source address selection
	 * algorithm:
	 * XXX: hardcoded 16 = # of ip6 multicast scope types + 1
	 */
	/* number of times that address selection fails */
	u_int64_t ip6s_sources_none;
	/* number of times that an address on the outgoing I/F is chosen */
	u_int64_t ip6s_sources_sameif[16];
	/* number of times that an address on a non-outgoing I/F is chosen */
	u_int64_t ip6s_sources_otherif[16];
	/*
	 * number of times that an address that has the same scope
	 * from the destination is chosen.
	 */
	u_int64_t ip6s_sources_samescope[16];
	/*
	 * number of times that an address that has a different scope
	 * from the destination is chosen.
	 */
	u_int64_t ip6s_sources_otherscope[16];
	/* number of times that an deprecated address is chosen */
	u_int64_t ip6s_sources_deprecated[16];

	u_int64_t ip6s_forward_cachehit;
	u_int64_t ip6s_forward_cachemiss;
};

#ifdef _KERNEL
/* flags passed to ip6_output as last parameter */
#define	IPV6_UNSPECSRC		0x01	/* allow :: as the source address */
#define	IPV6_FORWARDING		0x02	/* most of IPv6 header exists */
#define	IPV6_MINMTU		0x04	/* use minimum MTU (IPV6_USE_MIN_MTU) */

extern struct	ip6stat ip6stat;	/* statistics */
extern int	ip6_defhlim;		/* default hop limit */
extern int	ip6_defmcasthlim;	/* default multicast hop limit */
extern int	ip6_forwarding;		/* act as router? */
extern int	ip6_mforwarding;	/* act as multicast router? */
extern int	ip6_multipath;		/* use multipath routes */
extern int	ip6_sendredirect;	/* send ICMPv6 redirect? */
extern int	ip6_use_deprecated;	/* allow deprecated addr as source */
extern int	ip6_rr_prune;		/* router renumbering prefix
					 * walk list every 5 sec.    */
extern int	ip6_mcast_pmtu;		/* path MTU discovery for multicast */
extern const int	ip6_v6only;
extern int	ip6_neighborgcthresh; /* Threshold # of NDP entries for GC */
extern int	ip6_maxifprefixes; /* Max acceptable prefixes via RA per IF */
extern int	ip6_maxifdefrouters; /* Max acceptable def routers via RA */
extern int	ip6_maxdynroutes; /* Max # of routes created via redirect */

extern struct socket *ip6_mrouter; 	/* multicast routing daemon */
extern int	ip6_sendredirects;	/* send IP redirects when forwarding? */
extern int	ip6_maxfragpackets; /* Maximum packets in reassembly queue */
extern int	ip6_maxfrags;	/* Maximum fragments in reassembly queue */
extern int	ip6_accept_rtadv;	/* Acts as a host not a router */
extern int	ip6_keepfaith;		/* Firewall Aided Internet Translator */
extern int	ip6_log_interval;
extern time_t	ip6_log_time;
extern int	ip6_hdrnestlimit; /* upper limit of # of extension headers */
extern int	ip6_dad_count;		/* DupAddrDetectionTransmits */
extern int	ip6_dad_pending;	/* number of currently running DADs */

extern int ip6_auto_flowlabel;
extern int ip6_auto_linklocal;

struct in6pcb;
struct inpcb;

int	icmp6_ctloutput(int, struct socket *, int, int, struct mbuf **);

void	ip6_init(void);
void	ip6intr(void);
void	ip6_input(struct mbuf *);
void	ip6_freepcbopts(struct ip6_pktopts *);
void	ip6_freemoptions(struct ip6_moptions *);
int	ip6_unknown_opt(u_int8_t *, struct mbuf *, int);
u_int8_t *ip6_get_prevhdr(struct mbuf *, int);
int	ip6_nexthdr(struct mbuf *, int, int, int *);
int	ip6_lasthdr(struct mbuf *, int, int, int *);
int	ip6_mforward(struct ip6_hdr *, struct ifnet *, struct mbuf *);
int	ip6_process_hopopts(struct mbuf *, u_int8_t *, int, u_int32_t *,
	     u_int32_t *);
void	ip6_savecontrol(struct inpcb *, struct mbuf *, struct mbuf **);
int	ip6_sysctl(int *, u_int, void *, size_t *, void *, size_t);

void	ip6_forward(struct mbuf *, int);

void	ip6_mloopback(struct ifnet *, struct mbuf *, struct sockaddr_in6 *);
int	ip6_output(struct mbuf *, struct ip6_pktopts *, struct route_in6 *, int,
	    struct ip6_moptions *, struct ifnet **, struct inpcb *);
int	ip6_ctloutput(int, struct socket *, int, int, struct mbuf **);
int	ip6_raw_ctloutput(int, struct socket *, int, int, struct mbuf **);
void	ip6_initpktopts(struct ip6_pktopts *);
int	ip6_setpktopts(struct mbuf *, struct ip6_pktopts *,
	    struct ip6_pktopts *, int, int);
void	ip6_clearpktopts(struct ip6_pktopts *, int);
void	ip6_randomid_init(void);
u_int32_t ip6_randomid(void);

int	route6_input(struct mbuf **, int *, int);

void	frag6_init(void);
int	frag6_input(struct mbuf **, int *, int);
void	frag6_slowtimo(void);
void	frag6_drain(void);

void	rip6_init(void);
int	rip6_input(struct mbuf **mp, int *offp, int proto);
void	rip6_ctlinput(int, struct sockaddr *, void *);
int	rip6_ctloutput(int, struct socket *, int, int, struct mbuf **);
int	rip6_output(struct mbuf *, ...);
int	rip6_usrreq(struct socket *,
	    int, struct mbuf *, struct mbuf *, struct mbuf *, struct proc *);
int	rip6_sysctl(int *, u_int, void *, size_t *, void *, size_t);

int	dest6_input(struct mbuf **, int *, int);
int	none_input(struct mbuf **, int *, int);

struct in6_addr *in6_selectsrc(struct sockaddr_in6 *, struct ip6_pktopts *,
	    struct ip6_moptions *, struct route_in6 *, struct in6_addr *,
	    int *);
int	in6_selectroute(struct sockaddr_in6 *, struct ip6_pktopts *,
	    struct ip6_moptions *, struct route_in6 *, struct ifnet **,
	    struct rtentry **);

u_int32_t ip6_randomflowlabel(void);
#endif /* _KERNEL */

#endif /* !_NETINET6_IP6_VAR_H_ */

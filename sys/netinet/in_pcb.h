/*	$OpenBSD: in_pcb.h,v 1.100 2016/06/27 16:33:48 jca Exp $	*/
/*	$NetBSD: in_pcb.h,v 1.14 1996/02/13 23:42:00 christos Exp $	*/

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
 * Copyright (c) 1982, 1986, 1990, 1993
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
 *	@(#)in_pcb.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NETINET_IN_PCB_H_
#define _NETINET_IN_PCB_H_

#include <sys/queue.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet/ip_ipsp.h>

#include <crypto/siphash.h>

struct pf_state_key;

union inpaddru {
	struct in6_addr iau_addr6;
	struct {
		uint8_t pad[12];
		struct in_addr inaddr;	/* easier transition */
	} iau_a4u;
};

/*
 * Common structure pcb for internet protocol implementation.
 * Here are stored pointers to local and foreign host table
 * entries, local and foreign socket numbers, and pointers
 * up (to a socket structure) and down (to a protocol-specific)
 * control block.
 */
struct inpcb {
	LIST_ENTRY(inpcb) inp_hash;
	LIST_ENTRY(inpcb) inp_lhash;		/* extra hash for lport */
	TAILQ_ENTRY(inpcb) inp_queue;
	struct	  inpcbtable *inp_table;
	union	  inpaddru inp_faddru;		/* Foreign address. */
	union	  inpaddru inp_laddru;		/* Local address. */
#define	inp_faddr	inp_faddru.iau_a4u.inaddr
#define	inp_faddr6	inp_faddru.iau_addr6
#define	inp_laddr	inp_laddru.iau_a4u.inaddr
#define	inp_laddr6	inp_laddru.iau_addr6
	u_int16_t inp_fport;		/* foreign port */
	u_int16_t inp_lport;		/* local port */
	struct	  socket *inp_socket;	/* back pointer to socket */
	caddr_t	  inp_ppcb;		/* pointer to per-protocol pcb */
	union {				/* Route (notice increased size). */
		struct route ru_route;
		struct route_in6 ru_route6;
	} inp_ru;
#define	inp_route	inp_ru.ru_route
#define	inp_route6	inp_ru.ru_route6
	int	  inp_flags;		/* generic IP/datagram flags */
	union {				/* Header prototype. */
		struct ip hu_ip;
		struct ip6_hdr hu_ipv6;
	} inp_hu;
#define	inp_ip		inp_hu.hu_ip
#define	inp_ipv6	inp_hu.hu_ipv6
	struct	  mbuf *inp_options;	/* IP options */
	struct ip6_pktopts *inp_outputopts6; /* IP6 options for outgoing packets */
	int inp_hops;
	union {
		struct ip_moptions *mou_mo;    /* IPv4 multicast options */
		struct ip6_moptions *mou_mo6; /* IPv6 multicast options */
	} inp_mou;
#define inp_moptions inp_mou.mou_mo
#define inp_moptions6 inp_mou.mou_mo6
	u_char	  inp_seclevel[4];
#define SL_AUTH           0             /* Authentication level */
#define SL_ESP_TRANS      1             /* ESP transport level */
#define SL_ESP_NETWORK    2             /* ESP network (encapsulation) level */
#define SL_IPCOMP         3             /* Compression level */
	u_char	inp_ip_minttl;		/* minimum TTL or drop */
#define inp_ip6_minhlim inp_ip_minttl	/* minimum Hop Limit or drop */
#define	inp_flowinfo	inp_hu.hu_ipv6.ip6_flow

	int	inp_cksum6;
#ifndef _KERNEL
#define inp_csumoffset	inp_cksum6
#endif
	struct	icmp6_filter *inp_icmp6filt;
	struct	pf_state_key *inp_pf_sk;
	u_int	inp_rtableid;
	int	inp_pipex;		/* pipex indication */
	int	inp_divertfl;		/* divert flags */
};

LIST_HEAD(inpcbhead, inpcb);

struct inpcbtable {
	TAILQ_HEAD(inpthead, inpcb) inpt_queue;
	struct inpcbhead *inpt_hashtbl, *inpt_lhashtbl;
	SIPHASH_KEY inpt_key;
	u_long	  inpt_hash, inpt_lhash;
	int	  inpt_count;
};

/* flags in inp_flags: */
#define	INP_RECVOPTS	0x001	/* receive incoming IP options */
#define	INP_RECVRETOPTS	0x002	/* receive IP options for reply */
#define	INP_RECVDSTADDR	0x004	/* receive IP dst address */

#define	INP_RXDSTOPTS	INP_RECVOPTS
#define	INP_RXHOPOPTS	INP_RECVRETOPTS
#define	INP_RXINFO	INP_RECVDSTADDR
#define	INP_RXSRCRT	0x010
#define	INP_HOPLIMIT	0x020

#define	INP_HDRINCL	0x008	/* user supplies entire IP header */
#define	INP_HIGHPORT	0x010	/* user wants "high" port binding */
#define	INP_LOWPORT	0x020	/* user wants "low" port binding */
#define	INP_RECVIF	0x080	/* receive incoming interface */
#define	INP_RECVTTL	0x040	/* receive incoming IP TTL */
#define	INP_RECVDSTPORT	0x200	/* receive IP dst addr before rdr */
#define	INP_RECVRTABLE	0x400	/* receive routing table */
#define	INP_IPSECFLOWINFO 0x800	/* receive IPsec flow info */

#define	INP_CONTROLOPTS	(INP_RECVOPTS|INP_RECVRETOPTS|INP_RECVDSTADDR| \
	    INP_RXSRCRT|INP_HOPLIMIT|INP_RECVIF|INP_RECVTTL|INP_RECVDSTPORT| \
	    INP_RECVRTABLE)

/*
 * These flags' values should be determined by either the transport
 * protocol at PRU_BIND, PRU_LISTEN, PRU_CONNECT, etc, or by in_pcb*().
 */
#define	INP_IPV6	0x100	/* sotopf(inp->inp_socket) == PF_INET6 */

/*
 * Flags in inp_flags for IPV6
 */
#define IN6P_HIGHPORT		INP_HIGHPORT	/* user wants "high" port */
#define IN6P_LOWPORT		INP_LOWPORT	/* user wants "low" port */
#define IN6P_RECVDSTPORT	INP_RECVDSTPORT	/* receive IP dst addr before rdr */
#define IN6P_PKTINFO		0x010000 /* receive IP6 dst and I/F */
#define IN6P_HOPLIMIT		0x020000 /* receive hoplimit */
#define IN6P_HOPOPTS		0x040000 /* receive hop-by-hop options */
#define IN6P_DSTOPTS		0x080000 /* receive dst options after rthdr */
#define IN6P_RTHDR		0x100000 /* receive routing header */
#define IN6P_TCLASS		0x400000 /* receive traffic class value */
#define IN6P_AUTOFLOWLABEL	0x800000 /* attach flowlabel automatically */

#define IN6P_ANONPORT		0x4000000 /* port chosen for user */
#define IN6P_RFC2292		0x40000000 /* used RFC2292 API on the socket */
#define IN6P_MTU		0x80000000 /* receive path MTU */

#define IN6P_MINMTU		0x20000000 /* use minimum MTU */

#define IN6P_CONTROLOPTS	(IN6P_PKTINFO|IN6P_HOPLIMIT|IN6P_HOPOPTS|\
				 IN6P_DSTOPTS|IN6P_RTHDR|\
				 IN6P_TCLASS|IN6P_AUTOFLOWLABEL|IN6P_RFC2292|\
				 IN6P_MTU|IN6P_RECVDSTPORT)

#define	INPLOOKUP_WILDCARD	1
#define	INPLOOKUP_SETLOCAL	2
#define	INPLOOKUP_IPV6		4

#define	sotoinpcb(so)	((struct inpcb *)(so)->so_pcb)

/* macros for handling bitmap of ports not to allocate dynamically */
#define	DP_MAPBITS	(sizeof(u_int32_t) * NBBY)
#define	DP_MAPSIZE	(howmany(65536, DP_MAPBITS))
#define	DP_SET(m, p)	((m)[(p) / DP_MAPBITS] |= (1 << ((p) % DP_MAPBITS)))
#define	DP_CLR(m, p)	((m)[(p) / DP_MAPBITS] &= ~(1 << ((p) % DP_MAPBITS)))
#define	DP_ISSET(m, p)	((m)[(p) / DP_MAPBITS] & (1 << ((p) % DP_MAPBITS)))

/* default values for baddynamicports [see ip_init()] */
#define	DEFBADDYNAMICPORTS_TCP	{ \
	587, 749, 750, 751, 871, 2049, \
	6000, 6001, 6002, 6003, 6004, 6005, 6006, 6007, 6008, 6009, 6010, \
	0 }
#define	DEFBADDYNAMICPORTS_UDP	{ 623, 664, 749, 750, 751, 2049, 0 }

#define DEFROOTONLYPORTS_TCP { \
	2049, \
	0 }
#define DEFROOTONLYPORTS_UDP { \
	2049, \
	0 }

struct baddynamicports {
	u_int32_t tcp[DP_MAPSIZE];
	u_int32_t udp[DP_MAPSIZE];
};

#ifdef _KERNEL

extern struct baddynamicports baddynamicports;
extern struct baddynamicports rootonlyports;

#define sotopf(so)  (so->so_proto->pr_domain->dom_family)

void	 in_losing(struct inpcb *);
int	 in_pcballoc(struct socket *, struct inpcbtable *);
int	 in_pcbbind(struct inpcb *, struct mbuf *, struct proc *);
int	 in_pcbaddrisavail(struct inpcb *, struct sockaddr_in *, int,
	    struct proc *);
int	 in_pcbconnect(struct inpcb *, struct mbuf *);
void	 in_pcbdetach(struct inpcb *);
void	 in_pcbdisconnect(struct inpcb *);
struct inpcb *
	 in_pcbhashlookup(struct inpcbtable *, struct in_addr,
			       u_int, struct in_addr, u_int, u_int);
struct inpcb *
	 in_pcblookup_listen(struct inpcbtable *, struct in_addr, u_int, int,
	    struct mbuf *, u_int);
#ifdef INET6
struct inpcb *
	 in6_pcbhashlookup(struct inpcbtable *, const struct in6_addr *,
			       u_int, const struct in6_addr *, u_int, u_int);
struct inpcb *
	 in6_pcblookup_listen(struct inpcbtable *,
			       struct in6_addr *, u_int, int, struct mbuf *,
			       u_int);
int	 in6_pcbaddrisavail(struct inpcb *, struct sockaddr_in6 *, int,
	    struct proc *);
int	 in6_pcbconnect(struct inpcb *, struct mbuf *);
int	 in6_setsockaddr(struct inpcb *, struct mbuf *);
int	 in6_setpeeraddr(struct inpcb *, struct mbuf *);
#endif /* INET6 */
void	 in_pcbinit(struct inpcbtable *, int);
struct inpcb *
	 in_pcblookup_local(struct inpcbtable *, void *, u_int, int, u_int);
void	 in_pcbnotifyall(struct inpcbtable *, struct sockaddr *,
	    u_int, int, void (*)(struct inpcb *, int));
void	 in_pcbrehash(struct inpcb *);
void	 in_rtchange(struct inpcb *, int);
void	 in_setpeeraddr(struct inpcb *, struct mbuf *);
void	 in_setsockaddr(struct inpcb *, struct mbuf *);
int	 in_baddynamic(u_int16_t, u_int16_t);
int	 in_rootonly(u_int16_t, u_int16_t);
int	 in_selectsrc(struct in_addr **, struct sockaddr_in *,
	    struct ip_moptions *, struct route *, struct in_addr *, u_int);
struct rtentry *
	in_pcbrtentry(struct inpcb *);

/* INET6 stuff */
int	in6_pcbnotify(struct inpcbtable *, struct sockaddr_in6 *,
	u_int, const struct sockaddr_in6 *, u_int, u_int, int, void *,
	void (*)(struct inpcb *, int));
int	in6_selecthlim(struct inpcb *);
int	in_pcbpickport(u_int16_t *, void *, int, struct inpcb *, struct proc *);
#endif /* _KERNEL */
#endif /* _NETINET_IN_PCB_H_ */

/*	$OpenBSD: in_pcb.h,v 1.13 1999/01/07 21:50:52 deraadt Exp $	*/
/*	$NetBSD: in_pcb.h,v 1.14 1996/02/13 23:42:00 christos Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#include <sys/queue.h>
#include <netinet6/in6.h>
#include <netinet6/ipv6.h>
#include <netinet6/ipv6_var.h>
#include <netinet6/icmpv6.h>

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
	CIRCLEQ_ENTRY(inpcb) inp_queue;
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
		struct route6 ru_route6;
	} inp_ru;
#define	inp_route	inp_ru.ru_route
#define	inp_route6	inp_ru.ru_route6
	int	  inp_flags;		/* generic IP/datagram flags */
	union {				/* Header prototype. */
		struct ip hu_ip;
		struct ipv6 hu_ipv6;
	} inp_hu;
#define	inp_ip		inp_hu.hu_ip
#define	inp_ipv6	inp_hu.hu_ipv6
	struct	  mbuf *inp_options;	/* IP options */
	struct	  ip_moptions *inp_moptions; /* IP multicast options */
	u_char	  inp_seclevel[3];	/* Only the first 3 are used for now */
#define SL_AUTH           0             /* Authentication level */
#define SL_ESP_TRANS      1             /* ESP transport level */
#define SL_ESP_NETWORK    2             /* ESP network (encapsulation) level */
	u_int8_t  inp_secrequire:4,     /* Condensed State from above */
	          inp_secresult:4;	/* Result from Key Management */
#define SR_FAILED         1             /* Negotiation failed permanently */
#define SR_SUCCESS        2             /* SA successfully established */
#define SR_WAIT           3             /* Waiting for SA */
	int	inp_fflowinfo;          /* Foreign flowlabel & priority */
	int	inp_csumoffset;
	struct	icmpv6_filter inp_filter;
};

struct inpcbtable {
	CIRCLEQ_HEAD(, inpcb) inpt_queue;
	LIST_HEAD(inpcbhead, inpcb) *inpt_hashtbl;
	u_long	  inpt_hash;
	u_int16_t inpt_lastport;
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

#define	INP_CONTROLOPTS	(INP_RECVOPTS|INP_RECVRETOPTS|INP_RECVDSTADDR| \
	    INP_RXSRCRT|INP_HOPLIMIT)

#define	INP_HDRINCL	0x008	/* user supplies entire IP header */
#define	INP_HIGHPORT	0x010	/* user wants "high" port binding */
#define	INP_LOWPORT	0x020	/* user wants "low" port binding */

/*
 * These flags' values should be determined by either the transport
 * protocol at PRU_BIND, PRU_LISTEN, PRU_CONNECT, etc, or by in_pcb*().
 */
#define	INP_IPV6	0x100	/* sotopf(inp->inp_socket) == PF_INET6 */
#define	INP_IPV6_UNDEC	0x200	/* PCB is PF_INET6, but listens for V4/V6 */
#define	INP_IPV6_MAPPED	0x400	/* PF_INET6 PCB which is connected to
				 * an IPv4 host, or is bound to
				 * an IPv4 address (specified with
				 * the mapped form of v6 addresses) */
#define INP_IPV6_MCAST	0x800	/* Set if inp_moptions points to ipv6 ones */

#define	INPLOOKUP_WILDCARD	1
#define	INPLOOKUP_SETLOCAL	2
#define	INPLOOKUP_IPV6		4

#define	sotoinpcb(so)	((struct inpcb *)(so)->so_pcb)

/* macros for handling bitmap of ports not to allocate dynamically */
#define	DP_MAPBITS	(sizeof(u_int32_t) * NBBY)
#define	DP_MAPSIZE	(howmany(IPPORT_RESERVED/2, DP_MAPBITS))
#define	DP_SET(m, p)	((m)[((p) - IPPORT_RESERVED/2) / DP_MAPBITS] |= (1 << ((p) % DP_MAPBITS)))
#define	DP_CLR(m, p)	((m)[((p) - IPPORT_RESERVED/2) / DP_MAPBITS] &= ~(1 << ((p) % DP_MAPBITS)))
#define	DP_ISSET(m, p)	((m)[((p) - IPPORT_RESERVED/2) / DP_MAPBITS] & (1 << ((p) % DP_MAPBITS)))

/* default values for baddynamicports [see ip_init()] */
#define	DEFBADDYNAMICPORTS_TCP	{ 749, 750, 751, 760, 761, 871, 0 }
#define	DEFBADDYNAMICPORTS_UDP	{ 750, 751, 0 }

struct baddynamicports {
	u_int32_t tcp[DP_MAPSIZE];
	u_int32_t udp[DP_MAPSIZE];
};

#ifdef _KERNEL

#define sotopf(so)  (so->so_proto->pr_domain->dom_family)

void	 in_losing __P((struct inpcb *));
int	 in_pcballoc __P((struct socket *, void *));
int	 in_pcbbind __P((void *, struct mbuf *));
int	 in_pcbconnect __P((void *, struct mbuf *));
void	 in_pcbdetach __P((void *));
void	 in_pcbdisconnect __P((void *));
struct inpcb *
	 in_pcbhashlookup __P((struct inpcbtable *, struct in_addr,
			       u_int, struct in_addr, u_int));
void	 in_pcbinit __P((struct inpcbtable *, int));
struct inpcb *
	 in_pcblookup __P((struct inpcbtable *, void *, u_int, void *,
	    u_int, int));
void	 in_pcbnotify __P((struct inpcbtable *, struct sockaddr *,
	    u_int, struct in_addr, u_int, int, void (*)(struct inpcb *, int)));
void	 in_pcbnotifyall __P((struct inpcbtable *, struct sockaddr *,
	    int, void (*)(struct inpcb *, int)));
void	 in_pcbrehash __P((struct inpcb *));
void	 in_rtchange __P((struct inpcb *, int));
void	 in_setpeeraddr __P((struct inpcb *, struct mbuf *));
void	 in_setsockaddr __P((struct inpcb *, struct mbuf *));
int	 in_baddynamic __P((u_int16_t, u_int16_t));
#endif

/*	$OpenBSD: ipx_if.h,v 1.6 2003/06/02 23:28:16 millert Exp $	*/

/*-
 *
 * Copyright (c) 1996 Michael Shalayeff
 * Copyright (c) 1995, Mike Mitchell
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
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
 *	@(#)ipx_if.h
 *
 * from FreeBSD Id: ipx_if.h,v 1.5 1995/11/24 12:25:05 bde Exp
 */

#ifndef _NETIPX_IPX_IF_H_
#define	_NETIPX_IPX_IF_H_

/*
 * Interface address.  One of these structures
 * is allocated for each interface with an internet address.
 * The ifaddr structure contains the protocol-independent part
 * of the structure and is assumed to be first.
 */

struct ipx_ifaddr {
	struct	ifaddr ia_ifa;		/* protocol-independent info */
#define	ia_ifp		ia_ifa.ifa_ifp
#define	ia_flags	ia_ifa.ifa_flags
	TAILQ_ENTRY(ipx_ifaddr)	ia_list;/* list of IPX addresses */
	struct	sockaddr_ipx ia_addr;	/* reserve space for my address */
	struct	sockaddr_ipx ia_dstaddr;/* space for my broadcast address */
#define ia_broadaddr	ia_dstaddr
	struct	sockaddr_ipx ia_netmask;/* space for my network mask */
};

struct	ipx_aliasreq {
	char	ifra_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	struct	sockaddr_ipx ifra_addr;
	struct	sockaddr_ipx ifra_dstaddr;
#define ifra_broadaddr ifra_dstaddr
};
/*
 * Given a pointer to an ipx_ifaddr (ifaddr),
 * return a pointer to the addr as a sockadd_ipx.
 */

#define	IA_SIPX(ia) (&(((struct ipx_ifaddr *)(ia))->ia_addr))

#define IPX_ETHERTYPE_8023	0x0001	/* Ethernet_802.3 */
#define IPX_ETHERTYPE_8022	0x0004	/* Ethernet_802.2 */
#define	IPX_ETHERTYPE_SNAP	0x0005	/* Ethernet_SNAP, internal use only */
#define	IPX_ETHERTYPE_8022TR	0x0011	/* Ethernet_802.2 w/ trailers */
#define IPX_ETHERTYPE_II	0x8137	/* Ethernet_II */
#define IPX_ETHERTYPE_IPX	IPX_ETHERTYPE_II

#ifdef	IPXIP
struct ipxip_req {
	struct sockaddr rq_ipx;	/* must be ipx format destination */
	struct sockaddr rq_ip;	/* must be ip format gateway */
	u_int16_t	rq_flags;
};
#endif

#ifdef	_KERNEL
TAILQ_HEAD(ipx_ifaddrhead, ipx_ifaddr);
extern struct	ifqueue	ipxintrq;	/* IPX input packet queue */
extern struct	ipx_ifaddrhead ipx_ifaddr;

struct ipx_ifaddr *
	ipx_iaonnetof(struct ipx_addr *dst);
int	ipx_ifinit(struct ifnet *ifp, struct ipx_ifaddr *ia,
			struct sockaddr_ipx *sipx, int scrub);
void	ipx_ifscrub(struct ifnet *ifp, struct ipx_ifaddr *ia);
#endif

#endif /* !_NETIPX_IPX_IF_H_ */

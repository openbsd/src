/*	$OpenBSD: in_var.h,v 1.46 2026/01/03 14:10:04 bluhm Exp $	*/
/*	$NetBSD: in_var.h,v 1.16 1996/02/13 23:42:15 christos Exp $	*/

/*
 * Copyright (c) 1985, 1986, 1993
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
 *	@(#)in_var.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _NETINET_IN_VAR_H_
#define _NETINET_IN_VAR_H_

#include <sys/queue.h>

#ifdef _KERNEL
/*
 * Interface address, Internet version.  One of these structures
 * is allocated for each interface with an Internet address.
 * The ifaddr structure contains the protocol-independent part
 * of the structure and is assumed to be first.
 */
struct in_ifaddr {
	struct	ifaddr ia_ifa;		/* protocol-independent info */
#define	ia_ifp		ia_ifa.ifa_ifp
#define	ia_flags	ia_ifa.ifa_flags
					/* ia_net{,mask} in host order */
	u_int32_t ia_net;		/* network number of interface */
	u_int32_t ia_netmask;		/* mask of net part */
	TAILQ_ENTRY(in_ifaddr) ia_list;	/* list of internet addresses */
	struct	sockaddr_in ia_addr;	/* reserve space for interface name */
	struct	sockaddr_in ia_dstaddr;	/* reserve space for broadcast addr */
#define	ia_broadaddr	ia_dstaddr
	struct	sockaddr_in ia_sockmask; /* reserve space for general netmask */
	struct  in_multi *ia_allhosts;	/* multicast address record for
					   the allhosts multicast group */
};
#endif

struct	in_aliasreq {
	char	ifra_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	union {
		struct	sockaddr_in ifrau_addr;
		int	ifrau_align;
	} ifra_ifrau;
#ifndef ifra_addr
#define ifra_addr	ifra_ifrau.ifrau_addr
#endif
	struct	sockaddr_in ifra_dstaddr;
#define	ifra_broadaddr	ifra_dstaddr
	struct	sockaddr_in ifra_mask;
};

#ifdef _KERNEL
struct router_info;

/*
 * Internet multicast address structure.  There is one of these for each IP
 * multicast group to which this host belongs on a given network interface.
 */
struct in_multi {
	struct ifmaddr		inm_ifma;   /* Protocol-independent info */
#define inm_refcnt		inm_ifma.ifma_refcnt
#define inm_ifidx		inm_ifma.ifma_ifidx

	struct sockaddr_in	inm_sin;   /* IPv4 multicast address */
#define inm_addr		inm_sin.sin_addr

	u_int			inm_state; /* state of membership */
	u_int			inm_timer; /* IGMP membership report timer */

	struct router_info	*inm_rti;  /* router version info */
};

static __inline struct in_multi *
ifmatoinm(struct ifmaddr *ifma)
{
       return ((struct in_multi *)(ifma));
}

struct	in_ifaddr *in_ifp2ia(struct ifnet *);
int	in_ifinit(struct ifnet *,
	    struct in_ifaddr *, struct sockaddr_in *, int);
struct	in_multi *in_lookupmulti(const struct in_addr *, struct ifnet *);
struct	in_multi *in_addmulti(const struct in_addr *, struct ifnet *);
void	in_delmulti(struct in_multi *);
int	in_hasmulti(const struct in_addr *, struct ifnet *);
void	in_ifscrub(struct ifnet *, struct in_ifaddr *);
int	in_control(struct socket *, u_long, caddr_t, struct ifnet *);
int	in_ioctl(u_long, caddr_t, struct ifnet *, int);
void	in_prefixlen2mask(struct in_addr *, int);
#endif /* _KERNEL */

#endif /* _NETINET_IN_VAR_H_ */

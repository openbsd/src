/*	$OpenBSD: in_var.h,v 1.7 2003/12/10 07:22:43 itojun Exp $	*/
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

/*
 * Interface address, Internet version.  One of these structures
 * is allocated for each interface with an Internet address.
 * The ifaddr structure contains the protocol-independent part
 * of the structure and is assumed to be first.
 */
struct in_ifaddr {
	struct	ifaddr ia_ifa;		/* protocol-independent info */
#define	ia_ifp		ia_ifa.ifa_ifp
#define ia_flags	ia_ifa.ifa_flags
					/* ia_{,sub}net{,mask} in host order */
	u_int32_t ia_net;		/* network number of interface */
	u_int32_t ia_netmask;		/* mask of net part */
	u_int32_t ia_subnet;		/* subnet number, including net */
	u_int32_t ia_subnetmask;	/* mask of subnet part */
	struct	in_addr ia_netbroadcast; /* to recognize net broadcasts */
	TAILQ_ENTRY(in_ifaddr) ia_list;	/* list of internet addresses */
	struct	sockaddr_in ia_addr;	/* reserve space for interface name */
	struct	sockaddr_in ia_dstaddr;	/* reserve space for broadcast addr */
#define	ia_broadaddr	ia_dstaddr
	struct	sockaddr_in ia_sockmask; /* reserve space for general netmask */
	LIST_HEAD(, in_multi) ia_multiaddrs; /* list of multicast addresses */
};

struct	in_aliasreq {
	char	ifra_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	struct	sockaddr_in ifra_addr;
	struct	sockaddr_in ifra_dstaddr;
#define	ifra_broadaddr	ifra_dstaddr
	struct	sockaddr_in ifra_mask;
};
/*
 * Given a pointer to an in_ifaddr (ifaddr),
 * return a pointer to the addr as a sockaddr_in.
 */
#define	IA_SIN(ia) (&(((struct in_ifaddr *)(ia))->ia_addr))


#ifdef	_KERNEL
TAILQ_HEAD(in_ifaddrhead, in_ifaddr);
extern	struct	in_ifaddrhead in_ifaddr;
extern	struct	ifqueue	ipintrq;		/* ip packet input queue */
extern	int	inetctlerrmap[];
void	in_socktrim(struct sockaddr_in *);


/*
 * Macro for finding the interface (ifnet structure) corresponding to one
 * of our IP addresses.
 */
#define INADDR_TO_IFP(addr, ifp) \
	/* struct in_addr addr; */ \
	/* struct ifnet *ifp; */ \
{ \
	struct in_ifaddr *ia; \
\
	for (ia = in_ifaddr.tqh_first; \
	    ia != NULL && ia->ia_addr.sin_addr.s_addr != (addr).s_addr; \
	    ia = ia->ia_list.tqe_next) \
		 continue; \
	(ifp) = (ia == NULL) ? NULL : ia->ia_ifp; \
}

/*
 * Macro for finding the internet address structure (in_ifaddr) corresponding
 * to a given interface (ifnet structure).
 */
#define IFP_TO_IA(ifp, ia) \
	/* struct ifnet *ifp; */ \
	/* struct in_ifaddr *ia; */ \
{ \
	for ((ia) = in_ifaddr.tqh_first; \
	    (ia) != NULL && (ia)->ia_ifp != (ifp); \
	    (ia) = (ia)->ia_list.tqe_next) \
		continue; \
}
#endif

/*
 * Per-interface router version information.
 */
struct router_info {
	struct	ifnet *rti_ifp;
	int	rti_type;	/* type of router on this interface */
	int	rti_age;	/* time since last v1 query */
	struct	router_info *rti_next;
};

/*
 * Internet multicast address structure.  There is one of these for each IP
 * multicast group to which this host belongs on a given network interface.
 * They are kept in a linked list, rooted in the interface's in_ifaddr
 * structure.
 */
struct in_multi {
	struct	in_addr inm_addr;	/* IP multicast address */
	struct	ifnet *inm_ifp;		/* back pointer to ifnet */
	struct	in_ifaddr *inm_ia;	/* back pointer to in_ifaddr */
	u_int	inm_refcount;		/* no. membership claims by sockets */
	u_int	inm_timer;		/* IGMP membership report timer */
	LIST_ENTRY(in_multi) inm_list;	/* list of multicast addresses */
	u_int	inm_state;		/* state of membership */
	struct	router_info *inm_rti;	/* router version info */
};

#ifdef _KERNEL
/*
 * Structure used by macros below to remember position when stepping through
 * all of the in_multi records.
 */
struct in_multistep {
	struct in_ifaddr *i_ia;
	struct in_multi *i_inm;
};

/*
 * Macro for looking up the in_multi record for a given IP multicast address
 * on a given interface.  If no matching record is found, "inm" returns NULL.
 */
#define IN_LOOKUP_MULTI(addr, ifp, inm) \
	/* struct in_addr addr; */ \
	/* struct ifnet *ifp; */ \
	/* struct in_multi *inm; */ \
{ \
	struct in_ifaddr *ia; \
\
	IFP_TO_IA((ifp), ia); \
	if (ia == NULL) \
		(inm) = NULL; \
	else \
		for ((inm) = ia->ia_multiaddrs.lh_first; \
		    (inm) != NULL && (inm)->inm_addr.s_addr != (addr).s_addr; \
		     (inm) = inm->inm_list.le_next) \
			 continue; \
}

/*
 * Macro to step through all of the in_multi records, one at a time.
 * The current position is remembered in "step", which the caller must
 * provide.  IN_FIRST_MULTI(), below, must be called to initialize "step"
 * and get the first record.  Both macros return a NULL "inm" when there
 * are no remaining records.
 */
#define IN_NEXT_MULTI(step, inm) \
	/* struct in_multistep  step; */ \
	/* struct in_multi *inm; */ \
{ \
	if (((inm) = (step).i_inm) != NULL) \
		(step).i_inm = (inm)->inm_list.le_next; \
	else \
		while ((step).i_ia != NULL) { \
			(inm) = (step).i_ia->ia_multiaddrs.lh_first; \
			(step).i_ia = (step).i_ia->ia_list.tqe_next; \
			if ((inm) != NULL) { \
				(step).i_inm = (inm)->inm_list.le_next; \
				break; \
			} \
		} \
}

#define IN_FIRST_MULTI(step, inm) \
	/* struct in_multistep step; */ \
	/* struct in_multi *inm; */ \
{ \
	(step).i_ia = in_ifaddr.tqh_first; \
	(step).i_inm = NULL; \
	IN_NEXT_MULTI((step), (inm)); \
}

int	in_ifinit(struct ifnet *,
	    struct in_ifaddr *, struct sockaddr_in *, int);
struct	in_multi *in_addmulti(struct in_addr *, struct ifnet *);
void	in_delmulti(struct in_multi *);
void	in_ifscrub(struct ifnet *, struct in_ifaddr *);
int	in_control(struct socket *, u_long, caddr_t, struct ifnet *);
#endif


/* INET6 stuff */
#include <netinet6/in6_var.h>

#endif /* _NETINET_IN_VAR_H_ */

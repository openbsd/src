/*	$OpenBSD: iso_var.h,v 1.6 2003/06/02 23:28:18 millert Exp $	*/
/*	$NetBSD: iso_var.h,v 1.8 1996/02/13 22:10:32 christos Exp $	*/

/*-
 * Copyright (c) 1988, 1991, 1993
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
 *	@(#)iso_var.h	8.1 (Berkeley) 6/10/93
 */

/***********************************************************
		Copyright IBM Corporation 1987

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of IBM not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
IBM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/
#ifndef _NETISO_ISO_VAR_H_
#define _NETISO_ISO_VAR_H_
/*
 * ARGO Project, Computer Sciences Dept., University of Wisconsin - Madison
 */

/*
 *	Interface address, iso version. One of these structures is
 *	allocated for each interface with an osi address. The ifaddr
 *	structure conatins the protocol-independent part
 *	of the structure, and is assumed to be first.
 */
struct iso_ifaddr {
	struct ifaddr   ia_ifa;	/* protocol-independent info */
#define ia_ifp		ia_ifa.ifa_ifp
#define	ia_flags	ia_ifa.ifa_flags
	int             ia_snpaoffset;
	                TAILQ_ENTRY(iso_ifaddr) ia_list;	/* list of iso addresses */
	struct sockaddr_iso ia_addr;	/* reserve space for interface name */
	struct sockaddr_iso ia_dstaddr;	/* reserve space for broadcast addr */
#define	ia_broadaddr	ia_dstaddr
	struct sockaddr_iso ia_sockmask;	/* reserve space for general
						 * netmask */
};

struct iso_aliasreq {
	char            ifra_name[IFNAMSIZ];	/* if name, e.g. "en0" */
	struct sockaddr_iso ifra_addr;
	struct sockaddr_iso ifra_dstaddr;
	struct sockaddr_iso ifra_mask;
	int             ifra_snpaoffset;
};

struct iso_ifreq {
	char            ifr_name[IFNAMSIZ];	/* if name, e.g. "en0" */
	struct sockaddr_iso ifr_Addr;
};

/*
 *	Given a pointer to an iso_ifaddr (ifaddr),
 *	return a pointer to the addr as a sockaddr_iso
 */
#define	IA_SIS(ia) (&(((struct iso_ifaddr *)(ia))->ia_addr))

#define	SIOCDIFADDR_ISO	_IOW('i',25, struct iso_ifreq)	/* delete IF addr */
#define	SIOCAIFADDR_ISO	_IOW('i',26, struct iso_aliasreq)	/* add/chg IFalias */
#define	SIOCGIFADDR_ISO	_IOWR('i',33, struct iso_ifreq)	/* get ifnet address */
#define	SIOCGIFDSTADDR_ISO _IOWR('i',34, struct iso_ifreq)	/* get dst address */
#define	SIOCGIFNETMASK_ISO _IOWR('i',37, struct iso_ifreq)	/* get dst address */

/*
 * This stuff should go in if.h or if_llc.h or someplace else,
 * but for now . . .
 */

struct llc_etherhdr {
	char            dst[6];
	char            src[6];
	char            len[2];
	char            llc_dsap;
	char            llc_ssap;
	char            llc_ui_byte;
};

struct snpa_hdr {
	struct ifnet   *snh_ifp;
	char            snh_dhost[6];
	char            snh_shost[6];
	short           snh_flags;
};
#ifdef _KERNEL
TAILQ_HEAD(iso_ifaddrhead, iso_ifaddr);
struct iso_ifaddrhead iso_ifaddr;	/* linked list of iso address ifaces */
struct ifqueue  clnlintrq;	/* clnl packet input queue */
struct afhash;
struct llinfo_llc;

/* iso.c */
int iso_addrmatch1(struct iso_addr *, struct iso_addr *);
int iso_addrmatch(struct sockaddr_iso *, struct sockaddr_iso *);
int iso_netmatch(struct sockaddr_iso *, struct sockaddr_iso *);
u_long iso_hashchar(caddr_t, int);
int iso_hash(struct sockaddr_iso *, struct afhash *);
int iso_netof(struct iso_addr *, caddr_t);
int iso_control(struct socket *, u_long, caddr_t, struct ifnet *);
void iso_ifscrub(struct ifnet *, struct iso_ifaddr *);
int iso_ifinit(struct ifnet *, struct iso_ifaddr *, struct sockaddr_iso *, int);
struct ifaddr *iso_ifwithidi(struct sockaddr *);
int iso_ck_addr(struct iso_addr *);
int iso_eqtype(struct iso_addr *, struct iso_addr *);
struct iso_ifaddr *iso_localifa(struct sockaddr_iso *);
int iso_nlctloutput(int, int, caddr_t, struct mbuf *);
void dump_isoaddr(struct sockaddr_iso *);

/* iso_chksum.c */
int iso_check_csum(struct mbuf *, int);
void iso_gen_csum(struct mbuf *, int, int);
int m_datalen(struct mbuf *);
int m_compress(struct mbuf *, struct mbuf **);

/* iso_snpac.c */
void llc_rtrequest(int, struct rtentry *, struct rt_addrinfo *);
void iso_setmcasts(struct ifnet *, int);
int iso_snparesolve(struct ifnet *, struct sockaddr_iso *,
			 caddr_t, int *);
void snpac_free(struct llinfo_llc *);
int snpac_add(struct ifnet *, struct iso_addr *, caddr_t, int,
		   u_short, int);
int snpac_ioctl(struct socket *, u_long, caddr_t);
void snpac_logdefis(struct rtentry *);
void snpac_age(void *);
int snpac_ownmulti(caddr_t, u_int);
void snpac_flushifp(struct ifnet *);
void snpac_rtrequest(int, struct iso_addr *, struct iso_addr *,
			 struct iso_addr *, int, struct rtentry **);
void snpac_addrt(struct ifnet *, struct iso_addr *, struct iso_addr *,
	             struct iso_addr *);
#endif /* _KERNEL */
#endif /* _NETISO_ISO_VAR_H_ */

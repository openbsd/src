/*	$OpenBSD: ipx_ip.h,v 1.4 2003/06/02 23:28:16 millert Exp $	*/

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
 *	@(#)ipxip.h
 *
 * from FreeBSD Id: ipx_ip.h,v 1.5 1995/12/16 03:43:49 bde Exp
 */

#ifndef _NETIPX_IPXIP_H_
#define	_NETIPX_IPXIP_H_

struct ifnet_en {
	struct ifnet ifen_ifnet;
	struct route ifen_route;
	struct in_addr ifen_src;
	struct in_addr ifen_dst;
	struct ifnet_en *ifen_next;
};

#define LOMTU	(1024+512)	/* XXX this is TINY_LOMTU */

#ifdef _KERNEL

extern struct ifnet ipxipif;
extern struct ifnet_en *ipxip_list;

void	ipxipprotoinit(void);
struct ifnet_en * ipxipattach(void);
void	*ipxip_ctlinput(int cmd, struct sockaddr *sa, void *arg);
int	ipxip_free(struct ifnet *ifp);
void	ipxip_input(struct mbuf *m, ... );
int	ipxipioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
int	ipxipoutput(struct ifnet *ifp, struct mbuf *m,
			 struct sockaddr *dst, struct rtentry *rt);
int	ipxip_route(struct mbuf *m);
void	ipxip_rtchange(struct in_addr *dst);
void	ipxipstart(struct ifnet *ifp);

#endif /* _KERNEL */

#endif /* !_NETIPX_IPXIP_H_ */

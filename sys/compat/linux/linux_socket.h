/*	$OpenBSD: linux_socket.h,v 1.7 2002/11/27 07:30:36 ish Exp $	*/
/*	$NetBSD: linux_socket.h,v 1.3 1995/05/28 10:16:34 mycroft Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUX_SOCKET_H
#define _LINUX_SOCKET_H

/*
 * Various Linux socket defines. Everything that is not re-defined here
 * is the same as in OpenBSD.
 *
 * COMPAT_43 is assumed, and the osockaddr struct is used (it is what
 * Linux uses)
 */

/*
 * Address families. There are fewer of them, and they're numbered
 * a bit different
 */

#define LINUX_AF_UNSPEC		0
#define LINUX_AF_UNIX		1
#define LINUX_AF_INET		2
#define LINUX_AF_AX25		3
#define LINUX_AF_IPX		4
#define LINUX_AF_APPLETALK	5
#define LINUX_AF_INET6		10
#define LINUX_AF_MAX		32

/*
 * Option levels for [gs]etsockopt(2). Only SOL_SOCKET is different,
 * the rest matches IPPROTO_XXX
 */

#define LINUX_SOL_SOCKET	1
#define LINUX_SOL_IP		0
#define LINUX_SOL_IPX		256
#define LINUX_SOL_AX25		257
#define LINUX_SOL_TCP		6
#define LINUX_SOL_UDP		17

/*
 * Options for [gs]etsockopt(2), socket level. For Linux, they
 * are not masks, but just increasing numbers.
 */

#define LINUX_SO_DEBUG		1
#define LINUX_SO_REUSEADDR	2
#define LINUX_SO_TYPE		3
#define LINUX_SO_ERROR		4
#define LINUX_SO_DONTROUTE	5
#define LINUX_SO_BROADCAST	6
#define LINUX_SO_SNDBUF		7
#define LINUX_SO_RCVBUF		8
#define LINUX_SO_KEEPALIVE	9
#define LINUX_SO_OOBINLINE	10
#define LINUX_SO_NO_CHECK	11
#define LINUX_SO_PRIORITY	12
#define LINUX_SO_LINGER		13

/*
 * Options vor [gs]etsockopt(2), IP level.
 */

#define	LINUX_IP_TOS		1
#define	LINUX_IP_TTL		2
#define	LINUX_IP_HDRINCL	3
#define	LINUX_IP_MULTICAST_IF	32
#define	LINUX_IP_MULTICAST_TTL	33
#define	LINUX_IP_MULTICAST_LOOP	34
#define	LINUX_IP_ADD_MEMBERSHIP	35
#define	LINUX_IP_DROP_MEMBERSHIP 36

/*
 * Options vor [gs]etsockopt(2), TCP level.
 */

#define	LINUX_TCP_NODELAY	1
#define	LINUX_TCP_MAXSEG	2

struct linux_sockaddr {
	unsigned short	sa_family;
	char		sa_data[14];
};

struct linux_ifmap {
	unsigned long	mem_start;
	unsigned long	mem_end;
	unsigned short	base_addr; 
	unsigned char	irq;
	unsigned char	dma;
	unsigned char	port;
};

struct linux_ifreq {
#define LINUX_IFHWADDRLEN     6
#define LINUX_IFNAMSIZ        16
	union {
		char ifrn_name[LINUX_IFNAMSIZ];		/* if name, e.g. "en0" */       
	} ifr_ifrn;

	union {
		struct linux_sockaddr	ifru_addr;
		struct linux_sockaddr	ifru_dstaddr;
		struct linux_sockaddr	ifru_broadaddr;
		struct linux_sockaddr	ifru_netmask;
		struct linux_sockaddr	ifru_hwaddr;
		short			ifru_flags;
		int			ifru_metric;
		int			ifru_mtu;
		struct linux_ifmap	ifru_map;
		char			ifru_slave[LINUX_IFNAMSIZ];
		caddr_t			ifru_data;
	} ifr_ifru;
};

#define ifr_name	ifr_ifrn.ifrn_name		/* interface name */
#define ifr_hwaddr	ifr_ifru.ifru_hwaddr		/* MAC address */

#endif /* _LINUX_SOCKET_H */

/*
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: ipv6.h,v 1.9 2001/07/17 20:29:33 gson Exp $ */

#ifndef ISC_IPV6_H
#define ISC_IPV6_H 1

/*****
 ***** Module Info
 *****/

/*
 * This file defines additional information necessary for IP v6 support
 */

#ifndef AF_INET6
#define AF_INET6 99
#endif

#ifndef PF_INET6
#define PF_INET6 AF_INET6
#endif

#define s6_addr8	s6_addr
#define in6_addr in_addr6

#define IN6ADDR_ANY_INIT 	{{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }}
#define IN6ADDR_LOOPBACK_INIT 	{{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 }}

LIBISC_EXTERNAL_DATA extern const struct in_addr6 in6addr_any;
LIBISC_EXTERNAL_DATA extern const struct in_addr6 in6addr_loopback;

#ifndef ISC_PLATFORM_HAVEIN6PKTINFO
struct in6_pktinfo {
	struct in6_addr ipi6_addr;    /* src/dst IPv6 address */
	unsigned int    ipi6_ifindex; /* send/recv interface index */
};
#endif

/*
 * Unspecified
 */

#define IN6_IS_ADDR_UNSPECIFIED(x)      \
*((u_long *)((x)->s6_addr)    ) == 0 && \
*((u_long *)((x)->s6_addr) + 1) == 0 && \
*((u_long *)((x)->s6_addr) + 2) == 0 && \
*((u_long *)((x)->s6_addr) + 3) == 1 \
)

/*
 * Loopback
 */
#define IN6_IS_ADDR_LOOPBACK(x) (\
*((u_long *)((x)->s6_addr)    ) == 0 && \
*((u_long *)((x)->s6_addr) + 1) == 0 && \
*((u_long *)((x)->s6_addr) + 2) == 0 && \
*((u_long *)((x)->s6_addr) + 3) == 1 \
)

/*
 * IPv4 compatible
 */
#define IN6_IS_ADDR_V4COMPAT(x)  (\
*((u_long *)((x)->s6_addr)    ) == 0 && \
*((u_long *)((x)->s6_addr) + 1) == 0 && \
*((u_long *)((x)->s6_addr) + 2) == 0 && \
*((u_long *)((x)->s6_addr) + 3) != 0 && \
*((u_long *)((x)->s6_addr) + 3) != htonl(1) \
)

/*
 * Mapped
 */
#define IN6_IS_ADDR_V4MAPPED(x) (\
*((u_long *)((x)->s6_addr)    ) == 0 && \
*((u_long *)((x)->s6_addr) + 1) == 0 && \
*((u_long *)((x)->s6_addr) + 2) == htonl(0x0000ffff))

/*
 * Multicast
 */
#define IN6_IS_ADDR_MULTICAST(a)	\
	((a)->s6_addr8[0] == 0xffU)

ISC_LANG_ENDDECLS

#endif /* ISC_IPV6_H */

/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: net.h,v 1.5 2020/02/13 13:53:01 jsg Exp $ */

#ifndef ISC_NET_H
#define ISC_NET_H 1

/*****
 ***** Module Info
 *****/

/*! \file
 * \brief
 * Basic Networking Types
 *
 * This module is responsible for defining the following basic networking
 * types:
 *
 *\li		struct in_addr
 *\li		struct in6_addr
 *\li		struct in6_pktinfo
 *\li		struct sockaddr
 *\li		struct sockaddr_in
 *\li		struct sockaddr_in6
 *\li		struct sockaddr_storage
 *\li		in_port_t
 *
 * It ensures that the AF_ and PF_ macros are defined.
 *
 * It declares ntoh[sl]() and hton[sl]().
 *
 * It declares inet_aton(), inet_ntop(), and inet_pton().
 *
 * It ensures that #INADDR_LOOPBACK, #INADDR_ANY, #IN6ADDR_ANY_INIT,
 * IN6ADDR_V4MAPPED_INIT, in6addr_any, and in6addr_loopback are available.
 *
 * It ensures that IN_MULTICAST() is available to check for multicast
 * addresses.
 *
 * MP:
 *\li	No impact.
 *
 * Reliability:
 *\li	No anticipated impact.
 *
 * Resources:
 *\li	N/A.
 *
 * Security:
 *\li	No anticipated impact.
 *
 * Standards:
 *\li	BSD Socket API
 *\li	RFC2553
 */

/***
 *** Imports.
 ***/

#include <sys/socket.h>		/* Contractual promise. */

#include <netinet/in.h>		/* Contractual promise. */
#include <arpa/inet.h>		/* Contractual promise. */

/*% IP address. */
#define ISC__IPADDR(x)	((uint32_t)htonl((uint32_t)(x)))

/*% Is IP address multicast? */
#define ISC_IPADDR_ISMULTICAST(i) \
		(((uint32_t)(i) & ISC__IPADDR(0xf0000000)) \
		 == ISC__IPADDR(0xe0000000))

#define ISC_IPADDR_ISEXPERIMENTAL(i) \
		(((uint32_t)(i) & ISC__IPADDR(0xf0000000)) \
		 == ISC__IPADDR(0xf0000000))

/***
 *** Functions.
 ***/

void
isc_net_disableipv4(void);

void
isc_net_disableipv6(void);

#define ISC_NET_DSCPRECVV4	0x01	/* Can receive sent DSCP value IPv4 */
#define ISC_NET_DSCPRECVV6	0x02	/* Can receive sent DSCP value IPv6 */
#define ISC_NET_DSCPSETV4	0x04	/* Can set DSCP on socket IPv4 */
#define ISC_NET_DSCPSETV6	0x08	/* Can set DSCP on socket IPv6 */
#define ISC_NET_DSCPPKTV4	0x10	/* Can set DSCP on per packet IPv4 */
#define ISC_NET_DSCPPKTV6	0x20	/* Can set DSCP on per packet IPv6 */
#define ISC_NET_DSCPALL		0x3f	/* All valid flags */

unsigned int
isc_net_probedscp(void);
/*%<
 * Probe the level of DSCP support.
 */

#endif /* ISC_NET_H */

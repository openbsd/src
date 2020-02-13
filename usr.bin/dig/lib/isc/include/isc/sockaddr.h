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

/* $Id: sockaddr.h,v 1.3 2020/02/13 13:53:01 jsg Exp $ */

#ifndef ISC_SOCKADDR_H
#define ISC_SOCKADDR_H 1

/*! \file isc/sockaddr.h */

#include <isc/net.h>
#include <isc/types.h>
#include <sys/un.h>

struct isc_sockaddr {
	union {
		struct sockaddr		sa;
		struct sockaddr_in	sin;
		struct sockaddr_in6	sin6;
		struct sockaddr_storage ss;
		struct sockaddr_un	sunix;
	}				type;
	unsigned int			length;		/* XXXRTH beginning? */
	ISC_LINK(struct isc_sockaddr)	link;
};

#define ISC_SOCKADDR_CMPADDR	  0x0001	/*%< compare the address
						 *   sin_addr/sin6_addr */
#define ISC_SOCKADDR_CMPPORT 	  0x0002	/*%< compare the port
						 *   sin_port/sin6_port */
#define ISC_SOCKADDR_CMPSCOPE     0x0004	/*%< compare the scope
						 *   sin6_scope */
#define ISC_SOCKADDR_CMPSCOPEZERO 0x0008	/*%< when comparing scopes
						 *   zero scopes always match */

isc_boolean_t
isc_sockaddr_compare(const isc_sockaddr_t *a, const isc_sockaddr_t *b,
		     unsigned int flags);
/*%<
 * Compare the elements of the two address ('a' and 'b') as specified
 * by 'flags' and report if they are equal or not.
 *
 * 'flags' is set from ISC_SOCKADDR_CMP*.
 */

isc_boolean_t
isc_sockaddr_equal(const isc_sockaddr_t *a, const isc_sockaddr_t *b);
/*%<
 * Return ISC_TRUE iff the socket addresses 'a' and 'b' are equal.
 */

isc_boolean_t
isc_sockaddr_eqaddr(const isc_sockaddr_t *a, const isc_sockaddr_t *b);
/*%<
 * Return ISC_TRUE iff the address parts of the socket addresses
 * 'a' and 'b' are equal, ignoring the ports.
 */

void
isc_sockaddr_any(isc_sockaddr_t *sockaddr);
/*%<
 * Return the IPv4 wildcard address.
 */

void
isc_sockaddr_any6(isc_sockaddr_t *sockaddr);
/*%<
 * Return the IPv6 wildcard address.
 */

void
isc_sockaddr_anyofpf(isc_sockaddr_t *sockaddr, int family);
/*%<
 * Set '*sockaddr' to the wildcard address of protocol family
 * 'family'.
 *
 * Requires:
 * \li	'family' is AF_INET or AF_INET6.
 */

void
isc_sockaddr_fromin(isc_sockaddr_t *sockaddr, const struct in_addr *ina,
		    in_port_t port);
/*%<
 * Construct an isc_sockaddr_t from an IPv4 address and port.
 */

void
isc_sockaddr_fromin6(isc_sockaddr_t *sockaddr, const struct in6_addr *ina6,
		     in_port_t port);
/*%<
 * Construct an isc_sockaddr_t from an IPv6 address and port.
 */

int
isc_sockaddr_pf(const isc_sockaddr_t *sockaddr);
/*%<
 * Get the protocol family of 'sockaddr'.
 *
 * Requires:
 *
 *\li	'sockaddr' is a valid sockaddr with an address family of AF_INET
 *	or AF_INET6.
 *
 * Returns:
 *
 *\li	The protocol family of 'sockaddr', e.g. PF_INET or PF_INET6.
 */

in_port_t
isc_sockaddr_getport(const isc_sockaddr_t *sockaddr);
/*%<
 * Get the port stored in 'sockaddr'.
 */

isc_result_t
isc_sockaddr_totext(const isc_sockaddr_t *sockaddr, isc_buffer_t *target);
/*%<
 * Append a text representation of 'sockaddr' to the buffer 'target'.
 * The text will include both the IP address (v4 or v6) and the port.
 * The text is null terminated, but the terminating null is not
 * part of the buffer's used region.
 *
 * Returns:
 * \li	ISC_R_SUCCESS
 * \li	ISC_R_NOSPACE	The text or the null termination did not fit.
 */

void
isc_sockaddr_format(const isc_sockaddr_t *sa, char *array, unsigned int size);
/*%<
 * Format a human-readable representation of the socket address '*sa'
 * into the character array 'array', which is of size 'size'.
 * The resulting string is guaranteed to be null-terminated.
 */

isc_boolean_t
isc_sockaddr_ismulticast(const isc_sockaddr_t *sa);
/*%<
 * Returns #ISC_TRUE if the address is a multicast address.
 */

isc_boolean_t
isc_sockaddr_islinklocal(const isc_sockaddr_t *sa);
/*%<
 * Returns ISC_TRUE if the address is a link local address.
 */

isc_boolean_t
isc_sockaddr_issitelocal(const isc_sockaddr_t *sa);
/*%<
 * Returns ISC_TRUE if the address is a sitelocal address.
 */

#define ISC_SOCKADDR_FORMATSIZE \
	sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:XXX.XXX.XXX.XXX%SSSSSSSSSS#YYYYY")
/*%<
 * Minimum size of array to pass to isc_sockaddr_format().
 */

#endif /* ISC_SOCKADDR_H */

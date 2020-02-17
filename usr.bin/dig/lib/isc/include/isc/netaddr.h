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

/* $Id: netaddr.h,v 1.4 2020/02/17 18:58:39 jung Exp $ */

#ifndef ISC_NETADDR_H
#define ISC_NETADDR_H 1

/*! \file isc/netaddr.h */

#include <isc/net.h>
#include <isc/types.h>

#include <sys/types.h>
#include <sys/un.h>

struct isc_netaddr {
	unsigned int family;
	union {
		struct in_addr in;
		struct in6_addr in6;
		char un[sizeof(((struct sockaddr_un *)0)->sun_path)];
	} type;
	uint32_t zone;
};

/*%<
 * Compare the 'prefixlen' most significant bits of the network
 * addresses 'a' and 'b'.  If 'b''s scope is zero then 'a''s scope is
 * ignored.  Return #ISC_TRUE if they are equal, #ISC_FALSE if not.
 */

isc_result_t
isc_netaddr_totext(const isc_netaddr_t *netaddr, isc_buffer_t *target);
/*%<
 * Append a text representation of 'sockaddr' to the buffer 'target'.
 * The text is NOT null terminated.  Handles IPv4 and IPv6 addresses.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOSPACE	The text or the null termination did not fit.
 *\li	#ISC_R_FAILURE	Unspecified failure
 */

void
isc_netaddr_format(const isc_netaddr_t *na, char *array, unsigned int size);
/*%<
 * Format a human-readable representation of the network address '*na'
 * into the character array 'array', which is of size 'size'.
 * The resulting string is guaranteed to be null-terminated.
 */

#define ISC_NETADDR_FORMATSIZE \
	sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:XXX.XXX.XXX.XXX%SSSSSSSSSS")
/*%<
 * Minimum size of array to pass to isc_netaddr_format().
 */

void
isc_netaddr_fromsockaddr(isc_netaddr_t *netaddr, const isc_sockaddr_t *source);

isc_boolean_t
isc_netaddr_ismulticast(isc_netaddr_t *na);
/*%<
 * Returns ISC_TRUE if the address is a multicast address.
 */

isc_boolean_t
isc_netaddr_islinklocal(isc_netaddr_t *na);
/*%<
 * Returns #ISC_TRUE if the address is a link local address.
 */

isc_boolean_t
isc_netaddr_issitelocal(isc_netaddr_t *na);
/*%<
 * Returns #ISC_TRUE if the address is a site local address.
 */

#endif /* ISC_NETADDR_H */

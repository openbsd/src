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

/* $Id: sockaddr.c,v 1.16 2022/01/17 18:19:51 naddy Exp $ */

/*! \file */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>

#include <isc/buffer.h>

#include <isc/region.h>
#include <isc/sockaddr.h>
#include <string.h>
#include <isc/util.h>

int
isc_sockaddr_equal(const struct sockaddr_storage *a, const struct sockaddr_storage *b) {
	return (isc_sockaddr_compare(a, b, ISC_SOCKADDR_CMPADDR|
					   ISC_SOCKADDR_CMPPORT|
					   ISC_SOCKADDR_CMPSCOPE));
}

int
isc_sockaddr_eqaddr(const struct sockaddr_storage *a, const struct sockaddr_storage *b) {
	return (isc_sockaddr_compare(a, b, ISC_SOCKADDR_CMPADDR|
					   ISC_SOCKADDR_CMPSCOPE));
}

int
isc_sockaddr_compare(const struct sockaddr_storage *a, const struct sockaddr_storage *b,
		     unsigned int flags)
{
	struct sockaddr_in	*sin_a, *sin_b;
	struct sockaddr_in6	*sin6_a, *sin6_b;

	REQUIRE(a != NULL && b != NULL);

	if (a->ss_len != b->ss_len)
		return (0);

	/*
	 * We don't just memcmp because the sin_zero field isn't always
	 * zero.
	 */

	if (a->ss_family != b->ss_family)
		return (0);
	switch (a->ss_family) {
	case AF_INET:
		sin_a = (struct sockaddr_in *) a;
		sin_b = (struct sockaddr_in *) b;
		if ((flags & ISC_SOCKADDR_CMPADDR) != 0 &&
		    memcmp(&sin_a->sin_addr, &sin_b->sin_addr,
			   sizeof(sin_a->sin_addr)) != 0)
			return (0);
		if ((flags & ISC_SOCKADDR_CMPPORT) != 0 &&
		    sin_a->sin_port != sin_b->sin_port)
			return (0);
		break;
	case AF_INET6:
		sin6_a = (struct sockaddr_in6 *) a;
		sin6_b = (struct sockaddr_in6 *) b;

		if ((flags & ISC_SOCKADDR_CMPADDR) != 0 &&
		    memcmp(&sin6_a->sin6_addr, &sin6_b->sin6_addr,
			   sizeof(sin6_a->sin6_addr)) != 0)
			return (0);
		/*
		 * If ISC_SOCKADDR_CMPSCOPEZERO is set then don't return
		 * 0 if one of the scopes in zero.
		 */
		if ((flags & ISC_SOCKADDR_CMPSCOPE) != 0 &&
		    sin6_a->sin6_scope_id != sin6_b->sin6_scope_id &&
		    ((flags & ISC_SOCKADDR_CMPSCOPEZERO) == 0 ||
		      (sin6_a->sin6_scope_id != 0 &&
		       sin6_b->sin6_scope_id != 0)))
			return (0);
		if ((flags & ISC_SOCKADDR_CMPPORT) != 0 &&
		    sin6_a->sin6_port != sin6_b->sin6_port)
			return (0);
		break;
	default:
		if (memcmp(a, b, a->ss_len) != 0)
			return (0);
	}
	return (1);
}

isc_result_t
isc_sockaddr_totext(const struct sockaddr_storage *sockaddr, isc_buffer_t *target) {
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	char pbuf[sizeof("65000")];
	unsigned int plen;
	isc_region_t avail;
	char tmp[NI_MAXHOST];

	REQUIRE(sockaddr != NULL);

	/*
	 * Do the port first, giving us the opportunity to check for
	 * unsupported address families.
	 */
	switch (sockaddr->ss_family) {
	case AF_INET:
		sin = (struct sockaddr_in *)sockaddr;
		snprintf(pbuf, sizeof(pbuf), "%u", ntohs(sin->sin_port));
		break;
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)sockaddr;
		snprintf(pbuf, sizeof(pbuf), "%u", ntohs(sin6->sin6_port));
		break;
	default:
		return (ISC_R_FAILURE);
	}

	plen = strlen(pbuf);
	INSIST(plen < sizeof(pbuf));

	if (getnameinfo((struct sockaddr *)sockaddr, sockaddr->ss_len,
	    tmp, sizeof(tmp), NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV) != 0)
		return (ISC_R_FAILURE);
	if (strlen(tmp) > isc_buffer_availablelength(target))
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(target, tmp, strlen(tmp));

	if (1 + plen + 1 > isc_buffer_availablelength(target))
		return (ISC_R_NOSPACE);

	isc_buffer_putmem(target, (const unsigned char *)"#", 1);
	isc_buffer_putmem(target, (const unsigned char *)pbuf, plen);

	/*
	 * Null terminate after used region.
	 */
	isc_buffer_availableregion(target, &avail);
	INSIST(avail.length >= 1);
	avail.base[0] = '\0';

	return (ISC_R_SUCCESS);
}

void
isc_sockaddr_format(const struct sockaddr_storage *sa, char *array, unsigned int size) {
	isc_result_t result;
	isc_buffer_t buf;

	if (size == 0U)
		return;

	isc_buffer_init(&buf, array, size);
	result = isc_sockaddr_totext(sa, &buf);
	if (result != ISC_R_SUCCESS) {
		snprintf(array, size, "<unknown address, family %u>",
			 sa->ss_family);
		array[size - 1] = '\0';
	}
}

void
isc_sockaddr_any(struct sockaddr_storage *sockaddr)
{
	struct sockaddr_in *sin = (struct sockaddr_in *) sockaddr;
	memset(sockaddr, 0, sizeof(*sockaddr));
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	sin->sin_addr.s_addr = INADDR_ANY;
	sin->sin_port = 0;
}

void
isc_sockaddr_any6(struct sockaddr_storage *sockaddr)
{
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) sockaddr;
	memset(sockaddr, 0, sizeof(*sockaddr));
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(*sin6);
	sin6->sin6_addr = in6addr_any;
	sin6->sin6_port = 0;
}

void
isc_sockaddr_anyofpf(struct sockaddr_storage *sockaddr, int pf) {
     switch (pf) {
     case AF_INET:
	     isc_sockaddr_any(sockaddr);
	     break;
     case AF_INET6:
	     isc_sockaddr_any6(sockaddr);
	     break;
     default:
	     INSIST(0);
     }
}

int
isc_sockaddr_pf(const struct sockaddr_storage *sockaddr) {

	/*
	 * Get the protocol family of 'sockaddr'.
	 */

	return (sockaddr->ss_family);
}

in_port_t
isc_sockaddr_getport(const struct sockaddr_storage *sockaddr) {
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;

	switch (sockaddr->ss_family) {
	case AF_INET:
		sin = (struct sockaddr_in *)sockaddr;
		return (ntohs(sin->sin_port));
		break;
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)sockaddr;
		return (ntohs(sin6->sin6_port));
		break;
	default:
		FATAL_ERROR(__FILE__, __LINE__,
			    "unknown address family: %d",
			    (int)sockaddr->ss_family);
	}
}

int
isc_sockaddr_ismulticast(const struct sockaddr_storage *sockaddr) {
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;

	switch (sockaddr->ss_family) {
	case AF_INET:
		sin = (struct sockaddr_in *)sockaddr;
		return (IN_MULTICAST(sin->sin_addr.s_addr));
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)sockaddr;
		return (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr));
	default:
		return (0);
	}
}

int
isc_sockaddr_issitelocal(const struct sockaddr_storage *sockaddr) {
	struct sockaddr_in6 *sin6;
	if (sockaddr->ss_family == AF_INET6) {
		sin6 = (struct sockaddr_in6 *)sockaddr;
		return (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr));
	}
	return (0);
}

int
isc_sockaddr_islinklocal(const struct sockaddr_storage *sockaddr) {
	struct sockaddr_in6 *sin6;
	if (sockaddr->ss_family == AF_INET6) {
		sin6 = (struct sockaddr_in6 *)sockaddr;
		return (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr));
	}
	return (0);
}

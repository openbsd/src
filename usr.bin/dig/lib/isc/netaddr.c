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

/* $Id: netaddr.c,v 1.7 2020/02/25 05:00:43 jsg Exp $ */

/*! \file */

#include <stdio.h>

#include <isc/buffer.h>
#include <isc/net.h>
#include <isc/netaddr.h>

#include <isc/sockaddr.h>
#include <string.h>
#include <isc/util.h>

isc_result_t
isc_netaddr_totext(const isc_netaddr_t *netaddr, isc_buffer_t *target) {
	char abuf[sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255")];
	char zbuf[sizeof("%4294967295")];
	unsigned int alen;
	int zlen;
	const char *r;
	const void *type;

	REQUIRE(netaddr != NULL);

	switch (netaddr->family) {
	case AF_INET:
		type = &netaddr->type.in;
		break;
	case AF_INET6:
		type = &netaddr->type.in6;
		break;
	default:
		return (ISC_R_FAILURE);
	}
	r = inet_ntop(netaddr->family, type, abuf, sizeof(abuf));
	if (r == NULL)
		return (ISC_R_FAILURE);

	alen = strlen(abuf);
	INSIST(alen < sizeof(abuf));

	zlen = 0;
	if (netaddr->family == AF_INET6 && netaddr->zone != 0) {
		zlen = snprintf(zbuf, sizeof(zbuf), "%%%u", netaddr->zone);
		if (zlen < 0)
			return (ISC_R_FAILURE);
		INSIST((unsigned int)zlen < sizeof(zbuf));
	}

	if (alen + zlen > isc_buffer_availablelength(target))
		return (ISC_R_NOSPACE);

	isc_buffer_putmem(target, (unsigned char *)abuf, alen);
	isc_buffer_putmem(target, (unsigned char *)zbuf, zlen);

	return (ISC_R_SUCCESS);
}

void
isc_netaddr_format(const isc_netaddr_t *na, char *array, unsigned int size) {
	isc_result_t result;
	isc_buffer_t buf;

	isc_buffer_init(&buf, array, size);
	result = isc_netaddr_totext(na, &buf);

	if (size == 0)
		return;

	/*
	 * Null terminate.
	 */
	if (result == ISC_R_SUCCESS) {
		if (isc_buffer_availablelength(&buf) >= 1)
			isc_buffer_putuint8(&buf, 0);
		else
			result = ISC_R_NOSPACE;
	}

	if (result != ISC_R_SUCCESS) {
		snprintf(array, size, "<unknown address, family %u>",
			 na->family);
		array[size - 1] = '\0';
	}
}

void
isc_netaddr_fromsockaddr(isc_netaddr_t *t, const isc_sockaddr_t *s) {
	int family = s->type.sa.sa_family;
	t->family = family;
	switch (family) {
	case AF_INET:
		t->type.in = s->type.sin.sin_addr;
		t->zone = 0;
		break;
	case AF_INET6:
		memmove(&t->type.in6, &s->type.sin6.sin6_addr, 16);
		t->zone = s->type.sin6.sin6_scope_id;
		break;
	default:
		INSIST(0);
	}
}

isc_boolean_t
isc_netaddr_ismulticast(isc_netaddr_t *na) {
	switch (na->family) {
	case AF_INET:
		return (ISC_TF(ISC_IPADDR_ISMULTICAST(na->type.in.s_addr)));
	case AF_INET6:
		return (ISC_TF(IN6_IS_ADDR_MULTICAST(&na->type.in6)));
	default:
		return (ISC_FALSE);  /* XXXMLG ? */
	}
}

isc_boolean_t
isc_netaddr_islinklocal(isc_netaddr_t *na) {
	switch (na->family) {
	case AF_INET:
		return (ISC_FALSE);
	case AF_INET6:
		return (ISC_TF(IN6_IS_ADDR_LINKLOCAL(&na->type.in6)));
	default:
		return (ISC_FALSE);
	}
}

isc_boolean_t
isc_netaddr_issitelocal(isc_netaddr_t *na) {
	switch (na->family) {
	case AF_INET:
		return (ISC_FALSE);
	case AF_INET6:
		return (ISC_TF(IN6_IS_ADDR_SITELOCAL(&na->type.in6)));
	default:
		return (ISC_FALSE);
	}
}

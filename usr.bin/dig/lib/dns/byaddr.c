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

/* $Id: byaddr.c,v 1.2 2020/02/11 23:26:11 jsg Exp $ */

/*! \file */

#include <string.h>

#include <isc/buffer.h>
#include <isc/netaddr.h>
#include <isc/util.h>

#include <dns/byaddr.h>
#include <dns/name.h>

/*
 * XXXRTH  We could use a static event...
 */

static char hex_digits[] = {
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

isc_result_t
dns_byaddr_createptrname2(isc_netaddr_t *address, unsigned int options,
			  dns_name_t *name)
{
	char textname[128];
	unsigned char *bytes;
	int i;
	char *cp;
	isc_buffer_t buffer;
	unsigned int len;

	REQUIRE(address != NULL);

	/*
	 * We create the text representation and then convert to a
	 * dns_name_t.  This is not maximally efficient, but it keeps all
	 * of the knowledge of wire format in the dns_name_ routines.
	 */

	bytes = (unsigned char *)(&address->type);
	if (address->family == AF_INET) {
		(void)snprintf(textname, sizeof(textname),
			       "%u.%u.%u.%u.in-addr.arpa.",
			       (bytes[3] & 0xffU),
			       (bytes[2] & 0xffU),
			       (bytes[1] & 0xffU),
			       (bytes[0] & 0xffU));
	} else if (address->family == AF_INET6) {
		size_t remaining;

		cp = textname;
		for (i = 15; i >= 0; i--) {
			*cp++ = hex_digits[bytes[i] & 0x0f];
			*cp++ = '.';
			*cp++ = hex_digits[(bytes[i] >> 4) & 0x0f];
			*cp++ = '.';
		}
		remaining = sizeof(textname) - (cp - textname);
		if ((options & DNS_BYADDROPT_IPV6INT) != 0) {
			strlcpy(cp, "ip6.int.", remaining);
		} else {
			strlcpy(cp, "ip6.arpa.", remaining);
		}
	} else
		return (ISC_R_NOTIMPLEMENTED);

	len = (unsigned int)strlen(textname);
	isc_buffer_init(&buffer, textname, len);
	isc_buffer_add(&buffer, len);
	return (dns_name_fromtext(name, &buffer, dns_rootname, 0, NULL));
}

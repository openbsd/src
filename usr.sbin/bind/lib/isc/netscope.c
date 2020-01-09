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

/*! \file */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] =
	"$Id: netscope.c,v 1.5 2020/01/09 18:17:19 florian Exp $";
#endif /* LIBC_SCCS and not lint */

#include <config.h>

#include <isc/string.h>
#include <isc/net.h>
#include <isc/netscope.h>
#include <isc/result.h>

isc_result_t
isc_netscope_pton(int af, char *scopename, void *addr, uint32_t *zoneid) {
	char *ep;
	unsigned int ifid;
	struct in6_addr *in6;
	uint32_t zone;
	uint64_t llz;

	/* at this moment, we only support AF_INET6 */
	if (af != AF_INET6)
		return (ISC_R_FAILURE);

	/*
	 * Basically, "names" are more stable than numeric IDs in terms of
	 * renumbering, and are more preferred.  However, since there is no
	 * standard naming convention and APIs to deal with the names.  Thus,
	 * we only handle the case of link-local addresses, for which we use
	 * interface names as link names, assuming one to one mapping between
	 * interfaces and links.
	 */
	in6 = (struct in6_addr *)addr;
	if (IN6_IS_ADDR_LINKLOCAL(in6) &&
	    (ifid = if_nametoindex((const char *)scopename)) != 0)
		zone = (uint32_t)ifid;
	else {
		llz = isc_string_touint64(scopename, &ep, 10);
		if (ep == scopename)
			return (ISC_R_FAILURE);

		/* check overflow */
		zone = (uint32_t)(llz & 0xffffffffUL);
		if (zone != llz)
			return (ISC_R_FAILURE);
	}

	*zoneid = zone;
	return (ISC_R_SUCCESS);
}

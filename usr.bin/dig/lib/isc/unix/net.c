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

/* $Id: net.c,v 1.1 2020/02/07 09:58:54 florian Exp $ */

#include <isc/net.h>
#include <isc/result.h>

static isc_result_t	ipv4_result = ISC_R_SUCCESS;
static isc_result_t	ipv6_result = ISC_R_SUCCESS;
static unsigned int	dscp_result =
		ISC_NET_DSCPSETV4 | ISC_NET_DSCPRECVV4 | ISC_NET_DSCPPKTV4 |
		ISC_NET_DSCPSETV6 | ISC_NET_DSCPRECVV6 | ISC_NET_DSCPPKTV6;

unsigned int
isc_net_probedscp(void) {
	return (dscp_result);
}

void
isc_net_disableipv4(void) {
	if (ipv4_result == ISC_R_SUCCESS)
		ipv4_result = ISC_R_DISABLED;
}

void
isc_net_disableipv6(void) {
	if (ipv6_result == ISC_R_SUCCESS)
		ipv6_result = ISC_R_DISABLED;
}

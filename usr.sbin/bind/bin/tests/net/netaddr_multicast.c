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

/* $ISC: netaddr_multicast.c,v 1.8 2001/01/09 21:42:07 bwelling Exp $ */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>

#include <isc/net.h>
#include <isc/netaddr.h>
#include <isc/string.h>
#include <isc/types.h>
#include <isc/util.h>

#include "driver.h"

TESTDECL(netaddr_multicast);

typedef struct {
	int family;
	const char *addr;
	isc_boolean_t is_multicast;
} t_addr_t;

static t_addr_t addrs[] = {
	{ AF_INET, "1.2.3.4", ISC_FALSE },
	{ AF_INET, "4.3.2.1", ISC_FALSE },
	{ AF_INET, "224.1.1.1", ISC_TRUE },
	{ AF_INET, "1.1.1.244", ISC_FALSE },
	{ AF_INET6, "::1", ISC_FALSE },
	{ AF_INET6, "ff02::1", ISC_TRUE }
};
#define NADDRS (sizeof(addrs) / sizeof(t_addr_t))

static isc_result_t to_netaddr(t_addr_t *, isc_netaddr_t *);

static isc_result_t
to_netaddr(t_addr_t *addr, isc_netaddr_t *na) {
	int r;
	struct in_addr in;
	struct in6_addr in6;

	switch (addr->family) {
	case AF_INET:
		r = inet_pton(AF_INET, addr->addr, (unsigned char *)&in);
		if (r != 1)
			return (ISC_R_FAILURE);
		isc_netaddr_fromin(na, &in);
		break;
	case AF_INET6:
		r = inet_pton(AF_INET6, addr->addr, (unsigned char *)&in6);
		if (r != 1)
			return (ISC_R_FAILURE);
		isc_netaddr_fromin6(na, &in6);
		break;
	default:
		return (ISC_R_UNEXPECTED);
	}

	return (ISC_R_SUCCESS);
}

test_result_t
netaddr_multicast(void) {
	isc_netaddr_t na;
	unsigned int n_fail;
	t_addr_t *addr;
	unsigned int i;
	isc_result_t result;
	isc_boolean_t tf;

	n_fail = 0;
	for (i = 0 ; i < NADDRS ; i++) {
		addr = &addrs[i];
		result = to_netaddr(addr, &na);
		if (result != ISC_R_SUCCESS) {
			printf("I:to_netaddr() returned %s on item %u\n",
			       isc_result_totext(result), i);
			return (UNKNOWN);
		}
		tf = isc_netaddr_ismulticast(&na);
		if (tf == addr->is_multicast) {
			printf("I:%s is%s multicast (PASSED)\n",
			       (addr->addr), (tf ? "" : " not"));
		} else {
			printf("I:%s is%s multicast (FAILED)\n",
			       (addr->addr), (tf ? "" : " not"));
			n_fail++;
		}
	}

	if (n_fail > 0)
		return (FAILED);

	return (PASSED);
}

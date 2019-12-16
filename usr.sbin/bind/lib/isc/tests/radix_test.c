/*
 * Copyright (C) 2014  Internet Systems Consortium, Inc. ("ISC")
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

#include <config.h>

#include <isc/mem.h>
#include <isc/netaddr.h>
#include <isc/radix.h>
#include <isc/result.h>
#include <isc/util.h>

#include <atf-c.h>

#include <stdlib.h>
#include <stdint.h>

#include "isctest.h"

ATF_TC(isc_radix_search);
ATF_TC_HEAD(isc_radix_search, tc) {
	atf_tc_set_md_var(tc, "descr", "test radix seaching");
}
ATF_TC_BODY(isc_radix_search, tc) {
	isc_radix_tree_t *radix = NULL;
	isc_radix_node_t *node;
	isc_prefix_t prefix;
	isc_result_t result;
	struct in_addr in_addr;
	isc_netaddr_t netaddr;

	UNUSED(tc);

	result = isc_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = isc_radix_create(mctx, &radix, 32);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	in_addr.s_addr = inet_addr("3.3.3.0");
	isc_netaddr_fromin(&netaddr, &in_addr);
	NETADDR_TO_PREFIX_T(&netaddr, prefix, 24);

	node = NULL;
	result = isc_radix_insert(radix, &node, NULL, &prefix);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	node->data[0] = (void *)1;
	isc_refcount_destroy(&prefix.refcount);

	in_addr.s_addr = inet_addr("3.3.0.0");
	isc_netaddr_fromin(&netaddr, &in_addr);
	NETADDR_TO_PREFIX_T(&netaddr, prefix, 16);

	node = NULL;
	result = isc_radix_insert(radix, &node, NULL, &prefix);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	node->data[0] = (void *)2;
	isc_refcount_destroy(&prefix.refcount);

	in_addr.s_addr = inet_addr("3.3.3.3");
	isc_netaddr_fromin(&netaddr, &in_addr);
	NETADDR_TO_PREFIX_T(&netaddr, prefix, 22);

	node = NULL;
	result = isc_radix_search(radix, &node, &prefix);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	ATF_REQUIRE_EQ(node->data[0], (void *)2);

	isc_refcount_destroy(&prefix.refcount);

	isc_radix_destroy(radix, NULL);

	isc_test_end();
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, isc_radix_search);

	return (atf_no_error());
}

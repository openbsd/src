/*
 * Copyright (C) 2016  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: acl_test.c,v 1.1 2019/12/16 16:31:34 deraadt Exp $ */

/*! \file */

#include <config.h>

#include <atf-c.h>

#include <stdio.h>
#include <unistd.h>

#include <isc/print.h>

#include <dns/acl.h>
#include "dnstest.h"

/*
 * Helper functions
 */

#define	BUFLEN		255
#define	BIGBUFLEN	(70 * 1024)
#define TEST_ORIGIN	"test"

ATF_TC(dns_acl_isinsecure);
ATF_TC_HEAD(dns_acl_isinsecure, tc) {
	atf_tc_set_md_var(tc, "descr", "test that dns_acl_isinsecure works");
}
ATF_TC_BODY(dns_acl_isinsecure, tc) {
	isc_result_t result;
	dns_acl_t *any = NULL;
	dns_acl_t *none = NULL;
	dns_acl_t *notnone = NULL;
	dns_acl_t *notany = NULL;

	dns_acl_t *pos4pos6 = NULL;
	dns_acl_t *notpos4pos6 = NULL;
	dns_acl_t *neg4pos6 = NULL;
	dns_acl_t *notneg4pos6 = NULL;
	dns_acl_t *pos4neg6 = NULL;
	dns_acl_t *notpos4neg6 = NULL;
	dns_acl_t *neg4neg6 = NULL;
	dns_acl_t *notneg4neg6 = NULL;

	dns_acl_t *loop4 = NULL;
	dns_acl_t *notloop4 = NULL;

	dns_acl_t *loop6 = NULL;
	dns_acl_t *notloop6 = NULL;

	dns_acl_t *loop4pos6 = NULL;
	dns_acl_t *notloop4pos6 = NULL;
	dns_acl_t *loop4neg6 = NULL;
	dns_acl_t *notloop4neg6 = NULL;

	struct in_addr inaddr;
	isc_netaddr_t addr;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_any(mctx, &any);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_none(mctx, &none);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_create(mctx, 1, &notnone);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_create(mctx, 1, &notany);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_merge(notnone, none, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_merge(notany, any, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	ATF_CHECK(dns_acl_isinsecure(any));		/* any; */
	ATF_CHECK(!dns_acl_isinsecure(none));		/* none; */
	ATF_CHECK(!dns_acl_isinsecure(notany));		/* !any; */
	ATF_CHECK(!dns_acl_isinsecure(notnone));	/* !none; */

	dns_acl_detach(&any);
	dns_acl_detach(&none);
	dns_acl_detach(&notany);
	dns_acl_detach(&notnone);

	result = dns_acl_create(mctx, 1, &pos4pos6);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_create(mctx, 1, &notpos4pos6);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_create(mctx, 1, &neg4pos6);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_create(mctx, 1, &notneg4pos6);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_create(mctx, 1, &pos4neg6);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_create(mctx, 1, &notpos4neg6);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_create(mctx, 1, &neg4neg6);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_create(mctx, 1, &notneg4neg6);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	inaddr.s_addr = htonl(0x0a000000);		/* 10.0.0.0 */
	isc_netaddr_fromin(&addr, &inaddr);
	result = dns_iptable_addprefix(pos4pos6->iptable, &addr, 8, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	addr.family = AF_INET6;				/* 0a00:: */
	result = dns_iptable_addprefix(pos4pos6->iptable, &addr, 8, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_merge(notpos4pos6, pos4pos6, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	inaddr.s_addr = htonl(0x0a000000);		/* !10.0.0.0/8 */
	isc_netaddr_fromin(&addr, &inaddr);
	result = dns_iptable_addprefix(neg4pos6->iptable, &addr, 8, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	addr.family = AF_INET6;				/* 0a00::/8 */
	result = dns_iptable_addprefix(neg4pos6->iptable, &addr, 8, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_merge(notneg4pos6, neg4pos6, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	inaddr.s_addr = htonl(0x0a000000);		/* 10.0.0.0/8 */
	isc_netaddr_fromin(&addr, &inaddr);
	result = dns_iptable_addprefix(pos4neg6->iptable, &addr, 8, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	addr.family = AF_INET6;				/* !0a00::/8 */
	result = dns_iptable_addprefix(pos4neg6->iptable, &addr, 8, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_merge(notpos4neg6, pos4neg6, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	inaddr.s_addr = htonl(0x0a000000);		/* !10.0.0.0/8 */
	isc_netaddr_fromin(&addr, &inaddr);
	result = dns_iptable_addprefix(neg4neg6->iptable, &addr, 8, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	addr.family = AF_INET6;				/* !0a00::/8 */
	result = dns_iptable_addprefix(neg4neg6->iptable, &addr, 8, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_merge(notneg4neg6, neg4neg6, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	ATF_CHECK(dns_acl_isinsecure(pos4pos6));
	ATF_CHECK(!dns_acl_isinsecure(notpos4pos6));
	ATF_CHECK(dns_acl_isinsecure(neg4pos6));
	ATF_CHECK(!dns_acl_isinsecure(notneg4pos6));
	ATF_CHECK(dns_acl_isinsecure(pos4neg6));
	ATF_CHECK(!dns_acl_isinsecure(notpos4neg6));
	ATF_CHECK(!dns_acl_isinsecure(neg4neg6));
	ATF_CHECK(!dns_acl_isinsecure(notneg4neg6));

	dns_acl_detach(&pos4pos6);
	dns_acl_detach(&notpos4pos6);
	dns_acl_detach(&neg4pos6);
	dns_acl_detach(&notneg4pos6);
	dns_acl_detach(&pos4neg6);
	dns_acl_detach(&notpos4neg6);
	dns_acl_detach(&neg4neg6);
	dns_acl_detach(&notneg4neg6);

	result = dns_acl_create(mctx, 1, &loop4);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_create(mctx, 1, &notloop4);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_create(mctx, 1, &loop6);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_create(mctx, 1, &notloop6);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	inaddr.s_addr = htonl(0x7f000001);		/* 127.0.0.1 */
	isc_netaddr_fromin(&addr, &inaddr);
	result = dns_iptable_addprefix(loop4->iptable, &addr, 32, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_merge(notloop4, loop4, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	isc_netaddr_fromin6(&addr, &in6addr_loopback);	/* ::1 */
	result = dns_iptable_addprefix(loop6->iptable, &addr, 128, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_merge(notloop6, loop6, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	ATF_CHECK(!dns_acl_isinsecure(loop4));
	ATF_CHECK(!dns_acl_isinsecure(notloop4));
	ATF_CHECK(!dns_acl_isinsecure(loop6));
	ATF_CHECK(!dns_acl_isinsecure(notloop6));

	dns_acl_detach(&loop4);
	dns_acl_detach(&notloop4);
	dns_acl_detach(&loop6);
	dns_acl_detach(&notloop6);

	result = dns_acl_create(mctx, 1, &loop4pos6);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_create(mctx, 1, &notloop4pos6);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_create(mctx, 1, &loop4neg6);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_create(mctx, 1, &notloop4neg6);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	inaddr.s_addr = htonl(0x7f000001);		/* 127.0.0.1 */
	isc_netaddr_fromin(&addr, &inaddr);
	result = dns_iptable_addprefix(loop4pos6->iptable, &addr, 32, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	addr.family = AF_INET6;				/* f700:0001::/32 */
	result = dns_iptable_addprefix(loop4pos6->iptable, &addr, 32, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_merge(notloop4pos6, loop4pos6, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	inaddr.s_addr = htonl(0x7f000001);		/* 127.0.0.1 */
	isc_netaddr_fromin(&addr, &inaddr);
	result = dns_iptable_addprefix(loop4neg6->iptable, &addr, 32, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	addr.family = AF_INET6;				/* !f700:0001::/32 */
	result = dns_iptable_addprefix(loop4neg6->iptable, &addr, 32,
					ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_acl_merge(notloop4neg6, loop4neg6, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	ATF_CHECK(dns_acl_isinsecure(loop4pos6));
	ATF_CHECK(!dns_acl_isinsecure(notloop4pos6));
	ATF_CHECK(!dns_acl_isinsecure(loop4neg6));
	ATF_CHECK(!dns_acl_isinsecure(notloop4neg6));

	dns_acl_detach(&loop4pos6);
	dns_acl_detach(&notloop4pos6);
	dns_acl_detach(&loop4neg6);
	dns_acl_detach(&notloop4neg6);

	dns_test_end();
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, dns_acl_isinsecure);
	return (atf_no_error());
}

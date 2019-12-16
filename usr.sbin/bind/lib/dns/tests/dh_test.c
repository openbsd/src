/*
 * Copyright (C) 2014, 2016  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: dh_test.c,v 1.1 2019/12/16 16:31:34 deraadt Exp $ */

/* ! \file */

#include <config.h>

#include <atf-c.h>

#include <unistd.h>

#include <isc/util.h>
#include <isc/string.h>

#include <pk11/site.h>

#include <dns/name.h>
#include <dst/result.h>

#include "../dst_internal.h"

#include "dnstest.h"

#if defined(OPENSSL) && !defined(PK11_DH_DISABLE)

ATF_TC(isc_dh_computesecret);
ATF_TC_HEAD(isc_dh_computesecret, tc) {
	atf_tc_set_md_var(tc, "descr", "OpenSSL DH_compute_key() failure");
}
ATF_TC_BODY(isc_dh_computesecret, tc) {
	dst_key_t *key = NULL;
	isc_buffer_t buf;
	unsigned char array[1024];
	isc_result_t ret;
	dns_fixedname_t fname;
	dns_name_t *name;

	UNUSED(tc);

	ret = dns_test_begin(NULL, ISC_FALSE);
	ATF_REQUIRE_EQ(ret, ISC_R_SUCCESS);

	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	isc_buffer_constinit(&buf, "dh.", 3);
	isc_buffer_add(&buf, 3);
	ret = dns_name_fromtext(name, &buf, NULL, 0, NULL);
	ATF_REQUIRE_EQ(ret, ISC_R_SUCCESS);

	ret = dst_key_fromfile(name, 18602, DST_ALG_DH,
			       DST_TYPE_PUBLIC | DST_TYPE_KEY,
			       "./", mctx, &key);
	ATF_REQUIRE_EQ(ret, ISC_R_SUCCESS);

	isc_buffer_init(&buf, array, sizeof(array));
	ret = dst_key_computesecret(key, key, &buf);
	ATF_REQUIRE_EQ(ret, DST_R_NOTPRIVATEKEY);
	ret = key->func->computesecret(key, key, &buf);
	ATF_REQUIRE_EQ(ret, DST_R_COMPUTESECRETFAILURE);

	dst_key_free(&key);
	dns_test_end();
}
#else
ATF_TC(untested);
ATF_TC_HEAD(untested, tc) {
	atf_tc_set_md_var(tc, "descr", "skipping OpenSSL DH test");
}
ATF_TC_BODY(untested, tc) {
	UNUSED(tc);
	atf_tc_skip("OpenSSL DH not compiled in");
}
#endif
/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
#if defined(OPENSSL) && !defined(PK11_DH_DISABLE)
	ATF_TP_ADD_TC(tp, isc_dh_computesecret);
#else
	ATF_TP_ADD_TC(tp, untested);
#endif
	return (atf_no_error());
}

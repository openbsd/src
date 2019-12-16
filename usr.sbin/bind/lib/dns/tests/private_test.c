/*
 * Copyright (C) 2011, 2012  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: private_test.c,v 1.1 2019/12/16 16:31:34 deraadt Exp $ */

/*! \file */

#include <config.h>

#include <atf-c.h>

#include <unistd.h>

#include <isc/buffer.h>

#include <dns/nsec3.h>
#include <dns/private.h>
#include <dns/rdataclass.h>
#include <dns/rdatatype.h>

#include <dst/dst.h>

#include "dnstest.h"

static dns_rdatatype_t privatetype = 65534;

typedef struct {
	unsigned char alg;
	dns_keytag_t keyid;
	isc_boolean_t remove;
	isc_boolean_t complete;
} signing_testcase_t;

typedef struct {
	unsigned char hash;
	unsigned char flags;
	unsigned int iterations;
	unsigned long salt;
	isc_boolean_t remove;
	isc_boolean_t pending;
	isc_boolean_t nonsec;
} nsec3_testcase_t;

/*
 * Helper functions
 */
static void
make_signing(signing_testcase_t *testcase, dns_rdata_t *private,
	     unsigned char *buf, size_t len)
{
	dns_rdata_init(private);

	buf[0] = testcase->alg;
	buf[1] = (testcase->keyid & 0xff00) >> 8;
	buf[2] = (testcase->keyid & 0xff);
	buf[3] = testcase->remove;
	buf[4] = testcase->complete;
	private->data = buf;
	private->length = len;
	private->type = privatetype;
	private->rdclass = dns_rdataclass_in;
}

static void
make_nsec3(nsec3_testcase_t *testcase, dns_rdata_t *private,
	   unsigned char *pbuf)
{
	dns_rdata_nsec3param_t params;
	dns_rdata_t nsec3param = DNS_RDATA_INIT;
	unsigned char bufdata[BUFSIZ];
	isc_buffer_t buf;
	isc_uint32_t salt;
	unsigned char *sp;
	int slen = 4;

	/* for simplicity, we're using a maximum salt length of 4 */
	salt = htonl(testcase->salt);
	sp = (unsigned char *) &salt;
	while (*sp == '\0' && slen > 0) {
		slen--;
		sp++;
	}

	params.common.rdclass = dns_rdataclass_in;
	params.common.rdtype = dns_rdatatype_nsec3param;
	params.hash = testcase->hash;
	params.iterations = testcase->iterations;
	params.salt = sp;
	params.salt_length = slen;

	params.flags = testcase->flags;
	if (testcase->remove) {
		params.flags |= DNS_NSEC3FLAG_REMOVE;
		if (testcase->nonsec)
			params.flags |= DNS_NSEC3FLAG_NONSEC;
	} else {
		params.flags |= DNS_NSEC3FLAG_CREATE;
		if (testcase->pending)
			params.flags |= DNS_NSEC3FLAG_INITIAL;
	}

	isc_buffer_init(&buf, bufdata, sizeof(bufdata));
	dns_rdata_fromstruct(&nsec3param, dns_rdataclass_in,
			     dns_rdatatype_nsec3param, &params, &buf);

	dns_rdata_init(private);

	dns_nsec3param_toprivate(&nsec3param, private, privatetype,
				 pbuf, DNS_NSEC3PARAM_BUFFERSIZE + 1);
}

/*
 * Individual unit tests
 */
ATF_TC(private_signing_totext);
ATF_TC_HEAD(private_signing_totext, tc) {
	atf_tc_set_md_var(tc, "descr",
			  "convert private signing records to text");
}
ATF_TC_BODY(private_signing_totext, tc) {
	isc_result_t result;
	dns_rdata_t private;
	int i;

	signing_testcase_t testcases[] = {
		{ DST_ALG_RSASHA512, 12345, 0, 0 },
		{ DST_ALG_RSASHA256, 54321, 1, 0 },
		{ DST_ALG_NSEC3RSASHA1, 22222, 0, 1 },
		{ DST_ALG_RSASHA1, 33333, 1, 1 }
	};
	const char *results[] = {
		"Signing with key 12345/RSASHA512",
		"Removing signatures for key 54321/RSASHA256",
		"Done signing with key 22222/NSEC3RSASHA1",
		"Done removing signatures for key 33333/RSASHA1"
	};
	int ncases = 4;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	for (i = 0; i < ncases; i++) {
		unsigned char data[5];
		char output[BUFSIZ];
		isc_buffer_t buf;

		isc_buffer_init(&buf, output, sizeof(output));

		make_signing(&testcases[i], &private, data, sizeof(data));
		dns_private_totext(&private, &buf);
		ATF_CHECK_STREQ(output, results[i]);
	}

	dns_test_end();
}

ATF_TC(private_nsec3_totext);
ATF_TC_HEAD(private_nsec3_totext, tc) {
	atf_tc_set_md_var(tc, "descr", "convert private chain records to text");
}
ATF_TC_BODY(private_nsec3_totext, tc) {
	isc_result_t result;
	dns_rdata_t private;
	int i;

	nsec3_testcase_t testcases[] = {
		{ 1, 0, 1, 0xbeef, 0, 0, 0 },
		{ 1, 1, 10, 0xdadd, 0, 0, 0 },
		{ 1, 0, 20, 0xbead, 0, 1, 0 },
		{ 1, 0, 30, 0xdeaf, 1, 0, 0 },
		{ 1, 0, 100, 0xfeedabee, 1, 0, 1 },
	};
	const char *results[] = {
		"Creating NSEC3 chain 1 0 1 BEEF",
		"Creating NSEC3 chain 1 1 10 DADD",
		"Pending NSEC3 chain 1 0 20 BEAD",
		"Removing NSEC3 chain 1 0 30 DEAF / creating NSEC chain",
		"Removing NSEC3 chain 1 0 100 FEEDABEE"
	};
	int ncases = 5;

	UNUSED(tc);

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	for (i = 0; i < ncases; i++) {
		unsigned char data[DNS_NSEC3PARAM_BUFFERSIZE + 1];
		char output[BUFSIZ];
		isc_buffer_t buf;

		isc_buffer_init(&buf, output, sizeof(output));

		make_nsec3(&testcases[i], &private, data);
		dns_private_totext(&private, &buf);
		ATF_CHECK_STREQ(output, results[i]);
	}

	dns_test_end();
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, private_signing_totext);
	ATF_TP_ADD_TC(tp, private_nsec3_totext);
	return (atf_no_error());
}


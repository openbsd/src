/*
 * Copyright (C) 2017  Internet Systems Consortium, Inc. ("ISC")
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

/* ! \file */

#include <config.h>
#include <atf-c.h>

#include <isc/mem.h>
#include <isc/print.h>

#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/tsig.h>

#include "dnstest.h"

#ifdef HAVE_INTTYPES_H
#include <inttypes.h> /* uintptr_t */
#endif

static int debug = 0;

static isc_result_t
add_mac(dst_context_t *tsigctx, isc_buffer_t *buf) {
	dns_rdata_any_tsig_t tsig;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_buffer_t databuf;
	isc_region_t r;
	isc_result_t result;
	unsigned char tsigbuf[1024];

	isc_buffer_usedregion(buf, &r);
	dns_rdata_fromregion(&rdata, dns_rdataclass_any,
			     dns_rdatatype_tsig, &r);
	isc_buffer_init(&databuf, tsigbuf, sizeof(tsigbuf));
	CHECK(dns_rdata_tostruct(&rdata, &tsig, NULL));
	isc_buffer_putuint16(&databuf, tsig.siglen);
	isc_buffer_putmem(&databuf, tsig.signature, tsig.siglen);
	isc_buffer_usedregion(&databuf, &r);
	result = dst_context_adddata(tsigctx, &r);
	dns_rdata_freestruct(&tsig);
 cleanup:
	return (result);
}

static isc_result_t
add_tsig(dst_context_t *tsigctx, dns_tsigkey_t *key, isc_buffer_t *target) {
	dns_compress_t cctx;
	dns_rdata_any_tsig_t tsig;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdatalist_t rdatalist;
	dns_rdataset_t rdataset;
	isc_buffer_t *dynbuf = NULL;
	isc_buffer_t databuf;
	isc_buffer_t sigbuf;
	isc_region_t r;
	isc_result_t result = ISC_R_SUCCESS;
	isc_stdtime_t now;
	unsigned char tsigbuf[1024];
	unsigned int count;
	unsigned int sigsize;
	isc_boolean_t invalidate_ctx = ISC_FALSE;

	CHECK(dns_compress_init(&cctx, -1, mctx));
	invalidate_ctx = ISC_TRUE;

	memset(&tsig, 0, sizeof(tsig));
	       tsig.common.rdclass = dns_rdataclass_any;
	tsig.common.rdtype = dns_rdatatype_tsig;
	ISC_LINK_INIT(&tsig.common, link);
	dns_name_init(&tsig.algorithm, NULL);
	dns_name_clone(key->algorithm, &tsig.algorithm);

	isc_stdtime_get(&now);
	tsig.timesigned = now;
	tsig.fudge = DNS_TSIG_FUDGE;
	tsig.originalid = 50;
	tsig.error = dns_rcode_noerror;
	tsig.otherlen = 0;
	tsig.other = NULL;

	isc_buffer_init(&databuf, tsigbuf, sizeof(tsigbuf));
	isc_buffer_putuint48(&databuf, tsig.timesigned);
	isc_buffer_putuint16(&databuf, tsig.fudge);
	isc_buffer_usedregion(&databuf, &r);
	CHECK(dst_context_adddata(tsigctx, &r));

	CHECK(dst_key_sigsize(key->key, &sigsize));
	tsig.signature = (unsigned char *) isc_mem_get(mctx, sigsize);
	if (tsig.signature == NULL)
		CHECK(ISC_R_NOMEMORY);
	isc_buffer_init(&sigbuf, tsig.signature, sigsize);
	CHECK(dst_context_sign(tsigctx, &sigbuf));
	tsig.siglen = isc_buffer_usedlength(&sigbuf);

	CHECK(isc_buffer_allocate(mctx, &dynbuf, 512));
	CHECK(dns_rdata_fromstruct(&rdata, dns_rdataclass_any,
				   dns_rdatatype_tsig, &tsig, dynbuf));
	dns_rdatalist_init(&rdatalist);
	rdatalist.rdclass = dns_rdataclass_any;
	rdatalist.type = dns_rdatatype_tsig;
	ISC_LIST_APPEND(rdatalist.rdata, &rdata, link);
	dns_rdataset_init(&rdataset);
	CHECK(dns_rdatalist_tordataset(&rdatalist, &rdataset));
	CHECK(dns_rdataset_towire(&rdataset, &key->name, &cctx,
				  target, 0, &count));

	/*
	 * Fixup additional record count.
	 */
	((unsigned char*)target->base)[11]++;
	if (((unsigned char*)target->base)[11] == 0)
		((unsigned char*)target->base)[10]++;
 cleanup:
	if (tsig.signature != NULL)
		isc_mem_put(mctx, tsig.signature, sigsize);
	if (dynbuf != NULL)
		isc_buffer_free(&dynbuf);
	if (invalidate_ctx)
		dns_compress_invalidate(&cctx);

	return (result);
}

static void
printmessage(dns_message_t *msg) {
	isc_buffer_t b;
	char *buf = NULL;
	int len = 1024;
	isc_result_t result = ISC_R_SUCCESS;

	if (!debug)
		return;

	do {
		buf = isc_mem_get(mctx, len);
		if (buf == NULL) {
			result = ISC_R_NOMEMORY;
			break;
		}

		isc_buffer_init(&b, buf, len);
		result = dns_message_totext(msg, &dns_master_style_debug,
					    0, &b);
		if (result == ISC_R_NOSPACE) {
			isc_mem_put(mctx, buf, len);
			len *= 2;
		} else if (result == ISC_R_SUCCESS)
			printf("%.*s\n", (int) isc_buffer_usedlength(&b), buf);
	} while (result == ISC_R_NOSPACE);

	if (buf != NULL)
		isc_mem_put(mctx, buf, len);
}

static void
render(isc_buffer_t *buf, unsigned flags, dns_tsigkey_t *key,
       isc_buffer_t **tsigin, isc_buffer_t **tsigout,
       dst_context_t *tsigctx)
{
	dns_message_t *msg = NULL;
	dns_compress_t cctx;
	isc_result_t result;

	result = dns_message_create(mctx, DNS_MESSAGE_INTENTRENDER, &msg);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_message_create: %s",
			 dns_result_totext(result));

	msg->id = 50;
	msg->rcode = dns_rcode_noerror;
	msg->flags = flags;

	if (tsigin == tsigout)
		msg->tcp_continuation = 1;

	if (tsigctx == NULL) {
		result = dns_message_settsigkey(msg, key);
		ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
				 "dns_message_settsigkey: %s",
				 dns_result_totext(result));

		result = dns_message_setquerytsig(msg, *tsigin);
		ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
				 "dns_message_setquerytsig: %s",
				 dns_result_totext(result));
	}

	result = dns_compress_init(&cctx, -1, mctx);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_compress_init: %s",
			 dns_result_totext(result));

	result = dns_message_renderbegin(msg, &cctx, buf);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_message_renderbegin: %s",
			 dns_result_totext(result));

	result = dns_message_renderend(msg);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_message_renderend: %s",
			 dns_result_totext(result));

	if (tsigctx != NULL) {
		isc_region_t r;

		isc_buffer_usedregion(buf, &r);
		result = dst_context_adddata(tsigctx, &r);
	} else {
		if (tsigin == tsigout && *tsigin != NULL)
			isc_buffer_free(tsigin);

		result = dns_message_getquerytsig(msg, mctx, tsigout);
		ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
				 "dns_message_getquerytsig: %s",
				 dns_result_totext(result));
	}

	dns_compress_invalidate(&cctx);
	dns_message_destroy(&msg);
}

/*
 * Check that a simulated three message TCP sequence where the first
 * and last messages contain TSIGs but the intermediate message doesn't
 * correctly verifies.
 */
ATF_TC(tsig_tcp);
ATF_TC_HEAD(tsig_tcp, tc) {
	atf_tc_set_md_var(tc, "descr", "test tsig tcp-continuation validation");
}
ATF_TC_BODY(tsig_tcp, tc) {
	dns_name_t *tsigowner = NULL;
	dns_fixedname_t fkeyname;
	dns_message_t *msg = NULL;
	dns_name_t *keyname;
	dns_tsig_keyring_t *ring = NULL;
	dns_tsigkey_t *key = NULL;
	isc_buffer_t *buf = NULL;
	isc_buffer_t *querytsig = NULL;
	isc_buffer_t *tsigin = NULL;
	isc_buffer_t *tsigout = NULL;
	isc_result_t result;
	unsigned char secret[16] = { 0 };
	dst_context_t *tsigctx = NULL;
	dst_context_t *outctx = NULL;

	UNUSED(tc);

	result = dns_test_begin(stderr, ISC_FALSE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	/* isc_log_setdebuglevel(lctx, 99); */

	dns_fixedname_init(&fkeyname);
	keyname = dns_fixedname_name(&fkeyname);
	result = dns_name_fromstring(keyname, "test", 0, NULL);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_tsigkeyring_create(mctx, &ring);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = dns_tsigkey_create(keyname, dns_tsig_hmacsha256_name,
				    secret, sizeof(secret), ISC_FALSE,
				    NULL, 0, 0, mctx, ring, &key);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	/*
	 * Create request.
	 */
	result = isc_buffer_allocate(mctx, &buf, 65535);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	render(buf, 0, key, &tsigout, &querytsig, NULL);
	isc_buffer_free(&buf);

	/*
	 * Create response message 1.
	 */
	result = isc_buffer_allocate(mctx, &buf, 65535);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	render(buf, DNS_MESSAGEFLAG_QR, key, &querytsig, &tsigout, NULL);

	/*
	 * Process response message 1.
	 */
	result = dns_message_create(mctx, DNS_MESSAGE_INTENTPARSE, &msg);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_message_create: %s",
			 dns_result_totext(result));

	result = dns_message_settsigkey(msg, key);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_message_settsigkey: %s",
			 dns_result_totext(result));

	result = dns_message_parse(msg, buf, 0);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_message_parse: %s",
			 dns_result_totext(result));

	printmessage(msg);

	result = dns_message_setquerytsig(msg, querytsig);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_message_setquerytsig: %s",
			 dns_result_totext(result));

	result = dns_tsig_verify(buf, msg, NULL, NULL);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_tsig_verify: %s",
			 dns_result_totext(result));
	ATF_CHECK_EQ(msg->verified_sig, 1);
	ATF_CHECK_EQ(msg->tsigstatus, dns_rcode_noerror);

	/*
	 * Check that we have a TSIG in the first message.
	 */
	ATF_REQUIRE(dns_message_gettsig(msg, &tsigowner) != NULL);

	result = dns_message_getquerytsig(msg, mctx, &tsigin);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_message_getquerytsig: %s",
			 dns_result_totext(result));

	tsigctx = msg->tsigctx;
	msg->tsigctx = NULL;
	isc_buffer_free(&buf);
	dns_message_destroy(&msg);

	result = dst_context_create3(key->key, mctx, DNS_LOGCATEGORY_DNSSEC,
				     ISC_FALSE, &outctx);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	/*
	 * Start digesting.
	 */
	result = add_mac(outctx, tsigout);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	/*
	 * Create response message 2.
	 */
	result = isc_buffer_allocate(mctx, &buf, 65535);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	render(buf, DNS_MESSAGEFLAG_QR, key, &tsigout, &tsigout, outctx);

	/*
	 * Process response message 2.
	 */
	result = dns_message_create(mctx, DNS_MESSAGE_INTENTPARSE, &msg);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_message_create: %s",
			 dns_result_totext(result));

	msg->tcp_continuation = 1;
	msg->tsigctx = tsigctx;
	tsigctx = NULL;

	result = dns_message_settsigkey(msg, key);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_message_settsigkey: %s",
			 dns_result_totext(result));

	result = dns_message_parse(msg, buf, 0);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_message_parse: %s",
			 dns_result_totext(result));

	printmessage(msg);

	result = dns_message_setquerytsig(msg, tsigin);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_message_setquerytsig: %s",
			 dns_result_totext(result));

	result = dns_tsig_verify(buf, msg, NULL, NULL);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_tsig_verify: %s",
			 dns_result_totext(result));
	ATF_CHECK_EQ(msg->verified_sig, 1);
	ATF_CHECK_EQ(msg->tsigstatus, dns_rcode_noerror);

	/*
	 * Check that we don't have a TSIG in the second message.
	 */
	tsigowner = NULL;
	ATF_REQUIRE(dns_message_gettsig(msg, &tsigowner) == NULL);

	tsigctx = msg->tsigctx;
	msg->tsigctx = NULL;
	isc_buffer_free(&buf);
	dns_message_destroy(&msg);

	/*
	 * Create response message 3.
	 */
	result = isc_buffer_allocate(mctx, &buf, 65535);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	render(buf, DNS_MESSAGEFLAG_QR, key, &tsigout, &tsigout, outctx);

	result = add_tsig(outctx, key, buf);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "add_tsig: %s",
			 dns_result_totext(result));

	/*
	 * Process response message 3.
	 */
	result = dns_message_create(mctx, DNS_MESSAGE_INTENTPARSE, &msg);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_message_create: %s",
			 dns_result_totext(result));

	msg->tcp_continuation = 1;
	msg->tsigctx = tsigctx;
	tsigctx = NULL;

	result = dns_message_settsigkey(msg, key);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_message_settsigkey: %s",
			 dns_result_totext(result));

	result = dns_message_parse(msg, buf, 0);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_message_parse: %s",
			 dns_result_totext(result));

	printmessage(msg);

	/*
	 * Check that we had a TSIG in the third message.
	 */
	ATF_REQUIRE(dns_message_gettsig(msg, &tsigowner) != NULL);

	result = dns_message_setquerytsig(msg, tsigin);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_message_setquerytsig: %s",
			 dns_result_totext(result));

	result = dns_tsig_verify(buf, msg, NULL, NULL);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_tsig_verify: %s",
			 dns_result_totext(result));
	ATF_CHECK_EQ(msg->verified_sig, 1);
	ATF_CHECK_EQ(msg->tsigstatus, dns_rcode_noerror);

	if (tsigin != NULL)
		isc_buffer_free(&tsigin);

	result = dns_message_getquerytsig(msg, mctx, &tsigin);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS,
			 "dns_message_getquerytsig: %s",
			 dns_result_totext(result));

	isc_buffer_free(&buf);
	dns_message_destroy(&msg);

	if (outctx != NULL)
		dst_context_destroy(&outctx);
	if (querytsig != NULL)
		isc_buffer_free(&querytsig);
	if (tsigin != NULL)
		isc_buffer_free(&tsigin);
	if (tsigout != NULL)
		isc_buffer_free(&tsigout);
	if (buf != NULL)
		isc_buffer_free(&buf);
	if (msg != NULL)
		dns_message_destroy(&msg);
	if (key != NULL)
		dns_tsigkey_detach(&key);
	if (ring != NULL)
		dns_tsigkeyring_detach(&ring);
	dns_test_end();
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, tsig_tcp);
	return (atf_no_error());
}

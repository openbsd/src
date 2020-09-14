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

/* RFC 7477 */

#ifndef RDATA_GENERIC_CSYNC_62_C
#define RDATA_GENERIC_CSYNC_62_C

static inline isc_result_t
totext_csync(ARGS_TOTEXT) {
	unsigned long num;
	char buf[sizeof("0123456789")];	/* Also TYPE65535 */
	isc_region_t sr;

	REQUIRE(rdata->type == dns_rdatatype_csync);
	REQUIRE(rdata->length >= 6);

	UNUSED(tctx);

	dns_rdata_toregion(rdata, &sr);

	num = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);
	snprintf(buf, sizeof(buf), "%lu", num);
	RETERR(isc_str_tobuffer(buf, target));

	RETERR(isc_str_tobuffer(" ", target));

	num = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	snprintf(buf, sizeof(buf), "%lu", num);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Don't leave a trailing space when there's no typemap present.
	 */
	if (sr.length > 0) {
		RETERR(isc_str_tobuffer(" ", target));
	}
	return (typemap_totext(&sr, NULL, target));
}

static /* inline */ isc_result_t
fromwire_csync(ARGS_FROMWIRE) {
	isc_region_t sr;

	REQUIRE(type == dns_rdatatype_csync);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(options);
	UNUSED(dctx);

	/*
	 * Serial + Flags
	 */
	isc_buffer_activeregion(source, &sr);
	if (sr.length < 6)
		return (ISC_R_UNEXPECTEDEND);

	RETERR(isc_mem_tobuffer(target, sr.base, 6));
	isc_buffer_forward(source, 6);
	isc_region_consume(&sr, 6);

	RETERR(typemap_test(&sr, 1));

	RETERR(isc_mem_tobuffer(target, sr.base, sr.length));
	isc_buffer_forward(source, sr.length);
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_csync(ARGS_TOWIRE) {

	REQUIRE(rdata->type == dns_rdatatype_csync);
	REQUIRE(rdata->length >= 6);

	UNUSED(cctx);

	return (isc_mem_tobuffer(target, rdata->data, rdata->length));
}

#endif	/* RDATA_GENERIC_CSYNC_62_C */

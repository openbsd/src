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

#ifndef RDATA_GENERIC_DOA_259_C
#define RDATA_GENERIC_DOA_259_C

static inline isc_result_t
totext_doa(ARGS_TOTEXT) {
	char buf[sizeof("4294967295 ")];
	isc_region_t region;
	uint32_t n;

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_doa);
	REQUIRE(rdata->length != 0);

	UNUSED(tctx);

	dns_rdata_toregion(rdata, &region);

	/*
	 * DOA-ENTERPRISE
	 */
	n = uint32_fromregion(&region);
	isc_region_consume(&region, 4);
	snprintf(buf, sizeof(buf), "%u ", n);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * DOA-TYPE
	 */
	n = uint32_fromregion(&region);
	isc_region_consume(&region, 4);
	snprintf(buf, sizeof(buf), "%u ", n);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * DOA-LOCATION
	 */
	n = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	snprintf(buf, sizeof(buf), "%u ", n);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * DOA-MEDIA-TYPE
	 */
	RETERR(txt_totext(&region, 1, target));
	RETERR(isc_str_tobuffer(" ", target));

	/*
	 * DOA-DATA
	 */
	if (region.length == 0) {
		return (isc_str_tobuffer("-", target));
	} else {
		return (isc_base64_totext(&region, 60, "", target));
	}
}

static inline isc_result_t
fromwire_doa(ARGS_FROMWIRE) {
	isc_region_t region;

	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	REQUIRE(type == dns_rdatatype_doa);

	isc_buffer_activeregion(source, &region);
	/*
	 * DOA-MEDIA-TYPE may be an empty <character-string> (i.e.,
	 * comprising of just the length octet) and DOA-DATA can have
	 * zero length.
	 */
	if (region.length < 4 + 4 + 1 + 1) {
		return (ISC_R_UNEXPECTEDEND);
	}

	/*
	 * Check whether DOA-MEDIA-TYPE length is not malformed.
	 */
	if (region.base[9] > region.length - 10) {
		return (ISC_R_UNEXPECTEDEND);
	}

	isc_buffer_forward(source, region.length);
	return (isc_mem_tobuffer(target, region.base, region.length));
}

static inline isc_result_t
towire_doa(ARGS_TOWIRE) {
	isc_region_t region;

	UNUSED(cctx);

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_doa);
	REQUIRE(rdata->length != 0);

	dns_rdata_toregion(rdata, &region);
	return (isc_mem_tobuffer(target, region.base, region.length));
}

#endif	/* RDATA_GENERIC_DOA_259_C */

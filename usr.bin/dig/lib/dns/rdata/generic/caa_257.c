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

#ifndef GENERIC_CAA_257_C
#define GENERIC_CAA_257_C 1

static unsigned char const alphanumeric[256] = {
	/* 0x00-0x0f */ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
	/* 0x10-0x1f */ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
	/* 0x20-0x2f */ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
	/* 0x30-0x3f */ 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 0, 0, 0, 0, 0, 0,
	/* 0x40-0x4f */ 0, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
	/* 0x50-0x5f */ 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 0, 0, 0, 0, 0,
	/* 0x60-0x6f */ 0, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
	/* 0x70-0x7f */ 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 0, 0, 0, 0, 0,
	/* 0x80-0x8f */ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
	/* 0x90-0x9f */ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
	/* 0xa0-0xaf */ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
	/* 0xb0-0xbf */ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
	/* 0xc0-0xcf */ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
	/* 0xd0-0xdf */ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
	/* 0xe0-0xef */ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
	/* 0xf0-0xff */ 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
};

static inline isc_result_t
totext_caa(ARGS_TOTEXT) {
	isc_region_t region;
	uint8_t flags;
	char buf[256];

	UNUSED(tctx);

	REQUIRE(rdata->type == dns_rdatatype_caa);
	REQUIRE(rdata->length >= 3U);
	REQUIRE(rdata->data != NULL);

	dns_rdata_toregion(rdata, &region);

	/*
	 * Flags
	 */
	flags = uint8_consume_fromregion(&region);
	snprintf(buf, sizeof(buf), "%u ", flags);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Tag
	 */
	RETERR(txt_totext(&region, 0, target));
	RETERR(isc_str_tobuffer(" ", target));

	/*
	 * Value
	 */
	RETERR(multitxt_totext(&region, target));
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_caa(ARGS_FROMWIRE) {
	isc_region_t sr;
	unsigned int len, i;

	REQUIRE(type == dns_rdatatype_caa);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	/*
	 * Flags
	 */
	isc_buffer_activeregion(source, &sr);
	if (sr.length < 2)
		return (ISC_R_UNEXPECTEDEND);

	/*
	 * Flags, tag length
	 */
	RETERR(isc_mem_tobuffer(target, sr.base, 2));
	len = sr.base[1];
	isc_region_consume(&sr, 2);
	isc_buffer_forward(source, 2);

	/*
	 * Zero length tag fields are illegal.
	 */
	if (sr.length < len || len == 0)
		return (DNS_R_FORMERR);

	/* Check the Tag's value */
	for (i = 0; i < len; i++)
		if (!alphanumeric[sr.base[i]])
			return (DNS_R_FORMERR);
	/*
	 * Tag + Value
	 */
	isc_buffer_forward(source, sr.length);
	return (isc_mem_tobuffer(target, sr.base, sr.length));
}

static inline isc_result_t
towire_caa(ARGS_TOWIRE) {
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_caa);
	REQUIRE(rdata->length >= 3U);
	REQUIRE(rdata->data != NULL);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &region);
	return (isc_mem_tobuffer(target, region.base, region.length));
}

#endif /* GENERIC_CAA_257_C */

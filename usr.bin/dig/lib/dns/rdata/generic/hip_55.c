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

/* $Id: hip_55.c,v 1.15 2020/09/14 08:40:43 florian Exp $ */

/* reviewed: TBC */

/* RFC 5205 */

#ifndef RDATA_GENERIC_HIP_5_C
#define RDATA_GENERIC_HIP_5_C

static inline isc_result_t
totext_hip(ARGS_TOTEXT) {
	isc_region_t region;
	dns_name_t name;
	unsigned int length, key_len, hit_len;
	unsigned char algorithm;
	char buf[sizeof("225 ")];

	REQUIRE(rdata->type == dns_rdatatype_hip);
	REQUIRE(rdata->length != 0);

	dns_rdata_toregion(rdata, &region);

	hit_len = uint8_fromregion(&region);
	isc_region_consume(&region, 1);

	algorithm = uint8_fromregion(&region);
	isc_region_consume(&region, 1);

	key_len = uint16_fromregion(&region);
	isc_region_consume(&region, 2);

	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(isc_str_tobuffer("( ", target));

	/*
	 * Algorithm
	 */
	snprintf(buf, sizeof(buf), "%u ", algorithm);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * HIT.
	 */
	INSIST(hit_len < region.length);
	length = region.length;
	region.length = hit_len;
	RETERR(isc_hex_totext(&region, 1, "", target));
	region.length = length - hit_len;
	RETERR(isc_str_tobuffer(tctx->linebreak, target));

	/*
	 * Public KEY.
	 */
	INSIST(key_len <= region.length);
	length = region.length;
	region.length = key_len;
	RETERR(isc_base64_totext(&region, 1, "", target));
	region.length = length - key_len;
	RETERR(isc_str_tobuffer(tctx->linebreak, target));

	/*
	 * Rendezvous Servers.
	 */
	dns_name_init(&name, NULL);
	while (region.length > 0) {
		dns_name_fromregion(&name, &region);

		RETERR(dns_name_totext(&name, 0, target));
		isc_region_consume(&region, name.length);
		if (region.length > 0)
			RETERR(isc_str_tobuffer(tctx->linebreak, target));
	}
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(isc_str_tobuffer(" )", target));
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_hip(ARGS_FROMWIRE) {
	isc_region_t region, rr;
	dns_name_t name;
	uint8_t hit_len;
	uint16_t key_len;

	REQUIRE(type == dns_rdatatype_hip);

	UNUSED(type);
	UNUSED(rdclass);

	isc_buffer_activeregion(source, &region);
	if (region.length < 4U)
		return (DNS_R_FORMERR);

	rr = region;
	hit_len = uint8_fromregion(&region);
	if (hit_len == 0)
		return (DNS_R_FORMERR);
	isc_region_consume(&region, 2);  	/* hit length + algorithm */
	key_len = uint16_fromregion(&region);
	if (key_len == 0)
		return (DNS_R_FORMERR);
	isc_region_consume(&region, 2);
	if (region.length < (unsigned) (hit_len + key_len))
		return (DNS_R_FORMERR);

	RETERR(isc_mem_tobuffer(target, rr.base, 4 + hit_len + key_len));
	isc_buffer_forward(source, 4 + hit_len + key_len);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_NONE);
	while (isc_buffer_activelength(source) > 0) {
		dns_name_init(&name, NULL);
		RETERR(dns_name_fromwire(&name, source, dctx, options, target));
	}
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_hip(ARGS_TOWIRE) {
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_hip);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &region);
	return (isc_mem_tobuffer(target, region.base, region.length));
}
#endif	/* RDATA_GENERIC_HIP_5_C */

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

/* $Id: tkey_249.c,v 1.12 2020/02/26 18:47:59 florian Exp $ */

/*
 * Reviewed: Thu Mar 16 17:35:30 PST 2000 by halley.
 */

/* draft-ietf-dnsext-tkey-01.txt */

#ifndef RDATA_GENERIC_TKEY_249_C
#define RDATA_GENERIC_TKEY_249_C

static inline isc_result_t
totext_tkey(ARGS_TOTEXT) {
	isc_region_t sr, dr;
	char buf[sizeof("4294967295 ")];
	unsigned long n;
	dns_name_t name;
	dns_name_t prefix;
	isc_boolean_t sub;

	REQUIRE(rdata->type == dns_rdatatype_tkey);
	REQUIRE(rdata->length != 0);

	dns_rdata_toregion(rdata, &sr);

	/*
	 * Algorithm.
	 */
	dns_name_init(&name, NULL);
	dns_name_init(&prefix, NULL);
	dns_name_fromregion(&name, &sr);
	sub = name_prefix(&name, tctx->origin, &prefix);
	RETERR(dns_name_totext(&prefix, sub, target));
	RETERR(isc_str_tobuffer(" ", target));
	isc_region_consume(&sr, name_length(&name));

	/*
	 * Inception.
	 */
	n = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);
	snprintf(buf, sizeof(buf), "%lu ", n);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Expiration.
	 */
	n = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);
	snprintf(buf, sizeof(buf), "%lu ", n);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Mode.
	 */
	n = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	snprintf(buf, sizeof(buf), "%lu ", n);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Error.
	 */
	n = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	if (dns_tsigrcode_totext((dns_rcode_t)n, target) == ISC_R_SUCCESS)
		RETERR(isc_str_tobuffer(" ", target));
	else {
		snprintf(buf, sizeof(buf), "%lu ", n);
		RETERR(isc_str_tobuffer(buf, target));
	}

	/*
	 * Key Size.
	 */
	n = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	snprintf(buf, sizeof(buf), "%lu", n);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Key Data.
	 */
	REQUIRE(n <= sr.length);
	dr = sr;
	dr.length = n;
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(isc_str_tobuffer(" (", target));
	RETERR(isc_str_tobuffer(tctx->linebreak, target));
	if (tctx->width == 0)   /* No splitting */
		RETERR(isc_base64_totext(&dr, 60, "", target));
	else
		RETERR(isc_base64_totext(&dr, tctx->width - 2,
					 tctx->linebreak, target));
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(isc_str_tobuffer(" ) ", target));
	else
		RETERR(isc_str_tobuffer(" ", target));
	isc_region_consume(&sr, n);

	/*
	 * Other Size.
	 */
	n = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	snprintf(buf, sizeof(buf), "%lu", n);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Other Data.
	 */
	REQUIRE(n <= sr.length);
	if (n != 0U) {
	    dr = sr;
	    dr.length = n;
	    if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		    RETERR(isc_str_tobuffer(" (", target));
	    RETERR(isc_str_tobuffer(tctx->linebreak, target));
		if (tctx->width == 0)   /* No splitting */
			RETERR(isc_base64_totext(&dr, 60, "", target));
		else
			RETERR(isc_base64_totext(&dr, tctx->width - 2,
						 tctx->linebreak, target));
	    if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		    RETERR(isc_str_tobuffer(" )", target));
	}
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_tkey(ARGS_FROMWIRE) {
	isc_region_t sr;
	unsigned long n;
	dns_name_t name;

	REQUIRE(type == dns_rdatatype_tkey);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_NONE);

	/*
	 * Algorithm.
	 */
	dns_name_init(&name, NULL);
	RETERR(dns_name_fromwire(&name, source, dctx, options, target));

	/*
	 * Inception: 4
	 * Expiration: 4
	 * Mode: 2
	 * Error: 2
	 */
	isc_buffer_activeregion(source, &sr);
	if (sr.length < 12)
		return (ISC_R_UNEXPECTEDEND);
	RETERR(isc_mem_tobuffer(target, sr.base, 12));
	isc_region_consume(&sr, 12);
	isc_buffer_forward(source, 12);

	/*
	 * Key Length + Key Data.
	 */
	if (sr.length < 2)
		return (ISC_R_UNEXPECTEDEND);
	n = uint16_fromregion(&sr);
	if (sr.length < n + 2)
		return (ISC_R_UNEXPECTEDEND);
	RETERR(isc_mem_tobuffer(target, sr.base, n + 2));
	isc_region_consume(&sr, n + 2);
	isc_buffer_forward(source, n + 2);

	/*
	 * Other Length + Other Data.
	 */
	if (sr.length < 2)
		return (ISC_R_UNEXPECTEDEND);
	n = uint16_fromregion(&sr);
	if (sr.length < n + 2)
		return (ISC_R_UNEXPECTEDEND);
	isc_buffer_forward(source, n + 2);
	return (isc_mem_tobuffer(target, sr.base, n + 2));
}

static inline isc_result_t
towire_tkey(ARGS_TOWIRE) {
	isc_region_t sr;
	dns_name_t name;
	dns_offsets_t offsets;

	REQUIRE(rdata->type == dns_rdatatype_tkey);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_NONE);
	/*
	 * Algorithm.
	 */
	dns_rdata_toregion(rdata, &sr);
	dns_name_init(&name, offsets);
	dns_name_fromregion(&name, &sr);
	RETERR(dns_name_towire(&name, cctx, target));
	isc_region_consume(&sr, name_length(&name));

	return (isc_mem_tobuffer(target, sr.base, sr.length));
}

#endif	/* RDATA_GENERIC_TKEY_249_C */

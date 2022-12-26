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

/* $Id: zonemd_63.c,v 1.2 2022/12/26 19:24:11 jmc Exp $ */

/* RFC8976 */

#ifndef RDATA_GENERIC_ZONEMD_63_C
#define RDATA_GENERIC_ZONEMD_63_C

#define	DNS_ZONEMD_DIGEST_SHA384	(1)

static inline isc_result_t
totext_zonemd(ARGS_TOTEXT) {
	isc_region_t sr;
	char buf[sizeof("4294967295 ")];
	uint32_t n;

	REQUIRE(rdata->type == dns_rdatatype_zonemd);
	REQUIRE(rdata->length > 6);

	dns_rdata_toregion(rdata, &sr);

	/* serial */
	n = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);
	snprintf(buf, sizeof(buf), "%u ", n);
	RETERR(isc_str_tobuffer(buf, target));

	/* scheme */
	n = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);
	snprintf(buf, sizeof(buf), "%u ", n);
	RETERR(isc_str_tobuffer(buf, target));

	/* hash algo */
	n = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);
	snprintf(buf, sizeof(buf), "%u", n);
	RETERR(isc_str_tobuffer(buf, target));

	/* Digest */
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(isc_str_tobuffer(" (", target));
	RETERR(isc_str_tobuffer(tctx->linebreak, target));
	if ((tctx->flags & DNS_STYLEFLAG_NOCRYPTO) == 0) {
		if (tctx->width == 0) /* No splitting */
			RETERR(isc_hex_totext(&sr, 0, "", target));
		else
			RETERR(isc_hex_totext(&sr, tctx->width - 2,
					      tctx->linebreak, target));
	} else
		RETERR(isc_str_tobuffer("[omitted]", target));
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(isc_str_tobuffer(" )", target));
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_zonemd(ARGS_FROMWIRE) {
	isc_region_t sr;

	REQUIRE(type == dns_rdatatype_zonemd);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	isc_buffer_activeregion(source, &sr);
	/*
	 * serial: 4
	 * scheme: 1
	 * hash algorithm: 1
	 * digest: at least 1
	 */
	if (sr.length < 7)
		return (ISC_R_UNEXPECTEDEND);

	if (sr.base[5] == DNS_ZONEMD_DIGEST_SHA384) {
		if (sr.length < 6 + ISC_SHA384_DIGESTLENGTH)
			return (ISC_R_UNEXPECTEDEND);
		else
			/* truncate in case there is additional junk */
			sr.length = 6 + ISC_SHA384_DIGESTLENGTH;
	}

	isc_buffer_forward(source, sr.length);
	return (isc_mem_tobuffer(target, sr.base, sr.length));
}

static inline isc_result_t
towire_zonemd(ARGS_TOWIRE) {
	isc_region_t sr;

	REQUIRE(rdata->type == dns_rdatatype_zonemd);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &sr);
	return (isc_mem_tobuffer(target, sr.base, sr.length));
}

#endif	/* RDATA_GENERIC_ZONEMD_63_C */

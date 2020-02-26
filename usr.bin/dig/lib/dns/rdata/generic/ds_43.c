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

/* $Id: ds_43.c,v 1.13 2020/02/26 18:47:59 florian Exp $ */

/* RFC3658 */

#ifndef RDATA_GENERIC_DS_43_C
#define RDATA_GENERIC_DS_43_C

#include <isc/sha1.h>
#include <isc/sha2.h>

#include <dns/ds.h>

static inline isc_result_t
generic_totext_ds(ARGS_TOTEXT) {
	isc_region_t sr;
	char buf[sizeof("64000 ")];
	unsigned int n;

	REQUIRE(rdata->length != 0);

	UNUSED(tctx);

	dns_rdata_toregion(rdata, &sr);

	/*
	 * Key tag.
	 */
	n = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	snprintf(buf, sizeof(buf), "%u ", n);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Algorithm.
	 */
	n = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);
	snprintf(buf, sizeof(buf), "%u ", n);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Digest type.
	 */
	n = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);
	snprintf(buf, sizeof(buf), "%u", n);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Digest.
	 */
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
totext_ds(ARGS_TOTEXT) {

	REQUIRE(rdata->type == dns_rdatatype_ds);

	return (generic_totext_ds(rdata, tctx, target));
}

static inline isc_result_t
generic_fromwire_ds(ARGS_FROMWIRE) {
	isc_region_t sr;

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	isc_buffer_activeregion(source, &sr);

	/*
	 * Check digest lengths if we know them.
	 */
	if (sr.length < 4 ||
	    (sr.base[3] == DNS_DSDIGEST_SHA1 &&
	     sr.length < 4 + ISC_SHA1_DIGESTLENGTH) ||
	    (sr.base[3] == DNS_DSDIGEST_SHA256 &&
	     sr.length < 4 + ISC_SHA256_DIGESTLENGTH) ||
	    (sr.base[3] == DNS_DSDIGEST_SHA384 &&
	     sr.length < 4 + ISC_SHA384_DIGESTLENGTH))
		return (ISC_R_UNEXPECTEDEND);

	/*
	 * Only copy digest lengths if we know them.
	 * If there is extra data dns_rdata_fromwire() will
	 * detect that.
	 */
	if (sr.base[3] == DNS_DSDIGEST_SHA1)
		sr.length = 4 + ISC_SHA1_DIGESTLENGTH;
	else if (sr.base[3] == DNS_DSDIGEST_SHA256)
		sr.length = 4 + ISC_SHA256_DIGESTLENGTH;
	else if (sr.base[3] == DNS_DSDIGEST_SHA384)
		sr.length = 4 + ISC_SHA384_DIGESTLENGTH;

	isc_buffer_forward(source, sr.length);
	return (isc_mem_tobuffer(target, sr.base, sr.length));
}

static inline isc_result_t
fromwire_ds(ARGS_FROMWIRE) {

	REQUIRE(type == dns_rdatatype_ds);

	return (generic_fromwire_ds(rdclass, type, source, dctx, options,
				    target));
}

static inline isc_result_t
towire_ds(ARGS_TOWIRE) {
	isc_region_t sr;

	REQUIRE(rdata->type == dns_rdatatype_ds);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &sr);
	return (isc_mem_tobuffer(target, sr.base, sr.length));
}

#endif	/* RDATA_GENERIC_DS_43_C */

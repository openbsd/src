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

/* $Id: rrsig_46.c,v 1.12 2020/02/26 18:47:59 florian Exp $ */

/* Reviewed: Fri Mar 17 09:05:02 PST 2000 by gson */

/* RFC2535 */

#ifndef RDATA_GENERIC_RRSIG_46_C
#define RDATA_GENERIC_RRSIG_46_C

static inline isc_result_t
totext_rrsig(ARGS_TOTEXT) {
	isc_region_t sr;
	char buf[sizeof("4294967295")];	/* Also TYPE65000. */
	dns_rdatatype_t covered;
	unsigned long ttl;
	unsigned long when;
	unsigned long exp;
	unsigned long foot;
	dns_name_t name;

	REQUIRE(rdata->type == dns_rdatatype_rrsig);
	REQUIRE(rdata->length != 0);

	dns_rdata_toregion(rdata, &sr);

	/*
	 * Type covered.
	 */
	covered = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);

	RETERR(dns_rdatatype_totext(covered, target));
	RETERR(isc_str_tobuffer(" ", target));

	/*
	 * Algorithm.
	 */
	snprintf(buf, sizeof(buf), "%u", sr.base[0]);
	isc_region_consume(&sr, 1);
	RETERR(isc_str_tobuffer(buf, target));
	RETERR(isc_str_tobuffer(" ", target));

	/*
	 * Labels.
	 */
	snprintf(buf, sizeof(buf), "%u", sr.base[0]);
	isc_region_consume(&sr, 1);
	RETERR(isc_str_tobuffer(buf, target));
	RETERR(isc_str_tobuffer(" ", target));

	/*
	 * Ttl.
	 */
	ttl = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);
	snprintf(buf, sizeof(buf), "%lu", ttl);
	RETERR(isc_str_tobuffer(buf, target));

	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(isc_str_tobuffer(" (", target));
	RETERR(isc_str_tobuffer(tctx->linebreak, target));

	/*
	 * Sig exp.
	 */
	exp = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);
	RETERR(dns_time32_totext(exp, target));
	RETERR(isc_str_tobuffer(" ", target));

	/*
	 * Time signed.
	 */
	when = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);
	RETERR(dns_time32_totext(when, target));
	RETERR(isc_str_tobuffer(" ", target));

	/*
	 * Footprint.
	 */
	foot = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	snprintf(buf, sizeof(buf), "%lu", foot);
	RETERR(isc_str_tobuffer(buf, target));
	RETERR(isc_str_tobuffer(" ", target));

	/*
	 * Signer.
	 */
	dns_name_init(&name, NULL);
	dns_name_fromregion(&name, &sr);
	isc_region_consume(&sr, name_length(&name));
	RETERR(dns_name_totext(&name, ISC_FALSE, target));

	/*
	 * Sig.
	 */
	RETERR(isc_str_tobuffer(tctx->linebreak, target));
	if ((tctx->flags & DNS_STYLEFLAG_NOCRYPTO) == 0) {
		if (tctx->width == 0)   /* No splitting */
			RETERR(isc_base64_totext(&sr, 60, "", target));
		else
			RETERR(isc_base64_totext(&sr, tctx->width - 2,
						 tctx->linebreak, target));
	} else
		RETERR(isc_str_tobuffer("[omitted]", target));

	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(isc_str_tobuffer(" )", target));

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_rrsig(ARGS_FROMWIRE) {
	isc_region_t sr;
	dns_name_t name;

	REQUIRE(type == dns_rdatatype_rrsig);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_NONE);

	isc_buffer_activeregion(source, &sr);
	/*
	 * type covered: 2
	 * algorithm: 1
	 * labels: 1
	 * original ttl: 4
	 * signature expiration: 4
	 * time signed: 4
	 * key footprint: 2
	 */
	if (sr.length < 18)
		return (ISC_R_UNEXPECTEDEND);

	isc_buffer_forward(source, 18);
	RETERR(isc_mem_tobuffer(target, sr.base, 18));

	/*
	 * Signer.
	 */
	dns_name_init(&name, NULL);
	RETERR(dns_name_fromwire(&name, source, dctx, options, target));

	/*
	 * Sig.
	 */
	isc_buffer_activeregion(source, &sr);
	isc_buffer_forward(source, sr.length);
	return (isc_mem_tobuffer(target, sr.base, sr.length));
}

static inline isc_result_t
towire_rrsig(ARGS_TOWIRE) {
	isc_region_t sr;
	dns_name_t name;
	dns_offsets_t offsets;

	REQUIRE(rdata->type == dns_rdatatype_rrsig);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_NONE);
	dns_rdata_toregion(rdata, &sr);
	/*
	 * type covered: 2
	 * algorithm: 1
	 * labels: 1
	 * original ttl: 4
	 * signature expiration: 4
	 * time signed: 4
	 * key footprint: 2
	 */
	RETERR(isc_mem_tobuffer(target, sr.base, 18));
	isc_region_consume(&sr, 18);

	/*
	 * Signer.
	 */
	dns_name_init(&name, offsets);
	dns_name_fromregion(&name, &sr);
	isc_region_consume(&sr, name_length(&name));
	RETERR(dns_name_towire(&name, cctx, target));

	/*
	 * Signature.
	 */
	return (isc_mem_tobuffer(target, sr.base, sr.length));
}

static inline dns_rdatatype_t
covers_rrsig(dns_rdata_t *rdata) {
	dns_rdatatype_t type;
	isc_region_t r;

	REQUIRE(rdata->type == dns_rdatatype_rrsig);

	dns_rdata_toregion(rdata, &r);
	type = uint16_fromregion(&r);

	return (type);
}

#endif	/* RDATA_GENERIC_RRSIG_46_C */

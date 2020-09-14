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

/* $Id: tsig_250.c,v 1.10 2020/09/14 08:40:43 florian Exp $ */

/* Reviewed: Thu Mar 16 13:39:43 PST 2000 by gson */

#ifndef RDATA_ANY_255_TSIG_250_C
#define RDATA_ANY_255_TSIG_250_C

static inline isc_result_t
totext_any_tsig(ARGS_TOTEXT) {
	isc_region_t sr;
	isc_region_t sigr;
	char buf[sizeof(" 281474976710655 ")];
	char *bufp;
	dns_name_t name;
	dns_name_t prefix;
	int sub;
	uint64_t sigtime;
	unsigned short n;

	REQUIRE(rdata->type == dns_rdatatype_tsig);
	REQUIRE(rdata->rdclass == dns_rdataclass_any);
	REQUIRE(rdata->length != 0);

	dns_rdata_toregion(rdata, &sr);
	/*
	 * Algorithm Name.
	 */
	dns_name_init(&name, NULL);
	dns_name_init(&prefix, NULL);
	dns_name_fromregion(&name, &sr);
	sub = name_prefix(&name, tctx->origin, &prefix);
	RETERR(dns_name_totext(&prefix, sub, target));
	RETERR(isc_str_tobuffer(" ", target));
	isc_region_consume(&sr, name_length(&name));

	/*
	 * Time Signed.
	 */
	sigtime = ((uint64_t)sr.base[0] << 40) |
		  ((uint64_t)sr.base[1] << 32) |
		  ((uint64_t)sr.base[2] << 24) |
		  ((uint64_t)sr.base[3] << 16) |
		  ((uint64_t)sr.base[4] << 8) |
		  (uint64_t)sr.base[5];
	isc_region_consume(&sr, 6);
	bufp = &buf[sizeof(buf) - 1];
	*bufp-- = 0;
	*bufp-- = ' ';
	do {
		*bufp-- = decdigits[sigtime % 10];
		sigtime /= 10;
	} while (sigtime != 0);
	bufp++;
	RETERR(isc_str_tobuffer(bufp, target));

	/*
	 * Fudge.
	 */
	n = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	snprintf(buf, sizeof(buf), "%u ", n);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Signature Size.
	 */
	n = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	snprintf(buf, sizeof(buf), "%u", n);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Signature.
	 */
	REQUIRE(n <= sr.length);
	sigr = sr;
	sigr.length = n;
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(isc_str_tobuffer(" (", target));
	RETERR(isc_str_tobuffer(tctx->linebreak, target));
	if (tctx->width == 0)   /* No splitting */
		RETERR(isc_base64_totext(&sigr, 60, "", target));
	else
		RETERR(isc_base64_totext(&sigr, tctx->width - 2,
					 tctx->linebreak, target));
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(isc_str_tobuffer(" ) ", target));
	else
		RETERR(isc_str_tobuffer(" ", target));
	isc_region_consume(&sr, n);

	/*
	 * Original ID.
	 */
	n = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	snprintf(buf, sizeof(buf), "%u ", n);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Error.
	 */
	n = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	RETERR(dns_tsigrcode_totext((dns_rcode_t)n, target));

	/*
	 * Other Size.
	 */
	n = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	snprintf(buf, sizeof(buf), " %u ", n);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Other.
	 */
	if (tctx->width == 0)   /* No splitting */
		return (isc_base64_totext(&sr, 60, "", target));
	else
		return (isc_base64_totext(&sr, 60, " ", target));
}

static inline isc_result_t
fromwire_any_tsig(ARGS_FROMWIRE) {
	isc_region_t sr;
	dns_name_t name;
	unsigned long n;

	REQUIRE(type == dns_rdatatype_tsig);
	REQUIRE(rdclass == dns_rdataclass_any);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_NONE);

	/*
	 * Algorithm Name.
	 */
	dns_name_init(&name, NULL);
	RETERR(dns_name_fromwire(&name, source, dctx, options, target));

	isc_buffer_activeregion(source, &sr);
	/*
	 * Time Signed + Fudge.
	 */
	if (sr.length < 8)
		return (ISC_R_UNEXPECTEDEND);
	RETERR(isc_mem_tobuffer(target, sr.base, 8));
	isc_region_consume(&sr, 8);
	isc_buffer_forward(source, 8);

	/*
	 * Signature Length + Signature.
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
	 * Original ID + Error.
	 */
	if (sr.length < 4)
		return (ISC_R_UNEXPECTEDEND);
	RETERR(isc_mem_tobuffer(target, sr.base,  4));
	isc_region_consume(&sr, 4);
	isc_buffer_forward(source, 4);

	/*
	 * Other Length + Other.
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
towire_any_tsig(ARGS_TOWIRE) {
	isc_region_t sr;
	dns_name_t name;
	dns_offsets_t offsets;

	REQUIRE(rdata->type == dns_rdatatype_tsig);
	REQUIRE(rdata->rdclass == dns_rdataclass_any);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_NONE);
	dns_rdata_toregion(rdata, &sr);
	dns_name_init(&name, offsets);
	dns_name_fromregion(&name, &sr);
	RETERR(dns_name_towire(&name, cctx, target));
	isc_region_consume(&sr, name_length(&name));
	return (isc_mem_tobuffer(target, sr.base, sr.length));
}

static inline isc_result_t
fromstruct_any_tsig(ARGS_FROMSTRUCT) {
	dns_rdata_any_tsig_t *tsig = source;
	isc_region_t tr;

	REQUIRE(type == dns_rdatatype_tsig);
	REQUIRE(rdclass == dns_rdataclass_any);
	REQUIRE(source != NULL);
	REQUIRE(tsig->common.rdclass == rdclass);
	REQUIRE(tsig->common.rdtype == type);

	UNUSED(type);
	UNUSED(rdclass);

	/*
	 * Algorithm Name.
	 */
	RETERR(name_tobuffer(&tsig->algorithm, target));

	isc_buffer_availableregion(target, &tr);
	if (tr.length < 6 + 2 + 2)
		return (ISC_R_NOSPACE);

	/*
	 * Time Signed: 48 bits.
	 */
	RETERR(uint16_tobuffer((uint16_t)(tsig->timesigned >> 32),
			       target));
	RETERR(uint32_tobuffer((uint32_t)(tsig->timesigned & 0xffffffffU),
			       target));

	/*
	 * Fudge.
	 */
	RETERR(uint16_tobuffer(tsig->fudge, target));

	/*
	 * Signature Size.
	 */
	RETERR(uint16_tobuffer(tsig->siglen, target));

	/*
	 * Signature.
	 */
	RETERR(isc_mem_tobuffer(target, tsig->signature, tsig->siglen));

	isc_buffer_availableregion(target, &tr);
	if (tr.length < 2 + 2 + 2)
		return (ISC_R_NOSPACE);

	/*
	 * Original ID.
	 */
	RETERR(uint16_tobuffer(tsig->originalid, target));

	/*
	 * Error.
	 */
	RETERR(uint16_tobuffer(tsig->error, target));

	/*
	 * Other Len.
	 */
	RETERR(uint16_tobuffer(tsig->otherlen, target));

	/*
	 * Other Data.
	 */
	return (isc_mem_tobuffer(target, tsig->other, tsig->otherlen));
}

static inline isc_result_t
tostruct_any_tsig(ARGS_TOSTRUCT) {
	dns_rdata_any_tsig_t *tsig;
	dns_name_t alg;
	isc_region_t sr;

	REQUIRE(rdata->type == dns_rdatatype_tsig);
	REQUIRE(rdata->rdclass == dns_rdataclass_any);
	REQUIRE(rdata->length != 0);

	tsig = (dns_rdata_any_tsig_t *) target;
	tsig->common.rdclass = rdata->rdclass;
	tsig->common.rdtype = rdata->type;
	ISC_LINK_INIT(&tsig->common, link);

	dns_rdata_toregion(rdata, &sr);

	/*
	 * Algorithm Name.
	 */
	dns_name_init(&alg, NULL);
	dns_name_fromregion(&alg, &sr);
	dns_name_init(&tsig->algorithm, NULL);
	RETERR(name_duporclone(&alg, &tsig->algorithm));

	isc_region_consume(&sr, name_length(&tsig->algorithm));

	/*
	 * Time Signed.
	 */
	INSIST(sr.length >= 6);
	tsig->timesigned = ((uint64_t)sr.base[0] << 40) |
			   ((uint64_t)sr.base[1] << 32) |
			   ((uint64_t)sr.base[2] << 24) |
			   ((uint64_t)sr.base[3] << 16) |
			   ((uint64_t)sr.base[4] << 8) |
			   (uint64_t)sr.base[5];
	isc_region_consume(&sr, 6);

	/*
	 * Fudge.
	 */
	tsig->fudge = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);

	/*
	 * Signature Size.
	 */
	tsig->siglen = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);

	/*
	 * Signature.
	 */
	INSIST(sr.length >= tsig->siglen);
	tsig->signature = mem_maybedup(sr.base, tsig->siglen);
	if (tsig->signature == NULL)
		goto cleanup;
	isc_region_consume(&sr, tsig->siglen);

	/*
	 * Original ID.
	 */
	tsig->originalid = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);

	/*
	 * Error.
	 */
	tsig->error = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);

	/*
	 * Other Size.
	 */
	tsig->otherlen = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);

	/*
	 * Other.
	 */
	INSIST(sr.length == tsig->otherlen);
	tsig->other = mem_maybedup(sr.base, tsig->otherlen);
	if (tsig->other == NULL)
		goto cleanup;

	return (ISC_R_SUCCESS);

 cleanup:
	dns_name_free(&tsig->algorithm);
	free(tsig->signature);
	return (ISC_R_NOMEMORY);
}

static inline void
freestruct_any_tsig(ARGS_FREESTRUCT) {
	dns_rdata_any_tsig_t *tsig = (dns_rdata_any_tsig_t *) source;

	REQUIRE(source != NULL);
	REQUIRE(tsig->common.rdtype == dns_rdatatype_tsig);
	REQUIRE(tsig->common.rdclass == dns_rdataclass_any);

	dns_name_free(&tsig->algorithm);
	free(tsig->signature);
	free(tsig->other);
}

#endif	/* RDATA_ANY_255_TSIG_250_C */

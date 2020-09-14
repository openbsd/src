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

/* $Id: apl_42.c,v 1.14 2020/09/14 08:40:44 florian Exp $ */

/* RFC3123 */

#ifndef RDATA_IN_1_APL_42_C
#define RDATA_IN_1_APL_42_C

static inline isc_result_t
totext_in_apl(ARGS_TOTEXT) {
	isc_region_t sr;
	isc_region_t ir;
	uint16_t afi;
	uint8_t prefix;
	uint8_t len;
	int neg;
	unsigned char buf[16];
	char txt[sizeof(" !64000:")];
	const char *sep = "";
	int n;

	REQUIRE(rdata->type == dns_rdatatype_apl);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);

	UNUSED(tctx);

	dns_rdata_toregion(rdata, &sr);
	ir.base = buf;
	ir.length = sizeof(buf);

	while (sr.length > 0) {
		INSIST(sr.length >= 4);
		afi = uint16_fromregion(&sr);
		isc_region_consume(&sr, 2);
		prefix = *sr.base;
		isc_region_consume(&sr, 1);
		len = (*sr.base & 0x7f);
		neg = (*sr.base & 0x80) != 0;
		isc_region_consume(&sr, 1);
		INSIST(len <= sr.length);
		n = snprintf(txt, sizeof(txt), "%s%s%u:", sep,
			     neg ? "!" : "", afi);
		INSIST(n < (int)sizeof(txt));
		RETERR(isc_str_tobuffer(txt, target));
		switch (afi) {
		case 1:
			INSIST(len <= 4);
			INSIST(prefix <= 32);
			memset(buf, 0, sizeof(buf));
			memmove(buf, sr.base, len);
			RETERR(inet_totext(AF_INET, &ir, target));
			break;

		case 2:
			INSIST(len <= 16);
			INSIST(prefix <= 128);
			memset(buf, 0, sizeof(buf));
			memmove(buf, sr.base, len);
			RETERR(inet_totext(AF_INET6, &ir, target));
			break;

		default:
			return (ISC_R_NOTIMPLEMENTED);
		}
		n = snprintf(txt, sizeof(txt), "/%u", prefix);
		INSIST(n < (int)sizeof(txt));
		RETERR(isc_str_tobuffer(txt, target));
		isc_region_consume(&sr, len);
		sep = " ";
	}
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_in_apl(ARGS_FROMWIRE) {
	isc_region_t sr, sr2;
	isc_region_t tr;
	uint16_t afi;
	uint8_t prefix;
	uint8_t len;

	REQUIRE(type == dns_rdatatype_apl);
	REQUIRE(rdclass == dns_rdataclass_in);

	UNUSED(type);
	UNUSED(dctx);
	UNUSED(rdclass);
	UNUSED(options);

	isc_buffer_activeregion(source, &sr);
	isc_buffer_availableregion(target, &tr);
	if (sr.length > tr.length)
		return (ISC_R_NOSPACE);
	sr2 = sr;

	/* Zero or more items */
	while (sr.length > 0) {
		if (sr.length < 4)
			return (ISC_R_UNEXPECTEDEND);
		afi = uint16_fromregion(&sr);
		isc_region_consume(&sr, 2);
		prefix = *sr.base;
		isc_region_consume(&sr, 1);
		len = (*sr.base & 0x7f);
		isc_region_consume(&sr, 1);
		if (len > sr.length)
			return (ISC_R_UNEXPECTEDEND);
		switch (afi) {
		case 1:
			if (prefix > 32 || len > 4)
				return (ISC_R_RANGE);
			break;
		case 2:
			if (prefix > 128 || len > 16)
				return (ISC_R_RANGE);
		}
		if (len > 0 && sr.base[len - 1] == 0)
			return (DNS_R_FORMERR);
		isc_region_consume(&sr, len);
	}
	isc_buffer_forward(source, sr2.length);
	return (isc_mem_tobuffer(target, sr2.base, sr2.length));
}

static inline isc_result_t
towire_in_apl(ARGS_TOWIRE) {
	UNUSED(cctx);

	REQUIRE(rdata->type == dns_rdatatype_apl);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);

	return (isc_mem_tobuffer(target, rdata->data, rdata->length));
}

#endif	/* RDATA_IN_1_APL_42_C */

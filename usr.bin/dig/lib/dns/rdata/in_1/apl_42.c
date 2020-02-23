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

/* $Id: apl_42.c,v 1.4 2020/02/23 19:54:26 jung Exp $ */

/* RFC3123 */

#ifndef RDATA_IN_1_APL_42_C
#define RDATA_IN_1_APL_42_C

#define RRTYPE_APL_ATTRIBUTES (0)

static inline isc_result_t
totext_in_apl(ARGS_TOTEXT) {
	isc_region_t sr;
	isc_region_t ir;
	uint16_t afi;
	uint8_t prefix;
	uint8_t len;
	isc_boolean_t neg;
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
		neg = ISC_TF((*sr.base & 0x80) != 0);
		isc_region_consume(&sr, 1);
		INSIST(len <= sr.length);
		n = snprintf(txt, sizeof(txt), "%s%s%u:", sep,
			     neg ? "!" : "", afi);
		INSIST(n < (int)sizeof(txt));
		RETERR(str_totext(txt, target));
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
		RETERR(str_totext(txt, target));
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
	return (mem_tobuffer(target, sr2.base, sr2.length));
}

static inline isc_result_t
towire_in_apl(ARGS_TOWIRE) {
	UNUSED(cctx);

	REQUIRE(rdata->type == dns_rdatatype_apl);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);

	return (mem_tobuffer(target, rdata->data, rdata->length));
}

static inline int
compare_in_apl(ARGS_COMPARE) {
	isc_region_t r1;
	isc_region_t r2;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == dns_rdatatype_apl);
	REQUIRE(rdata1->rdclass == dns_rdataclass_in);

	dns_rdata_toregion(rdata1, &r1);
	dns_rdata_toregion(rdata2, &r2);
	return (isc_region_compare(&r1, &r2));
}

static inline isc_result_t
fromstruct_in_apl(ARGS_FROMSTRUCT) {
	dns_rdata_in_apl_t *apl = source;
	isc_buffer_t b;

	REQUIRE(type == dns_rdatatype_apl);
	REQUIRE(rdclass == dns_rdataclass_in);
	REQUIRE(source != NULL);
	REQUIRE(apl->common.rdtype == type);
	REQUIRE(apl->common.rdclass == rdclass);
	REQUIRE(apl->apl != NULL || apl->apl_len == 0);

	isc_buffer_init(&b, apl->apl, apl->apl_len);
	isc_buffer_add(&b, apl->apl_len);
	isc_buffer_setactive(&b, apl->apl_len);
	return(fromwire_in_apl(rdclass, type, &b, NULL, ISC_FALSE, target));
}

static inline isc_result_t
tostruct_in_apl(ARGS_TOSTRUCT) {
	dns_rdata_in_apl_t *apl = target;
	isc_region_t r;

	REQUIRE(rdata->type == dns_rdatatype_apl);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);

	apl->common.rdclass = rdata->rdclass;
	apl->common.rdtype = rdata->type;
	ISC_LINK_INIT(&apl->common, link);

	dns_rdata_toregion(rdata, &r);
	apl->apl_len = r.length;
	apl->apl = mem_maybedup(r.base, r.length);
	if (apl->apl == NULL)
		return (ISC_R_NOMEMORY);

	apl->offset = 0;
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_in_apl(ARGS_FREESTRUCT) {
	dns_rdata_in_apl_t *apl = source;

	REQUIRE(source != NULL);
	REQUIRE(apl->common.rdtype == dns_rdatatype_apl);
	REQUIRE(apl->common.rdclass == dns_rdataclass_in);

	if (apl->apl != NULL)
		free(apl->apl);
}

static inline isc_boolean_t
checkowner_in_apl(ARGS_CHECKOWNER) {

	REQUIRE(type == dns_rdatatype_apl);
	REQUIRE(rdclass == dns_rdataclass_in);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

static inline int
casecompare_in_apl(ARGS_COMPARE) {
	return (compare_in_apl(rdata1, rdata2));
}

#endif	/* RDATA_IN_1_APL_42_C */

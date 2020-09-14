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

/* $Id: a_1.c,v 1.13 2020/09/14 08:40:43 florian Exp $ */

/* by Bjorn.Victor@it.uu.se, 2005-05-07 */
/* Based on generic/soa_6.c and generic/mx_15.c */

#ifndef RDATA_CH_3_A_1_C
#define RDATA_CH_3_A_1_C

static inline isc_result_t
totext_ch_a(ARGS_TOTEXT) {
	isc_region_t region;
	dns_name_t name;
	dns_name_t prefix;
	int sub;
	char buf[sizeof("0177777")];
	uint16_t addr;

	REQUIRE(rdata->type == dns_rdatatype_a);
	REQUIRE(rdata->rdclass == dns_rdataclass_ch); /* 3 */
	REQUIRE(rdata->length != 0);

	dns_name_init(&name, NULL);
	dns_name_init(&prefix, NULL);

	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);
	isc_region_consume(&region, name_length(&name));
	addr = uint16_fromregion(&region);

	sub = name_prefix(&name, tctx->origin, &prefix);
	RETERR(dns_name_totext(&prefix, sub, target));

	snprintf(buf, sizeof(buf), "%o", addr); /* note octal */
	RETERR(isc_str_tobuffer(" ", target));
	return (isc_str_tobuffer(buf, target));
}

static inline isc_result_t
fromwire_ch_a(ARGS_FROMWIRE) {
	isc_region_t sregion;
	isc_region_t tregion;
	dns_name_t name;

	REQUIRE(type == dns_rdatatype_a);
	REQUIRE(rdclass == dns_rdataclass_ch);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_GLOBAL14);

	dns_name_init(&name, NULL);

	RETERR(dns_name_fromwire(&name, source, dctx, options, target));

	isc_buffer_activeregion(source, &sregion);
	isc_buffer_availableregion(target, &tregion);
	if (sregion.length < 2)
		return (ISC_R_UNEXPECTEDEND);
	if (tregion.length < 2)
		return (ISC_R_NOSPACE);

	memmove(tregion.base, sregion.base, 2);
	isc_buffer_forward(source, 2);
	isc_buffer_add(target, 2);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_ch_a(ARGS_TOWIRE) {
	dns_name_t name;
	dns_offsets_t offsets;
	isc_region_t sregion;
	isc_region_t tregion;

	REQUIRE(rdata->type == dns_rdatatype_a);
	REQUIRE(rdata->rdclass == dns_rdataclass_ch);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_GLOBAL14);

	dns_name_init(&name, offsets);

	dns_rdata_toregion(rdata, &sregion);

	dns_name_fromregion(&name, &sregion);
	isc_region_consume(&sregion, name_length(&name));
	RETERR(dns_name_towire(&name, cctx, target));

	isc_buffer_availableregion(target, &tregion);
	if (tregion.length < 2)
		return (ISC_R_NOSPACE);

	memmove(tregion.base, sregion.base, 2);
	isc_buffer_add(target, 2);
	return (ISC_R_SUCCESS);
}

#endif	/* RDATA_CH_3_A_1_C */

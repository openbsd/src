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

/* $Id: a6_38.c,v 1.12 2020/02/26 18:47:59 florian Exp $ */

/* RFC2874 */

#ifndef RDATA_IN_1_A6_28_C
#define RDATA_IN_1_A6_28_C

#include <isc/net.h>

static inline isc_result_t
totext_in_a6(ARGS_TOTEXT) {
	isc_region_t sr, ar;
	unsigned char addr[16];
	unsigned char prefixlen;
	unsigned char octets;
	unsigned char mask;
	char buf[sizeof("128")];
	dns_name_t name;
	dns_name_t prefix;
	isc_boolean_t sub;

	REQUIRE(rdata->type == dns_rdatatype_a6);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->length != 0);

	dns_rdata_toregion(rdata, &sr);
	prefixlen = sr.base[0];
	INSIST(prefixlen <= 128);
	isc_region_consume(&sr, 1);
	snprintf(buf, sizeof(buf), "%u", prefixlen);
	RETERR(isc_str_tobuffer(buf, target));
	RETERR(isc_str_tobuffer(" ", target));

	if (prefixlen != 128) {
		octets = prefixlen/8;
		memset(addr, 0, sizeof(addr));
		memmove(&addr[octets], sr.base, 16 - octets);
		mask = 0xff >> (prefixlen % 8);
		addr[octets] &= mask;
		ar.base = addr;
		ar.length = sizeof(addr);
		RETERR(inet_totext(AF_INET6, &ar, target));
		isc_region_consume(&sr, 16 - octets);
	}

	if (prefixlen == 0)
		return (ISC_R_SUCCESS);

	RETERR(isc_str_tobuffer(" ", target));
	dns_name_init(&name, NULL);
	dns_name_init(&prefix, NULL);
	dns_name_fromregion(&name, &sr);
	sub = name_prefix(&name, tctx->origin, &prefix);
	return (dns_name_totext(&prefix, sub, target));
}

static inline isc_result_t
fromwire_in_a6(ARGS_FROMWIRE) {
	isc_region_t sr;
	unsigned char prefixlen;
	unsigned char octets;
	unsigned char mask;
	dns_name_t name;

	REQUIRE(type == dns_rdatatype_a6);
	REQUIRE(rdclass == dns_rdataclass_in);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_NONE);

	isc_buffer_activeregion(source, &sr);
	/*
	 * Prefix length.
	 */
	if (sr.length < 1)
		return (ISC_R_UNEXPECTEDEND);
	prefixlen = sr.base[0];
	if (prefixlen > 128)
		return (ISC_R_RANGE);
	isc_region_consume(&sr, 1);
	RETERR(isc_mem_tobuffer(target, &prefixlen, 1));
	isc_buffer_forward(source, 1);

	/*
	 * Suffix.
	 */
	if (prefixlen != 128) {
		octets = 16 - prefixlen / 8;
		if (sr.length < octets)
			return (ISC_R_UNEXPECTEDEND);
		mask = 0xff >> (prefixlen % 8);
		sr.base[0] &= mask;	/* Ensure pad bits are zero. */
		RETERR(isc_mem_tobuffer(target, sr.base, octets));
		isc_buffer_forward(source, octets);
	}

	if (prefixlen == 0)
		return (ISC_R_SUCCESS);

	dns_name_init(&name, NULL);
	return (dns_name_fromwire(&name, source, dctx, options, target));
}

static inline isc_result_t
towire_in_a6(ARGS_TOWIRE) {
	isc_region_t sr;
	dns_name_t name;
	dns_offsets_t offsets;
	unsigned char prefixlen;
	unsigned char octets;

	REQUIRE(rdata->type == dns_rdatatype_a6);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_NONE);
	dns_rdata_toregion(rdata, &sr);
	prefixlen = sr.base[0];
	INSIST(prefixlen <= 128);

	octets = 1 + 16 - prefixlen / 8;
	RETERR(isc_mem_tobuffer(target, sr.base, octets));
	isc_region_consume(&sr, octets);

	if (prefixlen == 0)
		return (ISC_R_SUCCESS);

	dns_name_init(&name, offsets);
	dns_name_fromregion(&name, &sr);
	return (dns_name_towire(&name, cctx, target));
}

#endif	/* RDATA_IN_1_A6_38_C */

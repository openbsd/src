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

/* $Id: wks_11.c,v 1.13 2020/02/26 18:47:59 florian Exp $ */

/* Reviewed: Fri Mar 17 15:01:49 PST 2000 by explorer */

#ifndef RDATA_IN_1_WKS_11_C
#define RDATA_IN_1_WKS_11_C

#include <netdb.h>
#include <limits.h>
#include <stdlib.h>

#include <isc/net.h>

static inline isc_result_t
totext_in_wks(ARGS_TOTEXT) {
	isc_region_t sr;
	unsigned short proto;
	char buf[sizeof("65535")];
	unsigned int i, j;

	UNUSED(tctx);

	REQUIRE(rdata->type == dns_rdatatype_wks);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->length >= 5);

	dns_rdata_toregion(rdata, &sr);
	RETERR(inet_totext(AF_INET, &sr, target));
	isc_region_consume(&sr, 4);

	proto = uint8_fromregion(&sr);
	snprintf(buf, sizeof(buf), "%u", proto);
	RETERR(isc_str_tobuffer(" ", target));
	RETERR(isc_str_tobuffer(buf, target));
	isc_region_consume(&sr, 1);

	INSIST(sr.length <= 8*1024);
	for (i = 0; i < sr.length; i++) {
		if (sr.base[i] != 0)
			for (j = 0; j < 8; j++)
				if ((sr.base[i] & (0x80 >> j)) != 0) {
					snprintf(buf, sizeof(buf),
						 "%u", i * 8 + j);
					RETERR(isc_str_tobuffer(" ", target));
					RETERR(isc_str_tobuffer(buf, target));
				}
	}

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_in_wks(ARGS_FROMWIRE) {
	isc_region_t sr;
	isc_region_t tr;

	REQUIRE(type == dns_rdatatype_wks);
	REQUIRE(rdclass == dns_rdataclass_in);

	UNUSED(type);
	UNUSED(dctx);
	UNUSED(options);
	UNUSED(rdclass);

	isc_buffer_activeregion(source, &sr);
	isc_buffer_availableregion(target, &tr);

	if (sr.length < 5)
		return (ISC_R_UNEXPECTEDEND);
	if (sr.length > 8 * 1024 + 5)
		return (DNS_R_EXTRADATA);
	if (tr.length < sr.length)
		return (ISC_R_NOSPACE);

	memmove(tr.base, sr.base, sr.length);
	isc_buffer_add(target, sr.length);
	isc_buffer_forward(source, sr.length);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_in_wks(ARGS_TOWIRE) {
	isc_region_t sr;

	UNUSED(cctx);

	REQUIRE(rdata->type == dns_rdatatype_wks);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->length != 0);

	dns_rdata_toregion(rdata, &sr);
	return (isc_mem_tobuffer(target, sr.base, sr.length));
}

#endif	/* RDATA_IN_1_WKS_11_C */

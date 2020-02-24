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

/* $Id: wks_11.c,v 1.6 2020/02/24 12:06:51 florian Exp $ */

/* Reviewed: Fri Mar 17 15:01:49 PST 2000 by explorer */

#ifndef RDATA_IN_1_WKS_11_C
#define RDATA_IN_1_WKS_11_C

#include <netdb.h>
#include <limits.h>
#include <stdlib.h>

#include <isc/net.h>

#define RRTYPE_WKS_ATTRIBUTES (0)

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
	RETERR(str_totext(" ", target));
	RETERR(str_totext(buf, target));
	isc_region_consume(&sr, 1);

	INSIST(sr.length <= 8*1024);
	for (i = 0; i < sr.length; i++) {
		if (sr.base[i] != 0)
			for (j = 0; j < 8; j++)
				if ((sr.base[i] & (0x80 >> j)) != 0) {
					snprintf(buf, sizeof(buf),
						 "%u", i * 8 + j);
					RETERR(str_totext(" ", target));
					RETERR(str_totext(buf, target));
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
	return (mem_tobuffer(target, sr.base, sr.length));
}


static inline isc_result_t
fromstruct_in_wks(ARGS_FROMSTRUCT) {
	dns_rdata_in_wks_t *wks = source;
	uint32_t a;

	REQUIRE(type == dns_rdatatype_wks);
	REQUIRE(rdclass == dns_rdataclass_in);
	REQUIRE(source != NULL);
	REQUIRE(wks->common.rdtype == type);
	REQUIRE(wks->common.rdclass == rdclass);
	REQUIRE((wks->map != NULL && wks->map_len <= 8*1024) ||
		 wks->map_len == 0);

	UNUSED(type);
	UNUSED(rdclass);

	a = ntohl(wks->in_addr.s_addr);
	RETERR(uint32_tobuffer(a, target));
	RETERR(uint8_tobuffer(wks->protocol, target));
	return (mem_tobuffer(target, wks->map, wks->map_len));
}

static inline isc_result_t
tostruct_in_wks(ARGS_TOSTRUCT) {
	dns_rdata_in_wks_t *wks = target;
	uint32_t n;
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_wks);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->length != 0);

	wks->common.rdclass = rdata->rdclass;
	wks->common.rdtype = rdata->type;
	ISC_LINK_INIT(&wks->common, link);

	dns_rdata_toregion(rdata, &region);
	n = uint32_fromregion(&region);
	wks->in_addr.s_addr = htonl(n);
	isc_region_consume(&region, 4);
	wks->protocol = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	wks->map_len = region.length;
	wks->map = mem_maybedup(region.base, region.length);
	if (wks->map == NULL)
		return (ISC_R_NOMEMORY);
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_in_wks(ARGS_FREESTRUCT) {
	dns_rdata_in_wks_t *wks = source;

	REQUIRE(source != NULL);
	REQUIRE(wks->common.rdtype == dns_rdatatype_wks);
	REQUIRE(wks->common.rdclass == dns_rdataclass_in);

	if (wks->map != NULL)
		free(wks->map);
}



#endif	/* RDATA_IN_1_WKS_11_C */

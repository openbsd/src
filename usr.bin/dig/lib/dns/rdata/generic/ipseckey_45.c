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

/* $Id: ipseckey_45.c,v 1.12 2020/02/26 18:47:59 florian Exp $ */

#ifndef RDATA_GENERIC_IPSECKEY_45_C
#define RDATA_GENERIC_IPSECKEY_45_C

#include <string.h>

#include <isc/net.h>

static inline isc_result_t
totext_ipseckey(ARGS_TOTEXT) {
	isc_region_t region;
	dns_name_t name;
	char buf[sizeof("255 ")];
	unsigned short num;
	unsigned short gateway;

	REQUIRE(rdata->type == dns_rdatatype_ipseckey);
	REQUIRE(rdata->length >= 3);

	dns_name_init(&name, NULL);

	if (rdata->data[1] > 3U)
		return (ISC_R_NOTIMPLEMENTED);

	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(isc_str_tobuffer("( ", target));

	/*
	 * Precedence.
	 */
	dns_rdata_toregion(rdata, &region);
	num = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	snprintf(buf, sizeof(buf), "%u ", num);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Gateway type.
	 */
	gateway = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	snprintf(buf, sizeof(buf), "%u ", gateway);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Algorithm.
	 */
	num = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	snprintf(buf, sizeof(buf), "%u ", num);
	RETERR(isc_str_tobuffer(buf, target));

	/*
	 * Gateway.
	 */
	switch (gateway) {
	case 0:
		RETERR(isc_str_tobuffer(".", target));
		break;

	case 1:
		RETERR(inet_totext(AF_INET, &region, target));
		isc_region_consume(&region, 4);
		break;

	case 2:
		RETERR(inet_totext(AF_INET6, &region, target));
		isc_region_consume(&region, 16);
		break;

	case 3:
		dns_name_fromregion(&name, &region);
		RETERR(dns_name_totext(&name, ISC_FALSE, target));
		isc_region_consume(&region, name_length(&name));
		break;
	}

	/*
	 * Key.
	 */
	if (region.length > 0U) {
		RETERR(isc_str_tobuffer(tctx->linebreak, target));
		if (tctx->width == 0)   /* No splitting */
			RETERR(isc_base64_totext(&region, 60, "", target));
		else
			RETERR(isc_base64_totext(&region, tctx->width - 2,
						 tctx->linebreak, target));
	}

	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(isc_str_tobuffer(" )", target));
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_ipseckey(ARGS_FROMWIRE) {
	dns_name_t name;
	isc_region_t region;

	REQUIRE(type == dns_rdatatype_ipseckey);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_NONE);

	dns_name_init(&name, NULL);

	isc_buffer_activeregion(source, &region);
	if (region.length < 3)
		return (ISC_R_UNEXPECTEDEND);

	switch (region.base[1]) {
	case 0:
		isc_buffer_forward(source, region.length);
		return (isc_mem_tobuffer(target, region.base, region.length));

	case 1:
		if (region.length < 7)
			return (ISC_R_UNEXPECTEDEND);
		isc_buffer_forward(source, region.length);
		return (isc_mem_tobuffer(target, region.base, region.length));

	case 2:
		if (region.length < 19)
			return (ISC_R_UNEXPECTEDEND);
		isc_buffer_forward(source, region.length);
		return (isc_mem_tobuffer(target, region.base, region.length));

	case 3:
		RETERR(isc_mem_tobuffer(target, region.base, 3));
		isc_buffer_forward(source, 3);
		RETERR(dns_name_fromwire(&name, source, dctx, options, target));
		isc_buffer_activeregion(source, &region);
		isc_buffer_forward(source, region.length);
		return(isc_mem_tobuffer(target, region.base, region.length));

	default:
		return (ISC_R_NOTIMPLEMENTED);
	}
}

static inline isc_result_t
towire_ipseckey(ARGS_TOWIRE) {
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_ipseckey);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &region);
	return (isc_mem_tobuffer(target, region.base, region.length));
}

#endif	/* RDATA_GENERIC_IPSECKEY_45_C */

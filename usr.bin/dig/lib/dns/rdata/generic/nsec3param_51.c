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

/* $Id: nsec3param_51.c,v 1.13 2020/02/26 18:49:02 florian Exp $ */

/*
 * Copyright (C) 2004  Nominet, Ltd.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND NOMINET DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* RFC 5155 */

#ifndef RDATA_GENERIC_NSEC3PARAM_51_C
#define RDATA_GENERIC_NSEC3PARAM_51_C

#include <isc/base32.h>

static inline isc_result_t
totext_nsec3param(ARGS_TOTEXT) {
	isc_region_t sr;
	unsigned int i, j;
	unsigned char hash;
	unsigned char flags;
	char buf[sizeof("65535 ")];
	uint32_t iterations;

	REQUIRE(rdata->type == dns_rdatatype_nsec3param);
	REQUIRE(rdata->length != 0);

	UNUSED(tctx);

	dns_rdata_toregion(rdata, &sr);

	hash = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);

	flags = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);

	iterations = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);

	snprintf(buf, sizeof(buf), "%u ", hash);
	RETERR(isc_str_tobuffer(buf, target));

	snprintf(buf, sizeof(buf), "%u ", flags);
	RETERR(isc_str_tobuffer(buf, target));

	snprintf(buf, sizeof(buf), "%u ", iterations);
	RETERR(isc_str_tobuffer(buf, target));

	j = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);
	INSIST(j <= sr.length);

	if (j != 0) {
		i = sr.length;
		sr.length = j;
		RETERR(isc_hex_totext(&sr, 1, "", target));
		sr.length = i - j;
	} else
		RETERR(isc_str_tobuffer("-", target));

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_nsec3param(ARGS_FROMWIRE) {
	isc_region_t sr, rr;
	unsigned int saltlen;

	REQUIRE(type == dns_rdatatype_nsec3param);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(options);
	UNUSED(dctx);

	isc_buffer_activeregion(source, &sr);
	rr = sr;

	/* hash(1), flags(1), iterations(2), saltlen(1) */
	if (sr.length < 5U)
		return (DNS_R_FORMERR);
	saltlen = sr.base[4];
	isc_region_consume(&sr, 5);

	if (sr.length < saltlen)
		return (DNS_R_FORMERR);
	isc_region_consume(&sr, saltlen);
	RETERR(isc_mem_tobuffer(target, rr.base, rr.length));
	isc_buffer_forward(source, rr.length);
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_nsec3param(ARGS_TOWIRE) {
	isc_region_t sr;

	REQUIRE(rdata->type == dns_rdatatype_nsec3param);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &sr);
	return (isc_mem_tobuffer(target, sr.base, sr.length));
}

#endif	/* RDATA_GENERIC_NSEC3PARAM_51_C */

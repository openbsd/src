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

#ifndef RDATA_GENERIC_EUI48_108_C
#define RDATA_GENERIC_EUI48_108_C

#include <string.h>

static inline isc_result_t
totext_eui48(ARGS_TOTEXT) {
	char buf[sizeof("xx-xx-xx-xx-xx-xx")];

	REQUIRE(rdata->type == dns_rdatatype_eui48);
	REQUIRE(rdata->length == 6);

	UNUSED(tctx);

	(void)snprintf(buf, sizeof(buf), "%02x-%02x-%02x-%02x-%02x-%02x",
		       rdata->data[0], rdata->data[1], rdata->data[2],
		       rdata->data[3], rdata->data[4], rdata->data[5]);
	return (isc_str_tobuffer(buf, target));
}

static inline isc_result_t
fromwire_eui48(ARGS_FROMWIRE) {
	isc_region_t sregion;

	REQUIRE(type == dns_rdatatype_eui48);

	UNUSED(type);
	UNUSED(options);
	UNUSED(rdclass);
	UNUSED(dctx);

	isc_buffer_activeregion(source, &sregion);
	if (sregion.length != 6)
		return (DNS_R_FORMERR);
	isc_buffer_forward(source, sregion.length);
	return (isc_mem_tobuffer(target, sregion.base, sregion.length));
}

static inline isc_result_t
towire_eui48(ARGS_TOWIRE) {

	REQUIRE(rdata->type == dns_rdatatype_eui48);
	REQUIRE(rdata->length == 6);

	UNUSED(cctx);

	return (isc_mem_tobuffer(target, rdata->data, rdata->length));
}

#endif	/* RDATA_GENERIC_EUI48_108_C */

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

/* $Id: hinfo_13.c,v 1.13 2020/09/14 08:40:43 florian Exp $ */

/*
 * Reviewed: Wed Mar 15 16:47:10 PST 2000 by halley.
 */

#ifndef RDATA_GENERIC_HINFO_13_C
#define RDATA_GENERIC_HINFO_13_C

static inline isc_result_t
totext_hinfo(ARGS_TOTEXT) {
	isc_region_t region;

	UNUSED(tctx);

	REQUIRE(rdata->type == dns_rdatatype_hinfo);
	REQUIRE(rdata->length != 0);

	dns_rdata_toregion(rdata, &region);
	RETERR(txt_totext(&region, 1, target));
	RETERR(isc_str_tobuffer(" ", target));
	return (txt_totext(&region, 1, target));
}

static inline isc_result_t
fromwire_hinfo(ARGS_FROMWIRE) {

	REQUIRE(type == dns_rdatatype_hinfo);

	UNUSED(type);
	UNUSED(dctx);
	UNUSED(rdclass);
	UNUSED(options);

	RETERR(txt_fromwire(source, target));
	return (txt_fromwire(source, target));
}

static inline isc_result_t
towire_hinfo(ARGS_TOWIRE) {

	UNUSED(cctx);

	REQUIRE(rdata->type == dns_rdatatype_hinfo);
	REQUIRE(rdata->length != 0);

	return (isc_mem_tobuffer(target, rdata->data, rdata->length));
}

#endif	/* RDATA_GENERIC_HINFO_13_C */

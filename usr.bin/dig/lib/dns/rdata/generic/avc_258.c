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

#ifndef RDATA_GENERIC_AVC_258_C
#define RDATA_GENERIC_AVC_258_C

static inline isc_result_t
totext_avc(ARGS_TOTEXT) {

	UNUSED(tctx);

	REQUIRE(rdata->type == dns_rdatatype_avc);

	return (generic_totext_txt(rdata, tctx, target));
}

static inline isc_result_t
fromwire_avc(ARGS_FROMWIRE) {

	REQUIRE(type == dns_rdatatype_avc);

	UNUSED(type);
	UNUSED(dctx);
	UNUSED(rdclass);
	UNUSED(options);

	return (generic_fromwire_txt(rdclass, type, source, dctx, options,
				     target));
}

static inline isc_result_t
towire_avc(ARGS_TOWIRE) {

	REQUIRE(rdata->type == dns_rdatatype_avc);

	UNUSED(cctx);

	return (isc_mem_tobuffer(target, rdata->data, rdata->length));
}

#endif	/* RDATA_GENERIC_AVC_258_C */

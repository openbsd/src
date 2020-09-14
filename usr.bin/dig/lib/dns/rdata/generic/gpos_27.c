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

/* $Id: gpos_27.c,v 1.13 2020/09/14 08:40:43 florian Exp $ */

/* reviewed: Wed Mar 15 16:48:45 PST 2000 by brister */

/* RFC1712 */

#ifndef RDATA_GENERIC_GPOS_27_C
#define RDATA_GENERIC_GPOS_27_C

static inline isc_result_t
totext_gpos(ARGS_TOTEXT) {
	isc_region_t region;
	int i;

	REQUIRE(rdata->type == dns_rdatatype_gpos);
	REQUIRE(rdata->length != 0);

	UNUSED(tctx);

	dns_rdata_toregion(rdata, &region);

	for (i = 0; i < 3; i++) {
		RETERR(txt_totext(&region, 1, target));
		if (i != 2)
			RETERR(isc_str_tobuffer(" ", target));
	}

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_gpos(ARGS_FROMWIRE) {
	int i;

	REQUIRE(type == dns_rdatatype_gpos);

	UNUSED(type);
	UNUSED(dctx);
	UNUSED(rdclass);
	UNUSED(options);

	for (i = 0; i < 3; i++)
		RETERR(txt_fromwire(source, target));
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_gpos(ARGS_TOWIRE) {

	REQUIRE(rdata->type == dns_rdatatype_gpos);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	return (isc_mem_tobuffer(target, rdata->data, rdata->length));
}

#endif	/* RDATA_GENERIC_GPOS_27_C */

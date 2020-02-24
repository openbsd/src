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

/* $Id: spf_99.c,v 1.4 2020/02/24 12:06:14 florian Exp $ */

/* Reviewed: Thu Mar 16 15:40:00 PST 2000 by bwelling */

#ifndef RDATA_GENERIC_SPF_99_C
#define RDATA_GENERIC_SPF_99_C

#define RRTYPE_SPF_ATTRIBUTES (0)

static inline isc_result_t
totext_spf(ARGS_TOTEXT) {

	UNUSED(tctx);

	REQUIRE(rdata->type == dns_rdatatype_spf);

	return (generic_totext_txt(rdata, tctx, target));
}

static inline isc_result_t
fromwire_spf(ARGS_FROMWIRE) {

	REQUIRE(type == dns_rdatatype_spf);

	UNUSED(type);
	UNUSED(dctx);
	UNUSED(rdclass);
	UNUSED(options);

	return (generic_fromwire_txt(rdclass, type, source, dctx, options,
				     target));
}

static inline isc_result_t
towire_spf(ARGS_TOWIRE) {

	REQUIRE(rdata->type == dns_rdatatype_spf);

	UNUSED(cctx);

	return (mem_tobuffer(target, rdata->data, rdata->length));
}


static inline isc_result_t
fromstruct_spf(ARGS_FROMSTRUCT) {

	REQUIRE(type == dns_rdatatype_spf);

	return (generic_fromstruct_txt(rdclass, type, source, target));
}

static inline isc_result_t
tostruct_spf(ARGS_TOSTRUCT) {
	dns_rdata_spf_t *spf = target;

	REQUIRE(rdata->type == dns_rdatatype_spf);
	REQUIRE(target != NULL);

	spf->common.rdclass = rdata->rdclass;
	spf->common.rdtype = rdata->type;
	ISC_LINK_INIT(&spf->common, link);

	return (generic_tostruct_txt(rdata, target));
}

static inline void
freestruct_spf(ARGS_FREESTRUCT) {
	dns_rdata_spf_t *txt = source;

	REQUIRE(source != NULL);
	REQUIRE(txt->common.rdtype == dns_rdatatype_spf);

	generic_freestruct_txt(source);
}

static inline isc_boolean_t
checkowner_spf(ARGS_CHECKOWNER) {

	REQUIRE(type == dns_rdatatype_spf);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

#endif	/* RDATA_GENERIC_SPF_99_C */

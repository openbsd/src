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

/* $Id: nsec3_50.c,v 1.13 2020/09/14 08:40:43 florian Exp $ */

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

#ifndef RDATA_GENERIC_NSEC3_50_C
#define RDATA_GENERIC_NSEC3_50_C

#include <isc/base32.h>

static inline isc_result_t
totext_nsec3(ARGS_TOTEXT) {
	isc_region_t sr;
	unsigned int i, j;
	unsigned char hash;
	unsigned char flags;
	char buf[sizeof("TYPE65535")];
	uint32_t iterations;

	REQUIRE(rdata->type == dns_rdatatype_nsec3);
	REQUIRE(rdata->length != 0);

	dns_rdata_toregion(rdata, &sr);

	/* Hash */
	hash = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);
	snprintf(buf, sizeof(buf), "%u ", hash);
	RETERR(isc_str_tobuffer(buf, target));

	/* Flags */
	flags = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);
	snprintf(buf, sizeof(buf), "%u ", flags);
	RETERR(isc_str_tobuffer(buf, target));

	/* Iterations */
	iterations = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	snprintf(buf, sizeof(buf), "%u ", iterations);
	RETERR(isc_str_tobuffer(buf, target));

	/* Salt */
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

	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(isc_str_tobuffer(" (", target));
	RETERR(isc_str_tobuffer(tctx->linebreak, target));

	/* Next hash */
	j = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);
	INSIST(j <= sr.length);

	i = sr.length;
	sr.length = j;
	RETERR(isc_base32hexnp_totext(&sr, 1, "", target));
	sr.length = i - j;

	/*
	 * Don't leave a trailing space when there's no typemap present.
	 */
	if (((tctx->flags & DNS_STYLEFLAG_MULTILINE) == 0) && (sr.length > 0)) {
		RETERR(isc_str_tobuffer(" ", target));
	}
	RETERR(typemap_totext(&sr, tctx, target));

	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(isc_str_tobuffer(" )", target));

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_nsec3(ARGS_FROMWIRE) {
	isc_region_t sr, rr;
	unsigned int saltlen, hashlen;

	REQUIRE(type == dns_rdatatype_nsec3);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(options);
	UNUSED(dctx);

	isc_buffer_activeregion(source, &sr);
	rr = sr;

	/* hash(1), flags(1), iteration(2), saltlen(1) */
	if (sr.length < 5U)
		return (DNS_R_FORMERR);
	saltlen = sr.base[4];
	isc_region_consume(&sr, 5);

	if (sr.length < saltlen)
		return (DNS_R_FORMERR);
	isc_region_consume(&sr, saltlen);

	if (sr.length < 1U)
		return (DNS_R_FORMERR);
	hashlen = sr.base[0];
	isc_region_consume(&sr, 1);

	if (sr.length < hashlen)
		return (DNS_R_FORMERR);
	isc_region_consume(&sr, hashlen);

	RETERR(typemap_test(&sr, 1));

	RETERR(isc_mem_tobuffer(target, rr.base, rr.length));
	isc_buffer_forward(source, rr.length);
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_nsec3(ARGS_TOWIRE) {
	isc_region_t sr;

	REQUIRE(rdata->type == dns_rdatatype_nsec3);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &sr);
	return (isc_mem_tobuffer(target, sr.base, sr.length));
}

#define NSEC3_MAX_HASH_LENGTH 155
static inline int
checkowner_nsec3(ARGS_CHECKOWNER) {
	unsigned char owner[NSEC3_MAX_HASH_LENGTH];
	isc_buffer_t buffer;
	dns_label_t label;

	REQUIRE(type == dns_rdatatype_nsec3);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	/*
	 * First label is a base32hex string without padding.
	 */
	dns_name_getlabel(name, 0, &label);
	isc_region_consume(&label, 1);
	isc_buffer_init(&buffer, owner, sizeof(owner));
	if (isc_base32hexnp_decoderegion(&label, &buffer) == ISC_R_SUCCESS)
		return (1);

	return (0);
}

#endif	/* RDATA_GENERIC_NSEC3_50_C */

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

/* $Id: afsdb_18.c,v 1.2 2020/02/20 18:08:51 florian Exp $ */

/* Reviewed: Wed Mar 15 14:59:00 PST 2000 by explorer */

/* RFC1183 */

#ifndef RDATA_GENERIC_AFSDB_18_C
#define RDATA_GENERIC_AFSDB_18_C

#define RRTYPE_AFSDB_ATTRIBUTES (0)

static inline isc_result_t
totext_afsdb(ARGS_TOTEXT) {
	dns_name_t name;
	dns_name_t prefix;
	isc_region_t region;
	char buf[sizeof("64000 ")];
	isc_boolean_t sub;
	unsigned int num;

	REQUIRE(rdata->type == dns_rdatatype_afsdb);
	REQUIRE(rdata->length != 0);

	dns_name_init(&name, NULL);
	dns_name_init(&prefix, NULL);

	dns_rdata_toregion(rdata, &region);
	num = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	snprintf(buf, sizeof(buf), "%u ", num);
	RETERR(str_totext(buf, target));
	dns_name_fromregion(&name, &region);
	sub = name_prefix(&name, tctx->origin, &prefix);
	return (dns_name_totext(&prefix, sub, target));
}

static inline isc_result_t
fromwire_afsdb(ARGS_FROMWIRE) {
	dns_name_t name;
	isc_region_t sr;
	isc_region_t tr;

	REQUIRE(type == dns_rdatatype_afsdb);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_NONE);

	dns_name_init(&name, NULL);

	isc_buffer_activeregion(source, &sr);
	isc_buffer_availableregion(target, &tr);
	if (tr.length < 2)
		return (ISC_R_NOSPACE);
	if (sr.length < 2)
		return (ISC_R_UNEXPECTEDEND);
	memmove(tr.base, sr.base, 2);
	isc_buffer_forward(source, 2);
	isc_buffer_add(target, 2);
	return (dns_name_fromwire(&name, source, dctx, options, target));
}

static inline isc_result_t
towire_afsdb(ARGS_TOWIRE) {
	isc_region_t tr;
	isc_region_t sr;
	dns_name_t name;
	dns_offsets_t offsets;

	REQUIRE(rdata->type == dns_rdatatype_afsdb);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_NONE);
	isc_buffer_availableregion(target, &tr);
	dns_rdata_toregion(rdata, &sr);
	if (tr.length < 2)
		return (ISC_R_NOSPACE);
	memmove(tr.base, sr.base, 2);
	isc_region_consume(&sr, 2);
	isc_buffer_add(target, 2);

	dns_name_init(&name, offsets);
	dns_name_fromregion(&name, &sr);

	return (dns_name_towire(&name, cctx, target));
}

static inline int
compare_afsdb(ARGS_COMPARE) {
	int result;
	dns_name_t name1;
	dns_name_t name2;
	isc_region_t region1;
	isc_region_t region2;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == dns_rdatatype_afsdb);
	REQUIRE(rdata1->length != 0);
	REQUIRE(rdata2->length != 0);

	result = memcmp(rdata1->data, rdata2->data, 2);
	if (result != 0)
		return (result < 0 ? -1 : 1);

	dns_name_init(&name1, NULL);
	dns_name_init(&name2, NULL);

	dns_rdata_toregion(rdata1, &region1);
	dns_rdata_toregion(rdata2, &region2);

	isc_region_consume(&region1, 2);
	isc_region_consume(&region2, 2);

	dns_name_fromregion(&name1, &region1);
	dns_name_fromregion(&name2, &region2);

	return (dns_name_rdatacompare(&name1, &name2));
}

static inline isc_result_t
fromstruct_afsdb(ARGS_FROMSTRUCT) {
	dns_rdata_afsdb_t *afsdb = source;
	isc_region_t region;

	REQUIRE(type == dns_rdatatype_afsdb);
	REQUIRE(source != NULL);
	REQUIRE(afsdb->common.rdclass == rdclass);
	REQUIRE(afsdb->common.rdtype == type);

	UNUSED(type);
	UNUSED(rdclass);

	RETERR(uint16_tobuffer(afsdb->subtype, target));
	dns_name_toregion(&afsdb->server, &region);
	return (isc_buffer_copyregion(target, &region));
}

static inline isc_result_t
tostruct_afsdb(ARGS_TOSTRUCT) {
	isc_region_t region;
	dns_rdata_afsdb_t *afsdb = target;
	dns_name_t name;

	REQUIRE(rdata->type == dns_rdatatype_afsdb);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	afsdb->common.rdclass = rdata->rdclass;
	afsdb->common.rdtype = rdata->type;
	ISC_LINK_INIT(&afsdb->common, link);

	dns_name_init(&afsdb->server, NULL);

	dns_rdata_toregion(rdata, &region);

	afsdb->subtype = uint16_fromregion(&region);
	isc_region_consume(&region, 2);

	dns_name_init(&name, NULL);
	dns_name_fromregion(&name, &region);

	RETERR(name_duporclone(&name, &afsdb->server));
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_afsdb(ARGS_FREESTRUCT) {
	dns_rdata_afsdb_t *afsdb = source;

	REQUIRE(source != NULL);
	REQUIRE(afsdb->common.rdtype == dns_rdatatype_afsdb);

	dns_name_free(&afsdb->server);
}

static inline isc_result_t
additionaldata_afsdb(ARGS_ADDLDATA) {
	dns_name_t name;
	dns_offsets_t offsets;
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_afsdb);

	dns_name_init(&name, offsets);
	dns_rdata_toregion(rdata, &region);
	isc_region_consume(&region, 2);
	dns_name_fromregion(&name, &region);

	return ((add)(arg, &name, dns_rdatatype_a));
}

static inline isc_result_t
digest_afsdb(ARGS_DIGEST) {
	isc_region_t r1, r2;
	dns_name_t name;

	REQUIRE(rdata->type == dns_rdatatype_afsdb);

	dns_rdata_toregion(rdata, &r1);
	r2 = r1;
	isc_region_consume(&r2, 2);
	r1.length = 2;
	RETERR((digest)(arg, &r1));
	dns_name_init(&name, NULL);
	dns_name_fromregion(&name, &r2);

	return (dns_name_digest(&name, digest, arg));
}

static inline isc_boolean_t
checkowner_afsdb(ARGS_CHECKOWNER) {

	REQUIRE(type == dns_rdatatype_afsdb);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

static inline isc_boolean_t
checknames_afsdb(ARGS_CHECKNAMES) {
	isc_region_t region;
	dns_name_t name;

	REQUIRE(rdata->type == dns_rdatatype_afsdb);

	UNUSED(owner);

	dns_rdata_toregion(rdata, &region);
	isc_region_consume(&region, 2);
	dns_name_init(&name, NULL);
	dns_name_fromregion(&name, &region);
	if (!dns_name_ishostname(&name, ISC_FALSE)) {
		if (bad != NULL)
			dns_name_clone(&name, bad);
		return (ISC_FALSE);
	}
	return (ISC_TRUE);
}

static inline int
casecompare_afsdb(ARGS_COMPARE) {
	return (compare_afsdb(rdata1, rdata2));
}
#endif	/* RDATA_GENERIC_AFSDB_18_C */

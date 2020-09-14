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

/* $Id: soa_6.c,v 1.9 2020/09/14 08:40:43 florian Exp $ */

/* Reviewed: Thu Mar 16 15:18:32 PST 2000 by explorer */

#ifndef RDATA_GENERIC_SOA_6_C
#define RDATA_GENERIC_SOA_6_C

static const char *soa_fieldnames[5] = {
	"serial", "refresh", "retry", "expire", "minimum"
};

static inline isc_result_t
totext_soa(ARGS_TOTEXT) {
	isc_region_t dregion;
	dns_name_t mname;
	dns_name_t rname;
	dns_name_t prefix;
	int sub;
	int i;
	int multiline;
	int comm;

	REQUIRE(rdata->type == dns_rdatatype_soa);
	REQUIRE(rdata->length != 0);

	multiline = (tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0;
	if (multiline)
		comm = (tctx->flags & DNS_STYLEFLAG_RRCOMMENT) != 0;
	else
		comm = 0;

	dns_name_init(&mname, NULL);
	dns_name_init(&rname, NULL);
	dns_name_init(&prefix, NULL);

	dns_rdata_toregion(rdata, &dregion);

	dns_name_fromregion(&mname, &dregion);
	isc_region_consume(&dregion, name_length(&mname));

	dns_name_fromregion(&rname, &dregion);
	isc_region_consume(&dregion, name_length(&rname));

	sub = name_prefix(&mname, tctx->origin, &prefix);
	RETERR(dns_name_totext(&prefix, sub, target));

	RETERR(isc_str_tobuffer(" ", target));

	sub = name_prefix(&rname, tctx->origin, &prefix);
	RETERR(dns_name_totext(&prefix, sub, target));

	if (multiline)
		RETERR(isc_str_tobuffer(" (" , target));
	RETERR(isc_str_tobuffer(tctx->linebreak, target));

	for (i = 0; i < 5; i++) {
		char buf[sizeof("0123456789 ; ")];
		unsigned long num;
		num = uint32_fromregion(&dregion);
		isc_region_consume(&dregion, 4);
		snprintf(buf, sizeof(buf), comm ? "%-10lu ; " : "%lu", num);
		RETERR(isc_str_tobuffer(buf, target));
		if (comm) {
			RETERR(isc_str_tobuffer(soa_fieldnames[i], target));
			/* Print times in week/day/hour/minute/second form */
			if (i >= 1) {
				RETERR(isc_str_tobuffer(" (", target));
				RETERR(dns_ttl_totext(num, 1, target));
				RETERR(isc_str_tobuffer(")", target));
			}
			RETERR(isc_str_tobuffer(tctx->linebreak, target));
		} else if (i < 4) {
			RETERR(isc_str_tobuffer(tctx->linebreak, target));
		}
	}

	if (multiline)
		RETERR(isc_str_tobuffer(")", target));

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_soa(ARGS_FROMWIRE) {
	dns_name_t mname;
	dns_name_t rname;
	isc_region_t sregion;
	isc_region_t tregion;

	REQUIRE(type == dns_rdatatype_soa);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_GLOBAL14);

	dns_name_init(&mname, NULL);
	dns_name_init(&rname, NULL);

	RETERR(dns_name_fromwire(&mname, source, dctx, options, target));
	RETERR(dns_name_fromwire(&rname, source, dctx, options, target));

	isc_buffer_activeregion(source, &sregion);
	isc_buffer_availableregion(target, &tregion);

	if (sregion.length < 20)
		return (ISC_R_UNEXPECTEDEND);
	if (tregion.length < 20)
		return (ISC_R_NOSPACE);

	memmove(tregion.base, sregion.base, 20);
	isc_buffer_forward(source, 20);
	isc_buffer_add(target, 20);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_soa(ARGS_TOWIRE) {
	isc_region_t sregion;
	isc_region_t tregion;
	dns_name_t mname;
	dns_name_t rname;
	dns_offsets_t moffsets;
	dns_offsets_t roffsets;

	REQUIRE(rdata->type == dns_rdatatype_soa);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_GLOBAL14);

	dns_name_init(&mname, moffsets);
	dns_name_init(&rname, roffsets);

	dns_rdata_toregion(rdata, &sregion);

	dns_name_fromregion(&mname, &sregion);
	isc_region_consume(&sregion, name_length(&mname));
	RETERR(dns_name_towire(&mname, cctx, target));

	dns_name_fromregion(&rname, &sregion);
	isc_region_consume(&sregion, name_length(&rname));
	RETERR(dns_name_towire(&rname, cctx, target));

	isc_buffer_availableregion(target, &tregion);
	if (tregion.length < 20)
		return (ISC_R_NOSPACE);

	memmove(tregion.base, sregion.base, 20);
	isc_buffer_add(target, 20);
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromstruct_soa(ARGS_FROMSTRUCT) {
	dns_rdata_soa_t *soa = source;
	isc_region_t region;

	REQUIRE(type == dns_rdatatype_soa);
	REQUIRE(source != NULL);
	REQUIRE(soa->common.rdtype == type);
	REQUIRE(soa->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	dns_name_toregion(&soa->origin, &region);
	RETERR(isc_buffer_copyregion(target, &region));
	dns_name_toregion(&soa->contact, &region);
	RETERR(isc_buffer_copyregion(target, &region));
	RETERR(uint32_tobuffer(soa->serial, target));
	RETERR(uint32_tobuffer(soa->refresh, target));
	RETERR(uint32_tobuffer(soa->retry, target));
	RETERR(uint32_tobuffer(soa->expire, target));
	return (uint32_tobuffer(soa->minimum, target));
}

static inline isc_result_t
tostruct_soa(ARGS_TOSTRUCT) {
	isc_region_t region;
	dns_rdata_soa_t *soa = target;
	dns_name_t name;
	isc_result_t result;

	REQUIRE(rdata->type == dns_rdatatype_soa);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	soa->common.rdclass = rdata->rdclass;
	soa->common.rdtype = rdata->type;
	ISC_LINK_INIT(&soa->common, link);

	dns_rdata_toregion(rdata, &region);

	dns_name_init(&name, NULL);
	dns_name_fromregion(&name, &region);
	isc_region_consume(&region, name_length(&name));
	dns_name_init(&soa->origin, NULL);
	RETERR(name_duporclone(&name, &soa->origin));

	dns_name_fromregion(&name, &region);
	isc_region_consume(&region, name_length(&name));
	dns_name_init(&soa->contact, NULL);
	result = name_duporclone(&name, &soa->contact);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	soa->serial = uint32_fromregion(&region);
	isc_region_consume(&region, 4);

	soa->refresh = uint32_fromregion(&region);
	isc_region_consume(&region, 4);

	soa->retry = uint32_fromregion(&region);
	isc_region_consume(&region, 4);

	soa->expire = uint32_fromregion(&region);
	isc_region_consume(&region, 4);

	soa->minimum = uint32_fromregion(&region);

	return (ISC_R_SUCCESS);

 cleanup:
	dns_name_free(&soa->origin);
	return (ISC_R_NOMEMORY);
}

static inline void
freestruct_soa(ARGS_FREESTRUCT) {
	dns_rdata_soa_t *soa = source;

	REQUIRE(source != NULL);
	REQUIRE(soa->common.rdtype == dns_rdatatype_soa);

	dns_name_free(&soa->origin);
	dns_name_free(&soa->contact);
}

#endif	/* RDATA_GENERIC_SOA_6_C */

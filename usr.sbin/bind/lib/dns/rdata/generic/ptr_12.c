/*
 * Copyright (C) 1998-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: ptr_12.c,v 1.39 2001/07/16 03:06:26 marka Exp $ */

/* Reviewed: Thu Mar 16 14:05:12 PST 2000 by explorer */

#ifndef RDATA_GENERIC_PTR_12_C
#define RDATA_GENERIC_PTR_12_C

#define RRTYPE_PTR_ATTRIBUTES (0)

static inline isc_result_t
fromtext_ptr(ARGS_FROMTEXT) {
	isc_token_t token;
	dns_name_t name;
	isc_buffer_t buffer;

	REQUIRE(type == 12);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(callbacks);

	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_string,
				      ISC_FALSE));

	dns_name_init(&name, NULL);
	buffer_fromregion(&buffer, &token.value.as_region);
	origin = (origin != NULL) ? origin : dns_rootname;
	RETTOK(dns_name_fromtext(&name, &buffer, origin, downcase, target));
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
totext_ptr(ARGS_TOTEXT) {
	isc_region_t region;
	dns_name_t name;
	dns_name_t prefix;
	isc_boolean_t sub;

	REQUIRE(rdata->type == 12);
	REQUIRE(rdata->length != 0);

	dns_name_init(&name, NULL);
	dns_name_init(&prefix, NULL);

	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);

	sub = name_prefix(&name, tctx->origin, &prefix);

	return (dns_name_totext(&prefix, sub, target));
}

static inline isc_result_t
fromwire_ptr(ARGS_FROMWIRE) {
        dns_name_t name;

	REQUIRE(type == 12);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_GLOBAL14);

        dns_name_init(&name, NULL);
        return (dns_name_fromwire(&name, source, dctx, downcase, target));
}

static inline isc_result_t
towire_ptr(ARGS_TOWIRE) {
	dns_name_t name;
	dns_offsets_t offsets;
	isc_region_t region;

	REQUIRE(rdata->type == 12);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_GLOBAL14);

	dns_name_init(&name, offsets);
	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);

	return (dns_name_towire(&name, cctx, target));
}

static inline int
compare_ptr(ARGS_COMPARE) {
	dns_name_t name1;
	dns_name_t name2;
	isc_region_t region1;
	isc_region_t region2;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == 12);
	REQUIRE(rdata1->length != 0);
	REQUIRE(rdata2->length != 0);

	dns_name_init(&name1, NULL);
	dns_name_init(&name2, NULL);

	dns_rdata_toregion(rdata1, &region1);
	dns_rdata_toregion(rdata2, &region2);

	dns_name_fromregion(&name1, &region1);
	dns_name_fromregion(&name2, &region2);

	return (dns_name_rdatacompare(&name1, &name2));
}

static inline isc_result_t
fromstruct_ptr(ARGS_FROMSTRUCT) {
	dns_rdata_ptr_t *ptr = source;
	isc_region_t region;

	REQUIRE(type == 12);
	REQUIRE(source != NULL);
	REQUIRE(ptr->common.rdtype == type);
	REQUIRE(ptr->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	dns_name_toregion(&ptr->ptr, &region);
	return (isc_buffer_copyregion(target, &region));
}

static inline isc_result_t
tostruct_ptr(ARGS_TOSTRUCT) {
	isc_region_t region;
	dns_rdata_ptr_t *ptr = target;
	dns_name_t name;

	REQUIRE(rdata->type == 12);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	ptr->common.rdclass = rdata->rdclass;
	ptr->common.rdtype = rdata->type;
	ISC_LINK_INIT(&ptr->common, link);

	dns_name_init(&name, NULL);
	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);
	dns_name_init(&ptr->ptr, NULL);
	RETERR(name_duporclone(&name, mctx, &ptr->ptr));
	ptr->mctx = mctx;
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_ptr(ARGS_FREESTRUCT) {
	dns_rdata_ptr_t *ptr = source;

	REQUIRE(source != NULL);
	REQUIRE(ptr->common.rdtype == 12);

	if (ptr->mctx == NULL)
		return;

	dns_name_free(&ptr->ptr, ptr->mctx);
	ptr->mctx = NULL;
}

static inline isc_result_t
additionaldata_ptr(ARGS_ADDLDATA) {
	REQUIRE(rdata->type == 12);

	UNUSED(rdata);
	UNUSED(add);
	UNUSED(arg);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
digest_ptr(ARGS_DIGEST) {
	isc_region_t r;
	dns_name_t name;

	REQUIRE(rdata->type == 12);

	dns_rdata_toregion(rdata, &r);
	dns_name_init(&name, NULL);
	dns_name_fromregion(&name, &r);

	return (dns_name_digest(&name, digest, arg));
}

#endif	/* RDATA_GENERIC_PTR_12_C */

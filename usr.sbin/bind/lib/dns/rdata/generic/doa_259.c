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

#ifndef RDATA_GENERIC_DOA_259_C
#define RDATA_GENERIC_DOA_259_C

#define RRTYPE_DOA_ATTRIBUTES (0)

static inline isc_result_t
fromtext_doa(ARGS_FROMTEXT) {
	isc_token_t token;

	REQUIRE(type == dns_rdatatype_doa);

	UNUSED(rdclass);
	UNUSED(origin);
	UNUSED(options);
	UNUSED(callbacks);

	/*
	 * DOA-ENTERPRISE
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_number,
				      ISC_FALSE));
	RETERR(uint32_tobuffer(token.value.as_ulong, target));

	/*
	 * DOA-TYPE
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_number,
				      ISC_FALSE));
	RETERR(uint32_tobuffer(token.value.as_ulong, target));

	/*
	 * DOA-LOCATION
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_number,
				      ISC_FALSE));
	if (token.value.as_ulong > 0xffU) {
		RETTOK(ISC_R_RANGE);
	}
	RETERR(uint8_tobuffer(token.value.as_ulong, target));

	/*
	 * DOA-MEDIA-TYPE
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_qstring,
				      ISC_FALSE));
	RETTOK(txt_fromtext(&token.value.as_textregion, target));

	/*
	 * DOA-DATA
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_string,
				      ISC_FALSE));
	if (strcmp(DNS_AS_STR(token), "-") == 0) {
		return (ISC_R_SUCCESS);
	} else {
		isc_lex_ungettoken(lexer, &token);
		return (isc_base64_tobuffer(lexer, target, -1));
	}
}

static inline isc_result_t
totext_doa(ARGS_TOTEXT) {
	char buf[sizeof("4294967295 ")];
	isc_region_t region;
	uint32_t n;

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_doa);
	REQUIRE(rdata->length != 0);

	UNUSED(tctx);

	dns_rdata_toregion(rdata, &region);

	/*
	 * DOA-ENTERPRISE
	 */
	n = uint32_fromregion(&region);
	isc_region_consume(&region, 4);
	snprintf(buf, sizeof(buf), "%u ", n);
	RETERR(str_totext(buf, target));

	/*
	 * DOA-TYPE
	 */
	n = uint32_fromregion(&region);
	isc_region_consume(&region, 4);
	snprintf(buf, sizeof(buf), "%u ", n);
	RETERR(str_totext(buf, target));

	/*
	 * DOA-LOCATION
	 */
	n = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	snprintf(buf, sizeof(buf), "%u ", n);
	RETERR(str_totext(buf, target));

	/*
	 * DOA-MEDIA-TYPE
	 */
	RETERR(txt_totext(&region, ISC_TRUE, target));
	RETERR(str_totext(" ", target));

	/*
	 * DOA-DATA
	 */
	if (region.length == 0) {
		return (str_totext("-", target));
	} else {
		return (isc_base64_totext(&region, 60, "", target));
	}
}

static inline isc_result_t
fromwire_doa(ARGS_FROMWIRE) {
	isc_region_t region;

	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	REQUIRE(type == dns_rdatatype_doa);

	isc_buffer_activeregion(source, &region);
	/*
	 * DOA-MEDIA-TYPE may be an empty <character-string> (i.e.,
	 * comprising of just the length octet) and DOA-DATA can have
	 * zero length.
	 */
	if (region.length < 4 + 4 + 1 + 1) {
		return (ISC_R_UNEXPECTEDEND);
	}

	/*
	 * Check whether DOA-MEDIA-TYPE length is not malformed.
	 */
	if (region.base[9] > region.length - 10) {
		return (ISC_R_UNEXPECTEDEND);
	}

	isc_buffer_forward(source, region.length);
	return (mem_tobuffer(target, region.base, region.length));
}

static inline isc_result_t
towire_doa(ARGS_TOWIRE) {
	isc_region_t region;

	UNUSED(cctx);

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_doa);
	REQUIRE(rdata->length != 0);

	dns_rdata_toregion(rdata, &region);
	return (mem_tobuffer(target, region.base, region.length));
}

static inline int
compare_doa(ARGS_COMPARE) {
	isc_region_t r1;
	isc_region_t r2;

	REQUIRE(rdata1 != NULL);
	REQUIRE(rdata2 != NULL);
	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->type == dns_rdatatype_doa);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->length != 0);
	REQUIRE(rdata2->length != 0);

	dns_rdata_toregion(rdata1, &r1);
	dns_rdata_toregion(rdata2, &r2);
	return (isc_region_compare(&r1, &r2));
}

static inline isc_result_t
fromstruct_doa(ARGS_FROMSTRUCT) {
	dns_rdata_doa_t *doa = source;

	REQUIRE(type == dns_rdatatype_doa);
	REQUIRE(source != NULL);
	REQUIRE(doa->common.rdtype == dns_rdatatype_doa);
	REQUIRE(doa->common.rdclass == rdclass);

	RETERR(uint32_tobuffer(doa->enterprise, target));
	RETERR(uint32_tobuffer(doa->type, target));
	RETERR(uint8_tobuffer(doa->location, target));
	RETERR(uint8_tobuffer(doa->mediatype_len, target));
	RETERR(mem_tobuffer(target, doa->mediatype, doa->mediatype_len));
	return (mem_tobuffer(target, doa->data, doa->data_len));
}

static inline isc_result_t
tostruct_doa(ARGS_TOSTRUCT) {
	dns_rdata_doa_t *doa = target;
	isc_region_t region;

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_doa);
	REQUIRE(rdata->length != 0);

	doa->common.rdclass = rdata->rdclass;
	doa->common.rdtype = rdata->type;
	ISC_LINK_INIT(&doa->common, link);

	dns_rdata_toregion(rdata, &region);

	/*
	 * DOA-ENTERPRISE
	 */
	if (region.length < 4) {
		return (ISC_R_UNEXPECTEDEND);
	}
	doa->enterprise = uint32_fromregion(&region);
	isc_region_consume(&region, 4);

	/*
	 * DOA-TYPE
	 */
	if (region.length < 4) {
		return (ISC_R_UNEXPECTEDEND);
	}
	doa->type = uint32_fromregion(&region);
	isc_region_consume(&region, 4);

	/*
	 * DOA-LOCATION
	 */
	if (region.length < 1) {
		return (ISC_R_UNEXPECTEDEND);
	}
	doa->location = uint8_fromregion(&region);
	isc_region_consume(&region, 1);

	/*
	 * DOA-MEDIA-TYPE
	 */
	if (region.length < 1) {
		return (ISC_R_UNEXPECTEDEND);
	}
	doa->mediatype_len = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	INSIST(doa->mediatype_len <= region.length);
	doa->mediatype = mem_maybedup(mctx, region.base, doa->mediatype_len);
	if (doa->mediatype == NULL) {
		goto cleanup;
	}
	isc_region_consume(&region, doa->mediatype_len);

	/*
	 * DOA-DATA
	 */
	doa->data_len = region.length;
	doa->data = NULL;
	if (doa->data_len > 0) {
		doa->data = mem_maybedup(mctx, region.base, doa->data_len);
		if (doa->data == NULL) {
			goto cleanup;
		}
		isc_region_consume(&region, doa->data_len);
	}

	doa->mctx = mctx;

	return (ISC_R_SUCCESS);

cleanup:
	if (mctx != NULL && doa->mediatype != NULL) {
		isc_mem_free(mctx, doa->mediatype);
	}
	return (ISC_R_NOMEMORY);
}

static inline void
freestruct_doa(ARGS_FREESTRUCT) {
	dns_rdata_doa_t *doa = source;

	REQUIRE(source != NULL);
	REQUIRE(doa->common.rdtype == dns_rdatatype_doa);

	if (doa->mctx == NULL) {
		return;
	}

	if (doa->mediatype != NULL) {
		isc_mem_free(doa->mctx, doa->mediatype);
	}
	if (doa->data != NULL) {
		isc_mem_free(doa->mctx, doa->data);
	}

	doa->mctx = NULL;
}

static inline isc_result_t
additionaldata_doa(ARGS_ADDLDATA) {
	UNUSED(rdata);
	UNUSED(add);
	UNUSED(arg);

	REQUIRE(rdata->type == dns_rdatatype_doa);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
digest_doa(ARGS_DIGEST) {
	isc_region_t r;

	REQUIRE(rdata->type == dns_rdatatype_doa);

	dns_rdata_toregion(rdata, &r);

	return ((digest)(arg, &r));
}

static inline isc_boolean_t
checkowner_doa(ARGS_CHECKOWNER) {
	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	REQUIRE(type == dns_rdatatype_doa);

	return (ISC_TRUE);
}

static inline isc_boolean_t
checknames_doa(ARGS_CHECKNAMES) {
	UNUSED(rdata);
	UNUSED(owner);
	UNUSED(bad);

	REQUIRE(rdata->type == dns_rdatatype_doa);

	return (ISC_TRUE);
}

static inline int
casecompare_doa(ARGS_COMPARE) {
	return (compare_doa(rdata1, rdata2));
}

#endif	/* RDATA_GENERIC_DOA_259_C */

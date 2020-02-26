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

/* Reviewed: Thu Mar 16 14:06:44 PST 2000 by gson */

/* RFC2671 */

#ifndef RDATA_GENERIC_OPT_41_C
#define RDATA_GENERIC_OPT_41_C

static inline isc_result_t
totext_opt(ARGS_TOTEXT) {
	isc_region_t r;
	isc_region_t or;
	uint16_t option;
	uint16_t length;
	char buf[sizeof("64000 64000")];

	/*
	 * OPT records do not have a text format.
	 */

	REQUIRE(rdata->type == dns_rdatatype_opt);

	dns_rdata_toregion(rdata, &r);
	while (r.length > 0) {
		option = uint16_fromregion(&r);
		isc_region_consume(&r, 2);
		length = uint16_fromregion(&r);
		isc_region_consume(&r, 2);
		snprintf(buf, sizeof(buf), "%u %u", option, length);
		RETERR(isc_str_tobuffer(buf, target));
		INSIST(r.length >= length);
		if (length > 0) {
			if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
				RETERR(isc_str_tobuffer(" (", target));
			RETERR(isc_str_tobuffer(tctx->linebreak, target));
			or = r;
			or.length = length;
			if (tctx->width == 0)   /* No splitting */
				RETERR(isc_base64_totext(&or, 60, "", target));
			else
				RETERR(isc_base64_totext(&or, tctx->width - 2,
							 tctx->linebreak,
							 target));
			isc_region_consume(&r, length);
			if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
				RETERR(isc_str_tobuffer(" )", target));
		}
		if (r.length > 0)
			RETERR(isc_str_tobuffer(" ", target));
	}

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_opt(ARGS_FROMWIRE) {
	isc_region_t sregion;
	isc_region_t tregion;
	uint16_t opt;
	uint16_t length;
	unsigned int total;

	REQUIRE(type == dns_rdatatype_opt);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	isc_buffer_activeregion(source, &sregion);
	if (sregion.length == 0)
		return (ISC_R_SUCCESS);
	total = 0;
	while (sregion.length != 0) {
		if (sregion.length < 4)
			return (ISC_R_UNEXPECTEDEND);
		opt = uint16_fromregion(&sregion);
		isc_region_consume(&sregion, 2);
		length = uint16_fromregion(&sregion);
		isc_region_consume(&sregion, 2);
		total += 4;
		if (sregion.length < length)
			return (ISC_R_UNEXPECTEDEND);
		switch (opt) {
		case DNS_OPT_CLIENT_SUBNET: {
			uint16_t family;
			uint8_t addrlen;
			uint8_t scope;
			uint8_t addrbytes;

			if (length < 4)
				return (DNS_R_OPTERR);
			family = uint16_fromregion(&sregion);
			isc_region_consume(&sregion, 2);
			addrlen = uint8_fromregion(&sregion);
			isc_region_consume(&sregion, 1);
			scope = uint8_fromregion(&sregion);
			isc_region_consume(&sregion, 1);

			switch (family) {
			case 0:
				/*
				 * XXXMUKS: In queries and replies, if
				 * FAMILY is set to 0, SOURCE
				 * PREFIX-LENGTH and SCOPE PREFIX-LENGTH
				 * must be 0 and ADDRESS should not be
				 * present as the address and prefix
				 * lengths don't make sense because the
				 * family is unknown.
				 */
				if (addrlen != 0U || scope != 0U)
					return (DNS_R_OPTERR);
				break;
			case 1:
				if (addrlen > 32U || scope > 32U)
					return (DNS_R_OPTERR);
				break;
			case 2:
				if (addrlen > 128U || scope > 128U)
					return (DNS_R_OPTERR);
				break;
			default:
				return (DNS_R_OPTERR);
			}
			addrbytes = (addrlen + 7) / 8;
			if (addrbytes + 4 != length)
				return (DNS_R_OPTERR);

			if (addrbytes != 0U && (addrlen % 8) != 0) {
				uint8_t bits = ~0U << (8 - (addrlen % 8));
				bits &= sregion.base[addrbytes - 1];
				if (bits != sregion.base[addrbytes - 1])
					return (DNS_R_OPTERR);
			}
			isc_region_consume(&sregion, addrbytes);
			break;
		}
		case DNS_OPT_EXPIRE:
			/*
			 * Request has zero length.  Response is 32 bits.
			 */
			if (length != 0 && length != 4)
				return (DNS_R_OPTERR);
			isc_region_consume(&sregion, length);
			break;
		case DNS_OPT_COOKIE:
			if (length != 8 && (length < 16 || length > 40))
				return (DNS_R_OPTERR);
			isc_region_consume(&sregion, length);
			break;
		case DNS_OPT_KEY_TAG:
			if (length == 0 || (length % 2) != 0)
				return (DNS_R_OPTERR);
			isc_region_consume(&sregion, length);
			break;
		default:
			isc_region_consume(&sregion, length);
			break;
		}
		total += length;
	}

	isc_buffer_activeregion(source, &sregion);
	isc_buffer_availableregion(target, &tregion);
	if (tregion.length < total)
		return (ISC_R_NOSPACE);
	memmove(tregion.base, sregion.base, total);
	isc_buffer_forward(source, total);
	isc_buffer_add(target, total);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_opt(ARGS_TOWIRE) {

	REQUIRE(rdata->type == dns_rdatatype_opt);

	UNUSED(cctx);

	return (isc_mem_tobuffer(target, rdata->data, rdata->length));
}

#endif	/* RDATA_GENERIC_OPT_41_C */

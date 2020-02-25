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

#ifndef GENERIC_KEYDATA_65533_C
#define GENERIC_KEYDATA_65533_C 1

#include <time.h>

#include <dst/dst.h>

#define RRTYPE_KEYDATA_ATTRIBUTES (0)

/*
 * ISC_FORMATHTTPTIMESTAMP_SIZE needs to be 30 in C locale and potentially
 * more for other locales to handle longer national abbreviations when
 * expanding strftime's %a and %b.
 */
#define ISC_FORMATHTTPTIMESTAMP_SIZE 50

static void
isc_time_formathttptimestamp(const struct timespec *t, char *buf, size_t len) {
	size_t flen;
	/*
	 * 5 spaces, 1 comma, 3 GMT, 2 %d, 4 %Y, 8 %H:%M:%S, 3+ %a, 3+ %b (29+)
	 */

	flen = strftime(buf, len, "%a, %d %b %Y %H:%M:%S GMT",
	    gmtime(&t->tv_sec));
	INSIST(flen < len);
}

static inline isc_result_t
totext_keydata(ARGS_TOTEXT) {
	isc_region_t sr;
	char buf[sizeof("64000")];
	unsigned int flags;
	unsigned char algorithm;
	unsigned long refresh, add, deltime;
	char algbuf[DNS_NAME_FORMATSIZE];
	const char *keyinfo;

	REQUIRE(rdata->type == dns_rdatatype_keydata);

	if ((tctx->flags & DNS_STYLEFLAG_KEYDATA) == 0 || rdata->length < 16)
		return (unknown_totext(rdata, tctx, target));

	dns_rdata_toregion(rdata, &sr);

	/* refresh timer */
	refresh = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);
	RETERR(dns_time32_totext(refresh, target));
	RETERR(str_totext(" ", target));

	/* add hold-down */
	add = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);
	RETERR(dns_time32_totext(add, target));
	RETERR(str_totext(" ", target));

	/* remove hold-down */
	deltime = uint32_fromregion(&sr);
	isc_region_consume(&sr, 4);
	RETERR(dns_time32_totext(deltime, target));
	RETERR(str_totext(" ", target));

	/* flags */
	flags = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	snprintf(buf, sizeof(buf), "%u", flags);
	RETERR(str_totext(buf, target));
	RETERR(str_totext(" ", target));
	if ((flags & DNS_KEYFLAG_KSK) != 0) {
		if (flags & DNS_KEYFLAG_REVOKE)
			keyinfo = "revoked KSK";
		else
			keyinfo = "KSK";
	} else
		keyinfo = "ZSK";

	/* protocol */
	snprintf(buf, sizeof(buf), "%u", sr.base[0]);
	isc_region_consume(&sr, 1);
	RETERR(str_totext(buf, target));
	RETERR(str_totext(" ", target));

	/* algorithm */
	algorithm = sr.base[0];
	snprintf(buf, sizeof(buf), "%u", algorithm);
	isc_region_consume(&sr, 1);
	RETERR(str_totext(buf, target));

	/* No Key? */
	if ((flags & 0xc000) == 0xc000)
		return (ISC_R_SUCCESS);

	/* key */
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext(" (", target));
	RETERR(str_totext(tctx->linebreak, target));
	if (tctx->width == 0)   /* No splitting */
		RETERR(isc_base64_totext(&sr, 60, "", target));
	else
		RETERR(isc_base64_totext(&sr, tctx->width - 2,
					 tctx->linebreak, target));

	if ((tctx->flags & DNS_STYLEFLAG_RRCOMMENT) != 0)
		RETERR(str_totext(tctx->linebreak, target));
	else if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext(" ", target));

	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext(")", target));

	if ((tctx->flags & DNS_STYLEFLAG_RRCOMMENT) != 0) {
		isc_region_t tmpr;
		char rbuf[ISC_FORMATHTTPTIMESTAMP_SIZE];
		char abuf[ISC_FORMATHTTPTIMESTAMP_SIZE];
		char dbuf[ISC_FORMATHTTPTIMESTAMP_SIZE];
		struct timespec t;

		RETERR(str_totext(" ; ", target));
		RETERR(str_totext(keyinfo, target));
		dns_secalg_format((dns_secalg_t) algorithm, algbuf,
				  sizeof(algbuf));
		RETERR(str_totext("; alg = ", target));
		RETERR(str_totext(algbuf, target));
		RETERR(str_totext("; key id = ", target));
		dns_rdata_toregion(rdata, &tmpr);
		/* Skip over refresh, addhd, and removehd */
		isc_region_consume(&tmpr, 12);
		snprintf(buf, sizeof(buf), "%u",
			 dst_region_computeid(&tmpr, algorithm));
		RETERR(str_totext(buf, target));

		if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0) {
			time_t now;

			time(&now);

			RETERR(str_totext(tctx->linebreak, target));
			RETERR(str_totext("; next refresh: ", target));
			t.tv_sec = refresh;
			t.tv_nsec = 0;
			isc_time_formathttptimestamp(&t, rbuf, sizeof(rbuf));
			RETERR(str_totext(rbuf, target));

			if (add == 0U) {
				RETERR(str_totext(tctx->linebreak, target));
				RETERR(str_totext("; no trust", target));
			} else {
				RETERR(str_totext(tctx->linebreak, target));
				if ((time_t)add < now) {
					RETERR(str_totext("; trusted since: ",
							  target));
				} else {
					RETERR(str_totext("; trust pending: ",
							  target));
				}
				t.tv_sec = add;
				t.tv_nsec = 0;
				isc_time_formathttptimestamp(&t, abuf,
							     sizeof(abuf));
				RETERR(str_totext(abuf, target));
			}

			if (deltime != 0U) {
				RETERR(str_totext(tctx->linebreak, target));
				RETERR(str_totext("; removal pending: ",
						  target));
				t.tv_sec = deltime;
				t.tv_nsec = 0;
				isc_time_formathttptimestamp(&t, dbuf,
							     sizeof(dbuf));
				RETERR(str_totext(dbuf, target));
			}
		}

	}
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_keydata(ARGS_FROMWIRE) {
	isc_region_t sr;

	REQUIRE(type == dns_rdatatype_keydata);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	isc_buffer_activeregion(source, &sr);
	isc_buffer_forward(source, sr.length);
	return (mem_tobuffer(target, sr.base, sr.length));
}

static inline isc_result_t
towire_keydata(ARGS_TOWIRE) {
	isc_region_t sr;

	REQUIRE(rdata->type == dns_rdatatype_keydata);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &sr);
	return (mem_tobuffer(target, sr.base, sr.length));
}

#endif /* GENERIC_KEYDATA_65533_C */

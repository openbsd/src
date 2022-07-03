/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2022 Florian Obser <florian@openbsd.org>
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

/* $Id: svcb_64.c,v 1.1 2022/07/03 12:07:52 florian Exp $ */

/* draft-ietf-dnsop-svcb-https-10, based on srv_33.c */

#ifndef RDATA_IN_1_SVCB_64_C
#define RDATA_IN_1_SVCB_64_C

#define SVC_PARAM_MANDATORY	0
#define SVC_PARAM_ALPN		1
#define SVC_PARAM_NO_DEF_ALPN	2
#define SVC_PARAM_PORT		3
#define SVC_PARAM_IPV4HINT	4
#define SVC_PARAM_ECH		5
#define SVC_PARAM_IPV6HINT	6
#define SVC_PARAM_DOHPATH	7

static inline const char*
svc_param_key_to_text(uint16_t key)
{
	static char buf[sizeof "key65535"];

	switch (key) {
	case SVC_PARAM_MANDATORY:
		return ("mandatory");
	case SVC_PARAM_ALPN:
		return ("alpn");
	case SVC_PARAM_NO_DEF_ALPN:
		return ("no-default-alpn");
	case SVC_PARAM_PORT:
		return ("port");
	case SVC_PARAM_IPV4HINT:
		return ("ipv4hint");
	case SVC_PARAM_ECH:
		return ("ech");
	case SVC_PARAM_IPV6HINT:
		return ("ipv6hint");
	case SVC_PARAM_DOHPATH:
		return ("dohpath");
	default:
		snprintf(buf, sizeof buf, "key%u", key);
		return (buf);
	}
}

static inline isc_result_t
totext_in_svcb_https(ARGS_TOTEXT) {
	isc_region_t region;
	dns_name_t name;
	dns_name_t prefix;
	int sub;
	char buf[sizeof "xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255"];
	unsigned short num;

	dns_name_init(&name, NULL);
	dns_name_init(&prefix, NULL);

	/*
	 * Priority.
	 */
	dns_rdata_toregion(rdata, &region);
	num = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	snprintf(buf, sizeof buf, "%u", num);
	RETERR(isc_str_tobuffer(buf, target));
	RETERR(isc_str_tobuffer(" ", target));

	/*
	 * Target.
	 */
	dns_name_fromregion(&name, &region);
	isc_region_consume(&region, name_length(&name));
	sub = name_prefix(&name, tctx->origin, &prefix);
	RETERR(dns_name_totext(&prefix, sub, target));

	while (region.length > 0) {
		isc_region_t val_region;
		uint16_t svc_param_key, svc_param_value_len, man_key, port;

		RETERR(isc_str_tobuffer(" ", target));

		svc_param_key = uint16_fromregion(&region);
		isc_region_consume(&region, 2);

		svc_param_value_len = uint16_fromregion(&region);
		isc_region_consume(&region, 2);

		RETERR(isc_str_tobuffer(svc_param_key_to_text(svc_param_key),
		    target));

		val_region = region;
		val_region.length = svc_param_value_len;

		isc_region_consume(&region, svc_param_value_len);

		switch (svc_param_key) {
		case SVC_PARAM_MANDATORY:
			INSIST(val_region.length % 2 == 0);
			RETERR(isc_str_tobuffer("=", target));
			while (val_region.length > 0) {
				man_key = uint16_fromregion(&val_region);
				isc_region_consume(&val_region, 2);
				RETERR(isc_str_tobuffer(svc_param_key_to_text(
				    man_key), target));
				if (val_region.length != 0)
					RETERR(isc_str_tobuffer(",", target));
			}
			break;
		case SVC_PARAM_ALPN:
			RETERR(isc_str_tobuffer("=\"", target));
			while (val_region.length > 0) {
				txt_totext(&val_region, 0, target);
				if (val_region.length != 0)
					RETERR(isc_str_tobuffer(",", target));
			}
			RETERR(isc_str_tobuffer("\"", target));
			break;
		case SVC_PARAM_NO_DEF_ALPN:
			INSIST(val_region.length == 0);
			break;
		case SVC_PARAM_PORT:
			INSIST(val_region.length == 2);
			RETERR(isc_str_tobuffer("=", target));
			port = uint16_fromregion(&val_region);
			isc_region_consume(&val_region, 2);
			snprintf(buf, sizeof buf, "%u", port);
			RETERR(isc_str_tobuffer(buf, target));
			break;
		case SVC_PARAM_IPV4HINT:
			INSIST(val_region.length % 4 == 0);
			RETERR(isc_str_tobuffer("=", target));
			while (val_region.length > 0) {
				inet_ntop(AF_INET, val_region.base, buf,
				    sizeof buf);
				RETERR(isc_str_tobuffer(buf, target));
				isc_region_consume(&val_region, 4);
				if (val_region.length != 0)
					RETERR(isc_str_tobuffer(",", target));
			}
			break;
		case SVC_PARAM_ECH:
			RETERR(isc_str_tobuffer("=", target));
			RETERR(isc_base64_totext(&val_region, 0, "", target));
			break;
		case SVC_PARAM_IPV6HINT:
			INSIST(val_region.length % 16 == 0);
			RETERR(isc_str_tobuffer("=", target));
			while (val_region.length > 0) {
				inet_ntop(AF_INET6, val_region.base, buf,
				    sizeof buf);
				RETERR(isc_str_tobuffer(buf, target));
				isc_region_consume(&val_region, 16);
				if (val_region.length != 0)
					RETERR(isc_str_tobuffer(",", target));
			}
			break;
		case SVC_PARAM_DOHPATH:
			RETERR(isc_str_tobuffer("=", target));
			RETERR(multitxt_totext(&val_region, target));
			break;
		default:
			RETERR(isc_str_tobuffer("=", target));
			RETERR(multitxt_totext(&val_region, target));
			break;
		}
	}
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
totext_in_svcb(ARGS_TOTEXT) {
	REQUIRE(rdata->type == dns_rdatatype_svcb);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->length != 0);

	return (totext_in_svcb_https(rdata, tctx, target));
}

static inline isc_result_t
fromwire_in_svcb_https(ARGS_FROMWIRE) {
	dns_name_t name;
	isc_region_t sr;
	unsigned int svc_param_value_len;
	int alias_mode = 0;

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_NONE);

	dns_name_init(&name, NULL);

	/*
	 * SvcPriority.
	 */
	isc_buffer_activeregion(source, &sr);
	if (sr.length < 2)
		return (ISC_R_UNEXPECTEDEND);
	RETERR(isc_mem_tobuffer(target, sr.base, 2));
	alias_mode = uint16_fromregion(&sr) == 0;
	isc_buffer_forward(source, 2);

	/*
	 * TargetName.
	 */
	RETERR(dns_name_fromwire(&name, source, dctx, options, target));
	if (alias_mode) {
		/*
		 * In AliasMode, recipients MUST ignore any SvcParams that
		 * are present.
		 */
		return (ISC_R_SUCCESS);
	}

	isc_buffer_activeregion(source, &sr);
	while (sr.length > 0) {
		/*
		 * SvcParamKey.
		 */
		if (sr.length < 2)
			return (ISC_R_UNEXPECTEDEND);

		RETERR(isc_mem_tobuffer(target, sr.base, 2));
		isc_region_consume(&sr, 2);
		isc_buffer_forward(source, 2);

		/*
		 * SvcParamValue length.
		 */
		if (sr.length < 2)
			return (ISC_R_UNEXPECTEDEND);

		RETERR(isc_mem_tobuffer(target, sr.base, 2));
		svc_param_value_len = uint16_fromregion(&sr);
		isc_region_consume(&sr, 2);
		isc_buffer_forward(source, 2);

		if (sr.length < svc_param_value_len)
			return (ISC_R_UNEXPECTEDEND);

		RETERR(isc_mem_tobuffer(target, sr.base, svc_param_value_len));
		isc_region_consume(&sr, svc_param_value_len);
		isc_buffer_forward(source, svc_param_value_len);
	}

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_in_svcb(ARGS_FROMWIRE) {
	REQUIRE(type == dns_rdatatype_svcb);
	REQUIRE(rdclass == dns_rdataclass_in);
	return (fromwire_in_svcb_https(rdclass, type, source, dctx, options,
	    target));
}

static inline isc_result_t
towire_in_svcb_https(ARGS_TOWIRE) {
	dns_name_t name;
	dns_offsets_t offsets;
	isc_region_t sr;

	dns_compress_setmethods(cctx, DNS_COMPRESS_NONE);

	/*
	 * SvcPriority.
	 */
	dns_rdata_toregion(rdata, &sr);
	RETERR(isc_mem_tobuffer(target, sr.base, 2));
	isc_region_consume(&sr, 2);

	/*
	 * TargetName.
	 */
	dns_name_init(&name, offsets);
	dns_name_fromregion(&name, &sr);
	RETERR(dns_name_towire(&name, cctx, target));
	isc_region_consume(&sr, name_length(&name));

	/*
	 * SvcParams.
	 */
	return (isc_mem_tobuffer(target, sr.base, sr.length));
}

static inline isc_result_t
towire_in_svcb(ARGS_TOWIRE) {
	REQUIRE(rdata->type == dns_rdatatype_svcb);
	REQUIRE(rdata->length != 0);

	return (towire_in_svcb_https(rdata, cctx, target));
}
#endif	/* RDATA_IN_1_SVCB_64_C */

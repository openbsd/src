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

/* $Id: rcode.c,v 1.9 2020/02/26 18:47:58 florian Exp $ */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <isc/buffer.h>
#include <isc/region.h>
#include <isc/result.h>
#include <isc/types.h>
#include <isc/util.h>

#include <dns/cert.h>
#include <dns/keyvalues.h>
#include <dns/rcode.h>
#include <dns/rdataclass.h>
#include <dns/result.h>
#include <dns/secalg.h>

#define TOTEXTONLY 0x01

#define RCODENAMES \
	/* standard rcodes */ \
	{ dns_rcode_noerror, "NOERROR", 0}, \
	{ dns_rcode_formerr, "FORMERR", 0}, \
	{ dns_rcode_servfail, "SERVFAIL", 0}, \
	{ dns_rcode_nxdomain, "NXDOMAIN", 0}, \
	{ dns_rcode_notimp, "NOTIMP", 0}, \
	{ dns_rcode_refused, "REFUSED", 0}, \
	{ dns_rcode_yxdomain, "YXDOMAIN", 0}, \
	{ dns_rcode_yxrrset, "YXRRSET", 0}, \
	{ dns_rcode_nxrrset, "NXRRSET", 0}, \
	{ dns_rcode_notauth, "NOTAUTH", 0}, \
	{ dns_rcode_notzone, "NOTZONE", 0}, \
	{ 11, "RESERVED11", TOTEXTONLY}, \
	{ 12, "RESERVED12", TOTEXTONLY}, \
	{ 13, "RESERVED13", TOTEXTONLY}, \
	{ 14, "RESERVED14", TOTEXTONLY}, \
	{ 15, "RESERVED15", TOTEXTONLY},

#define TSIGRCODENAMES \
	/* extended rcodes */ \
	{ dns_tsigerror_badsig, "BADSIG", 0}, \
	{ dns_tsigerror_badkey, "BADKEY", 0}, \
	{ dns_tsigerror_badtime, "BADTIME", 0}, \
	{ dns_tsigerror_badmode, "BADMODE", 0}, \
	{ dns_tsigerror_badname, "BADNAME", 0}, \
	{ dns_tsigerror_badalg, "BADALG", 0}, \
	{ dns_tsigerror_badtrunc, "BADTRUNC", 0}, \
	{ 0, NULL, 0 }

/* RFC4398 section 2.1 */

#define CERTNAMES \
	{ 1, "PKIX", 0}, \
	{ 2, "SPKI", 0}, \
	{ 3, "PGP", 0}, \
	{ 4, "IPKIX", 0}, \
	{ 5, "ISPKI", 0}, \
	{ 6, "IPGP", 0}, \
	{ 7, "ACPKIX", 0}, \
	{ 8, "IACPKIX", 0}, \
	{ 253, "URI", 0}, \
	{ 254, "OID", 0}, \
	{ 0, NULL, 0}

/* RFC2535 section 7, RFC3110 */

#define MD5_SECALGNAMES
#define DH_SECALGNAMES
#define DSA_SECALGNAMES

#define SECALGNAMES \
	MD5_SECALGNAMES \
	DH_SECALGNAMES \
	DSA_SECALGNAMES \
	{ DNS_KEYALG_ECC, "ECC", 0 }, \
	{ DNS_KEYALG_RSASHA1, "RSASHA1", 0 }, \
	{ DNS_KEYALG_NSEC3RSASHA1, "NSEC3RSASHA1", 0 }, \
	{ DNS_KEYALG_RSASHA256, "RSASHA256", 0 }, \
	{ DNS_KEYALG_RSASHA512, "RSASHA512", 0 }, \
	{ DNS_KEYALG_ECCGOST, "ECCGOST", 0 }, \
	{ DNS_KEYALG_ECDSA256, "ECDSAP256SHA256", 0 }, \
	{ DNS_KEYALG_ECDSA384, "ECDSAP384SHA384", 0 }, \
	{ DNS_KEYALG_ED25519, "ED25519", 0 }, \
	{ DNS_KEYALG_ED448, "ED448", 0 }, \
	{ DNS_KEYALG_INDIRECT, "INDIRECT", 0 }, \
	{ DNS_KEYALG_PRIVATEDNS, "PRIVATEDNS", 0 }, \
	{ DNS_KEYALG_PRIVATEOID, "PRIVATEOID", 0 }, \
	{ 0, NULL, 0}

/* RFC2535 section 7.1 */

struct tbl {
	unsigned int    value;
	const char      *name;
	int             flags;
};

static struct tbl tsigrcodes[] = { RCODENAMES TSIGRCODENAMES };
static struct tbl certs[] = { CERTNAMES };
static struct tbl secalgs[] = { SECALGNAMES };

static isc_result_t
dns_mnemonic_totext(unsigned int value, isc_buffer_t *target,
		    struct tbl *table)
{
	int i = 0;
	char buf[sizeof("4294967296")];
	while (table[i].name != NULL) {
		if (table[i].value == value) {
			return (isc_str_tobuffer(table[i].name, target));
		}
		i++;
	}
	snprintf(buf, sizeof(buf), "%u", value);
	return (isc_str_tobuffer(buf, target));
}

isc_result_t
dns_tsigrcode_totext(dns_rcode_t rcode, isc_buffer_t *target) {
	return (dns_mnemonic_totext(rcode, target, tsigrcodes));
}

isc_result_t
dns_cert_totext(dns_cert_t cert, isc_buffer_t *target) {
	return (dns_mnemonic_totext(cert, target, certs));
}

isc_result_t
dns_secalg_totext(dns_secalg_t secalg, isc_buffer_t *target) {
	return (dns_mnemonic_totext(secalg, target, secalgs));
}

void
dns_secalg_format(dns_secalg_t alg, char *cp, unsigned int size) {
	isc_buffer_t b;
	isc_region_t r;
	isc_result_t result;

	REQUIRE(cp != NULL && size > 0);
	isc_buffer_init(&b, cp, size - 1);
	result = dns_secalg_totext(alg, &b);
	isc_buffer_usedregion(&b, &r);
	r.base[r.length] = 0;
	if (result != ISC_R_SUCCESS)
		r.base[0] = 0;
}

/*
 * This uses lots of hard coded values, but how often do we actually
 * add classes?
 */
isc_result_t
dns_rdataclass_fromtext(dns_rdataclass_t *classp, isc_textregion_t *source) {
#define COMPARE(string, rdclass) \
	if (((sizeof(string) - 1) == source->length) \
	    && (strncasecmp(source->base, string, source->length) == 0)) { \
		*classp = rdclass; \
		return (ISC_R_SUCCESS); \
	}

	switch (tolower((unsigned char)source->base[0])) {
	case 'a':
		COMPARE("any", dns_rdataclass_any);
		break;
	case 'c':
		/*
		 * RFC1035 says the mnemonic for the CHAOS class is CH,
		 * but historical BIND practice is to call it CHAOS.
		 * We will accept both forms, but only generate CH.
		 */
		COMPARE("ch", dns_rdataclass_chaos);
		COMPARE("chaos", dns_rdataclass_chaos);

		if (source->length > 5 &&
		    source->length < (5 + sizeof("65000")) &&
		    strncasecmp("class", source->base, 5) == 0) {
			char buf[sizeof("65000")];
			char *endp;
			unsigned int val;

			/*
			 * source->base is not required to be NUL terminated.
			 * Copy up to remaining bytes and NUL terminate.
			 */
			snprintf(buf, sizeof(buf), "%.*s",
				 (int)(source->length - 5), source->base + 5);
			val = strtoul(buf, &endp, 10);
			if (*endp == '\0' && val <= 0xffff) {
				*classp = (dns_rdataclass_t)val;
				return (ISC_R_SUCCESS);
			}
		}
		break;
	case 'h':
		COMPARE("hs", dns_rdataclass_hs);
		COMPARE("hesiod", dns_rdataclass_hs);
		break;
	case 'i':
		COMPARE("in", dns_rdataclass_in);
		break;
	case 'n':
		COMPARE("none", dns_rdataclass_none);
		break;
	case 'r':
		COMPARE("reserved0", dns_rdataclass_reserved0);
		break;
	}

#undef COMPARE

	return (DNS_R_UNKNOWN);
}

isc_result_t
dns_rdataclass_totext(dns_rdataclass_t rdclass, isc_buffer_t *target) {
	char buf[sizeof("CLASS65535")];

	switch (rdclass) {
	case dns_rdataclass_any:
		return (isc_str_tobuffer("ANY", target));
	case dns_rdataclass_chaos:
		return (isc_str_tobuffer("CH", target));
	case dns_rdataclass_hs:
		return (isc_str_tobuffer("HS", target));
	case dns_rdataclass_in:
		return (isc_str_tobuffer("IN", target));
	case dns_rdataclass_none:
		return (isc_str_tobuffer("NONE", target));
	case dns_rdataclass_reserved0:
		return (isc_str_tobuffer("RESERVED0", target));
	default:
		snprintf(buf, sizeof(buf), "CLASS%u", rdclass);
		return (isc_str_tobuffer(buf, target));
	}
}

void
dns_rdataclass_format(dns_rdataclass_t rdclass,
		      char *array, unsigned int size)
{
	isc_result_t result;
	isc_buffer_t buf;

	if (size == 0U)
		return;

	isc_buffer_init(&buf, array, size);
	result = dns_rdataclass_totext(rdclass, &buf);
	/*
	 * Null terminate.
	 */
	if (result == ISC_R_SUCCESS) {
		if (isc_buffer_availablelength(&buf) >= 1)
			isc_buffer_putuint8(&buf, 0);
		else
			result = ISC_R_NOSPACE;
	}
	if (result != ISC_R_SUCCESS)
		strlcpy(array, "<unknown>", size);
}

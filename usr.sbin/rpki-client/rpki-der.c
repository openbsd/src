/*	$OpenBSD: rpki-der.c,v 1.1 2026/11/03 14:33:14 tb Exp $ */

/*
 * Copyright (c) 2026 Theo Buehler <tb@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>

#include <openssl/asn1.h>
#include <openssl/objects.h>

#include "bytestring.h"
#include "extern.h"
#include "rpki-der.h"

int
cbs_oid_value_matches_obj(CBS *oid, const ASN1_OBJECT *obj)
{
	return CBS_mem_equal(oid, OBJ_get0_data(obj), OBJ_length(obj));
}

int
cbs_oid_value_matches_nid(CBS *oid, int nid)
{
	const ASN1_OBJECT *obj;

	if ((obj = OBJ_nid2obj(nid)) == NULL)
		errx(1, "OBJ_nid2obj failed for %s", nid2str(nid));

	return cbs_oid_value_matches_obj(oid, obj);
}

/*
 * RFC 5280, 4.1.1.2.
 *
 * AlgorithmIdentifier  ::=  SEQUENCE  {
 *      algorithm               OBJECT IDENTIFIER,
 *      parameters              ANY DEFINED BY algorithm OPTIONAL  }
 */

int
cbs_is_sha256_algorithmIdentifier_value(CBS *cbs)
{
	CBS alg, oid, param;
	int param_present;

	CBS_dup(cbs, &alg);

	if (!CBS_get_asn1(&alg, &oid, CBS_ASN1_OBJECT))
		return 0;
	/* Allow for faulty encoding coming from the last Millennium. */
	if (!CBS_get_optional_asn1(&alg, &param, &param_present, CBS_ASN1_NULL))
		return 0;

	if (CBS_len(&alg) != 0)
		return 0;

	if (!cbs_oid_value_matches_nid(&oid, NID_sha256))
		return 0;

	/* Long live Postel's Law - cf., e.g., RFC 5754, section 2. */
	return !param_present || CBS_len(&param) == 0;
}

int cbs_is_rfc7935_signatureAlgorithm_value(CBS *cbs)
{
	CBS alg, oid, param;
	int param_present;

	CBS_dup(cbs, &alg);

	if (!CBS_get_asn1(&alg, &oid, CBS_ASN1_OBJECT))
		return 0;
	if (!CBS_get_optional_asn1(&alg, &param, &param_present, CBS_ASN1_NULL))
		return 0;

	if (CBS_len(&alg) != 0)
		return 0;

	/* RFC 4055, 1.2: parameters field MUST contain NULL. */
	if (cbs_oid_value_matches_nid(&oid, NID_rsaEncryption))
		return param_present && CBS_len(&param) == 0;

	/*
	 * Need to accept sha256WithRSAEncryption for compat with RFC 6485.
	 * RFC 4055, 5: parameters MUST be NULL. Implementations MUST accept
	 * the parameters being absent as well as present.
	 */
	if (cbs_oid_value_matches_nid(&oid, NID_sha256WithRSAEncryption))
		return !param_present || CBS_len(&param) == 0;

	if (!experimental)
		return 0;

	/* RFC 5758, 3.2: encoding MUST omit parameters. */
	if (cbs_oid_value_matches_nid(&oid, NID_ecdsa_with_SHA256))
		return !param_present;

	return 0;
}

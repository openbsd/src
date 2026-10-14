/*	$OpenBSD: cms.c,v 1.72 2026/10/14 06:52:16 tb Exp $ */

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

#include <assert.h>
#include <err.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <openssl/asn1.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

#include "bytestring.h"
#include "extern.h"

/* RFC 5280, 4.1.2.5.1 and 4.1.2.5.2 as well as RFC 5652, 11.3. */
#define UTCTIME_LEN		15	/* 0x17 0x0d   YYMMDDHHMMSSZ */
#define GENERALIZEDTIME_LEN	17	/* 0x18 0x0f YYYYMMDDHHMMSSZ */

struct cms_signedData;

static int cms_get_encapContentInfo(const char *, struct cms_signedData *,
    CBS *, CBS *);

static int
cbs_oid_value_matches_obj(CBS *oid, const ASN1_OBJECT *obj)
{
	return CBS_mem_equal(oid, OBJ_get0_data(obj), OBJ_length(obj));
}

static int
cbs_oid_value_matches_nid(CBS *oid, int nid)
{
	const ASN1_OBJECT *obj;

	if ((obj = OBJ_nid2obj(nid)) == NULL)
		errx(1, "OBJ_nid2obj failed for %s", nid2str(nid));

	return cbs_oid_value_matches_obj(oid, obj);
}

/* Helper for syntax version of signedData and signerInfo. */
static int
cbs_integer_value_is_3(CBS *cbs)
{
	uint8_t u8;

	if (CBS_len(cbs) != 1)
		return 0;
	if (!CBS_peek_u8(cbs, &u8))
		return 0;

	return u8 == 3;
}

/*
 * RFC 5280, 4.1.1.2.
 *
 * AlgorithmIdentifier  ::=  SEQUENCE  {
 *      algorithm               OBJECT IDENTIFIER,
 *      parameters              ANY DEFINED BY algorithm OPTIONAL  }
 */

static int
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
	if (param_present)
		return CBS_len(&param) == 0;

	return 1;
}

static int
cbs_is_rfc7935_signatureAlgorithm_value(CBS *cbs)
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

/*
 * RFC 5652, section 3:
 *
 * ContentInfo ::= SEQUENCE {
 *   contentType ContentType,
 *   content [0] EXPLICIT ANY DEFINED BY contentType }
 *
 * ContentType ::= OBJECT IDENTIFIER
 */

static int
cms_get_contentInfo(CBS *cbs, CBS *out_contentType, CBS *out_content)
{
	CBS contentInfo;

	if (!CBS_get_asn1(cbs, &contentInfo, CBS_ASN1_SEQUENCE))
		return 0;

	if (!CBS_get_asn1(&contentInfo, out_contentType, CBS_ASN1_OBJECT))
		return 0;
	if (!CBS_get_asn1(&contentInfo, out_content,
	    CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0x00))
		return 0;

	if (CBS_len(&contentInfo) != 0)
		return 0;

	/* Don't check CBS_len(&cbs) == 0 to allow for concatenated objects. */

	return 1;
}

/*
 * RFC 5652, section 5.1:
 *
 * SignedData ::= SEQUENCE {
 *   version CMSVersion,
 *   digestAlgorithms DigestAlgorithmIdentifiers,
 *   encapContentInfo EncapsulatedContentInfo,
 *   certificates [0] IMPLICIT CertificateSet OPTIONAL,
 *   crls [1] IMPLICIT RevocationInfoChoices OPTIONAL,
 *   signerInfos SignerInfos }
 *
 * DigestAlgorithmIdentifiers ::= SET OF DigestAlgorithmIdentifier
 *
 * SignerInfos ::= SET OF SignerInfo
 */

struct cms_signedData {
	CBS version;
	CBS digestAlgorithms;
	CBS encapContentInfo;
	CBS certificates;
	CBS crls;
	CBS signerInfos;

	int certificates_present;
	int crls_present;
};

static int
cms_get_signedData(CBS *cbs, struct cms_signedData *out)
{
	CBS signedData;

	memset(out, 0, sizeof(*out));

	if (!CBS_get_asn1(cbs, &signedData, CBS_ASN1_SEQUENCE))
		return 0;

	if (!CBS_get_asn1(&signedData, &out->version, CBS_ASN1_INTEGER))
		return 0;
	if (!CBS_get_asn1(&signedData, &out->digestAlgorithms, CBS_ASN1_SET))
		return 0;
	if (!CBS_get_asn1(&signedData,
	    &out->encapContentInfo, CBS_ASN1_SEQUENCE))
		return 0;
	if (!CBS_get_optional_asn1(&signedData,
	    &out->certificates, &out->certificates_present,
	    CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0x00))
		return 0;
	if (!CBS_get_optional_asn1(&signedData,
	    &out->crls, &out->crls_present,
	    CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0x01))
		return 0;
	if (!CBS_get_asn1(&signedData, &out->signerInfos, CBS_ASN1_SET))
		return 0;

	if (CBS_len(&signedData) != 0)
		return 0;

	if (CBS_len(cbs) != 0)
		return 0;

	return 1;
}

/*
 * RFC 5652, 5.3
 *
 * SignerInfo ::= SEQUENCE {
 *   version CMSVersion,
 *   sid SignerIdentifier,
 *   digestAlgorithm DigestAlgorithmIdentifier,
 *   signedAttrs [0] IMPLICIT SignedAttributes OPTIONAL,
 *   signatureAlgorithm SignatureAlgorithmIdentifier,
 *   signature SignatureValue,
 *   unsignedAttrs [1] IMPLICIT UnsignedAttributes OPTIONAL }
 *
 * SignerIdentifier ::= CHOICE {
 *   issuerAndSerialNumber IssuerAndSerialNumber,
 *   subjectKeyIdentifier [0] SubjectKeyIdentifier }
 *
 * SignedAttributes ::= SET SIZE (1..MAX) OF Attribute
 *
 * UnsignedAttributes ::= SET SIZE (1..MAX) OF Attribute
 *
 * Attribute ::= SEQUENCE {
 *   attrType OBJECT IDENTIFIER,
 *   attrValues SET OF AttributeValue }
 *
 * AttributeValue ::= ANY
 *
 * SignatureValue ::= OCTET STRING
 */

struct cms_signerInfo {
	CBS version;
	CBS sid;
	CBS digestAlgorithm;
	CBS signedAttrs;
	CBS signatureAlgorithm;
	CBS signature;
	CBS unsignedAttrs;

	int sid_is_subjectKeyIdentifier;
	int signedAttrs_present;
	int unsignedAttrs_present;
};

static int
cms_check_rfc6488_signerInfo_version(CBS *cbs)
{
	return cbs_integer_value_is_3(cbs);
}

/*
 * Time ::= CHOICE {
 *   utcTime UTCTime,
 *   generalizedTime GeneralizedTime }
 */

static int
cbs_is_Time(CBS *cbs, uint8_t *out_tag)
{
	uint8_t tag;

	if (!CBS_peek_u8(cbs, &tag))
		return 0;

	*out_tag = tag;

	if (tag == CBS_ASN1_UTCTIME &&
	    CBS_len(cbs) == UTCTIME_LEN)
		return 1;

	if (tag == CBS_ASN1_GENERALIZEDTIME &&
	    CBS_len(cbs) == GENERALIZEDTIME_LEN)
		return 1;

	return 0;
}

/*
 * RFC 5652, 11.3 SigningTime ::= Time
 * Enforce the usual 1950-2049 rule for UTCTime vs GeneralizedTime.
 */
static int
cbs_get_signingtime(CBS *cbs, time_t *out_signingtime)
{
	CBS signingTime;
	ASN1_TIME *at = NULL;
	char *str = NULL;
	uint8_t tag;
	int ret = 0;

	if (!cbs_is_Time(cbs, &tag))
		goto out;
	if (!CBS_get_asn1(cbs, &signingTime, tag))
		goto out;
	if (CBS_len(cbs) != 0)
		goto out;

	if (!CBS_strdup(&signingTime, &str))
		goto out;

	if ((at = ASN1_TIME_new()) == NULL)
		goto out;
	if (!ASN1_TIME_set_string_X509(at, str))
		goto out;
	if (!x509_get_time(at, out_signingtime))
		goto out;

	ret = 1;

 out:
	ASN1_TIME_free(at);
	free(str);

	return ret;
}

/*
 * Helper for checking correct set-of encoding.
 *
 * X.690, 11.6:
 *
 * The encodings of the component values of a set-of value shall appear in
 * ascending order, the encodings being compared as octet strings with the
 * shorter components being padded at their trailing end with 0-octets.
 *
 * Note: The padding octets are for comparison purposes only and do not
 * appear in the encodings.
 */
static int
cbs_compare_octets(CBS *cbs1, CBS *cbs2, int *out_cmp)
{
	CBS tail;
	size_t len;
	int cmp, tail_cmp;

	if (CBS_len(cbs1) > CBS_len(cbs2)) {
		len = CBS_len(cbs2);

		CBS_dup(cbs1, &tail);
		tail_cmp = 1;
	} else {
		len = CBS_len(cbs1);

		CBS_dup(cbs2, &tail);
		tail_cmp = -1;
	}

	if ((cmp = memcmp(CBS_data(cbs1), CBS_data(cbs2), len)) != 0) {
		if (cmp > 0)
			*out_cmp = 1;
		else
			*out_cmp = -1;
		return 1;
	}

	if (!CBS_skip(&tail, len))
		return 0;

	while (CBS_len(&tail) > 0) {
		uint8_t u8;

		if (!CBS_get_u8(&tail, &u8))
			return 0;

		if (u8 > 0) {
			*out_cmp = tail_cmp;
			return 1;
		}
	}

	*out_cmp = 0;
	/* Multisets are valid. Trailing zeroes imply invalid tag or length. */
	return CBS_len(cbs1) == CBS_len(cbs2);
}

/*
 * RFC 6488, 2.1.6.4, as updated by RFC 9589:
 *
 * The signedAttrs element MUST be present and MUST include the
 * content-type, message-digest, and signing-time attributes
 * [RFC5652].  Other signed attributes MUST NOT be included.
 *
 * Check proper DER encoding of the signedAttrs set, verify that
 * each attribute value is a single element set, check that the
 * encapContentInfo's OID and message digest match the ones in
 * the signed attribute values. Parse and return the signingtime.
 */
static int
cms_check_rfc6488_signedAttrs(const char *fn, CBS *cbs,
    struct cms_signedData *sd, time_t *out_signingtime)
{
	CBS attrs;
	CBS curr, prev;
	CBS eContentType, eContentValue;
	int contentType_seen = 0, messageDigest_seen = 0, signingTime_seen = 0;
	int cmp;

	CBS_dup(cbs, &attrs);

	/*
	 * Enforce correct set-of encoding. While we could merge the two loops,
	 * it is way simpler to make two passes over the signedAttrs. We test
	 * for cmp == -1 rather than <= 0 because "[t]he signedAttrs element
	 * MUST include only a single instance of any particular attribute."
	 */
	if (!CBS_get_asn1_element(&attrs, &prev, CBS_ASN1_SEQUENCE))
		return 0;
	while (CBS_len(&attrs) > 0) {
		if (!CBS_get_asn1_element(&attrs, &curr, CBS_ASN1_SEQUENCE))
			return 0;
		if (!cbs_compare_octets(&prev, &curr, &cmp))
			return 0;
		if (cmp != -1)
			return 0;
		CBS_dup(&curr, &prev);
	}

	CBS_dup(cbs, &attrs);

	if (!cms_get_encapContentInfo(fn, sd, &eContentType, &eContentValue))
		return 0;

	/*
	 * RFC 6488, 2.1.6.4: signedAttrs
	 *
	 * SignedAttributes ::= SET SIZE (1..MAX) OF Attribute
	 *
	 * Attribute ::= SEQUENCE {
	 *   attrType OBJECT IDENTIFIER,
	 *   attrValues SET OF AttributeValue }
	 *
	 * AttributeValue ::= ANY
	 */
	while (CBS_len(&attrs) > 0) {
		CBS attr, oid, value;

		if (!CBS_get_asn1(&attrs, &attr, CBS_ASN1_SEQUENCE))
			return 0;

		if (!CBS_get_asn1(&attr, &oid, CBS_ASN1_OBJECT))
			return 0;
		if (!CBS_get_asn1(&attr, &value, CBS_ASN1_SET))
			return 0;

		if (CBS_len(&attr) != 0)
			return 0;

		if (cbs_oid_value_matches_obj(&oid, cnt_type_oid)) {
			/*
			 * 2.1.6.4.1: Content-Type Attribute
			 * attrValues [...] MUST match the eContentType
			 */
			CBS contentType;

			if (contentType_seen++ > 0)
				return 0;

			if (!CBS_get_asn1(&value, &contentType,
			    CBS_ASN1_OBJECT))
				return 0;

			if (!CBS_mem_equal(&contentType,
			    CBS_data(&eContentType), CBS_len(&eContentType)))
				return 0;
		} else if (cbs_oid_value_matches_obj(&oid, msg_dgst_oid)) {
			/*
			 * 2.1.6.4.2: Message-Digest Attribute
			 * contains the output of the digest algorithm applied
			 * to the content being signed
			 */
			CBS messageDigest;
			uint8_t digest[EVP_MAX_MD_SIZE];
			unsigned int len = sizeof(digest);

			if (messageDigest_seen++ > 0)
				return 0;

			if (!CBS_get_asn1(&value, &messageDigest,
			    CBS_ASN1_OCTETSTRING))
				return 0;

			if (!EVP_Digest(CBS_data(&eContentValue),
			    CBS_len(&eContentValue), digest, &len, EVP_sha256(),
			    NULL))
				return 0;

			if (!CBS_mem_equal(&messageDigest, digest, len))
				return 0;
		} else if (cbs_oid_value_matches_obj(&oid, sign_time_oid)) {
			/*
			 * 2.1.6.4.3: Signing-Time Attribute
			 * time [...] at which the digital signature was applied
			 */
			if (signingTime_seen++ > 0)
				return 0;

			if (!cbs_get_signingtime(&value, out_signingtime))
				return 0;
		} else {
			return 0;
		}

		/* attrValues MUST consist of only a single AttributeValue. */
		if (CBS_len(&value) != 0)
			return 0;
	}

	return messageDigest_seen && signingTime_seen && contentType_seen;
}

/*
 * RFC 6488, 2.1: the SignerInfos set MUST contain only a single SignerInfo
 * object. So we only parse one of these.
 */
static int
cms_get_rfc6488_signerInfo(const char *fn, struct cms_signedData *sd,
    struct cms_signerInfo *out, time_t *out_signingtime)
{
	CBS sis, cbs;

	memset(out, 0, sizeof(*out));

	CBS_dup(&sd->signerInfos, &sis);

	if (!CBS_get_asn1(&sis, &cbs, CBS_ASN1_SEQUENCE))
		return 0;

	if (!CBS_get_asn1(&cbs, &out->version, CBS_ASN1_INTEGER))
		return 0;

	/*
	 * RFC 6488, 2.1.6.1: version number MUST be 3. Ensure this right away
	 * because his simplifies parsing the sid next: per RFC 5652, 5.3 this
	 * implies that the CHOICE has the subjectKeyIdentifier variant, so we
	 * can ignore the issuerAndSerialNumber complication.
	 */
	if (!cms_check_rfc6488_signerInfo_version(&out->version))
		return 0;

	/*
	 * RFC 6488, 2.1.6.2: sid. cms_validate_rfc6488_object() checks
	 * that it matches the EE cert's SKI.
	 */
	if (!CBS_get_asn1(&cbs, &out->sid,
	    CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_PRIMITIVE | 0x00))
		return 0;
	out->sid_is_subjectKeyIdentifier = 1;

	/*
	 * RFC 6488, 2.1.6.3: digestAlgorithm.
	 */
	if (!CBS_get_asn1(&cbs, &out->digestAlgorithm, CBS_ASN1_SEQUENCE))
		return 0;
	/* RFC 7935, section 2: The hashing algorithm [...] is SHA-256. */
	if (!cbs_is_sha256_algorithmIdentifier_value(&out->digestAlgorithm))
		return 0;

	/*
	 * RFC 6488, 2.1.6.4: signedAttrs
	 */
	if (!CBS_get_optional_asn1(&cbs, &out->signedAttrs,
	    &out->signedAttrs_present,
	    CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0x00))
		return 0;
	if (!out->signedAttrs_present)
		return 0;
	if (!cms_check_rfc6488_signedAttrs(fn, &out->signedAttrs, sd,
	    out_signingtime))
		return 0;

	/*
	 * RFC 6488, 2.1.6.5: signatureAlgorithm.
	 */
	if (!CBS_get_asn1(&cbs, &out->signatureAlgorithm, CBS_ASN1_SEQUENCE))
		return 0;
	if (!cbs_is_rfc7935_signatureAlgorithm_value(&out->signatureAlgorithm))
		return 0;

	/*
	 * RFC 6488, 2.1.6.6: signature.
	 * Checked in cms_validate_rfc6488_object().
	 */
	if (!CBS_get_asn1(&cbs, &out->signature, CBS_ASN1_OCTETSTRING))
		return 0;

	/*
	 * RFC 6488, 2.1.6.7: unsignedAttrs MUST be omitted.
	 */
	if (!CBS_get_optional_asn1(&cbs, &out->unsignedAttrs,
	    &out->unsignedAttrs_present,
	    CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0x01))
		return 0;
	if (out->unsignedAttrs_present)
		return 0;

	/* No trailing garbage in signerInfo. */
	if (CBS_len(&cbs) != 0)
		return 0;

	/* Only support a single signerInfo. */
	if (CBS_len(&sis) != 0)
		return 0;

	return 1;
}

/*
 * RFC 6488, 2.1.1
 * Check that syntax version number is 3.
 */
static int
cms_check_rfc6488_signedData_version(const char *fn, struct cms_signedData *sd)
{
	return cbs_integer_value_is_3(&sd->version);
}

/*
 * RFC 6488, 2.1.2
 * Exactly one AlgorithmIdentfier, must be SHA-256 per RFC 7935, section 2.
 */
static int
cms_check_rfc6488_digestAlgorithms(const char *fn, struct cms_signedData *sd)
{
	CBS digestAlgorithms, alg;

	CBS_dup(&sd->digestAlgorithms, &digestAlgorithms);

	if (!CBS_get_asn1(&digestAlgorithms, &alg, CBS_ASN1_SEQUENCE))
		return 0;
	if (!cbs_is_sha256_algorithmIdentifier_value(&alg))
		return 0;

	/* Only one algorithm is allowed. */
	if (CBS_len(&digestAlgorithms) != 0)
		return 0;

	return 1;
}

/*
 * RFC 5652, section 5.2:
 *
 * EncapsulatedContentInfo ::= SEQUENCE {
 *   eContentType ContentType,
 *   eContent [0] EXPLICIT OCTET STRING OPTIONAL }
 *
 * ContentType ::= OBJECT IDENTIFIER
 *
 * Detached signatures are not supported, so this requires eContent to be
 * present. For the callers' convenience, which never need tag and length,
 * out_eContentValue contains the eContent's value octets.
 */
static int
cms_get_encapContentInfo(const char *fn, struct cms_signedData *sd,
    CBS *out_eContentType, CBS *out_eContentValue)
{
	CBS cbs, eContent;
	int eContent_present;

	CBS_dup(&sd->encapContentInfo, &cbs);

	if (!CBS_get_asn1(&cbs, out_eContentType, CBS_ASN1_OBJECT))
		return 0;
	if (!CBS_get_optional_asn1(&cbs, &eContent, &eContent_present,
	    CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0x00))
		return 0;
	/* No support for detached signatures for now. */
	if (!eContent_present)
		return 0;

	/*
	 * Strip tag and length octets off the eContent OCTET STRING because
	 * they are never used explicitly. Avoids repeated dances.
	 */
	if (!CBS_get_asn1(&eContent, out_eContentValue, CBS_ASN1_OCTETSTRING))
		return 0;
	if (CBS_len(&eContent) != 0)
		return 0;

	if (CBS_len(&cbs) != 0)
		return 0;

	return 1;
}

static int
cms_get_rfc6488_ee_cert(const char *fn, struct cms_signedData *sd, int talid,
    struct cert **out_cert)
{
	struct cert *cert;

	/*
	 * RFC 6488, 2.1.4: The certificates field MUST be included, and MUST
	 * contain exactly one certificate [...].
	 * The last point is ensured by the trailing garbage check of
	 * cert_deserialize_and_parse().
	 */
	if (!sd->certificates_present)
		return 0;

	if ((cert = cert_parse_ee_cert(fn, talid, CBS_data(&sd->certificates),
	    CBS_len(&sd->certificates))) == NULL)
		return 0;

	*out_cert = cert;

	return 1;
}

static int
cms_check_rfc6488_crls(const char *fn, struct cms_signedData *sd)
{
	/* RFC 6488, 2.1.5: the crls field MUST be omitted. */
	return !sd->crls_present;
}

static const struct signed_obj *
cms_object_from_rtype(const char *fn, enum rtype rtype)
{
	switch (rtype) {
	case RTYPE_ASPA:
		return aspa_obj();
	case RTYPE_MFT:
		return mft_obj();
	case RTYPE_ROA:
		return roa_obj();
	case RTYPE_RSC:
		return rsc_obj();
	case RTYPE_SPL:
		return spl_obj();
	case RTYPE_TAK:
		return tak_obj();
	default:
		errx(1, "%s: unsupported signed object", fn);
	}
}

static void *
cms_validate_rfc6488_object(struct cert **out_cert, const char *fn,
    enum rtype rtype, int talid, size_t len, struct cms_signedData *sd)
{
	void *obj = NULL, *ret_obj = NULL;
	const struct signed_obj *sobj;
	CBB cbb, child;
	uint8_t *sig = NULL;
	size_t sig_len = 0;
	CBS eContentType, eContentValue;
	struct cert *cert = NULL;
	uint8_t ski[SHA_DIGEST_LENGTH];
	struct cms_signerInfo signerInfo;
	EVP_MD_CTX *md_ctx = NULL;
	EVP_PKEY *pkey;
	time_t signingtime = 0;

	assert(*out_cert == NULL);

	memset(&cbb, 0, sizeof(cbb));

	sobj = cms_object_from_rtype(fn, rtype);

	if (!cms_check_rfc6488_signedData_version(fn, sd))
		goto out;
	if (!cms_check_rfc6488_digestAlgorithms(fn, sd))
		goto out;
	if (!cms_get_encapContentInfo(fn, sd, &eContentType, &eContentValue))
		goto out;
	/* XXX - should be a _check_ function and directly validate cert. */
	if (!cms_get_rfc6488_ee_cert(fn, sd, talid, &cert))
		goto out;
	if (!cms_check_rfc6488_crls(fn, sd))
		goto out;
	if (!cms_get_rfc6488_signerInfo(fn, sd, &signerInfo, &signingtime))
		goto out;

	if (signingtime > cert->notafter)
		warnx("%s: CMS signing-time after X.509 notAfter", fn);

	/*
	 * The eContentType must match the OID of the signed object we expect.
	 */
	if (!cbs_oid_value_matches_obj(&eContentType, sobj->oid()))
		goto out;

	/*
	 * RFC 6488, section 2.1.6.2: For RPKI signed objects the sid MUST be
	 * the SubjectKeyIdentifier that appears in the EE certificate [...]
	 *
	 * This is a bit ugly but seems simplest: cert_ski() fully validates
	 * the SKI against the SPKI and stores its hex encoding in cert->ski.
	 */
	if (hex_decode(cert->ski, ski, sizeof(ski)) == -1)
		goto out;
	if (!CBS_mem_equal(&signerInfo.sid, ski, sizeof(ski)))
		goto out;

	/*
	 * RFC 5652, section 5.4: A separate encoding of the signedAttrs
	 * field is performed for message digest calculation. [...] an
	 * EXPLICIT SET OF tag is used.
	 */
	if (!CBB_init(&cbb, 0))
		goto out;
	if (!CBB_add_asn1(&cbb, &child, CBS_ASN1_SET))
		goto out;
	if (!CBB_add_bytes(&child, CBS_data(&signerInfo.signedAttrs),
	    CBS_len(&signerInfo.signedAttrs)))
		goto out;
	if (!CBB_finish(&cbb, &sig, &sig_len))
		goto out;

	/*
	 * RFC 5652, section 5.6: Signature Verification Process
	 *
	 * Verify sig with the EE cert's pubkey using digestAlgorithm and
	 * signature in signerInfo. This covers section 5.6 because we matched
	 * the eContentType and the digested eContentValue in the signedAttrs
	 * as part of cms_check_rfc6488_signedAttrs().
	 *
	 * Since RFC 7935 only allows SHA-256, the additional requirements
	 * for algorithm protection in RFC 8933, section 3, are covered.
	 *
	 * XXX - The recommended use of the RFC 6211 CMSAlgorithmProtection
	 * attribute (or its draft update) in RFC 8933, section 4, conflicts
	 * with the requirements on signedAttrs of RFC 6488 (and RFC 9589).
	 * This is arguably also covered by RFC 7935, but this may be worth
	 * a reconsideration when more algorithms will be added to the RPKI
	 * CMS profile.
	 */
	if ((md_ctx = EVP_MD_CTX_new()) == NULL)
		goto out;
	if ((pkey = X509_get0_pubkey(cert->x509)) == NULL)
		goto out;
	if (EVP_DigestVerifyInit(md_ctx, NULL, EVP_sha256(), NULL, pkey) <= 0)
		goto out;
	if (EVP_DigestVerify(md_ctx, CBS_data(&signerInfo.signature),
	    CBS_len(&signerInfo.signature), sig, sig_len) <= 0)
		goto out;

	/*
	 * Finally parse and validate the eContent.
	 */
	obj = sobj->new(len, signingtime);
	if (!sobj->cert_info(fn, obj, cert))
		goto out;
	if (!sobj->parse_econtent(fn, obj,
	    CBS_data(&eContentValue), CBS_len(&eContentValue)))
		goto out;
	if (!sobj->validate(fn, obj, cert))
		goto out;

	*out_cert = cert;
	cert = NULL;

	ret_obj = obj;
	obj = NULL;

 out:
	CBB_cleanup(&cbb);
	sobj->free(obj);
	cert_free(cert);
	free(sig);
	EVP_MD_CTX_free(md_ctx);

	return ret_obj;
}

static void *
cms_parse_validate(struct cert **out_cert, const char *fn, enum rtype rtype,
    int talid, const unsigned char *der, size_t len)
{
	CBS cbs, contentType, content;
	struct cms_signedData signedData;
	uint8_t *buf = NULL;
	size_t buf_len;
	void *ret = NULL;

	/* just fail for empty buffers, the warning was printed elsewhere */
	if (der == NULL)
		goto out;

	CBS_init(&cbs, der, len);

	/*
	 * ARIN special. In filemode re-encode the "der" buffer into definite
	 * length encoding. The DER parser can then deserialize the signed
	 * object. This miraculously works because of the way CMS signatures
	 * are computed: signedAttrs MUST be DER encoded (per RFC 5652, 5.3)
	 * and the value octets of the eContent do not use the invalid encoding
	 * (but its length octets do!). The signature and the message-digest
	 * attribute therefore check out despite this modification.
	 */
	if (filemode) {
		if (!CBS_asn1_indefinite_to_definite(&cbs, &buf, &buf_len))
			goto out;
		if (buf != NULL) {
			warnx("%s: RFC 6488: indefinite length encoding "
			    "disallowed in DER", fn);
			CBS_init(&cbs, buf, buf_len);
		}
	}

	if (!cms_get_contentInfo(&cbs, &contentType, &content)) {
		warnx("%s: RFC 6488: failed to parse outermost DER", fn);
		goto out;
	}
	if (CBS_len(&cbs) != 0) {
		warnx("%s: RFC 6488: trailing garbage", fn);
		goto out;
	}

	/* We only support RFC 6488, which yields a few simplifications. */
	if (!cbs_oid_value_matches_nid(&contentType, NID_pkcs7_signed)) {
		warnx("%s: RFC 6488, 3.1.a: CMS content not signedData", fn);
		goto out;
	}
	if (!cms_get_signedData(&content, &signedData)) {
		warnx("%s: RFC 6488, failed to parse signedData DER", fn);
		goto out;
	}

	/*
	 * In filemode we pass the original len, not buf_len. This is only
	 * used for mft->mftsize, for which we want the actual file size.
	 */
	ret = cms_validate_rfc6488_object(out_cert, fn, rtype, talid, len,
	    &signedData);

 out:
	free(buf);

	return ret;
}

void *
signed_object_parse(struct cert **out_cert, const char *fn, enum rtype rtype,
    int talid, const unsigned char *der, size_t len)
{
	return cms_parse_validate(out_cert, fn, rtype, talid, der, len);
}

/*
 * filemode CMS rtype prober. If this is the DER of a CMS object, try
 * to determine its rtype from the contentType or the eContentType.
 * Returns rtype if heuristics succeed and RTYPE_INVALID on error.
 */
enum rtype
cms_rtype_from_der(const char *fn, const unsigned char *der, size_t len)
{
	CBS cbs, contentType, content, eContentType;
	struct cms_signedData signedData;
	uint8_t *buf = NULL;
	size_t buf_len;
	enum rtype rtype = RTYPE_INVALID;

	CBS_init(&cbs, der, len);

	/* ARIN special. See cms_parse_validate(). */
	if (!CBS_asn1_indefinite_to_definite(&cbs, &buf, &buf_len))
		goto out;
	if (buf != NULL)
		CBS_init(&cbs, buf, buf_len);

	if (!cms_get_contentInfo(&cbs, &contentType, &content))
		goto out;

	if (cbs_oid_value_matches_obj(&contentType, ccr_oid)) {
		rtype = RTYPE_CCR;
		goto out;
	}

	if (!cbs_oid_value_matches_nid(&contentType, NID_pkcs7_signed))
		goto out;
	if (!cms_get_signedData(&content, &signedData))
		goto out;
	if (!cms_get_encapContentInfo(fn, &signedData, &eContentType,
	    NULL))
		goto out;

	if (cbs_oid_value_matches_obj(&eContentType, aspa_oid))
		rtype = RTYPE_ASPA;
	else if (cbs_oid_value_matches_obj(&eContentType, mft_oid))
		rtype = RTYPE_MFT;
	else if (cbs_oid_value_matches_obj(&eContentType, roa_oid))
		rtype = RTYPE_ROA;
	else if (cbs_oid_value_matches_obj(&eContentType, rsc_oid))
		rtype = RTYPE_RSC;
	else if (cbs_oid_value_matches_obj(&eContentType, spl_oid))
		rtype = RTYPE_SPL;
	else if (cbs_oid_value_matches_obj(&eContentType, tak_oid))
		rtype = RTYPE_TAK;

 out:
	free(buf);

	return rtype;
}

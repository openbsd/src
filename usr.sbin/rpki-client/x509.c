/*	$OpenBSD: x509.c,v 1.100 2024/07/08 16:11:47 tb Exp $ */
/*
 * Copyright (c) 2022 Theo Buehler <tb@openbsd.org>
 * Copyright (c) 2021 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/evp.h>
#include <openssl/x509v3.h>

#include "extern.h"

ASN1_OBJECT	*certpol_oid;	/* id-cp-ipAddr-asNumber cert policy */
ASN1_OBJECT	*carepo_oid;	/* 1.3.6.1.5.5.7.48.5 (caRepository) */
ASN1_OBJECT	*manifest_oid;	/* 1.3.6.1.5.5.7.48.10 (rpkiManifest) */
ASN1_OBJECT	*signedobj_oid;	/* 1.3.6.1.5.5.7.48.11 (signedObject) */
ASN1_OBJECT	*notify_oid;	/* 1.3.6.1.5.5.7.48.13 (rpkiNotify) */
ASN1_OBJECT	*roa_oid;	/* id-ct-routeOriginAuthz CMS content type */
ASN1_OBJECT	*mft_oid;	/* id-ct-rpkiManifest CMS content type */
ASN1_OBJECT	*gbr_oid;	/* id-ct-rpkiGhostbusters CMS content type */
ASN1_OBJECT	*bgpsec_oid;	/* id-kp-bgpsec-router Key Purpose */
ASN1_OBJECT	*cnt_type_oid;	/* pkcs-9 id-contentType */
ASN1_OBJECT	*msg_dgst_oid;	/* pkcs-9 id-messageDigest */
ASN1_OBJECT	*sign_time_oid;	/* pkcs-9 id-signingTime */
ASN1_OBJECT	*rsc_oid;	/* id-ct-signedChecklist */
ASN1_OBJECT	*aspa_oid;	/* id-ct-ASPA */
ASN1_OBJECT	*tak_oid;	/* id-ct-SignedTAL */
ASN1_OBJECT	*geofeed_oid;	/* id-ct-geofeedCSVwithCRLF */
ASN1_OBJECT	*spl_oid;	/* id-ct-signedPrefixList */

static const struct {
	const char	 *oid;
	ASN1_OBJECT	**ptr;
} oid_table[] = {
	{
		.oid = "1.3.6.1.5.5.7.14.2",
		.ptr = &certpol_oid,
	},
	{
		.oid = "1.3.6.1.5.5.7.48.5",
		.ptr = &carepo_oid,
	},
	{
		.oid = "1.3.6.1.5.5.7.48.10",
		.ptr = &manifest_oid,
	},
	{
		.oid = "1.3.6.1.5.5.7.48.11",
		.ptr = &signedobj_oid,
	},
	{
		.oid = "1.3.6.1.5.5.7.48.13",
		.ptr = &notify_oid,
	},
	{
		.oid = "1.2.840.113549.1.9.16.1.24",
		.ptr = &roa_oid,
	},
	{
		.oid = "1.2.840.113549.1.9.16.1.26",
		.ptr = &mft_oid,
	},
	{
		.oid = "1.2.840.113549.1.9.16.1.35",
		.ptr = &gbr_oid,
	},
	{
		.oid = "1.3.6.1.5.5.7.3.30",
		.ptr = &bgpsec_oid,
	},
	{
		.oid = "1.2.840.113549.1.9.3",
		.ptr = &cnt_type_oid,
	},
	{
		.oid = "1.2.840.113549.1.9.4",
		.ptr = &msg_dgst_oid,
	},
	{
		.oid = "1.2.840.113549.1.9.5",
		.ptr = &sign_time_oid,
	},
	{
		.oid = "1.2.840.113549.1.9.16.1.47",
		.ptr = &geofeed_oid,
	},
	{
		.oid = "1.2.840.113549.1.9.16.1.48",
		.ptr = &rsc_oid,
	},
	{
		.oid = "1.2.840.113549.1.9.16.1.49",
		.ptr = &aspa_oid,
	},
	{
		.oid = "1.2.840.113549.1.9.16.1.50",
		.ptr = &tak_oid,
	},
	{
		.oid = "1.2.840.113549.1.9.16.1.51",
		.ptr = &spl_oid,
	},
};

void
x509_init_oid(void)
{
	size_t	i;

	for (i = 0; i < sizeof(oid_table) / sizeof(oid_table[0]); i++) {
		*oid_table[i].ptr = OBJ_txt2obj(oid_table[i].oid, 1);
		if (*oid_table[i].ptr == NULL)
			errx(1, "OBJ_txt2obj for %s failed", oid_table[i].oid);
	}
}

/*
 * A number of critical OpenSSL API functions can't properly indicate failure
 * and are unreliable if the extensions aren't already cached. An old trick is
 * to cache the extensions using an error-checked call to X509_check_purpose()
 * with a purpose of -1. This way functions such as X509_check_ca(), X509_cmp(),
 * X509_get_key_usage(), X509_get_extended_key_usage() won't lie.
 *
 * Should be called right after deserialization and is essentially free to call
 * multiple times.
 */
int
x509_cache_extensions(X509 *x509, const char *fn)
{
	if (X509_check_purpose(x509, -1, 0) <= 0) {
		warnx("%s: could not cache X509v3 extensions", fn);
		return 0;
	}
	return 1;
}

/*
 * Parse X509v3 authority key identifier (AKI), RFC 6487 sec. 4.8.3.
 * Returns the AKI or NULL if it could not be parsed.
 * The AKI is formatted as a hex string.
 */
int
x509_get_aki(X509 *x, const char *fn, char **aki)
{
	const unsigned char	*d;
	AUTHORITY_KEYID		*akid;
	ASN1_OCTET_STRING	*os;
	int			 dsz, crit, rc = 0;

	*aki = NULL;
	akid = X509_get_ext_d2i(x, NID_authority_key_identifier, &crit, NULL);
	if (akid == NULL) {
		if (crit != -1) {
			warnx("%s: RFC 6487 section 4.8.3: error parsing AKI",
			    fn);
			return 0;
		}
		return 1;
	}
	if (crit != 0) {
		warnx("%s: RFC 6487 section 4.8.3: "
		    "AKI: extension not non-critical", fn);
		goto out;
	}
	if (akid->issuer != NULL || akid->serial != NULL) {
		warnx("%s: RFC 6487 section 4.8.3: AKI: "
		    "authorityCertIssuer or authorityCertSerialNumber present",
		    fn);
		goto out;
	}

	os = akid->keyid;
	if (os == NULL) {
		warnx("%s: RFC 6487 section 4.8.3: AKI: "
		    "Key Identifier missing", fn);
		goto out;
	}

	d = os->data;
	dsz = os->length;

	if (dsz != SHA_DIGEST_LENGTH) {
		warnx("%s: RFC 6487 section 4.8.2: AKI: "
		    "want %d bytes SHA1 hash, have %d bytes",
		    fn, SHA_DIGEST_LENGTH, dsz);
		goto out;
	}

	*aki = hex_encode(d, dsz);
	rc = 1;
out:
	AUTHORITY_KEYID_free(akid);
	return rc;
}

/*
 * Validate the X509v3 subject key identifier (SKI), RFC 6487 section 4.8.2:
 * "The SKI is a SHA-1 hash of the value of the DER-encoded ASN.1 BIT STRING of
 * the Subject Public Key, as described in Section 4.2.1.2 of RFC 5280."
 * Returns the SKI formatted as hex string, or NULL if it couldn't be parsed.
 */
int
x509_get_ski(X509 *x, const char *fn, char **ski)
{
	ASN1_OCTET_STRING	*os;
	unsigned char		 md[EVP_MAX_MD_SIZE];
	unsigned int		 md_len = EVP_MAX_MD_SIZE;
	int			 crit, rc = 0;

	*ski = NULL;
	os = X509_get_ext_d2i(x, NID_subject_key_identifier, &crit, NULL);
	if (os == NULL) {
		if (crit != -1) {
			warnx("%s: RFC 6487 section 4.8.2: error parsing SKI",
			    fn);
			return 0;
		}
		return 1;
	}
	if (crit != 0) {
		warnx("%s: RFC 6487 section 4.8.2: "
		    "SKI: extension not non-critical", fn);
		goto out;
	}

	if (!X509_pubkey_digest(x, EVP_sha1(), md, &md_len)) {
		warnx("%s: X509_pubkey_digest", fn);
		goto out;
	}

	if (os->length < 0 || md_len != (size_t)os->length) {
		warnx("%s: RFC 6487 section 4.8.2: SKI: "
		    "want %u bytes SHA1 hash, have %d bytes",
		    fn, md_len, os->length);
		goto out;
	}

	if (memcmp(os->data, md, md_len) != 0) {
		warnx("%s: SKI does not match SHA1 hash of SPK", fn);
		goto out;
	}

	*ski = hex_encode(md, md_len);
	rc = 1;
 out:
	ASN1_OCTET_STRING_free(os);
	return rc;
}

/*
 * Check the cert's purpose: the cA bit in basic constraints distinguishes
 * between TA/CA and EE/BGPsec router and the key usage bits must match.
 * TAs are self-signed, CAs not self-issued, EEs have no extended key usage,
 * BGPsec router have id-kp-bgpsec-router OID.
 */
enum cert_purpose
x509_get_purpose(X509 *x, const char *fn)
{
	BASIC_CONSTRAINTS		*bc = NULL;
	EXTENDED_KEY_USAGE		*eku = NULL;
	const X509_EXTENSION		*ku;
	int				 crit, ext_flags, i, is_ca, ku_idx;
	enum cert_purpose		 purpose = CERT_PURPOSE_INVALID;

	if (!x509_cache_extensions(x, fn))
		goto out;

	ext_flags = X509_get_extension_flags(x);

	/* Key usage must be present and critical. KU bits are checked below. */
	if ((ku_idx = X509_get_ext_by_NID(x, NID_key_usage, -1)) < 0) {
		warnx("%s: RFC 6487, section 4.8.4: missing KeyUsage", fn);
		goto out;
	}
	if ((ku = X509_get_ext(x, ku_idx)) == NULL) {
		warnx("%s: RFC 6487, section 4.8.4: missing KeyUsage", fn);
		goto out;
	}
	if (!X509_EXTENSION_get_critical(ku)) {
		warnx("%s: RFC 6487, section 4.8.4: KeyUsage not critical", fn);
		goto out;
	}

	/* This weird API can return 0, 1, 2, 4, 5 but can't error... */
	if ((is_ca = X509_check_ca(x)) > 1) {
		if (is_ca == 4)
			warnx("%s: RFC 6487: sections 4.8.1 and 4.8.4: "
			    "no basic constraints, but keyCertSign set", fn);
		else
			warnx("%s: unexpected legacy certificate", fn);
		goto out;
	}

	if (is_ca) {
		bc = X509_get_ext_d2i(x, NID_basic_constraints, &crit, NULL);
		if (bc == NULL) {
			if (crit != -1)
				warnx("%s: RFC 6487 section 4.8.1: "
				    "error parsing basic constraints", fn);
			else
				warnx("%s: RFC 6487 section 4.8.1: "
				    "missing basic constraints", fn);
			goto out;
		}
		if (crit != 1) {
			warnx("%s: RFC 6487 section 4.8.1: Basic Constraints "
			    "must be marked critical", fn);
			goto out;
		}
		if (bc->pathlen != NULL) {
			warnx("%s: RFC 6487 section 4.8.1: Path Length "
			    "Constraint must be absent", fn);
			goto out;
		}

		if (X509_get_key_usage(x) != (KU_KEY_CERT_SIGN | KU_CRL_SIGN)) {
			warnx("%s: RFC 6487 section 4.8.4: key usage violation",
			    fn);
			goto out;
		}

		if (X509_get_extended_key_usage(x) != UINT32_MAX) {
			warnx("%s: RFC 6487 section 4.8.5: EKU not allowed",
			    fn);
			goto out;
		}

		/*
		 * EXFLAG_SI means that issuer and subject are identical.
		 * EXFLAG_SS is SI plus the AKI is absent or matches the SKI.
		 * Thus, exactly the trust anchors should have EXFLAG_SS set
		 * and we should never see EXFLAG_SI without EXFLAG_SS.
		 */
		if ((ext_flags & EXFLAG_SS) != 0)
			purpose = CERT_PURPOSE_TA;
		else if ((ext_flags & EXFLAG_SI) == 0)
			purpose = CERT_PURPOSE_CA;
		else
			warnx("%s: RFC 6487, section 4.8.3: "
			    "self-issued cert with AKI-SKI mismatch", fn);
		goto out;
	}

	if ((ext_flags & EXFLAG_BCONS) != 0) {
		warnx("%s: Basic Constraints ext in non-CA cert", fn);
		goto out;
	}

	if (X509_get_key_usage(x) != KU_DIGITAL_SIGNATURE) {
		warnx("%s: RFC 6487 section 4.8.4: KU must be digitalSignature",
		    fn);
		goto out;
	}

	/*
	 * EKU is only defined for BGPsec Router certs and must be absent from
	 * EE certs.
	 */
	eku = X509_get_ext_d2i(x, NID_ext_key_usage, &crit, NULL);
	if (eku == NULL) {
		if (crit != -1)
			warnx("%s: error parsing EKU", fn);
		else
			purpose = CERT_PURPOSE_EE; /* EKU absent */
		goto out;
	}
	if (crit != 0) {
		warnx("%s: EKU: extension must not be marked critical", fn);
		goto out;
	}

	/*
	 * Per RFC 8209, section 3.1.3.2 the id-kp-bgpsec-router OID must be
	 * present and others are allowed, which we don't need to recognize.
	 * This matches RFC 5280, section 4.2.1.12.
	 */
	for (i = 0; i < sk_ASN1_OBJECT_num(eku); i++) {
		if (OBJ_cmp(bgpsec_oid, sk_ASN1_OBJECT_value(eku, i)) == 0) {
			purpose = CERT_PURPOSE_BGPSEC_ROUTER;
			break;
		}
	}

 out:
	BASIC_CONSTRAINTS_free(bc);
	EXTENDED_KEY_USAGE_free(eku);
	return purpose;
}

/*
 * Extract Subject Public Key Info (SPKI) from BGPsec X.509 Certificate.
 * Returns NULL on failure, on success return the SPKI as base64 encoded pubkey
 */
char *
x509_get_pubkey(X509 *x, const char *fn)
{
	EVP_PKEY	*pkey;
	EC_KEY		*eckey;
	int		 nid;
	const char	*cname;
	uint8_t		*pubkey = NULL;
	char		*res = NULL;
	int		 len;

	pkey = X509_get0_pubkey(x);
	if (pkey == NULL) {
		warnx("%s: X509_get0_pubkey failed in %s", fn, __func__);
		goto out;
	}
	if (EVP_PKEY_base_id(pkey) != EVP_PKEY_EC) {
		warnx("%s: Expected EVP_PKEY_EC, got %d", fn,
		    EVP_PKEY_base_id(pkey));
		goto out;
	}

	eckey = EVP_PKEY_get0_EC_KEY(pkey);
	if (eckey == NULL) {
		warnx("%s: Incorrect key type", fn);
		goto out;
	}

	nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(eckey));
	if (nid != NID_X9_62_prime256v1) {
		if ((cname = EC_curve_nid2nist(nid)) == NULL)
			cname = nid2str(nid);
		warnx("%s: Expected P-256, got %s", fn, cname);
		goto out;
	}

	if (!EC_KEY_check_key(eckey)) {
		warnx("%s: EC_KEY_check_key failed in %s", fn, __func__);
		goto out;
	}

	len = i2d_PUBKEY(pkey, &pubkey);
	if (len <= 0) {
		warnx("%s: i2d_PUBKEY failed in %s", fn, __func__);
		goto out;
	}

	if (base64_encode(pubkey, len, &res) == -1)
		errx(1, "base64_encode failed in %s", __func__);

 out:
	free(pubkey);
	return res;
}

/*
 * Compute the SKI of an RSA public key in an X509_PUBKEY using SHA-1.
 * Returns allocated hex-encoded SKI on success, NULL on failure.
 */
char *
x509_pubkey_get_ski(X509_PUBKEY *pubkey, const char *fn)
{
	ASN1_OBJECT		*obj;
	const unsigned char	*der;
	int			 der_len, nid;
	unsigned char		 md[EVP_MAX_MD_SIZE];
	unsigned int		 md_len = EVP_MAX_MD_SIZE;

	if (!X509_PUBKEY_get0_param(&obj, &der, &der_len, NULL, pubkey)) {
		warnx("%s: X509_PUBKEY_get0_param failed", fn);
		return NULL;
	}

	/* XXX - should allow other keys as well. */
	if ((nid = OBJ_obj2nid(obj)) != NID_rsaEncryption) {
		warnx("%s: RFC 7935: wrong signature algorithm %s, want %s",
		    fn, nid2str(nid), LN_rsaEncryption);
		return NULL;
	}

	if (!EVP_Digest(der, der_len, md, &md_len, EVP_sha1(), NULL)) {
		warnx("%s: EVP_Digest failed", fn);
		return NULL;
	}

	return hex_encode(md, md_len);
}

/*
 * Parse the Authority Information Access (AIA) extension
 * See RFC 6487, section 4.8.7 for details.
 * Returns NULL on failure, on success returns the AIA URI
 * (which has to be freed after use).
 */
int
x509_get_aia(X509 *x, const char *fn, char **out_aia)
{
	ACCESS_DESCRIPTION		*ad;
	AUTHORITY_INFO_ACCESS		*info;
	int				 crit, rc = 0;

	assert(*out_aia == NULL);

	info = X509_get_ext_d2i(x, NID_info_access, &crit, NULL);
	if (info == NULL) {
		if (crit != -1) {
			warnx("%s: RFC 6487 section 4.8.7: error parsing AIA",
			    fn);
			return 0;
		}
		return 1;
	}

	if (crit != 0) {
		warnx("%s: RFC 6487 section 4.8.7: "
		    "AIA: extension not non-critical", fn);
		goto out;
	}

	if ((X509_get_extension_flags(x) & EXFLAG_SS) != 0) {
		warnx("%s: RFC 6487 section 4.8.7: AIA must be absent from "
		    "a self-signed certificate", fn);
		goto out;
	}

	if (sk_ACCESS_DESCRIPTION_num(info) != 1) {
		warnx("%s: RFC 6487 section 4.8.7: AIA: "
		    "want 1 element, have %d", fn,
		    sk_ACCESS_DESCRIPTION_num(info));
		goto out;
	}

	ad = sk_ACCESS_DESCRIPTION_value(info, 0);
	if (OBJ_obj2nid(ad->method) != NID_ad_ca_issuers) {
		warnx("%s: RFC 6487 section 4.8.7: AIA: "
		    "expected caIssuers, have %d", fn, OBJ_obj2nid(ad->method));
		goto out;
	}

	if (!x509_location(fn, "AIA: caIssuers", ad->location, out_aia))
		goto out;

	rc = 1;

 out:
	AUTHORITY_INFO_ACCESS_free(info);
	return rc;
}

/*
 * Parse the Subject Information Access (SIA) extension for an EE cert.
 * See RFC 6487, section 4.8.8.2 for details.
 * Returns NULL on failure, on success returns the SIA signedObject URI
 * (which has to be freed after use).
 */
int
x509_get_sia(X509 *x, const char *fn, char **out_sia)
{
	ACCESS_DESCRIPTION		*ad;
	AUTHORITY_INFO_ACCESS		*info;
	ASN1_OBJECT			*oid;
	int				 i, crit, rc = 0;

	assert(*out_sia == NULL);

	info = X509_get_ext_d2i(x, NID_sinfo_access, &crit, NULL);
	if (info == NULL) {
		if (crit != -1) {
			warnx("%s: error parsing SIA", fn);
			return 0;
		}
		return 1;
	}

	if (crit != 0) {
		warnx("%s: RFC 6487 section 4.8.8: "
		    "SIA: extension not non-critical", fn);
		goto out;
	}

	for (i = 0; i < sk_ACCESS_DESCRIPTION_num(info); i++) {
		char	*sia;

		ad = sk_ACCESS_DESCRIPTION_value(info, i);
		oid = ad->method;

		/*
		 * XXX: RFC 6487 4.8.8.2 states that the accessMethod MUST be
		 * signedObject. However, rpkiNotify accessMethods currently
		 * exist in the wild. Consider removing this special case.
		 * See also https://www.rfc-editor.org/errata/eid7239.
		 */
		if (OBJ_cmp(oid, notify_oid) == 0) {
			if (verbose > 1)
				warnx("%s: RFC 6487 section 4.8.8.2: SIA should"
				    " not contain rpkiNotify accessMethod", fn);
			continue;
		}
		if (OBJ_cmp(oid, signedobj_oid) != 0) {
			char buf[128];

			OBJ_obj2txt(buf, sizeof(buf), oid, 0);
			warnx("%s: RFC 6487 section 4.8.8.2: unexpected"
			    " accessMethod: %s", fn, buf);
			goto out;
		}

		sia = NULL;
		if (!x509_location(fn, "SIA: signedObject", ad->location, &sia))
			goto out;

		if (*out_sia == NULL && strncasecmp(sia, RSYNC_PROTO,
		    RSYNC_PROTO_LEN) == 0) {
			const char *p = sia + RSYNC_PROTO_LEN;
			size_t fnlen, plen;

			if (filemode) {
				*out_sia = sia;
				continue;
			}

			fnlen = strlen(fn);
			plen = strlen(p);

			if (fnlen < plen || strcmp(p, fn + fnlen - plen) != 0) {
				warnx("%s: mismatch between pathname and SIA "
				    "(%s)", fn, sia);
				free(sia);
				goto out;
			}

			*out_sia = sia;
			continue;
		}
		if (verbose)
			warnx("%s: RFC 6487 section 4.8.8: SIA: "
			    "ignoring location %s", fn, sia);
		free(sia);
	}

	if (*out_sia == NULL) {
		warnx("%s: RFC 6487 section 4.8.8.2: "
		    "SIA without rsync accessLocation", fn);
		goto out;
	}

	rc = 1;

 out:
	AUTHORITY_INFO_ACCESS_free(info);
	return rc;
}

/*
 * Extract the notBefore of a certificate.
 */
int
x509_get_notbefore(X509 *x, const char *fn, time_t *tt)
{
	const ASN1_TIME	*at;

	at = X509_get0_notBefore(x);
	if (at == NULL) {
		warnx("%s: X509_get0_notBefore failed", fn);
		return 0;
	}
	if (!x509_get_time(at, tt)) {
		warnx("%s: ASN1_TIME_to_tm failed", fn);
		return 0;
	}
	return 1;
}

/*
 * Extract the notAfter from a certificate.
 */
int
x509_get_notafter(X509 *x, const char *fn, time_t *tt)
{
	const ASN1_TIME	*at;

	at = X509_get0_notAfter(x);
	if (at == NULL) {
		warnx("%s: X509_get0_notafter failed", fn);
		return 0;
	}
	if (!x509_get_time(at, tt)) {
		warnx("%s: ASN1_TIME_to_tm failed", fn);
		return 0;
	}
	return 1;
}

/*
 * Check whether all RFC 3779 extensions are set to inherit.
 * Return 1 if both AS & IP are set to inherit.
 * Return 0 on failure (such as missing extensions or no inheritance).
 */
int
x509_inherits(X509 *x)
{
	STACK_OF(IPAddressFamily)	*addrblk = NULL;
	ASIdentifiers			*asidentifiers = NULL;
	const IPAddressFamily		*af;
	int				 crit, i, rc = 0;

	addrblk = X509_get_ext_d2i(x, NID_sbgp_ipAddrBlock, &crit, NULL);
	if (addrblk == NULL) {
		if (crit != -1)
			warnx("error parsing ipAddrBlock");
		goto out;
	}

	/*
	 * Check by hand, since X509v3_addr_inherits() success only means that
	 * at least one address family inherits, not all of them.
	 */
	for (i = 0; i < sk_IPAddressFamily_num(addrblk); i++) {
		af = sk_IPAddressFamily_value(addrblk, i);
		if (af->ipAddressChoice->type != IPAddressChoice_inherit)
			goto out;
	}

	asidentifiers = X509_get_ext_d2i(x, NID_sbgp_autonomousSysNum, NULL,
	    NULL);
	if (asidentifiers == NULL) {
		if (crit != -1)
			warnx("error parsing asIdentifiers");
		goto out;
	}

	/* We need to have AS numbers and don't want RDIs. */
	if (asidentifiers->asnum == NULL || asidentifiers->rdi != NULL)
		goto out;
	if (!X509v3_asid_inherits(asidentifiers))
		goto out;

	rc = 1;
 out:
	ASIdentifiers_free(asidentifiers);
	sk_IPAddressFamily_pop_free(addrblk, IPAddressFamily_free);
	return rc;
}

/*
 * Check whether at least one RFC 3779 extension is set to inherit.
 * Return 1 if an inherit element is encountered in AS or IP.
 * Return 0 otherwise.
 */
int
x509_any_inherits(X509 *x)
{
	STACK_OF(IPAddressFamily)	*addrblk = NULL;
	ASIdentifiers			*asidentifiers = NULL;
	int				 crit, rc = 0;

	addrblk = X509_get_ext_d2i(x, NID_sbgp_ipAddrBlock, &crit, NULL);
	if (addrblk == NULL && crit != -1)
		warnx("error parsing ipAddrBlock");
	if (X509v3_addr_inherits(addrblk))
		rc = 1;

	asidentifiers = X509_get_ext_d2i(x, NID_sbgp_autonomousSysNum, &crit,
	    NULL);
	if (asidentifiers == NULL && crit != -1)
		warnx("error parsing asIdentifiers");
	if (X509v3_asid_inherits(asidentifiers))
		rc = 1;

	ASIdentifiers_free(asidentifiers);
	sk_IPAddressFamily_pop_free(addrblk, IPAddressFamily_free);
	return rc;
}

/*
 * Parse the very specific subset of information in the CRL distribution
 * point extension.
 * See RFC 6487, section 4.8.6 for details.
 * Returns NULL on failure, the crl URI on success which has to be freed
 * after use.
 */
int
x509_get_crl(X509 *x, const char *fn, char **out_crl)
{
	CRL_DIST_POINTS		*crldp;
	DIST_POINT		*dp;
	GENERAL_NAMES		*names;
	GENERAL_NAME		*name;
	int			 i, crit, rc = 0;

	assert(*out_crl == NULL);

	crldp = X509_get_ext_d2i(x, NID_crl_distribution_points, &crit, NULL);
	if (crldp == NULL) {
		if (crit != -1) {
			warnx("%s: RFC 6487 section 4.8.6: failed to parse "
			    "CRL distribution points", fn);
			return 0;
		}
		return 1;
	}

	if (crit != 0) {
		warnx("%s: RFC 6487 section 4.8.6: "
		    "CRL distribution point: extension not non-critical", fn);
		goto out;
	}

	if (sk_DIST_POINT_num(crldp) != 1) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "want 1 element, have %d", fn,
		    sk_DIST_POINT_num(crldp));
		goto out;
	}

	dp = sk_DIST_POINT_value(crldp, 0);
	if (dp->CRLissuer != NULL) {
		warnx("%s: RFC 6487 section 4.8.6: CRL CRLIssuer field"
		    " disallowed", fn);
		goto out;
	}
	if (dp->reasons != NULL) {
		warnx("%s: RFC 6487 section 4.8.6: CRL Reasons field"
		    " disallowed", fn);
		goto out;
	}
	if (dp->distpoint == NULL) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "no distribution point name", fn);
		goto out;
	}
	if (dp->distpoint->dpname != NULL) {
		warnx("%s: RFC 6487 section 4.8.6: nameRelativeToCRLIssuer"
		    " disallowed", fn);
		goto out;
	}
	/* Need to hardcode the alternative 0 due to missing macros or enum. */
	if (dp->distpoint->type != 0) {
		warnx("%s: RFC 6487 section 4.8.6: CRL DistributionPointName:"
		    " expected fullName, have %d", fn, dp->distpoint->type);
		goto out;
	}

	names = dp->distpoint->name.fullname;
	for (i = 0; i < sk_GENERAL_NAME_num(names); i++) {
		char	*crl = NULL;

		name = sk_GENERAL_NAME_value(names, i);

		if (!x509_location(fn, "CRL distribution point", name, &crl))
			goto out;

		if (*out_crl == NULL && strncasecmp(crl, RSYNC_PROTO,
		    RSYNC_PROTO_LEN) == 0) {
			*out_crl = crl;
			continue;
		}
		if (verbose)
			warnx("%s: ignoring CRL distribution point %s",
			    fn, crl);
		free(crl);
	}

	if (*out_crl == NULL) {
		warnx("%s: RFC 6487 section 4.8.6: no rsync URI "
		    "in CRL distributionPoint", fn);
		goto out;
	}

	rc = 1;

 out:
	CRL_DIST_POINTS_free(crldp);
	return rc;
}

/*
 * Convert passed ASN1_TIME to time_t *t.
 * Returns 1 on success and 0 on failure.
 */
int
x509_get_time(const ASN1_TIME *at, time_t *t)
{
	struct tm	 tm;

	*t = 0;
	memset(&tm, 0, sizeof(tm));
	/* Fail instead of silently falling back to the current time. */
	if (at == NULL)
		return 0;
	if (!ASN1_TIME_to_tm(at, &tm))
		return 0;
	if ((*t = timegm(&tm)) == -1)
		errx(1, "timegm failed");
	return 1;
}

/*
 * Extract and validate an accessLocation, RFC 6487, 4.8 and RFC 8182, 3.2.
 * Returns 0 on failure and 1 on success.
 */
int
x509_location(const char *fn, const char *descr, GENERAL_NAME *location,
    char **out)
{
	ASN1_IA5STRING	*uri;

	assert(*out == NULL);

	if (location->type != GEN_URI) {
		warnx("%s: RFC 6487 section 4.8: %s not URI", fn, descr);
		return 0;
	}

	uri = location->d.uniformResourceIdentifier;

	if (!valid_uri(uri->data, uri->length, NULL)) {
		warnx("%s: RFC 6487 section 4.8: %s bad location", fn, descr);
		return 0;
	}

	if ((*out = strndup(uri->data, uri->length)) == NULL)
		err(1, NULL);

	return 1;
}

/*
 * Check that subject or issuer only contain commonName and serialNumber.
 * Return 0 on failure.
 */
int
x509_valid_name(const char *fn, const char *descr, const X509_NAME *xn)
{
	const X509_NAME_ENTRY *ne;
	const ASN1_OBJECT *ao;
	const ASN1_STRING *as;
	int cn = 0, sn = 0;
	int i, nid;

	for (i = 0; i < X509_NAME_entry_count(xn); i++) {
		if ((ne = X509_NAME_get_entry(xn, i)) == NULL) {
			warnx("%s: X509_NAME_get_entry", fn);
			return 0;
		}
		if ((ao = X509_NAME_ENTRY_get_object(ne)) == NULL) {
			warnx("%s: X509_NAME_ENTRY_get_object", fn);
			return 0;
		}

		nid = OBJ_obj2nid(ao);
		switch (nid) {
		case NID_commonName:
			if (cn++ > 0) {
				warnx("%s: duplicate commonName in %s",
				    fn, descr);
				return 0;
			}
			if ((as = X509_NAME_ENTRY_get_data(ne)) == NULL) {
				warnx("%s: X509_NAME_ENTRY_get_data failed",
				    fn);
				return 0;
			}
/*
 * The following check can be enabled after AFRINIC re-issues CA certs.
 * https://lists.afrinic.net/pipermail/dbwg/2023-March/000436.html
 */
#if 0
			/*
			 * XXX - For some reason RFC 8209, section 3.1.1 decided
			 * to allow UTF8String for BGPsec Router Certificates.
			 */
			if (ASN1_STRING_type(as) != V_ASN1_PRINTABLESTRING) {
				warnx("%s: RFC 6487 section 4.5: commonName is"
				    " not PrintableString", fn);
				return 0;
			}
#endif
			break;
		case NID_serialNumber:
			if (sn++ > 0) {
				warnx("%s: duplicate serialNumber in %s",
				    fn, descr);
				return 0;
			}
			break;
		case NID_undef:
			warnx("%s: OBJ_obj2nid failed", fn);
			return 0;
		default:
			warnx("%s: RFC 6487 section 4.5: unexpected attribute"
			    " %s in %s", fn, nid2str(nid), descr);
			return 0;
		}
	}

	if (cn == 0) {
		warnx("%s: RFC 6487 section 4.5: %s missing commonName",
		    fn, descr);
		return 0;
	}

	return 1;
}

/*
 * Convert an ASN1_INTEGER into a hexstring, enforcing that it is non-negative
 * and representable by at most 20 octets (RFC 5280, section 4.1.2.2).
 * Returned string needs to be freed by the caller.
 */
char *
x509_convert_seqnum(const char *fn, const ASN1_INTEGER *i)
{
	BIGNUM	*seqnum = NULL;
	char	*s = NULL;

	if (i == NULL)
		goto out;

	if (ASN1_STRING_length(i) > 20) {
		warnx("%s: %s: want 20 octets or fewer, have more.",
		    __func__, fn);
		goto out;
	}

	seqnum = ASN1_INTEGER_to_BN(i, NULL);
	if (seqnum == NULL) {
		warnx("%s: ASN1_INTEGER_to_BN error", fn);
		goto out;
	}

	if (BN_is_negative(seqnum)) {
		warnx("%s: %s: want positive integer, have negative.",
		    __func__, fn);
		goto out;
	}

	s = BN_bn2hex(seqnum);
	if (s == NULL)
		warnx("%s: BN_bn2hex error", fn);

 out:
	BN_free(seqnum);
	return s;
}

/*
 * Find the closest expiry moment by walking the chain of authorities.
 */
time_t
x509_find_expires(time_t notafter, struct auth *a, struct crl_tree *crlt)
{
	struct crl	*crl;
	time_t		 expires;

	expires = notafter;

	for (; a != NULL; a = a->issuer) {
		if (expires > a->cert->notafter)
			expires = a->cert->notafter;
		crl = crl_get(crlt, a);
		if (crl != NULL && expires > crl->nextupdate)
			expires = crl->nextupdate;
	}

	return expires;
}

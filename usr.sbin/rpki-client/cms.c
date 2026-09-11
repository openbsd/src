/*	$OpenBSD: cms.c,v 1.69 2026/09/11 06:25:00 tb Exp $ */
/*
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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/cms.h>

#include "extern.h"

#define ASN1_TAG_SEQUENCE		0x30	/* X.690, section 8.9 */
#define ASN1_LENGTH_INDEFINITE		0x80	/* X.690, section 8.1.3.6.1 */

extern int filemode;

static int
cms_extract_econtent(const char *fn, CMS_ContentInfo *cms, const uint8_t **res,
    size_t *rsz)
{
	ASN1_OCTET_STRING		**os = NULL;

	if ((os = CMS_get0_content(cms)) == NULL || *os == NULL) {
		warnx("%s: RFC 6488 section 2.1.4: "
		    "eContent: zero-length content", fn);
		return 0;
	}

	if ((*rsz = ASN1_STRING_length(*os)) == 0) {
		warnx("%s: RFC 6488 section 2.1.4: "
		    "eContent: zero-length content", fn);
		return 0;
	}
	if (*rsz > MAX_FILE_SIZE) {
		warnx("%s: overlong eContent of length %zu", fn, *rsz);
		return 0;
	}

	*res = ASN1_STRING_get0_data(*os);
	return 1;
}

static int
cms_get_signtime(const char *fn, X509_ATTRIBUTE *attr, time_t *signtime)
{
	const ASN1_TIME		*at;
	const char		*time_str = "UTCTime";
	int			 time_type = V_ASN1_UTCTIME;

	*signtime = 0;
	at = X509_ATTRIBUTE_get0_data(attr, 0, time_type, NULL);
	if (at == NULL) {
		time_str = "GeneralizedTime";
		time_type = V_ASN1_GENERALIZEDTIME;
		at = X509_ATTRIBUTE_get0_data(attr, 0, time_type, NULL);
		if (at == NULL) {
			warnx("%s: CMS signing-time issue", fn);
			return 0;
		}
		warnx("%s: GeneralizedTime instead of UTCTime", fn);
	}

	if (!x509_get_time(at, signtime)) {
		warnx("%s: failed to convert %s", fn, time_str);
		return 0;
	}

	return 1;
}

static int
cms_SignerInfo_check_attributes(const char *fn, const CMS_SignerInfo *si,
    time_t *signtime)
{
	char buf[128];
	const ASN1_OBJECT *obj;
	int i, nattrs;
	int has_ct = 0, has_md = 0, has_st = 0;

	*signtime = 0;

	nattrs = CMS_signed_get_attr_count(si);
	if (nattrs <= 0) {
		warnx("%s: RFC 6488: error extracting signedAttrs", fn);
		return 0;
	}
	for (i = 0; i < nattrs; i++) {
		X509_ATTRIBUTE *attr;

		attr = CMS_signed_get_attr(si, i);
		if (attr == NULL || X509_ATTRIBUTE_count(attr) != 1) {
			warnx("%s: RFC 6488: bad signed attribute encoding",
			    fn);
			return 0;
		}

		obj = X509_ATTRIBUTE_get0_object(attr);
		if (obj == NULL) {
			warnx("%s: RFC 6488: bad signed attribute", fn);
			return 0;
		}
		if (OBJ_cmp(obj, cnt_type_oid) == 0) {
			if (has_ct++ != 0) {
				warnx("%s: RFC 6488: duplicate "
				    "signed attribute", fn);
				return 0;
			}
		} else if (OBJ_cmp(obj, msg_dgst_oid) == 0) {
			if (has_md++ != 0) {
				warnx("%s: RFC 6488: duplicate "
				    "signed attribute", fn);
				return 0;
			}
		} else if (OBJ_cmp(obj, sign_time_oid) == 0) {
			if (has_st++ != 0) {
				warnx("%s: RFC 6488: duplicate "
				    "signed attribute", fn);
				return 0;
			}
			if (!cms_get_signtime(fn, attr, signtime))
				return 0;
		} else {
			OBJ_obj2txt(buf, sizeof(buf), obj, 1);
			warnx("%s: RFC 6488: "
			    "CMS has unexpected signed attribute %s",
			    fn, buf);
			return 0;
		}
	}

	if (!has_ct || !has_md) {
		/* RFC 9589, section 4 */
		warnx("%s: RFC 6488: CMS missing required "
		    "signed attribute", fn);
		return 0;
	}

	if (!has_st) {
		/* RFC 9589, section 4 */
		warnx("%s: missing CMS signing-time attribute", fn);
		return 0;
	}

	if (CMS_unsigned_get_attr_count(si) != -1) {
		warnx("%s: RFC 6488: CMS has unsignedAttrs", fn);
		return 0;
	}

	return 1;
}

static int
cms_check_SignerInfo(const char *fn, CMS_ContentInfo *cms,
    const ASN1_OBJECT *oid, struct cert *cert, time_t *signtime)
{
	char				 buf[128], obuf[128];
	const ASN1_OBJECT		*obj, *octype;
	ASN1_OCTET_STRING		*kid = NULL;
	long				 version;
	STACK_OF(CMS_SignerInfo)	*sinfos;
	CMS_SignerInfo			*si;
	X509_ALGOR			*pdig, *psig;
	int				 nid;

	/* Should only return NULL if cms is not of type SignedData. */
	if ((sinfos = CMS_get0_SignerInfos(cms)) == NULL) {
		if ((obj = CMS_get0_type(cms)) == NULL) {
			warnx("%s: RFC 6488: missing content-type", fn);
			return 0;
		}
		OBJ_obj2txt(buf, sizeof(buf), obj, 1);
		warnx("%s: RFC 6488: no signerInfo in CMS object of type %s",
		    fn, buf);
		return 0;
	}
	if (sk_CMS_SignerInfo_num(sinfos) != 1) {
		warnx("%s: RFC 6488: CMS has multiple signerInfos", fn);
		return 0;
	}
	si = sk_CMS_SignerInfo_value(sinfos, 0);

	if (!CMS_get_version(cms, &version)) {
		warnx("%s: Failed to retrieve SignedData version", fn);
		return 0;
	}
	if (version != 3) {
		warnx("%s: SignedData version %ld != 3", fn, version);
		return 0;
	}
	if (!CMS_SignerInfo_get_version(si, &version)) {
		warnx("%s: Failed to retrieve SignerInfo version", fn);
		return 0;
	}
	if (version != 3) {
		warnx("%s: SignerInfo version %ld != 3", fn, version);
		return 0;
	}

	if (!cms_SignerInfo_check_attributes(fn, si, signtime))
		return 0;

	/* Check digest and signature algorithms (RFC 7935) */
	CMS_SignerInfo_get0_algs(si, NULL, NULL, &pdig, &psig);

	X509_ALGOR_get0(&obj, NULL, NULL, pdig);
	nid = OBJ_obj2nid(obj);
	if (nid != NID_sha256) {
		warnx("%s: RFC 6488: wrong digest %s, want %s", fn,
		    nid2str(nid), LN_sha256);
		return 0;
	}
	X509_ALGOR_get0(&obj, NULL, NULL, psig);
	nid = OBJ_obj2nid(obj);
	/* RFC7935 last paragraph of section 2 specifies the allowed psig */
	if (experimental && nid == NID_ecdsa_with_SHA256) {
		if (verbose)
			warnx("%s: P-256 support is experimental", fn);
	} else if (nid != NID_rsaEncryption &&
	    nid != NID_sha256WithRSAEncryption) {
		warnx("%s: RFC 6488: wrong signature algorithm %s, want %s",
		    fn, nid2str(nid), LN_rsaEncryption);
		return 0;
	}

	/* RFC 6488 section 2.1.3.1: check the object's eContentType. */

	obj = CMS_get0_eContentType(cms);
	if (obj == NULL) {
		warnx("%s: RFC 6488 section 2.1.3.1: eContentType: "
		    "OID object is NULL", fn);
		return 0;
	}
	if (OBJ_cmp(obj, oid) != 0) {
		OBJ_obj2txt(buf, sizeof(buf), obj, 1);
		OBJ_obj2txt(obuf, sizeof(obuf), oid, 1);
		warnx("%s: RFC 6488 section 2.1.3.1: eContentType: "
		    "unknown OID: %s, want %s", fn, buf, obuf);
		return 0;
	}

	/* Compare content-type with eContentType */
	octype = CMS_signed_get0_data_by_OBJ(si, cnt_type_oid,
	    -3, V_ASN1_OBJECT);
	/*
	 * Since lastpos == -3, octype can be NULL for 4 reasons:
	 * 1. requested attribute OID is missing
	 * 2. signedAttrs contains multiple attributes with requested OID
	 * 3. attribute with requested OID has multiple values (malformed)
	 * 4. X509_ATTRIBUTE_get0_data() returned NULL. This is also malformed,
	 *    but libcrypto will create, sign, and verify such objects.
	 * Reasons 1 and 2 are excluded because has_ct == 1. We don't know which
	 * one of 3 or 4 we hit. Doesn't matter, drop the garbage on the floor.
	 */
	if (octype == NULL) {
		warnx("%s: RFC 6488, section 2.1.6.4.1: malformed value "
		    "for content-type attribute", fn);
		return 0;
	}
	if (OBJ_cmp(obj, octype) != 0) {
		OBJ_obj2txt(buf, sizeof(buf), obj, 1);
		OBJ_obj2txt(obuf, sizeof(obuf), octype, 1);
		warnx("%s: RFC 6488: eContentType does not match Content-Type "
		    "OID: %s, want %s", fn, buf, obuf);
		return 0;
	}

	if (CMS_SignerInfo_get0_signer_id(si, &kid, NULL, NULL) != 1 ||
	    kid == NULL) {
		warnx("%s: RFC 6488: could not extract SKI from SID", fn);
		return 0;
	}
	if (CMS_SignerInfo_cert_cmp(si, cert->x509) != 0) {
		warnx("%s: RFC 6488: wrong cert referenced by SignerInfo", fn);
		return 0;
	}

	return 1;
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
cms_parse_validate(struct cert **out_cert, const char *fn, enum rtype rtype,
    int talid, const unsigned char *der, size_t len)
{
	void				*obj = NULL, *ret_obj = NULL;
	const struct signed_obj		*sobj;
	struct cert			*cert = NULL;
	const unsigned char		*oder;
	CMS_ContentInfo			*cms = NULL;
	STACK_OF(X509)			*certs = NULL;
	STACK_OF(X509_CRL)		*crls = NULL;
	const uint8_t			*econtent = NULL;
	size_t				 econtent_len = 0;
	time_t				 signtime = 0;

	assert(*out_cert == NULL);

	sobj = cms_object_from_rtype(fn, rtype);

	/* just fail for empty buffers, the warning was printed elsewhere */
	if (der == NULL)
		goto out;

	if (len < 2) {
		warnx("%s: RFC 6488: CMS encoding too short", fn);
		goto out;
	}
	if (der[0] == ASN1_TAG_SEQUENCE && der[1] == ASN1_LENGTH_INDEFINITE) {
		warnx("%s: RFC 6488: indefinite length encoding disallowed "
		    "in DER", fn);
		if (!filemode)
			goto out;
	}

	oder = der;
	if ((cms = d2i_CMS_ContentInfo(NULL, &der, len)) == NULL) {
		warnx("%s: RFC 6488: failed CMS parse", fn);
		goto out;
	}
	if (der != oder + len) {
		warnx("%s: %td bytes trailing garbage", fn, oder + len - der);
		goto out;
	}

	/*
	 * The CMS is self-signed with a signing certificate.
	 * Verify that the self-signage is correct and set up internal
	 * structs so that the following CMS API calls work correctly.
	 */
	if (!CMS_verify(cms, NULL, NULL, NULL, NULL,
	    CMS_NO_SIGNER_CERT_VERIFY)) {
		warnx("%s: CMS verification error", fn);
		goto out;
	}

	/*
	 * Check that there are no CRLs in this CMS message.
	 * XXX - can only error check for OpenSSL >= 3.4.
	 */
	crls = CMS_get1_crls(cms);
	if (crls != NULL && sk_X509_CRL_num(crls) != 0) {
		warnx("%s: RFC 6488: CMS has CRLs", fn);
		goto out;
	}

	/*
	 * The self-signing certificate is further signed by the input
	 * signing authority according to RFC 6488, 2.1.4.
	 * We extract that certificate now for later verification.
	 */

	certs = CMS_get0_signers(cms);
	if (certs == NULL || sk_X509_num(certs) != 1) {
		warnx("%s: RFC 6488 section 2.1.4: eContent: "
		    "want 1 signer, have %d", fn, sk_X509_num(certs));
		goto out;
	}

	cert = cert_parse_ee_cert(fn, talid, sk_X509_value(certs, 0));
	if (cert == NULL)
		goto out;

	/* RFC 6488 section 3 verify the CMS */
	if (!cms_check_SignerInfo(fn, cms, sobj->oid(), cert, &signtime))
		goto out;

	if (signtime > cert->notafter)
		warnx("%s: dating issue: CMS signing-time after X.509 notAfter",
		    fn);

	if (!cms_extract_econtent(fn, cms, &econtent, &econtent_len))
		goto out;

	obj = sobj->new(len, signtime);
	if (!sobj->cert_info(fn, obj, cert))
		goto out;
	if (!sobj->parse_econtent(fn, obj, econtent, econtent_len))
		goto out;
	if (!sobj->validate(fn, obj, cert))
		goto out;

	*out_cert = cert;
	cert = NULL;

	ret_obj = obj;
	obj = NULL;

 out:
	sobj->free(obj);
	cert_free(cert);
	sk_X509_CRL_pop_free(crls, X509_CRL_free);
	sk_X509_free(certs);
	CMS_ContentInfo_free(cms);
	return ret_obj;
}

void *
signed_object_parse(struct cert **out_cert, const char *fn, enum rtype rtype,
    int talid, const unsigned char *der, size_t len)
{
	return cms_parse_validate(out_cert, fn, rtype, talid, der, len);
}

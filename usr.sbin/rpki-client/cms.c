/*	$OpenBSD: cms.c,v 1.21 2022/08/12 13:19:02 tb Exp $ */
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
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/cms.h>

#include "extern.h"

extern ASN1_OBJECT	*cnt_type_oid;
extern ASN1_OBJECT	*msg_dgst_oid;
extern ASN1_OBJECT	*sign_time_oid;
extern ASN1_OBJECT	*bin_sign_time_oid;

/*
 * Parse and validate a self-signed CMS message, where the signing X509
 * certificate has been hashed to dgst (optional).
 * Conforms to RFC 6488.
 * The eContentType of the message must be an oid object.
 * Return the eContent as a string and set "rsz" to be its length.
 */
unsigned char *
cms_parse_validate(X509 **xp, const char *fn, const unsigned char *der,
    size_t derlen, const ASN1_OBJECT *oid, size_t *rsz)
{
	char				 buf[128], obuf[128];
	const ASN1_OBJECT		*obj, *octype;
	ASN1_OCTET_STRING		**os = NULL, *kid = NULL;
	CMS_ContentInfo			*cms;
	int				 rc = 0;
	STACK_OF(X509)			*certs = NULL;
	STACK_OF(X509_CRL)		*crls;
	STACK_OF(CMS_SignerInfo)	*sinfos;
	CMS_SignerInfo			*si;
	X509_ALGOR			*pdig, *psig;
	unsigned char			*res = NULL;
	int				 i, nattrs, nid;
	int				 has_ct = 0, has_md = 0, has_st = 0,
					 has_bst = 0;

	*rsz = 0;
	*xp = NULL;

	/* just fail for empty buffers, the warning was printed elsewhere */
	if (der == NULL)
		return NULL;

	if ((cms = d2i_CMS_ContentInfo(NULL, &der, derlen)) == NULL) {
		cryptowarnx("%s: RFC 6488: failed CMS parse", fn);
		goto out;
	}

	/*
	 * The CMS is self-signed with a signing certifiate.
	 * Verify that the self-signage is correct.
	 */

	if (!CMS_verify(cms, NULL, NULL, NULL, NULL,
	    CMS_NO_SIGNER_CERT_VERIFY)) {
		cryptowarnx("%s: RFC 6488: CMS not self-signed", fn);
		goto out;
	}

	/* RFC 6488 section 3 verify the CMS */
	/* the version of SignedData and SignerInfos can't be verified */

	sinfos = CMS_get0_SignerInfos(cms);
	assert(sinfos != NULL);
	if (sk_CMS_SignerInfo_num(sinfos) != 1) {
		cryptowarnx("%s: RFC 6488: CMS has multiple signerInfos", fn);
		goto out;
	}
	si = sk_CMS_SignerInfo_value(sinfos, 0);

	nattrs = CMS_signed_get_attr_count(si);
	if (nattrs <= 0) {
		cryptowarnx("%s: RFC 6488: error extracting signedAttrs", fn);
		goto out;
	}
	for (i = 0; i < nattrs; i++) {
		X509_ATTRIBUTE *attr;

		attr = CMS_signed_get_attr(si, i);
		if (attr == NULL || X509_ATTRIBUTE_count(attr) != 1) {
			cryptowarnx("%s: RFC 6488: "
			    "bad signed attribute encoding", fn);
			goto out;
		}

		obj = X509_ATTRIBUTE_get0_object(attr);
		if (obj == NULL) {
			cryptowarnx("%s: RFC 6488: bad signed attribute", fn);
			goto out;
		}
		if (OBJ_cmp(obj, cnt_type_oid) == 0) {
			if (has_ct++ != 0) {
				cryptowarnx("%s: RFC 6488: duplicate "
				    "signed attribute", fn);
				goto out;
			}
		} else if (OBJ_cmp(obj, msg_dgst_oid) == 0) {
			if (has_md++ != 0) {
				cryptowarnx("%s: RFC 6488: duplicate "
				    "signed attribute", fn);
				goto out;
			}
		} else if (OBJ_cmp(obj, sign_time_oid) == 0) {
			if (has_st++ != 0) {
				cryptowarnx("%s: RFC 6488: duplicate "
				    "signed attribute", fn);
				goto out;
			}
		} else if (OBJ_cmp(obj, bin_sign_time_oid) == 0) {
			if (has_bst++ != 0) {
				cryptowarnx("%s: RFC 6488: duplicate "
				    "signed attribute", fn);
				goto out;
			}
		} else {
			OBJ_obj2txt(buf, sizeof(buf), obj, 1);
			cryptowarnx("%s: RFC 6488: "
			    "CMS has unexpected signed attribute %s",
			    fn, buf);
			goto out;
		}
	}
	if (!has_ct || !has_md) {
		cryptowarnx("%s: RFC 6488: CMS missing required "
		    "signed attribute", fn);
		goto out;
	}
	if (CMS_unsigned_get_attr_count(si) != -1) {
		cryptowarnx("%s: RFC 6488: CMS has unsignedAttrs", fn);
		goto out;
	}

	/* Check digest and signature algorithms */
	CMS_SignerInfo_get0_algs(si, NULL, NULL, &pdig, &psig);
	X509_ALGOR_get0(&obj, NULL, NULL, pdig);
	nid = OBJ_obj2nid(obj);
	if (nid != NID_sha256) {
		warnx("%s: RFC 6488: wrong digest %s, want %s", fn,
		    OBJ_nid2ln(nid), OBJ_nid2ln(NID_sha256));
		goto out;
	}
	X509_ALGOR_get0(&obj, NULL, NULL, psig);
	nid = OBJ_obj2nid(obj);
	/* RFC7395 last paragraph of section 2 specifies the allowed psig */
	if (nid != NID_rsaEncryption && nid != NID_sha256WithRSAEncryption) {
		warnx("%s: RFC 6488: wrong signature algorithm %s, want %s",
		    fn, OBJ_nid2ln(nid), OBJ_nid2ln(NID_rsaEncryption));
		goto out;
	}

	/* RFC 6488 section 2.1.3.1: check the object's eContentType. */

	obj = CMS_get0_eContentType(cms);
	if (obj == NULL) {
		warnx("%s: RFC 6488 section 2.1.3.1: eContentType: "
		    "OID object is NULL", fn);
		goto out;
	}
	if (OBJ_cmp(obj, oid) != 0) {
		OBJ_obj2txt(buf, sizeof(buf), obj, 1);
		OBJ_obj2txt(obuf, sizeof(obuf), oid, 1);
		warnx("%s: RFC 6488 section 2.1.3.1: eContentType: "
		    "unknown OID: %s, want %s", fn, buf, obuf);
		goto out;
	}

	/* Compare content-type with eContentType */
	octype = CMS_signed_get0_data_by_OBJ(si, cnt_type_oid,
	    -3, V_ASN1_OBJECT);
	assert(octype != NULL);
	if (OBJ_cmp(obj, octype) != 0) {
		OBJ_obj2txt(buf, sizeof(buf), obj, 1);
		OBJ_obj2txt(obuf, sizeof(obuf), oid, 1);
		warnx("%s: RFC 6488: eContentType does not match Content-Type "
		    "OID: %s, want %s", fn, buf, obuf);
		goto out;
	}

	/*
	 * Check that there are no CRLS in this CMS message.
	 */
	crls = CMS_get1_crls(cms);
	if (crls != NULL) {
		sk_X509_CRL_pop_free(crls, X509_CRL_free);
		cryptowarnx("%s: RFC 6488: CMS has CRLs", fn);
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
	*xp = sk_X509_value(certs, 0);
	if (!X509_up_ref(*xp)) {
		*xp = NULL;
		goto out;
	}

	/* Cache X509v3 extensions, see X509_check_ca(3). */
	if (X509_check_purpose(*xp, -1, -1) <= 0) {
		cryptowarnx("%s: could not cache X509v3 extensions", fn);
		goto out;
	}

	if (CMS_SignerInfo_get0_signer_id(si, &kid, NULL, NULL) != 1 ||
	    kid == NULL) {
		warnx("%s: RFC 6488: could not extract SKI from SID", fn);
		goto out;
	}
	if (CMS_SignerInfo_cert_cmp(si, *xp) != 0) {
		warnx("%s: RFC 6488: wrong cert referenced by SignerInfo", fn);
		goto out;
	}

	/* Verify that we have eContent to disseminate. */

	if ((os = CMS_get0_content(cms)) == NULL || *os == NULL) {
		warnx("%s: RFC 6488 section 2.1.4: "
		    "eContent: zero-length content", fn);
		goto out;
	}

	/*
	 * Extract and duplicate the eContent.
	 * The CMS framework offers us no other way of easily managing
	 * this information; and since we're going to d2i it anyway,
	 * simply pass it as the desired underlying types.
	 */

	if ((res = malloc((*os)->length)) == NULL)
		err(1, NULL);
	memcpy(res, (*os)->data, (*os)->length);
	*rsz = (*os)->length;

	rc = 1;
out:
	sk_X509_free(certs);
	CMS_ContentInfo_free(cms);

	if (rc == 0) {
		X509_free(*xp);
		*xp = NULL;
	}

	return res;
}

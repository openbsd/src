/*	$OpenBSD: ct_oct.c,v 1.5 2021/12/18 15:59:50 jsing Exp $ */
/*
 * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#ifdef OPENSSL_NO_CT
# error "CT is disabled"
#endif

#include <limits.h>
#include <string.h>

#include <openssl/asn1.h>
#include <openssl/buffer.h>
#include <openssl/ct.h>
#include <openssl/err.h>

#include "bytestring.h"
#include "ct_local.h"

static int
o2i_SCT_signature_internal(SCT *sct, CBS *cbs)
{
	uint8_t hash_alg, sig_alg;
	CBS signature;

	if (sct->version != SCT_VERSION_V1) {
		CTerror(CT_R_UNSUPPORTED_VERSION);
		return 0;
	}

	/*
	 * Parse a digitally-signed element - see RFC 6962 section 3.2 and
	 * RFC 5246 sections 4.7 and 7.4.1.4.1.
	 */
	if (!CBS_get_u8(cbs, &hash_alg))
		goto err_invalid;
	if (!CBS_get_u8(cbs, &sig_alg))
		goto err_invalid;
	if (!CBS_get_u16_length_prefixed(cbs, &signature))
		goto err_invalid;
	if (CBS_len(cbs) != 0)
		goto err_invalid;

	/*
	 * Reject empty signatures since they are invalid for all supported
	 * algorithms (this really should be done by SCT_set1_signature()).
	 */
	if (CBS_len(&signature) == 0)
		goto err_invalid;

	sct->hash_alg = hash_alg;
	sct->sig_alg = sig_alg;

	if (SCT_get_signature_nid(sct) == NID_undef)
		goto err_invalid;

	if (!SCT_set1_signature(sct, CBS_data(&signature), CBS_len(&signature)))
		return 0;

	return 1;

 err_invalid:
	CTerror(CT_R_SCT_INVALID_SIGNATURE);
	return 0;
}

int
o2i_SCT_signature(SCT *sct, const unsigned char **in, size_t len)
{
	size_t sig_len;
	CBS cbs;

	CBS_init(&cbs, *in, len);

	if (!o2i_SCT_signature_internal(sct, &cbs))
		return -1;

	sig_len = len - CBS_len(&cbs);
	if (sig_len > INT_MAX)
		return -1;

	*in = CBS_data(&cbs);

	return sig_len;
}

static int
o2i_SCT_internal(SCT **out_sct, CBS *cbs)
{
	SCT *sct = NULL;
	uint8_t version;

	*out_sct = NULL;

	if ((sct = SCT_new()) == NULL)
		goto err;

	if (CBS_len(cbs) > MAX_SCT_SIZE)
		goto err_invalid;
	if (!CBS_peek_u8(cbs, &version))
		goto err_invalid;

	sct->version = version;

	if (version == SCT_VERSION_V1) {
		CBS extensions, log_id;
		uint64_t timestamp;

		/*
		 * Parse a v1 SignedCertificateTimestamp - see RFC 6962
		 * section 3.2.
		 */
		if (!CBS_get_u8(cbs, &version))
			goto err_invalid;
		if (!CBS_get_bytes(cbs, &log_id, CT_V1_LOG_ID_LEN))
			goto err_invalid;
		if (!CBS_get_u64(cbs, &timestamp))
			goto err_invalid;
		if (!CBS_get_u16_length_prefixed(cbs, &extensions))
			goto err_invalid;

		if (!CBS_stow(&log_id, &sct->log_id, &sct->log_id_len))
			goto err;

		sct->timestamp = timestamp;

		if (!CBS_stow(&extensions, &sct->ext, &sct->ext_len))
			goto err;

		if (!o2i_SCT_signature_internal(sct, cbs))
			goto err;

		if (CBS_len(cbs) != 0)
			goto err_invalid;
	} else {
		/* If not V1 just cache encoding. */
		if (!CBS_stow(cbs, &sct->sct, &sct->sct_len))
			goto err;
	}

	*out_sct = sct;

	return 1;

 err_invalid:
	CTerror(CT_R_SCT_INVALID);
 err:
	SCT_free(sct);

	return 0;
}

SCT *
o2i_SCT(SCT **psct, const unsigned char **in, size_t len)
{
	SCT *sct;
	CBS cbs;

	CBS_init(&cbs, *in, len);

	if (psct != NULL) {
		SCT_free(*psct);
		*psct = NULL;
	}

	if (!o2i_SCT_internal(&sct, &cbs))
		return NULL;

	if (psct != NULL)
		*psct = sct;

	*in = CBS_data(&cbs);

	return sct;
}

int
i2o_SCT_signature(const SCT *sct, unsigned char **out)
{
	size_t len;
	unsigned char *p = NULL, *pstart = NULL;

	if (!SCT_signature_is_complete(sct)) {
		CTerror(CT_R_SCT_INVALID_SIGNATURE);
		goto err;
	}

	if (sct->version != SCT_VERSION_V1) {
		CTerror(CT_R_UNSUPPORTED_VERSION);
		goto err;
	}

	/*
	 * (1 byte) Hash algorithm
	 * (1 byte) Signature algorithm
	 * (2 bytes + ?) Signature
	 */
	len = 4 + sct->sig_len;

	if (out != NULL) {
		if (*out != NULL) {
			p = *out;
			*out += len;
		} else {
			pstart = p = malloc(len);
			if (p == NULL) {
				CTerror(ERR_R_MALLOC_FAILURE);
				goto err;
			}
			*out = p;
		}

		*p++ = sct->hash_alg;
		*p++ = sct->sig_alg;
		s2n(sct->sig_len, p);
		memcpy(p, sct->sig, sct->sig_len);
	}

	return len;
 err:
	free(pstart);
	return -1;
}

int
i2o_SCT(const SCT *sct, unsigned char **out)
{
	size_t len;
	unsigned char *p = NULL, *pstart = NULL;

	if (!SCT_is_complete(sct)) {
		CTerror(CT_R_SCT_NOT_SET);
		goto err;
	}
	/*
	 * Fixed-length header: struct { (1 byte) Version sct_version; (32 bytes)
	 * log_id id; (8 bytes) uint64 timestamp; (2 bytes + ?) CtExtensions
	 * extensions; (1 byte) Hash algorithm (1 byte) Signature algorithm (2
	 * bytes + ?) Signature
	 */
	if (sct->version == SCT_VERSION_V1)
		len = 43 + sct->ext_len + 4 + sct->sig_len;
	else
		len = sct->sct_len;

	if (out == NULL)
		return len;

	if (*out != NULL) {
		p = *out;
		*out += len;
	} else {
		pstart = p = malloc(len);
		if (p == NULL) {
			CTerror(ERR_R_MALLOC_FAILURE);
			goto err;
		}
		*out = p;
	}

	if (sct->version == SCT_VERSION_V1) {
		*p++ = sct->version;
		memcpy(p, sct->log_id, CT_V1_HASHLEN);
		p += CT_V1_HASHLEN;
		l2n8(sct->timestamp, p);
		s2n(sct->ext_len, p);
		if (sct->ext_len > 0) {
			memcpy(p, sct->ext, sct->ext_len);
			p += sct->ext_len;
		}
		if (i2o_SCT_signature(sct, &p) <= 0)
			goto err;
	} else {
		memcpy(p, sct->sct, len);
	}

	return len;
 err:
	free(pstart);
	return -1;
}

STACK_OF(SCT) *
o2i_SCT_LIST(STACK_OF(SCT) **scts, const unsigned char **pp, size_t len)
{
	CBS cbs, cbs_scts, cbs_sct;
	STACK_OF(SCT) *sk = NULL;

	CBS_init(&cbs, *pp, len);

	if (CBS_len(&cbs) > MAX_SCT_LIST_SIZE)
		goto err_invalid;
	if (!CBS_get_u16_length_prefixed(&cbs, &cbs_scts))
		goto err_invalid;
	if (CBS_len(&cbs) != 0)
		goto err_invalid;

	if (scts == NULL || *scts == NULL) {
		if ((sk = sk_SCT_new_null()) == NULL)
			return NULL;
	} else {
		SCT *sct;

		/* Use the given stack, but empty it first. */
		sk = *scts;
		while ((sct = sk_SCT_pop(sk)) != NULL)
			SCT_free(sct);
	}

	while (CBS_len(&cbs_scts) > 0) {
		SCT *sct;

		if (!CBS_get_u16_length_prefixed(&cbs_scts, &cbs_sct))
			goto err_invalid;

		if (!o2i_SCT_internal(&sct, &cbs_sct))
			goto err;
		if (!sk_SCT_push(sk, sct)) {
			SCT_free(sct);
			goto err;
		}
	}

	if (scts != NULL && *scts == NULL)
		*scts = sk;

	*pp = CBS_data(&cbs);

	return sk;

 err_invalid:
	CTerror(CT_R_SCT_LIST_INVALID);
 err:
	if (scts == NULL || *scts == NULL)
		SCT_LIST_free(sk);

	return NULL;
}

int
i2o_SCT_LIST(const STACK_OF(SCT) *a, unsigned char **pp)
{
	int len, sct_len, i, is_pp_new = 0;
	size_t len2;
	unsigned char *p = NULL, *p2;

	if (pp != NULL) {
		if (*pp == NULL) {
			if ((len = i2o_SCT_LIST(a, NULL)) == -1) {
				CTerror(CT_R_SCT_LIST_INVALID);
				return -1;
			}
			if ((*pp = malloc(len)) == NULL) {
				CTerror(ERR_R_MALLOC_FAILURE);
				return -1;
			}
			is_pp_new = 1;
		}
		p = *pp + 2;
	}

	len2 = 2;
	for (i = 0; i < sk_SCT_num(a); i++) {
		if (pp != NULL) {
			p2 = p;
			p += 2;
			if ((sct_len = i2o_SCT(sk_SCT_value(a, i), &p)) == -1)
				goto err;
			s2n(sct_len, p2);
		} else {
			if ((sct_len = i2o_SCT(sk_SCT_value(a, i), NULL)) == -1)
				goto err;
		}
		len2 += 2 + sct_len;
	}

	if (len2 > MAX_SCT_LIST_SIZE)
		goto err;

	if (pp != NULL) {
		p = *pp;
		s2n(len2 - 2, p);
		if (!is_pp_new)
			*pp += len2;
	}
	return len2;

 err:
	if (is_pp_new) {
		free(*pp);
		*pp = NULL;
	}
	return -1;
}

STACK_OF(SCT) *
d2i_SCT_LIST(STACK_OF(SCT) **a, const unsigned char **pp, long len)
{
	ASN1_OCTET_STRING *oct = NULL;
	STACK_OF(SCT) *sk = NULL;
	const unsigned char *p;

	p = *pp;
	if (d2i_ASN1_OCTET_STRING(&oct, &p, len) == NULL)
		return NULL;

	p = oct->data;
	if ((sk = o2i_SCT_LIST(a, &p, oct->length)) != NULL)
		*pp += len;

	ASN1_OCTET_STRING_free(oct);
	return sk;
}

int
i2d_SCT_LIST(const STACK_OF(SCT) *a, unsigned char **out)
{
	ASN1_OCTET_STRING oct;
	int len;

	oct.data = NULL;
	if ((oct.length = i2o_SCT_LIST(a, &oct.data)) == -1)
		return -1;

	len = i2d_ASN1_OCTET_STRING(&oct, out);
	free(oct.data);
	return len;
}

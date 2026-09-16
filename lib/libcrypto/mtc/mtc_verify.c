/*	$OpenBSD$ */
/*
 * Copyright (c) 2026, Bob Beck <beck@obtuse.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Verification of a Merkle Tree Certificate's proof against a trusted CA,
 * per section 7.2 of draft-ietf-plants-merkle-tree-certs-05, and of the
 * certificate in an X509_STORE_CTX.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <openssl/mtc.h>

#include <openssl/asn1.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>

#include "bytestring.h"
#include "mtc_internal.h"
#include "x509_internal.h"
#include "x509_local.h"

/* 1.3.6.1.4.1.44363.47.0, the signature algorithm of an MTC (6.2). */
static const uint8_t mtc_proof_alg_oid[] = {
	0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0xda, 0x4b, 0x2f, 0x00,
};

/* The prefix of a cosigner_name and a log_origin (5.3.1). */
static const char mtc_tai_prefix[] = "oid/1.3.6.1.4.1.";

/* The label of a CosignedMessage (5.3.1), including its trailing NUL. */
static const uint8_t mtc_cosigned_label[12] = "subtree/v1\n";

int
mtc_is_mtc(const X509 *cert)
{
	const ASN1_BIT_STRING *sig;
	const X509_ALGOR *alg;

	X509_get0_signature(&sig, &alg, cert);

	return OBJ_length(alg->algorithm) == sizeof(mtc_proof_alg_oid) &&
	    memcmp(OBJ_get0_data(alg->algorithm), mtc_proof_alg_oid,
	    sizeof(mtc_proof_alg_oid)) == 0;
}

struct mtc_ca *
mtc_ca_for_cert(STACK_OF(OSSL_MTC_CA) *cas, const X509 *cert, int *error)
{
	struct mtc_ca *ca;
	uint8_t *id;
	size_t id_len;

	if (!mtc_ca_id_from_name(X509_get_issuer_name(cert), &id, &id_len)) {
		*error = X509_V_ERR_MTC_NOT_MTC;
		return NULL;
	}
	if ((ca = mtc_ca_stack_lookup(cas, id, id_len)) == NULL)
		*error = X509_V_ERR_MTC_UNTRUSTED_CA;
	free(id);

	return ca;
}

/* Appends the dotted-decimal form of relative-OID content octets. */
static int
cbb_add_reloid_text(CBB *cbb, const uint8_t *id, size_t id_len)
{
	CBS cbs;
	char text[21];
	uint64_t v;
	uint8_t b;
	int len, first = 1;

	CBS_init(&cbs, id, id_len);
	while (CBS_len(&cbs) > 0) {
		v = 0;
		do {
			if (!CBS_get_u8(&cbs, &b))
				return 0;
			if (v > (UINT64_MAX >> 7))
				return 0;
			v = (v << 7) | (b & 0x7f);
		} while ((b & 0x80) != 0);
		len = snprintf(text, sizeof(text), "%s%llu", first ? "" : ".",
		    v);
		if (len < 0 || (size_t)len >= sizeof(text))
			return 0;
		if (!CBB_add_bytes(cbb, (const uint8_t *)text, len))
			return 0;
		first = 0;
	}

	return 1;
}

/*
 * Builds the MerkleTreeCertEntry of type tbs_cert_entry for the
 * TBSCertificate in [tbs, tbs_len) (section 5.2.1), following the
 * single-pass construction of section 7.2: version, issuer, validity and
 * subject as encoded, then the subjectPublicKeyInfo's algorithm and an
 * OCTET STRING holding the hash of the whole subjectPublicKeyInfo, then the
 * rest of the TBSCertificate contents.
 */
int
mtc_cert_entry(const EVP_MD *md, const uint8_t *tbs, size_t tbs_len,
    const CBS *extensions, uint8_t **out, size_t *out_len)
{
	CBB cbb, child;
	CBS cbs, seq, field, spki, spki_alg;
	uint8_t spki_hash[EVP_MAX_MD_SIZE];
	unsigned int spki_hash_len;
	int ret = 0;

	*out = NULL;
	*out_len = 0;

	CBS_init(&cbs, tbs, tbs_len);
	if (!CBS_get_asn1(&cbs, &seq, CBS_ASN1_SEQUENCE))
		return 0;
	if (CBS_len(&cbs) != 0)
		return 0;

	if (!CBB_init(&cbb, 0))
		return 0;

	if (!CBB_add_u16_length_prefixed(&cbb, &child))
		goto err;
	if (!CBB_add_bytes(&child, CBS_data(extensions), CBS_len(extensions)))
		goto err;
	if (!CBB_add_u16(&cbb, 1))
		goto err;

	if (CBS_peek_asn1_tag(&seq, CBS_ASN1_CONTEXT_SPECIFIC |
	    CBS_ASN1_CONSTRUCTED | 0)) {
		if (!CBS_get_asn1_element(&seq, &field,
		    CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0))
			goto err;
		if (!CBB_add_bytes(&cbb, CBS_data(&field), CBS_len(&field)))
			goto err;
	}
	if (!CBS_get_asn1(&seq, &field, CBS_ASN1_INTEGER))
		goto err;
	if (!CBS_get_asn1(&seq, &field, CBS_ASN1_SEQUENCE))
		goto err;
	if (!CBS_get_asn1_element(&seq, &field, CBS_ASN1_SEQUENCE))
		goto err;
	if (!CBB_add_bytes(&cbb, CBS_data(&field), CBS_len(&field)))
		goto err;
	if (!CBS_get_asn1_element(&seq, &field, CBS_ASN1_SEQUENCE))
		goto err;
	if (!CBB_add_bytes(&cbb, CBS_data(&field), CBS_len(&field)))
		goto err;
	if (!CBS_get_asn1_element(&seq, &field, CBS_ASN1_SEQUENCE))
		goto err;
	if (!CBB_add_bytes(&cbb, CBS_data(&field), CBS_len(&field)))
		goto err;

	if (!CBS_get_asn1_element(&seq, &spki, CBS_ASN1_SEQUENCE))
		goto err;
	if (!EVP_Digest(CBS_data(&spki), CBS_len(&spki), spki_hash,
	    &spki_hash_len, md, NULL))
		goto err;
	if (!CBS_get_asn1(&spki, &field, CBS_ASN1_SEQUENCE))
		goto err;
	if (!CBS_get_asn1_element(&field, &spki_alg, CBS_ASN1_SEQUENCE))
		goto err;
	if (!CBB_add_bytes(&cbb, CBS_data(&spki_alg), CBS_len(&spki_alg)))
		goto err;
	if (!CBB_add_asn1(&cbb, &child, CBS_ASN1_OCTETSTRING))
		goto err;
	if (!CBB_add_bytes(&child, spki_hash, spki_hash_len))
		goto err;

	if (!CBB_add_bytes(&cbb, CBS_data(&seq), CBS_len(&seq)))
		goto err;

	if (!CBB_finish(&cbb, out, out_len))
		goto err;

	ret = 1;

 err:
	CBB_cleanup(&cbb);

	return ret;
}

/*
 * Builds the CosignedMessage of section 5.3.1 that cosigner_id signs over
 * the hash of [start, end) in log log_number of the CA with ID ca_id, with
 * a zero timestamp as verification requires (7.2).
 */
int
mtc_cosigned_message(const uint8_t *cosigner_id, size_t cosigner_id_len,
    const uint8_t *ca_id, size_t ca_id_len, uint16_t log_number,
    uint64_t start, uint64_t end, const uint8_t *subtree_hash,
    size_t subtree_hash_len, uint8_t **out, size_t *out_len)
{
	CBB cbb, child;
	char log_suffix[16];
	int len, ret = 0;

	*out = NULL;
	*out_len = 0;

	len = snprintf(log_suffix, sizeof(log_suffix), ".0.%u", log_number);
	if (len < 0 || (size_t)len >= sizeof(log_suffix))
		return 0;

	if (!CBB_init(&cbb, 0))
		return 0;

	if (!CBB_add_bytes(&cbb, mtc_cosigned_label,
	    sizeof(mtc_cosigned_label)))
		goto err;
	if (!CBB_add_u8_length_prefixed(&cbb, &child))
		goto err;
	if (!CBB_add_bytes(&child, (const uint8_t *)mtc_tai_prefix,
	    sizeof(mtc_tai_prefix) - 1))
		goto err;
	if (!cbb_add_reloid_text(&child, cosigner_id, cosigner_id_len))
		goto err;
	if (!CBB_add_u64(&cbb, 0))
		goto err;
	if (!CBB_add_u8_length_prefixed(&cbb, &child))
		goto err;
	if (!CBB_add_bytes(&child, (const uint8_t *)mtc_tai_prefix,
	    sizeof(mtc_tai_prefix) - 1))
		goto err;
	if (!cbb_add_reloid_text(&child, ca_id, ca_id_len))
		goto err;
	if (!CBB_add_bytes(&child, (const uint8_t *)log_suffix, len))
		goto err;
	if (!CBB_add_u64(&cbb, start))
		goto err;
	if (!CBB_add_u64(&cbb, end))
		goto err;
	if (!CBB_add_bytes(&cbb, subtree_hash, subtree_hash_len))
		goto err;

	if (!CBB_finish(&cbb, out, out_len))
		goto err;

	ret = 1;

 err:
	CBB_cleanup(&cbb);

	return ret;
}

/*
 * Checks the proof's cosignatures for a valid one by the CA cosigner, whose
 * ID is the CA's own (5.4).  Other cosigners are ignored (7.2 step 12).
 */
static int
verify_cosignatures(struct mtc_ca *ca, uint16_t log_number,
    const struct mtc_proof *proof, const uint8_t *subtree_hash,
    size_t subtree_hash_len, int *error)
{
	struct mtc_cosignature *sigs = NULL;
	EVP_MD_CTX *md_ctx = NULL;
	uint8_t *msg = NULL;
	const uint8_t *ca_id;
	size_t ca_id_len, count, msg_len, i;
	int ret = 0;

	*error = X509_V_ERR_MTC_BAD_PROOF;

	if (!mtc_proof_get_cosignatures(proof, NULL, 0, &count))
		goto err;
	if ((sigs = reallocarray(NULL, count, sizeof(*sigs))) == NULL)
		goto err;
	if (!mtc_proof_get_cosignatures(proof, sigs, count, &count))
		goto err;

	*error = X509_V_ERR_MTC_NOT_TRUSTED;

	ca_id = mtc_ca_id(ca, &ca_id_len);
	for (i = 0; i < count; i++) {
		if (CBS_len(&sigs[i].cosigner_id) != ca_id_len ||
		    memcmp(CBS_data(&sigs[i].cosigner_id), ca_id,
		    ca_id_len) != 0)
			continue;

		if (!mtc_cosigned_message(ca_id, ca_id_len, ca_id, ca_id_len,
		    log_number, proof->start, proof->end, subtree_hash,
		    subtree_hash_len, &msg, &msg_len))
			goto err;
		if ((md_ctx = EVP_MD_CTX_new()) == NULL)
			goto err;
		if (!EVP_DigestVerifyInit(md_ctx, NULL, NULL, NULL,
		    mtc_ca_cosigner_pkey(ca)))
			goto err;
		if (EVP_DigestVerify(md_ctx, CBS_data(&sigs[i].signature),
		    CBS_len(&sigs[i].signature), msg, msg_len) != 1)
			goto err;

		*error = X509_V_OK;
		ret = 1;
		break;
	}

 err:
	EVP_MD_CTX_free(md_ctx);
	free(msg);
	free(sigs);

	return ret;
}

/* The TBSCertificate as received, from the encoding cached at decoding. */
static int
cert_tbs(X509 *cert, uint8_t **out, size_t *out_len)
{
	int len;

	*out = NULL;
	*out_len = 0;

	if ((len = i2d_X509_CINF(cert->cert_info, out)) < 0)
		return 0;
	*out_len = len;

	return 1;
}

int
mtc_verify(struct mtc_ca *ca, X509 *cert, int *error)
{
	struct mtc_proof proof;
	struct mtc_subtree subtree;
	const ASN1_BIT_STRING *sig;
	const X509_ALGOR *alg;
	uint8_t entry_hash[EVP_MAX_MD_SIZE], subtree_hash[EVP_MAX_MD_SIZE];
	uint8_t *tbs = NULL, *entry = NULL;
	size_t tbs_len, entry_len, hash_len;
	uint64_t serial, index;
	uint16_t log_number;
	int found = 0, ret = 0;

	*error = X509_V_ERR_MTC_BAD_PROOF;

	X509_get0_signature(&sig, &alg, cert);
	if (!mtc_is_mtc(cert) || alg->parameter != NULL)
		goto err;
	if ((sig->flags & 0x07) != 0)
		goto err;
	if (!mtc_proof_parse(ASN1_STRING_get0_data(sig),
	    ASN1_STRING_length(sig), &proof))
		goto err;

	if (!ASN1_INTEGER_get_uint64(&serial, X509_get0_serialNumber(cert)))
		goto err;
	index = serial & (MTC_MAX_TREE_SIZE - 1);
	log_number = serial >> 48;
	if (log_number == 0)
		goto err;

	if (mtc_ca_serial_is_revoked(ca, serial)) {
		*error = X509_V_ERR_MTC_REVOKED;
		goto err;
	}

	if (!cert_tbs(cert, &tbs, &tbs_len))
		goto err;
	if (!mtc_cert_entry(mtc_ca_hash(ca), tbs, tbs_len, &proof.extensions,
	    &entry, &entry_len))
		goto err;
	if (!mtc_hash_leaf(mtc_ca_hash(ca), entry, entry_len, entry_hash))
		goto err;
	hash_len = EVP_MD_size(mtc_ca_hash(ca));

	subtree.start = proof.start;
	subtree.end = proof.end;
	if (!mtc_eval_subtree_inclusion_proof(mtc_ca_hash(ca),
	    CBS_data(&proof.inclusion_proof), CBS_len(&proof.inclusion_proof),
	    index, entry_hash, subtree, subtree_hash)) {
		*error = X509_V_ERR_MTC_INCLUSION_FAILED;
		goto err;
	}

	if (mtc_ca_trusted_subtree_matches(ca, log_number, subtree,
	    subtree_hash, hash_len, &found)) {
		*error = X509_V_OK;
		ret = 1;
	} else if (found)
		*error = X509_V_ERR_MTC_NOT_TRUSTED;
	else
		ret = verify_cosignatures(ca, log_number, &proof, subtree_hash,
		    hash_len, error);

 err:
	free(tbs);
	free(entry);

	return ret;
}

static int
leaf_check_times(X509_STORE_CTX *ctx)
{
	X509 *cert = ctx->cert;
	time_t when, not_before, not_after;

	if (ctx->param->flags & X509_V_FLAG_USE_CHECK_TIME)
		when = ctx->param->check_time;
	else if (ctx->param->flags & X509_V_FLAG_NO_CHECK_TIME)
		return 1;
	else
		when = time(NULL);

	if (!x509_verify_asn1_time_to_time_t(X509_get_notBefore(cert), 0,
	    &not_before)) {
		ctx->error = X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD;
		return 0;
	}
	if (when < not_before) {
		ctx->error = X509_V_ERR_CERT_NOT_YET_VALID;
		return 0;
	}
	if (!x509_verify_asn1_time_to_time_t(X509_get_notAfter(cert), 1,
	    &not_after)) {
		ctx->error = X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD;
		return 0;
	}
	if (when > not_after) {
		ctx->error = X509_V_ERR_CERT_HAS_EXPIRED;
		return 0;
	}

	return 1;
}

static int
leaf_check_hosts(X509_STORE_CTX *ctx)
{
	X509_VERIFY_PARAM *vpm = ctx->param;
	const char *name;
	int i;

	free(vpm->peername);
	vpm->peername = NULL;

	for (i = 0; i < sk_OPENSSL_STRING_num(vpm->hosts); i++) {
		name = sk_OPENSSL_STRING_value(vpm->hosts, i);
		if (X509_check_host(ctx->cert, name, strlen(name),
		    vpm->hostflags, &vpm->peername) > 0)
			return 1;
	}

	return 0;
}

static int
leaf_check_id(X509_STORE_CTX *ctx)
{
	X509_VERIFY_PARAM *vpm = ctx->param;
	X509 *cert = ctx->cert;

	if (vpm->hosts != NULL && !leaf_check_hosts(ctx)) {
		ctx->error = X509_V_ERR_HOSTNAME_MISMATCH;
		return 0;
	}
	if (vpm->email != NULL && X509_check_email(cert, vpm->email,
	    vpm->emaillen, 0) <= 0) {
		ctx->error = X509_V_ERR_EMAIL_MISMATCH;
		return 0;
	}
	if (vpm->ip != NULL && X509_check_ip(cert, vpm->ip, vpm->iplen,
	    0) <= 0) {
		ctx->error = X509_V_ERR_IP_ADDRESS_MISMATCH;
		return 0;
	}

	return 1;
}

int
mtc_leaf_checks(X509_STORE_CTX *ctx)
{
	X509 *cert = ctx->cert;

	if (!x509v3_cache_extensions(cert)) {
		ctx->error = X509_V_ERR_UNSPECIFIED;
		return 0;
	}
	if (!leaf_check_times(ctx))
		return 0;
	if (!leaf_check_id(ctx))
		return 0;
	if (ctx->param->purpose >= X509_PURPOSE_MIN &&
	    X509_check_purpose(cert, ctx->param->purpose, 0) != 1) {
		ctx->error = X509_V_ERR_INVALID_PURPOSE;
		return 0;
	}
	if ((ctx->param->flags & X509_V_FLAG_IGNORE_CRITICAL) == 0 &&
	    (cert->ex_flags & EXFLAG_CRITICAL) != 0) {
		ctx->error = X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION;
		return 0;
	}
	if (cert->altname != NULL && sk_GENERAL_NAME_num(cert->altname) <= 0) {
		ctx->error = X509_V_ERR_INVALID_EXTENSION;
		return 0;
	}
	if (X509_ALGOR_cmp(cert->sig_alg, cert->cert_info->signature) != 0) {
		ctx->error = X509_V_ERR_MTC_BAD_PROOF;
		return 0;
	}

	return 1;
}

int
x509_verify_mtc(X509_STORE_CTX *ctx)
{
	struct mtc_ca *ca;
	STACK_OF(X509) *chain = NULL;
	int ret = 0;

	if (ctx->chain != NULL) {
		ctx->error = X509_V_ERR_INVALID_CALL;
		goto err;
	}
	if (ctx->store == NULL) {
		ctx->error = X509_V_ERR_MTC_UNTRUSTED_CA;
		goto err;
	}

	CRYPTO_w_lock(CRYPTO_LOCK_X509_STORE);
	ca = mtc_ca_for_cert(x509_store_get0_mtc_cas(ctx->store), ctx->cert,
	    &ctx->error);
	CRYPTO_w_unlock(CRYPTO_LOCK_X509_STORE);
	if (ca == NULL)
		goto err;

	if (!mtc_verify(ca, ctx->cert, &ctx->error))
		goto err;
	if (!mtc_leaf_checks(ctx))
		goto err;

	if ((chain = sk_X509_new_null()) == NULL ||
	    !sk_X509_push(chain, ctx->cert)) {
		ctx->error = X509_V_ERR_OUT_OF_MEM;
		goto err;
	}
	X509_up_ref(ctx->cert);
	ctx->chain = chain;
	chain = NULL;
	ctx->error = X509_V_OK;

	ret = 1;

 err:
	sk_X509_free(chain);
	ctx->current_cert = ctx->cert;
	ctx->error_depth = 0;

	return ret;
}

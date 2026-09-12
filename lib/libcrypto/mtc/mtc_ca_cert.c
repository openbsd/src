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
 * Merkle Tree CAs represented as X.509 certificates, per section 5.5 of
 * draft-ietf-plants-merkle-tree-certs-05, using the OIDs it assigns for
 * early implementations.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/mtc.h>

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include "bytestring.h"
#include "mtc_internal.h"

/* 1.3.6.1.4.1.44363.47.1, the subject attribute holding the CA ID (5.1). */
static const uint8_t mtc_ca_id_attr_oid[] = {
	0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0xda, 0x4b, 0x2f, 0x01,
};

/* 1.3.6.1.4.1.44363.47.2, the MTCCertificationAuthority extension (5.5). */
static const uint8_t mtc_ca_ext_oid[] = {
	0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0xda, 0x4b, 0x2f, 0x02,
};

typedef struct {
	X509_ALGOR *logHash;
	X509_ALGOR *sigAlg;
	ASN1_INTEGER *minSerial;
	ASN1_INTEGER *maxSerial;
} MTC_CERTIFICATION_AUTHORITY;

static const ASN1_TEMPLATE MTC_CERTIFICATION_AUTHORITY_seq_tt[] = {
	{
		.flags = 0,
		.tag = 0,
		.offset = offsetof(MTC_CERTIFICATION_AUTHORITY, logHash),
		.field_name = "logHash",
		.item = &X509_ALGOR_it,
	},
	{
		.flags = 0,
		.tag = 0,
		.offset = offsetof(MTC_CERTIFICATION_AUTHORITY, sigAlg),
		.field_name = "sigAlg",
		.item = &X509_ALGOR_it,
	},
	{
		.flags = 0,
		.tag = 0,
		.offset = offsetof(MTC_CERTIFICATION_AUTHORITY, minSerial),
		.field_name = "minSerial",
		.item = &ASN1_INTEGER_it,
	},
	{
		.flags = 0,
		.tag = 0,
		.offset = offsetof(MTC_CERTIFICATION_AUTHORITY, maxSerial),
		.field_name = "maxSerial",
		.item = &ASN1_INTEGER_it,
	},
};

static const ASN1_ITEM MTC_CERTIFICATION_AUTHORITY_it = {
	.itype = ASN1_ITYPE_SEQUENCE,
	.utype = V_ASN1_SEQUENCE,
	.templates = MTC_CERTIFICATION_AUTHORITY_seq_tt,
	.tcount = sizeof(MTC_CERTIFICATION_AUTHORITY_seq_tt) /
	    sizeof(ASN1_TEMPLATE),
	.funcs = NULL,
	.size = sizeof(MTC_CERTIFICATION_AUTHORITY),
	.sname = "MTC_CERTIFICATION_AUTHORITY",
};

static int
obj_is(const ASN1_OBJECT *obj, const uint8_t *oid, size_t oid_len)
{
	return OBJ_length(obj) == oid_len &&
	    memcmp(OBJ_get0_data(obj), oid, oid_len) == 0;
}

static int
cbb_add_base128(CBB *cbb, uint64_t v)
{
	uint8_t bytes[10];
	size_t n = 0;

	do {
		bytes[n++] = v & 0x7f;
		v >>= 7;
	} while (v != 0);

	while (n-- > 0) {
		if (!CBB_add_u8(cbb, bytes[n] | (n > 0 ? 0x80 : 0)))
			return 0;
	}

	return 1;
}

int
mtc_reloid_from_text(const char *text, size_t text_len, uint8_t **out,
    size_t *out_len)
{
	CBB cbb;
	uint64_t v;
	size_t i = 0;
	int ret = 0;

	*out = NULL;
	*out_len = 0;

	if (!CBB_init(&cbb, 0))
		return 0;

	for (;;) {
		if (i == text_len || text[i] < '0' || text[i] > '9')
			goto err;
		v = 0;
		while (i < text_len && text[i] >= '0' && text[i] <= '9') {
			if (v > (UINT64_MAX - (text[i] - '0')) / 10)
				goto err;
			v = v * 10 + (text[i] - '0');
			i++;
		}
		if (!cbb_add_base128(&cbb, v))
			goto err;
		if (i == text_len)
			break;
		if (text[i] != '.')
			goto err;
		i++;
	}

	if (!CBB_finish(&cbb, out, out_len))
		goto err;

	ret = 1;

 err:
	CBB_cleanup(&cbb);

	return ret;
}

int
mtc_ca_id_from_name(const X509_NAME *name, uint8_t **out_id,
    size_t *out_id_len)
{
	const X509_NAME_ENTRY *entry;
	const ASN1_STRING *value;

	if (X509_NAME_entry_count(name) != 1)
		return 0;
	entry = X509_NAME_get_entry(name, 0);
	if (!obj_is(X509_NAME_ENTRY_get_object(entry), mtc_ca_id_attr_oid,
	    sizeof(mtc_ca_id_attr_oid)))
		return 0;
	value = X509_NAME_ENTRY_get_data(entry);
	if (ASN1_STRING_type(value) != V_ASN1_UTF8STRING)
		return 0;

	return mtc_reloid_from_text((const char *)ASN1_STRING_get0_data(value),
	    ASN1_STRING_length(value), out_id, out_id_len);
}

static MTC_CERTIFICATION_AUTHORITY *
ca_params_from_cert(X509 *cert)
{
	MTC_CERTIFICATION_AUTHORITY *params;
	X509_EXTENSION *ext;
	const ASN1_OCTET_STRING *data;
	const unsigned char *p;
	int i, count;

	count = X509_get_ext_count(cert);
	for (i = 0; i < count; i++) {
		ext = X509_get_ext(cert, i);
		if (obj_is(X509_EXTENSION_get_object(ext), mtc_ca_ext_oid,
		    sizeof(mtc_ca_ext_oid)))
			break;
	}
	if (i == count)
		return NULL;
	if (!X509_EXTENSION_get_critical(ext))
		return NULL;

	data = X509_EXTENSION_get_data(ext);
	p = ASN1_STRING_get0_data(data);
	params = (MTC_CERTIFICATION_AUTHORITY *)ASN1_item_d2i(NULL, &p,
	    ASN1_STRING_length(data), &MTC_CERTIFICATION_AUTHORITY_it);
	if (params != NULL && p != ASN1_STRING_get0_data(data) +
	    ASN1_STRING_length(data)) {
		ASN1_item_free((ASN1_VALUE *)params,
		    &MTC_CERTIFICATION_AUTHORITY_it);
		return NULL;
	}

	return params;
}

static struct mtc_ca *
ca_from_cert(X509 *cert)
{
	MTC_CERTIFICATION_AUTHORITY *params = NULL;
	struct mtc_ca *ca = NULL;
	EVP_PKEY *pkey;
	const EVP_MD *md;
	uint8_t *id = NULL;
	size_t id_len = 0;
	uint64_t min_serial, max_serial;

	if (!mtc_ca_id_from_name(X509_get_subject_name(cert), &id, &id_len))
		goto err;
	if ((params = ca_params_from_cert(cert)) == NULL)
		goto err;
	if ((pkey = X509_get0_pubkey(cert)) == NULL)
		goto err;
	if ((md = EVP_get_digestbyobj(params->logHash->algorithm)) == NULL)
		goto err;
	if (OBJ_obj2nid(params->sigAlg->algorithm) == NID_undef)
		goto err;
	if (!ASN1_INTEGER_get_uint64(&min_serial, params->minSerial))
		goto err;
	if (!ASN1_INTEGER_get_uint64(&max_serial, params->maxSerial))
		goto err;

	if ((ca = mtc_ca_new(id, id_len, md, min_serial, pkey)) == NULL)
		goto err;
	if (!mtc_ca_set_max_serial(ca, max_serial)) {
		mtc_ca_free(ca);
		ca = NULL;
	}

 err:
	ASN1_item_free((ASN1_VALUE *)params, &MTC_CERTIFICATION_AUTHORITY_it);
	free(id);

	return ca;
}

int
mtc_ca_parse_certificates(BIO *in, STACK_OF(OSSL_MTC_CA) *cas)
{
	STACK_OF(OSSL_MTC_CA) *added = NULL;
	struct mtc_ca *ca;
	X509 *cert;
	const uint8_t *id;
	size_t id_len;
	int i, ret = 0;

	if ((added = sk_OSSL_MTC_CA_new(mtc_ca_cmp)) == NULL)
		goto err;

	for (;;) {
		if ((cert = PEM_read_bio_X509(in, NULL, NULL, NULL)) == NULL) {
			if (ERR_GET_REASON(ERR_peek_last_error()) !=
			    PEM_R_NO_START_LINE)
				goto err;
			ERR_clear_error();
			break;
		}
		ca = ca_from_cert(cert);
		X509_free(cert);
		if (ca == NULL)
			goto err;
		id = mtc_ca_id(ca, &id_len);
		if (mtc_ca_stack_lookup(cas, id, id_len) != NULL ||
		    !mtc_ca_stack_add(added, ca)) {
			mtc_ca_free(ca);
			goto err;
		}
	}

	for (i = 0; i < sk_OSSL_MTC_CA_num(added); i++) {
		if (!mtc_ca_stack_add(cas, sk_OSSL_MTC_CA_value(added, i)))
			goto err;
	}

	ret = 1;

 err:
	if (!ret) {
		for (i = 0; i < sk_OSSL_MTC_CA_num(added); i++) {
			ca = sk_OSSL_MTC_CA_value(added, i);
			sk_OSSL_MTC_CA_delete_ptr(cas, ca);
			mtc_ca_free(ca);
		}
	}
	sk_OSSL_MTC_CA_free(added);

	return ret;
}

int
OSSL_MTC_CA_parse_certificates(void *libctx, const char *propq, BIO *in,
    STACK_OF(OSSL_MTC_CA) *out_cas)
{
	if (in == NULL || out_cas == NULL)
		return 0;

	return mtc_ca_parse_certificates(in, out_cas);
}
LCRYPTO_ALIAS(OSSL_MTC_CA_parse_certificates);

OSSL_MTC_CA *
OSSL_MTC_CA_find(STACK_OF(OSSL_MTC_CA) *cas, const uint8_t *ca_id,
    size_t ca_id_len, const char *ca_id_str)
{
	struct mtc_ca *ca;
	uint8_t *id;
	size_t id_len;

	if (cas == NULL)
		return NULL;

	if (ca_id_str != NULL) {
		if (!mtc_reloid_from_text(ca_id_str, strlen(ca_id_str), &id,
		    &id_len))
			return NULL;
		ca = mtc_ca_stack_lookup(cas, id, id_len);
		free(id);
		return ca;
	}

	if (ca_id == NULL)
		return NULL;

	return mtc_ca_stack_lookup(cas, ca_id, ca_id_len);
}
LCRYPTO_ALIAS(OSSL_MTC_CA_find);

/* $OpenBSD: t1_lib.c,v 1.167 2020/05/29 17:39:42 jsing Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2007 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <stdio.h>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/objects.h>
#include <openssl/ocsp.h>

#include "ssl_locl.h"

#include "bytestring.h"
#include "ssl_sigalgs.h"
#include "ssl_tlsext.h"

static int tls_decrypt_ticket(SSL *s, CBS *session_id, CBS *ticket,
    SSL_SESSION **psess);

SSL3_ENC_METHOD TLSv1_enc_data = {
	.enc_flags = 0,
};

SSL3_ENC_METHOD TLSv1_1_enc_data = {
	.enc_flags = SSL_ENC_FLAG_EXPLICIT_IV,
};

SSL3_ENC_METHOD TLSv1_2_enc_data = {
	.enc_flags = SSL_ENC_FLAG_EXPLICIT_IV|SSL_ENC_FLAG_SIGALGS|
	    SSL_ENC_FLAG_SHA256_PRF|SSL_ENC_FLAG_TLS1_2_CIPHERS,
};

long
tls1_default_timeout(void)
{
	/* 2 hours, the 24 hours mentioned in the TLSv1 spec
	 * is way too long for http, the cache would over fill */
	return (60 * 60 * 2);
}

int
tls1_new(SSL *s)
{
	if (!ssl3_new(s))
		return (0);
	s->method->internal->ssl_clear(s);
	return (1);
}

void
tls1_free(SSL *s)
{
	if (s == NULL)
		return;

	free(s->internal->tlsext_session_ticket);
	ssl3_free(s);
}

void
tls1_clear(SSL *s)
{
	ssl3_clear(s);
	s->version = s->method->internal->version;
}

static int nid_list[] = {
	NID_sect163k1,		/* sect163k1 (1) */
	NID_sect163r1,		/* sect163r1 (2) */
	NID_sect163r2,		/* sect163r2 (3) */
	NID_sect193r1,		/* sect193r1 (4) */
	NID_sect193r2,		/* sect193r2 (5) */
	NID_sect233k1,		/* sect233k1 (6) */
	NID_sect233r1,		/* sect233r1 (7) */
	NID_sect239k1,		/* sect239k1 (8) */
	NID_sect283k1,		/* sect283k1 (9) */
	NID_sect283r1,		/* sect283r1 (10) */
	NID_sect409k1,		/* sect409k1 (11) */
	NID_sect409r1,		/* sect409r1 (12) */
	NID_sect571k1,		/* sect571k1 (13) */
	NID_sect571r1,		/* sect571r1 (14) */
	NID_secp160k1,		/* secp160k1 (15) */
	NID_secp160r1,		/* secp160r1 (16) */
	NID_secp160r2,		/* secp160r2 (17) */
	NID_secp192k1,		/* secp192k1 (18) */
	NID_X9_62_prime192v1,	/* secp192r1 (19) */
	NID_secp224k1,		/* secp224k1 (20) */
	NID_secp224r1,		/* secp224r1 (21) */
	NID_secp256k1,		/* secp256k1 (22) */
	NID_X9_62_prime256v1,	/* secp256r1 (23) */
	NID_secp384r1,		/* secp384r1 (24) */
	NID_secp521r1,		/* secp521r1 (25) */
	NID_brainpoolP256r1,	/* brainpoolP256r1 (26) */
	NID_brainpoolP384r1,	/* brainpoolP384r1 (27) */
	NID_brainpoolP512r1,	/* brainpoolP512r1 (28) */
	NID_X25519,		/* X25519 (29) */
};

#if 0
static const uint8_t ecformats_list[] = {
	TLSEXT_ECPOINTFORMAT_uncompressed,
	TLSEXT_ECPOINTFORMAT_ansiX962_compressed_prime,
	TLSEXT_ECPOINTFORMAT_ansiX962_compressed_char2
};
#endif

static const uint8_t ecformats_default[] = {
	TLSEXT_ECPOINTFORMAT_uncompressed,
};

#if 0
static const uint16_t eccurves_list[] = {
	29,			/* X25519 (29) */
	14,			/* sect571r1 (14) */
	13,			/* sect571k1 (13) */
	25,			/* secp521r1 (25) */
	28,			/* brainpoolP512r1 (28) */
	11,			/* sect409k1 (11) */
	12,			/* sect409r1 (12) */
	27,			/* brainpoolP384r1 (27) */
	24,			/* secp384r1 (24) */
	9,			/* sect283k1 (9) */
	10,			/* sect283r1 (10) */
	26,			/* brainpoolP256r1 (26) */
	22,			/* secp256k1 (22) */
	23,			/* secp256r1 (23) */
	8,			/* sect239k1 (8) */
	6,			/* sect233k1 (6) */
	7,			/* sect233r1 (7) */
	20,			/* secp224k1 (20) */
	21,			/* secp224r1 (21) */
	4,			/* sect193r1 (4) */
	5,			/* sect193r2 (5) */
	18,			/* secp192k1 (18) */
	19,			/* secp192r1 (19) */
	1,			/* sect163k1 (1) */
	2,			/* sect163r1 (2) */
	3,			/* sect163r2 (3) */
	15,			/* secp160k1 (15) */
	16,			/* secp160r1 (16) */
	17,			/* secp160r2 (17) */
};
#endif

static const uint16_t eccurves_default[] = {
	29,			/* X25519 (29) */
	23,			/* secp256r1 (23) */
	24,			/* secp384r1 (24) */
};

int
tls1_ec_curve_id2nid(const uint16_t curve_id)
{
	/* ECC curves from draft-ietf-tls-ecc-12.txt (Oct. 17, 2005) */
	if ((curve_id < 1) ||
	    ((unsigned int)curve_id > sizeof(nid_list) / sizeof(nid_list[0])))
		return 0;
	return nid_list[curve_id - 1];
}

uint16_t
tls1_ec_nid2curve_id(const int nid)
{
	/* ECC curves from draft-ietf-tls-ecc-12.txt (Oct. 17, 2005) */
	switch (nid) {
	case NID_sect163k1: /* sect163k1 (1) */
		return 1;
	case NID_sect163r1: /* sect163r1 (2) */
		return 2;
	case NID_sect163r2: /* sect163r2 (3) */
		return 3;
	case NID_sect193r1: /* sect193r1 (4) */
		return 4;
	case NID_sect193r2: /* sect193r2 (5) */
		return 5;
	case NID_sect233k1: /* sect233k1 (6) */
		return 6;
	case NID_sect233r1: /* sect233r1 (7) */
		return 7;
	case NID_sect239k1: /* sect239k1 (8) */
		return 8;
	case NID_sect283k1: /* sect283k1 (9) */
		return 9;
	case NID_sect283r1: /* sect283r1 (10) */
		return 10;
	case NID_sect409k1: /* sect409k1 (11) */
		return 11;
	case NID_sect409r1: /* sect409r1 (12) */
		return 12;
	case NID_sect571k1: /* sect571k1 (13) */
		return 13;
	case NID_sect571r1: /* sect571r1 (14) */
		return 14;
	case NID_secp160k1: /* secp160k1 (15) */
		return 15;
	case NID_secp160r1: /* secp160r1 (16) */
		return 16;
	case NID_secp160r2: /* secp160r2 (17) */
		return 17;
	case NID_secp192k1: /* secp192k1 (18) */
		return 18;
	case NID_X9_62_prime192v1: /* secp192r1 (19) */
		return 19;
	case NID_secp224k1: /* secp224k1 (20) */
		return 20;
	case NID_secp224r1: /* secp224r1 (21) */
		return 21;
	case NID_secp256k1: /* secp256k1 (22) */
		return 22;
	case NID_X9_62_prime256v1: /* secp256r1 (23) */
		return 23;
	case NID_secp384r1: /* secp384r1 (24) */
		return 24;
	case NID_secp521r1: /* secp521r1 (25) */
		return 25;
	case NID_brainpoolP256r1: /* brainpoolP256r1 (26) */
		return 26;
	case NID_brainpoolP384r1: /* brainpoolP384r1 (27) */
		return 27;
	case NID_brainpoolP512r1: /* brainpoolP512r1 (28) */
		return 28;
	case NID_X25519:		/* X25519 (29) */
		return 29;
	default:
		return 0;
	}
}

/*
 * Return the appropriate format list. If client_formats is non-zero, return
 * the client/session formats. Otherwise return the custom format list if one
 * exists, or the default formats if a custom list has not been specified.
 */
void
tls1_get_formatlist(SSL *s, int client_formats, const uint8_t **pformats,
    size_t *pformatslen)
{
	if (client_formats != 0) {
		*pformats = SSI(s)->tlsext_ecpointformatlist;
		*pformatslen = SSI(s)->tlsext_ecpointformatlist_length;
		return;
	}

	*pformats = s->internal->tlsext_ecpointformatlist;
	*pformatslen = s->internal->tlsext_ecpointformatlist_length;
	if (*pformats == NULL) {
		*pformats = ecformats_default;
		*pformatslen = sizeof(ecformats_default);
	}
}

/*
 * Return the appropriate group list. If client_groups is non-zero, return
 * the client/session groups. Otherwise return the custom group list if one
 * exists, or the default groups if a custom list has not been specified.
 */
void
tls1_get_group_list(SSL *s, int client_groups, const uint16_t **pgroups,
    size_t *pgroupslen)
{
	if (client_groups != 0) {
		*pgroups = SSI(s)->tlsext_supportedgroups;
		*pgroupslen = SSI(s)->tlsext_supportedgroups_length;
		return;
	}

	*pgroups = s->internal->tlsext_supportedgroups;
	*pgroupslen = s->internal->tlsext_supportedgroups_length;
	if (*pgroups == NULL) {
		*pgroups = eccurves_default;
		*pgroupslen = sizeof(eccurves_default) / 2;
	}
}

int
tls1_set_groups(uint16_t **out_group_ids, size_t *out_group_ids_len,
    const int *groups, size_t ngroups)
{
	uint16_t *group_ids;
	size_t i;

	group_ids = calloc(ngroups, sizeof(uint16_t));
	if (group_ids == NULL)
		return 0;

	for (i = 0; i < ngroups; i++) {
		group_ids[i] = tls1_ec_nid2curve_id(groups[i]);
		if (group_ids[i] == 0) {
			free(group_ids);
			return 0;
		}
	}

	free(*out_group_ids);
	*out_group_ids = group_ids;
	*out_group_ids_len = ngroups;

	return 1;
}

int
tls1_set_group_list(uint16_t **out_group_ids, size_t *out_group_ids_len,
    const char *groups)
{
	uint16_t *new_group_ids, *group_ids = NULL;
	size_t ngroups = 0;
	char *gs, *p, *q;
	int nid;

	if ((gs = strdup(groups)) == NULL)
		return 0;

	q = gs;
	while ((p = strsep(&q, ":")) != NULL) {
		nid = OBJ_sn2nid(p);
		if (nid == NID_undef)
			nid = OBJ_ln2nid(p);
		if (nid == NID_undef)
			nid = EC_curve_nist2nid(p);
		if (nid == NID_undef)
			goto err;

		if ((new_group_ids = reallocarray(group_ids, ngroups + 1,
		    sizeof(uint16_t))) == NULL)
			goto err;
		group_ids = new_group_ids;

		group_ids[ngroups] = tls1_ec_nid2curve_id(nid);
		if (group_ids[ngroups] == 0)
			goto err;

		ngroups++;
	}

	free(gs);
	free(*out_group_ids);
	*out_group_ids = group_ids;
	*out_group_ids_len = ngroups;

	return 1;

 err:
	free(gs);
	free(group_ids);

	return 0;
}

/* Check that a curve is one of our preferences. */
int
tls1_check_curve(SSL *s, const uint16_t curve_id)
{
	const uint16_t *groups;
	size_t groupslen, i;

	tls1_get_group_list(s, 0, &groups, &groupslen);

	for (i = 0; i < groupslen; i++) {
		if (groups[i] == curve_id)
			return (1);
	}
	return (0);
}

int
tls1_get_shared_curve(SSL *s)
{
	size_t preflen, supplen, i, j;
	const uint16_t *pref, *supp;
	unsigned long server_pref;

	/* Cannot do anything on the client side. */
	if (s->server == 0)
		return (NID_undef);

	/* Return first preference shared curve. */
	server_pref = (s->internal->options & SSL_OP_CIPHER_SERVER_PREFERENCE);
	tls1_get_group_list(s, (server_pref == 0), &pref, &preflen);
	tls1_get_group_list(s, (server_pref != 0), &supp, &supplen);

	for (i = 0; i < preflen; i++) {
		for (j = 0; j < supplen; j++) {
			if (pref[i] == supp[j])
				return (tls1_ec_curve_id2nid(pref[i]));
		}
	}
	return (NID_undef);
}

/* For an EC key set TLS ID and required compression based on parameters. */
static int
tls1_set_ec_id(uint16_t *curve_id, uint8_t *comp_id, EC_KEY *ec)
{
	const EC_GROUP *grp;
	const EC_METHOD *meth;
	int is_prime = 0;
	int nid, id;

	if (ec == NULL)
		return (0);

	/* Determine if it is a prime field. */
	if ((grp = EC_KEY_get0_group(ec)) == NULL)
		return (0);
	if ((meth = EC_GROUP_method_of(grp)) == NULL)
		return (0);
	if (EC_METHOD_get_field_type(meth) == NID_X9_62_prime_field)
		is_prime = 1;

	/* Determine curve ID. */
	nid = EC_GROUP_get_curve_name(grp);
	id = tls1_ec_nid2curve_id(nid);

	/* If we have an ID set it, otherwise set arbitrary explicit curve. */
	if (id != 0)
		*curve_id = id;
	else
		*curve_id = is_prime ? 0xff01 : 0xff02;

	/* Specify the compression identifier. */
	if (comp_id != NULL) {
		if (EC_KEY_get0_public_key(ec) == NULL)
			return (0);

		if (EC_KEY_get_conv_form(ec) == POINT_CONVERSION_COMPRESSED) {
			*comp_id = is_prime ?
			    TLSEXT_ECPOINTFORMAT_ansiX962_compressed_prime :
			    TLSEXT_ECPOINTFORMAT_ansiX962_compressed_char2;
		} else {
			*comp_id = TLSEXT_ECPOINTFORMAT_uncompressed;
		}
	}
	return (1);
}

/* Check that an EC key is compatible with extensions. */
static int
tls1_check_ec_key(SSL *s, const uint16_t *curve_id, const uint8_t *comp_id)
{
	size_t groupslen, formatslen, i;
	const uint16_t *groups;
	const uint8_t *formats;

	/*
	 * Check point formats extension if present, otherwise everything
	 * is supported (see RFC4492).
	 */
	tls1_get_formatlist(s, 1, &formats, &formatslen);
	if (comp_id != NULL && formats != NULL) {
		for (i = 0; i < formatslen; i++) {
			if (formats[i] == *comp_id)
				break;
		}
		if (i == formatslen)
			return (0);
	}

	/*
	 * Check curve list if present, otherwise everything is supported.
	 */
	tls1_get_group_list(s, 1, &groups, &groupslen);
	if (curve_id != NULL && groups != NULL) {
		for (i = 0; i < groupslen; i++) {
			if (groups[i] == *curve_id)
				break;
		}
		if (i == groupslen)
			return (0);
	}

	return (1);
}

/* Check EC server key is compatible with client extensions. */
int
tls1_check_ec_server_key(SSL *s)
{
	CERT_PKEY *cpk = s->cert->pkeys + SSL_PKEY_ECC;
	uint16_t curve_id;
	uint8_t comp_id;
	EVP_PKEY *pkey;
	int rv;

	if (cpk->x509 == NULL || cpk->privatekey == NULL)
		return (0);
	if ((pkey = X509_get_pubkey(cpk->x509)) == NULL)
		return (0);
	rv = tls1_set_ec_id(&curve_id, &comp_id, pkey->pkey.ec);
	EVP_PKEY_free(pkey);
	if (rv != 1)
		return (0);

	return tls1_check_ec_key(s, &curve_id, &comp_id);
}

int
ssl_check_clienthello_tlsext_early(SSL *s)
{
	int ret = SSL_TLSEXT_ERR_NOACK;
	int al = SSL_AD_UNRECOGNIZED_NAME;

	/* The handling of the ECPointFormats extension is done elsewhere, namely in
	 * ssl3_choose_cipher in s3_lib.c.
	 */
	/* The handling of the EllipticCurves extension is done elsewhere, namely in
	 * ssl3_choose_cipher in s3_lib.c.
	 */

	if (s->ctx != NULL && s->ctx->internal->tlsext_servername_callback != 0)
		ret = s->ctx->internal->tlsext_servername_callback(s, &al,
		    s->ctx->internal->tlsext_servername_arg);
	else if (s->initial_ctx != NULL && s->initial_ctx->internal->tlsext_servername_callback != 0)
		ret = s->initial_ctx->internal->tlsext_servername_callback(s, &al,
		    s->initial_ctx->internal->tlsext_servername_arg);

	switch (ret) {
	case SSL_TLSEXT_ERR_ALERT_FATAL:
		ssl3_send_alert(s, SSL3_AL_FATAL, al);
		return -1;
	case SSL_TLSEXT_ERR_ALERT_WARNING:
		ssl3_send_alert(s, SSL3_AL_WARNING, al);
		return 1;
	case SSL_TLSEXT_ERR_NOACK:
	default:
		return 1;
	}
}

int
ssl_check_clienthello_tlsext_late(SSL *s)
{
	int ret = SSL_TLSEXT_ERR_OK;
	int al = 0;	/* XXX gcc3 */

	/* If status request then ask callback what to do.
 	 * Note: this must be called after servername callbacks in case
 	 * the certificate has changed, and must be called after the cipher
	 * has been chosen because this may influence which certificate is sent
 	 */
	if ((s->tlsext_status_type != -1) &&
	    s->ctx && s->ctx->internal->tlsext_status_cb) {
		int r;
		CERT_PKEY *certpkey;
		certpkey = ssl_get_server_send_pkey(s);
		/* If no certificate can't return certificate status */
		if (certpkey == NULL) {
			s->internal->tlsext_status_expected = 0;
			return 1;
		}
		/* Set current certificate to one we will use so
		 * SSL_get_certificate et al can pick it up.
		 */
		s->cert->key = certpkey;
		r = s->ctx->internal->tlsext_status_cb(s,
		    s->ctx->internal->tlsext_status_arg);
		switch (r) {
			/* We don't want to send a status request response */
		case SSL_TLSEXT_ERR_NOACK:
			s->internal->tlsext_status_expected = 0;
			break;
			/* status request response should be sent */
		case SSL_TLSEXT_ERR_OK:
			if (s->internal->tlsext_ocsp_resp)
				s->internal->tlsext_status_expected = 1;
			else
				s->internal->tlsext_status_expected = 0;
			break;
			/* something bad happened */
		case SSL_TLSEXT_ERR_ALERT_FATAL:
			ret = SSL_TLSEXT_ERR_ALERT_FATAL;
			al = SSL_AD_INTERNAL_ERROR;
			goto err;
		}
	} else
		s->internal->tlsext_status_expected = 0;

err:
	switch (ret) {
	case SSL_TLSEXT_ERR_ALERT_FATAL:
		ssl3_send_alert(s, SSL3_AL_FATAL, al);
		return -1;
	case SSL_TLSEXT_ERR_ALERT_WARNING:
		ssl3_send_alert(s, SSL3_AL_WARNING, al);
		return 1;
	default:
		return 1;
	}
}

int
ssl_check_serverhello_tlsext(SSL *s)
{
	int ret = SSL_TLSEXT_ERR_NOACK;
	int al = SSL_AD_UNRECOGNIZED_NAME;

	ret = SSL_TLSEXT_ERR_OK;

	if (s->ctx != NULL && s->ctx->internal->tlsext_servername_callback != 0)
		ret = s->ctx->internal->tlsext_servername_callback(s, &al,
		    s->ctx->internal->tlsext_servername_arg);
	else if (s->initial_ctx != NULL && s->initial_ctx->internal->tlsext_servername_callback != 0)
		ret = s->initial_ctx->internal->tlsext_servername_callback(s, &al,
		    s->initial_ctx->internal->tlsext_servername_arg);

	/* If we've requested certificate status and we wont get one
 	 * tell the callback
 	 */
	if ((s->tlsext_status_type != -1) && !(s->internal->tlsext_status_expected) &&
	    s->ctx && s->ctx->internal->tlsext_status_cb) {
		int r;

		free(s->internal->tlsext_ocsp_resp);
		s->internal->tlsext_ocsp_resp = NULL;
		s->internal->tlsext_ocsp_resp_len = 0;

		r = s->ctx->internal->tlsext_status_cb(s,
		    s->ctx->internal->tlsext_status_arg);
		if (r == 0) {
			al = SSL_AD_BAD_CERTIFICATE_STATUS_RESPONSE;
			ret = SSL_TLSEXT_ERR_ALERT_FATAL;
		}
		if (r < 0) {
			al = SSL_AD_INTERNAL_ERROR;
			ret = SSL_TLSEXT_ERR_ALERT_FATAL;
		}
	}

	switch (ret) {
	case SSL_TLSEXT_ERR_ALERT_FATAL:
		ssl3_send_alert(s, SSL3_AL_FATAL, al);
		return -1;
	case SSL_TLSEXT_ERR_ALERT_WARNING:
		ssl3_send_alert(s, SSL3_AL_WARNING, al);
		return 1;
	case SSL_TLSEXT_ERR_NOACK:
	default:
		return 1;
	}
}

/* Since the server cache lookup is done early on in the processing of the
 * ClientHello, and other operations depend on the result, we need to handle
 * any TLS session ticket extension at the same time.
 *
 *   session_id: a CBS containing the session ID.
 *   ext_block: a CBS for the ClientHello extensions block.
 *   ret: (output) on return, if a ticket was decrypted, then this is set to
 *       point to the resulting session.
 *
 * If s->internal->tls_session_secret_cb is set then we are expecting a pre-shared key
 * ciphersuite, in which case we have no use for session tickets and one will
 * never be decrypted, nor will s->internal->tlsext_ticket_expected be set to 1.
 *
 * Returns:
 *   -1: fatal error, either from parsing or decrypting the ticket.
 *    0: no ticket was found (or was ignored, based on settings).
 *    1: a zero length extension was found, indicating that the client supports
 *       session tickets but doesn't currently have one to offer.
 *    2: either s->internal->tls_session_secret_cb was set, or a ticket was offered but
 *       couldn't be decrypted because of a non-fatal error.
 *    3: a ticket was successfully decrypted and *ret was set.
 *
 * Side effects:
 *   Sets s->internal->tlsext_ticket_expected to 1 if the server will have to issue
 *   a new session ticket to the client because the client indicated support
 *   (and s->internal->tls_session_secret_cb is NULL) but the client either doesn't have
 *   a session ticket or we couldn't use the one it gave us, or if
 *   s->ctx->tlsext_ticket_key_cb asked to renew the client's ticket.
 *   Otherwise, s->internal->tlsext_ticket_expected is set to 0.
 */
int
tls1_process_ticket(SSL *s, CBS *session_id, CBS *ext_block, SSL_SESSION **ret)
{
	CBS extensions, ext_data;
	uint16_t ext_type = 0;
	int r;

	s->internal->tlsext_ticket_expected = 0;
	*ret = NULL;

	/*
	 * If tickets disabled behave as if no ticket present to permit stateful
	 * resumption.
	 */
	if (SSL_get_options(s) & SSL_OP_NO_TICKET)
		return 0;

	/*
	 * An empty extensions block is valid, but obviously does not contain
	 * a session ticket.
	 */
	if (CBS_len(ext_block) == 0)
		return 0;

	if (!CBS_get_u16_length_prefixed(ext_block, &extensions))
		return -1;

	while (CBS_len(&extensions) > 0) {
		if (!CBS_get_u16(&extensions, &ext_type) ||
		    !CBS_get_u16_length_prefixed(&extensions, &ext_data))
			return -1;

		if (ext_type == TLSEXT_TYPE_session_ticket)
			break;
	}

	if (ext_type != TLSEXT_TYPE_session_ticket)
		return 0;

	if (CBS_len(&ext_data) == 0) {
		/*
		 * The client will accept a ticket but does not currently
		 * have one.
		 */
		s->internal->tlsext_ticket_expected = 1;
		return 1;
	}

	if (s->internal->tls_session_secret_cb != NULL) {
		/*
		 * Indicate that the ticket could not be decrypted rather than
		 * generating the session from ticket now, trigger abbreviated
		 * handshake based on external mechanism to calculate the master
		 * secret later.
		 */
		return 2;
	}

	r = tls_decrypt_ticket(s, session_id, &ext_data, ret);
	switch (r) {
	case 2: /* ticket couldn't be decrypted */
		s->internal->tlsext_ticket_expected = 1;
		return 2;
	case 3: /* ticket was decrypted */
		return r;
	case 4: /* ticket decrypted but need to renew */
		s->internal->tlsext_ticket_expected = 1;
		return 3;
	default: /* fatal error */
		return -1;
	}
}

/* tls_decrypt_ticket attempts to decrypt a session ticket.
 *
 *   session_id: a CBS containing the session ID.
 *   ticket: a CBS containing the body of the session ticket extension.
 *   psess: (output) on return, if a ticket was decrypted, then this is set to
 *       point to the resulting session.
 *
 * Returns:
 *   -1: fatal error, either from parsing or decrypting the ticket.
 *    2: the ticket couldn't be decrypted.
 *    3: a ticket was successfully decrypted and *psess was set.
 *    4: same as 3, but the ticket needs to be renewed.
 */
static int
tls_decrypt_ticket(SSL *s, CBS *session_id, CBS *ticket, SSL_SESSION **psess)
{
	CBS ticket_name, ticket_iv, ticket_encdata, ticket_hmac;
	SSL_SESSION *sess = NULL;
	unsigned char *sdec = NULL;
	size_t sdec_len = 0;
	size_t session_id_len;
	const unsigned char *p;
	unsigned char hmac[EVP_MAX_MD_SIZE];
	HMAC_CTX *hctx = NULL;
	EVP_CIPHER_CTX *cctx = NULL;
	SSL_CTX *tctx = s->initial_ctx;
	int slen, hlen;
	int renew_ticket = 0;
	int ret = -1;

	*psess = NULL;

	if (!CBS_get_bytes(ticket, &ticket_name, 16))
		goto derr;

	/*
	 * Initialize session ticket encryption and HMAC contexts.
	 */
	if ((cctx = EVP_CIPHER_CTX_new()) == NULL)
		goto err;
	if ((hctx = HMAC_CTX_new()) == NULL)
		goto err;

	if (tctx->internal->tlsext_ticket_key_cb != NULL) {
		int rv;

		/*
		 * The API guarantees EVP_MAX_IV_LENGTH bytes of space for
		 * the iv to tlsext_ticket_key_cb().  Since the total space
		 * required for a session cookie is never less than this,
		 * this check isn't too strict.  The exact check comes later.
		 */
		if (CBS_len(ticket) < EVP_MAX_IV_LENGTH)
			goto derr;

		if ((rv = tctx->internal->tlsext_ticket_key_cb(s,
		    (unsigned char *)CBS_data(&ticket_name),
		    (unsigned char *)CBS_data(ticket), cctx, hctx, 0)) < 0)
			goto err;
		if (rv == 0)
			goto derr;
		if (rv == 2)
			renew_ticket = 1;

		/*
		 * Now that the cipher context is initialised, we can extract
		 * the IV since its length is known.
		 */
		if (!CBS_get_bytes(ticket, &ticket_iv,
		    EVP_CIPHER_CTX_iv_length(cctx)))
			goto derr;
	} else {
		/* Check that the key name matches. */
		if (!CBS_mem_equal(&ticket_name,
		    tctx->internal->tlsext_tick_key_name,
		    sizeof(tctx->internal->tlsext_tick_key_name)))
			goto derr;
		if (!CBS_get_bytes(ticket, &ticket_iv,
		    EVP_CIPHER_iv_length(EVP_aes_128_cbc())))
			goto derr;
		if (!EVP_DecryptInit_ex(cctx, EVP_aes_128_cbc(), NULL,
		    tctx->internal->tlsext_tick_aes_key, CBS_data(&ticket_iv)))
			goto err;
		if (!HMAC_Init_ex(hctx, tctx->internal->tlsext_tick_hmac_key,
		    sizeof(tctx->internal->tlsext_tick_hmac_key), EVP_sha256(),
		    NULL))
			goto err;
	}

	/*
	 * Attempt to process session ticket.
	 */

	if ((hlen = HMAC_size(hctx)) < 0)
		goto err;

	if (hlen > CBS_len(ticket))
		goto derr;
	if (!CBS_get_bytes(ticket, &ticket_encdata, CBS_len(ticket) - hlen))
		goto derr;
	if (!CBS_get_bytes(ticket, &ticket_hmac, hlen))
		goto derr;
	if (CBS_len(ticket) != 0)
		goto err;

	/* Check HMAC of encrypted ticket. */
	if (HMAC_Update(hctx, CBS_data(&ticket_name),
	    CBS_len(&ticket_name)) <= 0)
		goto err;
	if (HMAC_Update(hctx, CBS_data(&ticket_iv),
	    CBS_len(&ticket_iv)) <= 0)
		goto err;
	if (HMAC_Update(hctx, CBS_data(&ticket_encdata),
	    CBS_len(&ticket_encdata)) <= 0)
		goto err;
	if (HMAC_Final(hctx, hmac, &hlen) <= 0)
		goto err;

	if (!CBS_mem_equal(&ticket_hmac, hmac, hlen))
		goto derr;

	/* Attempt to decrypt session data. */
	sdec_len = CBS_len(&ticket_encdata);
	if ((sdec = calloc(1, sdec_len)) == NULL)
		goto err;
	if (EVP_DecryptUpdate(cctx, sdec, &slen, CBS_data(&ticket_encdata),
	    CBS_len(&ticket_encdata)) <= 0)
		goto derr;
	if (EVP_DecryptFinal_ex(cctx, sdec + slen, &hlen) <= 0)
		goto derr;

	slen += hlen;

	/*
	 * For session parse failures, indicate that we need to send a new
	 * ticket.
	 */
	p = sdec;
	if ((sess = d2i_SSL_SESSION(NULL, &p, slen)) == NULL)
		goto derr;

	/*
	 * The session ID, if non-empty, is used by some clients to detect that
	 * the ticket has been accepted. So we copy it to the session structure.
	 * If it is empty set length to zero as required by standard.
	 */
	if (!CBS_write_bytes(session_id, sess->session_id,
	    sizeof(sess->session_id), &session_id_len))
		goto err;
	sess->session_id_length = (unsigned int)session_id_len;

	*psess = sess;
	sess = NULL;

	if (renew_ticket)
		ret = 4;
	else
		ret = 3;

	goto done;

 derr:
	ret = 2;
	goto done;

 err:
	ret = -1;
	goto done;

 done:
	freezero(sdec, sdec_len);
	EVP_CIPHER_CTX_free(cctx);
	HMAC_CTX_free(hctx);
	SSL_SESSION_free(sess);

	if (ret == 2)
		ERR_clear_error();

	return ret;
}

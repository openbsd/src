/* $OpenBSD: evp_local.h,v 1.3 2023/03/01 11:16:06 tb Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2000.
 */
/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
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

#ifndef HEADER_EVP_LOCAL_H
#define HEADER_EVP_LOCAL_H

__BEGIN_HIDDEN_DECLS

/*
 * Don't free md_ctx->pctx in EVP_MD_CTX_cleanup().  Needed for ownership
 * handling in EVP_MD_CTX_set_pkey_ctx().
 */
#define EVP_MD_CTX_FLAG_KEEP_PKEY_CTX   0x0400

typedef int evp_sign_method(int type, const unsigned char *m,
    unsigned int m_length, unsigned char *sigret, unsigned int *siglen,
    void *key);
typedef int evp_verify_method(int type, const unsigned char *m,
    unsigned int m_length, const unsigned char *sigbuf, unsigned int siglen,
    void *key);

struct ecx_key_st {
	int nid;
	int key_len;
	uint8_t *priv_key;
	size_t priv_key_len;
	uint8_t *pub_key;
	size_t pub_key_len;
};

/* Type needs to be a bit field
 * Sub-type needs to be for variations on the method, as in, can it do
 * arbitrary encryption.... */
struct evp_pkey_st {
	int type;
	int save_type;
	int references;
	const EVP_PKEY_ASN1_METHOD *ameth;
	ENGINE *engine;
	union	{
		void *ptr;
#ifndef OPENSSL_NO_RSA
		struct rsa_st *rsa;	/* RSA */
#endif
#ifndef OPENSSL_NO_DSA
		struct dsa_st *dsa;	/* DSA */
#endif
#ifndef OPENSSL_NO_DH
		struct dh_st *dh;	/* DH */
#endif
#ifndef OPENSSL_NO_EC
		struct ec_key_st *ec;	/* ECC */
		struct ecx_key_st *ecx;	/* ECX */
#endif
#ifndef OPENSSL_NO_GOST
		struct gost_key_st *gost; /* GOST */
#endif
	} pkey;
	int save_parameters;
	STACK_OF(X509_ATTRIBUTE) *attributes; /* [ 0 ] */
} /* EVP_PKEY */;

struct env_md_st {
	int type;
	int pkey_type;
	int md_size;
	unsigned long flags;
	int (*init)(EVP_MD_CTX *ctx);
	int (*update)(EVP_MD_CTX *ctx, const void *data, size_t count);
	int (*final)(EVP_MD_CTX *ctx, unsigned char *md);
	int (*copy)(EVP_MD_CTX *to, const EVP_MD_CTX *from);
	int (*cleanup)(EVP_MD_CTX *ctx);

	int block_size;
	int ctx_size; /* how big does the ctx->md_data need to be */
	/* control function */
	int (*md_ctrl)(EVP_MD_CTX *ctx, int cmd, int p1, void *p2);
} /* EVP_MD */;

struct env_md_ctx_st {
	const EVP_MD *digest;
	ENGINE *engine; /* functional reference if 'digest' is ENGINE-provided */
	unsigned long flags;
	void *md_data;
	/* Public key context for sign/verify */
	EVP_PKEY_CTX *pctx;
	/* Update function: usually copied from EVP_MD */
	int (*update)(EVP_MD_CTX *ctx, const void *data, size_t count);
} /* EVP_MD_CTX */;

struct evp_cipher_st {
	int nid;
	int block_size;
	int key_len;		/* Default value for variable length ciphers */
	int iv_len;
	unsigned long flags;	/* Various flags */
	int (*init)(EVP_CIPHER_CTX *ctx, const unsigned char *key,
	    const unsigned char *iv, int enc);	/* init key */
	int (*do_cipher)(EVP_CIPHER_CTX *ctx, unsigned char *out,
	    const unsigned char *in, size_t inl);/* encrypt/decrypt data */
	int (*cleanup)(EVP_CIPHER_CTX *); /* cleanup ctx */
	int ctx_size;		/* how big ctx->cipher_data needs to be */
	int (*set_asn1_parameters)(EVP_CIPHER_CTX *, ASN1_TYPE *); /* Populate a ASN1_TYPE with parameters */
	int (*get_asn1_parameters)(EVP_CIPHER_CTX *, ASN1_TYPE *); /* Get parameters from a ASN1_TYPE */
	int (*ctrl)(EVP_CIPHER_CTX *, int type, int arg, void *ptr); /* Miscellaneous operations */
	void *app_data;		/* Application data */
} /* EVP_CIPHER */;

struct evp_cipher_ctx_st {
	const EVP_CIPHER *cipher;
	ENGINE *engine;	/* functional reference if 'cipher' is ENGINE-provided */
	int encrypt;		/* encrypt or decrypt */
	int buf_len;		/* number we have left */

	unsigned char  oiv[EVP_MAX_IV_LENGTH];	/* original iv */
	unsigned char  iv[EVP_MAX_IV_LENGTH];	/* working iv */
	unsigned char buf[EVP_MAX_BLOCK_LENGTH];/* saved partial block */
	int num;				/* used by cfb/ofb/ctr mode */

	void *app_data;		/* application stuff */
	int key_len;		/* May change for variable length cipher */
	unsigned long flags;	/* Various flags */
	void *cipher_data; /* per EVP data */
	int final_used;
	int block_mask;
	unsigned char final[EVP_MAX_BLOCK_LENGTH];/* possible final block */
} /* EVP_CIPHER_CTX */;

struct evp_Encode_Ctx_st {

	int num;	/* number saved in a partial encode/decode */
	int length;	/* The length is either the output line length
			 * (in input bytes) or the shortest input line
			 * length that is ok.  Once decoding begins,
			 * the length is adjusted up each time a longer
			 * line is decoded */
	unsigned char enc_data[80];	/* data to encode */
	int line_num;	/* number read on current line */
	int expect_nl;
} /* EVP_ENCODE_CTX */;

#define EVP_MAXCHUNK ((size_t)1<<(sizeof(long)*8-2))

struct evp_pkey_ctx_st {
	/* Method associated with this operation */
	const EVP_PKEY_METHOD *pmeth;
	/* Engine that implements this method or NULL if builtin */
	ENGINE *engine;
	/* Key: may be NULL */
	EVP_PKEY *pkey;
	/* Peer key for key agreement, may be NULL */
	EVP_PKEY *peerkey;
	/* Actual operation */
	int operation;
	/* Algorithm specific data */
	void *data;
	/* Application specific data */
	void *app_data;
	/* Keygen callback */
	EVP_PKEY_gen_cb *pkey_gencb;
	/* implementation specific keygen data */
	int *keygen_info;
	int keygen_info_count;
} /* EVP_PKEY_CTX */;

#define EVP_PKEY_FLAG_DYNAMIC	1

struct evp_pkey_method_st {
	int pkey_id;
	int flags;

	int (*init)(EVP_PKEY_CTX *ctx);
	int (*copy)(EVP_PKEY_CTX *dst, EVP_PKEY_CTX *src);
	void (*cleanup)(EVP_PKEY_CTX *ctx);

	int (*paramgen_init)(EVP_PKEY_CTX *ctx);
	int (*paramgen)(EVP_PKEY_CTX *ctx, EVP_PKEY *pkey);

	int (*keygen_init)(EVP_PKEY_CTX *ctx);
	int (*keygen)(EVP_PKEY_CTX *ctx, EVP_PKEY *pkey);

	int (*sign_init)(EVP_PKEY_CTX *ctx);
	int (*sign)(EVP_PKEY_CTX *ctx, unsigned char *sig, size_t *siglen,
	    const unsigned char *tbs, size_t tbslen);

	int (*verify_init)(EVP_PKEY_CTX *ctx);
	int (*verify)(EVP_PKEY_CTX *ctx,
	    const unsigned char *sig, size_t siglen,
	    const unsigned char *tbs, size_t tbslen);

	int (*verify_recover_init)(EVP_PKEY_CTX *ctx);
	int (*verify_recover)(EVP_PKEY_CTX *ctx,
	    unsigned char *rout, size_t *routlen,
	    const unsigned char *sig, size_t siglen);

	int (*signctx_init)(EVP_PKEY_CTX *ctx, EVP_MD_CTX *mctx);
	int (*signctx)(EVP_PKEY_CTX *ctx, unsigned char *sig, size_t *siglen,
	    EVP_MD_CTX *mctx);

	int (*verifyctx_init)(EVP_PKEY_CTX *ctx, EVP_MD_CTX *mctx);
	int (*verifyctx)(EVP_PKEY_CTX *ctx, const unsigned char *sig,
	    int siglen, EVP_MD_CTX *mctx);

	int (*encrypt_init)(EVP_PKEY_CTX *ctx);
	int (*encrypt)(EVP_PKEY_CTX *ctx, unsigned char *out, size_t *outlen,
	    const unsigned char *in, size_t inlen);

	int (*decrypt_init)(EVP_PKEY_CTX *ctx);
	int (*decrypt)(EVP_PKEY_CTX *ctx, unsigned char *out, size_t *outlen,
	    const unsigned char *in, size_t inlen);

	int (*derive_init)(EVP_PKEY_CTX *ctx);
	int (*derive)(EVP_PKEY_CTX *ctx, unsigned char *key, size_t *keylen);

	int (*ctrl)(EVP_PKEY_CTX *ctx, int type, int p1, void *p2);
	int (*ctrl_str)(EVP_PKEY_CTX *ctx, const char *type, const char *value);

	int (*digestsign)(EVP_MD_CTX *ctx, unsigned char *sig, size_t *siglen,
	    const unsigned char *tbs, size_t tbslen);
	int (*digestverify) (EVP_MD_CTX *ctx, const unsigned char *sig,
	    size_t siglen, const unsigned char *tbs, size_t tbslen);

	int (*check)(EVP_PKEY *pkey);
	int (*public_check)(EVP_PKEY *pkey);
	int (*param_check)(EVP_PKEY *pkey);
} /* EVP_PKEY_METHOD */;

void evp_pkey_set_cb_translate(BN_GENCB *cb, EVP_PKEY_CTX *ctx);

int PKCS5_v2_PBKDF2_keyivgen(EVP_CIPHER_CTX *ctx, const char *pass, int passlen,
    ASN1_TYPE *param, const EVP_CIPHER *c, const EVP_MD *md, int en_de);

/* EVP_AEAD represents a specific AEAD algorithm. */
struct evp_aead_st {
	unsigned char key_len;
	unsigned char nonce_len;
	unsigned char overhead;
	unsigned char max_tag_len;

	int (*init)(struct evp_aead_ctx_st*, const unsigned char *key,
	    size_t key_len, size_t tag_len);
	void (*cleanup)(struct evp_aead_ctx_st*);

	int (*seal)(const struct evp_aead_ctx_st *ctx, unsigned char *out,
	    size_t *out_len, size_t max_out_len, const unsigned char *nonce,
	    size_t nonce_len, const unsigned char *in, size_t in_len,
	    const unsigned char *ad, size_t ad_len);

	int (*open)(const struct evp_aead_ctx_st *ctx, unsigned char *out,
	    size_t *out_len, size_t max_out_len, const unsigned char *nonce,
	    size_t nonce_len, const unsigned char *in, size_t in_len,
	    const unsigned char *ad, size_t ad_len);
};

/* An EVP_AEAD_CTX represents an AEAD algorithm configured with a specific key
 * and message-independent IV. */
struct evp_aead_ctx_st {
	const EVP_AEAD *aead;
	/* aead_state is an opaque pointer to the AEAD specific state. */
	void *aead_state;
};

int EVP_PKEY_CTX_str2ctrl(EVP_PKEY_CTX *ctx, int cmd, const char *str);
int EVP_PKEY_CTX_hex2ctrl(EVP_PKEY_CTX *ctx, int cmd, const char *hex);
int EVP_PKEY_CTX_md(EVP_PKEY_CTX *ctx, int optype, int cmd, const char *md_name);

__END_HIDDEN_DECLS

#endif /* !HEADER_EVP_LOCAL_H */

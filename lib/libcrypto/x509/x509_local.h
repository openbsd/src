/*	$OpenBSD: x509_local.h,v 1.8 2023/05/08 14:51:00 tb Exp $ */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2013.
 */
/* ====================================================================
 * Copyright (c) 2013 The OpenSSL Project.  All rights reserved.
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

#ifndef HEADER_X509_LOCAL_H
#define HEADER_X509_LOCAL_H

__BEGIN_HIDDEN_DECLS

#define TS_HASH_EVP		EVP_sha1()
#define TS_HASH_LEN		SHA_DIGEST_LENGTH

#define X509_CERT_HASH_EVP	EVP_sha512()
#define X509_CERT_HASH_LEN	SHA512_DIGEST_LENGTH
#define X509_CRL_HASH_EVP	EVP_sha512()
#define X509_CRL_HASH_LEN	SHA512_DIGEST_LENGTH

struct X509_pubkey_st {
	X509_ALGOR *algor;
	ASN1_BIT_STRING *public_key;
	EVP_PKEY *pkey;
};

struct X509_sig_st {
	X509_ALGOR *algor;
	ASN1_OCTET_STRING *digest;
} /* X509_SIG */;

struct X509_name_entry_st {
	ASN1_OBJECT *object;
	ASN1_STRING *value;
	int set;
	int size;	/* temp variable */
} /* X509_NAME_ENTRY */;

/* we always keep X509_NAMEs in 2 forms. */
struct X509_name_st {
	STACK_OF(X509_NAME_ENTRY) *entries;
	int modified;	/* true if 'bytes' needs to be built */
#ifndef OPENSSL_NO_BUFFER
	BUF_MEM *bytes;
#else
	char *bytes;
#endif
/*	unsigned long hash; Keep the hash around for lookups */
	unsigned char *canon_enc;
	int canon_enclen;
} /* X509_NAME */;

struct X509_extension_st {
	ASN1_OBJECT *object;
	ASN1_BOOLEAN critical;
	ASN1_OCTET_STRING *value;
} /* X509_EXTENSION */;

struct x509_attributes_st {
	ASN1_OBJECT *object;
	STACK_OF(ASN1_TYPE) *set;
} /* X509_ATTRIBUTE */;

struct X509_req_info_st {
	ASN1_ENCODING enc;
	ASN1_INTEGER *version;
	X509_NAME *subject;
	X509_PUBKEY *pubkey;
	/*  d=2 hl=2 l=  0 cons: cont: 00 */
	STACK_OF(X509_ATTRIBUTE) *attributes; /* [ 0 ] */
} /* X509_REQ_INFO */;

struct X509_req_st {
	X509_REQ_INFO *req_info;
	X509_ALGOR *sig_alg;
	ASN1_BIT_STRING *signature;
	int references;
} /* X509_REQ */;

/*
 * This stuff is certificate "auxiliary info" it contains details which are
 * useful in certificate stores and databases. When used this is tagged onto
 * the end of the certificate itself.
 */
struct x509_cert_aux_st {
	STACK_OF(ASN1_OBJECT) *trust;		/* trusted uses */
	STACK_OF(ASN1_OBJECT) *reject;		/* rejected uses */
	ASN1_UTF8STRING *alias;			/* "friendly name" */
	ASN1_OCTET_STRING *keyid;		/* key id of private key */
	STACK_OF(X509_ALGOR) *other;		/* other unspecified info */
} /* X509_CERT_AUX */;

struct x509_cinf_st {
	ASN1_INTEGER *version;		/* [ 0 ] default of v1 */
	ASN1_INTEGER *serialNumber;
	X509_ALGOR *signature;
	X509_NAME *issuer;
	X509_VAL *validity;
	X509_NAME *subject;
	X509_PUBKEY *key;
	ASN1_BIT_STRING *issuerUID;		/* [ 1 ] optional in v2 */
	ASN1_BIT_STRING *subjectUID;		/* [ 2 ] optional in v2 */
	STACK_OF(X509_EXTENSION) *extensions;	/* [ 3 ] optional in v3 */
	ASN1_ENCODING enc;
} /* X509_CINF */;

struct x509_st {
	X509_CINF *cert_info;
	X509_ALGOR *sig_alg;
	ASN1_BIT_STRING *signature;
	int valid;
	int references;
	char *name;
	CRYPTO_EX_DATA ex_data;
	/* These contain copies of various extension values */
	long ex_pathlen;
	unsigned long ex_flags;
	unsigned long ex_kusage;
	unsigned long ex_xkusage;
	unsigned long ex_nscert;
	ASN1_OCTET_STRING *skid;
	AUTHORITY_KEYID *akid;
	STACK_OF(DIST_POINT) *crldp;
	STACK_OF(GENERAL_NAME) *altname;
	NAME_CONSTRAINTS *nc;
#ifndef OPENSSL_NO_RFC3779
	STACK_OF(IPAddressFamily) *rfc3779_addr;
	struct ASIdentifiers_st *rfc3779_asid;
#endif
	unsigned char hash[X509_CERT_HASH_LEN];
	time_t not_before;
	time_t not_after;
	X509_CERT_AUX *aux;
} /* X509 */;

struct x509_revoked_st {
	ASN1_INTEGER *serialNumber;
	ASN1_TIME *revocationDate;
	STACK_OF(X509_EXTENSION) /* optional */ *extensions;
	/* Set up if indirect CRL */
	STACK_OF(GENERAL_NAME) *issuer;
	/* Revocation reason */
	int reason;
	int sequence; /* load sequence */
};

struct X509_crl_info_st {
	ASN1_INTEGER *version;
	X509_ALGOR *sig_alg;
	X509_NAME *issuer;
	ASN1_TIME *lastUpdate;
	ASN1_TIME *nextUpdate;
	STACK_OF(X509_REVOKED) *revoked;
	STACK_OF(X509_EXTENSION) /* [0] */ *extensions;
	ASN1_ENCODING enc;
} /* X509_CRL_INFO */;

struct X509_crl_st {
	/* actual signature */
	X509_CRL_INFO *crl;
	X509_ALGOR *sig_alg;
	ASN1_BIT_STRING *signature;
	int references;
	int flags;
	/* Copies of various extensions */
	AUTHORITY_KEYID *akid;
	ISSUING_DIST_POINT *idp;
	/* Convenient breakdown of IDP */
	int idp_flags;
	int idp_reasons;
	/* CRL and base CRL numbers for delta processing */
	ASN1_INTEGER *crl_number;
	ASN1_INTEGER *base_crl_number;
	unsigned char hash[X509_CRL_HASH_LEN];
	STACK_OF(GENERAL_NAMES) *issuers;
	const X509_CRL_METHOD *meth;
	void *meth_data;
} /* X509_CRL */;

struct pkcs8_priv_key_info_st {
        ASN1_INTEGER *version;
        X509_ALGOR *pkeyalg;
        ASN1_OCTET_STRING *pkey;
        STACK_OF(X509_ATTRIBUTE) *attributes;
};

struct x509_object_st {
	/* one of the above types */
	int type;
	union {
		X509 *x509;
		X509_CRL *crl;
	} data;
} /* X509_OBJECT */;

struct x509_lookup_method_st {
	const char *name;
	int (*new_item)(X509_LOOKUP *ctx);
	void (*free)(X509_LOOKUP *ctx);
	int (*init)(X509_LOOKUP *ctx);
	int (*shutdown)(X509_LOOKUP *ctx);
	int (*ctrl)(X509_LOOKUP *ctx, int cmd, const char *argc, long argl,
	    char **ret);
	int (*get_by_subject)(X509_LOOKUP *ctx, int type, X509_NAME *name,
	    X509_OBJECT *ret);
	int (*get_by_issuer_serial)(X509_LOOKUP *ctx, int type, X509_NAME *name,
	    ASN1_INTEGER *serial,X509_OBJECT *ret);
	int (*get_by_fingerprint)(X509_LOOKUP *ctx, int type,
	    const unsigned char *bytes, int len, X509_OBJECT *ret);
	int (*get_by_alias)(X509_LOOKUP *ctx, int type, const char *str,
	    int len, X509_OBJECT *ret);
} /* X509_LOOKUP_METHOD */;

struct X509_VERIFY_PARAM_st {
	char *name;
	time_t check_time;	/* Time to use */
	unsigned long inh_flags; /* Inheritance flags */
	unsigned long flags;	/* Various verify flags */
	int purpose;		/* purpose to check untrusted certificates */
	int trust;		/* trust setting to check */
	int depth;		/* Verify depth */
	int security_level;	/* 'Security level', see SP800-57. */
	STACK_OF(ASN1_OBJECT) *policies;	/* Permissible policies */
	X509_VERIFY_PARAM_ID *id;	/* opaque ID data */
} /* X509_VERIFY_PARAM */;

/*
 * This is used to hold everything.  It is used for all certificate
 * validation.  Once we have a certificate chain, the 'verify'
 * function is then called to actually check the cert chain.
 */
struct x509_store_st {
	/* The following is a cache of trusted certs */
	STACK_OF(X509_OBJECT) *objs;	/* Cache of all objects */

	/* These are external lookup methods */
	STACK_OF(X509_LOOKUP) *get_cert_methods;

	X509_VERIFY_PARAM *param;

	/* Callbacks for various operations */
	int (*verify)(X509_STORE_CTX *ctx);	/* called to verify a certificate */
	int (*verify_cb)(int ok,X509_STORE_CTX *ctx);	/* error callback */
	int (*get_issuer)(X509 **issuer, X509_STORE_CTX *ctx, X509 *x);	/* get issuers cert from ctx */
	int (*check_issued)(X509_STORE_CTX *ctx, X509 *x, X509 *issuer); /* check issued */
	int (*check_revocation)(X509_STORE_CTX *ctx); /* Check revocation status of chain */
	int (*get_crl)(X509_STORE_CTX *ctx, X509_CRL **crl, X509 *x); /* retrieve CRL */
	int (*check_crl)(X509_STORE_CTX *ctx, X509_CRL *crl); /* Check CRL validity */
	int (*cert_crl)(X509_STORE_CTX *ctx, X509_CRL *crl, X509 *x); /* Check certificate against CRL */
	STACK_OF(X509) * (*lookup_certs)(X509_STORE_CTX *ctx, X509_NAME *nm);
	STACK_OF(X509_CRL) * (*lookup_crls)(X509_STORE_CTX *ctx, X509_NAME *nm);
	int (*cleanup)(X509_STORE_CTX *ctx);

	CRYPTO_EX_DATA ex_data;
	int references;
} /* X509_STORE */;

/* This is the functions plus an instance of the local variables. */
struct x509_lookup_st {
	int init;			/* have we been started */
	X509_LOOKUP_METHOD *method;	/* the functions */
	char *method_data;		/* method data */

	X509_STORE *store_ctx;	/* who owns us */
} /* X509_LOOKUP */;

/*
 * This is used when verifying cert chains.  Since the gathering of the cert
 * chain can take some time (and has to be 'retried'), this needs to be kept
 * and passed around.
 */
struct x509_store_ctx_st {
	X509_STORE *store;
	int current_method;	/* used when looking up certs */

	/* The following are set by the caller */
	X509 *cert;		/* The cert to check */
	STACK_OF(X509) *untrusted;	/* chain of X509s - untrusted - passed in */
	STACK_OF(X509) *trusted;	/* trusted stack for use with get_issuer() */
	STACK_OF(X509_CRL) *crls;	/* set of CRLs passed in */

	X509_VERIFY_PARAM *param;

	/* Callbacks for various operations */
	int (*verify)(X509_STORE_CTX *ctx);	/* called to verify a certificate */
	int (*verify_cb)(int ok,X509_STORE_CTX *ctx);		/* error callback */
	int (*get_issuer)(X509 **issuer, X509_STORE_CTX *ctx, X509 *x);	/* get issuers cert from ctx */
	int (*check_issued)(X509_STORE_CTX *ctx, X509 *x, X509 *issuer); /* check issued */
	int (*check_revocation)(X509_STORE_CTX *ctx); /* Check revocation status of chain */
	int (*get_crl)(X509_STORE_CTX *ctx, X509_CRL **crl, X509 *x); /* retrieve CRL */
	int (*check_crl)(X509_STORE_CTX *ctx, X509_CRL *crl); /* Check CRL validity */
	int (*cert_crl)(X509_STORE_CTX *ctx, X509_CRL *crl, X509 *x); /* Check certificate against CRL */
	int (*check_policy)(X509_STORE_CTX *ctx);
	STACK_OF(X509) * (*lookup_certs)(X509_STORE_CTX *ctx, X509_NAME *nm);
	STACK_OF(X509_CRL) * (*lookup_crls)(X509_STORE_CTX *ctx, X509_NAME *nm);
	int (*cleanup)(X509_STORE_CTX *ctx);

	/* The following is built up */
	int valid;		/* if 0, rebuild chain */
	int num_untrusted;	/* number of untrusted certs in chain */
	STACK_OF(X509) *chain;		/* chain of X509s - built up and trusted */

	int explicit_policy;	/* Require explicit policy value */

	/* When something goes wrong, this is why */
	int error_depth;
	int error;
	X509 *current_cert;
	X509 *current_issuer;	/* cert currently being tested as valid issuer */
	X509_CRL *current_crl;	/* current CRL */

	int current_crl_score;  /* score of current CRL */
	unsigned int current_reasons;  /* Reason mask */

	X509_STORE_CTX *parent; /* For CRL path validation: parent context */

	CRYPTO_EX_DATA ex_data;
} /* X509_STORE_CTX */;

struct X509_VERIFY_PARAM_ID_st {
	STACK_OF(OPENSSL_STRING) *hosts; /* Set of acceptable names */
	unsigned int hostflags;     /* Flags to control matching features */
	char *peername;             /* Matching hostname in peer certificate */
	char *email;                /* If not NULL email address to match */
	size_t emaillen;
	unsigned char *ip;          /* If not NULL IP address to match */
	size_t iplen;               /* Length of IP address */
	int poisoned;
};

int x509_check_cert_time(X509_STORE_CTX *ctx, X509 *x, int quiet);

int name_cmp(const char *name, const char *cmp);

int X509_policy_check(const STACK_OF(X509) *certs,
    const STACK_OF(ASN1_OBJECT) *user_policies, unsigned long flags,
    X509 **out_current_cert);

__END_HIDDEN_DECLS

#endif /* !HEADER_X509_LOCAL_H */

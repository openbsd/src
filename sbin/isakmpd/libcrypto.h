/*	$OpenBSD: libcrypto.h,v 1.5 2000/02/07 01:32:54 niklas Exp $	*/
/*	$EOM: libcrypto.h,v 1.11 2000/02/07 01:30:36 angelos Exp $	*/

/*
 * Copyright (c) 1999 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 1999, 2000 Angelos D. Keromytis.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ericsson Radio Systems.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#ifndef _LIBCRYPTO_H_
#define _LIBCRYPTO_H_

#include <stdio.h>
/* XXX I want #include <ssl/cryptall.h> but we appear to not install meth.h  */
#include <ssl/ssl.h>
#include <ssl/bio.h>
#include <ssl/pem.h>
#include <ssl/x509_vfy.h>
#include <ssl/x509.h>

extern void *libcrypto;

#if defined (USE_LIBCRYPTO)
#if defined (HAVE_DLOPEN)
#define LC(sym, args) (libcrypto ? lc_ ## sym args : sym args)
#else
#define LC(sym, args) sym args
#endif
#elif defined (HAVE_DLOPEN)
#define LC(sym, args) lc_ ## sym args
#else
#define LC(sym, args) !!libcrypto called but no USE_LIBCRYPTO nor HAVE_DLOPEN!!
#endif

#ifdef HAVE_DLOPEN

/* 
 * These prototypes matches SSLeay version 0.9.0b or OpenSSL 0.9.4, if you
 * try to load a different version than that, you are on your own.
 */
extern char *(*lc_ASN1_d2i_bio) (char *(*) (), char *(*) (), BIO *bp,
				 unsigned char **);
extern char *(*lc_ASN1_dup) (int (*) (), char *(*) (), char *);
extern long (*lc_BIO_ctrl) (BIO *bp, int, long, char *);
extern int (*lc_BIO_free) (BIO *a);
extern BIO *(*lc_BIO_new) (BIO_METHOD *type);
extern int (*lc_BIO_write) (BIO *, char *, int);
extern BIO_METHOD *(*lc_BIO_s_file) (void);
extern BIO_METHOD *(*lc_BIO_s_mem) (void);
extern int (*lc_BN_print_fp) (FILE *, BIGNUM *);
extern char *(*lc_PEM_ASN1_read_bio) (char *(*) (), char *, BIO *, char **,
				      int (*) ());
extern void (*lc_RSA_free) (RSA *);
extern RSA *(*lc_RSA_generate_key) (int, unsigned long,
				    void (*) (int, int, char *), char *);
extern int (*lc_RSA_private_encrypt) (int, unsigned char *, unsigned char *,
				       RSA *, int);
extern int (*lc_RSA_public_decrypt) (int, unsigned char *, unsigned char *,
				     RSA *, int);
extern int (*lc_RSA_size) (RSA *);
extern void (*lc_SSLeay_add_all_algorithms) (void);
extern int (*lc_X509_NAME_cmp) (X509_NAME *, X509_NAME *);
extern void (*lc_X509_OBJECT_free_contents) (X509_OBJECT *);
extern void (*lc_X509_STORE_CTX_cleanup) (X509_STORE_CTX *);
#if SSLEAY_VERSION_NUMBER >= 0x00904100L
extern void (*lc_X509_STORE_CTX_init) (X509_STORE_CTX *, X509_STORE *, X509 *,
				       STACK_OF (X509) *);
#else
extern void (*lc_X509_STORE_CTX_init) (X509_STORE_CTX *, X509_STORE *, X509 *,
				       STACK *);
#endif
extern int (*lc_X509_STORE_add_cert) (X509_STORE *, X509 *);
extern void (*lc_X509_STORE_free) (X509_STORE *);
extern X509_STORE *(*lc_X509_STORE_new) (void);
extern X509 *(*lc_X509_dup) (X509 *);
#if SSLEAY_VERSION_NUMBER >= 0x00904100L
extern X509 *(*lc_X509_find_by_subject) (STACK_OF (X509) *, X509_NAME *);
#else
extern X509 *(*lc_X509_find_by_subject) (STACK *, X509_NAME *);
#endif
extern int (*lc_X509_STORE_get_by_subject) (X509_STORE_CTX *, int,
					    X509_NAME *, X509_OBJECT *);
extern void (*lc_X509_free) (X509 *);
extern X509_EXTENSION *(*lc_X509_get_ext) (X509 *, int);
extern int (*lc_X509_get_ext_by_NID) (X509 *, int, int);
extern X509_NAME *(*lc_X509_get_issuer_name) (X509 *);
extern EVP_PKEY *(*lc_X509_get_pubkey) (X509 *);
extern X509_NAME *(*lc_X509_get_subject_name) (X509 *);
extern X509 *(*lc_X509_new) (void);
extern int (*lc_X509_verify) (X509 *, EVP_PKEY *);
extern char *(*lc_X509_NAME_oneline) (X509_NAME *, char *, int);
extern int (*lc_X509_verify_cert) (X509_STORE_CTX *);
extern RSA *(*lc_d2i_RSAPrivateKey) (RSA **, unsigned char **, long);
extern RSA *(*lc_d2i_RSAPublicKey) (RSA **, unsigned char **, long);
extern X509 *(*lc_d2i_X509) (X509 **, unsigned char **, long);
extern int (*lc_i2d_RSAPublicKey) (RSA *, unsigned char **);
extern int (*lc_i2d_RSAPrivateKey) (RSA *, unsigned char **);
extern int (*lc_i2d_X509) (X509 *, unsigned char **);
#if SSLEAY_VERSION_NUMBER >= 0x00904100L
extern void (*lc_sk_X509_free) (STACK_OF (X509) *);
extern STACK_OF (X509) *(*lc_sk_X509_new_null) (void);
#else
extern void (*lc_sk_free) (STACK *);
extern STACK *(*lc_sk_new) (int (*) ());
#endif

#define lc_BIO_read_filename(b, name) \
  lc_BIO_ctrl (b, BIO_C_SET_FILENAME, BIO_CLOSE | BIO_FP_READ, name)

#if SSLEAY_VERSION_NUMBER >= 0x00904100L
#define	lc_PEM_read_bio_RSAPrivateKey(bp, x, cb, u) \
  (RSA *)lc_PEM_ASN1_read_bio ((char *(*) ())lc_d2i_RSAPrivateKey, \
			       PEM_STRING_RSA, bp, (char **)x, cb)
#define	lc_PEM_read_bio_X509(bp, x, cb, u) \
  (X509 *)lc_PEM_ASN1_read_bio ((char *(*) ())lc_d2i_X509, PEM_STRING_X509, \
				bp, (char **)x, cb)
#else
#define	lc_PEM_read_bio_RSAPrivateKey(bp, x, cb) \
  (RSA *)lc_PEM_ASN1_read_bio ((char *(*) ())lc_d2i_RSAPrivateKey, \
			       PEM_STRING_RSA, bp, (char **)x, cb)
#define	lc_PEM_read_bio_X509(bp, x, cb) \
  (X509 *)lc_PEM_ASN1_read_bio ((char *(*) ())lc_d2i_X509, PEM_STRING_X509, \
				bp, (char **)x, cb)
#endif

#define lc_RSAPublicKey_dup(rsa) \
  (RSA *)lc_ASN1_dup ((int (*) ())lc_i2d_RSAPublicKey, \
		      (char *(*) ())lc_d2i_RSAPublicKey, (char *)rsa)

#define lc_X509_name_cmp(a, b) lc_X509_NAME_cmp ((a), (b))

#define lc_d2i_X509_bio(bp, x509) \
  (X509 *)lc_ASN1_d2i_bio ((char *(*) ())lc_X509_new, \
			   (char *(*) ())lc_d2i_X509, (bp), \
			   (unsigned char **)(x509))

#if SSLEAY_VERSION_NUMBER < 0x00904100L
#define lc_sk_new_null() lc_sk_new (NULL)
#endif

#endif

extern void libcrypto_init (void);

#endif /* _LIBCRYPTO_H_ */

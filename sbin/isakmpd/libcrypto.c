/*	$OpenBSD: libcrypto.c,v 1.5 2000/02/07 01:32:54 niklas Exp $	*/
/*	$EOM: libcrypto.c,v 1.11 2000/02/07 01:30:36 angelos Exp $	*/

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

#include "sysdep.h"

#include "dyn.h"
#include "libcrypto.h"

void *libcrypto = 0;

#ifdef HAVE_DLOPEN

/*
 * These prototypes matches SSLeay version 0.9.0b or OpenSSL 0.9.4, if
 * you try to load a different version than that, you are on your own.
 */
char *(*lc_ASN1_d2i_bio) (char *(*) (), char *(*) (), BIO *bp,
			  unsigned char **);
char *(*lc_ASN1_dup) (int (*) (), char *(*) (), char *);
long (*lc_BIO_ctrl) (BIO *bp, int, long, char *);
int (*lc_BIO_free) (BIO *a);
BIO *(*lc_BIO_new) (BIO_METHOD *type);
int (*lc_BIO_write) (BIO *, char *, int);
BIO_METHOD *(*lc_BIO_s_file) (void);
BIO_METHOD *(*lc_BIO_s_mem) (void);
int (*lc_BN_print_fp) (FILE *, BIGNUM *);
char *(*lc_PEM_ASN1_read_bio) (char *(*) (), char *, BIO *, char **,
			       int (*) ());
void (*lc_RSA_free) (RSA *);
RSA *(*lc_RSA_generate_key) (int, unsigned long, void (*) (int, int, char *),
			     char *);
int (*lc_RSA_private_encrypt) (int, unsigned char *, unsigned char *, RSA *,
			       int);
int (*lc_RSA_public_decrypt) (int, unsigned char *, unsigned char *, RSA *,
			      int);
int (*lc_RSA_size) (RSA *);
void (*lc_SSLeay_add_all_algorithms) (void);
int (*lc_X509_NAME_cmp) (X509_NAME *, X509_NAME *);
void (*lc_X509_STORE_CTX_cleanup) (X509_STORE_CTX *);
void (*lc_X509_OBJECT_free_contents) (X509_OBJECT *);

#if SSLEAY_VERSION_NUMBER >= 0x00904100L
void (*lc_X509_STORE_CTX_init) (X509_STORE_CTX *, X509_STORE *, X509 *,
				STACK_OF (X509) *);
#else
void (*lc_X509_STORE_CTX_init) (X509_STORE_CTX *, X509_STORE *, X509 *,
				STACK *);
#endif

int (*lc_X509_STORE_add_cert) (X509_STORE *, X509 *);
X509_STORE *(*lc_X509_STORE_new) (void);
void (*lc_X509_STORE_free) (X509_STORE *);
X509 *(*lc_X509_dup) (X509 *);
void (*lc_X509_free) (X509 *);
X509_EXTENSION *(*lc_X509_get_ext) (X509 *, int);
int (*lc_X509_get_ext_by_NID) (X509 *, int, int);
X509_NAME *(*lc_X509_get_issuer_name) (X509 *);
EVP_PKEY *(*lc_X509_get_pubkey) (X509 *);
X509_NAME *(*lc_X509_get_subject_name) (X509 *);
X509 *(*lc_X509_new) (void);
int (*lc_X509_verify) (X509 *, EVP_PKEY *);
int (*lc_X509_verify_cert) (X509_STORE_CTX *);
RSA *(*lc_d2i_RSAPrivateKey) (RSA **, unsigned char **, long);
RSA *(*lc_d2i_RSAPublicKey) (RSA **, unsigned char **, long);
X509 *(*lc_d2i_X509) (X509 **, unsigned char **, long);
char *(*lc_X509_NAME_oneline) (X509_NAME *, char *, int);
int (*lc_i2d_RSAPublicKey) (RSA *, unsigned char **);
int (*lc_i2d_RSAPrivateKey) (RSA *, unsigned char **);
int (*lc_i2d_X509) (X509 *, unsigned char **);
#if SSLEAY_VERSION_NUMBER >= 0x00904100L
void (*lc_sk_X509_free) (STACK_OF (X509) *);
STACK_OF (X509) *(*lc_sk_X509_new_null) ();
#else
void (*lc_sk_free) (STACK *);
STACK *(*lc_sk_new) (int (*) ());
#endif

#if SSLEAY_VERSION_NUMBER >= 0x00904100L
X509 *(*lc_X509_find_by_subject) (STACK_OF (X509) *, X509_NAME *);
#else
X509 *(*lc_X509_find_by_subject) (STACK *, X509_NAME *);
#endif

int (*lc_X509_STORE_get_by_subject) (X509_STORE_CTX *, int, X509_NAME *,
				     X509_OBJECT *);

#define SYMENTRY(x) { SYM, SYM (x), (void **)&lc_ ## x }

static struct dynload_script libcrypto_script[] = {
  { LOAD, "libc.so", &libcrypto },
  { LOAD, "libcrypto.so", &libcrypto },
  SYMENTRY (ASN1_d2i_bio),
  SYMENTRY (ASN1_dup),
  SYMENTRY (BIO_ctrl),
  SYMENTRY (BIO_free),
  SYMENTRY (BIO_new),
  SYMENTRY (BIO_write),
  SYMENTRY (BIO_s_file),
  SYMENTRY (BIO_s_mem),
  SYMENTRY (BN_print_fp),
  SYMENTRY (PEM_ASN1_read_bio),
  SYMENTRY (RSA_generate_key),
  SYMENTRY (RSA_free),
  SYMENTRY (RSA_private_encrypt),
  SYMENTRY (RSA_public_decrypt),
  SYMENTRY (RSA_size),
  SYMENTRY (SSLeay_add_all_algorithms),
  SYMENTRY (X509_NAME_cmp),
  SYMENTRY (X509_STORE_CTX_cleanup),
  SYMENTRY (X509_STORE_CTX_init),
  SYMENTRY (X509_STORE_add_cert),
  SYMENTRY (X509_STORE_new),
  SYMENTRY (X509_STORE_free),
  SYMENTRY (X509_dup),
  SYMENTRY (X509_find_by_subject),
  SYMENTRY (X509_free),
  SYMENTRY (X509_get_ext),
  SYMENTRY (X509_get_ext_by_NID),
  SYMENTRY (X509_get_issuer_name),
  SYMENTRY (X509_get_pubkey),
  SYMENTRY (X509_get_subject_name),
  SYMENTRY (X509_new),
  SYMENTRY (X509_verify),
  SYMENTRY (X509_verify_cert),
  SYMENTRY (X509_STORE_get_by_subject),
  SYMENTRY (X509_OBJECT_free_contents),
  SYMENTRY (X509_NAME_oneline),
  SYMENTRY (d2i_RSAPrivateKey),
  SYMENTRY (d2i_RSAPublicKey),
  SYMENTRY (d2i_X509),
  SYMENTRY (i2d_RSAPublicKey),
  SYMENTRY (i2d_RSAPrivateKey),
  SYMENTRY (i2d_X509),
#if SSLEAY_VERSION_NUMBER >= 0x00904100L
  SYMENTRY (sk_X509_free),
  SYMENTRY (sk_X509_new_null),
#else
  SYMENTRY (sk_free),
  SYMENTRY (sk_new),
#endif
  { EOS }
};
#endif

void
libcrypto_init (void)
{
#ifdef HAVE_DLOPEN
  dyn_load (libcrypto_script);
#elif !defined (USE_LIBCRYPTO)
  return;
#endif

  /*
   * XXX Do something imaginative with libcrypto here.  The problem is if
   * the dynload fails libcrypto will be 0 which is good for the macros but
   * not the tests for support.
   */

#if defined (USE_LIBCRYPTO)
  /* Add all algorithms known by SSL */
  LC (SSLeay_add_all_algorithms, ());
#endif
}

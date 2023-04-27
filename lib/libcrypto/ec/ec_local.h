/* $OpenBSD: ec_local.h,v 1.13 2023/04/27 07:10:05 tb Exp $ */
/*
 * Originally written by Bodo Moeller for the OpenSSL project.
 */
/* ====================================================================
 * Copyright (c) 1998-2010 The OpenSSL Project.  All rights reserved.
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
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * The elliptic curve binary polynomial software is originally written by
 * Sheueling Chang Shantz and Douglas Stebila of Sun Microsystems Laboratories.
 *
 */

#include <stdlib.h>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/objects.h>

#include "bn_local.h"

__BEGIN_HIDDEN_DECLS

#if defined(__SUNPRO_C)
# if __SUNPRO_C >= 0x520
# pragma error_messages (off,E_ARRAY_OF_INCOMPLETE_NONAME,E_ARRAY_OF_INCOMPLETE)
# endif
#endif

struct ec_method_st {
	int field_type;

	int (*group_init)(EC_GROUP *);
	void (*group_finish)(EC_GROUP *);
	int (*group_copy)(EC_GROUP *, const EC_GROUP *);

	int (*group_set_curve)(EC_GROUP *, const BIGNUM *p, const BIGNUM *a,
	    const BIGNUM *b, BN_CTX *);
	int (*group_get_curve)(const EC_GROUP *, BIGNUM *p, BIGNUM *a,
	    BIGNUM *b, BN_CTX *);

	int (*group_get_degree)(const EC_GROUP *);
	int (*group_order_bits)(const EC_GROUP *);
	int (*group_check_discriminant)(const EC_GROUP *, BN_CTX *);

	int (*point_init)(EC_POINT *);
	void (*point_finish)(EC_POINT *);
	int (*point_copy)(EC_POINT *, const EC_POINT *);

	int (*point_set_to_infinity)(const EC_GROUP *, EC_POINT *);
	int (*point_set_Jprojective_coordinates)(const EC_GROUP *, EC_POINT *,
	    const BIGNUM *x, const BIGNUM *y, const BIGNUM *z, BN_CTX *);
	int (*point_get_Jprojective_coordinates)(const EC_GROUP *,
	    const EC_POINT *, BIGNUM *x, BIGNUM *y, BIGNUM *z, BN_CTX *);
	int (*point_set_affine_coordinates)(const EC_GROUP *, EC_POINT *,
	    const BIGNUM *x, const BIGNUM *y, BN_CTX *);
	int (*point_get_affine_coordinates)(const EC_GROUP *, const EC_POINT *,
	    BIGNUM *x, BIGNUM *y, BN_CTX *);
	int (*point_set_compressed_coordinates)(const EC_GROUP *, EC_POINT *,
	    const BIGNUM *x, int y_bit, BN_CTX *);

	size_t (*point2oct)(const EC_GROUP *, const EC_POINT *,
	    point_conversion_form_t form, unsigned char *buf, size_t len,
	    BN_CTX *);
	int (*oct2point)(const EC_GROUP *, EC_POINT *, const unsigned char *buf,
	    size_t len, BN_CTX *);

	int (*add)(const EC_GROUP *, EC_POINT *r, const EC_POINT *a,
	    const EC_POINT *b, BN_CTX *);
	int (*dbl)(const EC_GROUP *, EC_POINT *r, const EC_POINT *a, BN_CTX *);
	int (*invert)(const EC_GROUP *, EC_POINT *, BN_CTX *);

	int (*is_at_infinity)(const EC_GROUP *, const EC_POINT *);
	int (*is_on_curve)(const EC_GROUP *, const EC_POINT *, BN_CTX *);
	int (*point_cmp)(const EC_GROUP *, const EC_POINT *a, const EC_POINT *b,
	    BN_CTX *);

	int (*make_affine)(const EC_GROUP *, EC_POINT *, BN_CTX *);
	int (*points_make_affine)(const EC_GROUP *, size_t num, EC_POINT *[],
	    BN_CTX *);

	int (*mul_generator_ct)(const EC_GROUP *, EC_POINT *r,
	    const BIGNUM *scalar, BN_CTX *);
	int (*mul_single_ct)(const EC_GROUP *group, EC_POINT *r,
	    const BIGNUM *scalar, const EC_POINT *point, BN_CTX *);
	int (*mul_double_nonct)(const EC_GROUP *group, EC_POINT *r,
	    const BIGNUM *g_scalar, const BIGNUM *p_scalar,
	    const EC_POINT *point, BN_CTX *);
	int (*precompute_mult)(EC_GROUP *group, BN_CTX *);
	int (*have_precompute_mult)(const EC_GROUP *group);

	/*
	 * Internal methods.
	 */

	/*
	 * These can be used by 'add' and 'dbl' so that the same implementations
	 * of point operations can be used with different optimized versions of
	 * expensive field operations.
	 */
	int (*field_mul)(const EC_GROUP *, BIGNUM *r, const BIGNUM *a,
	    const BIGNUM *b, BN_CTX *);
	int (*field_sqr)(const EC_GROUP *, BIGNUM *r, const BIGNUM *a,
	    BN_CTX *);
	int (*field_div)(const EC_GROUP *, BIGNUM *r, const BIGNUM *a,
	    const BIGNUM *b, BN_CTX *);

	/* Encode to and decode from other forms (e.g. Montgomery). */
	int (*field_encode)(const EC_GROUP *, BIGNUM *r, const BIGNUM *a,
	    BN_CTX *);
	int (*field_decode)(const EC_GROUP *, BIGNUM *r, const BIGNUM *a,
	    BN_CTX *);

	int (*field_set_to_one)(const EC_GROUP *, BIGNUM *r, BN_CTX *);
	int (*blind_coordinates)(const EC_GROUP *group, EC_POINT *p,
	    BN_CTX *ctx);
} /* EC_METHOD */;

typedef struct ec_extra_data_st {
	struct ec_extra_data_st *next;
	void *data;
	void *(*dup_func)(void *);
	void (*free_func)(void *);
	void (*clear_free_func)(void *);
} EC_EXTRA_DATA; /* used in EC_GROUP */

struct ec_group_st {
	/*
	 * Methods and members exposed via the public API.
	 */

	const EC_METHOD *meth;

	EC_POINT *generator;	/* Optional */
	BIGNUM order;
	BIGNUM cofactor;

	int curve_name;		/* Optional NID for named curve. */

	/* ASN.1 encoding controls. */
	int asn1_flag;
	point_conversion_form_t asn1_form;

	/* Optional seed for parameters (appears in ASN.1). */
	unsigned char *seed;
	size_t seed_len;

	/*
	 * Internal methods and members. Handled by the method functions, even
	 * if they appear to be generic.
	 */

	EC_EXTRA_DATA *extra_data;

	/*
	 * Field specification. For GF(p) this is the modulus; for GF(2^m),
	 * this is the irreducible polynomial defining the field.
	 */
	BIGNUM field;

	/*
	 * Field specification for GF(2^m). The irreducible polynomial  is
	 *	f(t) = t^poly[0] + t^poly[1] + ... + t^poly[k],
	 * where
	 *	m = poly[0] > poly[1] > ... > poly[k] = 0,
	 * and the array is terminated with poly[k+1] = -1. All elliptic curve
	 * irreducibles have at most 5 non-zero terms.
	 */
	int poly[6];

	/*
	 * Curve coefficients. In characteristic > 3, the curve is defined by a
	 * Weierstrass equation of the form
	 *	y^2 = x^3 + a*x + b.
	 * For characteristic 2, the curve is defined by an equation of the form
	 *	y^2 + x*y = x^3 + a*x^2 + b.
	 */
	BIGNUM a, b;

	/* Enables optimized point arithmetics for special case. */
	int a_is_minus3;

	/* Montgomery context and values used by EC_GFp_mont_method. */
	BN_MONT_CTX *mont_ctx;
	BIGNUM *mont_one;

	int (*field_mod_func)(BIGNUM *, const BIGNUM *, const BIGNUM *,
	    BN_CTX *);
} /* EC_GROUP */;

struct ec_key_st {
	const EC_KEY_METHOD *meth;
	ENGINE	*engine;

	int version;

	EC_GROUP *group;

	EC_POINT *pub_key;
	BIGNUM	 *priv_key;

	unsigned int enc_flag;
	point_conversion_form_t conv_form;

	int	references;
	int	flags;

	EC_EXTRA_DATA *method_data;
	CRYPTO_EX_DATA ex_data;
} /* EC_KEY */;

/* Basically a 'mixin' for extra data, but available for EC_GROUPs/EC_KEYs only
 * (with visibility limited to 'package' level for now).
 * We use the function pointers as index for retrieval; this obviates
 * global ex_data-style index tables.
 */
int EC_EX_DATA_set_data(EC_EXTRA_DATA **, void *data,
	void *(*dup_func)(void *), void (*free_func)(void *), void (*clear_free_func)(void *));
void *EC_EX_DATA_get_data(const EC_EXTRA_DATA *,
	void *(*dup_func)(void *), void (*free_func)(void *), void (*clear_free_func)(void *));
void EC_EX_DATA_free_data(EC_EXTRA_DATA **,
	void *(*dup_func)(void *), void (*free_func)(void *), void (*clear_free_func)(void *));
void EC_EX_DATA_clear_free_data(EC_EXTRA_DATA **,
	void *(*dup_func)(void *), void (*free_func)(void *), void (*clear_free_func)(void *));
void EC_EX_DATA_free_all_data(EC_EXTRA_DATA **);
void EC_EX_DATA_clear_free_all_data(EC_EXTRA_DATA **);

int ec_group_simple_order_bits(const EC_GROUP *group);

struct ec_point_st {
	const EC_METHOD *meth;

	/*
	 * All members except 'meth' are handled by the method functions,
	 * even if they appear generic.
	 */

	/*
	 * Jacobian projective coordinates: (X, Y, Z) represents (X/Z^2, Y/Z^3)
	 * if Z != 0
	 */
	BIGNUM X;
	BIGNUM Y;
	BIGNUM Z;
	int Z_is_one; /* enable optimized point arithmetics for special case */
} /* EC_POINT */;

/* method functions in ec_mult.c
 * (ec_lib.c uses these as defaults if group->method->mul is 0) */
int ec_wNAF_mul(const EC_GROUP *group, EC_POINT *r, const BIGNUM *scalar,
	size_t num, const EC_POINT *points[], const BIGNUM *scalars[], BN_CTX *);
int ec_wNAF_precompute_mult(EC_GROUP *group, BN_CTX *);
int ec_wNAF_have_precompute_mult(const EC_GROUP *group);


/* method functions in ecp_smpl.c */
int ec_GFp_simple_group_init(EC_GROUP *);
void ec_GFp_simple_group_finish(EC_GROUP *);
int ec_GFp_simple_group_copy(EC_GROUP *, const EC_GROUP *);
int ec_GFp_simple_group_set_curve(EC_GROUP *, const BIGNUM *p, const BIGNUM *a, const BIGNUM *b, BN_CTX *);
int ec_GFp_simple_group_get_curve(const EC_GROUP *, BIGNUM *p, BIGNUM *a, BIGNUM *b, BN_CTX *);
int ec_GFp_simple_group_get_degree(const EC_GROUP *);
int ec_GFp_simple_group_check_discriminant(const EC_GROUP *, BN_CTX *);
int ec_GFp_simple_point_init(EC_POINT *);
void ec_GFp_simple_point_finish(EC_POINT *);
int ec_GFp_simple_point_copy(EC_POINT *, const EC_POINT *);
int ec_GFp_simple_point_set_to_infinity(const EC_GROUP *, EC_POINT *);
int ec_GFp_simple_set_Jprojective_coordinates(const EC_GROUP *, EC_POINT *,
    const BIGNUM *x, const BIGNUM *y, const BIGNUM *z, BN_CTX *);
int ec_GFp_simple_get_Jprojective_coordinates(const EC_GROUP *,
    const EC_POINT *, BIGNUM *x, BIGNUM *y, BIGNUM *z, BN_CTX *);
int ec_GFp_simple_point_set_affine_coordinates(const EC_GROUP *, EC_POINT *,
	const BIGNUM *x, const BIGNUM *y, BN_CTX *);
int ec_GFp_simple_point_get_affine_coordinates(const EC_GROUP *, const EC_POINT *,
	BIGNUM *x, BIGNUM *y, BN_CTX *);
int ec_GFp_simple_set_compressed_coordinates(const EC_GROUP *, EC_POINT *,
	const BIGNUM *x, int y_bit, BN_CTX *);
size_t ec_GFp_simple_point2oct(const EC_GROUP *, const EC_POINT *, point_conversion_form_t form,
	unsigned char *buf, size_t len, BN_CTX *);
int ec_GFp_simple_oct2point(const EC_GROUP *, EC_POINT *,
	const unsigned char *buf, size_t len, BN_CTX *);
int ec_GFp_simple_add(const EC_GROUP *, EC_POINT *r, const EC_POINT *a, const EC_POINT *b, BN_CTX *);
int ec_GFp_simple_dbl(const EC_GROUP *, EC_POINT *r, const EC_POINT *a, BN_CTX *);
int ec_GFp_simple_invert(const EC_GROUP *, EC_POINT *, BN_CTX *);
int ec_GFp_simple_is_at_infinity(const EC_GROUP *, const EC_POINT *);
int ec_GFp_simple_is_on_curve(const EC_GROUP *, const EC_POINT *, BN_CTX *);
int ec_GFp_simple_cmp(const EC_GROUP *, const EC_POINT *a, const EC_POINT *b, BN_CTX *);
int ec_GFp_simple_make_affine(const EC_GROUP *, EC_POINT *, BN_CTX *);
int ec_GFp_simple_points_make_affine(const EC_GROUP *, size_t num, EC_POINT *[], BN_CTX *);
int ec_GFp_simple_field_mul(const EC_GROUP *, BIGNUM *r, const BIGNUM *a, const BIGNUM *b, BN_CTX *);
int ec_GFp_simple_field_sqr(const EC_GROUP *, BIGNUM *r, const BIGNUM *a, BN_CTX *);
int ec_GFp_simple_blind_coordinates(const EC_GROUP *group, EC_POINT *p, BN_CTX *ctx);
int ec_GFp_simple_mul_generator_ct(const EC_GROUP *, EC_POINT *r, const BIGNUM *scalar, BN_CTX *);
int ec_GFp_simple_mul_single_ct(const EC_GROUP *, EC_POINT *r, const BIGNUM *scalar,
	const EC_POINT *point, BN_CTX *);
int ec_GFp_simple_mul_double_nonct(const EC_GROUP *, EC_POINT *r, const BIGNUM *g_scalar,
	const BIGNUM *p_scalar, const EC_POINT *point, BN_CTX *);

int ec_point_blind_coordinates(const EC_GROUP *group, EC_POINT *p, BN_CTX *ctx);

int ec_GF2m_simple_set_compressed_coordinates(const EC_GROUP *, EC_POINT *,
	const BIGNUM *x, int y_bit, BN_CTX *);
size_t ec_GF2m_simple_point2oct(const EC_GROUP *, const EC_POINT *, point_conversion_form_t form,
	unsigned char *buf, size_t len, BN_CTX *);
int ec_GF2m_simple_oct2point(const EC_GROUP *, EC_POINT *,
	const unsigned char *buf, size_t len, BN_CTX *);

int ec_GF2m_simple_mul(const EC_GROUP *group, EC_POINT *r, const BIGNUM *scalar,
	size_t num, const EC_POINT *points[], const BIGNUM *scalars[], BN_CTX *);
int ec_GF2m_precompute_mult(EC_GROUP *group, BN_CTX *ctx);
int ec_GF2m_have_precompute_mult(const EC_GROUP *group);

/* EC_METHOD definitions */

struct ec_key_method_st {
	const char *name;
	int32_t flags;
	int (*init)(EC_KEY *key);
	void (*finish)(EC_KEY *key);
	int (*copy)(EC_KEY *dest, const EC_KEY *src);
	int (*set_group)(EC_KEY *key, const EC_GROUP *grp);
	int (*set_private)(EC_KEY *key, const BIGNUM *priv_key);
	int (*set_public)(EC_KEY *key, const EC_POINT *pub_key);
	int (*keygen)(EC_KEY *key);
	int (*compute_key)(void *out, size_t outlen, const EC_POINT *pub_key, EC_KEY *ecdh,
	    void *(*KDF) (const void *in, size_t inlen, void *out, size_t *outlen));
	int (*sign)(int type, const unsigned char *dgst, int dlen, unsigned char
	    *sig, unsigned int *siglen, const BIGNUM *kinv,
	    const BIGNUM *r, EC_KEY *eckey);
	int (*sign_setup)(EC_KEY *eckey, BN_CTX *ctx_in, BIGNUM **kinvp,
	    BIGNUM **rp);
	ECDSA_SIG *(*sign_sig)(const unsigned char *dgst, int dgst_len,
	    const BIGNUM *in_kinv, const BIGNUM *in_r,
	    EC_KEY *eckey);
	int (*verify)(int type, const unsigned char *dgst, int dgst_len,
	    const unsigned char *sigbuf, int sig_len, EC_KEY *eckey);
	int (*verify_sig)(const unsigned char *dgst, int dgst_len,
	    const ECDSA_SIG *sig, EC_KEY *eckey);
} /* EC_KEY_METHOD */;

#define EC_KEY_METHOD_DYNAMIC   1

int ossl_ec_key_gen(EC_KEY *eckey);
int ossl_ecdh_compute_key(void *out, size_t outlen, const EC_POINT *pub_key, EC_KEY *ecdh,
    void *(*KDF) (const void *in, size_t inlen, void *out, size_t *outlen));
int ossl_ecdsa_verify(int type, const unsigned char *dgst, int dgst_len,
    const unsigned char *sigbuf, int sig_len, EC_KEY *eckey);
int ossl_ecdsa_verify_sig(const unsigned char *dgst, int dgst_len,
    const ECDSA_SIG *sig, EC_KEY *eckey);

void *EC_KEY_get_key_method_data(EC_KEY *key,
    void *(*dup_func)(void *), void (*free_func)(void *),
    void (*clear_free_func)(void *));
void *EC_KEY_insert_key_method_data(EC_KEY *key, void *data,
    void *(*dup_func)(void *), void (*free_func)(void *),
    void (*clear_free_func)(void *));

int EC_POINT_set_Jprojective_coordinates(const EC_GROUP *group, EC_POINT *p,
    const BIGNUM *x, const BIGNUM *y, const BIGNUM *z, BN_CTX *ctx);
int EC_POINT_get_Jprojective_coordinates(const EC_GROUP *group,
    const EC_POINT *p, BIGNUM *x, BIGNUM *y, BIGNUM *z, BN_CTX *ctx);

__END_HIDDEN_DECLS

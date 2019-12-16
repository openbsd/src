/*
 * Copyright (C) 2016, 2017  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* The documentation about this file is in README.site */

#ifndef PK11_SITE_H
#define PK11_SITE_H 1

/*! \file pk11/site.h */

/*\brief Put here specific PKCS#11 tweaks
 *
 *\li PK11_<mechanism>_SKIP:
 *	Don't consider the lack of this mechanism as a fatal error.
 *
 *\li PK11_<mechanism>_REPLACE:
 *      Same as SKIP, and implement the mechanism using lower-level steps.
 *
 *\li PK11_<algorithm>_DISABLE:
 *	Same as SKIP, and disable support for the algorithm.
 */

/* current implemented flags are:
PK11_DH_PKCS_PARAMETER_GEN_SKIP
PK11_DSA_PARAMETER_GEN_SKIP
PK11_RSA_PKCS_REPLACE
PK11_MD5_HMAC_REPLACE
PK11_SHA_1_HMAC_REPLACE
PK11_SHA224_HMAC_REPLACE
PK11_SHA256_HMAC_REPLACE
PK11_SHA384_HMAC_REPLACE
PK11_SHA512_HMAC_REPLACE
PK11_MD5_DISABLE
PK11_DSA_DISABLE
PK11_DH_DISABLE
*/

/*
 * Predefined flavors
 */
/* Thales nCipher */
#define PK11_THALES_FLAVOR 0
/* SoftHSMv1 with SHA224 */
#define PK11_SOFTHSMV1_FLAVOR 1
/* SoftHSMv2 */
#define PK11_SOFTHSMV2_FLAVOR 2
/* Cryptech */
#define PK11_CRYPTECH_FLAVOR 3
/* AEP Keyper */
#define PK11_AEP_FLAVOR 4

/* Default is for Thales nCipher */
#ifndef PK11_FLAVOR
#define PK11_FLAVOR PK11_THALES_FLAVOR
#endif

#if PK11_FLAVOR == PK11_THALES_FLAVOR
#define PK11_DH_PKCS_PARAMETER_GEN_SKIP
/* doesn't work but supported #define PK11_DSA_PARAMETER_GEN_SKIP */
#define PK11_MD5_HMAC_REPLACE
#endif

#if PK11_FLAVOR == PK11_SOFTHSMV1_FLAVOR
#define PK11_DH_DISABLE
#define PK11_DSA_DISABLE
#define PK11_MD5_HMAC_REPLACE
#define PK11_SHA_1_HMAC_REPLACE
#define PK11_SHA224_HMAC_REPLACE
#define PK11_SHA256_HMAC_REPLACE
#define PK11_SHA384_HMAC_REPLACE
#define PK11_SHA512_HMAC_REPLACE
#endif

#if PK11_FLAVOR == PK11_SOFTHSMV2_FLAVOR
#endif

#if PK11_FLAVOR == PK11_CRYPTECH_FLAVOR
#define PK11_DH_DISABLE
#define PK11_DSA_DISABLE
#define PK11_MD5_DISABLE
#define PK11_SHA_1_HMAC_REPLACE
#define PK11_SHA224_HMAC_REPLACE
#define PK11_SHA256_HMAC_REPLACE
#define PK11_SHA384_HMAC_REPLACE
#define PK11_SHA512_HMAC_REPLACE
#endif

#if PK11_FLAVOR == PK11_AEP_FLAVOR
#define PK11_DH_DISABLE
#define PK11_DSA_DISABLE
#define PK11_RSA_PKCS_REPLACE
#define PK11_MD5_HMAC_REPLACE
#define PK11_SHA_1_HMAC_REPLACE
#define PK11_SHA224_HMAC_REPLACE
#define PK11_SHA256_HMAC_REPLACE
#define PK11_SHA384_HMAC_REPLACE
#define PK11_SHA512_HMAC_REPLACE
#endif

#endif /* PK11_SITE_H */

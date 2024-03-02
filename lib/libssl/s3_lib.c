/* $OpenBSD: s3_lib.c,v 1.251 2024/03/02 11:46:55 tb Exp $ */
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
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * ECC cipher suite support in OpenSSL originally written by
 * Vipul Gupta and Sumit Gupta of Sun Microsystems Laboratories.
 *
 */
/* ====================================================================
 * Copyright 2005 Nokia. All rights reserved.
 *
 * The portions of the attached software ("Contribution") is developed by
 * Nokia Corporation and is licensed pursuant to the OpenSSL open source
 * license.
 *
 * The Contribution, originally written by Mika Kousa and Pasi Eronen of
 * Nokia Corporation, consists of the "PSK" (Pre-Shared Key) ciphersuites
 * support (see RFC 4279) to OpenSSL.
 *
 * No patent licenses or other rights except those expressly stated in
 * the OpenSSL open source license shall be deemed granted or received
 * expressly, by implication, estoppel, or otherwise.
 *
 * No assurances are provided by Nokia that the Contribution does not
 * infringe the patent or other intellectual property rights of any third
 * party or that the license provides you with all the necessary rights
 * to make use of the Contribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. IN
 * ADDITION TO THE DISCLAIMERS INCLUDED IN THE LICENSE, NOKIA
 * SPECIFICALLY DISCLAIMS ANY LIABILITY FOR CLAIMS BROUGHT BY YOU OR ANY
 * OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS OR
 * OTHERWISE.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <openssl/bn.h>
#include <openssl/curve25519.h>
#include <openssl/dh.h>
#include <openssl/md5.h>
#include <openssl/objects.h>
#include <openssl/opensslconf.h>

#include "bytestring.h"
#include "dtls_local.h"
#include "ssl_local.h"
#include "ssl_sigalgs.h"
#include "ssl_tlsext.h"
#include "tls_content.h"

#define SSL3_NUM_CIPHERS	(sizeof(ssl3_ciphers) / sizeof(SSL_CIPHER))

/*
 * FIXED_NONCE_LEN is a macro that provides in the correct value to set the
 * fixed nonce length in algorithms2. It is the inverse of the
 * SSL_CIPHER_AEAD_FIXED_NONCE_LEN macro.
 */
#define FIXED_NONCE_LEN(x) (((x / 2) & 0xf) << 24)

/* list of available SSLv3 ciphers (sorted by id) */
const SSL_CIPHER ssl3_ciphers[] = {

	/* The RSA ciphers */
	/* Cipher 01 */
	{
		.valid = 1,
		.name = SSL3_TXT_RSA_NULL_MD5,
		.id = SSL3_CK_RSA_NULL_MD5,
		.algorithm_mkey = SSL_kRSA,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_eNULL,
		.algorithm_mac = SSL_MD5,
		.algorithm_ssl = SSL_SSLV3,
		.algo_strength = SSL_STRONG_NONE,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 0,
		.alg_bits = 0,
	},

	/* Cipher 02 */
	{
		.valid = 1,
		.name = SSL3_TXT_RSA_NULL_SHA,
		.id = SSL3_CK_RSA_NULL_SHA,
		.algorithm_mkey = SSL_kRSA,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_eNULL,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_SSLV3,
		.algo_strength = SSL_STRONG_NONE,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 0,
		.alg_bits = 0,
	},

	/* Cipher 04 */
	{
		.valid = 1,
		.name = SSL3_TXT_RSA_RC4_128_MD5,
		.id = SSL3_CK_RSA_RC4_128_MD5,
		.algorithm_mkey = SSL_kRSA,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_RC4,
		.algorithm_mac = SSL_MD5,
		.algorithm_ssl = SSL_SSLV3,
		.algo_strength = SSL_LOW,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher 05 */
	{
		.valid = 1,
		.name = SSL3_TXT_RSA_RC4_128_SHA,
		.id = SSL3_CK_RSA_RC4_128_SHA,
		.algorithm_mkey = SSL_kRSA,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_RC4,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_SSLV3,
		.algo_strength = SSL_LOW,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher 0A */
	{
		.valid = 1,
		.name = SSL3_TXT_RSA_DES_192_CBC3_SHA,
		.id = SSL3_CK_RSA_DES_192_CBC3_SHA,
		.algorithm_mkey = SSL_kRSA,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_3DES,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_SSLV3,
		.algo_strength = SSL_MEDIUM,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 112,
		.alg_bits = 168,
	},

	/*
	 * Ephemeral DH (DHE) ciphers.
	 */

	/* Cipher 16 */
	{
		.valid = 1,
		.name = SSL3_TXT_EDH_RSA_DES_192_CBC3_SHA,
		.id = SSL3_CK_EDH_RSA_DES_192_CBC3_SHA,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_3DES,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_SSLV3,
		.algo_strength = SSL_MEDIUM,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 112,
		.alg_bits = 168,
	},

	/* Cipher 18 */
	{
		.valid = 1,
		.name = SSL3_TXT_ADH_RC4_128_MD5,
		.id = SSL3_CK_ADH_RC4_128_MD5,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aNULL,
		.algorithm_enc = SSL_RC4,
		.algorithm_mac = SSL_MD5,
		.algorithm_ssl = SSL_SSLV3,
		.algo_strength = SSL_LOW,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher 1B */
	{
		.valid = 1,
		.name = SSL3_TXT_ADH_DES_192_CBC_SHA,
		.id = SSL3_CK_ADH_DES_192_CBC_SHA,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aNULL,
		.algorithm_enc = SSL_3DES,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_SSLV3,
		.algo_strength = SSL_MEDIUM,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 112,
		.alg_bits = 168,
	},

	/*
	 * AES ciphersuites.
	 */

	/* Cipher 2F */
	{
		.valid = 1,
		.name = TLS1_TXT_RSA_WITH_AES_128_SHA,
		.id = TLS1_CK_RSA_WITH_AES_128_SHA,
		.algorithm_mkey = SSL_kRSA,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES128,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher 33 */
	{
		.valid = 1,
		.name = TLS1_TXT_DHE_RSA_WITH_AES_128_SHA,
		.id = TLS1_CK_DHE_RSA_WITH_AES_128_SHA,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES128,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher 34 */
	{
		.valid = 1,
		.name = TLS1_TXT_ADH_WITH_AES_128_SHA,
		.id = TLS1_CK_ADH_WITH_AES_128_SHA,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aNULL,
		.algorithm_enc = SSL_AES128,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher 35 */
	{
		.valid = 1,
		.name = TLS1_TXT_RSA_WITH_AES_256_SHA,
		.id = TLS1_CK_RSA_WITH_AES_256_SHA,
		.algorithm_mkey = SSL_kRSA,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES256,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* Cipher 39 */
	{
		.valid = 1,
		.name = TLS1_TXT_DHE_RSA_WITH_AES_256_SHA,
		.id = TLS1_CK_DHE_RSA_WITH_AES_256_SHA,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES256,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* Cipher 3A */
	{
		.valid = 1,
		.name = TLS1_TXT_ADH_WITH_AES_256_SHA,
		.id = TLS1_CK_ADH_WITH_AES_256_SHA,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aNULL,
		.algorithm_enc = SSL_AES256,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* TLS v1.2 ciphersuites */
	/* Cipher 3B */
	{
		.valid = 1,
		.name = TLS1_TXT_RSA_WITH_NULL_SHA256,
		.id = TLS1_CK_RSA_WITH_NULL_SHA256,
		.algorithm_mkey = SSL_kRSA,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_eNULL,
		.algorithm_mac = SSL_SHA256,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_STRONG_NONE,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256,
		.strength_bits = 0,
		.alg_bits = 0,
	},

	/* Cipher 3C */
	{
		.valid = 1,
		.name = TLS1_TXT_RSA_WITH_AES_128_SHA256,
		.id = TLS1_CK_RSA_WITH_AES_128_SHA256,
		.algorithm_mkey = SSL_kRSA,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES128,
		.algorithm_mac = SSL_SHA256,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher 3D */
	{
		.valid = 1,
		.name = TLS1_TXT_RSA_WITH_AES_256_SHA256,
		.id = TLS1_CK_RSA_WITH_AES_256_SHA256,
		.algorithm_mkey = SSL_kRSA,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES256,
		.algorithm_mac = SSL_SHA256,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256,
		.strength_bits = 256,
		.alg_bits = 256,
	},

#ifndef OPENSSL_NO_CAMELLIA
	/* Camellia ciphersuites from RFC4132 (128-bit portion) */

	/* Cipher 41 */
	{
		.valid = 1,
		.name = TLS1_TXT_RSA_WITH_CAMELLIA_128_CBC_SHA,
		.id = TLS1_CK_RSA_WITH_CAMELLIA_128_CBC_SHA,
		.algorithm_mkey = SSL_kRSA,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_CAMELLIA128,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher 45 */
	{
		.valid = 1,
		.name = TLS1_TXT_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,
		.id = TLS1_CK_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_CAMELLIA128,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher 46 */
	{
		.valid = 1,
		.name = TLS1_TXT_ADH_WITH_CAMELLIA_128_CBC_SHA,
		.id = TLS1_CK_ADH_WITH_CAMELLIA_128_CBC_SHA,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aNULL,
		.algorithm_enc = SSL_CAMELLIA128,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 128,
		.alg_bits = 128,
	},
#endif /* OPENSSL_NO_CAMELLIA */

	/* TLS v1.2 ciphersuites */
	/* Cipher 67 */
	{
		.valid = 1,
		.name = TLS1_TXT_DHE_RSA_WITH_AES_128_SHA256,
		.id = TLS1_CK_DHE_RSA_WITH_AES_128_SHA256,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES128,
		.algorithm_mac = SSL_SHA256,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher 6B */
	{
		.valid = 1,
		.name = TLS1_TXT_DHE_RSA_WITH_AES_256_SHA256,
		.id = TLS1_CK_DHE_RSA_WITH_AES_256_SHA256,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES256,
		.algorithm_mac = SSL_SHA256,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256,
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* Cipher 6C */
	{
		.valid = 1,
		.name = TLS1_TXT_ADH_WITH_AES_128_SHA256,
		.id = TLS1_CK_ADH_WITH_AES_128_SHA256,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aNULL,
		.algorithm_enc = SSL_AES128,
		.algorithm_mac = SSL_SHA256,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher 6D */
	{
		.valid = 1,
		.name = TLS1_TXT_ADH_WITH_AES_256_SHA256,
		.id = TLS1_CK_ADH_WITH_AES_256_SHA256,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aNULL,
		.algorithm_enc = SSL_AES256,
		.algorithm_mac = SSL_SHA256,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256,
		.strength_bits = 256,
		.alg_bits = 256,
	},

#ifndef OPENSSL_NO_CAMELLIA
	/* Camellia ciphersuites from RFC4132 (256-bit portion) */

	/* Cipher 84 */
	{
		.valid = 1,
		.name = TLS1_TXT_RSA_WITH_CAMELLIA_256_CBC_SHA,
		.id = TLS1_CK_RSA_WITH_CAMELLIA_256_CBC_SHA,
		.algorithm_mkey = SSL_kRSA,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_CAMELLIA256,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* Cipher 88 */
	{
		.valid = 1,
		.name = TLS1_TXT_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
		.id = TLS1_CK_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_CAMELLIA256,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* Cipher 89 */
	{
		.valid = 1,
		.name = TLS1_TXT_ADH_WITH_CAMELLIA_256_CBC_SHA,
		.id = TLS1_CK_ADH_WITH_CAMELLIA_256_CBC_SHA,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aNULL,
		.algorithm_enc = SSL_CAMELLIA256,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 256,
		.alg_bits = 256,
	},
#endif /* OPENSSL_NO_CAMELLIA */

	/*
	 * GCM ciphersuites from RFC5288.
	 */

	/* Cipher 9C */
	{
		.valid = 1,
		.name = TLS1_TXT_RSA_WITH_AES_128_GCM_SHA256,
		.id = TLS1_CK_RSA_WITH_AES_128_GCM_SHA256,
		.algorithm_mkey = SSL_kRSA,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES128GCM,
		.algorithm_mac = SSL_AEAD,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256|
		    FIXED_NONCE_LEN(4)|
		    SSL_CIPHER_ALGORITHM2_VARIABLE_NONCE_IN_RECORD,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher 9D */
	{
		.valid = 1,
		.name = TLS1_TXT_RSA_WITH_AES_256_GCM_SHA384,
		.id = TLS1_CK_RSA_WITH_AES_256_GCM_SHA384,
		.algorithm_mkey = SSL_kRSA,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES256GCM,
		.algorithm_mac = SSL_AEAD,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA384|TLS1_PRF_SHA384|
		    FIXED_NONCE_LEN(4)|
		    SSL_CIPHER_ALGORITHM2_VARIABLE_NONCE_IN_RECORD,
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* Cipher 9E */
	{
		.valid = 1,
		.name = TLS1_TXT_DHE_RSA_WITH_AES_128_GCM_SHA256,
		.id = TLS1_CK_DHE_RSA_WITH_AES_128_GCM_SHA256,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES128GCM,
		.algorithm_mac = SSL_AEAD,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256|
		    FIXED_NONCE_LEN(4)|
		    SSL_CIPHER_ALGORITHM2_VARIABLE_NONCE_IN_RECORD,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher 9F */
	{
		.valid = 1,
		.name = TLS1_TXT_DHE_RSA_WITH_AES_256_GCM_SHA384,
		.id = TLS1_CK_DHE_RSA_WITH_AES_256_GCM_SHA384,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES256GCM,
		.algorithm_mac = SSL_AEAD,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA384|TLS1_PRF_SHA384|
		    FIXED_NONCE_LEN(4)|
		    SSL_CIPHER_ALGORITHM2_VARIABLE_NONCE_IN_RECORD,
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* Cipher A6 */
	{
		.valid = 1,
		.name = TLS1_TXT_ADH_WITH_AES_128_GCM_SHA256,
		.id = TLS1_CK_ADH_WITH_AES_128_GCM_SHA256,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aNULL,
		.algorithm_enc = SSL_AES128GCM,
		.algorithm_mac = SSL_AEAD,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256|
		    FIXED_NONCE_LEN(4)|
		    SSL_CIPHER_ALGORITHM2_VARIABLE_NONCE_IN_RECORD,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher A7 */
	{
		.valid = 1,
		.name = TLS1_TXT_ADH_WITH_AES_256_GCM_SHA384,
		.id = TLS1_CK_ADH_WITH_AES_256_GCM_SHA384,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aNULL,
		.algorithm_enc = SSL_AES256GCM,
		.algorithm_mac = SSL_AEAD,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA384|TLS1_PRF_SHA384|
		    FIXED_NONCE_LEN(4)|
		    SSL_CIPHER_ALGORITHM2_VARIABLE_NONCE_IN_RECORD,
		.strength_bits = 256,
		.alg_bits = 256,
	},

#ifndef OPENSSL_NO_CAMELLIA
	/* TLS 1.2 Camellia SHA-256 ciphersuites from RFC5932 */

	/* Cipher BA */
	{
		.valid = 1,
		.name = TLS1_TXT_RSA_WITH_CAMELLIA_128_CBC_SHA256,
		.id = TLS1_CK_RSA_WITH_CAMELLIA_128_CBC_SHA256,
		.algorithm_mkey = SSL_kRSA,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_CAMELLIA128,
		.algorithm_mac = SSL_SHA256,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher BE */
	{
		.valid = 1,
		.name = TLS1_TXT_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256,
		.id = TLS1_CK_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_CAMELLIA128,
		.algorithm_mac = SSL_SHA256,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher BF */
	{
		.valid = 1,
		.name = TLS1_TXT_ADH_WITH_CAMELLIA_128_CBC_SHA256,
		.id = TLS1_CK_ADH_WITH_CAMELLIA_128_CBC_SHA256,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aNULL,
		.algorithm_enc = SSL_CAMELLIA128,
		.algorithm_mac = SSL_SHA256,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher C0 */
	{
		.valid = 1,
		.name = TLS1_TXT_RSA_WITH_CAMELLIA_256_CBC_SHA256,
		.id = TLS1_CK_RSA_WITH_CAMELLIA_256_CBC_SHA256,
		.algorithm_mkey = SSL_kRSA,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_CAMELLIA256,
		.algorithm_mac = SSL_SHA256,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256,
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* Cipher C4 */
	{
		.valid = 1,
		.name = TLS1_TXT_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256,
		.id = TLS1_CK_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_CAMELLIA256,
		.algorithm_mac = SSL_SHA256,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256,
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* Cipher C5 */
	{
		.valid = 1,
		.name = TLS1_TXT_ADH_WITH_CAMELLIA_256_CBC_SHA256,
		.id = TLS1_CK_ADH_WITH_CAMELLIA_256_CBC_SHA256,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aNULL,
		.algorithm_enc = SSL_CAMELLIA256,
		.algorithm_mac = SSL_SHA256,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256,
		.strength_bits = 256,
		.alg_bits = 256,
	},
#endif /* OPENSSL_NO_CAMELLIA */

	/*
	 * TLSv1.3 cipher suites.
	 */

#ifdef LIBRESSL_HAS_TLS1_3
	/* Cipher 1301 */
	{
		.valid = 1,
		.name = TLS1_3_RFC_AES_128_GCM_SHA256,
		.id = TLS1_3_CK_AES_128_GCM_SHA256,
		.algorithm_mkey = SSL_kTLS1_3,
		.algorithm_auth = SSL_aTLS1_3,
		.algorithm_enc = SSL_AES128GCM,
		.algorithm_mac = SSL_AEAD,
		.algorithm_ssl = SSL_TLSV1_3,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256, /* XXX */
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher 1302 */
	{
		.valid = 1,
		.name = TLS1_3_RFC_AES_256_GCM_SHA384,
		.id = TLS1_3_CK_AES_256_GCM_SHA384,
		.algorithm_mkey = SSL_kTLS1_3,
		.algorithm_auth = SSL_aTLS1_3,
		.algorithm_enc = SSL_AES256GCM,
		.algorithm_mac = SSL_AEAD,
		.algorithm_ssl = SSL_TLSV1_3,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA384, /* XXX */
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* Cipher 1303 */
	{
		.valid = 1,
		.name = TLS1_3_RFC_CHACHA20_POLY1305_SHA256,
		.id = TLS1_3_CK_CHACHA20_POLY1305_SHA256,
		.algorithm_mkey = SSL_kTLS1_3,
		.algorithm_auth = SSL_aTLS1_3,
		.algorithm_enc = SSL_CHACHA20POLY1305,
		.algorithm_mac = SSL_AEAD,
		.algorithm_ssl = SSL_TLSV1_3,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256, /* XXX */
		.strength_bits = 256,
		.alg_bits = 256,
	},
#endif

	/* Cipher C006 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_ECDSA_WITH_NULL_SHA,
		.id = TLS1_CK_ECDHE_ECDSA_WITH_NULL_SHA,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aECDSA,
		.algorithm_enc = SSL_eNULL,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_STRONG_NONE,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 0,
		.alg_bits = 0,
	},

	/* Cipher C007 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_ECDSA_WITH_RC4_128_SHA,
		.id = TLS1_CK_ECDHE_ECDSA_WITH_RC4_128_SHA,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aECDSA,
		.algorithm_enc = SSL_RC4,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_LOW,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher C008 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_ECDSA_WITH_DES_192_CBC3_SHA,
		.id = TLS1_CK_ECDHE_ECDSA_WITH_DES_192_CBC3_SHA,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aECDSA,
		.algorithm_enc = SSL_3DES,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_MEDIUM,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 112,
		.alg_bits = 168,
	},

	/* Cipher C009 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
		.id = TLS1_CK_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aECDSA,
		.algorithm_enc = SSL_AES128,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher C00A */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
		.id = TLS1_CK_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aECDSA,
		.algorithm_enc = SSL_AES256,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* Cipher C010 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_RSA_WITH_NULL_SHA,
		.id = TLS1_CK_ECDHE_RSA_WITH_NULL_SHA,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_eNULL,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_STRONG_NONE,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 0,
		.alg_bits = 0,
	},

	/* Cipher C011 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_RSA_WITH_RC4_128_SHA,
		.id = TLS1_CK_ECDHE_RSA_WITH_RC4_128_SHA,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_RC4,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_LOW,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher C012 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_RSA_WITH_DES_192_CBC3_SHA,
		.id = TLS1_CK_ECDHE_RSA_WITH_DES_192_CBC3_SHA,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_3DES,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_MEDIUM,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 112,
		.alg_bits = 168,
	},

	/* Cipher C013 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_RSA_WITH_AES_128_CBC_SHA,
		.id = TLS1_CK_ECDHE_RSA_WITH_AES_128_CBC_SHA,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES128,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher C014 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_RSA_WITH_AES_256_CBC_SHA,
		.id = TLS1_CK_ECDHE_RSA_WITH_AES_256_CBC_SHA,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES256,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* Cipher C015 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDH_anon_WITH_NULL_SHA,
		.id = TLS1_CK_ECDH_anon_WITH_NULL_SHA,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aNULL,
		.algorithm_enc = SSL_eNULL,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_STRONG_NONE,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 0,
		.alg_bits = 0,
	},

	/* Cipher C016 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDH_anon_WITH_RC4_128_SHA,
		.id = TLS1_CK_ECDH_anon_WITH_RC4_128_SHA,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aNULL,
		.algorithm_enc = SSL_RC4,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_LOW,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher C017 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDH_anon_WITH_DES_192_CBC3_SHA,
		.id = TLS1_CK_ECDH_anon_WITH_DES_192_CBC3_SHA,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aNULL,
		.algorithm_enc = SSL_3DES,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_MEDIUM,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 112,
		.alg_bits = 168,
	},

	/* Cipher C018 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDH_anon_WITH_AES_128_CBC_SHA,
		.id = TLS1_CK_ECDH_anon_WITH_AES_128_CBC_SHA,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aNULL,
		.algorithm_enc = SSL_AES128,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher C019 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDH_anon_WITH_AES_256_CBC_SHA,
		.id = TLS1_CK_ECDH_anon_WITH_AES_256_CBC_SHA,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aNULL,
		.algorithm_enc = SSL_AES256,
		.algorithm_mac = SSL_SHA1,
		.algorithm_ssl = SSL_TLSV1,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_DEFAULT|TLS1_PRF,
		.strength_bits = 256,
		.alg_bits = 256,
	},


	/* HMAC based TLS v1.2 ciphersuites from RFC5289 */

	/* Cipher C023 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_SHA256,
		.id = TLS1_CK_ECDHE_ECDSA_WITH_AES_128_SHA256,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aECDSA,
		.algorithm_enc = SSL_AES128,
		.algorithm_mac = SSL_SHA256,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher C024 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_SHA384,
		.id = TLS1_CK_ECDHE_ECDSA_WITH_AES_256_SHA384,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aECDSA,
		.algorithm_enc = SSL_AES256,
		.algorithm_mac = SSL_SHA384,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA384|TLS1_PRF_SHA384,
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* Cipher C027 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_RSA_WITH_AES_128_SHA256,
		.id = TLS1_CK_ECDHE_RSA_WITH_AES_128_SHA256,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES128,
		.algorithm_mac = SSL_SHA256,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher C028 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_RSA_WITH_AES_256_SHA384,
		.id = TLS1_CK_ECDHE_RSA_WITH_AES_256_SHA384,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES256,
		.algorithm_mac = SSL_SHA384,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA384|TLS1_PRF_SHA384,
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* GCM based TLS v1.2 ciphersuites from RFC5289 */

	/* Cipher C02B */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		.id = TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aECDSA,
		.algorithm_enc = SSL_AES128GCM,
		.algorithm_mac = SSL_AEAD,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256|
		    FIXED_NONCE_LEN(4)|
		    SSL_CIPHER_ALGORITHM2_VARIABLE_NONCE_IN_RECORD,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher C02C */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
		.id = TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aECDSA,
		.algorithm_enc = SSL_AES256GCM,
		.algorithm_mac = SSL_AEAD,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA384|TLS1_PRF_SHA384|
		    FIXED_NONCE_LEN(4)|
		    SSL_CIPHER_ALGORITHM2_VARIABLE_NONCE_IN_RECORD,
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* Cipher C02F */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
		.id = TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES128GCM,
		.algorithm_mac = SSL_AEAD,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256|
		    FIXED_NONCE_LEN(4)|
		    SSL_CIPHER_ALGORITHM2_VARIABLE_NONCE_IN_RECORD,
		.strength_bits = 128,
		.alg_bits = 128,
	},

	/* Cipher C030 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
		.id = TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_AES256GCM,
		.algorithm_mac = SSL_AEAD,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA384|TLS1_PRF_SHA384|
		    FIXED_NONCE_LEN(4)|
		    SSL_CIPHER_ALGORITHM2_VARIABLE_NONCE_IN_RECORD,
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* Cipher CCA8 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_RSA_WITH_CHACHA20_POLY1305,
		.id = TLS1_CK_ECDHE_RSA_CHACHA20_POLY1305,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_CHACHA20POLY1305,
		.algorithm_mac = SSL_AEAD,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256|
		    FIXED_NONCE_LEN(12),
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* Cipher CCA9 */
	{
		.valid = 1,
		.name = TLS1_TXT_ECDHE_ECDSA_WITH_CHACHA20_POLY1305,
		.id = TLS1_CK_ECDHE_ECDSA_CHACHA20_POLY1305,
		.algorithm_mkey = SSL_kECDHE,
		.algorithm_auth = SSL_aECDSA,
		.algorithm_enc = SSL_CHACHA20POLY1305,
		.algorithm_mac = SSL_AEAD,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256|
		    FIXED_NONCE_LEN(12),
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* Cipher CCAA */
	{
		.valid = 1,
		.name = TLS1_TXT_DHE_RSA_WITH_CHACHA20_POLY1305,
		.id = TLS1_CK_DHE_RSA_CHACHA20_POLY1305,
		.algorithm_mkey = SSL_kDHE,
		.algorithm_auth = SSL_aRSA,
		.algorithm_enc = SSL_CHACHA20POLY1305,
		.algorithm_mac = SSL_AEAD,
		.algorithm_ssl = SSL_TLSV1_2,
		.algo_strength = SSL_HIGH,
		.algorithm2 = SSL_HANDSHAKE_MAC_SHA256|TLS1_PRF_SHA256|
		    FIXED_NONCE_LEN(12),
		.strength_bits = 256,
		.alg_bits = 256,
	},

	/* end of list */
};

int
ssl3_num_ciphers(void)
{
	return (SSL3_NUM_CIPHERS);
}

const SSL_CIPHER *
ssl3_get_cipher(unsigned int u)
{
	if (u < SSL3_NUM_CIPHERS)
		return (&(ssl3_ciphers[SSL3_NUM_CIPHERS - 1 - u]));
	else
		return (NULL);
}

static int
ssl3_cipher_id_cmp(const void *id, const void *cipher)
{
	unsigned long a = *(const unsigned long *)id;
	unsigned long b = ((const SSL_CIPHER *)cipher)->id;

	return a < b ? -1 : a > b;
}

const SSL_CIPHER *
ssl3_get_cipher_by_id(unsigned long id)
{
	const SSL_CIPHER *cipher;

	cipher = bsearch(&id, ssl3_ciphers, SSL3_NUM_CIPHERS, sizeof(*cipher),
	    ssl3_cipher_id_cmp);
	if (cipher != NULL && cipher->valid == 1)
		return cipher;

	return NULL;
}

const SSL_CIPHER *
ssl3_get_cipher_by_value(uint16_t value)
{
	return ssl3_get_cipher_by_id(SSL3_CK_ID | value);
}

uint16_t
ssl3_cipher_get_value(const SSL_CIPHER *c)
{
	return (c->id & SSL3_CK_VALUE_MASK);
}

int
ssl3_pending(const SSL *s)
{
	if (s->s3->rcontent == NULL)
		return 0;
	if (tls_content_type(s->s3->rcontent) != SSL3_RT_APPLICATION_DATA)
		return 0;

	return tls_content_remaining(s->s3->rcontent);
}

int
ssl3_handshake_msg_hdr_len(SSL *s)
{
	return (SSL_is_dtls(s) ? DTLS1_HM_HEADER_LENGTH :
            SSL3_HM_HEADER_LENGTH);
}

int
ssl3_handshake_msg_start(SSL *s, CBB *handshake, CBB *body, uint8_t msg_type)
{
	int ret = 0;

	if (!CBB_init(handshake, SSL3_RT_MAX_PLAIN_LENGTH))
		goto err;
	if (!CBB_add_u8(handshake, msg_type))
		goto err;
	if (SSL_is_dtls(s)) {
		unsigned char *data;

		if (!CBB_add_space(handshake, &data, DTLS1_HM_HEADER_LENGTH -
		    SSL3_HM_HEADER_LENGTH))
			goto err;
	}
	if (!CBB_add_u24_length_prefixed(handshake, body))
		goto err;

	ret = 1;

 err:
	return (ret);
}

int
ssl3_handshake_msg_finish(SSL *s, CBB *handshake)
{
	unsigned char *data = NULL;
	size_t outlen;
	int ret = 0;

	if (!CBB_finish(handshake, &data, &outlen))
		goto err;

	if (outlen > INT_MAX)
		goto err;

	if (!BUF_MEM_grow_clean(s->init_buf, outlen))
		goto err;

	memcpy(s->init_buf->data, data, outlen);

	s->init_num = (int)outlen;
	s->init_off = 0;

	if (SSL_is_dtls(s)) {
		unsigned long len;
		uint8_t msg_type;
		CBS cbs;

		CBS_init(&cbs, data, outlen);
		if (!CBS_get_u8(&cbs, &msg_type))
			goto err;

		len = outlen - ssl3_handshake_msg_hdr_len(s);

		dtls1_set_message_header(s, msg_type, len, 0, len);
		dtls1_buffer_message(s, 0);
	}

	ret = 1;

 err:
	free(data);

	return (ret);
}

int
ssl3_handshake_write(SSL *s)
{
	return ssl3_record_write(s, SSL3_RT_HANDSHAKE);
}

int
ssl3_record_write(SSL *s, int type)
{
	if (SSL_is_dtls(s))
		return dtls1_do_write(s, type);

	return ssl3_do_write(s, type);
}

int
ssl3_new(SSL *s)
{
	if ((s->s3 = calloc(1, sizeof(*s->s3))) == NULL)
		return (0);

	s->method->ssl_clear(s);

	return (1);
}

void
ssl3_free(SSL *s)
{
	if (s == NULL)
		return;

	tls1_cleanup_key_block(s);
	ssl3_release_read_buffer(s);
	ssl3_release_write_buffer(s);

	tls_content_free(s->s3->rcontent);

	tls_buffer_free(s->s3->alert_fragment);
	tls_buffer_free(s->s3->handshake_fragment);

	freezero(s->s3->hs.sigalgs, s->s3->hs.sigalgs_len);
	sk_X509_pop_free(s->s3->hs.peer_certs, X509_free);
	sk_X509_pop_free(s->s3->hs.peer_certs_no_leaf, X509_free);
	sk_X509_pop_free(s->s3->hs.verified_chain, X509_free);
	tls_key_share_free(s->s3->hs.key_share);

	tls13_secrets_destroy(s->s3->hs.tls13.secrets);
	freezero(s->s3->hs.tls13.cookie, s->s3->hs.tls13.cookie_len);
	tls13_clienthello_hash_clear(&s->s3->hs.tls13);

	tls_buffer_free(s->s3->hs.tls13.quic_read_buffer);

	sk_X509_NAME_pop_free(s->s3->hs.tls12.ca_names, X509_NAME_free);

	tls1_transcript_free(s);
	tls1_transcript_hash_free(s);

	free(s->s3->alpn_selected);

	freezero(s->s3->peer_quic_transport_params,
	    s->s3->peer_quic_transport_params_len);

	freezero(s->s3, sizeof(*s->s3));

	s->s3 = NULL;
}

void
ssl3_clear(SSL *s)
{
	unsigned char *rp, *wp;
	size_t rlen, wlen;

	tls1_cleanup_key_block(s);
	sk_X509_NAME_pop_free(s->s3->hs.tls12.ca_names, X509_NAME_free);

	tls_buffer_free(s->s3->alert_fragment);
	s->s3->alert_fragment = NULL;
	tls_buffer_free(s->s3->handshake_fragment);
	s->s3->handshake_fragment = NULL;

	freezero(s->s3->hs.sigalgs, s->s3->hs.sigalgs_len);
	s->s3->hs.sigalgs = NULL;
	s->s3->hs.sigalgs_len = 0;

	sk_X509_pop_free(s->s3->hs.peer_certs, X509_free);
	s->s3->hs.peer_certs = NULL;
	sk_X509_pop_free(s->s3->hs.peer_certs_no_leaf, X509_free);
	s->s3->hs.peer_certs_no_leaf = NULL;
	sk_X509_pop_free(s->s3->hs.verified_chain, X509_free);
	s->s3->hs.verified_chain = NULL;

	tls_key_share_free(s->s3->hs.key_share);
	s->s3->hs.key_share = NULL;

	tls13_secrets_destroy(s->s3->hs.tls13.secrets);
	s->s3->hs.tls13.secrets = NULL;
	freezero(s->s3->hs.tls13.cookie, s->s3->hs.tls13.cookie_len);
	s->s3->hs.tls13.cookie = NULL;
	s->s3->hs.tls13.cookie_len = 0;
	tls13_clienthello_hash_clear(&s->s3->hs.tls13);

	tls_buffer_free(s->s3->hs.tls13.quic_read_buffer);
	s->s3->hs.tls13.quic_read_buffer = NULL;
	s->s3->hs.tls13.quic_read_level = ssl_encryption_initial;
	s->s3->hs.tls13.quic_write_level = ssl_encryption_initial;

	s->s3->hs.extensions_seen = 0;

	rp = s->s3->rbuf.buf;
	wp = s->s3->wbuf.buf;
	rlen = s->s3->rbuf.len;
	wlen = s->s3->wbuf.len;

	tls_content_free(s->s3->rcontent);
	s->s3->rcontent = NULL;

	tls1_transcript_free(s);
	tls1_transcript_hash_free(s);

	free(s->s3->alpn_selected);
	s->s3->alpn_selected = NULL;
	s->s3->alpn_selected_len = 0;

	freezero(s->s3->peer_quic_transport_params,
	    s->s3->peer_quic_transport_params_len);
	s->s3->peer_quic_transport_params = NULL;
	s->s3->peer_quic_transport_params_len = 0;

	memset(s->s3, 0, sizeof(*s->s3));

	s->s3->rbuf.buf = rp;
	s->s3->wbuf.buf = wp;
	s->s3->rbuf.len = rlen;
	s->s3->wbuf.len = wlen;

	ssl_free_wbio_buffer(s);

	/* Not needed... */
	s->s3->renegotiate = 0;
	s->s3->total_renegotiations = 0;
	s->s3->num_renegotiations = 0;
	s->s3->in_read_app_data = 0;

	s->packet_length = 0;
	s->version = TLS1_2_VERSION;

	s->s3->hs.state = SSL_ST_BEFORE|((s->server) ? SSL_ST_ACCEPT : SSL_ST_CONNECT);
}

long
_SSL_get_shared_group(SSL *s, long n)
{
	size_t count;
	int nid;

	/* OpenSSL document that they return -1 for clients. They return 0. */
	if (!s->server)
		return 0;

	if (n == -1) {
		if (!tls1_count_shared_groups(s, &count))
			return 0;

		if (count > LONG_MAX)
			count = LONG_MAX;

		return count;
	}

	/* Undocumented special case added for Suite B profile support. */
	if (n == -2)
		n = 0;

	if (n < 0)
		return 0;

	if (!tls1_get_shared_group_by_index(s, n, &nid))
		return NID_undef;

	return nid;
}

long
_SSL_get_peer_tmp_key(SSL *s, EVP_PKEY **key)
{
	EVP_PKEY *pkey = NULL;
	int ret = 0;

	*key = NULL;

	if (s->s3->hs.key_share == NULL)
		goto err;

	if ((pkey = EVP_PKEY_new()) == NULL)
		goto err;
	if (!tls_key_share_peer_pkey(s->s3->hs.key_share, pkey))
		goto err;

	*key = pkey;
	pkey = NULL;

	ret = 1;

 err:
	EVP_PKEY_free(pkey);

	return (ret);
}

static int
_SSL_session_reused(SSL *s)
{
	return s->hit;
}

static int
_SSL_num_renegotiations(SSL *s)
{
	return s->s3->num_renegotiations;
}

static int
_SSL_clear_num_renegotiations(SSL *s)
{
	int renegs;

	renegs = s->s3->num_renegotiations;
	s->s3->num_renegotiations = 0;

	return renegs;
}

static int
_SSL_total_renegotiations(SSL *s)
{
	return s->s3->total_renegotiations;
}

static int
_SSL_set_tmp_dh(SSL *s, DH *dh)
{
	DH *dhe_params;

	if (dh == NULL) {
		SSLerror(s, ERR_R_PASSED_NULL_PARAMETER);
		return 0;
	}

	if (!ssl_security_dh(s, dh)) {
		SSLerror(s, SSL_R_DH_KEY_TOO_SMALL);
		return 0;
	}

	if ((dhe_params = DHparams_dup(dh)) == NULL) {
		SSLerror(s, ERR_R_DH_LIB);
		return 0;
	}

	DH_free(s->cert->dhe_params);
	s->cert->dhe_params = dhe_params;

	return 1;
}

static int
_SSL_set_dh_auto(SSL *s, int state)
{
	s->cert->dhe_params_auto = state;
	return 1;
}

static int
_SSL_set_tmp_ecdh(SSL *s, EC_KEY *ecdh)
{
	const EC_GROUP *group;
	int nid;

	if (ecdh == NULL)
		return 0;
	if ((group = EC_KEY_get0_group(ecdh)) == NULL)
		return 0;

	nid = EC_GROUP_get_curve_name(group);
	return SSL_set1_groups(s, &nid, 1);
}

static int
_SSL_set_ecdh_auto(SSL *s, int state)
{
	return 1;
}

static int
_SSL_set_tlsext_host_name(SSL *s, const char *name)
{
	int is_ip;
	CBS cbs;

	free(s->tlsext_hostname);
	s->tlsext_hostname = NULL;

	if (name == NULL)
		return 1;

	CBS_init(&cbs, name, strlen(name));

	if (!tlsext_sni_is_valid_hostname(&cbs, &is_ip)) {
		SSLerror(s, SSL_R_SSL3_EXT_INVALID_SERVERNAME);
		return 0;
	}
	if ((s->tlsext_hostname = strdup(name)) == NULL) {
		SSLerror(s, ERR_R_INTERNAL_ERROR);
		return 0;
	}

	return 1;
}

static int
_SSL_set_tlsext_debug_arg(SSL *s, void *arg)
{
	s->tlsext_debug_arg = arg;
	return 1;
}

static int
_SSL_get_tlsext_status_type(SSL *s)
{
	return s->tlsext_status_type;
}

static int
_SSL_set_tlsext_status_type(SSL *s, int type)
{
	s->tlsext_status_type = type;
	return 1;
}

static int
_SSL_get_tlsext_status_exts(SSL *s, STACK_OF(X509_EXTENSION) **exts)
{
	*exts = s->tlsext_ocsp_exts;
	return 1;
}

static int
_SSL_set_tlsext_status_exts(SSL *s, STACK_OF(X509_EXTENSION) *exts)
{
	/* XXX - leak... */
	s->tlsext_ocsp_exts = exts;
	return 1;
}

static int
_SSL_get_tlsext_status_ids(SSL *s, STACK_OF(OCSP_RESPID) **ids)
{
	*ids = s->tlsext_ocsp_ids;
	return 1;
}

static int
_SSL_set_tlsext_status_ids(SSL *s, STACK_OF(OCSP_RESPID) *ids)
{
	/* XXX - leak... */
	s->tlsext_ocsp_ids = ids;
	return 1;
}

static int
_SSL_get_tlsext_status_ocsp_resp(SSL *s, unsigned char **resp)
{
	if (s->tlsext_ocsp_resp != NULL &&
	    s->tlsext_ocsp_resp_len < INT_MAX) {
		*resp = s->tlsext_ocsp_resp;
		return (int)s->tlsext_ocsp_resp_len;
	}

	*resp = NULL;

	return -1;
}

static int
_SSL_set_tlsext_status_ocsp_resp(SSL *s, unsigned char *resp, int resp_len)
{
	free(s->tlsext_ocsp_resp);
	s->tlsext_ocsp_resp = NULL;
	s->tlsext_ocsp_resp_len = 0;

	if (resp_len < 0)
		return 0;

	s->tlsext_ocsp_resp = resp;
	s->tlsext_ocsp_resp_len = (size_t)resp_len;

	return 1;
}

int
SSL_set0_chain(SSL *ssl, STACK_OF(X509) *chain)
{
	return ssl_cert_set0_chain(NULL, ssl, chain);
}
LSSL_ALIAS(SSL_set0_chain);

int
SSL_set1_chain(SSL *ssl, STACK_OF(X509) *chain)
{
	return ssl_cert_set1_chain(NULL, ssl, chain);
}
LSSL_ALIAS(SSL_set1_chain);

int
SSL_add0_chain_cert(SSL *ssl, X509 *x509)
{
	return ssl_cert_add0_chain_cert(NULL, ssl, x509);
}
LSSL_ALIAS(SSL_add0_chain_cert);

int
SSL_add1_chain_cert(SSL *ssl, X509 *x509)
{
	return ssl_cert_add1_chain_cert(NULL, ssl, x509);
}
LSSL_ALIAS(SSL_add1_chain_cert);

int
SSL_get0_chain_certs(const SSL *ssl, STACK_OF(X509) **out_chain)
{
	*out_chain = NULL;

	if (ssl->cert->key != NULL)
		*out_chain = ssl->cert->key->chain;

	return 1;
}
LSSL_ALIAS(SSL_get0_chain_certs);

int
SSL_clear_chain_certs(SSL *ssl)
{
	return ssl_cert_set0_chain(NULL, ssl, NULL);
}
LSSL_ALIAS(SSL_clear_chain_certs);

int
SSL_set1_groups(SSL *s, const int *groups, size_t groups_len)
{
	return tls1_set_groups(&s->tlsext_supportedgroups,
	    &s->tlsext_supportedgroups_length, groups, groups_len);
}
LSSL_ALIAS(SSL_set1_groups);

int
SSL_set1_groups_list(SSL *s, const char *groups)
{
	return tls1_set_group_list(&s->tlsext_supportedgroups,
	    &s->tlsext_supportedgroups_length, groups);
}
LSSL_ALIAS(SSL_set1_groups_list);

static int
_SSL_get_signature_nid(SSL *s, int *nid)
{
	const struct ssl_sigalg *sigalg;

	if ((sigalg = s->s3->hs.our_sigalg) == NULL)
		return 0;

	*nid = EVP_MD_type(sigalg->md());

	return 1;
}

static int
_SSL_get_peer_signature_nid(SSL *s, int *nid)
{
	const struct ssl_sigalg *sigalg;

	if ((sigalg = s->s3->hs.peer_sigalg) == NULL)
		return 0;

	*nid = EVP_MD_type(sigalg->md());

	return 1;
}

int
SSL_get_signature_type_nid(const SSL *s, int *nid)
{
	const struct ssl_sigalg *sigalg;

	if ((sigalg = s->s3->hs.our_sigalg) == NULL)
		return 0;

	*nid = sigalg->key_type;
	if (sigalg->key_type == EVP_PKEY_RSA &&
	    (sigalg->flags & SIGALG_FLAG_RSA_PSS))
		*nid = EVP_PKEY_RSA_PSS;

	return 1;
}
LSSL_ALIAS(SSL_get_signature_type_nid);

int
SSL_get_peer_signature_type_nid(const SSL *s, int *nid)
{
	const struct ssl_sigalg *sigalg;

	if ((sigalg = s->s3->hs.peer_sigalg) == NULL)
		return 0;

	*nid = sigalg->key_type;
	if (sigalg->key_type == EVP_PKEY_RSA &&
	    (sigalg->flags & SIGALG_FLAG_RSA_PSS))
		*nid = EVP_PKEY_RSA_PSS;

	return 1;
}
LSSL_ALIAS(SSL_get_peer_signature_type_nid);

long
ssl3_ctrl(SSL *s, int cmd, long larg, void *parg)
{
	switch (cmd) {
	case SSL_CTRL_GET_SESSION_REUSED:
		return _SSL_session_reused(s);

	case SSL_CTRL_GET_NUM_RENEGOTIATIONS:
		return _SSL_num_renegotiations(s);

	case SSL_CTRL_CLEAR_NUM_RENEGOTIATIONS:
		return _SSL_clear_num_renegotiations(s);

	case SSL_CTRL_GET_TOTAL_RENEGOTIATIONS:
		return _SSL_total_renegotiations(s);

	case SSL_CTRL_SET_TMP_DH:
		return _SSL_set_tmp_dh(s, parg);

	case SSL_CTRL_SET_TMP_DH_CB:
		SSLerror(s, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return 0;

	case SSL_CTRL_SET_DH_AUTO:
		return _SSL_set_dh_auto(s, larg);

	case SSL_CTRL_SET_TMP_ECDH:
		return _SSL_set_tmp_ecdh(s, parg);

	case SSL_CTRL_SET_TMP_ECDH_CB:
		SSLerror(s, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return 0;

	case SSL_CTRL_SET_ECDH_AUTO:
		return _SSL_set_ecdh_auto(s, larg);

	case SSL_CTRL_SET_TLSEXT_HOSTNAME:
		if (larg != TLSEXT_NAMETYPE_host_name) {
			SSLerror(s, SSL_R_SSL3_EXT_INVALID_SERVERNAME_TYPE);
			return 0;
		}
		return _SSL_set_tlsext_host_name(s, parg);

	case SSL_CTRL_SET_TLSEXT_DEBUG_ARG:
		return _SSL_set_tlsext_debug_arg(s, parg);

	case SSL_CTRL_GET_TLSEXT_STATUS_REQ_TYPE:
		return _SSL_get_tlsext_status_type(s);

	case SSL_CTRL_SET_TLSEXT_STATUS_REQ_TYPE:
		return _SSL_set_tlsext_status_type(s, larg);

	case SSL_CTRL_GET_TLSEXT_STATUS_REQ_EXTS:
		return _SSL_get_tlsext_status_exts(s, parg);

	case SSL_CTRL_SET_TLSEXT_STATUS_REQ_EXTS:
		return _SSL_set_tlsext_status_exts(s, parg);

	case SSL_CTRL_GET_TLSEXT_STATUS_REQ_IDS:
		return _SSL_get_tlsext_status_ids(s, parg);

	case SSL_CTRL_SET_TLSEXT_STATUS_REQ_IDS:
		return _SSL_set_tlsext_status_ids(s, parg);

	case SSL_CTRL_GET_TLSEXT_STATUS_REQ_OCSP_RESP:
		return _SSL_get_tlsext_status_ocsp_resp(s, parg);

	case SSL_CTRL_SET_TLSEXT_STATUS_REQ_OCSP_RESP:
		return _SSL_set_tlsext_status_ocsp_resp(s, parg, larg);

	case SSL_CTRL_CHAIN:
		if (larg == 0)
			return SSL_set0_chain(s, (STACK_OF(X509) *)parg);
		else
			return SSL_set1_chain(s, (STACK_OF(X509) *)parg);

	case SSL_CTRL_CHAIN_CERT:
		if (larg == 0)
			return SSL_add0_chain_cert(s, (X509 *)parg);
		else
			return SSL_add1_chain_cert(s, (X509 *)parg);

	case SSL_CTRL_GET_CHAIN_CERTS:
		return SSL_get0_chain_certs(s, (STACK_OF(X509) **)parg);

	case SSL_CTRL_SET_GROUPS:
		return SSL_set1_groups(s, parg, larg);

	case SSL_CTRL_SET_GROUPS_LIST:
		return SSL_set1_groups_list(s, parg);

	case SSL_CTRL_GET_SHARED_GROUP:
		return _SSL_get_shared_group(s, larg);

	/* XXX - rename to SSL_CTRL_GET_PEER_TMP_KEY and remove server check. */
	case SSL_CTRL_GET_SERVER_TMP_KEY:
		if (s->server != 0)
			return 0;
		return _SSL_get_peer_tmp_key(s, parg);

	case SSL_CTRL_GET_MIN_PROTO_VERSION:
		return SSL_get_min_proto_version(s);

	case SSL_CTRL_GET_MAX_PROTO_VERSION:
		return SSL_get_max_proto_version(s);

	case SSL_CTRL_SET_MIN_PROTO_VERSION:
		if (larg < 0 || larg > UINT16_MAX)
			return 0;
		return SSL_set_min_proto_version(s, larg);

	case SSL_CTRL_SET_MAX_PROTO_VERSION:
		if (larg < 0 || larg > UINT16_MAX)
			return 0;
		return SSL_set_max_proto_version(s, larg);

	case SSL_CTRL_GET_SIGNATURE_NID:
		return _SSL_get_signature_nid(s, parg);

	case SSL_CTRL_GET_PEER_SIGNATURE_NID:
		return _SSL_get_peer_signature_nid(s, parg);

	/*
	 * Legacy controls that should eventually be removed.
	 */
	case SSL_CTRL_GET_CLIENT_CERT_REQUEST:
		return 0;

	case SSL_CTRL_GET_FLAGS:
		return (int)(s->s3->flags);

	case SSL_CTRL_NEED_TMP_RSA:
		return 0;

	case SSL_CTRL_SET_TMP_RSA:
	case SSL_CTRL_SET_TMP_RSA_CB:
		SSLerror(s, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return 0;
	}

	return 0;
}

long
ssl3_callback_ctrl(SSL *s, int cmd, void (*fp)(void))
{
	switch (cmd) {
	case SSL_CTRL_SET_TMP_RSA_CB:
		SSLerror(s, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return 0;

	case SSL_CTRL_SET_TMP_DH_CB:
		s->cert->dhe_params_cb = (DH *(*)(SSL *, int, int))fp;
		return 1;

	case SSL_CTRL_SET_TMP_ECDH_CB:
		return 1;

	case SSL_CTRL_SET_TLSEXT_DEBUG_CB:
		s->tlsext_debug_cb = (void (*)(SSL *, int , int,
		    unsigned char *, int, void *))fp;
		return 1;
	}

	return 0;
}

static int
_SSL_CTX_set_tmp_dh(SSL_CTX *ctx, DH *dh)
{
	DH *dhe_params;

	if (dh == NULL) {
		SSLerrorx(ERR_R_PASSED_NULL_PARAMETER);
		return 0;
	}

	if (!ssl_ctx_security_dh(ctx, dh)) {
		SSLerrorx(SSL_R_DH_KEY_TOO_SMALL);
		return 0;
	}

	if ((dhe_params = DHparams_dup(dh)) == NULL) {
		SSLerrorx(ERR_R_DH_LIB);
		return 0;
	}

	DH_free(ctx->cert->dhe_params);
	ctx->cert->dhe_params = dhe_params;

	return 1;
}

static int
_SSL_CTX_set_dh_auto(SSL_CTX *ctx, int state)
{
	ctx->cert->dhe_params_auto = state;
	return 1;
}

static int
_SSL_CTX_set_tmp_ecdh(SSL_CTX *ctx, EC_KEY *ecdh)
{
	const EC_GROUP *group;
	int nid;

	if (ecdh == NULL)
		return 0;
	if ((group = EC_KEY_get0_group(ecdh)) == NULL)
		return 0;

	nid = EC_GROUP_get_curve_name(group);
	return SSL_CTX_set1_groups(ctx, &nid, 1);
}

static int
_SSL_CTX_set_ecdh_auto(SSL_CTX *ctx, int state)
{
	return 1;
}

static int
_SSL_CTX_set_tlsext_servername_arg(SSL_CTX *ctx, void *arg)
{
	ctx->tlsext_servername_arg = arg;
	return 1;
}

static int
_SSL_CTX_get_tlsext_ticket_keys(SSL_CTX *ctx, unsigned char *keys, int keys_len)
{
	if (keys == NULL)
		return 48;

	if (keys_len != 48) {
		SSLerrorx(SSL_R_INVALID_TICKET_KEYS_LENGTH);
		return 0;
	}

	memcpy(keys, ctx->tlsext_tick_key_name, 16);
	memcpy(keys + 16, ctx->tlsext_tick_hmac_key, 16);
	memcpy(keys + 32, ctx->tlsext_tick_aes_key, 16);

	return 1;
}

static int
_SSL_CTX_set_tlsext_ticket_keys(SSL_CTX *ctx, unsigned char *keys, int keys_len)
{
	if (keys == NULL)
		return 48;

	if (keys_len != 48) {
		SSLerrorx(SSL_R_INVALID_TICKET_KEYS_LENGTH);
		return 0;
	}

	memcpy(ctx->tlsext_tick_key_name, keys, 16);
	memcpy(ctx->tlsext_tick_hmac_key, keys + 16, 16);
	memcpy(ctx->tlsext_tick_aes_key, keys + 32, 16);

	return 1;
}

static int
_SSL_CTX_get_tlsext_status_arg(SSL_CTX *ctx, void **arg)
{
	*arg = ctx->tlsext_status_arg;
	return 1;
}

static int
_SSL_CTX_set_tlsext_status_arg(SSL_CTX *ctx, void *arg)
{
	ctx->tlsext_status_arg = arg;
	return 1;
}

int
SSL_CTX_set0_chain(SSL_CTX *ctx, STACK_OF(X509) *chain)
{
	return ssl_cert_set0_chain(ctx, NULL, chain);
}
LSSL_ALIAS(SSL_CTX_set0_chain);

int
SSL_CTX_set1_chain(SSL_CTX *ctx, STACK_OF(X509) *chain)
{
	return ssl_cert_set1_chain(ctx, NULL, chain);
}
LSSL_ALIAS(SSL_CTX_set1_chain);

int
SSL_CTX_add0_chain_cert(SSL_CTX *ctx, X509 *x509)
{
	return ssl_cert_add0_chain_cert(ctx, NULL, x509);
}
LSSL_ALIAS(SSL_CTX_add0_chain_cert);

int
SSL_CTX_add1_chain_cert(SSL_CTX *ctx, X509 *x509)
{
	return ssl_cert_add1_chain_cert(ctx, NULL, x509);
}
LSSL_ALIAS(SSL_CTX_add1_chain_cert);

int
SSL_CTX_get0_chain_certs(const SSL_CTX *ctx, STACK_OF(X509) **out_chain)
{
	*out_chain = NULL;

	if (ctx->cert->key != NULL)
		*out_chain = ctx->cert->key->chain;

	return 1;
}
LSSL_ALIAS(SSL_CTX_get0_chain_certs);

int
SSL_CTX_clear_chain_certs(SSL_CTX *ctx)
{
	return ssl_cert_set0_chain(ctx, NULL, NULL);
}
LSSL_ALIAS(SSL_CTX_clear_chain_certs);

static int
_SSL_CTX_add_extra_chain_cert(SSL_CTX *ctx, X509 *cert)
{
	if (ctx->extra_certs == NULL) {
		if ((ctx->extra_certs = sk_X509_new_null()) == NULL)
			return 0;
	}
	if (sk_X509_push(ctx->extra_certs, cert) == 0)
		return 0;

	return 1;
}

static int
_SSL_CTX_get_extra_chain_certs(SSL_CTX *ctx, STACK_OF(X509) **certs)
{
	*certs = ctx->extra_certs;
	if (*certs == NULL)
		*certs = ctx->cert->key->chain;

	return 1;
}

static int
_SSL_CTX_get_extra_chain_certs_only(SSL_CTX *ctx, STACK_OF(X509) **certs)
{
	*certs = ctx->extra_certs;
	return 1;
}

static int
_SSL_CTX_clear_extra_chain_certs(SSL_CTX *ctx)
{
	sk_X509_pop_free(ctx->extra_certs, X509_free);
	ctx->extra_certs = NULL;
	return 1;
}

int
SSL_CTX_set1_groups(SSL_CTX *ctx, const int *groups, size_t groups_len)
{
	return tls1_set_groups(&ctx->tlsext_supportedgroups,
	    &ctx->tlsext_supportedgroups_length, groups, groups_len);
}
LSSL_ALIAS(SSL_CTX_set1_groups);

int
SSL_CTX_set1_groups_list(SSL_CTX *ctx, const char *groups)
{
	return tls1_set_group_list(&ctx->tlsext_supportedgroups,
	    &ctx->tlsext_supportedgroups_length, groups);
}
LSSL_ALIAS(SSL_CTX_set1_groups_list);

long
ssl3_ctx_ctrl(SSL_CTX *ctx, int cmd, long larg, void *parg)
{
	switch (cmd) {
	case SSL_CTRL_SET_TMP_DH:
		return _SSL_CTX_set_tmp_dh(ctx, parg);

	case SSL_CTRL_SET_TMP_DH_CB:
		SSLerrorx(ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return 0;

	case SSL_CTRL_SET_DH_AUTO:
		return _SSL_CTX_set_dh_auto(ctx, larg);

	case SSL_CTRL_SET_TMP_ECDH:
		return _SSL_CTX_set_tmp_ecdh(ctx, parg);

	case SSL_CTRL_SET_TMP_ECDH_CB:
		SSLerrorx(ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return 0;

	case SSL_CTRL_SET_ECDH_AUTO:
		return _SSL_CTX_set_ecdh_auto(ctx, larg);

	case SSL_CTRL_SET_TLSEXT_SERVERNAME_ARG:
		return _SSL_CTX_set_tlsext_servername_arg(ctx, parg);

	case SSL_CTRL_GET_TLSEXT_TICKET_KEYS:
		return _SSL_CTX_get_tlsext_ticket_keys(ctx, parg, larg);

	case SSL_CTRL_SET_TLSEXT_TICKET_KEYS:
		return _SSL_CTX_set_tlsext_ticket_keys(ctx, parg, larg);

	case SSL_CTRL_GET_TLSEXT_STATUS_REQ_CB_ARG:
		return _SSL_CTX_get_tlsext_status_arg(ctx, parg);

	case SSL_CTRL_SET_TLSEXT_STATUS_REQ_CB_ARG:
		return _SSL_CTX_set_tlsext_status_arg(ctx, parg);

	case SSL_CTRL_CHAIN:
		if (larg == 0)
			return SSL_CTX_set0_chain(ctx, (STACK_OF(X509) *)parg);
		else
			return SSL_CTX_set1_chain(ctx, (STACK_OF(X509) *)parg);

	case SSL_CTRL_CHAIN_CERT:
		if (larg == 0)
			return SSL_CTX_add0_chain_cert(ctx, (X509 *)parg);
		else
			return SSL_CTX_add1_chain_cert(ctx, (X509 *)parg);

	case SSL_CTRL_GET_CHAIN_CERTS:
		return SSL_CTX_get0_chain_certs(ctx, (STACK_OF(X509) **)parg);

	case SSL_CTRL_EXTRA_CHAIN_CERT:
		return _SSL_CTX_add_extra_chain_cert(ctx, parg);

	case SSL_CTRL_GET_EXTRA_CHAIN_CERTS:
		if (larg == 0)
			return _SSL_CTX_get_extra_chain_certs(ctx, parg);
		else
			return _SSL_CTX_get_extra_chain_certs_only(ctx, parg);

	case SSL_CTRL_CLEAR_EXTRA_CHAIN_CERTS:
		return _SSL_CTX_clear_extra_chain_certs(ctx);

	case SSL_CTRL_SET_GROUPS:
		return SSL_CTX_set1_groups(ctx, parg, larg);

	case SSL_CTRL_SET_GROUPS_LIST:
		return SSL_CTX_set1_groups_list(ctx, parg);

	case SSL_CTRL_GET_MIN_PROTO_VERSION:
		return SSL_CTX_get_min_proto_version(ctx);

	case SSL_CTRL_GET_MAX_PROTO_VERSION:
		return SSL_CTX_get_max_proto_version(ctx);

	case SSL_CTRL_SET_MIN_PROTO_VERSION:
		if (larg < 0 || larg > UINT16_MAX)
			return 0;
		return SSL_CTX_set_min_proto_version(ctx, larg);

	case SSL_CTRL_SET_MAX_PROTO_VERSION:
		if (larg < 0 || larg > UINT16_MAX)
			return 0;
		return SSL_CTX_set_max_proto_version(ctx, larg);

	/*
	 * Legacy controls that should eventually be removed.
	 */
	case SSL_CTRL_NEED_TMP_RSA:
		return 0;

	case SSL_CTRL_SET_TMP_RSA:
	case SSL_CTRL_SET_TMP_RSA_CB:
		SSLerrorx(ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return 0;
	}

	return 0;
}

long
ssl3_ctx_callback_ctrl(SSL_CTX *ctx, int cmd, void (*fp)(void))
{
	switch (cmd) {
	case SSL_CTRL_SET_TMP_RSA_CB:
		SSLerrorx(ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
		return 0;

	case SSL_CTRL_SET_TMP_DH_CB:
		ctx->cert->dhe_params_cb =
		    (DH *(*)(SSL *, int, int))fp;
		return 1;

	case SSL_CTRL_SET_TMP_ECDH_CB:
		return 1;

	case SSL_CTRL_SET_TLSEXT_SERVERNAME_CB:
		ctx->tlsext_servername_callback =
		    (int (*)(SSL *, int *, void *))fp;
		return 1;

	case SSL_CTRL_GET_TLSEXT_STATUS_REQ_CB:
		*(int (**)(SSL *, void *))fp = ctx->tlsext_status_cb;
		return 1;

	case SSL_CTRL_SET_TLSEXT_STATUS_REQ_CB:
		ctx->tlsext_status_cb = (int (*)(SSL *, void *))fp;
		return 1;

	case SSL_CTRL_SET_TLSEXT_TICKET_KEY_CB:
		ctx->tlsext_ticket_key_cb = (int (*)(SSL *, unsigned char  *,
		    unsigned char *, EVP_CIPHER_CTX *, HMAC_CTX *, int))fp;
		return 1;
	}

	return 0;
}

SSL_CIPHER *
ssl3_choose_cipher(SSL *s, STACK_OF(SSL_CIPHER) *clnt,
    STACK_OF(SSL_CIPHER) *srvr)
{
	unsigned long alg_k, alg_a, mask_k, mask_a;
	STACK_OF(SSL_CIPHER) *prio, *allow;
	SSL_CIPHER *c, *ret = NULL;
	int can_use_ecc;
	int i, ii, nid, ok;
	SSL_CERT *cert;

	/* Let's see which ciphers we can support */
	cert = s->cert;

	can_use_ecc = tls1_get_supported_group(s, &nid);

	/*
	 * Do not set the compare functions, because this may lead to a
	 * reordering by "id". We want to keep the original ordering.
	 * We may pay a price in performance during sk_SSL_CIPHER_find(),
	 * but would have to pay with the price of sk_SSL_CIPHER_dup().
	 */

	if (s->options & SSL_OP_CIPHER_SERVER_PREFERENCE) {
		prio = srvr;
		allow = clnt;
	} else {
		prio = clnt;
		allow = srvr;
	}

	for (i = 0; i < sk_SSL_CIPHER_num(prio); i++) {
		c = sk_SSL_CIPHER_value(prio, i);

		/* Skip TLS v1.2 only ciphersuites if not supported. */
		if ((c->algorithm_ssl & SSL_TLSV1_2) &&
		    !SSL_USE_TLS1_2_CIPHERS(s))
			continue;

		/* Skip TLS v1.3 only ciphersuites if not supported. */
		if ((c->algorithm_ssl & SSL_TLSV1_3) &&
		    !SSL_USE_TLS1_3_CIPHERS(s))
			continue;

		/* If TLS v1.3, only allow TLS v1.3 ciphersuites. */
		if (SSL_USE_TLS1_3_CIPHERS(s) &&
		    !(c->algorithm_ssl & SSL_TLSV1_3))
			continue;

		if (!ssl_security_shared_cipher(s, c))
			continue;

		ssl_set_cert_masks(cert, c);
		mask_k = cert->mask_k;
		mask_a = cert->mask_a;

		alg_k = c->algorithm_mkey;
		alg_a = c->algorithm_auth;

		ok = (alg_k & mask_k) && (alg_a & mask_a);

		/*
		 * If we are considering an ECC cipher suite that uses our
		 * certificate check it.
		 */
		if (alg_a & SSL_aECDSA)
			ok = ok && tls1_check_ec_server_key(s);
		/*
		 * If we are considering an ECC cipher suite that uses
		 * an ephemeral EC key check it.
		 */
		if (alg_k & SSL_kECDHE)
			ok = ok && can_use_ecc;

		if (!ok)
			continue;
		ii = sk_SSL_CIPHER_find(allow, c);
		if (ii >= 0) {
			ret = sk_SSL_CIPHER_value(allow, ii);
			break;
		}
	}
	return (ret);
}

#define SSL3_CT_RSA_SIGN	1
#define SSL3_CT_RSA_FIXED_DH	3
#define SSL3_CT_ECDSA_SIGN	64

int
ssl3_get_req_cert_types(SSL *s, CBB *cbb)
{
	unsigned long alg_k;

	alg_k = s->s3->hs.cipher->algorithm_mkey;

	if ((alg_k & SSL_kDHE) != 0) {
		if (!CBB_add_u8(cbb, SSL3_CT_RSA_FIXED_DH))
			return 0;
	}

	if (!CBB_add_u8(cbb, SSL3_CT_RSA_SIGN))
		return 0;

	/*
	 * ECDSA certs can be used with RSA cipher suites as well
	 * so we don't need to check for SSL_kECDH or SSL_kECDHE.
	 */
	if (!CBB_add_u8(cbb, SSL3_CT_ECDSA_SIGN))
		return 0;

	return 1;
}

int
ssl3_shutdown(SSL *s)
{
	int	ret;

	/*
	 * Don't do anything much if we have not done the handshake or
	 * we don't want to send messages :-)
	 */
	if ((s->quiet_shutdown) || (s->s3->hs.state == SSL_ST_BEFORE)) {
		s->shutdown = (SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
		return (1);
	}

	if (!(s->shutdown & SSL_SENT_SHUTDOWN)) {
		s->shutdown|=SSL_SENT_SHUTDOWN;
		ssl3_send_alert(s, SSL3_AL_WARNING, SSL_AD_CLOSE_NOTIFY);
		/*
		 * Our shutdown alert has been sent now, and if it still needs
		 * to be written, s->s3->alert_dispatch will be true
		 */
		if (s->s3->alert_dispatch)
			return (-1);	/* return WANT_WRITE */
	} else if (s->s3->alert_dispatch) {
		/* resend it if not sent */
		ret = ssl3_dispatch_alert(s);
		if (ret == -1) {
			/*
			 * We only get to return -1 here the 2nd/Nth
			 * invocation, we must  have already signalled
			 * return 0 upon a previous invoation,
			 * return WANT_WRITE
			 */
			return (ret);
		}
	} else if (!(s->shutdown & SSL_RECEIVED_SHUTDOWN)) {
		/* If we are waiting for a close from our peer, we are closed */
		s->method->ssl_read_bytes(s, 0, NULL, 0, 0);
		if (!(s->shutdown & SSL_RECEIVED_SHUTDOWN)) {
			return (-1);	/* return WANT_READ */
		}
	}

	if ((s->shutdown == (SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN)) &&
	    !s->s3->alert_dispatch)
		return (1);
	else
		return (0);
}

int
ssl3_write(SSL *s, const void *buf, int len)
{
	errno = 0;

	if (s->s3->renegotiate)
		ssl3_renegotiate_check(s);

	return s->method->ssl_write_bytes(s, SSL3_RT_APPLICATION_DATA,
	    buf, len);
}

static int
ssl3_read_internal(SSL *s, void *buf, int len, int peek)
{
	int	ret;

	errno = 0;
	if (s->s3->renegotiate)
		ssl3_renegotiate_check(s);
	s->s3->in_read_app_data = 1;

	ret = s->method->ssl_read_bytes(s, SSL3_RT_APPLICATION_DATA, buf, len,
	    peek);
	if ((ret == -1) && (s->s3->in_read_app_data == 2)) {
		/*
		 * ssl3_read_bytes decided to call s->handshake_func,
		 * which called ssl3_read_bytes to read handshake data.
		 * However, ssl3_read_bytes actually found application data
		 * and thinks that application data makes sense here; so disable
		 * handshake processing and try to read application data again.
		 */
		s->in_handshake++;
		ret = s->method->ssl_read_bytes(s, SSL3_RT_APPLICATION_DATA,
		    buf, len, peek);
		s->in_handshake--;
	} else
		s->s3->in_read_app_data = 0;

	return (ret);
}

int
ssl3_read(SSL *s, void *buf, int len)
{
	return ssl3_read_internal(s, buf, len, 0);
}

int
ssl3_peek(SSL *s, void *buf, int len)
{
	return ssl3_read_internal(s, buf, len, 1);
}

int
ssl3_renegotiate(SSL *s)
{
	if (s->handshake_func == NULL)
		return 1;

	if (s->s3->flags & SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS)
		return 0;

	s->s3->renegotiate = 1;

	return 1;
}

int
ssl3_renegotiate_check(SSL *s)
{
	if (!s->s3->renegotiate)
		return 0;
	if (SSL_in_init(s) || s->s3->rbuf.left != 0 || s->s3->wbuf.left != 0)
		return 0;

	s->s3->hs.state = SSL_ST_RENEGOTIATE;
	s->s3->renegotiate = 0;
	s->s3->num_renegotiations++;
	s->s3->total_renegotiations++;

	return 1;
}

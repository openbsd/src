/*	$OpenBSD: evp_names.c,v 1.1 2024/01/13 10:57:08 tb Exp $ */
/*
 * Copyright (c) 2023 Theo Buehler <tb@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <openssl/evp.h>
#include <openssl/objects.h>

/*
 * The .name is the lookup name used by EVP_get_cipherbyname() while .alias
 * keeps track of the aliased name.
 */

struct cipher_name {
	const char *name;
	const EVP_CIPHER *(*cipher)(void);
	const char *alias;
};

/*
 * Keep this table alphabetically sorted by increasing .name.
 * regress/lib/libcrypto/evp/evp_test.c checks that.
 */

const struct cipher_name cipher_names[] = {
#ifndef OPENSSL_NO_AES
	{
		.name = SN_aes_128_cbc,
		.cipher = EVP_aes_128_cbc,
	},
	{
		.name = SN_aes_128_cbc_hmac_sha1,
		.cipher = EVP_aes_128_cbc_hmac_sha1,
	},
	{
		.name = SN_aes_128_cfb128,
		.cipher = EVP_aes_128_cfb128,
	},
	{
		.name = SN_aes_128_cfb1,
		.cipher = EVP_aes_128_cfb1,
	},
	{
		.name = SN_aes_128_cfb8,
		.cipher = EVP_aes_128_cfb8,
	},
	{
		.name = SN_aes_128_ctr,
		.cipher = EVP_aes_128_ctr,
	},
	{
		.name = SN_aes_128_ecb,
		.cipher = EVP_aes_128_ecb,
	},
	{
		.name = SN_aes_128_ofb128,
		.cipher = EVP_aes_128_ofb,
	},
	{
		.name = SN_aes_128_xts,
		.cipher = EVP_aes_128_xts,
	},

	{
		.name = SN_aes_192_cbc,
		.cipher = EVP_aes_192_cbc,
	},
	{
		.name = SN_aes_192_cfb128,
		.cipher = EVP_aes_192_cfb128,
	},
	{
		.name = SN_aes_192_cfb1,
		.cipher = EVP_aes_192_cfb1,
	},
	{
		.name = SN_aes_192_cfb8,
		.cipher = EVP_aes_192_cfb8,
	},
	{
		.name = SN_aes_192_ctr,
		.cipher = EVP_aes_192_ctr,
	},
	{
		.name = SN_aes_192_ecb,
		.cipher = EVP_aes_192_ecb,
	},
	{
		.name = SN_aes_192_ofb128,
		.cipher = EVP_aes_192_ofb,
	},

	{
		.name = SN_aes_256_cbc,
		.cipher = EVP_aes_256_cbc,
	},
	{
		.name = SN_aes_256_cbc_hmac_sha1,
		.cipher = EVP_aes_256_cbc_hmac_sha1,
	},
	{
		.name = SN_aes_256_cfb128,
		.cipher = EVP_aes_256_cfb128,
	},
	{
		.name = SN_aes_256_cfb1,
		.cipher = EVP_aes_256_cfb1,
	},
	{
		.name = SN_aes_256_cfb8,
		.cipher = EVP_aes_256_cfb8,
	},
	{
		.name = SN_aes_256_ctr,
		.cipher = EVP_aes_256_ctr,
	},
	{
		.name = SN_aes_256_ecb,
		.cipher = EVP_aes_256_ecb,
	},
	{
		.name = SN_aes_256_ofb128,
		.cipher = EVP_aes_256_ofb,
	},
	{
		.name = SN_aes_256_xts,
		.cipher = EVP_aes_256_xts,
	},

	{
		.name = "AES128",
		.cipher = EVP_aes_128_cbc,
		.alias = SN_aes_128_cbc,
	},
	{
		.name = "AES192",
		.cipher = EVP_aes_192_cbc,
		.alias = SN_aes_192_cbc,
	},
	{
		.name = "AES256",
		.cipher = EVP_aes_256_cbc,
		.alias = SN_aes_256_cbc,
	},
#endif /* OPENSSL_NO_AES */

#ifndef OPENSSL_NO_BF
	{
		.name = "BF",
		.cipher = EVP_bf_cbc,
		.alias = SN_bf_cbc,
	},

	{
		.name = SN_bf_cbc,
		.cipher = EVP_bf_cbc,
	},
	{
		.name = SN_bf_cfb64,
		.cipher = EVP_bf_cfb64,
	},
	{
		.name = SN_bf_ecb,
		.cipher = EVP_bf_ecb,
	},
	{
		.name = SN_bf_ofb64,
		.cipher = EVP_bf_ofb,
	},
#endif

#ifndef OPENSSL_NO_CAMELLIA
	{
		.name = SN_camellia_128_cbc,
		.cipher = EVP_camellia_128_cbc,
	},
	{
		.name = SN_camellia_128_cfb128,
		.cipher = EVP_camellia_128_cfb128,
	},
	{
		.name = SN_camellia_128_cfb1,
		.cipher = EVP_camellia_128_cfb1,
	},
	{
		.name = SN_camellia_128_cfb8,
		.cipher = EVP_camellia_128_cfb8,
	},
	{
		.name = SN_camellia_128_ecb,
		.cipher = EVP_camellia_128_ecb,
	},
	{
		.name = SN_camellia_128_ofb128,
		.cipher = EVP_camellia_128_ofb,
	},

	{
		.name = SN_camellia_192_cbc,
		.cipher = EVP_camellia_192_cbc,
	},
	{
		.name = SN_camellia_192_cfb128,
		.cipher = EVP_camellia_192_cfb128,
	},
	{
		.name = SN_camellia_192_cfb1,
		.cipher = EVP_camellia_192_cfb1,
	},
	{
		.name = SN_camellia_192_cfb8,
		.cipher = EVP_camellia_192_cfb8,
	},
	{
		.name = SN_camellia_192_ecb,
		.cipher = EVP_camellia_192_ecb,
	},
	{
		.name = SN_camellia_192_ofb128,
		.cipher = EVP_camellia_192_ofb,
	},

	{
		.name = SN_camellia_256_cbc,
		.cipher = EVP_camellia_256_cbc,
	},
	{
		.name = SN_camellia_256_cfb128,
		.cipher = EVP_camellia_256_cfb128,
	},
	{
		.name = SN_camellia_256_cfb1,
		.cipher = EVP_camellia_256_cfb1,
	},
	{
		.name = SN_camellia_256_cfb8,
		.cipher = EVP_camellia_256_cfb8,
	},
	{
		.name = SN_camellia_256_ecb,
		.cipher = EVP_camellia_256_ecb,
	},
	{
		.name = SN_camellia_256_ofb128,
		.cipher = EVP_camellia_256_ofb,
	},

	{
		.name = "CAMELLIA128",
		.cipher = EVP_camellia_128_cbc,
		.alias = SN_camellia_128_cbc,
	},
	{
		.name = "CAMELLIA192",
		.cipher = EVP_camellia_192_cbc,
		.alias = SN_camellia_192_cbc,
	},
	{
		.name = "CAMELLIA256",
		.cipher = EVP_camellia_256_cbc,
		.alias = SN_camellia_256_cbc,
	},
#endif /* OPENSSL_NO_CAMELLIA */

#ifndef OPENSSL_NO_CAST
	{
		.name = "CAST",
		.cipher = EVP_cast5_cbc,
		.alias = SN_cast5_cbc,
	},
	{
		.name = "CAST-cbc",
		.cipher = EVP_cast5_cbc,
		.alias = SN_cast5_cbc,
	},

	{
		.name = SN_cast5_cbc,
		.cipher = EVP_cast5_cbc,
	},
	{
		.name = SN_cast5_cfb64,
		.cipher = EVP_cast5_cfb,
	},
	{
		.name = SN_cast5_ecb,
		.cipher = EVP_cast5_ecb,
	},
	{
		.name = SN_cast5_ofb64,
		.cipher = EVP_cast5_ofb,
	},
#endif

#ifndef OPENSSL_NO_CHACHA
	{
		.name = SN_chacha20,
		.cipher = EVP_chacha20,
	},
	{
		.name = "ChaCha20",
		.cipher = EVP_chacha20,
		.alias = SN_chacha20,
	},
#endif /* OPENSSL_NO_CHACHA */

#if !defined(OPENSSL_NO_CHACHA) && !defined(OPENSSL_NO_POLY1305)
	{
		.name = SN_chacha20_poly1305,
		.cipher = EVP_chacha20_poly1305,
	},
#endif /* OPENSSL_NO_CHACHA && OPENSSL_NO_POLY1305 */

#ifndef OPENSSL_NO_DES
	{
		.name = "DES",
		.cipher = EVP_des_cbc,
		.alias = SN_des_cbc,
	},

	{
		.name = SN_des_cbc,
		.cipher = EVP_des_cbc,
	},
	{
		.name = SN_des_cfb64,
		.cipher = EVP_des_cfb64,
	},
	{
		.name = SN_des_cfb1,
		.cipher = EVP_des_cfb1,
	},
	{
		.name = SN_des_cfb8,
		.cipher = EVP_des_cfb8,
	},
	{
		.name = SN_des_ecb,
		.cipher = EVP_des_ecb,
	},
	{
		.name = SN_des_ede_ecb,
		.cipher = EVP_des_ede,
	},
	{
		.name = SN_des_ede_cbc,
		.cipher = EVP_des_ede_cbc,
	},
	{
		.name = SN_des_ede_cfb64,
		.cipher = EVP_des_ede_cfb64,
	},
	{
		.name = SN_des_ede_ofb64,
		.cipher = EVP_des_ede_ofb,
	},
	{
		.name = SN_des_ede3_ecb,
		.cipher = EVP_des_ede3_ecb,
	},
	{
		.name = SN_des_ede3_cbc,
		.cipher = EVP_des_ede3_cbc,
	},
	{
		.name = SN_des_ede3_cfb64,
		.cipher = EVP_des_ede3_cfb,
	},
	{
		.name = SN_des_ede3_cfb1,
		.cipher = EVP_des_ede3_cfb1,
	},
	{
		.name = SN_des_ede3_cfb8,
		.cipher = EVP_des_ede3_cfb8,
	},
	{
		.name = SN_des_ede3_ofb64,
		.cipher = EVP_des_ede3_ofb,
	},
	{
		.name = SN_des_ofb64,
		.cipher = EVP_des_ofb,
	},

	{
		.name = "DES3",
		.cipher = EVP_des_ede3_cbc,
		.alias = SN_des_ede3_cbc,
	},

	{
		.name = "DESX",
		.cipher = EVP_desx_cbc,
		.alias = SN_desx_cbc,
	},
	{
		.name = SN_desx_cbc,
		.cipher = EVP_desx_cbc,
	},
#endif /* OPENSSL_NO_DES */

#ifndef OPENSSL_NO_GOST
	{
		.name = LN_id_Gost28147_89,
		.cipher = EVP_gost2814789_cfb64,
	},
#endif /* OPENSSL_NO_GOST */

#ifndef OPENSSL_NO_IDEA
	{
		.name = "IDEA",
		.cipher = EVP_idea_cbc,
		.alias = SN_idea_cbc,
	},

	{
		.name = SN_idea_cbc,
		.cipher = EVP_idea_cbc,
	},
	{
		.name = SN_idea_cfb64,
		.cipher = EVP_idea_cfb64,
	},
	{
		.name = SN_idea_ecb,
		.cipher = EVP_idea_ecb,
	},
	{
		.name = SN_idea_ofb64,
		.cipher = EVP_idea_ofb,
	},
#endif /* OPENSSL_NO_IDEA */

#ifndef OPENSSL_NO_RC2
	{
		.name = "RC2",
		.cipher = EVP_rc2_cbc,
		.alias = SN_rc2_cbc,
	},

	{
		.name = SN_rc2_40_cbc,
		.cipher = EVP_rc2_40_cbc,
	},
	{
		.name = SN_rc2_64_cbc,
		.cipher = EVP_rc2_64_cbc,
	},
	{
		.name = SN_rc2_cbc,
		.cipher = EVP_rc2_cbc,
	},
	{
		.name = SN_rc2_cfb64,
		.cipher = EVP_rc2_cfb64,
	},
	{
		.name = SN_rc2_ecb,
		.cipher = EVP_rc2_ecb,
	},
	{
		.name = SN_rc2_ofb64,
		.cipher = EVP_rc2_ofb,
	},
#endif /* OPENSSL_NO_RC2 */

#ifndef OPENSSL_NO_RC4
	{
		.name = SN_rc4,
		.cipher = EVP_rc4,
	},
	{
		.name = SN_rc4_40,
		.cipher = EVP_rc4_40,
	},
	{
		.name = SN_rc4_hmac_md5,
		.cipher = EVP_rc4_hmac_md5,
	},
#endif /* OPENSSL_NO_RC4 */

#ifndef OPENSSL_NO_SM4
	{
		.name = "SM4",
		.cipher = EVP_sm4_cbc,
		.alias = SN_sm4_cbc,
	},

	{
		.name = SN_sm4_cbc,
		.cipher = EVP_sm4_cbc,
	},
	{
		.name = SN_sm4_cfb128,
		.cipher = EVP_sm4_cfb128,
	},
	{
		.name = SN_sm4_ctr,
		.cipher = EVP_sm4_ctr,
	},
	{
		.name = SN_sm4_ecb,
		.cipher = EVP_sm4_ecb,
	},
	{
		.name = SN_sm4_ofb128,
		.cipher = EVP_sm4_ofb,
	},
#endif /* OPENSSL_NO_SM4 */

#ifndef OPENSSL_NO_AES
	{
		.name = LN_aes_128_cbc,
		.cipher = EVP_aes_128_cbc,
	},
	{
		.name = LN_aes_128_cbc_hmac_sha1,
		.cipher = EVP_aes_128_cbc_hmac_sha1,
	},
	{
		.name = LN_aes_128_ccm,
		.cipher = EVP_aes_128_ccm,
	},
	{
		.name = LN_aes_128_cfb128,
		.cipher = EVP_aes_128_cfb128,
	},
	{
		.name = LN_aes_128_cfb1,
		.cipher = EVP_aes_128_cfb1,
	},
	{
		.name = LN_aes_128_cfb8,
		.cipher = EVP_aes_128_cfb8,
	},
	{
		.name = LN_aes_128_ctr,
		.cipher = EVP_aes_128_ctr,
	},
	{
		.name = LN_aes_128_ecb,
		.cipher = EVP_aes_128_ecb,
	},
	{
		.name = LN_aes_128_gcm,
		.cipher = EVP_aes_128_gcm,
	},
	{
		.name = LN_aes_128_ofb128,
		.cipher = EVP_aes_128_ofb,
	},
	{
		.name = LN_aes_128_xts,
		.cipher = EVP_aes_128_xts,
	},

	{
		.name = LN_aes_192_cbc,
		.cipher = EVP_aes_192_cbc,
	},
	{
		.name = LN_aes_192_ccm,
		.cipher = EVP_aes_192_ccm,
	},
	{
		.name = LN_aes_192_cfb128,
		.cipher = EVP_aes_192_cfb128,
	},
	{
		.name = LN_aes_192_cfb1,
		.cipher = EVP_aes_192_cfb1,
	},
	{
		.name = LN_aes_192_cfb8,
		.cipher = EVP_aes_192_cfb8,
	},
	{
		.name = LN_aes_192_ctr,
		.cipher = EVP_aes_192_ctr,
	},
	{
		.name = LN_aes_192_ecb,
		.cipher = EVP_aes_192_ecb,
	},
	{
		.name = LN_aes_192_gcm,
		.cipher = EVP_aes_192_gcm,
	},
	{
		.name = LN_aes_192_ofb128,
		.cipher = EVP_aes_192_ofb,
	},

	{
		.name = LN_aes_256_cbc,
		.cipher = EVP_aes_256_cbc,
	},
	{
		.name = LN_aes_256_cbc_hmac_sha1,
		.cipher = EVP_aes_256_cbc_hmac_sha1,
	},
	{
		.name = LN_aes_256_ccm,
		.cipher = EVP_aes_256_ccm,
	},
	{
		.name = LN_aes_256_cfb128,
		.cipher = EVP_aes_256_cfb128,
	},
	{
		.name = LN_aes_256_cfb1,
		.cipher = EVP_aes_256_cfb1,
	},
	{
		.name = LN_aes_256_cfb8,
		.cipher = EVP_aes_256_cfb8,
	},
	{
		.name = LN_aes_256_ctr,
		.cipher = EVP_aes_256_ctr,
	},
	{
		.name = LN_aes_256_ecb,
		.cipher = EVP_aes_256_ecb,
	},
	{
		.name = LN_aes_256_gcm,
		.cipher = EVP_aes_256_gcm,
	},
	{
		.name = LN_aes_256_ofb128,
		.cipher = EVP_aes_256_ofb,
	},
	{
		.name = LN_aes_256_xts,
		.cipher = EVP_aes_256_xts,
	},

	{
		.name = "aes128",
		.cipher = EVP_aes_128_cbc,
		.alias = SN_aes_128_cbc,
	},
	{
		.name = "aes192",
		.cipher = EVP_aes_192_cbc,
		.alias = SN_aes_192_cbc,
	},
	{
		.name = "aes256",
		.cipher = EVP_aes_256_cbc,
		.alias = SN_aes_256_cbc,
	},
#endif /* OPENSSL_NO_AES */

#ifndef OPENSSL_NO_BF
	{
		.name = "bf",
		.cipher = EVP_bf_cbc,
		.alias = SN_bf_cbc,
	},

	{
		.name = LN_bf_cbc,
		.cipher = EVP_bf_cbc,
	},
	{
		.name = LN_bf_cfb64,
		.cipher = EVP_bf_cfb64,
	},
	{
		.name = LN_bf_ecb,
		.cipher = EVP_bf_ecb,
	},
	{
		.name = LN_bf_ofb64,
		.cipher = EVP_bf_ofb,
	},

	{
		.name = "blowfish",
		.cipher = EVP_bf_cbc,
		.alias = SN_bf_cbc,
	},
#endif /* OPENSSL_NO_BF */

#ifndef OPENSSL_NO_CAMELLIA
	{
		.name = LN_camellia_128_cbc,
		.cipher = EVP_camellia_128_cbc,
	},
	{
		.name = LN_camellia_128_cfb128,
		.cipher = EVP_camellia_128_cfb128,
	},
	{
		.name = LN_camellia_128_cfb1,
		.cipher = EVP_camellia_128_cfb1,
	},
	{
		.name = LN_camellia_128_cfb8,
		.cipher = EVP_camellia_128_cfb8,
	},
	{
		.name = LN_camellia_128_ecb,
		.cipher = EVP_camellia_128_ecb,
	},
	{
		.name = LN_camellia_128_ofb128,
		.cipher = EVP_camellia_128_ofb,
	},

	{
		.name = LN_camellia_192_cbc,
		.cipher = EVP_camellia_192_cbc,
	},
	{
		.name = LN_camellia_192_cfb128,
		.cipher = EVP_camellia_192_cfb128,
	},
	{
		.name = LN_camellia_192_cfb1,
		.cipher = EVP_camellia_192_cfb1,
	},
	{
		.name = LN_camellia_192_cfb8,
		.cipher = EVP_camellia_192_cfb8,
	},
	{
		.name = LN_camellia_192_ecb,
		.cipher = EVP_camellia_192_ecb,
	},
	{
		.name = LN_camellia_192_ofb128,
		.cipher = EVP_camellia_192_ofb,
	},

	{
		.name = LN_camellia_256_cbc,
		.cipher = EVP_camellia_256_cbc,
	},
	{
		.name = LN_camellia_256_cfb128,
		.cipher = EVP_camellia_256_cfb128,
	},
	{
		.name = LN_camellia_256_cfb1,
		.cipher = EVP_camellia_256_cfb1,
	},
	{
		.name = LN_camellia_256_cfb8,
		.cipher = EVP_camellia_256_cfb8,
	},
	{
		.name = LN_camellia_256_ecb,
		.cipher = EVP_camellia_256_ecb,
	},
	{
		.name = LN_camellia_256_ofb128,
		.cipher = EVP_camellia_256_ofb,
	},

	{
		.name = "camellia128",
		.cipher = EVP_camellia_128_cbc,
		.alias = SN_camellia_128_cbc,
	},
	{
		.name = "camellia192",
		.cipher = EVP_camellia_192_cbc,
		.alias = SN_camellia_192_cbc,
	},
	{
		.name = "camellia256",
		.cipher = EVP_camellia_256_cbc,
		.alias = SN_camellia_256_cbc,
	},
#endif /* OPENSSL_NO_CAMELLIA */

#ifndef OPENSSL_NO_CAST
	{
		.name = "cast",
		.cipher = EVP_cast5_cbc,
		.alias = SN_cast5_cbc,
	},
	{
		.name = "cast-cbc",
		.cipher = EVP_cast5_cbc,
		.alias = SN_cast5_cbc,
	},

	{
		.name = LN_cast5_cbc,
		.cipher = EVP_cast5_cbc,
	},
	{
		.name = LN_cast5_cfb64,
		.cipher = EVP_cast5_cfb,
	},
	{
		.name = LN_cast5_ecb,
		.cipher = EVP_cast5_ecb,
	},
	{
		.name = LN_cast5_ofb64,
		.cipher = EVP_cast5_ofb,
	},
#endif

#ifndef OPENSSL_NO_CHACHA
	{
		.name = LN_chacha20,
		.cipher = EVP_chacha20,
	},
	{
		.name = "chacha20",
		.cipher = EVP_chacha20,
		.alias = LN_chacha20,
	},

	{
		.name = LN_chacha20_poly1305,
		.cipher = EVP_chacha20_poly1305,
	},
#endif

#ifndef OPENSSL_NO_DES
	{
		.name = "des",
		.cipher = EVP_des_cbc,
		.alias = SN_des_cbc,
	},

	{
		.name = LN_des_cbc,
		.cipher = EVP_des_cbc,
	},
	{
		.name = LN_des_cfb64,
		.cipher = EVP_des_cfb64,
	},
	{
		.name = LN_des_cfb1,
		.cipher = EVP_des_cfb1,
	},
	{
		.name = LN_des_cfb8,
		.cipher = EVP_des_cfb8,
	},
	{
		.name = LN_des_ecb,
		.cipher = EVP_des_ecb,
	},
	{
		.name = LN_des_ede_ecb,
		.cipher = EVP_des_ede,
	},
	{
		.name = LN_des_ede_cbc,
		.cipher = EVP_des_ede_cbc,
	},
	{
		.name = LN_des_ede_cfb64,
		.cipher = EVP_des_ede_cfb64,
	},
	{
		.name = LN_des_ede_ofb64,
		.cipher = EVP_des_ede_ofb,
	},
	{
		.name = LN_des_ede3_ecb,
		.cipher = EVP_des_ede3_ecb,
	},
	{
		.name = LN_des_ede3_cbc,
		.cipher = EVP_des_ede3_cbc,
	},
	{
		.name = LN_des_ede3_cfb64,
		.cipher = EVP_des_ede3_cfb,
	},
	{
		.name = LN_des_ede3_cfb1,
		.cipher = EVP_des_ede3_cfb1,
	},
	{
		.name = LN_des_ede3_cfb8,
		.cipher = EVP_des_ede3_cfb8,
	},
	{
		.name = LN_des_ede3_ofb64,
		.cipher = EVP_des_ede3_ofb,
	},
	{
		.name = LN_des_ofb64,
		.cipher = EVP_des_ofb,
	},

	{
		.name = "des3",
		.cipher = EVP_des_ede3_cbc,
		.alias = SN_des_ede3_cbc,
	},

	{
		.name = "desx",
		.cipher = EVP_desx_cbc,
		.alias = SN_desx_cbc,
	},
	{
		.name = LN_desx_cbc,
		.cipher = EVP_desx_cbc,
	},
#endif /* OPENSSL_NO_DES */

#ifndef OPENSSL_NO_GOST
	{
		.name = SN_id_Gost28147_89,
		.cipher = EVP_gost2814789_cfb64,
	},
	{
		.name = SN_gost89_cnt,
		.cipher = EVP_gost2814789_cnt,
	},
	{
		.name = SN_gost89_ecb,
		.cipher = EVP_gost2814789_ecb,
	},
#endif /* OPENSSL_NO_GOST */

#ifndef OPENSSL_NO_AES
	{
		.name = SN_aes_128_ccm,
		.cipher = EVP_aes_128_ccm,
	},
	{
		.name = SN_aes_128_gcm,
		.cipher = EVP_aes_128_gcm,
	},
	{
		.name = SN_id_aes128_wrap,
		.cipher = EVP_aes_128_wrap,
	},

	{
		.name = SN_aes_192_ccm,
		.cipher = EVP_aes_192_ccm,
	},
	{
		.name = SN_aes_192_gcm,
		.cipher = EVP_aes_192_gcm,
	},
	{
		.name = SN_id_aes192_wrap,
		.cipher = EVP_aes_192_wrap,
	},

	{
		.name = SN_aes_256_ccm,
		.cipher = EVP_aes_256_ccm,
	},
	{
		.name = SN_aes_256_gcm,
		.cipher = EVP_aes_256_gcm,
	},
	{
		.name = SN_id_aes256_wrap,
		.cipher = EVP_aes_256_wrap,
	},
#endif /* OPENSSL_NO_AES */

#ifndef OPENSSL_NO_IDEA
	{
		.name = "idea",
		.cipher = EVP_idea_cbc,
		.alias = SN_idea_cbc,
	},

	{
		.name = LN_idea_cbc,
		.cipher = EVP_idea_cbc,
	},
	{
		.name = LN_idea_cfb64,
		.cipher = EVP_idea_cfb64,
	},
	{
		.name = LN_idea_ecb,
		.cipher = EVP_idea_ecb,
	},
	{
		.name = LN_idea_ofb64,
		.cipher = EVP_idea_ofb,
	},
#endif /* OPENSSL_NO_IDEA */

#ifndef OPENSSL_NO_RC2
	{
		.name = "rc2",
		.cipher = EVP_rc2_cbc,
		.alias = SN_rc2_cbc,
	},

	{
		.name = LN_rc2_40_cbc,
		.cipher = EVP_rc2_40_cbc,
	},
	{
		.name = LN_rc2_64_cbc,
		.cipher = EVP_rc2_64_cbc,
	},
	{
		.name = LN_rc2_cbc,
		.cipher = EVP_rc2_cbc,
	},
	{
		.name = LN_rc2_cfb64,
		.cipher = EVP_rc2_cfb64,
	},
	{
		.name = LN_rc2_ecb,
		.cipher = EVP_rc2_ecb,
	},
	{
		.name = LN_rc2_ofb64,
		.cipher = EVP_rc2_ofb,
	},
#endif /* OPENSSL_NO_RC2 */

#ifndef OPENSSL_NO_RC4
	{
		.name = LN_rc4,
		.cipher = EVP_rc4,
	},
	{
		.name = LN_rc4_40,
		.cipher = EVP_rc4_40,
	},
	{
		.name = LN_rc4_hmac_md5,
		.cipher = EVP_rc4_hmac_md5,
	},
#endif /* OPENSSL_NO_RC4 */

#ifndef OPENSSL_NO_SM4
	{
		.name = "sm4",
		.cipher = EVP_sm4_cbc,
		.alias = SN_sm4_cbc,
	},

	{
		.name = LN_sm4_cbc,
		.cipher = EVP_sm4_cbc,
	},
	{
		.name = LN_sm4_cfb128,
		.cipher = EVP_sm4_cfb128,
	},
	{
		.name = LN_sm4_ctr,
		.cipher = EVP_sm4_ctr,
	},
	{
		.name = LN_sm4_ecb,
		.cipher = EVP_sm4_ecb,
	},
	{
		.name = LN_sm4_ofb128,
		.cipher = EVP_sm4_ofb,
	},
#endif /* OPENSSL_NO_SM4 */
};

#define N_CIPHER_NAMES (sizeof(cipher_names) / sizeof(cipher_names[0]))

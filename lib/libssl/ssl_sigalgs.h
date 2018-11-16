/* $OpenBSD: ssl_sigalgs.h,v 1.8 2018/11/16 02:41:16 beck Exp $ */
/*
 * Copyright (c) 2018, Bob Beck <beck@openbsd.org>
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

#ifndef HEADER_SSL_SIGALGS_H
#define HEADER_SSL_SIGALGS_H

__BEGIN_HIDDEN_DECLS

#define SIGALG_NONE			0x0000

/*
 * RFC 8446 Section 4.2.3
 * RFC 5246 Section 7.4.1.4.1
 */
#define SIGALG_RSA_PKCS1_SHA224		0x0301
#define SIGALG_RSA_PKCS1_SHA256		0x0401
#define SIGALG_RSA_PKCS1_SHA384		0x0501
#define SIGALG_RSA_PKCS1_SHA512		0x0601
#define SIGALG_ECDSA_SECP224R1_SHA224	0x0303
#define SIGALG_ECDSA_SECP256R1_SHA256	0x0403
#define SIGALG_ECDSA_SECP384R1_SHA384	0x0503
#define SIGALG_ECDSA_SECP512R1_SHA512	0x0603
#define SIGALG_RSA_PSS_RSAE_SHA256	0x0804
#define SIGALG_RSA_PSS_RSAE_SHA384	0x0805
#define SIGALG_RSA_PSS_RSAE_SHA512	0x0806
#define SIGALG_ED25519			0x0807
#define SIGALG_ED448			0x0808
#define SIGALG_RSA_PSS_PSS_SHA256	0x0809
#define SIGALG_RSA_PSS_PSS_SHA384	0x080a
#define SIGALG_RSA_PSS_PSS_SHA512	0x080b
#define SIGALG_RSA_PKCS1_SHA1		0x0201
#define SIGALG_ECDSA_SHA1		0x0203
#define SIGALG_PRIVATE_START		0xFE00
#define SIGALG_PRIVATE_END		0xFFFF

/*
 * If Russia can elect the US President, surely
 * IANA could fix this problem.
 */
#define SIGALG_GOSTR12_512_STREEBOG_512	0xEFEF
#define SIGALG_GOSTR12_256_STREEBOG_256	0xEEEE
#define SIGALG_GOSTR01_GOST94		0xEDED

/* Legacy sigalg for < 1.2 same value as boring uses*/
#define SIGALG_RSA_PKCS1_MD5_SHA1	0xFF01

#define SIGALG_FLAG_RSA_PSS	0x00000001

struct ssl_sigalg{
	uint16_t value;
	const EVP_MD *(*md)(void);
	int key_type;
	int pkey_idx; /* XXX get rid of this eventually */
	int curve_nid;
	int flags;
};

extern uint16_t tls12_sigalgs[];
extern size_t tls12_sigalgs_len;

const struct ssl_sigalg *ssl_sigalg_lookup(uint16_t sigalg);
const struct ssl_sigalg *ssl_sigalg(uint16_t sigalg, uint16_t *values, size_t len);
int ssl_sigalgs_build(CBB *cbb, uint16_t *values, size_t len);
int ssl_sigalg_pkey_check(uint16_t sigalg, EVP_PKEY *pk);
int ssl_sigalg_pkey_ok(const struct ssl_sigalg *sigalg, EVP_PKEY *pkey);

__END_HIDDEN_DECLS

#endif

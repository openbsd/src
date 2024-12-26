/*	$OpenBSD: mlkem_tests_util.h,v 1.4 2024/12/26 00:04:24 tb Exp $ */
/*
 * Copyright (c) 2024 Bob Beck <beck@obtuse.com>
 * Copyright (c) 2024 Theo Buehler <tb@openbsd.org>
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

#ifndef MLKEM_TEST_UTIL_H
#define MLKEM_TEST_UTIL_H

#include <stddef.h>
#include <stdint.h>

#include "bytestring.h"

#include "mlkem.h"
#include "mlkem_internal.h"

int compare_data(const uint8_t *want, const uint8_t *got, size_t len,
    const char *msg);

int mlkem768_encode_private_key(const void *priv, uint8_t **out_buf,
    size_t *out_len);
int mlkem768_encode_public_key(const void *pub, uint8_t **out_buf,
    size_t *out_len);
int mlkem1024_encode_private_key(const void *priv, uint8_t **out_buf,
    size_t *out_len);
int mlkem1024_encode_public_key(const void *pub, uint8_t **out_buf,
    size_t *out_len);

int mlkem768_decap(uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const uint8_t *ciphertext, size_t ciphertext_len, const void *priv);
void mlkem768_encap(uint8_t *out_ciphertext,
    uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES], const void *pub);
void mlkem768_encap_external_entropy(uint8_t *out_ciphertext,
    uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES], const void *pub,
    const uint8_t entropy[MLKEM_ENCAP_ENTROPY]);
void mlkem768_generate_key(uint8_t *out_encoded_public_key,
    uint8_t optional_out_seed[MLKEM_SEED_BYTES], void *out_private_key);
void mlkem768_generate_key_external_entropy(uint8_t *out_encoded_public_key,
    void *out_private_key, const uint8_t entropy[MLKEM_SEED_BYTES]);
int mlkem768_parse_private_key(void *priv, CBS *private_key_cbs);
int mlkem768_parse_public_key(void *pub, CBS *in);
void mlkem768_public_from_private(void *out_public_key, const void *private_key);

int mlkem1024_decap(uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const uint8_t *ciphertext, size_t ciphertext_len, const void *priv);
void mlkem1024_encap(uint8_t *out_ciphertext,
    uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES], const void *pub);
void mlkem1024_encap_external_entropy(uint8_t *out_ciphertext,
    uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES], const void *pub,
    const uint8_t entropy[MLKEM_ENCAP_ENTROPY]);
void mlkem1024_generate_key(uint8_t *out_encoded_public_key,
    uint8_t optional_out_seed[MLKEM_SEED_BYTES], void *out_private_key);
void mlkem1024_generate_key_external_entropy(uint8_t *out_encoded_public_key,
    void *out_private_key, const uint8_t entropy[MLKEM_SEED_BYTES]);
int mlkem1024_parse_private_key(void *priv, CBS *private_key_cbs);
int mlkem1024_parse_public_key(void *pub, CBS *in);
void mlkem1024_public_from_private(void *out_public_key, const void *private_key);

typedef int (*mlkem_encode_private_key_fn)(const void *, uint8_t **, size_t *);
typedef int (*mlkem_encode_public_key_fn)(const void *, uint8_t **, size_t *);
typedef int (*mlkem_decap_fn)(uint8_t [MLKEM_SHARED_SECRET_BYTES],
    const uint8_t *, size_t, const void *);
typedef void (*mlkem_encap_fn)(uint8_t *, uint8_t [MLKEM_SHARED_SECRET_BYTES],
    const void *);
typedef void (*mlkem_encap_external_entropy_fn)(uint8_t *,
    uint8_t [MLKEM_SHARED_SECRET_BYTES], const void *,
    const uint8_t [MLKEM_ENCAP_ENTROPY]);
typedef void (*mlkem_generate_key_fn)(uint8_t *, uint8_t *, void *);
typedef void (*mlkem_generate_key_external_entropy_fn)(uint8_t *, void *,
    const uint8_t [MLKEM_SEED_BYTES]);
typedef int (*mlkem_parse_private_key_fn)(void *, CBS *);
typedef int (*mlkem_parse_public_key_fn)(void *, CBS *);
typedef void (*mlkem_public_from_private_fn)(void *out_public_key,
    const void *private_key);

#endif /* MLKEM_TEST_UTIL_H */

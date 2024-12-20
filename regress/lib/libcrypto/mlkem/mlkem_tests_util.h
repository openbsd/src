/*	$OpenBSD: mlkem_tests_util.h,v 1.3 2024/12/20 00:07:12 tb Exp $ */
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

struct MLKEM1024_private_key;
struct MLKEM1024_public_key;
struct MLKEM768_private_key;
struct MLKEM768_public_key;

/* XXX - return values of the two compare functions are inconsistent */
int compare_data(const uint8_t *want, const uint8_t *got, size_t len,
    size_t line, const char *msg);
int compare_length(size_t want, size_t got, size_t line, const char *msg);

void hex_decode_cbs(CBS *cbs, CBB *cbb, size_t line, const char *msg);
int get_string_cbs(CBS *cbs, const char *str, size_t line, const char *msg);

int mlkem768_encode_private_key(const struct MLKEM768_private_key *priv,
    uint8_t **out_buf, size_t *out_len);
int mlkem768_encode_public_key(const struct MLKEM768_public_key *pub,
    uint8_t **out_buf, size_t *out_len);
int mlkem1024_encode_private_key(const struct MLKEM1024_private_key *priv,
    uint8_t **out_buf, size_t *out_len);
int mlkem1024_encode_public_key(const struct MLKEM1024_public_key *pub,
    uint8_t **out_buf, size_t *out_len);

#endif /* MLKEM_TEST_UTIL_H */

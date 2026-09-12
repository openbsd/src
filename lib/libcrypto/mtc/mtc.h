/*	$OpenBSD$ */
/*
 * Copyright (c) 2026, Bob Beck <beck@obtuse.com>
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

#ifndef HEADER_MTC_H
#define HEADER_MTC_H

#include <stddef.h>
#include <stdint.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/ossl_typ.h>
#include <openssl/safestack.h>

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_STACK_OF(OSSL_MTC_CA)

OSSL_MTC_CA *OSSL_MTC_CA_new(const uint8_t *ca_id, size_t ca_id_len,
    const EVP_MD *hash, uint64_t min_serial, EVP_PKEY *cosigner_pkey);
void OSSL_MTC_CA_free(OSSL_MTC_CA *ca);

int OSSL_MTC_CA_add1_cosigner(OSSL_MTC_CA *ca, const uint8_t *id,
    size_t id_len, const char *sig_name, EVP_PKEY *pkey);
int OSSL_MTC_CA_add_revoked_range(OSSL_MTC_CA *ca, uint64_t start,
    uint64_t end);
int OSSL_MTC_CA_set_max_serial(OSSL_MTC_CA *ca, uint64_t max_serial);
int OSSL_MTC_CA_get0_id(const OSSL_MTC_CA *ca, const uint8_t **out_id,
    size_t *out_id_len);
int OSSL_MTC_CA_cmp(const OSSL_MTC_CA * const *a, const OSSL_MTC_CA * const *b);
OSSL_MTC_CA *OSSL_MTC_CA_find(STACK_OF(OSSL_MTC_CA) *cas, const uint8_t *ca_id,
    size_t ca_id_len, const char *ca_id_str);
int OSSL_MTC_CA_parse_certificates(void *libctx, const char *propq, BIO *in,
    STACK_OF(OSSL_MTC_CA) *out_cas);

int OSSL_MTC_CA_load_landmarks(OSSL_MTC_CA *ca, uint64_t log_number, BIO *in);
int OSSL_MTC_CA_add_subtree_hash(OSSL_MTC_CA *ca, uint64_t log_number,
    uint64_t start, uint64_t end, const uint8_t *hash, size_t hash_len);

uint64_t OSSL_MTC_serial(uint16_t log_number, uint64_t index);

#ifdef __cplusplus
}
#endif

#endif /* !HEADER_MTC_H */

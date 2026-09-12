/* $OpenBSD$ */
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

#ifndef _LIBCRYPTO_MTC_H
#define _LIBCRYPTO_MTC_H

#ifndef _MSC_VER
#include_next <openssl/mtc.h>
#else
#include "../include/openssl/mtc.h"
#endif
#include "crypto_namespace.h"

LCRYPTO_USED(OSSL_MTC_CA_new);
LCRYPTO_USED(OSSL_MTC_CA_free);
LCRYPTO_USED(OSSL_MTC_CA_add1_cosigner);
LCRYPTO_USED(OSSL_MTC_CA_add_revoked_range);
LCRYPTO_USED(OSSL_MTC_CA_set_max_serial);
LCRYPTO_USED(OSSL_MTC_CA_get0_id);
LCRYPTO_USED(OSSL_MTC_CA_cmp);
LCRYPTO_USED(OSSL_MTC_CA_load_landmarks);
LCRYPTO_USED(OSSL_MTC_CA_add_subtree_hash);
LCRYPTO_USED(OSSL_MTC_serial);

#endif /* _LIBCRYPTO_MTC_H */

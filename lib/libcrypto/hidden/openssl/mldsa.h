/* $OpenBSD$ */
/*
 * Copyright (c) 2026 Bob Beck <beck@openbsd.org>
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

#ifndef _LIBCRYPTO_MLDSA_H
#define _LIBCRYPTO_MLDSA_H

#ifndef _MSC_VER
#include_next <openssl/mldsa.h>
#else
#include "../include/openssl/mldsa.h"
#endif
#include "crypto_namespace.h"

LCRYPTO_USED(MLDSA_private_key_new);
LCRYPTO_USED(MLDSA_private_key_free);
LCRYPTO_USED(MLDSA_public_key_new);
LCRYPTO_USED(MLDSA_public_key_free);
LCRYPTO_USED(MLDSA_generate_key);
LCRYPTO_USED(MLDSA_private_key_from_seed);
LCRYPTO_USED(MLDSA_public_from_private);
LCRYPTO_USED(MLDSA_sign);
LCRYPTO_USED(MLDSA_verify);
LCRYPTO_USED(MLDSA_marshal_public_key);
LCRYPTO_USED(MLDSA_parse_public_key);
LCRYPTO_USED(MLDSA_parse_private_key);

#endif /* _LIBCRYPTO_MLDSA_H */

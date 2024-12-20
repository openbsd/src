/* $OpenBSD: mlkem.h,v 1.4 2024/12/20 15:10:31 tb Exp $ */
/*
 * Copyright (c) 2024 Bob Beck <beck@obtuse.com>
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

#ifndef _LIBCRYPTO_MLKEM_H
#define _LIBCRYPTO_MLKEM_H

/* Undo when making public */
#ifdef LIBRESSL_HAS_MLKEM

#ifndef _MSC_VER
#include_next <openssl/mlkem.h>
#else
#include "../include/openssl/mlkem.h"
#endif
#include "crypto_namespace.h"

LCRYPTO_USED(MLKEM768_generate_key);
LCRYPTO_USED(MLKEM768_public_from_private);
LCRYPTO_USED(MLKEM768_encap);
LCRYPTO_USED(MLKEM768_decap);
LCRYPTO_USED(MLKEM768_marshal_public_key);
LCRYPTO_USED(MLKEM768_parse_public_key);
LCRYPTO_USED(MLKEM768_private_key_from_seed);
LCRYPTO_USED(MLKEM768_parse_private_key);
LCRYPTO_USED(MLKEM1024_generate_key);
LCRYPTO_USED(MLKEM1024_public_from_private);
LCRYPTO_USED(MLKEM1024_encap);
LCRYPTO_USED(MLKEM1024_decap);
LCRYPTO_USED(MLKEM1024_marshal_public_key);
LCRYPTO_USED(MLKEM1024_parse_public_key);
LCRYPTO_USED(MLKEM1024_private_key_from_seed);
LCRYPTO_USED(MLKEM1024_parse_private_key);
#endif /* LIBRESSL_HAS_MLKEM */

#endif /* _LIBCRYPTO_MLKEM_H */

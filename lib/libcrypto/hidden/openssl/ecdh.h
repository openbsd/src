/* $OpenBSD: ecdh.h,v 1.1 2023/07/08 06:04:33 beck Exp $ */
/*
 * Copyright (c) 2023 Bob Beck <beck@openbsd.org>
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

#ifndef _LIBCRYPTO_ECDH_H
#define _LIBCRYPTO_ECDH_H

#ifndef _MSC_VER
#include_next <openssl/ecdh.h>
#else
#include "../include/openssl/ecdh.h"
#endif
#include "crypto_namespace.h"

LCRYPTO_USED(ECDH_OpenSSL);
LCRYPTO_USED(ECDH_set_default_method);
LCRYPTO_USED(ECDH_get_default_method);
LCRYPTO_USED(ECDH_set_method);
LCRYPTO_USED(ECDH_size);
LCRYPTO_USED(ECDH_compute_key);
LCRYPTO_USED(ECDH_get_ex_new_index);
LCRYPTO_USED(ECDH_set_ex_data);
LCRYPTO_USED(ECDH_get_ex_data);
LCRYPTO_USED(ERR_load_ECDH_strings);

#endif /* _LIBCRYPTO_ECDH_H */

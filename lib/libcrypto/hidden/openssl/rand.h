/* $OpenBSD: rand.h,v 1.3 2023/07/28 09:53:55 tb Exp $ */
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

#ifndef _LIBCRYPTO_RAND_H
#define _LIBCRYPTO_RAND_H

#ifndef _MSC_VER
#include_next <openssl/rand.h>
#else
#include "../include/openssl/rand.h"
#endif
#include "crypto_namespace.h"

LCRYPTO_USED(RAND_set_rand_method);
LCRYPTO_USED(RAND_get_rand_method);
LCRYPTO_USED(RAND_SSLeay);
LCRYPTO_USED(ERR_load_RAND_strings);

#endif /* _LIBCRYPTO_RAND_H */

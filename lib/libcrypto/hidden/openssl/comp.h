/* $OpenBSD: comp.h,v 1.1 2023/07/08 08:26:26 beck Exp $ */
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

#ifndef _LIBCRYPTO_COMP_H
#define _LIBCRYPTO_COMP_H

#ifndef _MSC_VER
#include_next <openssl/comp.h>
#else
#include "../include/openssl/comp.h"
#endif
#include "crypto_namespace.h"

LCRYPTO_USED(COMP_CTX_new);
LCRYPTO_USED(COMP_CTX_free);
LCRYPTO_USED(COMP_compress_block);
LCRYPTO_USED(COMP_expand_block);
LCRYPTO_USED(COMP_rle);
LCRYPTO_USED(COMP_zlib);
LCRYPTO_USED(COMP_zlib_cleanup);
LCRYPTO_USED(ERR_load_COMP_strings);

#endif /* _LIBCRYPTO_COMP_H */

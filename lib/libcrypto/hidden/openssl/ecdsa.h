/* $OpenBSD: ecdsa.h,v 1.2 2023/07/07 19:37:54 beck Exp $ */
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

#ifndef _LIBCRYPTO_ECDSA_H
#define _LIBCRYPTO_ECDSA_H

#ifndef _MSC_VER
#include_next <openssl/ecdsa.h>
#else
#include "../include/openssl/ecdsa.h"
#endif
#include "crypto_namespace.h"

LCRYPTO_USED(ECDSA_SIG_new);
LCRYPTO_USED(ECDSA_SIG_free);
LCRYPTO_USED(i2d_ECDSA_SIG);
LCRYPTO_USED(d2i_ECDSA_SIG);
LCRYPTO_USED(ECDSA_SIG_get0);
LCRYPTO_USED(ECDSA_SIG_get0_r);
LCRYPTO_USED(ECDSA_SIG_get0_s);
LCRYPTO_USED(ECDSA_SIG_set0);
LCRYPTO_USED(ECDSA_do_sign);
LCRYPTO_USED(ECDSA_do_sign_ex);
LCRYPTO_USED(ECDSA_do_verify);
LCRYPTO_USED(ECDSA_OpenSSL);
LCRYPTO_USED(ECDSA_set_default_method);
LCRYPTO_USED(ECDSA_get_default_method);
LCRYPTO_USED(ECDSA_set_method);
LCRYPTO_USED(ECDSA_size);
LCRYPTO_USED(ECDSA_sign_setup);
LCRYPTO_USED(ECDSA_sign);
LCRYPTO_USED(ECDSA_sign_ex);
LCRYPTO_USED(ECDSA_verify);
LCRYPTO_USED(ECDSA_get_ex_new_index);
LCRYPTO_USED(ECDSA_set_ex_data);
LCRYPTO_USED(ECDSA_get_ex_data);
LCRYPTO_USED(EC_KEY_METHOD_set_sign);
LCRYPTO_USED(EC_KEY_METHOD_set_verify);
LCRYPTO_USED(EC_KEY_METHOD_get_sign);
LCRYPTO_USED(EC_KEY_METHOD_get_verify);
LCRYPTO_USED(ERR_load_ECDSA_strings);

#endif /* _LIBCRYPTO_ECDSA_H */

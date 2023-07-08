/* $OpenBSD: dso.h,v 1.1 2023/07/08 07:22:58 beck Exp $ */
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

#ifndef _LIBCRYPTO_DSO_H
#define _LIBCRYPTO_DSO_H

#ifndef _MSC_VER
#include_next <openssl/dso.h>
#else
#include "../include/openssl/dso.h"
#endif
#include "crypto_namespace.h"

LCRYPTO_USED(DSO_new);
LCRYPTO_USED(DSO_new_method);
LCRYPTO_USED(DSO_free);
LCRYPTO_USED(DSO_flags);
LCRYPTO_USED(DSO_up_ref);
LCRYPTO_USED(DSO_ctrl);
LCRYPTO_USED(DSO_set_name_converter);
LCRYPTO_USED(DSO_get_filename);
LCRYPTO_USED(DSO_set_filename);
LCRYPTO_USED(DSO_convert_filename);
LCRYPTO_USED(DSO_merge);
LCRYPTO_USED(DSO_get_loaded_filename);
LCRYPTO_USED(DSO_set_default_method);
LCRYPTO_USED(DSO_get_default_method);
LCRYPTO_USED(DSO_get_method);
LCRYPTO_USED(DSO_set_method);
LCRYPTO_USED(DSO_load);
LCRYPTO_USED(DSO_bind_var);
LCRYPTO_USED(DSO_bind_func);
LCRYPTO_USED(DSO_METHOD_openssl);
LCRYPTO_USED(DSO_METHOD_null);
LCRYPTO_USED(DSO_METHOD_dlfcn);
LCRYPTO_USED(DSO_pathbyaddr);
LCRYPTO_USED(DSO_global_lookup);
LCRYPTO_USED(ERR_load_DSO_strings);

#endif /* _LIBCRYPTO_DSO_H */

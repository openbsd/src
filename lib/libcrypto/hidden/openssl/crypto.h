/* $OpenBSD: crypto.h,v 1.2 2023/07/28 10:19:20 tb Exp $ */
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

#ifndef _LIBCRYPTO_CRYPTO_H
#define _LIBCRYPTO_CRYPTO_H

#ifndef _MSC_VER
#include_next <openssl/crypto.h>
#else
#include "../include/openssl/crypto.h"
#endif
#include "crypto_namespace.h"

LCRYPTO_USED(OpenSSL_version);
LCRYPTO_USED(OpenSSL_version_num);
LCRYPTO_USED(SSLeay_version);
LCRYPTO_USED(SSLeay);
LCRYPTO_USED(CRYPTO_get_ex_new_index);
LCRYPTO_USED(CRYPTO_new_ex_data);
LCRYPTO_USED(CRYPTO_dup_ex_data);
LCRYPTO_USED(CRYPTO_free_ex_data);
LCRYPTO_USED(CRYPTO_set_ex_data);
LCRYPTO_USED(CRYPTO_get_ex_data);
LCRYPTO_USED(CRYPTO_cleanup_all_ex_data);
LCRYPTO_USED(CRYPTO_lock);
LCRYPTO_USED(CRYPTO_add_lock);
LCRYPTO_USED(CRYPTO_THREADID_current);
LCRYPTO_USED(CRYPTO_THREADID_cmp);
LCRYPTO_USED(CRYPTO_THREADID_cpy);
LCRYPTO_USED(CRYPTO_THREADID_hash);
LCRYPTO_USED(CRYPTO_set_mem_functions);
LCRYPTO_USED(CRYPTO_set_locked_mem_functions);
LCRYPTO_USED(CRYPTO_set_mem_ex_functions);
LCRYPTO_USED(CRYPTO_set_locked_mem_ex_functions);
LCRYPTO_USED(CRYPTO_set_mem_debug_functions);
LCRYPTO_USED(CRYPTO_get_mem_functions);
LCRYPTO_USED(CRYPTO_get_locked_mem_functions);
LCRYPTO_USED(CRYPTO_get_mem_ex_functions);
LCRYPTO_USED(CRYPTO_get_locked_mem_ex_functions);
LCRYPTO_USED(CRYPTO_get_mem_debug_functions);
LCRYPTO_USED(CRYPTO_realloc_clean);
LCRYPTO_USED(CRYPTO_remalloc);
LCRYPTO_USED(CRYPTO_set_mem_debug_options);
LCRYPTO_USED(CRYPTO_get_mem_debug_options);
LCRYPTO_USED(CRYPTO_push_info_);
LCRYPTO_USED(CRYPTO_pop_info);
LCRYPTO_USED(CRYPTO_remove_all_info);
LCRYPTO_USED(CRYPTO_dbg_malloc);
LCRYPTO_USED(CRYPTO_dbg_realloc);
LCRYPTO_USED(CRYPTO_dbg_free);
LCRYPTO_USED(CRYPTO_dbg_set_options);
LCRYPTO_USED(CRYPTO_dbg_get_options);
LCRYPTO_USED(CRYPTO_mem_leaks_fp);
LCRYPTO_USED(CRYPTO_mem_leaks);
LCRYPTO_USED(CRYPTO_mem_leaks_cb);
LCRYPTO_USED(OpenSSLDie);
LCRYPTO_USED(OPENSSL_cpu_caps);
LCRYPTO_USED(OPENSSL_init_crypto);
LCRYPTO_USED(OPENSSL_cleanup);
LCRYPTO_USED(ERR_load_CRYPTO_strings);

#endif /* _LIBCRYPTO_CRYPTO_H */

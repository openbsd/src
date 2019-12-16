/*
 * Copyright (C) 2014, 2016  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef DST_GOST_H
#define DST_GOST_H 1

#include <isc/lang.h>
#include <isc/log.h>
#include <dst/result.h>

#define ISC_GOST_DIGESTLENGTH 32U

#ifdef HAVE_OPENSSL_GOST
#include <openssl/evp.h>

typedef struct {
	EVP_MD_CTX *ctx;
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	EVP_MD_CTX _ctx;
#endif
} isc_gost_t;

#endif
#ifdef HAVE_PKCS11_GOST
#include <pk11/pk11.h>

typedef pk11_context_t isc_gost_t;
#endif

ISC_LANG_BEGINDECLS

#if defined(HAVE_OPENSSL_GOST) || defined(HAVE_PKCS11_GOST)

isc_result_t
isc_gost_init(isc_gost_t *ctx);

void
isc_gost_invalidate(isc_gost_t *ctx);

isc_result_t
isc_gost_update(isc_gost_t *ctx, const unsigned char *data, unsigned int len);

isc_result_t
isc_gost_final(isc_gost_t *ctx, unsigned char *digest);

ISC_LANG_ENDDECLS

#endif /* HAVE_OPENSSL_GOST || HAVE_PKCS11_GOST */

#endif /* DST_GOST_H */

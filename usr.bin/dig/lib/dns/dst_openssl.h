/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: dst_openssl.h,v 1.2 2020/02/12 13:05:03 jsg Exp $ */

#ifndef DST_OPENSSL_H
#define DST_OPENSSL_H 1

#include <isc/lang.h>
#include <isc/log.h>
#include <isc/result.h>

#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <openssl/bn.h>


/*
 * EVP_dss1() is a version of EVP_sha1() that was needed prior to
 * 1.1.0 because there was a link between digests and signing algorithms;
 * the link has been eliminated and EVP_sha1() can be used now instead.
 */
#define EVP_dss1 EVP_sha1

ISC_LANG_BEGINDECLS

#define dst__openssl_getengine(x) NULL

ISC_LANG_ENDDECLS

#endif /* DST_OPENSSL_H */
/*! \file */

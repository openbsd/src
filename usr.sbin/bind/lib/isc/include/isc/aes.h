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

/* $Id: aes.h,v 1.3 2020/01/09 14:18:30 florian Exp $ */

/*! \file isc/aes.h */

#ifndef ISC_AES_H
#define ISC_AES_H 1

#include <isc/lang.h>
#include <isc/platform.h>
#include <isc/types.h>

#define ISC_AES128_KEYLENGTH 16U
#define ISC_AES192_KEYLENGTH 24U
#define ISC_AES256_KEYLENGTH 32U
#define ISC_AES_BLOCK_LENGTH 16U

ISC_LANG_BEGINDECLS

void
isc_aes128_crypt(const unsigned char *key, const unsigned char *in,
		 unsigned char *out);

void
isc_aes192_crypt(const unsigned char *key, const unsigned char *in,
		 unsigned char *out);

void
isc_aes256_crypt(const unsigned char *key, const unsigned char *in,
		 unsigned char *out);

ISC_LANG_ENDDECLS

#endif /* ISC_AES_H */

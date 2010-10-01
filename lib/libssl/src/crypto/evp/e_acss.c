/*	$Id: e_acss.c,v 1.3 2010/10/01 23:33:22 djm Exp $	*/
/*
 * Copyright (c) 2004 The OpenBSD project
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

#ifndef OPENSSL_NO_ACSS

#include "cryptlib.h"
#include <openssl/evp.h>
#include <openssl/objects.h>
#include "evp_locl.h"
#include <openssl/acss.h>

typedef struct {
    ACSS_KEY ks;
} EVP_ACSS_KEY;

#define data(ctx) EVP_C_DATA(EVP_ACSS_KEY,ctx)

static int acss_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
		const unsigned char *iv, int enc);
static int acss_ciph(EVP_CIPHER_CTX *ctx, unsigned char *out,
		const unsigned char *in, size_t inl);
static int acss_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr);
static const EVP_CIPHER acss_cipher = {
	NID_undef,
	1,5,0,
	0,
	acss_init_key,
	acss_ciph,
	NULL,
	sizeof(EVP_ACSS_KEY),
	NULL,
	NULL,
	acss_ctrl,
	NULL
};

const
EVP_CIPHER *EVP_acss(void)
{
	return(&acss_cipher);
}

static int
acss_init_key(EVP_CIPHER_CTX *ctx, const unsigned char *key,
		const unsigned char *iv, int enc)
{
	acss_setkey(&data(ctx)->ks,key,enc,ACSS_MODE1);
	return 1;
}

static int
acss_ciph(EVP_CIPHER_CTX *ctx, unsigned char *out, const unsigned char *in,
		size_t inl)
{
	acss(&data(ctx)->ks,inl,in,out);
	return 1;
}

static int
acss_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr)
{
	switch(type) {
	case EVP_CTRL_SET_ACSS_MODE:
		data(ctx)->ks.mode = arg;
		return 1;

	default:
		return -1;
	}
}
#endif

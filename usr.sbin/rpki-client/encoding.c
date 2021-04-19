/*	$OpenBSD: encoding.c,v 1.2 2021/04/19 17:04:35 deraadt Exp $  */
/*
 * Copyright (c) 2020 Claudio Jeker <claudio@openbsd.org>
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
#include <err.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>

#include "extern.h"

/*
 * Decode base64 encoded string into binary buffer returned in out.
 * The out buffer size is stored in outlen.
 * Returns 0 on success or -1 for any errors.
 */
int
base64_decode(const unsigned char *in, unsigned char **out, size_t *outlen)
{
	static EVP_ENCODE_CTX *ctx;
	unsigned char *to;
	size_t inlen;
	int tolen;

	if (ctx == NULL && (ctx = EVP_ENCODE_CTX_new()) == NULL)
		err(1, "EVP_ENCODE_CTX_new");

	*out = NULL;
	*outlen = 0;

	inlen = strlen(in);
	if (inlen >= INT_MAX - 3)
		return -1;
	tolen = ((inlen + 3) / 4) * 3 + 1;
	if ((to = malloc(tolen)) == NULL)
		return -1;

	EVP_DecodeInit(ctx);
	if (EVP_DecodeUpdate(ctx, to, &tolen, in, inlen) == -1)
		goto fail;
	*outlen = tolen;
	if (EVP_DecodeFinal(ctx, to + tolen, &tolen) == -1)
		goto fail;
	*outlen += tolen;
	*out = to;
	return 0;

fail:
	free(to);
	return -1;
}

/*
 * Convert binary buffer of size dsz into an upper-case hex-string.
 * Returns pointer to the newly allocated string. Function can't fail.
 */
char *
hex_encode(const unsigned char *in, size_t insz)
{
	const char hex[] = "0123456789ABCDEF";
	size_t i;
	char *out;

	if ((out = calloc(2, insz + 1)) == NULL)
		err(1, NULL);

	for (i = 0; i < insz; i++) {
		out[i * 2] = hex[in[i] >> 4];
		out[i * 2 + 1] = hex[in[i] & 0xf];
	}
	out[i * 2] = '\0';

	return out;
}

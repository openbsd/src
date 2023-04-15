/*	$OpenBSD: sha3_internal.h,v 1.7 2023/04/15 18:30:27 jsing Exp $	*/
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Markku-Juhani O. Saarinen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stddef.h>
#include <stdint.h>

#ifndef HEADER_SHA3_INTERNAL_H
#define HEADER_SHA3_INTERNAL_H

typedef struct {
	union {
		uint8_t b[200];		/* State as 8 bit bytes. */
		uint64_t q[25];		/* State as 64 bit words. */
	} st;
	int pt, rsiz, mdlen;
} sha3_ctx;

void sha3_keccakf(uint64_t st[25]);

int sha3_init(sha3_ctx *c, int mdlen);
int sha3_update(sha3_ctx *c, const void *data, size_t len);
int sha3_final(void *md, sha3_ctx *c);

void *sha3(const void *in, size_t inlen, void *md, int mdlen);

/* SHAKE128 and SHAKE256 extensible-output functions. */
#define shake128_init(c) sha3_init(c, 16)
#define shake256_init(c) sha3_init(c, 32)
#define shake_update sha3_update

void shake_xof(sha3_ctx *c);
void shake_out(sha3_ctx *c, void *out, size_t len);

#endif

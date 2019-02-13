/*	$Id: hash.c,v 1.3 2019/02/13 05:41:35 tb Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <sys/types.h>

#include <assert.h>
#include <endian.h>
#include <stdint.h>
#include <stdlib.h>

#include <openssl/md4.h>

#include "extern.h"

/*
 * A fast 32-bit hash.
 * Described in Tridgell's "Efficient Algorithms for Sorting and
 * Synchronization" thesis and the "Rolling checksum" document.
 */
uint32_t
hash_fast(const void *buf, size_t len)
{
	size_t			 i = 0;
	uint32_t		 a = 0, /* part of a(k, l) */
				 b = 0; /* b(k, l) */
	const signed char	*dat = buf;

	if (len > 4)
		for ( ; i < len - 4; i += 4) {
			b += 4 * (a + dat[i]) +
			    3 * dat[i + 1] +
			    2 * dat[i + 2] +
			    dat[i + 3];
			a += dat[i + 0] +
			    dat[i + 1] +
			    dat[i + 2] +
			    dat[i + 3];
		}

	for ( ; i < len; i++) {
		a += dat[i];
		b += a;
	}

	/* s(k, l) = (eps % M) + 2^16 b(k, l) % M */

	return (a & 0xffff) + (b << 16);
}

/*
 * Slow MD4-based hash with trailing seed.
 */
void
hash_slow(const void *buf, size_t len,
	unsigned char *md, const struct sess *sess)
{
	MD4_CTX		 ctx;
	int32_t		 seed = htole32(sess->seed);

	MD4_Init(&ctx);
	MD4_Update(&ctx, buf, len);
	MD4_Update(&ctx, (unsigned char *)&seed, sizeof(int32_t));
	MD4_Final(md, &ctx);
}

/*
 * Hash an entire file.
 * This is similar to hash_slow() except the seed is hashed at the end
 * of the sequence, not the beginning.
 */
void
hash_file(const void *buf, size_t len,
	unsigned char *md, const struct sess *sess)
{
	MD4_CTX		 ctx;
	int32_t		 seed = htole32(sess->seed);

	MD4_Init(&ctx);
	MD4_Update(&ctx, (unsigned char *)&seed, sizeof(int32_t));
	MD4_Update(&ctx, buf, len);
	MD4_Final(md, &ctx);
}

/*	$OpenBSD: strlcpy.c,v 1.16 2019/01/25 00:19:25 millert Exp $	*/

/*
 * Copyright (c) 1998, 2015 Todd C. Miller <millert@openbsd.org>
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
#include <string.h>

/*
        The function copies characters from src to dst.

        src is a nul terminating string.
        dst is a string buffer.
        dsize is the size of dst.
        If (dsize != 0), the terminating nul is always appended to dst;
        So, at most (dsize - 1) chars will be copied from src.

        The function returns the number of truncated chars of src.
        If the return value is zero, everything is OK;
        otherwise truncation happened.

        Example 1:

                if (strlcpy(dst, src, dsize))
                        puts("truncation");

        Example 2:

                size_t n;

                if ((n = strlcpy(dst, src, dsize)))
                        printf("truncation: %lu %s\n", n,
                                n > 1 ? "chars" : "char");
 */
size_t
strlcpy(char *dst, const char *src, size_t dsize)
{
	const char *sbeg;

	if (dsize != 0)
		for (dst[--dsize] = '\0'; dsize-- != 0; ++src)
			if ((*dst++ = *src) == '\0') break;

	for (sbeg = src; *src != '\0'; ++src) { }

	return (src - sbeg);
}
DEF_WEAK(strlcpy);

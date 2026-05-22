/*	$Id: base64.c,v 1.10 2026/05/22 01:53:10 jmatthew Exp $ */
/*
 * Copyright (c) 2016 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <netinet/in.h>
#include <resolv.h>

#include <stdlib.h>
#include <string.h>

#include "extern.h"

/*
 * Compute the maximum buffer required for a base64 encoded string of
 * length "len".
 */
size_t
base64len(size_t len)
{

	return (len + 2) / 3 * 4 + 1;
}

/*
 * Pass a stream of bytes to be base64 encoded, then converted into
 * base64url format.
 * Returns NULL on allocation failure (not logged).
 */
char *
base64buf_url(const char *data, size_t len)
{
	size_t	 i, sz;
	char	*buf;

	sz = base64len(len);
	if ((buf = malloc(sz)) == NULL)
		return NULL;

	b64_ntop(data, len, buf, sz);

	for (i = 0; i < sz; i++)
		switch (buf[i]) {
		case '+':
			buf[i] = '-';
			break;
		case '/':
			buf[i] = '_';
			break;
		case '=':
			buf[i] = '\0';
			break;
		}

	return buf;
}

static const u_int8_t index_64[128] = {
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
	255, 255, 255, 255, 255,  62, 255, 255,  52,  53,
	 54,  55,  56,  57,  58,  59,  60,  61, 255, 255,
	255, 255, 255, 255, 255,   0,   1,   2,   3,   4,
	  5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
	 15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
	 25, 255, 255, 255, 255,  63, 255,  26,  27,  28,
	 29,  30,  31,  32,  33,  34,  35,  36,  37,  38,
	 39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
	 49,  50,  51, 255, 255, 255, 255, 255
};
#define CHAR64(c)  ( (c) > 127 ? 255 : index_64[(c)])

int
unbase64buf_url(const unsigned char *data, unsigned char **decoded)
{
	unsigned char		*out, *bp;
	const unsigned char 	*p;
	unsigned char		c1, c2, c3, c4;
	int 			len;

	len = strlen(data);
	out = malloc(len);
	if (out == NULL)
		return -1;

	p = data;
	bp = out;
	while (p < data + len) {
                c1 = CHAR64(*p);
                /* Invalid data */
                if (c1 == 255)
                        return -1;

                c2 = CHAR64(*(p + 1));
                if (c2 == 255)
                        return -1;

                *bp++ = (c1 << 2) | ((c2 & 0x30) >> 4);
                if ((p + 2) >= data + len)
                        break;

                c3 = CHAR64(*(p + 2));
                if (c3 == 255)
                        return -1;

                *bp++ = ((c2 & 0x0f) << 4) | ((c3 & 0x3c) >> 2);
                if ((p + 3) >= data + len)
                        break;

                c4 = CHAR64(*(p + 3));
                if (c4 == 255)
                        return -1;
                *bp++ = ((c3 & 0x03) << 6) | c4;

                p += 4;
	}

	*decoded = out;
	return (bp - out);
}

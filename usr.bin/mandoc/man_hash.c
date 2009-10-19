/*	$Id: man_hash.c,v 1.7 2009/10/19 10:20:24 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009 Kristaps Dzonsons <kristaps@kth.se>
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
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "libman.h"

static	u_char		table[26 * 6];

/*
 * XXX - this hash has global scope, so if intended for use as a library
 * with multiple callers, it will need re-invocation protection.
 */
void
man_hash_init(void)
{
	int		 i, j, x;

	memset(table, UCHAR_MAX, sizeof(table));

	for (i = 0; i < MAN_MAX; i++) {
		x = man_macronames[i][0];
		assert((x >= 65 && x <= 90) ||
				(x >= 97 && x <= 122));

		x -= (x <= 90) ? 65 : 97;
		x *= 6;

		for (j = 0; j < 6; j++)
			if (UCHAR_MAX == table[x + j]) {
				table[x + j] = (u_char)i;
				break;
			}
		assert(j < 6);
	}
}

int
man_hash_find(const char *tmp)
{
	int		 x, i, tok;

	if (0 == (x = tmp[0]))
		return(MAN_MAX);
	if ( ! ((x >= 65 && x <= 90) || (x >= 97 && x <= 122)))
		return(MAN_MAX);

	x -= (x <= 90) ? 65 : 97;
	x *= 6;

	for (i = 0; i < 6; i++) {
		if (UCHAR_MAX == (tok = table[x + i]))
			return(MAN_MAX);
		if (0 == strcmp(tmp, man_macronames[tok]))
			return(tok);
	}

	return(MAN_MAX);
}

/* $Id: man_hash.c,v 1.1 2009/04/06 20:30:40 kristaps Exp $ */
/*
 * Copyright (c) 2008, 2009 Kristaps Dzonsons <kristaps@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "libman.h"


/* ARGUSED */
void
man_hash_free(void *htab)
{

	free(htab);
}


/* ARGUSED */
void *
man_hash_alloc(void)
{
	int		*htab;
	int		 i, j, x;

	htab = calloc(26 * 5, sizeof(int));
	if (NULL == htab)
		return(NULL);

	for (i = 1; i < MAN_MAX; i++) {
		x = man_macronames[i][0];

		assert((x >= 65 && x <= 90) ||
				(x >= 97 && x <= 122));

		x -= (x <= 90) ? 65 : 97;
		x *= 5;

		for (j = 0; j < 5; j++)
			if (0 == htab[x + j]) {
				htab[x + j] = i;
				break;
			}

		assert(j < 5);
	}

	return((void *)htab);
}


int
man_hash_find(const void *arg, const char *tmp)
{
	int		 x, i, tok;
	const int	*htab;

	htab = (const int *)arg;

	if (0 == (x = tmp[0]))
		return(MAN_MAX);
	if ( ! ((x >= 65 && x <= 90) || (x >= 97 && x <= 122)))
		return(MAN_MAX);

	x -= (x <= 90) ? 65 : 97;
	x *= 5;

	for (i = 0; i < 5; i++) {
		if (0 == (tok = htab[x + i]))
			return(MAN_MAX);
		if (0 == strcmp(tmp, man_macronames[tok]))
			return(tok);
	}

	return(MAN_MAX);
}


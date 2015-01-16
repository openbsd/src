/*	$OpenBSD: path.c,v 1.4 2015/01/16 16:18:07 deraadt Exp $	*/

/*
 * Copyright (c) 2013 Kurt Miller <kurt@intricatesoftware.com>
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
#include "path.h"
#include "util.h"

char **
_dl_split_path(const char *searchpath)
{
	int pos = 0;
	int count = 1;
	const char *pp, *p_begin;
	char **retval;

	if (searchpath == NULL)
		return (NULL);

	/* Count ':' or ';' in searchpath */
	pp = searchpath;
	while (*pp) {
		if (*pp == ':' || *pp == ';')
			count++;
		pp++;
	}

	/* one more for NULL entry */
	count++;

	retval = _dl_reallocarray(NULL, count, sizeof(retval));
	if (retval == NULL)
		return (NULL);

	pp = searchpath;
	while (pp) {
		p_begin = pp;
		while (*pp != '\0' && *pp != ':' && *pp != ';')
			pp++;

		/* interpret "" as curdir "." */
		if (p_begin == pp) {
			retval[pos] = _dl_malloc(2);
			if (retval[pos] == NULL)
				goto badret;

			_dl_bcopy(".", retval[pos++], 2);
		} else {
			retval[pos] = _dl_malloc(pp - p_begin + 1);
			if (retval[pos] == NULL)
				goto badret;

			_dl_bcopy(p_begin, retval[pos], pp - p_begin);
			retval[pos++][pp - p_begin] = '\0';
		}

		if (*pp)        /* Try curdir if ':' at end */
			pp++;
		else
			pp = NULL;
	}

	retval[pos] = NULL;
	return (retval);

badret:
	_dl_free_path(retval);
	return (NULL);
}

void
_dl_free_path(char **path)
{
	char **p = path;

	if (path == NULL)
		return;

	while (*p != NULL)
		_dl_free(*p++);

	_dl_free(path);
}

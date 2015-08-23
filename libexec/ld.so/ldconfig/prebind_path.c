/*	$OpenBSD: prebind_path.c,v 1.4 2015/08/23 06:27:32 deraadt Exp $	*/

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
#include <stdlib.h>
#include <string.h>
#include "util.h"

void *
_dl_reallocarray(void *ptr, size_t cnt, size_t num)
{
	return reallocarray(ptr, cnt, num);
}

void *
_dl_malloc(size_t need)
{
	void *ret = malloc(need);
	if (ret != NULL)
		memset(ret, 0, need);
	return (ret);
}

void
_dl_free(void *p)
{
	free(p);
}

#include "path.c"

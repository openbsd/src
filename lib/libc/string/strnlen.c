/*	$OpenBSD: strnlen.c,v 1.7 2016/10/14 18:19:04 dtucker Exp $	*/

/*
 * Copyright (c) 2010 Todd C. Miller <Todd.Miller@courtesan.com>
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
#include <stdint.h>

size_t
strnlen(const char *str, size_t maxlen)
{
	const char *cp;

	for (cp = str; maxlen != 0 && *cp != '\0'; cp++, maxlen--)
		;

	/*
	 * Cast pointers to unsigned type before calculation, to avoid signed
	 * overflow when the string ends where the MSB has changed.
	 */
	return (size_t)((uintptr_t)cp - (uintptr_t)str);
}
DEF_WEAK(strnlen);

/*	$OpenBSD: subr_poison.c,v 1.8 2014/07/08 17:19:25 deraadt Exp $ */
/*
 * Copyright (c) 2013 Ted Unangst <tedu@openbsd.org>
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
#include <sys/param.h>
#include <sys/malloc.h>

/*
 * The POISON is used as known text to copy into free objects so
 * that modifications after frees can be detected.
 */
#ifdef DEADBEEF0
#define POISON0	((unsigned) DEADBEEF0)
#else
#define POISON0	((unsigned) 0xdeadbeef)
#endif
#ifdef DEADBEEF1
#define POISON1	((unsigned) DEADBEEF1)
#else
#define POISON1	((unsigned) 0xdeafbead)
#endif
#define POISON_SIZE	64

uint32_t
poison_value(void *v)
{
	ulong l = (u_long)v;

	l = l >> PAGE_SHIFT;

	return (l & 1) ? POISON0 : POISON1;
}

void
poison_mem(void *v, size_t len)
{
	uint32_t *ip = v;
	size_t i;
	uint32_t poison;

	poison = poison_value(v);

	if (len > POISON_SIZE)
		len = POISON_SIZE;
	len = len / sizeof(*ip);
	for (i = 0; i < len; i++)
		ip[i] = poison;
}

int
poison_check(void *v, size_t len, size_t *pidx, uint32_t *pval)
{
	uint32_t *ip = v;
	size_t i;
	uint32_t poison;

	poison = poison_value(v);

	if (len > POISON_SIZE)
		len = POISON_SIZE;
	len = len / sizeof(*ip);
	for (i = 0; i < len; i++) {
		if (ip[i] != poison) {
			*pidx = i;
			*pval = poison;
			return 1;
		}
	}
	return 0;
}


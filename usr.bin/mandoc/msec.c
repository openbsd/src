/* $OpenBSD: msec.c,v 1.14 2026/10/02 16:43:17 schwarze Exp $ */
/*
 * Copyright (c) 2026 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2009 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <stdio.h>
#include <string.h>

#include "mandoc.h"
#include "libmandoc.h"

#define LINE(x, y) \
	if (0 == strcmp(p, x)) return(y);

static const char	*a2msec_internal(const char *);


const char *
mandoc_a2msec(const char *sec_full)
{
	const char	*vol_title;
	char		 sec_short[2];

	vol_title = a2msec_internal(sec_full);
	if (vol_title == NULL) {
		sec_short[0] = sec_full[0];
		sec_short[1] = '\0';
		vol_title = a2msec_internal(sec_short);
	}
	return vol_title;
}

static const char *
a2msec_internal(const char *p)
{

#include "msec.in"

	return NULL;
}

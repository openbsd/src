/*	$OpenBSD: strtof.c,v 1.1 2008/06/13 21:04:24 landry Exp $ */

/*
 * Copyright (c) 2008 Landry Breuil
 * All rights reserved.
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

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>

float
strtof(const char *s00, char **se)
{
	double	d;

	d = strtod(s00, se);
	if (d > FLT_MAX) {
		errno = ERANGE;
		return (FLT_MAX);
	} else if (d < -FLT_MAX) {
		errno = ERANGE;
		return (-FLT_MAX);
	}
	return ((float) d);
}

/*
 * Copyright (C) 2002  Jakob Schlyter
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $OpenBSD: lcg_test.c,v 1.2 2004/09/28 17:14:04 jakob Exp $ */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include <isc/lcg.h>

int
main(int argc, char **argv) {
	int i, n;
	isc_uint16_t val;
	isc_lcg_t lcg;

	if (argc > 1)
		n = atoi(argv[1]);
	else
		n = 10;

	isc_lcg_init(&lcg);

	for (i=0; i<n; i++) {
		val = isc_lcg_generate16(&lcg);
		printf("%06d\n", val);
	}

	return (0);
}

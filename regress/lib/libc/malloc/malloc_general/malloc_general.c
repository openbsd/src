/*	$OpenBSD	*/
/*
 * Copyright (c) 2017 Otto Moerbeek <otto@drijf.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* $define VERBOSE */

#define N 1000

size_t size(void)
{
	int p = arc4random_uniform(13) + 3;
	return arc4random_uniform(1 << p);
}

void *a[N];

extern char *malloc_options;

int
main(int argc, char *argv[])
{
	int count, p, i;
	void * q;
	size_t sz;

	if (argc == 1)
		errx(1, "usage: malloc_options");

	malloc_options = argv[1];

	for (count = 0; count < 800000; count++) {
		if (count % 10000 == 0) {
			printf(".");
			fflush(stdout);
		}
		p = arc4random_uniform(2);
		i = arc4random_uniform(N);
		switch (p) {
		case 0:
			if (a[i]) {
#ifdef VERBOSE
				printf("F %p\n", a[i]);
#endif
				free(a[i]);
				a[i] = NULL;
			}
			sz = size();
#ifdef VERBOSE
			printf("M %zu=", sz);
#endif
			a[i] = malloc(sz);
#ifdef VERBOSE
			printf("%p\n", a[i]);
#endif
			if (a[i])
				memset(a[i], 0xff, sz);
			break;
		case 1:
			sz = size();
#ifdef VERBOSE
			printf("R %p %zu=", a[i], sz);
#endif
			q = realloc(a[i], sz);
#ifdef VERBOSE
			printf("%p\n", q);
#endif
			if (q) {
				a[i]= q;
				if (a[i])
					memset(a[i], 0xff, sz);
			}
			break;
		}
	}
	for (i = 0; i < N; i++)
		free(a[i]);
	printf("\n");
	return 0;
}

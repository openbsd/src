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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* $define VERBOSE */

#define N 1000

size_t
size(void)
{
	int p = arc4random_uniform(13) + 3;
	return arc4random_uniform(1 << p);
}

struct { void *p; size_t sz; } a[N];

extern char *malloc_options;

void
fill(u_char *p, size_t sz)
{
	size_t i;

	for (i = 0; i < sz; i++)
		p[i] = i % 256;
}

void
check(u_char *p, size_t sz)
{
	size_t i;

	for (i = 0; i < sz; i++)
		if (p[i] != i % 256)
			errx(1, "check");
}

int
main(int argc, char *argv[])
{
	int count, p, i;
	void * q;
	size_t sz;
	char mo[20];

	if (argc == 1)
		errx(1, "usage: malloc_options");

	/* first reset flags that might be set by env or sysctl */
	strlcpy(mo, "cfgju", sizeof(mo));
	strlcat(mo, argv[1], sizeof(mo));
	malloc_options = mo;

	for (count = 0; count < 800000; count++) {
		if (count % 10000 == 0) {
			printf(".");
			fflush(stdout);
		}
		p = arc4random_uniform(2);
		i = arc4random_uniform(N);
		switch (p) {
		case 0:
			if (a[i].p) {
#ifdef VERBOSE
				printf("F %p\n", a[i].p);
#endif
				if (a[i].p)
					check(a[i].p, a[i].sz);
				free(a[i].p);
				a[i].p = NULL;
			}
			sz = size();
#ifdef VERBOSE
			printf("M %zu=", sz);
#endif
			a[i].p = malloc(sz);
			a[i].sz = sz;
#ifdef VERBOSE
			printf("%p\n", a[i].p);
#endif
			if (a[i].p)
				fill(a[i].p, sz);
			break;
		case 1:
			sz = size();
#ifdef VERBOSE
			printf("R %p %zu=", a[i].p, sz);
#endif
			q = realloc(a[i].p, sz);
#ifdef VERBOSE
			printf("%p\n", q);
#endif
			if (a[i].p && q)
				check(q, a[i].sz < sz ? a[i].sz : sz);
			if (q) {
				a[i].p = q;
				a[i].sz = sz;
				fill(a[i].p, sz);
			}
			break;
		}
	}
	for (i = 0; i < N; i++) {
		if (a[i].p)
			check(a[i].p, a[i].sz);
		free(a[i].p);
	}
	printf("\n");
	return 0;
}

/*
 * Copyright (c) 2017 Todd C. Miller <Todd.Miller@courtesan.com>
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
#include <setjmp.h>
#include <math.h>
#include <err.h>

/*
 * Test program based on Bentley & McIlroy's "Engineering a Sort Function".
 * Uses heapsort(3) to check the results.
 */

enum distribution {
    SAWTOOTH,
    RAND,
    STAGGER,
    PLATEAU,
    SHUFFLE,
    INVALID
};

#define min(a, b)	(((a) < (b)) ? (a) : (b))

static size_t compares;
static size_t max_compares;
static size_t abrt_compares;
static sigjmp_buf cmpjmp;

static int
cmp(const void *v1, const void *v2)
{
	const int a = *(const int *)v1;
	const int b = *(const int *)v2;

	return a > b ? 1 : a < b ? -1 : 0;
}

static int
cmp_checked(const void *v1, const void *v2)
{
	const int a = *(const int *)v1;
	const int b = *(const int *)v2;

	compares++;
	if (compares > abrt_compares)
		siglongjmp(cmpjmp, 1);
	return a > b ? 1 : a < b ? -1 : 0;
}

static int
check_result(char *prefix, int *got, int *expected, enum distribution dist,
    int m, int n)
{
	int i;

	if (compares > max_compares) {
		warnx("%s: %zu compares, max %zu, dist %d, m: %d, n: %d",
		    prefix, compares, max_compares, dist, m, n);
	}

	for (i = 0; i < n; i++) {
		if (got[i] != expected[i]) {
			warnx("%s: failure at %d, dist %d, m: %d, n: %d",
			    prefix, i, dist, m, n);
			return 1;
		}
	}
	return 0;
}

static void
fill_test_array(int *x, int n, int dist, int m)
{
	int i, j, k;

	for (i = 0, j = 0, k = 1; i < n; i++) {
		switch (dist) {
		case SAWTOOTH:
			x[i] = i % m;
			break;
		case RAND:
			x[i] = arc4random_uniform(m);
			break;
		case STAGGER:
			x[i] = (i * m + i) % n;
			break;
		case PLATEAU:
			x[i] = min(i, m);
			break;
		case SHUFFLE:
			x[i] = arc4random_uniform(m) ? (j += 2) : (k += 2);
			break;
		default:
			err(1, "unexpected distribution %d", dist);
		}
	}
}

static int
test_distribution(int dist, int m, int n, int *x, int *y, int *z)
{
	int i, j;
	int ret = 0;

	/* Fill in x[] based on distribution and 'm' */
	fill_test_array(x, n, dist, m);

	/* Test on copy of x[] */
	for (i = 0; i < n; i++)
		y[i] = z[i] = x[i];
	compares = 0;
	if (sigsetjmp(cmpjmp, 1) != 0) {
		warnx("qsort aborted: %zu compares, dist %d, m: %d, n: %d",
		    compares, dist, m, n);
		ret = 1;
	} else {
		qsort(y, n, sizeof(y[0]), cmp_checked);
		heapsort(z, n, sizeof(z[0]), cmp);
		if (check_result("copy", y, z, dist, m, n) != 0)
			ret = 1;
	}

	/* Test on reversed copy of x[] */
	for (i = 0, j = n - 1; i < n; i++, j--)
		y[i] = z[i] = x[j];
	compares = 0;
	if (sigsetjmp(cmpjmp, 1) != 0) {
		warnx("qsort aborted (%s): %zu compares, dist %d, m: %d, n: %d",
		    "reversed", compares, dist, m, n);
		ret = 1;
	} else {
		qsort(y, n, sizeof(y[0]), cmp_checked);
		heapsort(z, n, sizeof(z[0]), cmp);
		if (check_result("reversed", y, z, dist, m, n) != 0)
			ret = 1;
	}

	/* Test with front half of x[] reversed */
	for (i = 0, j = (n / 2) - 1; i < n / 2; i++, j--)
		y[i] = z[i] = x[j];
	for (; i < n; i++)
		y[i] = z[i] = x[i];
	compares = 0;
	if (sigsetjmp(cmpjmp, 1) != 0) {
		warnx("qsort aborted (%s): %zu compares, dist %d, m: %d, n: %d",
		    "front reversed", compares, dist, m, n);
		ret = 1;
	} else {
		qsort(y, n, sizeof(y[0]), cmp_checked);
		heapsort(z, n, sizeof(z[0]), cmp);
		if (check_result("front reversed", y, z, dist, m, n) != 0)
			ret = 1;
	}

	/* Test with back half of x[] reversed */
	for (i = 0; i < n / 2; i++)
		y[i] = z[i] = x[i];
	for (j = n - 1; i < n; i++, j--)
		y[i] = z[i] = x[j];
	compares = 0;
	if (sigsetjmp(cmpjmp, 1) != 0) {
		warnx("qsort aborted (%s): %zu compares, dist %d, m: %d, n: %d",
		    "back reversed", compares, dist, m, n);
		ret = 1;
	} else {
		qsort(y, n, sizeof(y[0]), cmp_checked);
		heapsort(z, n, sizeof(z[0]), cmp);
		if (check_result("back reversed", y, z, dist, m, n) != 0)
			ret = 1;
	}

	/* Test on sorted copy of x[] */
	heapsort(x, n, sizeof(x[0]), cmp);
	for (i = 0; i < n; i++)
		y[i] = x[i];
	compares = 0;
	if (sigsetjmp(cmpjmp, 1) != 0) {
		warnx("qsort aborted (%s): %zu compares, dist %d, m: %d, n: %d",
		    "sorted", compares, dist, m, n);
		ret = 1;
	} else {
		qsort(y, n, sizeof(y[0]), cmp_checked);
		if (check_result("sorted", y, x, dist, m, n) != 0)
			ret = 1;
	}

	/* Test with i%5 added to x[i] (dither) */
	for (i = 0, j = n - 1; i < n; i++, j--)
		y[i] = z[i] = x[j] + (i % 5);
	compares = 0;
	if (sigsetjmp(cmpjmp, 1) != 0) {
		warnx("qsort aborted (%s): %zu compares, dist %d, m: %d, n: %d",
		    "dithered", compares, dist, m, n);
		ret = 1;
	} else {
		qsort(y, n, sizeof(y[0]), cmp_checked);
		heapsort(z, n, sizeof(z[0]), cmp);
		if (check_result("dithered", y, z, dist, m, n) != 0)
			ret = 1;
	}

	return ret;
}

static int
run_tests(int m, int n)
{
	int *x, *y, *z;
	int ret = 0;
	double nlgn;
	enum distribution dist;

	/*
	 * The expected number of compares is A * n * log2(n)
	 * where A is between 1 and 2.  If A is greater than 1.5
	 * we'll print a warning.  If A is greater than 10 we
	 * abort the test since qsort has probably gone quadratic.
	 */
	nlgn = n * log2(n);
	max_compares = nlgn * 1.5;
	abrt_compares = nlgn * 10;

	x = reallocarray(NULL, n, sizeof(x[0]));
	y = reallocarray(NULL, n, sizeof(y[0]));
	z = reallocarray(NULL, n, sizeof(z[0]));
	if (y == NULL || y == NULL || z == NULL)
		err(1, NULL);

	for (dist = SAWTOOTH; dist != INVALID; dist++) {
		if (test_distribution(dist, m, n, x, y, z) != 0)
			ret = 1;
	}

	free(x);
	free(y);
	free(z);
	return ret;
}

int
main(int argc, char *argv[])
{
	int *nn, nums[] = { 100, 1023, 1024, 1025, 4095, 4096, 4097, -1 };
	int m, n;
	int ret = 0;

	for (nn = nums; (n = *nn) > 0; nn++) {
		for (m = 1; m < 2 * n; m *= 2) {
			if (run_tests(m, n) != 0)
				ret = 1;
		}
	}

	return ret;
}

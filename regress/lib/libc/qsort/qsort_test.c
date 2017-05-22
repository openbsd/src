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

#include <sys/time.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <time.h>
#include <unistd.h>
#include <err.h>

/*
 * Test program based on Bentley & McIlroy's "Engineering a Sort Function".
 * Uses mergesort(3) to check the results.
 *
 * Additional "killer" input taken from:
 *  David R. Musser's "Introspective Sorting and Selection Algorithms"
 *  http://calmerthanyouare.org/2014/06/11/algorithmic-complexity-attacks-and-libc-qsort.html
 *  M. D. McIlroy's "A Killer Adversary for Quicksort"
 */

struct test_distribution {
	const char *name;
	void (*fill)(int *x, int n, int m);
	int (*test)(struct test_distribution *d, int n, int *x, int *y, int *z);
};

#define min(a, b)	(((a) < (b)) ? (a) : (b))

static size_t compares;
static size_t max_compares;
static size_t abrt_compares;
static sigjmp_buf cmpjmp;
static bool dump_table, timing, verbose;

extern int antiqsort(int n, int *a, int *ptr);

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
check_result(char *sub, int *got, int *expected, struct test_distribution *d,
    int m, int n)
{
	int i;

	if (verbose || compares > max_compares) {
		if (sub != NULL) {
			if (m != 0) {
				warnx("%s (%s): m: %d, n: %d, %zu compares, "
				    "max %zu(%zu)", d->name, sub, m, n,
				    compares, max_compares, abrt_compares);
			} else {
				warnx("%s (%s): n: %d, %zu compares, "
				    "max %zu(%zu)", d->name, sub, n,
				    compares, max_compares, abrt_compares);
			}
		} else {
			if (m != 0) {
				warnx("%s: m: %d, n: %d, %zu compares, "
				    "max %zu(%zu)", d->name, m, n,
				    compares, max_compares, abrt_compares);
			} else {
				warnx("%s: n: %d, %zu compares, "
				    "max %zu(%zu)", d->name, n,
				    compares, max_compares, abrt_compares);
			}
		}
	}

	for (i = 0; i < n; i++) {
		if (got[i] != expected[i])
			break;
	}
	if (i == n)
		return 0;

	if (sub != NULL) {
		warnx("%s (%s): failure at %d, m: %d, n: %d",
		    d->name, sub, i, m, n);
	} else {
		warnx("%s: failure at %d, m: %d, n: %d",
		    d->name, i, m, n);
	}
	return 1;
}

static void
fill_sawtooth(int *x, int n, int m)
{
	int i;

	for (i = 0; i < n; i++)
		x[i] = i % m;
}

static void
fill_random(int *x, int n, int m)
{
	int i;

	for (i = 0; i < n; i++)
		x[i] = arc4random_uniform(m);
}

static void
fill_stagger(int *x, int n, int m)
{
	int i;

	for (i = 0; i < n; i++)
		x[i] = (i * m + i) % n;
}

static void
fill_plateau(int *x, int n, int m)
{
	int i;

	for (i = 0; i < n; i++)
		x[i] = min(i, m);
}

static void
fill_shuffle(int *x, int n, int m)
{
	int i, j, k;

	for (i = 0, j = 0, k = 1; i < n; i++)
		x[i] = arc4random_uniform(m) ? (j += 2) : (k += 2);
}

static void
fill_bsd_killer(int *x, int n, int m)
{
	int i, k;

	/* 4.4BSD insertion sort optimization killer, Mats Linander */
	k = n / 2;
	for (i = 0; i < n; i++) {
		if (i < k)
			x[i] = k - i;
		else if (i > k)
			x[i] = n + k + 1 - i;
		else
			x[i] = k + 1;
	}
}

static void
fill_med3_killer(int *x, int n, int m)
{
	int i, k;

	/*
	 * Median of 3 killer, as seen in David R Musser's
	 * "Introspective Sorting and Selection Algorithms"
	 */
	if (n & 1) {
		/* odd size, store last at end and make even. */
		x[n - 1] = n;
		n--;
	}
	k = n / 2;
	for (i = 0; i < n; i++) {
		if (i & 1) {
			x[i] = k + x[i - 1];
		} else {
			x[i] = i + 1;
		}
	}
}

static void
print_timing(struct test_distribution *d, char *sub, int m, int n, double elapsed)
{
	if (sub != NULL) {
		if (m != 0) {
			warnx("%s (%s): m: %d, n: %d, %f seconds",
			    d->name, sub, m, n, elapsed);
		} else {
			warnx("%s (%s): n: %d, %f seconds",
			    d->name, sub, n, elapsed);
		}
	} else {
		if (m != 0) {
			warnx("%s: m: %d, n: %d, %f seconds",
			    d->name, m, n, elapsed);
		} else {
			warnx("%s: n: %d, %f seconds",
			    d->name, n, elapsed);
		}
	}
}

static int
do_test(struct test_distribution *d, char *sub, int m, int n, int *y, int *z)
{
	int ret = 0;
	struct timespec before, after;

	compares = 0;
	if (sigsetjmp(cmpjmp, 1) != 0) {
		if (sub != NULL) {
			warnx("%s (%s): qsort aborted after %zu compares, m: %d, n: %d",
			    d->name, sub, compares, m, n);
		} else {
			warnx("%s: qsort aborted after %zu compares, m: %d, n: %d",
			    d->name, compares, m, n);
		}
		ret = 1;
	} else {
		if (timing)
			clock_gettime(CLOCK_MONOTONIC, &before);
		qsort(y, n, sizeof(y[0]), cmp_checked);
		if (timing) {
			double elapsed;
			clock_gettime(CLOCK_MONOTONIC, &after);
			timespecsub(&after, &before, &after);
			elapsed = after.tv_sec + after.tv_nsec / 1000000000.0;
			print_timing(d, sub, m, n, elapsed);
		}
		if (check_result(sub, y, z, d, m, n) != 0)
			ret = 1;
	}
	return ret;
}

static int
test_perturbed(struct test_distribution *d, int n, int *x, int *y, int *z)
{
	int i, j, m;
	int ret = 0;

	for (m = 1; m < 2 * n; m *= 2) {
		/* Fill in x[n] modified by m */
		d->fill(x, n, m);

		/* Test on copy of x[] */
		for (i = 0; i < n; i++)
			y[i] = z[i] = x[i];
		if (mergesort(z, n, sizeof(z[0]), cmp) != 0)
			err(1, NULL);
		if (do_test(d, "copy", m, n, y, z) != 0)
			ret = 1;

		/* Test on reversed copy of x[] */
		for (i = 0, j = n - 1; i < n; i++, j--)
			y[i] = z[i] = x[j];
		if (mergesort(z, n, sizeof(z[0]), cmp) != 0)
			err(1, NULL);
		if (do_test(d, "reversed", m, n, y, z) != 0)
			ret = 1;

		/* Test with front half of x[] reversed */
		for (i = 0, j = (n / 2) - 1; i < n / 2; i++, j--)
			y[i] = z[i] = x[j];
		for (; i < n; i++)
			y[i] = z[i] = x[i];
		if (mergesort(z, n, sizeof(z[0]), cmp) != 0)
			err(1, NULL);
		if (do_test(d, "front reversed", m, n, y, z) != 0)
			ret = 1;

		/* Test with back half of x[] reversed */
		for (i = 0; i < n / 2; i++)
			y[i] = z[i] = x[i];
		for (j = n - 1; i < n; i++, j--)
			y[i] = z[i] = x[j];
		if (mergesort(z, n, sizeof(z[0]), cmp) != 0)
			err(1, NULL);
		if (do_test(d, "back reversed", m, n, y, z) != 0)
			ret = 1;

		/* Test on sorted copy of x[] */
		if (mergesort(x, n, sizeof(x[0]), cmp) != 0)
			err(1, NULL);
		for (i = 0; i < n; i++)
			y[i] = x[i];
		if (do_test(d, "sorted", m, n, y, x) != 0)
			ret = 1;

		/* Test with i%5 added to x[i] (dither) */
		for (i = 0, j = n - 1; i < n; i++, j--)
			y[i] = z[i] = x[j] + (i % 5);
		if (mergesort(z, n, sizeof(z[0]), cmp) != 0)
			err(1, NULL);
		if (do_test(d, "front reversed", m, n, y, z) != 0)
			ret = 1;
	}

	return ret;
}

/*
 * Like test_perturbed() but because the input is tailored to cause
 * quicksort to go quadratic we don't perturb the input.
 */
static int
test_simple(struct test_distribution *d, int n, int *x, int *y, int *z)
{
	int i, ret = 0;

	/* Fill in x[n] */
	d->fill(x, n, 0);

	/* Make a copy of x[] */
	for (i = 0; i < n; i++)
		y[i] = z[i] = x[i];
	if (mergesort(z, n, sizeof(z[0]), cmp) != 0)
		err(1, NULL);
	if (do_test(d, NULL, 0, n, y, z) != 0)
		ret = 1;

	return ret;
}

/*
 * Use D. McIlroy's "Killer Adversary for Quicksort"
 * We don't compare the results in this case.
 */
static int
test_antiqsort(struct test_distribution *d, int n, int *x, int *y, int *z)
{
	struct timespec before, after;
	int i, ret = 0;

	/*
	 * We expect antiqsort to generate > 1.5 * nlgn compares.
	 * If introspection is not used, it will be > 10 * nlgn compares.
	 */
	if (timing)
		clock_gettime(CLOCK_MONOTONIC, &before);
	i = antiqsort(n, x, y);
	if (timing)
		clock_gettime(CLOCK_MONOTONIC, &after);
	if (i > abrt_compares)
		ret = 1;
	if (dump_table) {
		printf("/* %d items, %d compares */\n", n, i);
		printf("static const int table_%d[] = {", n);
		for (i = 0; i < n - 1; i++) {
			if ((i % 12) == 0)
				printf("\n\t");
			printf("%4d, ", x[i]);
		}
		printf("%4d\n};\n", x[i]);
	} else {
		if (timing) {
			double elapsed;
			timespecsub(&after, &before, &after);
			elapsed = after.tv_sec + after.tv_nsec / 1000000000.0;
			print_timing(d, NULL, 0, n, elapsed);
		}
		if (verbose || ret != 0) {
			warnx("%s: n: %d, %d compares, max %zu(%zu)",
			    d->name, n, i, max_compares, abrt_compares);
		}
	}

	return ret;
}

static struct test_distribution distributions[] = {
	{ "sawtooth", fill_sawtooth, test_perturbed },
	{ "random", fill_random, test_perturbed },
	{ "stagger", fill_stagger, test_perturbed },
	{ "plateau", fill_plateau, test_perturbed },
	{ "shuffle", fill_shuffle, test_perturbed },
	{ "bsd_killer", fill_bsd_killer, test_simple },
	{ "med3_killer", fill_med3_killer, test_simple },
	{ "antiqsort", NULL, test_antiqsort },
	{ NULL }
};

static int
run_tests(int n, const char *name)
{
	int *x, *y, *z;
	int i, nlgn = 0;
	int ret = 0;
	struct test_distribution *d;

	/*
	 * We expect A*n*lg(n) compares where A is between 1 and 2.
	 * For A > 1.5, print a warning.
	 * For A > 10 abort the test since qsort has probably gone quadratic.
	 */
	for (i = n - 1; i > 0; i >>= 1)
	    nlgn++;
	nlgn *= n;
	max_compares = nlgn * 1.5;
	abrt_compares = nlgn * 10;

	x = reallocarray(NULL, n, sizeof(x[0]));
	y = reallocarray(NULL, n, sizeof(y[0]));
	z = reallocarray(NULL, n, sizeof(z[0]));
	if (x == NULL || y == NULL || z == NULL)
		err(1, NULL);

	for (d = distributions; d->name != NULL; d++) {
		if (name != NULL && strcmp(name, d->name) != 0)
			continue;
		if (d->test(d, n, x, y, z) != 0)
			ret = 1;
	}

	free(x);
	free(y);
	free(z);
	return ret;
}

__dead void
usage(void)
{
        fprintf(stderr, "usage: qsort_test [-dvt] [-n test_name] [num ...]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *nums[] = { "100", "1023", "1024", "1025", "4095", "4096", "4097" };
	int ch, n, ret = 0;
	char *name = NULL;

        while ((ch = getopt(argc, argv, "dn:tv")) != -1) {
                switch (ch) {
                case 'd':
                        dump_table = true;
                        break;
                case 'n':
                        name = optarg;
                        break;
                case 't':
                        timing = true;
                        break;
                case 'v':
                        verbose = true;
                        break;
                default:
                        usage();
                        break;
                }
        }
        argc -= optind;
        argv += optind;

	if (argc == 0) {
		argv = nums;
		argc = sizeof(nums) / sizeof(nums[0]);
	}

	while (argc > 0) {
		n = atoi(*argv);
		if (run_tests(n, name) != 0)
			ret = 1;
		argc--;
		argv++;
	}

	return ret;
}

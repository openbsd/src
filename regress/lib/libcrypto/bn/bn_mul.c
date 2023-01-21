/*	$OpenBSD: bn_mul.c,v 1.1 2023/01/21 13:24:39 jsing Exp $ */
/*
 * Copyright (c) 2023 Joel Sing <jsing@openbsd.org>
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

#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <openssl/bn.h>

static int
benchmark_bn_mul_setup(BIGNUM *a, size_t a_bits, BIGNUM *b, size_t b_bits,
    BIGNUM *r)
{
	if (!BN_rand(a, a_bits - 1, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY))
		return 0;
	if (!BN_rand(b, b_bits - 1, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY))
		return 0;
	if (!BN_set_bit(r, (a_bits + b_bits) - 1))
		return 0;

	return 1;
}

static void
benchmark_bn_mul_run_once(BIGNUM *r, BIGNUM *a, BIGNUM *b, BN_CTX *bn_ctx)
{
	if (!BN_mul(r, a, b, bn_ctx))
		errx(1, "BN_mul");
}

static int
benchmark_bn_sqr_setup(BIGNUM *a, size_t a_bits, BIGNUM *b, size_t b_bits,
    BIGNUM *r)
{
	if (!BN_rand(a, a_bits - 1, BN_RAND_TOP_ONE, BN_RAND_BOTTOM_ANY))
		return 0;
	if (!BN_set_bit(r, (a_bits + a_bits) - 1))
		return 0;

	return 1;
}

static void
benchmark_bn_sqr_run_once(BIGNUM *r, BIGNUM *a, BIGNUM *b, BN_CTX *bn_ctx)
{
	if (!BN_sqr(r, a, bn_ctx))
		errx(1, "BN_sqr");
}

struct benchmark {
	const char *desc;
	int (*setup)(BIGNUM *, size_t, BIGNUM *, size_t, BIGNUM *);
	void (*run_once)(BIGNUM *, BIGNUM *, BIGNUM *, BN_CTX *);
	size_t a_bits;
	size_t b_bits;
};

struct benchmark benchmarks[] = {
	{
		.desc = "BN_mul (128 bit x 128 bit)",
		.setup = benchmark_bn_mul_setup,
		.run_once = benchmark_bn_mul_run_once,
		.a_bits = 128,
		.b_bits = 128,
	},
	{
		.desc = "BN_mul (128 bit x 256 bit)",
		.setup = benchmark_bn_mul_setup,
		.run_once = benchmark_bn_mul_run_once,
		.a_bits = 128,
		.b_bits = 256,
	},
	{
		.desc = "BN_mul (256 bit x 256 bit)",
		.setup = benchmark_bn_mul_setup,
		.run_once = benchmark_bn_mul_run_once,
		.a_bits = 256,
		.b_bits = 256,
	},
	{
		.desc = "BN_mul (512 bit x 512 bit)",
		.setup = benchmark_bn_mul_setup,
		.run_once = benchmark_bn_mul_run_once,
		.a_bits = 512,
		.b_bits = 512,
	},
	{
		.desc = "BN_mul (1024 bit x 1024 bit)",
		.setup = benchmark_bn_mul_setup,
		.run_once = benchmark_bn_mul_run_once,
		.a_bits = 1024,
		.b_bits = 1024,
	},
	{
		.desc = "BN_mul (1024 bit x 2048 bit)",
		.setup = benchmark_bn_mul_setup,
		.run_once = benchmark_bn_mul_run_once,
		.a_bits = 1024,
		.b_bits = 2048,
	},
	{
		.desc = "BN_mul (2048 bit x 2048 bit)",
		.setup = benchmark_bn_mul_setup,
		.run_once = benchmark_bn_mul_run_once,
		.a_bits = 2048,
		.b_bits = 2048,
	},
	{
		.desc = "BN_mul (4096 bit x 4096 bit)",
		.setup = benchmark_bn_mul_setup,
		.run_once = benchmark_bn_mul_run_once,
		.a_bits = 4096,
		.b_bits = 4096,
	},
	{
		.desc = "BN_mul (4096 bit x 8192 bit)",
		.setup = benchmark_bn_mul_setup,
		.run_once = benchmark_bn_mul_run_once,
		.a_bits = 4096,
		.b_bits = 8192,
	},
	{
		.desc = "BN_mul (8192 bit x 8192 bit)",
		.setup = benchmark_bn_mul_setup,
		.run_once = benchmark_bn_mul_run_once,
		.a_bits = 8192,
		.b_bits = 8192,
	},
	{
		.desc = "BN_sqr (128 bit)",
		.setup = benchmark_bn_sqr_setup,
		.run_once = benchmark_bn_sqr_run_once,
		.a_bits = 128,
	},
	{
		.desc = "BN_sqr (256 bit)",
		.setup = benchmark_bn_sqr_setup,
		.run_once = benchmark_bn_sqr_run_once,
		.a_bits = 256,
	},
	{
		.desc = "BN_sqr (512 bit)",
		.setup = benchmark_bn_sqr_setup,
		.run_once = benchmark_bn_sqr_run_once,
		.a_bits = 512,
	},
	{
		.desc = "BN_sqr (1024 bit)",
		.setup = benchmark_bn_sqr_setup,
		.run_once = benchmark_bn_sqr_run_once,
		.a_bits = 1024,
	},
	{
		.desc = "BN_sqr (2048 bit)",
		.setup = benchmark_bn_sqr_setup,
		.run_once = benchmark_bn_sqr_run_once,
		.a_bits = 2048,
	},
	{
		.desc = "BN_sqr (4096 bit)",
		.setup = benchmark_bn_sqr_setup,
		.run_once = benchmark_bn_sqr_run_once,
		.a_bits = 4096,
	},
	{
		.desc = "BN_sqr (8192 bit)",
		.setup = benchmark_bn_sqr_setup,
		.run_once = benchmark_bn_sqr_run_once,
		.a_bits = 8192,
	},
};

#define N_BENCHMARKS (sizeof(benchmarks) / sizeof(benchmarks[0]))

static volatile sig_atomic_t benchmark_stop;

static void
benchmark_sig_alarm(int sig)
{
	benchmark_stop = 1;
}

static void
benchmark_run(const struct benchmark *bm, int seconds)
{
	struct timespec start, end, duration;
	BIGNUM *a, *b, *r;
	BN_CTX *bn_ctx;
	int i;

	signal(SIGALRM, benchmark_sig_alarm);

	if ((bn_ctx = BN_CTX_new()) == NULL)
		errx(1, "BN_CTX_new");

	BN_CTX_start(bn_ctx);

	if ((a = BN_CTX_get(bn_ctx)) == NULL)
		errx(1, "BN_CTX_get");
	if ((b = BN_CTX_get(bn_ctx)) == NULL)
		errx(1, "BN_CTX_get");
	if ((r = BN_CTX_get(bn_ctx)) == NULL)
		errx(1, "BN_CTX_get");

	if (!bm->setup(a, bm->a_bits, b, bm->b_bits, r))
		errx(1, "benchmark setup failed");

	benchmark_stop = 0;
	i = 0;
	alarm(seconds);

	clock_gettime(CLOCK_MONOTONIC, &start);

	fprintf(stderr, "Benchmarking %s for %ds: ", bm->desc, seconds);
	while (!benchmark_stop) {
		bm->run_once(r, a, b, bn_ctx);
		i++;
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
	timespecsub(&end, &start, &duration);
	fprintf(stderr, "%d iterations in %f seconds\n", i,
	    duration.tv_sec + duration.tv_nsec / 1000000000.0);

	BN_CTX_end(bn_ctx);
	BN_CTX_free(bn_ctx);
}

static void
benchmark_bn_mul_sqr(void)
{
	const struct benchmark *bm;
	size_t i;

	for (i = 0; i < N_BENCHMARKS; i++) {
		bm = &benchmarks[i];
		benchmark_run(bm, 5);
	}
}

int
main(int argc, char **argv)
{
	int benchmark = 0, failed = 0;

	if (argc == 2 && strcmp(argv[1], "--benchmark") == 0)
		benchmark = 1;

	if (benchmark && !failed)
		benchmark_bn_mul_sqr();

	return failed;
}

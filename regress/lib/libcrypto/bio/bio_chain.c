/*	$OpenBSD: bio_chain.c,v 1.3 2022/12/08 18:12:39 tb Exp $	*/
/*
 * Copyright (c) 2022 Theo Buehler <tb@openbsd.org>
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
#include <string.h>

#include <openssl/bio.h>

#include "bio_local.h"

#define N_CHAIN_BIOS	5

static BIO *
BIO_prev(BIO *bio)
{
	if (bio == NULL)
		return NULL;

	return bio->prev_bio;
}

static int
bio_chain_pop_test(void)
{
	BIO *bio[N_CHAIN_BIOS];
	BIO *prev, *next;
	size_t i, j;
	int failed = 1;

	for (i = 0; i < N_CHAIN_BIOS; i++) {
		memset(bio, 0, sizeof(bio));
		prev = NULL;

		/* Create a linear chain of BIOs. */
		for (j = 0; j < N_CHAIN_BIOS; j++) {
			if ((bio[j] = BIO_new(BIO_s_null())) == NULL)
				errx(1, "BIO_new");
			if ((prev = BIO_push(prev, bio[j])) == NULL)
				errx(1, "BIO_push");
		}

		/* Check that the doubly-linked list was set up as expected. */
		if (BIO_prev(bio[0]) != NULL) {
			fprintf(stderr,
			    "i = %zu: first BIO has predecessor\n", i);
			goto err;
		}
		if (BIO_next(bio[N_CHAIN_BIOS - 1]) != NULL) {
			fprintf(stderr, "i = %zu: last BIO has successor\n", i);
			goto err;
		}
		for (j = 0; j < N_CHAIN_BIOS; j++) {
			if (j > 0) {
				if (BIO_prev(bio[j]) != bio[j - 1]) {
					fprintf(stderr, "i = %zu: "
					    "BIO_prev(bio[%zu]) != bio[%zu]\n",
					    i, j, j - 1);
					goto err;
				}
			}
			if (j < N_CHAIN_BIOS - 1) {
				if (BIO_next(bio[j]) != bio[j + 1]) {
					fprintf(stderr, "i = %zu: "
					    "BIO_next(bio[%zu]) != bio[%zu]\n",
					    i, j, j + 1);
					goto err;
				}
			}
		}

		/* Drop the ith bio from the chain. */
		next = BIO_pop(bio[i]);

		if (BIO_prev(bio[i]) != NULL || BIO_next(bio[i]) != NULL) {
			fprintf(stderr,
			    "BIO_pop() didn't isolate bio[%zu]\n", i);
			goto err;
		}

		if (i < N_CHAIN_BIOS - 1) {
			if (next != bio[i + 1]) {
				fprintf(stderr, "BIO_pop(bio[%zu]) did not "
				    "return bio[%zu]\n", i, i + 1);
				goto err;
			}
		} else {
			if (next != NULL) {
				fprintf(stderr, "i = %zu: "
				    "BIO_pop(last) != NULL\n", i);
				goto err;
			}
		}

		/*
		 * Walk the remainder of the chain and see if the doubly linked
		 * list checks out.
		 */
		if (i == 0) {
			prev = bio[1];
			j = 2;
		} else {
			prev = bio[0];
			j = 1;
		}

		for (; j < N_CHAIN_BIOS; j++) {
			if (j == i)
				continue;
			if (BIO_next(prev) != bio[j]) {
				fprintf(stderr, "i = %zu, j = %zu: "
				    "BIO_next(prev) != bio[%zu]\n", i, j, j);
				goto err;
			}
			if (BIO_prev(bio[j]) != prev) {
				fprintf(stderr, "i = %zu, j = %zu: "
				    "BIO_prev(bio[%zu]) != prev\n", i, j, j);
				goto err;
			}
			prev = bio[j];
		}

		if (BIO_next(prev) != NULL) {
			fprintf(stderr, "i = %zu: BIO_next(prev) != NULL\n", i);
			goto err;
		}

		for (j = 0; j < N_CHAIN_BIOS; j++)
			BIO_free(bio[j]);
		memset(bio, 0, sizeof(bio));
	}

	failed = 0;

 err:
	for (i = 0; i < N_CHAIN_BIOS; i++)
		BIO_free(bio[i]);

	return failed;
}

static int
walk_forward(BIO *start, BIO *end, size_t expected_length,
    size_t i, size_t j, const char *fn, const char *description)
{
	BIO *prev, *next;
	size_t length;
	int ret = 0;

	if (start == NULL || end == NULL)
		goto done;

	next = start;
	length = 0;

	do {
		prev = next;
		next = BIO_next(prev);
		length++;
	} while (next != NULL);

	if (prev != end) {
		fprintf(stderr, "%s case (%zu, %zu) %s has unexpected end\n",
		    fn, i, j, description);
		goto err;
	}

	if (length != expected_length) {
		fprintf(stderr, "%s case (%zu, %zu) %s length "
		    "(walking forward) want: %zu, got %zu\n",
		    fn, i, j, description, expected_length, length);
		goto err;
	}

 done:
	ret = 1;

 err:
	return ret;
}

static int
walk_backward(BIO *start, BIO *end, size_t expected_length,
    size_t i, size_t j, const char *fn, const char *description)
{
	BIO *prev, *next;
	size_t length;
	int ret = 0;

	if (start == NULL || end == NULL)
		goto done;

	length = 0;
	prev = end;
	do {
		next = prev;
		prev = BIO_prev(prev);
		length++;
	} while (prev != NULL);

	if (next != start) {
		fprintf(stderr, "%s case (%zu, %zu) %s has unexpected start\n",
		    fn, i, j, description);
		goto err;
	}

	if (length != expected_length) {
		fprintf(stderr, "%s case (%zu, %zu) %s length "
		    "(walking backward) want: %zu, got %zu\n",
		    fn, i, j, description, expected_length, length);
		goto err;
	}

 done:
	ret = 1;

 err:
	return ret;
}

static int
check_chain(BIO *start, BIO *end, size_t expected_length,
    size_t i, size_t j, const char *fn, const char *description)
{
	if (!walk_forward(start, end, expected_length, i, j, fn, description))
		return 0;
	if (!walk_backward(start, end, expected_length, i, j, fn, description))
		return 0;

	return 1;
}

/*
 * Link two linear chains of BIOs, A[] and B[], of length N_CHAIN_BIOS together
 * using either BIO_push(A[i], B[j]) or BIO_set_next(A[i], B[j]).
 *
 * BIO_push() first walks the chain A[] to its end and then appends the tail
 * of chain B[] starting at B[j]. If j > 0, we get two chains
 *
 *     A[0] -- ... -- A[N_CHAIN_BIOS - 1] -- B[j] -- ... -- B[N_CHAIN_BIOS - 1]
 *                                         `- link created by BIO_push()
 *     B[0] -- ... -- B[j-1]
 *       |<-- oldhead -->|
 *
 * of lengths N_CHAIN_BIOS + N_CHAIN_BIOS - j and j, respectively.
 * If j == 0, the second chain (oldhead) is empty. One quirk of BIO_push() is
 * that the outcome of BIO_push(A[i], B[j]) apart from the return value is
 * independent of i.
 *
 * Prior to bio_lib.c r1.41, BIO_push(A[i], B[j]) would fail to dissociate the
 * two chains and leave B[j] with two parents for 0 < j < N_CHAIN_BIOS.
 * B[j]->prev_bio would point at A[N_CHAIN_BIOS - 1], while both B[j - 1] and
 * A[N_CHAIN_BIOS - 1] would point at B[j]. In particular, BIO_free_all(A[0])
 * followed by BIO_free_all(B[0]) results in a double free of B[j].
 *
 * The result for BIO_set_next() is different: three chains are created.
 *
 *                                 |--- oldtail -->
 *     ... -- A[i-1] -- A[i] -- A[i+1] -- ...
 *                         \
 *                          \  link created by BIO_set_next()
 *     --- oldhead -->|      \
 *          ... -- B[j-1] -- B[j] -- B[j+1] -- ...
 *
 * After creating a new link, the new chain has length i + 1 + N_CHAIN_BIOS - j,
 * oldtail has length N_CHAIN_BIOS - i - 1 and oldhead has length j.
 *
 * Prior to bio_lib.c r1.40, BIO_set_next(A[i], B[j]) results in both A[i] and
 * B[j - 1] pointing at B[j] while B[j] points back at A[i]. The result is
 * again double frees.
 *
 * XXX: Should check that the callback is called on BIO_push() as expected.
 */

static int
link_chains_at(size_t i, size_t j, int use_bio_push)
{
	const char *fn = use_bio_push ? "BIO_push" : "BIO_set_next";
	BIO *A[N_CHAIN_BIOS], *B[N_CHAIN_BIOS];
	BIO *new_start, *new_end, *prev;
	BIO *oldhead_start, *oldhead_end, *oldtail_start, *oldtail_end;
	size_t k, new_length, oldhead_length, oldtail_length;
	int failed = 1;

	memset(A, 0, sizeof(A));
	memset(B, 0, sizeof(B));

	/* Create two linear chains of BIOs. */
	prev = NULL;
	for (k = 0; k < N_CHAIN_BIOS; k++) {
		if ((A[k] = BIO_new(BIO_s_null())) == NULL)
			errx(1, "BIO_new");
		if ((prev = BIO_push(prev, A[k])) == NULL)
			errx(1, "BIO_push");
	}
	prev = NULL;
	for (k = 0; k < N_CHAIN_BIOS; k++) {
		if ((B[k] = BIO_new(BIO_s_null())) == NULL)
			errx(1, "BIO_new");
		if ((prev = BIO_push(prev, B[k])) == NULL)
			errx(1, "BIO_push");
	}

	/*
	 * Set our expectations. ... it's complicated.
	 */

	new_start = A[0];
	new_end = B[N_CHAIN_BIOS - 1];

	oldhead_length = j;
	oldhead_start = B[0];
	oldhead_end = BIO_prev(B[j]);

	/* If we push B[0] or set next to B[0], the oldhead chain is empty. */
	if (j == 0) {
		oldhead_length = 0;
		oldhead_start = NULL;
	}

	if (use_bio_push) {
		new_length = N_CHAIN_BIOS + N_CHAIN_BIOS - j;

		/* oldtail doesn't exist in the BIO_push() case. */
		oldtail_start = NULL;
		oldtail_end = NULL;
		oldtail_length = 0;
	} else {
		new_length = i + 1 + N_CHAIN_BIOS - j;

		oldtail_start = BIO_next(A[i]);
		oldtail_end = A[N_CHAIN_BIOS - 1];
		oldtail_length = N_CHAIN_BIOS - i - 1;
	}

	/*
	 * Now actually push or set next.
	 */

	if (use_bio_push) {
		if (BIO_push(A[i], B[j]) != A[i]) {
			fprintf(stderr, "BIO_push(A[%zu], B[%zu]) != A[%zu]\n",
			    i, j, i);
			goto err;
		}
	} else {
		BIO_set_next(A[i], B[j]);
	}

	/*
	 * Check that all the chains match our expectations.
	 */

	if (!check_chain(new_start, new_end, new_length, i, j, fn, "new chain"))
		goto err;

	if (!check_chain(oldhead_start, oldhead_end, oldhead_length, i, j, fn,
	    "oldhead"))
		goto err;

	if (!check_chain(oldtail_start, oldtail_end, oldtail_length, i, j, fn,
	    "oldtail"))
		goto err;

	/*
	 * All sanity checks passed. We can now free the our chains
	 * with the BIO API without risk of leaks or double frees.
	 */

	BIO_free_all(A[0]);
	BIO_free_all(oldhead_start);
	BIO_free_all(oldtail_start);

	memset(A, 0, sizeof(A));
	memset(B, 0, sizeof(B));

	failed = 0;

 err:
	for (i = 0; i < N_CHAIN_BIOS; i++)
		BIO_free(A[i]);
	for (i = 0; i < N_CHAIN_BIOS; i++)
		BIO_free(B[i]);

	return failed;
}

static int
link_chains(int use_bio_push)
{
	size_t i, j;
	int failure = 0;

	for (i = 0; i < N_CHAIN_BIOS; i++) {
		for (j = 0; j < N_CHAIN_BIOS; j++) {
			failure |= link_chains_at(i, j, use_bio_push);
		}
	}

	return failure;
}

static int
bio_push_link_test(void)
{
	int use_bio_push = 1;

	return link_chains(use_bio_push);
}

static int
bio_set_next_link_test(void)
{
	int use_bio_push = 0;

	return link_chains(use_bio_push);
}

int
main(int argc, char **argv)
{
	int failed = 0;

	failed |= bio_chain_pop_test();
	failed |= bio_push_link_test();
	failed |= bio_set_next_link_test();

	return failed;
}

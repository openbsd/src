/*	$OpenBSD: biotest.c,v 1.11 2022/12/08 11:32:27 tb Exp $	*/
/*
 * Copyright (c) 2014, 2022 Joel Sing <jsing@openbsd.org>
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

#include <sys/types.h>

#include <err.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>

#include "bio_local.h"

struct bio_get_host_ip_test {
	char *input;
	uint32_t ip;
	int ret;
};

struct bio_get_host_ip_test bio_get_host_ip_tests[] = {
	{"", 0, 0},
	{".", 0, 0},
	{"1", 0, 0},
	{"1.2", 0, 0},
	{"1.2.3", 0, 0},
	{"1.2.3.", 0, 0},
	{"1.2.3.4", 0x01020304, 1},
	{"1.2.3.256", 0, 0},
	{"1:2:3::4", 0, 0},
	{"0.0.0.0", INADDR_ANY, 1},
	{"127.0.0.1", INADDR_LOOPBACK, 1},
	{"localhost", INADDR_LOOPBACK, 1},
	{"255.255.255.255", INADDR_BROADCAST, 1},
	{"0xff.0xff.0xff.0xff", 0, 0},
};

#define N_BIO_GET_IP_TESTS \
    (sizeof(bio_get_host_ip_tests) / sizeof(*bio_get_host_ip_tests))

struct bio_get_port_test {
	char *input;
	unsigned short port;
	int ret;
};

struct bio_get_port_test bio_get_port_tests[] = {
	{NULL, 0, 0},
	{"", 0, 0},
	{"-1", 0, 0},
	{"0", 0, 1},
	{"1", 1, 1},
	{"12345", 12345, 1},
	{"65535", 65535, 1},
	{"65536", 0, 0},
	{"999999999999", 0, 0},
	{"xyzzy", 0, 0},
	{"https", 443, 1},
	{"imaps", 993, 1},
	{"telnet", 23, 1},
};

#define N_BIO_GET_PORT_TESTS \
    (sizeof(bio_get_port_tests) / sizeof(*bio_get_port_tests))

static int
do_bio_get_host_ip_tests(void)
{
	struct bio_get_host_ip_test *bgit;
	union {
		unsigned char c[4];
		uint32_t i;
	} ip;
	int failed = 0;
	size_t i;
	int ret;

	for (i = 0; i < N_BIO_GET_IP_TESTS; i++) {
		bgit = &bio_get_host_ip_tests[i];
		memset(&ip, 0, sizeof(ip));

		ret = BIO_get_host_ip(bgit->input, ip.c);
		if (ret != bgit->ret) {
			fprintf(stderr, "FAIL: test %zd (\"%s\") %s, want %s\n",
			    i, bgit->input, ret ? "success" : "failure",
			    bgit->ret ? "success" : "failure");
			failed = 1;
			continue;
		}
		if (ret && ntohl(ip.i) != bgit->ip) {
			fprintf(stderr, "FAIL: test %zd (\"%s\") returned ip "
			    "%x != %x\n", i, bgit->input,
			    ntohl(ip.i), bgit->ip);
			failed = 1;
		}
	}

	return failed;
}

static int
do_bio_get_port_tests(void)
{
	struct bio_get_port_test *bgpt;
	unsigned short port;
	int failed = 0;
	size_t i;
	int ret;

	for (i = 0; i < N_BIO_GET_PORT_TESTS; i++) {
		bgpt = &bio_get_port_tests[i];
		port = 0;

		ret = BIO_get_port(bgpt->input, &port);
		if (ret != bgpt->ret) {
			fprintf(stderr, "FAIL: test %zd (\"%s\") %s, want %s\n",
			    i, bgpt->input, ret ? "success" : "failure",
			    bgpt->ret ? "success" : "failure");
			failed = 1;
			continue;
		}
		if (ret && port != bgpt->port) {
			fprintf(stderr, "FAIL: test %zd (\"%s\") returned port "
			    "%u != %u\n", i, bgpt->input, port, bgpt->port);
			failed = 1;
		}
	}

	return failed;
}

static int
bio_mem_test(void)
{
	uint8_t *data = NULL;
	size_t data_len;
	uint8_t *rodata;
	long rodata_len;
	BUF_MEM *pbuf;
	BUF_MEM *buf = NULL;
	BIO *bio = NULL;
	int ret;
	int failed = 1;

	data_len = 4096;
	if ((data = malloc(data_len)) == NULL)
		err(1, "malloc");

	memset(data, 0xdb, data_len);
	data[0] = 0x01;
	data[data_len - 1] = 0xff;

	if ((bio = BIO_new(BIO_s_mem())) == NULL) {
		fprintf(stderr, "FAIL: BIO_new() returned NULL\n");
		goto failure;
	}
	if ((ret = BIO_write(bio, data, data_len)) != (int)data_len) {
		fprintf(stderr, "FAIL: BIO_write() = %d, want %zu\n", ret,
		    data_len);
		goto failure;
	}
	if ((rodata_len = BIO_get_mem_data(bio, &rodata)) != (long)data_len) {
		fprintf(stderr, "FAIL: BIO_get_mem_data() = %ld, want %zu\n",
		    rodata_len, data_len);
		goto failure;
	}
	if (rodata[0] != 0x01) {
		fprintf(stderr, "FAIL: got 0x%x, want 0x%x\n", rodata[0], 0x01);
		goto failure;
	}
	if (rodata[rodata_len - 1] != 0xff) {
		fprintf(stderr, "FAIL: got 0x%x, want 0x%x\n",
		    rodata[rodata_len - 1], 0xff);
		goto failure;
	}

	if (!BIO_get_mem_ptr(bio, &pbuf)) {
		fprintf(stderr, "FAIL: BIO_get_mem_ptr() failed\n");
		goto failure;
	}
	if (pbuf->length != data_len) {
		fprintf(stderr, "FAIL: Got buffer with length %zu, want %zu\n",
		    pbuf->length, data_len);
		goto failure;
	}
	if (memcmp(pbuf->data, data, data_len) != 0) {
		fprintf(stderr, "FAIL: Got buffer with differing data\n");
		goto failure;
	}
	pbuf = NULL;

	if ((buf = BUF_MEM_new()) == NULL) {
		fprintf(stderr, "FAIL: BUF_MEM_new() returned NULL\n");
		goto failure;
	}
	if (!BIO_set_mem_buf(bio, buf, BIO_NOCLOSE)) {
		fprintf(stderr, "FAIL: BUF_set_mem_buf() failed\n");
		goto failure;
	}
	if ((ret = BIO_puts(bio, "Hello\n")) != 6) {
		fprintf(stderr, "FAIL: BUF_puts() = %d, want %d\n", ret, 6);
		goto failure;
	}
	if ((ret = BIO_puts(bio, "World\n")) != 6) {
		fprintf(stderr, "FAIL: BUF_puts() = %d, want %d\n", ret, 6);
		goto failure;
	}
	if (buf->length != 12) {
		fprintf(stderr, "FAIL: buffer has length %zu, want %d\n",
		    buf->length, 12);
		goto failure;
	}
	buf->length = 11;
	if ((ret = BIO_gets(bio, data, data_len)) != 6) {
		fprintf(stderr, "FAIL: BUF_gets() = %d, want %d\n", ret, 6);
		goto failure;
	}
	if (strcmp(data, "Hello\n") != 0) {
		fprintf(stderr, "FAIL: BUF_gets() returned '%s', want '%s'\n",
		    data, "Hello\\n");
		goto failure;
	}
	if ((ret = BIO_gets(bio, data, data_len)) != 5) {
		fprintf(stderr, "FAIL: BUF_gets() = %d, want %d\n", ret, 5);
		goto failure;
	}
	if (strcmp(data, "World") != 0) {
		fprintf(stderr, "FAIL: BUF_gets() returned '%s', want '%s'\n",
		    data, "World");
		goto failure;
	}

	if (!BIO_eof(bio)) {
		fprintf(stderr, "FAIL: BIO is not EOF\n");
		goto failure;
	}
	if ((ret = BIO_read(bio, data, data_len)) != -1) {
		fprintf(stderr, "FAIL: BIO_read() = %d, want -1\n", ret);
		goto failure;
	}
	if (!BIO_set_mem_eof_return(bio, -2)) {
		fprintf(stderr, "FAIL: BIO_set_mem_eof_return() failed\n");
		goto failure;
	}
	if ((ret = BIO_read(bio, data, data_len)) != -2) {
		fprintf(stderr, "FAIL: BIO_read() = %d, want -2\n", ret);
		goto failure;
	}

	failed = 0;

 failure:
	free(data);
	BUF_MEM_free(buf);
	BIO_free(bio);

	return failed;
}

static int
bio_mem_small_io_test(void)
{
	uint8_t buf[2];
	int i, j, ret;
	BIO *bio;
	int failed = 1;

	memset(buf, 0xdb, sizeof(buf));

	if ((bio = BIO_new(BIO_s_mem())) == NULL) {
		fprintf(stderr, "FAIL: BIO_new() returned NULL\n");
		goto failure;
	}

	for (i = 0; i < 100; i++) {
		if (!BIO_reset(bio)) {
			fprintf(stderr, "FAIL: BIO_reset() failed\n");
			goto failure;
		}
		for (j = 0; j < 25000; j++) {
			ret = BIO_write(bio, buf, sizeof(buf));
			if (ret != sizeof(buf)) {
				fprintf(stderr, "FAIL: BIO_write() = %d, "
				    "want %zu\n", ret, sizeof(buf));
				goto failure;
			}
		}
		for (j = 0; j < 25000; j++) {
			ret = BIO_read(bio, buf, sizeof(buf));
			if (ret != sizeof(buf)) {
				fprintf(stderr, "FAIL: BIO_read() = %d, "
				    "want %zu\n", ret, sizeof(buf));
				goto failure;
			}
			ret = BIO_write(bio, buf, sizeof(buf));
			if (ret != sizeof(buf)) {
				fprintf(stderr, "FAIL: BIO_write() = %d, "
				    "want %zu\n", ret, sizeof(buf));
				goto failure;
			}
		}
		for (j = 0; j < 25000; j++) {
			ret = BIO_read(bio, buf, sizeof(buf));
			if (ret != sizeof(buf)) {
				fprintf(stderr, "FAIL: BIO_read() = %d, "
				    "want %zu\n", ret, sizeof(buf));
				goto failure;
			}
		}
		if (!BIO_eof(bio)) {
			fprintf(stderr, "FAIL: BIO not EOF\n");
			goto failure;
		}
	}

	if (buf[0] != 0xdb || buf[1] != 0xdb) {
		fprintf(stderr, "FAIL: buf = {0x%x, 0x%x}, want {0xdb, 0xdb}\n",
		    buf[0], buf[1]);
		goto failure;
	}

	failed = 0;

 failure:
	BIO_free(bio);

	return failed;
}

static int
bio_mem_readonly_test(void)
{
	uint8_t *data = NULL;
	size_t data_len;
	uint8_t buf[2048];
	BIO *bio = NULL;
	int ret;
	int failed = 1;

	data_len = 4096;
	if ((data = malloc(data_len)) == NULL)
		err(1, "malloc");

	memset(data, 0xdb, data_len);
	data[0] = 0x01;
	data[data_len - 1] = 0xff;

	if ((bio = BIO_new_mem_buf(data, data_len)) == NULL) {
		fprintf(stderr, "FAIL: BIO_new_mem_buf failed\n");
		goto failure;
	}
	if ((ret = BIO_read(bio, buf, 1)) != 1) {
		fprintf(stderr, "FAIL: BIO_read() = %d, want %zu\n", ret,
		    sizeof(buf));
		goto failure;
	}
	if (buf[0] != 0x01) {
		fprintf(stderr, "FAIL: got 0x%x, want 0x%x\n", buf[0], 0x01);
		goto failure;
	}
	if ((ret = BIO_read(bio, buf, sizeof(buf))) != sizeof(buf)) {
		fprintf(stderr, "FAIL: BIO_read() = %d, want %zu\n", ret,
		    sizeof(buf));
		goto failure;
	}
	if (buf[0] != 0xdb) {
		fprintf(stderr, "FAIL: got 0x%x, want 0x%x\n", buf[0], 0xdb);
		goto failure;
	}
	if ((ret = BIO_write(bio, buf, 1)) != -1) {
		fprintf(stderr, "FAIL: BIO_write() = %d, want -1\n", ret);
		goto failure;
	}
	if (BIO_eof(bio)) {
		fprintf(stderr, "FAIL: BIO is EOF\n");
		goto failure;
	}
	if (BIO_ctrl_pending(bio) != 2047) {
		fprintf(stderr, "FAIL: BIO_ctrl_pending() = %zu, want 2047\n",
		    BIO_ctrl_pending(bio));
		goto failure;
	}
	if ((ret = BIO_read(bio, buf, sizeof(buf))) != 2047) {
		fprintf(stderr, "FAIL: BIO_read() = %d, want 2047\n", ret);
		goto failure;
	}
	if (buf[2045] != 0xdb) {
		fprintf(stderr, "FAIL: got 0x%x, want 0x%x\n", buf[2045], 0xdb);
		goto failure;
	}
	if (buf[2046] != 0xff) {
		fprintf(stderr, "FAIL: got 0x%x, want 0x%x\n", buf[2046], 0xff);
		goto failure;
	}
	if (!BIO_eof(bio)) {
		fprintf(stderr, "FAIL: BIO is not EOF\n");
		goto failure;
	}
	if (BIO_ctrl_pending(bio) != 0) {
		fprintf(stderr, "FAIL: BIO_ctrl_pending() = %zu, want 0\n",
		    BIO_ctrl_pending(bio));
		goto failure;
	}

	if (!BIO_reset(bio)) {
		fprintf(stderr, "FAIL: failed to reset bio\n");
		goto failure;
	}
	if (BIO_eof(bio)) {
		fprintf(stderr, "FAIL: BIO is EOF\n");
		goto failure;
	}
	if (BIO_ctrl_pending(bio) != 4096) {
		fprintf(stderr, "FAIL: BIO_ctrl_pending() = %zu, want 4096\n",
		    BIO_ctrl_pending(bio));
		goto failure;
	}
	if ((ret = BIO_read(bio, buf, 2)) != 2) {
		fprintf(stderr, "FAIL: BIO_read() = %d, want 2\n", ret);
		goto failure;
	}
	if (buf[0] != 0x01) {
		fprintf(stderr, "FAIL: got 0x%x, want 0x%x\n", buf[0], 0x01);
		goto failure;
	}
	if (buf[1] != 0xdb) {
		fprintf(stderr, "FAIL: got 0x%x, want 0x%x\n", buf[1], 0xdb);
		goto failure;
	}

	failed = 0;

 failure:
	BIO_free(bio);
	free(data);

	return failed;
}

static int
do_bio_mem_tests(void)
{
	int failed = 0;

	failed |= bio_mem_test();
	failed |= bio_mem_small_io_test();
	failed |= bio_mem_readonly_test();

	return failed;
}

#define N_CHAIN_BIOS	5

static BIO *
BIO_prev(BIO *bio)
{
	if (bio == NULL)
		return NULL;

	return bio->prev_bio;
}

static int
do_bio_chain_pop_test(void)
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

/*
 * Link two linear chains of BIOs, A[], B[] of length N_CHAIN_BIOS together
 * using either BIO_push(A[i], B[j]) or BIO_set_next(A[i], B[j]).
 *
 * BIO_push() first walks the chain A[] to its end and then appends the tail
 * of chain B[] starting at B[j]. If j > 0, we get two chains
 *
 *     A[0] -- ... -- A[N_CHAIN_BIOS - 1] -- B[j] -- ... -- B[N_CHAIN_BIOS - 1]
 *
 *     B[0] -- ... -- B[j-1]
 *     --- oldhead -->|
 *
 * of lengths N_CHAIN_BIOS + N_CHAIN_BIOS - j and j, respectively.
 * If j == 0, the second chain (oldhead) is empty. One quirk of BIO_push() is
 * that the outcome of BIO_push(A[i], B[j]) apart from the return value is
 * independent of i.
 *
 * Prior to bio_lib.c r1.41, BIO_push() would keep * BIO_next(B[j-1]) == B[j]
 * for 0 < j < N_CHAIN_BIOS, and at the same time for all j the inconsistent
 * BIO_prev(B[j]) == A[N_CHAIN_BIOS - 1].
 *
 * The result for BIO_set_next() is different: three chains are created.
 *
 *                                 |--- oldtail -->
 *     ... -- A[i-1] -- A[i] -- A[i+1] -- ...
 *                         \
 *                          \  new link created by BIO_set_next()
 *     --- oldhead -->|      \
 *          ... -- B[j-1] -- B[j] -- B[j+1] -- ...
 *
 * After creating a new link, the new chain has length i + 1 + N_CHAIN_BIOS - j,
 * oldtail has length N_CHAIN_BIOS - i - 1 and oldhead has length j.
 *
 * Prior to bio_lib.c r1.40, BIO_set_next() results in BIO_next(B[j-1]) == B[j],
 * B[j-1] == BIO_prev(B[j]), A[i] == BIO_prev(A[i+1]) and, in addition,
 * BIO_next(A[i]) == B[j], thus leaving A[] and B[] in an inconsistent state.
 *
 * XXX: Should check that the callback is called on BIO_push() as expected.
 */

static int
do_bio_link_chains_at(size_t i, size_t j, int use_bio_push)
{
	BIO *A[N_CHAIN_BIOS], *B[N_CHAIN_BIOS];
	BIO *oldhead_start, *oldhead_end, *oldtail_start, *oldtail_end;
	BIO *prev, *next;
	size_t k, length, new_chain_length, oldhead_length, oldtail_length;
	int failed = 1;

	memset(A, 0, sizeof(A));
	memset(B, 0, sizeof(A));

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

	oldhead_length = j;
	oldhead_start = B[0];
	oldhead_end = BIO_prev(B[j]);

	/*
	 * Adjust for edge case where we push B[0] or set next to B[0].
	 * The oldhead chain is then empty.
	 */

	if (j == 0) {
		if (oldhead_end != NULL) {
			fprintf(stderr, "%s: oldhead(B) not empty at start\n",
			    __func__);
			goto err;
		}

		oldhead_length = 0;
		oldhead_start = NULL;
	}

	if (use_bio_push) {
		new_chain_length = N_CHAIN_BIOS + N_CHAIN_BIOS - j;

		/* oldtail doesn't exist in the BIO_push() case. */
		oldtail_start = NULL;
		oldtail_end = NULL;
		oldtail_length = 0;
	} else {
		new_chain_length = i + 1 + N_CHAIN_BIOS - j;

		oldtail_start = BIO_next(A[i]);
		oldtail_end = A[N_CHAIN_BIOS - 1];
		oldtail_length = N_CHAIN_BIOS - i - 1;

		/*
		 * In case we push onto (or set next at) the end of A[],
		 * the oldtail chain is empty.
		 */
		if (i == N_CHAIN_BIOS - 1) {
			if (oldtail_start != NULL) {
				fprintf(stderr,
				    "%s: oldtail(A) not empty at end\n",
				    __func__);
				goto err;
			}

			oldtail_end = NULL;
			oldtail_length = 0;
		}
	}

	/*
	 * Now actually push or set next.
	 */

	if (use_bio_push) {
		if (BIO_push(A[i], B[j]) != A[i]) {
			fprintf(stderr,
			    "%s: BIO_push(A[%zu], B[%zu]) != A[%zu]\n",
			    __func__, i, j, i);
			goto err;
		}
	} else {
		BIO_set_next(A[i], B[j]);
	}

	/*
	 * Walk the new chain starting at A[0] forward. Check that we reach
	 * B[N_CHAIN_BIOS - 1] and that the new chain's length is as expected.
	 */

	next = A[0];
	length = 0;
	do {
		prev = next;
		next = BIO_next(prev);
		length++;
	} while (next != NULL);

	if (prev != B[N_CHAIN_BIOS - 1]) {
		fprintf(stderr,
		    "%s(%zu, %zu, %d): new chain doesn't end in end of B\n",
		    __func__, i, j, use_bio_push);
		goto err;
	}

	if (length != new_chain_length) {
		fprintf(stderr, "%s(%zu, %zu, %d) unexpected new chain length"
		    " (walking forward). want %zu, got %zu\n",
		    __func__, i, j, use_bio_push, new_chain_length, length);
		goto err;
	}

	/*
	 * Walk the new chain ending in B[N_CHAIN_BIOS - 1] backward.
	 * Check that we reach A[0] and that its length is what we expect.
	 */

	prev = B[N_CHAIN_BIOS - 1];
	length = 0;
	do {
		next = prev;
		prev = BIO_prev(next);
		length++;
	} while (prev != NULL);

	if (next != A[0]) {
		fprintf(stderr,
		    "%s(%zu, %zu, %d): new chain doesn't start at start of A\n",
		    __func__, i, j, use_bio_push);
		goto err;
	}

	if (length != new_chain_length) {
		fprintf(stderr, "%s(%zu, %zu, %d) unexpected new chain length"
		    " (walking backward). want %zu, got %zu\n",
		    __func__, i, j, use_bio_push, new_chain_length, length);
		goto err;
	}

	if (oldhead_start != NULL) {

		/*
		 * Walk the old head forward and check its length.
		 */

		next = oldhead_start;
		length = 0;
		do {
			prev = next;
			next = BIO_next(prev);
			length++;
		} while (next != NULL);

		if (prev != oldhead_end) {
			fprintf(stderr,
			    "%s(%zu, %zu, %d): unexpected old head end\n",
			    __func__, i, j, use_bio_push);
			goto err;
		}

		if (length != oldhead_length) {
			fprintf(stderr,
			    "%s(%zu, %zu, %d) unexpected old head length"
			    " (walking forward). want %zu, got %zu\n",
			    __func__, i, j, use_bio_push,
			    oldhead_length, length);
			goto err;
		}

		/*
		 * Walk the old head backward and check its length.
		 */

		prev = oldhead_end;
		length = 0;
		do {
			next = prev;
			prev = BIO_prev(next);
			length++;
		} while (prev != NULL);

		if (next != oldhead_start) {
			fprintf(stderr,
			    "%s(%zu, %zu, %d): unexpected old head start\n",
			    __func__, i, j, use_bio_push);
			goto err;
		}

		if (length != oldhead_length) {
			fprintf(stderr,
			    "%s(%zu, %zu, %d) unexpected old head length"
			    " (walking backward). want %zu, got %zu\n",
			    __func__, i, j, use_bio_push,
			    oldhead_length, length);
			goto err;
		}
	}

	if (oldtail_start != NULL) {

		/*
		 * Walk the old tail forward and check its length.
		 */

		next = oldtail_start;
		length = 0;
		do {
			prev = next;
			next = BIO_next(prev);
			length++;
		} while (next != NULL);

		if (prev != oldtail_end) {
			fprintf(stderr,
			    "%s(%zu, %zu, %d): unexpected old tail end\n",
			    __func__, i, j, use_bio_push);
			goto err;
		}

		if (length != oldtail_length) {
			fprintf(stderr,
			    "%s(%zu, %zu, %d) unexpected old tail length"
			    " (walking forward). want %zu, got %zu\n",
			    __func__, i, j, use_bio_push,
			    oldtail_length, length);
			goto err;
		}

		/*
		 * Walk the old tail backward and check its length.
		 */

		prev = oldtail_end;
		length = 0;
		do {
			next = prev;
			prev = BIO_prev(next);
			length++;
		} while (prev != NULL);

		if (next != oldtail_start) {
			fprintf(stderr,
			    "%s(%zu, %zu, %d): unexpected old tail start\n",
			    __func__, i, j, use_bio_push);
			goto err;
		}

		if (length != oldtail_length) {
			fprintf(stderr,
			    "%s(%zu, %zu, %d) unexpected old tail length"
			    " (walking backward). want %zu, got %zu\n",
			    __func__, i, j, use_bio_push,
			    oldtail_length, length);
			goto err;
		}
	}

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
do_bio_link_chains(int use_bio_push)
{
	size_t i, j;
	int failure = 0;

	for (i = 0; i < N_CHAIN_BIOS; i++) {
		for (j = 0; j < N_CHAIN_BIOS; j++) {
			failure |= do_bio_link_chains_at(i, j, use_bio_push);
		}
	}

	return failure;
}

static int
do_bio_push_link_test(void)
{
	int use_bio_push = 1;

	return do_bio_link_chains(use_bio_push);
}

static int
do_bio_set_next_link_test(void)
{
	int use_bio_push = 0;

	return do_bio_link_chains(use_bio_push);
}

int
main(int argc, char **argv)
{
	int ret = 0;

	ret |= do_bio_get_host_ip_tests();
	ret |= do_bio_get_port_tests();
	ret |= do_bio_mem_tests();
	ret |= do_bio_chain_pop_test();
	ret |= do_bio_push_link_test();
	ret |= do_bio_set_next_link_test();

	return (ret);
}

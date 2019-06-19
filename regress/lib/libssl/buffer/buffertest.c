/*
 * Copyright (c) 2019 Joel Sing <jsing@openbsd.org>
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
#include <string.h>

#include "tls13_internal.h"

uint8_t testdata[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
};

struct read_state {
	uint8_t *buf;
	size_t len;
	size_t offset;
};

static ssize_t
read_cb(void *buf, size_t buflen, void *cb_arg)
{
	struct read_state *rs = cb_arg;
	ssize_t n;

	if (rs->offset > rs->len)
		return TLS13_IO_EOF;

	if ((size_t)(n = buflen) > (rs->len - rs->offset))
		n = rs->len - rs->offset;

	if (n == 0)
		return TLS13_IO_WANT_POLLIN;

	memcpy(buf, &rs->buf[rs->offset], n);
	rs->offset += n;

	return n;
}

struct extend_test {
	size_t extend_len;
	size_t read_len;
	ssize_t want_ret;
};

struct extend_test extend_tests[] = {
	{
		.extend_len = 4,
		.read_len = 0,
		.want_ret = TLS13_IO_WANT_POLLIN,
	},
	{
		.extend_len = 4,
		.read_len = 8,
		.want_ret = 4,
	},
	{
		.extend_len = 12,
		.read_len = 8,
		.want_ret = TLS13_IO_WANT_POLLIN,
	},
	{
		.extend_len = 12,
		.read_len = 10,
		.want_ret = TLS13_IO_WANT_POLLIN,
	},
	{
		.extend_len = 12,
		.read_len = 12,
		.want_ret = 12,
	},
	{
		.extend_len = 16,
		.read_len = 16,
		.want_ret = 16,
	},
	{
		.extend_len = 20,
		.read_len = 1,
		.want_ret = TLS13_IO_EOF,
	},
};

#define N_EXTEND_TESTS (sizeof(extend_tests) / sizeof(extend_tests[0]))

int
main(int argc, char **argv)
{
	struct tls13_buffer *buf;
	struct extend_test *et;
	struct read_state rs;
	uint8_t *data;
	size_t i, data_len;
	ssize_t ret;
	CBS cbs;

	rs.buf = testdata;
	rs.offset = 0;

	if ((buf = tls13_buffer_new(0)) == NULL)
		errx(1, "tls13_buffer_new");

	for (i = 0; i < N_EXTEND_TESTS; i++) {
		et = &extend_tests[i];
		rs.len = et->read_len;

		ret = tls13_buffer_extend(buf, et->extend_len, read_cb, &rs);
		if (ret != extend_tests[i].want_ret) {
			fprintf(stderr, "FAIL: Test %zi - extend returned %zi, "
			    "want %zi\n", i, ret, et->want_ret);
			return 1;
		}

		tls13_buffer_cbs(buf, &cbs);

		if (!CBS_mem_equal(&cbs, testdata, CBS_len(&cbs))) {
			fprintf(stderr, "FAIL: Test %zi - extend buffer "
			    "mismatch", i);
			return 1;
		}
	}

	if (!tls13_buffer_finish(buf, &data, &data_len)) {
		fprintf(stderr, "FAIL: failed to finish\n");
		return 1;
	}

	tls13_buffer_free(buf);

	if (data_len != sizeof(testdata)) {
		fprintf(stderr, "FAIL: got data length %zu, want %zu\n",
		    data_len, sizeof(testdata));
		return 1;
	}
	if (memcmp(data, testdata, data_len) != 0) {
		fprintf(stderr, "FAIL: data mismatch\n");
		return 1;
	}
	free(data);

	return 0;
}

/*	$OpenBSD: biotest.c,v 1.8 2022/02/19 16:00:57 jsing Exp $	*/
/*
 * Copyright (c) 2014, 2022 Joel Sing <jsing@openbsd.org>
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
			fprintf(stderr, "FAIL: test %zi (\"%s\") %s, want %s\n",
			    i, bgit->input, ret ? "success" : "failure",
			    bgit->ret ? "success" : "failure");
			failed = 1;
			continue;
		}
		if (ret && ntohl(ip.i) != bgit->ip) {
			fprintf(stderr, "FAIL: test %zi (\"%s\") returned ip "
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
			fprintf(stderr, "FAIL: test %zi (\"%s\") %s, want %s\n",
			    i, bgpt->input, ret ? "success" : "failure",
			    bgpt->ret ? "success" : "failure");
			failed = 1;
			continue;
		}
		if (ret && port != bgpt->port) {
			fprintf(stderr, "FAIL: test %zi (\"%s\") returned port "
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

int
main(int argc, char **argv)
{
	int ret = 0;

	ret |= do_bio_get_host_ip_tests();
	ret |= do_bio_get_port_tests();
	ret |= do_bio_mem_tests();

	return (ret);
}

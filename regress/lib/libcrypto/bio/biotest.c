/*	$OpenBSD: biotest.c,v 1.1 2014/06/22 14:30:52 jsing Exp $	*/
/*
 * Copyright (c) 2014 Joel Sing <jsing@openbsd.org>
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

#include <stdlib.h>

#include <openssl/bio.h>
#include <openssl/err.h>

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

int
main(int argc, char **argv)
{
	return do_bio_get_port_tests();
}

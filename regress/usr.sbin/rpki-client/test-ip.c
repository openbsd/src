/*	$Id: test-ip.c,v 1.1 2019/06/18 12:09:07 claudio Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <sys/socket.h>
#include <arpa/inet.h>

#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "extern.h"

static void
test(const char *res, uint16_t afiv, size_t sz, size_t unused, ...)
{
	va_list		 ap;
	struct ip_addr	 addr;
	char		 buf[64];
	size_t		 i;
	enum afi	 afi;
	struct cert_ip	 ip;
	int		 rc;

	afi = (afiv == 1) ? AFI_IPV4 : AFI_IPV6;

	memset(&addr, 0, sizeof(struct ip_addr));

	va_start(ap, unused);
	for (i = 0; i < sz - 1; i++)
		addr.addr[i] = (unsigned char)va_arg(ap, int);
	va_end(ap);

	addr.sz = sz - 1;
	addr.unused = unused;
	ip_addr_print(&addr, afi, buf, sizeof(buf));
	if (res != NULL && strcmp(res, buf))
		errx(EXIT_FAILURE, "fail: %s != %s\n", res, buf);
	else if (res != NULL)
		warnx("pass: %s", buf);
	else
		warnx("check: %s", buf);

	ip.afi = afi;
	ip.type = CERT_IP_ADDR;
	ip.ip = addr;
	rc = ip_cert_compose_ranges(&ip);

	inet_ntop((afiv == 1) ? AF_INET : AF_INET6, ip.min, buf, sizeof(buf));
	warnx("minimum: %s", buf);
	inet_ntop((afiv == 1) ? AF_INET : AF_INET6, ip.max, buf, sizeof(buf));
	warnx("maximum: %s", buf);
	if (!rc)
		errx(EXIT_FAILURE, "fail: minimum > maximum");
}

int
main(int argc, char *argv[])
{

	SSL_library_init();
	SSL_load_error_strings();

	test("10.5.0.4/32",
	     1, 0x05, 0x00, 0x0a, 0x05, 0x00, 0x04);

	test("10.5.0.0/23",
	     1, 0x04, 0x01, 0x0a, 0x05, 0x00);

	test("2001:0:200:3:0:0:0:1/128",
	     2, 0x11, 0x00, 0x20, 0x01, 0x00, 0x00, 0x02, 0x00, 0x00, 0x03,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01);

	test("2001:0:200::/39",
	     2, 0x06, 0x01, 0x20, 0x01, 0x00, 0x00, 0x02);

	test(NULL,
	     1, 0x03, 0x00, 0x0a, 0x05);

	test(NULL,
	     1, 0x04, 0x01, 0x0a, 0x05, 0x00);

	test(NULL,
	     2, 0x06, 0x01, 0x20, 0x01, 0x00, 0x00, 0x02);

	test(NULL,
	     2, 0x06, 0x02, 0x20, 0x01, 0x00, 0x00, 0x00);

	test("0.0.0.0/0",
	     1, 0x01, 0x00);

	test("10.64.0.0/12",
	     1, 0x03, 0x04, 0x0a, 0x40);

	test("10.64.0.0/20",
	     1, 0x04, 0x04, 0x0a, 0x40, 0x00);

	test(NULL,
	     1, 0x02, 0x04, 0x80);
	test(NULL,
	     1, 0x03, 0x06, 0x81, 0x40);
	test(NULL,
	     1, 0x02, 0x04, 0x80);

	ERR_free_strings();

	printf("OK\n");
	return 0;
}

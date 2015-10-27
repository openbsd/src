/* $OpenBSD: broadcast_bind.c,v 1.1 2015/10/27 16:05:54 vgross Exp $ */

/*
 * Copyright (c) 2015 Vincent Gross <vgross@openbsd.org>
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
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include <sys/socket.h>

#include <netinet/in.h>


int test_result = EXIT_SUCCESS;

void
test_bind(char *paddr, struct in_addr *addr, u_int16_t port, int type, int expected_errno)
{
	int s, rc;
	struct sockaddr_in sin;

	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	sin.sin_port = htons(port);
	memcpy(&sin.sin_addr, addr, sizeof(*addr));

	s = socket(PF_INET, type, 0);
	if (s < 0) {
		warn("socket(PF_INET, %d, 0)", type);
		test_result = EXIT_FAILURE;
		return;
	}

	rc = bind(s, (struct sockaddr *)&sin, sin.sin_len);
	if ((rc == 0 && expected_errno == 0) ||
	    (rc != 0 && expected_errno == errno)) {
		close(s);
		return;
	}
		
	warn("bind(%s,%d) (type %d) expected %d, got %d", paddr, port, type, expected_errno, errno);
	test_result = EXIT_FAILURE;
	close(s);
	return;
}

int
main(int argc, char *argv[])
{
	int rc;
	struct in_addr uc_addr, err_addr, bc_addr;
	int port = 30000;

	if (argc != 4)
		errx(EXIT_FAILURE, "needs 2 arguments: <unicast> <error> <broadcast>");

	rc = inet_pton(AF_INET, argv[1], &uc_addr);
	if (rc != 1) {
		if (rc)
			err(EXIT_FAILURE, "inet_pton(unicast)");
		else
			errx(EXIT_FAILURE, "inet_pton(unicast): error parsing %s",
			    argv[1]);
	}
	rc = inet_pton(AF_INET, argv[2], &err_addr);
	if (rc != 1) {
		if (rc)
			err(EXIT_FAILURE, "inet_pton(error)");
		else
			errx(EXIT_FAILURE, "inet_pton(error): error parsing %s",
			    argv[2]);
	}
	rc = inet_pton(AF_INET, argv[3], &bc_addr);
	if (rc != 1) {
		if (rc)
			err(EXIT_FAILURE, "inet_pton(broadcast)");
		else
			errx(EXIT_FAILURE, "inet_pton(broadcast): error parsing %s",
			    argv[3]);
	}

	test_result = EXIT_SUCCESS;

	test_bind(argv[1], &uc_addr, port, SOCK_STREAM, 0);
	test_bind(argv[2], &err_addr, port, SOCK_STREAM, EADDRNOTAVAIL);
	test_bind(argv[3], &bc_addr, port, SOCK_STREAM, EADDRNOTAVAIL);

	test_bind(argv[1], &uc_addr, port, SOCK_DGRAM, 0);
	test_bind(argv[2], &err_addr, port, SOCK_STREAM, EADDRNOTAVAIL);
	test_bind(argv[3], &bc_addr, port, SOCK_DGRAM, 0);

	return test_result;
}

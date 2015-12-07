/* $OpenBSD: runtest.c,v 1.1 2015/12/07 17:05:52 vgross Exp $ */
/*
 * Copyright (c) 2015 Vincent Gross <vincent.gross@kilob.yt>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>


int
runtest(int *sockp, struct addrinfo *ai, int reuseaddr, int reuseport,
    int expected)
{
	int error, optval;

	*sockp = socket(ai->ai_family, ai->ai_socktype, 0);
	if (*sockp == -1) {
		warn("%s : socket()", ai->ai_canonname);
		return (3);
	}

	if (reuseaddr) {
		optval = 1;
		error = setsockopt(*sockp, SOL_SOCKET, SO_REUSEADDR,
		    &optval, sizeof(int));
		if (error) {
			warn("%s : setsockopt(SO_REUSEADDR)", ai->ai_canonname);
			return (2);
		}
	}

	if (reuseport) {
		optval = 1;
		error = setsockopt(*sockp, SOL_SOCKET, SO_REUSEPORT,
		    &optval, sizeof(int));
		if (error) {
			warn("%s : setsockopt(SO_REUSEPORT)", ai->ai_canonname);
			return (2);
		}
	}

	error = bind(*sockp, ai->ai_addr, ai->ai_addrlen);
	if (error && (expected == 0 || expected != errno)) {
		warn("bind(%s,%s,%s)", ai->ai_canonname,
				reuseaddr ? "REUSEADDR" : "",
				reuseport ? "REUSEPORT" : "");
		return (1);
	}
	if (error == 0 && expected != 0) {
		warnx("%s : bind() succeeded, expected : %s",
		    ai->ai_canonname, strerror(errno));
		return (1);
	}

	return (0);
}

void
cleanup(int *fds, int num_fds)
{
	while (num_fds-- > 0)
		if (close(*fds++) && errno != EBADF)
			err(2, "unable to clean up sockets, aborting");
}

int
main(int argc, char *argv[])
{
	int error, i, rc, test_rc;
	struct addrinfo hints, *local, *any;
	int sockets[4];
	int *s;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICHOST | AI_NUMERICSERV | \
	    AI_PASSIVE;
	hints.ai_socktype = SOCK_DGRAM;
	if ((error = getaddrinfo(argv[2], argv[1], &hints, &local)))
		errx(1, "getaddrinfo(%s,%s): %s", argv[2], argv[1],
		    gai_strerror(error));
	local->ai_canonname = argv[2];

	hints.ai_family = local->ai_family;
	if ((error = getaddrinfo(NULL, argv[1], &hints, &any)))
		errx(1, "getaddrinfo(NULL,%s): %s", argv[1],
		    gai_strerror(error));
	any->ai_canonname = "ANY";

	test_rc = 0;

	rc = 0; s = sockets;
	rc |= runtest(s++, local, 0, 0, 0);
	rc |= runtest(s++, any,   0, 0, EADDRINUSE);
	rc |= runtest(s++, any,   1, 0, 0);
	cleanup(sockets, 3);
	test_rc |= rc;
	if (rc)
		warnx("test #1 failed");

	rc = 0; s = sockets;
	rc |= runtest(s++, any,   0, 0, 0);
	rc |= runtest(s++, local, 0, 0, EADDRINUSE);
	rc |= runtest(s++, local, 1, 0, 0);
	cleanup(sockets, 3);
	test_rc |= rc;
	if (rc)
		warnx("test #2 failed");
	
	rc = 0; s = sockets;
	rc |= runtest(s++, local, 0, 1, 0);
	rc |= runtest(s++, local, 0, 1, 0);
	rc |= runtest(s++, local, 1, 0, EADDRINUSE); /* 0 if multicast */
	rc |= runtest(s++, local, 0, 0, EADDRINUSE);
	cleanup(sockets, 4);
	test_rc |= rc;
	if (rc)
		warnx("test #3 failed");

	rc = 0; s = sockets;
	rc |= runtest(s++, any, 0, 1, 0);
	rc |= runtest(s++, any, 0, 1, 0);
	rc |= runtest(s++, any, 1, 0, EADDRINUSE); /* 0 if multicast */
	rc |= runtest(s++, any, 0, 0, EADDRINUSE);
	cleanup(sockets, 4);
	test_rc |= rc;
	if (rc)
		warnx("test #4 failed");

	rc = 0; s = sockets;
	rc |= runtest(s++, local, 1, 0, 0);
	rc |= runtest(s++, local, 1, 0, EADDRINUSE); /* 0 if multicast */
	rc |= runtest(s++, local, 0, 1, EADDRINUSE); /* 0 if multicast */
	cleanup(sockets, 3);
	test_rc |= rc;
	if (rc)
		warnx("test #5 failed");

	rc = 0; s = sockets;
	rc |= runtest(s++, local, 1, 0, 0);
	rc |= runtest(s++, local, 1, 0, EADDRINUSE); /* 0 if multicast */
	rc |= runtest(s++, local, 0, 1, EADDRINUSE); /* 0 if multicast */
	cleanup(sockets, 3);
	test_rc |= rc;
	if (rc)
		warnx("test #6 failed");

	return (test_rc);
}

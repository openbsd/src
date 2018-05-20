/*	$OpenBSD: nonxt-reflect.c,v 1.1 2018/05/19 10:50:57 bluhm Exp $	*/
/*
 * Copyright (c) Alexander Bluhm <bluhm@genua.de>
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
#include <sys/socket.h>

#include <netdb.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void __dead usage(void);

void
usage(void)
{
	fprintf(stderr, "usage: nonxt-reflect [localaddr]\n"
	    "Wait for protocol 59 packet in background and send answer.\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct addrinfo hints, *res, *res0;
	struct sockaddr_storage ss;
	const char *cause = NULL, *local;
	socklen_t slen;
	int error;
	int save_errno;
	int s;
	char buf[1024];

	switch (argc) {
	case 1:
		local = NULL;
		break;
	case 2:
		local = argv[1];
		break;
	default:
		usage();
	}

	/* Create socket and bind it to local address. */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_NONE;
	hints.ai_flags = AI_PASSIVE;
	error = getaddrinfo(local, NULL, &hints, &res0);
	if (error)
		errx(1, "getaddrinfo local: %s", gai_strerror(error));
	for (res = res0; res; res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s == -1) {
			cause = "socket";
			continue;
		}
		if (bind(s, res->ai_addr, res->ai_addrlen) == -1) {
			cause = "bind";
			save_errno = errno;
			close(s);
			errno = save_errno;
			continue;
		}
		break;
	}
	if (res == NULL)
		err(1, "%s", cause);
	freeaddrinfo(res0);

	/* Scoket is ready to receive, test may proceed. */
	daemon(0, 0);

	/* Receive a protocol 59 packet. */
	slen = sizeof(ss);
	if (recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&ss, &slen)
	    == -1)
		err(1, "recv");
	/* Send back a reply packet. */
	if (sendto(s, buf, 0, 0, (struct sockaddr *)&ss, slen) == -1)
		err(1, "send");

	return 0;
}

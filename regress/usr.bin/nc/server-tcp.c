/*	$OpenBSD: server-tcp.c,v 1.1 2020/01/16 21:11:17 bluhm Exp $	*/

/*
 * Copyright (c) 2020 Alexander Bluhm <bluhm@openbsd.org>
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

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

void __dead usage(void);
int listen_socket(const char *, const char *);
int accept_socket(int);

void __dead
usage(void)
{
	fprintf(stderr, "server-tcp [-r rcvmsg] [-s sndmsg] host port\n"
	"    -r rcvmsg  receive from client and check message\n"
	"    -s sndmsg  send message to client\n");
	exit(2);
}

int
main(int argc, char *argv[])
{
	const char *host, *port;
	const char *rcvmsg = NULL, *sndmsg = NULL;
	int ch, s;

	while ((ch = getopt(argc, argv, "r:s:")) != -1) {
		switch (ch) {
		case 'r':
			rcvmsg = optarg;
			break;
		case 's':
			sndmsg = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 2) {
		host = argv[0];
		port = argv[1];
	} else {
		usage();
	}

	alarm_timeout();
	s = listen_socket(host, port);
	print_sockname(s);

	switch (fork()) {
	case -1:
		err(1, "fork");
	case 0:
		/* child continues */
		break;
	default:
		/* parent exits and test runs in parallel */
		_exit(0);
	}

	s = accept_socket(s);
	if (sndmsg != NULL)
		send_line(s, sndmsg);
	if (rcvmsg != NULL)
		receive_line(s, rcvmsg);

	if (close(s) == -1)
		err(1, "close");

	return 0;
}

int
listen_socket(const char *host, const char *port)
{
	struct addrinfo hints, *res, *res0;
	int error;
	int save_errno;
	int s;
	const char *cause = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	error = getaddrinfo(host, port, &hints, &res0);
	if (error)
		errx(1, "%s", gai_strerror(error));
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
		break;  /* okay we got one */
	}
	if (s == -1)
		err(1, "%s", cause);
	freeaddrinfo(res0);

	if (listen(s, 5) == -1)
		err(1, "listen");
	return s;
}

int
accept_socket(int s)
{
	struct sockaddr_storage ss;
	socklen_t slen;
	char host[NI_MAXHOST], port[NI_MAXSERV];

	slen = sizeof(ss);
	s = accept(s, (struct sockaddr *)&ss, &slen);
	if (s == -1)
		err(1, "accept");
	if (getnameinfo((struct sockaddr *)&ss, ss.ss_len, host, sizeof(host),
	    port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV))
		errx(1, "getnameinfo");
	fprintf(stderr, "peer: %s %s\n", host, port);

	return s;
}

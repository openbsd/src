/*	$OpenBSD: util.c,v 1.3 2021/07/06 11:50:34 bluhm Exp $	*/

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

void
task_enqueue(struct task *todo, int ch, const char *msg)
{
	switch (ch) {
	case 'E':
		todo->t_type = TEOF;
		todo->t_msg = NULL;
		break;
	case 'N':
		todo->t_type = TDWN;
		todo->t_msg = NULL;
		break;
	case 'r':
		todo->t_type = TRCV;
		todo->t_msg = msg;
		break;
	case 's':
		todo->t_type = TSND;
		todo->t_msg = msg;
		break;
	}
}

void
task_run(int s, struct task *todolist, size_t tlen)
{
	size_t t;

	for (t = 0; t < tlen; t++) {
		switch(todolist[t].t_type) {
		case TEOF:
			receive_eof(s);
			break;
		case TDWN:
			send_shutdown(s);
			break;
		case TRCV:
			receive_line(s, todolist[t].t_msg);
			break;
		case TSND:
			send_line(s, todolist[t].t_msg);
			break;
		}
	}
}

void
print_sockname(int s)
{
	struct sockaddr_storage ss;
	socklen_t slen;
	char host[NI_MAXHOST], port[NI_MAXSERV];

	slen = sizeof(ss);
	if (getsockname(s, (struct sockaddr *)&ss, &slen) == -1)
		err(1, "getsockname");
	if (getnameinfo((struct sockaddr *)&ss, ss.ss_len, host, sizeof(host),
	    port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV))
		errx(1, "getnameinfo");
	printf("%s\n", port);
	if (fflush(stdout) != 0)
		err(1, "fflush stdout");
	fprintf(stderr, "sock: %s %s\n", host, port);
}

void
print_peername(int s)
{
	struct sockaddr_storage ss;
	socklen_t slen;
	char host[NI_MAXHOST], port[NI_MAXSERV];

	slen = sizeof(ss);
	if (getpeername(s, (struct sockaddr *)&ss, &slen) == -1)
		err(1, "getpeername");
	if (getnameinfo((struct sockaddr *)&ss, ss.ss_len, host, sizeof(host),
	    port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV))
		errx(1, "getnameinfo");
	fprintf(stderr, "peer: %s %s\n", host, port);
}

void
receive_eof(int s)
{
	char buf[100];
	size_t len;
	ssize_t n;

	n = recv(s, buf, sizeof(buf) - 1, 0);
	if (n == -1)
		err(1, "recv");
	if (n == 0) {
		fprintf(stderr, "<<< EOF\n");
		return;
	}
	len = n;
	buf[len] = '\0';
	if (buf[len - 1] == '\n')
		buf[--len] = '\0';
	fprintf(stderr, "<<< %s\n", buf);
	errx(1, "expected receive EOF, got '%s'", buf);
}

void
send_shutdown(int s)
{
	if (shutdown(s, SHUT_WR) == -1)
		err(1, "shutdown");
}

void
receive_line(int s, const char *msg)
{
	char buf[100];
	size_t off, len;
	ssize_t n;

	len = 0;
	while (len < sizeof(buf) - 1) {
		off = len;
		n = recv(s, buf + off, sizeof(buf) - 1 - off, 0);
		if (n == -1)
			err(1, "recv");
		if (n == 0) {
			fprintf(stderr, "<<< EOF\n");
			break;
		}
		len += n;
		buf[len] = '\0';
		if (buf[len - 1] == '\n')
			fprintf(stderr, "<<< %s", buf + off);
		else
			fprintf(stderr, "<<< %s\n", buf + off);
		if (strchr(buf + off, '\n') != NULL)
			break;
	}
	if (len == 0)
		errx(1, "empty receive buffer");
	if (buf[len - 1] != '\n')
		errx(1, "new line missing in receive buffer");
	buf[--len] = '\0';
	if (strcmp(msg, buf) != 0)
		errx(1, "expected receive '%s', got '%s'", msg, buf);
}

void
send_line(int s, const char *msg)
{
	char buf[100];
	size_t off, len;
	ssize_t n;

	len = strlcpy(buf, msg, sizeof(buf));
	if (len >= sizeof(buf))
		errx(1, "message too long for send buffer");
	if (buf[len] != '\n') {
		buf[len++] = '\n';
		if (len >= sizeof(buf))
			errx(1, "new line too long for send buffer");
		buf[len] = 0;
	}

	off = 0;
	while (off < len) {
		fprintf(stderr, ">>> %s", buf + off);
		n = send(s, buf + off, len - off, 0);
		if (n == -1)
			err(1, "send");
		off += n;
	}
}

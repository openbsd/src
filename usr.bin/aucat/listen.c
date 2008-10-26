/*	$OpenBSD: listen.c,v 1.1 2008/10/26 08:49:44 ratchov Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
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
#include <sys/un.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "conf.h"
#include "sock.h"
#include "listen.h"

struct fileops listen_ops = {
	"listen",
	sizeof(struct listen),
	listen_close,
	NULL, /* read */
	NULL, /* write */
	NULL, /* start */
	NULL, /* stop */
	listen_nfds,
	listen_pollfd,
	listen_revents
};

struct listen *
listen_new(struct fileops *ops, char *path)
{
	int sock;
	struct sockaddr_un sockname;
	struct listen *f;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}
	if (unlink(path) < 0 && errno != ENOENT) {
		perror("unlink");
		exit(1);
	}
	sockname.sun_family = AF_UNIX;
	strlcpy(sockname.sun_path, path, sizeof(sockname.sun_path));
	if (bind(sock, (struct sockaddr *)&sockname, 
	        sizeof(struct sockaddr_un)) < 0) {
		perror("bind");
		exit(1);
	}
	if (listen(sock, 1) < 0) {
		perror("listen");
		exit(1);
	}
	f = (struct listen *)file_new(ops, path, 1);
	f->path = strdup(path);
	if (f->path == NULL) {
		perror("strdup");
		exit(1);
	}
	f->fd = sock;
	return f;
}

int
listen_nfds(struct file *f) {
	return 1;
}

int
listen_pollfd(struct file *file, struct pollfd *pfd, int events)
{
	struct listen *f = (struct listen *)file;

	pfd->fd = f->fd;
	pfd->events = POLLIN;
	return 1;
}

int
listen_revents(struct file *file, struct pollfd *pfd)
{
	struct listen *f = (struct listen *)file;
	struct sockaddr caddr;
	socklen_t caddrlen;
	int sock;

	if (pfd->revents & POLLIN) {
		DPRINTF("listen_revents: accepting connection\n");
		caddrlen = sizeof(caddrlen);
		sock = accept(f->fd, &caddr, &caddrlen);
		if (sock < 0) {
			perror("accept");
			return 0;
		}
		if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
			perror("fcntl(sock, O_NONBLOCK)");
			close(sock);
			return 0;
		}
		(void)sock_new(&sock_ops, sock, "socket");
	}
	return 0;
}

void
listen_close(struct file *file)
{
	struct listen *f = (struct listen *)file;

	(void)unlink(f->path);
	free(f->path);
	(void)close(f->fd);
}

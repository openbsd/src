/*	$OpenBSD: listen.c,v 1.12 2011/04/19 00:02:29 ratchov Exp $	*/
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
#include <sys/stat.h>
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
#include "listen.h"
#include "sock.h"

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

void
listen_new_un(char *path)
{
	int sock, oldumask;
	struct sockaddr_un sockname;
	struct listen *f;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}
	if (unlink(path) < 0 && errno != ENOENT) {
		perror("unlink");
		goto bad_close;
	}
	sockname.sun_family = AF_UNIX;
	strlcpy(sockname.sun_path, path, sizeof(sockname.sun_path));
	oldumask = umask(0111);
	if (bind(sock, (struct sockaddr *)&sockname,
		sizeof(struct sockaddr_un)) < 0) {
		perror("bind");
		goto bad_close;
	}
	umask(oldumask);
	if (listen(sock, 1) < 0) {
		perror("listen");
		goto bad_close;
	}
	f = (struct listen *)file_new(&listen_ops, path, 1);
	if (f == NULL)
		goto bad_close;
	f->path = strdup(path);
	if (f->path == NULL) {
		perror("strdup");
		exit(1);
	}
	f->fd = sock;
	return;
 bad_close:
	close(sock);
	exit(1);	
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
		caddrlen = sizeof(caddrlen);
		sock = accept(f->fd, &caddr, &caddrlen);
		if (sock < 0) {
			/* XXX: should we kill the socket here ? */
			perror("accept");
			return 0;
		}
		if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
			perror("fcntl(sock, O_NONBLOCK)");
			close(sock);
			return 0;
		}
		if (sock_new(&sock_ops, sock) == NULL) {
			close(sock);
			return 0;
		}
	}
	return 0;
}

void
listen_close(struct file *file)
{
	struct listen *f = (struct listen *)file;

	if (f->path != NULL) {
		unlink(f->path);
		free(f->path);
	}
	close(f->fd);
}

void
listen_closeall(void)
{
	struct file *f, *fnext;

	for (f = LIST_FIRST(&file_list); f != NULL; f = fnext) {
		fnext = LIST_NEXT(f, entry);
		if (f->ops == &listen_ops)
			file_close(f);
	}
}

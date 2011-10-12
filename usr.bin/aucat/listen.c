/*	$OpenBSD: listen.c,v 1.17 2011/10/12 07:20:04 ratchov Exp $	*/
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
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

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
#include "dbg.h"

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

struct listen *listen_list = NULL;

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
	f = (struct listen *)file_new(&listen_ops, path, 1);
	if (f == NULL)
		goto bad_close;
	f->path = strdup(path);
	if (f->path == NULL) {
		perror("strdup");
		exit(1);
	}
	f->fd = sock;
	f->next = listen_list;
	listen_list = f;
	return;
 bad_close:
	close(sock);
	exit(1);	
}

void
listen_new_tcp(char *addr, unsigned port)
{
	char *host, serv[sizeof(unsigned) * 3 + 1];
	struct addrinfo *ailist, *ai, aihints;
	struct listen *f;
	int s, error, opt = 1, n = 0;
	
	/* 
	 * obtain a list of possible addresses for the host/port 
	 */
	memset(&aihints, 0, sizeof(struct addrinfo));
	snprintf(serv, sizeof(serv), "%u", port);
	host = strcmp(addr, "-") == 0 ? NULL : addr;
	aihints.ai_flags |= AI_PASSIVE;
	aihints.ai_socktype = SOCK_STREAM;
	aihints.ai_protocol = IPPROTO_TCP;
	error = getaddrinfo(host, serv, &aihints, &ailist);
	if (error) {
		fprintf(stderr, "%s: %s\n", addr, gai_strerror(error));
		exit(1);
	}

	/* 
	 * for each address, try create a listening socket bound on
	 * that address
	 */
	for (ai = ailist; ai != NULL; ai = ai->ai_next) {
		s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (s < 0) {
			perror("socket");
			continue;
		}
		opt = 1;
		if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) < 0) {
			perror("setsockopt");
			goto bad_close;
		}
		if (bind(s, ai->ai_addr, ai->ai_addrlen) < 0) {
			perror("bind");
			goto bad_close;
		}
		f = (struct listen *)file_new(&listen_ops, addr, 1);
		if (f == NULL) {
		bad_close:
			close(s);
			continue;
		}
		f->path = NULL;
		f->fd = s;
		f->next = listen_list;
		listen_list = f;
		n++;
	}
	freeaddrinfo(ailist);
	if (n == 0)
		exit(1);
}

int
listen_init(struct listen *f)
{
	if (listen(f->fd, 1) < 0) {
		perror("listen");
		return 0;
	}
	return 1;
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
	int sock, opt;

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
		if (f->path == NULL) {
			opt = 1;
			if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
				&opt, sizeof(int)) < 0) {
				perror("setsockopt");
				close(sock);
				return 0;
			}
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
	struct listen *f = (struct listen *)file, **pf;

	if (f->path != NULL) {
		unlink(f->path);
		free(f->path);
	}
	close(f->fd);
	for (pf = &listen_list; *pf != f; pf = &(*pf)->next) {
#ifdef DEBUG
		if (*pf == NULL) {
			dbg_puts("listen_close: not on list\n");
			dbg_panic();
		}
#endif
	}
	*pf = f->next;
}

/*	$OpenBSD: listen.c,v 1.14 2020/01/23 20:55:01 ratchov Exp $	*/
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

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "listen.h"
#include "file.h"
#include "sock.h"
#include "utils.h"

int listen_pollfd(void *, struct pollfd *);
int listen_revents(void *, struct pollfd *);
void listen_in(void *);
void listen_out(void *);
void listen_hup(void *);

struct fileops listen_fileops = {
	"listen",
	listen_pollfd,
	listen_revents,
	listen_in,
	listen_out,
	listen_hup
};

struct listen *listen_list = NULL;

void
listen_close(struct listen *f)
{
	struct listen **pf;

	for (pf = &listen_list; *pf != f; pf = &(*pf)->next) {
#ifdef DEBUG
		if (*pf == NULL) {
			log_puts("listen_close: not on list\n");
			panic();
		}
#endif
	}
	*pf = f->next;

	if (f->path != NULL) {
		xfree(f->path);
	}
	file_del(f->file);
	close(f->fd);
	xfree(f);
}

int
listen_new_un(char *path)
{
	int sock, oldumask;
	struct sockaddr_un sockname;
	struct listen *f;

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1) {
		log_puts(path);
		log_puts(": failed to create socket\n");
		return 0;
	}
	if (unlink(path) == -1 && errno != ENOENT) {
		log_puts(path);
		log_puts(": failed to unlink socket\n");
		goto bad_close;
	}
	sockname.sun_family = AF_UNIX;
	strlcpy(sockname.sun_path, path, sizeof(sockname.sun_path));
	oldumask = umask(0111);
	if (bind(sock, (struct sockaddr *)&sockname,
		sizeof(struct sockaddr_un)) == -1) {
		log_puts(path);
		log_puts(": failed to bind socket\n");
		goto bad_close;
	}
	if (listen(sock, 1) == -1) {
		log_puts(path);
		log_puts(": failed to listen\n");
		goto bad_close;
	}
	umask(oldumask);
	f = xmalloc(sizeof(struct listen));
	f->file = file_new(&listen_fileops, f, path, 1);
	if (f->file == NULL)
		goto bad_close;
	f->path = xstrdup(path);
	f->fd = sock;
	f->next = listen_list;
	listen_list = f;
	return 1;
 bad_close:
	close(sock);
	return 0;
}

int
listen_new_tcp(char *addr, unsigned int port)
{
	char *host, serv[sizeof(unsigned int) * 3 + 1];
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
		log_puts(addr);
		log_puts(": failed to resolve address\n");
		return 0;
	}

	/*
	 * for each address, try create a listening socket bound on
	 * that address
	 */
	for (ai = ailist; ai != NULL; ai = ai->ai_next) {
		s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (s == -1) {
			log_puts(addr);
			log_puts(": failed to create socket\n");
			continue;
		}
		opt = 1;
		if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
		    &opt, sizeof(int)) == -1) {
			log_puts(addr);
			log_puts(": failed to set SO_REUSEADDR\n");
			goto bad_close;
		}
		if (bind(s, ai->ai_addr, ai->ai_addrlen) == -1) {
			log_puts(addr);
			log_puts(": failed to bind socket\n");
			goto bad_close;
		}
		if (listen(s, 1) == -1) {
			log_puts(addr);
			log_puts(": failed to listen\n");
			goto bad_close;
		}
		f = xmalloc(sizeof(struct listen));
		f->file = file_new(&listen_fileops, f, addr, 1);
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
	return n;
}

int
listen_init(struct listen *f)
{
	return 1;
}

int
listen_pollfd(void *arg, struct pollfd *pfd)
{
	struct listen *f = arg;

	f->slowaccept = file_slowaccept;
	if (f->slowaccept)
		return 0;
	pfd->fd = f->fd;
	pfd->events = POLLIN;
	return 1;
}

int
listen_revents(void *arg, struct pollfd *pfd)
{
	struct listen *f = arg;

	if (f->slowaccept)
		return 0;
	return pfd->revents;
}

void
listen_in(void *arg)
{
	struct listen *f = arg;
	struct sockaddr caddr;
	socklen_t caddrlen;
	int sock, opt;

	caddrlen = sizeof(caddrlen);
	while ((sock = accept(f->fd, &caddr, &caddrlen)) == -1) {
		if (errno == EINTR)
			continue;
		if (errno == ENFILE || errno == EMFILE)
			file_slowaccept = 1;
		return;
	}
	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
		file_log(f->file);
		log_puts(": failed to set non-blocking mode\n");
		goto bad_close;
	}
	if (f->path == NULL) {
		opt = 1;
		if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		    &opt, sizeof(int)) == -1) {
			file_log(f->file);
			log_puts(": failed to set TCP_NODELAY flag\n");
			goto bad_close;
		}
	}
	if (sock_new(sock) == NULL)
		goto bad_close;
	return;
bad_close:
	close(sock);
}

void
listen_out(void *arg)
{
}

void
listen_hup(void *arg)
{
	struct listen *f = arg;

	listen_close(f);
}

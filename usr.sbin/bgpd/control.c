/*	$OpenBSD: control.c,v 1.2 2004/01/02 02:27:57 henning Exp $ */

/*
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"
#include "session.h"

#define	CONTROL_BACKLOG	5

struct {
	int	fd;
} control_state;

struct ctl_conn	*control_connbyfd(int);

int
control_init(void)
{
	struct sockaddr_un	sun;
	int			fd;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		log_err("control_init: socket");
		return (-1);
	}

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, SOCKET_NAME, sizeof(sun.sun_path));
	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		log_err("control_init: bind: %s", SOCKET_NAME);
		return (-1);
	}

	control_state.fd = fd;

	return (fd);
}

int
control_listen(void)
{
	if (listen(control_state.fd, CONTROL_BACKLOG) == -1) {
		log_err("control_listen: listen");
		return (-1);
	}

	return (control_state.fd);
}

void
control_shutdown(void)
{
	close(control_state.fd);
}

void
control_cleanup(void)
{
	unlink(SOCKET_NAME);
}

void
control_accept(int listenfd)
{
	int			 connfd;
	socklen_t		 len;
	struct sockaddr_un	 sun;
	uid_t			 uid;
	gid_t			 gid;
	struct ctl_conn		*ctl_conn;

	len = sizeof(sun);
	if ((connfd = accept(listenfd,
	    (struct sockaddr *)&sun, &len)) == -1) {
		if (errno == EWOULDBLOCK || errno == EINTR)
			return;
		else
			log_err("session_control_accept");
	}

	if (getpeereid(connfd, &uid, &gid) == -1) {
		log_err("session_control_accept");
		return;
	}

	if (uid) {
		log_err("Connection to control socket with uid %ld", uid);
		return;
	}

	if ((ctl_conn = malloc(sizeof(struct ctl_conn))) == NULL) {
		log_err("session_control_accept");
		return;
	}

	imsg_init(&ctl_conn->ibuf, connfd);

	TAILQ_INSERT_TAIL(&ctl_conns, ctl_conn, entries);
}

struct ctl_conn *
control_connbyfd(int fd)
{
	struct ctl_conn	*c;

	for (c = TAILQ_FIRST(&ctl_conns); c != NULL && c->ibuf.sock != fd;
	    c = TAILQ_NEXT(c, entries))
		;	/* nothing */

	return (c);
}

void
control_close(int fd)
{
	struct ctl_conn	*c;

	if ((c = control_connbyfd(fd)) == NULL) {
		log_err("control_close: fd %d: not found", fd);
		return;
	}

	TAILQ_REMOVE(&ctl_conns, c, entries);

	close(c->ibuf.sock);
	free(c);
}

int
control_dispatch_msg(struct pollfd *pfd, int i)
{
	struct imsg		 imsg;
	struct ctl_conn		*c;
	int			 n;
	struct peer		*p;

	if ((c = control_connbyfd(pfd->fd)) == NULL) {
		log_err("control_dispatch_msg: fd %d: not found", pfd->fd);
		return (0);
	}

	if (imsg_read(&c->ibuf) == -1) {
		control_close(pfd->fd);
		return (1);
	}

	for (;;) {
		if ((n = imsg_get(&c->ibuf, &imsg)) == -1) {
			control_close(pfd->fd);
			return (1);
		}

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_CTL_SHOW_NEIGHBOR:
			for (p = conf->peers; p != NULL; p = p->next)
				imsg_compose(&c->ibuf, IMSG_CTL_SHOW_NEIGHBOR,
				    0, p, sizeof(struct peer));
			break;
		default:
			break;
		}
		imsg_free(&imsg);
	}

	return (0);
}

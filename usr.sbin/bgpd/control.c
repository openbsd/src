/*	$OpenBSD: control.c,v 1.29 2004/04/29 19:56:04 deraadt Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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
#include <sys/stat.h>
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
struct ctl_conn	*control_connbypid(pid_t);

int
control_init(void)
{
	struct sockaddr_un	 sun;
	int			 fd;
	mode_t			 old_umask;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		log_warn("control_init: socket");
		return (-1);
	}

	old_umask = umask(S_IXUSR|S_IXGRP|S_IWOTH|S_IROTH|S_IXOTH);
	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, SOCKET_NAME, sizeof(sun.sun_path));

	if (unlink(SOCKET_NAME) == -1)
		if (errno != ENOENT) {
			log_warn("unlink %s", SOCKET_NAME);
			close(fd);
			return (-1);
		}

	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		log_warn("control_init: bind: %s", SOCKET_NAME);
		close(fd);
		return (-1);
	}

	if (chmod(SOCKET_NAME, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP) == -1) {
		log_warn("control_init chmod");
		close(fd);
		return (-1);
	}

	umask(old_umask);

	session_socket_blockmode(fd, BM_NONBLOCK);
	control_state.fd = fd;

	return (fd);
}

int
control_listen(void)
{
	if (listen(control_state.fd, CONTROL_BACKLOG) == -1) {
		log_warn("control_listen: listen");
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
	struct ctl_conn		*ctl_conn;

	len = sizeof(sun);
	if ((connfd = accept(listenfd,
	    (struct sockaddr *)&sun, &len)) == -1) {
		if (errno != EWOULDBLOCK && errno != EINTR)
			log_warn("session_control_accept");
		return;
	}

	session_socket_blockmode(connfd, BM_NONBLOCK);

	if ((ctl_conn = malloc(sizeof(struct ctl_conn))) == NULL) {
		log_warn("session_control_accept");
		return;
	}

	imsg_init(&ctl_conn->ibuf, connfd);

	TAILQ_INSERT_TAIL(&ctl_conns, ctl_conn, entries);
}

struct ctl_conn *
control_connbyfd(int fd)
{
	struct ctl_conn	*c;

	for (c = TAILQ_FIRST(&ctl_conns); c != NULL && c->ibuf.fd != fd;
	    c = TAILQ_NEXT(c, entries))
		;	/* nothing */

	return (c);
}

struct ctl_conn *
control_connbypid(pid_t pid)
{
	struct ctl_conn	*c;

	for (c = TAILQ_FIRST(&ctl_conns); c != NULL && c->ibuf.pid != pid;
	    c = TAILQ_NEXT(c, entries))
		;	/* nothing */

	return (c);
}

void
control_close(int fd)
{
	struct ctl_conn	*c;

	if ((c = control_connbyfd(fd)) == NULL) {
		log_warn("control_close: fd %d: not found", fd);
		return;
	}

	msgbuf_clear(&c->ibuf.w);
	TAILQ_REMOVE(&ctl_conns, c, entries);

	close(c->ibuf.fd);
	free(c);
}

int
control_dispatch_msg(struct pollfd *pfd, int i)
{
	struct imsg		 imsg;
	struct ctl_conn		*c;
	int			 n;
	struct peer		*p;
	struct bgpd_addr	*addr;

	if ((c = control_connbyfd(pfd->fd)) == NULL) {
		log_warn("control_dispatch_msg: fd %d: not found", pfd->fd);
		return (0);
	}

	if (pfd->revents & POLLOUT)
		if (msgbuf_write(&c->ibuf.w) < 0) {
			control_close(pfd->fd);
			return (1);
		}

	if (!(pfd->revents & POLLIN))
		return (0);

	if (imsg_read(&c->ibuf) <= 0) {
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
			if (imsg.hdr.len == IMSG_HEADER_SIZE +
			    sizeof(struct bgpd_addr)) {
				addr = imsg.data;
				p = getpeerbyaddr(addr);
				if (p != NULL)
					imsg_compose(&c->ibuf,
					    IMSG_CTL_SHOW_NEIGHBOR,
					    0, p, sizeof(struct peer));
			} else
				for (p = peers; p != NULL; p = p->next)
					imsg_compose(&c->ibuf,
					    IMSG_CTL_SHOW_NEIGHBOR,
					    0, p, sizeof(struct peer));
			imsg_compose(&c->ibuf, IMSG_CTL_END, 0, NULL, 0);
			break;
		case IMSG_CTL_RELOAD:
		case IMSG_CTL_FIB_COUPLE:
		case IMSG_CTL_FIB_DECOUPLE:
			imsg_compose_parent(imsg.hdr.type, 0, NULL, 0);
			break;
		case IMSG_CTL_NEIGHBOR_UP:
			if (imsg.hdr.len == IMSG_HEADER_SIZE +
			    sizeof(struct bgpd_addr)) {
				addr = imsg.data;
				p = getpeerbyaddr(addr);
				if (p != NULL)
					bgp_fsm(p, EVNT_START);
				else
					log_warnx("IMSG_CTL_NEIGHBOR_UP "
					    "with unknown neighbor");
			} else
				log_warnx("got IMSG_CTL_NEIGHBOR_UP with "
				    "wrong length");
			break;
		case IMSG_CTL_NEIGHBOR_DOWN:
			if (imsg.hdr.len == IMSG_HEADER_SIZE +
			    sizeof(struct bgpd_addr)) {
				addr = imsg.data;
				p = getpeerbyaddr(addr);
				if (p != NULL)
					bgp_fsm(p, EVNT_STOP);
				else
					log_warnx("IMSG_CTL_NEIGHBOR_DOWN"
					    " with unknown neighbor");
			} else
				log_warnx("got IMSG_CTL_NEIGHBOR_DOWN "
				    "with wrong length");
			break;
		case IMSG_CTL_KROUTE:
		case IMSG_CTL_KROUTE_ADDR:
		case IMSG_CTL_SHOW_NEXTHOP:
		case IMSG_CTL_SHOW_INTERFACE:
			c->ibuf.pid = imsg.hdr.pid;
			imsg_compose_parent(imsg.hdr.type, imsg.hdr.pid,
			    imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);
			break;
		case IMSG_CTL_SHOW_RIB:
		case IMSG_CTL_SHOW_RIB_AS:
		case IMSG_CTL_SHOW_RIB_PREFIX:
			c->ibuf.pid = imsg.hdr.pid;
			imsg_compose_rde(imsg.hdr.type, imsg.hdr.pid,
			    imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);
			break;
		default:
			break;
		}
		imsg_free(&imsg);
	}

	return (0);
}

int
control_imsg_relay(struct imsg *imsg)
{
	struct ctl_conn	*c;

	if ((c = control_connbypid(imsg->hdr.pid)) == NULL)
		return (0);

	return (imsg_compose_pid(&c->ibuf, imsg->hdr.type, imsg->hdr.pid,
	    imsg->data, imsg->hdr.len - IMSG_HEADER_SIZE));
}

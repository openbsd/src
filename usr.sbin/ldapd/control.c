/*	$OpenBSD: control.c,v 1.2 2010/06/23 12:40:19 martinh Exp $	*/

/*
 * Copyright (c) 2010 Martin Hedenfalk <martin@bzero.se>
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

#include <sys/queue.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/tree.h>

#include <net/if.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ldapd.h"

#define	CONTROL_BACKLOG	5

struct ctl_connlist ctl_conns;

struct ctl_conn	*control_connbyfd(int);
struct ctl_conn	*control_connbypid(pid_t);
void		 control_close(int);

void
control_init(struct control_sock *cs)
{
	struct sockaddr_un	 sun;
	int			 fd;
	mode_t			 old_umask, mode;

	if (cs->cs_name == NULL)
		return;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		fatal("control_init: socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	if (strlcpy(sun.sun_path, cs->cs_name,
	    sizeof(sun.sun_path)) >= sizeof(sun.sun_path))
		fatalx("control_init: name too long");

	if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == 0)
		fatalx("control socket already listening");

	if (unlink(cs->cs_name) == -1 && errno != ENOENT)
		fatal("control_init: unlink");

	if (cs->cs_restricted) {
		old_umask = umask(S_IXUSR|S_IXGRP|S_IXOTH);
		mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
	} else {
		old_umask = umask(S_IXUSR|S_IXGRP|S_IWOTH|S_IROTH|S_IXOTH);
		mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP;
	}

	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		(void)umask(old_umask);
		fatal("control_init: bind");
	}
	(void)umask(old_umask);

	if (chmod(cs->cs_name, mode) == -1) {
		(void)unlink(cs->cs_name);
		fatal("control_init: chmod");
	}

	fd_nonblock(fd);
	cs->cs_fd = fd;
}

void
control_listen(struct control_sock *cs)
{
	if (cs->cs_name == NULL)
		return;

	if (listen(cs->cs_fd, CONTROL_BACKLOG) == -1)
		fatal("control_listen: listen");

	event_set(&cs->cs_ev, cs->cs_fd, EV_READ | EV_PERSIST,
	    control_accept, cs);
	event_add(&cs->cs_ev, NULL);
}

void
control_cleanup(struct control_sock *cs)
{
	if (cs->cs_name == NULL)
		return;
	(void)unlink(cs->cs_name);
}

/* ARGSUSED */
void
control_accept(int listenfd, short event, void *arg)
{
	struct control_sock	*cs = arg;
	int			 connfd;
	socklen_t		 len;
	struct sockaddr_un	 sun;
	struct ctl_conn		*c;

	len = sizeof(sun);
	if ((connfd = accept(listenfd,
	    (struct sockaddr *)&sun, &len)) == -1) {
		if (errno != EWOULDBLOCK && errno != EINTR)
			log_warn("control_accept");
		return;
	}

	fd_nonblock(connfd);

	if ((c = calloc(1, sizeof(*c))) == NULL) {
		log_warn("control_accept");
		close(connfd);
		return;
	}

	imsg_init(&c->iev.ibuf, connfd);
	c->iev.handler = control_dispatch_imsg;
	c->iev.events = EV_READ;
	event_set(&c->iev.ev, c->iev.ibuf.fd, c->iev.events,
	    c->iev.handler, cs);
	event_add(&c->iev.ev, NULL);

	TAILQ_INSERT_TAIL(&ctl_conns, c, entry);
}

struct ctl_conn *
control_connbyfd(int fd)
{
	struct ctl_conn	*c;

	for (c = TAILQ_FIRST(&ctl_conns); c != NULL && c->iev.ibuf.fd != fd;
	    c = TAILQ_NEXT(c, entry))
		;	/* nothing */

	return (c);
}

void
control_close(int fd)
{
	struct ctl_conn	*c;

	if ((c = control_connbyfd(fd)) == NULL) {
		log_warnx("control_close: fd %d: not found", fd);
		return;
	}

	msgbuf_clear(&c->iev.ibuf.w);
	TAILQ_REMOVE(&ctl_conns, c, entry);

	event_del(&c->iev.ev);
	close(c->iev.ibuf.fd);
	free(c);
}

static int
send_stats(struct imsgev *iev)
{
	struct namespace	*ns;
	const struct btree_stat	*st;
	struct ns_stat		 nss;

	imsg_compose(&iev->ibuf, IMSG_CTL_STATS, 0, iev->ibuf.pid, -1,
	    &stats, sizeof(stats));

	TAILQ_FOREACH(ns, &conf->namespaces, next) {
		strlcpy(nss.suffix, ns->suffix, sizeof(nss.suffix));
		st = btree_stat(ns->data_db);
		bcopy(st, &nss.data_stat, sizeof(nss.data_stat));

		st = btree_stat(ns->indx_db);
		bcopy(st, &nss.indx_stat, sizeof(nss.indx_stat));

		imsg_compose(&iev->ibuf, IMSG_CTL_NSSTATS, 0, iev->ibuf.pid, -1,
		    &nss, sizeof(nss));
	}

	imsg_compose(&iev->ibuf, IMSG_CTL_END, 0, iev->ibuf.pid, -1, NULL, 0);

	return 0;
}

/* ARGSUSED */
void
control_dispatch_imsg(int fd, short event, void *arg)
{
	int			 n, verbose;
	struct control_sock	*cs = arg;
	struct ctl_conn		*c;
	struct imsg		 imsg;

	if ((c = control_connbyfd(fd)) == NULL) {
		log_warnx("control_dispatch_imsg: fd %d: not found", fd);
		return;
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&c->iev.ibuf.w) < 0) {
			control_close(fd);
			return;
		}
		imsg_event_add(&c->iev);
	}

	if (event & EV_READ) {
		if ((n = imsg_read(&c->iev.ibuf)) == -1 || n == 0) {
			control_close(fd);
			return;
		}
	} else
		return;

	for (;;) {
		if ((n = imsg_get(&c->iev.ibuf, &imsg)) == -1) {
			control_close(fd);
			return;
		}

		if (n == 0)
			break;

		log_debug("control_dispatch_imsg: imsg type %u", imsg.hdr.type);

		if (cs->cs_restricted || (c->flags & CTL_CONN_LOCKED)) {
			switch (imsg.hdr.type) {
			case IMSG_CTL_STATS:
				break;
			default:
				log_debug("control_dispatch_imsg: "
				    "client requested restricted command");
				imsg_free(&imsg);
				control_close(fd);
				return;
			}
		}

		switch (imsg.hdr.type) {
		case IMSG_CTL_STATS:
			if (send_stats(&c->iev) == -1) {
				log_debug("control_dispatch_imsg: "
				    "failed to send statistics");
				imsg_free(&imsg);
				control_close(fd);
				return;
			}
			break;
		case IMSG_CTL_LOG_VERBOSE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(verbose))
				break;

			memcpy(&verbose, imsg.data, sizeof(verbose));

			imsg_compose_event(iev_ldapd, IMSG_CTL_LOG_VERBOSE,
			    0, 0, -1, &verbose, sizeof(verbose));
			memcpy(imsg.data, &verbose, sizeof(verbose));
			control_imsg_forward(&imsg);

			log_verbose(verbose);
			break;
		default:
			log_debug("control_dispatch_imsg: "
			    "error handling imsg %d", imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}

	imsg_event_add(&c->iev);
}

void
control_imsg_forward(struct imsg *imsg)
{
	struct ctl_conn *c;

	TAILQ_FOREACH(c, &ctl_conns, entry)
		if (c->flags & CTL_CONN_NOTIFY)
			imsg_compose(&c->iev.ibuf, imsg->hdr.type, 0,
			    imsg->hdr.pid, -1, imsg->data,
			    imsg->hdr.len - IMSG_HEADER_SIZE);
}

void
control_end(struct ctl_conn *c)
{
	imsg_compose(&c->iev.ibuf, IMSG_CTL_END, 0, c->iev.ibuf.pid,
	    -1, NULL, 0);
	imsg_event_add(&c->iev);
}


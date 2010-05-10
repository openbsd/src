/*	$OpenBSD: control.c,v 1.35 2010/05/10 02:00:50 krw Exp $	*/

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

#include <sys/queue.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <net/if.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <openssl/ssl.h>

#include "relayd.h"

#define	CONTROL_BACKLOG	5

struct ctl_connlist ctl_conns;

struct ctl_conn	*control_connbyfd(int);
void		 control_close(int);

struct imsgev	*iev_main = NULL;
struct imsgev	*iev_hce = NULL;

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

	sun.sun_family = AF_UNIX;
	if (strlcpy(sun.sun_path, RELAYD_SOCKET,
	    sizeof(sun.sun_path)) >= sizeof(sun.sun_path)) {
		log_warn("control_init: %s name too long", RELAYD_SOCKET);
		close(fd);
		return (-1);
	}

	if (unlink(RELAYD_SOCKET) == -1)
		if (errno != ENOENT) {
			log_warn("control_init: unlink %s", RELAYD_SOCKET);
			close(fd);
			return (-1);
		}

	old_umask = umask(S_IXUSR|S_IXGRP|S_IWOTH|S_IROTH|S_IXOTH);
	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		log_warn("control_init: bind: %s", RELAYD_SOCKET);
		close(fd);
		(void)umask(old_umask);
		return (-1);
	}
	(void)umask(old_umask);

	if (chmod(RELAYD_SOCKET, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP) == -1) {
		log_warn("control_init: chmod");
		close(fd);
		(void)unlink(RELAYD_SOCKET);
		return (-1);
	}

	session_socket_blockmode(fd, BM_NONBLOCK);
	control_state.fd = fd;
	TAILQ_INIT(&ctl_conns);

	return (0);
}

int
control_listen(struct relayd *env, struct imsgev *i_main,
    struct imsgev *i_hce)
{

	iev_main = i_main;
	iev_hce = i_hce;

	if (listen(control_state.fd, CONTROL_BACKLOG) == -1) {
		log_warn("control_listen: listen");
		return (-1);
	}

	event_set(&control_state.ev, control_state.fd, EV_READ | EV_PERSIST,
	    control_accept, env);
	event_add(&control_state.ev, NULL);

	return (0);
}

void
control_cleanup(void)
{
	(void)unlink(RELAYD_SOCKET);
}

/* ARGSUSED */
void
control_accept(int listenfd, short event, void *arg)
{
	int			 connfd;
	socklen_t		 len;
	struct sockaddr_un	 sun;
	struct ctl_conn		*c;
	struct relayd		*env = arg;

	len = sizeof(sun);
	if ((connfd = accept(listenfd,
	    (struct sockaddr *)&sun, &len)) == -1) {
		if (errno != EWOULDBLOCK && errno != EINTR)
			log_warn("control_accept: accept");
		return;
	}

	session_socket_blockmode(connfd, BM_NONBLOCK);

	if ((c = malloc(sizeof(struct ctl_conn))) == NULL) {
		close(connfd);
		log_warn("control_accept");
		return;
	}

	imsg_init(&c->iev.ibuf, connfd);
	c->iev.handler = control_dispatch_imsg;
	c->iev.events = EV_READ;
	event_set(&c->iev.ev, c->iev.ibuf.fd, c->iev.events,
	    c->iev.handler, env);
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
		log_warn("control_close: fd %d: not found", fd);
		return;
	}

	msgbuf_clear(&c->iev.ibuf.w);
	TAILQ_REMOVE(&ctl_conns, c, entry);

	event_del(&c->iev.ev);
	close(c->iev.ibuf.fd);
	free(c);
}

/* ARGSUSED */
void
control_dispatch_imsg(int fd, short event, void *arg)
{
	struct ctl_conn		*c;
	struct imsg		 imsg;
	struct ctl_id		 id;
	int			 n;
	int			 verbose;
	struct relayd		*env = arg;

	if ((c = control_connbyfd(fd)) == NULL) {
		log_warn("control_dispatch_imsg: fd %d: not found", fd);
		return;
	}

	if (event & EV_READ) {
		if ((n = imsg_read(&c->iev.ibuf)) == -1 || n == 0) {
			control_close(fd);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&c->iev.ibuf.w) < 0) {
			control_close(fd);
			return;
		}
	}

	for (;;) {
		if ((n = imsg_get(&c->iev.ibuf, &imsg)) == -1) {
			control_close(fd);
			return;
		}

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_CTL_SHOW_SUM:
			show(c);
			break;
		case IMSG_CTL_SESSION:
			show_sessions(c);
			break;
		case IMSG_CTL_RDR_DISABLE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(id))
				fatalx("invalid imsg header len");
			memcpy(&id, imsg.data, sizeof(id));
			if (disable_rdr(c, &id))
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL,
				    0, 0, -1, NULL, 0);
			else {
				memcpy(imsg.data, &id, sizeof(id));
				control_imsg_forward(&imsg);
				imsg_compose_event(&c->iev, IMSG_CTL_OK,
				    0, 0, -1, NULL, 0);
			}
			break;
		case IMSG_CTL_RDR_ENABLE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(id))
				fatalx("invalid imsg header len");
			memcpy(&id, imsg.data, sizeof(id));
			if (enable_rdr(c, &id))
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL,
				    0, 0, -1, NULL, 0);
			else {
				memcpy(imsg.data, &id, sizeof(id));
				control_imsg_forward(&imsg);
				imsg_compose_event(&c->iev, IMSG_CTL_OK,
				    0, 0, -1, NULL, 0);
			}
			break;
		case IMSG_CTL_TABLE_DISABLE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(id))
				fatalx("invalid imsg header len");
			memcpy(&id, imsg.data, sizeof(id));
			if (disable_table(c, &id))
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL,
				    0, 0, -1, NULL, 0);
			else {
				memcpy(imsg.data, &id, sizeof(id));
				control_imsg_forward(&imsg);
				imsg_compose_event(&c->iev, IMSG_CTL_OK,
				    0, 0, -1, NULL, 0);
			}
			break;
		case IMSG_CTL_TABLE_ENABLE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(id))
				fatalx("invalid imsg header len");
			memcpy(&id, imsg.data, sizeof(id));
			if (enable_table(c, &id))
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL,
				    0, 0, -1, NULL, 0);
			else {
				memcpy(imsg.data, &id, sizeof(id));
				control_imsg_forward(&imsg);
				imsg_compose_event(&c->iev, IMSG_CTL_OK,
				    0, 0, -1, NULL, 0);
			}
			break;
		case IMSG_CTL_HOST_DISABLE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(id))
				fatalx("invalid imsg header len");
			memcpy(&id, imsg.data, sizeof(id));
			if (disable_host(c, &id, NULL))
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL,
				    0, 0, -1, NULL, 0);
			else {
				memcpy(imsg.data, &id, sizeof(id));
				control_imsg_forward(&imsg);
				imsg_compose_event(&c->iev, IMSG_CTL_OK,
				    0, 0, -1, NULL, 0);
			}
			break;
		case IMSG_CTL_HOST_ENABLE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(id))
				fatalx("invalid imsg header len");
			memcpy(&id, imsg.data, sizeof(id));
			if (enable_host(c, &id, NULL))
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL,
				    0, 0, -1, NULL, 0);
			else {
				memcpy(imsg.data, &id, sizeof(id));
				control_imsg_forward(&imsg);
				imsg_compose_event(&c->iev, IMSG_CTL_OK,
				    0, 0, -1, NULL, 0);
			}
			break;
		case IMSG_CTL_SHUTDOWN:
			imsg_compose_event(&c->iev, IMSG_CTL_FAIL,
			    0, 0, -1, NULL, 0);
			break;
		case IMSG_CTL_POLL:
			imsg_compose_event(iev_hce, IMSG_CTL_POLL,
			    0, 0,-1, NULL, 0);
			imsg_compose_event(&c->iev, IMSG_CTL_OK,
			    0, 0, -1, NULL, 0);
			break;
		case IMSG_CTL_RELOAD:
			if (env->sc_prefork_relay > 0) {
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL,
				    0, 0, -1, NULL, 0);
				break;
			}
			imsg_compose_event(iev_main, IMSG_CTL_RELOAD,
			    0, 0, -1, NULL, 0);
			/*
			 * we unconditionnaly return a CTL_OK imsg because
			 * we have no choice.
			 *
			 * so in this case, the reply relayctl gets means
			 * that the reload command has been set,
			 * it doesn't say whether the command succeeded or not.
			 */
			imsg_compose_event(&c->iev, IMSG_CTL_OK,
			    0, 0, -1, NULL, 0);
			break;
		case IMSG_CTL_NOTIFY:
			if (c->flags & CTL_CONN_NOTIFY) {
				log_debug("control_dispatch_imsg: "
				    "client requested notify more than once");
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL,
				    0, 0, -1, NULL, 0);
				break;
			}
			c->flags |= CTL_CONN_NOTIFY;
			break;
		case IMSG_CTL_LOG_VERBOSE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(verbose))
				break;

			memcpy(&verbose, imsg.data, sizeof(verbose));

			imsg_compose_event(iev_hce, IMSG_CTL_LOG_VERBOSE,
			    0, 0, -1, &verbose, sizeof(verbose));
			imsg_compose_event(iev_main, IMSG_CTL_LOG_VERBOSE,
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
			imsg_compose_event(&c->iev, imsg->hdr.type,
			    0, imsg->hdr.pid, -1, imsg->data,
			    imsg->hdr.len - IMSG_HEADER_SIZE);
}

void
session_socket_blockmode(int fd, enum blockmodes bm)
{
	int	flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		fatal("fcntl F_GETFL");

	if (bm == BM_NONBLOCK)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;

	if ((flags = fcntl(fd, F_SETFL, flags)) == -1)
		fatal("fcntl F_SETFL");
}

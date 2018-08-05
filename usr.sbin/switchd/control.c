/*	$OpenBSD: control.c,v 1.9 2018/08/05 08:16:24 mestre Exp $	*/

/*
 * Copyright (c) 2010-2016 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/tree.h>

#include <net/if.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <imsg.h>

#include "proc.h"
#include "switchd.h"

#define	CONTROL_BACKLOG	5

struct ctl_connlist ctl_conns;

void
	 control_accept(int, short, void *);
struct ctl_conn
	*control_connbyfd(int);
void	 control_close(int, struct control_sock *);
void	 control_dispatch_imsg(int, short, void *);
void	 control_imsg_forward(struct imsg *);
void	 control_run(struct privsep *, struct privsep_proc *, void *);

int	 control_dispatch_ofp(int, struct privsep_proc *, struct imsg *);

static struct privsep_proc procs[] = {
	{ "ofp",	PROC_OFP,	control_dispatch_ofp },
	{ "parent",	PROC_PARENT,	NULL },
	{ "ofcconn",	PROC_OFCCONN,	NULL }
};

void
control(struct privsep *ps, struct privsep_proc *p)
{
	proc_run(ps, p, procs, nitems(procs), control_run, NULL);
}

void
control_run(struct privsep *ps, struct privsep_proc *p, void *arg)
{
	/*
	 * pledge in the control process:
	 * stdio - for malloc and basic I/O including events.
	 * unix - for the control socket.
	 * recvfd - for the proc fd exchange.
	 */
	if (pledge("stdio unix recvfd", NULL) == -1)
		fatal("pledge");
}

int
control_dispatch_ofp(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	int		 cfd;
	struct ctl_conn	*c;
	uint8_t		*d = imsg->data;
	size_t		 s;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SWITCH:
	case IMSG_CTL_MAC:
		IMSG_SIZE_CHECK(imsg, &cfd);
		memcpy(&cfd, d, sizeof(cfd));

		if ((c = control_connbyfd(cfd)) == NULL)
			fatalx("invalid control connection");

		s = IMSG_DATA_SIZE(imsg) - sizeof(cfd);
		d += sizeof(cfd);
		imsg_compose_event(&c->iev, imsg->hdr.type, 0, 0, -1, d, s);
		return (0);
	case IMSG_CTL_END:
		IMSG_SIZE_CHECK(imsg, &cfd);
		memcpy(&cfd, d, sizeof(cfd));

		if ((c = control_connbyfd(cfd)) == NULL)
			fatalx("invalid control connection");

		imsg_compose_event(&c->iev, IMSG_CTL_END, 0, 0, -1, NULL, 0);
		return (0);

	default:
		break;
	}

	return (-1);
}

int
control_init(struct privsep *ps, struct control_sock *cs)
{
	struct switchd		*env = ps->ps_env;
	struct sockaddr_un	 sun;
	int			 fd;
	mode_t			 old_umask, mode;

	if (cs->cs_name == NULL)
		return (0);

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		log_warn("%s: socket", __func__);
		return (-1);
	}

	sun.sun_family = AF_UNIX;
	if (strlcpy(sun.sun_path, cs->cs_name,
	    sizeof(sun.sun_path)) >= sizeof(sun.sun_path)) {
		log_warn("%s: %s name too long", __func__, cs->cs_name);
		close(fd);
		return (-1);
	}

	if (unlink(cs->cs_name) == -1)
		if (errno != ENOENT) {
			log_warn("%s: unlink %s", __func__, cs->cs_name);
			close(fd);
			return (-1);
		}

	if (cs->cs_restricted) {
		old_umask = umask(S_IXUSR|S_IXGRP|S_IXOTH);
		mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
	} else {
		old_umask = umask(S_IXUSR|S_IXGRP|S_IWOTH|S_IROTH|S_IXOTH);
		mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP;
	}

	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		log_warn("%s: bind: %s", __func__, cs->cs_name);
		close(fd);
		(void)umask(old_umask);
		return (-1);
	}
	(void)umask(old_umask);

	if (chmod(cs->cs_name, mode) == -1) {
		log_warn("%s: chmod", __func__);
		close(fd);
		(void)unlink(cs->cs_name);
		return (-1);
	}

	socket_set_blockmode(fd, BM_NONBLOCK);
	cs->cs_fd = fd;
	cs->cs_env = env;

	return (0);
}

int
control_listen(struct control_sock *cs)
{
	if (cs->cs_name == NULL)
		return (0);

	if (listen(cs->cs_fd, CONTROL_BACKLOG) == -1) {
		log_warn("%s: listen", __func__);
		return (-1);
	}

	event_set(&cs->cs_ev, cs->cs_fd, EV_READ,
	    control_accept, cs);
	event_add(&cs->cs_ev, NULL);
	evtimer_set(&cs->cs_evt, control_accept, cs);

	return (0);
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

	event_add(&cs->cs_ev, NULL);
	if ((event & EV_TIMEOUT))
		return;

	len = sizeof(sun);
	if ((connfd = accept(listenfd,
	    (struct sockaddr *)&sun, &len)) == -1) {
		/*
		 * Pause accept if we are out of file descriptors, or
		 * libevent will haunt us here too.
		 */
		if (errno == ENFILE || errno == EMFILE) {
			struct timeval evtpause = { 1, 0 };

			event_del(&cs->cs_ev);
			evtimer_add(&cs->cs_evt, &evtpause);
		} else if (errno != EWOULDBLOCK && errno != EINTR &&
		    errno != ECONNABORTED)
			log_warn("%s: accept", __func__);
		return;
	}

	socket_set_blockmode(connfd, BM_NONBLOCK);

	if ((c = calloc(1, sizeof(struct ctl_conn))) == NULL) {
		log_warn("%s", __func__);
		close(connfd);
		return;
	}

	imsg_init(&c->iev.ibuf, connfd);
	c->iev.handler = control_dispatch_imsg;
	c->iev.events = EV_READ;
	c->iev.data = cs;
	event_set(&c->iev.ev, c->iev.ibuf.fd, c->iev.events,
	    c->iev.handler, c->iev.data);
	event_add(&c->iev.ev, NULL);

	TAILQ_INSERT_TAIL(&ctl_conns, c, entry);
}

struct ctl_conn *
control_connbyfd(int fd)
{
	struct ctl_conn	*c;

	TAILQ_FOREACH(c, &ctl_conns, entry) {
		if (c->iev.ibuf.fd == fd)
			break;
	}

	return (c);
}

void
control_close(int fd, struct control_sock *cs)
{
	struct ctl_conn	*c;

	if ((c = control_connbyfd(fd)) == NULL) {
		log_warn("%s: fd %d: not found", __func__, fd);
		return;
	}

	msgbuf_clear(&c->iev.ibuf.w);
	TAILQ_REMOVE(&ctl_conns, c, entry);

	event_del(&c->iev.ev);
	close(c->iev.ibuf.fd);

	/* Some file descriptors are available again. */
	if (evtimer_pending(&cs->cs_evt, NULL)) {
		evtimer_del(&cs->cs_evt);
		event_add(&cs->cs_ev, NULL);
	}

	free(c);
}

/* ARGSUSED */
void
control_dispatch_imsg(int fd, short event, void *arg)
{
	struct control_sock	*cs = arg;
	struct switchd		*env = cs->cs_env;
	struct ctl_conn		*c;
	struct imsg		 imsg;
	int			 n, v;

	if ((c = control_connbyfd(fd)) == NULL) {
		log_warn("%s: fd %d: not found", __func__, fd);
		return;
	}

	if (event & EV_READ) {
		if (((n = imsg_read(&c->iev.ibuf)) == -1 && errno != EAGAIN) ||
		    n == 0) {
			control_close(fd, cs);
			return;
		}
	}
	if (event & EV_WRITE) {
		if (msgbuf_write(&c->iev.ibuf.w) <= 0 && errno != EAGAIN) {
			control_close(fd, cs);
			return;
		}
	}

	for (;;) {
		if ((n = imsg_get(&c->iev.ibuf, &imsg)) == -1) {
			control_close(fd, cs);
			return;
		}

		if (n == 0)
			break;

		control_imsg_forward(&imsg);

		switch (imsg.hdr.type) {
		case IMSG_CTL_SHOW_SUM:
			/* Forward request and use control fd as _id_ */
			proc_compose(&env->sc_ps, PROC_OFP,
			    imsg.hdr.type, &fd, sizeof(fd));
			break;
		case IMSG_CTL_CONNECT:
		case IMSG_CTL_DISCONNECT:
			proc_compose(&env->sc_ps, PROC_PARENT,
			    imsg.hdr.type, imsg.data, IMSG_DATA_SIZE(&imsg));
			break;
		case IMSG_CTL_NOTIFY:
			if (c->flags & CTL_CONN_NOTIFY) {
				log_debug("%s: "
				    "client requested notify more than once",
				    __func__);
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL,
				    0, 0, -1, NULL, 0);
				break;
			}
			c->flags |= CTL_CONN_NOTIFY;
			break;
		case IMSG_CTL_VERBOSE:
			IMSG_SIZE_CHECK(&imsg, &v);

			memcpy(&v, imsg.data, sizeof(v));
			log_setverbose(v);

			proc_forward_imsg(&env->sc_ps, &imsg, PROC_PARENT, -1);
			proc_forward_imsg(&env->sc_ps, &imsg, PROC_OFP, -1);
			break;
		default:
			log_debug("%s: error handling imsg %d",
			    __func__, imsg.hdr.type);
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
			imsg_compose(&c->iev.ibuf, imsg->hdr.type,
			    0, imsg->hdr.pid, -1, imsg->data,
			    imsg->hdr.len - IMSG_HEADER_SIZE);
}

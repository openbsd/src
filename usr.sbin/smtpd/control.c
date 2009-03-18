/*	$OpenBSD: control.c,v 1.20 2009/03/18 14:48:27 gilles Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
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
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

#define CONTROL_BACKLOG 5

/* control specific headers */
struct {
	struct event		 ev;
	int			 fd;
} control_state;

__dead void	 control_shutdown(void);
int		 control_init(void);
int		 control_listen(struct smtpd *);
void		 control_cleanup(void);
void		 control_accept(int, short, void *);
struct ctl_conn	*control_connbyfd(int);
void		 control_close(int);
void		 control_sig_handler(int, short, void *);
void		 control_dispatch_ext(int, short, void *);
void		 control_dispatch_lka(int, short, void *);
void		 control_dispatch_mfa(int, short, void *);
void		 control_dispatch_queue(int, short, void *);
void		 control_dispatch_runner(int, short, void *);
void		 control_dispatch_smtp(int, short, void *);
void		 control_dispatch_parent(int, short, void *);

struct ctl_connlist	ctl_conns;

void
control_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		control_shutdown();
		break;
	default:
		fatalx("control_sig_handler: unexpected signal");
	}
}


pid_t
control(struct smtpd *env)
{
	struct sockaddr_un	 sun;
	int			 fd;
	mode_t			 old_umask;
	pid_t			 pid;
	struct passwd		*pw;
	struct event		 ev_sigint;
	struct event		 ev_sigterm;
	struct peer		 peers [] = {
		{ PROC_QUEUE,	 control_dispatch_queue },
		{ PROC_RUNNER,	 control_dispatch_runner },
		{ PROC_SMTP,	 control_dispatch_smtp },
		{ PROC_MFA,	 control_dispatch_mfa },
		{ PROC_PARENT,	 control_dispatch_parent },
	};

	switch (pid = fork()) {
	case -1:
		fatal("control: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		fatal("control: socket");

	sun.sun_family = AF_UNIX;
	if (strlcpy(sun.sun_path, SMTPD_SOCKET,
	    sizeof(sun.sun_path)) >= sizeof(sun.sun_path))
		fatal("control: socket name too long");

	if (unlink(SMTPD_SOCKET) == -1)
		if (errno != ENOENT)
			fatal("control: cannot unlink socket");

	old_umask = umask(S_IXUSR|S_IXGRP|S_IWOTH|S_IROTH|S_IXOTH);
	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		(void)umask(old_umask);
		fatal("control: bind");
	}
	(void)umask(old_umask);

	if (chmod(SMTPD_SOCKET, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH) == -1) {
		(void)unlink(SMTPD_SOCKET);
		fatal("control: chmod");
	}

	session_socket_blockmode(fd, BM_NONBLOCK);
	control_state.fd = fd;

#ifndef DEBUG
	if (chroot(pw->pw_dir) == -1)
		fatal("control: chroot");
	if (chdir("/") == -1)
		fatal("control: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
#endif

	setproctitle("control process");
	smtpd_process = PROC_CONTROL;

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("control: cannot drop privileges");
#endif

	event_init();

	signal_set(&ev_sigint, SIGINT, control_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, control_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	TAILQ_INIT(&ctl_conns);

	config_pipes(env, peers, 5);
	config_peers(env, peers, 5);
	control_listen(env);
	event_dispatch();
	control_shutdown();

	return (0);
}

void
control_shutdown(void)
{
	log_info("control process exiting");
	_exit(0);
}

int
control_listen(struct smtpd *env)
{
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
	(void)unlink(SMTPD_SOCKET);
}

/* ARGSUSED */
void
control_accept(int listenfd, short event, void *arg)
{
	int			 connfd;
	socklen_t		 len;
	struct sockaddr_un	 sun;
	struct ctl_conn		*c;
	struct smtpd		*env = arg;

	len = sizeof(sun);
	if ((connfd = accept(listenfd,
	    (struct sockaddr *)&sun, &len)) == -1) {
		if (errno != EWOULDBLOCK && errno != EINTR)
			log_warn("control_accept");
		return;
	}

	session_socket_blockmode(connfd, BM_NONBLOCK);

	if ((c = calloc(1, sizeof(struct ctl_conn))) == NULL) {
		close(connfd);
		log_warn("control_accept");
		return;
	}

	imsg_init(&c->ibuf, connfd, control_dispatch_ext);
	c->ibuf.events = EV_READ;
	c->ibuf.data = env;
	event_set(&c->ibuf.ev, c->ibuf.fd, c->ibuf.events,
	    c->ibuf.handler, env);
	event_add(&c->ibuf.ev, NULL);

	TAILQ_INSERT_TAIL(&ctl_conns, c, entry);
}

struct ctl_conn *
control_connbyfd(int fd)
{
	struct ctl_conn	*c;

	for (c = TAILQ_FIRST(&ctl_conns); c != NULL && c->ibuf.fd != fd;
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

	msgbuf_clear(&c->ibuf.w);
	TAILQ_REMOVE(&ctl_conns, c, entry);

	event_del(&c->ibuf.ev);
	close(c->ibuf.fd);
	free(c);
}

/* ARGSUSED */
void
control_dispatch_ext(int fd, short event, void *arg)
{
	struct ctl_conn		*c;
	struct smtpd		*env = arg;
	struct imsg		 imsg;
	int			 n;
	uid_t			 euid;
	gid_t			 egid;

	if (getpeereid(fd, &euid, &egid) == -1)
		fatal("getpeereid");

	if ((c = control_connbyfd(fd)) == NULL) {
		log_warn("control_dispatch_ext: fd %d: not found", fd);
		return;
	}

	switch (event) {
	case EV_READ:
		if ((n = imsg_read(&c->ibuf)) == -1 || n == 0) {
			control_close(fd);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&c->ibuf.w) < 0) {
			control_close(fd);
			return;
		}
		imsg_event_add(&c->ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(&c->ibuf, &imsg)) == -1) {
			control_close(fd);
			return;
		}

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_MFA_RCPT: {
			struct message_recipient *mr;

			if (c->state != CS_INIT && c->state != CS_RCPT)
				goto badstate;

			mr = imsg.data;
			imsg_compose(env->sc_ibufs[PROC_MFA], IMSG_MFA_RCPT, 0, 0, -1,
			    mr, sizeof(*mr));
			event_del(&c->ibuf.ev);
			break;
		}
		case IMSG_QUEUE_CREATE_MESSAGE: {
			struct message *messagep;

			if (c->state != CS_NONE && c->state != CS_DONE)
				goto badstate;

			messagep = imsg.data;
			messagep->session_id = fd;
			imsg_compose(env->sc_ibufs[PROC_QUEUE], IMSG_QUEUE_CREATE_MESSAGE, 0, 0, -1,
			    messagep, sizeof(*messagep));
			event_del(&c->ibuf.ev);
			break;
		}
		case IMSG_QUEUE_MESSAGE_FILE: {
			struct message *messagep;

			if (c->state != CS_RCPT)
				goto badstate;

			messagep = imsg.data;
			messagep->session_id = fd;
			imsg_compose(env->sc_ibufs[PROC_QUEUE], IMSG_QUEUE_MESSAGE_FILE, 0, 0, -1,
			    messagep, sizeof(*messagep));
			event_del(&c->ibuf.ev);
			break;
		}
		case IMSG_QUEUE_COMMIT_MESSAGE: {
			struct message *messagep;

			if (c->state != CS_FD)
				goto badstate;

			messagep = imsg.data;
			messagep->session_id = fd;
			imsg_compose(env->sc_ibufs[PROC_QUEUE], IMSG_QUEUE_COMMIT_MESSAGE, 0, 0, -1,
			    messagep, sizeof(*messagep));
			event_del(&c->ibuf.ev);
			break;
		}
		case IMSG_STATS: {
			struct stats	s;

			if (euid)
				goto badcred;

			s.fd = fd;
			imsg_compose(env->sc_ibufs[PROC_PARENT], IMSG_STATS, 0, 0, -1, &s, sizeof(s));
			imsg_compose(env->sc_ibufs[PROC_QUEUE], IMSG_STATS, 0, 0, -1, &s, sizeof(s));
			imsg_compose(env->sc_ibufs[PROC_RUNNER], IMSG_STATS, 0, 0, -1, &s, sizeof(s));
			imsg_compose(env->sc_ibufs[PROC_SMTP], IMSG_STATS, 0, 0, -1, &s, sizeof(s));
			break;
		}
		case IMSG_RUNNER_SCHEDULE: {
			struct sched s;

			if (euid)
				goto badcred;

			s = *(struct sched *)imsg.data;
			s.fd = fd;

			if (! valid_message_id(s.mid) && ! valid_message_uid(s.mid)) {
				imsg_compose(&c->ibuf, IMSG_CTL_FAIL, 0, 0, -1,
				    NULL, 0);
				break;
			}

			imsg_compose(env->sc_ibufs[PROC_RUNNER], IMSG_RUNNER_SCHEDULE, 0, 0, -1, &s, sizeof(s));
			break;
		}
		case IMSG_CTL_SHUTDOWN:
			/* NEEDS_FIX */
			log_debug("received shutdown request");

			if (euid)
				goto badcred;

			if (env->sc_flags & SMTPD_EXITING) {
				imsg_compose(&c->ibuf, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			env->sc_flags |= SMTPD_EXITING;
			imsg_compose(&c->ibuf, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;
		case IMSG_MDA_PAUSE:
			if (euid)
				goto badcred;

			if (env->sc_flags & SMTPD_MDA_PAUSED) {
				imsg_compose(&c->ibuf, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			env->sc_flags |= SMTPD_MDA_PAUSED;
			imsg_compose(env->sc_ibufs[PROC_RUNNER], IMSG_MDA_PAUSE,
			    0, 0, -1, NULL, 0);
			imsg_compose(&c->ibuf, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;
		case IMSG_MTA_PAUSE:
			if (euid)
				goto badcred;

			if (env->sc_flags & SMTPD_MTA_PAUSED) {
				imsg_compose(&c->ibuf, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			env->sc_flags |= SMTPD_MTA_PAUSED;
			imsg_compose(env->sc_ibufs[PROC_RUNNER], IMSG_MTA_PAUSE,
			    0, 0, -1, NULL, 0);
			imsg_compose(&c->ibuf, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;
		case IMSG_SMTP_PAUSE:
			if (euid)
				goto badcred;

			if (env->sc_flags & SMTPD_SMTP_PAUSED) {
				imsg_compose(&c->ibuf, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			env->sc_flags |= SMTPD_SMTP_PAUSED;
			imsg_compose(env->sc_ibufs[PROC_SMTP], IMSG_SMTP_PAUSE,			
			    0, 0, -1, NULL, 0);
			imsg_compose(&c->ibuf, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;
		case IMSG_MDA_RESUME:
			if (euid)
				goto badcred;

			if (! (env->sc_flags & SMTPD_MDA_PAUSED)) {
				imsg_compose(&c->ibuf, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			env->sc_flags &= ~SMTPD_MDA_PAUSED;
			imsg_compose(env->sc_ibufs[PROC_RUNNER], IMSG_MTA_RESUME,
			    0, 0, -1, NULL, 0);
			imsg_compose(&c->ibuf, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;
		case IMSG_MTA_RESUME:
			if (euid)
				goto badcred;

			if (!(env->sc_flags & SMTPD_MTA_PAUSED)) {
				imsg_compose(&c->ibuf, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			env->sc_flags &= ~SMTPD_MTA_PAUSED;
			imsg_compose(env->sc_ibufs[PROC_RUNNER], IMSG_MTA_RESUME,
			    0, 0, -1, NULL, 0);
			imsg_compose(&c->ibuf, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;
		case IMSG_SMTP_RESUME:
			if (euid)
				goto badcred;

			if (!(env->sc_flags & SMTPD_SMTP_PAUSED)) {
				imsg_compose(&c->ibuf, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			env->sc_flags &= ~SMTPD_SMTP_PAUSED;
			imsg_compose(env->sc_ibufs[PROC_SMTP], IMSG_SMTP_RESUME,
			    0, 0, -1, NULL, 0);
			imsg_compose(&c->ibuf, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;
		default:
			log_debug("control_dispatch_ext: "
			    "error handling imsg %d", imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
		continue;

badstate:
badcred:
		imsg_compose(&c->ibuf, IMSG_CTL_FAIL, 0, 0, -1,
		    NULL, 0);
	}

	imsg_event_add(&c->ibuf);
}

void
control_dispatch_parent(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_PARENT];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("control_dispatch_parent: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_STATS: {
			struct stats	*s;
			struct ctl_conn	*c;

			s = imsg.data;
			if ((c = control_connbyfd(s->fd)) == NULL) {
				log_warn("control_dispatch_parent: fd %d not found", s->fd);
				return;
			}

			imsg_compose(&c->ibuf, IMSG_PARENT_STATS, 0, 0, -1,
			    &s->u.parent, sizeof(s->u.parent));

			break;
		}
		default:
			log_debug("control_dispatch_parent: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
control_dispatch_lka(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_LKA];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("control_dispatch_lka: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_QUEUE_TEMPFAIL: {
			struct submit_status	 *ss;

			log_debug("GOT LFA REPLY");
			ss = imsg.data;
			if (ss->code != 250)
				log_debug("LKA FAILED WITH TEMPORARY ERROR");

			break;
		}
		default:
			log_debug("control_dispatch_lka: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
control_dispatch_mfa(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_MFA];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("control_dispatch_mfa: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_MFA_RCPT: {
			struct submit_status	 *ss;
			struct ctl_conn		*c;

			ss = imsg.data;
			if ((c = control_connbyfd(ss->id)) == NULL) {
				log_warn("control_dispatch_queue: fd %lld: not found", ss->id);
				return;
			}

			event_add(&c->ibuf.ev, NULL);
			if (ss->code == 250) {
				c->state = CS_RCPT;
				break;
			}

			imsg_compose(&c->ibuf, IMSG_CTL_FAIL, 0, 0, -1, NULL, 0);

			break;
		}
		default:
			log_debug("control_dispatch_mfa: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
control_dispatch_queue(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_QUEUE];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("control_dispatch_queue: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_QUEUE_CREATE_MESSAGE: {
			struct submit_status	 *ss;
			struct ctl_conn		*c;
			
			ss = imsg.data;
			if ((c = control_connbyfd(ss->id)) == NULL) {
				log_warn("control_dispatch_queue: fd %lld: not found", ss->id);
				return;
			}
			event_add(&c->ibuf.ev, NULL);

			if (ss->code != 250) {
				imsg_compose(&c->ibuf, IMSG_CTL_FAIL, 0, 0, -1,
				    NULL, 0);
			}
			else {
				c->state = CS_INIT;
				ss->msg.session_id = ss->id;
				strlcpy(ss->msg.message_id, ss->u.msgid,
				    sizeof(ss->msg.message_id));
				imsg_compose(&c->ibuf, IMSG_CTL_OK, 0, 0, -1,
				    &ss->msg, sizeof(struct message));
			}

			break;
		}
		case IMSG_QUEUE_COMMIT_ENVELOPES: {
			struct submit_status	 *ss;
			struct ctl_conn		*c;
			
			ss = imsg.data;
			if ((c = control_connbyfd(ss->id)) == NULL) {
				log_warn("control_dispatch_queue: fd %lld: not found", ss->id);
				return;
			}
			event_add(&c->ibuf.ev, NULL);
			c->state = CS_RCPT;
			imsg_compose(&c->ibuf, IMSG_CTL_OK, 0, 0, -1,
			    NULL, 0);

			break;
		}
		case IMSG_QUEUE_MESSAGE_FILE: {
			struct submit_status	 *ss;
			struct ctl_conn *c;
			int fd;

			ss = imsg.data;
			if ((c = control_connbyfd(ss->id)) == NULL) {
				log_warn("control_dispatch_queue: fd %lld: not found",
				    ss->id);
				return;
			}
			event_add(&c->ibuf.ev, NULL);

			fd = imsg_get_fd(ibuf, &imsg);
			if (ss->code == 250) {
				c->state = CS_FD;
				imsg_compose(&c->ibuf, IMSG_CTL_OK, 0, 0, fd,
				    &ss->msg, sizeof(struct message));
			}
			else
				imsg_compose(&c->ibuf, IMSG_CTL_FAIL, 0, 0, -1,
				    &ss->msg, sizeof(struct message));
			break;
		}
		case IMSG_QUEUE_COMMIT_MESSAGE: {
			struct submit_status	 *ss;
			struct ctl_conn *c;

			ss = imsg.data;
			if ((c = control_connbyfd(ss->id)) == NULL) {
				log_warn("control_dispatch_queue: fd %lld: not found",
				    ss->id);
				return;
			}
			event_add(&c->ibuf.ev, NULL);

			if (ss->code == 250) {
				c->state = CS_DONE;
				imsg_compose(&c->ibuf, IMSG_CTL_OK, 0, 0, -1,
				    &ss->msg, sizeof(struct message));
			}
			else
				imsg_compose(&c->ibuf, IMSG_CTL_FAIL, 0, 0, -1,
				    &ss->msg, sizeof(struct message));
			break;
		}
		case IMSG_STATS: {
			struct stats	*s;
			struct ctl_conn	*c;

			s = imsg.data;
			if ((c = control_connbyfd(s->fd)) == NULL) {
				log_warn("control_dispatch_queue: fd %d not found", s->fd);
				return;
			}

			imsg_compose(&c->ibuf, IMSG_QUEUE_STATS, 0, 0, -1,
			    &s->u.queue, sizeof(s->u.queue));

			break;
		}
		default:
			log_debug("control_dispatch_queue: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
control_dispatch_runner(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_RUNNER];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("control_dispatch_runner: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_STATS: {
			struct stats	*s;
			struct ctl_conn	*c;

			s = imsg.data;
			if ((c = control_connbyfd(s->fd)) == NULL) {
				log_warn("control_dispatch_runner: fd %d not found", s->fd);
				return;
			}

			imsg_compose(&c->ibuf, IMSG_RUNNER_STATS, 0, 0, -1,
			    &s->u.runner, sizeof(s->u.runner));

			break;
		}
		case IMSG_RUNNER_SCHEDULE: {
			struct sched	*s;
			struct ctl_conn	*c;

			s = imsg.data;
			if ((c = control_connbyfd(s->fd)) == NULL) {
				log_warn("control_dispatch_runner: fd %d not found", s->fd);
				return;
			}

			if (s->ret)
				imsg_compose(&c->ibuf, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			else
				imsg_compose(&c->ibuf, IMSG_CTL_FAIL, 0, 0, -1, NULL, 0);
			break;
		}
		default:
			log_debug("control_dispatch_runner: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
control_dispatch_smtp(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_SMTP];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("control_dispatch_smtp: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_STATS: {
			struct stats	*s;
			struct ctl_conn	*c;

			s = imsg.data;
			if ((c = control_connbyfd(s->fd)) == NULL) {
				log_warn("control_dispatch_queue: fd %d not found", s->fd);
				return;
			}

			imsg_compose(&c->ibuf, IMSG_SMTP_STATS, 0, 0, -1,
			    &s->u.smtp, sizeof(s->u.smtp));

			break;
		}
		default:
			log_debug("control_dispatch_smtp: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
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

/*	$OpenBSD: mta.c,v 1.12 2009/01/01 16:15:47 jacekm Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
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
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <event.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

__dead void	mta_shutdown(void);
void		mta_sig_handler(int, short, void *);
void		mta_dispatch_parent(int, short, void *);
void		mta_dispatch_queue(int, short, void *);
void		mta_dispatch_runner(int, short, void *);
void		mta_setup_events(struct smtpd *);
void		mta_disable_events(struct smtpd *);
void		mta_timeout(int, short, void *);
void		mta_write(int, short, void *);
int		mta_connect(struct batch *);
void		mta_read_handler(struct bufferevent *, void *);
void		mta_write_handler(struct bufferevent *, void *);
void		mta_error_handler(struct bufferevent *, short, void *);
int		mta_reply_handler(struct bufferevent *, void *);
void		mta_batch_update_queue(struct batch *);

void
mta_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		mta_shutdown();
		break;
	default:
		fatalx("mta_sig_handler: unexpected signal");
	}
}

void
mta_dispatch_parent(int sig, short event, void *p)
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
			fatal("parent_dispatch_mta: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("parent_dispatch_mta: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
mta_dispatch_queue(int sig, short event, void *p)
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
			fatal("parent_dispatch_mta: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_QUEUE_MESSAGE_FD: {
			struct batch	*batchp;
			int fd;

			if ((fd = imsg_get_fd(ibuf, &imsg)) == -1) {
				/* NEEDS_FIX - unsure yet how it must be handled */
				fatalx("mta_dispatch_queue: imsg_get_fd");
			}

			batchp = (struct batch *)imsg.data;
			batchp = batch_by_id(env, batchp->id);

			if ((batchp->messagefp = fdopen(fd, "r")) == NULL)
				fatal("mta_dispatch_queue: fdopen");

			evbuffer_add_printf(batchp->bev->output, "DATA\r\n");

			bufferevent_enable(batchp->bev, EV_WRITE|EV_READ);
			break;
		}
		default:
			log_debug("parent_dispatch_mta: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
mta_dispatch_runner(int sig, short event, void *p)
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
			fatal("mta_dispatch_runner: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_BATCH_CREATE: {
			struct batch *batchp;

			batchp = calloc(1, sizeof (struct batch));
			if (batchp == NULL)
				fatal("mta_dispatch_runner: calloc");

			*batchp = *(struct batch *)imsg.data;
			batchp->mx_off = 0;
			batchp->env = env;
			batchp->flags = 0;

			TAILQ_INIT(&batchp->messages);
			SPLAY_INSERT(batchtree, &env->batch_queue, batchp);

			break;
		}
		case IMSG_BATCH_APPEND: {
			struct batch	*batchp;
			struct message	*messagep;

			messagep = calloc(1, sizeof (struct message));
			if (messagep == NULL)
				fatal("mta_dispatch_runner: calloc");

			*messagep = *(struct message *)imsg.data;

			batchp = batch_by_id(env, messagep->batch_id);
			if (batchp == NULL)
				fatalx("mta_dispatch_runner: internal inconsistency.");

			TAILQ_INSERT_TAIL(&batchp->messages, messagep, entry);

			break;
		}
		case IMSG_BATCH_CLOSE: {
			struct batch	*batchp;

			batchp = (struct batch *)imsg.data;
			batchp = batch_by_id(env, batchp->id);
			if (batchp == NULL)
				fatalx("mta_dispatch_runner: internal inconsistency.");

			batchp->flags |= F_BATCH_COMPLETE;

			while (! mta_connect(batchp)) {
				if (batchp->mx_off == batchp->mx_cnt) {
					break;
				}
			}
			break;
		}
		default:
			log_debug("mta_dispatch_runner: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
mta_shutdown(void)
{
	log_info("mail transfer agent exiting");
	_exit(0);
}

void
mta_setup_events(struct smtpd *env)
{
	struct timeval	 tv;

	evtimer_set(&env->sc_ev, mta_timeout, env);
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	evtimer_add(&env->sc_ev, &tv);
}

void
mta_disable_events(struct smtpd *env)
{
	evtimer_del(&env->sc_ev);
}

void
mta_timeout(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct timeval		 tv;

	tv.tv_sec = 3;
	tv.tv_usec = 0;
	evtimer_add(&env->sc_ev, &tv);
}

pid_t
mta(struct smtpd *env)
{
	pid_t		 pid;

	struct passwd	*pw;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_QUEUE,	mta_dispatch_queue },
		{ PROC_RUNNER,	mta_dispatch_runner }
	};

	switch (pid = fork()) {
	case -1:
		fatal("mta: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;
#ifndef DEBUG
	if (chroot(pw->pw_dir) == -1)
		fatal("mta: chroot");
	if (chdir("/") == -1)
		fatal("mta: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
#endif

	setproctitle("mail transfer agent");
	smtpd_process = PROC_MTA;

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("mta: cannot drop privileges");
#endif

	event_init();

	signal_set(&ev_sigint, SIGINT, mta_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, mta_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_peers(env, peers, 2);

	SPLAY_INIT(&env->batch_queue);

	mta_setup_events(env);
	event_dispatch();
	mta_shutdown();

	return (0);
}

/* shamelessly ripped usr.sbin/relayd/check_tcp.c ;) */
int
mta_connect(struct batch *batchp)
{
	int s;
	int type;
	struct linger lng;
	struct sockaddr_in ssin;
	struct sockaddr_in6 ssin6;

	if ((s = socket(batchp->mxarray[batchp->mx_off].ss.ss_family, SOCK_STREAM, 0)) == -1) {
		goto bad;
	}

	bzero(&lng, sizeof(lng));
	if (setsockopt(s, SOL_SOCKET, SO_LINGER, &lng, sizeof (lng)) == -1) {
		goto bad;
	}

	type = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &type, sizeof (type)) == -1) {
		goto bad;
	}

	session_socket_blockmode(s, BM_NONBLOCK);

	if (batchp->mxarray[batchp->mx_off].ss.ss_family == PF_INET) {
		ssin = *(struct sockaddr_in *)&batchp->mxarray[batchp->mx_off].ss;
		if (connect(s, (struct sockaddr *)&ssin, sizeof(struct sockaddr_in)) == -1) {
			if (errno != EINPROGRESS) {
				goto bad;
			}
		}
	}

	if (batchp->mxarray[batchp->mx_off].ss.ss_family == PF_INET6) {
		ssin6 = *(struct sockaddr_in6 *)&batchp->mxarray[batchp->mx_off].ss;
		if (connect(s, (struct sockaddr *)&ssin6, sizeof(struct sockaddr_in6)) == -1) {
			if (errno != EINPROGRESS) {
				goto bad;
			}
		}
	}

	batchp->tv.tv_sec = SMTPD_CONNECT_TIMEOUT;
	batchp->tv.tv_usec = 0;
	batchp->peerfd = s;
	event_set(&batchp->ev, s, EV_TIMEOUT|EV_WRITE, mta_write, batchp);
	event_add(&batchp->ev, &batchp->tv);

	return 1;

bad:
	batchp->mx_off++;
	close(s);
	return 0;
}

void
mta_write(int s, short event, void *arg)
{
	struct batch *batchp = arg;
	int ret;

	if (event == EV_TIMEOUT) {
		batchp->mx_off++;
		close(s);
		if (batchp->bev) {
			bufferevent_free(batchp->bev);
			batchp->bev = NULL;
		}
		strlcpy(batchp->errorline, "connection timed-out.", MAX_LINE_SIZE);

		ret = 0;
		while (batchp->mx_off < batchp->mx_cnt &&
		    (ret = mta_connect(batchp)) == 0) {
			continue;
		}
		if (ret)
			return;

		mta_batch_update_queue(batchp);
		return;
	}

	batchp->bev = bufferevent_new(s, mta_read_handler, mta_write_handler,
	    mta_error_handler, batchp);

	if (batchp->bev == NULL) {
		mta_batch_update_queue(batchp);
		close(s);
		return;
	}

	bufferevent_enable(batchp->bev, EV_READ|EV_WRITE);
}

void
mta_read_handler(struct bufferevent *bev, void *arg)
{
	while (mta_reply_handler(bev, arg))
		;
}

int
mta_reply_handler(struct bufferevent *bev, void *arg)
{
	struct batch *batchp = arg;
	struct smtpd *env = batchp->env;
	struct message *messagep = NULL;
	char *line;
	int i;
	int code;
#define F_ISINFO	0x1
#define F_ISPROTOERROR	0x2
	char codebuf[4];
	const char *errstr;
	int flags = 0;

	line = evbuffer_readline(bev->input);
	if (line == NULL) {
		bufferevent_enable(bev, EV_READ|EV_WRITE);
		return 0;
	}

	bufferevent_enable(bev, EV_READ|EV_WRITE);

	log_debug("remote server sent: [%s]", line);

	strlcpy(codebuf, line, sizeof codebuf);
	code = strtonum(codebuf, 0, UINT16_MAX, &errstr);
	if (errstr || code < 100) {
		/* Server sent invalid line, protocol error */
		batchp->status |= S_BATCH_PERMFAILURE;
		strlcpy(batchp->errorline, line, MAX_LINE_SIZE);
		mta_batch_update_queue(batchp);
		return 0;
	}

	if (line[3] == '-') {
		return 1;
	}

	switch (code) {
	case 250:
		if (batchp->state == S_DONE) {
			mta_batch_update_queue(batchp);
			return 0;
		}
		break;

	case 220:
		evbuffer_add_printf(batchp->bev->output, "EHLO %s\r\n", env->sc_hostname);
		batchp->state = S_GREETED;
		return 1;

	case 421:
	case 450:
	case 451:
		batchp->status |= S_BATCH_TEMPFAILURE;
		strlcpy(batchp->errorline, line, MAX_LINE_SIZE);
		mta_batch_update_queue(batchp);
		return 0;

		/* The following codes are state dependant and will cause
		 * a batch rejection if returned at the wrong state.
		 */
	case 530:
	case 550:
		if (batchp->state == S_RCPT) {
			batchp->messagep->status = (S_MESSAGE_REJECTED|S_MESSAGE_PERMFAILURE);
			strlcpy(batchp->messagep->session_errorline, line, MAX_LINE_SIZE);
			break;
		}
	case 354:
		if (batchp->state == S_RCPT && batchp->messagep == NULL) {
			batchp->state = S_DATA;
			break;
		}

	case 221:
		if (batchp->state == S_DONE) {
			mta_batch_update_queue(batchp);
			return 0;
		}

	case 552:
	case 553:
		flags |= F_ISPROTOERROR;
	default:
		/* Server sent code we know nothing about, error */
		if (!(flags & F_ISPROTOERROR))
			log_debug("Ouch, SMTP session returned unhandled %d status.", code);

		batchp->status |= S_BATCH_PERMFAILURE;
		strlcpy(batchp->errorline, line, MAX_LINE_SIZE);
		mta_batch_update_queue(batchp);
		return 0;
	}


	switch (batchp->state) {
	case S_GREETED: {
		char *user;
		char *domain;

		if (batchp->type & T_DAEMON_BATCH) {
			user = "MAILER-DAEMON";
			domain = env->sc_hostname;
		}
		else {
			messagep = TAILQ_FIRST(&batchp->messages);
			user = messagep->sender.user;
			domain = messagep->sender.domain;
		}

		if (user[0] == '\0' && domain[0] == '\0')
			evbuffer_add_printf(batchp->bev->output, "MAIL FROM:<>\r\n");
		else
			evbuffer_add_printf(batchp->bev->output, "MAIL FROM:<%s@%s>\r\n", user, domain);
		batchp->state = S_MAIL;

		break;
	}

	case S_MAIL:
		batchp->state = S_RCPT;

	case S_RCPT: {
		char *user;
		char *domain;

		/* Is this the first RCPT ? */
		if (batchp->messagep == NULL)
			messagep = TAILQ_FIRST(&batchp->messages);
		else {
			/* We already had a RCPT, mark is as accepted and
			 * fetch next one from queue if we aren't dealing
			 * with a daemon batch.
			 */
			if (batchp->type & T_DAEMON_BATCH)
				messagep = NULL;
			else {
				messagep = batchp->messagep;
				if ((messagep->status & S_MESSAGE_REJECTED) == 0)
					messagep->status = S_MESSAGE_ACCEPTED;
				messagep = TAILQ_NEXT(batchp->messagep, entry);
			}
		}
		batchp->messagep = messagep;

		if (messagep) {
			if (batchp->type & T_DAEMON_BATCH) {
				user = messagep->sender.user;
				domain = messagep->sender.domain;
			}
			else {
				user = messagep->recipient.user;
				domain = messagep->recipient.domain;
			}
			evbuffer_add_printf(batchp->bev->output, "RCPT TO:<%s@%s>\r\n", user, domain);
		}
		else {
			/* Do we have at least one accepted recipient ? */
			if ((batchp->type & T_DAEMON_BATCH) == 0) {
				TAILQ_FOREACH(messagep, &batchp->messages, entry) {
					if (messagep->status & S_MESSAGE_ACCEPTED)
						break;
				}
				if (messagep == NULL) {
					batchp->status |= S_BATCH_PERMFAILURE;
					mta_batch_update_queue(batchp);
					return 0;
				}
			}

			bufferevent_disable(batchp->bev, EV_WRITE|EV_READ);
			imsg_compose(env->sc_ibufs[PROC_QUEUE], IMSG_QUEUE_MESSAGE_FD,
			    0, 0, -1, batchp, sizeof(*batchp));
		}
		break;
	}

	case S_DATA: {
		bufferevent_enable(batchp->bev, EV_READ|EV_WRITE);

		evbuffer_add_printf(batchp->bev->output,
		    "Received: from %s (%s [%s])\r\n"
		    "\tby %s with ESMTP id %s\r\n",
		    "localhost", "localhost", "127.0.0.1",
		    "", batchp->message_id);

		evbuffer_add_printf(batchp->bev->output, "X-OpenSMTPD: experiment\r\n");

		if (batchp->type & T_DAEMON_BATCH) {
			evbuffer_add_printf(batchp->bev->output,
			    "Hi !\r\n\r\n"
			    "This is the MAILER-DAEMON, please DO NOT REPLY to this e-mail it is\r\n"
			    "just a notification to let you know that an error has occured.\r\n\r\n");

			if (batchp->status & S_BATCH_PERMFAILURE) {
				evbuffer_add_printf(batchp->bev->output,
				    "You ran into a PERMANENT FAILURE, which means that the e-mail can't\r\n"
				    "be delivered to the remote host no matter how much I'll try.\r\n\r\n"
				    "Diagnostic:\r\n"
				    "%s\r\n\r\n", batchp->errorline);
			}

			if (batchp->status & S_BATCH_TEMPFAILURE) {
				evbuffer_add_printf(batchp->bev->output,
				    "You ran into a TEMPORARY FAILURE, which means that the e-mail can't\r\n"
				    "be delivered right now, but could be deliberable at a later time. I\r\n"
				    "will attempt until it succeeds for the next four days, then let you\r\n"
				    "know if it didn't work out.\r\n"
				    "Diagnostic:\r\n"
				    "%s\r\n\r\n", batchp->errorline);
			}

			i = 0;
			TAILQ_FOREACH(messagep, &batchp->messages, entry) {
				if (messagep->status & S_MESSAGE_TEMPFAILURE) {
					if (i == 0) {
						evbuffer_add_printf(batchp->bev->output,
						    "The following recipients caused a temporary failure:\r\n");
						++i;
					}

					evbuffer_add_printf(batchp->bev->output,
					    "\t<%s@%s>:\r\n%s\r\n\r\n",
					    messagep->recipient.user, messagep->recipient.domain, messagep->session_errorline);
				}
			}

			i = 0;
			TAILQ_FOREACH(messagep, &batchp->messages, entry) {
				if (messagep->status & S_MESSAGE_PERMFAILURE) {
					if (i == 0) {
						evbuffer_add_printf(batchp->bev->output,
						    "The following recipients caused a permanent failure:\r\n");
						++i;
					}

					evbuffer_add_printf(batchp->bev->output,
					    "\t<%s@%s>:\r\n%s\r\n\r\n",
					    messagep->recipient.user, messagep->recipient.domain, messagep->session_errorline);
				}
			}

			evbuffer_add_printf(batchp->bev->output,
			    "Below is a copy of the original message:\r\n\r\n");
		}

		break;
	}
	case S_DONE:
		evbuffer_add_printf(batchp->bev->output, "QUIT\r\n");
		batchp->state = S_QUIT;
		break;

	default:
		log_info("unknown command: %d", batchp->state);
	}

	return 1;
}

void
mta_write_handler(struct bufferevent *bev, void *arg)
{
	struct batch *batchp = arg;
	char *buf, *lbuf;
	size_t len;

	if (batchp->state == S_QUIT) {
		bufferevent_disable(bev, EV_READ|EV_WRITE);
		log_debug("closing connection because of QUIT");
		close(batchp->peerfd);
		return;
	}

	/* Progressively fill the output buffer with data */
	if (batchp->state == S_DATA) {

		lbuf = NULL;
		if ((buf = fgetln(batchp->messagefp, &len))) {
			if (buf[len - 1] == '\n')
				buf[len - 1] = '\0';
			else {
				if ((lbuf = malloc(len + 1)) == NULL)
					fatal("mta_write_handler: malloc");
				memcpy(lbuf, buf, len);
				lbuf[len] = '\0';
				buf = lbuf;
			}
			evbuffer_add_printf(batchp->bev->output, "%s\r\n", buf);
			free(lbuf);
			lbuf = NULL;
		}
		else {
			evbuffer_add_printf(batchp->bev->output, ".\r\n");
			batchp->state = S_DONE;
			fclose(batchp->messagefp);
			batchp->messagefp = NULL;
		}
	}
	bufferevent_enable(batchp->bev, EV_READ|EV_WRITE);
}

void
mta_error_handler(struct bufferevent *bev, short error, void *arg)
{
	struct batch *batchp = arg;
	if (error & (EVBUFFER_TIMEOUT|EVBUFFER_EOF)) {
		bufferevent_disable(bev, EV_READ|EV_WRITE);
		log_debug("closing connection because of an error");
		close(batchp->peerfd);
		return;
	}
}

void
mta_batch_update_queue(struct batch *batchp)
{
	struct smtpd *env = batchp->env;
	struct message *messagep;

	while ((messagep = TAILQ_FIRST(&batchp->messages)) != NULL) {

		if (batchp->status & S_BATCH_PERMFAILURE) {
			messagep->status |= S_MESSAGE_PERMFAILURE;
		}

		if (batchp->status & S_BATCH_TEMPFAILURE) {
			if (messagep->status != S_MESSAGE_PERMFAILURE)
				messagep->status |= S_MESSAGE_TEMPFAILURE;
		}

		imsg_compose(env->sc_ibufs[PROC_QUEUE],
		    IMSG_QUEUE_MESSAGE_UPDATE, 0, 0, -1, messagep,
		    sizeof(struct message));
		TAILQ_REMOVE(&batchp->messages, messagep, entry);
		free(messagep);
	}

	SPLAY_REMOVE(batchtree, &env->batch_queue, batchp);

	if (batchp->messagefp)
		fclose(batchp->messagefp);

	if (batchp->bev)
		bufferevent_free(batchp->bev);

	if (batchp->peerfd != -1)
		close(batchp->peerfd);

	free(batchp);
}

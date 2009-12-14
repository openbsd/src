/*	$OpenBSD: queue.c,v 1.75 2009/12/14 16:44:14 jacekm Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
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
#include <sys/stat.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <libgen.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

__dead void	queue_shutdown(void);
void		queue_sig_handler(int, short, void *);
void		queue_dispatch_control(int, short, void *);
void		queue_dispatch_smtp(int, short, void *);
void		queue_dispatch_mda(int, short, void *);
void		queue_dispatch_mta(int, short, void *);
void		queue_dispatch_lka(int, short, void *);
void		queue_dispatch_runner(int, short, void *);
void		queue_setup_events(struct smtpd *);
void		queue_disable_events(struct smtpd *);
void		queue_purge(char *);

int		queue_create_layout_message(char *, char *);
void		queue_delete_layout_message(char *, char *);
int		queue_record_layout_envelope(char *, struct message *);
int		queue_remove_layout_envelope(char *, struct message *);
int		queue_commit_layout_message(char *, struct message *);
int		queue_open_layout_messagefile(char *, struct message *);

void		queue_submit_envelope(struct smtpd *, struct message *);
void	        queue_commit_envelopes(struct smtpd *, struct message*);

void
queue_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		queue_shutdown();
		break;
	default:
		fatalx("queue_sig_handler: unexpected signal");
	}
}

void
queue_dispatch_control(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_CONTROL];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("queue_dispatch_control: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_warnx("queue_dispatch_control: got imsg %d",
			    imsg.hdr.type);
			fatalx("queue_dispatch_control: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
queue_dispatch_smtp(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_SMTP];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("queue_dispatch_smtp: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_QUEUE_CREATE_MESSAGE: {
			struct message		*messagep = imsg.data;
			struct submit_status	 ss;
			int			(*f)(char *);

			log_debug("queue_dispatch_smtp: creating message file");

			IMSG_SIZE_CHECK(messagep);

			ss.id = messagep->session_id;
			ss.code = 250;
			bzero(ss.u.msgid, MAX_ID_SIZE);

			if (messagep->flags & F_MESSAGE_ENQUEUED)
				f = enqueue_create_layout;
			else
				f = queue_create_incoming_layout;

			if (! f(ss.u.msgid))
				ss.code = 421;

			imsg_compose_event(iev, IMSG_QUEUE_CREATE_MESSAGE, 0, 0, -1,
			    &ss, sizeof(ss));
			break;
		}
		case IMSG_QUEUE_REMOVE_MESSAGE: {
			struct message		*messagep = imsg.data;
			void			(*f)(char *);

			IMSG_SIZE_CHECK(messagep);

			if (messagep->flags & F_MESSAGE_ENQUEUED)
				f = enqueue_delete_message;
			else
				f = queue_delete_incoming_message;

			f(messagep->message_id);

			break;
		}
		case IMSG_QUEUE_COMMIT_MESSAGE: {
			struct message		*messagep = imsg.data;
			struct submit_status	 ss;
			size_t			*counter;
			int			(*f)(struct message *);

			IMSG_SIZE_CHECK(messagep);

			ss.id = messagep->session_id;

			if (messagep->flags & F_MESSAGE_ENQUEUED) {
				f = enqueue_commit_message;
				counter = &env->stats->queue.inserts_local;
			} else {
				f = queue_commit_incoming_message;
				counter = &env->stats->queue.inserts_remote;
			}

			if (f(messagep))
				(*counter)++;
			else
				ss.code = 421;

			imsg_compose_event(iev, IMSG_QUEUE_COMMIT_MESSAGE, 0, 0, -1,
			    &ss, sizeof(ss));

			break;
		}
		case IMSG_QUEUE_MESSAGE_FILE: {
			struct message		*messagep = imsg.data;
			struct submit_status	 ss;
			int fd;
			int			(*f)(struct message *);

			IMSG_SIZE_CHECK(messagep);

			ss.id = messagep->session_id;

			if (messagep->flags & F_MESSAGE_ENQUEUED)
				f = enqueue_open_messagefile;
			else
				f = queue_open_incoming_message_file;

			fd = f(messagep);
			if (fd == -1)
				ss.code = 421;

			imsg_compose_event(iev, IMSG_QUEUE_MESSAGE_FILE, 0, 0, fd,
			    &ss, sizeof(ss));
			break;
		}
		default:
			log_warnx("queue_dispatch_smtp: got imsg %d",
			    imsg.hdr.type);
			fatalx("queue_dispatch_smtp: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
queue_dispatch_mda(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_MDA];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("queue_dispatch_mda: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {

		case IMSG_QUEUE_MESSAGE_UPDATE: {
			imsg_compose_event(env->sc_ievs[PROC_RUNNER], IMSG_RUNNER_UPDATE_ENVELOPE,
			    0, 0, -1, imsg.data, sizeof(struct message));
			break;
		}

		default:
			log_warnx("got imsg %d", imsg.hdr.type);
			fatalx("queue_dispatch_mda: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
queue_dispatch_mta(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_MTA];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("queue_dispatch_mta: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {

		case IMSG_QUEUE_MESSAGE_FD: {
			struct batch *batchp = imsg.data;
			int fd;

			IMSG_SIZE_CHECK(batchp);

			fd = queue_open_message_file(batchp->message_id);
			imsg_compose_event(iev,  IMSG_QUEUE_MESSAGE_FD, 0, 0, fd, batchp,
			    sizeof(*batchp));
			break;
		}

		case IMSG_QUEUE_MESSAGE_UPDATE: {
			imsg_compose_event(env->sc_ievs[PROC_RUNNER], IMSG_RUNNER_UPDATE_ENVELOPE,
			    0, 0, -1, imsg.data, sizeof(struct message));
			break;
		}

		default:
			log_warnx("got imsg %d", imsg.hdr.type);
			fatalx("queue_dispatch_mda: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
queue_dispatch_lka(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_LKA];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("queue_dispatch_lka: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {

		case IMSG_QUEUE_SUBMIT_ENVELOPE: {
			struct message		*messagep = imsg.data;
			struct submit_status	 ss;
			int (*f)(struct message *);

			IMSG_SIZE_CHECK(messagep);

			messagep->id = generate_uid();
			ss.id = messagep->session_id;

			if (IS_MAILBOX(messagep->recipient) ||
			    IS_EXT(messagep->recipient))
				messagep->type = T_MDA_MESSAGE;
			else
				messagep->type = T_MTA_MESSAGE;

			/* Write to disk */
			if (messagep->flags & F_MESSAGE_ENQUEUED)
				f = enqueue_record_envelope;
			else
				f = queue_record_incoming_envelope;

			if (! f(messagep)) {
				ss.code = 421;
				imsg_compose_event(env->sc_ievs[PROC_SMTP],
				    IMSG_QUEUE_TEMPFAIL, 0, 0, -1, &ss,
				    sizeof(ss));
			}

			break;
		}

		case IMSG_QUEUE_COMMIT_ENVELOPES: {
			struct message		*messagep = imsg.data;
			struct submit_status	 ss;

			IMSG_SIZE_CHECK(messagep);

			ss.id = messagep->session_id;
			ss.code = 250;

			imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_QUEUE_COMMIT_ENVELOPES,
			    0, 0, -1, &ss, sizeof(ss));

			break;
		}

		default:
			log_warnx("got imsg %d", imsg.hdr.type);
			fatalx("queue_dispatch_lka: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
queue_dispatch_runner(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_RUNNER];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("queue_dispatch_runner: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_warnx("got imsg %d", imsg.hdr.type);
			fatalx("queue_dispatch_runner: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
queue_shutdown(void)
{
	log_info("queue handler exiting");
	_exit(0);
}

void
queue_setup_events(struct smtpd *env)
{
}

void
queue_disable_events(struct smtpd *env)
{
}

pid_t
queue(struct smtpd *env)
{
	pid_t		 pid;
	struct passwd	*pw;

	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_CONTROL,	queue_dispatch_control },
		{ PROC_SMTP,	queue_dispatch_smtp },
		{ PROC_MDA,	queue_dispatch_mda },
		{ PROC_MTA,	queue_dispatch_mta },
		{ PROC_LKA,	queue_dispatch_lka },
		{ PROC_RUNNER,	queue_dispatch_runner }
	};

	switch (pid = fork()) {
	case -1:
		fatal("queue: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;

#ifndef DEBUG
	if (chroot(PATH_SPOOL) == -1)
		fatal("queue: chroot");
	if (chdir("/") == -1)
		fatal("queue: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
#endif

	smtpd_process = PROC_QUEUE;
	setproctitle("%s", env->sc_title[smtpd_process]);

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("queue: cannot drop privileges");
#endif

	event_init();

	signal_set(&ev_sigint, SIGINT, queue_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, queue_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/*
	 * queue opens fds for four purposes: smtp, mta, mda, and bounces.
	 * Therefore, double the fdlimit the second time to achieve a 4x
	 * increase relative to default.
	 */
	fdlimit(getdtablesize() * 2);
	if ((env->sc_maxconn = availdesc() / 4) < 1)
		fatalx("runner: fd starvation");

	config_pipes(env, peers, nitems(peers));
	config_peers(env, peers, nitems(peers));

	queue_purge(PATH_INCOMING);
	queue_purge(PATH_ENQUEUE);

	queue_setup_events(env);
	event_dispatch();
	queue_shutdown();

	return (0);
}

struct batch *
batch_by_id(struct smtpd *env, u_int64_t id)
{
	struct batch lookup;

	lookup.id = id;
	return SPLAY_FIND(batchtree, &env->batch_queue, &lookup);
}


void
queue_purge(char *queuepath)
{
	char		 path[MAXPATHLEN];
	struct qwalk	*q;

	q = qwalk_new(queuepath);

	while (qwalk(q, path))
		queue_delete_layout_message(queuepath, basename(path));

	qwalk_close(q);
}

void
queue_submit_envelope(struct smtpd *env, struct message *message)
{
	imsg_compose_event(env->sc_ievs[PROC_QUEUE],
	    IMSG_QUEUE_SUBMIT_ENVELOPE, 0, 0, -1,
	    message, sizeof(struct message));
}

void
queue_commit_envelopes(struct smtpd *env, struct message *message)
{
	imsg_compose_event(env->sc_ievs[PROC_QUEUE],
	    IMSG_QUEUE_COMMIT_ENVELOPES, 0, 0, -1,
	    message, sizeof(struct message));
}

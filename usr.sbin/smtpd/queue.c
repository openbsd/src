/*	$OpenBSD: queue.c,v 1.105 2011/08/29 18:49:29 chl Exp $	*/

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

#include <event.h>
#include <imsg.h>
#include <libgen.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

static void queue_imsg(struct imsgev *, struct imsg *);
static void queue_pass_to_runner(struct imsgev *, struct imsg *);
static void queue_shutdown(void);
static void queue_sig_handler(int, short, void *);
static void queue_purge(enum queue_kind, char *);

static void
queue_imsg(struct imsgev *iev, struct imsg *imsg)
{
	struct submit_status	 ss;
	struct envelope		*e;
	struct ramqueue_batch	*rq_batch;
	int			 fd, ret;

	if (iev->proc == PROC_SMTP) {
		e = imsg->data;

		switch (imsg->hdr.type) {
		case IMSG_QUEUE_CREATE_MESSAGE:
			ss.id = e->session_id;
			ss.code = 250;
			ss.u.msgid = 0;
			if (e->delivery.flags & DF_ENQUEUED)
				ret = queue_message_create(Q_ENQUEUE, &ss.u.msgid);
			else
				ret = queue_message_create(Q_INCOMING, &ss.u.msgid);
			if (ret == 0)
				ss.code = 421;
			imsg_compose_event(iev, IMSG_QUEUE_CREATE_MESSAGE, 0, 0, -1,
			    &ss, sizeof ss);
			return;

		case IMSG_QUEUE_REMOVE_MESSAGE:
			if (e->delivery.flags & DF_ENQUEUED)
				queue_message_purge(Q_ENQUEUE, evpid_to_msgid(e->delivery.id));
			else
				queue_message_purge(Q_INCOMING, evpid_to_msgid(e->delivery.id));
			return;

		case IMSG_QUEUE_COMMIT_MESSAGE:
			ss.id = e->session_id;
			if (e->delivery.flags & DF_ENQUEUED) {
				if (queue_message_commit(Q_ENQUEUE, evpid_to_msgid(e->delivery.id)))
					env->stats->queue.inserts_local++;
				else
					ss.code = 421;
			} else {
				if (queue_message_commit(Q_INCOMING, evpid_to_msgid(e->delivery.id)))
					env->stats->queue.inserts_remote++;
				else
					ss.code = 421;
			}
			imsg_compose_event(iev, IMSG_QUEUE_COMMIT_MESSAGE, 0, 0, -1,
			    &ss, sizeof ss);

			if (ss.code != 421)
				queue_pass_to_runner(iev, imsg);

			return;

		case IMSG_QUEUE_MESSAGE_FILE:
			ss.id = e->session_id;
			if (e->delivery.flags & DF_ENQUEUED)
				fd = queue_message_fd_rw(Q_ENQUEUE, evpid_to_msgid(e->delivery.id));
			else
				fd = queue_message_fd_rw(Q_INCOMING, evpid_to_msgid(e->delivery.id));
			if (fd == -1)
				ss.code = 421;
			imsg_compose_event(iev, IMSG_QUEUE_MESSAGE_FILE, 0, 0, fd,
			    &ss, sizeof ss);
			return;

		case IMSG_SMTP_ENQUEUE:
			queue_pass_to_runner(iev, imsg);
			return;
		}
	}

	if (iev->proc == PROC_LKA) {
		e = imsg->data;

		switch (imsg->hdr.type) {
		case IMSG_QUEUE_SUBMIT_ENVELOPE:
			ss.id = e->session_id;

			/* Write to disk */
			if (e->delivery.flags & DF_ENQUEUED)
				ret = queue_envelope_create(Q_ENQUEUE, e);
			else
				ret = queue_envelope_create(Q_INCOMING, e);

			if (ret == 0) {
				ss.code = 421;
				imsg_compose_event(env->sc_ievs[PROC_SMTP],
				    IMSG_QUEUE_TEMPFAIL, 0, 0, -1, &ss,
				    sizeof ss);
			}
			return;

		case IMSG_QUEUE_COMMIT_ENVELOPES:
			ss.id = e->session_id;
			ss.code = 250;
			imsg_compose_event(env->sc_ievs[PROC_SMTP],
			    IMSG_QUEUE_COMMIT_ENVELOPES, 0, 0, -1, &ss,
			    sizeof ss);
			return;
		}
	}

	if (iev->proc == PROC_RUNNER) {
		/* forward imsgs from runner on its behalf */
		imsg_compose_event(env->sc_ievs[imsg->hdr.peerid], imsg->hdr.type,
		    0, imsg->hdr.pid, imsg->fd, (char *)imsg->data,
		    imsg->hdr.len - sizeof imsg->hdr);
		return;
	}

	if (iev->proc == PROC_MTA) {
		switch (imsg->hdr.type) {
		case IMSG_QUEUE_MESSAGE_FD:
			rq_batch = imsg->data;
			fd = queue_message_fd_r(Q_QUEUE, rq_batch->msgid);
			imsg_compose_event(iev,  IMSG_QUEUE_MESSAGE_FD, 0, 0,
			    fd, rq_batch, sizeof *rq_batch);
			return;

		case IMSG_QUEUE_MESSAGE_UPDATE:
		case IMSG_BATCH_DONE:
			queue_pass_to_runner(iev, imsg);
			return;
		}
	}

	if (iev->proc == PROC_MDA) {
		switch (imsg->hdr.type) {
		case IMSG_QUEUE_MESSAGE_UPDATE:
		case IMSG_MDA_SESS_NEW:
			queue_pass_to_runner(iev, imsg);
			return;
		}
	}

	if (iev->proc == PROC_CONTROL) {
		switch (imsg->hdr.type) {
		case IMSG_QUEUE_PAUSE_LOCAL:
		case IMSG_QUEUE_PAUSE_OUTGOING:
		case IMSG_QUEUE_RESUME_LOCAL:
		case IMSG_QUEUE_RESUME_OUTGOING:
		case IMSG_QUEUE_SCHEDULE:
		case IMSG_QUEUE_REMOVE:
			queue_pass_to_runner(iev, imsg);
			return;
		}
	}

	if (iev->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {
		case IMSG_PARENT_ENQUEUE_OFFLINE:
			queue_pass_to_runner(iev, imsg);
			return;

		case IMSG_CTL_VERBOSE:
			log_verbose(*(int *)imsg->data);
			queue_pass_to_runner(iev, imsg);
			return;
		}
	}

	fatalx("queue_imsg: unexpected imsg");
}

static void
queue_pass_to_runner(struct imsgev *iev, struct imsg *imsg)
{
	imsg_compose_event(env->sc_ievs[PROC_RUNNER], imsg->hdr.type,
	    iev->proc, imsg->hdr.pid, imsg->fd, imsg->data,
	    imsg->hdr.len - sizeof imsg->hdr);
}

static void
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

static void
queue_shutdown(void)
{
	log_info("queue handler exiting");
	_exit(0);
}

pid_t
queue(void)
{
	pid_t		 pid;
	struct passwd	*pw;

	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_PARENT,	imsg_dispatch },
		{ PROC_CONTROL,	imsg_dispatch },
		{ PROC_SMTP,	imsg_dispatch },
		{ PROC_MDA,	imsg_dispatch },
		{ PROC_MTA,	imsg_dispatch },
		{ PROC_LKA,	imsg_dispatch },
		{ PROC_RUNNER,	imsg_dispatch }
	};

	switch (pid = fork()) {
	case -1:
		fatal("queue: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	purge_config(PURGE_EVERYTHING);

	pw = env->sc_pw;

	if (chroot(PATH_SPOOL) == -1)
		fatal("queue: chroot");
	if (chdir("/") == -1)
		fatal("queue: chdir(\"/\")");

	smtpd_process = PROC_QUEUE;
	setproctitle("%s", env->sc_title[smtpd_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("queue: cannot drop privileges");

	imsg_callback = queue_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, queue_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, queue_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/*
	 * queue opens fds for four purposes: smtp, mta, mda, and bounces.
	 * Therefore, use all available fd space and set the maxconn (=max
	 * session count for mta and mda) to a quarter of this value.
	 */
	fdlimit(1.0);
	if ((env->sc_maxconn = availdesc() / 4) < 1)
		fatalx("runner: fd starvation");

	config_pipes(peers, nitems(peers));
	config_peers(peers, nitems(peers));

	queue_purge(Q_INCOMING, PATH_INCOMING);
	queue_purge(Q_ENQUEUE, PATH_ENQUEUE);

	if (event_dispatch() <  0)
		fatal("event_dispatch");
	queue_shutdown();

	return (0);
}

static void
queue_purge(enum queue_kind qkind, char *queuepath)
{
	char		 path[MAXPATHLEN];
	struct qwalk	*q;

	q = qwalk_new(queuepath);

	while (qwalk(q, path)) {
		u_int32_t msgid;

		if ((msgid = filename_to_msgid(basename(path))) == 0) {
			log_warnx("queue_purge: invalid evpid");
			continue;
		}
		queue_message_purge(qkind, msgid);
	}

	qwalk_close(q);
}

void
queue_submit_envelope(struct envelope *ep)
{
	imsg_compose_event(env->sc_ievs[PROC_QUEUE],
	    IMSG_QUEUE_SUBMIT_ENVELOPE, 0, 0, -1,
	    ep, sizeof(*ep));
}

void
queue_commit_envelopes(struct envelope *ep)
{
	imsg_compose_event(env->sc_ievs[PROC_QUEUE],
	    IMSG_QUEUE_COMMIT_ENVELOPES, 0, 0, -1,
	    ep, sizeof(*ep));
}

/*	$OpenBSD: queue.c,v 1.124 2012/08/09 16:00:31 eric Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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

#include <err.h>
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
static void queue_timeout(int, short, void *);
static void queue_bounce(struct envelope *);
static void queue_pass_to_scheduler(struct imsgev *, struct imsg *);
static void queue_shutdown(void);
static void queue_sig_handler(int, short, void *);

static void
queue_imsg(struct imsgev *iev, struct imsg *imsg)
{
	static struct mta_batch	 batch, *mta_batch;
	struct submit_status	 ss;
	struct envelope		*e, evp;
	int			 fd, ret;
	uint64_t		 id;
	uint32_t		 msgid;

	log_imsg(PROC_QUEUE, iev->proc, imsg);

	if (iev->proc == PROC_SMTP) {
		e = imsg->data;

		switch (imsg->hdr.type) {
		case IMSG_QUEUE_CREATE_MESSAGE:
			ss.id = e->session_id;
			ss.code = 250;
			ss.u.msgid = 0;
			ret = queue_message_create(&ss.u.msgid);
			if (ret == 0)
				ss.code = 421;
			imsg_compose_event(iev, IMSG_QUEUE_CREATE_MESSAGE, 0, 0,
			    -1, &ss, sizeof ss);
			return;

		case IMSG_QUEUE_REMOVE_MESSAGE:
			queue_message_incoming_delete(evpid_to_msgid(e->id));
			return;

		case IMSG_QUEUE_COMMIT_MESSAGE:
			ss.id = e->session_id;
			ss.code = 250;
			msgid = evpid_to_msgid(e->id);
			if (queue_message_commit(msgid)) {
				stat_increment(e->flags & DF_ENQUEUED ?
				    STATS_QUEUE_LOCAL : STATS_QUEUE_REMOTE);
				imsg_compose_event(env->sc_ievs[PROC_SCHEDULER],
				    IMSG_QUEUE_COMMIT_MESSAGE, 0, 0, -1,
				    &msgid, sizeof msgid);
			} else
				ss.code = 421;

			imsg_compose_event(iev, IMSG_QUEUE_COMMIT_MESSAGE, 0, 0,
			    -1, &ss, sizeof ss);
			return;

		case IMSG_QUEUE_MESSAGE_FILE:
			ss.id = e->session_id;
			fd = queue_message_fd_rw(evpid_to_msgid(e->id));
			if (fd == -1)
				ss.code = 421;
			imsg_compose_event(iev, IMSG_QUEUE_MESSAGE_FILE, 0, 0,
			    fd, &ss, sizeof ss);
			return;

		case IMSG_SMTP_ENQUEUE:
			id = *(uint64_t*)(imsg->data);
			bounce_run(id, imsg->fd);
			return;
		}
	}

	if (iev->proc == PROC_LKA) {
		e = imsg->data;
		switch (imsg->hdr.type) {
		case IMSG_QUEUE_SUBMIT_ENVELOPE:
			if (!queue_envelope_create(e)) {
				ss.id = e->session_id;
				ss.code = 421;
				imsg_compose_event(env->sc_ievs[PROC_SMTP],
				    IMSG_QUEUE_TEMPFAIL, 0, 0, -1, &ss,
				    sizeof ss);
			} else {
				/* tell the scheduler */
				imsg_compose_event(env->sc_ievs[PROC_SCHEDULER],
				    IMSG_QUEUE_SUBMIT_ENVELOPE, 0, 0, -1, e,
				    sizeof *e);
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

	if (iev->proc == PROC_SCHEDULER) {
		switch (imsg->hdr.type) {
		case IMSG_QUEUE_REMOVE:
			evp.id = *(uint64_t*)(imsg->data);
			queue_envelope_delete(&evp);
			return;

		case IMSG_QUEUE_EXPIRE:
			id = *(uint64_t*)(imsg->data);
			queue_envelope_load(id, &evp);
			envelope_set_errormsg(&evp, "envelope expired");
			queue_bounce(&evp);
			queue_envelope_delete(&evp);
			return;

		case IMSG_MDA_SESS_NEW:
			id = *(uint64_t*)(imsg->data);
			queue_envelope_load(id, &evp);
			evp.lasttry = time(NULL);
			fd = queue_message_fd_r(evpid_to_msgid(id));
			imsg_compose_event(env->sc_ievs[PROC_MDA],
			    IMSG_MDA_SESS_NEW, 0, 0, fd, &evp, sizeof evp);
			return;

		case IMSG_SMTP_ENQUEUE:
			id = *(uint64_t*)(imsg->data);
			bounce_add(id);
			return;

		case IMSG_BATCH_CREATE:
			bzero(&batch, sizeof batch);
			return;

		case IMSG_BATCH_APPEND:
			id = *(uint64_t*)(imsg->data);
			queue_envelope_load(id, &evp);
			if (!batch.id) {   
				batch.id = generate_uid();
				batch.relay = evp.agent.mta.relay;
				imsg_compose_event(env->sc_ievs[PROC_MTA],
				    IMSG_BATCH_CREATE, 0, 0, -1,
				    &batch, sizeof batch);
			}
			evp.lasttry = time(NULL);
			evp.batch_id = batch.id;
			imsg_compose_event(env->sc_ievs[PROC_MTA],
			    IMSG_BATCH_APPEND, 0, 0, -1, &evp, sizeof evp);
			return;

		case IMSG_BATCH_CLOSE:
			imsg_compose_event(env->sc_ievs[PROC_MTA],
			    IMSG_BATCH_CLOSE, 0, 0, -1,
			    &batch.id, sizeof batch.id);
			return;
 		}
	}

	if (iev->proc == PROC_MTA || iev->proc == PROC_MDA) {
		switch (imsg->hdr.type) {
		case IMSG_QUEUE_MESSAGE_FD:
			mta_batch = imsg->data;
			fd = queue_message_fd_r(mta_batch->msgid);
			imsg_compose_event(iev,  IMSG_QUEUE_MESSAGE_FD, 0, 0,
			    fd, mta_batch, sizeof *mta_batch);
			return;

		case IMSG_QUEUE_DELIVERY_OK:
			e = imsg->data;
			queue_envelope_delete(e);
			imsg_compose_event(env->sc_ievs[PROC_SCHEDULER],
			    IMSG_QUEUE_DELIVERY_OK, 0, 0, -1, &e->id,
			    sizeof e->id);
			return;

		case IMSG_QUEUE_DELIVERY_TEMPFAIL:
			e = imsg->data;
			e->retry++;
			queue_envelope_update(e);
			imsg_compose_event(env->sc_ievs[PROC_SCHEDULER],
			    IMSG_QUEUE_DELIVERY_TEMPFAIL, 0, 0, -1, e,
			    sizeof *e);
			return;

		case IMSG_QUEUE_DELIVERY_PERMFAIL:
			e = imsg->data;
			queue_bounce(e);
			queue_envelope_delete(e);
			imsg_compose_event(env->sc_ievs[PROC_SCHEDULER],
			    IMSG_QUEUE_DELIVERY_PERMFAIL, 0, 0, -1, &e->id,
			    sizeof e->id);
			return;
		}
	}

	if (iev->proc == PROC_CONTROL) {
		switch (imsg->hdr.type) {
		case IMSG_QUEUE_PAUSE_MDA:
		case IMSG_QUEUE_PAUSE_MTA:
		case IMSG_QUEUE_RESUME_MDA:
		case IMSG_QUEUE_RESUME_MTA:
		case IMSG_QUEUE_REMOVE:
			queue_pass_to_scheduler(iev, imsg);
			return;
		}
	}

	if (iev->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {
		case IMSG_CTL_VERBOSE:
			log_verbose(*(int *)imsg->data);
			queue_pass_to_scheduler(iev, imsg);
			return;
		}
	}

	errx(1, "queue_imsg: unexpected %s imsg", imsg_to_str(imsg->hdr.type));
}

static void
queue_pass_to_scheduler(struct imsgev *iev, struct imsg *imsg)
{
	imsg_compose_event(env->sc_ievs[PROC_SCHEDULER], imsg->hdr.type,
	    iev->proc, imsg->hdr.pid, imsg->fd, imsg->data,
	    imsg->hdr.len - sizeof imsg->hdr);
}

static void
queue_bounce(struct envelope *e)
{
	struct envelope	b;
	uint32_t	msgid;

	b = *e;
	b.type = D_BOUNCE;
	b.retry = 0;
	b.lasttry = 0;
	b.creation = time(NULL);
	b.expire = 3600 * 24 * 7;

	if (e->type == D_BOUNCE) {
		log_warnx("queue: double bounce!");
	} else if (e->sender.user[0] == '\0') {
		log_warnx("queue: no return path!");
	} else if (!queue_envelope_create(&b)) {
		log_warnx("queue: cannot bounce!");
	} else {
		log_debug("queue: bouncing evp:%016" PRIx64
		    " as evp:%016" PRIx64, e->id, b.id);
		imsg_compose_event(env->sc_ievs[PROC_SCHEDULER],
		    IMSG_QUEUE_SUBMIT_ENVELOPE, 0, 0, -1, &b, sizeof b);
		msgid = evpid_to_msgid(b.id);
		imsg_compose_event(env->sc_ievs[PROC_SCHEDULER],
		    IMSG_QUEUE_COMMIT_MESSAGE, 0, 0, -1, &msgid, sizeof msgid);
	}
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
	struct timeval	 tv;
	struct event	 ev_qload;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_PARENT,		imsg_dispatch },
		{ PROC_CONTROL,		imsg_dispatch },
		{ PROC_SMTP,		imsg_dispatch },
		{ PROC_MDA,		imsg_dispatch },
		{ PROC_MTA,		imsg_dispatch },
		{ PROC_LKA,		imsg_dispatch },
		{ PROC_SCHEDULER,	imsg_dispatch }
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
		fatalx("queue: fd starvation");

	config_pipes(peers, nitems(peers));
	config_peers(peers, nitems(peers));

	/* setup queue loading task */
	evtimer_set(&ev_qload, queue_timeout, &ev_qload);
	tv.tv_sec = 0;
	tv.tv_usec = 10;
	evtimer_add(&ev_qload, &tv);

	if (event_dispatch() <  0)
		fatal("event_dispatch");
	queue_shutdown();

	return (0);
}

static void
queue_timeout(int fd, short event, void *p)
{
	static struct qwalk	*q = NULL;
	static uint32_t		 last_msgid = 0;
	struct event		*ev = p;
	struct envelope		 envelope;
	struct timeval		 tv;
	uint64_t		 evpid;

	if (q == NULL) {
		log_info("queue: loading queue into scheduler");
		q = qwalk_new(0);
	}

	while (qwalk(q, &evpid)) {
		if (queue_envelope_load(evpid, &envelope))
			imsg_compose_event(env->sc_ievs[PROC_SCHEDULER],
			    IMSG_QUEUE_SUBMIT_ENVELOPE, 0, 0, -1, &envelope,
			    sizeof envelope);

		if (last_msgid && evpid_to_msgid(evpid) != last_msgid)
			imsg_compose_event(env->sc_ievs[PROC_SCHEDULER],
			    IMSG_QUEUE_COMMIT_MESSAGE, 0, 0, -1, &last_msgid,
			    sizeof last_msgid);

		last_msgid = evpid_to_msgid(evpid);
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		evtimer_add(ev, &tv);	
		return;
	}

	if (last_msgid) {
		imsg_compose_event(env->sc_ievs[PROC_SCHEDULER],
		    IMSG_QUEUE_COMMIT_MESSAGE, 0, 0, -1, &last_msgid,
		    sizeof last_msgid);
	}

	log_debug("queue: done loading queue into scheduler");
	qwalk_close(q);
}

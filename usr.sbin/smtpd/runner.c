/*	$OpenBSD: runner.c,v 1.79 2010/04/19 08:14:07 jacekm Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008-2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
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
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <libgen.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"

__dead void	runner_shutdown(void);
void		runner_sig_handler(int, short, void *);
void		runner_dispatch_parent(int, short, void *);
void	        runner_dispatch_control(int, short, void *);
void	        runner_dispatch_queue(int, short, void *);
void	        runner_dispatch_mda(int, short, void *);
void		runner_dispatch_mta(int, short, void *);
void		runner_dispatch_lka(int, short, void *);
void		runner_dispatch_smtp(int, short, void *);
void		runner_setup_events(struct smtpd *);
void		runner_disable_events(struct smtpd *);

void		runner_reset_flags(void);
void		runner_process_offline(struct smtpd *);

void		runner_timeout(int, short, void *);

void		runner_process_queue(struct smtpd *);
void		runner_process_runqueue(struct smtpd *);
void		runner_process_batchqueue(struct smtpd *);

int		runner_message_schedule(struct message *, time_t);

void		runner_purge_run(void);
void		runner_purge_message(char *);

int		runner_check_loop(struct message *);

struct batch	*batch_record(struct smtpd *, struct message *);
struct batch	*batch_lookup(struct smtpd *, struct message *);

int		runner_force_envelope_schedule(char *);
int		runner_force_message_schedule(char *);

int		runner_force_envelope_remove(char *);
int		runner_force_message_remove(char *);

void
runner_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		runner_shutdown();
		break;
	default:
		fatalx("runner_sig_handler: unexpected signal");
	}
}

void
runner_dispatch_parent(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_PARENT];
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
			fatal("runner_dispatch_parent: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_PARENT_ENQUEUE_OFFLINE:
			runner_process_offline(env);
			break;
		case IMSG_CTL_VERBOSE: {
			int verbose;

			IMSG_SIZE_CHECK(&verbose);

			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_verbose(verbose);
			break;
		}
		default:
			log_warnx("runner_dispatch_parent: got imsg %d",
			    imsg.hdr.type);
			fatalx("runner_dispatch_parent: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
runner_dispatch_control(int sig, short event, void *p)
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
			fatal("runner_dispatch_control: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_MDA_PAUSE:
			env->sc_opts |= SMTPD_MDA_PAUSED;
			break;
		case IMSG_MTA_PAUSE:
			env->sc_opts |= SMTPD_MTA_PAUSED;
			break;
		case IMSG_MDA_RESUME:
			env->sc_opts &= ~SMTPD_MDA_PAUSED;
			break;
		case IMSG_MTA_RESUME:
			env->sc_opts &= ~SMTPD_MTA_PAUSED;
			break;
		case IMSG_RUNNER_SCHEDULE: {
			struct sched *s = imsg.data;

			IMSG_SIZE_CHECK(s);

			s->ret = 0;
			if (valid_message_uid(s->mid))
				s->ret = runner_force_envelope_schedule(s->mid);
			else if (valid_message_id(s->mid))
				s->ret = runner_force_message_schedule(s->mid);

			imsg_compose_event(iev, IMSG_RUNNER_SCHEDULE, 0, 0, -1, s, sizeof(*s));
			break;
		}
		case IMSG_RUNNER_REMOVE: {
			struct remove *s = imsg.data;

			IMSG_SIZE_CHECK(s);

			s->ret = 0;
			if (valid_message_uid(s->mid))
				s->ret = runner_force_envelope_remove(s->mid);
			else if (valid_message_id(s->mid))
				s->ret = runner_force_message_remove(s->mid);

			imsg_compose_event(iev, IMSG_RUNNER_REMOVE, 0, 0, -1, s, sizeof(*s));
			break;
		}
		default:
			log_warnx("runner_dispatch_control: got imsg %d",
			    imsg.hdr.type);
			fatalx("runner_dispatch_control: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
runner_dispatch_queue(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_QUEUE];
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
			fatal("runner_dispatch_queue: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_RUNNER_UPDATE_ENVELOPE: {
			struct message	*m = imsg.data;

			IMSG_SIZE_CHECK(m);

			env->stats->runner.active--;
			queue_message_update(m);
			break;
		}
		default:
			log_warnx("runner_dispatch_queue: got imsg %d",
			    imsg.hdr.type);
			fatalx("runner_dispatch_queue: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
runner_dispatch_mda(int sig, short event, void *p)
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
			fatal("runner_dispatch_mda: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_MDA_SESS_NEW:
			env->stats->mda.sessions_active--;
			break;

		default:
			log_warnx("runner_dispatch_mda: got imsg %d",
			    imsg.hdr.type);
			fatalx("runner_dispatch_mda: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
runner_dispatch_mta(int sig, short event, void *p)
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
			fatal("runner_dispatch_mta: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_BATCH_DONE:
			env->stats->mta.sessions_active--;
			break;

		default:
			log_warnx("runner_dispatch_mta: got imsg %d",
			    imsg.hdr.type);
			fatalx("runner_dispatch_mta: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
runner_dispatch_lka(int sig, short event, void *p)
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
			fatal("runner_dispatch_lka: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_warnx("runner_dispatch_lka: got imsg %d",
			    imsg.hdr.type);
			fatalx("runner_dispatch_lka: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
runner_dispatch_smtp(int sig, short event, void *p)
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
			fatal("runner_dispatch_smtp: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_SMTP_ENQUEUE: {
			struct message	*m = imsg.data;

			IMSG_SIZE_CHECK(m);

			if (imsg.fd < 0 || ! bounce_session(env, imsg.fd, m)) {
				m->status = S_MESSAGE_TEMPFAILURE;
				queue_message_update(m);
			}
			break;
		}

		default:
			log_warnx("runner_dispatch_smtp: got imsg %d",
			    imsg.hdr.type);
			fatalx("runner_dispatch_smtp: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
runner_shutdown(void)
{
	log_info("runner handler exiting");
	_exit(0);
}

void
runner_setup_events(struct smtpd *env)
{
	struct timeval	 tv;

	evtimer_set(&env->sc_ev, runner_timeout, env);
	tv.tv_sec = 0;
	tv.tv_usec = 10;
	evtimer_add(&env->sc_ev, &tv);
}

void
runner_disable_events(struct smtpd *env)
{
	evtimer_del(&env->sc_ev);
}

pid_t
runner(struct smtpd *env)
{
	pid_t		 pid;
	struct passwd	*pw;

	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_PARENT,	runner_dispatch_parent },
		{ PROC_CONTROL,	runner_dispatch_control },
		{ PROC_MDA,	runner_dispatch_mda },
		{ PROC_MTA,	runner_dispatch_mta },
		{ PROC_QUEUE,	runner_dispatch_queue },
		{ PROC_LKA,	runner_dispatch_lka },
		{ PROC_SMTP,	runner_dispatch_smtp }
	};

	switch (pid = fork()) {
	case -1:
		fatal("runner: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;

#ifndef DEBUG
	if (chroot(PATH_SPOOL) == -1)
		fatal("runner: chroot");
	if (chdir("/") == -1)
		fatal("runner: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
#endif

	smtpd_process = PROC_RUNNER;
	setproctitle("%s", env->sc_title[smtpd_process]);

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("runner: cannot drop privileges");
#endif

	SPLAY_INIT(&env->batch_queue);

	event_init();

	signal_set(&ev_sigint, SIGINT, runner_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, runner_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* see fdlimit()-related comment in queue.c */
	fdlimit(1.0);
	if ((env->sc_maxconn = availdesc() / 4) < 1)
		fatalx("runner: fd starvation");

	config_pipes(env, peers, nitems(peers));
	config_peers(env, peers, nitems(peers));

	unlink(PATH_QUEUE "/envelope.tmp");
	runner_reset_flags();
	runner_process_offline(env);

	runner_setup_events(env);
	event_dispatch();
	runner_shutdown();

	return (0);
}

void
runner_process_offline(struct smtpd *env)
{
	char		 path[MAXPATHLEN];
	struct qwalk	*q;

	q = qwalk_new(PATH_OFFLINE);

	if (qwalk(q, path))
		imsg_compose_event(env->sc_ievs[PROC_PARENT],
		    IMSG_PARENT_ENQUEUE_OFFLINE, 0, 0, -1, path,
		    strlen(path) + 1);

	qwalk_close(q);
}

void
runner_reset_flags(void)
{
	char		 path[MAXPATHLEN];
	struct message	 message;
	struct qwalk	*q;

	q = qwalk_new(PATH_QUEUE);

	while (qwalk(q, path)) {
		while (! queue_load_envelope(&message, basename(path)))
			sleep(1);
		message_reset_flags(&message);
	}

	qwalk_close(q);
}

void
runner_timeout(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct timeval		 tv;

	runner_purge_run();

	runner_process_queue(env);
	runner_process_runqueue(env);
	runner_process_batchqueue(env);

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	evtimer_add(&env->sc_ev, &tv);
}

void
runner_process_queue(struct smtpd *env)
{
	char		 path[MAXPATHLEN];
	char		 rqpath[MAXPATHLEN];
	struct message	 message;
	time_t		 now;
	size_t		 mta_av, mda_av, bnc_av;
	struct qwalk	*q;

	mta_av = env->sc_maxconn - env->stats->mta.sessions_active;
	mda_av = env->sc_maxconn - env->stats->mda.sessions_active;
	bnc_av = env->sc_maxconn - env->stats->runner.bounces_active;

	now = time(NULL);
	q = qwalk_new(PATH_QUEUE);

	while (qwalk(q, path)) {
		if (! queue_load_envelope(&message, basename(path)))
			continue;

		if (message.type & T_MDA_MESSAGE) {
			if (env->sc_opts & SMTPD_MDA_PAUSED)
				continue;
			if (mda_av == 0)
				continue;
		}

		if (message.type & T_MTA_MESSAGE) {
			if (env->sc_opts & SMTPD_MTA_PAUSED)
				continue;
			if (mta_av == 0)
				continue;
		}

		if (message.type & T_BOUNCE_MESSAGE) {
			if (env->sc_opts & (SMTPD_MDA_PAUSED|SMTPD_MTA_PAUSED))
				continue;
			if (bnc_av == 0)
				continue;
		}

		if (! runner_message_schedule(&message, now))
			continue;

		if (runner_check_loop(&message)) {
			message_set_errormsg(&message, "loop has been detected");
			bounce_record_message(&message);
			queue_remove_envelope(&message);
			continue;
		}

		message.flags |= F_MESSAGE_SCHEDULED;
		message.flags &= ~F_MESSAGE_FORCESCHEDULE;
		queue_update_envelope(&message);

		if (! bsnprintf(rqpath, sizeof(rqpath), "%s/%s", PATH_RUNQUEUE,
			basename(path)))
			fatalx("runner_process_queue: snprintf");

		if (symlink(path, rqpath) == -1) {
			if (errno == EEXIST)
				continue;
			if (errno == ENOSPC)
				break;
			fatal("runner_process_queue: symlink");
		}

		if (message.type & T_MDA_MESSAGE)
			mda_av--;
		if (message.type & T_MTA_MESSAGE)
			mta_av--;
		if (message.type & T_BOUNCE_MESSAGE)
			bnc_av--;
	}
	
	qwalk_close(q);
}

void
runner_process_runqueue(struct smtpd *env)
{
	char		 path[MAXPATHLEN];
	struct message	 message;
	time_t		 tm;
	struct batch	*batchp;
	struct message	*messagep;
	struct qwalk	*q;

	tm = time(NULL);

	q = qwalk_new(PATH_RUNQUEUE);

	while (qwalk(q, path)) {
		unlink(path);

		if (! queue_load_envelope(&message, basename(path)))
			continue;

		if (message.flags & F_MESSAGE_PROCESSING)
			continue;

		message.lasttry = tm;
		message.flags &= ~F_MESSAGE_SCHEDULED;
		message.flags |= F_MESSAGE_PROCESSING;

		if (! queue_update_envelope(&message))
			continue;

		messagep = calloc(1, sizeof (struct message));
		if (messagep == NULL)
			fatal("runner_process_runqueue: calloc");
		*messagep = message;

		messagep->batch_id = 0;
		batchp = batch_lookup(env, messagep);
		if (batchp != NULL)
			messagep->batch_id = batchp->id;

		batchp = batch_record(env, messagep);
		if (messagep->batch_id == 0)
			messagep->batch_id = batchp->id;
	}

	qwalk_close(q);
}

void
runner_process_batchqueue(struct smtpd *env)
{
	struct batch	*batchp;
	struct message	*m;
	int		 fd;

	while ((batchp = SPLAY_MIN(batchtree, &env->batch_queue)) != NULL) {
		switch (batchp->type) {
		case T_BOUNCE_BATCH:
			while ((m = TAILQ_FIRST(&batchp->messages))) {
				bounce_process(env, m);
				TAILQ_REMOVE(&batchp->messages, m, entry);
				free(m);
			}
			env->stats->runner.bounces_active++;
			env->stats->runner.bounces++;
			break;

		case T_MDA_BATCH:
			m = TAILQ_FIRST(&batchp->messages);
			fd = queue_open_message_file(m->message_id);
			imsg_compose_event(env->sc_ievs[PROC_MDA],
			    IMSG_MDA_SESS_NEW, 0, 0, fd, m, sizeof *m);
			TAILQ_REMOVE(&batchp->messages, m, entry);
			free(m);
			env->stats->mda.sessions_active++;
			env->stats->mda.sessions++;
			break;

		case T_MTA_BATCH:
			imsg_compose_event(env->sc_ievs[PROC_MTA],
			    IMSG_BATCH_CREATE, 0, 0, -1, batchp,
			    sizeof *batchp);
			while ((m = TAILQ_FIRST(&batchp->messages))) {
				imsg_compose_event(env->sc_ievs[PROC_MTA],
				    IMSG_BATCH_APPEND, 0, 0, -1, m, sizeof *m);
				TAILQ_REMOVE(&batchp->messages, m, entry);
				free(m);
			}
			imsg_compose_event(env->sc_ievs[PROC_MTA],
			    IMSG_BATCH_CLOSE, 0, 0, -1, batchp, sizeof *batchp);
			env->stats->mta.sessions_active++;
			env->stats->mta.sessions++;
			break;

		default:
			fatalx("runner_process_batchqueue: unknown type");
		}

		SPLAY_REMOVE(batchtree, &env->batch_queue, batchp);
		free(batchp);
	}
}

int
runner_message_schedule(struct message *messagep, time_t tm)
{
	time_t delay;

	if (messagep->flags & (F_MESSAGE_SCHEDULED|F_MESSAGE_PROCESSING))
		return 0;

	if (messagep->flags & F_MESSAGE_FORCESCHEDULE)
		return 1;

	/* Batch has been in the queue for too long and expired */
	if (tm - messagep->creation >= SMTPD_QUEUE_EXPIRY) {
		message_set_errormsg(messagep, "message expired after sitting in queue for %d days",
			SMTPD_QUEUE_EXPIRY / 60 / 60 / 24);
		bounce_record_message(messagep);
		queue_remove_envelope(messagep);
		return 0;
	}

	if (messagep->lasttry == 0)
		return 1;

	delay = SMTPD_QUEUE_MAXINTERVAL;

	// recompute path

	if (messagep->type == T_MDA_MESSAGE ||
	    messagep->type == T_BOUNCE_MESSAGE) {
		if (messagep->retry < 5)
			return 1;
			
		if (messagep->retry < 15)
			delay = (messagep->retry * 60) + arc4random_uniform(60);
	}

	if (messagep->type == T_MTA_MESSAGE) {
		if (messagep->retry < 3)
			delay = SMTPD_QUEUE_INTERVAL;
		else if (messagep->retry <= 7) {
			delay = SMTPD_QUEUE_INTERVAL * (1 << (messagep->retry - 3));
			if (delay > SMTPD_QUEUE_MAXINTERVAL)
				delay = SMTPD_QUEUE_MAXINTERVAL;
		}
	}

	if (tm >= messagep->lasttry + delay)
		return 1;

	return 0;
}

int
runner_force_envelope_schedule(char *mid)
{
	struct message message;

	if (! queue_load_envelope(&message, mid))
		return 0;

	if (! message.flags & (F_MESSAGE_PROCESSING|F_MESSAGE_SCHEDULED))
		return 1;

	message.flags |= F_MESSAGE_FORCESCHEDULE;

	if (! queue_update_envelope(&message))
		return 0;

	return 1;
}

int
runner_force_message_schedule(char *mid)
{
	char path[MAXPATHLEN];
	DIR *dirp;
	struct dirent *dp;

	if (! bsnprintf(path, MAXPATHLEN, "%s/%d/%s/envelopes",
		PATH_QUEUE, queue_hash(mid), mid))
		return 0;

	dirp = opendir(path);
	if (dirp == NULL)
		return 0;

	while ((dp = readdir(dirp)) != NULL) {
		if (valid_message_uid(dp->d_name))
			runner_force_envelope_schedule(dp->d_name);
	}
	closedir(dirp);

	return 1;
}


int
runner_force_envelope_remove(char *mid)
{
	struct message message;

	if (! queue_load_envelope(&message, mid))
		return 0;

	if (! message.flags & (F_MESSAGE_PROCESSING|F_MESSAGE_SCHEDULED))
		return 0;

	if (! queue_remove_envelope(&message))
		return 0;

	return 1;
}

int
runner_force_message_remove(char *mid)
{
	char path[MAXPATHLEN];
	DIR *dirp;
	struct dirent *dp;

	if (! bsnprintf(path, MAXPATHLEN, "%s/%d/%s/envelopes",
		PATH_QUEUE, queue_hash(mid), mid))
		return 0;

	dirp = opendir(path);
	if (dirp == NULL)
		return 0;

	while ((dp = readdir(dirp)) != NULL) {
		if (valid_message_uid(dp->d_name))
			runner_force_envelope_remove(dp->d_name);
	}
	closedir(dirp);

	return 1;
}

void
runner_purge_run(void)
{
	char		 path[MAXPATHLEN];
	struct qwalk	*q;

	q = qwalk_new(PATH_PURGE);

	while (qwalk(q, path))
		runner_purge_message(basename(path));

	qwalk_close(q);
}

void
runner_purge_message(char *msgid)
{
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];
	char evppath[MAXPATHLEN];
	char msgpath[MAXPATHLEN];
	DIR *dirp;
	struct dirent *dp;
	
	if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%s", PATH_PURGE, msgid))
		fatal("runner_purge_message: snprintf");

	if (! bsnprintf(evpdir, sizeof(evpdir), "%s%s", rootdir,
		PATH_ENVELOPES))
		fatal("runner_purge_message: snprintf");
	
	if (! bsnprintf(msgpath, sizeof(msgpath), "%s/message", rootdir))
		fatal("runner_purge_message: snprintf");

	if (unlink(msgpath) == -1)
		if (errno != ENOENT)
			fatal("runner_purge_message: unlink");

	dirp = opendir(evpdir);
	if (dirp == NULL) {
		if (errno == ENOENT)
			goto delroot;
		fatal("runner_purge_message: opendir");
	}
	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;
		if (! bsnprintf(evppath, sizeof(evppath), "%s/%s", evpdir,
			dp->d_name))
			fatal("runner_purge_message: snprintf");

		if (unlink(evppath) == -1)
			if (errno != ENOENT)
				fatal("runner_purge_message: unlink");
	}
	closedir(dirp);

	if (rmdir(evpdir) == -1)
		if (errno != ENOENT)
			fatal("runner_purge_message: rmdir");

delroot:
	if (rmdir(rootdir) == -1)
		if (errno != ENOENT)
			fatal("runner_purge_message: rmdir");
}

struct batch *
batch_record(struct smtpd *env, struct message *messagep)
{
	struct batch *batchp;
	struct path *path;

	batchp = NULL;
	if (messagep->batch_id != 0) {
		batchp = batch_by_id(env, messagep->batch_id);
		if (batchp == NULL)
			fatalx("batch_record: internal inconsistency.");
	}
	if (batchp == NULL) {
		batchp = calloc(1, sizeof(struct batch));
		if (batchp == NULL)
			fatal("batch_record: calloc");

		batchp->id = generate_uid();

		(void)strlcpy(batchp->message_id, messagep->message_id,
		    sizeof(batchp->message_id));
		TAILQ_INIT(&batchp->messages);
		SPLAY_INSERT(batchtree, &env->batch_queue, batchp);

		if (messagep->type & T_BOUNCE_MESSAGE) {
			batchp->type = T_BOUNCE_BATCH;
			path = &messagep->sender;
		}
		else {
			path = &messagep->recipient;
		}
		batchp->rule = path->rule;

		(void)strlcpy(batchp->hostname, path->domain,
		    sizeof(batchp->hostname));

		if (batchp->type != T_BOUNCE_BATCH) {
			if (IS_MAILBOX(*path) || IS_EXT(*path)) {
				batchp->type = T_MDA_BATCH;
			}
			else {
				batchp->type = T_MTA_BATCH;
			}
		}
	}

	TAILQ_INSERT_TAIL(&batchp->messages, messagep, entry);
	env->stats->runner.active++;
	return batchp;
}

struct batch *
batch_lookup(struct smtpd *env, struct message *message)
{
	struct batch *batchp;
	struct batch lookup;

	/* We only support delivery of one message at a time, in MDA
	 * and bounces messages.
	 */
	if (message->type == T_BOUNCE_MESSAGE || message->type == T_MDA_MESSAGE)
		return NULL;

	/* If message->batch_id != 0, we can retrieve batch by id */
	if (message->batch_id != 0) {
		lookup.id = message->batch_id;
		return SPLAY_FIND(batchtree, &env->batch_queue, &lookup);
	}

	/* We do not know the batch_id yet, maybe it was created but we could not
	 * be notified, or it just does not exist. Let's scan to see if we can do
	 * a match based on our message_id and flags.
	 */
	SPLAY_FOREACH(batchp, batchtree, &env->batch_queue) {

		if (batchp->type != message->type)
			continue;

		if (strcasecmp(batchp->message_id, message->message_id) != 0)
			continue;

		if (batchp->type & T_MTA_BATCH)
			if (strcasecmp(batchp->hostname, message->recipient.domain) != 0)
				continue;

		break;
	}

	return batchp;
}

int
batch_cmp(struct batch *s1, struct batch *s2)
{
	/*
	 * do not return u_int64_t's
	 */
	if (s1->id < s2->id)
		return (-1);

	if (s1->id > s2->id)
		return (1);

	return (0);
}

int
runner_check_loop(struct message *messagep)
{
	int fd;
	FILE *fp;
	char *buf, *lbuf;
	size_t len;
	struct path chkpath;
	int ret = 0;
	int rcvcount = 0;

	fd = queue_open_message_file(messagep->message_id);
	if ((fp = fdopen(fd, "r")) == NULL)
		fatal("fdopen");

	lbuf = NULL;
	while ((buf = fgetln(fp, &len))) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			/* EOF without EOL, copy and add the NUL */
			if ((lbuf = malloc(len + 1)) == NULL)
				err(1, NULL);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		if (strchr(buf, ':') == NULL && !isspace((int)*buf))
			break;

		if (strncasecmp("Received: ", buf, 10) == 0) {
			rcvcount++;
			if (rcvcount == MAX_HOPS_COUNT) {
				ret = 1;
				break;
			}
		}

		else if (strncasecmp("Delivered-To: ", buf, 14) == 0) {
			struct path rcpt;

			bzero(&chkpath, sizeof (struct path));
			if (! recipient_to_path(&chkpath, buf + 14))
				continue;

			rcpt = messagep->recipient;
			if (messagep->type == T_BOUNCE_MESSAGE)
				rcpt = messagep->sender;

			if (strcasecmp(chkpath.user, rcpt.user) == 0 &&
			    strcasecmp(chkpath.domain, rcpt.domain) == 0) {
				ret = 1;
				break;
			}
		}
	}
	free(lbuf);

	fclose(fp);
	return ret;
}

void
message_reset_flags(struct message *m)
{
	m->flags &= ~F_MESSAGE_SCHEDULED;
	m->flags &= ~F_MESSAGE_PROCESSING;

	while (! queue_update_envelope(m))
		sleep(1);
}

SPLAY_GENERATE(batchtree, batch, b_nodes, batch_cmp);

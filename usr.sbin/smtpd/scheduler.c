/*	$OpenBSD: scheduler.c,v 1.8 2012/08/08 08:50:42 eric Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008-2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
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

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <inttypes.h>
#include <libgen.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

static void scheduler_imsg(struct imsgev *, struct imsg *);
static void scheduler_shutdown(void);
static void scheduler_sig_handler(int, short, void *);
static void scheduler_reset_events(void);
static void scheduler_timeout(int, short, void *);
static void scheduler_process_remove(struct scheduler_batch *);
static void scheduler_process_expire(struct scheduler_batch *);
static void scheduler_process_bounce(struct scheduler_batch *);
static void scheduler_process_mda(struct scheduler_batch *);
static void scheduler_process_mta(struct scheduler_batch *);
static int scheduler_load_message(u_int32_t);

void scheduler_envelope_update(struct envelope *);
void scheduler_envelope_delete(struct envelope *);

static struct scheduler_backend *backend = NULL;

extern const char *backend_scheduler;

void
scheduler_imsg(struct imsgev *iev, struct imsg *imsg)
{
	struct envelope	*e, bounce;
	struct scheduler_info	si;

	log_imsg(PROC_SCHEDULER, iev->proc, imsg);

	switch (imsg->hdr.type) {
	case IMSG_QUEUE_COMMIT_MESSAGE:
		e = imsg->data;
		log_trace(TRACE_SCHEDULER,
		    "scheduler: IMSG_QUEUE_COMMIT_MESSAGE: %016"PRIx64, e->id);
		scheduler_load_message(evpid_to_msgid(e->id));
		scheduler_reset_events();
		return;

	case IMSG_QUEUE_DELIVERY_OK:
		stat_decrement(STATS_SCHEDULER);
		e = imsg->data;
		log_trace(TRACE_SCHEDULER,
		    "scheduler: IMSG_QUEUE_DELIVERY_OK: %016"PRIx64, e->id);
		backend->delete(e->id);
		queue_envelope_delete(e);
		scheduler_reset_events();
		return;

	case IMSG_QUEUE_DELIVERY_TEMPFAIL:
		stat_decrement(STATS_SCHEDULER);
		e = imsg->data;
		log_trace(TRACE_SCHEDULER,
		    "scheduler: IMSG_QUEUE_DELIVERY_TEMPFAIL: %016"PRIx64, e->id);
		e->retry++;
		queue_envelope_update(e);
		scheduler_info(&si, e);
		backend->update(&si);
		scheduler_reset_events();
		return;

	case IMSG_QUEUE_DELIVERY_PERMFAIL:
		stat_decrement(STATS_SCHEDULER);
		e = imsg->data;
		log_trace(TRACE_SCHEDULER,
		    "scheduler: IMSG_QUEUE_DELIVERY_PERMFAIL: %016"PRIx64, e->id);
		if (e->type != D_BOUNCE && e->sender.user[0] != '\0') {
			bounce_record_message(e, &bounce);
			scheduler_info(&si, &bounce);
			backend->insert(&si);
			backend->commit(evpid_to_msgid(bounce.id));
		}
		backend->delete(e->id);
		queue_envelope_delete(e);
		scheduler_reset_events();
		return;

	case IMSG_MDA_SESS_NEW:
		log_trace(TRACE_SCHEDULER, "scheduler: IMSG_MDA_SESS_NEW");
		stat_decrement(STATS_MDA_SESSION);
		if (env->sc_maxconn - stat_get(STATS_MDA_SESSION, STAT_ACTIVE))
			env->sc_flags &= ~SMTPD_MDA_BUSY;
		scheduler_reset_events();
		return;

	case IMSG_BATCH_DONE:
		log_trace(TRACE_SCHEDULER, "scheduler: IMSG_BATCH_DONE");
		stat_decrement(STATS_MTA_SESSION);
		if (env->sc_maxconn - stat_get(STATS_MTA_SESSION, STAT_ACTIVE))
			env->sc_flags &= ~SMTPD_MTA_BUSY;
		scheduler_reset_events();
		return;

	case IMSG_SMTP_ENQUEUE:
		e = imsg->data;
		log_trace(TRACE_SCHEDULER,
		    "scheduler: IMSG_SMTP_ENQUEUE: %016"PRIx64, e->id);
		if (imsg->fd < 0 || !bounce_session(imsg->fd, e)) {
			queue_envelope_update(e);
			scheduler_info(&si, e);
			backend->update(&si);
			scheduler_reset_events();
			return;
		}
		return;

	case IMSG_QUEUE_PAUSE_MDA:
		log_trace(TRACE_SCHEDULER, "scheduler: IMSG_QUEUE_PAUSE_MDA");
		env->sc_flags |= SMTPD_MDA_PAUSED;
		return;

	case IMSG_QUEUE_RESUME_MDA:
		log_trace(TRACE_SCHEDULER, "scheduler: IMSG_QUEUE_RESUME_MDA");
		env->sc_flags &= ~SMTPD_MDA_PAUSED;
		scheduler_reset_events();
		return;

	case IMSG_QUEUE_PAUSE_MTA:
		log_trace(TRACE_SCHEDULER, "scheduler: IMSG_QUEUE_PAUSE_MTA");
		env->sc_flags |= SMTPD_MTA_PAUSED;
		return;

	case IMSG_QUEUE_RESUME_MTA:
		log_trace(TRACE_SCHEDULER, "scheduler: IMSG_QUEUE_RESUME_MTA");
		env->sc_flags &= ~SMTPD_MTA_PAUSED;
		scheduler_reset_events();
		return;

	case IMSG_CTL_VERBOSE:
		log_trace(TRACE_SCHEDULER, "scheduler: IMSG_CTL_VERBOSE");
		log_verbose(*(int *)imsg->data);
		return;

	case IMSG_SCHEDULER_SCHEDULE:
		log_trace(TRACE_SCHEDULER,
		    "scheduler: IMSG_SCHEDULER_SCHEDULE: %016"PRIx64,
		    *(u_int64_t *)imsg->data);
		backend->schedule(*(u_int64_t *)imsg->data);
		scheduler_reset_events();
		return;

	case IMSG_SCHEDULER_REMOVE:
		log_trace(TRACE_SCHEDULER,
		    "scheduler: IMSG_SCHEDULER_REMOVE: %016"PRIx64,
		    *(u_int64_t *)imsg->data);
		backend->remove(*(u_int64_t *)imsg->data);
		scheduler_reset_events();
		return;
	}

	errx(1, "scheduler_imsg: unexpected %s imsg", imsg_to_str(imsg->hdr.type));
}

static void
scheduler_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		scheduler_shutdown();
		break;
	default:
		fatalx("scheduler_sig_handler: unexpected signal");
	}
}

static void
scheduler_shutdown(void)
{
	log_info("scheduler handler exiting");
	_exit(0);
}

static void
scheduler_reset_events(void)
{
	struct timeval	 tv;

	tv.tv_sec = 0;
	tv.tv_usec = 10;
	evtimer_add(&env->sc_ev, &tv);
}

pid_t
scheduler(void)
{
	pid_t		 pid;
	struct passwd	*pw;

	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_CONTROL,	imsg_dispatch },
		{ PROC_QUEUE,	imsg_dispatch }
	};

	switch (pid = fork()) {
	case -1:
		fatal("scheduler: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	purge_config(PURGE_EVERYTHING);

	pw = env->sc_pw;

	if (chroot(PATH_SPOOL) == -1)
		fatal("scheduler: chroot");
	if (chdir("/") == -1)
		fatal("scheduler: chdir(\"/\")");

	smtpd_process = PROC_SCHEDULER;
	setproctitle("%s", env->sc_title[smtpd_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("scheduler: cannot drop privileges");

	/* see fdlimit()-related comment in queue.c */
	fdlimit(1.0);
	if ((env->sc_maxconn = availdesc() / 4) < 1)
		fatalx("scheduler: fd starvation");

	env->sc_scheduler = scheduler_backend_lookup(backend_scheduler);
	if (env->sc_scheduler == NULL)
		errx(1, "cannot find scheduler backend \"%s\"", backend_scheduler);
	backend = env->sc_scheduler;

	backend->init();

	imsg_callback = scheduler_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, scheduler_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, scheduler_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_pipes(peers, nitems(peers));
	config_peers(peers, nitems(peers));

	evtimer_set(&env->sc_ev, scheduler_timeout, NULL);
	scheduler_reset_events();
	event_dispatch();
	scheduler_shutdown();

	return (0);
}

static void
scheduler_timeout(int fd, short event, void *p)
{
	struct timeval		tv;
	struct scheduler_batch	batch;
	int			typemask;

	log_trace(TRACE_SCHEDULER, "scheduler: getting next batch");

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	typemask = SCHED_REMOVE | SCHED_EXPIRE | SCHED_BOUNCE;
	if (!(env->sc_flags & SMTPD_MDA_PAUSED))
		typemask |= SCHED_MDA;
	if (!(env->sc_flags & SMTPD_MTA_PAUSED))
		typemask |= SCHED_MTA;

	backend->batch(typemask, time(NULL), &batch);
	switch (batch.type) {
	case SCHED_NONE:
		log_trace(TRACE_SCHEDULER, "scheduler: sleeping");
		return;

	case SCHED_DELAY:
		tv.tv_sec = batch.delay;
		log_trace(TRACE_SCHEDULER,
		    "scheduler: pausing for %li seconds", tv.tv_sec);
		break;

	case SCHED_REMOVE:
		scheduler_process_remove(&batch);
		break;

	case SCHED_EXPIRE:
		scheduler_process_expire(&batch);
		break;

	case SCHED_BOUNCE:
		scheduler_process_bounce(&batch);
		break;

	case SCHED_MDA:
		scheduler_process_mda(&batch);
		break;

	case SCHED_MTA:
		scheduler_process_mta(&batch);
		break;

	default:
		fatalx("scheduler_timeout: unknown batch type");
	}

	evtimer_add(&env->sc_ev, &tv);	
}

static void
scheduler_process_remove(struct scheduler_batch *batch)
{
	struct envelope evp;
	struct id_list	*e;

	while ((e = batch->evpids)) {
		batch->evpids = e->next;
		log_debug("scheduler: deleting evp:%016" PRIx64 " (removed)",
		    e->id);
		evp.id = e->id;
		queue_envelope_delete(&evp);
		free(e);
	}
}

static void
scheduler_process_expire(struct scheduler_batch *batch)
{
	struct envelope evp;
	struct id_list	*e;

	while ((e = batch->evpids)) {
		batch->evpids = e->next;
		log_debug("scheduler: deleting evp:%016" PRIx64 " (expire)",
		    e->id);
		evp.id = e->id;
		queue_envelope_delete(&evp);
		free(e);
	}
}

static void
scheduler_process_bounce(struct scheduler_batch *batch)
{
	struct envelope	 evp;
	struct id_list	*e;

	while ((e = batch->evpids)) {
		batch->evpids = e->next;
		log_debug("scheduler: scheduling evp:%016" PRIx64 " (bounce)",
		    e->id);
		queue_envelope_load(e->id, &evp);
		evp.lasttry = time(NULL);
		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_SMTP_ENQUEUE, PROC_SMTP, 0, -1, &evp,
		    sizeof evp);
		stat_increment(STATS_SCHEDULER);
		stat_increment(STATS_SCHEDULER_BOUNCES);
		free(e);
	}
}

static void
scheduler_process_mda(struct scheduler_batch *batch)
{
	struct envelope	 evp;
	struct id_list	*e;
	int		 fd;

	while ((e = batch->evpids)) {
		batch->evpids = e->next;
		log_debug("scheduler: scheduling evp:%016" PRIx64 " (mda)",
		    e->id);
		queue_envelope_load(e->id, &evp);
		evp.lasttry = time(NULL);
		fd = queue_message_fd_r(evpid_to_msgid(evp.id));
		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_MDA_SESS_NEW, PROC_MDA, 0, fd, &evp,
		    sizeof evp);
		stat_increment(STATS_SCHEDULER);
		stat_increment(STATS_MDA_SESSION);
		free(e);
	}
}

static void
scheduler_process_mta(struct scheduler_batch *batch)
{
	struct envelope		 evp;
	struct mta_batch	 mta_batch;
	struct id_list		*e;

	queue_envelope_load(batch->evpids->id, &evp);

	bzero(&mta_batch, sizeof mta_batch);
	mta_batch.id    = arc4random();
	mta_batch.relay = evp.agent.mta.relay;

	imsg_compose_event(env->sc_ievs[PROC_QUEUE],
	    IMSG_BATCH_CREATE, PROC_MTA, 0, -1, &mta_batch,
	    sizeof mta_batch);

	while ((e = batch->evpids)) {
		batch->evpids = e->next;
		log_debug("scheduler: scheduling evp:%016" PRIx64 " (mta)",
		    e->id);
		queue_envelope_load(e->id, &evp);
		evp.lasttry = time(NULL);
		evp.batch_id = mta_batch.id;
		imsg_compose_event(env->sc_ievs[PROC_QUEUE], IMSG_BATCH_APPEND,
		    PROC_MTA, 0, -1, &evp, sizeof evp);
		free(e);
		stat_increment(STATS_SCHEDULER);
	}

	imsg_compose_event(env->sc_ievs[PROC_QUEUE], IMSG_BATCH_CLOSE,
	    PROC_MTA, 0, -1, &mta_batch, sizeof mta_batch);

	stat_increment(STATS_MTA_SESSION);
}

void
scheduler_envelope_update(struct envelope *e)
{
	struct scheduler_info si;

	scheduler_info(&si, e);
	backend->update(&si);
	scheduler_reset_events();
}

void
scheduler_envelope_delete(struct envelope *e)
{
	backend->delete(e->id);
	scheduler_reset_events();
}

static int
scheduler_load_message(u_int32_t msgid)
{
	struct qwalk	*q;
	u_int64_t	 evpid;
	struct envelope	 envelope;
	struct scheduler_info   si;

	q = qwalk_new(msgid);
	while (qwalk(q, &evpid)) {
		if (! queue_envelope_load(evpid, &envelope))
			continue;
		scheduler_info(&si, &envelope);
		backend->insert(&si);
	}
	qwalk_close(q);
	backend->commit(msgid);

	return 1;
}

/*	$OpenBSD: scheduler.c,v 1.9 2012/08/09 09:48:02 eric Exp $	*/

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

static struct scheduler_backend *backend = NULL;

extern const char *backend_scheduler;

void
scheduler_imsg(struct imsgev *iev, struct imsg *imsg)
{
	struct envelope		*e;
	struct scheduler_info	 si;
	uint64_t		 id;
	uint32_t		 msgid;

	log_imsg(PROC_SCHEDULER, iev->proc, imsg);

	switch (imsg->hdr.type) {

	case IMSG_QUEUE_SUBMIT_ENVELOPE:
		e = imsg->data;
		log_trace(TRACE_SCHEDULER,
		    "scheduler: inserting evp:%016" PRIx64, e->id);
		scheduler_info(&si, e);
		backend->insert(&si);
		return;

	case IMSG_QUEUE_COMMIT_MESSAGE:
		msgid = *(uint32_t *)(imsg->data);
		log_trace(TRACE_SCHEDULER,
		    "scheduler: commiting msg:%08" PRIx32, msgid);
		backend->commit(msgid);
		scheduler_reset_events();
		return;

	case IMSG_QUEUE_TEMPFAIL:
		msgid = *(uint32_t *)(imsg->data);
		log_trace(TRACE_SCHEDULER, "scheduler: aborting msg:%08" PRIx32,
		    msgid);
		backend->rollback(msgid);
		scheduler_reset_events();
		return;

	case IMSG_QUEUE_DELIVERY_OK:
		id = *(uint64_t *)(imsg->data);
		log_trace(TRACE_SCHEDULER,
		    "scheduler: deleting evp:%016" PRIx64 " (ok)", id);
		backend->delete(id);
		scheduler_reset_events();
		return;

	case IMSG_QUEUE_DELIVERY_TEMPFAIL:
		e = imsg->data;
		log_trace(TRACE_SCHEDULER,
		    "scheduler: updating evp:%016" PRIx64, e->id);
		scheduler_info(&si, e);
		backend->update(&si);
		scheduler_reset_events();
		return;

	case IMSG_QUEUE_DELIVERY_PERMFAIL:
		id = *(uint64_t *)(imsg->data);
		log_trace(TRACE_SCHEDULER,
		    "scheduler: deleting evp:%016" PRIx64 " (fail)", id);
		backend->delete(id);
		scheduler_reset_events();
		return;

	case IMSG_QUEUE_PAUSE_MDA:
		log_trace(TRACE_SCHEDULER, "scheduler: pausing mda");
		env->sc_flags |= SMTPD_MDA_PAUSED;
		return;

	case IMSG_QUEUE_RESUME_MDA:
		log_trace(TRACE_SCHEDULER, "scheduler: resuming mda");
		env->sc_flags &= ~SMTPD_MDA_PAUSED;
		scheduler_reset_events();
		return;

	case IMSG_QUEUE_PAUSE_MTA:
		log_trace(TRACE_SCHEDULER, "scheduler: pausing mta");
		env->sc_flags |= SMTPD_MTA_PAUSED;
		return;

	case IMSG_QUEUE_RESUME_MTA:
		log_trace(TRACE_SCHEDULER, "scheduler: resuming mta");
		env->sc_flags &= ~SMTPD_MTA_PAUSED;
		scheduler_reset_events();
		return;

	case IMSG_CTL_VERBOSE:
		log_verbose(*(int *)imsg->data);
		return;

	case IMSG_SCHEDULER_SCHEDULE:
		id = *(uint64_t *)(imsg->data);
		log_debug("scheduler: scheduling evp:%016" PRIx64, id);
		backend->schedule(id);
		scheduler_reset_events();
		return;

	case IMSG_SCHEDULER_REMOVE:
		id = *(uint64_t *)(imsg->data);
		if (id <= 0xffffffffL)
			log_debug("scheduler: removing msg:%08" PRIx64, id);
		else
			log_debug("scheduler: removing evp:%016" PRIx64, id);
		backend->remove(id);
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
	struct id_list	*e;

	while ((e = batch->evpids)) {
		batch->evpids = e->next;
		log_debug("scheduler: evp:%016" PRIx64 " removed",
		    e->id);
		backend->delete(e->id);
		imsg_compose_event(env->sc_ievs[PROC_QUEUE], IMSG_QUEUE_REMOVE,
		    0, 0, -1, &e->id, sizeof e->id);
		free(e);
	}
}

static void
scheduler_process_expire(struct scheduler_batch *batch)
{
	struct id_list	*e;

	while ((e = batch->evpids)) {
		batch->evpids = e->next;
		log_debug("scheduler: evp:%016" PRIx64 " expired",
		    e->id);
		backend->delete(e->id);
		imsg_compose_event(env->sc_ievs[PROC_QUEUE], IMSG_QUEUE_EXPIRE,
		    0, 0, -1, &e->id, sizeof e->id);
		free(e);
	}
}

static void
scheduler_process_bounce(struct scheduler_batch *batch)
{
	struct id_list	*e;

	while ((e = batch->evpids)) {
		batch->evpids = e->next;
		log_debug("scheduler: evp:%016" PRIx64 " scheduled (bounce)",
		    e->id);
		imsg_compose_event(env->sc_ievs[PROC_QUEUE], IMSG_SMTP_ENQUEUE,
		    0, 0, -1, &e->id, sizeof e->id);
		free(e);
	}
}

static void
scheduler_process_mda(struct scheduler_batch *batch)
{
	struct id_list	*e;

	while ((e = batch->evpids)) {
		batch->evpids = e->next;
		log_debug("scheduler: evp:%016" PRIx64 " scheduled (mda)",
		    e->id);
		imsg_compose_event(env->sc_ievs[PROC_QUEUE], IMSG_MDA_SESS_NEW,
		    0, 0, -1, &e->id, sizeof e->id);
		free(e);
	}
}

static void
scheduler_process_mta(struct scheduler_batch *batch)
{
	struct id_list		*e;

	imsg_compose_event(env->sc_ievs[PROC_QUEUE], IMSG_BATCH_CREATE,
	    0, 0, -1, NULL, 0);

	while ((e = batch->evpids)) {
		batch->evpids = e->next;
		log_debug("scheduler: evp:%016" PRIx64 " scheduled (mta)",
		    e->id);
		imsg_compose_event(env->sc_ievs[PROC_QUEUE], IMSG_BATCH_APPEND,
		    0, 0, -1, &e->id, sizeof e->id);
		free(e);
	}

	imsg_compose_event(env->sc_ievs[PROC_QUEUE], IMSG_BATCH_CLOSE,
	    0, 0, -1, NULL, 0);

	stat_increment(STATS_MTA_SESSION);
}

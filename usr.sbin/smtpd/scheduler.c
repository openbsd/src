/*	$OpenBSD: scheduler.c,v 1.27 2013/02/10 15:01:16 eric Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@poolp.org>
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

static void scheduler_imsg(struct mproc *, struct imsg *);
static void scheduler_shutdown(void);
static void scheduler_sig_handler(int, short, void *);
static void scheduler_reset_events(void);
static void scheduler_timeout(int, short, void *);
static void scheduler_process_remove(struct scheduler_batch *);
static void scheduler_process_expire(struct scheduler_batch *);
static void scheduler_process_bounce(struct scheduler_batch *);
static void scheduler_process_mda(struct scheduler_batch *);
static void scheduler_process_mta(struct scheduler_batch *);

static struct scheduler_backend *backend = NULL;

extern const char *backend_scheduler;

#define	MSGBATCHSIZE	1024
#define	EVPBATCHSIZE	256

#define SCHEDULE_MAX	1024

void
scheduler_imsg(struct mproc *p, struct imsg *imsg)
{
	struct bounce_req_msg	 req;
	struct evpstate		 state[EVPBATCHSIZE];
	struct envelope		 evp;
	struct scheduler_info	 si;
	struct msg		 m;
	uint64_t		 evpid, id;
	uint32_t		 msgid, msgids[MSGBATCHSIZE];
	size_t			 n, i;
	time_t			 timestamp;
	int			 v;

	switch (imsg->hdr.type) {

	case IMSG_QUEUE_SUBMIT_ENVELOPE:
		m_msg(&m, imsg);
		m_get_envelope(&m, &evp);
		m_end(&m);
		log_trace(TRACE_SCHEDULER,
		    "scheduler: inserting evp:%016" PRIx64, evp.id);
		scheduler_info(&si, &evp);
		stat_increment("scheduler.envelope.incoming", 1);
		backend->insert(&si);
		return;

	case IMSG_QUEUE_COMMIT_MESSAGE:
		m_msg(&m, imsg);
		m_get_msgid(&m, &msgid);
		m_end(&m);
		log_trace(TRACE_SCHEDULER,
		    "scheduler: commiting msg:%08" PRIx32, msgid);
		n = backend->commit(msgid);
		stat_decrement("scheduler.envelope.incoming", n);
		stat_increment("scheduler.envelope", n);
		scheduler_reset_events();
		return;

	case IMSG_QUEUE_REMOVE_MESSAGE:
		m_msg(&m, imsg);
		m_get_msgid(&m, &msgid);
		m_end(&m);
		log_trace(TRACE_SCHEDULER, "scheduler: aborting msg:%08" PRIx32,
		    msgid);
		n = backend->rollback(msgid);
		stat_decrement("scheduler.envelope.incoming", n);
		scheduler_reset_events();
		return;

	case IMSG_DELIVERY_OK:
		m_msg(&m, imsg);
		m_get_evpid(&m, &evpid);
		m_end(&m);
		log_trace(TRACE_SCHEDULER,
		    "scheduler: deleting evp:%016" PRIx64 " (ok)", evpid);
		backend->delete(evpid);
		stat_increment("scheduler.delivery.ok", 1);
		stat_decrement("scheduler.envelope.inflight", 1);
		stat_decrement("scheduler.envelope", 1);
		scheduler_reset_events();
		return;

	case IMSG_DELIVERY_TEMPFAIL:
		m_msg(&m, imsg);
		m_get_envelope(&m, &evp);
		m_end(&m);
		log_trace(TRACE_SCHEDULER,
		    "scheduler: updating evp:%016" PRIx64, evp.id);
		scheduler_info(&si, &evp);
		backend->update(&si);
		stat_increment("scheduler.delivery.tempfail", 1);
		stat_decrement("scheduler.envelope.inflight", 1);

		for (i = 0; i < MAX_BOUNCE_WARN; i++) {
			if (env->sc_bounce_warn[i] == 0)
				break;
			timestamp = si.creation + env->sc_bounce_warn[i];
			if (si.nexttry >= timestamp &&
			    si.lastbounce < timestamp) {
	    			req.evpid = evp.id;
				req.timestamp = timestamp;
				req.bounce.type = B_WARNING;
				req.bounce.delay = env->sc_bounce_warn[i];
				req.bounce.expire = si.expire;
				m_compose(p, IMSG_QUEUE_BOUNCE, 0, 0, -1,
				    &req, sizeof req);
				break;
			}
		}
		scheduler_reset_events();
		return;

	case IMSG_DELIVERY_PERMFAIL:
		m_msg(&m, imsg);
		m_get_evpid(&m, &evpid);
		m_end(&m);
		log_trace(TRACE_SCHEDULER,
		    "scheduler: deleting evp:%016" PRIx64 " (fail)", evpid);
		backend->delete(evpid);
		stat_increment("scheduler.delivery.permfail", 1);
		stat_decrement("scheduler.envelope.inflight", 1);
		stat_decrement("scheduler.envelope", 1);
		scheduler_reset_events();
		return;

	case IMSG_DELIVERY_LOOP:
		m_msg(&m, imsg);
		m_get_evpid(&m, &evpid);
		m_end(&m);
		log_trace(TRACE_SCHEDULER,
		    "scheduler: deleting evp:%016" PRIx64 " (loop)", evpid);
		backend->delete(evpid);
		stat_increment("scheduler.delivery.loop", 1);
		stat_decrement("scheduler.envelope.inflight", 1);
		stat_decrement("scheduler.envelope", 1);
		scheduler_reset_events();
		return;

	case IMSG_CTL_PAUSE_MDA:
		log_trace(TRACE_SCHEDULER, "scheduler: pausing mda");
		env->sc_flags |= SMTPD_MDA_PAUSED;
		return;

	case IMSG_CTL_RESUME_MDA:
		log_trace(TRACE_SCHEDULER, "scheduler: resuming mda");
		env->sc_flags &= ~SMTPD_MDA_PAUSED;
		scheduler_reset_events();
		return;

	case IMSG_CTL_PAUSE_MTA:
		log_trace(TRACE_SCHEDULER, "scheduler: pausing mta");
		env->sc_flags |= SMTPD_MTA_PAUSED;
		return;

	case IMSG_CTL_RESUME_MTA:
		log_trace(TRACE_SCHEDULER, "scheduler: resuming mta");
		env->sc_flags &= ~SMTPD_MTA_PAUSED;
		scheduler_reset_events();
		return;

	case IMSG_CTL_VERBOSE:
		m_msg(&m, imsg);
		m_get_int(&m, &v);
		m_end(&m);
		log_verbose(v);
		return;

	case IMSG_CTL_LIST_MESSAGES:
		msgid = *(uint32_t *)(imsg->data);
		n = backend->messages(msgid, msgids, MSGBATCHSIZE);
		m_compose(p, IMSG_CTL_LIST_MESSAGES, imsg->hdr.peerid, 0, -1,
		    msgids, n * sizeof (*msgids));
		return;

	case IMSG_CTL_LIST_ENVELOPES:
		id = *(uint64_t *)(imsg->data);
		n = backend->envelopes(id, state, EVPBATCHSIZE);
		for (i = 0; i < n; i++) {
			m_create(p_queue, IMSG_CTL_LIST_ENVELOPES,
			    imsg->hdr.peerid, 0, -1, 32);
			m_add_evpid(p_queue, state[i].evpid);
			m_add_int(p_queue, state[i].flags);
			m_add_time(p_queue, state[i].time);
			m_close(p_queue);
		}
		m_compose(p_queue, IMSG_CTL_LIST_ENVELOPES,
		    imsg->hdr.peerid, 0, -1, NULL, 0);
		return;

	case IMSG_CTL_SCHEDULE:
		id = *(uint64_t *)(imsg->data);
		if (id <= 0xffffffffL)
			log_debug("debug: scheduler: "
			    "scheduling msg:%08" PRIx64, id);
		else
			log_debug("debug: scheduler: "
			    "scheduling evp:%016" PRIx64, id);
		backend->schedule(id);
		scheduler_reset_events();
		return;

	case IMSG_CTL_REMOVE:
		id = *(uint64_t *)(imsg->data);
		if (id <= 0xffffffffL)
			log_debug("debug: scheduler: "
			    "removing msg:%08" PRIx64, id);
		else
			log_debug("debug: scheduler: "
			    "removing evp:%016" PRIx64, id);
		backend->remove(id);
		scheduler_reset_events();
		return;
	}

	errx(1, "scheduler_imsg: unexpected %s imsg",
	    imsg_to_str(imsg->hdr.type));
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
	log_info("info: scheduler handler exiting");
	_exit(0);
}

static void
scheduler_reset_events(void)
{
	struct timeval	 tv;

	evtimer_del(&env->sc_ev);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	evtimer_add(&env->sc_ev, &tv);
}

pid_t
scheduler(void)
{
	pid_t		 pid;
	struct passwd	*pw;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	switch (pid = fork()) {
	case -1:
		fatal("scheduler: cannot fork");
	case 0:
		env->sc_pid = getpid();
		break;
	default:
		return (pid);
	}

	purge_config(PURGE_EVERYTHING);

	pw = env->sc_pw;
	if (chroot(pw->pw_dir) == -1)
		fatal("scheduler: chroot");
	if (chdir("/") == -1)
		fatal("scheduler: chdir(\"/\")");

	config_process(PROC_SCHEDULER);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("scheduler: cannot drop privileges");

	fdlimit(1.0);

	env->sc_scheduler = scheduler_backend_lookup(backend_scheduler);
	if (env->sc_scheduler == NULL)
		errx(1, "cannot find scheduler backend \"%s\"",
		    backend_scheduler);
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

	config_peer(PROC_CONTROL);
	config_peer(PROC_QUEUE);
	config_done();

	evtimer_set(&env->sc_ev, scheduler_timeout, NULL);
	scheduler_reset_events();
	if (event_dispatch() < 0)
		fatal("event_dispatch");
	scheduler_shutdown();

	return (0);
}

static void
scheduler_timeout(int fd, short event, void *p)
{
	struct timeval		tv;
	struct scheduler_batch	batch;
	int			typemask;
	uint64_t		evpids[SCHEDULE_MAX];

	log_trace(TRACE_SCHEDULER, "scheduler: getting next batch");

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	typemask = SCHED_REMOVE | SCHED_EXPIRE | SCHED_BOUNCE;
	if (!(env->sc_flags & SMTPD_MDA_PAUSED))
		typemask |= SCHED_MDA;
	if (!(env->sc_flags & SMTPD_MTA_PAUSED))
		typemask |= SCHED_MTA;

	bzero(&batch, sizeof (batch));
	batch.evpids = evpids;
	batch.evpcount = SCHEDULE_MAX;

	backend->batch(typemask, &batch);

	switch (batch.type) {
	case SCHED_NONE:
		log_trace(TRACE_SCHEDULER, "scheduler: SCHED_NONE");
		return;

	case SCHED_DELAY:
		tv.tv_sec = batch.delay;
		log_trace(TRACE_SCHEDULER,
		    "scheduler: SCHED_DELAY %s", duration_to_text(tv.tv_sec));
		break;

	case SCHED_REMOVE:
		log_trace(TRACE_SCHEDULER, "scheduler: SCHED_REMOVE %zu",
		    batch.evpcount);
		scheduler_process_remove(&batch);
		break;

	case SCHED_EXPIRE:
		log_trace(TRACE_SCHEDULER, "scheduler: SCHED_EXPIRE %zu",
		    batch.evpcount);
		scheduler_process_expire(&batch);
		break;

	case SCHED_BOUNCE:
		log_trace(TRACE_SCHEDULER, "scheduler: SCHED_BOUNCE %zu",
		    batch.evpcount);
		scheduler_process_bounce(&batch);
		break;

	case SCHED_MDA:
		log_trace(TRACE_SCHEDULER, "scheduler: SCHED_MDA %zu",
		    batch.evpcount);
		scheduler_process_mda(&batch);
		break;

	case SCHED_MTA:
		log_trace(TRACE_SCHEDULER, "scheduler: SCHED_MTA %zu",
		    batch.evpcount);
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
	size_t	i;

	for (i = 0; i < batch->evpcount; i++) {
		log_debug("debug: scheduler: evp:%016" PRIx64 " removed",
		    batch->evpids[i]);
		m_create(p_queue, IMSG_QUEUE_REMOVE, 0, 0, -1, 9);
		m_add_evpid(p_queue, batch->evpids[i]);
		m_close(p_queue);
	}

	stat_decrement("scheduler.envelope", batch->evpcount);
	stat_increment("scheduler.envelope.removed", batch->evpcount);
}

static void
scheduler_process_expire(struct scheduler_batch *batch)
{
	size_t	i;

	for (i = 0; i < batch->evpcount; i++) {
		log_debug("debug: scheduler: evp:%016" PRIx64 " expired",
		    batch->evpids[i]);
		m_create(p_queue, IMSG_QUEUE_EXPIRE, 0, 0, -1, 9);
		m_add_evpid(p_queue, batch->evpids[i]);
		m_close(p_queue);
	}

	stat_decrement("scheduler.envelope", batch->evpcount);
	stat_increment("scheduler.envelope.expired", batch->evpcount);
}

static void
scheduler_process_bounce(struct scheduler_batch *batch)
{
	size_t	i;

	for (i = 0; i < batch->evpcount; i++) {
		log_debug("debug: scheduler: evp:%016" PRIx64
		    " scheduled (bounce)", batch->evpids[i]);
		m_create(p_queue, IMSG_BOUNCE_INJECT, 0, 0, -1, 9);
		m_add_evpid(p_queue, batch->evpids[i]);
		m_close(p_queue);
	}

	stat_increment("scheduler.envelope.inflight", batch->evpcount);
}

static void
scheduler_process_mda(struct scheduler_batch *batch)
{
	size_t	i;

	for (i = 0; i < batch->evpcount; i++) {
		log_debug("debug: scheduler: evp:%016" PRIx64
		    " scheduled (mda)", batch->evpids[i]);
		m_create(p_queue, IMSG_MDA_DELIVER, 0, 0, -1, 9);
		m_add_evpid(p_queue, batch->evpids[i]);
		m_close(p_queue);
	}

	stat_increment("scheduler.envelope.inflight", batch->evpcount);
}

static void
scheduler_process_mta(struct scheduler_batch *batch)
{
	size_t	i;

	m_compose(p_queue, IMSG_MTA_BATCH, 0, 0, -1, NULL, 0);

	for (i = 0; i < batch->evpcount; i++) {
		log_debug("debug: scheduler: evp:%016" PRIx64
		    " scheduled (mta)", batch->evpids[i]);
		m_create(p_queue, IMSG_MTA_BATCH_ADD, 0, 0, -1, 9);
		m_add_evpid(p_queue, batch->evpids[i]);
		m_close(p_queue);
	}

	m_compose(p_queue, IMSG_MTA_BATCH_END, 0, 0, -1, NULL, 0);

	stat_increment("scheduler.envelope.inflight", batch->evpcount);
}

/*	$OpenBSD: scheduler.c,v 1.4 2012/07/09 09:57:53 gilles Exp $	*/

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
static void scheduler_setup_events(void);
static void scheduler_reset_events(void);
static void scheduler_disable_events(void);
static void scheduler_timeout(int, short, void *);
static void scheduler_remove(u_int64_t);
static void scheduler_remove_envelope(u_int64_t);
static int scheduler_process_envelope(u_int64_t);
static int scheduler_process_batch(enum delivery_type, u_int64_t);
static int scheduler_check_loop(struct envelope *);
static int scheduler_message_to_scheduler(u_int32_t);

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
		scheduler_message_to_scheduler(evpid_to_msgid(e->id));
		scheduler_reset_events();
		return;

	case IMSG_QUEUE_DELIVERY_OK:
		stat_decrement(STATS_SCHEDULER);
		e = imsg->data;
		log_debug("queue_delivery_ok: %016"PRIx64, e->id);
		backend->remove(e->id);
		queue_envelope_delete(e);
		return;

	case IMSG_QUEUE_DELIVERY_TEMPFAIL:
		stat_decrement(STATS_SCHEDULER);
		e = imsg->data;
		e->retry++;
		queue_envelope_update(e);
		log_debug("queue_delivery_tempfail: %016"PRIx64, e->id);
		scheduler_info(&si, e);
		backend->insert(&si);
		scheduler_reset_events();
		return;

	case IMSG_QUEUE_DELIVERY_PERMFAIL:
		stat_decrement(STATS_SCHEDULER);
		e = imsg->data;
		if (e->type != D_BOUNCE && e->sender.user[0] != '\0') {
			bounce_record_message(e, &bounce);
			log_debug("queue_delivery_permfail: %016"PRIx64,
			    bounce.id);
			scheduler_info(&si, &bounce);
			backend->insert(&si);
			scheduler_reset_events();
		}
		backend->remove(e->id);
		queue_envelope_delete(e);
		return;

	case IMSG_MDA_SESS_NEW:
		stat_decrement(STATS_MDA_SESSION);
		if (env->sc_maxconn - stat_get(STATS_MDA_SESSION, STAT_ACTIVE))
			env->sc_flags &= ~SMTPD_MDA_BUSY;
		scheduler_reset_events();
		return;

	case IMSG_BATCH_DONE:
		stat_decrement(STATS_MTA_SESSION);
		if (env->sc_maxconn - stat_get(STATS_MTA_SESSION, STAT_ACTIVE))
			env->sc_flags &= ~SMTPD_MTA_BUSY;
		scheduler_reset_events();
		return;

	case IMSG_SMTP_ENQUEUE:
		e = imsg->data;
		if (imsg->fd < 0 || !bounce_session(imsg->fd, e)) {
			queue_envelope_update(e);
			log_debug("smtp_enqueue: %016"PRIx64, e->id);
			scheduler_info(&si, e);
			backend->insert(&si);
			scheduler_reset_events();
			return;
		}
		return;

	case IMSG_QUEUE_PAUSE_MDA:
		env->sc_flags |= SMTPD_MDA_PAUSED;
		return;

	case IMSG_QUEUE_RESUME_MDA:
		env->sc_flags &= ~SMTPD_MDA_PAUSED;
		scheduler_reset_events();
		return;

	case IMSG_QUEUE_PAUSE_MTA:
		env->sc_flags |= SMTPD_MTA_PAUSED;
		return;

	case IMSG_QUEUE_RESUME_MTA:
		env->sc_flags &= ~SMTPD_MTA_PAUSED;
		scheduler_reset_events();
		return;

	case IMSG_CTL_VERBOSE:
		log_verbose(*(int *)imsg->data);
		return;

	case IMSG_SCHEDULER_SCHEDULE:
		backend->force(*(u_int64_t *)imsg->data);
		scheduler_reset_events();		
		return;

	case IMSG_SCHEDULER_REMOVE: {
		scheduler_remove(*(u_int64_t *)imsg->data);
		scheduler_reset_events();
		return;
	}
	}

	errx(1, "scheduler_imsg: unexpected %s imsg", imsg_to_str(imsg->hdr.type));
}

void
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

void
scheduler_shutdown(void)
{
	log_info("scheduler handler exiting");
	_exit(0);
}

void
scheduler_setup_events(void)
{
	struct timeval	 tv;

	evtimer_set(&env->sc_ev, scheduler_timeout, NULL);
	tv.tv_sec = 0;
	tv.tv_usec = 10;
	evtimer_add(&env->sc_ev, &tv);
}

void
scheduler_reset_events(void)
{
	struct timeval	 tv;

	tv.tv_sec = 0;
	tv.tv_usec = 10;
	evtimer_add(&env->sc_ev, &tv);
}

void
scheduler_disable_events(void)
{
	evtimer_del(&env->sc_ev);
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

	scheduler_setup_events();
	event_dispatch();
	scheduler_disable_events();
	scheduler_shutdown();

	return (0);
}

void
scheduler_timeout(int fd, short event, void *p)
{
	struct timeval		 tv;
	static int		 rq_done = 0;
	time_t			 nsched;
	time_t			 curtm;
	u_int64_t		 evpid;

	nsched = 0;

again:
/*
	if (scheduler->display)
		scheduler->display();
*/

	curtm = time(NULL);

	/* set nsched to the time() of next schedulable envelope */
	backend->next(NULL, &nsched);

	/* load as many envelopes as possible from disk-queue to
	 * ram-queue until a schedulable envelope is found.
	 */
	if (! rq_done)
		rq_done = backend->setup(curtm, nsched);

	if (rq_done) {
		if (! backend->next(NULL, &nsched))
			goto scheduler_sleep;
		if (curtm < nsched)
			goto scheduler_pause;
	}

	/* let's do the schedule dance baby ! */
	while (backend->next(&evpid, &nsched)) {
		if (curtm < nsched)
			goto scheduler_pause;

		scheduler_process_envelope(evpid);
	}

	if (rq_done)
		goto scheduler_sleep;

	goto again;


scheduler_pause:
	nsched = nsched - curtm;

	log_debug("scheduler: nothing to do for the next %lld seconds, zZzZzZ",
	    (long long int) nsched);

	tv.tv_sec = nsched;
	tv.tv_usec = 0;
	evtimer_add(&env->sc_ev, &tv);
	return;


scheduler_sleep:
	log_debug("scheduler: nothing to schedule, wake me up. zZzZzZ");
	return;
}

static int
scheduler_process_envelope(u_int64_t evpid)
{
	struct envelope	 envelope;
	size_t		 mta_av, mda_av, bnc_av;
	struct scheduler_info	si;

	mta_av = env->sc_maxconn - stat_get(STATS_MTA_SESSION, STAT_ACTIVE);
	mda_av = env->sc_maxconn - stat_get(STATS_MDA_SESSION, STAT_ACTIVE);
	bnc_av = env->sc_maxconn - stat_get(STATS_SCHEDULER_BOUNCES, STAT_ACTIVE);

	if (! queue_envelope_load(evpid, &envelope))
		return 0;

	if (envelope.type == D_MDA)
		if (mda_av == 0) {
			env->sc_flags |= SMTPD_MDA_BUSY;
			return 0;
		}

	if (envelope.type == D_MTA)
		if (mta_av == 0) {
			env->sc_flags |= SMTPD_MTA_BUSY;
			return 0;
		}

	if (envelope.type == D_BOUNCE)
		if (bnc_av == 0) {
			env->sc_flags |= SMTPD_BOUNCE_BUSY;
			return 0;
		}

	if (scheduler_check_loop(&envelope)) {
		struct envelope bounce;

		envelope_set_errormsg(&envelope, "loop has been detected");
		if (bounce_record_message(&envelope, &bounce)) {
			scheduler_info(&si, &bounce);
			backend->insert(&si);
		}
		backend->remove(evpid);
		queue_envelope_delete(&envelope);

		scheduler_reset_events();

		return 0;
	}


	return scheduler_process_batch(envelope.type, evpid);
}

static int
scheduler_process_batch(enum delivery_type type, u_int64_t evpid)
{
	struct envelope evp;
	void *batch;
	int fd;

	batch = backend->batch(evpid);
	switch (type) {
	case D_BOUNCE:
		while (backend->fetch(batch, &evpid)) {
			if (! queue_envelope_load(evpid, &evp))
				goto end;

			evp.lasttry = time(NULL);
			imsg_compose_event(env->sc_ievs[PROC_QUEUE],
			    IMSG_SMTP_ENQUEUE, PROC_SMTP, 0, -1, &evp,
			    sizeof evp);
			backend->schedule(evpid);
		}
		stat_increment(STATS_SCHEDULER);
		stat_increment(STATS_SCHEDULER_BOUNCES);
		break;
		
	case D_MDA:
		backend->fetch(batch, &evpid);
		if (! queue_envelope_load(evpid, &evp))
			goto end;
		
		evp.lasttry = time(NULL);
		fd = queue_message_fd_r(evpid_to_msgid(evpid));
		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_MDA_SESS_NEW, PROC_MDA, 0, fd, &evp,
		    sizeof evp);
		backend->schedule(evpid);

		stat_increment(STATS_SCHEDULER);
		stat_increment(STATS_MDA_SESSION);
		break;

	case D_MTA: {
		struct mta_batch mta_batch;

		/* FIXME */
		if (! backend->fetch(batch, &evpid))
			goto end;
		if (! queue_envelope_load(evpid, &evp))
			goto end;

		bzero(&mta_batch, sizeof mta_batch);
		mta_batch.id    = arc4random();
		mta_batch.relay = evp.agent.mta.relay;

		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_BATCH_CREATE, PROC_MTA, 0, -1, &mta_batch,
		    sizeof mta_batch);

		while (backend->fetch(batch, &evpid)) {
			if (! queue_envelope_load(evpid, &evp))
				goto end;
			evp.lasttry = time(NULL); /* FIXME */
			evp.batch_id = mta_batch.id;

			imsg_compose_event(env->sc_ievs[PROC_QUEUE],
			    IMSG_BATCH_APPEND, PROC_MTA, 0, -1, &evp,
			    sizeof evp);

			backend->schedule(evpid);
			stat_increment(STATS_SCHEDULER);
		}

		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_BATCH_CLOSE, PROC_MTA, 0, -1, &mta_batch,
		    sizeof mta_batch);

		stat_increment(STATS_MTA_SESSION);
		break;
	}
		
	default:
		fatalx("scheduler_process_batchqueue: unknown type");
	}

end:
	backend->close(batch);
	return 1;
}

static int
scheduler_message_to_scheduler(u_int32_t msgid)
{
	struct qwalk	*q;
	u_int64_t	 evpid;
	struct envelope	 envelope;
	struct scheduler_info	si;

	q = qwalk_new(msgid);
	while (qwalk(q, &evpid)) {
		if (! queue_envelope_load(evpid, &envelope))
			continue;
		scheduler_info(&si, &envelope);
		backend->insert(&si);
	}
 	qwalk_close(q);

	return 1;
}

static int
scheduler_check_loop(struct envelope *ep)
{
	int fd;
	FILE *fp;
	char *buf, *lbuf;
	size_t len;
	struct mailaddr maddr;
	int ret = 0;
	int rcvcount = 0;

	fd = queue_message_fd_r(evpid_to_msgid(ep->id));
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
			struct mailaddr dest;

			bzero(&maddr, sizeof (struct mailaddr));
			if (! email_to_mailaddr(&maddr, buf + 14))
				continue;
			
			dest = ep->dest;
			if (ep->type == D_BOUNCE)
				dest = ep->sender;

			if (strcasecmp(maddr.user, dest.user) == 0 &&
			    strcasecmp(maddr.domain, dest.domain) == 0) {
				ret = 1;
				break;
			}
		}
	}
	free(lbuf);

	fclose(fp);
	return ret;
}

static void
scheduler_remove(u_int64_t id)
{
	void	*msg;

	/* removing by evpid */
	if (id > 0xffffffffL) {
		scheduler_remove_envelope(id);
		return;
	}

	/* removing by msgid */
	msg = backend->message(id);
	while (backend->fetch(msg, &id))
		scheduler_remove_envelope(id);
	backend->close(msg);
}

static void
scheduler_remove_envelope(u_int64_t evpid)
{
	struct envelope evp;

	evp.id = evpid;
	queue_envelope_delete(&evp);
	backend->remove(evpid);
}

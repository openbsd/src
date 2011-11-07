/*	$OpenBSD: runner.c,v 1.123 2011/11/07 11:14:10 eric Exp $	*/

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


static void runner_imsg(struct imsgev *, struct imsg *);
static void runner_shutdown(void);
static void runner_sig_handler(int, short, void *);
static void runner_setup_events(void);
static void runner_reset_events(void);
static void runner_disable_events(void);
static void runner_timeout(int, short, void *);
static int runner_process_envelope(struct ramqueue_envelope *, time_t);
static void runner_process_batch(struct ramqueue_envelope *, time_t);
static void runner_purge_run(void);
static void runner_purge_message(u_int32_t);
static int runner_check_loop(struct envelope *);
static int runner_force_message_to_ramqueue(struct ramqueue *, u_int32_t);


/*temporary*/
u_int64_t	filename_to_evpid(char *);
u_int32_t	filename_to_msgid(char *);


void
runner_imsg(struct imsgev *iev, struct imsg *imsg)
{
	struct envelope	*e;

	log_imsg(PROC_RUNNER, iev->proc, imsg);

	switch (imsg->hdr.type) {
	case IMSG_QUEUE_COMMIT_MESSAGE:
		e = imsg->data;
		runner_force_message_to_ramqueue(&env->sc_rqueue,
		    evpid_to_msgid(e->id));
		runner_reset_events();
		return;

	case IMSG_QUEUE_MESSAGE_UPDATE:
		e = imsg->data;
		e->retry++;
		stat_decrement(STATS_RUNNER);

		/* temporary failure, message remains in queue,
		 * gets reinserted in ramqueue
		 */
		if (e->status & DS_TEMPFAILURE) {
			log_debug("TEMPFAIL: %016" PRIx64, e->id);
			e->status &= ~DS_TEMPFAILURE;
			queue_envelope_update(Q_QUEUE, e);
			ramqueue_insert(&env->sc_rqueue, e, time(NULL));
			runner_reset_events();
			return;
		}

		/* permanent failure, eventually generate a
		 * bounce (and insert bounce in ramqueue).
		 */
		if (e->status & DS_PERMFAILURE) {
			struct envelope bounce;
			log_debug("PERMFAIL: %016" PRIx64, e->id);
			if (e->type != D_BOUNCE &&
			    e->sender.user[0] != '\0') {
				log_debug("PERMFAIL #2: %016" PRIx64, e->id);
				bounce_record_message(e, &bounce);
				ramqueue_insert(&env->sc_rqueue, &bounce, time(NULL));
				runner_reset_events();
			}
		}

		/* successful delivery or permanent failure,
		 * remove envelope from queue.
		 */
		log_debug("#### %s: queue_envelope_delete: %016" PRIx64,
		    __func__, e->id);
		queue_envelope_delete(Q_QUEUE, e);
		return;

	case IMSG_MDA_SESS_NEW:
		stat_decrement(STATS_MDA_SESSION);
		return;

	case IMSG_BATCH_DONE:
		stat_decrement(STATS_MTA_SESSION);
		return;

	case IMSG_SMTP_ENQUEUE:
		e = imsg->data;
		if (imsg->fd < 0 || !bounce_session(imsg->fd, e)) {
			e->status = 0;
			queue_envelope_update(Q_QUEUE, e);
			ramqueue_insert(&env->sc_rqueue, e, time(NULL));
			runner_reset_events();
			return;
		}
		return;

	case IMSG_QUEUE_PAUSE_MDA:
		env->sc_opts |= SMTPD_MDA_PAUSED;
		return;

	case IMSG_QUEUE_RESUME_MDA:
		env->sc_opts &= ~SMTPD_MDA_PAUSED;
		runner_reset_events();
		return;

	case IMSG_QUEUE_PAUSE_MTA:
		env->sc_opts |= SMTPD_MTA_PAUSED;
		return;

	case IMSG_QUEUE_RESUME_MTA:
		env->sc_opts &= ~SMTPD_MTA_PAUSED;
		runner_reset_events();
		return;

	case IMSG_CTL_VERBOSE:
		log_verbose(*(int *)imsg->data);
		return;

	case IMSG_RUNNER_SCHEDULE:
		runner_schedule(&env->sc_rqueue,
		    *(u_int64_t *)imsg->data);
		runner_reset_events();		
		return;

	case IMSG_RUNNER_REMOVE: {
		runner_remove(&env->sc_rqueue,
		    *(u_int64_t *)imsg->data);
		runner_reset_events();
		return;
	}
	}

	fatalx("runner_imsg: unexpected imsg");
}

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
runner_shutdown(void)
{
	log_info("runner handler exiting");
	_exit(0);
}

void
runner_setup_events(void)
{
	struct timeval	 tv;

	evtimer_set(&env->sc_ev, runner_timeout, NULL);
	tv.tv_sec = 0;
	tv.tv_usec = 10;
	evtimer_add(&env->sc_ev, &tv);
}

void
runner_reset_events(void)
{
	struct timeval	 tv;

	tv.tv_sec = 0;
	tv.tv_usec = 10;
	evtimer_add(&env->sc_ev, &tv);
}

void
runner_disable_events(void)
{
	evtimer_del(&env->sc_ev);
}

pid_t
runner(void)
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
		fatal("runner: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	purge_config(PURGE_EVERYTHING);

	pw = env->sc_pw;

	if (chroot(PATH_SPOOL) == -1)
		fatal("runner: chroot");
	if (chdir("/") == -1)
		fatal("runner: chdir(\"/\")");

	smtpd_process = PROC_RUNNER;
	setproctitle("%s", env->sc_title[smtpd_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("runner: cannot drop privileges");

	ramqueue_init(&env->sc_rqueue);

	imsg_callback = runner_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, runner_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, runner_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* see fdlimit()-related comment in queue.c */
	fdlimit(1.0);
	if ((env->sc_maxconn = availdesc() / 4) < 1)
		fatalx("runner: fd starvation");

	config_pipes(peers, nitems(peers));
	config_peers(peers, nitems(peers));

	unlink(PATH_QUEUE "/envelope.tmp");

	runner_setup_events();
	event_dispatch();
	runner_disable_events();
	runner_shutdown();

	return (0);
}

void
runner_timeout(int fd, short event, void *p)
{
	struct ramqueue		 *rqueue = &env->sc_rqueue;
	struct ramqueue_envelope *rq_evp;
	struct timeval		 tv;
	static int		 rq_done = 0;
	time_t			 nsched;
	time_t			 curtm;

	runner_purge_run();

	nsched = 0;
	rq_evp = ramqueue_first_envelope(rqueue);
	if (rq_evp)
		nsched = rq_evp->sched;

	/* load as many envelopes as possible from disk-queue to
	 * ram-queue until a schedulable envelope is found.
	 */
	if (! rq_done)
		rq_done = ramqueue_load(rqueue, &nsched);


	/* let's do the schedule dance baby ! */
	curtm = time(NULL);
	rq_evp = ramqueue_next_envelope(rqueue);
	while (rq_evp) {
		if (rq_evp->sched > curtm) {
			nsched = rq_evp->sched;
			break;
		}
		runner_process_envelope(rq_evp, curtm);
		rq_evp = ramqueue_next_envelope(rqueue);
	}

	if (rq_evp == NULL ||
	    (rq_done && ramqueue_is_empty(rqueue))) {
		log_debug("runner: nothing to schedule, wake me up. zZzZzZ");
		return;
	}

	/* disk-queues not fully loaded, no time for sleeping */
	if (!rq_done)
		nsched = 0;
	else {
		nsched = nsched - curtm;
		if (nsched < 0)
			nsched = 0;
	}

	log_debug("runner: nothing to do for the next %lld seconds, zZzZzZ",
	    (long long int) nsched);

	tv.tv_sec = nsched;
	tv.tv_usec = 0;
	evtimer_add(&env->sc_ev, &tv);
}

int
runner_process_envelope(struct ramqueue_envelope *rq_evp, time_t curtm)
{
	size_t		 mta_av, mda_av, bnc_av;
	struct envelope	 envelope;

	mta_av = env->sc_maxconn - stat_get(STATS_MTA_SESSION, STAT_ACTIVE);
	mda_av = env->sc_maxconn - stat_get(STATS_MDA_SESSION, STAT_ACTIVE);
	bnc_av = env->sc_maxconn - stat_get(STATS_RUNNER_BOUNCES, STAT_ACTIVE);
	
	if (rq_evp->rq_batch->type == D_MDA) {
		if (env->sc_opts & SMTPD_MDA_PAUSED)
			return 0;
		if (mda_av == 0)
			return 0;
	}

	if (rq_evp->rq_batch->type == D_MTA) {
		if (env->sc_opts & SMTPD_MTA_PAUSED)
			return 0;
		if (mta_av == 0)
			return 0;
	}

	if (rq_evp->rq_batch->type == D_BOUNCE) {
		if (env->sc_opts & (SMTPD_MDA_PAUSED|SMTPD_MTA_PAUSED))
			return 0;
		if (bnc_av == 0)
			return 0;
	}

	if (! queue_envelope_load(Q_QUEUE, rq_evp->evpid, &envelope))
		return 0;

	if (runner_check_loop(&envelope)) {
		struct envelope bounce;

		envelope_set_errormsg(&envelope, "loop has been detected");
		bounce_record_message(&envelope, &bounce);
		ramqueue_insert(&env->sc_rqueue, &bounce, time(NULL));
		runner_setup_events();
		log_debug("#### %s: queue_envelope_delete: %016" PRIx64,
		    __func__, envelope.id);
		queue_envelope_delete(Q_QUEUE, &envelope);
		return 0;
	}

	runner_process_batch(rq_evp, curtm);

	return 1;
}


void
runner_process_batch(struct ramqueue_envelope *rq_evp, time_t curtm)
{
	struct ramqueue_batch	 *rq_batch;
	struct ramqueue_message	 *rq_msg;
	struct ramqueue_host	 *rq_host;
	struct envelope evp;
	int fd;

	rq_msg = rq_evp->rq_msg;
	rq_batch = rq_evp->rq_batch;
	rq_host = rq_evp->rq_host;

	switch (rq_batch->type) {
	case D_BOUNCE:
		while ((rq_evp = ramqueue_batch_first_envelope(rq_batch))) {
			if (! queue_envelope_load(Q_QUEUE, rq_evp->evpid,
				&evp))
				return;
			evp.lasttry = curtm;
			imsg_compose_event(env->sc_ievs[PROC_QUEUE],
			    IMSG_SMTP_ENQUEUE, PROC_SMTP, 0, -1, &evp,
			    sizeof evp);
			ramqueue_remove_envelope(&env->sc_rqueue, rq_evp);
		}

		stat_increment(STATS_RUNNER);
		stat_increment(STATS_RUNNER_BOUNCES);

		break;
		
	case D_MDA:
		rq_evp = ramqueue_batch_first_envelope(rq_batch);
		if (! queue_envelope_load(Q_QUEUE, rq_evp->evpid, &evp))
			return;
		evp.lasttry = curtm;
		fd = queue_message_fd_r(Q_QUEUE,
		    evpid_to_msgid(rq_evp->evpid));
		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_MDA_SESS_NEW, PROC_MDA, 0, fd, &evp,
		    sizeof evp);
		ramqueue_remove_envelope(&env->sc_rqueue, rq_evp);

		stat_increment(STATS_RUNNER);
		stat_increment(STATS_MDA_SESSION);

		break;
		
	case D_MTA:
		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_BATCH_CREATE, PROC_MTA, 0, -1, rq_batch,
		    sizeof *rq_batch);
		while ((rq_evp = ramqueue_batch_first_envelope(rq_batch))) {
			if (! queue_envelope_load(Q_QUEUE, rq_evp->evpid,
				&evp))
				return;
			evp.lasttry = curtm;
			evp.batch_id = rq_batch->b_id;
			imsg_compose_event(env->sc_ievs[PROC_QUEUE],
			    IMSG_BATCH_APPEND, PROC_MTA, 0, -1, &evp,
			    sizeof evp);
			ramqueue_remove_envelope(&env->sc_rqueue, rq_evp);
			stat_increment(STATS_RUNNER);
		}
		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_BATCH_CLOSE, PROC_MTA, 0, -1, rq_batch,
		    sizeof *rq_batch);

		stat_increment(STATS_MTA_SESSION);
		break;
		
	default:
		fatalx("runner_process_batchqueue: unknown type");
	}

	if (ramqueue_message_is_empty(rq_msg))
		ramqueue_remove_message(&env->sc_rqueue, rq_msg);

	if (ramqueue_batch_is_empty(rq_batch))
		ramqueue_remove_batch(rq_host, rq_batch);

	if (ramqueue_host_is_empty(rq_host))
		ramqueue_remove_host(&env->sc_rqueue, rq_host);
}

/* XXX - temporary solution */
int
runner_force_message_to_ramqueue(struct ramqueue *rqueue, u_int32_t msgid)
{
	char path[MAXPATHLEN];
	DIR *dirp;
	struct dirent *dp;
	struct envelope envelope;
	time_t curtm;

	if (! bsnprintf(path, MAXPATHLEN, "%s/%03x/%08x/envelopes",
		PATH_QUEUE, msgid & 0xfff, msgid))
		return 0;

	dirp = opendir(path);
	if (dirp == NULL)
		return 0;

	curtm = time(NULL);
	while ((dp = readdir(dirp)) != NULL) {
		u_int64_t evpid;

		if (dp->d_name[0] == '.')
			continue;

		if ((evpid = filename_to_evpid(dp->d_name)) == 0) {
			log_warnx("runner_force_message_to_ramqueue: "
				  "invalid evpid: %016" PRIx64, evpid);
			continue;
		}

		if (! queue_envelope_load(Q_QUEUE, evpid,
			&envelope))
			continue;
		ramqueue_insert(rqueue, &envelope, curtm);
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

	while (qwalk(q, path)) {
		u_int32_t msgid;
		char *bpath;
		
		bpath = basename(path);
		if (bpath[0] == '.')
			continue;

		if ((msgid = filename_to_msgid(bpath)) == 0) {
			log_warnx("runner_purge_run: invalid msgid: %08x", msgid);
			continue;
		}
		runner_purge_message(msgid);
	}

	qwalk_close(q);
}

void
runner_purge_message(u_int32_t msgid)
{
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];
	char evppath[MAXPATHLEN];
	char msgpath[MAXPATHLEN];
	DIR *dirp;
	struct dirent *dp;
	
	if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%08x", PATH_PURGE, msgid))
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

int
runner_check_loop(struct envelope *ep)
{
	int fd;
	FILE *fp;
	char *buf, *lbuf;
	size_t len;
	struct mailaddr maddr;
	int ret = 0;
	int rcvcount = 0;

	fd = queue_message_fd_r(Q_QUEUE,
	    evpid_to_msgid(ep->id));
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

void
runner_schedule(struct ramqueue *rq, u_int64_t id)
{
	ramqueue_schedule(rq, id);
}


void
runner_remove(struct ramqueue *rq, u_int64_t id)
{
	struct ramqueue_message *rq_msg;
	struct ramqueue_envelope *rq_evp;

	/* removing by evpid */
	if (id > 0xffffffffL) {
		rq_evp = ramqueue_lookup_envelope(rq, id);
		if (rq_evp == NULL)
			return;
		runner_remove_envelope(rq, rq_evp);
		return;
	}

	rq_msg = ramqueue_lookup_message(rq, id);
	if (rq_msg == NULL)
		return;

	/* scheduling by msgid */
	RB_FOREACH(rq_evp, evptree, &rq_msg->evptree) {
		runner_remove_envelope(rq, rq_evp);
	}
}

void
runner_remove_envelope(struct ramqueue *rq, struct ramqueue_envelope *rq_evp)
{
	struct envelope evp;

	if (queue_envelope_load(Q_QUEUE, rq_evp->evpid, &evp)) {
		log_debug("#### %s: queue_envelope_delete: %016" PRIx64,
		    __func__, evp.id);
		queue_envelope_delete(Q_QUEUE, &evp);
	}

	ramqueue_remove_envelope(rq, rq_evp);
}

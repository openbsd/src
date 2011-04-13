/*	$OpenBSD: runner.c,v 1.96 2011/04/13 20:53:18 gilles Exp $	*/

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
#include <libgen.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

void		runner_imsg(struct smtpd *, struct imsgev *, struct imsg *);

__dead void	runner_shutdown(void);
void		runner_sig_handler(int, short, void *);
void		runner_setup_events(struct smtpd *);
void		runner_disable_events(struct smtpd *);

void		runner_timeout(int, short, void *);

int		runner_process_envelope(struct smtpd *, struct ramqueue_envelope *, time_t);
void		runner_process_batch(struct smtpd *, struct ramqueue_envelope *, time_t);

void		runner_purge_run(void);
void		runner_purge_message(char *);

int		runner_check_loop(struct message *);

int		runner_force_message_to_ramqueue(struct ramqueue *, char *);

void		ramqueue_insert(struct ramqueue *, struct message *, time_t);

void
runner_imsg(struct smtpd *env, struct imsgev *iev, struct imsg *imsg)
{
	struct message	*m;

	switch (imsg->hdr.type) {
	case IMSG_QUEUE_COMMIT_MESSAGE:
		m = imsg->data;
		runner_force_message_to_ramqueue(&env->sc_rqueue, m->message_id);
		runner_setup_events(env);
		return;

	case IMSG_QUEUE_MESSAGE_UPDATE:
		m = imsg->data;
		m->retry++;
		env->stats->runner.active--;

		/* temporary failure, message remains in queue,
		 * gets reinserted in ramqueue
		 */
		if (m->status & S_MESSAGE_TEMPFAILURE) {
			m->status &= ~S_MESSAGE_TEMPFAILURE;
			queue_update_envelope(m);
			ramqueue_insert(&env->sc_rqueue, m, time(NULL));
			runner_setup_events(env);
			return;
		}

		/* permanent failure, eventually generate a
		 * bounce (and insert bounce in ramqueue).
		 */
		if (m->status & S_MESSAGE_PERMFAILURE) {
			struct message bounce;

			if (m->type != T_BOUNCE_MESSAGE &&
			    m->sender.user[0] != '\0') {
				bounce_record_message(m, &bounce);
				ramqueue_insert(&env->sc_rqueue, &bounce, time(NULL));
				runner_setup_events(env);
			}
		}

		/* successful delivery or permanent failure,
		 * remove envelope from queue.
		 */
		queue_remove_envelope(m);
		return;

	case IMSG_MDA_SESS_NEW:
		env->stats->mda.sessions_active--;
		return;

	case IMSG_BATCH_DONE:
		env->stats->mta.sessions_active--;
		return;

	case IMSG_PARENT_ENQUEUE_OFFLINE:
		/*		runner_process_offline(env);*/
		return;

	case IMSG_SMTP_ENQUEUE:
		m = imsg->data;
		if (imsg->fd < 0 || !bounce_session(env, imsg->fd, m)) {
			m->status = 0;
			queue_update_envelope(m);
			ramqueue_insert(&env->sc_rqueue, m, time(NULL));
			runner_setup_events(env);
			return;
		}
		return;

	case IMSG_QUEUE_PAUSE_LOCAL:
		env->sc_opts |= SMTPD_MDA_PAUSED;
		return;

	case IMSG_QUEUE_RESUME_LOCAL:
		env->sc_opts &= ~SMTPD_MDA_PAUSED;
		return;

	case IMSG_QUEUE_PAUSE_OUTGOING:
		env->sc_opts |= SMTPD_MTA_PAUSED;
		return;

	case IMSG_QUEUE_RESUME_OUTGOING:
		env->sc_opts &= ~SMTPD_MTA_PAUSED;
		return;

	case IMSG_CTL_VERBOSE:
		log_verbose(*(int *)imsg->data);
		return;
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

	purge_config(env, PURGE_EVERYTHING);

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

	ramqueue_init(env, &env->sc_rqueue);

	imsg_callback = runner_imsg;
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

	runner_setup_events(env);
	event_dispatch();
	runner_shutdown();

	return (0);
}

void
runner_timeout(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct ramqueue		 *rqueue = &env->sc_rqueue;
	struct ramqueue_envelope *rq_evp;
	struct timeval		 tv;
	static int		 rq_done = 0;
	static int		 rq_off_done = 0;
	time_t			 nsched;
	time_t			 curtm;

	runner_purge_run();

	nsched = 0;
	rq_evp = ramqueue_first_envelope(rqueue);
	if (rq_evp)
		nsched = rq_evp->sched;


	/* fetch one offline message at a time to prevent a huge
	 * offline queue from hogging the deliveries of incoming
	 * messages.
	 */
	if (! rq_off_done)
		rq_off_done = ramqueue_load_offline(rqueue);


	/* load as many envelopes as possible from disk-queue to
	 * ram-queue until a schedulable envelope is found.
	 */
	if (! rq_done)
		rq_done = ramqueue_load(rqueue, &nsched);


	/* let's do the schedule dance baby ! */
	curtm = time(NULL);
	rq_evp = ramqueue_next_envelope(rqueue);
	while (rq_evp) {
		if (rq_evp->sched > curtm)
			break;
		runner_process_envelope(env, rq_evp, curtm);
		rq_evp = ramqueue_next_envelope(rqueue);
	}

	if (rq_done && rq_off_done && ramqueue_is_empty(rqueue)) {
		log_debug("runner: ramqueue is empty, wake me up. zZzZzZ");
		return;
	}

	/* disk-queues not fully loaded, no time for sleeping */
	if (!rq_done || !rq_off_done)
		nsched = 0;
	else {
		nsched = nsched - curtm;
		if (nsched < 0)
			nsched = 0;
	}

	log_debug("runner: nothing to do for the next %d seconds, zZzZzZ",
	    nsched);

	tv.tv_sec = nsched;
	tv.tv_usec = 0;
	evtimer_add(&env->sc_ev, &tv);
}

int
runner_process_envelope(struct smtpd *env, struct ramqueue_envelope *rq_evp, time_t curtm)
{
	size_t		 mta_av, mda_av, bnc_av;
	struct message	 envelope;

	mta_av = env->sc_maxconn - env->stats->mta.sessions_active;
	mda_av = env->sc_maxconn - env->stats->mda.sessions_active;
	bnc_av = env->sc_maxconn - env->stats->runner.bounces_active;
	
	if (! queue_load_envelope(&envelope, rq_evp->id))
		return 0;

	if (envelope.type & T_MDA_MESSAGE) {
		if (env->sc_opts & SMTPD_MDA_PAUSED)
			return 0;
		if (mda_av == 0)
			return 0;
	}

	if (envelope.type & T_MTA_MESSAGE) {
		if (env->sc_opts & SMTPD_MTA_PAUSED)
			return 0;
		if (mta_av == 0)
			return 0;
	}

	if (envelope.type & T_BOUNCE_MESSAGE) {
		if (env->sc_opts & (SMTPD_MDA_PAUSED|SMTPD_MTA_PAUSED))
			return 0;
		if (bnc_av == 0)
			return 0;
	}

	if (runner_check_loop(&envelope)) {
		struct message bounce;

		message_set_errormsg(&envelope, "loop has been detected");
		bounce_record_message(&envelope, &bounce);
		ramqueue_insert(&env->sc_rqueue, &bounce, time(NULL));
		runner_setup_events(env);
		queue_remove_envelope(&envelope);
		return 0;
	}

	log_debug("dispatching host: %p, batch: %p, envelope: %p, %s",
	    rq_evp->host,
	    rq_evp->batch,
	    rq_evp, rq_evp->id);
	runner_process_batch(env, rq_evp, curtm);

	return 1;
}


void
runner_process_batch(struct smtpd *env, struct ramqueue_envelope *rq_evp, time_t curtm)
{
	struct ramqueue_host	 *host = rq_evp->host;
	struct ramqueue_batch	 *batch = rq_evp->batch;
	struct message envelope;
	int fd;

	switch (batch->type) {
	case T_BOUNCE_MESSAGE:		
		while ((rq_evp = ramqueue_batch_first_envelope(batch))) {
			if (! queue_load_envelope(&envelope, rq_evp->id))
				return;
			envelope.lasttry = curtm;
			imsg_compose_event(env->sc_ievs[PROC_QUEUE],
			    IMSG_SMTP_ENQUEUE, PROC_SMTP, 0, -1, &envelope,
			    sizeof envelope);
			ramqueue_remove(&env->sc_rqueue, rq_evp);
			free(rq_evp);
		}
		env->stats->runner.bounces_active++;
		env->stats->runner.bounces++;
		SET_IF_GREATER(env->stats->runner.bounces_active,
		    env->stats->runner.bounces_maxactive);
		env->stats->runner.active++;
		SET_IF_GREATER(env->stats->runner.active,
		    env->stats->runner.maxactive);
		break;
		
	case T_MDA_MESSAGE:

		rq_evp = ramqueue_batch_first_envelope(batch);
		if (! queue_load_envelope(&envelope, rq_evp->id))
			return;
		envelope.lasttry = curtm;
		fd = queue_open_message_file(envelope.message_id);
		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_MDA_SESS_NEW, PROC_MDA, 0, fd, &envelope,
		    sizeof envelope);
		ramqueue_remove(&env->sc_rqueue, rq_evp);
		free(rq_evp);

		env->stats->mda.sessions_active++;
		env->stats->mda.sessions++;
		SET_IF_GREATER(env->stats->mda.sessions_active,
		    env->stats->mda.sessions_maxactive);
		env->stats->runner.active++;
		SET_IF_GREATER(env->stats->runner.active,
		    env->stats->runner.maxactive);
		break;
		
	case T_MTA_MESSAGE:
		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_BATCH_CREATE, PROC_MTA, 0, -1, batch,
		    sizeof *batch);
		while ((rq_evp = ramqueue_batch_first_envelope(batch))) {
			if (! queue_load_envelope(&envelope, rq_evp->id))
				return;
			envelope.lasttry = curtm;
			envelope.batch_id = batch->b_id;
			imsg_compose_event(env->sc_ievs[PROC_QUEUE],
			    IMSG_BATCH_APPEND, PROC_MTA, 0, -1, &envelope,
			    sizeof envelope);
			ramqueue_remove(&env->sc_rqueue, rq_evp);
			free(rq_evp);
			env->stats->runner.active++;
			SET_IF_GREATER(env->stats->runner.active,
			    env->stats->runner.maxactive);
		}
		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_BATCH_CLOSE, PROC_MTA, 0, -1, batch,
		    sizeof *batch);
		env->stats->mta.sessions_active++;
		env->stats->mta.sessions++;
		SET_IF_GREATER(env->stats->mta.sessions_active,
		    env->stats->mta.sessions_maxactive);
		break;
		
	default:
		fatalx("runner_process_batchqueue: unknown type");
	}

	if (ramqueue_batch_is_empty(batch)) {
		ramqueue_remove_batch(host, batch);
		free(batch);
		env->stats->ramqueue.batches--;
		
	}

	if (ramqueue_host_is_empty(host)) {
		ramqueue_remove_host(&env->sc_rqueue, host);
		free(host);
		env->stats->ramqueue.hosts--;
	}
}

/* XXX - temporary solution */
int
runner_force_message_to_ramqueue(struct ramqueue *rqueue, char *mid)
{
	char path[MAXPATHLEN];
	DIR *dirp;
	struct dirent *dp;
	struct message envelope;
	time_t curtm;

	if (! bsnprintf(path, MAXPATHLEN, "%s/%d/%s/envelopes",
		PATH_QUEUE, queue_hash(mid), mid))
		return 0;


	dirp = opendir(path);
	if (dirp == NULL)
		return 0;

	curtm = time(NULL);
	while ((dp = readdir(dirp)) != NULL) {
		if (valid_message_uid(dp->d_name)) {
			if (! queue_load_envelope(&envelope, dp->d_name))
				continue;
			ramqueue_insert(rqueue, &envelope, curtm);
		}
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

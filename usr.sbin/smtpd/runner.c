/*	$OpenBSD: runner.c,v 1.39 2009/04/12 15:42:13 gilles Exp $	*/

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
void	        runner_dispatch_control(int, short, void *);
void	        runner_dispatch_queue(int, short, void *);
void	        runner_dispatch_mda(int, short, void *);
void		runner_dispatch_mta(int, short, void *);
void		runner_dispatch_lka(int, short, void *);
void		runner_setup_events(struct smtpd *);
void		runner_disable_events(struct smtpd *);

void		runner_reset_flags(void);

void		runner_timeout(int, short, void *);

void		runner_process_queue(struct smtpd *);
void		runner_process_runqueue(struct smtpd *);
void		runner_process_batchqueue(struct smtpd *);

void		runner_batch_dispatch(struct smtpd *, struct batch *, time_t);

int		runner_message_schedule(struct message *, time_t);

void		runner_purge_run(void);
void		runner_purge_message(char *);

int		runner_check_loop(struct message *);

struct batch	*batch_record(struct smtpd *, struct message *);
struct batch	*batch_lookup(struct smtpd *, struct message *);

int		runner_force_envelope_schedule(char *);
int		runner_force_message_schedule(char *);

struct s_runner	 s_runner;

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
runner_dispatch_control(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_CONTROL];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("runner_dispatch_control: imsg_read error");
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
		case IMSG_STATS: {
			struct stats *s;

			s = imsg.data;
			s->u.runner = s_runner;
			imsg_compose(ibuf, IMSG_STATS, 0, 0, -1, s, sizeof(*s));
			break;
		}
		case IMSG_RUNNER_SCHEDULE: {
			struct sched *s;

			s = imsg.data;

			s->ret = 0;
			if (valid_message_uid(s->mid))
				s->ret = runner_force_envelope_schedule(s->mid);
			else if (valid_message_id(s->mid))
				s->ret = runner_force_message_schedule(s->mid);

			imsg_compose(ibuf, IMSG_RUNNER_SCHEDULE, 0, 0, -1, s, sizeof(*s));
			break;
		}
		default:
			log_warnx("runner_dispatch_control: got imsg %d",
			    imsg.hdr.type);
			fatalx("runner_dispatch_control: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
runner_dispatch_queue(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_QUEUE];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("runner_dispatch_queue: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_RUNNER_UPDATE_ENVELOPE: {
			s_runner.active--;
			queue_message_update(imsg.data);
			break;
		}
		default:
			log_warnx("runner_dispatch_queue: got imsg %d",
			    imsg.hdr.type);
			fatalx("runner_dispatch_queue: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
runner_dispatch_mda(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_MDA];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("runner_dispatch_mda: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_warnx("runner_dispatch_mda: got imsg %d",
			    imsg.hdr.type);
			fatalx("runner_dispatch_mda: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
runner_dispatch_mta(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_MTA];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("runner_dispatch_mta: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {

		default:
			log_warnx("runner_dispatch_mta: got imsg %d",
			    imsg.hdr.type);
			fatalx("runner_dispatch_mta: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
runner_dispatch_lka(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_LKA];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("runner_dispatch_lka: imsg_read error");
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
	imsg_event_add(ibuf);
}

void
runner_shutdown(void)
{
	log_info("runner handler");
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
		{ PROC_CONTROL,	runner_dispatch_control },
		{ PROC_MDA,	runner_dispatch_mda },
		{ PROC_MTA,	runner_dispatch_mta },
		{ PROC_QUEUE,	runner_dispatch_queue },
		{ PROC_LKA,	runner_dispatch_lka },
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

	setproctitle("runner");
	smtpd_process = PROC_RUNNER;

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

	config_pipes(env, peers, 5);
	config_peers(env, peers, 5);

	unlink(PATH_QUEUE "/envelope.tmp");
	runner_reset_flags();

	runner_setup_events(env);
	event_dispatch();
	runner_shutdown();

	return (0);
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

		message.flags &= ~F_MESSAGE_SCHEDULED;
		message.flags &= ~F_MESSAGE_PROCESSING;

		while (! queue_update_envelope(&message))
			sleep(1);
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
	struct qwalk	*q;

	now = time(NULL);
	q = qwalk_new(PATH_QUEUE);

	while (qwalk(q, path)) {
		if (! queue_load_envelope(&message, basename(path)))
			continue;

		if (message.type & T_MDA_MESSAGE)
			if (env->sc_opts & SMTPD_MDA_PAUSED)
				continue;

		if (message.type & T_MTA_MESSAGE)
			if (env->sc_opts & SMTPD_MTA_PAUSED)
				continue;

		if (! runner_message_schedule(&message, now))
			continue;

		if (runner_check_loop(&message)) {
			log_debug("TODO: generate mailer daemon");
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
	time_t curtime;
	struct batch *batchp, *nxt;

	curtime = time(NULL);
	for (batchp = SPLAY_MIN(batchtree, &env->batch_queue);
	     batchp != NULL;
	     batchp = nxt) {
		nxt = SPLAY_NEXT(batchtree, &env->batch_queue, batchp);

		runner_batch_dispatch(env, batchp, curtime);

		SPLAY_REMOVE(batchtree, &env->batch_queue, batchp);
		bzero(batchp, sizeof(struct batch));
		free(batchp);
	}
}

void
runner_batch_dispatch(struct smtpd *env, struct batch *batchp, time_t curtime)
{
	u_int8_t proctype;
	struct message *messagep;

	if ((batchp->type & (T_MDA_BATCH|T_MTA_BATCH)) == 0)
		fatal("runner_batch_dispatch: unknown batch type");

	if (batchp->type & T_MDA_BATCH)
		proctype = PROC_MDA;
	else if (batchp->type & T_MTA_BATCH)
		proctype = PROC_MTA;

	imsg_compose(env->sc_ibufs[proctype], IMSG_BATCH_CREATE, 0, 0, -1,
	    batchp, sizeof (struct batch));

	while ((messagep = TAILQ_FIRST(&batchp->messages))) {
		imsg_compose(env->sc_ibufs[proctype], IMSG_BATCH_APPEND, 0, 0,
		    -1, messagep, sizeof (struct message));
		TAILQ_REMOVE(&batchp->messages, messagep, entry);
		bzero(messagep, sizeof(struct message));
		free(messagep);
	}

	imsg_compose(env->sc_ibufs[proctype], IMSG_BATCH_CLOSE, 0, 0, -1,
	    batchp, sizeof(struct batch));
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
		queue_remove_envelope(messagep);
		return 0;
	}

	if (messagep->lasttry == 0)
		return 1;

	delay = SMTPD_QUEUE_MAXINTERVAL;

	if (messagep->type & T_MDA_MESSAGE) {
		if (messagep->status & S_MESSAGE_LOCKFAILURE) {
			if (messagep->retry < 128)
				return 1;
			delay = (messagep->retry * 60) + arc4random() % 60;
		}
		else {
			if (messagep->retry < 5)
				return 1;
			
			if (messagep->retry < 15)
				delay = (messagep->retry * 60) + arc4random() % 60;
		}
	}

	if (messagep->type & T_MTA_MESSAGE) {
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
		fatal("queue_delete_incoming_message: snprintf");

	if (! bsnprintf(evpdir, sizeof(evpdir), "%s%s", rootdir,
		PATH_ENVELOPES))
		fatal("queue_delete_incoming_message: snprintf");
	
	if (! bsnprintf(msgpath, sizeof(msgpath), "%s/message", rootdir))
		fatal("queue_delete_incoming_message: snprintf");

	if (unlink(msgpath) == -1)
		if (errno != ENOENT)
			fatal("queue_delete_incoming_message: unlink");

	dirp = opendir(evpdir);
	if (dirp == NULL) {
		if (errno == ENOENT)
			goto delroot;
		fatal("queue_delete_incoming_message: opendir");
	}
	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;
		if (! bsnprintf(evppath, sizeof(evppath), "%s/%s", evpdir,
			dp->d_name))
			fatal("queue_delete_incoming_message: snprintf");

		if (unlink(evppath) == -1)
			if (errno != ENOENT)
				fatal("queue_delete_incoming_message: unlink");
	}
	closedir(dirp);

	if (rmdir(evpdir) == -1)
		if (errno != ENOENT)
			fatal("queue_delete_incoming_message: rmdir");

delroot:
	if (rmdir(rootdir) == -1)
		if (errno != ENOENT)
			fatal("queue_delete_incoming_message: rmdir");
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

		batchp->id = queue_generate_id();
		batchp->creation = messagep->creation;

		(void)strlcpy(batchp->message_id, messagep->message_id,
		    sizeof(batchp->message_id));
		TAILQ_INIT(&batchp->messages);
		SPLAY_INSERT(batchtree, &env->batch_queue, batchp);

		if (messagep->type & T_DAEMON_MESSAGE) {
			batchp->type = T_DAEMON_BATCH;
			path = &messagep->sender;
		}
		else {
			path = &messagep->recipient;
		}
		batchp->rule = path->rule;

		(void)strlcpy(batchp->hostname, path->domain,
		    sizeof(batchp->hostname));

		if (IS_MAILBOX(path->rule.r_action) ||
		    IS_EXT(path->rule.r_action)) {
			batchp->type |= T_MDA_BATCH;
		}
		else {
			batchp->type |= T_MTA_BATCH;
		}
	}

	TAILQ_INSERT_TAIL(&batchp->messages, messagep, entry);
	s_runner.active++;
	return batchp;
}

struct batch *
batch_lookup(struct smtpd *env, struct message *message)
{
	struct batch *batchp;
	struct batch lookup;

	/* We only support delivery of one message at a time, in MDA */
	if (message->type & T_MDA_MESSAGE)
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

		if (strchr(buf, ':') == NULL && !isspace(*buf))
			break;

		if (strncasecmp("Received: ", buf, 10) == 0) {
			rcvcount++;
			if (rcvcount == MAX_HOPS_COUNT) {
				log_debug("LOOP DETECTED THROUGH RECEIVED LINES COUNT");
				ret = 1;
				break;
			}
		}

		else if (strncasecmp("X-OpenSMTPD-Loop: ", buf, 18) == 0) {
			bzero(&chkpath, sizeof (struct path));
			if (! recipient_to_path(&chkpath, buf + 18))
				continue;

			if (strcasecmp(chkpath.user, messagep->recipient.user) == 0 &&
			    strcasecmp(chkpath.domain, messagep->recipient.domain) == 0) {
				log_debug("LOOP DETECTED THROUGH X-OPENSMTPD-LOOP HEADER: %s@%s",
				    chkpath.user, chkpath.domain);
				ret = 1;
				break;
			}
		}
	}
	free(lbuf);

	fclose(fp);
	return ret;
}

SPLAY_GENERATE(batchtree, batch, b_nodes, batch_cmp);

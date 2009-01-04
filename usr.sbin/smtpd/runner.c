/*	$OpenBSD: runner.c,v 1.15 2009/01/04 19:26:30 jacekm Exp $	*/

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

#include <dirent.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
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
void		runner_timeout(int, short, void *);

void		runner_process_queue(struct smtpd *);
void		runner_process_bucket(struct smtpd *, u_int16_t);
void		runner_process_message(struct smtpd *, char *);
void		runner_process_envelope(struct smtpd *, char *, char *);
void		runner_process_runqueue(struct smtpd *);
void		runner_process_batchqueue(struct smtpd *);

int		runner_batch_resolved(struct smtpd *, struct batch *);
void		runner_batch_dispatch(struct smtpd *, struct batch *, time_t);

int		runner_message_schedule(struct message *, time_t);

void		runner_purge_run(void);
void		runner_purge_message(char *);

struct batch	*batch_record(struct smtpd *, struct message *);
struct batch	*batch_lookup(struct smtpd *, struct message *);

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
		default:
			log_debug("queue_dispatch_control: unexpected imsg %d",
			    imsg.hdr.type);
			break;
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
		default:
			log_debug("runner_dispatch_queue: unexpected imsg %d",
			    imsg.hdr.type);
			break;
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
			log_debug("runner_dispatch_mda: unexpected imsg %d",
			    imsg.hdr.type);
			break;
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
			log_debug("runner_dispatch_mta: unexpected imsg %d",
			    imsg.hdr.type);
			break;
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
		case IMSG_LKA_MX: {
			runner_batch_resolved(env, imsg.data);
			break;
		}
		default:
			log_debug("runner_dispatch_lka: unexpected imsg %d",
			    imsg.hdr.type);
			break;
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

	config_peers(env, peers, 5);

	runner_setup_events(env);
	event_dispatch();
	runner_shutdown();

	return (0);
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
	DIR *dirp;
	struct dirent *dp;
	const char *errstr;
	u_int16_t bucket;

	dirp = opendir(PATH_QUEUE);
	if (dirp == NULL)
		fatal("runner_process_queue: opendir");
	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;
		bucket = strtonum(dp->d_name, 0, DIRHASH_BUCKETS - 1, &errstr);
		if (errstr) {
			log_warnx("runner_process_queue: invalid bucket: %s/%s",
			    PATH_QUEUE, dp->d_name);
			continue;
		}
		runner_process_bucket(env, bucket);
	}
	closedir(dirp);
}

void
runner_process_bucket(struct smtpd *env, u_int16_t bucket)
{
	DIR *dirp = NULL;
	struct dirent *dp;
	char bucketpath[MAXPATHLEN];

	if (! bsnprintf(bucketpath, MAXPATHLEN, "%s/%d", PATH_QUEUE, bucket))
		fatal("runner_process_bucket: snprintf");
	dirp = opendir(bucketpath);
	if (dirp == NULL)
		return;
	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;
		runner_process_message(env, dp->d_name);
	}
	closedir(dirp);
}

void
runner_process_message(struct smtpd *env, char *messageid)
{
	DIR *dirp = NULL;
	struct dirent *dp;
	char evppath[MAXPATHLEN];
	u_int16_t hval;

	hval = queue_hash(messageid);
	if (! bsnprintf(evppath, MAXPATHLEN, "%s/%d/%s%s", PATH_QUEUE, hval,
		messageid, PATH_ENVELOPES))
		fatal("runner_process_message: snprintf");
	dirp = opendir(evppath);
	if (dirp == NULL)
		return;
	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;
		runner_process_envelope(env, dp->d_name, dp->d_name);
	}
	closedir(dirp);
}

void
runner_process_envelope(struct smtpd *env, char *msgid, char *evpid)
{
	struct message message;
	time_t tm;
	char evppath[MAXPATHLEN];
	char rqpath[MAXPATHLEN];
	u_int16_t hval;
	struct stat sb;

	if (! queue_load_envelope(&message, evpid))
		return;

	tm = time(NULL);

	if (! runner_message_schedule(&message, tm))
		return;

	message.flags |= F_MESSAGE_SCHEDULED;
	queue_update_envelope(&message);

	hval = queue_hash(msgid);
	if (! bsnprintf(evppath, MAXPATHLEN, "%s/%d/%s%s/%s", PATH_QUEUE, hval,
		msgid, PATH_ENVELOPES, evpid))
		fatal("runner_process_envelope: snprintf");

	if (! bsnprintf(rqpath, MAXPATHLEN, "%s/%s", PATH_RUNQUEUE, evpid))
		fatal("runner_process_envelope: snprintf");

	if (stat(rqpath, &sb) == -1) {
		if (errno != ENOENT)
			fatal("runner_process_envelope: stat");

		if (symlink(evppath, rqpath) == -1) {
			log_info("queue_process_envelope: "
			    "failed to place envelope in runqueue");
		}
	}
}

void
runner_process_runqueue(struct smtpd *env)
{
	DIR *dirp;
	struct dirent *dp;
	struct message message;
	struct message *messagep;
	struct batch *batchp;
	char pathname[MAXPATHLEN];
	time_t tm;

	tm = time(NULL);

	dirp = opendir(PATH_RUNQUEUE);
	if (dirp == NULL)
		fatal("runner_process_runqueue: opendir");

	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;

		snprintf(pathname, MAXPATHLEN, "%s/%s", PATH_RUNQUEUE,
		    dp->d_name);
		unlink(pathname);

		if (! queue_load_envelope(&message, dp->d_name))
			continue;

		if (message.flags & F_MESSAGE_PROCESSING)
			continue;

		message.lasttry = tm;
		message.flags &= ~F_MESSAGE_SCHEDULED;
		message.flags |= F_MESSAGE_PROCESSING;
		queue_update_envelope(&message);

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

	closedir(dirp);
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
		if ((batchp->type & T_MTA_BATCH) &&
		    (batchp->flags & F_BATCH_RESOLVED) == 0) {
			continue;
		}

		runner_batch_dispatch(env, batchp, curtime);

		SPLAY_REMOVE(batchtree, &env->batch_queue, batchp);
		bzero(batchp, sizeof(struct batch));
		free(batchp);
	}
}

int
runner_batch_resolved(struct smtpd *env, struct batch *lookup)
{
	u_int32_t i;
	struct batch *batchp;

	batchp = batch_by_id(env, lookup->id);
	batchp->getaddrinfo_error = lookup->getaddrinfo_error;
	batchp->mx_cnt = lookup->mx_cnt;

/*
           EAI_NODATA        no address associated with hostname
           EAI_NONAME        hostname or servname not provided, or not known
           EAI_PROTOCOL      resolved protocol is unknown
           EAI_SERVICE       servname not supported for ai_socktype
           EAI_SOCKTYPE      ai_socktype not supported
           EAI_SYSTEM        system error returned in errno


 */

	switch (batchp->getaddrinfo_error) {
	case 0:
		batchp->flags |= F_BATCH_RESOLVED;
		for (i = 0; i < batchp->mx_cnt; ++i)
			batchp->mxarray[i].ss = lookup->mxarray[i].ss;
		break;
	case EAI_ADDRFAMILY:
	case EAI_BADFLAGS:
	case EAI_BADHINTS:
	case EAI_FAIL:
	case EAI_FAMILY:
	case EAI_NODATA:
	case EAI_NONAME:
	case EAI_SERVICE:
	case EAI_SOCKTYPE:
	case EAI_SYSTEM:
		/* XXX */
		/*
		 * In the case of a DNS permanent error, do not generate a
		 * daemon message if the error originates from one already
		 * as this would cause a loop. Remove the initial batch as
		 * it will never succeed.
		 *
		 */
		return 0;

	case EAI_AGAIN:
	case EAI_MEMORY:
		/* XXX */
		/*
		 * Do not generate a daemon message if this error happened
		 * while processing a daemon message. Do NOT remove batch,
		 * it may succeed later.
		 */
		return 0;

	default:
		fatalx("runner_batch_resolved: unknown getaddrinfo error.");
	}
	return 1;
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

void
runner_purge_run(void)
{
	DIR		*dirp;
	struct dirent	*dp;

	dirp = opendir(PATH_PURGE);
	if (dirp == NULL)
		fatal("runner_purge_run: opendir");

	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0) {
			continue;
		}
		runner_purge_message(dp->d_name);
	}

	closedir(dirp);
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
	
	if (! bsnprintf(rootdir, MAXPATHLEN, "%s/%s", PATH_PURGE, msgid))
		fatal("queue_delete_incoming_message: snprintf");

	if (! bsnprintf(evpdir, MAXPATHLEN, "%s%s", rootdir, PATH_ENVELOPES))
		fatal("queue_delete_incoming_message: snprintf");
	
	if (! bsnprintf(msgpath, MAXPATHLEN, "%s/message", rootdir))
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
		if (! bsnprintf(evppath, MAXPATHLEN, "%s/%s", evpdir,
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
			imsg_compose(env->sc_ibufs[PROC_LKA], IMSG_LKA_MX,
			    0, 0, -1, batchp, sizeof(struct batch));
		}
	}

	TAILQ_INSERT_TAIL(&batchp->messages, messagep, entry);
	return batchp;
}

struct batch *
batch_lookup(struct smtpd *env, struct message *message)
{
	struct batch *batchp;
	struct batch lookup;

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

SPLAY_GENERATE(batchtree, batch, b_nodes, batch_cmp);

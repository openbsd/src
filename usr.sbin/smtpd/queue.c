/*	$OpenBSD: queue.c,v 1.18 2008/12/03 20:08:08 gilles Exp $	*/

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
#include <err.h>
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

__dead void	queue_shutdown(void);
void		queue_sig_handler(int, short, void *);
void		queue_dispatch_control(int, short, void *);
void		queue_dispatch_smtp(int, short, void *);
void		queue_dispatch_mda(int, short, void *);
void		queue_dispatch_mta(int, short, void *);
void		queue_dispatch_lka(int, short, void *);
void		queue_setup_events(struct smtpd *);
void		queue_disable_events(struct smtpd *);
void		queue_timeout(int, short, void *);
void		queue_process_runqueue(int, short, void *);
int		queue_create_incoming_layout(char *);
int		queue_record_envelope(struct message *);
int		queue_remove_envelope(struct message *);
int		queue_open_message_file(struct batch *);
int		queue_batch_resolved(struct smtpd *, struct batch *);
int		queue_message_schedule(struct message *, time_t);
void		queue_delete_message_file(char *);
u_int16_t	queue_message_hash(struct message *);
int		queue_record_incoming_envelope(struct message *);
int		queue_update_incoming_envelope(struct message *);
int		queue_remove_incoming_envelope(struct message *);
int		queue_commit_incoming_message(struct message *);
void		queue_delete_incoming_message(char *);
int		queue_update_envelope(struct message *);
int		queue_open_incoming_message_file(struct message *);
void		queue_process(struct smtpd *);
int		queue_process_bucket(struct smtpd *, u_int16_t);
int		queue_process_message(struct smtpd *, char *);
void		queue_process_envelope(struct smtpd *, char *, char *);
int		queue_load_envelope(struct message *, char *);
void		queue_delete_message(char *);

void		batch_send(struct smtpd *, struct batch *, time_t);
u_int32_t	hash(u_int8_t *, size_t);
struct batch	*queue_record_batch(struct smtpd *, struct message *);
struct batch    *batch_by_id(struct smtpd *, u_int64_t);
struct batch    *batch_lookup(struct smtpd *, struct message *);
struct message	*message_by_id(struct smtpd *, struct batch *, u_int64_t);

void
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

void
queue_dispatch_control(int sig, short event, void *p)
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
			fatal("queue_dispatch_control: imsg_read error");
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
queue_dispatch_smtp(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_SMTP];
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
			fatal("queue_dispatch_smtp: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_QUEUE_CREATE_MESSAGE: {
			struct message		*messagep;
			struct submit_status	 ss;

			log_debug("mfa_dispatch_smtp: creating message file");
			messagep = imsg.data;
			ss.id = messagep->session_id;
			ss.code = 250;
			bzero(ss.u.msgid, MAXPATHLEN);

			if (! queue_create_incoming_layout(ss.u.msgid))
				ss.code = 421;

			imsg_compose(ibuf, IMSG_SMTP_MESSAGE_ID, 0, 0, -1,
			    &ss, sizeof(ss));
			break;
		}
		case IMSG_QUEUE_REMOVE_MESSAGE: {
			struct message		*messagep;

			messagep = imsg.data;
			if (messagep->message_id[0] != '\0')
				queue_delete_incoming_message(messagep->message_id);
			break;
		}
		case IMSG_QUEUE_SUBMIT_ENVELOPE: {
			struct message		*messagep;
			struct submit_status	 ss;

			messagep = imsg.data;
			messagep->id = queue_generate_id();
			ss.id = messagep->session_id;
			ss.code = 250;
			ss.u.path = messagep->recipient;

			if (IS_MAILBOX(messagep->recipient.rule.r_action) ||
			    IS_EXT(messagep->recipient.rule.r_action))
				messagep->type = T_MDA_MESSAGE;
			else
				messagep->type = T_MTA_MESSAGE;

			/* Write to disk */
			if (! queue_record_incoming_envelope(messagep)) {
				ss.code = 421;
				imsg_compose(ibuf, IMSG_SMTP_SUBMIT_ACK, 0, 0, -1,
				    &ss, sizeof(ss));
				break;
			}

			imsg_compose(ibuf, IMSG_SMTP_SUBMIT_ACK, 0, 0, -1,
			    &ss, sizeof(ss));

			if (messagep->type & T_MTA_MESSAGE) {
				break;
			}

			if ((messagep->recipient.flags & (F_ALIAS|F_VIRTUAL)) == 0) {
				/* not an alias, perform ~/.forward resolution */
				imsg_compose(env->sc_ibufs[PROC_LKA], IMSG_LKA_FORWARD_LOOKUP, 0, 0, -1,
				    messagep, sizeof(struct message));
				break;
			}

			/* Recipient is an alias, proceed to resolving it.
			 * ~/.forward will be handled by the IMSG_LKA_ALIAS_RESULT
			 * dispatch case.
			 */
			imsg_compose(env->sc_ibufs[PROC_LKA], IMSG_LKA_ALIAS_LOOKUP, 0, 0, -1,
			    messagep, sizeof (struct message));

			break;
		}
		case IMSG_QUEUE_COMMIT_MESSAGE: {
			struct message		*messagep;
			struct submit_status	 ss;

			messagep = imsg.data;
			ss.id = messagep->session_id;

			if (! queue_commit_incoming_message(messagep))
				ss.code = 421;

			imsg_compose(ibuf, IMSG_SMTP_SUBMIT_ACK, 0, 0, -1,
			    &ss, sizeof(ss));

			break;
		}
		case IMSG_QUEUE_MESSAGE_FILE: {
			struct message		*messagep;
			struct submit_status	 ss;
			int fd;

			messagep = imsg.data;
			ss.id = messagep->session_id;

			fd = queue_open_incoming_message_file(messagep);
			if (fd == -1)
				ss.code = 421;

			imsg_compose(ibuf, IMSG_SMTP_MESSAGE_FILE, 0, 0, fd,
			    &ss, sizeof(ss));
			break;
		}
		default:
			log_debug("queue_dispatch_smtp: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
queue_dispatch_mda(int sig, short event, void *p)
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
			fatal("queue_dispatch_mda: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {

		case IMSG_QUEUE_MESSAGE_UPDATE: {
			struct message *messagep;

			messagep = (struct message *)imsg.data;
			messagep->batch_id = 0;
			messagep->retry++;

			if (messagep->status & S_MESSAGE_TEMPFAILURE) {
				messagep->status &= ~S_MESSAGE_TEMPFAILURE;
				messagep->flags &= ~F_MESSAGE_PROCESSING;
				queue_update_envelope(messagep);
				break;
			}

			if (messagep->status & S_MESSAGE_PERMFAILURE) {
				struct message msave;

				messagep->status &= ~S_MESSAGE_PERMFAILURE;
				if ((messagep->type & T_DAEMON_MESSAGE) == 0) {
					msave = *messagep;
					messagep->id = queue_generate_id();
					messagep->batch_id = 0;
					messagep->type |= T_DAEMON_MESSAGE;
					messagep->lasttry = 0;
					messagep->retry = 0;
					queue_record_envelope(messagep);
					*messagep = msave;
				}
				queue_remove_envelope(messagep);
				break;
			}

			/* no error, remove envelope */
			queue_remove_envelope(messagep);
			break;
		}

		default:
			log_debug("queue_dispatch_mda: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
queue_dispatch_mta(int sig, short event, void *p)
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
			fatal("queue_dispatch_mda: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {

		case IMSG_QUEUE_MESSAGE_FD: {
			int fd;
			struct batch *batchp;

			batchp = imsg.data;
			fd = queue_open_message_file(batchp);
			imsg_compose(ibuf,  IMSG_QUEUE_MESSAGE_FD, 0, 0, fd, batchp,
			    sizeof(*batchp));
			break;
		}

		case IMSG_QUEUE_MESSAGE_UPDATE: {
			struct message *messagep;

			messagep = (struct message *)imsg.data;
			messagep->batch_id = 0;
			messagep->retry++;

			if (messagep->status & S_MESSAGE_TEMPFAILURE) {
				messagep->status &= ~S_MESSAGE_TEMPFAILURE;
				messagep->flags &= ~F_MESSAGE_PROCESSING;
				queue_update_envelope(messagep);
				break;
			}

			if (messagep->status & S_MESSAGE_PERMFAILURE) {
				struct message msave;

				messagep->status &= ~S_MESSAGE_PERMFAILURE;
				if ((messagep->type & T_DAEMON_MESSAGE) == 0) {
					msave = *messagep;
					messagep->id = queue_generate_id();
					messagep->batch_id = 0;
					messagep->type |= T_DAEMON_MESSAGE;
					messagep->lasttry = 0;
					messagep->retry = 0;
					queue_record_envelope(messagep);
					*messagep = msave;
				}
				queue_remove_envelope(messagep);
				break;
			}

			/* no error, remove envelope */
			queue_remove_envelope(messagep);
			break;
		}

		default:
			log_debug("queue_dispatch_mda: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
queue_dispatch_lka(int sig, short event, void *p)
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
			fatal("queue_dispatch_lka: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {

		case IMSG_LKA_ALIAS_RESULT: {
			struct message *messagep;

			messagep = imsg.data;
			messagep->id = queue_generate_id();
			messagep->batch_id = 0;
			queue_record_incoming_envelope(messagep);

			if (messagep->type & T_MDA_MESSAGE) {
				imsg_compose(ibuf, IMSG_LKA_FORWARD_LOOKUP, 0, 0, -1,
				    messagep, sizeof(struct message));
			}
			break;
		}

		case IMSG_LKA_FORWARD_LOOKUP: {
			struct message *messagep;

			messagep = (struct message *)imsg.data;
			messagep->id = queue_generate_id();
			messagep->batch_id = 0;
			queue_record_incoming_envelope(messagep);
			break;
		}

		case IMSG_QUEUE_REMOVE_SUBMISSION: {
			struct message *messagep;

			messagep = (struct message *)imsg.data;
			queue_remove_incoming_envelope(messagep);
			break;
		}

		case IMSG_LKA_MX_LOOKUP: {
			queue_batch_resolved(env, imsg.data);
			break;
		}
		default:
			log_debug("queue_dispatch_lka: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
queue_shutdown(void)
{
	log_info("queue handler");
	_exit(0);
}

void
queue_setup_events(struct smtpd *env)
{
	struct timeval	 tv;

	evtimer_set(&env->sc_ev, queue_timeout, env);
	tv.tv_sec = 0;
	tv.tv_usec = 10;
	evtimer_add(&env->sc_ev, &tv);

	evtimer_set(&env->sc_rqev, queue_process_runqueue, env);
	tv.tv_sec = 0;
	tv.tv_usec = 10;
	evtimer_add(&env->sc_rqev, &tv);
}

void
queue_disable_events(struct smtpd *env)
{
	evtimer_del(&env->sc_ev);
}

void
queue_timeout(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct timeval		 tv;
	time_t curtime;
	struct batch *batchp, *nxt;

	queue_process(env);

	curtime = time(NULL);

	for (batchp = SPLAY_MIN(batchtree, &env->batch_queue);
	     batchp != NULL;
	     batchp = nxt) {
		nxt = SPLAY_NEXT(batchtree, &env->batch_queue, batchp);
		if ((batchp->type & T_MTA_BATCH) &&
		    (batchp->flags & F_BATCH_RESOLVED) == 0) {
			continue;
		}

		batch_send(env, batchp, curtime);

		SPLAY_REMOVE(batchtree, &env->batch_queue, batchp);
		bzero(batchp, sizeof(struct batch));
		free(batchp);

	}

	tv.tv_sec = 0;
	tv.tv_usec = 10;
	evtimer_add(&env->sc_ev, &tv);
}

pid_t
queue(struct smtpd *env)
{
	pid_t		 pid;
	struct passwd	*pw;

	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_CONTROL,	queue_dispatch_control },
		{ PROC_SMTP,	queue_dispatch_smtp },
		{ PROC_MDA,	queue_dispatch_mda },
		{ PROC_MTA,	queue_dispatch_mta },
		{ PROC_LKA,	queue_dispatch_lka }
	};

	switch (pid = fork()) {
	case -1:
		fatal("queue: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;

#ifndef DEBUG
	if (chroot(PATH_SPOOL) == -1)
		fatal("queue: chroot");
	if (chdir("/") == -1)
		fatal("queue: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
#endif

	setproctitle("queue handler");
	smtpd_process = PROC_QUEUE;

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("queue: cannot drop privileges");
#endif

	SPLAY_INIT(&env->batch_queue);

	event_init();

	signal_set(&ev_sigint, SIGINT, queue_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, queue_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_peers(env, peers, 5);

	queue_setup_events(env);
	event_dispatch();
	queue_shutdown();

	return (0);
}

void
queue_process(struct smtpd *env)
{
	u_int16_t cbucket = 0;
	static u_int16_t lbucket = 0;
	DIR *dirp;
	struct dirent *dp;
	const char *errstr;
	static u_int8_t bucketdone = 1;

	if (! bucketdone) {
		bucketdone = queue_process_bucket(env, lbucket);
		if (bucketdone)
			lbucket = (lbucket + 1) % DIRHASH_BUCKETS;
		return;
	}

	dirp = opendir(PATH_QUEUE);
	if (dirp == NULL)
		fatal("queue_process: opendir");

	while ((dp = readdir(dirp)) != NULL) {

		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;

		cbucket = strtonum(dp->d_name, 0, DIRHASH_BUCKETS - 1, &errstr);
		if (errstr) {
			log_warn("queue_process: %s/%s is not a valid bucket",
			    PATH_QUEUE, dp->d_name);
			continue;
		}

		if (cbucket == lbucket)
			break;
	}
	closedir(dirp);

	if (dp == NULL) {
		lbucket = (lbucket + 1) % DIRHASH_BUCKETS;
		return;
	}

	bucketdone = queue_process_bucket(env, cbucket);
	if (bucketdone)
		lbucket = (lbucket + 1) % DIRHASH_BUCKETS;
}

int
queue_process_bucket(struct smtpd *env, u_int16_t bucket)
{
	int spret;
	static DIR *dirp = NULL;
	struct dirent *dp;
	static char *msgid = NULL;
	char bucketpath[MAXPATHLEN];
	static u_int8_t messagedone = 1;

	if (! messagedone) {
		messagedone = queue_process_message(env, msgid);
		if (! messagedone)
			return 0;
		msgid = NULL;
	}

	spret = snprintf(bucketpath, MAXPATHLEN, "%s/%d", PATH_QUEUE, bucket);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_process_bucket: snprintf");

	if (dirp == NULL) {
		dirp = opendir(bucketpath);
		if (dirp == NULL)
			fatal("queue_process_bucket: opendir");
	}

	while ((dp = readdir(dirp)) != NULL) {

		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;

		break;
	}

	if (dp != NULL) {
		msgid = dp->d_name;
		messagedone = queue_process_message(env, msgid);
		if (! messagedone)
			return 0;
		msgid = NULL;
	}

	closedir(dirp);
	dirp = NULL;
	return 1;
}

int
queue_process_message(struct smtpd *env, char *messageid)
{
	int spret;
	static DIR *dirp = NULL;
	struct dirent *dp;
	char evppath[MAXPATHLEN];
	u_int16_t hval = 0;

	hval = hash(messageid, strlen(messageid)) % DIRHASH_BUCKETS;

	spret = snprintf(evppath, MAXPATHLEN, "%s/%d/%s%s", PATH_QUEUE, hval,
	    messageid, PATH_ENVELOPES);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_process_message: snprintf");

	if (dirp == NULL) {
		dirp = opendir(evppath);
		if (dirp == NULL)
			fatal("queue_process_message: opendir");
	}

	while ((dp = readdir(dirp)) != NULL) {

		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;
		break;
	}

	if (dp != NULL) {
		queue_process_envelope(env, messageid, dp->d_name);
		return 0;
	}

	closedir(dirp);
	dirp = NULL;
	return 1;
}

void
queue_process_envelope(struct smtpd *env, char *msgid, char *evpid)
{
	int spret;
	struct message message;
	time_t tm;
	char evppath[MAXPATHLEN];
	char rqpath[MAXPATHLEN];
	u_int16_t hval;
	struct stat sb;

	if (! queue_load_envelope(&message, evpid)) {
		log_debug("failed to load envelope: %s", evpid);
		return;
	}

	tm = time(NULL);

	if (! queue_message_schedule(&message, tm)) {
		if (message.flags & F_MESSAGE_EXPIRED) {
			log_debug("message has expired, mdaemon");
			queue_remove_envelope(&message);
		}
		return;
	}

	message.flags |= F_MESSAGE_SCHEDULED;
	queue_update_envelope(&message);

	log_debug("SCHEDULED: %s", evpid);
	hval = hash(msgid, strlen(msgid)) % DIRHASH_BUCKETS;
	spret = snprintf(evppath, MAXPATHLEN, "%s/%d/%s%s/%s", PATH_QUEUE, hval,
	    msgid, PATH_ENVELOPES, evpid);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_process_envelope: snprintf");

	spret = snprintf(rqpath, MAXPATHLEN, "%s/%s", PATH_RUNQUEUE, evpid);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_process_envelope: snprintf");

	if (stat(rqpath, &sb) == -1) {
		if (errno != ENOENT)
			fatal("queue_process_envelope: stat");

		if (symlink(evppath, rqpath) == -1) {
			log_info("queue_process_envelope: "
			    "failed to place envelope in runqueue");
		}
	}
}

void
queue_process_runqueue(int fd, short event, void *p)
{
	DIR *dirp;
	struct dirent *dp;
	struct message message;
	struct message *messagep;
	struct batch *batchp;
	char pathname[MAXPATHLEN];
	time_t tm;
	struct smtpd *env = p;
	struct timeval	 tv;

	tm = time(NULL);

	dirp = opendir(PATH_RUNQUEUE);
	if (dirp == NULL)
		fatal("queue_process_runqueue: opendir");

	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;

		/* XXX */
		snprintf(pathname, MAXPATHLEN, "%s/%s", PATH_RUNQUEUE, dp->d_name);
		unlink(pathname);

		if (! queue_load_envelope(&message, dp->d_name)) {
			log_debug("failed to load envelope");
			continue;
		}

		if (message.flags & F_MESSAGE_PROCESSING)
			continue;

		message.lasttry = tm;
		message.flags &= ~F_MESSAGE_SCHEDULED;
		message.flags |= F_MESSAGE_PROCESSING;
		queue_update_envelope(&message);

		messagep = calloc(1, sizeof (struct message));
		if (messagep == NULL)
			err(1, "calloc");
		*messagep = message;

		batchp = batch_lookup(env, messagep);
		if (batchp != NULL)
			messagep->batch_id = batchp->id;

		batchp = queue_record_batch(env, messagep);
		if (messagep->batch_id == 0)
			messagep->batch_id = batchp->id;
	}

	closedir(dirp);

	tv.tv_sec = 0;
	tv.tv_usec = 10;
	evtimer_add(&env->sc_rqev, &tv);
}

u_int64_t
queue_generate_id(void)
{
	u_int64_t	id;
	struct timeval	tp;

	if (gettimeofday(&tp, NULL) == -1)
		fatal("queue_generate_id: time");

	id = (u_int32_t)tp.tv_sec;
	id <<= 32;
	id |= (u_int32_t)tp.tv_usec;
	usleep(1);

	return (id);
}

struct batch *
queue_record_batch(struct smtpd *env, struct message *messagep)
{
	struct batch *batchp;
	struct path *path;

	batchp = NULL;
	if (messagep->batch_id != 0) {
		batchp = batch_by_id(env, messagep->batch_id);
		if (batchp == NULL)
			errx(1, "%s: internal inconsistency.", __func__);
	}

	if (batchp == NULL) {
		batchp = calloc(1, sizeof(struct batch));
		if (batchp == NULL)
			err(1, "%s: calloc", __func__);

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
			imsg_compose(env->sc_ibufs[PROC_LKA], IMSG_LKA_MX_LOOKUP, 0, 0, -1,
			    batchp, sizeof(struct batch));
		}
	}

	TAILQ_INSERT_TAIL(&batchp->messages, messagep, entry);

	return batchp;
}

int
queue_remove_batch_message(struct smtpd *env, struct batch *batchp, struct message *messagep)
{
	TAILQ_REMOVE(&batchp->messages, messagep, entry);
	bzero(messagep, sizeof(struct message));
	free(messagep);

	if (TAILQ_FIRST(&batchp->messages) == NULL) {
		SPLAY_REMOVE(batchtree, &env->batch_queue, batchp);
		bzero(batchp, sizeof(struct batch));
		free(batchp);
		return 1;
	}

	return 0;
}

int
queue_batch_resolved(struct smtpd *env, struct batch *lookup)
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
		batchp->flags |= F_BATCH_RESOLVED;
		for (i = 0; i < batchp->mx_cnt; ++i)
			batchp->mxarray[i].ss = lookup->mxarray[i].ss;
	}
	return 1;
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

int
queue_message_schedule(struct message *messagep, time_t tm)
{
	time_t delay;

	/* Batch has been in the queue for too long and expired */
	if (tm - messagep->creation >= SMTPD_QUEUE_EXPIRY) {
		messagep->flags |= F_MESSAGE_EXPIRED;
		return 0;
	}

	if (messagep->retry == 255) {
		messagep->flags |= F_MESSAGE_EXPIRED;
		return 0;
	}
	
	if ((messagep->flags & F_MESSAGE_SCHEDULED) != 0)
		return 0;

	if ((messagep->flags & F_MESSAGE_PROCESSING) != 0)
		return 0;

	if (messagep->lasttry == 0)
		return 1;

	delay = SMTPD_QUEUE_MAXINTERVAL;

	if (messagep->type & T_MDA_MESSAGE) {
		if (messagep->retry < 5)
			return 1;

		if (messagep->retry < 15)
			delay = (messagep->retry * 60) + arc4random() % 60;
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
batch_send(struct smtpd *env, struct batch *batchp, time_t curtime)
{
	u_int8_t proctype;
	struct message *messagep;

	if ((batchp->type & (T_MDA_BATCH|T_MTA_BATCH)) == 0)
		fatal("batch_send: unknown batch type");

	if (batchp->type & T_MDA_BATCH)
		proctype = PROC_MDA;
	else if (batchp->type & T_MTA_BATCH)
		proctype = PROC_MTA;

	imsg_compose(env->sc_ibufs[proctype], IMSG_CREATE_BATCH, 0, 0, -1,
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

struct batch *
batch_by_id(struct smtpd *env, u_int64_t id)
{
	struct batch lookup;

	lookup.id = id;
	return SPLAY_FIND(batchtree, &env->batch_queue, &lookup);
}


struct message *
message_by_id(struct smtpd *env, struct batch *batchp, u_int64_t id)
{
	struct message *messagep;

	if (batchp != NULL) {
		TAILQ_FOREACH(messagep, &batchp->messages, entry) {
			if (messagep->id == id)
				break;
		}
		return messagep;
	}

	SPLAY_FOREACH(batchp, batchtree, &env->batch_queue) {
		TAILQ_FOREACH(messagep, &batchp->messages, entry) {
			if (messagep->id == id)
				return messagep;
		}
	}
	return NULL;
}

int
queue_create_incoming_layout(char *message_id)
{
	int spret;
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];

	spret = snprintf(rootdir, MAXPATHLEN, "%s/%d.XXXXXXXXXXXXXXXX",
	    PATH_INCOMING, time(NULL));
	if (spret == -1 || spret >= MAXPATHLEN)
		return -1;

	if (mkdtemp(rootdir) == NULL)
		return -1;

	if (strlcpy(message_id, rootdir + strlen(PATH_INCOMING) + 1, MAXPATHLEN)
	    >= MAXPATHLEN)
		goto badroot;
	
	spret = snprintf(evpdir, MAXPATHLEN, "%s%s",
	    rootdir, PATH_ENVELOPES);
	if (spret == -1 || spret >= MAXPATHLEN)
		goto badroot;

	if (mkdir(evpdir, 0700) == -1)
		goto badroot;

	return 1;

badroot:
	if (rmdir(rootdir) == -1)
		fatal("queue_create_incoming_layout: rmdir");

	return 0;
}

void
queue_delete_incoming_message(char *message_id)
{
	int spret;
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];
	char evppath[MAXPATHLEN];
	char msgpath[MAXPATHLEN];
	DIR *dirp;
	struct dirent *dp;
	
	spret = snprintf(rootdir, MAXPATHLEN, "%s/%s", PATH_INCOMING,
	    message_id);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_delete_incoming_message: snprintf");

	spret = snprintf(evpdir, MAXPATHLEN, "%s%s",
	    rootdir, PATH_ENVELOPES);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_delete_incoming_message: snprintf");
	
	spret = snprintf(msgpath, MAXPATHLEN, "%s/message", rootdir);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_delete_incoming_message: snprintf");

	if (unlink(msgpath) == -1) {
		if (errno != ENOENT)
			fatal("queue_delete_incoming_message: unlink");
	}

	dirp = opendir(evpdir);
	if (dirp == NULL)
		fatal("queue_delete_incoming_message: opendir");
	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;
		spret = snprintf(evppath, MAXPATHLEN, "%s/%s", evpdir, dp->d_name);
		if (spret == -1 || spret >= MAXPATHLEN)
			fatal("queue_create_incoming_message: snprintf");

		if (unlink(evppath) == -1) {
			if (errno != ENOENT)
				fatal("queue_create_incoming_message: unlink");
		}
	}
	closedir(dirp);

	if (rmdir(evpdir) == -1)
		if (errno != ENOENT)
			fatal("queue_create_incoming_message: rmdir");

	if (rmdir(rootdir) == -1)
		if (errno != ENOENT)
			fatal("queue_create_incoming_message: rmdir");

	return;
}

int
queue_record_incoming_envelope(struct message *message)
{
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];
	char evpname[MAXPATHLEN];
	char message_uid[MAXPATHLEN];
	int fd;
	int mode = O_CREAT|O_TRUNC|O_WRONLY|O_EXCL|O_SYNC;
	int spret;
	FILE *fp;
	int ret;

	spret = snprintf(rootdir, MAXPATHLEN, "%s/%s", PATH_INCOMING,
	    message->message_id);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_record_incoming_envelope: snprintf");

	spret = snprintf(evpdir, MAXPATHLEN, "%s%s", rootdir, PATH_ENVELOPES);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_record_incoming_envelope: snprintf");

	for (;;) {
		spret = snprintf(evpname, MAXPATHLEN, "%s/%s.%qu", evpdir,
		    message->message_id, (u_int64_t)arc4random());
		if (spret == -1 || spret >= MAXPATHLEN)
			fatal("queue_record_incoming_envelope: snprintf");

		(void)strlcpy(message_uid, evpname + strlen(evpdir) + 1, MAXPATHLEN);

		fd = open(evpname, mode, 0600);
		if (fd == -1) {
			if (errno == EEXIST)
				continue;
			return 0;
		}

		if (flock(fd, LOCK_EX) == -1)
			fatal("queue_record_submission: flock");

		fp = fdopen(fd, "w");
		if (fp == NULL)
			fatal("fdopen");

		if (strlcpy(message->message_uid, message_uid, MAXPATHLEN)
		    >= MAXPATHLEN)
			fatal("queue_record_submission: strlcpy");

		message->creation = time(NULL);

		if ((ret = fwrite(message, sizeof (struct message), 1, fp)) != 1) {
			fclose(fp);
			unlink(evpname);
			return 0;
		}
		fflush(fp);
		fsync(fd);
		fclose(fp);

		break;
	}
	return 1;
}

int
queue_update_incoming_envelope(struct message *messagep)
{
	int fd;
	char pathname[MAXPATHLEN];
	int spret;
	FILE *fp;
	mode_t mode = O_RDWR;

	spret = snprintf(pathname, MAXPATHLEN, "%s/%s%s/%s", PATH_INCOMING,
	    messagep->message_id, PATH_ENVELOPES, messagep->message_uid);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_update_incoming_envelope: snprintf");

	if ((fd = open(pathname, mode)) == -1)
		fatal("queue_update_incoming_envelope: open");

	if (flock(fd, LOCK_EX) == -1)
		fatal("queue_update_incoming_envelope: flock");

	fp = fdopen(fd, "w");
	if (fp == NULL)
		fatal("queue_update_incoming_envelope: fdopen");

	if (fwrite(messagep, sizeof(struct message), 1, fp) != 1)
		fatal("queue_update_incoming_envelope: fwrite");
	fflush(fp);
	fsync(fd);
	fclose(fp);

	return 1;
}

int
queue_remove_incoming_envelope(struct message *message)
{
	char pathname[MAXPATHLEN];
	int spret;

	spret = snprintf(pathname, MAXPATHLEN, "%s/%s%s/%s", PATH_INCOMING,
	    message->message_id, PATH_ENVELOPES, message->message_uid);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_remove_incoming_envelope: snprintf");

	if (unlink(pathname) == -1)
		if (errno != ENOENT)
			fatal("queue_remove_incoming_envelope: unlink");

	return 1;
}

int
queue_commit_incoming_message(struct message *messagep)
{
	int spret;
	char rootdir[MAXPATHLEN];
	char queuedir[MAXPATHLEN];
	u_int16_t hval;
	
	spret = snprintf(rootdir, MAXPATHLEN, "%s/%s", PATH_INCOMING,
	    messagep->message_id);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_commit_message_incoming: snprintf");

	hval = queue_message_hash(messagep);

	spret = snprintf(queuedir, MAXPATHLEN, "%s/%d", PATH_QUEUE, hval);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_commit_message_incoming: snprintf");

	if (mkdir(queuedir, 0700) == -1) {
		if (errno == ENOSPC)
			return 0;
		if (errno != EEXIST)
			fatal("queue_commit_message_incoming: mkdir");
	}

	spret = snprintf(queuedir, MAXPATHLEN, "%s/%d/%s", PATH_QUEUE, hval,
		messagep->message_id);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_commit_message_incoming: snprintf");
	

	if (rename(rootdir, queuedir) == -1)
		fatal("queue_commit_message_incoming: rename");

	return 1;
}

int
queue_open_incoming_message_file(struct message *messagep)
{
	char pathname[MAXPATHLEN];
	int spret;
	mode_t mode = O_CREAT|O_EXCL|O_RDWR;

	spret = snprintf(pathname, MAXPATHLEN, "%s/%s/message", PATH_INCOMING,
	    messagep->message_id);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_open_incoming_message_file: snprintf");

	return open(pathname, mode, 0600);
}

int
queue_record_envelope(struct message *messagep)
{
	char queuedir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];
	char evpname[MAXPATHLEN];
	char message_uid[MAXPATHLEN];
	int fd;
	int mode = O_CREAT|O_TRUNC|O_WRONLY|O_EXCL|O_SYNC;
	int spret;
	FILE *fp;
	int ret;
	u_int16_t hval;

	spret = snprintf(queuedir, MAXPATHLEN, "%s/%s", PATH_QUEUE,
	    messagep->message_id);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_record_envelope: snprintf");

	hval = queue_message_hash(messagep);

	spret = snprintf(queuedir, MAXPATHLEN, "%s/%d", PATH_QUEUE, hval);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_record_envelope: snprintf");

	spret = snprintf(evpdir, MAXPATHLEN, "%s/%s%s", queuedir, messagep->message_id,
	    PATH_ENVELOPES);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_record_envelope: snprintf");

	for (;;) {
		spret = snprintf(evpname, MAXPATHLEN, "%s/%s.%qu", evpdir,
		    messagep->message_id, (u_int64_t)arc4random());
		if (spret == -1 || spret >= MAXPATHLEN)
			fatal("queue_record_envelope: snprintf");

		(void)strlcpy(message_uid, evpname + strlen(evpdir) + 1, MAXPATHLEN);

		fd = open(evpname, mode, 0600);
		if (fd == -1) {
			if (errno == EEXIST)
				continue;
			log_debug("failed to open %s", evpname);
			fatal("queue_record_envelope: open");
		}

		if (flock(fd, LOCK_EX) == -1)
			fatal("queue_record_envelope: flock");

		fp = fdopen(fd, "w");
		if (fp == NULL)
			fatal("fdopen");

		if (strlcpy(messagep->message_uid, message_uid, MAXPATHLEN)
		    >= MAXPATHLEN)
			fatal("queue_record_envelope: strlcpy");

		messagep->creation = time(NULL);

		if ((ret = fwrite(messagep, sizeof (struct message), 1, fp)) != 1) {
			fclose(fp);
			unlink(evpname);
			return 0;
		}
		fflush(fp);
		fsync(fd);
		fclose(fp);

		break;
	}
	return 1;

}

int
queue_remove_envelope(struct message *messagep)
{
	char pathname[MAXPATHLEN];
	u_int16_t hval;
	int spret;

	hval = queue_message_hash(messagep);

	spret = snprintf(pathname, MAXPATHLEN, "%s/%d/%s%s/%s", PATH_QUEUE,
	    hval, messagep->message_id, PATH_ENVELOPES, messagep->message_uid);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_remove_incoming_envelope: snprintf");

	if (unlink(pathname) == -1)
		fatal("queue_remove_incoming_envelope: unlink");

	spret = snprintf(pathname, MAXPATHLEN, "%s/%d/%s%s", PATH_QUEUE,
	    hval, messagep->message_id, PATH_ENVELOPES);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_remove_incoming_envelope: snprintf");

	if (rmdir(pathname) != -1)
		queue_delete_message(messagep->message_id);

	return 1;
}

int
queue_update_envelope(struct message *messagep)
{
	int fd;
	char pathname[MAXPATHLEN];
	int spret;
	FILE *fp;
	mode_t mode = O_RDWR;
	u_int16_t hval;

	hval = queue_message_hash(messagep);

	spret = snprintf(pathname, MAXPATHLEN, "%s/%d/%s%s/%s", PATH_QUEUE,
	    hval, messagep->message_id, PATH_ENVELOPES, messagep->message_uid);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_update_envelope: snprintf");

	if ((fd = open(pathname, mode)) == -1)
		fatal("queue_update_envelope: open");

	if (flock(fd, LOCK_EX) == -1)
		fatal("queue_update_envelope: flock");

	fp = fdopen(fd, "w");
	if (fp == NULL)
		fatal("queue_update_envelope: fdopen");

	if (fwrite(messagep, sizeof(struct message), 1, fp) != 1)
		fatal("queue_update_envelope: fwrite");
	fflush(fp);
	fsync(fd);
	fclose(fp);

	return 1;
}

int
queue_load_envelope(struct message *messagep, char *evpid)
{
	int spret;
	char pathname[MAXPATHLEN];
	u_int16_t hval;
	FILE *fp;
	char msgid[MAXPATHLEN];

	strlcpy(msgid, evpid, MAXPATHLEN);
	*strrchr(msgid, '.') = '\0';

	hval = hash(msgid, strlen(msgid)) % DIRHASH_BUCKETS;
	spret = snprintf(pathname, MAXPATHLEN, "%s/%d/%s%s/%s", PATH_QUEUE,
	    hval, msgid, PATH_ENVELOPES, evpid);

	fp = fopen(pathname, "r");
	if (fp == NULL)
		return 0;

	if (fread(messagep, sizeof(struct message), 1, fp) != 1)
		fatal("queue_load_envelope: fread");

	fclose(fp);

	return 1;
}

int
queue_open_message_file(struct batch *batchp)
{
	int fd;
	char pathname[MAXPATHLEN];
	int spret;
	mode_t mode = O_RDONLY;
	u_int16_t hval;

	hval = hash(batchp->message_id, strlen(batchp->message_id)) % DIRHASH_BUCKETS;

	spret = snprintf(pathname, MAXPATHLEN, "%s/%d/%s/message", PATH_QUEUE,
	    hval, batchp->message_id);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_open_message_file: snprintf");

	if ((fd = open(pathname, mode)) == -1)
		fatal("queue_open_message_file: open");

	return fd;
}

void
queue_delete_message(char *msgid)
{
	int spret;
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];
	char msgpath[MAXPATHLEN];
	u_int16_t hval;

	hval = hash(msgid, strlen(msgid)) % DIRHASH_BUCKETS;
	spret = snprintf(rootdir, MAXPATHLEN, "%s/%d/%s", PATH_QUEUE,
	    hval, msgid);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_delete_message: snprintf");

	spret = snprintf(evpdir, MAXPATHLEN, "%s%s",
	    rootdir, PATH_ENVELOPES);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_delete_message: snprintf");
	
	spret = snprintf(msgpath, MAXPATHLEN, "%s/message", rootdir);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_delete_message: snprintf");

	if (unlink(msgpath) == -1)
		if (errno != ENOENT)
			fatal("queue_delete_message: unlink");

	if (rmdir(evpdir) == -1)
		if (errno != ENOENT)
			fatal("queue_delete_message: rmdir");

	if (rmdir(rootdir) == -1)
		if (errno != ENOENT)
			fatal("queue_delete_message: rmdir");

	spret = snprintf(rootdir, MAXPATHLEN, "%s/%d", PATH_QUEUE,
	    hval);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_delete_message: snprintf");

	rmdir(rootdir);

	return;
}

u_int16_t
queue_message_hash(struct message *messagep)
{
	return hash(messagep->message_id, strlen(messagep->message_id))
	    % DIRHASH_BUCKETS;
}

u_int32_t
hash(u_int8_t *buf, size_t len)
{
	u_int32_t h;

	for (h = 5381; len; len--)
		h = ((h << 5) + h) + *buf++;

	return h;
}

SPLAY_GENERATE(batchtree, batch, b_nodes, batch_cmp);

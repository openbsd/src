/*	$OpenBSD: queue.c,v 1.12 2008/11/17 20:37:48 gilles Exp $	*/

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
int		queue_create_message_file(char *);
void		queue_delete_message_file(char *);
int		queue_record_submission(struct message *);
int		queue_remove_submission(struct message *);
struct batch    *batch_lookup(struct smtpd *, struct message *);
int		batch_schedule(struct batch *, time_t);
void		batch_unschedule(struct batch *);
void		batch_send(struct smtpd *, struct batch *, time_t);
int		queue_update_database(struct message *);
int		queue_open_message_file(struct batch *);
int		queue_batch_resolved(struct smtpd *, struct batch *);
struct batch	*queue_record_batch(struct smtpd *, struct message *);
struct batch    *batch_by_id(struct smtpd *, u_int64_t);
struct message	*message_by_id(struct smtpd *, struct batch *, u_int64_t);
void		queue_mailer_daemon(struct smtpd *, struct batch *, enum batch_status);
void		debug_display_batch(struct batch *);
void		debug_display_message(struct message *);
int		queue_record_daemon(struct message *);
struct batch	*queue_register_daemon_batch(struct smtpd *, struct batch *);
void		queue_register_daemon_message(struct smtpd *, struct batch *, struct message *);
void		queue_load_submissions(struct smtpd *, time_t);
int		queue_message_schedule(struct message *, time_t);
int		queue_message_from_id(char *, struct message *);
int		queue_message_complete(struct message *);
int		queue_init_submissions(void);

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
		case IMSG_QUEUE_CREATE_MESSAGE_FILE: {
			struct message		*messagep;
			struct submit_status	 ss;
			int			 fd;

			log_debug("mfa_dispatch_smtp: creating message file");
			messagep = imsg.data;
			ss.id = messagep->session_id;
			ss.code = 250;
			fd = queue_create_message_file(ss.u.msgid);
			imsg_compose(ibuf, IMSG_SMTP_MESSAGE_FILE, 0, 0, fd,
			    &ss, sizeof(ss));
			break;
		}
		case IMSG_QUEUE_DELETE_MESSAGE_FILE: {
			struct message		*messagep;

			messagep = imsg.data;
			queue_delete_message_file(messagep->message_id);
			break;
		}
		case IMSG_QUEUE_MESSAGE_SUBMIT: {
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
			queue_record_submission(messagep);
			imsg_compose(ibuf, IMSG_SMTP_SUBMIT_ACK, 0, 0, -1,
			    &ss, sizeof(ss));

			if (messagep->type & T_MTA_MESSAGE) {
				messagep->flags |= F_MESSAGE_READY;
				queue_update_database(messagep);
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
		case IMSG_QUEUE_MESSAGE_COMPLETE: {
			struct message		*messagep;
			struct submit_status	 ss;

			messagep = imsg.data;
			ss.id = messagep->session_id;

			queue_message_complete(messagep);

			imsg_compose(ibuf, IMSG_SMTP_SUBMIT_ACK, 0, 0, -1,
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
				queue_update_database(messagep);
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
					messagep->flags |= F_MESSAGE_READY;
					messagep->lasttry = 0;
					messagep->retry = 0;
					queue_record_submission(messagep);
					*messagep = msave;
				}
				queue_remove_submission(messagep);
				break;
			}

			/* no error, remove submission */
			queue_remove_submission(messagep);
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
				queue_update_database(messagep);
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
					messagep->flags |= F_MESSAGE_READY;
					messagep->lasttry = 0;
					messagep->retry = 0;
					queue_record_submission(messagep);
					*messagep = msave;
				}
				queue_remove_submission(messagep);
				break;
			}

			/* no error, remove submission */
			queue_remove_submission(messagep);
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
			queue_record_submission(messagep);

			if (messagep->type & T_MTA_MESSAGE) {
				messagep->flags |= F_MESSAGE_READY;
				queue_update_database(messagep);
			}

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
			messagep->flags |= F_MESSAGE_READY;
			queue_record_submission(messagep);
			break;
		}

		case IMSG_QUEUE_REMOVE_SUBMISSION: {
			struct message *messagep;

			messagep = (struct message *)imsg.data;
			queue_remove_submission(messagep);
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
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	evtimer_add(&env->sc_ev, &tv);
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
	struct batch		*batchp, *nxt;
	struct timeval		 tv;
	time_t curtime;

	curtime = time(NULL);
	queue_load_submissions(env, curtime);

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

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	evtimer_add(&env->sc_ev, &tv);
}

void
queue_load_submissions(struct smtpd *env, time_t tm)
{
	DIR *dirp;
	struct dirent *dp;
	struct batch *batchp;
	struct message *messagep;
	struct message message;

	dirp = opendir(PATH_ENVELOPES);
	if (dirp == NULL)
		err(1, "opendir");

	while ((dp = readdir(dirp)) != NULL) {

		if (dp->d_name[0] == '.')
			continue;

		if (! queue_message_from_id(dp->d_name, &message)) {
			warnx("failed to load message \"%s\"", dp->d_name);
			continue;
		}

		if (! queue_message_schedule(&message, tm)) {
			if (message.flags & F_MESSAGE_EXPIRED) {
				log_debug("message expired, create mdaemon");
				queue_remove_submission(&message);
			}
			continue;
		}

		message.lasttry = tm;
		message.flags |= F_MESSAGE_PROCESSING;
		queue_update_database(&message);

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
}

int
queue_message_from_id(char *message_id, struct message *message)
{
	int spret;
	char pathname[MAXPATHLEN];
	FILE *fp;

	spret = snprintf(pathname, MAXPATHLEN, "%s/%s", PATH_ENVELOPES, message_id);
	if (spret == -1 || spret >= MAXPATHLEN) {
		warnx("queue_load_submissions: filename too long.");
		return 0;
	}

	fp = fopen(pathname, "r");
	if (fp == NULL) {
		warnx("queue_load_submissions: fopen: %s", message_id);
		goto bad;
	}

	if (fread(message, 1, sizeof(struct message), fp) !=
	    sizeof(struct message)) {
		warnx("queue_load_submissions: fread: %s", message_id);
		goto bad;
	}

	fclose(fp);
	return 1;
bad:
	if (fp != NULL)
		fclose(fp);
	return 0;
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

	queue_init_submissions();
	queue_load_submissions(env, time(NULL));

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

int
queue_create_message_file(char *message_id)
{
	int fd;
	char pathname[MAXPATHLEN];
	int spret;

	spret = snprintf(pathname, MAXPATHLEN, "%s/%d.XXXXXXXXXXXXXXXX",
	    PATH_MESSAGES, time(NULL));
	if (spret == -1 || spret >= MAXPATHLEN)
		return -1;

	fd = mkstemp(pathname);
	if (fd == -1)
		fatal("queue_create_message_file: mkstemp");

	/* XXX - this won't fail if message_id is MAXPATHLEN bytes */
	if (strlcpy(message_id, pathname + sizeof(PATH_MESSAGES), MAXPATHLEN)
	    >= MAXPATHLEN)
		fatal("queue_create_message_file: message id too long");

	return fd;
}

void
queue_delete_message_file(char *message_id)
{
	char pathname[MAXPATHLEN];
	int spret;

	spret = snprintf(pathname, MAXPATHLEN, "%s/%s", PATH_MESSAGES, message_id);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_delete_message_file: message id too long");

	if (unlink(pathname) == -1)
		fatal("queue_delete_message_file: unlink");
}

int
queue_record_submission(struct message *message)
{
	char pathname[MAXPATHLEN];
	char linkname[MAXPATHLEN];
	char dbname[MAXPATHLEN];
	char message_uid[MAXPATHLEN];
	char *spool;
	size_t spoolsz;
	int fd;
	int mode = O_CREAT|O_TRUNC|O_WRONLY|O_EXCL|O_SYNC;
	int spret;
	FILE *fp;
	int hm;

	if (message->type & T_DAEMON_MESSAGE) {
		spool = PATH_DAEMON;
	}
	else {
		switch (message->recipient.rule.r_action) {
		case A_MBOX:
		case A_MAILDIR:
		case A_EXT:
			spool = PATH_LOCAL;
			break;
		default:
			spool = PATH_RELAY;
		}
	}
	spoolsz = strlen(spool);

	spret = snprintf(pathname, MAXPATHLEN, "%s/%s", PATH_MESSAGES,
	    message->message_id);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_record_submission: message id too long");

	for (;;) {
		spret = snprintf(linkname, MAXPATHLEN, "%s/%s.%qu", spool,
		    message->message_id, (u_int64_t)arc4random());
		if (spret == -1 || spret >= MAXPATHLEN)
			fatal("queue_record_submission: message uid too long");

		(void)strlcpy(message_uid, linkname + spoolsz + 1, MAXPATHLEN);

		if (link(pathname, linkname) == -1) {
			if (errno == EEXIST)
				continue;
			err(1, "link: %s , %s", pathname, linkname);
		}

		spret = snprintf(dbname, MAXPATHLEN, "%s/%s", PATH_ENVELOPES,
		    message_uid);
		if (spret == -1 || spret >= MAXPATHLEN)
			fatal("queue_record_submission: database uid too long");

		fd = open(dbname, mode, 0600);
		if (fd == -1)
			if (unlink(linkname) == -1)
				fatal("queue_record_submission: unlink");

		if (flock(fd, LOCK_EX) == -1)
			fatal("queue_record_submission: flock");

		fp = fdopen(fd, "w");
		if (fp == NULL)
			fatal("fdopen");

		if (strlcpy(message->message_uid, message_uid, MAXPATHLEN)
		    >= MAXPATHLEN)
			fatal("queue_record_submission: message uid too long");

		message->creation = time(NULL);

		if ((hm = fwrite(message, 1, sizeof(struct message), fp)) !=
		    sizeof(struct message)) {
			fclose(fp);
			unlink(dbname);
			return 0;
		}
		fflush(fp);
		fsync(fd);
		fclose(fp);

		break;
	}
	return 1;
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
queue_remove_submission(struct message *message)
{
	char pathname[MAXPATHLEN];
	char linkname[MAXPATHLEN];
	char dbname[MAXPATHLEN];
	char *spool;
	struct stat sb;
	int spret;

	if (message->type & T_DAEMON_MESSAGE) {
		spool = PATH_DAEMON;
	}
	else {
		switch (message->recipient.rule.r_action) {
		case A_MBOX:
		case A_MAILDIR:
		case A_EXT:
			spool = PATH_LOCAL;
			break;
		default:
			spool = PATH_RELAY;
		}
	}

	spret = snprintf(dbname, MAXPATHLEN, "%s/%s", PATH_ENVELOPES,
	    message->message_uid);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_remove_submission: database uid too long");

	spret = snprintf(linkname, MAXPATHLEN, "%s/%s", spool,
	    message->message_uid);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_remove_submission: message uid too long");

	spret = snprintf(pathname, MAXPATHLEN, "%s/%s", PATH_MESSAGES,
	    message->message_id);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_remove_submission: message id too long");

	if (unlink(dbname) == -1) {
		warnx("dbname: %s", dbname);
		fatal("queue_remove_submission: unlink");
	}

	if (unlink(linkname) == -1) {
		warnx("linkname: %s", linkname);
		fatal("queue_remove_submission: unlink");
	}

	if (stat(pathname, &sb) == -1) {
		warnx("pathname: %s", pathname);
		fatal("queue_remove_submission: stat");
	}

	if (sb.st_nlink == 1) {
		if (unlink(pathname) == -1) {
			warnx("pathname: %s", pathname);
			fatal("queue_remove_submission: unlink");
		}
	}

	return 1;
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
	batchp->ss_cnt = lookup->ss_cnt;

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
		for (i = 0; i < batchp->ss_cnt; ++i)
			batchp->ss[i] = lookup->ss[i];
	}
	return 1;
}

int
queue_open_message_file(struct batch *batch)
{
	int fd;
	char pathname[MAXPATHLEN];
	int spret;

	spret = snprintf(pathname, MAXPATHLEN, "%s/%s", PATH_MESSAGES,
	    batch->message_id);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_open_message_file: message id too long");

	fd = open(pathname, O_RDONLY);
	if (fd == -1)
		fatal("queue_open_message_file: open");

	return fd;
}

int
queue_update_database(struct message *message)
{
	int fd;
	char *spool;
	char pathname[MAXPATHLEN];
	int spret;
	FILE *fp;
	mode_t mode = O_RDWR;

	if (message->type & T_DAEMON_MESSAGE) {
		spool = PATH_DAEMON;
	}
	else {
		switch (message->recipient.rule.r_action) {
		case A_MBOX:
		case A_MAILDIR:
		case A_EXT:
			spool = PATH_LOCAL;
			break;
		default:
			spool = PATH_RELAY;
		}
	}

	spret = snprintf(pathname, MAXPATHLEN, "%s/%s", PATH_ENVELOPES,
	    message->message_uid);
	if (spret == -1 || spret >= MAXPATHLEN)
		fatal("queue_update_database: pathname too long");

	if ((fd = open(pathname, mode)) == -1)
		fatal("queue_update_database: cannot open database");


	if (flock(fd, LOCK_EX) == -1)
		fatal("queue_update_database: flock");

	fp = fdopen(fd, "w");
	if (fp == NULL)
		fatal("fdopen");

	if (fwrite(message, 1, sizeof(struct message), fp) !=
	    sizeof(struct message))
		fatal("queue_update_database: cannot write database");
	fflush(fp);
	fsync(fd);
	fclose(fp);

	return 1;
}


int
queue_record_daemon(struct message *message)
{
	char pathname[MAXPATHLEN];
	char linkname[MAXPATHLEN];
	char dbname[MAXPATHLEN];
	char message_uid[MAXPATHLEN];
	size_t spoolsz;
	int fd;
	int mode = O_CREAT|O_TRUNC|O_WRONLY|O_EXCL|O_SYNC;
	int spret;
	FILE *fp;

	spret = snprintf(pathname, MAXPATHLEN, "%s/%s",
	    PATH_MESSAGES, message->message_id);
	if (spret == -1 || spret >= MAXPATHLEN)
		return 0;

	spoolsz = strlen(PATH_DAEMON);

	for (;;) {
		spret = snprintf(linkname, MAXPATHLEN, "%s/%s.%qu",
		    PATH_DAEMON, message->message_id, (u_int64_t)arc4random());
		if (spret == -1 || spret >= MAXPATHLEN)
			return 0;

		if (strlcpy(message_uid, linkname + spoolsz + 1, MAXPATHLEN)
		    >= MAXPATHLEN)
			return 0;

		if (link(pathname, linkname) == -1) {
			if (errno == EEXIST)
				continue;
			err(1, "link");
		}

		spret = snprintf(dbname, MAXPATHLEN, "%s/%s",
		    PATH_ENVELOPES, message_uid);
		if (spret == -1 || spret >= MAXPATHLEN)
			return 0;

		fd = open(dbname, mode, 0600);
		if (fd == -1)
			if (unlink(linkname) == -1)
				err(1, "unlink");

		if (flock(fd, LOCK_EX) == -1)
			err(1, "flock");

		fp = fdopen(fd, "w");
		if (fp == NULL)
			fatal("fdopen");

		(void)strlcpy(message->message_uid, message_uid, MAXPATHLEN);

		message->creation = time(NULL);

		if (fwrite(message, 1, sizeof(struct message), fp) !=
		    sizeof(struct message)) {
			fclose(fp);
			unlink(dbname);
			return 0;
		}
		fflush(fp);
		fsync(fd);
		fclose(fp);
		break;
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
queue_init_submissions(void)
{
	DIR *dirp;
	struct dirent *dp;
	struct message message;
	char pathname[MAXPATHLEN];
	FILE *fp;
	int spret;

	dirp = opendir(PATH_ENVELOPES);
	if (dirp == NULL)
		err(1, "opendir");

	while ((dp = readdir(dirp)) != NULL) {

		if (dp->d_name[0] == '.')
			continue;

		spret = snprintf(pathname, MAXPATHLEN, "%s/%s", PATH_ENVELOPES,
		    dp->d_name);
		if (spret == -1 || spret >= MAXPATHLEN)
			continue;

		fp = fopen(pathname, "r");
		if (fp == NULL)
			continue;

		if (fread(&message, 1, sizeof(struct message), fp) !=
		    sizeof(struct message)) {
			fclose(fp);
			continue;
		}
		fclose(fp);

		if ((message.flags & F_MESSAGE_COMPLETE) == 0)
			unlink(pathname);
		else {
			message.flags &= ~F_MESSAGE_PROCESSING;
			queue_update_database(&message);
		}
	}

	closedir(dirp);
	return 1;
}

int
queue_message_complete(struct message *messagep)
{
	DIR *dirp;
	struct dirent *dp;
	struct message message;
	char pathname[MAXPATHLEN];
	FILE *fp;
	int spret;

	dirp = opendir(PATH_ENVELOPES);
	if (dirp == NULL)
		err(1, "opendir");

	while ((dp = readdir(dirp)) != NULL) {

		if (dp->d_name[0] == '.')
			continue;

		if (strncmp(messagep->message_id,
			dp->d_name, strlen(messagep->message_id)) != 0)
			continue;

		spret = snprintf(pathname, MAXPATHLEN, "%s/%s", PATH_ENVELOPES,
		    dp->d_name);
		if (spret == -1 || spret >= MAXPATHLEN)
			continue;

		fp = fopen(pathname, "r");
		if (fp == NULL)
			continue;

		if (fread(&message, 1, sizeof(struct message), fp) !=
		    sizeof(struct message)) {
			fclose(fp);
			continue;
		}
		fclose(fp);

		message.flags |= F_MESSAGE_COMPLETE;
		queue_update_database(&message);
	}

	closedir(dirp);
	return 1;
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

	if ((messagep->flags & F_MESSAGE_READY) == 0)
		return 0;

	if ((messagep->flags & F_MESSAGE_COMPLETE) == 0)
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
batch_unschedule(struct batch *batchp)
{
	batchp->flags &= ~(F_BATCH_SCHEDULED);
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

SPLAY_GENERATE(batchtree, batch, b_nodes, batch_cmp);

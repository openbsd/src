/*	$OpenBSD: queue.c,v 1.25 2008/12/13 23:19:34 jacekm Exp $	*/

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

__dead void	queue_shutdown(void);
void		queue_sig_handler(int, short, void *);
void		queue_dispatch_control(int, short, void *);
void		queue_dispatch_smtp(int, short, void *);
void		queue_dispatch_mda(int, short, void *);
void		queue_dispatch_mta(int, short, void *);
void		queue_dispatch_lka(int, short, void *);
void		queue_dispatch_runner(int, short, void *);
void		queue_setup_events(struct smtpd *);
void		queue_disable_events(struct smtpd *);
void		queue_timeout(int, short, void *);
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

			imsg_compose(ibuf, IMSG_QUEUE_CREATE_MESSAGE, 0, 0, -1,
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
				imsg_compose(ibuf, IMSG_QUEUE_SUBMIT_ENVELOPE,
				    0, 0, -1, &ss, sizeof(ss));
				break;
			}

			imsg_compose(ibuf, IMSG_QUEUE_SUBMIT_ENVELOPE, 0, 0, -1,
			    &ss, sizeof(ss));

			if (messagep->type & T_MTA_MESSAGE) {
				break;
			}

			if ((messagep->recipient.flags & (F_ALIAS|F_VIRTUAL)) == 0) {
				/* not an alias, perform ~/.forward resolution */
				imsg_compose(env->sc_ibufs[PROC_LKA],
				    IMSG_LKA_FORWARD, 0, 0, -1, messagep,
				    sizeof(struct message));
				break;
			}

			/* Recipient is an alias, proceed to resolving it.
			 * ~/.forward will be handled by the IMSG_LKA_ALIAS
			 * dispatch case.
			 */
			imsg_compose(env->sc_ibufs[PROC_LKA],
			    IMSG_LKA_ALIAS, 0, 0, -1, messagep,
			    sizeof (struct message));

			break;
		}
		case IMSG_QUEUE_COMMIT_MESSAGE: {
			struct message		*messagep;
			struct submit_status	 ss;

			messagep = imsg.data;
			ss.id = messagep->session_id;

			if (! queue_commit_incoming_message(messagep))
				ss.code = 421;

			imsg_compose(ibuf, IMSG_QUEUE_COMMIT_MESSAGE, 0, 0, -1,
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

			imsg_compose(ibuf, IMSG_QUEUE_MESSAGE_FILE, 0, 0, fd,
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

		case IMSG_LKA_ALIAS: {
			struct message *messagep;

			messagep = imsg.data;
			messagep->id = queue_generate_id();
			messagep->batch_id = 0;
			queue_record_incoming_envelope(messagep);

			if (messagep->type & T_MDA_MESSAGE) {
				imsg_compose(ibuf, IMSG_LKA_FORWARD, 0, 0, -1,
				    messagep, sizeof(struct message));
			}
			break;
		}

		case IMSG_LKA_FORWARD: {
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
queue_dispatch_runner(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_RUNNER];
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
			fatal("queue_dispatch_runner: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("queue_dispatch_runner: unexpected imsg %d",
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
}

void
queue_disable_events(struct smtpd *env)
{
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
		{ PROC_LKA,	queue_dispatch_lka },
		{ PROC_RUNNER,	queue_dispatch_runner }
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

	event_init();

	signal_set(&ev_sigint, SIGINT, queue_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, queue_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_peers(env, peers, 6);

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
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];

	if (! bsnprintf(rootdir, MAXPATHLEN, "%s/%d.XXXXXXXXXXXXXXXX",
		PATH_INCOMING, time(NULL)))
		return -1;

	if (mkdtemp(rootdir) == NULL)
		return -1;

	if (strlcpy(message_id, rootdir + strlen(PATH_INCOMING) + 1, MAXPATHLEN)
	    >= MAXPATHLEN)
		goto badroot;
	
	if (! bsnprintf(evpdir, MAXPATHLEN, "%s%s",
		rootdir, PATH_ENVELOPES))
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
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];
	char evppath[MAXPATHLEN];
	char msgpath[MAXPATHLEN];
	DIR *dirp;
	struct dirent *dp;
	
	if (! bsnprintf(rootdir, MAXPATHLEN, "%s/%s", PATH_INCOMING,
		message_id))
		fatal("queue_delete_incoming_message: snprintf");

	if (! bsnprintf(evpdir, MAXPATHLEN, "%s%s",
		rootdir, PATH_ENVELOPES))
		fatal("queue_delete_incoming_message: snprintf");
	
	if (! bsnprintf(msgpath, MAXPATHLEN, "%s/message", rootdir))
		fatal("queue_delete_incoming_message: snprintf");

	if (unlink(msgpath) == -1) {
		if (errno != ENOENT)
			fatal("queue_delete_incoming_message: unlink");
	}

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
		if (! bsnprintf(evppath, MAXPATHLEN, "%s/%s", evpdir, dp->d_name))
			fatal("queue_delete_incoming_message: snprintf");

		if (unlink(evppath) == -1) {
			if (errno != ENOENT)
				fatal("queue_delete_incoming_message: unlink");
		}
	}
	closedir(dirp);

	if (rmdir(evpdir) == -1)
		if (errno != ENOENT)
			fatal("queue_delete_incoming_message: rmdir");

delroot:
	if (rmdir(rootdir) == -1)
		if (errno != ENOENT)
			fatal("queue_delete_incoming_message: rmdir");

	return;
}

int
queue_record_incoming_envelope(struct message *message)
{
	char evpdir[MAXPATHLEN];
	char evpname[MAXPATHLEN];
	char message_uid[MAXPATHLEN];
	int fd;
	int mode = O_CREAT|O_TRUNC|O_WRONLY|O_EXCL|O_SYNC;
	FILE *fp;
	int ret;

	if (! bsnprintf(evpdir, MAXPATHLEN, "%s/%s%s", PATH_INCOMING,
		message->message_id, PATH_ENVELOPES))
		fatal("queue_record_incoming_envelope: snprintf");

	for (;;) {
		if (! bsnprintf(evpname, MAXPATHLEN, "%s/%s.%qu", evpdir,
			message->message_id, (u_int64_t)arc4random()))
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
	FILE *fp;
	mode_t mode = O_RDWR;

	if (! bsnprintf(pathname, MAXPATHLEN, "%s/%s%s/%s", PATH_INCOMING,
		messagep->message_id, PATH_ENVELOPES, messagep->message_uid))
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

	if (! bsnprintf(pathname, MAXPATHLEN, "%s/%s%s/%s", PATH_INCOMING,
		message->message_id, PATH_ENVELOPES, message->message_uid))
		fatal("queue_remove_incoming_envelope: snprintf");

	if (unlink(pathname) == -1)
		if (errno != ENOENT)
			fatal("queue_remove_incoming_envelope: unlink");

	return 1;
}

int
queue_commit_incoming_message(struct message *messagep)
{
	char rootdir[MAXPATHLEN];
	char queuedir[MAXPATHLEN];
	u_int16_t hval;
	
	if (! bsnprintf(rootdir, MAXPATHLEN, "%s/%s", PATH_INCOMING,
		messagep->message_id))
		fatal("queue_commit_message_incoming: snprintf");

	hval = queue_message_hash(messagep);

	if (! bsnprintf(queuedir, MAXPATHLEN, "%s/%d", PATH_QUEUE, hval))
		fatal("queue_commit_message_incoming: snprintf");

	if (mkdir(queuedir, 0700) == -1) {
		if (errno == ENOSPC)
			return 0;
		if (errno != EEXIST)
			fatal("queue_commit_message_incoming: mkdir");
	}

	if (! bsnprintf(queuedir, MAXPATHLEN, "%s/%d/%s", PATH_QUEUE, hval,
		messagep->message_id))
		fatal("queue_commit_message_incoming: snprintf");
	

	if (rename(rootdir, queuedir) == -1)
		fatal("queue_commit_message_incoming: rename");

	return 1;
}

int
queue_open_incoming_message_file(struct message *messagep)
{
	char pathname[MAXPATHLEN];
	mode_t mode = O_CREAT|O_EXCL|O_RDWR;
	
	if (! bsnprintf(pathname, MAXPATHLEN, "%s/%s/message", PATH_INCOMING,
		messagep->message_id))
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
	FILE *fp;
	int ret;
	u_int16_t hval;

	if (! bsnprintf(queuedir, MAXPATHLEN, "%s/%s", PATH_QUEUE,
		messagep->message_id))
		fatal("queue_record_envelope: snprintf");

	hval = queue_message_hash(messagep);

	if (! bsnprintf(queuedir, MAXPATHLEN, "%s/%d", PATH_QUEUE, hval))
		fatal("queue_record_envelope: snprintf");

	if (! bsnprintf(evpdir, MAXPATHLEN, "%s/%s%s", queuedir,
		messagep->message_id, PATH_ENVELOPES))
		fatal("queue_record_envelope: snprintf");

	for (;;) {
		if (! bsnprintf(evpname, MAXPATHLEN, "%s/%s.%qu", evpdir,
			messagep->message_id, (u_int64_t)arc4random()))
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

	hval = queue_message_hash(messagep);

	if (! bsnprintf(pathname, MAXPATHLEN, "%s/%d/%s%s/%s", PATH_QUEUE,
		hval, messagep->message_id, PATH_ENVELOPES,
		messagep->message_uid))
		fatal("queue_remove_incoming_envelope: snprintf");

	if (unlink(pathname) == -1)
		fatal("queue_remove_incoming_envelope: unlink");

	if (! bsnprintf(pathname, MAXPATHLEN, "%s/%d/%s%s", PATH_QUEUE,
		hval, messagep->message_id, PATH_ENVELOPES))
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
	FILE *fp;
	mode_t mode = O_RDWR;
	u_int16_t hval;

	hval = queue_message_hash(messagep);

	if (! bsnprintf(pathname, MAXPATHLEN, "%s/%d/%s%s/%s", PATH_QUEUE,
		hval, messagep->message_id, PATH_ENVELOPES, messagep->message_uid))
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
	char pathname[MAXPATHLEN];
	u_int16_t hval;
	FILE *fp;
	char msgid[MAXPATHLEN];

	strlcpy(msgid, evpid, MAXPATHLEN);
	*strrchr(msgid, '.') = '\0';

	hval = hash(msgid, strlen(msgid)) % DIRHASH_BUCKETS;
	if (! bsnprintf(pathname, MAXPATHLEN, "%s/%d/%s%s/%s", PATH_QUEUE,
		hval, msgid, PATH_ENVELOPES, evpid))
		return 0;

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
	mode_t mode = O_RDONLY;
	u_int16_t hval;

	hval = hash(batchp->message_id, strlen(batchp->message_id)) % DIRHASH_BUCKETS;

	if (! bsnprintf(pathname, MAXPATHLEN, "%s/%d/%s/message", PATH_QUEUE,
		hval, batchp->message_id))
		fatal("queue_open_message_file: snprintf");

	if ((fd = open(pathname, mode)) == -1)
		fatal("queue_open_message_file: open");

	return fd;
}

void
queue_delete_message(char *msgid)
{
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];
	char msgpath[MAXPATHLEN];
	u_int16_t hval;

	hval = hash(msgid, strlen(msgid)) % DIRHASH_BUCKETS;
	if (! bsnprintf(rootdir, MAXPATHLEN, "%s/%d/%s", PATH_QUEUE,
		hval, msgid))
		fatal("queue_delete_message: snprintf");

	if (! bsnprintf(evpdir, MAXPATHLEN, "%s%s",
		rootdir, PATH_ENVELOPES))
		fatal("queue_delete_message: snprintf");
	
	if (! bsnprintf(msgpath, MAXPATHLEN, "%s/message", rootdir))
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

	if (! bsnprintf(rootdir, MAXPATHLEN, "%s/%d", PATH_QUEUE,
		hval))
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

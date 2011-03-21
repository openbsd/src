/*	$OpenBSD: queue_shared.c,v 1.36 2011/03/21 09:21:57 gilles Exp $	*/

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

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

#define	QWALK_AGAIN	0x1
#define	QWALK_RECURSE	0x2
#define	QWALK_RETURN	0x3

struct qwalk {
	char	  path[MAXPATHLEN];
	DIR	 *dirs[3];
	int	(*filefn)(struct qwalk *, char *);
	int	  bucket;
	int	  level;
	int	  strict;
};

int		walk_simple(struct qwalk *, char *);
int		walk_queue(struct qwalk *, char *);

void		display_envelope(struct message *, int);
void		getflag(u_int *, int, char *, char *, size_t);

int
queue_create_layout_message(char *queuepath, char *message_id)
{
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];

	if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%d.XXXXXXXXXXXXXXXX",
		queuepath, time(NULL)))
		fatalx("queue_create_layout_message: snprintf");

	if (mkdtemp(rootdir) == NULL) {
		if (errno == ENOSPC) {
			log_debug("FAILED WITH ENOSPC");
			bzero(message_id, MAX_ID_SIZE);
			return 0;
		}
		fatal("queue_create_layout_message: mkdtemp");
	}

	if (strlcpy(message_id, rootdir + strlen(queuepath) + 1, MAX_ID_SIZE)
	    >= MAX_ID_SIZE)
		fatalx("queue_create_layout_message: truncation");

	if (! bsnprintf(evpdir, sizeof(evpdir), "%s%s", rootdir,
		PATH_ENVELOPES))
		fatalx("queue_create_layout_message: snprintf");

	if (mkdir(evpdir, 0700) == -1) {
		if (errno == ENOSPC) {
			log_debug("FAILED WITH ENOSPC");
			rmdir(rootdir);
			bzero(message_id, MAX_ID_SIZE);
			return 0;
		}
		fatal("queue_create_layout_message: mkdir");
	}
	return 1;
}

void
queue_delete_layout_message(char *queuepath, char *msgid)
{
	char rootdir[MAXPATHLEN];
	char purgedir[MAXPATHLEN];

	if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%s", queuepath, msgid))
		fatalx("snprintf");

	if (! bsnprintf(purgedir, sizeof(purgedir), "%s/%s", PATH_PURGE, msgid))
		fatalx("snprintf");

	if (rename(rootdir, purgedir) == -1) {
		log_debug("ID: %s", msgid);
		log_debug("PATH: %s", rootdir);
		log_debug("PURGE: %s", purgedir);
		fatal("queue_delete_layout_message: rename");
	}
}

int
queue_record_layout_envelope(char *queuepath, struct message *message)
{
	char evpname[MAXPATHLEN];
	FILE *fp;
	int fd;

	fp = NULL;

again:
	if (! bsnprintf(evpname, sizeof(evpname), "%s/%s%s/%s.%qu", queuepath,
		message->message_id, PATH_ENVELOPES, message->message_id,
		(u_int64_t)arc4random()))
		fatalx("queue_record_incoming_envelope: snprintf");

	fd = open(evpname, O_WRONLY|O_CREAT|O_EXCL, 0600);
	if (fd == -1) {
		if (errno == EEXIST)
			goto again;
		if (errno == ENOSPC || errno == ENFILE)
			goto tempfail;
		fatal("queue_record_incoming_envelope: open");
	}

	fp = fdopen(fd, "w");
	if (fp == NULL)
		fatal("queue_record_incoming_envelope: fdopen");

	message->creation = time(NULL);
	if (strlcpy(message->message_uid, strrchr(evpname, '/') + 1,
	    sizeof(message->message_uid)) >= sizeof(message->message_uid))
		fatalx("queue_record_incoming_envelope: truncation");

	if (fwrite(message, sizeof (struct message), 1, fp) != 1) {
		if (errno == ENOSPC)
			goto tempfail;
		fatal("queue_record_incoming_envelope: write");
	}

	if (! safe_fclose(fp)) {
		fp = NULL;
		fd = -1;
		goto tempfail;
	}

	return 1;

tempfail:
	unlink(evpname);
	if (fp)
		fclose(fp);
	else if (fd != -1)
		close(fd);
	message->creation = 0;
	message->message_uid[0] = '\0';

	return 0;
}

int
queue_remove_layout_envelope(char *queuepath, struct message *message)
{
	char pathname[MAXPATHLEN];

	if (! bsnprintf(pathname, sizeof(pathname), "%s/%s%s/%s", queuepath,
		message->message_id, PATH_ENVELOPES, message->message_uid))
		fatal("queue_remove_incoming_envelope: snprintf");

	if (unlink(pathname) == -1)
		fatal("queue_remove_incoming_envelope: unlink");

	return 1;
}

int
queue_commit_layout_message(char *queuepath, struct message *messagep)
{
	char rootdir[MAXPATHLEN];
	char queuedir[MAXPATHLEN];
	
	if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%s", queuepath,
		messagep->message_id))
		fatal("queue_commit_message_incoming: snprintf");

	if (! bsnprintf(queuedir, sizeof(queuedir), "%s/%d", PATH_QUEUE,
		queue_hash(messagep->message_id)))
		fatal("queue_commit_message_incoming: snprintf");

	if (mkdir(queuedir, 0700) == -1) {
		if (errno == ENOSPC)
			return 0;
		if (errno != EEXIST)
			fatal("queue_commit_message_incoming: mkdir");
	}

	if (strlcat(queuedir, "/", sizeof(queuedir)) >= sizeof(queuedir) ||
	    strlcat(queuedir, messagep->message_id, sizeof(queuedir)) >=
	    sizeof(queuedir))
		fatalx("queue_commit_incoming_message: truncation");

	if (rename(rootdir, queuedir) == -1) {
		if (errno == ENOSPC)
			return 0;
		fatal("queue_commit_message_incoming: rename");
	}

	return 1;
}

int
queue_open_layout_messagefile(char *queuepath, struct message *messagep)
{
	char pathname[MAXPATHLEN];
	
	if (! bsnprintf(pathname, sizeof(pathname), "%s/%s/message", queuepath,
		messagep->message_id))
		fatal("queue_open_incoming_message_file: snprintf");

	return open(pathname, O_CREAT|O_EXCL|O_RDWR, 0600);
}

int
enqueue_create_layout(char *msgid)
{
	return queue_create_layout_message(PATH_ENQUEUE, msgid);
}

void
enqueue_delete_message(char *msgid)
{
	queue_delete_layout_message(PATH_ENQUEUE, msgid);
}

int
enqueue_record_envelope(struct message *message)
{
	return queue_record_layout_envelope(PATH_ENQUEUE, message);
}

int
enqueue_remove_envelope(struct message *message)
{
	return queue_remove_layout_envelope(PATH_ENQUEUE, message);
}

int
enqueue_commit_message(struct message *message)
{
	return queue_commit_layout_message(PATH_ENQUEUE, message);
}

int
enqueue_open_messagefile(struct message *message)
{
	return queue_open_layout_messagefile(PATH_ENQUEUE, message);
}

int
bounce_create_layout(char *msgid, struct message *message)
{
	char	msgpath[MAXPATHLEN];
	char	lnkpath[MAXPATHLEN];

	if (! queue_create_layout_message(PATH_BOUNCE, msgid))
		return 0;

	if (! bsnprintf(msgpath, sizeof(msgpath), "%s/%d/%s/message",
		PATH_QUEUE, queue_hash(message->message_id),
		message->message_id))
		return 0;

	if (! bsnprintf(lnkpath, sizeof(lnkpath), "%s/%s/message",
		PATH_BOUNCE, msgid))
		return 0;

	if (link(msgpath, lnkpath) == -1)
		fatal("link");

	return 1;
}

void
bounce_delete_message(char *msgid)
{
	queue_delete_layout_message(PATH_BOUNCE, msgid);
}

int
bounce_record_envelope(struct message *message)
{
	message->lasttry = 0;
	message->retry = 0;
	return queue_record_layout_envelope(PATH_BOUNCE, message);
}

int
bounce_remove_envelope(struct message *message)
{
	return queue_remove_layout_envelope(PATH_BOUNCE, message);
}

int
bounce_commit_message(struct message *message)
{
	return queue_commit_layout_message(PATH_BOUNCE, message);
}

int
bounce_record_message(struct message *messagep)
{
	char	msgid[MAX_ID_SIZE];
	struct message mbounce;

	if (messagep->type == T_BOUNCE_MESSAGE) {
		log_debug("mailer daemons loop detected !");
		return 0;
	}

	mbounce = *messagep;
	mbounce.type = T_BOUNCE_MESSAGE;
	mbounce.status &= ~S_MESSAGE_PERMFAILURE;

	if (! bounce_create_layout(msgid, messagep))
		return 0;

	strlcpy(mbounce.message_id, msgid, sizeof(mbounce.message_id));
	if (! bounce_record_envelope(&mbounce))
		return 0;

	return bounce_commit_message(&mbounce);
}

int
queue_create_incoming_layout(char *msgid)
{
	return queue_create_layout_message(PATH_INCOMING, msgid);
}

void
queue_delete_incoming_message(char *msgid)
{
	queue_delete_layout_message(PATH_INCOMING, msgid);
}

int
queue_record_incoming_envelope(struct message *message)
{
	return queue_record_layout_envelope(PATH_INCOMING, message);
}

int
queue_remove_incoming_envelope(struct message *message)
{
	return queue_remove_layout_envelope(PATH_INCOMING, message);
}

int
queue_commit_incoming_message(struct message *message)
{
	return queue_commit_layout_message(PATH_INCOMING, message);
}

int
queue_open_incoming_message_file(struct message *message)
{
	return queue_open_layout_messagefile(PATH_INCOMING, message);
}

int
queue_open_message_file(char *msgid)
{
	int fd;
	char pathname[MAXPATHLEN];
	u_int16_t hval;

	hval = queue_hash(msgid);

	if (! bsnprintf(pathname, sizeof(pathname), "%s/%d/%s/message",
		PATH_QUEUE, hval, msgid))
		fatal("queue_open_message_file: snprintf");

	if ((fd = open(pathname, O_RDONLY)) == -1)
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

	hval = queue_hash(msgid);
	if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%d/%s", PATH_QUEUE,
		hval, msgid))
		fatal("queue_delete_message: snprintf");

	if (! bsnprintf(evpdir, sizeof(evpdir), "%s%s", rootdir,
		PATH_ENVELOPES))
		fatal("queue_delete_message: snprintf");
	
	if (! bsnprintf(msgpath, sizeof(msgpath), "%s/message", rootdir))
		fatal("queue_delete_message: snprintf");

	if (unlink(msgpath) == -1)
		fatal("queue_delete_message: unlink");

	if (rmdir(evpdir) == -1) {
		/* It is ok to fail rmdir with ENOENT here
		 * because upon successful delivery of the
		 * last envelope, we remove the directory.
		 */
		if (errno != ENOENT)
			fatal("queue_delete_message: rmdir");
	}

	if (rmdir(rootdir) == -1)
		fatal("#2 queue_delete_message: rmdir");

	if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%d", PATH_QUEUE, hval))
		fatal("queue_delete_message: snprintf");

	rmdir(rootdir);

	return;
}

void
queue_message_update(struct message *messagep)
{
	messagep->flags &= ~F_MESSAGE_PROCESSING;
	messagep->status &= ~(S_MESSAGE_ACCEPTED|S_MESSAGE_REJECTED);
	messagep->batch_id = 0;
	messagep->retry++;

	if (messagep->status & S_MESSAGE_PERMFAILURE) {
		if (messagep->type != T_BOUNCE_MESSAGE &&
		    messagep->sender.user[0] != '\0')
			bounce_record_message(messagep);
		queue_remove_envelope(messagep);
		return;
	}

	if (messagep->status & S_MESSAGE_TEMPFAILURE) {
		messagep->status &= ~S_MESSAGE_TEMPFAILURE;
		queue_update_envelope(messagep);
		return;
	}

	/* no error, remove envelope */
	queue_remove_envelope(messagep);
}

int
queue_remove_envelope(struct message *messagep)
{
	char pathname[MAXPATHLEN];
	u_int16_t hval;

	hval = queue_hash(messagep->message_id);

	if (! bsnprintf(pathname, sizeof(pathname), "%s/%d/%s%s/%s",
		PATH_QUEUE, hval, messagep->message_id, PATH_ENVELOPES,
		messagep->message_uid))
		fatal("queue_remove_envelope: snprintf");

	if (unlink(pathname) == -1)
		fatal("queue_remove_envelope: unlink");

	if (! bsnprintf(pathname, sizeof(pathname), "%s/%d/%s%s", PATH_QUEUE,
		hval, messagep->message_id, PATH_ENVELOPES))
		fatal("queue_remove_envelope: snprintf");

	if (rmdir(pathname) != -1)
		queue_delete_message(messagep->message_id);

	return 1;
}

int
queue_update_envelope(struct message *messagep)
{
	char temp[MAXPATHLEN];
	char dest[MAXPATHLEN];
	FILE *fp;
	u_int64_t batch_id;

	batch_id = messagep->batch_id;
	messagep->batch_id = 0;

	if (! bsnprintf(temp, sizeof(temp), "%s/envelope.tmp", PATH_QUEUE))
		fatalx("queue_update_envelope");

	if (! bsnprintf(dest, sizeof(dest), "%s/%d/%s%s/%s", PATH_QUEUE,
		queue_hash(messagep->message_id), messagep->message_id,
		PATH_ENVELOPES, messagep->message_uid))
		fatal("queue_update_envelope: snprintf");

	fp = fopen(temp, "w");
	if (fp == NULL) {
		if (errno == ENOSPC || errno == ENFILE)
			goto tempfail;
		fatal("queue_update_envelope: open");
	}
	if (fwrite(messagep, sizeof(struct message), 1, fp) != 1) {
		if (errno == ENOSPC)
			goto tempfail;
		fatal("queue_update_envelope: fwrite");
	}
	if (! safe_fclose(fp))
		goto tempfail;

	if (rename(temp, dest) == -1) {
		if (errno == ENOSPC)
			goto tempfail;
		fatal("queue_update_envelope: rename");
	}

	messagep->batch_id = batch_id;
	return 1;

tempfail:
	if (unlink(temp) == -1)
		fatal("queue_update_envelope: unlink");
	if (fp)
		fclose(fp);

	messagep->batch_id = batch_id;
	return 0;
}

int
queue_load_envelope(struct message *messagep, char *evpid)
{
	char pathname[MAXPATHLEN];
	char msgid[MAX_ID_SIZE];
	FILE *fp;

	if (strlcpy(msgid, evpid, sizeof(msgid)) >= sizeof(msgid))
		fatalx("queue_load_envelope: truncation");
	*strrchr(msgid, '.') = '\0';

	if (! bsnprintf(pathname, sizeof(pathname), "%s/%d/%s%s/%s", PATH_QUEUE,
		queue_hash(msgid), msgid, PATH_ENVELOPES, evpid))
		fatalx("queue_load_envelope: snprintf");

	fp = fopen(pathname, "r");
	if (fp == NULL) {
		if (errno == ENOENT || errno == ENFILE)
			return 0;
		fatal("queue_load_envelope: fopen");
	}
	if (fread(messagep, sizeof(struct message), 1, fp) != 1)
		fatal("queue_load_envelope: fread");
	fclose(fp);

	return 1;
}

u_int16_t
queue_hash(char *msgid)
{
	u_int16_t	h;

	for (h = 5381; *msgid; msgid++)
		h = ((h << 5) + h) + *msgid;

	return (h % DIRHASH_BUCKETS);
}

struct qwalk *
qwalk_new(char *path)
{
	struct qwalk *q;

	q = calloc(1, sizeof(struct qwalk));
	if (q == NULL)
		fatal("qwalk_new: calloc");

	strlcpy(q->path, path, sizeof(q->path));

	q->level = 0;
	q->strict = 0;
	q->filefn = walk_simple;

	if (smtpd_process == PROC_QUEUE || smtpd_process == PROC_RUNNER)
		q->strict = 1;

	if (strcmp(path, PATH_QUEUE) == 0)
		q->filefn = walk_queue;

	q->dirs[0] = opendir(q->path);
	if (q->dirs[0] == NULL)
		fatal("qwalk_new: opendir");

	return (q);
}

int
qwalk(struct qwalk *q, char *filepath)
{
	struct dirent	*dp;

again:
	errno = 0;
	dp = readdir(q->dirs[q->level]);
	if (errno)
		fatal("qwalk: readdir");
	if (dp == NULL) {
		closedir(q->dirs[q->level]);
		q->dirs[q->level] = NULL;
		if (q->level == 0)
			return (0);
		q->level--;
		goto again;
	}

	if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
		goto again;

	switch (q->filefn(q, dp->d_name)) {
	case QWALK_AGAIN:
		goto again;
	case QWALK_RECURSE:
		goto recurse;
	case QWALK_RETURN:
		if (! bsnprintf(filepath, MAXPATHLEN, "%s/%s", q->path,
			dp->d_name))
			fatalx("qwalk: snprintf");
		return (1);
	default:
		fatalx("qwalk: callback failed");
	}

recurse:
	q->level++;
	q->dirs[q->level] = opendir(q->path);
	if (q->dirs[q->level] == NULL) {
		if (errno == ENOENT && !q->strict) {
			q->level--;
			goto again;
		}
		fatal("qwalk: opendir");
	}
	goto again;
}

void
qwalk_close(struct qwalk *q)
{
	int i;

	for (i = 0; i <= q->level; i++)
		if (q->dirs[i])
			closedir(q->dirs[i]);

	bzero(q, sizeof(struct qwalk));
	free(q);
}

int
walk_simple(struct qwalk *q, char *fname)
{
	return (QWALK_RETURN);
}

int
walk_queue(struct qwalk *q, char *fname)
{
	const char	*errstr;

	switch (q->level) {
	case 0:
		if (strcmp(fname, "envelope.tmp") == 0)
			return (QWALK_AGAIN);
		q->bucket = strtonum(fname, 0, DIRHASH_BUCKETS - 1, &errstr);
		if (errstr) {
			log_warnx("walk_queue: invalid bucket: %s", fname);
			return (QWALK_AGAIN);
		}
		if (! bsnprintf(q->path, sizeof(q->path), "%s/%d", PATH_QUEUE,
			q->bucket))
			fatalx("walk_queue: snprintf");
		return (QWALK_RECURSE);
	case 1:
		if (! bsnprintf(q->path, sizeof(q->path), "%s/%d/%s%s",
			PATH_QUEUE, q->bucket, fname, PATH_ENVELOPES))
			fatalx("walk_queue: snprintf");
		return (QWALK_RECURSE);
	case 2:
		return (QWALK_RETURN);
	}

	return (-1);
}

void
show_queue(char *queuepath, int flags)
{
	char		 path[MAXPATHLEN];
	struct message	 message;
	struct qwalk	*q;
	FILE		*fp;

	log_init(1);

	if (chroot(PATH_SPOOL) == -1 || chdir(".") == -1)
		err(1, "%s", PATH_SPOOL);

	q = qwalk_new(queuepath);

	while (qwalk(q, path)) {
		fp = fopen(path, "r");
		if (fp == NULL) {
			if (errno == ENOENT)
				continue;
			err(1, "%s", path);
		}

		errno = 0;
		if (fread(&message, sizeof(struct message), 1, fp) != 1)
			err(1, "%s", path);
		fclose(fp);

		display_envelope(&message, flags);
	}

	qwalk_close(q);
}

void
display_envelope(struct message *envelope, int flags)
{
	char	 status[128];

	status[0] = '\0';

	getflag(&envelope->status, S_MESSAGE_TEMPFAILURE, "TEMPFAIL",
	    status, sizeof(status));

	if (envelope->status)
		errx(1, "%s: unexpected status 0x%04x", envelope->message_uid,
		    envelope->status);

	getflag(&envelope->flags, F_MESSAGE_BOUNCE, "BOUNCE",
	    status, sizeof(status));
	getflag(&envelope->flags, F_MESSAGE_AUTHENTICATED, "AUTH",
	    status, sizeof(status));
	getflag(&envelope->flags, F_MESSAGE_PROCESSING, "PROCESSING",
	    status, sizeof(status));
	getflag(&envelope->flags, F_MESSAGE_SCHEDULED, "SCHEDULED",
	    status, sizeof(status));
	getflag(&envelope->flags, F_MESSAGE_ENQUEUED, "ENQUEUED",
	    status, sizeof(status));
	getflag(&envelope->flags, F_MESSAGE_FORCESCHEDULE, "SCHEDULED_MANUAL",
	    status, sizeof(status));

	if (envelope->flags)
		errx(1, "%s: unexpected flags 0x%04x", envelope->message_uid,
		    envelope->flags);
	
	if (status[0])
		status[strlen(status) - 1] = '\0';
	else
		strlcpy(status, "-", sizeof(status));

	switch (envelope->type) {
	case T_MDA_MESSAGE:
		printf("MDA");
		break;
	case T_MTA_MESSAGE:
		printf("MTA");
		break;
	case T_BOUNCE_MESSAGE:
		printf("BOUNCE");
		break;
	default:
		printf("UNKNOWN");
	}
	
	printf("|%s|%s|%s@%s|%s@%s|%d|%d|%u",
	    envelope->message_uid,
	    status,
	    envelope->sender.user, envelope->sender.domain,
	    envelope->recipient.user, envelope->recipient.domain,
	    envelope->lasttry,
	    envelope->expire,
	    envelope->retry);
	
	if (envelope->session_errorline[0] != '\0')
		printf("|%s", envelope->session_errorline);

	printf("\n");
}

void
getflag(u_int *bitmap, int bit, char *bitstr, char *buf, size_t len)
{
	if (*bitmap & bit) {
		*bitmap &= ~bit;
		strlcat(buf, bitstr, len);
		strlcat(buf, ",", len);
	}
}

/*	$OpenBSD: queue_shared.c,v 1.4 2009/01/28 17:29:11 jacekm Exp $	*/

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

#include <dirent.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

int
queue_create_layout_message(char *queuepath, char *message_id)
{
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];

	if (! bsnprintf(rootdir, MAXPATHLEN, "%s/%d.XXXXXXXXXXXXXXXX",
		queuepath, time(NULL)))
		fatalx("queue_create_layout_message: snprintf");

	if (mkdtemp(rootdir) == NULL) {
		if (errno == ENOSPC)
			return 0;
		fatal("queue_create_layout_message: mkdtemp");
	}

	if (strlcpy(message_id, rootdir + strlen(queuepath) + 1, MAX_ID_SIZE)
	    >= MAX_ID_SIZE)
		fatalx("queue_create_layout_message: truncation");

	if (! bsnprintf(evpdir, MAXPATHLEN, "%s%s", rootdir, PATH_ENVELOPES))
		fatalx("queue_create_layout_message: snprintf");

	if (mkdir(evpdir, 0700) == -1) {
		if (errno == ENOSPC) {
			rmdir(rootdir);
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

	if (! bsnprintf(rootdir, MAXPATHLEN, "%s/%s", queuepath, msgid))
		fatalx("snprintf");

	if (! bsnprintf(purgedir, MAXPATHLEN, "%s/%s", PATH_PURGE, msgid))
		fatalx("snprintf");

	if (rename(rootdir, purgedir) == -1) {
		fatal("queue_delete_layout_message: rename");
	}
}

int
queue_record_layout_envelope(char *queuepath, struct message *message)
{
	char evpname[MAXPATHLEN];
	FILE *fp;
	int fd;

again:
	if (! bsnprintf(evpname, MAXPATHLEN, "%s/%s%s/%s.%qu", queuepath,
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
	if (strlcpy(message->message_uid, strrchr(evpname, '/') + 1, MAX_ID_SIZE)
	    >= MAX_ID_SIZE)
		fatalx("queue_record_incoming_envelope: truncation");

	if (fwrite(message, sizeof (struct message), 1, fp) != 1) {
		if (errno == ENOSPC)
			goto tempfail;
		fatal("queue_record_incoming_envelope: write");
	}

	if (! safe_fclose(fp))
		goto tempfail;

	return 1;

tempfail:
	unlink(evpname);
	close(fd);
	message->creation = 0;
	message->message_uid[0] = '\0';

	return 0;
}

int
queue_remove_layout_envelope(char *queuepath, struct message *message)
{
	char pathname[MAXPATHLEN];

	if (! bsnprintf(pathname, MAXPATHLEN, "%s/%s%s/%s", queuepath,
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
	
	if (! bsnprintf(rootdir, MAXPATHLEN, "%s/%s", queuepath,
		messagep->message_id))
		fatal("queue_commit_message_incoming: snprintf");

	if (! bsnprintf(queuedir, MAXPATHLEN, "%s/%d", PATH_QUEUE,
		queue_hash(messagep->message_id)))
		fatal("queue_commit_message_incoming: snprintf");

	if (mkdir(queuedir, 0700) == -1) {
		if (errno == ENOSPC)
			return 0;
		if (errno != EEXIST)
			fatal("queue_commit_message_incoming: mkdir");
	}

	if (strlcat(queuedir, "/", MAXPATHLEN) >= MAXPATHLEN ||
	    strlcat(queuedir, messagep->message_id, MAXPATHLEN) >= MAXPATHLEN)
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
	mode_t mode = O_CREAT|O_EXCL|O_RDWR;
	
	if (! bsnprintf(pathname, MAXPATHLEN, "%s/%s/message", queuepath,
		messagep->message_id))
		fatal("queue_open_incoming_message_file: snprintf");

	return open(pathname, mode, 0600);
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
	mode_t mode = O_RDONLY;
	u_int16_t hval;

	hval = queue_hash(msgid);

	if (! bsnprintf(pathname, MAXPATHLEN, "%s/%d/%s/message", PATH_QUEUE,
		hval, msgid))
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

	hval = queue_hash(msgid);
	if (! bsnprintf(rootdir, MAXPATHLEN, "%s/%d/%s", PATH_QUEUE,
		hval, msgid))
		fatal("queue_delete_message: snprintf");

	if (! bsnprintf(evpdir, MAXPATHLEN, "%s%s",
		rootdir, PATH_ENVELOPES))
		fatal("queue_delete_message: snprintf");
	
	if (! bsnprintf(msgpath, MAXPATHLEN, "%s/message", rootdir))
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

	if (! bsnprintf(rootdir, MAXPATHLEN, "%s/%d", PATH_QUEUE,
		hval))
		fatal("queue_delete_message: snprintf");

	rmdir(rootdir);

	return;
}

void
queue_message_update(struct message *messagep)
{
	messagep->flags &= ~F_MESSAGE_PROCESSING;
	messagep->batch_id = 0;
	messagep->retry++;

	if (messagep->status & S_MESSAGE_PERMFAILURE) {
		if (messagep->type & T_DAEMON_MESSAGE)
			queue_remove_envelope(messagep);
		else {
			messagep->id = queue_generate_id();
			messagep->type |= T_DAEMON_MESSAGE;
			messagep->status &= ~S_MESSAGE_PERMFAILURE;
			messagep->lasttry = 0;
			messagep->retry = 0;
			messagep->creation = time(NULL);
			queue_update_envelope(messagep);
		}
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

	if (! bsnprintf(pathname, MAXPATHLEN, "%s/%d/%s%s/%s", PATH_QUEUE,
		hval, messagep->message_id, PATH_ENVELOPES,
		messagep->message_uid))
		fatal("queue_remove_envelope: snprintf");

	if (unlink(pathname) == -1)
		fatal("queue_remove_envelope: unlink");

	if (! bsnprintf(pathname, MAXPATHLEN, "%s/%d/%s%s", PATH_QUEUE,
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

	if (! bsnprintf(temp, MAXPATHLEN, "%s/envelope.tmp", PATH_INCOMING))
		fatalx("queue_update_envelope");

	if (! bsnprintf(dest, MAXPATHLEN, "%s/%d/%s%s/%s", PATH_QUEUE,
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

	return 1;

tempfail:
	if (unlink(temp) == -1)
		fatal("queue_update_envelope: unlink");
	if (fp)
		fclose(fp);

	return 0;
}

int
queue_load_envelope(struct message *messagep, char *evpid)
{
	char pathname[MAXPATHLEN];
	char msgid[MAX_ID_SIZE];
	FILE *fp;

	if (strlcpy(msgid, evpid, MAX_ID_SIZE) >= MAX_ID_SIZE)
		fatalx("queue_load_envelope: truncation");
	*strrchr(msgid, '.') = '\0';

	if (! bsnprintf(pathname, MAXPATHLEN, "%s/%d/%s%s/%s", PATH_QUEUE,
		queue_hash(msgid), msgid, PATH_ENVELOPES, evpid))
		fatalx("queue_load_envelope: snprintf");

	fp = fopen(pathname, "r");
	if (fp == NULL) {
		if (errno == ENOSPC || errno == ENFILE)
			return 0;
		fatal("queue_load_envelope: fopen");
	}
	if (fread(messagep, sizeof(struct message), 1, fp) != 1)
		fatal("queue_load_envelope: fread");
	fclose(fp);

	return 1;
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

u_int16_t
queue_hash(char *msgid)
{
	u_int16_t	h;

	for (h = 5381; *msgid; msgid++)
		h = ((h << 5) + h) + *msgid;

	return (h % DIRHASH_BUCKETS);
}

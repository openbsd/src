/*	$OpenBSD: queue_shared.c,v 1.2 2009/01/28 11:27:57 gilles Exp $	*/

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
		if (errno == ENOENT)
			return;
		fatal("queue_delete_incoming_message: rename");
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
		if (errno != ENOENT)
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

u_int16_t
queue_hash(char *msgid)
{
	u_int16_t	h;

	for (h = 5381; *msgid; msgid++)
		h = ((h << 5) + h) + *msgid;

	return (h % DIRHASH_BUCKETS);
}

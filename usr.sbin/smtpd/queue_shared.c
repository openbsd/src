/*	$OpenBSD: queue_shared.c,v 1.53 2011/10/27 14:32:57 chl Exp $	*/

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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

#define	QWALK_AGAIN	0x1
#define	QWALK_RECURSE	0x2
#define	QWALK_RETURN	0x3

int	fsqueue_load_envelope_ascii(FILE *, struct envelope *);

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

void		display_envelope(struct envelope *, int);
void		getflag(u_int *, int, char *, char *, size_t);

int
bounce_record_message(struct envelope *e, struct envelope *bounce)
{
	u_int32_t msgid;

	bzero(bounce, sizeof(*bounce));

	if (e->type == D_BOUNCE) {
		log_debug("mailer daemons loop detected !");
		return 0;
	}

	*bounce = *e;
	 bounce->type = D_BOUNCE;
	 bounce->status &= ~DS_PERMFAILURE;

	msgid = evpid_to_msgid(e->id);
	if (! queue_message_create(Q_BOUNCE, &msgid))
		return 0;

	bounce->id = msgid_to_evpid(msgid);
	if (! queue_envelope_create(Q_BOUNCE, bounce))
		return 0;

	return queue_message_commit(Q_BOUNCE, msgid);
}

void
queue_message_update(struct envelope *e)
{
	e->batch_id = 0;
	e->status &= ~(DS_ACCEPTED|DS_REJECTED);
	e->retry++;


	if (e->status & DS_PERMFAILURE) {
		if (e->type != D_BOUNCE &&
		    e->sender.user[0] != '\0') {
			struct envelope bounce;

			bounce_record_message(e, &bounce);
		}
		queue_envelope_delete(Q_QUEUE, e);
		return;
	}

	if (e->status & DS_TEMPFAILURE) {
		e->status &= ~DS_TEMPFAILURE;
		queue_envelope_update(Q_QUEUE, e);
		return;
	}

	/* no error, remove envelope */
	queue_envelope_delete(Q_QUEUE, e);
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
	char	*ep;

	switch (q->level) {
	case 0:
		if (strcmp(fname, "envelope.tmp") == 0)
			return (QWALK_AGAIN);

		q->bucket = strtoul(fname, &ep, 16);
		if (fname[0] == '\0' || *ep != '\0') {
			log_warnx("walk_queue: invalid bucket: %s", fname);
			return (QWALK_AGAIN);
		}
		if (errno == ERANGE || q->bucket >= DIRHASH_BUCKETS) {
			log_warnx("walk_queue: invalid bucket: %s", fname);
			return (QWALK_AGAIN);
		}
		if (! bsnprintf(q->path, sizeof(q->path), "%s/%03x", PATH_QUEUE,
			q->bucket & 0xfff))
			fatalx("walk_queue: snprintf");
		return (QWALK_RECURSE);
	case 1:
		if (! bsnprintf(q->path, sizeof(q->path), "%s/%03x/%s%s",
			PATH_QUEUE, q->bucket & 0xfff, fname, PATH_ENVELOPES))
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
	struct envelope	 message;
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
		if (! fsqueue_load_envelope_ascii(fp, &message))
			err(1, "%s", path);
		fclose(fp);

		display_envelope(&message, flags);
	}

	qwalk_close(q);
}

void
display_envelope(struct envelope *e, int flags)
{
	char	 status[128];

	status[0] = '\0';

	getflag(&e->status, DS_TEMPFAILURE, "TEMPFAIL",
	    status, sizeof(status));

	if (e->status)
		errx(1, "%016" PRIx64 ": unexpected status 0x%04x", e->id,
		    e->status);

	getflag(&e->flags, DF_BOUNCE, "BOUNCE",
	    status, sizeof(status));
	getflag(&e->flags, DF_AUTHENTICATED, "AUTH",
	    status, sizeof(status));
	getflag(&e->flags, DF_ENQUEUED, "ENQUEUED",
	    status, sizeof(status));
	getflag(&e->flags, DF_INTERNAL, "INTERNAL",
	    status, sizeof(status));

	if (e->flags)
		errx(1, "%016" PRIx64 ": unexpected flags 0x%04x", e->id,
		    e->flags);
	
	if (status[0])
		status[strlen(status) - 1] = '\0';
	else
		strlcpy(status, "-", sizeof(status));

	switch (e->type) {
	case D_MDA:
		printf("MDA");
		break;
	case D_MTA:
		printf("MTA");
		break;
	case D_BOUNCE:
		printf("BOUNCE");
		break;
	default:
		printf("UNKNOWN");
	}
	
	printf("|%016" PRIx64 "|%s|%s@%s|%s@%s|%" PRId64 "|%" PRId64 "|%u",
	    e->id,
	    status,
	    e->sender.user, e->sender.domain,
	    e->dest.user, e->dest.domain,
	    (int64_t) e->lasttry,
	    (int64_t) e->expire,
	    e->retry);
	
	if (e->errorline[0] != '\0')
		printf("|%s", e->errorline);

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

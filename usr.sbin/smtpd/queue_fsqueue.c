/*	$OpenBSD: queue_fsqueue.c,v 1.19 2011/11/15 23:06:39 gilles Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@openbsd.org>
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
#include <libgen.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"
#include "queue_fsqueue.h"

static char		*fsqueue_getpath(enum queue_kind);

static int	fsqueue_envelope_load(enum queue_kind, struct envelope *);
static int	fsqueue_envelope_update(enum queue_kind, struct envelope *);
static int	fsqueue_envelope_delete(enum queue_kind, struct envelope *);

static int	fsqueue_message_create(enum queue_kind, u_int32_t *);
static int	fsqueue_message_commit(enum queue_kind, u_int32_t);
static int	fsqueue_message_fd_r(enum queue_kind, u_int32_t);
static int	fsqueue_message_fd_rw(enum queue_kind, u_int32_t);
static int	fsqueue_message_delete(enum queue_kind, u_int32_t);
static int	fsqueue_message_purge(enum queue_kind, u_int32_t);
static int	fsqueue_message_corrupt(enum queue_kind, u_int32_t);

int	fsqueue_init(void);
int	fsqueue_message(enum queue_kind, enum queue_op, u_int32_t *);
int	fsqueue_envelope(enum queue_kind, enum queue_op , struct envelope *);
int	fsqueue_load_envelope_ascii(FILE *, struct envelope *);
int	fsqueue_dump_envelope_ascii(FILE *, struct envelope *);

void   *fsqueue_qwalk_new(enum queue_kind, u_int32_t);
int	fsqueue_qwalk(void *, u_int64_t *);
void	fsqueue_qwalk_close(void *);

static char *
fsqueue_getpath(enum queue_kind kind)
{
        switch (kind) {
        case Q_INCOMING:
                return (PATH_INCOMING);

        case Q_ENQUEUE:
                return (PATH_ENQUEUE);

        case Q_QUEUE:
                return (PATH_QUEUE);

        case Q_PURGE:
                return (PATH_PURGE);

        case Q_BOUNCE:
                return (PATH_BOUNCE);

        case Q_CORRUPT:
                return (PATH_CORRUPT);

        default:
		fatalx("queue_fsqueue_getpath: unsupported queue kind.");
        }
	return NULL;
}

static int
fsqueue_envelope_create(enum queue_kind qkind, struct envelope *ep)
{
	char evpname[MAXPATHLEN];
	FILE *fp;
	int fd;
	u_int64_t evpid;

	fp = NULL;

again:
	evpid = queue_generate_evpid(ep->id);

	if (! bsnprintf(evpname, sizeof(evpname), "%s/%08x%s/%016" PRIx64,
		fsqueue_getpath(qkind),
		evpid_to_msgid(evpid),
		PATH_ENVELOPES, evpid))
		fatalx("fsqueue_envelope_create: snprintf");

	fd = open(evpname, O_WRONLY|O_CREAT|O_EXCL, 0600);
	if (fd == -1) {
		if (errno == EEXIST)
			goto again;
		if (errno == ENOSPC || errno == ENFILE)
			goto tempfail;
		fatal("fsqueue_envelope_create: open");
	}

	fp = fdopen(fd, "w");
	if (fp == NULL)
		fatal("fsqueue_envelope_create: fdopen");
 
	ep->creation = time(NULL);
	ep->id = evpid;

	if (qkind == Q_BOUNCE) {
		ep->lasttry = 0;
		ep->retry = 0;
	}

	if (! fsqueue_dump_envelope_ascii(fp, ep)) {
		if (errno == ENOSPC)
			goto tempfail;
		fatal("fsqueue_dump_envelope_ascii: write");
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
	ep->creation = 0;
	ep->id = 0;

	return 0;
}

static int
fsqueue_envelope_load(enum queue_kind qkind, struct envelope *ep)
{
	char pathname[MAXPATHLEN];
	FILE *fp;
	int  ret;

	if (! bsnprintf(pathname, sizeof(pathname), "%s/%03x/%08x%s/%016" PRIx64,
		fsqueue_getpath(qkind),
		evpid_to_msgid(ep->id) & 0xfff,
		evpid_to_msgid(ep->id),
		PATH_ENVELOPES, ep->id))
		fatalx("fsqueue_envelope_load: snprintf");

	fp = fopen(pathname, "r");
	if (fp == NULL) {
		if (errno == ENOENT || errno == ENFILE)
			return 0;
		fatal("fsqueue_envelope_load: fopen");
	}
	ret = fsqueue_load_envelope_ascii(fp, ep);

	fclose(fp);

	return ret;
}

static int
fsqueue_envelope_update(enum queue_kind qkind, struct envelope *ep)
{
	char temp[MAXPATHLEN];
	char dest[MAXPATHLEN];
	FILE *fp;

	if (! bsnprintf(temp, sizeof(temp), "%s/envelope.tmp", PATH_QUEUE))
		fatalx("fsqueue_envelope_update");

	if (! bsnprintf(dest, sizeof(dest), "%s/%03x/%08x%s/%016" PRIx64,
		fsqueue_getpath(qkind),
		evpid_to_msgid(ep->id) & 0xfff,
		evpid_to_msgid(ep->id),
		PATH_ENVELOPES, ep->id))
		fatal("fsqueue_envelope_update: snprintf");

	fp = fopen(temp, "w");
	if (fp == NULL) {
		if (errno == ENOSPC || errno == ENFILE)
			goto tempfail;
		fatal("fsqueue_envelope_update: open");
	}
	if (! fsqueue_dump_envelope_ascii(fp, ep)) {
		if (errno == ENOSPC)
			goto tempfail;
		fatal("fsqueue_dump_envelope_ascii: fwrite");
	}
	if (! safe_fclose(fp))
		goto tempfail;

	if (rename(temp, dest) == -1) {
		if (errno == ENOSPC)
			goto tempfail;
		fatal("fsqueue_envelope_update: rename");
	}

	return 1;

tempfail:
	if (unlink(temp) == -1)
		fatal("fsqueue_envelope_update: unlink");
	if (fp)
		fclose(fp);

	return 0;
}

static int
fsqueue_envelope_delete(enum queue_kind qkind, struct envelope *ep)
{
	char pathname[MAXPATHLEN];

	if (! bsnprintf(pathname, sizeof(pathname), "%s/%03x/%08x%s/%016" PRIx64,
		fsqueue_getpath(qkind),
		evpid_to_msgid(ep->id) & 0xfff,
		evpid_to_msgid(ep->id),
		PATH_ENVELOPES,
		ep->id))
		fatal("fsqueue_envelope_delete: snprintf");

	if (unlink(pathname) == -1) {
		log_debug("######: %s [errno: %d]", pathname, errno);
		fatal("fsqueue_envelope_delete: unlink");
	}

	if (! bsnprintf(pathname, sizeof(pathname), "%s/%03x/%08x%s", PATH_QUEUE,
		evpid_to_msgid(ep->id) & 0xfff,
		evpid_to_msgid(ep->id), PATH_ENVELOPES))
		fatal("fsqueue_envelope_delete: snprintf");

	if (rmdir(pathname) != -1)
		fsqueue_message_delete(qkind, evpid_to_msgid(ep->id));

	return 1;
}

static int
fsqueue_message_create(enum queue_kind qkind, u_int32_t *msgid)
{
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];
	char *queuepath = fsqueue_getpath(qkind);
	char msgpath[MAXPATHLEN];
	char lnkpath[MAXPATHLEN];
	u_int32_t msgid_save;

	msgid_save = *msgid;

again:
	*msgid = queue_generate_msgid();
	if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%08x",
		queuepath, *msgid))
		fatalx("fsqueue_message_create: snprintf");

	if (mkdir(rootdir, 0700) == -1) {
		if (errno == EEXIST)
			goto again;

		if (errno == ENOSPC) {
			*msgid = 0;
			return 0;
		}
		fatal("fsqueue_message_create: mkdir");
	}

	if (! bsnprintf(evpdir, sizeof(evpdir), "%s%s", rootdir,
		PATH_ENVELOPES))
		fatalx("fsqueue_message_create: snprintf");

	if (mkdir(evpdir, 0700) == -1) {
		if (errno == ENOSPC) {
			rmdir(rootdir);
			*msgid = 0;
			return 0;
		}
		fatal("fsqueue_message_create: mkdir");
	}

	if (qkind == Q_BOUNCE) {
		if (! bsnprintf(msgpath, sizeof(msgpath), "%s/%03x/%08x/message",
			fsqueue_getpath(Q_QUEUE),
			msgid_save & 0xfff,
			msgid_save))
			return 0;

		if (! bsnprintf(lnkpath, sizeof(lnkpath), "%s/%08x/message",
			fsqueue_getpath(Q_BOUNCE), *msgid))
			return 0;
		
		if (link(msgpath, lnkpath) == -1)
			fatal("link");
	}

	return 1;
}

static int
fsqueue_message_commit(enum queue_kind qkind, u_int32_t msgid)
{
	char rootdir[MAXPATHLEN];
	char queuedir[MAXPATHLEN];
	char msgdir[MAXPATHLEN];
	
	if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%08x",
		fsqueue_getpath(qkind), msgid))
		fatal("fsqueue_message_commit: snprintf");

	if (! bsnprintf(queuedir, sizeof(queuedir), "%s/%03x",
		fsqueue_getpath(Q_QUEUE), msgid & 0xfff))
		fatal("fsqueue_message_commit: snprintf");

	if (mkdir(queuedir, 0700) == -1) {
		if (errno == ENOSPC)
			return 0;
		if (errno != EEXIST)
			fatal("fsqueue_message_commit: mkdir");
	}

	if (! bsnprintf(msgdir, sizeof(msgdir),"%s/%08x",
		queuedir, msgid))
		fatal("fsqueue_message_commit: snprintf");

	if (rename(rootdir, msgdir) == -1) {
		if (errno == ENOSPC)
			return 0;
		fatal("fsqueue_message_commit: rename");
	}

	return 1;
}

static int
fsqueue_message_fd_r(enum queue_kind qkind, u_int32_t msgid)
{
	int fd;
	char pathname[MAXPATHLEN];

	if (qkind == Q_ENQUEUE || qkind == Q_INCOMING) {
		if (! bsnprintf(pathname, sizeof(pathname), "%s/%08x/message",
			fsqueue_getpath(qkind), msgid))
			fatal("fsqueue_message_fd_r: snprintf");
	}
	else {
		if (! bsnprintf(pathname, sizeof(pathname), "%s/%03x/%08x/message",
			fsqueue_getpath(qkind), msgid & 0xfff, msgid))
			fatal("fsqueue_message_fd_r: snprintf");
	}

	if ((fd = open(pathname, O_RDONLY)) == -1)
		fatal("fsqueue_message_fd_r: open");

	return fd;
}

static int
fsqueue_message_fd_rw(enum queue_kind qkind, u_int32_t msgid)
{
	char pathname[MAXPATHLEN];
	
	if (! bsnprintf(pathname, sizeof(pathname), "%s/%08x/message",
		fsqueue_getpath(qkind),
		msgid))
		fatal("fsqueue_message_fd_rw: snprintf");

	return open(pathname, O_CREAT|O_EXCL|O_RDWR, 0600);
}

static int
fsqueue_message_delete(enum queue_kind qkind, u_int32_t msgid)
{
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];
	char msgpath[MAXPATHLEN];
	DIR *dirp;
	struct dirent *dp;

	if (qkind == Q_QUEUE) {
		if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%03x/%08x", PATH_QUEUE,
			msgid & 0xfff, msgid))
			fatal("fsqueue_message_delete: snprintf");
	}
	else if (qkind == Q_PURGE) {
		if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%08x", PATH_PURGE,
			msgid))
			fatal("fsqueue_message_delete: snprintf");
	}

	if (! bsnprintf(evpdir, sizeof(evpdir), "%s%s", rootdir,
		PATH_ENVELOPES))
		fatal("fsqueue_message_delete: snprintf");

	dirp = opendir(evpdir);
	if (dirp) {
		char envelope[MAXPATHLEN];

		while ((dp = readdir(dirp)) != NULL) {
			if (! bsnprintf(envelope, MAXPATHLEN, "%s/%s",
				evpdir, dp->d_name))
				fatal("fsqueue_message_delete: truncated evp");
			unlink(envelope);
		}
		closedir(dirp);
	}

	if (! bsnprintf(msgpath, sizeof(msgpath), "%s/message", rootdir))
		fatal("fsqueue_message_delete: snprintf");

	if (unlink(msgpath) == -1) {
		if (errno != ENOENT)
			fatal("fsqueue_message_delete: unlink");
	}

	if (rmdir(evpdir) == -1) {
		/* It is ok to fail rmdir with ENOENT here
		 * because upon successful delivery of the
		 * last envelope, we remove the directory.
		 */
		if (errno != ENOENT)
			fatal("fsqueue_message_delete: rmdir");
	}

	if (rmdir(rootdir) == -1)
		fatal("#2 fsqueue_message_delete: rmdir");


	if (qkind == Q_QUEUE) {
		if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%03x", PATH_QUEUE, msgid & 0xffff))
			fatal("fsqueue_message_delete: snprintf");
		rmdir(rootdir);
	}

	return 1;
}

static int
fsqueue_message_purge(enum queue_kind qkind, u_int32_t msgid)
{
	char rootdir[MAXPATHLEN];
	char purgedir[MAXPATHLEN];

	if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%08x",
		fsqueue_getpath(qkind), msgid))
		fatalx("fsqueue_message_purge: snprintf");

	if (! bsnprintf(purgedir, sizeof(purgedir), "%s/%08x",
		fsqueue_getpath(Q_PURGE), msgid))
		fatalx("fsqueue_message_purge: snprintf");

	if (rename(rootdir, purgedir) == -1)
		fatal("fsqueue_message_purge: rename");

	return 1;
}

static int
fsqueue_message_corrupt(enum queue_kind qkind, u_int32_t msgid)
{
	char rootdir[MAXPATHLEN];
	char corruptdir[MAXPATHLEN];

	if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%03x/%08x",
		fsqueue_getpath(qkind), msgid & 0xfff, msgid))
		fatalx("fsqueue_message_corrupt: snprintf");

	if (! bsnprintf(corruptdir, sizeof(corruptdir), "%s/%08x",
		fsqueue_getpath(Q_CORRUPT), msgid))
		fatalx("fsqueue_message_corrupt: snprintf");

	if (rename(rootdir, corruptdir) == -1)
		fatalx("fsqueue_message_corrupt: rename");

	return 1;
}


int
fsqueue_init(void)
{
	unsigned int	 n;
	char		*paths[] = { PATH_INCOMING, PATH_ENQUEUE, PATH_QUEUE,
				     PATH_PURGE, PATH_BOUNCE, PATH_CORRUPT };
	char		 path[MAXPATHLEN];
	int		 ret;

	ret = 1;
	for (n = 0; n < nitems(paths); n++) {
		strlcpy(path, PATH_SPOOL, sizeof(path));
		if (strlcat(path, paths[n], sizeof(path)) >= sizeof(path))
			errx(1, "path too long %s%s", PATH_SPOOL, paths[n]);

		if (ckdir(path, 0700, env->sc_pw->pw_uid, 0, 1) == 0)
			ret = 0;
	}

	if (! bsnprintf(path, sizeof path, "%s/envelope.tmp", PATH_QUEUE))
		errx(1, "path too long %s/envelope.tmp", PATH_QUEUE);

	unlink(path);

	return ret;
}

int
fsqueue_message(enum queue_kind qkind, enum queue_op qop, u_int32_t *msgid)
{
        switch (qop) {
        case QOP_CREATE:
		return fsqueue_message_create(qkind, msgid);

        case QOP_DELETE:
		return fsqueue_message_delete(qkind, *msgid);

        case QOP_COMMIT:
		return fsqueue_message_commit(qkind, *msgid);

        case QOP_FD_R:
                return fsqueue_message_fd_r(qkind, *msgid);

        case QOP_FD_RW:
                return fsqueue_message_fd_rw(qkind, *msgid);

	case QOP_PURGE:
		return fsqueue_message_purge(qkind, *msgid);

	case QOP_CORRUPT:
		return fsqueue_message_corrupt(qkind, *msgid);

        default:
		fatalx("queue_fsqueue_message: unsupported operation.");
        }

	return 0;
}

int
fsqueue_envelope(enum queue_kind qkind, enum queue_op qop, struct envelope *m)
{
        switch (qop) {
        case QOP_CREATE:
		return fsqueue_envelope_create(qkind, m);

        case QOP_DELETE:
		return fsqueue_envelope_delete(qkind, m);

        case QOP_LOAD:
		return fsqueue_envelope_load(qkind, m);

        case QOP_UPDATE:
		return fsqueue_envelope_update(qkind, m);

        default:
		fatalx("queue_fsqueue_envelope: unsupported operation.");
        }

	return 0;
}

#define	QWALK_AGAIN	0x1
#define	QWALK_RECURSE	0x2
#define	QWALK_RETURN	0x3

struct qwalk {
	enum queue_kind kind;
	char	  path[MAXPATHLEN];
	DIR	 *dirs[3];
	int	(*filefn)(struct qwalk *, char *);
	int	  bucket;
	int	  level;
	int	  strict;
	u_int32_t msgid;
};

int		walk_simple(struct qwalk *, char *);
int		walk_queue(struct qwalk *, char *);
int		walk_queue_nobucket(struct qwalk *, char *);

void *
fsqueue_qwalk_new(enum queue_kind kind, u_int32_t msgid)
{
	struct qwalk *q;

	q = calloc(1, sizeof(struct qwalk));
	if (q == NULL)
		fatal("qwalk_new: calloc");

	strlcpy(q->path, fsqueue_getpath(kind),
	    sizeof(q->path));

	q->kind = kind;
	q->level = 0;
	q->strict = 0;
	q->filefn = walk_simple;
	q->msgid = msgid;

	if (q->msgid) {
		/* force level and bucket */
		q->bucket = q->msgid & 0xfff;
		q->level = 2;
		if (! bsnprintf(q->path, sizeof(q->path), "%s/%03x/%08x/%s",
			PATH_QUEUE, q->bucket, q->msgid, PATH_ENVELOPES))
			fatalx("walk_queue: snprintf");
	}

	if (smtpd_process == PROC_QUEUE || smtpd_process == PROC_RUNNER)
		q->strict = 1;

	if (kind == Q_QUEUE)
		q->filefn = walk_queue;
	if (kind == Q_INCOMING || kind == Q_ENQUEUE || kind == Q_PURGE)
		q->filefn = walk_queue_nobucket;

	q->dirs[q->level] = opendir(q->path);
	if (q->dirs[q->level] == NULL)
		fatal("qwalk_new: opendir");

	return (q);
}

int
fsqueue_qwalk(void *hdl, u_int64_t *evpid)
{
	struct qwalk *q = hdl;
	struct dirent	*dp;

again:
	errno = 0;
	dp = readdir(q->dirs[q->level]);
	if (errno)
		fatal("qwalk: readdir");
	if (dp == NULL) {
		closedir(q->dirs[q->level]);
		q->dirs[q->level] = NULL;
		if (q->level == 0 || q->msgid)
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
	case QWALK_RETURN: {
		char *endptr;

		errno = 0;
		*evpid = (u_int64_t)strtoull(dp->d_name, &endptr, 16);
		if (q->path[0] == '\0' || *endptr != '\0')
			goto again;
		if (errno == ERANGE && *evpid == ULLONG_MAX)
			goto again;
		if (q->msgid)
			if (evpid_to_msgid(*evpid) != q->msgid)
				return 0;

		return (1);
	}
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
fsqueue_qwalk_close(void *hdl)
{
	int i;
	struct qwalk *q = hdl;

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
		if (! bsnprintf(q->path, sizeof(q->path), "%s/%03x",
			fsqueue_getpath(q->kind), q->bucket & 0xfff))
			fatalx("walk_queue: snprintf");
		return (QWALK_RECURSE);
	case 1:
		if (! bsnprintf(q->path, sizeof(q->path), "%s/%03x/%s%s",
			fsqueue_getpath(q->kind), q->bucket & 0xfff, fname,
			PATH_ENVELOPES))
			fatalx("walk_queue: snprintf");
		return (QWALK_RECURSE);
	case 2:
		return (QWALK_RETURN);
	}

	return (-1);
}

int
walk_queue_nobucket(struct qwalk *q, char *fname)
{
	switch (q->level) {
	case 0:
		if (strcmp(fname, "envelope.tmp") == 0)
			return (QWALK_AGAIN);
		if (! bsnprintf(q->path, sizeof(q->path), "%s/%s%s",
			fsqueue_getpath(q->kind), fname, PATH_ENVELOPES))
			fatalx("walk_queue_nobucket: snprintf");
		return (QWALK_RECURSE);
	case 1:
		return (QWALK_RETURN);
	}

	return (-1);
}

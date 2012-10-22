/*	$OpenBSD: queue_fsqueue.c,v 1.54 2012/10/22 21:58:14 chl Exp $	*/

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

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <fts.h>
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

static int	fsqueue_envelope_create(uint64_t *, char *, size_t);
static int	fsqueue_envelope_load(uint64_t, char *, size_t);
static int	fsqueue_envelope_update(uint64_t, char *, size_t);
static int	fsqueue_envelope_delete(uint64_t);

static int	fsqueue_message_create(uint32_t *);
static int	fsqueue_message_commit(uint32_t);
static int	fsqueue_message_fd_r(uint32_t);
static int	fsqueue_message_delete(uint32_t);
static int	fsqueue_message_corrupt(uint32_t);

static int	fsqueue_message_path(uint32_t, char *, size_t);
static int	fsqueue_envelope_path(uint64_t, char *, size_t);
static int	fsqueue_envelope_dump_atomic(char *, char *, size_t);

static int	fsqueue_init(int);
static int	fsqueue_message(enum queue_op, uint32_t *);
static int	fsqueue_envelope(enum queue_op , uint64_t *, char *, size_t);

static void    *fsqueue_qwalk_new(uint32_t);
static int	fsqueue_qwalk(void *, uint64_t *);
static void	fsqueue_qwalk_close(void *);

#define PATH_QUEUE		"/queue"
#define PATH_CORRUPT		"/corrupt"

#define PATH_EVPTMP		PATH_INCOMING "/envelope.tmp"

struct queue_backend	queue_backend_fs = {
	  fsqueue_init,
	  fsqueue_message,
	  fsqueue_envelope,
	  fsqueue_qwalk_new,
	  fsqueue_qwalk,
	  fsqueue_qwalk_close
};

static struct timespec	startup;

static int
fsqueue_message_path(uint32_t msgid, char *buf, size_t len)
{
	return bsnprintf(buf, len, "%s/%02x/%08x",
	    PATH_QUEUE,
	    msgid & 0xff,
	    msgid);
}

static int
fsqueue_message_corrupt_path(uint32_t msgid, char *buf, size_t len)
{
	return bsnprintf(buf, len, "%s/%08x",
	    PATH_CORRUPT,
	    msgid);
}

static int
fsqueue_envelope_path(uint64_t evpid, char *buf, size_t len)
{
	return bsnprintf(buf, len, "%s/%02x/%08x%s/%016" PRIx64,
	    PATH_QUEUE,
	    evpid_to_msgid(evpid) & 0xff,
	    evpid_to_msgid(evpid),
	    PATH_ENVELOPES, evpid);
}

static int
fsqueue_envelope_dump_atomic(char *dest, char *evpbuf, size_t evplen)
{
	int	 fd;
	char	 evpname[MAXPATHLEN];
	ssize_t	 w;

	/* temporary fix for multi-process access to the queue,
	 * should be fixed by rerouting ALL queue access through
	 * the queue process.
	 */
	snprintf(evpname, sizeof evpname, PATH_EVPTMP".%d", getpid());

	if ((fd = open(evpname, O_RDWR | O_CREAT | O_EXCL, 0600)) == -1) {
		if (errno == ENOSPC || errno == ENFILE)
			goto tempfail;
		fatal("fsqueue_envelope_dump_atomic: open");
	}

	w = write(fd, evpbuf, evplen);
	if (w == -1) {
		log_warn("fsqueue_envelope_dump_atomic: write");
		if (errno == ENOSPC)
			goto tempfail;
		fatal("fsqueue_envelope_dump_atomic: write");
	}

	if ((size_t) w != evplen) {
		log_warnx("fsqueue_envelope_dump_atomic: partial write");
		goto tempfail;
	}

	if (fsync(fd))
		fatal("fsync");
	close(fd);

	if (rename(evpname, dest) == -1) {
		log_warn("fsqueue_envelope_dump_atomic: rename");
		if (errno == ENOSPC)
			goto tempfail;
		fatal("fsqueue_envelope_dump_atomic: rename");
	}

	return (1);

tempfail:
	if (fd != -1)
		close(fd);
	if (unlink(evpname) == -1)
		fatal("fsqueue_envelope_dump_atomic: unlink");
	return (0);
}

static int
fsqueue_envelope_create(uint64_t *evpid, char *buf, size_t len)
{
	char		path[MAXPATHLEN];
	uint32_t	msgid;
	int		queued = 0, i;
	struct stat	sb;

	msgid = evpid_to_msgid(*evpid);
	queue_message_incoming_path(msgid, path, sizeof(path));
	if (stat(path, &sb) == -1)
		queued = 1;

	for (i = 0; i < 20; i ++) {
		*evpid = queue_generate_evpid(msgid);
		if (queued)
			fsqueue_envelope_path(*evpid, path, sizeof(path));
		else
			queue_envelope_incoming_path(*evpid, path, sizeof(path));

		if (stat(path, &sb) == -1 && errno == ENOENT)
			goto found;
	}
	fatal("couldn't figure out a new envelope id");

found:
	return (fsqueue_envelope_dump_atomic(path, buf, len));
}

static int
fsqueue_envelope_load(uint64_t evpid, char *buf, size_t len)
{
	char	 pathname[MAXPATHLEN];
	FILE	*fp;
	ssize_t	 r;

	fsqueue_envelope_path(evpid, pathname, sizeof(pathname));

	fp = fopen(pathname, "r");
	if (fp == NULL) {
		if (errno == ENOENT || errno == ENFILE)
			return (0);
		fatal("fsqueue_envelope_load: fopen");
	}

	r = fread(buf, 1, len, fp);

	fclose(fp);

	return (r);
}

static int
fsqueue_envelope_update(uint64_t evpid, char *buf, size_t len)
{
	char dest[MAXPATHLEN];

	fsqueue_envelope_path(evpid, dest, sizeof(dest));

	return (fsqueue_envelope_dump_atomic(dest, buf, len));
}

static int
fsqueue_envelope_delete(uint64_t evpid)
{
	char pathname[MAXPATHLEN];

	fsqueue_envelope_path(evpid, pathname, sizeof(pathname));

	if (unlink(pathname) == -1)
		fatal("fsqueue_envelope_delete: unlink");

	*strrchr(pathname, '/') = '\0';

	if (rmdir(pathname) != -1)
		fsqueue_message_delete(evpid_to_msgid(evpid));

	return 1;
}

static int
fsqueue_message_create(uint32_t *msgid)
{
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];
	struct stat sb;

again:
	*msgid = queue_generate_msgid();
	
	/* prevent possible collision later when moving to Q_QUEUE */
	fsqueue_message_path(*msgid, rootdir, sizeof(rootdir));
	if (stat(rootdir, &sb) != -1 || errno != ENOENT)
		goto again;

	queue_message_incoming_path(*msgid, rootdir, sizeof(rootdir));
	if (mkdir(rootdir, 0700) == -1) {
		if (errno == EEXIST)
			goto again;

		if (errno == ENOSPC) {
			*msgid = 0;
			return 0;
		}
		fatal("fsqueue_message_create: mkdir");
	}

	strlcpy(evpdir, rootdir, sizeof(evpdir));
	strlcat(evpdir, PATH_ENVELOPES, sizeof(evpdir));

	if (mkdir(evpdir, 0700) == -1) {
		if (errno == ENOSPC) {
			rmdir(rootdir);
			*msgid = 0;
			return 0;
		}
		fatal("fsqueue_message_create: mkdir");
	}

	return 1;
}

static int
fsqueue_message_commit(uint32_t msgid)
{
	char incomingdir[MAXPATHLEN];
	char queuedir[MAXPATHLEN];
	char msgdir[MAXPATHLEN];

	queue_message_incoming_path(msgid, incomingdir, sizeof(incomingdir));
	fsqueue_message_path(msgid, msgdir, sizeof(msgdir));
	strlcpy(queuedir, msgdir, sizeof(queuedir));
	*strrchr(queuedir, '/') = '\0';

	if (mkdir(queuedir, 0700) == -1) {
		if (errno == ENOSPC)
			return 0;
		if (errno != EEXIST)
			fatal("fsqueue_message_commit: mkdir");
	}

	if (rename(incomingdir, msgdir) == -1) {
		if (errno == ENOSPC)
			return 0;
		fatal("fsqueue_message_commit: rename");
	}

	return 1;
}

static int
fsqueue_message_fd_r(uint32_t msgid)
{
	int fd;
	char path[MAXPATHLEN];

	fsqueue_message_path(msgid, path, sizeof(path));
	strlcat(path, PATH_MESSAGE, sizeof(path));

	if ((fd = open(path, O_RDONLY)) == -1)
		fatal("fsqueue_message_fd_r: open");

	return fd;
}

static int
fsqueue_message_delete(uint32_t msgid)
{
	char rootdir[MAXPATHLEN];

	if (! fsqueue_message_path(msgid, rootdir, sizeof(rootdir)))
		fatal("fsqueue_message_delete: snprintf");

	if (rmtree(rootdir, 0) == -1)
		fatal("fsqueue_message_delete: rmtree");

	return 1;
}

static int
fsqueue_message_corrupt(uint32_t msgid)
{
	struct stat sb;
	char rootdir[MAXPATHLEN];
	char corruptdir[MAXPATHLEN];
	char buf[64];
	int  retry = 0;

	fsqueue_message_path(msgid, rootdir, sizeof(rootdir));
	fsqueue_message_corrupt_path(msgid, corruptdir, sizeof(corruptdir));

again:
	if (stat(corruptdir, &sb) != -1 || errno != ENOENT) {
		fsqueue_message_corrupt_path(msgid, corruptdir, sizeof(corruptdir));
		snprintf(buf, sizeof(buf), ".%i", retry++);
		strlcat(corruptdir, buf, sizeof(corruptdir));
		goto again;
	}

	if (rename(rootdir, corruptdir) == -1)
		fatalx("fsqueue_message_corrupt: rename");

	return 1;
}

static int
fsqueue_init(int server)
{
	unsigned int	 n;
	char		*paths[] = { PATH_QUEUE, PATH_CORRUPT };
	char		 path[MAXPATHLEN];
	int		 ret;
	struct timeval	 tv;

	if (!fsqueue_envelope_path(0, path, sizeof(path)))
		errx(1, "cannot store envelope path in %s", PATH_QUEUE);

	ret = 1;
	for (n = 0; n < nitems(paths); n++) {
		strlcpy(path, PATH_SPOOL, sizeof(path));
		if (strlcat(path, paths[n], sizeof(path)) >= sizeof(path))
			errx(1, "path too long %s%s", PATH_SPOOL, paths[n]);

		if (ckdir(path, 0700, env->sc_pw->pw_uid, 0, server) == 0)
			ret = 0;
	}

	if (gettimeofday(&tv, NULL) == -1)
		err(1, "gettimeofday");
	TIMEVAL_TO_TIMESPEC(&tv, &startup);

	return ret;
}

static int
fsqueue_message(enum queue_op qop, uint32_t *msgid)
{
        switch (qop) {
        case QOP_CREATE:
		return fsqueue_message_create(msgid);

        case QOP_DELETE:
		return fsqueue_message_delete(*msgid);

        case QOP_COMMIT:
		return fsqueue_message_commit(*msgid);

        case QOP_FD_R:
                return fsqueue_message_fd_r(*msgid);

	case QOP_CORRUPT:
		return fsqueue_message_corrupt(*msgid);

        default:
		fatalx("queue_fsqueue_message: unsupported operation.");
        }

	return 0;
}

static int
fsqueue_envelope(enum queue_op qop, uint64_t *evpid, char *buf, size_t len)
{
        switch (qop) {
        case QOP_CREATE:
		return fsqueue_envelope_create(evpid, buf, len);

        case QOP_DELETE:
		return fsqueue_envelope_delete(*evpid);

        case QOP_LOAD:
		return fsqueue_envelope_load(*evpid, buf, len);

        case QOP_UPDATE:
		return fsqueue_envelope_update(*evpid, buf, len);

        default:
		fatalx("queue_fsqueue_envelope: unsupported operation.");
        }

	return 0;
}

struct qwalk {
	FTS	*fts;
	uint32_t msgid;
	int	 depth;
};

static void *
fsqueue_qwalk_new(uint32_t msgid)
{
	char		 path[MAXPATHLEN];
	char * const	 path_argv[] = { path, NULL };
	struct qwalk	*q;

	q = xcalloc(1, sizeof(*q), "fsqueue_qwalk_new");
	q->msgid = msgid;
	strlcpy(path, PATH_QUEUE, sizeof(path));
	q->fts = fts_open(path_argv,
	    FTS_PHYSICAL | FTS_NOCHDIR, NULL);

	if (q->fts == NULL)
		err(1, "fsqueue_qwalk_new: fts_open: %s", path);

	return (q);
}

static void
fsqueue_qwalk_close(void *hdl)
{
	struct qwalk	*q = hdl;

	fts_close(q->fts);

	free(q);
}

static int
fsqueue_qwalk(void *hdl, uint64_t *evpid)
{
	struct qwalk	*q = hdl;
        FTSENT 		*e;
	char		*tmp;
	uint32_t	 msgid;

        while ((e = fts_read(q->fts)) != NULL) {

		switch(e->fts_info) {
		case FTS_D:
			q->depth += 1;
			if (q->depth == 2 && e->fts_namelen != 2) {
				log_debug("fsqueue: bogus directory %s",
				    e->fts_path);
				fts_set(q->fts, e, FTS_SKIP);
				break;
			}
			if (q->depth == 3 && e->fts_namelen != 8) {
				log_debug("fsqueue: bogus directory %s",
				    e->fts_path);
				fts_set(q->fts, e, FTS_SKIP);
				break;
			}
			if (q->msgid && (q->depth == 2 || q->depth == 3)) {
				msgid = strtoull(e->fts_name, &tmp, 16);
				if (msgid != (q->depth == 1) ?
				    (q->msgid & 0xff) : q->msgid) {
					fts_set(q->fts, e, FTS_SKIP);
					break;
				}
			}
			break;

		case FTS_DP:
			q->depth -= 1;
			break;

		case FTS_F:
			if (q->depth != 4)
				break;
			if (e->fts_namelen != 16)
				break;
			if (timespeccmp(&e->fts_statp->st_mtim, &startup, >))
				break;
			tmp = NULL;
			*evpid = strtoull(e->fts_name, &tmp, 16);
			if (tmp && *tmp !=  '\0') {
				log_debug("fsqueue: bogus file %s",
				    e->fts_path);
				break;
			}
			return (1);
		default:
			break;
		}
	}

        return (0); 
}

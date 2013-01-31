/*	$OpenBSD: queue_fsqueue.c,v 1.58 2013/01/31 18:34:43 eric Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@poolp.org>
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
static int	fsqueue_envelope_walk(uint64_t *, char *, size_t);

static int	fsqueue_message_create(uint32_t *);
static int	fsqueue_message_commit(uint32_t);
static int	fsqueue_message_fd_r(uint32_t);
static int	fsqueue_message_fd_rw(uint32_t);
static int	fsqueue_message_delete(uint32_t);
static int	fsqueue_message_corrupt(uint32_t);

static void	fsqueue_message_path(uint32_t, char *, size_t);
static void	fsqueue_envelope_path(uint64_t, char *, size_t);
static void	fsqueue_envelope_incoming_path(uint64_t, char *, size_t);

static int	fsqueue_envelope_dump(char *, char *, size_t, int, int);

static int	fsqueue_init(int);
static int	fsqueue_message(enum queue_op, uint32_t *);
static int	fsqueue_envelope(enum queue_op , uint64_t *, char *, size_t);

static void    *fsqueue_qwalk_new(void);
static int	fsqueue_qwalk(void *, uint64_t *);
static void	fsqueue_qwalk_close(void *);

struct tree	evpcount;

#define PATH_QUEUE		"/queue"
#define PATH_CORRUPT		"/corrupt"

#define PATH_EVPTMP		PATH_INCOMING "/envelope.tmp"

struct queue_backend	queue_backend_fs = {
	fsqueue_init,
	fsqueue_message,
	fsqueue_envelope,
};

static struct timespec	startup;

static void
fsqueue_message_path(uint32_t msgid, char *buf, size_t len)
{
	if (! bsnprintf(buf, len, "%s/%02x/%08x",
		PATH_QUEUE,
		(msgid & 0xff000000) >> 24,
		msgid))
		fatalx("fsqueue_message_path: path does not fit buffer");
}

static void
fsqueue_message_corrupt_path(uint32_t msgid, char *buf, size_t len)
{
	if (! bsnprintf(buf, len, "%s/%08x",
		PATH_CORRUPT,
		msgid))
		fatalx("fsqueue_message_corrupt_path: path does not fit buffer");
}

static void
fsqueue_envelope_path(uint64_t evpid, char *buf, size_t len)
{
	if (! bsnprintf(buf, len, "%s/%02x/%08x/%016" PRIx64,
		PATH_QUEUE,
		(evpid_to_msgid(evpid) & 0xff000000) >> 24,
		evpid_to_msgid(evpid),
		evpid))
		fatalx("fsqueue_envelope_path: path does not fit buffer");
}

static void
fsqueue_envelope_incoming_path(uint64_t evpid, char *buf, size_t len)
{
	if (! bsnprintf(buf, len, "%s/%08x/%016" PRIx64,
		PATH_INCOMING,
		evpid_to_msgid(evpid),
		evpid))
		fatalx("fsqueue_envelope_incoming_path: path does not fit buffer");
}

static int
fsqueue_envelope_dump(char *dest, char *evpbuf, size_t evplen, int do_atomic, int do_sync)
{
	const char     *path = do_atomic ? PATH_EVPTMP : dest;
	FILE	       *fp = NULL;
	int		fd;
	size_t		w;

	if ((fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0600)) == -1) {
		if (errno == EEXIST)
			return -1;
		log_warn("warn: fsqueue_envelope_dump: open");
		goto tempfail;
	}

	if ((fp = fdopen(fd, "w")) == NULL) {
		log_warn("warn: fsqueue_envelope_dump: fdopen");
		goto tempfail;
	}

	w = fwrite(evpbuf, 1, evplen, fp);
	if (w < evplen) {
		log_warn("warn: fsqueue_envelope_dump: short write");
		goto tempfail;
	}
	if (fflush(fp)) {
		log_warn("warn: fsqueue_envelope_dump: fflush");
		goto tempfail;
	}
	if (do_sync && fsync(fileno(fp))) {
		log_warn("warn: fsqueue_envelope_dump: fsync");
		goto tempfail;
	}
	if (fclose(fp) != 0) {
		log_warn("warn: fsqueue_envelope_dump: fclose");
		fp = NULL;
		goto tempfail;
	}
	fp = NULL;
	fd = -1;

	if (do_atomic && rename(path, dest) == -1) {
		log_warn("warn: fsqueue_envelope_dump: rename");
		goto tempfail;
	}
	return (1);

tempfail:
	if (fp)
		fclose(fp);
	else if (fd != -1)
		close(fd);
	if (unlink(path) == -1)
		log_warn("warn: fsqueue_envelope_dump: unlink");
	return (0);
}

static int
fsqueue_envelope_create(uint64_t *evpid, char *buf, size_t len)
{
	char		path[MAXPATHLEN];
	uint32_t	msgid;
	int		queued = 0, i, r = 0;
	struct stat	sb;
	uintptr_t	*n;

	msgid = evpid_to_msgid(*evpid);
	if (msgid == 0) {
		log_warnx("warn: fsqueue_envelope_create: msgid=0, "
		    "evpid=%016"PRIx64, *evpid);
		goto done;
	}
	
	queue_message_incoming_path(msgid, path, sizeof(path));
	if (stat(path, &sb) == -1)
		queued = 1;

	for (i = 0; i < 20; i ++) {
		*evpid = queue_generate_evpid(msgid);
		if (queued)
			fsqueue_envelope_path(*evpid, path, sizeof(path));
		else
			fsqueue_envelope_incoming_path(*evpid, path,
			    sizeof(path));

		r = fsqueue_envelope_dump(path, buf, len, 0, 1);
		if (r >= 0)
			goto done;
	}
	r = 0;
	log_warnx("warn: fsqueue_envelope_create: could not allocate evpid");

done:
	if (r) {
		n = tree_pop(&evpcount, msgid);
		n += 1;
		tree_xset(&evpcount, msgid, n);
	}
	return (r);
}

static int
fsqueue_envelope_load(uint64_t evpid, char *buf, size_t len)
{
	char	 pathname[MAXPATHLEN];
	FILE	*fp;
	size_t	 r;

	fsqueue_envelope_path(evpid, pathname, sizeof(pathname));

	fp = fopen(pathname, "r");
	if (fp == NULL) {
		if (errno != ENOENT && errno != ENFILE)
			log_warn("warn: fsqueue_envelope_load: fopen");
		return 0;
	}

	r = fread(buf, 1, len, fp);
	if (r) {
		if (r == len) {
			log_warn("warn: fsqueue_envelope_load: too large");
			r = 0;
		}
		else
			buf[r] = '\0';
	}
	fclose(fp);

	return (r);
}

static int
fsqueue_envelope_update(uint64_t evpid, char *buf, size_t len)
{
	char dest[MAXPATHLEN];

	fsqueue_envelope_path(evpid, dest, sizeof(dest));

	return (fsqueue_envelope_dump(dest, buf, len, 1, 1));
}

static int
fsqueue_envelope_delete(uint64_t evpid)
{
	char		pathname[MAXPATHLEN];
	uint32_t	msgid;
	uintptr_t	*n;

	fsqueue_envelope_path(evpid, pathname, sizeof(pathname));
	if (unlink(pathname) == -1)
		if (errno != ENOENT)
			return 0;

	msgid = evpid_to_msgid(evpid);
	n = tree_pop(&evpcount, msgid);
	n -= 1;
	if (n == NULL)
		fsqueue_message_delete(msgid);
	else
		tree_xset(&evpcount, msgid, n);

	return (1);
}

static int
fsqueue_envelope_walk(uint64_t *evpid, char *buf, size_t len)
{
	static int	 done = 0;
	static void	*hdl = NULL;
	uintptr_t	*n;
	int		 r;
	uint32_t	 msgid;
	struct envelope	 ep;

	if (done)
		return (-1);

	if (hdl == NULL)
		hdl = fsqueue_qwalk_new();

	if (fsqueue_qwalk(hdl, evpid)) {
		bzero(buf, len);
		r = fsqueue_envelope_load(*evpid, buf, len);
		if (r) {
			msgid = evpid_to_msgid(*evpid);
			if (! envelope_load_buffer(&ep, buf, r))
				(void)fsqueue_message_corrupt(msgid);
			else {
				n = tree_pop(&evpcount, msgid);
				n += 1;
				tree_xset(&evpcount, msgid, n);
			}
		}
		return (r);
	}

	fsqueue_qwalk_close(hdl);
	done = 1;
	return (-1);
}

static int
fsqueue_message_create(uint32_t *msgid)
{
	char rootdir[MAXPATHLEN];
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

		log_warn("warn: fsqueue_message_create: mkdir");
		*msgid = 0;
		return 0;
	}

	return (1);
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

	/* first attempt to rename */
	if (rename(incomingdir, msgdir) == 0)
		return 1;
	if (errno == ENOSPC)
		return 0;
	if (errno != ENOENT) {
		log_warn("warn: fsqueue_message_commit: rename");
		return 0;
	}

	/* create the bucket */
	*strrchr(queuedir, '/') = '\0';
	if (mkdir(queuedir, 0700) == -1) {
		if (errno == ENOSPC)
			return 0;
		if (errno != EEXIST) {
			log_warn("warn: fsqueue_message_commit: mkdir");
			return 0;
		}
	}

	/* rename */
	if (rename(incomingdir, msgdir) == -1) {
		if (errno == ENOSPC)
			return 0;
		log_warn("warn: fsqueue_message_commit: rename");
		return 0;
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

	if ((fd = open(path, O_RDONLY)) == -1) {
		log_warn("fsqueue_message_fd_r: open");
		return -1;
	}

	return fd;
}

static int
fsqueue_message_fd_rw(uint32_t msgid)
{
	char msgpath[MAXPATHLEN];

	queue_message_incoming_path(msgid, msgpath, sizeof msgpath);
	strlcat(msgpath, PATH_MESSAGE, sizeof(msgpath));

	return open(msgpath, O_RDWR | O_CREAT | O_EXCL, 0600);
}

static int
fsqueue_message_delete(uint32_t msgid)
{
	char		path[MAXPATHLEN];
	struct stat	sb;

	queue_message_incoming_path(msgid, path, sizeof(path));
	if (stat(path, &sb) == -1)
		fsqueue_message_path(msgid, path, sizeof(path));

	if (rmtree(path, 0) == -1)
		log_warn("warn: fsqueue_message_delete: rmtree");

	tree_pop(&evpcount, msgid);

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
	fsqueue_message_corrupt_path(msgid, corruptdir,
	    sizeof(corruptdir));

again:
	if (stat(corruptdir, &sb) != -1 || errno != ENOENT) {
		fsqueue_message_corrupt_path(msgid, corruptdir,
		    sizeof(corruptdir));
		snprintf(buf, sizeof(buf), ".%i", retry++);
		strlcat(corruptdir, buf, sizeof(corruptdir));
		goto again;
	}

	if (rename(rootdir, corruptdir) == -1) {
		log_warn("warn: fsqueue_message_corrupt: rename");
		return 0;
	}

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

	fsqueue_envelope_path(0, path, sizeof(path));

	ret = 1;
	for (n = 0; n < nitems(paths); n++) {
		strlcpy(path, PATH_SPOOL, sizeof(path));
		if (strlcat(path, paths[n], sizeof(path)) >= sizeof(path))
			errx(1, "path too long %s%s", PATH_SPOOL, paths[n]);

		if (ckdir(path, 0700, env->sc_pwqueue->pw_uid, 0, server) == 0)
			ret = 0;
	}

	if (gettimeofday(&tv, NULL) == -1)
		err(1, "gettimeofday");
	TIMEVAL_TO_TIMESPEC(&tv, &startup);

	tree_init(&evpcount);

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
	case QOP_FD_RW:
		return fsqueue_message_fd_rw(*msgid);
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
	case QOP_WALK:
		return fsqueue_envelope_walk(evpid, buf, len);
	default:
		fatalx("queue_fsqueue_envelope: unsupported operation.");
	}
	return 0;
}

struct qwalk {
	FTS	*fts;
	int	 depth;
};

static void *
fsqueue_qwalk_new(void)
{
	char		 path[MAXPATHLEN];
	char * const	 path_argv[] = { path, NULL };
	struct qwalk	*q;

	q = xcalloc(1, sizeof(*q), "fsqueue_qwalk_new");
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
	FTSENT		*e;
	char		*tmp;

	while ((e = fts_read(q->fts)) != NULL) {
		switch (e->fts_info) {
		case FTS_D:
			q->depth += 1;
			if (q->depth == 2 && e->fts_namelen != 2) {
				log_debug("debug: fsqueue: bogus directory %s",
				    e->fts_path);
				fts_set(q->fts, e, FTS_SKIP);
				break;
			}
			if (q->depth == 3 && e->fts_namelen != 8) {
				log_debug("debug: fsqueue: bogus directory %s",
				    e->fts_path);
				fts_set(q->fts, e, FTS_SKIP);
				break;
			}
			break;

		case FTS_DP:
		case FTS_DNR:
			q->depth -= 1;
			break;

		case FTS_F:
			if (q->depth != 3)
				break;
			if (e->fts_namelen != 16)
				break;
			if (timespeccmp(&e->fts_statp->st_mtim, &startup, >))
				break;
			tmp = NULL;
			*evpid = strtoull(e->fts_name, &tmp, 16);
			if (tmp && *tmp !=  '\0') {
				log_debug("debug: fsqueue: bogus file %s",
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

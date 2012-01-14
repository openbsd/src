/*	$OpenBSD: queue_fsqueue.c,v 1.33 2012/01/14 12:56:49 eric Exp $	*/

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

static char	*fsqueue_getpath(enum queue_kind);

static int	fsqueue_envelope_load(enum queue_kind, struct envelope *);
static int	fsqueue_envelope_update(enum queue_kind, struct envelope *);
static int	fsqueue_envelope_delete(enum queue_kind, struct envelope *);

static int	fsqueue_message_create(enum queue_kind, u_int32_t *);
static int	fsqueue_message_commit(enum queue_kind, u_int32_t);
static int	fsqueue_message_fd_r(enum queue_kind, u_int32_t);
static int	fsqueue_message_fd_rw(enum queue_kind, u_int32_t);
static int	fsqueue_message_delete(enum queue_kind, u_int32_t);
static int	fsqueue_message_corrupt(enum queue_kind, u_int32_t);

static int	fsqueue_message_path(enum queue_kind, uint32_t, char *, size_t);
static int	fsqueue_envelope_path(enum queue_kind, u_int64_t, char *, size_t);
static int	fsqueue_envelope_dump_atomic(char *, struct envelope *);

int	fsqueue_init(void);
int	fsqueue_message(enum queue_kind, enum queue_op, u_int32_t *);
int	fsqueue_envelope(enum queue_kind, enum queue_op , struct envelope *);
int	fsqueue_load_envelope_ascii(FILE *, struct envelope *);
int	fsqueue_dump_envelope_ascii(FILE *, struct envelope *);

void   *fsqueue_qwalk_new(enum queue_kind, u_int32_t);
int	fsqueue_qwalk(void *, u_int64_t *);
void	fsqueue_qwalk_close(void *);

#define PATH_INCOMING		"/incoming"
#define PATH_QUEUE		"/queue"
#define PATH_CORRUPT		"/corrupt"

#define PATH_MESSAGE		"/message"
#define PATH_ENVELOPES		"/envelopes"

#define PATH_EVPTMP		PATH_INCOMING "/envelope.tmp"

struct queue_backend	queue_backend_fs = {
	  fsqueue_init,
	  fsqueue_message,
	  fsqueue_envelope,
	  fsqueue_qwalk_new,
	  fsqueue_qwalk,
	  fsqueue_qwalk_close
};

static char *
fsqueue_getpath(enum queue_kind kind)
{
        switch (kind) {
        case Q_INCOMING:
                return (PATH_INCOMING);

        case Q_QUEUE:
                return (PATH_QUEUE);

        case Q_CORRUPT:
                return (PATH_CORRUPT);

        default:
		fatalx("queue_fsqueue_getpath: unsupported queue kind.");
        }
	return NULL;
}

static int
fsqueue_message_path(enum queue_kind qkind, uint32_t msgid, char *buf, size_t len)
{
	if (qkind == Q_QUEUE)
		return bsnprintf(buf, len, "%s/%03x/%08x",
		    fsqueue_getpath(qkind),
		    msgid & 0xfff,
		    msgid);
	else
		return bsnprintf(buf, len, "%s/%08x",
		    fsqueue_getpath(qkind),
		    msgid);
}

static int
fsqueue_envelope_path(enum queue_kind qkind, uint64_t evpid, char *buf, size_t len)
{
	if (qkind == Q_QUEUE)
		return bsnprintf(buf, len, "%s/%03x/%08x%s/%016" PRIx64,
		    fsqueue_getpath(qkind),
		    evpid_to_msgid(evpid) & 0xfff,
		    evpid_to_msgid(evpid),
		    PATH_ENVELOPES, evpid);
	else
		return bsnprintf(buf, len, "%s/%08x%s/%016" PRIx64,
		    fsqueue_getpath(qkind),
		    evpid_to_msgid(evpid),
		    PATH_ENVELOPES, evpid);
}

static int
fsqueue_envelope_dump_atomic(char *dest, struct envelope *ep)
{
	FILE	*fp;

	fp = fopen(PATH_EVPTMP, "w");
	if (fp == NULL) {
		if (errno == ENOSPC || errno == ENFILE)
			goto tempfail;
		fatal("fsqueue_envelope_dump_atomic: open");
	}
	if (! fsqueue_dump_envelope_ascii(fp, ep)) {
		if (errno == ENOSPC)
			goto tempfail;
		fatal("fsqueue_envelope_dump_atomic: fwrite");
	}
	if (! safe_fclose(fp))
		goto tempfail;
	fp = NULL;

	if (rename(PATH_EVPTMP, dest) == -1) {
		if (errno == ENOSPC)
			goto tempfail;
		fatal("fsqueue_envelope_dump_atomic: rename");
	}

	return 1;

tempfail:
	if (fp)
		fclose(fp);
	if (unlink(PATH_EVPTMP) == -1)
		fatal("fsqueue_envelope_dump_atomic: unlink");

	return 0;
}

static int
fsqueue_envelope_create(enum queue_kind qkind, struct envelope *ep)
{
	char		evpname[MAXPATHLEN];
	u_int64_t	evpid;
	struct stat	sb;
	int		r;

again:
	evpid = queue_generate_evpid(evpid_to_msgid(ep->id));
	fsqueue_envelope_path(qkind, evpid, evpname, sizeof(evpname));
	if (stat(evpname, &sb) != -1 || errno != ENOENT)
		goto again;

	ep->creation = time(NULL);
	ep->id = evpid;

	if ((r = fsqueue_envelope_dump_atomic(evpname, ep)) == 0) {
		ep->creation = 0;
		ep->id = 0;
	}

	return (r);
}

static int
fsqueue_envelope_load(enum queue_kind qkind, struct envelope *ep)
{
	char pathname[MAXPATHLEN];
	FILE *fp;
	int  ret;

	fsqueue_envelope_path(qkind, ep->id, pathname, sizeof(pathname));

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
	char dest[MAXPATHLEN];

	fsqueue_envelope_path(qkind, ep->id, dest, sizeof(dest));

	return (fsqueue_envelope_dump_atomic(dest, ep));
}

static int
fsqueue_envelope_delete(enum queue_kind qkind, struct envelope *ep)
{
	char pathname[MAXPATHLEN];

	fsqueue_envelope_path(qkind, ep->id, pathname, sizeof(pathname));

	if (unlink(pathname) == -1) {
		log_debug("######: %s [errno: %d]", pathname, errno);
		fatal("fsqueue_envelope_delete: unlink");
	}

	*strrchr(pathname, '/') = '\0';

	if (rmdir(pathname) != -1)
		fsqueue_message_delete(qkind, evpid_to_msgid(ep->id));

	return 1;
}

static int
fsqueue_message_create(enum queue_kind qkind, u_int32_t *msgid)
{
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];
	struct stat sb;

again:
	*msgid = queue_generate_msgid();
	
	/* prevent possible collision later when moving to Q_QUEUE */
	fsqueue_message_path(Q_QUEUE, *msgid, rootdir, sizeof(rootdir));
	if (stat(rootdir, &sb) != -1 || errno != ENOENT)
		goto again;

	fsqueue_message_path(qkind, *msgid, rootdir, sizeof(rootdir));
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
fsqueue_message_commit(enum queue_kind qkind, u_int32_t msgid)
{
	char rootdir[MAXPATHLEN];
	char queuedir[MAXPATHLEN];
	char msgdir[MAXPATHLEN];

	fsqueue_message_path(qkind, msgid, rootdir, sizeof(rootdir));
	fsqueue_message_path(Q_QUEUE, msgid, msgdir, sizeof(msgdir));
	strlcpy(queuedir, msgdir, sizeof(queuedir));
	*strrchr(queuedir, '/') = '\0';

	if (mkdir(queuedir, 0700) == -1) {
		if (errno == ENOSPC)
			return 0;
		if (errno != EEXIST)
			fatal("fsqueue_message_commit: mkdir");
	}

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
	char path[MAXPATHLEN];

	fsqueue_message_path(qkind, msgid, path, sizeof(path));
	strlcat(path, PATH_MESSAGE, sizeof(path));

	if ((fd = open(path, O_RDONLY)) == -1)
		fatal("fsqueue_message_fd_r: open");

	return fd;
}

static int
fsqueue_message_fd_rw(enum queue_kind qkind, u_int32_t msgid)
{
	char path[MAXPATHLEN];

	fsqueue_message_path(qkind, msgid, path, sizeof(path));
	strlcat(path, PATH_MESSAGE, sizeof(path));

	return open(path, O_CREAT|O_EXCL|O_RDWR, 0600);
}

static int
fsqueue_message_delete(enum queue_kind qkind, u_int32_t msgid)
{
	char rootdir[MAXPATHLEN];

	fsqueue_message_path(qkind, msgid, rootdir, sizeof(rootdir));

	if (mvpurge(rootdir, PATH_PURGE) == -1)
		fatal("fsqueue_message_delete: mvpurge");

	return 1;
}

static int
fsqueue_message_corrupt(enum queue_kind qkind, u_int32_t msgid)
{
	char rootdir[MAXPATHLEN];
	char corruptdir[MAXPATHLEN];

	fsqueue_message_path(qkind, msgid, rootdir, sizeof(rootdir));
	fsqueue_message_path(Q_CORRUPT, msgid, corruptdir, sizeof(corruptdir));

	if (rename(rootdir, corruptdir) == -1)
		fatalx("fsqueue_message_corrupt: rename");

	return 1;
}

int
fsqueue_init(void)
{
	unsigned int	 n;
	char		*paths[] = { PATH_INCOMING, PATH_QUEUE, PATH_CORRUPT };
	char		 path[MAXPATHLEN];
	int		 ret;

	if (!fsqueue_envelope_path(Q_QUEUE, 0, path, sizeof(path)))
		errx(1, "cannot store envelope path in %s", PATH_QUEUE);
	if (!fsqueue_envelope_path(Q_INCOMING, 0, path, sizeof(path)))
		errx(1, "cannot store envelope path in %s", PATH_INCOMING);

	mvpurge(PATH_SPOOL PATH_INCOMING, PATH_SPOOL PATH_PURGE);

	ret = 1;
	for (n = 0; n < nitems(paths); n++) {
		strlcpy(path, PATH_SPOOL, sizeof(path));
		if (strlcat(path, paths[n], sizeof(path)) >= sizeof(path))
			errx(1, "path too long %s%s", PATH_SPOOL, paths[n]);

		if (ckdir(path, 0700, env->sc_pw->pw_uid, 0, 1) == 0)
			ret = 0;
	}

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
	if (kind == Q_INCOMING)
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
		if (! bsnprintf(q->path, sizeof(q->path), "%s/%s%s",
			fsqueue_getpath(q->kind), fname, PATH_ENVELOPES))
			fatalx("walk_queue_nobucket: snprintf");
		return (QWALK_RECURSE);
	case 1:
		return (QWALK_RETURN);
	}

	return (-1);
}

int
fsqueue_load_envelope_ascii(FILE *fp, struct envelope *ep)
{
	char *buf, *lbuf;
	char *field;
	size_t	len;
	enum envelope_field fields[] = {
		EVP_VERSION,
		EVP_ID,
		EVP_HOSTNAME,
		EVP_SOCKADDR,
		EVP_HELO,
		EVP_SENDER,
		EVP_RCPT,
		EVP_DEST,
		EVP_TYPE,
		EVP_CTIME,
		EVP_EXPIRE,
		EVP_RETRY,
		EVP_LASTTRY,
		EVP_FLAGS,
		EVP_ERRORLINE,
		EVP_MDA_METHOD,
		EVP_MDA_BUFFER,
		EVP_MDA_USER,
		EVP_MTA_RELAY_HOST,
		EVP_MTA_RELAY_PORT,
		EVP_MTA_RELAY_CERT,
		EVP_MTA_RELAY_FLAGS,
		EVP_MTA_RELAY_AUTHMAP
	};
	int	i;
	int	n;
	int	ret;

	n = sizeof(fields) / sizeof(enum envelope_field);
	bzero(ep, sizeof (*ep));
	lbuf = NULL;
	while ((buf = fgetln(fp, &len))) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			if ((lbuf = malloc(len + 1)) == NULL)
				err(1, NULL);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		for (i = 0; i < n; ++i) {
			field = envelope_ascii_field_name(fields[i]);
			len = strlen(field);
			if (! strncasecmp(field, buf, len)) {
				/* skip kw and tailing whitespaces */
				buf += len;
				while (*buf && isspace(*buf))
					buf++;

				/* we *want* ':' */
				if (*buf != ':')
					continue;
				buf++;

				/* skip whitespaces after separator */
				while (*buf && isspace(*buf))
				    buf++;

				ret = envelope_ascii_load(fields[i], ep, buf);
				if (ret == 0)
					goto err;
				break;
			}
		}

		/* unknown keyword */
		if (i == n)
			goto err;
	}
	free(lbuf);
	return 1;

err:
	free(lbuf);
	return 0;
}

int
fsqueue_dump_envelope_ascii(FILE *fp, struct envelope *ep)
{
	char	buf[8192];

	enum envelope_field fields[] = {
		EVP_VERSION,
		EVP_ID,
		EVP_TYPE,
		EVP_HELO,
		EVP_HOSTNAME,
		EVP_ERRORLINE,
		EVP_SOCKADDR,
		EVP_SENDER,
		EVP_RCPT,
		EVP_DEST,
		EVP_CTIME,
		EVP_LASTTRY,
		EVP_EXPIRE,
		EVP_RETRY,
		EVP_FLAGS
	};
	enum envelope_field mda_fields[] = {
		EVP_MDA_METHOD,
		EVP_MDA_BUFFER,
		EVP_MDA_USER
	};
	enum envelope_field mta_fields[] = {
		EVP_MTA_RELAY_HOST,
		EVP_MTA_RELAY_PORT,
		EVP_MTA_RELAY_CERT,
		EVP_MTA_RELAY_AUTHMAP,
		EVP_MTA_RELAY_FLAGS
	};
	enum envelope_field *pfields = NULL;
	int	i;
	int	n;

	n = sizeof(fields) / sizeof(enum envelope_field);
	for (i = 0; i < n; ++i) {
		bzero(buf, sizeof buf);
		if (! envelope_ascii_dump(fields[i], ep, buf, sizeof buf))
			goto err;
		if (buf[0] == '\0')
			continue;
		fprintf(fp, "%s: %s\n",
		    envelope_ascii_field_name(fields[i]), buf);
	}

	switch (ep->type) {
	case D_MDA:
		pfields = mda_fields;
		n = sizeof(mda_fields) / sizeof(enum envelope_field);
		break;
	case D_MTA:
		pfields = mta_fields;
		n = sizeof(mta_fields) / sizeof(enum envelope_field);
		break;
	case D_BOUNCE:
		/* nothing ! */
		break;
	default:
		goto err;
	}

	if (pfields) {
		for (i = 0; i < n; ++i) {
			bzero(buf, sizeof buf);
			if (! envelope_ascii_dump(pfields[i], ep, buf,
				sizeof buf))
				goto err;
			if (buf[0] == '\0')
				continue;
			fprintf(fp, "%s: %s\n",
			    envelope_ascii_field_name(pfields[i]), buf);
		}
	}

	if (fflush(fp) != 0)
		goto err;

	return 1;

err:
	return 0;
}

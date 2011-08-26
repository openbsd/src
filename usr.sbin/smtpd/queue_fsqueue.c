/*	$OpenBSD: queue_fsqueue.c,v 1.13 2011/08/26 14:39:47 chl Exp $	*/

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

#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <libgen.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

static char		*fsqueue_getpath(enum queue_kind);
/*static*/ u_int16_t	 fsqueue_hash(u_int32_t);

static int	fsqueue_envelope_load(enum queue_kind, struct envelope *);
static int	fsqueue_envelope_update(enum queue_kind, struct envelope *);
static int	fsqueue_envelope_delete(enum queue_kind, struct envelope *);

static int	fsqueue_message_create(enum queue_kind, u_int32_t *);
static int	fsqueue_message_commit(enum queue_kind, u_int32_t);
static int	fsqueue_message_fd_r(enum queue_kind, u_int32_t);
static int	fsqueue_message_fd_rw(enum queue_kind, u_int32_t);
static int	fsqueue_message_delete(enum queue_kind, u_int32_t);
static int	fsqueue_message_purge(enum queue_kind, u_int32_t);

int	fsqueue_init(void);
int	fsqueue_message(enum queue_kind, enum queue_op, u_int32_t *);
int	fsqueue_envelope(enum queue_kind, enum queue_op , struct envelope *);

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

        case Q_OFFLINE:
                return (PATH_OFFLINE);

        case Q_BOUNCE:
                return (PATH_BOUNCE);

        default:
		fatalx("queue_fsqueue_getpath: unsupported queue kind.");
        }
	return NULL;
}

/*static*/ u_int16_t
fsqueue_hash(u_int32_t h)
{
        return (h % DIRHASH_BUCKETS);
}

static int
fsqueue_envelope_create(enum queue_kind qkind, struct envelope *ep)
{
	char evpname[MAXPATHLEN];
	FILE *fp;
	int fd;
	u_int32_t rnd;
	u_int64_t evpid;

	fp = NULL;

again:
	rnd = (u_int32_t)arc4random();
	if (rnd == 0)
		goto again;
	evpid = ep->delivery.id | rnd;


	if (! bsnprintf(evpname, sizeof(evpname), "%s/%08x%s/%016llx",
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

	ep->delivery.creation = time(NULL);
	ep->delivery.id = evpid;

	if (qkind == Q_BOUNCE) {
		ep->delivery.lasttry = 0;
		ep->delivery.retry = 0;
	}

	if (fwrite(ep, sizeof (*ep), 1, fp) != 1) {
		if (errno == ENOSPC)
			goto tempfail;
		fatal("fsqueue_envelope_create: write");
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
	ep->delivery.creation = 0;
	ep->delivery.id = 0;

	return 0;
}

static int
fsqueue_envelope_load(enum queue_kind qkind, struct envelope *ep)
{
	char pathname[MAXPATHLEN];
	FILE *fp;

	if (! bsnprintf(pathname, sizeof(pathname), "%s/%04x/%08x%s/%016llx",
		fsqueue_getpath(qkind),
		fsqueue_hash(evpid_to_msgid(ep->delivery.id)),
		evpid_to_msgid(ep->delivery.id),
		PATH_ENVELOPES, ep->delivery.id))
		fatalx("fsqueue_envelope_load: snprintf");

	fp = fopen(pathname, "r");
	if (fp == NULL) {
		if (errno == ENOENT || errno == ENFILE)
			return 0;
		fatal("fsqueue_envelope_load: fopen");
	}
	if (fread(ep, sizeof (*ep), 1, fp) != 1)
		fatal("fsqueue_envelope_load: fread");
	fclose(fp);
	return 1;
}

static int
fsqueue_envelope_update(enum queue_kind qkind, struct envelope *ep)
{
	char temp[MAXPATHLEN];
	char dest[MAXPATHLEN];
	FILE *fp;
	u_int64_t batch_id;

	batch_id = ep->batch_id;
	ep->batch_id = 0;

	if (! bsnprintf(temp, sizeof(temp), "%s/envelope.tmp", PATH_QUEUE))
		fatalx("fsqueue_envelope_update");

	if (! bsnprintf(dest, sizeof(dest), "%s/%04x/%08x%s/%016llx",
		fsqueue_getpath(qkind),
		fsqueue_hash(evpid_to_msgid(ep->delivery.id)),
		evpid_to_msgid(ep->delivery.id),
		PATH_ENVELOPES, ep->delivery.id))
		fatal("fsqueue_envelope_update: snprintf");

	fp = fopen(temp, "w");
	if (fp == NULL) {
		if (errno == ENOSPC || errno == ENFILE)
			goto tempfail;
		fatal("fsqueue_envelope_update: open");
	}
	if (fwrite(ep, sizeof (*ep), 1, fp) != 1) {
		if (errno == ENOSPC)
			goto tempfail;
		fatal("fsqueue_envelope_update: fwrite");
	}
	if (! safe_fclose(fp))
		goto tempfail;

	if (rename(temp, dest) == -1) {
		if (errno == ENOSPC)
			goto tempfail;
		fatal("fsqueue_envelope_update: rename");
	}

	ep->batch_id = batch_id;
	return 1;

tempfail:
	if (unlink(temp) == -1)
		fatal("fsqueue_envelope_update: unlink");
	if (fp)
		fclose(fp);

	ep->batch_id = batch_id;
	return 0;
}

static int
fsqueue_envelope_delete(enum queue_kind qkind, struct envelope *ep)
{
	char pathname[MAXPATHLEN];
	u_int16_t hval;

	hval = fsqueue_hash(evpid_to_msgid(ep->delivery.id));

	if (! bsnprintf(pathname, sizeof(pathname), "%s/%04x/%08x%s/%016llx",
		fsqueue_getpath(qkind),
		hval,
		evpid_to_msgid(ep->delivery.id),
		PATH_ENVELOPES,
		ep->delivery.id))
		fatal("fsqueue_envelope_delete: snprintf");

	if (unlink(pathname) == -1)
		fatal("fsqueue_envelope_delete: unlink");

	if (! bsnprintf(pathname, sizeof(pathname), "%s/%04x/%08x%s", PATH_QUEUE,
		hval, evpid_to_msgid(ep->delivery.id), PATH_ENVELOPES))
		fatal("fsqueue_envelope_delete: snprintf");

	if (rmdir(pathname) != -1)
		fsqueue_message_delete(qkind, evpid_to_msgid(ep->delivery.id));

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
	*msgid = (u_int32_t)arc4random();
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
		if (! bsnprintf(msgpath, sizeof(msgpath), "%s/%04x/%08x/message",
			fsqueue_getpath(Q_QUEUE),
			fsqueue_hash(msgid_save),
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

	if (! bsnprintf(queuedir, sizeof(queuedir), "%s/%04x",
		fsqueue_getpath(Q_QUEUE), fsqueue_hash(msgid)))
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
	u_int16_t hval;

	if (qkind == Q_ENQUEUE || qkind == Q_INCOMING) {
		if (! bsnprintf(pathname, sizeof(pathname), "%s/%08x/message",
			fsqueue_getpath(qkind), msgid))
			fatal("fsqueue_message_fd_r: snprintf");
	}
	else {
		hval = fsqueue_hash(msgid);
		if (! bsnprintf(pathname, sizeof(pathname), "%s/%04x/%08x/message",
			fsqueue_getpath(qkind), hval, msgid))
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
	u_int16_t hval;

	hval = fsqueue_hash(msgid);
	if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%04x/%08x", PATH_QUEUE,
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

	if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%04x", PATH_QUEUE, hval))
		fatal("queue_delete_message: snprintf");

	rmdir(rootdir);

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


int
fsqueue_init(void)
{
	unsigned int	 n;
	char		*paths[] = { PATH_INCOMING, PATH_ENQUEUE, PATH_QUEUE,
				     PATH_PURGE, PATH_OFFLINE, PATH_BOUNCE };
	char		 pathname[MAXPATHLEN];
	struct stat	 sb;
	int		 ret;

	if (! bsnprintf(pathname, sizeof(pathname), "%s", PATH_SPOOL))
		fatal("snprintf");

	if (stat(pathname, &sb) == -1) {
		if (errno != ENOENT) {
			warn("stat: %s", pathname);
			return 0;
		}

		if (mkdir(pathname, 0711) == -1) {
			warn("mkdir: %s", pathname);
			return 0;
		}

		if (chown(pathname, 0, 0) == -1) {
			warn("chown: %s", pathname);
			return 0;
		}

		if (stat(pathname, &sb) == -1)
			err(1, "stat: %s", pathname);
	}

	/* check if it's a directory */
	if (!S_ISDIR(sb.st_mode)) {
		warnx("%s is not a directory", pathname);
		return 0;
	}

	/* check that it is owned by uid/gid */
	if (sb.st_uid != 0 || sb.st_gid != 0) {
		warnx("%s must be owned by root:wheel", pathname);
		return 0;
	}

	/* check permission */
	if ((sb.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR)) != (S_IRUSR|S_IWUSR|S_IXUSR) ||
	    (sb.st_mode & (S_IRGRP|S_IWGRP|S_IXGRP)) != S_IXGRP ||
	    (sb.st_mode & (S_IROTH|S_IWOTH|S_IXOTH)) != S_IXOTH) {
		warnx("%s must be rwx--x--x (0711)", pathname);
		return 0;
	}

	ret = 1;
	for (n = 0; n < nitems(paths); n++) {
		mode_t	mode;
		uid_t	owner;
		gid_t	group;

		if (!strcmp(paths[n], PATH_OFFLINE)) {
			mode = 01777;
			owner = 0;
			group = 0;
		} else {
			mode = 0700;
			owner = env->sc_pw->pw_uid;
			group = 0;
		}

		if (! bsnprintf(pathname, sizeof(pathname), "%s%s", PATH_SPOOL,
			paths[n]))
			fatal("snprintf");

		if (stat(pathname, &sb) == -1) {
			if (errno != ENOENT) {
				warn("stat: %s", pathname);
				ret = 0;
				continue;
			}

			/* chmod is deffered to avoid umask effect */
			if (mkdir(pathname, 0) == -1) {
				ret = 0;
				warn("mkdir: %s", pathname);
			}

			if (chown(pathname, owner, group) == -1) {
				ret = 0;
				warn("chown: %s", pathname);
			}

			if (chmod(pathname, mode) == -1) {
				ret = 0;
				warn("chmod: %s", pathname);
			}

			if (stat(pathname, &sb) == -1)
				err(1, "stat: %s", pathname);
		}

		/* check if it's a directory */
		if (!S_ISDIR(sb.st_mode)) {
			ret = 0;
			warnx("%s is not a directory", pathname);
		}

		/* check that it is owned by owner/group */
		if (sb.st_uid != owner) {
			ret = 0;
			warnx("%s is not owned by uid %d", pathname, owner);
		}
		if (sb.st_gid != group) {
			ret = 0;
			warnx("%s is not owned by gid %d", pathname, group);
		}

		/* check permission */
		if ((sb.st_mode & 07777) != mode) {
			char mode_str[12];

			ret = 0;
			strmode(mode, mode_str);
			mode_str[10] = '\0';
			warnx("%s must be %s (%o)", pathname, mode_str + 1, mode);
		}
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

	case QOP_PURGE:
		return fsqueue_message_purge(qkind, *msgid);

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

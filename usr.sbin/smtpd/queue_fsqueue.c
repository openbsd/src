/*	$OpenBSD: queue_fsqueue.c,v 1.5 2011/04/14 22:36:09 gilles Exp $	*/

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
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

static char		*fsqueue_getpath(enum queue_kind);
static u_int16_t	 fsqueue_hash(char *);

static int	fsqueue_envelope_load(struct smtpd *, enum queue_kind, struct message *);
static int	fsqueue_envelope_update(struct smtpd *, enum queue_kind, struct message *);
static int	fsqueue_envelope_delete(struct smtpd *, enum queue_kind, struct message *);

static int	fsqueue_message_fd_r(struct smtpd *, enum queue_kind, char *);
static int	fsqueue_message_fd_rw(struct smtpd *, enum queue_kind, char *);
static int	fsqueue_message_delete(struct smtpd *, enum queue_kind, char *);

int	fsqueue_init(struct smtpd *);
int	fsqueue_message(struct smtpd *, enum queue_kind,
    enum queue_op, char *);
int	fsqueue_envelope(struct smtpd *, enum queue_kind,
    enum queue_op , struct message *);

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

static u_int16_t
fsqueue_hash(char *msgid)
{
	u_int16_t h;

        for (h = 5381; *msgid; msgid++)
                h = ((h << 5) + h) + *msgid;
	
        return (h % DIRHASH_BUCKETS);
}

static int
fsqueue_envelope_load(struct smtpd *env, enum queue_kind qkind,
    struct message *envelope)
{
	char pathname[MAXPATHLEN];
	char msgid[MAX_ID_SIZE];
	FILE *fp;

	if (strlcpy(msgid, envelope->message_uid, sizeof(msgid)) >= sizeof(msgid))
		return 0;

	*strrchr(msgid, '.') = '\0';
	if (! bsnprintf(pathname, sizeof(pathname), "%s/%d/%s%s/%s",
		fsqueue_getpath(qkind),
		fsqueue_hash(msgid), msgid, PATH_ENVELOPES, envelope->message_uid))
		fatalx("fsqueue_envelope_load: snprintf");

	fp = fopen(pathname, "r");
	if (fp == NULL) {
		if (errno == ENOENT || errno == ENFILE)
			return 0;
		fatal("fsqueue_envelope_load: fopen");
	}
	if (fread(envelope, sizeof(struct message), 1, fp) != 1)
		fatal("fsqueue_envelope_load: fread");
	fclose(fp);
	return 1;
}

static int
fsqueue_envelope_update(struct smtpd *env, enum queue_kind qkind,
    struct message *envelope)
{
	char temp[MAXPATHLEN];
	char dest[MAXPATHLEN];
	FILE *fp;
	u_int64_t batch_id;

	batch_id = envelope->batch_id;
	envelope->batch_id = 0;

	if (! bsnprintf(temp, sizeof(temp), "%s/envelope.tmp", PATH_QUEUE))
		fatalx("fsqueue_envelope_update");

	if (! bsnprintf(dest, sizeof(dest), "%s/%d/%s%s/%s",
		fsqueue_getpath(qkind),
		fsqueue_hash(envelope->message_id),
		envelope->message_id,
		PATH_ENVELOPES, envelope->message_uid))
		fatal("fsqueue_envelope_update: snprintf");

	fp = fopen(temp, "w");
	if (fp == NULL) {
		if (errno == ENOSPC || errno == ENFILE)
			goto tempfail;
		fatal("fsqueue_envelope_update: open");
	}
	if (fwrite(envelope, sizeof(struct message), 1, fp) != 1) {
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

	envelope->batch_id = batch_id;
	return 1;

tempfail:
	if (unlink(temp) == -1)
		fatal("fsqueue_envelope_update: unlink");
	if (fp)
		fclose(fp);

	envelope->batch_id = batch_id;
	return 0;
}

static int
fsqueue_envelope_delete(struct smtpd *env, enum queue_kind qkind,
    struct message *envelope)
{
	char pathname[MAXPATHLEN];
	u_int16_t hval;

	hval = fsqueue_hash(envelope->message_id);

	if (! bsnprintf(pathname, sizeof(pathname), "%s/%d/%s%s/%s",
		fsqueue_getpath(qkind),
		hval, envelope->message_id, PATH_ENVELOPES,
		envelope->message_uid))
		fatal("fsqueue_envelope_delete: snprintf");

	if (unlink(pathname) == -1)
		fatal("fsqueue_envelope_delete: unlink");

	if (! bsnprintf(pathname, sizeof(pathname), "%s/%d/%s%s", PATH_QUEUE,
		hval, envelope->message_id, PATH_ENVELOPES))
		fatal("fsqueue_envelope_delete: snprintf");

	if (rmdir(pathname) != -1)
		fsqueue_message_delete(env, qkind, envelope->message_id);

	return 1;
}

static int
fsqueue_message_create(struct smtpd *env, enum queue_kind qkind, char *msgid)
{
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];
	char *queuepath = fsqueue_getpath(qkind);
	char msgpath[MAXPATHLEN];
	char lnkpath[MAXPATHLEN];
	char msgid_save[MAX_ID_SIZE];

	strlcpy(msgid_save, msgid, sizeof(msgid_save));

	if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%d.XXXXXXXXXXXXXXXX",
		queuepath, time(NULL)))
		fatalx("fsqueue_message_create: snprintf");

	if (mkdtemp(rootdir) == NULL) {
		if (errno == ENOSPC) {
			bzero(msgid, MAX_ID_SIZE);
			return 0;
		}
		fatal("fsqueue_message_create: mkdtemp");
	}

	if (strlcpy(msgid, rootdir + strlen(queuepath) + 1, MAX_ID_SIZE)
	    >= MAX_ID_SIZE)
		fatalx("fsqueue_message_create: truncation");

	if (! bsnprintf(evpdir, sizeof(evpdir), "%s%s", rootdir,
		PATH_ENVELOPES))
		fatalx("fsqueue_message_create: snprintf");

	if (mkdir(evpdir, 0700) == -1) {
		if (errno == ENOSPC) {
			rmdir(rootdir);
			bzero(msgid, MAX_ID_SIZE);
			return 0;
		}
		fatal("fsqueue_message_create: mkdir");
	}

	if (qkind == Q_BOUNCE) {
		if (! bsnprintf(msgpath, sizeof(msgpath), "%s/%d/%s/message",
			fsqueue_getpath(Q_QUEUE),
			queue_hash(msgid_save), msgid_save))
			return 0;

		if (! bsnprintf(lnkpath, sizeof(lnkpath), "%s/%s/message",
			fsqueue_getpath(Q_BOUNCE), msgid))
			return 0;
		
		if (link(msgpath, lnkpath) == -1)
			fatal("link");
	}

	return 1;
}

static int
fsqueue_message_commit(struct smtpd *env, enum queue_kind qkind, char *msgid)
{
	char rootdir[MAXPATHLEN];
	char queuedir[MAXPATHLEN];
	
	if (! bsnprintf(rootdir, sizeof(rootdir), "%s/%s",
		fsqueue_getpath(qkind), msgid))
		fatal("fsqueue_message_commit: snprintf");

	if (! bsnprintf(queuedir, sizeof(queuedir), "%s/%d",
		fsqueue_getpath(Q_QUEUE), fsqueue_hash(msgid)))
		fatal("fsqueue_message_commit: snprintf");
	
	if (mkdir(queuedir, 0700) == -1) {
		if (errno == ENOSPC)
			return 0;
		if (errno != EEXIST)
			fatal("fsqueue_message_commit: mkdir");
	}

	if (strlcat(queuedir, "/", sizeof(queuedir)) >= sizeof(queuedir) ||
	    strlcat(queuedir, msgid, sizeof(queuedir)) >=
	    sizeof(queuedir))
		fatalx("fsqueue_message_commit: truncation");

	if (rename(rootdir, queuedir) == -1) {
		if (errno == ENOSPC)
			return 0;
		fatal("fsqueue_message_commit: rename");
	}

	return 1;
}

static int
fsqueue_message_fd_r(struct smtpd *env, enum queue_kind qkind, char *msgid)
{
	int fd;
	char pathname[MAXPATHLEN];
	u_int16_t hval;

	if (qkind == Q_ENQUEUE || qkind == Q_INCOMING) {
		if (! bsnprintf(pathname, sizeof(pathname), "%s/%s/message",
			fsqueue_getpath(qkind), msgid))
			fatal("fsqueue_message_fd_r: snprintf");
	}
	else {
		hval = fsqueue_hash(msgid);
		if (! bsnprintf(pathname, sizeof(pathname), "%s/%d/%s/message",
			fsqueue_getpath(qkind), hval, msgid))
			fatal("fsqueue_message_fd_r: snprintf");
	}

	if ((fd = open(pathname, O_RDONLY)) == -1)
		fatal("fsqueue_message_fd_r: open");

	return fd;
}

static int
fsqueue_message_fd_rw(struct smtpd *env, enum queue_kind qkind, char *msgid)
{
	char pathname[MAXPATHLEN];
	
	if (! bsnprintf(pathname, sizeof(pathname), "%s/%s/message",
		fsqueue_getpath(qkind),
		msgid))
		fatal("fsqueue_message_fd_rw: snprintf");

	return open(pathname, O_CREAT|O_EXCL|O_RDWR, 0600);
}

static int
fsqueue_message_delete(struct smtpd *env, enum queue_kind qkind, char *msgid)
{
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];
	char msgpath[MAXPATHLEN];
	u_int16_t hval;

	hval = fsqueue_hash(msgid);
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

	return 1;
}

int
fsqueue_init(struct smtpd *env)
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
			group = env->sc_pw->pw_gid;
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
fsqueue_message(struct smtpd *env, enum queue_kind qkind,
    enum queue_op qop, char *msgid)
{
        switch (qop) {
        case QOP_CREATE:
		return fsqueue_message_create(env, qkind, msgid);

        case QOP_DELETE:
		return fsqueue_message_delete(env, qkind, msgid);

        case QOP_COMMIT:
		return fsqueue_message_commit(env, qkind, msgid);

        case QOP_FD_R:
                return fsqueue_message_fd_r(env, qkind, msgid);

        case QOP_FD_RW:
                return fsqueue_message_fd_rw(env, qkind, msgid);

        default:
		fatalx("queue_fsqueue_message: unsupported operation.");
        }

	return 0;
}

int
fsqueue_envelope(struct smtpd *env, enum queue_kind qkind,
    enum queue_op qop, struct message *envelope)
{
        switch (qop) {
        case QOP_CREATE:
                return 0;

        case QOP_DELETE:
		return fsqueue_envelope_delete(env, qkind, envelope);

        case QOP_LOAD:
		return fsqueue_envelope_load(env, qkind, envelope);

        case QOP_UPDATE:
		return fsqueue_envelope_update(env, qkind, envelope);

        default:
		fatalx("queue_fsqueue_envelope: unsupported operation.");
        }

	return 0;
}

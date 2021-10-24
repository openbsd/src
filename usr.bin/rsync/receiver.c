/*	$OpenBSD: receiver.c,v 1.31 2021/10/24 21:24:17 deraadt Exp $ */

/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2019 Florian Obser <florian@openbsd.org>
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
#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"

enum	pfdt {
	PFD_SENDER_IN = 0, /* input from the sender */
	PFD_UPLOADER_IN, /* uploader input from a local file */
	PFD_DOWNLOADER_IN, /* downloader input from a local file */
	PFD_SENDER_OUT, /* output to the sender */
	PFD__MAX
};

int
rsync_set_metadata(struct sess *sess, int newfile,
	int fd, const struct flist *f, const char *path)
{
	uid_t		 uid = (uid_t)-1;
	gid_t		 gid = (gid_t)-1;
	mode_t		 mode;
	struct timespec	 ts[2];

	/* Conditionally adjust file modification time. */

	if (sess->opts->preserve_times) {
		ts[0].tv_nsec = UTIME_NOW;
		ts[1].tv_sec = f->st.mtime;
		ts[1].tv_nsec = 0;
		if (futimens(fd, ts) == -1) {
			ERR("%s: futimens", path);
			return 0;
		}
		LOG4("%s: updated date", f->path);
	}

	/*
	 * Conditionally adjust identifiers.
	 * If we have an EPERM, report it but continue on: this just
	 * means that we're mapping into an unknown (or disallowed)
	 * group identifier.
	 */
	if (getuid() == 0 && sess->opts->preserve_uids)
		uid = f->st.uid;
	if (sess->opts->preserve_gids)
		gid = f->st.gid;

	mode = f->st.mode;
	if (uid != (uid_t)-1 || gid != (gid_t)-1) {
		if (fchown(fd, uid, gid) == -1) {
			if (errno != EPERM) {
				ERR("%s: fchown", path);
				return 0;
			}
			if (getuid() == 0)
				WARNX("%s: identity unknown or not available "
				    "to user.group: %u.%u", f->path, uid, gid);
		} else
			LOG4("%s: updated uid and/or gid", f->path);
		mode &= ~(S_ISTXT | S_ISUID | S_ISGID);
	}

	/* Conditionally adjust file permissions. */

	if (newfile || sess->opts->preserve_perms) {
		if (fchmod(fd, mode) == -1) {
			ERR("%s: fchmod", path);
			return 0;
		}
		LOG4("%s: updated permissions", f->path);
	}

	return 1;
}

int
rsync_set_metadata_at(struct sess *sess, int newfile, int rootfd,
	const struct flist *f, const char *path)
{
	uid_t		 uid = (uid_t)-1;
	gid_t		 gid = (gid_t)-1;
	mode_t		 mode;
	struct timespec	 ts[2];

	/* Conditionally adjust file modification time. */

	if (sess->opts->preserve_times) {
		ts[0].tv_nsec = UTIME_NOW;
		ts[1].tv_sec = f->st.mtime;
		ts[1].tv_nsec = 0;
		if (utimensat(rootfd, path, ts, AT_SYMLINK_NOFOLLOW) == -1) {
			ERR("%s: utimensat", path);
			return 0;
		}
		LOG4("%s: updated date", f->path);
	}

	/*
	 * Conditionally adjust identifiers.
	 * If we have an EPERM, report it but continue on: this just
	 * means that we're mapping into an unknown (or disallowed)
	 * group identifier.
	 */
	if (getuid() == 0 && sess->opts->preserve_uids)
		uid = f->st.uid;
	if (sess->opts->preserve_gids)
		gid = f->st.gid;

	mode = f->st.mode;
	if (uid != (uid_t)-1 || gid != (gid_t)-1) {
		if (fchownat(rootfd, path, uid, gid, AT_SYMLINK_NOFOLLOW) == -1) {
			if (errno != EPERM) {
				ERR("%s: fchownat", path);
				return 0;
			}
			if (getuid() == 0)
				WARNX("%s: identity unknown or not available "
				    "to user.group: %u.%u", f->path, uid, gid);
		} else
			LOG4("%s: updated uid and/or gid", f->path);
		mode &= ~(S_ISTXT | S_ISUID | S_ISGID);
	}

	/* Conditionally adjust file permissions. */

	if (newfile || sess->opts->preserve_perms) {
		if (fchmodat(rootfd, path, mode, AT_SYMLINK_NOFOLLOW) == -1) {
			ERR("%s: fchmodat", path);
			return 0;
		}
		LOG4("%s: updated permissions", f->path);
	}

	return 1;
}

/*
 * Pledges: unveil, unix, rpath, cpath, wpath, stdio, fattr, chown.
 * Pledges (dry-run): -unix, -cpath, -wpath, -fattr, -chown.
 */
int
rsync_receiver(struct sess *sess, int fdin, int fdout, const char *root)
{
	struct flist	*fl = NULL, *dfl = NULL;
	size_t		 i, flsz = 0, dflsz = 0;
	char		*tofree;
	int		 rc = 0, dfd = -1, phase = 0, c;
	int32_t		 ioerror;
	struct pollfd	 pfd[PFD__MAX];
	struct download	*dl = NULL;
	struct upload	*ul = NULL;
	mode_t		 oumask;

	if (pledge("stdio unix rpath wpath cpath dpath fattr chown getpw unveil", NULL) == -1)
		err(ERR_IPC, "pledge");

	/*
	 * Create the path for our destination directory, if we're not
	 * in dry-run mode (which would otherwise crash w/the pledge).
	 * This uses our current umask: we might set the permissions on
	 * this directory in post_dir().
	 */

	if (!sess->opts->dry_run) {
		if ((tofree = strdup(root)) == NULL)
			err(ERR_NOMEM, NULL);
		if (mkpath(tofree) < 0)
			err(ERR_FILE_IO, "%s: mkpath", tofree);
		free(tofree);
	}

	/*
	 * Make our entire view of the file-system be limited to what's
	 * in the root directory.
	 * This prevents us from accidentally (or "under the influence")
	 * writing into other parts of the file-system.
	 */
	if (sess->opts->basedir[0]) {
		/*
		 * XXX just unveil everything for read
		 * Could unveil each basedir or maybe a common path
		 * also the fact that relative path are relative to the
		 * root does not help.
		 */
		if (unveil("/", "r") == -1)
			err(ERR_IPC, "%s: unveil", root);
	}

	if (unveil(root, "rwc") == -1)
		err(ERR_IPC, "%s: unveil", root);

	if (unveil(NULL, NULL) == -1)
		err(ERR_IPC, "unveil");

	/* Client sends exclusions. */
	if (!sess->opts->server)
		send_rules(sess, fdout);

	/* Server receives exclusions if delete is on. */
	if (sess->opts->server && sess->opts->del)
		recv_rules(sess, fdin);

	/*
	 * Start by receiving the file list and our mystery number.
	 * These we're going to be touching on our local system.
	 */

	if (!flist_recv(sess, fdin, &fl, &flsz)) {
		ERRX1("flist_recv");
		goto out;
	}

	/* The IO error is sent after the file list. */

	if (!io_read_int(sess, fdin, &ioerror)) {
		ERRX1("io_read_int");
		goto out;
	} else if (ioerror != 0) {
		ERRX1("io_error is non-zero");
		goto out;
	}

	if (flsz == 0 && !sess->opts->server) {
		WARNX("receiver has empty file list: exiting");
		rc = 1;
		goto out;
	} else if (!sess->opts->server)
		LOG1("Transfer starting: %zu files", flsz);

	LOG2("%s: receiver destination", root);

	/*
	 * Disable umask() so we can set permissions fully.
	 * Then open the directory iff we're not in dry_run.
	 */

	oumask = umask(0);

	if (!sess->opts->dry_run) {
		dfd = open(root, O_RDONLY | O_DIRECTORY);
		if (dfd == -1)
			err(ERR_FILE_IO, "%s: open", root);
	}

	/*
	 * Begin by conditionally getting all files we have currently
	 * available in our destination.
	 */

	if (sess->opts->del &&
	    sess->opts->recursive &&
	    !flist_gen_dels(sess, root, &dfl, &dflsz, fl, flsz)) {
		ERRX1("flist_gen_local");
		goto out;
	}

	/* If we have a local set, go for the deletion. */

	if (!flist_del(sess, dfd, dfl, dflsz)) {
		ERRX1("flist_del");
		goto out;
	}

	/* Initialise poll events to listen from the sender. */

	pfd[PFD_SENDER_IN].fd = fdin;
	pfd[PFD_UPLOADER_IN].fd = -1;
	pfd[PFD_DOWNLOADER_IN].fd = -1;
	pfd[PFD_SENDER_OUT].fd = fdout;

	pfd[PFD_SENDER_IN].events = POLLIN;
	pfd[PFD_UPLOADER_IN].events = POLLIN;
	pfd[PFD_DOWNLOADER_IN].events = POLLIN;
	pfd[PFD_SENDER_OUT].events = POLLOUT;

	ul = upload_alloc(root, dfd, fdout, CSUM_LENGTH_PHASE1, fl, flsz,
	    oumask);

	if (ul == NULL) {
		ERRX1("upload_alloc");
		goto out;
	}

	dl = download_alloc(sess, fdin, fl, flsz, dfd);
	if (dl == NULL) {
		ERRX1("download_alloc");
		goto out;
	}

	LOG2("%s: ready for phase 1 data", root);

	for (;;) {
		if ((c = poll(pfd, PFD__MAX, poll_timeout)) == -1) {
			ERR("poll");
			goto out;
		} else if (c == 0) {
			ERRX("poll: timeout");
			goto out;
		}

		for (i = 0; i < PFD__MAX; i++)
			if (pfd[i].revents & (POLLERR|POLLNVAL)) {
				ERRX("poll: bad fd");
				goto out;
			} else if (pfd[i].revents & POLLHUP) {
				ERRX("poll: hangup");
				goto out;
			}

		/*
		 * If we have a read event and we're multiplexing, we
		 * might just have error messages in the pipe.
		 * It's important to flush these out so that we don't
		 * clog the pipe.
		 * Unset our polling status if there's nothing that
		 * remains in the pipe.
		 */

		if (sess->mplex_reads &&
		    (pfd[PFD_SENDER_IN].revents & POLLIN)) {
			if (!io_read_flush(sess, fdin)) {
				ERRX1("io_read_flush");
				goto out;
			} else if (sess->mplex_read_remain == 0)
				pfd[PFD_SENDER_IN].revents &= ~POLLIN;
		}


		/*
		 * We run the uploader if we have files left to examine
		 * (i < flsz) or if we have a file that we've opened and
		 * is read to mmap.
		 */

		if ((pfd[PFD_UPLOADER_IN].revents & POLLIN) ||
		    (pfd[PFD_SENDER_OUT].revents & POLLOUT)) {
			c = rsync_uploader(ul,
				&pfd[PFD_UPLOADER_IN].fd,
				sess, &pfd[PFD_SENDER_OUT].fd);
			if (c < 0) {
				ERRX1("rsync_uploader");
				goto out;
			}
		}

		/*
		 * We need to run the downloader when we either have
		 * read events from the sender or an asynchronous local
		 * open is ready.
		 * XXX: we don't disable PFD_SENDER_IN like with the
		 * uploader because we might stop getting error
		 * messages, which will otherwise clog up the pipes.
		 */

		if ((pfd[PFD_SENDER_IN].revents & POLLIN) ||
		    (pfd[PFD_DOWNLOADER_IN].revents & POLLIN)) {
			c = rsync_downloader(dl, sess,
				&pfd[PFD_DOWNLOADER_IN].fd);
			if (c < 0) {
				ERRX1("rsync_downloader");
				goto out;
			} else if (c == 0) {
				assert(phase == 0);
				phase++;
				LOG2("%s: receiver ready for phase 2 data", root);
				break;
			}

			/*
			 * FIXME: if we have any errors during the
			 * download, most notably files getting out of
			 * sync between the send and the receiver, then
			 * here we should bump our checksum length and
			 * go into the second phase.
			 */
		}
	}

	/* Properly close us out by progressing through the phases. */

	if (phase == 1) {
		if (!io_write_int(sess, fdout, -1)) {
			ERRX1("io_write_int");
			goto out;
		}
		if (!io_read_int(sess, fdin, &ioerror)) {
			ERRX1("io_read_int");
			goto out;
		}
		if (ioerror != -1) {
			ERRX("expected phase ack");
			goto out;
		}
	}

	/*
	 * Now all of our transfers are complete, so we can fix up our
	 * directory permissions.
	 */

	if (!rsync_uploader_tail(ul, sess)) {
		ERRX1("rsync_uploader_tail");
		goto out;
	}

	/* Process server statistics and say good-bye. */

	if (!sess_stats_recv(sess, fdin)) {
		ERRX1("sess_stats_recv");
		goto out;
	}
	if (!io_write_int(sess, fdout, -1)) {
		ERRX1("io_write_int");
		goto out;
	}

	LOG2("receiver finished updating");
	rc = 1;
out:
	if (dfd != -1)
		close(dfd);
	upload_free(ul);
	download_free(dl);
	flist_free(fl, flsz);
	flist_free(dfl, dflsz);
	return rc;
}

/*	$OpenBSD: uploader.c,v 1.33 2021/11/03 14:42:12 deraadt Exp $ */
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"

enum	uploadst {
	UPLOAD_FIND_NEXT = 0, /* find next to upload to sender */
	UPLOAD_WRITE, /* wait to write to sender */
	UPLOAD_FINISHED /* nothing more to do in phase */
};

/*
 * Used to keep track of data flowing from the receiver to the sender.
 * This is managed by the receiver process.
 */
struct	upload {
	enum uploadst	    state;
	char		   *buf; /* if not NULL, pending upload */
	size_t		    bufsz; /* size of buf */
	size_t		    bufmax; /* maximum size of buf */
	size_t		    bufpos; /* position in buf */
	size_t		    idx; /* current transfer index */
	mode_t		    oumask; /* umask for creating files */
	char		   *root; /* destination directory path */
	int		    rootfd; /* destination directory */
	size_t		    csumlen; /* checksum length */
	int		    fdout; /* write descriptor to sender */
	const struct flist *fl; /* file list */
	size_t		    flsz; /* size of file list */
	int		   *newdir; /* non-zero if mkdir'd */
};

/*
 * Log a directory by emitting the file and a trailing slash, just to
 * show the operator that we're a directory.
 */
static void
log_dir(struct sess *sess, const struct flist *f)
{
	size_t	 sz;

	if (sess->opts->server)
		return;
	sz = strlen(f->path);
	assert(sz > 0);
	LOG1("%s%s", f->path, (f->path[sz - 1] == '/') ? "" : "/");
}

/*
 * Log a link by emitting the file and the target, just to show the
 * operator that we're a link.
 */
static void
log_symlink(struct sess *sess, const struct flist *f)
{

	if (!sess->opts->server)
		LOG1("%s -> %s", f->path, f->link);
}

/*
 * Simply log the filename.
 */
static void
log_file(struct sess *sess, const struct flist *f)
{

	if (!sess->opts->server)
		LOG1("%s", f->path);
}

/*
 * Prepare the overall block set's metadata.
 * We always have at least one block.
 * The block size is an important part of the algorithm.
 * I use the same heuristic as the reference rsync, but implemented in a
 * bit more of a straightforward way.
 * In general, the individual block length is the rounded square root of
 * the total file size.
 * The minimum block length is 700.
 */
static void
init_blkset(struct blkset *p, off_t sz)
{
	double	 v;

	if (sz >= (BLOCK_SIZE_MIN * BLOCK_SIZE_MIN)) {
		/* Simple rounded-up integer square root. */

		v = sqrt(sz);
		p->len = ceil(v);

		/*
		 * Always be a multiple of eight.
		 * There's no reason to do this, but rsync does.
		 */

		if ((p->len % 8) > 0)
			p->len += 8 - (p->len % 8);
	} else
		p->len = BLOCK_SIZE_MIN;

	p->size = sz;
	if ((p->blksz = sz / p->len) == 0)
		p->rem = sz;
	else
		p->rem = sz % p->len;

	/* If we have a remainder, then we need an extra block. */

	if (p->rem)
		p->blksz++;
}

/*
 * For each block, prepare the block's metadata.
 * We use the mapped "map" file to set our checksums.
 */
static void
init_blk(struct blk *p, const struct blkset *set, off_t offs,
	size_t idx, const void *map, const struct sess *sess)
{

	p->idx = idx;
	/* Block length inherits for all but the last. */
	p->len = idx < set->blksz - 1 ? set->len : set->rem;
	p->offs = offs;

	p->chksum_short = hash_fast(map, p->len);
	hash_slow(map, p->len, p->chksum_long, sess);
}

/*
 * Handle a symbolic link.
 * If we encounter directories existing in the symbolic link's place,
 * then try to unlink the directory.
 * Otherwise, simply overwrite with the symbolic link by renaming.
 * Return <0 on failure 0 on success.
 */
static int
pre_symlink(struct upload *p, struct sess *sess)
{
	struct stat		 st;
	const struct flist	*f;
	int			 rc, newlink = 0, updatelink = 0;
	char			*b, *temp = NULL;

	f = &p->fl[p->idx];
	assert(S_ISLNK(f->st.mode));

	if (!sess->opts->preserve_links) {
		WARNX("%s: ignoring symlink", f->path);
		return 0;
	}
	if (sess->opts->dry_run) {
		log_symlink(sess, f);
		return 0;
	}

	/*
	 * See if the symlink already exists.
	 * If it's a directory, then try to unlink the directory prior
	 * to overwriting with a symbolic link.
	 * If it's a non-directory, we just overwrite it.
	 */

	assert(p->rootfd != -1);
	rc = fstatat(p->rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW);

	if (rc == -1 && errno != ENOENT) {
		ERR("%s: fstatat", f->path);
		return -1;
	}
	if (rc != -1 && !S_ISLNK(st.st_mode)) {
		if (S_ISDIR(st.st_mode) &&
		    unlinkat(p->rootfd, f->path, AT_REMOVEDIR) == -1) {
			ERR("%s: unlinkat", f->path);
			return -1;
		}
		rc = -1;
	}

	/*
	 * If the symbolic link already exists, then make sure that it
	 * points to the correct place.
	 */

	if (rc != -1) {
		b = symlinkat_read(p->rootfd, f->path);
		if (b == NULL) {
			ERRX1("symlinkat_read");
			return -1;
		}
		if (strcmp(f->link, b)) {
			free(b);
			b = NULL;
			LOG3("%s: updating symlink: %s", f->path, f->link);
			updatelink = 1;
		}
		free(b);
		b = NULL;
	}

	/*
	 * Create the temporary file as a symbolic link, then rename the
	 * temporary file as the real one, overwriting anything there.
	 */

	if (rc == -1 || updatelink) {
		LOG3("%s: creating symlink: %s", f->path, f->link);
		if (mktemplate(&temp, f->path, sess->opts->recursive) == -1) {
			ERRX1("mktemplate");
			return -1;
		}
		if (mkstemplinkat(f->link, p->rootfd, temp) == NULL) {
			ERR("mkstemplinkat");
			free(temp);
			return -1;
		}
		newlink = 1;
	}

	rsync_set_metadata_at(sess, newlink,
		p->rootfd, f, newlink ? temp : f->path);

	if (newlink) {
		if (renameat(p->rootfd, temp, p->rootfd, f->path) == -1) {
			ERR("%s: renameat %s", temp, f->path);
			(void)unlinkat(p->rootfd, temp, 0);
			free(temp);
			return -1;
		}
		free(temp);
	}

	log_symlink(sess, f);
	return 0;
}

/*
 * See pre_symlink(), but for devices.
 * FIXME: this is very similar to the other pre_xxx() functions.
 * Return <0 on failure 0 on success.
 */
static int
pre_dev(struct upload *p, struct sess *sess)
{
	struct stat		 st;
	const struct flist	*f;
	int			 rc, newdev = 0, updatedev = 0;
	char			*temp = NULL;

	f = &p->fl[p->idx];
	assert(S_ISBLK(f->st.mode) || S_ISCHR(f->st.mode));

	if (!sess->opts->devices || getuid() != 0) {
		WARNX("skipping non-regular file %s", f->path);
		return 0;
	}
	if (sess->opts->dry_run) {
		log_file(sess, f);
		return 0;
	}

	/*
	 * See if the dev already exists.
	 * If a non-device exists in its place, we'll replace that.
	 * If it replaces a directory, remove the directory first.
	 */

	assert(p->rootfd != -1);
	rc = fstatat(p->rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW);

	if (rc == -1 && errno != ENOENT) {
		ERR("%s: fstatat", f->path);
		return -1;
	}
	if (rc != -1 && !(S_ISBLK(st.st_mode) || S_ISCHR(st.st_mode))) {
		if (S_ISDIR(st.st_mode) &&
		    unlinkat(p->rootfd, f->path, AT_REMOVEDIR) == -1) {
			ERR("%s: unlinkat", f->path);
			return -1;
		}
		rc = -1;
	}

	/* Make sure existing device is of the correct type. */

	if (rc != -1) {
		if ((f->st.mode & (S_IFCHR|S_IFBLK)) !=
		    (st.st_mode & (S_IFCHR|S_IFBLK)) ||
		    f->st.rdev != st.st_rdev) {
			LOG3("%s: updating device", f->path);
			updatedev = 1;
		}
	}

	if (rc == -1 || updatedev) {
		newdev = 1;
		if (mktemplate(&temp, f->path, sess->opts->recursive) == -1) {
			ERRX1("mktemplate");
			return -1;
		}
		if (mkstempnodat(p->rootfd, temp,
		    f->st.mode & (S_IFCHR|S_IFBLK), f->st.rdev) == NULL) {
			ERR("mkstempnodat");
			free(temp);
			return -1;
		}
	}

	rsync_set_metadata_at(sess, newdev,
	    p->rootfd, f, newdev ? temp : f->path);

	if (newdev) {
		if (renameat(p->rootfd, temp, p->rootfd, f->path) == -1) {
			ERR("%s: renameat %s", temp, f->path);
			(void)unlinkat(p->rootfd, temp, 0);
			free(temp);
			return -1;
		}
		free(temp);
	}

	log_file(sess, f);
	return 0;
}

/*
 * See pre_symlink(), but for FIFOs.
 * FIXME: this is very similar to the other pre_xxx() functions.
 * Return <0 on failure 0 on success.
 */
static int
pre_fifo(struct upload *p, struct sess *sess)
{
	struct stat		 st;
	const struct flist	*f;
	int			 rc, newfifo = 0;
	char			*temp = NULL;

	f = &p->fl[p->idx];
	assert(S_ISFIFO(f->st.mode));

	if (!sess->opts->specials) {
		WARNX("skipping non-regular file %s", f->path);
		return 0;
	}
	if (sess->opts->dry_run) {
		log_file(sess, f);
		return 0;
	}

	/*
	 * See if the fifo already exists.
	 * If it exists as a non-FIFO, unlink it (if a directory) then
	 * mark it from replacement.
	 */

	assert(p->rootfd != -1);
	rc = fstatat(p->rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW);

	if (rc == -1 && errno != ENOENT) {
		ERR("%s: fstatat", f->path);
		return -1;
	}
	if (rc != -1 && !S_ISFIFO(st.st_mode)) {
		if (S_ISDIR(st.st_mode) &&
		    unlinkat(p->rootfd, f->path, AT_REMOVEDIR) == -1) {
			ERR("%s: unlinkat", f->path);
			return -1;
		}
		rc = -1;
	}

	if (rc == -1) {
		newfifo = 1;
		if (mktemplate(&temp, f->path, sess->opts->recursive) == -1) {
			ERRX1("mktemplate");
			return -1;
		}
		if (mkstempfifoat(p->rootfd, temp) == NULL) {
			ERR("mkstempfifoat");
			free(temp);
			return -1;
		}
	}

	rsync_set_metadata_at(sess, newfifo,
		p->rootfd, f, newfifo ? temp : f->path);

	if (newfifo) {
		if (renameat(p->rootfd, temp, p->rootfd, f->path) == -1) {
			ERR("%s: renameat %s", temp, f->path);
			(void)unlinkat(p->rootfd, temp, 0);
			free(temp);
			return -1;
		}
		free(temp);
	}

	log_file(sess, f);
	return 0;
}

/*
 * See pre_symlink(), but for socket files.
 * FIXME: this is very similar to the other pre_xxx() functions.
 * Return <0 on failure 0 on success.
 */
static int
pre_sock(struct upload *p, struct sess *sess)
{
	struct stat		 st;
	const struct flist	*f;
	int			 rc, newsock = 0;
	char			*temp = NULL;

	f = &p->fl[p->idx];
	assert(S_ISSOCK(f->st.mode));

	if (!sess->opts->specials) {
		WARNX("skipping non-regular file %s", f->path);
		return 0;
	}
	if (sess->opts->dry_run) {
		log_file(sess, f);
		return 0;
	}

	/*
	 * See if the fifo already exists.
	 * If it exists as a non-FIFO, unlink it (if a directory) then
	 * mark it from replacement.
	 */

	assert(p->rootfd != -1);
	rc = fstatat(p->rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW);

	if (rc == -1 && errno != ENOENT) {
		ERR("%s: fstatat", f->path);
		return -1;
	}
	if (rc != -1 && !S_ISSOCK(st.st_mode)) {
		if (S_ISDIR(st.st_mode) &&
		    unlinkat(p->rootfd, f->path, AT_REMOVEDIR) == -1) {
			ERR("%s: unlinkat", f->path);
			return -1;
		}
		rc = -1;
	}

	if (rc == -1) {
		newsock = 1;
		if (mktemplate(&temp, f->path, sess->opts->recursive) == -1) {
			ERRX1("mktemplate");
			return -1;
		}
		if (mkstempsock(p->root, temp) == NULL) {
			ERR("mkstempsock");
			free(temp);
			return -1;
		}
	}

	rsync_set_metadata_at(sess, newsock,
		p->rootfd, f, newsock ? temp : f->path);

	if (newsock) {
		if (renameat(p->rootfd, temp, p->rootfd, f->path) == -1) {
			ERR("%s: renameat %s", temp, f->path);
			(void)unlinkat(p->rootfd, temp, 0);
			free(temp);
			return -1;
		}
		free(temp);
	}

	log_file(sess, f);
	return 0;
}

/*
 * If not found, create the destination directory in prefix order.
 * Create directories using the existing umask.
 * Return <0 on failure 0 on success.
 */
static int
pre_dir(const struct upload *p, struct sess *sess)
{
	struct stat	 st;
	int		 rc;
	const struct flist *f;

	f = &p->fl[p->idx];
	assert(S_ISDIR(f->st.mode));

	if (!sess->opts->recursive) {
		WARNX("%s: ignoring directory", f->path);
		return 0;
	}
	if (sess->opts->dry_run) {
		log_dir(sess, f);
		return 0;
	}

	assert(p->rootfd != -1);
	rc = fstatat(p->rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW);

	if (rc == -1 && errno != ENOENT) {
		ERR("%s: fstatat", f->path);
		return -1;
	}
	if (rc != -1 && !S_ISDIR(st.st_mode)) {
		ERRX("%s: not a directory", f->path);
		return -1;
	} else if (rc != -1) {
		/*
		 * FIXME: we should fchmod the permissions here as well,
		 * as we may locally have shut down writing into the
		 * directory and that doesn't work.
		 */
		LOG3("%s: updating directory", f->path);
		return 0;
	}

	/*
	 * We want to make the directory with default permissions (using
	 * our old umask, which we've since unset), then adjust
	 * permissions (assuming preserve_perms or new) afterward in
	 * case it's u-w or something.
	 */

	LOG3("%s: creating directory", f->path);
	if (mkdirat(p->rootfd, f->path, 0777 & ~p->oumask) == -1) {
		ERR("%s: mkdirat", f->path);
		return -1;
	}

	p->newdir[p->idx] = 1;
	log_dir(sess, f);
	return 0;
}

/*
 * Process the directory time and mode for "idx" in the file list.
 * Returns zero on failure, non-zero on success.
 */
static int
post_dir(struct sess *sess, const struct upload *u, size_t idx)
{
	struct timespec	 tv[2];
	int		 rc;
	struct stat	 st;
	const struct flist *f;

	f = &u->fl[idx];
	assert(S_ISDIR(f->st.mode));

	/* We already warned about the directory in pre_process_dir(). */

	if (!sess->opts->recursive)
		return 1;
	if (sess->opts->dry_run)
		return 1;

	if (fstatat(u->rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW) == -1) {
		ERR("%s: fstatat", f->path);
		return 0;
	}
	if (!S_ISDIR(st.st_mode)) {
		WARNX("%s: not a directory", f->path);
		return 0;
	}

	/*
	 * Update the modification time if we're a new directory *or* if
	 * we're preserving times and the time has changed.
	 * FIXME: run rsync_set_metadata()?
	 */

	if (u->newdir[idx] ||
	    (sess->opts->preserve_times &&
	     st.st_mtime != f->st.mtime)) {
		tv[0].tv_sec = time(NULL);
		tv[0].tv_nsec = 0;
		tv[1].tv_sec = f->st.mtime;
		tv[1].tv_nsec = 0;
		rc = utimensat(u->rootfd, f->path, tv, 0);
		if (rc == -1) {
			ERR("%s: utimensat", f->path);
			return 0;
		}
		LOG4("%s: updated date", f->path);
	}

	/*
	 * Update the mode if we're a new directory *or* if we're
	 * preserving modes and it has changed.
	 */

	if (u->newdir[idx] ||
	    (sess->opts->preserve_perms && st.st_mode != f->st.mode)) {
		rc = fchmodat(u->rootfd, f->path, f->st.mode, 0);
		if (rc == -1) {
			ERR("%s: fchmodat", f->path);
			return 0;
		}
		LOG4("%s: updated mode", f->path);
	}

	return 1;
}

/*
 * Check if file exists in the specified root directory.
 * Returns:
 *    -1 on error
 *     0 if file is considered the same
 *     1 if file exists and is possible match
 *     2 if file exists but quick check failed
 *     3 if file does not exist
 * The stat pointer st is only valid for 0, 1, and 2 returns.
 */
static int
check_file(int rootfd, const struct flist *f, struct stat *st)
{
	if (fstatat(rootfd, f->path, st, AT_SYMLINK_NOFOLLOW) == -1) {
		if (errno == ENOENT)
			return 3;

		ERR("%s: fstatat", f->path);
		return -1;
	}

	/* non-regular file needs attention */
	if (!S_ISREG(st->st_mode))
		return 2;

	/* quick check if file is the same */
	/* TODO: add support for --checksum, --size-only and --ignore-times */
	if (st->st_size == f->st.size) {
		if (st->st_mtime == f->st.mtime)
			return 0;
		return 1;
	}

	/* file needs attention */
	return 2;
}

/*
 * Try to open the file at the current index.
 * If the file does not exist, returns with >0.
 * Return <0 on failure, 0 on success w/nothing to be done, >0 on
 * success and the file needs attention.
 */
static int
pre_file(const struct upload *p, int *filefd, off_t *size,
    struct sess *sess)
{
	const struct flist *f;
	struct stat st;
	int i, rc, match = -1;

	f = &p->fl[p->idx];
	assert(S_ISREG(f->st.mode));

	if (sess->opts->dry_run) {
		log_file(sess, f);
		if (!io_write_int(sess, p->fdout, p->idx)) {
			ERRX1("io_write_int");
			return -1;
		}
		return 0;
	}

	if (sess->opts->max_size >= 0 && f->st.size > sess->opts->max_size) {
		WARNX("skipping over max-size file %s", f->path);
		return 0;
	}
	if (sess->opts->min_size >= 0 && f->st.size < sess->opts->min_size) {
		WARNX("skipping under min-size file %s", f->path);
		return 0;
	}

	/*
	 * For non dry-run cases, we'll write the acknowledgement later
	 * in the rsync_uploader() function.
	 */

	*size = 0;
	*filefd = -1;

	rc = check_file(p->rootfd, f, &st);
	if (rc == -1)
		return -1;
	if (rc == 2 && !S_ISREG(st.st_mode)) {
		if (S_ISDIR(st.st_mode) &&
		    unlinkat(p->rootfd, f->path, AT_REMOVEDIR) == -1) {
			ERR("%s: unlinkat", f->path);
			return -1;
		}
	}
	if (rc == 0) {
		if (!rsync_set_metadata_at(sess, 0, p->rootfd, f, f->path)) {
			ERRX1("rsync_set_metadata");
			return -1;
		}
		LOG3("%s: skipping: up to date", f->path);
		return 0;
	}

	/* check alternative locations for better match */
	for (i = 0; sess->opts->basedir[i] != NULL; i++) {
		const char *root = sess->opts->basedir[i];
		int dfd, x;

		dfd = openat(p->rootfd, root, O_RDONLY | O_DIRECTORY);
		if (dfd == -1)
			err(ERR_FILE_IO, "%s: openat", root);
		x = check_file(dfd, f, &st);
		/* found a match */
		if (x == 0) {
			if (rc >= 0) {
				/* found better match, delete file in rootfd */
				if (unlinkat(p->rootfd, f->path, 0) == -1 &&
				    errno != ENOENT) {
					ERR("%s: unlinkat", f->path);
					return -1;
				}
			}
			LOG3("%s: skipping: up to date in %s", f->path, root);
			/* TODO: depending on mode link or copy file */
			close(dfd);
			return 0;
		} else if (x == 1 && match == -1) {
			/* found a local file that is a close match */
			match = i;
		}
		close(dfd);
	}
	if (match != -1) {
		/* copy match from basedir into root as a start point */
		copy_file(p->rootfd, sess->opts->basedir[match], f);
		if (fstatat(p->rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW) ==
		    -1) {
			ERR("%s: fstatat", f->path);
			return -1;
		}
	}

	*size = st.st_size;
	*filefd = openat(p->rootfd, f->path, O_RDONLY | O_NOFOLLOW);
	if (*filefd == -1 && errno != ENOENT) {
		ERR("%s: openat", f->path);
		return -1;
	}

	/* file needs attention */
	return 1;
}

/*
 * Allocate an uploader object in the correct state to start.
 * Returns NULL on failure or the pointer otherwise.
 * On success, upload_free() must be called with the allocated pointer.
 */
struct upload *
upload_alloc(const char *root, int rootfd, int fdout,
	size_t clen, const struct flist *fl, size_t flsz, mode_t msk)
{
	struct upload	*p;

	if ((p = calloc(1, sizeof(struct upload))) == NULL) {
		ERR("calloc");
		return NULL;
	}

	p->state = UPLOAD_FIND_NEXT;
	p->oumask = msk;
	p->root = strdup(root);
	if (p->root == NULL) {
		ERR("strdup");
		free(p);
		return NULL;
	}
	p->rootfd = rootfd;
	p->csumlen = clen;
	p->fdout = fdout;
	p->fl = fl;
	p->flsz = flsz;
	p->newdir = calloc(flsz, sizeof(int));
	if (p->newdir == NULL) {
		ERR("calloc");
		free(p->root);
		free(p);
		return NULL;
	}
	return p;
}

/*
 * Perform all cleanups and free.
 * Passing a NULL to this function is ok.
 */
void
upload_free(struct upload *p)
{

	if (p == NULL)
		return;
	free(p->root);
	free(p->newdir);
	free(p->buf);
	free(p);
}

/*
 * Iterates through all available files and conditionally gets the file
 * ready for processing to check whether it's up to date.
 * If not up to date or empty, sends file information to the sender.
 * If returns 0, we've processed all files there are to process.
 * If returns >0, we're waiting for POLLIN or POLLOUT data.
 * Otherwise returns <0, which is an error.
 */
int
rsync_uploader(struct upload *u, int *fileinfd,
	struct sess *sess, int *fileoutfd)
{
	struct blkset	    blk;
	void		   *mbuf, *bufp;
	ssize_t		    msz;
	size_t		    i, pos, sz;
	off_t		    offs, filesize;
	int		    c;

	/* Once finished this should never get called again. */
	assert(u->state != UPLOAD_FINISHED);

	/*
	 * If we have an upload in progress, then keep writing until the
	 * buffer has been fully written.
	 * We must only have the output file descriptor working and also
	 * have a valid buffer to write.
	 */

	if (u->state == UPLOAD_WRITE) {
		assert(u->buf != NULL);
		assert(*fileoutfd != -1);
		assert(*fileinfd == -1);

		/*
		 * Unfortunately, we need to chunk these: if we're
		 * the server side of things, then we're multiplexing
		 * output and need to wrap this in chunks.
		 * This is a major deficiency of rsync.
		 * FIXME: add a "fast-path" mode that simply dumps out
		 * the buffer non-blocking if we're not mplexing.
		 */

		if (u->bufpos < u->bufsz) {
			sz = MAX_CHUNK < (u->bufsz - u->bufpos) ?
				MAX_CHUNK : (u->bufsz - u->bufpos);
			c = io_write_buf(sess, u->fdout,
				u->buf + u->bufpos, sz);
			if (c == 0) {
				ERRX1("io_write_nonblocking");
				return -1;
			}
			u->bufpos += sz;
			if (u->bufpos < u->bufsz)
				return 1;
		}

		/*
		 * Let the UPLOAD_FIND_NEXT state handle things if we
		 * finish, as we'll need to write a POLLOUT message and
		 * not have a writable descriptor yet.
		 */

		u->state = UPLOAD_FIND_NEXT;
		u->idx++;
		return 1;
	}

	/*
	 * If we invoke the uploader without a file currently open, then
	 * we iterate through til the next available regular file and
	 * start the opening process.
	 * This means we must have the output file descriptor working.
	 */

	if (u->state == UPLOAD_FIND_NEXT) {
		assert(*fileinfd == -1);
		assert(*fileoutfd != -1);

		for ( ; u->idx < u->flsz; u->idx++) {
			if (S_ISDIR(u->fl[u->idx].st.mode))
				c = pre_dir(u, sess);
			else if (S_ISLNK(u->fl[u->idx].st.mode))
				c = pre_symlink(u, sess);
			else if (S_ISREG(u->fl[u->idx].st.mode))
				c = pre_file(u, fileinfd, &filesize, sess);
			else if (S_ISBLK(u->fl[u->idx].st.mode) ||
			    S_ISCHR(u->fl[u->idx].st.mode))
				c = pre_dev(u, sess);
			else if (S_ISFIFO(u->fl[u->idx].st.mode))
				c = pre_fifo(u, sess);
			else if (S_ISSOCK(u->fl[u->idx].st.mode))
				c = pre_sock(u, sess);
			else
				c = 0;

			if (c < 0)
				return -1;
			else if (c > 0)
				break;
		}

		/*
		 * Whether we've finished writing files or not, we
		 * disable polling on the output channel.
		 */

		*fileoutfd = -1;
		if (u->idx == u->flsz) {
			assert(*fileinfd == -1);
			if (!io_write_int(sess, u->fdout, -1)) {
				ERRX1("io_write_int");
				return -1;
			}
			u->state = UPLOAD_FINISHED;
			LOG4("uploader: finished");
			return 0;
		}

		/* Go back to the event loop, if necessary. */

		u->state = UPLOAD_WRITE;
	}

	/* Initialies our blocks. */

	assert(u->state == UPLOAD_WRITE);
	memset(&blk, 0, sizeof(struct blkset));
	blk.csum = u->csumlen;

	if (*fileinfd != -1 && filesize > 0) {
		init_blkset(&blk, filesize);
		assert(blk.blksz);

		blk.blks = calloc(blk.blksz, sizeof(struct blk));
		if (blk.blks == NULL) {
			ERR("calloc");
			close(*fileinfd);
			*fileinfd = -1;
			return -1;
		}

		if ((mbuf = malloc(blk.len)) == NULL) {
			ERR("malloc");
			close(*fileinfd);
			*fileinfd = -1;
			free(blk.blks);
			return -1;
		}

		offs = 0;
		i = 0;
		do {
			msz = pread(*fileinfd, mbuf, blk.len, offs);
			if ((size_t)msz != blk.len && (size_t)msz != blk.rem) {
				ERR("pread");
				close(*fileinfd);
				*fileinfd = -1;
				free(mbuf);
				free(blk.blks);
				return -1;
			}
			init_blk(&blk.blks[i], &blk, offs, i, mbuf, sess);
			offs += blk.len;
			LOG3(
			    "i=%ld, offs=%lld, msz=%ld, blk.len=%lu, blk.rem=%lu",
			    i, offs, msz, blk.len, blk.rem);
			i++;
		} while (i < blk.blksz);

		free(mbuf);
		close(*fileinfd);
		*fileinfd = -1;
		LOG3("%s: mapped %jd B with %zu blocks",
		    u->fl[u->idx].path, (intmax_t)blk.size,
		    blk.blksz);
	} else {
		if (*fileinfd != -1) {
			close(*fileinfd);
			*fileinfd = -1;
		}
		blk.len = MAX_CHUNK; /* Doesn't matter. */
		LOG3("%s: not mapped", u->fl[u->idx].path);
	}

	assert(*fileinfd == -1);

	/* Make sure the block metadata buffer is big enough. */

	u->bufsz =
	    sizeof(int32_t) + /* identifier */
	    sizeof(int32_t) + /* block count */
	    sizeof(int32_t) + /* block length */
	    sizeof(int32_t) + /* checksum length */
	    sizeof(int32_t) + /* block remainder */
	    blk.blksz *
	    (sizeof(int32_t) + /* short checksum */
	    blk.csum); /* long checksum */

	if (u->bufsz > u->bufmax) {
		if ((bufp = realloc(u->buf, u->bufsz)) == NULL) {
			ERR("realloc");
			free(blk.blks);
			return -1;
		}
		u->buf = bufp;
		u->bufmax = u->bufsz;
	}

	u->bufpos = pos = 0;
	io_buffer_int(u->buf, &pos, u->bufsz, u->idx);
	io_buffer_int(u->buf, &pos, u->bufsz, blk.blksz);
	io_buffer_int(u->buf, &pos, u->bufsz, blk.len);
	io_buffer_int(u->buf, &pos, u->bufsz, blk.csum);
	io_buffer_int(u->buf, &pos, u->bufsz, blk.rem);
	for (i = 0; i < blk.blksz; i++) {
		io_buffer_int(u->buf, &pos, u->bufsz,
			blk.blks[i].chksum_short);
		io_buffer_buf(u->buf, &pos, u->bufsz,
			blk.blks[i].chksum_long, blk.csum);
	}
	assert(pos == u->bufsz);

	/* Reenable the output poller and clean up. */

	*fileoutfd = u->fdout;
	free(blk.blks);
	return 1;
}

/*
 * Fix up the directory permissions and times post-order.
 * We can't fix up directory permissions in place because the server may
 * want us to have overly-tight permissions---say, those that don't
 * allow writing into the directory.
 * We also need to do our directory times post-order because making
 * files within the directory will change modification times.
 * Returns zero on failure, non-zero on success.
 */
int
rsync_uploader_tail(struct upload *u, struct sess *sess)
{
	size_t	 i;


	if (!sess->opts->preserve_times &&
	    !sess->opts->preserve_perms)
		return 1;

	LOG2("fixing up directory times and permissions");

	for (i = 0; i < u->flsz; i++)
		if (S_ISDIR(u->fl[i].st.mode))
			if (!post_dir(sess, u, i))
				return 0;

	return 1;
}

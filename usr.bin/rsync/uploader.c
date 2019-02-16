/*	$Id: uploader.c,v 1.8 2019/02/16 05:30:28 deraadt Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

enum	uploadst {
	UPLOAD_FIND_NEXT = 0, /* find next to upload to sender */
	UPLOAD_WRITE_LOCAL, /* wait to write to sender */
	UPLOAD_READ_LOCAL, /* wait to read from local file */
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
	LOG1(sess, "%s%s", f->path, ('/' == f->path[sz - 1]) ? "" : "/");
}

/*
 * Log a link by emitting the file and the target, just to show the
 * operator that we're a link.
 */
static void
log_link(struct sess *sess, const struct flist *f)
{

	if (!sess->opts->server)
		LOG1(sess, "%s -> %s", f->path, f->link);
}

/*
 * Simply log the filename.
 */
static void
log_file(struct sess *sess, const struct flist *f)
{

	if (!sess->opts->server)
		LOG1(sess, "%s", f->path);
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

	assert(map != MAP_FAILED);

	/* Block length inherits for all but the last. */

	p->idx = idx;
	p->len = idx < set->blksz - 1 ? set->len : set->rem;
	p->offs = offs;

	p->chksum_short = hash_fast(map + offs, p->len);
	hash_slow(map + offs, p->len, p->chksum_long, sess);
}

/*
 * Return <0 on failure 0 on success.
 */
static int
pre_link(struct upload *p, struct sess *sess)
{
	int		 rc, newlink = 0;
	char		*b;
	struct stat	 st;
	struct timespec	 tv[2];
	const struct flist *f;

	f = &p->fl[p->idx];
	assert(S_ISLNK(f->st.mode));

	if (!sess->opts->preserve_links) {
		WARNX(sess, "%s: ignoring symlink", f->path);
		return 0;
	} else if (sess->opts->dry_run) {
		log_link(sess, f);
		return 0;
	}

	/* See if the symlink already exists. */

	assert(p->rootfd != -1);
	rc = fstatat(p->rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW);
	if (rc != -1 && !S_ISLNK(st.st_mode)) {
		WARNX(sess, "%s: not a symlink", f->path);
		return -1;
	} else if (rc == -1 && errno != ENOENT) {
		WARN(sess, "%s: fstatat", f->path);
		return -1;
	}

	/*
	 * If the symbolic link already exists, then make sure that it
	 * points to the correct place.
	 * FIXME: does symlinkat() set permissions on the link using the
	 * destination file or the default umask?
	 * Do we need a fchmod in here as well?
	 */

	if (rc == -1) {
		LOG3(sess, "%s: creating "
			"symlink: %s", f->path, f->link);
		if (symlinkat(f->link, p->rootfd, f->path) == -1) {
			WARN(sess, "%s: symlinkat", f->path);
			return -1;
		}
		newlink = 1;
	} else {
		b = symlinkat_read(sess, p->rootfd, f->path);
		if (b == NULL) {
			ERRX1(sess, "%s: symlinkat_read", f->path);
			return -1;
		}
		if (strcmp(f->link, b)) {
			free(b);
			b = NULL;
			LOG3(sess, "%s: updating "
				"symlink: %s", f->path, f->link);
			if (unlinkat(p->rootfd, f->path, 0) == -1) {
				WARN(sess, "%s: unlinkat", f->path);
				return -1;
			}
			if (symlinkat(f->link, p->rootfd, f->path) == -1) {
				WARN(sess, "%s: symlinkat", f->path);
				return -1;
			}
			newlink = 1;
		}
		free(b);
	}

	/* 
	 * Optionally preserve times/perms on the symlink.
	 * FIXME: run rsync_set_metadata()?
	 */

	if (sess->opts->preserve_times) {
		struct timeval now;

		gettimeofday(&now, NULL);
		TIMEVAL_TO_TIMESPEC(&now, &tv[0]);
		tv[1].tv_sec = f->st.mtime;
		tv[1].tv_nsec = 0;
		rc = utimensat(p->rootfd, f->path, tv, AT_SYMLINK_NOFOLLOW);
		if (rc == -1) {
			ERR(sess, "%s: utimensat", f->path);
			return -1;
		}
		LOG4(sess, "%s: updated symlink date", f->path);
	}

	/*
	 * FIXME: if newlink is set because we updated the symlink, we
	 * want to carry over the permissions from the last.
	 */

	if (newlink || sess->opts->preserve_perms) {
		rc = fchmodat(p->rootfd, f->path, f->st.mode, AT_SYMLINK_NOFOLLOW);
		if (rc == -1) {
			ERR(sess, "%s: fchmodat", f->path);
			return -1;
		}
		LOG4(sess, "%s: updated symlink mode", f->path);
	}

	log_link(sess, f);
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
		WARNX(sess, "%s: ignoring directory", f->path);
		return 0;
	} else if (sess->opts->dry_run) {
		log_dir(sess, f);
		return 0;
	}

	assert(p->rootfd != -1);
	rc = fstatat(p->rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW);
	if (rc == -1 && errno != ENOENT) {
		WARN(sess, "%s: fstatat", f->path);
		return -1;
	} else if (rc != -1 && !S_ISDIR(st.st_mode)) {
		WARNX(sess, "%s: not a directory", f->path);
		return -1;
	} else if (rc != -1) {
		/*
		 * FIXME: we should fchmod the permissions here as well,
		 * as we may locally have shut down writing into the
		 * directory and that doesn't work.
		 */
		LOG3(sess, "%s: updating directory", f->path);
		return 0;
	}

	/*
	 * We want to make the directory with default permissions (using
	 * our old umask, which we've since unset), then adjust
	 * permissions (assuming preserve_perms or new) afterward in
	 * case it's u-w or something.
	 */

	LOG3(sess, "%s: creating directory", f->path);
	if (mkdirat(p->rootfd, f->path, 0777 & ~p->oumask) == -1) {
		WARN(sess, "%s: mkdirat", f->path);
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
	else if (sess->opts->dry_run)
		return 1;

	if (fstatat(u->rootfd, f->path, &st, AT_SYMLINK_NOFOLLOW) == -1) {
		ERR(sess, "%s: fstatat", f->path);
		return 0;
	} else if (!S_ISDIR(st.st_mode)) {
		WARNX(sess, "%s: not a directory", f->path);
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
			ERR(sess, "%s: utimensat", f->path);
			return 0;
		}
		LOG4(sess, "%s: updated date", f->path);
	}

	/*
	 * Update the mode if we're a new directory *or* if we're
	 * preserving modes and it has changed.
	 */

	if (u->newdir[idx] ||
	    (sess->opts->preserve_perms &&
	     st.st_mode != f->st.mode)) {
		rc = fchmodat(u->rootfd, f->path, f->st.mode, 0);
		if (rc == -1) {
			ERR(sess, "%s: fchmodat", f->path);
			return 0;
		}
		LOG4(sess, "%s: updated mode", f->path);
	}

	return 1;
}

/*
 * Try to open the file at the current index.
 * If the file does not exist, returns with success.
 * Return <0 on failure, 0 on success w/nothing to be done, >0 on
 * success and the file needs attention.
 */
static int
pre_file(const struct upload *p, int *filefd, struct sess *sess)
{
	const struct flist *f;

	f = &p->fl[p->idx];
	assert(S_ISREG(f->st.mode));

	if (sess->opts->dry_run) {
		log_file(sess, f);
		if (!io_write_int(sess, p->fdout, p->idx)) {
			ERRX1(sess, "io_write_int");
			return -1;
		}
		return 0;
	}

	/*
	 * For non dry-run cases, we'll write the acknowledgement later
	 * in the rsync_uploader() function because we need to wait for
	 * the open() call to complete.
	 * If the call to openat() fails with ENOENT, there's a
	 * fast-path between here and the write function, so we won't do
	 * any blocking between now and then.
	 */

	*filefd = openat(p->rootfd, f->path,
		O_RDONLY | O_NOFOLLOW | O_NONBLOCK, 0);
	if (*filefd != -1 || errno == ENOENT)
		return 1;
	ERR(sess, "%s: openat", f->path);
	return -1;
}

/*
 * Allocate an uploader object in the correct state to start.
 * Returns NULL on failure or the pointer otherwise.
 * On success, upload_free() must be called with the allocated pointer.
 */
struct upload *
upload_alloc(struct sess *sess, int rootfd, int fdout,
	size_t clen, const struct flist *fl, size_t flsz, mode_t msk)
{
	struct upload	*p;

	if ((p = calloc(1, sizeof(struct upload))) == NULL) {
		ERR(sess, "calloc");
		return NULL;
	}

	p->state = UPLOAD_FIND_NEXT;
	p->oumask = msk;
	p->rootfd = rootfd;
	p->csumlen = clen;
	p->fdout = fdout;
	p->fl = fl;
	p->flsz = flsz;
	p->newdir = calloc(flsz, sizeof(int));
	if (p->newdir == NULL) {
		ERR(sess, "calloc");
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
	struct stat	    st;
	void		   *map, *bufp;
	size_t		    i, mapsz, pos, sz;
	off_t		    offs;
	int		    c;
	const struct flist *f;

	/* This should never get called. */

	assert(u->state != UPLOAD_FINISHED);

	/*
	 * If we have an upload in progress, then keep writing until the
	 * buffer has been fully written.
	 * We must only have the output file descriptor working and also
	 * have a valid buffer to write.
	 */

	if (u->state == UPLOAD_WRITE_LOCAL) {
		assert(NULL != u->buf);
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
				ERRX1(sess, "io_write_nonblocking");
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
				c = pre_link(u, sess);
			else if (S_ISREG(u->fl[u->idx].st.mode))
				c = pre_file(u, fileinfd, sess);
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
				ERRX1(sess, "io_write_int");
				return -1;
			}
			u->state = UPLOAD_FINISHED;
			LOG4(sess, "uploader: finished");
			return 0;
		}

		/* Go back to the event loop, if necessary. */

		u->state = -1 == *fileinfd ?
			UPLOAD_WRITE_LOCAL : UPLOAD_READ_LOCAL;
		if (u->state == UPLOAD_READ_LOCAL)
			return 1;
	}

	/*
	 * If an input file is open, stat it and see if it's already up
	 * to date, in which case close it and go to the next one.
	 * Either way, we don't have a write channel open.
	 */

	if (u->state == UPLOAD_READ_LOCAL) {
		assert(*fileinfd != -1);
		assert(*fileoutfd == -1);
		f = &u->fl[u->idx];

		if (fstat(*fileinfd, &st) == -1) {
			ERR(sess, "%s: fstat", f->path);
			close(*fileinfd);
			*fileinfd = -1;
			return -1;
		} else if (!S_ISREG(st.st_mode)) {
			ERRX(sess, "%s: not regular", f->path);
			close(*fileinfd);
			*fileinfd = -1;
			return -1;
		}

		if (st.st_size == f->st.size &&
		    st.st_mtime == f->st.mtime) {
			LOG3(sess, "%s: skipping: up to date", f->path);
#if 0
			/* Not yet: investigate behaviour. */
			if (!rsync_set_metadata
			    (sess, 0, *fileinfd, f, f->path)) {
				ERRX1(sess, "rsync_set_metadata");
				close(*fileinfd);
				*fileinfd = -1;
				return -1;
			}
#endif
			close(*fileinfd);
			*fileinfd = -1;
			*fileoutfd = u->fdout;
			u->state = UPLOAD_FIND_NEXT;
			u->idx++;
			return 1;
		}

		/* Fallthrough... */

		u->state = UPLOAD_WRITE_LOCAL;
	}

	/* Initialies our blocks. */

	assert(u->state == UPLOAD_WRITE_LOCAL);
	memset(&blk, 0, sizeof(struct blkset));
	blk.csum = u->csumlen;

	if (*fileinfd != -1 && st.st_size > 0) {
		mapsz = st.st_size;
		map = mmap(NULL, mapsz, PROT_READ, MAP_SHARED, *fileinfd, 0);
		if (map == MAP_FAILED) {
			WARN(sess, "%s: mmap", u->fl[u->idx].path);
			close(*fileinfd);
			*fileinfd = -1;
			return -1;
		}

		init_blkset(&blk, st.st_size);
		assert(blk.blksz);

		blk.blks = calloc(blk.blksz, sizeof(struct blk));
		if (blk.blks == NULL) {
			ERR(sess, "calloc");
			munmap(map, mapsz);
			close(*fileinfd);
			*fileinfd = -1;
			return -1;
		}

		offs = 0;
		for (i = 0; i < blk.blksz; i++) {
			init_blk(&blk.blks[i],
				&blk, offs, i, map, sess);
			offs += blk.len;
		}

		munmap(map, mapsz);
		close(*fileinfd);
		*fileinfd = -1;
		LOG3(sess, "%s: mapped %jd B with %zu blocks",
			u->fl[u->idx].path, (intmax_t)blk.size,
			blk.blksz);
	} else {
		if (*fileinfd != -1) {
			close(*fileinfd);
			*fileinfd = -1;
		}
		blk.len = MAX_CHUNK; /* Doesn't matter. */
		LOG3(sess, "%s: not mapped", u->fl[u->idx].path);
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
			ERR(sess, "realloc");
			return -1;
		}
		u->buf = bufp;
		u->bufmax = u->bufsz;
	}

	u->bufpos = pos = 0;
	io_buffer_int(sess, u->buf, &pos, u->bufsz, u->idx);
	io_buffer_int(sess, u->buf, &pos, u->bufsz, blk.blksz);
	io_buffer_int(sess, u->buf, &pos, u->bufsz, blk.len);
	io_buffer_int(sess, u->buf, &pos, u->bufsz, blk.csum);
	io_buffer_int(sess, u->buf, &pos, u->bufsz, blk.rem);
	for (i = 0; i < blk.blksz; i++) {
		io_buffer_int(sess, u->buf, &pos, u->bufsz,
			blk.blks[i].chksum_short);
		io_buffer_buf(sess, u->buf, &pos, u->bufsz,
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

	LOG2(sess, "fixing up directory times and permissions");

	for (i = 0; i < u->flsz; i++)
		if (S_ISDIR(u->fl[i].st.mode))
			if (!post_dir(sess, u, i))
				return 0;

	return 1;
}

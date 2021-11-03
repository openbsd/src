/*	$OpenBSD: flist.c,v 1.36 2021/11/03 14:42:12 deraadt Exp $ */
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
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <limits.h>
#include <inttypes.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

/*
 * We allocate our file list in chunk sizes so as not to do it one by
 * one.
 * Preferrably we get one or two allocation.
 */
#define	FLIST_CHUNK_SIZE (1024)

/*
 * These flags are part of the rsync protocol.
 * They are sent as the first byte for a file transmission and encode
 * information that affects subsequent transmissions.
 */
#define FLIST_TOP_LEVEL	 0x0001 /* needed for remote --delete */
#define FLIST_MODE_SAME  0x0002 /* mode is repeat */
#define	FLIST_RDEV_SAME  0x0004 /* rdev is repeat */
#define	FLIST_UID_SAME	 0x0008 /* uid is repeat */
#define	FLIST_GID_SAME	 0x0010 /* gid is repeat */
#define	FLIST_NAME_SAME  0x0020 /* name is repeat */
#define FLIST_NAME_LONG	 0x0040 /* name >255 bytes */
#define FLIST_TIME_SAME  0x0080 /* time is repeat */

/*
 * Required way to sort a filename list.
 */
static int
flist_cmp(const void *p1, const void *p2)
{
	const struct flist *f1 = p1, *f2 = p2;

	return strcmp(f1->wpath, f2->wpath);
}

/*
 * Deduplicate our file list (which may be zero-length).
 * Returns zero on failure, non-zero on success.
 */
static int
flist_dedupe(struct flist **fl, size_t *sz)
{
	size_t		 i, j;
	struct flist	*new;
	struct flist	*f, *fnext;

	if (*sz == 0)
		return 1;

	/* Create a new buffer, "new", and copy. */

	new = calloc(*sz, sizeof(struct flist));
	if (new == NULL) {
		ERR("calloc");
		return 0;
	}

	for (i = j = 0; i < *sz - 1; i++) {
		f = &(*fl)[i];
		fnext = &(*fl)[i + 1];

		if (strcmp(f->wpath, fnext->wpath)) {
			new[j++] = *f;
			continue;
		}

		/*
		 * Our working (destination) paths are the same.
		 * If the actual file is the same (as given on the
		 * command-line), then we can just discard the first.
		 * Otherwise, we need to bail out: it means we have two
		 * different files with the relative path on the
		 * destination side.
		 */

		if (strcmp(f->path, fnext->path) == 0) {
			new[j++] = *f;
			i++;
			WARNX("%s: duplicate path: %s",
			    f->wpath, f->path);
			free(fnext->path);
			free(fnext->link);
			fnext->path = fnext->link = NULL;
			continue;
		}

		ERRX("%s: duplicate working path for "
		    "possibly different file: %s, %s",
		    f->wpath, f->path, fnext->path);
		free(new);
		return 0;
	}

	/* Don't forget the last entry. */

	if (i == *sz - 1)
		new[j++] = (*fl)[i];

	/*
	 * Reassign to the deduplicated array.
	 * If we started out with *sz > 0, which we check for at the
	 * beginning, then we'll always continue having *sz > 0.
	 */

	free(*fl);
	*fl = new;
	*sz = j;
	assert(*sz);
	return 1;
}

/*
 * We're now going to find our top-level directories.
 * This only applies to recursive mode.
 * If we have the first element as the ".", then that's the "top
 * directory" of our transfer.
 * Otherwise, mark up all top-level directories in the set.
 * XXX: the FLIST_TOP_LEVEL flag should indicate what is and what isn't
 * a top-level directory, but I'm not sure if GPL rsync(1) respects it
 * the same way.
 */
static void
flist_topdirs(struct sess *sess, struct flist *fl, size_t flsz)
{
	size_t		 i;
	const char	*cp;

	if (!sess->opts->recursive)
		return;

	if (flsz && strcmp(fl[0].wpath, ".")) {
		for (i = 0; i < flsz; i++) {
			if (!S_ISDIR(fl[i].st.mode))
				continue;
			cp = strchr(fl[i].wpath, '/');
			if (cp != NULL && cp[1] != '\0')
				continue;
			fl[i].st.flags |= FLSTAT_TOP_DIR;
			LOG4("%s: top-level", fl[i].wpath);
		}
	} else if (flsz) {
		fl[0].st.flags |= FLSTAT_TOP_DIR;
		LOG4("%s: top-level", fl[0].wpath);
	}
}

/*
 * Filter through the fts() file information.
 * We want directories (pre-order), regular files, and symlinks.
 * Everything else is skipped and possibly warned about.
 * Return zero to skip, non-zero to examine.
 */
static int
flist_fts_check(struct sess *sess, FTSENT *ent)
{

	if (ent->fts_info == FTS_F  ||
	    ent->fts_info == FTS_D ||
	    ent->fts_info == FTS_SL ||
	    ent->fts_info == FTS_SLNONE)
		return 1;

	if (ent->fts_info == FTS_DC) {
		WARNX("%s: directory cycle", ent->fts_path);
	} else if (ent->fts_info == FTS_DNR) {
		errno = ent->fts_errno;
		WARN("%s: unreadable directory", ent->fts_path);
	} else if (ent->fts_info == FTS_DOT) {
		WARNX("%s: skipping dot-file", ent->fts_path);
	} else if (ent->fts_info == FTS_ERR) {
		errno = ent->fts_errno;
		WARN("%s", ent->fts_path);
	} else if (ent->fts_info == FTS_DEFAULT) {
		if ((sess->opts->devices && (S_ISBLK(ent->fts_statp->st_mode) ||
		    S_ISCHR(ent->fts_statp->st_mode))) ||
		    (sess->opts->specials &&
		    (S_ISFIFO(ent->fts_statp->st_mode) ||
		    S_ISSOCK(ent->fts_statp->st_mode)))) {
			return 1;
		}
		WARNX("%s: skipping special", ent->fts_path);
	} else if (ent->fts_info == FTS_NS) {
		errno = ent->fts_errno;
		WARN("%s: could not stat", ent->fts_path);
	}

	return 0;
}

/*
 * Copy necessary elements in "st" into the fields of "f".
 */
static void
flist_copy_stat(struct flist *f, const struct stat *st)
{
	f->st.mode = st->st_mode;
	f->st.uid = st->st_uid;
	f->st.gid = st->st_gid;
	f->st.size = st->st_size;
	f->st.mtime = st->st_mtime;
	f->st.rdev = st->st_rdev;
}

void
flist_free(struct flist *f, size_t sz)
{
	size_t	 i;

	if (f == NULL)
		return;

	for (i = 0; i < sz; i++) {
		free(f[i].path);
		free(f[i].link);
	}
	free(f);
}

/*
 * Serialise our file list (which may be zero-length) to the wire.
 * Makes sure that the receiver isn't going to block on sending us
 * return messages on the log channel.
 * Return zero on failure, non-zero on success.
 */
int
flist_send(struct sess *sess, int fdin, int fdout, const struct flist *fl,
    size_t flsz)
{
	size_t		 i, sz, gidsz = 0, uidsz = 0;
	uint8_t		 flag;
	const struct flist *f;
	const char	*fn;
	struct ident	*gids = NULL, *uids = NULL;
	int		 rc = 0;

	/* Double-check that we've no pending multiplexed data. */

	LOG2("sending file metadata list: %zu", flsz);

	for (i = 0; i < flsz; i++) {
		f = &fl[i];
		fn = f->wpath;
		sz = strlen(f->wpath);
		assert(sz > 0);
		assert(sz < INT32_MAX);

		/*
		 * If applicable, unclog the read buffer.
		 * This happens when the receiver has a lot of log
		 * messages and all we're doing is sending our file list
		 * without checking for messages.
		 */

		if (sess->mplex_reads &&
		    io_read_check(fdin) &&
		    !io_read_flush(sess, fdin)) {
			ERRX1("io_read_flush");
			goto out;
		}

		/*
		 * For ease, make all of our filenames be "long"
		 * regardless their actual length.
		 * This also makes sure that we don't transmit a zero
		 * byte unintentionally.
		 */

		flag = FLIST_NAME_LONG;
		if ((FLSTAT_TOP_DIR & f->st.flags))
			flag |= FLIST_TOP_LEVEL;

		LOG3("%s: sending file metadata: "
			"size %jd, mtime %jd, mode %o",
			fn, (intmax_t)f->st.size,
			(intmax_t)f->st.mtime, f->st.mode);

		/* Now write to the wire. */
		/* FIXME: buffer this. */

		if (!io_write_byte(sess, fdout, flag)) {
			ERRX1("io_write_byte");
			goto out;
		} else if (!io_write_int(sess, fdout, sz)) {
			ERRX1("io_write_int");
			goto out;
		} else if (!io_write_buf(sess, fdout, fn, sz)) {
			ERRX1("io_write_buf");
			goto out;
		} else if (!io_write_long(sess, fdout, f->st.size)) {
			ERRX1("io_write_long");
			goto out;
		} else if (!io_write_uint(sess, fdout, (uint32_t)f->st.mtime)) {
			ERRX1("io_write_uint");
			goto out;
		} else if (!io_write_uint(sess, fdout, f->st.mode)) {
			ERRX1("io_write_uint");
			goto out;
		}

		/* Conditional part: uid. */

		if (sess->opts->preserve_uids) {
			if (!io_write_uint(sess, fdout, f->st.uid)) {
				ERRX1("io_write_uint");
				goto out;
			}
			if (!idents_add(0, &uids, &uidsz, f->st.uid)) {
				ERRX1("idents_add");
				goto out;
			}
		}

		/* Conditional part: gid. */

		if (sess->opts->preserve_gids) {
			if (!io_write_uint(sess, fdout, f->st.gid)) {
				ERRX1("io_write_uint");
				goto out;
			}
			if (!idents_add(1, &gids, &gidsz, f->st.gid)) {
				ERRX1("idents_add");
				goto out;
			}
		}

		/* Conditional part: devices & special files. */

		if ((sess->opts->devices && (S_ISBLK(f->st.mode) ||
		    S_ISCHR(f->st.mode))) ||
		    (sess->opts->specials && (S_ISFIFO(f->st.mode) ||
		    S_ISSOCK(f->st.mode)))) {
			if (!io_write_int(sess, fdout, f->st.rdev)) {
				ERRX1("io_write_int");
				goto out;
			}
		}

		/* Conditional part: link. */

		if (S_ISLNK(f->st.mode) &&
		    sess->opts->preserve_links) {
			fn = f->link;
			sz = strlen(f->link);
			assert(sz < INT32_MAX);
			if (!io_write_int(sess, fdout, sz)) {
				ERRX1("io_write_int");
				goto out;
			}
			if (!io_write_buf(sess, fdout, fn, sz)) {
				ERRX1("io_write_buf");
				goto out;
			}
		}

		if (S_ISREG(f->st.mode))
			sess->total_size += f->st.size;
	}

	/* Signal end of file list. */

	if (!io_write_byte(sess, fdout, 0)) {
		ERRX1("io_write_byte");
		goto out;
	}

	/* Conditionally write identifier lists. */

	if (sess->opts->preserve_uids && !sess->opts->numeric_ids) {
		LOG2("sending uid list: %zu", uidsz);
		if (!idents_send(sess, fdout, uids, uidsz)) {
			ERRX1("idents_send");
			goto out;
		}
	}

	if (sess->opts->preserve_gids && !sess->opts->numeric_ids) {
		LOG2("sending gid list: %zu", gidsz);
		if (!idents_send(sess, fdout, gids, gidsz)) {
			ERRX1("idents_send");
			goto out;
		}
	}

	rc = 1;
out:
	idents_free(gids, gidsz);
	idents_free(uids, uidsz);
	return rc;
}

/*
 * Read the filename of a file list.
 * This is the most expensive part of the file list transfer, so a lot
 * of attention has gone into transmitting as little as possible.
 * Micro-optimisation, but whatever.
 * Fills in "f" with the full path on success.
 * Returns zero on failure, non-zero on success.
 */
static int
flist_recv_name(struct sess *sess, int fd, struct flist *f, uint8_t flags,
    char last[PATH_MAX])
{
	uint8_t		 bval;
	size_t		 partial = 0;
	size_t		 pathlen = 0, len;

	/*
	 * Read our filename.
	 * If we have FLIST_NAME_SAME, we inherit some of the last
	 * transmitted name.
	 * If we have FLIST_NAME_LONG, then the string length is greater
	 * than byte-size.
	 */

	if (FLIST_NAME_SAME & flags) {
		if (!io_read_byte(sess, fd, &bval)) {
			ERRX1("io_read_byte");
			return 0;
		}
		partial = bval;
	}

	/* Get the (possibly-remaining) filename length. */

	if (FLIST_NAME_LONG & flags) {
		if (!io_read_size(sess, fd, &pathlen)) {
			ERRX1("io_read_size");
			return 0;
		}
	} else {
		if (!io_read_byte(sess, fd, &bval)) {
			ERRX1("io_read_byte");
			return 0;
		}
		pathlen = bval;
	}

	/* Allocate our full filename length. */
	/* FIXME: maximum pathname length. */

	if ((len = pathlen + partial) == 0) {
		ERRX("security violation: zero-length pathname");
		return 0;
	}

	if ((f->path = malloc(len + 1)) == NULL) {
		ERR("malloc");
		return 0;
	}
	f->path[len] = '\0';

	if (FLIST_NAME_SAME & flags)
		memcpy(f->path, last, partial);

	if (!io_read_buf(sess, fd, f->path + partial, pathlen)) {
		ERRX1("io_read_buf");
		return 0;
	}

	if (f->path[0] == '/') {
		ERRX("security violation: absolute pathname: %s",
		    f->path);
		return 0;
	}

	if (strstr(f->path, "/../") != NULL ||
	    (len > 2 && strcmp(f->path + len - 3, "/..") == 0) ||
	    (len > 2 && strncmp(f->path, "../", 3) == 0) ||
	    strcmp(f->path, "..") == 0) {
		ERRX("%s: security violation: backtracking pathname",
		    f->path);
		return 0;
	}

	/* Record our last path and construct our filename. */

	strlcpy(last, f->path, PATH_MAX);
	f->wpath = f->path;
	return 1;
}

/*
 * Reallocate a file list in chunks of FLIST_CHUNK_SIZE;
 * Returns zero on failure, non-zero on success.
 */
static int
flist_realloc(struct flist **fl, size_t *sz, size_t *max)
{
	void	*pp;

	if (*sz + 1 <= *max)  {
		(*sz)++;
		return 1;
	}

	pp = recallocarray(*fl, *max,
		*max + FLIST_CHUNK_SIZE, sizeof(struct flist));
	if (pp == NULL) {
		ERR("recallocarray");
		return 0;
	}
	*fl = pp;
	*max += FLIST_CHUNK_SIZE;
	(*sz)++;
	return 1;
}

/*
 * Copy a regular or symbolic link file "path" into "f".
 * This handles the correct path creation and symbolic linking.
 * Returns zero on failure, non-zero on success.
 */
static int
flist_append(struct flist *f, struct stat *st, const char *path)
{

	/*
	 * Copy the full path for local addressing and transmit
	 * only the filename part for the receiver.
	 */

	if ((f->path = strdup(path)) == NULL) {
		ERR("strdup");
		return 0;
	}

	if ((f->wpath = strrchr(f->path, '/')) == NULL)
		f->wpath = f->path;
	else
		f->wpath++;

	/*
	 * On the receiving end, we'll strip out all bits on the
	 * mode except for the file permissions.
	 * No need to warn about it here.
	 */

	flist_copy_stat(f, st);

	/* Optionally copy link information. */

	if (S_ISLNK(st->st_mode)) {
		f->link = symlink_read(f->path);
		if (f->link == NULL) {
			ERRX1("symlink_read");
			return 0;
		}
	}

	return 1;
}

/*
 * Receive a file list from the wire, filling in length "sz" (which may
 * possibly be zero) and list "flp" on success.
 * Return zero on failure, non-zero on success.
 */
int
flist_recv(struct sess *sess, int fd, struct flist **flp, size_t *sz)
{
	struct flist	*fl = NULL;
	struct flist	*ff;
	const struct flist *fflast = NULL;
	size_t		 flsz = 0, flmax = 0, lsz, gidsz = 0, uidsz = 0;
	uint8_t		 flag;
	char		 last[PATH_MAX];
	int64_t		 lval; /* temporary values... */
	int32_t		 ival;
	uint32_t	 uival;
	struct ident	*gids = NULL, *uids = NULL;

	last[0] = '\0';

	for (;;) {
		if (!io_read_byte(sess, fd, &flag)) {
			ERRX1("io_read_byte");
			goto out;
		} else if (flag == 0)
			break;

		if (!flist_realloc(&fl, &flsz, &flmax)) {
			ERRX1("flist_realloc");
			goto out;
		}

		ff = &fl[flsz - 1];
		fflast = flsz > 1 ? &fl[flsz - 2] : NULL;

		/* Filename first. */

		if (!flist_recv_name(sess, fd, ff, flag, last)) {
			ERRX1("flist_recv_name");
			goto out;
		}

		/* Read the file size. */

		if (!io_read_long(sess, fd, &lval)) {
			ERRX1("io_read_long");
			goto out;
		}
		ff->st.size = lval;

		/* Read the modification time. */

		if (!(FLIST_TIME_SAME & flag)) {
			if (!io_read_uint(sess, fd, &uival)) {
				ERRX1("io_read_uint");
				goto out;
			}
			ff->st.mtime = uival;	/* beyond 2038 */
		} else if (fflast == NULL) {
			ERRX("same time without last entry");
			goto out;
		}  else
			ff->st.mtime = fflast->st.mtime;

		/* Read the file mode. */

		if (!(FLIST_MODE_SAME & flag)) {
			if (!io_read_uint(sess, fd, &uival)) {
				ERRX1("io_read_uint");
				goto out;
			}
			ff->st.mode = uival;
		} else if (fflast == NULL) {
			ERRX("same mode without last entry");
			goto out;
		} else
			ff->st.mode = fflast->st.mode;

		/* Conditional part: uid. */

		if (sess->opts->preserve_uids) {
			if (!(FLIST_UID_SAME & flag)) {
				if (!io_read_uint(sess, fd, &uival)) {
					ERRX1("io_read_int");
					goto out;
				}
				ff->st.uid = uival;
			} else if (fflast == NULL) {
				ERRX("same uid without last entry");
				goto out;
			} else
				ff->st.uid = fflast->st.uid;
		}

		/* Conditional part: gid. */

		if (sess->opts->preserve_gids) {
			if (!(FLIST_GID_SAME & flag)) {
				if (!io_read_uint(sess, fd, &uival)) {
					ERRX1("io_read_uint");
					goto out;
				}
				ff->st.gid = uival;
			} else if (fflast == NULL) {
				ERRX("same gid without last entry");
				goto out;
			} else
				ff->st.gid = fflast->st.gid;
		}

		/* Conditional part: devices & special files. */

		if ((sess->opts->devices && (S_ISBLK(ff->st.mode) ||
		    S_ISCHR(ff->st.mode))) ||
		    (sess->opts->specials && (S_ISFIFO(ff->st.mode) ||
		    S_ISSOCK(ff->st.mode)))) {
			if (!(FLIST_RDEV_SAME & flag)) {
				if (!io_read_int(sess, fd, &ival)) {
					ERRX1("io_read_int");
					goto out;
				}
				ff->st.rdev = ival;
			} else if (fflast == NULL) {
				ERRX("same device without last entry");
				goto out;
			} else
				ff->st.rdev = fflast->st.rdev;
		}

		/* Conditional part: link. */

		if (S_ISLNK(ff->st.mode) &&
		    sess->opts->preserve_links) {
			if (!io_read_size(sess, fd, &lsz)) {
				ERRX1("io_read_size");
				goto out;
			} else if (lsz == 0) {
				ERRX("empty link name");
				goto out;
			}
			ff->link = calloc(lsz + 1, 1);
			if (ff->link == NULL) {
				ERR("calloc");
				goto out;
			}
			if (!io_read_buf(sess, fd, ff->link, lsz)) {
				ERRX1("io_read_buf");
				goto out;
			}
		}

		LOG3("%s: received file metadata: "
			"size %jd, mtime %jd, mode %o, rdev (%d, %d)",
			ff->path, (intmax_t)ff->st.size,
			(intmax_t)ff->st.mtime, ff->st.mode,
			major(ff->st.rdev), minor(ff->st.rdev));

		if (S_ISREG(ff->st.mode))
			sess->total_size += ff->st.size;
	}

	/* Conditionally read the user/group list. */

	if (sess->opts->preserve_uids && !sess->opts->numeric_ids) {
		if (!idents_recv(sess, fd, &uids, &uidsz)) {
			ERRX1("idents_recv");
			goto out;
		}
		LOG2("received uid list: %zu", uidsz);
	}

	if (sess->opts->preserve_gids && !sess->opts->numeric_ids) {
		if (!idents_recv(sess, fd, &gids, &gidsz)) {
			ERRX1("idents_recv");
			goto out;
		}
		LOG2("received gid list: %zu", gidsz);
	}

	/* Remember to order the received list. */

	LOG2("received file metadata list: %zu", flsz);
	qsort(fl, flsz, sizeof(struct flist), flist_cmp);
	flist_topdirs(sess, fl, flsz);
	*sz = flsz;
	*flp = fl;

	/* Conditionally remap and reassign identifiers. */

	if (sess->opts->preserve_uids && !sess->opts->numeric_ids) {
		idents_remap(sess, 0, uids, uidsz);
		idents_assign_uid(sess, fl, flsz, uids, uidsz);
	}

	if (sess->opts->preserve_gids && !sess->opts->numeric_ids) {
		idents_remap(sess, 1, gids, gidsz);
		idents_assign_gid(sess, fl, flsz, gids, gidsz);
	}

	idents_free(gids, gidsz);
	idents_free(uids, uidsz);
	return 1;
out:
	flist_free(fl, flsz);
	idents_free(gids, gidsz);
	idents_free(uids, uidsz);
	*sz = 0;
	*flp = NULL;
	return 0;
}

/*
 * Generate a flist possibly-recursively given a file root, which may
 * also be a regular file or symlink.
 * On success, augments the generated list in "flp" of length "sz".
 * Returns zero on failure, non-zero on success.
 */
static int
flist_gen_dirent(struct sess *sess, char *root, struct flist **fl, size_t *sz,
    size_t *max)
{
	char		*cargv[2], *cp;
	int		 rc = 0, flag;
	FTS		*fts;
	FTSENT		*ent;
	struct flist	*f;
	size_t		 i, flsz = 0, nxdev = 0, stripdir;
	dev_t		*newxdev, *xdev = NULL;
	struct stat	 st;

	cargv[0] = root;
	cargv[1] = NULL;

	/*
	 * If we're a file, then revert to the same actions we use for
	 * the non-recursive scan.
	 */

	if (lstat(root, &st) == -1) {
		ERR("%s: lstat", root);
		return 0;
	} else if (S_ISREG(st.st_mode)) {
		/* filter files */
		if (rules_match(root, 0) == -1) {
			WARNX("%s: skipping excluded file", root);
			return 1;
		}
		if (!flist_realloc(fl, sz, max)) {
			ERRX1("flist_realloc");
			return 0;
		}
		f = &(*fl)[(*sz) - 1];
		assert(f != NULL);

		if (!flist_append(f, &st, root)) {
			ERRX1("flist_append");
			return 0;
		}
		return 1;
	} else if (S_ISLNK(st.st_mode)) {
		if (!sess->opts->preserve_links) {
			WARNX("%s: skipping symlink", root);
			return 1;
		}
		/* filter files */
		if (rules_match(root, 0) == -1) {
			WARNX("%s: skipping excluded symlink", root);
			return 1;
		}
		if (!flist_realloc(fl, sz, max)) {
			ERRX1("flist_realloc");
			return 0;
		}
		f = &(*fl)[(*sz) - 1];
		assert(f != NULL);

		if (!flist_append(f, &st, root)) {
			ERRX1("flist_append");
			return 0;
		}
		return 1;
	} else if (!S_ISDIR(st.st_mode)) {
		WARNX("%s: skipping special", root);
		return 1;
	}

	/*
	 * If we end with a slash, it means that we're not supposed to
	 * copy the directory part itself---only the contents.
	 * So set "stripdir" to be what we take out.
	 */

	stripdir = strlen(root);
	assert(stripdir > 0);
	if (root[stripdir - 1] != '/')
		stripdir = 0;

	/*
	 * If we're not stripping anything, then see if we need to strip
	 * out the leading material in the path up to and including the
	 * last directory component.
	 */

	if (stripdir == 0)
		if ((cp = strrchr(root, '/')) != NULL)
			stripdir = cp - root + 1;

	/*
	 * If we're recursive, then we need to take down all of the
	 * files and directory components, so use fts(3).
	 * Copying the information file-by-file into the flstat.
	 * We'll make sense of it in flist_send.
	 */

	if ((fts = fts_open(cargv, FTS_PHYSICAL, NULL)) == NULL) {
		ERR("fts_open");
		return 0;
	}

	errno = 0;
	while ((ent = fts_read(fts)) != NULL) {
		if (!flist_fts_check(sess, ent)) {
			errno = 0;
			continue;
		}

		/* We don't allow symlinks without -l. */

		assert(ent->fts_statp != NULL);
		if (S_ISLNK(ent->fts_statp->st_mode) &&
		    !sess->opts->preserve_links) {
			WARNX("%s: skipping symlink", ent->fts_path);
			continue;
		}

		/*
		 * If rsync is told to avoid crossing a filesystem
		 * boundary when recursing, then replace all mount point
		 * directories with empty directories.  The latter is
		 * prevented by telling rsync multiple times to avoid
		 * crossing a filesystem boundary when recursing.
		 * Replacing mount point directories is tricky. We need
		 * to sort out which directories to include.  As such,
		 * keep track of unique device inodes, and use these for
		 * comparison.
		 */

		if (sess->opts->one_file_system &&
		    ent->fts_statp->st_dev != st.st_dev) {
			if (sess->opts->one_file_system > 1 ||
			    !S_ISDIR(ent->fts_statp->st_mode))
				continue;

			flag = 0;
			for (i = 0; i < nxdev; i++)
				if (xdev[i] == ent->fts_statp->st_dev) {
					flag = 1;
					break;
				}
			if (flag)
				continue;

			if ((newxdev = reallocarray(xdev, nxdev + 1,
			    sizeof(dev_t))) == NULL) {
				ERRX1("reallocarray");
				goto out;
			}
			xdev = newxdev;
			xdev[nxdev] = ent->fts_statp->st_dev;
			nxdev++;
		}

		/* filter files */
		if (rules_match(ent->fts_path + stripdir,
		    (ent->fts_info == FTS_D)) == -1) {
			WARNX("%s: skipping excluded file",
			    ent->fts_path + stripdir);
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}

		/* Allocate a new file entry. */

		if (!flist_realloc(fl, sz, max)) {
			ERRX1("flist_realloc");
			goto out;
		}
		flsz++;
		f = &(*fl)[*sz - 1];

		/* Our path defaults to "." for the root. */

		if (ent->fts_path[stripdir] == '\0') {
			if (asprintf(&f->path, "%s.", ent->fts_path) == -1) {
				ERR("asprintf");
				f->path = NULL;
				goto out;
			}
		} else {
			if ((f->path = strdup(ent->fts_path)) == NULL) {
				ERR("strdup");
				goto out;
			}
		}

		f->wpath = f->path + stripdir;
		flist_copy_stat(f, ent->fts_statp);

		/* Optionally copy link information. */

		if (S_ISLNK(ent->fts_statp->st_mode)) {
			f->link = symlink_read(ent->fts_accpath);
			if (f->link == NULL) {
				ERRX1("symlink_read");
				goto out;
			}
		}

		/* Reset errno for next fts_read() call. */
		errno = 0;
	}
	if (errno) {
		ERR("fts_read");
		goto out;
	}

	LOG3("generated %zu filenames: %s", flsz, root);
	rc = 1;
out:
	fts_close(fts);
	free(xdev);
	return rc;
}

/*
 * Generate a flist recursively given the array of directories (or
 * files, symlinks, doesn't matter) specified in argv (argc >0).
 * On success, stores the generated list in "flp" with length "sz",
 * which may be zero.
 * Returns zero on failure, non-zero on success.
 */
static int
flist_gen_dirs(struct sess *sess, size_t argc, char **argv, struct flist **flp,
    size_t *sz)
{
	size_t		 i, max = 0;

	for (i = 0; i < argc; i++)
		if (!flist_gen_dirent(sess, argv[i], flp, sz, &max))
			break;

	if (i == argc) {
		LOG2("recursively generated %zu filenames", *sz);
		return 1;
	}

	ERRX1("flist_gen_dirent");
	flist_free(*flp, max);
	*flp = NULL;
	*sz = 0;
	return 0;
}

/*
 * Generate list of files from the command-line argc (>0) and argv.
 * On success, stores the generated list in "flp" with length "sz",
 * which may be zero.
 * Returns zero on failure, non-zero on success.
 */
static int
flist_gen_files(struct sess *sess, size_t argc, char **argv,
    struct flist **flp, size_t *sz)
{
	struct flist	*fl = NULL, *f;
	size_t		 i, flsz = 0;
	struct stat	 st;

	assert(argc);

	if ((fl = calloc(argc, sizeof(struct flist))) == NULL) {
		ERR("calloc");
		return 0;
	}

	for (i = 0; i < argc; i++) {
		if (argv[i][0] == '\0')
			continue;
		if (lstat(argv[i], &st) == -1) {
			ERR("%s: lstat", argv[i]);
			goto out;
		}

		/*
		 * File type checks.
		 * In non-recursive mode, we don't accept directories.
		 * We also skip symbolic links without -l.
		 * Beyond that, we only accept regular files.
		 */

		if (S_ISDIR(st.st_mode)) {
			WARNX("%s: skipping directory", argv[i]);
			continue;
		} else if (S_ISLNK(st.st_mode)) {
			if (!sess->opts->preserve_links) {
				WARNX("%s: skipping symlink", argv[i]);
				continue;
			}
		} else if (!S_ISREG(st.st_mode)) {
			WARNX("%s: skipping special", argv[i]);
			continue;
		}

		/* filter files */
		if (rules_match(argv[i], S_ISDIR(st.st_mode)) == -1) {
			WARNX("%s: skipping excluded file", argv[i]);
			continue;
		}

		f = &fl[flsz++];
		assert(f != NULL);

		/* Add this file to our file-system worldview. */

		if (!flist_append(f, &st, argv[i])) {
			ERRX1("flist_append");
			goto out;
		}
	}

	LOG2("non-recursively generated %zu filenames", flsz);
	*sz = flsz;
	*flp = fl;
	return 1;
out:
	flist_free(fl, argc);
	*sz = 0;
	*flp = NULL;
	return 0;
}

/*
 * Generate a sorted, de-duplicated list of file metadata.
 * In non-recursive mode (the default), we use only the files we're
 * given.
 * Otherwise, directories are recursively examined.
 * Returns zero on failure, non-zero on success.
 * On success, "fl" will need to be freed with flist_free().
 */
int
flist_gen(struct sess *sess, size_t argc, char **argv, struct flist **flp,
    size_t *sz)
{
	int	 rc;

	assert(argc > 0);
	rc = sess->opts->recursive ?
		flist_gen_dirs(sess, argc, argv, flp, sz) :
		flist_gen_files(sess, argc, argv, flp, sz);

	/* After scanning, lock our file-system view. */

	if (!rc)
		return 0;

	qsort(*flp, *sz, sizeof(struct flist), flist_cmp);

	if (flist_dedupe(flp, sz)) {
		flist_topdirs(sess, *flp, *sz);
		return 1;
	}

	ERRX1("flist_dedupe");
	flist_free(*flp, *sz);
	*flp = NULL;
	*sz = 0;
	return 0;
}

/*
 * Generate a list of files in root to delete that are within the
 * top-level directories stipulated by "wfl".
 * Only handles symbolic links, directories, and regular files.
 * Returns zero on failure (fl and flsz will be NULL and zero), non-zero
 * on success.
 * On success, "fl" will need to be freed with flist_free().
 */
int
flist_gen_dels(struct sess *sess, const char *root, struct flist **fl,
    size_t *sz,	const struct flist *wfl, size_t wflsz)
{
	char		**cargv = NULL;
	int		  rc = 0, c, flag;
	FTS		 *fts = NULL;
	FTSENT		 *ent;
	struct flist	 *f;
	struct stat	  st;
	size_t		  cargvs = 0, i, j, max = 0, stripdir;
	ENTRY		  hent;
	ENTRY		 *hentp;

	*fl = NULL;
	*sz = 0;

	/* Only run this code when we're recursive. */

	if (!sess->opts->recursive)
		return 1;

	/*
	 * Gather up all top-level directories for scanning.
	 * This is stipulated by rsync's --delete behaviour, where we
	 * only delete things in the top-level directories given on the
	 * command line.
	 */

	assert(wflsz > 0);
	for (i = 0; i < wflsz; i++)
		if (FLSTAT_TOP_DIR & wfl[i].st.flags)
			cargvs++;
	if (cargvs == 0)
		return 1;

	if ((cargv = calloc(cargvs + 1, sizeof(char *))) == NULL) {
		ERR("calloc");
		return 0;
	}

	/*
	 * If we're given just a "." as the first entry, that means
	 * we're doing a relative copy with a trailing slash.
	 * Special-case this just for the sake of simplicity.
	 * Otherwise, look through all top-levels.
	 */

	if (wflsz && strcmp(wfl[0].wpath, ".") == 0) {
		assert(cargvs == 1);
		assert(S_ISDIR(wfl[0].st.mode));
		if (asprintf(&cargv[0], "%s/", root) == -1) {
			ERR("asprintf");
			cargv[0] = NULL;
			goto out;
		}
		cargv[1] = NULL;
	} else {
		for (i = j = 0; i < wflsz; i++) {
			if (!(FLSTAT_TOP_DIR & wfl[i].st.flags))
				continue;
			assert(S_ISDIR(wfl[i].st.mode));
			assert(strcmp(wfl[i].wpath, "."));
			c = asprintf(&cargv[j], "%s/%s", root, wfl[i].wpath);
			if (c == -1) {
				ERR("asprintf");
				cargv[j] = NULL;
				goto out;
			}
			LOG4("%s: will scan for deletions", cargv[j]);
			j++;
		}
		assert(j == cargvs);
		cargv[j] = NULL;
	}

	LOG2("delete from %zu directories", cargvs);

	/*
	 * Next, use the standard hcreate(3) hashtable interface to hash
	 * all of the files that we want to synchronise.
	 * This way, we'll be able to determine which files we want to
	 * delete in O(n) time instead of O(n * search) time.
	 * Plus, we can do the scan in-band and only allocate the files
	 * we want to delete.
	 */

	if (!hcreate(wflsz)) {
		ERR("hcreate");
		goto out;
	}

	for (i = 0; i < wflsz; i++) {
		memset(&hent, 0, sizeof(ENTRY));
		if ((hent.key = strdup(wfl[i].wpath)) == NULL) {
			ERR("strdup");
			goto out;
		}
		if ((hentp = hsearch(hent, ENTER)) == NULL) {
			ERR("hsearch");
			goto out;
		} else if (hentp->key != hent.key) {
			ERRX("%s: duplicate", wfl[i].wpath);
			free(hent.key);
			goto out;
		}
	}

	/*
	 * Now we're going to try to descend into all of the top-level
	 * directories stipulated by the file list.
	 * If the directories don't exist, it's ok.
	 */

	if ((fts = fts_open(cargv, FTS_PHYSICAL, NULL)) == NULL) {
		ERR("fts_open");
		goto out;
	}

	stripdir = strlen(root) + 1;
	errno = 0;
	while ((ent = fts_read(fts)) != NULL) {
		if (ent->fts_info == FTS_NS)
			continue;
		if (!flist_fts_check(sess, ent)) {
			errno = 0;
			continue;
		} else if (stripdir >= ent->fts_pathlen)
			continue;

		assert(ent->fts_statp != NULL);

		/*
		 * If rsync is told to avoid crossing a filesystem
		 * boundary when recursing, then exclude all entries
		 * from the list with a device inode, which does not
		 * match that of one of the top-level directories.
		 */

		if (sess->opts->one_file_system) {
			flag = 0;
			for (i = 0; i < wflsz; i++) {
				if (stat(wfl[i].path, &st) == -1) {
					ERR("%s: stat", wfl[i].path);
					goto out;
				}
				if (ent->fts_statp->st_dev == st.st_dev) {
					flag = 1;
					break;
				}
			}
			if (!flag)
				continue;
		}

		/* filter files on delete */
		/* TODO handle --delete-excluded */
		if (rules_match(ent->fts_path + stripdir,
		    (ent->fts_info == FTS_D)) == -1) {
			WARNX("skip excluded file %s",
			    ent->fts_path + stripdir);
			fts_set(fts, ent, FTS_SKIP);
			continue;
		}

		/* Look up in hashtable. */

		memset(&hent, 0, sizeof(ENTRY));
		hent.key = ent->fts_path + stripdir;
		if (hsearch(hent, FIND) != NULL)
			continue;

		/* Not found: we'll delete it. */

		if (!flist_realloc(fl, sz, &max)) {
			ERRX1("flist_realloc");
			goto out;
		}
		f = &(*fl)[*sz - 1];

		if ((f->path = strdup(ent->fts_path)) == NULL) {
			ERR("strdup");
			goto out;
		}
		f->wpath = f->path + stripdir;
		flist_copy_stat(f, ent->fts_statp);
		errno = 0;
	}

	if (errno) {
		ERR("fts_read");
		goto out;
	}

	qsort(*fl, *sz, sizeof(struct flist), flist_cmp);
	rc = 1;
out:
	if (fts != NULL)
		fts_close(fts);
	for (i = 0; i < cargvs; i++)
		free(cargv[i]);
	free(cargv);
	hdestroy();
	return rc;
}

/*
 * Delete all files and directories in "fl".
 * If called with a zero-length "fl", does nothing.
 * If dry_run is specified, simply write what would be done.
 * Return zero on failure, non-zero on success.
 */
int
flist_del(struct sess *sess, int root, const struct flist *fl, size_t flsz)
{
	ssize_t	 i;
	int	 flag;

	if (flsz == 0)
		return 1;

	assert(sess->opts->del);
	assert(sess->opts->recursive);

	for (i = flsz - 1; i >= 0; i--) {
		LOG1("%s: deleting", fl[i].wpath);
		if (sess->opts->dry_run)
			continue;
		assert(root != -1);
		flag = S_ISDIR(fl[i].st.mode) ? AT_REMOVEDIR : 0;
		if (unlinkat(root, fl[i].wpath, flag) == -1 &&
		    errno != ENOENT) {
			ERR("%s: unlinkat", fl[i].wpath);
			return 0;
		}
	}

	return 1;
}

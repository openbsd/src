/* $OpenBSD: fuse_ops.c,v 1.46 2026/08/04 15:20:28 helg Exp $ */
/*
 * Copyright (c) 2013 Sylvestre Gallon <ccna.syl@gmail.com>
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

#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

#include "fuse_private.h"
#include "debug.h"

#define CHECK_OPT(opname)	if (!f->op.opname) {			\
					err = -ENOSYS;			\
					goto out;			\
				}

static int ifuse_fill_readdir(void *, const char *, const struct stat *, off_t);

/*
 * Store the current request so that it is available on demand for
 * fuse_get_context(3).
 */
static fuse_req_t ireq;

fuse_req_t
ifuse_req(void)
{
	return (ireq);
}

static void
ictx_init(fuse_req_t req)
{
	ireq = req;
}

static void
ictx_destroy(void)
{
	ireq = NULL;
}

static int
update_attr(struct fuse *f, struct stat *attr, const char *realname,
    const fuse_ino_t ino)
{
	int err;

	memset(attr, 0, sizeof(struct stat));
	err = f->op.getattr(realname, attr);
	if (err)
		return (err);

	if (!f->conf.use_ino)
		attr->st_ino = ino;

	if (f->conf.set_mode)
		attr->st_mode = (attr->st_mode & S_IFMT) | (0777 & ~f->conf.umask);

	if (f->conf.set_uid)
		attr->st_uid = f->conf.uid;

	if (f->conf.set_gid)
		attr->st_gid = f->conf.gid;

	return (0);
}

static void
ifuse_ops_init(void *userdata, struct fuse_conn_info *fci)
{
	struct fuse *f = (struct fuse *)userdata;

	if (f->op.init) {
		f->private_data = f->op.init(fci);
		fuse_get_context()->private_data = f->private_data;
	}
}

static void
ifuse_ops_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *ffi)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	struct stat stbuf;
	char *realname;
	int err;

	realname = build_realname(f, ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	ictx_init(req);
	err = update_attr(f, &stbuf, realname, ino);
	ictx_destroy();
	free(realname);

out:
	if (!err)
		fuse_reply_attr(req, &stbuf, 0.0);
	else
		fuse_reply_err(req, -err);
}

static void
ifuse_ops_access(fuse_req_t req, fuse_ino_t ino, int mask)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	char *realname;
	int err;

	CHECK_OPT(access);

	realname = build_realname(f, ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	ictx_init(req);
	err = f->op.access(realname, mask);
	ictx_destroy();
	free(realname);

out:
	fuse_reply_err(req, -err);
}

static void
ifuse_ops_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *ffi)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	char *realname;
	int err = 0;

	/* open is optional */
	if (f->op.open) {
		realname = build_realname(f, ino);
		if (realname == NULL) {
			err = -errno;
			goto out;
		}

		ictx_init(req);
		err = f->op.open(realname, ffi);
		ictx_destroy();
		free(realname);
	}

out:
	if (!err)
		fuse_reply_open(req, ffi);
	else
		fuse_reply_err(req, -err);
}

static void
ifuse_ops_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *ffi)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	struct fuse_dirhandle *fd;
	char *realname;
	int err = 0;

	/* opendir is optional */
	if (f->op.opendir) {
		realname = build_realname(f, ino);
		if (realname == NULL) {
			err = -errno;
			goto out;
		}

		ictx_init(req);
		err = f->op.opendir(realname, ffi);
		ictx_destroy();
		free(realname);

		if (err)
			goto out;
	}

	/* Create a dirhandle to maintain state. */
	fd = calloc(1, sizeof(*fd));
	if (fd == NULL) {
		err = -errno;
		goto out;
	}

	fd->filler = ifuse_fill_readdir;
	fd->fuse = f;
	fd->ino = ino;
	fd->fh = ffi->fh;

	/*
	 * use the address of the dirhandle as the file handle so we can
	 * retrieve it in readdir and releasedir
	 */
	ffi->fh = (uint64_t)(uintptr_t)fd;

out:
	if (!err)
		fuse_reply_open(req, ffi);
	else
		fuse_reply_err(req, -err);
}

/*
 * This function adds one directory entry to the buffer.
 * FUSE file systems can implement readdir in one of two ways.
 *
 * 1. Read all directory entries in one operation. The off parameter
 *    will always be 0 and this filler function always returns 0.
 * 2. The file system keeps track of the directory entry offsets and
 *    this filler function returns 1 when the buffer is full.
 */
static int
ifuse_fill_readdir(void *dh, const char *name, const struct stat *stbuf,
    off_t off)
{
	struct fuse *f;
	struct fuse_dirhandle *fd = dh;
	struct fuse_vnode *v;
	struct stat attr;
	char *buf;
	uint32_t len, resid;
	size_t new_size;
	char *new_buf;

	/* may have run out of memory resizing buffer */
	if (fd->err)
		return (1);

	f = fd->fuse;

	/* calculate the size needed for the entry */
	len = fuse_add_direntry(NULL, NULL, 0, name, stbuf, off);

	/*
	 * buffer is full
	 *   mode 1: grow the buffer
	 *   mode 2: stop
	 */
	if (fd->len + len > fd->size) {
		if (off > 0)
			return (1);

		new_size = fd->size * 2;
		new_buf = realloc(fd->buf, new_size);
		if (new_buf == NULL) {
			fd->err = errno;
			return (1);
		}
		fd->buf = new_buf;
		fd->size = new_size;
	}

	/* set the inode number for the entry */
	if (stbuf != NULL && f->conf.use_ino)
		attr.st_ino = stbuf->st_ino;
	else {
		/*
		 * This always behaves as if readdir_ino option is set so
		 * getcwd(3) works.
		 */
		v = get_vn_by_name_and_parent(f, name, fd->ino);
		if (v == NULL) {
			if (strcmp(name, ".") == 0)
				attr.st_ino = fd->ino;
			else
				attr.st_ino = 0xffffffff;
		} else
			attr.st_ino = v->ino;
	}

	/* set the file type for the entry */
	if (stbuf != NULL)
		attr.st_mode = stbuf->st_mode;
	else
		attr.st_mode = S_IFREG;

	/* advance buf to start of next entry and now add it for real */
	buf = (char *) fd->buf + fd->len;
	resid = fd->size - fd->len;
	fd->len += len;

	if (off)
		fuse_add_direntry(ifuse_req(), buf, resid, name, &attr, off);
	else {
		fuse_add_direntry(ifuse_req(), buf, resid, name, &attr,
		    fd->len);

		/*
		 * There is no way to determine whether this is the last entry
		 * but unless something goes wrong, the buffer will have all
		 * entries at the end, so just flag it each time.
		 */
		fd->full = 1;
	}

	return (0);
}

static int
ifuse_fill_getdir(fuse_dirh_t fd, const char *name, int type, ino_t ino)
{
	struct stat st;

	memset(&st, 0, sizeof(st));
	st.st_mode = type << 12;
	if (ino == 0)
		st.st_ino = 0xffffffff;
	else
		st.st_ino = ino;

	return (fd->filler(fd, name, &st, 0));
}

static void
ifuse_ops_readdir(struct fuse_req *req, fuse_ino_t ino, size_t size,
    off_t offset, struct fuse_file_info *ffi)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	struct fuse_dirhandle *fd;
	struct fuse_dirent *dent;
	char *realname;
	off_t next;
	int err;

	/*
	 * We stored a pointer to our own struct fuse_dirhandle in
	 * ifuse_ops_opendir so we can retrieve it here.
	 */
	fd = (struct fuse_dirhandle *)(uintptr_t)ffi->fh;

	if (fd->ino != ino) {
		DPRINTF("%s: invalid ino %llu != %llu\n", __func__, ino,
		    fd->ino);
		err = -EBADF;
		goto out;
	}

	/*
	 * Refresh cache on rewinddir(3).
	 */
	if (offset == 0)
		fd->full = 0;

	/*
	 * Allocate buf here rather than opendir since the size is only known
	 * here. The size is not important in mode 1 since the buffer can grow
	 * but in mode 2 it doesn't.
	 */
	if (fd->buf == NULL) {
		fd->buf = calloc(1, size);
		if (fd->buf == NULL) {
			err = -errno;
			goto out;
		}

		fd->size = size;
	}

	/*
	 * If the file system has already filled our buffer, return the
	 * cached entries.
	 */
	if (fd->full) {
		if (offset >= 0 && (uint64_t)offset > (uint64_t)fd->len) {
			err = -EINVAL;
			goto out;
		}

		/* seek along dirents to confirm offset is valid */
		dent = (struct fuse_dirent *)fd->buf;
		for (next = 0; next < offset;) {
			next += FUSE_DIRENT_SIZE(dent);
			dent = (struct fuse_dirent *)((char *)fd->buf + next);
		}

		if (next != offset) {
			err = -EINVAL;
			goto out;
		}

		if (fd->len - offset < size)
			size = fd->len - offset;

		fuse_reply_buf(req, fd->buf + offset, size);
		return;
	}

	/* always fill from the start of the buffer */
	fd->len = 0;

	realname = build_realname(f, ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	ictx_init(req);
	if (f->op.readdir) {
		ffi->fh = fd->fh;
		err = f->op.readdir(realname, fd, ifuse_fill_readdir,
		    offset, ffi);
		ffi->fh = (uint64_t)(uintptr_t)fd;
	} else if (f->op.getdir)
		err = f->op.getdir(realname, fd, ifuse_fill_getdir);
	else
		err = -ENOSYS;
	ictx_destroy();
	free(realname);

out:
	if (!err) {
		/*
		 * File system may not have returned an error but check there
		 * were no errors in our filler function.
		 */
		if (fd->err)
			fuse_reply_err(req, fd->err);
		else
			fuse_reply_buf(req, fd->buf, fd->len > size ? size :
			    fd->len);
	} else
		fuse_reply_err(req, -err);
}

static void
ifuse_ops_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *ffi)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	struct fuse_dirhandle *fd;
	char *realname;
	int err = 0;

	/*
	 * We stored a pointer to our own struct fuse_dirhandle in
	 * ifuse_ops_opendir so we can retrieve it here.
	 */
	fd = (struct fuse_dirhandle *)(uintptr_t)ffi->fh;

	/* releasedir is optional */
	if (f->op.releasedir) {
		realname = build_realname(f, ino);
		if (realname == NULL) {
			err = -errno;
			goto out;
		}

		ictx_init(req);
		ffi->fh = fd->fh;
		err = f->op.releasedir(realname, ffi);
		ictx_destroy();
		free(realname);
	}

out:
	free(fd->buf);
	free(fd);
	fuse_reply_err(req, -err);
}

static void
ifuse_ops_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *ffi)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	char *realname;
	int err = 0;

	/* release is optional */
	if (f->op.release) {
		realname = build_realname(f, ino);
		if (realname == NULL) {
			err = -errno;
			goto out;
		}

		ictx_init(req);
		err = f->op.release(realname, ffi);
		ictx_destroy();
		free(realname);
	}

out:
	fuse_reply_err(req, -err);
}

static void
ifuse_ops_fsync(fuse_req_t req, fuse_ino_t ino, int datasync,
    struct fuse_file_info *ffi)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	char *realname;
	int err;

	CHECK_OPT(fsync);

	realname = build_realname(f, ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	ictx_init(req);
	err = f->op.fsync(realname, datasync, ffi);
	ictx_destroy();
	free(realname);

out:
	fuse_reply_err(req, -err);
}

static void
ifuse_ops_flush(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *ffi)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	char *realname;
	int err;

	CHECK_OPT(flush);

	realname = build_realname(f, ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	ictx_init(req);
	err = f->op.flush(realname, ffi);
	ictx_destroy();
	free(realname);

out:
	fuse_reply_err(req, -err);
}

static void
ifuse_ops_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	struct fuse_entry_param entry;
	struct fuse_vnode *vn;
	char *realname;
	int err;

	vn = get_vn_by_name_and_parent(f, name, parent);
	if (vn == NULL) {
		vn = alloc_vn(f, name, -1, parent);
		if (vn == NULL) {
			err = -errno;
			goto out;
		}
		set_vn(f, vn); /*XXX*/
	} else if (vn->ino != FUSE_ROOT_INO)
		ref_vn(vn);

	realname = build_realname(f, vn->ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	memset(&entry, 0, sizeof(entry));
	entry.ino = vn->ino;
	err = update_attr(f, &entry.attr, realname, vn->ino);
	free(realname);

out:
	if (!err)
		fuse_reply_entry(req, &entry);
	else
		fuse_reply_err(req, -err);
}

static void
ifuse_ops_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset,
    struct fuse_file_info *ffi)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	char *buf = NULL;
	char *realname = NULL;
	int err;

	CHECK_OPT(read);

	realname = build_realname(f, ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	buf = calloc(1, size);
	if (buf == NULL) {
		err = -errno;
		goto out;
	}

	ictx_init(req);
	err = f->op.read(realname, buf, size, offset, ffi);
	ictx_destroy();

out:
	if (err >= 0)
		fuse_reply_buf(req, buf, err);
	else
		fuse_reply_err(req, -err);

	free(realname);
	free(buf);
}

static void
ifuse_ops_write(fuse_req_t req, fuse_ino_t ino, const char *buf,
    size_t size, off_t offset, struct fuse_file_info *ffi)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	char *realname;
	int err;

	CHECK_OPT(write);

	realname = build_realname(f, ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	ictx_init(req);
	err = f->op.write(realname, buf, size, offset, ffi);
	ictx_destroy();
	free(realname);

out:
	if (err >= 0)
		fuse_reply_write(req, err);
	else
		fuse_reply_err(req, -err);
}

static void
ifuse_ops_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name,
    mode_t mode)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	struct fuse_entry_param entry;
	struct fuse_vnode *newvn;
	char *realname;
	int err;

	CHECK_OPT(mkdir);

	newvn = get_vn_by_name_and_parent(f, name, parent);
	if (newvn == NULL) {
		err = -errno;
		goto out;
	}

	realname = build_realname(f, newvn->ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	ictx_init(req);
	err = f->op.mkdir(realname, mode);
	ictx_destroy();

	if (!err)
		err = update_attr(f, &entry.attr, realname, newvn->ino);

	free(realname);

out:
	if (!err) {
		entry.ino = newvn->ino;
		fuse_reply_entry(req, &entry);
	} else
		fuse_reply_err(req, -err);
}

static void
ifuse_ops_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	struct fuse_vnode *vn;
	char *realname;
	int err;

	CHECK_OPT(rmdir);

	vn = get_vn_by_name_and_parent(f, name, parent);
	if (vn == NULL) {
		err = -errno;
		goto out;
	}

	realname = build_realname(f, vn->ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	ictx_init(req);
	err = f->op.rmdir(realname);
	ictx_destroy();
	free(realname);

out:
	fuse_reply_err(req, -err);
}

static void
ifuse_ops_readlink(fuse_req_t req, fuse_ino_t ino)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	char path[PATH_MAX];
	char *realname;
	int err;

	CHECK_OPT(readlink);

	realname = build_realname(f, ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	ictx_init(req);
	err = f->op.readlink(realname, path, sizeof(path));
	ictx_destroy();
	free(realname);

out:
	if (!err)
		fuse_reply_readlink(req, path);
	else
		fuse_reply_err(req, -err);
}

static void
ifuse_ops_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	struct fuse_vnode *vn;
	char *realname;
	int err;

	CHECK_OPT(unlink);

	vn = get_vn_by_name_and_parent(f, name, parent);
	if (vn == NULL) {
		err = -errno;
		goto out;
	}

	realname = build_realname(f, vn->ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	ictx_init(req);
	err = f->op.unlink(realname);
	ictx_destroy();
	free(realname);

out:
	fuse_reply_err(req, -err);
}

static void
ifuse_ops_statfs(fuse_req_t req, fuse_ino_t ino)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	struct statvfs stbuf;
	char *realname;
	int err;

	CHECK_OPT(statfs);

	realname = build_realname(f, ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	memset(&stbuf, 0, sizeof(stbuf));
	ictx_init(req);
	err = f->op.statfs(realname, &stbuf);
	ictx_destroy();
	free(realname);

out:
	if (!err)
		fuse_reply_statfs(req, &stbuf);
	else
		fuse_reply_err(req, -err);
}

static void
ifuse_ops_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t newparent,
    const char *newname)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	struct fuse_entry_param entry;
	struct fuse_vnode *newvn;
	char *realname = NULL;
	char *target = NULL;
	int err;

	CHECK_OPT(link);

	newvn = get_vn_by_name_and_parent(f, newname, newparent);
	if (newvn == NULL) {
		err = -errno;
		goto out;
	}

	target = build_realname(f, ino);
	if (target == NULL) {
		err = -errno;
		goto out;
	}

	realname = build_realname(f, newvn->ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	ictx_init(req);
	err = f->op.link(target, realname);
	ictx_destroy();

	if (!err)
		err = update_attr(f, &entry.attr, realname, newvn->ino);

out:
	free(realname);
	free(target);

	if (!err) {
		entry.ino = newvn->ino;
		fuse_reply_entry(req, &entry);
	} else
		fuse_reply_err(req, -err);
}

static void
ifuse_ops_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
    int flags, struct fuse_file_info *ffi)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	struct stat stbuf;
	struct timespec ts[2];
	struct utimbuf tbuf;
	char *realname;
	uid_t uid;
	gid_t gid;
	int err = 0;

	realname = build_realname(f, ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	ictx_init(req);
	if (flags & FUSE_SET_ATTR_MODE) {
		if (f->op.chmod)
			err = f->op.chmod(realname, attr->st_mode);
		else
			err = -ENOSYS;
	}

	if (!err && (flags & FUSE_SET_ATTR_UID || flags & FUSE_SET_ATTR_GID)) {
		uid = (flags & FUSE_SET_ATTR_UID) ? attr->st_uid : (uid_t)-1;
		gid = (flags & FUSE_SET_ATTR_GID) ? attr->st_gid : (gid_t)-1;
		if (f->op.chown)
			err = f->op.chown(realname, uid, gid);
		else
			err = -ENOSYS;
	}

	if (!err && (flags & FUSE_SET_ATTR_MTIME || flags & FUSE_SET_ATTR_ATIME)) {
		if (f->op.utimens) {
			ts[0] = attr->st_atim;
			ts[1] = attr->st_mtim;
			err = f->op.utimens(realname, ts);
		} else if (f->op.utime) {
			tbuf.actime = attr->st_atim.tv_sec;
			tbuf.modtime = attr->st_mtim.tv_sec;
			err = f->op.utime(realname, &tbuf);
		} else
			err = -ENOSYS;
	}

	if (!err && (flags & FUSE_SET_ATTR_SIZE)) {
		if (f->op.truncate)
			err = f->op.truncate(realname, attr->st_size);
		else
			err = -ENOSYS;
	}

	ictx_destroy();

	if (!err)
		err = update_attr(f, &stbuf, realname, ino);

	free(realname);

out:
	if (!err)
		fuse_reply_attr(req, &stbuf, 0.0);
	else
		fuse_reply_err(req, -err);
}

static void
ifuse_ops_symlink(fuse_req_t req, const char *link, fuse_ino_t parent,
    const char *name)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	struct fuse_entry_param entry;
	struct fuse_vnode *newvn;
	char *realname;
	int err;

	CHECK_OPT(symlink);

	newvn = get_vn_by_name_and_parent(f, name, parent);
	if (newvn == NULL) {
		err = -errno;
		goto out;
	}

	realname = build_realname(f, newvn->ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	ictx_init(req);
	err = f->op.symlink(link, realname);
	ictx_destroy();

	if (!err)
		err = update_attr(f, &entry.attr, realname, newvn->ino);

	free(realname);

out:
	if (!err) {
		entry.ino = newvn->ino;
		fuse_reply_entry(req, &entry);
	} else
		fuse_reply_err(req, -err);
}

static void
ifuse_ops_rename(fuse_req_t req, fuse_ino_t parent, const char *name,
    fuse_ino_t newparent, const char *newname)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	struct fuse_vnode *vn;
	struct fuse_vnode *newvn;
	char *realname = NULL;
	char *newrealname = NULL;
	int err;

	CHECK_OPT(rename);

	vn = get_vn_by_name_and_parent(f, name, parent);
	if (vn == NULL) {
		err = -errno;
		goto out;
	}

	newvn = get_vn_by_name_and_parent(f, newname, newparent);
	if (newvn == NULL) {
		err = -errno;
		goto out;
	}

	realname = build_realname(f, vn->ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	newrealname = build_realname(f, newvn->ino);
	if (newrealname == NULL) {
		err = -errno;
		goto out;
	}

	ictx_init(req);
	err = f->op.rename(realname, newrealname);
	ictx_destroy();

out:
	free(realname);
	free(newrealname);

	fuse_reply_err(req, -err);
}

static void
ifuse_ops_destroy(void *userdata)
{
	struct fuse *f = (struct fuse *)userdata;

	if (f->op.destroy)
		f->op.destroy(f->private_data);
}

static void
ifuse_ops_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	struct fuse_vnode *vn;

	vn = tree_get(&f->vnode_tree, ino);
	if (vn != NULL)
		unref_vn(f, vn, nlookup);

	fuse_reply_none(req);
}

static void
ifuse_ops_mknod(fuse_req_t req, fuse_ino_t parent, const char *name,
    mode_t mode, dev_t dev)
{
	struct fuse *f = (struct fuse *)fuse_req_userdata(req);
	struct fuse_entry_param entry;
	struct fuse_vnode *newvn;
	char *realname;
	int err;

	CHECK_OPT(mknod);

	newvn = get_vn_by_name_and_parent(f, name, parent);
	if (newvn == NULL) {
		err = -errno;
		goto out;
	}

	realname = build_realname(f, newvn->ino);
	if (realname == NULL) {
		err = -errno;
		goto out;
	}

	ictx_init(req);
	err = f->op.mknod(realname, mode, dev);
	ictx_destroy();

	if (!err)
		err = update_attr(f, &entry.attr, realname, newvn->ino);

	free(realname);

out:
	if (!err) {
		entry.ino = newvn->ino;
		fuse_reply_entry(req, &entry);
	} else
		fuse_reply_err(req, -err);
}

struct fuse_lowlevel_ops llops = {
	.init = ifuse_ops_init,
	.destroy = ifuse_ops_destroy,
	.access = ifuse_ops_access,
	.flush = ifuse_ops_flush,
	.forget = ifuse_ops_forget,
	.fsync = ifuse_ops_fsync,
	.getattr = ifuse_ops_getattr,
	.link = ifuse_ops_link,
	.lookup = ifuse_ops_lookup,
	.mkdir = ifuse_ops_mkdir,
	.mknod = ifuse_ops_mknod,
	.open = ifuse_ops_open,
	.opendir = ifuse_ops_opendir,
	.read = ifuse_ops_read,
	.readdir = ifuse_ops_readdir,
	.readlink = ifuse_ops_readlink,
	.release = ifuse_ops_release,
	.releasedir = ifuse_ops_releasedir,
	.rename = ifuse_ops_rename,
	.rmdir = ifuse_ops_rmdir,
	.setattr = ifuse_ops_setattr,
	.statfs = ifuse_ops_statfs,
	.symlink = ifuse_ops_symlink,
	.unlink = ifuse_ops_unlink,
	.write = ifuse_ops_write,
};

/* $OpenBSD: fuse_ops.c,v 1.9 2013/10/07 18:16:43 syl Exp $ */
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
#include <stdlib.h>

#include "fuse_private.h"
#include "debug.h"

#define CHECK_OPT(opname)	DPRINTF("Opcode:\t%s\n", #opname);	\
				DPRINTF("Inode:\t%llu\n",		\
				    (unsigned long long)fbuf->fb_ino);	\
				if (!f->op.opname) {			\
					fbuf->fb_err = -ENOSYS;		\
					fbuf->fb_len = 0;		\
					return (0);			\
				}

static void
stat2attr(struct vattr *v, struct stat *st)
{
	v->va_fileid = st->st_ino;
	v->va_bytes = st->st_blocks;
	v->va_mode = st->st_mode;
	v->va_nlink = st->st_nlink;
	v->va_uid = st->st_uid;
	v->va_gid = st->st_gid;
	v->va_rdev = st->st_rdev;
	v->va_size = st->st_size;
	v->va_blocksize = st->st_blksize;
	v->va_atime.tv_sec = st->st_atime;
	v->va_atime.tv_nsec = st->st_atimensec;
	v->va_mtime.tv_sec = st->st_mtime;
	v->va_mtime.tv_nsec = st->st_mtimensec;
	v->va_ctime.tv_sec = st->st_ctime;
	v->va_ctime.tv_nsec = st->st_ctimensec;
}

static int
update_vattr(struct fuse *f, struct vattr *attr, const char *realname,
    struct fuse_vnode *vn)
{
	struct stat st;
	int ret;

	bzero(&st, sizeof(st));
	ret = f->op.getattr(realname, &st);

	if (st.st_blksize == 0)
		st.st_blksize = 512;
	if (st.st_blocks == 0)
		st.st_blocks = 4;
	if (st.st_size == 0)
		st.st_size = 512;

	st.st_ino = vn->ino;

	if (f->conf.set_mode)
		st.st_mode = (st.st_mode & S_IFMT) | (0777 & ~f->conf.umask);

	if (f->conf.set_uid)
		st.st_uid = f->conf.uid;

	if (f->conf.set_gid)
		st.st_gid = f->conf.gid;

	stat2attr(attr, &st);

	return (ret);
}

static int
ifuse_ops_init(struct fusebuf *fbuf)
{
	DPRINTF("Opcode:\tinit\n");
	fbuf->fb_len = 0;

	return (0);
}

static int
ifuse_ops_getattr(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_vnode *vn;
	struct stat st;
	char *realname;

	DPRINTF("Opcode:\tgetattr\n");
	DPRINTF("Inode:\t%llu\n", (unsigned long long)fbuf->fb_ino);
	bzero(&st, sizeof(st));

	fbuf->fb_len = 0;
	bzero(&fbuf->fb_vattr, sizeof(fbuf->fb_vattr));

	vn = tree_get(&f->vnode_tree, fbuf->fb_ino);

	realname = build_realname(f, vn->ino);
	fbuf->fb_err = update_vattr(f, &fbuf->fb_vattr, realname, vn);
	free(realname);

	if (fbuf->fb_err)
		fbuf->fb_len = 0;

	return (0);
}

static int
ifuse_ops_access(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_vnode *vn;
	char *realname;

	CHECK_OPT(access);

	fbuf->fb_len = 0;

	vn = tree_get(&f->vnode_tree, fbuf->fb_ino);

	realname = build_realname(f, vn->ino);
	fbuf->fb_err = f->op.access(realname, fbuf->fb_io_mode);
	free(realname);

	return (0);
}

static int
ifuse_ops_open(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_file_info ffi;
	struct fuse_vnode *vn;
	char *realname;

	CHECK_OPT(open);

	bzero(&ffi, sizeof(ffi));
	ffi.flags = fbuf->fb_io_flags;

	fbuf->fb_len = 0;

	vn = tree_get(&f->vnode_tree, fbuf->fb_ino);

	realname = build_realname(f, vn->ino);
	fbuf->fb_err = f->op.open(realname, &ffi);
	free(realname);

	if (!fbuf->fb_err)
		fbuf->fb_io_fd = ffi.fh;

	return (0);
}

static int
ifuse_ops_opendir(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_file_info ffi;
	struct fuse_vnode *vn;
	char *realname;

	DPRINTF("Opcode:\topendir\n");
	DPRINTF("Inode:\t%llu\n", (unsigned long long)fbuf->fb_ino);

	memset(&ffi, 0, sizeof(ffi));
	ffi.flags = fbuf->fb_io_flags;

	fbuf->fb_len = 0;

	vn = tree_get(&f->vnode_tree, fbuf->fb_ino);

	if (f->op.opendir) {
		realname = build_realname(f, vn->ino);
		fbuf->fb_err = f->op.opendir(realname, &ffi);
		free(realname);
	}

	if (!fbuf->fb_err) {
		fbuf->fb_io_fd = ffi.fh;
		fbuf->fb_len = 0;

		vn->fd = calloc(1, sizeof(*vn->fd));
		if (vn->fd == NULL)
			return (errno);

		vn->fd->filled = 0;
		vn->fd->size = 0;
		vn->fd->start = 0;
	}

	return (0);
}

#define GENERIC_DIRSIZ(NLEN) \
((sizeof (struct dirent) - (MAXNAMLEN+1)) + ((NLEN+1 + 3) &~ 3))

static int
ifuse_fill_readdir(void *dh, const char *name, const struct stat *stbuf,
    off_t off)
{
	struct fuse_dirhandle *fd = dh;
	struct fusebuf *fbuf;
	struct dirent *dir;
	uint32_t namelen;
	uint32_t len;

	fbuf = fd->buf;
	namelen = strnlen(name, MAXNAMLEN);
	len = GENERIC_DIRSIZ(namelen);

	if ((fd->full || (fbuf->fb_len + len > fd->size)) ||
	    (!fd->isgetdir && fd->off != off)) {
		fd->full = 1;
		return (0);
	}

	if (fd->isgetdir && fd->start != 0 &&  fd->idx < fd->start) {
		fd->idx += len;
		return (0);
	}

	dir = (struct dirent *) &fbuf->fb_dat[fbuf->fb_len];

	if (off)
		fd->filled = 0;

	if (stbuf) {
		dir->d_fileno = stbuf->st_ino;
		dir->d_type = stbuf->st_mode;
	} else {
		dir->d_fileno = 0xffffffff;
		dir->d_type = 0;
	}
	dir->d_namlen = namelen;
	dir->d_reclen = len;
	memcpy(dir->d_name, name, namelen);

	fbuf->fb_len += len;
	if (fd->isgetdir) {
		fd->start += len;
		fd->idx += len;
	}
	return (0);
}

static int
ifuse_fill_getdir(fuse_dirh_t fd, const char *name, int type, ino_t ino)
{
	struct stat st;

	bzero(&st, sizeof(st));
	st.st_mode = type << 12;
	if (ino == 0)
		st.st_ino = 0xffffffff;
	else
		st.st_ino = ino;

	return (fd->filler(fd, name, &st, 0));
}

static int
ifuse_ops_readdir(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_file_info ffi;
	struct fuse_vnode *vn;
	char *realname;
	uint64_t offset;
	uint32_t size;
	uint32_t startsave;

	DPRINTF("Opcode:\treaddir\n");
	DPRINTF("Inode:\t%llu\n", (unsigned long long)fbuf->fb_ino);
	DPRINTF("Offset:\t%llu\n", fbuf->fb_io_off);
	DPRINTF("Size:\t%lu\n", fbuf->fb_io_len);

	bzero(&ffi, sizeof(ffi));
	ffi.fh = fbuf->fb_io_fd;
	offset = fbuf->fb_io_off;
	size = fbuf->fb_io_len;
	startsave = 0;

	fbuf->fb_len = 0;
	fbuf->fb_dat = malloc(FUSEBUFMAXSIZE);
	bzero(fbuf->fb_dat, FUSEBUFMAXSIZE);

	vn = tree_get(&f->vnode_tree, fbuf->fb_ino);

	if (!vn->fd->filled) {
		vn->fd->filler = ifuse_fill_readdir;
		vn->fd->buf = fbuf;
		vn->fd->filled = 0;
		vn->fd->full = 0;
		vn->fd->isgetdir = 0;
		vn->fd->size = size;
		vn->fd->off = offset;

		realname = build_realname(f, vn->ino);
		if (f->op.readdir)
			fbuf->fb_err = f->op.readdir(realname, vn->fd,
			    ifuse_fill_readdir, offset, &ffi);
		else if (f->op.getdir) {
			vn->fd->isgetdir = 1;
			vn->fd->idx = 0;
			startsave = vn->fd->start;
			fbuf->fb_err = f->op.getdir(realname, vn->fd,
			    ifuse_fill_getdir);
		} else
			fbuf->fb_err = -ENOSYS;
		free(realname);
	}

	if (!vn->fd->full && vn->fd->isgetdir && vn->fd->start == startsave)
		vn->fd->filled = 1;

	if (fbuf->fb_err) {
		fbuf->fb_len = 0;
		vn->fd->filled = 1;
	}

	return (0);
}

static int
ifuse_ops_releasedir(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_file_info ffi;
	struct fuse_vnode *vn;
	char *realname;

	DPRINTF("Opcode:\treleasedir\n");
	DPRINTF("Inode:\t%llu\n", (unsigned long long)fbuf->fb_ino);

	bzero(&ffi, sizeof(ffi));
	ffi.fh = fbuf->fb_io_fd;
	ffi.fh_old = ffi.fh;
	ffi.flags = fbuf->fb_io_flags;

	fbuf->fb_len = 0;

	vn = tree_get(&f->vnode_tree, fbuf->fb_ino);

	if (f->op.releasedir) {
		realname = build_realname(f, vn->ino);
		fbuf->fb_err = f->op.releasedir(realname, &ffi);
		free(realname);
	} else
		fbuf->fb_err = 0;

	if (!fbuf->fb_err)
		free(vn->fd);

	return (0);
}

static int
ifuse_ops_release(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_file_info ffi;
	struct fuse_vnode *vn;
	char *realname;

	CHECK_OPT(release);

	bzero(&ffi, sizeof(ffi));
	ffi.fh = fbuf->fb_io_fd;
	ffi.fh_old = ffi.fh;
	ffi.flags = fbuf->fb_io_flags;

	fbuf->fb_len = 0;

	vn = tree_get(&f->vnode_tree, fbuf->fb_ino);

	realname = build_realname(f, vn->ino);
	fbuf->fb_err = f->op.release(realname, &ffi);
	free(realname);

	return (0);
}

static int
ifuse_ops_lookup(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_vnode *vn;
	char *realname;

	DPRINTF("Opcode:\tlookup\n");
	DPRINTF("Inode:\t%llu\n", (unsigned long long)fbuf->fb_ino);
	DPRINTF("For file %s\n", fbuf->fb_dat);

	vn = get_vn_by_name_and_parent(f, fbuf->fb_dat, fbuf->fb_ino);
	if (vn == NULL) {
		vn = alloc_vn(f, fbuf->fb_dat, -1, fbuf->fb_ino);
		if (vn == NULL) {
			fbuf->fb_err = -ENOMEM;
			fbuf->fb_len = 0;
			free(fbuf->fb_dat);
			return (0);
		}
		set_vn(f, vn); /*XXX*/
	}

	DPRINTF("new ino %llu\n", (unsigned long long)vn->ino);
	fbuf->fb_len = 0;

	realname = build_realname(f, vn->ino);
	fbuf->fb_err = update_vattr(f, &fbuf->fb_vattr, realname, vn);
	free(fbuf->fb_dat);
	free(realname);

	if (fbuf->fb_err)
		fbuf->fb_len = 0;

	return (0);
}

static int
ifuse_ops_read(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_file_info ffi;
	struct fuse_vnode *vn;
	char *realname;
	uint64_t offset;
	uint32_t size;
	int ret;

	CHECK_OPT(read);

	bzero(&ffi, sizeof(ffi));
	ffi.fh = fbuf->fb_io_fd;
	size = fbuf->fb_io_len;
	offset = fbuf->fb_io_off;

	fbuf->fb_len = 0;
	fbuf->fb_dat = malloc(size);

	vn = tree_get(&f->vnode_tree, fbuf->fb_ino);

	realname = build_realname(f, vn->ino);
	ret = f->op.read(realname, fbuf->fb_dat, size, offset, &ffi);
	free(realname);
	if (ret >= 0) {
		fbuf->fb_len = ret;
		fbuf->fb_err = 0;
	} else	{
		fbuf->fb_len = 0;
		fbuf->fb_err = ret;
	}

	return (0);
}

static int
ifuse_ops_write(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_file_info ffi;
	struct fuse_vnode *vn;
	char *realname;
	uint64_t offset;
	uint32_t size;
	int ret;

	CHECK_OPT(write);

	bzero(&ffi, sizeof(ffi));
	ffi.fh = fbuf->fb_io_fd;
	ffi.fh_old = ffi.fh;
	ffi.writepage = fbuf->fb_io_flags & 1;
	size = fbuf->fb_io_len;
	offset = fbuf->fb_io_off;

	fbuf->fb_len = 0;

	vn = tree_get(&f->vnode_tree, fbuf->fb_ino);

	realname = build_realname(f, vn->ino);
	ret = f->op.write(realname, fbuf->fb_dat, size, offset, &ffi);
	free(realname);
	free(fbuf->fb_dat);

	if (ret >= 0) {
		fbuf->fb_io_len = ret;
		fbuf->fb_err = 0;
		fbuf->fb_len = 0;
	} else
		fbuf->fb_err = ret;

	return (0);
}

static int
ifuse_ops_create(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_file_info ffi;
	struct fuse_vnode *vn;
	uint32_t mode;

	char *realname;

	CHECK_OPT(create);

	bzero(&ffi, sizeof(ffi));
	ffi.flags = fbuf->fb_io_flags;
	mode = fbuf->fb_io_mode;
	vn = get_vn_by_name_and_parent(f, fbuf->fb_dat, fbuf->fb_ino);

	fbuf->fb_len = 0;
	free(fbuf->fb_dat);

	realname = build_realname(f, vn->ino);
	fbuf->fb_err = f->op.create(realname, mode,  &ffi);

	if (!fbuf->fb_err) {
		fbuf->fb_err = update_vattr(f, &fbuf->fb_vattr, realname, vn);
		fbuf->fb_ino = fbuf->fb_vattr.va_fileid;
		fbuf->fb_io_mode = fbuf->fb_vattr.va_mode;

		if (!fbuf->fb_err)
			fbuf->fb_len = 0;
	}
	free(realname);

	return (0);
}

static int
ifuse_ops_mkdir(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_vnode *vn;
	char *realname;
	uint32_t mode;

	CHECK_OPT(mkdir);

	mode = fbuf->fb_io_mode;
	vn = get_vn_by_name_and_parent(f, fbuf->fb_dat, fbuf->fb_ino);

	fbuf->fb_len = 0;
	free(fbuf->fb_dat);

	realname = build_realname(f, vn->ino);
	fbuf->fb_err = f->op.mkdir(realname, mode);

	if (!fbuf->fb_err) {
		fbuf->fb_err = update_vattr(f, &fbuf->fb_vattr, realname, vn);
		fbuf->fb_io_mode = fbuf->fb_vattr.va_mode;
		fbuf->fb_ino = vn->ino;

		if (!fbuf->fb_err)
			fbuf->fb_len = 0;
	}
	free(realname);

	return (0);
}

static int
ifuse_ops_rmdir(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_vnode *vn;
	char *realname;

	CHECK_OPT(rmdir);

	fbuf->fb_len = 0;
	vn = get_vn_by_name_and_parent(f, fbuf->fb_dat, fbuf->fb_ino);

	free(fbuf->fb_dat);
	realname = build_realname(f, vn->ino);
	fbuf->fb_err = f->op.rmdir(realname);
	free(realname);

	return (0);
}

static int
ifuse_ops_readlink(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_vnode *vn;
	char *realname;
	char name[PATH_MAX + 1];
	int len, ret;

	DPRINTF("Opcode:\treadlink\n");
	DPRINTF("Inode:\t%llu\n", (unsigned long long)fbuf->fb_ino);

	vn = tree_get(&f->vnode_tree, fbuf->fb_ino);

	realname = build_realname(f, vn->ino);
	if (f->op.readlink)
		ret = f->op.readlink(realname, name, sizeof(name));
	else
		ret = -ENOSYS;
	free(realname);

	if (!ret)
		len = strnlen(name, PATH_MAX);
	else
		len = -1;

	fbuf->fb_len = len + 1;
	fbuf->fb_err = ret;

	if (!ret) {
		fbuf->fb_dat = malloc(fbuf->fb_len);
		memcpy(fbuf->fb_dat, name, len);
		fbuf->fb_dat[len] = '\0';
	} else
		fbuf->fb_len = 0;

	return (0);
}

static int
ifuse_ops_unlink(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_vnode *vn;
	char *realname;

	fbuf->fb_len = 0;

	CHECK_OPT(unlink);

	vn = get_vn_by_name_and_parent(f, fbuf->fb_dat, fbuf->fb_ino);
	free(fbuf->fb_dat);

	realname = build_realname(f, vn->ino);
	fbuf->fb_err = f->op.unlink(realname);
	free(realname);

	return (0);
}

static int
ifuse_ops_statfs(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_vnode *vn;
	char *realname;

	fbuf->fb_len = 0;
	bzero(&fbuf->fb_stat, sizeof(fbuf->fb_stat));

	CHECK_OPT(statfs);

	vn = tree_get(&f->vnode_tree, fbuf->fb_ino);

	realname = build_realname(f, vn->ino);
	fbuf->fb_err = f->op.statfs(realname, &fbuf->fb_stat);
	free(realname);

	if (fbuf->fb_err)
		fbuf->fb_len = 0;

	return (0);
}

static int
ifuse_ops_link(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_vnode *vn;
	char *realname;
	char *realname_ln;
	ino_t oldnodeid;

	CHECK_OPT(link);
	oldnodeid = fbuf->fb_io_ino;
	vn = get_vn_by_name_and_parent(f, fbuf->fb_dat, fbuf->fb_ino);

	fbuf->fb_len = 0;
	free(fbuf->fb_dat);

	realname = build_realname(f, oldnodeid);
	realname_ln = build_realname(f, vn->ino);
	fbuf->fb_err = f->op.link(realname, realname_ln);
	free(realname);
	free(realname_ln);

	return (0);
}

static int
ifuse_ops_setattr(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_vnode *vn;
	struct timespec ts[2];
	struct utimbuf tbuf;
	struct fb_io *io;
	char *realname;
	uid_t uid;
	gid_t gid;

	DPRINTF("Opcode:\tsetattr\n");
	DPRINTF("Inode:\t%llu\n", (unsigned long long)fbuf->fb_ino);

	vn = tree_get(&f->vnode_tree, fbuf->fb_ino);
	realname = build_realname(f, vn->ino);
	io = fbtod(fbuf, struct fb_io *);

	if (io->fi_flags & FUSE_FATTR_MODE) {
		if (f->op.chmod)
			fbuf->fb_err = f->op.chmod(realname,
			    fbuf->fb_vattr.va_mode);
		else
			fbuf->fb_err = -ENOSYS;
	}

	if (!fbuf->fb_err && (io->fi_flags & FUSE_FATTR_UID ||
	    io->fi_flags & FUSE_FATTR_GID) ) {
		uid = (io->fi_flags & FUSE_FATTR_UID) ?
		    fbuf->fb_vattr.va_uid : (gid_t)-1;
		gid = (io->fi_flags & FUSE_FATTR_GID) ?
		    fbuf->fb_vattr.va_gid : (uid_t)-1;
		if (f->op.chown)
			fbuf->fb_err = f->op.chown(realname, uid, gid);
		else
			fbuf->fb_err = -ENOSYS;
	}

	if (!fbuf->fb_err && ( io->fi_flags & FUSE_FATTR_MTIME ||
		io->fi_flags & FUSE_FATTR_ATIME)) {
		ts[0].tv_sec = fbuf->fb_vattr.va_atime.tv_sec;
		ts[0].tv_nsec = fbuf->fb_vattr.va_atime.tv_nsec;
		ts[1].tv_sec = fbuf->fb_vattr.va_mtime.tv_sec;
		ts[1].tv_nsec = fbuf->fb_vattr.va_mtime.tv_nsec;
		tbuf.actime = ts[0].tv_sec;
		tbuf.modtime = ts[1].tv_sec;

		if (f->op.utimens)
			fbuf->fb_err = f->op.utimens(realname, ts);
		else if (f->op.utime)
			fbuf->fb_err = f->op.utime(realname, &tbuf);
		else
			fbuf->fb_err = -ENOSYS;
	}

	fbuf->fb_len = 0;
	bzero(&fbuf->fb_vattr, sizeof(fbuf->fb_vattr));

	if (!fbuf->fb_err)
		fbuf->fb_err = update_vattr(f, &fbuf->fb_vattr, realname, vn);
	free(realname);

	if (fbuf->fb_err)
		fbuf->fb_len = 0;
	free(fbuf->fb_dat);

	return (0);
}

static int
ifuse_ops_symlink(unused struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_vnode *vn;
	char *realname;
	int len;

	CHECK_OPT(symlink);

	vn = get_vn_by_name_and_parent(f, fbuf->fb_dat, fbuf->fb_ino);
	len = strlen(fbuf->fb_dat);
	fbuf->fb_len = 0;

	realname = build_realname(f, vn->ino);
	/* fuse invert the symlink params */
	fbuf->fb_err = f->op.symlink(&fbuf->fb_dat[len + 1], realname);
	fbuf->fb_ino = vn->ino;
	free(fbuf->fb_dat);
	free(realname);

	return (0);
}

static int
ifuse_ops_rename(struct fuse *f, struct fusebuf *fbuf)
{
	struct fuse_vnode *vnt;
	struct fuse_vnode *vnf;
	char *realnamef;
	char *realnamet;
	int len;

	CHECK_OPT(rename);

	len = strlen(fbuf->fb_dat);
	vnf = get_vn_by_name_and_parent(f, fbuf->fb_dat, fbuf->fb_ino);
	vnt = get_vn_by_name_and_parent(f, &fbuf->fb_dat[len + 1],
	    fbuf->fb_io_ino);
	fbuf->fb_len = 0;
	free(fbuf->fb_dat);

	realnamef = build_realname(f, vnf->ino);
	realnamet = build_realname(f, vnt->ino);
	fbuf->fb_err = f->op.rename(realnamef, realnamet);
	free(realnamef);
	free(realnamet);

	return (0);
}

static int
ifuse_ops_destroy(struct fuse *f, struct fusebuf *fbuf)
{
	DPRINTF("Opcode:\tdestroy\n");

	fbuf->fb_len = 0;

	fbuf->fb_err = 0;
	f->fc->dead = 1;

	return (0);
}

int
ifuse_exec_opcode(struct fuse *f, struct fusebuf *fbuf)
{
	int ret = 0;

	switch (fbuf->fb_type) {
	case FBT_LOOKUP:
		ret = ifuse_ops_lookup(f, fbuf);
		break;
	case FBT_GETATTR:
		ret = ifuse_ops_getattr(f, fbuf);
		break;
	case FBT_SETATTR:
		ret = ifuse_ops_setattr(f, fbuf);
		break;
	case FBT_READLINK:
		ret = ifuse_ops_readlink(f, fbuf);
		break;
	case FBT_MKDIR:
		ret = ifuse_ops_mkdir(f, fbuf);
		break;
	case FBT_UNLINK:
		ret = ifuse_ops_unlink(f, fbuf);
		break;
	case FBT_RMDIR:
		ret = ifuse_ops_rmdir(f, fbuf);
		break;
	case FBT_LINK:
		ret = ifuse_ops_link(f, fbuf);
		break;
	case FBT_OPEN:
		ret = ifuse_ops_open(f, fbuf);
		break;
	case FBT_READ:
		ret = ifuse_ops_read(f, fbuf);
		break;
	case FBT_WRITE:
		ret = ifuse_ops_write(f, fbuf);
		break;
	case FBT_STATFS:
		ret = ifuse_ops_statfs(f, fbuf);
		break;
	case FBT_RELEASE:
		ret = ifuse_ops_release(f, fbuf);
		break;
	case FBT_INIT:
		ret = ifuse_ops_init(fbuf);
		break;
	case FBT_OPENDIR:
		ret = ifuse_ops_opendir(f, fbuf);
		break;
	case FBT_READDIR:
		ret = ifuse_ops_readdir(f, fbuf);
		break;
	case FBT_RELEASEDIR:
		ret = ifuse_ops_releasedir(f, fbuf);
		break;
	case FBT_ACCESS:
		ret = ifuse_ops_access(f, fbuf);
		break;
	case FBT_CREATE:
		ret = ifuse_ops_create(f, fbuf);
		break;
	case FBT_SYMLINK:
		ret = ifuse_ops_symlink(f, fbuf);
		break;
	case FBT_RENAME:
		ret = ifuse_ops_rename(f, fbuf);
		break;
	case FBT_DESTROY:
		ret = ifuse_ops_destroy(f, fbuf);
		break;
	default:
		DPRINTF("Opcode:\t%i not supported\n", fbuf->fb_type);
		DPRINTF("Inode:\t%llu\n", (unsigned long long)fbuf->fb_ino);

		fbuf->fb_err = -ENOSYS;
		fbuf->fb_len = 0;
	}
	DPRINTF("\n");

	/* fuse api use negative errno */
	fbuf->fb_err = -fbuf->fb_err;
	return (ret);
}

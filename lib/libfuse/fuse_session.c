/* $OpenBSD: fuse_session.c,v 1.3 2026/06/22 05:24:19 helg Exp $ */
/*
 * Copyright (c) 2025 Helg Bredow <helg@openbsd.org>
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

#include <sys/uio.h>
#include <sys/fusebuf.h>
#include <errno.h>
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "fuse_private.h"

void
fuse_session_destroy(struct fuse_session *se)
{
	if (se->init && se->llops.destroy)
		se->llops.destroy(se->userdata);

	free(se->chan);
	free(se);
}
DEF(fuse_session_destroy);

void
fuse_session_add_chan(struct fuse_session *se, struct fuse_chan *ch)
{
	if (se->chan == NULL && ch->se == NULL) {
		se->chan = ch;
		ch->se = se;
	}
}
DEF(fuse_session_add_chan);

void
fuse_session_remove_chan(struct fuse_chan *ch)
{
	if (ch->se->chan == ch) {
		ch->se->chan = NULL;
		ch->se = NULL;
	}
}
DEF(fuse_session_remove_chan);

void
fuse_session_exit(struct fuse_session *se)
{
	se->exit = 1;
}
DEF(fuse_session_exit);

int
fuse_session_exited(const struct fuse_session *se)
{
	return (se->exit);
}
DEF(fuse_session_exited);

void
fuse_session_reset(struct fuse_session *se)
{
	if (se != NULL)
		se->exit = 0;
}
DEF(fuse_session_reset);

int
fuse_session_loop(struct fuse_session *se)
{
	struct fuse_chan *ch;
	char *buf;
	size_t bufsize;
	int err;

	if (se == NULL)
		return (-1);

	ch = se->chan;
	if (ch == NULL)
		return (-1);

	/*
	 * Prepare the read data buffer. We need enough space for the header,
	 * input struct and any additional data, filenames or the buffer for
	 * write(2). The minimum buffer size must be large enough for the
	 * name and path parameters for FUSE_SYMLINK.
	 */
	if (se->fci.max_write > FUSEBUFMAXSIZE)
		bufsize = sizeof(struct fusebuf) + FUSEBUFMAXSIZE;
	else if (se->fci.max_write < PATH_MAX + NAME_MAX)
		bufsize = sizeof(struct fusebuf) + PATH_MAX + NAME_MAX;
	else
		bufsize = sizeof(struct fusebuf) + se->fci.max_write;

	buf = calloc(1, bufsize);
	if (buf == NULL) {
		DPERROR(__func__);
		return (-1);
	}

	while (!fuse_session_exited(se)) {
		err = fuse_chan_recv(&ch, buf, bufsize);
		if (err == -EINTR || err == -ENODEV) {
			fuse_session_exit(se);
			continue;
		} else if (err <= 0) {
			DPERROR(__func__);
			break;
		}

		fuse_session_process(se, buf, bufsize, ch);
	}

	free(buf);
	fuse_session_reset(se);

	return (err == 0 ? 0 : -1);
}
DEF(fuse_session_loop);

static void
iprocess_init(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_conn_info *fci = &se->fci;
	struct fuse_init_out out;
	uint32_t major = fbuf->in.init.major;
	uint32_t minor = fbuf->in.init.minor;

	DPRINTF("%-11s", "init");
	DPRINTF("Kernel: %d.%d\t", major, minor);
	DPRINTF("libfuse: %d.%d\t", fci->proto_major, fci->proto_minor);

	if (major != fci->proto_major && minor != fci->proto_minor)
		errx(1, "FUSE kernel protocol version mismatch");

	if (se->llops.init)
		se->llops.init(se->userdata, fci);

	memset(&out, 0, sizeof(out));
	out.major = fci->proto_major;
	out.minor = fci->proto_minor;
	out.max_write = fci->max_write;

	DPRINTF("max_write: %u\t", out.max_write);

	fuse_reply_buf(req, (const char *)&out, sizeof(out));

	se->init = 1;
}

static void
iprocess_destroy(fuse_req_t req)
{
	struct fuse_session *se = req->se;

	DPRINTF("%-11s", "destroy");

	se->chan->dead = 1;
	fuse_session_exit(se);
	fuse_reply_err(req, 0);
}

static void
iprocess_lookup(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const char *name = fb_dat(fbuf->hdr);

	DPRINTF("%-11s", "lookup");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("name: %s\t", name);

	if (se->llops.lookup)
		se->llops.lookup(req, fbuf->fb_ino, name);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_getattr(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;

	DPRINTF("%-11s", "getattr");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);

	if (se->llops.getattr) {
		/* fuse_getattr_in is unused */
		se->llops.getattr(req, fbuf->fb_ino, NULL);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_setattr(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct stat stbuf;
	const int flags = fbuf->in.setattr.valid;

	DPRINTF("%-11s", "setattr");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);

	if (se->llops.setattr) {
		memset(&stbuf, 0, sizeof(stbuf));
		stbuf.st_size =  fbuf->in.setattr.size;
		stbuf.st_atime =  fbuf->in.setattr.atime;
		stbuf.st_mtime =  fbuf->in.setattr.mtime;
		stbuf.st_atim.tv_nsec =  fbuf->in.setattr.atimensec;
		stbuf.st_mtim.tv_nsec =  fbuf->in.setattr.mtimensec;
		stbuf.st_mode =  fbuf->in.setattr.mode;
		stbuf.st_uid =  fbuf->in.setattr.uid;
		stbuf.st_gid =  fbuf->in.setattr.gid;

		se->llops.setattr(req, fbuf->fb_ino, &stbuf, flags, NULL);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_opendir(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "opendir");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);

	memset(&ffi, 0, sizeof(ffi));
	ffi.flags = fbuf->in.open.flags;

	if (se->llops.opendir)
		se->llops.opendir(req, fbuf->fb_ino, &ffi);
	else
		fuse_reply_open(req, &ffi);
}

static void
iprocess_readdir(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "readdir");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("size: %u\t", fbuf->in.read.size);
	DPRINTF("offset: %llu\t", fbuf->in.read.offset);

	if (se->llops.readdir) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->in.read.fh;
		ffi.fh_old = ffi.fh;

		se->llops.readdir(req, fbuf->fb_ino, fbuf->in.read.size,
		    fbuf->in.read.offset, &ffi);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_releasedir(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "releasedir");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);

	if (se->llops.releasedir) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->in.release.fh;
		ffi.fh_old = ffi.fh;
		ffi.flags = fbuf->in.release.flags;

		se->llops.releasedir(req, fbuf->fb_ino, &ffi);
	} else
		fuse_reply_err(req, 0);
}

static void
iprocess_mkdir(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const char *name = fb_dat(fbuf->in.mkdir);

	DPRINTF("%-11s", "mkdir");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("mode: %#6o\t", fbuf->in.mkdir.mode);
	DPRINTF("name: %s\t", name);

	if (se->llops.mkdir) {
		req->ctx.umask = fbuf->in.mkdir.umask;
		se->llops.mkdir(req, fbuf->fb_ino, name, fbuf->in.mkdir.mode);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_rmdir(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const char *name = fb_dat(fbuf->hdr);

	DPRINTF("%-11s", "rmdir");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("name: %s\t", name);

	if (se->llops.rmdir)
		se->llops.rmdir(req, fbuf->fb_ino, name);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_mknod(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const char *name = fb_dat(fbuf->in.mknod);

	DPRINTF("%-11s", "mknod");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("mode: %#6o\t", fbuf->in.mknod.mode);
	DPRINTF("name: %s\t", name);

	if (se->llops.mknod) {
		req->ctx.umask = fbuf->in.mknod.umask;
		se->llops.mknod(req, fbuf->fb_ino, name, fbuf->in.mknod.mode,
		    fbuf->in.mknod.rdev);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_open(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "open");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);

	memset(&ffi, 0, sizeof(ffi));
	ffi.flags = fbuf->in.open.flags;

	if (se->llops.open)
		se->llops.open(req, fbuf->fb_ino, &ffi);
	else
		fuse_reply_open(req, &ffi);
}

static void
iprocess_read(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "read");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("size: %u\t", fbuf->in.read.size);
	DPRINTF("offset: %llu\t", fbuf->in.read.offset);

	if (se->llops.read) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->in.read.fh;
		ffi.fh_old = ffi.fh;
		ffi.flags = fbuf->in.read.flags;

		se->llops.read(req, fbuf->fb_ino, fbuf->in.read.size,
		    fbuf->in.read.offset, &ffi);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_write(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "write");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("size: %u\t", fbuf->in.write.size);
	DPRINTF("offset: %llu\t", fbuf->in.write.offset);

	if (se->llops.write) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->in.write.fh;
		ffi.fh_old = ffi.fh;
		ffi.writepage = fbuf->in.write.flags & 1; // XXX

		se->llops.write(req, fbuf->fb_ino, fb_dat(fbuf->in.write),
		    fbuf->in.write.size, fbuf->in.write.offset, &ffi);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_fsync(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;
	const uint32_t fsync_flags = fbuf->in.fsync.fsync_flags & 1;

	DPRINTF("%-11s", "fsync");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("flags: %u", fsync_flags);

	if (se->llops.fsync) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->in.fsync.fh;
		ffi.fh_old = ffi.fh;

		se->llops.fsync(req, fbuf->fb_ino, fsync_flags, &ffi);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_flush(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "flush");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);

	if (se->llops.flush) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->in.flush.fh;
		ffi.fh_old = ffi.fh;
		ffi.flush = 1;

		se->llops.flush(req, fbuf->fb_ino, &ffi);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_release(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "release");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);

	if (se->llops.release) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->in.release.fh;
		ffi.fh_old = ffi.fh;
		ffi.flags = fbuf->in.release.flags;

		se->llops.release(req, fbuf->fb_ino, &ffi);
	} else
		fuse_reply_err(req, 0);
}

static void
iprocess_forget(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const uint64_t nlookup = fbuf->in.forget.nlookup;

	DPRINTF("%-11s", "forget");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("nlookup: %llu\t", nlookup);

	if (se->llops.forget)
		se->llops.forget(req, fbuf->fb_ino, nlookup);
	else
		fuse_reply_none(req);
}

static void
iprocess_symlink(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const char *target;
	const char *name;
	int len;

	name = fb_dat(fbuf->hdr);
	len = strlen(name);
	target = &name[len + 1];

	DPRINTF("%-11s", "symlink");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("name: %s\t", name);
	DPRINTF("target: %s\t", target);

	if (se->llops.symlink)
		se->llops.symlink(req, target, fbuf->fb_ino, name);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_readlink(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;

	DPRINTF("%-11s", "readlink");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);

	if (se->llops.readlink)
		se->llops.readlink(req, fbuf->fb_ino);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_link(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const char *name = fb_dat(fbuf->in.link);
	const uint64_t oldnodeid = fbuf->in.link.oldnodeid;

	DPRINTF("%-11s", "link");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("inode: %llu\t", oldnodeid);
	DPRINTF("name: %s\t", name);

	if (se->llops.link)
		se->llops.link(req, oldnodeid, fbuf->fb_ino, name);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_unlink(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const char *name = fb_dat(fbuf->hdr);

	DPRINTF("%-11s", "unlink");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("name: %s\t", name);

	if (se->llops.unlink)
		se->llops.unlink(req, fbuf->fb_ino, name);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_rename(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const uint64_t newdir = fbuf->in.rename.newdir;
	const char *target;
	const char *name;
	int len;

	name = fb_dat(fbuf->in.rename);
	len = strlen(name);
	target = &name[len + 1];

	DPRINTF("%-11s", "rename");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("name: %s\t", name);
	DPRINTF("newdir: %llu\t", newdir);
	DPRINTF("target: %s\t", target);

	if (se->llops.rename)
		se->llops.rename(req, fbuf->fb_ino, name, newdir, target);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_statfs(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;

	DPRINTF("%-11s", "statfs");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);

	if (se->llops.statfs)
		se->llops.statfs(req, fbuf->fb_ino);
	else
		fuse_reply_err(req, ENOSYS);
}

void
fuse_session_process(struct fuse_session *se, const char *buf, size_t len,
    struct fuse_chan *ch)
{
	struct fusebuf *fbuf;
	struct fuse_req req;

	fbuf = (struct fusebuf *)buf;
	req.fbuf = fbuf;
	req.se = se;
	req.ch = (ch == NULL) ? se->chan : ch;
	req.ctx.uid = fbuf->fb_uid;
	req.ctx.gid = fbuf->fb_gid;
	req.ctx.pid = fbuf->fb_tid;

	/* later set in create, mknod, mkdir */
	req.ctx.umask = 0;

	/* need to at least have the header for the next check */
	if (len < sizeof(fbuf->hdr))
		return;

	if (len < fbuf->hdr.len)
		return;

	switch (fbuf->fb_type) {
	case FUSE_INIT:
		iprocess_init(&req);
		break;
	case FUSE_DESTROY:
		iprocess_destroy(&req);
		break;
	case FUSE_LOOKUP:
		iprocess_lookup(&req);
		break;
	case FUSE_GETATTR:
		iprocess_getattr(&req);
		break;
	case FUSE_SETATTR:
		iprocess_setattr(&req);
		break;
	case FUSE_OPENDIR:
		iprocess_opendir(&req);
		break;
	case FUSE_READDIR:
		iprocess_readdir(&req);
		break;
	case FUSE_RELEASEDIR:
		iprocess_releasedir(&req);
		break;
	case FUSE_MKDIR:
		iprocess_mkdir(&req);
		break;
	case FUSE_RMDIR:
		iprocess_rmdir(&req);
		break;
	case FUSE_MKNOD:
		iprocess_mknod(&req);
		break;
	case FUSE_OPEN:
		iprocess_open(&req);
		break;
	case FUSE_READ:
		iprocess_read(&req);
		break;
	case FUSE_WRITE:
		iprocess_write(&req);
		break;
	case FUSE_FSYNC:
		iprocess_fsync(&req);
		break;
	case FUSE_FLUSH:
		iprocess_flush(&req);
		break;
	case FUSE_RELEASE:
		iprocess_release(&req);
		break;
	case FUSE_FORGET:
		iprocess_forget(&req);
		break;
	case FUSE_SYMLINK:
		iprocess_symlink(&req);
		break;
	case FUSE_READLINK:
		iprocess_readlink(&req);
		break;
	case FUSE_LINK:
		iprocess_link(&req);
		break;
	case FUSE_UNLINK:
		iprocess_unlink(&req);
		break;
	case FUSE_RENAME:
		iprocess_rename(&req);
		break;
	case FUSE_STATFS:
		iprocess_statfs(&req);
		break;
	default:
		DPRINTF("Opcode: %i not supported\t", fbuf->fb_type);
		fuse_reply_err(&req, ENOSYS);
	}
	DPRINTF("\n");
}
DEF(fuse_session_process);

/* $OpenBSD: fuse_session.c,v 1.1 2026/01/22 11:53:31 helg Exp $ */
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
	struct fusebuf fbuf;
	char *buf = (char *)&fbuf;
	size_t bufsize;
	int err;

	if (se == NULL)
		return (-1);

	ch = se->chan;
	if (ch == NULL)
		return (-1);

	/* prepare the read and write data buffer */
	fbuf.fb_dat = calloc(1, FUSEBUFMAXSIZE);
	if (fbuf.fb_dat == NULL) {
		DPERROR(__func__);
		return (-1);
	}

	bufsize = sizeof(fbuf.fb_hdr) + sizeof(fbuf.FD) + FUSEBUFMAXSIZE;

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

	free(fbuf.fb_dat);
	fuse_session_reset(se);

	return (err == 0 ? 0 : -1);
}
DEF(fuse_session_loop);

static void
iprocess_init(fuse_req_t req)
{
	struct fuse_session *se = req->se;
	struct fuse_conn_info fci;

	DPRINTF("%-11s", "init");

	if (se->llops.init) {
		memset(&fci, 0, sizeof(fci));
		fci.proto_minor = FUSE_MINOR_VERSION;
		fci.proto_major = FUSE_MAJOR_VERSION;

		se->llops.init(se->userdata, &fci);
	}

	fuse_reply_err(req, 0);

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
	const char *name = fbuf->fb_dat;

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
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "getattr");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);

	if (se->llops.getattr) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->fb_io_fd;
		ffi.fh_old = ffi.fh;

		se->llops.getattr(req, fbuf->fb_ino, &ffi);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_setattr(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;
	struct stat *stbuf = &fbuf->fb_attr;
	struct fb_io *io = fbtod(fbuf, struct fb_io *);

	DPRINTF("%-11s", "setattr");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);

	if (se->llops.setattr) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->fb_io_fd;
		ffi.fh_old = ffi.fh;

		se->llops.setattr(req, fbuf->fb_ino, stbuf, io->fi_flags, &ffi);
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
	ffi.flags = fbuf->fb_io_flags;

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
	DPRINTF("size: %lu\t", fbuf->fb_io_len);
	DPRINTF("offset: %llu\t", fbuf->fb_io_off);

	if (se->llops.readdir) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->fb_io_fd;
		ffi.fh_old = ffi.fh;

		se->llops.readdir(req, fbuf->fb_ino, fbuf->fb_io_len,
		    fbuf->fb_io_off, &ffi);
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
		ffi.fh = fbuf->fb_io_fd;
		ffi.fh_old = ffi.fh;
		ffi.flags = fbuf->fb_io_flags;

		se->llops.releasedir(req, fbuf->fb_ino, &ffi);
	} else
		fuse_reply_err(req, 0);
}

static void
iprocess_mkdir(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;

	DPRINTF("%-11s", "mkdir");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("mode: %#6o\t", fbuf->fb_io_mode);
	DPRINTF("name: %s\t", fbuf->fb_dat);

	if (se->llops.mkdir)
		se->llops.mkdir(req, fbuf->fb_ino, fbuf->fb_dat,
		    fbuf->fb_io_mode);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_rmdir(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;

	DPRINTF("%-11s", "rmdir");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("name: %s\t", fbuf->fb_dat);

	if (se->llops.rmdir)
		se->llops.rmdir(req, fbuf->fb_ino, fbuf->fb_dat);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_mknod(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;

	DPRINTF("%-11s", "mknod");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("mode: %#6o\t", fbuf->fb_io_mode);
	DPRINTF("name: %s\t", fbuf->fb_dat);

	if (se->llops.mknod)
		se->llops.mknod(req, fbuf->fb_ino, fbuf->fb_dat,
		    fbuf->fb_io_mode, fbuf->fb_io_rdev);
	else
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
	ffi.flags = fbuf->fb_io_flags;

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
	DPRINTF("size: %lu\t", fbuf->fb_io_len);
	DPRINTF("offset: %llu\t", fbuf->fb_io_off);

	if (se->llops.read) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->fb_io_fd;
		ffi.fh_old = ffi.fh;
		ffi.flags = fbuf->fb_io_flags;

		se->llops.read(req, fbuf->fb_ino, fbuf->fb_io_len,
		    fbuf->fb_io_off, &ffi);
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
	DPRINTF("size: %lu\t", fbuf->fb_io_len);
	DPRINTF("offset: %llu\t", fbuf->fb_io_off);

	if (se->llops.write) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->fb_io_fd;
		ffi.fh_old = ffi.fh;
		ffi.writepage = fbuf->fb_io_flags & 1; // XXX

		se->llops.write(req, fbuf->fb_ino, fbuf->fb_dat,
		    fbuf->fb_io_len, fbuf->fb_io_off, &ffi);
	} else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_fsync(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	struct fuse_file_info ffi;

	DPRINTF("%-11s", "fsync");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);

	if (se->llops.fsync) {
		memset(&ffi, 0, sizeof(ffi));
		ffi.fh = fbuf->fb_io_fd;
		ffi.fh_old = ffi.fh;

	        /*
	         * fdatasync(2) is just a wrapper around fsync(2) so datasync
	         * is always false.
	         */
		se->llops.fsync(req, fbuf->fb_ino, 0 /* datasync */, &ffi);
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
		ffi.fh = fbuf->fb_io_fd;
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
		ffi.fh = fbuf->fb_io_fd;
		ffi.fh_old = ffi.fh;
		ffi.flags = fbuf->fb_io_flags;

		se->llops.release(req, fbuf->fb_ino, &ffi);
	} else
		fuse_reply_err(req, 0);
}

static void
iprocess_forget(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;

	DPRINTF("%-11s", "forget");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);

	if (se->llops.forget)
		se->llops.forget(req, fbuf->fb_ino, 1 /* TODO */);
	else
		fuse_reply_err(req, 0);
}

static void
iprocess_symlink(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const char *target;
	const char *name;
	int len;

	name = fbuf->fb_dat;
	len = strnlen(name, fbuf->fb_len);
	target = &fbuf->fb_dat[len + 1];

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
	const char *name = fbuf->fb_dat;

	DPRINTF("%-11s", "link");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("inode: %llu\t", fbuf->fb_io_ino);
	DPRINTF("name: %s\t", name);

	if (se->llops.link)
		se->llops.link(req, fbuf->fb_io_ino, fbuf->fb_ino, name);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_unlink(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;

	DPRINTF("%-11s", "unlink");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("name: %s\t", fbuf->fb_dat);

	if (se->llops.unlink)
		se->llops.unlink(req, fbuf->fb_ino, fbuf->fb_dat);
	else
		fuse_reply_err(req, ENOSYS);
}

static void
iprocess_rename(fuse_req_t req)
{
	struct fusebuf *fbuf = req->fbuf;
	struct fuse_session *se = req->se;
	const char *target;
	const char *name;
	int len;

	name = fbuf->fb_dat;
	len = strnlen(name, fbuf->fb_len);
	target = &fbuf->fb_dat[len + 1];

	DPRINTF("%-11s", "rename");
	DPRINTF("inode: %llu\t", fbuf->fb_ino);
	DPRINTF("name: %s\t", name);
	DPRINTF("inode: %llu\t", fbuf->fb_io_ino);
	DPRINTF("target: %s\t", target);

	if (se->llops.rename)
		se->llops.rename(req, fbuf->fb_ino, name, fbuf->fb_io_ino,
		    target);
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
	req.ctx.umask = fbuf->fb_umask;

	/* need to at least have the header for the next check */
	if (len < sizeof(fbuf->fb_hdr))
		return;

	if (len < sizeof(fbuf->fb_hdr) + sizeof(fbuf->FD) + fbuf->fb_len)
		return;

	switch (fbuf->fb_type) {
	case FBT_INIT:
		iprocess_init(&req);
		break;
	case FBT_DESTROY:
		iprocess_destroy(&req);
		break;
	case FBT_LOOKUP:
		iprocess_lookup(&req);
		break;
	case FBT_GETATTR:
		iprocess_getattr(&req);
		break;
	case FBT_SETATTR:
		iprocess_setattr(&req);
		break;
	case FBT_OPENDIR:
		iprocess_opendir(&req);
		break;
	case FBT_READDIR:
		iprocess_readdir(&req);
		break;
	case FBT_RELEASEDIR:
		iprocess_releasedir(&req);
		break;
	case FBT_MKDIR:
		iprocess_mkdir(&req);
		break;
	case FBT_RMDIR:
		iprocess_rmdir(&req);
		break;
	case FBT_MKNOD:
		iprocess_mknod(&req);
		break;
	case FBT_OPEN:
		iprocess_open(&req);
		break;
	case FBT_READ:
		iprocess_read(&req);
		break;
	case FBT_WRITE:
		iprocess_write(&req);
		break;
	case FBT_FSYNC:
		iprocess_fsync(&req);
		break;
	case FBT_FLUSH:
		iprocess_flush(&req);
		break;
	case FBT_RELEASE:
		iprocess_release(&req);
		break;
	case FBT_RECLAIM:
		iprocess_forget(&req);
		break;
	case FBT_SYMLINK:
		iprocess_symlink(&req);
		break;
	case FBT_READLINK:
		iprocess_readlink(&req);
		break;
	case FBT_LINK:
		iprocess_link(&req);
		break;
	case FBT_UNLINK:
		iprocess_unlink(&req);
		break;
	case FBT_RENAME:
		iprocess_rename(&req);
		break;
	case FBT_STATFS:
		iprocess_statfs(&req);
		break;
	default:
		DPRINTF("Opcode: %i not supported\t", fbuf->fb_type);
		fuse_reply_err(&req, ENOSYS);
	}
	DPRINTF("\n");
}
DEF(fuse_session_process);

/* $OpenBSD: fuse_lowlevel.c,v 1.2 2026/01/29 06:04:27 helg Exp $ */
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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "fuse_private.h"

enum {
	KEY_HELP,
	KEY_VERSION,
	KEY_DEBUG
};

static const struct fuse_opt fuse_ll_opts[] = {
	FUSE_OPT_KEY("debug",		KEY_DEBUG),
	FUSE_OPT_KEY("-d",		KEY_DEBUG),
	FUSE_OPT_KEY("-h",		KEY_HELP),
	FUSE_OPT_KEY("--help",		KEY_HELP),
	FUSE_OPT_KEY("-V",		KEY_VERSION),
	FUSE_OPT_KEY("--version",	KEY_VERSION),
	FUSE_OPT_KEY("max_read=",	FUSE_OPT_KEY_DISCARD),
	FUSE_OPT_END
};

static void
dump_version(void)
{
	fprintf(stderr, "FUSE library version: %d.%d\n", FUSE_MAJOR_VERSION,
	    FUSE_MINOR_VERSION);
}

static void
dump_help(void)
{
	fprintf(stderr,	"    -o max_write=N         max buffer size for "
	    "write operations\n"
	);
}

static int
ifuse_ll_opt_proc(void *data, const char *arg, int key,
    struct fuse_args *outargs)
{
	switch (key) {
	case KEY_HELP:
		dump_help();
		break;
	case KEY_VERSION:
		dump_version();
		break;
	case KEY_DEBUG:
		ifuse_debug_init();
		return (1);
	default:
		fprintf(stderr, "fuse: unknown option -- %s\n", arg);
	}

	return -1;
}

struct fuse_session *
fuse_lowlevel_new(struct fuse_args *fargs,
    const struct fuse_lowlevel_ops *llops, const size_t llops_len,
    void *userdata)
{
	struct fuse_session *se;

	se = calloc(1, sizeof(*se));
	if (se == NULL)
		return (NULL);

	if (fuse_opt_parse(fargs, NULL, fuse_ll_opts, ifuse_ll_opt_proc) == -1) {
		free(se);
		return (NULL);
	}

	if (llops->create && !llops->mknod)
		DPRINTF("libfuse: WARNING: filesystem supports creating files "
		    "but does not implement mknod. No new files can be "
		    "created.\n");

	/* validate size of ops struct */
	if (sizeof(se->llops) == llops_len)
		memcpy(&se->llops, llops, sizeof(se->llops));
	else {
		free(se);
		return (NULL);
	}

	se->userdata = userdata;

	return (se);
}
DEF(fuse_lowlevel_new);

static int
ifuse_reply(fuse_req_t req, const char *data, const size_t data_size, int err)
{
	struct fusebuf *fbuf;
	struct iovec iov[2];
	size_t fbuf_size;

	/* check for sanity */
	if (data == NULL && data_size > 0) {
		DPRINTF("\nlibfuse: NULL data with size: %zu\n", data_size);
		fuse_reply_err(req, EIO);
		return (-EINVAL);
	}

	fbuf = req->fbuf;
	fbuf_size = sizeof(fbuf->fb_hdr) + sizeof(fbuf->FD);

	fbuf->fb_err = err;
	fbuf->fb_len = data_size;

	iov[0].iov_base = fbuf;
	iov[0].iov_len  = fbuf_size;
	iov[1].iov_base = (void *)data;
	iov[1].iov_len  = data_size;

	DPRINTF("errno: %d", fbuf->fb_err);

	return fuse_chan_send(req->ch, iov, 2);
}

static int
ifuse_reply_ok(fuse_req_t req)
{
	return ifuse_reply(req, NULL, 0, 0);
}

int
fuse_reply_err(fuse_req_t req, int err)
{
	return ifuse_reply(req, NULL, 0, err);
}
DEF(fuse_reply_err);

int
fuse_reply_buf(fuse_req_t req, const char *buf, off_t size)
{
	return ifuse_reply(req, buf, size, 0);
}
DEF(fuse_reply_buf);

int
fuse_reply_readlink(fuse_req_t req, char *path)
{
	return ifuse_reply(req, path, strlen(path), 0);
}
DEF(fuse_reply_readlink);

int
fuse_reply_write(fuse_req_t req, size_t size)
{
	struct fusebuf *fbuf;

	fbuf = req->fbuf;
	fbuf->fb_io_len = size;

	return ifuse_reply_ok(req);
}
DEF(fuse_reply_write);

int
fuse_reply_attr(fuse_req_t req, const struct stat *stbuf, double attr_timeout)
{
	struct fusebuf *fbuf;

	fbuf = req->fbuf;
	fbuf->fb_attr = *stbuf;

	return ifuse_reply_ok(req);
}
DEF(fuse_reply_attr);

int
fuse_reply_entry(fuse_req_t req, const struct fuse_entry_param *e)
{
	struct fusebuf *fbuf;

	fbuf = req->fbuf;
	fbuf->fb_attr = e->attr;
	fbuf->fb_ino = e->ino;
	DPRINTF("inode: %llu\t", e->ino);

	return ifuse_reply_ok(req);
}
DEF(fuse_reply_entry);

int
fuse_reply_statfs(fuse_req_t req, const struct statvfs *stbuf)
{
	struct fusebuf *fbuf;

	fbuf = req->fbuf;
	fbuf->fb_stat = *stbuf;

	return ifuse_reply_ok(req);
}
DEF(fuse_reply_statfs);

int
fuse_reply_bmap(fuse_req_t req, uint64_t idx)
{
	DPRINTF("%s: Unsupported", __func__);
	return (-EOPNOTSUPP);
}
DEF(fuse_reply_bmap);

int
fuse_reply_create(fuse_req_t req, const struct fuse_entry_param *e,
     const struct fuse_file_info *ffi)
{
	DPRINTF("%s: Unsupported", __func__);
	return (-EOPNOTSUPP);
}
DEF(fuse_reply_create);

int
fuse_reply_open(fuse_req_t req, const struct fuse_file_info *ffi)
{
	struct fusebuf *fbuf;

	fbuf = req->fbuf;
	fbuf->fb_io_fd = ffi->fh;

	return ifuse_reply_ok(req);
}
DEF(fuse_reply_open);

void
fuse_reply_none(fuse_req_t req)
{
	/* no-op */
}
DEF(fuse_reply_none);

#define GENERIC_DIRSIZ(NLEN) \
((sizeof (struct dirent) - (MAXNAMLEN+1)) + ((NLEN+1 + 7) &~ 7))

size_t
fuse_add_direntry(fuse_req_t req, char *buf, const size_t bufsize,
     const char *name, const struct stat *stbuf, off_t off)
{
	struct dirent *dir;
	size_t namelen;
	size_t len;

	if (name == NULL)
		return (0);

	namelen = strnlen(name, MAXNAMLEN);
	len = GENERIC_DIRSIZ(namelen);

	/* NULL buf is used to request size to be calculated */
	if (buf == NULL || stbuf == NULL || req == NULL)
		return (len);

	/* buffer is full */
	if (bufsize < len)
		return (len);

	dir = (struct dirent *)buf;
	dir->d_fileno = stbuf->st_ino;
	dir->d_type = IFTODT(stbuf->st_mode);
	dir->d_reclen = len;
	dir->d_off = off;
	strlcpy(dir->d_name, name, sizeof(dir->d_name));
	dir->d_namlen = strlen(dir->d_name);

	return (len);
}
DEF(fuse_add_direntry);

const struct fuse_ctx *
fuse_req_ctx(fuse_req_t req)
{
	return (&req->ctx);
}
DEF(fuse_req_ctx);

void *
fuse_req_userdata(fuse_req_t req)
{
	return (req->se->userdata);
}
DEF(fuse_req_userdata);

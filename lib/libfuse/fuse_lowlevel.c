/* $OpenBSD: fuse_lowlevel.c,v 1.3 2026/06/17 13:29:01 helg Exp $ */
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
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "fuse_private.h"

#if defined(__clang__) || __GNUC_PREREQ__(4, 6)
/* confirm that constants in sys/fusebuf.h and fuse_lowlevel.h match */
static_assert(FUSE_FATTR_MODE==FUSE_SET_ATTR_MODE, "definition mismatch");
static_assert(FUSE_FATTR_UID==FUSE_SET_ATTR_UID, "definition mismatch");
static_assert(FUSE_FATTR_GID==FUSE_SET_ATTR_GID, "definition mismatch");
static_assert(FUSE_FATTR_SIZE==FUSE_SET_ATTR_SIZE, "definition mismatch");
static_assert(FUSE_FATTR_ATIME==FUSE_SET_ATTR_ATIME, "definition mismatch");
static_assert(FUSE_FATTR_MTIME==FUSE_SET_ATTR_MTIME, "definition mismatch");
/* TODO: not implemented in kernel yet
static_assert(FUSE_FATTR_ATIME_NOW==FUSE_SET_ATTR_ATIME_NOW,
    "definition mismatch");
static_assert(FUSE_FATTR_MTIME_NOW==FUSE_SET_ATTR_MTIME_NOW,
    "definition mismatch");
*/
#endif

enum {
	KEY_HELP,
	KEY_VERSION,
	KEY_DEBUG
};

/* options supported by fuse_lowlevel_new */
#define FUSE_LIB_OPT(o, m) {o, offsetof(struct fuse_session, fci.m), 1}
static const struct fuse_opt fuse_ll_opts[] = {
	/* core options, also supported by fuse_parse_cmdline(3) */
	FUSE_OPT_KEY("debug",		KEY_DEBUG),
	FUSE_OPT_KEY("-d",		KEY_DEBUG),
	FUSE_OPT_KEY("-h",		KEY_HELP),
	FUSE_OPT_KEY("--help",		KEY_HELP),
	FUSE_OPT_KEY("-V",		KEY_VERSION),
	FUSE_OPT_KEY("--version",	KEY_VERSION),
	/* fuse_lowlevel_new(3) options */
	FUSE_LIB_OPT("max_write=%u",	max_write),
	FUSE_OPT_END
};

static void
dump_version(void)
{
	fprintf(stderr, "OpenBSD FUSE library version: %d.%d\n",
	    FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
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
		return (0);
	default:
		fprintf(stderr, "fuse: unknown option -- %s\n", arg);
	}

	return (-1);
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

	se->fci.proto_major = FUSE_KERNEL_VERSION;
	se->fci.proto_minor = FUSE_KERNEL_MINOR_VERSION;
	/* default that may be overridden by fargs */
	se->fci.max_write = FUSEBUFMAXSIZE;

	if (fuse_opt_parse(fargs, se, fuse_ll_opts, ifuse_ll_opt_proc) == -1) {
		free(se);
		return (NULL);
	}

	/* validate parsed options */
	if (se->fci.max_write > FUSEBUFMAXSIZE) {
		DPRINTF("libfuse: max_write %u too large, using %u instead.\n",
		    se->fci.max_write, FUSEBUFMAXSIZE);
		se->fci.max_write = FUSEBUFMAXSIZE;
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

static void
ifuse_stat2attr(const struct stat *stbuf, struct fuse_attr *attr)
{
	memset(attr, 0, sizeof(*attr));
	attr->ino = stbuf->st_ino;
	attr->size = stbuf->st_size;
	attr->blocks = stbuf->st_blocks;
	attr->atime = stbuf->st_atim.tv_sec;
	attr->mtime = stbuf->st_mtim.tv_sec;
	attr->ctime = stbuf->st_ctim.tv_sec;
	attr->atimensec = stbuf->st_atim.tv_nsec;
	attr->mtimensec = stbuf->st_mtim.tv_nsec;
	attr->ctimensec = stbuf->st_ctim.tv_nsec;
	attr->mode = stbuf->st_mode;
	attr->nlink = stbuf->st_nlink;
	attr->uid = stbuf->st_uid;
	attr->gid = stbuf->st_gid;
	attr->rdev = stbuf->st_rdev;
	attr->blksize = stbuf->st_blksize;
}

static int
ifuse_reply(fuse_req_t req, const void *data, const size_t data_size, int err)
{
	struct fusebuf *fbuf;
	struct fuse_out_header hdr;
	struct iovec iov[2];

	/* check for sanity */
	if (data == NULL && data_size > 0) {
		DPRINTF("\nlibfuse: NULL data with size: %zu\n", data_size);
		fuse_reply_err(req, EIO);
		return (-EINVAL);
	}

	fbuf = req->fbuf;

	hdr.unique = fbuf->fb_uuid;
	hdr.len = sizeof(hdr) + data_size;
	hdr.error = err;

	iov[0].iov_base = &hdr;
	iov[0].iov_len  = sizeof(hdr);
	iov[1].iov_base = (void *)data;
	iov[1].iov_len  = data_size;

	DPRINTF("errno: %d", err);

	return fuse_chan_send(req->ch, iov, 2);
}

/*
 * fuse_reply_err takes a non-negated errno but kernel expects negated errno.
 */
int
fuse_reply_err(fuse_req_t req, int err)
{
	return ifuse_reply(req, NULL, 0, -err);
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
	struct fuse_write_out out;

	memset(&out, 0, sizeof(out));
	out.size = size;

	return ifuse_reply(req, &out, sizeof(out), 0);
}
DEF(fuse_reply_write);

int
fuse_reply_attr(fuse_req_t req, const struct stat *stbuf, double attr_timeout)
{
	struct fuse_attr_out out;

	memset(&out, 0, sizeof(out));
	ifuse_stat2attr(stbuf, &out.attr);

	return ifuse_reply(req, &out, sizeof(out), 0);
}
DEF(fuse_reply_attr);

int
fuse_reply_entry(fuse_req_t req, const struct fuse_entry_param *e)
{
	struct fuse_entry_out out;

	memset(&out, 0, sizeof(out));
	out.nodeid = e->ino;
	out.generation = e->generation;
	ifuse_stat2attr(&e->attr, &out.attr);

	DPRINTF("inode: %llu\t", e->ino);

	return ifuse_reply(req, &out, sizeof(out), 0);
}
DEF(fuse_reply_entry);

int
fuse_reply_statfs(fuse_req_t req, const struct statvfs *stbuf)
{
	struct fuse_statfs_out out;

	memset(&out, 0, sizeof(out));
	out.st.bsize = stbuf->f_bsize;
	out.st.frsize = stbuf->f_frsize;
	out.st.blocks = stbuf->f_blocks;
	out.st.bfree = stbuf->f_bfree;
	out.st.bavail = stbuf->f_bavail;
	out.st.files = stbuf->f_files;
	out.st.ffree = stbuf->f_ffree;
	out.st.namelen = stbuf->f_namemax;

	return ifuse_reply(req, &out, sizeof(out), 0);
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
	struct fuse_open_out out;

	memset(&out, 0, sizeof(out));
	out.fh = ffi->fh;
	out.open_flags = ffi->flags;

	return ifuse_reply(req, &out, sizeof(out), 0);
}
DEF(fuse_reply_open);

void
fuse_reply_none(fuse_req_t req)
{
	/* no-op */
}
DEF(fuse_reply_none);

size_t
fuse_add_direntry(fuse_req_t req, char *buf, const size_t bufsize,
     const char *name, const struct stat *stbuf, off_t off)
{
	struct fuse_dirent *dir;
	size_t namelen;
	size_t len;

	if (name == NULL)
		return (0);

	namelen = strlen(name);
	len = FUSE_DIRENT_ALIGN(FUSE_NAME_OFFSET + namelen);

	/* NULL buf is used to request size to be calculated */
	if (buf == NULL || stbuf == NULL || req == NULL)
		return (len);

	/* buffer is full */
	if (bufsize < len)
		return (len);

	dir = (struct fuse_dirent *)buf;
	memset(dir, 0, len);
	dir->ino = stbuf->st_ino;
	dir->type = IFTODT(stbuf->st_mode);
	dir->off = off;
	dir->namelen = namelen;
	/* name is not NUL-terminated */
	memcpy(dir->name, name, namelen);

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

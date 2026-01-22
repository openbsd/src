/* $OpenBSD: fuse_lowlevel.h,v 1.2 2026/01/22 11:53:31 helg Exp $ */
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

#ifndef _FUSE_LOWLEVEL_H_
#define _FUSE_LOWLEVEL_H_

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 26
#endif

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/uio.h>

#include "fuse_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ino_t fuse_ino_t;

typedef struct fuse_req *fuse_req_t;

struct fuse_entry_param {
	fuse_ino_t	ino;
	unsigned long	generation;
	struct stat 	attr;
	double		attr_timeout;
	double		entry_timeout;
};

struct fuse_ctx {
        uid_t           uid;
        gid_t           gid;
        pid_t           pid;
        mode_t          umask;
};

struct fuse_lowlevel_ops {
	void (*init)(void *, struct fuse_conn_info *);
	void (*destroy)(void *);
	void (*lookup)(fuse_req_t, fuse_ino_t, const char *);
	void (*forget)(fuse_req_t, fuse_ino_t, uint64_t);
	void (*getattr)(fuse_req_t, fuse_ino_t, struct fuse_file_info *);
	void (*setattr)(fuse_req_t, fuse_ino_t, struct stat *, int,
	    struct fuse_file_info *);
	void (*readlink)(fuse_req_t, fuse_ino_t);
	void (*mknod)(fuse_req_t, fuse_ino_t, const char *, mode_t, dev_t);
	void (*mkdir)(fuse_req_t, fuse_ino_t, const char *, mode_t);
	void (*unlink)(fuse_req_t, fuse_ino_t, const char *);
	void (*rmdir)(fuse_req_t, fuse_ino_t, const char *);
	void (*symlink)(fuse_req_t, const char *, fuse_ino_t, const char *);
	void (*rename)(fuse_req_t, fuse_ino_t, const char *, fuse_ino_t,
	    const char *);
	void (*link)(fuse_req_t, fuse_ino_t, fuse_ino_t, const char *);
	void (*open)(fuse_req_t, fuse_ino_t, struct fuse_file_info *);
	void (*read)(fuse_req_t, fuse_ino_t, size_t, off_t,
	    struct fuse_file_info *);
	void (*write)(fuse_req_t, fuse_ino_t, const char *, size_t, off_t,
	    struct fuse_file_info *);
	void (*flush)(fuse_req_t, fuse_ino_t, struct fuse_file_info *);
	void (*release)(fuse_req_t, fuse_ino_t, struct fuse_file_info *);
	void (*fsync)(fuse_req_t, fuse_ino_t, int, struct fuse_file_info *);
	void (*opendir)(fuse_req_t, fuse_ino_t, struct fuse_file_info *);
	void (*readdir)(fuse_req_t, fuse_ino_t, size_t, off_t,
	    struct fuse_file_info *);
	void (*releasedir)(fuse_req_t, fuse_ino_t, struct fuse_file_info *);
	void (*statfs)(fuse_req_t, fuse_ino_t);

	/*
	 * The following are not supported on OpenBSD but are included for
	 * compatibilty. Porters must take care that ports do not rely on
	 * these file system operations being called. In particular, if create
	 * is implemented but mknod is not. On OpenBSD, file creation results
	 * in mknod() and then open() being called instead.
	 */
	void (*access) (fuse_req_t req, fuse_ino_t ino, int mask);
	void (*create)(fuse_req_t, fuse_ino_t, const char *, mode_t,
	    struct fuse_file_info *);
	void (*bmap)(fuse_req_t, fuse_ino_t, size_t, uint64_t);
	void (*fsyncdir)(fuse_req_t, fuse_ino_t, int, struct fuse_file_info *);

	/* setxattr */
	/* getxattr */
	/* listxattr */
	/* removexattr */
	/* getlk */
	/* setlk */
	/* others... */
};


/*
 * Helper function for readdir operations
 */
size_t fuse_add_direntry(fuse_req_t, char *, const size_t, const char *,
    const struct stat *, off_t);

/*
 * FUSE Sesssion API Prototypes
 */
struct fuse_session *fuse_lowlevel_new(struct fuse_args *,
    const struct fuse_lowlevel_ops *, const size_t, void *);
int fuse_session_loop(struct fuse_session *);
int fuse_session_exited(const struct fuse_session *);
void fuse_session_exit(struct fuse_session *);
void fuse_session_add_chan(struct fuse_session *, struct fuse_chan *);
void fuse_session_remove_chan(struct fuse_chan *);
void fuse_session_destroy(struct fuse_session *);
void fuse_session_reset(struct fuse_session *);
void fuse_session_process(struct fuse_session *, const char *, size_t,
    struct fuse_chan *);


/*
 * FUSE Channel API Prototypes
 */
int fuse_chan_recv(struct fuse_chan **, char *, size_t);
int fuse_chan_send(struct fuse_chan *, const struct iovec *, size_t);
int fuse_chan_fd(struct fuse_chan *);

/*
 * API Prototypes to reply from a file system operation back to the kernel.
 */
int fuse_reply_err(fuse_req_t, int);
int fuse_reply_buf(fuse_req_t, const char *, off_t);
int fuse_reply_attr(fuse_req_t, const struct stat *, double);
int fuse_reply_open(fuse_req_t, const struct fuse_file_info *);
int fuse_reply_write(fuse_req_t, size_t);
int fuse_reply_entry(fuse_req_t, const struct fuse_entry_param *);
int fuse_reply_statfs(fuse_req_t, const struct statvfs *);
int fuse_reply_readlink(fuse_req_t, char *);
void fuse_reply_none(fuse_req_t);

/*
 * The following are unsupported but are included for compatibility.
 */
int fuse_reply_bmap(fuse_req_t, uint64_t);
int fuse_reply_create(fuse_req_t, const struct fuse_entry_param *,
     const struct fuse_file_info *);

/*
 * FUSE Request API Prototypes
 */
const struct fuse_ctx *fuse_req_ctx(fuse_req_t);
void *fuse_req_userdata(fuse_req_t);

/*
 * Bitmasks for setattr to indicate what to set.
 */
#define FUSE_SET_ATTR_MODE	(1 << 0)
#define FUSE_SET_ATTR_UID	(1 << 1)
#define FUSE_SET_ATTR_GID	(1 << 2)
#define FUSE_SET_ATTR_SIZE	(1 << 3)
#define FUSE_SET_ATTR_ATIME	(1 << 4)
#define FUSE_SET_ATTR_MTIME	(1 << 5)
#define FUSE_SET_ATTR_ATIME_NOW	(1 << 7)
#define FUSE_SET_ATTR_MTIME_NOW	(1 << 8)

#ifdef __cplusplus
}
#endif

#endif /* _FUSE_LOWLEVEL_H_ */

/* $OpenBSD: fuse_common.h,v 1.1 2025/12/08 06:37:04 helg Exp $ */
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

/*
 * This file contains definitions common to both the low and high-level FUSE
 * APIs.
 */

#if !defined(_FUSE_H_) && !defined(_FUSE_LOWLEVEL_H_)
#error "Never include <fuse_common.h> directly; use <fuse.h> or <fuse_lowlevel.h> instead."
#endif

#ifndef _FUSE_COMMON_H_
#define _FUSE_COMMON_H_

#if FUSE_USE_VERSION >= 26
#define FUSE_VERSION 26
#else
#error "Fuse version < 26 not supported"
#endif

#include <sys/types.h>
#include <stdint.h>

#include "fuse_opt.h"

#define FUSE_MAJOR_VERSION 2
#define FUSE_MINOR_VERSION 6

#ifdef __cplusplus
extern "C" {
#endif

struct fuse_file_info {
	int32_t		flags;		/* open(2) flags */
	uint32_t	fh_old;		/* old file handle */
	int32_t		writepage;
	uint32_t	direct_io	: 1;
	uint32_t	keep_cache	: 1;
	uint32_t	flush		: 1;
	uint32_t	nonseekable	: 1;
	uint32_t	__padd		: 27;
	uint32_t	flock_release	: 1;
	uint64_t	fh;		/* file handle */
	uint64_t	lock_owner;
};

/* unused but needed for gvfs compilation */
#define FUSE_CAP_ASYNC_READ     (1 << 0)
#define FUSE_CAP_POSIX_LOCKS    (1 << 1)
#define FUSE_CAP_ATOMIC_O_TRUNC (1 << 3)
#define FUSE_CAP_EXPORT_SUPPORT (1 << 4)
#define FUSE_CAP_BIG_WRITES     (1 << 5)
#define FUSE_CAP_DONT_MASK      (1 << 6)
#define FUSE_CAP_SPLICE_WRITE   (1 << 7)
#define FUSE_CAP_SPLICE_MOVE    (1 << 8)
#define FUSE_CAP_SPLICE_READ    (1 << 9)
#define FUSE_CAP_FLOCK_LOCKS    (1 << 10)
#define FUSE_CAP_IOCTL_DIR      (1 << 11)

struct fuse_conn_info {
	uint32_t	proto_major;
	uint32_t	proto_minor;
	uint32_t	async_read;
	uint32_t	max_write;
	uint32_t	max_readahead;
	uint32_t	capable;
	uint32_t	want;
	uint32_t	max_background;
	uint32_t	congestion_threshold;
	uint32_t	reserved[23];
};

struct fuse_chan;
struct fuse_args;
struct fuse_session;

/*
 * API prototypes
 */
int fuse_version(void);
int fuse_parse_cmdline(struct fuse_args *, char **, int *, int *);
int fuse_daemonize(int);
int fuse_set_signal_handlers(struct fuse_session *);
void fuse_unmount(const char *, struct fuse_chan *);
void fuse_remove_signal_handlers(struct fuse_session *);
struct fuse_chan *fuse_mount(const char *, struct fuse_args *);

#ifdef __cplusplus
}
#endif

#endif /* _FUSE_COMMON_H_ */

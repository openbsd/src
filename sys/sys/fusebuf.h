/* $OpenBSD: fusebuf.h,v 1.17 2026/06/17 13:29:01 helg Exp $ */
/*
 * Copyright (c) 2013 Sylvestre Gallon
 * Copyright (c) 2013 Martin Pieuchot
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

#ifndef _SYS_FUSEBUF_H_
#define _SYS_FUSEBUF_H_

#include <sys/stdint.h>

/*
 * Maximum size of the read or write buffer sent from the kernel for VFS
 * syscalls: read, readdir, readlink, write.
 */
#define FUSEBUFMAXSIZE	(4096*1024)

/** Linux FUSE kernel interface major version we somewhat emulate. */
#define FUSE_KERNEL_VERSION 7

/** Linux FUSE kernel interface minor version we somewhat emulate. */
#define FUSE_KERNEL_MINOR_VERSION 19

/*
 * The root inode is the root of the mounted FUSE file system.
 * Also note that inode 0 can't be used for normal purposes.
 */
#define FUSE_ROOT_ID ((ino_t)1)

/*
 * An operation is issued by the kernel through fuse(4) when the
 * userland file system needs to execute an action (mkdir(2),
 * link(2), etc).
 *
 * F_databuf can be superior to FUSELEN for fusefs_read, fusefs_writes and
 * fusefs_readdir. If it is the case the transfer will be split in N
 * fusebuf with a changing offset in FD_io.
 *
 * When the userland file system answers to this operation it uses
 * the same ID (fh_uuid).
 */

/* flags needed by setattr */
#define FUSE_FATTR_MODE		(1 << 0)
#define FUSE_FATTR_UID		(1 << 1)
#define FUSE_FATTR_GID		(1 << 2)
#define FUSE_FATTR_SIZE		(1 << 3)
#define FUSE_FATTR_ATIME	(1 << 4)
#define FUSE_FATTR_MTIME	(1 << 5)

/* fusebuf types */
#define FUSE_LOOKUP	1
#define FUSE_GETATTR	3
#define FUSE_SETATTR	4
#define FUSE_READLINK	5
#define FUSE_SYMLINK	6
#define FUSE_MKNOD	8
#define FUSE_MKDIR	9
#define FUSE_UNLINK	10
#define FUSE_RMDIR	11
#define FUSE_RENAME	12
#define FUSE_LINK	13
#define FUSE_OPEN	14
#define FUSE_READ	15
#define FUSE_WRITE	16
#define FUSE_STATFS	17
#define FUSE_RELEASE	18
#define FUSE_FSYNC	20
#define FUSE_FLUSH	25
#define FUSE_INIT	26
#define FUSE_OPENDIR	27
#define FUSE_READDIR	28
#define FUSE_RELEASEDIR	29
#define FUSE_DESTROY	38
#define FUSE_FORGET	2

struct fuse_attr {
	uint64_t	ino;
	uint64_t	size;
	uint64_t	blocks;
	uint64_t	atime;
	uint64_t	mtime;
	uint64_t	ctime;
	uint32_t	atimensec;
	uint32_t	mtimensec;
	uint32_t	ctimensec;
	uint32_t	mode;
	uint32_t	nlink;
	uint32_t	uid;
	uint32_t	gid;
	uint32_t	rdev;
	uint32_t	blksize;
	uint32_t	padding;
};


struct fuse_entry_out {
	uint64_t	nodeid;			/* Inode number */
	uint64_t	generation;		/* Not implemented */
	uint64_t	entry_valid;		/* Not implemented */
	uint64_t	attr_valid;		/* Not implemented */
	uint32_t	entry_valid_nsec;	/* Not implemented */
	uint32_t	attr_valid_nsec;	/* Not implemented */
	struct		fuse_attr attr;
};

struct fuse_forget_in {
	uint64_t	nlookup;
};

struct fuse_getattr_in {
	uint32_t	getattr_flags;		/* Not implemented */
	uint32_t	dummy;
	uint64_t	fh;			/* Not implemented */
};

struct fuse_attr_out {
	uint64_t	attr_valid;		/* Not implemented */
	uint32_t	attr_valid_nsec;	/* Not implemented */
	uint32_t	dummy;
	struct		fuse_attr attr;
};

struct fuse_mknod_in {
	uint32_t	mode;
	uint32_t	rdev;
	uint32_t	umask;
	uint32_t	padding;
};

struct fuse_mkdir_in {
	uint32_t	mode;
	uint32_t	umask;
};

struct fuse_rename_in {
	uint64_t	newdir;
};

struct fuse_link_in {
	uint64_t	oldnodeid;
};

struct fuse_setattr_in {
	uint32_t	valid;
	uint32_t	padding;
	uint64_t	fh;
	uint64_t	size;
	uint64_t	lock_owner;
	uint64_t	atime;
	uint64_t	mtime;
	uint64_t	unused2;
	uint32_t	atimensec;
	uint32_t	mtimensec;
	uint32_t	unused3;
	uint32_t	mode;
	uint32_t	unused4;
	uint32_t	uid;
	uint32_t	gid;
	uint32_t	unused5;
};

struct fuse_open_in {
	uint32_t	flags;
	uint32_t	unused;
};

struct fuse_open_out {
	uint64_t	fh;
	uint32_t	open_flags;
	uint32_t	padding;
};

struct fuse_release_in {
	uint64_t	fh;
	uint32_t	flags;
	uint32_t	release_flags;
	uint64_t	lock_owner;
};

struct fuse_flush_in {
	uint64_t	fh;
	uint32_t	unused;
	uint32_t	padding;
	uint64_t	lock_owner;
};

struct fuse_read_in {
	uint64_t	fh;
	uint64_t	offset;
	uint32_t	size;
	uint32_t	read_flags;
	uint64_t	lock_owner;
	uint32_t	flags;
	uint32_t	padding;
};

struct fuse_write_in {
	uint64_t	fh;
	uint64_t	offset;
	uint32_t	size;
	uint32_t	write_flags;
	uint64_t	lock_owner;
	uint32_t	flags;
	uint32_t	padding;
};

struct fuse_write_out {
	uint32_t	size;
	uint32_t	padding;
};

struct fuse_kstatfs {
	uint64_t	blocks;
	uint64_t	bfree;
	uint64_t	bavail;
	uint64_t	files;
	uint64_t	ffree;
	uint32_t	bsize;
	uint32_t	namelen;
	uint32_t	frsize;
	uint32_t	padding;
	uint32_t	spare[6];
};

struct fuse_statfs_out {
	struct		fuse_kstatfs st;
};

struct fuse_fsync_in {
	uint64_t	fh;
	uint32_t	fsync_flags;		//XXX
	uint32_t	padding;
};

struct fuse_init_in {
	uint32_t	major;
	uint32_t	minor;
	uint32_t	max_readahead;
	uint32_t	flags;
};

struct fuse_init_out {
	uint32_t	major;
	uint32_t	minor;
	uint32_t	max_readahead;		/* Not implemented */
	uint32_t	flags;			/* Not implemented */
	uint16_t   	max_background;		/* Not implemented */
	uint16_t   	congestion_threshold;	/* Not implemented */
	uint32_t	max_write;
};

struct fuse_in_header {
	uint32_t	len;
	uint32_t	opcode;
	uint64_t	unique;
	uint64_t	nodeid;
	uint32_t	uid;
	uint32_t	gid;
	uint32_t	pid;
	uint32_t	padding;
};

struct fuse_out_header {
	uint32_t	len;
	int32_t		error;
	uint64_t	unique;
};

struct fuse_dirent {
	uint64_t	ino;
	uint64_t	off;
	uint32_t	namelen;
	uint32_t	type;
	char		name[];			/* unterminated string */
};

#define FUSE_NAME_OFFSET offsetof(struct fuse_dirent, name)
#define FUSE_DIRENT_ALIGN(x) \
	(((x) + sizeof(uint64_t) - 1) & ~(sizeof(uint64_t) - 1))
#define FUSE_DIRENT_SIZE(d) \
	FUSE_DIRENT_ALIGN(FUSE_NAME_OFFSET + (d)->namelen)

#ifdef _KERNEL

/*
 * An operation is issued by the kernel through fuse(4) when the
 * userland file system needs to execute an action (mkdir(2),
 * link(2), etc).
 *
 * When the userland file system answers to this operation it uses
 * the same ID (fb_uuid).
 */
struct fusebuf {
	SIMPLEQ_ENTRY(fusebuf)	next;		/* next buffer in chain */
	struct fuse_in_header	hdr;
	int32_t			error;		/* error returned by daemon */
	size_t			op_in_len;	/* size of input */
	size_t			op_out_len;	/* size of output */
	uint8_t			op_out_buf;	/* whether to expect data */
	union {
		union {
			struct fuse_forget_in	forget;
			struct fuse_getattr_in	getattr;
			struct fuse_setattr_in	setattr;
			struct fuse_mknod_in	mknod;
			struct fuse_mkdir_in	mkdir;
			struct fuse_rename_in	rename;
			struct fuse_link_in	link;
			struct fuse_open_in	open;
			struct fuse_read_in	read;
			struct fuse_write_in	write;
			struct fuse_release_in	release;
			struct fuse_fsync_in	fsync;
			struct fuse_flush_in	flush;
			struct fuse_init_in	init;
		} in;
		union {
			struct fuse_entry_out	entry;
			struct fuse_attr_out	attr;
			struct fuse_open_out	open;
			struct fuse_write_out	write;
			struct fuse_statfs_out	statfs;
			struct fuse_init_out	init;
		} out;
	} op;
	uint64_t dat_len;
	uint8_t *dat;
};

#define fb_dat		dat
#define fb_next		next
#define fb_err		error
#define fb_len		dat_len
#define fb_type		hdr.opcode
#define fb_uuid		hdr.unique
#define fb_ino		hdr.nodeid
#define fb_tid		hdr.pid
#define fb_uid		hdr.uid
#define fb_gid		hdr.gid

/* fusebuf prototypes */
struct	fusebuf *fb_setup(size_t, ino_t, int, struct proc *);
int	fb_queue(dev_t, struct fusebuf *);
void	fb_delete(struct fusebuf *);

#else /* _KERNEL */

/*
 * Userland fusebuf more directly maps to the protocol binary layout for each
 * file operation request (input). This consists of a header followed by an
 * optional input struct with data immediately following either the header or
 * input struct.
 *
 * FUSE_LOOKUP:		hdr, 		dat (filename)
 * FUSE_GETATTR:	hdr, getattr
 * FUSE_SETATTR:	hdr, setattr
 * FUSE_READLINK:	hdr
 * FUSE_SYMLINK:	hdr, 		dat (source\0target)
 * FUSE_MKNOD:		hdr, mknod, 	dat (filename)
 * FUSE_MKDIR:		hdr, mkdir, 	dat (dirname)
 * FUSE_UNLINK:		hdr, 		dat (filename)
 * FUSE_RMDIR:		hdr, 		dat (dirname)
 * FUSE_RENAME:		hdr, rename, 	dat (oldname\0newname)
 * FUSE_LINK:		hdr, 		dat (source\0target)
 * FUSE_OPEN:		hdr, open
 * FUSE_READ:		hdr, read
 * FUSE_WRITE:		hdr, write, 	dat
 * FUSE_STATFS:		hdr, statfs
 * FUSE_RELEASE:	hdr, release
 * FUSE_FSYNC:		hdr, fsync
 * FUSE_FLUSH:		hdr, flush
 * FUSE_INIT:		hdr, init
 * FUSE_OPENDIR:	hdr, open
 * FUSE_READDIR:	hdr, read
 * FUSE_RELEASEDIR:	hdr, release
 * FUSE_DESTROY:	hdr
 * FUSE_FORGET:		hdr, forget
 */
struct fusebuf {
	struct fuse_in_header hdr;
	union {
		struct fuse_forget_in	forget;
		struct fuse_getattr_in	getattr;
		struct fuse_setattr_in	setattr;
		struct fuse_mknod_in	mknod;
		struct fuse_mkdir_in	mkdir;
		struct fuse_rename_in	rename;
		struct fuse_link_in	link;
		struct fuse_open_in	open;
		struct fuse_read_in	read;
		struct fuse_write_in	write;
		struct fuse_release_in	release;
		struct fuse_fsync_in	fsync;
		struct fuse_flush_in	flush;
		struct fuse_init_in	init;
	} in;
};

#define fb_type	hdr.opcode
#define fb_uuid	hdr.unique
#define fb_ino	hdr.nodeid
#define fb_tid	hdr.pid
#define fb_uid	hdr.uid
#define fb_gid	hdr.gid

/*
 * Macro to get the additional data for operations that have it.
 * The data starts immediately after the related input struct.
 */
#define fb_dat(in) (((char *)&(in)) + sizeof(in))

#endif /* _KERNEL */
#endif /* _SYS_FUSEBUF_H_ */

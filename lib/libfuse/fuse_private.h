/* $OpenBSD: fuse_private.h,v 1.11 2015/01/16 16:48:51 deraadt Exp $ */
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

#ifndef _FUSE_SUBR_H_
#define _FUSE_SUBR_H_

#include <sys/dirent.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <sys/vnode.h>
#include <sys/fusebuf.h>
#include <limits.h>

#include "fuse.h"

struct fuse_dirhandle;
struct fuse_args;

struct fuse_vnode {
	ino_t ino;
	ino_t parent;

	char path[NAME_MAX + 1];
	struct fuse_dirhandle *fd;

	SIMPLEQ_ENTRY(fuse_vnode) node; /* for dict */
};

SIMPLEQ_HEAD(fuse_vn_head, fuse_vnode);
SPLAY_HEAD(dict, dictentry);
SPLAY_HEAD(tree, treeentry);

struct fuse_session {
	void *args;
};

struct fuse_chan {
	char *dir;
	struct fuse_args *args;

	int fd;
	int dead;

	/* kqueue stuff */
	int kq;
	struct kevent event;
};

struct fuse_config {
	uid_t			uid;
	gid_t			gid;
	pid_t			pid;
	mode_t			umask;
	int			set_mode;
	int			set_uid;
	int			set_gid;
};

struct fuse_core_opt {
	char *mp;
};

struct fuse {
	struct fuse_chan	*fc;
	struct fuse_operations	op;

	int			compat;

	struct tree		vnode_tree;
	struct dict		name_tree;
	uint64_t		max_ino;
	void			*private_data;

	struct fuse_config	conf;
	struct fuse_session	se;
};

#define	FUSE_MAX_OPS	39
#define FUSE_ROOT_INO ((ino_t)1)

/* fuse_ops.c */
int	ifuse_exec_opcode(struct fuse *, struct fusebuf *);

/* fuse_subr.c */
struct fuse_vnode	*alloc_vn(struct fuse *, const char *, ino_t, ino_t);
struct fuse_vnode	*get_vn_by_name_and_parent(struct fuse *, uint8_t *,
    ino_t);
void			remove_vnode_from_name_tree(struct fuse *,
    struct fuse_vnode *);
int			set_vn(struct fuse *, struct fuse_vnode *);
char			*build_realname(struct fuse *, ino_t);

/* tree.c */
#define tree_init(t)	SPLAY_INIT((t))
#define tree_empty(t)	SPLAY_EMPTY((t))
int			tree_check(struct tree *, uint64_t);
void			*tree_set(struct tree *, uint64_t, void *);
void			*tree_get(struct tree *, uint64_t);;
void			*tree_pop(struct tree *, uint64_t);

/* dict.c */
int			dict_check(struct dict *, const char *);
void			*dict_set(struct dict *, const char *, void *);
void			*dict_get(struct dict *, const char *);;
void			*dict_pop(struct dict *, const char *);

#define FUSE_VERSION_PKG_INFO "2.8.0"
#define unused __attribute__ ((unused))

#endif /* _FUSE_SUBR_ */

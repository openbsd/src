/*	$OpenBSD: lfs_extern.h,v 1.7 2002/03/14 01:27:15 millert Exp $	*/
/*	$NetBSD: lfs_extern.h,v 1.5 1996/02/12 15:20:12 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)lfs_extern.h	8.6 (Berkeley) 5/8/95
 */

struct fid;
struct mount;
struct nameidata;
struct proc;
struct statfs;
struct timeval;
struct inode;
struct uio;
struct mbuf;
struct dinode;
struct buf;
struct vnode;
struct lfs;
struct segment;
struct ucred;

#include <ufs/ufs/dinode.h>

__BEGIN_DECLS
/* lfs_alloc.c */
int lfs_vcreate(struct mount *, ino_t, struct vnode **);


/* lfs_balloc.c */
int lfs_balloc(struct vnode *, int, u_long, ufs_daddr_t, struct buf **);

/* lfs_bio.c */
void lfs_flush(void);
int lfs_check(struct vnode *, ufs_daddr_t);

/* lfs_cksum.c */
u_long cksum(void *, size_t);

/* lfs_debug.c */
#ifdef DEBUG
void lfs_dump_super(struct lfs *);
void lfs_dump_dinode(struct dinode *);
#endif

/* lfs_inode.c */
void lfs_init(void);
struct dinode *lfs_ifind(struct lfs *, ino_t, struct dinode *);

/* lfs_segment.c */
int lfs_vflush(struct vnode *);
void lfs_writevnodes(struct lfs *, struct mount *, struct segment *, int);
int lfs_segwrite(struct mount *, int);
void lfs_writefile(struct lfs *, struct segment *, struct vnode *);
int lfs_writeinode(struct lfs *, struct segment *, struct inode *);
int lfs_gatherblock(struct segment *, struct buf *, int *);
void lfs_gather __P((struct lfs *, struct segment *, struct vnode *, int (*match )(struct lfs *, struct buf *)));
void lfs_updatemeta(struct segment *);
int lfs_initseg(struct lfs *);
void lfs_newseg(struct lfs *);
int lfs_writeseg(struct lfs *, struct segment *);
void lfs_writesuper(struct lfs *);
int lfs_match_data(struct lfs *, struct buf *);
int lfs_match_indir(struct lfs *, struct buf *);
int lfs_match_dindir(struct lfs *, struct buf *);
int lfs_match_tindir(struct lfs *, struct buf *);
struct buf *lfs_newbuf(struct vnode *, ufs_daddr_t, size_t);
void lfs_callback(struct buf *);
void lfs_supercallback(struct buf *);
void lfs_shellsort(struct buf **, ufs_daddr_t *, int);
int lfs_vref(struct vnode *);
void lfs_vunref(struct vnode *);

/* lfs_subr.c */
void lfs_seglock(struct lfs *, unsigned long);
void lfs_segunlock(struct lfs *);

/* lfs_syscalls.c */
int lfs_fastvget(struct mount *, ino_t, ufs_daddr_t, struct vnode **, struct dinode *);
struct buf *lfs_fakebuf(struct vnode *, int, size_t, caddr_t);

/* lfs_vfsops.c */
int lfs_mountroot(void);
int lfs_mount(struct mount *, const char *, void *, struct nameidata *, struct proc *);
int lfs_mountfs(struct vnode *, struct mount *, struct proc *);
int lfs_unmount(struct mount *, int, struct proc *);
int lfs_statfs(struct mount *, struct statfs *, struct proc *);
int lfs_sync(struct mount *, int, struct ucred *, struct proc *);
int lfs_vget(struct mount *, ino_t, struct vnode **);
int lfs_fhtovp(struct mount *, struct fid *, struct vnode **);
int lfs_vptofh(struct vnode *, struct fid *);
int lfs_sysctl(int *, u_int, void *, size_t *, void *, size_t,
		    struct proc *);

int lfs_valloc(void *);
int lfs_vfree(void *);
int lfs_bwrite(void *);
int lfs_update(void *);
int lfs_truncate(void *);
int lfs_blkatoff(void *);
int lfs_fsync(void *);
int lfs_symlink(void *);
int lfs_mknod(void *);
int lfs_create(void *);
int lfs_mkdir(void *);
int lfs_read(void *);
int lfs_remove(void *);
int lfs_rmdir(void *);
int lfs_link(void *);
int lfs_rename(void *);
int lfs_getattr(void *);
int lfs_close(void *);
int lfs_inactive(void *);
int lfs_reclaim(void *);
int lfs_write(void *);

__END_DECLS
extern int (**lfs_vnodeop_p)(void *);
extern int (**lfs_specop_p)(void *);
#ifdef FIFO
extern int (**lfs_fifoop_p)(void *);
#define LFS_FIFOOPS lfs_fifoop_p
#else
#define LFS_FIFOOPS NULL
#endif

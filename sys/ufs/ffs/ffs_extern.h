/*	$OpenBSD: ffs_extern.h,v 1.15 2001/11/27 05:27:12 art Exp $	*/
/*	$NetBSD: ffs_extern.h,v 1.4 1996/02/09 22:22:22 christos Exp $	*/

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
 *	@(#)ffs_extern.h	8.3 (Berkeley) 4/16/94
 */

#define FFS_CLUSTERREAD		1	/* cluster reading enabled */
#define FFS_CLUSTERWRITE	2	/* cluster writing enabled */
#define FFS_REALLOCBLKS		3	/* block reallocation enabled */
#define FFS_ASYNCFREE		4	/* asynchronous block freeing enabled */
#define	FFS_MAXID		5	/* number of valid ffs ids */

#define FFS_NAMES { \
	{ 0, 0 }, \
	{ "doclusterread", CTLTYPE_INT }, \
	{ "doclusterwrite", CTLTYPE_INT }, \
	{ "doreallocblks", CTLTYPE_INT }, \
	{ "doasyncfree", CTLTYPE_INT }, \
}


struct buf;
struct fid;
struct fs;
struct inode;
struct mount;
struct nameidata;
struct proc;
struct statfs;
struct timeval;
struct ucred;
struct ufsmount;
struct vfsconf;
struct uio;
struct vnode;
struct mbuf;
struct cg;
struct vop_vfree_args;

__BEGIN_DECLS

/* ffs_alloc.c */
int ffs_alloc __P((struct inode *, daddr_t, daddr_t , int, struct ucred *,
		   daddr_t *));
int ffs_realloccg __P((struct inode *, daddr_t, daddr_t, int, int ,
		       struct ucred *, struct buf **, daddr_t *));
int ffs_reallocblks __P((void *));
int ffs_inode_alloc(struct inode *, int, struct ucred *, struct vnode **);
int ffs_inode_free(struct inode *, ino_t, int);
int ffs_freefile(struct inode *, ino_t, int);

daddr_t ffs_blkpref __P((struct inode *, daddr_t, int, daddr_t *));
void ffs_blkfree __P((struct inode *, daddr_t, long));
void ffs_clusteracct __P((struct fs *, struct cg *, daddr_t, int));

/* ffs_balloc.c */
int ffs_balloc(struct inode *, off_t, int, struct ucred *, int, struct buf **);
int ffs_ballocn(void *);

/* ffs_inode.c */
int ffs_init __P((struct vfsconf *));
int ffs_update(struct inode *, struct timespec *, struct timespec *, int);
int ffs_truncate(struct inode *, off_t, int, struct ucred *);

/* ffs_subr.c */
int ffs_bufatoff(struct inode *, off_t, char **, struct buf **);
void ffs_fragacct __P((struct fs *, int, int32_t[], int));
#ifdef DIAGNOSTIC
void	ffs_checkoverlap __P((struct buf *, struct inode *));
#endif
int   ffs_isfreeblock __P((struct fs *, unsigned char *, daddr_t));
int ffs_isblock __P((struct fs *, unsigned char *, daddr_t));
void ffs_clrblock __P((struct fs *, u_char *, daddr_t));
void ffs_setblock __P((struct fs *, unsigned char *, daddr_t));

/* ffs_vfsops.c */
int ffs_mountroot __P((void));
int ffs_mount __P((struct mount *, const char *, void *, struct nameidata *,
		   struct proc *));
int ffs_reload __P((struct mount *, struct ucred *, struct proc *));
int ffs_mountfs __P((struct vnode *, struct mount *, struct proc *));
int ffs_oldfscompat __P((struct fs *));
int ffs_unmount __P((struct mount *, int, struct proc *));
int ffs_flushfiles __P((struct mount *, int, struct proc *));
int ffs_statfs __P((struct mount *, struct statfs *, struct proc *));
int ffs_sync __P((struct mount *, int, struct ucred *, struct proc *));
int ffs_vget __P((struct mount *, ino_t, struct vnode **));
int ffs_fhtovp __P((struct mount *, struct fid *, struct vnode **));
int ffs_vptofh __P((struct vnode *, struct fid *));
int ffs_sysctl __P((int *, u_int, void *, size_t *, void *, size_t,
		    struct proc *));
int ffs_sbupdate __P((struct ufsmount *, int));
int ffs_cgupdate __P((struct ufsmount *, int));

/* ffs_vnops.c */
int ffs_read __P((void *));
int ffs_write __P((void *));
int ffs_fsync __P((void *));
int ffs_reclaim __P((void *));
int ffs_size __P((void *));

/*
 * Soft dependency function prototypes.
 */

struct vop_vfree_args;
struct vop_fsync_args;

void  softdep_initialize __P((void));
int   softdep_process_worklist __P((struct mount *));
int   softdep_mount __P((struct vnode *, struct mount *, struct fs *,
          struct ucred *));
int   softdep_flushworklist __P((struct mount *, int *, struct proc *));
int   softdep_flushfiles __P((struct mount *, int, struct proc *));
void  softdep_update_inodeblock __P((struct inode *, struct buf *, int));
void  softdep_load_inodeblock __P((struct inode *));
void  softdep_freefile __P((struct vnode *, ino_t, int));
void  softdep_setup_freeblocks __P((struct inode *, off_t));
void  softdep_setup_inomapdep __P((struct buf *, struct inode *, ino_t));
void  softdep_setup_blkmapdep __P((struct buf *, struct fs *, ufs_daddr_t));
void  softdep_setup_allocdirect __P((struct inode *, ufs_lbn_t, ufs_daddr_t,
            ufs_daddr_t, long, long, struct buf *));
void  softdep_setup_allocindir_meta __P((struct buf *, struct inode *,
            struct buf *, int, ufs_daddr_t));
void  softdep_setup_allocindir_page __P((struct inode *, ufs_lbn_t,
            struct buf *, int, ufs_daddr_t, ufs_daddr_t, struct buf *));
void  softdep_fsync_mountdev __P((struct vnode *));
int   softdep_sync_metadata __P((struct vop_fsync_args *));
int   softdep_fsync __P((struct vnode *vp));
__END_DECLS

extern int (**ffs_vnodeop_p) __P((void *));
extern int (**ffs_specop_p) __P((void *));
#ifdef FIFO
extern int (**ffs_fifoop_p) __P((void *));
#define FFS_FIFOOPS ffs_fifoop_p
#else
#define FFS_FIFOOPS NULL
#endif

extern struct pool ffs_ino_pool;

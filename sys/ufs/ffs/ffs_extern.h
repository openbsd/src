/*	$OpenBSD: ffs_extern.h,v 1.2 1996/02/27 07:27:36 niklas Exp $	*/
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
struct uio;
struct vnode;
struct mbuf;
struct cg;

__BEGIN_DECLS

/* ffs_alloc.c */
int ffs_alloc __P((struct inode *, daddr_t, daddr_t , int, struct ucred *,
		   daddr_t *));
int ffs_realloccg __P((struct inode *, daddr_t, daddr_t, int, int ,
		       struct ucred *, struct buf **));
int ffs_reallocblks __P((void *));
int ffs_valloc __P((void *));
daddr_t ffs_blkpref __P((struct inode *, daddr_t, int, daddr_t *));
void ffs_blkfree __P((struct inode *, daddr_t, long));
int ffs_vfree __P((void *));
void ffs_clusteracct __P((struct fs *, struct cg *, daddr_t, int));

/* ffs_balloc.c */
int ffs_balloc __P((struct inode *, daddr_t, int, struct ucred *,
		    struct buf **, int));

/* ffs_inode.c */
void ffs_init __P((void));
int ffs_update __P((void *));
int ffs_truncate __P((void *));

/* ffs_subr.c */
int ffs_blkatoff __P((void *));
void ffs_fragacct __P((struct fs *, int, int32_t[], int));
#ifdef DIAGNOSTIC
void	ffs_checkoverlap __P((struct buf *, struct inode *));
#endif
int ffs_isblock __P((struct fs *, unsigned char *, daddr_t));
void ffs_clrblock __P((struct fs *, u_char *, daddr_t));
void ffs_setblock __P((struct fs *, unsigned char *, daddr_t));

/* ffs_vfsops.c */
int ffs_mountroot __P((void));
int ffs_mount __P((struct mount *, char *, caddr_t, struct nameidata *,
		   struct proc *));
int ffs_reload __P((struct mount *, struct ucred *, struct proc *));
int ffs_mountfs __P((struct vnode *, struct mount *, struct proc *));
int ffs_oldfscompat __P((struct fs *));
int ffs_unmount __P((struct mount *, int, struct proc *));
int ffs_flushfiles __P((struct mount *, int, struct proc *));
int ffs_statfs __P((struct mount *, struct statfs *, struct proc *));
int ffs_sync __P((struct mount *, int, struct ucred *, struct proc *));
int ffs_vget __P((struct mount *, ino_t, struct vnode **));
int ffs_fhtovp __P((struct mount *, struct fid *, struct mbuf *,
		    struct vnode **, int *, struct ucred **));
int ffs_vptofh __P((struct vnode *, struct fid *));
int ffs_sbupdate __P((struct ufsmount *, int));
int ffs_cgupdate __P((struct ufsmount *, int));

/* ffs_vnops.c */
int ffs_read __P((void *));
int ffs_write __P((void *));
int ffs_fsync __P((void *));
int ffs_reclaim __P((void *));
__END_DECLS

extern int (**ffs_vnodeop_p) __P((void *));
extern int (**ffs_specop_p) __P((void *));
#ifdef FIFO
extern int (**ffs_fifoop_p) __P((void *));
#define FFS_FIFOOPS ffs_fifoop_p
#else
#define FFS_FIFOOPS NULL
#endif

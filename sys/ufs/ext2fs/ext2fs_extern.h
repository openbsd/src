/*	$OpenBSD: ext2fs_extern.h,v 1.6 2000/02/07 04:57:18 assar Exp $	*/
/*	$NetBSD: ext2fs_extern.h,v 1.1 1997/06/11 09:33:55 bouyer Exp $	*/

/*-
 * Copyright (c) 1997 Manuel Bouyer.
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
 * Modified for ext2fs by Manuel Bouyer.
 */

struct buf;
struct fid;
struct m_ext2fs;
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
struct vfsconf;
struct mbuf;
struct componentname;

__BEGIN_DECLS

/* ext2fs_alloc.c */
int ext2fs_alloc __P((struct inode *, daddr_t, daddr_t , struct ucred *,
		   daddr_t *));
int ext2fs_realloccg __P((struct inode *, daddr_t, daddr_t, int, int ,
			  struct ucred *, struct buf **));
int ext2fs_reallocblks __P((void *));
int ext2fs_valloc __P((void *));
daddr_t ext2fs_blkpref __P((struct inode *, daddr_t, int, daddr_t *));
void ext2fs_blkfree __P((struct inode *, daddr_t));
int ext2fs_vfree __P((void *));

/* ext2fs_balloc.c */
int ext2fs_balloc __P((struct inode *, daddr_t, int, struct ucred *,
			struct buf **, int));

/* ext2fs_bmap.c */
int ext2fs_bmap __P((void *));

/* ext2fs_inode.c */
int ext2fs_init __P((struct vfsconf *));
int ext2fs_update __P((void *));
int ext2fs_truncate __P((void *));
int ext2fs_inactive __P((void *));

/* ext2fs_lookup.c */
int ext2fs_readdir __P((void *));
int ext2fs_lookup  __P((void *));
int ext2fs_direnter __P((struct inode *, struct vnode *,
									struct componentname *));
int ext2fs_dirremove __P((struct vnode *, struct componentname *));
int ext2fs_dirrewrite __P((struct inode *, struct inode *,
								struct componentname *));
int ext2fs_dirempty __P((struct inode *, ino_t, struct ucred *));
int ext2fs_checkpath __P((struct inode *, struct inode *, struct ucred *));

/* ext2fs_subr.c */
int ext2fs_blkatoff __P((void *));
void ext2fs_fragacct __P((struct m_ext2fs *, int, int32_t[], int));
#ifdef DIAGNOSTIC
void	ext2fs_checkoverlap __P((struct buf *, struct inode *));
#endif

/* ext2fs_vfsops.c */
int ext2fs_mountroot __P((void));
int ext2fs_mount __P((struct mount *, const char *, caddr_t,
		   struct nameidata *, struct proc *));
int ext2fs_reload __P((struct mount *, struct ucred *, struct proc *));
int ext2fs_mountfs __P((struct vnode *, struct mount *, struct proc *));
int ext2fs_unmount __P((struct mount *, int, struct proc *));
int ext2fs_flushfiles __P((struct mount *, int, struct proc *));
int ext2fs_statfs __P((struct mount *, struct statfs *, struct proc *));
int ext2fs_sync __P((struct mount *, int, struct ucred *, struct proc *));
int ext2fs_vget __P((struct mount *, ino_t, struct vnode **));
int ext2fs_fhtovp __P((struct mount *, struct fid *, struct vnode **));
int ext2fs_vptofh __P((struct vnode *, struct fid *));
int ext2fs_sbupdate __P((struct ufsmount *, int));
int ext2fs_cgupdate __P((struct ufsmount *, int));
int ext2fs_sysctl __P((int *, u_int, void *, size_t *, void *, size_t,
		       struct proc *));

/* ext2fs_readwrite.c */
int ext2fs_read __P((void *));
int ext2fs_write __P((void *));

/* ext2fs_vnops.c */
int ext2fs_create __P((void *));
int ext2fs_mknod __P((void *));
int ext2fs_open __P((void *));
int ext2fs_access __P((void *));
int ext2fs_getattr __P((void *));
int ext2fs_setattr __P((void *));
int ext2fs_remove __P((void *));
int ext2fs_link __P((void *));
int ext2fs_rename __P((void *));
int ext2fs_mkdir __P((void *));
int ext2fs_rmdir __P((void *));
int ext2fs_symlink __P((void *));
int ext2fs_readlink __P((void *));
int ext2fs_advlock __P((void *));
int ext2fs_vinit __P((struct mount *, int (**specops) __P((void *)),
                      int (**fifoops) __P((void *)), struct vnode **));
int ext2fs_makeinode __P((int, struct vnode *, struct vnode **,
                          struct componentname *cnp));
int ext2fs_fsync __P((void *));
int ext2fs_reclaim __P((void *));
__END_DECLS

#define IS_EXT2_VNODE(vp)   (vp->v_tag == VT_EXT2FS)

extern int (**ext2fs_vnodeop_p) __P((void *));
extern int (**ext2fs_specop_p) __P((void *));
#ifdef FIFO
extern int (**ext2fs_fifoop_p) __P((void *));
#define EXT2FS_FIFOOPS ext2fs_fifoop_p
#else
#define EXT2FS_FIFOOPS NULL
#endif

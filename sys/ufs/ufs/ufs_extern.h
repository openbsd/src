/*	$OpenBSD: ufs_extern.h,v 1.9 2000/02/07 04:57:19 assar Exp $	*/
/*	$NetBSD: ufs_extern.h,v 1.5 1996/02/09 22:36:03 christos Exp $	*/

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
 *	@(#)ufs_extern.h	8.6 (Berkeley) 8/10/94
 */

struct buf;
struct componentname;
struct direct;
struct disklabel;
struct dquot;
struct fid;
struct flock;
struct indir;
struct inode;
struct mbuf;
struct mount;
struct nameidata;
struct proc;
struct ucred;
struct ufs_args;
struct ufsmount;
struct uio;
struct vattr;
struct vfsconf;
struct vnode;

__BEGIN_DECLS
int	 ufs_access	__P((void *));
int	 ufs_advlock	__P((void *));
int	 ufs_bmap	__P((void *));
int	 ufs_close	__P((void *));
int	 ufs_create	__P((void *));
int	 ufs_getattr	__P((void *));
int	 ufs_inactive	__P((void *));
int	 ufs_ioctl	__P((void *));
int	 ufs_islocked	__P((void *));
#ifdef NFSSERVER
int	 lease_check	__P((void *));
#define	 ufs_lease_check lease_check
#else
#define	 ufs_lease_check ((int (*) __P((void *)))nullop)
#endif
int	 ufs_link	__P((void *));
int	 ufs_lock	__P((void *));
int	 ufs_lookup	__P((void *));
int	 ufs_mkdir	__P((void *));
int	 ufs_mknod	__P((void *));
int	 ufs_mmap	__P((void *));
int	 ufs_open	__P((void *));
int	 ufs_pathconf	__P((void *));
int	 ufs_print	__P((void *));
int	 ufs_readdir	__P((void *));
int	 ufs_readlink	__P((void *));
int	 ufs_remove	__P((void *));
int	 ufs_rename	__P((void *));
#define  ufs_revoke  vop_generic_revoke
int	 ufs_rmdir	__P((void *));
int	 ufs_seek	__P((void *));
int	 ufs_select	__P((void *));
int	 ufs_setattr	__P((void *));
int	 ufs_strategy	__P((void *));
int	 ufs_symlink	__P((void *));
int	 ufs_unlock	__P((void *));
int	 ufs_whiteout	__P((void *));
int	 ufsspec_close	__P((void *));
int	 ufsspec_read	__P((void *));
int	 ufsspec_write	__P((void *));

#ifdef FIFO
int	ufsfifo_read	__P((void *));
int	ufsfifo_write	__P((void *));
int	ufsfifo_close	__P((void *));
#endif

/* ufs_bmap.c */
int ufs_bmaparray __P((struct vnode *, daddr_t, daddr_t *, struct indir *,
		       int *, int *));
int ufs_getlbns __P((struct vnode *, daddr_t, struct indir *, int *));

/* ufs_ihash.c */
void ufs_ihashinit __P((void));
struct vnode *ufs_ihashlookup __P((dev_t, ino_t));
struct vnode *ufs_ihashget __P((dev_t, ino_t));
int ufs_ihashins __P((struct inode *));
void ufs_ihashrem __P((struct inode *));

/* ufs_inode.c */
int ufs_init __P((struct vfsconf *));
int ufs_reclaim __P((struct vnode *, struct proc *));

/* ufs_lookup.c */
void ufs_dirbad __P((struct inode *, doff_t, char *));
int ufs_dirbadentry __P((struct vnode *, struct direct *, int));
void ufs_makedirentry __P((struct inode *, struct componentname *,
			   struct direct *));
int ufs_direnter __P((struct vnode *, struct vnode *, struct direct *,
		      struct componentname *, struct buf *));
int ufs_dirremove __P((struct vnode *, struct inode *, int, int));
int ufs_dirrewrite __P((struct inode *, struct inode *,
		        ino_t, int, int));
int ufs_dirempty __P((struct inode *, ino_t, struct ucred *));
int ufs_checkpath __P((struct inode *, struct inode *, struct ucred *));

/* ufs_quota.c */
int getinoquota __P((struct inode *));
int chkdq __P((struct inode *, long, struct ucred *, int));
int chkdqchg __P((struct inode *, long, struct ucred *, int));
int chkiq __P((struct inode *, long, struct ucred *, int));
int chkiqchg __P((struct inode *, long, struct ucred *, int));
void chkdquot __P((struct inode *));
int quotaon __P((struct proc *, struct mount *, int, caddr_t));
int quotaoff __P((struct proc *, struct mount *, int));
int getquota __P((struct mount *, u_long, int, caddr_t));
int setquota __P((struct mount *, u_long, int, caddr_t));
int setuse __P((struct mount *, u_long, int, caddr_t));
int qsync __P((struct mount *));
int dqget __P((struct vnode *, u_long, struct ufsmount *, int,
	       struct dquot **));
void dqref __P((struct dquot *));
void dqrele __P((struct vnode *, struct dquot *));
int dqsync __P((struct vnode *, struct dquot *));
void dqflush __P((struct vnode *));

/* ufs_vfsops.c */
int ufs_start __P((struct mount *, int, struct proc *));
int ufs_root __P((struct mount *, struct vnode **));
int ufs_quotactl __P((struct mount *, int, uid_t, caddr_t, struct proc *));
int ufs_fhtovp __P((struct mount *, struct ufid *, struct vnode **));
int ufs_check_export __P((struct mount *, struct mbuf *, int *,
		struct ucred **));

/* ufs_vnops.c */
int ufs_vinit __P((struct mount *, int (**) __P((void *)),
		   int (**) __P((void *)), struct vnode **));
int ufs_makeinode __P((int, struct vnode *, struct vnode **,
		       struct componentname *));

 
/*
 * Soft dependency function prototypes.
 */
void  softdep_setup_directory_add __P((struct buf *, struct inode *, off_t,
          long, struct buf *));
void  softdep_change_directoryentry_offset __P((struct inode *, caddr_t,
          caddr_t, caddr_t, int));
void  softdep_setup_remove __P((struct buf *,struct inode *, struct inode *,
          int));
void  softdep_setup_directory_change __P((struct buf *, struct inode *,
          struct inode *, long, int));
void  softdep_increase_linkcnt __P((struct inode *));

__END_DECLS

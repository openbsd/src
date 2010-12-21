/*	$OpenBSD: ufs_extern.h,v 1.32 2010/12/21 20:14:44 thib Exp $	*/
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
 * 3. Neither the name of the University nor the names of its contributors
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

int	 ufs_access(void *);
int	 ufs_advlock(void *);
int	 ufs_bmap(void *);
int	 ufs_close(void *);
int	 ufs_create(void *);
int	 ufs_getattr(void *);
int	 ufs_inactive(void *);
int	 ufs_ioctl(void *);
int	 ufs_islocked(void *);
int	 ufs_link(void *);
int	 ufs_lock(void *);
int	 ufs_lookup(void *);
int	 ufs_mkdir(void *);
int	 ufs_mknod(void *);
int	 ufs_mmap(void *);
int	 ufs_open(void *);
int	 ufs_pathconf(void *);
int	 ufs_print(void *);
int	 ufs_readdir(void *);
int	 ufs_readlink(void *);
int	 ufs_remove(void *);
int	 ufs_rename(void *);
int	 ufs_rmdir(void *);
int	 ufs_poll(void *);
int	 ufs_kqfilter(void *);
int	 ufs_setattr(void *);
int	 ufs_strategy(void *);
int	 ufs_symlink(void *);
int	 ufs_unlock(void *);
int	 ufsspec_close(void *);
int	 ufsspec_read(void *);
int	 ufsspec_write(void *);

#ifdef FIFO
int	ufsfifo_read(void *);
int	ufsfifo_write(void *);
int	ufsfifo_close(void *);
#endif

/* ufs_bmap.c */
int ufs_bmaparray(struct vnode *, daddr64_t, daddr64_t *, struct indir *,
		       int *, int *);
int ufs_getlbns(struct vnode *, daddr64_t, struct indir *, int *);

/* ufs_ihash.c */
void ufs_ihashinit(void);
struct vnode *ufs_ihashlookup(dev_t, ino_t);
struct vnode *ufs_ihashget(dev_t, ino_t);
int ufs_ihashins(struct inode *);
void ufs_ihashrem(struct inode *);

/* ufs_inode.c */
int ufs_init(struct vfsconf *);
int ufs_reclaim(struct vnode *, struct proc *);

/* ufs_lookup.c */
void ufs_dirbad(struct inode *, doff_t, char *);
int ufs_dirbadentry(struct vnode *, struct direct *, int);
void ufs_makedirentry(struct inode *, struct componentname *,
			   struct direct *);
int ufs_direnter(struct vnode *, struct vnode *, struct direct *,
		      struct componentname *, struct buf *);
int ufs_dirremove(struct vnode *, struct inode *, int, int);
int ufs_dirrewrite(struct inode *, struct inode *,
		        ino_t, int, int);
int ufs_dirempty(struct inode *, ino_t, struct ucred *);
int ufs_checkpath(struct inode *, struct inode *, struct ucred *);

/* ufs_vfsops.c */
int ufs_start(struct mount *, int, struct proc *);
int ufs_root(struct mount *, struct vnode **);
int ufs_quotactl(struct mount *, int, uid_t, caddr_t, struct proc *);
int ufs_fhtovp(struct mount *, struct ufid *, struct vnode **);
int ufs_check_export(struct mount *, struct mbuf *, int *,
		struct ucred **);

/* ufs_vnops.c */
int ufs_vinit(struct mount *, struct vops *, struct vops *, struct vnode **);
int ufs_makeinode(int, struct vnode *, struct vnode **,
		  struct componentname *);

 
/*
 * Soft dependency function prototypes.
 */
int  softdep_setup_directory_add(struct buf *, struct inode *, off_t,
          long, struct buf *, int);
void  softdep_change_directoryentry_offset(struct inode *, caddr_t,
          caddr_t, caddr_t, int);
void  softdep_setup_remove(struct buf *,struct inode *, struct inode *,
          int);
void  softdep_setup_directory_change(struct buf *, struct inode *,
          struct inode *, long, int);
void  softdep_change_linkcnt(struct inode *, int);
int   softdep_slowdown(struct vnode *);

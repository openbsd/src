/*	$OpenBSD: vfs_syscalls.c,v 1.98 2002/10/02 21:56:30 nordin Exp $	*/
/*	$NetBSD: vfs_syscalls.c,v 1.71 1996/04/23 10:29:02 mycroft Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)vfs_syscalls.c	8.28 (Berkeley) 12/10/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/extattr.h>

#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>
#include <sys/sysctl.h>

extern int suid_clear;
int	usermount = 0;		/* sysctl: by default, users may not mount */

static int change_dir(struct nameidata *, struct proc *);

void checkdirs(struct vnode *);

/*
 * Redirection info so we don't have to include the union fs routines in
 * the kernel directly.  This way, we can build unionfs as an LKM.  The
 * pointer gets filled in later, when we modload the LKM, or when the
 * compiled-in unionfs code gets initialized.  For now, we just set
 * it to a stub routine.
 */

int (*union_check_p)(struct proc *, struct vnode **,
    struct file *, struct uio, int *) = NULL;

/*
 * Virtual File System System Calls
 */

/*
 * Mount a file system.
 */
/* ARGSUSED */
int
sys_mount(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_mount_args /* {
		syscallarg(char *) type;
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(void *) data;
	} */ *uap = v;
	register struct vnode *vp;
	register struct mount *mp;
	int error, flag = 0;
#ifdef COMPAT_43
	u_long fstypenum = 0;
#endif
	char fstypename[MFSNAMELEN];
	char fspath[MNAMELEN];
	struct vattr va;
	struct nameidata nd;
	struct vfsconf *vfsp;
	struct timeval tv;

	if (usermount == 0 && (error = suser(p->p_ucred, &p->p_acflag)))
		return (error);

	/*
	 * Mount points must fit in MNAMELEN, not MAXPATHLEN.
	 */
	error = copyinstr(SCARG(uap, path), fspath, MNAMELEN, NULL);
	if (error)
		return(error);

	/*
	 * Get vnode to be covered
	 */
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, fspath, p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	if (SCARG(uap, flags) & MNT_UPDATE) {
		if ((vp->v_flag & VROOT) == 0) {
			vput(vp);
			return (EINVAL);
		}
		mp = vp->v_mount;
		flag = mp->mnt_flag;
		/*
		 * We only allow the filesystem to be reloaded if it
		 * is currently mounted read-only.
		 */
		if ((SCARG(uap, flags) & MNT_RELOAD) &&
		    ((mp->mnt_flag & MNT_RDONLY) == 0)) {
			vput(vp);
			return (EOPNOTSUPP);	/* Needs translation */
		}
		mp->mnt_flag |=
		    SCARG(uap, flags) & (MNT_RELOAD | MNT_UPDATE);
		/*
		 * Only root, or the user that did the original mount is
		 * permitted to update it.
		 */
		if (mp->mnt_stat.f_owner != p->p_ucred->cr_uid &&
		    (error = suser(p->p_ucred, &p->p_acflag))) {
			vput(vp);
			return (error);
		}
		/*
		 * Do not allow NFS export by non-root users. Silently
		 * enforce MNT_NOSUID and MNT_NODEV for non-root users.
		 */
		if (p->p_ucred->cr_uid != 0) {
			if (SCARG(uap, flags) & MNT_EXPORTED) {
				vput(vp);
				return (EPERM);
			}
			SCARG(uap, flags) |= MNT_NOSUID | MNT_NODEV;
		}
		if ((error = vfs_busy(mp, LK_NOWAIT, 0, p)) != 0) {
			vput(vp);
			return (error);
		}
		VOP_UNLOCK(vp, 0, p);
		goto update;
	}
	/*
	 * If the user is not root, ensure that they own the directory
	 * onto which we are attempting to mount.
	 */
	if ((error = VOP_GETATTR(vp, &va, p->p_ucred, p)) ||
	    (va.va_uid != p->p_ucred->cr_uid &&
	    (error = suser(p->p_ucred, &p->p_acflag)))) {
		vput(vp);
		return (error);
	}
	/*
	 * Do not allow NFS export by non-root users. Silently
	 * enforce MNT_NOSUID and MNT_NODEV for non-root users.
	 */
	if (p->p_ucred->cr_uid != 0) {
		if (SCARG(uap, flags) & MNT_EXPORTED) {
			vput(vp);
			return (EPERM);
		}
		SCARG(uap, flags) |= MNT_NOSUID | MNT_NODEV;
	}
	if ((error = vinvalbuf(vp, V_SAVE, p->p_ucred, p, 0, 0)) != 0)
		return (error);
	if (vp->v_type != VDIR) {
		vput(vp);
		return (ENOTDIR);
	}
	error = copyinstr(SCARG(uap, type), fstypename, MFSNAMELEN, NULL);
	if (error) {
#ifdef COMPAT_43
		/*
		 * Historically filesystem types were identified by number.
		 * If we get an integer for the filesystem type instead of a
		 * string, we check to see if it matches one of the historic
		 * filesystem types.
		 */
		fstypenum = (u_long)SCARG(uap, type);

		for (vfsp = vfsconf; vfsp; vfsp = vfsp->vfc_next)
			if (vfsp->vfc_typenum == fstypenum)
				break;
		if (vfsp == NULL) {
			vput(vp);
			return (ENODEV);
		}
		strncpy(fstypename, vfsp->vfc_name, MFSNAMELEN);

#else
		vput(vp);
		return (error);
#endif
	}
	for (vfsp = vfsconf; vfsp; vfsp = vfsp->vfc_next) {
		if (!strcmp(vfsp->vfc_name, fstypename))
			break;
	}

	if (vfsp == NULL) {
		vput(vp);
		return (EOPNOTSUPP);
	}

	if (vp->v_mountedhere != NULL) {
		vput(vp);
		return (EBUSY);
	}

	/*
	 * Allocate and initialize the file system.
	 */
	mp = (struct mount *)malloc((u_long)sizeof(struct mount),
		M_MOUNT, M_WAITOK);
	bzero((char *)mp, (u_long)sizeof(struct mount));
	lockinit(&mp->mnt_lock, PVFS, "vfslock", 0, 0);
	/* This error never happens, but it makes auditing easier */
	if ((error = vfs_busy(mp, LK_NOWAIT, 0, p)))
		return (error);
	mp->mnt_op = vfsp->vfc_vfsops;
	mp->mnt_vfc = vfsp;
	mp->mnt_flag |= (vfsp->vfc_flags & MNT_VISFLAGMASK);
	strncpy(mp->mnt_stat.f_fstypename, vfsp->vfc_name, MFSNAMELEN);
	mp->mnt_vnodecovered = vp;
	mp->mnt_stat.f_owner = p->p_ucred->cr_uid;
update:
	/*
	 * Set the mount level flags.
	 */
	if (SCARG(uap, flags) & MNT_RDONLY)
		mp->mnt_flag |= MNT_RDONLY;
	else if (mp->mnt_flag & MNT_RDONLY)
		mp->mnt_flag |= MNT_WANTRDWR;
	mp->mnt_flag &=~ (MNT_NOSUID | MNT_NOEXEC | MNT_NODEV |
	    MNT_SYNCHRONOUS | MNT_UNION | MNT_ASYNC | MNT_SOFTDEP |
	    MNT_NOATIME | MNT_FORCE);
	mp->mnt_flag |= SCARG(uap, flags) & (MNT_NOSUID | MNT_NOEXEC |
	    MNT_NODEV | MNT_SYNCHRONOUS | MNT_UNION | MNT_ASYNC |
	    MNT_SOFTDEP | MNT_NOATIME | MNT_FORCE);
	/*
	 * Mount the filesystem.
	 */
	error = VFS_MOUNT(mp, SCARG(uap, path), SCARG(uap, data), &nd, p);
	if (!error) {
		microtime(&tv);
		mp->mnt_stat.f_ctime = tv.tv_sec;
	}
	if (mp->mnt_flag & MNT_UPDATE) {
		vrele(vp);
		if (mp->mnt_flag & MNT_WANTRDWR)
			mp->mnt_flag &= ~MNT_RDONLY;
		mp->mnt_flag &=~
		    (MNT_UPDATE | MNT_RELOAD | MNT_FORCE | MNT_WANTRDWR);
		if (error)
			mp->mnt_flag = flag;

 		if ((mp->mnt_flag & MNT_RDONLY) == 0) {
 			if (mp->mnt_syncer == NULL)
 				error = vfs_allocate_syncvnode(mp);
 		} else {
 			if (mp->mnt_syncer != NULL)
 				vgone(mp->mnt_syncer);
 			mp->mnt_syncer = NULL;
 		}

		vfs_unbusy(mp, p);
		return (error);
	}

	vp->v_mountedhere = mp;

	/*
	 * Put the new filesystem on the mount list after root.
	 */
	cache_purge(vp);
	if (!error) {
		vfsp->vfc_refcount++;
		simple_lock(&mountlist_slock);
		CIRCLEQ_INSERT_TAIL(&mountlist, mp, mnt_list);
		simple_unlock(&mountlist_slock);
		checkdirs(vp);
		VOP_UNLOCK(vp, 0, p);
 		if ((mp->mnt_flag & MNT_RDONLY) == 0)
 			error = vfs_allocate_syncvnode(mp);
		vfs_unbusy(mp, p);
		(void) VFS_STATFS(mp, &mp->mnt_stat, p);
		if ((error = VFS_START(mp, 0, p)) != 0)
			vrele(vp);
	} else {
		mp->mnt_vnodecovered->v_mountedhere = (struct mount *)0;
		vfs_unbusy(mp, p);
		free((caddr_t)mp, M_MOUNT);
		vput(vp);
	}
	return (error);
}

/*
 * Scan all active processes to see if any of them have a current
 * or root directory onto which the new filesystem has just been
 * mounted. If so, replace them with the new mount point.
 */
void
checkdirs(olddp)
	struct vnode *olddp;
{
	struct filedesc *fdp;
	struct vnode *newdp;
	struct proc *p;

	if (olddp->v_usecount == 1)
		return;
	if (VFS_ROOT(olddp->v_mountedhere, &newdp))
		panic("mount: lost mount");
	for (p = LIST_FIRST(&allproc); p != 0; p = LIST_NEXT(p, p_list)) {
		/*
		 * XXX - we have a race with fork here. We should probably
		 *       check if the process is SIDL before we fiddle with it.
		 */
		fdp = p->p_fd;
		if (fdp->fd_cdir == olddp) {
			vrele(fdp->fd_cdir);
			VREF(newdp);
			fdp->fd_cdir = newdp;
		}
		if (fdp->fd_rdir == olddp) {
			vrele(fdp->fd_rdir);
			VREF(newdp);
			fdp->fd_rdir = newdp;
		}
	}
	if (rootvnode == olddp) {
		vrele(rootvnode);
		VREF(newdp);
		rootvnode = newdp;
	}
	vput(newdp);
}

/*
 * Unmount a file system.
 *
 * Note: unmount takes a path to the vnode mounted on as argument,
 * not special file (as before).
 */
/* ARGSUSED */
int
sys_unmount(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_unmount_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
	} */ *uap = v;
	register struct vnode *vp;
	struct mount *mp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	mp = vp->v_mount;

	/*
	 * Only root, or the user that did the original mount is
	 * permitted to unmount this filesystem.
	 */
	if ((mp->mnt_stat.f_owner != p->p_ucred->cr_uid) &&
	    (error = suser(p->p_ucred, &p->p_acflag))) {
		vput(vp);
		return (error);
	}

	/*
	 * Don't allow unmounting the root file system.
	 */
	if (mp->mnt_flag & MNT_ROOTFS) {
		vput(vp);
		return (EINVAL);
	}

	/*
	 * Must be the root of the filesystem
	 */
	if ((vp->v_flag & VROOT) == 0) {
		vput(vp);
		return (EINVAL);
	}
	vput(vp);

	if (vfs_busy(mp, LK_EXCLUSIVE, NULL, p))
		return (EBUSY);

	return (dounmount(mp, SCARG(uap, flags), p));
}

/*
 * Do the actual file system unmount.
 */
int
dounmount(struct mount *mp, int flags, struct proc *p)
{
	struct vnode *coveredvp;
	int error;

 	mp->mnt_flag &=~ MNT_ASYNC;
 	cache_purgevfs(mp);	/* remove cache entries for this file sys */
 	if (mp->mnt_syncer != NULL)
 		vgone(mp->mnt_syncer);
	if (((mp->mnt_flag & MNT_RDONLY) ||
	    (error = VFS_SYNC(mp, MNT_WAIT, p->p_ucred, p)) == 0) ||
 	    (flags & MNT_FORCE))
 		error = VFS_UNMOUNT(mp, flags, p);
	simple_lock(&mountlist_slock);
 	if (error) {
 		if ((mp->mnt_flag & MNT_RDONLY) == 0 && mp->mnt_syncer == NULL)
 			(void) vfs_allocate_syncvnode(mp);
		lockmgr(&mp->mnt_lock, LK_RELEASE | LK_INTERLOCK,
		    &mountlist_slock, p);
		return (error);
	}
	CIRCLEQ_REMOVE(&mountlist, mp, mnt_list);
	if ((coveredvp = mp->mnt_vnodecovered) != NULLVP) {
		coveredvp->v_mountedhere = (struct mount *)0;
 		vrele(coveredvp);
 	}
	mp->mnt_vfc->vfc_refcount--;
	if (mp->mnt_vnodelist.lh_first != NULL)
		panic("unmount: dangling vnode");
	lockmgr(&mp->mnt_lock, LK_RELEASE | LK_INTERLOCK, &mountlist_slock, p);
	free((caddr_t)mp, M_MOUNT);
	return (0);
}

/*
 * Sync each mounted filesystem.
 */
#ifdef DEBUG
int syncprt = 0;
struct ctldebug debug0 = { "syncprt", &syncprt };
#endif

/* ARGSUSED */
int
sys_sync(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct mount *mp, *nmp;
	int asyncflag;

	simple_lock(&mountlist_slock);
	for (mp = CIRCLEQ_LAST(&mountlist); mp != CIRCLEQ_END(&mountlist);
	    mp = nmp) {
		if (vfs_busy(mp, LK_NOWAIT, &mountlist_slock, p)) {
			nmp = CIRCLEQ_PREV(mp, mnt_list);
			continue;
		}
		if ((mp->mnt_flag & MNT_RDONLY) == 0) {
			asyncflag = mp->mnt_flag & MNT_ASYNC;
			mp->mnt_flag &= ~MNT_ASYNC;
			uvm_vnp_sync(mp);
			VFS_SYNC(mp, MNT_NOWAIT, p->p_ucred, p);
			if (asyncflag)
				mp->mnt_flag |= MNT_ASYNC;
		}
		simple_lock(&mountlist_slock);
		nmp = CIRCLEQ_PREV(mp, mnt_list);
		vfs_unbusy(mp, p);
	}
	simple_unlock(&mountlist_slock);

#ifdef DEBUG
	if (syncprt)
		vfs_bufstats();
#endif /* DEBUG */
	return (0);
}

/*
 * Change filesystem quotas.
 */
/* ARGSUSED */
int
sys_quotactl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_quotactl_args /* {
		syscallarg(char *) path;
		syscallarg(int) cmd;
		syscallarg(int) uid;
		syscallarg(caddr_t) arg;
	} */ *uap = v;
	register struct mount *mp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	mp = nd.ni_vp->v_mount;
	vrele(nd.ni_vp);
	return (VFS_QUOTACTL(mp, SCARG(uap, cmd), SCARG(uap, uid),
	    SCARG(uap, arg), p));
}

/*
 * Get filesystem statistics.
 */
/* ARGSUSED */
int
sys_statfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_statfs_args /* {
		syscallarg(char *) path;
		syscallarg(struct statfs *) buf;
	} */ *uap = v;
	register struct mount *mp;
	register struct statfs *sp;
	int error;
	struct nameidata nd;
	struct statfs sb;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	mp = nd.ni_vp->v_mount;
	sp = &mp->mnt_stat;
	vrele(nd.ni_vp);
	if ((error = VFS_STATFS(mp, sp, p)) != 0)
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
#if notyet
	if (mp->mnt_flag & MNT_SOFTDEP)
		sp->f_eflags = STATFS_SOFTUPD;
#endif
	/* Don't let non-root see filesystem id (for NFS security) */
	if (suser(p->p_ucred, &p->p_acflag)) {
		bcopy((caddr_t)sp, (caddr_t)&sb, sizeof(sb));
		sb.f_fsid.val[0] = sb.f_fsid.val[1] = 0;
		sp = &sb;
	}
	return (copyout((caddr_t)sp, (caddr_t)SCARG(uap, buf), sizeof(*sp)));
}

/*
 * Get filesystem statistics.
 */
/* ARGSUSED */
int
sys_fstatfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_fstatfs_args /* {
		syscallarg(int) fd;
		syscallarg(struct statfs *) buf;
	} */ *uap = v;
	struct file *fp;
	struct mount *mp;
	struct statfs *sp;
	int error;
	struct statfs sb;

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);
	mp = ((struct vnode *)fp->f_data)->v_mount;
	sp = &mp->mnt_stat;
	error = VFS_STATFS(mp, sp, p);
	FRELE(fp);
	if (error)
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
#if notyet
	if (mp->mnt_flag & MNT_SOFTDEP)
		sp->f_eflags = STATFS_SOFTUPD;
#endif
	/* Don't let non-root see filesystem id (for NFS security) */
	if (suser(p->p_ucred, &p->p_acflag)) {
		bcopy((caddr_t)sp, (caddr_t)&sb, sizeof(sb));
		sb.f_fsid.val[0] = sb.f_fsid.val[1] = 0;
		sp = &sb;
	}
	return (copyout((caddr_t)sp, (caddr_t)SCARG(uap, buf), sizeof(*sp)));
}

/*
 * Get statistics on all filesystems.
 */
int
sys_getfsstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_getfsstat_args /* {
		syscallarg(struct statfs *) buf;
		syscallarg(size_t) bufsize;
		syscallarg(int) flags;
	} */ *uap = v;
	register struct mount *mp, *nmp;
	register struct statfs *sp;
	struct statfs sb;
	caddr_t sfsp;
	size_t count, maxcount;
	int error, flags = SCARG(uap, flags);

	maxcount = SCARG(uap, bufsize) / sizeof(struct statfs);
	sfsp = (caddr_t)SCARG(uap, buf);
	count = 0;
	simple_lock(&mountlist_slock);
	for (mp = CIRCLEQ_FIRST(&mountlist); mp != CIRCLEQ_END(&mountlist);
	    mp = nmp) {
		if (vfs_busy(mp, LK_NOWAIT, &mountlist_slock, p)) {
			nmp = CIRCLEQ_NEXT(mp, mnt_list);
			continue;
		}
		if (sfsp && count < maxcount) {
			sp = &mp->mnt_stat;

			/* Refresh stats unless MNT_NOWAIT is specified */
			if (flags != MNT_NOWAIT &&
			    flags != MNT_LAZY &&
			    (flags == MNT_WAIT ||
			    flags == 0) &&
			    (error = VFS_STATFS(mp, sp, p))) {
				simple_lock(&mountlist_slock);
				nmp = CIRCLEQ_NEXT(mp, mnt_list);
				vfs_unbusy(mp, p);
 				continue;
			}

			sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
#if notyet
			if (mp->mnt_flag & MNT_SOFTDEP)
				sp->f_eflags = STATFS_SOFTUPD;
#endif
			if (suser(p->p_ucred, &p->p_acflag)) {
				bcopy((caddr_t)sp, (caddr_t)&sb, sizeof(sb));
				sb.f_fsid.val[0] = sb.f_fsid.val[1] = 0;
				sp = &sb;
			}
			error = copyout((caddr_t)sp, sfsp, sizeof(*sp));
			if (error) {
				vfs_unbusy(mp, p);
				return (error);
			}
			sfsp += sizeof(*sp);
		}
		count++;
		simple_lock(&mountlist_slock);
		nmp = CIRCLEQ_NEXT(mp, mnt_list);
		vfs_unbusy(mp, p);
	}
	simple_unlock(&mountlist_slock);
	if (sfsp && count > maxcount)
		*retval = maxcount;
	else
		*retval = count;
	return (0);
}

/*
 * Change current working directory to a given file descriptor.
 */
/* ARGSUSED */
int
sys_fchdir(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_fchdir_args /* {
		syscallarg(int) fd;
	} */ *uap = v;
	struct filedesc *fdp = p->p_fd;
	struct vnode *vp, *tdp;
	struct mount *mp;
	struct file *fp;
	int error;

	if ((error = getvnode(fdp, SCARG(uap, fd), &fp)) != 0)
		return (error);
	vp = (struct vnode *)fp->f_data;
	VREF(vp);
	FRELE(fp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(vp, VEXEC, p->p_ucred, p);

	while (!error && (mp = vp->v_mountedhere) != NULL) {
		if (vfs_busy(mp, 0, 0, p))
			continue;
		error = VFS_ROOT(mp, &tdp);
		vfs_unbusy(mp, p);
		if (error)
			break;
		vput(vp);
		vp = tdp;
	}
	if (error) {
		vput(vp);
		return (error);
	}
	VOP_UNLOCK(vp, 0, p);
	vrele(fdp->fd_cdir);
	fdp->fd_cdir = vp;
	return (0);
}

/*
 * Change current working directory (``.'').
 */
/* ARGSUSED */
int
sys_chdir(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_chdir_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	register struct filedesc *fdp = p->p_fd;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = change_dir(&nd, p)) != 0)
		return (error);
	vrele(fdp->fd_cdir);
	fdp->fd_cdir = nd.ni_vp;
	return (0);
}

/*
 * Change notion of root (``/'') directory.
 */
/* ARGSUSED */
int
sys_chroot(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_chroot_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	register struct filedesc *fdp = p->p_fd;
	int error;
	struct nameidata nd;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = change_dir(&nd, p)) != 0)
		return (error);
	if (fdp->fd_rdir != NULL) {
		/*
		 * A chroot() done inside a changed root environment does
		 * an automatic chdir to avoid the out-of-tree experience.
		 */
		vrele(fdp->fd_rdir);
		vrele(fdp->fd_cdir);
		VREF(nd.ni_vp);
		fdp->fd_cdir = nd.ni_vp;
	}
	fdp->fd_rdir = nd.ni_vp;
	return (0);
}

/*
 * Common routine for chroot and chdir.
 */
static int
change_dir(ndp, p)
	register struct nameidata *ndp;
	struct proc *p;
{
	struct vnode *vp;
	int error;

	if ((error = namei(ndp)) != 0)
		return (error);
	vp = ndp->ni_vp;
	if (vp->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(vp, VEXEC, p->p_ucred, p);
	if (error)
		vput(vp);
	else
		VOP_UNLOCK(vp, 0, p);
	return (error);
}

/*
 * Check permissions, allocate an open file structure,
 * and call the device open routine if any.
 */
int
sys_open(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_open_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(int) mode;
	} */ *uap = v;
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct vnode *vp;
	struct vattr vattr;
	int flags, cmode;
	int type, indx, error, localtrunc = 0;
	struct flock lf;
	struct nameidata nd;

	if ((error = falloc(p, &fp, &indx)) != 0)
		return (error);

	flags = FFLAGS(SCARG(uap, flags));
	cmode = ((SCARG(uap, mode) &~ fdp->fd_cmask) & ALLPERMS) &~ S_ISTXT;
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	p->p_dupfd = -indx - 1;			/* XXX check for fdopen */
	if ((flags & O_TRUNC) && (flags & (O_EXLOCK | O_SHLOCK))) {
		localtrunc = 1;
		flags &= ~O_TRUNC;	/* Must do truncate ourselves */
	}
	if ((error = vn_open(&nd, flags, cmode)) != 0) {
		if ((error == ENODEV || error == ENXIO) &&
		    p->p_dupfd >= 0 &&			/* XXX from fdopen */
		    (error =
			dupfdopen(fdp, indx, p->p_dupfd, flags, error)) == 0) {
			closef(fp, p);
			*retval = indx;
			return (0);
		}
		if (error == ERESTART)
			error = EINTR;
		fdremove(fdp, indx);
		closef(fp, p);
		return (error);
	}
	p->p_dupfd = 0;
	vp = nd.ni_vp;
	fp->f_flag = flags & FMASK;
	fp->f_type = DTYPE_VNODE;
	fp->f_ops = &vnops;
	fp->f_data = (caddr_t)vp;
	if (flags & (O_EXLOCK | O_SHLOCK)) {
		lf.l_whence = SEEK_SET;
		lf.l_start = 0;
		lf.l_len = 0;
		if (flags & O_EXLOCK)
			lf.l_type = F_WRLCK;
		else
			lf.l_type = F_RDLCK;
		type = F_FLOCK;
		if ((flags & FNONBLOCK) == 0)
			type |= F_WAIT;
		VOP_UNLOCK(vp, 0, p);
		error = VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf, type);
		if (error) {
			/* closef will vn_close the file for us. */
			fdremove(fdp, indx);
			closef(fp, p);
			return (error);
		}
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		fp->f_flag |= FHASLOCK;
	}
	if (localtrunc) {
		VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
		if ((fp->f_flag & FWRITE) == 0)
			error = EACCES;
		else if (vp->v_mount->mnt_flag & MNT_RDONLY)
			error = EROFS;
		else if (vp->v_type == VDIR)
			error = EISDIR;
		else if ((error = vn_writechk(vp)) == 0) {
			VATTR_NULL(&vattr);
			vattr.va_size = 0;
			error = VOP_SETATTR(vp, &vattr, fp->f_cred, p);
		}
		if (error) {
			VOP_UNLOCK(vp, 0, p);
			/* closef will close the file for us. */
			fdremove(fdp, indx);
			closef(fp, p);
			return (error);
		}
	}
	VOP_UNLOCK(vp, 0, p);
	*retval = indx;
	FILE_SET_MATURE(fp);
	return (0);
}

/*
 * Get file handle system call
 */
int
sys_getfh(p, v, retval)
	struct proc *p;
	register void *v;
	register_t *retval;
{
	register struct sys_getfh_args /* {
		syscallarg(char *) fname;
		syscallarg(fhandle_t *) fhp;
	} */ *uap = v;
	register struct vnode *vp;
	fhandle_t fh;
	int error;
	struct nameidata nd;

	/*
	 * Must be super user
	 */
	error = suser(p->p_ucred, &p->p_acflag);
	if (error)
		return (error);
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, fname), p);
	error = namei(&nd);
	if (error)
		return (error);
	vp = nd.ni_vp;
	bzero((caddr_t)&fh, sizeof(fh));
	fh.fh_fsid = vp->v_mount->mnt_stat.f_fsid;
	error = VFS_VPTOFH(vp, &fh.fh_fid);
	vput(vp);
	if (error)
		return (error);
	error = copyout((caddr_t)&fh, (caddr_t)SCARG(uap, fhp), sizeof (fh));
	return (error);
}

/*
 * Open a file given a file handle.
 *
 * Check permissions, allocate an open file structure,
 * and call the device open routine if any.
 */
int
sys_fhopen(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_fhopen_args /* {
		syscallarg(const fhandle_t *) fhp;
		syscallarg(int) flags;
	} */ *uap = v;
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct vnode *vp = NULL;
	struct mount *mp;
	struct ucred *cred = p->p_ucred;
	int flags;
	int type, indx, error=0;
	struct flock lf;
	struct vattr va;
	fhandle_t fh;

	/*
	 * Must be super user
	 */
	if ((error = suser(p->p_ucred, &p->p_acflag)))
		return (error);

	flags = FFLAGS(SCARG(uap, flags));
	if ((flags & (FREAD | FWRITE)) == 0)
		return (EINVAL);
	if ((flags & O_CREAT))
		return (EINVAL);

	if ((error = falloc(p, &fp, &indx)) != 0)
		return (error);

	if ((error = copyin(SCARG(uap, fhp), &fh, sizeof(fhandle_t))) != 0)
		goto bad;

	if ((mp = vfs_getvfs(&fh.fh_fsid)) == NULL) {
		error = ESTALE;
		goto bad;
	}

	if ((error = VFS_FHTOVP(mp, &fh.fh_fid, &vp)) != 0) {
		vp = NULL;	/* most likely unnecessary sanity for bad: */
		goto bad;
	}

	/* Now do an effective vn_open */

	if (vp->v_type == VSOCK) {
		error = EOPNOTSUPP;
		goto bad;
	}
	if (flags & FREAD) {
		if ((error = VOP_ACCESS(vp, VREAD, cred, p)) != 0)
			goto bad;
	}
	if (flags & (FWRITE | O_TRUNC)) {
		if (vp->v_type == VDIR) {
			error = EISDIR;
			goto bad;
		}
		if ((error = vn_writechk(vp)) != 0 ||
		    (error = VOP_ACCESS(vp, VWRITE, cred, p)) != 0)
			goto bad;
	}
	if (flags & O_TRUNC) {
		VOP_UNLOCK(vp, 0, p);				/* XXX */
		VOP_LEASE(vp, p, cred, LEASE_WRITE);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);	/* XXX */
		VATTR_NULL(&va);
		va.va_size = 0;
		if ((error = VOP_SETATTR(vp, &va, cred, p)) != 0)
			goto bad;
	}
	if ((error = VOP_OPEN(vp, flags, cred, p)) != 0)
		goto bad;
	if (flags & FWRITE)
		vp->v_writecount++;

	/* done with modified vn_open, now finish what sys_open does. */

	fp->f_flag = flags & FMASK;
	fp->f_type = DTYPE_VNODE;
	fp->f_ops = &vnops;
	fp->f_data = (caddr_t)vp;
	if (flags & (O_EXLOCK | O_SHLOCK)) {
		lf.l_whence = SEEK_SET;
		lf.l_start = 0;
		lf.l_len = 0;
		if (flags & O_EXLOCK)
			lf.l_type = F_WRLCK;
		else
			lf.l_type = F_RDLCK;
		type = F_FLOCK;
		if ((flags & FNONBLOCK) == 0)
			type |= F_WAIT;
		VOP_UNLOCK(vp, 0, p);
		error = VOP_ADVLOCK(vp, (caddr_t)fp, F_SETLK, &lf, type);
		if (error) {
			/* closef will vn_close the file for us. */
			fdremove(fdp, indx);
			closef(fp, p);
			return (error);
		}
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		fp->f_flag |= FHASLOCK;
	}
	VOP_UNLOCK(vp, 0, p);
	*retval = indx;
	FILE_SET_MATURE(fp);
	return (0);

bad:
	fdremove(fdp, indx);
	closef(fp, p);
	if (vp != NULL)
		vput(vp);
	return (error);
}

/* ARGSUSED */
int
sys_fhstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_fhstat_args /* {
		syscallarg(const fhandle_t *) fhp;
		syscallarg(struct stat *) sb;
	} */ *uap = v;
	struct stat sb;
	int error;
	fhandle_t fh;
	struct mount *mp;
	struct vnode *vp;

	/*
	 * Must be super user
	 */
	if ((error = suser(p->p_ucred, &p->p_acflag)))
		return (error);

	if ((error = copyin(SCARG(uap, fhp), &fh, sizeof(fhandle_t))) != 0)
		return (error);

	if ((mp = vfs_getvfs(&fh.fh_fsid)) == NULL)
		return (ESTALE);
	if ((error = VFS_FHTOVP(mp, &fh.fh_fid, &vp)))
		return (error);
	error = vn_stat(vp, &sb, p);
	vput(vp);
	if (error)
		return (error);
	error = copyout(&sb, SCARG(uap, sb), sizeof(sb));
	return (error);
}

/* ARGSUSED */
int
sys_fhstatfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_fhstatfs_args /*
		syscallarg(const fhandle_t *) fhp;
		syscallarg(struct statfs *) buf;
	} */ *uap = v;
	struct statfs sp;
	fhandle_t fh;
	struct mount *mp;
	struct vnode *vp;
	int error;

	/*
	 * Must be super user
	 */
	if ((error = suser(p->p_ucred, &p->p_acflag)))
		return (error);

	if ((error = copyin(SCARG(uap, fhp), &fh, sizeof(fhandle_t))) != 0)
		return (error);

	if ((mp = vfs_getvfs(&fh.fh_fsid)) == NULL)
		return (ESTALE);
	if ((error = VFS_FHTOVP(mp, &fh.fh_fid, &vp)))
		return (error);
	mp = vp->v_mount;
	vput(vp);
	if ((error = VFS_STATFS(mp, &sp, p)) != 0)
		return (error);
	sp.f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	return (copyout(&sp, SCARG(uap, buf), sizeof(sp)));
}

/*
 * Create a special file.
 */
/* ARGSUSED */
int
sys_mknod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_mknod_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
		syscallarg(int) dev;
	} */ *uap = v;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	int whiteout = 0;
	struct nameidata nd;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
	if (p->p_fd->fd_rdir)
		return (EINVAL);
	NDINIT(&nd, CREATE, LOCKPARENT, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	if (vp != NULL)
		error = EEXIST;
	else {
		VATTR_NULL(&vattr);
		vattr.va_mode = (SCARG(uap, mode) & ALLPERMS) &~ p->p_fd->fd_cmask;
		vattr.va_rdev = SCARG(uap, dev);
		whiteout = 0;

		switch (SCARG(uap, mode) & S_IFMT) {
		case S_IFMT:	/* used by badsect to flag bad sectors */
			vattr.va_type = VBAD;
			break;
		case S_IFCHR:
			vattr.va_type = VCHR;
			break;
		case S_IFBLK:
			vattr.va_type = VBLK;
			break;
		case S_IFWHT:
			whiteout = 1;
			break;
		default:
			error = EINVAL;
			break;
		}
	}
	if (!error) {
		VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
		if (whiteout) {
			error = VOP_WHITEOUT(nd.ni_dvp, &nd.ni_cnd, CREATE);
			if (error)
				VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
			vput(nd.ni_dvp);
		} else {
			error = VOP_MKNOD(nd.ni_dvp, &nd.ni_vp,
						&nd.ni_cnd, &vattr);
		}
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (vp)
			vrele(vp);
	}
	return (error);
}

/*
 * Create a named pipe.
 */
/* ARGSUSED */
int
sys_mkfifo(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#ifndef FIFO
	return (EOPNOTSUPP);
#else
	register struct sys_mkfifo_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap = v;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, CREATE, LOCKPARENT, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	if (nd.ni_vp != NULL) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(nd.ni_vp);
		return (EEXIST);
	}
	VATTR_NULL(&vattr);
	vattr.va_type = VFIFO;
	vattr.va_mode = (SCARG(uap, mode) & ALLPERMS) &~ p->p_fd->fd_cmask;
	VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	return (VOP_MKNOD(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr));
#endif /* FIFO */
}

/*
 * Make a hard file link.
 */
/* ARGSUSED */
int
sys_link(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_link_args /* {
		syscallarg(char *) path;
		syscallarg(char *) link;
	} */ *uap = v;
	register struct vnode *vp;
	struct nameidata nd;
	int error;
	int flags;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;

	flags = LOCKPARENT;
	if (vp->v_type == VDIR) {
		flags |= STRIPSLASHES;
	}

	NDINIT(&nd, CREATE, flags, UIO_USERSPACE, SCARG(uap, link), p);
	if ((error = namei(&nd)) != 0)
		goto out;
	if (nd.ni_vp) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(nd.ni_vp);
		error = EEXIST;
		goto out;
	}
	VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	error = VOP_LINK(nd.ni_dvp, vp, &nd.ni_cnd);
out:
	vrele(vp);
	return (error);
}

/*
 * Make a symbolic link.
 */
/* ARGSUSED */
int
sys_symlink(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_symlink_args /* {
		syscallarg(char *) path;
		syscallarg(char *) link;
	} */ *uap = v;
	struct vattr vattr;
	char *path;
	int error;
	struct nameidata nd;

	MALLOC(path, char *, MAXPATHLEN, M_NAMEI, M_WAITOK);
	error = copyinstr(SCARG(uap, path), path, MAXPATHLEN, NULL);
	if (error)
		goto out;
	NDINIT(&nd, CREATE, LOCKPARENT, UIO_USERSPACE, SCARG(uap, link), p);
	if ((error = namei(&nd)) != 0)
		goto out;
	if (nd.ni_vp) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(nd.ni_vp);
		error = EEXIST;
		goto out;
	}
	VATTR_NULL(&vattr);
	vattr.va_mode = ACCESSPERMS &~ p->p_fd->fd_cmask;
	VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	error = VOP_SYMLINK(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr, path);
out:
	FREE(path, M_NAMEI);
	return (error);
}

/*
 * Delete a whiteout from the filesystem.
 */
/* ARGSUSED */
int
sys_undelete(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_undelete_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	int error;
	struct nameidata nd;

	NDINIT(&nd, DELETE, LOCKPARENT|DOWHITEOUT, UIO_USERSPACE,
	    SCARG(uap, path), p);
	error = namei(&nd);
	if (error)
		return (error);

	if (nd.ni_vp != NULLVP || !(nd.ni_cnd.cn_flags & ISWHITEOUT)) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == nd.ni_vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		if (nd.ni_vp)
			vrele(nd.ni_vp);
		return (EEXIST);
	}

	VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	if ((error = VOP_WHITEOUT(nd.ni_dvp, &nd.ni_cnd, DELETE)) != 0)
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
	vput(nd.ni_dvp);
	return (error);
}

/*
 * Delete a name from the filesystem.
 */
/* ARGSUSED */
int
sys_unlink(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_unlink_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	register struct vnode *vp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, DELETE, LOCKPARENT | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;

	/*
	 * The root of a mounted filesystem cannot be deleted.
	 */
	if (vp->v_flag & VROOT) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vput(vp);
		error = EBUSY;
		goto out;
	}

	(void)uvm_vnp_uncache(vp);

	VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	error = VOP_REMOVE(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
out:
	return (error);
}

/*
 * Reposition read/write file offset.
 */
int
sys_lseek(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_lseek_args /* {
		syscallarg(int) fd;
		syscallarg(int) pad;
		syscallarg(off_t) offset;
		syscallarg(int) whence;
	} */ *uap = v;
	struct ucred *cred = p->p_ucred;
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct vattr vattr;
	struct vnode *vp;
	int error, special;

	if ((fp = fd_getfile(fdp, SCARG(uap, fd))) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE)
		return (ESPIPE);
	vp = (struct vnode *)fp->f_data;
	if (vp->v_type == VFIFO)
		return (ESPIPE);
	if (vp->v_type == VCHR)
		special = 1;
	else
		special = 0;
	switch (SCARG(uap, whence)) {
	case SEEK_CUR:
		if (!special && fp->f_offset + SCARG(uap, offset) < 0)
			return (EINVAL);
		fp->f_offset += SCARG(uap, offset);
		break;
	case SEEK_END:
		error = VOP_GETATTR((struct vnode *)fp->f_data, &vattr,
				    cred, p);
		if (error)
			return (error);
		if (!special && (off_t)vattr.va_size + SCARG(uap, offset) < 0)
			return (EINVAL);
		fp->f_offset = SCARG(uap, offset) + vattr.va_size;
		break;
	case SEEK_SET:
		if (!special && SCARG(uap, offset) < 0)
			return (EINVAL);
		fp->f_offset = SCARG(uap, offset);
		break;
	default:
		return (EINVAL);
	}
	*(off_t *)retval = fp->f_offset;
	return (0);
}

/*
 * Check access permissions.
 */
int
sys_access(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_access_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
	} */ *uap = v;
	register struct ucred *cred = p->p_ucred;
	register struct vnode *vp;
	int error, flags, t_gid, t_uid;
	struct nameidata nd;

	if (SCARG(uap, flags) & ~(R_OK | W_OK | X_OK))
		return (EINVAL);
	t_uid = cred->cr_uid;
	t_gid = cred->cr_gid;
	cred->cr_uid = p->p_cred->p_ruid;
	cred->cr_gid = p->p_cred->p_rgid;
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		goto out1;
	vp = nd.ni_vp;

	/* Flags == 0 means only check for existence. */
	if (SCARG(uap, flags)) {
		flags = 0;
		if (SCARG(uap, flags) & R_OK)
			flags |= VREAD;
		if (SCARG(uap, flags) & W_OK)
			flags |= VWRITE;
		if (SCARG(uap, flags) & X_OK)
			flags |= VEXEC;
		if ((flags & VWRITE) == 0 || (error = vn_writechk(vp)) == 0)
			error = VOP_ACCESS(vp, flags, cred, p);
	}
	vput(vp);
out1:
	cred->cr_uid = t_uid;
	cred->cr_gid = t_gid;
	return (error);
}

/*
 * Get file status; this version follows links.
 */
/* ARGSUSED */
int
sys_stat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_stat_args /* {
		syscallarg(char *) path;
		syscallarg(struct stat *) ub;
	} */ *uap = v;
	struct stat sb;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	error = vn_stat(nd.ni_vp, &sb, p);
	vput(nd.ni_vp);
	if (error)
		return (error);
	/* Don't let non-root see generation numbers (for NFS security) */
	if (suser(p->p_ucred, &p->p_acflag))
		sb.st_gen = 0;
	error = copyout((caddr_t)&sb, (caddr_t)SCARG(uap, ub), sizeof (sb));
	return (error);
}

/*
 * Get file status; this version does not follow links.
 */
/* ARGSUSED */
int
sys_lstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_lstat_args /* {
		syscallarg(char *) path;
		syscallarg(struct stat *) ub;
	} */ *uap = v;
	struct stat sb;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	error = vn_stat(nd.ni_vp, &sb, p);
	vput(nd.ni_vp);
	if (error)
		return (error);
	/* Don't let non-root see generation numbers (for NFS security) */
	if (suser(p->p_ucred, &p->p_acflag))
		sb.st_gen = 0;
	error = copyout((caddr_t)&sb, (caddr_t)SCARG(uap, ub), sizeof (sb));
	return (error);
}

/*
 * Get configurable pathname variables.
 */
/* ARGSUSED */
int
sys_pathconf(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_pathconf_args /* {
		syscallarg(char *) path;
		syscallarg(int) name;
	} */ *uap = v;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	error = VOP_PATHCONF(nd.ni_vp, SCARG(uap, name), retval);
	vput(nd.ni_vp);
	return (error);
}

/*
 * Return target name of a symbolic link.
 */
/* ARGSUSED */
int
sys_readlink(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_readlink_args /* {
		syscallarg(char *) path;
		syscallarg(char *) buf;
		syscallarg(size_t) count;
	} */ *uap = v;
	register struct vnode *vp;
	struct iovec aiov;
	struct uio auio;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	if (vp->v_type != VLNK)
		error = EINVAL;
	else {
		aiov.iov_base = SCARG(uap, buf);
		aiov.iov_len = SCARG(uap, count);
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		auio.uio_rw = UIO_READ;
		auio.uio_segflg = UIO_USERSPACE;
		auio.uio_procp = p;
		auio.uio_resid = SCARG(uap, count);
		error = VOP_READLINK(vp, &auio, p->p_ucred);
	}
	vput(vp);
	*retval = SCARG(uap, count) - auio.uio_resid;
	return (error);
}

/*
 * Change flags of a file given a path name.
 */
/* ARGSUSED */
int
sys_chflags(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_chflags_args /* {
		syscallarg(char *) path;
		syscallarg(unsigned int) flags;
	} */ *uap = v;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else if (SCARG(uap, flags) == VNOVAL)
		error = EINVAL;
	else {
		if (suser(p->p_ucred, &p->p_acflag)) {
			if ((error = VOP_GETATTR(vp, &vattr, p->p_ucred, p)) != 0)
				goto out;
			if (vattr.va_type == VCHR || vattr.va_type == VBLK) {
				error = EINVAL;
				goto out;
			}
		}
		VATTR_NULL(&vattr);
		vattr.va_flags = SCARG(uap, flags);
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
out:
	vput(vp);
	return (error);
}

/*
 * Change flags of a file given a file descriptor.
 */
/* ARGSUSED */
int
sys_fchflags(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_fchflags_args /* {
		syscallarg(int) fd;
		syscallarg(unsigned int) flags;
	} */ *uap = v;
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else if (SCARG(uap, flags) == VNOVAL)
		error = EINVAL;
	else {
		if (suser(p->p_ucred, &p->p_acflag)) {
			if ((error = VOP_GETATTR(vp, &vattr, p->p_ucred, p))
			    != 0)
				goto out;
			if (vattr.va_type == VCHR || vattr.va_type == VBLK) {
				error = EINVAL;
				goto out;
			}
		}
		VATTR_NULL(&vattr);
		vattr.va_flags = SCARG(uap, flags);
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
out:
	VOP_UNLOCK(vp, 0, p);
	FRELE(fp);
	return (error);
}

/*
 * Change mode of a file given path name.
 */
/* ARGSUSED */
int
sys_chmod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_chmod_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap = v;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	if (SCARG(uap, mode) & ~(S_IFMT | ALLPERMS))
		return (EINVAL);

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else {
		VATTR_NULL(&vattr);
		vattr.va_mode = SCARG(uap, mode) & ALLPERMS;
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
	vput(vp);
	return (error);
}

/*
 * Change mode of a file given a file descriptor.
 */
/* ARGSUSED */
int
sys_fchmod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_fchmod_args /* {
		syscallarg(int) fd;
		syscallarg(int) mode;
	} */ *uap = v;
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if (SCARG(uap, mode) & ~(S_IFMT | ALLPERMS))
		return (EINVAL);

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else {
		VATTR_NULL(&vattr);
		vattr.va_mode = SCARG(uap, mode) & ALLPERMS;
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
	VOP_UNLOCK(vp, 0, p);
	FRELE(fp);
	return (error);
}

/*
 * Set ownership given a path name.
 */
/* ARGSUSED */
int
sys_chown(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_chown_args /* {
		syscallarg(char *) path;
		syscallarg(int) uid;
		syscallarg(int) gid;
	} */ *uap = v;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;
	u_short mode;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else {
		if ((SCARG(uap, uid) != -1 || SCARG(uap, gid) != -1) &&
		    (suser(p->p_ucred, &p->p_acflag) || suid_clear)) {
			error = VOP_GETATTR(vp, &vattr, p->p_ucred, p);
			if (error)
				goto out;
			mode = vattr.va_mode & ~(VSUID | VSGID);
			if (mode == vattr.va_mode)
				mode = VNOVAL;
		}
		else
			mode = VNOVAL;
		VATTR_NULL(&vattr);
		vattr.va_uid = SCARG(uap, uid);
		vattr.va_gid = SCARG(uap, gid);
		vattr.va_mode = mode;
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
out:
	vput(vp);
	return (error);
}

/*
 * Set ownership given a path name, without following links.
 */
/* ARGSUSED */
int
sys_lchown(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_lchown_args /* {
		syscallarg(char *) path;
		syscallarg(int) uid;
		syscallarg(int) gid;
	} */ *uap = v;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;
	u_short mode;

	NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else {
		if ((SCARG(uap, uid) != -1 || SCARG(uap, gid) != -1) &&
		    (suser(p->p_ucred, &p->p_acflag) || suid_clear)) {
			error = VOP_GETATTR(vp, &vattr, p->p_ucred, p);
			if (error)
				goto out;
			mode = vattr.va_mode & ~(VSUID | VSGID);
			if (mode == vattr.va_mode)
				mode = VNOVAL;
		}
		else
			mode = VNOVAL;
		VATTR_NULL(&vattr);
		vattr.va_uid = SCARG(uap, uid);
		vattr.va_gid = SCARG(uap, gid);
		vattr.va_mode = mode;
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
out:
	vput(vp);
	return (error);
}

/*
 * Set ownership given a file descriptor.
 */
/* ARGSUSED */
int
sys_fchown(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_fchown_args /* {
		syscallarg(int) fd;
		syscallarg(int) uid;
		syscallarg(int) gid;
	} */ *uap = v;
	struct vnode *vp;
	struct vattr vattr;
	int error;
	struct file *fp;
	u_short mode;

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else {
		if ((SCARG(uap, uid) != -1 || SCARG(uap, gid) != -1) &&
		    (suser(p->p_ucred, &p->p_acflag) || suid_clear)) {
			error = VOP_GETATTR(vp, &vattr, p->p_ucred, p);
			if (error)
				goto out;
			mode = vattr.va_mode & ~(VSUID | VSGID);
			if (mode == vattr.va_mode)
				mode = VNOVAL;
		} else
			mode = VNOVAL;
		VATTR_NULL(&vattr);
		vattr.va_uid = SCARG(uap, uid);
		vattr.va_gid = SCARG(uap, gid);
		vattr.va_mode = mode;
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
out:
	VOP_UNLOCK(vp, 0, p);
	FRELE(fp);
	return (error);
}

/*
 * Set the access and modification times given a path name.
 */
/* ARGSUSED */
int
sys_utimes(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_utimes_args /* {
		syscallarg(char *) path;
		syscallarg(struct timeval *) tptr;
	} */ *uap = v;
	register struct vnode *vp;
	struct timeval tv[2];
	struct vattr vattr;
	int error;
	struct nameidata nd;

	VATTR_NULL(&vattr);
	if (SCARG(uap, tptr) == NULL) {
		microtime(&tv[0]);
		tv[1] = tv[0];
		vattr.va_vaflags |= VA_UTIMES_NULL;
	} else {
		error = copyin((caddr_t)SCARG(uap, tptr), (caddr_t)tv,
		    sizeof (tv));
		if (error)
			return (error);
		/* XXX workaround timeval matching the VFS constant VNOVAL */
		if (tv[0].tv_sec == VNOVAL)
			tv[0].tv_sec = VNOVAL - 1;
		if (tv[1].tv_sec == VNOVAL)
			tv[1].tv_sec = VNOVAL - 1;
	}
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else {
		vattr.va_atime.tv_sec = tv[0].tv_sec;
		vattr.va_atime.tv_nsec = tv[0].tv_usec * 1000;
		vattr.va_mtime.tv_sec = tv[1].tv_sec;
		vattr.va_mtime.tv_nsec = tv[1].tv_usec * 1000;
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
	vput(vp);
	return (error);
}


/*
 * Set the access and modification times given a file descriptor.
 */
/* ARGSUSED */
int
sys_futimes(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_futimes_args /* {
		syscallarg(int) fd;
		syscallarg(struct timeval *) tptr;
	} */ *uap = v;
	struct vnode *vp;
	struct timeval tv[2];
	struct vattr vattr;
	int error;
	struct file *fp;

	VATTR_NULL(&vattr);
	if (SCARG(uap, tptr) == NULL) {
		microtime(&tv[0]);
		tv[1] = tv[0];
		vattr.va_vaflags |= VA_UTIMES_NULL;
	} else {
		error = copyin((caddr_t)SCARG(uap, tptr), (caddr_t)tv,
		    sizeof (tv));
		if (error)
			return (error);
		/* XXX workaround timeval matching the VFS constant VNOVAL */
		if (tv[0].tv_sec == VNOVAL)
			tv[0].tv_sec = VNOVAL - 1;
		if (tv[1].tv_sec == VNOVAL)
			tv[1].tv_sec = VNOVAL - 1;
	}
	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		error = EROFS;
	else {
		vattr.va_atime.tv_sec = tv[0].tv_sec;
		vattr.va_atime.tv_nsec = tv[0].tv_usec * 1000;
		vattr.va_mtime.tv_sec = tv[1].tv_sec;
		vattr.va_mtime.tv_nsec = tv[1].tv_usec * 1000;
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
	VOP_UNLOCK(vp, 0, p);
	FRELE(fp);
	return (error);
}

/*
 * Truncate a file given its path name.
 */
/* ARGSUSED */
int
sys_truncate(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_truncate_args /* {
		syscallarg(char *) path;
		syscallarg(int) pad;
		syscallarg(off_t) length;
	} */ *uap = v;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_type == VDIR)
		error = EISDIR;
	else if ((error = vn_writechk(vp)) == 0 &&
	    (error = VOP_ACCESS(vp, VWRITE, p->p_ucred, p)) == 0) {
		VATTR_NULL(&vattr);
		vattr.va_size = SCARG(uap, length);
		error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
	}
	vput(vp);
	return (error);
}

/*
 * Truncate a file given a file descriptor.
 */
/* ARGSUSED */
int
sys_ftruncate(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_ftruncate_args /* {
		syscallarg(int) fd;
		syscallarg(int) pad;
		syscallarg(off_t) length;
	} */ *uap = v;
	struct vattr vattr;
	struct vnode *vp;
	struct file *fp;
	int error;

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);
	if ((fp->f_flag & FWRITE) == 0) {
		error = EINVAL;
		goto bad;
	}
	vp = (struct vnode *)fp->f_data;
	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (vp->v_type == VDIR)
		error = EISDIR;
	else if ((error = vn_writechk(vp)) == 0) {
		VATTR_NULL(&vattr);
		vattr.va_size = SCARG(uap, length);
		error = VOP_SETATTR(vp, &vattr, fp->f_cred, p);
	}
	VOP_UNLOCK(vp, 0, p);
bad:
	FRELE(fp);
	return (error);
}

/*
 * Sync an open file.
 */
/* ARGSUSED */
int
sys_fsync(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_fsync_args /* {
		syscallarg(int) fd;
	} */ *uap = v;
	struct vnode *vp;
	struct file *fp;
	int error;

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);
	vp = (struct vnode *)fp->f_data;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	error = VOP_FSYNC(vp, fp->f_cred, MNT_WAIT, p);
#ifdef FFS_SOFTUPDATES
	if (error == 0 && vp->v_mount && (vp->v_mount->mnt_flag & MNT_SOFTDEP))
		error = softdep_fsync(vp);
#endif

	VOP_UNLOCK(vp, 0, p);
	FRELE(fp);
	return (error);
}

/*
 * Rename files.  Source and destination must either both be directories,
 * or both not be directories.  If target is a directory, it must be empty.
 */
/* ARGSUSED */
int
sys_rename(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_rename_args /* {
		syscallarg(char *) from;
		syscallarg(char *) to;
	} */ *uap = v;
	register struct vnode *tvp, *fvp, *tdvp;
	struct nameidata fromnd, tond;
	int error;
	int flags;

	NDINIT(&fromnd, DELETE, WANTPARENT | SAVESTART, UIO_USERSPACE,
	    SCARG(uap, from), p);
	if ((error = namei(&fromnd)) != 0)
		return (error);
	fvp = fromnd.ni_vp;

	flags = LOCKPARENT | LOCKLEAF | NOCACHE | SAVESTART;
	/*
	 * rename("foo/", "bar/");  is  OK
	 */
	if (fvp->v_type == VDIR)
		flags |= STRIPSLASHES;

	NDINIT(&tond, RENAME, flags,
	    UIO_USERSPACE, SCARG(uap, to), p);
	if ((error = namei(&tond)) != 0) {
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
		goto out1;
	}
	tdvp = tond.ni_dvp;
	tvp = tond.ni_vp;
	if (tvp != NULL) {
		if (fvp->v_type == VDIR && tvp->v_type != VDIR) {
			error = ENOTDIR;
			goto out;
		} else if (fvp->v_type != VDIR && tvp->v_type == VDIR) {
			error = EISDIR;
			goto out;
		}
	}
	if (fvp == tdvp)
		error = EINVAL;
	/*
	 * If source is the same as the destination (that is the
	 * same inode number)
	 */
	if (fvp == tvp)
		error = -1;
out:
	if (!error) {
		VOP_LEASE(tdvp, p, p->p_ucred, LEASE_WRITE);
		if (fromnd.ni_dvp != tdvp)
			VOP_LEASE(fromnd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
		if (tvp) {
			(void)uvm_vnp_uncache(tvp);
			VOP_LEASE(tvp, p, p->p_ucred, LEASE_WRITE);
		}
		error = VOP_RENAME(fromnd.ni_dvp, fromnd.ni_vp, &fromnd.ni_cnd,
				   tond.ni_dvp, tond.ni_vp, &tond.ni_cnd);
	} else {
		VOP_ABORTOP(tond.ni_dvp, &tond.ni_cnd);
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		VOP_ABORTOP(fromnd.ni_dvp, &fromnd.ni_cnd);
		vrele(fromnd.ni_dvp);
		vrele(fvp);
	}
	vrele(tond.ni_startdir);
	FREE(tond.ni_cnd.cn_pnbuf, M_NAMEI);
out1:
	if (fromnd.ni_startdir)
		vrele(fromnd.ni_startdir);
	FREE(fromnd.ni_cnd.cn_pnbuf, M_NAMEI);
	if (error == -1)
		return (0);
	return (error);
}

/*
 * Make a directory file.
 */
/* ARGSUSED */
int
sys_mkdir(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_mkdir_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap = v;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, CREATE, LOCKPARENT | STRIPSLASHES,
	    UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	if (vp != NULL) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(vp);
		return (EEXIST);
	}
	VATTR_NULL(&vattr);
	vattr.va_type = VDIR;
	vattr.va_mode = (SCARG(uap, mode) & ACCESSPERMS) &~ p->p_fd->fd_cmask;
	VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
	error = VOP_MKDIR(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	if (!error)
		vput(nd.ni_vp);
	return (error);
}

/*
 * Remove a directory file.
 */
/* ARGSUSED */
int
sys_rmdir(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_rmdir_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	register struct vnode *vp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, DELETE, LOCKPARENT | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}
	/*
	 * No rmdir "." please.
	 */
	if (nd.ni_dvp == vp) {
		error = EBUSY;
		goto out;
	}
	/*
	 * The root of a mounted filesystem cannot be deleted.
	 */
	if (vp->v_flag & VROOT)
		error = EBUSY;
out:
	if (!error) {
		VOP_LEASE(nd.ni_dvp, p, p->p_ucred, LEASE_WRITE);
		VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
		error = VOP_RMDIR(nd.ni_dvp, nd.ni_vp, &nd.ni_cnd);
	} else {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vput(vp);
	}
	return (error);
}

/*
 * Read a block of directory entries in a file system independent format.
 */
int
sys_getdirentries(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_getdirentries_args /* {
		syscallarg(int) fd;
		syscallarg(char *) buf;
		syscallarg(int) count;
		syscallarg(long *) basep;
	} */ *uap = v;
	struct vnode *vp;
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	long loff;
	int error, eofflag;

	if (SCARG(uap, count) < 0)
		return EINVAL;
	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);
	if ((fp->f_flag & FREAD) == 0) {
		error = EBADF;
		goto bad;
	}
	vp = (struct vnode *)fp->f_data;
unionread:
	if (vp->v_type != VDIR) {
		error = EINVAL;
		goto bad;
	}
	aiov.iov_base = SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, count);
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
	auio.uio_resid = SCARG(uap, count);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	loff = auio.uio_offset = fp->f_offset;
	error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, 0, 0);
	fp->f_offset = auio.uio_offset;
	VOP_UNLOCK(vp, 0, p);
	if (error)
		goto bad;
	if ((SCARG(uap, count) == auio.uio_resid) &&
	    union_check_p &&
	    (union_check_p(p, &vp, fp, auio, &error) != 0))
		goto unionread;
	if (error)
		goto bad;

	if ((SCARG(uap, count) == auio.uio_resid) &&
	    (vp->v_flag & VROOT) &&
	    (vp->v_mount->mnt_flag & MNT_UNION)) {
		struct vnode *tvp = vp;
		vp = vp->v_mount->mnt_vnodecovered;
		VREF(vp);
		fp->f_data = (caddr_t) vp;
		fp->f_offset = 0;
		vrele(tvp);
		goto unionread;
	}
	error = copyout((caddr_t)&loff, (caddr_t)SCARG(uap, basep),
	    sizeof(long));
	*retval = SCARG(uap, count) - auio.uio_resid;
bad:
	FRELE(fp);
	return (error);
}

/*
 * Set the mode mask for creation of filesystem nodes.
 */
int
sys_umask(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_umask_args /* {
		syscallarg(int) newmask;
	} */ *uap = v;
	register struct filedesc *fdp;

	fdp = p->p_fd;
	*retval = fdp->fd_cmask;
	fdp->fd_cmask = SCARG(uap, newmask) & ACCESSPERMS;
	return (0);
}

/*
 * Void all references to file by ripping underlying filesystem
 * away from vnode.
 */
/* ARGSUSED */
int
sys_revoke(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_revoke_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	register struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	if ((error = VOP_GETATTR(vp, &vattr, p->p_ucred, p)) != 0)
		goto out;
	if (p->p_ucred->cr_uid != vattr.va_uid &&
	    (error = suser(p->p_ucred, &p->p_acflag)))
		goto out;
	if (vp->v_usecount > 1 || (vp->v_flag & VALIASED))
		VOP_REVOKE(vp, REVOKEALL);
out:
	vrele(vp);
	return (error);
}

/*
 * Convert a user file descriptor to a kernel file entry.
 *
 * On return *fpp is FREF:ed.
 */
int
getvnode(fdp, fd, fpp)
	struct filedesc *fdp;
	struct file **fpp;
	int fd;
{
	struct file *fp;

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_VNODE)
		return (EINVAL);
	FREF(fp);
	*fpp = fp;

	return (0);
}

/*
 * Positional read system call.
 */
int
sys_pread(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_pread_args /* {
		syscallarg(int) fd;
		syscallarg(void *) buf;
		syscallarg(size_t) nbyte;
		syscallarg(int) pad;
		syscallarg(off_t) offset;
	} */ *uap = v;
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct vnode *vp;
	off_t offset;
	int fd = SCARG(uap, fd);

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);
	if ((fp->f_flag & FREAD) == 0)
		return (EBADF);

	vp = (struct vnode *)fp->f_data;
	if (fp->f_type != DTYPE_VNODE || vp->v_type == VFIFO) {
		return (ESPIPE);
	}

	offset = SCARG(uap, offset);

	FREF(fp);

	/* dofileread() will FRELE the descriptor for us */
	return (dofileread(p, fd, fp, SCARG(uap, buf), SCARG(uap, nbyte),
	    &offset, retval));
}

/*
 * Positional scatter read system call.
 */
int
sys_preadv(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_preadv_args /* {
		syscallarg(int) fd;
		syscallarg(const struct iovec *) iovp;
		syscallarg(int) iovcnt;
		syscallarg(int) pad;
		syscallarg(off_t) offset;
	} */ *uap = v;
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct vnode *vp;
	off_t offset;
	int fd = SCARG(uap, fd);

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);
	if ((fp->f_flag & FREAD) == 0)
		return (EBADF);

	vp = (struct vnode *)fp->f_data;
	if (fp->f_type != DTYPE_VNODE || vp->v_type == VFIFO) {
		return (ESPIPE);
	}

	FREF(fp);

	offset = SCARG(uap, offset);

	/* dofilereadv() will FRELE the descriptor for us */
	return (dofilereadv(p, fd, fp, SCARG(uap, iovp), SCARG(uap, iovcnt),
	    &offset, retval));
}

/*
 * Positional write system call.
 */
int
sys_pwrite(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_pwrite_args /* {
		syscallarg(int) fd;
		syscallarg(const void *) buf;
		syscallarg(size_t) nbyte;
		syscallarg(int) pad;
		syscallarg(off_t) offset;
	} */ *uap = v;
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct vnode *vp;
	off_t offset;
	int fd = SCARG(uap, fd);

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);
	if ((fp->f_flag & FWRITE) == 0)
		return (EBADF);

	vp = (struct vnode *)fp->f_data;
	if (fp->f_type != DTYPE_VNODE || vp->v_type == VFIFO) {
		return (ESPIPE);
	}

	FREF(fp);

	offset = SCARG(uap, offset);

	/* dofilewrite() will FRELE the descriptor for us */
	return (dofilewrite(p, fd, fp, SCARG(uap, buf), SCARG(uap, nbyte),
	    &offset, retval));
}


/*
 * Positional gather write system call.
 */
int
sys_pwritev(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_pwritev_args /* {
		syscallarg(int) fd;
		syscallarg(const struct iovec *) iovp;
		syscallarg(int) iovcnt;
		syscallarg(int) pad;
		syscallarg(off_t) offset;
	} */ *uap = v;
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	struct vnode *vp;
	off_t offset;
	int fd = SCARG(uap, fd);

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);
	if ((fp->f_flag & FWRITE) == 0)
		return (EBADF);

	vp = (struct vnode *)fp->f_data;
	if (fp->f_type != DTYPE_VNODE || vp->v_type == VFIFO) {
		return (ESPIPE);
	}

	FREF(fp);

	offset = SCARG(uap, offset);

	/* dofilewritev() will FRELE the descriptor for us */
	return (dofilewritev(p, fd, fp, SCARG(uap, iovp), SCARG(uap, iovcnt),
	    &offset, retval));
}

#ifdef UFS_EXTATTR
/*
 * Syscall to push extended attribute configuration information into the
 * VFS.  Accepts a path, which it converts to a mountpoint, as well as
 * a command (int cmd), and attribute name and misc data.  For now, the
 * attribute name is left in userspace for consumption by the VFS_op.
 * It will probably be changed to be copied into sysspace by the
 * syscall in the future, once issues with various consumers of the
 * attribute code have raised their hands.
 *
 * Currently this is used only by UFS Extended Attributes.
 */
int
sys_extattrctl(struct proc *p, void *v, register_t *reval)
{
	struct sys_extattrctl_args /* {
		syscallarg(const char *) path;
		syscallarg(int) cmd;
		syscallarg(const char *) filename;
		syscallarg(int) attrnamespace;
		syscallarg(const char *) attrname;
	} */ *uap = v;
	struct vnode *filename_vp;
	struct nameidata nd;
	struct mount *mp;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	/*
	 * SCARG(uap, attrname) not always defined.  We check again later
	 * when we invoke the VFS call so as to pass in NULL there if needed.
	 */
	if (SCARG(uap, attrname) != NULL) {
		error = copyinstr(SCARG(uap, attrname), attrname,
		    EXTATTR_MAXNAMELEN, NULL);
		if (error)
			return (error);
	}

	/*
	 * SCARG(uap, filename) not always defined.  If it is, grab
	 * a vnode lock, which VFS_EXTATTRCTL() will later release.
	 */
	filename_vp = NULL;
	if (SCARG(uap, filename) != NULL) {
		NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
		    SCARG(uap, filename), p);
		if ((error = namei(&nd)) != 0)
			return (error);
		filename_vp = nd.ni_vp;
	}

	/* SCARG(uap, path) always defined. */
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0) {
		if (filename_vp != NULL)
			vput(filename_vp);
		return (error);
	}

	mp = nd.ni_vp->v_mount;
	if (error) {
		if (filename_vp != NULL)
			vput(filename_vp);
		return (error);
	}

	if (SCARG(uap, attrname) != NULL) {
		error = VFS_EXTATTRCTL(mp, SCARG(uap, cmd), filename_vp,
		    SCARG(uap, attrnamespace), attrname, p);
	} else {
		error = VFS_EXTATTRCTL(mp, SCARG(uap, cmd), filename_vp,
		    SCARG(uap, attrnamespace), NULL, p);
	}

	/*
	 * VFS_EXTATTRCTL will have unlocked, but not de-ref'd,
	 * filename_vp, so vrele it if it is defined.
	 */
	if (filename_vp != NULL)
		vrele(filename_vp);

	return (error);
}

/*-
 * Set a named extended attribute on a file or directory
 * 
 * Arguments: unlocked vnode "vp", attribute namespace "attrnamespace",
 *            kernelspace string pointer "attrname", userspace buffer
 *            pointer "data", buffer length "nbytes", thread "td".
 * Returns: 0 on success, an error number otherwise
 * Locks: none
 * References: vp must be a valid reference for the duration of the call
 */
static int
extattr_set_vp(struct vnode *vp, int attrnamespace, const char *attrname,
    void *data, size_t nbytes, struct proc *p, register_t *retval)
{
	struct uio auio;
	struct iovec aiov;
	ssize_t cnt;
	int error;

	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);

	aiov.iov_base = data;
	aiov.iov_len = nbytes;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	if (nbytes > INT_MAX) {
		error = EINVAL;
		goto done;
	}
	auio.uio_resid = nbytes;
	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
	cnt = nbytes;

	error = VOP_SETEXTATTR(vp, attrnamespace, attrname, &auio,
	    p->p_ucred, p);
	cnt -= auio.uio_resid;
	retval[0] = cnt;

done:
	VOP_UNLOCK(vp, 0, p);
	return (error);
}

int
sys_extattr_set_file(struct proc *p, void *v, register_t *retval)
{
	struct sys_extattr_set_file_args /* {
		syscallarg(const char *) path;
		syscallarg(int) attrnamespace;
		syscallarg(const char *) attrname;
		syscallarg(void *) data;
		syscallarg(size_t) nbytes;
	} */ *uap = v;
	struct nameidata nd;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(SCARG(uap, attrname), attrname, EXTATTR_MAXNAMELEN,
	    NULL);
	if (error)
		return (error);

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);

	error = extattr_set_vp(nd.ni_vp, SCARG(uap, attrnamespace), attrname,
	    SCARG(uap, data), SCARG(uap, nbytes), p, retval);

	vrele(nd.ni_vp);
	return (error);
}

int
sys_extattr_set_fd(struct proc *p, void *v, register_t *retval)
{
	struct sys_extattr_set_fd_args /* {
		syscallarg(int) fd;
		syscallarg(int) attrnamespace;
		syscallarg(const char *) attrname;
		syscallarg(struct iovec *) iovp;
		syscallarg(int) iovcnt;
	} */ *uap = v;
	struct file *fp;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(SCARG(uap, attrname), attrname, EXTATTR_MAXNAMELEN,
	    NULL);
	if (error)
		return (error);

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);

	error = extattr_set_vp((struct vnode *)fp->f_data,
	    SCARG(uap, attrnamespace), attrname, SCARG(uap, data),
	    SCARG(uap, nbytes), p, retval);
	FRELE(fp);

	return (error);
}

/*-
 * Get a named extended attribute on a file or directory
 * 
 * Arguments: unlocked vnode "vp", attribute namespace "attrnamespace",
 *            kernelspace string pointer "attrname", userspace buffer
 *            pointer "data", buffer length "nbytes", thread "td".
 * Returns: 0 on success, an error number otherwise
 * Locks: none
 * References: vp must be a valid reference for the duration of the call
 */
static int
extattr_get_vp(struct vnode *vp, int attrnamespace, const char *attrname,
    void *data, size_t nbytes, struct proc *p, register_t *retval)
{
	struct uio auio;
	struct iovec aiov;
	ssize_t cnt;
	size_t size;
	int error;

	VOP_LEASE(vp, p, p->p_ucred, LEASE_READ);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);

	/*
	 * Slightly unusual semantics: if the user provides a NULL data
	 * pointer, they don't want to receive the data, just the
	 * maximum read length.
	 */
	if (data != NULL) {
		aiov.iov_base = data;
		aiov.iov_len = nbytes;
		auio.uio_iov = &aiov;
		auio.uio_offset = 0;
		if (nbytes > INT_MAX) {
			error = EINVAL;
			goto done;
		}
		auio.uio_resid = nbytes;
		auio.uio_rw = UIO_READ;
		auio.uio_segflg = UIO_USERSPACE;
		auio.uio_procp = p;
		cnt = nbytes;
		error = VOP_GETEXTATTR(vp, attrnamespace, attrname, &auio,
		    NULL, p->p_ucred, p);
		cnt -= auio.uio_resid;
		retval[0] = cnt;
	} else {
		error = VOP_GETEXTATTR(vp, attrnamespace, attrname, NULL,
		    &size, p->p_ucred, p);
		retval[0] = size;
	}
done:
	VOP_UNLOCK(vp, 0, p);
	return (error);
}

int
sys_extattr_get_file(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_extattr_get_file_args /* {
		syscallarg(const char *) path;
		syscallarg(int) attrnamespace;
		syscallarg(const char *) attrname;
		syscallarg(void *) data;
		syscallarg(size_t) nbytes;
	} */ *uap = v;
	struct nameidata nd;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(SCARG(uap, attrname), attrname, EXTATTR_MAXNAMELEN,
	    NULL);
	if (error)
		return (error);

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return (error);

	error = extattr_get_vp(nd.ni_vp, SCARG(uap, attrnamespace), attrname,
	    SCARG(uap, data), SCARG(uap, nbytes), p, retval);

	vrele(nd.ni_vp);
	return (error);
}

int
sys_extattr_get_fd(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_extattr_get_fd_args /* {
		syscallarg(int) fd;
		syscallarg(int) attrnamespace;
		syscallarg(const char *) attrname;
		syscallarg(void *) data;
		syscallarg(size_t) nbytes;
	} */ *uap = v;
	struct file *fp;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(SCARG(uap, attrname), attrname, EXTATTR_MAXNAMELEN,
	    NULL);
	if (error)
		return (error);

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);

	error = extattr_get_vp((struct vnode *)fp->f_data,
	    SCARG(uap, attrnamespace), attrname, SCARG(uap, data),
	    SCARG(uap, nbytes), p, retval);
	FRELE(fp);

	return (error);
}

/*
 * extattr_delete_vp(): Delete a named extended attribute on a file or
 *                      directory
 * 
 * Arguments: unlocked vnode "vp", attribute namespace "attrnamespace",
 *            kernelspace string pointer "attrname", proc "p"
 * Returns: 0 on success, an error number otherwise
 * Locks: none
 * References: vp must be a valid reference for the duration of the call
 */
static int
extattr_delete_vp(struct vnode *vp, int attrnamespace, const char *attrname,
    struct proc *p)
{
	int error;

	VOP_LEASE(vp, p, p->p_ucred, LEASE_WRITE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);

	error = VOP_SETEXTATTR(vp, attrnamespace, attrname, NULL,
	    p->p_ucred, p);

	VOP_UNLOCK(vp, 0, p);
	return (error);
}

int
sys_extattr_delete_file(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_extattr_delete_file_args /* {
		syscallarg(int) fd;
		syscallarg(int) attrnamespace;
		syscallarg(const char *) attrname;
	} */ *uap = v;
	struct nameidata nd;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(SCARG(uap, attrname), attrname, EXTATTR_MAXNAMELEN,
	     NULL);
	if (error)
		return(error);

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if ((error = namei(&nd)) != 0)
		return(error);

	error = extattr_delete_vp(nd.ni_vp, SCARG(uap, attrnamespace),
	    attrname, p);

	vrele(nd.ni_vp);
	return(error);
}

int
sys_extattr_delete_fd(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_extattr_delete_fd_args /* {
		syscallarg(int) fd;
		syscallarg(int) attrnamespace;
		syscallarg(const char *) attrname;
	} */ *uap = v; 
	struct file *fp;
	char attrname[EXTATTR_MAXNAMELEN];
	int error;

	error = copyinstr(SCARG(uap, attrname), attrname, EXTATTR_MAXNAMELEN,
	    NULL);
	if (error)
		return (error);

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);

	error = extattr_delete_vp((struct vnode *)fp->f_data,
	    SCARG(uap, attrnamespace), attrname, p);
	FRELE(fp);

	return (error);
}
#endif 

/*	$OpenBSD: procfs_vfsops.c,v 1.26 2010/09/23 18:43:37 oga Exp $	*/
/*	$NetBSD: procfs_vfsops.c,v 1.25 1996/02/09 22:40:53 christos Exp $	*/

/*
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs_vfsops.c	8.5 (Berkeley) 6/15/94
 */

/*
 * procfs VFS interface
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/syslog.h>
#include <sys/mount.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/malloc.h>

#include <miscfs/procfs/procfs.h>

#include <uvm/uvm_extern.h>

int	procfs_mount(struct mount *, const char *, void *,
			  struct nameidata *, struct proc *);
int	procfs_start(struct mount *, int, struct proc *);
int	procfs_unmount(struct mount *, int, struct proc *);
int	procfs_statfs(struct mount *, struct statfs *, struct proc *);
/*
 * VFS Operations.
 *
 * mount system call
 */
/* ARGSUSED */
int
procfs_mount(struct mount *mp, const char *path, void *data, struct nameidata *ndp,
    struct proc *p)
{
	size_t size;
	struct procfsmount *pmnt;
	struct procfs_args args;
	int error;

	if (UIO_MX & (UIO_MX-1)) {
		log(LOG_ERR, "procfs: invalid directory entry size");
		return (EINVAL);
	}

	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

	if (data != NULL) {
		error = copyin(data, &args, sizeof args);
		if (error != 0)
			return (error);

		if (args.version != PROCFS_ARGSVERSION)
			return (EINVAL);
	} else
		args.flags = 0;

	mp->mnt_flag |= MNT_LOCAL;
	pmnt = (struct procfsmount *) malloc(sizeof(struct procfsmount),
	    M_MISCFSMNT, M_WAITOK);

	mp->mnt_data = pmnt;
	vfs_getnewfsid(mp);

	(void) copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
	bzero(mp->mnt_stat.f_mntfromname, MNAMELEN);
	bcopy("procfs", mp->mnt_stat.f_mntfromname, sizeof("procfs"));
	bcopy(&args, &mp->mnt_stat.mount_info.procfs_args, sizeof(args));

#ifdef notyet
	pmnt->pmnt_exechook = exechook_establish(procfs_revoke_vnodes, mp);
#endif
	pmnt->pmnt_flags = args.flags;

	return (0);
}

/*
 * unmount system call
 */
int
procfs_unmount(struct mount *mp, int mntflags, struct proc *p)
{
	int error;
	extern int doforce;
	int flags = 0;

	if (mntflags & MNT_FORCE) {
		/* procfs can never be rootfs so don't check for it */
		if (!doforce)
			return (EINVAL);
		flags |= FORCECLOSE;
	}

	if ((error = vflush(mp, 0, flags)) != 0)
		return (error);

	free(VFSTOPROC(mp), M_MISCFSMNT);
	mp->mnt_data = 0;

	return (0);
}

int
procfs_root(struct mount *mp, struct vnode **vpp)
{
	int error;

	error = procfs_allocvp(mp, vpp, 0, Proot);
	if (error)
		return (error);
	vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY, curproc);

	return (0);
}

/* ARGSUSED */
int
procfs_start(struct mount *mp, int flags, struct proc *p)
{

	return (0);
}

/*
 * Get file system statistics.
 */
int
procfs_statfs(struct mount *mp, struct statfs *sbp, struct proc *p)
{
	struct vmtotal	vmtotals;

	uvm_total(&vmtotals);
	sbp->f_bsize = PAGE_SIZE;
	sbp->f_iosize = PAGE_SIZE;
	sbp->f_blocks = vmtotals.t_vm;
	sbp->f_bfree = vmtotals.t_vm - vmtotals.t_avm;
	sbp->f_bavail = 0;
	sbp->f_files = maxproc;			/* approx */
	sbp->f_ffree = maxproc - nprocs;	/* approx */
	if (sbp != &mp->mnt_stat) {
		bcopy(&mp->mnt_stat.f_fsid, &sbp->f_fsid, sizeof(sbp->f_fsid));
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
		bcopy(&mp->mnt_stat.mount_info.procfs_args,
		    &sbp->mount_info.procfs_args, sizeof(struct procfs_args));
	}
	strncpy(sbp->f_fstypename, mp->mnt_vfc->vfc_name, MFSNAMELEN);
	return (0);
}


#define procfs_sync ((int (*)(struct mount *, int, struct ucred *, \
				  struct proc *))nullop)

#define procfs_fhtovp ((int (*)(struct mount *, struct fid *, \
	    struct vnode **))eopnotsupp)
#define procfs_quotactl ((int (*)(struct mount *, int, uid_t, caddr_t, \
	    struct proc *))eopnotsupp)
#define procfs_sysctl ((int (*)(int *, u_int, void *, size_t *, void *, \
	    size_t, struct proc *))eopnotsupp)
#define procfs_vget ((int (*)(struct mount *, ino_t, struct vnode **)) \
	    eopnotsupp)
#define procfs_vptofh ((int (*)(struct vnode *, struct fid *))eopnotsupp)
#define procfs_checkexp ((int (*)(struct mount *, struct mbuf *,	\
	int *, struct ucred **))eopnotsupp)

const struct vfsops procfs_vfsops = {
	procfs_mount,
	procfs_start,
	procfs_unmount,
	procfs_root,
	procfs_quotactl,
	procfs_statfs,
	procfs_sync,
	procfs_vget,
	procfs_fhtovp,
	procfs_vptofh,
	procfs_init,
	procfs_sysctl,
	procfs_checkexp
};

/*	$OpenBSD: fdesc_vfsops.c,v 1.11 2002/03/14 01:27:07 millert Exp $	*/
/*	$NetBSD: fdesc_vfsops.c,v 1.21 1996/02/09 22:40:07 christos Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
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
 *	@(#)fdesc_vfsops.c	8.4 (Berkeley) 1/21/94
 *
 * #Id: fdesc_vfsops.c,v 1.9 1993/04/06 15:28:33 jsp Exp #
 */

/*
 * /dev/fd Filesystem
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/filedesc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <miscfs/fdesc/fdesc.h>

int	fdesc_mount(struct mount *, const char *, void *,
			 struct nameidata *, struct proc *);
int	fdesc_start(struct mount *, int, struct proc *);
int	fdesc_unmount(struct mount *, int, struct proc *);
int	fdesc_root(struct mount *, struct vnode **);
int	fdesc_quotactl(struct mount *, int, uid_t, caddr_t,
			    struct proc *);
int	fdesc_statfs(struct mount *, struct statfs *, struct proc *);
int	fdesc_sync(struct mount *, int, struct ucred *, struct proc *);
int	fdesc_vget(struct mount *, ino_t, struct vnode **);
int	fdesc_fhtovp(struct mount *, struct fid *, struct vnode **);
int	fdesc_vptofh(struct vnode *, struct fid *);

/*
 * Mount the per-process file descriptors (/dev/fd)
 */
int
fdesc_mount(mp, path, data, ndp, p)
	struct mount *mp;
	const char *path;
	void *data;
	struct nameidata *ndp;
	struct proc *p;
{
	size_t size;

	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

	mp->mnt_flag |= MNT_LOCAL;
	vfs_getnewfsid(mp);

	(void) copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
	bzero(mp->mnt_stat.f_mntfromname, MNAMELEN);
	bcopy("fdesc", mp->mnt_stat.f_mntfromname, sizeof("fdesc"));
	return (0);
}

int
fdesc_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{
	return (0);
}

int
fdesc_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	int flags = 0;
	int error;

	if (mntflags & MNT_FORCE) 
		flags |= FORCECLOSE;

	/*
	 * Flush out our vnodes.
	 */
	if ((error = vflush(mp, NULL, flags)) != 0)
		return (error);

	return (0);
}

int
fdesc_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct vnode *vp;
	int error;
	/*
	 * Return locked reference to root.
	 */
	error = fdesc_allocvp(Froot, FD_ROOT, mp, &vp);
	if (error)
		return (error);
	vp->v_type = VDIR;
	vp->v_flag |= VROOT;
	*vpp = vp;
	return (0);
}

int
fdesc_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	struct filedesc *fdp;
	int lim;
	int i;
	int last;
	int freefd;

	/*
	 * Compute number of free file descriptors.
	 * [ Strange results will ensue if the open file
	 * limit is ever reduced below the current number
	 * of open files... ]
	 */
	lim = p->p_rlimit[RLIMIT_NOFILE].rlim_cur;
	fdp = p->p_fd;
	last = min(fdp->fd_nfiles, lim);
	freefd = 0;
	for (i = fdp->fd_freefile; i < last; i++)
		if (fdp->fd_ofiles[i] == NULL)
			freefd++;

	/*
	 * Adjust for the fact that the fdesc array may not
	 * have been fully allocated yet.
	 */
	if (fdp->fd_nfiles < lim)
		freefd += (lim - fdp->fd_nfiles);

	sbp->f_bsize = DEV_BSIZE;
	sbp->f_iosize = DEV_BSIZE;
	sbp->f_blocks = 2;		/* 1K to keep df happy */
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = lim + 1;		/* Allow for "." */
	sbp->f_ffree = freefd;		/* See comments above */
	if (sbp != &mp->mnt_stat) {
		bcopy(&mp->mnt_stat.f_fsid, &sbp->f_fsid, sizeof(sbp->f_fsid));
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	strncpy(sbp->f_fstypename, mp->mnt_vfc->vfc_name, MFSNAMELEN);
	return (0);
}

/*ARGSUSED*/
int
fdesc_sync(mp, waitfor, uc, p)
	struct mount *mp;
	int waitfor;
	struct ucred *uc;
	struct proc *p;
{

	return (0);
}

#define fdesc_fhtovp ((int (*)(struct mount *, struct fid *, \
	    struct vnode **))eopnotsupp)
#define fdesc_quotactl ((int (*)(struct mount *, int, uid_t, caddr_t, \
	    struct proc *))eopnotsupp)
#define fdesc_sysctl ((int (*)(int *, u_int, void *, size_t *, void *, \
	    size_t, struct proc *))eopnotsupp)
#define fdesc_vget ((int (*)(struct mount *, ino_t, struct vnode **)) \
	    eopnotsupp)
#define fdesc_vptofh ((int (*)(struct vnode *, struct fid *))eopnotsupp)
 
#define fdesc_checkexp ((int (*)(struct mount *, struct mbuf *,	\
	int *, struct ucred **))eopnotsupp)

struct vfsops fdesc_vfsops = {
	fdesc_mount,
	fdesc_start,
	fdesc_unmount,
	fdesc_root,
	fdesc_quotactl,
	fdesc_statfs,
	fdesc_sync,
	fdesc_vget,
	fdesc_fhtovp,
	fdesc_vptofh,
	fdesc_init,
	fdesc_sysctl,
	fdesc_checkexp
};

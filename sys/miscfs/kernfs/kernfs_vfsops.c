/*	$OpenBSD: kernfs_vfsops.c,v 1.19 2002/10/12 02:03:46 krw Exp $	*/
/*	$NetBSD: kernfs_vfsops.c,v 1.26 1996/04/22 01:42:27 christos Exp $	*/

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
 *	@(#)kernfs_vfsops.c	8.5 (Berkeley) 6/15/94
 */

/*
 * Kernel params Filesystem
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>	/* for uvmexp */

#include <miscfs/specfs/specdev.h>
#include <miscfs/kernfs/kernfs.h>

dev_t rrootdev = NODEV;

void	kernfs_get_rrootdev(void);
int	kernfs_mount(struct mount *, const char *, void *, struct nameidata *,
			  struct proc *);
int	kernfs_start(struct mount *, int, struct proc *);
int	kernfs_unmount(struct mount *, int, struct proc *);
int	kernfs_root(struct mount *, struct vnode **);
int	kernfs_statfs(struct mount *, struct statfs *, struct proc *);

void
kernfs_get_rrootdev()
{
	static int tried = 0;
	int cmaj;

	if (tried) {
		/* Already did it once. */
		return;
	}
	tried = 1;

	if (rootdev == NODEV)
		return;
	for (cmaj = 0; cmaj < nchrdev; cmaj++) {
		rrootdev = makedev(cmaj, minor(rootdev));
		if (chrtoblk(rrootdev) == rootdev)
			return;
	}
	rrootdev = NODEV;
	printf("kernfs_get_rrootdev: no raw root device\n");
}

/*
 * Mount the Kernel params filesystem
 */
int
kernfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	const char *path;
	void *data;
	struct nameidata *ndp;
	struct proc *p;
{
	size_t size;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_mount(mp = %p)\n", mp);
#endif

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
	bcopy("kernfs", mp->mnt_stat.f_mntfromname, sizeof("kernfs"));
#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_mount: at %s\n", mp->mnt_stat.f_mntonname);
#endif

	kernfs_get_rrootdev();
	return (0);
}

int
kernfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{
	return (0);
}

int
kernfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	int error;
	int flags = 0;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_unmount(mp = %p)\n", mp);
#endif

	if (mntflags & MNT_FORCE) {
		flags |= FORCECLOSE;
	}

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_unmount: calling vflush\n");
#endif
	if ((error = vflush(mp, 0, flags)) != 0)
		return (error);

	return (0);
}

int
kernfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct kern_target *kt;
	int error;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_root(mp = %p)\n", mp);
#endif
	kt = kernfs_findtarget(".", 1);
	/* this should never happen */
	if (kt == NULL) 
		panic("kernfs_root: findtarget returned NULL");
	
	error = kernfs_allocvp(kt, mp, vpp);
	/* this should never happen */
	if (error) 
		panic("kernfs_root: couldn't find root");

	return(0);
	
}

int
kernfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	extern long numvnodes; /* XXX */

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_statfs(mp = %p)\n", mp);
#endif

	sbp->f_flags = 0;
	sbp->f_bsize = uvmexp.pagesize;
	sbp->f_iosize = uvmexp.pagesize;
	sbp->f_bfree = physmem - uvmexp.wired;
	sbp->f_blocks = physmem;
	sbp->f_bavail = 0;
	sbp->f_files = desiredvnodes;
	sbp->f_ffree = desiredvnodes - numvnodes;
	if (sbp != &mp->mnt_stat) {
		bcopy(&mp->mnt_stat.f_fsid, &sbp->f_fsid, sizeof(sbp->f_fsid));
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	strncpy(sbp->f_fstypename, mp->mnt_vfc->vfc_name, MFSNAMELEN);
	return (0);
}

struct vfsops kernfs_vfsops = {
	kernfs_mount,
	kernfs_start,
	kernfs_unmount,
	kernfs_root,
	kernfs_quotactl,
	kernfs_statfs,
	kernfs_sync,
	kernfs_vget,
	kernfs_fhtovp,
	kernfs_vptofh,
	kernfs_init,
	kernfs_sysctl,
	kernfs_checkexp
};

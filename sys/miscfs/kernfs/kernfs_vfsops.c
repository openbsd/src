/*	$OpenBSD: kernfs_vfsops.c,v 1.11 1999/02/26 03:44:16 art Exp $	*/
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

#if defined(UVM)
#include <vm/vm.h>
#include <uvm/uvm_extern.h>	/* for uvmexp */
#else
#include <sys/vmmeter.h>	/* for cnt */
#endif

#include <miscfs/specfs/specdev.h>
#include <miscfs/kernfs/kernfs.h>

dev_t rrootdev = NODEV;

int kernfs_init __P((struct vfsconf *));
void	kernfs_get_rrootdev __P((void));
int	kernfs_mount __P((struct mount *, const char *, caddr_t, struct nameidata *,
			  struct proc *));
int	kernfs_start __P((struct mount *, int, struct proc *));
int	kernfs_unmount __P((struct mount *, int, struct proc *));
int	kernfs_root __P((struct mount *, struct vnode **));
int	kernfs_statfs __P((struct mount *, struct statfs *, struct proc *));

/*ARGSUSED*/
int
kernfs_init(vfsp)
	struct vfsconf *vfsp;
{
	return (0);
}

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
	caddr_t data;
	struct nameidata *ndp;
	struct proc *p;
{
	int error = 0;
	size_t size;
	struct kernfs_mount *fmp;
	struct vnode *rvp;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_mount(mp = %p)\n", mp);
#endif

	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

	error = getnewvnode(VT_KERNFS, mp, kernfs_vnodeop_p, &rvp);
	if (error)
		return (error);

	MALLOC(fmp, struct kernfs_mount *, sizeof(struct kernfs_mount),
	    M_MISCFSMNT, M_WAITOK);
	rvp->v_type = VDIR;
	rvp->v_flag |= VROOT;
#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_mount: root vp = %p\n", rvp);
#endif
	fmp->kf_root = rvp;
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = (qaddr_t)fmp;
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
	struct vnode *rootvp = VFSTOKERNFS(mp)->kf_root;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_unmount(mp = %p)\n", mp);
#endif

	if (mntflags & MNT_FORCE) {
		flags |= FORCECLOSE;
	}

	/*
	 * Clear out buffer cache.  I don't think we
	 * ever get anything cached at this level at the
	 * moment, but who knows...
	 */
	if (rootvp->v_usecount > 1)
		return (EBUSY);
#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_unmount: calling vflush\n");
#endif
	if ((error = vflush(mp, rootvp, flags)) != 0)
		return (error);

#ifdef KERNFS_DIAGNOSTIC
	vprint("kernfs root", rootvp);
#endif
	/*
	 * Clean out the old root vnode for reuse.
	 */
	vrele(rootvp);
	vgone(rootvp);
	/*
	 * Finally, throw away the kernfs_mount structure
	 */
	free(mp->mnt_data, M_MISCFSMNT);
	mp->mnt_data = 0;
	return (0);
}

int
kernfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct vnode *vp;
	struct proc *p = curproc;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_root(mp = %p)\n", mp);
#endif

	/*
	 * Return locked reference to root.
	 */
	vp = VFSTOKERNFS(mp)->kf_root;
	VREF(vp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	*vpp = vp;
	return (0);
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

#ifdef COMPAT_09
	sbp->f_type = 7;
#endif
	sbp->f_flags = 0;
#if defined(UVM)
	sbp->f_bsize = uvmexp.pagesize;
	sbp->f_iosize = uvmexp.pagesize;
	sbp->f_bfree = physmem - uvmexp.wired;
#else
	sbp->f_bsize = cnt.v_page_size;
	sbp->f_iosize = cnt.v_page_size;
	sbp->f_bfree = physmem - cnt.v_wire_count;
#endif
	sbp->f_blocks = physmem;
	sbp->f_bavail = 0;
	sbp->f_files = desiredvnodes;
	sbp->f_ffree = desiredvnodes - numvnodes;
	if (sbp != &mp->mnt_stat) {
		sbp->f_type = mp->mnt_vfc->vfc_typenum;
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
	kernfs_sysctl
};

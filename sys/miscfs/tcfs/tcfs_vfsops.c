/*	$OpenBSD: tcfs_vfsops.c,v 1.5 2002/03/14 01:27:08 millert Exp $	*/
/*
 * Copyright 2000 The TCFS Project at http://tcfs.dia.unisa.it/
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <miscfs/tcfs/tcfs.h>

int	tcfs_mount(struct mount *, const char *, void *,
			  struct nameidata *, struct proc *);
int	tcfs_start(struct mount *, int, struct proc *);
int	tcfs_unmount(struct mount *, int, struct proc *);
int	tcfs_root(struct mount *, struct vnode **);
int	tcfs_quotactl(struct mount *, int, uid_t, caddr_t,
			     struct proc *);
int	tcfs_statfs(struct mount *, struct statfs *, struct proc *);
int	tcfs_sync(struct mount *, int, struct ucred *, struct proc *);
int	tcfs_vget(struct mount *, ino_t, struct vnode **);

/*
 * Mount tcfs layer
 */
int
tcfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	const char *path;
	void *data;
	struct nameidata *ndp;
	struct proc *p;
{
	int error = 0;
	struct tcfs_args args;
	struct vnode *lowerrootvp, *vp;
	struct vnode *tcfsm_rootvp;
	struct tcfs_mount *xmp;
	size_t size;
	int tcfs_error = 0;

#ifdef TCFS_DIAGNOSTIC
	printf("tcfs_mount(mp = %p)\n", mp);
#endif

	/*
	 * Get argument
	 */
	error = copyin(data, &args, sizeof(struct tcfs_args));
	if (error)
		return (error);

	/* receiving user directives */
        if (mp->mnt_flag & MNT_UPDATE) {
                 int i;
                 i = tcfs_exec_cmd(MOUNTTOTCFSMOUNT(mp), &args);
                 copyout((caddr_t)&args, data, sizeof(struct tcfs_args));
                 return i;
        }
	
	/*
	 * Find lower node
	 */
	NDINIT(ndp, LOOKUP, FOLLOW|WANTPARENT|LOCKLEAF,
		UIO_USERSPACE, args.target, p);
	if ((error = namei(ndp)) != 0)
		return (error);

	/*
	 * Sanity check on lower vnode
	 */
	lowerrootvp = ndp->ni_vp;

	vrele(ndp->ni_dvp);
	ndp->ni_dvp = NULL;

	if (lowerrootvp->v_type != VDIR) {
		vput(lowerrootvp);
		return (EINVAL);
	}

	xmp = (struct tcfs_mount *) malloc(sizeof(struct tcfs_mount),
				M_UFSMNT, M_WAITOK);	/* XXX */

	/*
	 * Save reference to underlying FS
	 */
	xmp->tcfsm_vfs = lowerrootvp->v_mount;

	/*
	 * Save reference.  Each mount also holds
	 * a reference on the root vnode.
	 */
	error = tcfs_node_create(mp, lowerrootvp, &vp, 1);
	/*
	 * Unlock the node (either the lower or the alias)
	 */
	VOP_UNLOCK(vp, 0, p);
	/*
	 * Make sure the node alias worked
	 */
	if (error) {
		vrele(lowerrootvp);
		free(xmp, M_UFSMNT);	/* XXX */
		return (error);
	}

	/*
	 * Keep a held reference to the root vnode.
	 * It is vrele'd in tcfs_unmount.
	 */
	tcfsm_rootvp = vp;
	tcfsm_rootvp->v_flag |= VROOT;
	xmp->tcfsm_rootvp = tcfsm_rootvp;
	if (TCFSVPTOLOWERVP(tcfsm_rootvp)->v_mount->mnt_flag & MNT_LOCAL)
		mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = (qaddr_t) xmp;
	vfs_getnewfsid(mp);

	(void) copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
	(void) copyinstr(args.target, mp->mnt_stat.f_mntfromname, MNAMELEN - 1, 
	    &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
#ifdef TCFS_DIAGNOSTIC
	printf("tcfs_mount: lower %s, alias at %s\n",
		mp->mnt_stat.f_mntfromname, mp->mnt_stat.f_mntonname);
#endif

        tcfs_error=tcfs_init_mp(xmp,&args);
        copyout((caddr_t)&args,data,sizeof(struct tcfs_args));
        return (tcfs_error);
}

/*
 * VFS start.  Nothing needed here - the start routine
 * on the underlying filesystem will have been called
 * when that filesystem was mounted.
 */
int
tcfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{

	return (0);
	/* return VFS_START(MOUNTTOTCFSMOUNT(mp)->tcfsm_vfs, flags, p); */
}

/*
 * Free reference to tcfs layer
 */
int
tcfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	struct vnode *tcfsm_rootvp = MOUNTTOTCFSMOUNT(mp)->tcfsm_rootvp;
	int error;
	int flags = 0;

#ifdef TCFS_DIAGNOSTIC
	printf("tcfs_unmount(mp = %p)\n", mp);
#endif

	if (mntflags & MNT_FORCE) {
		flags |= FORCECLOSE;
	}

	/*
	 * Clear out buffer cache.  I don't think we
	 * ever get anything cached at this level at the
	 * moment, but who knows...
	 */
#if 0
	mntflushbuf(mp, 0); 
	if (mntinvalbuf(mp, 1))
		return (EBUSY);
#endif
	if (tcfsm_rootvp->v_usecount > 1)
		return (EBUSY);
	if ((error = vflush(mp, tcfsm_rootvp, flags)) != 0)
		return (error);

#ifdef TCFS_DIAGNOSTIC
	vprint("alias root of lower", tcfsm_rootvp);
#endif	 
	/*
	 * Release reference on underlying root vnode
	 */
	vrele(tcfsm_rootvp);
	/*
	 * And blow it away for future re-use
	 */
	vgone(tcfsm_rootvp);
	/*
	 * Finally, throw away the tcfs_mount structure
	 */

        tcfs_keytab_dispose(MOUNTTOTCFSMOUNT(mp)->tcfs_uid_kt);
        tcfs_keytab_dispose(MOUNTTOTCFSMOUNT(mp)->tcfs_gid_kt);


	free(mp->mnt_data, M_UFSMNT);	/* XXX */
	mp->mnt_data = 0;
	return (0);
}

int
tcfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct vnode *vp;
	struct proc *p = curproc;

#ifdef TCFS_DIAGNOSTIC
	printf("tcfs_root(mp = %p, vp = %p->%p)\n", mp,
			MOUNTTOTCFSMOUNT(mp)->tcfsm_rootvp,
			TCFSVPTOLOWERVP(MOUNTTOTCFSMOUNT(mp)->tcfsm_rootvp)
			);
#endif

	/*
	 * Return locked reference to root.
	 */
	vp = MOUNTTOTCFSMOUNT(mp)->tcfsm_rootvp;
	VREF(vp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	*vpp = vp;
	return (0);
}

int
tcfs_quotactl(mp, cmd, uid, arg, p)
	struct mount *mp;
	int cmd;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{
	return VFS_QUOTACTL(MOUNTTOTCFSMOUNT(mp)->tcfsm_vfs, cmd, uid, arg, p);
}

int
tcfs_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{
	int error;
	struct statfs mstat;

#ifdef TCFS_DIAGNOSTIC
	printf("tcfs_statfs(mp = %p, vp = %p->%p)\n", mp,
			MOUNTTOTCFSMOUNT(mp)->tcfsm_rootvp,
			TCFSVPTOLOWERVP(MOUNTTOTCFSMOUNT(mp)->tcfsm_rootvp)
			);
#endif

	bzero(&mstat, sizeof(mstat));

	error = VFS_STATFS(MOUNTTOTCFSMOUNT(mp)->tcfsm_vfs, &mstat, p);
	if (error)
		return (error);

	/* now copy across the "interesting" information and fake the rest */
	sbp->f_flags = mstat.f_flags;
	sbp->f_bsize = mstat.f_bsize;
	sbp->f_iosize = mstat.f_iosize;
	sbp->f_blocks = mstat.f_blocks;
	sbp->f_bfree = mstat.f_bfree;
	sbp->f_bavail = mstat.f_bavail;
	sbp->f_files = mstat.f_files;
	sbp->f_ffree = mstat.f_ffree;
	if (sbp != &mp->mnt_stat) {
		bcopy(&mp->mnt_stat.f_fsid, &sbp->f_fsid, sizeof(sbp->f_fsid));
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	strncpy(sbp->f_fstypename, mp->mnt_vfc->vfc_name, MFSNAMELEN);
	return (0);
}

int
tcfs_sync(mp, waitfor, cred, p)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct proc *p;
{

	/*
	 * XXX - Assumes no data cached at tcfs layer.
	 */
	return (0);
}

int
tcfs_vget(mp, ino, vpp)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
{
	
	return VFS_VGET(MOUNTTOTCFSMOUNT(mp)->tcfsm_vfs, ino, vpp);
}

#define tcfs_sysctl ((int (*)(int *, u_int, void *, size_t *, void *, \
           size_t, struct proc *))eopnotsupp)

struct vfsops tcfs_vfsops = {
	tcfs_mount,
	tcfs_start,
	tcfs_unmount,
	tcfs_root,
	tcfs_quotactl,
	tcfs_statfs,
	tcfs_sync,
	tcfs_vget,
	tcfs_fhtovp,
	tcfs_vptofh,
	tcfs_init,
	tcfs_sysctl
};

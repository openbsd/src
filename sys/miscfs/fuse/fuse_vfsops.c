/* $OpenBSD: fuse_vfsops.c,v 1.33 2018/03/28 16:34:28 visa Exp $ */
/*
 * Copyright (c) 2012-2013 Sylvestre Gallon <ccna.syl@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/specdev.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/fusebuf.h>

#include "fusefs_node.h"
#include "fusefs.h"

int	fusefs_mount(struct mount *, const char *, void *, struct nameidata *,
	    struct proc *);
int	fusefs_start(struct mount *, int, struct proc *);
int	fusefs_unmount(struct mount *, int, struct proc *);
int	fusefs_root(struct mount *, struct vnode **);
int	fusefs_quotactl(struct mount *, int, uid_t, caddr_t, struct proc *);
int	fusefs_statfs(struct mount *, struct statfs *, struct proc *);
int	fusefs_sync(struct mount *, int, int, struct ucred *, struct proc *);
int	fusefs_vget(struct mount *, ino_t, struct vnode **);
int	fusefs_fhtovp(struct mount *, struct fid *, struct vnode **);
int	fusefs_vptofh(struct vnode *, struct fid *);
int	fusefs_init(struct vfsconf *);
int	fusefs_sysctl(int *, u_int, void *, size_t *, void *, size_t,
	    struct proc *);
int	fusefs_checkexp(struct mount *, struct mbuf *, int *,
	    struct ucred **);

const struct vfsops fusefs_vfsops = {
	fusefs_mount,
	fusefs_start,
	fusefs_unmount,
	fusefs_root,
	fusefs_quotactl,
	fusefs_statfs,
	fusefs_sync,
	fusefs_vget,
	fusefs_fhtovp,
	fusefs_vptofh,
	fusefs_init,
	fusefs_sysctl,
	fusefs_checkexp
};

struct pool fusefs_fbuf_pool;

int
fusefs_mount(struct mount *mp, const char *path, void *data,
    struct nameidata *ndp, struct proc *p)
{
	struct fusefs_mnt *fmp;
	struct fusebuf *fbuf;
	struct fusefs_args *args = data;
	struct vnode *vp;
	struct file *fp;
	int error = 0;

	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

	if ((fp = fd_getfile(p->p_fd, args->fd)) == NULL)
		return (EBADF);
	FREF(fp);

	if (fp->f_type != DTYPE_VNODE) {
		error = EINVAL;
		goto bad;
	}

	vp = fp->f_data;
	if (vp->v_type != VCHR) {
		error = EBADF;
		goto bad;
	}

	fmp = malloc(sizeof(*fmp), M_FUSEFS, M_WAITOK | M_ZERO);
	fmp->mp = mp;
	fmp->sess_init = 0;
	fmp->dev = vp->v_rdev;
	if (args->max_read > 0)
		fmp->max_read = MIN(args->max_read, FUSEBUFMAXSIZE);
	else
		fmp->max_read = FUSEBUFMAXSIZE;

	mp->mnt_data = fmp;
	mp->mnt_flag |= MNT_LOCAL;
	vfs_getnewfsid(mp);

	bzero(mp->mnt_stat.f_mntonname, MNAMELEN);
	strlcpy(mp->mnt_stat.f_mntonname, path, MNAMELEN);
	bzero(mp->mnt_stat.f_mntfromname, MNAMELEN);
	strlcpy(mp->mnt_stat.f_mntfromname, "fusefs", MNAMELEN);
	bzero(mp->mnt_stat.f_mntfromspec, MNAMELEN);
	strlcpy(mp->mnt_stat.f_mntfromspec, "fusefs", MNAMELEN);

	fuse_device_set_fmp(fmp, 1);
	fbuf = fb_setup(0, 0, FBT_INIT, p);

	/* cannot tsleep on mount */
	fuse_device_queue_fbuf(fmp->dev, fbuf);

bad:
	FRELE(fp, p);
	return (error);
}

int
fusefs_start(struct mount *mp, int flags, struct proc *p)
{
	return (0);
}

int
fusefs_unmount(struct mount *mp, int mntflags, struct proc *p)
{
	struct fusefs_mnt *fmp;
	struct fusebuf *fbuf;
	int flags = 0;
	int error;

	fmp = VFSTOFUSEFS(mp);

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	if ((error = vflush(mp, NULLVP, flags)))
		return (error);

	if (fmp->sess_init) {
		fmp->sess_init = 0;
		fbuf = fb_setup(0, 0, FBT_DESTROY, p);

		error = fb_queue(fmp->dev, fbuf);

		if (error)
			printf("fusefs: error %d on destroy\n", error);

		fb_delete(fbuf);
	}

	fuse_device_cleanup(fmp->dev, NULL);
	fuse_device_set_fmp(fmp, 0);
	free(fmp, M_FUSEFS, 0);
	mp->mnt_data = NULL;

	return (0);
}

int
fusefs_root(struct mount *mp, struct vnode **vpp)
{
	struct vnode *nvp;
	int error;

	if ((error = VFS_VGET(mp, FUSE_ROOTINO, &nvp)) != 0)
		return (error);

	nvp->v_type = VDIR;

	*vpp = nvp;
	return (0);
}

int
fusefs_quotactl(struct mount *mp, int cmds, uid_t uid, caddr_t arg,
    struct proc *p)
{
	return (EOPNOTSUPP);
}

int
fusefs_statfs(struct mount *mp, struct statfs *sbp, struct proc *p)
{
	struct fusefs_mnt *fmp;
	struct fusebuf *fbuf;
	int error;

	fmp = VFSTOFUSEFS(mp);

	copy_statfs_info(sbp, mp);

	if (fmp->sess_init) {
		fbuf = fb_setup(0, FUSE_ROOTINO, FBT_STATFS, p);

		error = fb_queue(fmp->dev, fbuf);

		if (error) {
			fb_delete(fbuf);
			return (error);
		}

		sbp->f_bavail = fbuf->fb_stat.f_bavail;
		sbp->f_bfree = fbuf->fb_stat.f_bfree;
		sbp->f_blocks = fbuf->fb_stat.f_blocks;
		sbp->f_files = fbuf->fb_stat.f_files;
		sbp->f_ffree = fbuf->fb_stat.f_ffree;
		sbp->f_favail = fbuf->fb_stat.f_favail;
		sbp->f_bsize = fbuf->fb_stat.f_frsize;
		sbp->f_iosize = fbuf->fb_stat.f_bsize;
		sbp->f_namemax = fbuf->fb_stat.f_namemax;
		fb_delete(fbuf);
	} else {
		sbp->f_bavail = 0;
		sbp->f_bfree = 0;
		sbp->f_blocks = 0;
		sbp->f_ffree = 0;
		sbp->f_favail = 0;
		sbp->f_files = 0;
		sbp->f_bsize = 0;
		sbp->f_iosize = 0;
		sbp->f_namemax = 0;
	}

	return (0);
}

int
fusefs_sync(struct mount *mp, int waitfor, int stall, struct ucred *cred,
    struct proc *p)
{
	return (0);
}

int
fusefs_vget(struct mount *mp, ino_t ino, struct vnode **vpp)
{
	struct fusefs_mnt *fmp;
	struct fusefs_node *ip;
	struct vnode *nvp;
	int i;
	int error;
retry:
	fmp = VFSTOFUSEFS(mp);
	/*
	 * check if vnode is in hash.
	 */
	if ((*vpp = ufs_ihashget(fmp->dev, ino)) != NULLVP)
		return (0);

	/*
	 * if not create it
	 */
	if ((error = getnewvnode(VT_FUSEFS, mp, &fusefs_vops, &nvp)) != 0) {
		printf("fusefs: getnewvnode error\n");
		*vpp = NULLVP;
		return (error);
	}

	ip = malloc(sizeof(*ip), M_FUSEFS, M_WAITOK | M_ZERO);
	rrw_init_flags(&ip->ufs_ino.i_lock, "fuseinode",
	    RWL_DUPOK | RWL_IS_VNODE);
	nvp->v_data = ip;
	ip->ufs_ino.i_vnode = nvp;
	ip->ufs_ino.i_dev = fmp->dev;
	ip->ufs_ino.i_number = ino;

	for (i = 0; i < FUFH_MAXTYPE; i++)
		ip->fufh[i].fh_type = FUFH_INVALID;

	error = ufs_ihashins(&ip->ufs_ino);
	if (error) {
		vrele(nvp);

		if (error == EEXIST)
			goto retry;

		return (error);
	}

	ip->ufs_ino.i_ump = (struct ufsmount *)fmp;

	if (ino == FUSE_ROOTINO)
		nvp->v_flag |= VROOT;

	*vpp = nvp;

	return (0);
}

int
fusefs_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	return (EINVAL);
}

int
fusefs_vptofh(struct vnode *vp, struct fid *fhp)
{
	return (EINVAL);
}

int
fusefs_init(struct vfsconf *vfc)
{
	pool_init(&fusefs_fbuf_pool, sizeof(struct fusebuf), 0, 0, PR_WAITOK,
	    "fmsg", NULL);

	return (0);
}

int
fusefs_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen, struct proc *p)
{
	extern int stat_fbufs_in, stat_fbufs_wait, stat_opened_fusedev;

	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case FUSEFS_OPENDEVS:
		return (sysctl_rdint(oldp, oldlenp, newp,
		    stat_opened_fusedev));
	case FUSEFS_INFBUFS:
		return (sysctl_rdint(oldp, oldlenp, newp, stat_fbufs_in));
	case FUSEFS_WAITFBUFS:
		return (sysctl_rdint(oldp, oldlenp, newp, stat_fbufs_wait));
	case FUSEFS_POOL_NBPAGES:
		return (sysctl_rdint(oldp, oldlenp, newp,
		    fusefs_fbuf_pool.pr_npages));
	default:
		return (EOPNOTSUPP);
	}
}

int
fusefs_checkexp(struct mount *mp, struct mbuf *nam, int *extflagsp,
    struct ucred **credanonp)
{
	return (EOPNOTSUPP);
}

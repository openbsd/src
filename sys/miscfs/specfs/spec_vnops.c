/*	$OpenBSD: spec_vnops.c,v 1.40 2007/05/29 06:28:15 otto Exp $	*/
/*	$NetBSD: spec_vnops.c,v 1.29 1996/04/22 01:42:38 christos Exp $	*/

/*
 * Copyright (c) 1989, 1993
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
 *	@(#)spec_vnops.c	8.8 (Berkeley) 11/21/94
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/disklabel.h>
#include <sys/lockf.h>
#include <sys/poll.h>

#include <miscfs/specfs/specdev.h>

#define v_lastr v_specinfo->si_lastr

struct vnode *speclisth[SPECHSZ];

int (**spec_vnodeop_p)(void *);
struct vnodeopv_entry_desc spec_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, spec_lookup },		/* lookup */
	{ &vop_create_desc, spec_create },		/* create */
	{ &vop_mknod_desc, spec_mknod },		/* mknod */
	{ &vop_open_desc, spec_open },			/* open */
	{ &vop_close_desc, spec_close },		/* close */
	{ &vop_access_desc, spec_access },		/* access */
	{ &vop_getattr_desc, spec_getattr },		/* getattr */
	{ &vop_setattr_desc, spec_setattr },		/* setattr */
	{ &vop_read_desc, spec_read },			/* read */
	{ &vop_write_desc, spec_write },		/* write */
	{ &vop_ioctl_desc, spec_ioctl },		/* ioctl */
	{ &vop_poll_desc, spec_poll },			/* poll */
	{ &vop_kqfilter_desc, spec_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, spec_revoke },              /* revoke */
	{ &vop_fsync_desc, spec_fsync },		/* fsync */
	{ &vop_remove_desc, spec_remove },		/* remove */
	{ &vop_link_desc, spec_link },			/* link */
	{ &vop_rename_desc, spec_rename },		/* rename */
	{ &vop_mkdir_desc, spec_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, spec_rmdir },		/* rmdir */
	{ &vop_symlink_desc, spec_symlink },		/* symlink */
	{ &vop_readdir_desc, spec_readdir },		/* readdir */
	{ &vop_readlink_desc, spec_readlink },		/* readlink */
	{ &vop_abortop_desc, spec_abortop },		/* abortop */
	{ &vop_inactive_desc, spec_inactive },		/* inactive */
	{ &vop_reclaim_desc, spec_reclaim },		/* reclaim */
	{ &vop_lock_desc, spec_lock },			/* lock */
	{ &vop_unlock_desc, spec_unlock },		/* unlock */
	{ &vop_bmap_desc, spec_bmap },			/* bmap */
	{ &vop_strategy_desc, spec_strategy },		/* strategy */
	{ &vop_print_desc, spec_print },		/* print */
	{ &vop_islocked_desc, spec_islocked },		/* islocked */
	{ &vop_pathconf_desc, spec_pathconf },		/* pathconf */
	{ &vop_advlock_desc, spec_advlock },		/* advlock */
	{ &vop_bwrite_desc, spec_bwrite },		/* bwrite */
	{ NULL, NULL }
};
struct vnodeopv_desc spec_vnodeop_opv_desc =
	{ &spec_vnodeop_p, spec_vnodeop_entries };

int
spec_vnoperate(void *v)
{
	struct vop_generic_args *ap = v;

	return (VOCALL(spec_vnodeop_p, ap->a_desc->vdesc_offset, ap));
}

/*
 * Trivial lookup routine that always fails.
 */
int
spec_lookup(v)
	void *v;
{
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;

	*ap->a_vpp = NULL;
	return (ENOTDIR);
}

/*
 * Open a special file.
 */
/* ARGSUSED */
int
spec_open(v)
	void *v;
{
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct proc *p = ap->a_p;
	struct vnode *vp = ap->a_vp;
	struct vnode *bvp;
	dev_t bdev;
	dev_t dev = (dev_t)vp->v_rdev;
	register int maj = major(dev);
	int error;

	/*
	 * Don't allow open if fs is mounted -nodev.
	 */
	if (vp->v_mount && (vp->v_mount->mnt_flag & MNT_NODEV))
		return (ENXIO);

	switch (vp->v_type) {

	case VCHR:
		if ((u_int)maj >= nchrdev)
			return (ENXIO);
		if (ap->a_cred != FSCRED && (ap->a_mode & FWRITE)) {
			/*
			 * When running in very secure mode, do not allow
			 * opens for writing of any disk character devices.
			 */
			if (securelevel >= 2 && cdevsw[maj].d_type == D_DISK)
				return (EPERM);
			/*
			 * When running in secure mode, do not allow opens
			 * for writing of /dev/mem, /dev/kmem, or character
			 * devices whose corresponding block devices are
			 * currently mounted.
			 */
			if (securelevel >= 1) {
				if ((bdev = chrtoblk(dev)) != NODEV &&
				    vfinddev(bdev, VBLK, &bvp) &&
				    bvp->v_usecount > 0 &&
				    (error = vfs_mountedon(bvp)))
					return (error);
				if (iskmemdev(dev))
					return (EPERM);
			}
		}
		if (cdevsw[maj].d_type == D_TTY)
			vp->v_flag |= VISTTY;
		if (cdevsw[maj].d_flags & D_CLONE)
			return (spec_open_clone(ap));
		VOP_UNLOCK(vp, 0, p);
		error = (*cdevsw[maj].d_open)(dev, ap->a_mode, S_IFCHR, ap->a_p);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		return (error);

	case VBLK:
		if ((u_int)maj >= nblkdev)
			return (ENXIO);
		/*
		 * When running in very secure mode, do not allow
		 * opens for writing of any disk block devices.
		 */
		if (securelevel >= 2 && ap->a_cred != FSCRED &&
		    (ap->a_mode & FWRITE) && bdevsw[maj].d_type == D_DISK)
			return (EPERM);
		/*
		 * Do not allow opens of block devices that are
		 * currently mounted.
		 */
		if ((error = vfs_mountedon(vp)) != 0)
			return (error);
		return ((*bdevsw[maj].d_open)(dev, ap->a_mode, S_IFBLK, ap->a_p));
	case VNON:
	case VLNK:
	case VDIR:
	case VREG:
	case VBAD:
	case VFIFO:
	case VSOCK:
		break;
	}
	return (0);
}

/*
 * Vnode op for read
 */
/* ARGSUSED */
int
spec_read(v)
	void *v;
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;
	register struct uio *uio = ap->a_uio;
 	struct proc *p = uio->uio_procp;
	struct buf *bp;
	daddr64_t bn, nextbn;
	long bsize, bscale;
	struct partinfo dpart;
	int n, on, majordev;
	int (*ioctl)(dev_t, u_long, caddr_t, int, struct proc *);
	int error = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("spec_read mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("spec_read proc");
#endif
	if (uio->uio_resid == 0)
		return (0);

	switch (vp->v_type) {

	case VCHR:
		VOP_UNLOCK(vp, 0, p);
		error = (*cdevsw[major(vp->v_rdev)].d_read)
			(vp->v_rdev, uio, ap->a_ioflag);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		return (error);

	case VBLK:
		if (uio->uio_offset < 0)
			return (EINVAL);
		bsize = BLKDEV_IOSIZE;
		if ((majordev = major(vp->v_rdev)) < nblkdev &&
		    (ioctl = bdevsw[majordev].d_ioctl) != NULL &&
		    (*ioctl)(vp->v_rdev, DIOCGPART, (caddr_t)&dpart, FREAD, p) == 0) {
			u_int32_t frag =
			    DISKLABELV1_FFS_FRAG(dpart.part->p_fragblock);
			u_int32_t fsize =
			    DISKLABELV1_FFS_FSIZE(dpart.part->p_fragblock);
			if (dpart.part->p_fstype == FS_BSDFFS && frag != 0 &&
			    fsize != 0)
				bsize = frag * fsize;
		}
		bscale = btodb(bsize);
		do {
			bn = btodb(uio->uio_offset) & ~(bscale - 1);
			on = uio->uio_offset % bsize;
			n = min((unsigned)(bsize - on), uio->uio_resid);
			if (vp->v_lastr + bscale == bn) {
				nextbn = bn + bscale;
				error = breadn(vp, bn, (int)bsize, &nextbn,
					(int *)&bsize, 1, NOCRED, &bp);
			} else
				error = bread(vp, bn, (int)bsize, NOCRED, &bp);
			vp->v_lastr = bn;
			n = min(n, bsize - bp->b_resid);
			if (error) {
				brelse(bp);
				return (error);
			}
			error = uiomove((char *)bp->b_data + on, n, uio);
			brelse(bp);
		} while (error == 0 && uio->uio_resid > 0 && n != 0);
		return (error);

	default:
		panic("spec_read type");
	}
	/* NOTREACHED */
}

int
spec_inactive(v)
	void *v;
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap = v;

	VOP_UNLOCK(ap->a_vp, 0, ap->a_p);
	return (0);
}

/*
 * Vnode op for write
 */
/* ARGSUSED */
int
spec_write(v)
	void *v;
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;
	register struct uio *uio = ap->a_uio;
	struct proc *p = uio->uio_procp;
	struct buf *bp;
	daddr_t bn;
	long bsize, bscale;
	struct partinfo dpart;
	int n, on, majordev;
	int (*ioctl)(dev_t, u_long, caddr_t, int, struct proc *);
	int error = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("spec_write mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("spec_write proc");
#endif

	switch (vp->v_type) {

	case VCHR:
		VOP_UNLOCK(vp, 0, p);
		error = (*cdevsw[major(vp->v_rdev)].d_write)
			(vp->v_rdev, uio, ap->a_ioflag);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		return (error);

	case VBLK:
		if (uio->uio_resid == 0)
			return (0);
		if (uio->uio_offset < 0)
			return (EINVAL);
		bsize = BLKDEV_IOSIZE;
		if ((majordev = major(vp->v_rdev)) < nblkdev &&
		    (ioctl = bdevsw[majordev].d_ioctl) != NULL &&
		    (*ioctl)(vp->v_rdev, DIOCGPART, (caddr_t)&dpart, FREAD, p) == 0) {
			u_int32_t frag =
			    DISKLABELV1_FFS_FRAG(dpart.part->p_fragblock);
			u_int32_t fsize =
			    DISKLABELV1_FFS_FSIZE(dpart.part->p_fragblock);
			if (dpart.part->p_fstype == FS_BSDFFS && frag != 0 &&
			    fsize != 0)
				bsize = frag * fsize;
		}
		bscale = btodb(bsize);
		do {
			bn = btodb(uio->uio_offset) & ~(bscale - 1);
			on = uio->uio_offset % bsize;
			n = min((unsigned)(bsize - on), uio->uio_resid);
			error = bread(vp, bn, bsize, NOCRED, &bp);
			n = min(n, bsize - bp->b_resid);
			if (error) {
				brelse(bp);
				return (error);
			}
			error = uiomove((char *)bp->b_data + on, n, uio);
			if (n + on == bsize)
				bawrite(bp);
			else
				bdwrite(bp);
		} while (error == 0 && uio->uio_resid > 0 && n != 0);
		return (error);

	default:
		panic("spec_write type");
	}
	/* NOTREACHED */
}

/*
 * Device ioctl operation.
 */
/* ARGSUSED */
int
spec_ioctl(v)
	void *v;
{
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	dev_t dev = ap->a_vp->v_rdev;
	int maj = major(dev);

	switch (ap->a_vp->v_type) {

	case VCHR:
		return ((*cdevsw[maj].d_ioctl)(dev, ap->a_command, ap->a_data,
		    ap->a_fflag, ap->a_p));

	case VBLK:
		return ((*bdevsw[maj].d_ioctl)(dev, ap->a_command, ap->a_data,
		    ap->a_fflag, ap->a_p));

	default:
		panic("spec_ioctl");
		/* NOTREACHED */
	}
}

/* ARGSUSED */
int
spec_poll(v)
	void *v;
{
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int  a_events;
		struct proc *a_p;
	} */ *ap = v;
	register dev_t dev;

	switch (ap->a_vp->v_type) {

	default:
		return (ap->a_events &
		    (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));

	case VCHR:
		dev = ap->a_vp->v_rdev;
		return (*cdevsw[major(dev)].d_poll)(dev, ap->a_events, ap->a_p);
	}
}
/* ARGSUSED */
int
spec_kqfilter(v)
	void *v;
{
	struct vop_kqfilter_args /* {
		struct vnode *a_vp;
		struct knote *a_kn;
	} */ *ap = v;

	dev_t dev;

	dev = ap->a_vp->v_rdev;
	if (cdevsw[major(dev)].d_flags & D_KQFILTER)
		return (*cdevsw[major(dev)].d_kqfilter)(dev, ap->a_kn);
	return (1);
}

/*
 * Synch buffers associated with a block device
 */
/* ARGSUSED */
int
spec_fsync(v)
	void *v;
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int  a_waitfor;
		struct proc *a_p;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;
	register struct buf *bp;
	struct buf *nbp;
	int s;

	if (vp->v_type == VCHR)
		return (0);
	/*
	 * Flush all dirty buffers associated with a block device.
	 */
loop:
	s = splbio();
	for (bp = LIST_FIRST(&vp->v_dirtyblkhd);
	    bp != LIST_END(&vp->v_dirtyblkhd); bp = nbp) {
		nbp = LIST_NEXT(bp, b_vnbufs);
		if ((bp->b_flags & B_BUSY))
			continue;
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("spec_fsync: not dirty");
		bremfree(bp);
		bp->b_flags |= B_BUSY;
		splx(s);
		bawrite(bp);
		goto loop;
	}
	if (ap->a_waitfor == MNT_WAIT) {
		vwaitforio (vp, 0, "spec_fsync", 0);

#ifdef DIAGNOSTIC
		if (!LIST_EMPTY(&vp->v_dirtyblkhd)) {
			splx(s);
			vprint("spec_fsync: dirty", vp);
			goto loop;
		}
#endif
	}
	splx(s);
	return (0);
}

int
spec_strategy(v)
	void *v;
{
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap = v;
	struct buf *bp = ap->a_bp;
	int maj = major(bp->b_dev);
	
	if (LIST_FIRST(&bp->b_dep) != NULL)
		buf_start(bp);

	(*bdevsw[maj].d_strategy)(bp);
	return (0);
}

/*
 * This is a noop, simply returning what one has been given.
 */
int
spec_bmap(v)
	void *v;
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap = v;

	if (ap->a_vpp != NULL)
		*ap->a_vpp = ap->a_vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn;
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
	
	return (0);
}

/*
 * Device close routine
 */
/* ARGSUSED */
int
spec_close(v)
	void *v;
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;
	dev_t dev = vp->v_rdev;
	int (*devclose)(dev_t, int, int, struct proc *);
	int mode, error;

	switch (vp->v_type) {

	case VCHR:
		/*
		 * Hack: a tty device that is a controlling terminal
		 * has a reference from the session structure.
		 * We cannot easily tell that a character device is
		 * a controlling terminal, unless it is the closing
		 * process' controlling terminal.  In that case,
		 * if the reference count is 2 (this last descriptor
		 * plus the session), release the reference from the session.
		 */
		if (vcount(vp) == 2 && ap->a_p &&
		    vp == ap->a_p->p_session->s_ttyvp) {
			vrele(vp);
			ap->a_p->p_session->s_ttyvp = NULL;
		}
		/*
		 * If the vnode is locked, then we are in the midst
		 * of forcably closing the device, otherwise we only
		 * close on last reference.
		 */
		if (vcount(vp) > 1 && (vp->v_flag & VXLOCK) == 0)
			return (0);
		if (cdevsw[major(dev)].d_flags & D_CLONE)
			return (spec_close_clone(ap));
		devclose = cdevsw[major(dev)].d_close;
		mode = S_IFCHR;
		break;

	case VBLK:
		/*
		 * On last close of a block device (that isn't mounted)
		 * we must invalidate any in core blocks, so that
		 * we can, for instance, change floppy disks. In order to do
		 * that, we must lock the vnode. If we are coming from
		 * vclean(), the vnode is already locked.
		 */
		if (!(vp->v_flag & VXLOCK))
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, ap->a_p);
		error = vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p, 0, 0);
		if (!(vp->v_flag & VXLOCK))
			VOP_UNLOCK(vp, 0, ap->a_p);
		if (error)
			return (error);
		/*
		 * We do not want to really close the device if it
		 * is still in use unless we are trying to close it
		 * forcibly. Since every use (buffer, vnode, swap, cmap)
		 * holds a reference to the vnode, and because we mark
		 * any other vnodes that alias this device, when the
		 * sum of the reference counts on all the aliased
		 * vnodes descends to one, we are on last close.
		 */
		if (vcount(vp) > 1 && (vp->v_flag & VXLOCK) == 0)
			return (0);
		devclose = bdevsw[major(dev)].d_close;
		mode = S_IFBLK;
		break;

	default:
		panic("spec_close: not special");
	}

	return ((*devclose)(dev, ap->a_fflag, mode, ap->a_p));
}

/*
 * Print out the contents of a special device vnode.
 */
int
spec_print(v)
	void *v;
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;

	printf("tag VT_NON, dev %d, %d\n", major(ap->a_vp->v_rdev),
		minor(ap->a_vp->v_rdev));
	return 0;
}

/*
 * Return POSIX pathconf information applicable to special devices.
 */
int
spec_pathconf(v)
	void *v;
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return (0);
	case _PC_MAX_CANON:
		*ap->a_retval = MAX_CANON;
		return (0);
	case _PC_MAX_INPUT:
		*ap->a_retval = MAX_INPUT;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_VDISABLE:
		*ap->a_retval = _POSIX_VDISABLE;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Special device advisory byte-level locks.
 */
/* ARGSUSED */
int
spec_advlock(v)
	void *v;
{
	struct vop_advlock_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;

	return (lf_advlock(&vp->v_speclockf, (off_t)0, ap->a_id,
		ap->a_op, ap->a_fl, ap->a_flags));
}

/*
 * Special device failed operation
 */
/*ARGSUSED*/
int
spec_ebadf(v)
	void *v;
{

	return (EBADF);
}

/*
 * Special device bad operation
 */
/*ARGSUSED*/
int
spec_badop(v)
	void *v;
{

	panic("spec_badop called");
	/* NOTREACHED */
}

/*	$OpenBSD: mfs_vnops.c,v 1.10 1999/01/12 04:24:10 millert Exp $	*/
/*	$NetBSD: mfs_vnops.c,v 1.8 1996/03/17 02:16:32 christos Exp $	*/

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
 *	@(#)mfs_vnops.c	8.5 (Berkeley) 7/28/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/map.h>
#include <sys/vnode.h>
#include <sys/malloc.h>

#include <miscfs/specfs/specdev.h>

#include <machine/vmparam.h>

#include <ufs/mfs/mfsnode.h>
#include <ufs/mfs/mfsiom.h>
#include <ufs/mfs/mfs_extern.h>

/*
 * mfs vnode operations.
 */
int (**mfs_vnodeop_p) __P((void *));
struct vnodeopv_entry_desc mfs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, mfs_lookup },		/* lookup */
	{ &vop_create_desc, mfs_create },		/* create */
	{ &vop_mknod_desc, mfs_mknod },			/* mknod */
	{ &vop_open_desc, mfs_open },			/* open */
	{ &vop_close_desc, mfs_close },			/* close */
	{ &vop_access_desc, mfs_access },		/* access */
	{ &vop_getattr_desc, mfs_getattr },		/* getattr */
	{ &vop_setattr_desc, mfs_setattr },		/* setattr */
	{ &vop_read_desc, mfs_read },			/* read */
	{ &vop_write_desc, mfs_write },			/* write */
	{ &vop_ioctl_desc, mfs_ioctl },			/* ioctl */
	{ &vop_select_desc, mfs_select },		/* select */
	{ &vop_revoke_desc, mfs_revoke },               /* revoke */
	{ &vop_mmap_desc, mfs_mmap },			/* mmap */
	{ &vop_fsync_desc, spec_fsync },		/* fsync */
	{ &vop_seek_desc, mfs_seek },			/* seek */
	{ &vop_remove_desc, mfs_remove },		/* remove */
	{ &vop_link_desc, mfs_link },			/* link */
	{ &vop_rename_desc, mfs_rename },		/* rename */
	{ &vop_mkdir_desc, mfs_mkdir },			/* mkdir */
	{ &vop_rmdir_desc, mfs_rmdir },			/* rmdir */
	{ &vop_symlink_desc, mfs_symlink },		/* symlink */
	{ &vop_readdir_desc, mfs_readdir },		/* readdir */
	{ &vop_readlink_desc, mfs_readlink },		/* readlink */
	{ &vop_abortop_desc, mfs_abortop },		/* abortop */
	{ &vop_inactive_desc, mfs_inactive },		/* inactive */
	{ &vop_reclaim_desc, mfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, mfs_lock },			/* lock */
	{ &vop_unlock_desc, mfs_unlock },		/* unlock */
	{ &vop_bmap_desc, mfs_bmap },			/* bmap */
	{ &vop_strategy_desc, mfs_strategy },		/* strategy */
	{ &vop_print_desc, mfs_print },			/* print */
	{ &vop_islocked_desc, mfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, mfs_pathconf },		/* pathconf */
	{ &vop_advlock_desc, mfs_advlock },		/* advlock */
	{ &vop_blkatoff_desc, mfs_blkatoff },		/* blkatoff */
	{ &vop_valloc_desc, mfs_valloc },		/* valloc */
	{ &vop_vfree_desc, mfs_vfree },			/* vfree */
	{ &vop_truncate_desc, mfs_truncate },		/* truncate */
	{ &vop_update_desc, mfs_update },		/* update */
	{ &vop_bwrite_desc, mfs_bwrite },		/* bwrite */
	{ (struct vnodeop_desc*)NULL, (int(*) __P((void *)))NULL }
};
struct vnodeopv_desc mfs_vnodeop_opv_desc =
	{ &mfs_vnodeop_p, mfs_vnodeop_entries };

/*
 * Vnode Operations.
 *
 * Open called to allow memory filesystem to initialize and
 * validate before actual IO. Record our process identifier
 * so we can tell when we are doing I/O to ourself.
 */
/* ARGSUSED */
int
mfs_open(v)
	void *v;
{
#ifdef DIAGNOSTIC
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;

	if (ap->a_vp->v_type != VBLK) {
		panic("mfs_open not VBLK");
		/* NOTREACHED */
	}
#endif
	return (0);
}

/*
 * Ioctl operation.
 */
/* ARGSUSED */
int
mfs_ioctl(v)
	void *v;
{
#if 0
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
#endif

	return (ENOTTY);
}

/*
 * Pass I/O requests to the memory filesystem process.
 */
int
mfs_strategy(v)
	void *v;
{
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap = v;
	register struct buf *bp = ap->a_bp;
	register struct mfsnode *mfsp;
	struct vnode *vp;
	struct proc *p = curproc;		/* XXX */

#ifdef DIAGNOSTIC
	if (!vfinddev(bp->b_dev, VBLK, &vp) || vp->v_usecount == 0)
		panic("mfs_strategy: bad dev");
#endif
	mfsp = VTOMFS(vp);
	/* check for mini-root access */
	if (mfsp->mfs_pid == 0) {
		caddr_t base;

		base = mfsp->mfs_baseoff + (bp->b_blkno << DEV_BSHIFT);
		if (bp->b_flags & B_READ)
			bcopy(base, bp->b_data, bp->b_bcount);
		else
			bcopy(bp->b_data, base, bp->b_bcount);
		biodone(bp);
	} else if (mfsp->mfs_pid == p->p_pid) {
		mfs_doio(bp, mfsp->mfs_baseoff);
	} else {
		bp->b_actf = mfsp->mfs_buflist;
		mfsp->mfs_buflist = bp;
		wakeup((caddr_t)vp);
	}
	return (0);
}

/*
 * Memory file system I/O.
 *
 * Trivial on the HP since buffer has already been mapping into KVA space.
 */
void
mfs_doio(bp, base)
	register struct buf *bp;
	caddr_t base;
{

	base += (bp->b_blkno << DEV_BSHIFT);
	if (bp->b_flags & B_READ)
		bp->b_error = copyin(base, bp->b_data, bp->b_bcount);
	else
		bp->b_error = copyout(bp->b_data, base, bp->b_bcount);
	if (bp->b_error)
		bp->b_flags |= B_ERROR;
	biodone(bp);
}

/*
 * This is a noop, simply returning what one has been given.
 */
int
mfs_bmap(v)
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
 * Memory filesystem close routine
 */
/* ARGSUSED */
int
mfs_close(v)
	void *v;
{
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;
	register struct mfsnode *mfsp = VTOMFS(vp);
	register struct buf *bp;
	int error;

	/*
	 * Finish any pending I/O requests.
	 */
	while ((bp = mfsp->mfs_buflist) != NULL) {
		mfsp->mfs_buflist = bp->b_actf;
		mfs_doio(bp, mfsp->mfs_baseoff);
		wakeup((caddr_t)bp);
	}
	/*
	 * On last close of a memory filesystem
	 * we must invalidate any in core blocks, so that
	 * we can, free up its vnode.
	 */
	if ((error = vinvalbuf(vp, 1, ap->a_cred, ap->a_p, 0, 0)) != 0)
		return (error);
#ifdef DIAGNOSTIC
	/*
	 * There should be no way to have any more uses of this
	 * vnode, so if we find any other uses, it is a panic.
	 */
	if (vp->v_usecount > 1)
		printf("mfs_close: ref count %d > 1\n", vp->v_usecount);
	if (mfsp->mfs_buflist)
		printf("mfs_close: dirty buffers\n");
	if (vp->v_usecount > 1 || mfsp->mfs_buflist)
		panic("mfs_close");
#endif
	/*
	 * Send a request to the filesystem server to exit.
	 */
	mfsp->mfs_buflist = (struct buf *)(-1);
	wakeup((caddr_t)vp);
	return (0);
}

/*
 * Memory filesystem inactive routine
 */
/* ARGSUSED */
int
mfs_inactive(v)
	void *v;
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap = v;
#ifdef DIAGNOSTIC
	register struct mfsnode *mfsp = VTOMFS(ap->a_vp);

	if (mfsp->mfs_buflist && mfsp->mfs_buflist != (struct buf *)(-1))
		panic("mfs_inactive: not inactive (mfs_buflist %p)",
			mfsp->mfs_buflist);
#endif
	VOP_UNLOCK(ap->a_vp, 0, ap->a_p);
	return (0);
}

/*
 * Reclaim a memory filesystem devvp so that it can be reused.
 */
int
mfs_reclaim(v)
	void *v;
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;

	FREE(vp->v_data, M_MFSNODE);
	vp->v_data = NULL;
	return (0);
}

/*
 * Print out the contents of an mfsnode.
 */
int
mfs_print(v)
	void *v;
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	register struct mfsnode *mfsp = VTOMFS(ap->a_vp);

	printf("tag VT_MFS, pid %d, base %p, size %ld\n", mfsp->mfs_pid,
	    mfsp->mfs_baseoff, mfsp->mfs_size);
	return (0);
}

/*
 * Block device bad operation
 */
int
mfs_badop(v)
	void *v;
{

	panic("mfs_badop called");
	/* NOTREACHED */
}

/*
 * Memory based filesystem initialization.
 */
int
mfs_init(vfsp)
	struct vfsconf *vfsp;
{
	return  (0);
}

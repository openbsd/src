/*	$OpenBSD: mfs_vnops.c,v 1.41 2011/04/05 14:14:07 thib Exp $	*/
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
 *	@(#)mfs_vnops.c	8.5 (Berkeley) 7/28/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/malloc.h>

#include <miscfs/specfs/specdev.h>

#include <machine/vmparam.h>

#include <ufs/mfs/mfsnode.h>
#include <ufs/mfs/mfs_extern.h>

/* mfs vnode operations. */
struct vops mfs_vops = {
        .vop_lookup     = mfs_badop,
        .vop_create     = mfs_badop,
        .vop_mknod      = mfs_badop,
        .vop_open       = mfs_open,
        .vop_close      = mfs_close,
        .vop_access     = mfs_badop,
        .vop_getattr    = mfs_badop,
        .vop_setattr    = mfs_badop,
        .vop_read       = mfs_badop,
        .vop_write      = mfs_badop,
        .vop_ioctl      = mfs_ioctl,
        .vop_poll       = mfs_badop,
        .vop_revoke     = mfs_revoke,
        .vop_fsync      = spec_fsync,
        .vop_remove     = mfs_badop,
        .vop_link       = mfs_badop,
        .vop_rename     = mfs_badop,
        .vop_mkdir      = mfs_badop,
        .vop_rmdir      = mfs_badop,
        .vop_symlink    = mfs_badop,
        .vop_readdir    = mfs_badop,
        .vop_readlink   = mfs_badop,
        .vop_abortop    = mfs_badop,
        .vop_inactive   = mfs_inactive,
        .vop_reclaim    = mfs_reclaim,
        .vop_lock       = vop_generic_lock,
        .vop_unlock     = vop_generic_unlock,
        .vop_bmap       = vop_generic_bmap,
        .vop_strategy   = mfs_strategy,
        .vop_print      = mfs_print,
        .vop_islocked   = vop_generic_islocked,
        .vop_pathconf   = mfs_badop,
        .vop_advlock    = mfs_badop,
        .vop_bwrite     = vop_generic_bwrite
};

/*
 * Vnode Operations.
 *
 * Open called to allow memory filesystem to initialize and
 * validate before actual IO. Record our process identifier
 * so we can tell when we are doing I/O to ourself.
 */
/* ARGSUSED */
int
mfs_open(void *v)
{
#ifdef DIAGNOSTIC
	struct vop_open_args *ap = v;

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
mfs_ioctl(void *v)
{
#if 0
	struct vop_ioctl_args *ap = v;
#endif

	return (ENOTTY);
}

/*
 * Pass I/O requests to the memory filesystem process.
 */
int
mfs_strategy(void *v)
{
	struct vop_strategy_args *ap = v;
	struct buf *bp = ap->a_bp;
	struct mfsnode *mfsp;
	struct vnode *vp;
	struct proc *p = curproc;
	int s;

	if (!vfinddev(bp->b_dev, VBLK, &vp) || vp->v_usecount == 0)
		panic("mfs_strategy: bad dev");

	mfsp = VTOMFS(vp);
	if (p != NULL && mfsp->mfs_pid == p->p_pid) {
		mfs_doio(mfsp, bp);
	} else {
		s = splbio();
		bp->b_actf = mfsp->mfs_buflist;
		mfsp->mfs_buflist = bp;
		splx(s);
		wakeup((caddr_t)vp);
	}
	return (0);
}

/*
 * Memory file system I/O.
 *
 * Trivial on the HP since buffer has already been mapped into KVA space.
 */
void
mfs_doio(struct mfsnode *mfsp, struct buf *bp)
{
	caddr_t base;
	long offset = bp->b_blkno << DEV_BSHIFT;
	int s;

	if (bp->b_bcount > mfsp->mfs_size - offset)
		bp->b_bcount = mfsp->mfs_size - offset;

	base = mfsp->mfs_baseoff + offset;
	if (bp->b_flags & B_READ)
		bp->b_error = copyin(base, bp->b_data, bp->b_bcount);
	else
		bp->b_error = copyout(bp->b_data, base, bp->b_bcount);
	if (bp->b_error)
		bp->b_flags |= B_ERROR;
	else
		bp->b_resid = 0;
	s = splbio();
	biodone(bp);
	splx(s);
}

/*
 * Memory filesystem close routine
 */
/* ARGSUSED */
int
mfs_close(void *v)
{
	struct vop_close_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct mfsnode *mfsp = VTOMFS(vp);
	struct buf *bp;
	int error, s;

	/*
	 * Finish any pending I/O requests.
	 */
	while (1) {
		s = splbio();
		bp = mfsp->mfs_buflist;
		if (bp == NULL) {
			splx(s);
			break;
		}
		mfsp->mfs_buflist = bp->b_actf;
		splx(s);
		mfs_doio(mfsp, bp);
		wakeup((caddr_t)bp);
	}
	/*
	 * On last close of a memory filesystem
	 * we must invalidate any in core blocks, so that
	 * we can free up its vnode.
	 */
	if ((error = vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p, 0, 0)) != 0)
		return (error);
#ifdef DIAGNOSTIC
	/*
	 * There should be no way to have any more buffers on this vnode.
	 */
	if (mfsp->mfs_buflist)
		printf("mfs_close: dirty buffers\n");
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
mfs_inactive(void *v)
{
	struct vop_inactive_args *ap = v;
#ifdef DIAGNOSTIC
	struct mfsnode *mfsp = VTOMFS(ap->a_vp);

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
mfs_reclaim(void *v)
{
	struct vop_reclaim_args *ap = v;
	struct vnode *vp = ap->a_vp;

	free(vp->v_data, M_MFSNODE);
	vp->v_data = NULL;
	return (0);
}

/*
 * Print out the contents of an mfsnode.
 */
int
mfs_print(void *v)
{
	struct vop_print_args *ap = v;
	struct mfsnode *mfsp = VTOMFS(ap->a_vp);

	printf("tag VT_MFS, pid %d, base %p, size %ld\n", mfsp->mfs_pid,
	    mfsp->mfs_baseoff, mfsp->mfs_size);
	return (0);
}

/*
 * Block device bad operation
 */
int
mfs_badop(void *v)
{
	panic("mfs_badop called");
	/* NOTREACHED */
}

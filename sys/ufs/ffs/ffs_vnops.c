/*	$OpenBSD: ffs_vnops.c,v 1.32 2005/02/17 18:07:37 jfb Exp $	*/
/*	$NetBSD: ffs_vnops.c,v 1.7 1996/05/11 18:27:24 mycroft Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)ffs_vnops.c	8.10 (Berkeley) 8/10/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/signalvar.h>
#include <sys/pool.h>

#include <uvm/uvm_extern.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/fifofs/fifo.h>

#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ufs/ufsmount.h>

#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>

/* Global vfs data structures for ufs. */
int (**ffs_vnodeop_p)(void *);
struct vnodeopv_entry_desc ffs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, ufs_lookup },		/* lookup */
	{ &vop_create_desc, ufs_create },		/* create */
	{ &vop_whiteout_desc, ufs_whiteout },		/* whiteout */
	{ &vop_mknod_desc, ufs_mknod },			/* mknod */
	{ &vop_open_desc, ufs_open },			/* open */
	{ &vop_close_desc, ufs_close },			/* close */
	{ &vop_access_desc, ufs_access },		/* access */
	{ &vop_getattr_desc, ufs_getattr },		/* getattr */
	{ &vop_setattr_desc, ufs_setattr },		/* setattr */
	{ &vop_read_desc, ffs_read },			/* read */
	{ &vop_write_desc, ffs_write },			/* write */
	{ &vop_lease_desc, ufs_lease_check },		/* lease */
	{ &vop_ioctl_desc, ufs_ioctl },			/* ioctl */
	{ &vop_poll_desc, ufs_poll },			/* poll */
	{ &vop_kqfilter_desc, ufs_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, ufs_revoke },		/* revoke */
	{ &vop_fsync_desc, ffs_fsync },			/* fsync */
	{ &vop_remove_desc, ufs_remove },		/* remove */
	{ &vop_link_desc, ufs_link },			/* link */
	{ &vop_rename_desc, ufs_rename },		/* rename */
	{ &vop_mkdir_desc, ufs_mkdir },			/* mkdir */
	{ &vop_rmdir_desc, ufs_rmdir },			/* rmdir */
	{ &vop_symlink_desc, ufs_symlink },		/* symlink */
	{ &vop_readdir_desc, ufs_readdir },		/* readdir */
	{ &vop_readlink_desc, ufs_readlink },		/* readlink */
	{ &vop_abortop_desc, vop_generic_abortop },	/* abortop */
	{ &vop_inactive_desc, ufs_inactive },		/* inactive */
	{ &vop_reclaim_desc, ffs_reclaim },		/* reclaim */
	{ &vop_lock_desc, ufs_lock },			/* lock */
	{ &vop_unlock_desc, ufs_unlock },		/* unlock */
	{ &vop_bmap_desc, ufs_bmap },			/* bmap */
	{ &vop_strategy_desc, ufs_strategy },		/* strategy */
	{ &vop_print_desc, ufs_print },			/* print */
	{ &vop_islocked_desc, ufs_islocked },		/* islocked */
	{ &vop_pathconf_desc, ufs_pathconf },		/* pathconf */
	{ &vop_advlock_desc, ufs_advlock },		/* advlock */
	{ &vop_reallocblks_desc, ffs_reallocblks },	/* reallocblks */
	{ &vop_bwrite_desc, vop_generic_bwrite },
#ifdef UFS_EXTATTR
	{ &vop_getextattr_desc, ufs_vop_getextattr },
	{ &vop_setextattr_desc, ufs_vop_setextattr },
#endif
	{ NULL, NULL }
};
struct vnodeopv_desc ffs_vnodeop_opv_desc =
	{ &ffs_vnodeop_p, ffs_vnodeop_entries };

int (**ffs_specop_p)(void *);
struct vnodeopv_entry_desc ffs_specop_entries[] = {
	{ &vop_default_desc, spec_vnoperate },
	{ &vop_close_desc, ufsspec_close },		/* close */
	{ &vop_access_desc, ufs_access },		/* access */
	{ &vop_getattr_desc, ufs_getattr },		/* getattr */
	{ &vop_setattr_desc, ufs_setattr },		/* setattr */
	{ &vop_read_desc, ufsspec_read },		/* read */
	{ &vop_write_desc, ufsspec_write },		/* write */
	{ &vop_fsync_desc, ffs_fsync },			/* fsync */
	{ &vop_inactive_desc, ufs_inactive },		/* inactive */
	{ &vop_reclaim_desc, ffs_reclaim },		/* reclaim */
	{ &vop_lock_desc, ufs_lock },			/* lock */
	{ &vop_unlock_desc, ufs_unlock },		/* unlock */
	{ &vop_print_desc, ufs_print },			/* print */
	{ &vop_islocked_desc, ufs_islocked },		/* islocked */
#ifdef UFS_EXTATTR
	{ &vop_getextattr_desc, ufs_vop_getextattr },
	{ &vop_setextattr_desc, ufs_vop_setextattr },
#endif
	{ NULL, NULL }
};
struct vnodeopv_desc ffs_specop_opv_desc =
	{ &ffs_specop_p, ffs_specop_entries };

#ifdef FIFO
int (**ffs_fifoop_p)(void *);
struct vnodeopv_entry_desc ffs_fifoop_entries[] = {
	{ &vop_default_desc, fifo_vnoperate },
	{ &vop_close_desc, ufsfifo_close },		/* close */
	{ &vop_access_desc, ufs_access },		/* access */
	{ &vop_getattr_desc, ufs_getattr },		/* getattr */
	{ &vop_setattr_desc, ufs_setattr },		/* setattr */
	{ &vop_read_desc, ufsfifo_read },		/* read */
	{ &vop_write_desc, ufsfifo_write },		/* write */
	{ &vop_fsync_desc, ffs_fsync },			/* fsync */
	{ &vop_inactive_desc, ufs_inactive },		/* inactive */
	{ &vop_reclaim_desc, ffsfifo_reclaim },		/* reclaim */
	{ &vop_lock_desc, ufs_lock },			/* lock */
	{ &vop_unlock_desc, ufs_unlock },		/* unlock */
	{ &vop_print_desc, ufs_print },			/* print */
	{ &vop_islocked_desc, ufs_islocked },		/* islocked */
	{ &vop_bwrite_desc, vop_generic_bwrite },
#ifdef UFS_EXTATTR
	{ &vop_getextattr_desc, ufs_vop_getextattr },
	{ &vop_setextattr_desc, ufs_vop_setextattr },
#endif
	{ NULL, NULL }
};
struct vnodeopv_desc ffs_fifoop_opv_desc =
	{ &ffs_fifoop_p, ffs_fifoop_entries };
#endif /* FIFO */

/*
 * Enabling cluster read/write operations.
 */
int doclusterread = 1;
int doclusterwrite = 1;

#include <ufs/ufs/ufs_readwrite.c>

/*
 * Synch an open file.
 */
/* ARGSUSED */
int
ffs_fsync(v)
	void *v;
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct buf *bp, *nbp;
	int s, error, passes, skipmeta;

	if (vp->v_type == VBLK &&
	    vp->v_specmountpoint != NULL &&
	    (vp->v_specmountpoint->mnt_flag & MNT_SOFTDEP))
		softdep_fsync_mountdev(vp);

	/*
	 * Flush all dirty buffers associated with a vnode.
	 */
	passes = NIADDR + 1;
	skipmeta = 0;
	if (ap->a_waitfor == MNT_WAIT)
		skipmeta = 1;
	s = splbio();
loop:
	for (bp = LIST_FIRST(&vp->v_dirtyblkhd); bp;
	     bp = LIST_NEXT(bp, b_vnbufs))
		bp->b_flags &= ~B_SCANNED;
	for (bp = LIST_FIRST(&vp->v_dirtyblkhd); bp; bp = nbp) {
		nbp = LIST_NEXT(bp, b_vnbufs);
		/* 
		 * Reasons to skip this buffer: it has already been considered
		 * on this pass, this pass is the first time through on a
		 * synchronous flush request and the buffer being considered
		 * is metadata, the buffer has dependencies that will cause
		 * it to be redirtied and it has not already been deferred,
		 * or it is already being written.
		 */
		if (bp->b_flags & (B_BUSY | B_SCANNED))
			continue;
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("ffs_fsync: not dirty");
		if (skipmeta && bp->b_lblkno < 0)
			continue;
		if (ap->a_waitfor != MNT_WAIT &&
		    LIST_FIRST(&bp->b_dep) != NULL &&
		    (bp->b_flags & B_DEFERRED) == 0 &&
		    buf_countdeps(bp, 0, 1)) {
			bp->b_flags |= B_DEFERRED;
			continue;
		}

		bremfree(bp);
		bp->b_flags |= B_BUSY | B_SCANNED;
		splx(s);
		/*
		 * On our final pass through, do all I/O synchronously
		 * so that we can find out if our flush is failing
		 * because of write errors.
		 */
		if (passes > 0 || ap->a_waitfor != MNT_WAIT)
			(void) bawrite(bp);
		else if ((error = bwrite(bp)) != 0)
			return (error);
		s = splbio();
		/*
		 * Since we may have slept during the I/O, we need
		 * to start from a known point.
		 */
		nbp = LIST_FIRST(&vp->v_dirtyblkhd);
	}
	if (skipmeta) {
		skipmeta = 0;
		goto loop;
	}
	if (ap->a_waitfor == MNT_WAIT) {
		vwaitforio(vp, 0, "ffs_fsync", 0);

		/*
		 * Ensure that any filesystem metadata associated
		 * with the vnode has been written.
		 */
		splx(s);
		if ((error = softdep_sync_metadata(ap)) != 0)
			return (error);
		s = splbio();
		if (!LIST_EMPTY(&vp->v_dirtyblkhd)) {
			/*
			 * Block devices associated with filesystems may
			 * have new I/O requests posted for them even if
			 * the vnode is locked, so no amount of trying will
			 * get them clean. Thus we give block devices a
			 * good effort, then just give up. For all other file
			 * types, go around and try again until it is clean.
			 */
			if (passes > 0) {
				passes -= 1;
				goto loop;
			}
#ifdef DIAGNOSTIC
			if (vp->v_type != VBLK)
				vprint("ffs_fsync: dirty", vp);
#endif
		}
	}
	splx(s);
	return (UFS_UPDATE(VTOI(vp), ap->a_waitfor == MNT_WAIT));
}

/*
 * Reclaim an inode so that it can be used for other purposes.
 */
int
ffs_reclaim(v)
	void *v;
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;
	int error;

	if ((error = ufs_reclaim(vp, ap->a_p)) != 0)
		return (error);
	/* XXX - same for for both mfs and ffs */
	pool_put(&ffs_ino_pool, vp->v_data);
	vp->v_data = NULL;
	return (0);
}

#ifdef FIFO
int
ffsfifo_reclaim(void *v)
{
	fifo_reclaim(v);
	return (ffs_reclaim(v));
}
#endif

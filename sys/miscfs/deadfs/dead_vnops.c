/*	$OpenBSD: dead_vnops.c,v 1.6 1998/08/06 19:34:30 csapuntz Exp $	*/
/*	$NetBSD: dead_vnops.c,v 1.16 1996/02/13 13:12:48 mycroft Exp $	*/

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
 *	@(#)dead_vnops.c	8.2 (Berkeley) 11/21/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/proc.h>

/*
 * Prototypes for dead operations on vnodes.
 */
int	dead_badop	__P((void *));
int	dead_ebadf	__P((void *));

int	dead_lookup	__P((void *));
#define dead_create	dead_badop
#define dead_mknod	dead_badop
int	dead_open	__P((void *));
#define dead_close	nullop
#define dead_access	dead_ebadf
#define dead_getattr	dead_ebadf
#define dead_setattr	dead_ebadf
int	dead_read	__P((void *));
int	dead_write	__P((void *));
int	dead_ioctl	__P((void *));
int	dead_select	__P((void *));
#define dead_mmap	dead_badop
#define dead_fsync	nullop
#define dead_seek	nullop
#define dead_remove	dead_badop
#define dead_link	dead_badop
#define dead_rename	dead_badop
#define dead_mkdir	dead_badop
#define dead_rmdir	dead_badop
#define dead_symlink	dead_badop
#define dead_readdir	dead_ebadf
#define dead_readlink	dead_ebadf
#define dead_abortop	dead_badop
#define dead_inactive	nullop
#define dead_reclaim	nullop
int	dead_lock	__P((void *));
#define dead_unlock	vop_generic_unlock
int	dead_bmap	__P((void *));
int	dead_strategy	__P((void *));
int	dead_print	__P((void *));
#define dead_islocked	vop_generic_islocked
#define dead_pathconf	dead_ebadf
#define dead_advlock	dead_ebadf
#define dead_blkatoff	dead_badop
#define dead_valloc	dead_badop
#define dead_vfree	dead_badop
#define dead_truncate	nullop
#define dead_update	nullop
#define dead_bwrite	nullop

int	chkvnlock __P((struct vnode *));

int (**dead_vnodeop_p) __P((void *));

struct vnodeopv_entry_desc dead_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, dead_lookup },	/* lookup */
	{ &vop_create_desc, dead_create },	/* create */
	{ &vop_mknod_desc, dead_mknod },	/* mknod */
	{ &vop_open_desc, dead_open },		/* open */
	{ &vop_close_desc, dead_close },	/* close */
	{ &vop_access_desc, dead_access },	/* access */
	{ &vop_getattr_desc, dead_getattr },	/* getattr */
	{ &vop_setattr_desc, dead_setattr },	/* setattr */
	{ &vop_read_desc, dead_read },		/* read */
	{ &vop_write_desc, dead_write },	/* write */
	{ &vop_ioctl_desc, dead_ioctl },	/* ioctl */
	{ &vop_select_desc, dead_select },	/* select */
	{ &vop_mmap_desc, dead_mmap },		/* mmap */
	{ &vop_fsync_desc, dead_fsync },	/* fsync */
	{ &vop_seek_desc, dead_seek },		/* seek */
	{ &vop_remove_desc, dead_remove },	/* remove */
	{ &vop_link_desc, dead_link },		/* link */
	{ &vop_rename_desc, dead_rename },	/* rename */
	{ &vop_mkdir_desc, dead_mkdir },	/* mkdir */
	{ &vop_rmdir_desc, dead_rmdir },	/* rmdir */
	{ &vop_symlink_desc, dead_symlink },	/* symlink */
	{ &vop_readdir_desc, dead_readdir },	/* readdir */
	{ &vop_readlink_desc, dead_readlink },	/* readlink */
	{ &vop_abortop_desc, dead_abortop },	/* abortop */
	{ &vop_inactive_desc, dead_inactive },	/* inactive */
	{ &vop_reclaim_desc, dead_reclaim },	/* reclaim */
	{ &vop_lock_desc, dead_lock },		/* lock */
	{ &vop_unlock_desc, dead_unlock },	/* unlock */
	{ &vop_bmap_desc, dead_bmap },		/* bmap */
	{ &vop_strategy_desc, dead_strategy },	/* strategy */
	{ &vop_print_desc, dead_print },	/* print */
	{ &vop_islocked_desc, dead_islocked },	/* islocked */
	{ &vop_pathconf_desc, dead_pathconf },	/* pathconf */
	{ &vop_advlock_desc, dead_advlock },	/* advlock */
	{ &vop_blkatoff_desc, dead_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, dead_valloc },	/* valloc */
	{ &vop_vfree_desc, dead_vfree },	/* vfree */
	{ &vop_truncate_desc, dead_truncate },	/* truncate */
	{ &vop_update_desc, dead_update },	/* update */
	{ &vop_bwrite_desc, dead_bwrite },	/* bwrite */
	{ (struct vnodeop_desc*)NULL, (int(*) __P((void *)))NULL }
};
struct vnodeopv_desc dead_vnodeop_opv_desc =
	{ &dead_vnodeop_p, dead_vnodeop_entries };

/*
 * Trivial lookup routine that always fails.
 */
/* ARGSUSED */
int
dead_lookup(v)
	void *v;
{
	struct vop_lookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap = v;

	*ap->a_vpp = NULL;
	return (ENOTDIR);
}

/*
 * Open always fails as if device did not exist.
 */
/* ARGSUSED */
int
dead_open(v)
	void *v;
{

	return (ENXIO);
}

/*
 * Vnode op for read
 */
/* ARGSUSED */
int
dead_read(v)
	void *v;
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;

	if (chkvnlock(ap->a_vp))
		panic("dead_read: lock");
	/*
	 * Return EOF for tty devices, EIO for others
	 */
	if ((ap->a_vp->v_flag & VISTTY) == 0)
		return (EIO);
	return (0);
}

/*
 * Vnode op for write
 */
/* ARGSUSED */
int
dead_write(v)
	void *v;
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;

	if (chkvnlock(ap->a_vp))
		panic("dead_write: lock");
	return (EIO);
}

/*
 * Device ioctl operation.
 */
/* ARGSUSED */
int
dead_ioctl(v)
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

	if (!chkvnlock(ap->a_vp))
		return (EBADF);
	return (VCALL(ap->a_vp, VOFFSET(vop_ioctl), ap));
}

/* ARGSUSED */
int
dead_select(v)
	void *v;
{
	/*
	 * Let the user find out that the descriptor is gone.
	 */
	return (1);
}

/*
 * Just call the device strategy routine
 */
int
dead_strategy(v)
	void *v;
{

	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap = v;
	if (ap->a_bp->b_vp == NULL || !chkvnlock(ap->a_bp->b_vp)) {
		ap->a_bp->b_flags |= B_ERROR;
		biodone(ap->a_bp);
		return (EIO);
	}
	return (VOP_STRATEGY(ap->a_bp));
}

/*
 * Wait until the vnode has finished changing state.
 */
int
dead_lock(v)
	void *v;
{
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	/*
	 * Since we are not using the lock manager, we must clear
	 * the interlock here.
	 */
	if (ap->a_flags & LK_INTERLOCK) {
		simple_unlock(&vp->v_interlock);
		ap->a_flags &= ~LK_INTERLOCK;
	}
	if (!chkvnlock(vp))
 		return (0);

	return (VCALL(vp, VOFFSET(vop_lock), ap));
}

/*
 * Wait until the vnode has finished changing state.
 */
int
dead_bmap(v)
	void *v;
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap = v;

	if (!chkvnlock(ap->a_vp))
		return (EIO);
	return (VOP_BMAP(ap->a_vp, ap->a_bn, ap->a_vpp, ap->a_bnp, ap->a_runp));
}

/*
 * Print out the contents of a dead vnode.
 */
/* ARGSUSED */
int
dead_print(v)
	void *v;
{
	printf("tag VT_NON, dead vnode\n");
	return 0;
}

/*
 * Empty vnode failed operation
 */
/*ARGSUSED*/
int
dead_ebadf(v)
	void *v;
{

	return (EBADF);
}

/*
 * Empty vnode bad operation
 */
/*ARGSUSED*/
int
dead_badop(v)
	void *v;
{

	panic("dead_badop called");
	/* NOTREACHED */
}

/*
 * We have to wait during times when the vnode is
 * in a state of change.
 */
int
chkvnlock(vp)
	register struct vnode *vp;
{
	int locked = 0;

	while (vp->v_flag & VXLOCK) {
		vp->v_flag |= VXWANT;
		sleep((caddr_t)vp, PINOD);
		locked = 1;
	}
	return (locked);
}

/*	$OpenBSD: tcfs_vnops.c,v 1.4 2001/12/04 22:44:32 art Exp $	*/
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
#include <sys/buf.h>
#include <miscfs/tcfs/tcfs.h>


int tcfs_bug_bypass = 0;   /* for debugging: enables bypass printf'ing */

int     tcfs_bypass __P((void *));
int     tcfs_getattr __P((void *));
int     tcfs_setattr __P((void *));
int     tcfs_inactive __P((void *));
int     tcfs_reclaim __P((void *));
int     tcfs_print __P((void *));
int     tcfs_strategy __P((void *));
int     tcfs_bwrite __P((void *));
int     tcfs_lock __P((void *));
int     tcfs_unlock __P((void *));
int     tcfs_islocked __P((void *));
int     tcfs_read __P((void *));
int     tcfs_readdir __P((void *));
int     tcfs_write __P((void *));
int     tcfs_create __P((void *));
int     tcfs_mknod __P((void *));
int     tcfs_mkdir __P((void *));
int     tcfs_link __P((void *));
int     tcfs_symlink __P((void *));
int     tcfs_rename __P((void *));
int     tcfs_lookup __P((void *));

int
tcfs_bypass(v)
	void *v;
{
	struct vop_generic_args /* {
				   struct vnodeop_desc *a_desc;
				   <other random data follows, presumably>
				   } */ *ap = v;
	register struct vnode **this_vp_p;
	int error;
	struct vnode *old_vps[VDESC_MAX_VPS];
	struct vnode **vps_p[VDESC_MAX_VPS];
	struct vnode ***vppp;
	struct vnodeop_desc *descp = ap->a_desc;
	int reles, i;

	if (tcfs_bug_bypass)
		printf ("tcfs_bypass: %s\n", descp->vdesc_name);

#ifdef SAFETY
	/*
	 * We require at least one vp.
	 */
	if (descp->vdesc_vp_offsets == TCFS ||
	    descp->vdesc_vp_offsets[0] == VDESC_NO_OFFSET)
		panic ("tcfs_bypass: no vp's in map.");
#endif

	/*
	 * Map the vnodes going in.
	 * Later, we'll invoke the operation based on
	 * the first mapped vnode's operation vector.
	 */
	reles = descp->vdesc_flags;
	for (i = 0; i < VDESC_MAX_VPS; reles >>= 1, i++) {
		if (descp->vdesc_vp_offsets[i] == VDESC_NO_OFFSET)
			break;   /* bail out at end of list */
		vps_p[i] = this_vp_p = 
			VOPARG_OFFSETTO(struct vnode**,
					descp->vdesc_vp_offsets[i], ap);
		/*
		 * We're not guaranteed that any but the first vnode
		 * are of our type.  Check for and don't map any
		 * that aren't.  (We must always map first vp or vclean fails.)
		 */
		if (i && (*this_vp_p == NULLVP ||
			  (*this_vp_p)->v_op != tcfs_vnodeop_p)) {
			old_vps[i] = NULLVP;
		} else {
			old_vps[i] = *this_vp_p;
			*(vps_p[i]) = TCFSVPTOLOWERVP(*this_vp_p);
			/*
			 * XXX - Several operations have the side effect
			 * of vrele'ing their vp's.  We must account for
			 * that.  (This should go away in the future.)
			 */
			if (reles & 1)
				VREF(*this_vp_p);
		}
			
	}

	/*
	 * Call the operation on the lower layer
	 * with the modified argument structure.
	 */
	error = VCALL(*(vps_p[0]), descp->vdesc_offset, ap);

	/*
	 * Maintain the illusion of call-by-value
	 * by restoring vnodes in the argument structure
	 * to their original value.
	 */
	reles = descp->vdesc_flags;
	for (i = 0; i < VDESC_MAX_VPS; reles >>= 1, i++) {
		if (descp->vdesc_vp_offsets[i] == VDESC_NO_OFFSET)
			break;   /* bail out at end of list */
		if (old_vps[i] != NULLVP) {
			*(vps_p[i]) = old_vps[i];
			if (reles & 1) {
				vrele(*(vps_p[i]));
			}
		}

		/*
		 * Map the possible out-going vpp
		 * (Assumes that the lower layer always returns
		 * a VREF'ed vpp unless it gets an error.)
		 */
		if (descp->vdesc_vpp_offset != VDESC_NO_OFFSET &&
		    !(descp->vdesc_flags & VDESC_NOMAP_VPP) &&
		    !error) {
			/*
			 * XXX - even though some ops have vpp returned vp's,
			 * several ops actually vrele this before returning.
			 * We must avoid these ops.
			 * (This should go away when these ops are regularized.)
			 */
			if (descp->vdesc_flags & VDESC_VPP_WILLRELE)
				goto out;
			vppp = VOPARG_OFFSETTO(struct vnode***,
					       descp->vdesc_vpp_offset,ap);
			/*
			 * This assumes that **vppp is a locked vnode (it is always
			 * so as of this writing, NetBSD-current 1995/02/16)
			 *
			 * (don't want to lock it if being called on behalf
			 * of lookup--it plays weird locking games depending
			 * on whether or not it's looking up ".", "..", etc.
			 */
			error = tcfs_node_create(old_vps[0]->v_mount, **vppp, *vppp,
						 descp == &vop_lookup_desc ? 0 : 1);
		}
	}

 out:
	return (error);
}


/*
 * We must handle open to be able to catch MNT_NODEV and friends.
 */
int
tcfs_open(v)
	void *v;
{
	struct vop_open_args *ap = v;
	struct vnode *vp = ap->a_vp;
	enum vtype lower_type = VTOTCFS(vp)->tcfs_lowervp->v_type;


	if (((lower_type == VBLK) || (lower_type == VCHR)) &&
	    (vp->v_mount->mnt_flag & MNT_NODEV))
		return ENXIO;

	return tcfs_bypass(ap);
}

int
tcfs_inactive(v)
	void *v;
{
	struct vop_inactive_args *ap = v;

	/*
	 * Do nothing (and _don't_ bypass).
	 * Wait to vrele lowervp until reclaim,
	 * so that until then our tcfs_node is in the
	 * cache and reusable.
	 *
	 * NEEDSWORK: Someday, consider inactive'ing
	 * the lowervp and then trying to reactivate it
	 * with capabilities (v_id)
	 * like they do in the name lookup cache code.
	 * That's too much work for now.
	 */
	VOP_UNLOCK(ap->a_vp, 0, ap->a_p);

	return (0);
}

int
tcfs_reclaim(v)
	void *v;
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct tcfs_node *xp = VTOTCFS(vp);
	struct vnode *lowervp = xp->tcfs_lowervp;

	/*
	 * Note: in vop_reclaim, vp->v_op == dead_vnodeop_p,
	 * so we can't call VOPs on ourself.
	 */
	/* After this assignment, this node will not be re-used. */
	xp->tcfs_lowervp = NULL;
	LIST_REMOVE(xp, tcfs_hash);
	FREE(vp->v_data, M_TEMP);
	vp->v_data = NULL;
	vrele (lowervp);
	return (0);
}

int
tcfs_print(v)
	void *v;
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	register struct vnode *vp = ap->a_vp;

	printf ("\ttag VT_TCFS, vp=%p, lowervp=%p\n", vp, TCFSVPTOLOWERVP(vp));
	vprint("tcfs lowervp", TCFSVPTOLOWERVP(vp));
	return (0);
}


/*
 * XXX - vop_strategy must be hand coded because it has no
 * vnode in its arguments.
 * This goes away with a merged VM/buffer cache.
 */
int
tcfs_strategy(v)
	void *v;
{
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap = v;
	struct buf *bp = ap->a_bp;
	int error;
	struct vnode *savedvp;

	savedvp = bp->b_vp;
	bp->b_vp = TCFSVPTOLOWERVP(bp->b_vp);

	error = VOP_STRATEGY(bp);

	bp->b_vp = savedvp;

	return (error);
}


/*
 * XXX - like vop_strategy, vop_bwrite must be hand coded because it has no
 * vnode in its arguments.
 * This goes away with a merged VM/buffer cache.
 */
int
tcfs_bwrite(v)
	void *v;
{
	struct vop_bwrite_args /* {
		struct buf *a_bp;
	} */ *ap = v;
	struct buf *bp = ap->a_bp;
	int error;
	struct vnode *savedvp;

	savedvp = bp->b_vp;
	bp->b_vp = TCFSVPTOLOWERVP(bp->b_vp);

	error = VOP_BWRITE(bp);

	bp->b_vp = savedvp;

	return (error);
}

/*
 * We need a separate tcfs lock routine, to avoid deadlocks at reclaim time.
 * If a process holds the lower-vnode locked when it tries to reclaim
 * the tcfs upper-vnode, _and_ tcfs_bypass is used as the locking operation,
 * then a process can end up locking against itself.
 * This has been observed when a tcfs mount is set up to "tunnel" beneath a
 * union mount (that setup is useful if you still wish to be able to access
 * the non-union version of either the above or below union layer)
 */
int
tcfs_lock(v)
	void *v;
{
	struct vop_lock_args *ap = v;

#if 0
	vop_generic_lock(ap);
#endif
	if ((ap->a_flags & LK_TYPE_MASK) == LK_DRAIN)
		return (0);
	ap->a_flags &= ~LK_INTERLOCK;

	return (tcfs_bypass((struct vop_generic_args *)ap));
}

int
tcfs_unlock(v)
	void *v;
{
	struct vop_unlock_args *ap = v;
#if 0
	vop_generic_unlock(ap);
#endif
	ap->a_flags &= ~LK_INTERLOCK;

	return (tcfs_bypass((struct vop_generic_args *)ap));
}

int
tcfs_islocked(v)
	void *v;
{
	/* XXX */
	return (0);
}

int
tcfs_lookup(v)
	void *v;
{
	register struct vop_lookup_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap = v;
	register int error;
	int flags = ap->a_cnp->cn_flags;
	struct componentname *cnp = ap->a_cnp;
#if 0
	register struct vnode *dvp, *vp;
	struct proc *p = cnp->cn_proc;
	struct vop_unlock_args unlockargs;
	struct vop_lock_args lockargs;
#endif

#ifdef TCFS_DIAGNOSTIC
	printf("tcfs_lookup: dvp=%lx, name='%s'\n",
	       ap->a_dvp, cnp->cn_nameptr);
#endif

	if ((flags & ISLASTCN) && (ap->a_dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);
	error = tcfs_bypass((struct vop_generic_args *)ap);
	if (error == EJUSTRETURN && (flags & ISLASTCN) &&
	    (ap->a_dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME))
		error = EROFS;

#if 0
	/*
	 * We must do the same locking and unlocking at this layer as 
	 * is done in the layers below us. We could figure this out 
	 * based on the error return and the LASTCN, LOCKPARENT, and
	 * LOCKLEAF flags. However, it is more expidient to just find 
	 * out the state of the lower level vnodes and set ours to the
	 * same state.
	 */
	dvp = ap->a_dvp;
	vp = *ap->a_vpp;
	if (dvp == vp)
		return (error);
	if (!VOP_ISLOCKED(dvp)) {
		unlockargs.a_vp = dvp;
		unlockargs.a_flags = 0;
		unlockargs.a_p = p;
		vop_generic_unlock(&unlockargs);
	}
	if (vp != TCFSVP && VOP_ISLOCKED(vp)) {
		lockargs.a_vp = vp;
		lockargs.a_flags = LK_SHARED;
		lockargs.a_p = p;
		vop_generic_lock(&lockargs);
	}
#endif
	return (error);
}

/*
 * Global vfs data structures
 */
int (**tcfs_vnodeop_p) __P((void *));
struct vnodeopv_entry_desc tcfs_vnodeop_entries[] = {
        { &vop_default_desc,    tcfs_bypass },

        { &vop_getattr_desc,    tcfs_getattr },
        { &vop_setattr_desc,    tcfs_setattr },
        { &vop_inactive_desc,   tcfs_inactive },
        { &vop_reclaim_desc,    tcfs_reclaim },
        { &vop_print_desc,      tcfs_print },

        { &vop_lock_desc,       tcfs_lock },
        { &vop_unlock_desc,     tcfs_unlock },
        { &vop_islocked_desc,   tcfs_islocked },
        { &vop_lookup_desc,     tcfs_lookup }, /* special locking frob */

        { &vop_strategy_desc,   tcfs_strategy },
        { &vop_bwrite_desc,     tcfs_bwrite },
        { &vop_read_desc,       tcfs_read },
        { &vop_readdir_desc,    tcfs_readdir },
        { &vop_write_desc,      tcfs_write },
        { &vop_create_desc,     tcfs_create },
        { &vop_mknod_desc,      tcfs_mknod },
        { &vop_mkdir_desc,      tcfs_mkdir },
        { &vop_link_desc,       tcfs_link },
        { &vop_rename_desc,     tcfs_rename },
        { &vop_symlink_desc,    tcfs_symlink },
        { NULL, NULL }
};

struct vnodeopv_desc tcfs_vnodeop_opv_desc =
        { &tcfs_vnodeop_p, tcfs_vnodeop_entries };

/*	$OpenBSD: layer_vnops.c,v 1.7 2004/10/26 18:23:47 pedro Exp $ */
/*	$NetBSD: layer_vnops.c,v 1.10 2001/12/06 04:29:23 chs Exp $	*/

/*
 * Copyright (c) 1999 National Aeronautics & Space Administration
 * All rights reserved.
 *
 * This software was written by William Studenmund of the
 * Numerical Aerospace Simulation Facility, NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the National Aeronautics & Space Administration
 *    nor the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NATIONAL AERONAUTICS & SPACE ADMINISTRATION
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ADMINISTRATION OR CONTRIB-
 * UTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * John Heidemann of the UCLA Ficus project.
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
 *	@(#)null_vnops.c	8.6 (Berkeley) 5/27/95
 *
 * Ancestors:
 *	@(#)lofs_vnops.c	1.2 (Berkeley) 6/18/92
 *	$Id: layer_vnops.c,v 1.7 2004/10/26 18:23:47 pedro Exp $
 *	...and...
 *	@(#)null_vnodeops.c 1.20 92/07/07 UCLA Ficus project
 */

/*
 * Null Layer vnode routines.
 *
 * (See mount_null(8) for more information.)
 *
 * The layer.h, layer_extern.h, layer_vfs.c, and layer_vnops.c files provide
 * the core implementation of the null file system and most other stacked
 * fs's. The description below refers to the null file system, but the
 * services provided by the layer* files are useful for all layered fs's.
 *
 * The null layer duplicates a portion of the file system
 * name space under a new name.  In this respect, it is
 * similar to the loopback file system.  It differs from
 * the loopback fs in two respects:  it is implemented using
 * a stackable layers technique, and it's "null-node"s stack above
 * all lower-layer vnodes, not just over directory vnodes.
 *
 * The null layer has two purposes.  First, it serves as a demonstration
 * of layering by proving a layer which does nothing.  (It actually
 * does everything the loopback file system does, which is slightly
 * more than nothing.)  Second, the null layer can serve as a prototype
 * layer.  Since it provides all necessary layer framework,
 * new file system layers can be created very easily by starting
 * with a null layer.
 *
 * The remainder of the man page examines the null layer as a basis
 * for constructing new layers.
 *
 *
 * INSTANTIATING NEW NULL LAYERS
 *
 * New null layers are created with mount_null(8).
 * Mount_null(8) takes two arguments, the pathname
 * of the lower vfs (target-pn) and the pathname where the null
 * layer will appear in the namespace (alias-pn).  After
 * the null layer is put into place, the contents
 * of target-pn subtree will be aliased under alias-pn.
 *
 * It is conceivable that other overlay filesystems will take different
 * parameters. For instance, data migration or access control layers might
 * only take one pathname which will serve both as the target-pn and
 * alias-pn described above.
 *
 *
 * OPERATION OF A NULL LAYER
 *
 * The null layer is the minimum file system layer,
 * simply bypassing all possible operations to the lower layer
 * for processing there.  The majority of its activity centers
 * on the bypass routine, through which nearly all vnode operations
 * pass.
 *
 * The bypass routine accepts arbitrary vnode operations for
 * handling by the lower layer.  It begins by examing vnode
 * operation arguments and replacing any layered nodes by their
 * lower-layer equivalents.  It then invokes the operation
 * on the lower layer.  Finally, it replaces the layered nodes
 * in the arguments and, if a vnode is returned by the operation,
 * stacks a layered node on top of the returned vnode.
 *
 * The bypass routine in this file, layer_bypass(), is suitable for use
 * by many different layered filesystems. It can be used by multiple
 * filesystems simultaneously. Alternatively, a layered fs may provide
 * its own bypass routine, in which case layer_bypass() should be used as
 * a model. For instance, the main functionality provided by umapfs, the user
 * identity mapping file system, is handled by a custom bypass routine.
 *
 * Typically a layered fs registers its selected bypass routine as the
 * default vnode operation in its vnodeopv_entry_desc table. Additionally
 * the filesystem must store the bypass entry point in the layerm_bypass
 * field of struct layer_mount. All other layer routines in this file will
 * use the layerm_bypass routine.
 *
 * Although the bypass routine handles most operations outright, a number
 * of operations are special cased, and handled by the layered fs. One
 * group, layer_setattr, layer_getattr, layer_access, layer_open, and
 * layer_fsync, perform layer-specific manipulation in addition to calling
 * the bypass routine.

 * Although bypass handles most operations, vop_getattr, vop_lock,
 * vop_unlock, vop_inactive, vop_reclaim, and vop_print are not
 * bypassed. Vop_getattr must change the fsid being returned.
 * Vop_lock and vop_unlock must handle any locking for the
 * current vnode as well as pass the lock request down.
 * Vop_inactive and vop_reclaim are not bypassed so that
 * they can handle freeing null-layer specific data. Vop_print
 * is not bypassed to avoid excessive debugging information.
 * Also, certain vnode operations change the locking state within
 * the operation (create, mknod, remove, link, rename, mkdir, rmdir,
 * and symlink). Ideally these operations should not change the
 * lock state, but should be changed to let the caller of the
 * function unlock them. Otherwise all intermediate vnode layers
 * (such as union, umapfs, etc) must catch these functions to do
 * the necessary locking at their layer.
 *
 *
 * INSTANTIATING VNODE STACKS
 *
 * Mounting associates the null layer with a lower layer,
 * effectively stacking two VFSes.  Vnode stacks are instead
 * created on demand as files are accessed.
 *
 * The initial mount creates a single vnode stack for the
 * root of the new null layer.  All other vnode stacks
 * are created as a result of vnode operations on
 * this or other null vnode stacks.
 *
 * New vnode stacks come into existence as a result of
 * an operation which returns a vnode.  
 * The bypass routine stacks a null-node above the new
 * vnode before returning it to the caller.
 *
 * For example, imagine mounting a null layer with
 * "mount_null /usr/include /dev/layer/null".
 * Changing directory to /dev/layer/null will assign
 * the root null-node (which was created when the null layer was mounted).
 * Now consider opening "sys".  A vop_lookup would be
 * done on the root null-node.  This operation would bypass through
 * to the lower layer which would return a vnode representing 
 * the UFS "sys".  layer_bypass then builds a null-node
 * aliasing the UFS "sys" and returns this to the caller.
 * Later operations on the null-node "sys" will repeat this
 * process when constructing other vnode stacks.
 *
 *
 * CREATING OTHER FILE SYSTEM LAYERS
 *
 * One of the easiest ways to construct new file system layers is to make
 * a copy of the null layer, rename all files and variables, and
 * then begin modifying the copy.  Sed can be used to easily rename
 * all variables.
 *
 * The umap layer is an example of a layer descended from the 
 * null layer.
 *
 *
 * INVOKING OPERATIONS ON LOWER LAYERS
 *
 * There are two techniques to invoke operations on a lower layer 
 * when the operation cannot be completely bypassed.  Each method
 * is appropriate in different situations.  In both cases,
 * it is the responsibility of the aliasing layer to make
 * the operation arguments "correct" for the lower layer
 * by mapping all vnode arguments to the lower layer.
 *
 * The first approach is to call the aliasing layer's bypass routine.
 * This method is most suitable when you wish to invoke the operation
 * currently being handled on the lower layer.  It has the advantage
 * that the bypass routine already must do argument mapping.
 * An example of this is null_getattrs in the null layer.
 *
 * A second approach is to directly invoke vnode operations on
 * the lower layer with the VOP_OPERATIONNAME interface.
 * The advantage of this method is that it is easy to invoke
 * arbitrary operations on the lower layer.  The disadvantage
 * is that vnodes' arguments must be manually mapped.
 *
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <miscfs/genfs/layer.h>
#include <miscfs/genfs/layer_extern.h>


/*
 * This is the 08-June-99 bypass routine, based on the 10-Apr-92 bypass
 *		routine by John Heidemann.
 *	The new element for this version is that the whole nullfs
 * system gained the concept of locks on the lower node, and locks on
 * our nodes. When returning from a call to the lower layer, we may
 * need to update lock state ONLY on our layer. The LAYERFS_UPPER*LOCK()
 * macros provide this functionality.
 *    The 10-Apr-92 version was optimized for speed, throwing away some
 * safety checks.  It should still always work, but it's not as
 * robust to programmer errors.
 *    Define SAFETY to include some error checking code.
 *
 * In general, we map all vnodes going down and unmap them on the way back.
 *
 * Also, some BSD vnode operations have the side effect of vrele'ing
 * their arguments.  With stacking, the reference counts are held
 * by the upper node, not the lower one, so we must handle these
 * side-effects here.  This is not of concern in Sun-derived systems
 * since there are no such side-effects.
 *
 * New for the 08-June-99 version: we also handle operations which unlock
 * the passed-in node (typically they vput the node).
 *
 * This makes the following assumptions:
 * - only one returned vpp
 * - no INOUT vpp's (Sun's vop_open has one of these)
 * - the vnode operation vector of the first vnode should be used
 *   to determine what implementation of the op should be invoked
 * - all mapped vnodes are of our vnode-type (NEEDSWORK:
 *   problems on rmdir'ing mount points and renaming?)
 */ 
int
layer_bypass(v)
	void *v;
{
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
		<other random data follows, presumably>
	} */ *ap = v;
	int (**our_vnodeop_p)(void *); 
	struct vnode **this_vp_p;
	int error, error1;
	struct vnode *old_vps[VDESC_MAX_VPS], *vp0;
	struct vnode **vps_p[VDESC_MAX_VPS];
	struct vnode ***vppp;
	struct vnodeop_desc *descp = ap->a_desc;
	int reles, i, flags;

#ifdef SAFETY
	/*
	 * We require at least one vp.
	 */
	if (descp->vdesc_vp_offsets == NULL ||
	    descp->vdesc_vp_offsets[0] == VDESC_NO_OFFSET)
		panic("layer_bypass: no vp's in map");
#endif

	vps_p[0] = VOPARG_OFFSETTO(struct vnode**,descp->vdesc_vp_offsets[0],ap);
	vp0 = *vps_p[0];
	flags = MOUNTTOLAYERMOUNT(vp0->v_mount)->layerm_flags;
	our_vnodeop_p = vp0->v_op;

	if (flags & LAYERFS_MBYPASSDEBUG)
		printf ("layer_bypass: %s\n", descp->vdesc_name);

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
			VOPARG_OFFSETTO(struct vnode**,descp->vdesc_vp_offsets[i],ap);
		/*
		 * We're not guaranteed that any but the first vnode
		 * are of our type.  Check for and don't map any
		 * that aren't.  (We must always map first vp or vclean fails.)
		 */
		if (i && (*this_vp_p == NULL ||
		    (*this_vp_p)->v_op != our_vnodeop_p)) {
			old_vps[i] = NULL;
		} else {
			old_vps[i] = *this_vp_p;
			*(vps_p[i]) = LAYERVPTOLOWERVP(*this_vp_p);
			/*
			 * XXX - Several operations have the side effect
			 * of vrele'ing their vp's.  We must account for
			 * that.  (This should go away in the future.)
			 */
			if (reles & VDESC_VP0_WILLRELE)
				VREF(*this_vp_p);
		}
			
	}

	/*
	 * Call the operation on the lower layer
	 * with the modified argument structure.
	 */
	error = VCALL(*vps_p[0], descp->vdesc_offset, ap);

	/*
	 * Maintain the illusion of call-by-value
	 * by restoring vnodes in the argument structure
	 * to their original value.
	 */
	reles = descp->vdesc_flags;
	for (i = 0; i < VDESC_MAX_VPS; reles >>= 1, i++) {
		if (descp->vdesc_vp_offsets[i] == VDESC_NO_OFFSET)
			break;   /* bail out at end of list */
		if (old_vps[i]) {
			*(vps_p[i]) = old_vps[i];
			if (reles & VDESC_VP0_WILLUNLOCK)
				LAYERFS_UPPERUNLOCK(*(vps_p[i]), 0, error1);
			if (reles & VDESC_VP0_WILLRELE)
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
		 * Only vop_lookup, vop_create, vop_makedir, vop_bmap,
		 * vop_mknod, and vop_symlink return vpp's. vop_bmap
		 * doesn't call bypass as the lower vpp is fine (we're just
		 * going to do i/o on it). vop_lookup doesn't call bypass
		 * as a lookup on "." would generate a locking error.
		 * So all the calls which get us here have a locked vpp. :-)
		 */
		error = layer_node_create(old_vps[0]->v_mount, **vppp, *vppp);
	}

 out:
	return (error);
}

/*
 * We have to carry on the locking protocol on the layer vnodes
 * as we progress through the tree. We also have to enforce read-only
 * if this layer is mounted read-only.
 */
int
layer_lookup(v)
	void *v;
{
	struct vop_lookup_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap = v;
	struct componentname *cnp = ap->a_cnp;
	int flags = cnp->cn_flags;
	struct vnode *dvp, *vp, *ldvp;
	int error, r;

	dvp = ap->a_dvp;
	*ap->a_vpp = NULL;

	if ((flags & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	ldvp = LAYERVPTOLOWERVP(dvp);
	ap->a_dvp = ldvp;
	error = VCALL(ldvp, ap->a_desc->vdesc_offset, ap);
	vp = *ap->a_vpp;

	if (error == EJUSTRETURN && (flags & ISLASTCN) &&
	    (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME))
		error = EROFS;
	/*
	 * We must do the same locking and unlocking at this layer as 
	 * is done in the layers below us. It used to be we would try
	 * to guess based on what was set with the flags and error codes.
	 *
	 * But that doesn't work. So now we have the underlying VOP_LOOKUP
	 * tell us if it released the parent vnode, and we adjust the
	 * upper node accordingly. We can't just look at the lock states
	 * of the lower nodes as someone else might have come along and
	 * locked the parent node after our call to VOP_LOOKUP locked it.
	 */
	if ((cnp->cn_flags & PDIRUNLOCK)) {
		LAYERFS_UPPERUNLOCK(dvp, 0, r);
	}
	if (ldvp == vp) {
		/*
		 * Did lookup on "." or ".." in the root node of a mount point.
		 * So we return dvp after a VREF.
		 */
		*ap->a_vpp = dvp;
		VREF(dvp);
		vrele(vp);
	} else if (vp != NULL) {
		error = layer_node_create(dvp->v_mount, vp, ap->a_vpp);
	}
	return (error);
}

/*
 * Setattr call. Disallow write attempts if the layer is mounted read-only.
 */
int
layer_setattr(v)
	void *v;
{
	struct vop_setattr_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;

  	if ((vap->va_flags != VNOVAL || vap->va_uid != (uid_t)VNOVAL ||
	    vap->va_gid != (gid_t)VNOVAL || vap->va_atime.tv_sec != VNOVAL ||
	    vap->va_mtime.tv_sec != VNOVAL || vap->va_mode != (mode_t)VNOVAL) &&
	    (vp->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);
	if (vap->va_size != VNOVAL) {
 		switch (vp->v_type) {
 		case VDIR:
 			return (EISDIR);
 		case VCHR:
 		case VBLK:
 		case VSOCK:
 		case VFIFO:
			return (0);
		case VREG:
		case VLNK:
 		default:
			/*
			 * Disallow write attempts if the filesystem is
			 * mounted read-only.
			 */
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
		}
	}
	return (LAYERFS_DO_BYPASS(vp, ap));
}

/*
 *  We handle getattr only to change the fsid.
 */
int
layer_getattr(v)
	void *v;
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	int error;

	if ((error = LAYERFS_DO_BYPASS(vp, ap)) != 0)
		return (error);
	/* Requires that arguments be restored. */
	ap->a_vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	return (0);
}

int
layer_access(v)
	void *v;
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	mode_t mode = ap->a_mode;

	/*
	 * Disallow write attempts on read-only layers;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the file system.
	 */
	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			break;
		default:
			break;
		}
	}
	return (LAYERFS_DO_BYPASS(vp, ap));
}

/*
 * We must handle open to be able to catch MNT_NODEV and friends.
 */
int
layer_open(v)
	void *v;
{
	struct vop_open_args *ap = v;
	struct vnode *vp = ap->a_vp;
	enum vtype lower_type = LAYERVPTOLOWERVP(vp)->v_type;

	if (((lower_type == VBLK) || (lower_type == VCHR)) &&
	    (vp->v_mount->mnt_flag & MNT_NODEV))
		return ENXIO;

	return LAYERFS_DO_BYPASS(vp, ap);
}

/*
 * We need to process our own vnode lock and then clear the
 * interlock flag as it applies only to our vnode, not the
 * vnodes below us on the stack.
 */
int
layer_lock(v)
	void *v;
{
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp, *lowervp;
	int	flags = ap->a_flags, error;
	struct proc *p = ap->a_p;

	if (vp->v_flag & VXLOCK) /* XXX this is a disaster -tedu XXX */
		return (0);

	if (vp->v_vnlock != NULL) {
		/*
		 * The lower level has exported a struct lock to us. Use
		 * it so that all vnodes in the stack lock and unlock
		 * simultaneously. Note: we don't DRAIN the lock as DRAIN
		 * decommissions the lock - just because our vnode is
		 * going away doesn't mean the struct lock below us is.
		 * LK_EXCLUSIVE is fine.
		 */
		if ((flags & LK_TYPE_MASK) == LK_DRAIN)
			return (lockmgr(vp->v_vnlock, (flags & ~LK_TYPE_MASK) |
			    LK_EXCLUSIVE, &vp->v_interlock, p));
		else
			return (lockmgr(vp->v_vnlock, flags, &vp->v_interlock, p));
	} else {
		/*
		 * Ahh well. It would be nice if the fs we're over would
		 * export a struct lock for us to use, but it doesn't.
		 *
		 * To prevent race conditions involving doing a lookup
		 * on "..", we have to lock the lower node, then lock our
		 * node. Most of the time it won't matter that we lock our
		 * node (as any locking would need the lower one locked
		 * first). But we can LK_DRAIN the upper lock as a step
		 * towards decomissioning it.
		 */
		lowervp = LAYERVPTOLOWERVP(vp);
		if (flags & LK_INTERLOCK) {
			simple_unlock(&vp->v_interlock);
			flags &= ~LK_INTERLOCK;
		}
		if ((flags & LK_TYPE_MASK) == LK_DRAIN) {
			error = VOP_LOCK(lowervp, (flags & ~LK_TYPE_MASK) |
			    LK_EXCLUSIVE, p);
		} else {
			error = VOP_LOCK(lowervp, flags, p);
		}
		if (error)
			return (error);
		if ((error = lockmgr(&vp->v_lock, flags, &vp->v_interlock, p))) {
			VOP_UNLOCK(lowervp, 0, p);
		}
		return (error);
	}
}

/*
 */
int
layer_unlock(v)
	void *v;
{
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	int	flags = ap->a_flags;
	struct proc *p = ap->a_p;

	if (vp->v_flag & VXLOCK)
		return (0);

	if (vp->v_vnlock != NULL) {
		return (lockmgr(vp->v_vnlock, ap->a_flags | LK_RELEASE,
		    &vp->v_interlock, p));
	} else {
		if (flags & LK_INTERLOCK) {
			simple_unlock(&vp->v_interlock);
			flags &= ~LK_INTERLOCK;
		}
		VOP_UNLOCK(LAYERVPTOLOWERVP(vp), flags, p);
		return (lockmgr(&vp->v_lock, ap->a_flags | LK_RELEASE,
		    &vp->v_interlock, p));
	}
}

/*
 * As long as genfs_nolock is in use, don't call VOP_ISLOCKED(lowervp)
 * if vp->v_vnlock == NULL as genfs_noislocked will always report 0.
 */
int
layer_islocked(v)
	void *v;
{
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;

	if (vp->v_vnlock != NULL)
		return (lockstatus(vp->v_vnlock));
	else
		return (lockstatus(&vp->v_lock));
}

/*
 * If vinvalbuf is calling us, it's a "shallow fsync" -- don't bother
 * syncing the underlying vnodes, since they'll be fsync'ed when
 * reclaimed; otherwise,
 * pass it through to the underlying layer.
 *
 * XXX Do we still need to worry about shallow fsync?
 */

int
layer_fsync(v)
	void *v;
{
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int  a_flags;
		off_t offlo;
		off_t offhi;
		struct proc *a_p;
	} */ *ap = v;

	return (LAYERFS_DO_BYPASS(ap->a_vp, ap));
}


int
layer_inactive(v)
	void *v;
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct proc *p = ap->a_p;

	/*
	 * Do nothing (and _don't_ bypass).
	 * Wait to vrele lowervp until reclaim,
	 * so that until then our layer_node is in the
	 * cache and reusable.
	 *
	 * NEEDSWORK: Someday, consider inactive'ing
	 * the lowervp and then trying to reactivate it
	 * with capabilities (v_id)
	 * like they do in the name lookup cache code.
	 * That's too much work for now.
	 */
	VOP_UNLOCK(vp, 0, p);

	/* ..., but don't cache the device node. */
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		vgone(vp);
	return (0);
}

int
layer_reclaim(v)
	void *v;
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct layer_mount *lmp = MOUNTTOLAYERMOUNT(vp->v_mount);
	struct layer_node *xp = VTOLAYER(vp);
	struct vnode *lowervp = xp->layer_lowervp;

	/*
	 * Note: in vop_reclaim, the node's struct lock has been
	 * decomissioned, so we have to be careful about calling
	 * VOP's on ourself. Even if we turned a LK_DRAIN into an
	 * LK_EXCLUSIVE in layer_lock, we still must be careful as VXLOCK is
	 * set.
	 */
	/* After this assignment, this node will not be re-used. */
	if ((vp == lmp->layerm_rootvp)) {
		/*
		 * Oops! We no longer have a root node. Most likely reason is
		 * that someone forcably unmunted the underlying fs.
		 *
		 * Now getting the root vnode will fail. We're dead. :-(
		 */
		lmp->layerm_rootvp = NULL;
	}
	xp->layer_lowervp = NULL;
	simple_lock(&lmp->layerm_hashlock);
	LIST_REMOVE(xp, layer_hash);
	simple_unlock(&lmp->layerm_hashlock);
	FREE(vp->v_data, M_TEMP);
	vp->v_data = NULL;
	vrele (lowervp);
	return (0);
}

/*
 * We just feed the returned vnode up to the caller - there's no need
 * to build a layer node on top of the node on which we're going to do
 * i/o. :-)
 */
int
layer_bmap(v)
	void *v;
{
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap = v;
	struct vnode *vp;

	ap->a_vp = vp = LAYERVPTOLOWERVP(ap->a_vp);

	return (VCALL(vp, ap->a_desc->vdesc_offset, ap));
}

int
layer_print(v)
	void *v;
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	printf ("\ttag VT_LAYERFS, vp=%p, lowervp=%p\n", vp, LAYERVPTOLOWERVP(vp));
	VOP_PRINT(LAYERVPTOLOWERVP(vp));
	return (0);
}

/*
 * XXX - vop_strategy must be hand coded because it has no
 * vnode in its arguments.
 * This goes away with a merged VM/buffer cache.
 */
int
layer_strategy(v)
	void *v;
{
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap = v;
	struct buf *bp = ap->a_bp;
	int error;
	struct vnode *savedvp;

	savedvp = bp->b_vp;
	bp->b_vp = LAYERVPTOLOWERVP(bp->b_vp);

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
layer_bwrite(v)
	void *v;
{
	struct vop_bwrite_args /* {
		struct buf *a_bp;
	} */ *ap = v;
	struct buf *bp = ap->a_bp;
	int error;
	struct vnode *savedvp;

	savedvp = bp->b_vp;
	bp->b_vp = LAYERVPTOLOWERVP(bp->b_vp);

	error = VOP_BWRITE(bp);

	bp->b_vp = savedvp;

	return (error);
}

#if 0
int
layer_getpages(v)
	void *v;
{
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		voff_t a_offset;
		struct vm_page **a_m;
		int *a_count;
		int a_centeridx;
		vm_prot_t a_access_type;
		int a_advice;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	int error;

	/*
	 * just pass the request on to the underlying layer.
	 */

	if (ap->a_flags & PGO_LOCKED) {
		return EBUSY;
	}
	ap->a_vp = LAYERVPTOLOWERVP(vp);
	simple_unlock(&vp->v_interlock);
	simple_lock(&ap->a_vp->v_interlock);
	error = VCALL(ap->a_vp, VOFFSET(vop_getpages), ap);
	return error;
}

int
layer_putpages(v)
	void *v;
{
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		voff_t a_offlo;
		voff_t a_offhi;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	int error;

	/*
	 * just pass the request on to the underlying layer.
	 */

	ap->a_vp = LAYERVPTOLOWERVP(vp);
	simple_unlock(&vp->v_interlock);
	simple_lock(&ap->a_vp->v_interlock);
	error = VCALL(ap->a_vp, VOFFSET(vop_putpages), ap);
	return error;
}
#endif

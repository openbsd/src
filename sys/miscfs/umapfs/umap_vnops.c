/*	$OpenBSD: umap_vnops.c,v 1.19 2003/10/24 19:13:22 tedu Exp $ */
/*	$NetBSD: umap_vnops.c,v 1.22 2002/01/04 07:19:34 chs Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * the UCLA Ficus project.
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
 *	@(#)umap_vnops.c	8.6 (Berkeley) 5/22/95
 */

/*
 * Umap Layer
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <miscfs/umapfs/umap.h>
#include <miscfs/genfs/layer_extern.h>

int	umap_lookup(void *);
int	umap_getattr(void *);
int	umap_print(void *);
int	umap_rename(void *);

/*
 * Global vfs data structures
 */
/*
 * XXX - strategy, bwrite are hand coded currently.  They should
 * go away with a merged buffer/block cache.
 *
 */
int (**umapfs_vnodeop_p)(void *);
struct vnodeopv_entry_desc umapfs_vnodeop_entries[] = {
	{ &vop_default_desc,	umap_bypass },

	{ &vop_lookup_desc,	umap_lookup },
	{ &vop_getattr_desc,	umap_getattr },
	{ &vop_print_desc,	umap_print },
	{ &vop_rename_desc,	umap_rename },

	{ &vop_lock_desc,	layer_lock },
	{ &vop_unlock_desc,	layer_unlock },
	{ &vop_islocked_desc,	layer_islocked },
	{ &vop_fsync_desc,	layer_fsync },
	{ &vop_inactive_desc,	layer_inactive },
	{ &vop_reclaim_desc,	layer_reclaim },
	{ &vop_open_desc,	layer_open },
	{ &vop_setattr_desc,	layer_setattr },
	{ &vop_access_desc,	layer_access },

	{ &vop_strategy_desc,	layer_strategy },
	{ &vop_bwrite_desc,	layer_bwrite },
	{ &vop_bmap_desc,	layer_bmap },
	{ NULL, NULL }
};
const struct vnodeopv_desc umapfs_vnodeop_opv_desc =
	{ &umapfs_vnodeop_p, umapfs_vnodeop_entries };

/*
 * This is the 08-June-1999 bypass routine.
 * See layer_vnops.c:layer_bypass for more details.
 */ 
int
umap_bypass(v)
	void *v;
{
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
		<other random data follows, presumably>
	} */ *ap = v;
	struct ucred **credpp = 0, *credp = 0;
	struct ucred *savecredp = 0, *savecompcredp = 0;
	struct ucred *compcredp = 0;
	struct vnode **this_vp_p;
	int error, error1;
	int (**our_vnodeop_p)(void *);
	struct vnode *old_vps[VDESC_MAX_VPS], *vp0;
	struct vnode **vps_p[VDESC_MAX_VPS];
	struct vnode ***vppp;
	struct vnodeop_desc *descp = ap->a_desc;
	int reles, i, flags;
	struct componentname **compnamepp = 0;

#ifdef SAFETY
	/*
	 * We require at least one vp.
	 */
	if (descp->vdesc_vp_offsets == NULL ||
	    descp->vdesc_vp_offsets[0] == VDESC_NO_OFFSET)
		panic("umap_bypass: no vp's in map");
#endif
	vps_p[0] = VOPARG_OFFSETTO(struct vnode**,descp->vdesc_vp_offsets[0],
				ap);
	vp0 = *vps_p[0];
	flags = MOUNTTOUMAPMOUNT(vp0->v_mount)->umapm_flags;
	our_vnodeop_p = vp0->v_op;

	if (flags & LAYERFS_MBYPASSDEBUG)
		printf("umap_bypass: %s\n", descp->vdesc_name);

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
			VOPARG_OFFSETTO(struct vnode**, descp->vdesc_vp_offsets[i], ap);

		/*
		 * We're not guaranteed that any but the first vnode
		 * are of our type.  Check for and don't map any
		 * that aren't.  (Must map first vp or vclean fails.)
		 */

		if (i && ((*this_vp_p)==NULL ||
		    (*this_vp_p)->v_op != our_vnodeop_p)) {
			old_vps[i] = NULL;
		} else {
			old_vps[i] = *this_vp_p;
			*(vps_p[i]) = UMAPVPTOLOWERVP(*this_vp_p);
			if (reles & 1)
				VREF(*this_vp_p);
		}
			
	}

	/*
	 * Fix the credentials.  (That's the purpose of this layer.)
	 */

	if (descp->vdesc_cred_offset != VDESC_NO_OFFSET) {

		credpp = VOPARG_OFFSETTO(struct ucred**, 
		    descp->vdesc_cred_offset, ap);

		/* Save old values */

		savecredp = *credpp;
		if (savecredp != NOCRED)
			*credpp = crdup(savecredp);
		credp = *credpp;

		if ((flags & LAYERFS_MBYPASSDEBUG) && credp->cr_uid != 0)
			printf("umap_bypass: user was %d, group %d\n", 
			    credp->cr_uid, credp->cr_gid);

		/* Map all ids in the credential structure. */

		umap_mapids(vp0->v_mount, credp);

		if ((flags & LAYERFS_MBYPASSDEBUG) && credp->cr_uid != 0)
			printf("umap_bypass: user now %d, group %d\n", 
			    credp->cr_uid, credp->cr_gid);
	}

	/* BSD often keeps a credential in the componentname structure
	 * for speed.  If there is one, it better get mapped, too. 
	 */

	if (descp->vdesc_componentname_offset != VDESC_NO_OFFSET) {

		compnamepp = VOPARG_OFFSETTO(struct componentname**, 
		    descp->vdesc_componentname_offset, ap);

		savecompcredp = (*compnamepp)->cn_cred;
		if (savecompcredp != NOCRED)
			(*compnamepp)->cn_cred = crdup(savecompcredp);
		compcredp = (*compnamepp)->cn_cred;

		if ((flags & LAYERFS_MBYPASSDEBUG) && compcredp->cr_uid != 0)
			printf("umap_bypass: component credit user was %d, group %d\n", 
			    compcredp->cr_uid, compcredp->cr_gid);

		/* Map all ids in the credential structure. */

		umap_mapids(vp0->v_mount, compcredp);

		if ((flags & LAYERFS_MBYPASSDEBUG) && compcredp->cr_uid != 0)
			printf("umap_bypass: component credit user now %d, group %d\n", 
			    compcredp->cr_uid, compcredp->cr_gid);
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
		if (descp->vdesc_flags & VDESC_VPP_WILLRELE)
			goto out;
		vppp = VOPARG_OFFSETTO(struct vnode***,
				 descp->vdesc_vpp_offset, ap);
		error = layer_node_create(old_vps[0]->v_mount, **vppp, *vppp);
	}

 out:
	/* 
	 * Free duplicate cred structure and restore old one.
	 */
	if (descp->vdesc_cred_offset != VDESC_NO_OFFSET) {
		if ((flags & LAYERFS_MBYPASSDEBUG) && credp &&
					credp->cr_uid != 0)
			printf("umap_bypass: returning-user was %d\n",
			    credp->cr_uid);

		if (savecredp != NOCRED) {
			crfree(credp);
			*credpp = savecredp;
			if ((flags & LAYERFS_MBYPASSDEBUG) && credpp &&
					(*credpp)->cr_uid != 0)
			 	printf("umap_bypass: returning-user now %d\n\n", 
				    savecredp->cr_uid);
		}
	}

	if (descp->vdesc_componentname_offset != VDESC_NO_OFFSET) {
		if ((flags & LAYERFS_MBYPASSDEBUG) && compcredp &&
					compcredp->cr_uid != 0)
			printf("umap_bypass: returning-component-user was %d\n", 
			    compcredp->cr_uid);

		if (savecompcredp != NOCRED) {
			crfree(compcredp);
			(*compnamepp)->cn_cred = savecompcredp;
			if ((flags & LAYERFS_MBYPASSDEBUG) && savecompcredp &&
					savecompcredp->cr_uid != 0)
			 	printf("umap_bypass: returning-component-user now %d\n", 
				    savecompcredp->cr_uid);
		}
	}

	return (error);
}

/*
 * This is based on the 08-June-1999 bypass routine.
 * See layer_vnops.c:layer_bypass for more details.
 */ 
int
umap_lookup(v)
	void *v;
{
	struct vop_lookup_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap = v;
	struct componentname *cnp = ap->a_cnp;
	struct ucred *savecompcredp = NULL;
	struct ucred *compcredp = NULL;
	struct vnode *dvp, *vp, *ldvp;
	struct mount *mp;
	int error;
	int i, flags, cnf = cnp->cn_flags;

	dvp = ap->a_dvp;
	mp = dvp->v_mount;

	if ((cnf & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
		(cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	flags = MOUNTTOUMAPMOUNT(mp)->umapm_flags;
	ldvp = UMAPVPTOLOWERVP(dvp);

	if (flags & LAYERFS_MBYPASSDEBUG)
		printf("umap_lookup\n");

	/*
	 * Fix the credentials.  (That's the purpose of this layer.)
	 *
	 * BSD often keeps a credential in the componentname structure
	 * for speed.  If there is one, it better get mapped, too. 
	 */

	if ((savecompcredp = cnp->cn_cred)) {
		compcredp = crdup(savecompcredp);
		cnp->cn_cred = compcredp;

		if ((flags & LAYERFS_MBYPASSDEBUG) && compcredp->cr_uid != 0)
			printf("umap_lookup: component credit user was %d, group %d\n", 
			    compcredp->cr_uid, compcredp->cr_gid);

		/* Map all ids in the credential structure. */
		umap_mapids(mp, compcredp);
	}

	if ((flags & LAYERFS_MBYPASSDEBUG) && compcredp->cr_uid != 0)
		printf("umap_lookup: component credit user now %d, group %d\n", 
		    compcredp->cr_uid, compcredp->cr_gid);

	ap->a_dvp = ldvp;
	error = VCALL(ldvp, ap->a_desc->vdesc_offset, ap);
	vp = *ap->a_vpp;

	if (error == EJUSTRETURN && (cnf & ISLASTCN) &&
	    (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME))
		error = EROFS;

	/* Do locking fixup as appropriate. See layer_lookup() for info */
	if ((cnp->cn_flags & PDIRUNLOCK)) {
		LAYERFS_UPPERUNLOCK(dvp, 0, i);
	}
	if (ldvp == vp) {
		*ap->a_vpp = dvp;
		VREF(dvp);
		vrele(vp);
	} else if (vp != NULL) {
		error = layer_node_create(mp, vp, ap->a_vpp);
	}

	/* 
	 * Free duplicate cred structure and restore old one.
	 */
	if ((flags & LAYERFS_MBYPASSDEBUG) && compcredp &&
					compcredp->cr_uid != 0)
		printf("umap_lookup: returning-component-user was %d\n", 
			    compcredp->cr_uid);

	if (savecompcredp != NOCRED) {
		crfree(compcredp);
		cnp->cn_cred = savecompcredp;
		if ((flags & LAYERFS_MBYPASSDEBUG) && savecompcredp &&
				savecompcredp->cr_uid != 0)
		 	printf("umap_lookup: returning-component-user now %d\n", 
			    savecompcredp->cr_uid);
	}

	return (error);
}

/*
 *  We handle getattr to change the fsid.
 */
int
umap_getattr(v)
	void *v;
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	uid_t uid;
	gid_t gid;
	int error, tmpid, unentries, gnentries, flags;
	u_long (*umapdata)[2];
	u_long (*gmapdata)[2];
	struct vnode **vp1p;
	const struct vnodeop_desc *descp = ap->a_desc;

	if ((error = umap_bypass(ap)) != 0)
		return (error);
	/* Requires that arguments be restored. */
	ap->a_vap->va_fsid = ap->a_vp->v_mount->mnt_stat.f_fsid.val[0];

	flags = MOUNTTOUMAPMOUNT(ap->a_vp->v_mount)->umapm_flags;
	/*
	 * Umap needs to map the uid and gid returned by a stat
	 * into the proper values for this site.  This involves
	 * finding the returned uid in the mapping information,
	 * translating it into the uid on the other end,
	 * and filling in the proper field in the vattr
	 * structure pointed to by ap->a_vap.  The group
	 * is easier, since currently all groups will be
	 * translate to the NULLGROUP.
	 */

	/* Find entry in map */

	uid = ap->a_vap->va_uid;
	gid = ap->a_vap->va_gid;
	if ((flags & LAYERFS_MBYPASSDEBUG))
		printf("umap_getattr: mapped uid = %d, mapped gid = %d\n", uid, 
		    gid);

	vp1p = VOPARG_OFFSETTO(struct vnode**, descp->vdesc_vp_offsets[0], ap);
	unentries =  MOUNTTOUMAPMOUNT((*vp1p)->v_mount)->info_unentries;
	umapdata =  (MOUNTTOUMAPMOUNT((*vp1p)->v_mount)->info_umapdata);
	gnentries =  MOUNTTOUMAPMOUNT((*vp1p)->v_mount)->info_gnentries;
	gmapdata =  (MOUNTTOUMAPMOUNT((*vp1p)->v_mount)->info_gmapdata);

	/* Reverse map the uid for the vnode.  Since it's a reverse
		map, we can't use umap_mapids() to do it. */

	tmpid = umap_reverse_findid(uid, umapdata, unentries);

	if (tmpid != -1) {
		ap->a_vap->va_uid = (uid_t) tmpid;
		if ((flags & LAYERFS_MBYPASSDEBUG))
			printf("umap_getattr: original uid = %d\n", uid);
	} else 
		ap->a_vap->va_uid = (uid_t) NOBODY;

	/* Reverse map the gid for the vnode. */

	tmpid = umap_reverse_findid(gid, gmapdata, gnentries);

	if (tmpid != -1) {
		ap->a_vap->va_gid = (gid_t) tmpid;
		if ((flags & LAYERFS_MBYPASSDEBUG))
			printf("umap_getattr: original gid = %d\n", gid);
	} else
		ap->a_vap->va_gid = (gid_t) NULLGROUP;
	
	return (0);
}

int
umap_print(v)
	void *v;
{
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	printf("\ttag VT_UMAPFS, vp=%p, lowervp=%p\n", vp,
	    UMAPVPTOLOWERVP(vp));
	return (0);
}

int
umap_rename(v)
	void *v;
{
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap = v;
	int error, flags;
	struct componentname *compnamep;
	struct ucred *compcredp, *savecompcredp;
	struct vnode *vp;

	/*
	 * Rename is irregular, having two componentname structures.
	 * We need to map the cre in the second structure,
	 * and then bypass takes care of the rest.
	 */

	vp = ap->a_fdvp;
	flags = MOUNTTOUMAPMOUNT(vp->v_mount)->umapm_flags;
	compnamep = ap->a_tcnp;
	compcredp = compnamep->cn_cred;

	savecompcredp = compcredp;
	compcredp = compnamep->cn_cred = crdup(savecompcredp);

	if ((flags & LAYERFS_MBYPASSDEBUG) && compcredp->cr_uid != 0)
		printf("umap_rename: rename component credit user was %d, group %d\n", 
		    compcredp->cr_uid, compcredp->cr_gid);

	/* Map all ids in the credential structure. */

	umap_mapids(vp->v_mount, compcredp);

	if ((flags & LAYERFS_MBYPASSDEBUG) && compcredp->cr_uid != 0)
		printf("umap_rename: rename component credit user now %d, group %d\n", 
		    compcredp->cr_uid, compcredp->cr_gid);

	error = umap_bypass(ap);
	
	/* Restore the additional mapped componentname cred structure. */

	crfree(compcredp);
	compnamep->cn_cred = savecompcredp;

	return error;
}

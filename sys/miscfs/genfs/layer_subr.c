/*	$OpenBSD: layer_subr.c,v 1.1 2003/05/12 20:58:40 tedu Exp $ */
/*	$NetBSD: layer_subr.c,v 1.11 2002/02/20 06:16:22 enami Exp $	*/

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
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
 *	from: Id: lofs_subr.c,v 1.11 1992/05/30 10:05:43 jsp Exp
 *	@(#)null_subr.c	8.7 (Berkeley) 5/14/95
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <miscfs/specfs/specdev.h>
#include <miscfs/genfs/layer.h>
#include <miscfs/genfs/layer_extern.h>

#define	NLAYERNODECACHE 16

/*
 * layer cache:
 * Each cache entry holds a reference to the lower vnode
 * along with a pointer to the alias vnode.  When an
 * entry is added the lower vnode is VREF'd.  When the
 * alias is removed the lower vnode is vrele'd.
 */

/*
 * Initialise cache headers
 */
int
layerfs_init(struct vfsconf *v)
{
#ifdef LAYERFS_DIAGNOSTIC
	printf("layerfs_init\n");		/* printed during system boot */
#endif
	return (0);
}

/*
 * Free global resources of layerfs.
 */
void
layerfs_done()
{
#ifdef LAYERFS_DIAGNOSTIC
	printf("layerfs_done\n");		/* printed on layerfs detach */
#endif
}

/*
 * Return a locked, VREF'ed alias for lower vnode if already exists, else 0.
 */
struct vnode *
layer_node_find(mp, lowervp)
	struct mount *mp;
	struct vnode *lowervp;
{
	struct layer_mount *lmp = MOUNTTOLAYERMOUNT(mp);
	struct layer_node_hashhead *hd;
	struct layer_node *a;
	struct vnode *vp;

	/*
	 * Find hash base, and then search the (two-way) linked
	 * list looking for a layer_node structure which is referencing
	 * the lower vnode.  If found, the increment the layer_node
	 * reference count (but NOT the lower vnode's VREF counter)
	 * and return the vnode locked.
	 */
	hd = LAYER_NHASH(lmp, lowervp);
loop:
	simple_lock(&lmp->layerm_hashlock);
	for (a = hd->lh_first; a != 0; a = a->layer_hash.le_next) {
		if (a->layer_lowervp == lowervp && LAYERTOV(a)->v_mount == mp) {
			vp = LAYERTOV(a);
			simple_unlock(&lmp->layerm_hashlock);
			/*
			 * We must be careful here as the fact the lower
			 * vnode is locked will imply vp is locked unless
			 * someone has decided to start vclean'ing either
			 * vp or lowervp.
			 *
			 * So we try for an exclusive, recursive lock
			 * on the upper vnode. If it fails, vcleaning
			 * is in progress (so when we try again, we'll
			 * fail). If it succeeds, we now have double
			 * locked the bottom node. So we do an explicit
			 * VOP_UNLOCK on it to keep the counts right. Note
			 * that we will end up with the upper node and
			 * the lower node locked once.
			 */
			if (vget(vp, LK_EXCLUSIVE | LK_CANRECURSE, curproc)) {
				printf ("layer_node_find: vget failed.\n");
				goto loop;
			}
			VOP_UNLOCK(lowervp, 0, curproc);
			return (vp);
		}
	}

	simple_unlock(&lmp->layerm_hashlock);
	return NULL;
}


/*
 * Make a new layer_node node.
 * Vp is the alias vnode, lowervp is the lower vnode.
 * Maintain a reference to lowervp.
 */
int
layer_node_alloc(mp, lowervp, vpp)
	struct mount *mp;
	struct vnode *lowervp;
	struct vnode **vpp;
{
	struct layer_mount *lmp = MOUNTTOLAYERMOUNT(mp);
	struct layer_node_hashhead *hd;
	struct layer_node *xp;
	struct vnode *vp, *nvp;
	int error;
	extern int (**dead_vnodeop_p)(void *);

	if ((error = getnewvnode(lmp->layerm_tag, mp, lmp->layerm_vnodeop_p,
			&vp)) != 0)
		return (error);
	vp->v_type = lowervp->v_type;
	vp->v_flag |= VLAYER;

	xp = malloc(lmp->layerm_size, M_TEMP, M_WAITOK);
	if (vp->v_type == VBLK || vp->v_type == VCHR) {
		MALLOC(vp->v_specinfo, struct specinfo *,
		    sizeof(struct specinfo), M_VNODE, M_WAITOK);
		vp->v_hashchain = NULL;
		vp->v_rdev = lowervp->v_rdev;
	}

	vp->v_data = xp;
	xp->layer_vnode = vp;
	xp->layer_lowervp = lowervp;
	xp->layer_flags = 0;
	/*
	 * Before we insert our new node onto the hash chains,
	 * check to see if someone else has beaten us to it.
	 * (We could have slept in MALLOC.)
	 */
	if ((nvp = layer_node_find(mp, lowervp)) != NULL) {
		*vpp = nvp;

		/* free the substructures we've allocated. */
		FREE(xp, M_TEMP);
		if (vp->v_type == VBLK || vp->v_type == VCHR)
			FREE(vp->v_specinfo, M_VNODE);

		vp->v_type = VBAD;		/* node is discarded */
		vp->v_op = dead_vnodeop_p;	/* so ops will still work */
		vrele(vp);			/* get rid of it. */
		return (0);
	}

	simple_lock(&lmp->layerm_hashlock);

	/*
	 * Now lock the new node. We rely on the fact that we were passed
	 * a locked vnode. If the lower node is exporting a struct lock
	 * (v_vnlock != NULL) then we just set the upper v_vnlock to the
	 * lower one, and both are now locked. If the lower node is exporting
	 * NULL, then we copy that up and manually lock the upper node.
	 *
	 * LAYERFS_UPPERLOCK already has the test, so we use it after copying
	 * up the v_vnlock from below.
	 */

	vp->v_vnlock = lowervp->v_vnlock;
	LAYERFS_UPPERLOCK(vp, LK_EXCLUSIVE, error);

	if (error) {
		/*
		 * How did we get a locking error? The node just came off
		 * of the free list, and we're the only routine which
		 * knows it's there...
		 */
		vp->v_vnlock = &vp->v_lock;
		*vpp = NULL;

		/* free the substructures we've allocated. */
		FREE(xp, M_TEMP);
		if (vp->v_type == VBLK || vp->v_type == VCHR)
			FREE(vp->v_specinfo, M_VNODE);

		vp->v_type = VBAD;		/* node is discarded */
		vp->v_op = dead_vnodeop_p;	/* so ops will still work */
		vrele(vp);			/* get rid of it. */
		return (error);
	}
	/*
	 * NetBSD used to do an inlined checkalias here. We do not, as
	 * we never flag device nodes as being aliased. The lowervp
	 * node will, when appropriate, be flaged as an alias.
	 */

	*vpp = vp;
	VREF(lowervp);	/* Take into account reference held in layer_node */
	hd = LAYER_NHASH(lmp, lowervp);
	LIST_INSERT_HEAD(hd, xp, layer_hash);
	uvm_vnp_setsize(vp, 0);
	simple_unlock(&lmp->layerm_hashlock);
	return (0);
}


/*
 * Try to find an existing layer_node vnode refering
 * to it, otherwise make a new layer_node vnode which
 * contains a reference to the lower vnode.
 *
 * >>> we assume that the lower node is already locked upon entry, so we
 * propagate the lock state to upper node <<
 */
int
layer_node_create(mp, lowervp, newvpp)
	struct mount *mp;
	struct vnode *lowervp;
	struct vnode **newvpp;
{
	struct vnode *aliasvp;
	struct layer_mount *lmp = MOUNTTOLAYERMOUNT(mp);

	if ((aliasvp = layer_node_find(mp, lowervp)) != NULL) {
		/*
		 * layer_node_find has taken another reference
		 * to the alias vnode and moved the lock holding to
		 * aliasvp
		 */
#ifdef LAYERFS_DIAGNOSTIC
		vprint("layer_node_create: exists", aliasvp);
#endif
	} else {
		int error;

		/*
		 * Get new vnode.
		 */
#ifdef LAYERFS_DIAGNOSTIC
		printf("layer_node_create: create new alias vnode\n");
#endif

		/*
		 * Make new vnode reference the layer_node.
		 */
		if ((error = (lmp->layerm_alloc)(mp, lowervp, &aliasvp)) != 0)
			return error;

		/*
		 * aliasvp is already VREF'd by getnewvnode()
		 */
	}

	/*
	 * Now that we have VREF'd the upper vnode, release the reference
	 * to the lower node. The existance of the layer_node retains one
	 * reference to the lower node.
	 */
	vrele(lowervp);

#ifdef DIAGNOSTIC
	if (lowervp->v_usecount < 1) {
		/* Should never happen... */
		vprint("layer_node_create: alias", aliasvp);
		vprint("layer_node_create: lower", lowervp);
		panic("layer_node_create: lower has 0 usecount.");
	}
#endif

#ifdef LAYERFS_DIAGNOSTIC
	vprint("layer_node_create: alias", aliasvp);
#endif
	*newvpp = aliasvp;
	return (0);
}

struct vnode *
layer_checkvp(vp, fil, lno)
	struct vnode *vp;
	char *fil;
	int lno;
{
	struct layer_node *a = VTOLAYER(vp);
#ifdef notyet
	/*
	 * Can't do this check because vop_reclaim runs
	 * with a funny vop vector.
	 *
	 * WRS - no it doesnt...
	 */
	if (vp->v_op != layer_vnodeop_p) {
		printf ("layer_checkvp: on non-layer-node\n");
#ifdef notyet
		while (layer_checkvp_barrier) /*WAIT*/ ;
#endif
		panic("layer_checkvp");
	}
#endif
	if (a->layer_lowervp == NULL) {
		/* Should never happen */
		int i; u_long *p;
		printf("vp = %p, ZERO ptr\n", vp);
		for (p = (u_long *) a, i = 0; i < 8; i++)
			printf(" %lx", p[i]);
		printf("\n");
		/* wait for debugger */
		panic("layer_checkvp");
	}
	if (a->layer_lowervp->v_usecount < 1) {
		int i; u_long *p;
		printf("vp = %p, unref'ed lowervp\n", vp);
		for (p = (u_long *) a, i = 0; i < 8; i++)
			printf(" %lx", p[i]);
		printf("\n");
		/* wait for debugger */
		panic ("layer with unref'ed lowervp");
	}
#ifdef notnow
	printf("layer %p/%d -> %p/%d [%s, %d]\n",
	        LAYERTOV(a), LAYERTOV(a)->v_usecount,
		a->layer_lowervp, a->layer_lowervp->v_usecount,
		fil, lno);
#endif
	return a->layer_lowervp;
}

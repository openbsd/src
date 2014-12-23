/*	$OpenBSD: nfs_node.c,v 1.60 2014/12/23 04:48:47 tedu Exp $	*/
/*	$NetBSD: nfs_node.c,v 1.16 1996/02/18 11:53:42 fvdl Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	@(#)nfs_node.c	8.6 (Berkeley) 5/22/95
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/timeout.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/rwlock.h>
#include <sys/queue.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsnode.h>
#include <nfs/nfsmount.h>
#include <nfs/nfs_var.h>

struct pool nfs_node_pool;
extern int prtactive;

struct rwlock nfs_hashlock = RWLOCK_INITIALIZER("nfshshlk");

/* XXX */
extern struct vops nfs_vops;

/* filehandle to node lookup. */
static __inline int
nfsnode_cmp(const struct nfsnode *a, const struct nfsnode *b)
{
	if (a->n_fhsize != b->n_fhsize)
		return (a->n_fhsize - b->n_fhsize);
	return (memcmp(a->n_fhp, b->n_fhp, a->n_fhsize));
}

RB_PROTOTYPE(nfs_nodetree, nfsnode, n_entry, nfsnode_cmp);
RB_GENERATE(nfs_nodetree, nfsnode, n_entry, nfsnode_cmp);

/*
 * Look up a vnode/nfsnode by file handle.
 * Callers must check for mount points!!
 * In all cases, a pointer to a
 * nfsnode structure is returned.
 */
int
nfs_nget(struct mount *mnt, nfsfh_t *fh, int fhsize, struct nfsnode **npp)
{
	struct nfsmount		*nmp;
	struct nfsnode		*np, find, *np2;
	struct vnode		*vp, *nvp;
	struct proc		*p = curproc;		/* XXX */
	int			 error;

	nmp = VFSTONFS(mnt);

loop:
	rw_enter_write(&nfs_hashlock);
	find.n_fhp = fh;
	find.n_fhsize = fhsize;
	np = RB_FIND(nfs_nodetree, &nmp->nm_ntree, &find);
	if (np != NULL) {
		rw_exit_write(&nfs_hashlock);
		vp = NFSTOV(np);
		error = vget(vp, LK_EXCLUSIVE, p);
		if (error)
			goto loop;
		*npp = np;
		return (0);
	}

	/*
	 * getnewvnode() could recycle a vnode, potentially formerly
	 * owned by NFS. This will cause a VOP_RECLAIM() to happen,
	 * which will cause recursive locking, so we unlock before
	 * calling getnewvnode() lock again afterwards, but must check
	 * to see if this nfsnode has been added while we did not hold
	 * the lock.
	 */
	rw_exit_write(&nfs_hashlock);
	error = getnewvnode(VT_NFS, mnt, &nfs_vops, &nvp);
	if (error) {
		*npp = NULL;
		return (error);
	}
	/* grab one of these too while we're outside the lock */
	np2 = pool_get(&nfs_node_pool, PR_WAITOK | PR_ZERO);

	/* note that we don't have this vnode set up completely yet */
	rw_enter_write(&nfs_hashlock);
	nvp->v_flag |= VLARVAL;
	np = RB_FIND(nfs_nodetree, &nmp->nm_ntree, &find);
	/* lost race. undo and repeat */
	if (np != NULL) {
		pool_put(&nfs_node_pool, np2);
		vgone(nvp);
		rw_exit_write(&nfs_hashlock);
		goto loop;
	}

	vp = nvp;
	np = np2;
	vp->v_data = np;
	/* we now have an nfsnode on this vnode */
	vp->v_flag &= ~VLARVAL;
	np->n_vnode = vp;

	rw_init(&np->n_commitlock, "nfs_commitlk");

	/* 
	 * Are we getting the root? If so, make sure the vnode flags
	 * are correct 
	 */
	if ((fhsize == nmp->nm_fhsize) && !bcmp(fh, nmp->nm_fh, fhsize)) {
		if (vp->v_type == VNON)
			vp->v_type = VDIR;
		vp->v_flag |= VROOT;
	}

	np->n_fhp = &np->n_fh;
	bcopy(fh, np->n_fhp, fhsize);
	np->n_fhsize = fhsize;
	np2 = RB_INSERT(nfs_nodetree, &nmp->nm_ntree, np);
	KASSERT(np2 == NULL);
	np->n_accstamp = -1;
	rw_exit(&nfs_hashlock);
	*npp = np;

	return (0);
}

int
nfs_inactive(void *v)
{
	struct vop_inactive_args	*ap = v;
	struct nfsnode			*np;
	struct sillyrename		*sp;

#ifdef DIAGNOSTIC
	if (prtactive && ap->a_vp->v_usecount != 0)
		vprint("nfs_inactive: pushing active", ap->a_vp);
#endif
	if (ap->a_vp->v_flag & VLARVAL)
		/*
		 * vnode was incompletely set up, just return
		 * as we are throwing it away.
		 */
		return(0);
#ifdef DIAGNOSTIC
	if (ap->a_vp->v_data == NULL)
		panic("NULL v_data (no nfsnode set up?) in vnode %p",
		    ap->a_vp);
#endif
	np = VTONFS(ap->a_vp);
	if (ap->a_vp->v_type != VDIR) {
		sp = np->n_sillyrename;
		np->n_sillyrename = NULL;
	} else
		sp = NULL;
	if (sp) {
		/*
		 * Remove the silly file that was rename'd earlier
		 */
		nfs_vinvalbuf(ap->a_vp, 0, sp->s_cred, curproc);
		nfs_removeit(sp);
		crfree(sp->s_cred);
		vrele(sp->s_dvp);
		free(sp, M_NFSREQ, sizeof(*sp));
	}
	np->n_flag &= (NMODIFIED | NFLUSHINPROG | NFLUSHWANT);

	VOP_UNLOCK(ap->a_vp, 0, ap->a_p);
	return (0);
}

/*
 * Reclaim an nfsnode so that it can be used for other purposes.
 */
int
nfs_reclaim(void *v)
{
	struct vop_reclaim_args	*ap = v;
	struct vnode		*vp = ap->a_vp;
	struct nfsmount		*nmp;
	struct nfsnode		*np = VTONFS(vp);

#ifdef DIAGNOSTIC
	if (prtactive && vp->v_usecount != 0)
		vprint("nfs_reclaim: pushing active", vp);
#endif
	if (ap->a_vp->v_flag & VLARVAL)
		/*
		 * vnode was incompletely set up, just return
		 * as we are throwing it away.
		 */
		return(0);
#ifdef DIAGNOSTIC
	if (ap->a_vp->v_data == NULL)
		panic("NULL v_data (no nfsnode set up?) in vnode %p",
		    ap->a_vp);
#endif
	nmp = VFSTONFS(vp->v_mount);
	rw_enter_write(&nfs_hashlock);
	RB_REMOVE(nfs_nodetree, &nmp->nm_ntree, np);
	rw_exit_write(&nfs_hashlock);

	if (np->n_rcred)
		crfree(np->n_rcred);
	if (np->n_wcred)
		crfree(np->n_wcred);

	cache_purge(vp);
	pool_put(&nfs_node_pool, vp->v_data);
	vp->v_data = NULL;

	return (0);
}

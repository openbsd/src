/*	$OpenBSD: vfs_cache.c,v 1.12 2004/10/04 12:03:45 pedro Exp $	*/
/*	$NetBSD: vfs_cache.c,v 1.13 1996/02/04 02:18:09 christos Exp $	*/

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
 *	@(#)vfs_cache.c	8.3 (Berkeley) 8/22/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/hash.h>

/*
 * Name caching works as follows:
 *
 * Names found by directory scans are retained in a cache
 * for future reference.  It is managed LRU, so frequently
 * used names will hang around.  Cache is indexed by hash value
 * obtained from (vp, name) where vp refers to the directory
 * containing name.
 *
 * For simplicity (and economy of storage), names longer than
 * a maximum length of NCHNAMLEN are not cached; they occur
 * infrequently in any case, and are almost never of interest.
 *
 * Upon reaching the last segment of a path, if the reference
 * is for DELETE, or NOCACHE is set (rewrite), and the
 * name is located in the cache, it will be dropped.
 */

/*
 * Structures associated with name caching.
 */
LIST_HEAD(nchashhead, namecache) *nchashtbl;
u_long	nchash;				/* size of hash table - 1 */
long	numcache;			/* number of cache entries allocated */
TAILQ_HEAD(, namecache) nclruhead;		/* LRU chain */
struct	nchstats nchstats;		/* cache effectiveness statistics */

int doingcache = 1;			/* 1 => enable the cache */

struct pool nch_pool;

u_long nextvnodeid;

/*
 * Look for a the name in the cache. We don't do this
 * if the segment name is long, simply so the cache can avoid
 * holding long names (which would either waste space, or
 * add greatly to the complexity).
 *
 * Lookup is called with ni_dvp pointing to the directory to search,
 * ni_ptr pointing to the name of the entry being sought, ni_namelen
 * tells the length of the name, and ni_hash contains a hash of
 * the name. If the lookup succeeds, the vnode is returned in ni_vp
 * and a status of 0 is returned. If the locking fails for whatever
 * reason, the vnode is unlocked and the error is returned to caller.
 * If the lookup determines that the name does not exist (negative caching),
 * a status of ENOENT is returned. If the lookup fails, a status of -1
 * is returned.
 */
int
cache_lookup(dvp, vpp, cnp)
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
{
	struct namecache *ncp;
	struct nchashhead *ncpp;
	struct vnode *vp;
	struct proc *p = curproc;
	u_long vpid;
	int error;

	*vpp = NULL;

	if (!doingcache) {
		cnp->cn_flags &= ~MAKEENTRY;
		return (-1);
	}
	if (cnp->cn_namelen > NCHNAMLEN) {
		nchstats.ncs_long++;
		cnp->cn_flags &= ~MAKEENTRY;
		return (-1);
	}

	ncpp = &nchashtbl[
	    hash32_buf(&dvp->v_id, sizeof(dvp->v_id), cnp->cn_hash) & nchash];
	LIST_FOREACH(ncp, ncpp, nc_hash) {
		if (ncp->nc_dvp == dvp &&
		    ncp->nc_dvpid == dvp->v_id &&
		    ncp->nc_nlen == cnp->cn_namelen &&
		    !memcmp(ncp->nc_name, cnp->cn_nameptr, (u_int)ncp->nc_nlen))
			break;
	}
	if (ncp == 0) {
		nchstats.ncs_miss++;
		return (-1);
	}
	if ((cnp->cn_flags & MAKEENTRY) == 0) {
		nchstats.ncs_badhits++;
		goto remove;
	} else if (ncp->nc_vp == NULL) {
		/*
		 * Restore the ISWHITEOUT flag saved earlier.
		 */
		cnp->cn_flags |= ncp->nc_vpid;
		if (cnp->cn_nameiop != CREATE ||
		    (cnp->cn_flags & ISLASTCN) == 0) {
			nchstats.ncs_neghits++;
			/*
			 * Move this slot to end of LRU chain,
			 * if not already there.
			 */
			if (TAILQ_NEXT(ncp, nc_lru) != NULL) {
				TAILQ_REMOVE(&nclruhead, ncp, nc_lru);
				TAILQ_INSERT_TAIL(&nclruhead, ncp, nc_lru);
			}
			return (ENOENT);
		} else {
			nchstats.ncs_badhits++;
			goto remove;
		}
	} else if (ncp->nc_vpid != ncp->nc_vp->v_id) {
		nchstats.ncs_falsehits++;
		goto remove;
	}

	vp = ncp->nc_vp;
	vpid = vp->v_id;
	if (vp == dvp) {	/* lookup on "." */
		VREF(dvp);
		error = 0;
	} else if (cnp->cn_flags & ISDOTDOT) {
		VOP_UNLOCK(dvp, 0, p);
		cnp->cn_flags |= PDIRUNLOCK;
		error = vget(vp, LK_EXCLUSIVE, p);
		/*
		 * If the above vget() succeeded and both LOCKPARENT and
		 * ISLASTCN is set, lock the directory vnode as well.
		 */
		if (!error && (~cnp->cn_flags & (LOCKPARENT|ISLASTCN)) == 0) {
			if ((error = vn_lock(dvp, LK_EXCLUSIVE, p)) != 0) {
				vput(vp);
				return (error);
			}
			cnp->cn_flags &= ~PDIRUNLOCK;
		}
	} else {
		error = vget(vp, LK_EXCLUSIVE, p);
		/*
		 * If the above vget() failed or either of LOCKPARENT or
		 * ISLASTCN is set, unlock the directory vnode.
		 */
		if (error || (~cnp->cn_flags & (LOCKPARENT|ISLASTCN)) != 0) {
			VOP_UNLOCK(dvp, 0, p);
			cnp->cn_flags |= PDIRUNLOCK;
		}
	}

	/*
	 * Check that the lock succeeded, and that the capability number did
	 * not change while we were waiting for the lock.
	 */
	if (error || vpid != vp->v_id) {
		if (!error) {
			vput(vp);
			nchstats.ncs_falsehits++;
		} else
			nchstats.ncs_badhits++;
		/*
		 * The parent needs to be locked when we return to VOP_LOOKUP().
		 * The `.' case here should be extremely rare (if it can happen
		 * at all), so we don't bother optimizing out the unlock/relock.
		 */
		if (vp == dvp || error ||
		    (~cnp->cn_flags & (LOCKPARENT|ISLASTCN)) != 0) {
			if ((error = vn_lock(dvp, LK_EXCLUSIVE, p)) != 0)
				return (error);
			cnp->cn_flags &= ~PDIRUNLOCK;
		}
		return (-1);
	}

	nchstats.ncs_goodhits++;
	/*
	 * Move this slot to end of LRU chain, if not already there.
	 */
	if (TAILQ_NEXT(ncp, nc_lru) != NULL) {
		TAILQ_REMOVE(&nclruhead, ncp, nc_lru);
		TAILQ_INSERT_TAIL(&nclruhead, ncp, nc_lru);
	}
	*vpp = vp;
	return (0);

remove:
	/*
	 * Last component and we are renaming or deleting,
	 * the cache entry is invalid, or otherwise don't
	 * want cache entry to exist.
	 */
	TAILQ_REMOVE(&nclruhead, ncp, nc_lru);
	LIST_REMOVE(ncp, nc_hash);
	ncp->nc_hash.le_prev = NULL;
#if 0
	if (ncp->nc_vhash.le_prev != NULL) {
		LIST_REMOVE(ncp, nc_vhash);
		ncp->nc_vhash.le_prev = NULL;
	}
#endif
	TAILQ_INSERT_HEAD(&nclruhead, ncp, nc_lru);
	return (-1);
}

/*
 * Add an entry to the cache
 */
void
cache_enter(dvp, vp, cnp)
	struct vnode *dvp;
	struct vnode *vp;
	struct componentname *cnp;
{
	register struct namecache *ncp;
	register struct nchashhead *ncpp;

#ifdef DIAGNOSTIC
	if (cnp->cn_namelen > NCHNAMLEN)
		panic("cache_enter: name too long");
#endif
	if (!doingcache)
		return;
	/*
	 * Free the cache slot at head of lru chain.
	 */
	if (numcache < desiredvnodes) {
		ncp = pool_get(&nch_pool, PR_WAITOK);
		bzero((char *)ncp, sizeof *ncp);
		numcache++;
	} else if ((ncp = nclruhead.tqh_first) != NULL) {
		TAILQ_REMOVE(&nclruhead, ncp, nc_lru);
		if (ncp->nc_hash.le_prev != 0) {
			LIST_REMOVE(ncp, nc_hash);
			ncp->nc_hash.le_prev = 0;
		}
	} else
		return;
	/* grab the vnode we just found */
	ncp->nc_vp = vp;
	if (vp)
		ncp->nc_vpid = vp->v_id;
	else {
		/*
		 * For negative hits, save the ISWHITEOUT flag so we can
		 * restore it later when the cache entry is used again.
		 */
		ncp->nc_vpid = cnp->cn_flags & ISWHITEOUT;
	}
	/* fill in cache info */
	ncp->nc_dvp = dvp;
	ncp->nc_dvpid = dvp->v_id;
	ncp->nc_nlen = cnp->cn_namelen;
	bcopy(cnp->cn_nameptr, ncp->nc_name, (unsigned)ncp->nc_nlen);
	TAILQ_INSERT_TAIL(&nclruhead, ncp, nc_lru);
	ncpp = &nchashtbl[
	    hash32_buf(&dvp->v_id, sizeof(dvp->v_id), cnp->cn_hash) & nchash];
	LIST_INSERT_HEAD(ncpp, ncp, nc_hash);
}

/*
 * Name cache initialization, from vfs_init() when we are booting
 */
void
nchinit()
{

	TAILQ_INIT(&nclruhead);
	nchashtbl = hashinit(desiredvnodes, M_CACHE, M_WAITOK, &nchash);
	pool_init(&nch_pool, sizeof(struct namecache), 0, 0, 0, "nchpl",
	    &pool_allocator_nointr);
}

/*
 * Cache flush, a particular vnode; called when a vnode is renamed to
 * hide entries that would now be invalid
 */
void
cache_purge(vp)
	struct vnode *vp;
{
	struct namecache *ncp;
	struct nchashhead *ncpp;

	vp->v_id = ++nextvnodeid;
	if (nextvnodeid != 0)
		return;
	for (ncpp = &nchashtbl[nchash]; ncpp >= nchashtbl; ncpp--) {
		for (ncp = ncpp->lh_first; ncp != 0; ncp = ncp->nc_hash.le_next) {
			ncp->nc_vpid = 0;
			ncp->nc_dvpid = 0;
		}
	}
	vp->v_id = ++nextvnodeid;
}

/*
 * Cache flush, a whole filesystem; called when filesys is umounted to
 * remove entries that would now be invalid
 *
 * The line "nxtcp = nchhead" near the end is to avoid potential problems
 * if the cache lru chain is modified while we are dumping the
 * inode.  This makes the algorithm O(n^2), but do you think I care?
 */
void
cache_purgevfs(mp)
	struct mount *mp;
{
	register struct namecache *ncp, *nxtcp;

	for (ncp = nclruhead.tqh_first; ncp != 0; ncp = nxtcp) {
		if (ncp->nc_dvp == NULL || ncp->nc_dvp->v_mount != mp) {
			nxtcp = ncp->nc_lru.tqe_next;
			continue;
		}
		/* free the resources we had */
		ncp->nc_vp = NULL;
		ncp->nc_dvp = NULL;
		TAILQ_REMOVE(&nclruhead, ncp, nc_lru);
		if (ncp->nc_hash.le_prev != 0) {
			LIST_REMOVE(ncp, nc_hash);
			ncp->nc_hash.le_prev = 0;
		}
		/* cause rescan of list, it may have altered */
		nxtcp = nclruhead.tqh_first;
		TAILQ_INSERT_HEAD(&nclruhead, ncp, nc_lru);
	}
}

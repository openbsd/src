/*	$NetBSD: adlookup.c,v 1.12 1996/02/13 17:05:47 christos Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christian E. Hopps.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <adosfs/adosfs.h>

#ifdef ADOSFS_EXACTMATCH
#define strmatch(s1, l1, s2, l2) \
    ((l1) == (l2) && bcmp((s1), (s2), (l1)) == 0)
#else
#define strmatch(s1, l1, s2, l2) \
    ((l1) == (l2) && strncasecmp((s1), (s2), (l1)) == 0)
#endif

/*
 * adosfs lookup. enters with:
 * pvp (parent vnode) referenced and locked.
 * exit with:
 *	target vp referenced and locked.
 *	unlock pvp unless LOCKPARENT and at end of search.
 * special cases:
 *	pvp == vp, just ref pvp, pvp already holds a ref and lock from
 *	    caller, this will not occur with RENAME or CREATE.
 *	LOOKUP always unlocks parent if last element. (not now!?!?)
 */
int
adosfs_lookup(v)
	void *v;
{
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *sp = v;
	int nameiop, last, lockp, wantp, flags, error, vpid, nocache, i;
	struct componentname *cnp;
	struct vnode **vpp;	/* place to store result */
	struct anode *ap;	/* anode to find */
	struct vnode *vdp;	/* vnode of search dir */
	struct anode *adp;	/* anode of search dir */
	struct ucred *ucp;	/* lookup credentials */
	u_long bn, plen, hval;
	char *pelt;

#ifdef ADOSFS_DIAGNOSTIC
	advopprint(sp);
#endif
	cnp = sp->a_cnp;
	vdp = sp->a_dvp;
	adp = VTOA(vdp);
	vpp = sp->a_vpp;
	*vpp = NULL;
	ucp = cnp->cn_cred;
	nameiop = cnp->cn_nameiop;
	flags = cnp->cn_flags;
	last = flags & ISLASTCN;
	lockp = flags & LOCKPARENT;
	wantp = flags & (LOCKPARENT | WANTPARENT);
	pelt = cnp->cn_nameptr;
	plen = cnp->cn_namelen;
	nocache = 0;
	
	/* 
	 * check that:
	 * pvp is a dir, and that the user has rights to access 
	 */
	if (vdp->v_type != VDIR)
		return (ENOTDIR);
	if ((error = VOP_ACCESS(vdp, VEXEC, ucp, cnp->cn_proc)) != 0)
		return (error);
	/*
	 * cache lookup algorithm borrowed from ufs_lookup()
	 * its not consistent with otherthings in this function..
	 */
	if ((error = cache_lookup(vdp, vpp, cnp)) != 0) {
		if (error == ENOENT)
			return (error);

		vpid = (*vpp)->v_id;
		if (vdp == *vpp) {
			VREF(vdp);
			error = 0;
		} else if (flags & ISDOTDOT) {
			VOP_UNLOCK(vdp);	/* race */
			error = vget(*vpp, 1);
			if (error == 0 && lockp && last)
				error = VOP_LOCK(vdp);
		} else {
			error = vget(*vpp, 1);
			/* if (lockp == 0 || error || last) */
			if (lockp == 0 || error || last == 0)
				VOP_UNLOCK(vdp);
		}
		if (error == 0) {
			if (vpid == vdp->v_id)
				return (0);
			vput(*vpp);
			if (lockp && vdp != *vpp && last)
				VOP_UNLOCK(vdp);
		}
		*vpp = NULL;
		if ((error = VOP_LOCK(vdp)) != 0)
			return (error);
	}

	/*
	 * fake a '.'
	 */
	if (plen == 1 && pelt[0] == '.') {
		/* see special cases in prologue. */
		*vpp = vdp;
		goto found;
	}
	/*
	 * fake a ".."
	 */
	if (flags & ISDOTDOT) {
		if (vdp->v_type == VDIR && (vdp->v_flag & VROOT)) 
			panic("adosfs .. attemped through root");
		/*
		 * cannot get `..' while `vdp' is locked
		 * e.g. procA holds lock on `..' and waits for `vdp'
		 * we wait for `..' and hold lock on `vdp'. deadlock.
		 * becuase `vdp' may have been acheived through symlink
		 * fancy detection code that decreases the race
		 * window size is not reasonably possible.
		 *
		 * basically unlock the parent, try and lock the child (..)
		 * if that fails relock the parent (ignoring error) and 
		 * fail.  Otherwise we have the child (..) if this is the
		 * last and the caller requested LOCKPARENT, attempt to
		 * relock the parent.  If that fails unlock the child (..)
		 * and fail. Otherwise we have succeded.
		 * 
		 */
		VOP_UNLOCK(vdp); /* race */
		if ((error = VFS_VGET(vdp->v_mount, 
				      (ino_t)adp->pblock, vpp)) != 0)
			VOP_LOCK(vdp);
		else if (last && lockp && (error = VOP_LOCK(vdp)))
			vput(*vpp);
		if (error) {
			*vpp = NULL;
			return (error);
		}
		goto found_lockdone;
	}

	/*
	 * hash the name and grab the first block in chain
	 * then walk the chain. if chain has not been fully
	 * walked before, track the count in `tabi'
	 */
	hval = adoshash(pelt, plen, adp->ntabent);
	bn = adp->tab[hval];
	i = min(adp->tabi[hval], 0);
	while (bn != 0) {
		if ((error = VFS_VGET(vdp->v_mount, (ino_t)bn, vpp)) != 0) {
#ifdef ADOSFS_DIAGNOSTIC
			printf("[aget] %d)", error);
#endif
			/* XXX check to unlock parent possibly? */
			return(error);
		}
		ap = VTOA(*vpp);
		if (i <= 0) {
			if (--i < adp->tabi[hval])
				adp->tabi[hval] = i;	
			/*
			 * last header in chain lock count down by
			 * negating it to positive
			 */
			if (ap->hashf == 0) {
#ifdef DEBUG
				if (i != adp->tabi[hval])
					panic("adlookup: wrong chain count");
#endif
				adp->tabi[hval] = -adp->tabi[hval];
			}
		}
		if (strmatch(pelt, plen, ap->name, strlen(ap->name)))
			goto found;
		bn = ap->hashf;
		vput(*vpp);
	}
	*vpp = NULL;
	/*
	 * not found
	 */
	if ((nameiop == CREATE || nameiop == RENAME) && last) {
		if ((error = VOP_ACCESS(vdp, VWRITE, ucp, cnp->cn_proc)) != 0) {
#ifdef ADOSFS_DIAGNOSTIC
			printf("[VOP_ACCESS] %d)", error);
#endif
			return (error);
		}
		if (lockp == 0)
			VOP_UNLOCK(vdp);
		cnp->cn_nameiop |= SAVENAME;
#ifdef ADOSFS_DIAGNOSTIC
		printf("EJUSTRETURN)");
#endif
		return(EJUSTRETURN);
	}
	if ((cnp->cn_flags & MAKEENTRY) && nameiop != CREATE)
		cache_enter(vdp, NULL, cnp);
#ifdef ADOSFS_DIAGNOSTIC
	printf("ENOENT)");
#endif
	return(ENOENT);

found:
	if (nameiop == DELETE && last)  {
		if ((error = VOP_ACCESS(vdp, VWRITE, ucp, cnp->cn_proc)) != 0) {
			if (vdp != *vpp)
				vput(*vpp);
			*vpp = NULL;
			return (error);
		}
		nocache = 1;
	} 
	if (nameiop == RENAME && wantp && last) {
		if (vdp == *vpp)
			return(EISDIR);
		if ((error = VOP_ACCESS(vdp, VWRITE, ucp, cnp->cn_proc)) != 0) {
			vput(*vpp);
			*vpp = NULL;
			return (error);
		}
		cnp->cn_flags |= SAVENAME;
		nocache = 1;
	}
	if (vdp == *vpp)
		VREF(vdp);
	else if (lockp == 0 || last == 0)
		VOP_UNLOCK(vdp);
found_lockdone:
	if ((cnp->cn_flags & MAKEENTRY) && nocache == 0)
		cache_enter(vdp, *vpp, cnp);

#ifdef ADOSFS_DIAGNOSTIC
	printf("0)\n");
#endif
	return(0);
}

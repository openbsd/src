/*       $OpenBSD: vfs_default.c,v 1.10 2001/11/29 02:08:21 art Exp $  */

/*
 *    Portions of this code are:
 *
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/event.h>
#include <miscfs/specfs/specdev.h>

#include <uvm/uvm.h>

extern struct simplelock spechash_slock;

int filt_generic_readwrite __P((struct knote *kn, long hint));
void filt_generic_detach __P((struct knote *kn));

/*
 * Eliminate all activity associated with  the requested vnode
 * and with all vnodes aliased to the requested vnode.
 */
int
vop_generic_revoke(v)
	void *v;
{
	struct vop_revoke_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap = v;
	struct vnode *vp, *vq;
	struct proc *p = curproc;

#ifdef DIAGNOSTIC
	if ((ap->a_flags & REVOKEALL) == 0)
		panic("vop_generic_revoke");
#endif

	vp = ap->a_vp;
	simple_lock(&vp->v_interlock);
 
	if (vp->v_flag & VALIASED) {
		/*
		 * If a vgone (or vclean) is already in progress,
		 * wait until it is done and return.
		 */
		if (vp->v_flag & VXLOCK) {
			vp->v_flag |= VXWANT;
			simple_unlock(&vp->v_interlock);
			tsleep((caddr_t)vp, PINOD, "vop_generic_revokeall", 0);
			return(0);
		}
		/*
		 * Ensure that vp will not be vgone'd while we
		 * are eliminating its aliases.
		 */
		vp->v_flag |= VXLOCK;
		simple_unlock(&vp->v_interlock);
		while (vp->v_flag & VALIASED) {
			simple_lock(&spechash_slock);
			for (vq = *vp->v_hashchain; vq; vq = vq->v_specnext) {
				if (vq->v_rdev != vp->v_rdev ||
				    vq->v_type != vp->v_type || vp == vq)
					continue;
				simple_unlock(&spechash_slock);
				vgone(vq);
				break;
			}
			simple_unlock(&spechash_slock);
		}
		/*
		 * Remove the lock so that vgone below will
		 * really eliminate the vnode after which time
		 * vgone will awaken any sleepers.
		 */
		simple_lock(&vp->v_interlock);
		vp->v_flag &= ~VXLOCK;
	}
	vgonel(vp, p);
	return (0);
}


int
vop_generic_bwrite(v)
	void *v;
{
	struct vop_bwrite_args *ap = v;

	return (bwrite(ap->a_bp));
}


int
vop_generic_abortop(v)
	void *v;
{
	struct vop_abortop_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
	} */ *ap = v;
 
	if ((ap->a_cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF)
		FREE(ap->a_cnp->cn_pnbuf, M_NAMEI);
	return (0);
}

/*
 * Stubs to use when there is no locking to be done on the underlying object.
 * A minimal shared lock is necessary to ensure that the underlying object
 * is not revoked while an operation is in progress. So, an active shared
 * count is maintained in an auxillary vnode lock structure.
 */
int
vop_generic_lock(v)
	void *v;
{
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap = v;

#ifdef notyet
	/*
	 * This code cannot be used until all the non-locking filesystems
	 * (notably NFS) are converted to properly lock and release nodes.
	 * Also, certain vnode operations change the locking state within
	 * the operation (create, mknod, remove, link, rename, mkdir, rmdir,
	 * and symlink). Ideally these operations should not change the
	 * lock state, but should be changed to let the caller of the
	 * function unlock them. Otherwise all intermediate vnode layers
	 * (such as union, umapfs, etc) must catch these functions to do
	 * the necessary locking at their layer. Note that the inactive
	 * and lookup operations also change their lock state, but this 
	 * cannot be avoided, so these two operations will always need
	 * to be handled in intermediate layers.
	 */
	struct vnode *vp = ap->a_vp;
	int vnflags, flags = ap->a_flags;

	if (vp->v_vnlock == NULL) {
		if ((flags & LK_TYPE_MASK) == LK_DRAIN)
			return (0);
		MALLOC(vp->v_vnlock, struct lock *, sizeof(struct lock),
		    M_VNODE, M_WAITOK);
		lockinit(vp->v_vnlock, PVFS, "vnlock", 0, 0);
	}
	switch (flags & LK_TYPE_MASK) {
	case LK_DRAIN:
		vnflags = LK_DRAIN;
		break;
	case LK_EXCLUSIVE:
	case LK_SHARED:
		vnflags = LK_SHARED;
		break;
	case LK_UPGRADE:
	case LK_EXCLUPGRADE:
	case LK_DOWNGRADE:
		return (0);
	case LK_RELEASE:
	default:
		panic("vop_generic_lock: bad operation %d", flags & LK_TYPE_MASK);
	}
	if (flags & LK_INTERLOCK)
		vnflags |= LK_INTERLOCK;
	return(lockmgr(vp->v_vnlock, vnflags, &vp->v_interlock, ap->a_p));
#else /* for now */
	/*
	 * Since we are not using the lock manager, we must clear
	 * the interlock here.
	 */
	if (ap->a_flags & LK_INTERLOCK)
		simple_unlock(&ap->a_vp->v_interlock);
	return (0);
#endif
}
 
/*
 * Decrement the active use count.
 */

int
vop_generic_unlock(v)
	void *v;
{
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap = v;

	struct vnode *vp = ap->a_vp;

	if (vp->v_vnlock == NULL)
		return (0);
	return (lockmgr(vp->v_vnlock, LK_RELEASE, NULL, ap->a_p));
}

/*
 * Return whether or not the node is in use.
 */
int
vop_generic_islocked(v)
	void *v;
{
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap = v;

	struct vnode *vp = ap->a_vp;

	if (vp->v_vnlock == NULL)
		return (0);
	return (lockstatus(vp->v_vnlock));
}

struct filterops generic_filtops = 
	{ 1, NULL, filt_generic_detach, filt_generic_readwrite };

int
vop_generic_kqfilter(v)
	void *v;
{
	struct vop_kqfilter_args /* {
		struct vnode *a_vp;
		struct knote *a_kn;
	} */ *ap = v;
	struct knote *kn = ap->a_kn;

	switch (kn->kn_filter) {
	case EVFILT_READ:
	case EVFILT_WRITE:
		kn->kn_fop = &generic_filtops;
		break;
	default:
		return (1);
	}

	return (0);
}

void
filt_generic_detach(struct knote *kn)
{
}

int
filt_generic_readwrite(struct knote *kn, long hint)
{
	/*
	 * filesystem is gone, so set the EOF flag and schedule 
	 * the knote for deletion.
	 */
	if (hint == NOTE_REVOKE) {
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		return (1);
	}

        kn->kn_data = 0;
        return (1);
}

int lease_check(void *);

int
lease_check(void *v)
{
	return (0);
}

/*
 * generic VM getpages routine.
 * Return PG_BUSY pages for the given range,
 * reading from backing store if necessary.
 */

int
genfs_getpages(v)
	void *v;
{
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		voff_t a_offset;
		vm_page_t *a_m;
		int *a_count;
		int a_centeridx;
		vm_prot_t a_access_type;
		int a_advice;
		int a_flags;
	} */ *ap = v;

	off_t newsize, diskeof, memeof;
	off_t offset, origoffset, startoffset, endoffset, raoffset;
	daddr_t lbn, blkno;
	int s, i, error, npages, orignpages, npgs, run, ridx, pidx, pcount;
	int fs_bshift, fs_bsize, dev_bshift, dev_bsize;
	int flags = ap->a_flags;
	size_t bytes, iobytes, tailbytes, totalbytes, skipbytes;
	vaddr_t kva;
	struct buf *bp, *mbp;
	struct vnode *vp = ap->a_vp;
	struct uvm_object *uobj = &vp->v_uvm.u_obj;
	struct vm_page *pgs[16];			/* XXXUBC 16 */
	struct ucred *cred = curproc->p_ucred;		/* XXXUBC curproc */
	boolean_t async = (flags & PGO_SYNCIO) == 0;
	boolean_t write = (ap->a_access_type & VM_PROT_WRITE) != 0;
	boolean_t sawhole = FALSE;
	struct proc *p = curproc;
	UVMHIST_FUNC("genfs_getpages"); UVMHIST_CALLED(ubchist);

	UVMHIST_LOG(ubchist, "vp %p off 0x%x/%x count %d",
		    vp, ap->a_offset >> 32, ap->a_offset, *ap->a_count);

	/* XXXUBC temp limit */
	if (*ap->a_count > 16) {
		return EINVAL;
	}

	error = 0;
	origoffset = ap->a_offset;
	orignpages = *ap->a_count;
	error = VOP_SIZE(vp, vp->v_uvm.u_size, &diskeof);
	if (error) {
		return error;
	}
	if (flags & PGO_PASTEOF) {
		newsize = MAX(vp->v_uvm.u_size,
			      origoffset + (orignpages << PAGE_SHIFT));
		error = VOP_SIZE(vp, newsize, &memeof);
		if (error) {
			return error;
		}
	} else {
		memeof = diskeof;
	}
	KASSERT(ap->a_centeridx >= 0 || ap->a_centeridx <= orignpages);
	KASSERT((origoffset & (PAGE_SIZE - 1)) == 0 && origoffset >= 0);
	KASSERT(orignpages > 0);

	/*
	 * Bounds-check the request.
	 */

	if (origoffset + (ap->a_centeridx << PAGE_SHIFT) >= memeof) {
		if ((flags & PGO_LOCKED) == 0) {
			simple_unlock(&uobj->vmobjlock);
		}
		UVMHIST_LOG(ubchist, "off 0x%x count %d goes past EOF 0x%x",
			    origoffset, *ap->a_count, memeof,0);
		return EINVAL;
	}

	/*
	 * For PGO_LOCKED requests, just return whatever's in memory.
	 */

	if (flags & PGO_LOCKED) {
		uvn_findpages(uobj, origoffset, ap->a_count, ap->a_m,
			      UFP_NOWAIT|UFP_NOALLOC|UFP_NORDONLY);

		return ap->a_m[ap->a_centeridx] == NULL ? EBUSY : 0;
	}

	/* vnode is VOP_LOCKed, uobj is locked */

	if (write && (vp->v_bioflag & VBIOONSYNCLIST) == 0) {
		vn_syncer_add_to_worklist(vp, syncdelay);
	}

	/*
	 * find the requested pages and make some simple checks.
	 * leave space in the page array for a whole block.
	 */

	fs_bshift = vp->v_mount->mnt_fs_bshift;
	fs_bsize = 1 << fs_bshift;
	dev_bshift = vp->v_mount->mnt_dev_bshift;
	dev_bsize = 1 << dev_bshift;
	KASSERT((diskeof & (dev_bsize - 1)) == 0);
	KASSERT((memeof & (dev_bsize - 1)) == 0);

	orignpages = MIN(orignpages,
	    round_page(memeof - origoffset) >> PAGE_SHIFT);
	npages = orignpages;
	startoffset = origoffset & ~(fs_bsize - 1);
	endoffset = round_page((origoffset + (npages << PAGE_SHIFT)
				+ fs_bsize - 1) & ~(fs_bsize - 1));
	endoffset = MIN(endoffset, round_page(memeof));
	ridx = (origoffset - startoffset) >> PAGE_SHIFT;

	memset(pgs, 0, sizeof(pgs));
	uvn_findpages(uobj, origoffset, &npages, &pgs[ridx], UFP_ALL);

	/*
	 * if PGO_OVERWRITE is set, don't bother reading the pages.
	 * PGO_OVERWRITE also means that the caller guarantees
	 * that the pages already have backing store allocated.
	 */

	if (flags & PGO_OVERWRITE) {
		UVMHIST_LOG(ubchist, "PGO_OVERWRITE",0,0,0,0);

		for (i = 0; i < npages; i++) {
			struct vm_page *pg = pgs[ridx + i];

			if (pg->flags & PG_FAKE) {
				uvm_pagezero(pg);
				pg->flags &= ~(PG_FAKE);
			}
			pg->flags &= ~(PG_RDONLY);
		}
		npages += ridx;
		goto out;
	}

	/*
	 * if the pages are already resident, just return them.
	 */

	for (i = 0; i < npages; i++) {
		struct vm_page *pg = pgs[ridx + i];

		if ((pg->flags & PG_FAKE) ||
		    (write && (pg->flags & PG_RDONLY))) {
			break;
		}
	}
	if (i == npages) {
		UVMHIST_LOG(ubchist, "returning cached pages", 0,0,0,0);
		raoffset = origoffset + (orignpages << PAGE_SHIFT);
		npages += ridx;
		goto raout;
	}

	/*
	 * the page wasn't resident and we're not overwriting,
	 * so we're going to have to do some i/o.
	 * find any additional pages needed to cover the expanded range.
	 */

	if (startoffset != origoffset) {

		/*
		 * XXXUBC we need to avoid deadlocks caused by locking
		 * additional pages at lower offsets than pages we
		 * already have locked.  for now, unlock them all and
		 * start over.
		 */

		for (i = 0; i < npages; i++) {
			struct vm_page *pg = pgs[ridx + i];

			if (pg->flags & PG_FAKE) {
				pg->flags |= PG_RELEASED;
			}
		}
		uvm_page_unbusy(&pgs[ridx], npages);
		memset(pgs, 0, sizeof(pgs));

		UVMHIST_LOG(ubchist, "reset npages start 0x%x end 0x%x",
			    startoffset, endoffset, 0,0);
		npages = (endoffset - startoffset) >> PAGE_SHIFT;
		npgs = npages;
		uvn_findpages(uobj, startoffset, &npgs, pgs, UFP_ALL);
	}
	simple_unlock(&uobj->vmobjlock);

	/*
	 * read the desired page(s).
	 */

	totalbytes = npages << PAGE_SHIFT;
	bytes = MIN(totalbytes, MAX(diskeof - startoffset, 0));
	tailbytes = totalbytes - bytes;
	skipbytes = 0;

	kva = uvm_pagermapin(pgs, npages, UVMPAGER_MAPIN_WAITOK |
			     UVMPAGER_MAPIN_READ);

	s = splbio();
	mbp = pool_get(&bufpool, PR_WAITOK);
	splx(s);
	mbp->b_bufsize = totalbytes;
	mbp->b_data = (void *)kva;
	mbp->b_resid = mbp->b_bcount = bytes;
	mbp->b_flags = B_BUSY|B_READ| (async ? B_CALL : 0);
	mbp->b_iodone = uvm_aio_biodone;
	mbp->b_vp = NULL;
	LIST_INIT(&mbp->b_dep);
	bgetvp(vp, mbp);

	/*
	 * if EOF is in the middle of the range, zero the part past EOF.
	 */

	if (tailbytes > 0) {
		memset((void *)(kva + bytes), 0, tailbytes);
	}

	/*
	 * now loop over the pages, reading as needed.
	 */

	if (write) {
		lockmgr(&vp->v_glock, LK_EXCLUSIVE, NULL, p);
	} else {
		lockmgr(&vp->v_glock, LK_SHARED, NULL, p);
	}

	bp = NULL;
	for (offset = startoffset;
	     bytes > 0;
	     offset += iobytes, bytes -= iobytes) {

		/*
		 * skip pages which don't need to be read.
		 */

		pidx = (offset - startoffset) >> PAGE_SHIFT;
		while ((pgs[pidx]->flags & PG_FAKE) == 0) {
			size_t b;

			KASSERT((offset & (PAGE_SIZE - 1)) == 0);
			b = MIN(PAGE_SIZE, bytes);
			offset += b;
			bytes -= b;
			skipbytes += b;
			pidx++;
			UVMHIST_LOG(ubchist, "skipping, new offset 0x%x",
				    offset, 0,0,0);
			if (bytes == 0) {
				goto loopdone;
			}
		}

		/*
		 * bmap the file to find out the blkno to read from and
		 * how much we can read in one i/o.  if bmap returns an error,
		 * skip the rest of the top-level i/o.
		 */

		lbn = offset >> fs_bshift;
		error = VOP_BMAP(vp, lbn, NULL, &blkno, &run);
		if (error) {
			UVMHIST_LOG(ubchist, "VOP_BMAP lbn 0x%x -> %d\n",
				    lbn, error,0,0);
			skipbytes += bytes;
			goto loopdone;
		}

		/*
		 * see how many pages can be read with this i/o.
		 * reduce the i/o size if necessary to avoid
		 * overwriting pages with valid data.
		 */

		iobytes = MIN((((off_t)lbn + 1 + run) << fs_bshift) - offset,
		    bytes);
		if (offset + iobytes > round_page(offset)) {
			pcount = 1;
			while (pidx + pcount < npages &&
			       pgs[pidx + pcount]->flags & PG_FAKE) {
				pcount++;
			}
			iobytes = MIN(iobytes, (pcount << PAGE_SHIFT) -
				      (offset - trunc_page(offset)));
		}

		/*
		 * if this block isn't allocated, zero it instead of reading it.
		 * if this is a read access, mark the pages we zeroed PG_RDONLY.
		 */

		if (blkno < 0) {
			UVMHIST_LOG(ubchist, "lbn 0x%x -> HOLE", lbn,0,0,0);

			sawhole = TRUE;
			memset((char *)kva + (offset - startoffset), 0,
			       iobytes);
			skipbytes += iobytes;

			if (!write) {
				int holepages =
					(round_page(offset + iobytes) - 
					 trunc_page(offset)) >> PAGE_SHIFT;
				for (i = 0; i < holepages; i++) {
					pgs[pidx + i]->flags |= PG_RDONLY;
				}
			}
			continue;
		}

		/*
		 * allocate a sub-buf for this piece of the i/o
		 * (or just use mbp if there's only 1 piece),
		 * and start it going.
 		 */

		if (offset == startoffset && iobytes == bytes) {
			bp = mbp;
		} else {
			s = splbio();
			bp = pool_get(&bufpool, PR_WAITOK);
			splx(s);
			bp->b_data = (char *)kva + offset - startoffset;
			bp->b_resid = bp->b_bcount = iobytes;
			bp->b_flags = B_BUSY|B_READ|B_CALL;
			bp->b_iodone = uvm_aio_biodone1;
			bp->b_vp = vp;
			LIST_INIT(&bp->b_dep);
		}
		bp->b_lblkno = 0;
		bp->b_private = mbp;

		/* adjust physical blkno for partial blocks */
		bp->b_blkno = blkno + ((offset - ((off_t)lbn << fs_bshift)) >>
				       dev_bshift);

		UVMHIST_LOG(ubchist, "bp %p offset 0x%x bcount 0x%x blkno 0x%x",
			    bp, offset, iobytes, bp->b_blkno);

		VOP_STRATEGY(bp);
	}

loopdone:
	if (skipbytes) {
		s = splbio();
		if (error) {
			mbp->b_flags |= B_ERROR;
			mbp->b_error = error;
		}
		mbp->b_resid -= skipbytes;
		if (mbp->b_resid == 0) {
			biodone(mbp);
		}
		splx(s);
	}

	if (async) {
		UVMHIST_LOG(ubchist, "returning PEND",0,0,0,0);
		lockmgr(&vp->v_glock, LK_RELEASE, NULL, p);
		return EINPROGRESS;
	}
	if (bp != NULL) {
		error = biowait(mbp);
	}
	s = splbio();
	if (mbp->b_vp != NULL) {
		brelvp(mbp);
	}
	pool_put(&bufpool, mbp);
	splx(s);
	uvm_pagermapout(kva, npages);
	raoffset = startoffset + totalbytes;

	/*
	 * if this we encountered a hole then we have to do a little more work.
	 * for read faults, we marked the page PG_RDONLY so that future
	 * write accesses to the page will fault again.
	 * for write faults, we must make sure that the backing store for
	 * the page is completely allocated while the pages are locked.
	 */

	if (error == 0 && sawhole && write) {
		error = VOP_BALLOCN(vp, startoffset, npages << PAGE_SHIFT,
				   cred, 0);
		if (error) {
			UVMHIST_LOG(ubchist, "balloc lbn 0x%x -> %d",
				    lbn, error,0,0);
			lockmgr(&vp->v_glock, LK_RELEASE, NULL, p);
			simple_lock(&uobj->vmobjlock);
			goto out;
		}
	}
	lockmgr(&vp->v_glock, LK_RELEASE, NULL, p);
	simple_lock(&uobj->vmobjlock);

	/*
	 * see if we want to start any readahead.
	 * XXXUBC for now, just read the next 128k on 64k boundaries.
	 * this is pretty nonsensical, but it is 50% faster than reading
	 * just the next 64k.
	 */

raout:
	if (!error && !async && !write && ((int)raoffset & 0xffff) == 0 &&
	    PAGE_SHIFT <= 16) {
		int racount;

		racount = 1 << (16 - PAGE_SHIFT);
		(void) VOP_GETPAGES(vp, raoffset, NULL, &racount, 0,
				    VM_PROT_READ, 0, 0);
		simple_lock(&uobj->vmobjlock);

		racount = 1 << (16 - PAGE_SHIFT);
		(void) VOP_GETPAGES(vp, raoffset + 0x10000, NULL, &racount, 0,
				    VM_PROT_READ, 0, 0);
		simple_lock(&uobj->vmobjlock);
	}

	/*
	 * we're almost done!  release the pages...
	 * for errors, we free the pages.
	 * otherwise we activate them and mark them as valid and clean.
	 * also, unbusy pages that were not actually requested.
	 */

out:
	if (error) {
		uvm_lock_pageq();
		for (i = 0; i < npages; i++) {
			if (pgs[i] == NULL) {
				continue;
			}
			UVMHIST_LOG(ubchist, "examining pg %p flags 0x%x",
				    pgs[i], pgs[i]->flags, 0,0);
			if (pgs[i]->flags & PG_WANTED) {
				wakeup(pgs[i]);
			}
			if (pgs[i]->flags & PG_RELEASED) {
				uvm_unlock_pageq();
				(uobj->pgops->pgo_releasepg)(pgs[i], NULL);
				uvm_lock_pageq();
				continue;
			}
			if (pgs[i]->flags & PG_FAKE) {
				uvm_pagefree(pgs[i]);
				continue;
			}
			uvm_pageactivate(pgs[i]);
			pgs[i]->flags &= ~(PG_WANTED|PG_BUSY);
			UVM_PAGE_OWN(pgs[i], NULL);
		}
		uvm_unlock_pageq();
		simple_unlock(&uobj->vmobjlock);
		UVMHIST_LOG(ubchist, "returning error %d", error,0,0,0);
		return error;
	}

	UVMHIST_LOG(ubchist, "succeeding, npages %d", npages,0,0,0);
	uvm_lock_pageq();
	for (i = 0; i < npages; i++) {
		if (pgs[i] == NULL) {
			continue;
		}
		UVMHIST_LOG(ubchist, "examining pg %p flags 0x%x",
			    pgs[i], pgs[i]->flags, 0,0);
		if (pgs[i]->flags & PG_FAKE) {
			UVMHIST_LOG(ubchist, "unfaking pg %p offset 0x%x",
				    pgs[i], pgs[i]->offset,0,0);
			pgs[i]->flags &= ~(PG_FAKE);
			pmap_clear_modify(pgs[i]);
			pmap_clear_reference(pgs[i]);
		}
		if (write) {
			pgs[i]->flags &= ~(PG_RDONLY);
		}
		if (i < ridx || i >= ridx + orignpages || async) {
			UVMHIST_LOG(ubchist, "unbusy pg %p offset 0x%x",
				    pgs[i], pgs[i]->offset,0,0);
			if (pgs[i]->flags & PG_WANTED) {
				wakeup(pgs[i]);
			}
			if (pgs[i]->flags & PG_RELEASED) {
				uvm_unlock_pageq();
				(uobj->pgops->pgo_releasepg)(pgs[i], NULL);
				uvm_lock_pageq();
				continue;
			}
			uvm_pageactivate(pgs[i]);
			pgs[i]->flags &= ~(PG_WANTED|PG_BUSY);
			UVM_PAGE_OWN(pgs[i], NULL);
		}
	}
	uvm_unlock_pageq();
	simple_unlock(&uobj->vmobjlock);
	if (ap->a_m != NULL) {
		memcpy(ap->a_m, &pgs[ridx],
		       orignpages * sizeof(struct vm_page *));
	}
	return 0;
}

/*
 * generic VM putpages routine.
 * Write the given range of pages to backing store.
 */

int
genfs_putpages(v)
	void *v;
{
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		struct vm_page **a_m;
		int a_count;
		int a_flags;
		int *a_rtvals;
	} */ *ap = v;

	int s, error, npages, run;
	int fs_bshift, dev_bshift, dev_bsize;
	vaddr_t kva;
	off_t eof, offset, startoffset;
	size_t bytes, iobytes, skipbytes;
	daddr_t lbn, blkno;
	struct vm_page *pg;
	struct buf *mbp, *bp;
	struct vnode *vp = ap->a_vp;
	boolean_t async = (ap->a_flags & PGO_SYNCIO) == 0;
	UVMHIST_FUNC("genfs_putpages"); UVMHIST_CALLED(ubchist);
	UVMHIST_LOG(ubchist, "vp %p offset 0x%x count %d",
		    vp, ap->a_m[0]->offset, ap->a_count, 0);

	simple_unlock(&vp->v_uvm.u_obj.vmobjlock);

	error = VOP_SIZE(vp, vp->v_uvm.u_size, &eof);
	if (error) {
		return error;
	}

	error = 0;
	npages = ap->a_count;
	fs_bshift = vp->v_mount->mnt_fs_bshift;
	dev_bshift = vp->v_mount->mnt_dev_bshift;
	dev_bsize = 1 << dev_bshift;
	KASSERT((eof & (dev_bsize - 1)) == 0);

	pg = ap->a_m[0];
	startoffset = pg->offset;
	bytes = MIN(npages << PAGE_SHIFT, eof - startoffset);
	skipbytes = 0;
	KASSERT(bytes != 0);

	kva = uvm_pagermapin(ap->a_m, npages, UVMPAGER_MAPIN_WAITOK);

	s = splbio();
	vp->v_numoutput += 2;
	mbp = pool_get(&bufpool, PR_WAITOK);
	UVMHIST_LOG(ubchist, "vp %p mbp %p num now %d bytes 0x%x",
		    vp, mbp, vp->v_numoutput, bytes);
	splx(s);
	mbp->b_bufsize = npages << PAGE_SHIFT;
	mbp->b_data = (void *)kva;
	mbp->b_resid = mbp->b_bcount = bytes;
	mbp->b_flags = B_BUSY|B_WRITE|B_AGE |
		(async ? B_CALL : 0) |
		(curproc == uvm.pagedaemon_proc ? B_PDAEMON : 0);
	mbp->b_iodone = uvm_aio_biodone;
	mbp->b_vp = NULL;
	LIST_INIT(&mbp->b_dep);
	bgetvp(vp, mbp);

	bp = NULL;
	for (offset = startoffset;
	     bytes > 0;
	     offset += iobytes, bytes -= iobytes) {
		lbn = offset >> fs_bshift;
		error = VOP_BMAP(vp, lbn, NULL, &blkno, &run);
		if (error) {
			UVMHIST_LOG(ubchist, "VOP_BMAP() -> %d", error,0,0,0);
			skipbytes += bytes;
			bytes = 0;
			break;
		}

		iobytes = MIN((((off_t)lbn + 1 + run) << fs_bshift) - offset,
		    bytes);
		if (blkno == (daddr_t)-1) {
			skipbytes += iobytes;
			continue;
		}

		/* if it's really one i/o, don't make a second buf */
		if (offset == startoffset && iobytes == bytes) {
			bp = mbp;
		} else {
			s = splbio();
			vp->v_numoutput++;
			bp = pool_get(&bufpool, PR_WAITOK);
			UVMHIST_LOG(ubchist, "vp %p bp %p num now %d",
				    vp, bp, vp->v_numoutput, 0);
			splx(s);
			bp->b_data = (char *)kva +
				(vaddr_t)(offset - pg->offset);
			bp->b_resid = bp->b_bcount = iobytes;
			bp->b_flags = B_BUSY|B_WRITE|B_CALL|B_ASYNC;
			bp->b_iodone = uvm_aio_biodone1;
			bp->b_vp = vp;
			LIST_INIT(&bp->b_dep);
		}
		bp->b_lblkno = 0;
		bp->b_private = mbp;

		/* adjust physical blkno for partial blocks */
		bp->b_blkno = blkno + ((offset - ((off_t)lbn << fs_bshift)) >>
				       dev_bshift);
		UVMHIST_LOG(ubchist, "vp %p offset 0x%x bcount 0x%x blkno 0x%x",
			    vp, offset, bp->b_bcount, bp->b_blkno);
		VOP_STRATEGY(bp);
	}
	if (skipbytes) {
		UVMHIST_LOG(ubchist, "skipbytes %d", skipbytes, 0,0,0);
		s = splbio();
		mbp->b_resid -= skipbytes;
		if (error) {
			mbp->b_flags |= B_ERROR;
			mbp->b_error = error;
		}
		if (mbp->b_resid == 0) {
			biodone(mbp);
		}
		splx(s);
	}
	if (async) {
		UVMHIST_LOG(ubchist, "returning PEND", 0,0,0,0);
		return EINPROGRESS;
	}
	if (bp != NULL) {
		UVMHIST_LOG(ubchist, "waiting for mbp %p", mbp,0,0,0);
		error = biowait(mbp);
	}
	if (bioops.io_pageiodone) {
		(*bioops.io_pageiodone)(mbp);
	}
	s = splbio();
	if (mbp->b_vp) {
		vwakeup(mbp->b_vp);
		brelvp(mbp);
	}
	pool_put(&bufpool, mbp);
	splx(s);
	uvm_pagermapout(kva, npages);
	UVMHIST_LOG(ubchist, "returning, error %d", error,0,0,0);
	return error;
}

int
genfs_size(v)
	void *v;
{
	struct vop_size_args /* {
		struct vnode *a_vp;
		off_t a_size;
		off_t *a_eobp;
	} */ *ap = v;
	int bsize;

	bsize = 1 << ap->a_vp->v_mount->mnt_fs_bshift;
	*ap->a_eobp = (ap->a_size + bsize - 1) & ~(bsize - 1);
	return 0;
}

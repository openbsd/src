/*	$NetBSD: ext2fs_balloc.c,v 1.8 2000/12/10 06:38:31 chs Exp $	*/

/*
 * Copyright (c) 1997 Manuel Bouyer.
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
 *	@(#)ffs_balloc.c	8.4 (Berkeley) 9/23/93
 * Modified for ext2fs by Manuel Bouyer.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/mount.h>

#include <uvm/uvm.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufs_extern.h>

#include <ufs/ext2fs/ext2fs.h>
#include <ufs/ext2fs/ext2fs_extern.h>

/*
 * Balloc defines the structure of file system storage
 * by allocating the physical blocks on a device given
 * the inode and the logical block number in a file.
 */
int
ext2fs_buf_alloc(struct inode *ip, daddr_t bn, int size, struct ucred *cred, 
    struct buf **bpp, int flags)
{
	struct m_ext2fs *fs;
	ufs_daddr_t nb;
	struct buf *bp, *nbp;
	struct vnode *vp = ITOV(ip);
	struct indir indirs[NIADDR + 2];
	ufs_daddr_t newb, lbn, *bap, pref;
	int num, i, error;
	u_int deallocated;
	ufs_daddr_t *allocib, *blkp, *allocblk, allociblk[NIADDR + 1];
	int unwindidx = -1;
	UVMHIST_FUNC("ext2fs_buf_alloc"); UVMHIST_CALLED(ubchist);

	UVMHIST_LOG(ubchist, "bn 0x%x", bn,0,0,0);

	if (bpp != NULL) {
		*bpp = NULL;
	}
	if (bn < 0)
		return (EFBIG);
	fs = ip->i_e2fs;
	lbn = bn;

	/*
	 * The first NDADDR blocks are direct blocks
	 */
	if (bn < NDADDR) {
		nb = fs2h32(ip->i_e2fs_blocks[bn]);
		if (nb != 0) {

			/*
			 * the block is already allocated, just read it.
			 */

			if (bpp != NULL) {
				error = bread(vp, bn, fs->e2fs_bsize, NOCRED,
					      &bp);
				if (error) {
					brelse(bp);
					return (error);
				}
				*bpp = bp;
			}
			return (0);
		}

		/*
		 * allocate a new direct block.
		 */

		error = ext2fs_alloc(ip, bn,
		    ext2fs_blkpref(ip, bn, bn, &ip->i_e2fs_blocks[0]),
		    cred, &newb);
		if (error)
			return (error);
		ip->i_e2fs_last_lblk = lbn;
		ip->i_e2fs_last_blk = newb;
		ip->i_e2fs_blocks[bn] = h2fs32(newb);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		if (bpp != NULL) {
			bp = getblk(vp, bn, fs->e2fs_bsize, 0, 0);
			bp->b_blkno = fsbtodb(fs, newb);
			if (flags & B_CLRBUF)
				clrbuf(bp);
			*bpp = bp;
		}
		return (0);
	}
	/*
	 * Determine the number of levels of indirection.
	 */
	pref = 0;
	if ((error = ufs_getlbns(vp, bn, indirs, &num)) != 0)
		return(error);
#ifdef DIAGNOSTIC
	if (num < 1)
		panic ("ext2fs_balloc: ufs_getlbns returned indirect block");
#endif
	/*
	 * Fetch the first indirect block allocating if necessary.
	 */
	--num;
	nb = fs2h32(ip->i_e2fs_blocks[NDADDR + indirs[0].in_off]);
	allocib = NULL;
	allocblk = allociblk;
	if (nb == 0) {
		pref = ext2fs_blkpref(ip, lbn, 0, (ufs_daddr_t *)0);
		error = ext2fs_alloc(ip, lbn, pref, cred, &newb);
		if (error)
			return (error);
		nb = newb;
		*allocblk++ = nb;
		ip->i_e2fs_last_blk = newb;
		bp = getblk(vp, indirs[1].in_lbn, fs->e2fs_bsize, 0, 0);
		bp->b_blkno = fsbtodb(fs, newb);
		clrbuf(bp);
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		if ((error = bwrite(bp)) != 0)
			goto fail;
		unwindidx = 0;
		allocib = &ip->i_e2fs_blocks[NDADDR + indirs[0].in_off];
		*allocib = h2fs32(newb);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	/*
	 * Fetch through the indirect blocks, allocating as necessary.
	 */
	for (i = 1;;) {
		error = bread(vp,
		    indirs[i].in_lbn, (int)fs->e2fs_bsize, NOCRED, &bp);
		if (error) {
			brelse(bp);
			goto fail;
		}
		bap = (ufs_daddr_t *)bp->b_data;
		nb = fs2h32(bap[indirs[i].in_off]);
		if (i == num)
			break;
		i++;
		if (nb != 0) {
			brelse(bp);
			continue;
		}
		pref = ext2fs_blkpref(ip, lbn, 0, (ufs_daddr_t *)0);
		error = ext2fs_alloc(ip, lbn, pref, cred, &newb);
		if (error) {
			brelse(bp);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		ip->i_e2fs_last_blk = newb;
		nbp = getblk(vp, indirs[i].in_lbn, fs->e2fs_bsize, 0, 0);
		nbp->b_blkno = fsbtodb(fs, nb);
		clrbuf(nbp);
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		if ((error = bwrite(nbp)) != 0) {
			brelse(bp);
			goto fail;
		}
		if (unwindidx < 0)
			unwindidx = i - 1;
		bap[indirs[i - 1].in_off] = h2fs32(nb);
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		if (flags & B_SYNC) {
			bwrite(bp);
		} else {
			bdwrite(bp);
		}
	}
	/*
	 * Get the data block, allocating if necessary.
	 */
	if (nb == 0) {
		pref = ext2fs_blkpref(ip, lbn, indirs[num].in_off, &bap[0]);
		error = ext2fs_alloc(ip, lbn, pref, cred, &newb);
		if (error) {
			brelse(bp);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		ip->i_e2fs_last_lblk = lbn;
		ip->i_e2fs_last_blk = newb;
		bap[indirs[num].in_off] = h2fs32(nb);
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		if (flags & B_SYNC) {
			bwrite(bp);
		} else {
			bdwrite(bp);
		}
		if (bpp != NULL) {
			nbp = getblk(vp, lbn, fs->e2fs_bsize, 0, 0);
			nbp->b_blkno = fsbtodb(fs, nb);
			if (flags & B_CLRBUF)
				clrbuf(nbp);
			*bpp = nbp;
		}
		return (0);
	}
	brelse(bp);
	if (bpp != NULL) {
		if (flags & B_CLRBUF) {
			error = bread(vp, lbn, (int)fs->e2fs_bsize, NOCRED,
				      &nbp);
			if (error) {
				brelse(nbp);
				goto fail;
			}
		} else {
			nbp = getblk(vp, lbn, fs->e2fs_bsize, 0, 0);
			nbp->b_blkno = fsbtodb(fs, nb);
		}
		*bpp = nbp;
	}
	return (0);
fail:
	/*
	 * If we have failed part way through block allocation, we
	 * have to deallocate any indirect blocks that we have allocated.
	 */
	for (deallocated = 0, blkp = allociblk; blkp < allocblk; blkp++) {
		ext2fs_blkfree(ip, *blkp);
		deallocated += fs->e2fs_bsize;
	}
	if (unwindidx >= 0) {
		if (unwindidx == 0) {
			*allocib = 0;
		} else {
			int r;
	
			r = bread(vp, indirs[unwindidx].in_lbn, 
			    (int)fs->e2fs_bsize, NOCRED, &bp);
			if (r) {
				panic("Could not unwind indirect block, error %d", r);
				brelse(bp);
			} else {
				bap = (ufs_daddr_t *)bp->b_data;
				bap[indirs[unwindidx].in_off] = 0;
				if (flags & B_SYNC)
					bwrite(bp);
				else
					bdwrite(bp);
			}
		}
		for (i = unwindidx + 1; i <= num; i++) {
			bp = getblk(vp, indirs[i].in_lbn, (int)fs->e2fs_bsize,
			    0, 0);
			bp->b_flags |= B_INVAL;
			brelse(bp);
		}
	}
	if (deallocated) {
		ip->i_e2fs_nblock -= btodb(deallocated);
		ip->i_e2fs_flags |= IN_CHANGE | IN_UPDATE;
	}
	return error;
}

int
ext2fs_gop_alloc(struct vnode *vp, off_t off, off_t len, int flags,
    struct ucred *cred)
{
	struct inode *ip = VTOI(vp);
	struct m_ext2fs *fs = ip->i_e2fs;
	int error, delta, bshift, bsize;
	UVMHIST_FUNC("ext2fs_gop_alloc"); UVMHIST_CALLED(ubchist);

	bshift = fs->e2fs_bshift;
	bsize = 1 << bshift;

	delta = off & (bsize - 1);
	off -= delta;
	len += delta;

	while (len > 0) {
		bsize = min(bsize, len);
		UVMHIST_LOG(ubchist, "off 0x%x len 0x%x bsize 0x%x",
			    off, len, bsize, 0);

		error = ext2fs_buf_alloc(ip, lblkno(fs, off), bsize, cred,
		    NULL, flags);
		if (error) {
			UVMHIST_LOG(ubchist, "error %d", error, 0,0,0);
			return error;
		}

		/*
		 * increase file size now, VOP_BALLOC() requires that
		 * EOF be up-to-date before each call.
		 */

		if (ip->i_e2fs_size < off + bsize) {
			UVMHIST_LOG(ubchist, "old 0x%x new 0x%x",
				    ip->i_e2fs_size, off + bsize,0,0);
			ip->i_e2fs_size = off + bsize;
			if (vp->v_size < ip->i_e2fs_size) {
				uvm_vnp_setsize(vp, ip->i_e2fs_size);
			}
		}

		off += bsize;
		len -= bsize;
	}
	return 0;
}

/*
 * allocate a range of blocks in a file.
 * after this function returns, any page entirely contained within the range
 * will map to invalid data and thus must be overwritten before it is made
 * accessible to others.
 */

int
ext2fs_balloc_range(vp, off, len, cred, flags)
	struct vnode *vp;
	off_t off, len;
	struct ucred *cred;
	int flags;
{
	off_t oldeof, eof, pagestart;
	struct uvm_object *uobj;
	struct genfs_node *gp = VTOG(vp);
	int i, delta, error, npages;
	int bshift = vp->v_mount->mnt_fs_bshift;
	int bsize = 1 << bshift;
	int ppb = max(bsize >> PAGE_SHIFT, 1);
	struct vm_page *pgs[ppb];
	UVMHIST_FUNC("ext2fs_balloc_range"); UVMHIST_CALLED(ubchist);
	UVMHIST_LOG(ubchist, "vp %p off 0x%x len 0x%x u_size 0x%x",
		    vp, off, len, vp->v_size);

	error = 0;
	uobj = &vp->v_uobj;
	oldeof = vp->v_size;
	eof = max(oldeof, off + len);
	UVMHIST_LOG(ubchist, "new eof 0x%x", eof,0,0,0);
	pgs[0] = NULL;

	/*
	 * cache the new range of the file.  this will create zeroed pages
	 * where the new block will be and keep them locked until the
	 * new block is allocated, so there will be no window where
	 * the old contents of the new block is visible to racing threads.
	 */

	pagestart = trunc_page(off) & ~(bsize - 1);
	npages = min(ppb, (round_page(eof) - pagestart) >> PAGE_SHIFT);
	memset(pgs, 0, npages);
	simple_lock(&uobj->vmobjlock);
	error = VOP_GETPAGES(vp, pagestart, pgs, &npages, 0,
	    VM_PROT_READ, 0, PGO_SYNCIO | PGO_PASTEOF);
	if (error) {
		UVMHIST_LOG(ubchist, "getpages %d", error,0,0,0);
		goto errout;
	}
	for (i = 0; i < npages; i++) {
		UVMHIST_LOG(ubchist, "got pgs[%d] %p", i, pgs[i],0,0);
		KASSERT((pgs[i]->flags & PG_RELEASED) == 0);
		pgs[i]->flags &= ~PG_CLEAN;
		uvm_pageactivate(pgs[i]);
	}

	/*
	 * adjust off to be block-aligned.
	 */

	delta = off & (bsize - 1);
	off -= delta;
	len += delta;

	/*
	 * now allocate the range.
	 */

	lockmgr(&gp->g_glock, LK_EXCLUSIVE, NULL, curproc);
	error = GOP_ALLOC(vp, off, len, flags, cred);
	UVMHIST_LOG(ubchist, "alloc %d", error,0,0,0);
	lockmgr(&gp->g_glock, LK_RELEASE, NULL, curproc);

	/*
	 * unbusy any pages we are holding.
	 */

errout:
	simple_lock(&uobj->vmobjlock);
	if (error) {
		(void) (uobj->pgops->pgo_flush)(uobj, oldeof, pagestart + ppb,
		    PGO_FREE);
	}
	if (pgs[0] != NULL) {
		uvm_page_unbusy(pgs, npages);
	}
	simple_unlock(&uobj->vmobjlock);
	return (error);
}

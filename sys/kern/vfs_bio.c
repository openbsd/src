/*	$OpenBSD: vfs_bio.c,v 1.169 2015/03/14 03:38:51 jsg Exp $	*/
/*	$NetBSD: vfs_bio.c,v 1.44 1996/06/11 11:15:36 pk Exp $	*/

/*
 * Copyright (c) 1994 Christopher G. Demetriou
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
 *	@(#)vfs_bio.c	8.6 (Berkeley) 1/11/94
 */

/*
 * Some references:
 *	Bach: The Design of the UNIX Operating System (Prentice Hall, 1986)
 *	Leffler, et al.: The Design and Implementation of the 4.3BSD
 *		UNIX Operating System (Addison Welley, 1989)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/resourcevar.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/specdev.h>
#include <uvm/uvm_extern.h>

int nobuffers;
int needbuffer;
struct bio_ops bioops;

/* private bufcache functions */
void bufcache_init(void);
void bufcache_adjust(void);

/*
 * Buffer pool for I/O buffers.
 */
struct pool bufpool;
struct bufhead bufhead = LIST_HEAD_INITIALIZER(bufhead);
void buf_put(struct buf *);

struct buf *bio_doread(struct vnode *, daddr_t, int, int);
struct buf *buf_get(struct vnode *, daddr_t, size_t);
void bread_cluster_callback(struct buf *);

struct bcachestats bcstats;  /* counters */
long lodirtypages;      /* dirty page count low water mark */
long hidirtypages;      /* dirty page count high water mark */
long targetpages;   	/* target number of pages for cache size */
long buflowpages;	/* smallest size cache allowed */
long bufhighpages; 	/* largest size cache allowed */
long bufbackpages; 	/* minimum number of pages we shrink when asked to */

vsize_t bufkvm;

struct proc *cleanerproc;
int bd_req;			/* Sleep point for cleaner daemon. */

void
buf_put(struct buf *bp)
{
	splassert(IPL_BIO);

#ifdef DIAGNOSTIC
	if (bp->b_pobj != NULL)
		KASSERT(bp->b_bufsize > 0);
	if (ISSET(bp->b_flags, B_DELWRI))
		panic("buf_put: releasing dirty buffer");
	if (bp->b_freelist.tqe_next != NOLIST &&
	    bp->b_freelist.tqe_next != (void *)-1)
		panic("buf_put: still on the free list");
	if (bp->b_vnbufs.le_next != NOLIST &&
	    bp->b_vnbufs.le_next != (void *)-1)
		panic("buf_put: still on the vnode list");
	if (!LIST_EMPTY(&bp->b_dep))
		panic("buf_put: b_dep is not empty");
#endif

	LIST_REMOVE(bp, b_list);
	bcstats.numbufs--;

	if (buf_dealloc_mem(bp) != 0)
		return;
	pool_put(&bufpool, bp);
}

/*
 * Initialize buffers and hash links for buffers.
 */
void
bufinit(void)
{
	u_int64_t dmapages;

	dmapages = uvm_pagecount(&dma_constraint);
	/* take away a guess at how much of this the kernel will consume */
	dmapages -= (atop(physmem) - atop(uvmexp.free));

	/*
	 * If MD code doesn't say otherwise, use up to 10% of DMA'able
	 * memory for buffers.
	 */
	if (bufcachepercent == 0)
		bufcachepercent = 10;

	/*
	 * XXX these values and their same use in kern_sysctl
	 * need to move into buf.h
	 */
	KASSERT(bufcachepercent <= 90);
	KASSERT(bufcachepercent >= 5);
	if (bufpages == 0)
		bufpages = dmapages * bufcachepercent / 100;
	if (bufpages < BCACHE_MIN)
		bufpages = BCACHE_MIN;
	KASSERT(bufpages < dmapages);

	bufhighpages = bufpages;

	/*
	 * Set the base backoff level for the buffer cache.  We will
	 * not allow uvm to steal back more than this number of pages.
	 */
	buflowpages = dmapages * 5 / 100;
	if (buflowpages < BCACHE_MIN)
		buflowpages = BCACHE_MIN;

	/*
	 * set bufbackpages to 100 pages, or 10 percent of the low water mark
	 * if we don't have that many pages.
	 */

	bufbackpages = buflowpages * 10 / 100;
	if (bufbackpages > 100)
		bufbackpages = 100;

	/*
	 * If the MD code does not say otherwise, reserve 10% of kva
	 * space for mapping buffers.
	 */
	if (bufkvm == 0)
		bufkvm = VM_KERNEL_SPACE_SIZE / 10;

	/*
	 * Don't use more than twice the amount of bufpages for mappings.
	 * It's twice since we map things sparsely.
	 */
	if (bufkvm > bufpages * PAGE_SIZE)
		bufkvm = bufpages * PAGE_SIZE;
	/*
	 * Round bufkvm to MAXPHYS because we allocate chunks of va space
	 * in MAXPHYS chunks.
	 */
	bufkvm &= ~(MAXPHYS - 1);

	pool_init(&bufpool, sizeof(struct buf), 0, 0, 0, "bufpl", NULL);
	pool_setipl(&bufpool, IPL_BIO);

	bufcache_init();

	/*
	 * hmm - bufkvm is an argument because it's static, while
	 * bufpages is global because it can change while running.
 	 */
	buf_mem_init(bufkvm);

	/*
	 * Set the dirty page high water mark to be less than the low
	 * water mark for pages in the buffer cache. This ensures we
	 * can always back off by throwing away clean pages, and give
	 * ourselves a chance to write out the dirty pages eventually.
	 */
	hidirtypages = (buflowpages / 4) * 3;
	lodirtypages = buflowpages / 2;

	/*
	 * We are allowed to use up to the reserve.
	 */
	targetpages = bufpages - RESERVE_PAGES;
}

/*
 * Change cachepct
 */
void
bufadjust(int newbufpages)
{
	struct buf *bp;
	int s;

	if (newbufpages < buflowpages)
		newbufpages = buflowpages;

	s = splbio();
	bufpages = newbufpages;

	/*
	 * We are allowed to use up to the reserve
	 */
	targetpages = bufpages - RESERVE_PAGES;

	/*
	 * Shrinking the cache happens here only if someone has manually
	 * adjusted bufcachepercent - or the pagedaemon has told us
	 * to give back memory *now* - so we give it all back.
	 */
	while ((bp = bufcache_getcleanbuf()) &&
	    (bcstats.numbufpages > targetpages)) {
		bufcache_take(bp);
		if (bp->b_vp) {
			RB_REMOVE(buf_rb_bufs,
			    &bp->b_vp->v_bufs_tree, bp);
			brelvp(bp);
		}
		buf_put(bp);
	}
	bufcache_adjust();

	/*
	 * Wake up the cleaner if we have lots of dirty pages,
	 * or if we are getting low on buffer cache kva.
	 */
	if ((UNCLEAN_PAGES >= hidirtypages) ||
	    bcstats.kvaslots_avail <= 2 * RESERVE_SLOTS)
		wakeup(&bd_req);

	splx(s);
}

/*
 * Make the buffer cache back off from cachepct.
 */
int
bufbackoff(struct uvm_constraint_range *range, long size)
{
	/*
	 * Back off "size" buffer cache pages. Called by the page
	 * daemon to consume buffer cache pages rather than scanning.
	 *
	 * It returns 0 to the pagedaemon to indicate that it has
	 * succeeded in freeing enough pages. It returns -1 to
	 * indicate that it could not and the pagedaemon should take
	 * other measures.
	 *
	 */
	long pdelta, oldbufpages;

	/*
	 * Back off by at least bufbackpages. If the page daemon gave us
	 * a larger size, back off by that much.
	 */
	pdelta = (size > bufbackpages) ? size : bufbackpages;

	if (bufpages <= buflowpages)
		return(-1);
	if (bufpages - pdelta < buflowpages)
		pdelta = bufpages - buflowpages;
	oldbufpages = bufpages;
	bufadjust(bufpages - pdelta);
	if (oldbufpages - bufpages < size)
		return (-1); /* we did not free what we were asked */
	else
		return(0);
}

struct buf *
bio_doread(struct vnode *vp, daddr_t blkno, int size, int async)
{
	struct buf *bp;
	struct mount *mp;

	bp = getblk(vp, blkno, size, 0, 0);

	/*
	 * If buffer does not have valid data, start a read.
	 * Note that if buffer is B_INVAL, getblk() won't return it.
	 * Therefore, it's valid if its I/O has completed or been delayed.
	 */
	if (!ISSET(bp->b_flags, (B_DONE | B_DELWRI))) {
		SET(bp->b_flags, B_READ | async);
		bcstats.pendingreads++;
		bcstats.numreads++;
		VOP_STRATEGY(bp);
		/* Pay for the read. */
		curproc->p_ru.ru_inblock++;			/* XXX */
	} else if (async) {
		brelse(bp);
	}

	mp = vp->v_type == VBLK? vp->v_specmountpoint : vp->v_mount;

	/*
	 * Collect statistics on synchronous and asynchronous reads.
	 * Reads from block devices are charged to their associated
	 * filesystem (if any).
	 */
	if (mp != NULL) {
		if (async == 0)
			mp->mnt_stat.f_syncreads++;
		else
			mp->mnt_stat.f_asyncreads++;
	}

	return (bp);
}

/*
 * Read a disk block.
 * This algorithm described in Bach (p.54).
 */
int
bread(struct vnode *vp, daddr_t blkno, int size, struct buf **bpp)
{
	struct buf *bp;

	/* Get buffer for block. */
	bp = *bpp = bio_doread(vp, blkno, size, 0);

	/* Wait for the read to complete, and return result. */
	return (biowait(bp));
}

/*
 * Read-ahead multiple disk blocks. The first is sync, the rest async.
 * Trivial modification to the breada algorithm presented in Bach (p.55).
 */
int
breadn(struct vnode *vp, daddr_t blkno, int size, daddr_t rablks[],
    int rasizes[], int nrablks, struct buf **bpp)
{
	struct buf *bp;
	int i;

	bp = *bpp = bio_doread(vp, blkno, size, 0);

	/*
	 * For each of the read-ahead blocks, start a read, if necessary.
	 */
	for (i = 0; i < nrablks; i++) {
		/* If it's in the cache, just go on to next one. */
		if (incore(vp, rablks[i]))
			continue;

		/* Get a buffer for the read-ahead block */
		(void) bio_doread(vp, rablks[i], rasizes[i], B_ASYNC);
	}

	/* Otherwise, we had to start a read for it; wait until it's valid. */
	return (biowait(bp));
}

/*
 * Called from interrupt context.
 */
void
bread_cluster_callback(struct buf *bp)
{
	struct buf **xbpp = bp->b_saveaddr;
	int i;

	if (xbpp[1] != NULL) {
		size_t newsize = xbpp[1]->b_bufsize;

		/*
		 * Shrink this buffer's mapping to only cover its part of
		 * the total I/O.
		 */
		buf_fix_mapping(bp, newsize);
		bp->b_bcount = newsize;
	}

	for (i = 1; xbpp[i] != 0; i++) {
		if (ISSET(bp->b_flags, B_ERROR))
			SET(xbpp[i]->b_flags, B_INVAL | B_ERROR);
		biodone(xbpp[i]);
	}

	free(xbpp, M_TEMP, 0);

	if (ISSET(bp->b_flags, B_ASYNC)) {
		brelse(bp);
	} else {
		CLR(bp->b_flags, B_WANTED);
		wakeup(bp);
	}
}

int
bread_cluster(struct vnode *vp, daddr_t blkno, int size, struct buf **rbpp)
{
	struct buf *bp, **xbpp;
	int howmany, maxra, i, inc;
	daddr_t sblkno;

	*rbpp = bio_doread(vp, blkno, size, 0);

	if (size != round_page(size))
		goto out;

	if (VOP_BMAP(vp, blkno + 1, NULL, &sblkno, &maxra))
		goto out;

	maxra++; 
	if (sblkno == -1 || maxra < 2)
		goto out;

	howmany = MAXPHYS / size;
	if (howmany > maxra)
		howmany = maxra;

	xbpp = mallocarray(howmany + 1, sizeof(struct buf *), M_TEMP, M_NOWAIT);
	if (xbpp == NULL)
		goto out;

	for (i = howmany - 1; i >= 0; i--) {
		size_t sz;

		/*
		 * First buffer allocates big enough size to cover what
		 * all the other buffers need.
		 */
		sz = i == 0 ? howmany * size : 0;

		xbpp[i] = buf_get(vp, blkno + i + 1, sz);
		if (xbpp[i] == NULL) {
			for (++i; i < howmany; i++) {
				SET(xbpp[i]->b_flags, B_INVAL);
				brelse(xbpp[i]);
			}
			free(xbpp, M_TEMP, 0);
			goto out;
		}
	}

	bp = xbpp[0];

	xbpp[howmany] = 0;

	inc = btodb(size);

	for (i = 1; i < howmany; i++) {
		bcstats.pendingreads++;
		bcstats.numreads++;
		SET(xbpp[i]->b_flags, B_READ | B_ASYNC);
		xbpp[i]->b_blkno = sblkno + (i * inc);
		xbpp[i]->b_bufsize = xbpp[i]->b_bcount = size;
		xbpp[i]->b_data = NULL;
		xbpp[i]->b_pobj = bp->b_pobj;
		xbpp[i]->b_poffs = bp->b_poffs + (i * size);
	}

	KASSERT(bp->b_lblkno == blkno + 1);
	KASSERT(bp->b_vp == vp);

	bp->b_blkno = sblkno;
	SET(bp->b_flags, B_READ | B_ASYNC | B_CALL);

	bp->b_saveaddr = (void *)xbpp;
	bp->b_iodone = bread_cluster_callback;

	bcstats.pendingreads++;
	bcstats.numreads++;
	VOP_STRATEGY(bp);
	curproc->p_ru.ru_inblock++;

out:
	return (biowait(*rbpp));
}

/*
 * Block write.  Described in Bach (p.56)
 */
int
bwrite(struct buf *bp)
{
	int rv, async, wasdelayed, s;
	struct vnode *vp;
	struct mount *mp;

	vp = bp->b_vp;
	if (vp != NULL)
		mp = vp->v_type == VBLK? vp->v_specmountpoint : vp->v_mount;
	else
		mp = NULL;

	/*
	 * Remember buffer type, to switch on it later.  If the write was
	 * synchronous, but the file system was mounted with MNT_ASYNC,
	 * convert it to a delayed write.
	 * XXX note that this relies on delayed tape writes being converted
	 * to async, not sync writes (which is safe, but ugly).
	 */
	async = ISSET(bp->b_flags, B_ASYNC);
	if (!async && mp && ISSET(mp->mnt_flag, MNT_ASYNC)) {
		bdwrite(bp);
		return (0);
	}

	/*
	 * Collect statistics on synchronous and asynchronous writes.
	 * Writes to block devices are charged to their associated
	 * filesystem (if any).
	 */
	if (mp != NULL) {
		if (async)
			mp->mnt_stat.f_asyncwrites++;
		else
			mp->mnt_stat.f_syncwrites++;
	}
	bcstats.pendingwrites++;
	bcstats.numwrites++;

	wasdelayed = ISSET(bp->b_flags, B_DELWRI);
	CLR(bp->b_flags, (B_READ | B_DONE | B_ERROR | B_DELWRI));

	s = splbio();

	/*
	 * If not synchronous, pay for the I/O operation and make
	 * sure the buf is on the correct vnode queue.  We have
	 * to do this now, because if we don't, the vnode may not
	 * be properly notified that its I/O has completed.
	 */
	if (wasdelayed) {
		reassignbuf(bp);
	} else
		curproc->p_ru.ru_oublock++;


	/* Initiate disk write.  Make sure the appropriate party is charged. */
	bp->b_vp->v_numoutput++;
	splx(s);
	SET(bp->b_flags, B_WRITEINPROG);
	VOP_STRATEGY(bp);

	/*
	 * If the queue is above the high water mark, wait till
	 * the number of outstanding write bufs drops below the low
	 * water mark.
	 */
	if (bp->b_bq)
		bufq_wait(bp->b_bq);

	if (async)
		return (0);

	/*
	 * If I/O was synchronous, wait for it to complete.
	 */
	rv = biowait(bp);

	/* Release the buffer. */
	brelse(bp);

	return (rv);
}


/*
 * Delayed write.
 *
 * The buffer is marked dirty, but is not queued for I/O.
 * This routine should be used when the buffer is expected
 * to be modified again soon, typically a small write that
 * partially fills a buffer.
 *
 * NB: magnetic tapes cannot be delayed; they must be
 * written in the order that the writes are requested.
 *
 * Described in Leffler, et al. (pp. 208-213).
 */
void
bdwrite(struct buf *bp)
{
	int s;

	/*
	 * If the block hasn't been seen before:
	 *	(1) Mark it as having been seen,
	 *	(2) Charge for the write.
	 *	(3) Make sure it's on its vnode's correct block list,
	 *	(4) If a buffer is rewritten, move it to end of dirty list
	 */
	if (!ISSET(bp->b_flags, B_DELWRI)) {
		SET(bp->b_flags, B_DELWRI);
		s = splbio();
		reassignbuf(bp);
		splx(s);
		curproc->p_ru.ru_oublock++;		/* XXX */
	}

	/* If this is a tape block, write the block now. */
	if (major(bp->b_dev) < nblkdev &&
	    bdevsw[major(bp->b_dev)].d_type == D_TAPE) {
		bawrite(bp);
		return;
	}

	/* Otherwise, the "write" is done, so mark and release the buffer. */
	CLR(bp->b_flags, B_NEEDCOMMIT);
	SET(bp->b_flags, B_DONE);
	brelse(bp);
}

/*
 * Asynchronous block write; just an asynchronous bwrite().
 */
void
bawrite(struct buf *bp)
{

	SET(bp->b_flags, B_ASYNC);
	VOP_BWRITE(bp);
}

/*
 * Must be called at splbio()
 */
void
buf_dirty(struct buf *bp)
{
	splassert(IPL_BIO);

#ifdef DIAGNOSTIC
	if (!ISSET(bp->b_flags, B_BUSY))
		panic("Trying to dirty buffer on freelist!");
#endif

	if (ISSET(bp->b_flags, B_DELWRI) == 0) {
		SET(bp->b_flags, B_DELWRI);
		reassignbuf(bp);
	}
}

/*
 * Must be called at splbio()
 */
void
buf_undirty(struct buf *bp)
{
	splassert(IPL_BIO);

#ifdef DIAGNOSTIC
	if (!ISSET(bp->b_flags, B_BUSY))
		panic("Trying to undirty buffer on freelist!");
#endif
	if (ISSET(bp->b_flags, B_DELWRI)) {
		CLR(bp->b_flags, B_DELWRI);
		reassignbuf(bp);
	}
}

/*
 * Release a buffer on to the free lists.
 * Described in Bach (p. 46).
 */
void
brelse(struct buf *bp)
{
	int s;

	s = splbio();

	if (bp->b_data != NULL)
		KASSERT(bp->b_bufsize > 0);

	/*
	 * Determine which queue the buffer should be on, then put it there.
	 */

	/* If it's not cacheable, or an error, mark it invalid. */
	if (ISSET(bp->b_flags, (B_NOCACHE|B_ERROR)))
		SET(bp->b_flags, B_INVAL);

	if (ISSET(bp->b_flags, B_INVAL)) {
		/*
		 * If the buffer is invalid, free it now rather than leaving
		 * it in a queue and wasting memory.
		 */
		if (LIST_FIRST(&bp->b_dep) != NULL)
			buf_deallocate(bp);

		if (ISSET(bp->b_flags, B_DELWRI)) {
			CLR(bp->b_flags, B_DELWRI);
		}

		if (bp->b_vp) {
			RB_REMOVE(buf_rb_bufs, &bp->b_vp->v_bufs_tree,
			    bp);
			brelvp(bp);
		}
		bp->b_vp = NULL;

		/*
		 * Wake up any processes waiting for _this_ buffer to
		 * become free. They are not allowed to grab it
		 * since it will be freed. But the only sleeper is
		 * getblk and it will restart the operation after
		 * sleep.
		 */
		if (ISSET(bp->b_flags, B_WANTED)) {
			CLR(bp->b_flags, B_WANTED);
			wakeup(bp);
		}
		buf_put(bp);
	} else {
		/*
		 * It has valid data.  Put it on the end of the appropriate
		 * queue, so that it'll stick around for as long as possible.
		 */
		bufcache_release(bp);

		/* Unlock the buffer. */
		CLR(bp->b_flags, (B_AGE | B_ASYNC | B_NOCACHE | B_DEFERRED));
		buf_release(bp);

		/* Wake up any processes waiting for _this_ buffer to
		 * become free. */
		if (ISSET(bp->b_flags, B_WANTED)) {
			CLR(bp->b_flags, B_WANTED);
			wakeup(bp);
		}
	}

	/* Wake up syncer and cleaner processes waiting for buffers. */
	if (nobuffers) {
		nobuffers = 0;
		wakeup(&nobuffers);
	}

	/* Wake up any processes waiting for any buffer to become free. */
	if (needbuffer && bcstats.numbufpages < targetpages &&
	    bcstats.kvaslots_avail > RESERVE_SLOTS) {
		needbuffer = 0;
		wakeup(&needbuffer);
	}

	splx(s);
}

/*
 * Determine if a block is in the cache. Just look on what would be its hash
 * chain. If it's there, return a pointer to it, unless it's marked invalid.
 */
struct buf *
incore(struct vnode *vp, daddr_t blkno)
{
	struct buf *bp;
	struct buf b;
	int s;

	s = splbio();

	/* Search buf lookup tree */
	b.b_lblkno = blkno;
	bp = RB_FIND(buf_rb_bufs, &vp->v_bufs_tree, &b);
	if (bp != NULL && ISSET(bp->b_flags, B_INVAL))
		bp = NULL;

	splx(s);
	return (bp);
}

/*
 * Get a block of requested size that is associated with
 * a given vnode and block offset. If it is found in the
 * block cache, mark it as having been found, make it busy
 * and return it. Otherwise, return an empty block of the
 * correct size. It is up to the caller to ensure that the
 * cached blocks be of the correct size.
 */
struct buf *
getblk(struct vnode *vp, daddr_t blkno, int size, int slpflag, int slptimeo)
{
	struct buf *bp;
	struct buf b;
	int s, error;

	/*
	 * XXX
	 * The following is an inlined version of 'incore()', but with
	 * the 'invalid' test moved to after the 'busy' test.  It's
	 * necessary because there are some cases in which the NFS
	 * code sets B_INVAL prior to writing data to the server, but
	 * in which the buffers actually contain valid data.  In this
	 * case, we can't allow the system to allocate a new buffer for
	 * the block until the write is finished.
	 */
start:
	s = splbio();
	b.b_lblkno = blkno;
	bp = RB_FIND(buf_rb_bufs, &vp->v_bufs_tree, &b);
	if (bp != NULL) {
		if (ISSET(bp->b_flags, B_BUSY)) {
			SET(bp->b_flags, B_WANTED);
			error = tsleep(bp, slpflag | (PRIBIO + 1), "getblk",
			    slptimeo);
			splx(s);
			if (error)
				return (NULL);
			goto start;
		}

		if (!ISSET(bp->b_flags, B_INVAL)) {
			bcstats.cachehits++;
			SET(bp->b_flags, B_CACHE);
			bufcache_take(bp);
			buf_acquire(bp);
			splx(s);
			return (bp);
		}
	}
	splx(s);

	if ((bp = buf_get(vp, blkno, size)) == NULL)
		goto start;

	return (bp);
}

/*
 * Get an empty, disassociated buffer of given size.
 */
struct buf *
geteblk(int size)
{
	struct buf *bp;

	while ((bp = buf_get(NULL, 0, size)) == NULL)
		;

	return (bp);
}

/*
 * Allocate a buffer.
 */
struct buf *
buf_get(struct vnode *vp, daddr_t blkno, size_t size)
{
	struct buf *bp;
	int poolwait = size == 0 ? PR_NOWAIT : PR_WAITOK;
	int npages;
	int s;

	s = splbio();
	if (size) {
		/*
		 * Wake up the cleaner if we have lots of dirty pages,
		 * or if we are getting low on buffer cache kva.
		 */
		if (UNCLEAN_PAGES >= hidirtypages ||
			bcstats.kvaslots_avail <= 2 * RESERVE_SLOTS)
			wakeup(&bd_req);

		npages = atop(round_page(size));

		/*
		 * if our cache has been previously shrunk,
		 * allow it to grow again with use up to
		 * bufhighpages (cachepercent)
		 */
		if (bufpages < bufhighpages)
			bufadjust(bufhighpages);

		/*
		 * If would go over the page target with our
		 * new allocation, free enough buffers first
		 * to stay at the target with our new allocation.
		 */
		while ((bcstats.numbufpages + npages > targetpages) &&
		    (bp = bufcache_getcleanbuf())) {
			bufcache_take(bp);
			if (bp->b_vp) {
				RB_REMOVE(buf_rb_bufs,
				    &bp->b_vp->v_bufs_tree, bp);
				brelvp(bp);
			}
			buf_put(bp);
		}

		/*
		 * If we get here, we tried to free the world down
		 * above, and couldn't get down - Wake the cleaner
		 * and wait for it to push some buffers out.
		 */
		if ((bcstats.numbufpages + npages > targetpages ||
		    bcstats.kvaslots_avail <= RESERVE_SLOTS) &&
		    curproc != syncerproc && curproc != cleanerproc) {
			wakeup(&bd_req);
			needbuffer++;
			tsleep(&needbuffer, PRIBIO, "needbuffer", 0);
			splx(s);
			return (NULL);
		}
		if (bcstats.numbufpages + npages > bufpages) {
			/* cleaner or syncer */
			nobuffers = 1;
			tsleep(&nobuffers, PRIBIO, "nobuffers", 0);
			splx(s);
			return (NULL);
		}
	}

	bp = pool_get(&bufpool, poolwait|PR_ZERO);

	if (bp == NULL) {
		splx(s);
		return (NULL);
	}

	bp->b_freelist.tqe_next = NOLIST;
	bp->b_dev = NODEV;
	LIST_INIT(&bp->b_dep);
	bp->b_bcount = size;

	buf_acquire_nomap(bp);

	if (vp != NULL) {
		/*
		 * We insert the buffer into the hash with B_BUSY set
		 * while we allocate pages for it. This way any getblk
		 * that happens while we allocate pages will wait for
		 * this buffer instead of starting its own buf_get.
		 *
		 * But first, we check if someone beat us to it.
		 */
		if (incore(vp, blkno)) {
			pool_put(&bufpool, bp);
			splx(s);
			return (NULL);
		}

		bp->b_blkno = bp->b_lblkno = blkno;
		bgetvp(vp, bp);
		if (RB_INSERT(buf_rb_bufs, &vp->v_bufs_tree, bp))
			panic("buf_get: dup lblk vp %p bp %p", vp, bp);
	} else {
		bp->b_vnbufs.le_next = NOLIST;
		SET(bp->b_flags, B_INVAL);
		bp->b_vp = NULL;
	}

	LIST_INSERT_HEAD(&bufhead, bp, b_list);
	bcstats.numbufs++;

	if (size) {
		buf_alloc_pages(bp, round_page(size));
		buf_map(bp);
	}

	splx(s);

	return (bp);
}

/*
 * Buffer cleaning daemon.
 */
void
buf_daemon(struct proc *p)
{
	struct buf *bp = NULL;
	int s, pushed = 0;

	cleanerproc = curproc;

	s = splbio();
	for (;;) {
		if (bp == NULL || (pushed >= 16 &&
		    UNCLEAN_PAGES < hidirtypages &&
		    bcstats.kvaslots_avail > 2 * RESERVE_SLOTS)){
			pushed = 0;
			/*
			 * Wake up anyone who was waiting for buffers
			 * to be released.
			 */
			if (needbuffer) {
				needbuffer = 0;
				wakeup(&needbuffer);
			}
			tsleep(&bd_req, PRIBIO - 7, "cleaner", 0);
		}

		while ((bp = bufcache_getdirtybuf())) {

			if (UNCLEAN_PAGES < lodirtypages &&
			    bcstats.kvaslots_avail > 2 * RESERVE_SLOTS &&
			    pushed >= 16)
				break;

			bufcache_take(bp);
			buf_acquire(bp);
			splx(s);

			if (ISSET(bp->b_flags, B_INVAL)) {
				brelse(bp);
				s = splbio();
				continue;
			}
#ifdef DIAGNOSTIC
			if (!ISSET(bp->b_flags, B_DELWRI))
				panic("Clean buffer on dirty queue");
#endif
			if (LIST_FIRST(&bp->b_dep) != NULL &&
			    !ISSET(bp->b_flags, B_DEFERRED) &&
			    buf_countdeps(bp, 0, 0)) {
				SET(bp->b_flags, B_DEFERRED);
				s = splbio();
				bufcache_release(bp);
				buf_release(bp);
				continue;
			}

			bawrite(bp);
			pushed++;

			sched_pause();

			s = splbio();
		}
	}
}

/*
 * Wait for operations on the buffer to complete.
 * When they do, extract and return the I/O's error value.
 */
int
biowait(struct buf *bp)
{
	int s;

	KASSERT(!(bp->b_flags & B_ASYNC));

	s = splbio();
	while (!ISSET(bp->b_flags, B_DONE))
		tsleep(bp, PRIBIO + 1, "biowait", 0);
	splx(s);

	/* check for interruption of I/O (e.g. via NFS), then errors. */
	if (ISSET(bp->b_flags, B_EINTR)) {
		CLR(bp->b_flags, B_EINTR);
		return (EINTR);
	}

	if (ISSET(bp->b_flags, B_ERROR))
		return (bp->b_error ? bp->b_error : EIO);
	else
		return (0);
}

/*
 * Mark I/O complete on a buffer.
 *
 * If a callback has been requested, e.g. the pageout
 * daemon, do so. Otherwise, awaken waiting processes.
 *
 * [ Leffler, et al., says on p.247:
 *	"This routine wakes up the blocked process, frees the buffer
 *	for an asynchronous write, or, for a request by the pagedaemon
 *	process, invokes a procedure specified in the buffer structure" ]
 *
 * In real life, the pagedaemon (or other system processes) wants
 * to do async stuff to, and doesn't want the buffer brelse()'d.
 * (for swap pager, that puts swap buffers on the free lists (!!!),
 * for the vn device, that puts malloc'd buffers on the free lists!)
 *
 * Must be called at splbio().
 */
void
biodone(struct buf *bp)
{
	splassert(IPL_BIO);

	if (ISSET(bp->b_flags, B_DONE))
		panic("biodone already");
	SET(bp->b_flags, B_DONE);		/* note that it's done */

	if (bp->b_bq)
		bufq_done(bp->b_bq, bp);

	if (LIST_FIRST(&bp->b_dep) != NULL)
		buf_complete(bp);

	if (!ISSET(bp->b_flags, B_READ)) {
		CLR(bp->b_flags, B_WRITEINPROG);
		vwakeup(bp->b_vp);
	}
	if (bcstats.numbufs &&
	    (!(ISSET(bp->b_flags, B_RAW) || ISSET(bp->b_flags, B_PHYS)))) {
		if (!ISSET(bp->b_flags, B_READ))
			bcstats.pendingwrites--;
		else
			bcstats.pendingreads--;
	}
	if (ISSET(bp->b_flags, B_CALL)) {	/* if necessary, call out */
		CLR(bp->b_flags, B_CALL);	/* but note callout done */
		(*bp->b_iodone)(bp);
	} else {
		if (ISSET(bp->b_flags, B_ASYNC)) {/* if async, release it */
			brelse(bp);
		} else {			/* or just wakeup the buffer */
			CLR(bp->b_flags, B_WANTED);
			wakeup(bp);
		}
	}
}

#ifdef DDB
void	bcstats_print(int (*)(const char *, ...)
    __attribute__((__format__(__kprintf__,1,2))));
/*
 * bcstats_print: ddb hook to print interesting buffer cache counters
 */
void
bcstats_print(
    int (*pr)(const char *, ...) __attribute__((__format__(__kprintf__,1,2))))
{
	(*pr)("Current Buffer Cache status:\n");
	(*pr)("numbufs %lld busymapped %lld, delwri %lld\n",
	    bcstats.numbufs, bcstats.busymapped, bcstats.delwribufs);
	(*pr)("kvaslots %lld avail kva slots %lld\n",
	    bcstats.kvaslots, bcstats.kvaslots_avail);
    	(*pr)("bufpages %lld, dirtypages %lld\n",
	    bcstats.numbufpages,  bcstats.numdirtypages);
	(*pr)("pendingreads %lld, pendingwrites %lld\n",
	    bcstats.pendingreads, bcstats.pendingwrites);
}
#endif

/* bufcache freelist code below */
/*
 * Copyright (c) 2014 Ted Unangst <tedu@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * The code below implements a variant of the 2Q buffer cache algorithm by
 * Johnson and Shasha.
 *
 * General Outline
 * We divide the buffer cache into three working sets: current, previous,
 * and long term. Each list is itself LRU and buffers get promoted and moved
 * around between them. A buffer starts its life in the current working set.
 * As time passes and newer buffers push it out, it will turn into the previous
 * working set and is subject to recycling. But if it's accessed again from
 * the previous working set, that's an indication that it's actually in the
 * long term working set, so we promote it there. The separation of current
 * and previous working sets prevents us from promoting a buffer that's only
 * temporarily hot to the long term cache.
 *
 * The objective is to provide scan resistance by making the long term
 * working set ineligible for immediate recycling, even as the current 
 * working set is rapidly turned over.
 *
 * Implementation
 * The code below identifies the current, previous, and long term sets as
 * hotqueue, coldqueue, and warmqueue. The hot and warm queues are capped at
 * 1/3 of the total clean pages, after which point they start pushing their
 * oldest buffers into coldqueue.
 * A buf always starts out with neither WARM or COLD flags set (implying HOT).
 * When released, it will be returned to the tail of the hotqueue list.
 * When the hotqueue gets too large, the oldest hot buf will be moved to the
 * coldqueue, with the B_COLD flag set. When a cold buf is released, we set
 * the B_WARM flag and put it onto the warmqueue. Warm bufs are also
 * directly returned to the end of the warmqueue. As with the hotqueue, when
 * the warmqueue grows too large, bufs are moved onto the coldqueue.
 *
 * Note that this design does still support large working sets, greater
 * than the cap of hotqueue or warmqueue would imply. The coldqueue is still
 * cached and has no maximum length. The hot and warm queues form a Y feeding
 * into the coldqueue. Moving bufs between queues is constant time, so this
 * design decays to one long warm->cold queue.
 *
 * In the 2Q paper, hotqueue and coldqueue are A1in and A1out. The warmqueue
 * is Am. We always cache pages, as opposed to pointers to pages for A1.
 * 
 */

/*
 * 
 */
TAILQ_HEAD(bufqueue, buf);
struct bufqueue hotqueue;
int64_t hotbufpages;
struct bufqueue coldqueue;
struct bufqueue warmqueue;
int64_t warmbufpages;
struct bufqueue dirtyqueue;

/*
 * this function is called when a hot or warm queue may have exceeded its
 * size limit. it will move a buf to the coldqueue.
 */
int chillbufs(struct bufqueue *queue, int64_t *queuepages);

void
bufcache_init(void)
{

	TAILQ_INIT(&hotqueue);
	TAILQ_INIT(&coldqueue);
	TAILQ_INIT(&warmqueue);
	TAILQ_INIT(&dirtyqueue);
}

/*
 * if the buffer cache shrunk, we may need to rebalance our queues.
 */
void
bufcache_adjust(void)
{
	while (chillbufs(&warmqueue, &warmbufpages) ||
	    chillbufs(&hotqueue, &hotbufpages))
		;
}

struct buf *
bufcache_getcleanbuf(void)
{
	struct buf *bp;

	if ((bp = TAILQ_FIRST(&coldqueue)))
		return bp;
	if ((bp = TAILQ_FIRST(&warmqueue)))
		return bp;
	return TAILQ_FIRST(&hotqueue);
}

struct buf *
bufcache_getdirtybuf(void)
{
	return TAILQ_FIRST(&dirtyqueue);
}

void
bufcache_take(struct buf *bp)
{
	struct bufqueue *queue;
	int64_t pages;

	splassert(IPL_BIO);

	pages = atop(bp->b_bufsize);
	if (!ISSET(bp->b_flags, B_DELWRI)) {
		if (ISSET(bp->b_flags, B_WARM)) {
			queue = &warmqueue;
			warmbufpages -= pages;
		} else if (ISSET(bp->b_flags, B_COLD)) {
			queue = &coldqueue;
		} else {
			queue = &hotqueue;
			hotbufpages -= pages;
		}
		bcstats.numcleanpages -= pages;
	} else {
		queue = &dirtyqueue;
		bcstats.numdirtypages -= pages;
		bcstats.delwribufs--;
	}
	TAILQ_REMOVE(queue, bp, b_freelist);
}

int
chillbufs(struct bufqueue *queue, int64_t *queuepages)
{
	struct buf *bp;
	int64_t limit, pages;

	/*
	 * The warm and hot queues are allowed to be up to one third each.
	 * We impose a minimum size of 96 to prevent too much "wobbling".
	 */
	limit = bcstats.numcleanpages / 3;
	if (*queuepages > 96 && *queuepages > limit) {
		bp = TAILQ_FIRST(queue);
		if (!bp)
			panic("inconsistent bufpage counts");
		pages = atop(bp->b_bufsize);
		*queuepages -= pages;
		TAILQ_REMOVE(queue, bp, b_freelist);
		CLR(bp->b_flags, B_WARM);
		SET(bp->b_flags, B_COLD);
		TAILQ_INSERT_TAIL(&coldqueue, bp, b_freelist);
		return 1;
	}
	return 0;
}

void
bufcache_release(struct buf *bp)
{
	struct bufqueue *queue;
	int64_t pages;
	
	pages = atop(bp->b_bufsize);
	if (!ISSET(bp->b_flags, B_DELWRI)) {
		int64_t *queuepages;
		if (ISSET(bp->b_flags, B_WARM | B_COLD)) {
			SET(bp->b_flags, B_WARM);
			queue = &warmqueue;
			queuepages = &warmbufpages;
		} else {
			queue = &hotqueue;
			queuepages = &hotbufpages;
		}
		*queuepages += pages;
		bcstats.numcleanpages += pages;
		chillbufs(queue, queuepages);
	} else {
		queue = &dirtyqueue;
		bcstats.numdirtypages += pages;
		bcstats.delwribufs++;
	}
	TAILQ_INSERT_TAIL(queue, bp, b_freelist);
}

#ifdef HIBERNATE
/*
 * Flush buffercache to lowest value on hibernate suspend
 */
void
hibernate_suspend_bufcache(void)
{
	long save_buflowpages = buflowpages;

	/* Shrink buffercache to 16MB (4096 pages) */
	buflowpages = 4096;
	bufadjust(buflowpages);
	buflowpages = save_buflowpages;
	bufhighpages = bufpages;
}

void
hibernate_resume_bufcache(void)
{
	uint64_t dmapages, pgs;

	dmapages = uvm_pagecount(&dma_constraint);
	pgs = bufcachepercent * dmapages / 100;
	bufadjust(pgs);
	bufhighpages = bufpages;
}
#endif /* HIBERNATE */

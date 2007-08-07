/*	$OpenBSD: vfs_bio.c,v 1.99 2007/08/07 04:32:45 beck Exp $	*/
/*	$NetBSD: vfs_bio.c,v 1.44 1996/06/11 11:15:36 pk Exp $	*/

/*-
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

#include <uvm/uvm_extern.h>

#include <miscfs/specfs/specdev.h>

/*
 * Definitions for the buffer hash lists.
 */
#define	BUFHASH(dvp, lbn)	\
	(&bufhashtbl[((long)(dvp) / sizeof(*(dvp)) + (int)(lbn)) & bufhash])
LIST_HEAD(bufhashhdr, buf) *bufhashtbl, invalhash;
u_long	bufhash;

/*
 * Insq/Remq for the buffer hash lists.
 */
#define	binshash(bp, dp)	LIST_INSERT_HEAD(dp, bp, b_hash)
#define	bremhash(bp)		LIST_REMOVE(bp, b_hash)

/*
 * Definitions for the buffer free lists.
 */
#define	BQUEUES		6		/* number of free buffer queues */

#define	BQ_DIRTY	0		/* LRU queue with dirty buffers */


TAILQ_HEAD(bqueues, buf) bufqueues[BQUEUES];
int bqpages[BQUEUES];		/* pages allocated, per queue */
int bqpagelow;
int needbuffer;
struct bio_ops bioops;

/*
 * Buffer pool for I/O buffers.
 */
struct pool bufpool;
struct vm_map *buf_map;
struct bufhead bufhead = LIST_HEAD_INITIALIZER(bufhead);
struct buf *buf_get(size_t);
struct buf *buf_stub(struct vnode *, daddr64_t);
void buf_put(struct buf *);

/*
 * Insq/Remq for the buffer free lists.
 */
#define	binsheadfree(bp, dp)	TAILQ_INSERT_HEAD(dp, bp, b_freelist)
#define	binstailfree(bp, dp)	TAILQ_INSERT_TAIL(dp, bp, b_freelist)

struct buf *bio_doread(struct vnode *, daddr64_t, int, int);
struct buf *getnewbuf(size_t, int, int, int *);
void buf_init(struct buf *, int);
void bread_cluster_callback(struct buf *);

/*
 * We keep a few counters to monitor the utilization of the buffer cache
 *
 *  numbufpages   - number of pages totally allocated.
 *  numdirtypages - number of pages on BQ_DIRTY queue.
 *  lodirtypages  - low water mark for buffer cleaning daemon.
 *  hidirtypages  - high water mark for buffer cleaning daemon.
 *  numfreepages  - number of pages on BQ_CLEAN and BQ_DIRTY queues. unused.
 *  numcleanpages - number of pages on BQ_CLEAN queue.
 *		    Used to track the need to speedup the cleaner and 
 *		    as a reserve for special processes like syncer.
 *  maxcleanpages - the highest page count on BQ_CLEAN.
 */
long numbufpages;
long numdirtypages;
long lodirtypages;
long hidirtypages;
long numfreepages;
long numcleanpages;
long locleanpages;
long hicleanpages;
long maxcleanpages;

struct proc *cleanerproc;
int bd_req;			/* Sleep point for cleaner daemon. */

int size2cqueue(int *size);

int
size2cqueue(int *size)
{
	int i = 0, q;
	int s = *size;
	s -= 1;
	while (s > 0) {
		s = s >> 1;
		i++;
	}
	if (i < PAGE_SHIFT) {
		i = PAGE_SHIFT; /* < 4096 -> 4096 */
        }
	*size = 1 << i;
	q = (i + 1 - PAGE_SHIFT); /* XXX 4096 is queue 1 */
	if (q >= BQUEUES)
		panic("queue %d > BQUEUES %d", q, BQUEUES);
	if (q == 0)
		panic("can't return dirty q");
	return(q);
}

void
bremfree(struct buf *bp)
{
	struct bqueues *dp = NULL;
	int queue;

	/*
	 * We only calculate the head of the freelist when removing
	 * the last element of the list as that is the only time that
	 * it is needed (e.g. to reset the tail pointer).
	 *
	 * NB: This makes an assumption about how tailq's are implemented.
	 */
	if (TAILQ_NEXT(bp, b_freelist) == NULL) {
		for (dp = bufqueues; dp < &bufqueues[BQUEUES]; dp++)
			if (dp->tqh_last == &TAILQ_NEXT(bp, b_freelist))
				break;
		if (dp == &bufqueues[BQUEUES])
			panic("bremfree: lost tail");
	}
	numfreepages -= btoc(bp->b_bufsize);
	if (!ISSET(bp->b_flags, B_DELWRI)) {
		int qs = bp->b_bufsize;
		queue = size2cqueue(&qs);
		numcleanpages -= btoc(bp->b_bufsize);
		bqpages[queue] -= btoc(bp->b_bufsize);
	} else
		numdirtypages -= btoc(bp->b_bufsize);
	TAILQ_REMOVE(dp, bp, b_freelist);
}

void
buf_init(struct buf *bp, int size)
{
	int npages, queue;

	splassert(IPL_BIO);

	npages = btoc(size);
	bzero((char *)bp, sizeof *bp);
	bp->b_vnbufs.le_next = NOLIST;
	bp->b_freelist.tqe_next = NOLIST;
	bp->b_synctime = time_uptime + 300;
	bp->b_dev = NODEV;
	queue = size2cqueue(&size);
	LIST_INIT(&bp->b_dep);
	numbufpages += npages;
	numfreepages += npages;
	numcleanpages += npages;
	bqpages[queue] += npages;
	if (maxcleanpages < numcleanpages)
		maxcleanpages = numcleanpages;
}

/*
 * This is a non-sleeping expanded equivalent of getblk() that allocates only
 * the buffer structure, and not its contents.
 */
struct buf *
buf_stub(struct vnode *vp, daddr64_t lblkno)
{
	struct buf *bp;
	int s;

	s = splbio();
	bp = pool_get(&bufpool, PR_NOWAIT);
	splx(s);

	if (bp == NULL)
		return (NULL);

	bzero((char *)bp, sizeof *bp);
	bp->b_vnbufs.le_next = NOLIST;
	bp->b_freelist.tqe_next = NOLIST;
	bp->b_synctime = time_uptime + 300;
	bp->b_dev = NODEV;
	bp->b_bufsize = 0;
	bp->b_data = NULL;
	bp->b_flags = B_BUSY;
	bp->b_dev = NODEV;
	bp->b_blkno = bp->b_lblkno = lblkno;
	bp->b_iodone = NULL;
	bp->b_error = 0;
	bp->b_resid = 0;
	bp->b_bcount = 0;
	bp->b_dirtyoff = bp->b_dirtyend = 0;
	bp->b_validoff = bp->b_validend = 0;

	LIST_INIT(&bp->b_dep);

	s = splbio();
	LIST_INSERT_HEAD(&bufhead, bp, b_list);
	bgetvp(vp, bp);
	splx(s);

	return (bp);
}

struct buf *
buf_get(size_t size)
{
	struct bqueues *dp;
	struct buf *bp;
	int npages;
	int queue, qs;
	void *data;

	splassert(IPL_BIO);

	KASSERT(size > 0);

	size = round_page(size);
	qs = size;
	queue = size2cqueue(&qs);
	npages = btoc(qs);

	if (numbufpages + npages > bufpages)
		return (NULL);

	bp = pool_get(&bufpool, PR_WAITOK);

	data = (void *)uvm_km_alloc(buf_map, qs);
	if (data == NULL) {
		pool_put(&bufpool, bp);
		return (NULL);
	}
	buf_init(bp, qs);
	bp->b_flags = B_INVAL;
	bp->b_bufsize = qs;
	bp->b_data = data;
	dp = &bufqueues[queue];
	binsheadfree(bp, dp);
	binshash(bp, &invalhash);
	LIST_INSERT_HEAD(&bufhead, bp, b_list);

	return (bp);
}

void
buf_put(struct buf *bp)
{
	splassert(IPL_BIO);
#ifdef DIAGNOSTIC
	if (bp->b_data != NULL)
		KASSERT(bp->b_bufsize > 0);
#endif
#ifdef QUEUE_MACRO_DEBUG
	if (bp->b_freelist.tqe_next != NOLIST &&
	    bp->b_freelist.tqe_next != (void *)-1)
		panic("buf_put: still on the free list");

	if (bp->b_vnbufs.le_next != NOLIST &&
	    bp->b_vnbufs.le_next != (void *)-1)
		panic("buf_put: still on the vnode list");
#endif
#ifdef DIAGNOSTIC
	if (!LIST_EMPTY(&bp->b_dep))
		panic("buf_put: b_dep is not empty");
#endif
	LIST_REMOVE(bp, b_list);

	if (bp->b_data != NULL) {
		bremhash(bp);
		numbufpages -= btoc(bp->b_bufsize);
		uvm_km_free(buf_map, (vaddr_t)bp->b_data, bp->b_bufsize);
	}

	pool_put(&bufpool, bp);
}

/*
 * Initialize buffers and hash links for buffers.
 */
void
bufinit(void)
{
	vaddr_t minaddr, maxaddr;
	struct bqueues *dp;

	pool_init(&bufpool, sizeof(struct buf), 0, 0, 0, "bufpl", NULL);
	pool_setipl(&bufpool, IPL_BIO);
	for (dp = bufqueues; dp < &bufqueues[BQUEUES]; dp++)
		TAILQ_INIT(dp);
	minaddr = vm_map_min(kernel_map);
	buf_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    ptoa(bufpages), 0, FALSE, NULL);

	/* 
	 * XXX don't starve any one queue below 5% of the total number
	 * of buffer cache pages.
	 */
	bqpagelow = bufpages / 20; 

	bufhashtbl = hashinit(bufpages / 4, M_CACHE, M_WAITOK, &bufhash);
	hidirtypages = (bufpages / 4) * 3;
	lodirtypages = bufpages / 2;

	/*
	 * Reserve 5% of bufpages for syncer's needs,
	 * but not more than 25% and if possible
	 * not less than 2 * MAXBSIZE. locleanpages
	 * value must be not too small
	 */
	hicleanpages = bufpages / 2;
	locleanpages = hicleanpages / 2;
	if (locleanpages < btoc(2 * MAXBSIZE))
		locleanpages = btoc(2 * MAXBSIZE);
	if (locleanpages > bufpages / 4)
		locleanpages = bufpages / 4;

	maxcleanpages = locleanpages;
}

struct buf *
bio_doread(struct vnode *vp, daddr64_t blkno, int size, int async)
{
	struct buf *bp;

	bp = getblk(vp, blkno, size, 0, 0);

	/*
	 * If buffer does not have valid data, start a read.
	 * Note that if buffer is B_INVAL, getblk() won't return it.
	 * Therefore, it's valid if its I/O has completed or been delayed.
	 */
	if (!ISSET(bp->b_flags, (B_DONE | B_DELWRI))) {
		SET(bp->b_flags, B_READ | async);
		VOP_STRATEGY(bp);

		/* Pay for the read. */
		curproc->p_stats->p_ru.ru_inblock++;		/* XXX */
	} else if (async) {
		brelse(bp);
	}

	return (bp);
}

/*
 * Read a disk block.
 * This algorithm described in Bach (p.54).
 */
int
bread(struct vnode *vp, daddr64_t blkno, int size, struct ucred *cred,
    struct buf **bpp)
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
breadn(struct vnode *vp, daddr64_t blkno, int size, daddr64_t rablks[],
    int rasizes[], int nrablks, struct ucred *cred, struct buf **bpp)
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
	int i;
	struct buf **xbpp;

	xbpp = (struct buf **)bp->b_saveaddr;

	for (i = 0; xbpp[i] != 0; i++) {
		if (ISSET(bp->b_flags, B_ERROR))
			SET(xbpp[i]->b_flags, B_INVAL | B_ERROR);
		biodone(xbpp[i]);
	}

	free(xbpp, M_TEMP);
	bp->b_data = NULL;
	buf_put(bp);
}

int
bread_cluster(struct vnode *vp, daddr64_t blkno, int size, struct buf **rbpp)
{
	struct buf *bp, **xbpp;
	int howmany, i, maxra, inc;
	daddr64_t sblkno;
	size_t spill;

	*rbpp = bio_doread(vp, blkno, size, 0);

	if (size != round_page(size))
		return (biowait(*rbpp));

	if (VOP_BMAP(vp, blkno + 1, NULL, &sblkno, &maxra))
		return (biowait(*rbpp));

	maxra++; 
	if (sblkno == -1 || maxra < 2)
		return (biowait(*rbpp));

	howmany = MAXPHYS / size;
	if (howmany > maxra)
		howmany = maxra;

	xbpp = malloc((howmany + 1) * sizeof(struct buf *), M_TEMP, M_NOWAIT);
	if (xbpp == NULL)
		return (biowait(*rbpp));

	for (i = 0; i < howmany; i++) {
		if (incore(vp, blkno + i + 1)) {
			for (--i; i >= 0; i--) {
				SET(xbpp[i]->b_flags, B_INVAL);
				brelse(xbpp[i]);
			}
			free(xbpp, M_TEMP);
			return (biowait(*rbpp));
		}
		xbpp[i] = buf_stub(vp, blkno + i + 1);
		if (xbpp[i] == NULL) {
			for (--i; i >= 0; i--) {
				SET(xbpp[i]->b_flags, B_INVAL);
				brelse(xbpp[i]);
			}
			free(xbpp, M_TEMP);
			return (biowait(*rbpp));
		}
	}

	xbpp[howmany] = 0;

	bp = getnewbuf(howmany * size, 0, 0, NULL);
	if (bp == NULL) {
		for (i = 0; i < howmany; i++) {
			SET(xbpp[i]->b_flags, B_INVAL);
			brelse(xbpp[i]);
		}
		free(xbpp, M_TEMP);
		return (biowait(*rbpp));
	}

	inc = btodb(size);

	for (i = 0; i < howmany; i++) {
		SET(xbpp[i]->b_flags, B_READ | B_ASYNC);
		binshash(xbpp[i], BUFHASH(vp, xbpp[i]->b_lblkno));
		xbpp[i]->b_blkno = sblkno + (i * inc);
		xbpp[i]->b_bufsize = xbpp[i]->b_bcount = size;
		xbpp[i]->b_data = bp->b_data + (i * size);
	}

	bp->b_blkno = sblkno;
	bp->b_lblkno = blkno + 1;
	SET(bp->b_flags, B_READ | B_ASYNC | B_CALL);
	bp->b_saveaddr = (void *)xbpp;
	bp->b_iodone = bread_cluster_callback;
	bp->b_vp = vp;
	spill = bp->b_bufsize - bp->b_bcount;
	if (spill) {
		uvm_km_free(buf_map, (vaddr_t) bp->b_data + bp->b_bcount,
		    spill);
		numbufpages -= atop(spill);
	}
	VOP_STRATEGY(bp);
	curproc->p_stats->p_ru.ru_inblock++;

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
		curproc->p_stats->p_ru.ru_oublock++;
	

	/* Initiate disk write.  Make sure the appropriate party is charged. */
	bp->b_vp->v_numoutput++;
	splx(s);
	SET(bp->b_flags, B_WRITEINPROG);
	VOP_STRATEGY(bp);

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
		bp->b_synctime = time_uptime + 35;
		s = splbio();
		reassignbuf(bp);
		splx(s);
		curproc->p_stats->p_ru.ru_oublock++;	/* XXX */
	} else {
		/*
		 * see if this buffer has slacked through the syncer
		 * and enforce an async write upon it.
		 */
		if (bp->b_synctime < time_uptime) {
			bawrite(bp);
			return;
		}
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

	if (ISSET(bp->b_flags, B_DELWRI) == 0) {
		SET(bp->b_flags, B_DELWRI);
		bp->b_synctime = time_uptime + 35;
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
	struct bqueues *bufq;
	int s;

	/* Block disk interrupts. */
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
		int queue, qs;

		/*
		 * If the buffer is invalid, place it in the clean queue, so it
		 * can be reused.
		 */
		if (LIST_FIRST(&bp->b_dep) != NULL)
			buf_deallocate(bp);

		if (ISSET(bp->b_flags, B_DELWRI)) {
			CLR(bp->b_flags, B_DELWRI);
		}

		if (bp->b_vp)
			brelvp(bp);

		/*
		 * If the buffer has no associated data, place it back in the
		 * pool.
		 */
		if (bp->b_data == NULL) {
			buf_put(bp);
			splx(s);
			return;
		}

		qs = bp->b_bufsize;
		queue = size2cqueue(&qs);
		numcleanpages += btoc(bp->b_bufsize);
		bqpages[queue] += btoc(bp->b_bufsize);
		if (maxcleanpages < numcleanpages)
			maxcleanpages = numcleanpages;
		binsheadfree(bp, &bufqueues[queue]);
	} else {
		/*
		 * It has valid data.  Put it on the end of the appropriate
		 * queue, so that it'll stick around for as long as possible.
		 */
		int queue, qs;
		numfreepages += btoc(bp->b_bufsize);
		qs = bp->b_bufsize;
		queue = size2cqueue(&qs);

		if (!ISSET(bp->b_flags, B_DELWRI)) {
			numcleanpages += btoc(bp->b_bufsize);
			bqpages[queue] += btoc(bp->b_bufsize);
			if (maxcleanpages < numcleanpages)
				maxcleanpages = numcleanpages;
			bufq = &bufqueues[queue];
		} else {
			numdirtypages += btoc(bp->b_bufsize);
			bufq = &bufqueues[BQ_DIRTY];
		}
		if (ISSET(bp->b_flags, B_AGE)) {
			binsheadfree(bp, bufq);
			bp->b_synctime = time_uptime + 30;
		} else {
			binstailfree(bp, bufq);
			bp->b_synctime = time_uptime + 300;
		}
	}

	/* Unlock the buffer. */
	CLR(bp->b_flags, (B_AGE | B_ASYNC | B_BUSY | B_NOCACHE | B_DEFERRED));

	/* Wake up any processes waiting for any buffer to become free. */
	if (needbuffer) {
		needbuffer--;
		wakeup_one(&needbuffer);
	}

	/* Wake up any processes waiting for _this_ buffer to become free. */
	if (ISSET(bp->b_flags, B_WANTED)) {
		CLR(bp->b_flags, B_WANTED);
		wakeup(bp);
	}

	splx(s);
}

/*
 * Determine if a block is in the cache. Just look on what would be its hash
 * chain. If it's there, return a pointer to it, unless it's marked invalid.
 */
struct buf *
incore(struct vnode *vp, daddr64_t blkno)
{
	struct buf *bp;

	/* Search hash chain */
	LIST_FOREACH(bp, BUFHASH(vp, blkno), b_hash) {
		if (bp->b_lblkno == blkno && bp->b_vp == vp &&
		    !ISSET(bp->b_flags, B_INVAL))
			return (bp);
	}

	return (NULL);
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
getblk(struct vnode *vp, daddr64_t blkno, int size, int slpflag, int slptimeo)
{
	struct bufhashhdr *bh;
	struct buf *bp, *nb = NULL;
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
	bh = BUFHASH(vp, blkno);
start:
	LIST_FOREACH(bp, BUFHASH(vp, blkno), b_hash) {
		if (bp->b_lblkno != blkno || bp->b_vp != vp)
			continue;

		s = splbio();
		if (ISSET(bp->b_flags, B_BUSY)) {
			if (nb != NULL) {
				SET(nb->b_flags, B_INVAL);
				binshash(nb, &invalhash);
				brelse(nb);
				nb = NULL;
			}
			SET(bp->b_flags, B_WANTED);
			error = tsleep(bp, slpflag | (PRIBIO + 1), "getblk",
			    slptimeo);
			splx(s);
			if (error)
				return (NULL);
			goto start;
		}

		if (!ISSET(bp->b_flags, B_INVAL)) {
			SET(bp->b_flags, (B_BUSY | B_CACHE));
			bremfree(bp);
			splx(s);
			break;
		}
		splx(s);
	}
	if (nb && bp) {
		SET(nb->b_flags, B_INVAL);
		binshash(nb, &invalhash);
		brelse(nb);
		nb = NULL;
	}
	if (bp == NULL && nb == NULL) {
		nb = getnewbuf(size, slpflag, slptimeo, &error);
		if (nb == NULL) {
			if (error == ERESTART || error == EINTR)
				return (NULL);
		}
		goto start;
	}
	if (nb) {
		bp = nb;
		binshash(bp, bh);
		bp->b_blkno = bp->b_lblkno = blkno;
		s = splbio();
		bgetvp(vp, bp);
		splx(s);
	}
	return (bp);
}

/*
 * Get an empty, disassociated buffer of given size.
 */
struct buf *
geteblk(int size)
{
	struct buf *bp;

	while ((bp = getnewbuf(size, 0, 0, NULL)) == NULL)
		;
	SET(bp->b_flags, B_INVAL);
	binshash(bp, &invalhash);

	return (bp);
}

/*
 * Find a buffer which is available for use.
 */
struct buf *
getnewbuf(size_t size, int slpflag, int slptimeo, int *ep)
{
	struct buf *bp;
	int s, error, queue, qs;

#if 0		/* we would really like this but sblock update kills it */
	KASSERT(curproc != syncerproc && curproc != cleanerproc);
#endif

	s = splbio();
	/*
	 * Wake up cleaner if we're getting low on pages.
	 */
	if (numdirtypages >= hidirtypages || numcleanpages <= locleanpages)
		wakeup(&bd_req);

	/* we just ask. it can say no.. */
getsome:
	qs = size;
	queue = size2cqueue(&qs);
	bp = buf_get(qs); /* XXX use qs instead and no need in buf_get? */
	if (bp == NULL) {
		/*
		 * No free ones, try to reuse a clean one of the same or
		 * larger size.
		 */
		do {
			bp = TAILQ_FIRST(&bufqueues[queue]);
			queue++;
		} while (bp == NULL && queue < BQUEUES);
	}
	if (bp == NULL) {
		/* we couldn't reuse a free one, nothing of the right size */
		/* XXX free 20 buffers per q - ugly hack  should really
		 * reuse big ones without truncating. fix later 
		 */
		int q, gotsome = 0;
		int freemax = 20; 
		for (q = 1; q < BQUEUES; q++) {
			int i = freemax;
			while (bqpages[q] > bqpagelow
			    && (bp = TAILQ_FIRST(&bufqueues[q]))
			    && i--) {
				gotsome++;
				bremfree(bp);
				if (LIST_FIRST(&bp->b_dep) != NULL)
					buf_deallocate(bp);

				if (ISSET(bp->b_flags, B_DELWRI)) {
					CLR(bp->b_flags, B_DELWRI);
				}

				if (bp->b_vp)
					brelvp(bp);

				buf_put(bp);
			}
		}
		if (gotsome)
			goto getsome;
	}
	if (bp == NULL) {
		/* wait for a free buffer of any kind */
		needbuffer++;
		error = tsleep(&needbuffer, slpflag | (PRIBIO + 1),
		    "getnewbuf", slptimeo);
		if (ep != NULL) {
			*ep = error;
			if (error) {
				splx(s);
				return (NULL);
			}
		}
		goto getsome;
	}

	bremfree(bp);
	/* Buffer is no longer on free lists. */
	SET(bp->b_flags, B_BUSY);

#ifdef DIAGNOSTIC
	if (ISSET(bp->b_flags, B_DELWRI))
		panic("Dirty buffer on BQ_CLEAN");
#endif

	/* disassociate us from our vnode, if we had one... */
	if (bp->b_vp)
		brelvp(bp);

	splx(s);

#ifdef DIAGNOSTIC
	/* CLEAN buffers must have no dependencies */ 
	if (LIST_FIRST(&bp->b_dep) != NULL)
		panic("BQ_CLEAN has buffer with dependencies");
#endif

	/* clear out various other fields */
	bp->b_flags = B_BUSY;
	bp->b_dev = NODEV;
	bp->b_blkno = bp->b_lblkno = 0;
	bp->b_iodone = NULL;
	bp->b_error = 0;
	bp->b_resid = 0;
	bp->b_bcount = size;
	bp->b_dirtyoff = bp->b_dirtyend = 0;
	bp->b_validoff = bp->b_validend = 0;

	bremhash(bp);
	return (bp);
}

/*
 * Buffer cleaning daemon.
 */
void
buf_daemon(struct proc *p)
{
	struct timeval starttime, timediff;
	struct buf *bp;
	int s;

	cleanerproc = curproc;

	s = splbio();
	for (;;) {
		if (!numdirtypages ||
		    (numdirtypages < hidirtypages && !needbuffer))
			tsleep(&bd_req, PRIBIO - 7, "cleaner", 0);

		getmicrouptime(&starttime);

		while ((bp = TAILQ_FIRST(&bufqueues[BQ_DIRTY]))) {
			struct timeval tv;

			if (numdirtypages < lodirtypages && !needbuffer)
				break;

			bremfree(bp);
			SET(bp->b_flags, B_BUSY);
			splx(s);

			if (ISSET(bp->b_flags, B_INVAL)) {
				brelse(bp);
				s = splbio();
				continue;
			}
#ifdef DIAGNOSTIC
			if (!ISSET(bp->b_flags, B_DELWRI))
				panic("Clean buffer on BQ_DIRTY");
#endif
			if (LIST_FIRST(&bp->b_dep) != NULL &&
			    !ISSET(bp->b_flags, B_DEFERRED) &&
			    buf_countdeps(bp, 0, 0)) {
				SET(bp->b_flags, B_DEFERRED);
				s = splbio();
				numfreepages += btoc(bp->b_bufsize);
				numdirtypages += btoc(bp->b_bufsize);
				binstailfree(bp, &bufqueues[BQ_DIRTY]);
				CLR(bp->b_flags, B_BUSY);
				continue;
			}

			bawrite(bp);

			/* Never allow processing to run for more than 1 sec */
			getmicrouptime(&tv);
			timersub(&tv, &starttime, &timediff);
			if (timediff.tv_sec)
				break;

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

	if (LIST_FIRST(&bp->b_dep) != NULL)
		buf_complete(bp);

	if (!ISSET(bp->b_flags, B_READ)) {
		CLR(bp->b_flags, B_WRITEINPROG);
		vwakeup(bp->b_vp);
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

#if 1
void
vfs_bufstats(void) {
	return;
}
/* #ifdef DDB */
#else
/*
 * Print out statistics on the current allocation of the buffer pool.
 * Can be enabled to print out on every ``sync'' by setting "syncprt"
 * in vfs_syscalls.c using sysctl.
 */
void
vfs_bufstats(void)
{
	int s, i, j, count;
	struct buf *bp;
	struct bqueues *dp;
	int counts[MAXBSIZE/PAGE_SIZE+1];
	int totals[BQUEUES];
	long ptotals[BQUEUES];
	long pages;
	static char *bname[BQUEUES] = { "CLEAN", "DIRTY", "EMPTY" };

	s = splbio();
	for (dp = bufqueues, i = 0; dp < &bufqueues[BQUEUES]; dp++, i++) {
		count = 0;
		pages = 0;
		for (j = 0; j <= MAXBSIZE/PAGE_SIZE; j++)
			counts[j] = 0;
		TAILQ_FOREACH(bp, dp, b_freelist) {
			counts[bp->b_bufsize/PAGE_SIZE]++;
			count++;
			pages += btoc(bp->b_bufsize);
		}
		totals[i] = count;
		ptotals[i] = pages;
		printf("%s: total-%d(%d pages)", bname[i], count, pages);
		for (j = 0; j <= MAXBSIZE/PAGE_SIZE; j++)
			if (counts[j] != 0)
				printf(", %d-%d", j * PAGE_SIZE, counts[j]);
		printf("\n");
	}
	if ((ptotals[BQ_CLEAN] + ptotals[BQ_DIRTY]) != numfreepages)
		printf("numfreepages counter wrong: %ld != %ld\n",
		    numfreepages, ptotals[BQ_CLEAN] + ptotals[BQ_DIRTY]);
	if (ptotals[BQ_CLEAN] != numcleanpages)
		printf("numcleanpages counter wrong: %ld != %ld\n",
		    numcleanpages, ptotals[<BQ_CLEAN]);
	else
		printf("numcleanpages: %ld\n", numcleanpages);
	if (numdirtypages != ptotals[BQ_DIRTY])
		printf("numdirtypages counter wrong: %ld != %ld\n",
		    numdirtypages, ptotals[BQ_DIRTY]);
	else
		printf("numdirtypages: %ld\n", numdirtypages);

	printf("syncer eating up to %ld pages from %ld reserved\n",
	    maxcleanpages - hicleanpages, locleanpages);
	splx(s);
}
#endif /* DEBUG */

/*	$OpenBSD: vfs_bio.c,v 1.71 2004/11/01 15:55:38 pedro Exp $	*/
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
#define	BQUEUES		4		/* number of free buffer queues */

#define	BQ_LOCKED	0		/* super-blocks &c */
#define	BQ_CLEAN	1		/* LRU queue with clean buffers */
#define	BQ_DIRTY	2		/* LRU queue with dirty buffers */
#define	BQ_EMPTY	3		/* buffer headers with no memory */

TAILQ_HEAD(bqueues, buf) bufqueues[BQUEUES];
int needbuffer;
int nobuffers;
struct bio_ops bioops;

/*
 * Buffer pool for I/O buffers.
 */
struct pool bufpool;

/*
 * Insq/Remq for the buffer free lists.
 */
#define	binsheadfree(bp, dp)	TAILQ_INSERT_HEAD(dp, bp, b_freelist)
#define	binstailfree(bp, dp)	TAILQ_INSERT_TAIL(dp, bp, b_freelist)

static __inline struct buf *bio_doread(struct vnode *, daddr_t, int, int);
int getnewbuf(int slpflag, int slptimeo, struct buf **);

/*
 * We keep a few counters to monitor the utilization of the buffer cache
 *
 *  numdirtypages - number of pages on BQ_DIRTY queue.
 *  lodirtypages  - low water mark for buffer cleaning daemon.
 *  hidirtypages  - high water mark for buffer cleaning daemon.
 *  numfreepages  - number of pages on BQ_CLEAN and BQ_DIRTY queues. unused.
 *  numcleanpages - number of pages on BQ_CLEAN queue.
 *		    Used to track the need to speedup the cleaner and 
 *		    as a reserve for special processes like syncer.
 *  mincleanpages - the lowest byte count on BQ_CLEAN.
 *  numemptybufs  - number of buffers on BQ_EMPTY. unused.
 */
long numdirtypages;
long lodirtypages;
long hidirtypages;
long numfreepages;
long numcleanpages;
long locleanpages;
int numemptybufs;
#ifdef DEBUG
long mincleanpages;
#endif

struct proc *cleanerproc;
int bd_req;			/* Sleep point for cleaner daemon. */

void
bremfree(struct buf *bp)
{
	struct bqueues *dp = NULL;

	/*
	 * We only calculate the head of the freelist when removing
	 * the last element of the list as that is the only time that
	 * it is needed (e.g. to reset the tail pointer).
	 *
	 * NB: This makes an assumption about how tailq's are implemented.
	 */
	if (bp->b_freelist.tqe_next == NULL) {
		for (dp = bufqueues; dp < &bufqueues[BQUEUES]; dp++)
			if (dp->tqh_last == &bp->b_freelist.tqe_next)
				break;
		if (dp == &bufqueues[BQUEUES])
			panic("bremfree: lost tail");
	}
	if (bp->b_bufsize <= 0) {
		numemptybufs--;
	} else if (!ISSET(bp->b_flags, B_LOCKED)) {
		numfreepages -= btoc(bp->b_bufsize);
		if (!ISSET(bp->b_flags, B_DELWRI)) {
			numcleanpages -= btoc(bp->b_bufsize);
#ifdef DEBUG
			if (mincleanpages > numcleanpages)
				mincleanpages = numcleanpages;
#endif
		} else {
			numdirtypages -= btoc(bp->b_bufsize);
		}
	}
	TAILQ_REMOVE(dp, bp, b_freelist);
}

/*
 * Initialize buffers and hash links for buffers.
 */
void
bufinit(void)
{
	struct buf *bp;
	struct bqueues *dp;
	int i;
	int base, residual;

	pool_init(&bufpool, sizeof(struct buf), 0, 0, 0, "bufpl", NULL);
	for (dp = bufqueues; dp < &bufqueues[BQUEUES]; dp++)
		TAILQ_INIT(dp);
	bufhashtbl = hashinit(nbuf, M_CACHE, M_WAITOK, &bufhash);
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (i = 0; i < nbuf; i++) {
		bp = &buf[i];
		bzero((char *)bp, sizeof *bp);
		bp->b_dev = NODEV;
		bp->b_vnbufs.le_next = NOLIST;
		bp->b_data = buffers + i * MAXBSIZE;
		LIST_INIT(&bp->b_dep);
		if (i < residual)
			bp->b_bufsize = (base + 1) * PAGE_SIZE;
		else
			bp->b_bufsize = base * PAGE_SIZE;
		bp->b_flags = B_INVAL;
		if (bp->b_bufsize) {
			dp = &bufqueues[BQ_CLEAN];
			numfreepages += btoc(bp->b_bufsize);
			numcleanpages += btoc(bp->b_bufsize);
		} else {
			dp = &bufqueues[BQ_EMPTY];
			numemptybufs++;
		}
		binsheadfree(bp, dp);
		binshash(bp, &invalhash);
	}

	hidirtypages = bufpages / 4;
	lodirtypages = hidirtypages / 2;

	/*
	 * Reserve 5% of bufpages for syncer's needs,
	 * but not more than 25% and if possible
	 * not less then 2 * MAXBSIZE. locleanpages
	 * value must be not too small, but probably
	 * there are no reason to set it more than 1-2 MB.
	 */
	locleanpages = bufpages / 20;
	if (locleanpages < btoc(2 * MAXBSIZE))
		locleanpages = btoc(2 * MAXBSIZE);
	if (locleanpages > bufpages / 4)
		locleanpages = bufpages / 4;
	if (locleanpages > btoc(2 * 1024 * 1024))
		locleanpages = btoc(2 * 1024 * 1024);

#ifdef DEBUG
	mincleanpages = locleanpages;
#endif
}

static __inline struct buf *
bio_doread(struct vnode *vp, daddr_t blkno, int size, int async)
{
	struct buf *bp;

	bp = getblk(vp, blkno, size, 0, 0);

	/*
	 * If buffer does not have data valid, start a read.
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
bread(struct vnode *vp, daddr_t blkno, int size, struct ucred *cred,
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
breadn(struct vnode *vp, daddr_t blkno, int size, daddr_t rablks[],
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
 * Read with single-block read-ahead.  Defined in Bach (p.55), but
 * implemented as a call to breadn().
 * XXX for compatibility with old file systems.
 */
int
breada(struct vnode *vp, daddr_t blkno, int size, daddr_t rablkno, int rabsize,
    struct ucred *cred, struct buf **bpp)
{

	return (breadn(vp, blkno, size, &rablkno, &rabsize, 1, cred, bpp));	
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

	/*
	 * Remember buffer type, to switch on it later.  If the write was
	 * synchronous, but the file system was mounted with MNT_ASYNC,
	 * convert it to a delayed write.
	 * XXX note that this relies on delayed tape writes being converted
	 * to async, not sync writes (which is safe, but ugly).
	 */
	async = ISSET(bp->b_flags, B_ASYNC);
	if (!async && bp->b_vp && bp->b_vp->v_mount &&
	    ISSET(bp->b_vp->v_mount->mnt_flag, MNT_ASYNC)) {
		bdwrite(bp);
		return (0);
	}

	/*
	 * Collect statistics on synchronous and asynchronous writes.
	 * Writes to block devices are charged to their associated
	 * filesystem (if any).
	 */
	if ((vp = bp->b_vp) != NULL) {
		if (vp->v_type == VBLK)
			mp = vp->v_specmountpoint;
		else
			mp = vp->v_mount;
		if (mp != NULL) {
			if (async)
				mp->mnt_stat.f_asyncwrites++;
			else
				mp->mnt_stat.f_syncwrites++;
		}
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
		s = splbio();
		reassignbuf(bp);
		splx(s);
		curproc->p_stats->p_ru.ru_oublock++;	/* XXX */
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

	/*
	 * Determine which queue the buffer should be on, then put it there.
	 */

	/* If it's locked, don't report an error; try again later. */
	if (ISSET(bp->b_flags, (B_LOCKED|B_ERROR)) == (B_LOCKED|B_ERROR))
		CLR(bp->b_flags, B_ERROR);

	/* If it's not cacheable, or an error, mark it invalid. */
	if (ISSET(bp->b_flags, (B_NOCACHE|B_ERROR)))
		SET(bp->b_flags, B_INVAL);

	if ((bp->b_bufsize <= 0) || ISSET(bp->b_flags, B_INVAL)) {
		/*
		 * If it's invalid or empty, dissociate it from its vnode
		 * and put on the head of the appropriate queue.
		 */
		if (LIST_FIRST(&bp->b_dep) != NULL)
			buf_deallocate(bp);

		if (ISSET(bp->b_flags, B_DELWRI)) {
			CLR(bp->b_flags, B_DELWRI);
		}

		if (bp->b_vp) {
			reassignbuf(bp);
			brelvp(bp);
		}
		if (bp->b_bufsize <= 0) {
			/* no data */
			bufq = &bufqueues[BQ_EMPTY];
			numemptybufs++;
		} else {
			/* invalid data */
			bufq = &bufqueues[BQ_CLEAN];
			numfreepages += btoc(bp->b_bufsize);
			numcleanpages += btoc(bp->b_bufsize);
		}
		binsheadfree(bp, bufq);
	} else {
		/*
		 * It has valid data.  Put it on the end of the appropriate
		 * queue, so that it'll stick around for as long as possible.
		 */
		if (ISSET(bp->b_flags, B_LOCKED))
			/* locked in core */
			bufq = &bufqueues[BQ_LOCKED];
		else {
			numfreepages += btoc(bp->b_bufsize);
			if (!ISSET(bp->b_flags, B_DELWRI)) {
				numcleanpages += btoc(bp->b_bufsize);
				bufq = &bufqueues[BQ_CLEAN];
			} else {
				numdirtypages += btoc(bp->b_bufsize);
				bufq = &bufqueues[BQ_DIRTY];
			}
		}
		if (ISSET(bp->b_flags, B_AGE))
			binsheadfree(bp, bufq);
		else
			binstailfree(bp, bufq);
	}

	/* Unlock the buffer. */
	CLR(bp->b_flags, (B_AGE | B_ASYNC | B_BUSY | B_NOCACHE | B_DEFERRED));


	/* Wake up syncer and cleaner processes waiting for buffers */
	if (nobuffers) {
		wakeup(&nobuffers);
		nobuffers = 0;
	}

	/* Wake up any processes waiting for any buffer to become free. */
	if (needbuffer && (numcleanpages > locleanpages)) {
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
 * Determine if a block is in the cache.
 * Just look on what would be its hash chain.  If it's there, return
 * a pointer to it, unless it's marked invalid.  If it's marked invalid,
 * we normally don't return the buffer, unless the caller explicitly
 * wants us to.
 */
struct buf *
incore(struct vnode *vp, daddr_t blkno)
{
	struct buf *bp;

	/* Search hash chain */
	LIST_FOREACH(bp, BUFHASH(vp, blkno), b_hash) {
		if (bp->b_lblkno == blkno && bp->b_vp == vp &&
		    !ISSET(bp->b_flags, B_INVAL))
			return (bp);
	}

	return (0);
}

/*
 * Get a block of requested size that is associated with
 * a given vnode and block offset. If it is found in the
 * block cache, mark it as having been found, make it busy
 * and return it. Otherwise, return an empty block of the
 * correct size. It is up to the caller to insure that the
 * cached blocks be of the correct size.
 */
struct buf *
getblk(struct vnode *vp, daddr_t blkno, int size, int slpflag, int slptimeo)
{
	struct bufhashhdr *bh;
	struct buf *bp, *nbp = NULL;
	int s, err;

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
			SET(bp->b_flags, B_WANTED);
			err = tsleep(bp, slpflag | (PRIBIO + 1), "getblk",
			    slptimeo);
			splx(s);
			if (err)
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

	if (bp == NULL) {
		if (nbp == NULL && getnewbuf(slpflag, slptimeo, &nbp) != 0) {
			goto start;
		}
		bp = nbp;
		binshash(bp, bh);
		bp->b_blkno = bp->b_lblkno = blkno;
		s = splbio();
		bgetvp(vp, bp);
		splx(s);
	} else if (nbp != NULL) {
		/*
		 * Set B_AGE so that buffer appear at BQ_CLEAN head
		 * and gets reused ASAP.
		 */
		SET(nbp->b_flags, B_AGE);
		brelse(nbp);
	}
	allocbuf(bp, size);

	return (bp);
}

/*
 * Get an empty, disassociated buffer of given size.
 */
struct buf *
geteblk(int size)
{
	struct buf *bp;

	getnewbuf(0, 0, &bp);
	SET(bp->b_flags, B_INVAL);
	binshash(bp, &invalhash);
	allocbuf(bp, size);

	return (bp);
}

/*
 * Expand or contract the actual memory allocated to a buffer.
 *
 * If the buffer shrinks, data is lost, so it's up to the
 * caller to have written it out *first*; this routine will not
 * start a write.  If the buffer grows, it's the callers
 * responsibility to fill out the buffer's additional contents.
 */
void
allocbuf(struct buf *bp, int size)
{
	struct buf	*nbp;
	vsize_t		desired_size;
	int		s;

	desired_size = round_page(size);
	if (desired_size > MAXBSIZE)
		panic("allocbuf: buffer larger than MAXBSIZE requested");

	if (bp->b_bufsize == desired_size)
		goto out;

	/*
	 * If the buffer is smaller than the desired size, we need to snarf
	 * it from other buffers.  Get buffers (via getnewbuf()), and
	 * steal their pages.
	 */
	while (bp->b_bufsize < desired_size) {
		int amt;

		/* find a buffer */
		getnewbuf(0, 0, &nbp);
 		SET(nbp->b_flags, B_INVAL);
		binshash(nbp, &invalhash);

		/* and steal its pages, up to the amount we need */
		amt = MIN(nbp->b_bufsize, (desired_size - bp->b_bufsize));
		pagemove((nbp->b_data + nbp->b_bufsize - amt),
			 bp->b_data + bp->b_bufsize, amt);
		bp->b_bufsize += amt;
		nbp->b_bufsize -= amt;

		/* reduce transfer count if we stole some data */
		if (nbp->b_bcount > nbp->b_bufsize)
			nbp->b_bcount = nbp->b_bufsize;

#ifdef DIAGNOSTIC
		if (nbp->b_bufsize < 0)
			panic("allocbuf: negative bufsize");
#endif

		brelse(nbp);
	}

	/*
	 * If we want a buffer smaller than the current size,
	 * shrink this buffer.  Grab a buf head from the EMPTY queue,
	 * move a page onto it, and put it on front of the AGE queue.
	 * If there are no free buffer headers, leave the buffer alone.
	 */
	if (bp->b_bufsize > desired_size) {
		s = splbio();
		if ((nbp = bufqueues[BQ_EMPTY].tqh_first) == NULL) {
			/* No free buffer head */
			splx(s);
			goto out;
		}
		bremfree(nbp);
		SET(nbp->b_flags, B_BUSY);
		splx(s);

		/* move the page to it and note this change */
		pagemove(bp->b_data + desired_size,
		    nbp->b_data, bp->b_bufsize - desired_size);
		nbp->b_bufsize = bp->b_bufsize - desired_size;
		bp->b_bufsize = desired_size;
		nbp->b_bcount = 0;
		SET(nbp->b_flags, B_INVAL);

		/* release the newly-filled buffer and leave */
		brelse(nbp);
	}

out:
	bp->b_bcount = size;
}

/*
 * Find a buffer which is available for use.
 *
 * We must notify getblk if we slept during the buffer allocation. When
 * that happens, we allocate a buffer anyway (unless tsleep is interrupted
 * or times out) and return !0.
 */
int
getnewbuf(int slpflag, int slptimeo, struct buf **bpp)
{
	struct buf *bp;
	int s, ret, error;

	*bpp = NULL;
	ret = 0;

start:
	s = splbio();
	/*
	 * Wake up cleaner if we're getting low on buffers.
	 */
	if (numdirtypages >= hidirtypages)
		wakeup(&bd_req);

	if ((numcleanpages <= locleanpages) &&
	    curproc != syncerproc && curproc != cleanerproc) {
		needbuffer++;
		error = tsleep(&needbuffer, slpflag|(PRIBIO+1), "getnewbuf",
				slptimeo);
		splx(s);
		if (error)
			return (1);
		ret = 1;
		goto start;
	}
	if ((bp = TAILQ_FIRST(&bufqueues[BQ_CLEAN])) == NULL) {
		/* wait for a free buffer of any kind */
		nobuffers = 1;
		error = tsleep(&nobuffers, slpflag|(PRIBIO-3),
				"getnewbuf", slptimeo);
		splx(s);
		if (error)
			return (1);
		ret = 1;
		goto start;
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
	bp->b_iodone = 0;
	bp->b_error = 0;
	bp->b_resid = 0;
	bp->b_bcount = 0;
	bp->b_dirtyoff = bp->b_dirtyend = 0;
	bp->b_validoff = bp->b_validend = 0;

	bremhash(bp);
	*bpp = bp;
	return (ret);
}

/*
 * Buffer cleaning daemon.
 */
void
buf_daemon(struct proc *p)
{
	int s;
	struct buf *bp;
	struct timeval starttime, timediff;

	cleanerproc = curproc;

	for (;;) {
		if (numdirtypages < hidirtypages) {
			tsleep(&bd_req, PRIBIO - 7, "cleaner", 0);
		}

		getmicrouptime(&starttime);
		s = splbio();
		while ((bp = TAILQ_FIRST(&bufqueues[BQ_DIRTY]))) {
			struct timeval tv;

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
			    buf_countdeps(bp, 0, 1)) {
				SET(bp->b_flags, B_DEFERRED);
				s = splbio();
				numfreepages += btoc(bp->b_bufsize);
				numdirtypages += btoc(bp->b_bufsize);
				binstailfree(bp, &bufqueues[BQ_DIRTY]);
				CLR(bp->b_flags, B_BUSY);
				continue;
			}

			bawrite(bp);

			if (numdirtypages < lodirtypages)
				break;
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

#ifdef DEBUG
/*
 * Print out statistics on the current allocation of the buffer pool.
 * Can be enabled to print out on every ``sync'' by setting "syncprt"
 * in vfs_syscalls.c using sysctl.
 */
void
vfs_bufstats()
{
	int s, i, j, count;
	register struct buf *bp;
	register struct bqueues *dp;
	int counts[MAXBSIZE/PAGE_SIZE+1];
	int totals[BQUEUES];
	long ptotals[BQUEUES];
	long pages;
	static char *bname[BQUEUES] = { "LOCKED", "CLEAN", "DIRTY", "EMPTY" };

	s = splbio();
	for (dp = bufqueues, i = 0; dp < &bufqueues[BQUEUES]; dp++, i++) {
		count = 0;
		pages = 0;
		for (j = 0; j <= MAXBSIZE/PAGE_SIZE; j++)
			counts[j] = 0;
		for (bp = dp->tqh_first; bp; bp = bp->b_freelist.tqe_next) {
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
	if (totals[BQ_EMPTY] != numemptybufs)
		printf("numemptybufs counter wrong: %d != %d\n",
			numemptybufs, totals[BQ_EMPTY]);
	if ((ptotals[BQ_CLEAN] + ptotals[BQ_DIRTY]) != numfreepages)
		printf("numfreepages counter wrong: %ld != %ld\n",
			numfreepages, ptotals[BQ_CLEAN] + ptotals[BQ_DIRTY]);
	if (ptotals[BQ_CLEAN] != numcleanpages)
		printf("numcleanpages counter wrong: %ld != %ld\n",
			numcleanpages, ptotals[BQ_CLEAN]);
	else
		printf("numcleanpages: %ld\n", numcleanpages);
	if (numdirtypages != ptotals[BQ_DIRTY])
		printf("numdirtypages counter wrong: %ld != %ld\n",
			numdirtypages, ptotals[BQ_DIRTY]);
	else
		printf("numdirtypages: %ld\n", numdirtypages);

	printf("syncer eating up to %ld pages from %ld reserved\n",
			locleanpages - mincleanpages, locleanpages);
	splx(s);
}
#endif /* DEBUG */

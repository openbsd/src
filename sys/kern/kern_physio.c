/*	$OpenBSD: kern_physio.c,v 1.24 2005/12/08 14:02:47 krw Exp $	*/
/*	$NetBSD: kern_physio.c,v 1.28 1997/05/19 10:43:28 pk Exp $	*/

/*-
 * Copyright (c) 1994 Christopher G. Demetriou
 * Copyright (c) 1982, 1986, 1990, 1993
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
 *	@(#)kern_physio.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/pool.h>

#include <uvm/uvm_extern.h>

/*
 * The routines implemented in this file are described in:
 *	Leffler, et al.: The Design and Implementation of the 4.3BSD
 *	    UNIX Operating System (Addison Welley, 1989)
 * on pages 231-233.
 *
 * The routines "getphysbuf" and "putphysbuf" steal and return a swap
 * buffer.  Leffler, et al., says that swap buffers are used to do the
 * I/O, so raw I/O requests don't have to be single-threaded.
 */

struct buf *getphysbuf(void);
void putphysbuf(struct buf *bp);

/*
 * Do "physical I/O" on behalf of a user.  "Physical I/O" is I/O directly
 * from the raw device to user buffers, and bypasses the buffer cache.
 *
 * Comments in brackets are from Leffler, et al.'s pseudo-code implementation.
 */
int
physio(void (*strategy)(struct buf *), struct buf *bp, dev_t dev, int flags,
    void (*minphys)(struct buf *), struct uio *uio)
{
	struct iovec *iovp;
	struct proc *p = curproc;
	int error, done, i, nobuf, s, todo;

	error = 0;
	flags &= B_READ | B_WRITE;

	/* Make sure we have a buffer, creating one if necessary. */
	if ((nobuf = (bp == NULL)) != 0)
		bp = getphysbuf();

	/* [raise the processor priority level to splbio;] */
	s = splbio();

	/* [while the buffer is marked busy] */
	while (bp->b_flags & B_BUSY) {
		/* [mark the buffer wanted] */
		bp->b_flags |= B_WANTED;
		/* [wait until the buffer is available] */
		tsleep(bp, PRIBIO+1, "physbuf", 0);
	}

	/* Mark it busy, so nobody else will use it. */
	bp->b_flags |= B_BUSY;

	/* [lower the priority level] */
	splx(s);

	/* [set up the fixed part of the buffer for a transfer] */
	bp->b_dev = dev;
	bp->b_error = 0;
	bp->b_proc = p;
	LIST_INIT(&bp->b_dep);

	/*
	 * [while there are data to transfer and no I/O error]
	 * Note that I/O errors are handled with a 'goto' at the bottom
	 * of the 'while' loop.
	 */
	for (i = 0; i < uio->uio_iovcnt; i++) {
		iovp = &uio->uio_iov[i];
		while (iovp->iov_len > 0) {
			/*
			 * [mark the buffer busy for physical I/O]
			 * (i.e. set B_PHYS (because it's an I/O to user
			 * memory, and B_RAW, because B_RAW is to be
			 * "Set by physio for raw transfers.", in addition
			 * to the "busy" and read/write flag.)
			 */
			bp->b_flags = B_BUSY | B_PHYS | B_RAW | flags;

			/* [set up the buffer for a maximum-sized transfer] */
			bp->b_blkno = btodb(uio->uio_offset);
			bp->b_data = iovp->iov_base;

			/*
			 * Because iov_len is unsigned but b_bcount is signed,
			 * an overflow is possible. Therefore bound to MAXPHYS
			 * before calling minphys.
			 */
			if (iovp->iov_len > MAXPHYS)
				bp->b_bcount = MAXPHYS;
			else
				bp->b_bcount = iovp->iov_len;

			/*
			 * [call minphys to bound the tranfer size]
			 * and remember the amount of data to transfer,
			 * for later comparison.
			 */
			(*minphys)(bp);
			todo = bp->b_bcount;
#ifdef DIAGNOSTIC
			if (todo < 0)
				panic("todo < 0; minphys broken");
			if (todo > MAXPHYS)
				panic("todo > MAXPHYS; minphys broken");
#endif

			/*
			 * [lock the part of the user address space involved
			 *    in the transfer]
			 * Beware vmapbuf(); it clobbers b_data and
			 * saves it in b_saveaddr.  However, vunmapbuf()
			 * restores it.
			 */
			PHOLD(p);
			error = uvm_vslock(p, bp->b_data, todo,
			    (flags & B_READ) ?
			    VM_PROT_READ | VM_PROT_WRITE : VM_PROT_READ);
			if (error) {
				bp->b_flags |= B_ERROR;
				bp->b_error = error;
				goto after_unlock;
			}
			vmapbuf(bp, todo);

			/* [call strategy to start the transfer] */
			(*strategy)(bp);

			/*
			 * Note that the raise/wait/lower/get error
			 * steps below would be done by biowait(), but
			 * we want to unlock the address space before
			 * we lower the priority.
			 *
			 * [raise the priority level to splbio]
			 */
			s = splbio();

			/* [wait for the transfer to complete] */
			while ((bp->b_flags & B_DONE) == 0)
				tsleep(bp, PRIBIO + 1, "physio", 0);

			/* Mark it busy again, so nobody else will use it. */
			bp->b_flags |= B_BUSY;

			/* [lower the priority level] */
			splx(s);

			/*
			 * [unlock the part of the address space previously
			 *    locked]
			 */
			vunmapbuf(bp, todo);
			uvm_vsunlock(p, bp->b_data, todo);
after_unlock:
			PRELE(p);

			/* remember error value (save a splbio/splx pair) */
			if (bp->b_flags & B_ERROR)
				error = (bp->b_error ? bp->b_error : EIO);

			/*
			 * [deduct the transfer size from the total number
			 *    of data to transfer]
			 */
			done = bp->b_bcount - bp->b_resid;
#ifdef DIAGNOSTIC
			if (done < 0)
				panic("done < 0; strategy broken");
			if (done > todo)
				panic("done > todo; strategy broken");
#endif
			iovp->iov_len -= done;
			iovp->iov_base = (caddr_t)iovp->iov_base + done;
			uio->uio_offset += done;
			uio->uio_resid -= done;

			/*
			 * Now, check for an error.
			 * Also, handle weird end-of-disk semantics.
			 */
			if (error || done < todo)
				goto done;
		}
	}

done:
	/*
	 * [clean up the state of the buffer]
	 * Remember if somebody wants it, so we can wake them up below.
	 * Also, if we had to steal it, give it back.
	 */
	s = splbio();
	bp->b_flags &= ~(B_BUSY | B_PHYS | B_RAW);
	if (nobuf)
		putphysbuf(bp);
	else {
		/*
		 * [if another process is waiting for the raw I/O buffer,
		 *    wake up processes waiting to do physical I/O;
		 */
		if (bp->b_flags & B_WANTED) {
			bp->b_flags &= ~B_WANTED;
			wakeup(bp);
		}
	}
	splx(s);

	return (error);
}

/*
 * Get a swap buffer structure, for use in physical I/O.
 * Mostly taken from /sys/vm/swap_pager.c, except that it no longer
 * records buffer list-empty conditions, and sleeps at PRIBIO + 1,
 * rather than PSWP + 1 (and on a different wchan).
 */
struct buf *
getphysbuf(void)
{
	struct buf *bp;

	bp = pool_get(&bufpool, PR_WAITOK);
	bzero(bp, sizeof(*bp));

	/* XXXCDC: are the following two lines necessary? */
	bp->b_vnbufs.le_next = NOLIST;

	return (bp);
}

/*
 * Get rid of a swap buffer structure which has been used in physical I/O.
 * Mostly taken from /sys/vm/swap_pager.c, except that it now uses
 * wakeup() rather than the VM-internal thread_wakeup(), and that the caller
 * must mask disk interrupts, rather than putphysbuf() itself.
 */
void
putphysbuf(struct buf *bp)
{
	/* XXXCDC: is this necessary? */
	if (bp->b_vp)
		brelvp(bp);

#ifdef DIAGNOSTIC
	if (bp->b_flags & B_WANTED)
		panic("putphysbuf: private buf B_WANTED");
#endif
	pool_put(&bufpool, bp);
}

/*
 * Leffler, et al., says on p. 231:
 * "The minphys() routine is called by physio() to adjust the
 * size of each I/O transfer before the latter is passed to
 * the strategy routine..."
 *
 * so, just adjust the buffer's count accounting to MAXPHYS here,
 * and return the new count;
 */
void
minphys(struct buf *bp)
{

	if (bp->b_bcount > MAXPHYS)
		bp->b_bcount = MAXPHYS;
}

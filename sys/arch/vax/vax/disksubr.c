/*	$OpenBSD: disksubr.c,v 1.62 2011/02/26 13:07:48 krw Exp $	*/
/*	$NetBSD: disksubr.c,v 1.21 1999/06/30 18:48:06 ragge Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/disk.h>

#include <uvm/uvm_extern.h>

#include <machine/macros.h>
#include <machine/pte.h>
#include <machine/pcb.h>
#include <machine/cpu.h>

#include <vax/mscp/mscp.h> /* For disk encoding scheme */

#include "mba.h"

/*
 * Attempt to read a disk label from a device
 * using the indicated strategy routine.
 * The label must be partly set up before this:
 * secpercyl and anything required in the strategy routine
 * (e.g., sector size) must be filled in before calling us.
 * Returns null on success and an error string on failure.
 */
int
readdisklabel(dev_t dev, void (*strat)(struct buf *),
    struct disklabel *lp, int spoofonly)
{
	struct buf *bp = NULL;
	char error;

	if ((error = initdisklabel(lp)))
		goto done;

	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	DL_SETBSTART(lp, 16);

	if (spoofonly)
		goto done;

	bp->b_blkno = LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ | B_RAW;
	(*strat)(bp);
	if (biowait(bp)) {
		error = bp->b_error;
		goto done;
	}

	error = checkdisklabel(bp->b_data + LABELOFFSET, lp,
	    16, DL_GETDSIZE(lp));
	if (error == 0)
		goto done;

#if defined(CD9660)
	error = iso_disklabelspoof(dev, strat, lp);
	if (error == 0)
		goto done;
#endif
#if defined(UDF)
	error = udf_disklabelspoof(dev, strat, lp);
	if (error == 0)
		goto done;
#endif

done:
	if (bp) {
		bp->b_flags |= B_INVAL;
		brelse(bp);
	}
	disk_change = 1;
	return (error);
}

/*
 * Write disk label back to device after modification.
 * Always allow writing of disk label; even if the disk is unlabeled.
 */
int
writedisklabel(dev_t dev, void (*strat)(struct buf *), struct disklabel *lp)
{
	struct buf *bp = NULL;
	struct disklabel *dlp;
	int error = 0;

	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* Read it in, slap the new label in, and write it back out */
	bp->b_blkno = LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ | B_RAW;
	(*strat)(bp);
	if ((error = biowait(bp)) != 0)
		goto done;

	dlp = (struct disklabel *)(bp->b_data + LABELOFFSET);
	*dlp = *lp;
	bp->b_flags = B_BUSY | B_WRITE | B_RAW;
	(*strat)(bp);
	error = biowait(bp);

done:
	if (bp) {
		bp->b_flags |= B_INVAL;
		brelse(bp);
	}
	disk_change = 1;
	return (error);
}

/*
 * Print out the name of the device; ex. TK50, RA80. DEC uses a common
 * disk type encoding scheme for most of its disks.
 */
void
disk_printtype(int unit, int type)
{
	printf(" drive %d: %c%c", unit, (int)MSCP_MID_CHAR(2, type),
	    (int)MSCP_MID_CHAR(1, type));
	if (MSCP_MID_ECH(0, type))
		printf("%c", (int)MSCP_MID_CHAR(0, type));
	printf("%d\n", MSCP_MID_NUM(type));
}

#if NMBA > 0
/*
 * Be sure that the pages we want to do DMA to is actually there
 * by faking page-faults if necessary. If given a map-register address,
 * also map it in.
 */
void
disk_reallymapin(struct buf *bp, pt_entry_t *map, int reg, int flag)
{
	struct proc *p;
	volatile pt_entry_t *io;
	pt_entry_t *pte;
	struct pcb *pcb;
	int pfnum, npf, o;
	caddr_t addr;

	o = (int)bp->b_data & VAX_PGOFSET;
	npf = vax_atop(bp->b_bcount + o) + 1;
	addr = bp->b_data;
	p = bp->b_proc;

	/*
	 * Get a pointer to the pte pointing out the first virtual address.
	 * Use different ways in kernel and user space.
	 */
	if ((bp->b_flags & B_PHYS) == 0) {
		pte = kvtopte(addr);
		p = &proc0;
	} else {
		pcb = &p->p_addr->u_pcb;
		pte = uvtopte(addr, pcb);
	}

	if (map) {
		io = &map[reg];
		while (--npf > 0) {
			pfnum = (*pte & PG_FRAME);
			if (pfnum == 0)
				panic("mapin zero entry");
			pte++;
			*(int *)io++ = pfnum | flag;
		}
		*(int *)io = 0;
	}
}
#endif

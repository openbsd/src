/*	$OpenBSD: disksubr.c,v 1.50 2007/06/14 03:35:30 deraadt Exp $	*/
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
 *
 *	@(#)ufs_disksubr.c	7.16 (Berkeley) 5/4/91
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

/*
 * Attempt to read a disk label from a device
 * using the indicated strategy routine.
 * The label must be partly set up before this:
 * secpercyl and anything required in the strategy routine
 * (e.g., sector size) must be filled in before calling us.
 * Returns null on success and an error string on failure.
 */
char *
readdisklabel(dev_t dev, void (*strat)(struct buf *),
    struct disklabel *lp, struct cpu_disklabel *osdep, int spoofonly)
{
	struct buf *bp = NULL;
	struct disklabel *dlp;
	char *msg = NULL;
	int i;

	/* minimal requirements for archetypal disk label */
	if (lp->d_secsize < DEV_BSIZE)
		lp->d_secsize = DEV_BSIZE;
	if (DL_GETDSIZE(lp) == 0)
		DL_SETDSIZE(lp, MAXDISKSIZE);
	if (lp->d_secpercyl == 0) {
		msg = "invalid geometry";
		goto done;
	}
	lp->d_npartitions = RAW_PART + 1;
	for (i = 0; i < RAW_PART; i++) {
		DL_SETPSIZE(&lp->d_partitions[i], 0);
		DL_SETPOFFSET(&lp->d_partitions[i], 0);
	}
	if (DL_GETPSIZE(&lp->d_partitions[RAW_PART]) == 0)
		DL_SETPSIZE(&lp->d_partitions[RAW_PART], DL_GETDSIZE(lp));
	DL_SETPOFFSET(&lp->d_partitions[RAW_PART], 0);
	lp->d_version = 1;
	lp->d_bbsize = 8192;
	lp->d_sbsize = 64 * 1024;

	/* don't read the on-disk label if we are in spoofed-only mode */
	if (spoofonly)
		goto done;

	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;
	bp->b_blkno = LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylinder = LABELSECTOR / lp->d_secpercyl;
	(*strat)(bp);
	if (biowait(bp)) {
		msg = "I/O error";
	} else {
		dlp = (struct disklabel *)(bp->b_data + LABELOFFSET);
		if (dlp->d_magic != DISKMAGIC || dlp->d_magic2 != DISKMAGIC) {
			msg = "no disk label";
		} else if (dlp->d_npartitions > MAXPARTITIONS ||
		    dkcksum(dlp) != 0)
			msg = "disk label corrupted";
		else {
			DL_SETDSIZE(dlp, DL_GETDSIZE(lp));
			*lp = *dlp;
		}
	}

#if defined(CD9660)
	if (msg && iso_disklabelspoof(dev, strat, lp) == 0)
		msg = NULL;
#endif
#if defined(UDF)
	if (msg && udf_disklabelspoof(dev, strat, lp) == 0)
		msg = NULL;
#endif

done:
	if (bp) {
		bp->b_flags |= B_INVAL;
		brelse(bp);
	}
	disklabeltokernlabel(lp);
	return (msg);
}


/*
 * Write disk label back to device after modification.
 * Always allow writing of disk label; even if the disk is unlabeled.
 */
int
writedisklabel(dev_t dev, void (*strat)(struct buf *),
    struct disklabel *lp, struct cpu_disklabel *osdep)
{
	struct buf *bp = NULL;
	struct disklabel *dlp;
	int error = 0;

	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = MAKEDISKDEV(major(dev), DISKUNIT(dev), RAW_PART);
	bp->b_blkno = LABELSECTOR;
	bp->b_cylinder = LABELSECTOR / lp->d_secpercyl;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);
	if ((error = biowait(bp)) != 0)
		goto done;

	dlp = (struct disklabel *)(bp->b_data + LABELOFFSET);
	bcopy(lp, dlp, sizeof(struct disklabel));
	bp->b_flags = B_BUSY | B_WRITE;
	(*strat)(bp);
	error = biowait(bp);

done:
	if (bp) {
		bp->b_flags |= B_INVAL;
		brelse(bp);
	}
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
	npf = vax_btoc(bp->b_bcount + o) + 1;
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

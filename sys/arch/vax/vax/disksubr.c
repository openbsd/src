/*	$OpenBSD: disksubr.c,v 1.26 2006/01/20 23:27:26 miod Exp $	*/
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
#include <sys/dkbad.h>
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
 * Determine the size of the transfer, and make sure it is
 * within the boundaries of the partition. Adjust transfer
 * if needed, and signal errors or early completion.
 */
int
bounds_check_with_label(bp, lp, osdep, wlabel)
	struct	buf *bp;
	struct	disklabel *lp;
	struct	cpu_disklabel *osdep;
	int	wlabel;
{
	struct partition *p = lp->d_partitions + DISKPART(bp->b_dev);
	int labelsect = lp->d_partitions[2].p_offset;
	int maxsz = p->p_size,
		sz = (bp->b_bcount + DEV_BSIZE - 1) >> DEV_BSHIFT;

	/* avoid division by zero */
	if (lp->d_secpercyl == 0) {
		bp->b_error = EINVAL;
		goto bad;
	}

	/* overwriting disk label ? */
	if (bp->b_blkno + p->p_offset <= LABELSECTOR + labelsect &&
	    (bp->b_flags & B_READ) == 0 && wlabel == 0) {
		bp->b_error = EROFS;
		goto bad;
	}

	/* beyond partition? */
	if (bp->b_blkno < 0 || bp->b_blkno + sz > maxsz) {
		/* if exactly at end of disk, return an EOF */
		if (bp->b_blkno == maxsz) {
			bp->b_resid = bp->b_bcount;
			return(0);
		}
		/* or truncate if part of it fits */
		sz = maxsz - bp->b_blkno;
		if (sz <= 0) {
			bp->b_error = EINVAL;
			goto bad;
		}
		bp->b_bcount = sz << DEV_BSHIFT;
	}

	/* calculate cylinder for disksort to order transfers with */
	bp->b_cylinder = (bp->b_blkno + p->p_offset) / lp->d_secpercyl;	
	return(1);

bad:
	bp->b_flags |= B_ERROR;
	return(-1);
}

/*
 * Attempt to read a disk label from a device
 * using the indicated strategy routine.
 * The label must be partly set up before this:
 * secpercyl and anything required in the strategy routine
 * (e.g., sector size) must be filled in before calling us.
 * Returns null on success and an error string on failure.
 */
char *
readdisklabel(dev, strat, lp, osdep, spoofonly)
	dev_t dev;
	void (*strat)(struct buf *);
	register struct disklabel *lp;
	struct cpu_disklabel *osdep;
	int spoofonly;
{
	register struct buf *bp;
	struct disklabel *dlp;
	char *msg = NULL;
	int i;

	/* minimal requirements for archetypal disk label */
	if (lp->d_secsize < DEV_BSIZE)
		lp->d_secsize = DEV_BSIZE;
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;
	lp->d_npartitions = RAW_PART + 1;
	for (i = 0; i < RAW_PART; i++) {
		lp->d_partitions[i].p_size = 0;
		lp->d_partitions[i].p_offset = 0;
	}
	if (lp->d_partitions[i].p_size == 0)
		lp->d_partitions[i].p_size = 0x1fffffff;
	lp->d_partitions[i].p_offset = 0;
	lp->d_bbsize = 8192;
	lp->d_sbsize = 64 * 1024;

	/* don't read the on-disk label if we are in spoofed-only mode */
	if (spoofonly)
		return (NULL);

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

	bp->b_flags = B_INVAL | B_AGE | B_READ;
	brelse(bp);
	return (msg);
}

void 
dk_establish(p, q)
	struct disk *p;
	struct device *q;
{
}

/*
 * Check new disk label for sensibility
 * before setting it.
 */
int
setdisklabel(olp, nlp, openmask, osdep)
	register struct disklabel *olp, *nlp;
	u_long openmask;
	struct cpu_disklabel *osdep;
{
	register int i;
	register struct partition *opp, *npp;

	/* sanity clause */
	if (nlp->d_secpercyl == 0 || nlp->d_secsize == 0 ||
	    (nlp->d_secsize % DEV_BSIZE) != 0)
		return (EINVAL);

	/* special case to allow disklabel to be invalidated */
	if (nlp->d_magic == 0xffffffff) {
		*olp = *nlp;
		return (0);
	}
		
	if (nlp->d_magic != DISKMAGIC || nlp->d_magic2 != DISKMAGIC ||
	    dkcksum(nlp) != 0)
		return (EINVAL);

	while ((i = ffs((long)openmask)) != 0) {
		i--;
		openmask &= ~(1 << i);
		if (nlp->d_npartitions <= i)
			return (EBUSY);
		opp = &olp->d_partitions[i];
		npp = &nlp->d_partitions[i];
		if (npp->p_offset != opp->p_offset || npp->p_size < opp->p_size)
			return (EBUSY);
		/*
		 * Copy internally-set partition information
		 * if new label doesn't include it.		XXX
		 */
		if (npp->p_fstype == FS_UNUSED && opp->p_fstype != FS_UNUSED) {
			npp->p_fstype = opp->p_fstype;
			npp->p_fsize = opp->p_fsize;
			npp->p_frag = opp->p_frag;
			npp->p_cpg = opp->p_cpg;
		}
	}
	nlp->d_checksum = 0;
	nlp->d_checksum = dkcksum(nlp);
	*olp = *nlp;
	return (0);
}

/*
 * Write disk label back to device after modification.
 * Always allow writing of disk label; even if the disk is unlabeled.
 */
int
writedisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat)(struct buf *);
	register struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	struct buf *bp;
	struct disklabel *dlp;
	int error = 0;

	bp = geteblk((int)lp->d_secsize);

	dlp = (struct disklabel *)(bp->b_data + LABELOFFSET);
	bcopy(lp, dlp, sizeof(struct disklabel));

	bp->b_dev = MAKEDISKDEV(major(dev), DISKUNIT(dev), RAW_PART);
	bp->b_blkno = LABELSECTOR;
	bp->b_cylinder = LABELSECTOR / lp->d_secpercyl;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_WRITE;
	(*strat)(bp);
	error = biowait(bp);
	brelse(bp);

	return (error);
}

/*	
 * Print out the name of the device; ex. TK50, RA80. DEC uses a common
 * disk type encoding scheme for most of its disks.
 */   
void  
disk_printtype(unit, type)
	int unit, type;
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
disk_reallymapin(bp, map, reg, flag)
	struct buf *bp;
	pt_entry_t *map;
	int reg, flag;
{
	struct proc *p;
	volatile pt_entry_t *io;
	pt_entry_t *pte;
	struct pcb *pcb;
	int pfnum, npf, o, i;
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

	/*
	 * When we are doing DMA to user space, be sure that all pages
	 * we want to transfer to is mapped. WHY DO WE NEED THIS???
	 * SHOULDN'T THEY ALWAYS BE MAPPED WHEN DOING THIS???
	 */
	for (i = 0; i < (npf - 1); i++) {
		if ((pte[i] & PG_FRAME) == 0) {
			int rv;
			rv = uvm_fault(&p->p_vmspace->vm_map,
			    (unsigned)addr + i * VAX_NBPG, 0,
			    VM_PROT_READ|VM_PROT_WRITE);
			if (rv)
				panic("DMA to nonexistent page, %d", rv);
		}
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

/*	$NetBSD: disksubr.c,v 1.1 1996/09/30 16:34:43 ws Exp $	*/

/*
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/systm.h>

static inline unsigned short get_short __P((void *p));
static inline unsigned long get_long __P((void *p));
static int get_netbsd_label __P((dev_t dev, void (*strat)(struct buf *),
				 struct disklabel *lp, daddr_t bno));
static int mbr_to_label __P((dev_t dev, void (*strat)(struct buf *),
			     daddr_t bno, struct disklabel *lp,
			     unsigned short *pnpart,
			     struct cpu_disklabel *osdep, daddr_t off));

/*
 * Little endian access routines
 */
static inline unsigned short
get_short(p)
	void *p;
{
	unsigned char *cp = p;

	return cp[0] | (cp[1] << 8);
}

static inline unsigned long
get_long(p)
	void *p;
{
	unsigned char *cp = p;

	return cp[0] | (cp[1] << 8) | (cp[2] << 16) | (cp[3] << 24);
}

/*
 * Get real NetBSD disk label
 */
static int
get_netbsd_label(dev, strat, lp, bno)
	dev_t dev;
	void (*strat)();
	struct disklabel *lp;
	daddr_t bno;
{
	struct buf *bp;
	struct disklabel *dlp;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* Now get the label block */
	bp->b_blkno = bno + LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylinder = bp->b_blkno / (lp->d_secsize / DEV_BSIZE) / lp->d_secpercyl;
	(*strat)(bp);

	if (biowait(bp))
		goto done;

	for (dlp = (struct disklabel *)bp->b_data;
	     dlp <= (struct disklabel *)(bp->b_data + lp->d_secsize - sizeof (*dlp));
	     dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic == DISKMAGIC
		    && dlp->d_magic2 == DISKMAGIC
		    && dlp->d_npartitions <= MAXPARTITIONS
		    && dkcksum(dlp) == 0) {
			*lp = *dlp;
			brelse(bp);
			return 1;
		}
	}
done:
	bp->b_flags |= B_INVAL;
	brelse(bp);
	return 0;
}

/*
 * Construct disklabel entries from partition entries.
 */
static int
mbr_to_label(dev, strat, bno, lp, pnpart, osdep, off)
	dev_t dev;
	void (*strat)();
	daddr_t bno;
	struct disklabel *lp;
	unsigned short *pnpart;
	struct cpu_disklabel *osdep;
	daddr_t off;
{
	static int recursion = 0;
	struct mbr_partition *mp;
	struct partition *pp;
	struct buf *bp;
	int i, found = 0;

	/* Check for recursion overflow. */
	if (recursion > MAXPARTITIONS)
		return 0;

	/*
	 * Extended partitions seem to be relative to their first occurence?
	 */
	if (recursion++ == 1)
		off = bno;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* Now get the MBR */
	bp->b_blkno = bno;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylinder = bp->b_blkno / (lp->d_secsize / DEV_BSIZE) / lp->d_secpercyl;
	(*strat)(bp);

	if (biowait(bp))
		goto done;

	if (get_short(bp->b_data + MBRMAGICOFF) != MBRMAGIC)
		goto done;

	/* Extract info from MBR partition table */
	mp = (struct mbr_partition *)(bp->b_data + MBRPARTOFF);
	for (i = 0; i < NMBRPART; i++, mp++) {
		if (get_long(&mp->mbr_size)) {
			switch (mp->mbr_type) {
			case MBR_EXTENDED:
				if (*pnpart < MAXPARTITIONS) {
					pp = lp->d_partitions + *pnpart;
					bzero(pp, sizeof *pp);
					pp->p_size = get_long(&mp->mbr_size);
					pp->p_offset = off + get_long(&mp->mbr_start);
					++*pnpart;
				}
				if (found = mbr_to_label(dev, strat,
							 off + get_long(&mp->mbr_start),
							 lp, pnpart, osdep, off))
					goto done;
				break;
			case MBR_NETBSD:
				/* Found the real NetBSD partition, use it */
				osdep->cd_start = off + get_long(&mp->mbr_start);
				if (found = get_netbsd_label(dev, strat, lp, osdep->cd_start))
					goto done;
				/* FALLTHROUGH */
			default:
				if (*pnpart < MAXPARTITIONS) {
					pp = lp->d_partitions + *pnpart;
					bzero(pp, sizeof *pp);
					pp->p_size = get_long(&mp->mbr_size);
					pp->p_offset = off + get_long(&mp->mbr_start);
					++*pnpart;
				}
				break;
			}
		}
	}
done:
	recursion--;
	bp->b_flags |= B_INVAL;
	brelse(bp);
	return found;
}

/*
 * Attempt to read a disk label from a device
 * using the indicated strategy routine.
 *
 * If we can't find a NetBSD label, we attempt to fake one
 * based on the MBR (and extended partition) information
 */
char *
readdisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat)();
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	struct mbr_partition *mp;
	struct buf *bp;
	char *msg = 0;
	int i;

	/* Initialize disk label with some defaults */
	if (lp->d_secsize == 0)
		lp->d_secsize = DEV_BSIZE;
	if (lp->d_secpercyl == 0)
		lp->d_secpercyl = 1;
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x7fffffff;
	lp->d_npartitions = RAW_PART + 1;
	for (i = 0; i < MAXPARTITIONS; i++) {
		if (i != RAW_PART) {
			lp->d_partitions[i].p_size = 0;
			lp->d_partitions[i].p_offset = 0;
		}
	}
	if (lp->d_partitions[RAW_PART].p_size == 0) {
		lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;
		lp->d_partitions[RAW_PART].p_offset = 0;
	}

	osdep->cd_start = -1;

	mbr_to_label(dev, strat, MBRSECTOR, lp, &lp->d_npartitions, osdep, 0);
	return 0;
}

/*
 * Check new disk label for sensibility before setting it.
 */
int
setdisklabel(olp, nlp, openmask, osdep)
	struct disklabel *olp, *nlp;
	u_long openmask;
	struct cpu_disklabel *osdep;
{
	/* sanity clause */
	if (nlp->d_secpercyl == 0 || nlp->d_secsize == 0
	    || (nlp->d_secsize % DEV_BSIZE) != 0)
		return EINVAL;

	/* special case to allow disklabel to be invalidated */
	if (nlp->d_magic == 0xffffffff) {
		*olp = *nlp;
		return 0;
	}

	if (nlp->d_magic != DISKMAGIC || nlp->d_magic2 != DISKMAGIC
	    || dkcksum(nlp) != 0)
		return EINVAL;

	/* openmask parameter ignored */

	*olp = *nlp;
	return 0;
}

/*
 * Write disk label back to device after modification.
 */
int
writedisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat)();
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	struct buf *bp;
	int error;
	struct disklabel label;

	/*
	 * Try to re-read a disklabel, in case he changed the MBR.
	 */
	label = *lp;
	readdisklabel(dev, strat, &label, osdep);
	if (osdep->cd_start < 0)
		return EINVAL;

	/* get a buffer and initialize it */
	bp = geteblk(lp->d_secsize);
	bp->b_dev = dev;

	bp->b_blkno = osdep->cd_start + LABELSECTOR;
	bp->b_cylinder = bp->b_blkno / (lp->d_secsize / DEV_BSIZE) / lp->d_secpercyl;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_WRITE;

	bcopy((caddr_t)lp, (caddr_t)bp->b_data, sizeof *lp);

	(*strat)(bp);
	error = biowait(bp);

	bp->b_flags |= B_INVAL;
	brelse(bp);

	return error;
}

/*
 * Determine the size of the transfer, and make sure it is
 * within the boundaris of the partition.  Adjust transfer
 * if needed, and signal errors or early completion.
 */
int
bounds_check_with_label(bp, lp, wlabel)
	struct buf *bp;
	struct disklabel *lp;
	int wlabel;
{
	struct partition *p = lp->d_partitions + DISKPART(bp->b_dev);
	int sz;

	sz = howmany(bp->b_bcount, lp->d_secsize);

	if (bp->b_blkno + sz > p->p_size) {
		sz = p->p_size - bp->b_blkno;
		if (sz == 0) {
			/* If axactly at end of disk, return EOF. */
			bp->b_resid = bp->b_bcount;
			goto done;
		}
		if (sz < 0) {
			/* If past end of disk, return EINVAL. */
			bp->b_error = EINVAL;
			goto bad;
		}
		/* Otherwise truncate request. */
		bp->b_bcount = sz * lp->d_secsize;
	}

	/* calculate cylinder for disksort to order transfers with */
	bp->b_cylinder = (bp->b_blkno + p->p_offset)
			 / (lp->d_secsize / DEV_BSIZE) / lp->d_secpercyl;

	return 1;

bad:
	bp->b_flags |= B_ERROR;
done:
	return 0;
}

extern int dumpsize;
/*
 * This is called by configure to set dumplo and dumpsize.
 */
void
dumpconf()
{
	int nblks;		/* size of dump device */
	int skip;
	int maj;

	if (dumpdev == NODEV)
		return;
	maj = major(dumpdev);
	if (maj < 0 || maj >= nblkdev)
		panic("dumpconf: bad dumpdev=0x%x", dumpdev);
	if (bdevsw[maj].d_psize == NULL)
		return;
	nblks = (*bdevsw[maj].d_psize)(dumpdev);
	if (nblks <= ctod(1))
		return;

	dumpsize = physmem;

	/* Skip enough blocks at start of disk to preserve an eventual disklabel. */
	skip = LABELSECTOR + 1;
	skip += ctod(1) - 1;
	skip = ctod(dtoc(skip));
	if (dumplo < skip)
		dumplo = skip;

	/* Put dump at end of partition */
	if (dumpsize > dtoc(nblks - dumplo))
		dumpsize = dtoc(nblks - dumplo);
	if (dumplo < nblks - ctod(dumpsize))
		dumplo = nblks - ctod(dumpsize);
}

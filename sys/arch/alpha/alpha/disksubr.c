/*	$NetBSD: disksubr.c,v 1.4 1996/01/07 22:01:38 thorpej Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Keith Bostic, Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/ioccom.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/cpu.h>
#include <machine/autoconf.h>

extern struct device *bootdv;

/* was this the boot device ? */
int
dk_establish(dk, dev)
	struct disk *dk;
	struct device *dev;
{
#ifdef NOTDEF
	/* XXX: sd -> scsibus -> esp */
	struct bootpath *bp = ((struct esp_softc *)dev->dv_parent->dv_parent)->sc_bp;
	char name[10];

#define CRAZYMAP(v) ((v) == 3 ? 0 : (v) == 0 ? 3 : (v))

	if (bp == NULL) {
		printf("no boot path\n");
		return -1;
	}
	sprintf(name, "%s%d", bp->name, CRAZYMAP(bp->val[0]));
	if (strcmp(name, dev->dv_xname) == 0) {
		bootdv = dev;
	}
#endif
	return 1;
}

/*
 * Attempt to read a disk label from a device
 * using the indicated stategy routine.
 * The label must be partly set up before this:
 * secpercyl and anything required in the strategy routine
 * (e.g., sector size) must be filled in before calling us.
 * Returns null on success and an error string on failure.
 */
char *
readdisklabel(dev, strat, lp, clp)
	dev_t dev;
	void (*strat)();
	struct disklabel *lp;
	struct cpu_disklabel *clp;
{
	struct buf *bp;
	struct disklabel *dlp;
	char *msg = NULL;

	/* minimal requirements for archtypal disk label */
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff; 
	lp->d_npartitions = 1;
	if (lp->d_partitions[0].p_size == 0)
		lp->d_partitions[0].p_size = 0x1fffffff;
	lp->d_partitions[0].p_offset = 0;

	/* obtain buffer to probe drive with */
	bp = geteblk((int)lp->d_secsize);

	/* next, dig out disk label */
	bp->b_dev = dev;
	bp->b_blkno = LABELSECTOR;
	bp->b_resid = 0;			/* was b_cylin */
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);  

	/* if successful, locate disk label within block and validate */
	if (biowait(bp)) {
		msg = "disk label read error";
		goto done;
	}

	dlp = (struct disklabel *)(bp->b_un.b_addr + LABELOFFSET);
	if (dlp->d_magic == DISKMAGIC) {
		if (dkcksum(dlp)) {
			msg = "NetBSD disk label corrupted";
			goto done;
		}
		*lp = *dlp;
		goto done;
	}
	msg = "no disk label";
done:
	bp->b_flags = B_INVAL | B_AGE | B_READ;
	brelse(bp);
	return (msg);
}

/*
 * Check new disk label for sensibility before setting it.
 */
int
setdisklabel(olp, nlp, openmask, clp)
	register struct disklabel *olp, *nlp;
	u_long openmask;
	struct cpu_disklabel *clp;
{
	register i;
	register struct partition *opp, *npp;

	/* sanity clause */
	if (nlp->d_secpercyl == 0 || nlp->d_secsize == 0 ||
	    (nlp->d_secsize % DEV_BSIZE) != 0)
		return(EINVAL);

#ifdef notdef
	/* XXX WHY WAS THIS HERE?! */
	/* special case to allow disklabel to be invalidated */
	if (nlp->d_magic == 0xffffffff) { 
		*olp = *nlp;
		return (0);
	}
#endif

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
		* if new label doesn't include it.             XXX
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
 * this means write out the Rigid disk blocks to represent the 
 * label.  Hope the user was carefull.
 */
int
writedisklabel(dev, strat, lp, clp)
	dev_t dev;
	void (*strat)();
	register struct disklabel *lp;
	struct cpu_disklabel *clp;
{
	struct buf *bp; 
	struct disklabel *dlp;
	int error = 0, cyl, i;

	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;
	bp->b_blkno = LABELSECTOR;
	bp->b_resid = 0;			/* was b_cylin */
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_READ;           /* get current label */
	(*strat)(bp);
	if (error = biowait(bp))
		goto done;

	dlp = (struct disklabel *)(bp->b_un.b_addr + LABELOFFSET);
	*dlp = *lp;     /* struct assignment */

	/*
	 * The Alpha requires that the boot block be checksummed.
	 * The first 63 8-bit quantites are summed into the 64th.
	 */
	{
		int i;
		u_long *dp, sum;

		dp = (u_long *)bp->b_un.b_addr;
		sum = 0;
		for (i = 0; i < 63; i++)
			sum += dp[i];
		dp[63] = sum;
	}

	bp->b_flags = B_WRITE;
	(*strat)(bp);
	error = biowait(bp);

done:
	brelse(bp);
	return (error); 
}


/* 
 * Determine the size of the transfer, and make sure it is
 * within the boundaries of the partition. Adjust transfer
 * if needed, and signal errors or early completion.
 */
int
bounds_check_with_label(bp, lp, wlabel)
	struct buf *bp;
	struct disklabel *lp;
	int wlabel;
{
#define dkpart(dev) (minor(dev) & 7)

	struct partition *p = lp->d_partitions + dkpart(bp->b_dev);
	int labelsect = lp->d_partitions[0].p_offset;
	int maxsz = p->p_size;
	int sz = (bp->b_bcount + DEV_BSIZE - 1) >> DEV_BSHIFT;

	/* overwriting disk label ? */
	/* XXX should also protect bootstrap in first 8K */ 
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
	bp->b_resid = (bp->b_blkno + p->p_offset) / lp->d_secpercyl;
	return(1);
bad:
	bp->b_flags |= B_ERROR;
	return(-1);
}

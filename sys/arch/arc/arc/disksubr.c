/*	$OpenBSD: disksubr.c,v 1.2 1996/08/26 11:01:34 pefo Exp $	*/
/*	$NetBSD: disksubr.c,v 1.3 1995/04/22 12:43:22 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Keith Bostic, Chris G. Demetriou, Per Fogelstrom (R4000)
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
void
dk_establish(dk, dev)
	struct disk *dk;
	struct device *dev;
{
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
	struct dos_partition *dp = clp->dosparts;
	char *msg = NULL;
	int dospartoff = 0;
	int i;

	/* minimal requirements for archtypal disk label */
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff; 
	lp->d_npartitions = RAW_PART + 1;
	for(i = 0; i < RAW_PART; i++) {
		lp->d_partitions[i].p_size = 0;
		lp->d_partitions[i].p_offset = 0;
	}
	if (lp->d_partitions[RAW_PART].p_size == 0)
		lp->d_partitions[RAW_PART].p_size = 0x1fffffff;
	lp->d_partitions[RAW_PART].p_offset = 0;

	/* obtain buffer to probe drive with */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* do dos partitions in the process of getting disklabel? */
	if (dp) {
		/* read master boot record */
		bp->b_blkno = DOSBBSECTOR;
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		bp->b_resid = 0;
		(*strat)(bp);

		/* if successful, wander through dos partition table */
		if (biowait(bp)) {
			msg = "dos partition I/O error";
			goto done;
		} else if (*(unsigned int *)(bp->b_data) == 0x8ec033fa) {
			/* XXX how do we check veracity/bounds of this? */
			bcopy(bp->b_data + DOSPARTOFF, dp, NDOSPART * sizeof(*dp));
			for (i = 0; i < NDOSPART; i++, dp++) {
				/* is this ours? */
				if (dp->dp_size && dp->dp_typ == DOSPTYP_386BSD
				    && dospartoff == 0) {
					dospartoff = dp->dp_start;

					/* set part a to show NetBSD part */
					lp->d_partitions[0].p_size = dp->dp_size;
					lp->d_partitions[0].p_offset = dp->dp_start;
					lp->d_ntracks = dp->dp_ehd + 1;
					lp->d_nsectors = DPSECT(dp->dp_esect);
					lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
				}
			}
		}
			
	}
	/* next, dig out disk label */
	bp->b_blkno = dospartoff + LABELSECTOR;
	bp->b_resid = 0;
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
	struct dos_partition *dp = clp->dosparts;
	int error = 0, i;
	int dospartoff = 0;

	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* do dos partitions in the process of getting disklabel? */
	if (dp) {
		/* read master boot record */
		bp->b_blkno = DOSBBSECTOR;
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		bp->b_resid = 0;
		(*strat)(bp);

		if (((error = biowait(bp)) == 0) 
		   && *(unsigned int *)(bp->b_data) == 0x8ec033fa) {
			/* XXX how do we check veracity/bounds of this? */
			bcopy(bp->b_data + DOSPARTOFF, dp, NDOSPART * sizeof(*dp));
			for (i = 0; i < NDOSPART; i++, dp++) {
				/* is this ours? */
				if (dp->dp_size && dp->dp_typ == DOSPTYP_386BSD
				    && dospartoff == 0) {
					dospartoff = dp->dp_start;
				}
			}
		}
			
	}
	bp->b_blkno = dospartoff + LABELSECTOR;
	bp->b_resid = 0;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_READ;           /* get current label */
	(*strat)(bp);
	if (error = biowait(bp))
		goto done;

	dlp = (struct disklabel *)(bp->b_un.b_addr + LABELOFFSET);
	*dlp = *lp;     /* struct assignment */

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
#define dkpart(dev) (minor(dev) % MAXPARTITIONS )

	struct partition *p = lp->d_partitions + dkpart(bp->b_dev);
	int labelsect = lp->d_partitions[RAW_PART].p_offset;
	int maxsz = p->p_size;
	int sz = (bp->b_bcount + DEV_BSIZE - 1) >> DEV_BSHIFT;

	/* overwriting disk label ? */
	/* XXX should also protect bootstrap in first 8K */ 
	if (bp->b_blkno + p->p_offset == LABELSECTOR + labelsect &&
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

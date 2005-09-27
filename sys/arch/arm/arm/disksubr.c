/*	$OpenBSD: disksubr.c,v 1.5 2005/09/27 23:56:11 krw Exp $	*/
/*	$NetBSD: disksubr.c,v 1.21 1996/05/03 19:42:03 christos Exp $	*/

/*
 * Copyright (c) 1996 Theo de Raadt
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
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>
#include <sys/disk.h>

void
dk_establish(dk, dev)
	struct disk *dk;
	struct device *dev;
{
}

/*
 * Attempt to read a disk label from a device
 * using the indicated strategy routine.
 * The label must be partly set up before this:
 * secpercyl, secsize and anything required for a block i/o read
 * operation in the driver's strategy/start routines
 * must be filled in before calling us.
 *
 * If dos partition table requested, attempt to load it and
 * find disklabel inside a DOS partition. Also, if bad block
 * table needed, attempt to extract it as well. Return buffer
 * for use in signalling errors if requested.
 *
 * We would like to check if each MBR has a valid DOSMBR_SIGNATURE, but
 * we cannot because it doesn't always exist. So.. we assume the
 * MBR is valid.
 *
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
	struct dos_partition *dp = osdep->dosparts, *dp2;
	struct buf *bp = NULL;
	struct disklabel *dlp;
	char *msg = NULL, *cp;
	int dospartoff, cyl, i, ourpart = -1;

	/* minimal requirements for archtypal disk label */
	if (lp->d_secsize < DEV_BSIZE)
		lp->d_secsize = DEV_BSIZE;
	if (lp->d_secpercyl == 0) {
		msg = "invalid geometry";
		goto done;
	}
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

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* do dos partitions in the process of getting disklabel? */
	dospartoff = 0;
	cyl = LABELSECTOR / lp->d_secpercyl;
	if (dp) {
	        daddr_t part_blkno = DOSBBSECTOR;
		unsigned long extoff = 0;
		int wander = 1, n = 0, loop = 0;

		/*
		 * Read dos partition table, follow extended partitions.
		 * Map the partitions to disklabel entries i-p
		 */
		while (wander && n < 8 && loop < 8) {
		        loop++;
			wander = 0;
			if (part_blkno < extoff)
				part_blkno = extoff;

			if (spoofonly) {
				bzero(dp, NDOSPART * sizeof(*dp));
				goto donot;
			}

			/* read boot record */
			bp->b_blkno = part_blkno;
			bp->b_bcount = lp->d_secsize;
			bp->b_flags = B_BUSY | B_READ;
			bp->b_cylinder = part_blkno / lp->d_secpercyl;
			(*strat)(bp);
		     
			/* if successful, wander through dos partition table */
			if (biowait(bp)) {
				msg = "dos partition I/O error";
				goto done;
			}
			bcopy(bp->b_data + DOSPARTOFF, dp, NDOSPART * sizeof(*dp));

			if (ourpart == -1) {
				/* Search for our MBR partition */
				for (dp2=dp, i=0; i < NDOSPART && ourpart == -1;
				    i++, dp2++)
					if (get_le(&dp2->dp_size) &&
					    dp2->dp_typ == DOSPTYP_OPENBSD)
						ourpart = i;
				for (dp2=dp, i=0; i < NDOSPART && ourpart == -1;
				    i++, dp2++)
					if (get_le(&dp2->dp_size) &&
					    dp2->dp_typ == DOSPTYP_FREEBSD)
						ourpart = i;
				for (dp2=dp, i=0; i < NDOSPART && ourpart == -1;
				    i++, dp2++)
					if (get_le(&dp2->dp_size) &&
					    dp2->dp_typ == DOSPTYP_NETBSD)
						ourpart = i;
				if (ourpart == -1)
					goto donot;
				/*
				 * This is our MBR partition. need sector address
				 * for SCSI/IDE, cylinder for ESDI/ST506/RLL
				 */
				dp2 = &dp[ourpart];
				dospartoff = get_le(&dp2->dp_start) + part_blkno;
				cyl = DPCYL(dp2->dp_scyl, dp2->dp_ssect);

				/* XXX build a temporary disklabel */
				lp->d_partitions[0].p_size = get_le(&dp2->dp_size);
				lp->d_partitions[0].p_offset =
					get_le(&dp2->dp_start) + part_blkno;
				if (lp->d_ntracks == 0)
					lp->d_ntracks = dp2->dp_ehd + 1;
				if (lp->d_nsectors == 0)
					lp->d_nsectors = DPSECT(dp2->dp_esect);
				if (lp->d_secpercyl == 0)
					lp->d_secpercyl = lp->d_ntracks *
					    lp->d_nsectors;
			}
donot:
			/*
			 * In case the disklabel read below fails, we want to
			 * provide a fake label in i-p.
			 */
			for (dp2=dp, i=0; i < NDOSPART && n < 8; i++, dp2++) {
				struct partition *pp = &lp->d_partitions[8+n];

				if (dp2->dp_typ == DOSPTYP_OPENBSD)
					continue;
				if (get_le(&dp2->dp_size) > lp->d_secperunit)
					continue;
				if (get_le(&dp2->dp_size))
					pp->p_size = get_le(&dp2->dp_size);
				if (get_le(&dp2->dp_start))
					pp->p_offset =
					    get_le(&dp2->dp_start) + part_blkno;

				switch (dp2->dp_typ) {
				case DOSPTYP_UNUSED:
					for (cp = (char *)dp2;
					    cp < (char *)(dp2 + 1); cp++)
						if (*cp)
							break;
					/*
					 * Was it all zeroes?  If so, it is
					 * an unused entry that we don't
					 * want to show.
					 */
					if (cp == (char *)(dp2 + 1))
					    continue;
					lp->d_partitions[8 + n++].p_fstype =
					    FS_UNUSED;
					break;

				case DOSPTYP_LINUX:
					pp->p_fstype = FS_EXT2FS;
					n++;
					break;

				case DOSPTYP_FAT12:
				case DOSPTYP_FAT16S:
				case DOSPTYP_FAT16B:
				case DOSPTYP_FAT32:
				case DOSPTYP_FAT32L:
					pp->p_fstype = FS_MSDOS;
					n++;
					break;
				case DOSPTYP_EXTEND:
				case DOSPTYP_EXTENDL:
					part_blkno = get_le(&dp2->dp_start) + extoff;
					if (!extoff) {
						extoff = get_le(&dp2->dp_start);
						part_blkno = 0;
					}
					wander = 1;
					break;
				default:
					pp->p_fstype = FS_OTHER;
					n++;
					break;
				}
			}
		}
		lp->d_bbsize = 8192;
		lp->d_sbsize = 64*1024;		/* XXX ? */
		lp->d_npartitions = MAXPARTITIONS;
	}

	/* don't read the on-disk label if we are in spoofed-only mode */
	if (spoofonly)
		goto done;

	/* next, dig out disk label */
	bp->b_blkno = dospartoff + LABELSECTOR;
	bp->b_cylinder = cyl;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);

	/* if successful, locate disk label within block and validate */
	if (biowait(bp)) {
		/* XXX we return the faked label built so far */
		msg = "disk label I/O error";
		goto done;
	}
	for (dlp = (struct disklabel *)bp->b_data;
	    dlp <= (struct disklabel *)(bp->b_data + lp->d_secsize - sizeof(*dlp));
	    dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic != DISKMAGIC || dlp->d_magic2 != DISKMAGIC) {
			if (msg == NULL)
				msg = "no disk label";
		} else if (dlp->d_npartitions > MAXPARTITIONS ||
			   dkcksum(dlp) != 0)
			msg = "disk label corrupted";
		else {
			*lp = *dlp;
			msg = NULL;
			break;
		}
	}

	if (msg) {
#if defined(CD9660)
		if (iso_disklabelspoof(dev, strat, lp) == 0)
			msg = NULL;
#endif
#if defined(UDF)
		if (msg && udf_disklabelspoof(dev, strat, lp) == 0)
			msg = NULL;
#endif
		goto done;
	}

	/* obtain bad sector table if requested and present */

done:
	if (bp) {
		bp->b_flags |= B_INVAL;
		brelse(bp);
	}
	return (msg);
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
		return(EINVAL);

	/* special case to allow disklabel to be invalidated */
	if (nlp->d_magic == 0xffffffff) {
		*olp = *nlp;
		return (0);
	}

	if (nlp->d_magic != DISKMAGIC || nlp->d_magic2 != DISKMAGIC ||
	    dkcksum(nlp) != 0)
		return (EINVAL);

	/* XXX missing check if other dos partitions will be overwritten */

	while (openmask != 0) {
		i = ffs(openmask) - 1;
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
 * XXX cannot handle OpenBSD partitions in extended partitions!
 */
int
writedisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat)(struct buf *);
	register struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	struct dos_partition *dp = osdep->dosparts, *dp2;
	struct buf *bp;
	struct disklabel *dlp;
	int error, dospartoff, cyl, i;
	int ourpart = -1;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* do dos partitions in the process of getting disklabel? */
	dospartoff = 0;
	cyl = LABELSECTOR / lp->d_secpercyl;
	if (dp) {
		/* read master boot record */
		bp->b_blkno = DOSBBSECTOR;
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		bp->b_cylinder = DOSBBSECTOR / lp->d_secpercyl;
		(*strat)(bp);

		if ((error = biowait(bp)) != 0)
			goto done;

		/* XXX how do we check veracity/bounds of this? */
		bcopy(bp->b_data + DOSPARTOFF, dp,
		    NDOSPART * sizeof(*dp));

		for (dp2=dp, i=0; i < NDOSPART && ourpart == -1; i++, dp2++)
			if (get_le(&dp2->dp_size) && dp2->dp_typ == DOSPTYP_OPENBSD)
				ourpart = i;
		for (dp2=dp, i=0; i < NDOSPART && ourpart == -1; i++, dp2++)
			if (get_le(&dp2->dp_size) && dp2->dp_typ == DOSPTYP_FREEBSD)
				ourpart = i;
		for (dp2=dp, i=0; i < NDOSPART && ourpart == -1; i++, dp2++)
			if (get_le(&dp2->dp_size) && dp2->dp_typ == DOSPTYP_NETBSD)
				ourpart = i;

		if (ourpart != -1) {
			dp2 = &dp[ourpart];

			/*
			 * need sector address for SCSI/IDE,
			 * cylinder for ESDI/ST506/RLL
			 */
			dospartoff = get_le(&dp2->dp_start);
			cyl = DPCYL(dp2->dp_scyl, dp2->dp_ssect);
		}
	}

	/* next, dig out disk label */
	bp->b_blkno = dospartoff + LABELSECTOR;
	bp->b_cylinder = cyl;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);

	/* if successful, locate disk label within block and validate */
	if ((error = biowait(bp)) != 0)
		goto done;
	for (dlp = (struct disklabel *)bp->b_data;
	    dlp <= (struct disklabel *)(bp->b_data + lp->d_secsize - sizeof(*dlp));
	    dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic == DISKMAGIC && dlp->d_magic2 == DISKMAGIC &&
		    dkcksum(dlp) == 0) {
			*dlp = *lp;
			bp->b_flags = B_BUSY | B_WRITE;
			(*strat)(bp);
			error = biowait(bp);
			goto done;
		}
	}

	/* Write it in the regular place. */
	*(struct disklabel *)bp->b_data = *lp;
	bp->b_flags = B_BUSY | B_WRITE;
	(*strat)(bp);
	error = biowait(bp);
	goto done;

done:
	bp->b_flags |= B_INVAL;
	brelse(bp);
	return (error);
}

/*
 * Determine the size of the transfer, and make sure it is
 * within the boundaries of the partition. Adjust transfer
 * if needed, and signal errors or early completion.
 */
int
bounds_check_with_label(bp, lp, osdep, wlabel)
	struct buf *bp;
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
	int wlabel;
{
#define blockpersec(count, lp) ((count) * (((lp)->d_secsize) / DEV_BSIZE))
	struct partition *p = lp->d_partitions + DISKPART(bp->b_dev);
	int labelsector = blockpersec(lp->d_partitions[RAW_PART].p_offset, lp) +
	    LABELSECTOR;
	int sz = howmany(bp->b_bcount, DEV_BSIZE);

	/* avoid division by zero */
	if (lp->d_secpercyl == 0) {
		bp->b_error = EINVAL;
		goto bad;
	}

	/* beyond partition? */
	if (bp->b_blkno + sz > blockpersec(p->p_size, lp)) {
		sz = blockpersec(p->p_size, lp) - bp->b_blkno;
		if (sz == 0) {
			/* If exactly at end of disk, return EOF. */
			bp->b_resid = bp->b_bcount;
			goto done;
		}
		if (sz < 0) {
			/* If past end of disk, return EINVAL. */
			bp->b_error = EINVAL;
			goto bad;
		}
		/* Otherwise, truncate request. */
		bp->b_bcount = sz << DEV_BSHIFT;
	}

	/* Overwriting disk label? */
	if (bp->b_blkno + blockpersec(p->p_offset, lp) <= labelsector &&
#if LABELSECTOR != 0
	    bp->b_blkno + blockpersec(p->p_offset, lp) + sz > labelsector &&
#endif
	    (bp->b_flags & B_READ) == 0 && !wlabel) {
		bp->b_error = EROFS;
		goto bad;
	}

	/* calculate cylinder for disksort to order transfers with */
	bp->b_cylinder = (bp->b_blkno + blockpersec(p->p_offset, lp)) /
		lp->d_secpercyl;
	return (1);

bad:
	bp->b_flags |= B_ERROR;
done:
	return (0);
}

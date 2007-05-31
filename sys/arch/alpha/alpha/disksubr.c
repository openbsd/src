/*	$OpenBSD: disksubr.c,v 1.70 2007/05/31 16:05:50 krw Exp $	*/
/*	$NetBSD: disksubr.c,v 1.21 1996/05/03 19:42:03 christos Exp $	*/

/*
 * Copyright (c) 1997 Niklas Hallqvist
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

/*
 * This disksubr.c module started to take its present form on OpenBSD/alpha
 * but it was always thought it should be made completely MI and not need to
 * be in that alpha-specific tree at all.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>
#include <sys/disk.h>

char   *readbsdlabel(struct buf *, void (*)(struct buf *), int, int,
    int, struct disklabel *, int);
char   *readdoslabel(struct buf *, void (*)(struct buf *),
    struct disklabel *, struct cpu_disklabel *, int *, int *, int);

/*
 * Try to read a standard BSD disklabel at a certain sector.
 */
char *
readbsdlabel(bp, strat, cyl, sec, off, lp, spoofonly)
	struct buf *bp;
	void (*strat)(struct buf *);
	int cyl, sec, off;
	struct disklabel *lp;
	int spoofonly;
{
	struct disklabel *dlp;
	char *msg = NULL;
	u_int16_t cksum;

	/* don't read the on-disk label if we are in spoofed-only mode */
	if (spoofonly)
		return (NULL);

	bp->b_blkno = sec;
	bp->b_cylinder = cyl;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);

	/* if successful, locate disk label within block and validate */
	if (biowait(bp)) {
		/* XXX we return the faked label built so far */
		msg = "disk label I/O error";
		return (msg);
	}

	/*
	 * If off is negative, search until the end of the sector for
	 * the label, otherwise, just look at the specific location
	 * we're given.
	 */
	dlp = (struct disklabel *)(bp->b_data + (off >= 0 ? off : 0));
	do {
		if (dlp->d_magic != DISKMAGIC || dlp->d_magic2 != DISKMAGIC) {
			if (msg == NULL)
				msg = "no disk label";
		} else {
			cksum = dkcksum(dlp);
			if (dlp->d_npartitions > MAXPARTITIONS || cksum != 0) {
				msg = "disk label corrupted";
			} else {
				*lp = *dlp;
				msg = NULL;
				break;
			}
		}
		if (off >= 0)
			break;
		dlp = (struct disklabel *)((char *)dlp + sizeof(int32_t));
	} while (dlp <= (struct disklabel *)(bp->b_data + lp->d_secsize -
	    sizeof(*dlp)));
	return (msg);
}

/*
 * Attempt to read a disk label from a device
 * using the indicated strategy routine.
 * The label must be partly set up before this:
 * secpercyl, secsize and anything required for a block i/o read
 * operation in the driver's strategy/start routines
 * must be filled in before calling us.
 *
 * Returns null on success and an error string on failure.
 */
char *
readdisklabel(dev, strat, lp, osdep, spoofonly)
	dev_t dev;
	void (*strat)(struct buf *);
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
	int spoofonly;
{
	struct buf *bp = NULL;
	char *msg = "no disk label";
	int i;
	struct disklabel minilabel, fallbacklabel;

	/* minimal requirements for archetypal disk label */
	if (lp->d_secsize < DEV_BSIZE)
		lp->d_secsize = DEV_BSIZE;
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;
	if (lp->d_secpercyl == 0) {
		msg = "invalid geometry";
		goto done;
	}
	lp->d_npartitions = RAW_PART + 1;
	for (i = 0; i < RAW_PART; i++) {
		lp->d_partitions[i].p_size = 0;
		lp->d_partitions[i].p_offset = 0;
	}
	if (lp->d_partitions[i].p_size == 0)
		lp->d_partitions[i].p_size = lp->d_secperunit;
	lp->d_partitions[i].p_offset = 0;
	minilabel = fallbacklabel = *lp;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	msg = readbsdlabel(bp, strat, 0, ALPHA_LABELSECTOR, ALPHA_LABELOFFSET,
	    lp, spoofonly);
	if (msg)
		*lp = minilabel;
	if (msg) {
		msg = readdoslabel(bp, strat, lp, osdep, 0, 0, spoofonly);
		if (msg) {
			/* Fallback alternative */
			fallbacklabel = *lp;
			*lp = minilabel;
		}
	}
	/* Record metainformation about the disklabel.  */
	if (msg == NULL) {
		osdep->labelsector = bp->b_blkno;
	}

#if defined(CD9660)
	if (msg && iso_disklabelspoof(dev, strat, lp) == 0)
		msg = NULL;
#endif
#if defined(UDF)
	if (msg && udf_disklabelspoof(dev, strat, lp) == 0)
		msg = NULL;
#endif

	/* If there was an error, still provide a decent fake one.  */
	if (msg)
		*lp = fallbacklabel;

done:
	if (bp) {
		bp->b_flags |= B_INVAL;
		brelse(bp);
	}
	cvtdisklabelv1(lp);
	return (msg);
}

/*
 * If dos partition table requested, attempt to load it and
 * find disklabel inside a DOS partition. Return buffer
 * for use in signalling errors if requested.
 *
 * We would like to check if each MBR has a valid BOOT_MAGIC, but
 * we cannot because it doesn't always exist. So.. we assume the
 * MBR is valid.
 */
char *
readdoslabel(bp, strat, lp, osdep, partoffp, cylp, spoofonly)
	struct buf *bp;
	void (*strat)(struct buf *);
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
	int *partoffp;
	int *cylp;
	int spoofonly;
{
	struct dos_partition dp[NDOSPART], *dp2;
	struct partition *pp;
	unsigned long extoff = 0;
	unsigned int fattest;
	daddr_t part_blkno = DOSBBSECTOR;
	char *msg = NULL;
	int dospartoff, cyl, i, ourpart = -1;
	int wander = 1, n = 0, loop = 0;

	if (lp->d_secpercyl == 0) {
		msg = "invalid label, d_secpercyl == 0";
		return (msg);
	}
	if (lp->d_secsize == 0) {
		msg = "invalid label, d_secsize == 0";
		return (msg);
	}

	/* do dos partitions in the process of getting disklabel? */
	dospartoff = 0;
	cyl = I386_LABELSECTOR / lp->d_secpercyl;

	/*
	 * Read dos partition table, follow extended partitions.
	 * Map the partitions to disklabel entries i-p
	 */
	while (wander && n < 8 && loop < 8) {
		loop++;
		wander = 0;
		if (part_blkno < extoff)
			part_blkno = extoff;

		/* read boot record */
		bp->b_blkno = part_blkno;
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		bp->b_cylinder = part_blkno / lp->d_secpercyl;
		(*strat)(bp);

		/* if successful, wander through dos partition table */
		if (biowait(bp)) {
			msg = "dos partition I/O error";
			if (partoffp)
				*partoffp = -1;
			return (msg);
		}
		bcopy(bp->b_data + DOSPARTOFF, dp, sizeof(dp));

		if (ourpart == -1 && part_blkno == DOSBBSECTOR) {
			/* Search for our MBR partition */
			for (dp2=dp, i=0; i < NDOSPART && ourpart == -1;
			    i++, dp2++)
				if (letoh32(dp2->dp_size) &&
				    dp2->dp_typ == DOSPTYP_OPENBSD)
					ourpart = i;
			if (ourpart == -1)
				goto donot;
			/*
			 * This is our MBR partition. need sector
			 * address for SCSI/IDE, cylinder for
			 * ESDI/ST506/RLL
			 */
			dp2 = &dp[ourpart];
			dospartoff = letoh32(dp2->dp_start) + part_blkno;
			cyl = DPCYL(dp2->dp_scyl, dp2->dp_ssect);

			/* XXX build a temporary disklabel */
			lp->d_partitions[0].p_size = letoh32(dp2->dp_size);
			lp->d_partitions[0].p_offset =
			    letoh32(dp2->dp_start) + part_blkno;
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
			pp = &lp->d_partitions[8+n];

			if (dp2->dp_typ == DOSPTYP_OPENBSD)
				continue;
			if (letoh32(dp2->dp_size) > lp->d_secperunit)
				continue;
			if (letoh32(dp2->dp_start) > lp->d_secperunit)
				continue;
			if (letoh32(dp2->dp_size) == 0)
				continue;
			if (letoh32(dp2->dp_start))
				pp->p_offset =
				    letoh32(dp2->dp_start) + part_blkno;

			pp->p_size = letoh32(dp2->dp_size);

			switch (dp2->dp_typ) {
			case DOSPTYP_UNUSED:
				pp->p_fstype = FS_UNUSED;
				n++;
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
			case DOSPTYP_FAT16L:
				pp->p_fstype = FS_MSDOS;
				n++;
				break;
			case DOSPTYP_EXTEND:
			case DOSPTYP_EXTENDL:
				part_blkno =
				    letoh32(dp2->dp_start) + extoff;
				if (!extoff) {
					extoff = letoh32(dp2->dp_start);
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

	if (n == 0 && part_blkno == DOSBBSECTOR) {
		/* Check for a short jump instruction. */
		fattest = ((bp->b_data[0] << 8) & 0xff00) | (bp->b_data[2] &
		    0xff);
		if (fattest != 0xeb90 && fattest != 0xe900)
			goto notfat;

		/* Check for a valid bytes per sector value. */
		fattest = ((bp->b_data[12] << 8) & 0xff00) | (bp->b_data[11] &
		    0xff);
		if (fattest < 512 || fattest > 4096 || (fattest % 512 != 0))
			goto notfat;

		/* Check the end of sector marker. */
		fattest = ((bp->b_data[510] << 8) & 0xff00) | (bp->b_data[511] &
		    0xff);
		if (fattest != 0x55aa)
			goto notfat;

		/* Looks like a FAT filesystem. Spoof 'i'. */
		lp->d_partitions['i' - 'a'].p_size =
		    lp->d_partitions[RAW_PART].p_size;
		lp->d_partitions['i' - 'a'].p_offset = 0;
		lp->d_partitions['i' - 'a'].p_fstype = FS_MSDOS;
	}
notfat:

	/* record the OpenBSD partition's placement for the caller */
	if (partoffp)
		*partoffp = dospartoff;
	if (cylp)
		*cylp = cyl;

	/* next, dig out disk label */
	msg = readbsdlabel(bp, strat, cyl, dospartoff + I386_LABELSECTOR, -1,
	    lp, spoofonly);

	return (msg);
}

/*
 * Check new disk label for sensibility
 * before setting it.
 */
int
setdisklabel(olp, nlp, openmask, osdep)
	struct disklabel *olp, *nlp;
	u_long openmask;
	struct cpu_disklabel *osdep;
{
	int i;
	struct partition *opp, *npp;

	/* sanity clause */
	if (nlp->d_secpercyl == 0 || nlp->d_secsize == 0 ||
	    (nlp->d_secsize % DEV_BSIZE) != 0)
		return(EINVAL);

	/*
	 * XXX Nice thought, but it doesn't work, if the intention was to
	 * force a reread at the next *readdisklabel call.  That does not
	 * happen.  There's still some use for it though as you can pseudo-
	 * partition the disk.
	 *
	 * Special case to allow disklabel to be invalidated.
	 */
	if (nlp->d_magic == 0xffffffff) {
		*olp = *nlp;
		return (0);
	}

	if (nlp->d_magic != DISKMAGIC || nlp->d_magic2 != DISKMAGIC ||
	    dkcksum(nlp) != 0)
		return (EINVAL);

	/* XXX missing check if other dos partitions will be overwritten */

	while (openmask != 0) {
		i = ffs((long)openmask) - 1;
		openmask &= ~(1 << i);
		if (nlp->d_npartitions <= i)
			return (EBUSY);
		opp = &olp->d_partitions[i];
		npp = &nlp->d_partitions[i];
		if (npp->p_offset != opp->p_offset ||
		    npp->p_size < opp->p_size)
			return (EBUSY);
		/*
		 * Copy internally-set partition information
		 * if new label doesn't include it.		XXX
		 */
		if (npp->p_fstype == FS_UNUSED && opp->p_fstype != FS_UNUSED) {
			npp->p_fstype = opp->p_fstype;
			npp->p_fragblock = opp->p_fragblock;
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
 */
int
writedisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat)(struct buf *);
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	char *msg = "no disk label";
	struct buf *bp;
	struct disklabel dl;
	struct cpu_disklabel cdl;
	int labeloffset, error, i, partoff = 0, cyl = 0, needcsum = 0;
	u_int64_t csum, *p;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/*
	 * I once played with the thought of using osdep->label{tag,sector}
	 * as a cache for knowing where (and what) to write.  However, now I
	 * think it might be useful to reprobe if someone has written
	 * a newer disklabel of another type with disklabel(8) and -r.
	 */
	dl = *lp;
	msg = readbsdlabel(bp, strat, 0, ALPHA_LABELSECTOR, ALPHA_LABELOFFSET,
	    &dl, 0);
	labeloffset = ALPHA_LABELOFFSET;
	if (msg == NULL)
		needcsum = 1;
	if (msg) {
		dl = *lp;
		msg = readdoslabel(bp, strat, &dl, &cdl, &partoff, &cyl, 0);
		labeloffset = I386_LABELOFFSET;
	}
	if (msg) {
		if (partoff == -1)
			return EIO;

		/* Write it in the regular place with native byte order. */
		labeloffset = LABELOFFSET;
		bp->b_blkno = partoff + LABELSECTOR;
		bp->b_cylinder = cyl;
		bp->b_bcount = lp->d_secsize;
	}

	*(struct disklabel *)(bp->b_data + labeloffset) = *lp;

	/* Alpha bootblocks are checksummed. */
	if (needcsum) {
		for (csum = i = 0, p = (u_int64_t *)bp->b_data; i < 63; i++)
			csum += *p++;
		*p = csum;
	}

	bp->b_flags = B_BUSY | B_WRITE;
	(*strat)(bp);
	error = biowait(bp);

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
	int labelsector = blockpersec(lp->d_partitions[RAW_PART].p_offset,
	    lp) + osdep->labelsector;
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
	    bp->b_blkno + blockpersec(p->p_offset, lp) + sz > labelsector &&
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
	return (-1);
}

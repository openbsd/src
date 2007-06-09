/*	$OpenBSD: disksubr.c,v 1.48 2007/06/09 04:08:39 deraadt Exp $	*/
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

#define BOOT_MAGIC 0xAA55
#define BOOT_MAGIC_OFF (DOSPARTOFF+NDOSPART*sizeof(struct dos_partition))

/*
 * Attempt to read a disk label from a device
 * using the indicated strategy routine.
 * The label must be partly set up before this:
 * secpercyl, secsize and anything required for a block i/o read
 * operation in the driver's strategy/start routines
 * must be filled in before calling us.
 *
 * If dos partition table requested, attempt to load it and
 * find disklabel inside a DOS partition. Return buffer
 * for use in signalling errors if requested.
 *
 * We would like to check if each MBR has a valid BOOT_MAGIC, but
 * we cannot because it doesn't always exist. So.. we assume the
 * MBR is valid.
 *
 * Returns null on success and an error string on failure.
 */
char *
readdisklabel(dev_t dev, void (*strat)(struct buf *),
    struct disklabel *lp, struct cpu_disklabel *osdep, int spoofonly)
{
	struct dos_partition dp[NDOSPART], *dp2;
	struct partition *pp;
	struct disklabel *dlp;
	unsigned long extoff = 0;
	unsigned int fattest;
	struct buf *bp = NULL;
	daddr64_t part_blkno = DOSBBSECTOR;
	char *msg = NULL;
	char *s;
	int dospartoff, cyl, i, ourpart = -1;
	int wander = 1, loop = 0;
	/* HFS variables */
	int part_cnt, n, hfspartoff;
	struct part_map_entry *part;


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
	if (DL_GETPSIZE(&lp->d_partitions[i]) == 0)
		DL_SETPSIZE(&lp->d_partitions[i], DL_GETDSIZE(lp));
	DL_SETPOFFSET(&lp->d_partitions[i], 0);

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* DPME (HFS) disklabel */

	bp->b_blkno = 1;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylinder = 1 / lp->d_secpercyl;
	(*strat)(bp);

	/* if successful, wander through DPME partition table */
	if (biowait(bp)) {
		msg = "DPME partition I/O error";
		goto hfs_done;
	}

	part = (struct part_map_entry *)bp->b_data;
	/* if first partition is not valid, assume not HFS/DPME partitioned */
        if (part->pmSig != PART_ENTRY_MAGIC) {
		osdep->macparts[0].pmSig = 0; /* make invalid */
		goto hfs_done;
        }
	osdep->macparts[0] = *part;
	part_cnt = part->pmMapBlkCnt;
	n = 0;
	for (i = 0; i < part_cnt; i++) {
		pp = &lp->d_partitions[8+n];

		bp->b_blkno = 1+i;
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		bp->b_cylinder = 1+i / lp->d_secpercyl;
		(*strat)(bp);

		if (biowait(bp)) {
			msg = "DPME partition I/O error";
			goto hfs_done;
		}
		part = (struct part_map_entry *)bp->b_data;
		/* toupper the string, in case caps are different... */
		for (s = part->pmPartType; *s; s++)
			if ((*s >= 'a') && (*s <= 'z'))
				*s = (*s - 'a' + 'A');

		if (strcmp(part->pmPartType, PART_TYPE_OPENBSD) == 0) {
			hfspartoff = part->pmPyPartStart;
			osdep->macparts[1] = *part;
		}
		/* currently we ignore all but HFS partitions */
		if (strcmp(part->pmPartType, PART_TYPE_MAC) == 0) {
			DL_SETPOFFSET(pp, part->pmPyPartStart);
			DL_SETPSIZE(pp, part->pmPartBlkCnt);
			pp->p_fstype = FS_HFS;
			n++;
		}
	}
	lp->d_npartitions = MAXPARTITIONS;

	/* don't read the on-disk label if we are in spoofed-only mode */
	if (spoofonly)
		goto done;

	/* next, dig out disk label */
	bp->b_blkno = hfspartoff;
	bp->b_cylinder = hfspartoff/lp->d_secpercyl; /* XXX */
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);

	/* if successful, locate disk label within block and validate */
	if (biowait(bp)) {
		/* XXX we return the faked label built so far */
		msg = "disk label I/O error";
		goto done;
	}
	goto found_disklabel;


hfs_done:
	/* MBR type disklabel */
	/* do dos partitions in the process of getting disklabel? */
	dospartoff = 0;
	cyl = LABELSECTOR / lp->d_secpercyl;
	n = 0;

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
			goto done;
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
			 * This is our MBR partition.
			 * need sector address * for SCSI/IDE,
			 * cylinder for ESDI/ST506/RLL
			 */
			dp2 = &dp[ourpart];
			dospartoff = letoh32(dp2->dp_start) +
			    part_blkno;
			cyl = DPCYL(dp2->dp_scyl, dp2->dp_ssect);

			/* XXX build a temporary disklabel */
			DL_SETPSIZE(&lp->d_partitions[0], letoh32(dp2->dp_size));
			DL_SETPOFFSET(&lp->d_partitions[0],
			    letoh32(dp2->dp_start) + part_blkno);
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
			if (letoh32(dp2->dp_size) > DL_GETDSIZE(lp))
				continue;
			if (letoh32(dp2->dp_start) > DL_GETDSIZE(lp))
				continue;
			if (letoh32(dp2->dp_size) == 0)
				continue;
			if (letoh32(dp2->dp_start))
				DL_SETPOFFSET(pp,
				    letoh32(dp2->dp_start) + part_blkno);

			DL_SETPSIZE(pp, letoh32(dp2->dp_size));

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
			case DOSPTYP_FAT16L:
			case DOSPTYP_FAT32:
				pp->p_fstype = FS_MSDOS;
				n++;
				break;
			case DOSPTYP_EXTEND:
			case DOSPTYP_EXTENDL:
				part_blkno = letoh32(dp2->dp_start) + extoff;
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
		DL_SETPSIZE(&lp->d_partitions['i' - 'a'],
		    DL_GETPSIZE(&lp->d_partitions[RAW_PART]));
		DL_SETPOFFSET(&lp->d_partitions['i' - 'a'], 0);
		lp->d_partitions['i' - 'a'].p_fstype = FS_MSDOS;
	}
notfat:

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

found_disklabel:
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
			DL_SETDSIZE(dlp, DL_GETDSIZE(lp));
			*lp = *dlp;
			msg = NULL;
			break;
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
 * XXX cannot handle OpenBSD partitions in extended partitions!
 */
int
writedisklabel(dev_t dev, void (*strat)(struct buf *),
    struct disklabel *lp, struct cpu_disklabel *osdep)
{
	struct dos_partition dp[NDOSPART], *dp2;
	struct disklabel *dlp;
	struct buf *bp;
	int error, dospartoff, cyl, i;
	int ourpart = -1;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* try DPME partition */
	if (osdep->macparts[0].pmSig == PART_ENTRY_MAGIC) {
		/* only write if a valid "OpenBSD" partition type exists */
		if (osdep->macparts[1].pmSig == PART_ENTRY_MAGIC) {
			bp->b_blkno = osdep->macparts[1].pmPyPartStart;
			bp->b_cylinder = bp->b_blkno/lp->d_secpercyl;
			bp->b_bcount = lp->d_secsize;
			bp->b_flags = B_BUSY | B_WRITE;
			*(struct disklabel *)bp->b_data = *lp;
			(*strat)(bp);
			error = biowait(bp);
			goto done;
		}

		/* SHOULD FAIL TO WRITE LABEL IF VALID HFS partition exists
		 * and no OpenBSD partition exists
		 */
		error = 1; /* EPERM? */
		goto done;
	}

	/* do dos partitions in the process of getting disklabel? */
	dospartoff = 0;
	cyl = LABELSECTOR / lp->d_secpercyl;

	/* read master boot record */
	bp->b_blkno = DOSBBSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylinder = DOSBBSECTOR / lp->d_secpercyl;
	(*strat)(bp);

	if ((error = biowait(bp)) != 0)
		goto done;

	/* XXX how do we check veracity/bounds of this? */
	bcopy(bp->b_data + DOSPARTOFF, dp, sizeof(dp));

	for (dp2=dp, i=0; i < NDOSPART && ourpart == -1; i++, dp2++)
		if (letoh32(dp2->dp_size) && dp2->dp_typ == DOSPTYP_OPENBSD)
			ourpart = i;

	if (ourpart != -1) {
		dp2 = &dp[ourpart];

		/*
		 * need sector address for SCSI/IDE,
		 * cylinder for ESDI/ST506/RLL
		 */
		dospartoff = letoh32(dp2->dp_start);
		cyl = DPCYL(dp2->dp_scyl, dp2->dp_ssect);
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
bounds_check_with_label(struct buf *bp, struct disklabel *lp,
    struct cpu_disklabel *osdep, int wlabel)
{
#define blockpersec(count, lp) ((count) * (((lp)->d_secsize) / DEV_BSIZE))
	struct partition *p = lp->d_partitions + DISKPART(bp->b_dev);
	int labelsector = blockpersec(DL_GETPOFFSET(&lp->d_partitions[RAW_PART]), lp) +
	    LABELSECTOR;
	int sz = howmany(bp->b_bcount, DEV_BSIZE);

	/* avoid division by zero */
	if (lp->d_secpercyl == 0) {
		bp->b_error = EINVAL;
		goto bad;
	}

	/* beyond partition? */
	if (bp->b_blkno + sz > blockpersec(DL_GETPSIZE(p), lp)) {
		sz = blockpersec(DL_GETPSIZE(p), lp) - bp->b_blkno;
		if (sz == 0) {
			/* If exactly at end of disk, return EOF. */
			bp->b_resid = bp->b_bcount;
			return (-1);
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
	if (bp->b_blkno + blockpersec(DL_GETPOFFSET(p), lp) <= labelsector &&
	    bp->b_blkno + blockpersec(DL_GETPOFFSET(p), lp) + sz > labelsector &&
	    (bp->b_flags & B_READ) == 0 && !wlabel) {
		bp->b_error = EROFS;
		goto bad;
	}

	/* calculate cylinder for disksort to order transfers with */
	bp->b_cylinder = (bp->b_blkno + blockpersec(DL_GETPOFFSET(p), lp)) /
	    lp->d_secpercyl;
	return (1);

bad:
	bp->b_flags |= B_ERROR;
	return (-1);
}

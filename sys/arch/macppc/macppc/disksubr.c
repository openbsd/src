/*	$OpenBSD: disksubr.c,v 1.55 2007/06/17 00:27:29 deraadt Exp $	*/
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
	struct partition *pp;
	struct buf *bp = NULL;
	int i, part_cnt, n, hfspartoff;
	struct part_map_entry *part;
	char *msg;

	if ((msg = initdisklabel(lp)))
		goto done;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* First check for a DPME (HFS) disklabel */
	bp->b_blkno = 1;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylinder = 1 / lp->d_secpercyl;
	(*strat)(bp);
	if (biowait(bp)) {
		msg = "DPME partition I/O error";
		goto doslabel;
	}

	/* if successful, wander through DPME partition table */
	part = (struct part_map_entry *)bp->b_data;
	/* if first partition is not valid, assume not HFS/DPME partitioned */
	if (part->pmSig != PART_ENTRY_MAGIC) {
		osdep->macparts[0].pmSig = 0; /* make invalid */
		goto doslabel;
	}
	osdep->macparts[0] = *part;
	part_cnt = part->pmMapBlkCnt;
	n = 0;
	for (i = 0; i < part_cnt; i++) {
		char *s;

		pp = &lp->d_partitions[8+n];

		bp->b_blkno = 1+i;
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		bp->b_cylinder = (1+i) / lp->d_secpercyl;
		(*strat)(bp);
		if (biowait(bp)) {
			msg = "DPME partition I/O error";
			goto doslabel;
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

	if (spoofonly)
		goto doslabel;

	/* next, dig out disk label */
	bp->b_blkno = hfspartoff;
	bp->b_cylinder = hfspartoff / lp->d_secpercyl;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);
	if (biowait(bp)) {
		msg = "disk label I/O error";
		goto done;
	}
	goto realdisklabel;

doslabel:
	msg = readdoslabel(bp, strat, lp, osdep, NULL, NULL, spoofonly);
	if (msg == NULL)
		goto done;

realdisklabel:
	msg = checkdisklabel(bp->b_data + LABELOFFSET, lp);
	if (msg == NULL)
		goto done;

#if defined(CD9660)
	if (iso_disklabelspoof(dev, strat, lp) == 0) {
		msg = NULL;
		goto done;
	}
#endif
#if defined(UDF)
	if (udf_disklabelspoof(dev, strat, lp) == 0) {
		msg = NULL;
		goto done;
	}
#endif

done:
	if (bp) {
		bp->b_flags |= B_INVAL;
		brelse(bp);
	}
	return (msg);
}

/*
 * Write disk label back to device after modification.
 */
int
writedisklabel(dev_t dev, void (*strat)(struct buf *),
    struct disklabel *lp, struct cpu_disklabel *osdep)
{
	int error, partoff = -1, cyl = 0;
	struct disklabel *dlp;
	struct buf *bp = NULL;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* try DPME partition */
	if (osdep->macparts[0].pmSig == PART_ENTRY_MAGIC) {
		/* only write if a valid "OpenBSD" partition type exists */
		if (osdep->macparts[1].pmSig == PART_ENTRY_MAGIC) {
			bp->b_blkno = osdep->macparts[1].pmPyPartStart;
			bp->b_cylinder = bp->b_blkno / lp->d_secpercyl;
			bp->b_bcount = lp->d_secsize;
			goto writeit;
		}
		error = EIO;
		goto done;
	}

	if (readdoslabel(bp, strat, lp, osdep, &partoff, &cyl, 1) != NULL) {
		error = EIO;
		goto done;
	}

	/* Read it in, slap the new label in, and write it back out */
	bp->b_blkno = partoff + LABELSECTOR;
	bp->b_cylinder = cyl;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);
	if ((error = biowait(bp)) != 0)
		goto done;

writeit:
	dlp = (struct disklabel *)(bp->b_data + LABELOFFSET);
	*dlp = *lp;
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

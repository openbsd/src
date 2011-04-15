/*	$OpenBSD: disksubr.c,v 1.22 2011/04/15 14:57:29 krw Exp $	*/

/*
 * Copyright (c) 1999 Michael Shalayeff
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>
#include <sys/disk.h>

int	readsgilabel(struct buf *, void (*)(struct buf *),
	    struct disklabel *, int *, int);

/*
 * Attempt to read a disk label from a device
 * using the indicated strategy routine.
 * The label must be partly set up before this:
 * secpercyl, secsize and anything required for a block i/o read
 * operation in the driver's strategy/start routines
 * must be filled in before calling us.
 */
int
readdisklabel(dev_t dev, void (*strat)(struct buf *),
    struct disklabel *lp, int spoofonly)
{
	struct buf *bp = NULL;
	int error;

	if ((error = initdisklabel(lp)))
		goto done;
	lp->d_flags |= D_VENDOR;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	error = readsgilabel(bp, strat, lp, NULL, spoofonly);
	if (error == 0)
		goto done;

	error = readdoslabel(bp, strat, lp, NULL, spoofonly);
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

static struct {
	int m;
	int b;
} maptab[] = {
    { 0,	FS_BSDFFS},	{ 1,	FS_SWAP},	{ 10,	FS_BSDFFS},
    { 3,	FS_BSDFFS},	{ 4,	FS_BSDFFS},	{ 5,	FS_BSDFFS},
    { 6,	FS_BSDFFS},	{ 7,	FS_BSDFFS},	{ 15,	FS_OTHER},
    { 9,	FS_BSDFFS},	{ 2,	FS_UNUSED},	{ 11,	FS_BSDFFS},
    { 12,	FS_BSDFFS},	{ 13,	FS_BSDFFS},	{ 14,	FS_BSDFFS},
    { 8,	FS_BSDFFS}
};

int
readsgilabel(struct buf *bp, void (*strat)(struct buf *),
    struct disklabel *lp, int *partoffp, int spoofonly)
{
	struct sgilabel *dlp;
	int i, *p, cs = 0;
	u_int64_t fsoffs = 0;
	u_int64_t fsend = DL_GETBEND(lp);
	int offset;

	bp->b_blkno = 0;
	bp->b_bcount = lp->d_secsize;
	CLR(bp->b_flags, B_WRITE | B_DONE);
	SET(bp->b_flags, B_BUSY | B_READ | B_RAW);
	(*strat)(bp);

	/* if successful, locate disk label within block and validate */
	if (biowait(bp))
		return (bp->b_error);

	dlp = (struct sgilabel *)(bp->b_data + LABELOFFSET);
	if (dlp->magic != htobe32(SGILABEL_MAGIC))
		goto finished;

	if (dlp->partitions[0].blocks == 0)
		return (EINVAL);
	fsoffs = dlp->partitions[0].first * (dlp->dp.dp_secbytes / DEV_BSIZE);
	fsend = fsoffs + dlp->partitions[0].blocks *
	    (dlp->dp.dp_secbytes / DEV_BSIZE);

	/* Only came here to find the offset... */
	if (partoffp) {
		*partoffp = fsoffs;
		goto finished;
	}

	p = (int *)dlp;
	i = sizeof(struct sgilabel) / sizeof(int);
	while (i--)
		cs += *p++;
	if (cs != 0)
		return (EINVAL);	/* sgilabel checksum error */

	/* Spoof info from sgi label, in case there is no OpenBSD label. */
	DL_SETDSIZE(lp, (DL_GETDSIZE(lp)*lp->d_secsize) / dlp->dp.dp_secbytes);
	lp->d_secsize = dlp->dp.dp_secbytes;
	lp->d_nsectors = dlp->dp.dp_secs;
	lp->d_ntracks = dlp->dp.dp_trks0;
	lp->d_ncylinders = dlp->dp.dp_cyls;
	lp->d_npartitions = MAXPARTITIONS;

	for (i = 0; i < 16; i++) {
		int bsd = maptab[i].m;
		int type = maptab[i].b;

		if (spoofonly && type != FS_UNUSED && type != FS_OTHER)
			continue;

		DL_SETPOFFSET(&lp->d_partitions[bsd],
		    dlp->partitions[i].first);
		DL_SETPSIZE(&lp->d_partitions[bsd],
		    dlp->partitions[i].blocks);
		lp->d_partitions[bsd].p_fstype = type;
		if (type == FS_BSDFFS) {
			lp->d_partitions[bsd].p_fragblock =
			    DISKLABELV1_FFS_FRAGBLOCK(1024, 8);
			lp->d_partitions[bsd].p_cpg = 16;
		}
	}

	DL_SETBSTART(lp, fsoffs);
	DL_SETBEND(lp, fsend);
	lp->d_version = 1;
	lp->d_flags = D_VENDOR;
	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);

finished:
	/* record the OpenBSD partition's placement for the caller */
	if (partoffp)
		*partoffp = fsoffs;
	else {
		DL_SETBSTART(lp, fsoffs);
		DL_SETBEND(lp, fsend);
	}

	/* don't read the on-disk label if we are in spoofed-only mode */
	if (spoofonly)
		return (0);

	bp->b_blkno = DL_BLKTOSEC(lp, fsoffs + LABELSECTOR) *
	    DL_BLKSPERSEC(lp);
	offset = DL_BLKOFFSET(lp, fsoffs + LABELSECTOR) + LABELOFFSET;
	bp->b_bcount = lp->d_secsize;
	CLR(bp->b_flags, B_WRITE | B_DONE);
	SET(bp->b_flags, B_BUSY | B_READ | B_RAW);
	(*strat)(bp);
	if (biowait(bp))
		return (bp->b_error);

	return checkdisklabel(bp->b_data + offset, lp, fsoffs, fsend);
}

/*
 * Write disk label back to device after modification.
 */
int
writedisklabel(dev_t dev, void (*strat)(struct buf *), struct disklabel *lp)
{
	int error = EIO, partoff = -1;
	int offset;
	struct buf *bp = NULL;
	struct disklabel *dlp;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	if (readsgilabel(bp, strat, lp, &partoff, 1) != 0 &&
	    readdoslabel(bp, strat, lp, &partoff, 1) != 0)
		goto done;

	/* Read it in, slap the new label in, and write it back out */
	bp->b_blkno = DL_BLKTOSEC(lp, partoff+LABELSECTOR) * DL_BLKSPERSEC(lp);
	offset = DL_BLKOFFSET(lp, partoff + LABELSECTOR) + LABELOFFSET;
	bp->b_bcount = lp->d_secsize;
	CLR(bp->b_flags, B_WRITE | B_DONE);
	SET(bp->b_flags, B_BUSY | B_READ | B_RAW);
	(*strat)(bp);
	if ((error = biowait(bp)) != 0)
		goto done;

	dlp = (struct disklabel *)(bp->b_data + offset);
	*dlp = *lp;
	CLR(bp->b_flags, B_READ | B_DONE);
	SET(bp->b_flags, B_BUSY | B_WRITE | B_RAW);
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

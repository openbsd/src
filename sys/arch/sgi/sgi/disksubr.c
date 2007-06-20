/*	$OpenBSD: disksubr.c,v 1.2 2007/06/20 18:15:46 deraadt Exp $	*/

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

char   *readbsdlabel(struct buf *, void (*)(struct buf *), int, int,
    int, struct disklabel *, int);
char   *readsgilabel(struct buf *, void (*)(struct buf *),
    struct disklabel *, int *, int);

/*
 * Try to read a standard BSD disklabel at a certain sector.
 */
char *
readbsdlabel(struct buf *bp, void (*strat)(struct buf *),
    int cyl, int sec, int off, struct disklabel *lp,
    int spoofonly)
{
	/* don't read the on-disk label if we are in spoofed-only mode */
	if (spoofonly)
		return (NULL);

	bp->b_blkno = sec;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);

	/* if successful, locate disk label within block and validate */
	if (biowait(bp))
		return ("disk label I/O error");

	return checkdisklabel(bp->b_data + LABELOFFSET, lp);
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
readdisklabel(dev_t dev, void (*strat)(struct buf *),
    struct disklabel *lp, int spoofonly)
{
	struct buf *bp = NULL;
	char *msg;

	if ((msg = initdisklabel(lp)))
		goto done;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	msg = readsgilabel(bp, strat, lp, 0, spoofonly);
	if (msg == NULL)
		goto done;

	msg = readdoslabel(bp, strat, lp, 0, spoofonly);
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

char *
readsgilabel(struct buf *bp, void (*strat)(struct buf *),
    struct disklabel *lp, int *partoffp, int spoofonly)
{
	struct sgilabel *dlp;
	char *msg = NULL;
	int i, *p, cs = 0;
	int fsoffs = 0;

	bp->b_blkno = 0;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);

	/* if successful, locate disk label within block and validate */
	if (biowait(bp)) {
		msg = "disk label I/O error";
		goto done;
	}

	dlp = (struct sgilabel *)(bp->b_data + LABELOFFSET);
	if (dlp->magic != htobe32(SGILABEL_MAGIC)) {
		fsoffs = 0;
		goto finished;
	}

	if (dlp->partitions[0].blocks == 0) {
		msg = "no BSD partition";
		goto done;
	}
	fsoffs = dlp->partitions[0].first;

	if (spoofonly)
		goto finished;

	p = (int *)dlp;
	i = sizeof(struct sgilabel) / sizeof(int);
	while (i--)
		cs += *p++;
	if (cs != 0) {
		msg = "sgilabel checksum error";
		goto done;
	}

	/* Set up partitions i-l if there is no BSD label. */
	lp->d_secsize = dlp->dp.dp_secbytes;
	lp->d_nsectors = dlp->dp.dp_secs;
	lp->d_ntracks = dlp->dp.dp_trks0;
	lp->d_ncylinders = dlp->dp.dp_cyls;
	lp->d_interleave = dlp->dp.dp_interleave;
	lp->d_npartitions = MAXPARTITIONS;

	for (i = 0; i < 16; i++) {
		int bsd = maptab[i].m;

		DL_SETPOFFSET(&lp->d_partitions[bsd],
		    dlp->partitions[i].first);
		DL_SETPSIZE(&lp->d_partitions[bsd],
		    dlp->partitions[i].blocks);
		lp->d_partitions[bsd].p_fstype = maptab[i].b;
		if (lp->d_partitions[bsd].p_fstype == FS_BSDFFS) {
			lp->d_partitions[bsd].p_fragblock =
			    DISKLABELV1_FFS_FRAGBLOCK(1024, 8);
			lp->d_partitions[bsd].p_cpg = 16;
		}
	}

	lp->d_version = 1;
	lp->d_flags = D_VENDOR;
	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);

finished:
	if (partoffp)
		*partoffp = fsoffs;

	if (spoofonly)
		goto done;

	bp->b_blkno = fsoffs + LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);
	if (biowait(bp)) {
		msg = "disk label I/O error";
		goto done;
	}

	return checkdisklabel(bp->b_data + LABELOFFSET, lp);

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
writedisklabel(dev_t dev, void (*strat)(struct buf *), struct disklabel *lp)
{
	int error = EIO, partoff = -1;
	struct buf *bp = NULL;
	struct disklabel *dlp;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	if (readsgilabel(bp, strat, lp, &partoff, 1) != NULL &&
	    readdoslabel(bp, strat, lp, &partoff, 1) != NULL)
		goto done;

	/* Read it in, slap the new label in, and write it back out */
	bp->b_blkno = partoff + LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);
	if ((error = biowait(bp)) != 0)
		goto done;

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

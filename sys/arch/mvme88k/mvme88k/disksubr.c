/*	$OpenBSD: disksubr.c,v 1.56 2007/06/20 18:15:46 deraadt Exp $	*/
/*
 * Copyright (c) 1998 Steve Murphree, Jr.
 * Copyright (c) 1995 Dale Rahn.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/disk.h>

void bsdtocpulabel(struct disklabel *, struct mvmedisklabel *);
void cputobsdlabel(struct disklabel *, struct mvmedisklabel *);

/*
 * Attempt to read a disk label from a device
 * using the indicated strategy routine.
 * The label must be partly set up before this:
 * secpercyl and anything required in the strategy routine
 * (e.g., sector size) must be filled in before calling us.
 * Returns NULL on success and an error string on failure.
 */

char *
readdisklabel(dev_t dev, void (*strat)(struct buf *),
    struct disklabel *lp, int spoofonly)
{
	struct buf *bp = NULL;
	struct mvmedisklabel *mlp;
	int error;
	char *msg;

	if ((msg = initdisklabel(lp)))
		goto done;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* don't read the on-disk label if we are in spoofed-only mode */
	if (spoofonly)
		goto done;

	bp->b_blkno = LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);
	error = biowait(bp);
	if (error) {
		msg = "disk label read error";
		goto done;
	}

	mlp = (struct mvmedisklabel *)bp->b_data;
	if (mlp->magic1 != DISKMAGIC || mlp->magic2 != DISKMAGIC) {
		msg = "no disk label";
		goto done;
	}

	cputobsdlabel(lp, mlp);
	if (dkcksum(lp) == 0)
		goto done;
	msg = "disk label corrupted";

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
writedisklabel(dev_t dev, void (*strat)(struct buf *), struct disklabel *lp)
{
	struct buf *bp = NULL;
	int error;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* Read it in, slap the new label in, and write it back out */
	bp->b_blkno = LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);
	if ((error = biowait(bp)) != 0)
		goto done;

	bsdtocpulabel(lp, (struct mvmedisklabel *)bp->b_data);

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

void
bsdtocpulabel(struct disklabel *lp, struct mvmedisklabel *clp)
{
	char *tmot = "MOTOROLA", *id = "M88K", *mot;
	int i;

	clp->magic1 = lp->d_magic;
	clp->type = lp->d_type;
	clp->subtype = lp->d_subtype;
	strncpy(clp->vid_vd, lp->d_typename, 16);
	strncpy(clp->packname, lp->d_packname, 16);
	clp->cfg_psm = lp->d_secsize;
	clp->cfg_spt = lp->d_nsectors;
	clp->cfg_trk = lp->d_ncylinders;	/* trk is really num of cyl! */
	clp->cfg_hds = lp->d_ntracks;

	clp->secpercyl = lp->d_secpercyl;
	clp->secperunit = DL_GETDSIZE(lp);
	clp->sparespertrack = lp->d_sparespertrack;
	clp->sparespercyl = lp->d_sparespercyl;
	clp->acylinders = lp->d_acylinders;
	clp->rpm = lp->d_rpm;

	clp->cfg_ilv = lp->d_interleave;
	clp->cfg_sof = lp->d_trackskew;
	clp->cylskew = lp->d_cylskew;
	clp->headswitch = lp->d_headswitch;

	/* this silly table is for winchester drives */
	if (lp->d_trkseek < 6)
		clp->cfg_ssr = 0;
	else if (lp->d_trkseek < 10)
		clp->cfg_ssr = 1;
	else if (lp->d_trkseek < 15)
		clp->cfg_ssr = 2;
	else if (lp->d_trkseek < 20)
		clp->cfg_ssr = 3;
	else
		clp->cfg_ssr = 4;

	clp->flags = lp->d_flags;
	for (i = 0; i < NDDATA; i++)
		clp->drivedata[i] = lp->d_drivedata[i];
	for (i = 0; i < NSPARE; i++)
		clp->spare[i] = lp->d_spare[i];

	clp->magic2 = lp->d_magic2;
	clp->checksum = lp->d_checksum;
	clp->partitions = lp->d_npartitions;
	clp->bbsize = lp->d_bbsize;
	clp->sbsize = lp->d_sbsize;
	clp->checksum = lp->d_checksum;
	bcopy(&lp->d_partitions[0], clp->vid_4, sizeof(struct partition) * 4);
	bcopy(&lp->d_partitions[4], clp->cfg_4, sizeof(struct partition) * 12);
	clp->version = 2;

	/* Put "MOTOROLA" in the VID. This makes it a valid boot disk. */
	for (mot = clp->vid_mot, i = 0; i < 8; i++)
		*mot++ = *tmot++;

	/* put volume id in the VID */
	for (mot = clp->vid_id, i = 0; i < 4; i++)
		*mot++ = *id++;
}

void
cputobsdlabel(struct disklabel *lp, struct mvmedisklabel *clp)
{
	int i;

	lp->d_magic = clp->magic1;
	lp->d_type = clp->type;
	lp->d_subtype = clp->subtype;
	strncpy(lp->d_typename, clp->vid_vd, sizeof lp->d_typename);
	strncpy(lp->d_packname, clp->packname, sizeof lp->d_packname);
	lp->d_secsize = clp->cfg_psm;
	lp->d_nsectors = clp->cfg_spt;
	lp->d_ncylinders = clp->cfg_trk; /* trk is really num of cyl! */
	lp->d_ntracks = clp->cfg_hds;

	lp->d_secpercyl = clp->secpercyl;
	if (DL_GETDSIZE(lp) == 0)
		DL_SETDSIZE(lp, clp->secperunit);
	lp->d_sparespertrack = clp->sparespertrack;
	lp->d_sparespercyl = clp->sparespercyl;
	lp->d_acylinders = clp->acylinders;
	lp->d_rpm = clp->rpm;
	lp->d_interleave = clp->cfg_ilv;
	lp->d_trackskew = clp->cfg_sof;
	lp->d_cylskew = clp->cylskew;
	lp->d_headswitch = clp->headswitch;

	/* this silly table is for winchester drives */
	switch (clp->cfg_ssr) {
	case 1:
		lp->d_trkseek = 6;
		break;
	case 2:
		lp->d_trkseek = 10;
		break;
	case 3:
		lp->d_trkseek = 15;
		break;
	case 4:
		lp->d_trkseek = 20;
		break;
	default:
		lp->d_trkseek = 0;
	}

	lp->d_flags = clp->flags;
	for (i = 0; i < NDDATA; i++)
		lp->d_drivedata[i] = clp->drivedata[i];
	for (i = 0; i < NSPARE; i++)
		lp->d_spare[i] = clp->spare[i];

	lp->d_magic2 = clp->magic2;
	lp->d_npartitions = clp->partitions;
	lp->d_bbsize = clp->bbsize;
	lp->d_sbsize = clp->sbsize;

	bcopy(clp->vid_4, &lp->d_partitions[0], sizeof(struct partition) * 4);
	bcopy(clp->cfg_4, &lp->d_partitions[4], sizeof(struct partition) * 12);

	if (clp->version < 2) {
		struct __partitionv0 *v0pp = (struct __partitionv0 *)lp->d_partitions;
		struct partition *pp = lp->d_partitions;

		for (i = 0; i < lp->d_npartitions; i++, pp++, v0pp++) {
			pp->p_fragblock = DISKLABELV1_FFS_FRAGBLOCK(v0pp->
			    p_fsize, v0pp->p_frag);
			pp->p_offseth = 0;
			pp->p_sizeh = 0;
		}
	}

	lp->d_version = 1;
	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);
}

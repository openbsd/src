/*	$OpenBSD: disksubr.c,v 1.67 2010/04/05 02:09:15 miod Exp $	*/
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

void	bsdtocpulabel(struct disklabel *, struct mvmedisklabel *);
int	cputobsdlabel(struct disklabel *, struct mvmedisklabel *);

/*
 * Attempt to read a disk label from a device
 * using the indicated strategy routine.
 * The label must be partly set up before this:
 * secpercyl and anything required in the strategy routine
 * (e.g., sector size) must be filled in before calling us.
 * Returns NULL on success and an error string on failure.
 */

int
readdisklabel(dev_t dev, void (*strat)(struct buf *),
    struct disklabel *lp, int spoofonly)
{
	struct buf *bp = NULL;
	int error;

	if ((error = initdisklabel(lp)))
		goto done;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/* don't read the on-disk label if we are in spoofed-only mode */
	if (spoofonly)
		goto done;

	bp->b_blkno = LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ | B_RAW;
	(*strat)(bp);
	if (biowait(bp)) {
		error = bp->b_error;
		goto done;
	}

	error = cputobsdlabel(lp, (struct mvmedisklabel *)bp->b_data);
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
	return (error);
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
	bp->b_flags = B_BUSY | B_READ | B_RAW;
	(*strat)(bp);
	if ((error = biowait(bp)) != 0)
		goto done;

	bsdtocpulabel(lp, (struct mvmedisklabel *)bp->b_data);

	bp->b_flags = B_BUSY | B_WRITE | B_RAW;
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
	char *tmot = "MOTOROLA", *id = "M68K", *mot;
	int i;
	u_short osa_u, osa_l, osl;
	u_int oss;

	/* preserve existing VID boot code information */
	osa_u = clp->vid_osa_u;
	osa_l = clp->vid_osa_l;
	osl = clp->vid_osl;
	oss = clp->vid_oss;
	bzero(clp, sizeof(*clp));
	clp->vid_osa_u = osa_u;
	clp->vid_osa_l = osa_l;
	clp->vid_osl = osl;
	clp->vid_oss = oss;
	clp->vid_cas = clp->vid_cal = 1;

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
	clp->cfg_sof = 1;
	clp->cylskew = 1;
	clp->headswitch = 0;
	clp->cfg_ssr = 0;
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

int
cputobsdlabel(struct disklabel *lp, struct mvmedisklabel *clp)
{
	int i;

	if (clp->magic1 != DISKMAGIC || clp->magic2 != DISKMAGIC)
		return (EINVAL);	/* no disk label */

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
	return (0);
}

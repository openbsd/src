/*	$OpenBSD: disksubr.c,v 1.44 2007/05/29 06:28:15 otto Exp $	*/
/*	$NetBSD: disksubr.c,v 1.16 1996/04/28 20:25:59 thorpej Exp $ */

/*
 * Copyright (c) 1994, 1995 Gordon W. Ross
 * Copyright (c) 1994 Theo de Raadt
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
#include <sys/ioccom.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/dkbad.h>

#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <dev/sun/disklabel.h>
#if defined(SUN4)
#include <machine/oldmon.h>
#endif

#include "cd.h"

#if MAXPARTITIONS != 16
#warn beware: Sun disklabel compatibility assumes MAXPARTITIONS == 16
#endif

static	char *disklabel_sun_to_bsd(char *, struct disklabel *);
static	int disklabel_bsd_to_sun(struct disklabel *, char *);
static __inline u_long sun_extended_sum(struct sun_disklabel *);

#if NCD > 0
/* XXX for comparison below. */
extern void cdstrategy(struct buf *);
#endif

/*
 * Attempt to read a disk label from a device
 * using the indicated strategy routine.
 * The label must be partly set up before this:
 * secpercyl, secsize and anything required for a block i/o read
 * operation in the driver's strategy/start routines
 * must be filled in before calling us.
 *
 * Return buffer for use in signalling errors if requested.
 *
 * Returns null on success and an error string on failure.
 */
char *
readdisklabel(dev, strat, lp, clp, spoofonly)
	dev_t dev;
	void (*strat)(struct buf *);
	struct disklabel *lp;
	struct cpu_disklabel *clp;
	int spoofonly;
{
	struct buf *bp = NULL;
	struct disklabel *dlp;
	struct sun_disklabel *slp;
	char *msg = NULL;
	int error, i;

	/* minimal requirements for archetypal disk label */
	if (lp->d_secsize < DEV_BSIZE)
		lp->d_secsize = DEV_BSIZE;
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;
	if (lp->d_secpercyl == 0) {
		msg = "invalid geometry";
		goto done;
	}
	lp->d_npartitions = RAW_PART+1;
	for (i = 0; i < RAW_PART; i++) {
		lp->d_partitions[i].p_size = 0;
		lp->d_partitions[i].p_offset = 0;
	}
	if (lp->d_partitions[i].p_size == 0)
		lp->d_partitions[i].p_size = lp->d_secperunit;
	lp->d_partitions[i].p_offset = 0;
	lp->d_bbsize = 8192;
	lp->d_sbsize = 64*1024;		/* XXX ? */

	/* don't read the on-disk label if we are in spoofed-only mode */
	if (spoofonly)
		goto done;

	/* obtain buffer to probe drive with */
	bp = geteblk((int)lp->d_secsize);

	/* next, dig out disk label */
	bp->b_dev = dev;
	bp->b_blkno = LABELSECTOR;
	bp->b_cylinder = 0;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);

	/* if successful, locate disk label within block and validate */
	error = biowait(bp);
	if (error == 0) {
		/* Save the whole block in case it has info we need. */
		bcopy(bp->b_data, clp->cd_block, sizeof(clp->cd_block));
	}
	if (error) {
		msg = "disk label read error";
		goto done;
	}

#if NCD > 0
	if (strat == cdstrategy) {
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
	}
#endif /* NCD > 0 */

	/* Check for a Sun disk label (for PROM compatibility). */
	slp = (struct sun_disklabel *) clp->cd_block;
	if (slp->sl_magic == SUN_DKMAGIC) {
		msg = disklabel_sun_to_bsd(clp->cd_block, lp);
		goto done;
	}

	/* Check for a native disk label (PROM can not boot it). */
	dlp = (struct disklabel *) (clp->cd_block + LABELOFFSET);
	if (dlp->d_magic == DISKMAGIC) {
		if (dkcksum(dlp)) {
			msg = "disk label corrupted";
			goto done;
		}
		*lp = *dlp;	/* struct assignment */
		msg = NULL;
		goto done;
	}

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
	bzero(clp->cd_block, sizeof(clp->cd_block));
	msg = "no disk label";

done:
	if (bp) {
		bp->b_flags = B_INVAL | B_AGE | B_READ;
		brelse(bp);
	}
	cvtdisklabelv1(lp);
	return (msg);
}

/*
 * Check new disk label for sensibility
 * before setting it.
 */
int
setdisklabel(olp, nlp, openmask, clp)
	struct disklabel *olp, *nlp;
	u_long openmask;
	struct cpu_disklabel *clp;
{
	int i;
	struct partition *opp, *npp;

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

	while ((i = ffs((long)openmask)) != 0) {
		i--;
		openmask &= ~(1 << i);
		if (nlp->d_npartitions <= i)
			return (EBUSY);
		opp = &olp->d_partitions[i];
		npp = &nlp->d_partitions[i];
		if (npp->p_offset != opp->p_offset || npp->p_size < opp->p_size)
			return (EBUSY);
	}

	*olp = *nlp;
	return (0);
}

/*
 * Write disk label back to device after modification.
 * Current label is already in clp->cd_block[]
 */
int
writedisklabel(dev, strat, lp, clp)
	dev_t dev;
	void (*strat)(struct buf *);
	struct disklabel *lp;
	struct cpu_disklabel *clp;
{
	struct buf *bp;
	int error;

	error = disklabel_bsd_to_sun(lp, clp->cd_block);
	if (error)
		return (error);

#if 0	/* XXX - Allow writing native disk labels? */
	{
		struct disklabel *dlp;
		dlp = (struct disklabel *)(clp->cd_block + LABELOFFSET);
		*dlp = *lp;	/* struct assignment */
	}
#endif

	/* Get a buffer and copy the new label into it. */
	bp = geteblk((int)lp->d_secsize);
	bcopy(clp->cd_block, bp->b_data, sizeof(clp->cd_block));

	/* Write out the updated label. */
	bp->b_dev = dev;
	bp->b_blkno = LABELSECTOR;
	bp->b_cylinder = 0;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_WRITE;
	(*strat)(bp);
	error = biowait(bp);
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
	int sz = howmany(bp->b_bcount, DEV_BSIZE);

	/* avoid division by zero */
	if (lp->d_secpercyl == 0) {
		bp->b_error = EINVAL;
		goto bad;
	}

	/* overwriting disk label ? */
	/* XXX should also protect bootstrap in first 8K */
	/* XXX this assumes everything <=LABELSECTOR is label! */
	/*     But since LABELSECTOR is 0, that's ok for now. */
	if ((bp->b_blkno + blockpersec(p->p_offset, lp) <= LABELSECTOR) &&
	    ((bp->b_flags & B_READ) == 0) && (wlabel == 0)) {
		bp->b_error = EROFS;
		goto bad;
	}

	/* beyond partition? */
	if (bp->b_blkno + sz > blockpersec(p->p_size, lp)) {
		sz = blockpersec(p->p_size, lp) - bp->b_blkno;
		if (sz == 0) {
			/* If exactly at end of disk, return an EOF */
			bp->b_resid = bp->b_bcount;
			return (0);
		}
		if (sz < 0) {
			/* If past end of disk, return EINVAL. */
			bp->b_error = EINVAL;
			goto bad;
		}
		/* Or truncate if part of it fits */
		bp->b_bcount = sz << DEV_BSHIFT;
	}

	/* calculate cylinder for disksort to order transfers with */
	bp->b_resid = (bp->b_blkno + blockpersec(p->p_offset, lp)) /
	    lp->d_secpercyl;
	return (1);
bad:
	bp->b_flags |= B_ERROR;
	return (-1);
}

/************************************************************************
 *
 * The rest of this was taken from arch/sparc/scsi/sun_disklabel.c
 * and then substantially rewritten by Gordon W. Ross
 *
 ************************************************************************/

/* What partition types to assume for Sun disklabels: */
static u_char
sun_fstypes[16] = {
	FS_BSDFFS,	/* a */
	FS_SWAP,	/* b */
	FS_UNUSED,	/* c - whole disk */
	FS_BSDFFS,	/* d */
	FS_BSDFFS,	/* e */
	FS_BSDFFS,	/* f */
	FS_BSDFFS,	/* g */
	FS_BSDFFS,	/* h */
	FS_BSDFFS,	/* i */
	FS_BSDFFS,	/* j */
	FS_BSDFFS,	/* k */
	FS_BSDFFS,	/* l */
	FS_BSDFFS,	/* m */
	FS_BSDFFS,	/* n */
	FS_BSDFFS,	/* o */
	FS_BSDFFS	/* p */
};

/*
 * Given a struct sun_disklabel, assume it has an extended partition
 * table and compute the correct value for sl_xpsum.
 */
static __inline u_long
sun_extended_sum(sl)
	struct sun_disklabel *sl;
{
	u_long lsum;
	u_long *xp;
	u_long *ep;

	xp = (u_long *) &sl->sl_xpmag;
	ep = (u_long *) &sl->sl_xxx1[0];

	lsum = 0;
	for (; xp < ep; xp++)
		lsum += *xp;
	return(lsum);
}
	
/*
 * Given a SunOS disk label, set lp to a BSD disk label.
 * Returns NULL on success, else an error string.
 *
 * The BSD label is cleared out before this is called.
 */
static char *
disklabel_sun_to_bsd(cp, lp)
	char *cp;
	struct disklabel *lp;
{
	struct sun_disklabel *sl;
	struct partition *npp;
	struct sun_dkpart *spp;
	int i, secpercyl;
	u_short cksum, *sp1, *sp2;

	sl = (struct sun_disklabel *)cp;

	/* Verify the XOR check. */
	sp1 = (u_short *)sl;
	sp2 = (u_short *)(sl + 1);
	cksum = 0;
	while (sp1 < sp2)
		cksum ^= *sp1++;
	if (cksum != 0)
		return("SunOS disk label, bad checksum");

	/* Format conversion. */
	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	memcpy(lp->d_packname, sl->sl_text, sizeof(lp->d_packname));

	lp->d_secsize = 512;
	lp->d_nsectors   = sl->sl_nsectors;
	lp->d_ntracks    = sl->sl_ntracks;
	lp->d_ncylinders = sl->sl_ncylinders;

	secpercyl = sl->sl_nsectors * sl->sl_ntracks;
	lp->d_secpercyl  = secpercyl;
	lp->d_secperunit = secpercyl * sl->sl_ncylinders;

	lp->d_sparespercyl = sl->sl_sparespercyl;
	lp->d_acylinders   = sl->sl_acylinders;
	lp->d_rpm          = sl->sl_rpm;
	lp->d_interleave   = sl->sl_interleave;

	lp->d_npartitions = MAXPARTITIONS;
	/* These are as defined in <ufs/ffs/fs.h> */
	lp->d_bbsize = 8192;	/* XXX */
	lp->d_sbsize = 8192;	/* XXX */

	for (i = 0; i < 8; i++) {
		spp = &sl->sl_part[i];
		npp = &lp->d_partitions[i];
		npp->p_offset = spp->sdkp_cyloffset * secpercyl;
		npp->p_size = spp->sdkp_nsectors;
		if (npp->p_size == 0) {
			npp->p_fstype = FS_UNUSED;
		} else {
			npp->p_fstype = sun_fstypes[i];
			if (npp->p_fstype == FS_BSDFFS) {
				/*
				 * The sun label does not store the FFS fields,
				 * so just set them with default values here.
				 */
				npp->p_fragblock =
				    DISKLABELV1_FFS_FRAGBLOCK(2048, 8);
				npp->p_cpg = 16;
			}
		}
	}

	/* Clear "extended" partition info, tentatively */
	for (i = 0; i < SUNXPART; i++) {
		npp = &lp->d_partitions[i+8];
		npp->p_offset = 0;
		npp->p_size = 0;
		npp->p_fstype = FS_UNUSED;
	}

	/* Check to see if there's an "extended" partition table */
	if (sl->sl_xpmag == SL_XPMAG &&
	    sun_extended_sum(sl) == sl->sl_xpsum) {	/* ...yes! */
		/*
		 * There is.  Copy over the "extended" partitions.
		 * This code parallels the loop for partitions a-h.
		 */
		for (i = 0; i < SUNXPART; i++) {
			spp = &sl->sl_xpart[i];
			npp = &lp->d_partitions[i+8];
			npp->p_offset = spp->sdkp_cyloffset * secpercyl;
			npp->p_size = spp->sdkp_nsectors;
			if (npp->p_size == 0) {
				npp->p_fstype = FS_UNUSED;
				continue;
			}
			npp->p_fstype = sun_fstypes[i+8];
			if (npp->p_fstype == FS_BSDFFS) {
				npp->p_fragblock =
				    DISKLABELV1_FFS_FRAGBLOCK(2048, 8);
				npp->p_cpg = 16;
			}
		}
	}

	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);
	return (NULL);
}

/*
 * Given a BSD disk label, update the Sun disklabel
 * pointed to by cp with the new info.  Note that the
 * Sun disklabel may have other info we need to keep.
 * Returns zero or error code.
 */
static int
disklabel_bsd_to_sun(lp, cp)
	struct disklabel *lp;
	char *cp;
{
	struct sun_disklabel *sl;
	struct partition *npp;
	struct sun_dkpart *spp;
	int i, secpercyl;
	u_short cksum, *sp1, *sp2;

	/* Enforce preconditions */
	if (lp->d_secsize != 512 || lp->d_nsectors == 0 || lp->d_ntracks == 0)
		return (EINVAL);

	sl = (struct sun_disklabel *)cp;

	/* Format conversion. */
	memcpy(sl->sl_text, lp->d_packname, sizeof(lp->d_packname));
	sl->sl_rpm = lp->d_rpm;
	sl->sl_pcylinders   = lp->d_ncylinders + lp->d_acylinders; /* XXX */
	sl->sl_sparespercyl = lp->d_sparespercyl;
	sl->sl_interleave   = lp->d_interleave;
	sl->sl_ncylinders   = lp->d_ncylinders;
	sl->sl_acylinders   = lp->d_acylinders;
	sl->sl_ntracks      = lp->d_ntracks;
	sl->sl_nsectors     = lp->d_nsectors;

	secpercyl = sl->sl_nsectors * sl->sl_ntracks;
	for (i = 0; i < 8; i++) {
		spp = &sl->sl_part[i];
		npp = &lp->d_partitions[i];

		if (npp->p_offset % secpercyl)
			return (EINVAL);
		spp->sdkp_cyloffset = npp->p_offset / secpercyl;
		spp->sdkp_nsectors = npp->p_size;
	}
	sl->sl_magic = SUN_DKMAGIC;

	/*
	 * The reason we store the extended table stuff only conditionally
	 * is so that a label that doesn't need it will have NULs there, like
	 * a "traditional" Sun label.  Since Suns seem to ignore everything
	 * between sl_text and sl_rpm, this probably doesn't matter, but it
	 * certainly doesn't hurt anything and it's easy to do.
	 */
	for (i = 0; i < SUNXPART; i++) {
		if (lp->d_partitions[i+8].p_offset ||
		    lp->d_partitions[i+8].p_size)
			break;
	}
	/* We do need to load the extended table? */
	if (i < SUNXPART) {
		sl->sl_xpmag = SL_XPMAG;
		for (i = 0; i < SUNXPART; i++) {
			spp = &sl->sl_xpart[i];
			npp = &lp->d_partitions[i+8];
			if (npp->p_offset % secpercyl)
				return (EINVAL);
			sl->sl_xpart[i].sdkp_cyloffset =
			    npp->p_offset / secpercyl;
			sl->sl_xpart[i].sdkp_nsectors = npp->p_size;
		}
		sl->sl_xpsum = sun_extended_sum(sl);
	} else {
		sl->sl_xpmag = 0;
		for (i = 0; i < SUNXPART; i++) {
			sl->sl_xpart[i].sdkp_cyloffset = 0;
			sl->sl_xpart[i].sdkp_nsectors = 0;
		}
		sl->sl_xpsum = 0;
	}

	/* Correct the XOR check. */
	sp1 = (u_short *)sl;
	sp2 = (u_short *)(sl + 1);
	sl->sl_cksum = cksum = 0;
	while (sp1 < sp2)
		cksum ^= *sp1++;
	sl->sl_cksum = cksum;

	return (0);
}

/*
 * Search the bad sector table looking for the specified sector.
 * Return index if found.
 * Return -1 if not found.
 */
int
isbad(bt, cyl, trk, sec)
	struct dkbad *bt;
	int cyl, trk, sec;
{
	int i;
	long blk, bblk;

	blk = ((long)cyl << 16) + (trk << 8) + sec;
	for (i = 0; i < NBT_BAD; i++) {
		bblk = ((long)bt->bt_bad[i].bt_cyl << 16) +
			bt->bt_bad[i].bt_trksec;
		if (blk == bblk)
			return (i);
		if (blk < bblk || bblk < 0)
			break;
	}
	return (-1);
}

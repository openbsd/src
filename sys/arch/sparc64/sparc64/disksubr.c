/*	$NetBSD: disksubr.c,v 1.13 2000/12/17 22:39:18 pk Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Theo de Raadt.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/sun/disklabel.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#if defined(SUN4)
#include <machine/oldmon.h>
#endif

#include <dev/sbus/sbusvar.h>

static	char *disklabel_sun_to_bsd __P((char *, struct disklabel *));
static	int disklabel_bsd_to_sun __P((struct disklabel *, char *));

extern struct device *bootdv;

void
dk_establish(struct disk *dk, struct device *dev)
{
	/* fix later */
}

/*
 * Attempt to read a disk label from a device
 * using the indicated stategy routine.
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
	void (*strat) __P((struct buf *));
	struct disklabel *lp;
	struct cpu_disklabel *clp;
	int spoofonly;
{
	struct buf *bp;
	struct disklabel *dlp;
	struct sun_disklabel *slp;
	int error;

	/* minimal requirements for archtypal disk label */
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;
	if (lp->d_npartitions == 0) {
		lp->d_npartitions = RAW_PART + 1;
		if (lp->d_partitions[RAW_PART].p_size == 0)
			lp->d_partitions[RAW_PART].p_size = 0x1fffffff;
		lp->d_partitions[RAW_PART].p_offset = 0;
	}

	if (spoofonly)
		return (NULL);

	/* obtain buffer to probe drive with */
	bp = geteblk((int)lp->d_secsize);

	/* next, dig out disk label */
	bp->b_dev = dev;
	bp->b_blkno = LABELSECTOR;
	bp->b_cylinder = 0;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags |= B_READ;
	(*strat)(bp);

	/* if successful, locate disk label within block and validate */
	error = biowait(bp);
	if (error == 0) {
		/* Save the whole block in case it has info we need. */
		bcopy(bp->b_data, clp->cd_block, sizeof(clp->cd_block));
	}
	brelse(bp);
	if (error)
		return ("disk label read error");

	/* Check for a NetBSD disk label. */
	dlp = (struct disklabel *) (clp->cd_block + LABELOFFSET);
	if (dlp->d_magic == DISKMAGIC) {
		if (dkcksum(dlp))
			return ("NetBSD disk label corrupted");
		*lp = *dlp;
		return (NULL);
	}

	/* Check for a Sun disk label (for PROM compatibility). */
	slp = (struct sun_disklabel *) clp->cd_block;
	if (slp->sl_magic == SUN_DKMAGIC)
		return (disklabel_sun_to_bsd(clp->cd_block, lp));


	bzero(clp->cd_block, sizeof(clp->cd_block));
	return ("no disk label");
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
	void (*strat) __P((struct buf *));
	struct disklabel *lp;
	struct cpu_disklabel *clp;
{
	struct buf *bp;
	int error;
	struct disklabel *dlp;
	struct sun_disklabel *slp;

	/*
	 * Embed native label in a piece of wasteland.
	 */
	if (sizeof(struct disklabel) > sizeof slp->sl_bsdlabel)
		return EFBIG;

	slp = (struct sun_disklabel *)clp->cd_block;
	bzero(slp->sl_bsdlabel, sizeof(slp->sl_bsdlabel));
	dlp = (struct disklabel *)slp->sl_bsdlabel;
	*dlp = *lp;

	/* Build a SunOS compatible label around the native label */
	error = disklabel_bsd_to_sun(lp, clp->cd_block);
	if (error)
		return (error);

	/* Get a buffer and copy the new label into it. */
	bp = geteblk((int)lp->d_secsize);
	bcopy(clp->cd_block, bp->b_data, sizeof(clp->cd_block));

	/* Write out the updated label. */
	bp->b_dev = dev;
	bp->b_blkno = LABELSECTOR;
	bp->b_cylinder = 0;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags |= B_WRITE;
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
	struct partition *p = lp->d_partitions + DISKPART(bp->b_dev);
	int maxsz = p->p_size;
	int sz = (bp->b_bcount + DEV_BSIZE - 1) >> DEV_BSHIFT;

	/*
	 * overwriting disk label ?
	 * The label is always in sector LABELSECTOR.
	 * XXX should also protect bootstrap in first 8K
	 */
	if (bp->b_blkno + p->p_offset <= LABELSECTOR &&
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

/************************************************************************
 *
 * The rest of this was taken from arch/sparc/scsi/sun_disklabel.c
 * and then substantially rewritten by Gordon W. Ross
 *
 ************************************************************************/

/* What partition types to assume for Sun disklabels: */
static u_char
sun_fstypes[8] = {
	FS_BSDFFS,	/* a */
	FS_SWAP,	/* b */
	FS_OTHER,	/* c - whole disk */
	FS_BSDFFS,	/* d */
	FS_BSDFFS,	/* e */
	FS_BSDFFS,	/* f */
	FS_BSDFFS,	/* g */
	FS_BSDFFS,	/* h */
};

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

	lp->d_npartitions = 8;
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
				npp->p_fsize = 1024;
				npp->p_frag = 8;
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

	if (lp->d_secsize != 512)
		return (EINVAL);

	sl = (struct sun_disklabel *)cp;

	/*
	 * Format conversion.
	 */
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

		/*
		 * SunOS partitions must start on a cylinder boundary.
		 * Note this restriction is forced upon NetBSD/sparc
		 * labels too, since we want to keep both labels
		 * synchronised.
		 */
		if (npp->p_offset % secpercyl)
			return (EINVAL);
		spp->sdkp_cyloffset = npp->p_offset / secpercyl;
		spp->sdkp_nsectors = npp->p_size;
	}
	sl->sl_magic = SUN_DKMAGIC;

	/* Compute the XOR check. */
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
	for (i = 0; i < 126; i++) {
		bblk = ((long)bt->bt_bad[i].bt_cyl << 16) +
			bt->bt_bad[i].bt_trksec;
		if (blk == bblk)
			return (i);
		if (blk < bblk || bblk < 0)
			break;
	}
	return (-1);
}

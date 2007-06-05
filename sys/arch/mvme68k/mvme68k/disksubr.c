/*	$OpenBSD: disksubr.c,v 1.43 2007/06/05 00:38:17 deraadt Exp $	*/
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
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>

static void bsdtocpulabel(struct disklabel *lp,
	struct cpu_disklabel *clp);
static void cputobsdlabel(struct disklabel *lp,
	struct cpu_disklabel *clp);

/*
 * Attempt to read a disk label from a device
 * using the indicated strategy routine.
 * The label must be partly set up before this:
 * secpercyl and anything required in the strategy routine
 * (e.g., sector size) must be filled in before calling us.
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
	char *msg = NULL;
	int error, i;

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

	/* don't read the on-disk label if we are in spoofed-only mode */
	if (spoofonly)
		goto done;

	/* obtain buffer to probe drive with */
	bp = geteblk((int)lp->d_secsize);

	/* request no partition relocation by driver on I/O operations */
	bp->b_dev = dev;
	bp->b_blkno = LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylinder = 0; /* contained in block 0 */
	(*strat)(bp);

	error = biowait(bp);
	if (error == 0)
		bcopy(bp->b_data, clp, sizeof (struct cpu_disklabel));

	if (error) {
		msg = "disk label read error";
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

	if (clp->magic1 != DISKMAGIC || clp->magic2 != DISKMAGIC) {
		msg = "no disk label";
		goto done;
	}

	cputobsdlabel(lp, clp);

	if (dkcksum(lp) != 0) {
		msg = "disk label corrupted";
		goto done;
	}

done:
	if (bp) {
		bp->b_flags = B_INVAL | B_AGE | B_READ;
		brelse(bp);
	}
	disklabeltokernlabel(lp);
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
		return (EINVAL);

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
		if (DL_GETPOFFSET(npp) != DL_GETPOFFSET(opp) ||
		    DL_GETPSIZE(npp) < DL_GETPSIZE(opp))
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
writedisklabel(dev, strat, lp, clp)
	dev_t dev;
	void (*strat)(struct buf *);
	struct disklabel *lp;
	struct cpu_disklabel *clp;
{
	struct buf *bp;
	int error;

	/* obtain buffer to read initial cpu_disklabel, for bootloader size :-) */
	bp = geteblk((int)lp->d_secsize);

	/* request no partition relocation by driver on I/O operations */
	bp->b_dev = dev;
	bp->b_blkno = LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylinder = 0; /* contained in block 0 */
	(*strat)(bp);

	if ((error = biowait(bp)) != 0) {
		/* nothing */
	} else {
		bcopy(bp->b_data, clp, sizeof(struct cpu_disklabel));
	}

	bp->b_flags = B_INVAL | B_AGE | B_READ;
	brelse(bp);

	if (error) {
		return (error);
	}

	bsdtocpulabel(lp, clp);

	if (lp->d_magic == DISKMAGIC && lp->d_magic2 == DISKMAGIC &&
	    dkcksum(lp) == 0) {
		/* obtain buffer to scrozz drive with */
		bp = geteblk((int)lp->d_secsize);

		bcopy(clp, bp->b_data, sizeof(struct cpu_disklabel));

		/* request no partition relocation by driver on I/O operations */
		bp->b_dev = dev;
		bp->b_blkno = 0; /* contained in block 0 */
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_WRITE;
		bp->b_cylinder = 0; /* contained in block 0 */
		(*strat)(bp);

		error = biowait(bp);

		bp->b_flags = B_INVAL | B_AGE | B_READ;
		brelse(bp);
	}
	return (error); 
}


int
bounds_check_with_label(struct buf *bp, struct disklabel *lp,
    struct cpu_disklabel *osdep, int wlabel)
{
#define blockpersec(count, lp) ((count) * (((lp)->d_secsize) / DEV_BSIZE))
	struct partition *p = lp->d_partitions + DISKPART(bp->b_dev);
	int labelsector = blockpersec(DL_GETPOFFSET(&lp->d_partitions[0]), lp) +
	    LABELSECTOR;
	int sz = howmany(bp->b_bcount, DEV_BSIZE);

	/* avoid division by zero */
	if (lp->d_secpercyl == 0) {
		bp->b_error = EINVAL;
		goto bad;
	}

	/* overwriting disk label ? */
	/* XXX should also protect bootstrap in first 8K */
	if (bp->b_blkno + blockpersec(DL_GETPOFFSET(p), lp) <= labelsector &&
#if LABELSECTOR != 0
	    bp->b_blkno + blockpersec(DL_GETPOFFSET(p), lp) + sz > labelsector &&
#endif
	    (bp->b_flags & B_READ) == 0 && wlabel == 0) {
		bp->b_error = EROFS;
		goto bad;
	}

	/* beyond partition? */
	if (bp->b_blkno + sz > blockpersec(DL_GETPSIZE(p), lp)) {
		sz = blockpersec(DL_GETPSIZE(p), lp) - bp->b_blkno;
		if (sz == 0) {
			/* If exactly at end of disk, return EOF. */
			bp->b_resid = bp->b_bcount;
			return(0);
		}
		if (sz < 0) {
			/* If past end of disk, return EINVAL. */
			bp->b_error = EINVAL;
			goto bad;
		}
		/* Otherwise, truncate request. */
		bp->b_bcount = sz << DEV_BSHIFT;
	}

	/* calculate cylinder for disksort to order transfers with */
	bp->b_cylinder = (bp->b_blkno + blockpersec(DL_GETPOFFSET(p), lp)) /
	    lp->d_secpercyl;
	return (1);

bad:
	bp->b_flags |= B_ERROR;
	return (-1);
}


static void
bsdtocpulabel(lp, clp)
	struct disklabel *lp;
	struct cpu_disklabel *clp;
{
	char *tmot = "MOTOROLA";
	char *id = "M68K";
	char *mot;
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
	if (lp->d_trkseek < 6) {
		clp->cfg_ssr = 0;
	} else if (lp->d_trkseek < 10) {
		clp->cfg_ssr = 1;
	} else if (lp->d_trkseek < 15) {
		clp->cfg_ssr = 2;
	} else if (lp->d_trkseek < 20) {
		clp->cfg_ssr = 3;
	} else {
		clp->cfg_ssr = 4;
	}

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
	clp->version = 1;

	/* Put "MOTOROLA" in the VID.  This makes it a valid boot disk. */
	mot = clp->vid_mot;
	for (i = 0; i < 8; i++) {
		*mot++ = *tmot++;
	}
	/* put volume id in the VID */
	mot = clp->vid_id;
	for (i = 0; i < 4; i++) {
		*mot++ = *id++;
	}
}

struct cpu_disklabel_old {
	/* VID */
	u_char		vid_id[4];
	u_char		vid_0[16];
	u_int		vid_oss;
	u_short		vid_osl;
	u_char		vid_1[4];
	u_short		vid_osa_u;
	u_short		vid_osa_l;
	u_char		vid_2[2];
	u_short		partitions;
	u_char		vid_vd[16];
	u_long		bbsize;
	u_long		magic1;		/* 4 */
	u_short		type;		/* 2 */
	u_short		subtype;	/* 2 */
	u_char		packname[16];	/* 16 */
	u_long		flags;		/* 4 */
	u_long		drivedata[5];	/* 4 */
	u_long		spare[5];	/* 4 */
	u_short		checksum;	/* 2 */

	u_long		secpercyl;	/* 4 */
	u_long		secperunit;	/* 4 */
	u_long		headswitch;	/* 4 */

	u_char		vid_3[4];
	u_int		vid_cas;
	u_char		vid_cal;
	u_char		vid_4_0[3];
	u_char		vid_4[64];
	u_char		vid_4_1[28];
	u_long		sbsize;
	u_char		vid_mot[8];

	/* CFG */
	u_char		cfg_0[4];
	u_short		cfg_atm;
	u_short		cfg_prm;
	u_short		cfg_atw;
	u_short		cfg_rec;

	u_short		sparespertrack;
	u_short		sparespercyl;
	u_long		acylinders;
	u_short		rpm;
	u_short		cylskew;

	u_char		cfg_spt;
	u_char		cfg_hds;
	u_short		cfg_trk;
	u_char		cfg_ilv;
	u_char		cfg_sof;
	u_short		cfg_psm;
	u_short		cfg_shd;
	u_char		cfg_2[2];
	u_short		cfg_pcom;
	u_char		cfg_3;
	u_char		cfg_ssr;
	u_short		cfg_rwcc;
	u_short		cfg_ecc;
	u_short		cfg_eatm;
	u_short		cfg_eprm;
	u_short		cfg_eatw;
	u_char		cfg_gpb1;
	u_char		cfg_gpb2;
	u_char		cfg_gpb3;
	u_char		cfg_gpb4;
	u_char		cfg_ssc;
	u_char		cfg_runit;
	u_short		cfg_rsvc1;
	u_short		cfg_rsvc2;
	u_long		magic2;
	u_char		cfg_4[192];
};

static void
cputobsdlabel(lp, clp)
	struct disklabel *lp;
	struct cpu_disklabel *clp;
{
	int i;

	if (clp->version == 0) {
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
		case 0:
			lp->d_trkseek = 0;
			break;
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
		lp->d_checksum = clp->checksum;
		lp->d_npartitions = clp->partitions;
		lp->d_bbsize = clp->bbsize;
		lp->d_sbsize = clp->sbsize;
		bcopy(clp->vid_4, &lp->d_partitions[0], sizeof(struct partition) * 4);
		bcopy(clp->cfg_4, &lp->d_partitions[4], sizeof(struct partition) * 12);
		lp->d_checksum = 0;
		lp->d_checksum = dkcksum(lp);
	} else {
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
		case 0:
			lp->d_trkseek = 0;
			break;
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
		lp->d_checksum = clp->checksum;
		lp->d_npartitions = clp->partitions;
		lp->d_bbsize = clp->bbsize;
		lp->d_sbsize = clp->sbsize;
		bcopy(clp->vid_4, &lp->d_partitions[0], sizeof(struct partition) * 4);
		bcopy(clp->cfg_4, &lp->d_partitions[4], sizeof(struct partition) * 12);
		lp->d_checksum = 0;
		lp->d_checksum = dkcksum(lp);
	}
}

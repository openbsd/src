/*	$OpenBSD: disksubr.c,v 1.16 1998/10/03 21:18:55 millert Exp $ */

/*
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *   This product includes software developed by Dale Rahn.
 * 4. The name of the author may not be used to endorse or promote products
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
#include <sys/buf.h>
#include <sys/device.h>
#define DKTYPENAMES
#include <sys/disklabel.h>
#include <sys/disk.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/autoconf.h>

#define b_cylin b_resid

#ifdef DEBUG
int disksubr_debug = 0;
#endif

static void bsdtocpulabel __P((struct disklabel *lp,
	struct cpu_disklabel *clp));
static void cputobsdlabel __P((struct disklabel *lp,
	struct cpu_disklabel *clp));

#ifdef DEBUG
static void printlp __P((struct disklabel *lp, char *str));
static void printclp __P((struct cpu_disklabel *clp, char *str));
#endif

void
dk_establish(dk, dev)
	struct disk *dk;
	struct device *dev;
{
	struct scsibus_softc *sbsc;
	int target, lun;

	if (bootpart == -1) /* ignore flag from controller driver? */
		return;

	/*
	 * scsi: sd,cd
	 */

	if (strncmp("sd", dev->dv_xname, 2) == 0 ||
	    strncmp("cd", dev->dv_xname, 2) == 0) {

		sbsc = (struct scsibus_softc *)dev->dv_parent;
		if (cputyp == CPU_147) {
			target = bootctrllun % 8; /* XXX: 147 only */
			lun = bootdevlun; /* XXX: 147, untested */
		} else {
			/* 
			 * XXX: on the 167: 
			 * ignore bootctrllun
			 */
			target = bootdevlun / 10;
			lun = bootdevlun % 10;
		}

		if (sbsc->sc_link[target][lun] != NULL &&
		    sbsc->sc_link[target][lun]->device_softc == (void *)dev) {
			bootdv = dev;
			return;
		}
	}
}

/*
 * Attempt to read a disk label from a device
 * using the indicated stategy routine.
 * The label must be partly set up before this:
 * secpercyl and anything required in the strategy routine
 * (e.g., sector size) must be filled in before calling us.
 * Returns null on success and an error string on failure.
 */
char *
readdisklabel(dev, strat, lp, clp, spoofonly)
	dev_t dev;
	void (*strat)();
	struct disklabel *lp;
	struct cpu_disklabel *clp;
	int spoofonly;
{
	struct buf *bp;
	char *msg = NULL;

	/* minimal requirements for archetypal disk label */
	if (lp->d_secsize == 0)
		lp->d_secsize = DEV_BSIZE;
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;
	lp->d_npartitions = RAW_PART + 1;
	for (i = 0; i < RAW_PART; i++) {
		lp->d_partitions[i].p_size = 0;
		lp->d_partitions[i].p_offset = 0;
	}
	if (lp->d_partitions[0].p_size == 0)
		lp->d_partitions[0].p_size = 0x1fffffff;
	lp->d_partitions[0].p_offset = 0;

	/* don't read the on-disk label if we are in spoofed-only mode */
	if (spoofonly)
		return (NULL);

	/* obtain buffer to probe drive with */
	bp = geteblk((int)lp->d_secsize);

	/* request no partition relocation by driver on I/O operations */
	bp->b_dev = dev;
	bp->b_blkno = 0; /* contained in block 0 */
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylin = 0; /* contained in block 0 */
	(*strat)(bp);

	if (biowait(bp)) {
		msg = "cpu_disklabel read error\n";
	} else {
		bcopy(bp->b_data, clp, sizeof (struct cpu_disklabel));
	}

	bp->b_flags = B_INVAL | B_AGE | B_READ;
	brelse(bp);

	if (msg) {
#if defined(CD9660)
		if (iso_disklabelspoof(dev, strat, lp) == 0)
			msg = NULL;
#endif
		return (msg); 
	}
	cputobsdlabel(lp, clp);
#ifdef DEBUG
	if (disksubr_debug > 0) {
		printlp(lp, "readdisklabel:bsd label");
		printclp(clp, "readdisklabel:cpu label");
	}
#endif
	return (msg);
}

/*
 * Check new disk label for sensibility
 * before setting it.
 */
int
setdisklabel(olp, nlp, openmask, clp)
	register struct disklabel *olp, *nlp;
	u_long openmask;
	struct cpu_disklabel *clp;
{
	register i;
	register struct partition *opp, *npp;

#ifdef DEBUG
	if(disksubr_debug > 0) {
		printlp(nlp, "setdisklabel:new disklabel");
		printlp(olp, "setdisklabel:old disklabel");
		printclp(clp, "setdisklabel:cpu disklabel");
	}
#endif


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
		if (npp->p_offset != opp->p_offset || npp->p_size < opp->p_size)
			return (EBUSY);
		/*
		 * Copy internally-set partition information
		 * if new label doesn't include it.		XXX
		 */
		if (npp->p_fstype == FS_UNUSED && opp->p_fstype != FS_UNUSED) {
			npp->p_fstype = opp->p_fstype;
			npp->p_fsize = opp->p_fsize;
			npp->p_frag = opp->p_frag;
			npp->p_cpg = opp->p_cpg;
		}
	}

 	nlp->d_checksum = 0;
 	nlp->d_checksum = dkcksum(nlp);
	*olp = *nlp;
#ifdef DEBUG
	if(disksubr_debug > 0) {
		printlp(olp, "setdisklabel:old->new disklabel");
	}
#endif
	return (0);
}

/*
 * Write disk label back to device after modification.
 */
writedisklabel(dev, strat, lp, clp)
	dev_t dev;
	void (*strat)();
	register struct disklabel *lp;
	struct cpu_disklabel *clp;
{
	struct buf *bp;
	int error;

#ifdef DEBUG
	if(disksubr_debug > 0) {
		printlp(lp, "writedisklabel: bsd label");
	}
#endif

	/* obtain buffer to read initial cpu_disklabel, for bootloader size :-) */
	bp = geteblk((int)lp->d_secsize);

	/* request no partition relocation by driver on I/O operations */
	bp->b_dev = dev;
	bp->b_blkno = 0; /* contained in block 0 */
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylin = 0; /* contained in block 0 */
	(*strat)(bp);

	if (error = biowait(bp)) {
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

#ifdef DEBUG
	if (disksubr_debug > 0) {
		printclp(clp, "writedisklabel: cpu label");
	}
#endif

	/* obtain buffer to scrozz drive with */
	bp = geteblk((int)lp->d_secsize);
	bcopy(clp, bp->b_data, sizeof(struct cpu_disklabel));
	/* request no partition relocation by driver on I/O operations */
	bp->b_dev = dev;
	bp->b_blkno = 0; /* contained in block 0 */
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_WRITE;
	bp->b_cylin = 0; /* contained in block 0 */
	(*strat)(bp);
	error = biowait(bp);
	bp->b_flags = B_INVAL | B_AGE | B_READ;
	brelse(bp);
	return (error); 
}


int
bounds_check_with_label(bp, lp, osdep, wlabel)
	struct buf *bp;
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
	int wlabel;
{
#define blockpersec(count, lp) ((count) * (((lp)->d_secsize) / DEV_BSIZE))
	struct partition *p = lp->d_partitions + DISKPART(bp->b_dev);
	int labelsect = blockpersec(lp->d_partitions[0].p_offset, lp) +
	    LABELSECTOR;
	int sz = howmany(bp->b_bcount, DEV_BSIZE);

	/* overwriting disk label ? */
	/* XXX should also protect bootstrap in first 8K */
	if (bp->b_blkno + blockpersec(p->p_offset, lp) <= labelsect &&
#if LABELSECTOR != 0
	    bp->b_blkno + blockpersec(p->p_offset, lp) + sz > labelsect &&
#endif
	    (bp->b_flags & B_READ) == 0 && wlabel == 0) {
		bp->b_error = EROFS;
		goto bad;
	}

	/* beyond partition? */
	if (bp->b_blkno + sz > blockpersec(p->p_size, lp)) {
		sz = blockpersec(p->p_size, lp) - bp->b_blkno;
		if (sz == 0) {
			/* if exactly at end of disk, return an EOF */
			bp->b_resid = bp->b_bcount;
			return(0);
		}
		if (sz < 0) {
			bp->b_error = EINVAL;
			goto bad;
		}
		/* or truncate if part of it fits */
		bp->b_bcount = sz << DEV_BSHIFT;
	}

	/* calculate cylinder for disksort to order transfers with */
	bp->b_cylin = (bp->b_blkno + blockpersec(p->p_offset, lp)) /
	    lp->d_secpercyl;
	return(1);

bad:
	bp->b_flags |= B_ERROR;
	return(-1);
}


static void
bsdtocpulabel(lp, clp)
	struct disklabel *lp;
	struct cpu_disklabel *clp;
{
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
	clp->secperunit = lp->d_secperunit;
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
	struct disklabel llp;
	struct disklabel *nlp = &llp;

	bzero (&llp, sizeof (llp));
	if (clp->version == 0) {
		struct cpu_disklabel_old *clpo = (void *) clp;
		nlp->d_magic = clpo->magic1;
		nlp->d_type = clpo->type;
		nlp->d_subtype = clpo->subtype;
		strncpy(nlp->d_typename, clpo->vid_vd, 16);
		strncpy(nlp->d_packname, clpo->packname, 16);
		nlp->d_secsize = clpo->cfg_psm;
		nlp->d_nsectors = clpo->cfg_spt;
		nlp->d_ncylinders = clpo->cfg_trk; /* trk is really num of cyl! */
		nlp->d_ntracks = clpo->cfg_hds;
		nlp->d_secpercyl = clpo->secpercyl;
		nlp->d_secperunit = clpo->secperunit;
		nlp->d_secpercyl = clpo->secpercyl;
		nlp->d_secperunit = clpo->secperunit;
		nlp->d_sparespertrack = clpo->sparespertrack;
		nlp->d_sparespercyl = clpo->sparespercyl;
		nlp->d_acylinders = clpo->acylinders;
		nlp->d_rpm = clpo->rpm;
		nlp->d_interleave = clpo->cfg_ilv;
		nlp->d_trackskew = clpo->cfg_sof;
		nlp->d_cylskew = clpo->cylskew;
		nlp->d_headswitch = clpo->headswitch;
		/* this silly table is for winchester drives */
		switch (clpo->cfg_ssr) {
		case 0:
			nlp->d_trkseek = 0;
			break;
		case 1:
			nlp->d_trkseek = 6;
			break;
		case 2:
			nlp->d_trkseek = 10;
			break;
		case 3:
			nlp->d_trkseek = 15;
			break;
		case 4:
			nlp->d_trkseek = 20;
			break;
		default:
			nlp->d_trkseek = 0;
		}
		nlp->d_flags = clpo->flags;
		for (i = 0; i < NDDATA; i++)
			nlp->d_drivedata[i] = clpo->drivedata[i];
		for (i = 0; i < NSPARE; i++)
			nlp->d_spare[i] = clpo->spare[i];

		nlp->d_magic2 = clpo->magic2;
		nlp->d_checksum = clpo->checksum;
		nlp->d_npartitions = clpo->partitions;
		nlp->d_bbsize = clpo->bbsize;
		nlp->d_sbsize = clpo->sbsize;
		bcopy(clpo->vid_4, &nlp->d_partitions[0], sizeof(struct partition) * 4);
		bcopy(clpo->cfg_4, &nlp->d_partitions[4], sizeof(struct partition) * 12);
#if 0
		nlp->d_checksum = 0;
		nlp->d_checksum = dkcksum(nlp);
#endif
	} else {
		nlp->d_magic = clp->magic1;
		nlp->d_type = clp->type;
		nlp->d_subtype = clp->subtype;
		strncpy(nlp->d_typename, clp->vid_vd, 16);
		strncpy(nlp->d_packname, clp->packname, 16);
		nlp->d_secsize = clp->cfg_psm;
		nlp->d_nsectors = clp->cfg_spt;
		nlp->d_ncylinders = clp->cfg_trk; /* trk is really num of cyl! */
		nlp->d_ntracks = clp->cfg_hds;

		nlp->d_secpercyl = clp->secpercyl;
		nlp->d_secperunit = clp->secperunit;
		nlp->d_secpercyl = clp->secpercyl;
		nlp->d_secperunit = clp->secperunit;
		nlp->d_sparespertrack = clp->sparespertrack;
		nlp->d_sparespercyl = clp->sparespercyl;
		nlp->d_acylinders = clp->acylinders;
		nlp->d_rpm = clp->rpm;
		nlp->d_interleave = clp->cfg_ilv;
		nlp->d_trackskew = clp->cfg_sof;
		nlp->d_cylskew = clp->cylskew;
		nlp->d_headswitch = clp->headswitch;

		/* this silly table is for winchester drives */
		switch (clp->cfg_ssr) {
		case 0:
			nlp->d_trkseek = 0;
			break;
		case 1:
			nlp->d_trkseek = 6;
			break;
		case 2:
			nlp->d_trkseek = 10;
			break;
		case 3:
			nlp->d_trkseek = 15;
			break;
		case 4:
			nlp->d_trkseek = 20;
			break;
		default:
			nlp->d_trkseek = 0;
		}
		nlp->d_flags = clp->flags;
		for (i = 0; i < NDDATA; i++)
			nlp->d_drivedata[i] = clp->drivedata[i];
		for (i = 0; i < NSPARE; i++)
			nlp->d_spare[i] = clp->spare[i];

		nlp->d_magic2 = clp->magic2;
		nlp->d_checksum = clp->checksum;
		nlp->d_npartitions = clp->partitions;
		nlp->d_bbsize = clp->bbsize;
		nlp->d_sbsize = clp->sbsize;
		bcopy(clp->vid_4, &nlp->d_partitions[0], sizeof(struct partition) * 4);
		bcopy(clp->cfg_4, &nlp->d_partitions[4], sizeof(struct partition) * 12);
#if 0
		nlp->d_checksum = 0;
		nlp->d_checksum = dkcksum(nlp);
#endif
	}
	{
		int oldcksum;

		oldcksum = nlp->d_checksum;
		nlp->d_checksum = 0;
#ifdef DEBUG
printf("old chksum = %x new %x\n", oldcksum, dkcksum(nlp));
	printlp(nlp, "lp disklabel");
	printclp(clp, "clp disklabel");
#endif
		if ((nlp->d_magic == DISKMAGIC) && (oldcksum == dkcksum(nlp))) {
			nlp->d_checksum = oldcksum;
			bcopy (nlp, lp, sizeof (*lp));
		}
	}
	
#ifdef DEBUG
	if (disksubr_debug > 0) {
		printlp(lp, "translated label read from disk\n");
	}
#endif
}

#ifdef DEBUG
static void
printlp(lp, str)
	struct disklabel *lp;
	char *str;
{
	int i;

	printf("%s\n", str);
	printf("magic1 %x\n", lp->d_magic);
	printf("magic2 %x\n", lp->d_magic2);
	printf("typename %s\n", lp->d_typename);
	printf("secsize %x nsect %x ntrack %x ncylinders %x\n",
	    lp->d_secsize, lp->d_nsectors, lp->d_ntracks, lp->d_ncylinders);
	printf("Num partitions %x\n", lp->d_npartitions);
	for (i = 0; i < lp->d_npartitions; i++) {
		struct partition *part = &lp->d_partitions[i];
		char *fstyp = fstypenames[part->p_fstype];
		
		printf("%c: size %10x offset %10x type %7s frag %5x cpg %3x\n",
		    'a' + i, part->p_size, part->p_offset, fstyp,
		    part->p_frag, part->p_cpg);
	}
}

static void
printclp(clp, str)
	struct cpu_disklabel *clp;
	char *str;
{
	int max, i;

	if (clp->version == 0) {
		printf("cannot print old version cpudisklabel\n");
		return;
	}
	printf("%s\n", str);
	printf("magic1 %x\n", clp->magic1);
	printf("magic2 %x\n", clp->magic2);
	printf("typename %s\n", clp->vid_vd);
	printf("secsize %x nsect %x ntrack %x ncylinders %x\n",
	    clp->cfg_psm, clp->cfg_spt, clp->cfg_hds, clp->cfg_trk);
	printf("Num partitions %x\n", clp->partitions);
	max = clp->partitions < 16 ? clp->partitions : 16;
	for (i = 0; i < max; i++) {
		struct partition *part;
		char *fstyp;

		if (i < 4) {
			part = (void *)&clp->vid_4[0];
			part = &part[i];
		} else {
			part = (void *)&clp->cfg_4[0];
			part = &part[i-4];
		}

		fstyp = fstypenames[part->p_fstype];
		
		printf("%c: size %10x offset %10x type %7s frag %5x cpg %3x\n",
		    'a' + i, part->p_size, part->p_offset, fstyp,
		    part->p_frag, part->p_cpg);
	}
}
#endif

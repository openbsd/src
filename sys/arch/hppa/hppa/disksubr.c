/*	$OpenBSD: disksubr.c,v 1.12 2003/02/18 19:01:50 deraadt Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

/*
 * This disksubr.c module started to take it's present form on OpenBSD/alpha
 * but it was always thought it should be made completely MI and not need to
 * be in that alpha-specific tree at all.
 *
 * XXX The DOS partitioning code is not endian-independent, only native
 * endian DOS partition tables can be parsed yet.
 *
 * XXX Amiga RDB partitioning is not understood yet.
 *
 * XXX HPUX disklabel is not understood yet.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>
#include <sys/disk.h>

/* The native defaults... */
#if defined(alpha) && !defined(DISKLABEL_ALPHA)
#define DISKLABEL_ALPHA
#elif (defined(i386) || defined(arc)) && !defined(DISKLABEL_I386)
#define DISKLABEL_I386
#elif defined(amiga) && !defined(DISKLABEL_AMIGA)
#define DISKLABEL_AMIGA
#elif defined(hppa) && !defined(DISKLABEL_HPPA)
#define DISKLABEL_HPPA
#endif

#define	b_cylin	b_resid

#if defined(DISKLABEL_I386) || defined(DISKLABEL_ALPHA) || defined(DISKLABEL_AMIGA) || defined(DISKLABEL_HPPA) || defined(DISKLABEL_ALL)
void	swapdisklabel(struct disklabel *d);
char   *readbsdlabel(struct buf *, void (*)(struct buf *), int, int,
    int, int, struct disklabel *, int);
#endif
#if defined(DISKLABEL_I386) || defined(DISKLABEL_ALL)
char   *readdoslabel(struct buf *, void (*)(struct buf *),
    struct disklabel *, struct cpu_disklabel *, int *, int *, int);
#endif
#if defined(DISKLABEL_AMIGA) || defined(DISKLABEL_ALL)
char   *readamigalabel(struct buf *, void (*)(struct buf *),
    struct disklabel *, struct cpu_disklabel *, int);
#endif
#if defined(DISKLABEL_HPPA) || defined(DISKLABEL_ALL)
char   *readliflabel(struct buf *, void (*)(struct buf *),
    struct disklabel *, struct cpu_disklabel *, int *, int *, int);
#endif

static enum disklabel_tag probe_order[] = { LABELPROBES, -1 };

void
dk_establish(dk, dev)
	struct disk *dk;
	struct device *dev;
{
}

#if defined(DISKLABEL_I386) || defined(DISKLABEL_ALPHA) || defined(DISKLABEL_AMIGA) || defined(DISKLABEL_HPPA) || defined(DISKLABEL_ALL)

/*
 * Byteswap all the fields that might be swapped.
 */
void
swapdisklabel(dlp)
	struct disklabel *dlp;
{
	int i;
	struct partition *pp;

	swap32(dlp->d_magic);
	swap16(dlp->d_type);
	swap16(dlp->d_subtype);
	swap32(dlp->d_secsize);
	swap32(dlp->d_nsectors);
	swap32(dlp->d_ntracks);
	swap32(dlp->d_ncylinders);
	swap32(dlp->d_secpercyl);
	swap32(dlp->d_secperunit);
	swap16(dlp->d_sparespertrack);
	swap16(dlp->d_sparespercyl);
	swap32(dlp->d_acylinders);
	swap16(dlp->d_rpm);
	swap16(dlp->d_interleave);
	swap16(dlp->d_trackskew);
	swap16(dlp->d_cylskew);
	swap32(dlp->d_headswitch);
	swap32(dlp->d_trkseek);
	swap32(dlp->d_flags);
	for (i = 0; i < NDDATA; i++)
		swap32(dlp->d_drivedata[i]);
	for (i = 0; i < NSPARE; i++)
		swap32(dlp->d_spare[i]);
	swap32(dlp->d_magic2);
	swap16(dlp->d_checksum);
	swap16(dlp->d_npartitions);
	swap32(dlp->d_bbsize);
	swap32(dlp->d_sbsize);
	for (i = 0; i < MAXPARTITIONS; i++) {
		pp = &dlp->d_partitions[i];
		swap32(pp->p_size);
		swap32(pp->p_offset);
		swap32(pp->p_fsize);
		swap16(pp->p_cpg);
	}
}

/*
 * Try to read a standard BSD disklabel at a certain sector.
 */
char *
readbsdlabel(bp, strat, cyl, sec, off, endian, lp, spoofonly)
	struct buf *bp;
	void (*strat)(struct buf *);
	int cyl, sec, off, endian;
	struct disklabel *lp;
	int spoofonly;
{
	struct disklabel *dlp;
	char *msg = NULL;
	u_int16_t cksum;
	u_int32_t magic;

	if (endian != LITTLE_ENDIAN && endian != BIG_ENDIAN)
		panic("readbsdlabel: unsupported byteorder %d", endian);

	/* don't read the on-disk label if we are in spoofed-only mode */
	if (spoofonly)
		return (NULL);

	bp->b_blkno = sec;
	bp->b_cylin = cyl;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);

	/* if successful, locate disk label within block and validate */
	if (biowait(bp)) {
		/* XXX we return the faked label built so far */
		msg = "disk label I/O error";
		return (msg);
	}

	magic = endian == BIG_ENDIAN ? htobe32(DISKMAGIC) : htole32(DISKMAGIC);

	/*
	 * If off is negative, search until the end of the sector for
	 * the label, otherwise, just look at the specific location
	 * we're given.
	 */
	dlp = (struct disklabel *)(bp->b_data + (off >= 0 ? off : 0));
	do {
		if (dlp->d_magic != magic || dlp->d_magic2 != magic) {
			if (msg == NULL)
				msg = "no disk label";
		} else {
			cksum = dkcksum(dlp);
			if (endian != BYTE_ORDER)
				swapdisklabel(dlp);
			if (dlp->d_npartitions > MAXPARTITIONS || cksum != 0) {
				msg = "disk label corrupted";
				/* swap back if necessary.  */
				if (off < 0 && endian != BYTE_ORDER)
					swapdisklabel(dlp);
			} else {
				*lp = *dlp;
				/* Recalc magic on foreign labels */
				if (endian != BYTE_ORDER) {
					lp->d_checksum = 0;
					lp->d_checksum = dkcksum(lp);
				}
				msg = NULL;
				break;
			}
		}
		if (off >= 0)
			break;
		dlp = (struct disklabel *)((char *)dlp + sizeof(int32_t));
	} while (dlp <= (struct disklabel *)(bp->b_data + lp->d_secsize -
	    sizeof(*dlp)));
	return (msg);
}
#endif

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
readdisklabel(dev, strat, lp, osdep, spoofonly)
	dev_t dev;
	void (*strat)(struct buf *);
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
	int spoofonly;
{
	struct buf *bp = NULL;
	char *msg = "no disk label";
	enum disklabel_tag *tp;
	int i;
	struct disklabel minilabel, fallbacklabel;

	/* minimal requirements for archtypal disk label */
	if (lp->d_secsize == 0)
		lp->d_secsize = DEV_BSIZE;
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;
	if (lp->d_secpercyl == 0) {
		msg = "invalid geometry";
		goto done;
	}
	lp->d_npartitions = RAW_PART + 1;
	for (i = 0; i < RAW_PART; i++) {
		lp->d_partitions[i].p_size = 0;
		lp->d_partitions[i].p_offset = 0;
	}
	if (lp->d_partitions[i].p_size == 0)
		lp->d_partitions[i].p_size = 0x1fffffff;
	lp->d_partitions[i].p_offset = 0;
	minilabel = fallbacklabel = *lp;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	for (tp = probe_order; msg && *tp != -1; tp++) {
		switch (*tp) {
		case DLT_ALPHA:
#if defined(DISKLABEL_ALPHA) || defined(DISKLABEL_ALL)
			msg = readbsdlabel(bp, strat, 0, ALPHA_LABELSECTOR,
			    ALPHA_LABELOFFSET, LITTLE_ENDIAN, lp, spoofonly);
#endif
			break;

		case DLT_I386:
#if defined(DISKLABEL_I386) || defined(DISKLABEL_ALL)
			msg = readdoslabel(bp, strat, lp, osdep, 0, 0, spoofonly);
			if (msg)
				/* Fallback alternative */
				fallbacklabel = *lp;
#endif
			break;

		case DLT_AMIGA:
#if defined(DISKLABEL_AMIGA) || defined(DISKLABEL_ALL)
			msg = readamigalabel(bp, strat, lp, osdep, spoofonly);
#endif
			break;

		case DLT_HPPA:
#if defined(DISKLABEL_HPPA) || defined(DISKLABEL_ALL)
			msg = readliflabel(bp, strat, lp, osdep, 0, 0, spoofonly);
#endif
			break;

		default:
			panic("unrecognized disklabel tag %d", *tp);
		}
		if (msg)
			*lp = minilabel;
	}

	/* Record metainformation about the disklabel.  */
	if (msg == NULL) {
		osdep->labelsector = bp->b_blkno;
		osdep->labeltag = *tp;
	}

#if defined(CD9660)
	if (msg && iso_disklabelspoof(dev, strat, lp) == 0)
		msg = NULL;
#endif

	/* If there was an error, still provide a decent fake one.  */
	if (msg)
		*lp = fallbacklabel;

done:
	if (bp) {
		bp->b_flags |= B_INVAL;
		brelse(bp);
	}
	return (msg);
}

#if defined(DISKLABEL_I386) || defined(DISKLABEL_ALL)
/*
 * If dos partition table requested, attempt to load it and
 * find disklabel inside a DOS partition. Also, if bad block
 * table needed, attempt to extract it as well. Return buffer
 * for use in signalling errors if requested.
 *
 * We would like to check if each MBR has a valid BOOT_MAGIC, but
 * we cannot because it doesn't always exist. So.. we assume the
 * MBR is valid.
 */
char *
readdoslabel(bp, strat, lp, osdep, partoffp, cylp, spoofonly)
	struct buf *bp;
	void (*strat)(struct buf *);
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
	int *partoffp;
	int *cylp;
	int spoofonly;
{
	struct dos_partition *dp = osdep->u._i386.dosparts, *dp2;
	struct dkbad *db, *bdp = &DKBAD(osdep);
	char *msg = NULL, *cp;
	int dospartoff, cyl, i, ourpart = -1;
	dev_t dev;

	if (lp->d_secpercyl == 0) {
		msg = "invalid label, d_secpercyl == 0";
		return (msg);
	}
	if (lp->d_secsize == 0) {
		msg = "invalid label, d_secsize == 0";
		return (msg);
	}

	/* do dos partitions in the process of getting disklabel? */
	dospartoff = 0;
	cyl = I386_LABELSECTOR / lp->d_secpercyl;
	if (dp) {
		daddr_t part_blkno = DOSBBSECTOR;
		unsigned long extoff = 0;
		int wander = 1, n = 0, loop = 0;

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
			bp->b_cylin = part_blkno / lp->d_secpercyl;
			(*strat)(bp);

			/* if successful, wander through dos partition table */
			if (biowait(bp)) {
				msg = "dos partition I/O error";
				if (partoffp)
					*partoffp = -1;
				return (msg);
			}
			bcopy(bp->b_data + DOSPARTOFF, dp,
			    NDOSPART * sizeof(*dp));

			if (ourpart == -1) {
				/* Search for our MBR partition */
				for (dp2=dp, i=0;
				    i < NDOSPART && ourpart == -1; i++, dp2++)
					if (dp2->dp_size &&
					    dp2->dp_typ == DOSPTYP_OPENBSD)
						ourpart = i;
				for (dp2=dp, i=0;
				    i < NDOSPART && ourpart == -1; i++, dp2++)
					if (dp2->dp_size &&
					    dp2->dp_typ == DOSPTYP_FREEBSD)
						ourpart = i;
				for (dp2=dp, i=0;
				    i < NDOSPART && ourpart == -1; i++, dp2++)
					if (dp2->dp_size &&
					    dp2->dp_typ == DOSPTYP_NETBSD)
						ourpart = i;
				if (ourpart == -1)
					goto donot;
				/*
				 * This is our MBR partition. need sector
				 * address for SCSI/IDE, cylinder for
				 * ESDI/ST506/RLL
				 */
				dp2 = &dp[ourpart];
				dospartoff = dp2->dp_start + part_blkno;
				cyl = DPCYL(dp2->dp_scyl, dp2->dp_ssect);

				/* XXX build a temporary disklabel */
				lp->d_partitions[0].p_size = dp2->dp_size;
				lp->d_partitions[0].p_offset = dp2->dp_start +
				    part_blkno;
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
				struct partition *pp = &lp->d_partitions[8+n];

				if (dp2->dp_typ == DOSPTYP_OPENBSD)
					continue;
				if (dp2->dp_size)
					pp->p_size = dp2->dp_size;
				if (dp2->dp_start)
					pp->p_offset =
					    dp2->dp_start + part_blkno;

				switch (dp2->dp_typ) {
				case DOSPTYP_UNUSED:
					for (cp = (char *)dp2;
					    cp < (char *)(dp2 + 1); cp++)
						if (*cp)
							break;
					/*
					 * Was it all zeroes?  If so, it is
					 * an unused entry that we don't
					 * want to show.
					 */
					if (cp == (char *)(dp2 + 1))
					    continue;
					lp->d_partitions[8 + n++].p_fstype =
					    FS_UNUSED;
					break;

				case DOSPTYP_LINUX:
					pp->p_fstype = FS_EXT2FS;
					n++;
					break;

				case DOSPTYP_FAT12:
				case DOSPTYP_FAT16S:
				case DOSPTYP_FAT16B:
				case DOSPTYP_FAT32:
				case DOSPTYP_FAT32L:
				case DOSPTYP_FAT16L:
					pp->p_fstype = FS_MSDOS;
					n++;
					break;
				case DOSPTYP_EXTEND:
					part_blkno = dp2->dp_start + extoff;
					if (!extoff) {
						extoff = dp2->dp_start;
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
		lp->d_npartitions = n > 0 ? n + 8 : 3;
	}

	/* record the OpenBSD partition's placement for the caller */
	if (partoffp)
		*partoffp = dospartoff;
	if (cylp)
		*cylp = cyl;

	/* next, dig out disk label */
	msg = readbsdlabel(bp, strat, cyl, dospartoff + I386_LABELSECTOR, -1,
	    LITTLE_ENDIAN, lp, spoofonly);
	if (msg)
		return (msg);

	/* obtain bad sector table if requested and present */
	if (bdp && (lp->d_flags & D_BADSECT)) {
		/*
		 * get a new buffer and initialize it as callers trust the
		 * buffer given to us, to point at the disklabel sector.
		 */
		dev = bp->b_dev;
		bp = geteblk((int)lp->d_secsize);
		bp->b_dev = dev;

		i = 0;
		do {
			/* read a bad sector table */
			bp->b_flags = B_BUSY | B_READ;
			bp->b_blkno = lp->d_secperunit - lp->d_nsectors + i;
			if (lp->d_secsize > DEV_BSIZE)
				bp->b_blkno *= lp->d_secsize / DEV_BSIZE;
			else
				bp->b_blkno /= DEV_BSIZE / lp->d_secsize;
			bp->b_bcount = lp->d_secsize;
			bp->b_cylin = lp->d_ncylinders - 1;
			(*strat)(bp);

			/* if successful, validate, otherwise try another */
			if (biowait(bp))
				msg = "bad sector table I/O error";
			else {
				db = (struct dkbad *)(bp->b_data);
#define DKBAD_MAGIC 0x4321
				if (db->bt_mbz == 0 &&
				    db->bt_flag == DKBAD_MAGIC) {
					msg = NULL;
					*bdp = *db;
					break;
				} else
					msg = "bad sector table corrupted";
			}
		} while ((bp->b_flags & B_ERROR) && (i += 2) < 10 &&
		    i < lp->d_nsectors);

		/* Give back the bad block buffer.  */
		bp->b_flags |= B_INVAL;
		brelse(bp);
	}
	return (msg);
}
#endif

#if defined(DISKLABEL_AMIGA) || defined(DISKLABEL_ALL)
/*
 * XXX RDB parsing is missing still.
 */
char *
readamigalabel(bp, strat, lp, osdep, spoofonly)
	struct buf *bp;
	void (*strat)(struct buf *);
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
	int spoofonly;
{
	char *msg;

	msg = readbsdlabel(bp, strat, 0, AMIGA_LABELSECTOR, AMIGA_LABELOFFSET,
	    BIG_ENDIAN, lp, spoofonly);
	return (msg);
}
#endif

#if defined(DISKLABEL_HPPA) || defined(DISKLABEL_ALL)
char *
readliflabel (bp, strat, lp, osdep, partoffp, cylp, spoofonly)
	struct buf *bp;
	void (*strat)(struct buf *);
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
	int *partoffp;
	int *cylp;
	int spoofonly;
{
	int fsoff;

	/* read LIF volume header */
	bp->b_blkno = btodb(LIF_VOLSTART);
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylin = btodb(LIF_VOLSTART) / lp->d_secpercyl;
	(*strat)(bp);

	if (biowait(bp)) {
		if (partoffp)
			*partoffp = -1;
		return "LIF volume header I/O error";
	}

	bcopy (bp->b_data, &osdep->u._hppa.lifvol, sizeof(struct lifvol));
	if (osdep->u._hppa.lifvol.vol_id != LIF_VOL_ID) {
		fsoff = 0;
	} else {
		register struct lifdir *p;
		dev_t dev;

		dev = bp->b_dev;
		bp = geteblk(LIF_DIRSIZE);
		bp->b_dev = dev;

		/* read LIF directory */
		bp->b_blkno = btodb(LIF_DIRSTART);
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		bp->b_cylin = (LIF_DIRSTART) / lp->d_secpercyl;
		(*strat)(bp);

		if (biowait(bp)) {
			if (partoffp)
				*partoffp = -1;

			bp->b_flags |= B_INVAL;
			brelse(bp);
			return ("LIF directory I/O error");
		}

		bcopy(bp->b_data, osdep->u._hppa.lifdir, LIF_DIRSIZE);
		bp->b_flags |= B_INVAL;
		brelse(bp);

		/* scan for LIF_DIR_FS dir entry */
		for (fsoff = -1,  p = &osdep->u._hppa.lifdir[0];
		    fsoff < 0 && p < &osdep->u._hppa.lifdir[LIF_NUMDIR]; p++)
			if (p->dir_type == LIF_DIR_FS)
				fsoff = lifstodb(p->dir_addr);

		/* if no suitable lifdir entry found assume zero */
		if (fsoff < 0)
			fsoff = 0;
	}

	if (partoffp)
		*partoffp = fsoff;

	return readbsdlabel(bp, strat, 0,  fsoff + HPPA_LABELSECTOR,
	    HPPA_LABELOFFSET, BIG_ENDIAN, lp, spoofonly);
}
#endif

/*
 * Check new disk label for sensibility
 * before setting it.
 */
int
setdisklabel(olp, nlp, openmask, osdep)
	struct disklabel *olp, *nlp;
	u_long openmask;
	struct cpu_disklabel *osdep;
{
	int i;
	struct partition *opp, *npp;

	/* sanity clause */
	if (nlp->d_secpercyl == 0 || nlp->d_secsize == 0 ||
	    (nlp->d_secsize % DEV_BSIZE) != 0)
		return(EINVAL);

	/*
	 * XXX Nice thought, but it doesn't work, if the intention was to
	 * force a reread at the next *readdisklabel call.  That does not
	 * happen.  There's still some use for it though as you can pseudo-
	 * partitition the disk.
	 *
	 * Special case to allow disklabel to be invalidated.
	 */
	if (nlp->d_magic == 0xffffffff) {
		*olp = *nlp;
		return (0);
	}

	if (nlp->d_magic != DISKMAGIC || nlp->d_magic2 != DISKMAGIC ||
	    dkcksum(nlp) != 0)
		return (EINVAL);

	/* XXX missing check if other dos partitions will be overwritten */

	while (openmask != 0) {
		i = ffs((long)openmask) - 1;
		openmask &= ~(1 << i);
		if (nlp->d_npartitions <= i)
			return (EBUSY);
		opp = &olp->d_partitions[i];
		npp = &nlp->d_partitions[i];
		if (npp->p_offset != opp->p_offset ||
		    npp->p_size < opp->p_size)
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
	return (0);
}


/*
 * Write disk label back to device after modification.
 */
int
writedisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat)(struct buf *);
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	enum disklabel_tag *tp;
	char *msg = "no disk label";
	struct buf *bp;
	struct disklabel dl;
#if defined(DISKLABEL_I386) || defined(DISKLABEL_HPPA) || defined(DISKLABEL_ALL)
	struct cpu_disklabel cdl;
#endif
	int labeloffset, error, i, endian, partoff = 0, cyl = 0;
	u_int64_t csum, *p;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	/*
	 * I once played with the thought of using osdep->label{tag,sector}
	 * as a cache for knowing where (and what) to write.  However, now I
	 * think it might be useful to reprobe if someone has written
	 * a newer disklabel of another type with disklabel(8) and -r.
	 */
	for (tp = probe_order; msg && *tp != -1; tp++) {
		dl = *lp;
		switch (*tp) {
		case DLT_ALPHA:
#if defined(DISKLABEL_ALPHA) || defined(DISKLABEL_ALL)
			msg = readbsdlabel(bp, strat, 0, ALPHA_LABELSECTOR,
			    ALPHA_LABELOFFSET, LITTLE_ENDIAN, &dl, 0);
			labeloffset = ALPHA_LABELOFFSET;
			endian = LITTLE_ENDIAN;
#endif
			break;

		case DLT_I386:
#if defined(DISKLABEL_I386) || defined(DISKLABEL_ALL)
			msg = readdoslabel(bp, strat, &dl, &cdl, &partoff,
			    &cyl, 0);
			labeloffset = I386_LABELOFFSET;
			endian = LITTLE_ENDIAN;
#endif
			break;

		case DLT_AMIGA:
#if defined(DISKLABEL_AMIGA) || defined(DISKLABEL_ALL)
			msg = readamigalabel(bp, strat, &dl, &cdl, 0);
			labeloffset = AMIGA_LABELOFFSET;
			endian = BIG_ENDIAN;
#endif
			break;

		case DLT_HPPA:
#if defined(DISKLABEL_HPPA) || defined(DISKLABEL_ALL)
			msg = readliflabel(bp, strat, &dl, &cdl, &partoff,
			    &cyl, 0);
			labeloffset = HPPA_LABELOFFSET;
			endian = BIG_ENDIAN;
#endif
			break;

		default:
			panic("unrecognized disklabel tag %d", *tp);
		}
	}

	if (msg) {
		if (partoff == -1)
			return EIO;

		/* Write it in the regular place with native byte order. */
		labeloffset = LABELOFFSET;
		endian = BYTE_ORDER;
		bp->b_blkno = partoff + LABELSECTOR;
		bp->b_cylin = cyl;
		bp->b_bcount = lp->d_secsize;
	}

	if (endian != BYTE_ORDER) {
		swapdisklabel(lp);
		/* recalc checksum */
		lp->d_checksum = 0;
		lp->d_checksum = dkcksum(lp);
	}

	*(struct disklabel *)(bp->b_data + labeloffset) = *lp;

	/* Alpha bootblocks are checksummed.  */
	if (*tp == DLT_ALPHA) {
		for (csum = i = 0, p = (u_int64_t *)bp->b_data; i < 63; i++)
			csum += *p++;
		*p = csum;
	}

	bp->b_flags = B_BUSY | B_WRITE;
	(*strat)(bp);
	error = biowait(bp);

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
bounds_check_with_label(bp, lp, osdep, wlabel)
	struct buf *bp;
	struct disklabel *lp;
	struct cpu_disklabel *osdep;
	int wlabel;
{
#define blockpersec(count, lp) ((count) * (((lp)->d_secsize) / DEV_BSIZE))
	struct partition *p = lp->d_partitions + DISKPART(bp->b_dev);
	int labelsector = blockpersec(lp->d_partitions[RAW_PART].p_offset,
	    lp) + osdep->labelsector;
	int sz = howmany(bp->b_bcount, DEV_BSIZE);

	/* avoid division by zero */
	if (lp->d_secpercyl == 0) {
		bp->b_error = EINVAL;
		goto bad;
	}

	/* beyond partition? */
	if (bp->b_blkno + sz > blockpersec(p->p_size, lp)) {
		sz = blockpersec(p->p_size, lp) - bp->b_blkno;
		if (sz == 0) {
			/* If exactly at end of disk, return EOF. */
			bp->b_resid = bp->b_bcount;
			goto done;
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
	if (bp->b_blkno + blockpersec(p->p_offset, lp) <= labelsector &&
	    bp->b_blkno + blockpersec(p->p_offset, lp) + sz > labelsector &&
	    (bp->b_flags & B_READ) == 0 && !wlabel) {
		bp->b_error = EROFS;
		goto bad;
	}

	/* calculate cylinder for disksort to order transfers with */
	bp->b_cylin = (bp->b_blkno + blockpersec(p->p_offset, lp)) /
	    lp->d_secpercyl;
	return (1);

bad:
	bp->b_flags |= B_ERROR;
done:
	return (0);
}

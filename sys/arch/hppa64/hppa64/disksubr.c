/*	$OpenBSD: disksubr.c,v 1.49 2007/06/17 00:27:28 deraadt Exp $	*/

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
#include <sys/disklabel.h>
#include <sys/disk.h>

char   *readliflabel(struct buf *, void (*)(struct buf *),
    struct disklabel *, struct cpu_disklabel *, int *, int *, int);

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
    struct disklabel *lp, struct cpu_disklabel *osdep, int spoofonly)
{
	struct buf *bp = NULL;
	char *msg;

	if ((msg = initdisklabel(lp)))
		goto done;

	/* get a buffer and initialize it */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;

	msg = readliflabel(bp, strat, lp, osdep, NULL, NULL, spoofonly);
	if (msg == NULL)
		goto done;

	msg = readdoslabel(bp, strat, lp, osdep, NULL, NULL, spoofonly);
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

char *
readliflabel(struct buf *bp, void (*strat)(struct buf *),
    struct disklabel *lp, struct cpu_disklabel *osdep,
    int *partoffp, int *cylp, int spoofonly)
{
	struct buf *dbp = NULL;
	struct lifdir *p;
	char *msg = NULL;
	int fsoff = 0;

	/* read LIF volume header */
	bp->b_blkno = btodb(LIF_VOLSTART);
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylinder = btodb(LIF_VOLSTART) / lp->d_secpercyl;
	(*strat)(bp);
	if (biowait(bp))
		return "LIF volume header I/O error";

	bcopy(bp->b_data, &osdep->u._hppa.lifvol, sizeof(struct lifvol));
	if (osdep->u._hppa.lifvol.vol_id != LIF_VOL_ID) {
		fsoff = 0;
		goto finished;
	}

	dbp = geteblk(LIF_DIRSIZE);
	dbp->b_dev = bp->b_dev;

	/* read LIF directory */
	dbp->b_blkno = lifstodb(osdep->u._hppa.lifvol.vol_addr);
	dbp->b_bcount = lp->d_secsize;
	dbp->b_flags = B_BUSY | B_READ;
	dbp->b_cylinder = dbp->b_blkno / lp->d_secpercyl;
	(*strat)(dbp);

	if (biowait(dbp)) {
		msg = "LIF directory I/O error";
		goto done;
	}

	bcopy(dbp->b_data, osdep->u._hppa.lifdir, LIF_DIRSIZE);

	/* scan for LIF_DIR_FS dir entry */
	for (fsoff = -1,  p = &osdep->u._hppa.lifdir[0];
	    fsoff < 0 && p < &osdep->u._hppa.lifdir[LIF_NUMDIR]; p++) {
		if (p->dir_type == LIF_DIR_FS ||
		    p->dir_type == LIF_DIR_HPLBL)
			break;
	}

	if (p->dir_type == LIF_DIR_FS) {
		fsoff = lifstodb(p->dir_addr);
		goto finished;
	}

	/* Only came here to find the offset... */
	if (partoffp && spoofonly)
		goto finished;

	if (p->dir_type == LIF_DIR_HPLBL) {
		struct hpux_label *hl;
		struct partition *pp;
		u_int8_t fstype;
		int i;

		/* read LIF directory */
		dbp->b_blkno = lifstodb(p->dir_addr);
		dbp->b_bcount = lp->d_secsize;
		dbp->b_flags = B_BUSY | B_READ;
		dbp->b_cylinder = dbp->b_blkno / lp->d_secpercyl;
		(*strat)(dbp);

		if (biowait(dbp)) {
			msg = "HPUX label I/O error";
			goto done;
		}

		bcopy(dbp->b_data, &osdep->u._hppa.hplabel,
		    sizeof(osdep->u._hppa.hplabel));

		hl = &osdep->u._hppa.hplabel;
		if (hl->hl_magic1 != hl->hl_magic2 ||
		    hl->hl_magic != HPUX_MAGIC || hl->hl_version != 1) {
			msg = "HPUX label magic mismatch";
			goto done;
		}

		lp->d_bbsize = 8192;
		lp->d_sbsize = 8192;
		for (i = 0; i < MAXPARTITIONS; i++) {
			DL_SETPSIZE(&lp->d_partitions[i], 0);
			DL_SETPOFFSET(&lp->d_partitions[i], 0);
			lp->d_partitions[i].p_fstype = 0;
		}

		for (i = 0; i < HPUX_MAXPART; i++) {
			if (!hl->hl_flags[i])
				continue;
			if (hl->hl_flags[i] == HPUX_PART_ROOT) {
				pp = &lp->d_partitions[0];
				fstype = FS_BSDFFS;
			} else if (hl->hl_flags[i] == HPUX_PART_SWAP) {
				pp = &lp->d_partitions[1];
				fstype = FS_SWAP;
			} else if (hl->hl_flags[i] == HPUX_PART_BOOT) {
				pp = &lp->d_partitions[RAW_PART + 1];
				fstype = FS_BSDFFS;
			} else
				continue;

			DL_SETPSIZE(pp, hl->hl_parts[i].hlp_length * 2);
			DL_SETPOFFSET(pp, hl->hl_parts[i].hlp_start * 2);
			pp->p_fstype = fstype;
		}

		DL_SETPSIZE(&lp->d_partitions[RAW_PART], DL_GETDSIZE(lp));
		DL_SETPOFFSET(&lp->d_partitions[RAW_PART], 0);
		lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
		lp->d_npartitions = MAXPARTITIONS;
		lp->d_magic = DISKMAGIC;
		lp->d_magic2 = DISKMAGIC;
		lp->d_version = 1;
		lp->d_checksum = 0;
		lp->d_checksum = dkcksum(lp);
		/* drop through */
	}

finished:
	/* if no suitable lifdir entry found assume zero */
	if (fsoff < 0)
		fsoff = 0;
	if (partoffp)
		*partoffp = fsoff;

	if (spoofonly)
		goto done;

	bp->b_blkno = fsoff + LABELSECTOR;
	bp->b_cylinder = 0;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);

	/* if successful, locate disk label within block and validate */
	if (biowait(bp)) {
		msg = "disk label I/O error";
		goto done;
	}

	return checkdisklabel(bp->b_data + LABELOFFSET, lp);

done:
	if (dbp) {
		dbp->b_flags |= B_INVAL;
		brelse(dbp);
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

	if (readliflabel(bp, strat, lp, osdep, &partoff, &cyl, 1) == NULL)
		goto writeit;

	if (readdoslabel(bp, strat, lp, osdep, &partoff, &cyl, 1) == NULL)
		goto writeit;

	error = EIO;
	goto done;

writeit:
	/* Read it in, slap the new label in, and write it back out */
	bp->b_blkno = partoff + LABELSECTOR;
	bp->b_cylinder = cyl;
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

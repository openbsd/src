/*	$OpenBSD: disksubr.c,v 1.11 1998/05/07 16:03:21 millert Exp $	*/
/*	$NetBSD: disksubr.c,v 1.14 1997/01/15 00:55:43 jonathan Exp $	*/

/*
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>

#define	b_cylin	b_resid

#ifdef COMPAT_ULTRIX
#include <pmax/stand/dec_boot.h>

extern char *
compat_label __P((dev_t dev, void (*strat) __P((struct buf *bp)),
		  struct disklabel *lp, struct cpu_disklabel *osdep));

#endif

char*	readdisklabel __P((dev_t dev, void (*strat) __P((struct buf *bp)),
		       register struct disklabel *lp,
		       struct cpu_disklabel *osdep));

/*
 * Attempt to read a disk label from a device
 * using the indicated stategy routine.
 * The label must be partly set up before this:
 * secpercyl and anything required in the strategy routine
 * (e.g., sector size) must be filled in before calling us.
 * Returns null on success and an error string on failure.
 */
char *
readdisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat) __P((struct buf *bp));
	register struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	register struct buf *bp;
	struct disklabel *dlp;
	char *msg = NULL;

	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;
	lp->d_npartitions = 1;
	if (lp->d_partitions[0].p_size == 0)
		lp->d_partitions[0].p_size = 0x1fffffff;
	lp->d_partitions[0].p_offset = 0;

	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;
	bp->b_blkno = LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylin = LABELSECTOR / lp->d_secpercyl;
	(*strat)(bp);
	if (biowait(bp)) {
		msg = "I/O error";
	} else {
		dlp = (struct disklabel *)bp->b_un.b_addr + LABELOFFSET;
		if (dlp->d_magic == DISKMAGIC && dlp->d_magic2 == DISKMAGIC) {
			if (dlp->d_npartitions > MAXPARTITIONS || dkcksum(dlp))
				msg = "disk label corrupted";
			else
				*lp = *dlp;
		} else for (dlp = (struct disklabel *)bp->b_un.b_addr;
		    dlp <= (struct disklabel *)(bp->b_un.b_addr+DEV_BSIZE-sizeof(*dlp));
		    dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
			if (dlp->d_magic != DISKMAGIC || dlp->d_magic2 != DISKMAGIC) {
				if (msg == NULL)
					msg = "no disk label";
			} else if (dlp->d_npartitions > MAXPARTITIONS ||
				   dkcksum(dlp) != 0)
				msg = "disk label corrupted";
			else {
				*lp = *dlp;
				msg = NULL;
				break;
			}
		}
	}

	bp->b_flags = B_INVAL | B_AGE;
	brelse(bp);
	return (msg);
}

#ifdef COMPAT_ULTRIX
/*
 * Given a buffer bp, try and interpret it as an Ultrix disk label,
 * putting the partition info into a native OpenBSD label
 */
char *
compat_label(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat) __P((struct buf *bp));
	register struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	Dec_DiskLabel *dlp;
	struct buf *bp = NULL;
	char *msg = NULL;

	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;
	bp->b_blkno = DEC_LABEL_SECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylin = DEC_LABEL_SECTOR / lp->d_secpercyl;
	(*strat)(bp);

	if (biowait(bp)) {
                msg = "I/O error";
		goto done;
	}

	for (dlp = (Dec_DiskLabel *)bp->b_un.b_addr;
	     dlp <= (Dec_DiskLabel *)(bp->b_un.b_addr+DEV_BSIZE-sizeof(*dlp));
	     dlp = (Dec_DiskLabel *)((char *)dlp + sizeof(long))) {

		int part;

		if (dlp->magic != DEC_LABEL_MAGIC) {
			printf("label: %x\n",dlp->magic);
			msg = ((msg != NULL) ? msg: "no disk label");
			goto done;
		}

#ifdef DIAGNOSTIC
/*XXX*/		printf("Interpreting Ultrix label\n");
#endif

		lp->d_magic = DEC_LABEL_MAGIC;
		lp->d_npartitions = 0;
		for (part = 0;
		     part <((MAXPARTITIONS<DEC_NUM_DISK_PARTS) ?
			    MAXPARTITIONS : DEC_NUM_DISK_PARTS);
		     part++) {
			lp->d_partitions[part].p_size = dlp->map[part].numBlocks;
			lp->d_partitions[part].p_offset = dlp->map[part].startBlock;
			lp->d_partitions[part].p_fsize = 1024;
			lp->d_partitions[part].p_fstype =
			  (part==1) ? FS_SWAP : FS_BSDFFS;
			lp->d_npartitions += 1;

#ifdef DIAGNOSTIC
			printf(" Ultrix label rz%d%c: start %d len %d\n",
			       DISKUNIT(dev), "abcdefgh"[part],
			       lp->d_partitions[part].p_offset,
			       lp->d_partitions[part].p_size);
#endif			
		}
		break;
	}

done:
	bp->b_flags = B_INVAL | B_AGE;
	brelse(bp);
	return (msg);
}
#endif /* COMPAT_ULTRIX */


/*
 * Check new disk label for sensibility
 * before setting it.
 */
int
setdisklabel(olp, nlp, openmask, osdep)
	register struct disklabel *olp, *nlp;
	u_long openmask;
	struct cpu_disklabel *osdep;
{
	register i;
	register struct partition *opp, *npp;

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

/* encoding of disk minor numbers, should be elsewhere... */
#define dkunit(dev)		(minor(dev) >> 3)
#define dkpart(dev)		(minor(dev) & 07)
#define dkminor(unit, part)	(((unit) << 3) | (part))

/*
 * Write disk label back to device after modification.
 */
int
writedisklabel(dev, strat, lp, osdep)
	dev_t dev;
	void (*strat) __P((struct buf *bp));
	register struct disklabel *lp;
	struct cpu_disklabel *osdep;
{
	struct buf *bp;
	struct disklabel *dlp;
	int labelpart;
	int error = 0;

	labelpart = dkpart(dev);
	if (lp->d_partitions[labelpart].p_offset != 0) {
		if (lp->d_partitions[0].p_offset != 0)
			return (EXDEV);			/* not quite right */
		labelpart = 0;
	}
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = makedev(major(dev), dkminor(dkunit(dev), labelpart));
	bp->b_blkno = LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_READ;
	(*strat)(bp);
	if ((error = biowait(bp)) != 0)
		goto done;
	dlp = (struct disklabel *)bp->b_un.b_addr + LABELOFFSET;
	if (dlp->d_magic == DISKMAGIC && dlp->d_magic2 == DISKMAGIC &&
	    dkcksum(dlp) == 0) {
		*dlp = *lp;
		bp->b_flags = B_WRITE;
		(*strat)(bp);
		error = biowait(bp);
		goto done;
	} else for (dlp = (struct disklabel *)bp->b_un.b_addr;
	    dlp <= (struct disklabel *)
	      (bp->b_un.b_addr + lp->d_secsize - sizeof(*dlp));
	    dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic == DISKMAGIC && dlp->d_magic2 == DISKMAGIC &&
		    dkcksum(dlp) == 0) {
			*dlp = *lp;
			bp->b_flags = B_WRITE;
			(*strat)(bp);
			error = biowait(bp);
			goto done;
		}
	}
	/* Write it in the regular place. */
	*(struct disklabel *)(bp->b_data + LABELOFFSET) = *lp;
	bp->b_flags = B_BUSY | B_WRITE;
	(*strat)(bp);
	error = biowait(bp);
	goto done;

done:
	brelse(bp);
	return (error);
}


/*
 * was this the boot device ?
 */
void
dk_establish(dk, dev)
	struct disk *dk;
	struct device *dev;
{
	/* see also arch/alpha/alpha/disksubr.c */
	printf("Warning: boot path unknown\n");
	return;
}

/* 
 * UNTESTED !!
 *
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
	int labelsect = blockpersec(lp->d_partitions[0].p_offset, lp) +
	    LABELSECTOR;
	int sz = howmany(bp->b_bcount, DEV_BSIZE);

	/* overwriting disk label ? */
	/* XXX should also protect bootstrap in first 8K */ 
	if (bp->b_blkno + blockpersec(p->p_offset, lp) <= labelsect &&
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
	bp->b_resid = (bp->b_blkno + blockpersec(p->p_offset, lp)) /
	    lp->d_secpercyl;
	return(1);
bad:
	bp->b_flags |= B_ERROR;
	return(-1);
}

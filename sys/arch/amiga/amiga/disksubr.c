/*	$NetBSD: disksubr.c,v 1.21 1996/01/07 22:01:44 thorpej Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/dkstat.h>
#include <amiga/amiga/adosglue.h>

/*
 * bitmap id's
 */
#define RDBLOCK_BID	1
#define PARTBLOCK_BID	2
#define BADBLOCK_BID	3
#define FSBLOCK_BID	4
#define LSEGBLOCK_BID	5

struct rdbmap {
	long firstblk;
	long lastblk;
	int  bigtype;
	struct {
		short next;
		short prev;
		char  shortid;
	} big[0];
	struct {
		char next;
		char prev;
		char shortid;
	} tab[0];
};

#define b_cylin b_resid
#define baddr(bp) (void *)((bp)->b_un.b_addr)

u_long rdbchksum __P((void *));
struct adostype getadostype __P((u_long));
struct rdbmap *getrdbmap __P((dev_t, void (*)(), struct disklabel *,
    struct cpu_disklabel *));

/* XXX unknown function but needed for /sys/scsi to link */
int
dk_establish(dk, dev)
	struct disk *dk;
	struct device *dev;
{
	return(-1);
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
readdisklabel(dev, strat, lp, clp)
	dev_t dev;
	void (*strat)();
	struct disklabel *lp;
	struct cpu_disklabel *clp;
{
	struct adostype adt;
	struct partition *pp;
	struct partblock *pbp;
	struct rdblock *rbp;
	struct buf *bp;
	char *msg, *bcpls, *s, bcpli, cindex;
	int i, trypart, nopname;
	u_char fstype;
	u_long nextb;

	clp->rdblock = RDBNULL;
	/*
	 * give some guarnteed validity to
	 * the disklabel
	 */
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;
	if (lp->d_secpercyl == 0)
		lp->d_secpercyl = 0x1fffffff;
	lp->d_npartitions = RAW_PART + 1;

	for (i = 0; i < MAXPARTITIONS; i++) {
		clp->pbindex[i] = -1;
		clp->pblist[i] = RDBNULL;
		if (i == RAW_PART)
			continue;
		lp->d_partitions[i].p_size = 0;
		lp->d_partitions[i].p_offset = 0;
	}
	if (lp->d_partitions[RAW_PART].p_size == 0)
		lp->d_partitions[RAW_PART].p_size = 0x1fffffff;
	lp->d_partitions[RAW_PART].p_offset = 0;

	/* obtain buffer to probe drive with */
	bp = (void *)geteblk((int)lp->d_secsize);

	/*
	 * request no partition relocation by driver on I/O operations
	 */
#ifdef _KERNEL
	bp->b_dev = MAKEDISKDEV(major(dev), DISKUNIT(dev), RAW_PART);
#else
	bp->b_dev = dev;
#endif
	msg = NULL;

	/*
	 * find the RDB block
	 * XXX Need to check for a standard label if this fails (fd0 etc..)
	 */
	for (nextb = 0; nextb < RDB_MAXBLOCKS; nextb++) {
		bp->b_blkno = nextb;
		bp->b_cylin = bp->b_blkno / lp->d_secpercyl;
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		strat(bp);

		if (biowait(bp)) {
			msg = "rdb scan I/O error";
			goto done;
		}
		rbp = baddr(bp);
		if (rbp->id == RDBLOCK_ID) {
			if (rdbchksum(rbp) == 0)
				break;
			else
				msg = "rdb bad checksum";
		}
	}
	if (nextb == RDB_MAXBLOCKS) {
		if (msg == NULL)
			msg = "no rdb found";
		goto done;
	} else if (msg) {
		/*
		 * maybe we found an invalid one before a valid.
		 * clear err.
		 */
		msg = NULL;
	}
	clp->rdblock = nextb;

	lp->d_secsize = rbp->nbytes;
	lp->d_nsectors = rbp->nsectors;
	lp->d_ntracks = rbp->nheads;
	/*
	 * should be rdb->ncylinders however this is a bogus value 
	 * sometimes it seems
	 */
	if (rbp->highcyl == 0)
		lp->d_ncylinders = rbp->ncylinders;
	else
		lp->d_ncylinders = rbp->highcyl + 1;
	/*
	 * I also don't trust rdb->secpercyl
	 */
	lp->d_secpercyl = min(rbp->secpercyl, lp->d_nsectors * lp->d_ntracks);
	if (lp->d_secpercyl == 0)
		lp->d_secpercyl = lp->d_nsectors * lp->d_ntracks;
#ifdef DIAGNOSTIC
	if (lp->d_ncylinders != rbp->ncylinders)
		printf("warning found rdb->ncylinders(%d) != "
		    "rdb->highcyl(%d) + 1\n", rbp->ncylinders,
		    rbp->highcyl);
	if (lp->d_nsectors * lp->d_ntracks != rbp->secpercyl)
		printf("warning found rdb->secpercyl(%d) != "
		    "rdb->nsectors(%d) * rdb->nheads(%d)\n", rbp->secpercyl,
		    rbp->nsectors, rbp->nheads);
#endif
	lp->d_sparespercyl =
	    max(rbp->secpercyl, lp->d_nsectors * lp->d_ntracks) 
	    - lp->d_secpercyl;
	if (lp->d_sparespercyl == 0)
		lp->d_sparespertrack = 0;
	else {
		lp->d_sparespertrack = lp->d_sparespercyl / lp->d_ntracks;
#ifdef DIAGNOSTIC
		if (lp->d_sparespercyl % lp->d_ntracks)
			printf("warning lp->d_sparespercyl(%d) not multiple "
			    "of lp->d_ntracks(%d)\n", lp->d_sparespercyl,
			    lp->d_ntracks);
#endif
	}

	lp->d_secperunit = lp->d_secpercyl * lp->d_ncylinders;
	lp->d_acylinders = rbp->ncylinders - (rbp->highcyl - rbp->lowcyl + 1);
	lp->d_rpm = 3600; 		/* good guess I suppose. */
	lp->d_interleave = rbp->interleave;
	lp->d_headswitch = lp->d_flags = lp->d_trackskew = lp->d_cylskew = 0;
	lp->d_trkseek = /* rbp->steprate */ 0;	

	/*
	 * raw partition gets the entire disk
	 */
	lp->d_partitions[RAW_PART].p_size = rbp->ncylinders * lp->d_secpercyl;

	/*
	 * scan for partition blocks
	 */
	nopname = 1;
	cindex = 0;
	for (nextb = rbp->partbhead; nextb != RDBNULL; nextb = pbp->next) {
		bp->b_blkno = nextb;
		bp->b_cylin = bp->b_blkno / lp->d_secpercyl;
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		strat(bp);
		
		if (biowait(bp)) {
			msg = "partition scan I/O error";
			goto done;
		}
		pbp = baddr(bp);

		if (pbp->id != PARTBLOCK_ID) {
			msg = "partition block with bad id";
			goto done;
		}
		if (rdbchksum(pbp)) {
			msg = "partition block bad checksum";
			goto done;
		}

		if (pbp->e.tabsize < 11) {
			/*
			 * not enough info, too funky for us.
			 * I don't want to skip I want it fixed.
			 */
			msg = "bad partition info (environ < 11)";
			goto done;
			continue;
		}

		/*
		 * XXXX should be ">" however some vendors don't know
		 * what a table size is so, we hack for them.
		 * the other checks can fail for all I care but this
		 * is a very common value. *sigh*.
		 */
		if (pbp->e.tabsize >= 16)
			adt = getadostype(pbp->e.dostype);
		else {
			adt.archtype = ADT_UNKNOWN;
			adt.fstype = FS_UNUSED;
		}

		switch (adt.archtype) {
		case ADT_NETBSDROOT:
			pp = &lp->d_partitions[0];
			if (pp->p_size) {
				printf("WARN: more than one root, ignoring\n");
				clp->rdblock = RDBNULL;	/* invlidate cpulab */
				continue;
			}
			break;
		case ADT_NETBSDSWAP:
			pp = &lp->d_partitions[1];
			if (pp->p_size) {
				printf("WARN: more than one swap, ignoring\n");
				clp->rdblock = RDBNULL;	/* invlidate cpulab */
				continue;
			}
			break;
		case ADT_NETBSDUSER:
		case ADT_AMIGADOS:
		case ADT_AMIX:
		case ADT_UNKNOWN:
			pp = &lp->d_partitions[lp->d_npartitions];
			break;
		}
		/*
		 * insert sort in increasing offset order
		 */
		while ((pp - lp->d_partitions) > RAW_PART + 1) {
			daddr_t boff;
			
			boff = pbp->e.lowcyl * lp->d_secpercyl;
			if (boff > (pp - 1)->p_offset)
				break;
			*pp = *(pp - 1);	/* struct copy */
			pp--;
		}
		i = (pp - lp->d_partitions);
		if (nopname || i == 1) {
			/*
			 * either we have no packname yet or we found 
			 * the swap partition. copy BCPL string into packname
			 * [the reason we use the swap partition: the user
			 *  can supply a decent packname without worry
			 *  of having to access an odly named partition 
			 *  under AmigaDos]
			 */
			s = lp->d_packname;
			bcpls = &pbp->partname[1];
			bcpli = pbp->partname[0];
			if (sizeof(lp->d_packname) <= bcpli)
				bcpli = sizeof(lp->d_packname) - 1;
			while (bcpli--)
				*s++ = *bcpls++;
			*s = 0;
			nopname = 0;
		}

		if (lp->d_npartitions <= i)
			lp->d_npartitions = i + 1;

		pp->p_size = (pbp->e.highcyl - pbp->e.lowcyl + 1)
		    * lp->d_secpercyl;
		pp->p_offset = pbp->e.lowcyl * lp->d_secpercyl;
		pp->p_fstype = adt.fstype;
		if (pbp->e.tabsize > 22 && ISFSARCH_NETBSD(adt)) {
			pp->p_fsize = pbp->e.fsize;
			pp->p_frag = pbp->e.frag;	
			pp->p_cpg = pbp->e.cpg;
		} else {
			pp->p_fsize = 1024;
			pp->p_frag = 8;	
			pp->p_cpg = 0;
		}
		if (adt.archtype == ADT_AMIGADOS) {
			/*
			 * Save reserved blocks at begin in cpg and
			 *  adjust size by reserved blocks at end
			 */
			pp->p_cpg = pbp->e.resvblocks;
			pp->p_size -= pbp->e.prefac;
		}

		/*
		 * store this partitions block number
		 */
		clp->pblist[clp->pbindex[i] = cindex++];
	}
	/*
	 * calulate new checksum.
	 */
	lp->d_magic = lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);
	if (clp->rdblock != RDBNULL)
		clp->valid = 1;
done:
	if (clp->valid == 0)
		clp->rdblock = RDBNULL;
	bp->b_flags = B_INVAL | B_AGE | B_READ;
	brelse(bp);
	return(msg);
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

/*
 * Write disk label back to device after modification.
 * this means write out the Rigid disk blocks to represent the 
 * label.  Hope the user was carefull.
 */
writedisklabel(dev, strat, lp, clp)
	dev_t dev;
	void (*strat)();
	register struct disklabel *lp;
	struct cpu_disklabel *clp;
{
	struct rdbmap *bmap;
	return(EINVAL);
	/*
	 * get write out partition list iff cpu_label is valid.
	 */
	if (clp->valid == 0 ||
	    (clp->rdblock <= 0 || clp->rdblock >= RDB_MAXBLOCKS))
		return(EINVAL);

	bmap = getrdbmap(dev, strat, lp, clp);
	return(EINVAL);
}

int
bounds_check_with_label(bp, lp, wlabel)
	struct buf *bp;
	struct disklabel *lp;
	int wlabel;
{
	struct partition *pp;
	long maxsz, sz;

	pp = &lp->d_partitions[DISKPART(bp->b_dev)];
	if (bp->b_flags & B_RAW) {
		maxsz = pp->p_size * (lp->d_secsize / DEV_BSIZE);
		sz = (bp->b_bcount + DEV_BSIZE - 1) >> DEV_BSHIFT;
	} else {
		maxsz = pp->p_size;
		sz = (bp->b_bcount + lp->d_secsize - 1) / lp->d_secsize;
	}

	if (bp->b_blkno < 0 || bp->b_blkno + sz > maxsz) {
		if (bp->b_blkno == maxsz) {
			/* 
			 * trying to get one block beyond return EOF.
			 */
			bp->b_resid = bp->b_bcount;
			return(0);
		}
		sz = maxsz - bp->b_blkno;
		if (sz <= 0 || bp->b_blkno < 0) {
			bp->b_error = EINVAL;
			bp->b_flags |= B_ERROR;
			return(-1);
		}
		/* 
		 * adjust count down
		 */
		if (bp->b_flags & B_RAW)
			bp->b_bcount = sz << DEV_BSHIFT;
		else
			bp->b_bcount = sz * lp->d_secsize;
	}

	/*
	 * calc cylinder for disksort to order transfers with
	 */
	bp->b_cylin = (bp->b_blkno + pp->p_offset) / lp->d_secpercyl;
	return(1);
}

u_long
rdbchksum(bdata)
	void *bdata;
{
	u_long *blp, cnt, val;

	blp = bdata;
	cnt = blp[1];
	val = 0;

	while (cnt--)
		val += *blp++;
	return(val);
}

struct adostype 
getadostype(dostype)
	u_long dostype;
{
	struct adostype adt;
	u_long t3, b1;

	t3 = dostype & 0xffffff00;
	b1 = dostype & 0x000000ff;

	adt.fstype = b1;

	switch (t3) {
	case DOST_NBR:
		adt.archtype = ADT_NETBSDROOT;
		return(adt);
	case DOST_NBS:
		adt.archtype = ADT_NETBSDSWAP;
		return(adt);
	case DOST_NBU:
		adt.archtype = ADT_NETBSDUSER;
		return(adt);
	case DOST_MUFS:
		/* check for 'muFS'? */
	case DOST_DOS:
		adt.archtype = ADT_AMIGADOS;
		adt.fstype = FS_ADOS;
		return(adt);
	case DOST_AMIX:
		adt.archtype = ADT_AMIX;
		if (b1 == 2)
			adt.fstype = FS_BSDFFS;
		else
			adt.fstype = FS_UNUSED;
		return(adt);
	case DOST_XXXBSD:
#ifdef DIAGNOSTIC
		printf("found dostype: 0x%x which is deprecated", dostype);
#endif
		if (b1 == 'S') {
			dostype = DOST_NBS;
			dostype |= FS_SWAP;
		} else {
			if (b1 == 'R')
				dostype = DOST_NBR;
			else
				dostype = DOST_NBU;
			dostype |= FS_BSDFFS;
		}
#ifdef DIAGNOSTIC
		printf(" using: 0x%x instead\n", dostype);
#endif
		return(getadostype(dostype));
	default:
#ifdef DIAGNOSTIC
		printf("warning unknown dostype: 0x%x marking unused\n",
		    dostype);
#endif
		adt.archtype = ADT_UNKNOWN;
		adt.fstype = FS_UNUSED;
		return(adt);
	}	
}

/*
 * if we find a bad block we kill it (and the chain it belongs to for
 * lseg or end the chain for part, badb, fshd)
 */
struct rdbmap *
getrdbmap(dev, strat, lp, clp)
	dev_t dev;
	void (*strat)();
	struct disklabel *lp;
	struct cpu_disklabel *clp;
{
	struct buf *bp;

	bp = (void *)geteblk(lp->d_secsize);
	/*
	 * get the raw partition
	 */

	bp->b_dev = MAKEDISKDEV(major(dev), DISKUNIT(dev), RAW_PART);
	/* XXX finish */
bad:
	brelse(bp);
	return(NULL);
}


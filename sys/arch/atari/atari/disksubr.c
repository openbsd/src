/*	$NetBSD: disksubr.c,v 1.3.2.1 1995/10/21 21:34:19 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman.
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
 *      This product includes software developed by Leo Weppelman.
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
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/disklabel.h>
#include <machine/tospart.h>

/*
 * This is ugly, but as long as disklabel(8) uses
 * BBSIZE from ufs/ffs/fs.h, there's no alternative.
 */
#include <ufs/ffs/fs.h>
#if BBSIZE < 8192
#error BBSIZE in /sys/ufs/ffs/fs.h must be at least 8192 bytes
#endif

#if 0
#define MACHDSBR_DEBUG(x)	printf x
#else
#define MACHDSBR_DEBUG(x)
#endif

static void check_label __P((struct disklabel *, struct cpu_disklabel *));
static void fake_label __P((struct disklabel *, struct cpu_disklabel *));
static int  real_label __P((dev_t, void (*)(), u_int32_t, struct disklabel *));
static int  rd_rootparts __P((dev_t, void (*)(), u_int32_t,
						struct cpu_disklabel *));
static int  rd_extparts __P((dev_t, void (*)(), u_int32_t, u_int32_t,
				u_int32_t, struct cpu_disklabel *));
static int  add_tospart __P((struct cpu_disklabel *, struct tos_part *));

/*
 * XXX unknown function but needed for /sys/scsi to link
 */
int
dk_establish()
{
	return(-1);
}

/*
 * Determine the size of the transfer, and make sure it is
 * within the boundaries of the partition. Adjust transfer
 * if needed, and signal errors or early completion.
 */
int
bounds_check_with_label(bp, lp, wlabel)
struct buf		*bp;
struct disklabel	*lp;
int			wlabel;
{
	struct partition	*pp;
	u_int32_t		maxsz, sz;

	pp = &lp->d_partitions[DISKPART(bp->b_dev)];
	if (bp->b_flags & B_RAW) {
		if (bp->b_bcount & (DEV_BSIZE - 1)) {
			bp->b_error = EINVAL;
			bp->b_flags |= B_ERROR;
			return (-1);
		}
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
		else bp->b_bcount = sz * lp->d_secsize;
	}

	/*
	 * calc cylinder for disksort to order transfers with
	 */
	bp->b_cylinder = (bp->b_blkno + pp->p_offset) / lp->d_secpercyl;
	return(1);
}

/*
 * Attempt to read a disk label from a device using the
 * indicated strategy routine. The label must be partly
 * set up before this:
 * secpercyl and anything required in the strategy routine
 * (e.g. sector size) must be filled in before calling us.
 * Returns NULL on success and an error string on failure.
 */
char *
readdisklabel(dev, strat, lp, clp)
dev_t			dev;
void			(*strat)();
struct disklabel	*lp;
struct cpu_disklabel	*clp;
{
	int		i;

	/*
	 * Give some guaranteed validity to the disk label.
	 */
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;
	if (lp->d_secsize == 0)
		lp->d_secsize = DEV_BSIZE;
	else if ((lp->d_secsize % DEV_BSIZE) != 0)
		return("Illegal secsize");
	if (lp->d_secpercyl == 0)
		return("Zero secpercyl");
	for (i = 0; i < MAXPARTITIONS; i++) {
		lp->d_partitions[i].p_size   = 0;
		lp->d_partitions[i].p_offset = 0;
		lp->d_partitions[i].p_fstype = FS_UNUSED;
	}
	lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;
	lp->d_npartitions                 = RAW_PART + 1;
	lp->d_bbsize                      = BBSIZE;
	lp->d_sbsize                      = SBSIZE;

	MACHDSBR_DEBUG(("unit: %lu secsize: %lu secperunit: %lu\n",
	 					(u_long)DISKUNIT(dev),
						(u_long)lp->d_secsize,
						(u_long)lp->d_secperunit));

	/*
	 * The `native' TOS (actually AHDI) partition layout requires
	 * a 512 byte sector size, therefore we can safely assume that
	 * a device with a different sector size is not `infected' by
	 * HDX cs.
	 */
	bzero(clp, sizeof *clp);
	if (lp->d_secsize == TOS_BSIZE
	  && rd_rootparts(dev, strat, lp->d_secpercyl, clp)) {
		/*
		 * This volume carries an AHDI compatible partition layout.
		 * Search for a partition with id NBD or RAW, which contains
		 * a NetBSD boot block with a valid disk label in it.
		 */
		MACHDSBR_DEBUG(("AHDI partition table: "));
		for (i = 0; i < clp->cd_npartitions; i++) {
			struct cpu_partition	*cp = &clp->cd_partitions[i];
			u_int32_t		id = cp->cp_id;
			if (id != CPU_PID_NBD && id != CPU_PID_RAW)
				continue;
			if (!real_label(dev, strat, cp->cp_st, lp)) {
				/*
				 * No disk label, but if this is the first NBD
				 * partition on this volume, we'll mark it
				 * anyway as a possible destination for future
				 * writedisklabel() calls, just in case there
				 * is no valid disk label on any of the
				 * other AHDI partitions.
				 */
				if (id == CPU_PID_NBD
					&& clp->cd_labelpart == NULL)
					clp->cd_labelpart = cp;
				continue;
			}
			/*
			 * Found a valid disk label, mark this TOS partition for
			 * writedisklabel(), and check for possibly dangerous
			 * overlap between TOS and NetBSD partition layout.
			 */
			MACHDSBR_DEBUG(("found real disklabel\n"));
			clp->cd_labelpart = cp;
			check_label(lp, clp);
			break;
		}
		if (i == clp->cd_npartitions) {
			/*
			 * No disk label on this volume, use the TOS partition
			 * layout to create a fake disk label. If there is no
			 * NBD partition on this volume either, subsequent
			 * writedisklabel() calls will fail.
			 */
			MACHDSBR_DEBUG(("creating fake disklabel\n"));
			fake_label(lp, clp);
		}
	} else {
		/*
		 * This volume carries no AHDI compatible partition layout,
		 * therefor the boot block must be located at the start of the
		 * volume. If the boot block does not contain a valid disk
		 * label, chances are this volume has never been used before,
		 * so the best we can do is to pretend that there's only one
		 * partition.
		 */
		MACHDSBR_DEBUG(("No AHDI partition table: "));
		if (!real_label(dev, strat, LABELSECTOR, lp)) {
			MACHDSBR_DEBUG(("unpartitioned unit\n"));
			lp->d_partitions[RAW_PART+1].p_size
					= lp->d_partitions[RAW_PART].p_size;
			lp->d_partitions[RAW_PART+1].p_offset
					= lp->d_partitions[RAW_PART].p_offset;
			lp->d_partitions[RAW_PART+1].p_fstype = FS_BSDFFS;
			lp->d_npartitions = RAW_PART + 2;
		}
		else MACHDSBR_DEBUG(("disklabel OK\n"));
	}

	/*
	 * Calulate new checksum.
	 */
	lp->d_magic = lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = 0;
	lp->d_checksum = dkcksum(lp);

	return(NULL);
}

/*
 * Check new disk label for sensibility before setting it.
 */
int
setdisklabel(olp, nlp, openmask, clp)
struct disklabel	*olp, *nlp;
u_long			openmask;
struct cpu_disklabel	*clp;
{
	/* special case to allow disklabel to be invalidated */
	if (nlp->d_magic == 0xffffffff) {
		*olp = *nlp;
		return(0);
	}

	/* sanity checks */
	if (nlp->d_secpercyl == 0 || nlp->d_secsize == 0
	  || (nlp->d_secsize % DEV_BSIZE) != 0 || dkcksum(nlp) != 0
	  || nlp->d_magic != DISKMAGIC || nlp->d_magic2 != DISKMAGIC)
		return(EINVAL);

	/* 
	 * If a TOS partition table exists, do an advisory check
	 * for consistency between NetBSD and TOS partition table.
	 */
	if (clp->cd_partitions != NULL)
		check_label(nlp, clp);

	while (openmask) {
		struct partition *op, *np;
		int i = ffs(openmask) - 1;

		openmask &= ~(1 << i);
		if (i >= nlp->d_npartitions)
			return(EBUSY);
		op = &olp->d_partitions[i];
		np = &nlp->d_partitions[i];
		if (np->p_offset != op->p_offset || np->p_size < op->p_size)
			return(EBUSY);
		/*
		 * Copy internally-set partition information
		 * if new label doesn't include it.		XXX
		 */
		if (np->p_fstype == FS_UNUSED && op->p_fstype != FS_UNUSED) {
			np->p_fstype = op->p_fstype;
			np->p_fsize  = op->p_fsize;
			np->p_frag   = op->p_frag;
			np->p_cpg    = op->p_cpg;
		}
	}
 	nlp->d_checksum = 0;
 	nlp->d_checksum = dkcksum(nlp);
	*olp = *nlp;
	return(0);
}

/*
 * Write disk label back to device after modification.
 */
int
writedisklabel(dev, strat, lp, clp)
dev_t			dev;
void			(*strat)();
struct disklabel	*lp;
struct cpu_disklabel	*clp;
{
	struct buf	*bp;
	u_int32_t	bbo = LABELSECTOR;
	int		rv;

	if (clp->cd_partitions != NULL) {
		if (clp->cd_labelpart == NULL)
			return(ENXIO);
		bbo = clp->cd_labelpart->cp_st;
	}
	bp = geteblk(BBSIZE);
	bp->b_dev      = MAKEDISKDEV(major(dev), DISKUNIT(dev), RAW_PART);
	bp->b_flags    = B_BUSY | B_READ;
	bp->b_bcount   = BBSIZE;
	bp->b_blkno    = bbo;
	bp->b_cylinder = bbo / lp->d_secpercyl;
	(*strat)(bp);
	rv = biowait(bp);
	if (!rv) {
		struct disklabel *nlp = (struct disklabel *)
				((char *)bp->b_data + LABELOFFSET);
		*nlp = *lp;
		bp->b_flags    = B_BUSY | B_WRITE;
		bp->b_bcount   = BBSIZE;
		bp->b_blkno    = bbo;
		bp->b_cylinder = bbo / lp->d_secpercyl;
		(*strat)(bp);
		rv = biowait(bp);
	}
	bp->b_flags |= B_INVAL | B_AGE;
	brelse(bp);
	return(rv);
}

/*
 * Check for consistency between the NetBSD partition table in
 * disklabel `lp' and the TOS partition table in cpu_disklabel
 * `clp'. There's no good reason to force such consistency, but
 * issueing a warning may help an inexperienced sysadmin to
 * prevent corruption of TOS partitions.
 */
static void
check_label(lp, clp)
struct disklabel	*lp;
struct cpu_disklabel	*clp;
{
	int			i, j;

	for (i = 0; i < lp->d_npartitions; i++) {
		struct partition *p = &lp->d_partitions[i];
		if (p->p_size == 0 || i == RAW_PART)
			continue;
		if ( (p->p_offset <= clp->cd_bslst
		   && p->p_offset + p->p_size > clp->cd_bslst)
		  || (p->p_offset > clp->cd_bslst
		   && clp->cd_bslst + clp->cd_bslsize > p->p_offset)) {
			printf("Warning: NetBSD partition `%c' contains "
				"the AHDI bad sector list\n", 'a'+i);
		}

		for (j = 0; j < clp->cd_npartitions; j++) {
			struct cpu_partition *cp = &clp->cd_partitions[i];
			if (cp->cp_id != CPU_PID_XGM)
				continue;
			if (cp->cp_st >= p->p_offset
			  && cp->cp_st < p->p_offset + p->p_size) {
				printf("Warning: NetBSD partition `%c' "
					"contains an AHDI auxilary root "
					"sector\n", 'a'+i);
				break;
			}
		}
	}
}

/*
 * Map the partition table from TOS to the NetBSD table.
 *
 * This means:
 *  Part 0   : Root
 *  Part 1   : Swap
 *  Part 2   : Whole disk
 *  Part 3.. : User partitions
 *
 * When more than one root partition is found, only the first one will
 * be recognized as such. The others are mapped as user partitions.
 */
static void
fake_label(lp, clp)
struct disklabel	*lp;
struct cpu_disklabel	*clp;
{
	int	i, have_root, user_part;

	user_part = RAW_PART;
	have_root = (clp->cd_labelpart != NULL);

	for (i = 0; i < clp->cd_npartitions; i++) {
		struct cpu_partition	*cp = &clp->cd_partitions[i];
		int			fst, pno = -1;

		switch (cp->cp_id) {
			case CPU_PID_XGM:
				continue;
			case CPU_PID_NBD:
				/*
				 * If this partition has been marked as the
				 * first NBD partition, it will be the root
				 * partition.
				 */
				if (cp == clp->cd_labelpart)
					pno = 0;
				/* FALL THROUGH */
			case CPU_PID_NBR:
				/*
				 * If there is no NBD partition and this is
				 * the first NBR partition, it will be the
				 * root partition.
				 */
				if (!have_root) {
					have_root = 1;
					pno = 0;
				}
				/* FALL THROUGH */
			case CPU_PID_NBU:
				fst = FS_BSDFFS;
				break;
			case CPU_PID_NBS:
			case CPU_PID_SWP:
				if (lp->d_partitions[1].p_size == 0)
					pno = 1;
				fst = FS_SWAP;
				break;
			case CPU_PID_BGM:
			case CPU_PID_GEM:
				fst = FS_MSDOS;
				break;
			default:
				fst = FS_OTHER;
				break;
		}
		if (pno < 0) {
			pno = ++user_part;
			if(pno >= MAXPARTITIONS)
				break;	/* XXX */
		}
		lp->d_partitions[pno].p_size   = cp->cp_size;
		lp->d_partitions[pno].p_offset = cp->cp_st;
		lp->d_partitions[pno].p_fstype = fst;
	}
	lp->d_npartitions = user_part + 1;
}

/*
 * Read bootblock at block `offset' and check
 * if it contains a valid disklabel.
 * Returns 0 if an error occured, 1 if successfull.
 */
static int
real_label(dev, strat, offset, lp)
dev_t			dev;
void			(*strat)();
u_int32_t		offset;
struct disklabel	*lp;
{
	struct buf		*bp;
	int			rv = 0;

	bp = geteblk(BBSIZE);
	bp->b_dev      = MAKEDISKDEV(major(dev), DISKUNIT(dev), RAW_PART);
	bp->b_flags    = B_BUSY | B_READ;
	bp->b_bcount   = BBSIZE;
	bp->b_blkno    = offset;
	bp->b_cylinder = offset / lp->d_secpercyl;
	(*strat)(bp);
	if (!biowait(bp)) {
		struct disklabel *nlp = (struct disklabel *)
				((char *)bp->b_data + LABELOFFSET);
		if (nlp->d_magic == DISKMAGIC && nlp->d_magic2 == DISKMAGIC
	  	  && dkcksum(nlp) == 0 && nlp->d_npartitions <= MAXPARTITIONS) {
			*lp = *nlp;
			rv  = 1;
		}
	}
	bp->b_flags = B_INVAL | B_AGE | B_READ;
	brelse(bp);
	return(rv);
}

/*
 * Create a list of TOS partitions in cpu_disklabel `clp'.
 * Returns 0 if an error occured, 1 if successfull.
 */
static int
rd_rootparts(dev, strat, spc, clp)
dev_t			dev;
void			(*strat)();
u_int32_t		spc;
struct cpu_disklabel	*clp;
{
	struct tos_root	*root;
	struct buf	*bp;
	int		i, j, rv = 0;
	int		ntospart  = 0;

	bp = geteblk(TOS_BSIZE);
	bp->b_dev      = MAKEDISKDEV(major(dev), DISKUNIT(dev), RAW_PART);
	bp->b_flags    = B_BUSY | B_READ;
	bp->b_bcount   = TOS_BSIZE;
	bp->b_blkno    = TOS_BBLOCK;
	bp->b_cylinder = TOS_BBLOCK / spc;
	(*strat)(bp);
	if (biowait(bp))
		goto done;

	root = (struct tos_root *)bp->b_data;
	MACHDSBR_DEBUG(("hdsize: %lu bsl-start: %lu bsl-size: %lu\n",
	 		(u_long)root->tr_hdsize, (u_long)root->tr_bslst,
			(u_long)root->tr_bslsize));

	for (i = 0; i < NTOS_PARTS; ++i) {
		struct tos_part	*part = &root->tr_parts[i];
		if (!(part->tp_flg & 1)) /* skip invalid entries */
			continue;
		ntospart++;
		MACHDSBR_DEBUG(("  %c%c%c %9lu %9lu\n",
		  part->tp_id[0], part->tp_id[1], part->tp_id[2],
		    (u_long)part->tp_st, (u_long)part->tp_size));
		/*
		 * a primary partition (normal or extended) must
		 * have a size >= 1 and must not interfere with
		 * the bad sector list, nor with any of the
		 * other primary partitions.
		 */
		if (part->tp_st == 0 || part->tp_size == 0
		  || (part->tp_st <= root->tr_bslst
		   && part->tp_st + part->tp_size > root->tr_bslst)
		  || (part->tp_st >  root->tr_bslst
		   && root->tr_bslst + root->tr_bslsize > part->tp_st))
			goto done;
		for (j = 0; j < i; ++j) {
			struct tos_part	*p = &root->tr_parts[j];
			if ( (p->tp_st <= part->tp_st
			   && p->tp_st + p->tp_size > part->tp_st)
			  || (p->tp_st >  part->tp_st
			   && part->tp_st + part->tp_size > p->tp_st))
				goto done;
		}
		if (add_tospart(clp, part)
		  && !rd_extparts(dev,strat,spc,part->tp_st,part->tp_size,clp))
			goto done;
	}
	rv = ntospart ? 1 : 0;
	clp->cd_bslst   = root->tr_bslst;
	clp->cd_bslsize = root->tr_bslsize;
done:
	if (!rv && clp->cd_partitions != NULL) {
		FREE(clp->cd_partitions, M_DEVBUF);
		clp->cd_partitions  = NULL;
		clp->cd_npartitions = 0;
	}
	bp->b_flags = B_INVAL | B_AGE | B_READ;
	brelse(bp);
	return(rv);
}

/*
 * Add all subpartitions within an extended
 * partition to cpu_disklabel `clp'.
 * Returns 0 if an error occured, 1 if successfull.
 */
static int
rd_extparts(dev, strat, spc, extst, extsize, clp)
dev_t			dev;
void			(*strat)();
u_int32_t		spc, extst, extsize;
struct cpu_disklabel	*clp;
{
	struct buf	*bp;
	u_int32_t	subst = extst, subsize = extsize;
	int		i, rv = 0, pno = clp->cd_npartitions;

	bp = geteblk(TOS_BSIZE);
	bp->b_dev    = MAKEDISKDEV(major(dev), DISKUNIT(dev), RAW_PART);

	for (;;) {
		struct tos_root	*root = (struct tos_root *)bp->b_data;
		struct tos_part	*part = root->tr_parts;

		MACHDSBR_DEBUG(("auxilary root at sector %lu\n",(u_long)subst));
		bp->b_flags    = B_BUSY | B_READ;
		bp->b_bcount   = TOS_BSIZE;
		bp->b_blkno    = subst;
		bp->b_cylinder = subst / spc;
		(*strat)(bp);
		if (biowait(bp))
			goto done;
		/*
		 * The first entry in an auxilary root sector must be
		 * marked as valid. The entry must describe a normal
		 * partition. The partition must not extend beyond
		 * the boundaries of the subpartition that it's
		 * part of.
		 */
		MACHDSBR_DEBUG(("  %c%c%c %9lu %9lu\n",
		  part->tp_id[0], part->tp_id[1], part->tp_id[2],
		    (u_long)part->tp_st, (u_long)part->tp_size));
		if (!(part->tp_flg & 1)
		  || part->tp_st == 0 || part->tp_st >= subsize
		  || part->tp_size == 0 || part->tp_size >= subsize
		  || part->tp_st + part->tp_size > subsize) {
			MACHDSBR_DEBUG(("first entry exceeds parent\n"));
			goto done;
		}
		part->tp_st += subst;
		if (add_tospart(clp, part++)) {
			MACHDSBR_DEBUG(("first entry is XGM\n"));
			goto done;
		}
		/*
		 * If the second entry in an auxilary rootsector is
		 * marked as invalid, we've reached the end of the
		 * linked list of subpartitions.
		 */
		if (!(part->tp_flg & 1)) {
			rv = 1;
			goto done;
		}
		/*
		 * If marked valid, the second entry in an auxilary
		 * rootsector must describe a subpartition (id XGM).
		 * The subpartition must not extend beyond the
		 * boundaries of the extended partition that it's
		 * part of. The subpartition must not share any
		 * sectors with any other subpartition on this
		 * extended partition.
		 */
		MACHDSBR_DEBUG(("  %c%c%c %9lu %9lu\n",
		  part->tp_id[0], part->tp_id[1], part->tp_id[2],
		    (u_long)part->tp_st, (u_long)part->tp_size));
		if (part->tp_st == 0 || part->tp_st >= extsize
		  || part->tp_size == 0 || part->tp_size >= extsize
		  || part->tp_st + part->tp_size > extsize) {
			MACHDSBR_DEBUG(("second entry exceeds parent\n"));
			goto done;
		}
		part->tp_st += extst;
		for (i = pno; i < clp->cd_npartitions; ++i) {
			struct cpu_partition *cp = &clp->cd_partitions[i];
			if (cp->cp_id != CPU_PID_XGM)
				continue; /* skip normal partitions */
			if ( (cp->cp_st <= part->tp_st
			   && cp->cp_st + cp->cp_size > part->tp_st)
			  || (cp->cp_st >  part->tp_st
			   && part->tp_st + part->tp_size > cp->cp_st)) {
				MACHDSBR_DEBUG(("second entry not "
				  "consistent with subpartition %u\n", i));
				goto done;
			}
		}
		if (!add_tospart(clp, part)) {
			MACHDSBR_DEBUG(("second entry is not XGM\n"));
			goto done;
		}
		subsize = part->tp_size;
		subst   = part->tp_st;
	}
done:
	bp->b_flags = B_INVAL | B_AGE | B_READ;
	brelse(bp);
	return(rv);
}

/*
 * Add a TOS partition to the list of TOS partitions
 * in cpu_disklabel `clp'.
 * Returns 1 if `tp' is an XGM partition, otherwise 0.
 */
static int
add_tospart (clp, tp)
struct cpu_disklabel	*clp;
struct tos_part		*tp;
{
	struct cpu_partition	*cp;
	int			i;

	i = clp->cd_npartitions;
	if ((i & 15) == 0) {
		MALLOC(cp, struct cpu_partition *, (i + 16)
				* sizeof *cp, M_DEVBUF, M_WAITOK);
		if (i) {
			bcopy(clp->cd_partitions, cp, i * sizeof *cp);
			FREE(clp->cd_partitions, M_DEVBUF);
		}
		clp->cd_partitions = cp;
	}
	clp->cd_npartitions += 1;
	cp = clp->cd_partitions + i;
	cp->cp_st   = tp->tp_st;
	cp->cp_size = tp->tp_size;
	cp->cp_id   = (u_int32_t)tp->tp_id[2];
	cp->cp_id  |= (u_int32_t)tp->tp_id[1] << 8;
	cp->cp_id  |= (u_int32_t)tp->tp_id[0] << 16;
	return(cp->cp_id == CPU_PID_XGM);
}

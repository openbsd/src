/*	$OpenBSD: rdbdisksubr.c,v 1.2 2003/12/20 22:40:28 miod Exp $	*/
/*	$NetBSD: disksubr.c,v 1.27 1996/10/13 03:06:34 christos Exp $	*/

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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>
#include <sys/disk.h>

struct adostype getadostype(u_long dostype);

u_long rdbchksum(void *bdata);

#define	b_cylin	b_resid
int
try_rdb_label(dev_t dev, void (*strat)(struct buf *), struct buf *bp,
    struct disklabel *lp, struct cpu_disklabel *osdep, char **pmsg,
    int *bsdpartoff);
int
try_rdb_label(dev_t dev, void (*strat)(struct buf *), struct buf *bp,
    struct disklabel *lp, struct cpu_disklabel *clp, char **pmsg,
    int *bsdpartoff)
{
	int nextb, i;
	int rdbpartoff = -1;
	struct rdblock *rbp;
	struct partblock *pbp;
	struct partition *pp = NULL;
	struct adostype adt;
	int cindex;

	clp->rdblock = RDBNULL;

	/* initialize */
	for (i = 0; i < MAXPARTITIONS; i++) {
		clp->pbindex[i] = -1;
		clp->pblist[i] = RDBNULL;
	} 
        if (lp->d_partitions[RAW_PART].p_size == 0)
                lp->d_partitions[RAW_PART].p_size = 0x1fffffff;
        lp->d_partitions[RAW_PART].p_offset = 0;

	lp->d_npartitions = 'i' - 'a';

	bp->b_dev = MAKEDISKDEV(major(dev), DISKUNIT(dev), RAW_PART);

	/* Find RDB block */

	for (nextb = 0; nextb < RDB_MAXBLOCKS; nextb++) {
		bp->b_blkno = nextb;
		bp->b_cylin = nextb / lp->d_secpercyl;
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		strat(bp);

		if (biowait(bp)) {
			*pmsg = "RDB search I/O error";
			goto done;
		}
		rbp = (void *)(bp->b_data);

		if (rbp->id == RDBLOCK_ID) {
			if (rdbchksum(rbp) == 0)
				break;
			else
				*pmsg = "bad rdb checksum";
		}
	}
	if (nextb == RDB_MAXBLOCKS)
		goto done;	/* no RDB found */
	
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

	cindex = 0;
	for (nextb = rbp->partbhead; nextb != RDBNULL; nextb = pbp->next) {
		bp->b_blkno = nextb;
		bp->b_cylin = nextb / lp->d_secpercyl;
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		strat(bp);

		if (biowait(bp)) {
			*pmsg = "RDB partition scan I/O error";
			goto done;
		}
		pbp = (void *)(bp->b_data);

		if (pbp->id != PARTBLOCK_ID) {
			*pmsg = "RDB partition block bad id";
			goto done;
		}
		if (rdbchksum(pbp)) {
			*pmsg = "RDB partition block bad cksum";
			goto done;
		}
		if (pbp->e.tabsize < 11)
			*pmsg = "RDB bad partition info (environ < 11)";

		if (pbp->e.dostype == DOST_OBSD) {
			rdbpartoff = pbp->e.lowcyl * pbp->e.secpertrk
			    * pbp->e.numheads;
			clp->rd_bsdlbl = rdbpartoff;
			continue;
		}

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
				clp->rdblock = RDBNULL; /* invlidate cpulab */
				continue;
			}
			break;
		case ADT_NETBSDSWAP:
			pp = &lp->d_partitions[1];
			if (pp->p_size) {
				printf("WARN: more than one swap, ignoring\n");
				clp->rdblock = RDBNULL; /* invlidate cpulab */
				continue;
			}
			break;
		case ADT_NETBSDUSER:
		case ADT_AMIGADOS:
		case ADT_AMIX:
		case ADT_EXT2:
		case ADT_UNKNOWN:
			pp = &lp->d_partitions[lp->d_npartitions];
			break;
		}

		if (lp->d_npartitions <= (pp - lp->d_partitions))
			lp->d_npartitions = (pp - lp->d_partitions) + 1;

#if 0
		if (lp->d_secpercyl != (pbp->e.secpertrk * pbp->e.numheads)) {
			if (pbp->partname[0] < sizeof(pbp->partname))
				pbp->partname[pbp->partname[0] + 1] = 0;
			else
				pbp->partname[sizeof(pbp->partname) - 1] = 0;
			printf("Partition '%s' geometry %ld/%ld differs",
			    pbp->partname + 1, pbp->e.numheads,
			    pbp->e.secpertrk);
			printf(" from RDB %d/%d\n", lp->d_ntracks,
			    lp->d_nsectors);
		}
#endif
#if 0
		/*
		 * insert sort in increasing offset order
		 */
		while ((pp - lp->d_partitions) > RAW_PART + 1) {
			daddr_t boff;

			boff = pbp->e.lowcyl * pbp->e.secpertrk
			    * pbp->e.numheads;
			if (boff > (pp - 1)->p_offset)
				break;
			*pp = *(pp - 1);        /* struct copy */
			pp--;
		}
#endif
		i = (pp - lp->d_partitions);

		pp->p_size = (pbp->e.highcyl - pbp->e.lowcyl + 1)
		    * pbp->e.secpertrk * pbp->e.numheads;
		pp->p_offset = pbp->e.lowcyl * pbp->e.secpertrk
		    * pbp->e.numheads;
		pp->p_fstype = adt.fstype;
		if (adt.archtype == ADT_AMIGADOS) {
			/*
			 * Save reserved blocks at begin in cpg and
			 *  adjust size by reserved blocks at end
			 */
			pp->p_fsize = 512;
			pp->p_frag = pbp->e.secperblk;
			pp->p_cpg = pbp->e.resvblocks;
			pp->p_size -= pbp->e.prefac;
		} else if (pbp->e.tabsize > 22 && ISFSARCH_NETBSD(adt)) {
			pp->p_fsize = pbp->e.fsize;
			pp->p_frag = pbp->e.frag;
			pp->p_cpg = pbp->e.cpg;
		} else {
			pp->p_fsize = 1024;
			pp->p_frag = 8;
			pp->p_cpg = 0;
		}

		/*
		 * store this partitions block number
		 */
		clp->pbindex[i] = cindex; 
		clp->pblist[cindex] = nextb;
		cindex++;
	}
done:
	if (clp->rdblock != RDBNULL && rdbpartoff != -1)
		*bsdpartoff = rdbpartoff;
	bp->b_dev = dev;
	return (clp->rdblock != RDBNULL);
}

struct adostype 
getadostype(u_long dostype)
{
	struct adostype adt;
	u_long t3, b1;

	t3 = dostype & 0xffffff00;
	b1 = dostype & 0x000000ff;

	adt.fstype = b1;

	switch (t3) {
	case DOST_NBR:
		adt.archtype = ADT_NETBSDROOT;
		return (adt);
	case DOST_NBS:
		adt.archtype = ADT_NETBSDSWAP;
		return (adt);
	case DOST_NBU:
		adt.archtype = ADT_NETBSDUSER;
		return (adt);
	case DOST_MUFS:
		/* check for 'muFS'? */
		adt.archtype = ADT_AMIGADOS;
		adt.fstype = FS_ADOS;
		return (adt);
	case DOST_DOS:
		adt.archtype = ADT_AMIGADOS;
                if (b1 > 5)
#if 0
			/*
			 * XXX at least I, <niklas@appli.se>, have a partition 
			 * that looks like "DOS\023", wherever that came from,
			 * but ADOS accepts it, so should we.
			 */
			goto unknown;
		else
#else
			printf("found dostype: 0x%lx, assuming an ADOS FS "
			    "although it's unknown\n", dostype);
#endif
		adt.fstype = FS_ADOS;
		return (adt);
	case DOST_AMIX:
		adt.archtype = ADT_AMIX;
		if (b1 == 2)
			adt.fstype = FS_BSDFFS;
		else
			goto unknown;
		return (adt);
	case DOST_XXXBSD:
#ifdef DIAGNOSTIC
		printf("found dostype: 0x%lx which is deprecated", dostype);
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
		printf(" using: 0x%lx instead\n", dostype);
#endif
		return (getadostype(dostype));
	case DOST_EXT2:
		adt.archtype = ADT_EXT2;
		adt.fstype = FS_EXT2FS;
		return(adt);
	default:
	unknown:
#ifdef DIAGNOSTIC
		printf("warning unknown dostype: 0x%lx marking unused\n",
		    dostype);
#endif
		adt.archtype = ADT_UNKNOWN;
		adt.fstype = FS_UNUSED;
		return (adt);
	}	
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
	return (val);
}

/*	$OpenBSD: disksubr_mbr.c,v 1.1 2004/02/01 05:09:48 drahn Exp $	*/
/*	$NetBSD: disksubr.c,v 1.21 1996/05/03 19:42:03 christos Exp $	*/

/*
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
 *
 *	@(#)ufs_disksubr.c	7.16 (Berkeley) 5/4/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>
#include <sys/disk.h>

#define	b_cylin	b_resid

#define BOOT_MAGIC 0xAA55
#define BOOT_MAGIC_OFF (DOSPARTOFF+NDOSPART*sizeof(struct dos_partition))

int
try_mbr_label(dev_t dev, void (*strat)(struct buf *), struct buf *bp,
    struct disklabel *lp, struct cpu_disklabel *osdep, char **pmsg,
    int *bsdpartoff);
int
try_mbr_label(dev_t dev, void (*strat)(struct buf *), struct buf *bp,
    struct disklabel *lp, struct cpu_disklabel *osdep, char **pmsg,
    int *bsdpartoff)
{
	struct dos_partition *dp = osdep->dosparts, *dp2;
	char *cp;
	int cyl, n = 0, i, ourpart = -1;
	int dospartoff = -1;

	/* MBR type disklabel */
	/* do dos partitions in the process of getting disklabel? */
	cyl = LABELSECTOR / lp->d_secpercyl;
	if (dp) {
	        daddr_t part_blkno = DOSBBSECTOR;
		unsigned long extoff = 0;
		int wander = 1, loop = 0;

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
				*pmsg = "dos partition I/O error";
				return 0;
			}
			bcopy(bp->b_data + DOSPARTOFF, dp, NDOSPART * sizeof(*dp));

			if (ourpart == -1) {
				/* Search for our MBR partition */
				for (dp2=dp, i=0; i < NDOSPART && ourpart == -1;
				    i++, dp2++)
					if (get_le(&dp2->dp_size) &&
					    dp2->dp_typ == DOSPTYP_OPENBSD)
						ourpart = i;
				for (dp2=dp, i=0; i < NDOSPART && ourpart == -1;
				    i++, dp2++)
					if (get_le(&dp2->dp_size) &&
					    dp2->dp_typ == DOSPTYP_FREEBSD)
						ourpart = i;
				for (dp2=dp, i=0; i < NDOSPART && ourpart == -1;
				    i++, dp2++)
					if (get_le(&dp2->dp_size) &&
					    dp2->dp_typ == DOSPTYP_NETBSD)
						ourpart = i;
				if (ourpart == -1)
					goto donot;
				/*
				 * This is our MBR partition. need sector address
				 * for SCSI/IDE, cylinder for ESDI/ST506/RLL
				 */
				dp2 = &dp[ourpart];
				dospartoff = get_le(&dp2->dp_start) + part_blkno;
				cyl = DPCYL(dp2->dp_scyl, dp2->dp_ssect);

				/* XXX build a temporary disklabel */
				lp->d_partitions[0].p_size = get_le(&dp2->dp_size);
				lp->d_partitions[0].p_offset =
					get_le(&dp2->dp_start) + part_blkno;
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
				if (get_le(&dp2->dp_size) > lp->d_secperunit)
					continue;
				if (get_le(&dp2->dp_size))
					pp->p_size = get_le(&dp2->dp_size);
				if (get_le(&dp2->dp_start))
					pp->p_offset =
					    get_le(&dp2->dp_start) + part_blkno;

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
				case DOSPTYP_FAT16C:
				case DOSPTYP_FAT32:
					pp->p_fstype = FS_MSDOS;
					n++;
					break;
				case DOSPTYP_EXTEND:
				case DOSPTYP_EXTENDL:
					part_blkno = get_le(&dp2->dp_start) + extoff;
					if (!extoff) {
						extoff = get_le(&dp2->dp_start);
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
		lp->d_npartitions = MAXPARTITIONS;
	}

	/* if not partitions found return failure */
	if (n == 0 && dospartoff == -1)
		return 0;
	*bsdpartoff = dospartoff + LABELSECTOR;
	return 1;
}

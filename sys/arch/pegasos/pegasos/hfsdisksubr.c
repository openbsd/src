/*	$OpenBSD: hfsdisksubr.c,v 1.1 2003/11/13 23:00:55 drahn Exp $	*/
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

#define      b_cylin b_resid

int
try_hfs_label(dev_t dev, void (*strat)(struct buf *), struct buf *bp,
    struct disklabel *lp, struct cpu_disklabel *osdep, char **pmsg,
    int *bsdpartoff);

int
try_hfs_label(dev_t dev, void (*strat)(struct buf *), struct buf *bp,
    struct disklabel *lp, struct cpu_disklabel *osdep, char **pmsg,
    int *bsdpartoff)
{
	int part_cnt, n, i;
	struct part_map_entry *part;
	int hfspartoff = -1;
	char *s;

	bp->b_blkno = 1; 
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylin = 1 / lp->d_secpercyl;
	(*strat)(bp);

	/* if successful, wander through DPME partition table */
	if (biowait(bp)) {
		*pmsg = "DPME partition I/O error";
		return 0;
	}

	part = (struct part_map_entry *)bp->b_data;
	/* if first partition is not valid, assume not HFS/DPME partitioned */
        if (part->pmSig != PART_ENTRY_MAGIC) {
		osdep->macparts[0].pmSig = 0; /* make invalid */
		return 0;
        }
	osdep->macparts[0] = *part;
	part_cnt = part->pmMapBlkCnt;
	n = 0;
	for (i = 0; i < part_cnt; i++) {
		struct partition *pp = &lp->d_partitions[8+n];

		bp->b_blkno = 1+i; 
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		bp->b_cylin = 1+i / lp->d_secpercyl;
		(*strat)(bp);

		if (biowait(bp)) {
			*pmsg = "DPME partition I/O error";
			return 0;
		}
		part = (struct part_map_entry *)bp->b_data;
		/* toupper the string, in case caps are different... */
		for (s = part->pmPartType; *s; s++)
			if ((*s >= 'a') && (*s <= 'z'))
				*s = (*s - 'a' + 'A');

		if (0 == strcmp(part->pmPartType, PART_TYPE_OPENBSD)) {
			hfspartoff = part->pmPyPartStart;
			osdep->macparts[1] = *part;
		}
		/* currently we ignore all but HFS partitions */
		if (0 == strcmp(part->pmPartType, PART_TYPE_MAC)) {
			pp->p_offset = part->pmPyPartStart;
			pp->p_size = part->pmPartBlkCnt;
			pp->p_fstype = FS_HFS;
			n++;
#if 0
			printf("found DPME HFS partition [%s], adding to fake\n",
				part->pmPartName);
#endif
		}
	}
	lp->d_npartitions = MAXPARTITIONS;

	*bsdpartoff = hfspartoff;
	return 1;
}

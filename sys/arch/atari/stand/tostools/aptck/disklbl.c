/*	$NetBSD: disklbl.c,v 1.2 1996/01/20 13:54:46 leo Exp $	*/

/*
 * Copyright (c) 1995 Waldi Ravens.
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
 *        This product includes software developed by Waldi Ravens.
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

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include "libtos.h"
#include "aptck.h"
#include "ahdilbl.h"
#include "disklbl.h"

static int	dkcksum    PROTO((struct disklabel *));
static int	bsd_label  PROTO((disk_t *, u_int));
static int	ahdi_label PROTO((disk_t *));
static int	ahdi_display PROTO((disk_t *));
static u_int	ahdi_getparts PROTO((disk_t *, u_int, u_int));

int
readdisklabel(dd)
	disk_t	*dd;
{
	int	e;

	printf("Device     : %s (%s) [%s]\n", dd->sname, dd->fname, dd->product);
	printf("Medium size: %lu sectors\n", (u_long)dd->msize);
	printf("Sector size: %lu bytes\n\n", (u_long)dd->bsize);

	e = bsd_label(dd, LABELSECTOR);
	if (e < 0) {
		printf("Device I/O error (hardware problem?)\n\n");
		return(-1);
	}
	if (!e) {
		printf("NetBSD/Atari format, boot block: "
		       "sector %u labeloffset %u\n\n",
		       dd->bblock, dd->lblofs);
		return(0);
	}

	e = ahdi_label(dd);
	if (e < 0) {
		printf("Device I/O error (hardware problem?)\n\n");
		return(-1);
	}
	if (!e) {
		printf("AHDI format, NetBSD boot block: ");
		if (dd->bblock != NO_BOOT_BLOCK)
			printf("sector %u labeloffset %u\n\n",
			       dd->bblock, dd->lblofs);
		else printf("none\n\n");
		return(0);
	}

	printf("Unknown label format.\n\n");
	return(-1);
}

static int
bsd_label(dd, offset)
	disk_t		*dd;
	u_int		offset;
{
	u_char		*bblk;
	u_int		nsec;
	int		rv;

	nsec = (BBMINSIZE + (dd->bsize - 1)) / dd->bsize;
	bblk = disk_read(dd, offset, nsec);
	if (bblk) {
		u_short	*end, *p;
		
		end = (u_short *)&bblk[BBMINSIZE - sizeof(struct disklabel)];
		rv = 1;
		for (p = (u_short *)bblk; p < end; ++p) {
			struct disklabel *dl = (struct disklabel *)p;
			if (dl->d_magic == DISKMAGIC && dl->d_magic2 == DISKMAGIC
		    	    && dl->d_npartitions <= MAXPARTITIONS && !dkcksum(dl)) {
		    		dd->lblofs = (u_char *)p - bblk;
		    		dd->bblock = offset;
				rv = 0;
				break;
			}
		}
		free(bblk);
	}
	else rv = -1;

	return(rv);
}

static int
dkcksum(dl)
	struct disklabel *dl;
{
	u_short	*start, *end, sum = 0;

	start = (u_short *)dl;
	end   = (u_short *)&dl->d_partitions[dl->d_npartitions];
	while (start < end)
		sum ^= *start++;
	return(sum);
}

int
ahdi_label(dd)
	disk_t	*dd;
{
	u_int	i;
	int	e;

	/*
	 * The AHDI format requires a specific block size.
	 */
	if (dd->bsize != AHDI_BSIZE)
		return(1);

	/*
	 * Fetch the AHDI partition descriptors.
	 */
	i = ahdi_getparts(dd, AHDI_BBLOCK, AHDI_BBLOCK);
	if (i) {
		if (i < dd->msize)
			return(-1);	/* disk read error		*/
		else return(1);		/* reading past end of medium	*/
	}

	/*
	 * Display and perform sanity checks.
	 */
	i = ahdi_display(dd);
	if (i)
		return(i);

	/*
	 * Search for a NetBSD disk label
	 */
	dd->bblock = NO_BOOT_BLOCK;
	for (i = 0; i < dd->nparts; ++i) {
		part_t	*pd = &dd->parts[i];
		u_int	id  = *((u_int32_t *)&pd->id) >> 8;
		if (id == AHDI_PID_NBD || id == AHDI_PID_RAW) {
			u_int	offs = pd->start;
			if ((e = bsd_label(dd, offs)) < 0) {
				return(e);		/* I/O error */
			}
			if (!e) {
				dd->bblock = offs;	/* got it */
				return(0);
			}
			if (id == AHDI_PID_NBD && dd->bblock == NO_BOOT_BLOCK)
				dd->bblock = offs;
		}
	}
	return(0);
}

static int
root_cmp(x1, x2)
	const void	*x1, *x2;
{
	const u_int	*r1 = x1,
			*r2 = x2;

	if (*r1 < *r2)
		return(-1);
	if (*r1 > *r2)
		return(1);
	return(0);
}

static int
part_cmp(x1, x2)
	const void	*x1, *x2;
{
	const part_t	*p1 = x1,
			*p2 = x2;

	if (p1->start < p2->start)
		return(-1);
	if (p1->start > p2->start)
		return(1);
	if (p1->end < p2->end)
		return(-1);
	if (p1->end > p2->end)
		return(1);
	if (p1->rsec < p2->rsec)
		return(-1);
	if (p1->rsec > p2->rsec)
		return(1);
	if (p1->rent < p2->rent)
		return(-1);
	if (p1->rent > p2->rent)
		return(1);
	return(0);
}

static int
ahdi_display(dd)
	disk_t	*dd;
{
	int	i, j, rv = 0;

	printf("Start of bad sector list : %u\n", dd->bslst);
	if (dd->bslst == 0) {
		printf("* Illegal value (zero) *\n"); rv = 1;
	}
	printf("End of bad sector list   : %u\n", dd->bslend);
	if (dd->bslend == 0) {
		printf("* Illegal value (zero) *\n"); rv = 1;
	}
	printf("Medium size (in root sec): %u\n", dd->hdsize);
	if (dd->hdsize == 0) {
		printf("* Illegal value (zero) *\n"); rv = 1;
	}

	qsort(dd->roots, dd->nroots, sizeof *dd->roots, root_cmp);
	qsort(dd->parts, dd->nparts, sizeof *dd->parts, part_cmp);
	printf("\n    root  desc   id     start       end    MBs\n");

	for (i = 0; i < dd->nparts; ++i) {
		part_t	*p1 = &dd->parts[i];
		u_int	megs = p1->end - p1->start + 1,
			blpm = (1024 * 1024) / dd->bsize;
		megs = (megs + (blpm >> 1)) / blpm;
		printf("%8u  %4u  %s  %8u  %8u  (%3u)\n",
			p1->rsec, p1->rent, p1->id,
			p1->start, p1->end, megs);
		for (j = 0; j < dd->nroots; ++j) {
			u_int	aux = dd->roots[j];
			if (aux >= p1->start && aux <= p1->end) {
				printf("FATAL: auxilary root at %u\n", aux); rv = 1;
			}
		}
		for (j = i; j--;) {
			part_t	*p2 = &dd->parts[j];
			if (p1->start >= p2->start && p1->start <= p2->end) {
				printf("FATAL: clash with %u/%u\n", p2->rsec, p2->rent); rv = 1;
			}
			if (p2->start >= p1->start && p2->start <= p1->end) {
				printf("FATAL: clash with %u/%u\n", p2->rsec, p2->rent); rv = 1;
			}
		}
		if (p1->start >= dd->bslst && p1->start <= dd->bslend) {
			printf("FATAL: partition overlaps with bad sector list\n"); rv = 1;
		}
		if (dd->bslst >= p1->start && dd->bslst <= p1->end) {
			printf("FATAL: partition overlaps with bad sector list\n"); rv = 1;
		}
	}

	printf("\nTotal number of auxilary roots: %u\n", dd->nroots);
	printf("Total number of partitions    : %u\n", dd->nparts);
	if (dd->nparts == 0) {
		printf("* Weird # of partitions (zero) *\n"); rv = 1;
	}
	if (dd->nparts > AHDI_MAXPARTS) {
		printf("* Too many AHDI partitions for the default NetBSD "
			"kernel *\n  Increase MAXAUXROOTS in src/sys/arch/"
			"atari/include/disklabel.h\n  to at least %u, and "
			"recompile the NetBSD kernel.\n", dd->nroots);
		rv = -1;
	}
	return(rv);
}

static u_int
ahdi_getparts(dd, rsec, esec)
	disk_t			*dd;
	u_int			rsec,
				esec;
{
	struct ahdi_part	*part, *end;
	struct ahdi_root	*root;
	u_int			rv;

	root = disk_read(dd, rsec, 1);
	if (!root) {
		rv = rsec + (rsec == 0);
		goto done;
	}

	if (rsec == AHDI_BBLOCK)
		end = &root->ar_parts[AHDI_MAXRPD];
	else end = &root->ar_parts[AHDI_MAXARPD];
	for (part = root->ar_parts; part < end; ++part) {
		u_int	id = *((u_int32_t *)&part->ap_flg);
		if (!(id & 0x01000000))
			continue;
		if ((id &= 0x00ffffff) == AHDI_PID_XGM) {
			u_int	offs = part->ap_offs + esec;
			u_int	i = ++dd->nroots;
			dd->roots = xrealloc(dd->roots, i * sizeof *dd->roots);
			dd->roots[--i] = offs;
			rv = ahdi_getparts(dd, offs, esec == AHDI_BBLOCK ? offs : esec);
			if (rv)
				goto done;
		} else {
			part_t	*p;
			u_int	i = ++dd->nparts;
			dd->parts = xrealloc(dd->parts, i * sizeof *dd->parts);
			p = &dd->parts[--i];
			*((u_int32_t *)&p->id) = id << 8;
			p->start = part->ap_offs + rsec;
			p->end   = p->start + part->ap_size - 1;
			p->rsec  = rsec;
			p->rent  = part - root->ar_parts;
		}
	}
	dd->hdsize = root->ar_hdsize;
	dd->bslst  = root->ar_bslst;
	dd->bslend = root->ar_bslst + root->ar_bslsize - 1;
	rv = 0;
done:
	free(root);
	return(rv);
}

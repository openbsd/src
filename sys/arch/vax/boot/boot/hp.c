/*	$OpenBSD: hp.c,v 1.2 2002/06/11 09:36:23 hugh Exp $ */
/*	$NetBSD: hp.c,v 1.5 2000/07/19 00:58:25 matt Exp $ */
/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
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

 /* All bugs are subject to removal without further notice */
		


#include "sys/param.h"
#include "sys/disklabel.h"

#include "lib/libsa/stand.h"

#include "../include/pte.h"
#include "../include/rpb.h"
#include "../include/sid.h"
#define VAX780 1
struct proc;
#include "../include/ka750.h"

#include "../mba/mbareg.h"
#include "../mba/hpreg.h"

#include "vaxstand.h"

/*
 * These routines for HP disk standalone boot is wery simple,
 * assuming a lots of thing like that we only working at one hp disk
 * a time, no separate routines for mba driver etc..
 * But it works :)
 */

static struct disklabel hplabel;
static char io_buf[DEV_BSIZE];
static int dpart;
static int adpadr, unitadr;

#define	MBA_WCSR(reg, val) \
	((void)(*(volatile u_int32_t *)((adpadr) + (reg)) = (val)));
#define MBA_RCSR(reg) \
	(*(volatile u_int32_t *)((adpadr) + (reg)))
#define	HP_WCSR(reg, val) \
	((void)(*(volatile u_int32_t *)((unitadr) + (reg)) = (val)));
#define HP_RCSR(reg) \
	(*(volatile u_int32_t *)((unitadr) + (reg)))

int
hpopen(struct open_file *f, int adapt, int ctlr, int unit, int part)
{
	char *msg;
	int err;
	size_t i;

	if (askname == 0) { /* Take info from RPB */
		adpadr = bootrpb.adpphy;
		unitadr = adpadr + MUREG(bootrpb.unit, 0);
	} else {
		adpadr = nexaddr;
		unitadr = adpadr + MUREG(unit, 0);
		bootrpb.adpphy = adpadr;
		bootrpb.unit = unit;
	}
	bzero(&hplabel, sizeof(struct disklabel));

	hplabel.d_secpercyl = 32;
	hplabel.d_nsectors = 32;

	/* Set volume valid and 16 bit format; only done once */
	MBA_WCSR(MBA_CR, MBACR_INIT);
	HP_WCSR(HP_CS1, HPCS_PA);
	HP_WCSR(HP_OF, HPOF_FMT);

	err = hpstrategy(0, F_READ, LABELSECTOR, DEV_BSIZE, io_buf, &i);
	if (err) {
		printf("reading disklabel: %s\n", strerror(err));
		return 0;
	}

	msg = getdisklabel(io_buf + LABELOFFSET, &hplabel);
	if (msg)
		printf("getdisklabel: %s\n", msg);
	return 0;
}

int
hpstrategy(void *f, int func, daddr_t dblk,
    size_t size, void *buf, size_t *rsize)
{
	unsigned int pfnum, mapnr, nsize, bn, cn, sn, tn;

	pfnum = (u_int)buf >> VAX_PGSHIFT;

	for(mapnr = 0, nsize = size; (nsize + VAX_NBPG) > 0;
	    nsize -= VAX_NBPG, mapnr++, pfnum++)
		MBA_WCSR(MAPREG(mapnr), PG_V | pfnum);

	MBA_WCSR(MBA_VAR, ((u_int)buf & VAX_PGOFSET));
	MBA_WCSR(MBA_BC, (~size) + 1);
	bn = dblk + hplabel.d_partitions[dpart].p_offset;

	if (bn) {
		cn = bn / hplabel.d_secpercyl;
		sn = bn % hplabel.d_secpercyl;
		tn = sn / hplabel.d_nsectors;
		sn = sn % hplabel.d_nsectors;
	} else
		cn = sn = tn = 0;

	HP_WCSR(HP_DC, cn);
	HP_WCSR(HP_DA, (tn << 8) | sn);
#ifdef notdef
	if (func == F_WRITE)
		HP_WCSR(HP_CS1, HPCS_WRITE);
	else
#endif
		HP_WCSR(HP_CS1, HPCS_READ);

	while (MBA_RCSR(MBA_SR) & MBASR_DTBUSY)
		;

	if (MBA_RCSR(MBA_SR) & MBACR_ABORT)
		return 1;

	*rsize = size;
	return 0;
}

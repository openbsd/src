/*	$OpenBSD: rom.c,v 1.5 2011/03/13 00:13:53 deraadt Exp $ */
/*	$NetBSD: rom.c,v 1.3 2000/07/19 00:58:25 matt Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by
 * Bertram Barth.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
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

#include "sys/param.h"
#include "sys/reboot.h"
#include "sys/disklabel.h"

#include "lib/libsa/stand.h"
#include "lib/libsa/ufs.h"

#include "../include/pte.h"
#include "../include/sid.h"
#include "../include/mtpr.h"
#include "../include/reg.h"
#include "../include/rpb.h"

#include "data.h"
#include "vaxstand.h"

static struct disklabel romlabel;
static char io_buf[DEV_BSIZE];
static struct bqo *bqo;
static int dpart, dunit;

int
romopen(struct open_file *f, int adapt, int ctlr, int unit, int part)
{
	char *msg;
	struct disklabel *lp = &romlabel;
	size_t i;
	int err;

	bqo = (void *)bootrpb.iovec;

	if (bootrpb.unit > 0 && (bootrpb.unit % 100) == 0) {
		printf ("changing bootrpb.unit from %d ", bootrpb.unit);
		bootrpb.unit /= 100;
		printf ("to %d\n", bootrpb.unit);
	}

	bzero(lp, sizeof(struct disklabel));
	dunit = unit;
	dpart = part;

	err = romstrategy(0, F_READ, LABELSECTOR, DEV_BSIZE, io_buf, &i);
	if (err) {
		printf("reading disklabel: %s\n",strerror(err));
		return 0;
	}
	msg = getdisklabel(io_buf+LABELOFFSET, lp);
	if (msg)
		printf("getdisklabel: %s\n",msg);
	return(0);
}

int	romwrite_uvax(int, int, void *, struct rpb *);
int	romread_uvax(int, int, void *, struct rpb *);

int
romstrategy (f, func, dblk, size, buf, rsize)
	void *f;
	int func;
	daddr32_t dblk;
	size_t size;
	void *buf;
	size_t *rsize;
{
	struct	disklabel *lp;
	int	block;

	lp = &romlabel;
	block = dblk + lp->d_partitions[dpart].p_offset;
	if (dunit >= 0 && dunit < 10)
		bootrpb.unit = dunit;

	if (func == F_WRITE)
		romwrite_uvax(block, size, buf, &bootrpb);
	else
		romread_uvax(block, size, buf, &bootrpb);

	*rsize = size;
	return 0;
}


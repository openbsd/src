/*	$OpenBSD: rom.c,v 1.3 1998/05/13 07:30:26 niklas Exp $ */
/*	$NetBSD: rom.c,v 1.1 1996/08/02 11:22:21 ragge Exp $ */
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

extern unsigned *bootregs;
extern struct rpb *rpb;

struct rom_softc {
	int part;
	int unit;
};

int	romstrategy(), romopen();
struct	disklabel romlabel;
struct  rom_softc rom_softc;
char	io_buf[DEV_BSIZE];

romopen(f, adapt, ctlr, unit, part)
	struct open_file *f;
        int ctlr, unit, part;
{
	char *msg;
	struct disklabel *lp = &romlabel;
	volatile struct rom_softc *rsc = &rom_softc;
	int i,err;

	bootregs[11] = XXRPB;
	rpb = (void*)XXRPB;
	bqo = (void*)rpb->iovec;

	if (rpb->unit > 0 && (rpb->unit % 100) == 0) {
		printf ("changing rpb->unit from %d ", rpb->unit);
		rpb->unit /= 100;
		printf ("to %d\n", rpb->unit);
	}

	bzero(lp, sizeof(struct disklabel));
	rsc->unit = unit;
	rsc->part = part;

	err = romstrategy(rsc, F_READ, LABELSECTOR, DEV_BSIZE, io_buf, &i);
	if (err) {
		printf("reading disklabel: %s\n",strerror(err));
		return 0;
	}
	msg = getdisklabel(io_buf+LABELOFFSET, lp);
	if (msg)
		printf("getdisklabel: %s\n",msg);
	f->f_devdata = (void*)rsc;
	return(0);
}

romstrategy (rsc, func, dblk, size, buf, rsize)
	struct  rom_softc *rsc;
	int     func;
	daddr_t dblk;
	char    *buf;
	int     size, *rsize;
{
	struct	disklabel *lp;
	int	block;

	lp = &romlabel;
	block = dblk + lp->d_partitions[rsc->part].p_offset;
	if (rsc->unit >= 0 && rsc->unit < 10)
		rpb->unit = rsc->unit;

	if (func == F_WRITE)
		romwrite_uvax(block, size, buf, bootregs);
	else
		romread_uvax(block, size, buf, bootregs);

	*rsize = size;
	return 0;
}


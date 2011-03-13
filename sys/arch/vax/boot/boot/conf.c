/*	$OpenBSD: conf.c,v 1.5 2011/03/13 00:13:53 deraadt Exp $ */
/*	$NetBSD: conf.c,v 1.10 2000/06/15 19:53:23 ragge Exp $ */
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

#include <netinet/in.h>

#include "../../include/rpb.h"

#include "lib/libkern/libkern.h"

#include "lib/libsa/stand.h"
#include "lib/libsa/ufs.h"
#include "lib/libsa/nfs.h"
#include "lib/libsa/cd9660.h"

#include "vaxstand.h"

static int nostrategy(void *, int, daddr32_t, size_t, void *, size_t *);

struct	devsw devsw[]={
	SADEV("hp",hpstrategy, hpopen, nullsys, noioctl),
	SADEV("qe",nostrategy, qeopen, qeclose, noioctl), /* DEQNA */
	SADEV("ctu",ctustrategy, ctuopen, nullsys, noioctl),
	SADEV("ra",rastrategy, raopen, nullsys, noioctl),
	SADEV("mt",rastrategy, raopen, nullsys, noioctl),
        SADEV("rom",romstrategy, romopen, nullsys, noioctl),
        SADEV("hd",mfmstrategy, mfmopen, nullsys, noioctl),
        SADEV("sd",romstrategy, romopen, nullsys, noioctl),
	SADEV("sd",romstrategy, romopen, nullsys, noioctl),	/* SDN */
	SADEV("sd",romstrategy, romopen, nullsys, noioctl),	/* SDS */
	SADEV("st",nullsys, nullsys, nullsys, noioctl),
	SADEV("le",nostrategy, leopen, leclose, noioctl), /* LANCE */
	SADEV("ze",nostrategy, zeopen, zeclose, noioctl), /* SGEC */
	SADEV("rl",romstrategy, romopen, nullsys, noioctl),
	SADEV("de",nostrategy, deopen, declose, noioctl), /* DEUNA */
	SADEV("ni",nostrategy, niopen, nullsys, noioctl), /* DEBNA */
};

int	cnvtab[] = {
	BDEV_HP,
	BDEV_QE,
	BDEV_CNSL,
	BDEV_UDA,
	BDEV_TK,
	-1,
	BDEV_RD,
	BDEV_SD,
	BDEV_SDN,
	BDEV_SDS,
	BDEV_ST,
	BDEV_LE,
	BDEV_ZE,
	BDEV_RL,
	BDEV_DE,
	BDEV_NI,
};

int     ndevs = (sizeof(devsw)/sizeof(devsw[0]));

struct fs_ops file_system[] = {
	{ ufs_open, ufs_close, ufs_read, ufs_write, ufs_seek, ufs_stat },
	{ nfs_open, nfs_close, nfs_read, nfs_write, nfs_seek, nfs_stat },
	{ cd9660_open, cd9660_close, cd9660_read, cd9660_write,
	    cd9660_seek, cd9660_stat },
};

int nfsys = (sizeof(file_system) / sizeof(struct fs_ops));

int
nostrategy(void *f, int func, daddr32_t dblk,
    size_t size, void *buf, size_t *rsize)
{
	*rsize = size;
	bzero(buf, size);
	return 0;
}

/*	$OpenBSD: conf.c,v 1.5 1998/02/03 11:48:25 maja Exp $ */
/*	$NetBSD: conf.c,v 1.8 1997/04/10 21:25:21 ragge Exp $ */
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

#include <machine/rpb.h>

#include "lib/libsa/stand.h"
#include "lib/libsa/ufs.h"
#include "lib/libsa/nfs.h"

#include "vaxstand.h"

int	raopen(),  rastrategy();
int	hpopen(),  hpstrategy();
int	ctuopen(),  ctustrategy();
int     tmscpopen(), tmscpstrategy();
int     romopen(), romstrategy();
int     mfmopen(), mfmstrategy();
int     sdopen(), sdstrategy();
int	netopen(), netstrategy(), netclose();

struct	devsw devsw[]={
	SADEV("hp",hpstrategy, hpopen, nullsys, noioctl),
	SADEV("qe",netstrategy, netopen, netclose, noioctl), /* DEQNA */
	SADEV("ctu",ctustrategy, ctuopen, nullsys, noioctl),
	SADEV("ra",rastrategy, raopen, nullsys, noioctl),
	SADEV("mt",tmscpstrategy, tmscpopen, nullsys, noioctl),
        SADEV("rom",romstrategy, romopen, nullsys, noioctl),
        SADEV("rd",mfmstrategy, mfmopen, nullsys, noioctl),
        SADEV("sd",sdstrategy, sdopen, nullsys, noioctl),
	SADEV("st",sdstrategy, sdopen, nullsys, noioctl),
	SADEV("le",netstrategy, netopen, netclose, noioctl), /* LANCE */
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
	BDEV_ST,
	BDEV_LE,
};

int     ndevs = (sizeof(devsw)/sizeof(devsw[0]));

struct fs_ops file_system[] = {
	{ ufs_open, ufs_close, ufs_read, ufs_write, ufs_seek, ufs_stat }
};

struct fs_ops nfs_system[] = {
	{ nfs_open, nfs_close, nfs_read, nfs_write, nfs_seek, nfs_stat },
};

int nfsys = (sizeof(file_system) / sizeof(struct fs_ops));

extern struct netif_driver qe_driver;
extern struct netif_driver le_driver;
 
struct netif_driver *netif_drivers[] = {
/*	&qe_driver,  */
	&le_driver,
}; 
int     n_netif_drivers = (sizeof(netif_drivers) / sizeof(netif_drivers[0]));


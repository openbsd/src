/*	$OpenBSD: tmscp.c,v 1.4 2011/03/13 00:13:53 deraadt Exp $ */
/*	$NetBSD: tmscp.c,v 1.3 1999/06/30 18:19:26 ragge Exp $ */
/*
 * Copyright (c) 1995 Ludd, University of Lule}, Sweden.
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
		
#define NRSP 0 /* Kludge */
#define NCMD 0 /* Kludge */

#include "sys/param.h"
#include "sys/disklabel.h"

#include "lib/libsa/stand.h"

#include "../include/pte.h"
#include "arch/vax/mscp/mscp.h"
#include "arch/vax/mscp/mscpreg.h"

#include "vaxstand.h"

static command(int,int);

/*
 * These routines for TMSCP tape standalone boot is very simple,
 * assuming a lots of thing like that we only working at one tape at
 * a time, no separate routines for uba driver etc..
 * This code is directly copied from ra disk driver.
 */

struct ra_softc {
	int udaddr;
	int ubaddr;
	int unit;
};

static volatile struct uda {
        struct  mscp_1ca uda_ca;           /* communications area */
        struct  mscp uda_rsp;     /* response packets */
        struct  mscp uda_cmd;     /* command packets */
} uda;

struct  udadevice {
	short udaip;
	short udasa;
};

static volatile struct uda *ubauda;
static volatile struct udadevice *udacsr;
static struct ra_softc ra_softc;
static int curblock;


tmscpopen(f, adapt, ctlr, unit, part)
	struct open_file *f;
        int ctlr, unit, part;
{
	char *msg;
	extern u_int tmsaddr;
	volatile struct ra_softc *ra=&ra_softc;
	volatile u_int *nisse;
	unsigned short johan;
	int i,err;

	curblock = 0;
	if(adapt>nuba) return(EADAPT);
	if(ctlr>nuda) return(ECTLR);
	ra->udaddr=uioaddr[adapt]+tmsaddr;
	ra->ubaddr=(int)ubaaddr[adapt];
	ra->unit=unit;
	udacsr=(void*)ra->udaddr;
	nisse=((u_int *)ubaaddr[adapt]) + 512;
	nisse[494]=PG_V|(((u_int)&uda)>>9);
	nisse[495]=nisse[494]+1;
	ubauda=(void*)0x3dc00+(((u_int)(&uda))&0x1ff);

	/*
	 * Init of this tmscp ctlr.
	 */
	udacsr->udaip=0; /* Start init */
	while((udacsr->udasa&MP_STEP1) == 0);
	udacsr->udasa=0x8000;
	while((udacsr->udasa&MP_STEP2) == 0);
	johan=(((u_int)ubauda)&0xffff)+8;
	udacsr->udasa=johan;
	while((udacsr->udasa&MP_STEP3) == 0);
	udacsr->udasa=3;
	while((udacsr->udasa&MP_STEP4) == 0);
	udacsr->udasa=0x0001;

	uda.uda_ca.ca_rspdsc=(int)&ubauda->uda_rsp.mscp_cmdref;
	uda.uda_ca.ca_cmddsc=(int)&ubauda->uda_cmd.mscp_cmdref;
	uda.uda_cmd.mscp_un.un_seq.seq_addr = (long *)&uda.uda_ca.ca_cmddsc;
	uda.uda_rsp.mscp_un.un_seq.seq_addr = (long *)&uda.uda_ca.ca_rspdsc;
	uda.uda_cmd.mscp_vcid = 1;
	uda.uda_cmd.mscp_un.un_sccc.sccc_ctlrflags = 0;

	command(M_OP_SETCTLRC, 0);
	uda.uda_cmd.mscp_unit=ra->unit;
	command(M_OP_ONLINE, 0);

	if (part) {
		uda.uda_cmd.mscp_un.un_seq.seq_buffer = part;
		command(M_OP_POS, 0);
		uda.uda_cmd.mscp_un.un_seq.seq_buffer = 0;
	}

	f->f_devdata=(void *)ra;
	return(0);
}

static
command(cmd, arg)
{
	volatile int hej;

	uda.uda_cmd.mscp_opcode = cmd;
	uda.uda_cmd.mscp_modifier = arg;

	uda.uda_cmd.mscp_msglen = MSCP_MSGLEN;
	uda.uda_rsp.mscp_msglen = MSCP_MSGLEN;

	uda.uda_ca.ca_rspdsc |= MSCP_OWN|MSCP_INT;
	uda.uda_ca.ca_cmddsc |= MSCP_OWN|MSCP_INT;
	hej = udacsr->udaip;
	while (uda.uda_ca.ca_rspdsc < 0) {
		if (uda.uda_ca.ca_cmdint)
			uda.uda_ca.ca_cmdint = 0;
	}

}

tmscpstrategy(ra, func, dblk, size, buf, rsize)
	struct ra_softc *ra;
	int func;
	daddr32_t	dblk;
	char *buf;
	u_int size, *rsize;
{
	u_int i,j,pfnum, mapnr, nsize, bn, cn, sn, tn;
	volatile struct udadevice *udadev=(void*)ra->udaddr;
	volatile u_int *ptmapp = (u_int *)ra->ubaddr + 512;
	volatile int hej;

	pfnum=(u_int)buf>>VAX_PGSHIFT;

	for(mapnr=0, nsize=size;(nsize+VAX_NBPG)>0;nsize-=VAX_NBPG)
		ptmapp[mapnr++]=PG_V|pfnum++;

	/*
	 * First position tape. Remember where we are.
	 */
	if (dblk < curblock) {
		uda.uda_cmd.mscp_seq.seq_bytecount = curblock - dblk;
		command(M_OP_POS, 12); /* 12 == step block backward */
	} else {
		uda.uda_cmd.mscp_seq.seq_bytecount = dblk - curblock;
		command(M_OP_POS, 4); /* 4 == step block forward */
	}
	curblock = size/512 + dblk;

	/*
	 * Read in the number of blocks we need.
	 * Why doesn't read of multiple blocks work?????
	 */
	for (i = 0 ; i < size/512 ; i++) {
		uda.uda_cmd.mscp_seq.seq_lbn = 1;
		uda.uda_cmd.mscp_seq.seq_bytecount = 512;
		uda.uda_cmd.mscp_seq.seq_buffer =
		    (((u_int)buf) & 0x1ff) + i * 512;
		uda.uda_cmd.mscp_unit = ra->unit;
		command(M_OP_READ, 0);
	}

	*rsize=size;
	return 0;
}

/*	$NetBSD: ra.c,v 1.4 1996/02/17 18:23:23 ragge Exp $ */
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
#include "../include/macros.h"
#include "../uba/ubareg.h"
#include "../uba/udareg.h"
#include "../vax/mscp.h"

#include "vaxstand.h"

static command(int);

/*
 * These routines for RA disk standalone boot is wery simple,
 * assuming a lots of thing like that we only working at one ra disk
 * a time, no separate routines for uba driver etc..
 * This code is foolish and should need a cleanup.
 * But it works :)
 */

struct ra_softc {
	int udaddr;
	int ubaddr;
	int part;
	int unit;
};

volatile struct uda {
        struct  uda1ca uda_ca;           /* communications area */
        struct  mscp uda_rsp;     /* response packets */
        struct  mscp uda_cmd;     /* command packets */
} uda;

volatile struct uda *ubauda;
volatile struct udadevice *udacsr;
struct	disklabel ralabel;
struct ra_softc ra_softc;
char io_buf[MAXBSIZE];

raopen(f, adapt, ctlr, unit, part)
	struct open_file *f;
        int ctlr, unit, part;
{
	char *msg;
	struct disklabel *lp=&ralabel;
	volatile struct ra_softc *ra=&ra_softc;
	volatile struct uba_regs *mr=(void *)ubaaddr[adapt];
	volatile u_int *nisse;
	unsigned short johan;
	int i,err;

	if(adapt>nuba) return(EADAPT);
	if(ctlr>nuda) return(ECTLR);
	bzero(lp, sizeof(struct disklabel));
	ra->udaddr=uioaddr[adapt]+udaaddr[ctlr];
	ra->ubaddr=(int)mr;
	ra->unit=unit;
	ra->part = part;
	udacsr=(void*)ra->udaddr;
	nisse=(u_int *)&mr->uba_map[0];
	nisse[494]=PG_V|(((u_int)&uda)>>9);
	nisse[495]=nisse[494]+1;
	ubauda=(void*)0x3dc00+(((u_int)(&uda))&0x1ff);
	/* Init of this uda */
	udacsr->udaip=0; /* Start init */
	while((udacsr->udasa&UDA_STEP1) == 0);
	udacsr->udasa=0x8000;
	while((udacsr->udasa&UDA_STEP2) == 0);
	johan=(((u_int)ubauda)&0xffff)+8;
	udacsr->udasa=johan;
	while((udacsr->udasa&UDA_STEP3) == 0);
	udacsr->udasa=3;
	while((udacsr->udasa&UDA_STEP4) == 0);
	udacsr->udasa=0x0001;
	uda.uda_ca.ca_rspdsc=(int)&ubauda->uda_rsp.mscp_cmdref;
	uda.uda_ca.ca_cmddsc=(int)&ubauda->uda_cmd.mscp_cmdref;
	command(M_OP_SETCTLRC);
	uda.uda_cmd.mscp_unit=ra->unit;
	command(M_OP_ONLINE);

	err=rastrategy(ra,F_READ, LABELSECTOR, DEV_BSIZE, io_buf, &i);
	if(err){
		printf("reading disklabel: %s\n",strerror(err));
		return 0;
	}

	msg=getdisklabel(io_buf+LABELOFFSET, lp);
	if(msg) {
		printf("getdisklabel: %s\n",msg);
	}
	f->f_devdata=(void *)ra;
	return(0);
}

static
command(cmd)
{
	volatile int hej;

	uda.uda_cmd.mscp_opcode=cmd;
	uda.uda_cmd.mscp_msglen=MSCP_MSGLEN;
	uda.uda_rsp.mscp_msglen=MSCP_MSGLEN;
	uda.uda_ca.ca_rspdsc |= MSCP_OWN|MSCP_INT;
	uda.uda_ca.ca_cmddsc |= MSCP_OWN|MSCP_INT;
	hej=udacsr->udaip;
	while(uda.uda_ca.ca_rspdsc<0);

}

rastrategy(ra, func, dblk, size, buf, rsize)
	struct	ra_softc *ra;
	int	func;
	daddr_t	dblk;
	char	*buf;
	u_int	size, *rsize;
{
	volatile struct uba_regs *ur;
	volatile struct udadevice *udadev;
	volatile u_int *ptmapp;
	struct	disklabel *lp;
	u_int	i, j, pfnum, mapnr, nsize;
	volatile int hej;


	ur = (void *)ra->ubaddr;
	udadev = (void*)ra->udaddr;
	ptmapp = (u_int *)&ur->uba_map[0];
	lp = &ralabel;

	pfnum = (u_int)buf >> PGSHIFT;

	for(mapnr = 0, nsize = size; (nsize + NBPG) > 0; nsize -= NBPG)
		ptmapp[mapnr++] = PG_V | pfnum++;

	uda.uda_cmd.mscp_seq.seq_lbn =
	    dblk + lp->d_partitions[ra->part].p_offset;
	uda.uda_cmd.mscp_seq.seq_bytecount = size;
	uda.uda_cmd.mscp_seq.seq_buffer = ((u_int)buf) & 0x1ff;
	uda.uda_cmd.mscp_unit = ra->unit;
	if (func == F_WRITE)
		command(M_OP_WRITE);
	else
		command(M_OP_READ);

	*rsize = size;
	return 0;
}

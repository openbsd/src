/*	$OpenBSD: ra.c,v 1.2 2002/03/14 03:16:02 millert Exp $ */
/*	$NetBSD: ra.c,v 1.4 1999/08/07 11:19:04 ragge Exp $ */
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
		
#define NRSP 1 /* Kludge */
#define NCMD 1 /* Kludge */

#include "sys/param.h"
#include "sys/disklabel.h"

#include "lib/libsa/stand.h"

#include "../include/pte.h"
#include "../include/sid.h"

#include "arch/vax/mscp/mscp.h"
#include "arch/vax/mscp/mscpreg.h"

#include "arch/vax/bi/bireg.h"
#include "arch/vax/bi/kdbreg.h"

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
	unsigned short *ra_ip;
	unsigned short *ra_sa;
	unsigned short *ra_sw;
};

volatile struct uda {
        struct  mscp_1ca uda_ca;           /* communications area */
        struct  mscp uda_rsp;     /* response packets */
        struct  mscp uda_cmd;     /* command packets */
} uda;

volatile struct uda *ubauda;
struct	disklabel ralabel;
struct ra_softc ra_softc;
char io_buf[DEV_BSIZE];

raopen(f, adapt, ctlr, unit, part)
	struct open_file *f;
        int ctlr, unit, part;
{
	char *msg;
	struct disklabel *lp = &ralabel;
	volatile struct ra_softc *ra = &ra_softc;
	volatile u_int *nisse;
	unsigned short johan, johan2;
	int i,err, udacsr;

#ifdef DEV_DEBUG
	printf("raopen: adapter %d ctlr %d unit %d part %d\n", 
	    adapt, ctlr, unit, part);
#endif
	bzero(lp, sizeof(struct disklabel));
	ra->unit = unit;
	ra->part = part;
	if (vax_cputype != VAX_8200) {
		if (adapt > nuba)
			return(EADAPT);
		if (ctlr > nuda)
			return(ECTLR);
		nisse = ((u_int *)ubaaddr[adapt]) + 512;
		nisse[494] = PG_V | (((u_int)&uda) >> 9);
		nisse[495] = nisse[494] + 1;
		udacsr = (int)uioaddr[adapt] + udaaddr[ctlr];
		ubauda = (void *)0x3dc00 + (((u_int)(&uda))&0x1ff);
		johan = (((u_int)ubauda) & 0xffff) + 8;
		johan2 = 3;
		ra->ra_ip = (short *)udacsr;
		ra->ra_sa = ra->ra_sw = (short *)udacsr + 1;
		ra->udaddr = uioaddr[adapt] + udaaddr[ctlr];
		ra->ubaddr = (int)ubaaddr[adapt];
		*ra->ra_ip = 0; /* Start init */
	} else {
		paddr_t kdaddr = (paddr_t)biaddr[adapt] + BI_NODE(ctlr);
		volatile int *w;
		volatile int i = 10000;

		ra->ra_ip = (short *)(kdaddr + KDB_IP);
		ra->ra_sa = (short *)(kdaddr + KDB_SA);
		ra->ra_sw = (short *)(kdaddr + KDB_SW);
		johan = ((u_int)&uda.uda_ca.ca_rspdsc) & 0xffff;
		johan2 = (((u_int)&uda.uda_ca.ca_rspdsc) & 0xffff0000) >> 16;
		w = (int *)(kdaddr + BIREG_VAXBICSR);
		*w = *w | BICSR_NRST;
		while (i--) /* Need delay??? */
			;
		w = (int *)(kdaddr + BIREG_BER);
		*w = ~(BIBER_MBZ|BIBER_NMR|BIBER_UPEN);/* ??? */
		ubauda = &uda;
	}

	/* Init of this uda */
	while ((*ra->ra_sa & MP_STEP1) == 0)
		;
#ifdef DEV_DEBUG
	printf("MP_STEP1...");
#endif
	*ra->ra_sw = 0x8000;
	while ((*ra->ra_sa & MP_STEP2) == 0)
		;
#ifdef DEV_DEBUG
	printf("MP_STEP2...");
#endif

	*ra->ra_sw = johan;
	while ((*ra->ra_sa & MP_STEP3) == 0)
		;
#ifdef DEV_DEBUG
	printf("MP_STEP3...");
#endif

	*ra->ra_sw = johan2;
	while ((*ra->ra_sa & MP_STEP4) == 0)
		;
#ifdef DEV_DEBUG
	printf("MP_STEP4\n");
#endif

	*ra->ra_sw = 0x0001;
	uda.uda_ca.ca_rspdsc = (int)&ubauda->uda_rsp.mscp_cmdref;
	uda.uda_ca.ca_cmddsc = (int)&ubauda->uda_cmd.mscp_cmdref;

	command(M_OP_SETCTLRC);
	uda.uda_cmd.mscp_unit = ra->unit;
	command(M_OP_ONLINE);

#ifdef DEV_DEBUG
	printf("reading disklabel\n");
#endif
	err = rastrategy(ra,F_READ, LABELSECTOR, DEV_BSIZE, io_buf, &i);
	if(err){
		printf("reading disklabel: %s\n",strerror(err));
		return 0;
	}

#ifdef DEV_DEBUG
	printf("getting disklabel\n");
#endif
	msg = getdisklabel(io_buf+LABELOFFSET, lp);
	if (msg)
		printf("getdisklabel: %s\n", msg);
	f->f_devdata = (void *)ra;
	return(0);
}

static
command(cmd)
{
	volatile int hej;

	uda.uda_cmd.mscp_opcode = cmd;
	uda.uda_cmd.mscp_msglen = MSCP_MSGLEN;
	uda.uda_rsp.mscp_msglen = MSCP_MSGLEN;
	uda.uda_ca.ca_rspdsc |= MSCP_OWN|MSCP_INT;
	uda.uda_ca.ca_cmddsc |= MSCP_OWN|MSCP_INT;
#ifdef DEV_DEBUG
	printf("sending cmd %x...", cmd);
#endif
	hej = *ra_softc.ra_ip;
	while(uda.uda_ca.ca_rspdsc<0)
		;
#ifdef DEV_DEBUG
	printf("sent.\n");
#endif
}

rastrategy(ra, func, dblk, size, buf, rsize)
	struct	ra_softc *ra;
	int	func;
	daddr_t	dblk;
	char	*buf;
	u_int	size, *rsize;
{
	volatile u_int *ptmapp;
	struct	disklabel *lp;
	u_int	i, j, pfnum, mapnr, nsize;
	volatile int hej;

	if (vax_cputype != VAX_8200) {
		ptmapp = ((u_int *)ra->ubaddr) + 512;

		pfnum = (u_int)buf >> VAX_PGSHIFT;

		for(mapnr = 0, nsize = size; (nsize + VAX_NBPG) > 0;
		    nsize -= VAX_NBPG)
			ptmapp[mapnr++] = PG_V | pfnum++;
		uda.uda_cmd.mscp_seq.seq_buffer = ((u_int)buf) & 0x1ff;
	} else
		uda.uda_cmd.mscp_seq.seq_buffer = ((u_int)buf);

	lp = &ralabel;
	uda.uda_cmd.mscp_seq.seq_lbn =
	    dblk + lp->d_partitions[ra->part].p_offset;
	uda.uda_cmd.mscp_seq.seq_bytecount = size;
	uda.uda_cmd.mscp_unit = ra->unit;
#ifdef DEV_DEBUG
	printf("rastrategy: blk 0x%lx count %lx unit %lx\n", 
	    uda.uda_cmd.mscp_seq.seq_lbn, size, ra->unit);
#endif
	if (func == F_WRITE)
		command(M_OP_WRITE);
	else
		command(M_OP_READ);

	*rsize = size;
	return 0;
}

/*	$OpenBSD: ra.c,v 1.3 2002/06/11 09:36:23 hugh Exp $ */
/*	$NetBSD: ra.c,v 1.11 2002/06/04 15:13:55 ragge Exp $ */
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
#include "../include/rpb.h"

#include "arch/vax/mscp/mscp.h"
#include "arch/vax/mscp/mscpreg.h"

#include "arch/vax/bi/bireg.h"
#include "arch/vax/bi/kdbreg.h"

#include "vaxstand.h"

static void command(int, int);

/*
 * These routines for RA disk standalone boot is wery simple,
 * assuming a lots of thing like that we only working at one ra disk
 * a time, no separate routines for uba driver etc..
 * This code is foolish and should need a cleanup.
 * But it works :)
 */

static volatile struct uda {
	struct	mscp_1ca uda_ca;  /* communications area */
	struct	mscp uda_rsp;	  /* response packets */
	struct	mscp uda_cmd;	  /* command packets */
} uda;

static struct disklabel ralabel;
static char io_buf[DEV_BSIZE];
static int dpart, dunit, remap, is_tmscp, curblock;
static volatile u_short *ra_ip, *ra_sa, *ra_sw;
static volatile u_int *mapregs;

int
raopen(struct open_file *f, int adapt, int ctlr, int unit, int part)
{
	static volatile struct uda *ubauda;
	unsigned short johan, johan2;
	size_t i;
	int err;
	char *msg;

#ifdef DEV_DEBUG
	printf("raopen: adapter %d ctlr %d unit %d part %d\n", 
	    adapt, ctlr, unit, part);
	printf("raopen: csrbase %x nexaddr %x\n", csrbase, nexaddr);
#endif
	bzero(&ralabel, sizeof(struct disklabel));
	bzero((void *)&uda, sizeof(struct uda));
	if (bootrpb.devtyp == BDEV_TK)
		is_tmscp = 1;
	dunit = unit;
	dpart = part;
	if (ctlr < 0)
		ctlr = 0;
	remap = csrbase && nexaddr;
	curblock = 0;
	if (csrbase) { /* On a uda-alike adapter */
		if (askname == 0) {
			csrbase = bootrpb.csrphy;
			dunit = bootrpb.unit;
			nexaddr = bootrpb.adpphy;
		} else
			csrbase += (ctlr ? 000334 : 012150);
		ra_ip = (short *)csrbase;
		ra_sa = ra_sw = (short *)csrbase + 1;
		if (nexaddr) { /* have map registers */
			mapregs = (int *)nexaddr + 512;
			mapregs[494] = PG_V | (((u_int)&uda) >> 9);
			mapregs[495] = mapregs[494] + 1;
			(char *)ubauda = (char *)0x3dc00 +
			    (((u_int)(&uda))&0x1ff);
		} else
			ubauda = &uda;
		johan = (((u_int)ubauda) & 0xffff) + 8;
		johan2 = (((u_int)ubauda) >> 16) & 077;
		*ra_ip = 0; /* Start init */
		bootrpb.csrphy = csrbase;
	} else {
		paddr_t kdaddr;
		volatile int *w;
		volatile int i = 10000;

		if (askname == 0) {
			nexaddr = bootrpb.csrphy;
			dunit = bootrpb.unit;
		} else {
			nexaddr = (bootrpb.csrphy & ~(BI_NODESIZE - 1)) + KDB_IP;
			bootrpb.csrphy = nexaddr;
		}

		kdaddr = nexaddr & ~(BI_NODESIZE - 1);
		ra_ip = (short *)(kdaddr + KDB_IP);
		ra_sa = (short *)(kdaddr + KDB_SA);
		ra_sw = (short *)(kdaddr + KDB_SW);
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

#ifdef DEV_DEBUG
	printf("start init\n");
#endif
	/* Init of this uda */
	while ((*ra_sa & MP_STEP1) == 0)
		;
#ifdef DEV_DEBUG
	printf("MP_STEP1...");
#endif
	*ra_sw = 0x8000;
	while ((*ra_sa & MP_STEP2) == 0)
		;
#ifdef DEV_DEBUG
	printf("MP_STEP2...");
#endif

	*ra_sw = johan;
	while ((*ra_sa & MP_STEP3) == 0)
		;
#ifdef DEV_DEBUG
	printf("MP_STEP3...");
#endif

	*ra_sw = johan2;
	while ((*ra_sa & MP_STEP4) == 0)
		;
#ifdef DEV_DEBUG
	printf("MP_STEP4\n");
#endif

	*ra_sw = 0x0001;
	uda.uda_ca.ca_rspdsc = (int)&ubauda->uda_rsp.mscp_cmdref;
	uda.uda_ca.ca_cmddsc = (int)&ubauda->uda_cmd.mscp_cmdref;
	if (is_tmscp) {
		uda.uda_cmd.mscp_un.un_seq.seq_addr =
		    (long *)&uda.uda_ca.ca_cmddsc;
		uda.uda_rsp.mscp_un.un_seq.seq_addr =
		    (long *)&uda.uda_ca.ca_rspdsc;
		uda.uda_cmd.mscp_vcid = 1;
		uda.uda_cmd.mscp_un.un_sccc.sccc_ctlrflags = 0;
	}

	command(M_OP_SETCTLRC, 0);
	uda.uda_cmd.mscp_unit = dunit;
	command(M_OP_ONLINE, 0);

	if (is_tmscp) {
		if (part) {
#ifdef DEV_DEBUG
			printf("Repos of tape...");
#endif
			uda.uda_cmd.mscp_un.un_seq.seq_buffer = part;
			command(M_OP_POS, 0);
			uda.uda_cmd.mscp_un.un_seq.seq_buffer = 0;
#ifdef DEV_DEBUG
			printf("Done!\n");
#endif
		}
		return 0;
	}
#ifdef DEV_DEBUG
	printf("reading disklabel\n");
#endif
	err = rastrategy(0, F_READ, LABELSECTOR, DEV_BSIZE, io_buf, &i);
	if(err){
		printf("reading disklabel: %s\n",strerror(err));
		return 0;
	}

#ifdef DEV_DEBUG
	printf("getting disklabel\n");
#endif
	msg = getdisklabel(io_buf+LABELOFFSET, &ralabel);
	if (msg)
		printf("getdisklabel: %s\n", msg);
	return(0);
}

static void
command(int cmd, int arg)
{
	volatile short hej;
	int to;

igen:	uda.uda_cmd.mscp_opcode = cmd;
	uda.uda_cmd.mscp_modifier = arg;

	uda.uda_cmd.mscp_msglen = MSCP_MSGLEN;
	uda.uda_rsp.mscp_msglen = MSCP_MSGLEN;
	uda.uda_ca.ca_rspdsc |= MSCP_OWN|MSCP_INT;
	uda.uda_ca.ca_cmddsc |= MSCP_OWN|MSCP_INT;
#ifdef DEV_DEBUG
	printf("sending cmd %x...", cmd);
#endif
	hej = *ra_ip;
	to = 10000000;
	while (uda.uda_ca.ca_rspdsc < 0) {
//		if (uda.uda_ca.ca_cmdint)
//			uda.uda_ca.ca_cmdint = 0;
		if (--to < 0) {
#ifdef DEV_DEBUG
			printf("timing out, retry\n");
#endif
			goto igen;
		}
	}
#ifdef DEV_DEBUG
	printf("sent.\n");
#endif
}

int
rastrategy(void *f, int func, daddr_t dblk,
    size_t size, void *buf, size_t *rsize)
{
	u_int	pfnum, mapnr, nsize;

#ifdef DEV_DEBUG
	printf("rastrategy: buf %p remap %d is_tmscp %d\n",
	    buf, remap, is_tmscp);
#endif
	if (remap) {
		pfnum = (u_int)buf >> VAX_PGSHIFT;

		for(mapnr = 0, nsize = size; (nsize + VAX_NBPG) > 0;
		    nsize -= VAX_NBPG)
			mapregs[mapnr++] = PG_V | pfnum++;
		uda.uda_cmd.mscp_seq.seq_buffer = ((u_int)buf) & 0x1ff;
	} else
		uda.uda_cmd.mscp_seq.seq_buffer = ((u_int)buf);

	if (is_tmscp) {
		int i;

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
			uda.uda_cmd.mscp_unit = dunit;
			command(M_OP_READ, 0);
		}
	} else {

		uda.uda_cmd.mscp_seq.seq_lbn =
		    dblk + ralabel.d_partitions[dpart].p_offset;
		uda.uda_cmd.mscp_seq.seq_bytecount = size;
		uda.uda_cmd.mscp_unit = dunit;
#ifdef DEV_DEBUG
		printf("rastrategy: blk 0x%lx count %lx unit %x\n", 
		    uda.uda_cmd.mscp_seq.seq_lbn, size, dunit);
#endif
#ifdef notdef
		if (func == F_WRITE)
			command(M_OP_WRITE, 0);
		else
#endif
			command(M_OP_READ, 0);
	}

	*rsize = size;
	return 0;
}

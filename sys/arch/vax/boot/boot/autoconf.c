/*	$OpenBSD: autoconf.c,v 1.6 2001/10/31 17:20:21 hugh Exp $ */
/*	$NetBSD: autoconf.c,v 1.5 1999/08/23 19:09:27 ragge Exp $ */
/*
 * Copyright (c) 1994, 1998 Ludd, University of Lule}, Sweden.
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
#include "../../include/mtpr.h"
#include "../../include/sid.h"
#include "../../include/scb.h"
#include "vaxstand.h"

extern  const struct ivec_dsp idsptch;	/* since we are not KERNEL */

int	nmba=0, nuba=0, nbi=0,nsbi=0,nuda=0;
int	*mbaaddr, *ubaaddr, *biaddr;
int	*udaaddr, *uioaddr, tmsaddr, *bioaddr;

static int mba750[]={0xf28000,0xf2a000,0xf2c000};
static int uba750[]={0xf30000,0xf32000};
static int uio750[]={0xfc0000,0xf80000};
static int uda750[]={0772150};

/* 11/780's only have 4, 8600 have 8 of these. */
/* XXX - all of these should be bound to physical addresses */
static int mba780[]={0x20010000,0x20012000,0x20014000,0x20016000,
	0x22010000,0x22012000,0x22014000,0x22016000};
static int uba780[]={0, 0, 0, 0x20006000,0x20008000,0x2000a000,0x2000c000, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 
		0, 0, 0, 0x22006000,0x22008000,0x2200a000,0x2200c000};
static int uio780[]={0, 0, 0, 0x20100000,0x20140000,0x20180000,0x201c0000, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 
		0, 0, 0, 0x22100000,0x22140000,0x22180000,0x221c0000};
static int bi8200[]={0x20000000, 0x22000000, 0x24000000, 0x26000000,
	0x28000000, 0x2a000000};
static int bio8200[]={0x20400000};

static int uba630[]={0x20087800};
static int uio630[]={0x30000000};
#define qbdev(csr) (((csr) & 017777)-0x10000000)
static int uda630[]={qbdev(0772150),qbdev(0760334)};

static int uba670[]={0x20040000};
static int uio670[]={0x20000000};
static int uda670[]={0x20004030,0x20004230};
#define qb670dev(csr) (((csr) & 017777)+0x20000000)

/*
 * Autoconf routine is really stupid; but it actually don't
 * need any intelligence. We just assume that all possible
 * devices exists on each cpu. Fast & easy.
 */

autoconf()
{
	extern int memsz;
	int copyrpb = 1;

	findcpu(); /* Configures CPU variables */
	consinit(); /* Allow us to print out things */
	scbinit(); /* Fix interval clock etc */

	switch (vax_boardtype) {

	default:
		printf("\nCPU type %d not supported by boot\n",vax_cputype);
		printf("trying anyway...\n");
		break;

	case VAX_BTYP_780:
	case VAX_BTYP_790:
		memsz = 0;
		nmba = 8;
		nuba = 32; /* XXX */
		nuda = 1;
		mbaaddr = mba780;
		ubaaddr = uba780;
		udaaddr = uda750;
		uioaddr = uio780;
		tmsaddr = 0774500;
		break;

	case VAX_BTYP_750:
		memsz = 0;
		nmba = 3;
		nuba = 2;
		nuda = 1;
		mbaaddr = mba750;
		ubaaddr = uba750;
		udaaddr = uda750;
		uioaddr = uio750;
		tmsaddr = 0774500;
		break;

	case VAX_BTYP_630:	/* the same for uvaxIII */
	case VAX_BTYP_650:
	case VAX_BTYP_660:
	case VAX_BTYP_670:
		nuba = 1;
		nuda = 2;
		ubaaddr = uba630;
		udaaddr = uda630;
		uioaddr = uio630;
		tmsaddr = qbdev(0774500);
		break;

	case VAX_BTYP_8000:
		copyrpb = 0;
		memsz = 0;
		nbi = 1;
		biaddr = bi8200;
		bioaddr = bio8200;
		break;

	case VAX_BTYP_46:
	case VAX_BTYP_48:
		{int *map, i;

		/* Map all 16MB of I/O space to low 16MB of memory */
		map = (int *)0x700000; /* XXX */
		*(int *)0x20080008 = (int)map; /* XXX */
		for (i = 0; i < 0x8000; i++)
			map[i] = 0x80000000 | i;
		}break;

	case VAX_BTYP_410:
	case VAX_BTYP_420:
	case VAX_BTYP_43:
	case VAX_BTYP_49:
	case VAX_BTYP_1301:
	case VAX_BTYP_1303:
		break;
	}
}

/*
 * Clock handling routines, needed to do timing in standalone programs.
 */

volatile int tickcnt;

getsecs()
{
	volatile int loop;
	int todr;

	return tickcnt/100;
}

void scb_stray(), rtimer();
struct ivec_dsp **scb;
struct ivec_dsp *scb_vec;

/*
 * Init the SCB and set up a handler for all vectors in the lower space,
 * to detect unwanted interrupts.
 */
scbinit()
{
	extern int timer;
	int i;

	/*
	 * Allocate space. We need one page for the SCB, and 128*20 == 2.5k
	 * for the vectors. The SCB must be on a page boundary.
	 */
	i = (int)alloc(VAX_NBPG + 128*sizeof(scb_vec[0])) + VAX_PGOFSET;
	i &= ~VAX_PGOFSET;

	mtpr(i, PR_SCBB);
	scb = (void *)i;
	scb_vec = (struct ivec_dsp *)(i + VAX_NBPG);

	for (i = 0; i < 128; i++) {
		scb[i] = &scb_vec[i];
		(int)scb[i] |= SCB_ISTACK;	/* Only interrupt stack */
		scb_vec[i] = idsptch;
		scb_vec[i].hoppaddr = scb_stray;
		scb_vec[i].pushlarg = (void *) (i * 4);
		scb_vec[i].ev = NULL;
	}
	scb_vec[0xc0/4].hoppaddr = rtimer;

	mtpr(-10000, PR_NICR);		/* Load in count register */
	mtpr(0x800000d1, PR_ICCS);	/* Start clock and enable interrupt */

	mtpr(20, PR_IPL);
}

extern int jbuf[10];
extern int sluttid, senast, skip;

void
rtimer()
{
	mtpr(31, PR_IPL);
	tickcnt++;
	mtpr(0xc1, PR_ICCS);
	if (skip)
		return;
	if ((vax_boardtype == VAX_BTYP_46) ||
		(vax_boardtype == VAX_BTYP_48) ||
		(vax_boardtype == VAX_BTYP_49)) {
		int nu = sluttid - getsecs();
		if (senast != nu) {
			mtpr(20, PR_IPL);
			longjmp(jbuf);
		}
	}
}

asm("
	.align	2
	.globl  _idsptch, _eidsptch
_idsptch:
	pushr	$0x3f
	.word	0x9f16
	.long	_cmn_idsptch
	.long	0
	.long	0
	.long	0
_eidsptch:

_cmn_idsptch:
	movl	(sp)+,r0
	pushl	4(r0)
	calls	$1,*(r0)
	popr	$0x3f
	rei
");

/*
 * Stray interrupt handler.
 * This function must _not_ save any registers (in the reg save mask).
 */
void
scb_stray(arg)
	int arg;
{
	static int vector, ipl;

	ipl = mfpr(PR_IPL);
	vector = (int) arg;
	printf("stray interrupt: vector 0x%x, ipl %d\n", vector, ipl);
}


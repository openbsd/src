/*	$OpenBSD: ka410.c,v 1.13 2011/09/15 00:48:24 miod Exp $ */
/*	$NetBSD: ka410.c,v 1.21 1999/09/06 19:52:53 ragge Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by Bertram Barth.
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
 *	This product includes software developed at Ludd, University of 
 *	Lule}, Sweden and its contributors.
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/pmap.h>
#include <machine/uvax.h>
#include <machine/ka410.h>
#include <machine/ka420.h>
#include <machine/cvax.h>
#include <machine/clock.h>
#include <machine/vsbus.h>

static	void	ka410_conf(void);
static	void	ka410_memerr(void);
static	int	ka410_mchk(caddr_t);
static	void	ka410_halt(void);
static	void	ka410_reboot(int);
static	void	ka41_cache_enable(void);
static	void	ka410_clrf(void);

static	caddr_t	l2cache;	/* mapped in address */
static	long 	*cacr;		/* l2csche ctlr reg */

/* 
 * Declaration of 410-specific calls.
 */
struct	cpu_dep ka410_calls = {
	0,
	ka410_mchk,
	ka410_memerr, 
	ka410_conf,
	chip_clkread,
	chip_clkwrite,
	1,      /* ~VUPS */
	2,	/* SCB pages */
	ka410_halt,
	ka410_reboot,
	ka410_clrf,
	icr_hardclock
};


void
ka410_conf()
{
	struct vs_cpu *ka410_cpu;

	ka410_cpu = (struct vs_cpu *)vax_map_physmem(VS_REGS, 1);

	switch (vax_cputype) {
	case VAX_TYP_UV2:
		ka410_cpu->vc_410mser = 1;
		printf("cpu: KA410\n");
		break;

	case VAX_TYP_CVAX:
		printf("cpu: KA41/42\n");
		ka410_cpu->vc_vdcorg = 0; /* XXX */
		ka410_cpu->vc_parctl = PARCTL_CPEN | PARCTL_DPEN ;
		printf("cpu: Enabling primary cache, ");
		mtpr(CADR_SEN2 | CADR_SEN1 | CADR_CENI | CADR_CEND, PR_CADR);
		if (vax_confdata & KA420_CFG_CACHPR) {
			l2cache = (void *)vax_map_physmem(KA420_CH2_BASE,
			    (KA420_CH2_SIZE / VAX_NBPG));
			cacr = (void *)vax_map_physmem(KA420_CACR, 1);
			printf("secondary cache\n");
			ka41_cache_enable();
		} else
			printf("no secondary cache present\n");
	}
	/* Done with ka410_cpu - release it */
	vax_unmap_physmem((vaddr_t)ka410_cpu, 1);
	/*
	 * Setup parameters necessary to read time from clock chip.
	 */
	clk_adrshift = 1;       /* Addressed at long's... */
	clk_tweak = 2;          /* ...and shift two */
	clk_page = (short *)vax_map_physmem(KA420_WAT_BASE, 1);
}

void
ka41_cache_enable()
{
	*cacr = KA420_CACR_TPE; 	/* Clear any error, disable cache */
	bzero(l2cache, KA420_CH2_SIZE); /* Clear whole cache */
	*cacr = KA420_CACR_CEN;		/* Enable cache */
}

void
ka410_memerr()
{
	printf("Memory err!\n");
}

int
ka410_mchk(addr)
	caddr_t addr;
{
	panic("Machine check");
	return 0;
}

static void
ka410_halt()
{
	asm("movl $0xc, (%0)"::"r"((int)clk_page + 0x38)); /* Don't ask */
	asm("halt");
}

static void
ka410_reboot(arg)
	int arg;
{
	asm("movl $0xc, (%0)"::"r"((int)clk_page + 0x38)); /* Don't ask */
	asm("halt");
}

static void
ka410_clrf()
{
	struct ka410_clock *clk = (void *)clk_page;

	/*
	 * Clear restart and boot in progress flags
	 * in the CPMBX. (ie. clear bits 4 and 5)
	 */
	clk->cpmbx = (clk->cpmbx & ~0x30);
}

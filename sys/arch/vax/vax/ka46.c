/*	$OpenBSD: ka46.c,v 1.9 2008/08/18 23:05:38 miod Exp $	*/
/*	$NetBSD: ka46.c,v 1.12 2000/03/04 07:27:49 matt Exp $ */
/*
 * Copyright (c) 1998 Ludd, University of Lule}, Sweden.
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
#include <machine/nexus.h>
#include <machine/uvax.h>
#include <machine/ka410.h>
#include <machine/ka420.h>
#include <machine/ka46.h>
#include <machine/clock.h>
#include <machine/vsbus.h>

static	void	ka46_conf(void);
static	void	ka46_init(void);
static	void	ka46_memerr(void);
static	int	ka46_mchk(caddr_t);
static	void	ka46_halt(void);
static	void	ka46_reboot(int);
static	void	ka46_cache_enable(void);
static	void	ka46_hardclock(struct clockframe *);

struct	vs_cpu *ka46_cpu;

/* 
 * Declaration of 46-specific calls.
 */
struct	cpu_dep ka46_calls = {
	ka46_init,
	ka46_mchk,
	ka46_memerr, 
	ka46_conf,
	chip_clkread,
	chip_clkwrite,
	12,      /* ~VUPS */
	2,	/* SCB pages */
	ka46_halt,
	ka46_reboot,
	NULL,
	NULL,
	ka46_hardclock
};


void
ka46_conf()
{
        switch(vax_siedata & 0xFF) {
	case VAX_VTYP_47:
		printf("cpu: KA47\n");
		break;
	case VAX_VTYP_46:
		printf("cpu: KA46\n");
		break;
	default:
		printf("cpu: Unknown Mariah\n");
	}

	ka46_cpu = (void *)vax_map_physmem(VS_REGS, 1);
	printf("cpu: turning on floating point chip\n");
	mtpr(2, PR_ACCS); /* Enable floating points */
	/*
	 * Setup parameters necessary to read time from clock chip.
	 */
	clk_adrshift = 1;       /* Addressed at long's... */
	clk_tweak = 2;          /* ...and shift two */
	clk_page = (short *)vax_map_physmem(VS_CLOCK, 1);
}

void
ka46_cache_enable()
{
	int i, *tmp;

	/* Disable caches */
	*(int *)KA46_CCR &= ~CCR_SPECIO;/* secondary */
	mtpr(PCSTS_FLUSH, PR_PCSTS);	/* primary */
	*(int *)KA46_BWF0 &= ~BWF0_FEN; /* invalidate filter */

	/* Clear caches */
	tmp = (void *)KA46_INVFLT;	/* inv filter */
	for (i = 0; i < 32768; i++)
		tmp[i] = 0;

	/* Write valid parity to all primary cache entries */
	for (i = 0; i < 256; i++) {
		mtpr(i << 3, PR_PCIDX);
		mtpr(PCTAG_PARITY, PR_PCTAG);
	}

	/* Secondary cache */
	tmp = (void *)KA46_TAGST;
	for (i = 0; i < KA46_TAGSZ*2; i+=2)
		tmp[i] = 0;

	/* Enable cache */
	*(int *)KA46_BWF0 |= BWF0_FEN; /* invalidate filter */
	mtpr(PCSTS_ENABLE, PR_PCSTS);
	*(int *)KA46_CCR = CCR_SPECIO | CCR_CENA;
}

void
ka46_memerr()
{
	printf("Memory err!\n");
}

int
ka46_mchk(addr)
	caddr_t addr;
{
	panic("Machine check");
	return 0;
}

void
ka46_init()
{

	/* Turn on caches (to speed up execution a bit) */
	ka46_cache_enable();
}

#define	KA46_CPMBX	0x38
#define	KA46_HLT_HALT	0xcf
#define	KA46_HLT_BOOT	0x8b

static void
ka46_halt()
{
	if (((u_int8_t *) clk_page)[KA46_CPMBX] != KA46_HLT_HALT)
		((u_int8_t *) clk_page)[KA46_CPMBX] = KA46_HLT_HALT;
	asm("halt");
}

static void
ka46_reboot(arg)
	int arg;
{
	if (((u_int8_t *) clk_page)[KA46_CPMBX] != KA46_HLT_BOOT)
		((u_int8_t *) clk_page)[KA46_CPMBX] = KA46_HLT_BOOT;
	asm("halt");
}

static void
ka46_hardclock(struct clockframe *cf)
{
	ka46_cpu->vc_diagtimu = 0;
	hardclock(cf);
}

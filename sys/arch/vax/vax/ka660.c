/*	$OpenBSD: ka660.c,v 1.8 2011/09/15 00:48:24 miod Exp $	*/
/*	$NetBSD: ka660.c,v 1.3 2000/06/29 07:14:27 mrg Exp $	*/
/*
 * Copyright (c) 2000 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of 
 *     Lule}, Sweden and its contributors.
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
#include <machine/clock.h>
#include <machine/vsbus.h>

#define KA660_CCR	37	/* Cache Control Register */
#define KA660_CTAG	0x20150000	/* Cache Tags */
#define KA660_CDATA	0x20150400	/* Cache Data */
#define KA660_BEHR	0x20150800	/* Bank Enable/Hit Register */
#define CCR_WWP 8	/* Write Wrong Parity */
#define CCR_ENA 4	/* Cache Enable */
#define CCR_FLU 2	/* Cache Flush */
#define CCR_DIA 1	/* Diagnostic mode */

static void    ka660_conf(void);
static void    ka660_memerr(void);
static int     ka660_mchk(caddr_t);
static void    ka660_cache_enable(void);

/* 
 * Declaration of 660-specific calls.
 */
struct cpu_dep ka660_calls = {
	ka660_cache_enable,
	ka660_mchk,
	ka660_memerr, 
	ka660_conf,
	generic_clkread,
	generic_clkwrite,
	6,	/* ~VUPS */
	2,	/* SCB pages */
	generic_halt,
	generic_reboot,
	NULL,
	icr_hardclock
};


void
ka660_conf()
{
	printf("cpu0: KA660, microcode Rev. %d\n", vax_cpudata & 0377);

	cpmbx = (struct cpmbx *)vax_map_physmem(0x20140400, 1);
}

void
ka660_cache_enable()
{
	unsigned int *p;
	int cnt, bnk, behrtmp;

	mtpr(0, KA660_CCR);	/* Disable cache */
	mtpr(CCR_DIA, KA660_CCR);	/* Switch to diag mode */
	bnk = 1;
	behrtmp = 0;
	while(bnk <= 0x80)
	{
		*(int *)KA660_BEHR = bnk;
		p = (int *)KA660_CDATA;
		*p = 0x55aaff00L;
		if(*p == 0x55aaff00L) behrtmp |= bnk;
		*p = 0xffaa0055L;
		if(*p != 0xffaa0055L) behrtmp &= ~bnk;
		cnt = 256;
		while(cnt--) *p++ = 0L;
		p = (int *) KA660_CTAG;
		cnt =128;
		while(cnt--) { *p++ = 0x80000000L; p++; }
		bnk <<= 1;
	}
	*(int *)KA660_BEHR = behrtmp;

	mtpr(CCR_DIA|CCR_FLU, KA660_CCR);	/* Flush tags */
	mtpr(CCR_ENA, KA660_CCR);	/* Enable cache */
}

void
ka660_memerr()
{
	printf("Memory err!\n");
}

int
ka660_mchk(addr)
	caddr_t addr;
{
	panic("Machine check");
	return 0;
}

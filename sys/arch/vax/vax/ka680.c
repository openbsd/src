/*	$OpenBSD: ka680.c,v 1.9 2002/07/21 09:17:14 hugh Exp $	*/
/*	$NetBSD: ka680.c,v 1.3 2001/01/28 21:01:53 ragge Exp $	*/
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

/* Done by Michael Kukat (michael@unixiron.org) */
/* minor modifications for KA690 cache support by isildur@vaxpower.org */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/nexus.h>
#include <machine/uvax.h>
#include <machine/ka680.h>
#include <machine/clock.h>
#include <machine/scb.h>

static void	ka680_conf(void);
static void	ka680_cache_enable(void);
static void	ka680_softmem(void *);
static void	ka680_hardmem(void *);
static void	ka680_steal_pages(void);
static void	ka680_memerr(void);
static int	ka680_mchk(caddr_t);
static void	ka680_halt(void);
static void	ka680_reboot(int);
 
/*
 * KA680-specific IPRs. KA680 has the funny habit to control all caches
 * via IPRs.
 */
#define PR_CCTL	 0xa0
#define CCTL_ENABLE	0x00000001
#define CCTL_SW_ETM	0x40000000
#define CCTL_HW_ETM	0x80000000

#define PR_BCETSTS	0xa3
#define PR_BCEDSTS	0xa6
#define PR_NESTS	0xae

#define PR_VMAR	 0xd0
#define PR_VTAG	 0xd1
#define PR_ICSR	 0xd3
#define ICSR_ENABLE	0x01

#define PR_PCCTL	0xf8
#define PCCTL_P_EN	0x10
#define PCCTL_I_EN	0x02
#define PCCTL_D_EN	0x01

 
/* 
 * Declaration of KA680-specific calls.
 */
struct cpu_dep ka680_calls = {
	ka680_steal_pages,
	ka680_mchk,
	ka680_memerr, 
	ka680_conf,
	generic_clkread,
	generic_clkwrite,
	24,	 /* ~VUPS */
	2,	/* SCB pages */
	ka680_halt,
	ka680_reboot,
};


void
ka680_conf()
{
	char *cpuname;

	/* Don't ask why, but we seem to need this... */

	volatile int *hej = (void *)mfpr(PR_ISP);
	*hej = *hej;
	hej[-1] = hej[-1];

	switch(vax_boardtype) {
	case VAX_BTYP_1301:
		switch((vax_siedata & 0xff00) >> 8) {
		case VAX_STYP_675:
			cpuname = "KA675";
			break;
		case VAX_STYP_680:
			cpuname = "KA680";
			break;
		case VAX_STYP_690:
			cpuname = "KA690";
			break;
		default:
			cpuname = "unknown NVAX 1301";
		}
		break;
	case VAX_BTYP_1305:
		switch((vax_siedata & 0xff00) >> 8) {
		case VAX_STYP_681:
			cpuname = "KA681";
			break;
		case VAX_STYP_691:
			cpuname = "KA691";
			break;
		case VAX_STYP_694:
			if (vax_cpudata & 0x1000)
				cpuname = "KA694";
			else
				cpuname = "KA692";
			break;
		default:
			cpuname = "unknown NVAX 1305";
		}
	}
	printf("cpu0: %s, ucode rev %d\n", cpuname, vax_cpudata & 0xff);
}

void
ka680_cache_enable()
{
	int start, pslut, fslut, cslut, havevic;

	/*
	 * Turn caches off.
	 */
	mtpr(0, PR_ICSR);
	mtpr(0, PR_PCCTL);
	mtpr(mfpr(PR_CCTL) | CCTL_SW_ETM, PR_CCTL);

	/*
	 * Invalidate caches.
	 */
	mtpr(mfpr(PR_CCTL) | 6, PR_CCTL);	/* Set cache size and speed */
	mtpr(mfpr(PR_BCETSTS), PR_BCETSTS);	/* Clear error bits */
	mtpr(mfpr(PR_BCEDSTS), PR_BCEDSTS);	/* Clear error bits */
	mtpr(mfpr(PR_NESTS), PR_NESTS);	 /* Clear error bits */

	switch((vax_siedata & 0xff00) >> 8) {
	case VAX_STYP_680:
	case VAX_STYP_681:	/* XXX untested */
		fslut = 0x01420000;
		cslut = 0x01020000;
		havevic = 1;
		break;
	case VAX_STYP_690:
		fslut = 0x01440000;
		cslut = 0x01040000;
		havevic = 1;
		break;
	case VAX_STYP_691:	/* XXX untested */
		fslut = 0x01420000;
		cslut = 0x01020000;
		havevic = 1;
		break;
	case VAX_STYP_694:	/* XXX untested */
		fslut = 0x01440000;
		cslut = 0x01040000;
		havevic = 1;
		break;
	case VAX_STYP_675:
	default:		/* unknown cpu; cross fingers */
		fslut = 0x01420000;
		cslut = 0x01020000;
		havevic = 0;
		break;
	}

	start = 0x01400000;

	/* Flush cache lines */
	for (; start < fslut; start += 0x20)
		mtpr(0, start);

	mtpr((mfpr(PR_CCTL) & ~(CCTL_SW_ETM|CCTL_ENABLE)) | CCTL_HW_ETM,
	    PR_CCTL);

	start = 0x01000000;

	/* clear tag and valid */
	for (; start < cslut; start += 0x20)
		mtpr(0, start);

	mtpr(mfpr(PR_CCTL) | 6 | CCTL_ENABLE, PR_CCTL); /* enab. bcache */

	start = 0x01800000;
	pslut = 0x01802000;

	/* Clear primary cache */
	for (; start < pslut; start += 0x20)
		mtpr(0, start);

	/* Flush the pipes (via REI) */
	asm("movpsl -(sp); movab 1f,-(sp); rei; 1:;");

	/* Enable primary cache */
	mtpr(PCCTL_P_EN|PCCTL_I_EN|PCCTL_D_EN, PR_PCCTL);

	/* Enable the VIC */
	if (havevic) {
		int slut;

		start = 0;
		slut  = 0x800;
		for (; start < slut; start += 0x20) {
			mtpr(start, PR_VMAR);
			mtpr(0, PR_VTAG);
		}
		mtpr(ICSR_ENABLE, PR_ICSR);
	}
}

/*
 * Why may we get memory errors during startup???
 */

void
ka680_hardmem(void *arg)
{
	if (cold == 0)
		printf("Hard memory error\n");
	splhigh();
}

void
ka680_softmem(void *arg)
{
	if (cold == 0)
		printf("Soft memory error\n");
	splhigh();
}

void
ka680_steal_pages()
{
	/*
	 * Get the soft and hard memory error vectors now.
	 */
	scb_vecalloc(0x54, ka680_softmem, NULL, 0, NULL);
	scb_vecalloc(0x60, ka680_hardmem, NULL, 0, NULL);

	/* Turn on caches (to speed up execution a bit) */
	ka680_cache_enable();
}

void
ka680_memerr()
{
	printf("Memory err!\n");
}

int
ka680_mchk(caddr_t addr)
{
	panic("Machine check");
	return 0;
}

static void
ka680_halt()
{
	asm("halt");
}

static void
ka680_reboot(int arg)
{
	asm("halt");
}


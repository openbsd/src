/*	$NetBSD: uvaxII.c,v 1.4 1995/12/13 18:50:11 ragge Exp $	*/

/*-
 * Copyright (c) 1988 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)ka630.c     7.8 (Berkeley) 5/9/91
 */

#include "sys/param.h"
#include "sys/types.h"
#include "sys/device.h"
#include "vm/vm.h"
#include "vm/vm_kern.h"

#include "machine/uvaxII.h"
#include "machine/pte.h"
#include "machine/mtpr.h"
#include "machine/sid.h"
#include "machine/pmap.h"
#include "machine/nexus.h"

struct uvaxIIcpu *uvaxIIcpu_ptr;

#if VAX630
struct	ka630clock *ka630clk_ptr;
u_long	ka630_clkread();
void	ka630_clkwrite();
#endif

/*
 * uvaxII_conf() is called by cpu_attach to do the cpu_specific setup.
 */
void
uvaxII_conf(parent, self, aux)
	struct	device *parent, *self;
	void	*aux;
{
	extern char cpu_model[];

	switch (cpu_type) {
	case VAX_630:
		strcpy(cpu_model,"MicroVAX II");
		break;
	case VAX_410:
		strcpy(cpu_model,"MicroVAX 2000");
		break;
	};
	strcpy(cpu_model, "MicroVAX 78032/78132");
}

uvaxII_clock()
{
	mtpr(0x40, PR_ICCS); /* Start clock and enable interrupt */
	return 1;
}

/* log crd errors */
uvaxII_memerr()
{
	printf("memory err!\n");
}

#define NMC78032 10
char *mc78032[] = {
	0,		"immcr (fsd)",	"immcr (ssd)",	"fpu err 0",
	"fpu err 7",	"mmu st(tb)",	"mmu st(m=0)",	"pte in p0",
	"pte in p1",	"un intr id",
};

struct mc78032frame {
	int	mc63_bcnt;		/* byte count == 0xc */
	int	mc63_summary;		/* summary parameter */
	int	mc63_mrvaddr;		/* most recent vad */
	int	mc63_istate;		/* internal state */
	int	mc63_pc;		/* trapped pc */
	int	mc63_psl;		/* trapped psl */
};

uvaxII_mchk(cmcf)
	caddr_t cmcf;
{
	register struct mc78032frame *mcf = (struct mc78032frame *)cmcf;
	register u_int type = mcf->mc63_summary;

	printf("machine check %x", type);
	if (type < NMC78032 && mc78032[type])
		printf(": %s", mc78032[type]);
	printf("\n\tvap %x istate %x pc %x psl %x\n",
	    mcf->mc63_mrvaddr, mcf->mc63_istate,
	    mcf->mc63_pc, mcf->mc63_psl);
	if (uvaxIIcpu_ptr && uvaxIIcpu_ptr->uvaxII_mser & UVAXIIMSER_MERR) {
		printf("\tmser=0x%x ", uvaxIIcpu_ptr->uvaxII_mser);
		if (uvaxIIcpu_ptr->uvaxII_mser & UVAXIIMSER_CPUE)
			printf("page=%d", uvaxIIcpu_ptr->uvaxII_cear);
		if (uvaxIIcpu_ptr->uvaxII_mser & UVAXIIMSER_DQPE)
			printf("page=%d", uvaxIIcpu_ptr->uvaxII_dear);
		printf("\n");
	}
	return (-1);
}

/*
 * Handle the watch chip used by the ka630 and ka410 mother boards.
 */
u_long
uvaxII_gettodr(stopped_flag)
	int *stopped_flag;
{
	register u_long year_secs;

	switch (cpu_type) {
#if VAX630
	case VAX_630:
		year_secs = ka630_clkread(stopped_flag);
		break;
#endif
	default:
		year_secs = 0;
		*stopped_flag = 1;
	};
	return (year_secs * 100);
}

void
uvaxII_settodr(year_ticks)
	u_long year_ticks;
{
	register u_long year_secs;

	year_secs = year_ticks / 100;
	switch (cpu_type) {
#if VAX630
	case VAX_630:
		ka630_clkwrite(year_secs);
		break;
#endif
	};
}

#if VAX630
static short dayyr[12] = { 0,31,59,90,120,151,181,212,243,273,304,334 };
/* init system time from tod clock */
/* ARGSUSED */
u_long
ka630_clkread(stopped_flag)
	int *stopped_flag;
{
	register struct ka630clock *claddr = ka630clk_ptr;
	register int days, yr;
	register u_long year_secs;

	*stopped_flag = 0;
	claddr->csr1 = KA630CLK_SET;
	while ((claddr->csr0 & KA630CLK_UIP) != 0)
		;
	/* If the clock is valid, use it. */
	if ((claddr->csr3 & KA630CLK_VRT) != 0 &&
	    (claddr->csr1 & KA630CLK_ENABLE) == KA630CLK_ENABLE) {
		/* simple sanity checks */
		if (claddr->mon < 1 || claddr->mon > 12 ||
		    claddr->day < 1 || claddr->day > 31) {
			printf("WARNING: preposterous clock chip time.\n");
			year_secs = 0;
		} else {
			days = dayyr[claddr->mon - 1] + claddr->day - 1;
			year_secs = days * DAYSEC + claddr->hr * HRSEC +
				claddr->min * MINSEC + claddr->sec;
		}
		claddr->yr = 70;	/* any non-leap year */
#ifndef lint
		{ volatile int t = claddr->csr2; }	/* ??? */
#endif
		claddr->csr0 = KA630CLK_RATE;
		claddr->csr1 = KA630CLK_ENABLE;

		return (year_secs);
	}
	printf("WARNING: TOY clock invalid.\n");
	return (0);
}

/* Set the time of day clock, called via. stime system call.. */
void
ka630_clkwrite(year_secs)
	u_long year_secs;
{
	register struct ka630clock *claddr = ka630clk_ptr;
	register int t, t2;
	int s;

	s = splhigh();
	claddr->csr1 = KA630CLK_SET;
	while ((claddr->csr0 & KA630CLK_UIP) != 0)
		;
	claddr->yr = 70;	/* any non-leap year is ok */

	/* t = month + day; separate */
	t = year_secs % YEARSEC;
	for (t2 = 1; t2 < 12; t2++)
		if (t < dayyr[t2])
			break;

	/* t2 is month */
	claddr->mon = t2;
	claddr->day = t - dayyr[t2 - 1] + 1;

	/* the rest is easy */
	t = year_secs % DAYSEC;
	claddr->hr = t / HRSEC;
	t %= HRSEC;
	claddr->min = t / MINSEC;
	claddr->sec = t % MINSEC;
#ifndef lint
	{ volatile int t = claddr->csr2; }	/* ??? */
	{ volatile int t = claddr->csr3; }	/* ??? */
#endif
	claddr->csr0 = KA630CLK_RATE;
	claddr->csr1 = KA630CLK_ENABLE;
	splx(s);
}
#endif

uvaxII_steal_pages()
{
	extern  vm_offset_t avail_start, virtual_avail, avail_end;
	int	junk;

	/*
	 * MicroVAX II: get 10 pages from top of memory,
	 * map in Qbus map registers, cpu and clock registers.
	 */
	avail_end -= 10;

	MAPPHYS(junk, 2, VM_PROT_READ|VM_PROT_WRITE);
	MAPVIRT(nexus, btoc(0x400000));
	pmap_map((vm_offset_t)nexus, 0x20088000, 0x20090000,
	    VM_PROT_READ|VM_PROT_WRITE);

	MAPVIRT(uvaxIIcpu_ptr, 1);
	pmap_map((vm_offset_t)uvaxIIcpu_ptr, (vm_offset_t)UVAXIICPU,
	    (vm_offset_t)UVAXIICPU + NBPG, VM_PROT_READ|VM_PROT_WRITE);

	MAPVIRT(ka630clk_ptr, 1);
	pmap_map((vm_offset_t)ka630clk_ptr, (vm_offset_t)KA630CLK,
	    (vm_offset_t)KA630CLK + NBPG, VM_PROT_READ|VM_PROT_WRITE);

	/*
	 * Clear restart and boot in progress flags
	 * in the CPMBX.
	 /
	ka630clk_ptr->cpmbx = (ka630clk_ptr->cpmbx & KA630CLK_LANG);

	/*
	 * Enable memory parity error detection and clear error bits.
	 */
	uvaxIIcpu_ptr->uvaxII_mser = (UVAXIIMSER_PEN | UVAXIIMSER_MERR |
	    UVAXIIMSER_LEB);

	/*
	 * Set up cpu_type so that we can differ between 630 and 420.
	 */
        if (cpunumber == VAX_78032)
                cpu_type = (((*UVAXIISID) >> 24) & 0xff) |
		    (cpu_type & 0xff000000);
}

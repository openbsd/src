/*	$OpenBSD: ka630.c,v 1.10 2008/08/18 23:05:38 miod Exp $	*/
/*	$NetBSD: ka630.c,v 1.17 1999/09/06 19:52:52 ragge Exp $	*/
/*-
 * Copyright (c) 1982, 1988, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)ka630.c	7.8 (Berkeley) 5/9/91
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/pmap.h>
#include <machine/ka630.h>
#include <machine/clock.h>
#include <machine/vsbus.h>

static struct uvaxIIcpu *uvaxIIcpu_ptr;

static void ka630_conf(void);
static void ka630_memerr(void);
static int ka630_mchk(caddr_t);
static void ka630_halt(void);
static void ka630_reboot(int);
static void ka630_clrf(void);

struct	cpu_dep ka630_calls = {
	0,
	ka630_mchk,
	ka630_memerr,
	ka630_conf,
	chip_clkread,
	chip_clkwrite,
	1,      /* ~VUPS */
	2,	/* SCB pages */
	ka630_halt,
	ka630_reboot,
	ka630_clrf,
	NULL,
	hardclock
};

/*
 * uvaxII_conf() is called by cpu_attach to do the cpu_specific setup.
 */
void
ka630_conf()
{
	clk_adrshift = 0;	/* Addressed at short's... */
	clk_tweak = 0;		/* ...and no shifting */
	clk_page = (short *)vax_map_physmem((paddr_t)KA630CLK, 1);

	uvaxIIcpu_ptr = (void *)vax_map_physmem(VS_REGS, 1);

	/*
	 * Enable memory parity error detection and clear error bits.
	 */
	uvaxIIcpu_ptr->uvaxII_mser = (UVAXIIMSER_PEN | UVAXIIMSER_MERR |
	    UVAXIIMSER_LEB);
}

/* log crd errors */
void
ka630_memerr()
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

int
ka630_mchk(cmcf)
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
		printf("\tmser=0x%lx ", uvaxIIcpu_ptr->uvaxII_mser);
		if (uvaxIIcpu_ptr->uvaxII_mser & UVAXIIMSER_CPUE)
			printf("page=%ld", uvaxIIcpu_ptr->uvaxII_cear);
		if (uvaxIIcpu_ptr->uvaxII_mser & UVAXIIMSER_DQPE)
			printf("page=%ld", uvaxIIcpu_ptr->uvaxII_dear);
		printf("\n");
	}
	return (-1);
}

static void
ka630_halt()
{
	((struct ka630clock *)clk_page)->cpmbx = KA630CLK_DOTHIS|KA630CLK_HALT;
	asm("halt");
}

static void
ka630_reboot(arg)
	int arg;
{
	((struct ka630clock *)clk_page)->cpmbx =
	    KA630CLK_DOTHIS | KA630CLK_REBOOT;
}

/*
 * Clear restart and boot in progress flags in the CPMBX.
 */
static void
ka630_clrf()
{
	short i = ((struct ka630clock *)clk_page)->cpmbx;

	((struct ka630clock *)clk_page)->cpmbx = i & KA630CLK_LANG;
}

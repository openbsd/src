/*	$OpenBSD: ka48.c,v 1.11 2011/07/06 20:42:05 miod Exp $	*/
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

/*** needs to be completed MK-990306 ***/

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
#include <machine/ka48.h>
#include <machine/clock.h>
#include <machine/vsbus.h>

static	void	ka48_conf(void);
static	void	ka48_init(void);
static	void	ka48_memerr(void);
static	int	ka48_mchk(caddr_t);
static	void	ka48_halt(void);
static	void	ka48_reboot(int);
static	void	ka48_cache_enable(void);

struct	vs_cpu *ka48_cpu;

/* 
 * Declaration of 48-specific calls.
 */
struct	cpu_dep ka48_calls = {
	ka48_init,
	ka48_mchk,
	ka48_memerr, 
	ka48_conf,
	chip_clkread,
	chip_clkwrite,
	6,      /* ~VUPS */
	2,	/* SCB pages */
	ka48_halt,
	ka48_reboot,
	NULL,
	hardclock
};


void
ka48_conf()
{
	char *cpuname;
	switch((vax_siedata >> 8) & 0xFF) {
	case VAX_STYP_45:
		cpuname = "KA45";
		break;
	case VAX_STYP_48:
		cpuname = "KA48";
		break;
	default:
		cpuname = "Unknown SOC";
	}
	printf("cpu: %s\n", cpuname);
	ka48_cpu = (void *)vax_map_physmem(VS_REGS, 1);
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
ka48_cache_enable()
{
	int i, *tmp;
	long *par_ctl = (long *)KA48_PARCTL;

	/* Disable cache */
	mtpr(0, PR_CADR);		/* disable */
	*par_ctl &= ~KA48_PARCTL_INVENA;	/* clear ? invalid enable */
	mtpr(2, PR_CADR);		/* flush */

	/* Clear caches */
	tmp = (void *)KA48_INVFLT;	/* inv filter */
	for (i = 0; i < KA48_INVFLTSZ / sizeof(int); i++)
		tmp[i] = 0;
	*par_ctl |= KA48_PARCTL_INVENA;	/* Enable ???? */
	mtpr(4, PR_CADR);		/* enable cache */
	*par_ctl |= (KA48_PARCTL_AGS |	/* AGS? */
	    KA48_PARCTL_NPEN |		/* N? Parity Enable */
	    KA48_PARCTL_CPEN);		/* Cpu parity enable */
}

void
ka48_memerr()
{
	printf("Memory err!\n");
}

int
ka48_mchk(addr)
	caddr_t addr;
{
	panic("Machine check");
	return 0;
}

void
ka48_init()
{
	/* Turn on caches (to speed up execution a bit) */
	ka48_cache_enable();
}

#define	KA48_CPMBX	0x38
#define	KA48_HLT_HALT	0xcf	/* 11001111 */
#define	KA48_HLT_BOOT	0x8b	/* 10001011 */

static void
ka48_halt()
{
	if (((u_int8_t *) clk_page)[KA48_CPMBX] != KA48_HLT_HALT)
		((u_int8_t *) clk_page)[KA48_CPMBX] = KA48_HLT_HALT;
	asm("halt");
}

static void
ka48_reboot(arg)
	int arg;
{
	if (((u_int8_t *) clk_page)[KA48_CPMBX] != KA48_HLT_BOOT)
		((u_int8_t *) clk_page)[KA48_CPMBX] = KA48_HLT_BOOT;
	asm("halt");
}

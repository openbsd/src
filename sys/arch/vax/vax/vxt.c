/*	$OpenBSD: vxt.c,v 1.8 2011/09/15 00:48:24 miod Exp $	*/
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

#include <machine/cpu.h>
#include <machine/mtpr.h>
#include <machine/ka48.h>
#include <machine/clock.h>

static	void	vxt_conf(void);
static	void	vxt_init(void);
static	void	vxt_memerr(void);
static	int	vxt_mchk(caddr_t);
static	void	vxt_halt(void);
static	void	vxt_reboot(int);
static	void	vxt_cache_enable(void);
static	int	missing_clkread(struct timespec *, time_t);
static	void	missing_clkwrite(void);

/* 
 * Declaration of vxt-specific calls.
 */

struct	cpu_dep vxt_calls = {
	vxt_init,
	vxt_mchk,
	vxt_memerr, 
	vxt_conf,
	missing_clkread,
	missing_clkwrite,
	6,      /* ~VUPS */
	2,	/* SCB pages */
	vxt_halt,
	vxt_reboot,
	NULL,
	hardclock
};

void
vxt_conf()
{
	printf("cpu: VXT\n");
}

void
vxt_cache_enable()
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
vxt_memerr()
{
	printf("Memory err!\n");
}

int
vxt_mchk(addr)
	caddr_t addr;
{
	panic("Machine check");
	return 0;
}

void
vxt_init()
{
	/* Turn on caches (to speed up execution a bit) */
	vxt_cache_enable();
}

static void
vxt_halt()
{
	asm("halt");
}

static void
vxt_reboot(arg)
	int arg;
{
	asm("halt");
}

int
missing_clkread(struct timespec *ts, time_t base)
{
	printf("WARNING: no TOY clock");
	return EINVAL;
}

void
missing_clkwrite()
{
}

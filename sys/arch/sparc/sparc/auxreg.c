/*	$OpenBSD: auxreg.c,v 1.12 2005/04/17 18:47:50 miod Exp $	*/
/*	$NetBSD: auxreg.c,v 1.21 1997/05/24 20:15:59 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)auxreg.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/timeout.h>

#include <uvm/uvm_param.h>

#include <machine/autoconf.h>

#include <sparc/sparc/vaddrs.h>
#include <sparc/sparc/auxioreg.h>

static int auxregmatch(struct device *, void *, void *);
static void auxregattach(struct device *, struct device *, void *);

struct cfattach auxreg_ca = {
	sizeof(struct device), auxregmatch, auxregattach
};

struct cfdriver auxreg_cd = {
	0, "auxreg", DV_DULL
};

volatile u_char *auxio_reg;	/* Copy of AUXIO_REG */
u_char auxio_regval;
extern int sparc_led_blink;	/* from machdep */
struct timeout sparc_led_to;

void
led_blink(zero)
	void *zero;
{
	int s;

	/* Don't do anything if there's no auxreg, ok? */
	if (auxio_reg == 0)
		return;

	if (!sparc_led_blink) {
		/* If blink has been disabled, make sure it goes back on... */
		s = splhigh();
		LED_ON;
		splx(s);
	
		return;
	}

	s = splhigh();
	LED_FLIP;
	splx(s);
	/*
	 * Blink rate is:
	 *	full cycle every second if completely idle (loadav = 0)
	 *	full cycle every 2 seconds if loadav = 1
	 *	full cycle every 3 seconds if loadav = 2
	 * etc.
	 */
	s = (((averunnable.ldavg[0] + FSCALE) * hz) >> (FSHIFT + 1));

	timeout_add(&sparc_led_to, s);
}

/*
 * The OPENPROM calls this "auxiliary-io".
 */
static int
auxregmatch(parent, cf, aux)
	struct device *parent;
	void *cf, *aux;
{
	register struct confargs *ca = aux;

	switch (cputyp) {
	    case CPU_SUN4:
		return (0);
	    case CPU_SUN4C:
		return (strcmp("auxiliary-io", ca->ca_ra.ra_name) == 0);
	    case CPU_SUN4M:
		return (strcmp("auxio", ca->ca_ra.ra_name) == 0);
	    default:
		panic("auxregmatch");
	}
}

/* ARGSUSED */
static void
auxregattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	(void)mapdev(ra->ra_reg, AUXREG_VA, 0, sizeof(long));
	if (CPU_ISSUN4M) {
		auxio_reg = AUXIO4M_REG;
		auxio_regval = *AUXIO4M_REG | AUXIO4M_MB1;
	} else {
		auxio_reg = AUXIO4C_REG;
		auxio_regval = *AUXIO4C_REG | AUXIO4C_FEJ | AUXIO4C_MB1;
	}

	printf("\n");

	timeout_set(&sparc_led_to, led_blink, NULL);
	/* In case it's initialized to true... */
	if (sparc_led_blink)
		led_blink((caddr_t)0);
}

unsigned int
auxregbisc(bis, bic)
	int bis, bic;
{
	register int s;

	if (auxio_reg == 0)
		/*
		 * Not all machines have an `aux' register; devices that
		 * depend on it should not get configured if it's absent.
		 */
		panic("no aux register");

	s = splhigh();
	auxio_regval = (auxio_regval | bis) & ~bic;
	*auxio_reg = auxio_regval;
	splx(s);
	return (auxio_regval);
}

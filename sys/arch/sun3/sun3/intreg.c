/*	$NetBSD: intreg.c,v 1.1 1996/03/26 15:03:11 gwr Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
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
 *	This product includes software developed by Adam Glass.
 * 4. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This handles multiple attach of autovectored interrupts,
 * and the handy software interrupt request register.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/vmmeter.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/mon.h>
#include <machine/obio.h>
#include <machine/isr.h>

#include "interreg.h"

struct intreg_softc {
	struct device sc_dev;
	volatile u_char *sc_reg;
};

static int  intreg_match __P((struct device *, void *vcf, void *args));
static void intreg_attach __P((struct device *, struct device *, void *));
static int soft1intr();

struct cfattach intreg_ca = {
	sizeof(struct intreg_softc), intreg_match, intreg_attach
};

struct cfdriver intreg_cd = {
	NULL, "intreg", DV_DULL
};

volatile u_char *interrupt_reg;


/* called early (by internal_configure) */
void intreg_init()
{
	interrupt_reg = obio_find_mapping(OBIO_INTERREG, 1);
	if (!interrupt_reg)
		mon_panic("interrupt reg VA not found\n");
	/* Turn off all interrupts until clock_attach */
	*interrupt_reg = 0;
}


static int
intreg_match(parent, vcf, args)
    struct device *parent;
    void *vcf, *args;
{
    struct cfdata *cf = vcf;
	struct confargs *ca = args;
	int pa;

	/* This driver only supports one unit. */
	if (cf->cf_unit != 0)
		return (0);

	if ((pa = cf->cf_paddr) == -1) {
		/* Use our default PA. */
		pa = OBIO_INTERREG;
	} else {
		/* Validate the given PA. */
		if (pa != OBIO_INTERREG)
			panic("clock: wrong address");
	}
	if (pa != ca->ca_paddr)
		return (0);

	return (1);
}


static void
intreg_attach(parent, self, args)
	struct device *parent;
	struct device *self;
	void *args;
{
	struct intreg_softc *sc = (void *)self;
	struct cfdata *cf = self->dv_cfdata;

	printf("\n");

	sc->sc_reg = interrupt_reg;

	/* Install handler for our "soft" interrupt. */
	isr_add_autovect(soft1intr, (void *)sc, 1);
}


/*
 * Level 1 software interrupt.
 * Possible reasons:
 *	Network software interrupt
 *	Soft clock interrupt
 */
int soft1intr(arg)
	void *arg;
{
	union sun3sir sir;
	int n, s;

	s = splhigh();
	sir.sir_any = sun3sir.sir_any;
	sun3sir.sir_any = 0;
	isr_soft_clear(1);
	splx(s);

	if (sir.sir_any) {
		cnt.v_soft++;
		if (sir.sir_which[SIR_NET]) {
			sir.sir_which[SIR_NET] = 0;
			netintr();
		}
		if (sir.sir_which[SIR_CLOCK]) {
			sir.sir_which[SIR_CLOCK] = 0;
			softclock();
		}
		if (sir.sir_which[SIR_SPARE2]) {
			sir.sir_which[SIR_SPARE2] = 0;
			/* spare2intr(); */
		}
		if (sir.sir_which[SIR_SPARE3]) {
			sir.sir_which[SIR_SPARE3] = 0;
			/* spare3intr(); */
		}
		return (1);
	}
	return(0);
}


static int isr_soft_pending;
void isr_soft_request(level)
	int level;
{
	u_char bit, reg_val;
	int s;

	if ((level < 1) || (level > 3))
		panic("isr_soft_request");

	bit = 1 << level;

	/* XXX - Should do this in the callers... */
	if (isr_soft_pending & bit)
		return;

	s = splhigh();
	isr_soft_pending |= bit;
	reg_val = *interrupt_reg;
	*interrupt_reg &= ~IREG_ALL_ENAB;

	*interrupt_reg |= bit;
	*interrupt_reg |= IREG_ALL_ENAB;
	splx(s);
}

void isr_soft_clear(level)
	int level;
{
	u_char bit, reg_val;
	int s;

	if ((level < 1) || (level > 3))
		panic("isr_soft_clear");

	bit = 1 << level;

	s = splhigh();
	isr_soft_pending &= ~bit;
	reg_val = *interrupt_reg;
	*interrupt_reg &= ~IREG_ALL_ENAB;

	*interrupt_reg &= ~bit;
	*interrupt_reg |= IREG_ALL_ENAB;
	splx(s);
}


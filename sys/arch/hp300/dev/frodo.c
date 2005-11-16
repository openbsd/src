/*	$OpenBSD: frodo.c,v 1.7 2005/11/16 21:37:02 miod Exp $	*/
/*	$NetBSD: frodo.c,v 1.5 1999/07/31 21:15:20 thorpej Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1997 Michael Smith.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Support for the "Frodo" (a.k.a. "Apollo Utility") chip found
 * in HP Apollo 9000/4xx workstations, as well as HP 9000/362 and 9000/382.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/hp300spu.h>

#include <hp300/dev/intiovar.h>

#include <hp300/dev/frodoreg.h>
#include <hp300/dev/frodovar.h>

struct frodo_softc {
	struct device	sc_dev;		/* generic device glue */
	volatile u_int8_t *sc_regs;	/* register base */
	struct isr	*sc_intr[FRODO_NINTR]; /* interrupt handlers */
	struct isr	sc_isr;		/* main interrupt handler */
	int		sc_refcnt;	/* number of interrupt refs */
};

int	frodomatch(struct device *, void *, void *);
void	frodoattach(struct device *, struct device *, void *);

int	frodoprint(void *, const char *);
int	frodosubmatch(struct device *, void *, void *);

int	frodointr(void *);

void	frodo_imask(struct frodo_softc *, u_int16_t, u_int16_t);

struct cfattach frodo_ca = {
	sizeof(struct frodo_softc), frodomatch, frodoattach
};

struct cfdriver frodo_cd = {
	NULL, "frodo", DV_DULL
};

struct frodo_attach_args frodo_subdevs[] = {
	{ "dnkbd",	FRODO_APCI_OFFSET(0),	FRODO_INTR_APCI0 },
	{ "apci",	FRODO_APCI_OFFSET(1),	FRODO_INTR_APCI1 },
	{ "apci",	FRODO_APCI_OFFSET(2),	FRODO_INTR_APCI2 },
	{ "apci",	FRODO_APCI_OFFSET(3),	FRODO_INTR_APCI3 },
	{ NULL,		0,			0 },
};

#define	FRODO_IPL	5

int
frodomatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct intio_attach_args *ia = aux;
	caddr_t va;
	static int frodo_matched = 0;

	/* only allow one instance */
	if (frodo_matched)
		return (0);

	/* only 4xx workstations can have this */
	switch (machineid) {
	case HP_362:
	case HP_382:
	case HP_400:
	case HP_425:
	case HP_433:
		break;

	default:
		return (0);
	}

	/* make sure the hardware is there in any case */
	va = (caddr_t)IIOV(FRODO_BASE);
	if (badaddr(va))
		return (0);

	frodo_matched = 1;
	ia->ia_addr = (caddr_t)FRODO_BASE;
	return (1);
}

void
frodoattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct frodo_softc *sc = (struct frodo_softc *)self;
	struct intio_attach_args *ia = aux;
	int i;

	sc->sc_regs = (volatile u_int8_t *)IIOV(ia->ia_addr);

	printf(" ipl %d", FRODO_IPL);
	if ((FRODO_READ(sc, FRODO_IISR) & FRODO_IISR_SERVICE) == 0)
		printf(": service mode enabled");
	printf("\n");

	/* Clear all of the interrupt handlers. */
	bzero(sc->sc_intr, sizeof(sc->sc_intr));

	/*
	 * Disable all of the interrupt lines; we reenable them
	 * as subdevices attach.
	 */
	frodo_imask(sc, 0, 0xffff);

	/* Clear any pending interrupts. */
	FRODO_WRITE(sc, FRODO_PIC_PU, 0xff);
	FRODO_WRITE(sc, FRODO_PIC_PL, 0xff);

	/* Set interrupt polarities. */
	FRODO_WRITE(sc, FRODO_PIO_IPR, 0x10);

	/* ...and configure for edge triggering. */
	FRODO_WRITE(sc, FRODO_PIO_IELR, 0xcf);

	/*
	 * We defer hooking up our interrupt handler until
	 * a subdevice hooks up theirs.
	 */
	sc->sc_refcnt = 0;

	/* ... and attach subdevices. */
	for (i = 0; frodo_subdevs[i].fa_name != NULL; i++) {
		/*
		 * Skip the first serial port if we're not a 425e;
		 * it's mapped to the DCA at select code 9 on all
		 * other models.
		 */
		if (frodo_subdevs[i].fa_offset == FRODO_APCI_OFFSET(1) &&
		    mmuid != MMUID_425_E)
			continue;
		config_found_sm(self, &frodo_subdevs[i],
		    frodoprint, frodosubmatch);
	}
}

int
frodosubmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct frodo_attach_args *fa = aux;

	if (cf->frodocf_offset != FRODO_UNKNOWN_OFFSET &&
	    cf->frodocf_offset != fa->fa_offset)
		return (0);

	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

int
frodoprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct frodo_attach_args *fa = aux;

	if (pnp)
		printf("%s at %s", fa->fa_name, pnp);
	printf(" offset 0x%x", fa->fa_offset);
	return (UNCONF);
}

void
frodo_intr_establish(struct device *frdev, int line, struct isr *isr,
    const char *name)
{
	struct frodo_softc *sc = (struct frodo_softc *)frdev;
	int priority = isr->isr_priority;

	if (line < 0 || line >= FRODO_NINTR) {
		panic("%s: bad interrupt line %d",
		    sc->sc_dev.dv_xname, line);
	}
	if (sc->sc_intr[line] != NULL) {
		panic("%s: interrupt line %d already used",
		    sc->sc_dev.dv_xname, line);
	}

	/*
	 * If this is the first one, establish the frodo
	 * interrupt handler.  If not, reestablish at a
	 * higher priority if necessary.
	 */
	if (sc->sc_isr.isr_priority < priority) {
		if (sc->sc_refcnt != 0)
			intr_disestablish(&sc->sc_isr);
		sc->sc_isr.isr_func = frodointr;
		sc->sc_isr.isr_arg = sc;
		sc->sc_isr.isr_ipl = FRODO_IPL;
		sc->sc_isr.isr_priority = priority;
		intr_establish(&sc->sc_isr, sc->sc_dev.dv_xname);
	}

	sc->sc_refcnt++;

	/* Install the handler. */
	isr->isr_ipl = sc->sc_isr.isr_ipl;
	evcount_attach(&isr->isr_count, name, &isr->isr_ipl, &evcount_intr);
	sc->sc_intr[line] = isr;

	/* Enable the interrupt line. */
	frodo_imask(sc, (1 << line), 0);
}

void
frodo_intr_disestablish(frdev, line)
	struct device *frdev;
	int line;
{
	struct frodo_softc *sc = (struct frodo_softc *)frdev;
	int newpri;

	if (sc->sc_intr[line] == NULL) {
		panic("%s: no handler for line %d",
		    sc->sc_dev.dv_xname, line);
	}

	sc->sc_intr[line] = NULL;
	frodo_imask(sc, 0, (1 << line));

	/* If this was the last, unhook ourselves. */
	if (sc->sc_refcnt-- == 1) {
		intr_disestablish(&sc->sc_isr);
		return;
	}

	/* Lower our priority, if appropriate. */
	for (newpri = 0, line = 0; line < FRODO_NINTR; line++)
		if (sc->sc_intr[line] != NULL &&
		    sc->sc_intr[line]->isr_priority > newpri)
			newpri = sc->sc_intr[line]->isr_priority;

	if (newpri != sc->sc_isr.isr_priority) {
		intr_disestablish(&sc->sc_isr);
		sc->sc_isr.isr_func = frodointr;
		sc->sc_isr.isr_arg = sc;
		sc->sc_isr.isr_ipl = FRODO_IPL;
		sc->sc_isr.isr_priority = newpri;
		intr_establish(&sc->sc_isr, sc->sc_dev.dv_xname);
	}
}

int
frodointr(arg)
	void *arg;
{
	struct frodo_softc *sc = arg;
	struct isr *fisr;
	int line, taken = 0;

	/* Any interrupts pending? */
	if (FRODO_GETPEND(sc) == 0)
		return (0);

	do {
		/*
		 * Get pending interrupt; this also clears it for us.
		 */
		line = FRODO_IPEND(sc);
		fisr = sc->sc_intr[line];
		if (fisr == NULL) {
			printf("%s: unhandled interrupt on line %d\n",
			    sc->sc_dev.dv_xname, line);
		} else {
			if ((*fisr->isr_func)(fisr->isr_arg) != 0) {
				fisr->isr_count.ec_count++;
			} else {
				printf("%s: spurious interrupt on line %d\n",
				    sc->sc_dev.dv_xname, line);
			}
		}
		if (taken++ > 100)
			panic("frodointr: looping, line %d fisr %p", line, fisr);
	} while (FRODO_GETPEND(sc) != 0);

	return (1);
}

void
frodo_imask(sc, set, clear)
	struct frodo_softc *sc;
	u_int16_t set, clear;
{
	u_int16_t imask;

	imask = FRODO_GETMASK(sc);

	imask |= set;
	imask &= ~clear;

	FRODO_SETMASK(sc, imask);
}

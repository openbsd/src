/*	$OpenBSD: daadio.c,v 1.1 1999/07/23 19:11:24 jason Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
 * All rights reserved.
 *
 * This software was developed by Jason L. Wright under contract with
 * RTMX Incorporated (http://www.rtmx.com).
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
 *	This product includes software developed by Jason L. Wright for
 *	RTMX Incorporated.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for the MATRIX Corporation MD-DAADIO digital->analog,
 * analog->digial, parallel i/o VME board.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <sparc/cpu.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/dev/sbusvar.h>
#include <sparc/dev/dmareg.h>	/* for SBUS_BURST_* */

#include <sparc/dev/fgareg.h>
#include <sparc/dev/fgavar.h>
#include <sparc/dev/daadioreg.h>

int	daadiomatch	__P((struct device *, void *, void *));
void	daadioattach	__P((struct device *, struct device *, void *));
int	daadiointr	__P((void *));

struct daadio_softc {
	struct		device sc_dv;		/* base device */
	struct		daadioregs *sc_regs;	/* registers */
	struct		intrhand sc_ih;		/* interrupt vectoring */
	u_int8_t	sc_ier;			/* software copy of ier */
};

struct cfattach daadio_ca = {
	sizeof (struct daadio_softc), daadiomatch, daadioattach
};

struct cfdriver daadio_cd = {
	NULL, "daadio", DV_DULL
};

void	daadio_ier_setbit __P((struct daadio_softc *, u_int8_t));
void	daadio_ier_clearbit __P((struct daadio_softc *, u_int8_t));

int
daadiomatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct daadioregs *regs;

	if (ca->ca_bustype != BUS_FGA_A16D16)
		return (0);

	if (strcmp(ca->ca_ra.ra_name, cf->cf_driver->cd_name))
		return (0);

	if (ca->ca_ra.ra_reg[0].rr_len < sizeof(struct daadioboard))
		return (0);

	regs = ca->ca_ra.ra_reg[0].rr_paddr;
	if (probeget((caddr_t)&regs->sid, 1) != -1)
		return (1);

	return (0);
}

void    
daadioattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct daadio_softc *sc = (struct daadio_softc *)self;

	sc->sc_regs = mapiodev(&(ca->ca_ra.ra_reg[0]), 0,
	    sizeof(struct daadioboard));

	sc->sc_regs->pio_pattern = sc->sc_regs->pio_porta;
	sc->sc_regs->sid = ca->ca_ra.ra_intr[0].int_vec;
	sc->sc_regs->gvrilr &= ~ILR_IRQ_MASK;
	sc->sc_regs->gvrilr |= (ca->ca_ra.ra_intr[0].int_pri << ILR_IRQ_SHIFT)
	    & ILR_IRQ_MASK;
	sc->sc_ih.ih_fun = daadiointr;
	sc->sc_ih.ih_arg = sc;
	fvmeintrestablish(parent, ca->ca_ra.ra_intr[0].int_vec,
	    ca->ca_ra.ra_intr[0].int_pri, &sc->sc_ih);
	daadio_ier_setbit(sc, IER_PIOEVENT);

	printf(": level %d vec 0x%x\n",
	    ca->ca_ra.ra_intr[0].int_pri, ca->ca_ra.ra_intr[0].int_vec);
}

int
daadiointr(vsc)
	void *vsc;
{
	struct daadio_softc *sc = vsc;
	struct daadioregs *regs = sc->sc_regs;
	u_int8_t val;
	int r = 0;

	if (regs->isr & ISR_PIOEVENT) {
		val = regs->pio_porta;
		printf("pio value: %x\n", val);
		r |= 1;
		regs->pio_pattern = val;
	}

	return (r);
}

void
daadio_ier_setbit(sc, v)
	struct daadio_softc *sc;
	u_int8_t v;
{
	sc->sc_ier |= v;
	sc->sc_regs->ier = sc->sc_ier;
}

void
daadio_ier_clearbit(sc, v)
	struct daadio_softc *sc;
	u_int8_t v;
{
	sc->sc_ier &= ~v;
	sc->sc_regs->ier = sc->sc_ier;
}

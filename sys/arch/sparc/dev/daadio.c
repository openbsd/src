/*	$OpenBSD: daadio.c,v 1.6 2004/09/29 07:35:11 miod Exp $	*/

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
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/proc.h>

#include <machine/autoconf.h>
#include <sparc/cpu.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/dev/sbusvar.h>
#include <sparc/dev/dmareg.h>	/* for SBUS_BURST_* */
#include <machine/daadioio.h>

#include <sparc/dev/fgareg.h>
#include <sparc/dev/fgavar.h>
#include <sparc/dev/daadioreg.h>

int	daadiomatch(struct device *, void *, void *);
void	daadioattach(struct device *, struct device *, void *);
int	daadiointr(void *);

struct daadio_softc {
	struct		device sc_dv;		/* base device */
	struct		daadioregs *sc_regs;	/* registers */
	struct		intrhand sc_ih;		/* interrupt vectoring */
	struct		daadio_adc *sc_adc_p;
	int		sc_adc_done;
	int		sc_flags;		/* flags */
#define	DAF_LOCKED	0x1
#define	DAF_WANTED	0x2
	u_int16_t	sc_adc_val;
	u_int8_t	sc_ier;			/* software copy of ier */
};

struct cfattach daadio_ca = {
	sizeof (struct daadio_softc), daadiomatch, daadioattach
};

struct cfdriver daadio_cd = {
	NULL, "daadio", DV_DULL
};

void	daadio_ier_setbit(struct daadio_softc *, u_int8_t);
void	daadio_ier_clearbit(struct daadio_softc *, u_int8_t);
int	daadio_adc(struct daadio_softc *, struct daadio_adc *);

int	daadioopen(dev_t, int, int, struct proc *);
int	daadioclose(dev_t, int, int, struct proc *);
int	daadioioctl(dev_t, u_long, caddr_t, int, struct proc *);

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
	    ca->ca_ra.ra_intr[0].int_pri, &sc->sc_ih, self->dv_xname);
	daadio_ier_setbit(sc, IER_PIOEVENT);
	daadio_ier_setbit(sc, IER_CONVERSION);

	printf(": level %d vec 0x%x\n",
	    ca->ca_ra.ra_intr[0].int_pri, ca->ca_ra.ra_intr[0].int_vec);
}

int
daadiointr(vsc)
	void *vsc;
{
	struct daadio_softc *sc = vsc;
	struct daadioregs *regs = sc->sc_regs;
	u_int8_t val, isr;
	int r = 0;

	isr = regs->isr;

	if (isr & ISR_PIOEVENT) {
		val = regs->pio_porta;
		printf("pio value: %x\n", val);
		r = 1;
		regs->pio_pattern = val;
	}

	if (isr & ISR_CONVERSION) {
		r = 1;
		sc->sc_adc_val = sc->sc_regs->adc12bit[0];
		sc->sc_adc_done = 1;
		if (sc->sc_adc_p != NULL)
			wakeup(sc->sc_adc_p);
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

#define	DAADIO_CARD(d)	((minor(d) >> 6) & 3)

int
daadioopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	struct daadio_softc *sc;
	int card = DAADIO_CARD(dev);

	if (card >= daadio_cd.cd_ndevs)
		return (ENXIO);
	sc = daadio_cd.cd_devs[card];
	if (sc == NULL)
		return (ENXIO);
	return (0);
}

int
daadioclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	return (0);
}

int
daadioioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct daadio_softc *sc = daadio_cd.cd_devs[DAADIO_CARD(dev)];
	struct daadio_adc *adc = (struct daadio_adc *)data;
	struct daadio_dac *dac = (struct daadio_dac *)data;
	struct daadio_pio *pio = (struct daadio_pio *)data;
	int error = 0;
	u_int8_t reg, val;

	switch (cmd) {
	case DIOGADC:
		error = daadio_adc(sc, adc);
		break;
	case DIOGPIO:
		switch (pio->dap_reg) {
		case 0:
			pio->dap_val = sc->sc_regs->pio_porta;
			break;
		case 1:
			pio->dap_val = sc->sc_regs->pio_portb;
			break;
		case 2:
			pio->dap_val = sc->sc_regs->pio_portc;
			break;
		case 3:
			pio->dap_val = sc->sc_regs->pio_portd;
			break;
		case 4:
			pio->dap_val = sc->sc_regs->pio_porte;
			break;
		case 5:
			pio->dap_val = sc->sc_regs->pio_portf;
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	case DIOSPIO:
		if ((flags & FWRITE) == 0) {
			error = EPERM;
			break;
		}
		switch (pio->dap_reg) {
		case 0:
			sc->sc_regs->pio_porta = pio->dap_val;
			break;
		case 1:
			sc->sc_regs->pio_portb = pio->dap_val;
			break;
		case 2:
			sc->sc_regs->pio_portc = pio->dap_val;
			break;
		case 3:
			sc->sc_regs->pio_portd = pio->dap_val;
			break;
		case 4:
			sc->sc_regs->pio_porte = pio->dap_val;
			break;
		case 5:
			sc->sc_regs->pio_portf = pio->dap_val;
			break;
		default:
			error = EINVAL;
			break;
		}
		break;
	case DIOSOPIO:
		if ((flags & FWRITE) == 0) {
			error = EPERM;
			break;
		}
		if (pio->dap_reg >= 6) {
			error = EINVAL;
			break;
		}

		reg = sc->sc_regs->pio_oc;
		val = (1 << pio->dap_reg);

		if (pio->dap_val)
			reg |= val;
		else
			reg &= ~val;

		sc->sc_regs->pio_oc = reg;
		break;
	case DIOGOPIO:
		if (pio->dap_reg >= 6) {
			error = EINVAL;
			break;
		}

		reg = sc->sc_regs->pio_oc;
		val = (1 << pio->dap_reg);

		if ((reg & val) != 0)
			pio->dap_val = 1;
		else
			pio->dap_val = 0;
		break;
	case DIOSDAC:
		if ((flags & FWRITE) == 0) {
			error = EPERM;
			break;
		}
		if (dac->dac_reg >= 8) {
			error = EINVAL;
			break;
		}
		sc->sc_regs->dac_channel[dac->dac_reg] = dac->dac_val;
		break;
	default:
		error = ENOTTY;
	}

	return (error);
}

int
daadio_adc(sc, adc)
	struct daadio_softc *sc;
	struct daadio_adc *adc;
{
	int s, err = 0;

	if (adc->dad_reg >= 32)
		return (EINVAL);

	s = splhigh();

	/* Lock device. */
	while ((sc->sc_flags & DAF_LOCKED) != 0) {
		sc->sc_flags |= DAF_WANTED;
		if ((err = tsleep(sc, PWAIT, "daadio", 0)) != 0)
			goto out;
	}
	sc->sc_flags |= DAF_LOCKED;

	/* Start conversion. */
	sc->sc_adc_done = 0;
	sc->sc_adc_p = adc;
	sc->sc_regs->adc12bit[adc->dad_reg] = 0;

	/* Wait for conversion. */
	while (sc->sc_adc_done == 0)
		if ((err = tsleep(sc->sc_adc_p, PWAIT, "daadio", 0)) != 0)
			goto out;
	sc->sc_adc_p = NULL;
	adc->dad_val = sc->sc_adc_val;

	/* Unlock device. */
	sc->sc_flags &= ~DAF_LOCKED;
	if (sc->sc_flags & DAF_WANTED) {
		sc->sc_flags &= ~DAF_WANTED;
		wakeup(sc);
	}

out:
	splx(s);
	return (err);
}

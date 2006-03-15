/*	$OpenBSD: scf.c,v 1.10 2006/03/15 20:03:06 miod Exp $	*/

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
 * Driver for the flash memory and sysconfig registers found on Force CPU-5V
 * boards.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/timeout.h>

#include <machine/autoconf.h>
#include <sparc/cpu.h>
#include <sparc/sparc/cpuvar.h>
#include <machine/scfio.h>
#include <sparc/dev/scfreg.h>

int	scfmatch(struct device *, void *, void *);
void	scfattach(struct device *, struct device *, void *);

int	scfopen(dev_t, int, int, struct proc *);
int	scfclose(dev_t, int, int, struct proc *);
int	scfioctl(dev_t, u_long, caddr_t, int, struct proc *);

struct scf_softc {
	struct	device sc_dv;			/* base device */
	struct	scf_regs *sc_regs;	/* our registers */
	int	sc_open;
	int	sc_tick;
	struct	timeout sc_blink_tmo;	/* for scfblink() */
};

struct cfattach scf_ca = {
	sizeof (struct scf_softc), scfmatch, scfattach
};

struct cfdriver scf_cd = {
	NULL, "scf", DV_DULL
};

extern int sparc_led_blink;

static const u_int8_t scf_pattern[] = {
	SSLDCR_A|SSLDCR_B|SSLDCR_C|SSLDCR_D|SSLDCR_E|SSLDCR_F,
	SSLDCR_B|SSLDCR_C,
	SSLDCR_A|SSLDCR_B|SSLDCR_D|SSLDCR_E|SSLDCR_G,
	SSLDCR_A|SSLDCR_B|SSLDCR_C|SSLDCR_D|SSLDCR_G,
	SSLDCR_B|SSLDCR_C|SSLDCR_F|SSLDCR_G,
	SSLDCR_A|SSLDCR_C|SSLDCR_D|SSLDCR_F|SSLDCR_G,
	SSLDCR_A|SSLDCR_C|SSLDCR_D|SSLDCR_E|SSLDCR_F|SSLDCR_G,
	SSLDCR_A|SSLDCR_B|SSLDCR_C,
	SSLDCR_A|SSLDCR_B|SSLDCR_C|SSLDCR_D|SSLDCR_E|SSLDCR_F|SSLDCR_G,
	SSLDCR_A|SSLDCR_B|SSLDCR_C|SSLDCR_D|SSLDCR_F|SSLDCR_G,
	SSLDCR_A|SSLDCR_B|SSLDCR_C|SSLDCR_E|SSLDCR_F|SSLDCR_G,
	SSLDCR_C|SSLDCR_D|SSLDCR_E|SSLDCR_F|SSLDCR_G,
	SSLDCR_A|SSLDCR_D|SSLDCR_E|SSLDCR_F,
	SSLDCR_B|SSLDCR_C|SSLDCR_D|SSLDCR_E|SSLDCR_G,
	SSLDCR_A|SSLDCR_D|SSLDCR_E|SSLDCR_F|SSLDCR_G,
	SSLDCR_A|SSLDCR_E|SSLDCR_F|SSLDCR_G,
};

int
scfmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (strcmp("sysconfig", ra->ra_name))
		return (0);

	return (1);
}

void    
scfattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct scf_softc *sc = (struct scf_softc *)self;
	char *s;

	/* map registers */
	if (ca->ca_ra.ra_nreg != 1) {
		printf(": expected 1 register, got %d\n", ca->ca_ra.ra_nreg);
		return;
	}

	sc->sc_regs = mapiodev(&(ca->ca_ra.ra_reg[0]), 0,
			ca->ca_ra.ra_reg[0].rr_len);

	s = getpropstring(ca->ca_ra.ra_node, "model");
	printf(": model %s\n", s);

	sc->sc_regs->led1 &= ~LED_MASK;
	sc->sc_regs->led2 &= ~LED_MASK;
	sc->sc_regs->ssldcr = 0;

	timeout_set(&sc->sc_blink_tmo, scfblink, 0);

	if (sparc_led_blink)
		scfblink(0);
}

int
scfopen(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	struct scf_softc *sc;
	int card = 0;

	if (card >= scf_cd.cd_ndevs)
		return (ENXIO);
	sc = scf_cd.cd_devs[card];
	if (sc == NULL)
		return (ENXIO);
	if (sc->sc_open)
		return (EBUSY);

	sc->sc_open = 1;
	return (0);
}

int
scfclose(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	struct scf_softc *sc;
	int card = 0;

	sc = scf_cd.cd_devs[card];
	sc->sc_open = 0;
	return (0);
}

int
scfioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct scf_softc *sc = scf_cd.cd_devs[0];
	u_int8_t *ptr = (u_int8_t *)data, c;
	int error = 0;

	switch (cmd) {
	case SCFIOCSLED1:
		sc->sc_regs->led1 = LED_MASK | (*ptr);
		break;
	case SCFIOCGLED1:
		*ptr = sc->sc_regs->led1 & (LED_COLOR_MASK | LED_BLINK_MASK);
		break;
	case SCFIOCSLED2:
		sc->sc_regs->led2 = LED_MASK | (*ptr);
		break;
	case SCFIOCGLED2:
		*ptr = sc->sc_regs->led2 & (LED_COLOR_MASK | LED_BLINK_MASK);
		break;
	case SCFIOCSLED7:
		sc->sc_regs->ssldcr = *ptr;
		break;
	case SCFIOCGLED7:
		*ptr = sc->sc_regs->ssldcr;
		break;
	case SCFIOCGROT:
		*ptr = sc->sc_regs->rssr;
		break;
	case SCFIOCSFMCTRL:
		if ((*ptr) & SCF_FMCTRL_SELROM)
			sc->sc_regs->fmpcr1 |= FMPCR1_SELROM;
		else
			sc->sc_regs->fmpcr1 &= ~FMPCR1_SELROM;

		if ((*ptr) & SCF_FMCTRL_SELBOOT)
			sc->sc_regs->fmpcr2 |= FMPCR2_SELBOOT;
		else
			sc->sc_regs->fmpcr2 &= ~FMPCR2_SELBOOT;

		if ((*ptr) & SCF_FMCTRL_WRITEV)
			sc->sc_regs->fmpvcr |= FMPVCR_VPP;
		else
			sc->sc_regs->fmpvcr &= ~FMPVCR_VPP;

		c = ((*ptr) & SCF_FMCTRL_SELADDR) >> 3;
		sc->sc_regs->fmpcr1 =
		    (sc->sc_regs->fmpcr1 & ~FMPCR1_SELADDR) | (c << 1);

		break;
	case SCFIOCGFMCTRL:
		c = (sc->sc_regs->fmpcr1 & FMPCR1_SELADDR) << 2;
		if (sc->sc_regs->fmpcr1 & FMPCR1_SELROM)
			c |= SCF_FMCTRL_SELROM;
		if (sc->sc_regs->fmpcr2 & FMPCR2_SELBOOT)
			c |= SCF_FMCTRL_SELBOOT;
		if (sc->sc_regs->fmpvcr & FMPVCR_VPP)
			c |= SCF_FMCTRL_WRITEV;
		*ptr = c;
		break;
	default:
		error = ENOTTY;
	}

	return (error);
}

void
scfblink(v)
        void *v;
{
	struct scf_softc *sc;
	int s, avg, hi = 0;

	if (scf_cd.cd_ndevs == 0)
		return;

	sc = scf_cd.cd_devs[0];
	if (sc == NULL)
		return;

	if (sparc_led_blink == 0) {
		sc->sc_regs->led1 &= ~LED_MASK;
		sc->sc_regs->led2 &= ~LED_MASK;
		sc->sc_regs->ssldcr = 0;
		return;
	}

	avg = averunnable.ldavg[0] >> FSHIFT;
	while (avg > 15) {
		hi = 1;
		avg >>= 4;
	}

	s = splhigh();
	if (sc->sc_tick & 1) {
		sc->sc_regs->led1 &= ~LED_MASK;
		sc->sc_regs->led2 |= LED_COLOR_GREEN;
	}
	else {
		sc->sc_regs->led1 |= LED_COLOR_YELLOW;
		sc->sc_regs->led2 &= ~LED_MASK;
	}
	sc->sc_regs->ssldcr = scf_pattern[avg] | (hi ? SSLDCR_P : 0);
	splx(s);

	sc->sc_tick++;

	s = ((averunnable.ldavg[0] + FSCALE) * hz) >> (FSHIFT + 1);
	timeout_add(&sc->sc_blink_tmo, s);
}

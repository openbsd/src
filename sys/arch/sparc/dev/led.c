/*	$OpenBSD: led.c,v 1.13 2010/07/10 19:32:24 miod Exp $	*/

/*
 * Copyright (c) 1998 Jason L. Wright (jason@thought.net)
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
 * Driver for leds on the sun4, sun4e, and 4/600 (sun4m) systems.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/timeout.h>

#include <machine/autoconf.h>
#include <machine/ctlreg.h>
#include <machine/vmparam.h>
#include <sparc/sparc/asm.h>
#include <sparc/cpu.h>
#include <sparc/sparc/auxioreg.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/dev/led.h>

int	ledmatch(struct device *, void *, void *);
void	ledattach(struct device *, struct device *, void *);
void	led_cycle(void *);

struct cfattach led_ca = {
	sizeof (struct led_softc), ledmatch, ledattach
};

struct cfdriver led_cd = {
	NULL, "led", DV_DULL
};

static u_int8_t led_pattern[] = {
	0xfe, 0xfd, 0xfb, 0xf7, 0xef, 0xdf, 0xbf, 0x7f,
	0xbf, 0xdf, 0xef, 0xf7, 0xfb, 0xfd,
};

struct led_softc *led_sc = NULL;
extern int sparc_led_blink;	/* from machdep */

int
ledmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
#if defined(SUN4) || defined(SUN4E)
	struct cfdata *cf = vcf;
#endif
#if defined(SUN4) || defined(SUN4E) || defined(SUN4M)
	struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

#if defined(SUN4M)
	if (ca->ca_bustype == BUS_OBIO) {
		if (strcmp("leds", ra->ra_name))
			return (0);
		return (1);
	}
#endif /* SUN4M */

#if defined(SUN4) || defined(SUN4E)
	if (ca->ca_bustype == BUS_MAIN) {
		if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
			return (0);
		if (CPU_ISSUN4OR4E)
			return (1);
		return (0);
	}
#endif /* SUN4 || SUN4E*/
#endif /* SUN4 || SUN4E || SUN4M */

	return (0);
}

void    
ledattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct led_softc *sc = (struct led_softc *)self;

	sc->sc_node = ca->ca_ra.ra_node;

	timeout_set(&sc->sc_to, led_cycle, sc);

	if (CPU_ISSUN4M)
		sc->sc_reg = mapiodev(&(ca->ca_ra.ra_reg[0]), 0,
		    ca->ca_ra.ra_reg[0].rr_len);

	led_sc = sc;
	if (sparc_led_blink)
		led_cycle(sc);
	printf("\n");
}

void
led_cycle(v)
	void *v;
{
	struct led_softc *sc = v;
	int s;

	if (sc == NULL)
		return;

	sc->sc_index = (sc->sc_index + 1) %
			(sizeof(led_pattern)/sizeof(led_pattern[0]));

	if (sparc_led_blink == 0)
		sc->sc_index = 0;

	s = splhigh();
	switch (cputyp) {
	default:
#if defined(SUN4M)
	case CPU_SUN4M:
		(*sc->sc_reg) = led_pattern[sc->sc_index] | 0xff00;
		break;
#endif
#if defined(SUN4E)
	case CPU_SUN4E:
		*(volatile u_char *)(AUXREG_VA) = led_pattern[sc->sc_index];
		break;
#endif
#if defined(SUN4)
	case CPU_SUN4:
		stba(AC_DIAG_REG, ASI_CONTROL, led_pattern[sc->sc_index]);
		break;
#endif
	}
	splx(s);

	if (sparc_led_blink != 0) {
		s = (((averunnable.ldavg[0] + FSCALE) * hz) >> (FSHIFT + 3));
		timeout_add(&sc->sc_to, s);
	}
}

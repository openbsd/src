/*	$OpenBSD: led.c,v 1.5 1999/03/01 04:56:05 jason Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
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
 * Driver for leds on the 4/100, 4/200, 4/300, and 4/600. (sun4 & sun4m)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <machine/ctlreg.h>
#include <sparc/sparc/asm.h>
#include <sparc/cpu.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/dev/led.h>

int	ledmatch	__P((struct device *, void *, void *));
void	ledattach	__P((struct device *, struct device *, void *));
void	led_cycle	__P((void *));

struct cfattach led_ca = {
	sizeof (struct led_softc), ledmatch, ledattach
};

struct cfdriver led_cd = {
	NULL, "led", DV_IFNET
};

static u_int8_t led_pattern[] = {
	0xff, 0xfe, 0xfd, 0xfb, 0xf7, 0xef, 0xdf, 0xbf, 0x7f,
	0xff, 0x7f, 0xbf, 0xdf, 0xef, 0xf7, 0xfb, 0xfd, 0xfe,
};

struct led_softc *led_sc = NULL;
extern int sparc_led_blink;	/* from machdep */

int
ledmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
#if defined(SUN4)
	struct cfdata *cf = vcf;
#endif
	struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

#if defined(SUN4M)
	if (ca->ca_bustype == BUS_OBIO) {
		if (strcmp("leds", ra->ra_name))
			return (0);
		return (1);
	}
#endif

#if defined(SUN4)
	if (ca->ca_bustype == BUS_MAIN) {
		if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
			return (0);
		if (CPU_ISSUN4)
			return (1);
		return (0);
	}
#endif

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

#if defined(SUN4M)
	if (CPU_ISSUN4M) {
		s = splhigh();
		(*sc->sc_reg) = led_pattern[sc->sc_index] | 0xff00;
		splx(s);
	}
#endif

#if defined(SUN4)
	if (CPU_ISSUN4) {
		s = splhigh();
		stba(AC_DIAG_REG, ASI_CONTROL, led_pattern[sc->sc_index]);
		splx(s);
	}
#endif

	if (sparc_led_blink != 0) {
		s = (((averunnable.ldavg[0] + FSCALE) * hz) >> (FSHIFT + 3));
		timeout(led_cycle, sc, s);
	}
}

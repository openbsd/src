/*	$OpenBSD: led.c,v 1.6 2008/08/20 18:50:17 miod Exp $	*/
/*	$NetBSD: leds.c,v 1.4 2005/12/11 12:19:37 christos Exp $	*/

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
/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross and der Mouse.
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
 * Functions to flash the LEDs with some pattern.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/timeout.h>

#include <machine/cpu.h>
#include <machine/nexus.h>
#include <machine/sid.h>

#if VAX60
#include <arch/vax/mbus/mbusreg.h>
#include <arch/vax/mbus/mbusvar.h>
#endif

struct led_softc {
	struct device	sc_dev;
	struct timeout	sc_tmo;
	volatile u_short *sc_reg;
	const u_int8_t	*sc_pat, *sc_patpos;
};

/*
 * Patterns for 8 and 4 led displays.
 */
static const u_int8_t led_pattern8[] = {
	0xfe, 0xfd, 0xfb, 0xf7, 0xef, 0xdf, 0xbf, 0x7f,
	0xbf, 0xdf, 0xef, 0xf7, 0xfb, 0xfd, 0x00
};
static const u_int8_t led_pattern4[] = {
	0x0e, 0x0d, 0x0b, 0x07, 0x0b, 0x0d, 0x00
};

int	ledmatch(struct device *, void *, void *);
void	ledattach(struct device *, struct device *, void *);
void	led_blink(void *);

struct cfattach led_ca = {
	sizeof(struct led_softc), ledmatch, ledattach
};

struct cfdriver led_cd = {
	NULL, "led", DV_DULL
};

int
ledmatch(struct device *parent, void *cf, void *aux)
{
	struct mainbus_attach_args *maa = aux;

	if (maa->maa_bustype != VAX_LEDS)
		return (0);
	
	switch (vax_boardtype) {
#if VAX46 || VAX48 || VAX49 || VAX53 || VAX60 || VXT
#if VAX46
	case VAX_BTYP_46:
#endif
#if VAX48
	case VAX_BTYP_48:
#endif
#if VAX49
	case VAX_BTYP_49:
#endif
#if VAX53
	case VAX_BTYP_1303:
#endif
#if VAX60
	case VAX_BTYP_60:
#endif
#if VXT
	case VAX_BTYP_VXT:
#endif
		return (1);
#endif
	default:
		return (0);
	}
}

void
ledattach(struct device *parent, struct device *self, void *aux)
{
	struct led_softc *sc = (void *)self;
#if VAX49 || VAX53 || VXT
	vaddr_t pgva;
#endif

	printf("\n");

	switch (vax_boardtype) {
#if VAX46
	case VAX_BTYP_46:
	{
		extern struct vs_cpu *ka46_cpu;
		sc->sc_reg = (volatile u_short *)(&ka46_cpu->vc_diagdsp);
		sc->sc_pat = led_pattern8;
	}
		break;
#endif
#if VAX48
	case VAX_BTYP_48:
	{
		extern struct vs_cpu *ka48_cpu;
		sc->sc_reg = (volatile u_short *)(&ka48_cpu->vc_diagdsp);
		sc->sc_pat = led_pattern8;
	}
		break;
#endif
#if VAX49
	case VAX_BTYP_49:
		pgva = vax_map_physmem(0x25800000, 1);
		sc->sc_reg = (volatile u_short *)(pgva + 4);
		sc->sc_pat = led_pattern8;
		break;
#endif
#if VAX53
	case VAX_BTYP_1303:
		pgva = vax_map_physmem(0x20140000, 1);
		sc->sc_reg = (volatile u_short *)(pgva + 0x30);
		sc->sc_pat = led_pattern4;
		break;
#endif
#if VAX60
	case VAX_BTYP_60:
		pgva = vax_map_physmem(MBUS_SLOT_BASE(mbus_ioslot) + FBIC_BASE,
		    1);
		sc->sc_reg = (volatile u_short *)(pgva + FBIC_CSR);
		sc->sc_pat = NULL;
		break;
#endif
#if VXT
	case VAX_BTYP_VXT:
		pgva = vax_map_physmem(0x200c1000, 1);
		sc->sc_reg = (volatile u_short *)pgva;
		sc->sc_pat = led_pattern8;
		break;
#endif
	}

	sc->sc_patpos = sc->sc_pat;
	timeout_set(&sc->sc_tmo, led_blink, sc);
	led_blink(sc);
}

void
led_blink(void *v)
{
	struct led_softc *sc = v;
	extern int vax_led_blink;

	if (sc == NULL) {
		/* find our softc if we come from cpu_sysctl */
		if (led_cd.cd_ndevs != 0)
			sc = (struct led_softc *)led_cd.cd_devs[0];
		if (sc == NULL)
			return;
	}

	if (sc->sc_pat != NULL) {
		if (vax_led_blink == 0) {
			*sc->sc_reg = 0xff;
			return;
		}

		*sc->sc_reg = *sc->sc_patpos++;
		if (*sc->sc_patpos == 0)
			sc->sc_patpos = sc->sc_pat;
	} else {
#if VAX60
		uint32_t fbicsr, dot, digit;

		fbicsr= *(volatile uint32_t *)sc->sc_reg;
		dot = ((fbicsr & FBICSR_LEDS_MASK) >> FBICSR_LEDS_SHIFT) & 0x10;
		fbicsr &= ~FBICSR_LEDS_MASK;

		if (vax_led_blink == 0) {
			fbicsr |= 0x1f << FBICSR_LEDS_SHIFT;
			*(volatile uint32_t *)sc->sc_reg = fbicsr;
			return;
		}

		/* this is supposed to flip the decimal dot... doesn't work */
		fbicsr |= (dot ^ 0x10) << FBICSR_LEDS_SHIFT;
		/* display the load average in the hex digit */
		digit = averunnable.ldavg[0] >> FSHIFT;
		if (digit > 0x0f)
			digit = 0x0f;
		fbicsr |= (0x0f ^ digit) << FBICSR_LEDS_SHIFT;

		*(volatile uint32_t *)sc->sc_reg = fbicsr;
#endif
	}

	timeout_add(&sc->sc_tmo,
	    (((averunnable.ldavg[0] + FSCALE) * hz) >> (FSHIFT + 3)));
}

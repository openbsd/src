/*	$OpenBSD: zaurus_apm.c,v 1.1 2005/01/20 23:34:37 uwe Exp $	*/

/*
 * Copyright (c) 2005 Uwe Stuehler <uwe@bsdx.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/conf.h>

#include <arm/xscale/pxa2x0_apm.h>
#include <arm/xscale/pxa2x0_gpio.h>

#include <zaurus/dev/zaurus_scoopvar.h>

int	apm_match(struct device *, void *, void *);
void	apm_attach(struct device *, struct device *, void *);

struct cfattach apm_pxaip_ca = {
        sizeof (struct pxa2x0_apm_softc), apm_match, apm_attach
};

#define C3000_GPIO_AC_IN	115	/* active low */

int	zaurus_ac_present(void);
void	zaurus_battery_charge(int);
void	zaurus_battery_info(struct pxaapm_battery_info *);
void	zaurus_power_check(struct pxa2x0_apm_softc *);

int
apm_match(struct device *parent, void *match, void *aux)
{
	return 1;
}

void
apm_attach(struct device *parent, struct device *self, void *aux)
{
	struct pxa2x0_apm_softc *sc = (struct pxa2x0_apm_softc *)self;

	sc->sc_battery_info = zaurus_battery_info;
	sc->sc_periodic_check = zaurus_power_check;

	pxa2x0_apm_attach_sub(sc);
}

int
zaurus_ac_present(void)
{

	return !pxa2x0_gpio_get_bit(C3000_GPIO_AC_IN);
}

void
zaurus_battery_charge(int on)
{

	scoop_led_set(SCOOP_LED_ORANGE, on);
}

void
zaurus_battery_info(struct pxaapm_battery_info *battp)
{

	if (zaurus_ac_present())
		battp->flags |= PXAAPM_AC_PRESENT;
	else
		battp->flags &= ~PXAAPM_AC_PRESENT;
}

void
zaurus_power_check(struct pxa2x0_apm_softc *sc)
{

	if (zaurus_ac_present())
		zaurus_battery_charge(1);
	else
		zaurus_battery_charge(0);
}

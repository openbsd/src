/*	$OpenBSD: palm_mmc.c,v 1.1 2009/09/05 01:22:11 marex Exp $	*/

/*
 * Copyright (c) 2009 Marek Vasut <marex@openbsd.org>
 * Copyright (c) 2007 Uwe Stuehler <uwe@openbsd.org>
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

/* Attachment driver for pxammc(4) on Palm */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <dev/sdmmc/sdmmcreg.h>
#include <machine/machine_reg.h>
#include <machine/palm_var.h>
#include <arch/arm/xscale/pxa2x0_gpio.h>

#include <arch/arm/xscale/pxammcvar.h>

int	palm_mmc_match(struct device *, void *, void *);
void	palm_mmc_attach(struct device *, struct device *, void *);

struct cfattach pxammc_palm_ca = {
	sizeof(struct pxammc_softc),
	palm_mmc_match,
	palm_mmc_attach
};

u_int32_t palm_mmc_get_ocr(void *);
int	  palm_mmc_set_power(void *, u_int32_t);

int
palm_mmc_match(struct device *parent, void *match, void *aux)
{
	return pxammc_match();
}

void
palm_mmc_attach(struct device *parent, struct device *self, void *aux)
{
	struct pxammc_softc *sc = (struct pxammc_softc *)self;

	sc->tag.cookie = (void *)sc;
	sc->tag.get_ocr = palm_mmc_get_ocr;
	sc->tag.set_power = palm_mmc_set_power;

	sc->sc_gpio_detect = GPIO14_MMC_DETECT;

	pxammc_attach(sc, aux);
}

u_int32_t
palm_mmc_get_ocr(void *cookie)
{
	return MMC_OCR_3_2V_3_3V | MMC_OCR_3_3V_3_4V;
}

int
palm_mmc_set_power(void *cookie, u_int32_t ocr)
{
	if (ISSET(ocr, MMC_OCR_3_2V_3_3V | MMC_OCR_3_3V_3_4V)) {
		if (mach_is_palmz72)
			pxa2x0_gpio_clear_bit(GPIO98_PALMZ72_MMC_POWER);
		else
			pxa2x0_gpio_set_bit(GPIO114_MMC_POWER);
		return 0;
	} else if (ocr != 0) {
		printf("palm_mmc_set_power: unsupported OCR (%#x)\n", ocr);
		return EINVAL;
	} else {
		if (mach_is_palmz72)
			pxa2x0_gpio_set_bit(GPIO98_PALMZ72_MMC_POWER);
		else
			pxa2x0_gpio_clear_bit(GPIO114_MMC_POWER);
		return 0;
	}
}

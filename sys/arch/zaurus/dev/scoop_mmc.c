/*	$OpenBSD: scoop_mmc.c,v 1.2 2009/09/03 21:40:29 marex Exp $	*/

/*
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

/* Attachment driver for pxammc(4) on Zaurus */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <dev/sdmmc/sdmmcreg.h>

#include <machine/machine_reg.h>

#include <arch/arm/xscale/pxammcvar.h>
#include <arch/zaurus/dev/zaurus_scoopvar.h>

int	scoop_mmc_match(struct device *, void *, void *);
void	scoop_mmc_attach(struct device *, struct device *, void *);

struct cfattach pxammc_scoop_ca = {
	sizeof(struct pxammc_softc), scoop_mmc_match,
	scoop_mmc_attach
};

u_int32_t scoop_mmc_get_ocr(void *);
int	  scoop_mmc_set_power(void *, u_int32_t);

int
scoop_mmc_match(struct device *parent, void *match, void *aux)
{
	return pxammc_match();
}

void
scoop_mmc_attach(struct device *parent, struct device *self, void *aux)
{
	struct pxammc_softc *sc = (struct pxammc_softc *)self;

	sc->tag.cookie = (void *)sc;
	sc->tag.get_ocr = scoop_mmc_get_ocr;
	sc->tag.set_power = scoop_mmc_set_power;

	sc->sc_gpio_detect = GPIO_MMC_DETECT;

	pxammc_attach(sc, aux);
}

u_int32_t
scoop_mmc_get_ocr(void *cookie)
{
	return MMC_OCR_3_2V_3_3V | MMC_OCR_3_3V_3_4V;
}

int
scoop_mmc_set_power(void *cookie, u_int32_t ocr)
{
	if (ISSET(ocr, MMC_OCR_3_2V_3_3V | MMC_OCR_3_3V_3_4V)) {
		scoop_set_sdmmc_power(1);
		return 0;
	} else if (ocr != 0) {
		printf("scoop_mmc_set_power: unsupported OCR (%#x)\n", ocr);
		return EINVAL;
	} else {
		scoop_set_sdmmc_power(0);
		return 0;
	}
}

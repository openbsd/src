/*	$OpenBSD: zaurus_ssp.c,v 1.3 2005/01/31 02:22:17 uwe Exp $	*/

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
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_gpio.h>

#include <zaurus/dev/zaurus_sspvar.h>

#define GPIO_TG_CS_C3000	53
#define GPIO_MAX1111_CS_C3000	20
#define GPIO_ADS7846_CS_C3000	14	/* SSP SFRM */

#define	SSCR0_LZ9JG18		0x01ab
#define SSCR0_MAX1111		0x0387

struct zssp_softc {
	struct device sc_dev;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
};

int	zssp_match(struct device *, void *, void *);
void	zssp_attach(struct device *, struct device *, void *);

struct cfattach zssp_ca = {
	sizeof (struct zssp_softc), zssp_match, zssp_attach
};

struct cfdriver zssp_cd = {
	NULL, "zssp", DV_DULL
};

int
zssp_match(struct device *parent, void *match, void *aux)
{
	return 1;
}

void
zssp_attach(struct device *parent, struct device *self, void *aux)
{
	struct zssp_softc *sc = (struct zssp_softc *)self;

	sc->sc_iot = &pxa2x0_bs_tag;
	if (bus_space_map(sc->sc_iot, PXA2X0_SSP1_BASE, PXA2X0_SSP_SIZE,
	    0, &sc->sc_ioh))
		panic("can't map %s", sc->sc_dev.dv_xname);

	pxa2x0_clkman_config(CKEN_SSP, 1);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSP_SSCR0, SSCR0_LZ9JG18);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSP_SSCR1, 0);

	pxa2x0_gpio_set_function(GPIO_TG_CS_C3000, GPIO_OUT|GPIO_SET);
	pxa2x0_gpio_set_function(GPIO_MAX1111_CS_C3000, GPIO_OUT|GPIO_SET);
	pxa2x0_gpio_set_function(GPIO_ADS7846_CS_C3000, GPIO_OUT|GPIO_SET);

	printf("\n");
}

int
zssp_read_max1111(u_int32_t cmd)
{
	struct	zssp_softc *sc;
	int	voltage[2];
	int	i;
	int s;

	if (zssp_cd.cd_ndevs < 1 || zssp_cd.cd_devs[0] == NULL) {
		printf("zssp_read_max1111: not configured\n");
		return 0;
	}
	sc = (struct zssp_softc *)zssp_cd.cd_devs[0];

	s = splhigh();

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSP_SSCR0, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSP_SSCR0, SSCR0_MAX1111);

	pxa2x0_gpio_set_bit(GPIO_TG_CS_C3000);
	pxa2x0_gpio_set_bit(GPIO_ADS7846_CS_C3000);
	pxa2x0_gpio_clear_bit(GPIO_MAX1111_CS_C3000);

	delay(1);

	/* Send the command word and read a dummy word back. */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSP_SSDR, cmd);
	while ((bus_space_read_4(sc->sc_iot, sc->sc_ioh, SSP_SSSR)
	    & SSSR_TNF) != SSSR_TNF)
		/* poll */;
	/* XXX is this delay necessary? */
	delay(1);
	while ((bus_space_read_4(sc->sc_iot, sc->sc_ioh, SSP_SSSR)
	    & SSSR_RNE) != SSSR_RNE)
		/* poll */;
	i = bus_space_read_4(sc->sc_iot, sc->sc_ioh, SSP_SSDR);

	for (i = 0; i < 2; i++) {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSP_SSDR, 0);
		while ((bus_space_read_4(sc->sc_iot, sc->sc_ioh, SSP_SSSR)
		    & SSSR_TNF) != SSSR_TNF)
			/* poll */;
		/* XXX again, is this delay necessary? */
		delay(1);
		while ((bus_space_read_4(sc->sc_iot, sc->sc_ioh, SSP_SSSR)
		    & SSSR_RNE) != SSSR_RNE)
			/* poll */;
		voltage[i] = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    SSP_SSDR);         
	}

	pxa2x0_gpio_set_bit(GPIO_TG_CS_C3000);
	pxa2x0_gpio_set_bit(GPIO_ADS7846_CS_C3000);
	pxa2x0_gpio_set_bit(GPIO_MAX1111_CS_C3000);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSP_SSCR0, 0);

	/* XXX no idea what this means, but it's what Linux would do. */
	if ((voltage[0] & 0xc0) != 0 || (voltage[1] & 0x3f) != 0)
		voltage[0] = -1;
	else
		voltage[0] = ((voltage[0] << 2) & 0xfc) |
		    ((voltage[1] >> 6) & 0x03);

	splx(s);
	return voltage[0];
}

/* XXX - only does CS_ADS7846 */
u_int32_t pxa2x0_ssp_read_val(u_int32_t cmd);
u_int32_t
pxa2x0_ssp_read_val(u_int32_t cmd)
{
	struct	zssp_softc *sc;
	sc = (struct zssp_softc *)zssp_cd.cd_devs[0];
	unsigned int cr0;
	int s;
	u_int32_t val;

	if (zssp_cd.cd_ndevs < 1 || zssp_cd.cd_devs[0] == NULL) {
		printf("pxa2x0_ssp_read_val: not configured\n");
		return 0;
	}
	sc = (struct zssp_softc *)zssp_cd.cd_devs[0];

	s = splhigh();
	if (1 /* C3000 */) {
		cr0 =  0x06ab;
	} else {
		cr0 =  0x00ab;
	}
        bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSP_SSCR0, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSP_SSCR0, cr0);

	pxa2x0_gpio_set_bit(GPIO_TG_CS_C3000);
	pxa2x0_gpio_set_bit(GPIO_MAX1111_CS_C3000);
	pxa2x0_gpio_clear_bit(GPIO_ADS7846_CS_C3000);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSP_SSDR, cmd);

	while ((bus_space_read_4(sc->sc_iot, sc->sc_ioh, SSP_SSSR)
	    & SSSR_TNF) != SSSR_TNF)
		/* poll */;

	delay(1);

	while ((bus_space_read_4(sc->sc_iot, sc->sc_ioh, SSP_SSSR)
	    & SSSR_RNE) != SSSR_RNE)
		/* poll */;

	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, SSP_SSDR);

	pxa2x0_gpio_set_bit(GPIO_ADS7846_CS_C3000); /* deselect */

	splx(s);

	return val;
}

void
zssp_write_lz9jg18(u_int32_t data)
{
	int s;
	int i;
	int ssp_sclk;
	int ssp_sfrm;
	int ssp_txd;
	int ssp_rxd;

	/* XXX this creates a DAC command from a backlight duty value. */
	data = 0x40 | (data & 0x1f);

	if ((cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA27X) {
		/* C3000 */
		ssp_sclk = 19;
		ssp_sfrm = 14;
		ssp_txd = 87;
		ssp_rxd = 86;
	} else {
		ssp_sclk = 23;
		ssp_sfrm = 24;
		ssp_txd = 25;
		ssp_rxd = 26;
	}

	s = splhigh();

	pxa2x0_gpio_set_function(ssp_sfrm, GPIO_OUT | GPIO_SET);
	pxa2x0_gpio_set_function(ssp_sclk, GPIO_OUT | GPIO_CLR);
	pxa2x0_gpio_set_function(ssp_txd, GPIO_OUT | GPIO_CLR);
	pxa2x0_gpio_set_function(ssp_rxd, GPIO_IN);

	pxa2x0_gpio_set_bit(GPIO_MAX1111_CS_C3000);
	pxa2x0_gpio_set_bit(GPIO_ADS7846_CS_C3000);
	pxa2x0_gpio_clear_bit(GPIO_TG_CS_C3000);

	delay(10);
	
	for (i = 0; i < 8; i++) {
		if (data & 0x80)
			pxa2x0_gpio_set_bit(ssp_txd);
		else
			pxa2x0_gpio_clear_bit(ssp_txd);
		delay(10);
		pxa2x0_gpio_set_bit(ssp_sclk);
		delay(10);
		pxa2x0_gpio_clear_bit(ssp_sclk);
		delay(10);
		data <<= 1;
	}

	pxa2x0_gpio_clear_bit(ssp_txd);
	pxa2x0_gpio_set_bit(GPIO_TG_CS_C3000);

	/* XXX SFRM and RXD alternate functions are not restored here. */
	if ((cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA27X) {
		pxa2x0_gpio_set_function(ssp_sclk, GPIO_ALT_FN_1_OUT);
		pxa2x0_gpio_set_function(ssp_txd, GPIO_ALT_FN_1_OUT);
	} else {
		pxa2x0_gpio_set_function(ssp_sclk, GPIO_ALT_FN_2_OUT);
		pxa2x0_gpio_set_function(ssp_txd, GPIO_ALT_FN_2_OUT);
	}

	splx(s);
}

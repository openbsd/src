/*	$OpenBSD: zaurus_ssp.c,v 1.6 2005/04/08 21:58:49 uwe Exp $	*/

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

#define GPIO_ADS7846_CS_C3000	14	/* SSP SFRM */
#define GPIO_MAX1111_CS_C3000	20
#define GPIO_TG_CS_C3000	53

#define SSCR0_ADS7846_C3000	0x06ab
#define	SSCR0_LZ9JG18		0x01ab
#define SSCR0_MAX1111		0x0387

struct zssp_softc {
	struct device sc_dev;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
};

int	zssp_match(struct device *, void *, void *);
void	zssp_attach(struct device *, struct device *, void *);
void	zssp_init(void);
void	zssp_powerhook(int, void *);

int	zssp_read_max1111(u_int32_t);
u_int32_t zssp_read_ads7846(u_int32_t);
void	zssp_write_lz9jg18(u_int32_t);

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
	    0, &sc->sc_ioh)) {
		printf(": can't map bus space\n");
		return;
	}

	printf("\n");

	if (powerhook_establish(zssp_powerhook, sc) == NULL)
		printf("%s: can't establish power hook\n",
		    sc->sc_dev.dv_xname);

	zssp_init();
}

/*
 * Initialize the dedicated SSP unit and disable all chip selects.
 * This function is called with interrupts disabled.
 */
void
zssp_init(void)
{
	struct zssp_softc *sc;

	KASSERT(zssp_cd.cd_ndevs > 0 && zssp_cd.cd_devs[0] != NULL);
	sc = (struct zssp_softc *)zssp_cd.cd_devs[0];

	pxa2x0_clkman_config(CKEN_SSP, 1);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSP_SSCR0, SSCR0_LZ9JG18);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSP_SSCR1, 0);

	pxa2x0_gpio_set_function(GPIO_ADS7846_CS_C3000, GPIO_OUT|GPIO_SET);
	pxa2x0_gpio_set_function(GPIO_MAX1111_CS_C3000, GPIO_OUT|GPIO_SET);
	pxa2x0_gpio_set_function(GPIO_TG_CS_C3000, GPIO_OUT|GPIO_SET);
}

void
zssp_powerhook(int why, void *arg)
{
	int s;

	if (why == PWR_RESUME) {
		s = splhigh();
		zssp_init();
		splx(s);
	}
}

/*
 * Transmit a single data word to one of the ICs, keep the chip selected
 * afterwards, and don't wait for data to be returned in SSDR.  Interrupts
 * must be held off until zssp_ic_stop() gets called.
 */
void
zssp_ic_start(int ic, u_int32_t data)
{
	struct zssp_softc *sc;

	KASSERT(zssp_cd.cd_ndevs > 0 && zssp_cd.cd_devs[0] != NULL);
	sc = (struct zssp_softc *)zssp_cd.cd_devs[0];

	/* disable other ICs */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSP_SSCR0, 0);
	if (ic != ZSSP_IC_ADS7846)
		pxa2x0_gpio_set_bit(GPIO_ADS7846_CS_C3000);
	if (ic != ZSSP_IC_LZ9JG18)
		pxa2x0_gpio_set_bit(GPIO_TG_CS_C3000);
	if (ic != ZSSP_IC_MAX1111)
		pxa2x0_gpio_set_bit(GPIO_MAX1111_CS_C3000);

	/* activate the chosen one */
	switch (ic) {
	case ZSSP_IC_ADS7846:
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSP_SSCR0,
		    SSCR0_ADS7846_C3000);
		pxa2x0_gpio_clear_bit(GPIO_ADS7846_CS_C3000);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, SSP_SSDR, data);
		while ((bus_space_read_4(sc->sc_iot, sc->sc_ioh, SSP_SSSR)
		    & SSSR_TNF) != SSSR_TNF)
			/* poll */;
		break;
	case ZSSP_IC_LZ9JG18:
		pxa2x0_gpio_clear_bit(GPIO_TG_CS_C3000);
		break;
	case ZSSP_IC_MAX1111:
		pxa2x0_gpio_clear_bit(GPIO_MAX1111_CS_C3000);
		break;
	}
}

/*
 * Read the last value from SSDR and deactivate all chip-selects.
 */
u_int32_t
zssp_ic_stop(int ic)
{
	struct zssp_softc *sc;
	u_int32_t rv;

	KASSERT(zssp_cd.cd_ndevs > 0 && zssp_cd.cd_devs[0] != NULL);
	sc = (struct zssp_softc *)zssp_cd.cd_devs[0];

	switch (ic) {
	case ZSSP_IC_ADS7846:
		/* read result of last command */
		while ((bus_space_read_4(sc->sc_iot, sc->sc_ioh, SSP_SSSR)
		    & SSSR_RNE) != SSSR_RNE)
			/* poll */;
		rv = bus_space_read_4(sc->sc_iot, sc->sc_ioh, SSP_SSDR);
		break;
	case ZSSP_IC_LZ9JG18:
	case ZSSP_IC_MAX1111:
		/* last value received is irrelevant or undefined */
	default:
		rv = 0;
		break;
	}

	pxa2x0_gpio_set_bit(GPIO_ADS7846_CS_C3000);
	pxa2x0_gpio_set_bit(GPIO_TG_CS_C3000);
	pxa2x0_gpio_set_bit(GPIO_MAX1111_CS_C3000);

	return (rv);
}

/*
 * Activate one of the chip-select lines, transmit one word value in
 * each direction, and deactivate the chip-select again.
 */
u_int32_t
zssp_ic_send(int ic, u_int32_t data)
{

	switch (ic) {
	case ZSSP_IC_MAX1111:
		return (zssp_read_max1111(data));
	case ZSSP_IC_ADS7846:
		return (zssp_read_ads7846(data));
	case ZSSP_IC_LZ9JG18:
		zssp_write_lz9jg18(data);
		return 0;
	default:
		printf("zssp_ic_send: invalid IC %d\n", ic);
		return 0;
	}
}

int
zssp_read_max1111(u_int32_t cmd)
{
	struct zssp_softc *sc;
	int	voltage[2];
	int	i;
	int	s;

	KASSERT(zssp_cd.cd_ndevs > 0 && zssp_cd.cd_devs[0] != NULL);
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
u_int32_t
zssp_read_ads7846(u_int32_t cmd)
{
	struct zssp_softc *sc;

	sc = (struct zssp_softc *)zssp_cd.cd_devs[0];
	unsigned int cr0;
	int s;
	u_int32_t val;

	if (zssp_cd.cd_ndevs < 1 || zssp_cd.cd_devs[0] == NULL) {
		printf("zssp_read_ads7846: not configured\n");
		return 0;
	}
	sc = (struct zssp_softc *)zssp_cd.cd_devs[0];

	s = splhigh();
	if (1) {
		cr0 =  SSCR0_ADS7846_C3000;
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
	int sclk_pin, sclk_fn;
	int sfrm_pin, sfrm_fn;
	int txd_pin, txd_fn;
	int rxd_pin, rxd_fn;
	int i;

	/* XXX this creates a DAC command from a backlight duty value. */
	data = 0x40 | (data & 0x1f);

	if ((cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA27X) {
		/* C3000 */
		sclk_pin = 19;
		sfrm_pin = 14;
		txd_pin = 87;
		rxd_pin = 86;
	} else {
		sclk_pin = 23;
		sfrm_pin = 24;
		txd_pin = 25;
		rxd_pin = 26;
	}

	s = splhigh();

	sclk_fn = pxa2x0_gpio_get_function(sclk_pin);
	sfrm_fn = pxa2x0_gpio_get_function(sfrm_pin);
	txd_fn = pxa2x0_gpio_get_function(txd_pin);
	rxd_fn = pxa2x0_gpio_get_function(rxd_pin);

	pxa2x0_gpio_set_function(sfrm_pin, GPIO_OUT | GPIO_SET);
	pxa2x0_gpio_set_function(sclk_pin, GPIO_OUT | GPIO_CLR);
	pxa2x0_gpio_set_function(txd_pin, GPIO_OUT | GPIO_CLR);
	pxa2x0_gpio_set_function(rxd_pin, GPIO_IN);

	pxa2x0_gpio_set_bit(GPIO_MAX1111_CS_C3000);
	pxa2x0_gpio_set_bit(GPIO_ADS7846_CS_C3000);
	pxa2x0_gpio_clear_bit(GPIO_TG_CS_C3000);

	delay(10);
	
	for (i = 0; i < 8; i++) {
		if (data & 0x80)
			pxa2x0_gpio_set_bit(txd_pin);
		else
			pxa2x0_gpio_clear_bit(txd_pin);
		delay(10);
		pxa2x0_gpio_set_bit(sclk_pin);
		delay(10);
		pxa2x0_gpio_clear_bit(sclk_pin);
		delay(10);
		data <<= 1;
	}

	pxa2x0_gpio_clear_bit(txd_pin);
	pxa2x0_gpio_set_bit(GPIO_TG_CS_C3000);

	pxa2x0_gpio_set_function(sclk_pin, sclk_fn);
	pxa2x0_gpio_set_function(sfrm_pin, sfrm_fn);
	pxa2x0_gpio_set_function(txd_pin, txd_fn);
	pxa2x0_gpio_set_function(rxd_pin, rxd_fn);

	splx(s);
}

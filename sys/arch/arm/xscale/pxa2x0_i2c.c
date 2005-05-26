/*	$OpenBSD: pxa2x0_i2c.c,v 1.2 2005/05/26 03:52:07 pascoe Exp $	*/

/*
 * Copyright (c) 2005 Christopher Pascoe <pascoe@openbsd.org>
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

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_i2c.h>
#include <arm/xscale/pxa2x0_gpio.h>

#define I2C_RETRY_COUNT	10

int
pxa2x0_i2c_attach_sub(struct pxa2x0_i2c_softc *sc)
{
	if (bus_space_map(sc->sc_iot, PXA2X0_I2C_BASE,
	    PXA2X0_I2C_SIZE, 0, &sc->sc_ioh)) {
		sc->sc_size = 0;
		return EIO;
	}
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, 0, sc->sc_size,
	    BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE);

	/*
	 * Configure the alternate functions.  The _IN is arbitrary, as the
	 * direction is managed by the I2C unit when comms are in progress.
	 */
	pxa2x0_gpio_set_function(117, GPIO_ALT_FN_1_IN);	/* SCL */
	pxa2x0_gpio_set_function(118, GPIO_ALT_FN_1_IN);	/* SDA */

	pxa2x0_i2c_init(sc);

	return 0;
}

int
pxa2x0_i2c_detach_sub(struct pxa2x0_i2c_softc *sc)
{
	if (sc->sc_size) {
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
		sc->sc_size = 0;
	}
	pxa2x0_clkman_config(CKEN_I2C, 0);

	return 0;
}

void
pxa2x0_i2c_init(struct pxa2x0_i2c_softc *sc)
{
	pxa2x0_i2c_open(sc);
	pxa2x0_i2c_close(sc);
}

void
pxa2x0_i2c_open(struct pxa2x0_i2c_softc *sc)
{
	/* Enable the clock to the standard I2C unit. */
	pxa2x0_clkman_config(CKEN_I2C, 1);
}

void
pxa2x0_i2c_close(struct pxa2x0_i2c_softc *sc)
{
	/* Reset and disable the standard I2C unit. */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, I2C_ICR, ICR_UR);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, I2C_ISAR, 0);
	delay(1);
	pxa2x0_clkman_config(CKEN_I2C, 0);
}

int
pxa2x0_i2c_read(struct pxa2x0_i2c_softc *sc, u_char slave, u_char *valuep)
{
	u_int32_t rv;
	int timeout;
	int tries = I2C_RETRY_COUNT;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

retry:
	bus_space_write_4(iot, ioh, I2C_ICR, ICR_UR);
	bus_space_write_4(iot, ioh, I2C_ISAR, 0x00);
	bus_space_write_4(iot, ioh, I2C_ISR, ISR_ITE | ISR_IRF);
	delay(1);
	bus_space_write_4(iot, ioh, I2C_ICR, ICR_IUE | ICR_SCLE);

	/* Write slave device address. */
	bus_space_write_4(iot, ioh, I2C_IDBR, (slave<<1) | 0x1);
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv | ICR_START);
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv & ~ICR_STOP);
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv | ICR_TB);

	timeout = 10000;
	while ((bus_space_read_4(iot, ioh, I2C_ISR) & ISR_ITE) == 0) {
		if (timeout-- == 0)
			goto err;
		delay(1);
	}

	bus_space_write_4(iot, ioh, I2C_ISR, ISR_ITE);

	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv & ~ICR_START);

	/* Read data value. */
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv |
	    (ICR_STOP | ICR_ACKNAK));
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv | ICR_TB);

	timeout = 10000;
	while ((bus_space_read_4(iot, ioh, I2C_ISR) & ISR_IRF) == 0) {
		if (timeout-- == 0)
			goto err;
		delay(1);
	}

	bus_space_write_4(iot, ioh, I2C_ISR, ISR_IRF);

	rv = bus_space_read_4(iot, ioh, I2C_IDBR);
	*valuep = (u_char)rv;
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv &
	    ~(ICR_STOP | ICR_ACKNAK));

	return (0);
err:
	if (tries-- >= 0)
		goto retry;

	bus_space_write_4(iot, ioh, I2C_ICR, ICR_UR);
	bus_space_write_4(iot, ioh, I2C_ISAR, 0x00);
	bus_space_write_4(iot, ioh, I2C_ISR, ISR_ITE | ISR_IRF);
	bus_space_write_4(iot, ioh, I2C_ICR, ICR_IUE | ICR_SCLE);

	return (-EIO);
}

int
pxa2x0_i2c_write(struct pxa2x0_i2c_softc *sc, u_char slave, u_char value)
{
	u_int32_t rv;
	int timeout;
	int tries = I2C_RETRY_COUNT;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

retry:
	bus_space_write_4(iot, ioh, I2C_ICR, ICR_UR);
	bus_space_write_4(iot, ioh, I2C_ISAR, 0x00);
	bus_space_write_4(iot, ioh, I2C_ISR, ISR_ITE);
	delay(1);
	bus_space_write_4(iot, ioh, I2C_ICR, ICR_IUE | ICR_SCLE);

	/* Write slave device address. */
	bus_space_write_4(iot, ioh, I2C_IDBR, (slave<<1));
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv | ICR_START);
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv & ~ICR_STOP);
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv | ICR_TB);

	timeout = 10000;
	while ((bus_space_read_4(iot, ioh, I2C_ISR) & ISR_ITE) == 0) {
		if (timeout-- == 0)
			goto err;
		delay(1);
	}
	if ((bus_space_read_4(iot, ioh, I2C_ISR) & ISR_ACKNAK) != 0)
		goto err;

	bus_space_write_4(iot, ioh, I2C_ISR, ISR_ITE);

	/* Write data. */
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv & ~ICR_START);
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv | ICR_STOP);
	bus_space_write_4(iot, ioh, I2C_IDBR, value);
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv | ICR_TB);

	timeout = 10000;
	while ((bus_space_read_4(iot, ioh, I2C_ISR) & ISR_ITE) == 0) {
		if (timeout-- == 0)
			goto err;
		delay(1);
	}
	if ((bus_space_read_4(iot, ioh, I2C_ISR) & ISR_ACKNAK) != 0)
		goto err;

	bus_space_write_4(iot, ioh, I2C_ISR, ISR_ITE);

	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv & ~ICR_STOP);

	return (0);
err:
	if (tries-- >= 0)
		goto retry;

	bus_space_write_4(iot, ioh, I2C_ICR, ICR_UR);
	bus_space_write_4(iot, ioh, I2C_ISAR, 0x00);
	bus_space_write_4(iot, ioh, I2C_ISR, ISR_ITE);
	bus_space_write_4(iot, ioh, I2C_ICR, ICR_IUE | ICR_SCLE);

	return (-EIO);
}

int
pxa2x0_i2c_write_2(struct pxa2x0_i2c_softc *sc, u_char slave, u_short value)
{
	u_int32_t rv;
	int timeout;
	int tries = I2C_RETRY_COUNT;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

retry:
	bus_space_write_4(iot, ioh, I2C_ICR, ICR_UR);
	bus_space_write_4(iot, ioh, I2C_ISAR, 0x00);
	bus_space_write_4(iot, ioh, I2C_ISR, ISR_ITE);
	delay(1);
	bus_space_write_4(iot, ioh, I2C_ICR, ICR_IUE | ICR_SCLE);

	/* Write slave device address. */
	bus_space_write_4(iot, ioh, I2C_IDBR, (slave<<1));
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv | ICR_START);
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv & ~ICR_STOP);
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv | ICR_TB);

	timeout = 10000;
	while ((bus_space_read_4(iot, ioh, I2C_ISR) & ISR_ITE) == 0) {
		if (timeout-- == 0)
			goto err;
		delay(1);
	}
	if ((bus_space_read_4(iot, ioh, I2C_ISR) & ISR_ACKNAK) != 0)
		goto err;

	bus_space_write_4(iot, ioh, I2C_ISR, ISR_ITE);

	/* Write upper 8 bits of data. */
	bus_space_write_4(iot, ioh, I2C_IDBR, (value >> 8) & 0xff);
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv & ~ICR_START);
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv & ~ICR_STOP);
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv | ICR_TB);

	timeout = 10000;
	while ((bus_space_read_4(iot, ioh, I2C_ISR) & ISR_ITE) == 0) {
		if (timeout-- == 0)
			goto err;
		delay(1);
	}
	if ((bus_space_read_4(iot, ioh, I2C_ISR) & ISR_ACKNAK) != 0)
		goto err;

	bus_space_write_4(iot, ioh, I2C_ISR, ISR_ITE);

	/* Write lower 8 bits of data. */
	bus_space_write_4(iot, ioh, I2C_IDBR, value & 0xff);
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv & ~ICR_START);
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv | ICR_STOP);
	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv | ICR_TB);

	timeout = 10000;
	while ((bus_space_read_4(iot, ioh, I2C_ISR) & ISR_ITE) == 0) {
		if (timeout-- == 0)
			goto err;
		delay(1);
	}
	if ((bus_space_read_4(iot, ioh, I2C_ISR) & ISR_ACKNAK) != 0)
		goto err;

	bus_space_write_4(iot, ioh, I2C_ISR, ISR_ITE);

	rv = bus_space_read_4(iot, ioh, I2C_ICR);
	bus_space_write_4(iot, ioh, I2C_ICR, rv & ~ICR_STOP);

	return (0);
err:
	if (tries-- >= 0)
		goto retry;

	bus_space_write_4(iot, ioh, I2C_ICR, ICR_UR);
	bus_space_write_4(iot, ioh, I2C_ISAR, 0x00);
	bus_space_write_4(iot, ioh, I2C_ISR, ISR_ITE);
	bus_space_write_4(iot, ioh, I2C_ICR, ICR_IUE | ICR_SCLE);

	return (-EIO);
}

/*	$OpenBSD: sociic.c,v 1.2 2009/09/06 20:09:34 kettenis Exp $	*/

/*
 * Copyright (c) 2008 Mark Kettenis
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

/*
 * Driver for the I2C interface on the MPC8349E processors.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/rwlock.h>

#include <machine/autoconf.h>
#include <machine/bus.h>

#include <dev/ofw/openfirm.h>

#include <dev/i2c/i2cvar.h>

#define I2C_ADR		0x00	/* Address Register */
#define I2C_FDR		0x04	/* Fequency Divider Register */
#define I2C_CR		0x08	/* Control Register */
#define  I2C_CR_MEN		0x80
#define  I2C_CR_MIEN		0x40
#define  I2C_CR_MSTA		0x20
#define  I2C_CR_MTX		0x10
#define  I2C_CR_TXAK		0x08
#define  I2C_CR_RSTA		0x04
#define  I2C_CR_BCST		0x01
#define I2C_SR		0x0c	/* Status Register */
#define  I2C_SR_MCF		0x80
#define  I2C_SR_MAAS		0x40
#define  I2C_SR_MBB		0x20
#define  I2C_SR_MAL		0x10
#define  I2C_SR_BCSTM		0x08
#define  I2C_SR_SRW		0x04
#define  I2C_SR_MIF		0x02
#define  I2C_SR_RXAK		0x01
#define I2C_DR		0x10	/* Data Register */
#define I2C_DFSRR	0x14	/* Digital Filter Sampling Rate Register */

struct sociic_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	
	struct i2c_controller	sc_i2c;
	struct rwlock		sc_lock;
};

int	sociic_match(struct device *, void *, void *);
void	sociic_attach(struct device *, struct device *, void *);

struct cfattach sociic_ca = {
	sizeof(struct sociic_softc), sociic_match, sociic_attach
};

struct cfdriver sociic_cd = {
	NULL, "sociic", DV_DULL
};

void	sociic_write(struct sociic_softc *, bus_addr_t, uint8_t);
uint8_t	sociic_read(struct sociic_softc *, bus_addr_t);
int	sociic_wait(struct sociic_softc *, int);
int	sociic_wait_bus(struct sociic_softc *);
int	sociic_i2c_acquire_bus(void *, int);
void	sociic_i2c_release_bus(void *, int);
int	sociic_i2c_exec(void *, i2c_op_t, i2c_addr_t,
	    const void *, size_t, void *, size_t, int);

int
sociic_match(struct device *parent, void *cfdata, void *aux)
{
	struct obio_attach_args *oa = aux;
	char buf[32];

	if (OF_getprop(oa->oa_node, "compatible", buf, sizeof(buf)) <= 0 ||
	    strcmp(buf, "fsl-i2c") != 0)
		return (0);

	return (1);
}

void
sociic_attach(struct device *parent, struct device *self, void *aux)
{
	struct sociic_softc *sc = (void *)self;
	struct obio_attach_args *oa = aux;
	struct i2cbus_attach_args iba;

	sc->sc_iot = oa->oa_iot;
	if (bus_space_map(sc->sc_iot, oa->oa_offset, 24, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	printf("\n");

	rw_init(&sc->sc_lock, "iiclk");
	sc->sc_i2c.ic_cookie = sc;
	sc->sc_i2c.ic_acquire_bus = sociic_i2c_acquire_bus;
	sc->sc_i2c.ic_release_bus = sociic_i2c_release_bus;
	sc->sc_i2c.ic_exec = sociic_i2c_exec;

	bzero(&iba, sizeof iba);
	iba.iba_name = "iic";
	iba.iba_tag = &sc->sc_i2c;
	config_found(&sc->sc_dev, &iba, iicbus_print);
}

void
sociic_write(struct sociic_softc *sc, bus_addr_t addr, uint8_t data)
{
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, addr, data);
}

uint8_t
sociic_read(struct sociic_softc *sc, bus_addr_t addr)
{
	return (bus_space_read_1(sc->sc_iot, sc->sc_ioh, addr));
}

int
sociic_wait(struct sociic_softc *sc, int flags)
{
	uint8_t sr;
	int i;

	for (i = 0; i < 1000; i++) {
		sr = sociic_read(sc, I2C_SR);
		if (sr & I2C_SR_MIF) {
			sociic_write(sc, I2C_SR, 0);

			if (sr & I2C_SR_MAL)
				return (EIO);

			if ((sr & I2C_SR_MCF) == 0)
				return (EIO);

			if ((flags & I2C_F_READ) == 0 && (sr & I2C_SR_RXAK))
				return (EIO);

			return (0);
		}
		delay(100);
	}

	return (ETIMEDOUT);
}

int
sociic_wait_bus(struct sociic_softc *sc)
{
	uint8_t sr;
	int i;

	for (i = 0; i < 1000; i++) {
		sr = sociic_read(sc, I2C_SR);
		if ((sr & I2C_SR_MBB) == 0)
			return (0);
		delay(1000);
	}

	return (ETIMEDOUT);
}

int
sociic_i2c_acquire_bus(void *arg, int flags)
{
	struct sociic_softc *sc = arg;

	if (cold || (flags & I2C_F_POLL))
		return (0);

	return (rw_enter(&sc->sc_lock, RW_WRITE | RW_INTR));
}

void
sociic_i2c_release_bus(void *arg, int flags)
{
	struct sociic_softc *sc = arg;

	if (cold || (flags & I2C_F_POLL))
		return;

	rw_exit(&sc->sc_lock);
}

int
sociic_i2c_exec(void *arg, i2c_op_t op, i2c_addr_t addr,
    const void *vcmdbuf, size_t cmdlen, void *vbuf, size_t buflen, int flags)
{
	struct sociic_softc *sc = arg;
	const uint8_t *cmdbuf = vcmdbuf;
	uint8_t *buf = vbuf;
	int err = 0;
	size_t len;
	uint8_t val;

	/* Clear the bus. */
	sociic_write(sc, I2C_SR, 0);
	sociic_write(sc, I2C_CR, I2C_CR_MEN);
	err = sociic_wait_bus(sc);
	if (err)
		return (err);

	if (cmdlen > 0) {
		sociic_write(sc, I2C_CR, I2C_CR_MEN|I2C_CR_MSTA|I2C_CR_MTX);
		sociic_write(sc, I2C_DR, addr << 1);
		err = sociic_wait(sc, I2C_F_WRITE);
		if (err)
			goto out;

		len = cmdlen;
		while (len--) {
			sociic_write(sc, I2C_DR, *cmdbuf++);
			err = sociic_wait(sc, I2C_F_WRITE);
			if (err)
				goto out;
		}
	}

	if (I2C_OP_READ_P(op) && buflen > 0) {
		/* RESTART if we did write a command above. */
		val = I2C_CR_MEN|I2C_CR_MSTA|I2C_CR_MTX;
		if (cmdlen > 0)
			val |= I2C_CR_RSTA;
		sociic_write(sc, I2C_CR, val);
		sociic_write(sc, I2C_DR, (addr << 1) | 1);
		err = sociic_wait(sc, I2C_F_WRITE);
		if (err)
			goto out;

		/* NACK if we're only sending one byte. */
		val = I2C_CR_MEN|I2C_CR_MSTA;
		if (buflen == 1)
			val |= I2C_CR_TXAK;
		sociic_write(sc, I2C_CR, val);

		/* Dummy read. */
		sociic_read(sc, I2C_DR);

		len = buflen;
		while (len--) {
			err = sociic_wait(sc, I2C_F_READ);
			if (err)
				goto out;

			/* NACK on last byte. */
			if (len == 1)
				sociic_write(sc, I2C_CR,
				    I2C_CR_MEN|I2C_CR_MSTA|I2C_CR_TXAK);
			/* STOP after last byte. */
			if (len == 0)
				sociic_write(sc, I2C_CR,
				    I2C_CR_MEN|I2C_CR_TXAK);
			*buf++ = sociic_read(sc, I2C_DR);
		}
	}

	if (I2C_OP_WRITE_P(op) && cmdlen == 0 && buflen > 0) {
		/* START if we didn't write a command. */
		sociic_write(sc, I2C_CR, I2C_CR_MEN|I2C_CR_MSTA|I2C_CR_MTX);
		sociic_write(sc, I2C_DR, addr << 1);
		err = sociic_wait(sc, I2C_F_WRITE);
		if (err)
			goto out;
	}

	if (I2C_OP_WRITE_P(op) && buflen > 0) {
		len = buflen;
		while (len--) {
			sociic_write(sc, I2C_DR, *buf++);
			err = sociic_wait(sc, I2C_F_WRITE);
			if (err)
				goto out;
		}
	}

out:
	/* STOP if we're still holding the bus. */
	sociic_write(sc, I2C_CR, I2C_CR_MEN);
	return (err);
}

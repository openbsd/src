/*	$OpenBSD: ki2c.c,v 1.8 2005/11/18 23:19:28 kettenis Exp $	*/
/*	$NetBSD: ki2c.c,v 1.1 2003/12/27 02:19:34 grant Exp $	*/

/*-
 * Copyright (c) 2001 Tsubai Masanari.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <dev/ofw/openfirm.h>
#include <uvm/uvm_extern.h>
#include <machine/autoconf.h>

#include <macppc/dev/ki2cvar.h>
#include <macppc/dev/maci2cvar.h>

int ki2c_match(struct device *, void *, void *);
void ki2c_attach(struct device *, struct device *, void *);
inline u_int ki2c_readreg(struct ki2c_softc *, int);
inline void ki2c_writereg(struct ki2c_softc *, int, u_int);
u_int ki2c_getmode(struct ki2c_softc *);
void ki2c_setmode(struct ki2c_softc *, u_int, u_int);
u_int ki2c_getspeed(struct ki2c_softc *);
void ki2c_setspeed(struct ki2c_softc *, u_int);
int ki2c_intr(struct ki2c_softc *);
int ki2c_poll(struct ki2c_softc *, int);
int ki2c_start(struct ki2c_softc *, int, int, void *, int);
int ki2c_read(struct ki2c_softc *, int, int, void *, int);
int ki2c_write(struct ki2c_softc *, int, int, const void *, int);

/* I2C glue */
int ki2c_i2c_acquire_bus(void *, int);
void ki2c_i2c_release_bus(void *, int);
int ki2c_i2c_exec(void *, i2c_op_t, i2c_addr_t, const void *, size_t,
    void *, size_t, int);

struct cfattach ki2c_ca = {
	sizeof(struct ki2c_softc), ki2c_match, ki2c_attach
};
struct cfattach ki2c_memc_ca = {
	sizeof(struct ki2c_softc), ki2c_match, ki2c_attach
};

struct cfdriver ki2c_cd = {
	NULL, "ki2c", DV_DULL
};

int
ki2c_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct confargs *ca = aux;

	if (strcmp(ca->ca_name, "i2c") == 0)
		return 1;

	return 0;
}

void
ki2c_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct ki2c_softc *sc = (struct ki2c_softc *)self;
	struct confargs *ca = aux;
	struct maci2cbus_attach_args iba;
	int node = ca->ca_node;
	int rate;

	ca->ca_reg[0] += ca->ca_baseaddr;

	if (OF_getprop(node, "AAPL,i2c-rate", &rate, 4) != 4) {
		printf(": cannot get i2c-rate\n");
		return;
	}
	if (OF_getprop(node, "AAPL,address", &sc->sc_paddr, 4) != 4) {
		printf(": unable to find i2c address\n");
		return;
	}
	if (OF_getprop(node, "AAPL,address-step", &sc->sc_regstep, 4) != 4) {
		printf(": unable to find i2c address step\n");
		return;
	}
	sc->sc_reg = mapiodev(sc->sc_paddr, (DATA+1)*sc->sc_regstep);

	printf("\n");

	ki2c_writereg(sc, STATUS, 0);
	ki2c_writereg(sc, ISR, 0);
	ki2c_writereg(sc, IER, 0);

	ki2c_setmode(sc, I2C_STDSUBMODE, 0);
	ki2c_setspeed(sc, I2C_100kHz);		/* XXX rate */

	lockinit(&sc->sc_buslock, PZERO, sc->sc_dev.dv_xname, 0, 0);
	ki2c_writereg(sc, IER,I2C_INT_DATA|I2C_INT_ADDR|I2C_INT_STOP);

	sc->sc_i2c_tag.ic_cookie = sc;
	sc->sc_i2c_tag.ic_acquire_bus = ki2c_i2c_acquire_bus;
	sc->sc_i2c_tag.ic_release_bus = ki2c_i2c_release_bus;
	sc->sc_i2c_tag.ic_exec = ki2c_i2c_exec;

	iba.iba_node = node;
	iba.iba_tag = &sc->sc_i2c_tag;
	config_found(&sc->sc_dev, &iba, NULL);
}

u_int
ki2c_readreg(sc, reg)
	struct ki2c_softc *sc;
	int reg;
{
	u_char *addr = sc->sc_reg + sc->sc_regstep * reg;

	return *addr;
}

void
ki2c_writereg(sc, reg, val)
	struct ki2c_softc *sc;
	int reg;
	u_int val;
{
	u_char *addr = sc->sc_reg + sc->sc_regstep * reg;

	*addr = val;
	asm volatile ("eieio");
	delay(10);
}

u_int
ki2c_getmode(sc)
	struct ki2c_softc *sc;
{
	return ki2c_readreg(sc, MODE) & I2C_MODE;
}

void
ki2c_setmode(sc, mode, bus)
	struct ki2c_softc *sc;
	u_int mode;
	u_int bus;
{
	u_int x;

	KASSERT((mode & ~I2C_MODE) == 0);
	x = ki2c_readreg(sc, MODE);
	x &= ~(I2C_MODE);
	if (bus)
		x |= I2C_BUS1;
	else
		x &= ~I2C_BUS1;
	x |= mode;
	ki2c_writereg(sc, MODE, x);
}

u_int
ki2c_getspeed(sc)
	struct ki2c_softc *sc;
{
	return ki2c_readreg(sc, MODE) & I2C_SPEED;
}

void
ki2c_setspeed(sc, speed)
	struct ki2c_softc *sc;
	u_int speed;
{
	u_int x;

	KASSERT((speed & ~I2C_SPEED) == 0);
	x = ki2c_readreg(sc, MODE);
	x &= ~I2C_SPEED;
	x |= speed;
	ki2c_writereg(sc, MODE, x);
}

int
ki2c_intr(sc)
	struct ki2c_softc *sc;
{
	u_int isr, x;

	isr = ki2c_readreg(sc, ISR);
	if (isr & I2C_INT_ADDR) {
#if 0
		if ((ki2c_readreg(sc, STATUS) & I2C_ST_LASTAAK) == 0) {
			/* No slave responded. */
			sc->sc_flags |= I2C_ERROR;
			goto out;
		}
#endif

		if (sc->sc_flags & I2C_READING) {
			if (sc->sc_resid > 1) {
				x = ki2c_readreg(sc, CONTROL);
				x |= I2C_CT_AAK;
				ki2c_writereg(sc, CONTROL, x);
			}
		} else {
			ki2c_writereg(sc, DATA, *sc->sc_data++);
			sc->sc_resid--;
		}
	}

	if (isr & I2C_INT_DATA) {
		if (sc->sc_flags & I2C_READING) {
			*sc->sc_data++ = ki2c_readreg(sc, DATA);
			sc->sc_resid--;

			if (sc->sc_resid == 0) {	/* Completed */
				ki2c_writereg(sc, CONTROL, 0);
				goto out;
			}
		} else {
#if 0
			if ((ki2c_readreg(sc, STATUS) & I2C_ST_LASTAAK) == 0) {
				/* No slave responded. */
				sc->sc_flags |= I2C_ERROR;
				goto out;
			}
#endif

			if (sc->sc_resid == 0) {
				x = ki2c_readreg(sc, CONTROL) | I2C_CT_STOP;
				ki2c_writereg(sc, CONTROL, x);
			} else {
				ki2c_writereg(sc, DATA, *sc->sc_data++);
				sc->sc_resid--;
			}
		}
	}

out:
	if (isr & I2C_INT_STOP) {
		ki2c_writereg(sc, CONTROL, 0);
		sc->sc_flags &= ~I2C_BUSY;
	}

	ki2c_writereg(sc, ISR, isr);

	return 1;
}

int
ki2c_poll(sc, timo)
	struct ki2c_softc *sc;
	int timo;
{
	while (sc->sc_flags & I2C_BUSY) {
		if (ki2c_readreg(sc, ISR))
			ki2c_intr(sc);
		timo -= 100;
		if (timo < 0) {
			printf("i2c_poll: timeout\n");
			return -1;
		}
		delay(100);
	}
	return 0;
}

int
ki2c_start(sc, addr, subaddr, data, len)
	struct ki2c_softc *sc;
	int addr, subaddr, len;
	void *data;
{
	int rw = (sc->sc_flags & I2C_READING) ? 1 : 0;
	int timo, x;

	KASSERT((addr & 1) == 0);

	sc->sc_data = data;
	sc->sc_resid = len;
	sc->sc_flags |= I2C_BUSY;

	timo = 1000 + len * 200;

	/* XXX TAS3001 sometimes takes 50ms to finish writing registers. */
	/* if (addr == 0x68) */
		timo += 100000;

	ki2c_writereg(sc, ADDR, addr | rw);
	ki2c_writereg(sc, SUBADDR, subaddr);

	x = ki2c_readreg(sc, CONTROL) | I2C_CT_ADDR;
	ki2c_writereg(sc, CONTROL, x);

	if (ki2c_poll(sc, timo))
		return -1;
	if (sc->sc_flags & I2C_ERROR) {
		printf("I2C_ERROR\n");
		return -1;
	}
	return 0;
}

int
ki2c_read(sc, addr, subaddr, data, len)
	struct ki2c_softc *sc;
	int addr, subaddr, len;
	void *data;
{
	sc->sc_flags = I2C_READING;
	return ki2c_start(sc, addr, subaddr, data, len);
}

int
ki2c_write(sc, addr, subaddr, data, len)
	struct ki2c_softc *sc;
	int addr, subaddr, len;
	const void *data;
{
	sc->sc_flags = 0;
	return ki2c_start(sc, addr, subaddr, (void *)data, len);
}

int
ki2c_i2c_acquire_bus(void *cookie, int flags)
{
	struct ki2c_softc *sc = cookie;

	return (lockmgr(&sc->sc_buslock, LK_EXCLUSIVE, NULL, curproc));
}

void
ki2c_i2c_release_bus(void *cookie, int flags)
{
	struct ki2c_softc *sc = cookie;

	(void) lockmgr(&sc->sc_buslock, LK_RELEASE, NULL, curproc);
}

int
ki2c_i2c_exec(void *cookie, i2c_op_t op, i2c_addr_t addr,
    const void *cmdbuf, size_t cmdlen, void *buf, size_t len, int flags)
{
	struct ki2c_softc *sc = cookie;
	u_int mode = I2C_STDSUBMODE;
	u_int8_t cmd = 0;

	if (!I2C_OP_STOP_P(op) || cmdlen > 1)
		return (EINVAL);

	if (cmdlen == 0)
		mode = I2C_STDMODE;
	else if (I2C_OP_READ_P(op))
		mode = I2C_COMBMODE;

	if (cmdlen > 0)
		cmd = *(u_int8_t *)cmdbuf;

	ki2c_setmode(sc, mode, addr & 0x80);
	addr &= 0x7f;

	if (I2C_OP_READ_P(op)) {
		if (ki2c_read(sc, (addr << 1), cmd, buf, len) != 0)
			return (EIO);
	} else {
		if (ki2c_write(sc, (addr << 1), cmd, buf, len) != 0)
			return (EIO);
	}
	return (0);
}

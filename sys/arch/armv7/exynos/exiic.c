/*
 * Copyright (c) 2013 Patrick Wildt <patrick@blueri.se>
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
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <machine/bus.h>
#if NFDT > 0
#include <machine/fdt.h>
#endif

#include <armv7/armv7/armv7var.h>
#include <armv7/exynos/exgpiovar.h>
#include <armv7/exynos/exiicvar.h>
#include <armv7/exynos/exclockvar.h>

/* registers */
#define I2C_CON				0x00	/* control register */
#define I2C_STAT			0x04	/* control/status register */
#define I2C_ADD				0x08	/* address register */
#define I2C_DS				0x0C	/* transmit/receive data shift register */
#define I2C_LC				0x10	/* multi-master line control register */

/* bits and bytes */
#define I2C_CON_TXCLKVAL_MASK		(0xf << 0) /* tx clock = i2cclk / (i2ccon[3:0] + 1) */
#define I2C_CON_INTPENDING		(0x1 << 4) /* 0 = no interrupt pending/clear, 1 = pending */
#define I2C_CON_TXRX_INT		(0x1 << 5) /* enable/disable */
#define I2C_CON_TXCLKSRC_16		(0x0 << 6) /* i2clk = fpclk/16 */
#define I2C_CON_TXCLKSRC_512		(0x1 << 6) /* i2clk = fpclk/512 */
#define I2C_CON_ACK			(0x1 << 7)
#define I2C_STAT_LAST_RVCD_BIT		(0x1 << 0) /* last received bit 0 => ack, 1 => no ack */
#define I2C_STAT_ADDR_ZERO_FLAG		(0x1 << 1) /* 0 => start/stop cond. detected, 1 => received slave addr 0xb */
#define I2C_STAT_ADDR_SLAVE_ZERO_FLAG	(0x1 << 2) /* 0 => start/stop cond. detected, 1 => received slave addr matches i2cadd */
#define I2C_STAT_ARBITRATION		(0x1 << 3) /* 0 => successul, 1 => failed */
#define I2C_STAT_SERIAL_OUTPUT		(0x1 << 4) /* 0 => disable tx/rx, 1 => enable tx/rx */
#define I2C_STAT_BUSY_SIGNAL		(0x1 << 5) /* 0 => not busy / stop signal generation, 1 => busy / start signal generation */
#define I2C_STAT_MODE_SEL_SLAVE_RX	(0x0 << 6) /* slave receive mode */
#define I2C_STAT_MODE_SEL_SLAVE_TX	(0x1 << 6) /* slave transmit mode */
#define I2C_STAT_MODE_SEL_MASTER_RX	(0x2 << 6) /* master receive mode */
#define I2C_STAT_MODE_SEL_MASTER_TX	(0x3 << 6) /* master transmit */
#define I2C_ADD_SLAVE_ADDR(x)		(((x) & 0x7f) << 1)
#define I2C_DS_DATA_SHIFT(x)		(((x) & 0xff) << 0)

#define I2C_ACK				0
#define I2C_NACK			1
#define I2C_TIMEOUT			2

struct exiic_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_ios;
	void			*sc_ih;
	int			unit;

	struct rwlock		sc_buslock;
	struct i2c_controller	i2c_tag;

	uint16_t		frequency;
	uint16_t		intr_status;
};

int exiic_match(struct device *parent, void *v, void *aux);
void exiic_attach(struct device *, struct device *, void *);
int exiic_detach(struct device *, int);
void exiic_bus_scan(struct device *, struct i2cbus_attach_args *, void *);
void exiic_setspeed(struct exiic_softc *, int);
int exiic_intr(void *);
int exiic_wait_intr(struct exiic_softc *, int, int);
int exiic_wait_state(struct exiic_softc *, uint32_t, uint32_t, uint32_t);
int exiic_start(struct exiic_softc *, int, int, void *, int);

void exiic_xfer_start(struct exiic_softc *);
int exiic_xfer_wait(struct exiic_softc *);
int exiic_i2c_acquire_bus(void *, int);
void exiic_i2c_release_bus(void *, int);
int exiic_i2c_exec(void *, i2c_op_t, i2c_addr_t, const void *, size_t,
    void *, size_t, int);

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))


struct cfattach exiic_ca = {
	sizeof(struct exiic_softc), NULL, exiic_attach, exiic_detach
};
struct cfattach exiic_fdt_ca = {
	sizeof(struct exiic_softc), exiic_match, exiic_attach, exiic_detach
};

struct cfdriver exiic_cd = {
	NULL, "exiic", DV_DULL
};

int
exiic_match(struct device *parent, void *v, void *aux)
{
#if NFDT > 0
	struct armv7_attach_args *aa = aux;

	if (fdt_node_compatible("samsung,s3c2440-i2c", aa->aa_node))
		return 1;
#endif

	return 0;
}

void
exiic_attach(struct device *parent, struct device *self, void *args)
{
	struct exiic_softc *sc = (struct exiic_softc *)self;
	struct armv7_attach_args *aa = args;
	struct armv7mem mem;

	sc->sc_iot = aa->aa_iot;
#if NFDT > 0
	if (aa->aa_node) {
		struct fdt_memory fdtmem;
		static int unit = 0;

		sc->unit = unit++;
		if (fdt_get_memory_address(aa->aa_node, 0, &fdtmem))
			panic("%s: could not extract memory data from FDT",
			    __func__);
		mem.addr = fdtmem.addr;
		mem.size = fdtmem.size;
	} else
#endif
	{
		mem.addr = aa->aa_dev->mem[0].addr;
		mem.size = aa->aa_dev->mem[0].size;
		sc->unit = aa->aa_dev->unit;
	}
	if (bus_space_map(sc->sc_iot, mem.addr, mem.size, 0, &sc->sc_ioh))
		panic("%s: bus_space_map failed!", __func__);
	sc->sc_ios = mem.size;

#if 0
	sc->sc_ih = arm_intr_establish(aa->aa_dev->irq[0], IPL_BIO,
	    exiic_intr, sc, sc->sc_dev.dv_xname);
#endif

	printf("\n");

	rw_init(&sc->sc_buslock, sc->sc_dev.dv_xname);

	struct i2cbus_attach_args iba;

	sc->i2c_tag.ic_cookie = sc;
	sc->i2c_tag.ic_acquire_bus = exiic_i2c_acquire_bus;
	sc->i2c_tag.ic_release_bus = exiic_i2c_release_bus;
	sc->i2c_tag.ic_exec = exiic_i2c_exec;

	bzero(&iba, sizeof iba);
	iba.iba_name = "iic";
	iba.iba_tag = &sc->i2c_tag;
	iba.iba_bus_scan = exiic_bus_scan;
	iba.iba_bus_scan_arg = sc;
	config_found(&sc->sc_dev, &iba, NULL);
}

void
exiic_bus_scan(struct device *self, struct i2cbus_attach_args *iba, void *arg)
{
	struct exiic_softc *sc = (struct exiic_softc *)arg;
	struct i2c_attach_args ia;

	/* XXX: We currently only attach cros-ec on I2C4.  We'll use FDT later. */
	if (sc->unit != 4)
		return;

	char *name = "crosec";
	int addr = 0x1e;

	memset(&ia, 0, sizeof(ia));
	ia.ia_tag = iba->iba_tag;
	ia.ia_addr = addr;
	ia.ia_size = 1;
	ia.ia_name = name;
	config_found(self, &ia, iicbus_print);

	name = "tps65090";
	addr = 0x48;

	memset(&ia, 0, sizeof(ia));
	ia.ia_tag = iba->iba_tag;
	ia.ia_addr = addr;
	ia.ia_size = 1;
	ia.ia_name = name;
	config_found(self, &ia, iicbus_print);
}

void
exiic_setspeed(struct exiic_softc *sc, int speed)
{
	if (!sc->frequency) {
		uint32_t freq, div = 0, pres = 16;
		freq = exclock_get_i2cclk();

		/* calculate prescaler and divisor values */
		if ((freq / pres / (16 + 1)) > speed)
			/* set prescaler to 512 */
			pres = 512;

		while ((freq / pres / (div + 1)) > speed)
			div++;

		/* set prescaler, divisor according to freq, also set ACKGEN, IRQ */
		sc->frequency = (div & 0x0F) | 0xA0 | ((pres == 512) ? 0x40 : 0);
	}

	HWRITE4(sc, I2C_CON, sc->frequency);
}

#if 0
int
exiic_intr(void *arg)
{
	struct exiic_softc *sc = arg;
	u_int16_t status;
	int rc = 0;

	status = HREAD4(sc, I2C_CON);

	if (ISSET(status, I2C_CON_INTPENDING)) {
		/* we do not acknowledge the interrupt here */
		rc = 1;

		sc->intr_status |= status;
		wakeup(&sc->intr_status);
	}

	return (rc);
}

int
exiic_wait_intr(struct exiic_softc *sc, int mask, int timo)
{
	int status;
	int s;

	s = splbio();

	status = sc->intr_status & mask;
	while (status == 0) {
		if (tsleep(&sc->intr_status, PWAIT, "hcintr", timo)
		    == EWOULDBLOCK) {
			break;
		}
		status = sc->intr_status & mask;
	}
	status = sc->intr_status & mask;
	sc->intr_status &= ~status;

	splx(s);
	return status;
}
#endif

int
exiic_wait_state(struct exiic_softc *sc, uint32_t reg, uint32_t mask, uint32_t value)
{
	uint32_t state;
	int timeout;
	state = HREAD4(sc, reg);
	for (timeout = 1000; timeout > 0; timeout--) {
		if (((state = HREAD4(sc, reg)) & mask) == value)
			return 0;
		delay(1000);
	}
	return ETIMEDOUT;
}

int
exiic_i2c_acquire_bus(void *cookie, int flags)
{
	struct exiic_softc *sc = cookie;
	int ret = rw_enter(&sc->sc_buslock, RW_WRITE);

	if (!ret) {
		/* set speed to 100 Kbps */
		exiic_setspeed(sc, 100);

		/* STOP */
		HWRITE4(sc, I2C_STAT, 0);
		HWRITE4(sc, I2C_ADD, 0);
		HWRITE4(sc, I2C_STAT, I2C_STAT_MODE_SEL_MASTER_TX
				    | I2C_STAT_SERIAL_OUTPUT);
	}

	return ret;
}

void
exiic_i2c_release_bus(void *cookie, int flags)
{
	struct exiic_softc *sc = cookie;

	(void) rw_exit(&sc->sc_buslock);
}

int
exiic_i2c_exec(void *cookie, i2c_op_t op, i2c_addr_t _addr,
    const void *cmdbuf, size_t cmdlen, void *databuf, size_t datalen, int flags)
{
	struct exiic_softc *sc = cookie;
	uint32_t ret = 0;
	u_int8_t addr = 0;
	int i = 0;

	addr = (_addr & 0x7f) << 1;

	/* clock gating */
	//exccm_enable_i2c(sc->unit);

	if (exiic_wait_state(sc, I2C_STAT, I2C_STAT_BUSY_SIGNAL, 0)) {
		printf("%s: busy\n", __func__);
		return (EIO);
	}

	/* acknowledge generation */
	HSET4(sc, I2C_CON, I2C_CON_ACK);

	/* Send the slave-address */
	HWRITE4(sc, I2C_DS, addr);
	if (!I2C_OP_READ_P(op) || (cmdbuf && cmdlen))
		HWRITE4(sc, I2C_STAT, I2C_STAT_MODE_SEL_MASTER_TX
				    | I2C_STAT_SERIAL_OUTPUT
				    | I2C_STAT_BUSY_SIGNAL);
	else
		HWRITE4(sc, I2C_STAT, I2C_STAT_MODE_SEL_MASTER_RX
				    | I2C_STAT_SERIAL_OUTPUT
				    | I2C_STAT_BUSY_SIGNAL);

	ret = exiic_xfer_wait(sc);
	if (ret != I2C_ACK)
		goto fail;

	/* transmit commands */
	if (cmdbuf && cmdlen) {
		for (i = 0; i < cmdlen; i++) {
			HWRITE4(sc, I2C_DS, ((uint8_t *)cmdbuf)[i]);
			exiic_xfer_start(sc);
			ret = exiic_xfer_wait(sc);
			if (ret != I2C_ACK)
				goto fail;
		}
	}

	if (I2C_OP_READ_P(op)) {
		if (cmdbuf && cmdlen) {
			/* write slave chip address again for actual read */
			HWRITE4(sc, I2C_DS, addr);

			/* restart */
			HWRITE4(sc, I2C_STAT, I2C_STAT_MODE_SEL_MASTER_RX
					    | I2C_STAT_SERIAL_OUTPUT
					    | I2C_STAT_BUSY_SIGNAL);
			exiic_xfer_start(sc);
			ret = exiic_xfer_wait(sc);
			if (ret != I2C_ACK)
				goto fail;
		}

		for (i = 0; i < datalen && ret == I2C_ACK; i++) {
			/* disable ACK for final read */
			if (i == datalen - 1)
				HCLR4(sc, I2C_CON, I2C_CON_ACK);
			exiic_xfer_start(sc);
			ret = exiic_xfer_wait(sc);
			((uint8_t *)databuf)[i] = HREAD4(sc, I2C_DS);
		}
		if (ret == I2C_NACK)
			ret = I2C_ACK; /* Normal terminated read. */
	} else {
		for (i = 0; i < datalen && ret == I2C_ACK; i++) {
			HWRITE4(sc, I2C_DS, ((uint8_t *)databuf)[i]);
			exiic_xfer_start(sc);
			ret = exiic_xfer_wait(sc);
		}
	}

fail:
	/* send STOP */
	if (op & I2C_OP_READ_WITH_STOP) {
		HWRITE4(sc, I2C_STAT, I2C_STAT_MODE_SEL_MASTER_RX
				    | I2C_STAT_SERIAL_OUTPUT);
		exiic_xfer_start(sc);
	}

	return ret;
}

void
exiic_xfer_start(struct exiic_softc *sc)
{
	HCLR4(sc, I2C_CON, I2C_CON_INTPENDING);
}

int
exiic_xfer_wait(struct exiic_softc *sc)
{
	if (!exiic_wait_state(sc, I2C_CON, I2C_CON_INTPENDING,
					   I2C_CON_INTPENDING))
		return (HREAD4(sc, I2C_STAT) & I2C_STAT_LAST_RVCD_BIT) ?
			I2C_NACK : I2C_ACK;
	else
		return I2C_TIMEOUT;
}

int
exiic_detach(struct device *self, int flags)
{
	struct exiic_softc *sc = (struct exiic_softc *)self;

	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
	return 0;
}

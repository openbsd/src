/* $OpenBSD: sxitwi.c,v 1.5 2017/12/30 19:04:00 kettenis Exp $ */
/*	$NetBSD: gttwsi_core.c,v 1.2 2014/11/23 13:37:27 jmcneill Exp $	*/
/*
 * Copyright (c) 2008 Eiji Kawauchi.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Eiji Kawauchi.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 2005 Brocade Communcations, inc.
 * All rights reserved.
 *
 * Written by Matt Thomas for Brocade Communcations, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Brocade Communications, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BROCADE COMMUNICATIONS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL EITHER BROCADE COMMUNICATIONS, INC. BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Marvell Two-Wire Serial Interface (aka I2C) master driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/rwlock.h>

#define	_I2C_PRIVATE
#include <dev/i2c/i2cvar.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

#define	TWI_CCR_REG		0x14
#define	TWI_CCR_CLK_M		(0x0f << 3)
#define	TWI_CCR_CLK_N		(0x07 << 0)

#define	TWSI_SLAVEADDR		0x00
#define	TWSI_EXTEND_SLAVEADDR	0x04
#define	TWSI_DATA		0x08
#define	TWSI_CONTROL		0x0c
#define	TWSI_STATUS		0x10
#define	TWSI_BAUDRATE		0x14
#define	TWSI_SOFTRESET		0x18

#define	SLAVEADDR_GCE_MASK	0x01
#define	SLAVEADDR_SADDR_MASK	0xfe

#define	EXTEND_SLAVEADDR_MASK	0xff

#define	DATA_MASK		0xff

#define	CONTROL_ACK		(1 << 2)
#define	CONTROL_IFLG		(1 << 3)
#define	CONTROL_STOP		(1 << 4)
#define	CONTROL_START		(1 << 5)
#define	CONTROL_TWSIEN		(1 << 6)
#define	CONTROL_INTEN		(1 << 7)

#define	STAT_BE		0x00	/* Bus Error */
#define	STAT_SCT	0x08	/* Start condition transmitted */
#define	STAT_RSCT	0x10	/* Repeated start condition transmitted */
#define	STAT_AWBT_AR	0x18	/* Address + write bit transd, ack recvd */
#define	STAT_AWBT_ANR	0x20	/* Address + write bit transd, ack not recvd */
#define	STAT_MTDB_AR	0x28	/* Master transd data byte, ack recvd */
#define	STAT_MTDB_ANR	0x30	/* Master transd data byte, ack not recvd */
#define	STAT_MLADADT	0x38	/* Master lost arbitr during addr or data tx */
#define	STAT_ARBT_AR	0x40	/* Address + read bit transd, ack recvd */
#define	STAT_ARBT_ANR	0x48	/* Address + read bit transd, ack not recvd */
#define	STAT_MRRD_AT	0x50	/* Master received read data, ack transd */
#define	STAT_MRRD_ANT	0x58	/* Master received read data, ack not transd */
#define	STAT_SAWBT_AR	0xd0	/* Second addr + write bit transd, ack recvd */
#define	STAT_SAWBT_ANR	0xd8	/* S addr + write bit transd, ack not recvd */
#define	STAT_SARBT_AR	0xe0	/* Second addr + read bit transd, ack recvd */
#define	STAT_SARBT_ANR	0xe8	/* S addr + read bit transd, ack not recvd */
#define	STAT_NRS	0xf8	/* No relevant status */

#define	SOFTRESET_VAL		0		/* reset value */

#define	TWSI_RETRY_COUNT	1000		/* retry loop count */
#define	TWSI_RETRY_DELAY	1		/* retry delay */
#define	TWSI_STAT_DELAY		1		/* poll status delay */
#define	TWSI_READ_DELAY		2		/* read delay */
#define	TWSI_WRITE_DELAY	2		/* write delay */

struct sxitwi_softc {
	struct device		 sc_dev;
	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;
	int			 sc_node;
	u_int			 sc_started;
	u_int			 sc_twsien_iflg;
	struct i2c_controller	 sc_ic;
	struct rwlock		 sc_buslock;
	void			*sc_ih;
};

void	sxitwi_attach(struct device *, struct device *, void *);
int	sxitwi_match(struct device *, void *, void *);
void	sxitwi_bus_scan(struct device *, struct i2cbus_attach_args *, void *);

int	sxitwi_intr(void *);
int	sxitwi_acquire_bus(void *, int);
void	sxitwi_release_bus(void *, int);
int	sxitwi_send_start(void *, int);
int	sxitwi_send_stop(void *, int);
int	sxitwi_initiate_xfer(void *, i2c_addr_t, int);
int	sxitwi_read_byte(void *, uint8_t *, int);
int	sxitwi_write_byte(void *, uint8_t, int);
int	sxitwi_wait(struct sxitwi_softc *, u_int, u_int, int);
static inline u_int sxitwi_read_4(struct sxitwi_softc *, u_int);
static inline void sxitwi_write_4(struct sxitwi_softc *, u_int, u_int);

struct cfdriver sxitwi_cd = {
	NULL, "sxitwi", DV_DULL
};

struct cfattach sxitwi_ca = {
	sizeof(struct sxitwi_softc), sxitwi_match, sxitwi_attach
};

int
sxitwi_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "allwinner,sun4i-a10-i2c") ||
	    OF_is_compatible(faa->fa_node, "allwinner,sun6i-a31-i2c") ||
	    OF_is_compatible(faa->fa_node, "allwinner,sun7i-a20-i2c"));
}

void
sxitwi_attach(struct device *parent, struct device *self, void *aux)
{
	struct sxitwi_softc *sc = (struct sxitwi_softc *)self;
	struct fdt_attach_args *faa = aux;
	struct i2cbus_attach_args iba;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_node = faa->fa_node;
	sc->sc_iot = faa->fa_iot;

	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	rw_init(&sc->sc_buslock, sc->sc_dev.dv_xname);

	/* 
	 * On the Allwinner A31 we need to write 1 to clear a pending
	 * interrupt.
	 */
	sc->sc_twsien_iflg = CONTROL_TWSIEN;
	if (OF_is_compatible(sc->sc_node, "allwinner,sun6i-a31-i2c"))
		sc->sc_twsien_iflg |= CONTROL_IFLG;

	sc->sc_started = 0;
	sc->sc_ic.ic_cookie = sc;
	sc->sc_ic.ic_acquire_bus = sxitwi_acquire_bus;
	sc->sc_ic.ic_release_bus = sxitwi_release_bus;
	sc->sc_ic.ic_exec = NULL;
	sc->sc_ic.ic_send_start = sxitwi_send_start;
	sc->sc_ic.ic_send_stop = sxitwi_send_stop;
	sc->sc_ic.ic_initiate_xfer = sxitwi_initiate_xfer;
	sc->sc_ic.ic_read_byte = sxitwi_read_byte;
	sc->sc_ic.ic_write_byte = sxitwi_write_byte;

	pinctrl_byname(faa->fa_node, "default");

	/* Enable clock */
	clock_enable(faa->fa_node, NULL);
	reset_deassert_all(faa->fa_node);

	/*
	 * Set clock rate to 100kHz. From the datasheet:
	 *   For 100Khz standard speed 2Wire, CLK_N=2, CLK_M=11
	 *   F0=48M/2^2=12Mhz, F1=F0/(10*(11+1)) = 0.1Mhz
	 */
	sxitwi_write_4(sc, TWI_CCR_REG, (11 << 3) | (2 << 0));

	/* Put the controller into Soft Reset. */
	sxitwi_write_4(sc, TWSI_SOFTRESET, SOFTRESET_VAL);

	/* Establish interrupt */
	sc->sc_ih = arm_intr_establish_fdt(faa->fa_node, IPL_BIO,
	    sxitwi_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't to establish interrupt\n");
		return;
	}

	printf("\n");

	/* Configure its children */
	memset(&iba, 0, sizeof(iba));
	iba.iba_name = "iic";
	iba.iba_tag = &sc->sc_ic;
	iba.iba_bus_scan = sxitwi_bus_scan;
	iba.iba_bus_scan_arg = &sc->sc_node;

	config_found(&sc->sc_dev, &iba, iicbus_print);
}

void
sxitwi_bus_scan(struct device *self, struct i2cbus_attach_args *iba, void *arg)
{
	int iba_node = *(int *)arg;
	struct i2c_attach_args ia;
	char name[32];
	uint32_t reg[1];
	int node;

	for (node = OF_child(iba_node); node; node = OF_peer(node)) {
		memset(name, 0, sizeof(name));
		memset(reg, 0, sizeof(reg));

		if (OF_getprop(node, "compatible", name, sizeof(name)) == -1)
			continue;
		if (name[0] == '\0')
			continue;

		if (OF_getprop(node, "reg", &reg, sizeof(reg)) != sizeof(reg))
			continue;

		memset(&ia, 0, sizeof(ia));
		ia.ia_tag = iba->iba_tag;
		ia.ia_addr = bemtoh32(&reg[0]);
		ia.ia_name = name;
		ia.ia_cookie = &node;
		config_found(self, &ia, iic_print);
	}
}

u_int
sxitwi_read_4(struct sxitwi_softc *sc, u_int reg)
{
	u_int val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, reg);

	delay(TWSI_READ_DELAY);

	return val;
}

void
sxitwi_write_4(struct sxitwi_softc *sc, u_int reg, u_int val)
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, reg, val);

	delay(TWSI_WRITE_DELAY);

	return;
}

int
sxitwi_intr(void *arg)
{
	struct sxitwi_softc *sc = arg;
	u_int val;

	val = sxitwi_read_4(sc, TWSI_CONTROL);
	if (val & CONTROL_IFLG) {
		sxitwi_write_4(sc, TWSI_CONTROL, val & ~CONTROL_INTEN);
		wakeup(&sc->sc_dev);
		return 1;
	}
	return 0;
}

int
sxitwi_acquire_bus(void *arg, int flags)
{
	struct sxitwi_softc *sc = arg;

	if (flags & I2C_F_POLL)
		return 0;

	return rw_enter(&sc->sc_buslock, RW_WRITE);
}

void
sxitwi_release_bus(void *arg, int flags)
{
	struct sxitwi_softc *sc = arg;

	if (flags & I2C_F_POLL)
		return;

	rw_exit(&sc->sc_buslock);
}

int
sxitwi_send_start(void *v, int flags)
{
	struct sxitwi_softc *sc = v;
	int expect;

	if (sc->sc_started)
		expect = STAT_RSCT;
	else
		expect = STAT_SCT;
	sc->sc_started = 1;

	return sxitwi_wait(sc, CONTROL_START, expect, flags);
}

int
sxitwi_send_stop(void *v, int flags)
{
	struct sxitwi_softc *sc = v;
	int retry = TWSI_RETRY_COUNT;

	sc->sc_started = 0;

	/* Interrupt is not generated for STAT_NRS. */
	sxitwi_write_4(sc, TWSI_CONTROL, CONTROL_STOP | sc->sc_twsien_iflg);
	while (--retry > 0) {
		if (sxitwi_read_4(sc, TWSI_STATUS) == STAT_NRS)
			return 0;
		delay(TWSI_STAT_DELAY);
	}

	return -1;
}

int
sxitwi_initiate_xfer(void *v, i2c_addr_t addr, int flags)
{
	struct sxitwi_softc *sc = v;
	u_int data, expect;
	int error, read;

	sxitwi_send_start(v, flags);

	read = (flags & I2C_F_READ) != 0;
	if (read)
		expect = STAT_ARBT_AR;
	else
		expect = STAT_AWBT_AR;

	/*
	 * First byte contains whether this xfer is a read or write.
	 */
	data = read;
	if (addr > 0x7f) {
		/*
		 * If this is a 10bit request, the first address byte is
		 * 0b11110<b9><b8><r/w>.
		 */
		data |= 0xf0 | ((addr & 0x300) >> 7);
		sxitwi_write_4(sc, TWSI_DATA, data);
		error = sxitwi_wait(sc, 0, expect, flags);
		if (error)
			return error;
		/*
		 * The first address byte has been sent, now to send
		 * the second one.
		 */
		if (read)
			expect = STAT_SARBT_AR;
		else
			expect = STAT_SAWBT_AR;
		data = (uint8_t)addr;
	} else
		data |= (addr << 1);

	sxitwi_write_4(sc, TWSI_DATA, data);
	return sxitwi_wait(sc, 0, expect, flags);
}

int
sxitwi_read_byte(void *v, uint8_t *valp, int flags)
{
	struct sxitwi_softc *sc = v;
	int error;

	if (flags & I2C_F_LAST)
		error = sxitwi_wait(sc, 0, STAT_MRRD_ANT, flags);
	else
		error = sxitwi_wait(sc, CONTROL_ACK, STAT_MRRD_AT, flags);
	if (!error)
		*valp = sxitwi_read_4(sc, TWSI_DATA);
	if ((flags & (I2C_F_LAST | I2C_F_STOP)) == (I2C_F_LAST | I2C_F_STOP))
		error = sxitwi_send_stop(sc, flags);
	return error;
}

int
sxitwi_write_byte(void *v, uint8_t val, int flags)
{
	struct sxitwi_softc *sc = v;
	int error;

	sxitwi_write_4(sc, TWSI_DATA, val);
	error = sxitwi_wait(sc, 0, STAT_MTDB_AR, flags);
	if (flags & I2C_F_STOP)
		sxitwi_send_stop(sc, flags);
	return error;
}

int
sxitwi_wait(struct sxitwi_softc *sc, u_int control, u_int expect, int flags)
{
	u_int status;
	int timo, error = 0;

	delay(5);
	if (!(flags & I2C_F_POLL))
		control |= CONTROL_INTEN;
	sxitwi_write_4(sc, TWSI_CONTROL, control | sc->sc_twsien_iflg);

	timo = 0;
	do {
		control = sxitwi_read_4(sc, TWSI_CONTROL);
		if (control & CONTROL_IFLG)
			break;
		if (flags & I2C_F_POLL)
			delay(TWSI_RETRY_DELAY);
		else {
			error = tsleep(&sc->sc_dev, PWAIT, "sxitwi", 100);
			if (error)
				return error;
		}
	} while (++timo < 1000000);

	status = sxitwi_read_4(sc, TWSI_STATUS);
	if (status != expect)
		return EIO;

	return error;
}

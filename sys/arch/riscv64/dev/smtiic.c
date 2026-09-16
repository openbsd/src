/*	$OpenBSD: smtiic.c,v 1.2 2026/09/16 17:03:45 jca Exp $	*/
/*
 * Copyright (c) 2019 Patrick Wildt <patrick@blueri.se>
 * Copyright (c) 2026 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/systm.h>

#define _I2C_PRIVATE
#include <dev/i2c/i2cvar.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

/* registers */
#define ICR		0x00
#define  ICR_BEIE		(1U << 22)
#define  ICR_GCD		(1U << 21)
#define  ICR_DRFIE		(1U << 20)
#define  ICR_ITEIE		(1U << 19)
#define  ICR_IUE		(1U << 14)
#define  ICR_SCLE		(1U << 13)
#define  ICR_UR			(1U << 10)
#define  ICR_MODE_MASK		(0x3 << 8)
#define  ICR_MODE_FAST		(0x1 << 8)
#define  ICR_TB			(1U << 3)
#define  ICR_ACKNAK		(1U << 2)
#define  ICR_STOP		(1U << 1)
#define  ICR_START		(1U << 0)

#define ISR		0x04
#define  ISR_INIT		0xfdfc0000
#define	 ISR_BED		(1U << 22)
#define  ISR_IRF		(1U << 20)
#define  ISR_ITE		(1U << 19)
#define  ISR_IBB		(1U << 16)
#define  ISR_UB			(1U << 15)
#define  ISR_ACKNAK		(1U << 14)
#define IDBR		0x0c

struct smtiic_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	int			sc_node;
	uint32_t		sc_freq;

	struct i2c_controller	sc_ic;
};

int	smtiic_match(struct device *, void *, void *);
void	smtiic_attach(struct device *, struct device *, void *);

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

const struct cfattach smtiic_ca = {
	sizeof(struct smtiic_softc), smtiic_match, smtiic_attach
};

struct cfdriver smtiic_cd = {
	NULL, "smtiic", DV_DULL
};

int	smtiic_i2c_acquire_bus(void *, int);
void	smtiic_i2c_release_bus(void *, int);
int	smtiic_wait_state(struct smtiic_softc *, uint32_t, uint32_t);
int	smtiic_send_start(void *, int);
int	smtiic_send_stop(void *, int);
int	smtiic_initiate_xfer(void *, i2c_addr_t, int);
int	smtiic_read_byte(void *, uint8_t *, int);
int	smtiic_write_byte(void *, uint8_t, int);

void	smtiic_bus_scan(struct device *, struct i2cbus_attach_args *, void *);

int
smtiic_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "spacemit,k1-i2c");
}

void
smtiic_attach(struct device *parent, struct device *self, void *aux)
{
	struct smtiic_softc *sc = (struct smtiic_softc *)self;
	struct fdt_attach_args *faa = aux;
	struct i2cbus_attach_args iba;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}
	sc->sc_node = faa->fa_node;

	printf("\n");

	pinctrl_byname(sc->sc_node, "default");

	clock_enable_all(sc->sc_node);
	reset_deassert_all(sc->sc_node);

	sc->sc_freq = OF_getpropint(sc->sc_node, "clock-frequency", 100000);

	/* reset */
	HWRITE4(sc, ICR, ICR_UR);
	delay(5);
	HWRITE4(sc, ICR, 0);
	HWRITE4(sc, ISR, ISR_INIT);

	/* set defaults */
	HSET4(sc, ICR, ICR_SCLE | ICR_GCD);
	if (sc->sc_freq == 400000)
		HSET4(sc, ICR, ICR_MODE_FAST);

	sc->sc_ic.ic_cookie = sc;
	sc->sc_ic.ic_acquire_bus = smtiic_i2c_acquire_bus;
	sc->sc_ic.ic_release_bus = smtiic_i2c_release_bus;
	sc->sc_ic.ic_exec = NULL;
	sc->sc_ic.ic_send_start = smtiic_send_start;
	sc->sc_ic.ic_send_stop = smtiic_send_stop;
	sc->sc_ic.ic_initiate_xfer = smtiic_initiate_xfer;
	sc->sc_ic.ic_read_byte = smtiic_read_byte;
	sc->sc_ic.ic_write_byte = smtiic_write_byte;

	bzero(&iba, sizeof iba);
	iba.iba_name = "iic";
	iba.iba_tag = &sc->sc_ic;
	iba.iba_bus_scan = smtiic_bus_scan;
	iba.iba_bus_scan_arg = &sc->sc_node;
	config_found(&sc->sc_dev, &iba, iicbus_print);
}

int
smtiic_wait_state(struct smtiic_softc *sc, uint32_t mask, uint32_t value)
{
	uint32_t state;
	int timeout;

	for (timeout = 1000; timeout > 0; timeout--) {
		if (((state = HREAD4(sc, ISR)) & mask) == value)
			return 0;
		delay(10);
	}
	return ETIMEDOUT;
}

int
smtiic_i2c_acquire_bus(void *cookie, int flags)
{
	struct smtiic_softc *sc = cookie;

	HSET4(sc, ICR, ICR_IUE);
	delay(10);

	return 0;
}

void
smtiic_i2c_release_bus(void *cookie, int flags)
{
	struct smtiic_softc *sc = cookie;

	HCLR4(sc, ICR, ICR_IUE);
}

int
smtiic_send_start(void *v, int flags)
{
	struct smtiic_softc *sc = v;

	HSET4(sc, ICR, ICR_START);
	return 0;
}

int
smtiic_send_stop(void *v, int flags)
{
	struct smtiic_softc *sc = v;

	HSET4(sc, ICR, ICR_STOP);
	return 0;
}

int
smtiic_initiate_xfer(void *v, i2c_addr_t addr, int flags)
{
	struct smtiic_softc *sc = v;

	if (smtiic_wait_state(sc, ISR_IBB, 0))
		return EIO;

	HCLR4(sc, ICR, ICR_START);
	HCLR4(sc, ICR, ICR_STOP);
	HCLR4(sc, ICR, ICR_ACKNAK);
	if (flags & I2C_F_READ)
		HWRITE4(sc, IDBR, addr << 1 | 1);
	else
		HWRITE4(sc, IDBR, addr << 1);
	HSET4(sc, ICR, ICR_START);

	HSET4(sc, ICR, ICR_TB);
	if (smtiic_wait_state(sc, ISR_ITE, ISR_ITE))
		return EIO;
	HWRITE4(sc, ISR, ISR_ITE);
	if (HREAD4(sc, ISR) & ISR_ACKNAK)
		return EIO;

	return 0;
}

int
smtiic_read_byte(void *v, uint8_t *valp, int flags)
{
	struct smtiic_softc *sc = v;

	if (smtiic_wait_state(sc, ISR_IBB, 0))
		return EIO;

	HCLR4(sc, ICR, ICR_START);
	HCLR4(sc, ICR, ICR_STOP);
	HCLR4(sc, ICR, ICR_ACKNAK);
	if ((flags & (I2C_F_LAST | I2C_F_STOP)) == (I2C_F_LAST | I2C_F_STOP))
		HSET4(sc, ICR, ICR_STOP);
	if (flags & I2C_F_LAST)
		HSET4(sc, ICR, ICR_ACKNAK);

	HSET4(sc, ICR, ICR_TB);
	if (smtiic_wait_state(sc, ISR_IRF, ISR_IRF))
		return EIO;
	*valp = HREAD4(sc, IDBR);
	HWRITE4(sc, ISR, ISR_IRF);

	return 0;
}

int
smtiic_write_byte(void *v, uint8_t val, int flags)
{
	struct smtiic_softc *sc = v;

	HCLR4(sc, ICR, ICR_START);
	HCLR4(sc, ICR, ICR_STOP);
	HCLR4(sc, ICR, ICR_ACKNAK);
	HWRITE4(sc, IDBR, val);
	if (flags & I2C_F_STOP)
		HSET4(sc, ICR, ICR_STOP);

	HSET4(sc, ICR, ICR_TB);
	if (smtiic_wait_state(sc, ISR_ITE, ISR_ITE))
		return EIO;
	HWRITE4(sc, ISR, ISR_ITE);
	if (HREAD4(sc, ISR) & ISR_ACKNAK)
		return EIO;

	return 0;
}

void
smtiic_bus_scan(struct device *self, struct i2cbus_attach_args *iba, void *aux)
{
	int iba_node = *(int *)aux;
	struct i2c_attach_args ia;
	char name[32];
	uint32_t reg[1];
	int node;

	for (node = OF_child(iba_node); node; node = OF_peer(node)) {
		memset(name, 0, sizeof(name));
		memset(reg, 0, sizeof(reg));

		if (!OF_is_enabled(node))
			continue;

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

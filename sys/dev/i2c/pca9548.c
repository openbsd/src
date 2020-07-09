/*	$OpenBSD: pca9548.c,v 1.1 2020/06/18 18:05:00 kettenis Exp $	*/

/*
 * Copyright (c) 2020 Mark Kettenis
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

#define _I2C_PRIVATE
#include <dev/i2c/i2cvar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_misc.h>

#define PCA9548_NUM_CHANNELS	8

struct pcamux_bus {
	struct pcamux_softc	*pb_sc;
	int			pb_channel;
	struct i2c_controller	pb_ic;
	struct i2c_bus		pb_ib;
};

struct pcamux_softc {
	struct device		sc_dev;
	i2c_tag_t		sc_tag;
	i2c_addr_t		sc_addr;
	
	int			sc_node;
	int			sc_channel;
	struct pcamux_bus	sc_bus[PCA9548_NUM_CHANNELS];
	struct rwlock		sc_lock;
};

int	pcamux_match(struct device *, void *, void *);
void	pcamux_attach(struct device *, struct device *, void *);

struct cfattach pcamux_ca = {
	sizeof(struct pcamux_softc), pcamux_match, pcamux_attach
};

struct cfdriver pcamux_cd = {
	NULL, "pcamux", DV_DULL
};

int	pcamux_acquire_bus(void *, int);
void	pcamux_release_bus(void *, int);
int	pcamux_exec(void *, i2c_op_t, i2c_addr_t, const void *, size_t,
	    void *, size_t, int);
void	pcamux_bus_scan(struct device *, struct i2cbus_attach_args *, void *);

int
pcamux_match(struct device *parent, void *match, void *aux)
{
	struct i2c_attach_args *ia = aux;

	if (strcmp(ia->ia_name, "nxp,pca9548") == 0)
		return (1);
	return (0);
}

void
pcamux_attach(struct device *parent, struct device *self, void *aux)
{
	struct pcamux_softc *sc = (struct pcamux_softc *)self;
	struct i2c_attach_args *ia = aux;
	int node = *(int *)ia->ia_cookie;

	sc->sc_tag = ia->ia_tag;
	sc->sc_addr = ia->ia_addr;

	sc->sc_node = node;
	sc->sc_channel = -1;	/* unknown */
	rw_init(&sc->sc_lock, sc->sc_dev.dv_xname);

	printf("\n");

	for (node = OF_child(node); node; node = OF_peer(node)) {
		struct i2cbus_attach_args iba;
		struct pcamux_bus *pb;
		uint32_t channel;

		channel = OF_getpropint(node, "reg", -1);
		if (channel >= PCA9548_NUM_CHANNELS)
			continue;

		pb = &sc->sc_bus[channel];
		pb->pb_sc = sc;
		pb->pb_channel = channel;
		pb->pb_ic.ic_cookie = pb;
		pb->pb_ic.ic_acquire_bus = pcamux_acquire_bus;
		pb->pb_ic.ic_release_bus = pcamux_release_bus;
		pb->pb_ic.ic_exec = pcamux_exec;

		/* Configure the child busses. */
		memset(&iba, 0, sizeof(iba));
		iba.iba_name = "iic";
		iba.iba_tag = &pb->pb_ic;
		iba.iba_bus_scan = pcamux_bus_scan;
		iba.iba_bus_scan_arg = &sc->sc_node;

		config_found(&sc->sc_dev, &iba, iicbus_print);

		pb->pb_ib.ib_node = node;
		pb->pb_ib.ib_ic = &pb->pb_ic;
		i2c_register(&pb->pb_ib);
	}
}

int
pcamux_set_channel(struct pcamux_softc *sc, int channel, int flags)
{
	uint8_t data;
	int error;

	if (channel < -1 || channel >= PCA9548_NUM_CHANNELS)
		return ENXIO;

	if (sc->sc_channel == channel)
		return 0;

	data = (channel == -1) ? 0 : (1 << channel);
	error = iic_exec(sc->sc_tag, I2C_OP_WRITE_WITH_STOP,
	    sc->sc_addr, NULL, 0, &data, sizeof data, flags);

	return error;
}

int
pcamux_acquire_bus(void *cookie, int flags)
{
	struct pcamux_bus *pb = cookie;
	struct pcamux_softc *sc = pb->pb_sc;
	int rwflags = RW_WRITE;
	int error;

	if (flags & I2C_F_POLL)
		rwflags |= RW_NOSLEEP;

	error = rw_enter(&sc->sc_lock, rwflags);
	if (error)
		return error;

	/* Acquire parent bus. */
	error = iic_acquire_bus(sc->sc_tag, flags);
	if (error) {
		rw_exit_write(&sc->sc_lock);
		return error;
	}

	error = pcamux_set_channel(sc, pb->pb_channel, flags);
	if (error) {
		iic_release_bus(sc->sc_tag, flags);
		rw_exit_write(&sc->sc_lock);
		return error;
	}

	return 0;
}

void
pcamux_release_bus(void *cookie, int flags)
{
	struct pcamux_bus *pb = cookie;
	struct pcamux_softc *sc = pb->pb_sc;

	/* Release parent bus. */
	iic_release_bus(sc->sc_tag, flags);
	rw_exit_write(&sc->sc_lock);
}

int
pcamux_exec(void *cookie, i2c_op_t op, i2c_addr_t addr, const void *cmd,
    size_t cmdlen, void *buf, size_t buflen, int flags)
{
	struct pcamux_bus *pb = cookie;
	struct pcamux_softc *sc = pb->pb_sc;

	rw_assert_wrlock(&sc->sc_lock);

	/* Issue the transaction on the parent bus. */
	return iic_exec(sc->sc_tag, op, addr, cmd, cmdlen, buf, buflen, flags);
}

void
pcamux_bus_scan(struct device *self, struct i2cbus_attach_args *iba, void *arg)
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

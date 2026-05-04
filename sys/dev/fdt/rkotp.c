/*	$OpenBSD: rkotp.c,v 1.1 2026/05/04 08:02:05 kettenis Exp $	*/
/*
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
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/mutex.h>

#include <machine/fdt.h>
#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

/* Registers */
#define RK3588_OTPC_AUTO_CTRL		0x0004
#define  RK3588_OTPC_AUTO_CTRL_ADDR(x)	((x) << 16)
#define  RK3588_OTPC_AUTO_CTRL_BURST(x)	((x) << 8)
#define RK3588_OTPC_AUTO_EN		0x0008
#define RK3588_OTPC_DOUT0		0x0020
#define RK3588_OTPC_INT_ST		0x0084
#define  RK3588_OTPC_INT_ST_RD_DONE	(1 << 1)

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct rkotp_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t 	sc_ioh;
	int			sc_node;

	bus_size_t		sc_size;
	struct nvmem_device	sc_nd;
};

int	rkotp_match(struct device *, void *, void *);
void	rkotp_attach(struct device *, struct device *, void *);

const struct cfattach rkotp_ca = {
	sizeof(struct rkotp_softc), rkotp_match, rkotp_attach
};

struct cfdriver rkotp_cd = {
	NULL, "rkotp", DV_DULL
};

int	rkotp_read(void *, bus_addr_t, void *, bus_size_t);

int
rkotp_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "rockchip,rk3576-otp");
}

void
rkotp_attach(struct device *parent, struct device *self, void *aux)
{
	struct rkotp_softc *sc = (struct rkotp_softc *) self;
	struct fdt_attach_args *faa = aux;

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

	sc->sc_size = 0x100;
	
	sc->sc_nd.nd_node = faa->fa_node;
	sc->sc_nd.nd_cookie = sc;
	sc->sc_nd.nd_read = rkotp_read;
	nvmem_register(&sc->sc_nd);
}

int
rkotp_read(void *cookie, bus_addr_t addr, void *data, bus_size_t size)
{
	struct rkotp_softc *sc = cookie;
	uint8_t *buf = data;
	uint32_t stat, val;
	int pos, timo, i;

	if (addr >= sc->sc_size || addr + size > sc->sc_size)
		return EINVAL;

	clock_enable_all(sc->sc_node);

	reset_assert_all(sc->sc_node);
	delay(2);
	reset_deassert_all(sc->sc_node);

	pos = 0;
	while (pos < size) {
		HWRITE4(sc, RK3588_OTPC_AUTO_CTRL,
		    RK3588_OTPC_AUTO_CTRL_ADDR(addr / 4) |
		    RK3588_OTPC_AUTO_CTRL_BURST(1));
		HWRITE4(sc, RK3588_OTPC_AUTO_EN, 1);

		for (timo =10000; timo > 0; timo--) {
			stat = HREAD4(sc, RK3588_OTPC_INT_ST);
			if (stat & RK3588_OTPC_INT_ST_RD_DONE)
				break;
			delay(1);
		}
		if (timo == 0)
			return EIO;

		val = HREAD4(sc, RK3588_OTPC_DOUT0);
		for (i = 0; i < (addr & 0x3); i++)
			val >>= 8;
		for (; i < 4 && pos < size; i++) {
			buf[pos++] = val & 0xff;
			val >>= 8;
			addr++;
		}
	}

	clock_disable_all(sc->sc_node);

	return 0;
}

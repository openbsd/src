/*	$OpenBSD: sdhc_fdt.c,v 1.3 2018/08/06 10:52:30 patrick Exp $	*/
/*
 * Copyright (c) 2017 Mark Kettenis
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
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/intr.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

#include <dev/sdmmc/sdhcreg.h>
#include <dev/sdmmc/sdhcvar.h>
#include <dev/sdmmc/sdmmcvar.h>

struct sdhc_fdt_softc {
	struct sdhc_softc 	sc;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_size;
	void			*sc_ih;

	struct sdhc_host 	*sc_host;
};

int	sdhc_fdt_match(struct device *, void *, void *);
void	sdhc_fdt_attach(struct device *, struct device *, void *);

struct cfattach sdhc_fdt_ca = {
	sizeof(struct sdhc_fdt_softc), sdhc_fdt_match, sdhc_fdt_attach
};

int	sdhc_fdt_signal_voltage(struct sdhc_softc *, int);

int
sdhc_fdt_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "arasan,sdhci-5.1");
}

void
sdhc_fdt_attach(struct device *parent, struct device *self, void *aux)
{
	struct sdhc_fdt_softc *sc = (struct sdhc_fdt_softc *)self;
	struct fdt_attach_args *faa = aux;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	sc->sc_size = faa->fa_reg[0].size;

	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	pinctrl_byname(faa->fa_node, "default");

	clock_enable_all(faa->fa_node);
	reset_deassert_all(faa->fa_node);

	sc->sc_ih = fdt_intr_establish(faa->fa_node, IPL_BIO,
	    sdhc_intr, sc, sc->sc.sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt\n");
		goto unmap;
	}

	printf("\n");

	sc->sc.sc_host = &sc->sc_host;
	sc->sc.sc_dmat = faa->fa_dmat;

	/*
	 * Arasan controller always uses 1.8V and doesn't like an
	 * explicit switch.
	 * 
	 */
	if (OF_is_compatible(faa->fa_node, "arasan,shdc-5,1"))
		sc->sc.sc_signal_voltage = sdhc_fdt_signal_voltage;

	/*
	 * Rockchip RK3399 PHY doesn't like being powered down at low
	 * clock speeds.
	 */
	if (OF_is_compatible(faa->fa_node, "rockchip,rk3399-sdhci-5.1"))
		sc->sc.sc_flags |= SDHC_F_NOPWR0;

	/* XXX Doesn't work on Rockchip RK3399. */
	sc->sc.sc_flags |= SDHC_F_NODDR50;

	sdhc_host_found(&sc->sc, sc->sc_iot, sc->sc_ioh, sc->sc_size, 1, 0);
	return;

unmap:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_size);
}

int
sdhc_fdt_signal_voltage(struct sdhc_softc *sc, int signal_voltage)
{
	switch (signal_voltage) {
	case SDMMC_SIGNAL_VOLTAGE_180:
		return 0;
	default:
		return EINVAL;
	}
}

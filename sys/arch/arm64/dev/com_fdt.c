/* $OpenBSD: com_fdt.c,v 1.3 2017/08/29 13:33:03 jsg Exp $ */
/*
 * Copyright (c) 2016 Patrick Wildt <patrick@blueri.se>
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
#include <sys/tty.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#include <dev/cons.h>

#include <arm64/arm64/arm64var.h>

#include <dev/ofw/fdt.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_pinctrl.h>

#define com_usr 31	/* Synopsys DesignWare UART */

int	com_fdt_match(struct device *, void *, void *);
void	com_fdt_attach(struct device *, struct device *, void *);
int	com_fdt_intr_designware(void *);

struct com_fdt_softc {
	struct com_softc	 sc;
	struct bus_space	 sc_iot;
};

struct cfattach com_fdt_ca = {
	sizeof (struct com_fdt_softc), com_fdt_match, com_fdt_attach
};

int com_fdt_cngetc(dev_t);
void com_fdt_cnputc(dev_t, int);
void com_fdt_cnpollc(dev_t, int);

struct consdev com_fdt_cons = {
	NULL, NULL, com_fdt_cngetc, com_fdt_cnputc, com_fdt_cnpollc, NULL,
	NODEV, CN_LOWPRI
};

void
com_fdt_init_cons(void)
{
	struct fdt_reg reg;
	void *node;

	if ((node = fdt_find_cons("brcm,bcm2835-aux-uart")) == NULL &&
	    (node = fdt_find_cons("snps,dw-apb-uart")) == NULL &&
	    (node = fdt_find_cons("ti,omap3-uart")) == NULL &&
	    (node = fdt_find_cons("ti,omap4-uart")) == NULL)
			return;
	if (fdt_get_reg(node, 0, &reg))
		return;

	/*
	 * Figuring out the clock frequency is rather complicated as
	 * on many SoCs this requires traversing a fair amount of the
	 * clock tree.  Instead we rely on the firmware to set up the
	 * console for us and bypass the cominit() call that
	 * comcnattach() does by doing the minimal setup here.
	 */

	comconsiot = &arm64_a4x_bs_tag;
	if (bus_space_map(comconsiot, reg.addr, reg.size, 0, &comconsioh))
		return;

	comconsrate = B115200;
	cn_tab = &com_fdt_cons;
}

int
com_fdt_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "brcm,bcm2835-aux-uart") ||
	    OF_is_compatible(faa->fa_node, "snps,dw-apb-uart") ||
	    OF_is_compatible(faa->fa_node, "ti,omap3-uart") ||
	    OF_is_compatible(faa->fa_node, "ti,omap4-uart"));
}

void
com_fdt_attach(struct device *parent, struct device *self, void *aux)
{
	struct com_fdt_softc *sc = (struct com_fdt_softc *)self;
	struct fdt_attach_args *faa = aux;
	int (*intr)(void *) = comintr;
	uint32_t freq;

	if (faa->fa_nreg < 1)
		return;

	clock_enable(faa->fa_node, NULL);

	/*
	 * Determine the clock frequency after enabling the clock.
	 * This gives the clock code a chance to configure the
	 * appropriate frequency for us.
	 */
	freq = OF_getpropint(faa->fa_node, "clock-frequency", 0);
	if (freq == 0)
		freq = clock_get_frequency(faa->fa_node, NULL);

	/*
	 * XXX This sucks.  We need to get rid of the a4x bus tag
	 * altogether.  For this we will need to change com(4).
	 */
	sc->sc_iot = arm64_a4x_bs_tag;
	sc->sc_iot.bus_base = faa->fa_iot->bus_base;
	sc->sc_iot.bus_private = faa->fa_iot->bus_private;
	sc->sc_iot._space_map = faa->fa_iot->_space_map;

	sc->sc.sc_iot = &sc->sc_iot;
	sc->sc.sc_iobase = faa->fa_reg[0].addr;
	sc->sc.sc_uarttype = COM_UART_16550;
	sc->sc.sc_frequency = freq ? freq : COM_FREQ;

	if (OF_is_compatible(faa->fa_node, "snps,dw-apb-uart"))
		intr = com_fdt_intr_designware;

	if (OF_is_compatible(faa->fa_node, "ti,omap3-uart") ||
	    OF_is_compatible(faa->fa_node, "ti,omap4-uart"))
		sc->sc.sc_uarttype = COM_UART_TI16750;

	if (stdout_node == faa->fa_node) {
		SET(sc->sc.sc_hwflags, COM_HW_CONSOLE);
		SET(sc->sc.sc_swflags, COM_SW_SOFTCAR);
		comconsfreq = sc->sc.sc_frequency;
	}

	if (bus_space_map(sc->sc.sc_iot, sc->sc.sc_iobase,
	    faa->fa_reg[0].size, 0, &sc->sc.sc_ioh)) {
		printf("%s: bus_space_map failed\n", __func__);
		return;
	}

	pinctrl_byname(faa->fa_node, "default");

	com_attach_subr(&sc->sc);

	arm_intr_establish_fdt(faa->fa_node, IPL_TTY, intr,
	    sc, sc->sc.sc_dev.dv_xname);
}

int
com_fdt_intr_designware(void *cookie)
{
	struct com_softc *sc = cookie;

	bus_space_read_1(sc->sc_iot, sc->sc_ioh, com_usr);

	return comintr(sc);
}

int
com_fdt_cngetc(dev_t dev)
{
	return com_common_getc(comconsiot, comconsioh);
}

void
com_fdt_cnputc(dev_t dev, int c)
{
	com_common_putc(comconsiot, comconsioh, c);
}

void
com_fdt_cnpollc(dev_t dev, int on)
{
}

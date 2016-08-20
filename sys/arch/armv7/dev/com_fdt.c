/* $OpenBSD: com_fdt.c,v 1.7 2016/08/20 10:41:54 kettenis Exp $ */
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

/* pick up armv7_a4x_bs_tag */
#include <arch/arm/armv7/armv7var.h>

#include <armv7/armv7/armv7var.h>
#include <armv7/armv7/armv7_machdep.h>

#include <dev/ofw/fdt.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pinctrl.h>

#define com_usr 31	/* Synopsys DesignWare UART */

int	com_fdt_match(struct device *, void *, void *);
void	com_fdt_attach(struct device *, struct device *, void *);
int	com_fdt_intr_designware(void *);

extern int comcnspeed;
extern int comcnmode;

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

	if ((node = fdt_find_cons("ti,omap3-uart")) == NULL &&
	    (node = fdt_find_cons("ti,omap4-uart")) == NULL &&
	    (node = fdt_find_cons("snps,dw-apb-uart")) == NULL)
			return;
	if (fdt_get_reg(node, 0, &reg))
		return;

	/*
	 * Figuring out the clock frequency is rather complicated as
	 * om many SoCs this requires traversing a fair amount of the
	 * clock tree.  Instead we rely on the firmware to set up the
	 * console for us and bypass the cominit() call that
	 * comcnattach() does by doing the minimal setup here.
	 */

	comconsiot = &armv7_a4x_bs_tag;
	if (bus_space_map(comconsiot, reg.addr, reg.size, 0, &comconsioh))
		return;

	comconsrate = comcnspeed;
	comconscflag = comcnmode;
	cn_tab = &com_fdt_cons;
}

int
com_fdt_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "ti,omap3-uart") ||
	    OF_is_compatible(faa->fa_node, "ti,omap4-uart") ||
	    OF_is_compatible(faa->fa_node, "snps,dw-apb-uart"));
}

void
com_fdt_attach(struct device *parent, struct device *self, void *aux)
{
	struct com_fdt_softc *sc = (struct com_fdt_softc *)self;
	struct fdt_attach_args *faa = aux;
	int (*intr)(void *) = comintr;
	int node;

	if (faa->fa_nreg < 1)
		return;

	/*
	 * XXX This sucks.  We need to get rid of the a4x bus tag
	 * altogether.  For this we will need to change com(4).
	 */
	sc->sc_iot = armv7_a4x_bs_tag;
	sc->sc_iot.bs_cookie = faa->fa_iot->bs_cookie;
	sc->sc_iot.bs_map = faa->fa_iot->bs_map;

	sc->sc.sc_iot = &sc->sc_iot;
	sc->sc.sc_iobase = faa->fa_reg[0].addr;
	sc->sc.sc_frequency = 48000000;
	sc->sc.sc_uarttype = COM_UART_TI16750;

	if (OF_is_compatible(faa->fa_node, "snps,dw-apb-uart")) {
		sc->sc.sc_uarttype = COM_UART_16550;
		intr = com_fdt_intr_designware;
	}

	if ((node = OF_finddevice("/")) != 0 &&
	    (OF_is_compatible(node, "allwinner,sun4i-a10") ||
	    OF_is_compatible(node, "allwinner,sun5i-a10s") ||
	    OF_is_compatible(node, "allwinner,sun5i-r8") ||
	    OF_is_compatible(node, "allwinner,sun7i-a20")))
		sc->sc.sc_frequency = 24000000;

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

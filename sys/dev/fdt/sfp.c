/* $OpenBSD: sfp.c,v 1.1 2019/09/07 13:32:36 patrick Exp $ */
/*
 * Copyright (c) 2019 Patrick Wildt <patrick@blueri.se>
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
#include <sys/kernel.h>
#include <sys/device.h>

#include <net/if.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/i2c/i2cvar.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_misc.h>

struct sfp_softc {
	struct device		 sc_dev;
	i2c_tag_t		 sc_tag;
	int			 sc_node;

	struct sfp_device	 sc_sd;
};

int	 sfp_match(struct device *, void *, void *);
void	 sfp_attach(struct device *, struct device *, void *);
int	 sfp_detach(struct device *, int);

int	 sfp_i2c_get_sffpage(void *, struct if_sffpage *);

struct cfattach sfp_ca = {
	sizeof(struct sfp_softc), sfp_match, sfp_attach, sfp_detach,
};

struct cfdriver sfp_cd = {
	NULL, "sfp", DV_DULL
};

int
sfp_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "sff,sfp") ||
	    OF_is_compatible(faa->fa_node, "sff,sfp+"));
}

void
sfp_attach(struct device *parent, struct device *self, void *aux)
{
	struct sfp_softc *sc = (struct sfp_softc *)self;
	struct fdt_attach_args *faa = aux;

	sc->sc_node = faa->fa_node;
	sc->sc_tag = i2c_byphandle(OF_getpropint(sc->sc_node,
	    "i2c-bus", 0));

	if (sc->sc_tag == NULL) {
		printf(": can't get i2c bus\n");
		return;
	}

	printf("\n");

	sc->sc_sd.sd_node = faa->fa_node;
	sc->sc_sd.sd_cookie = sc;
	sc->sc_sd.sd_get_sffpage = sfp_i2c_get_sffpage;
	sfp_register(&sc->sc_sd);
}

int
sfp_detach(struct device *self, int flags)
{
	return 0;
}

int
sfp_i2c_get_sffpage(void *cookie, struct if_sffpage *sff)
{
	struct sfp_softc *sc = cookie;
	uint8_t reg = sff->sff_page;

	iic_acquire_bus(sc->sc_tag, 0);
	if (iic_exec(sc->sc_tag, I2C_OP_READ_WITH_STOP,
	    sff->sff_addr >> 1, &reg, sizeof(reg),
	    sff->sff_data, sizeof(sff->sff_data), 0)) {
		printf("%s: cannot read register 0x%x\n",
		    sc->sc_dev.dv_xname, reg);
	}
	iic_release_bus(sc->sc_tag, 0);

	return 0;
}

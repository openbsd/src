/* $OpenBSD: imxpd.c,v 1.1 2018/05/02 15:17:30 patrick Exp $ */
/*
 * Copyright (c) 2018 Patrick Wildt <patrick@blueri.se>
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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/cpufunc.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_power.h>

#define FSL_SIP_GPC			0xc2000000
#define  FSL_SIP_CONFIG_GPC_PM_DOMAIN		0x03

struct imxpd_softc {
	struct device			sc_dev;
	struct power_domain_device	sc_pd;
	int				sc_domain_id;
};

int imxpd_match(struct device *, void *, void *);
void imxpd_attach(struct device *, struct device *, void *);

void imxpd_enable(void *, uint32_t *, int);


struct cfattach	imxpd_ca = {
	sizeof (struct imxpd_softc), imxpd_match, imxpd_attach
};

struct cfdriver imxpd_cd = {
	NULL, "imxpd", DV_DULL
};

int
imxpd_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "fsl,imx8mq-pm-domain");
}

void
imxpd_attach(struct device *parent, struct device *self, void *aux)
{
	struct imxpd_softc *sc = (struct imxpd_softc *)self;
	struct fdt_attach_args *faa = aux;

	sc->sc_domain_id = OF_getpropint(faa->fa_node, "domain-id", 0);

	sc->sc_pd.pd_node = faa->fa_node;
	sc->sc_pd.pd_cookie = sc;
	sc->sc_pd.pd_enable = imxpd_enable;
	power_domain_register(&sc->sc_pd);

	printf("\n");
}

void
imxpd_enable(void *cookie, uint32_t *cells, int on)
{
	struct imxpd_softc *sc = cookie;

	/* Set up power domain */
	smc_call(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN,
	    sc->sc_domain_id, on);
}

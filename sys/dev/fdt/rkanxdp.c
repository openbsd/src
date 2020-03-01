/* $OpenBSD: rkanxdp.c,v 1.2 2020/03/01 10:19:35 kettenis Exp $ */
/* $NetBSD: rk_anxdp.c,v 1.2 2020/01/04 12:08:32 jmcneill Exp $ */
/*-
 * Copyright (c) 2019 Jonathan A. Kollasch <jakllsch@kollasch.net>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/ofw_pinctrl.h>
#include <dev/ofw/fdt.h>

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>

#include <dev/ic/anxdp.h>

#define	RK3399_GRF_SOC_CON20		0x6250
#define	 EDP_LCDC_SEL				(1 << 5)

enum {
	ANXDP_PORT_INPUT = 0,
	ANXDP_PORT_OUTPUT = 1,
};

struct rkanxdp_port {
	struct rkanxdp_softc	*sc;
	struct rkanxdp_ep	*ep;
	int			nep;
};

struct rkanxdp_ep {
	struct rkanxdp_port	*port;
	struct video_device	vd;
};

struct rkanxdp_softc {
	struct anxdp_softc	sc_base;

	struct drm_encoder	sc_encoder;
	struct drm_display_mode	sc_curmode;
	struct regmap		*sc_grf;

	int			sc_activated;

	struct rkanxdp_port	*sc_port;
	int			sc_nport;
};

#define	to_rkanxdp_softc(x)	container_of(x, struct rkanxdp_softc, sc_base)
#define	to_rkanxdp_encoder(x)	container_of(x, struct rkanxdp_softc, sc_encoder)

int rkanxdp_match(struct device *, void *, void *);
void rkanxdp_attach(struct device *, struct device *, void *);

void rkanxdp_select_input(struct rkanxdp_softc *, u_int);
bool rkanxdp_encoder_mode_fixup(struct drm_encoder *,
    const struct drm_display_mode *, struct drm_display_mode *);
void rkanxdp_encoder_mode_set(struct drm_encoder *,
    struct drm_display_mode *, struct drm_display_mode *);
void rkanxdp_encoder_enable(struct drm_encoder *);
void rkanxdp_encoder_disable(struct drm_encoder *);
void rkanxdp_encoder_prepare(struct drm_encoder *);
void rkanxdp_encoder_commit(struct drm_encoder *);
void rkanxdp_encoder_dpms(struct drm_encoder *, int);

int rkanxdp_ep_activate(void *, struct drm_device *);
void *rkanxdp_ep_get_data(void *);

struct cfattach	rkanxdp_ca = {
	sizeof (struct rkanxdp_softc), rkanxdp_match, rkanxdp_attach
};

struct cfdriver rkanxdp_cd = {
	NULL, "rkanxdp", DV_DULL
};

int
rkanxdp_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "rockchip,rk3399-edp");
}

void
rkanxdp_attach(struct device *parent, struct device *self, void *aux)
{
	struct rkanxdp_softc *sc = (struct rkanxdp_softc *)self;
	struct fdt_attach_args *faa = aux;
	int i, j, ep, port, ports, grf;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	pinctrl_byname(faa->fa_node, "default");

	reset_deassert(faa->fa_node, "dp");

	clock_enable(faa->fa_node, "pclk");
	clock_enable(faa->fa_node, "dp");
	clock_enable(faa->fa_node, "grf");

	sc->sc_base.sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_base.sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_base.sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	grf = OF_getpropint(faa->fa_node, "rockchip,grf", 0);
	sc->sc_grf = regmap_byphandle(grf);
	if (sc->sc_grf == NULL) {
		printf(": can't get grf\n");
		return;
	}

	printf(": eDP TX\n");

	sc->sc_base.sc_flags |= ANXDP_FLAG_ROCKCHIP;

	if (anxdp_attach(&sc->sc_base) != 0) {
		printf("%s: failed to attach driver\n",
		    sc->sc_base.sc_dev.dv_xname);
		return;
	}

	ports = OF_getnodebyname(faa->fa_node, "ports");
	if (!ports)
		return;

	for (port = OF_child(ports); port; port = OF_peer(port))
		sc->sc_nport++;
	if (!sc->sc_nport)
		return;

	sc->sc_port = mallocarray(sc->sc_nport, sizeof(*sc->sc_port), M_DEVBUF,
	    M_WAITOK | M_ZERO);
	for (i = 0, port = OF_child(ports); port; port = OF_peer(port), i++) {
		for (ep = OF_child(port); ep; ep = OF_peer(ep))
			sc->sc_port[i].nep++;
		if (!sc->sc_port[i].nep)
			continue;
		sc->sc_port[i].sc = sc;
		sc->sc_port[i].ep = mallocarray(sc->sc_port[i].nep,
		    sizeof(*sc->sc_port[i].ep), M_DEVBUF, M_WAITOK | M_ZERO);
		for (j = 0, ep = OF_child(port); ep; ep = OF_peer(ep), j++) {
			sc->sc_port[i].ep[j].port = &sc->sc_port[i];
			sc->sc_port[i].ep[j].vd.vd_node = ep;
			sc->sc_port[i].ep[j].vd.vd_cookie =
			    &sc->sc_port[i].ep[j];
			sc->sc_port[i].ep[j].vd.vd_ep_activate =
			    rkanxdp_ep_activate;
			sc->sc_port[i].ep[j].vd.vd_ep_get_data =
			    rkanxdp_ep_get_data;
			video_register(&sc->sc_port[i].ep[j].vd);
		}
	}
}

void
rkanxdp_select_input(struct rkanxdp_softc *sc, u_int crtc_index)
{
	uint32_t write_mask = EDP_LCDC_SEL << 16;
	uint32_t write_val = crtc_index == 0 ? EDP_LCDC_SEL : 0;

	regmap_write_4(sc->sc_grf, RK3399_GRF_SOC_CON20, write_mask | write_val);
}

bool
rkanxdp_encoder_mode_fixup(struct drm_encoder *encoder,
    const struct drm_display_mode *mode, struct drm_display_mode *adjusted_mode)
{
	return true;
}

void
rkanxdp_encoder_mode_set(struct drm_encoder *encoder,
    struct drm_display_mode *mode, struct drm_display_mode *adjusted)
{
}

void
rkanxdp_encoder_enable(struct drm_encoder *encoder)
{
}

void
rkanxdp_encoder_disable(struct drm_encoder *encoder)
{
}

void
rkanxdp_encoder_prepare(struct drm_encoder *encoder)
{
	struct rkanxdp_softc *sc = to_rkanxdp_encoder(encoder);
	u_int crtc_index = drm_crtc_index(encoder->crtc);

	rkanxdp_select_input(sc, crtc_index);
}

void
rkanxdp_encoder_commit(struct drm_encoder *encoder)
{
}

void
rkanxdp_encoder_dpms(struct drm_encoder *encoder, int mode)
{
	struct rkanxdp_softc *sc = to_rkanxdp_encoder(encoder);

	anxdp_dpms(&sc->sc_base, mode);
}

struct drm_encoder_funcs rkanxdp_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

struct drm_encoder_helper_funcs rkanxdp_encoder_helper_funcs = {
	.prepare = rkanxdp_encoder_prepare,
	.mode_fixup = rkanxdp_encoder_mode_fixup,
	.mode_set = rkanxdp_encoder_mode_set,
	.enable = rkanxdp_encoder_enable,
	.disable = rkanxdp_encoder_disable,
	.commit = rkanxdp_encoder_commit,
	.dpms = rkanxdp_encoder_dpms,
};

int
rkanxdp_ep_activate(void *cookie, struct drm_device *ddev)
{
	struct rkanxdp_ep *ep = cookie;
	struct rkanxdp_port *port = ep->port;
	struct rkanxdp_softc *sc = port->sc;
	int error;

	if (sc->sc_activated)
		return 0;

	if (OF_getpropint(OF_parent(ep->vd.vd_node), "reg", 0) != ANXDP_PORT_INPUT)
		return EINVAL;

	sc->sc_encoder.possible_crtcs = 0x3; /* XXX */
	drm_encoder_init(ddev, &sc->sc_encoder, &rkanxdp_encoder_funcs,
	    DRM_MODE_ENCODER_TMDS, NULL);
	drm_encoder_helper_add(&sc->sc_encoder, &rkanxdp_encoder_helper_funcs);

	sc->sc_base.sc_connector.base.connector_type = DRM_MODE_CONNECTOR_eDP;
	error = anxdp_bind(&sc->sc_base, &sc->sc_encoder);
	if (error != 0)
		return error;

	sc->sc_activated = 1;
	return 0;
}

void *
rkanxdp_ep_get_data(void *cookie)
{
	struct rkanxdp_ep *ep = cookie;
	struct rkanxdp_port *port = ep->port;
	struct rkanxdp_softc *sc = port->sc;

	return &sc->sc_encoder;
}

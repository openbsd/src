/* $OpenBSD: rkvop.c,v 1.3 2020/04/07 15:23:42 kettenis Exp $ */
/* $NetBSD: rk_vop.c,v 1.6 2020/01/05 12:14:35 mrg Exp $ */
/*-
 * Copyright (c) 2019 Jared D. McNeill <jmcneill@invisible.ca>
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
#include <dev/ofw/fdt.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>

#include <dev/fdt/rkdrm.h>

#define	VOP_REG_CFG_DONE		0x0000
#define	 REG_LOAD_EN			(1 << 0)
#define	VOP_SYS_CTRL			0x0008
#define	 VOP_STANDBY_EN			(1 << 22)
#define	 MIPI_OUT_EN			(1 << 15)
#define	 EDP_OUT_EN			(1 << 14)
#define	 HDMI_OUT_EN			(1 << 13)
#define	 RGB_OUT_EN			(1 << 12)
#define	VOP_DSP_CTRL0			0x0010
#define	 DSP_OUT_MODE(x)		((x) << 0)
#define	  DSP_OUT_MODE_MASK		0xf
#define	  DSP_OUT_MODE_RGB888		0
#define	  DSP_OUT_MODE_RGBaaa		15
#define	VOP_DSP_CTRL1			0x0014
#define	VOP_WIN0_CTRL			0x0030
#define	 WIN0_LB_MODE(x)		((x) << 5)
#define	  WIN0_LB_MODE_MASK		0x7
#define	  WIN0_LB_MODE_RGB_3840X2	2
#define	  WIN0_LB_MODE_RGB_2560X4	3
#define	  WIN0_LB_MODE_RGB_1920X5	4
#define	  WIN0_LB_MODE_RGB_1280X8	5
#define	 WIN0_DATA_FMT(x)		((x) << 1)
#define	  WIN0_DATA_FMT_MASK		0x7
#define	  WIN0_DATA_FMT_ARGB888		0
#define	 WIN0_EN			(1 << 0)
#define	VOP_WIN0_COLOR_KEY		0x0038
#define	VOP_WIN0_VIR			0x003c
#define	 WIN0_VIR_STRIDE(x)		(((x) & 0x3fff) << 0)
#define	VOP_WIN0_YRGB_MST		0x0040
#define	VOP_WIN0_ACT_INFO		0x0048
#define	 WIN0_ACT_HEIGHT(x)		(((x) & 0x1fff) << 16)
#define	 WIN0_ACT_WIDTH(x)		(((x) & 0x1fff) << 0)
#define	VOP_WIN0_DSP_INFO		0x004c
#define	 WIN0_DSP_HEIGHT(x)		(((x) & 0xfff) << 16)
#define	 WIN0_DSP_WIDTH(x)		(((x) & 0xfff) << 0)
#define	VOP_WIN0_DSP_ST			0x0050
#define	 WIN0_DSP_YST(x)		(((x) & 0x1fff) << 16)
#define	 WIN0_DSP_XST(x)		(((x) & 0x1fff) << 0)
#define	VOP_POST_DSP_HACT_INFO		0x0170
#define	 DSP_HACT_ST_POST(x)		(((x) & 0x1fff) << 16)
#define	 DSP_HACT_END_POST(x)		(((x) & 0x1fff) << 0)
#define	VOP_POST_DSP_VACT_INFO		0x0174
#define	 DSP_VACT_ST_POST(x)		(((x) & 0x1fff) << 16)
#define	 DSP_VACT_END_POST(x)		(((x) & 0x1fff) << 0)
#define	VOP_DSP_HTOTAL_HS_END		0x0188
#define	 DSP_HS_END(x)			(((x) & 0x1fff) << 16)
#define	 DSP_HTOTAL(x)			(((x) & 0x1fff) << 0)
#define	VOP_DSP_HACT_ST_END		0x018c
#define	 DSP_HACT_ST(x)			(((x) & 0x1fff) << 16)
#define	 DSP_HACT_END(x)		(((x) & 0x1fff) << 0)
#define	VOP_DSP_VTOTAL_VS_END		0x0190
#define	 DSP_VS_END(x)			(((x) & 0x1fff) << 16)
#define	 DSP_VTOTAL(x)			(((x) & 0x1fff) << 0)
#define	VOP_DSP_VACT_ST_END		0x0194
#define	 DSP_VACT_ST(x)			(((x) & 0x1fff) << 16)
#define	 DSP_VACT_END(x)		(((x) & 0x1fff) << 0)

/*
 * Polarity fields are in different locations depending on SoC and output type,
 * but always in the same order.
 */
#define	DSP_DCLK_POL			(1 << 3)
#define	DSP_DEN_POL			(1 << 2)
#define	DSP_VSYNC_POL			(1 << 1)
#define	DSP_HSYNC_POL			(1 << 0)

enum vop_ep_type {
	VOP_EP_MIPI,
	VOP_EP_EDP,
	VOP_EP_HDMI,
	VOP_EP_MIPI1,
	VOP_EP_DP,
	VOP_NEP
};

struct rkvop_softc;
struct rkvop_config;

struct rkvop_crtc {
	struct drm_crtc		base;
	struct rkvop_softc	*sc;
};

struct rkvop_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	int			sc_node;
	struct rkvop_config	*sc_conf;

	struct rkvop_crtc	sc_crtc;
	struct device_ports	sc_ports;
};

#define	to_rkvop_crtc(x)	container_of(x, struct rkvop_crtc, base)

#define	HREAD4(sc, reg)				\
	bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg))
#define	HWRITE4(sc, reg, val)			\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct rkvop_config {
	char		*descr;
	u_int		out_mode;
	void		(*init)(struct rkvop_softc *);
	void		(*set_polarity)(struct rkvop_softc *,
					enum vop_ep_type, uint32_t);
};

int rkvop_match(struct device *, void *, void *);
void rkvop_attach(struct device *, struct device *, void *);

int rkvop_mode_do_set_base(struct drm_crtc *, struct drm_framebuffer *,
    int, int, int);
void rkvop_destroy(struct drm_crtc *);
void rkvop_dpms(struct drm_crtc *, int);
bool rkvop_mode_fixup(struct drm_crtc *, const struct drm_display_mode *,
    struct drm_display_mode *);
int rkvop_mode_set(struct drm_crtc *, struct drm_display_mode *,
    struct drm_display_mode *, int, int, struct drm_framebuffer *);
int rkvop_mode_set_base(struct drm_crtc *, int, int, struct drm_framebuffer *);
int rkvop_mode_set_base_atomic(struct drm_crtc *, struct drm_framebuffer *,
    int, int, enum mode_set_atomic);
void rkvop_disable(struct drm_crtc *);
void rkvop_prepare(struct drm_crtc *);
void rkvop_commit(struct drm_crtc *);

void rk3399_vop_init(struct rkvop_softc *);
void rk3399_vop_set_polarity(struct rkvop_softc *, enum vop_ep_type, uint32_t);

int rkvop_ep_activate(void *, struct endpoint *, void *);
void *rkvop_ep_get_cookie(void *, struct endpoint *);

struct rkvop_config rk3399_vop_big_config = {
	.descr = "RK3399 VOPB",
	.out_mode = DSP_OUT_MODE_RGBaaa,
	.init = rk3399_vop_init,
	.set_polarity = rk3399_vop_set_polarity,
};

struct rkvop_config rk3399_vop_lit_config = {
	.descr = "RK3399 VOPL",
	.out_mode = DSP_OUT_MODE_RGB888,
	.init = rk3399_vop_init,
	.set_polarity = rk3399_vop_set_polarity,
};

struct cfattach	rkvop_ca = {
	sizeof (struct rkvop_softc), rkvop_match, rkvop_attach
};

struct cfdriver rkvop_cd = {
	NULL, "rkvop", DV_DULL
};

int
rkvop_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "rockchip,rk3399-vop-big") ||
	    OF_is_compatible(faa->fa_node, "rockchip,rk3399-vop-lit"));
}

void
rkvop_attach(struct device *parent, struct device *self, void *aux)
{
	struct rkvop_softc *sc = (struct rkvop_softc *)self;
	struct fdt_attach_args *faa = aux;
	int i, port, ep, nep;
	paddr_t paddr;

	if (faa->fa_nreg < 1)
		return;

	clock_set_assigned(faa->fa_node);

	reset_deassert(faa->fa_node, "axi");
	reset_deassert(faa->fa_node, "ahb");
	reset_deassert(faa->fa_node, "dclk");

	clock_enable(faa->fa_node, "aclk_vop");
	clock_enable(faa->fa_node, "hclk_vop");
	clock_enable(faa->fa_node, "dclk_vop");

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}
	sc->sc_node = faa->fa_node;

	if (OF_is_compatible(faa->fa_node, "rockchip,rk3399-vop-big"))
		sc->sc_conf = &rk3399_vop_big_config;
	if (OF_is_compatible(faa->fa_node, "rockchip,rk3399-vop-lit"))
		sc->sc_conf = &rk3399_vop_lit_config;

	printf(": %s\n", sc->sc_conf->descr);

	if (sc->sc_conf->init != NULL)
		sc->sc_conf->init(sc);

	sc->sc_ports.dp_node = faa->fa_node;
	sc->sc_ports.dp_cookie = sc;
	sc->sc_ports.dp_ep_activate = rkvop_ep_activate;
	sc->sc_ports.dp_ep_get_cookie = rkvop_ep_get_cookie;
	device_ports_register(&sc->sc_ports, EP_DRM_CRTC);

	paddr = HREAD4(sc, VOP_WIN0_YRGB_MST);
	if (paddr != 0) {
		uint32_t stride, height;

		stride = HREAD4(sc, VOP_WIN0_VIR) & 0xffff;
		height = (HREAD4(sc, VOP_WIN0_DSP_INFO) >> 16) + 1;
		rasops_claim_framebuffer(paddr, height * stride * 4, self);
	}
}

int
rkvop_mode_do_set_base(struct drm_crtc *crtc, struct drm_framebuffer *fb,
    int x, int y, int atomic)
{
	struct rkvop_crtc *mixer_crtc = to_rkvop_crtc(crtc);
	struct rkvop_softc *sc = mixer_crtc->sc;
	struct rkdrm_framebuffer *sfb = atomic?
	    to_rkdrm_framebuffer(fb) :
	    to_rkdrm_framebuffer(crtc->primary->fb);

	uint64_t paddr = (uint64_t)sfb->obj->dmamap->dm_segs[0].ds_addr;

	paddr += y * sfb->base.pitches[0];
	paddr += x * drm_format_plane_cpp(sfb->base.format->format, 0);

	KASSERT((paddr & ~0xffffffff) == 0);

	uint32_t vir = WIN0_VIR_STRIDE(sfb->base.pitches[0] / 4);
	HWRITE4(sc, VOP_WIN0_VIR, vir);

	/* Framebuffer start address */
	HWRITE4(sc, VOP_WIN0_YRGB_MST, (uint32_t)paddr);

	return 0;
}

void
rkvop_destroy(struct drm_crtc *crtc)
{
	drm_crtc_cleanup(crtc);
}

struct drm_crtc_funcs rkvop_crtc_funcs = {
	.set_config = drm_crtc_helper_set_config,
	.destroy = rkvop_destroy,
};

void
rkvop_dpms(struct drm_crtc *crtc, int mode)
{
	struct rkvop_crtc *mixer_crtc = to_rkvop_crtc(crtc);
	struct rkvop_softc *sc = mixer_crtc->sc;
	uint32_t val;

	val = HREAD4(sc, VOP_SYS_CTRL);

	switch (mode) {
	case DRM_MODE_DPMS_ON:
		val &= ~VOP_STANDBY_EN;
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		val |= VOP_STANDBY_EN;
		break;
	}

	HWRITE4(sc, VOP_SYS_CTRL, val);

	/* Commit settings */
	HWRITE4(sc, VOP_REG_CFG_DONE, REG_LOAD_EN);
}

bool
rkvop_mode_fixup(struct drm_crtc *crtc,
   const struct drm_display_mode *mode, struct drm_display_mode *adjusted_mode)
{
	return true;
}

int
rkvop_mode_set(struct drm_crtc *crtc, struct drm_display_mode *mode,
    struct drm_display_mode *adjusted_mode, int x, int y,
    struct drm_framebuffer *old_fb)
{
	struct rkvop_crtc *mixer_crtc = to_rkvop_crtc(crtc);
	struct rkvop_softc *sc = mixer_crtc->sc;
	uint32_t val;
	u_int lb_mode;
	u_int pol;
	int connector_type = 0;
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;

	u_int hactive = adjusted_mode->hdisplay;
	u_int hsync_len = adjusted_mode->hsync_end - adjusted_mode->hsync_start;
	u_int hback_porch = adjusted_mode->htotal - adjusted_mode->hsync_end;
	u_int hfront_porch = adjusted_mode->hsync_start - adjusted_mode->hdisplay;

	u_int vactive = adjusted_mode->vdisplay;
	u_int vsync_len = adjusted_mode->vsync_end - adjusted_mode->vsync_start;
	u_int vback_porch = adjusted_mode->vtotal - adjusted_mode->vsync_end;
	u_int vfront_porch = adjusted_mode->vsync_start - adjusted_mode->vdisplay;

	clock_set_frequency(sc->sc_node, "dclk_vop", adjusted_mode->clock * 1000);

	val = WIN0_ACT_WIDTH(hactive - 1) |
	      WIN0_ACT_HEIGHT(vactive - 1);
	HWRITE4(sc, VOP_WIN0_ACT_INFO, val);

	val = WIN0_DSP_WIDTH(hactive - 1) |
	      WIN0_DSP_HEIGHT(vactive - 1);
	HWRITE4(sc, VOP_WIN0_DSP_INFO, val);

	val = WIN0_DSP_XST(hsync_len + hback_porch) |
	      WIN0_DSP_YST(vsync_len + vback_porch);
	HWRITE4(sc, VOP_WIN0_DSP_ST, val);

	HWRITE4(sc, VOP_WIN0_COLOR_KEY, 0);

	if (adjusted_mode->hdisplay > 2560)
		lb_mode = WIN0_LB_MODE_RGB_3840X2;
	else if (adjusted_mode->hdisplay > 1920)
		lb_mode = WIN0_LB_MODE_RGB_2560X4;
	else if (adjusted_mode->hdisplay > 1280)
		lb_mode = WIN0_LB_MODE_RGB_1920X5;
	else
		lb_mode = WIN0_LB_MODE_RGB_1280X8;

	val = WIN0_LB_MODE(lb_mode) |
	      WIN0_DATA_FMT(WIN0_DATA_FMT_ARGB888) |
	      WIN0_EN;
	HWRITE4(sc, VOP_WIN0_CTRL, val);

	rkvop_mode_do_set_base(crtc, old_fb, x, y, 0);

	pol = DSP_DCLK_POL;
	if ((adjusted_mode->flags & DRM_MODE_FLAG_PHSYNC) != 0)
		pol |= DSP_HSYNC_POL;
	if ((adjusted_mode->flags & DRM_MODE_FLAG_PVSYNC) != 0)
		pol |= DSP_VSYNC_POL;

	drm_connector_list_iter_begin(crtc->dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		if ((connector->encoder) == NULL)
			continue;
		if (connector->encoder->crtc == crtc) {
			connector_type = connector->connector_type;
			break;
		}
	}

	switch (connector_type) {
	case DRM_MODE_CONNECTOR_HDMIA:
		sc->sc_conf->set_polarity(sc, VOP_EP_HDMI, pol);
		break;
	case DRM_MODE_CONNECTOR_eDP:
		sc->sc_conf->set_polarity(sc, VOP_EP_EDP, pol);
		break;
	}

	val = HREAD4(sc, VOP_SYS_CTRL);
	val &= ~VOP_STANDBY_EN;
	val &= ~(MIPI_OUT_EN|EDP_OUT_EN|HDMI_OUT_EN|RGB_OUT_EN);

	switch (connector_type) {
	case DRM_MODE_CONNECTOR_HDMIA:
		val |= HDMI_OUT_EN;
		break;
	case DRM_MODE_CONNECTOR_eDP:
		val |= EDP_OUT_EN;
		break;
	}
	HWRITE4(sc, VOP_SYS_CTRL, val);

	val = HREAD4(sc, VOP_DSP_CTRL0);
	val &= ~DSP_OUT_MODE(DSP_OUT_MODE_MASK);
	val |= DSP_OUT_MODE(sc->sc_conf->out_mode);
	HWRITE4(sc, VOP_DSP_CTRL0, val);

	val = DSP_HACT_ST_POST(hsync_len + hback_porch) |
	      DSP_HACT_END_POST(hsync_len + hback_porch + hactive);
	HWRITE4(sc, VOP_POST_DSP_HACT_INFO, val);

	val = DSP_HACT_ST(hsync_len + hback_porch) |
	      DSP_HACT_END(hsync_len + hback_porch + hactive);
	HWRITE4(sc, VOP_DSP_HACT_ST_END, val);

	val = DSP_HTOTAL(hsync_len) |
	      DSP_HS_END(hsync_len + hback_porch + hactive + hfront_porch);
	HWRITE4(sc, VOP_DSP_HTOTAL_HS_END, val);

	val = DSP_VACT_ST_POST(vsync_len + vback_porch) |
	      DSP_VACT_END_POST(vsync_len + vback_porch + vactive);
	HWRITE4(sc, VOP_POST_DSP_VACT_INFO, val);

	val = DSP_VACT_ST(vsync_len + vback_porch) |
	      DSP_VACT_END(vsync_len + vback_porch + vactive);
	HWRITE4(sc, VOP_DSP_VACT_ST_END, val);

	val = DSP_VTOTAL(vsync_len) |
	      DSP_VS_END(vsync_len + vback_porch + vactive + vfront_porch);
	HWRITE4(sc, VOP_DSP_VTOTAL_VS_END, val);

	return 0;
}

int
rkvop_mode_set_base(struct drm_crtc *crtc, int x, int y,
    struct drm_framebuffer *old_fb)
{
	struct rkvop_crtc *mixer_crtc = to_rkvop_crtc(crtc);
	struct rkvop_softc *sc = mixer_crtc->sc;

	rkvop_mode_do_set_base(crtc, old_fb, x, y, 0);

	/* Commit settings */
	HWRITE4(sc, VOP_REG_CFG_DONE, REG_LOAD_EN);

	return 0;
}

int
rkvop_mode_set_base_atomic(struct drm_crtc *crtc, struct drm_framebuffer *fb,
    int x, int y, enum mode_set_atomic state)
{
	struct rkvop_crtc *mixer_crtc = to_rkvop_crtc(crtc);
	struct rkvop_softc *sc = mixer_crtc->sc;

	rkvop_mode_do_set_base(crtc, fb, x, y, 1);

	/* Commit settings */
	HWRITE4(sc, VOP_REG_CFG_DONE, REG_LOAD_EN);

	return 0;
}

void
rkvop_disable(struct drm_crtc *crtc)
{
}

void
rkvop_prepare(struct drm_crtc *crtc)
{
}

void
rkvop_commit(struct drm_crtc *crtc)
{
	struct rkvop_crtc *mixer_crtc = to_rkvop_crtc(crtc);
	struct rkvop_softc *sc = mixer_crtc->sc;

	/* Commit settings */
	HWRITE4(sc, VOP_REG_CFG_DONE, REG_LOAD_EN);
}

struct drm_crtc_helper_funcs rkvop_crtc_helper_funcs = {
	.dpms = rkvop_dpms,
	.mode_fixup = rkvop_mode_fixup,
	.mode_set = rkvop_mode_set,
	.mode_set_base = rkvop_mode_set_base,
	.mode_set_base_atomic = rkvop_mode_set_base_atomic,
	.disable = rkvop_disable,
	.prepare = rkvop_prepare,
	.commit = rkvop_commit,
};

int
rkvop_ep_activate(void *cookie, struct endpoint *ep, void *arg)
{
	struct rkvop_softc *sc = cookie;
	struct drm_device *ddev = arg;

	if (sc->sc_crtc.sc == NULL) {
		sc->sc_crtc.sc = sc;

		drm_crtc_init(ddev, &sc->sc_crtc.base, &rkvop_crtc_funcs);
		drm_crtc_helper_add(&sc->sc_crtc.base, &rkvop_crtc_helper_funcs);

		printf("%s: using CRTC %d for %s\n", sc->sc_dev.dv_xname,
		    drm_crtc_index(&sc->sc_crtc.base), sc->sc_conf->descr);
	}

	return 0;
}

void *
rkvop_ep_get_cookie(void *cookie, struct endpoint *ep)
{
	struct rkvop_softc *sc = cookie;
	return &sc->sc_crtc.base;
}

/*
 * RK3399 VOP
 */
#define	RK3399_VOP_POL_MASK		0xf
#define	RK3399_VOP_MIPI_POL(x)		((x) << 28)
#define	RK3399_VOP_EDP_POL(x)		((x) << 24)
#define	RK3399_VOP_HDMI_POL(x)		((x) << 20)
#define	RK3399_VOP_DP_POL(x)		((x) << 16)

#define	RK3399_VOP_SYS_CTRL_ENABLE	(1 << 11)

void
rk3399_vop_init(struct rkvop_softc *sc)
{
	uint32_t val;

	val = HREAD4(sc, VOP_SYS_CTRL);
	val |= RK3399_VOP_SYS_CTRL_ENABLE;
	HWRITE4(sc, VOP_SYS_CTRL, val);
}

void
rk3399_vop_set_polarity(struct rkvop_softc *sc, enum vop_ep_type ep_type, uint32_t pol)
{
	uint32_t mask, val;

	switch (ep_type) {
	case VOP_EP_MIPI:
	case VOP_EP_MIPI1:
		pol = RK3399_VOP_MIPI_POL(pol);
		mask = RK3399_VOP_MIPI_POL(RK3399_VOP_POL_MASK);
		break;
	case VOP_EP_EDP:
		pol = RK3399_VOP_EDP_POL(pol);
		mask = RK3399_VOP_EDP_POL(RK3399_VOP_POL_MASK);
		break;
	case VOP_EP_HDMI:
		pol = RK3399_VOP_HDMI_POL(pol);
		mask = RK3399_VOP_HDMI_POL(RK3399_VOP_POL_MASK);
		break;
	case VOP_EP_DP:
		pol = RK3399_VOP_DP_POL(pol);
		mask = RK3399_VOP_DP_POL(RK3399_VOP_POL_MASK);
		break;
	default:
		return;
	}

	val = HREAD4(sc, VOP_DSP_CTRL1);
	val &= ~mask;
	val |= pol;
	HWRITE4(sc, VOP_DSP_CTRL1, val);
}

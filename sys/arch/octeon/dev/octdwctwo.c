/*	$OpenBSD: octdwctwo.c,v 1.5 2015/03/19 10:44:21 mpi Exp $	*/

/*
 * Copyright (c) 2015 Masao Uebayashi <uebayasi@tombiinc.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/octeonreg.h>
#include <machine/octeonvar.h>

#include <octeon/dev/iobusvar.h>
#include <octeon/dev/octhcireg.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>

#include <dev/usb/dwc2/dwc2var.h>
#include <dev/usb/dwc2/dwc2.h>
#include <dev/usb/dwc2/dwc2_core.h>

struct octdwctwo_softc {
	struct dwc2_softc	sc_dwc2;

	/* USBN bus space */
	bus_space_tag_t		sc_bust;
	bus_space_handle_t	sc_regh;
	bus_space_handle_t	sc_regh2;

	void			*sc_ih;
};

int			octdwctwo_match(struct device *, void *, void *);
void			octdwctwo_attach(struct device *, struct device *,
			    void *);
int			octdwctwo_set_dma_addr(void *, dma_addr_t, int);
u_int64_t		octdwctwo_reg2_rd(struct octdwctwo_softc *, bus_size_t);
void			octdwctwo_reg2_wr(struct octdwctwo_softc *, bus_size_t,
			    u_int64_t);

const struct cfattach octdwctwo_ca = {
	sizeof(struct octdwctwo_softc), octdwctwo_match, octdwctwo_attach,
};

struct cfdriver dwctwo_cd = {
	NULL, "dwctwo", DV_DULL
};

static struct dwc2_core_params octdwctwo_params = {
	.otg_cap = 2,
	.otg_ver = 0,
	.dma_enable = 1,
	.dma_desc_enable = 0,
	.speed = 0,
	.enable_dynamic_fifo = 1,
	.en_multiple_tx_fifo = 0,
	.host_rx_fifo_size = 128/*XXX*/,
	.host_nperio_tx_fifo_size = 128/*XXX*/,
	.host_perio_tx_fifo_size = 128/*XXX*/,
	.max_transfer_size = 65535,
	.max_packet_count = 511,
	.host_channels = 8,
	.phy_type = 1,
	.phy_utmi_width = 16,
	.phy_ulpi_ddr = 0,
	.phy_ulpi_ext_vbus = 0,
	.i2c_enable = 0,
	.ulpi_fs_ls = 0,
	.host_support_fs_ls_low_power = 0,
	.host_ls_low_power_phy_clk = 0,
	.ts_dline = 0,
	.reload_ctl = 0,
	.ahbcfg = 0x7,
	.uframe_sched = 1,
};

static struct dwc2_core_dma_config octdwctwo_dma_config = {
	.set_dma_addr = octdwctwo_set_dma_addr,
};

int
octdwctwo_match(struct device *parent, void *match, void *aux)
{
	return (1);
}

void
octdwctwo_attach(struct device *parent, struct device *self, void *aux)
{
	struct octdwctwo_softc *sc = (struct octdwctwo_softc *)self;
	struct iobus_attach_args *aa = aux;
	int rc;

	sc->sc_dwc2.sc_iot = aa->aa_bust;
	sc->sc_dwc2.sc_bus.pipe_size = sizeof(struct usbd_pipe);
	sc->sc_dwc2.sc_bus.dmatag = aa->aa_dmat;
	sc->sc_dwc2.sc_params = &octdwctwo_params;

	rc = bus_space_map(aa->aa_bust, USBC_BASE, USBC_SIZE,
	    0, &sc->sc_dwc2.sc_ioh);
	KASSERT(rc == 0);

	sc->sc_bust = aa->aa_bust;
	rc = bus_space_map(sc->sc_bust, USBN_BASE, USBN_SIZE,
	    0, &sc->sc_regh);
	KASSERT(rc == 0);
	rc = bus_space_map(sc->sc_bust, USBN_2_BASE, USBN_2_SIZE,
	    0, &sc->sc_regh2);
	KASSERT(rc == 0);

	rc = dwc2_init(&sc->sc_dwc2);
	if (rc != 0)
		return;
	octdwctwo_dma_config.set_dma_addr_data = sc;
	rc = dwc2_dma_config(&sc->sc_dwc2, &octdwctwo_dma_config);
	if (rc != 0)
		return;
	sc->sc_dwc2.sc_child = config_found(&sc->sc_dwc2.sc_bus.bdev,
	    &sc->sc_dwc2.sc_bus, usbctlprint);

	sc->sc_ih = octeon_intr_establish(CIU_INT_USB, IPL_USB, dwc2_intr,
	    (void *)&sc->sc_dwc2, sc->sc_dwc2.sc_bus.bdev.dv_xname);
	KASSERT(sc->sc_ih != NULL);

	printf("\n");
}

int
octdwctwo_set_dma_addr(void *data, dma_addr_t dma_addr, int ch)
{
	struct octdwctwo_softc *sc = data;

	octdwctwo_reg2_wr(sc,
	    USBN_DMA0_INB_CHN0_OFFSET + ch * 0x8, dma_addr);
	octdwctwo_reg2_wr(sc,
	    USBN_DMA0_OUTB_CHN0_OFFSET + ch * 0x8, dma_addr);
	return 0;
}

u_int64_t
octdwctwo_reg2_rd(struct octdwctwo_softc *sc, bus_size_t offset)
{
	u_int64_t value;

	value = bus_space_read_8(sc->sc_bust, sc->sc_regh2, offset);
	return value;
}

void
octdwctwo_reg2_wr(struct octdwctwo_softc *sc, bus_size_t offset, u_int64_t value)
{
	bus_space_write_8(sc->sc_bust, sc->sc_regh2, offset, value);
	/* guarantee completion of the store operation on RSL registers*/
	bus_space_read_8(sc->sc_bust, sc->sc_regh2, offset);
}

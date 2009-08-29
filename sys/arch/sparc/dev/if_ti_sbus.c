/*	$OpenBSD: if_ti_sbus.c,v 1.1 2009/08/29 21:30:48 kettenis Exp $	*/
/*
 * Copyright (c) 2009 Mark Kettenis
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
#include <sys/device.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <machine/autoconf.h>
#include <machine/bus.h>
extern struct sparc_bus_dma_tag *iommu_dmatag;

#include <dev/pci/pcireg.h>

#include <dev/ic/tireg.h>
#include <dev/ic/tivar.h>

struct ti_sbus_softc {
	struct ti_softc		tsc_sc;
	struct intrhand		tsc_ih;
	struct rom_reg		tsc_rr;
};

int	ti_sbus_match(struct device *, void *, void *);
void	ti_sbus_attach(struct device *, struct device *, void *);

struct cfattach ti_sbus_ca = {
	sizeof(struct ti_sbus_softc), ti_sbus_match, ti_sbus_attach
};

int
ti_sbus_match(struct device *parent, void *match, void *aux)
{
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	return (strcmp("SUNW,vge", ra->ra_name) == 0);
}

void
ti_sbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct confargs *ca = aux;
	struct ti_sbus_softc *tsc = (void *)self;
	struct ti_softc *sc = &tsc->tsc_sc;
	bus_space_handle_t ioh;

	/* Pass on the bus tags */
	tsc->tsc_rr = ca->ca_ra.ra_reg[1];
	sc->ti_btag = &tsc->tsc_rr;
	sc->sc_dmatag = iommu_dmatag;

	if (ca->ca_ra.ra_nintr < 1) {
                printf(": no interrupt\n");
                return;
        }

	if (ca->ca_ra.ra_nreg < 2) {
                printf(": only %d register sets\n", ca->ca_ra.ra_nreg);
		return;
	}

	if (bus_space_map(&ca->ca_ra.ra_reg[1], 0,
	    ca->ca_ra.ra_reg[1].rr_len, 0, &sc->ti_bhandle)) {
		printf(": can't map registers\n");
		return;
	}

	if (bus_space_map(&ca->ca_ra.ra_reg[0], 0,
	    ca->ca_ra.ra_reg[0].rr_len, 0, &ioh)) {
		printf(": can't map registers\n");
		goto fail;
	}

	tsc->tsc_ih.ih_fun = ti_intr;
	tsc->tsc_ih.ih_arg = sc;
	intr_establish(ca->ca_ra.ra_intr[0].int_pri, &tsc->tsc_ih,
	    IPL_NET, self->dv_xname);

	bus_space_write_4(sc->ti_btag, ioh, TI_PCI_CMDSTAT, 0x02000006);
	bus_space_write_4(sc->ti_btag, ioh, TI_PCI_BIST, 0xffffffff);
	bus_space_write_4(sc->ti_btag, ioh, TI_PCI_LOMEM, 0x00000400);

	bus_space_unmap(&ca->ca_ra.ra_reg[0], ioh, ca->ca_ra.ra_reg[0].rr_len);

	sc->ti_sbus = 1;
	if (ti_attach(sc) == 0)
		return;

fail:
	bus_space_unmap(&ca->ca_ra.ra_reg[1], sc->ti_bhandle,
	    ca->ca_ra.ra_reg[1].rr_len);
}

/*
 * Copyright (c) 2013 Sylvestre Gallon <ccna.syl@gmail.com>
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
#include <sys/types.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <armv7/omap/omapvar.h>
#include <armv7/omap/prcmvar.h>
#include <armv7/omap/edmareg.h>
#include <armv7/omap/edmavar.h>

#define DEVNAME(s)		((s)->sc_dev.dv_xname)

struct edma_softc *edma_sc;

void	edma_attach(struct device *, struct device *, void *);
int	edma_comp_intr(void *);

struct cfattach edma_ca = {
	sizeof(struct edma_softc), NULL, edma_attach
};

struct cfdriver edma_cd = {
	NULL, "edma", DV_DULL
};

void
edma_attach(struct device *parent, struct device *self, void *aux)
{
	struct omap_attach_args *oaa = aux;
	struct edma_softc *sc = (struct edma_softc *)self;
	uint32_t rev;
	int i;

	sc->sc_iot = oaa->oa_iot;

	/* Map Base address for TPCC and TPCTX */
	if (bus_space_map(sc->sc_iot, oaa->oa_dev->mem[0].addr,
	    oaa->oa_dev->mem[0].size, 0, &sc->sc_tpcc)) {
		printf("%s: bus_space_map failed for TPCC\n", DEVNAME(sc));
		return ;
	}

	/* Enable TPCC and TPTC0 in PRCM */
	prcm_enablemodule(PRCM_TPCC);
	prcm_enablemodule(PRCM_TPTC0);

	rev = TPCC_READ_4(sc, EDMA_TPCC_PID);
	printf(" rev %d.%d\n", rev >> 4 & 0xf, rev & 0xf);

	/* XXX IPL_VM ? */
	/* Enable interrupts line */
	sc->sc_ih_comp = arm_intr_establish(oaa->oa_dev->irq[0], IPL_VM,
	    edma_comp_intr, sc, DEVNAME(sc));
	if (sc->sc_ih_comp == NULL) {
		printf("%s: unable to establish interrupt comp\n", DEVNAME(sc));
		bus_space_unmap(sc->sc_iot, sc->sc_tpcc,
		    oaa->oa_dev->mem[0].size);
		return ;
	}

	/* Set global softc */
	edma_sc = sc;

	/* Clear Event Missed Events */
	TPCC_WRITE_4(sc, EDMA_TPCC_EMCR, 0xffffffff);
	TPCC_WRITE_4(sc, EDMA_TPCC_EMCRH, 0xffffffff);
	TPCC_WRITE_4(sc, EDMA_TPCC_CCERRCLR, 0xffffffff);

	/* Identity Map Channels PaRAM */
	for (i = 0; i < EDMA_NUM_DMA_CHANS; i++)
		TPCC_WRITE_4(sc, EDMA_TPCC_DHCM(i), i << 5);

	/*
	 * Enable SHADOW Region 0 and only use this region
	 * This is needed to have working intr...
	 */
	TPCC_WRITE_4(sc, EDMA_TPCC_DRAE0, 0xffffffff);
	TPCC_WRITE_4(sc, EDMA_TPCC_DRAEH0, 0xffffffff);

	return ;
}

int
edma_comp_intr(void *arg)
{
	struct edma_softc *sc = arg;
	uint32_t ipr, iprh;
	int i;

	ipr = TPCC_READ_4(sc, EDMA_TPCC_IPR);
	iprh = TPCC_READ_4(sc, EDMA_TPCC_IPRH);

	/* Lookup to intr in the first 32 chans */
	for (i = 0; i < (EDMA_NUM_DMA_CHANS/2); i++) {
		if (ISSET(ipr, (1<<i))) {
			TPCC_WRITE_4(sc, EDMA_TPCC_ICR, (1<<i));
			if (sc->sc_intr_cb[i])
				sc->sc_intr_cb[i](sc->sc_intr_dat[i]);
		}
	}

	for (i = 0; i < (EDMA_NUM_DMA_CHANS/2); i++) {
		if (ISSET(iprh, (1<<i))) {
			TPCC_WRITE_4(sc, EDMA_TPCC_ICRH, (1<<i));
			if (sc->sc_intr_cb[i + 32])
				sc->sc_intr_cb[i + 32](sc->sc_intr_dat[i + 32]);
		}
	}

	/* Trig pending intr */
	TPCC_WRITE_4(sc, EDMA_TPCC_IEVAL, 1);

	return (1);
}

int
edma_intr_dma_en(uint32_t ch, edma_intr_cb_t cb, void *dat)
{
	if (edma_sc == NULL || ch >= EDMA_NUM_DMA_CHANS)
		return (EINVAL);

	edma_sc->sc_intr_cb[ch] = cb;
	edma_sc->sc_intr_dat[ch] = dat;

	if (ch < 32) {
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_IESR, 1 << ch);
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_IESR + EDMA_REG_X(0), 1 << ch);
	} else {
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_IESRH, 1 << (ch - 32));
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_IESRH + EDMA_REG_X(0),
		    1 << (ch - 32));
	}

	return (0);
}

int
edma_intr_dma_dis(uint32_t ch)
{
	if (edma_sc == NULL || ch >= EDMA_NUM_DMA_CHANS)
		return (EINVAL);

	if (ch < 32)
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_IECR, 1 << ch);
	else
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_IECRH, 1 << (ch - 32));
	edma_sc->sc_intr_cb[ch] = NULL;
	edma_sc->sc_intr_dat[ch] = NULL;

	return (0);
}

int
edma_trig_xfer_man(uint32_t ch)
{
	if (edma_sc == NULL || ch >= EDMA_NUM_DMA_CHANS)
		return (EINVAL);

	/*
	 * Trig xfer
	 * enable IEVAL only if there is an intr associated
	 */
	if (ch < 32) {
		if (ISSET(TPCC_READ_4(edma_sc, EDMA_TPCC_IER), 1 << ch))
			TPCC_WRITE_4(edma_sc, EDMA_TPCC_IEVAL, 1);
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_ICR, 1 << ch);
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_EMCR, 1 << ch);
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_ESR, 1 << ch);
	} else {
		if (ISSET(TPCC_READ_4(edma_sc, EDMA_TPCC_IERH), 1 << (ch - 32)))
			TPCC_WRITE_4(edma_sc, EDMA_TPCC_IEVAL, 1);
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_ICRH, 1 << (ch - 32));
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_EMCRH, 1 << (ch - 32));
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_ESRH, 1 << (ch - 32));
	}

	return (0);
}

int
edma_trig_xfer_by_dev(uint32_t ch)
{
	if (edma_sc == NULL || ch >= EDMA_NUM_DMA_CHANS)
		return (EINVAL);

	if (ch < 32) {
		if (ISSET(TPCC_READ_4(edma_sc, EDMA_TPCC_IER), 1 << ch))
			TPCC_WRITE_4(edma_sc, EDMA_TPCC_IEVAL, 1);
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_ICR, 1 << ch);
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_SECR, 1 << ch);
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_EMCR, 1 << ch);
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_EESR, 1 << ch);
	} else {
		if (ISSET(TPCC_READ_4(edma_sc, EDMA_TPCC_IERH), 1 << (ch - 32)))
			TPCC_WRITE_4(edma_sc, EDMA_TPCC_IEVAL, 1);
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_ICRH, 1 << (ch - 32));
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_SECRH, 1 << (ch - 32));
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_EMCRH, 1 << (ch - 32));
		TPCC_WRITE_4(edma_sc, EDMA_TPCC_EESRH, 1 << (ch - 32));
	}
	return (0);
}

void
edma_param_write(uint32_t ch, struct edma_param *params)
{
	bus_space_write_region_4(edma_sc->sc_iot, edma_sc->sc_tpcc,
	    EDMA_TPCC_OPT(ch), (uint32_t *)params, 8);
}

void
edma_param_read(uint32_t ch, struct edma_param *params)
{
	bus_space_read_region_4(edma_sc->sc_iot, edma_sc->sc_tpcc,
	    EDMA_TPCC_OPT(ch), (uint32_t *)params, 8);
}


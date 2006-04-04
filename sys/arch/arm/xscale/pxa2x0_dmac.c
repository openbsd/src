/*	$OpenBSD: pxa2x0_dmac.c,v 1.3 2006/04/04 11:37:05 pascoe Exp $	*/

/*
 * Copyright (c) 2005 Christopher Pascoe <pascoe@openbsd.org>
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

/*
 * DMA Controller Handler for the Intel PXA2X0 processor.
 */
#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/evcount.h>
#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/lock.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_dmac.h>

typedef void (*pxadmac_intrhandler)(void *);

struct pxadmac_softc {
	struct device sc_dev;
	bus_space_tag_t sc_bust;
	bus_space_handle_t sc_bush;
	void *sc_ih;
	int sc_nchan;
	int sc_npri;
	pxadmac_intrhandler sc_intrhandlers[DMAC_N_CHANNELS_PXA27X];
	void *sc_intrargs[DMAC_N_CHANNELS_PXA27X];
};

int pxadmac_intr(void *);

/*
 * DMAC autoconf glue
 */
int	pxadmac_match(struct device *, void *, void *);
void	pxadmac_attach(struct device *, struct device *, void *);

struct cfattach pxadmac_ca = {
	sizeof(struct pxadmac_softc), pxadmac_match, pxadmac_attach
};

struct cfdriver pxadmac_cd = {
	NULL, "pxadmac", DV_DULL
};

static struct pxadmac_softc *pxadmac_softc = NULL;

int
pxadmac_match(struct device *parent, void *cf, void *aux)
{
	struct pxaip_attach_args *pxa = aux;

	if (pxadmac_softc != NULL || pxa->pxa_addr != PXA2X0_DMAC_BASE)
		return (0);

	return (1);
}

void
pxadmac_attach(struct device *parent, struct device *self, void *args)
{
	struct pxadmac_softc *sc = (struct pxadmac_softc *)self;
	struct pxaip_attach_args *pxa = args;
	bus_size_t bus_size;

	sc->sc_bust = pxa->pxa_iot;

	printf(": DMA Controller\n");

	if ((cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA27X) {
		sc->sc_nchan = DMAC_N_CHANNELS_PXA27X;
		sc->sc_npri = DMAC_N_PRIORITIES_PXA27X;
		bus_size = PXA27X_DMAC_SIZE;
	} else  {
		sc->sc_nchan = DMAC_N_CHANNELS;
		sc->sc_npri = DMAC_N_PRIORITIES;
		bus_size = PXA2X0_DMAC_SIZE;
	}

	if (bus_space_map(sc->sc_bust, pxa->pxa_addr, bus_size, 0,
	    &sc->sc_bush)) {
		printf("%s: Can't map registers!\n", sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_ih = pxa2x0_intr_establish(pxa->pxa_intr, IPL_BIO,
	    pxadmac_intr, sc, "pxadmac");
	if (sc->sc_ih == NULL) {
		printf(": unable to establish interrupt\n");
		bus_space_unmap(sc->sc_bust, sc->sc_bush, bus_size);
		return;
	}

	pxadmac_softc = sc;
}

/* Perform non-descriptor based DMA to a FIFO */
int
pxa2x0_dma_to_fifo(int periph, int chan, bus_addr_t fifo_addr, int width,
    int burstsize, bus_addr_t src_addr, int length, void (*intr)(void *),
    void *intrarg)
{
	struct pxadmac_softc *sc = pxadmac_softc;
	uint32_t cmd;

	if (periph < 0 || periph > 63 || periph == 23) {
		printf("pxa2x0_dma_to_fifo: bogus peripheral %d", periph);
		return EINVAL;
	}

	if (chan < 0 || chan >= sc->sc_nchan) {
		printf("pxa2x0_dma_to_fifo: bogus dma channel %d", chan);
		return EINVAL;
	}

	if (length < 0 || length > DCMD_LENGTH_MASK) {
		printf("pxa2x0_dma_to_fifo: bogus length %d", length);
		return EINVAL;
	}

	cmd = (length & DCMD_LENGTH_MASK) | DCMD_INCSRCADDR | DCMD_FLOWTRG
	    | DCMD_ENDIRQEN;

	switch (width) {
	case 1:
		cmd |= DCMD_WIDTH_1;
		break;
	case 4:
		cmd |= DCMD_WIDTH_4;
		break;
	default:
		printf("pxa2x0_dma_to_fifo: bogus width %d", width);
		return EINVAL;
	}

	switch (burstsize) {
	case 8:
		cmd |= DCMD_SIZE_8;
		break;
	case 16:
		cmd |= DCMD_SIZE_16;
		break;
	case 32:
		cmd |= DCMD_SIZE_32;
		break;
	default:
		printf("pxa2x0_dma_to_fifo: bogus burstsize %d", burstsize);
		return EINVAL;
	}

	/* XXX: abort anything already in progress, hopefully nothing. */
	bus_space_write_4(sc->sc_bust, sc->sc_bush, DMAC_DCSR(chan),
	    DCSR_NODESCFETCH);

	/* Save handler for interrupt-on-completion. */
	sc->sc_intrhandlers[chan] = intr;
	sc->sc_intrargs[chan] = intrarg;

	/* Map peripheral to channel for flow control setup. */
	bus_space_write_4(sc->sc_bust, sc->sc_bush, DMAC_DRCMR(periph),
	    chan | DRCMR_MAPVLD);

	/* Setup transfer addresses. */
	bus_space_write_4(sc->sc_bust, sc->sc_bush, DMAC_DDADR(chan),
	    DDADR_STOP);
	bus_space_write_4(sc->sc_bust, sc->sc_bush, DMAC_DSADR(chan),
	    src_addr);
	bus_space_write_4(sc->sc_bust, sc->sc_bush, DMAC_DTADR(chan),
	    fifo_addr);
	bus_space_write_4(sc->sc_bust, sc->sc_bush, DMAC_DCMD(chan),
	    cmd);

	/* Start the transfer. */
	bus_space_write_4(sc->sc_bust, sc->sc_bush, DMAC_DCSR(chan),
	    DCSR_RUN | DCSR_NODESCFETCH);

	return 0;
}

/* Perform non-descriptor based DMA from a FIFO */
int
pxa2x0_dma_from_fifo(int periph, int chan, bus_addr_t fifo_addr, int width,
    int burstsize, bus_addr_t trg_addr, int length, void (*intr)(void *),
    void *intrarg)
{
	struct pxadmac_softc *sc = pxadmac_softc;
	uint32_t cmd;

	if (periph < 0 || periph > 63 || periph == 23) {
		printf("pxa2x0_dma_from_fifo: bogus peripheral %d", periph);
		return EINVAL;
	}

	if (chan < 0 || chan >= sc->sc_nchan) {
		printf("pxa2x0_dma_from_fifo: bogus dma channel %d", chan);
		return EINVAL;
	}

	if (length < 0 || length > DCMD_LENGTH_MASK) {
		printf("pxa2x0_dma_from_fifo: bogus length %d", length);
		return EINVAL;
	}

	cmd = (length & DCMD_LENGTH_MASK) | DCMD_INCTRGADDR | DCMD_FLOWSRC
	    | DCMD_ENDIRQEN;

	switch (width) {
	case 1:
		cmd |= DCMD_WIDTH_1;
		break;
	case 4:
		cmd |= DCMD_WIDTH_4;
		break;
	default:
		printf("pxa2x0_dma_from_fifo: bogus width %d", width);
		return EINVAL;
	}

	switch (burstsize) {
	case 8:
		cmd |= DCMD_SIZE_8;
		break;
	case 16:
		cmd |= DCMD_SIZE_16;
		break;
	case 32:
		cmd |= DCMD_SIZE_32;
		break;
	default:
		printf("pxa2x0_dma_from_fifo: bogus burstsize %d", burstsize);
		return EINVAL;
	}

	/* XXX: abort anything already in progress, hopefully nothing. */
	bus_space_write_4(sc->sc_bust, sc->sc_bush, DMAC_DCSR(chan),
	    DCSR_NODESCFETCH);

	/* Save handler for interrupt-on-completion. */
	sc->sc_intrhandlers[chan] = intr;
	sc->sc_intrargs[chan] = intrarg;

	/* Map peripheral to channel for flow control setup. */
	bus_space_write_4(sc->sc_bust, sc->sc_bush, DMAC_DRCMR(periph),
	    chan | DRCMR_MAPVLD);

	/* Setup transfer addresses. */
	bus_space_write_4(sc->sc_bust, sc->sc_bush, DMAC_DDADR(chan),
	    DDADR_STOP);
	bus_space_write_4(sc->sc_bust, sc->sc_bush, DMAC_DSADR(chan),
	    fifo_addr);
	bus_space_write_4(sc->sc_bust, sc->sc_bush, DMAC_DTADR(chan),
	    trg_addr);
	bus_space_write_4(sc->sc_bust, sc->sc_bush, DMAC_DCMD(chan),
	    cmd);

	/* Start the transfer. */
	bus_space_write_4(sc->sc_bust, sc->sc_bush, DMAC_DCSR(chan),
	    DCSR_RUN | DCSR_NODESCFETCH);

	return 0;
}

int
pxadmac_intr(void *v)
{
	struct pxadmac_softc *sc = v;
	u_int32_t dint, dcsr;
	int chan;

	/* Interrupt for us? */
	dint = bus_space_read_4(sc->sc_bust, sc->sc_bush, DMAC_DINT);
	if (!dint)
		return 0;

	/* Process individual channels and run handlers. */
	/* XXX: this does not respect priority order for channels. */
	for (chan = 0; dint != 0 && chan < 32; chan++) {
		/* Don't ack channels that weren't ready at call time. */
		if ((dint & (1 << chan)) == 0)
			continue;
		dint &= ~(1 << chan);

		/* Acknowledge individual channel interrupt. */
		dcsr = bus_space_read_4(sc->sc_bust, sc->sc_bush,
		    DMAC_DCSR(chan));
		bus_space_write_4(sc->sc_bust, sc->sc_bush, DMAC_DCSR(chan),
		    dcsr & 0x7C80021F);

		/* Call the registered handler. */
		if (sc->sc_intrhandlers[chan])
			sc->sc_intrhandlers[chan](sc->sc_intrargs[chan]);
	}

	return 1;
}

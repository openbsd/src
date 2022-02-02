/*	$OpenBSD: apldma.c,v 1.1 2022/02/02 22:55:57 kettenis Exp $	*/
/*
 * Copyright (c) 2022 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_power.h>
#include <dev/ofw/fdt.h>

#include <dev/audio_if.h>

#include <arm64/dev/apldma.h>

/*
 * This driver is based on preliminary device tree bindings and will
 * almost certainly need changes once the official bindings land in
 * mainline Linux.  Support for these preliminary bindings will be
 * dropped as soon as official bindings are available.
 */

#define DMA_TX_EN			0x0000
#define DMA_TX_EN_CLR			0x0004
#define DMA_TX_INTR			0x0034

#define DMA_TX_CTL(chan)		(0x8000 + (chan) * 0x400)
#define  DMA_TX_CTL_RESET_RINGS		(1 << 0)
#define DMA_TX_INTRSTAT(chan)		(0x8014 + (chan) * 0x400)
#define  DMA_TX_INTRSTAT_DESC_DONE	(1 << 0)
#define  DMA_TX_INTRSTAT_ERR		(1 << 6)
#define DMA_TX_INTRMASK(chan)		(0x8024 + (chan) * 0x400)
#define  DMA_TX_INTRMASK_DESC_DONE	(1 << 0)
#define  DMA_TX_INTRMASK_ERR		(1 << 6)
#define DMA_TX_BUS_WIDTH(chan)		(0x8040 + (chan) * 0x400)
#define  DMA_TX_BUS_WIDTH_8BIT		(0 << 0)
#define  DMA_TX_BUS_WIDTH_16BIT		(1 << 0)
#define  DMA_TX_BUS_WIDTH_32BIT		(2 << 0)
#define  DMA_TX_BUS_WIDTH_FRAME_2_WORDS	(1 << 4)
#define  DMA_TX_BUS_WIDTH_FRAME_4_WORDS	(2 << 4)
#define DMA_TX_BURST_SIZE(chan)		(0x8054 + (chan) * 0x400)
#define DMA_TX_RESIDUE(chan)		(0x8064 + (chan) * 0x400)
#define DMA_TX_DESC_RING(chan)		(0x8070 + (chan) * 0x400)
#define  DMA_TX_DESC_RING_FULL		(1 << 9)
#define DMA_TX_REPORT_RING(chan)	(0x8074 + (chan) * 0x400)
#define  DMA_TX_REPORT_RING_EMPTY	(1 << 8)
#define DMA_TX_DESC_WRITE(chan)		(0x10000 + (chan) * 4)
#define DMA_TX_REPORT_READ(chan)	(0x10100 + (chan) * 4)

#define DMA_DESC_NOTIFY			(1 << 16)
#define DMA_NUM_DESCRIPTORS		4
#define DMA_NUM_CHANNELS		4

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct apldma_channel {
	struct apldma_softc	*ac_sc;
	unsigned int		ac_chan;

	bus_dmamap_t		ac_map;
	bus_dma_segment_t	ac_seg;
	caddr_t			ac_kva;
	bus_size_t		ac_size;

	bus_addr_t		ac_base;
	bus_size_t		ac_len;
	bus_size_t		ac_pos;
	bus_size_t		ac_blksize;

	void			(*ac_intr)(void *);
	void			*ac_intrarg;
};

struct apldma_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	bus_dma_tag_t		sc_dmat;
	int			sc_node;
	void			*sc_ih;

	struct apldma_channel	*sc_ac[DMA_NUM_CHANNELS];
};

struct apldma_softc *apldma_sc;

int	apldma_match(struct device *, void *, void *);
void	apldma_attach(struct device *, struct device *, void *);

const struct cfattach apldma_ca = {
	sizeof (struct apldma_softc), apldma_match, apldma_attach
};

struct cfdriver apldma_cd = {
	NULL, "apldma", DV_DULL
};

int	apldma_intr(void *);

int
apldma_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "apple,admac");
}

void
apldma_attach(struct device *parent, struct device *self, void *aux)
{
	struct apldma_softc *sc = (struct apldma_softc *)self;
	struct fdt_attach_args *faa = aux;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	sc->sc_dmat = faa->fa_dmat;
	sc->sc_node = faa->fa_node;

	power_domain_enable(sc->sc_node);

	sc->sc_ih = fdt_intr_establish(faa->fa_node, IPL_AUDIO | IPL_MPSAFE,
	    apldma_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt\n");
		goto unmap;
	}

	printf("\n");

	apldma_sc = sc;
	return;

unmap:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, faa->fa_reg[0].size);
}

void
apldma_fill_descriptors(struct apldma_channel *ac)
{
	struct apldma_softc *sc = ac->ac_sc;
	unsigned int i;

	for (i = 0; i < DMA_NUM_DESCRIPTORS; i++) {
		bus_addr_t addr = ac->ac_base + ac->ac_pos;
		uint32_t status;

		status = HREAD4(sc, DMA_TX_DESC_RING(ac->ac_chan));
		if (status & DMA_TX_DESC_RING_FULL)
			break;

		HWRITE4(sc, DMA_TX_DESC_WRITE(ac->ac_chan), addr);
		HWRITE4(sc, DMA_TX_DESC_WRITE(ac->ac_chan), addr >> 32);
		HWRITE4(sc, DMA_TX_DESC_WRITE(ac->ac_chan), ac->ac_blksize);
		HWRITE4(sc, DMA_TX_DESC_WRITE(ac->ac_chan), DMA_DESC_NOTIFY);

		ac->ac_pos += ac->ac_blksize;
		if (ac->ac_pos > ac->ac_len - ac->ac_blksize)
			ac->ac_pos = 0;
	}
}

int
apldma_intr(void *arg)
{
	struct apldma_softc *sc = arg;
	uint32_t intr, intrstat;
	unsigned int chan, i;

	intr = HREAD4(sc, DMA_TX_INTR);
	for (chan = 0; chan < DMA_NUM_CHANNELS; chan++) {
		if ((intr & (1 << chan)) == 0)
			continue;

		intrstat = HREAD4(sc, DMA_TX_INTRSTAT(chan));
		HWRITE4(sc, DMA_TX_INTRSTAT(chan), intrstat);

		if ((intrstat & DMA_TX_INTRSTAT_DESC_DONE) == 0)
			continue;
		
		for (i = 0; i < DMA_NUM_DESCRIPTORS; i++) {
			uint32_t status;

			status = HREAD4(sc, DMA_TX_REPORT_RING(chan));
			if (status & DMA_TX_REPORT_RING_EMPTY)
				break;

			/* Consume report descriptor. */
			HREAD4(sc, DMA_TX_REPORT_READ(chan));
			HREAD4(sc, DMA_TX_REPORT_READ(chan));
			HREAD4(sc, DMA_TX_REPORT_READ(chan));
			HREAD4(sc, DMA_TX_REPORT_READ(chan));
		}

		mtx_enter(&audio_lock);
		struct apldma_channel *ac = sc->sc_ac[chan];
		ac->ac_intr(ac->ac_intrarg);
		mtx_leave(&audio_lock);

		apldma_fill_descriptors(sc->sc_ac[chan]);
	}

	return 1;
}

struct apldma_channel *
apldma_alloc_channel(unsigned int chan)
{
	struct apldma_softc *sc = apldma_sc;
	struct apldma_channel *ac;

	if (chan >= DMA_NUM_CHANNELS)
		return NULL;

	ac = malloc(sizeof(*ac), M_DEVBUF, M_WAITOK);
	ac->ac_sc = sc;
	ac->ac_chan = chan;
	sc->sc_ac[chan] = ac;
	return ac;
}

void
apldma_free_channel(struct apldma_channel *ac)
{
	struct apldma_softc *sc = ac->ac_sc;

	sc->sc_ac[ac->ac_chan] = NULL;
	free(ac, M_DEVBUF, sizeof(*ac));
}

void *
apldma_allocm(struct apldma_channel *ac, size_t size, int flags)
{
	struct apldma_softc *sc = ac->ac_sc;
	int nsegs;
	int err;

	flags = (flags & M_WAITOK) ? BUS_DMA_WAITOK : BUS_DMA_NOWAIT;

	err = bus_dmamem_alloc(sc->sc_dmat, size, 0, 0, &ac->ac_seg, 1,
	    &nsegs, flags);
	if (err)
		return NULL;
	err = bus_dmamem_map(sc->sc_dmat, &ac->ac_seg, 1, size, &ac->ac_kva,
	    flags | BUS_DMA_COHERENT);
	if (err)
		goto free;
	err = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0, flags,
	    &ac->ac_map);
	if (err)
		goto unmap;
	err = bus_dmamap_load(sc->sc_dmat, ac->ac_map, ac->ac_kva, size,
	    NULL, flags);
	if (err)
		goto destroy;

	ac->ac_size = size;
	return ac->ac_kva;
	
destroy:
	bus_dmamap_destroy(sc->sc_dmat, ac->ac_map);
unmap:
	bus_dmamem_unmap(sc->sc_dmat, ac->ac_kva, size);
free:
	bus_dmamem_free(sc->sc_dmat, &ac->ac_seg, 1);
	return NULL;
}

void
apldma_freem(struct apldma_channel *ac)
{
	struct apldma_softc *sc = ac->ac_sc;

	bus_dmamap_unload(sc->sc_dmat, ac->ac_map);
	bus_dmamap_destroy(sc->sc_dmat, ac->ac_map);
	bus_dmamem_unmap(sc->sc_dmat, ac->ac_kva, ac->ac_size);
	bus_dmamem_free(sc->sc_dmat, &ac->ac_seg, 1);
}

int
apldma_trigger_output(struct apldma_channel *ac, void *start, void *end,
    int blksize, void (*intr)(void *), void *intrarg,
    struct audio_params *params)
{
	struct apldma_softc *sc = ac->ac_sc;

	KASSERT(start == ac->ac_kva);

	ac->ac_base = ac->ac_map->dm_segs[0].ds_addr;
	ac->ac_len = end - start;
	ac->ac_pos = 0;
	ac->ac_blksize = blksize;

	ac->ac_intr = intr;
	ac->ac_intrarg = intrarg;

	switch (params->bps) {
	case 2:
		HWRITE4(sc, DMA_TX_BUS_WIDTH(ac->ac_chan),
		    DMA_TX_BUS_WIDTH_16BIT | DMA_TX_BUS_WIDTH_FRAME_4_WORDS);
		break;
	case 4:
		HWRITE4(sc, DMA_TX_BUS_WIDTH(ac->ac_chan),
		    DMA_TX_BUS_WIDTH_32BIT | DMA_TX_BUS_WIDTH_FRAME_4_WORDS);
		break;
	default:
		return EINVAL;
	}

	/* Reset rings. */
	HWRITE4(sc, DMA_TX_CTL(ac->ac_chan), DMA_TX_CTL_RESET_RINGS);
	HWRITE4(sc, DMA_TX_CTL(ac->ac_chan), 0);

	/* Clear and unmask interrupts. */
	HWRITE4(sc, DMA_TX_INTRSTAT(ac->ac_chan),
	   DMA_TX_INTRSTAT_DESC_DONE | DMA_TX_INTRSTAT_ERR);
	HWRITE4(sc, DMA_TX_INTRMASK(ac->ac_chan),
	   DMA_TX_INTRMASK_DESC_DONE | DMA_TX_INTRMASK_ERR);

	apldma_fill_descriptors(ac);

	/* Start DMA transfer. */
	HWRITE4(sc, DMA_TX_EN, 1 << ac->ac_chan);

	return 0;
}

int
apldma_halt_output(struct apldma_channel *ac)
{
	struct apldma_softc *sc = ac->ac_sc;

	/* Stop DMA transfer. */
	HWRITE4(sc, DMA_TX_EN_CLR, 1 << ac->ac_chan);

	/* Mask all interrupts. */
	HWRITE4(sc, DMA_TX_INTRMASK(ac->ac_chan), 0);

	return 0;
}

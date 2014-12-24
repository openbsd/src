/*	$OpenBSD: dma.c,v 1.3 2014/12/24 22:48:27 miod Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
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
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <uvm/uvm.h>

#include <machine/autoconf.h>
#include <machine/board.h>

#include <aviion/dev/sysconvar.h>

#include <aviion/dev/dmareg.h>
#include <aviion/dev/dmavar.h>

/*
 * DMA request information structure.
 */
struct dmareq {
	TAILQ_ENTRY(dmareq)	chain;

	vaddr_t	mem;
	size_t	len;
	uint	dir;

	size_t	lastcnt;

	void	*cbarg;
	void	(*cbstart)(void *);
	void	(*cbdone)(void *);
};

struct dma_softc {
	struct device	sc_dev;

	struct intrhand	sc_ih;

	TAILQ_HEAD(, dmareq) sc_req;
};

int	dma_match(struct device *, void *, void *);
void	dma_attach(struct device *, struct device *, void *);

const struct cfattach dma_ca = {
	sizeof(struct dma_softc), dma_match, dma_attach
};

struct cfdriver dma_cd = {
	NULL, "dma", DV_DULL
};

int	dma_intr(void *);
void	dma_start(struct dma_softc *);

int
dma_match(struct device *parent, void *match, void *aux)
{
	struct confargs *ca = aux;

	switch (cpuid) {
#ifdef AV400
	case AVIION_300_310:
	case AVIION_400_4000:
	case AVIION_410_4100:
	case AVIION_300C_310C:
	case AVIION_300CD_310CD:
	case AVIION_300D_310D:
	case AVIION_4300_25:
	case AVIION_4300_20:
	case AVIION_4300_16:
		if (ca->ca_paddr == DMA_MAP)
			return 1;
		/* FALLTHROUGH */
#endif
	default:
		return 0;
	}
}

void
dma_attach(struct device *parent, struct device *self, void *aux)
{
	struct dma_softc *sc = (struct dma_softc *)self;

	printf("\n");

	*(volatile uint32_t *)DMA_COUNT = 0;	/* clear possibly pending int */

	TAILQ_INIT(&sc->sc_req);

	sc->sc_ih.ih_fn = dma_intr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_flags = 0;
	sc->sc_ih.ih_ipl = IPL_BIO;
	sysconintr_establish(INTSRC_DMA, &sc->sc_ih, self->dv_xname);
}

int
dma_intr(void *v)
{
	struct dma_softc *sc = v;
	struct dmareq *req;
	size_t tc;

	req = TAILQ_FIRST(&sc->sc_req);
	if (req == NULL) {
		printf("%s: interrupt but no active request?\n", __func__);
		*(volatile uint32_t *)DMA_STOP = DMASTOP_INHIBIT;
		return -1;
	}

	tc = *(volatile uint32_t *)DMA_COUNT;
	if (tc != 0)
		printf("%s: unexpected intr, TC = %x\n", __func__, tc);
	req->len -= req->lastcnt;
	if (req->len == 0) {
		/* this transfer is over */
		TAILQ_REMOVE(&sc->sc_req, req, chain);

		if (req->cbdone != NULL)
			(*req->cbdone)(req->cbarg);

		free(req, M_DEVBUF, sizeof *req);
	} else {
		req->mem += req->lastcnt;
	}
	dma_start(sc);

	return 1;
}

int
dma_req(void *vaddr, size_t sz, int dir, void (*cbstart)(void *),
    void (*cbdone)(void *), void *cbarg)
{
	struct dma_softc *sc;
	struct dmareq *req;
	int run;
	int s;

	if (dma_cd.cd_devs == NULL ||
	    (sc = (struct dma_softc *)dma_cd.cd_devs[0]) == NULL)
		return ENXIO;
	req = malloc(sizeof *req, M_DEVBUF, M_NOWAIT | M_CANFAIL);
	if (req == NULL)
		return ENOMEM;

	req->mem = (vaddr_t)vaddr;
	req->len = sz;
	req->dir = dir;
	req->cbstart = cbstart;
	req->cbdone = cbdone;
	req->cbarg = cbarg;
	req->lastcnt = 0;

	s = splbio();
	run = TAILQ_EMPTY(&sc->sc_req);
	TAILQ_INSERT_TAIL(&sc->sc_req, req, chain);
	if (run)
		dma_start(sc);
	splx(s);

	return 0;
}

void
dma_start(struct dma_softc *sc)
{
	struct dmareq *req;
	paddr_t pa, paoff;

	req = TAILQ_FIRST(&sc->sc_req);
	if (req == NULL) {
		*(volatile uint32_t *)DMA_STOP = DMASTOP_INHIBIT;
		return;
	}

	if (pmap_extract(pmap_kernel(), req->mem, &pa) == FALSE)
		panic("%s: pmap_extract(%p) failed",
		    __func__, (void *)req->mem);

	paoff = pa & ~DMAMAP_MASK;

	req->lastcnt = DMA_BOUNDARY - paoff;
	if (req->lastcnt > req->len)
		req->lastcnt = req->len;

	*(volatile uint32_t *)DMA_DIR = req->dir;
	*(volatile uint32_t *)DMA_COUNT = req->lastcnt;
	*(volatile uint32_t *)DMA_OFF = paoff;
	*(volatile uint32_t *)DMA_MAP = (pa & DMAMAP_MASK) |
	    DMAMAP_G | DMAMAP_V;
	*(volatile uint32_t *)DMA_CLR_INVALID = DMACLR;
	*(volatile uint32_t *)DMA_STOP = DMASTOP_RESTART;

	if (req->cbstart != NULL)
		(*req->cbstart)(req->cbarg);
}

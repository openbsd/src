/*	$OpenBSD: dmavar.h,v 1.7 2006/06/02 20:00:54 miod Exp $	*/
/*	$NetBSD: dmavar.h,v 1.11 1996/11/27 21:49:53 pk Exp $ */

/*
 * Copyright (c) 1994 Peter Galbavy.  All rights reserved.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

struct dma_softc {
	struct device sc_dev;			/* us as a device */
	struct esp_softc *sc_esp;		/* my scsi */
	struct le_softc *sc_le;			/* my ethernet */
	struct dma_regs *sc_regs;		/* the registers */
	int	sc_active;			/* DMA active ? */
	u_int	sc_rev;				/* revision */
	int	sc_burst;			/* DVMA burst size in effect */
	caddr_t	sc_dvmakaddr;			/* DVMA cookies */
	caddr_t	sc_dvmaaddr;			/*		*/
	size_t	sc_dmasize;
	caddr_t	*sc_dmaaddr;
	size_t  *sc_dmalen;
	void (*reset)(struct dma_softc *);	/* reset routine */
	void (*enintr)(struct dma_softc *);	/* enable interrupts */
	int (*isintr)(struct dma_softc *);	/* interrupt ? */
	int (*intr)(struct dma_softc *);	/* interrupt ! */
	int (*setup)(struct dma_softc *, caddr_t *, size_t *, int, size_t *);
	void (*go)(struct dma_softc *);
	u_int	sc_dmactl;
};

#define DMACSR(sc)	(sc->sc_regs->csr)
#define DMADDR(sc)	(sc->sc_regs->addr)
#define DMACNT(sc)	(sc->sc_regs->bcnt)

/* DMA engine functions */
#define DMA_ENINTR(r)		(((r)->enintr)(r))
#define DMA_ISINTR(r)		(((r)->isintr)(r))
#define DMA_RESET(r)		(((r)->reset)(r))
#define DMA_INTR(r)		(((r)->intr)(r))
#define DMA_ISACTIVE(r)		((r)->sc_active)
#define DMA_SETUP(a, b, c, d, e)	(((a)->setup)(a, b, c, d, e))
#define DMA_GO(r)		(((r)->go)(r))

void	dma_setuphandlers(struct dma_softc *);

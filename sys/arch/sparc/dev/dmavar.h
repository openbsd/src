/*	$NetBSD: dmavar.h,v 1.4 1994/11/27 00:08:34 deraadt Exp $ */

/*
 * Copyright (c) 1994 Peter Galbavy
 * Copyright (c) 1995 Theo de Raadt
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Peter Galbavy and
 *	Theo de Raadt.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
	struct	device sc_dev;			/* us as a device */
	struct	sbusdev sc_sd;			/* sbus device */
	struct	esp_softc *sc_esp;		/* my scsi */
	struct	dma_regs *sc_regs;		/* the registers */
	int	sc_active;			/* DMA active ? */
	int	sc_rev;				/* revision */
	int	sc_node;			/* PROM node ID */

	size_t	sc_segsize;			/* current operation */
	void	**sc_dmaaddr;
	size_t  *sc_dmalen;
	char	sc_dmapolling;			/* ... is polled */
	char	sc_dmadev2mem;			/* transfer direction */
};

void	dmareset	__P((struct dma_softc *sc));
void	dmastart	__P((struct dma_softc *sc, void *addr,
			    size_t *len, int datain, int poll));
int	dmaintr		__P((struct dma_softc *sc, int restart));
int	dmapending	__P((struct dma_softc *sc));
void	dmadrain	__P((struct dma_softc *sc));
void	dmaenintr	__P((struct dma_softc *sc));
int	dmadisintr	__P((struct dma_softc *sc));

#define DMACSR(sc)	(sc->sc_regs->csr)
#define DMADDR(sc)	(sc->sc_regs->addr)
#define DMABCNT(sc)	(sc->sc_regs->bcnt)

#define TIME_WAIT(cond, msg, sc) { \
	int count = 500000; \
	while (--count > 0 && (cond)) \
		DELAY(1); \
	if (count == 0) { \
		printf("CSR = %x\n", (sc)->sc_regs->csr); \
		panic(msg); \
	} \
}

#define DMAWAIT_PEND(sc) \
	TIME_WAIT((DMACSR(sc) & D_R_PEND), \
	    "DMAWAIT_PEND", sc)

/* keep punching the chip until it's flushed */
#define DMAWAIT_DRAIN(sc) \
	TIME_WAIT((DMACSR(sc) |= D_DRAIN, DMACSR(sc) & D_DRAINING), \
	    "DMAWAIT_DRAIN", sc)

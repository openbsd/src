/*	$NetBSD: dma.c,v 1.7 1996/02/14 02:44:17 thorpej Exp $	*/

/*
 * Copyright (c) 1995 Jason R. Thorpe.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)dma.c	8.1 (Berkeley) 6/10/93
 */

/*
 * DMA driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>

#include <machine/cpu.h>

#include <hp300/dev/device.h>
#include <hp300/dev/dmareg.h>
#include <hp300/dev/dmavar.h>

#include <hp300/hp300/isr.h>

extern void isrlink();
extern void _insque();
extern void _remque();
extern u_int kvtop();
extern void PCIA();

/*
 * The largest single request will be MAXPHYS bytes which will require
 * at most MAXPHYS/NBPG+1 chain elements to describe, i.e. if none of
 * the buffer pages are physically contiguous (MAXPHYS/NBPG) and the
 * buffer is not page aligned (+1).
 */
#define	DMAMAXIO	(MAXPHYS/NBPG+1)

struct	dma_chain {
	int	dc_count;
	char	*dc_addr;
};

struct	dma_channel {
	struct	dma_softc *dm_softc;		/* pointer back to softc */
	struct	dmadevice *dm_hwaddr;		/* registers if DMA_C */
	struct	dmaBdevice *dm_Bhwaddr;		/* registers if not DMA_C */
	char	dm_flags;			/* misc. flags */
	u_short	dm_cmd;				/* DMA controller command */
	struct	dma_chain *dm_cur;		/* current segment */
	struct	dma_chain *dm_last;		/* last segment */
	struct	dma_chain dm_chain[DMAMAXIO];	/* all segments */
};

struct	dma_softc {
	char	*sc_xname;			/* XXX external name */
	struct	dmareg *sc_dmareg;		/* pointer to our hardware */
	struct	dma_channel sc_chan[NDMACHAN];	/* 2 channels */
	char	sc_type;			/* A, B, or C */
} Dma_softc;

/* types */
#define	DMA_B	0
#define DMA_C	1

/* flags */
#define DMAF_PCFLUSH	0x01
#define DMAF_VCFLUSH	0x02
#define DMAF_NOINTR	0x04

struct	devqueue dmachan[NDMACHAN + 1];
int	dmaintr __P((void *));

#ifdef DEBUG
int	dmadebug = 0;
#define DDB_WORD	0x01	/* same as DMAGO_WORD */
#define DDB_LWORD	0x02	/* same as DMAGO_LWORD */
#define	DDB_FOLLOW	0x04
#define DDB_IO		0x08

void	dmatimeout __P((void *));
int	dmatimo[NDMACHAN];

long	dmahits[NDMACHAN];
long	dmamisses[NDMACHAN];
long	dmabyte[NDMACHAN];
long	dmaword[NDMACHAN];
long	dmalword[NDMACHAN];
#endif

void
dmainit()
{
	struct dma_softc *sc = &Dma_softc;
	struct dmareg *dma;
	struct dma_channel *dc;
	int i;
	char rev;

	/* There's just one. */
	sc->sc_dmareg = (struct dmareg *)DMA_BASE;
	dma = sc->sc_dmareg;
	sc->sc_xname = "dma0";

	/*
	 * Determine the DMA type.  A DMA_A or DMA_B will fail the
	 * following probe.
	 *
	 * XXX Don't know how to easily differentiate the A and B cards,
	 * so we just hope nobody has an A card (A cards will work if
	 * DMAINTLVL is set to 3).
	 */
	if (badbaddr((char *)&dma->dma_id[2])) {
		rev = 'B';
#if !defined(HP320)
		panic("dmainit: DMA card requires hp320 support");
#endif
	} else
		rev = dma->dma_id[2];

	sc->sc_type = (rev == 'B') ? DMA_B : DMA_C;

	for (i = 0; i < NDMACHAN; i++) {
		dc = &sc->sc_chan[i];
		dc->dm_softc = sc;
		switch (i) {
		case 0:
			dc->dm_hwaddr = &dma->dma_chan0;
			dc->dm_Bhwaddr = &dma->dma_Bchan0;
			break;

		case 1:
			dc->dm_hwaddr = &dma->dma_chan1;
			dc->dm_Bhwaddr = &dma->dma_Bchan1;
			break;

		default:
			panic("dmainit: more than 2 channels?");
			/* NOTREACHED */
		}
		dmachan[i].dq_forw = dmachan[i].dq_back = &dmachan[i];
	}
	dmachan[i].dq_forw = dmachan[i].dq_back = &dmachan[i];
#ifdef DEBUG
	/* make sure timeout is really not needed */
	timeout(dmatimeout, sc, 30 * hz);
#endif

	printf("%s: 98620%c, 2 channels, %d bit\n", sc->sc_xname,
	       rev, (rev == 'B') ? 16 : 32);

	/* Establish the interrupt handler */
	isrlink(dmaintr, sc, DMAINTLVL, ISRPRI_BIO);
}

int
dmareq(dq)
	register struct devqueue *dq;
{
	register int i;
	register int chan;
	register int s = splbio();

	chan = dq->dq_ctlr;
	i = NDMACHAN;
	while (--i >= 0) {
		if ((chan & (1 << i)) == 0)
			continue;
		if (dmachan[i].dq_forw != &dmachan[i])
			continue;
		insque(dq, &dmachan[i]);
		dq->dq_ctlr = i;
		splx(s);
		return(1);
	}
	insque(dq, dmachan[NDMACHAN].dq_back);
	splx(s);
	return(0);
}

void
dmafree(dq)
	register struct devqueue *dq;
{
	int unit = dq->dq_ctlr;
	struct dma_softc *sc = &Dma_softc;
	register struct dma_channel *dc = &sc->sc_chan[unit];
	register struct devqueue *dn;
	register int chan, s;

	s = splbio();
#ifdef DEBUG
	dmatimo[unit] = 0;
#endif
	DMA_CLEAR(dc);
#if defined(HP360) || defined(HP370) || defined(HP380)
	/*
	 * XXX we may not always go thru the flush code in dmastop()
	 */
	if (dc->dm_flags & DMAF_PCFLUSH) {
		PCIA();
		dc->dm_flags &= ~DMAF_PCFLUSH;
	}
#endif
#if defined(HP320) || defined(HP350)
	if (dc->dm_flags & DMAF_VCFLUSH) {
		/*
		 * 320/350s have VACs that may also need flushing.
		 * In our case we only flush the supervisor side
		 * because we know that if we are DMAing to user
		 * space, the physical pages will also be mapped
		 * in kernel space (via vmapbuf) and hence cache-
		 * inhibited by the pmap module due to the multiple
		 * mapping.
		 */
		DCIS();
		dc->dm_flags &= ~DMAF_VCFLUSH;
	}
#endif
	remque(dq);
	chan = 1 << unit;
	for (dn = dmachan[NDMACHAN].dq_forw;
	     dn != &dmachan[NDMACHAN]; dn = dn->dq_forw) {
		if (dn->dq_ctlr & chan) {
			remque((caddr_t)dn);
			insque((caddr_t)dn, (caddr_t)dq->dq_back);
			splx(s);
			dn->dq_ctlr = dq->dq_ctlr;
			(dn->dq_driver->d_start)(dn->dq_unit);
			return;
		}
	}
	splx(s);
}

void
dmago(unit, addr, count, flags)
	int unit;
	register char *addr;
	register int count;
	register int flags;
{
	struct dma_softc *sc = &Dma_softc;
	register struct dma_channel *dc = &sc->sc_chan[unit];
	register struct dma_chain *dcp;
	register char *dmaend = NULL;
	register int tcount;

	if (count > MAXPHYS)
		panic("dmago: count > MAXPHYS");
#if defined(HP320)
	if (sc->sc_type == DMA_B && (flags & DMAGO_LWORD))
		panic("dmago: no can do 32-bit DMA");
#endif
#ifdef DEBUG
	if (dmadebug & DDB_FOLLOW)
		printf("dmago(%d, %x, %x, %x)\n",
		       unit, addr, count, flags);
	if (flags & DMAGO_LWORD)
		dmalword[unit]++;
	else if (flags & DMAGO_WORD)
		dmaword[unit]++;
	else
		dmabyte[unit]++;
#endif
	/*
	 * Build the DMA chain
	 */
	for (dcp = dc->dm_chain; count > 0; dcp++) {
		dcp->dc_addr = (char *) kvtop(addr);
#if defined(HP380)
		/*
		 * Push back dirty cache lines
		 */
		if (mmutype == MMU_68040)
			DCFP(dcp->dc_addr);
#endif
		if (count < (tcount = NBPG - ((int)addr & PGOFSET)))
			tcount = count;
		dcp->dc_count = tcount;
		addr += tcount;
		count -= tcount;
		if (flags & DMAGO_LWORD)
			tcount >>= 2;
		else if (flags & DMAGO_WORD)
			tcount >>= 1;
		if (dcp->dc_addr == dmaend
#if defined(HP320)
		    /* only 16-bit count on 98620B */
		    && (sc->sc_type != DMA_B ||
			(dcp-1)->dc_count + tcount <= 65536)
#endif
		) {
#ifdef DEBUG
			dmahits[unit]++;
#endif
			dmaend += dcp->dc_count;
			(--dcp)->dc_count += tcount;
		} else {
#ifdef DEBUG
			dmamisses[unit]++;
#endif
			dmaend = dcp->dc_addr + dcp->dc_count;
			dcp->dc_count = tcount;
		}
	}
	dc->dm_cur = dc->dm_chain;
	dc->dm_last = --dcp;
	dc->dm_flags = 0;
	/*
	 * Set up the command word based on flags
	 */
	dc->dm_cmd = DMA_ENAB | DMA_IPL(DMAINTLVL) | DMA_START;
	if ((flags & DMAGO_READ) == 0)
		dc->dm_cmd |= DMA_WRT;
	if (flags & DMAGO_LWORD)
		dc->dm_cmd |= DMA_LWORD;
	else if (flags & DMAGO_WORD)
		dc->dm_cmd |= DMA_WORD;
	if (flags & DMAGO_PRI)
		dc->dm_cmd |= DMA_PRI;
#if defined(HP380)
	/*
	 * On the 68040 we need to flush (push) the data cache before a
	 * DMA (already done above) and flush again after DMA completes.
	 * In theory we should only need to flush prior to a write DMA
	 * and purge after a read DMA but if the entire page is not
	 * involved in the DMA we might purge some valid data.
	 */
	if (mmutype == MMU_68040 && (flags & DMAGO_READ))
		dc->dm_flags |= DMAF_PCFLUSH;
#endif
#if defined(HP360) || defined(HP370)
	/*
	 * Remember if we need to flush external physical cache when
	 * DMA is done.  We only do this if we are reading (writing memory).
	 */
	if (ectype == EC_PHYS && (flags & DMAGO_READ))
		dc->dm_flags |= DMAF_PCFLUSH;
#endif
#if defined(HP320) || defined(HP350)
	if (ectype == EC_VIRT && (flags & DMAGO_READ))
		dc->dm_flags |= DMAF_VCFLUSH;
#endif
	/*
	 * Remember if we can skip the dma completion interrupt on
	 * the last segment in the chain.
	 */
	if (flags & DMAGO_NOINT) {
		if (dc->dm_cur == dc->dm_last)
			dc->dm_cmd &= ~DMA_ENAB;
		else
			dc->dm_flags |= DMAF_NOINTR;
	}
#ifdef DEBUG
	if (dmadebug & DDB_IO)
		if ((dmadebug&DDB_WORD) && (dc->dm_cmd&DMA_WORD) ||
		    (dmadebug&DDB_LWORD) && (dc->dm_cmd&DMA_LWORD)) {
			printf("dmago: cmd %x, flags %x\n",
			       dc->dm_cmd, dc->dm_flags);
			for (dcp = dc->dm_chain; dcp <= dc->dm_last; dcp++)
				printf("  %d: %d@%x\n", dcp-dc->dm_chain,
				       dcp->dc_count, dcp->dc_addr);
		}
	dmatimo[unit] = 1;
#endif
	DMA_ARM(dc);
}

void
dmastop(unit)
	register int unit;
{
	struct dma_softc *sc = &Dma_softc;
	register struct dma_channel *dc = &sc->sc_chan[unit];
	register struct devqueue *dq;

#ifdef DEBUG
	if (dmadebug & DDB_FOLLOW)
		printf("dmastop(%d)\n", unit);
	dmatimo[unit] = 0;
#endif
	DMA_CLEAR(dc);
#if defined(HP360) || defined(HP370) || defined(HP380)
	if (dc->dm_flags & DMAF_PCFLUSH) {
		PCIA();
		dc->dm_flags &= ~DMAF_PCFLUSH;
	}
#endif
#if defined(HP320) || defined(HP350)
	if (dc->dm_flags & DMAF_VCFLUSH) {
		/*
		 * 320/350s have VACs that may also need flushing.
		 * In our case we only flush the supervisor side
		 * because we know that if we are DMAing to user
		 * space, the physical pages will also be mapped
		 * in kernel space (via vmapbuf) and hence cache-
		 * inhibited by the pmap module due to the multiple
		 * mapping.
		 */
		DCIS();
		dc->dm_flags &= ~DMAF_VCFLUSH;
	}
#endif
	/*
	 * We may get this interrupt after a device service routine
	 * has freed the dma channel.  So, ignore the intr if there's
	 * nothing on the queue.
	 */
	dq = dmachan[unit].dq_forw;
	if (dq != &dmachan[unit])
		(dq->dq_driver->d_done)(dq->dq_unit);
}

int
dmaintr(arg)
	void *arg;
{
	struct dma_softc *sc = arg;
	register struct dma_channel *dc;
	register int i, stat;
	int found = 0;

#ifdef DEBUG
	if (dmadebug & DDB_FOLLOW)
		printf("dmaintr\n");
#endif
	for (i = 0; i < NDMACHAN; i++) {
		dc = &sc->sc_chan[i];
		stat = DMA_STAT(dc);
		if ((stat & DMA_INTR) == 0)
			continue;
		found++;
#ifdef DEBUG
		if (dmadebug & DDB_IO) {
			if ((dmadebug&DDB_WORD) && (dc->dm_cmd&DMA_WORD) ||
			    (dmadebug&DDB_LWORD) && (dc->dm_cmd&DMA_LWORD))
				printf("dmaintr: unit %d stat %x next %d\n",
				       i, stat, (dc->dm_cur-dc->dm_chain)+1);
		}
		if (stat & DMA_ARMED)
			printf("%s, chan %d: intr when armed\n",
			    sc->sc_xname, i);
#endif
		if (++dc->dm_cur <= dc->dm_last) {
#ifdef DEBUG
			dmatimo[i] = 1;
#endif
			/*
			 * Last chain segment, disable DMA interrupt.
			 */
			if (dc->dm_cur == dc->dm_last &&
			    (dc->dm_flags & DMAF_NOINTR))
				dc->dm_cmd &= ~DMA_ENAB;
			DMA_CLEAR(dc);
			DMA_ARM(dc);
		} else
			dmastop(i);
	}
	return(found);
}

#ifdef DEBUG
void
dmatimeout(arg)
	void *arg;
{
	register int i, s;
	struct dma_softc *sc = arg;

	for (i = 0; i < NDMACHAN; i++) {
		s = splbio();
		if (dmatimo[i]) {
			if (dmatimo[i] > 1)
				printf("%s: timeout #%d\n", sc->sc_xname,
				       i, dmatimo[i]-1);
			dmatimo[i]++;
		}
		splx(s);
	}
	timeout(dmatimeout, sc, 30 * hz);
}
#endif

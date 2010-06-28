/* $OpenBSD: ncr.c,v 1.27 2010/06/28 18:31:01 krw Exp $ */
/*	$NetBSD: ncr.c,v 1.32 2000/06/25 16:00:43 ragge Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass, David Jones, Gordon W. Ross, and Jens A. Nilsson.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file contains the machine-dependent parts of the NCR-5380
 * controller. The machine-independent parts are in ncr5380sbc.c.
 *
 * Jens A. Nilsson.
 *
 * Credits:
 * 
 * This code is based on arch/sun3/dev/si*
 * Written by David Jones, Gordon Ross, and Adam Glass.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/disk.h>
#include <sys/proc.h>

#include <uvm/uvm_extern.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_debug.h>
#include <scsi/scsiconf.h>
#include <scsi/sdvar.h>

#include <dev/ic/ncr5380reg.h>
#include <dev/ic/ncr5380var.h>

#include <machine/cpu.h>
#include <machine/vsbus.h>
#include <machine/bus.h>
#include <machine/sid.h>
#include <machine/scb.h>
#include <machine/clock.h>

#define MIN_DMA_LEN 128

struct si_dma_handle {
	int	dh_flags;
#define SIDH_BUSY	1
#define SIDH_OUT	2
	caddr_t dh_addr;
	int	dh_len;
	struct	proc *dh_proc;
};

struct si_softc {
	struct	ncr5380_softc	ncr_sc;
	struct	evcount		ncr_intrcnt;
	int			ncr_cvec;
	caddr_t ncr_addr;
	int	ncr_off;
	int	ncr_dmaaddr;
	int	ncr_dmacount;
	int	ncr_dmadir;

	/* Pointers to bus_space */
	bus_space_tag_t     sc_regt;
	bus_space_handle_t  sc_regh;

	struct	si_dma_handle ncr_dma[SCI_OPENINGS];
	struct	vsbus_dma sc_vd;
	int	onlyscsi;	/* This machine needs no queueing */
};

static int ncr_dmasize;

static	int si_match(struct device *, void *, void *);
static	void si_attach(struct device *, struct device *, void *);
static	void si_minphys(struct buf *, struct scsi_link *);

static	void si_dma_alloc(struct ncr5380_softc *);
static	void si_dma_free(struct ncr5380_softc *);
static	void si_dma_setup(struct ncr5380_softc *);
static	void si_dma_start(struct ncr5380_softc *);
static	void si_dma_poll(struct ncr5380_softc *);
static	void si_dma_stop(struct ncr5380_softc *);
static	void si_dma_go(void *);

#define NCR5380_READ(sc, reg)	bus_space_read_1(sc->sc_regt,		\
	0, sc->ncr_sc.reg)
#define NCR5380_WRITE(sc, reg, val)	bus_space_write_1(sc->sc_regt,	\
	0, sc->ncr_sc.reg, val)

struct scsi_adapter	si_ops = {
	ncr5380_scsi_cmd,	/* scsi_cmd() */
	si_minphys,		/* scsi_minphys() */
	NULL,			/* probe_dev() */
	NULL			/* free_dev() */
};

struct cfattach ncr_ca = {
	sizeof(struct si_softc), si_match, si_attach
};

struct cfdriver ncr_cd = {
	NULL, "ncr", DV_DULL
};

static int
si_match(parent, cf, aux)
	struct device *parent;
	void *cf;
	void *aux;
{
	struct vsbus_attach_args *va = aux;
	volatile char *si_csr = (char *) va->va_addr;

	if (vax_boardtype == VAX_BTYP_49 || vax_boardtype == VAX_BTYP_46
	    || vax_boardtype == VAX_BTYP_48 || vax_boardtype == VAX_BTYP_1303)
		return 0;
	/* This is the way Linux autoprobes the interrupt MK-990321 */
	si_csr[12] = 0;
	si_csr[16] = 0x80;
	si_csr[0] = 0x80;
	si_csr[4] = 5; /* 0xcf */
	DELAY(100000);
	return 1;
}

static void
si_attach(parent, self, aux)
	struct device	*parent, *self;
	void		*aux;
{
	struct vsbus_attach_args *va = aux;
	struct si_softc *sc = (struct si_softc *) self;
	struct ncr5380_softc *ncr_sc = &sc->ncr_sc;
	struct scsibus_attach_args saa;
	int tweak, target;

	printf("\n");

	/* enable interrupts on vsbus too */
	scb_vecalloc(va->va_cvec, (void (*)(void *)) ncr5380_intr, sc,
	    SCB_ISTACK, &sc->ncr_intrcnt);
	sc->ncr_cvec = va->va_cvec;
	evcount_attach(&sc->ncr_intrcnt, self->dv_xname,
	    (void *)&sc->ncr_cvec, &evcount_intr);

	/*
	 * DMA area mapin.
	 * On VS3100, split the 128K block between the two devices.
	 * On VS2000, don't care for now.
	 */
#define DMASIZE (64*1024)
	if (va->va_paddr & 0x100) { /* Secondary SCSI controller */
		sc->ncr_off = DMASIZE;
		sc->onlyscsi = 1;
	}
	sc->ncr_addr = (caddr_t)va->va_dmaaddr;
	ncr_dmasize = min(va->va_dmasize, MAXPHYS);

	/*
	 * MD function pointers used by the MI code.
	 */
	ncr_sc->sc_dma_alloc = si_dma_alloc;
	ncr_sc->sc_dma_free  = si_dma_free;
	ncr_sc->sc_dma_setup = si_dma_setup;
	ncr_sc->sc_dma_start = si_dma_start;
	ncr_sc->sc_dma_poll  = si_dma_poll;
	ncr_sc->sc_dma_stop  = si_dma_stop;

	/* DMA control register offsets */
	sc->ncr_dmaaddr = 32;	/* DMA address in buffer, longword */
	sc->ncr_dmacount = 64;	/* DMA count register */
	sc->ncr_dmadir = 68;	/* Direction of DMA transfer */

	ncr_sc->sc_pio_out = ncr5380_pio_out;
	ncr_sc->sc_pio_in =  ncr5380_pio_in;

	ncr_sc->sc_min_dma_len = MIN_DMA_LEN;

	/*
	 * Initialize fields used by the MI code.
	 */
/*	sc->sc_regt =  Unused on VAX */
	sc->sc_regh = vax_map_physmem(va->va_paddr, 1);

	/* Register offsets */
	ncr_sc->sci_r0 = (void *)sc->sc_regh+0;
	ncr_sc->sci_r1 = (void *)sc->sc_regh+4;
	ncr_sc->sci_r2 = (void *)sc->sc_regh+8;
	ncr_sc->sci_r3 = (void *)sc->sc_regh+12;
	ncr_sc->sci_r4 = (void *)sc->sc_regh+16;
	ncr_sc->sci_r5 = (void *)sc->sc_regh+20;
	ncr_sc->sci_r6 = (void *)sc->sc_regh+24;
	ncr_sc->sci_r7 = (void *)sc->sc_regh+28;

	ncr_sc->sc_no_disconnect = 0xff;

	/*
	 * Get the SCSI chip target address out of NVRAM.
	 * This do not apply to the VS2000.
	 */
	tweak = clk_tweak + (va->va_paddr & 0x100 ? 3 : 0);
	if (vax_boardtype == VAX_BTYP_410)
		target = 7;
	else
		target = (clk_page[0xbc/2] >> tweak) & 7;

	ncr_sc->sc_link.adapter_softc =	sc;
	ncr_sc->sc_link.adapter_target = target;
	ncr_sc->sc_link.adapter = &si_ops;
	ncr_sc->sc_link.openings = 4;

	/*
	 * Init the vsbus DMA resource queue struct */
	sc->sc_vd.vd_go = si_dma_go;
	sc->sc_vd.vd_arg = sc;

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &(ncr_sc->sc_link);

	/*
	 * Initialize si board itself.
	 */
	ncr5380_init(ncr_sc);
	ncr5380_reset_scsibus(ncr_sc);
	DELAY(2000000);
	config_found(&(ncr_sc->sc_dev), &saa, scsiprint);

}

/*
 * Adjust the max transfer size. The DMA buffer is only 16k on VS2000.
 */
static void
si_minphys(struct buf *bp, struct scsi_link *sl)
{
	if (bp->b_bcount > ncr_dmasize)
		bp->b_bcount = ncr_dmasize;
	minphys(bp);
}

void
si_dma_alloc(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	struct scsi_xfer *xs = sr->sr_xs;
	struct si_dma_handle *dh;
	struct buf *bp;
	int xlen, i;

#ifdef DIAGNOSTIC
	if (sr->sr_dma_hand != NULL)
		panic("si_dma_alloc: already have DMA handle");
#endif

	/* Polled transfers shouldn't allocate a DMA handle. */
	if (sr->sr_flags & SR_IMMED)
		return;

	xlen = ncr_sc->sc_datalen;

	/* Make sure our caller checked sc_min_dma_len. */
	if (xlen < MIN_DMA_LEN)
		panic("si_dma_alloc: len=0x%x", xlen);

	/*
	 * Find free PDMA handle.  Guaranteed to find one since we
	 * have as many PDMA handles as the driver has processes.
	 * (instances?)
	 */
	 for (i = 0; i < SCI_OPENINGS; i++) {
		if ((sc->ncr_dma[i].dh_flags & SIDH_BUSY) == 0)
			goto found;
	}
	panic("ncr: no free PDMA handles");
found:
	dh = &sc->ncr_dma[i];
	dh->dh_flags = SIDH_BUSY;
	dh->dh_addr = ncr_sc->sc_dataptr;
	dh->dh_len = xlen;
	bp = xs->bp;
	if (bp != NULL)
		dh->dh_proc = (bp->b_flags & B_PHYS ? bp->b_proc : NULL);
	else
		dh->dh_proc = NULL;

	/* Remember dest buffer parameters */
	if (xs->flags & SCSI_DATA_OUT)
		dh->dh_flags |= SIDH_OUT;

	sr->sr_dma_hand = dh;
}

void
si_dma_free(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct sci_req *sr = ncr_sc->sc_current;
	struct si_dma_handle *dh = sr->sr_dma_hand;

#ifdef DIAGNOSTIC
	if (dh == NULL)
		panic("si_dma_free: no DMA handle");
#endif

	if (ncr_sc->sc_state & NCR_DOINGDMA)
		panic("si_dma_free: free while DMA in progress");

	if (dh->dh_flags & SIDH_BUSY)
		dh->dh_flags = 0;
	else
		printf("si_dma_free: free'ing unused buffer\n");

	sr->sr_dma_hand = NULL;
}

void
si_dma_setup(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	/* Do nothing here */
}

void
si_dma_start(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;

	/* Just put on queue; will call go() from below */
	if (sc->onlyscsi)
		si_dma_go(ncr_sc);
	else
		vsbus_dma_start(&sc->sc_vd);
}

/*
 * go() routine called when another transfer somewhere is finished.
 */
void
si_dma_go(arg)
	void *arg;
{
	struct ncr5380_softc *ncr_sc = arg;
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	struct si_dma_handle *dh = sr->sr_dma_hand;

	/*
	 * Set the VAX-DMA-specific registers, and copy the data if
	 * it is directed "outbound".
	 */
	if (dh->dh_flags & SIDH_OUT) {
		vsbus_copyfromproc(dh->dh_proc, dh->dh_addr,
		    sc->ncr_addr + sc->ncr_off, dh->dh_len);
		bus_space_write_1(sc->sc_regt, sc->sc_regh,
		    sc->ncr_dmadir, 0);
	} else {
		bus_space_write_1(sc->sc_regt, sc->sc_regh,
		    sc->ncr_dmadir, 1);
	}
	bus_space_write_4(sc->sc_regt, sc->sc_regh,
	    sc->ncr_dmacount, -dh->dh_len);
	bus_space_write_4(sc->sc_regt, sc->sc_regh,
	    sc->ncr_dmaaddr, sc->ncr_off);
	/*
	 * Now from the 5380-internal DMA registers.
	 */
	if (dh->dh_flags & SIDH_OUT) {
		NCR5380_WRITE(sc, sci_tcmd, PHASE_DATA_OUT);
		NCR5380_WRITE(sc, sci_icmd, SCI_ICMD_DATA);
		NCR5380_WRITE(sc, sci_mode, NCR5380_READ(sc, sci_mode)
		    | SCI_MODE_DMA | SCI_MODE_DMA_IE);
		NCR5380_WRITE(sc, sci_dma_send, 0);
	} else {
		NCR5380_WRITE(sc, sci_tcmd, PHASE_DATA_IN);
		NCR5380_WRITE(sc, sci_icmd, 0);
		NCR5380_WRITE(sc, sci_mode, NCR5380_READ(sc, sci_mode)
		    | SCI_MODE_DMA | SCI_MODE_DMA_IE);
		NCR5380_WRITE(sc, sci_irecv, 0);
	}
	ncr_sc->sc_state |= NCR_DOINGDMA;
}

/*
 * When?
 */
void
si_dma_poll(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	printf("si_dma_poll\n");
}

void
si_dma_stop(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	struct si_dma_handle *dh = sr->sr_dma_hand;
	int count, i;

	if (ncr_sc->sc_state & NCR_DOINGDMA) 
		ncr_sc->sc_state &= ~NCR_DOINGDMA;

	/*
	 * Sometimes the FIFO buffer isn't drained when the
	 * interrupt is posted. Just loop here and hope that
	 * it will drain soon.
	 */
	for (i = 0; i < 20000; i++) {
		count = bus_space_read_4(sc->sc_regt,
		    sc->sc_regh, sc->ncr_dmacount);
		if (count == 0)
			break;
		DELAY(100);
	}
	if (count == 0) {
		if (((dh->dh_flags & SIDH_OUT) == 0)) {
			vsbus_copytoproc(dh->dh_proc,
			    sc->ncr_addr + sc->ncr_off,
			    dh->dh_addr, dh->dh_len);
		}
		ncr_sc->sc_dataptr += dh->dh_len;
		ncr_sc->sc_datalen -= dh->dh_len;
	}

	NCR5380_WRITE(sc, sci_mode, NCR5380_READ(sc, sci_mode) &
	    ~(SCI_MODE_DMA | SCI_MODE_DMA_IE));
	NCR5380_WRITE(sc, sci_icmd, 0);
	if (sc->onlyscsi == 0)
		vsbus_dma_intr(); /* Try to start more transfers */
}

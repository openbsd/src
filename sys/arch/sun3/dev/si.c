/*	$OpenBSD: si.c,v 1.9 1999/01/11 05:12:02 millert Exp $	*/
/*	$NetBSD: si.c,v 1.31 1996/11/20 18:56:59 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass, David Jones, and Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file contains only the machine-dependent parts of the
 * Sun3 SCSI driver.  (Autoconfig stuff and DMA functions.)
 * The machine-independent parts are in ncr5380sbc.c
 *
 * Supported hardware includes:
 * Sun SCSI-3 on OBIO (Sun3/50,Sun3/60)
 * Sun SCSI-3 on VME (Sun3/160,Sun3/260)
 *
 * Could be made to support the Sun3/E if someone wanted to.
 *
 * Note:  Both supported variants of the Sun SCSI-3 adapter have
 * some really unusual "features" for this driver to deal with,
 * generally related to the DMA engine.  The OBIO variant will
 * ignore any attempt to write the FIFO count register while the
 * SCSI bus is in DATA_IN or DATA_OUT phase.  This is dealt with
 * by setting the FIFO count early in COMMAND or MSG_IN phase.
 *
 * The VME variant has a bit to enable or disable the DMA engine,
 * but that bit also gates the interrupt line from the NCR5380!
 * Therefore, in order to get any interrupt from the 5380, (i.e.
 * for reselect) one must clear the DMA engine transfer count and
 * then enable DMA.  This has the further complication that you
 * CAN NOT touch the NCR5380 while the DMA enable bit is set, so
 * we have to turn DMA back off before we even look at the 5380.
 *
 * What wonderfully whacky hardware this is!
 *
 * Credits, history:
 *
 * David Jones wrote the initial version of this module, which
 * included support for the VME adapter only. (no reselection).
 *
 * Gordon Ross added support for the OBIO adapter, and re-worked
 * both the VME and OBIO code to support disconnect/reselect.
 * (Required figuring out the hardware "features" noted above.)
 *
 * The autoconfiguration boilerplate came from Adam Glass.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_debug.h>
#include <scsi/scsiconf.h>

#include <machine/autoconf.h>
#include <machine/obio.h>
#include <machine/dvma.h>

#define DEBUG XXX

#include <dev/ic/ncr5380reg.h>
#include <dev/ic/ncr5380var.h>

#include "sireg.h"
#include "sivar.h"

/*
 * Transfers smaller than this are done using PIO
 * (on assumption they're not worth DMA overhead)
 */
#define	MIN_DMA_LEN 128

int si_debug = 0;
#ifdef	DEBUG
static int si_link_flags = 0 /* | SDEV_DB2 */ ;
#endif

/* How long to wait for DMA before declaring an error. */
int si_dma_intr_timo = 500;	/* ticks (sec. X 100) */

static void	si_minphys __P((struct buf *));

static struct scsi_adapter	si_ops = {
	ncr5380_scsi_cmd,		/* scsi_cmd()		*/
	si_minphys,			/* scsi_minphys()	*/
	NULL,				/* open_target_lu()	*/
	NULL,				/* close_target_lu()	*/
};

/* This is copied from julian's bt driver */
/* "so we have a default dev struct for our link struct." */
static struct scsi_device si_dev = {
	NULL,		/* Use default error handler.	    */
	NULL,		/* Use default start handler.		*/
	NULL,		/* Use default async handler.	    */
	NULL,		/* Use default "done" routine.	    */
};

/*
 * New-style autoconfig attachment. The cfattach
 * structures are in si_obio.c and si_vme.c
 */

struct cfdriver si_cd = {
	NULL, "si", DV_DULL
};


void
si_attach(sc)
	struct si_softc *sc;
{
	struct ncr5380_softc *ncr_sc = (void *)sc;
	volatile struct si_regs *regs = sc->sc_regs;
	int i;

	/*
	 * Support the "options" (config file flags).
	 */
	if ((sc->sc_options & SI_DO_RESELECT) != 0)
		ncr_sc->sc_flags |= NCR5380_PERMIT_RESELECT;
	if ((sc->sc_options & SI_DMA_INTR) == 0)
		ncr_sc->sc_flags |= NCR5380_FORCE_POLLING;
#if 1	/* XXX - Temporary */
	/* XXX - In case we think DMA is completely broken... */
	if ((sc->sc_options & SI_ENABLE_DMA) == 0) {
		/* Override this function pointer. */
		ncr_sc->sc_dma_alloc = NULL;
	}
#endif
	ncr_sc->sc_min_dma_len = MIN_DMA_LEN;

	/*
	 * Fill in the prototype scsi_link.
	 */
#ifndef __OpenBSD__
	ncr_sc->sc_link.channel = SCSI_CHANNEL_ONLY_ONE;
#endif
	ncr_sc->sc_link.adapter_softc = sc;
	ncr_sc->sc_link.adapter_target = 7;
	ncr_sc->sc_link.adapter = &si_ops;
	ncr_sc->sc_link.device = &si_dev;

#ifdef	DEBUG
	if (si_debug)
		printf("si: Set TheSoftC=%p TheRegs=%p\n", sc, regs);
	ncr_sc->sc_link.flags |= si_link_flags;
#endif

	/*
	 * Initialize fields used by the MI code
	 */
	ncr_sc->sci_r0 = &regs->sci.sci_r0;
	ncr_sc->sci_r1 = &regs->sci.sci_r1;
	ncr_sc->sci_r2 = &regs->sci.sci_r2;
	ncr_sc->sci_r3 = &regs->sci.sci_r3;
	ncr_sc->sci_r4 = &regs->sci.sci_r4;
	ncr_sc->sci_r5 = &regs->sci.sci_r5;
	ncr_sc->sci_r6 = &regs->sci.sci_r6;
	ncr_sc->sci_r7 = &regs->sci.sci_r7;

	/*
	 * Allocate DMA handles.
	 */
	i = SCI_OPENINGS * sizeof(struct si_dma_handle);
	sc->sc_dma = (struct si_dma_handle *)
		malloc(i, M_DEVBUF, M_WAITOK);
	if (sc->sc_dma == NULL)
		panic("si: dvma_malloc failed");
	for (i = 0; i < SCI_OPENINGS; i++)
		sc->sc_dma[i].dh_flags = 0;

	/*
	 *  Initialize si board itself.
	 */
	si_reset_adapter(ncr_sc);
	ncr5380_init(ncr_sc);
	ncr5380_reset_scsibus(ncr_sc);
	config_found(&(ncr_sc->sc_dev), &(ncr_sc->sc_link), scsiprint);
}

static void
si_minphys(struct buf *bp)
{
	if (bp->b_bcount > MAX_DMA_LEN) {
#ifdef	DEBUG
		if (si_debug) {
			printf("si_minphys len = 0x%lx.\n", bp->b_bcount);
			Debugger();
		}
#endif
		bp->b_bcount = MAX_DMA_LEN;
	}
	return (minphys(bp));
}


#define CSR_WANT (SI_CSR_SBC_IP | SI_CSR_DMA_IP | \
	SI_CSR_DMA_CONFLICT | SI_CSR_DMA_BUS_ERR )

int
si_intr(void *arg)
{
	struct si_softc *sc = arg;
	volatile struct si_regs *si = sc->sc_regs;
	int dma_error, claimed;
	u_short csr;

	claimed = 0;
	dma_error = 0;

	/* SBC interrupt? DMA interrupt? */
	csr = si->si_csr;
	NCR_TRACE("si_intr: csr=0x%x\n", csr);

	if (csr & SI_CSR_DMA_CONFLICT) {
		dma_error |= SI_CSR_DMA_CONFLICT;
		printf("si_intr: DMA conflict\n");
	}
	if (csr & SI_CSR_DMA_BUS_ERR) {
		dma_error |= SI_CSR_DMA_BUS_ERR;
		printf("si_intr: DMA bus error\n");
	}
	if (dma_error) {
		if (sc->ncr_sc.sc_state & NCR_DOINGDMA)
			sc->ncr_sc.sc_state |= NCR_ABORTING;
		/* Make sure we will call the main isr. */
		csr |= SI_CSR_DMA_IP;
	}

	if (csr & (SI_CSR_SBC_IP | SI_CSR_DMA_IP)) {
		claimed = ncr5380_intr(&sc->ncr_sc);
#ifdef	DEBUG
		if (!claimed) {
			printf("si_intr: spurious from SBC\n");
			if (si_debug & 4) {
				Debugger();	/* XXX */
			}
		}
#endif
	}

	return (claimed);
}


void
si_reset_adapter(struct ncr5380_softc *ncr_sc)
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	volatile struct si_regs *si = sc->sc_regs;

#ifdef	DEBUG
	if (si_debug) {
		printf("si_reset_adapter\n");
	}
#endif

	/*
	 * The SCSI3 controller has an 8K FIFO to buffer data between the
	 * 5380 and the DMA.  Make sure it starts out empty.
	 *
	 * The reset bits in the CSR are active low.
	 */
	si->si_csr = 0;
	delay(10);
	si->si_csr = SI_CSR_FIFO_RES | SI_CSR_SCSI_RES | SI_CSR_INTR_EN;
	delay(10);
	si->fifo_count = 0;

	if (sc->sc_adapter_type == BUS_VME16) {
		si->dma_addrh = 0;
		si->dma_addrl = 0;
		si->dma_counth = 0;
		si->dma_countl = 0;
		si->si_iv_am = sc->sc_adapter_iv_am;
		si->fifo_cnt_hi = 0;
	}

	SCI_CLR_INTR(ncr_sc);
}


/*****************************************************************
 * Common functions for DMA
 ****************************************************************/

/*
 * Allocate a DMA handle and put it in sc->sc_dma.  Prepare
 * for DMA transfer.  On the Sun3, this means mapping the buffer
 * into DVMA space.  dvma_mapin() flushes the cache for us.
 */
void
si_dma_alloc(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	struct scsi_xfer *xs = sr->sr_xs;
	struct si_dma_handle *dh;
	int i, xlen;
	u_long addr;

#ifdef	DIAGNOSTIC
	if (sr->sr_dma_hand != NULL)
		panic("si_dma_alloc: already have DMA handle");
#endif

	addr = (u_long) ncr_sc->sc_dataptr;
	xlen = ncr_sc->sc_datalen;

	/* If the DMA start addr is misaligned then do PIO */
	if ((addr & 1) || (xlen & 1)) {
		printf("si_dma_alloc: misaligned.\n");
		return;
	}

	/* Make sure our caller checked sc_min_dma_len. */
	if (xlen < MIN_DMA_LEN)
		panic("si_dma_alloc: xlen=0x%x", xlen);

	/*
	 * Never attempt single transfers of more than 63k, because
	 * our count register may be only 16 bits (an OBIO adapter).
	 * This should never happen since already bounded by minphys().
	 * XXX - Should just segment these...
	 */
	if (xlen > MAX_DMA_LEN) {
		printf("si_dma_alloc: excessive xlen=0x%x\n", xlen);
		Debugger();
		ncr_sc->sc_datalen = xlen = MAX_DMA_LEN;
	}

	/* Find free DMA handle.  Guaranteed to find one since we have
	   as many DMA handles as the driver has processes. */
	for (i = 0; i < SCI_OPENINGS; i++) {
		if ((sc->sc_dma[i].dh_flags & SIDH_BUSY) == 0)
			goto found;
	}
	panic("si: no free DMA handles.");
found:

	dh = &sc->sc_dma[i];
	dh->dh_flags = SIDH_BUSY;
	dh->dh_addr = (u_char*) addr;
	dh->dh_maplen  = xlen;
	dh->dh_dvma = 0;

	/* Copy the "write" flag for convenience. */
	if (xs->flags & SCSI_DATA_OUT)
		dh->dh_flags |= SIDH_OUT;

#if 0
	/*
	 * Some machines might not need to remap B_PHYS buffers.
	 * The sun3 does not map B_PHYS buffers into DVMA space,
	 * (they are mapped into normal KV space) so on the sun3
	 * we must always remap to a DVMA address here. Re-map is
	 * cheap anyway, because it's done by segments, not pages.
	 */
	if (xs->bp && (xs->bp->b_flags & B_PHYS))
		dh->dh_flags |= SIDH_PHYS;
#endif

	dh->dh_dvma = (u_long) dvma_mapin((char *)addr, xlen);
	if (!dh->dh_dvma) {
		/* Can't remap segment */
		printf("si_dma_alloc: can't remap %p/%x\n",
			dh->dh_addr, dh->dh_maplen);
		dh->dh_flags = 0;
		return;
	}

	/* success */
	sr->sr_dma_hand = dh;

	return;
}


void
si_dma_free(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct sci_req *sr = ncr_sc->sc_current;
	struct si_dma_handle *dh = sr->sr_dma_hand;

#ifdef	DIAGNOSTIC
	if (dh == NULL)
		panic("si_dma_free: no DMA handle");
#endif

	if (ncr_sc->sc_state & NCR_DOINGDMA)
		panic("si_dma_free: free while in progress");

	if (dh->dh_flags & SIDH_BUSY) {
		/* XXX - Should separate allocation and mapping. */
		/* Give back the DVMA space. */
		dvma_mapout((caddr_t)dh->dh_dvma, dh->dh_maplen);
		dh->dh_dvma = 0;
		dh->dh_flags = 0;
	}
	sr->sr_dma_hand = NULL;
}


#define	CSR_MASK (SI_CSR_SBC_IP | SI_CSR_DMA_IP | \
		SI_CSR_DMA_CONFLICT | SI_CSR_DMA_BUS_ERR)
#define	POLL_TIMO	50000	/* X100 = 5 sec. */

/*
 * Poll (spin-wait) for DMA completion.
 * Called right after xx_dma_start(), and
 * xx_dma_stop() will be called next.
 * Same for either VME or OBIO.
 */
void
si_dma_poll(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	volatile struct si_regs *si = sc->sc_regs;
	int tmo;

	/* Make sure DMA started successfully. */
	if (ncr_sc->sc_state & NCR_ABORTING)
		return;

	/*
	 * XXX: The Sun driver waits for ~SI_CSR_DMA_ACTIVE here
	 * XXX: (on obio) or even worse (on vme) a 10mS. delay!
	 * XXX: I really doubt that is necessary...
	 */

	/* Wait for any "dma complete" or error bits. */
	tmo = POLL_TIMO;
	for (;;) {
		if (si->si_csr & CSR_MASK)
			break;
		if (--tmo <= 0) {
			printf("si: DMA timeout (while polling)\n");
			/* Indicate timeout as MI code would. */
			sr->sr_flags |= SR_OVERDUE;
			break;
		}
		delay(100);
	}
	NCR_TRACE("si_dma_poll: waited %d\n",
			  POLL_TIMO - tmo);

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_poll: done, csr=0x%x\n", si->si_csr);
	}
#endif
}

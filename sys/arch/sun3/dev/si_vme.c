/*	$NetBSD: si_vme.c,v 1.7 1996/11/20 18:57:01 gwr Exp $	*/

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

/*****************************************************************
 * VME functions for DMA
 ****************************************************************/

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
#include <machine/isr.h>
#include <machine/obio.h>
#include <machine/dvma.h>

#define DEBUG XXX

#include <dev/ic/ncr5380reg.h>
#include <dev/ic/ncr5380var.h>

#include "sireg.h"
#include "sivar.h"

void si_vme_dma_setup __P((struct ncr5380_softc *));
void si_vme_dma_start __P((struct ncr5380_softc *));
void si_vme_dma_eop __P((struct ncr5380_softc *));
void si_vme_dma_stop __P((struct ncr5380_softc *));

void si_vme_intr_on  __P((struct ncr5380_softc *));
void si_vme_intr_off __P((struct ncr5380_softc *));

/*
 * New-style autoconfig attachment
 */

static int	si_vmes_match __P((struct device *, void *, void *));
static void	si_vmes_attach __P((struct device *, struct device *, void *));

struct cfattach si_vmes_ca = {
	sizeof(struct si_softc), si_vmes_match, si_vmes_attach
};

/* Options.  Interesting values are: 1,3,7 */
int si_vme_options = 3;


static int
si_vmes_match(parent, vcf, args)
	struct device	*parent;
	void		*vcf, *args;
{
	struct cfdata	*cf = vcf;
	struct confargs *ca = args;
	int probe_addr;

#ifdef	DIAGNOSTIC
	if (ca->ca_bustype != BUS_VME16) {
		printf("si_vmes_match: bustype %d?\n", ca->ca_bustype);
		return (0);
	}
#endif

	/*
	 * Other Sun3 models may have VME "si" or "sc".
	 * This driver has no default address.
	 */
	if (ca->ca_paddr == -1)
		return (0);

	/* Make sure there is something there... */
	probe_addr = ca->ca_paddr + 1;
	if (bus_peek(ca->ca_bustype, probe_addr, 1) == -1)
		return (0);

	/*
	 * If this is a VME SCSI board, we have to determine whether
	 * it is an "sc" (Sun2) or "si" (Sun3) SCSI board.  This can
	 * be determined using the fact that the "sc" board occupies
	 * 4K bytes in VME space but the "si" board occupies 2K bytes.
	 */
	/* Note: the "si" board should NOT respond here. */
	probe_addr = ca->ca_paddr + 0x801;
	if (bus_peek(ca->ca_bustype, probe_addr, 1) != -1) {
		/* Something responded at 2K+1.  Maybe an "sc" board? */
#ifdef	DEBUG
		printf("si_vmes_match: May be an `sc' board at pa=0x%x\n",
			   ca->ca_paddr);
#endif
		return(0);
	}

	/* Default interrupt priority (always splbio==2) */
	if (ca->ca_intpri == -1)
		ca->ca_intpri = 2;

	return (1);
}

static void
si_vmes_attach(parent, self, args)
	struct device	*parent, *self;
	void		*args;
{
	struct si_softc *sc = (struct si_softc *) self;
	struct ncr5380_softc *ncr_sc = &sc->ncr_sc;
	struct cfdata *cf = self->dv_cfdata;
	struct confargs *ca = args;

	/* Get options from config flags... */
	sc->sc_options = cf->cf_flags | si_vme_options;
	printf(": options=%d\n", sc->sc_options);

	sc->sc_adapter_type = ca->ca_bustype;
	sc->sc_regs = (struct si_regs *)
		bus_mapin(ca->ca_bustype, ca->ca_paddr,
				sizeof(struct si_regs));
	sc->sc_adapter_iv_am =
		VME_SUPV_DATA_24 | (ca->ca_intvec & 0xFF);

	/*
	 * MD function pointers used by the MI code.
	 */
	ncr_sc->sc_pio_out = ncr5380_pio_out;
	ncr_sc->sc_pio_in =  ncr5380_pio_in;
	ncr_sc->sc_dma_alloc = si_dma_alloc;
	ncr_sc->sc_dma_free  = si_dma_free;
	ncr_sc->sc_dma_setup = si_vme_dma_setup;
	ncr_sc->sc_dma_start = si_vme_dma_start;
	ncr_sc->sc_dma_poll  = si_dma_poll;
	ncr_sc->sc_dma_eop   = si_vme_dma_eop;
	ncr_sc->sc_dma_stop  = si_vme_dma_stop;
	ncr_sc->sc_intr_on   = si_vme_intr_on;
	ncr_sc->sc_intr_off  = si_vme_intr_off;

	/* Attach interrupt handler. */
	isr_add_vectored(si_intr, (void *)sc,
		ca->ca_intpri, ca->ca_intvec);

	/* Do the common attach stuff. */
	si_attach(sc);
}


/*
 * This is called when the bus is going idle,
 * so we want to enable the SBC interrupts.
 * That is controlled by the DMA enable!
 * Who would have guessed!
 * What a NASTY trick!
 */
void
si_vme_intr_on(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	volatile struct si_regs *si = sc->sc_regs;

	/* receive mode should be safer */
	si->si_csr &= ~SI_CSR_SEND;

	/* Clear the count so nothing happens. */
	si->dma_counth = 0;
	si->dma_countl = 0;
	
	/* Clear the start address too. (paranoid?) */
	si->dma_addrh = 0;
	si->dma_addrl = 0;

	/* Finally, enable the DMA engine. */
	si->si_csr |= SI_CSR_DMA_EN;
}

/*
 * This is called when the bus is idle and we are
 * about to start playing with the SBC chip.
 */
void
si_vme_intr_off(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	volatile struct si_regs *si = sc->sc_regs;

	si->si_csr &= ~SI_CSR_DMA_EN;
}

/*
 * This function is called during the COMMAND or MSG_IN phase
 * that preceeds a DATA_IN or DATA_OUT phase, in case we need
 * to setup the DMA engine before the bus enters a DATA phase.
 *
 * XXX: The VME adapter appears to suppress SBC interrupts
 * when the FIFO is not empty or the FIFO count is non-zero!
 *
 * On the VME version, setup the start addres, but clear the
 * count (to make sure it stays idle) and set that later.
 */
void
si_vme_dma_setup(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	struct si_dma_handle *dh = sr->sr_dma_hand;
	volatile struct si_regs *si = sc->sc_regs;
	long data_pa;
	int xlen;

	/*
	 * Get the DVMA mapping for this segment.
	 * XXX - Should separate allocation and mapin.
	 */
	data_pa = dvma_kvtopa(dh->dh_dvma, sc->sc_adapter_type);
	data_pa += (ncr_sc->sc_dataptr - dh->dh_addr);
	if (data_pa & 1)
		panic("si_dma_start: bad pa=0x%x", data_pa);
	xlen = ncr_sc->sc_datalen;
	xlen &= ~1;				/* XXX: necessary? */
	sc->sc_reqlen = xlen; 	/* XXX: or less? */

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_setup: dh=0x%x, pa=0x%x, xlen=%d\n",
			   dh, data_pa, xlen);
	}
#endif

	/* Set direction (send/recv) */
	if (dh->dh_flags & SIDH_OUT) {
		si->si_csr |= SI_CSR_SEND;
	} else {
		si->si_csr &= ~SI_CSR_SEND;
	}

	/* Reset the FIFO. */
	si->si_csr &= ~SI_CSR_FIFO_RES; 	/* active low */
	si->si_csr |= SI_CSR_FIFO_RES;

	if (data_pa & 2) {
		si->si_csr |= SI_CSR_BPCON;
	} else {
		si->si_csr &= ~SI_CSR_BPCON;
	}

	/* Load the start address. */
	si->dma_addrh = (ushort)(data_pa >> 16);
	si->dma_addrl = (ushort)(data_pa & 0xFFFF);

	/*
	 * Keep the count zero or it may start early!
	 */
	si->dma_counth = 0;
	si->dma_countl = 0;

#if 0
	/* Clear FIFO counter. (also hits dma_count) */
	si->fifo_cnt_hi = 0;
	si->fifo_count = 0;		
#endif
}


void
si_vme_dma_start(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	struct si_dma_handle *dh = sr->sr_dma_hand;
	volatile struct si_regs *si = sc->sc_regs;
	long data_pa;
	int s, xlen;

	xlen = sc->sc_reqlen;

	/* This MAY be time critical (not sure). */
	s = splhigh();

	si->dma_counth = (ushort)(xlen >> 16);
	si->dma_countl = (ushort)(xlen & 0xFFFF);

	/* Set it anyway, even though dma_count hits it. */
	si->fifo_cnt_hi = (ushort)(xlen >> 16);
	si->fifo_count  = (ushort)(xlen & 0xFFFF);

	/*
	 * Acknowledge the phase change.  (After DMA setup!)
	 * Put the SBIC into DMA mode, and start the transfer.
	 */
	if (dh->dh_flags & SIDH_OUT) {
		*ncr_sc->sci_tcmd = PHASE_DATA_OUT;
		SCI_CLR_INTR(ncr_sc);
		*ncr_sc->sci_icmd = SCI_ICMD_DATA;
		*ncr_sc->sci_mode |= (SCI_MODE_DMA | SCI_MODE_DMA_IE);
		*ncr_sc->sci_dma_send = 0;	/* start it */
	} else {
		*ncr_sc->sci_tcmd = PHASE_DATA_IN;
		SCI_CLR_INTR(ncr_sc);
		*ncr_sc->sci_icmd = 0;
		*ncr_sc->sci_mode |= (SCI_MODE_DMA | SCI_MODE_DMA_IE);
		*ncr_sc->sci_irecv = 0;	/* start it */
	}

	/* Let'er rip! */
	si->si_csr |= SI_CSR_DMA_EN;

	splx(s);
	ncr_sc->sc_state |= NCR_DOINGDMA;

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_start: started, flags=0x%x\n",
			   ncr_sc->sc_state);
	}
#endif
}


void
si_vme_dma_eop(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{

	/* Not needed - DMA was stopped prior to examining sci_csr */
}


void
si_vme_dma_stop(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	struct si_dma_handle *dh = sr->sr_dma_hand;
	volatile struct si_regs *si = sc->sc_regs;
	int resid, ntrans;

	if ((ncr_sc->sc_state & NCR_DOINGDMA) == 0) {
#ifdef	DEBUG
		printf("si_dma_stop: dma not running\n");
#endif
		return;
	}
	ncr_sc->sc_state &= ~NCR_DOINGDMA;

	/* First, halt the DMA engine. */
	si->si_csr &= ~SI_CSR_DMA_EN;	/* VME only */

	/* Set an impossible phase to prevent data movement? */
	*ncr_sc->sci_tcmd = PHASE_INVALID;

	if (si->si_csr & (SI_CSR_DMA_CONFLICT | SI_CSR_DMA_BUS_ERR)) {
		printf("si: DMA error, csr=0x%x, reset\n", si->si_csr);
		sr->sr_xs->error = XS_DRIVER_STUFFUP;
		ncr_sc->sc_state |= NCR_ABORTING;
		si_reset_adapter(ncr_sc);
		goto out;
	}

	/* Note that timeout may have set the error flag. */
	if (ncr_sc->sc_state & NCR_ABORTING)
		goto out;

	/* XXX: Wait for DMA to actually finish? */

	/*
	 * Now try to figure out how much actually transferred
	 *
	 * The fifo_count does not reflect how many bytes were
	 * actually transferred for VME.
	 *
	 * SCSI-3 VME interface is a little funny on writes:
	 * if we have a disconnect, the dma has overshot by
	 * one byte and the resid needs to be incremented.
	 * Only happens for partial transfers.
	 * (Thanks to Matt Jacob)
	 */

	resid = si->fifo_count & 0xFFFF;
	if (dh->dh_flags & SIDH_OUT)
		if ((resid > 0) && (resid < sc->sc_reqlen))
			resid++;
	ntrans = sc->sc_reqlen - resid;

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_stop: resid=0x%x ntrans=0x%x\n",
		       resid, ntrans);
	}
#endif

	if (ntrans < MIN_DMA_LEN) {
		printf("si: fifo count: 0x%x\n", resid);
		ncr_sc->sc_state |= NCR_ABORTING;
		goto out;
	}
	if (ntrans > ncr_sc->sc_datalen)
		panic("si_dma_stop: excess transfer");

	/* Adjust data pointer */
	ncr_sc->sc_dataptr += ntrans;
	ncr_sc->sc_datalen -= ntrans;

	/*
	 * After a read, we may need to clean-up
	 * "Left-over bytes" (yuck!)
	 */
	if (((dh->dh_flags & SIDH_OUT) == 0) &&
		((si->si_csr & SI_CSR_LOB) != 0))
	{
		char *cp = ncr_sc->sc_dataptr;
#ifdef DEBUG
		printf("si: Got Left-over bytes!\n");
#endif
		if (si->si_csr & SI_CSR_BPCON) {
			/* have SI_CSR_BPCON */
			cp[-1] = (si->si_bprl & 0xff00) >> 8;
		} else {
			switch (si->si_csr & SI_CSR_LOB) {
			case SI_CSR_LOB_THREE:
				cp[-3] = (si->si_bprh & 0xff00) >> 8;
				cp[-2] = (si->si_bprh & 0x00ff);
				cp[-1] = (si->si_bprl & 0xff00) >> 8;
				break;
			case SI_CSR_LOB_TWO:
				cp[-2] = (si->si_bprh & 0xff00) >> 8;
				cp[-1] = (si->si_bprh & 0x00ff);
				break;
			case SI_CSR_LOB_ONE:
				cp[-1] = (si->si_bprh & 0xff00) >> 8;
				break;
			}
		}
	}

out:
	si->dma_addrh = 0;
	si->dma_addrl = 0;

	si->dma_counth = 0;
	si->dma_countl = 0;

	si->fifo_cnt_hi = 0;
	si->fifo_count  = 0;

	/* Put SBIC back in PIO mode. */
	*ncr_sc->sci_mode &= ~(SCI_MODE_DMA | SCI_MODE_DMA_IE);
	*ncr_sc->sci_icmd = 0;
}

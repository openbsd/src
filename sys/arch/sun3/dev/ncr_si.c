/*	$NetBSD: ncr_si.c,v 1.1 1995/10/29 21:19:11 gwr Exp $	*/

/*
 * Copyright (c) 1995 David Jones
 * Copyright (c) 1994 Adam Glass, Gordon W. Ross
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
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by
 *      Adam Glass, David Jones and Gordon Ross
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file contains only the machine-dependent parts of the
 * Sun3 SCSI driver.  (Autoconfig stuff and DMA functions.)
 * The machine-independent parts are in ncr5380sbc.c
 */

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/device.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_debug.h>
#include <scsi/scsiconf.h>

#include <machine/autoconf.h>
#include <machine/isr.h>
#include <machine/obio.h>
#include <machine/dvma.h>

#if 0	/* XXX - not yet... */
#include <dev/ic/ncr5380reg.h>
#include <dev/ic/ncr5380var.h>
#else
#include "ncr5380reg.h"
#include "ncr5380var.h"
#endif

#include "ncr_sireg.h"
#include "am9516.h"

/*
 * Transfers smaller than this are done using PIO
 * (on assumption they're not worth DMA overhead)
 */
#define	MIN_DMA_LEN 128

/*
 * Transfers lager than 63K need to be broken up
 * because some of the device counters are 16 bits.
 */
#define	MAX_DMA_LEN 0xFC00

/*
 * How many uS. to delay after touching the am9616 UDC.
 */
#define UDC_WAIT_USEC 5

#define DEBUG XXX

#ifdef	DEBUG
int si_debug = 0;
static int si_flags = 0 /* | SDEV_DB2 */ ;
static int Seq = 0;
static int Nwrite = 0;
struct si_softc *TheSoftC;
volatile struct si_regs *TheRegs;
void log_intr(void);
void log_start(void);
void si_printregs(void);
#endif

/*
 * This structure is used to keep track of mapped DMA requests.
 * Note: combined the UDC command block with this structure, so
 * the array of these has to be in DVMA space.
 */
struct si_dma_handle {
	u_long		dh_addr;	/* KVA of start of buffer */
	int 		dh_len;		/* Original data length */
	u_long		dh_dvma;	/* VA of buffer in DVMA space */
	int 		dh_flags;
#define	SIDH_BUSY	1		/* This DH is in use */
#define	SIDH_OUT	2		/* DMA does data out (write) */
	/* DMA command block for the OBIO controller. */
	struct udc_table dh_cmd;
};

/*
 * The first structure member has to be the ncr5380_softc
 * so we can just cast to go back and fourth between them.
 */
struct si_softc {
	struct ncr5380_softc	ncr;
	volatile struct si_regs	*sc_regs;
	int		sc_adapter_type;
	int		sc_adapter_iv_am; /* int. vec + address modifier */
	int		sc_timo;
	struct si_dma_handle *sc_dma;
};

/*
 * XXX: Note that reselect CAN NOT WORK given the current need
 * to set the damn FIFO count logic during dma_alloc, because
 * the DMA might be need by another target in the mean time.
 * (It works when there is only one target/lun though...)
 */
int si_permit_reselect = 0;	/* XXX: Do not set this yet. */
int si_polled_dma = 0;	/* Set if interrupts don't work */

/* How long to wait for DMA before declaring an error. */
int si_dma_intr_timo = 500;	/* ticks (sec. X 100) */

static char si_name[] = "si";
static int	si_match();
static void	si_attach();
static int	si_intr(void *arg);
static void	si_reset_adapter(struct ncr5380_softc *sc);
static void	si_minphys(struct buf *bp);

void si_dma_alloc __P((struct ncr5380_softc *));
void si_dma_free __P((struct ncr5380_softc *));
void si_dma_poll __P((struct ncr5380_softc *));

void si_vme_dma_start __P((struct ncr5380_softc *));
void si_vme_dma_eop __P((struct ncr5380_softc *));
void si_vme_dma_stop __P((struct ncr5380_softc *));

void si_obio_dma_start __P((struct ncr5380_softc *));
void si_obio_dma_eop __P((struct ncr5380_softc *));
void si_obio_dma_stop __P((struct ncr5380_softc *));


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


struct cfdriver ncr_sicd = {
	NULL, si_name, si_match, si_attach, DV_DULL,
	sizeof(struct si_softc), NULL, 0,
};

static int
si_print(aux, name)
	void *aux;
	char *name;
{
	if (name != NULL)
		printf("%s: scsibus ", name);
	return UNCONF;
}

static int
si_match(parent, vcf, args)
	struct device	*parent;
	void		*vcf, *args;
{
	struct cfdata	*cf = vcf;
	struct confargs *ca = args;
	int x, probe_addr;

	/* Default interrupt priority always splbio==2 */
	if (ca->ca_intpri == -1)
		ca->ca_intpri = 2;

	if ((cpu_machine_id == SUN3_MACH_50) ||
	    (cpu_machine_id == SUN3_MACH_60) )
	{
		/* Sun3/50 or Sun3/60 have only OBIO "si" */
		if (ca->ca_bustype != BUS_OBIO)
			return(0);
		if (ca->ca_paddr == -1)
			ca->ca_paddr = OBIO_NCR_SCSI;
		/* OK... */
	} else {
		/* Other Sun3 models may have VME "si" or "sc" */
		if (ca->ca_bustype != BUS_VME16)
			return (0);
		if (ca->ca_paddr == -1)
			return (0);
		/* OK... */
	}

	/* Make sure there is something there... */
	x = bus_peek(ca->ca_bustype, ca->ca_paddr + 1, 1);
	if (x == -1)
		return (0);

	/*
	 * If this is a VME SCSI board, we have to determine whether
	 * it is an "sc" (Sun2) or "si" (Sun3) SCSI board.  This can
	 * be determined using the fact that the "sc" board occupies
	 * 4K bytes in VME space but the "si" board occupies 2K bytes.
	 */
	if (ca->ca_bustype == BUS_VME16) {
		/* Note, the "si" board should NOT respond here. */
		x = bus_peek(ca->ca_bustype, ca->ca_paddr + 0x801, 1);
		if (x != -1)
			return(0);
	}

    return (1);
}

static void
si_attach(parent, self, args)
	struct device	*parent, *self;
	void		*args;
{
	struct si_softc *sc = (struct si_softc *) self;
	volatile struct si_regs *regs;
	struct confargs *ca = args;
	int i;

	switch (ca->ca_bustype) {

	case BUS_OBIO:
		regs = (struct si_regs *)
			obio_alloc(ca->ca_paddr, sizeof(*regs));
		isr_add_autovect(si_intr, (void *)sc,
						 ca->ca_intpri);
		break;

	case BUS_VME16:
		regs = (struct si_regs *)
			bus_mapin(ca->ca_bustype, ca->ca_paddr, sizeof(*regs));
		isr_add_vectored(si_intr, (void *)sc,
						 ca->ca_intpri, ca->ca_intvec);
		sc->sc_adapter_iv_am =
			VME_SUPV_DATA_24 | (ca->ca_intvec & 0xFF);
		break;

	default:
		printf("unknown\n");
		return;
	}
	printf("\n");

	/*
	 * Fill in the prototype scsi_link.
	 */
	sc->ncr.sc_link.adapter_softc = sc;
	sc->ncr.sc_link.adapter_target = 7;
	sc->ncr.sc_link.adapter = &si_ops;
	sc->ncr.sc_link.device = &si_dev;

	/*
	 * Initialize fields used by the MI code
	 */
	sc->ncr.sci_data = &regs->sci.sci_data;
	sc->ncr.sci_icmd = &regs->sci.sci_icmd;
	sc->ncr.sci_mode = &regs->sci.sci_mode;
	sc->ncr.sci_tcmd = &regs->sci.sci_tcmd;
	sc->ncr.sci_bus_csr = &regs->sci.sci_bus_csr;
	sc->ncr.sci_csr = &regs->sci.sci_csr;
	sc->ncr.sci_idata = &regs->sci.sci_idata;
	sc->ncr.sci_iack = &regs->sci.sci_iack;

	/*
	 * MD function pointers used by the MI code.
	 */
	sc->ncr.sc_pio_out = ncr5380_pio_out;
	sc->ncr.sc_pio_in =  ncr5380_pio_in;
	sc->ncr.sc_dma_alloc = si_dma_alloc;
	sc->ncr.sc_dma_free  = si_dma_free;
	sc->ncr.sc_dma_poll  = si_dma_poll;
	if (ca->ca_bustype == BUS_VME16) {
		sc->ncr.sc_dma_start = si_vme_dma_start;
		sc->ncr.sc_dma_eop   = si_vme_dma_stop;
		sc->ncr.sc_dma_stop  = si_vme_dma_stop;
	} else {
		sc->ncr.sc_dma_start = si_obio_dma_start;
		sc->ncr.sc_dma_eop   = si_obio_dma_stop;
		sc->ncr.sc_dma_stop  = si_obio_dma_stop;
	}
	sc->ncr.sc_flags = (si_permit_reselect) ?
		NCR5380_PERMIT_RESELECT : 0;
	sc->ncr.sc_min_dma_len = MIN_DMA_LEN;

	/*
	 * Initialize fields used only here in the MD code.
	 */
	sc->sc_regs = regs;
	sc->sc_adapter_type = ca->ca_bustype;
	/*  sc_adapter_iv_am = (was set above) */
	sc->sc_timo = 0;	/* no timeout armed. */

	/* Need DVMA-capable memory for the UDC command blocks. */
	i = SCI_OPENINGS * sizeof(struct si_dma_handle);
	sc->sc_dma = (struct si_dma_handle *) dvma_malloc(i);
	if (sc->sc_dma == NULL)
		panic("si: dvma_malloc failed\n");
	for (i = 0; i < SCI_OPENINGS; i++)
		sc->sc_dma[i].dh_flags = 0;

#ifdef	DEBUG
	if (si_debug)
		printf("Set TheSoftC=%x TheRegs=%x\n", sc, regs);
	TheSoftC = sc;
	TheRegs = regs;
	sc->ncr.sc_link.flags |= si_flags;
#endif

	/*
	 *  Initialize si board itself.
	 */
	si_reset_adapter(&sc->ncr);
	ncr5380_init(&sc->ncr);
	ncr5380_reset_scsibus(&sc->ncr);
	config_found(self, &(sc->ncr.sc_link), si_print);
}

static void
si_minphys(struct buf *bp)
{
	if (bp->b_bcount > MAX_DMA_LEN) {
		printf("si_minphys len = %x.\n", MAX_DMA_LEN);
		bp->b_bcount = MAX_DMA_LEN;
	}
	return (minphys(bp));
}


static int
si_intr(void *arg)
{
	struct si_softc *sc = arg;
	volatile struct si_regs *si = sc->sc_regs;
	int claimed, rv = 0;

	/* DMA interrupt? */
	if (si->si_csr & SI_CSR_DMA_IP) {
		rv |= SI_CSR_DMA_IP;
		(*sc->ncr.sc_dma_stop)(&sc->ncr);
	}

	/* SBC interrupt? */
	if (si->si_csr & SI_CSR_SBC_IP) {
		rv |= SI_CSR_SBC_IP;
		claimed = ncr5380_sbc_intr(&sc->ncr);
#ifdef	DEBUG
		if (!claimed) {
			printf("si_intr: spurious from SBC\n");
			if (si_debug & 4) {
				Debugger();	/* XXX */
			}
		}
#endif
	}

	return (rv);
}


static void
si_dma_timeout(arg)
	void *arg;
{
	struct si_softc *sc = arg;
	int s;

	s = splbio();

	sc->sc_timo = 0;

	/* Timeout during DMA transfer? */
	if (sc->ncr.sc_dma_flags & DMA5380_INPROGRESS) {
		sc->ncr.sc_dma_flags |= DMA5380_ERROR;
		printf("si: DMA timeout (resetting)\n");
		si_reset_adapter(&sc->ncr);
		ncr5380_sbc_intr(&sc->ncr);
	}

	splx(s);
}


static void
si_reset_adapter(struct ncr5380_softc *ncr)
{
	struct si_softc *sc = (struct si_softc *)ncr;
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

	SCI_CLR_INTR(ncr);
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
si_dma_alloc(ncr)
	struct ncr5380_softc *ncr;
{
	struct si_softc *sc = (struct si_softc *)ncr;
	volatile struct si_regs *si = sc->sc_regs;
	struct si_dma_handle *dh;
	int i, xlen;
	u_long addr;

#if 1
	/* XXX - In case we don't trust interrupts... */
	if (si_polled_dma)
		sc->ncr.sc_dma_flags |= DMA5380_POLL;
#endif

	addr = (u_long) sc->ncr.sc_dataptr;
	xlen = sc->ncr.sc_datalen;

	/* If the DMA start addr is misaligned then do PIO */
	if ((addr & 1) || (xlen & 1)) {
		printf("si_dma_alloc: misaligned.\n");
		goto no_dma;
	}

	/* Make sure our caller checked sc_min_dma_len. */
	if (xlen < MIN_DMA_LEN)
		panic("si_dma_alloc: xlen=0x%x\n", xlen);

	/*
	 * Never attempt single transfers of more than 63k, because
	 * our count register may be only 16 bits (an OBIO adapter).
	 * This should never happen since already bounded by minphys().
	 */
	if (xlen > MAX_DMA_LEN) {
		printf("si_dma_alloc: excessive xlen=0x%x\n", xlen);
		Debugger();
		xlen = MAX_DMA_LEN;
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
	sc->ncr.sc_dma_hand = dh;
	dh->dh_addr = addr;
	dh->dh_len = xlen;
	dh->dh_flags = SIDH_BUSY;

	/* Copy the "write" flag for convenience. */
	if (sc->ncr.sc_dma_flags & DMA5380_WRITE)
		dh->dh_flags |= SIDH_OUT;

	/*
	 * We don't care about (sc_dma_flags & DMA5380_PHYS)
	 * because we always have to dup mappings anyway.
	 */
	dh->dh_dvma = (u_long) dvma_mapin((char *)addr, xlen);
	if (!dh->dh_dvma) {
		/* Can't remap segment */
		printf("si_dma_alloc: can't remap %x/%x\n",
			dh->dh_addr, dh->dh_len);
		dh->dh_flags = 0;
		goto no_dma;
	}

	/*
	 * Note:  We have to initialize the FIFO logic NOW,
	 * (just after selection, before talking on the bus)
	 * because after this point, (much to my surprise)
	 * writes to the fifo_count register ARE IGNORED!!!
	 */
	si->fifo_count = 0;		/* also hits dma_count */
	if (dh->dh_flags & SIDH_OUT) {
		si->si_csr |= SI_CSR_SEND;
	} else {
		si->si_csr &= ~SI_CSR_SEND;
	}
	si->si_csr &= ~SI_CSR_FIFO_RES; 	/* active low */
	delay(10);
	si->si_csr |= SI_CSR_FIFO_RES;
	delay(10);
	si->fifo_count = xlen;
	if (sc->sc_adapter_type == BUS_VME16)
		si->fifo_cnt_hi = 0;

#ifdef	DEBUG
	if ((si->fifo_count > xlen) || (si->fifo_count < (xlen - 1))) {
		printf("si_dma_alloc: fifo_count=0x%x, xlen=0x%x\n",
			   si->fifo_count, xlen);
		Debugger();
	}
#endif

	return;	/* success */

no_dma:
	sc->ncr.sc_dma_hand = NULL;
}


void
si_dma_free(ncr)
	struct ncr5380_softc *ncr;
{
	struct si_dma_handle *dh = ncr->sc_dma_hand;

	if (ncr->sc_dma_flags & DMA5380_INPROGRESS)
		panic("si_dma_free: free while in progress");

	if (dh->dh_flags & SIDH_BUSY) {
		/* XXX - Should separate allocation and mapping. */
		/* Give back the DVMA space. */
		dvma_mapout((caddr_t)dh->dh_dvma, dh->dh_len);
		dh->dh_dvma = 0;
		dh->dh_flags = 0;
	}
}


/*
 * Poll (spin-wait) for DMA completion.
 * Same for either VME or OBIO.
 */
void
si_dma_poll(ncr)
	struct ncr5380_softc *ncr;
{
	struct si_softc *sc = (struct si_softc *)ncr;
	volatile struct si_regs *si = sc->sc_regs;
	int tmo, csr_mask;

	/* Make sure the DMA actually started... */
	/* XXX - Check DMA5380_INPROGRESS instead? */
	if (sc->ncr.sc_dma_flags & DMA5380_ERROR)
		return;

	csr_mask = SI_CSR_SBC_IP | SI_CSR_DMA_IP |
		SI_CSR_DMA_CONFLICT | SI_CSR_DMA_BUS_ERR;

	tmo = 50000;	/* X100 = 5 sec. */
	for (;;) {
		if (si->si_csr & csr_mask)
			break;
		if (--tmo <= 0) {
			printf("si: DMA timeout\n");
			sc->ncr.sc_dma_flags |= DMA5380_ERROR;
			si_reset_adapter(&sc->ncr);
			break;
		}
		delay(100);
	}

#ifdef	DEBUG
	if (si_debug) {
		printf("si_dma_poll: done, csr=0x%x\n", si->si_csr);
	}
#endif
}


/*****************************************************************
 * VME functions for DMA
 ****************************************************************/


void
si_vme_dma_start(ncr)
	struct ncr5380_softc *ncr;
{
	struct si_softc *sc = (struct si_softc *)ncr;
	struct si_dma_handle *dh = sc->ncr.sc_dma_hand;
	volatile struct si_regs *si = sc->sc_regs;
	u_long data_pa;

	/*
	 * Get the DVMA mapping for this segment.
	 * XXX - Should separate allocation and mapin.
	 */
	data_pa = dvma_kvtopa((long)dh->dh_dvma, sc->sc_adapter_type) +
		((u_long)sc->ncr.sc_dataptr - dh->dh_addr);
	if (data_pa & 1)
		panic("si_dma_start: bad pa=0x%x", data_pa);

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_start: dh=0x%x, pa=0x%x, xlen=%d\n",
			   dh, data_pa, dh->dh_len);
	}
#endif

	/* Already setup FIFO in si_dma_alloc() */

#ifdef	DEBUG
	if ((si->fifo_count > dh->dh_len) ||
		(si->fifo_count < (dh->dh_len - 1)))
	{
		printf("si_dma_start: fifo_count=0x%x, xlen=0x%x\n",
			   si->fifo_count, dh->dh_len);
		Debugger();
	}
#endif

	/*
	 * Set up the DMA controller.
	 * Note that (dh-dh_len < sc_datalen)
	 */
	if (data_pa & 2) {
		si->si_csr |= SI_CSR_BPCON;
	} else {
		si->si_csr &= ~SI_CSR_BPCON;
	}
	si->dma_addrh = data_pa >> 16;
	si->dma_addrl = data_pa & 0xFFFF;
	si->dma_counth = dh->dh_len >> 16;
	si->dma_countl = dh->dh_len & 0xFFFF;

#if 0	/* XXX: Whack the FIFO again? */
	si->si_csr &= ~SI_CSR_FIFO_RES;
	delay(10);
	si->si_csr |= SI_CSR_FIFO_RES;
	delay(10);
#endif

	/*
	 * Put the SBIC into DMA mode and start the transfer.
	 */
	*sc->ncr.sci_mode |= (SCI_MODE_DMA | SCI_MODE_DMA_IE);
	if (dh->dh_flags & SIDH_OUT) {
		*sc->ncr.sci_icmd = SCI_ICMD_DATA;
		*sc->ncr.sci_dma_send = 0;	/* start it */
	} else {
		*sc->ncr.sci_icmd = 0;
		*sc->ncr.sci_irecv = 0;	/* start it */
	}
	si->si_csr |= SI_CSR_DMA_EN;	/* vme only */

	sc->ncr.sc_dma_flags |= DMA5380_INPROGRESS;

	if ((sc->ncr.sc_dma_flags & DMA5380_POLL) == 0) {
		/* Expect an interrupt when DMA completes. */
		sc->sc_timo = si_dma_intr_timo;
		timeout(si_dma_timeout, sc, sc->sc_timo);
	}

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_start: started, flags=0x%x\n",
			   sc->ncr.sc_dma_flags);
	}
#endif
}


void
si_vme_dma_eop(ncr)
	struct ncr5380_softc *ncr;
{

	/* Not needed - DMA was stopped prior to examining sci_csr */
}


void
si_vme_dma_stop(ncr)
	struct ncr5380_softc *ncr;
{
	struct si_softc *sc = (struct si_softc *)ncr;
	struct si_dma_handle *dh = sc->ncr.sc_dma_hand;
	volatile struct si_regs *si = sc->sc_regs;
	int resid, ntrans, si_csr;

	if ((sc->ncr.sc_dma_flags & DMA5380_INPROGRESS) == 0) {
#ifdef	DEBUG
		printf("si_dma_stop: dma not running\n");
#endif
		return;
	}
	sc->ncr.sc_dma_flags &= ~DMA5380_INPROGRESS;
	if (sc->sc_timo) {
		sc->sc_timo = 0;
		untimeout(si_dma_timeout, sc);
	}

#ifdef DEBUG
	log_intr();
#endif

	/* First, save csr bits and halt DMA. */
	si_csr = si->si_csr;
	si_csr &= ~SI_CSR_DMA_EN;	/* VME only */
	si->si_csr = si_csr;

	if (si_csr & (SI_CSR_DMA_CONFLICT | SI_CSR_DMA_BUS_ERR)) {
		printf("si: DMA error, csr=0x%x, reset\n", si_csr);
		sc->ncr.sc_dma_flags |= DMA5380_ERROR;
		si_reset_adapter(&sc->ncr);
	}

	/* Note that timeout may have set the error flag. */
	if (sc->ncr.sc_dma_flags & DMA5380_ERROR)
		goto out;

	resid = si->fifo_count;
#ifdef	DEBUG
	if (resid != 0) {
		printf("si_dma_stop: fifo resid=%d (ok?)\n", resid);
		Debugger();
	}
#endif
	/* XXX - Was getting (resid==-1), fixed now. */
	if (resid & ~3) {
		printf("si: fifo count: 0x%x\n", resid);
		sc->ncr.sc_dma_flags |= DMA5380_ERROR;
		goto out;
	}

	/* Adjust data pointer */
	ntrans = dh->dh_len - resid;
	sc->ncr.sc_dataptr += ntrans;
	sc->ncr.sc_datalen -= ntrans;

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_stop: ntrans=0x%x\n", ntrans);
	}
#endif

	/*
	 * After a read, we may need to clean-up
	 * "Left-over bytes" (yuck!)
	 */
	if (((dh->dh_flags & SIDH_OUT) == 0) &&
		((si->si_csr & SI_CSR_LOB) != 0))
	{
		char *cp = sc->ncr.sc_dataptr;
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

	/* Put SBIC back in PIO mode. */
	*sc->ncr.sci_mode &= ~(SCI_MODE_DMA | SCI_MODE_DMA_IE);
	*sc->ncr.sci_icmd = 0;
}


/*****************************************************************
 * OBIO functions for DMA
 ****************************************************************/


static __inline__ void
si_obio_udc_write(si, regnum, value)
	volatile struct si_regs *si;
	int regnum, value;
{
	delay(UDC_WAIT_USEC);
	si->udc_addr = regnum;
	delay(UDC_WAIT_USEC);
	si->udc_data = value;
}

static __inline__ int
si_obio_udc_read(si, regnum)
	volatile struct si_regs *si;
	int regnum;
{
	delay(UDC_WAIT_USEC);
	si->udc_addr = regnum;
	delay(UDC_WAIT_USEC);
	return (si->udc_data);
}


void
si_obio_dma_start(ncr)
	struct ncr5380_softc *ncr;
{
	struct si_softc *sc = (struct si_softc *)ncr;
	struct si_dma_handle *dh = sc->ncr.sc_dma_hand;
	volatile struct si_regs *si = sc->sc_regs;
	struct udc_table *cmd;
	u_long data_pa, cmd_pa;

	/*
	 * Get the DVMA mapping for this segment.
	 * XXX - Should separate allocation and mapin.
	 */
	data_pa = dvma_kvtopa((long)dh->dh_dvma, sc->sc_adapter_type) +
		((u_long)sc->ncr.sc_dataptr - dh->dh_addr);
	if (data_pa & 1)
		panic("si_dma_start: bad pa=0x%x", data_pa);
	if (dh->dh_len & 1)
		panic("si_dma_start: bad len=0x%x", dh->dh_len);

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_start: dh=0x%x, pa=0x%x, xlen=%d\n",
			   dh, data_pa, dh->dh_len);
	}
#endif

	/* Already setup FIFO in si_dma_alloc() */

#ifdef	DEBUG
	if ((si->fifo_count > dh->dh_len) ||
		(si->fifo_count < (dh->dh_len - 1)))
	{
		printf("si_dma_start: fifo_count=0x%x, xlen=0x%x\n",
			   si->fifo_count, dh->dh_len);
		Debugger();
	}
#endif

	/*
	 * Set up the DMA controller.
	 * Note that (dh-dh_len < sc_datalen)
	 */

	/*
	 * The OBIO controller needs a command block.
	 */
	cmd = &dh->dh_cmd;
	cmd->addrh = ((data_pa & 0xFF0000) >> 8) | UDC_ADDR_INFO;
	cmd->addrl = data_pa & 0xFFFF;
	cmd->count = dh->dh_len / 2;	/* bytes -> words */
	cmd->cmrh = UDC_CMR_HIGH;
	if (dh->dh_flags & SIDH_OUT) {
		cmd->cmrl = UDC_CMR_LSEND;
		cmd->rsel = UDC_RSEL_SEND;
	} else {
		cmd->cmrl = UDC_CMR_LRECV;
		cmd->rsel = UDC_RSEL_RECV;
	}

	/* Tell the DMA chip where the control block is. */
	cmd_pa = dvma_kvtopa((long)cmd, BUS_OBIO);
	si_obio_udc_write(si, UDC_ADR_CAR_HIGH,
					  (cmd_pa & 0xff0000) >> 8);
	si_obio_udc_write(si, UDC_ADR_CAR_LOW,
					  (cmd_pa & 0xffff));

	/* Tell the chip to be a DMA master. */
	si_obio_udc_write(si, UDC_ADR_MODE, UDC_MODE);

	/* Tell the chip to interrupt on error. */
	si_obio_udc_write(si, UDC_ADR_COMMAND, UDC_CMD_CIE);

	/* Finally, give the UDC a "start chain" command. */
	si_obio_udc_write(si, UDC_ADR_COMMAND, UDC_CMD_STRT_CHN);

	/*
	 * Put the SBIC into DMA mode and start the transfer.
	 */
	*sc->ncr.sci_mode |= (SCI_MODE_DMA | SCI_MODE_DMA_IE);
	if (dh->dh_flags & SIDH_OUT) {
		*sc->ncr.sci_icmd = SCI_ICMD_DATA;
		*sc->ncr.sci_dma_send = 0;	/* start it */
	} else {
		*sc->ncr.sci_icmd = 0;
		*sc->ncr.sci_irecv = 0;	/* start it */
	}

	sc->ncr.sc_dma_flags |= DMA5380_INPROGRESS;

	if ((sc->ncr.sc_dma_flags & DMA5380_POLL) == 0) {
		/* Expect an interrupt when DMA completes. */
		sc->sc_timo = si_dma_intr_timo;
		timeout(si_dma_timeout, sc, sc->sc_timo);
	}

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_start: started, flags=0x%x\n",
			   sc->ncr.sc_dma_flags);
	}
#endif
}


void
si_obio_dma_eop(ncr)
	struct ncr5380_softc *ncr;
{

	/* Not needed - DMA was stopped prior to examining sci_csr */
}


void
si_obio_dma_stop(ncr)
	struct ncr5380_softc *ncr;
{
	struct si_softc *sc = (struct si_softc *)ncr;
	struct si_dma_handle *dh = sc->ncr.sc_dma_hand;
	volatile struct si_regs *si = sc->sc_regs;
	int resid, ntrans, tmo, udc_cnt;

	if ((sc->ncr.sc_dma_flags & DMA5380_INPROGRESS) == 0) {
#ifdef	DEBUG
		printf("si_dma_stop: dma not running\n");
#endif
		return;
	}
	sc->ncr.sc_dma_flags &= ~DMA5380_INPROGRESS;
	if (sc->sc_timo) {
		sc->sc_timo = 0;
		untimeout(si_dma_timeout, sc);
	}

	if (si->si_csr & (SI_CSR_DMA_CONFLICT | SI_CSR_DMA_BUS_ERR)) {
		printf("si: DMA error, csr=0x%x, reset\n", si->si_csr);
		sc->ncr.sc_dma_flags |= DMA5380_ERROR;
		si_reset_adapter(&sc->ncr);
	}

	/* Note that timeout may have set the error flag. */
	if (sc->ncr.sc_dma_flags & DMA5380_ERROR)
		goto out;

	/* After a read, wait for the FIFO to empty. */
	if ((dh->dh_flags & SIDH_OUT) == 0) {
		/* XXX: This bit is not reliable.  Beware! -dej */
		tmo = 200000;	/* X10 = 2 sec. */
		for (;;) {
			if (si->si_csr & SI_CSR_FIFO_EMPTY)
				break;
			if (--tmo <= 0) {
				printf("si: dma fifo did not empty, reset\n");
				sc->ncr.sc_dma_flags |= DMA5380_ERROR;
				si_reset_adapter(&sc->ncr);
				goto out;
			}
			delay(10);
		}
	}


	resid = si->fifo_count;
#ifdef	DEBUG
	if (resid != 0) {
		printf("si_dma_stop: fifo resid=%d (ok?)\n", resid);
		Debugger();
	}
#endif
	/* XXX - Was getting (resid==-1), fixed now. */
	if (resid & ~3) {
		printf("si: fifo count: 0x%x\n", resid);
		sc->ncr.sc_dma_flags |= DMA5380_ERROR;
		goto out;
	}

	/* Adjust data pointer */
	ntrans = dh->dh_len - resid;
	sc->ncr.sc_dataptr += ntrans;
	sc->ncr.sc_datalen -= ntrans;

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_stop: ntrans=0x%x\n", ntrans);
	}
#endif

	/*
	 * After a read, we may need to clean-up
	 * "Left-over bytes" (yuck!)
	 */
	if ((dh->dh_flags & SIDH_OUT) == 0) {
		/* If odd transfer count, grab last byte by hand. */
		if (ntrans & 1) {
			sc->ncr.sc_dataptr[-1] =
				(si->fifo_data & 0xff00) >> 8;
			goto out;
		}
		/* UDC might not have transfered the last word. */
		udc_cnt = si_obio_udc_read(si, UDC_ADR_COUNT);
		if (((udc_cnt * 2) - si->fifo_count) == 2) {
			sc->ncr.sc_dataptr[-2] =
				(si->fifo_data & 0xff00) >> 8;
			sc->ncr.sc_dataptr[-1] =
				(si->fifo_data & 0x00ff);
		}
	}

out:
	/* Reset the UDC. */
	si_obio_udc_write(si, UDC_ADR_COMMAND, UDC_CMD_RESET);

	/* Put SBIC back in PIO mode. */
	*sc->ncr.sci_mode &= ~(SCI_MODE_DMA | SCI_MODE_DMA_IE);
	*sc->ncr.sci_icmd = 0;
}


#if 0
int hexdigit(char c)
{

	if (c >= '0' && c <= '9') return c - '0';
	else return c - ('a' - 10);
}


struct scsi_link *thescsilink;

void sicmdloop(void)
{
char hexbuf[40];
int c, i, pos;
u_char cmdbuf[6];

	while (1) {
		pos = 0;
		while (1) {
			c = cngetc();
			if ((c == 0x7F || c == 0x08) && pos) {
				pos--;
				cnputc(0x08);
				cnputc(' ');
				cnputc(0x08);
			}
			else if (c == '\r' || c == '\n') {
				hexbuf[pos] = 0;
				break;
			}
			else {
				hexbuf[pos++] = c;
				cnputc(c);
			}
		}

		pos = 0;
		for (i = 0; i < 6; i++) {
		    while (hexbuf[pos] == ' ') pos++;
		    cmdbuf[i] = 16 * hexdigit(hexbuf[pos++]) + hexdigit(hexbuf[pos++]);
		}

		scsi_scsi_cmd(thescsilink, (struct scsi_generic *)cmdbuf, 6,
			0, 0, 1, 1000, 0, SCSI_POLL);
	}
}
#endif


#ifdef DEBUG

#define NQENT 10
static int Qptr = 0;
static struct qent {
    int seq;
    int si_csr;
    int sci_csr;
    int sci_bus_csr;
    long dma_addr, dma_count, fifo_len;
    long start_addr, start_count, start_len;
} Qents[NQENT];


void log_start(void)
{
struct si_softc *sc = TheSoftC;
volatile struct si_regs *si = sc->sc_regs;

    Qents[Qptr].start_addr = (si->dma_addrh << 16) + si->dma_addrl;
    Qents[Qptr].start_count = (si->dma_counth << 16) + si->dma_countl;
    Qents[Qptr].start_len = si->fifo_count;
    Qents[Qptr].seq = Seq++;
}


void log_intr(void)
{
struct si_softc *sc = TheSoftC;
volatile struct si_regs *si = sc->sc_regs;

    Qents[Qptr].si_csr = si->si_csr;
    while (!(si->si_csr & SI_CSR_FIFO_EMPTY)) {
	printf("log_intr: FIFO not empty before DMA disable at %d\n", Seq);
    }

    if (sc->sc_adapter_type == BUS_VME16)
	    si->si_csr &= ~SI_CSR_DMA_EN;
    if (!(si->si_csr & SI_CSR_FIFO_EMPTY)) {
	printf("log_intr: FIFO not empty after DMA disable at %d\n", Seq);
    }
    Qents[Qptr].dma_addr = (si->dma_addrh << 16) + si->dma_addrl;

    Qents[Qptr].dma_count = (si->dma_counth << 16) + si->dma_countl;
    Qents[Qptr].fifo_len = si->fifo_count;
    Qents[Qptr].sci_csr = *sc->ncr.sci_csr;
    Qents[Qptr].sci_bus_csr = *sc->ncr.sci_bus_csr;
    Qptr++;
    if (Qptr == NQENT) Qptr = 0;
}


void si_printregs(void)
{
struct si_softc *sc = TheSoftC;
volatile struct si_regs *si = TheRegs;
struct sci_req *sr;
int i;
int a, b, c, d;

    if (!TheRegs) {
	printf("TheRegs == NULL, please re-set!\n");
	return;
    }

    c = si->si_csr;
    printf("si->si_csr=%04x\n", c);
    si->si_csr &= ~SI_CSR_DMA_EN;

    a = si->dma_addrh; b = si->dma_addrl;
    printf("si->dma_addr=%04x%04x\n", a, b);

    /* printf("si->dma_addr=%04x%04x\n", si->dma_addrh, si->dma_addrl); */
    printf("si->dma_count=%04x%04x\n", si->dma_counth, si->dma_countl);
    printf("si->fifo_count=%04x\n", si->fifo_count);
    printf("sci_icmd=%02x\n", si->sci.sci_icmd);
    printf("sci_mode=%02x\n", si->sci.sci_mode);
    printf("sci_tcmd=%02x\n", si->sci.sci_tcmd);
    printf("sci_bus_csr=%02x\n", si->sci.sci_bus_csr);
    printf("sci_csr=%02x\n", si->sci.sci_csr);
    printf("sci_data=%02x\n\n", si->sci.sci_data);

    if (!TheSoftC) {
	printf("TheSoftC == NULL, can't continue.\n");
	return;
    }

    printf("DMA handles:\n");
    for (i = 0; i < SCI_OPENINGS; i++) {
	if (sc->sc_dma[i].dh_flags & SIDH_BUSY) {
	    printf("%d: %x/%x => %x/%d %s\n", i,
		sc->sc_dma[i].dh_addr,
		sc->sc_dma[i].dh_len,
		sc->sc_dma[i].dh_dvma,
		sc->sc_dma[i].dh_flags,
		(&sc->sc_dma[i] == sc->ncr.sc_dma_hand) ? "(active)" : "");
	}
	else {
	    printf("%d: idle\n", i);
	}
    }

    printf("i\nsci_req queue:\n");
    for (i = 0; i < SCI_OPENINGS; i++) {
	sr = &sc->ncr.sc_ring[i];
	printf("%d: %d/%d %x/%x => %x\n", i, sr->sr_target, sr->sr_lun,
	    sr->sr_data, sr->sr_datalen,
	    sr->sr_dma_hand);
    }
    printf("Total commands (sc_ncmds): %d\n", sc->ncr.sc_ncmds);
    printf("\nCurrent SCSI data pointer: %x/%x\n", sc->ncr.sc_dataptr,
	sc->ncr.sc_datalen);
}

void print_qent(void)
{
int i;

    i = Qptr;
    do {
	printf("%d: si_csr=%04x csr=%02x bus_csr=%02x addr=%08x count=%08x fifo=%08x\n",
	    Qents[i].seq, Qents[i].si_csr, Qents[i].sci_csr, Qents[i].sci_bus_csr,
	    Qents[i].dma_addr, Qents[i].dma_count, Qents[i].fifo_len);
	printf("    from addr=%08x count=%08x fifo=%08x\n",
	    Qents[i].start_addr, Qents[i].start_count, Qents[i].start_len);
	i++;
	if (i == NQENT) i = 0;
    } while (i != Qptr);
}

#endif

/*	$NetBSD: ncr_si.c,v 1.3 1996/01/01 22:51:26 thorpej Exp $	*/

/*
 * Copyright (c) 1995 David Jones, Gordon W. Ross
 * Copyright (c) 1994 Adam Glass
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
 *      Adam Glass, David Jones, and Gordon Ross
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
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

#include "ncr_sireg.h"
#include "am9516.h"

/*
 * Transfers smaller than this are done using PIO
 * (on assumption they're not worth DMA overhead)
 */
#define	MIN_DMA_LEN 128

/*
 * Transfers lager than 65535 bytes need to be split-up.
 * (Some of the FIFO logic has only 16 bits counters.)
 * Make the size an integer multiple of the page size
 * to avoid buf/cluster remap problems.  (paranoid?)
 */
#define	MAX_DMA_LEN 0xE000

/*
 * How many uS. to delay after touching the am9516 UDC.
 */
#define UDC_WAIT_USEC 5

#ifdef	DEBUG
int si_debug = 0;
static int si_link_flags = 0 /* | SDEV_DB2 */ ;
#endif

/*
 * This structure is used to keep track of mapped DMA requests.
 * Note: combined the UDC command block with this structure, so
 * the array of these has to be in DVMA space.
 */
struct si_dma_handle {
	int 		dh_flags;
#define	SIDH_BUSY	1		/* This DH is in use */
#define	SIDH_OUT	2		/* DMA does data out (write) */
	u_char *	dh_addr;	/* KVA of start of buffer */
	int 		dh_maplen;	/* Length of KVA mapping. */
	long		dh_dvma;	/* VA of buffer in DVMA space */
	/* DMA command block for the OBIO controller. */
	struct udc_table dh_cmd;
};

/*
 * The first structure member has to be the ncr5380_softc
 * so we can just cast to go back and fourth between them.
 */
struct si_softc {
	struct ncr5380_softc	ncr_sc;
	volatile struct si_regs	*sc_regs;
	int		sc_adapter_type;
	int		sc_adapter_iv_am; /* int. vec + address modifier */
	struct si_dma_handle *sc_dma;
	int 	sc_xlen;		/* length of current DMA segment. */
};

/* Options.  Interesting values are: 1,3,7 */
int si_options = 0;
#define SI_ENABLE_DMA	1	/* Use DMA (maybe polled) */
#define SI_DMA_INTR 	2	/* DMA completion interrupts */
#define	SI_DO_RESELECT	4	/* Allow disconnect/reselect */

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

void si_vme_dma_setup __P((struct ncr5380_softc *));
void si_vme_dma_start __P((struct ncr5380_softc *));
void si_vme_dma_eop __P((struct ncr5380_softc *));
void si_vme_dma_stop __P((struct ncr5380_softc *));

void si_vme_intr_on  __P((struct ncr5380_softc *));
void si_vme_intr_off __P((struct ncr5380_softc *));

void si_obio_dma_setup __P((struct ncr5380_softc *));
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
	NULL, si_name, si_match, si_attach,
	DV_DULL, sizeof(struct si_softc), NULL, 0,
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
	struct ncr5380_softc *ncr_sc = (struct ncr5380_softc *)sc;
	volatile struct si_regs *regs;
	struct confargs *ca = args;
	int i;

	switch (ca->ca_bustype) {

	case BUS_OBIO:
		regs = (struct si_regs *)
			obio_alloc(ca->ca_paddr, sizeof(*regs));
		break;

	case BUS_VME16:
		regs = (struct si_regs *)
			bus_mapin(ca->ca_bustype, ca->ca_paddr, sizeof(*regs));
		break;

	default:
		printf("unknown\n");
		return;
	}
	printf("\n");

	/*
	 * Fill in the prototype scsi_link.
	 */
	ncr_sc->sc_link.adapter_softc = sc;
	ncr_sc->sc_link.adapter_target = 7;
	ncr_sc->sc_link.adapter = &si_ops;
	ncr_sc->sc_link.device = &si_dev;

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
	 * MD function pointers used by the MI code.
	 */
	ncr_sc->sc_pio_out = ncr5380_pio_out;
	ncr_sc->sc_pio_in =  ncr5380_pio_in;
	ncr_sc->sc_dma_alloc = si_dma_alloc;
	ncr_sc->sc_dma_free  = si_dma_free;
	ncr_sc->sc_dma_poll  = si_dma_poll;
	ncr_sc->sc_intr_on   = NULL;
	ncr_sc->sc_intr_off  = NULL;
	if (ca->ca_bustype == BUS_VME16) {
		ncr_sc->sc_dma_setup = si_vme_dma_setup;
		ncr_sc->sc_dma_start = si_vme_dma_start;
		ncr_sc->sc_dma_eop   = si_vme_dma_stop;
		ncr_sc->sc_dma_stop  = si_vme_dma_stop;
		if (si_options & SI_DO_RESELECT) {
			/*
			 * Need to enable interrupts (and DMA!)
			 * on this H/W for reselect to work.
			 */
			ncr_sc->sc_intr_on   = si_vme_intr_on;
			ncr_sc->sc_intr_off  = si_vme_intr_off;
		}
	} else {
		ncr_sc->sc_dma_setup = si_obio_dma_setup;
		ncr_sc->sc_dma_start = si_obio_dma_start;
		ncr_sc->sc_dma_eop   = si_obio_dma_stop;
		ncr_sc->sc_dma_stop  = si_obio_dma_stop;
	}
	ncr_sc->sc_flags = 0;
	if (si_options & SI_DO_RESELECT)
		ncr_sc->sc_flags |= NCR5380_PERMIT_RESELECT;
	if ((si_options & SI_DMA_INTR) == 0)
		ncr_sc->sc_flags |= NCR5380_FORCE_POLLING;
	ncr_sc->sc_min_dma_len = MIN_DMA_LEN;

	/*
	 * Initialize fields used only here in the MD code.
	 */

	/* Need DVMA-capable memory for the UDC command blocks. */
	i = SCI_OPENINGS * sizeof(struct si_dma_handle);
	sc->sc_dma = (struct si_dma_handle *) dvma_malloc(i);
	if (sc->sc_dma == NULL)
		panic("si: dvma_malloc failed\n");
	for (i = 0; i < SCI_OPENINGS; i++)
		sc->sc_dma[i].dh_flags = 0;

	sc->sc_regs = regs;
	sc->sc_adapter_type = ca->ca_bustype;

	/* Now ready for interrupts. */
	if (ca->ca_bustype == BUS_OBIO) {
		isr_add_autovect(si_intr, (void *)sc,
		                 ca->ca_intpri);
	} else {
		isr_add_vectored(si_intr, (void *)sc,
		                 ca->ca_intpri, ca->ca_intvec);
		sc->sc_adapter_iv_am =
			VME_SUPV_DATA_24 | (ca->ca_intvec & 0xFF);
	}

#ifdef	DEBUG
	if (si_debug)
		printf("si: Set TheSoftC=%x TheRegs=%x\n", sc, regs);
	ncr_sc->sc_link.flags |= si_link_flags;
#endif

	/*
	 *  Initialize si board itself.
	 */
	si_reset_adapter(ncr_sc);
	ncr5380_init(ncr_sc);
	ncr5380_reset_scsibus(ncr_sc);
	config_found(self, &(ncr_sc->sc_link), si_print);
}

static void
si_minphys(struct buf *bp)
{
	if (bp->b_bcount > MAX_DMA_LEN) {
#ifdef	DEBUG
		if (si_debug) {
			printf("si_minphys len = 0x%x.\n", bp->b_bcount);
			Debugger();
		}
#endif
		bp->b_bcount = MAX_DMA_LEN;
	}
	return (minphys(bp));
}


#define CSR_WANT (SI_CSR_SBC_IP | SI_CSR_DMA_IP | \
	SI_CSR_DMA_CONFLICT | SI_CSR_DMA_BUS_ERR )

static int
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


static void
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

#if 1	/* XXX - Temporary */
	/* XXX - In case we think DMA is completely broken... */
	if ((si_options & SI_ENABLE_DMA) == 0)
		return;
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
		panic("si_dma_alloc: xlen=0x%x\n", xlen);

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
		printf("si_dma_alloc: can't remap %x/%x\n",
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
	struct si_dma_handle *dh = sr->sr_dma_hand;
	volatile struct si_regs *si = sc->sc_regs;
	int tmo, csr_mask;

	/* Make sure DMA started successfully. */
	if (ncr_sc->sc_state & NCR_ABORTING)
		return;

	csr_mask = SI_CSR_SBC_IP | SI_CSR_DMA_IP |
		SI_CSR_DMA_CONFLICT | SI_CSR_DMA_BUS_ERR;

	tmo = 50000;	/* X100 = 5 sec. */
	for (;;) {
		if (si->si_csr & csr_mask)
			break;
		if (--tmo <= 0) {
			printf("si: DMA timeout (while polling)\n");
			/* Indicate timeout as MI code would. */
			sr->sr_flags |= SR_OVERDUE;
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

	si_vme_dma_setup(ncr_sc);
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
 * On the VME version we just clear the DMA count and address
 * here (to make sure it stays idle) and do the real setup
 * later, in dma_start.
 */
void
si_vme_dma_setup(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	volatile struct si_regs *si = sc->sc_regs;

	/* Reset the FIFO */
	si->si_csr &= ~SI_CSR_FIFO_RES; 	/* active low */
	si->si_csr |= SI_CSR_FIFO_RES;

	/* Set direction (assume recv here) */
	si->si_csr &= ~SI_CSR_SEND;
	/* Assume worst alignment */
	si->si_csr |= SI_CSR_BPCON;

	si->dma_addrh = 0;
	si->dma_addrl = 0;

	si->dma_counth = 0;
	si->dma_countl = 0;

	/* Clear FIFO counter. (also hits dma_count) */
	si->fifo_cnt_hi = 0;
	si->fifo_count = 0;		
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
	xlen &= ~1;
	sc->sc_xlen = xlen; 	/* XXX: or less... */

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_start: dh=0x%x, pa=0x%x, xlen=%d\n",
			   dh, data_pa, xlen);
	}
#endif

	/*
	 * Set up the DMA controller.
	 */
	si->si_csr &= ~SI_CSR_FIFO_RES; 	/* active low */
	si->si_csr |= SI_CSR_FIFO_RES;

	/* Set direction (send/recv) */
	if (dh->dh_flags & SIDH_OUT) {
		si->si_csr |= SI_CSR_SEND;
	} else {
		si->si_csr &= ~SI_CSR_SEND;
	}

	if (data_pa & 2) {
		si->si_csr |= SI_CSR_BPCON;
	} else {
		si->si_csr &= ~SI_CSR_BPCON;
	}

	si->dma_addrh = (ushort)(data_pa >> 16);
	si->dma_addrl = (ushort)(data_pa & 0xFFFF);

	si->dma_counth = (ushort)(xlen >> 16);
	si->dma_countl = (ushort)(xlen & 0xFFFF);

#if 1
	/* Set it anyway, even though dma_count hits it? */
	si->fifo_cnt_hi = (ushort)(xlen >> 16);
	si->fifo_count  = (ushort)(xlen & 0xFFFF);
#endif

#ifdef	DEBUG
	if (si->fifo_count != xlen) {
		printf("si_dma_start: fifo_count=0x%x, xlen=0x%x\n",
			   si->fifo_count, xlen);
		Debugger();
	}
#endif

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

	if (si->si_csr & (SI_CSR_DMA_CONFLICT | SI_CSR_DMA_BUS_ERR)) {
		printf("si: DMA error, csr=0x%x, reset\n", si->si_csr);
		sr->sr_xs->error = XS_DRIVER_STUFFUP;
		ncr_sc->sc_state |= NCR_ABORTING;
		si_reset_adapter(ncr_sc);
	}

	/* Note that timeout may have set the error flag. */
	if (ncr_sc->sc_state & NCR_ABORTING)
		goto out;

	/*
	 * Now try to figure out how much actually transferred
	 *
	 * The fifo_count does not reflect how many bytes were
	 * actually transferred for VME.
	 *
	 * SCSI-3 VME interface is a little funny on writes:
	 * if we have a disconnect, the dma has overshot by
	 * one byte and needs to be incremented.  This is
	 * true if we have not transferred either all data
	 * or no data.  XXX - from Matt Jacob
	 */

	resid = si->fifo_count & 0xFFFF;
	ntrans = sc->sc_xlen - resid;

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


/*
 * This function is called during the COMMAND or MSG_IN phase
 * that preceeds a DATA_IN or DATA_OUT phase, in case we need
 * to setup the DMA engine before the bus enters a DATA phase.
 *
 * The OBIO "si" IGNORES any attempt to set the FIFO count
 * register after the SCSI bus goes into any DATA phase, so
 * this function has to setup the evil FIFO logic.
 */
void
si_obio_dma_setup(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	volatile struct si_regs *si = sc->sc_regs;
	struct sci_req *sr;
	struct si_dma_handle *dh;
	int send = 0;
	int xlen = 0;

	/* Let this work even without a dma hand, for testing... */
	if ((sr = ncr_sc->sc_current) != NULL) {
		if ((dh = sr->sr_dma_hand) != NULL) {
			send = dh->dh_flags & SIDH_OUT;
			xlen = ncr_sc->sc_datalen;
			xlen &= ~1;
		}
	}

#ifdef	DEBUG
	if (si_debug) {
		printf("si_dma_setup: send=%d xlen=%d\n", send, xlen);
	}
#endif

	/* Reset the FIFO */
	si->si_csr &= ~SI_CSR_FIFO_RES; 	/* active low */
	si->si_csr |= SI_CSR_FIFO_RES;

	/* Set direction (send/recv) */
	if (send) {
		si->si_csr |= SI_CSR_SEND;
	} else {
		si->si_csr &= ~SI_CSR_SEND;
	}

	/* Set the FIFO counter. */
	si->fifo_count = xlen;

#ifdef	DEBUG
	if ((si->fifo_count > xlen) || (si->fifo_count < (xlen - 1))) {
		printf("si_dma_setup: fifo_count=0x%x, xlen=0x%x\n",
			   si->fifo_count, xlen);
		Debugger();
	}
#endif
}


void
si_obio_dma_start(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	struct si_dma_handle *dh = sr->sr_dma_hand;
	volatile struct si_regs *si = sc->sc_regs;
	struct udc_table *cmd;
	long data_pa, cmd_pa;
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
	xlen &= ~1;
	sc->sc_xlen = xlen; 	/* XXX: or less... */

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_start: dh=0x%x, pa=0x%x, xlen=%d\n",
			   dh, data_pa, xlen);
	}
#endif

	/*
	 * Set up the DMA controller.
	 * Already set FIFO count in dma_setup.
	 */

#ifdef	DEBUG
	if ((si->fifo_count > xlen) ||
		(si->fifo_count < (xlen - 1)))
	{
		printf("si_dma_start: fifo_count=0x%x, xlen=0x%x\n",
			   si->fifo_count, xlen);
		Debugger();
	}
#endif

	/*
	 * The OBIO controller needs a command block.
	 */
	cmd = &dh->dh_cmd;
	cmd->addrh = ((data_pa & 0xFF0000) >> 8) | UDC_ADDR_INFO;
	cmd->addrl = data_pa & 0xFFFF;
	cmd->count = xlen / 2;	/* bytes -> words */
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

	ncr_sc->sc_state |= NCR_DOINGDMA;

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_start: started, flags=0x%x\n",
			   ncr_sc->sc_state);
	}
#endif
}


void
si_obio_dma_eop(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{

	/* Not needed - DMA was stopped prior to examining sci_csr */
}


void
si_obio_dma_stop(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	struct si_dma_handle *dh = sr->sr_dma_hand;
	volatile struct si_regs *si = sc->sc_regs;
	int resid, ntrans, tmo, udc_cnt;

	if ((ncr_sc->sc_state & NCR_DOINGDMA) == 0) {
#ifdef	DEBUG
		printf("si_dma_stop: dma not running\n");
#endif
		return;
	}
	ncr_sc->sc_state &= ~NCR_DOINGDMA;

	if (si->si_csr & (SI_CSR_DMA_CONFLICT | SI_CSR_DMA_BUS_ERR)) {
		printf("si: DMA error, csr=0x%x, reset\n", si->si_csr);
		sr->sr_xs->error = XS_DRIVER_STUFFUP;
		ncr_sc->sc_state |= NCR_ABORTING;
		si_reset_adapter(ncr_sc);
	}

	/* Note that timeout may have set the error flag. */
	if (ncr_sc->sc_state & NCR_ABORTING)
		goto out;

	/*
	 * After a read, wait for the FIFO to empty.
	 * Note: this only works on the OBIO version.
	 */
	if ((dh->dh_flags & SIDH_OUT) == 0) {
		tmo = 200000;	/* X10 = 2 sec. */
		for (;;) {
			if (si->si_csr & SI_CSR_FIFO_EMPTY)
				break;
			if (--tmo <= 0) {
				printf("si: dma fifo did not empty, reset\n");
				ncr_sc->sc_state |= NCR_ABORTING;
				/* si_reset_adapter(ncr_sc); */
				goto out;
			}
			delay(10);
		}
	}

	/*
	 * Now try to figure out how much actually transferred
	 *
	 * The fifo_count might not reflect how many bytes were
	 * actually transferred for VME.
	 */

	resid = si->fifo_count & 0xFFFF;
	ntrans = sc->sc_xlen - resid;

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
	if ((dh->dh_flags & SIDH_OUT) == 0) {
		/* If odd transfer count, grab last byte by hand. */
		if (ntrans & 1) {
			ncr_sc->sc_dataptr[-1] =
				(si->fifo_data & 0xff00) >> 8;
			goto out;
		}
		/* UDC might not have transfered the last word. */
		udc_cnt = si_obio_udc_read(si, UDC_ADR_COUNT);
		if (((udc_cnt * 2) - resid) == 2) {
			ncr_sc->sc_dataptr[-2] =
				(si->fifo_data & 0xff00) >> 8;
			ncr_sc->sc_dataptr[-1] =
				(si->fifo_data & 0x00ff);
		}
	}

out:
	/* Reset the UDC. */
	si_obio_udc_write(si, UDC_ADR_COMMAND, UDC_CMD_RESET);
	si->fifo_count = 0;

	/* Put SBIC back in PIO mode. */
	*ncr_sc->sci_mode &= ~(SCI_MODE_DMA | SCI_MODE_DMA_IE);
	*ncr_sc->sci_icmd = 0;
}


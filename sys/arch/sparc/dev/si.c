/*	$OpenBSD: si.c,v 1.21 2004/11/29 06:20:02 jsg Exp $	*/
/*	$NetBSD: si.c,v 1.38 1997/08/27 11:24:20 bouyer Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass, David Jones, Gordon W. Ross, and Jason R. Thorpe.
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
 * Sun4 SCSI driver.  (Autoconfig stuff and DMA functions.)
 * The machine-independent parts are in ncr5380sbc.c
 *
 * Supported hardware includes:
 * Sun "SCSI Weird" on OBIO (sw: Sun 4/100-series)
 * Sun SCSI-3 on VME (si: Sun 4/200-series, others)
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
 * David Jones wrote the initial version of this module for NetBSD/sun3,
 * which included support for the VME adapter only. (no reselection).
 *
 * Gordon Ross added support for the Sun 3 OBIO adapter, and re-worked
 * both the VME and OBIO code to support disconnect/reselect.
 * (Required figuring out the hardware "features" noted above.)
 *
 * The autoconfiguration boilerplate came from Adam Glass.
 *
 * Jason R. Thorpe ported the autoconfiguration and VME portions to
 * NetBSD/sparc, and added initial support for the 4/100 "SCSI Weird",
 * a wacky OBIO variant of the VME SCSI-3.  Many thanks to Chuck Cranor
 * for lots of helpful tips and suggestions.  Thanks also to Paul Kranenburg
 * and Chris Torek for bits of insight needed along the way.  Thanks to
 * David Gilbert and Andrew Gillham who risked filesystem life-and-limb
 * for the sake of testing.  Andrew Gillham helped work out the bugs
 * in the 4/100 DMA code.
 */

/*
 * NOTE: support for the 4/100 "SCSI Weird" is not complete!  DMA
 * works, but interrupts (and, thus, reselection) don't.  I don't know
 * why, and I don't have a machine to test this on further.
 *
 * DMA, DMA completion interrupts, and reselection work fine on my
 * 4/260 with modern SCSI-II disks attached.  I've had reports of
 * reselection failing on Sun Shoebox-type configurations where
 * there are multiple non-SCSI devices behind Emulex or Adaptec
 * bridges.  These devices pre-date the SCSI-I spec, and might not
 * bahve the way the 5380 code expects.  For this reason, only
 * DMA is enabled by default in this driver.
 *
 *	Jason R. Thorpe <thorpej@NetBSD.ORG>
 *	December 8, 1995
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_debug.h>
#include <scsi/scsiconf.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/pmap.h>

#include <sparc/sparc/vaddrs.h>
#include <sparc/sparc/cpuvar.h>

#ifndef DDB
#define	Debugger()
#endif

#ifndef DEBUG
#define DEBUG XXX
#endif

#define COUNT_SW_LEFTOVERS	XXX	/* See sw DMA completion code */

#include <dev/ic/ncr5380reg.h>
#include <dev/ic/ncr5380var.h>

#include <sparc/dev/sireg.h>

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

#ifdef	DEBUG
int si_debug = 0;
static int si_link_flags = 0 /* | SDEV_DB2 */ ;
#endif

/*
 * This structure is used to keep track of mapped DMA requests.
 */
struct si_dma_handle {
	int 		dh_flags;
#define	SIDH_BUSY	0x01		/* This DH is in use */
#define	SIDH_OUT	0x02		/* DMA does data out (write) */
	u_char *	dh_addr;	/* KVA of start of buffer */
	int 		dh_maplen;	/* Original data length */
	long		dh_dvma;	/* VA of buffer in DVMA space */
	long		dh_startingpa;	/* PA of buffer; for "sw" */
};

/*
 * The first structure member has to be the ncr5380_softc
 * so we can just cast to go back and fourth between them.
 */
struct si_softc {
	struct ncr5380_softc	ncr_sc;
	volatile struct si_regs	*sc_regs;
	struct intrhand	sc_ih;
	int		sc_adapter_type;
	int		sc_adapter_iv_am; /* int. vec + address modifier */
	struct si_dma_handle *sc_dma;
	int		sc_xlen;	/* length of current DMA segment. */
	int		sc_options;	/* options for this instance. */
};

/*
 * Options.  By default, DMA is enabled and DMA completion interrupts
 * and reselect are disabled.  You may enable additional features
 * the `flags' directive in your kernel's configuration file.
 *
 * Alternatively, you can patch your kernel with DDB or some other
 * mechanism.  The sc_options member of the softc is OR'd with
 * the value in si_options.
 *
 * On the "sw", interrupts (and thus) reselection don't work, so they're
 * disabled by default.  DMA is still a little dangerous, too.
 *
 * Note, there's a separate sw_options to make life easier.
 */
#define	SI_ENABLE_DMA	0x01	/* Use DMA (maybe polled) */
#define	SI_DMA_INTR	0x02	/* DMA completion interrupts */
#define	SI_DO_RESELECT	0x04	/* Allow disconnect/reselect */
#define	SI_OPTIONS_MASK	(SI_ENABLE_DMA|SI_DMA_INTR|SI_DO_RESELECT)
#define SI_OPTIONS_BITS	"\10\3RESELECT\2DMA_INTR\1DMA"
int si_options = SI_ENABLE_DMA;
int sw_options = SI_ENABLE_DMA;

/* How long to wait for DMA before declaring an error. */
int si_dma_intr_timo = 500;	/* ticks (sec. X 100) */

static int	si_match(struct device *, void *, void *);
static void	si_attach(struct device *, struct device *, void *);
static int	si_intr(void *);
static void	si_reset_adapter(struct ncr5380_softc *);
static void	si_minphys(struct buf *);

void si_dma_alloc(struct ncr5380_softc *);
void si_dma_free(struct ncr5380_softc *);
void si_dma_poll(struct ncr5380_softc *);

void si_vme_dma_setup(struct ncr5380_softc *);
void si_vme_dma_start(struct ncr5380_softc *);
void si_vme_dma_eop(struct ncr5380_softc *);
void si_vme_dma_stop(struct ncr5380_softc *);

void si_vme_intr_on(struct ncr5380_softc *);
void si_vme_intr_off(struct ncr5380_softc *);

void si_obio_dma_setup(struct ncr5380_softc *);
void si_obio_dma_start(struct ncr5380_softc *);
void si_obio_dma_eop(struct ncr5380_softc *);
void si_obio_dma_stop(struct ncr5380_softc *);

void si_obio_intr_on(struct ncr5380_softc *);
void si_obio_intr_off(struct ncr5380_softc *);

static struct scsi_adapter	si_ops = {
	ncr5380_scsi_cmd,		/* scsi_cmd()		*/
	si_minphys,			/* scsi_minphys()	*/
	NULL,				/* open_target_lu()	*/
	NULL,				/* close_target_lu()	*/
};

/* This is copied from julian's bt driver */
/* "so we have a default dev struct for our link struct." */
static struct scsi_device si_dev = {
	NULL,		/* Use default error handler.		*/
	NULL,		/* Use default start handler.		*/
	NULL,		/* Use default async handler.		*/
	NULL,		/* Use default "done" routine.		*/
};


/* The Sun SCSI-3 VME controller. */
struct cfattach si_ca = {
	sizeof(struct si_softc), si_match, si_attach
};

struct cfdriver si_cd = {
	NULL, "si", DV_DULL
};

/* The Sun "SCSI Weird" 4/100 obio controller. */
struct cfattach sw_ca = {
	sizeof(struct si_softc), si_match, si_attach
};

struct cfdriver sw_cd = {
	NULL, "sw", DV_DULL
};

static int
si_match(parent, vcf, aux)
	struct device	*parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	/* Are we looking for the right thing? */
	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);

	/* Nothing but a Sun 4 is going to have these devices. */
	if (!CPU_ISSUN4)
		return (0);

	/*
	 * Default interrupt priority always is 3.  At least, that's
	 * what my board seems to be at.  --thorpej
	 */
	if (ra->ra_intr[0].int_pri == -1)
		ra->ra_intr[0].int_pri = 3;

	/* Figure out the bus type and look for the appropriate adapter. */
	switch (ca->ca_bustype) {
	case BUS_VME16:
		/* AFAIK, the `si' can only exist on the vmes. */
		if (strcmp(ra->ra_name, "si") ||
		    cpuinfo.cpu_type == CPUTYP_4_100)
			return (0);
		break;

	case BUS_OBIO:
		/* AFAIK, an `sw' can only exist on the obio. */
		if (strcmp(ra->ra_name, "sw") ||
		    cpuinfo.cpu_type != CPUTYP_4_100)
			return (0);
		break;

	default:
		/* Don't know what we ended up with ... */
		return (0);
	}

	/* Make sure there is something there... */
	if (probeget(ra->ra_vaddr + 1, 1) == -1)
		return (0);

	/*
	 * If this is a VME SCSI board, we have to determine whether
	 * it is an "sc" (Sun2) or "si" (Sun3) SCSI board.  This can
	 * be determined using the fact that the "sc" board occupies
	 * 4K bytes in VME space but the "si" board occupies 2K bytes.
	 */
	if (strcmp(cf->cf_driver->cd_name, "si") == 0)
		if (probeget(ra->ra_vaddr + 0x801, 1) != -1)
			return(0);

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
	struct romaux *ra = &ca->ca_ra;
	struct bootpath *bp;
	int i;

	/*
	 * Pull in the options flags.  Allow the user to completely
	 * override the default values.
	 */
	if ((ncr_sc->sc_dev.dv_cfdata->cf_flags & SI_OPTIONS_MASK) != 0)
		sc->sc_options =
		    (ncr_sc->sc_dev.dv_cfdata->cf_flags & SI_OPTIONS_MASK);
	else
		sc->sc_options =
		    (ca->ca_bustype == BUS_OBIO) ? sw_options : si_options;

	/* Map the controller registers. */
	regs = (struct si_regs *)
		mapiodev(ra->ra_reg, 0, sizeof(struct si_regs));

	/*
	 * Fill in the prototype scsi_link.
	 */
	ncr_sc->sc_link.adapter_softc = sc;
	ncr_sc->sc_link.adapter_target = 7;
	ncr_sc->sc_link.adapter = &si_ops;
	ncr_sc->sc_link.device = &si_dev;
	ncr_sc->sc_link.openings = 4;

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

	switch (ca->ca_bustype) {
	case BUS_VME16:
		ncr_sc->sc_dma_setup = si_vme_dma_setup;
		ncr_sc->sc_dma_start = si_vme_dma_start;
		ncr_sc->sc_dma_eop   = si_vme_dma_stop;
		ncr_sc->sc_dma_stop  = si_vme_dma_stop;
		if (sc->sc_options & SI_DO_RESELECT) {
			/*
			 * Need to enable interrupts (and DMA!)
			 * on this H/W for reselect to work.
			 */
			ncr_sc->sc_intr_on   = si_vme_intr_on;
			ncr_sc->sc_intr_off  = si_vme_intr_off;
		}
		break;

	case BUS_OBIO:
		ncr_sc->sc_dma_setup = si_obio_dma_setup;
		ncr_sc->sc_dma_start = si_obio_dma_start;
		ncr_sc->sc_dma_eop   = si_obio_dma_stop;
		ncr_sc->sc_dma_stop  = si_obio_dma_stop;
		ncr_sc->sc_intr_on   = si_obio_intr_on;
		ncr_sc->sc_intr_off  = si_obio_intr_off;
		break;

	default:
		panic("si_attach: impossible bus type 0x%x", ca->ca_bustype);
		/* NOTREACHED */
	}

	ncr_sc->sc_flags = 0;
	if ((sc->sc_options & SI_DO_RESELECT) == 0)
		ncr_sc->sc_flags |= NCR5380_PERMIT_RESELECT;
	if ((sc->sc_options & SI_DMA_INTR) == 0)
		ncr_sc->sc_flags |= NCR5380_FORCE_POLLING;
	ncr_sc->sc_min_dma_len = MIN_DMA_LEN;

	/*
	 * Initialize fields used only here in the MD code.
	 */
	sc->sc_regs = regs;
	sc->sc_adapter_type = ca->ca_bustype;
	/*  sc_adapter_iv_am = (was set above) */

	/*
	 * Allocate DMA handles.
	 */
	i = SCI_OPENINGS * sizeof(struct si_dma_handle);
	sc->sc_dma = (struct si_dma_handle *)malloc(i, M_DEVBUF, M_NOWAIT);
	if (sc->sc_dma == NULL)
		panic("si: dma handle malloc failed");
	for (i = 0; i < SCI_OPENINGS; i++)
		sc->sc_dma[i].dh_flags = 0;

	sc->sc_regs = regs;
	sc->sc_adapter_type = ca->ca_bustype;

	/* Establish the interrupt. */
	sc->sc_ih.ih_fun = si_intr;
	sc->sc_ih.ih_arg = sc;

	switch (ca->ca_bustype) {
	case BUS_OBIO:
		/*
		 * This will be an "sw" controller.
		 */
		intr_establish(ra->ra_intr[0].int_pri, &sc->sc_ih, IPL_BIO,
		    self->dv_xname);
		break;

	case BUS_VME16:
		/*
		 * This will be an "si" controller.
		 */
		vmeintr_establish(ra->ra_intr[0].int_vec,
		    ra->ra_intr[0].int_pri, &sc->sc_ih, IPL_BIO,
		    self->dv_xname);
		sc->sc_adapter_iv_am =
		    VME_SUPV_DATA_24 | (ra->ra_intr[0].int_vec & 0xFF);
		break;

	default:
		/* Impossible case handled above. */
		break;
	}
	printf(" pri %d\n", ra->ra_intr[0].int_pri);
	if (sc->sc_options) {
		printf("%s: options=%b\n", ncr_sc->sc_dev.dv_xname,
			sc->sc_options, SI_OPTIONS_BITS);
	}
#ifdef	DEBUG
	if (si_debug)
		printf("si: Set TheSoftC=%p TheRegs=%p\n", sc, regs);
	ncr_sc->sc_link.flags |= si_link_flags;
#endif

	/*
	 *  Initialize si board itself.
	 */
	si_reset_adapter(ncr_sc);
	ncr5380_init(ncr_sc);
	ncr5380_reset_scsibus(ncr_sc);

	/*
	 * If the boot path is "sw" or "si" at the moment and it's me, then
	 * walk out pointer to the sub-device, ready for the config
	 * below.
	 */
	bp = ra->ra_bp;
	if (bp != NULL && strcmp(bp->name, ra->ra_name) == 0 &&
	    bp->val[0] == -1 && bp->val[1] == ncr_sc->sc_dev.dv_unit)
		bootpath_store(1, bp + 1);

	/* Configure sub-devices */
	config_found(self, &(ncr_sc->sc_link), scsiprint);

	bootpath_store(1, NULL);
}

static void
si_minphys(struct buf *bp)
{
	if (bp->b_bcount > MAX_DMA_LEN) {
#ifdef DEBUG
		if (si_debug) {
			printf("si_minphys len = 0x%x.\n", MAX_DMA_LEN);
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
	if (sc->sc_adapter_type == BUS_OBIO)
		csr = si->sw_csr;
	else
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
#ifdef DEBUG
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
	switch(sc->sc_adapter_type) {
	case BUS_VME16:
		si->si_csr = 0;
		delay(10);
		si->si_csr = SI_CSR_FIFO_RES | SI_CSR_SCSI_RES | SI_CSR_INTR_EN;
		delay(10);
		si->fifo_count = 0;
		si->dma_addrh = 0;
		si->dma_addrl = 0;
		si->dma_counth = 0;
		si->dma_countl = 0;
		si->si_iv_am = sc->sc_adapter_iv_am;
		si->fifo_cnt_hi = 0;
		break;

	case BUS_OBIO:
		si->sw_csr = 0;
		delay(10);
		si->sw_csr = SI_CSR_SCSI_RES;
		si->dma_addr = 0;
		si->dma_count = 0;
		delay(10);
		si->sw_csr |= SI_CSR_INTR_EN;
		break;
	}

	SCI_CLR_INTR(ncr_sc);
}


/*****************************************************************
 * Common functions for DMA
 ****************************************************************/

/*
 * Allocate a DMA handle and put it in sc->sc_dma.  Prepare
 * for DMA transfer.  On the Sun4, this means mapping the buffer
 * into DVMA space.
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

#ifdef DIAGNOSTIC
	if (sr->sr_dma_hand != NULL)
		panic("si_dma_alloc: already have DMA handle");
#endif

#if 1	/* XXX - Temporary */
	/* XXX - In case we think DMA is completely broken... */
	if ((sc->sc_options & SI_ENABLE_DMA) == 0)
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
		panic("si_dma_alloc: xlen=0x%x", xlen);

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
	dh->dh_addr = (u_char *) addr;
	dh->dh_maplen  = xlen;
	dh->dh_dvma = 0;

	/* Copy the "write" flag for convenience. */
	if (xs->flags & SCSI_DATA_OUT)
		dh->dh_flags |= SIDH_OUT;

	/*
	 * Double-map the buffer into DVMA space.  If we can't re-map
	 * the buffer, we print a warning and fall back to PIO mode.
	 *
	 * NOTE: it is not safe to sleep here!
	 */
	dh->dh_dvma = (long)kdvma_mapin((caddr_t)addr, xlen, 0);
	if (dh->dh_dvma == 0) {
		/* Can't remap segment */
		printf("si_dma_alloc: can't remap %p/0x%x, doing PIO\n",
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

#ifdef DIAGNOSTIC
	if (dh == NULL)
		panic("si_dma_free: no DMA handle");
#endif

	if (ncr_sc->sc_state & NCR_DOINGDMA)
		panic("si_dma_free: free while in progress");

	if (dh->dh_flags & SIDH_BUSY) {
		/* XXX - Should separate allocation and mapping. */

		/* Give back the DVMA space. */
		dvma_mapout((vaddr_t)dh->dh_dvma, (vaddr_t)dh->dh_addr,
			    dh->dh_maplen);

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
	volatile struct si_regs *si = sc->sc_regs;
	int tmo, csr_mask, csr;

	/* Make sure DMA started successfully. */
	if (ncr_sc->sc_state & NCR_ABORTING)
		return;

	csr_mask = SI_CSR_SBC_IP | SI_CSR_DMA_IP |
		SI_CSR_DMA_CONFLICT | SI_CSR_DMA_BUS_ERR;

	tmo = 50000;	/* X100 = 5 sec. */
	for (;;) {
		if (sc->sc_adapter_type == BUS_OBIO)
			csr = si->sw_csr;
		else
			csr = si->si_csr;
		if (csr & csr_mask)
			break;
		if (--tmo <= 0) {
			printf("%s: DMA timeout (while polling)\n",
			    ncr_sc->sc_dev.dv_xname);
			/* Indicate timeout as MI code would. */
			sr->sr_flags |= SR_OVERDUE;
			break;
		}
		delay(100);
	}

#ifdef	DEBUG
	if (si_debug) {
		printf("si_dma_poll: done, csr=0x%x\n", csr);
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
 * that precedes a DATA_IN or DATA_OUT phase, in case we need
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
	si->si_csr &= ~SI_CSR_FIFO_RES;		/* active low */
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
	u_long data_pa;
	int xlen;

	/*
	 * Get the DVMA mapping for this segment.
	 * XXX - Should separate allocation and mapin.
	 */
	data_pa = (u_long)(dh->dh_dvma - DVMA_BASE);
	if (data_pa & 1)
		panic("si_dma_start: bad pa=0x%lx", data_pa);
	xlen = ncr_sc->sc_datalen;
	xlen &= ~1;
	sc->sc_xlen = xlen;	/* XXX: or less... */

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_start: dh=%p, pa=0x%lx, xlen=%d\n",
			   dh, data_pa, xlen);
	}
#endif

	/*
	 * Set up the DMA controller.
	 * Note that (dh->dh_len < sc_datalen)
	 */
	si->si_csr &= ~SI_CSR_FIFO_RES;		/* active low */
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

	si->dma_addrh = (u_short)(data_pa >> 16);
	si->dma_addrl = (u_short)(data_pa & 0xFFFF);

	si->dma_counth = (u_short)(xlen >> 16);
	si->dma_countl = (u_short)(xlen & 0xFFFF);

#if 1
	/* Set it anyway, even though dma_count hits it? */
	si->fifo_cnt_hi = (u_short)(xlen >> 16);
	si->fifo_count  = (u_short)(xlen & 0xFFFF);
#endif

#ifdef DEBUG
	if (si->fifo_count != xlen) {
		printf("si_dma_start: Fifo_count=0x%x, xlen=0x%x\n",
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
		*ncr_sc->sci_irecv = 0;		/* start it */
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
	 * one byte and the resid needs to be incremented.
	 * Only happens for partial transfers.
	 * (Thanks to Matt Jacob)
	 */

	resid = si->fifo_count & 0xFFFF;
	if (dh->dh_flags & SIDH_OUT)
		if ((resid > 0) && (resid < sc->sc_xlen))
			resid++;
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


/*
 * This is called when the bus is going idle,
 * so we want to enable the SBC interrupts.
 * That is controlled by the DMA enable!
 * Who would have guessed!
 * What a NASTY trick!
 *
 * XXX THIS MIGHT NOT WORK RIGHT!
 */
void
si_obio_intr_on(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	volatile struct si_regs *si = sc->sc_regs;

	si_obio_dma_setup(ncr_sc);
	si->sw_csr |= SI_CSR_DMA_EN;
}

/*
 * This is called when the bus is idle and we are
 * about to start playing with the SBC chip.
 *
 * XXX THIS MIGHT NOT WORK RIGHT!
 */
void
si_obio_intr_off(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	volatile struct si_regs *si = sc->sc_regs;

	si->sw_csr &= ~SI_CSR_DMA_EN;
}


/*
 * This function is called during the COMMAND or MSG_IN phase
 * that precedes a DATA_IN or DATA_OUT phase, in case we need
 * to setup the DMA engine before the bus enters a DATA phase.
 *
 * On the OBIO version we just clear the DMA count and address
 * here (to make sure it stays idle) and do the real setup
 * later, in dma_start.
 */
void
si_obio_dma_setup(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	volatile struct si_regs *si = sc->sc_regs;

	/* No FIFO to reset on "sw". */

	/* Set direction (assume recv here) */
	si->sw_csr &= ~SI_CSR_SEND;

	si->dma_addr = 0;
	si->dma_count = 0;
}


void
si_obio_dma_start(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	struct si_dma_handle *dh = sr->sr_dma_hand;
	volatile struct si_regs *si = sc->sc_regs;
	u_long data_pa;
	int xlen, adj, adjlen;

	/*
	 * Get the DVMA mapping for this segment.
	 * XXX - Should separate allocation and mapin.
	 */
	data_pa = (u_long)(dh->dh_dvma - DVMA_BASE);
	if (data_pa & 1)
		panic("si_dma_start: bad pa=0x%lx", data_pa);
	xlen = ncr_sc->sc_datalen;
	xlen &= ~1;
	sc->sc_xlen = xlen;	/* XXX: or less... */

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_start: dh=%p, pa=0x%lx, xlen=%d\n",
		    dh, data_pa, xlen);
	}
#endif

	/*
	 * Set up the DMA controller.
	 * Note that (dh->dh_len < sc_datalen)
	 */

	/* Set direction (send/recv) */
	if (dh->dh_flags & SIDH_OUT) {
		si->sw_csr |= SI_CSR_SEND;
	} else {
		si->sw_csr &= ~SI_CSR_SEND;
	}

	/*
	 * The "sw" needs longword aligned transfers.  We
	 * detect a shortword aligned transfer here, and adjust the
	 * DMA transfer by 2 bytes.  These two bytes are read/written
	 * in PIO mode just before the DMA is started.
	 */
	adj = 0;
	if (data_pa & 2) {
		adj = 2;
#ifdef DEBUG
		if (si_debug & 2)
			printf("si_dma_start: adjusted up %d bytes\n", adj);
#endif
	}

	/* We have to frob the address on the "sw". */
	dh->dh_startingpa = (data_pa | 0xF00000);
	si->dma_addr = (int)(dh->dh_startingpa + adj);
	si->dma_count = (xlen - adj);

	/*
	 * Acknowledge the phase change.  (After DMA setup!)
	 * Put the SBIC into DMA mode, and start the transfer.
	 */
	if (dh->dh_flags & SIDH_OUT) {
		*ncr_sc->sci_tcmd = PHASE_DATA_OUT;
		if (adj) {
			adjlen = ncr5380_pio_out(ncr_sc, PHASE_DATA_OUT,
			    adj, dh->dh_addr);
			if (adjlen != adj)
				printf("%s: bad outgoing adj, %d != %d\n",
				    ncr_sc->sc_dev.dv_xname, adjlen, adj);
		}
		SCI_CLR_INTR(ncr_sc);
		*ncr_sc->sci_icmd = SCI_ICMD_DATA;
		*ncr_sc->sci_mode |= (SCI_MODE_DMA | SCI_MODE_DMA_IE);
		*ncr_sc->sci_dma_send = 0;	/* start it */
	} else {
		*ncr_sc->sci_tcmd = PHASE_DATA_IN;
		if (adj) {
			adjlen = ncr5380_pio_in(ncr_sc, PHASE_DATA_IN,
			    adj, dh->dh_addr);
			if (adjlen != adj)
				printf("%s: bad incoming adj, %d != %d\n",
				    ncr_sc->sc_dev.dv_xname, adjlen, adj);
		}
		SCI_CLR_INTR(ncr_sc);
		*ncr_sc->sci_icmd = 0;
		*ncr_sc->sci_mode |= (SCI_MODE_DMA | SCI_MODE_DMA_IE);
		*ncr_sc->sci_irecv = 0;		/* start it */
	}

	/* Let'er rip! */
	si->sw_csr |= SI_CSR_DMA_EN;

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

#if (defined(DEBUG) || defined(DIAGNOSTIC)) && !defined(COUNT_SW_LEFTOVERS)
#define COUNT_SW_LEFTOVERS
#endif
#ifdef COUNT_SW_LEFTOVERS
/*
 * Let's find out how often these occur.  Read these with DDB from time
 * to time.
 */
int	sw_3_leftover = 0;
int	sw_2_leftover = 0;
int	sw_1_leftover = 0;
int	sw_0_leftover = 0;
#endif

void
si_obio_dma_stop(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	struct si_dma_handle *dh = sr->sr_dma_hand;
	volatile struct si_regs *si = sc->sc_regs;
	int ntrans = 0, Dma_addr;

	if ((ncr_sc->sc_state & NCR_DOINGDMA) == 0) {
#ifdef	DEBUG
		printf("si_dma_stop: dma not running\n");
#endif
		return;
	}
	ncr_sc->sc_state &= ~NCR_DOINGDMA;

	/* First, halt the DMA engine. */
	si->sw_csr &= ~SI_CSR_DMA_EN;

	/*
	 * XXX HARDWARE BUG!
	 * Apparently, some early 4/100 SCSI controllers had a hardware
	 * bug that caused the controller to do illegal memory access.
	 * We see this as SI_CSR_DMA_BUS_ERR (makes sense).  To work around
	 * this, we simply need to clean up after ourselves ... there will
	 * be as many as 3 bytes left over.  Since we clean up "left-over"
	 * bytes on every read anyway, we just continue to chug along
	 * if SI_CSR_DMA_BUS_ERR is asserted.  (This was probably worked
	 * around in hardware later with the "left-over byte" indicator
	 * in the VME controller.)
	 */
#if 0
	if (si->sw_csr & (SI_CSR_DMA_CONFLICT | SI_CSR_DMA_BUS_ERR)) {
#else
	if (si->sw_csr & (SI_CSR_DMA_CONFLICT)) {
#endif
		printf("sw: DMA error, csr=0x%x, reset\n", si->sw_csr);
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
	 * The "sw" doesn't have a FIFO or a bcr, so we've stored
	 * the starting PA of the transfer in the DMA handle,
	 * and subtract it from the ending PA left in the dma_addr
	 * register.
	 */
	Dma_addr = si->dma_addr;
	ntrans = (Dma_addr - dh->dh_startingpa);

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_stop: ntrans=0x%x\n", ntrans);
	}
#endif

	if (ntrans < MIN_DMA_LEN) {
		printf("sw: short transfer\n");
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
	 * "Left-over bytes"  (yuck!)  The "sw" doesn't
	 * have a "left-over" indicator, so we have to so
	 * this no matter what.  Ick.
	 */
	if ((dh->dh_flags & SIDH_OUT) == 0) {
		char *cp = ncr_sc->sc_dataptr;

		switch (Dma_addr & 3) {
		case 3:
			cp[0] = (si->sw_bpr & 0xff000000) >> 24;
			cp[1] = (si->sw_bpr & 0x00ff0000) >> 16;
			cp[2] = (si->sw_bpr & 0x0000ff00) >> 8;
#ifdef COUNT_SW_LEFTOVERS
			++sw_3_leftover;
#endif
			break;

		case 2:
			cp[0] = (si->sw_bpr & 0xff000000) >> 24;
			cp[1] = (si->sw_bpr & 0x00ff0000) >> 16;
#ifdef COUNT_SW_LEFTOVERS
			++sw_2_leftover;
#endif
			break;

		case 1:
			cp[0] = (si->sw_bpr & 0xff000000) >> 24;
#ifdef COUNT_SW_LEFTOVERS
			++sw_1_leftover;
#endif
			break;

#ifdef COUNT_SW_LEFTOVERS
		default:
			++sw_0_leftover;
			break;
#endif
		}
	}

 out:
	si->dma_addr = 0;
	si->dma_count = 0;

	/* Put SBIC back in PIO mode. */
	*ncr_sc->sci_mode &= ~(SCI_MODE_DMA | SCI_MODE_DMA_IE);
	*ncr_sc->sci_icmd = 0;

#ifdef DEBUG
	if (si_debug & 2) {
		printf("si_dma_stop: ntrans=0x%x\n", ntrans);
	}
#endif
}

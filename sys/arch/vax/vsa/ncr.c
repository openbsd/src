/*	$OpenBSD: ncr.c,v 1.4 1999/01/11 05:12:09 millert Exp $	*/
/*	$NetBSD: ncr.c,v 1.8 1997/02/26 22:29:12 gwr Exp $	*/

/* #define DEBUG	/* */
/* #define TRACE	/* */
/* #define POLL_MODE	/* */
#define USE_VMAPBUF

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
 *	This product includes software developed by
 *	Adam Glass, David Jones, and Gordon Ross
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
 * generally related to the DMA engine.	 The OBIO variant will
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
 *
 * VS2000:
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/stat.h> 
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/map.h>
#include <sys/device.h>
#include <sys/dkstat.h> 
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>

/* #include <sys/errno.h> */

#include <scsi/scsi_all.h>
#include <scsi/scsi_debug.h>
#include <scsi/scsiconf.h>

#include <machine/uvax.h>
#include <machine/ka410.h>
#include <machine/ka43.h>
#include <machine/vsbus.h>	/* struct confargs */

#include <dev/ic/ncr5380reg.h>
#include <dev/ic/ncr5380var.h>

#define trace(x)
#define debug(x)

#ifndef NCR5380_CSRBITS
#define NCR5380_CSRBITS \
	"\020\010DEND\007DREQ\006PERR\005IREQ\004MTCH\003DCON\002ATN\001ACK"
#endif

#ifndef NCR5380_BUSCSRBITS
#define NCR5380_BUSCSRBITS \
	"\020\010RST\007BSY\006REQ\005MSG\004C/D\003I/O\002SEL\001DBP"
#endif

#include "ncr.h"

#ifdef DDB
#define integrate
#else
#define integrate static
#endif

/*
 * Transfers smaller than this are done using PIO
 * (on assumption they're not worth DMA overhead)
 */
#define MIN_DMA_LEN 128 

/*
 * Transfers lager than 65535 bytes need to be split-up.
 * (Some of the FIFO logic has only 16 bits counters.)
 * Make the size an integer multiple of the page size
 * to avoid buf/cluster remap problems.	 (paranoid?)
 *
 * bertram: VS2000 has an DMA-area which is 16KB, thus
 * have a maximum DMA-size of 16KB...
 */
#ifdef DMA_SHARED
#define MAX_DMA_LEN	0x2000		/* (8 * 1024) */
#define DMA_ADDR_HBYTE	0x20
#define DMA_ADDR_LBYTE	0x00
#else
#define MAX_DMA_LEN	0x4000		/* (16 * 1024) */
#define DMA_ADDR_HBYTE	0x00
#define DMA_ADDR_LBYTE	0x00
#endif

#ifdef	DEBUG
int si_debug = 3;
static int si_link_flags = 0 /* | SDEV_DB2 */ ;
#endif

/*
 * This structure is used to keep track of mappedpwd DMA requests.
 * Note: combined the UDC command block with this structure, so
 * the array of these has to be in DVMA space.
 */
struct si_dma_handle {
	int		dh_flags;
#define SIDH_BUSY	1		/* This DH is in use */
#define SIDH_OUT	2		/* DMA does data out (write) */
#define SIDH_PHYS	4
#define SIDH_DONE	8
	u_char *	dh_addr;	/* KVA of start of buffer */
	int		dh_maplen;	/* Length of KVA mapping. */
	u_char *	dh_dvma;	/* VA of buffer in DVMA space */
	int		dh_xlen;
};

/*
 * The first structure member has to be the ncr5380_softc
 * so we can just cast to go back and fourth between them.
 */
struct si_softc {
	struct ncr5380_softc	ncr_sc;
	volatile struct si_regs *sc_regs;	/* do we really need this? */

	struct si_dma_handle	*sc_dma;
	struct confargs		*sc_cfargs;	

	int	sc_xflags;	/* ka410/ka43: resid, sizeof(areg) */

	char	*sc_dbase;
	int	sc_dsize;
	
	volatile char	*sc_dareg;
	volatile short	*sc_dcreg;
	volatile char	*sc_ddreg;
	volatile int	sc_dflags;
	
#define VSDMA_LOCKED	0x80	/* */
#define VSDMA_WANTED	0x40	/* */
#define VSDMA_IWANTED	0x20
#define VSDMA_BLOCKED	0x10
#define VSDMA_DMABUSY	0x08	/* DMA in progress */
#define VSDMA_REGBUSY	0x04	/* accessing registers */
#define VSDMA_WRBUF	0x02	/* writing to bounce-buffer */
#define VSDMA_RDBUF	0x01	/* reading from bounce-buffer */

#define VSDMA_STATUS	0xF0
#define VSDMA_LCKTYPE	0x0F

#ifdef POLL_MODE
	volatile u_char *intreq;
	volatile u_char *intclr;
	volatile u_char *intmsk;
	volatile int	intbit;
#endif
};

extern int cold;	/* enable polling while cold-flag set */

/* Options.  Interesting values are: 1,3,7 */
int si_options = 3;	/* bertram: 3 or 7 ??? */
#define SI_ENABLE_DMA	1	/* Use DMA (maybe polled) */
#define SI_DMA_INTR	2	/* DMA completion interrupts */
#define SI_DO_RESELECT	4	/* Allow disconnect/reselect */

#define DMA_DIR_IN  1
#define DMA_DIR_OUT 0

/* How long to wait for DMA before declaring an error. */
int si_dma_intr_timo = 500;	/* ticks (sec. X 100) */

integrate char si_name[] = "ncr";
integrate int	si_match();
integrate void	si_attach();
integrate int	si_intr __P((void *));

integrate void	si_minphys __P((struct buf *bp));
integrate void	si_reset_adapter __P((struct ncr5380_softc *sc));

void si_dma_alloc __P((struct ncr5380_softc *));
void si_dma_free __P((struct ncr5380_softc *));
void si_dma_poll __P((struct ncr5380_softc *));

void si_intr_on __P((struct ncr5380_softc *));
void si_intr_off __P((struct ncr5380_softc *));

int si_dmaLockBus __P((struct ncr5380_softc *, int));
int si_dmaToggleLock __P((struct ncr5380_softc *, int, int));
int si_dmaReleaseBus __P((struct ncr5380_softc *, int));

void si_dma_setup __P((struct ncr5380_softc *));
void si_dma_start __P((struct ncr5380_softc *));
void si_dma_eop __P((struct ncr5380_softc *));
void si_dma_stop __P((struct ncr5380_softc *));

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


struct cfdriver ncr_cd = {
	NULL, si_name, DV_DULL
};
struct cfattach ncr_ca = {
	sizeof(struct si_softc), si_match, si_attach,
};

void
dk_establish(p,q)
	struct disk *p;
	struct device *q;
{
#if 0
	printf ("faking dk_establish()...\n");
#endif
}


integrate int
si_match(parent, match, aux)
	struct device	*parent;
	void		*match, *aux;
{
	struct cfdata	*cf = match;
	struct confargs *ca = aux;

	trace(("ncr_match(0x%x, %d, %s)\n", parent, cf->cf_unit, ca->ca_name));

	if (strcmp(ca->ca_name, "ncr") &&
	    strcmp(ca->ca_name, "ncr5380") &&
	    strcmp(ca->ca_name, "NCR5380"))
		return (0);

	/*
	 * we just define it being there ...
	 */
	return (1);
}

integrate void
si_set_portid(pid,port)
	int pid;
	int port;
{
	struct {
	  u_long    :2;
	  u_long id0:3;
	  u_long id1:3;
	  u_long    :26;
	} *p;

#ifdef DEBUG
	int *ip;
	ip = (void*)uvax_phys2virt(KA410_SCSIPORT);
	p = (void*)uvax_phys2virt(KA410_SCSIPORT);
	printf("scsi-id: (%x/%d) %d / %d\n", *ip, *ip, p->id0, p->id1);
#endif

	p = (void*)uvax_phys2virt(KA410_SCSIPORT);
	switch (port) {
	case 0:
		p->id0 = pid;
		printf(": scsi-id %d\n", p->id0);
		break;
	case 1:
		p->id1 = pid;
		printf(": scsi-id %d\n", p->id1);
		break;
	default:
		printf("invalid port-number %d\n", port);
	}
}

integrate void
si_attach(parent, self, aux)
	struct device	*parent, *self;
	void		*aux;
{
	struct si_softc *sc = (struct si_softc *) self;
	struct ncr5380_softc *ncr_sc = (struct ncr5380_softc *)sc;
	volatile struct si_regs *regs;
	struct confargs *ca = aux;
	int i;
	int *ip = aux;;

	trace (("ncr_attach(0x%x, 0x%x, %s)\n", parent, self, ca->ca_name));

	/*
	 *
	 */
#ifdef POLL_MODE
	sc->intreq = (void*)uvax_phys2virt(KA410_INTREQ);
	sc->intmsk = (void*)uvax_phys2virt(KA410_INTMSK);
	sc->intclr = (void*)uvax_phys2virt(KA410_INTCLR);
	sc->intbit = ca->ca_intbit;
#endif

	sc->sc_cfargs = ca;	/* needed for interrupt-setup */

	regs = (void*)uvax_phys2virt(ca->ca_ioaddr);

	sc->sc_dareg = (void*)uvax_phys2virt(ca->ca_dareg);
	sc->sc_dcreg = (void*)uvax_phys2virt(ca->ca_dcreg);
	sc->sc_ddreg = (void*)uvax_phys2virt(ca->ca_ddreg);
	sc->sc_dbase = (void*)uvax_phys2virt(ca->ca_dbase);
	sc->sc_dsize = ca->ca_dsize;
	sc->sc_dflags = 4;	/* XXX */
	sc->sc_xflags = ca->ca_dflag;	/* should/will be renamed */
	/*
	 * Fill in the prototype scsi_link.
	 */
#ifndef __OpenBSD__
	ncr_sc->sc_link.channel = SCSI_CHANNEL_ONLY_ONE;
#endif
	ncr_sc->sc_link.adapter_softc = sc;
	ncr_sc->sc_link.adapter_target = ca->ca_idval;
	ncr_sc->sc_link.adapter = &si_ops;
	ncr_sc->sc_link.device = &si_dev;

	si_set_portid(ca->ca_idval, ncr_sc->sc_dev.dv_unit);

	/*
	 * Initialize fields used by the MI code
	 */
	ncr_sc->sci_r0 = (void*)&regs->sci.sci_r0;
	ncr_sc->sci_r1 = (void*)&regs->sci.sci_r1;
	ncr_sc->sci_r2 = (void*)&regs->sci.sci_r2;
	ncr_sc->sci_r3 = (void*)&regs->sci.sci_r3;
	ncr_sc->sci_r4 = (void*)&regs->sci.sci_r4;
	ncr_sc->sci_r5 = (void*)&regs->sci.sci_r5;
	ncr_sc->sci_r6 = (void*)&regs->sci.sci_r6;
	ncr_sc->sci_r7 = (void*)&regs->sci.sci_r7;

	/*
	 * MD function pointers used by the MI code.
	 */
	ncr_sc->sc_pio_out = ncr5380_pio_out;
	ncr_sc->sc_pio_in =  ncr5380_pio_in;
	ncr_sc->sc_dma_alloc = si_dma_alloc;
	ncr_sc->sc_dma_free  = si_dma_free;
	ncr_sc->sc_dma_poll  = si_dma_poll;	/* si_dma_poll not used! */
	ncr_sc->sc_intr_on   = si_intr_on;	/* vsbus_unlockDMA; */
	ncr_sc->sc_intr_off  = si_intr_off;	/* vsbus_lockDMA; */

	ncr_sc->sc_dma_setup = NULL;		/* si_dma_setup not used! */
	ncr_sc->sc_dma_start = si_dma_start;
	ncr_sc->sc_dma_eop   = NULL;	
	ncr_sc->sc_dma_stop  = si_dma_stop;

	ncr_sc->sc_flags = 0;
#ifndef __OpenBSD__
	if ((si_options & SI_DO_RESELECT) == 0)
		ncr_sc->sc_no_disconnect = 0xff;
#endif
	if ((si_options & SI_DMA_INTR) == 0)
		ncr_sc->sc_flags |= NCR5380_FORCE_POLLING;
	ncr_sc->sc_min_dma_len = MIN_DMA_LEN;

	/*
	 * Initialize fields used only here in the MD code.
	 */
	i = SCI_OPENINGS * sizeof(struct si_dma_handle);
	sc->sc_dma = (struct si_dma_handle *) malloc(i);
	if (sc->sc_dma == NULL)
		panic("si: dvma_malloc failed");
	for (i = 0; i < SCI_OPENINGS; i++)
		sc->sc_dma[i].dh_flags = 0;

	sc->sc_regs = regs;
	
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
	config_found(self, &(ncr_sc->sc_link), scsiprint);

	/* 
	 * Now ready for interrupts. 
	 */
	vsbus_intr_register(sc->sc_cfargs, si_intr, (void *)sc);
	vsbus_intr_enable(sc->sc_cfargs);
}

integrate void
si_minphys(struct buf *bp)
{
	debug(("minphys: blkno=%d, bcount=%d, data=0x%x, flags=%x\n",
	      bp->b_blkno, bp->b_bcount, bp->b_data, bp->b_flags));

	if (bp->b_bcount > MAX_DMA_LEN) {
#ifdef	DEBUG
		if (si_debug) {
			printf("si_minphys len = 0x%x.\n", bp->b_bcount);
#ifdef DDB
			Debugger();
#endif
		}
#endif
		bp->b_bcount = MAX_DMA_LEN;
	}
	return (minphys(bp));
}


#define CSR_WANT (SI_CSR_SBC_IP | SI_CSR_DMA_IP | \
	SI_CSR_DMA_CONFLICT | SI_CSR_DMA_BUS_ERR )

static int si_intrCount = 0;
static int lastCSR = 0;

integrate int
si_intr(arg)
	void *arg; 
{
	struct ncr5380_softc *ncr_sc = arg;
	struct si_softc *sc = arg;
	int count, claimed;

	count = ++si_intrCount;
	trace(("%s: si-intr(%d).....\n", ncr_sc->sc_dev.dv_xname, count));

#ifdef DEBUG
	/*
	 * Each DMA interrupt is followed by one spurious(?) interrupt.
	 * if (ncr_sc->sc_state & NCR_WORKING == 0) we know, that the
	 * interrupt was not claimed by the higher-level routine, so that
	 * it might be save to ignore these...
	 */
	if ((ncr_sc->sc_state & NCR_DOINGDMA) == 0) {
		printf("spurious(%d): %x, %d, status=%b\n", count,
		       sc->sc_dflags, ncr_sc->sc_ncmds,
		       *ncr_sc->sci_csr, NCR5380_CSRBITS);
	}
#endif
	/*
	 * If there was a DMA operation in progress, now it's no longer
	 * active, since whatever caused the interrupt also interrupted
	 * the DMA operation. Thus accessing the registers now doesn't
	 * harm anything which is not yet broken...
	 */
	debug(("si_intr(status: %x, dma-count: %d)\n", 
	       *ncr_sc->sci_csr, *sc->sc_dcreg));

	/*
	 * First check for DMA errors / incomplete transfers
	 * If operation was read/data-in, the copy data from buffer
	 */
	if (ncr_sc->sc_state & NCR_DOINGDMA) {
		struct sci_req *sr = ncr_sc->sc_current;
		struct si_dma_handle *dh = sr->sr_dma_hand;
		int resid, ntrans;

		resid = *sc->sc_dcreg;
		if (resid == 1 && sc->sc_xflags) {
		  debug(("correcting resid...\n"));
		  resid = 0;
		}
		ntrans = dh->dh_xlen + resid;
		if (resid == 0) {
			if ((dh->dh_flags & SIDH_OUT) == 0) {
				si_dmaToggleLock(ncr_sc,
						 VSDMA_DMABUSY, VSDMA_RDBUF);
				bcopy(sc->sc_dbase, dh->dh_dvma, ntrans);
				si_dmaToggleLock(ncr_sc,
						 VSDMA_RDBUF, VSDMA_DMABUSY);
				dh->dh_flags |= SIDH_DONE;
			}
		}
		else {
#ifdef DEBUG
			int csr = *ncr_sc->sci_csr;
			printf("DMA incomplete (%d/%d) status = %b\n",
			       ntrans, resid, csr, NCR5380_CSRBITS);
			if(csr != lastCSR) {
				int k = (csr & ~lastCSR) | (~csr & lastCSR);
				debug(("Changed status bits: %b\n",
				       k, NCR5380_CSRBITS));
				lastCSR = csr & 0xFF;
			}
#endif
			printf("DMA incomplete: ntrans=%d/%d, lock=%x\n", 
			       ntrans, dh->dh_xlen, sc->sc_dflags);
			ncr_sc->sc_state |= NCR_ABORTING;
		}

		if ((sc->sc_dflags & VSDMA_BLOCKED) == 0) {
			printf("not blocked during DMA.\n");
		}
		sc->sc_dflags &= ~VSDMA_BLOCKED;
		si_dmaReleaseBus(ncr_sc, VSDMA_DMABUSY);
	}
	if ((sc->sc_dflags & VSDMA_BLOCKED) != 0) {
		printf("blocked while not doing DMA.\n");
		sc->sc_dflags &= ~VSDMA_BLOCKED;
	}

	/*
	 * Now, whatever it was, let the ncr5380sbc routine handle it...
	 */
	claimed = ncr5380_intr(ncr_sc);
#ifdef	DEBUG
	if (!claimed) {
		printf("si_intr: spurious from SBC\n");
		if (si_debug & 4) {
			Debugger();	/* XXX */
		}
	}
#endif
	trace(("%s: si-intr(%d) done, claimed=%d\n", 
	       ncr_sc->sc_dev.dv_xname, count, claimed));
	return (claimed);
}


integrate void
si_reset_adapter(struct ncr5380_softc *ncr_sc)
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	volatile struct si_regs *si = sc->sc_regs;

#ifdef	DEBUG
	if (si_debug) {
		printf("si_reset_adapter\n");
	}
#endif
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
	struct buf *bp = sr->sr_xs->bp;
	struct si_dma_handle *dh;
	int i, xlen;
	u_long addr;

	trace (("si_dma_alloc()\n"));

#ifdef	DIAGNOSTIC
	if (sr->sr_dma_hand != NULL)
		panic("si_dma_alloc: already have DMA handle");
#endif

	addr = (u_long) ncr_sc->sc_dataptr;
	debug(("addr=%x, dataptr=%x\n", addr, ncr_sc->sc_dataptr));
	xlen = ncr_sc->sc_datalen;

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
#ifdef DEBUG
		printf("si_dma_alloc: excessive xlen=0x%x\n", xlen);
		Debugger();
#endif
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
	dh->dh_xlen  = xlen;
	dh->dh_dvma = 0;

	/* Copy the "write" flag for convenience. */
	if (xs->flags & SCSI_DATA_OUT)
		dh->dh_flags |= SIDH_OUT;

#if 1
	/*
	 * If the buffer has the flag B_PHYS, the the address specified
	 * in the buffer is a user-space address and we need to remap
	 * this address into kernel space so that using this buffer
	 * within the interrupt routine will work.
	 * If it's already a kernel space address, we need to make sure
	 * that all pages are in-core. the mapin() routine takes care
	 * of that.
	 */
	if (bp && (bp->b_flags & B_PHYS))
		dh->dh_flags |= SIDH_PHYS;
#endif
	
	if (!bp) {
		printf("ncr.c: struct buf *bp is null-pointer.\n");
		dh->dh_flags = 0;
		return;
	}
	if (bp->b_bcount < 0 || bp->b_bcount > MAX_DMA_LEN) {
		printf("ncr.c: invalid bcount %d (0x%x)\n", 
		       bp->b_bcount, bp->b_bcount);
		dh->dh_flags = 0;
		return;
	}
	dh->dh_dvma = bp->b_data;
#if 0
	/*
	 * mapping of user-space addresses is no longer neccessary, now
	 * that the vmapbuf/vunmapbuf routines exist. Now the higher-level
	 * driver already cares for the mapping!
	 */
	if (bp->b_flags & B_PHYS) {
		xdebug(("not mapping in... %x/%x %x\n", bp->b_saveaddr, 
			bp->b_data, bp->b_bcount));
#ifdef USE_VMAPBUF
		dh->dh_addr = bp->b_data;
		dh->dh_maplen = bp->b_bcount;
		vmapbuf(bp, bp->b_bcount);
		dh->dh_dvma = bp->b_data;
#else
		dh->dh_dvma = (u_char*)vsdma_mapin(bp);
#endif
		xdebug(("addr %x, maplen %d, dvma %x, bcount %d, dir %s\n", 
		       dh->dh_addr, dh->dh_maplen, dh->dh_dvma, bp->b_bcount,
		       (dh->dh_flags & SIDH_OUT ? "OUT" : "IN")));
	}
#endif
	/* success */
	sr->sr_dma_hand = dh;

	return;
}


void
si_dma_free(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	struct scsi_xfer *xs = sr->sr_xs;
	struct buf *bp = sr->sr_xs->bp;
	struct si_dma_handle *dh = sr->sr_dma_hand;

	trace (("si_dma_free()\n"));

#ifdef	DIAGNOSTIC
	if (dh == NULL)
		panic("si_dma_free: no DMA handle");
#endif

	if (ncr_sc->sc_state & NCR_DOINGDMA)
		panic("si_dma_free: free while in progress");

	if (dh->dh_flags & SIDH_BUSY) {
#if 0
		debug(("bp->b_flags=0x%x\n", bp->b_flags));
		if (bp->b_flags & B_PHYS) {
#ifdef USE_VMAPBUF
			printf("not unmapping(%x/%x %x/%x %d/%d)...\n", 
			       dh->dh_addr, dh->dh_dvma,
			       bp->b_saveaddr, bp->b_data,
			       bp->b_bcount, dh->dh_maplen);
			/* vunmapbuf(bp, dh->dh_maplen); */
			printf("done.\n");
#endif
			dh->dh_dvma = 0;
		}
#endif
		dh->dh_flags = 0;
	}
	sr->sr_dma_hand = NULL;
}


/*
 * REGBUSY and DMABUSY won't collide since the higher-level driver
 * issues intr_on/intr_off before/after doing DMA. The only problem
 * is to handle RDBUF/WRBUF wrt REGBUSY/DMABUSY
 *
 * There might be race-conditions, but for now we don't care for them...
 */
int 
si_dmaLockBus(ncr_sc, lt)
	struct ncr5380_softc *ncr_sc;
	int lt;			/* Lock-Type */
{
	struct si_softc *sc = (void*)ncr_sc;
	int timeout = 200;	/* wait .2 seconds max. */

	trace(("si_dmaLockBus(%x), cold: %d, current: %x\n", 
	       lt, cold, sc->sc_dflags));

#ifdef POLL_MODE
	if (cold)
		return (0);
#endif

	if ((ncr_sc->sc_current != NULL) && (lt == VSDMA_REGBUSY)) {
		printf("trying to use regs while sc_current is set.\n");
		printf("lt=%x, fl=%x, cur=%x\n", 
		       lt, sc->sc_dflags, ncr_sc->sc_current);
	}
	if ((ncr_sc->sc_current == NULL) && (lt != VSDMA_REGBUSY)) {
		printf("trying to use/prepare DMA without current.\n");
		printf("lt=%x, fl=%x, cur=%x\n", 
		       lt, sc->sc_dflags, ncr_sc->sc_current);
	}

	if ((sc->sc_dflags & VSDMA_LOCKED) == 0) {
		struct si_softc *sc = (struct si_softc *)ncr_sc;
		sc->sc_dflags |= VSDMA_WANTED;
		vsbus_lockDMA(sc->sc_cfargs);
		sc->sc_dflags = VSDMA_LOCKED | lt;
		return (0);
	}

#if 1
	while ((sc->sc_dflags & VSDMA_LCKTYPE) != lt) {
		debug(("busy wait(1)...\n"));
		if (--timeout == 0) {
			printf("timeout in busy-wait(%x %x)\n",
			       lt, sc->sc_dflags);
			sc->sc_dflags &= ~VSDMA_LCKTYPE;
			break;
		}
		delay(1000);
	}
	debug(("busy wait(1) done.\n"));
	sc->sc_dflags |= lt;

#else
	if ((sc->sc_dflags & VSDMA_LCKTYPE) != lt) {
		switch (lt) {

		case VSDMA_RDBUF:
			/* sc->sc_dflags |= VSDMA_IWANTED; */
			debug(("busy wait(1)...\n"));
			while (sc->sc_dflags & 
			       (VSDMA_WRBUF | VSDMA_DMABUSY)) {
				if (--timeout == 0) {
					printf("timeout in busy-wait(1)\n");
					sc->sc_dflags &= ~VSDMA_WRBUF;
					sc->sc_dflags &= ~VSDMA_DMABUSY;
				}
				delay(1000);
			}
			/* sc->sc_dflags &= ~VSDMA_IWANTED; */
			debug(("busy wait(1) done.\n"));
			sc->sc_dflags |= lt;
			break;

		case VSDMA_WRBUF:
			/* sc->sc_dflags |= VSDMA_IWANTED; */
			debug(("busy wait(2)...\n"));
			while (sc->sc_dflags & 
			       (VSDMA_RDBUF | VSDMA_DMABUSY)) {
				if (--timeout == 0) {
					printf("timeout in busy-wait(2)\n");
					sc->sc_dflags &= ~VSDMA_RDBUF;
					sc->sc_dflags &= ~VSDMA_DMABUSY;
				}
				delay(1000);
			}
			/* sc->sc_dflags &= ~VSDMA_IWANTED; */
			debug(("busy wait(2) done.\n"));
			sc->sc_dflags |= lt;
			break;

		case VSDMA_DMABUSY:
			/* sc->sc_dflags |= VSDMA_IWANTED; */
			debug(("busy wait(3)...\n"));
			while (sc->sc_dflags & 
			       (VSDMA_RDBUF | VSDMA_WRBUF)) {
				if (--timeout == 0) {
					printf("timeout in busy-wait(3)\n");
					sc->sc_dflags &= ~VSDMA_RDBUF;
					sc->sc_dflags &= ~VSDMA_WRBUF;
				}
				delay(1000);
			}
			/* sc->sc_dflags &= ~VSDMA_IWANTED; */
			debug(("busy wait(3) done.\n"));
			sc->sc_dflags |= lt;
			break;

		case VSDMA_REGBUSY:
			/* sc->sc_dflags |= VSDMA_IWANTED; */
			debug(("busy wait(4)...\n"));
			while (sc->sc_dflags & 
			       (VSDMA_RDBUF | VSDMA_WRBUF | VSDMA_DMABUSY)) {
				if (--timeout == 0) {
					printf("timeout in busy-wait(4)\n");
					sc->sc_dflags &= ~VSDMA_RDBUF;
					sc->sc_dflags &= ~VSDMA_WRBUF;
					sc->sc_dflags &= ~VSDMA_DMABUSY;
				}
				delay(1000);
			}
			/* sc->sc_dflags &= ~VSDMA_IWANTED; */
			debug(("busy wait(4) done.\n"));
			sc->sc_dflags |= lt;
			break;

		default:
			printf("illegal lockType %x in si_dmaLockBus()\n");
		}
	}
	else
		printf("already locked. (%x/%x)\n", lt, sc->sc_dflags);
#endif
	if (sc->sc_dflags & lt) /* successfully locked for this type */
		return (0);

	printf("spurious %x in si_dmaLockBus(%x)\n", lt, sc->sc_dflags);
}

/*
 * the lock of this type is no longer needed. If all (internal) locks are
 * released, release the DMA bus.
 */
int 
si_dmaReleaseBus(ncr_sc, lt)
	struct ncr5380_softc *ncr_sc;
	int lt;			/* Lock-Type */
{
	struct si_softc *sc = (void*)ncr_sc;

	trace(("si_dmaReleaseBus(%x), cold: %d, current: %x\n", 
	       lt, cold, sc->sc_dflags));

#ifdef POLL_MODE
	if (cold)
		return (0);
#endif

	if ((sc->sc_dflags & VSDMA_LCKTYPE) == lt) {
		sc->sc_dflags &= ~lt;
	}
	else
		printf("trying to release %x while flags = %x\n", lt,
		       sc->sc_dflags);

	if (sc->sc_dflags == VSDMA_LOCKED) {	/* no longer needed */
		struct si_softc *sc = (struct si_softc *)ncr_sc;
		vsbus_unlockDMA(sc->sc_cfargs);
		sc->sc_dflags = 0;
		return (0);
	}
}

/*
 * Just toggle the type of lock without releasing the lock...
 * This is usually needed before/after bcopy() to/from DMA-buffer
 */
int 
si_dmaToggleLock(ncr_sc, lt1, lt2)
	struct ncr5380_softc *ncr_sc;
	int lt1, lt2;		/* Lock-Type */
{
	struct si_softc *sc = (void*)ncr_sc;

#ifdef POLL_MODE
	if (cold)
		return (0);
#endif

	if (((sc->sc_dflags & lt1) != 0) &&
	    ((sc->sc_dflags & lt2) == 0)) {
		sc->sc_dflags |= lt2;
		sc->sc_dflags &= ~lt1;
		return (0);
	}
	printf("cannot toggle locking from %x to %x (current = %x)\n",
	       lt1, lt2, sc->sc_dflags);
}

/*
 * This is called when the bus is going idle,
 * so we want to enable the SBC interrupts.
 * That is controlled by the DMA enable!
 * Who would have guessed!
 * What a NASTY trick!
 */
void
si_intr_on(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	si_dmaReleaseBus(ncr_sc, VSDMA_REGBUSY);
}

/*
 * This is called when the bus is idle and we are
 * about to start playing with the SBC chip.
 *
 * VS2000 note: we have four kinds of access which are mutually exclusive: 
 * - access to the NCR5380 registers
 * - access to the HDC9224 registers
 * - access to the DMA area 
 * - doing DMA
 */
void
si_intr_off(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	si_dmaLockBus(ncr_sc, VSDMA_REGBUSY);
}

/*****************************************************************
 * VME functions for DMA
 ****************************************************************/


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
si_dma_setup(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	trace (("si_dma_setup(ncr_sc) !!!\n"));

	/*
	 * VS2000: nothing to do ...
	 */
}


void
si_dma_start(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	struct si_dma_handle *dh = sr->sr_dma_hand;
	volatile struct si_regs *si = sc->sc_regs;
	long data_pa;
	int xlen;

	trace(("si_dma_start(%x)\n", sr->sr_dma_hand));

	/*
	 * we always transfer from/to base of DMA-area,
	 * thus the DMA-address is always the same, only size
	 * and direction matter/differ on VS2000
	 */

	debug(("ncr_sc->sc_datalen = %d\n", ncr_sc->sc_datalen));
	xlen = ncr_sc->sc_datalen;
	dh->dh_xlen = xlen;

	/*
	 * VS2000 has a fixed 16KB-area where DMA is restricted to. 
	 * All DMA-addresses are relative to this base: KA410_DMA_BASE
	 * Thus we need to copy the data into this area when writing,
	 * or copy from this area when reading. (kind of bounce-buffer)
	 */

	/* Set direction (send/recv) */
	if (dh->dh_flags & SIDH_OUT) {
		/*
		 * We know that we are called while intr_off (regs locked)
		 * thus we toggle the lock from REGBUSY to WRBUF
		 * also we set the BLOCKIT flag, so that the locking of
		 * the DMA bus won't be released to the HDC9224...
		 */
		debug(("preparing msg-out (bcopy)\n"));
		si_dmaToggleLock(ncr_sc, VSDMA_REGBUSY, VSDMA_WRBUF);
		bcopy(dh->dh_dvma, sc->sc_dbase, xlen);
		si_dmaToggleLock(ncr_sc, VSDMA_WRBUF, VSDMA_REGBUSY);
		*sc->sc_ddreg = DMA_DIR_OUT;
	} 
	else {
		debug(("preparing data-in (bzero)\n"));
		/* bzero(sc->sc_dbase, xlen); */
		*sc->sc_ddreg = DMA_DIR_IN;
	}
	sc->sc_dflags |= VSDMA_BLOCKED;

	*sc->sc_dareg = DMA_ADDR_HBYTE; /* high byte (6 bits) */
	*sc->sc_dareg = DMA_ADDR_LBYTE; /* low byte */
	*sc->sc_dcreg = 0 - xlen; /* bertram XXX */

#ifdef	DEBUG
	if (si_debug & 2) {
		printf("si_dma_start: dh=0x%x, pa=0x%x, xlen=%d, creg=0x%x\n",
			   dh, data_pa, xlen, *sc->sc_dcreg);
	}
#endif

#ifdef POLL_MODE
	debug(("dma_start: cold=%d\n", cold));
	if (cold) {
		*sc->intmsk &= ~sc->intbit;
		*sc->intclr = sc->intbit;
	}
	else
		*sc->intmsk |= sc->intbit;
#endif
	/*
	 * Acknowledge the phase change.  (After DMA setup!)
	 * Put the SBIC into DMA mode, and start the transfer.
	 */
	si_dmaToggleLock(ncr_sc, VSDMA_REGBUSY, VSDMA_DMABUSY);
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
		*ncr_sc->sci_irecv = 0; /* start it */
	}
	ncr_sc->sc_state |= NCR_DOINGDMA;
	/*
	 * having a delay (eg. printf) here, seems to solve the problem.
	 * Isn't that strange ????
	 * Maybe the higher-level driver accesses one of the registers of
	 * the controller while DMA is in progress. Having a long enough
	 * delay here might prevent/delay this access until DMA bus is
	 * free again...
	 *
	 * The instruction ++++ printf("DMA started.\n"); ++++ 
	 * is long/slow enough, to make the SSCI driver work. Thus we
	 * try to find a delay() long/slow enough to do the same. The
	 * argument to this delay is relative to the transfer-count.
	 */
	delay(3*xlen/4);		/* XXX solve this problem!!! XXX */

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
	trace (("si_vme_dma_eop() !!!\n"));
	/* Not needed - DMA was stopped prior to examining sci_csr */
}

/*
 * si_dma_stop() has now become almost a nop-routine, since DMA-buffer
 * has already been read within si_intr(), so there's nothing left to do.
 */
void
si_dma_stop(ncr_sc)
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

	/* Note that timeout may have set the error flag. */
	if (ncr_sc->sc_state & NCR_ABORTING) {
		printf("si_dma_stop: timeout?\n");
		goto out;
	}

	/*
	 * Now try to figure out how much actually transferred
	 */
	si_dmaLockBus(ncr_sc, VSDMA_DMABUSY);
	si_dmaToggleLock(ncr_sc, VSDMA_DMABUSY, VSDMA_REGBUSY);
	resid = *sc->sc_dcreg;
	/* 
	 * XXX: don't correct at two places !!! 
	 */
	if (resid == 1 && sc->sc_xflags) {	
		resid = 0;
	}
	ntrans = dh->dh_xlen + resid;
	if (resid != 0) 
		printf("resid=%d, xlen=%d, ntrans=%d\n", 
		       resid, dh->dh_xlen, ntrans);

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

	/*
	 * On VS2000 in case of a READ-operation, we must now copy 
	 * the buffer-contents to the destination-address!
	 */
	if ((dh->dh_flags & SIDH_OUT) == 0 &&
	    (dh->dh_flags & SIDH_DONE) == 0) {
		printf("DMA buffer not yet copied.\n");
		si_dmaToggleLock(ncr_sc, VSDMA_REGBUSY, VSDMA_RDBUF);
		bcopy(sc->sc_dbase, dh->dh_dvma, ntrans);
		si_dmaToggleLock(ncr_sc, VSDMA_RDBUF, VSDMA_REGBUSY);
	}
	si_dmaReleaseBus(ncr_sc, VSDMA_REGBUSY);

	/* Adjust data pointer */
	ncr_sc->sc_dataptr += ntrans;
	ncr_sc->sc_datalen -= ntrans;

out:
	si_dmaLockBus(ncr_sc, VSDMA_DMABUSY);

	/* Put SBIC back in PIO mode. */
	*ncr_sc->sci_mode &= ~(SCI_MODE_DMA | SCI_MODE_DMA_IE);
	*ncr_sc->sci_icmd = 0;

	si_dmaReleaseBus(ncr_sc, VSDMA_DMABUSY);
}

/*
 * Poll (spin-wait) for DMA completion.
 * Called right after xx_dma_start(), and
 * xx_dma_stop() will be called next.
 */
void
si_dma_poll(ncr_sc)
	struct ncr5380_softc *ncr_sc;
{
	struct si_softc *sc = (struct si_softc *)ncr_sc;
	struct sci_req *sr = ncr_sc->sc_current;
	struct si_dma_handle *dh = sr->sr_dma_hand;
	int i, timeout;

	if (! cold) 
		printf("spurious call of DMA-poll ???");

#ifdef POLL_MODE

	delay(10000);
	trace(("si_dma_poll(%x)\n", *sc->sc_dcreg));

	/*
	 * interrupt-request has been cleared by dma_start, thus
	 * we do nothing else but wait for the intreq to reappear...
	 */

	timeout = 5000;
	for (i=0; i<timeout; i++) {
		if (*sc->intreq & sc->intbit)
			break;
		delay(100);
	}
	if ((*sc->intreq & sc->intbit) == 0) {
		printf("si: DMA timeout (while polling)\n");
		/* Indicate timeout as MI code would. */
		sr->sr_flags |= SR_OVERDUE;
	}
#endif
	return;
}

/*	$OpenBSD: mac68k5380.c,v 1.19 2004/11/26 21:21:24 miod Exp $	*/
/*	$NetBSD: mac68k5380.c,v 1.29 1997/02/28 15:50:50 scottr Exp $	*/

/*
 * Copyright (c) 1995 Allen Briggs
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
 *      This product includes software developed by Allen Briggs
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 * Derived from atari5380.c for the mac68k port of NetBSD.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/dkstat.h>
#include <sys/syslog.h>
#include <sys/buf.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsiconf.h>

/*
 * Include the driver definitions
 */
#include "ncr5380reg.h"

#include <machine/stdarg.h>
#include <machine/viareg.h>

#include "ncr5380var.h"

/*
 * Set the various driver options
 */
#define	NREQ		18	/* Size of issue queue			*/
#define	AUTO_SENSE	1	/* Automatically issue a request-sense 	*/

#define	DRNAME		ncrscsi	/* used in various prints	*/
#undef	DBG_SEL			/* Show the selection process		*/
#undef	DBG_REQ			/* Show enqueued/ready requests		*/
#undef	DBG_NOWRITE		/* Do not allow writes to the targets	*/
#undef	DBG_PIO			/* Show the polled-I/O process		*/
#undef	DBG_INF			/* Show information transfer process	*/
#define	DBG_NOSTATIC		/* No static functions, all in DDB trace*/
#define	DBG_PID		25	/* Keep track of driver			*/
#ifdef DBG_NOSTATIC
#	define	static
#endif
#ifdef DBG_SEL
#	define	DBG_SELPRINT(a,b)	printf(a,b)
#else
#	define DBG_SELPRINT(a,b)
#endif
#ifdef DBG_PIO
#	define DBG_PIOPRINT(a,b,c) 	printf(a,b,c)
#else
#	define DBG_PIOPRINT(a,b,c)
#endif
#ifdef DBG_INF
#	define DBG_INFPRINT(a,b,c)	a(b,c)
#else
#	define DBG_INFPRINT(a,b,c)
#endif
#ifdef DBG_PID
	/* static	char	*last_hit = NULL, *olast_hit = NULL; */
	static char *last_hit[DBG_PID];
#	define	PID(a)	\
	{ int i; \
	  for (i=0; i< DBG_PID-1; i++) \
		last_hit[i] = last_hit[i+1]; \
	  last_hit[DBG_PID-1] = a; }
#else
#	define	PID(a)
#endif

#undef 	REAL_DMA		/* Use DMA if sensible			*/
#define scsi_ipending()		(GET_5380_REG(NCR5380_DMSTAT) & SC_IRQ_SET)
#define fair_to_keep_dma()	1
#define claimed_dma()		1
#define reconsider_dma()
#define	USE_PDMA	1	/* Use special pdma-transfer function	*/
#define MIN_PHYS	0x2000	/* pdma space w/ /DSACK is only 0x2000  */

#define	ENABLE_NCR5380(sc)	cur_softc = sc;

/*
 * softc of currently active controller (well, we only have one for now).
 */

static struct ncr_softc	*cur_softc;

struct scsi_5380 {
	volatile u_char	scsi_5380[8*16]; /* 8 regs, 1 every 16th byte. */
};

extern vm_offset_t	SCSIBase;
static volatile u_char	*ncr		= (volatile u_char *) 0x10000;
static volatile u_char	*ncr_5380_with_drq	= (volatile u_char *)  0x6000;
static volatile u_char	*ncr_5380_without_drq	= (volatile u_char *) 0x12000;

#define SCSI_5380		((struct scsi_5380 *) ncr)
#define GET_5380_REG(rnum)	SCSI_5380->scsi_5380[((rnum)<<4)]
#define SET_5380_REG(rnum,val)	(SCSI_5380->scsi_5380[((rnum)<<4)] = (val))

static int	ncr5380_irq_intr(void *);
static int	ncr5380_drq_intr(void *);
static void	do_ncr5380_drq_intr(void *);

static __inline__ void	scsi_clr_ipend(void);
static		  void	scsi_mach_init(struct ncr_softc *sc);
static		  int	machine_match(struct device *parent,
			    struct cfdata *cf, void *aux,
			    struct cfdriver *cd);
static __inline__ int	pdma_ready(void);
static		  int	transfer_pdma(u_char *phasep, u_char *data,
					u_long *count);

static __inline__ void
scsi_clr_ipend()
{
	int	tmp;

	tmp = GET_5380_REG(NCR5380_IRCV);
	scsi_clear_irq();
}

static void
scsi_mach_init(sc)
	struct ncr_softc	*sc;
{
	static int	initted = 0;

	if (initted++)
		panic("scsi_mach_init called again.");

	ncr		= (volatile u_char *)
			  (SCSIBase + (u_long) ncr);
	ncr_5380_with_drq	= (volatile u_char *)
			  (SCSIBase + (u_int) ncr_5380_with_drq);
	ncr_5380_without_drq	= (volatile u_char *)
			  (SCSIBase + (u_int) ncr_5380_without_drq);

	if (VIA2 == VIA2OFF) {
		scsi_enable = Via1Base + VIA2 * 0x2000 + vIER;
		scsi_flag   = Via1Base + VIA2 * 0x2000 + vIFR;
	} else {
		scsi_enable = Via1Base + VIA2 * 0x2000 + rIER;
		scsi_flag   = Via1Base + VIA2 * 0x2000 + rIFR;
	}

	via2_register_irq(VIA2_SCSIIRQ, ncr5380_irq_intr, sc,
	    sc->sc_dev.dv_xname);
	via2_register_irq(VIA2_SCSIDRQ, ncr5380_drq_intr, sc,
	    sc->sc_dev.dv_xname);
}

static int
machine_match(parent, cf, aux, cd)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
	struct cfdriver *cd;
{
	if (!mac68k_machine.scsi80)
		return 0;
	return 1;
}

#if USE_PDMA
int	pdma_5380_dir = 0;

u_char	*pending_5380_data;
u_long	pending_5380_count;

#define NCR5380_PDMA_DEBUG 1 	/* Maybe we try with this off eventually. */

#if NCR5380_PDMA_DEBUG
int		pdma_5380_sends = 0;
int		pdma_5380_bytes = 0;

void
pdma_stat()
{
	printf("PDMA SCSI: %d xfers completed for %d bytes.\n",
		pdma_5380_sends, pdma_5380_bytes);
	printf("pdma_5380_dir = %d\t",
		pdma_5380_dir);
	printf("datap = %p, remainder = %ld.\n",
		pending_5380_data, pending_5380_count);
	scsi_show();
}
#endif

void
pdma_cleanup(void)
{
	SC_REQ	*reqp = connected;
	int	s;

	s = splbio();
	PID("pdma_cleanup0");

	pdma_5380_dir = 0;

#if NCR5380_PDMA_DEBUG
	pdma_5380_sends++;
	pdma_5380_bytes+=(reqp->xdata_len - pending_5380_count);
#endif

	/*
	 * Update pointers.
	 */
	reqp->xdata_ptr += reqp->xdata_len - pending_5380_count;
	reqp->xdata_len  = pending_5380_count;

	/*
	 * Reset DMA mode.
	 */
	SET_5380_REG(NCR5380_MODE, GET_5380_REG(NCR5380_MODE) & ~SC_M_DMA);

	/*
	 * Clear any pending interrupts.
	 */
	scsi_clr_ipend();

	/*
	 * Tell interrupt functions that DMA has ended.
	 */
	reqp->dr_flag &= ~DRIVER_IN_DMA;

	SET_5380_REG(NCR5380_MODE, IMODE_BASE);
	SET_5380_REG(NCR5380_ICOM, 0);

	splx(s);

	/*
	 * Back for more punishment.
	 */
	PID("pdma_cleanup1");
	run_main(cur_softc);
	PID("pdma_cleanup2");
}
#endif

static __inline__ int
pdma_ready()
{
#if USE_PDMA
	SC_REQ	*reqp = connected;
	int	dmstat, idstat;
extern	u_char	ncr5380_no_parchk;

	PID("pdma_ready0");
	if (pdma_5380_dir) {
		PID("pdma_ready1.");
		/*
		 * For a phase mis-match, ATN is a "don't care," IRQ is 1 and
		 * all other bits in the Bus & Status Register are 0.  Also,
		 * the current SCSI Bus Status Register has a 1 for BSY and
		 * REQ.  Since we're just checking that this interrupt isn't a
		 * reselection or a reset, we just check for either.
		 */
		dmstat = GET_5380_REG(NCR5380_DMSTAT);
		idstat = GET_5380_REG(NCR5380_IDSTAT);
		if (   ((dmstat & (0xff & ~SC_ATN_STAT)) == SC_IRQ_SET)
		    && ((idstat & (SC_S_BSY|SC_S_REQ))
			== (SC_S_BSY | SC_S_REQ)) ) {
			PID("pdma_ready2");
			pdma_cleanup();
			return 1;
		} else if (PH_IN(reqp->phase) && (dmstat & SC_PAR_ERR)) {
			if (!(ncr5380_no_parchk & (1 << reqp->targ_id)))
				/* XXX: Should be parity error ???? */
				reqp->xs->error = XS_DRIVER_STUFFUP;
			PID("pdma_ready3");
			/* XXX: is this the right reaction? */
			pdma_cleanup();
			return 1;
		} else if (   !(idstat & SC_S_REQ)
			   || (((idstat>>2) & 7) != reqp->phase)) {
#ifdef DIAGNOSTIC
			/* XXX: is this the right reaction? Can this happen? */
			scsi_show();
			printf("Unexpected phase change.\n");
#endif
			reqp->xs->error = XS_DRIVER_STUFFUP;
			pdma_cleanup();
			return 1;
		} else {
			scsi_show();
			panic("Spurious interrupt during PDMA xfer.");
		}
	} else
		PID("pdma_ready4");
#endif
	return 0;
}

static int
ncr5380_irq_intr(p)
	void	*p;
{
	PID("irq");

#if USE_PDMA
	if (pdma_ready()) {
		return (1);
	}
#endif
	scsi_idisable();
	ncr_ctrl_intr(cur_softc);
	return (1);
}

/*
 * This is the meat of the PDMA transfer.
 * When we get here, we shove data as fast as the mac can take it.
 * We depend on several things:
 *   * All macs after the Mac Plus that have a 5380 chip should have a general
 *     logic IC that handshakes data for blind transfers.
 *   * If the SCSI controller finishes sending/receiving data before we do,
 *     the same general logic IC will generate a /BERR for us in short order.
 *   * The fault address for said /BERR minus the base address for the
 *     transfer will be the amount of data that was actually written.
 *
 * We use the nofault flag and the setjmp/longjmp in locore.s so we can
 * detect and handle the bus error for early termination of a command.
 * This is usually caused by a disconnecting target.
 */
static void
do_ncr5380_drq_intr(p)
	void	*p;
{
#if USE_PDMA
extern	int			*nofault, m68k_fault_addr;
	label_t			faultbuf;
	register int		count;
	volatile u_int32_t	*long_drq;
	u_int32_t		*long_data;
	volatile u_int8_t	*drq, tmp_data;
	u_int8_t		*data;

#if DBG_PID
	if (pdma_5380_dir == 2) {
		PID("drq (in)");
	} else {
		PID("drq (out)");
	}
#endif

	/*
	 * Setup for a possible bus error caused by SCSI controller
	 * switching out of DATA-IN/OUT before we're done with the
	 * current transfer.
	 */
	nofault = (int *) &faultbuf;

	if (setjmp((label_t *) nofault)) {
		PID("drq berr");
		nofault = (int *) 0;
		count = (  (u_long) m68k_fault_addr
			 - (u_long) ncr_5380_with_drq);
		if ((count < 0) || (count > pending_5380_count)) {
			printf("pdma %s: cnt = %d (0x%x) (pending cnt %ld)\n",
				(pdma_5380_dir == 2) ? "in" : "out",
				count, count, pending_5380_count);
			panic("something is wrong");
		}

		pending_5380_data += count;
		pending_5380_count -= count;

		m68k_fault_addr = 0;

		PID("end drq early");

		return;
	}

	if (pdma_5380_dir == 2) { /* Data In */
		int	resid;

		/*
		 * Get the dest address aligned.
		 */
		resid = count = min(pending_5380_count,
				    4 - (((int) pending_5380_data) & 0x3));
		if (count && (count < 4)) {
			data = (u_int8_t *) pending_5380_data;
			drq = (u_int8_t *) ncr_5380_with_drq;
			while (count) {
#define R1	*data++ = *drq++
				R1; count--;
#undef R1
			}
			pending_5380_data += resid;
			pending_5380_count -= resid;
		}

		/*
		 * Get ready to start the transfer.
		 */
		while (pending_5380_count) {
		int dcount;

		dcount = count = min(pending_5380_count, MIN_PHYS);
		long_drq = (volatile u_int32_t *) ncr_5380_with_drq;
		long_data = (u_int32_t *) pending_5380_data;

#define R4	*long_data++ = *long_drq++
		while ( count > 64 ) {
			R4; R4; R4; R4; R4; R4; R4; R4;
			R4; R4; R4; R4; R4; R4; R4; R4;	/* 64 */
			count -= 64;
		}
		while (count > 8) {
			R4; R4; count -= 8;
		}
#undef R4
		data = (u_int8_t *) long_data;
		drq = (u_int8_t *) long_drq;
		while (count) {
#define R1	*data++ = *drq++
			R1; count--;
#undef R1
		}
		pending_5380_count -= dcount;
		pending_5380_data += dcount;
		}
	} else {
		int	resid;

		/*
		 * Get the source address aligned.
		 */
		resid = count = min(pending_5380_count,
				    4 - (((int) pending_5380_data) & 0x3));
		if (count && (count < 4)) {
			data = (u_int8_t *) pending_5380_data;
			drq = (u_int8_t *) ncr_5380_with_drq;
			while (count) {
#define W1	*drq++ = *data++
				W1; count--;
#undef W1
			}
			pending_5380_data += resid;
			pending_5380_count -= resid;
		}

		/*
		 * Get ready to start the transfer.
		 */
		while (pending_5380_count) {
		int dcount;

		dcount = count = min(pending_5380_count, MIN_PHYS);
		long_drq = (volatile u_int32_t *) ncr_5380_with_drq;
		long_data = (u_int32_t *) pending_5380_data;

#define W4	*long_drq++ = *long_data++
		while ( count > 64 ) {
			W4; W4; W4; W4; W4; W4; W4; W4;
			W4; W4; W4; W4; W4; W4; W4; W4; /*  64 */
			count -= 64;
		}
		while ( count > 8 ) {
			W4; W4;
			count -= 8;
		}
#undef W4
		data = (u_int8_t *) long_data;
		drq = (u_int8_t *) long_drq;
		while (count) {
#define W1	*drq++ = *data++
			W1; count--;
#undef W1
		}
		pending_5380_count -= dcount;
		pending_5380_data += dcount;
		}

		PID("write complete");

		drq = (volatile u_int8_t *) ncr_5380_with_drq;
		tmp_data = *drq;

		PID("read a byte to force a phase change");
	}

	/*
	 * OK.  No bus error occurred above.  Clear the nofault flag
	 * so we no longer short-circuit bus errors.
	 */
	nofault = (int *) 0;

	PID("end drq");
#endif	/* if USE_PDMA */
}

static int
ncr5380_drq_intr(p)
	void	*p;
{
	int rv = 0;

	while (GET_5380_REG(NCR5380_DMSTAT) & SC_DMA_REQ) {
		do_ncr5380_drq_intr(p);
		scsi_clear_drq();
		rv = 1;
	}

	return (rv);
}

#if USE_PDMA

#define SCSI_TIMEOUT_VAL	10000000

static int
transfer_pdma(phasep, data, count)
	u_char	*phasep;
	u_char	*data;
	u_long	*count;
{
	SC_REQ	*reqp = connected;
	int	len = *count, s, scsi_timeout = SCSI_TIMEOUT_VAL;

	if (pdma_5380_dir) {
		panic("ncrscsi: transfer_pdma called when operation already "
			"pending.");
	}
	PID("transfer_pdma0")

	/*
 	 * Don't bother with PDMA if we can't sleep or for small transfers.
 	 */
	if (reqp->dr_flag & DRIVER_NOINT) {
		PID("pdma, falling back to transfer_pio.")
		transfer_pio(phasep, data, count, 0);
		return -1;
	}

	/*
	 * We are probably already at spl2(), so this is likely a no-op.
	 * Paranoia.
	 */
	s = splbio();

	scsi_idisable();

	/*
	 * Match phases with target.
	 */
	SET_5380_REG(NCR5380_TCOM, *phasep);

	/*
	 * Clear pending interrupts.
	 */
	scsi_clr_ipend();

	/*
	 * Wait until target asserts BSY.
	 */
	while (    ((GET_5380_REG(NCR5380_IDSTAT) & SC_S_BSY) == 0)
		&& (--scsi_timeout) );
	if (!scsi_timeout) {
#ifdef DIAGNOSTIC
		printf("scsi timeout: waiting for BSY in %s.\n",
			(*phasep == PH_DATAOUT) ? "pdma_out" : "pdma_in");
#endif
		goto scsi_timeout_error;
	}

	/*
	 * Tell the driver that we're in DMA mode.
	 */
	reqp->dr_flag |= DRIVER_IN_DMA;

	/*
	 * Load transfer values for DRQ interrupt handlers.
	 */
	pending_5380_data = data;
	pending_5380_count = len;

	/*
	 * Set the transfer function to be called on DRQ interrupts.
	 * And note that we're waiting.
	 */
	switch (*phasep) {
	default:
		panic("Unexpected phase in transfer_pdma.");
	case PH_DATAOUT:
		pdma_5380_dir = 1;
		SET_5380_REG(NCR5380_ICOM, GET_5380_REG(NCR5380_ICOM)|SC_ADTB);
		SET_5380_REG(NCR5380_MODE, GET_5380_REG(NCR5380_MODE)|SC_M_DMA);
		SET_5380_REG(NCR5380_DMSTAT, 0);
		break;
	case PH_DATAIN:
		pdma_5380_dir = 2;
		SET_5380_REG(NCR5380_ICOM, 0);
		SET_5380_REG(NCR5380_MODE, GET_5380_REG(NCR5380_MODE)|SC_M_DMA);
		SET_5380_REG(NCR5380_IRCV, 0);
		break;
	}

	PID("waiting for interrupt.")

	/*
	 * Now that we're set up, enable interrupts and drop processor
	 * priority back down.
	 */
	scsi_ienable();
	splx(s);
	return 0;

scsi_timeout_error:
	/*
	 * Clear the DMA mode.
	 */
	SET_5380_REG(NCR5380_MODE, GET_5380_REG(NCR5380_MODE) & ~SC_M_DMA);
	splx(s);
	return -1;
}
#endif /* if USE_PDMA */

/* Include general routines. */
#include <mac68k/dev/ncr5380.c>

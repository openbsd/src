/*	$NetBSD: ncr5380sbc.c,v 1.1 1995/10/29 21:19:09 gwr Exp $	*/

/*
 * Copyright (c) 1995 David Jones, Gordon W. Ross
 * Copyright (c) 1994 Jarle Greipsland
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
 *      David Jones and Gordon Ross
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
 * This is a machine-independent driver for the NCR5380
 * SCSI Bus Controller (SBC), also known as the Am5380.
 *
 * This code should work with any memory-mapped 5380,
 * and can be shared by multiple adapters that address
 * the 5380 with different register offset spacings.
 * (This can happen on the atari, for example.)
 * For porting/design info. see: ncr5380.doc
 *
 * Credits, history:
 *
 * David Jones is the author of most of the code that now
 * appears in this file, and was the architect of the
 * current overall structure (MI/MD code separation, etc.)
 *
 * Gordon Ross integrated the message phase code, added lots of
 * comments about what happens when and why (re. SCSI spec.),
 * and debugged some reentrance problems.
 *
 * The message in/out code was taken nearly verbatim from
 * the aic6360 driver by Jarle Greipsland.
 *
 * Several other NCR5380 drivers were used for reference
 * while developing this driver, including work by:
 *   The Alice Group (mac68k port) namely:
 *       Allen K. Briggs, Chris P. Caputo, Michael L. Finch,
 *       Bradley A. Grantham, and Lawrence A. Kesteloot
 *   Michael L. Hitch (amiga drivers: sci.c)
 *   Leo Weppelman (atari driver: ncr5380.c)
 * There are others too.  Thanks, everyone.
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
#include <scsi/scsi_message.h>
#include <scsi/scsiconf.h>

#if 0	/* XXX - not yet... */
#include <dev/ic/ncr5380reg.h>
#include <dev/ic/ncr5380var.h>
#else
#include "ncr5380reg.h"
#include "ncr5380var.h"
#endif

static int	ncr5380_wait_req __P((struct ncr5380_softc *));
static int	ncr5380_wait_not_req __P((struct ncr5380_softc *));

static void	ncr5380_sched __P((struct ncr5380_softc *));
static void	ncr5380_done __P((struct ncr5380_softc *));

static int	ncr5380_select
	__P((struct ncr5380_softc *, struct scsi_xfer *));
static void	ncr5380_reselect __P((struct ncr5380_softc *));

static int	ncr5380_msg_in __P((struct ncr5380_softc *));
static int	ncr5380_msg_out __P((struct ncr5380_softc *));
static int	ncr5380_data_xfer __P((struct ncr5380_softc *, int));
static int	ncr5380_command __P((struct ncr5380_softc *));
static int	ncr5380_status __P((struct ncr5380_softc *));
static void	ncr5380_machine __P((struct ncr5380_softc *));

/*
 * Action flags returned by the info_tranfer functions:
 * (These determine what happens next.)
 */
#define ACT_CONTINUE	0x00	/* No flags: expect another phase */
#define ACT_DISCONNECT	0x01	/* Target is disconnecting */
#define ACT_CMD_DONE	0x02	/* Need to call scsi_done() */
#define ACT_ABORT_CMD	0x04	/* Need to forcibly disconnect it */
#define ACT_RESET_BUS	0x08	/* Need bus reset (cmd timeout) */
#define ACT_WAIT_INTR	0x10	/* Wait for interrupt (DMA) */

/*****************************************************************
 * Debugging stuff
 *****************************************************************/

#ifdef DDB
int Debugger();
#else
/* This is used only in recoverable places. */
#define Debugger() printf("Debug: ncr5380.c:%d\n", __LINE__)
#endif

#define DEBUG XXX

#ifdef DEBUG
int ncr5380_debug = 0;
/* extern struct scsi_link *thescsilink; */

#define	NCR_BREAK()	Debugger()
#define	NCR_PRINT(b, s)	\
	do {if ((ncr5380_debug & (b)) != 0) printf s;} while (0)

#else	/* DEBUG */

#define	NCR_BREAK()	/* nah */
#define	NCR_PRINT(b, s)

#endif	/* DEBUG */

#define NCR_MISC(s) 	NCR_PRINT(0x01, s)
#define NCR_MSGS(s) 	NCR_PRINT(0x02, s)
#define NCR_CMDS(s) 	NCR_PRINT(0x04, s)
#define NCR_TRACE(s)	NCR_PRINT(0x10, s)
#define NCR_START(s)	NCR_PRINT(0x20, s)

#ifdef	DEBUG

static char *
phase_names[8] = {
	"DATA_OUT",
	"DATA_IN",
	"COMMAND",
	"STATUS",
	"UNSPEC1",
	"UNSPEC2",
	"MSG_OUT",
	"MSG_IN",
};

static int
ncr5380_show_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	u_char	*b = (u_char *) xs->cmd;
	int	i  = 0;

	if ( ! ( xs->flags & SCSI_RESET ) ) {
		printf("si(%d:%d:%d)-",
			   xs->sc_link->scsibus,
			   xs->sc_link->target,
			   xs->sc_link->lun);
		while (i < xs->cmdlen) {
			if (i) printf(",");
			printf("%x",b[i++]);
		}
		printf("-\n");
	} else {
		printf("si(%d:%d:%d)-RESET-\n",
			   xs->sc_link->scsibus,
			   xs->sc_link->target,
			   xs->sc_link->lun);
	}
}


static void
ncr5380_show_sense(xs)
	struct scsi_xfer *xs;
{
	u_char	*b = (u_char *)&xs->sense;
	int	i;

	printf("sense:");
	for (i = 0; i < sizeof(xs->sense); i++)
		printf(" %02x", b[i]);
	printf("\n");
}
#endif


/*****************************************************************
 * Actual chip control
 *****************************************************************/

/*
 * XXX: These timeouts might need to be tuned...
 */

/* This one is used when waiting for a phase change. (X100uS.) */
int ncr5380_wait_phase_timo = 1000 * 10 * 300;	/* 5 min. */

/* These are used in the following inline functions. */
int ncr5380_wait_req_timo = 1000 * 50;	/* X2 = 100 mS. */
int ncr5380_wait_nrq_timo = 1000 * 25;	/* X2 =  50 mS. */

/* Return zero on success. */
static __inline__ int ncr5380_wait_req(sc)
	struct ncr5380_softc *sc;
{
	register int timo = ncr5380_wait_req_timo;
	for (;;) {
		if (*sc->sci_bus_csr & SCI_BUS_REQ) {
			timo = 0;	/* return 0 */
			break;
		}
		if (--timo < 0)
			break;	/* return -1 */
		delay(2);
	}
	return (timo);
}

/* Return zero on success. */
static __inline__ int ncr5380_wait_not_req(sc)
	struct ncr5380_softc *sc;
{
	register int timo = ncr5380_wait_nrq_timo;
	for (;;) {
		if ((*sc->sci_bus_csr & SCI_BUS_REQ) == 0) {
			timo = 0;	/* return 0 */
			break;
		}
		if (--timo < 0)
			break;	/* return -1 */
		delay(2);
	}
	return (timo);
}

/* Ask the target for a MSG_OUT phase. */
static __inline__ void
ncr_sched_msgout(sc, msg_code)
	struct ncr5380_softc *sc;
	int msg_code;
{
	/* First time, raise ATN line. */
	if (sc->sc_msgpriq == 0) {
		register u_char icmd;
		icmd = *sc->sci_icmd & SCI_ICMD_RMASK;
		*sc->sci_icmd = icmd | SCI_ICMD_ATN;
		delay(2);
	}
	sc->sc_msgpriq |= msg_code;
}


int
ncr5380_pio_out(sc, phase, count, data)
	struct ncr5380_softc *sc;
	int phase, count;
	unsigned char		*data;
{
	register u_char 	icmd;
	register int		resid;

	icmd = *(sc->sci_icmd) & SCI_ICMD_RMASK;

	icmd |= SCI_ICMD_DATA;
	*sc->sci_icmd = icmd;

	resid = count;
	while (resid > 0) {
		if (!SCI_BUSY(sc)) {
			NCR_MISC(("ncr5380_pio_out: lost BSY\n"));
			break;
		}
		if (ncr5380_wait_req(sc)) {
			NCR_MISC(("ncr5380_pio_out: no REQ\n"));
			break;
		}
		if (SCI_CUR_PHASE(*sc->sci_bus_csr) != phase)
			break;

		/* Put the data on the bus. */
		*sc->sci_odata = *data++;

		/* Tell the target it's there. */
		icmd |= SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;

		/* Wait for target to get it. */
		if (ncr5380_wait_not_req(sc)) {
			NCR_MISC(("ncr5380_pio_out: stuck REQ\n"));
			break;
		}

		/* OK, it's got it. */
		icmd &= ~SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;

		--resid;
	}

	/* Stop driving the data bus. */
	icmd &= ~SCI_ICMD_DATA;
	*sc->sci_icmd = icmd;

	return (count - resid);
}


int
ncr5380_pio_in(sc, phase, count, data)
	struct ncr5380_softc *sc;
	int phase, count;
	unsigned char			*data;
{
	register u_char 	icmd;
	register int		resid;

	icmd = *(sc->sci_icmd) & SCI_ICMD_RMASK;

	resid = count;
	while (resid > 0) {
		if (!SCI_BUSY(sc)) {
			NCR_MISC(("ncr5380_pio_in: lost BSY\n"));
			break;
		}
		if (ncr5380_wait_req(sc)) {
			NCR_MISC(("ncr5380_pio_in: no REQ\n"));
			break;
		}
		/* A phase change is not valid until AFTER REQ rises! */
		if (SCI_CUR_PHASE(*sc->sci_bus_csr) != phase)
			break;

		/* Read the data bus. */
		*data++ = *sc->sci_data;

		/* Tell target we got it. */
		icmd |= SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;

		/* Wait for target to drop REQ... */
		if (ncr5380_wait_not_req(sc)) {
			NCR_MISC(("ncr5380_pio_in: stuck REQ\n"));
			break;
		}

		/* OK, we can drop ACK. */
		icmd &= ~SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;

		--resid;
	}

	return (count - resid);
}


void
ncr5380_init(sc)
	struct ncr5380_softc *sc;
{
	int i, j;

	for (i = 0; i < SCI_OPENINGS; i++)
		sc->sc_ring[i].sr_xs = NULL;
	for (i = 0; i < 8; i++)
		for (j = 0; j < 8; j++)
			sc->sc_matrix[i][j] = NULL;

	sc->sc_link.openings = SCI_OPENINGS;
	sc->sc_prevphase = PHASE_INVALID;
	sc->sc_msg_flags = 0;	/* XXX */

	/* XXX: Reselect interrupts... */
	*sc->sci_sel_enb = 0x80;
}


void
ncr5380_reset_scsibus(sc)
	struct ncr5380_softc *sc;
{

	NCR_MISC(("ncr5380_reset_scsibus\n"));

	*sc->sci_icmd = SCI_ICMD_RST;
	delay(500);
	*sc->sci_icmd = 0;

	*sc->sci_mode = 0;
	*sc->sci_tcmd = 0;

	SCI_CLR_INTR(sc);
	/* XXX - Need long delay here! */
	delay(100000);

	/* XXX - Need to cancel disconnected requests. */
}


/*
 * Interrupt handler for the SCSI Bus Controller (SBC)
 * This is also called by a timeout handler after it
 * raises to the appropriate SPL.
 */
int
ncr5380_sbc_intr(sc)
	struct ncr5380_softc *sc;
{
	int claimed = 0;
	u_char st;

	if (sc->sc_dma_flags & DMA5380_INPROGRESS) {
		/* Stop DMA to prevent register conflicts */
		NCR_MISC(("ncr5380_sbc_intr: call DMA stop\n"));
		(*sc->sc_dma_stop)(sc);
		/* Will check for error in ncr5380_machine */
		/* We know the nexus is busy during DMA. */
	}

	/* OK, now we can look at the SBC. */
	st = *sc->sci_csr;
	SCI_CLR_INTR(sc);

	NCR_MISC(("ncr5380_sbc_intr: st=0x%x\n", st));

	/* XXX - There may be a better place to check... */
	if (st & SCI_CSR_PERR) {
		printf("ncr5380_sbc_intr: parity error!\n");
	}

	if (sc->sc_current == NULL) {
		/*
		 *  Might be reselect.  ncr5380_reselect() will check, and
		 *  set up the connection if so.
		 */
		ncr5380_reselect(sc);
	}

	/*
	 * The remaining documented interrupt causes are phase mismatch and
	 * disconnect.  In addition, the sunsi controller may produce a state
	 * where SCI_CSR_DONE is false, yet DMA is complete.
	 *
	 * The procedure in all these cases is to let ncr5380_machine() figure
	 * out what to do next.
	 */
	if (sc->sc_current) {
		/* This will usually free-up the nexus. */
		ncr5380_machine(sc);
		claimed = 1;
	}

	/* Maybe we can start another command now... */
	if (sc->sc_current == NULL)
		ncr5380_sched(sc);

	return claimed;
}


/*****************************************************************
 * Interface to higher level
 *****************************************************************/


/*
 * Enter a new SCSI command into the "issue" queue, and
 * if there is work to do, start it going.
 *
 * WARNING:  This can be called recursively!
 * (see comment in ncr5380_done)
 */
int
ncr5380_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct	ncr5380_softc *sc = xs->sc_link->adapter_softc;
	int s, r, i, flags;
	extern int cold;		/* XXX */

	/* thescsilink = xs->sc_link; */

	flags = xs->flags;
	/*
	 * XXX: Hack: During autoconfig, force polling mode.
	 * Needed as long as sdsize() can be called while cold,
	 * otherwise timeouts will never call back (grumble).
	 */
	if (cold)
		flags |= SCSI_POLL;

	if (flags & SCSI_DATA_UIO)
		panic("ncr5380: scsi data uio requested");

	if (sc->sc_current && (flags & SCSI_POLL))
		panic("ncr5380_scsi_cmd: can't poll while busy");

	s = splbio();

	/* Find lowest empty slot in ring buffer. */
	for (i = 0; i < SCI_OPENINGS; i++)
		if (sc->sc_ring[i].sr_xs == NULL)
			break;

	if (i == SCI_OPENINGS) {
		splx(s);
		return TRY_AGAIN_LATER;
	}

	/* Create queue entry */
	sc->sc_ring[i].sr_xs = xs;
	sc->sc_ring[i].sr_target = xs->sc_link->target;
	sc->sc_ring[i].sr_lun = xs->sc_link->lun;
	sc->sc_ring[i].sr_data = xs->data;
	sc->sc_ring[i].sr_datalen = xs->datalen;
	sc->sc_ring[i].sr_dma_hand = NULL;
	sc->sc_ring[i].sr_flags = (flags & SCSI_POLL) ? SR_IMMED : 0;
	sc->sc_ring[i].sr_status = -1;	/* no value */
	sc->sc_ncmds++;

	/*
	 * Consider starting another command if the bus is idle.
	 * To stop recursion, this is non-NULL when scsi_done()
	 * calls back here to issue the next command.
	 */
	if (sc->sc_current == NULL)
		ncr5380_sched(sc);

	splx(s);

	/* The call to ncr5380_sched() may have finished it. */
	if (xs->flags & ITSDONE) {
		return COMPLETE;
	}

	if (flags & SCSI_POLL)
		panic("ncr5380_scsi_cmd: poll didn't finish");

	return SUCCESSFULLY_QUEUED;
}


/*
 * Schedule a SCSI operation.  This routine should return
 * only after it achieves one of the following conditions:
 *  	Bus is busy (sc->sc_current != NULL)
 *  	All targets are busy (i.e. disconnected)
 */
static void
ncr5380_sched(sc)
	struct	ncr5380_softc *sc;
{
	struct scsi_xfer *xs;
	struct sci_req	*sr;
	int	target, lun;
	int	error, i;

#ifdef	DIAGNOSTIC
	if ((getsr() & PSL_IPL) < PSL_IPL2)
		panic("ncr5380_sched: bad spl");

	if (sc->sc_current)
		panic("ncr5380_sched: bus already busy");
#endif

next_job:
	/*
	 * Grab the next job from queue.
	 *
	 * Always start the search where we last looked.
	 * The REQUEST_SENSE logic depends on this to
	 * choose the same job as was last picked, so it
	 * can just clear sc_current and reschedule.
	 */
	i = sc->sc_rr;
	sr = NULL;
	do {
		if (sc->sc_ring[i].sr_xs) {
			target = sc->sc_ring[i].sr_target;
			lun = sc->sc_ring[i].sr_lun;
			if (sc->sc_matrix[target][lun] == NULL) {
			    sc->sc_matrix[target][lun] = sr = &sc->sc_ring[i];
			    sc->sc_rr = i;
			    break;
			}
		}
		i++;
		if (i == SCI_OPENINGS)
			i = 0;
	} while (i != sc->sc_rr);

	if (sr == NULL)
		return;		/* No more work to do. */

	xs = sr->sr_xs;
	error = ncr5380_select(sc, xs);
	if (sc->sc_current) {
		/* Lost the race!  reselected out from under us! */
		if (sr->sr_flags & SR_IMMED) {
			printf("%s: reselected while polling (reset)\n",
				   sc->sc_dev.dv_xname);
			error = XS_BUSY;
			goto reset;
		}
		/* Work with the reselected target. */
		sr = sc->sc_current;
		xs = sr->sr_xs;
		goto have_nexus;
	}
	sc->sc_current = sr;	/* connected */

	/* Initialize pointers, etc. */
	sc->sc_dataptr  = sr->sr_data;
	sc->sc_datalen  = sr->sr_datalen;
	sc->sc_dma_hand = sr->sr_dma_hand;

	switch (error) {
	case XS_NOERROR:
		break;

	case XS_BUSY:
	reset:
		/* XXX - Reset and try again. */
		printf("%s: SCSI bus busy, resetting...\n",
			   sc->sc_dev.dv_xname);
		ncr5380_reset_scsibus(sc);
		/* fallthrough */
	case XS_SELTIMEOUT:
	default:
		NCR_MISC(("ncr5380_sched: select error=%d\n", error));
		xs->error = error;
		ncr5380_done(sc);
		goto next_job;
	}

	/*
	 * Selection was successful.
	 * We now have a new command.
	 */
	NCR_CMDS(("next cmd: ",
			  ncr5380_show_scsi_cmd(xs) ));
	sc->sc_prevphase = PHASE_MSG_IN;
	if (xs->flags & SCSI_RESET) {
		sc->sc_msgpriq = SEND_DEV_RESET;
		goto have_nexus;
	}
	sc->sc_msgpriq = SEND_IDENTIFY;
	if (sc->sc_dataptr && sc->sc_dma_alloc &&
		(sc->sc_datalen >= sc->sc_min_dma_len))
	{
		/* Allocate DMA space */
		sc->sc_dma_flags = 0;
		if (xs->flags & SCSI_DATA_OUT)
			sc->sc_dma_flags |= DMA5380_WRITE;
		if (xs->bp && (xs->bp->b_flags & B_PHYS))
			sc->sc_dma_flags |= DMA5380_PHYS;
		if (sr->sr_flags & SR_IMMED)
			sc->sc_dma_flags |= DMA5380_POLL;
		/* This may also set DMA5380_POLL */
		(*sc->sc_dma_alloc)(sc);
		/* Now, _maybe_ sc_dma_hand is set. */
	}
	sr->sr_dma_hand = sc->sc_dma_hand;

have_nexus:
	ncr5380_machine(sc);

	/*
	 * What state did ncr5380_machine() leave us in?
	 * Hopefully it sometimes completes a job...
	 */
	if (sc->sc_current)
		return;		/* Have work in progress... */

	goto next_job;
}


/*
 * POST PROCESSING OF SCSI_CMD (usually current)
 * Called by ncr5380_sched(), ncr5380_machine()
 */
static void
ncr5380_done(sc)
	struct ncr5380_softc *sc;
{
	struct	sci_req *sr = sc->sc_current;
	struct	scsi_xfer *xs = sr->sr_xs;

	/* Clean up DMA associated with this command */
	if (sc->sc_dma_hand) {
		(*sc->sc_dma_free)(sc);
		sc->sc_dma_hand = NULL;
	}

	if (xs->error)
		goto finish;

	NCR_MISC(("ncr5380_done, sr_status=%d\n", sr->sr_status));

	switch (sr->sr_status) {
	case SCSI_OK:	/* 0 */
		if (sr->sr_flags & SR_SENSE) {
			/* ncr5380_show_sense(xs); */
			xs->error = XS_SENSE;
		}
		break;

	case SCSI_CHECK:
		if (sr->sr_flags & SR_SENSE) {
			/* Sense command also asked for sense? */
			printf("ncr5380_done: sense asked for sense\n");
			NCR_BREAK();
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		sr->sr_flags |= SR_SENSE;
		NCR_MISC(("ncr5380_done: start request sense\n"));
		NCR_CMDS(("cmd: ", ncr5380_show_scsi_cmd(xs) ));
		/*
		 * Leave queued, but clear sc_current so we start over
		 * with selection.  Guaranteed to get the same request.
		 */
		sc->sc_current = NULL;
		sc->sc_matrix[sr->sr_target][sr->sr_lun] = NULL;
		return;

	case SCSI_BUSY:
		xs->error = XS_BUSY;
		break;

	case -1:
		/* This is our "impossible" initial value. */
		/* fallthrough */
	default:
		printf("%s: target %d, bad status=%d\n",
			sc->sc_dev.dv_xname, sr->sr_target, sr->sr_status);
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}

finish:

	NCR_MISC(("ncr5380_done: finished target=%d, LUN=%d\n",
			  sr->sr_target, sr->sr_lun));

	/*
	 * Dequeue the finished command
	 */
	sc->sc_matrix[sr->sr_target][sr->sr_lun] = NULL;
	sc->sc_ncmds--;
	sr->sr_xs = NULL;

	/*
	 * Do not clear sc_current quite yet.  The call to scsi_done()
	 * below may recursively call ncr5380_scsi_cmd() and leaving
	 * sc->sc_current != NULL ends the recursion after the new
	 * command is enqueued.
	 */
	xs->flags |= ITSDONE;
	scsi_done(xs);

	/* Allow ncr5380_sched() to be called again. */
	sc->sc_current = NULL;	/* idle */
}


/*
 *  Reselect handler: checks for reselection, and if we are being
 *	reselected, it sets up sc->sc_current.
 *
 *  We are reselected when:
 *	SEL is TRUE
 *	IO  is TRUE
 *	BSY is FALSE
 */
void
ncr5380_reselect(sc)
	struct ncr5380_softc *sc;
{
	int target, lun, phase, timo;
	u_char bus, data, icmd, msg;

#ifdef	DIAGNOSTIC
	if (sc->sc_current)
		panic("ncr5380_reselect: already have nexus!");
#endif

	/*
	 * First, check the select line.
	 * (That has to be set by now.)
	 */
	bus = *(sc->sci_bus_csr);
	if ((bus & SCI_BUS_SEL) == 0) {
		/* Not a selection or reselection. */
		return;
	}

	/*
	 * Now wait for: SEL==1, BSY==0
	 * before reading the data bus.
	 */
	for (timo = 25000;;) {
		if ((bus & SCI_BUS_BSY) == 0)
			break;
		if (--timo <= 0) {
			printf("%s: reselect, BSY stuck, bus=0x%x\n",
				sc->sc_dev.dv_xname, bus);
			/* Not much we can do. Reset the bus. */
			ncr5380_reset_scsibus(sc);
			return;
		}
		delay(10);
		bus = *(sc->sci_bus_csr);
		/* If SEL went away, forget it. */
		if ((bus & SCI_BUS_SEL) == 0)
			return;
		/* Still have SEL, check BSY. */
	}

	/*
	 * Good.  We have SEL=1 and BSY=0.  Now wait for a
	 * "bus settle delay" before we sample the data bus
	 */
	delay(2);
	data = *(sc->sci_data) & 0xFF;
	/* XXX - Should check parity... */

	/*
	 * Is this a reselect (I/O == 1) or have we been
	 * selected as a target? (I/O == 0)
	 */
	if ((bus & SCI_BUS_IO) == 0) {
		printf("%s: selected as target, data=0x%x\n",
			sc->sc_dev.dv_xname, data);
		/* Not much we can do. Reset the bus. */
		ncr5380_reset_scsibus(sc);
		return;
	}

	/*
	 * OK, this is a reselection.
	 */
	for (target = 0; target < 7; target++)
		if (data & (1 << target))
			break;

	if ((data & 0x7F) != (1 << target)) {
		/* No selecting ID? or >2 IDs on bus? */
		printf("%s: bad reselect, data=0x%x\n",
			sc->sc_dev.dv_xname, data);
		return;
	}

	NCR_MISC(("ncr5380_reselect: data=0x%x\n", data));

	/* Raise BSY to acknowledge target reselection. */
	*(sc->sci_icmd) = SCI_ICMD_BSY;

	/* Wait for target to drop SEL. */
	for (timo = 1000;;) {
		bus = *(sc->sci_bus_csr);
		if ((bus & SCI_BUS_SEL) == 0)
			break;	/* success */
		if (--timo <= 0) {
			printf("%s: reselect, SEL stuck, bus=0x%x\n",
				sc->sc_dev.dv_xname, bus);
			/* Not much we can do. Reset the bus. */
			ncr5380_reset_scsibus(sc);
			return;
		}
		delay(5);
	}

	/* Now we drop BSY, and we are connected. */
	*(sc->sci_icmd) = 0;
	*sc->sci_sel_enb = 0;
	SCI_CLR_INTR(sc);

	/*
	 * At this point the target should send an IDENTIFY message,
	 * which will permit us to determine the reselecting LUN.
	 * If not, we assume LUN 0.
	 */
	lun = 0;
	/* Wait for REQ before reading bus phase. */
	if (ncr5380_wait_req(sc)) {
		printf("%s: reselect, no REQ\n",
			   sc->sc_dev.dv_xname);
		/* Try to send an ABORT message. */
		goto abort;
	}
	phase = SCI_CUR_PHASE(*sc->sci_bus_csr);
	if (phase != PHASE_MSG_IN) {
		printf("%s: reselect, phase=%d\n",
			   sc->sc_dev.dv_xname, phase);
		goto abort;
	}

	/* Ack. the new phase. */
	*(sc->sci_tcmd) = phase;

	/* Peek at the message byte without consuming it! */
	msg = *(sc->sci_data);
	if ((msg & 0x80) == 0) {
		printf("%s: reselect, not identify, msg=%d\n",
			   sc->sc_dev.dv_xname, msg);
		goto abort;
	}
	lun = msg & 7;
	
	/* We now know target/LUN.  Do we have the request? */
	sc->sc_current = sc->sc_matrix[target][lun];
	if (sc->sc_current) {
		/* Now consume the IDENTIFY message. */
		ncr5380_pio_in(sc, PHASE_MSG_IN, 1, &msg);
		/* Implicit restore pointers message */
		sc->sc_dataptr = sc->sc_current->sr_data;
		sc->sc_datalen = sc->sc_current->sr_datalen;
		sc->sc_dma_hand = sc->sc_current->sr_dma_hand;
		/* We now have a nexus. */
		return;
	}

	printf("%s: phantom reselect: target=%d, LUN=%d\n",
		   sc->sc_dev.dv_xname, target, lun);
abort:
	/*
	 * Try to send an ABORT message.
	 * Raise ATN, then raise ACK...
	 */
	icmd = SCI_ICMD_ATN;
	*sc->sci_icmd = icmd;
	delay(2);

	/* Now consume the IDENTIFY message. */
	ncr5380_pio_in(sc, PHASE_MSG_IN, 1, &msg);

	/* Finally try to send the ABORT. */
	sc->sc_prevphase = PHASE_MSG_IN;
	sc->sc_msgpriq = SEND_ABORT;
	*(sc->sci_tcmd) = PHASE_MSG_OUT;
	ncr5380_msg_out(sc);

	*sc->sci_sel_enb = 0x80;
}


/*
 *  Select target: xs is the transfer that we are selecting for.
 *  sc->sc_current should be NULL.
 *
 *  Returns:
 *	sc->sc_current != NULL  ==> we were reselected (race!)
 *	XS_NOERROR		==> selection worked
 *	XS_BUSY 		==> lost arbitration
 *	XS_SELTIMEOUT   	==> no response to selection
 */
static int
ncr5380_select(sc, xs)
	struct ncr5380_softc *sc;
	struct scsi_xfer *xs;
{
	int timo;
	u_char bus, data, icmd;

	NCR_MISC(("ncr5380_select: begin, target=%d\n",
			  xs->sc_link->target));

	/* Check for reselect */
	ncr5380_reselect(sc);
	if (sc->sc_current) {
		NCR_MISC(("ncr5380_select: reselected!\n"));
		return XS_NOERROR;	/* reselected */
	}

	/*
	 * Set phase bits to 0, otherwise the 5380 won't drive the bus during
	 * selection.
	 */
	*sc->sci_icmd = icmd = 0;
	*sc->sci_mode = 0;
	*sc->sci_tcmd = 0;

	NCR_MISC(("ncr5380_select: arbitration\n"));

	/*
	 * Arbitrate for the bus.  The 5380 takes care of the
	 * time-critical bus interactions.  We set our ID bit
	 * in the output data register and set MODE_ARB.  The
	 * 5380 watches for the required "bus free" period.
	 * If and when the "bus free" period is detected, the
	 * 5380 then drives BSY, drives the data bus, and sets
	 * the "arbitration in progress" (AIP) bit to let us
	 * know arbitration has started.  We then wait for one
	 * arbitration delay (2.2uS) and check the ICMD_LST bit,
	 * which will be set if someone else drives SEL.
	 */
	*(sc->sci_odata) = 0x80;	/* OUR_ID */
	*(sc->sci_mode) = SCI_MODE_ARB;

	/* Wait for ICMD_AIP. */
	for (timo = 50000;;) {
		if (*(sc->sci_icmd) & SCI_ICMD_AIP)
			break;
		if (--timo <= 0) {
			/* Did not see any "bus free" period. */
			NCR_MISC(("ncr5380_select: failed, bus busy\n"));
			*sc->sci_mode = 0;
			return XS_BUSY;
		}
		delay(2);
	}
	/* Got AIP.  Wait one arbitration delay (2.2 uS.) */
	delay(3);

	/* Check for ICMD_LST */
	if (*(sc->sci_icmd) & SCI_ICMD_LST) {
		/* Some other target asserted SEL. */
		NCR_MISC(("ncr5380_select: lost arbitration\n"));
		*sc->sci_mode = 0;
		return XS_BUSY;
	}

	/*
	 * No other device has declared itself the winner.
	 * The spec. says to check for higher IDs, but we
	 * are always the highest (ID=7) so don't bother.
	 * We can now declare victory by asserting SEL.
	 *
	 * Note that the 5380 is asserting BSY because we
	 * asked it to do arbitration.  We will now hold
	 * BSY directly so we can turn off ARB mode.
	 */
	icmd = (SCI_ICMD_BSY | SCI_ICMD_SEL);
	*sc->sci_icmd = icmd;

	/*
	 * "The SCSI device that wins arbitration shall wait
	 *  at least a bus clear delay plus a bus settle delay
	 *  after asserting the SEL signal before changing
	 *  any [other] signal."  (1.2uS. total)
	 */
	delay(2);

#if 1
	/*
	 * XXX: Check one last time to see if we really
	 * XXX: did win arbitration.  (too paranoid?)
	 */
	if (*(sc->sci_icmd) & SCI_ICMD_LST) {
		NCR_MISC(("ncr5380_select: lost arb. after SEL\n"));
		*sc->sci_icmd = 0;
		*sc->sci_mode = 0;
		return XS_BUSY;
	}
#endif
	/* Leave ARB mode Now that we drive BSY+SEL */
	*sc->sci_mode = 0;
	*sc->sci_sel_enb = 0;

	/*
	 * Arbitration is complete.  Now do selection:
	 * Drive the data bus with the ID bits for both
	 * the host and target.  Also set ATN now, to
	 * ask the target for a messgae out phase.
	 */
	NCR_MISC(("ncr5380_select: selection\n"));
	data = (1 << xs->sc_link->target);
	*(sc->sci_odata) = 0x80 | data;
	icmd |= (SCI_ICMD_DATA | SCI_ICMD_ATN);
	*(sc->sci_icmd) = icmd;
	delay(2);	/* two deskew delays. */

	/* De-assert BSY (targets sample the data now). */
	icmd &= ~SCI_ICMD_BSY;
	*(sc->sci_icmd) = icmd;
	delay(3);	/* Bus settle delay. */

	/*
	 * Wait for the target to assert BSY.
	 * SCSI spec. says wait for 250 mS.
	 */
	for (timo = 25000;;) {
		if (*sc->sci_bus_csr & SCI_BUS_BSY)
			goto success;
		if (--timo <= 0)
			break;
		delay(10);
	}

	/*
	 * There is no reaction from the target.  Start the selection
	 * timeout procedure. We release the databus but keep SEL+ATN
	 * asserted. After that we wait a 'selection abort time' (200
	 * usecs) and 2 deskew delays (90 ns) and check BSY again.
	 * When BSY is asserted, we assume the selection succeeded,
	 * otherwise we release the bus.
	 */
	icmd &= ~SCI_ICMD_DATA;
	*(sc->sci_icmd) = icmd;
	delay(201);
	if ((*sc->sci_bus_csr & SCI_BUS_BSY) == 0) {
		/* Really no device on bus */
		*sc->sci_icmd = 0;
		*sc->sci_mode = 0;
		*sc->sci_sel_enb = 0x80;
		NCR_MISC(("ncr5380_select: device down\n"));
		return XS_SELTIMEOUT;
	}

success:
	/*
	 * The target is now driving BSY, so we can stop
	 * driving SEL and the data bus (keep ATN true).
	 * Configure the ncr5380 to monitor BSY, parity.
	 */
	icmd &= ~(SCI_ICMD_DATA | SCI_ICMD_SEL);
	*sc->sci_icmd = icmd;

	/* XXX - Make parity checking optional? */
	*sc->sci_mode = (SCI_MODE_MONBSY | SCI_MODE_PAR_CHK);

	NCR_MISC(("ncr5380_select: success\n"));
	return XS_NOERROR;
}


/*****************************************************************
 * Functions to handle each info. transfer phase:
 *****************************************************************/

/*
 * The message system:
 *
 * This is a revamped message system that now should easier accomodate
 * new messages, if necessary.
 *
 * Currently we accept these messages:
 * IDENTIFY (when reselecting)
 * COMMAND COMPLETE # (expect bus free after messages marked #)
 * NOOP
 * MESSAGE REJECT
 * SYNCHRONOUS DATA TRANSFER REQUEST
 * SAVE DATA POINTER
 * RESTORE POINTERS
 * DISCONNECT #
 *
 * We may send these messages in prioritized order:
 * BUS DEVICE RESET #		if SCSI_RESET & xs->flags (or in weird sits.)
 * MESSAGE PARITY ERROR		par. err. during MSGI
 * MESSAGE REJECT		If we get a message we don't know how to handle
 * ABORT #			send on errors
 * INITIATOR DETECTED ERROR	also on errors (SCSI2) (during info xfer)
 * IDENTIFY			At the start of each transfer
 * SYNCHRONOUS DATA TRANSFER REQUEST	if appropriate
 * NOOP				if nothing else fits the bill ...
 */

#define IS1BYTEMSG(m) (((m) != 0x01 && (m) < 0x20) || (m) >= 0x80)
#define IS2BYTEMSG(m) (((m) & 0xf0) == 0x20)
#define ISEXTMSG(m) ((m) == 0x01)

/*
 * Precondition:
 * The SCSI bus is already in the MSGI phase and there is a message byte
 * on the bus, along with an asserted REQ signal.
 *
 * Our return value determines whether our caller, ncr5380_machine()
 * will expect to see another REQ (and possibly phase change).
 */
static int
ncr5380_msg_in(sc)
	register struct ncr5380_softc *sc;
{
	struct sci_req *sr = sc->sc_current;
	int n, phase, timo;
	int act_flags;
	register u_char icmd;

	act_flags = ACT_CONTINUE;
	icmd = *sc->sci_icmd & SCI_ICMD_RMASK;

	if (sc->sc_prevphase == PHASE_MSG_IN) {
		/* This is a continuation of the previous message. */
		n = sc->sc_imp - sc->sc_imess;
		goto nextbyte;
	}

	/* This is a new MESSAGE IN phase.  Clean up our state. */
	sc->sc_msg_flags &= ~NCR_DROP_MSGIN;

nextmsg:
	n = 0;
	sc->sc_imp = &sc->sc_imess[n];

nextbyte:
	/*
	 * Read a whole message, but don't ack the last byte.  If we reject the
	 * message, we have to assert ATN during the message transfer phase
	 * itself.
	 */
	for (;;) {
		/*
		 * Read a message byte.
		 * First, check BSY, REQ, phase...
		 */
		if (!SCI_BUSY(sc)) {
			NCR_MISC(("ncr5380_msg_in: lost BSY\n"));
			/* XXX - Assume the command completed? */
			act_flags |= (ACT_DISCONNECT | ACT_CMD_DONE);
			return (act_flags);
		}
		if (ncr5380_wait_req(sc)) {
			NCR_MISC(("ncr5380_msg_in: BSY but no REQ\n"));
			/* XXX - Try asserting ATN...? */
			act_flags |= ACT_RESET_BUS;
			return (act_flags);
		}
		phase = SCI_CUR_PHASE(*sc->sci_bus_csr);
		if (phase != PHASE_MSG_IN) {
			/*
			 * Target left MESSAGE IN, probably because it
			 * a) noticed our ATN signal, or
			 * b) ran out of messages.
			 */
			return (act_flags);
		}
		/* Still in MESSAGE IN phase, and REQ is asserted. */
		if (*sc->sci_csr & SCI_CSR_PERR) {
			ncr_sched_msgout(sc, SEND_PARITY_ERROR);
			sc->sc_msg_flags |= NCR_DROP_MSGIN;
		}

		/* Gather incoming message bytes if needed. */
		if ((sc->sc_msg_flags & NCR_DROP_MSGIN) == 0) {
			if (n >= NCR_MAX_MSG_LEN) {
				ncr_sched_msgout(sc, SEND_REJECT);
				sc->sc_msg_flags |= NCR_DROP_MSGIN;
			} else {
				*sc->sc_imp++ = *sc->sci_data;
				n++;
				/*
				 * This testing is suboptimal, but most
				 * messages will be of the one byte variety, so
				 * it should not affect performance
				 * significantly.
				 */
				if (n == 1 && IS1BYTEMSG(sc->sc_imess[0]))
					goto have_msg;
				if (n == 2 && IS2BYTEMSG(sc->sc_imess[0]))
					goto have_msg;
				if (n >= 3 && ISEXTMSG(sc->sc_imess[0]) &&
					n == sc->sc_imess[1] + 2)
					goto have_msg;
			}
		}

		/*
		 * If we reach this spot we're either:
		 * a) in the middle of a multi-byte message, or
		 * b) dropping bytes.
		 */

		/* Ack the last byte read. */
		icmd |= SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;

		if (ncr5380_wait_not_req(sc)) {
			NCR_MISC(("ncr5380_msg_in: stuck REQ\n"));
			act_flags |= ACT_RESET_BUS;
			return (act_flags);
		}

		icmd &= ~SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;
		/* back to nextbyte */
	}

have_msg:
	/* We now have a complete message.  Parse it. */
	NCR_MSGS(("n=%d imess=0x%02x  ", n, sc->sc_imess[0]));

	/* Make sure we are connected. */
	if (sc->sc_current == NULL) {
		printf("%s: unexpected MESSAGE IN; sending DEVICE RESET\n",
			sc->sc_dev.dv_xname);
		NCR_BREAK();
		goto reset;
	}
	sr = sc->sc_current;

	switch (sc->sc_imess[0]) {
	case MSG_CMDCOMPLETE:
		/* sc->sc_state = NCR_CMDCOMPLETE; */
		if (sc->sc_datalen < 0) {
			printf("%s: %d extra bytes from %d:%d\n",
				   sc->sc_dev.dv_xname, -sc->sc_datalen,
				   sr->sr_target, sr->sr_lun);
			sc->sc_datalen = 0;
		}
		/* Target is about to disconnect. */
		NCR_MISC(("ncr5380_msg_in: cmdcomplete\n"));
		act_flags |= (ACT_DISCONNECT | ACT_CMD_DONE);
		break;

	case MSG_DISCONNECT:
		/* sc->sc_state = NCR_DISCONNECT; */
		/* Target is about to disconnect. */
		NCR_MISC(("ncr5380_msg_in: disconnect\n"));
		act_flags |= ACT_DISCONNECT;
		break;

	case MSG_PARITY_ERROR:
		/* Resend the last message. */
		ncr_sched_msgout(sc, sc->sc_msgout);
		break;

	case MSG_MESSAGE_REJECT:
		/* The target rejects the last message we sent. */
		NCR_MSGS(("ncr5380_msg_in: reject\n"));
		switch (sc->sc_msgout) {
		case SEND_IDENTIFY:
			/* Really old target controller? */
			/* XXX ... */
			break;
		case SEND_INIT_DET_ERR:
			goto abort;
		}
		break;

	case MSG_NOOP:
		break;

	case MSG_SAVEDATAPOINTER:
		sr->sr_data = sc->sc_dataptr;
		sr->sr_datalen = sc->sc_datalen;
		sr->sr_dma_hand = sc->sc_dma_hand;
		break;

	case MSG_RESTOREPOINTERS:
		sc->sc_dataptr = sr->sr_data;
		sc->sc_datalen = sr->sr_datalen;
		sc->sc_dma_hand = sr->sr_dma_hand;
		break;

	case MSG_EXTENDED:
		switch (sc->sc_imess[2]) {
		case MSG_EXT_SDTR:
		case MSG_EXT_WDTR:
			/* The ncr5380 can not do synchronous mode. */
			goto reject;
		default:
			printf("%s: unrecognized MESSAGE EXTENDED; sending REJECT\n",
				   sc->sc_dev.dv_xname);
			NCR_BREAK();
			goto reject;
		}
		break;

	default:
		printf("%s: unrecognized MESSAGE; sending REJECT\n",
			   sc->sc_dev.dv_xname);
		NCR_BREAK();
		/* fallthrough */
	reject:
		ncr_sched_msgout(sc, SEND_REJECT);
		break;

	reset:
		sc->sc_msg_flags |= NCR_ABORTING;
		ncr_sched_msgout(sc, SEND_DEV_RESET);
		break;

	abort:
		sc->sc_msg_flags |= NCR_ABORTING;
		ncr_sched_msgout(sc, SEND_ABORT);
		break;
	}

	/* Ack the last byte read. */
	icmd |= SCI_ICMD_ACK;
	*sc->sci_icmd = icmd;

	if (ncr5380_wait_not_req(sc)) {
		NCR_MISC(("ncr5380_msg_in: stuck REQ\n"));
		act_flags |= ACT_RESET_BUS;
	}

	icmd &= ~SCI_ICMD_ACK;
	*sc->sci_icmd = icmd;

	/* Go get the next message, if any. */
	if (act_flags == ACT_CONTINUE)
		goto nextmsg;

	return (act_flags);
}


/*
 * The message out (and in) stuff is a bit complicated:
 * If the target requests another message (sequence) without
 * having changed phase in between it really asks for a
 * retransmit, probably due to parity error(s).
 * The following messages can be sent:
 * IDENTIFY	   @ These 4 stem from SCSI command activity
 * SDTR		   @
 * WDTR		   @
 * DEV_RESET	   @
 * REJECT if MSGI doesn't make sense
 * PARITY_ERROR if parity error while in MSGI
 * INIT_DET_ERR if parity error while not in MSGI
 * ABORT if INIT_DET_ERR rejected
 * NOOP if asked for a message and there's nothing to send
 *
 * Note that we call this one with (sc_current == NULL)
 * when sending ABORT for unwanted reselections.
 */
static int
ncr5380_msg_out(sc)
	register struct ncr5380_softc *sc;
{
	struct sci_req *sr = sc->sc_current;
	int n, phase, resel;
	int progress, act_flags;
	register u_char icmd;

	NCR_TRACE(("ncr_msgout  "));

	progress = 0;	/* did we send any messages? */
	act_flags = ACT_CONTINUE;
	icmd = *sc->sci_icmd & SCI_ICMD_RMASK;

	/*
	 * Set ATN.  If we're just sending a trivial 1-byte message,
	 * we'll clear ATN later on anyway.  Also drive the data bus.
	 */
	icmd |= (SCI_ICMD_ATN | SCI_ICMD_DATA);
	*sc->sci_icmd = icmd;

	if (sc->sc_prevphase == PHASE_MSG_OUT) {
		if (sc->sc_omp == sc->sc_omess) {
			/*
			 * This is a retransmission.
			 *
			 * We get here if the target stayed in MESSAGE OUT
			 * phase.  Section 5.1.9.2 of the SCSI 2 spec indicates
			 * that all of the previously transmitted messages must
			 * be sent again, in the same order.  Therefore, we
			 * requeue all the previously transmitted messages, and
			 * start again from the top.  Our simple priority
			 * scheme keeps the messages in the right order.
			 */
			NCR_MSGS(("ncr5380_msg_out: retransmit\n"));
			sc->sc_msgpriq |= sc->sc_msgoutq;
		} else {
			/* This is a continuation of the previous message. */
			n = sc->sc_omp - sc->sc_omess;
			goto nextbyte;
		}
	}

	/* No messages transmitted so far. */
	sc->sc_msgoutq = 0;

nextmsg:
	/* Pick up highest priority message. */
	sc->sc_msgout = sc->sc_msgpriq & -sc->sc_msgpriq;
	sc->sc_msgpriq &= ~sc->sc_msgout;
	sc->sc_msgoutq |= sc->sc_msgout;

	/* Build the outgoing message data. */
	switch (sc->sc_msgout) {
	case SEND_IDENTIFY:
		/* (sc->sc_state != NCR_CONNECTED) */
		if (sr == NULL) {
			printf("%s: SEND_IDENTIFY while not connected; sending NOOP\n",
				sc->sc_dev.dv_xname);
			NCR_BREAK();
			goto noop;
		}
		resel = (sc->sc_flags & NCR5380_PERMIT_RESELECT) ? 1 : 0;
		resel &= (sr->sr_flags & (SR_IMMED | SR_SENSE)) ? 0 : 1;
		sc->sc_omess[0] = MSG_IDENTIFY(sr->sr_lun, resel);
		n = 1;
		break;

	case SEND_DEV_RESET:
		/* Expect disconnect after this! */
		/* XXX: Kill jobs for this target? */
		act_flags |= (ACT_DISCONNECT | ACT_CMD_DONE);
		sc->sc_omess[0] = MSG_BUS_DEV_RESET;
		n = 1;
		break;

	case SEND_REJECT:
		sc->sc_omess[0] = MSG_MESSAGE_REJECT;
		n = 1;
		break;

	case SEND_PARITY_ERROR:
		sc->sc_omess[0] = MSG_PARITY_ERROR;
		n = 1;
		break;

	case SEND_INIT_DET_ERR:
		sc->sc_omess[0] = MSG_INITIATOR_DET_ERR;
		n = 1;
		break;

	case SEND_ABORT:
		/* Expect disconnect after this! */
		/* XXX: Set error flag? */
		act_flags |= (ACT_DISCONNECT | ACT_CMD_DONE);
		sc->sc_omess[0] = MSG_ABORT;
		n = 1;
		break;

	case 0:
		printf("%s: unexpected MESSAGE OUT; sending NOOP\n",
			sc->sc_dev.dv_xname);
		NCR_BREAK();
	noop:
		sc->sc_omess[0] = MSG_NOOP;
		n = 1;
		break;

	default:
		printf("%s: weird MESSAGE OUT; sending NOOP\n",
			sc->sc_dev.dv_xname);
		NCR_BREAK();
		goto noop;
	}
	sc->sc_omp = &sc->sc_omess[n];

nextbyte:
	/* Send message bytes. */
	while (n > 0) {
		/*
		 * Send a message byte.
		 * First check BSY, REQ, phase...
		 */
		if (!SCI_BUSY(sc)) {
			NCR_MISC(("ncr5380_msg_out: lost BSY\n"));
			goto out;
		}
		if (ncr5380_wait_req(sc)) {
			NCR_MISC(("ncr5380_msg_out: no REQ\n"));
			goto out;
		}
		phase = SCI_CUR_PHASE(*sc->sci_bus_csr);
		if (phase != PHASE_MSG_OUT) {
			/*
			 * Target left MESSAGE OUT, possibly to reject
			 * our message.
			 */
			goto out;
		}

		/* Yes, we can send this message byte. */
		--n;

		/* Clear ATN before last byte if this is the last message. */
		if (n == 0 && sc->sc_msgpriq == 0) {
			icmd &= ~SCI_ICMD_ATN;
			*sc->sci_icmd = icmd;
			/* 2 deskew delays */
			delay(2);	/* XXX */
		}

		/* Put data on the bus. */
		*sc->sci_odata = *--sc->sc_omp;

		/* Raise ACK to tell target data is on the bus. */
		icmd |= SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;

		/* Wait for REQ to be negated. */
		if (ncr5380_wait_not_req(sc)) {
			NCR_MISC(("ncr5380_msg_out: stuck REQ\n"));
			act_flags |= ACT_RESET_BUS;
			goto out;
		}

		/* Finally, drop ACK. */
		icmd &= ~SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;
	}
	progress++;

	/* We get here only if the entire message has been transmitted. */
	if ((sc->sc_msgpriq != 0) && (act_flags == ACT_CONTINUE)) {
		/* There are more outgoing messages. */
		goto nextmsg;
	}

	/*
	 * The last message has been transmitted.  We need to remember the last
	 * message transmitted (in case the target switches to MESSAGE IN phase
	 * and sends a MESSAGE REJECT), and the list of messages transmitted
	 * this time around (in case the target stays in MESSAGE OUT phase to
	 * request a retransmit).
	 */

out:
	/* Stop driving the data bus. */
	icmd &= ~SCI_ICMD_DATA;
	*sc->sci_icmd = icmd;

	if (!progress)
		act_flags |= ACT_RESET_BUS;

	return (act_flags);
}


/*
 * Handle command phase.
 */
static int
ncr5380_command(sc)
	struct ncr5380_softc *sc;
{
	int len;
	struct scsi_sense rqs;
	struct sci_req *sr = sc->sc_current;
	struct scsi_xfer *xs = sr->sr_xs;

	if (sr->sr_flags & SR_SENSE) {
		rqs.opcode = REQUEST_SENSE;
		rqs.byte2 = xs->sc_link->lun << 5;
		rqs.length = sizeof(xs->sense);

		rqs.unused[0] = rqs.unused[1] = rqs.control = 0;
		len = ncr5380_pio_out(sc, PHASE_CMD, sizeof(rqs),
			(u_char *)&rqs);
	}
	else {
		/* Assume command can be sent in one go. */
		/* XXX: Do this using DMA, and get a phase change intr? */
		len = ncr5380_pio_out(sc, PHASE_CMD, xs->cmdlen,
			(u_char *)xs->cmd);
	}

	if (len != xs->cmdlen) {
		printf("ncr5380_command: short transfer: wanted %d got %d.\n",
			   xs->cmdlen, len);
		NCR_BREAK();
		if (len < 6)
			return ACT_RESET_BUS;
	}

	return ACT_CONTINUE;
}


/*
 * Handle either data_in or data_out
 */
static int
ncr5380_data_xfer(sc, phase)
	struct ncr5380_softc *sc;
	int phase;
{
	struct sci_req *sr = sc->sc_current;
	struct scsi_xfer *xs = sr->sr_xs;
	int expected_phase;
	int i, len;

	if (sc->sc_msg_flags & NCR_ABORTING) {
		sc->sc_msg_flags &= ~NCR_ABORTING;
		printf("%s: data phase after abort\n",
			   sc->sc_dev.dv_xname);
		return ACT_RESET_BUS;
	}

	if (sr->sr_flags & SR_SENSE) {
		if (phase != PHASE_DATA_IN) {
			printf("%s: sense phase error\n", sc->sc_dev.dv_xname);
			return (ACT_ABORT_CMD);
		}
		NCR_MISC(("ncr5380_data_xfer: reading sense data\n"));
		len = ncr5380_pio_in(sc, phase, sizeof(xs->sense),
				(u_char *)&xs->sense);
		return ACT_CONTINUE;	/* XXX */
	}

	/* Validate expected phase (data_in or data_out) */
	expected_phase = (xs->flags & SCSI_DATA_OUT) ?
		PHASE_DATA_OUT : PHASE_DATA_IN;
	if (phase != expected_phase) {
		printf("%s: data phase error\n", sc->sc_dev.dv_xname);
		return (ACT_ABORT_CMD);
	}

	/* Make sure we have some data to move. */
	if (sc->sc_datalen <= 0) {
		printf("%s: can not transfer more data\n");
		return (ACT_ABORT_CMD);
	}
	NCR_MISC(("ncr5380_data_xfer: todo=%d\n", sc->sc_datalen));

	/*
	 * Attempt DMA only if dma_alloc gave us a DMA handle AND
	 * there is enough left to transfer so DMA is worth while.
	 * Note that we can come back here after a DMA transfer moves
	 * all but the last few bytes of a request, in which case
	 * we should finish the request using PIO.  In this case
	 * there WILL be a dma_handle, but we should not use it.
	 * Data alignment was checked in DMA alloc.
	 */
	if (sc->sc_dma_hand &&
		(sc->sc_datalen >= sc->sc_min_dma_len))
	{
		/* OK, really do DMA. */
		(*sc->sc_dma_start)(sc);
		if (sc->sc_dma_flags & DMA5380_POLL) {
			(*sc->sc_dma_poll)(sc);
			(*sc->sc_dma_stop)(sc);
		}
		if (sc->sc_dma_flags & DMA5380_INPROGRESS) {
			/* Interrupt will let us continue */
			NCR_MISC(("ncr5380_data_xfer: expect DMA intr.\n"));
			return ACT_WAIT_INTR;
		}
		/* Must have done polled DMA. */
		SCI_CLR_INTR(sc);	/* XXX */
		/* Let _machine call us again... */
		return ACT_CONTINUE;
	}

	NCR_MISC(("ncr5380_data_xfer: doing PIO, len=%d\n", sc->sc_datalen));
	if (phase == PHASE_DATA_OUT) {
		len = ncr5380_pio_out(sc, phase, sc->sc_datalen, sc->sc_dataptr);
	} else {
		len = ncr5380_pio_in (sc, phase, sc->sc_datalen, sc->sc_dataptr);
	}
	sc->sc_dataptr += len;
	sc->sc_datalen -= len;

	NCR_MISC(("ncr5380_data_xfer: did PIO, resid=%d\n", sc->sc_datalen));
	return (ACT_CONTINUE);
}


static int
ncr5380_status(sc)
	struct ncr5380_softc *sc;
{
	int len;
	u_char status;
	struct sci_req *sr = sc->sc_current;
	struct scsi_xfer *xs = sr->sr_xs;

	len = ncr5380_pio_in(sc, PHASE_STATUS, 1, &status);

	if (len) {
		sr->sr_status = status;
	} else {
		printf("ncr5380_status: none?\n");
	}
	return ACT_CONTINUE;
}


/*
 *  This is the big state machine that follows SCSI phase changes.
 *  This is somewhat like a co-routine.  It will do a SCSI command,
 *  and exit if the command is complete, or if it must wait (usually
 *  for DMA).
 *
 *  The bus must be selected, and we need to know which command is
 *  being undertaken.
 */
static void
ncr5380_machine(sc)
	struct ncr5380_softc *sc;
{
	struct sci_req *sr = sc->sc_current;
	struct scsi_xfer *xs = sr->sr_xs;
	int phase, act_flags, timo;

#ifdef	DIAGNOSTIC
	if ((getsr() & PSL_IPL) < PSL_IPL2)
		panic("ncr5380_machine: bad spl");
#endif

	act_flags = ACT_CONTINUE;

next_phase:
	/*
	 * We can arrive back here due to a DMA interrupt
	 * or DMA timeout error.  On error, reset it.
	 * If DMA failed, we probably can't talk to it.
	 */
	if (sc->sc_dma_flags & DMA5380_ERROR) {
		printf("ncr5380: DMA error\n");
		NCR_BREAK();
		/* Make sure we don't keep trying it... */
		if (sc->sc_dma_hand) {
			(*sc->sc_dma_free)(sc);
			sc->sc_dma_hand = NULL;
		}
		act_flags |= ACT_RESET_BUS;
		goto do_actions;
	}

	if (!SCI_BUSY(sc)) {
		/* Unexpected disconnect */
		printf("ncr5380_machine: unexpected disconnect.\n");
		xs->error = XS_DRIVER_STUFFUP;
		act_flags |= (ACT_DISCONNECT | ACT_CMD_DONE);
		goto do_actions;
	}

	/*
	 * Wait for REQ before reading the phase.
	 * Need to wait longer than usual here, because
	 * some devices are just plain slow...
	 */
	timo = ncr5380_wait_phase_timo;
	for (;;) {
		if (*sc->sci_bus_csr & SCI_BUS_REQ)
			break;
		if (--timo <= 0) {
			printf("%s: no REQ for next phase, reset\n",
				   sc->sc_dev.dv_xname);
			/* Reset implies disconnect, cmd fail. */
			act_flags |= ACT_RESET_BUS;
			goto do_actions;
		}
		delay(100);
	}

	phase = SCI_CUR_PHASE(*sc->sci_bus_csr);
	NCR_MISC(("ncr5380_machine: phase=%s\n",
			  phase_names[phase & 7]));

	/*
	 *  We make the assumption that the device knows what it's
	 *  doing, so any phase is good.
	 */
	*sc->sci_tcmd = phase;	/* acknowledge */

	switch (phase) {

	case PHASE_DATA_OUT:
	case PHASE_DATA_IN:
		act_flags = ncr5380_data_xfer(sc, phase);
		break;

	case PHASE_CMD:
		act_flags = ncr5380_command(sc);
		break;

	case PHASE_STATUS:
		act_flags = ncr5380_status(sc);
		break;

	case PHASE_MSG_OUT:
		act_flags = ncr5380_msg_out(sc);
		break;

	case PHASE_MSG_IN:
		act_flags = ncr5380_msg_in(sc);
		break;

	default:
		printf("ncr5380_machine: Unexpected phase 0x%x\n", phase);
		act_flags = ACT_ABORT_CMD;
		break;

	} /* switch */
	sc->sc_prevphase = phase;

	/* short-cut */
	if (act_flags == ACT_CONTINUE)
		goto next_phase;

do_actions:
	__asm("_ncr5380_actions:");

	NCR_MISC(("ncr5380_machine: act_flags=0x%x\n", act_flags));

	if (act_flags & ACT_WAIT_INTR) {
		/* Wait for DMA complete interrupt (or timeout). */
		NCR_MISC(("ncr5380_machine: wait for dma\n"));
		return;
	}

	if (act_flags & ACT_RESET_BUS) {
		/*
		 * Reset the SCSI bus, usually due to a timeout.
		 * The error code XS_TIMEOUT allows retries.
		 */
		NCR_MISC(("ncr5380_machine: reset scsi bus\n"));
		ncr5380_reset_scsibus(sc);
		xs->error = XS_TIMEOUT;
		act_flags = (ACT_DISCONNECT | ACT_CMD_DONE);
	}

	/* Try ONCE to send an ABORT message. */
	if (act_flags & ACT_ABORT_CMD) {
		act_flags &= ~ACT_ABORT_CMD;
		/* Try to send MSG_ABORT to the target. */
		NCR_MISC(("ncr5380_machine: send abort\n"));
		sc->sc_msg_flags |= NCR_ABORTING;
		ncr_sched_msgout(sc, SEND_ABORT);
		goto next_phase;
	}

	if (sc->sc_msg_flags & NCR_ABORTING) {
		sc->sc_msg_flags &= ~NCR_ABORTING;
		printf("ncr5380_machine: command aborted\n");
		xs->error = XS_DRIVER_STUFFUP;
		act_flags |= (ACT_DISCONNECT | ACT_CMD_DONE);
	}

	if (act_flags & ACT_CMD_DONE) {
		/* Need to call scsi_done() */
		xs->resid = sc->sc_datalen;
		/* Note: this will clear sc_current */
		ncr5380_done(sc);
		act_flags |= ACT_DISCONNECT;
	}

	if (act_flags & ACT_DISCONNECT) {
		/*
		 * The device has dropped BSY (or will soon).
		 * Return and let ncr5380_sched() do its thing.
		 */
		*sc->sci_icmd = 0;
		*sc->sci_mode = 0;
		*sc->sci_tcmd = 0;
		SCI_CLR_INTR(sc);

		*sc->sci_sel_enb = 0x80;

		/*
		 * We may be here due to a disconnect message,
		 * in which case we did NOT call ncr5380_done,
		 * so we need to clear sc_current.
		 */
		sc->sc_current = NULL;
		return;
	}
}

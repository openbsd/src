/*	$OpenBSD: ncr5380sbc.c,v 1.26 2010/06/26 23:24:44 guenther Exp $	*/
/*	$NetBSD: ncr5380sbc.c,v 1.13 1996/10/13 01:37:25 christos Exp $	*/

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
 * debugged some reentrance problems, and added several new
 * "hooks" needed for the Sun3 "si" adapters.
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_debug.h>
#include <scsi/scsi_message.h>
#include <scsi/scsiconf.h>

#ifdef DDB
#include <ddb/db_output.h>
#endif

#include <dev/ic/ncr5380reg.h>
#include <dev/ic/ncr5380var.h>

static void	ncr5380_sched(struct ncr5380_softc *);
static void	ncr5380_done(struct ncr5380_softc *);

static int	ncr5380_select(struct ncr5380_softc *, struct sci_req *);
static void	ncr5380_reselect(struct ncr5380_softc *);

static int	ncr5380_msg_in(struct ncr5380_softc *);
static int	ncr5380_msg_out(struct ncr5380_softc *);
static int	ncr5380_data_xfer(struct ncr5380_softc *, int);
static int	ncr5380_command(struct ncr5380_softc *);
static int	ncr5380_status(struct ncr5380_softc *);
static void	ncr5380_machine(struct ncr5380_softc *);

void	ncr5380_abort(struct ncr5380_softc *);
void	ncr5380_cmd_timeout(void *);
/*
 * Action flags returned by the info_transfer functions:
 * (These determine what happens next.)
 */
#define ACT_CONTINUE	0x00	/* No flags: expect another phase */
#define ACT_DISCONNECT	0x01	/* Target is disconnecting */
#define ACT_CMD_DONE	0x02	/* Need to call scsi_done() */
#define ACT_RESET_BUS	0x04	/* Need bus reset (cmd timeout) */
#define ACT_WAIT_DMA	0x10	/* Wait for DMA to complete */

/*****************************************************************
 * Debugging stuff
 *****************************************************************/

#ifndef DDB
/* This is used only in recoverable places. */
#define Debugger() printf("Debug: ncr5380.c:%d\n", __LINE__)
#endif

#ifdef	NCR5380_DEBUG

#define	NCR_DBG_BREAK	1
#define	NCR_DBG_CMDS	2
int ncr5380_debug = NCR_DBG_BREAK|NCR_DBG_CMDS;
struct ncr5380_softc *ncr5380_debug_sc;

#define	NCR_BREAK() \
	do { if (ncr5380_debug & NCR_DBG_BREAK) Debugger(); } while (0)

static void ncr5380_show_scsi_cmd(struct scsi_xfer *);
static void ncr5380_show_sense(struct scsi_xfer *);

#ifdef DDB
void ncr5380_trace(char *, long);
void ncr5380_clear_trace(void);
void ncr5380_show_trace(void);
void ncr5380_show_req(struct sci_req *);
void ncr5380_show_req(struct sci_req *);
void ncr5380_show_state(void);
#endif	/* DDB */
#else	/* NCR5380_DEBUG */

#define	NCR_BREAK() 		/* nada */
#define ncr5380_show_scsi_cmd(xs) /* nada */
#define ncr5380_show_sense(xs) /* nada */

#endif	/* NCR5380_DEBUG */

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

static __inline int ncr5380_wait_req(struct ncr5380_softc *);
static __inline int ncr5380_wait_not_req(struct ncr5380_softc *);
static __inline void ncr_sched_msgout(struct ncr5380_softc *, int);

/* Return zero on success. */
static __inline int ncr5380_wait_req(sc)
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
static __inline int ncr5380_wait_not_req(sc)
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
static __inline void
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
	register int		error;

	icmd = *(sc->sci_icmd) & SCI_ICMD_RMASK;

	icmd |= SCI_ICMD_DATA;
	*sc->sci_icmd = icmd;

	resid = count;
	while (resid > 0) {
		if (!SCI_BUSY(sc)) {
			NCR_TRACE("pio_out: lost BSY, resid=%d\n", resid);
			break;
		}
		if (ncr5380_wait_req(sc)) {
			NCR_TRACE("pio_out: no REQ, resid=%d\n", resid);
			break;
		}
		if (SCI_BUS_PHASE(*sc->sci_bus_csr) != phase)
			break;

		/* Put the data on the bus. */
		if (data)
			*sc->sci_odata = *data++;
		else
			*sc->sci_odata = 0;

		/* Tell the target it's there. */
		icmd |= SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;

		/* Wait for target to get it. */
		error = ncr5380_wait_not_req(sc);

		/* OK, it's got it (or we gave up waiting). */
		icmd &= ~SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;

		if (error) {
			NCR_TRACE("pio_out: stuck REQ, resid=%d\n", resid);
			break;
		}

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
	register int		error;

	icmd = *(sc->sci_icmd) & SCI_ICMD_RMASK;

	resid = count;
	while (resid > 0) {
		if (!SCI_BUSY(sc)) {
			NCR_TRACE("pio_in: lost BSY, resid=%d\n", resid);
			break;
		}
		if (ncr5380_wait_req(sc)) {
			NCR_TRACE("pio_in: no REQ, resid=%d\n", resid);
			break;
		}
		/* A phase change is not valid until AFTER REQ rises! */
		if (SCI_BUS_PHASE(*sc->sci_bus_csr) != phase)
			break;

		/* Read the data bus. */
		if (data)
			*data++ = *sc->sci_data;
		else
			(void) *sc->sci_data;

		/* Tell target we got it. */
		icmd |= SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;

		/* Wait for target to drop REQ... */
		error = ncr5380_wait_not_req(sc);

		/* OK, we can drop ACK. */
		icmd &= ~SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;

		if (error) {
			NCR_TRACE("pio_in: stuck REQ, resid=%d\n", resid);
			break;
		}

		--resid;
	}

	return (count - resid);
}


void
ncr5380_init(sc)
	struct ncr5380_softc *sc;
{
	int i, j;
	struct sci_req *sr;

#ifdef	NCR5380_DEBUG
	ncr5380_debug_sc = sc;
#endif

	for (i = 0; i < SCI_OPENINGS; i++) {
		sr = &sc->sc_ring[i];
		sr->sr_xs = NULL;
		timeout_set(&sr->sr_timeout, ncr5380_cmd_timeout, sr);
	}
	for (i = 0; i < 8; i++)
		for (j = 0; j < 8; j++)
			sc->sc_matrix[i][j] = NULL;

	sc->sc_link.openings = 2;	/* XXX - Not SCI_OPENINGS */
	sc->sc_prevphase = PHASE_INVALID;
	sc->sc_state = NCR_IDLE;

	*sc->sci_tcmd = PHASE_INVALID;
	*sc->sci_icmd = 0;
	*sc->sci_mode = 0;
	*sc->sci_sel_enb = 0;
	SCI_CLR_INTR(sc);

	/* XXX: Enable reselect interrupts... */
	*sc->sci_sel_enb = 0x80;

	/* Another hack (Er.. hook!) for the sun3 si: */
	if (sc->sc_intr_on) {
		NCR_TRACE("init: intr ON\n", 0);
		sc->sc_intr_on(sc);
	}
}


void
ncr5380_reset_scsibus(sc)
	struct ncr5380_softc *sc;
{

	NCR_TRACE("reset_scsibus, cur=0x%x\n",
			  (long) sc->sc_current);

	*sc->sci_icmd = SCI_ICMD_RST;
	delay(500);
	*sc->sci_icmd = 0;

	*sc->sci_mode = 0;
	*sc->sci_tcmd = PHASE_INVALID;

	SCI_CLR_INTR(sc);
	/* XXX - Need long delay here! */
	delay(100000);

	/* XXX - Need to cancel disconnected requests. */
}


/*
 * Interrupt handler for the SCSI Bus Controller (SBC)
 * This may also called for a DMA timeout (at splbio).
 */
int
ncr5380_intr(sc)
	struct ncr5380_softc *sc;
{
	int claimed = 0;

	/*
	 * Do not touch SBC regs here unless sc_current == NULL
	 * or it will complain about "register conflict" errors.
	 * Instead, just let ncr5380_machine() deal with it.
	 */
	NCR_TRACE("intr: top, state=%d\n", sc->sc_state);

	if (sc->sc_state == NCR_IDLE) {
		/*
		 * Might be reselect.  ncr5380_reselect() will check,
		 * and set up the connection if so.  This will verify
		 * that sc_current == NULL at the beginning...
		 */

		/* Another hack (Er.. hook!) for the sun3 si: */
		if (sc->sc_intr_off) {
			NCR_TRACE("intr: for reselect, intr off\n", 0);
		    sc->sc_intr_off(sc);
		}

		ncr5380_reselect(sc);
	}

	/*
	 * The remaining documented interrupt causes are phase mismatch and
	 * disconnect.  In addition, the sunsi controller may produce a state
	 * where SCI_CSR_DONE is false, yet DMA is complete.
	 *
	 * The procedure in all these cases is to let ncr5380_machine()
	 * figure out what to do next.
	 */
	if (sc->sc_state & NCR_WORKING) {
		NCR_TRACE("intr: call machine, cur=0x%x\n",
				  (long) sc->sc_current);
		/* This will usually free-up the nexus. */
		ncr5380_machine(sc);
		NCR_TRACE("intr: machine done, cur=0x%x\n",
				  (long) sc->sc_current);
		claimed = 1;
	}

	/* Maybe we can run some commands now... */
	if (sc->sc_state == NCR_IDLE) {
		NCR_TRACE("intr: call sched, cur=0x%x\n",
				  (long) sc->sc_current);
		ncr5380_sched(sc);
		NCR_TRACE("intr: sched done, cur=0x%x\n",
				  (long) sc->sc_current);
	}

	return claimed;
}


/*
 * Abort the current command (i.e. due to timeout)
 */
void
ncr5380_abort(sc)
	struct ncr5380_softc *sc;
{

	/*
	 * Finish it now.  If DMA is in progress, we
	 * can not call ncr_sched_msgout() because
	 * that hits the SBC (avoid DMA conflict).
	 */

	/* Another hack (Er.. hook!) for the sun3 si: */
	if (sc->sc_intr_off) {
		NCR_TRACE("abort: intr off\n", 0);
		sc->sc_intr_off(sc);
	}

	sc->sc_state |= NCR_ABORTING;
	if ((sc->sc_state & NCR_DOINGDMA) == 0) {
		ncr_sched_msgout(sc, SEND_ABORT);
	}
	NCR_TRACE("abort: call machine, cur=0x%x\n",
			  (long) sc->sc_current);
	ncr5380_machine(sc);
	NCR_TRACE("abort: machine done, cur=0x%x\n",
			  (long) sc->sc_current);

	/* Another hack (Er.. hook!) for the sun3 si: */
	if (sc->sc_intr_on) {
		NCR_TRACE("abort: intr ON\n", 0);
	    sc->sc_intr_on(sc);
	}
}

/*
 * Timeout handler, scheduled for each SCSI command.
 */
void
ncr5380_cmd_timeout(arg)
	void *arg;
{
	struct sci_req *sr = arg;
	struct scsi_xfer *xs;
	struct scsi_link *sc_link;
	struct ncr5380_softc *sc;
	int s;

	s = splbio();

	/* Get all our variables... */
	xs = sr->sr_xs;
	if (xs == NULL) {
		printf("ncr5380_cmd_timeout: no scsi_xfer\n");
		goto out;
	}
	sc_link = xs->sc_link;
	sc = sc_link->adapter_softc;

	printf("%s: cmd timeout, targ=%d, lun=%d\n",
	    sc->sc_dev.dv_xname,
	    sr->sr_target, sr->sr_lun);

	/*
	 * Mark the overdue job as failed, and arrange for
	 * ncr5380_machine to terminate it.  If the victim
	 * is the current job, call ncr5380_machine() now.
	 * Otherwise arrange for ncr5380_sched() to do it.
	 */
	sr->sr_flags |= SR_OVERDUE;
	if (sc->sc_current == sr) {
		NCR_TRACE("cmd_tmo: call abort, sr=0x%x\n", (long) sr);
		ncr5380_abort(sc);
	} else {
		/*
		 * The driver may be idle, or busy with another job.
		 * Arrange for ncr5380_sched() to do the deed.
		 */
		NCR_TRACE("cmd_tmo: clear matrix, t/l=0x%02x\n",
				  (sr->sr_target << 4) | sr->sr_lun);
		sc->sc_matrix[sr->sr_target][sr->sr_lun] = NULL;
	}

	/*
	 * We may have aborted the current job, or may have
	 * already been idle. In either case, we should now
	 * be idle, so try to start another job.
	 */
	if (sc->sc_state == NCR_IDLE) {
		NCR_TRACE("cmd_tmo: call sched, cur=0x%x\n",
				  (long) sc->sc_current);
		ncr5380_sched(sc);
		NCR_TRACE("cmd_tmo: sched done, cur=0x%x\n",
				  (long) sc->sc_current);
	}

out:
	splx(s);
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
void
ncr5380_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct	ncr5380_softc *sc;
	struct sci_req	*sr;
	int s, i, flags;

	sc = xs->sc_link->adapter_softc;
	flags = xs->flags;

	if (sc->sc_flags & NCR5380_FORCE_POLLING)
		flags |= SCSI_POLL;

	s = splbio();

	if (flags & SCSI_POLL) {
		/* Terminate any current command. */
		sr = sc->sc_current;
		if (sr) {
			printf("%s: polled request aborting %d/%d\n",
			    sc->sc_dev.dv_xname,
			    sr->sr_target, sr->sr_lun);
			ncr5380_abort(sc);
		}
		if (sc->sc_state != NCR_IDLE) {
			panic("ncr5380_scsi_cmd: polled request, abort failed");
		}
	}

	/*
	 * Find lowest empty slot in ring buffer.
	 * XXX: What about "fairness" and cmd order?
	 */
	for (i = 0; i < SCI_OPENINGS; i++)
		if (sc->sc_ring[i].sr_xs == NULL)
			goto new;

	xs->error = XS_NO_CCB;
	scsi_done(xs);
	NCR_TRACE("scsi_cmd: no openings\n", 0);
	goto out;

new:
	/* Create queue entry */
	sr = &sc->sc_ring[i];
	sr->sr_xs = xs;
	sr->sr_target = xs->sc_link->target;
	sr->sr_lun = xs->sc_link->lun;
	sr->sr_dma_hand = NULL;
	sr->sr_dataptr = xs->data;
	sr->sr_datalen = xs->datalen;
	sr->sr_flags = (flags & SCSI_POLL) ? SR_IMMED : 0;
	sr->sr_status = -1;	/* no value */
	sc->sc_ncmds++;

	NCR_TRACE("scsi_cmd: new sr=0x%x\n", (long)sr);

	if (flags & SCSI_POLL) {
		/* Force this new command to be next. */
		sc->sc_rr = i;
	}

	/*
	 * If we were idle, run some commands...
	 */
	if (sc->sc_state == NCR_IDLE) {
		NCR_TRACE("scsi_cmd: call sched, cur=0x%x\n",
				  (long) sc->sc_current);
		ncr5380_sched(sc);
		NCR_TRACE("scsi_cmd: sched done, cur=0x%x\n",
				  (long) sc->sc_current);
	}

	if (flags & SCSI_POLL) {
#ifdef DIAGNOSTIC
		/* Make sure ncr5380_sched() finished it. */
		if (sc->sc_state != NCR_IDLE)
			panic("ncr5380_scsi_cmd: poll didn't finish");
#endif
	}

out:
	splx(s);
}


/*
 * POST PROCESSING OF SCSI_CMD (usually current)
 * Called by ncr5380_sched(), ncr5380_machine()
 */
static void
ncr5380_done(sc)
	struct ncr5380_softc *sc;
{
	struct	sci_req *sr;
	struct	scsi_xfer *xs;

#ifdef	DIAGNOSTIC
	if (sc->sc_state == NCR_IDLE)
		panic("ncr5380_done: state=idle");
	if (sc->sc_current == NULL)
		panic("ncr5380_done: current=0");
#endif

	sr = sc->sc_current;
	xs = sr->sr_xs;

	NCR_TRACE("done: top, cur=0x%x\n", (long) sc->sc_current);

	/*
	 * Clean up DMA resources for this command.
	 */
	if (sr->sr_dma_hand) {
		NCR_TRACE("done: dma_free, dh=0x%x\n",
				  (long) sr->sr_dma_hand);
		(*sc->sc_dma_free)(sc);
	}
#ifdef	DIAGNOSTIC
	if (sr->sr_dma_hand)
		panic("ncr5380_done: dma free did not");
#endif

	if (sc->sc_state & NCR_ABORTING) {
		NCR_TRACE("done: aborting, error=%d\n", xs->error);
		if (xs->error == XS_NOERROR)
			xs->error = XS_TIMEOUT;
	}

	NCR_TRACE("done: check error=%d\n", (long) xs->error);

	/* If error is already set, ignore sr_status value. */
	if (xs->error != XS_NOERROR)
		goto finish;

	NCR_TRACE("done: check status=%d\n", sr->sr_status);

	switch (sr->sr_status) {
	case SCSI_OK:	/* 0 */
		if (sr->sr_flags & SR_SENSE) {
#ifdef	NCR5380_DEBUG
			if (ncr5380_debug & NCR_DBG_CMDS) {
				ncr5380_show_sense(xs);
			}
#endif
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
		NCR_TRACE("done: get sense, sr=0x%x\n", (long) sr);
		/*
		 * Leave queued, but clear sc_current so we start over
		 * with selection.  Guaranteed to get the same request.
		 */
		sc->sc_state = NCR_IDLE;
		sc->sc_current = NULL;
		sc->sc_matrix[sr->sr_target][sr->sr_lun] = NULL;
		return;		/* XXX */

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

	NCR_TRACE("done: finish, error=%d\n", xs->error);

	/*
	 * Dequeue the finished command, but don't clear sc_state until
	 * after the call to scsi_done(), because that may call back to
	 * ncr5380_scsi_cmd() - unwanted recursion!
	 *
	 * Keeping sc->sc_state != idle terminates the recursion.
	 */
#ifdef	DIAGNOSTIC
	if ((sc->sc_state & NCR_WORKING) == 0)
		panic("ncr5380_done: bad state");
#endif

	/* Clear our pointers to the request. */
	sc->sc_current = NULL;
	sc->sc_matrix[sr->sr_target][sr->sr_lun] = NULL;
	timeout_del(&sr->sr_timeout);

	/* Make the request free. */
	sr->sr_xs = NULL;
	sc->sc_ncmds--;

	/* Tell common SCSI code it is done. */
	scsi_done(xs);

	sc->sc_state = NCR_IDLE;
	/* Now ncr5380_sched() may be called again. */
}


/*
 * Schedule a SCSI operation.  This routine should return
 * only after it achieves one of the following conditions:
 *  	Busy (sc->sc_state != NCR_IDLE)
 *  	No more work can be started.
 */
static void
ncr5380_sched(sc)
	struct	ncr5380_softc *sc;
{
	struct sci_req	*sr;
	struct scsi_xfer *xs;
	int	target = 0, lun = 0;
	int	error, i;

	/* Another hack (Er.. hook!) for the sun3 si: */
	if (sc->sc_intr_off) {
		NCR_TRACE("sched: top, intr off\n", 0);
	    sc->sc_intr_off(sc);
	}

next_job:
	/*
	 * Grab the next job from queue.  Must be idle.
	 */
#ifdef	DIAGNOSTIC
	if (sc->sc_state != NCR_IDLE)
		panic("ncr5380_sched: not idle");
	if (sc->sc_current)
		panic("ncr5380_sched: current set");
#endif

	/*
	 * Always start the search where we last looked.
	 * The REQUEST_SENSE logic depends on this to
	 * choose the same job as was last picked, so it
	 * can just clear sc_current and reschedule.
	 * (Avoids loss of "contingent allegiance".)
	 */
	i = sc->sc_rr;
	sr = NULL;
	do {
		if (sc->sc_ring[i].sr_xs) {
			target = sc->sc_ring[i].sr_target;
			lun = sc->sc_ring[i].sr_lun;
			if (sc->sc_matrix[target][lun] == NULL) {
				/*
				 * Do not mark the  target/LUN busy yet,
				 * because reselect may cause some other
				 * job to become the current one, so we
				 * might not actually start this job.
				 * Instead, set sc_matrix later on.
				 */
				sc->sc_rr = i;
				sr = &sc->sc_ring[i];
				break;
			}
		}
		i++;
		if (i == SCI_OPENINGS)
			i = 0;
	} while (i != sc->sc_rr);

	if (sr == NULL) {
		NCR_TRACE("sched: no work, cur=0x%x\n",
				  (long) sc->sc_current);

		/* Another hack (Er.. hook!) for the sun3 si: */
		if (sc->sc_intr_on) {
			NCR_TRACE("sched: ret, intr ON\n", 0);
			sc->sc_intr_on(sc);
		}

		return;		/* No more work to do. */
	}

	NCR_TRACE("sched: select for t/l=0x%02x\n",
			  (sr->sr_target << 4) | sr->sr_lun);

	sc->sc_state = NCR_WORKING;
	error = ncr5380_select(sc, sr);
	if (sc->sc_current) {
		/* Lost the race!  reselected out from under us! */
		/* Work with the reselected job. */
		if (sr->sr_flags & SR_IMMED) {
			printf("%s: reselected while polling (abort)\n",
			    sc->sc_dev.dv_xname);
			/* Abort the reselected job. */
			sc->sc_state |= NCR_ABORTING;
			sc->sc_msgpriq |= SEND_ABORT;
		}
		sr = sc->sc_current;
		xs = sr->sr_xs;
		NCR_TRACE("sched: reselect, new sr=0x%x\n", (long)sr);
		goto have_nexus;
	}

	/* Normal selection result.  Target/LUN is now busy. */
	sc->sc_matrix[target][lun] = sr;
	sc->sc_current = sr;	/* connected */
	xs = sr->sr_xs;

	/*
	 * Initialize pointers, etc. for this job
	 */
	sc->sc_dataptr  = sr->sr_dataptr;
	sc->sc_datalen  = sr->sr_datalen;
	sc->sc_prevphase = PHASE_INVALID;
	sc->sc_msgpriq = SEND_IDENTIFY;
	sc->sc_msgoutq = 0;
	sc->sc_msgout  = 0;

	NCR_TRACE("sched: select rv=%d\n", error);

	switch (error) {
	case XS_NOERROR:
		break;

	case XS_BUSY:
		/* XXX - Reset and try again. */
		printf("%s: select found SCSI bus busy, resetting...\n",
		    sc->sc_dev.dv_xname);
		ncr5380_reset_scsibus(sc);
		/* fallthrough */
	case XS_SELTIMEOUT:
	default:
		xs->error = error;	/* from select */
		NCR_TRACE("sched: call done, sr=0x%x\n", (long)sr);
		ncr5380_done(sc);

		/* Paranoia: clear everything. */
		sc->sc_dataptr = NULL;
		sc->sc_datalen = 0;
		sc->sc_prevphase = PHASE_INVALID;
		sc->sc_msgpriq = 0;
		sc->sc_msgoutq = 0;
		sc->sc_msgout  = 0;

		goto next_job;
	}

	/*
	 * Selection was successful.  Normally, this means
	 * we are starting a new command.  However, this
	 * might be the termination of an overdue job.
	 */
	if (sr->sr_flags & SR_OVERDUE) {
		NCR_TRACE("sched: overdue, sr=0x%x\n", (long)sr);
		sc->sc_state |= NCR_ABORTING;
		sc->sc_msgpriq |= SEND_ABORT;
		goto have_nexus;
	}

	/*
	 * This may be the continuation of some job that
	 * completed with a "check condition" code.
	 */
	if (sr->sr_flags & SR_SENSE) {
		NCR_TRACE("sched: get sense, sr=0x%x\n", (long)sr);
		/* Do not allocate DMA, nor set timeout. */
		goto have_nexus;
	}

	/*
	 * OK, we are starting a new command.
	 * Initialize and allocate resources for the new command.
	 * Device reset is special (only uses MSG_OUT phase).
	 * Normal commands start in MSG_OUT phase where we will
	 * send and IDENDIFY message, and then expect CMD phase.
	 */
#ifdef	NCR5380_DEBUG
	if (ncr5380_debug & NCR_DBG_CMDS) {
		printf("ncr5380_sched: begin, target=%d, LUN=%d\n",
		    xs->sc_link->target, xs->sc_link->lun);
		ncr5380_show_scsi_cmd(xs);
	}
#endif
	if (xs->flags & SCSI_RESET) {
		NCR_TRACE("sched: cmd=reset, sr=0x%x\n", (long)sr);
		/* Not an error, so do not set NCR_ABORTING */
		sc->sc_msgpriq |= SEND_DEV_RESET;
		goto have_nexus;
	}

#ifdef	DIAGNOSTIC
	if ((xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) == 0) {
		if (sc->sc_dataptr) {
			printf("%s: ptr but no data in/out flags?\n",
			    sc->sc_dev.dv_xname);
			NCR_BREAK();
			sc->sc_dataptr = NULL;
		}
	}
#endif

	/* Allocate DMA space (maybe) */
	if (sc->sc_dataptr && sc->sc_dma_alloc &&
		(sc->sc_datalen >= sc->sc_min_dma_len))
	{
		NCR_TRACE("sched: dma_alloc, len=%d\n", sc->sc_datalen);
		(*sc->sc_dma_alloc)(sc);
	}

	/*
	 * Initialization hook called just after select,
	 * at the beginning of COMMAND phase.
	 * (but AFTER the DMA allocation is done)
	 *
	 * The evil Sun "si" adapter (OBIO variant) needs some
	 * setup done to the DMA engine BEFORE the target puts
	 * the SCSI bus into any DATA phase.
	 */
	if (sr->sr_dma_hand && sc->sc_dma_setup) {
		NCR_TRACE("sched: dma_setup, dh=0x%x\n",
				  (long) sr->sr_dma_hand);
	    sc->sc_dma_setup(sc);
	}

	/*
	 * Schedule a timeout for the job we are starting.
	 */
	if ((sr->sr_flags & SR_IMMED) == 0) {
		i = (xs->timeout * hz) / 1000;
		NCR_TRACE("sched: set timeout=%d\n", i);
		timeout_add(&sr->sr_timeout, i);
	}

have_nexus:
	NCR_TRACE("sched: call machine, cur=0x%x\n",
			  (long) sc->sc_current);
	ncr5380_machine(sc);
	NCR_TRACE("sched: machine done, cur=0x%x\n",
			  (long) sc->sc_current);

	/*
	 * What state did ncr5380_machine() leave us in?
	 * Hopefully it sometimes completes a job...
	 */
	if (sc->sc_state == NCR_IDLE)
		goto next_job;

	return; 	/* Have work in progress. */
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
	struct sci_req *sr;
	int target, lun, phase, timo;
	int target_mask;
	u_char bus, data, icmd, msg;

#ifdef	DIAGNOSTIC
	/*
	 * Note: sc_state will be "idle" when ncr5380_intr()
	 * calls, or "working" when ncr5380_select() calls.
	 * (So don't test that in this DIAGNOSTIC)
	 */
	if (sc->sc_current)
		panic("ncr5380_reselect: current set");
#endif

	/*
	 * First, check the select line.
	 * (That has to be set first.)
	 */
	bus = *(sc->sci_bus_csr);
	if ((bus & SCI_BUS_SEL) == 0) {
		/* Not a selection or reselection. */
		return;
	}

	/*
	 * The target will assert BSY first (for bus arbitration),
	 * then raise SEL, and finally drop BSY.  Only then is the
	 * data bus required to have valid selection ID bits set.
	 * Wait for: SEL==1, BSY==0 before reading the data bus.
	 * While this theoretically can happen, we are apparently
	 * never fast enough to get here before BSY drops.
	 */
	timo = ncr5380_wait_nrq_timo;
	for (;;) {
		if ((bus & SCI_BUS_BSY) == 0)
			break;
		/* Probably never get here... */
		if (--timo <= 0) {
			printf("%s: reselect, BSY stuck, bus=0x%x\n",
			    sc->sc_dev.dv_xname, bus);
			/* Not much we can do. Reset the bus. */
			ncr5380_reset_scsibus(sc);
			return;
		}
		delay(2);
		bus = *(sc->sci_bus_csr);
		/* If SEL went away, forget it. */
		if ((bus & SCI_BUS_SEL) == 0)
			return;
		/* Still have SEL, check BSY. */
	}
	NCR_TRACE("reselect, valid data after %d loops\n",
			  ncr5380_wait_nrq_timo - timo);

	/*
	 * Good.  We have SEL=1 and BSY=0.  Now wait for a
	 * "bus settle delay" before we sample the data bus
	 */
	delay(2);
	data = *(sc->sci_data) & 0xFF;
	/* Parity check is implicit in data validation below. */

	/*
	 * Is this a reselect (I/O == 1) or have we been
	 * selected as a target? (I/O == 0)
	 */
	if ((bus & SCI_BUS_IO) == 0) {
		printf("%s: selected as target, data=0x%x\n",
		    sc->sc_dev.dv_xname, data);
		/* Not much we can do. Reset the bus. */
		/* XXX: send some sort of message? */
		ncr5380_reset_scsibus(sc);
		return;
	}

	/*
	 * OK, this is a reselection.
	 */
	for (target = 0; target < 7; target++) {
		target_mask = (1 << target);
		if (data & target_mask)
			break;
	}
	if ((data & 0x7F) != target_mask) {
		/* No selecting ID? or >2 IDs on bus? */
		printf("%s: bad reselect, data=0x%x\n",
		    sc->sc_dev.dv_xname, data);
		return;
	}

	NCR_TRACE("reselect: target=0x%x\n", target);

	/* Raise BSY to acknowledge target reselection. */
	*(sc->sci_icmd) = SCI_ICMD_BSY;

	/* Wait for target to drop SEL. */
	timo = ncr5380_wait_nrq_timo;
	for (;;) {
		bus = *(sc->sci_bus_csr);
		if ((bus & SCI_BUS_SEL) == 0)
			break;	/* success */
		if (--timo <= 0) {
			printf("%s: reselect, SEL stuck, bus=0x%x\n",
			    sc->sc_dev.dv_xname, bus);
			NCR_BREAK();
			/* assume connected (fail later if not) */
			break;
		}
		delay(2);
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
	phase = SCI_BUS_PHASE(*sc->sci_bus_csr);
	if (phase != PHASE_MSG_IN) {
		printf("%s: reselect, phase=%d\n",
		    sc->sc_dev.dv_xname, phase);
		goto abort;
	}

	/* Ack. the change to PHASE_MSG_IN */
	*(sc->sci_tcmd) = PHASE_MSG_IN;

	/* Peek at the message byte without consuming it! */
	msg = *(sc->sci_data);
	if ((msg & 0x80) == 0) {
		printf("%s: reselect, not identify, msg=%d\n",
		    sc->sc_dev.dv_xname, msg);
		goto abort;
	}
	lun = msg & 7;
	
	/* We now know target/LUN.  Do we have the request? */
	sr = sc->sc_matrix[target][lun];
	if (sr) {
		/* We now have a nexus. */
		sc->sc_state |= NCR_WORKING;
		sc->sc_current = sr;
		NCR_TRACE("reselect: resume sr=0x%x\n", (long)sr);

		/* Implicit restore pointers message */
		sc->sc_dataptr = sr->sr_dataptr;
		sc->sc_datalen = sr->sr_datalen;

		sc->sc_prevphase = PHASE_INVALID;
		sc->sc_msgpriq = 0;
		sc->sc_msgoutq = 0;
		sc->sc_msgout  = 0;

		/* XXX: Restore the normal mode register. */
		/* If this target's bit is set, do NOT check parity. */
		if (sc->sc_parity_disable & target_mask)
			*sc->sci_mode = (SCI_MODE_MONBSY);
		else
			*sc->sci_mode = (SCI_MODE_MONBSY | SCI_MODE_PAR_CHK);

		/*
		 * Another hack for the Sun3 "si", which needs
		 * some setup done to its DMA engine before the
		 * target puts the SCSI bus into any DATA phase.
		 */
		if (sr->sr_dma_hand && sc->sc_dma_setup) {
			NCR_TRACE("reselect: call DMA setup, dh=0x%x\n",
					  (long) sr->sr_dma_hand);
		    sc->sc_dma_setup(sc);
		}

		/* Now consume the IDENTIFY message. */
		ncr5380_pio_in(sc, PHASE_MSG_IN, 1, &msg);
		return;
	}

	printf("%s: phantom reselect: target=%d, LUN=%d\n",
	    sc->sc_dev.dv_xname, target, lun);
abort:
	/*
	 * Try to send an ABORT message.  This makes us
	 * temporarily busy, but no current command...
	 */
	sc->sc_state |= NCR_ABORTING;

	/* Raise ATN, delay, raise ACK... */
	icmd = SCI_ICMD_ATN;
	*sc->sci_icmd = icmd;
	delay(2);

	/* Now consume the IDENTIFY message. */
	ncr5380_pio_in(sc, PHASE_MSG_IN, 1, &msg);

	/* Finally try to send the ABORT. */
	sc->sc_prevphase = PHASE_INVALID;
	sc->sc_msgpriq = SEND_ABORT;
	ncr5380_msg_out(sc);

	*(sc->sci_tcmd) = PHASE_INVALID;
	*sc->sci_sel_enb = 0;
	SCI_CLR_INTR(sc);
	*sc->sci_sel_enb = 0x80;

	sc->sc_state &= ~NCR_ABORTING;
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
ncr5380_select(sc, sr)
	struct ncr5380_softc *sc;
	struct sci_req *sr;
{
	int timo, s, target_mask;
	u_char data, icmd;

	/* Check for reselect */
	ncr5380_reselect(sc);
	if (sc->sc_current) {
		NCR_TRACE("select: reselect, cur=0x%x\n",
				  (long) sc->sc_current);
		return XS_BUSY;	/* reselected */
	}

	/*
	 * Set phase bits to 0, otherwise the 5380 won't drive the bus during
	 * selection.
	 */
	*sc->sci_tcmd = PHASE_DATA_OUT;
	*sc->sci_icmd = icmd = 0;
	*sc->sci_mode = 0;

	/*
	 * Arbitrate for the bus.  The 5380 takes care of the
	 * time-critical bus interactions.  We set our ID bit
	 * in the output data register and set MODE_ARB.  The
	 * 5380 watches for the required "bus free" period.
	 * If and when the "bus free" period is detected, the
	 * 5380 drives BSY, drives the data bus, and sets the
	 * "arbitration in progress" (AIP) bit to let us know
	 * arbitration has started (and that it asserts BSY).
	 * We then wait for one arbitration delay (2.2uS) and
	 * check the ICMD_LST bit, which will be set if some
	 * other target drives SEL during arbitration.
	 *
	 * There is a time-critical section during the period
	 * after we enter arbitration up until we assert SEL.
	 * Avoid long interrupts during this period.
	 */
	s = splvm();	/* XXX: Begin time-critical section */

	*(sc->sci_odata) = 0x80;	/* OUR_ID */
	*(sc->sci_mode) = SCI_MODE_ARB;

#define	WAIT_AIP_USEC	20	/* pleanty of time */
	/* Wait for the AIP bit to turn on. */
	timo = WAIT_AIP_USEC;
	for (;;) {
		if (*(sc->sci_icmd) & SCI_ICMD_AIP)
			break;
		if (timo <= 0) {
			/*
			 * Did not see any "bus free" period.
			 * The usual reason is a reselection,
			 * so treat this as arbitration loss.
			 */
			NCR_TRACE("select: bus busy, rc=%d\n", XS_BUSY);
			goto lost_arb;
		}
		timo -= 2;
		delay(2);
	}
	NCR_TRACE("select: have AIP after %d uSec.\n",
			  WAIT_AIP_USEC - timo);

	/* Got AIP.  Wait one arbitration delay (2.2 uS.) */
	delay(3);

	/* Check for ICMD_LST */
	if (*(sc->sci_icmd) & SCI_ICMD_LST) {
		/* Some other target asserted SEL. */
		NCR_TRACE("select: lost one, rc=%d\n", XS_BUSY);
		goto lost_arb;
	}

	/*
	 * No other device has declared itself the winner.
	 * The spec. says to check for higher IDs, but we
	 * are always the highest (ID=7) so don't bother.
	 * We can now declare victory by asserting SEL.
	 *
	 * Note that the 5380 is asserting BSY because we
	 * have entered arbitration mode.  We will now hold
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

	/*
	 * Check one last time to see if we really did
	 * win arbitration.  This might only happen if
	 * there can be a higher selection ID than ours.
	 * Keep this code for reference anyway...
	 */
	if (*(sc->sci_icmd) & SCI_ICMD_LST) {
		/* Some other target asserted SEL. */
		NCR_TRACE("select: lost two, rc=%d\n", XS_BUSY);

	lost_arb:
		*sc->sci_icmd = 0;
		*sc->sci_mode = 0;

		splx(s);	/* XXX: End of time-critical section. */

		/*
		 * When we lose arbitration, it usually means
		 * there is a target trying to reselect us.
		 */
		ncr5380_reselect(sc);
		return XS_BUSY;
	}

	/* Leave ARB mode Now that we drive BSY+SEL */
	*sc->sci_mode = 0;
	*sc->sci_sel_enb = 0;

	splx(s);	/* XXX: End of time-critical section. */

	/*
	 * Arbitration is complete.  Now do selection:
	 * Drive the data bus with the ID bits for both
	 * the host and target.  Also set ATN now, to
	 * ask the target for a message out phase.
	 */
	target_mask = (1 << sr->sr_target);
	data = 0x80 | target_mask;
	*(sc->sci_odata) = data;
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
		*sc->sci_tcmd = PHASE_INVALID;
		*sc->sci_icmd = 0;
		*sc->sci_mode = 0;
		*sc->sci_sel_enb = 0;
		SCI_CLR_INTR(sc);
		*sc->sci_sel_enb = 0x80;
		NCR_TRACE("select: device down, rc=%d\n", XS_SELTIMEOUT);
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

	/* If this target's bit is set, do NOT check parity. */
	if (sc->sc_parity_disable & target_mask)
		*sc->sci_mode = (SCI_MODE_MONBSY);
	else
		*sc->sci_mode = (SCI_MODE_MONBSY | SCI_MODE_PAR_CHK);

	return XS_NOERROR;
}


/*****************************************************************
 * Functions to handle each info. transfer phase:
 *****************************************************************/

/*
 * The message system:
 *
 * This is a revamped message system that now should easier accommodate
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
	struct scsi_xfer *xs = sr->sr_xs;
	int n, phase;
	int act_flags;
	register u_char icmd;

	/* acknowledge phase change */
	*sc->sci_tcmd = PHASE_MSG_IN;

	act_flags = ACT_CONTINUE;
	icmd = *sc->sci_icmd & SCI_ICMD_RMASK;

	if (sc->sc_prevphase == PHASE_MSG_IN) {
		/* This is a continuation of the previous message. */
		n = sc->sc_imp - sc->sc_imess;
		NCR_TRACE("msg_in: continuation, n=%d\n", n);
		goto nextbyte;
	}

	/* This is a new MESSAGE IN phase.  Clean up our state. */
	sc->sc_state &= ~NCR_DROP_MSGIN;

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
			NCR_TRACE("msg_in: lost BSY, n=%d\n", n);
			/* XXX - Assume the command completed? */
			act_flags |= (ACT_DISCONNECT | ACT_CMD_DONE);
			return (act_flags);
		}
		if (ncr5380_wait_req(sc)) {
			NCR_TRACE("msg_in: BSY but no REQ, n=%d\n", n);
			/* Just let ncr5380_machine() handle it... */
			return (act_flags);
		}
		phase = SCI_BUS_PHASE(*sc->sci_bus_csr);
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
			sc->sc_state |= NCR_DROP_MSGIN;
		}

		/* Gather incoming message bytes if needed. */
		if ((sc->sc_state & NCR_DROP_MSGIN) == 0) {
			if (n >= NCR_MAX_MSG_LEN) {
				ncr_sched_msgout(sc, SEND_REJECT);
				sc->sc_state |= NCR_DROP_MSGIN;
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
			NCR_TRACE("msg_in: drop, stuck REQ, n=%d\n", n);
			act_flags |= ACT_RESET_BUS;
		}

		icmd &= ~SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;

		if (act_flags != ACT_CONTINUE)
			return (act_flags);

		/* back to nextbyte */
	}

have_msg:
	/* We now have a complete message.  Parse it. */

	switch (sc->sc_imess[0]) {
	case MSG_CMDCOMPLETE:
		NCR_TRACE("msg_in: CMDCOMPLETE\n", 0);
		/* Target is about to disconnect. */
		act_flags |= (ACT_DISCONNECT | ACT_CMD_DONE);
		break;

	case MSG_PARITY_ERROR:
		NCR_TRACE("msg_in: PARITY_ERROR\n", 0);
		/* Resend the last message. */
		ncr_sched_msgout(sc, sc->sc_msgout);
		/* Reset icmd after scheduling the REJECT cmd - jwg */
		icmd = *sc->sci_icmd & SCI_ICMD_RMASK;
		break;

	case MSG_MESSAGE_REJECT:
		/* The target rejects the last message we sent. */
		NCR_TRACE("msg_in: got reject for 0x%x\n", sc->sc_msgout);
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
		NCR_TRACE("msg_in: NOOP\n", 0);
		break;

	case MSG_DISCONNECT:
		NCR_TRACE("msg_in: DISCONNECT\n", 0);
		/* Target is about to disconnect. */
		act_flags |= ACT_DISCONNECT;
		if ((xs->sc_link->quirks & SDEV_AUTOSAVE) == 0)
			break;
		/*FALLTHROUGH*/

	case MSG_SAVEDATAPOINTER:
		NCR_TRACE("msg_in: SAVE_PTRS\n", 0);
		sr->sr_dataptr = sc->sc_dataptr;
		sr->sr_datalen = sc->sc_datalen;
		break;

	case MSG_RESTOREPOINTERS:
		NCR_TRACE("msg_in: RESTORE_PTRS\n", 0);
		sc->sc_dataptr = sr->sr_dataptr;
		sc->sc_datalen = sr->sr_datalen;
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
		NCR_TRACE("msg_in: eh? imsg=0x%x\n", sc->sc_imess[0]);
		printf("%s: unrecognized MESSAGE; sending REJECT\n",
		    sc->sc_dev.dv_xname);
		NCR_BREAK();
		/* fallthrough */
	reject:
		ncr_sched_msgout(sc, SEND_REJECT);
		/* Reset icmd after scheduling the REJECT cmd - jwg */
		icmd = *sc->sci_icmd & SCI_ICMD_RMASK;
		break;

	abort:
		sc->sc_state |= NCR_ABORTING;
		ncr_sched_msgout(sc, SEND_ABORT);
		break;
	}

	/* Ack the last byte read. */
	icmd |= SCI_ICMD_ACK;
	*sc->sci_icmd = icmd;

	if (ncr5380_wait_not_req(sc)) {
		NCR_TRACE("msg_in: last, stuck REQ, n=%d\n", n);
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
	int act_flags, n, phase, progress;
	register u_char icmd, msg;

	/* acknowledge phase change */
	*sc->sci_tcmd = PHASE_MSG_OUT;

	progress = 0;	/* did we send any messages? */
	act_flags = ACT_CONTINUE;

	/*
	 * Set ATN.  If we're just sending a trivial 1-byte message,
	 * we'll clear ATN later on anyway.  Also drive the data bus.
	 */
	icmd = *sc->sci_icmd & SCI_ICMD_RMASK;
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
			sc->sc_msgpriq |= sc->sc_msgoutq;
			NCR_TRACE("msg_out: retrans priq=0x%x\n", sc->sc_msgpriq);
		} else {
			/* This is a continuation of the previous message. */
			n = sc->sc_omp - sc->sc_omess;
			NCR_TRACE("msg_out: continuation, n=%d\n", n);
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
		NCR_TRACE("msg_out: SEND_IDENTIFY\n", 0);
		if (sr == NULL) {
			printf("%s: SEND_IDENTIFY while not connected; sending NOOP\n",
			    sc->sc_dev.dv_xname);
			NCR_BREAK();
			goto noop;
		}
		/*
		 * The identify message we send determines whether 
		 * disconnect/reselect is allowed for this command.
		 * 0xC0+LUN: allows it, 0x80+LUN disallows it.
		 */
		msg = 0xc0;	/* MSG_IDENTIFY(0,1) */
		if (sc->sc_no_disconnect & (1 << sr->sr_target))
			msg = 0x80;
		if (sr->sr_flags & (SR_IMMED | SR_SENSE))
			msg = 0x80;
		sc->sc_omess[0] = msg | sr->sr_lun;
		n = 1;
		break;

	case SEND_DEV_RESET:
		NCR_TRACE("msg_out: SEND_DEV_RESET\n", 0);
		/* Expect disconnect after this! */
		/* XXX: Kill jobs for this target? */
		act_flags |= (ACT_DISCONNECT | ACT_CMD_DONE);
		sc->sc_omess[0] = MSG_BUS_DEV_RESET;
		n = 1;
		break;

	case SEND_REJECT:
		NCR_TRACE("msg_out: SEND_REJECT\n", 0);
		sc->sc_omess[0] = MSG_MESSAGE_REJECT;
		n = 1;
		break;

	case SEND_PARITY_ERROR:
		NCR_TRACE("msg_out: SEND_PARITY_ERROR\n", 0);
		sc->sc_omess[0] = MSG_PARITY_ERROR;
		n = 1;
		break;

	case SEND_INIT_DET_ERR:
		NCR_TRACE("msg_out: SEND_INIT_DET_ERR\n", 0);
		sc->sc_omess[0] = MSG_INITIATOR_DET_ERR;
		n = 1;
		break;

	case SEND_ABORT:
		NCR_TRACE("msg_out: SEND_ABORT\n", 0);
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
		NCR_TRACE("msg_out: send NOOP\n", 0);
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
			NCR_TRACE("msg_out: lost BSY, n=%d\n", n);
			goto out;
		}
		if (ncr5380_wait_req(sc)) {
			NCR_TRACE("msg_out: no REQ, n=%d\n", n);
			goto out;
		}
		phase = SCI_BUS_PHASE(*sc->sci_bus_csr);
		if (phase != PHASE_MSG_OUT) {
			/*
			 * Target left MESSAGE OUT, possibly to reject
			 * our message.
			 */
			NCR_TRACE("msg_out: new phase=%d\n", phase);
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
			NCR_TRACE("msg_out: stuck REQ, n=%d\n", n);
			act_flags |= ACT_RESET_BUS;
		}

		/* Finally, drop ACK. */
		icmd &= ~SCI_ICMD_ACK;
		*sc->sci_icmd = icmd;

		/* Stuck bus or something... */
		if (act_flags & ACT_RESET_BUS)
			goto out;

	}
	progress++;

	/* We get here only if the entire message has been transmitted. */
	if (sc->sc_msgpriq != 0) {
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
	struct sci_req *sr = sc->sc_current;
	struct scsi_xfer *xs = sr->sr_xs;
	struct scsi_sense rqs;
	int len;

	/* acknowledge phase change */
	*sc->sci_tcmd = PHASE_COMMAND;

	if (sr->sr_flags & SR_SENSE) {
		rqs.opcode = REQUEST_SENSE;
		rqs.byte2 = xs->sc_link->lun << 5;
		rqs.length = sizeof(xs->sense);

		rqs.unused[0] = rqs.unused[1] = rqs.control = 0;
		len = ncr5380_pio_out(sc, PHASE_COMMAND, sizeof(rqs),
			(u_char *)&rqs);
	}
	else {
		/* Assume command can be sent in one go. */
		/* XXX: Do this using DMA, and get a phase change intr? */
		len = ncr5380_pio_out(sc, PHASE_COMMAND, xs->cmdlen,
			(u_char *)xs->cmd);
	}

	if (len != xs->cmdlen) {
#ifdef	NCR5380_DEBUG
		printf("ncr5380_command: short transfer: wanted %d got %d.\n",
		    xs->cmdlen, len);
		ncr5380_show_scsi_cmd(xs);
		NCR_BREAK();
#endif
		if (len < 6) {
			xs->error = XS_DRIVER_STUFFUP;
			sc->sc_state |= NCR_ABORTING;
			ncr_sched_msgout(sc, SEND_ABORT);
		}

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
	int len;

	if (sr->sr_flags & SR_SENSE) {
		NCR_TRACE("data_xfer: get sense, sr=0x%x\n", (long)sr);
		if (phase != PHASE_DATA_IN) {
			printf("%s: sense phase error\n", sc->sc_dev.dv_xname);
			goto abort;
		}
		/* acknowledge phase change */
		*sc->sci_tcmd = PHASE_DATA_IN;
		len = ncr5380_pio_in(sc, phase, sizeof(xs->sense),
				(u_char *)&xs->sense);
		return ACT_CONTINUE;
	}

	/*
	 * When aborting a command, disallow any data phase.
	 */
	if (sc->sc_state & NCR_ABORTING) {
		printf("%s: aborting, but phase=%s (reset)\n",
		    sc->sc_dev.dv_xname, phase_names[phase & 7]);
		return ACT_RESET_BUS;	/* XXX */
	}

	/* Validate expected phase (data_in or data_out) */
	expected_phase = (xs->flags & SCSI_DATA_OUT) ?
		PHASE_DATA_OUT : PHASE_DATA_IN;
	if (phase != expected_phase) {
		printf("%s: data phase error\n", sc->sc_dev.dv_xname);
		goto abort;
	}

	/* Make sure we have some data to move. */
	if (sc->sc_datalen <= 0) {
		/* Device needs padding. */
		if (phase == PHASE_DATA_IN)
			ncr5380_pio_in(sc, phase, 4096, NULL);
		else
			ncr5380_pio_out(sc, phase, 4096, NULL);
		/* Make sure that caused a phase change. */
		if (SCI_BUS_PHASE(*sc->sci_bus_csr) == phase) {
			/* More than 4k is just too much! */
			printf("%s: too much data padding\n",
			       sc->sc_dev.dv_xname);
			goto abort;
		}
		return ACT_CONTINUE;
	}

	/*
	 * Attempt DMA only if dma_alloc gave us a DMA handle AND
	 * there is enough left to transfer so DMA is worth while.
	 */
	if (sr->sr_dma_hand &&
		(sc->sc_datalen >= sc->sc_min_dma_len))
	{
		/*
		 * OK, really start DMA.  Note, the MD start function
		 * is responsible for setting the TCMD register, etc.
		 * (Acknowledge the phase change there, not here.)
		 */
		NCR_TRACE("data_xfer: dma_start, dh=0x%x\n",
		          (long) sr->sr_dma_hand);
		(*sc->sc_dma_start)(sc);
		return ACT_WAIT_DMA;
	}

	/*
	 * Doing PIO for data transfer.  (Possibly "Pseudo DMA")
	 * XXX:  Do PDMA functions need to set tcmd later?
	 */
	NCR_TRACE("data_xfer: doing PIO, len=%d\n", sc->sc_datalen);
	/* acknowledge phase change */
	*sc->sci_tcmd = phase;	/* XXX: OK for PDMA? */
	if (phase == PHASE_DATA_OUT) {
		len = (*sc->sc_pio_out)(sc, phase, sc->sc_datalen, sc->sc_dataptr);
	} else {
		len = (*sc->sc_pio_in) (sc, phase, sc->sc_datalen, sc->sc_dataptr);
	}
	sc->sc_dataptr += len;
	sc->sc_datalen -= len;

	NCR_TRACE("data_xfer: did PIO, resid=%d\n", sc->sc_datalen);
	return (ACT_CONTINUE);

abort:
	sc->sc_state |= NCR_ABORTING;
	ncr_sched_msgout(sc, SEND_ABORT);
	return (ACT_CONTINUE);
}


static int
ncr5380_status(sc)
	struct ncr5380_softc *sc;
{
	int len;
	u_char status;
	struct sci_req *sr = sc->sc_current;

	/* acknowledge phase change */
	*sc->sci_tcmd = PHASE_STATUS;

	len = ncr5380_pio_in(sc, PHASE_STATUS, 1, &status);
	if (len) {
		sr->sr_status = status;
	} else {
		printf("ncr5380_status: none?\n");
	}

	return ACT_CONTINUE;
}


/*
 * This is the big state machine that follows SCSI phase changes.
 * This is somewhat like a co-routine.  It will do a SCSI command,
 * and exit if the command is complete, or if it must wait, i.e.
 * for DMA to complete or for reselect to resume the job.
 *
 * The bus must be selected, and we need to know which command is
 * being undertaken.
 */
static void
ncr5380_machine(sc)
	struct ncr5380_softc *sc;
{
	struct sci_req *sr;
	struct scsi_xfer *xs;
	int act_flags, phase, timo;

#ifdef	DIAGNOSTIC
	if (sc->sc_state == NCR_IDLE)
		panic("ncr5380_machine: state=idle");
	if (sc->sc_current == NULL)
		panic("ncr5380_machine: no current cmd");
#endif

	sr = sc->sc_current;
	xs = sr->sr_xs;
	act_flags = ACT_CONTINUE;

	/*
	 * This will be called by ncr5380_intr() when DMA is
	 * complete.  Must stop DMA before touching the 5380 or
	 * there will be "register conflict" errors.
	 */
	if (sc->sc_state & NCR_DOINGDMA) {
		/* Pick-up where where we left off... */
		goto dma_done;
	}

next_phase:

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
			if (sc->sc_state & NCR_ABORTING) {
				printf("%s: no REQ while aborting, reset\n",
				    sc->sc_dev.dv_xname);
				act_flags |= ACT_RESET_BUS;
				goto do_actions;
			}
			printf("%s: no REQ for next phase, abort\n",
			    sc->sc_dev.dv_xname);
			sc->sc_state |= NCR_ABORTING;
			ncr_sched_msgout(sc, SEND_ABORT);
			goto next_phase;
		}
		delay(100);
	}

	phase = SCI_BUS_PHASE(*sc->sci_bus_csr);
	NCR_TRACE("machine: phase=%s\n",
			  (long) phase_names[phase & 7]);

	/*
	 * We assume that the device knows what it's doing,
	 * so any phase is good.
	 */

#if 0
	/*
	 * XXX: Do not ACK the phase yet! do it later...
	 * XXX: ... each phase routine does that itself.
	 * In particular, DMA needs it done LATER.
	 */
	*sc->sci_tcmd = phase;	/* acknowledge phase change */
#endif

	switch (phase) {

	case PHASE_DATA_OUT:
	case PHASE_DATA_IN:
		act_flags = ncr5380_data_xfer(sc, phase);
		break;

	case PHASE_COMMAND:
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
		sc->sc_state |= NCR_ABORTING;
		ncr_sched_msgout(sc, SEND_ABORT);
		goto next_phase;

	} /* switch */
	sc->sc_prevphase = phase;

do_actions:
	__asm("_ncr5380_actions:");

	if (act_flags & ACT_WAIT_DMA) {
		act_flags &= ~ACT_WAIT_DMA;
		/* Wait for DMA to complete (polling, or interrupt). */
		if ((sr->sr_flags & SR_IMMED) == 0) {
			NCR_TRACE("machine: wait for DMA intr.\n", 0);
			return; 	/* will resume at dma_done */
		}
		/* Busy-wait for it to finish. */
		NCR_TRACE("machine: dma_poll, dh=0x%x\n",
				  (long) sr->sr_dma_hand);
		(*sc->sc_dma_poll)(sc);
	dma_done:
		/* Return here after interrupt. */
		if (sr->sr_flags & SR_OVERDUE)
			sc->sc_state |= NCR_ABORTING;
		NCR_TRACE("machine: dma_stop, dh=0x%x\n",
				  (long) sr->sr_dma_hand);
		(*sc->sc_dma_stop)(sc);
		SCI_CLR_INTR(sc);	/* XXX */
		/*
		 * While DMA is running we can not touch the SBC,
		 * so various places just set NCR_ABORTING and
		 * expect us the "kick it" when DMA is done.
		 */
		if (sc->sc_state & NCR_ABORTING) {
			ncr_sched_msgout(sc, SEND_ABORT);
		}
	}

	/*
	 * Check for parity error.
	 * XXX - better place to check?
	 */
	if (*(sc->sci_csr) & SCI_CSR_PERR) {
		printf("%s: parity error!\n", sc->sc_dev.dv_xname);
		/* XXX: sc->sc_state |= NCR_ABORTING; */
		ncr_sched_msgout(sc, SEND_PARITY_ERROR);
	}

	if (act_flags == ACT_CONTINUE)
		goto next_phase;
	/* All other actions "break" from the loop. */

	NCR_TRACE("machine: act_flags=0x%x\n", act_flags);

	if (act_flags & ACT_RESET_BUS) {
		act_flags |= ACT_CMD_DONE;
		/*
		 * Reset the SCSI bus, usually due to a timeout.
		 * The error code XS_TIMEOUT allows retries.
		 */
		sc->sc_state |= NCR_ABORTING;
		printf("%s: reset SCSI bus for TID=%d LUN=%d\n",
		    sc->sc_dev.dv_xname, sr->sr_target, sr->sr_lun);
		ncr5380_reset_scsibus(sc);
	}

	if (act_flags & ACT_CMD_DONE) {
		act_flags |= ACT_DISCONNECT;
		/* Need to call scsi_done() */
		/* XXX: from the aic6360 driver, but why? */
		if (sc->sc_datalen < 0) {
			printf("%s: %d extra bytes from %d:%d\n",
			    sc->sc_dev.dv_xname, -sc->sc_datalen,
			    sr->sr_target, sr->sr_lun);
			sc->sc_datalen = 0;
		}
		xs->resid = sc->sc_datalen;
		/* Note: this will clear sc_current */
		NCR_TRACE("machine: call done, cur=0x%x\n", (long)sr);
		ncr5380_done(sc);
	}

	if (act_flags & ACT_DISCONNECT) {
		/*
		 * The device has dropped BSY (or will soon).
		 * We have to wait here for BSY to drop, otherwise
		 * the next command may decide we need a bus reset.
		 */
		timo = ncr5380_wait_req_timo;	/* XXX */
		for (;;) {
			if (!SCI_BUSY(sc))
				goto busfree;
			if (--timo <= 0)
				break;
			delay(2);
		}
		/* Device is sitting on the bus! */
		printf("%s: Target %d LUN %d stuck busy, resetting...\n",
		    sc->sc_dev.dv_xname, sr->sr_target, sr->sr_lun);
		ncr5380_reset_scsibus(sc);
	busfree:
		NCR_TRACE("machine: discon, waited %d\n",
			ncr5380_wait_req_timo - timo);

		*sc->sci_icmd = 0;
		*sc->sci_mode = 0;
		*sc->sci_tcmd = PHASE_INVALID;
		*sc->sci_sel_enb = 0;
		SCI_CLR_INTR(sc);
		*sc->sci_sel_enb = 0x80;

		if ((act_flags & ACT_CMD_DONE) == 0) {
			__asm("_ncr5380_disconnected:");
			NCR_TRACE("machine: discon, cur=0x%x\n", (long)sr);
		}

		/*
		 * We may be here due to a disconnect message,
		 * in which case we did NOT call ncr5380_done,
		 * and we need to clear sc_current.
		 */
		sc->sc_state = NCR_IDLE;
		sc->sc_current = NULL;

		/* Paranoia: clear everything. */
		sc->sc_dataptr = NULL;
		sc->sc_datalen = 0;
		sc->sc_prevphase = PHASE_INVALID;
		sc->sc_msgpriq = 0;
		sc->sc_msgoutq = 0;
		sc->sc_msgout  = 0;

		/* Our caller will re-enable interrupts. */
	}
}


#ifdef	NCR5380_DEBUG

static void
ncr5380_show_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	u_char	*b = (u_char *) xs->cmd;
	int	i  = 0;

	if ( ! ( xs->flags & SCSI_RESET ) ) {
		printf("si(%d:%d:%d)-",
		    xs->sc_link->scsibus, xs->sc_link->target,
		    xs->sc_link->lun);
		while (i < xs->cmdlen) {
			if (i) printf(",");
			printf("%x",b[i++]);
		}
		printf("-\n");
	} else {
		printf("si(%d:%d:%d)-RESET-\n",
		    xs->sc_link->scsibus, xs->sc_link->target,
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

int ncr5380_traceidx = 0;

#define	TRACE_MAX	1024
struct trace_ent {
	char *msg;
	long  val;
} ncr5380_tracebuf[TRACE_MAX];

void
ncr5380_trace(msg, val)
	char *msg;
	long  val;
{
	register struct trace_ent *tr;
	register int s;

	s = splbio();

	tr = &ncr5380_tracebuf[ncr5380_traceidx];

	ncr5380_traceidx++;
	if (ncr5380_traceidx >= TRACE_MAX)
		ncr5380_traceidx = 0;

	tr->msg = msg;
	tr->val = val;

	splx(s);
}

#ifdef	DDB
void
ncr5380_clear_trace()
{
	ncr5380_traceidx = 0;
	bzero((char *) ncr5380_tracebuf, sizeof(ncr5380_tracebuf));
}

void
ncr5380_show_trace()
{
	struct trace_ent *tr;
	int idx;

	idx = ncr5380_traceidx;
	do {
		tr = &ncr5380_tracebuf[idx];
		idx++;
		if (idx >= TRACE_MAX)
			idx = 0;
		if (tr->msg)
			db_printf(tr->msg, tr->val);
	} while (idx != ncr5380_traceidx);
}

void
ncr5380_show_req(sr)
	struct sci_req *sr;
{
	struct scsi_xfer *xs = sr->sr_xs;

	db_printf("TID=%d ",	sr->sr_target);
	db_printf("LUN=%d ",	sr->sr_lun);
	db_printf("dh=%p ",	sr->sr_dma_hand);
	db_printf("dptr=%p ",	sr->sr_dataptr);
	db_printf("dlen=0x%x ",	sr->sr_datalen);
	db_printf("flags=%d ",	sr->sr_flags);
	db_printf("stat=%d ",	sr->sr_status);

	if (xs == NULL) {
		db_printf("(xs=NULL)\n");
		return;
	}
	db_printf("\n");
#ifdef	SCSIDEBUG
	show_scsi_xs(xs);
#else
	db_printf("xs=%p\n", xs);
#endif
}

void
ncr5380_show_state()
{
	struct ncr5380_softc *sc;
	struct sci_req *sr;
	int i, j, k;

	sc = ncr5380_debug_sc;

	if (sc == NULL) {
		db_printf("ncr5380_debug_sc == NULL\n");
		return;
	}

	db_printf("sc_ncmds=%d\n",  	sc->sc_ncmds);
	k = -1;	/* which is current? */
	for (i = 0; i < SCI_OPENINGS; i++) {
		sr = &sc->sc_ring[i];
		if (sr->sr_xs) {
			if (sr == sc->sc_current)
				k = i;
			db_printf("req %d: (sr=%p)", i, (long)sr);
			ncr5380_show_req(sr);
		}
	}
	db_printf("sc_rr=%d, current=%d\n", sc->sc_rr, k);

	db_printf("Active request matrix:\n");
	for(i = 0; i < 8; i++) {		/* targets */
		for (j = 0; j < 8; j++) {	/* LUN */
			sr = sc->sc_matrix[i][j];
			if (sr) {
				db_printf("TID=%d LUN=%d sr=0x%x\n", i, j, (long)sr);
			}
		}
	}

	db_printf("sc_state=0x%x\n",	sc->sc_state);
	db_printf("sc_current=%p\n",	sc->sc_current);
	db_printf("sc_dataptr=%p\n",	sc->sc_dataptr);
	db_printf("sc_datalen=0x%x\n",	sc->sc_datalen);

	db_printf("sc_prevphase=%d\n",	sc->sc_prevphase);
	db_printf("sc_msgpriq=0x%x\n",	sc->sc_msgpriq);
}

#endif	/* DDB */
#endif	/* NCR5380_DEBUG */

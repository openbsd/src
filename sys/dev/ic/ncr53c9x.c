/*	$OpenBSD: ncr53c9x.c,v 1.5 1998/04/30 01:43:46 jason Exp $	*/
/*	$NetBSD: ncr53c9x.c,v 1.23 1998/01/31 23:37:51 pk Exp $	*/

/*
 * Copyright (c) 1996 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 * Copyright (c) 1994 Peter Galbavy
 * Copyright (c) 1995 Paul Kranenburg
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
 *	This product includes software developed by Peter Galbavy
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Based on aic6360 by Jarle Greipsland
 *
 * Acknowledgements: Many of the algorithms used in this driver are
 * inspired by the work of Julian Elischer (julian@tfs.com) and
 * Charles Hannum (mycroft@duality.gnu.ai.mit.edu).  Thanks a million!
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/queue.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_message.h>

#include <machine/cpu.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

int ncr53c9x_debug = 0; /*NCR_SHOWPHASE|NCR_SHOWMISC|NCR_SHOWTRAC|NCR_SHOWCMDS;*/

/*static*/ void	ncr53c9x_readregs	__P((struct ncr53c9x_softc *));
/*static*/ void	ncr53c9x_select		__P((struct ncr53c9x_softc *,
					    struct ncr53c9x_ecb *));
/*static*/ int ncr53c9x_reselect	__P((struct ncr53c9x_softc *, int));
/*static*/ void	ncr53c9x_scsi_reset	__P((struct ncr53c9x_softc *));
/*static*/ void	ncr53c9x_init		__P((struct ncr53c9x_softc *, int));
/*static*/ int	ncr53c9x_poll		__P((struct ncr53c9x_softc *,
					    struct scsi_xfer *, int));
/*static*/ void	ncr53c9x_sched		__P((struct ncr53c9x_softc *));
/*static*/ void	ncr53c9x_done		__P((struct ncr53c9x_softc *,
					    struct ncr53c9x_ecb *));
/*static*/ void	ncr53c9x_msgin		__P((struct ncr53c9x_softc *));
/*static*/ void	ncr53c9x_msgout		__P((struct ncr53c9x_softc *));
/*static*/ void	ncr53c9x_timeout	__P((void *arg));
/*static*/ void	ncr53c9x_abort		__P((struct ncr53c9x_softc *,
					    struct ncr53c9x_ecb *));
/*static*/ void ncr53c9x_dequeue	__P((struct ncr53c9x_softc *,
					    struct ncr53c9x_ecb *));

void ncr53c9x_sense			__P((struct ncr53c9x_softc *,
					    struct ncr53c9x_ecb *));
void ncr53c9x_free_ecb			__P((struct ncr53c9x_softc *,
					    struct ncr53c9x_ecb *, int));
struct ncr53c9x_ecb *ncr53c9x_get_ecb	__P((struct ncr53c9x_softc *, int));

static inline int ncr53c9x_stp2cpb	__P((struct ncr53c9x_softc *, int));
static inline void ncr53c9x_setsync	__P((struct ncr53c9x_softc *,
					    struct ncr53c9x_tinfo *));

/*
 * Names for the NCR53c9x variants, correspnding to the variant tags
 * in ncr53c9xvar.h.
 */
const char *ncr53c9x_variant_names[] = {
	"ESP100",
	"ESP100A",
	"ESP200",
	"NCR53C94",
	"NCR53C96",
	"ESP406",
	"FAS408",
	"FAS216",
};

/*
 * Attach this instance, and then all the sub-devices
 */
void
ncr53c9x_attach(sc, adapter, dev)
	struct ncr53c9x_softc *sc;
	struct scsi_adapter *adapter;
	struct scsi_device *dev;
{

	/*
	 * Note, the front-end has set us up to print the chip variation.
	 */

	if (sc->sc_rev >= NCR_VARIANT_MAX) {
		printf("\n%s: unknown variant %d, devices not attached\n",
		    sc->sc_dev.dv_xname, sc->sc_rev);
		return;
	}

	printf(": %s, %dMHz, SCSI ID %d\n",
	    ncr53c9x_variant_names[sc->sc_rev], sc->sc_freq, sc->sc_id);

	sc->sc_ccf = FREQTOCCF(sc->sc_freq);

	/* The value *must not* be == 1. Make it 2 */
	if (sc->sc_ccf == 1)
		sc->sc_ccf = 2;

	/*
	 * The recommended timeout is 250ms. This register is loaded
	 * with a value calculated as follows, from the docs:
	 *
	 *		(timout period) x (CLK frequency)
	 *	reg = -------------------------------------
	 *		 8192 x (Clock Conversion Factor)
	 *
	 * Since CCF has a linear relation to CLK, this generally computes
	 * to the constant of 153.
	 */
	sc->sc_timeout = ((250 * 1000) * sc->sc_freq) / (8192 * sc->sc_ccf);

	/* CCF register only has 3 bits; 0 is actually 8 */
	sc->sc_ccf &= 7;

	/* Reset state & bus */
	sc->sc_cfflags = sc->sc_dev.dv_cfdata->cf_flags;
	sc->sc_state = 0;
	ncr53c9x_init(sc, 1);

	/*
	 * fill in the prototype scsi_link.
	 */
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = sc->sc_id;
	sc->sc_link.adapter = adapter;
	sc->sc_link.device = dev;
	sc->sc_link.openings = 2;

	/*
	 * Now try to attach all the sub-devices
	 */
	config_found(&sc->sc_dev, &sc->sc_link, scsiprint);

	/*
	 * Enable interupts from the SCSI core
	 */
	if ((sc->sc_rev == NCR_VARIANT_ESP406) ||
	    (sc->sc_rev == NCR_VARIANT_FAS408)) {
		NCR_PIOREGS(sc);
		NCR_WRITE_REG(sc, NCR_CFG5, NCRCFG5_SINT |
		    NCR_READ_REG(sc, NCR_CFG5));
		NCR_SCSIREGS(sc);
	}
}

/*
 * This is the generic esp reset function. It does not reset the SCSI bus,
 * only this controllers, but kills any on-going commands, and also stops
 * and resets the DMA.
 *
 * After reset, registers are loaded with the defaults from the attach
 * routine above.
 */
void
ncr53c9x_reset(sc)
	struct ncr53c9x_softc *sc;
{

	/* reset DMA first */
	NCRDMA_RESET(sc);

	/* reset SCSI chip */
	NCRCMD(sc, NCRCMD_RSTCHIP);
	NCRCMD(sc, NCRCMD_NOP);
	DELAY(500);

	/* do these backwards, and fall through */
	switch (sc->sc_rev) {
	case NCR_VARIANT_ESP406:
	case NCR_VARIANT_FAS408:
		NCR_SCSIREGS(sc);
	case NCR_VARIANT_FAS216:
	case NCR_VARIANT_NCR53C94:
	case NCR_VARIANT_NCR53C96:
	case NCR_VARIANT_ESP200:
		NCR_WRITE_REG(sc, NCR_CFG3, sc->sc_cfg3);
	case NCR_VARIANT_ESP100A:
		NCR_WRITE_REG(sc, NCR_CFG2, sc->sc_cfg2);
	case NCR_VARIANT_ESP100:
		NCR_WRITE_REG(sc, NCR_CFG1, sc->sc_cfg1);
		NCR_WRITE_REG(sc, NCR_CCF, sc->sc_ccf);
		NCR_WRITE_REG(sc, NCR_SYNCOFF, 0);
		NCR_WRITE_REG(sc, NCR_TIMEOUT, sc->sc_timeout);
		break;
	default:
		printf("%s: unknown revision code, assuming ESP100\n",
		    sc->sc_dev.dv_xname);
		NCR_WRITE_REG(sc, NCR_CFG1, sc->sc_cfg1);
		NCR_WRITE_REG(sc, NCR_CCF, sc->sc_ccf);
		NCR_WRITE_REG(sc, NCR_SYNCOFF, 0);
		NCR_WRITE_REG(sc, NCR_TIMEOUT, sc->sc_timeout);
	}
}

/*
 * Reset the SCSI bus, but not the chip
 */
void
ncr53c9x_scsi_reset(sc)
	struct ncr53c9x_softc *sc;
{

	(*sc->sc_glue->gl_dma_stop)(sc);

	printf("%s: resetting SCSI bus\n", sc->sc_dev.dv_xname);
	NCRCMD(sc, NCRCMD_RSTSCSI);
}

/*
 * Initialize esp state machine
 */
void
ncr53c9x_init(sc, doreset)
	struct ncr53c9x_softc *sc;
	int doreset;
{
	struct ncr53c9x_ecb *ecb;
	int r;

	NCR_TRACE(("[NCR_INIT(%d)] ", doreset));

	if (sc->sc_state == 0) {
		/* First time through; initialize. */
		TAILQ_INIT(&sc->ready_list);
		TAILQ_INIT(&sc->nexus_list);
		TAILQ_INIT(&sc->free_list);
		sc->sc_nexus = NULL;
		ecb = sc->sc_ecb;
		bzero(ecb, sizeof(sc->sc_ecb));
		for (r = 0; r < sizeof(sc->sc_ecb) / sizeof(*ecb); r++) {
			TAILQ_INSERT_TAIL(&sc->free_list, ecb, chain);
			ecb++;
		}
		bzero(sc->sc_tinfo, sizeof(sc->sc_tinfo));
	} else {
		/* Cancel any active commands. */
		sc->sc_state = NCR_CLEANING;
		if ((ecb = sc->sc_nexus) != NULL) {
			ecb->xs->error = XS_TIMEOUT;
			ncr53c9x_done(sc, ecb);
		}
		while ((ecb = sc->nexus_list.tqh_first) != NULL) {
			ecb->xs->error = XS_TIMEOUT;
			ncr53c9x_done(sc, ecb);
		}
	}

	/*
	 * reset the chip to a known state
	 */
	ncr53c9x_reset(sc);

	sc->sc_phase = sc->sc_prevphase = INVALID_PHASE;
	for (r = 0; r < 8; r++) {
		struct ncr53c9x_tinfo *ti = &sc->sc_tinfo[r];
/* XXX - config flags per target: low bits: no reselect; high bits: no synch */

		ti->flags = ((sc->sc_minsync && !(sc->sc_cfflags & (1<<(r+8))))
				? T_NEGOTIATE : 0) |
				((sc->sc_cfflags & (1<<r)) ? T_RSELECTOFF : 0) |
				T_NEED_TO_RESET;
		ti->period = sc->sc_minsync;
		ti->offset = 0;
	}

	if (doreset) {
		sc->sc_state = NCR_SBR;
		NCRCMD(sc, NCRCMD_RSTSCSI);
	} else {
		sc->sc_state = NCR_IDLE;
		ncr53c9x_sched(sc);
	}
}

/*
 * Read the NCR registers, and save their contents for later use.
 * NCR_STAT, NCR_STEP & NCR_INTR are mostly zeroed out when reading
 * NCR_INTR - so make sure it is the last read.
 *
 * I think that (from reading the docs) most bits in these registers
 * only make sense when he DMA CSR has an interrupt showing. Call only
 * if an interrupt is pending.
 */
void
ncr53c9x_readregs(sc)
	struct ncr53c9x_softc *sc;
{

	sc->sc_espstat = NCR_READ_REG(sc, NCR_STAT);
	/* Only the stepo bits are of interest */
	sc->sc_espstep = NCR_READ_REG(sc, NCR_STEP) & NCRSTEP_MASK;
	sc->sc_espintr = NCR_READ_REG(sc, NCR_INTR);

	if (sc->sc_glue->gl_clear_latched_intr != NULL)
		(*sc->sc_glue->gl_clear_latched_intr)(sc);

	/*
	 * Determine the SCSI bus phase, return either a real SCSI bus phase
	 * or some pseudo phase we use to detect certain exceptions.
	 */

	sc->sc_phase = (sc->sc_espintr & NCRINTR_DIS)
			? /* Disconnected */ BUSFREE_PHASE
			: sc->sc_espstat & NCRSTAT_PHASE;

	NCR_MISC(("regs[intr=%02x,stat=%02x,step=%02x] ",
		sc->sc_espintr, sc->sc_espstat, sc->sc_espstep));
}

/*
 * Convert Synchronous Transfer Period to chip register Clock Per Byte value.
 */
static inline int
ncr53c9x_stp2cpb(sc, period)
	struct ncr53c9x_softc *sc;
	int period;
{
	int v;
	v = (sc->sc_freq * period) / 250;
	if (ncr53c9x_cpb2stp(sc, v) < period)
		/* Correct round-down error */
		v++;
	return v;
}

static inline void
ncr53c9x_setsync(sc, ti)
	struct ncr53c9x_softc *sc;
	struct ncr53c9x_tinfo *ti;
{

	if (ti->flags & T_SYNCMODE) {
		NCR_WRITE_REG(sc, NCR_SYNCOFF, ti->offset);
		NCR_WRITE_REG(sc, NCR_SYNCTP,
		    ncr53c9x_stp2cpb(sc, ti->period));
	} else {
		NCR_WRITE_REG(sc, NCR_SYNCOFF, 0);
		NCR_WRITE_REG(sc, NCR_SYNCTP, 0);
	}
}

int ncr53c9x_dmaselect = 0;
/*
 * Send a command to a target, set the driver state to NCR_SELECTING
 * and let the caller take care of the rest.
 *
 * Keeping this as a function allows me to say that this may be done
 * by DMA instead of programmed I/O soon.
 */
void
ncr53c9x_select(sc, ecb)
	struct ncr53c9x_softc *sc;
	struct ncr53c9x_ecb *ecb;
{
	struct scsi_link *sc_link = ecb->xs->sc_link;
	int target = sc_link->target;
	int lun = sc_link->lun;
	struct ncr53c9x_tinfo *ti = &sc->sc_tinfo[target];
	int tiflags = ti->flags;
	u_char *cmd;
	int clen;

	NCR_TRACE(("[ncr53c9x_select(t%d,l%d,cmd:%x)] ",
		   target, lun, ecb->cmd.cmd.opcode));

	sc->sc_state = NCR_SELECTING;

	/*
	 * Schedule the timeout now, the first time we will go away
	 * expecting to come back due to an interrupt, because it is
	 * always possible that the interrupt may never happen.
	 */
	if ((ecb->xs->flags & SCSI_POLL) == 0)
		timeout(ncr53c9x_timeout, ecb,
		    (ecb->timeout * hz) / 1000);

	/*
	 * The docs say the target register is never reset, and I
	 * can't think of a better place to set it
	 */
	NCR_WRITE_REG(sc, NCR_SELID, target);
	ncr53c9x_setsync(sc, ti);

	if (ncr53c9x_dmaselect && (tiflags & T_NEGOTIATE) == 0) {
		size_t dmasize;

		ecb->cmd.id = 
		    MSG_IDENTIFY(lun, (tiflags & T_RSELECTOFF)?0:1);

		/* setup DMA transfer for command */
		dmasize = clen = ecb->clen + 1;
		sc->sc_cmdlen = clen;
		sc->sc_cmdp = (caddr_t)&ecb->cmd;
		NCRDMA_SETUP(sc, &sc->sc_cmdp, &sc->sc_cmdlen, 0, &dmasize);

		/* Program the SCSI counter */
		NCR_WRITE_REG(sc, NCR_TCL, dmasize);
		NCR_WRITE_REG(sc, NCR_TCM, dmasize >> 8);
		if (sc->sc_cfg2 & NCRCFG2_FE) {
			NCR_WRITE_REG(sc, NCR_TCH, dmasize >> 16);
		}

		/* load the count in */
		NCRCMD(sc, NCRCMD_NOP|NCRCMD_DMA);

		/* And get the targets attention */
		NCRCMD(sc, NCRCMD_SELATN | NCRCMD_DMA);
		NCRDMA_GO(sc);
		return;
	}

	/*
	 * Who am I. This is where we tell the target that we are
	 * happy for it to disconnect etc.
	 */
	NCR_WRITE_REG(sc, NCR_FIFO,
			MSG_IDENTIFY(lun, (tiflags & T_RSELECTOFF)?0:1));

	if (ti->flags & T_NEGOTIATE) {
		/* Arbitrate, select and stop after IDENTIFY message */
		NCRCMD(sc, NCRCMD_SELATNS);
		return;
	}

	/* Now the command into the FIFO */
	cmd = (u_char *)&ecb->cmd.cmd;
	clen = ecb->clen;
	while (clen--)
		NCR_WRITE_REG(sc, NCR_FIFO, *cmd++);

	/* And get the targets attention */
	NCRCMD(sc, NCRCMD_SELATN);
}

void
ncr53c9x_free_ecb(sc, ecb, flags)
	struct ncr53c9x_softc *sc;
	struct ncr53c9x_ecb *ecb;
	int flags;
{
	int s;

	s = splbio();

	ecb->flags = 0;
	TAILQ_INSERT_HEAD(&sc->free_list, ecb, chain);

	/*
	 * If there were none, wake anybody waiting for one to come free,
	 * starting with queued entries.
	 */
	if (ecb->chain.tqe_next == 0)
		wakeup(&sc->free_list);

	splx(s);
}

struct ncr53c9x_ecb *
ncr53c9x_get_ecb(sc, flags)
	struct ncr53c9x_softc *sc;
	int flags;
{
	struct ncr53c9x_ecb *ecb;
	int s;

	s = splbio();

	while ((ecb = sc->free_list.tqh_first) == NULL &&
	       (flags & SCSI_NOSLEEP) == 0)
		tsleep(&sc->free_list, PRIBIO, "especb", 0);
	if (ecb) {
		TAILQ_REMOVE(&sc->free_list, ecb, chain);
		ecb->flags |= ECB_ALLOC;
	}

	splx(s);
	return ecb;
}

/*
 * DRIVER FUNCTIONS CALLABLE FROM HIGHER LEVEL DRIVERS
 */

/*
 * Start a SCSI-command
 * This function is called by the higher level SCSI-driver to queue/run
 * SCSI-commands.
 */
int
ncr53c9x_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	struct ncr53c9x_softc *sc = sc_link->adapter_softc;
	struct ncr53c9x_ecb *ecb;
	int s, flags;

	NCR_TRACE(("[ncr53c9x_scsi_cmd] "));
	NCR_CMDS(("[0x%x, %d]->%d ", (int)xs->cmd->opcode, xs->cmdlen,
	    sc_link->target));

	flags = xs->flags;
	if ((ecb = ncr53c9x_get_ecb(sc, flags)) == NULL)
		return TRY_AGAIN_LATER;

	/* Initialize ecb */
	ecb->xs = xs;
	ecb->timeout = xs->timeout;

	if (flags & SCSI_RESET) {
		ecb->flags |= ECB_RESET;
		ecb->clen = 0;
		ecb->dleft = 0;
	} else {
		bcopy(xs->cmd, &ecb->cmd.cmd, xs->cmdlen);
		ecb->clen = xs->cmdlen;
		ecb->daddr = xs->data;
		ecb->dleft = xs->datalen;
	}
	ecb->stat = 0;

	s = splbio();

	TAILQ_INSERT_TAIL(&sc->ready_list, ecb, chain);
	if (sc->sc_state == NCR_IDLE)
		ncr53c9x_sched(sc);

	splx(s);

	if ((flags & SCSI_POLL) == 0)
		return SUCCESSFULLY_QUEUED;

	/* Not allowed to use interrupts, use polling instead */
	if (ncr53c9x_poll(sc, xs, ecb->timeout)) {
		ncr53c9x_timeout(ecb);
		if (ncr53c9x_poll(sc, xs, ecb->timeout))
			ncr53c9x_timeout(ecb);
	}
	return COMPLETE;
}

/*
 * Used when interrupt driven I/O isn't allowed, e.g. during boot.
 */
int
ncr53c9x_poll(sc, xs, count)
	struct ncr53c9x_softc *sc;
	struct scsi_xfer *xs;
	int count;
{

	NCR_TRACE(("[ncr53c9x_poll] "));
	while (count) {
		if (NCRDMA_ISINTR(sc)) {
			ncr53c9x_intr(sc);
		}
#if alternatively
		if (NCR_READ_REG(sc, NCR_STAT) & NCRSTAT_INT)
			ncr53c9x_intr(sc);
#endif
		if ((xs->flags & ITSDONE) != 0)
			return 0;
		if (sc->sc_state == NCR_IDLE) {
			NCR_TRACE(("[ncr53c9x_poll: rescheduling] "));
			ncr53c9x_sched(sc);
		}
		DELAY(1000);
		count--;
	}
	return 1;
}


/*
 * LOW LEVEL SCSI UTILITIES
 */

/*
 * Schedule a scsi operation.  This has now been pulled out of the interrupt
 * handler so that we may call it from ncr53c9x_scsi_cmd and ncr53c9x_done.
 * This may save us an unecessary interrupt just to get things going.
 * Should only be called when state == NCR_IDLE and at bio pl.
 */
void
ncr53c9x_sched(sc)
	struct ncr53c9x_softc *sc;
{
	struct ncr53c9x_ecb *ecb;
	struct scsi_link *sc_link;
	struct ncr53c9x_tinfo *ti;

	NCR_TRACE(("[ncr53c9x_sched] "));
	if (sc->sc_state != NCR_IDLE)
		panic("ncr53c9x_sched: not IDLE (state=%d)", sc->sc_state);

	/*
	 * Find first ecb in ready queue that is for a target/lunit
	 * combinations that is not busy.
	 */
	for (ecb = sc->ready_list.tqh_first; ecb; ecb = ecb->chain.tqe_next) {
		sc_link = ecb->xs->sc_link;
		ti = &sc->sc_tinfo[sc_link->target];
		if ((ti->lubusy & (1 << sc_link->lun)) == 0) {
			TAILQ_REMOVE(&sc->ready_list, ecb, chain);
			sc->sc_nexus = ecb;
			ncr53c9x_select(sc, ecb);
			break;
		} else
			NCR_MISC(("%d:%d busy\n",
				  sc_link->target,
				  sc_link->lun));
	}
}

void
ncr53c9x_sense(sc, ecb)
	struct ncr53c9x_softc *sc;
	struct ncr53c9x_ecb *ecb;
{
	struct scsi_xfer *xs = ecb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct ncr53c9x_tinfo *ti = &sc->sc_tinfo[sc_link->target];
	struct scsi_sense *ss = (void *)&ecb->cmd.cmd;

	NCR_MISC(("requesting sense "));
	/* Next, setup a request sense command block */
	bzero(ss, sizeof(*ss));
	ss->opcode = REQUEST_SENSE;
	ss->byte2 = sc_link->lun << 5;
	ss->length = sizeof(struct scsi_sense_data);
	ecb->clen = sizeof(*ss);
	ecb->daddr = (char *)&xs->sense;
	ecb->dleft = sizeof(struct scsi_sense_data);
	ecb->flags |= ECB_SENSE;
	ecb->timeout = NCR_SENSE_TIMEOUT;
	ti->senses++;
	if (ecb->flags & ECB_NEXUS)
		ti->lubusy &= ~(1 << sc_link->lun);
	if (ecb == sc->sc_nexus) {
		ecb->flags &= ~ECB_NEXUS;
		ncr53c9x_select(sc, ecb);
	} else {
		ncr53c9x_dequeue(sc, ecb);
		TAILQ_INSERT_HEAD(&sc->ready_list, ecb, chain);
		if (sc->sc_state == NCR_IDLE)
			ncr53c9x_sched(sc);
	}
}

/*
 * POST PROCESSING OF SCSI_CMD (usually current)
 */
void
ncr53c9x_done(sc, ecb)
	struct ncr53c9x_softc *sc;
	struct ncr53c9x_ecb *ecb;
{
	struct scsi_xfer *xs = ecb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct ncr53c9x_tinfo *ti = &sc->sc_tinfo[sc_link->target];

	NCR_TRACE(("[ncr53c9x_done(error:%x)] ", xs->error));

	untimeout(ncr53c9x_timeout, ecb);

	/*
	 * Now, if we've come here with no error code, i.e. we've kept the
	 * initial XS_NOERROR, and the status code signals that we should
	 * check sense, we'll need to set up a request sense cmd block and
	 * push the command back into the ready queue *before* any other
	 * commands for this target/lunit, else we lose the sense info.
	 * We don't support chk sense conditions for the request sense cmd.
	 */
	if (xs->error == XS_NOERROR) {
		xs->status = ecb->stat;
		if ((ecb->flags & ECB_ABORT) != 0) {
			xs->error = XS_TIMEOUT;
		} else if ((ecb->flags & ECB_SENSE) != 0) {
			xs->error = XS_SENSE;
		} else if ((ecb->stat & ST_MASK) == SCSI_CHECK) {
			/* First, save the return values */
			xs->resid = ecb->dleft;
			ncr53c9x_sense(sc, ecb);
			return;
		} else {
			xs->resid = ecb->dleft;
		}
	}

	xs->flags |= ITSDONE;

#ifdef NCR53C9X_DEBUG
	if (ncr53c9x_debug & NCR_SHOWMISC) {
		if (xs->resid != 0)
			printf("resid=%d ", xs->resid);
		if (xs->error == XS_SENSE)
			printf("sense=0x%02x\n", xs->sense.error_code);
		else
			printf("error=%d\n", xs->error);
	}
#endif

	/*
	 * Remove the ECB from whatever queue it's on.
	 */
	if (ecb->flags & ECB_NEXUS)
		ti->lubusy &= ~(1 << sc_link->lun);
	if (ecb == sc->sc_nexus) {
		sc->sc_nexus = NULL;
		if (sc->sc_state != NCR_CLEANING) {
			sc->sc_state = NCR_IDLE;
			ncr53c9x_sched(sc);
		}
	} else
		ncr53c9x_dequeue(sc, ecb);
		
	ncr53c9x_free_ecb(sc, ecb, xs->flags);
	ti->cmds++;
	scsi_done(xs);
}

void
ncr53c9x_dequeue(sc, ecb)
	struct ncr53c9x_softc *sc;
	struct ncr53c9x_ecb *ecb;
{

	if (ecb->flags & ECB_NEXUS) {
		TAILQ_REMOVE(&sc->nexus_list, ecb, chain);
		ecb->flags &= ~ECB_NEXUS;
	} else {
		TAILQ_REMOVE(&sc->ready_list, ecb, chain);
	}
}

/*
 * INTERRUPT/PROTOCOL ENGINE
 */

/*
 * Schedule an outgoing message by prioritizing it, and asserting
 * attention on the bus. We can only do this when we are the initiator
 * else there will be an illegal command interrupt.
 */
#define ncr53c9x_sched_msgout(m) \
	do {							\
		NCR_MISC(("ncr53c9x_sched_msgout %d ", m));	\
		NCRCMD(sc, NCRCMD_SETATN);			\
		sc->sc_flags |= NCR_ATN;			\
		sc->sc_msgpriq |= (m);				\
	} while (0)

int
ncr53c9x_reselect(sc, message)
	struct ncr53c9x_softc *sc;
	int message;
{
	u_char selid, target, lun;
	struct ncr53c9x_ecb *ecb;
	struct scsi_link *sc_link;
	struct ncr53c9x_tinfo *ti;

	/*
	 * The SCSI chip made a snapshot of the data bus while the reselection
	 * was being negotiated.  This enables us to determine which target did
	 * the reselect.
	 */
	selid = sc->sc_selid & ~(1 << sc->sc_id);
	if (selid & (selid - 1)) {
		printf("%s: reselect with invalid selid %02x;"
		    " sending DEVICE RESET\n", sc->sc_dev.dv_xname, selid);
		goto reset;
	}

	/*
	 * Search wait queue for disconnected cmd
	 * The list should be short, so I haven't bothered with
	 * any more sophisticated structures than a simple
	 * singly linked list.
	 */
	target = ffs(selid) - 1;
	lun = message & 0x07;
	for (ecb = sc->nexus_list.tqh_first; ecb != NULL;
	     ecb = ecb->chain.tqe_next) {
		sc_link = ecb->xs->sc_link;
		if (sc_link->target == target &&
		    sc_link->lun == lun)
			break;
	}
	if (ecb == NULL) {
		printf("%s: reselect from target %d lun %d with no nexus;"
		    " sending ABORT\n", sc->sc_dev.dv_xname, target, lun);
		goto abort;
	}

	/* Make this nexus active again. */
	TAILQ_REMOVE(&sc->nexus_list, ecb, chain);
	sc->sc_state = NCR_CONNECTED;
	sc->sc_nexus = ecb;
	ti = &sc->sc_tinfo[target];
#ifdef NCR53C9X_DEBUG
	if ((ti->lubusy & (1 << lun)) == 0) {
		printf("%s: reselect: target %d, lun %d: should be busy\n",
			sc->sc_dev.dv_xname, target, lun);
		ti->lubusy |= (1 << lun);
	}
#endif
	ncr53c9x_setsync(sc, ti);

	if (ecb->flags & ECB_RESET)
		ncr53c9x_sched_msgout(SEND_DEV_RESET);
	else if (ecb->flags & ECB_ABORT)
		ncr53c9x_sched_msgout(SEND_ABORT);

	/* Do an implicit RESTORE POINTERS. */
	sc->sc_dp = ecb->daddr;
	sc->sc_dleft = ecb->dleft;

	return (0);

reset:
	ncr53c9x_sched_msgout(SEND_DEV_RESET);
	return (1);

abort:
	ncr53c9x_sched_msgout(SEND_ABORT);
	return (1);
}

#define IS1BYTEMSG(m) (((m) != 1 && (m) < 0x20) || (m) & 0x80)
#define IS2BYTEMSG(m) (((m) & 0xf0) == 0x20)
#define ISEXTMSG(m) ((m) == 1)

/*
 * Get an incoming message as initiator.
 *
 * The SCSI bus must already be in MESSAGE_IN_PHASE and there is a
 * byte in the FIFO
 */
void
ncr53c9x_msgin(sc)
	register struct ncr53c9x_softc *sc;
{
	register int v;

	NCR_TRACE(("[ncr53c9x_msgin(curmsglen:%ld)] ", (long)sc->sc_imlen));

	if ((NCR_READ_REG(sc, NCR_FFLAG) & NCRFIFO_FF) == 0) {
		printf("%s: msgin: no msg byte available\n",
			sc->sc_dev.dv_xname);
		return;
	}

	/*
	 * Prepare for a new message.  A message should (according
	 * to the SCSI standard) be transmitted in one single
	 * MESSAGE_IN_PHASE. If we have been in some other phase,
	 * then this is a new message.
	 */
	if (sc->sc_prevphase != MESSAGE_IN_PHASE) {
		sc->sc_flags &= ~NCR_DROP_MSGI;
		sc->sc_imlen = 0;
	}

	v = NCR_READ_REG(sc, NCR_FIFO);
	NCR_MISC(("<msgbyte:0x%02x>", v));

#if 0
	if (sc->sc_state == NCR_RESELECTED && sc->sc_imlen == 0) {
		/*
		 * Which target is reselecting us? (The ID bit really)
		 */
		sc->sc_selid = v;
		NCR_MISC(("selid=0x%2x ", sc->sc_selid));
		return;
	}
#endif

	sc->sc_imess[sc->sc_imlen] = v;

	/*
	 * If we're going to reject the message, don't bother storing
	 * the incoming bytes.  But still, we need to ACK them.
	 */

	if ((sc->sc_flags & NCR_DROP_MSGI)) {
		NCRCMD(sc, NCRCMD_MSGOK);
		printf("<dropping msg byte %x>",
			sc->sc_imess[sc->sc_imlen]);
		return;
	}

	if (sc->sc_imlen >= NCR_MAX_MSG_LEN) {
		ncr53c9x_sched_msgout(SEND_REJECT);
		sc->sc_flags |= NCR_DROP_MSGI;
	} else {
		sc->sc_imlen++;
		/*
		 * This testing is suboptimal, but most
		 * messages will be of the one byte variety, so
		 * it should not effect performance
		 * significantly.
		 */
		if (sc->sc_imlen == 1 && IS1BYTEMSG(sc->sc_imess[0]))
			goto gotit;
		if (sc->sc_imlen == 2 && IS2BYTEMSG(sc->sc_imess[0]))
			goto gotit;
		if (sc->sc_imlen >= 3 && ISEXTMSG(sc->sc_imess[0]) &&
		    sc->sc_imlen == sc->sc_imess[1] + 2)
			goto gotit;
	}
	/* Ack what we have so far */
	NCRCMD(sc, NCRCMD_MSGOK);
	return;

gotit:
	NCR_MSGS(("gotmsg(%x)", sc->sc_imess[0]));
	/*
	 * Now we should have a complete message (1 byte, 2 byte
	 * and moderately long extended messages).  We only handle
	 * extended messages which total length is shorter than
	 * NCR_MAX_MSG_LEN.  Longer messages will be amputated.
	 */
	switch (sc->sc_state) {
		struct ncr53c9x_ecb *ecb;
		struct ncr53c9x_tinfo *ti;

	case NCR_CONNECTED:
		ecb = sc->sc_nexus;
		ti = &sc->sc_tinfo[ecb->xs->sc_link->target];

		switch (sc->sc_imess[0]) {
		case MSG_CMDCOMPLETE:
			NCR_MSGS(("cmdcomplete "));
			if (sc->sc_dleft < 0) {
				struct scsi_link *sc_link = ecb->xs->sc_link;
				printf("%s: %ld extra bytes from %d:%d\n",
				    sc->sc_dev.dv_xname, -(long)sc->sc_dleft,
				    sc_link->target, sc_link->lun);
				sc->sc_dleft = 0;
			}
			ecb->dleft = (ecb->flags & ECB_TENTATIVE_DONE)
				? 0
				: sc->sc_dleft;
			if ((ecb->flags & ECB_SENSE) == 0)
				ecb->xs->resid = ecb->dleft;
			sc->sc_state = NCR_CMDCOMPLETE;
			break;

		case MSG_MESSAGE_REJECT:
			NCR_MSGS(("msg reject (msgout=%x) ", sc->sc_msgout));
			switch (sc->sc_msgout) {
			case SEND_SDTR:
				sc->sc_flags &= ~NCR_SYNCHNEGO;
				ti->flags &= ~(T_NEGOTIATE | T_SYNCMODE);
				ncr53c9x_setsync(sc, ti);
				break;
			case SEND_INIT_DET_ERR:
				goto abort;
			}
			break;

		case MSG_NOOP:
			NCR_MSGS(("noop "));
			break;

		case MSG_DISCONNECT:
			NCR_MSGS(("disconnect "));
			ti->dconns++;
			sc->sc_state = NCR_DISCONNECT;

			/*
			 * Mark the fact that all bytes have moved. The
			 * target may not bother to do a SAVE POINTERS
			 * at this stage. This flag will set the residual
			 * count to zero on MSG COMPLETE.
			 */
			if (sc->sc_dleft == 0)
				ecb->flags |= ECB_TENTATIVE_DONE;

			break;

		case MSG_SAVEDATAPOINTER:
			NCR_MSGS(("save datapointer "));
			ecb->daddr = sc->sc_dp;
			ecb->dleft = sc->sc_dleft;
			break;

		case MSG_RESTOREPOINTERS:
			NCR_MSGS(("restore datapointer "));
			sc->sc_dp = ecb->daddr;
			sc->sc_dleft = ecb->dleft;
			break;

		case MSG_EXTENDED:
			NCR_MSGS(("extended(%x) ", sc->sc_imess[2]));
			switch (sc->sc_imess[2]) {
			case MSG_EXT_SDTR:
				NCR_MSGS(("SDTR period %d, offset %d ",
					sc->sc_imess[3], sc->sc_imess[4]));
				if (sc->sc_imess[1] != 3)
					goto reject;
				ti->period = sc->sc_imess[3];
				ti->offset = sc->sc_imess[4];
				ti->flags &= ~T_NEGOTIATE;
				if (sc->sc_minsync == 0 ||
				    ti->offset == 0 ||
				    ti->period > 124) {
					printf("%s:%d: async\n", "esp",
						ecb->xs->sc_link->target);
					if ((sc->sc_flags&NCR_SYNCHNEGO)
					    == 0) {
						/*
						 * target initiated negotiation
						 */
						ti->offset = 0;
						ti->flags &= ~T_SYNCMODE;
						ncr53c9x_sched_msgout(
						    SEND_SDTR);
					} else {
						/* we are async */
						ti->flags &= ~T_SYNCMODE;
					}
				} else {
					int r = 250/ti->period;
					int s = (100*250)/ti->period - 100*r;
					int p;

					p = ncr53c9x_stp2cpb(sc, ti->period);
					ti->period = ncr53c9x_cpb2stp(sc, p);
#ifdef NCR53C9X_DEBUG
					sc_print_addr(ecb->xs->sc_link);
					printf("max sync rate %d.%02dMb/s\n",
						r, s);
#endif
					if ((sc->sc_flags&NCR_SYNCHNEGO) == 0) {
						/*
						 * target initiated negotiation
						 */
						if (ti->period <
						    sc->sc_minsync)
							ti->period =
							    sc->sc_minsync;
						if (ti->offset > 15)
							ti->offset = 15;
						ti->flags &= ~T_SYNCMODE;
						ncr53c9x_sched_msgout(
						    SEND_SDTR);
					} else {
						/* we are sync */
						ti->flags |= T_SYNCMODE;
					}
				}
				sc->sc_flags &= ~NCR_SYNCHNEGO;
				ncr53c9x_setsync(sc, ti);
				break;

			default:
				printf("%s: unrecognized MESSAGE EXTENDED;"
				    " sending REJECT\n", sc->sc_dev.dv_xname);
				goto reject;
			}
			break;

		default:
			NCR_MSGS(("ident "));
			printf("%s: unrecognized MESSAGE; sending REJECT\n",
			    sc->sc_dev.dv_xname);
		reject:
			ncr53c9x_sched_msgout(SEND_REJECT);
			break;
		}
		break;

	case NCR_RESELECTED:
		if (!MSG_ISIDENTIFY(sc->sc_imess[0])) {
			printf("%s: reselect without IDENTIFY;"
			    " sending DEVICE RESET\n", sc->sc_dev.dv_xname);
			goto reset;
		}

		(void) ncr53c9x_reselect(sc, sc->sc_imess[0]);
		break;

	default:
		printf("%s: unexpected MESSAGE IN; sending DEVICE RESET\n",
		    sc->sc_dev.dv_xname);
	reset:
		ncr53c9x_sched_msgout(SEND_DEV_RESET);
		break;

	abort:
		ncr53c9x_sched_msgout(SEND_ABORT);
		break;
	}

	/* Ack last message byte */
	NCRCMD(sc, NCRCMD_MSGOK);

	/* Done, reset message pointer. */
	sc->sc_flags &= ~NCR_DROP_MSGI;
	sc->sc_imlen = 0;
}


/*
 * Send the highest priority, scheduled message
 */
void
ncr53c9x_msgout(sc)
	register struct ncr53c9x_softc *sc;
{
	struct ncr53c9x_tinfo *ti;
	struct ncr53c9x_ecb *ecb;
	size_t size;

	NCR_TRACE(("[ncr53c9x_msgout(priq:%x, prevphase:%x)]",
	    sc->sc_msgpriq, sc->sc_prevphase));

	/*
	 * XXX - the NCR_ATN flag is not in sync with the actual ATN
	 *	 condition on the SCSI bus. The 53c9x chip
	 *	 automatically turns off ATN before sending the
	 *	 message byte.  (see also the comment below in the
	 *	 default case when picking out a message to send)
	 */
	if (sc->sc_flags & NCR_ATN) {
		if (sc->sc_prevphase != MESSAGE_OUT_PHASE) {
		new:
			NCRCMD(sc, NCRCMD_FLUSH);
			DELAY(1);
			sc->sc_msgoutq = 0;
			sc->sc_omlen = 0;
		}
	} else {
		if (sc->sc_prevphase == MESSAGE_OUT_PHASE) {
			ncr53c9x_sched_msgout(sc->sc_msgoutq);
			goto new;
		} else {
			printf("%s at line %d: unexpected MESSAGE OUT phase\n",
			    sc->sc_dev.dv_xname, __LINE__);
		}
	}
			
	if (sc->sc_omlen == 0) {
		/* Pick up highest priority message */
		sc->sc_msgout = sc->sc_msgpriq & -sc->sc_msgpriq;
		sc->sc_msgoutq |= sc->sc_msgout;
		sc->sc_msgpriq &= ~sc->sc_msgout;
		sc->sc_omlen = 1;		/* "Default" message len */
		switch (sc->sc_msgout) {
		case SEND_SDTR:
			ecb = sc->sc_nexus;
			ti = &sc->sc_tinfo[ecb->xs->sc_link->target];
			sc->sc_omess[0] = MSG_EXTENDED;
			sc->sc_omess[1] = 3;
			sc->sc_omess[2] = MSG_EXT_SDTR;
			sc->sc_omess[3] = ti->period;
			sc->sc_omess[4] = ti->offset;
			sc->sc_omlen = 5;
			if ((sc->sc_flags & NCR_SYNCHNEGO) == 0) {
				ti->flags |= T_SYNCMODE;
				ncr53c9x_setsync(sc, ti);
			}
			break;
		case SEND_IDENTIFY:
			if (sc->sc_state != NCR_CONNECTED) {
				printf("%s at line %d: no nexus\n",
				    sc->sc_dev.dv_xname, __LINE__);
			}
			ecb = sc->sc_nexus;
			sc->sc_omess[0] =
			    MSG_IDENTIFY(ecb->xs->sc_link->lun, 0);
			break;
		case SEND_DEV_RESET:
			sc->sc_flags |= NCR_ABORTING;
			sc->sc_omess[0] = MSG_BUS_DEV_RESET;
			ecb = sc->sc_nexus;
			ti = &sc->sc_tinfo[ecb->xs->sc_link->target];
			ti->flags &= ~T_SYNCMODE;
			ti->flags |= T_NEGOTIATE;
			break;
		case SEND_PARITY_ERROR:
			sc->sc_omess[0] = MSG_PARITY_ERROR;
			break;
		case SEND_ABORT:
			sc->sc_flags |= NCR_ABORTING;
			sc->sc_omess[0] = MSG_ABORT;
			break;
		case SEND_INIT_DET_ERR:
			sc->sc_omess[0] = MSG_INITIATOR_DET_ERR;
			break;
		case SEND_REJECT:
			sc->sc_omess[0] = MSG_MESSAGE_REJECT;
			break;
		default:
			/*
			 * We normally do not get here, since the chip
			 * automatically turns off ATN before the last
			 * byte of a message is sent to the target.
			 * However, if the target rejects our (multi-byte)
			 * message early by switching to MSG IN phase
			 * ATN remains on, so the target may return to
			 * MSG OUT phase. If there are no scheduled messages
			 * left we send a NO-OP.
			 *
			 * XXX - Note that this leaves no useful purpose for
			 * the NCR_ATN flag.
			 */
			sc->sc_flags &= ~NCR_ATN;
			sc->sc_omess[0] = MSG_NOOP;
			break;
		}
		sc->sc_omp = sc->sc_omess;
	}

	/* (re)send the message */
	size = min(sc->sc_omlen, sc->sc_maxxfer);
	NCRDMA_SETUP(sc, &sc->sc_omp, &sc->sc_omlen, 0, &size);
	/* Program the SCSI counter */
	NCR_WRITE_REG(sc, NCR_TCL, size);
	NCR_WRITE_REG(sc, NCR_TCM, size >> 8);
	if (sc->sc_cfg2 & NCRCFG2_FE) {
		NCR_WRITE_REG(sc, NCR_TCH, size >> 16);
	}
	/* Load the count in and start the message-out transfer */
	NCRCMD(sc, NCRCMD_NOP|NCRCMD_DMA);
	NCRCMD(sc, NCRCMD_TRANS|NCRCMD_DMA);
	NCRDMA_GO(sc);
}

/*
 * This is the most critical part of the driver, and has to know
 * how to deal with *all* error conditions and phases from the SCSI
 * bus. If there are no errors and the DMA was active, then call the
 * DMA pseudo-interrupt handler. If this returns 1, then that was it
 * and we can return from here without further processing.
 *
 * Most of this needs verifying.
 */
int
ncr53c9x_intr(sc)
	register struct ncr53c9x_softc *sc;
{
	register struct ncr53c9x_ecb *ecb;
	register struct scsi_link *sc_link;
	struct ncr53c9x_tinfo *ti;
	int loop;
	size_t size;
	int nfifo;

	NCR_TRACE(("[ncr53c9x_intr] "));

	/*
	 * I have made some (maybe seriously flawed) assumptions here,
	 * but basic testing (uncomment the printf() below), show that
	 * certainly something happens when this loop is here.
	 *
	 * The idea is that many of the SCSI operations take very little
	 * time, and going away and getting interrupted is too high an
	 * overhead to pay. For example, selecting, sending a message
	 * and command and then doing some work can be done in one "pass".
	 *
	 * The DELAY is not variable because I do not understand that the
	 * DELAY loop should be fixed-time regardless of CPU speed, but
	 * I am *assuming* that the faster SCSI processors get things done
	 * quicker (sending a command byte etc), and so there is no
	 * need to be too slow.
	 *
	 * This is a heuristic. It is 2 when at 20Mhz, 2 at 25Mhz and 1
	 * at 40Mhz. This needs testing.
	 */
	for (loop = 0; 1;loop++, DELAY(50/sc->sc_freq)) {
		/* a feeling of deja-vu */
		if (!NCRDMA_ISINTR(sc))
			return (loop != 0);
#if 0
		if (loop)
			printf("*");
#endif

		/* and what do the registers say... */
		ncr53c9x_readregs(sc);

		sc->sc_intrcnt.ev_count++;

		/*
		 * At the moment, only a SCSI Bus Reset or Illegal
		 * Command are classed as errors. A disconnect is a
		 * valid condition, and we let the code check is the
		 * "NCR_BUSFREE_OK" flag was set before declaring it
		 * and error.
		 *
		 * Also, the status register tells us about "Gross
		 * Errors" and "Parity errors". Only the Gross Error
		 * is really bad, and the parity errors are dealt
		 * with later
		 *
		 * TODO
		 *	If there are too many parity error, go to slow
		 *	cable mode ?
		 */

		/* SCSI Reset */
		if (sc->sc_espintr & NCRINTR_SBR) {
			if (NCR_READ_REG(sc, NCR_FFLAG) & NCRFIFO_FF) {
				NCRCMD(sc, NCRCMD_FLUSH);
				DELAY(1);
			}
			if (sc->sc_state != NCR_SBR) {
				printf("%s: SCSI bus reset\n",
					sc->sc_dev.dv_xname);
				ncr53c9x_init(sc, 0); /* Restart everything */
				return 1;
			}
#if 0
	/*XXX*/		printf("<expected bus reset: "
				"[intr %x, stat %x, step %d]>\n",
				sc->sc_espintr, sc->sc_espstat,
				sc->sc_espstep);
#endif
			if (sc->sc_nexus)
				panic("%s: nexus in reset state",
				      sc->sc_dev.dv_xname);
			goto sched;
		}

		ecb = sc->sc_nexus;

#define NCRINTR_ERR (NCRINTR_SBR|NCRINTR_ILL)
		if (sc->sc_espintr & NCRINTR_ERR ||
		    sc->sc_espstat & NCRSTAT_GE) {

			if (sc->sc_espstat & NCRSTAT_GE) {
				/* no target ? */
				if (NCR_READ_REG(sc, NCR_FFLAG) & NCRFIFO_FF) {
					NCRCMD(sc, NCRCMD_FLUSH);
					DELAY(1);
				}
				if (sc->sc_state == NCR_CONNECTED ||
				    sc->sc_state == NCR_SELECTING) {
					ecb->xs->error = XS_TIMEOUT;
					ncr53c9x_done(sc, ecb);
				}
				return 1;
			}

			if (sc->sc_espintr & NCRINTR_ILL) {
				if (sc->sc_flags & NCR_EXPECT_ILLCMD) {
					/*
					 * Eat away "Illegal command" interrupt
					 * on a ESP100 caused by a re-selection
					 * while we were trying to select
					 * another target.
					 */
#ifdef DEBUG
					printf("%s: ESP100 work-around activated\n",
						sc->sc_dev.dv_xname);
#endif
					sc->sc_flags &= ~NCR_EXPECT_ILLCMD;
					continue;
				}
				/* illegal command, out of sync ? */
				printf("%s: illegal command: 0x%x "
				    "(state %d, phase %x, prevphase %x)\n",
					sc->sc_dev.dv_xname, sc->sc_lastcmd,
					sc->sc_state, sc->sc_phase,
					sc->sc_prevphase);
				if (NCR_READ_REG(sc, NCR_FFLAG) & NCRFIFO_FF) {
					NCRCMD(sc, NCRCMD_FLUSH);
					DELAY(1);
				}
				ncr53c9x_init(sc, 1); /* Restart everything */
				return 1;
			}
		}
		sc->sc_flags &= ~NCR_EXPECT_ILLCMD;

		/*
		 * Call if DMA is active.
		 *
		 * If DMA_INTR returns true, then maybe go 'round the loop
		 * again in case there is no more DMA queued, but a phase
		 * change is expected.
		 */
		if (NCRDMA_ISACTIVE(sc)) {
			int r = NCRDMA_INTR(sc);
			if (r == -1) {
				printf("%s: DMA error; resetting\n",
					sc->sc_dev.dv_xname);
				ncr53c9x_init(sc, 1);
			}
			/* If DMA active here, then go back to work... */
			if (NCRDMA_ISACTIVE(sc))
				return 1;

			if ((sc->sc_espstat & NCRSTAT_TC) == 0) {
				/*
				 * DMA not completed.  If we can not find a
				 * acceptable explanation, print a diagnostic.
				 */
				if (sc->sc_state == NCR_SELECTING)
					/*
					 * This can happen if we are reselected
					 * while using DMA to select a target.
					 */
					/*void*/;
				else if (sc->sc_prevphase == MESSAGE_OUT_PHASE){
					/*
					 * Our (multi-byte) message (eg SDTR)
					 * was interrupted by the target to
					 * send a MSG REJECT.
					 * Print diagnostic if current phase
					 * is not MESSAGE IN.
					 */
					if (sc->sc_phase != MESSAGE_IN_PHASE)
					    printf("%s: !TC on MSG OUT"
					       " [intr %x, stat %x, step %d]"
					       " prevphase %x, resid %x\n",
						sc->sc_dev.dv_xname,
						sc->sc_espintr,
						sc->sc_espstat,
						sc->sc_espstep,
						sc->sc_prevphase,
						sc->sc_omlen);
				} else if (sc->sc_dleft == 0) {
					/*
					 * The DMA operation was started for
					 * a DATA transfer. Print a diagnostic
					 * if the DMA counter and TC bit
					 * appear to be out of sync.
					 */
					printf("%s: !TC on DATA XFER"
					       " [intr %x, stat %x, step %d]"
					       " prevphase %x, resid %x\n",
						sc->sc_dev.dv_xname,
						sc->sc_espintr,
						sc->sc_espstat,
						sc->sc_espstep,
						sc->sc_prevphase,
						ecb?ecb->dleft:-1);
				}
			}
		}

#if 0	/* Unreliable on some NCR revisions? */
		if ((sc->sc_espstat & NCRSTAT_INT) == 0) {
			printf("%s: spurious interrupt\n",
			    sc->sc_dev.dv_xname);
			return 1;
		}
#endif

		/*
		 * check for less serious errors
		 */
		if (sc->sc_espstat & NCRSTAT_PE) {
			printf("%s: SCSI bus parity error\n",
				sc->sc_dev.dv_xname);
			if (sc->sc_prevphase == MESSAGE_IN_PHASE)
				ncr53c9x_sched_msgout(SEND_PARITY_ERROR);
			else
				ncr53c9x_sched_msgout(SEND_INIT_DET_ERR);
		}

		if (sc->sc_espintr & NCRINTR_DIS) {
			NCR_MISC(("<DISC [intr %x, stat %x, step %d]>",
				sc->sc_espintr,sc->sc_espstat,sc->sc_espstep));
			if (NCR_READ_REG(sc, NCR_FFLAG) & NCRFIFO_FF) {
				NCRCMD(sc, NCRCMD_FLUSH);
				DELAY(1);
			}
			/*
			 * This command must (apparently) be issued within
			 * 250mS of a disconnect. So here you are...
			 */
			NCRCMD(sc, NCRCMD_ENSEL);

			switch (sc->sc_state) {
			case NCR_RESELECTED:
				goto sched;

			case NCR_SELECTING:
				ecb->xs->error = XS_SELTIMEOUT;
				goto finish;

			case NCR_CONNECTED:
				if ((sc->sc_flags & NCR_SYNCHNEGO)) {
#ifdef NCR53C9X_DEBUG
					if (ecb)
						sc_print_addr(ecb->xs->sc_link);
					printf("sync nego not completed!\n");
#endif
					ti = &sc->sc_tinfo[ecb->xs->sc_link->target];
					sc->sc_flags &= ~NCR_SYNCHNEGO;
					ti->flags &=
					    ~(T_NEGOTIATE | T_SYNCMODE);
				}

				/* it may be OK to disconnect */
				if ((sc->sc_flags & NCR_ABORTING) == 0) {
					/*  
					 * Section 5.1.1 of the SCSI 2 spec
					 * suggests issuing a REQUEST SENSE
					 * following an unexpected disconnect.
					 * Some devices go into a contingent
					 * allegiance condition when
					 * disconnecting, and this is necessary
					 * to clean up their state.
					 */     
					printf("%s: unexpected disconnect; ",
					    sc->sc_dev.dv_xname);
					if (ecb->flags & ECB_SENSE) {
						printf("resetting\n");
						goto reset;
					}
					printf("sending REQUEST SENSE\n");
					untimeout(ncr53c9x_timeout, ecb);
					ncr53c9x_sense(sc, ecb);
					goto out;
				}

				ecb->xs->error = XS_TIMEOUT;
				goto finish;

			case NCR_DISCONNECT:
				TAILQ_INSERT_HEAD(&sc->nexus_list, ecb, chain);
				sc->sc_nexus = NULL;
				goto sched;

			case NCR_CMDCOMPLETE:
				goto finish;
			}
		}

		switch (sc->sc_state) {

		case NCR_SBR:
			printf("%s: waiting for SCSI Bus Reset to happen\n",
				sc->sc_dev.dv_xname);
			return 1;

		case NCR_RESELECTED:
			/*
			 * we must be continuing a message ?
			 */
			if (sc->sc_phase != MESSAGE_IN_PHASE) {
				printf("%s: target didn't identify\n",
					sc->sc_dev.dv_xname);
				ncr53c9x_init(sc, 1);
				return 1;
			}
printf("<<RESELECT CONT'd>>");
#if XXXX
			ncr53c9x_msgin(sc);
			if (sc->sc_state != NCR_CONNECTED) {
				/* IDENTIFY fail?! */
				printf("%s: identify failed\n",
					sc->sc_dev.dv_xname);
				ncr53c9x_init(sc, 1);
				return 1;
			}
#endif
			break;

		case NCR_IDLE:
		case NCR_SELECTING:
			sc->sc_msgpriq = sc->sc_msgout = sc->sc_msgoutq = 0;
			sc->sc_flags = 0;
			ecb = sc->sc_nexus;
			if (ecb != NULL && (ecb->flags & ECB_NEXUS)) {
				sc_print_addr(ecb->xs->sc_link);
				printf("ECB_NEXUS while in state %x\n",
					sc->sc_state);
			}

			if (sc->sc_espintr & NCRINTR_RESEL) {
				/*
				 * If we're trying to select a
				 * target ourselves, push our command
				 * back into the ready list.
				 */
				if (sc->sc_state == NCR_SELECTING) {
					NCR_MISC(("backoff selector "));
					untimeout(ncr53c9x_timeout, ecb);
					sc_link = ecb->xs->sc_link;
					ti = &sc->sc_tinfo[sc_link->target];
					TAILQ_INSERT_HEAD(&sc->ready_list,
					    ecb, chain);
					ecb = sc->sc_nexus = NULL;
				}
				sc->sc_state = NCR_RESELECTED;
				if (sc->sc_phase != MESSAGE_IN_PHASE) {
					/*
					 * Things are seriously fucked up.
					 * Pull the brakes, i.e. reset
					 */
					printf("%s: target didn't identify\n",
						sc->sc_dev.dv_xname);
					ncr53c9x_init(sc, 1);
					return 1;
				}
				/*
				 * The C90 only inhibits FIFO writes until
				 * reselection is complete, instead of
				 * waiting until the interrupt status register
				 * has been read. So, if the reselect happens
				 * while we were entering a command bytes (for
				 * another target) some of those bytes can
				 * appear in the FIFO here, after the
				 * interrupt is taken.
				 */
				nfifo = NCR_READ_REG(sc,NCR_FFLAG) & NCRFIFO_FF;
				if (nfifo < 2 ||
				    (nfifo > 2 &&
				     sc->sc_rev != NCR_VARIANT_ESP100)) {
					printf("%s: RESELECT: "
					    "%d bytes in FIFO! "
					    "[intr %x, stat %x, step %d, prevphase %x]\n",
						sc->sc_dev.dv_xname,
						nfifo,
						sc->sc_espintr,
						sc->sc_espstat,
						sc->sc_espstep,
						sc->sc_prevphase);
					ncr53c9x_init(sc, 1);
					return 1;
				}
				sc->sc_selid = NCR_READ_REG(sc, NCR_FIFO);
				NCR_MISC(("selid=0x%2x ", sc->sc_selid));

				/* Handle identify message */
				ncr53c9x_msgin(sc);
				if (nfifo != 2) {
					/*
					 * Note: this should not happen
					 * with `dmaselect' on.
					 */
					sc->sc_flags |= NCR_EXPECT_ILLCMD;
					NCRCMD(sc, NCRCMD_FLUSH);
				} else if (ncr53c9x_dmaselect &&
					   sc->sc_rev == NCR_VARIANT_ESP100) {
					sc->sc_flags |= NCR_EXPECT_ILLCMD;
				}

				if (sc->sc_state != NCR_CONNECTED) {
					/* IDENTIFY fail?! */
					printf("%s: identify failed\n",
						sc->sc_dev.dv_xname);
					ncr53c9x_init(sc, 1);
					return 1;
				}
				continue; /* ie. next phase expected soon */
			}

#define	NCRINTR_DONE	(NCRINTR_FC|NCRINTR_BS)
			if ((sc->sc_espintr & NCRINTR_DONE) == NCRINTR_DONE) {
				ecb = sc->sc_nexus;
				if (!ecb)
					panic("esp: not nexus at sc->sc_nexus");

				sc_link = ecb->xs->sc_link;
				ti = &sc->sc_tinfo[sc_link->target];

				switch (sc->sc_espstep) {
				case 0:
					/*
					 * The target did not respond with a
					 * message out phase - probably an old
					 * device that doesn't recognize ATN.
					 * Clear ATN and just continue, the
					 * target should be in the command
					 * phase.
					 * XXXX check for command phase?
					 */
					NCRCMD(sc, NCRCMD_RSTATN);
					break;
				case 1:
					if ((ti->flags & T_NEGOTIATE) == 0) {
						printf("%s: step 1 & !NEG\n",
							sc->sc_dev.dv_xname);
						goto reset;
					}
					if (sc->sc_phase != MESSAGE_OUT_PHASE) {
						printf("%s: !MSGOUT\n",
							sc->sc_dev.dv_xname);
						goto reset;
					}
					/* Start negotiating */
					ti->period = sc->sc_minsync;
					ti->offset = 15;
					sc->sc_flags |= NCR_SYNCHNEGO;
					ncr53c9x_sched_msgout(SEND_SDTR);
					break;
				case 3:
					/*
					 * Grr, this is supposed to mean
					 * "target left command phase
					 *  prematurely". It seems to happen
					 * regularly when sync mode is on.
					 * Look at FIFO to see if command
					 * went out.
					 * (Timing problems?)
					 */
					if (ncr53c9x_dmaselect) {
					    if (sc->sc_cmdlen == 0)
						/* Hope for the best.. */
						break;
					} else if ((NCR_READ_REG(sc, NCR_FFLAG)
					    & NCRFIFO_FF) == 0) {
						/* Hope for the best.. */
						break;
					}
					printf("(%s:%d:%d): selection failed;"
						" %d left in FIFO "
						"[intr %x, stat %x, step %d]\n",
						sc->sc_dev.dv_xname,
						sc_link->target,
						sc_link->lun,
						NCR_READ_REG(sc, NCR_FFLAG)
						 & NCRFIFO_FF,
						sc->sc_espintr, sc->sc_espstat,
						sc->sc_espstep);
					NCRCMD(sc, NCRCMD_FLUSH);
					ncr53c9x_sched_msgout(SEND_ABORT);
					return 1;
				case 2:
					/* Select stuck at Command Phase */
					NCRCMD(sc, NCRCMD_FLUSH);
				case 4:
					if (ncr53c9x_dmaselect &&
					    sc->sc_cmdlen != 0)
						printf("(%s:%d:%d): select; "
						       "%d left in DMA buffer "
						"[intr %x, stat %x, step %d]\n",
							sc->sc_dev.dv_xname,
							sc_link->target,
							sc_link->lun,
							sc->sc_cmdlen,
							sc->sc_espintr,
							sc->sc_espstat,
							sc->sc_espstep);
					/* So far, everything went fine */
					break;
				}
#if 0
				if (ecb->xs->flags & SCSI_RESET)
					ncr53c9x_sched_msgout(SEND_DEV_RESET);
				else if (ti->flags & T_NEGOTIATE)
					ncr53c9x_sched_msgout(
					    SEND_IDENTIFY | SEND_SDTR);
				else
					ncr53c9x_sched_msgout(SEND_IDENTIFY);
#endif

				ecb->flags |= ECB_NEXUS;
				ti->lubusy |= (1 << sc_link->lun);

				sc->sc_prevphase = INVALID_PHASE; /* ?? */
				/* Do an implicit RESTORE POINTERS. */
				sc->sc_dp = ecb->daddr;
				sc->sc_dleft = ecb->dleft;
				sc->sc_state = NCR_CONNECTED;
				break;
			} else {
				printf("%s: unexpected status after select"
					": [intr %x, stat %x, step %x]\n",
					sc->sc_dev.dv_xname,
					sc->sc_espintr, sc->sc_espstat,
					sc->sc_espstep);
				NCRCMD(sc, NCRCMD_FLUSH);
				DELAY(1);
				goto reset;
			}
			if (sc->sc_state == NCR_IDLE) {
				printf("%s: stray interrupt\n",
				    sc->sc_dev.dv_xname);
					return 0;
			}
			break;

		case NCR_CONNECTED:
			if (sc->sc_flags & NCR_ICCS) {
				u_char msg;

				sc->sc_flags &= ~NCR_ICCS;

				if (!(sc->sc_espintr & NCRINTR_DONE)) {
					printf("%s: ICCS: "
					      ": [intr %x, stat %x, step %x]\n",
						sc->sc_dev.dv_xname,
						sc->sc_espintr, sc->sc_espstat,
						sc->sc_espstep);
				}
				if ((NCR_READ_REG(sc, NCR_FFLAG)
				    & NCRFIFO_FF) != 2) {
					int i = (NCR_READ_REG(sc, NCR_FFLAG)
					    & NCRFIFO_FF) - 2;
					while (i--)
						(void) NCR_READ_REG(sc,
								    NCR_FIFO);
				}
				ecb->stat = NCR_READ_REG(sc, NCR_FIFO);
				msg = NCR_READ_REG(sc, NCR_FIFO);
				NCR_PHASE(("<stat:(%x,%x)>", ecb->stat, msg));
				if (msg == MSG_CMDCOMPLETE) {
					ecb->dleft =
					  (ecb->flags & ECB_TENTATIVE_DONE)
						? 0
						: sc->sc_dleft;
					if ((ecb->flags & ECB_SENSE) == 0)
						ecb->xs->resid = ecb->dleft;
					sc->sc_state = NCR_CMDCOMPLETE;
				} else
					printf("%s: STATUS_PHASE: msg %d\n",
						sc->sc_dev.dv_xname, msg);
				NCRCMD(sc, NCRCMD_MSGOK);
				continue; /* ie. wait for disconnect */
			}
			break;
		default:
			panic("%s: invalid state: %d",
			      sc->sc_dev.dv_xname,
			      sc->sc_state);
		}

		/*
		 * Driver is now in state NCR_CONNECTED, i.e. we
		 * have a current command working the SCSI bus.
		 */
		if (sc->sc_state != NCR_CONNECTED || ecb == NULL) {
			panic("esp no nexus");
		}

		switch (sc->sc_phase) {
		case MESSAGE_OUT_PHASE:
			NCR_PHASE(("MESSAGE_OUT_PHASE "));
			ncr53c9x_msgout(sc);
			sc->sc_prevphase = MESSAGE_OUT_PHASE;
			break;
		case MESSAGE_IN_PHASE:
			NCR_PHASE(("MESSAGE_IN_PHASE "));
			if (sc->sc_espintr & NCRINTR_BS) {
				NCRCMD(sc, NCRCMD_FLUSH);
				sc->sc_flags |= NCR_WAITI;
				NCRCMD(sc, NCRCMD_TRANS);
			} else if (sc->sc_espintr & NCRINTR_FC) {
				if ((sc->sc_flags & NCR_WAITI) == 0) {
					printf("%s: MSGIN: unexpected FC bit: "
						"[intr %x, stat %x, step %x]\n",
					sc->sc_dev.dv_xname,
					sc->sc_espintr, sc->sc_espstat,
					sc->sc_espstep);
				}
				sc->sc_flags &= ~NCR_WAITI;
				ncr53c9x_msgin(sc);
			} else {
				printf("%s: MSGIN: weird bits: "
					"[intr %x, stat %x, step %x]\n",
					sc->sc_dev.dv_xname,
					sc->sc_espintr, sc->sc_espstat,
					sc->sc_espstep);
			}
			sc->sc_prevphase = MESSAGE_IN_PHASE;
			break;
		case COMMAND_PHASE:
			/*
			 * Send the command block. Normally we don't see this
			 * phase because the SEL_ATN command takes care of
			 * all this. However, we end up here if either the
			 * target or we wanted to exchange some more messages
			 * first (e.g. to start negotiations).
			 */

			NCR_PHASE(("COMMAND_PHASE 0x%02x (%d) ",
				ecb->cmd.cmd.opcode, ecb->clen));
			if (NCR_READ_REG(sc, NCR_FFLAG) & NCRFIFO_FF) {
				NCRCMD(sc, NCRCMD_FLUSH);
				DELAY(1);
			}
			if (ncr53c9x_dmaselect) {
				size_t size;
				/* setup DMA transfer for command */
				size = ecb->clen;
				sc->sc_cmdlen = size;
				sc->sc_cmdp = (caddr_t)&ecb->cmd.cmd;
				NCRDMA_SETUP(sc, &sc->sc_cmdp, &sc->sc_cmdlen,
					     0, &size);
				/* Program the SCSI counter */
				NCR_WRITE_REG(sc, NCR_TCL, size);
				NCR_WRITE_REG(sc, NCR_TCM, size >> 8);
				if (sc->sc_cfg2 & NCRCFG2_FE) {
					NCR_WRITE_REG(sc, NCR_TCH, size >> 16);
				}

				/* load the count in */
				NCRCMD(sc, NCRCMD_NOP|NCRCMD_DMA);

				/* start the command transfer */
				NCRCMD(sc, NCRCMD_TRANS | NCRCMD_DMA);
				NCRDMA_GO(sc);
			} else {
				u_char *cmd = (u_char *)&ecb->cmd.cmd;
				int i;
				/* Now the command into the FIFO */
				for (i = 0; i < ecb->clen; i++)
					NCR_WRITE_REG(sc, NCR_FIFO, *cmd++);
				NCRCMD(sc, NCRCMD_TRANS);
			}
			sc->sc_prevphase = COMMAND_PHASE;
			break;
		case DATA_OUT_PHASE:
			NCR_PHASE(("DATA_OUT_PHASE [%ld] ",(long)sc->sc_dleft));
			NCRCMD(sc, NCRCMD_FLUSH);
			size = min(sc->sc_dleft, sc->sc_maxxfer);
			NCRDMA_SETUP(sc, &sc->sc_dp, &sc->sc_dleft,
				  0, &size);
			sc->sc_prevphase = DATA_OUT_PHASE;
			goto setup_xfer;
		case DATA_IN_PHASE:
			NCR_PHASE(("DATA_IN_PHASE "));
			if (sc->sc_rev == NCR_VARIANT_ESP100)
				NCRCMD(sc, NCRCMD_FLUSH);
			size = min(sc->sc_dleft, sc->sc_maxxfer);
			NCRDMA_SETUP(sc, &sc->sc_dp, &sc->sc_dleft,
				  1, &size);
			sc->sc_prevphase = DATA_IN_PHASE;
		setup_xfer:
			/* Target returned to data phase: wipe "done" memory */
			ecb->flags &= ~ECB_TENTATIVE_DONE;

			/* Program the SCSI counter */
			NCR_WRITE_REG(sc, NCR_TCL, size);
			NCR_WRITE_REG(sc, NCR_TCM, size >> 8);
			if (sc->sc_cfg2 & NCRCFG2_FE) {
				NCR_WRITE_REG(sc, NCR_TCH, size >> 16);
			}
			/* load the count in */
			NCRCMD(sc, NCRCMD_NOP|NCRCMD_DMA);

			/*
			 * Note that if `size' is 0, we've already transceived
			 * all the bytes we want but we're still in DATA PHASE.
			 * Apparently, the device needs padding. Also, a
			 * transfer size of 0 means "maximum" to the chip
			 * DMA logic.
			 */
			NCRCMD(sc,
			       (size==0?NCRCMD_TRPAD:NCRCMD_TRANS)|NCRCMD_DMA);
			NCRDMA_GO(sc);
			return 1;
		case STATUS_PHASE:
			NCR_PHASE(("STATUS_PHASE "));
			sc->sc_flags |= NCR_ICCS;
			NCRCMD(sc, NCRCMD_ICCS);
			sc->sc_prevphase = STATUS_PHASE;
			break;
		case INVALID_PHASE:
			break;
		default:
			printf("%s: unexpected bus phase; resetting\n",
			    sc->sc_dev.dv_xname);
			goto reset;
		}
	}
	panic("esp: should not get here..");

reset:
	ncr53c9x_init(sc, 1);
	return 1;

finish:
	ncr53c9x_done(sc, ecb);
	goto out;

sched:
	sc->sc_state = NCR_IDLE;
	ncr53c9x_sched(sc);
	goto out;

out:
	return 1;
}

void
ncr53c9x_abort(sc, ecb)
	struct ncr53c9x_softc *sc;
	struct ncr53c9x_ecb *ecb;
{

	/* 2 secs for the abort */
	ecb->timeout = NCR_ABORT_TIMEOUT;
	ecb->flags |= ECB_ABORT;

	if (ecb == sc->sc_nexus) {
		/*
		 * If we're still selecting, the message will be scheduled
		 * after selection is complete.
		 */
		if (sc->sc_state == NCR_CONNECTED)
			ncr53c9x_sched_msgout(SEND_ABORT);

		/*
		 * Reschedule timeout. First, cancel a queued timeout (if any)
		 * in case someone decides to call ncr53c9x_abort() from
		 * elsewhere.
		 */
		untimeout(ncr53c9x_timeout, ecb);
		timeout(ncr53c9x_timeout, ecb, (ecb->timeout * hz) / 1000);
	} else {
		/* The command should be on the nexus list */
		if ((ecb->flags & ECB_NEXUS) == 0) {
			sc_print_addr(ecb->xs->sc_link);
			printf("ncr53c9x_abort: not NEXUS\n");
			ncr53c9x_init(sc, 1);
		}
		/*
		 * Just leave the command on the nexus list.
		 * XXX - what choice do we have but to reset the SCSI
		 *	 eventually?
		 */
		if (sc->sc_state == NCR_IDLE)
			ncr53c9x_sched(sc);
	}
}

void
ncr53c9x_timeout(arg)
	void *arg;
{
	struct ncr53c9x_ecb *ecb = arg;
	struct scsi_xfer *xs = ecb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct ncr53c9x_softc *sc = sc_link->adapter_softc;
	struct ncr53c9x_tinfo *ti = &sc->sc_tinfo[sc_link->target];
	int s;

	sc_print_addr(sc_link);
	printf("%s: timed out [ecb %p (flags 0x%x, dleft %x, stat %x)], "
	       "<state %d, nexus %p, phase(l %x, c %x, p %x), resid %lx, "
	       "msg(q %x,o %x) %s>",
		sc->sc_dev.dv_xname,
		ecb, ecb->flags, ecb->dleft, ecb->stat,
		sc->sc_state, sc->sc_nexus,
		NCR_READ_REG(sc, NCR_STAT),
		sc->sc_phase, sc->sc_prevphase,
		(long)sc->sc_dleft, sc->sc_msgpriq, sc->sc_msgout,
		NCRDMA_ISACTIVE(sc) ? "DMA active" : "");
#if NCR53C9X_DEBUG > 1
	printf("TRACE: %s.", ecb->trace);
#endif

	s = splbio();

	if (ecb->flags & ECB_ABORT) {
		/* abort timed out */
		printf(" AGAIN\n");

		ncr53c9x_init(sc, 1);
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		xs->error = XS_TIMEOUT;
		ncr53c9x_abort(sc, ecb);

		/* Disable sync mode if stuck in a data phase */
		if (ecb == sc->sc_nexus &&
		    (ti->flags & T_SYNCMODE) != 0 &&
		    (sc->sc_phase & (MSGI|CDI)) == 0) {
			sc_print_addr(sc_link);
			printf("sync negotiation disabled\n");
			sc->sc_cfflags |= (1<<(sc_link->target+8));
		}
	}

	splx(s);
}

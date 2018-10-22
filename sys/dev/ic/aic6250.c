/*	$OpenBSD: aic6250.c,v 1.5 2018/10/22 17:31:25 krw Exp $	*/

/*
 * Copyright (c) 2010, 2013 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Derived from sys/dev/ic/aic6360.c under the following licence terms:
 */
/*	OpenBSD: aic6360.c,v 1.26 2011/04/03 12:42:36 krw Exp	*/
/*	$NetBSD: aic6360.c,v 1.52 1996/12/10 21:27:51 thorpej Exp $	*/
/*
 * Copyright (c) 1994, 1995, 1996 Charles Hannum.  All rights reserved.
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
 * 3. The name of the author may not be used to endorse or promote products
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
 * Acknowledgements: Many of the algorithms used in this driver are
 * inspired by the work of Julian Elischer (julian@tfs.com) and
 * Charles Hannum (mycroft@duality.gnu.ai.mit.edu).  Thanks a million!
 */

/* TODO list:
 * 1) Get the DMA stuff working.
 * 2) Get the synch stuff working (requires DMA first).
 */

/*
 * A few customizable items:
 */

/* Synchronous data transfers? */
#define AIC_USE_SYNCHRONOUS	0
#define AIC_SYNC_REQ_ACK_OFS 	8

/* Wide data transfers? */
#define	AIC_USE_WIDE		0
#define	AIC_MAX_WIDTH		0

/* Include debug functions?  At the end of this file there are a bunch of
 * functions that will print out various information regarding queued SCSI
 * commands, driver state and chip contents.  You can call them from the
 * kernel debugger.  If you set AIC_DEBUG to 0 they are not included (the
 * kernel uses less memory) but you lose the debugging facilities.
 */
#ifndef SMALL_KERNEL
#define AIC_DEBUG		1
#endif

#define	AIC_ABORT_TIMEOUT	2000	/* time to wait for abort */

/* threshold length for DMA transfer */
#define	AIC_MIN_DMA_LEN		32

/* End of customizable parameters */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <machine/intr.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_message.h>
#include <scsi/scsiconf.h>

#include <dev/ic/aic6250reg.h>
#include <dev/ic/aic6250var.h>

#ifndef DDB
#define	db_enter() panic("should call debugger here (aic6250.c)")
#endif /* ! DDB */

#ifdef AIC_DEBUG
int aic6250_debug = 0x00; /* AIC_SHOWSTART|AIC_SHOWMISC|AIC_SHOWTRACE; */
#endif

void	aic6250_minphys(struct buf *, struct scsi_link *);
void 	aic6250_init(struct aic6250_softc *);
void	aic6250_done(struct aic6250_softc *, struct aic6250_acb *);
void	aic6250_dequeue(struct aic6250_softc *, struct aic6250_acb *);
void	aic6250_scsi_cmd(struct scsi_xfer *);
int	aic6250_poll(struct aic6250_softc *, struct scsi_xfer *, int);
void	aic6250_sched_msgout(struct aic6250_softc *, uint8_t);
void	aic6250_setsync(struct aic6250_softc *, struct aic6250_tinfo *);
void	aic6250_select(struct aic6250_softc *, struct aic6250_acb *);
void	aic6250_seltimeout(void *);
void	aic6250_timeout(void *);
void	aic6250_sched(struct aic6250_softc *);
void	aic6250_scsi_reset(struct aic6250_softc *);
void	aic6250_reset(struct aic6250_softc *);
void	aic6250_acb_free(void *, void *);
void	*aic6250_acb_alloc(void *);
int	aic6250_reselect(struct aic6250_softc *, int);
void	aic6250_sense(struct aic6250_softc *, struct aic6250_acb *);
void	aic6250_msgin(struct aic6250_softc *);
void	aic6250_abort(struct aic6250_softc *, struct aic6250_acb *);
void	aic6250_msgout(struct aic6250_softc *);
void	aic6250_ack(struct aic6250_softc *);
int	aic6250_dataout_pio(struct aic6250_softc *, uint8_t *, int, int);
int	aic6250_datain_pio(struct aic6250_softc *, uint8_t *, int, int);
#ifdef AIC_DEBUG
void	aic6250_print_acb(struct aic6250_acb *);
void	aic6250_dump_driver(struct aic6250_softc *);
void	aic6250_show_scsi_cmd(struct aic6250_acb *);
void	aic6250_print_active_acb(void);
#endif

struct cfdriver oaic_cd = {
	NULL, "oaic", DV_DULL
};

struct scsi_adapter aic6250_switch = {
	.scsi_cmd = aic6250_scsi_cmd,
#ifdef notyet
	.scsi_minphys = aic6250_minphys,
#else
	.scsi_minphys = scsi_minphys,
#endif
};

/*
 * Attach the AIC6250, fill out some high and low level data structures
 */
void
aic6250_attach(struct aic6250_softc *sc)
{
	struct scsibus_attach_args saa;
	AIC_TRACE(("aic6250_attach  "));

	printf(": revision %d\n",
	    (*sc->sc_read)(sc, AIC_REV_CNTRL) & AIC_RC_MASK);
	sc->sc_state = AIC_INIT;

	if (sc->sc_freq >= 20)
		sc->sc_cr1 |= AIC_CR1_CLK_FREQ_MODE;

	/*
	 * These are the bounds of the sync period, based on the frequency of
	 * the chip's clock input and the size and offset of the sync period
	 * register.
	 *
	 * For a 20MHz clock, this gives us 25, or 100nS, or 10MB/s, as a
	 * maximum transfer rate, and 112.5, or 450nS, or 2.22MB/s, as a
	 * minimum transfer rate.
	 */
	sc->sc_minsync = (2 * 250) / sc->sc_freq;
	sc->sc_maxsync = (9 * 250) / sc->sc_freq;

	timeout_set(&sc->sc_seltimeout, aic6250_seltimeout, sc);

	aic6250_init(sc);	/* init chip and driver */

	/*
	 * Fill in the prototype scsi_link
	 */
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = sc->sc_initiator;
	sc->sc_link.adapter = &aic6250_switch;
	sc->sc_link.openings = 2;
	sc->sc_link.pool = &sc->sc_iopool;

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;

	config_found(&sc->sc_dev, &saa, scsiprint);
}

/*
 * Initialize AIC6250 chip itself.
 */
void
aic6250_reset(struct aic6250_softc *sc)
{
	/* reset chip */
	(*sc->sc_write)(sc, AIC_CONTROL_REG1, AIC_CR1_CHIP_SW_RESET);
	delay(200);
	(*sc->sc_write)(sc, AIC_CONTROL_REG1, 0);

	(*sc->sc_write)(sc, AIC_CONTROL_REG1, sc->sc_cr1);
	(*sc->sc_write)(sc, AIC_CONTROL_REG0, sc->sc_cr0 | sc->sc_initiator);
	/* asynchronous operation */
	(*sc->sc_write)(sc, AIC_OFFSET_CNTRL, 0);

	sc->sc_imr0 = sc->sc_imr1 = 0;
	(*sc->sc_write)(sc, AIC_INT_MSK_REG0, sc->sc_imr0);
	(*sc->sc_write)(sc, AIC_INT_MSK_REG1, sc->sc_imr1);

	(*sc->sc_write)(sc, AIC_DMA_BYTE_COUNT_L, 0);
	(*sc->sc_write)(sc, AIC_DMA_BYTE_COUNT_M, 0);
	(*sc->sc_write)(sc, AIC_DMA_BYTE_COUNT_H, 0);
	(*sc->sc_write)(sc, AIC_DMA_CNTRL, 0);
	(*sc->sc_write)(sc, AIC_PORT_A, 0);
	(*sc->sc_write)(sc, AIC_PORT_B, 0);
}

/* Pull the SCSI RST line for 500 us */
void
aic6250_scsi_reset(struct aic6250_softc *sc)
{
	/* reset SCSI bus */
	(*sc->sc_write)(sc, AIC_CONTROL_REG1,
	    sc->sc_cr1 | AIC_CR1_SCSI_RST_OUT);
	delay(500);
	(*sc->sc_write)(sc, AIC_CONTROL_REG1, sc->sc_cr1);
	delay(50);
}

/*
 * Initialize aic SCSI driver.
 */
void
aic6250_init(struct aic6250_softc *sc)
{
	struct aic6250_acb *acb;
	int r;

	aic6250_reset(sc);
	aic6250_scsi_reset(sc);
	aic6250_reset(sc);

	if (sc->sc_state == AIC_INIT) {
		/* First time through; initialize. */
		TAILQ_INIT(&sc->ready_list);
		TAILQ_INIT(&sc->nexus_list);
		TAILQ_INIT(&sc->free_list);
		mtx_init(&sc->sc_acb_mtx, IPL_BIO);
		scsi_iopool_init(&sc->sc_iopool, sc, aic6250_acb_alloc,
		    aic6250_acb_free);
		sc->sc_nexus = NULL;
		acb = sc->sc_acb;
		bzero(acb, sizeof(sc->sc_acb));
		for (r = 0; r < sizeof(sc->sc_acb) / sizeof(*acb); r++) {
			TAILQ_INSERT_TAIL(&sc->free_list, acb, chain);
			acb++;
		}
		bzero(&sc->sc_tinfo, sizeof(sc->sc_tinfo));
	} else {
		/* Cancel any active commands. */
		timeout_del(&sc->sc_seltimeout);
		sc->sc_state = AIC_CLEANING;
		if ((acb = sc->sc_nexus) != NULL) {
			acb->xs->error = XS_DRIVER_STUFFUP;
			timeout_del(&acb->xs->stimeout);
			aic6250_done(sc, acb);
		}
		while ((acb = TAILQ_FIRST(&sc->nexus_list)) != NULL) {
			acb->xs->error = XS_DRIVER_STUFFUP;
			timeout_del(&acb->xs->stimeout);
			aic6250_done(sc, acb);
		}
	}

	sc->sc_prevphase = PH_INVALID;
	for (r = 0; r < 8; r++) {
		struct aic6250_tinfo *ti = &sc->sc_tinfo[r];

		ti->flags = 0;
#if AIC_USE_SYNCHRONOUS
		ti->flags |= DO_SYNC;
		ti->period = sc->sc_minsync;
		ti->offset = AIC_SYNC_REQ_ACK_OFS;
#else
		ti->period = ti->offset = 0;
#endif
#if AIC_USE_WIDE
		ti->flags |= DO_WIDE;
		ti->width = AIC_MAX_WIDTH;
#else
		ti->width = 0;
#endif
	}

	sc->sc_state = AIC_IDLE;
	sc->sc_imr0 = AIC_IMR_EN_ERROR_INT;
	sc->sc_imr1 = AIC_IMR1_EN_SCSI_RST_INT;
	(*sc->sc_write)(sc, AIC_INT_MSK_REG0, sc->sc_imr0);
	(*sc->sc_write)(sc, AIC_INT_MSK_REG1, sc->sc_imr1);
}

void
aic6250_acb_free(void *xsc, void *xacb)
{
	struct aic6250_softc *sc = xsc;
	struct aic6250_acb *acb = xacb;

	mtx_enter(&sc->sc_acb_mtx);
	acb->flags = 0;
	TAILQ_INSERT_HEAD(&sc->free_list, acb, chain);
	mtx_leave(&sc->sc_acb_mtx);
}

void *
aic6250_acb_alloc(void *xsc)
{
	struct aic6250_softc *sc = xsc;
	struct aic6250_acb *acb;

	mtx_enter(&sc->sc_acb_mtx);
	acb = TAILQ_FIRST(&sc->free_list);
	if (acb) {
		TAILQ_REMOVE(&sc->free_list, acb, chain);
		acb->flags |= ACB_ALLOC;
	}
	mtx_leave(&sc->sc_acb_mtx);

	return acb;
}

/*
 * DRIVER FUNCTIONS CALLABLE FROM HIGHER LEVEL DRIVERS
 */

/*
 * Expected sequence:
 * 1) Command inserted into ready list
 * 2) Command selected for execution
 * 3) Command won arbitration and has selected target device
 * 4) Send message out (identify message, eventually also sync.negotiations)
 * 5) Send command
 * 5a) Receive disconnect message, disconnect.
 * 5b) Reselected by target
 * 5c) Receive identify message from target.
 * 6) Send or receive data
 * 7) Receive status
 * 8) Receive message (command complete etc.)
 * 9) If status == SCSI_CHECK construct a synthetic request sense SCSI cmd.
 *    Repeat 2-8 (no disconnects please...)
 */

/*
 * Start a SCSI-command
 * This function is called by the higher level SCSI-driver to queue/run
 * SCSI-commands.
 */
void
aic6250_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link *sc_link = xs->sc_link;
	struct aic6250_softc *sc = sc_link->adapter_softc;
	struct aic6250_acb *acb;
	int s, flags;

	AIC_TRACE(("aic6250_scsi_cmd  "));
	AIC_CMDS(("[0x%x, %d]->%d ", (int)xs->cmd->opcode, xs->cmdlen,
	    sc_link->target));

	flags = xs->flags;
	acb = xs->io;

	/* Initialize acb */
	acb->xs = xs;
	acb->timeout = xs->timeout;
	timeout_set(&xs->stimeout, aic6250_timeout, acb);

	if (xs->flags & SCSI_RESET) {
		acb->flags |= ACB_RESET;
		acb->scsi_cmd_length = 0;
		acb->data_length = 0;
	} else {
		bcopy(xs->cmd, &acb->scsi_cmd, xs->cmdlen);
		acb->scsi_cmd_length = xs->cmdlen;
		acb->data_addr = xs->data;
		acb->data_length = xs->datalen;
	}
	acb->target_stat = 0;

	s = splbio();

	TAILQ_INSERT_TAIL(&sc->ready_list, acb, chain);
	if (sc->sc_state == AIC_IDLE)
		aic6250_sched(sc);

	splx(s);

	if ((flags & SCSI_POLL) == 0)
		return;

	/* Not allowed to use interrupts, use polling instead */
	if (aic6250_poll(sc, xs, acb->timeout)) {
		aic6250_timeout(acb);
		if (aic6250_poll(sc, xs, acb->timeout))
			aic6250_timeout(acb);
	}
}

#ifdef notyet
/*
 * Adjust transfer size in buffer structure
 */
void
aic6250_minphys(struct buf *bp, struct scsi_link *sl)
{

	AIC_TRACE(("aic6250_minphys  "));
	if (bp->b_bcount > (AIC_NSEG << PGSHIFT))
		bp->b_bcount = (AIC_NSEG << PGSHIFT);
	minphys(bp);
}
#endif

/*
 * Used when interrupt driven I/O isn't allowed, e.g. during boot.
 */
int
aic6250_poll(struct aic6250_softc *sc, struct scsi_xfer *xs, int count)
{
	int s;
	uint8_t sr0, sr1, sr0mask, sr1mask;

	AIC_TRACE(("aic6250_poll  "));
	while (count) {
		/*
		 * If we had interrupts enabled, would we
		 * have got an interrupt?
		 */
		sr0mask = 0;
		sr1mask = 0;

		if (sc->sc_imr0 & AIC_IMR_EN_ERROR_INT)
			sr1mask |= AIC_SR1_ERROR;
		if (sc->sc_imr0 & AIC_IMR_EN_CMD_DONE_INT)
			sr1mask |= AIC_SR1_CMD_DONE;
		if (sc->sc_imr0 & AIC_IMR_EN_SEL_OUT_INT)
			sr1mask |= AIC_SR1_SEL_OUT;
		if (sc->sc_imr0 & AIC_IMR_EN_RESEL_INT)
			sr1mask |= AIC_SR1_RESELECTED;
		if (sc->sc_imr0 & AIC_IMR_EN_SELECT_INT)
			sr1mask |= AIC_SR1_SELECTED;

		if (sc->sc_imr1 & AIC_IMR1_EN_SCSI_RST_INT)
			sr0mask |= AIC_SR0_SCSI_RST_OCCURED;
#if 0 /* these bits are never set */
		if (sc->sc_imr1 & AIC_IMR1_EN_MEM_PARITY_ERR_INT)
			sr0mask |= AIC_SR0_MEMORY_PARITY_ERR;
		if (sc->sc_imr1 & AIC_IMR1_EN_PHASE_MISMATCH_INT)
			sr0mask |= AIC_SR0_PHASE_MISMATCH_ERR;
#endif
		if (sc->sc_imr1 & AIC_IMR1_EN_BUS_FREE_DETECT_INT)
			sr0mask |= AIC_SR0_BUS_FREE_DETECT;
		if (sc->sc_imr1 & AIC_IMR1_EN_SCSI_PARITY_ERR_INT)
			sr0mask |= AIC_SR0_SCSI_PARITY_ERR;
		if (sc->sc_imr1 & AIC_IMR1_EN_PHASE_CHANGE_INT)
			sr0mask |= AIC_SR0_SCSI_PHASE_CHG_ATTN;

		sr0 = (*sc->sc_read)(sc, AIC_STATUS_REG0);
		sr1 = (*sc->sc_read)(sc, AIC_STATUS_REG1);

		if ((sr0 & sr0mask) != 0 || (sr1 & sr1mask) != 0) {
			s = splbio();
			aic6250_intr(sc);
			splx(s);
		}
		if ((xs->flags & ITSDONE) != 0)
			return 0;
		delay(1000);
		count--;

		/* process the selection timeout timer as well if necessary */
		if (sc->sc_selto != 0) {
			sc->sc_selto--;
			if (sc->sc_selto == 0) {
				aic6250_seltimeout(sc);
			}
		}
	}
	return 1;
}

/*
 * LOW LEVEL SCSI UTILITIES
 */

void
aic6250_ack(struct aic6250_softc *sc)
{
	(*sc->sc_write)(sc, AIC_SCSI_SIGNAL_REG,
	    (*sc->sc_read)(sc, AIC_SCSI_SIGNAL_REG) | AIC_SS_ACK_OUT);
	while (((*sc->sc_read)(sc, AIC_SCSI_SIGNAL_REG) & AIC_SS_REQ_IN) != 0)
		continue;
	(*sc->sc_write)(sc, AIC_SCSI_SIGNAL_REG,
	    (*sc->sc_read)(sc, AIC_SCSI_SIGNAL_REG) & ~AIC_SS_ACK_OUT);
}

void
aic6250_sched_msgout(struct aic6250_softc *sc, uint8_t m)
{
	if (sc->sc_msgpriq == 0)
		(*sc->sc_write)(sc, AIC_SCSI_SIGNAL_REG,
		    sc->sc_phase | AIC_SS_ATN_OUT);
	sc->sc_msgpriq |= m;
}

/*
 * Set synchronous transfer offset and period.
 */
void
aic6250_setsync(struct aic6250_softc *sc, struct aic6250_tinfo *ti)
{
#if AIC_USE_SYNCHRONOUS
	if (ti->offset != 0)
		(*sc->sc_write)(sc, AIC_OFFSET_CNTRL,
		    ((((ti->period * sc->sc_freq) / 250 - 2) <<
		     AIC_OC_SYNC_XFER_SHIFT) & AIC_OC_SYNC_XFER_MASK) |
		    ti->offset);
	else
		(*sc->sc_write)(sc, AIC_OFFSET_CNTRL, 0);
#endif
}

/*
 * Start a selection.  This is used by aic6250_sched() to select an idle target,
 * and by aic6250_done() to immediately reselect a target to get sense
 * information.
 */
void
aic6250_select(struct aic6250_softc *sc, struct aic6250_acb *acb)
{
	struct scsi_link *sc_link = acb->xs->sc_link;
	int target = sc_link->target;
	struct aic6250_tinfo *ti = &sc->sc_tinfo[target];

	(*sc->sc_write)(sc, AIC_SCSI_ID_DATA,
	    (1 << sc->sc_initiator) | (1 << target));
	aic6250_setsync(sc, ti);

	/* Always enable reselections. */
	sc->sc_imr1 |= AIC_IMR1_EN_SCSI_RST_INT;
	sc->sc_imr1 &=
	    ~(AIC_IMR1_EN_SCSI_REQ_ON_INT | AIC_IMR1_EN_SCSI_PARITY_ERR_INT |
	      AIC_IMR1_EN_BUS_FREE_DETECT_INT | AIC_IMR1_EN_PHASE_CHANGE_INT);
	(*sc->sc_write)(sc, AIC_INT_MSK_REG1, sc->sc_imr1);
	sc->sc_imr0 = AIC_IMR_ARB_SEL_START | AIC_IMR_EN_ERROR_INT |
	    AIC_IMR_EN_CMD_DONE_INT | AIC_IMR_EN_SEL_OUT_INT |
	    AIC_IMR_EN_RESEL_INT | AIC_IMR_EN_SELECT_INT;
	(*sc->sc_write)(sc, AIC_INT_MSK_REG0, sc->sc_imr0);

	sc->sc_state = AIC_SELECTING;
}

int
aic6250_reselect(struct aic6250_softc *sc, int message)
{
	uint8_t selid, target, lun;
	struct aic6250_acb *acb;
	struct scsi_link *sc_link;
	struct aic6250_tinfo *ti;

	/*
	 * The SCSI chip made a snapshot of the data bus while the reselection
	 * was being negotiated.  This enables us to determine which target did
	 * the reselect.
	 */
	selid = sc->sc_selid & ~(1 << sc->sc_initiator);
	if (selid & (selid - 1)) {
		printf("%s: reselect with invalid selid %02x; ",
		    sc->sc_dev.dv_xname, selid);
		printf("sending DEVICE RESET\n");
		AIC_BREAK();
		goto reset;
	}

	/* Search wait queue for disconnected cmd
	 * The list should be short, so I haven't bothered with
	 * any more sophisticated structures than a simple
	 * singly linked list.
	 */
	target = ffs(selid) - 1;
	lun = message & 0x07;
	TAILQ_FOREACH(acb, &sc->nexus_list, chain) {
		sc_link = acb->xs->sc_link;
		if (sc_link->target == target && sc_link->lun == lun)
			break;
	}
	if (acb == NULL) {
		printf("%s: reselect from target %d lun %d with no nexus; ",
		    sc->sc_dev.dv_xname, target, lun);
		printf("sending ABORT\n");
		AIC_BREAK();
		goto abort;
	}

	/* Make this nexus active again. */
	TAILQ_REMOVE(&sc->nexus_list, acb, chain);
	sc->sc_state = AIC_CONNECTED;
	sc->sc_nexus = acb;
	ti = &sc->sc_tinfo[target];
	ti->lubusy |= (1 << lun);
	aic6250_setsync(sc, ti);

	if (acb->flags & ACB_RESET)
		aic6250_sched_msgout(sc, SEND_DEV_RESET);
	else if (acb->flags & ACB_ABORT)
		aic6250_sched_msgout(sc, SEND_ABORT);

	/* Do an implicit RESTORE POINTERS. */
	sc->sc_dp = acb->data_addr;
	sc->sc_dleft = acb->data_length;
	sc->sc_cp = (uint8_t *)&acb->scsi_cmd;
	sc->sc_cleft = acb->scsi_cmd_length;

	return (0);

reset:
	aic6250_sched_msgout(sc, SEND_DEV_RESET);
	return (1);

abort:
	aic6250_sched_msgout(sc, SEND_ABORT);
	return (1);
}

/*
 * Schedule a SCSI operation.  This has now been pulled out of the interrupt
 * handler so that we may call it from aic6250_scsi_cmd and aic6250_done.
 * This may save us an unnecessary interrupt just to get things going.
 * Should only be called when state == AIC_IDLE and at bio pl.
 */
void
aic6250_sched(struct aic6250_softc *sc)
{
	struct aic6250_acb *acb;
	struct scsi_link *sc_link;
	struct aic6250_tinfo *ti;

	/*
	 * Find first acb in ready queue that is for a target/lunit pair that
	 * is not busy.
	 */
	TAILQ_FOREACH(acb, &sc->ready_list, chain) {
		sc_link = acb->xs->sc_link;
		ti = &sc->sc_tinfo[sc_link->target];
		if ((ti->lubusy & (1 << sc_link->lun)) == 0) {
			AIC_MISC(("selecting %d:%d  ",
			    sc_link->target, sc_link->lun));
			TAILQ_REMOVE(&sc->ready_list, acb, chain);
			sc->sc_nexus = acb;
			aic6250_select(sc, acb);
			return;
		} else
			AIC_MISC(("%d:%d busy\n",
			    sc_link->target, sc_link->lun));
	}
	AIC_MISC(("idle  "));
	/* Nothing to start; just enable reselections and wait. */
	sc->sc_imr1 |= AIC_IMR1_EN_SCSI_RST_INT;
	sc->sc_imr1 &=
	    ~(AIC_IMR1_EN_SCSI_REQ_ON_INT | AIC_IMR1_EN_SCSI_PARITY_ERR_INT |
	      AIC_IMR1_EN_BUS_FREE_DETECT_INT | AIC_IMR1_EN_PHASE_CHANGE_INT);
	(*sc->sc_write)(sc, AIC_INT_MSK_REG1, sc->sc_imr1);
	sc->sc_imr0 = AIC_IMR_EN_ERROR_INT |
	    AIC_IMR_EN_RESEL_INT | AIC_IMR_EN_SELECT_INT;
	(*sc->sc_write)(sc, AIC_INT_MSK_REG0, sc->sc_imr0);
}

void
aic6250_sense(struct aic6250_softc *sc, struct aic6250_acb *acb)
{
	struct scsi_xfer *xs = acb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct aic6250_tinfo *ti = &sc->sc_tinfo[sc_link->target];
	struct scsi_sense *ss = (void *)&acb->scsi_cmd;

	AIC_MISC(("requesting sense  "));
	/* Next, setup a request sense command block */
	bzero(ss, sizeof(*ss));
	ss->opcode = REQUEST_SENSE;
	ss->byte2 = sc_link->lun << 5;
	ss->length = sizeof(struct scsi_sense_data);
	acb->scsi_cmd_length = sizeof(*ss);
	acb->data_addr = (char *)&xs->sense;
	acb->data_length = sizeof(struct scsi_sense_data);
	acb->flags |= ACB_SENSE;
	ti->senses++;
	if (acb->flags & ACB_NEXUS)
		ti->lubusy &= ~(1 << sc_link->lun);
	if (acb == sc->sc_nexus) {
		aic6250_select(sc, acb);
	} else {
		aic6250_dequeue(sc, acb);
		TAILQ_INSERT_HEAD(&sc->ready_list, acb, chain);
		if (sc->sc_state == AIC_IDLE)
			aic6250_sched(sc);
	}
}

/*
 * POST PROCESSING OF SCSI_CMD (usually current)
 */
void
aic6250_done(struct aic6250_softc *sc, struct aic6250_acb *acb)
{
	struct scsi_xfer *xs = acb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct aic6250_tinfo *ti = &sc->sc_tinfo[sc_link->target];

	AIC_TRACE(("aic6250_done  "));

	/*
	 * Now, if we've come here with no error code, i.e. we've kept the
	 * initial XS_NOERROR, and the status code signals that we should
	 * check sense, we'll need to set up a request sense cmd block and
	 * push the command back into the ready queue *before* any other
	 * commands for this target/lunit, else we lose the sense info.
	 * We don't support chk sense conditions for the request sense cmd.
	 */
	if (xs->error == XS_NOERROR) {
		if (acb->flags & ACB_ABORT) {
			xs->error = XS_DRIVER_STUFFUP;
		} else if (acb->flags & ACB_SENSE) {
			xs->error = XS_SENSE;
		} else if (acb->target_stat == SCSI_CHECK) {
			/* First, save the return values */
			xs->resid = acb->data_length;
			xs->status = acb->target_stat;
			aic6250_sense(sc, acb);
			return;
		} else {
			xs->resid = acb->data_length;
		}
	}

#ifdef AIC_DEBUG
	if ((aic6250_debug & AIC_SHOWMISC) != 0) {
		if (xs->resid != 0)
			printf("resid=%lu ", (u_long)xs->resid);
		if (xs->error == XS_SENSE)
			printf("sense=0x%02x\n", xs->sense.error_code);
		else
			printf("error=%d\n", xs->error);
	}
#endif

	/*
	 * Remove the ACB from whatever queue it happens to be on.
	 */
	if (acb->flags & ACB_NEXUS)
		ti->lubusy &= ~(1 << sc_link->lun);
	if (acb == sc->sc_nexus) {
		sc->sc_nexus = NULL;
		sc->sc_state = AIC_IDLE;
		aic6250_sched(sc);
	} else
		aic6250_dequeue(sc, acb);

	ti->cmds++;
	scsi_done(xs);
}

void
aic6250_dequeue(struct aic6250_softc *sc, struct aic6250_acb *acb)
{

	if (acb->flags & ACB_NEXUS) {
		TAILQ_REMOVE(&sc->nexus_list, acb, chain);
	} else {
		TAILQ_REMOVE(&sc->ready_list, acb, chain);
	}
}

/*
 * INTERRUPT/PROTOCOL ENGINE
 */

/*
 * Precondition:
 * The SCSI bus is already in the MSGI phase and there is a message byte
 * on the bus, along with an asserted REQ signal.
 */
void
aic6250_msgin(struct aic6250_softc *sc)
{
	uint8_t sr0, scsisig;
	int n;
	uint8_t msgbyte;

	AIC_TRACE(("aic6250_msgin  "));

	if (sc->sc_prevphase == PH_MSGIN) {
		/* This is a continuation of the previous message. */
		n = sc->sc_imp - sc->sc_imess;
		goto nextbyte;
	}

	/* This is a new MESSAGE IN phase.  Clean up our state. */
	sc->sc_flags &= ~AIC_DROP_MSGIN;

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
		for (;;) {
			scsisig = (*sc->sc_read)(sc, AIC_SCSI_SIGNAL_REG);
			if ((scsisig & PH_MASK) != PH_MSGIN) {
				/*
				 * Target left MESSAGE IN, probably because it
				 * a) noticed our ATN signal, or
				 * b) ran out of messages.
				 */
				goto out;
			}
			if ((scsisig & AIC_SS_REQ_IN) != 0)
				break;
		}

		/* If parity error, just dump everything on the floor. */
		sr0 = (*sc->sc_read)(sc, AIC_STATUS_REG0);
		if ((sr0 & AIC_SR0_SCSI_PARITY_ERR) != 0) {
			sc->sc_flags |= AIC_DROP_MSGIN;
			aic6250_sched_msgout(sc, SEND_PARITY_ERROR);
		}

		/* Gather incoming message bytes if needed. */
		msgbyte = (*sc->sc_read)(sc, AIC_SCSI_ID_DATA);
		if ((sc->sc_flags & AIC_DROP_MSGIN) == 0) {
			if (n >= AIC_MAX_MSG_LEN) {
				sc->sc_flags |= AIC_DROP_MSGIN;
				aic6250_sched_msgout(sc, SEND_REJECT);
			} else {
				*sc->sc_imp++ = msgbyte;
				n++;

				/*
				 * This testing is suboptimal, but most
				 * messages will be of the one byte variety, so
				 * it should not affect performance
				 * significantly.
				 */
				if (n == 1 && IS1BYTEMSG(sc->sc_imess[0]))
					break;
				if (n == 2 && IS2BYTEMSG(sc->sc_imess[0]))
					break;
				if (n >= 3 && ISEXTMSG(sc->sc_imess[0]) &&
				    n == sc->sc_imess[1] + 2)
					break;
			}
		}

		/*
		 * If we reach this spot we're either:
		 * a) in the middle of a multi-byte message, or
		 * b) dropping bytes.
		 */
		aic6250_ack(sc);
	}

	AIC_MISC(("n=%d imess=0x%02x  ", n, sc->sc_imess[0]));

	/* We now have a complete message.  Parse it. */
	switch (sc->sc_state) {
		struct aic6250_acb *acb;
		struct scsi_link *sc_link;
		struct aic6250_tinfo *ti;

	case AIC_CONNECTED:
		AIC_ASSERT(sc->sc_nexus != NULL);
		acb = sc->sc_nexus;
		ti = &sc->sc_tinfo[acb->xs->sc_link->target];

		switch (sc->sc_imess[0]) {
		case MSG_CMDCOMPLETE:
			if ((long)sc->sc_dleft < 0) {
				sc_link = acb->xs->sc_link;
				printf("%s: %lu extra bytes from %d:%d\n",
				    sc->sc_dev.dv_xname, (u_long)-sc->sc_dleft,
				    sc_link->target, sc_link->lun);
				acb->data_length = 0;
			}
			acb->xs->resid = acb->data_length = sc->sc_dleft;
			sc->sc_state = AIC_CMDCOMPLETE;
			break;

		case MSG_PARITY_ERROR:
			/* Resend the last message. */
			aic6250_sched_msgout(sc, sc->sc_lastmsg);
			break;

		case MSG_MESSAGE_REJECT:
			AIC_MISC(("message rejected %02x  ", sc->sc_lastmsg));
			switch (sc->sc_lastmsg) {
#if AIC_USE_SYNCHRONOUS + AIC_USE_WIDE
			case SEND_IDENTIFY:
				ti->flags &= ~(DO_SYNC | DO_WIDE);
				ti->period = ti->offset = 0;
				aic6250_setsync(sc, ti);
				ti->width = 0;
				break;
#endif
#if AIC_USE_SYNCHRONOUS
			case SEND_SDTR:
				ti->flags &= ~DO_SYNC;
				ti->period = ti->offset = 0;
				aic6250_setsync(sc, ti);
				break;
#endif
#if AIC_USE_WIDE
			case SEND_WDTR:
				ti->flags &= ~DO_WIDE;
				ti->width = 0;
				break;
#endif
			case SEND_INIT_DET_ERR:
				aic6250_sched_msgout(sc, SEND_ABORT);
				break;
			}
			break;

		case MSG_NOOP:
			break;

		case MSG_DISCONNECT:
			ti->dconns++;
			sc->sc_state = AIC_DISCONNECT;
			break;

		case MSG_SAVEDATAPOINTER:
			acb->data_addr = sc->sc_dp;
			acb->data_length = sc->sc_dleft;
			break;

		case MSG_RESTOREPOINTERS:
			sc->sc_dp = acb->data_addr;
			sc->sc_dleft = acb->data_length;
			sc->sc_cp = (uint8_t *)&acb->scsi_cmd;
			sc->sc_cleft = acb->scsi_cmd_length;
			break;

		case MSG_EXTENDED:
			switch (sc->sc_imess[2]) {
#if AIC_USE_SYNCHRONOUS
			case MSG_EXT_SDTR:
				if (sc->sc_imess[1] != 3)
					goto reject;
				ti->period = sc->sc_imess[3];
				ti->offset = sc->sc_imess[4];
				ti->flags &= ~DO_SYNC;
				if (ti->offset == 0) {
				} else if (ti->period < sc->sc_minsync ||
				    ti->period > sc->sc_maxsync ||
				    ti->offset > 8) {
					ti->period = ti->offset = 0;
					aic6250_sched_msgout(sc, SEND_SDTR);
				} else {
					sc_print_addr(acb->xs->sc_link);
					printf("sync, offset %d, ",
					    ti->offset);
					printf("period %dnsec\n",
					    ti->period * 4);
				}
				aic6250_setsync(sc, ti);
				break;
#endif

#if AIC_USE_WIDE
			case MSG_EXT_WDTR:
				if (sc->sc_imess[1] != 2)
					goto reject;
				ti->width = sc->sc_imess[3];
				ti->flags &= ~DO_WIDE;
				if (ti->width == 0) {
				} else if (ti->width > AIC_MAX_WIDTH) {
					ti->width = 0;
					aic6250_sched_msgout(sc, SEND_WDTR);
				} else {
					sc_print_addr(acb->xs->sc_link);
					printf("wide, width %d\n",
					    1 << (3 + ti->width));
				}
				break;
#endif

			default:
				printf("%s: unrecognized MESSAGE EXTENDED; ",
				    sc->sc_dev.dv_xname);
				printf("sending REJECT\n");
				AIC_BREAK();
				goto reject;
			}
			break;

		default:
			printf("%s: unrecognized MESSAGE; sending REJECT\n",
			    sc->sc_dev.dv_xname);
			AIC_BREAK();
		reject:
			aic6250_sched_msgout(sc, SEND_REJECT);
			break;
		}
		break;

	case AIC_RESELECTED:
		if (!MSG_ISIDENTIFY(sc->sc_imess[0])) {
			printf("%s: reselect without IDENTIFY; ",
			    sc->sc_dev.dv_xname);
			printf("sending DEVICE RESET\n");
			AIC_BREAK();
			goto reset;
		}

		(void) aic6250_reselect(sc, sc->sc_imess[0]);
		break;

	default:
		printf("%s: unexpected MESSAGE IN; sending DEVICE RESET\n",
		    sc->sc_dev.dv_xname);
		AIC_BREAK();
	reset:
		aic6250_sched_msgout(sc, SEND_DEV_RESET);
		break;

#ifdef notdef
	abort:
		aic6250_sched_msgout(sc, SEND_ABORT);
		break;
#endif
	}

	aic6250_ack(sc);

	/* Go get the next message, if any. */
	goto nextmsg;

out:
	AIC_MISC(("n=%d imess=0x%02x  ", n, sc->sc_imess[0]));

	/*
	 * We need to explicitly un-busy.
	 */
	(*sc->sc_write)(sc, AIC_SCSI_SIGNAL_REG,
	    (*sc->sc_read)(sc, AIC_SCSI_SIGNAL_REG) &
	    ~(AIC_SS_SEL_OUT | AIC_SS_BSY_OUT | AIC_SS_ACK_OUT));
}

/*
 * Send the highest priority, scheduled message.
 */
void
aic6250_msgout(struct aic6250_softc *sc)
{
#if AIC_USE_SYNCHRONOUS
	struct aic6250_tinfo *ti;
#endif
	uint8_t scsisig;
	int n;

	AIC_TRACE(("aic6250_msgout  "));

	if (sc->sc_prevphase == PH_MSGOUT) {
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
			AIC_MISC(("retransmitting  "));
			sc->sc_msgpriq |= sc->sc_msgoutq;
			/*
			 * Set ATN.  If we're just sending a trivial 1-byte
			 * message, we'll clear ATN later on anyway.
			 */
			(*sc->sc_write)(sc, AIC_SCSI_SIGNAL_REG,
			    PH_MSGOUT | AIC_SS_ATN_OUT);
		} else {
			/* This is a continuation of the previous message. */
			n = sc->sc_omp - sc->sc_omess;
			goto nextbyte;
		}
	}

	/* No messages transmitted so far. */
	sc->sc_msgoutq = 0;
	sc->sc_lastmsg = 0;

nextmsg:
	/* Pick up highest priority message. */
	sc->sc_currmsg = sc->sc_msgpriq & -sc->sc_msgpriq;
	sc->sc_msgpriq &= ~sc->sc_currmsg;
	sc->sc_msgoutq |= sc->sc_currmsg;

	/* Build the outgoing message data. */
	switch (sc->sc_currmsg) {
	case SEND_IDENTIFY:
		AIC_ASSERT(sc->sc_nexus != NULL);
		sc->sc_omess[0] =
		    MSG_IDENTIFY(sc->sc_nexus->xs->sc_link->lun, 1);
		n = 1;
		break;

#if AIC_USE_SYNCHRONOUS
	case SEND_SDTR:
		AIC_ASSERT(sc->sc_nexus != NULL);
		ti = &sc->sc_tinfo[sc->sc_nexus->xs->sc_link->target];
		sc->sc_omess[4] = MSG_EXTENDED;
		sc->sc_omess[3] = 3;
		sc->sc_omess[2] = MSG_EXT_SDTR;
		sc->sc_omess[1] = ti->period >> 2;
		sc->sc_omess[0] = ti->offset;
		n = 5;
		break;
#endif

#if AIC_USE_WIDE
	case SEND_WDTR:
		AIC_ASSERT(sc->sc_nexus != NULL);
		ti = &sc->sc_tinfo[sc->sc_nexus->xs->sc_link->target];
		sc->sc_omess[3] = MSG_EXTENDED;
		sc->sc_omess[2] = 2;
		sc->sc_omess[1] = MSG_EXT_WDTR;
		sc->sc_omess[0] = ti->width;
		n = 4;
		break;
#endif

	case SEND_DEV_RESET:
		sc->sc_flags |= AIC_ABORTING;
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
		sc->sc_flags |= AIC_ABORTING;
		sc->sc_omess[0] = MSG_ABORT;
		n = 1;
		break;

	default:
		printf("%s: unexpected MESSAGE OUT; sending NOOP\n",
		    sc->sc_dev.dv_xname);
		AIC_BREAK();
		sc->sc_omess[0] = MSG_NOOP;
		n = 1;
		break;
	}
	sc->sc_omp = &sc->sc_omess[n];

nextbyte:
	/* Send message bytes. */
	for (;;) {
		for (;;) {
			scsisig = (*sc->sc_read)(sc, AIC_SCSI_SIGNAL_REG);
			if ((scsisig & PH_MASK) != PH_MSGOUT) {
				/*
				 * Target left MESSAGE OUT, possibly to reject
				 * our message.
				 *
				 * If this is the last message being sent, then
				 * we deassert ATN, since either the target is
				 * going to ignore this message, or it's going
				 * to ask for a retransmission via MESSAGE
				 * PARITY ERROR (in which case we reassert ATN
				 * anyway).
				 */
				if (sc->sc_msgpriq == 0)
					(*sc->sc_write)(sc, AIC_SCSI_SIGNAL_REG,
					    scsisig & ~AIC_SS_ATN_OUT);
				return;
			}
			if ((scsisig & AIC_SS_REQ_IN) != 0)
				break;
		}

		/* Clear ATN before last byte if this is the last message. */
		if (n == 1 && sc->sc_msgpriq == 0)
			(*sc->sc_write)(sc, AIC_SCSI_SIGNAL_REG,
			    scsisig & ~AIC_SS_ATN_OUT);
		/* Send message byte. */
		(*sc->sc_write)(sc, AIC_SCSI_ID_DATA, *--sc->sc_omp);
		--n;
		/* Keep track of the last message we've sent any bytes of. */
		sc->sc_lastmsg = sc->sc_currmsg;

		aic6250_ack(sc);

		if (n == 0)
			break;
	}

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
}

/* aic6250_dataout_pio: perform a data transfer in CPU-controlled PIO mode.
 * Precondition: The SCSI bus should be in the DOUT or CMDOUT phase, with REQ
 * asserted and ACK deasserted (i.e. waiting for a data byte).
 */
int
aic6250_dataout_pio(struct aic6250_softc *sc, uint8_t *p, int n, int phase)
{
	uint8_t scsisig;
	int out = 0;

	sc->sc_imr1 &= ~AIC_IMR1_EN_SCSI_REQ_ON_INT;
	(*sc->sc_write)(sc, AIC_INT_MSK_REG1, sc->sc_imr1);

	/* I have tried to make the main loop as tight as possible.  This
	 * means that some of the code following the loop is a bit more
	 * complex than otherwise.
	 */
	while (out != n) {
		for (;;) {
			scsisig = (*sc->sc_read)(sc, AIC_SCSI_SIGNAL_REG);
			if ((scsisig & AIC_SS_REQ_IN) != 0)
				break;
		}
		if ((scsisig & PH_MASK) != phase)
			break;

		(*sc->sc_write)(sc, AIC_SCSI_ID_DATA, *p++);
		out++;

		aic6250_ack(sc);
	}

	sc->sc_imr1 |= AIC_IMR1_EN_SCSI_REQ_ON_INT;
	(*sc->sc_write)(sc, AIC_INT_MSK_REG1, sc->sc_imr1);

	return out;
}

/* aic6250_datain_pio: perform data transfers using the FIFO datapath in the
 * aic6250.
 * Precondition: The SCSI bus should be in the DIN or STAT phase, with REQ
 * asserted and ACK deasserted (i.e. at least one byte is ready).
 * For now, uses a pretty dumb algorithm, hangs around until all data has been
 * transferred.  This, is OK for fast targets, but not so smart for slow
 * targets which don't disconnect or for huge transfers.
 */
int
aic6250_datain_pio(struct aic6250_softc *sc, uint8_t *p, int n, int phase)
{
	uint8_t scsisig;
	int in = 0;

	sc->sc_imr1 &= ~AIC_IMR1_EN_SCSI_REQ_ON_INT;
	(*sc->sc_write)(sc, AIC_INT_MSK_REG1, sc->sc_imr1);

	/* We leave this loop if one or more of the following is true:
	 * a) phase != PH_DATAIN && FIFOs are empty
	 * b) SCSIRSTI is set (a reset has occurred) or busfree is detected.
	 */
	while (in != n) {
		for (;;) {
			scsisig = (*sc->sc_read)(sc, AIC_SCSI_SIGNAL_REG);
			if ((scsisig & AIC_SS_REQ_IN) != 0)
				break;
		}
		if ((scsisig & PH_MASK) != phase)
			break;

		*p++ = (*sc->sc_read)(sc, AIC_SCSI_ID_DATA);
		in++;

		aic6250_ack(sc);
	}

	sc->sc_imr1 |= AIC_IMR1_EN_SCSI_REQ_ON_INT;
	(*sc->sc_write)(sc, AIC_INT_MSK_REG1, sc->sc_imr1);

	return in;
}

/*
 * This is the workhorse routine of the driver.
 * Deficiencies (for now):
 * 1) always uses programmed I/O
 */
int
aic6250_intr(void *arg)
{
	struct aic6250_softc *sc = arg;
	uint8_t sr1, sr0;
	struct aic6250_acb *acb;
	struct scsi_link *sc_link;
	struct aic6250_tinfo *ti;
	int n, first = 1;

	/* Read SR1 before writing to IMR0 (which will reset some SR1 bits). */
	sr1 = (*sc->sc_read)(sc, AIC_STATUS_REG1);
	(*sc->sc_write)(sc, AIC_INT_MSK_REG0, 0);

	AIC_TRACE(("aic6250_intr  "));

loop:
	sr0 = (*sc->sc_read)(sc, AIC_STATUS_REG0);

	/*
	 * First check for abnormal conditions, such as reset.
	 */
	AIC_MISC(("sr0:0x%02x ", sr0));

	/*
	 * Check for the end of a DMA operation before doing anything else...
	 */
	if ((sc->sc_flags & AIC_DOINGDMA) != 0 &&
	    (sr0 & AIC_SR1_CMD_DONE) != 0) {
		(*sc->sc_dma_done)(sc);
	}

	if ((sr0 & AIC_SR0_SCSI_RST_OCCURED) != 0) {
		printf("%s: SCSI bus reset\n", sc->sc_dev.dv_xname);
		while (((*sc->sc_read)(sc, AIC_STATUS_REG1) &
		    AIC_SR1_SCSI_RST_IN) != 0)
			delay(5);
		goto reset;
	}

	/*
	 * Check for less serious errors.
	 */
	if ((sr0 & AIC_SR0_SCSI_PARITY_ERR) != 0) {
		printf("%s: SCSI bus parity error\n", sc->sc_dev.dv_xname);
		if (sc->sc_prevphase == PH_MSGIN) {
			sc->sc_flags |= AIC_DROP_MSGIN;
			aic6250_sched_msgout(sc, SEND_PARITY_ERROR);
		} else
			aic6250_sched_msgout(sc, SEND_INIT_DET_ERR);
	}


	/*
	 * If we're not already busy doing something test for the following
	 * conditions:
	 * 1) We have been reselected by something
	 * 2) We have selected something successfully
	 * 3) Our selection process has timed out
	 * 4) This is really a bus free interrupt just to get a new command
	 *    going?
	 * 5) Spurious interrupt?
	 */
	switch (sc->sc_state) {
	case AIC_IDLE:
	case AIC_SELECTING:
		if (first)
			first = 0;
		else
			sr1 = (*sc->sc_read)(sc, AIC_STATUS_REG1);
		AIC_MISC(("sr1:0x%02x ", sr1));

		if (sc->sc_state == AIC_SELECTING &&
		   (sr1 & AIC_SR1_SEL_OUT) != 0) {
			/* start selection timeout */
			AIC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;
			if ((acb->xs->flags & SCSI_POLL) != 0)
				sc->sc_selto = 250;	/* msec */
			else
				timeout_add_msec(&sc->sc_seltimeout, 250);
			sc->sc_imr0 &= ~AIC_IMR_EN_SEL_OUT_INT;
			goto out;
		}

		if ((sr1 & AIC_SR1_RESELECTED) != 0) {
			AIC_MISC(("reselected  "));

			/* kill selection timeout timer */
			sc->sc_imr0 &=
			    ~(AIC_IMR_EN_SEL_OUT_INT | AIC_IMR_EN_CMD_DONE_INT);
			timeout_del(&sc->sc_seltimeout);
			sc->sc_selto = 0;

			/*
			 * If we're trying to select a target ourselves,
			 * push our command back into the ready list.
			 */
			if (sc->sc_state == AIC_SELECTING) {
				AIC_MISC(("backoff selector  "));
				AIC_ASSERT(sc->sc_nexus != NULL);
				acb = sc->sc_nexus;
				sc->sc_nexus = NULL;
				TAILQ_INSERT_HEAD(&sc->ready_list, acb, chain);
			}

			/* Save reselection ID. */
			sc->sc_selid = (*sc->sc_read)(sc, AIC_SOURCE_DEST_ID);

			sc->sc_state = AIC_RESELECTED;
		} else if ((sr1 & (AIC_SR1_SELECTED | AIC_SR1_CMD_DONE)) != 0) {
			AIC_MISC(("selected  "));

			/* kill selection timeout timer */
			sc->sc_imr0 &=
			    ~(AIC_IMR_EN_SEL_OUT_INT | AIC_IMR_EN_CMD_DONE_INT);
			timeout_del(&sc->sc_seltimeout);
			sc->sc_selto = 0;

			/* We have selected a target. Things to do:
			 * a) Determine what message(s) to send.
			 * b) Verify that we're still selecting the target.
			 * c) Mark device as busy.
			 */
			if (sc->sc_state != AIC_SELECTING) {
				printf("%s: selection out while idle; ",
				    sc->sc_dev.dv_xname);
				printf("resetting\n");
				AIC_BREAK();
				goto reset;
			}
			AIC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;
			sc_link = acb->xs->sc_link;
			ti = &sc->sc_tinfo[sc_link->target];

			sc->sc_msgpriq = SEND_IDENTIFY;
			if (acb->flags & ACB_RESET)
				sc->sc_msgpriq |= SEND_DEV_RESET;
			else if (acb->flags & ACB_ABORT)
				sc->sc_msgpriq |= SEND_ABORT;
			else {
#if AIC_USE_SYNCHRONOUS
				if ((ti->flags & DO_SYNC) != 0)
					sc->sc_msgpriq |= SEND_SDTR;
#endif
#if AIC_USE_WIDE
				if ((ti->flags & DO_WIDE) != 0)
					sc->sc_msgpriq |= SEND_WDTR;
#endif
			}

			acb->flags |= ACB_NEXUS;
			ti->lubusy |= (1 << sc_link->lun);

			/* Do an implicit RESTORE POINTERS. */
			sc->sc_dp = acb->data_addr;
			sc->sc_dleft = acb->data_length;
			sc->sc_cp = (uint8_t *)&acb->scsi_cmd;
			sc->sc_cleft = acb->scsi_cmd_length;

			/* On our first connection, schedule a timeout. */
			if ((acb->xs->flags & SCSI_POLL) == 0)
				timeout_add_msec(&acb->xs->stimeout,
				    acb->timeout);

			sc->sc_state = AIC_CONNECTED;
		} else {
			if (sc->sc_state != AIC_IDLE) {
				printf("%s: BUS FREE while not idle; ",
				    sc->sc_dev.dv_xname);
				printf("state=%d\n", sc->sc_state);
				AIC_BREAK();
				goto out;
			}

			goto sched;
		}

		/*
		 * Turn off selection stuff, and prepare to catch bus free
		 * interrupts, parity errors, and phase changes.
		 */
		sc->sc_imr1 |=
		    AIC_IMR1_EN_SCSI_REQ_ON_INT | AIC_IMR1_EN_SCSI_RST_INT |
		    AIC_IMR1_EN_BUS_FREE_DETECT_INT |
		    AIC_IMR1_EN_SCSI_PARITY_ERR_INT |
		    AIC_IMR1_EN_PHASE_CHANGE_INT;
		(*sc->sc_write)(sc, AIC_INT_MSK_REG1, sc->sc_imr1);

		sc->sc_flags = 0;
		sc->sc_prevphase = PH_INVALID;
		goto dophase;
	}

	if ((sr0 & AIC_SR0_BUS_FREE_DETECT) != 0) {
		/* We've gone to BUS FREE phase. */
		switch (sc->sc_state) {
		case AIC_RESELECTED:
			goto sched;

		case AIC_CONNECTED:
			AIC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;

#if AIC_USE_SYNCHRONOUS + AIC_USE_WIDE
			if (sc->sc_prevphase == PH_MSGOUT) {
				/*
				 * If the target went to BUS FREE phase during
				 * or immediately after sending a SDTR or WDTR
				 * message, disable negotiation.
				 */
				sc_link = acb->xs->sc_link;
				ti = &sc->sc_tinfo[sc_link->target];
				switch (sc->sc_lastmsg) {
#if AIC_USE_SYNCHRONOUS
				case SEND_SDTR:
					ti->flags &= ~DO_SYNC;
					ti->period = ti->offset = 0;
					break;
#endif
#if AIC_USE_WIDE
				case SEND_WDTR:
					ti->flags &= ~DO_WIDE;
					ti->width = 0;
					break;
#endif
				}
			}
#endif

			if ((sc->sc_flags & AIC_ABORTING) == 0) {
				/*
				 * Section 5.1.1 of the SCSI 2 spec suggests
				 * issuing a REQUEST SENSE following an
				 * unexpected disconnect.  Some devices go into
				 * a contingent allegiance condition when
				 * disconnecting, and this is necessary to
				 * clean up their state.
				 */
				printf("%s: unexpected disconnect; ",
				    sc->sc_dev.dv_xname);
				printf("sending REQUEST SENSE\n");
				AIC_BREAK();
				aic6250_sense(sc, acb);
				goto out;
			}

			acb->xs->error = XS_DRIVER_STUFFUP;
			goto finish;

		case AIC_DISCONNECT:
			AIC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;
#if 1 /* XXX */
			acb->data_addr = sc->sc_dp;
			acb->data_length = sc->sc_dleft;
#endif
			TAILQ_INSERT_HEAD(&sc->nexus_list, acb, chain);
			sc->sc_nexus = NULL;
			goto sched;

		case AIC_CMDCOMPLETE:
			AIC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;
			goto finish;
		}
	}

	/*
	 * Do not change phase (yet) if we have a pending DMA operation.
	 */
	if ((sc->sc_flags & AIC_DOINGDMA) != 0) {
		goto out;
	}

dophase:
	if ((sr0 & AIC_SR0_SCSI_REQ_ON) == 0) {
		/* Wait for AIC_SR0_SCSI_REQ_ON. */
		goto out;
	}

	sc->sc_phase = (*sc->sc_read)(sc, AIC_SCSI_SIGNAL_REG) & PH_MASK;
	(*sc->sc_write)(sc, AIC_SCSI_SIGNAL_REG, sc->sc_phase);

	switch (sc->sc_phase) {
	case PH_MSGOUT:
		if (sc->sc_state != AIC_CONNECTED &&
		    sc->sc_state != AIC_RESELECTED)
			break;
		aic6250_msgout(sc);
		sc->sc_prevphase = PH_MSGOUT;
		goto loop;

	case PH_MSGIN:
		if (sc->sc_state != AIC_CONNECTED &&
		    sc->sc_state != AIC_RESELECTED)
			break;
		aic6250_msgin(sc);
		sc->sc_prevphase = PH_MSGIN;
		goto loop;

	case PH_CMD:
		if (sc->sc_state != AIC_CONNECTED)
			break;
#ifdef AIC_DEBUG
		if ((aic6250_debug & AIC_SHOWMISC) != 0) {
			AIC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;
			printf("cmd=0x%02x+%d ",
			    acb->scsi_cmd.opcode, acb->scsi_cmd_length-1);
		}
#endif
		n = aic6250_dataout_pio(sc, sc->sc_cp, sc->sc_cleft, PH_CMD);
		sc->sc_cp += n;
		sc->sc_cleft -= n;
		sc->sc_prevphase = PH_CMD;
		goto loop;

	case PH_DATAOUT:
		if (sc->sc_state != AIC_CONNECTED)
			break;
		AIC_MISC(("dataout dleft=%lu ", (u_long)sc->sc_dleft));
		if (sc->sc_dma_start != NULL &&
		    sc->sc_dleft > AIC_MIN_DMA_LEN) {
			AIC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;
			if ((acb->xs->flags & SCSI_POLL) == 0 &&
			    (*sc->sc_dma_start)
			    (sc, sc->sc_dp, sc->sc_dleft, 0) == 0) {
				sc->sc_prevphase = PH_DATAOUT;
				goto out;
			}
		}
		n = aic6250_dataout_pio(sc, sc->sc_dp, sc->sc_dleft, PH_DATAOUT);
		sc->sc_dp += n;
		sc->sc_dleft -= n;
		sc->sc_prevphase = PH_DATAOUT;
		goto loop;

	case PH_DATAIN:
		if (sc->sc_state != AIC_CONNECTED)
			break;
		AIC_MISC(("datain %lu ", (u_long)sc->sc_dleft));
		if (sc->sc_dma_start != NULL &&
		    sc->sc_dleft > AIC_MIN_DMA_LEN) {
			AIC_ASSERT(sc->sc_nexus != NULL);
			acb = sc->sc_nexus;
			if ((acb->xs->flags & SCSI_POLL) == 0 &&
			    (*sc->sc_dma_start)
			    (sc, sc->sc_dp, sc->sc_dleft, 1) == 0) {
				sc->sc_prevphase = PH_DATAIN;
				goto out;
			}
		}
		n = aic6250_datain_pio(sc, sc->sc_dp, sc->sc_dleft, PH_DATAIN);
		sc->sc_dp += n;
		sc->sc_dleft -= n;
		sc->sc_prevphase = PH_DATAIN;
		goto loop;

	case PH_STAT:
		if (sc->sc_state != AIC_CONNECTED)
			break;
		AIC_ASSERT(sc->sc_nexus != NULL);
		acb = sc->sc_nexus;
		aic6250_datain_pio(sc, &acb->target_stat, 1, PH_STAT);
		AIC_MISC(("target_stat=0x%02x ", acb->target_stat));
		sc->sc_prevphase = PH_STAT;
		goto loop;
	}

	printf("%s: unexpected bus phase; resetting\n", sc->sc_dev.dv_xname);
	AIC_BREAK();
reset:
	aic6250_init(sc);
	return 1;

finish:
	timeout_del(&acb->xs->stimeout);
	aic6250_done(sc, acb);
	goto out;

sched:
	sc->sc_state = AIC_IDLE;
	aic6250_sched(sc);
	goto out;

out:
	sc->sc_imr0 |= AIC_IMR_EN_ERROR_INT;
	(*sc->sc_write)(sc, AIC_INT_MSK_REG0, sc->sc_imr0);
	return 1;
}

void
aic6250_seltimeout(void *arg)
{
	struct aic6250_softc *sc = arg;
	struct aic6250_acb *acb;

	AIC_MISC(("selection timeout  "));

	if (sc->sc_state != AIC_SELECTING) {
		printf("%s: selection timeout while idle; ",
		    sc->sc_dev.dv_xname);
		printf("resetting\n");
		AIC_BREAK();
		aic6250_init(sc);
		return;
	}

	AIC_ASSERT(sc->sc_nexus != NULL);
	acb = sc->sc_nexus;

	(*sc->sc_write)(sc, AIC_SCSI_ID_DATA, 0);
	delay(200);

	acb->xs->error = XS_SELTIMEOUT;
	timeout_del(&acb->xs->stimeout);
	aic6250_done(sc, acb);

	sc->sc_imr0 |= AIC_IMR_EN_ERROR_INT;
	(*sc->sc_write)(sc, AIC_INT_MSK_REG0, sc->sc_imr0);
}

void
aic6250_abort(struct aic6250_softc *sc, struct aic6250_acb *acb)
{

	/* 2 secs for the abort */
	acb->timeout = AIC_ABORT_TIMEOUT;
	acb->flags |= ACB_ABORT;

	if (acb == sc->sc_nexus) {
		/*
		 * If we're still selecting, the message will be scheduled
		 * after selection is complete.
		 */
		if (sc->sc_state == AIC_CONNECTED)
			aic6250_sched_msgout(sc, SEND_ABORT);
	} else {
		aic6250_dequeue(sc, acb);
		TAILQ_INSERT_HEAD(&sc->ready_list, acb, chain);
		if (sc->sc_state == AIC_IDLE)
			aic6250_sched(sc);
	}
}

void
aic6250_timeout(void *arg)
{
	struct aic6250_acb *acb = arg;
	struct scsi_xfer *xs = acb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct aic6250_softc *sc = sc_link->adapter_softc;
	int s;

	sc_print_addr(sc_link);
	printf("timed out");

	s = splbio();

	if (acb->flags & ACB_ABORT) {
		/* abort timed out */
		printf(" AGAIN\n");
		/* XXX Must reset! */
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		acb->xs->error = XS_TIMEOUT;
		aic6250_abort(sc, acb);
	}

	splx(s);
}

#ifdef AIC_DEBUG
/*
 * The following functions are mostly used for debugging purposes, either
 * directly called from the driver or from the kernel debugger.
 */

void
aic6250_show_scsi_cmd(struct aic6250_acb *acb)
{
	uint8_t *b = (uint8_t *)&acb->scsi_cmd;
	struct scsi_link *sc_link = acb->xs->sc_link;
	int i;

	sc_print_addr(sc_link);
	if ((acb->xs->flags & SCSI_RESET) == 0) {
		for (i = 0; i < acb->scsi_cmd_length; i++) {
			if (i)
				printf(",");
			printf("%x", b[i]);
		}
		printf("\n");
	} else
		printf("RESET\n");
}

void
aic6250_print_acb(struct aic6250_acb *acb)
{

	printf("acb@%p xs=%p flags=%x", acb, acb->xs, acb->flags);
	printf(" dp=%p dleft=%d target_stat=%x\n",
	       acb->data_addr, acb->data_length, acb->target_stat);
	aic6250_show_scsi_cmd(acb);
}

void
aic6250_print_active_acb(void)
{
	struct aic6250_acb *acb;
	struct aic6250_softc *sc = oaic_cd.cd_devs[0];

	printf("ready list:\n");
	TAILQ_FOREACH(acb, &sc->ready_list, chain)
		aic6250_print_acb(acb);
	printf("nexus:\n");
	if (sc->sc_nexus != NULL)
		aic6250_print_acb(sc->sc_nexus);
	printf("nexus list:\n");
	TAILQ_FOREACH(acb, &sc->nexus_list, chain)
		aic6250_print_acb(acb);
}

void
aic6250_dump_driver(struct aic6250_softc *sc)
{
	struct aic6250_tinfo *ti;
	int i;

	printf("nexus=%p prevphase=%x\n", sc->sc_nexus, sc->sc_prevphase);
	printf("state=%x msgin=%x ", sc->sc_state, sc->sc_imess[0]);
	printf("msgpriq=%x msgoutq=%x lastmsg=%x currmsg=%x\n", sc->sc_msgpriq,
	    sc->sc_msgoutq, sc->sc_lastmsg, sc->sc_currmsg);
	for (i = 0; i < 7; i++) {
		ti = &sc->sc_tinfo[i];
		printf("tinfo%d: %d cmds %d disconnects %d timeouts",
		    i, ti->cmds, ti->dconns, ti->touts);
		printf(" %d senses flags=%x\n", ti->senses, ti->flags);
	}
}
#endif

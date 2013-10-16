/*	$OpenBSD: oaic.c,v 1.1 2013/10/16 16:59:34 miod Exp $	*/

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

#include <sys/param.h>
#include <stand.h>
#include "libsa.h"

#include "scsi.h"

#include <dev/ic/aic6250reg.h>

/*
 * ACB. Holds additional information for each SCSI command Comments: We
 * need a separate scsi command block because we may need to overwrite it
 * with a request sense command.  Basicly, we refrain from fiddling with
 * the scsi_xfer struct (except do the expected updating of return values).
 * We'll generally update: xs->{flags,resid,error,sense,status} and
 * occasionally xs->retries.
 */
struct aic6250_acb {
	struct scsi_generic *scsi_cmd;
	int scsi_cmd_length;
	void *data_addr;		/* Saved data pointer */
	int data_length;		/* Residue */

	uint8_t target_stat;		/* SCSI status byte */

	int xsflags;
	int error;
};

struct aic6250_softc {
	uint32_t	sc_baseaddr;

	int		sc_tgtid;
	int		sc_tgtlun;

	struct aic6250_acb *sc_nexus;	/* current command */
	struct aic6250_acb sc_acb;

	/* Data about the current nexus (updated for every cmd switch) */
	u_char	*sc_dp;		/* Current data pointer */
	size_t	sc_dleft;	/* Data bytes left to transfer */
	u_char	*sc_cp;		/* Current command pointer */
	size_t	sc_cleft;	/* Command bytes left to transfer */

	/* Adapter state */
	uint8_t	 sc_phase;	/* Current bus phase */
	uint8_t	 sc_prevphase;	/* Previous bus phase */
	uint8_t	 sc_state;	/* State applicable to the adapter */
#define	AIC_INIT	0
#define AIC_IDLE	1
#define AIC_SELECTING	2	/* SCSI command is arbiting  */
#define AIC_RESELECTED	3	/* Has been reselected */
#define AIC_CONNECTED	4	/* Actively using the SCSI bus */
#define	AIC_DISCONNECT	5	/* MSG_DISCONNECT received */
#define	AIC_CMDCOMPLETE	6	/* MSG_CMDCOMPLETE received */
#define AIC_CLEANING	7
	uint8_t	 sc_flags;
#define AIC_DROP_MSGIN	0x01	/* Discard all msgs (parity err detected) */
#define	AIC_ABORTING	0x02	/* Bailing out */
	uint8_t	sc_selid;	/* Reselection ID */

	uint8_t sc_imr0;
	uint8_t sc_imr1;
	uint8_t	sc_cr0;
	uint8_t	sc_cr1;
	uint	sc_selto;	/* Selection timeout (when polling) */

	/* Message stuff */
	uint8_t	sc_msgpriq;	/* Messages we want to send */
	uint8_t	sc_msgoutq;	/* Messages sent during last MESSAGE OUT */
	uint8_t	sc_lastmsg;	/* Message last transmitted */
	uint8_t	sc_currmsg;	/* Message currently ready to transmit */
#define SEND_DEV_RESET		0x01
#define SEND_PARITY_ERROR	0x02
#define SEND_INIT_DET_ERR	0x04
#define SEND_REJECT		0x08
#define SEND_IDENTIFY  		0x10
#define SEND_ABORT		0x20
#define AIC_MAX_MSG_LEN 8
	uint8_t  sc_omess[AIC_MAX_MSG_LEN];
	uint8_t	*sc_omp;		/* Outgoing message pointer */
	uint8_t	sc_imess[AIC_MAX_MSG_LEN];
	uint8_t	*sc_imp;		/* Incoming message pointer */

	/* Hardware stuff */
	int	sc_initiator;		/* Our scsi id */
	int	sc_freq;		/* Clock frequency in MHz */
};

int	aic6250_intr(void *);

void 	aic6250_init(struct aic6250_softc *);
void	aic6250_done(struct aic6250_softc *, struct aic6250_acb *);
void	aic6250_sched_msgout(struct aic6250_softc *, uint8_t);
void	aic6250_select(struct aic6250_softc *, struct aic6250_acb *);
void	aic6250_seltimeout(void *);
void	aic6250_sched(struct aic6250_softc *);
void	aic6250_scsi_reset(struct aic6250_softc *);
void	aic6250_reset(struct aic6250_softc *);
void	aic6250_acb_free(void *, void *);
void	*aic6250_acb_alloc(void *);
int	aic6250_reselect(struct aic6250_softc *, int);
void	aic6250_msgin(struct aic6250_softc *);
void	aic6250_msgout(struct aic6250_softc *);
void	aic6250_ack(struct aic6250_softc *);
int	aic6250_dataout_pio(struct aic6250_softc *, uint8_t *, int, int);
int	aic6250_datain_pio(struct aic6250_softc *, uint8_t *, int, int);

#define	oaic_read(sc, addr) \
	(*(volatile uint32_t *)((sc)->sc_baseaddr + ((addr) << 2)) & 0xff)
#define	oaic_write(sc, addr, val) \
	*(volatile uint32_t *)((sc)->sc_baseaddr + ((addr) << 2)) = (val)

/*
 * Attach the AIC6250, fill out some high and low level data structures
 */
void *
oaic_attach(uint32_t addr, int id, int lun)
{
	struct aic6250_softc *sc;

	sc = (struct aic6250_softc *)alloc(sizeof *sc);
	if (sc == NULL)
		return NULL;

	memset(sc, 0, sizeof *sc);
	sc->sc_baseaddr = addr;
	/* XXX */
	sc->sc_freq = 10;
	sc->sc_initiator = 7;
	sc->sc_cr0 = AIC_CR0_EN_PORT_A;
	sc->sc_cr1 = AIC_CR1_ENABLE_16BIT_MEM_BUS;
	/* XXX */

	sc->sc_state = AIC_INIT;

	if (sc->sc_freq >= 20)
		sc->sc_cr1 |= AIC_CR1_CLK_FREQ_MODE;

	aic6250_init(sc);	/* init chip and driver */

	sc->sc_tgtid = id;
	sc->sc_tgtlun = lun;

	return sc;
}

void
oaic_detach(void *cookie)
{
	free(cookie, sizeof(struct aic6250_softc));
}

/*
 * Initialize AIC6250 chip itself.
 */
void
aic6250_reset(struct aic6250_softc *sc)
{
	/* reset chip */
	oaic_write(sc, AIC_CONTROL_REG1, AIC_CR1_CHIP_SW_RESET);
	delay(200);
	oaic_write(sc, AIC_CONTROL_REG1, 0);

	oaic_write(sc, AIC_CONTROL_REG1, sc->sc_cr1);
	oaic_write(sc, AIC_CONTROL_REG0, sc->sc_cr0 | sc->sc_initiator);
	/* asynchronous operation */
	oaic_write(sc, AIC_OFFSET_CNTRL, 0);

	sc->sc_imr0 = sc->sc_imr1 = 0;
	oaic_write(sc, AIC_INT_MSK_REG0, sc->sc_imr0);
	oaic_write(sc, AIC_INT_MSK_REG1, sc->sc_imr1);

	oaic_write(sc, AIC_DMA_BYTE_COUNT_L, 0);
	oaic_write(sc, AIC_DMA_BYTE_COUNT_M, 0);
	oaic_write(sc, AIC_DMA_BYTE_COUNT_H, 0);
	oaic_write(sc, AIC_DMA_CNTRL, 0);
	oaic_write(sc, AIC_PORT_A, 0);
	oaic_write(sc, AIC_PORT_B, 0);
}

/* Pull the SCSI RST line for 500 us */
void
aic6250_scsi_reset(struct aic6250_softc *sc)
{
	/* reset SCSI bus */
	oaic_write(sc, AIC_CONTROL_REG1,
	    sc->sc_cr1 | AIC_CR1_SCSI_RST_OUT);
	delay(500);
	oaic_write(sc, AIC_CONTROL_REG1, sc->sc_cr1);
	delay(50);
}

/*
 * Initialize aic SCSI driver.
 */
void
aic6250_init(struct aic6250_softc *sc)
{
	struct aic6250_acb *acb;

	aic6250_reset(sc);
	aic6250_scsi_reset(sc);
	aic6250_reset(sc);

	if (sc->sc_state == AIC_INIT) {
		/* First time through; initialize. */
		sc->sc_nexus = NULL;
	} else {
		/* Cancel any active commands. */
		sc->sc_state = AIC_CLEANING;
		if ((acb = sc->sc_nexus) != NULL) {
			acb->error = XS_DRIVER_STUFFUP;
			aic6250_done(sc, acb);
		}
	}

	sc->sc_prevphase = PH_INVALID;
	sc->sc_state = AIC_IDLE;
	sc->sc_imr0 = AIC_IMR_EN_ERROR_INT;
	sc->sc_imr1 = AIC_IMR1_EN_SCSI_RST_INT;
	oaic_write(sc, AIC_INT_MSK_REG0, sc->sc_imr0);
	oaic_write(sc, AIC_INT_MSK_REG1, sc->sc_imr1);
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
int
oaic_scsicmd(void *cookie, void *cmdbuf, size_t cmdlen, void *databuf,
    size_t datalen, size_t *resid)
{
	struct aic6250_softc *sc = cookie;
	struct aic6250_acb *acb = &sc->sc_acb;

	if (resid != NULL)
		*resid = 0;

	acb->xsflags = datalen != 0 ? SCSI_DATA_IN : 0;	/* XXX */
	acb->scsi_cmd = cmdbuf;
	acb->scsi_cmd_length = cmdlen;
	acb->data_addr = databuf;
	acb->data_length = datalen;
	acb->target_stat = 0;

	aic6250_sched(sc);

	for (;;) {
		uint8_t sr0, sr1, sr0mask, sr1mask;

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

		sr0 = oaic_read(sc, AIC_STATUS_REG0);
		sr1 = oaic_read(sc, AIC_STATUS_REG1);

		if ((sr0 & sr0mask) != 0 || (sr1 & sr1mask) != 0) {
			aic6250_intr(sc);
		}
		if ((acb->xsflags & ITSDONE) != 0)
			break;
		delay(1000);

		/* process the selection timeout timer as well if necessary */
		if (sc->sc_selto != 0) {
			sc->sc_selto--;
			if (sc->sc_selto == 0) {
				aic6250_seltimeout(sc);
			}
		}
	}

	if (resid != NULL && acb->error == 0)
		*resid = datalen;

	return acb->error;
}

/*
 * LOW LEVEL SCSI UTILITIES
 */

void
aic6250_ack(struct aic6250_softc *sc)
{
	oaic_write(sc, AIC_SCSI_SIGNAL_REG,
	    oaic_read(sc, AIC_SCSI_SIGNAL_REG) | AIC_SS_ACK_OUT);
	while ((oaic_read(sc, AIC_SCSI_SIGNAL_REG) & AIC_SS_REQ_IN) != 0)
		continue;
	oaic_write(sc, AIC_SCSI_SIGNAL_REG,
	    oaic_read(sc, AIC_SCSI_SIGNAL_REG) & ~AIC_SS_ACK_OUT);
}

void
aic6250_sched_msgout(struct aic6250_softc *sc, uint8_t m)
{
	if (sc->sc_msgpriq == 0)
		oaic_write(sc, AIC_SCSI_SIGNAL_REG,
		    sc->sc_phase | AIC_SS_ATN_OUT);
	sc->sc_msgpriq |= m;
}

/*
 * Start a selection.  This is used by aic6250_sched() to select an idle target,
 * and by aic6250_done() to immediately reselect a target to get sense
 * information.
 */
void
aic6250_select(struct aic6250_softc *sc, struct aic6250_acb *acb)
{
	oaic_write(sc, AIC_SCSI_ID_DATA,
	    (1 << sc->sc_initiator) | (1 << sc->sc_tgtid));

	/* Always enable reselections. */
	sc->sc_imr1 |= AIC_IMR1_EN_SCSI_RST_INT;
	sc->sc_imr1 &=
	    ~(AIC_IMR1_EN_SCSI_REQ_ON_INT | AIC_IMR1_EN_SCSI_PARITY_ERR_INT |
	      AIC_IMR1_EN_BUS_FREE_DETECT_INT | AIC_IMR1_EN_PHASE_CHANGE_INT);
	oaic_write(sc, AIC_INT_MSK_REG1, sc->sc_imr1);
	sc->sc_imr0 = AIC_IMR_ARB_SEL_START | AIC_IMR_EN_ERROR_INT |
	    AIC_IMR_EN_CMD_DONE_INT | AIC_IMR_EN_SEL_OUT_INT |
	    AIC_IMR_EN_RESEL_INT | AIC_IMR_EN_SELECT_INT;
	oaic_write(sc, AIC_INT_MSK_REG0, sc->sc_imr0);

	sc->sc_state = AIC_SELECTING;
}

int
aic6250_reselect(struct aic6250_softc *sc, int message)
{
	uint8_t selid;
	struct aic6250_acb *acb;

	/*
	 * The SCSI chip made a snapshot of the data bus while the reselection
	 * was being negotiated.  This enables us to determine which target did
	 * the reselect.
	 */
	selid = sc->sc_selid & ~(1 << sc->sc_initiator);
	if (selid & (selid - 1)) {
		printf("insc: reselect with invalid selid %02x\n", selid);
		goto reset;
	}

	acb = sc->sc_nexus;
	if (acb == NULL) {
		printf("insc: unexpected reselect\n");
		goto abort;
	}

	/* Make this nexus active again. */
	sc->sc_state = AIC_CONNECTED;

	/* Do an implicit RESTORE POINTERS. */
	sc->sc_dp = acb->data_addr;
	sc->sc_dleft = acb->data_length;
	sc->sc_cp = (uint8_t *)acb->scsi_cmd;
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

	acb = &sc->sc_acb;
	sc->sc_nexus = acb;
	aic6250_select(sc, acb);
}

/*
 * POST PROCESSING OF SCSI_CMD (usually current)
 */
void
aic6250_done(struct aic6250_softc *sc, struct aic6250_acb *acb)
{
	switch (acb->target_stat) {
	case SCSI_OK:
		acb->error = XS_NOERROR;
		break;
	case SCSI_BUSY:
		acb->error = XS_BUSY;
		break;
	case SCSI_CHECK:
		acb->error = XS_DRIVER_STUFFUP;
		break;
	default:
		acb->error = XS_RESET;
		break;
	}

	acb->xsflags |= ITSDONE;
	sc->sc_nexus = NULL;
	sc->sc_state = AIC_IDLE;

	/* Nothing to start; just enable reselections. */
	sc->sc_imr1 |= AIC_IMR1_EN_SCSI_RST_INT;
	sc->sc_imr1 &=
	    ~(AIC_IMR1_EN_SCSI_REQ_ON_INT | AIC_IMR1_EN_SCSI_PARITY_ERR_INT |
	    AIC_IMR1_EN_BUS_FREE_DETECT_INT | AIC_IMR1_EN_PHASE_CHANGE_INT);
	oaic_write(sc, AIC_INT_MSK_REG1, sc->sc_imr1);
	sc->sc_imr0 = AIC_IMR_EN_ERROR_INT |
	    AIC_IMR_EN_RESEL_INT | AIC_IMR_EN_SELECT_INT;
	oaic_write(sc, AIC_INT_MSK_REG0, sc->sc_imr0);
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
			scsisig = oaic_read(sc, AIC_SCSI_SIGNAL_REG);
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
		sr0 = oaic_read(sc, AIC_STATUS_REG0);
		if ((sr0 & AIC_SR0_SCSI_PARITY_ERR) != 0) {
			sc->sc_flags |= AIC_DROP_MSGIN;
			aic6250_sched_msgout(sc, SEND_PARITY_ERROR);
		}

		/* Gather incoming message bytes if needed. */
		msgbyte = oaic_read(sc, AIC_SCSI_ID_DATA);
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

	/* We now have a complete message.  Parse it. */
	switch (sc->sc_state) {
		struct aic6250_acb *acb;

	case AIC_CONNECTED:
		acb = sc->sc_nexus;

		switch (sc->sc_imess[0]) {
		case MSG_CMDCOMPLETE:
			sc->sc_state = AIC_CMDCOMPLETE;
			break;

		case MSG_PARITY_ERROR:
			/* Resend the last message. */
			aic6250_sched_msgout(sc, sc->sc_lastmsg);
			break;

		case MSG_MESSAGE_REJECT:
			switch (sc->sc_lastmsg) {
			case SEND_INIT_DET_ERR:
				aic6250_sched_msgout(sc, SEND_ABORT);
				break;
			}
			break;

		case MSG_NOOP:
			break;

		case MSG_DISCONNECT:
			sc->sc_state = AIC_DISCONNECT;
			break;

		case MSG_SAVEDATAPOINTER:
			acb->data_addr = sc->sc_dp;
			acb->data_length = sc->sc_dleft;
			break;

		case MSG_RESTOREPOINTERS:
			sc->sc_dp = acb->data_addr;
			sc->sc_dleft = acb->data_length;
			sc->sc_cp = (uint8_t *)acb->scsi_cmd;
			sc->sc_cleft = acb->scsi_cmd_length;
			break;

		default:
			aic6250_sched_msgout(sc, SEND_REJECT);
			break;
		}
		break;

	case AIC_RESELECTED:
		if (!MSG_ISIDENTIFY(sc->sc_imess[0])) {
			printf("insc: reselect without IDENTIFY\n");
			goto reset;
		}

		(void) aic6250_reselect(sc, sc->sc_imess[0]);
		break;

	default:
		printf("insc: unexpected MESSAGE IN\n");
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
	/*
	 * We need to explicitely un-busy.
	 */
	oaic_write(sc, AIC_SCSI_SIGNAL_REG,
	    oaic_read(sc, AIC_SCSI_SIGNAL_REG) &
	    ~(AIC_SS_SEL_OUT | AIC_SS_BSY_OUT | AIC_SS_ACK_OUT));
}

/*
 * Send the highest priority, scheduled message.
 */
void
aic6250_msgout(struct aic6250_softc *sc)
{
	uint8_t scsisig;
	int n;

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
			sc->sc_msgpriq |= sc->sc_msgoutq;
			/*
			 * Set ATN.  If we're just sending a trivial 1-byte
			 * message, we'll clear ATN later on anyway.
			 */
			oaic_write(sc, AIC_SCSI_SIGNAL_REG,
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
		sc->sc_omess[0] =
		    MSG_IDENTIFY(sc->sc_tgtlun, 1);
		n = 1;
		break;

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
		printf("insc: unexpected MESSAGE OUT\n");
		sc->sc_omess[0] = MSG_NOOP;
		n = 1;
		break;
	}
	sc->sc_omp = &sc->sc_omess[n];

nextbyte:
	/* Send message bytes. */
	for (;;) {
		for (;;) {
			scsisig = oaic_read(sc, AIC_SCSI_SIGNAL_REG);
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
					oaic_write(sc, AIC_SCSI_SIGNAL_REG,
					    scsisig & ~AIC_SS_ATN_OUT);
				return;
			}
			if ((scsisig & AIC_SS_REQ_IN) != 0)
				break;
		}

		/* Clear ATN before last byte if this is the last message. */
		if (n == 1 && sc->sc_msgpriq == 0)
			oaic_write(sc, AIC_SCSI_SIGNAL_REG,
			    scsisig & ~AIC_SS_ATN_OUT);
		/* Send message byte. */
		oaic_write(sc, AIC_SCSI_ID_DATA, *--sc->sc_omp);
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
	oaic_write(sc, AIC_INT_MSK_REG1, sc->sc_imr1);

	/* I have tried to make the main loop as tight as possible.  This
	 * means that some of the code following the loop is a bit more
	 * complex than otherwise.
	 */
	while (out != n) {
		for (;;) {
			scsisig = oaic_read(sc, AIC_SCSI_SIGNAL_REG);
			if ((scsisig & AIC_SS_REQ_IN) != 0)
				break;
		}
		if ((scsisig & PH_MASK) != phase)
			break;

		oaic_write(sc, AIC_SCSI_ID_DATA, *p++);
		out++;

		aic6250_ack(sc);
	}

	sc->sc_imr1 |= AIC_IMR1_EN_SCSI_REQ_ON_INT;
	oaic_write(sc, AIC_INT_MSK_REG1, sc->sc_imr1);

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
	oaic_write(sc, AIC_INT_MSK_REG1, sc->sc_imr1);

	/* We leave this loop if one or more of the following is true:
	 * a) phase != PH_DATAIN && FIFOs are empty
	 * b) SCSIRSTI is set (a reset has occurred) or busfree is detected.
	 */
	while (in != n) {
		for (;;) {
			scsisig = oaic_read(sc, AIC_SCSI_SIGNAL_REG);
			if ((scsisig & AIC_SS_REQ_IN) != 0)
				break;
		}
		if ((scsisig & PH_MASK) != phase)
			break;

		*p++ = oaic_read(sc, AIC_SCSI_ID_DATA);
		in++;

		aic6250_ack(sc);
	}

	sc->sc_imr1 |= AIC_IMR1_EN_SCSI_REQ_ON_INT;
	oaic_write(sc, AIC_INT_MSK_REG1, sc->sc_imr1);

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
	int n, first = 1;

	/* Read SR1 before writing to IMR0 (which will reset some SR1 bits). */
	sr1 = oaic_read(sc, AIC_STATUS_REG1);
	oaic_write(sc, AIC_INT_MSK_REG0, 0);

loop:
	sr0 = oaic_read(sc, AIC_STATUS_REG0);
	/*
	 * First check for abnormal conditions, such as reset.
	 */
	if ((sr0 & AIC_SR0_SCSI_RST_OCCURED) != 0) {
		printf("insc: SCSI bus reset\n");
		while ((oaic_read(sc, AIC_STATUS_REG1) &
		    AIC_SR1_SCSI_RST_IN) != 0)
			delay(5);
		goto reset;
	}

	/*
	 * Check for less serious errors.
	 */
	if ((sr0 & AIC_SR0_SCSI_PARITY_ERR) != 0) {
		printf("insc: SCSI bus parity error\n");
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
			sr1 = oaic_read(sc, AIC_STATUS_REG1);

		if (sc->sc_state == AIC_SELECTING &&
		   (sr1 & AIC_SR1_SEL_OUT) != 0) {
			/* start selection timeout */
			acb = sc->sc_nexus;
			sc->sc_selto = 250;	/* msec */
			sc->sc_imr0 &= ~AIC_IMR_EN_SEL_OUT_INT;
			goto out;
		}

		if ((sr1 & AIC_SR1_RESELECTED) != 0) {
			/* kill selection timeout timer */
			sc->sc_imr0 &=
			    ~(AIC_IMR_EN_SEL_OUT_INT | AIC_IMR_EN_CMD_DONE_INT);
			sc->sc_selto = 0;

			/* Save reselection ID. */
			sc->sc_selid = oaic_read(sc, AIC_SOURCE_DEST_ID);

			sc->sc_state = AIC_RESELECTED;
		} else if ((sr1 & (AIC_SR1_SELECTED | AIC_SR1_CMD_DONE)) != 0) {
			/* kill selection timeout timer */
			sc->sc_imr0 &=
			    ~(AIC_IMR_EN_SEL_OUT_INT | AIC_IMR_EN_CMD_DONE_INT);
			sc->sc_selto = 0;

			/* We have selected a target. Things to do:
			 * a) Determine what message(s) to send.
			 * b) Verify that we're still selecting the target.
			 * c) Mark device as busy.
			 */
			if (sc->sc_state != AIC_SELECTING) {
				printf("insc: selection out while idle\n");
				goto reset;
			}
			acb = sc->sc_nexus;

			sc->sc_msgpriq = SEND_IDENTIFY;

			/* Do an implicit RESTORE POINTERS. */
			sc->sc_dp = acb->data_addr;
			sc->sc_dleft = acb->data_length;
			sc->sc_cp = (uint8_t *)acb->scsi_cmd;
			sc->sc_cleft = acb->scsi_cmd_length;

			sc->sc_state = AIC_CONNECTED;
		} else {
			if (sc->sc_state != AIC_IDLE) {
				printf("insc: BUS FREE while not idle\n");
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
		oaic_write(sc, AIC_INT_MSK_REG1, sc->sc_imr1);

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
			acb = sc->sc_nexus;

			if ((sc->sc_flags & AIC_ABORTING) == 0) {
				/*
				 * Section 5.1.1 of the SCSI 2 spec suggests
				 * issuing a REQUEST SENSE following an
				 * unexpected disconnect.  Some devices go into
				 * a contingent allegiance condition when
				 * disconnecting, and this is necessary to
				 * clean up their state.
				 */
				printf("insc: unexpected disconnect\n");
				goto out;
			}

			acb->error = XS_DRIVER_STUFFUP;
			goto finish;

		case AIC_DISCONNECT:
			acb = sc->sc_nexus;
#if 1 /* XXX */
			acb->data_addr = sc->sc_dp;
			acb->data_length = sc->sc_dleft;
#endif
			sc->sc_nexus = NULL;
			goto sched;

		case AIC_CMDCOMPLETE:
			acb = sc->sc_nexus;
			goto finish;
		}
	}

dophase:
	if ((sr0 & AIC_SR0_SCSI_REQ_ON) == 0) {
		/* Wait for AIC_SR0_SCSI_REQ_ON. */
		goto out;
	}

	sc->sc_phase = oaic_read(sc, AIC_SCSI_SIGNAL_REG) & PH_MASK;
	oaic_write(sc, AIC_SCSI_SIGNAL_REG, sc->sc_phase);

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
		n = aic6250_dataout_pio(sc, sc->sc_cp, sc->sc_cleft, PH_CMD);
		sc->sc_cp += n;
		sc->sc_cleft -= n;
		sc->sc_prevphase = PH_CMD;
		goto loop;

	case PH_DATAOUT:
		if (sc->sc_state != AIC_CONNECTED)
			break;
		n = aic6250_dataout_pio(sc, sc->sc_dp, sc->sc_dleft, PH_DATAOUT);
		sc->sc_dp += n;
		sc->sc_dleft -= n;
		sc->sc_prevphase = PH_DATAOUT;
		goto loop;

	case PH_DATAIN:
		if (sc->sc_state != AIC_CONNECTED)
			break;
		n = aic6250_datain_pio(sc, sc->sc_dp, sc->sc_dleft, PH_DATAIN);
		sc->sc_dp += n;
		sc->sc_dleft -= n;
		sc->sc_prevphase = PH_DATAIN;
		goto loop;

	case PH_STAT:
		if (sc->sc_state != AIC_CONNECTED)
			break;
		acb = sc->sc_nexus;
		aic6250_datain_pio(sc, &acb->target_stat, 1, PH_STAT);
		sc->sc_prevphase = PH_STAT;
		goto loop;
	}

	printf("insc: unexpected bus phase\n");
reset:
	aic6250_init(sc);
	return 1;

finish:
	aic6250_done(sc, acb);
	goto out;

sched:
	sc->sc_state = AIC_IDLE;

out:
	sc->sc_imr0 |= AIC_IMR_EN_ERROR_INT;
	oaic_write(sc, AIC_INT_MSK_REG0, sc->sc_imr0);
	return 1;
}

void
aic6250_seltimeout(void *arg)
{
	struct aic6250_softc *sc = arg;
	struct aic6250_acb *acb;

	if (sc->sc_state != AIC_SELECTING) {
		printf("insc: selection timeout while idle\n");
		aic6250_init(sc);
		return;
	}

	acb = sc->sc_nexus;

	oaic_write(sc, AIC_SCSI_ID_DATA, 0);
	delay(200);

	acb->error = XS_SELTIMEOUT;
	aic6250_done(sc, acb);

	sc->sc_imr0 |= AIC_IMR_EN_ERROR_INT;
	oaic_write(sc, AIC_INT_MSK_REG0, sc->sc_imr0);
}

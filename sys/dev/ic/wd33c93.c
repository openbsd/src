/*	$OpenBSD: wd33c93.c,v 1.6 2014/06/27 17:51:08 miod Exp $	*/
/*	$NetBSD: wd33c93.c,v 1.24 2010/11/13 13:52:02 uebayasi Exp $	*/

/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *  @(#)scsi.c  7.5 (Berkeley) 5/4/91
 */

/*
 * Changes Copyright (c) 2001 Wayne Knowles
 * Changes Copyright (c) 1996 Steve Woodford
 * Original Copyright (c) 1994 Christian E. Hopps
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
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
 *  This product includes software developed by the University of
 *  California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *  @(#)scsi.c  7.5 (Berkeley) 5/4/91
 */

/*
 * This version of the driver is pretty well generic, so should work with
 * any flavour of WD33C93 chip.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h> /* For hz */
#include <sys/malloc.h>
#include <sys/pool.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_message.h>
#include <scsi/scsi_disk.h>

#include <machine/bus.h>

#include <dev/ic/wd33c93reg.h>
#include <dev/ic/wd33c93var.h>

/*
 * SCSI delays
 * In u-seconds, primarily for state changes on the SPC.
 */
#define SBIC_CMD_WAIT	200000	/* wait per step of 'immediate' cmds */
#define SBIC_DATA_WAIT	200000	/* wait per data in/out step */

#define STATUS_UNKNOWN	0xff	/* uninitialized status */

/*
 * Convenience macro for waiting for a particular wd33c93 event
 */
#define SBIC_WAIT(regs, until, timeo) wd33c93_wait(regs, until, timeo, __LINE__)

void	wd33c93_init(struct wd33c93_softc *);
void	wd33c93_reset(struct wd33c93_softc *);
int	wd33c93_loop(struct wd33c93_softc *, u_char, u_char);
int	wd33c93_go(struct wd33c93_softc *, struct wd33c93_acb *);
int	wd33c93_dmaok(struct wd33c93_softc *, struct scsi_xfer *);
int	wd33c93_wait(struct wd33c93_softc *, u_char, int , int);
u_char	wd33c93_selectbus(struct wd33c93_softc *, struct wd33c93_acb *);
ssize_t	wd33c93_xfout(struct wd33c93_softc *, ssize_t, void *);
ssize_t	wd33c93_xfin(struct wd33c93_softc *, ssize_t, void *);
int	wd33c93_poll(struct wd33c93_softc *, struct wd33c93_acb *);
int	wd33c93_nextstate(struct wd33c93_softc *, struct wd33c93_acb *,
	    u_char, u_char);
int	wd33c93_abort(struct wd33c93_softc *, struct wd33c93_acb *,
	    const char *);
void	wd33c93_xferdone(struct wd33c93_softc *);
void	wd33c93_error(struct wd33c93_softc *, struct wd33c93_acb *);
void	wd33c93_scsidone(struct wd33c93_softc *, struct wd33c93_acb *, int);
void	wd33c93_sched(struct wd33c93_softc *);
void	wd33c93_dequeue(struct wd33c93_softc *, struct wd33c93_acb *);
void	wd33c93_dma_stop(struct wd33c93_softc *);
void	wd33c93_dma_setup(struct wd33c93_softc *, int);
int	wd33c93_msgin_phase(struct wd33c93_softc *, int);
void	wd33c93_msgin(struct wd33c93_softc *, u_char *, int);
void	wd33c93_reselect(struct wd33c93_softc *, int, int, int, int);
void	wd33c93_sched_msgout(struct wd33c93_softc *, u_short);
void	wd33c93_msgout(struct wd33c93_softc *);
void	wd33c93_timeout(void *arg);
void	wd33c93_watchdog(void *arg);
u_char	wd33c93_stp2syn(struct wd33c93_softc *, struct wd33c93_tinfo *);
void	wd33c93_setsync(struct wd33c93_softc *, struct wd33c93_tinfo *);

struct pool wd33c93_pool;		/* Adapter Control Blocks */
struct scsi_iopool wd33c93_iopool;
int wd33c93_pool_initialized = 0;

void *	wd33c93_io_get(void *);
void	wd33c93_io_put(void *, void *);

/*
 * Timeouts
 */
int	wd33c93_cmd_wait	= SBIC_CMD_WAIT;
int	wd33c93_data_wait	= SBIC_DATA_WAIT;

int	wd33c93_nodma		= 0;	/* Use polled IO transfers */
int	wd33c93_nodisc		= 0;	/* Allow command queues */
int	wd33c93_notags		= 0;	/* No Tags */

/*
 * Some useful stuff for debugging purposes
 */
#ifdef SBICDEBUG

#define QPRINTF(a)	SBIC_DEBUG(MISC, a)

int	wd33c93_debug	= 0;		/* Debug flags */

void	wd33c93_print_csr (u_char);
void	wd33c93_hexdump (u_char *, int);

#else
#define QPRINTF(a)  /* */
#endif

static const char *wd33c93_chip_names[] = SBIC_CHIP_LIST;

/*
 * Attach instance of driver and probe for sub devices
 */
void
wd33c93_attach(struct wd33c93_softc *sc, struct scsi_adapter *adapter)
{
	struct scsibus_attach_args saa;

	sc->sc_cfflags = sc->sc_dev.dv_cfdata->cf_flags;
	timeout_set(&sc->sc_watchdog, wd33c93_watchdog, sc);
	wd33c93_init(sc);
	
	printf(": %s, %d.%d MHz, %s\n",
	    wd33c93_chip_names[sc->sc_chip],
	    sc->sc_clkfreq / 10, sc->sc_clkfreq % 10,
	    (sc->sc_dmamode == SBIC_CTL_DMA) ? "DMA" :
	    (sc->sc_dmamode == SBIC_CTL_DBA_DMA) ? "DBA" :
	    (sc->sc_dmamode == SBIC_CTL_BURST_DMA) ? "burst DMA" : "PIO");
	if (sc->sc_chip == SBIC_CHIP_WD33C93B) {
		printf("%s: microcode revision 0x%02x",
		    sc->sc_dev.dv_xname, sc->sc_rev);
		if (sc->sc_minsyncperiod < 50)
			printf(", fast SCSI");
		printf("\n");
	}

	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = sc->sc_id;
	sc->sc_link.adapter_buswidth = SBIC_NTARG;
	sc->sc_link.adapter = adapter;
	sc->sc_link.openings = 2;
	sc->sc_link.luns = SBIC_NLUN;
	sc->sc_link.pool = &wd33c93_iopool;

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;

	config_found(&sc->sc_dev, &saa, scsiprint);
	timeout_add_sec(&sc->sc_watchdog, 60);
}

/*
 * Initialize driver-private structures
 */
void
wd33c93_init(struct wd33c93_softc *sc)
{
	u_int i;

	timeout_del(&sc->sc_watchdog);

	if (!wd33c93_pool_initialized) {
		/* All instances share the same pool */
		pool_init(&wd33c93_pool, sizeof(struct wd33c93_acb), 0, 0, 0,
		    "wd33c93_acb", NULL);
		pool_setipl(&wd33c93_pool, IPL_BIO);
		scsi_iopool_init(&wd33c93_iopool, NULL,
		    wd33c93_io_get, wd33c93_io_put);
		++wd33c93_pool_initialized;
	}

	if (sc->sc_state == 0) {
		TAILQ_INIT(&sc->ready_list);

		sc->sc_nexus = NULL;
		sc->sc_disc  = 0;
		memset(sc->sc_tinfo, 0, sizeof(sc->sc_tinfo));
	} else {
		/* XXX cancel all active commands */
		panic("wd33c93: reinitializing driver!");
	}

	sc->sc_flags = 0;
	sc->sc_state = SBIC_IDLE;
	wd33c93_reset(sc);

	for (i = 0; i < SBIC_NTARG; i++) {
		struct wd33c93_tinfo *ti = &sc->sc_tinfo[i];
		/*
		 * cf_flags = 0xTTSSRR
		 *
		 *   TT = Bitmask to disable Tagged Queues
		 *   SS = Bitmask to disable Sync negotiation
		 *   RR = Bitmask to disable disconnect/reselect
		 */
		ti->flags = T_NEED_RESET;
		if (CFFLAGS_NOSYNC(sc->sc_cfflags, i))
			ti->flags |= T_NOSYNC;
		if (CFFLAGS_NODISC(sc->sc_cfflags, i) || wd33c93_nodisc)
			ti->flags |= T_NODISC;
		ti->period = sc->sc_minsyncperiod;
		ti->offset = 0;
	}
}

void
wd33c93_reset(struct wd33c93_softc *sc)
{
	u_int	my_id, s, div, i;
	u_char	csr, reg;

	SET_SBIC_cmd(sc, SBIC_CMD_ABORT);
	WAIT_CIP(sc);

	s = splbio();

	if (sc->sc_reset != NULL)
		(*sc->sc_reset)(sc);

	my_id = sc->sc_link.adapter_target & SBIC_ID_MASK;

	/* Enable advanced features and really(!) advanced features */
#if 1
	my_id |= (SBIC_ID_EAF | SBIC_ID_RAF);	/* XXX - MD Layer */
#endif

	SET_SBIC_myid(sc, my_id);

	/* Reset the chip */
	SET_SBIC_cmd(sc, SBIC_CMD_RESET);
	DELAY(25);
	SBIC_WAIT(sc, SBIC_ASR_INT, 0);

	/* Set up various chip parameters */
	SET_SBIC_control(sc, SBIC_CTL_EDI | SBIC_CTL_IDI);

	GET_SBIC_csr(sc, csr);			/* clears interrupt also */
	GET_SBIC_cdb1(sc, sc->sc_rev);		/* valid with RAF on wd33c93b */

	switch (csr) {
	case SBIC_CSR_RESET:
		sc->sc_chip = SBIC_CHIP_WD33C93;
		break;
	case SBIC_CSR_RESET_AM:
		SET_SBIC_queue_tag(sc, 0x55);
		GET_SBIC_queue_tag(sc, reg);
		sc->sc_chip = (reg == 0x55) ?
		    	       SBIC_CHIP_WD33C93B : SBIC_CHIP_WD33C93A;
		SET_SBIC_queue_tag(sc, 0x0);
		break;
	default:
		sc->sc_chip = SBIC_CHIP_UNKNOWN;
	}

	/*
	 * Choose a suitable clock divisor and work out the resulting
	 * sync transfer periods in 4ns units.
	 */
	if (sc->sc_clkfreq < 110) {
		my_id |= SBIC_ID_FS_8_10;
		div = 2;
	} else if (sc->sc_clkfreq < 160) {
		my_id |= SBIC_ID_FS_12_15;
		div = 3;
	} else if (sc->sc_clkfreq < 210) {
		my_id |= SBIC_ID_FS_16_20;
		div = 4;
	} else
		panic("wd33c93: invalid clock speed %d", sc->sc_clkfreq);

	for (i = 0; i < 7; i++)
		sc->sc_syncperiods[i] =
		    (i + 2) * div * 1250 / sc->sc_clkfreq;
	sc->sc_minsyncperiod = sc->sc_syncperiods[0];
	SBIC_DEBUG(SYNC, ("available sync periods: %d %d %d %d %d %d %d\n",
	    sc->sc_syncperiods[0], sc->sc_syncperiods[1],
	    sc->sc_syncperiods[2], sc->sc_syncperiods[3],
	    sc->sc_syncperiods[4], sc->sc_syncperiods[5],
	    sc->sc_syncperiods[6]));

	if (sc->sc_clkfreq >= 160 && sc->sc_chip == SBIC_CHIP_WD33C93B) {
		for (i = 0; i < 3; i++)
			sc->sc_fsyncperiods[i] =
			    (i + 2) * 2 * 1250 / sc->sc_clkfreq;
		SBIC_DEBUG(SYNC, ("available fast sync periods: %d %d %d\n",
		    sc->sc_fsyncperiods[0], sc->sc_fsyncperiods[1],
		    sc->sc_fsyncperiods[2]));
		sc->sc_minsyncperiod = sc->sc_fsyncperiods[0];
	}

	/* Max Sync Offset */
	if (sc->sc_chip == SBIC_CHIP_WD33C93A ||
	    sc->sc_chip == SBIC_CHIP_WD33C93B)
		sc->sc_maxoffset = SBIC_SYN_93AB_MAX_OFFSET;
	else
		sc->sc_maxoffset = SBIC_SYN_93_MAX_OFFSET;

	/*
	 * don't allow Selection (SBIC_RID_ES)
	 * until we can handle target mode!!
	 */
	SET_SBIC_rselid(sc, SBIC_RID_ER);

	/* Asynchronous for now */
	SET_SBIC_syn(sc, 0);

	sc->sc_flags = 0;
	sc->sc_state = SBIC_IDLE;

	splx(s);
}

void
wd33c93_error(struct wd33c93_softc *sc, struct wd33c93_acb *acb)
{
	struct scsi_xfer *xs = acb->xs;

	KASSERT(xs);

	if (xs->flags & SCSI_SILENT)
		return;

	sc_print_addr(xs->sc_link);
	printf("SCSI Error\n");
}

/*
 * Determine an appropriate value for the synchronous transfer register
 * given the period and offset values in *ti.
 */
u_char
wd33c93_stp2syn(struct wd33c93_softc *sc, struct wd33c93_tinfo *ti)
{
	unsigned i;

	/* see if we can handle fast scsi (100-200ns) first */
	if (ti->period < 50 && sc->sc_minsyncperiod < 50) {
		for (i = 0; i < 3; i++)
			if (sc->sc_fsyncperiods[i] >= ti->period)
				return (SBIC_SYN(ti->offset, i + 2, 1));
	}

	for (i = 0; i < 7; i++) {
		if (sc->sc_syncperiods[i] >= ti->period) {
			if (i == 6)
				return (SBIC_SYN(0, 0, 0));
			else
				return (SBIC_SYN(ti->offset, i + 2, 0));
		}
	}

	/* XXX - can't handle it; do async */
	return (SBIC_SYN(0, 0, 0));
}

/*
 * Setup sync mode for given target
 */
void
wd33c93_setsync(struct wd33c93_softc *sc, struct wd33c93_tinfo *ti)
{
	u_char syncreg;

	if (ti->flags & T_SYNCMODE)
		syncreg = wd33c93_stp2syn(sc, ti);
	else
		syncreg = SBIC_SYN(0, 0, 0);

	SBIC_DEBUG(SYNC, ("wd33c93_setsync: sync reg = 0x%02x\n", syncreg));
	SET_SBIC_syn(sc, syncreg);
}

/*
 * Check if current operation can be done using DMA
 *
 * returns 1 if DMA OK, 0 for polled I/O transfer
 */
int
wd33c93_dmaok(struct wd33c93_softc *sc, struct scsi_xfer *xs)
{
	if (wd33c93_nodma || sc->sc_dmamode == SBIC_CTL_NO_DMA ||
	    (xs->flags & SCSI_POLL) || xs->datalen == 0)
		return (0);
	return(1);
}

/*
 * Setup for DMA transfer
 */
void
wd33c93_dma_setup(struct wd33c93_softc *sc, int datain)
{
	struct wd33c93_acb *acb = sc->sc_nexus;
	int s;

	sc->sc_daddr = acb->daddr;
	sc->sc_dleft = acb->dleft;

	s = splbio();
	/* Indicate that we're in DMA mode */
	if (sc->sc_dleft) {
		sc->sc_dmasetup(sc, &sc->sc_daddr, &sc->sc_dleft,
		    datain, &sc->sc_dleft);
	}
	splx(s);
	return;
}


/*
 * Save DMA pointers.  Take into account partial transfer. Shut down DMA.
 */
void
wd33c93_dma_stop(struct wd33c93_softc *sc)
{
	ssize_t count;
	int asr;

	/* Wait until WD chip is idle */
	do {
		GET_SBIC_asr(sc, asr);	/* XXX */
		if (asr & SBIC_ASR_DBR) {
			printf("%s: %s: asr %02x canceled!\n",
			    sc->sc_dev.dv_xname, __func__, asr);
			break;
		}
	} while (asr & (SBIC_ASR_BSY|SBIC_ASR_CIP));

	/* Only need to save pointers if DMA was active */
	if (sc->sc_flags & SBICF_INDMA) {
		int s = splbio();

		/* Shut down DMA and flush FIFO's */
		sc->sc_dmastop(sc);

		/* Fetch the residual count */
		SBIC_TC_GET(sc, count);

		/* Work out how many bytes were actually transferred */
		count = sc->sc_tcnt - count;

		if (sc->sc_dleft < count) {
			if (sc->sc_nexus && sc->sc_nexus->xs)
				sc_print_addr(sc->sc_nexus->xs->sc_link);
			else
				printf("%s: ", sc->sc_dev.dv_xname);
			printf("xfer too large: dleft=%zd resid=%zd\n",
			    sc->sc_dleft, count);
		}

		/* Fixup partial xfers */
		sc->sc_daddr = (char *)sc->sc_daddr + count;
		sc->sc_dleft -= count;
		sc->sc_tcnt   = 0;
		sc->sc_flags &= ~SBICF_INDMA;
		splx(s);
		SBIC_DEBUG(DMA, ("dma_stop\n"));
	}
	/*
	 * Ensure the WD chip is back in polled I/O mode, with nothing to
	 * transfer.
	 */
	SBIC_TC_PUT(sc, 0);
	SET_SBIC_control(sc, SBIC_CTL_EDI | SBIC_CTL_IDI);
}


/*
 * Handle new request from scsi layer
 */
void
wd33c93_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link *sc_link = xs->sc_link;
	struct wd33c93_softc *sc = sc_link->adapter_softc;
	struct wd33c93_acb *acb;
	int flags, s;

	SBIC_DEBUG(MISC, ("wd33c93_scsi_cmd\n"));

	/*
	 * The original 33C93 expects commands not in groups 0, 1 and 5
	 * to be six bytes long.
	 */
	if (sc->sc_chip <= SBIC_CHIP_WD33C93) {
		switch (xs->cmd->opcode >> 5) {
		case 0:
		case 1:
		case 5:
			break;
		default:
			if (xs->cmdlen == 6)
				break;
			memset(&xs->sense, 0, sizeof(xs->sense));
			/* sense data borrowed from gdt(4) */
			xs->sense.error_code = SSD_ERRCODE_VALID |
			    SSD_ERRCODE_CURRENT;
			xs->sense.flags = SKEY_ILLEGAL_REQUEST;
			xs->sense.add_sense_code = 0x20; /* illcmd */
			xs->error = XS_SENSE;
			scsi_done(xs);
			return;
		}
	}

	/*
	 * None of the 33C93 are documented to support the START_STOP
	 * command, and it turns out the chip takes some significant
	 * time to recover when the target powers down (the fewer
	 * targets on the bus, the larger the time needed to recover).
	 */
	if (xs->cmd->opcode == START_STOP &&
	    ((struct scsi_start_stop *)xs->cmd)->how == SSS_STOP) {
		if (xs->timeout < 30000)
			xs->timeout = 30000;
	}

	flags = xs->flags;

	if (sc->sc_nexus && (flags & SCSI_POLL))
		panic("wd33c93_scsicmd: busy");

	acb = xs->io;
	memset(acb, 0, sizeof(*acb));
	acb->flags = ACB_ACTIVE;
	acb->xs    = xs;
	acb->timeout = xs->timeout;
	timeout_set(&acb->to, wd33c93_timeout, acb);

	memcpy(&acb->cmd, xs->cmd, xs->cmdlen);
	acb->clen  = xs->cmdlen;
	acb->daddr = xs->data;
	acb->dleft = xs->datalen;

	if (flags & SCSI_POLL) {
		/*
		 * Complete currently active command(s) before
		 * issuing an immediate command
		 */
		while (sc->sc_nexus)
			wd33c93_poll(sc, sc->sc_nexus);
	}

	s = splbio();
	TAILQ_INSERT_TAIL(&sc->ready_list, acb, chain);
	acb->flags |= ACB_READY;

	/* If nothing is active, try to start it now. */
	if (sc->sc_state == SBIC_IDLE)
		wd33c93_sched(sc);
	splx(s);

	if ((flags & SCSI_POLL) == 0)
		return;

	if (wd33c93_poll(sc, acb)) {
		wd33c93_timeout(acb);
		if (wd33c93_poll(sc, acb)) /* 2nd retry for ABORT */
			wd33c93_timeout(acb);
	}
}

/*
 * attempt to start the next available command
 */
void
wd33c93_sched(struct wd33c93_softc *sc)
{
	struct scsi_link *sc_link;
	struct wd33c93_acb *acb;
	struct wd33c93_tinfo *ti;
	struct wd33c93_linfo *li;
	int lun, tag, flags;
	int s;

	if (sc->sc_state != SBIC_IDLE)
		return;

	KASSERT(sc->sc_nexus == NULL);

	/* Loop through the ready list looking for work to do... */
	TAILQ_FOREACH(acb, &sc->ready_list, chain) {
		sc_link = acb->xs->sc_link;
		lun = sc_link->lun;
		ti = &sc->sc_tinfo[sc_link->target];

		KASSERT(acb->flags & ACB_READY);

		/* Select type of tag for this command */
		if ((ti->flags & T_NODISC) != 0)
			tag = 0;
		else if ((ti->flags & T_TAG) == 0)
			tag = 0;
		else if ((acb->flags & ACB_SENSE) != 0)
			tag = 0;
		else if (acb->xs->flags & SCSI_POLL)
			tag = 0; /* No tags for polled commands */
		else
			tag = MSG_SIMPLE_Q_TAG;

		s = splbio();
		li = TINFO_LUN(ti, lun);
		if (li == NULL) {
			/* Initialize LUN info and add to list. */
			li = malloc(sizeof(*li), M_DEVBUF, M_NOWAIT | M_ZERO);
			if (li == NULL) {
				splx(s);
				continue;
			}
			li->lun = lun;
			if (lun < SBIC_NLUN)
				ti->lun[lun] = li;
		}
		li->last_used = time_second;

		/*
		 * We've found a potential command, but is the target/lun busy?
		 */

		if (tag == 0 && li->untagged == NULL)
			li->untagged = acb; /* Issue untagged */

		if (li->untagged != NULL) {
			tag = 0;
			if ((li->state != L_STATE_BUSY) && li->used == 0) {
				/* Issue this untagged command now */
				acb = li->untagged;
				sc_link = acb->xs->sc_link;
			} else{
				/* Not ready yet */
				splx(s);
				continue;
			}
		}

		acb->tag_type = tag;
		if (tag != 0) {
			int i;

			/* Allocate a tag */
			if (li->used == 255) {
				/* no free tags */
				splx(s);
				continue;
			}
			/* Start from the last used location */
			for (i = li->avail; i < 256; i++) {
				if (li->queued[i] == NULL)
					break;
			}
			/* Couldn't find one, start again from the beginning */
			if (i == 256) {
				for (i = 0; i < 256; i++) {
					if (li->queued[i] == NULL)
						break;
				}
			}
#ifdef DIAGNOSTIC
			if (i == 256)
				panic("%s: tag alloc failure", __func__);
#endif

			/* Save where to start next time. */
			li->avail = i + 1;
			li->used++;
			li->queued[i] = acb;
			acb->tag_id = i;
		}
		splx(s);
		if (li->untagged != NULL && (li->state != L_STATE_BUSY)) {
			li->state = L_STATE_BUSY;
			break;
		}
		if (li->untagged == NULL && tag != 0) {
			break;
		} else {
			sc_print_addr(sc_link);
			printf(" busy\n");
		}
	}

	if (acb == NULL) {
		SBIC_DEBUG(ACBS, ("wd33c93sched: no work\n"));
		return;			/* did not find an available command */
	}

	SBIC_DEBUG(ACBS, ("wd33c93_sched(%d,%d)\n", sc_link->target,
		       sc_link->lun));

	TAILQ_REMOVE(&sc->ready_list, acb, chain);
	acb->flags &= ~ACB_READY;

	flags = acb->xs->flags;
	if (flags & SCSI_RESET)
		wd33c93_reset(sc);

	/* XXX - Implicitly call scsidone on select timeout */
	if (wd33c93_go(sc, acb) != 0 || acb->xs->error == XS_SELTIMEOUT) {
		acb->dleft = sc->sc_dleft;
		wd33c93_scsidone(sc, acb, sc->sc_status);
		return;
	}
}

void
wd33c93_scsidone(struct wd33c93_softc *sc, struct wd33c93_acb *acb, int status)
{
	struct scsi_xfer	*xs = acb->xs;
	struct scsi_link	*sc_link = xs->sc_link;
	struct wd33c93_tinfo	*ti;
	struct wd33c93_linfo	*li;

#ifdef DIAGNOSTIC
	KASSERT(sc->target == sc_link->target);
	KASSERT(sc->lun    == sc_link->lun);
	KASSERT(acb->flags != ACB_FREE);
#endif

	SBIC_DEBUG(ACBS, ("scsidone: (%d,%d)->(%d,%d)%02x\n",
	       sc_link->target, sc_link->lun, sc->target, sc->lun, status));

	timeout_del(&acb->to);

	if (xs->error == XS_NOERROR) {
		xs->status = status & SCSI_STATUS_MASK;
		xs->resid = acb->dleft;

		switch (xs->status) {
		case SCSI_CHECK:
		case SCSI_TERMINATED:
			/* XXX Need to read sense - return busy for now */
			/*FALLTHROUGH*/
		case SCSI_QUEUE_FULL:
		case SCSI_BUSY:
			xs->error = XS_BUSY;
			break;
		}
	}

	ti = &sc->sc_tinfo[sc_link->target];
	li = TINFO_LUN(ti, sc_link->lun);
	ti->cmds++;
	if (xs->error == XS_SELTIMEOUT) {
		/* Selection timeout -- discard this LUN if empty */
		if (li->untagged == NULL && li->used == 0) {
			if (sc_link->lun < SBIC_NLUN)
				ti->lun[sc_link->lun] = NULL;
			free(li, M_DEVBUF);
			li = NULL;
		}
	}

	if (li != NULL)
		wd33c93_dequeue(sc, acb);
	if (sc->sc_nexus == acb) {
		sc->sc_state = SBIC_IDLE;
		sc->sc_nexus = NULL;
		sc->sc_flags = 0;

		if (!TAILQ_EMPTY(&sc->ready_list))
			wd33c93_sched(sc);
	}

	scsi_done(xs);
}

void
wd33c93_dequeue(struct wd33c93_softc *sc, struct wd33c93_acb *acb)
{
	struct wd33c93_tinfo *ti = &sc->sc_tinfo[acb->xs->sc_link->target];
	struct wd33c93_linfo *li;
	int lun = acb->xs->sc_link->lun;

	li = TINFO_LUN(ti, lun);
#ifdef DIAGNOSTIC
	if (li == NULL || li->lun != lun)
		panic("wd33c93_dequeue: lun %d for ecb %p does not exist",
		      lun, acb);
#endif
	if (li->untagged == acb) {
		li->state = L_STATE_IDLE;
		li->untagged = NULL;
	}
	if (acb->tag_type && li->queued[acb->tag_id] != NULL) {
#ifdef DIAGNOSTIC
		if (li->queued[acb->tag_id] != NULL &&
		    (li->queued[acb->tag_id] != acb))
			panic("wd33c93_dequeue: slot %d for lun %d has %p "
			    "instead of acb %p\n", acb->tag_id,
			    lun, li->queued[acb->tag_id], acb);
#endif
		li->queued[acb->tag_id] = NULL;
		li->used--;
	}
}


int
wd33c93_wait(struct wd33c93_softc *sc, u_char until, int timeo, int line)
{
	u_char val;

	if (timeo == 0)
		timeo = 1000000;	/* some large value.. */
	GET_SBIC_asr(sc, val);
	while ((val & until) == 0) {
		if (timeo-- == 0) {
			int csr;
			GET_SBIC_csr(sc, csr);
#ifdef SBICDEBUG
			printf("wd33c93_wait: TIMEO @%d with asr=0x%x csr=0x%x\n",
			    line, val, csr);
#ifdef DDB
			Debugger();
#endif
#endif
			return(val); /* Maybe I should abort */
			break;
		}
		DELAY(1);
		GET_SBIC_asr(sc, val);
	}
	return(val);
}

int
wd33c93_abort(struct wd33c93_softc *sc, struct wd33c93_acb *acb,
     const char *where)
{
	u_char csr, asr;

	GET_SBIC_asr(sc, asr);
	GET_SBIC_csr(sc, csr);

	sc_print_addr(acb->xs->sc_link);
	printf("ABORT in %s: csr=0x%02x, asr=0x%02x\n", where, csr, asr);

	acb->timeout = SBIC_ABORT_TIMEOUT;
	acb->flags |= ACB_ABORT;

	/*
	 * Clean up chip itself
	 */
	if (sc->sc_nexus == acb) {
		/* Reschedule timeout. */
		if ((acb->xs->flags & SCSI_POLL) == 0)
			timeout_add_msec(&acb->to, acb->timeout);

		while (asr & SBIC_ASR_DBR) {
			/*
			 * wd33c93 is jammed w/data. need to clear it
			 * But we don't know what direction it needs to go
			 */
			GET_SBIC_data(sc, asr);
			sc_print_addr(acb->xs->sc_link);
			printf("abort %s: clearing data buffer 0x%02x\n",
			       where, asr);
			GET_SBIC_asr(sc, asr);
			if (asr & SBIC_ASR_DBR) /* Not the read direction */
				SET_SBIC_data(sc, asr);
			GET_SBIC_asr(sc, asr);
		}

		sc_print_addr(acb->xs->sc_link);
		printf("sending ABORT command\n");

		WAIT_CIP(sc);
		SET_SBIC_cmd(sc, SBIC_CMD_ABORT);
		WAIT_CIP(sc);

		GET_SBIC_asr(sc, asr);

		sc_print_addr(acb->xs->sc_link);
		if (asr & (SBIC_ASR_BSY|SBIC_ASR_LCI)) {
			/*
			 * ok, get more drastic..
			 */
			printf("Resetting bus\n");
			wd33c93_reset(sc);
		} else {
			printf("sending DISCONNECT to target\n");
			SET_SBIC_cmd(sc, SBIC_CMD_DISC);
			WAIT_CIP(sc);

			do {
				SBIC_WAIT (sc, SBIC_ASR_INT, 0);
				GET_SBIC_asr(sc, asr);
				GET_SBIC_csr(sc, csr);
				SBIC_DEBUG(MISC, ("csr: 0x%02x, asr: 0x%02x\n",
					       csr, asr));
			} while ((csr != SBIC_CSR_DISC) &&
			    (csr != SBIC_CSR_DISC_1) &&
			    (csr != SBIC_CSR_CMD_INVALID));
		}
		sc->sc_state = SBIC_ERROR;
		sc->sc_flags = 0;
	}
	return SBIC_STATE_ERROR;
}


/*
 * select the bus, return when selected or error.
 *
 * Returns the current CSR following selection and optionally MSG out phase.
 * i.e. the returned CSR *should* indicate CMD phase...
 * If the return value is 0, some error happened.
 */
u_char
wd33c93_selectbus(struct wd33c93_softc *sc, struct wd33c93_acb *acb)
{
	struct scsi_xfer *xs = acb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct wd33c93_tinfo *ti;
	u_char target, lun, asr, csr, id;

	KASSERT(sc->sc_state == SBIC_IDLE);

	target = sc_link->target;
	lun    = sc_link->lun;
	ti     = &sc->sc_tinfo[target];

	sc->sc_state = SBIC_SELECTING;
	sc->target    = target;
	sc->lun       = lun;

	SBIC_DEBUG(PHASE, ("wd33c93_selectbus %d: ", target));

	if ((xs->flags & SCSI_POLL) == 0)
		timeout_add_msec(&acb->to, acb->timeout);

	/*
	 * issue select
	 */
	SBIC_TC_PUT(sc, 0);
	SET_SBIC_selid(sc, target);
	SET_SBIC_timeo(sc, SBIC_TIMEOUT(250, sc->sc_clkfreq));

	GET_SBIC_asr(sc, asr);
	if (asr & (SBIC_ASR_INT|SBIC_ASR_BSY)) {
		/* This means we got ourselves reselected upon */
		SBIC_DEBUG(PHASE, ("WD busy (reselect?) ASR=%02x\n", asr));
		return 0;
	}

	SET_SBIC_cmd(sc, SBIC_CMD_SEL_ATN);
	WAIT_CIP(sc);

	/*
	 * wait for select (merged from separate function may need
	 * cleanup)
	 */
	do {
		asr = SBIC_WAIT(sc, SBIC_ASR_INT | SBIC_ASR_LCI, 0);
		if (asr & SBIC_ASR_LCI) {
			QPRINTF(("late LCI: asr %02x\n", asr));
			return 0;
		}

		/* Clear interrupt */
		GET_SBIC_csr (sc, csr);

		/* Reselected from under our feet? */
		if (csr == SBIC_CSR_RSLT_NI || csr == SBIC_CSR_RSLT_IFY) {
			SBIC_DEBUG(PHASE, ("got reselected, asr %02x\n", asr));
			/*
			 * We need to handle this now so we don't lock up later
			 */
			wd33c93_nextstate(sc, acb, csr, asr);
			return 0;
		}

		/* Whoops! */
		if (csr == SBIC_CSR_SLT || csr == SBIC_CSR_SLT_ATN) {
			panic("wd33c93_selectbus: target issued select!");
			return 0;
		}

	} while (csr != (SBIC_CSR_MIS_2 | MESG_OUT_PHASE) &&
		 csr != (SBIC_CSR_MIS_2 | CMD_PHASE) &&
		 csr != SBIC_CSR_SEL_TIMEO);

	/* Anyone at home? */
	if (csr == SBIC_CSR_SEL_TIMEO) {
		xs->error = XS_SELTIMEOUT;
		SBIC_DEBUG(PHASE, ("-- Selection Timeout\n"));
		return 0;
	}

	SBIC_DEBUG(PHASE, ("Selection Complete\n"));

	/* Assume we're now selected */
	GET_SBIC_selid(sc, id);
	if (id != target) {
		/* Something went wrong - wrong target was select */
		sc_print_addr(sc_link);
		printf("%s: wrong target selected; WANTED %d GOT %d",
		    __func__, target, id);
		return 0;      /* XXX: Need to call nexstate to handle? */
	}

	sc->sc_flags |= SBICF_SELECTED;
	sc->sc_state  = SBIC_CONNECTED;

	/* setup correct sync mode for this target */
	wd33c93_setsync(sc, ti);

	if (ti->flags & T_NODISC && sc->sc_disc == 0)
		SET_SBIC_rselid (sc, 0); /* Not expecting a reselect */
	else
		SET_SBIC_rselid (sc, SBIC_RID_ER);

	/*
	 * We only really need to do anything when the target goes to MSG out
	 * If the device ignored ATN, it's probably old and brain-dead,
	 * but we'll try to support it anyhow.
	 * If it doesn't support message out, it definately doesn't
	 * support synchronous transfers, so no point in even asking...
	 */
	if (csr == (SBIC_CSR_MIS_2 | MESG_OUT_PHASE)) {
		if (ti->flags & T_NEGOTIATE) {
			/* Initiate a SDTR message */
			SBIC_DEBUG(SYNC, ("Sending SDTR to target %d\n", id));
			if (ti->flags & T_WANTSYNC) {
				ti->period = sc->sc_minsyncperiod;
				ti->offset = sc->sc_maxoffset;
			} else {
				ti->period = 0;
				ti->offset = 0;
			}
			/* Send Sync negotiation message */
			sc->sc_omsg[0] = MSG_IDENTIFY(lun, 0); /* No Disc */
			sc->sc_omsg[1] = MSG_EXTENDED;
			sc->sc_omsg[2] = MSG_EXT_SDTR_LEN;
			sc->sc_omsg[3] = MSG_EXT_SDTR;
			if (ti->flags & T_WANTSYNC) {
				sc->sc_omsg[4] = sc->sc_minsyncperiod;
				sc->sc_omsg[5] = sc->sc_maxoffset;
			} else {
				sc->sc_omsg[4] = 0;
				sc->sc_omsg[5] = 0;
			}
			wd33c93_xfout(sc, 6, sc->sc_omsg);
			sc->sc_msgout |= SEND_SDTR; /* may be rejected */
			sc->sc_flags  |= SBICF_SYNCNEGO;
		} else {
			if (sc->sc_nexus->tag_type != 0) {
				/* Use TAGS */
				SBIC_DEBUG(TAGS, ("<select %d:%d TAG=%x>\n",
					       sc->target, sc->lun,
					       sc->sc_nexus->tag_id));
				sc->sc_omsg[0] = MSG_IDENTIFY(lun, 1);
				sc->sc_omsg[1] = sc->sc_nexus->tag_type;
				sc->sc_omsg[2] = sc->sc_nexus->tag_id;
				wd33c93_xfout(sc, 3, sc->sc_omsg);
				sc->sc_msgout |= SEND_TAG;
			} else {
				int no_disc;

				/* Setup LUN nexus and disconnect privilege */
				no_disc = xs->flags & SCSI_POLL ||
					  ti->flags & T_NODISC;
				SEND_BYTE(sc, MSG_IDENTIFY(lun, !no_disc));
			}
		}
		/*
		 * There's one interrupt still to come:
		 * the change to CMD phase...
		 */
		SBIC_WAIT(sc, SBIC_ASR_INT , 0);
		GET_SBIC_csr(sc, csr);
	}

	return csr;
}

/*
 * Information Transfer *to* a SCSI Target.
 *
 * Note: Don't expect there to be an interrupt immediately after all
 * the data is transferred out. The WD spec sheet says that the Transfer-
 * Info command for non-MSG_IN phases only completes when the target
 * next asserts 'REQ'. That is, when the SCSI bus changes to a new state.
 *
 * This can have a nasty effect on commands which take a relatively long
 * time to complete, for example a START/STOP unit command may remain in
 * CMD phase until the disk has spun up. Only then will the target change
 * to STATUS phase. This is really only a problem for immediate commands
 * since we don't allow disconnection for them (yet).
 */
ssize_t
wd33c93_xfout(struct wd33c93_softc *sc, ssize_t len, void *bp)
{
	int wait = wd33c93_data_wait;
	u_char asr, *buf = bp;

	QPRINTF(("wd33c93_xfout {%d} %02x %02x %02x %02x %02x "
		    "%02x %02x %02x %02x %02x\n", len, buf[0], buf[1], buf[2],
		    buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9]));

	/*
	 * sigh.. WD-PROTO strikes again.. sending the command in one go
	 * causes the chip to lock up if talking to certain (misbehaving?)
	 * targets. Anyway, this procedure should work for all targets, but
	 * it's slightly slower due to the overhead
	 */

	SET_SBIC_control(sc, SBIC_CTL_EDI | SBIC_CTL_IDI);
	SBIC_TC_PUT (sc, (unsigned)len);

	WAIT_CIP (sc);
	SET_SBIC_cmd (sc, SBIC_CMD_XFER_INFO);

	/*
	 * Loop for each byte transferred
	 */
	do {
		GET_SBIC_asr (sc, asr);

		if (asr & SBIC_ASR_DBR) {
			if (len) {
				SET_SBIC_data (sc, *buf);
				buf++;
				len--;
			} else {
				SET_SBIC_data (sc, 0);
			}
			wait = wd33c93_data_wait;
		}
	} while (len && (asr & SBIC_ASR_INT) == 0 && wait-- > 0);

	QPRINTF(("wd33c93_xfout done: %d bytes remaining (wait:%d)\n", len, wait));

	/*
	 * Normally, an interrupt will be pending when this routing returns.
	 */
	return(len);
}

/*
 * Information Transfer *from* a Scsi Target
 * returns # bytes left to read
 */
ssize_t
wd33c93_xfin(struct wd33c93_softc *sc, ssize_t len, void *bp)
{
	int wait = wd33c93_data_wait;
	u_char *buf = bp;
	u_char asr;
#ifdef SBICDEBUG
	u_char *obp = bp;
#endif
	SET_SBIC_control(sc, SBIC_CTL_EDI | SBIC_CTL_IDI);
	SBIC_TC_PUT (sc, (unsigned)len);

	WAIT_CIP (sc);
	SET_SBIC_cmd (sc, SBIC_CMD_XFER_INFO);

	/*
	 * Loop for each byte transferred
	 */
	do {
		GET_SBIC_asr (sc, asr);

		if (asr & SBIC_ASR_DBR) {
			if (len) {
				GET_SBIC_data (sc, *buf);
				buf++;
				len--;
			} else {
				u_char foo;
				GET_SBIC_data (sc, foo);
			}
			wait = wd33c93_data_wait;
		}

	} while ((asr & SBIC_ASR_INT) == 0 && wait-- > 0);

	QPRINTF(("wd33c93_xfin {%d} %02x %02x %02x %02x %02x %02x "
		    "%02x %02x %02x %02x\n", len, obp[0], obp[1], obp[2],
		    obp[3], obp[4], obp[5], obp[6], obp[7], obp[8], obp[9]));

	SBIC_TC_PUT (sc, 0);

	/*
	 * this leaves with one csr to be read
	 */
	return len;
}


/*
 * Finish SCSI xfer command:  After the completion interrupt from
 * a read/write operation, sequence through the final phases in
 * programmed i/o.
 */
void
wd33c93_xferdone(struct wd33c93_softc *sc)
{
	u_char	phase, csr;
	int	s;

	QPRINTF(("{"));
	s = splbio();

	/*
	 * have the wd33c93 complete on its own
	 */
	SBIC_TC_PUT(sc, 0);
	SET_SBIC_cmd_phase(sc, 0x46);
	SET_SBIC_cmd(sc, SBIC_CMD_SEL_ATN_XFER);

	do {
		SBIC_WAIT (sc, SBIC_ASR_INT, 0);
		GET_SBIC_csr (sc, csr);
		QPRINTF(("%02x:", csr));
	} while ((csr != SBIC_CSR_DISC) &&
		 (csr != SBIC_CSR_DISC_1) &&
		 (csr != SBIC_CSR_S_XFERRED));

	sc->sc_flags &= ~SBICF_SELECTED;
	sc->sc_state = SBIC_DISCONNECT;

	GET_SBIC_cmd_phase (sc, phase);
	QPRINTF(("}%02x", phase));

	if (phase == 0x60)
		GET_SBIC_tlun(sc, sc->sc_status);
	else
		wd33c93_error(sc, sc->sc_nexus);

	QPRINTF(("=STS:%02x=\n", sc->sc_status));
	splx(s);
}

int
wd33c93_loop(struct wd33c93_softc *sc, u_char asr, u_char csr)
{
	int i;

	do {
		i = wd33c93_nextstate(sc, sc->sc_nexus, csr, asr);
		WAIT_CIP(sc);		/* XXX */
		if (sc->sc_state == SBIC_CONNECTED) {
			GET_SBIC_asr(sc, asr);

			if (asr & SBIC_ASR_LCI)
				printf("%s: %s: LCI asr:%02x csr:%02x\n",
				    sc->sc_dev.dv_xname, __func__, asr, csr);

			if (asr & SBIC_ASR_INT)
				GET_SBIC_csr(sc, csr);
		}
	} while (sc->sc_state == SBIC_CONNECTED &&
	    	 asr & (SBIC_ASR_INT | SBIC_ASR_LCI));

	return i;
}

int
wd33c93_go(struct wd33c93_softc *sc, struct wd33c93_acb *acb)
{
	struct scsi_xfer	*xs = acb->xs;
	struct scsi_link	*sc_link = xs->sc_link;
	int			i, dmaok;
	u_char			csr, asr;

	SBIC_DEBUG(ACBS, ("wd33c93_go(%d:%d)\n", sc_link->target, sc_link->lun));

	sc->sc_nexus = acb;

	sc->target = sc_link->target;
	sc->lun    = sc_link->lun;

	sc->sc_status = STATUS_UNKNOWN;
	sc->sc_daddr = acb->daddr;
	sc->sc_dleft = acb->dleft;

	sc->sc_msgpriq = sc->sc_msgout = sc->sc_msgoutq = 0;
	sc->sc_flags = 0;

	dmaok = wd33c93_dmaok(sc, xs);

	if (dmaok == 0)
		sc->sc_flags |= SBICF_NODMA;

	SBIC_DEBUG(DMA, ("wd33c93_go dmago:%d(tcnt=%zx) dmaok=%d\n",
		       sc->target, sc->sc_tcnt, dmaok));

	/* select the SCSI bus (it's an error if bus isn't free) */
	if ((csr = wd33c93_selectbus(sc, acb)) == 0)
		return(0); /* Not done: needs to be rescheduled */

	/*
	 * Lets cycle a while then let the interrupt handler take over.
	 */
	GET_SBIC_asr(sc, asr);
	QPRINTF(("go[0x%x] ", csr));
	i = wd33c93_loop(sc, asr, csr);
	QPRINTF(("> done i=%d stat=%02x\n", i, sc->sc_status));

	switch (i) {
	case SBIC_STATE_DONE:
		if (sc->sc_status == STATUS_UNKNOWN) {
			sc_print_addr(sc_link);
			printf("%s: done & stat == UNKNOWN\n", __func__);
			return 1;  /* Did we really finish that fast? */
		}
		break;
	case SBIC_STATE_DISCONNECT:
		xs->error = XS_SELTIMEOUT;
		/* wd33c93_sched() will invoke wd33c93_scsidone() for us */
		break;
	}
	return 0;
}


int
wd33c93_intr(void *v)
{
	struct wd33c93_softc *sc = v;
	u_char asr, csr;

	/*
	 * pending interrupt?
	 */
	GET_SBIC_asr (sc, asr);
	if ((asr & SBIC_ASR_INT) == 0)
		return 0;

	GET_SBIC_csr(sc, csr);
	SBIC_DEBUG(INTS, ("intr[csr=0x%x]", csr));
	(void)wd33c93_loop(sc, asr, csr);
	SBIC_DEBUG(INTS, ("intr done\n"));

	return 1;
}

/*
 * Complete current command using polled I/O.   Used when interrupt driven
 * I/O is not allowed (ie. during boot and shutdown)
 *
 * Polled I/O is very processor intensive
 */
int
wd33c93_poll(struct wd33c93_softc *sc, struct wd33c93_acb *acb)
{
	u_char			asr, csr;
	int			count;
	struct scsi_xfer	*xs = acb->xs;
	int s;

	SBIC_WAIT(sc, SBIC_ASR_INT, wd33c93_cmd_wait);
	for (count = acb->timeout; count;) {
		GET_SBIC_asr(sc, asr);
		GET_SBIC_csr(sc, csr);
		if (asr & SBIC_ASR_LCI) {
			sc_print_addr(xs->sc_link);
			printf("%s: LCI; asr:%02x csr:%02x\n",
			    __func__, asr, csr);
		}
		if (asr & SBIC_ASR_INT) {
			/* inline wd33c93_intr(sc) */
			s = splbio();
			(void)wd33c93_loop(sc, asr, csr);
			splx(s);
		} else {
			DELAY(1000);
			count--;
		}

		if ((xs->flags & ITSDONE) != 0) {
			return (0);
		}

		s = splbio();
		if (sc->sc_state == SBIC_IDLE) {
			SBIC_DEBUG(ACBS, ("[poll: rescheduling] "));
			wd33c93_sched(sc);
		}
		splx(s);
	}
	return (1);
}

static inline int
__verify_msg_format(u_char *p, int len)
{
	if (len == 1 && IS1BYTEMSG(p[0]))
		return 1;
	if (len == 2 && IS2BYTEMSG(p[0]))
		return 1;
	if (len >= 3 && ISEXTMSG(p[0]) &&
	    len == p[1] + 2)
		return 1;
	return 0;
}

/*
 * Handle message_in phase
 */
int
wd33c93_msgin_phase(struct wd33c93_softc *sc, int reselect)
{
	int len;
	u_char asr, csr, *msg;

	GET_SBIC_asr(sc, asr);

	SBIC_DEBUG(MSGS, ("wd33c93msgin asr=%02x\n", asr));

	GET_SBIC_selid (sc, csr);
	SET_SBIC_selid (sc, csr | SBIC_SID_FROM_SCSI);

	SBIC_TC_PUT(sc, 0);

	SET_SBIC_control(sc, SBIC_CTL_EDI | SBIC_CTL_IDI);

	msg = sc->sc_imsg;
	len = 0;

	do {
		/* Fetch the next byte of the message */
		RECV_BYTE(sc, *msg++);
		len++;

		/*
		 * get the command completion interrupt, or we
		 * can't send a new command (LCI)
		 */
		SBIC_WAIT(sc, SBIC_ASR_INT, 0);
		GET_SBIC_csr(sc, csr);

		if (__verify_msg_format(sc->sc_imsg, len))
			break; /* Complete message received */

		/*
		 * Clear ACK, and wait for the interrupt
		 * for the next byte or phase change
		 */
		SET_SBIC_cmd(sc, SBIC_CMD_CLR_ACK);
		SBIC_WAIT(sc, SBIC_ASR_INT, 0);

		GET_SBIC_csr(sc, csr);
	} while (len < SBIC_MAX_MSGLEN);

	if (__verify_msg_format(sc->sc_imsg, len))
		wd33c93_msgin(sc, sc->sc_imsg, len);

	/*
	 * Clear ACK, and wait for the interrupt
	 * for the phase change
	 */
	SET_SBIC_cmd(sc, SBIC_CMD_CLR_ACK);
	SBIC_WAIT(sc, SBIC_ASR_INT, 0);

	/* Should still have one CSR to read */
	return SBIC_STATE_RUNNING;
}


void wd33c93_msgin(struct wd33c93_softc *sc, u_char *msgaddr, int msglen)
{
	struct wd33c93_acb *acb = sc->sc_nexus;
	struct wd33c93_tinfo *ti = &sc->sc_tinfo[sc->target];
	struct wd33c93_linfo *li;
	u_char asr;

	switch (sc->sc_state) {
	case SBIC_CONNECTED:
		switch (msgaddr[0]) {
		case MSG_MESSAGE_REJECT:
			SBIC_DEBUG(MSGS, ("msgin: MSG_REJECT, "
				       "last msgout=%x\n", sc->sc_msgout));
			switch (sc->sc_msgout) {
			case SEND_TAG:
				sc_print_addr(acb->xs->sc_link);
				printf("tagged queuing rejected\n");
				ti->flags &= ~T_TAG;
				li = TINFO_LUN(ti, sc->lun);
				if (acb->tag_type &&
				    li->queued[acb->tag_id] != NULL) {
					li->queued[acb->tag_id] = NULL;
					li->used--;
				}
				acb->tag_type = acb->tag_id = 0;
				li->untagged = acb;
				li->state = L_STATE_BUSY;
				break;

			case SEND_SDTR:
				sc_print_addr(acb->xs->sc_link);
				printf("sync transfer rejected\n");

				sc->sc_flags &= ~SBICF_SYNCNEGO;
				ti->flags &= ~(T_NEGOTIATE | T_SYNCMODE);
				wd33c93_setsync(sc, ti);

			case SEND_INIT_DET_ERR:
				goto abort;

			default:
				SBIC_DEBUG(MSGS, ("Unexpected MSG_REJECT\n"));
				break;
			}
			sc->sc_msgout = 0;
			break;

		case MSG_HEAD_OF_Q_TAG:
		case MSG_ORDERED_Q_TAG:
		case MSG_SIMPLE_Q_TAG:
			sc_print_addr(acb->xs->sc_link);
			printf("out of phase TAG; Tag=%02x/%02x\n",
			    msgaddr[0], msgaddr[1]);
			break;

		case MSG_DISCONNECT:
			SBIC_DEBUG(MSGS, ("msgin: DISCONNECT"));
			/*
			 * Mark the fact that all bytes have moved. The
			 * target may not bother to do a SAVE POINTERS
			 * at this stage. This flag will set the residual
			 * count to zero on MSG COMPLETE.
			 */
			if (sc->sc_dleft == 0)
				acb->flags |= ACB_COMPLETE;

			if (acb->xs->flags & SCSI_POLL)
				/* Don't allow disconnect in immediate mode */
				goto reject;
			else {  /* Allow disconnect */
				sc->sc_flags &= ~SBICF_SELECTED;
				sc->sc_state = SBIC_DISCONNECT;
			}
			if ((acb->xs->sc_link->quirks & SDEV_AUTOSAVE) == 0)
				break;
			/*FALLTHROUGH*/

		case MSG_SAVEDATAPOINTER:
			SBIC_DEBUG(MSGS, ("msgin: SAVEDATAPTR"));
			acb->daddr = sc->sc_daddr;
			acb->dleft = sc->sc_dleft;
			break;

		case MSG_RESTOREPOINTERS:
			SBIC_DEBUG(MSGS, ("msgin: RESTOREPTR"));
			sc->sc_daddr = acb->daddr;
			sc->sc_dleft = acb->dleft;
			break;

		case MSG_CMDCOMPLETE:
			/*
			 * !! KLUDGE ALERT !! quite a few drives don't seem to
			 * really like the current way of sending the
			 * sync-handshake together with the ident-message, and
			 * they react by sending command-complete and
			 * disconnecting right after returning the valid sync
			 * handshake. So, all I can do is reselect the drive,
			 * and hope it won't disconnect again. I don't think
			 * this is valid behavior, but I can't help fixing a
			 * problem that apparently exists.
			 *
			 * Note: we should not get here on `normal' command
			 * completion, as that condition is handled by the
			 * high-level sel&xfer resume command used to walk
			 * thru status/cc-phase.
			 */
			SBIC_DEBUG(MSGS, ("msgin: CMD_COMPLETE"));
			SBIC_DEBUG(SYNC, ("GOT MSG %d! target %d"
				       " acting weird.."
				       " waiting for disconnect...\n",
				       msgaddr[0], sc->target));

			/* Check to see if wd33c93 is handling this */
			GET_SBIC_asr(sc, asr);
			if (asr & SBIC_ASR_BSY)
				break;

			/* XXX: Assume it works and set status to 00 */
			sc->sc_status = 0;
			sc->sc_state = SBIC_CMDCOMPLETE;
			break;

		case MSG_EXTENDED:
			switch(msgaddr[2]) {
			case MSG_EXT_SDTR: /* Sync negotiation */
				SBIC_DEBUG(MSGS, ("msgin: EXT_SDTR; "
					       "period %d, offset %d",
					       msgaddr[3], msgaddr[4]));
				if (msgaddr[1] != 3)
					goto reject;

				ti->period =
				    MAX(msgaddr[3], sc->sc_minsyncperiod);
				ti->offset = MIN(msgaddr[4], sc->sc_maxoffset);

				/*
				 * <SGI, IBM DORS-32160, WA6A> will do nothing
				 * but attempt sync negotiation until it gets
				 * what it wants. To avoid an infinite loop set
				 * off by the identify request, oblige them.
				 */
				if ((sc->sc_flags & SBICF_SYNCNEGO) == 0 &&
				    msgaddr[3] != 0)
					ti->flags |= T_WANTSYNC;

				if (!(ti->flags & T_WANTSYNC))
					ti->period = ti->offset = 0;

				ti->flags &= ~T_NEGOTIATE;

				if (ti->offset == 0)
					ti->flags &= ~T_SYNCMODE; /* Async */
				else
					ti->flags |= T_SYNCMODE; /* Sync */

				/* target initiated negotiation */
				if ((sc->sc_flags&SBICF_SYNCNEGO) == 0)
					wd33c93_sched_msgout(sc, SEND_SDTR);
				sc->sc_flags &= ~SBICF_SYNCNEGO;

				SBIC_DEBUG(SYNC, ("msgin(%d): SDTR(o=%d,p=%d)",
					       sc->target, ti->offset,
					       ti->period));
				wd33c93_setsync(sc, ti);
				break;

			case MSG_EXT_WDTR:
				SBIC_DEBUG(MSGS, ("msgin: EXT_WDTR rejected"));
				goto reject;

			default:
				sc_print_addr(acb->xs->sc_link);
				printf("unrecognized MESSAGE EXTENDED;"
				    " sending REJECT\n");
				goto reject;
			}
			break;

		default:
			sc_print_addr(acb->xs->sc_link);
			printf("unrecognized MESSAGE; sending REJECT\n");

		reject:
			/* We don't support whatever this message is... */
			wd33c93_sched_msgout(sc, SEND_REJECT);
			break;
		}
		break;

	case SBIC_IDENTIFIED:
		/*
		 * IDENTIFY message was received and queue tag is expected now
		 */
		if ((msgaddr[0]!=MSG_SIMPLE_Q_TAG) || (sc->sc_msgify==0)) {
			printf("%s: TAG reselect without IDENTIFY;"
			    " MSG %x; sending DEVICE RESET\n",
			    sc->sc_dev.dv_xname, msgaddr[0]);
			goto reset;
		}
		SBIC_DEBUG(TAGS, ("TAG %x/%x\n", msgaddr[0], msgaddr[1]));
		if (sc->sc_nexus)
			printf("*TAG Recv with active nexus!!\n");
		wd33c93_reselect(sc, sc->target, sc->lun,
		    	      msgaddr[0], msgaddr[1]);
		break;

	case SBIC_RESELECTED:
		/*
		 * IDENTIFY message with target
		 */
		if (MSG_ISIDENTIFY(msgaddr[0])) {
			SBIC_DEBUG(PHASE, ("IFFY[%x] ", msgaddr[0]));
			sc->sc_msgify = msgaddr[0];
		} else {
			printf("%s: reselect without IDENTIFY;"
			    " MSG %x; sending DEVICE RESET\n",
			    sc->sc_dev.dv_xname, msgaddr[0]);
			goto reset;
		}
		break;

	default:
		printf("%s: unexpected MESSAGE IN.  State=%d - Sending RESET\n",
		    sc->sc_dev.dv_xname, sc->sc_state);
	reset:
		wd33c93_sched_msgout(sc, SEND_DEV_RESET);
		break;
	abort:
		wd33c93_sched_msgout(sc, SEND_ABORT);
		break;
	}
}

void
wd33c93_sched_msgout(struct wd33c93_softc *sc, u_short msg)
{
	u_char	asr;

	SBIC_DEBUG(SYNC,("sched_msgout: %04x\n", msg));
	sc->sc_msgpriq |= msg;

	/* Schedule MSGOUT Phase to send message */

	WAIT_CIP(sc);
	SET_SBIC_cmd(sc, SBIC_CMD_SET_ATN);
	WAIT_CIP(sc);
	GET_SBIC_asr(sc, asr);
	if (asr & SBIC_ASR_LCI) {
		printf("%s: MSGOUT Failed!\n", sc->sc_dev.dv_xname);
	}
	SET_SBIC_cmd(sc, SBIC_CMD_CLR_ACK);
	WAIT_CIP(sc);
}

/*
 * Send the highest priority, scheduled message
 */
void
wd33c93_msgout(struct wd33c93_softc *sc)
{
	struct wd33c93_tinfo *ti;
	struct wd33c93_acb *acb = sc->sc_nexus;

	if (acb == NULL)
		panic("MSGOUT with no nexus");

	if (sc->sc_omsglen == 0) {
		/* Pick up highest priority message */
		sc->sc_msgout   = sc->sc_msgpriq & -sc->sc_msgpriq;
		sc->sc_msgoutq |= sc->sc_msgout;
		sc->sc_msgpriq &= ~sc->sc_msgout;
		sc->sc_omsglen = 1;		/* "Default" message len */
		switch (sc->sc_msgout) {
		case SEND_SDTR:
			ti = &sc->sc_tinfo[acb->xs->sc_link->target];
			sc->sc_omsg[0] = MSG_EXTENDED;
			sc->sc_omsg[1] = MSG_EXT_SDTR_LEN;
			sc->sc_omsg[2] = MSG_EXT_SDTR;
			if (ti->flags & T_WANTSYNC) {
				sc->sc_omsg[3] = ti->period;
				sc->sc_omsg[4] = ti->offset;
			} else {
				sc->sc_omsg[3] = 0;
				sc->sc_omsg[4] = 0;
			}
			sc->sc_omsglen = 5;
			if ((sc->sc_flags & SBICF_SYNCNEGO) == 0) {
				if (ti->flags & T_WANTSYNC)
					ti->flags |= T_SYNCMODE;
				else
					ti->flags &= ~T_SYNCMODE;
				wd33c93_setsync(sc, ti);
			}
			break;
		case SEND_IDENTIFY:
#ifdef DIAGNOSTIC
			if (sc->sc_state != SBIC_CONNECTED) {
				printf("%s at line %d: no nexus\n",
				    sc->sc_dev.dv_xname, __LINE__);
			}
#endif
			sc->sc_omsg[0] =
			    MSG_IDENTIFY(acb->xs->sc_link->lun, 0);
			break;
		case SEND_TAG:
#ifdef DIAGNOSTIC
			if (sc->sc_state != SBIC_CONNECTED) {
				printf("%s at line %d: no nexus\n",
				    sc->sc_dev.dv_xname, __LINE__);
			}
#endif
			sc->sc_omsg[0] = acb->tag_type;
			sc->sc_omsg[1] = acb->tag_id;
			sc->sc_omsglen = 2;
			break;
		case SEND_DEV_RESET:
			sc->sc_omsg[0] = MSG_BUS_DEV_RESET;
			ti = &sc->sc_tinfo[sc->target];
			ti->flags &= ~T_SYNCMODE;
			if ((ti->flags & T_NOSYNC) == 0)
				/* We can re-start sync negotiation */
				ti->flags |= T_NEGOTIATE;
			break;
		case SEND_PARITY_ERROR:
			sc->sc_omsg[0] = MSG_PARITY_ERROR;
			break;
		case SEND_ABORT:
			sc->sc_flags  |= SBICF_ABORTING;
			sc->sc_omsg[0] = MSG_ABORT;
			break;
		case SEND_INIT_DET_ERR:
			sc->sc_omsg[0] = MSG_INITIATOR_DET_ERR;
			break;
		case SEND_REJECT:
			sc->sc_omsg[0] = MSG_MESSAGE_REJECT;
			break;
		default:
			/* Wasn't expecting MSGOUT Phase */
			sc->sc_omsg[0] = MSG_NOOP;
			break;
		}
	}

	wd33c93_xfout(sc, sc->sc_omsglen, sc->sc_omsg);
}


/*
 * wd33c93_nextstate()
 * return:
 *	SBIC_STATE_DONE		== done
 *	SBIC_STATE_RUNNING	== working
 *	SBIC_STATE_DISCONNECT	== disconnected
 *	SBIC_STATE_ERROR	== error
 */
int
wd33c93_nextstate(struct wd33c93_softc *sc, struct wd33c93_acb *acb, u_char csr, u_char asr)
{
	SBIC_DEBUG(PHASE, ("next[a=%02x,c=%02x]: ",asr,csr));

	switch (csr) {

	case SBIC_CSR_XFERRED | CMD_PHASE:
	case SBIC_CSR_MIS     | CMD_PHASE:
	case SBIC_CSR_MIS_1   | CMD_PHASE:
	case SBIC_CSR_MIS_2   | CMD_PHASE:

		if (wd33c93_xfout(sc, acb->clen, &acb->cmd))
			goto abort;
		break;

	case SBIC_CSR_XFERRED | STATUS_PHASE:
	case SBIC_CSR_MIS     | STATUS_PHASE:
	case SBIC_CSR_MIS_1   | STATUS_PHASE:
	case SBIC_CSR_MIS_2   | STATUS_PHASE:

		SET_SBIC_control(sc, SBIC_CTL_EDI | SBIC_CTL_IDI);

		/*
		 * this should be the normal i/o completion case.
		 * get the status & cmd complete msg then let the
		 * device driver look at what happened.
		 */
		wd33c93_xferdone(sc);

		wd33c93_dma_stop(sc);

		/* Fixup byte count to be passed to higher layer */
		acb->dleft = (acb->flags & ACB_COMPLETE) ? 0 :
		    	      sc->sc_dleft;

		/*
		 * Indicate to the upper layers that the command is done
		 */
		wd33c93_scsidone(sc, acb, sc->sc_status);

		return SBIC_STATE_DONE;


	case SBIC_CSR_XFERRED | DATA_IN_PHASE:
	case SBIC_CSR_MIS     | DATA_IN_PHASE:
	case SBIC_CSR_MIS_1   | DATA_IN_PHASE:
	case SBIC_CSR_MIS_2   | DATA_IN_PHASE:
	case SBIC_CSR_XFERRED | DATA_OUT_PHASE:
	case SBIC_CSR_MIS     | DATA_OUT_PHASE:
	case SBIC_CSR_MIS_1   | DATA_OUT_PHASE:
	case SBIC_CSR_MIS_2   | DATA_OUT_PHASE:
		/*
		 * Verify that we expected to transfer data...
		 */
		if (acb->dleft <= 0) {
			sc_print_addr(acb->xs->sc_link);
			printf("next: DATA phase with xfer count == %zd, "
			    "asr:0x%02x csr:0x%02x\n",
			    acb->dleft, asr, csr);
			goto abort;
		}

		/*
		 * Should we transfer using PIO or DMA ?
		 */
		if (acb->xs->flags & SCSI_POLL ||
		    sc->sc_flags & SBICF_NODMA) {
			/* Perfrom transfer using PIO */
			ssize_t resid;

			SBIC_DEBUG(DMA, ("PIO xfer: %d(%p:%zx)\n", sc->target,
				       sc->sc_daddr, sc->sc_dleft));

			if (SBIC_PHASE(csr) == DATA_IN_PHASE)
				/* data in */
				resid = wd33c93_xfin(sc, sc->sc_dleft,
				    		 sc->sc_daddr);
			else	/* data out */
				resid = wd33c93_xfout(sc, sc->sc_dleft,
				    		  sc->sc_daddr);

			sc->sc_daddr = (char *)sc->sc_daddr +
				(acb->dleft - resid);
			sc->sc_dleft = resid;
		} else {
			int datain = SBIC_PHASE(csr) == DATA_IN_PHASE;

			/* Perform transfer using DMA */
			wd33c93_dma_setup(sc, datain);

			SET_SBIC_control(sc, SBIC_CTL_EDI | SBIC_CTL_IDI |
			    sc->sc_dmamode);

			SBIC_DEBUG(DMA, ("DMA xfer: %d(%p:%zx)\n", sc->target,
				       sc->sc_daddr, sc->sc_dleft));

			/* Setup byte count for transfer */
			SBIC_TC_PUT(sc, (unsigned)sc->sc_dleft);

			/* Start the transfer */
			SET_SBIC_cmd(sc, SBIC_CMD_XFER_INFO);

			/* Start the DMA chip going */
			sc->sc_tcnt = sc->sc_dmago(sc);

			/* Indicate that we're in DMA mode */
			sc->sc_flags |= SBICF_INDMA;
		}
		break;

	case SBIC_CSR_XFERRED | MESG_IN_PHASE:
	case SBIC_CSR_MIS     | MESG_IN_PHASE:
	case SBIC_CSR_MIS_1   | MESG_IN_PHASE:
	case SBIC_CSR_MIS_2   | MESG_IN_PHASE:

		wd33c93_dma_stop(sc);

		/* Handle a single message in... */
		return wd33c93_msgin_phase(sc, 0);

	case SBIC_CSR_MSGIN_W_ACK:

		/*
		 * We should never see this since it's handled in
		 * 'wd33c93_msgin_phase()' but just for the sake of paranoia...
		 */
		SET_SBIC_cmd(sc, SBIC_CMD_CLR_ACK);

		sc_print_addr(acb->xs->sc_link);
		printf("acking unknown msgin CSR:%02x\n", csr);
		break;

	case SBIC_CSR_XFERRED | MESG_OUT_PHASE:
	case SBIC_CSR_MIS     | MESG_OUT_PHASE:
	case SBIC_CSR_MIS_1   | MESG_OUT_PHASE:
	case SBIC_CSR_MIS_2   | MESG_OUT_PHASE:

		/*
		 * Message out phase.  ATN signal has been asserted
		 */
		wd33c93_dma_stop(sc);
		wd33c93_msgout(sc);
		return SBIC_STATE_RUNNING;

	case SBIC_CSR_DISC:
	case SBIC_CSR_DISC_1:
		SBIC_DEBUG(RSEL, ("wd33c93next target %d disconnected\n",
			       sc->target));
		wd33c93_dma_stop(sc);

		sc->sc_nexus = NULL;
		sc->sc_state = SBIC_IDLE;
		sc->sc_flags = 0;

		++sc->sc_tinfo[sc->target].dconns;
		++sc->sc_disc;

		if (acb->xs->flags & SCSI_POLL || wd33c93_nodisc)
			return SBIC_STATE_DISCONNECT;

		/* Try to schedule another target */
		wd33c93_sched(sc);

		return SBIC_STATE_DISCONNECT;

	case SBIC_CSR_RSLT_NI:
	case SBIC_CSR_RSLT_IFY:
	{
		/*
		 * A reselection.
		 * Note that since we don't enable Advanced Features (assuming
		 * the WD chip is at least the 'A' revision), we're only ever
		 * likely to see the 'SBIC_CSR_RSLT_NI' status. But for the
		 * hell of it, we'll handle it anyway, for all the extra code
		 * it needs...
		 */
		u_char  newtarget, newlun;

		if (sc->sc_flags & SBICF_INDMA) {
			printf("%s: **** RESELECT WHILE DMA ACTIVE!!! ***\n",
			    sc->sc_dev.dv_xname);
			wd33c93_dma_stop(sc);
		}

		sc->sc_state = SBIC_RESELECTED;
		GET_SBIC_rselid(sc, newtarget);

		/* check SBIC_RID_SIV? */
		newtarget &= SBIC_RID_MASK;

		if (csr == SBIC_CSR_RSLT_IFY) {
			/* Read Identify msg to avoid lockup */
			GET_SBIC_data(sc, newlun);
			WAIT_CIP(sc);
			newlun &= SBIC_TLUN_MASK;
			sc->sc_msgify = MSG_IDENTIFY(newlun, 0);
		} else {
			/*
			 * Need to read Identify message the hard way, assuming
			 * the target even sends us one...
			 */
			for (newlun = 255; newlun; --newlun) {
				GET_SBIC_asr(sc, asr);
				if (asr & SBIC_ASR_INT)
					break;
				DELAY(10);
			}

			/* If we didn't get an interrupt, somethink's up */
			if ((asr & SBIC_ASR_INT) == 0) {
				printf("%s: Reselect without identify? asr %x\n",
				    sc->sc_dev.dv_xname, asr);
				newlun = 0; /* XXXX */
			} else {
				/*
				 * We got an interrupt, verify that it's a
				 * change to message in phase, and if so
				 * read the message.
				 */
				GET_SBIC_csr(sc,csr);

				if (csr == (SBIC_CSR_MIS   | MESG_IN_PHASE) ||
				    csr == (SBIC_CSR_MIS_1 | MESG_IN_PHASE) ||
				    csr == (SBIC_CSR_MIS_2 | MESG_IN_PHASE)) {
					/*
					 * Yup, gone to message in.
					 * Fetch the target LUN
					 */
					sc->sc_msgify = 0;
					wd33c93_msgin_phase(sc, 1);
					newlun = sc->sc_msgify & SBIC_TLUN_MASK;
				} else {
					/*
					 * Whoops! Target didn't go to msg_in
					 * phase!!
					 */
					printf("%s: RSLT_NI - not MESG_IN_PHASE %x\n",
					    sc->sc_dev.dv_xname, csr);
					newlun = 0; /* XXXSCW */
				}
			}
		}

		/* Ok, we have the identity of the reselecting target. */
		SBIC_DEBUG(RSEL, ("wd33c93next: reselect from targ %d lun %d",
			       newtarget, newlun));
		wd33c93_reselect(sc, newtarget, newlun, 0, 0);
		sc->sc_disc--;

		if (csr == SBIC_CSR_RSLT_IFY)
			SET_SBIC_cmd(sc, SBIC_CMD_CLR_ACK);
		break;
	}

	default:
	abort:
		/* Something unexpected happend -- deal with it. */
		printf("%s: next: aborting asr 0x%02x csr 0x%02x\n",
		    sc->sc_dev.dv_xname, asr, csr);

#ifdef DDB
		Debugger();
#endif

		SET_SBIC_control(sc, SBIC_CTL_EDI | SBIC_CTL_IDI);
		if (acb->xs)
			wd33c93_error(sc, acb);
		wd33c93_abort(sc, acb, "next");

		if (sc->sc_flags & SBICF_INDMA) {
			wd33c93_dma_stop(sc);
			wd33c93_scsidone(sc, acb, STATUS_UNKNOWN);
		}
		return SBIC_STATE_ERROR;
	}
	return SBIC_STATE_RUNNING;
}


void
wd33c93_reselect(struct wd33c93_softc *sc, int target, int lun, int tag_type, int tag_id)
{

	struct wd33c93_tinfo *ti;
	struct wd33c93_linfo *li;
	struct wd33c93_acb *acb;

	if (sc->sc_nexus) {
		/*
		 * Whoops! We've been reselected with a
		 * command in progress!
		 * The best we can do is to put the current
		 * command back on the ready list and hope
		 * for the best.
		 */
		SBIC_DEBUG(RSEL, ("%s: reselect with active command\n",
		    sc->sc_dev.dv_xname));
		ti = &sc->sc_tinfo[sc->target];
		li = TINFO_LUN(ti, sc->lun);
		li->state = L_STATE_IDLE;

		wd33c93_dequeue(sc, sc->sc_nexus);
		TAILQ_INSERT_HEAD(&sc->ready_list, sc->sc_nexus, chain);
		sc->sc_nexus->flags |= ACB_READY;

		sc->sc_nexus = NULL;
	}

	/* Setup state for new nexus */
	acb = NULL;
	sc->sc_flags = SBICF_SELECTED;
	sc->sc_msgpriq = sc->sc_msgout = sc->sc_msgoutq = 0;

	ti = &sc->sc_tinfo[target];
	li = TINFO_LUN(ti, lun);

	if (li != NULL) {
		if (li->untagged != NULL && li->state)
			acb = li->untagged;
		else if (tag_type != MSG_SIMPLE_Q_TAG) {
			/* Wait for tag to come by during MESG_IN Phase */
			sc->target    = target; /* setup I_T_L nexus */
			sc->lun       = lun;
			sc->sc_state  = SBIC_IDENTIFIED;
			return;
		} else if (tag_type)
			acb = li->queued[tag_id];
	}

	if (acb == NULL) {
		printf("%s: reselect from target %d lun %d tag %x:%x "
		    "with no nexus; sending ABORT\n",
		    sc->sc_dev.dv_xname, target, lun, tag_type, tag_id);
		goto abort;
	}

	sc->target    = target;
	sc->lun       = lun;
	sc->sc_nexus  = acb;
	sc->sc_state  = SBIC_CONNECTED;

	if (!wd33c93_dmaok(sc, acb->xs))
		sc->sc_flags |= SBICF_NODMA;

	/* Do an implicit RESTORE POINTERS. */
	sc->sc_daddr = acb->daddr;
	sc->sc_dleft = acb->dleft;

	/* Set sync modes for new target */
	wd33c93_setsync(sc, ti);

	if (acb->flags & ACB_RESET)
		wd33c93_sched_msgout(sc, SEND_DEV_RESET);
	else if (acb->flags & ACB_ABORT)
		wd33c93_sched_msgout(sc, SEND_ABORT);
	return;

abort:
	wd33c93_sched_msgout(sc, SEND_ABORT);
	return;

}

void
wd33c93_timeout(void *arg)
{
	struct wd33c93_acb *acb = arg;
	struct scsi_xfer *xs = acb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct wd33c93_softc *sc = sc_link->adapter_softc;
	int s, asr, csr;

	s = splbio();

	GET_SBIC_asr(sc, asr);

	sc_print_addr(sc_link);
	printf("timed out; asr=0x%02x [acb %p (flags 0x%x, dleft %zd)], "
	    "<state %d, nexus %p, resid %zd, msg(q %x,o %x)>",
	    asr, acb, acb->flags, acb->dleft, sc->sc_state, sc->sc_nexus,
	    sc->sc_dleft, sc->sc_msgpriq, sc->sc_msgout);

	if (asr & SBIC_ASR_INT) {
		/* We need to service a missed IRQ */
		GET_SBIC_csr(sc, csr);
		(void)wd33c93_loop(sc, asr, csr);
	} else {
		(void)wd33c93_abort(sc, acb, "timeout");
	}
	splx(s);
}


void
wd33c93_watchdog(void *arg)
{
	struct wd33c93_softc *sc = arg;
	struct wd33c93_tinfo *ti;
	struct wd33c93_linfo *li;
	int t, s, l;
	/* scrub LUN's that have not been used in the last 10min. */
	time_t old = time_second - (10 * 60);

	for (t = 0; t < SBIC_NTARG; t++) {
		ti = &sc->sc_tinfo[t];
		for (l = 0; l < SBIC_NLUN; l++) {
			s = splbio();
			li = TINFO_LUN(ti, l);
			if (li && li->last_used < old &&
			    li->untagged == NULL && li->used == 0) {
				ti->lun[li->lun] = NULL;
				free(li, M_DEVBUF);
			}
			splx(s);
		}
	}
	timeout_add_sec(&sc->sc_watchdog, 60);
}

void *
wd33c93_io_get(void *null)
{
	return (pool_get(&wd33c93_pool, PR_NOWAIT));
}

void
wd33c93_io_put(void *null, void *io)
{
	pool_put(&wd33c93_pool, io);
}

#ifdef SBICDEBUG
void
wd33c93_hexdump(u_char *buf, int len)
{
	printf("{%d}:", len);
	while (len--)
		printf(" %02x", *buf++);
	printf("\n");
}


void
wd33c93_print_csr(u_char csr)
{
	switch (SCSI_PHASE(csr)) {
	case CMD_PHASE:
		printf("CMD_PHASE\n");
		break;

	case STATUS_PHASE:
		printf("STATUS_PHASE\n");
		break;

	case DATA_IN_PHASE:
		printf("DATAIN_PHASE\n");
		break;

	case DATA_OUT_PHASE:
		printf("DATAOUT_PHASE\n");
		break;

	case MESG_IN_PHASE:
		printf("MESG_IN_PHASE\n");
		break;

	case MESG_OUT_PHASE:
		printf("MESG_OUT_PHASE\n");
		break;

	default:
		switch (csr) {
		case SBIC_CSR_DISC_1:
			printf("DISC_1\n");
			break;

		case SBIC_CSR_RSLT_NI:
			printf("RESELECT_NO_IFY\n");
			break;

		case SBIC_CSR_RSLT_IFY:
			printf("RESELECT_IFY\n");
			break;

		case SBIC_CSR_SLT:
			printf("SELECT\n");
			break;

		case SBIC_CSR_SLT_ATN:
			printf("SELECT, ATN\n");
			break;

		case SBIC_CSR_UNK_GROUP:
			printf("UNK_GROUP\n");
			break;

		default:
			printf("UNKNOWN csr=%02x\n", csr);
		}
	}
}
#endif

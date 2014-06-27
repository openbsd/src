/*	$OpenBSD: wd33c93var.h,v 1.2 2014/06/27 17:51:08 miod Exp $	*/
/*	$NetBSD: wd33c93var.h,v 1.10 2009/05/12 14:25:18 cegger Exp $	*/

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
 *  @(#)scsivar.h   7.1 (Berkeley) 5/8/90
 */

#define SBIC_NTARG	8
#define SBIC_NLUN	8
#define SBIC_NTAGS	256

#define SBIC_MAX_MSGLEN 8

#define	SBIC_ABORT_TIMEOUT	2000	/* time to wait for abort */
#define	SBIC_SENSE_TIMEOUT	1000	/* time to wait for sense */

/*
 * ACB. Holds additional information for each SCSI command Comments: We
 * need a separate scsi command block because we may need to overwrite it
 * with a request sense command.  Basically, we refrain from fiddling with
 * the scsi_xfer struct (except do the expected updating of return values).
 * We'll generally update: xs->{flags,resid,error,sense,status} and
 * occasionally xs->retries.
 */
struct wd33c93_acb {
	TAILQ_ENTRY(wd33c93_acb) chain;
	struct scsi_xfer *xs;		/* SCSI xfer ctrl block from above */
	int	flags;			/* Status */
#define ACB_FREE	0x00
#define ACB_ACTIVE	0x01
#define ACB_READY	0x02		/* ACB is on ready list */
#define ACB_DONE	0x04
#define ACB_SENSE	0x08		/* ACB Requesting sense */
#define ACB_COMPLETE	0x10		/* Disconnected at end of xfer */
#define ACB_RESET	0x20		/* Require Reset */
#define ACB_ABORT	0x40		/* Require Abort */
	int	timeout;
	struct timeout to;

	struct scsi_generic cmd;	/* SCSI command block */
	char	*daddr;			/* kva for data */
	int	clen;
	ssize_t	dleft;			/* bytes remaining */
	u_char	tag_type;		/* TAG Type (0x20-0x22, 0=No Tags) */
	u_char	tag_id;			/* TAG id number */
};

/*
 * Some info about each (possible) target on the SCSI bus.  This should
 * probably have been a "per target+lunit" structure, but we'll leave it at
 * this for now.  Is there a way to reliably hook it up to sc->fordriver??
 */

struct wd33c93_linfo {
	int	lun;
	LIST_ENTRY(wd33c93_linfo)	link;
	time_t	last_used;
	u_char	used;			/* # slots in use */
	u_char	avail;			/* where to start scanning */
	u_char	state;
#define L_STATE_IDLE	0
#define L_STATE_BUSY	1
#define L_STATE_ESTAT	2
	struct wd33c93_acb	*untagged;
	struct wd33c93_acb	*queued[SBIC_NTAGS];
};

struct wd33c93_tinfo {
	int	cmds;			/* # of commands processed */
	int	dconns;			/* # of disconnects */

	u_char	flags;
#define T_NEED_RESET	0x01		/* Should send a BUS_DEV_RESET */
#define T_NEGOTIATE	0x02		/* (Re)Negotiate synchronous options */
#define T_BUSY		0x04		/* Target is busy */
#define T_SYNCMODE	0x08		/* SYNC mode has been negotiated */
#define T_NOSYNC	0x10		/* Force ASYNC mode */
#define T_NODISC	0x20		/* Don't allow disconnect */
#define T_TAG		0x40		/* Turn on TAG QUEUEs */
#define T_WANTSYNC	0x80		/* Negotiatious should aim for sync */
	u_char	period;			/* Period suggestion */
	u_char	offset;			/* Offset suggestion */
	struct wd33c93_linfo *lun[SBIC_NLUN]; /* LUN list for this target */
};

/* Look up a lun in a tinfo */
#define TINFO_LUN(t, l) 	((t)->lun[(l)])

struct wd33c93_softc {
	struct device	sc_dev;

	struct timeout sc_watchdog;
	struct scsi_link sc_link;
	void	*sc_driver;		/* driver specific field */

	int	target;			/* Currently active target */
	int	lun;			/* Currently active LUN */

	/* WD33c93 registers */
	bus_space_tag_t 	sc_regt;
	bus_space_handle_t 	sc_asr_regh;
	bus_space_handle_t 	sc_data_regh;

	/* Data about the current nexus (updated for every cmd switch) */
	void *	sc_daddr;		/* Current data pointer */
	ssize_t	sc_dleft;		/* Data left to transfer */
	ssize_t	sc_tcnt;		/* number of bytes transfered */

	/* Lists of command blocks */
	TAILQ_HEAD(acb_list, wd33c93_acb) ready_list;

	struct wd33c93_acb	 *sc_nexus;	/* current command */
	struct wd33c93_tinfo sc_tinfo[8];

	u_short	sc_state;
	u_short	sc_status;
	int	sc_disc;		/* current # of active nexus's */
	int	sc_flags;

	/* Message stuff */
	u_short	sc_msgify;		/* Last IDENTIFY message */
	u_short	sc_msgout;		/* Current message out */
	u_short	sc_msgpriq;		/* mesg_out queue (bitmap) */
	u_short	sc_msgoutq;		/* mesg_out queue */

	u_char	sc_imsg[SBIC_MAX_MSGLEN];
	u_char	sc_omsg[SBIC_MAX_MSGLEN];
	u_char	sc_imsglen;
	u_char	sc_omsglen;

	/* Static hardware attributes supplied by attachment */
	int	sc_id;			/* SCSI ID for controller */
	int	sc_clkfreq;		/* wd33c93 clk freq * 10 MHz */
	uint8_t	sc_dmamode;		/* One of SBIC_CTL_*DMA */

	/* Static hardware attributes derived by wd33c93_attach() */
	int	sc_chip;		/* Chip variation */
	int	sc_rev;			/* Chip revision */
	int	sc_cfflags;		/* Copy of config flags */
	int	sc_maxxfer;		/* Maximum transfer size */
	uint8_t	sc_maxoffset;		/* Maximum sync offset (bytes) */
	uint8_t sc_minsyncperiod;	/* Minimum supported sync xfer period */
	uint8_t	sc_syncperiods[7];	/* Sync transfer periods (4ns units) */
	uint8_t	sc_fsyncperiods[3];	/* Sync transfer periods for Fast SCSI*/

	int  (*sc_dmasetup)(struct wd33c93_softc *, void **, ssize_t *, int,
		    ssize_t *);
	int  (*sc_dmago)(struct wd33c93_softc *);
	void (*sc_dmastop)(struct wd33c93_softc *);
	void (*sc_reset)(struct wd33c93_softc *);
};

/* values for sc_flags */
#define SBICF_SELECTED		0x01	/* bus is in selected state. */
#define SBICF_NODMA		0x02	/* Polled transfer */
#define SBICF_INDMA		0x04	/* DMA I/O in progress */
#define SBICF_SYNCNEGO		0x08	/* Sync negotiation in progress */
#define SBICF_ABORTING		0x10	/* Aborting */

/* values for sc_state */
#define SBIC_UNINITIALIZED	0	/* Driver not initialized */
#define SBIC_IDLE		1	/* waiting for something to do */
#define SBIC_SELECTING		2	/* SCSI command is arbiting */
#define SBIC_RESELECTED		3	/* Has been reselected */
#define SBIC_IDENTIFIED		4	/* Has gotten IFY but not TAG */
#define SBIC_CONNECTED		5	/* Actively using the SCSI bus */
#define	SBIC_DISCONNECT		6	/* MSG_DISCONNECT received */
#define	SBIC_CMDCOMPLETE 	7	/* MSG_CMDCOMPLETE received */
#define	SBIC_ERROR		8	/* Error has occurred */
#define SBIC_SELTIMEOUT		9	/* Select Timeout */
#define	SBIC_CLEANING		10	/* Scrubbing ACB's */
#define SBIC_BUSRESET		11	/* SCSI RST has been issued */

/* values for sc_msgout */
#define SEND_DEV_RESET		0x0001
#define SEND_PARITY_ERROR	0x0002
#define SEND_INIT_DET_ERR	0x0004
#define SEND_REJECT		0x0008
#define SEND_IDENTIFY		0x0010
#define SEND_ABORT		0x0020
#define SEND_WDTR		0x0040
#define SEND_SDTR		0x0080
#define SEND_TAG		0x0100

/* WD33c93 chipset revisions - values for sc_rev */
#define	SBIC_CHIP_UNKNOWN	0
#define	SBIC_CHIP_WD33C93	1
#define	SBIC_CHIP_WD33C93A	2
#define	SBIC_CHIP_WD33C93B	3

#define SBIC_CHIP_LIST		{"UNKNOWN", "WD33C93", "WD33C93A", "WD33C93B"}

/* macros for sc_cfflags */
#define CFFLAGS_NODISC(_cf, _t) ((_cf) & (1 << ( 0 + (_t))))
#define CFFLAGS_NOSYNC(_cf, _t) ((_cf) & (1 << ( 8 + (_t))))
#define CFFLAGS_NOTAGS(_cf, _t) ((_cf) & (1 << (16 + (_t))))

/*
 * States returned by our state machine
 */
#define SBIC_STATE_ERROR	-1
#define SBIC_STATE_DONE		0
#define SBIC_STATE_RUNNING	1
#define SBIC_STATE_DISCONNECT	2

#define DEBUG_ACBS	0x01
#define DEBUG_INTS	0x02
#define DEBUG_CMDS	0x04
#define DEBUG_MISC	0x08
#define DEBUG_TRAC	0x10
#define DEBUG_RSEL	0x20
#define DEBUG_PHASE	0x40
#define DEBUG_DMA	0x80
#define DEBUG_CCMDS	0x100
#define DEBUG_MSGS	0x200
#define DEBUG_TAGS	0x400
#define DEBUG_SYNC	0x800

#ifdef SBICDEBUG
extern int wd33c93_debug_flags;
#define SBIC_DEBUG(level, str)						\
	do {								\
		if (wd33c93_debug & __CONCAT(DEBUG_,level))		\
			 printf str;					\
	} while (0)
#else
#define SBIC_DEBUG(level, str)
#endif

void wd33c93_scsi_cmd(struct scsi_xfer *);
void wd33c93_attach(struct wd33c93_softc *, struct scsi_adapter *);
int  wd33c93_intr(void *);

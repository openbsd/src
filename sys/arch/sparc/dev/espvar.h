/*	$NetBSD: espvar.h,v 1.3 1994/11/20 20:52:12 deraadt Exp $ */

/*
 * Copyright (c) 1994 Peter Galbavy.  All rights reserved.
 * Copyright (c) 1995 Theo de Raadt.  All rights reserved.
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
 *	This product includes software developed by Peter Galbavy and
 *	Theo de Raadt.
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

/* esp chip revisions */
#define	ESP100		0
#define	ESP100A		1
#define	ESP200		2

/* Grabbed from Julians SCSI aha-drivers */
#ifdef	DDB
int	Debugger();
#else	DDB
#define	Debugger() panic("should call debugger here (esp.c)")
#endif	DDB

#define ESP_MSGLEN_MAX	8		/* maximum msglen we handle */
#define ESP_SYNC_REQOFF	0		/* offset we request */
#define ESP_SYNC_MAXOFF	15		/* maximum supportable sync offset */
#if ESP_SYNC_REQOFF > ESP_SYNC_MAXOFF
#warning requested sync offset is higher than maximum possible sync offset.
#endif

#define FREQTOCCF(freq)	(((freq + 4) / 5))

/* 
 * ECB. Holds additional information for each SCSI command
 * Comments: We need a separate scsi command block because we may need
 * to overwrite it with a request sense command.  Basically, we refrain
 * from fiddling with the scsi_xfer struct (except do the expected
 * updating of return values). We'll generally update: 
 * xs->{flags,resid,error,sense,status} and occasionally xs->retries.
 */
struct ecb {
	TAILQ_ENTRY(ecb) chain;
	struct	scsi_xfer *xs;		/* SCSI xfer ctrl block from above */
	int	flags;			/* Status */
#define ECB_FREE	0x00
#define ECB_ACTIVE	0x01
#define ECB_DONE	0x04
#define ECB_CHKSENSE	0x08
	struct	scsi_generic cmd;	/* SCSI command block */
	int	clen;
	caddr_t	daddr;			/* Saved data pointer */
	int	dleft;			/* Residue */
	int	stat;			/* SCSI status byte */
};

/* 
 * Some info about each (possible) target on the SCSI bus.  This should 
 * probably have been a "per target+lunit" structure, but we'll leave it at 
 * this for now.  Is there a way to reliably hook it up to sc->fordriver??
 */
struct esp_tinfo {
	int	cmds;			/* #commands processed */
	int	dconns;			/* #disconnects */
	int	touts;			/* #timeouts */
	int	perrs;			/* #parity errors */
	int	senses;			/* #request sense commands sent */
	u_char	lubusy;			/* What luns are busy? */
	u_char	flags;
#define DO_NEGOTIATE	0x01		/* (re)negotiate synchronous options */
#define TARGET_BUSY	0x02		/* target is busy, i.e. cmd in progress */
	u_char	period;			/* sync period */
	u_char	synctp;			/* sync period translated for SYNCTP */
	u_char	offset;			/* sync offset */
};

/* Register a linenumber (for debugging) */
#define LOGLINE(p)

#define ESP_SHOWECBS 0x01
#define ESP_SHOWINTS 0x02
#define ESP_SHOWCMDS 0x04
#define ESP_SHOWMISC 0x08
#define ESP_SHOWTRAC 0x10
#define ESP_SHOWSTART 0x20
#define ESP_SHOWPHASE 0x40

#if ESP_DEBUG
#define ESP_ECBS(str)  do {if (esp_debug & ESP_SHOWECBS) printf str;} while (0)
#define ESP_MISC(str)  do {if (esp_debug & ESP_SHOWMISC) printf str;} while (0)
#define ESP_INTS(str)  do {if (esp_debug & ESP_SHOWINTS) printf str;} while (0)
#define ESP_TRACE(str) do {if (esp_debug & ESP_SHOWTRAC) printf str;} while (0)
#define ESP_CMDS(str)  do {if (esp_debug & ESP_SHOWCMDS) printf str;} while (0)
#define ESP_START(str) do {if (esp_debug & ESP_SHOWSTART) printf str;}while (0)
#define ESP_PHASE(str) do {if (esp_debug & ESP_SHOWPHASE) printf str;}while (0)
#else
#define ESP_ECBS(str)
#define ESP_MISC(str)
#define ESP_INTS(str)
#define ESP_TRACE(str)
#define ESP_CMDS(str)
#define ESP_START(str)
#define ESP_PHASE(str)
#endif

struct esp_softc {
	struct	device sc_dev;
	struct	sbusdev sc_sd;			/* sbus device */
	struct	intrhand sc_ih;			/* intr handler */
	struct	evcnt sc_intrcnt;		/* intr count */
	struct	scsi_link sc_link;		/* scsi lint struct */
	struct	espregs *sc_regs;		/* the registers */
	struct	dma_softc *sc_dma;		/* corresponding DMA ctrlr */

	/* register defaults */
	u_char	sc_cfg1;			/* config 1 */
	u_char	sc_cfg2;			/* config 2, only ESP100A/200 */
	u_char	sc_cfg3;			/* config 3, only ESP200 */
	u_char	sc_ccf;				/* clock conversion */
	u_char	sc_timeout;

	u_char	sc_espintr;			/* copies of registers */
	u_char	sc_espstat;
	u_char	sc_espstep;
	u_char	sc_espfflags;

	struct	bootpath *sc_bp;		/* current boot path component */

	/* Lists of command blocks */
	TAILQ_HEAD(ecb_list, ecb) free_list, ready_list, nexus_list;

	struct	ecb *sc_nexus;			/* current command */
	struct	ecb sc_ecb[8];			/* one per target */
	struct	esp_tinfo sc_tinfo[8];

	/* data about the current nexus (updated for every cmd switch) */
	caddr_t	sc_dp;				/* current data pointer */
	size_t	sc_dleft;			/* data left to transfer */

	/* adapter state */
	int	sc_phase;		/* bus phase (based on sc_espstat) */
	u_char	sc_state;		/* state applicable to the adapter */
	u_char	sc_flags;
	u_char	sc_selid;

	/* message stuff */
	u_char	sc_msgpriq;	/* messages to send (see SEND_* below) */
	u_char	sc_msgout;	/* message that is on it's way out */
	u_char	sc_omess[ESP_MSGLEN_MAX];
	u_char *sc_omp;		/* message pointer (for multibyte messages) */
	size_t	sc_omlen;
	u_char	sc_imess[ESP_MSGLEN_MAX + 1];
	u_char *sc_imp;		/* message pointer (for multibyte messages) */
	size_t	sc_imlen;

	/* hardware/openprom stuff */
	int	sc_node;			/* PROM node ID */
	int	sc_freq;			/* Freq in HZ */
	int	sc_pri;				/* SBUS priority */
	int	sc_id;				/* our scsi id */
	int	sc_rev;				/* esp revision */
	int	sc_minsync;			/* minimum sync period / 4 */
};

/* values for sc_state */
#define ESPS_IDLE		0x01	/* waiting for something to do */
#define ESPS_EXPECTDISC		0x02	/* a disconnect is going to happen */
#define ESPS_SELECTING		0x03	/* SCSI command is arbiting  */
#define ESPS_RESELECTED		0x04	/* has been reselected */
#define ESPS_NEXUS		0x05	/* actively using the SCSI bus */
#define ESPS_DOINGDMA		0x06	/* doing a DMA transaction */
#define ESPS_DOINGMSGOUT	0x07	/* message-out in progress */
#define ESPS_DOINGMSGIN		0x08	/* message-in in progress */
#define ESPS_DOINGSTATUS	0x09	/* status-in in progress */
#define ESPS_SELECTSTOP		0x0a	/* sent first byte of message.. */
#define ESPS_MULTMSGEND		0x0b	/* done sending multibyte msg */

/* values for sc_flags */
#define ESPF_DROP_MSGI	0x01	/* Discard all msgs (parity err detected) */
#define ESPF_MAYDISCON	0x02	/* disconnection allowed */
#define ESPF_DONE	0x04	/* finished transaction */
#define ESPF_SYNCHNEGO	0x08	/* Synch negotiation in progress. */

/* values for sc_msgout */
#define SEND_DEV_RESET		0x01
#define SEND_PARITY_ERROR	0x02
#define SEND_ABORT		0x04
#define SEND_REJECT		0x08
#define SEND_INIT_DET_ERR	0x10
#define SEND_IDENTIFY  		0x20
#define SEND_SDTR		0x40

#define ST_MASK			0x3e	/* bits 0, 6, and 7 are reserved */

/* physical phase bits */
#define IOI			0x01
#define CDI			0x02
#define MSGI			0x04

/* values for sc_phase (where information transfer happens) */
#define DATA_OUT_PHASE		(0)
#define DATA_IN_PHASE		(IOI)
#define COMMAND_PHASE		(CDI)
#define STATUS_PHASE		(CDI|IOI)
#define MESSAGE_OUT_PHASE	(MSGI|CDI)
#define MESSAGE_IN_PHASE	(MSGI|CDI|IOI)

#define PHASE_MASK		(MSGI|CDI|IOI)

#define TARGETNAME(ecb) \
	((struct device *)(ecb)->xs->sc_link->adapter_softc)->dv_xname

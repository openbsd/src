/*	$NetBSD: espvar.h,v 1.5 1995/08/18 10:09:57 pk Exp $ */

/*
 * Copyright (c) 1994 Peter Galbavy.  All rights reserved.
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
 *	This product includes software developed by Peter Galbavy.
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

#define ESP_SYNC_REQ_ACK_OFS 	0

#define ESP_DEBUG 0

#define FREQTOCCF(freq)	(((freq + 4) / 5))
#define	ESP_DEF_TIMEOUT 153

/* esp revisions */
#define	ESP100		0x01
#define	ESP100A		0x02
#define	ESP200		0x03

/* Grabbed from Julians SCSI aha-drivers */
#ifdef	DDB
int	Debugger();
#else	DDB
#define	Debugger() panic("should call debugger here (esp.c)")
#endif	DDB

typedef caddr_t physaddr;

struct esp_dma_seg {
	physaddr	addr;
	long		len;
};

extern int delaycount;
#define FUDGE(X)	((X)>>1) 	/* get 1 ms spincount */
#define MINIFUDGE(X)	((X)>>4) 	/* get (approx) 125us spincount */
#define NUM_CONCURRENT	7	/* Only one per target for now */

/* 
 * ECB. Holds additional information for each SCSI command Comments: We
 * need a separate scsi command block because we may need to overwrite it
 * with a request sense command.  Basicly, we refrain from fiddling with
 * the scsi_xfer struct (except do the expected updating of return values).
 * We'll generally update: xs->{flags,resid,error,sense,status} and
 * occasionally xs->retries.
 */
struct ecb {
	TAILQ_ENTRY(ecb) chain;
	struct scsi_xfer *xs;	/* SCSI xfer ctrl block from above */
	int		flags;	/* Status */
#define ECB_FREE	0x00
#define ECB_ACTIVE	0x01
#define ECB_DONE	0x04
#define ECB_CHKSENSE	0x08
	struct scsi_generic cmd;  /* SCSI command block */
	int	 clen;
	char	*daddr;		/* Saved data pointer */
	int	 dleft;		/* Residue */
	u_char 	 stat;		/* SCSI status byte */
};

/* 
 * Some info about each (possible) target on the SCSI bus.  This should 
 * probably have been a "per target+lunit" structure, but we'll leave it at 
 * this for now.  Is there a way to reliably hook it up to sc->fordriver??
 */
struct esp_tinfo {
	int	cmds;		/* #commands processed */
	int	dconns;		/* #disconnects */
	int	touts;		/* #timeouts */
	int	perrs;		/* #parity errors */
	int	senses;		/* #request sense commands sent */
	ushort	lubusy;		/* What local units/subr. are busy? */
	u_char  flags;
#define NEED_TO_RESET	0x01	/* Should send a BUS_DEV_RESET */
#define DO_NEGOTIATE	0x02	/* (Re)Negotiate synchronous options */
#define TARGET_BUSY	0x04	/* Target is busy, i.e. cmd in progress */
#define T_XXX		0x08	/* Target is XXX */
	u_char  period;		/* Period suggestion */
	u_char  offset;		/* Offset suggestion */
} tinfo_t;

/* Register a linenumber (for debugging) */
#define LOGLINE(p)

#define ESP_SHOWECBS	0x01
#define ESP_SHOWINTS	0x02
#define ESP_SHOWCMDS	0x04
#define ESP_SHOWMISC	0x08
#define ESP_SHOWTRAC	0x10
#define ESP_SHOWSTART	0x20
#define ESP_SHOWPHASE	0x40
#define ESP_SHOWDMA	0x80

#ifdef ESP_DEBUG
extern int esp_debug;
#define ESP_ECBS(str)  do {if (esp_debug & ESP_SHOWECBS) printf str;} while (0)
#define ESP_MISC(str)  do {if (esp_debug & ESP_SHOWMISC) printf str;} while (0)
#define ESP_INTS(str)  do {if (esp_debug & ESP_SHOWINTS) printf str;} while (0)
#define ESP_TRACE(str) do {if (esp_debug & ESP_SHOWTRAC) printf str;} while (0)
#define ESP_CMDS(str)  do {if (esp_debug & ESP_SHOWCMDS) printf str;} while (0)
#define ESP_START(str) do {if (esp_debug & ESP_SHOWSTART) printf str;}while (0)
#define ESP_PHASE(str) do {if (esp_debug & ESP_SHOWPHASE) printf str;}while (0)
#define ESP_DMA(str)   do {if (esp_debug & ESP_SHOWDMA) printf str;}while (0)
#else
#define ESP_ECBS(str)
#define ESP_MISC(str)
#define ESP_INTS(str)
#define ESP_TRACE(str)
#define ESP_CMDS(str)
#define ESP_START(str)
#define ESP_PHASE(str)
#define ESP_DMA(str)
#endif

#define ESP_MAX_MSG_LEN 8

struct esp_softc {
	struct device sc_dev;			/* us as a device */
	struct sbusdev sc_sd;			/* sbus device */
	struct intrhand sc_ih;			/* intr handler */
	struct evcnt sc_intrcnt;		/* intr count */
	struct scsi_link sc_link;		/* scsi lint struct */
	volatile caddr_t sc_reg;		/* the registers */
	struct dma_softc *sc_dma;		/* pointer to my dma */

	/* register defaults */
	u_char	sc_cfg1;			/* Config 1 */
	u_char	sc_cfg2;			/* Config 2, not ESP100 */
	u_char	sc_cfg3;			/* Config 3, only ESP200 */
	u_char	sc_ccf;				/* Clock Conversion */
	u_char	sc_timeout;

	/* register copies, see espreadregs() */
	u_char	sc_espintr;
	u_char	sc_espstat;
	u_char	sc_espstep;
	u_char	sc_espfflags;

	/* Lists of command blocks */
	TAILQ_HEAD(ecb_list, ecb) free_list,
				  ready_list,
				  nexus_list;

	struct ecb *sc_nexus;			/* current command */
	struct ecb sc_ecb[8];			/* one per target */
	struct esp_tinfo sc_tinfo[8];

	/* Data about the current nexus (updated for every cmd switch) */
	caddr_t	sc_dp;				/* Current data pointer */
	ssize_t	sc_dleft;			/* Data left to transfer */

	/* Adapter state */
	int	sc_phase;		/* Copy of what bus phase we are in */
	int	sc_prevphase;		/* Copy of what bus phase we were in */
	u_char	sc_state;		/* State applicable to the adapter */
	u_char	sc_flags;
	u_char	sc_selid;

	/* Message stuff */
	char	sc_msgpriq;	/* One or more messages to send (encoded) */
	char	sc_msgout;	/* What message is on its way out? */
	char	sc_omess[ESP_MAX_MSG_LEN];
	caddr_t	sc_omp;	/* Message pointer (for multibyte messages) */
	size_t	sc_omlen;
	char	sc_imess[ESP_MAX_MSG_LEN + 1];
	caddr_t	sc_imp;	/* Message pointer (for multibyte messages) */
	size_t	sc_imlen;

	/* hardware/openprom stuff */
	int sc_node;				/* PROM node ID */
	int sc_freq;				/* Freq in HZ */
	int sc_pri;				/* SBUS priority */
	int sc_id;				/* our scsi id */
	int sc_rev;				/* esp revision */
	int sc_minsync;				/* minimum sync period / 4 */
};

/* values for sc_state */
#define ESP_IDLE	0x01	/* waiting for something to do */
#define ESP_TMP_UNAVAIL	0x02	/* Don't accept SCSI commands */
#define ESP_SELECTING	0x03	/* SCSI command is arbiting  */
#define ESP_RESELECTED	0x04	/* Has been reselected */
#define ESP_HASNEXUS	0x05	/* Actively using the SCSI bus */
#define ESP_CLEANING	0x06

/* values for sc_flags */
#define ESP_DROP_MSGI	0x01	/* Discard all msgs (parity err detected) */
#define ESP_DOINGDMA	0x02	/* The FIFO data path is active! */
#define ESP_BUSFREE_OK	0x04	/* Bus free phase is OK. */
#define ESP_SYNCHNEGO	0x08	/* Synch negotiation in progress. */
#define ESP_BLOCKED	0x10	/* Don't schedule new scsi bus operations */

/* values for sc_msgout */
#define SEND_DEV_RESET		0x01
#define SEND_PARITY_ERROR	0x02
#define SEND_ABORT		0x04
#define SEND_REJECT		0x08
#define SEND_INIT_DET_ERR	0x10
#define SEND_IDENTIFY  		0x20
#define SEND_SDTR		0x40

/*
 * Generic SCSI messages. For now we reject most of them.
 */
/* Messages (1 byte) */		     /* I/T M(andatory) or (O)ptional */
#define MSG_CMDCOMPLETE		0x00 /* M/M */
#define MSG_EXTENDED		0x01 /* O/O */
#define MSG_SAVEDATAPOINTER	0x02 /* O/O */
#define MSG_RESTOREPOINTERS	0x03 /* O/O */
#define MSG_DISCONNECT		0x04 /* O/O */
#define MSG_INITIATOR_DET_ERR	0x05 /* M/M */
#define MSG_ABORT		0x06 /* O/M */
#define MSG_MESSAGE_REJECT	0x07 /* M/M */
#define MSG_NOOP		0x08 /* M/M */
#define MSG_PARITY_ERR		0x09 /* M/M */
#define MSG_LINK_CMD_COMPLETE	0x0a /* O/O */
#define MSG_LINK_CMD_COMPLETEF	0x0b /* O/O */
#define MSG_BUS_DEV_RESET	0x0c /* O/M */
#define MSG_ABORT_TAG		0x0d /* O/O */
#define MSG_CLEAR_QUEUE		0x0e /* O/O */
#define MSG_INIT_RECOVERY	0x0f /* O/O */
#define MSG_REL_RECOVERY	0x10 /* O/O */
#define MSG_TERM_IO_PROC	0x11 /* O/O */

/* Messages (2 byte) */
#define MSG_SIMPLE_Q_TAG	0x20 /* O/O */
#define MSG_HEAD_OF_Q_TAG	0x21 /* O/O */
#define MSG_ORDERED_Q_TAG	0x22 /* O/O */
#define MSG_IGN_WIDE_RESIDUE	0x23 /* O/O */

/* Identify message */
#define ESP_MSG_IDENTIFY(lun)	(0x80|(lun & 0x7))	/* XXX 0xc0=selection on */
#define ESP_MSG_ISIDENT(m)	((m) & 0x80)

/* Extended messages (opcode) */
#define MSG_EXT_SDTR		0x01

/* SCSI Status codes */
#define ST_GOOD			0x00
#define ST_CHKCOND		0x02
#define ST_CONDMET		0x04
#define ST_BUSY			0x08
#define ST_INTERMED		0x10
#define ST_INTERMED_CONDMET	0x14
#define ST_RESERVATION_CONFLICT	0x18
#define ST_CMD_TERM		0x22
#define ST_QUEUE_FULL		0x28

#define ST_MASK			0x3e /* bit 0,6,7 is reserved */

/* phase bits */
#define IOI			0x01
#define CDI			0x02
#define MSGI			0x04

/* Information transfer phases */
#define DATA_OUT_PHASE		(0)
#define DATA_IN_PHASE		(IOI)
#define COMMAND_PHASE		(CDI)
#define STATUS_PHASE		(CDI|IOI)
#define MESSAGE_OUT_PHASE	(MSGI|CDI)
#define MESSAGE_IN_PHASE	(MSGI|CDI|IOI)

#define PHASE_MASK		(MSGI|CDI|IOI)

/* Some pseudo phases for getphase()*/
#define BUSFREE_PHASE		0x100	/* Re/Selection no longer valid */
#define INVALID_PHASE		0x101	/* Re/Selection valid, but no REQ yet */
#define PSEUDO_PHASE		0x100	/* "pseudo" bit */

#if ESP_DEBUG > 1
#define	ESPCMD(sc, cmd)		printf("cmd:0x%02x ", sc->sc_reg[ESP_CMD] = cmd)
#else
#define	ESPCMD(sc, cmd)		sc->sc_reg[ESP_CMD] = cmd
#endif

#define SAME_ESP(sc, bp, ca) \
	((bp->val[0] == ca->ca_slot && bp->val[1] == ca->ca_offset) || \
	 (bp->val[0] == -1 && bp->val[1] == sc->sc_dev.dv_unit))

#define TARGETNAME(ecb) \
	((struct device *)ecb->xs->sc_link->adapter_softc)->dv_xname

/* DMA macros for ESP */
#define	DMA_ENINTR(r)		((r->enintr)(r))
#define	DMA_ISINTR(r)		((r->isintr)(r))
#define	DMA_RESET(r)		((r->reset)(r))
#define	DMA_START(a, b, c, d)	((a->start)(a, b, c, d))
#define	DMA_INTR(r)		((r->intr)(r))

#define DMA_DRAIN(sc)	if (sc->sc_rev < DMAREV_2) { \
				DMACSR(sc) |= D_DRAIN; \
				DMAWAIT1(sc); \
			}

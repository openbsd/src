/*	$OpenBSD: siopvar.h,v 1.2 1998/12/15 05:52:31 smurph Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)siopvar.h	7.1 (Berkeley) 5/8/90
 */
#ifndef _SIOPVAR_H_
#define _SIOPVAR_H_

/*
 * The largest single request will be MAXPHYS bytes which will require
 * at most MAXPHYS/NBPG+1 chain elements to describe, i.e. if none of
 * the buffer pages are physically contiguous (MAXPHYS/NBPG) and the
 * buffer is not page aligned (+1).
 */
#define	DMAMAXIO	(MAXPHYS/NBPG+1)

/*
 * Data Structure for SCRIPTS program
 */
struct siop_ds {
/*00*/	long	scsi_addr;		/* SCSI ID & sync */
/*04*/	long	idlen;			/* Identify message */
/*08*/	char	*idbuf;
/*0c*/	long	cmdlen;			/* SCSI command */
/*10*/	char	*cmdbuf;
/*14*/	long	stslen;			/* Status */
/*18*/	char	*stsbuf;
/*1c*/	long	msglen;			/* Message */
/*20*/	char	*msgbuf;
/*24*/	long	msginlen;		/* Message in */
/*28*/	char	*msginbuf;
/*2c*/	long	extmsglen;		/* Extended message in */
/*30*/	char	*extmsgbuf;
/*34*/	long	synmsglen;		/* Sync transfer request */
/*38*/	char	*synmsgbuf;
	struct {
/*3c*/		long datalen;
/*40*/		char *databuf;
	} chain[DMAMAXIO];
};

/*
 * ACB. Holds additional information for each SCSI command Comments: We
 * need a separate scsi command block because we may need to overwrite it
 * with a request sense command.  Basicly, we refrain from fiddling with
 * the scsi_xfer struct (except do the expected updating of return values).
 * We'll generally update: xs->{flags,resid,error,sense,status} and
 * occasionally xs->retries.
 */
struct siop_acb {
/*00*/	TAILQ_ENTRY(siop_acb) chain;
/*08*/	struct scsi_xfer *xs;	/* SCSI xfer ctrl block from above */
/*0c*/	int		flags;	/* Status */
#define ACB_FREE	0x00
#define ACB_ACTIVE	0x01
#define ACB_DONE	0x04
#define ACB_CHKSENSE	0x08
/*10*/	struct scsi_generic cmd;  /* SCSI command block */
/*1c*/	struct siop_ds ds;
/*a0*/	void	*iob_buf;
/*a4*/	u_long	iob_curbuf;
/*a8*/	u_long	iob_len, iob_curlen;
/*b0*/	u_char	msgout[6];
/*b6*/	u_char	msg[6];
/*bc*/	u_char	stat[1];
/*bd*/	u_char	status;
/*be*/	u_char	dummy[2];
/*c0*/	int	 clen;
/*c4*/	char	*daddr;		/* Saved data pointer */
/*c8*/	int	 dleft;		/* Residue */
};

/*
 * Some info about each (possible) target on the SCSI bus.  This should
 * probably have been a "per target+lunit" structure, but we'll leave it at
 * this for now.  Is there a way to reliably hook it up to sc->fordriver??
 */
struct siop_tinfo {
	int	cmds;		/* #commands processed */
	int	dconns;		/* #disconnects */
	int	touts;		/* #timeouts */
	int	perrs;		/* #parity errors */
	int	senses;		/* #request sense commands sent */
	ushort	lubusy;		/* What local units/subr. are busy? */
	u_char  flags;
	u_char  period;		/* Period suggestion */
	u_char  offset;		/* Offset suggestion */
} tinfo_t;

struct	siop_softc {
	struct	device sc_dev;
	struct	intrhand sc_ih;
	struct	evcnt sc_intrcnt;

	u_char	sc_istat;
	u_char	sc_dstat;
	u_char	sc_sstat0;
	u_char	sc_sstat1;
	u_long	sc_intcode;
	struct	scsi_link sc_link;	/* proto for sub devices */
	u_long	sc_scriptspa;		/* physical address of scripts */
	siop_regmap_p	sc_siopp;	/* the SIOP */
	u_long	sc_active;		/* number of active I/O's */

	/* Lists of command blocks */
	TAILQ_HEAD(acb_list, siop_acb) free_list,
				       ready_list,
				       nexus_list;

	struct siop_acb *sc_nexus;	/* current command */
#define SIOP_NACB 8
	struct siop_acb *sc_acb;	/* the real command blocks */
	struct siop_tinfo sc_tinfo[8];

	u_short	sc_clock_freq;
	u_char	sc_dcntl;
	u_char	sc_ctest0;
	u_char	sc_ctest7;
	u_short sc_tcp[4];
	u_char	sc_flags;
	u_char	sc_sien;
	u_char	sc_dien;
	u_char  sc_minsync;
	/* one for each target */
	struct syncpar {
		u_char state;
		u_char sxfer;
		u_char sbcl;
	} sc_sync[8];
};

/* sc_flags */
#define	SIOP_INTSOFF	0x80	/* Interrupts turned off */
#define	SIOP_INTDEFER	0x40	/* Level 6 interrupt has been deferred */
#define	SIOP_ALIVE	0x01	/* controller initialized */
#define SIOP_SELECTED	0x04	/* bus is in selected state. Needed for
				   correct abort procedure. */

/* sync states */
#define SYNC_START	0	/* no sync handshake started */
#define SYNC_SENT	1	/* we sent sync request, no answer yet */
#define SYNC_DONE	2	/* target accepted our (or inferior) settings,
				   or it rejected the request and we stay async */

#define	MSG_CMD_COMPLETE	0x00
#define MSG_EXT_MESSAGE		0x01
#define	MSG_SAVE_DATA_PTR	0x02
#define	MSG_RESTORE_PTR		0x03
#define	MSG_DISCONNECT		0x04
#define	MSG_INIT_DETECT_ERROR	0x05
#define	MSG_ABORT		0x06
#define	MSG_REJECT		0x07
#define	MSG_NOOP		0x08
#define	MSG_PARITY_ERROR	0x09
#define	MSG_BUS_DEVICE_RESET	0x0C
#define	MSG_IDENTIFY		0x80
#define	MSG_IDENTIFY_DR		0xc0	/* (disconnect/reconnect allowed) */
#define	MSG_SYNC_REQ		0x01

#define	STS_CHECKCOND	0x02	/* Check Condition (ie., read sense) */
#define	STS_CONDMET	0x04	/* Condition Met (ie., search worked) */
#define	STS_BUSY	0x08
#define	STS_INTERMED	0x10	/* Intermediate status sent */
#define	STS_EXT		0x80	/* Extended status valid */

void siop_minphys __P((struct buf *bp));
int siop_scsicmd __P((struct scsi_xfer *));

#endif /* _SIOPVAR_H */

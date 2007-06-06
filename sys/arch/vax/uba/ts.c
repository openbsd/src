/*	$OpenBSD: ts.c,v 1.17 2007/06/06 17:15:13 deraadt Exp $ */
/*	$NetBSD: ts.c,v 1.11 1997/01/11 11:34:43 ragge Exp $ */

/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	@(#)tmscp.c	7.16 (Berkeley) 5/9/91
 */

/*
 * sccsid = "@(#)tmscp.c	1.24	(ULTRIX)	1/21/86";
 */

/************************************************************************
 *									*
 *	  Licensed from Digital Equipment Corporation			*
 *			 Copyright (c)					*
 *		 Digital Equipment Corporation				*
 *		     Maynard, Massachusetts				*
 *			   1985, 1986					*
 *		      All rights reserved.				*
 *									*
 *	  The Information in this software is subject to change		*
 *   without notice and should not be construed as a commitment		*
 *   by	 Digital  Equipment  Corporation.   Digital   makes  no		*
 *   representations about the suitability of this software for		*
 *   any purpose.  It is supplied "As Is" without expressed  or		*
 *   implied  warranty.							*
 *									*
 *	  If the Regents of the University of California or its		*
 *   licensees modify the software in a manner creating			*
 *   diriviative copyright rights, appropriate copyright		*
 *   legends may be placed on  the drivative work in addition		*
 *   to that set forth above.						*
 *									*
 ************************************************************************/

/*
 * TSV05/TS05 device driver, written by Bertram Barth.
 *
 * should be TS11 compatible (untested)
 */

#define TS11_COMPAT	/* don't use extended features provided by TS05 */

#ifdef	NEED_18BIT
#define TS_UBAFLAGS	UBA_NEED16
#else
#define TS_UBAFLAGS	0
#endif

#define ENABLE_ESS
#define ENABLE_END

#define ENABLE_EAI	/* enable Attention-Interrupts */
#undef	ENABLE_EAI

#define ENABLE_ERI	/* Enable Release Buffer Interrupts */
#undef	ENABLE_ERI

#ifdef DEBUG
int tsdebug = 1;
# define debug(x)	if (tsdebug > 0) {DELAY(2000); printf x; DELAY(3000);}
# define debug10(x)	if (tsdebug > 9) printf x
#else
# define debug(x)	/* just ignore it */
# define debug10(x)	/* just ignore it */
#endif

#ifdef TRACE
int tstrace = 1;
# define trace(x)	if (tstrace > 0) {DELAY(2000); printf x; DELAY(3000);}
#else
# define trace(x)	/* just ignore it */
#endif

/*
 * TODO: most :-)
 *
 * include uba-mapping into tsinit();
 * merge tsinit(), tsreset() and tsprobe();
 * complete tsintr();
 * add proper error/status messages
 * make messages appear where they are intended to.
 * check for termination-classes and appropriate actions.
 * check if flags like CVC and ATTN should be used.
 * look for correct handling of attentions.
 * check for correct usage of retry-commands.
 * ...
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/syslog.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/uio.h>
#include <sys/proc.h>

#include <machine/pte.h>
#include <machine/sid.h>
#include <machine/cpu.h>
#include <machine/mtpr.h>

#include <vax/uba/ubareg.h>
#include <vax/uba/ubavar.h>

#include <vax/uba/tsreg.h>

#include "ts.h"

/*
 * ts command packets and communication area (per controller)
 */
struct	ts {
	struct	tsdevice *reg;		/* address of i/o-registers */
	struct	tscmd	  cmd;		/* command packet(s) */
	struct	tsmsg	  msg;		/* message packet(s) */
} ts[NTS];

/*
 * Software status, per controller.
 * also status per tape-unit, since only one unit per controller
 * (thus we have no struct ts_info)
 */
struct	ts_softc {
	struct	device sc_dev;		/* Autoconf ... */
	struct	uba_unit sc_unit;	/* Struct common for UBA to talk */
	struct	ts *sc_ts;		/* Unibus address of uda struct */
	short	sc_mapped;		/* Unibus map allocated ? */
	int	sc_ubainfo;		/* Unibus mapping info */
	short	sc_state;		/* see below: ST_xxx */
	short	sc_flags;		/* see below: FL_xxx */
	short	sc_lcmd;		/* last command word */
	short	sc_rtc;			/* retry count for lcmd */
	short	sc_lssr;		/* last status register */
	short	sc_lmsgh;		/* last message header */
	short	sc_lxst0;		/* last status word */
	short	sc_cmdf;		/* command flags (ack,cvc,ie) */
	short	sc_openf;		/* lock against multiple opens */
	short	sc_liowf;		/* last operation was write */
	int	sc_micro;		/* microcode revision */
	int	sc_ivec;		/* interrupt vector address */
	short	sc_ipl;			/* interrupt priority, Q-bus */
};

void	tsintr(int);
int	tsinit(struct ts_softc *);
void	tscommand(dev_t, int, int);
int	tsstatus(int);
int	tsexec(int, int);
int	tsstart(struct ts_softc *, struct buf *);
int	tswchar(int);
void	tsreset(int);
void	tsxstatus(struct tsmsg *);
int	tsmatch(struct device *, void *, void *);
void	tsattach(struct device *, struct device *, void *);
void	tsstrategy(struct buf *);

int	tsopen(dev_t, int, int, struct proc *);
int	tsclose(dev_t, int, int, struct proc *);
int	tsioctl(dev_t, u_long, caddr_t, int, struct proc *);
int	tsread(dev_t, struct uio *);
int	tswrite(dev_t, struct uio *);
int	tsdump(dev_t, daddr64_t, caddr_t, size_t);

struct	cfdriver ts_cd = {
	NULL, "ts", DV_DULL
};

struct	cfattach ts_ca = {
	sizeof(struct ts_softc), tsmatch, tsattach
};


#define ST_INVALID	0	/* uninitialized, before probe */
#define ST_PROBE	1	/* during tsprobe(), not used */
#define ST_SLAVE	2	/* in tsslave(), init almost complete */
#define ST_ATTACH	3	/* during tsattach(), not used */
#define ST_INITIALIZED	4	/* init completed, set by tsintr() */
#define ST_RUNNING	5
#define ST_IDLE		6
#define ST_BUSY		7

/* Bits in minor device */ 
#define TS_UNIT(dev)	(minor(dev)&03)
#define TS_HIDENSITY	010

#define TS_PRI	LOG_INFO


/*
 * Since we don't have credits and thus only one operation per time,
 * we don't have and don't need queues like MSCP/TMSCP use them.
 * Per controller we only need one internal buffer for ioctls and 
 * two pointers to buffers to simulate similar behaviour ...
 */
struct buf	 ts_cbuf[NTS];		/* internal cmd buffer (for ioctls) */
struct buf	*ts_wtab[NTS];		/* dummy I/O wait queue */
#define b_ubinfo	b_resid		/* Unibus mapping info, per buffer */

/*----------------------------------------------------------------------*/

/*
 * Initialize a TS device. Set up UBA mapping registers,
 * initialize data structures, what else ???
 */
int 
tsinit (sc)
	struct ts_softc *sc;
{
	volatile struct tsdevice *tsregs;
	int unit = sc->sc_dev.dv_unit;
	struct uba_unit *uu;
	
	uu = &sc->sc_unit;
	tsregs = (struct tsdevice *)ts[unit].reg;
	if (sc->sc_mapped == 0) {
		/*
		 * Map the communications area and command and message
		 * buffer into Unibus address space.
		 */
		sc->sc_ubainfo = uballoc((struct uba_softc *)
		    sc->sc_dev.dv_parent,
		    (caddr_t)&ts[unit], sizeof (struct ts), TS_UBAFLAGS);
		sc->sc_ts = (struct ts *)(UBAI_ADDR(sc->sc_ubainfo));
		sc->sc_mapped = 1;
	}

	/*
	 * bertram: start hardware initialization ??????
	 */
	
	/* tsreset(unit); */

	return (1);
}

/*
 * send a command using the default command-buffer for this unit/controller.
 * If a command-word is given, then assemble a one-word command.
 * other words in command-buffer are unchanged and must thus be initialized
 * before calling this function.
 */
int 
tsexec (ctlr, cmd)
	int ctlr;
	int cmd;
{
	register struct ts_softc *sc = ts_cd.cd_devs[ctlr];
	register struct tscmd *tscmdp = &ts[ctlr].cmd;
	register long tscmdma = (long)&sc->sc_ts->cmd;	/* mapped address */
	volatile struct tsdevice *tsreg = ts[ctlr].reg;
	volatile char *dbx = ((char *)tsreg) + 3;
	volatile short sr;

	sc->sc_cmdf |= TS_CF_ACK | TS_CF_IE;
	tscmdp->cmdr = sc->sc_cmdf | cmd;
	tscmdp->cmdr = TS_CF_ACK | TS_CF_IE | sc->sc_cmdf | cmd;
	sc->sc_cmdf = 0;		/* XXX */

#ifdef DEBUG
	switch (tscmdp->cmdr & TS_CF_CMASK) {
	case TS_CMD_RNF:	cmdName = "Read Next (Forward)";	break;
	case TS_CMD_RPR:	cmdName = "Read Previous (Reverse)";	break;
	case TS_CMD_WCHAR:	cmdName = "Write Characteristics";	break;
	case TS_CMD_WD:		cmdName = "Write Data (Next)";		break;
	case TS_CMD_WDR:	cmdName = "Write Data (Retry)";		break;
	case TS_CMD_SRF:	cmdName = "Space Records Forward";	break;
	case TS_CMD_SRR:	cmdName = "Space Records Reverse";	break;
	case TS_CMD_STMF:	cmdName = "Skip Tape Marks Forward";	break;
	case TS_CMD_STMR:	cmdName = "Skip Tape Marks Reverse";	break;
	case TS_CMD_RWND:	cmdName = "Rewind";			break;
	case TS_CMD_WTM:	cmdName = "Write Tape Mark";		break;
	case TS_CMD_WTMR:	cmdName = "Write Tape Mark (Retry)";	break;
	case TS_CMD_STAT:	cmdName = "Get Status (END)";		break;
	default:		cmdName = "Unexpected Command";		break;
	}
#endif

	sr = tsreg->tssr;
	if ((sr & TS_SSR) == 0) {	/* subsystem NOT ready */
		printf ("%s%d: subsystem not ready [%x]\n", 
		    sc->sc_dev.dv_xname, sr);
		return (-1);
	}
	dbx = ((char *)tsreg) + 3;	/* dbx is located at the fourth byte */
	*dbx = (tscmdma >> 18) & 0x0F;	/* load bits 18-21 into dbx */ 

	/* possible race-condition with ATTN !!! */

	sr = tsreg->tssr;
	if ((sr & TS_RMR) != 0) {	/* Register modification Refused */
		printf ("ts: error writing TSDBX\n");
		return (-1);
	}
	/* now load bits 15-2 at pos 15-2 and bits 17,16 at pos 1,0 of TSDB */
	tsreg->tsdb = (tscmdma & 0xfffc) | ((tscmdma >> 16) & 0x03);

	/*
	 * wait for SSR or RMR to show up
	 */
	sr = tsreg->tssr;
	if ((sr & TS_SSR) != 0) {	/* something went wrong .. */
		if (sr & TS_RMR) {
			printf ("ts: error writing TSDB (RMR)\n");
			return (-1);
		}
		if (sr & TS_NXM) {
			printf ("ts: error writing TSDB (NXM)\n");
			return (-1);
		}
		printf ("ts: error 0x%x while writing TSDB\n", sr);
		tsstatus (sr);
		return (-1);
	}

	return (0);	 /* completed successfully */
}

/*  
 * Execute a (ioctl) command on the tape drive a specified number of times.
 * This routine sets up a buffer and calls the strategy routine which
 * issues the command to the controller.
 */
void
tscommand (dev, cmd, count)
	register dev_t dev;
	int cmd;
	int count;
{
	register struct buf *bp;
	register int s;	 

	trace (("tscommand (%d, %x, %d)\n", TS_UNIT(dev), cmd, count));

	s = splbio();
	bp = &ts_cbuf[TS_UNIT(dev)];

	while (bp->b_flags & B_BUSY) {
		/*
		 * This special check is because B_BUSY never
		 * gets cleared in the non-waiting rewind case. ???
		 */
		if (bp->b_bcount == 0 && (bp->b_flags & B_DONE))
			break;
		bp->b_flags |= B_WANTED;
		tsleep ((caddr_t)bp, PRIBIO, "tscommand", 0);
		/* check MOT-flag !!! */
	}
	bp->b_flags = B_BUSY | B_READ;

	splx(s);

	/*
	 * Load the buffer.  The b_count field gets used to hold the command
	 * count.  the b_resid field gets used to hold the command mneumonic.
	 * These 2 fields are "known" to be "safe" to use for this purpose.
	 * (Most other drivers also use these fields in this way.)
	 */
	bp->b_dev = dev;
	bp->b_bcount = count;
	bp->b_resid = cmd;
	bp->b_blkno = 0;
	tsstrategy (bp);
	/*
	 * In case of rewind from close, don't wait.
	 * This is the only case where count can be 0.
	 */
	if (count == 0) {
		debug (("tscommand: direct return, no biowait.\n"));
		return;
	}
	debug (("tscommand: calling biowait ...\n"));
	biowait (bp);
	if (bp->b_flags & B_WANTED)
		wakeup ((caddr_t)bp);
	bp->b_flags &= B_ERROR;
}

/*
 * Start an I/O operation on TS05 controller
 */
int
tsstart (sc, bp)
	register struct ts_softc *sc;
	register struct buf *bp;
{
	int ctlr = sc->sc_dev.dv_unit;
	volatile struct tsdevice *tsreg = ts[ctlr].reg;
	register struct tscmd *tscmdp = &ts[ctlr].cmd;
	register struct buf *dp;
	volatile int i, itmp;
	int ioctl;
	int cmd;
	int s;

	if ((dp = ts_wtab[ctlr]) != NULL) {
		/*
		 * There's already a command pending ...
		 * Either we are called by tsintr or we have missed
		 * something important (race condition).
		 */

		/* bertram: ubarelse ??? */
		ts_wtab[ctlr] = NULL;
		dp->b_flags |= B_ERROR;
		s = splbio();
		biodone (dp);
		splx(s);

		if (tsreg->tssr & TS_SC) {	/* Special Condition; Error */
			log (TS_PRI, "%s: tssr 0x%x, state %d\n",
				sc->sc_dev.dv_xname, tsreg->tssr, sc->sc_state);
			tsinit (sc);
			return (-1);
		}
		/* XXX */
	}

	/*
	 * Check if command is an ioctl or not (ie. read or write).
	 * If it's an ioctl then just set the flags for later use;
	 * For other commands attempt to setup a buffer pointer.
	 */
	if (bp == &ts_cbuf[ctlr]) {
		ioctl = 1;
	} else {
		ioctl = 0;

		/*
		 * now we try to map the buffer into uba map space (???)
		 */
		i = TS_UBAFLAGS;
		switch (vax_cputype) {
		case VAX_8600:
		case VAX_780:
			i |= UBA_CANTWAIT;
			break;
		case VAX_750:
			i |= sc->sc_unit.uu_ubinfo | UBA_CANTWAIT;
			break;
		case VAX_730:
		case VAX_78032:
			i |= UBA_CANTWAIT;
			break;
		default:
			printf ("unsupported cpu %d in tsstart.\n", vax_cputype);
		} /* end switch (vax_cputype) */

		if ((i = ubasetup(sc->sc_dev.dv_parent->dv_unit, bp, i)) == 0) {
			/*
			 * For some reasons which I don't (yet? :) understand,
			 * tmscp.c initiates in this situation a GET-UNIT
			 * command. (Because no data-buffers are necessary?)
			 */
			cmd = TS_CMD_STAT;
			goto do_cmd;
			return (-1);	/* ??? */
		}
#if defined(VAX750)
		if (vax_cputype == VAX_750)
			itmp = i & 0xfffffff;		/* mask off bdp */
		else
#endif
			itmp = i;

		/* XXX */
	}

	/*
	 * If it's an ioctl command, then assemble the command.
	 * The "b_resid" field holds the command-number as defined
	 * in <sys/mtio.h>
	 */
	if (ioctl) {
		switch ((int)bp->b_resid) {
		case MTWEOF:
			cmd = TS_CMD_WTM;
			break;
		case MTFSF:
			cmd = TS_CMD_STMF;
			tscmdp->cw1 = bp->b_bcount;
			break;
		case MTBSF:
			cmd = TS_CMD_STMR; 
			tscmdp->cw1 = bp->b_bcount;
			break;
		case MTFSR:
			cmd = TS_CMD_SRF;
			tscmdp->cw1 = bp->b_bcount;
			break;
		case MTBSR:
			cmd = TS_CMD_SRR;
			tscmdp->cw1 = bp->b_bcount;
			break;
		case MTREW:
			cmd = TS_CMD_RWND;
#ifndef TS11_COMPAT
			if (bp->b_bcount == 0) {
				cmd = TS_CMD_RWII;
			}
#endif
			break;
		case MTOFFL:
			cmd = TS_CMD_RWUL;
			break;
		case MTNOP:
			cmd = TS_CMD_STAT;
			break;
		default:
			printf ("%s: bad ioctl %d\n", sc->sc_dev.dv_xname,
				(int)bp->b_resid);
			/* Need a no-op. get status */
			cmd = TS_CMD_STAT;
		} /* end switch (bp->b_resid) */
	} else {	/* Its a read/write command (not an ioctl) */ 
		tscmdp->cw1 = UBAI_ADDR(i) & 0xffff;
		tscmdp->cw2 = (UBAI_ADDR(i) >> 16) & 0x3f;
		tscmdp->cw3 = bp->b_bcount;

		if (bp->b_flags & B_READ) {
			cmd = TS_CMD_RNF;
		}
		else {
			cmd = TS_CMD_WD;
		}
		bp->b_ubinfo = itmp;			/* save mapping info */
	}

	/*
	 * Move buffer to I/O wait pseudo-queue
	 */
	if (ts_wtab[ctlr]) {
		/*
		 * we are already waiting for something ...
		 * this should not happen, so we have a problem now.
		 * bertram: set error-flag and call biodone() ???
		 */
	}
	ts_wtab[ctlr] = bp;

	/*
	 * Now that the command-buffer is setup, give it to the controller
	 */
do_cmd:
	return (tsexec(ctlr, cmd));
}

/*
 * initialize the controller by sending WRITE CHARACTERISTICS command.
 * contents of command- and message-buffer are assembled during this
 * function. 
 */
int 
tswchar (ctlr)
	int ctlr;
{
	struct ts_softc *sc = ts_cd.cd_devs[ctlr];
	volatile struct tsdevice *tsregs = ts[ctlr].reg;
	volatile struct tscmd *tscmdp = &ts[ctlr].cmd;
	volatile struct tsmsg *tsmsgp = &ts[ctlr].msg;
	volatile unsigned int sr, ma, timeout;

	/*
	 * assemble and send "WRITE CHARACTERISTICS" command
	 */
	ma = (long)tsmsgp;
	if (ma & 0x7FC00001) {	/* address must be even and 22-bit */
		printf ("invalid address 0x%0x for msg-buffer.\n", ma);
		return (-1);
	}

	tsmsgp->hdr = ma & 0xFFFF;		/* low order addr. bits */
	tsmsgp->dfl = (ma >> 16) & 0x003F;	/* high order addr. bits */
	tsmsgp->rbpcr = 16;			/* size of message-buffer */
	tsmsgp->xst0 = 0;			/* chacacteristics mode word */
	tsmsgp->xst1 = 0;			/* control word (ext.feat.) */

#ifdef TS11_COMPAT
	tsmsgp->rbpcr = 14;		/* size of message-buffer */
	tsmsgp->xst0  = 0;		/* chacacteristics mode word */
	tsmsgp->xst1  = 0;		/* control word (ext.feat.) */
#else
	tsmsgp->rbpcr = 16;		/* size of extended message buffer */
	tsmsgp->xst0  = 0;		/* chacacteristics mode word */
	tsmsgp->xst1  = 0;		/* unit-select */
	tsmsgp->xst1 |= TS_WC_HSP;	/* high speed */
#endif

#ifdef ENABLE_ESS
	tsmsgp->xst0 |= TS_WC_ESS;
#ifdef ENABLE_ENB
	tsmsgp->xst0 |= TS_WC_ENB;
#endif
#endif

#ifdef ENABLE_EAI
	tsmsgp->xst0 |= TS_WC_EAI;
#ifdef ENABLE_ERI
	tsmsgp->xst0 |= TS_WC_ERI;
#endif
#endif

	tscmdp->cmdr = TS_CF_ACK | TS_CF_IE | TS_CMD_WCHAR;	/* obsolete */
	tscmdp->cw1  = ma & 0xFFFF;
	tscmdp->cw2  = (ma >> 16) & 0x003F;
	tscmdp->cw3  = 10;		   /* size of charact.-data */

	if (tsexec (ctlr, TS_CMD_WCHAR) < 0) {
		printf ("%s: write characteristics command failed [%x]\n",
		    sc->sc_dev.dv_xname, tsregs->tssr);
		return (-1);
	}

	timeout = 1000;		       /* timeout in 10 seconds */
	do {
		DELAY(10000);
		sr = tsregs->tssr;
		debug10 (("\ttssr: 0x%x\n", sr));
		if (timeout-- > 0) {
			printf ("timeout during initialize.");
			tsstatus (sr);
			return (-1);
		}
	} while ((sr & TS_SSR) == 0);
	tsstatus (sr);

	return (0);
}

/*
 *
 */
void
tsreset(ctlr) 
	int ctlr;
{
	struct ts_softc *sc = ts_cd.cd_devs[ctlr];
	volatile struct tsdevice *tsreg = ts[ctlr].reg;
	volatile unsigned int sr, timeout;

	/*
	 * reset ctlr by writing into TSSR, then write characteristics
	 */
	timeout = 1000;		/* timeout in 10 seconds */	
	tsreg->tssr = 0;			/* start initialization */
	do {
		DELAY(10000);
		sr = tsreg->tssr;
		debug10 (("\ttssr: 0x%x\n", sr));
		if (timeout-- > 0) {
			if (sr != 0)
				printf ("%s: timeout waiting for TS_SSR\n",
					sc->sc_dev.dv_xname);
			tsstatus (sr);
			return;
		}
	} while ((sr & TS_SSR) == 0);	/* wait until subsystem ready */
	tsstatus (sr);

	return;
}

/*
 * probe for device. If found, try to raise an interrupt.
 * XXX - most of this should be done in the attach routine.
 */
int
tsmatch(parent, match, aux)
	struct	device *parent;
	void	*match, *aux;
{
	struct ts_softc *sc = match;
	struct uba_softc *uh = (void *)parent;
	struct uba_attach_args *ua = aux;
	struct tsdevice *tsregs = (struct tsdevice*)ua->ua_addr;
	volatile unsigned int sr, timeout, count;
	int ctlr = sc->sc_dev.dv_unit;

	ts_wtab[ctlr] = NULL;
	sc->sc_ts = &ts[ctlr];
	sc->sc_state = ST_PROBE;
	sc->sc_flags = 0;
	ts[ctlr].reg = (struct tsdevice*)ua->ua_addr;

	/*
	 * Set host-settable interrupt vector.
	 * Assign 0 to the TSSR register to start the ts-device initialization.
	 * The device is not really initialized at this point, this is just to
	 * find out if the device exists. 
	 */
	sc->sc_ivec = (uh->uh_lastiv -= 4);

	count = 0;
again:
	timeout = 1000;		/* timeout in 10 seconds */	
	tsregs->tssr = 0;			/* start initialization */
	do {
		DELAY(10000);
		sr = tsregs->tssr;
		debug10 (("\ttssr-1: 0x%x\n", sr));
		if (timeout-- > 0) {
			if (sr != 0)	/* the device exists !!! */
				printf ("%s: timeout waiting for TS_SSR\n",
					sc->sc_dev.dv_xname);
			tsstatus (sr);
			goto bad;
		}
	} while ((sr & TS_SSR) == 0);	/* wait until subsystem ready */
	tsstatus (sr);

	tswchar (ctlr);		/* write charact. to enable interrupts */
				/* completion of this will raise the intr. */

#ifdef notyet
	sc->sc_ipl = br = qbgetpri();
#else
	sc->sc_ipl = 0x15;
#endif
	return (sizeof (struct tsdevice));

bad:	if (++count < 3)
		goto again;

#ifdef notyet
	splx(s);
#endif
	return (0);
}


/*
 * Try to find a slave (a drive) on the controller.
 * Since there's only one drive per controller there's nothing to do.
 * (we could check the status of the drive (online/offline/...)
 */
void
tsattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ts_softc *sc = (void *)self;
	int ctlr = sc->sc_dev.dv_unit;
	struct tsmsg *tsmsgp = &ts[ctlr].msg;

	trace (("tsslave (%x, %x)\n", ui, reg));
	
	/*
	 * write the characteristics (again)
	 * This will raise an interrupt during ST_SLAVE which indicates
	 * completion of initialization ...
	 */
	sc->sc_state = ST_SLAVE;	/* tsintr() checks this ... */
	if (tswchar (ctlr) < 0) {
		printf ("%s: cannot initialize", sc->sc_dev.dv_xname);
	}
	sc->sc_micro = (tsmsgp->xst2 & TS_SF_MCRL) >> 2;
	printf ("%s: rev %d, extended features %s, transport %s\n",
		sc->sc_dev.dv_xname, sc->sc_micro,
		(tsmsgp->xst2 & TS_SF_EFES ? "enabled" : "disabled"),
		(ts[ctlr].reg->tssr & TS_OFL ? "offline" : "online"));

	tsinit (sc);		/* must be called once, why not here ? */
}


/*
 * TSV05/TS05 interrupt routine
 */
void
tsintr(ctlr)
	int ctlr;
{
	register struct ts_softc *sc = ts_cd.cd_devs[ctlr];
	register struct tsmsg *tsmsgp = &ts[ctlr].msg;
	register struct tscmd *tscmdp = &ts[ctlr].cmd;
	volatile struct tsdevice *tsreg = ts[ctlr].reg;
	struct uba_unit *um = &sc->sc_unit;
	register struct buf *bp;

	unsigned short sr = tsreg->tssr;	/* save TSSR */
	unsigned short mh = tsmsgp->hdr;	/* and msg-header */
		/* clear the message header ??? */

	short ccode = tscmdp->cmdr & TS_CF_CCODE;
	short cmask = tscmdp->cmdr & TS_CF_CMASK;

#ifdef DEBUG
	{
		char bits[64];
		printf ("TSSR: %s, MSG: %x ", bitmask_snprintf(sr,
		    TS_TSSR_BITS, bits, sizeof(bits)), mh);
	}
	switch (tsmsgp->hdr & 0x001F) {
	case 16:	printf ("(End)");	break;
	case 17:	printf ("(Fail)");	break;
	case 18:	printf ("(Error)");	break;
	case 19:	printf ("(Attention)"); break;
	}
#endif

	trace (("  tsintr (%d, %d, %d, %x)\n", uba, vector, level, ctlr));

	if (tsmsgp->xst0 & TS_SF_VCK)
		sc->sc_cmdf |= TS_CF_CVC;

#ifdef QBA				/* copied from uda.c */
	if(vax_cputype == VAX_78032)
		splx(sc->sc_ipl);	/* Qbus interrupt protocol is odd */
#endif

	/*
	 * There are two different things which can (and should) be checked:
	 * the actual (internal) state and the device's result (tssr/msg.hdr)
	 * 
	 * For each state there's only one "normal" interrupt. Anything else
	 * has to be checked more intensively. Thus in a first run according
	 * to the internal state the expected interrupt is checked/handled.
	 *
	 * In a second run the remaining (not yet handled) interrupts are
	 * checked according to the drive's result.
	 */
	switch (sc->sc_state) {

	case ST_INVALID:
		/*
		 * Ignore unsolicited interrupts.
		 */
		log (LOG_WARNING, "%s: stray intr [%x,%x]\n", 
			sc->sc_dev.dv_xname, sr, mh);
		return;

	case ST_SLAVE:
		/*
		 * this interrupt was caused by write-charact. command
		 * issued by tsslave() and indicates the end of the
		 * initialization phase. Just ignore it ...
		 */
		if ((sr & TS_SC) != 0 || (sr & TS_TC) != TS_TC_NORM) {
			printf("%s: problem during init [%x,%x]\n",
				sc->sc_dev.dv_xname, sr, mh);
			/* return here ??? */
			/* break and check the error outside switch ??? */
			break;
		}
		sc->sc_state = ST_RUNNING;
		return;


	case ST_RUNNING:
		/*
		 * Here we expect interrupts indicating the end of
		 * commands or indicating problems.
		 */
		/*
		 * Anything else is handled outside this switch ...
		 */
		break;

	case ST_IDLE:
		break;


	default:
		printf ("%s: unexpected interrupt during state %d [%x,%x]\n", 
			sc->sc_dev.dv_xname, sc->sc_state, sr, mh);
		return;
	}

	/*
	 * now we check the termination class.
	 */
	switch (sr & TS_TC) {

	case TS_TC_NORM:
		/*
		 * Normal termination -- The operation is completed
		 * witout incident.
		 */
		sc->sc_state = ST_IDLE;		/* XXX ??? */
		sc->sc_state = ST_RUNNING;
		sc->sc_liowf = (ccode == TS_CC_WRITE);
		sc->sc_rtc = 0;
		if ((bp = ts_wtab[ctlr]) != NULL) {
			ts_wtab[ctlr] = NULL;	/* pseudo-unlink */

			if (bp != &ts_cbuf[ctlr]) {	/* no ioctl */
				ubarelse((struct uba_softc *)
				    sc->sc_dev.dv_parent,
				    (int *)&bp->b_ubinfo);
#if defined(VAX750)
				if (vax_cputype == VAX_750 &&
				    sc->sc_unit.uu_ubinfo != 0)
					ubarelse((struct uba_softc *)
					    sc->sc_dev.dv_parent,
					    &sc->sc_unit.uu_ubinfo);
					/* XXX */
#endif
			}
			bp->b_resid = tsmsgp->rbpcr;
			debug (("tsintr: biodone(NORM) [%d,%d,%d]\n",
				bp->b_resid, bp->b_bcount, tsmsgp->rbpcr));
			biodone (bp); /* bertram: ioctl ??? */
		}
		return;

	case TS_TC_ATTN:
		/*
		 * Attention condition -- this code indicates that the
		 * drive has undergone a status change, such as going
		 * off-line or coming on-line.
		 * (Without EAI enabled, no Attention interrupts occur.
		 * drive status changes are signaled by the VCK flag.)
		 */
		return;

	case TS_TC_TSA:
		/* 
		 * Tape Status Alert -- A status condition is encountered
		 * that may have significance to the program. Bits of
		 * interest in the extended status registers include
		 * TMK, EOT and RLL.
		 */
		debug (("Tape Status Alert\n"));
		tsxstatus (tsmsgp);
		if (tsmsgp->xst0 & TS_SF_TMK) {
			debug (("Tape Mark detected"));
		}
		if (tsmsgp->xst0 & TS_SF_EOT) {
			debug (("End of Tape"));
		}
		break;

	case TS_TC_FR: 
		/*
		 * Function Reject -- The specified function was not
		 * initiated. Bits of interest include OFL, VCK, BOT,
		 * WLE, ILC and ILA.
		 */
		debug (("Function reject\n")); 
		tsxstatus (tsmsgp);
		if (sr & TS_OFL) {
			printf ("tape is off-line.\n");
			break;
		}
		if (tsmsgp->xst0 & TS_SF_VCK) {
			printf ("Volume check: repeating command ...\n");
			tsexec (ctlr, tscmdp->cmdr);
			return;
		}
		if (tsmsgp->xst0 & TS_SF_BOT) {
			printf ("bottom of tape.\n");
		}
		if (tsmsgp->xst0 & TS_SF_WLE) {
			printf ("Write Lock Error\n");
		}
		break;

	case TS_TC_TPD:
		/*
		 * Recoverable Error -- Tape position is a record beyond
		 * what its position was when the function was initiated.
		 * Suggested recovery procedure is to log the error and
		 * issue the appropriate retry command.
		 */
		debug (("Tape position down\n")); 
		switch (cmask) {
		case TS_CMD_RNF:	/* Read Next (forward) */
			debug (("retry read forward ...\n"));
			sc->sc_rtc = 1;
			tsexec (ctlr, TS_CMD_RPF);
			return;
		case TS_CMD_RPR:	/* Read Previous (Reverse) */
			debug (("retry read reverse ...\n"));
			sc->sc_rtc = 1;
			tsexec (ctlr, TS_CMD_RNR);
			return;
		case TS_CMD_WD:		/* Write Data (Next) */
			debug (("retry write data ...\n"));
			sc->sc_rtc = 1;		
			tsexec (ctlr, TS_CMD_WDR);
			return;
		case TS_CMD_WTM:
			debug (("retry write tape mark ...\n"));
			sc->sc_rtc = 1;
			tsexec (ctlr, TS_CMD_WTMR);
			return;
		default:
			debug (("TPD in command %x\n", cmask));
		}
		break;

	case TS_TC_TNM:
		/*
		 * Recoverable Error -- Tape position has not changed.
		 * Suggested recovery procedure is to log the error and
		 * reissue the original command.
		 */
		printf ("Tape not moved\n"); 
		if (sc->sc_rtc < 3) {
			sc->sc_rtc++;
			/* bertram: log the error !!! */
			printf ("retrying command %x (%d)\n", 
				tscmdp->cmdr, sc->sc_rtc);
			tsexec (ctlr, tscmdp->cmdr);
			return;
		}
		break;

	case TS_TC_TPL:
		/*
		 * Unrecoverable Error -- Tape position has been lost.
		 * No valid recovery procedures exist unless the tape
		 * has labels or sequence numbers.
		 */
		printf ("Tape position lost\n"); 
		break;

	case TS_TC_FCE:
		/*
		 * Fatal subsytem Error -- The subsytem is incapable
		 * of properly performing commands, or at least its
		 * integrity is seriously questionable. Refer to the 
		 * fatal class code field in the TSSR register for
		 * additional information on the type of fatal error.
		 */
		printf ("Fatal Controller Error\n"); 

	default:
		printf ("%s: error 0x%x, resetting controller\n", 
			sc->sc_dev.dv_xname, sr & TS_TC);
		tsreset (ctlr);
	}

	/*
	 * reset controller ?? call tsinit() ???
	 */
	if ((bp = ts_wtab[ctlr]) != NULL) {
		ts_wtab[ctlr] = NULL;		/* pseudo unlink */

		if (bp != &ts_cbuf[ctlr]) 	/* no ioctl */
			ubarelse((struct uba_softc *)sc->sc_dev.dv_parent,
			    (int *)&bp->b_ubinfo);

		if ((sr & TS_TC) != TS_TC_NORM)
			bp->b_flags |= B_ERROR;

		debug (("resid:%d, count:%d, rbpcr:%d\n",
			bp->b_resid, bp->b_bcount, tsmsgp->rbpcr));
		bp->b_resid = tsmsgp->rbpcr; /* XXX */
		debug (("tsintr: biodone(%x)\n", bp->b_flags));
		biodone (bp);
	}
	if ((sr & TS_TC) > TS_TC_FR)
		tsreset (ctlr);

	return;
}


/*
 * Open a ts device and set the unit online.  If the controller is not
 * in the run state, call init to initialize the ts controller first.
 */
int
tsopen (dev, flag, type, p)
	dev_t dev;
	int flag, type;
	struct proc *p;
{
	register struct uba_device *ui;
	register struct uba_ctlr *um;
	register struct ts_softc *sc;
	register int unit = TS_UNIT(dev);
	int s;

	trace (("tsopen (%x, %x)\n", dev, flag));
	
	if (unit >= ts_cd.cd_ndevs)
		return ENXIO;

	sc = ts_cd.cd_devs[unit];
	if (sc == 0)
		return ENXIO;

	if (sc->sc_openf)
		return EBUSY;

	sc->sc_openf = 1;

	s = splbio ();
	if (sc->sc_state < ST_RUNNING) {		/* XXX */
		printf ("%s not running.\n", sc->sc_dev.dv_xname);
		splx(s);
		sc->sc_openf = 0;
		return (ENXIO);
	}
	splx(s);

	/*
	 * check if transport is really online.
	 * (without attention-interrupts enabled, we really don't know
	 * the actual state of the transport. Thus we call get-status
	 * (ie. MTNOP) once and check the actual status.)
	 */
	tscommand (dev, MTNOP, 1);
	if (ts[unit].reg->tssr & TS_OFL) {
		printf ("%s: transport is offline.\n", sc->sc_dev.dv_xname);
		sc->sc_openf = 0;
		return EIO;		/* transport is offline */
	}

	sc->sc_liowf = 0;
	return 0;
}


/*
 * Close tape device.
 *
 * If tape was open for writing or last operation was
 * a write, then write two EOF's and backspace over the last one.
 * Unless this is a non-rewinding special file, rewind the tape.
 *
 * Make the tape available to others, by clearing openf flag.
 */
int
tsclose (dev, flag, type, p)
	dev_t dev;
	int flag, type;
	struct proc *p;
{
	register struct ts_softc *sc = ts_cd.cd_devs[TS_UNIT(dev)];

	if (flag == FWRITE || ((flag & FWRITE) && sc->sc_liowf)) {
		/* 
		 * We are writing two tape marks (EOT), but place the tape
		 * before the second one, so that another write operation
		 * will overwrite the second one and leave and EOF-mark.
		 */
		tscommand (dev, MTWEOF, 1);	/* Write Tape Mark */
		tscommand (dev, MTWEOF, 1);	/* Write Tape Mark */
		tscommand (dev, MTBSF,	1);	/* Skip Tape Marks Reverse */
	}

	if ((dev & T_NOREWIND) == 0)
		tscommand (dev, MTREW, 0);

	sc->sc_openf = 0;
	sc->sc_liowf = 0;
	return 0;
}


/*
 * Manage buffers and perform block mode read and write operations.
 */
void
tsstrategy (bp)
	register struct buf *bp;
{
	register int unit = TS_UNIT(bp->b_dev);
	struct ts_softc *sc = (void *)ts_cd.cd_devs[unit];
	int s;

	s = splbio ();
	/*
	 * we have only one command at one time, no credits.
	 * thus we don't need buffer management and controller queue
	 * just try to execute the command ...
	 */

	tsstart (sc, bp);
	splx(s);
	return;
}


/*
 * Catch ioctl commands, and call the "command" routine to do them.
 */
int
tsioctl (dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register struct buf *bp = &ts_cbuf[TS_UNIT(dev)];
	register struct ts_softc *sc;
	register struct mtop *mtop;	/* mag tape cmd op to perform */
	register struct mtget *mtget;	/* mag tape struct to get info in */
	register callcount;		/* number of times to call routine */
	int scount;			/* number of files/records to space */
	int spaceop = 0;		/* flag for skip/space operation */
	int error = 0;

	trace (("tsioctl (%x, %x, %x, %d)\n", dev, cmd, data, flag));

	switch (cmd) {
	case MTIOCTOP:			/* do a mag tape op */
		mtop = (struct mtop *)data;
		switch (mtop->mt_op) {
		case MTWEOF:		/* write an end-of-file record */
			callcount = mtop->mt_count;
			scount = 1;
			break;
		case MTFSF:		/* forward space file */
		case MTBSF:		/* backward space file */
		case MTFSR:		/* forward space record */
		case MTBSR:		/* backward space record */
			spaceop = 1;
			callcount = 1;
			scount = mtop->mt_count;
			break;
		case MTREW:		/* rewind */
		case MTOFFL:		/* rewind and put the drive offline */
		case MTNOP:		/* no operation, sets status only */
			callcount = 1;
			scount = 1;		/* wait for this rewind */
			break;
		case MTRETEN:		/* retension */
		case MTERASE:		/* erase entire tape */
		case MTEOM:		/* forward to end of media */
		case MTNBSF:		/* backward space to begin of file */
		case MTCACHE:		/* enable controller cache */
		case MTNOCACHE:		/* disable controller cache */
		case MTSETBSIZ:		/* set block size; 0 for variable */
		case MTSETDNSTY:	/* set density code for current mode */
			debug (("ioctl %d not implemented.\n", mtop->mt_op));
			return (ENXIO);
		default:
			debug (("invalid ioctl %d\n", mtop->mt_op));
			return (ENXIO);
		}	/* switch (mtop->mt_op) */

		if (callcount <= 0 || scount <= 0) {
			debug (("invalid values %d/%d\n", callcount, scount));
			return (EINVAL);
		}
		do {
			tscommand (dev, mtop->mt_op, scount);
			if (spaceop && bp->b_resid) {
				debug (("spaceop didn't complete\n"));
				return (EIO);
			}
			if (bp->b_flags & B_ERROR) {
				debug (("error in ioctl %d\n", mtop->mt_op));
				break;
			}
		} while (--callcount > 0);
		if (bp->b_flags & B_ERROR) 
			if ((error = bp->b_error) == 0)
				return (EIO);
		return (error);		

	case MTIOCGET:			/* get tape status */
		sc = ts_cd.cd_devs[TS_UNIT(dev)];
		mtget = (struct mtget *)data;
		mtget->mt_type = MT_ISTS;
		mtget->mt_dsreg = (unsigned)(ts[TS_UNIT(dev)].reg->tssr);
		mtget->mt_erreg = (unsigned)(ts[TS_UNIT(dev)].msg.hdr);
		mtget->mt_resid = 0;		/* ??? */
		mtget->mt_density = 0;		/* ??? */
		break;

	case MTIOCIEOT:			/* ignore EOT error */
		debug (("MTIOCIEOT not implemented.\n"));
		return (ENXIO); 

	case MTIOCEEOT:			/* enable EOT error */
		debug (("MTIOCEEOT not implemented.\n"));
		return (ENXIO);

	default:
		debug (("invalid ioctl cmd 0x%x\n", cmd));
		return (ENXIO);
	}

	return (0);
}


/*
 * 
 */
int
tsread (dev, uio)
	dev_t dev;
	struct uio *uio;
{
	return (physio (tsstrategy, NULL, dev, B_READ, minphys, uio));
}

/*
 *
 */
int
tswrite (dev, uio)
	dev_t dev;
	struct uio *uio;
{
	return (physio (tsstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 *
 */
int
tsdump(dev, blkno, va, size)
	dev_t dev;
	daddr64_t blkno;
	caddr_t va;
	size_t size;
{
	trace (("tsdump (%x)\n", dev));
	return 0;
}

/*----------------------------------------------------------------------*/

int
tsstatus (sr)
	int sr;
{
#ifdef DEBUG
	char bits[64];

	debug (("status: TSSR=%s\n", bitmask_snprintf(sr, TS_TSSR_BITS,
	    bits, sizeof(bits))));

	if (tsdebug < 5)
		return (0);

	if (sr & TS_SC)		printf ("special condition\n");
	if (sr & TS_UPE)	printf ("UPE\n");
	if (sr & TS_SCE)	printf ("Sanity Check Error\n");
	if (sr & TS_RMR)	printf ("Register Modification Refused\n");
	if (sr & TS_NXM)	printf ("Nonexistent Memory\n");
	if (sr & TS_NBA)	printf ("Need Buffer Address\n");
	if (sr & TS_A11)	printf ("Address Bits 17-16\n");
	if (sr & TS_SSR)	printf ("Subsystem Ready\n");
	if (sr & TS_OFL)	printf ("Off Line\n");
	if (sr & TS_FTC)	printf ("Fatal Termination Class Code\n");
	switch (sr & TS_TC) {
	case TS_TC_NORM:	printf ("Normal Termination\n"); break;
	case TS_TC_ATTN:	printf ("Attention Condition\n"); break;
	case TS_TC_TSA:		printf ("Tape status \n"); break;
	case TS_TC_FR:		printf ("Function reject\n"); break;
	case TS_TC_TPD:		printf ("Tape position down\n"); break;
	case TS_TC_TNM:		printf ("Tape not moved\n"); break;
	case TS_TC_TPL:		printf ("Tape position lost\n"); break;
	case TS_TC_FCE:		printf ("Fatal Controller Error\n"); break;
	}
#endif
	return (0);
}

void
tsxstatus (mp) 
	struct tsmsg *mp;
{
#ifdef DEBUG
	char bits[64];

	debug (("tsxstatus: xst0=%s, ", bitmask_snprintf(mp->xst0,
	    TS_XST0_BITS, bits, sizeof(bits))));
	debug (("xst1=%s, ", bitmask_snprintf(mp->xst1, TS_XST1_BITS,
	    bits, sizeof(bits))));
	debug (("xst2=%s, ", bitmask_snprintf(mp->xst2, TS_XST2_BITS, 
	    bits, sizeof(bits))));
	debug (("xst3=%s, ", bitmask_snprintf(mp->xst3, TS_XST3_BITS,
	    bits, sizeof(bits))));
	debug (("xst4=%s\n", bitmask_snprintf(mp->xst4, "\20",
	    bits, sizeof(bits))));

	if (tsdebug < 10)
		return (0);

	if (mp->xst0 & TS_SF_TMK)	printf ("Tape Mark Detected\n");
	if (mp->xst0 & TS_SF_RLS)	printf ("Record Length Short\n");
	if (mp->xst0 & TS_SF_LET)	printf ("Logical End of Tape\n");
	if (mp->xst0 & TS_SF_RLL)	printf ("Record Length Long\n");
	if (mp->xst0 & TS_SF_WLE)	printf ("Write Lock Error\n");
	if (mp->xst0 & TS_SF_NEF)	printf ("Nonexecutable Function\n");
	if (mp->xst0 & TS_SF_ILC)	printf ("Illegal Command\n");
	if (mp->xst0 & TS_SF_ILA)	printf ("Illegal Address\n");
	if (mp->xst0 & TS_SF_MOT)	printf ("Motion\n");
	if (mp->xst0 & TS_SF_ONL)	printf ("On-Line\n");
	if (mp->xst0 & TS_SF_IE)	printf ("Interrupt Enable\n");
	if (mp->xst0 & TS_SF_VCK)	printf ("Volume Check\n");
	if (mp->xst0 & TS_SF_PED)	printf ("Phase Encoded Drive\n");
	if (mp->xst0 & TS_SF_WLK)	printf ("Write Locked\n");
	if (mp->xst0 & TS_SF_BOT)	printf ("Beginning of Tape\n");
	if (mp->xst0 & TS_SF_EOT)	printf ("End of Tape\n");

	if (mp->xst1 & TS_SF_DLT)	printf ("Data Late\n");
	if (mp->xst1 & TS_SF_COR)	printf ("Correctable Data\n");
	if (mp->xst1 & TS_SF_RBP)	printf ("Read Bus Parity Error\n");
	if (mp->xst1 & TS_SF_UNC)	printf ("Uncorrectable Data or Hard Error\n");

	if (mp->xst2 & TS_SF_OPM)	printf ("Operation in Progress\n");
	if (mp->xst2 & TS_SF_RCE)	printf ("RAM Checksum Error\n");
	if (mp->xst2 & TS_SF_WCF)	printf ("Write Clock Failure\n");
	if (mp->xst2 & TS_SF_EFES)	printf ("extended features enabled\n");
	if (mp->xst2 & TS_SF_BES)	printf ("Buffering enabled\n");
	
	printf ("micro-code revision level: %d\n", (mp->xst2 & TS_SF_MCRL)>>2);
	printf ("unit number: %d\n", (mp->xst2 & TS_SF_UNIT));

	if (mp->xst3 & TS_SF_MDE)
		printf ("Micro-Diagnostics Error Code: 0x%x\n", mp->xst3 >> 8);
	if (mp->xst3 & TS_SF_OPI)	printf ("Operation Incomplete\n");
	if (mp->xst3 & TS_SF_REV)	printf ("Revers\n");
	if (mp->xst3 & TS_SF_DCK)	printf ("Density Check\n");
	if (mp->xst3 & TS_SF_RIB)	printf ("Reverse into BOT\n");

	if (mp->xst4 & TS_SF_HSP)	printf ("High Speed\n");
	if (mp->xst4 & TS_SF_RCX)	printf ("Retry Count Exceeded\n");
#endif
}

/*	$NetBSD: dcm.c,v 1.20 1995/12/02 18:18:50 thorpej Exp $	*/

/*
 * Copyright (c) 1995 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from Utah: $Hdr: dcm.c 1.29 92/01/21$
 *
 *	@(#)dcm.c	8.4 (Berkeley) 1/12/94
 */

/*
 * TODO:
 *	Timeouts
 *	Test console support.
 */

#include "dcm.h"
#if NDCM > 0
/*
 *  98642/MUX
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/time.h>

#include <machine/cpu.h>

#include <hp300/dev/device.h>
#include <hp300/dev/dcmreg.h>
#include <hp300/hp300/isr.h>

#ifndef DEFAULT_BAUD_RATE
#define DEFAULT_BAUD_RATE 9600
#endif

int	dcmmatch(), dcmintr(), dcmparam();
void	dcmattach(), dcmstart();
struct	driver dcmdriver = {
	dcmmatch, dcmattach, "dcm",
};

struct speedtab dcmspeedtab[] = {
	0,	BR_0,
	50,	BR_50,
	75,	BR_75,
	110,	BR_110,
	134,	BR_134,
	150,	BR_150,
	300,	BR_300,
	600,	BR_600,
	1200,	BR_1200,
	1800,	BR_1800,
	2400,	BR_2400,
	4800,	BR_4800,
	9600,	BR_9600,
	19200,	BR_19200,
	38400,	BR_38400,
	-1,	-1
};

/* u-sec per character based on baudrate (assumes 1 start/8 data/1 stop bit) */
#define	DCM_USPERCH(s)	(10000000 / (s))

/*
 * Per board interrupt scheme.  16.7ms is the polling interrupt rate
 * (16.7ms is about 550 baud, 38.4k is 72 chars in 16.7ms).
 */
#define DIS_TIMER	0
#define DIS_PERCHAR	1
#define DIS_RESET	2

int	dcmistype = -1;		/* -1 == dynamic, 0 == timer, 1 == perchar */
int     dcminterval = 5;	/* interval (secs) between checks */
struct	dcmischeme {
	int	dis_perchar;	/* non-zero if interrupting per char */
	long	dis_time;	/* last time examined */
	int	dis_intr;	/* recv interrupts during last interval */
	int	dis_char;	/* characters read during last interval */
};

/*
 * Console support
 */
#ifdef DCMCONSOLE
int	dcmconsole = DCMCONSOLE;
#else
int	dcmconsole = -1;
#endif
int	dcmconsinit;
int	dcmdefaultrate = DEFAULT_BAUD_RATE;
int	dcmconbrdbusy = 0;
int	dcmmajor;

#ifdef KGDB
/*
 * Kernel GDB support
 */
#include <machine/remote-sl.h>

extern dev_t kgdb_dev;
extern int kgdb_rate;
extern int kgdb_debug_init;
#endif

/* #define DCMSTATS */

#ifdef DEBUG
int	dcmdebug = 0x0;
#define DDB_SIOERR	0x01
#define DDB_PARAM	0x02
#define DDB_INPUT	0x04
#define DDB_OUTPUT	0x08
#define DDB_INTR	0x10
#define DDB_IOCTL	0x20
#define DDB_INTSCHM	0x40
#define DDB_MODEM	0x80
#define DDB_OPENCLOSE	0x100
#endif

#ifdef DCMSTATS
#define	DCMRBSIZE	94
#define DCMXBSIZE	24

struct	dcmstats {
	long	xints;		    /* # of xmit ints */
	long	xchars;		    /* # of xmit chars */
	long	xempty;		    /* times outq is empty in dcmstart */
	long	xrestarts;	    /* times completed while xmitting */
	long	rints;		    /* # of recv ints */
	long	rchars;		    /* # of recv chars */
	long	xsilo[DCMXBSIZE+2]; /* times this many chars xmit on one int */
	long	rsilo[DCMRBSIZE+2]; /* times this many chars read on one int */
};
#endif

#define DCMUNIT(x)		minor(x)
#define	DCMBOARD(x)		(((x) >> 2) & 0x3f)
#define DCMPORT(x)		((x) & 3)

/*
 * Conversion from "HP DCE" to almost-normal DCE: on the 638 8-port mux,
 * the distribution panel uses "HP DCE" conventions.  If requested via
 * the device flags, we swap the inputs to something closer to normal DCE,
 * allowing a straight-through cable to a DTE or a reversed cable
 * to a DCE (reversing 2-3, 4-5, 8-20 and leaving 6 unconnected;
 * this gets "DCD" on pin 20 and "CTS" on 4, but doesn't connect
 * DSR or make RTS work, though).  The following gives the full
 * details of a cable from this mux panel to a modem:
 *
 *		     HP		    modem
 *		name	pin	pin	name
 * HP inputs:
 *		"Rx"	 2	 3	Tx
 *		CTS	 4	 5	CTS	(only needed for CCTS_OFLOW)
 *		DCD	20	 8	DCD
 *		"DSR"	 9	 6	DSR	(unneeded)
 *		RI	22	22	RI	(unneeded)
 *
 * HP outputs:
 *		"Tx"	 3	 2	Rx
 *		"DTR"	 6	not connected
 *		"RTS"	 8	20	DTR
 *		"SR"	23	 4	RTS	(often not needed)
 */
#define hp2dce_in(ibits)	(iconv[(ibits) & 0xf])
static char iconv[16] = {
	0,		MI_DM,		MI_CTS,		MI_CTS|MI_DM,
	MI_CD,		MI_CD|MI_DM,	MI_CD|MI_CTS,	MI_CD|MI_CTS|MI_DM,
	MI_RI,		MI_RI|MI_DM,	MI_RI|MI_CTS,	MI_RI|MI_CTS|MI_DM,
	MI_RI|MI_CD,	MI_RI|MI_CD|MI_DM, MI_RI|MI_CD|MI_CTS,
	MI_RI|MI_CD|MI_CTS|MI_DM
};

#define	NDCMPORT	4	/* XXX what about 8-port cards? */

struct	dcm_softc {
	struct	hp_device *sc_hd;	/* device info */
	struct	dcmdevice *sc_dcm;	/* pointer to hardware */
	struct	tty *sc_tty[NDCMPORT];	/* our tty instances */
	struct	modemreg *sc_modem[NDCMPORT]; /* modem control */
	char	sc_mcndlast[NDCMPORT];	/* XXX last modem status for port */
	struct	isr sc_isr;		/* interrupt handler */
	short	sc_softCAR;		/* mask of ports with soft-carrier */
	struct	dcmischeme sc_scheme;	/* interrupt scheme for board */

	/*
	 * Mask of soft-carrier bits in config flags.
	 * XXX What about 8-port cards?
	 */
#define	DCM_SOFTCAR	0x0000000f

	int	sc_flags;		/* misc. configuration info */

	/*
	 * Bits for sc_flags
	 */
#define	DCM_ACTIVE	0x00000001	/* indicates board is alive */
#define	DCM_STDDCE	0x00000010	/* re-map DCE to standard */
#define	DCM_FLAGMASK	(DCM_STDDCE)	/* mask of valid bits in config flags */

#ifdef DCMSTATS
	struct	dcmstats sc_stats;	/* metrics gathering */
#endif
} dcm_softc[NDCM];

int
dcmmatch(hd)
	register struct hp_device *hd;
{
	struct dcm_softc *sc = &dcm_softc[hd->hp_unit];
	struct dcmdevice *dcm;
	int i, timo = 0;
	int s, brd, isconsole, mbits;

	dcm = (struct dcmdevice *)hd->hp_addr;
	if ((dcm->dcm_rsid & 0x1f) != DCMID)
		return (0);

	brd = hd->hp_unit;
	isconsole = (brd == DCMBOARD(dcmconsole));

	/*
	 * XXX selected console device (CONSUNIT) as determined by
	 * dcmcnprobe does not agree with logical numbering imposed
	 * by the config file (i.e. lowest address DCM is not unit
	 * CONSUNIT).  Don't recognize this card.
	 */
	if (isconsole && (dcm != sc->sc_dcm))
		return (0);

	sc->sc_hd = hd;
	hd->hp_ipl = DCMIPL(dcm->dcm_ic);

	/*
	 * Empirically derived self-test magic
	 */
	s = spltty();
	dcm->dcm_rsid = DCMRS;
	DELAY(50000);	/* 5000 is not long enough */
	dcm->dcm_rsid = 0; 
	dcm->dcm_ic = IC_IE;
	dcm->dcm_cr = CR_SELFT;
	while ((dcm->dcm_ic & IC_IR) == 0)
		if (++timo == 20000)
			return (0);
	DELAY(50000)	/* XXX why is this needed ???? */
	while ((dcm->dcm_iir & IIR_SELFT) == 0)
		if (++timo == 400000)
			return (0);
	DELAY(50000)	/* XXX why is this needed ???? */
	if (dcm->dcm_stcon != ST_OK) {
		if (!isconsole)
			printf("dcm%d: self test failed: %x\n",
			       brd, dcm->dcm_stcon);
		return (0);
	}
	dcm->dcm_ic = IC_ID;
	splx(s);

	return (1);
}

void
dcmattach(hd)
	register struct hp_device *hd;
{
	struct dcm_softc *sc = &dcm_softc[hd->hp_unit];
	struct dcmdevice *dcm;
	int i, timo = 0;
	int s, brd, isconsole, mbits;

	dcm = sc->sc_dcm = (struct dcmdevice *)hd->hp_addr;

	brd = hd->hp_unit;
	isconsole = (brd == DCMBOARD(dcmconsole));

	/* Extract configuration info from flags. */
	sc->sc_softCAR = (hd->hp_flags & DCM_SOFTCAR);
	sc->sc_flags = (hd->hp_flags & DCM_FLAGMASK);

	/* Mark our unit as configured. */
	sc->sc_flags |= DCM_ACTIVE;

	/* Establish the interrupt handler. */
	sc->sc_isr.isr_ipl = hd->hp_ipl;
	sc->sc_isr.isr_arg = brd;
	sc->sc_isr.isr_intr = dcmintr;
	isrlink(&sc->sc_isr);

	if (dcmistype == DIS_TIMER)
		dcmsetischeme(brd, DIS_RESET|DIS_TIMER);
	else
		dcmsetischeme(brd, DIS_RESET|DIS_PERCHAR);

	/* load pointers to modem control */
	sc->sc_modem[0] = &dcm->dcm_modem0;
	sc->sc_modem[1] = &dcm->dcm_modem1;
	sc->sc_modem[2] = &dcm->dcm_modem2;
	sc->sc_modem[3] = &dcm->dcm_modem3;

	/* set DCD (modem) and CTS (flow control) on all ports */
	if (sc->sc_flags & DCM_STDDCE)
		mbits = hp2dce_in(MI_CD|MI_CTS);
	else
		mbits = MI_CD|MI_CTS;

	for (i = 0; i < NDCMPORT; i++)
		sc->sc_modem[i]->mdmmsk = mbits;

	dcm->dcm_ic = IC_IE;		/* turn all interrupts on */

	/*
	 * Need to reset baud rate, etc. of next print so reset dcmconsole.
	 * Also make sure console is always "hardwired"
	 */
	if (isconsole) {
		dcmconsinit = 0;
		sc->sc_softCAR |= (1 << DCMPORT(dcmconsole));
		printf(": console on port %d\n", DCMPORT(dcmconsole));
	} else
		printf("\n");

#ifdef KGDB
	if (major(kgdb_dev) == dcmmajor &&
	    DCMBOARD(DCMUNIT(kgdb_dev)) == brd) {
		if (dcmconsole == DCMUNIT(kgdb_dev))
			kgdb_dev = NODEV; /* can't debug over console port */
#ifndef KGDB_CHEAT
		/*
		 * The following could potentially be replaced
		 * by the corresponding code in dcmcnprobe.
		 */
		else {
			(void) dcminit(kgdb_dev, kgdb_rate);
			if (kgdb_debug_init) {
				printf("%s port %d: ", sc->sc_hd->hp_xname,
				    DCMPORT(DCMUNIT(kgdb_dev)));
				kgdb_connect(1);
			} else
				printf("%s port %d: kgdb enabled\n",
				    sc->sc_hd->hp_xname,
				    DCMPORT(DCMUNIT(kgdb_dev)));
		}
		/* end could be replaced */
#endif
	}
#endif
}

/* ARGSUSED */
int
dcmopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct dcm_softc *sc;
	struct tty *tp;
	int unit, brd, port;
	int error = 0, mbits, s;

	unit = DCMUNIT(dev);
	brd = DCMBOARD(unit);
	port = DCMPORT(unit);

	if ((brd >= NDCM) || (port >= NDCMPORT))
		return (ENXIO);

	sc = &dcm_softc[brd];
	if ((sc->sc_flags & DCM_ACTIVE) == 0)
		return (ENXIO);

	if (sc->sc_tty[port] == NULL)
		tp = sc->sc_tty[port] = ttymalloc();
	else
		tp = sc->sc_tty[port];

	tp->t_oproc = dcmstart;
	tp->t_param = dcmparam;
	tp->t_dev = dev;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		/*
		 * Sanity clause: reset the card on first open.
		 * The card might be left in an inconsistent state
		 * if the card memory is read inadvertently.
		 */
		dcminit(dev, dcmdefaultrate);

		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;

		s = spltty();

		(void) dcmparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return (EBUSY);
	else
		s = spltty();

	/* Set modem control state. */
	mbits = MO_ON;
	if (sc->sc_flags & DCM_STDDCE)
		mbits |= MO_SR;		/* pin 23, could be used as RTS */

	(void) dcmmctl(dev, mbits, DMSET);	/* enable port */

	/* Set soft-carrier if so configured. */
	if ((sc->sc_softCAR & (1 << port)) ||
	    (dcmmctl(dev, MO_OFF, DMGET) & MI_CD))
		tp->t_state |= TS_CARR_ON;

#ifdef DEBUG
	if (dcmdebug & DDB_MODEM)
		printf("%s: dcmopen port %d softcarr %c\n",
		       sc->sc_hd->hp_xname, port,
		       (tp->t_state & TS_CARR_ON) ? '1' : '0');
#endif

	/* Wait for carrier if necessary. */
	if ((flag & O_NONBLOCK) == 0)
		while ((tp->t_cflag & CLOCAL) == 0 &&
		    (tp->t_state & TS_CARR_ON) == 0) {
			tp->t_state |= TS_WOPEN;
			error = ttysleep(tp, (caddr_t)&tp->t_rawq,
			    TTIPRI | PCATCH, ttopen, 0);
			if (error) {
				splx(s);
				return (error);
			}
		}
	
	splx(s);

#ifdef DEBUG
	if (dcmdebug & DDB_OPENCLOSE)
		printf("%s port %d: dcmopen: st %x fl %x\n",
			sc->sc_hd->hp_xname, port, tp->t_state, tp->t_flags);
#endif
	if (error == 0)
		error = (*linesw[tp->t_line].l_open)(dev, tp);

	return (error);
}
 
/*ARGSUSED*/
int
dcmclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int s, unit, board, port;
	struct dcm_softc *sc;
	struct tty *tp;
 
	unit = DCMUNIT(dev);
	board = DCMBOARD(unit);
	port = DCMPORT(unit);

	sc = &dcm_softc[board];
	tp = sc->sc_tty[port];

	(*linesw[tp->t_line].l_close)(tp, flag);

	s = spltty();

	if (tp->t_cflag & HUPCL || tp->t_state & TS_WOPEN ||
	    (tp->t_state & TS_ISOPEN) == 0)
		(void) dcmmctl(dev, MO_OFF, DMSET);
#ifdef DEBUG
	if (dcmdebug & DDB_OPENCLOSE)
		printf("%s port %d: dcmclose: st %x fl %x\n",
			sc->sc_hd->hp_xname, port, tp->t_state, tp->t_flags);
#endif
	splx(s);
	ttyclose(tp);
#if 0
	ttyfree(tp);
	sc->sc_tty[port] == NULL;
#endif
	return (0);
}
 
int
dcmread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int unit, board, port;
	struct dcm_softc *sc;
	register struct tty *tp;

	unit = DCMUNIT(dev);
	board = DCMBOARD(unit);
	port = DCMPORT(unit);

	sc = &dcm_softc[board];
	tp = sc->sc_tty[port];

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}
 
int
dcmwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int unit, board, port;
	struct dcm_softc *sc;
	register struct tty *tp;

	unit = DCMUNIT(dev);
	board = DCMBOARD(unit);
	port = DCMPORT(unit);

	sc = &dcm_softc[board];
	tp = sc->sc_tty[port];

	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
dcmtty(dev)
	dev_t dev;
{
	int unit, board, port;
	struct dcm_softc *sc;

	unit = DCMUNIT(dev);
	board = DCMBOARD(unit);
	port = DCMPORT(unit);

	sc = &dcm_softc[board];

	return (sc->sc_tty[port]);
}
 
int
dcmintr(brd)
	register int brd;
{
	struct dcm_softc *sc = &dcm_softc[brd];
	struct dcmdevice *dcm = sc->sc_dcm;
	struct dcmischeme *dis = &sc->sc_scheme;
	int code, i;
	int pcnd[4], mcode, mcnd[4];

	/*
	 * Do all guarded register accesses right off to minimize
	 * block out of hardware.
	 */
	SEM_LOCK(dcm);
	if ((dcm->dcm_ic & IC_IR) == 0) {
		SEM_UNLOCK(dcm);
		return (0);
	}
	for (i = 0; i < 4; i++) {
		pcnd[i] = dcm->dcm_icrtab[i].dcm_data;
		dcm->dcm_icrtab[i].dcm_data = 0;
		code = sc->sc_modem[i]->mdmin;
		if (sc->sc_flags & DCM_STDDCE)
			code = hp2dce_in(code);
		mcnd[i] = code;
	}
	code = dcm->dcm_iir & IIR_MASK;
	dcm->dcm_iir = 0;	/* XXX doc claims read clears interrupt?! */
	mcode = dcm->dcm_modemintr;
	dcm->dcm_modemintr = 0;
	SEM_UNLOCK(dcm);

#ifdef DEBUG
	if (dcmdebug & DDB_INTR) {
		printf("%s: dcmintr: iir %x pc %x/%x/%x/%x ",
		       sc->sc_hd->hp_xname, code, pcnd[0], pcnd[1],
		       pcnd[2], pcnd[3]); 
		printf("miir %x mc %x/%x/%x/%x\n",
		       mcode, mcnd[0], mcnd[1], mcnd[2], mcnd[3]);
	}
#endif
	if (code & IIR_TIMEO)
		dcmrint(sc);
	if (code & IIR_PORT0)
		dcmpint(sc, 0, pcnd[0]);
	if (code & IIR_PORT1)
		dcmpint(sc, 1, pcnd[1]);
	if (code & IIR_PORT2)
		dcmpint(sc, 2, pcnd[2]);
	if (code & IIR_PORT3)
		dcmpint(sc, 3, pcnd[3]);
	if (code & IIR_MODM) {
		if (mcode == 0 || mcode & 0x1)	/* mcode==0 -> 98642 board */
			dcmmint(sc, 0, mcnd[0]);
		if (mcode & 0x2)
			dcmmint(sc, 1, mcnd[1]);
		if (mcode & 0x4)
			dcmmint(sc, 2, mcnd[2]);
		if (mcode & 0x8)
			dcmmint(sc, 3, mcnd[3]);
	}

	/*
	 * Chalk up a receiver interrupt if the timer running or one of
	 * the ports reports a special character interrupt.
	 */
	if ((code & IIR_TIMEO) ||
	    ((pcnd[0]|pcnd[1]|pcnd[2]|pcnd[3]) & IT_SPEC))
		dis->dis_intr++;
	/*
	 * See if it is time to check/change the interrupt rate.
	 */
	if (dcmistype < 0 &&
	    (i = time.tv_sec - dis->dis_time) >= dcminterval) {
		/*
		 * If currently per-character and averaged over 70 interrupts
		 * per-second (66 is threshold of 600 baud) in last interval,
		 * switch to timer mode.
		 *
		 * XXX decay counts ala load average to avoid spikes?
		 */
		if (dis->dis_perchar && dis->dis_intr > 70 * i)
			dcmsetischeme(brd, DIS_TIMER);
		/*
		 * If currently using timer and had more interrupts than
		 * received characters in the last interval, switch back
		 * to per-character.  Note that after changing to per-char
		 * we must process any characters already in the queue
		 * since they may have arrived before the bitmap was setup.
		 *
		 * XXX decay counts?
		 */
		else if (!dis->dis_perchar && dis->dis_intr > dis->dis_char) {
			dcmsetischeme(brd, DIS_PERCHAR);
			dcmrint(sc);
		}
		dis->dis_intr = dis->dis_char = 0;
		dis->dis_time = time.tv_sec;
	}
	return (1);
}

/*
 *  Port interrupt.  Can be two things:
 *	First, it might be a special character (exception interrupt);
 *	Second, it may be a buffer empty (transmit interrupt);
 */
dcmpint(sc, port, code)
	struct dcm_softc *sc;
	int port, code;
{

	if (code & IT_SPEC)
		dcmreadbuf(sc, port);
	if (code & IT_TX)
		dcmxint(sc, port);
}

dcmrint(sc)
	struct dcm_softc *sc;
{
	int port;

	for (port = 0; port < NDCMPORT; port++)
		dcmreadbuf(sc, port);
}

dcmreadbuf(sc, port)
	struct dcm_softc *sc;
	int port;
{
	struct dcmdevice *dcm = sc->sc_dcm;
	struct tty *tp = sc->sc_tty[port];
	struct dcmpreg *pp = dcm_preg(dcm, port);
	struct dcmrfifo *fifo;
	int c, stat;
	u_int head;
	int nch = 0;
#ifdef DCMSTATS
	struct dcmstats *dsp = &sc->sc_stats;

	dsp->rints++;
#endif
	if ((tp->t_state & TS_ISOPEN) == 0) {
#ifdef KGDB
		if ((makedev(dcmmajor, minor(tp->t_dev)) == kgdb_dev) &&
		    (head = pp->r_head & RX_MASK) != (pp->r_tail & RX_MASK) &&
		    dcm->dcm_rfifos[3-port][head>>1].data_char == FRAME_START) {
			pp->r_head = (head + 2) & RX_MASK;
			kgdb_connect(0);	/* trap into kgdb */
			return;
		}
#endif /* KGDB */
		pp->r_head = pp->r_tail & RX_MASK;
		return;
	}

	head = pp->r_head & RX_MASK;
	fifo = &dcm->dcm_rfifos[3-port][head>>1];
	/*
	 * XXX upper bound on how many chars we will take in one swallow?
	 */
	while (head != (pp->r_tail & RX_MASK)) {
		/*
		 * Get character/status and update head pointer as fast
		 * as possible to make room for more characters.
		 */
		c = fifo->data_char;
		stat = fifo->data_stat;
		head = (head + 2) & RX_MASK;
		pp->r_head = head;
		fifo = head ? fifo+1 : &dcm->dcm_rfifos[3-port][0];
		nch++;

#ifdef DEBUG
		if (dcmdebug & DDB_INPUT)
			printf("%s port %d: dcmreadbuf: c%x('%c') s%x f%x h%x t%x\n",
			       sc->sc_hd->hp_xname, port,
			       c&0xFF, c, stat&0xFF,
			       tp->t_flags, head, pp->r_tail);
#endif
		/*
		 * Check for and handle errors
		 */
		if (stat & RD_MASK) {
#ifdef DEBUG
			if (dcmdebug & (DDB_INPUT|DDB_SIOERR))
				printf("%s port %d: dcmreadbuf: err: c%x('%c') s%x\n",
				       sc->sc_hd->hp_xname, port,
				       stat, c&0xFF, c);
#endif
			if (stat & (RD_BD | RD_FE))
				c |= TTY_FE;
			else if (stat & RD_PE)
				c |= TTY_PE;
			else if (stat & RD_OVF)
				log(LOG_WARNING,
				    "%s port %d: silo overflow\n",
				    sc->sc_hd->hp_xname, port);
			else if (stat & RD_OE)
				log(LOG_WARNING,
				    "%s port %d: uart overflow\n",
				    sc->sc_hd->hp_xname, port);
		}
		(*linesw[tp->t_line].l_rint)(c, tp);
	}
	sc->sc_scheme.dis_char += nch;

#ifdef DCMSTATS
	dsp->rchars += nch;
	if (nch <= DCMRBSIZE)
		dsp->rsilo[nch]++;
	else
		dsp->rsilo[DCMRBSIZE+1]++;
#endif
}

dcmxint(sc, port)
	struct dcm_softc *sc;
	int port;
{
	struct tty *tp = sc->sc_tty[port];

	tp->t_state &= ~TS_BUSY;
	if (tp->t_state & TS_FLUSH)
		tp->t_state &= ~TS_FLUSH;
	(*linesw[tp->t_line].l_start)(tp);
}

dcmmint(sc, port, mcnd)
	struct dcm_softc *sc;
	int port, mcnd;
{
	int delta;
	struct tty *tp;
	struct dcmdevice *dcm = sc->sc_dcm;

	tp = sc->sc_tty[port];

#ifdef DEBUG
	if (dcmdebug & DDB_MODEM)
		printf("%s port %d: dcmmint: mcnd %x mcndlast %x\n",
		       sc->sc_hd->hp_xname, port, mcnd, sc->sc_mcndlast[port]);
#endif
	delta = mcnd ^ sc->sc_mcndlast[port];
	sc->sc_mcndlast[port] = mcnd;
	if ((delta & MI_CTS) && (tp->t_state & TS_ISOPEN) &&
	    (tp->t_flags & CCTS_OFLOW)) {
		if (mcnd & MI_CTS) {
			tp->t_state &= ~TS_TTSTOP;
			ttstart(tp);
		} else
			tp->t_state |= TS_TTSTOP;	/* inline dcmstop */
	}
	if (delta & MI_CD) {
		if (mcnd & MI_CD)
			(void)(*linesw[tp->t_line].l_modem)(tp, 1);
		else if ((sc->sc_softCAR & (1 << port)) == 0 &&
		    (*linesw[tp->t_line].l_modem)(tp, 0) == 0) {
			sc->sc_modem[port]->mdmout = MO_OFF;
			SEM_LOCK(dcm);
			dcm->dcm_modemchng |= (1 << port);
			dcm->dcm_cr |= CR_MODM;
			SEM_UNLOCK(dcm);
			DELAY(10); /* time to change lines */
		}
	}
}

int
dcmioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct dcm_softc *sc;
	struct tty *tp;
	struct dcmdevice *dcm;
	int board, port, unit = DCMUNIT(dev);
	int error, s;

	port = DCMPORT(unit);
	board = DCMBOARD(unit);

	sc = &dcm_softc[board];
	dcm = sc->sc_dcm;
	tp = sc->sc_tty[port];
 
#ifdef DEBUG
	if (dcmdebug & DDB_IOCTL)
		printf("%s port %d: dcmioctl: cmd %x data %x flag %x\n",
		       sc->sc_hd->hp_xname, port, cmd, *data, flag);
#endif
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	switch (cmd) {
	case TIOCSBRK:
		/*
		 * Wait for transmitter buffer to empty
		 */
		s = spltty();
		while (dcm->dcm_thead[port].ptr != dcm->dcm_ttail[port].ptr)
			DELAY(DCM_USPERCH(tp->t_ospeed));
		SEM_LOCK(dcm);
		dcm->dcm_cmdtab[port].dcm_data |= CT_BRK;
		dcm->dcm_cr |= (1 << port);	/* start break */
		SEM_UNLOCK(dcm);
		splx(s);
		break;

	case TIOCCBRK:
		SEM_LOCK(dcm);
		dcm->dcm_cmdtab[port].dcm_data |= CT_BRK;
		dcm->dcm_cr |= (1 << port);	/* end break */
		SEM_UNLOCK(dcm);
		break;

	case TIOCSDTR:
		(void) dcmmctl(dev, MO_ON, DMBIS);
		break;

	case TIOCCDTR:
		(void) dcmmctl(dev, MO_ON, DMBIC);
		break;

	case TIOCMSET:
		(void) dcmmctl(dev, *(int *)data, DMSET);
		break;

	case TIOCMBIS:
		(void) dcmmctl(dev, *(int *)data, DMBIS);
		break;

	case TIOCMBIC:
		(void) dcmmctl(dev, *(int *)data, DMBIC);
		break;

	case TIOCMGET:
		*(int *)data = dcmmctl(dev, 0, DMGET);
		break;

	case TIOCGFLAGS: {
		int bits = 0;

		if ((sc->sc_softCAR & (1 << port)))
			bits |= TIOCFLAG_SOFTCAR;

		if (tp->t_cflag & CLOCAL)
			bits |= TIOCFLAG_CLOCAL;

		*(int *)data = bits;
		break;
	}

	case TIOCSFLAGS: {
		int userbits;

		error = suser(p->p_ucred, &p->p_acflag);
		if (error)
			return (EPERM);

		userbits = *(int *)data;

		if ((userbits & TIOCFLAG_SOFTCAR) ||
		    ((board == DCMBOARD(dcmconsole)) &&
		    (port == DCMPORT(dcmconsole))))
			sc->sc_softCAR |= (1 << port);

		if (userbits & TIOCFLAG_CLOCAL)
			tp->t_cflag |= CLOCAL;

		break;
	}

	default:
		return (ENOTTY);
	}
	return (0);
}

int
dcmparam(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	struct dcm_softc *sc;
	struct dcmdevice *dcm;
	int unit, board, port, mode, cflag = t->c_cflag;
	int ospeed = ttspeedtab(t->c_ospeed, dcmspeedtab);

	unit = DCMUNIT(tp->t_dev);
	board = DCMBOARD(unit);
	port = DCMPORT(unit);

	sc = &dcm_softc[board];
	dcm = sc->sc_dcm;

	/* check requested parameters */
        if (ospeed < 0 || (t->c_ispeed && t->c_ispeed != t->c_ospeed))
                return (EINVAL);
        /* and copy to tty */
        tp->t_ispeed = t->c_ispeed;
        tp->t_ospeed = t->c_ospeed;
        tp->t_cflag = cflag;
	if (ospeed == 0) {
		(void) dcmmctl(DCMUNIT(tp->t_dev), MO_OFF, DMSET);
		return (0);
	}

	mode = 0;
	switch (cflag&CSIZE) {
	case CS5:
		mode = LC_5BITS; break;
	case CS6:
		mode = LC_6BITS; break;
	case CS7:
		mode = LC_7BITS; break;
	case CS8:
		mode = LC_8BITS; break;
	}
	if (cflag&PARENB) {
		if (cflag&PARODD)
			mode |= LC_PODD;
		else
			mode |= LC_PEVEN;
	}
	if (cflag&CSTOPB)
		mode |= LC_2STOP;
	else
		mode |= LC_1STOP;
#ifdef DEBUG
	if (dcmdebug & DDB_PARAM)
		printf("%s port %d: dcmparam: cflag %x mode %x speed %d uperch %d\n",
		       sc->sc_hd->hp_xname, port, cflag, mode, tp->t_ospeed,
		       DCM_USPERCH(tp->t_ospeed));
#endif

	/*
	 * Wait for transmitter buffer to empty.
	 */
	while (dcm->dcm_thead[port].ptr != dcm->dcm_ttail[port].ptr)
		DELAY(DCM_USPERCH(tp->t_ospeed));
	/*
	 * Make changes known to hardware.
	 */
	dcm->dcm_data[port].dcm_baud = ospeed;
	dcm->dcm_data[port].dcm_conf = mode;
	SEM_LOCK(dcm);
	dcm->dcm_cmdtab[port].dcm_data |= CT_CON;
	dcm->dcm_cr |= (1 << port);
	SEM_UNLOCK(dcm);
	/*
	 * Delay for config change to take place. Weighted by baud.
	 * XXX why do we do this?
	 */
	DELAY(16 * DCM_USPERCH(tp->t_ospeed));
	return (0);
}
 
void
dcmstart(tp)
	register struct tty *tp;
{
	struct dcm_softc *sc;
	struct dcmdevice *dcm;
	struct dcmpreg *pp;
	struct dcmtfifo *fifo;
	char *bp;
	u_int head, tail, next;
	int unit, board, port, nch;
	char buf[16];
	int s;
#ifdef DCMSTATS
	struct dcmstats *dsp = &sc->sc_stats;
	int tch = 0;
#endif

	unit = DCMUNIT(tp->t_dev);
	board = DCMBOARD(unit);
	port = DCMPORT(unit);

	sc = &dcm_softc[board];
	dcm = sc->sc_dcm;

	s = spltty();
#ifdef DCMSTATS
	dsp->xints++;
#endif
#ifdef DEBUG
	if (dcmdebug & DDB_OUTPUT)
		printf("%s port %d: dcmstart: state %x flags %x outcc %d\n",
		       sc->sc_hd->hp_xname, port, tp->t_state, tp->t_flags,
		       tp->t_outq.c_cc);
#endif
	if (tp->t_state & (TS_TIMEOUT|TS_BUSY|TS_TTSTOP))
		goto out;
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state&TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
	if (tp->t_outq.c_cc == 0) {
#ifdef DCMSTATS
		dsp->xempty++;
#endif
		goto out;
	}

	pp = dcm_preg(dcm, port);
	tail = pp->t_tail & TX_MASK;
	next = (tail + 1) & TX_MASK;
	head = pp->t_head & TX_MASK;
	if (head == next)
		goto out;
	fifo = &dcm->dcm_tfifos[3-port][tail];
again:
	nch = q_to_b(&tp->t_outq, buf, (head - next) & TX_MASK);
#ifdef DCMSTATS
	tch += nch;
#endif
#ifdef DEBUG
	if (dcmdebug & DDB_OUTPUT)
		printf("\thead %x tail %x nch %d\n", head, tail, nch);
#endif
	/*
	 * Loop transmitting all the characters we can.
	 */
	for (bp = buf; --nch >= 0; bp++) {
		fifo->data_char = *bp;
		pp->t_tail = next;
		/*
		 * If this is the first character,
		 * get the hardware moving right now.
		 */
		if (bp == buf) {
			tp->t_state |= TS_BUSY;
			SEM_LOCK(dcm);
			dcm->dcm_cmdtab[port].dcm_data |= CT_TX;
			dcm->dcm_cr |= (1 << port);
			SEM_UNLOCK(dcm);
		}
		tail = next;
		fifo = tail ? fifo+1 : &dcm->dcm_tfifos[3-port][0];
		next = (next + 1) & TX_MASK;
	}
	/*
	 * Head changed while we were loading the buffer,
	 * go back and load some more if we can.
	 */
	if (tp->t_outq.c_cc && head != (pp->t_head & TX_MASK)) {
#ifdef DCMSTATS
		dsp->xrestarts++;
#endif
		head = pp->t_head & TX_MASK;
		goto again;
	}

	/*
	 * Kick it one last time in case it finished while we were
	 * loading the last bunch.
	 */
	if (bp > &buf[1]) {
		tp->t_state |= TS_BUSY;
		SEM_LOCK(dcm);
		dcm->dcm_cmdtab[port].dcm_data |= CT_TX;
		dcm->dcm_cr |= (1 << port);
		SEM_UNLOCK(dcm);
	}
#ifdef DEBUG
	if (dcmdebug & DDB_INTR)
		printf("%s port %d: dcmstart(%d): head %x tail %x outqcc %d\n",
		    sc->sc_hd->hp_xname, port, head, tail, tp->t_outq.c_cc);
#endif
out:
#ifdef DCMSTATS
	dsp->xchars += tch;
	if (tch <= DCMXBSIZE)
		dsp->xsilo[tch]++;
	else
		dsp->xsilo[DCMXBSIZE+1]++;
#endif
	splx(s);
}
 
/*
 * Stop output on a line.
 */
int
dcmstop(tp, flag)
	register struct tty *tp;
	int flag;
{
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY) {
		/* XXX is there some way to safely stop transmission? */
		if ((tp->t_state&TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
}
 
/*
 * Modem control
 */
dcmmctl(dev, bits, how)
	dev_t dev;
	int bits, how;
{
	struct dcm_softc *sc;
	struct dcmdevice *dcm;
	int s, unit, brd, port, hit = 0;

	unit = DCMUNIT(dev);
	brd = DCMBOARD(unit);
	port = DCMPORT(unit);
	sc = &dcm_softc[brd];
	dcm = sc->sc_dcm;

#ifdef DEBUG
	if (dcmdebug & DDB_MODEM)
		printf("%s port %d: dcmmctl: bits 0x%x how %x\n",
		       sc->sc_hd->hp_xname, port, bits, how);
#endif

	s = spltty();

	switch (how) {
	case DMSET:
		sc->sc_modem[port]->mdmout = bits;
		hit++;
		break;

	case DMBIS:
		sc->sc_modem[port]->mdmout |= bits;
		hit++;
		break;

	case DMBIC:
		sc->sc_modem[port]->mdmout &= ~bits;
		hit++;
		break;

	case DMGET:
		bits = sc->sc_modem[port]->mdmin;
		if (sc->sc_flags & DCM_STDDCE)
			bits = hp2dce_in(bits);
		break;
	}
	if (hit) {
		SEM_LOCK(dcm);
		dcm->dcm_modemchng |= 1<<(unit & 3);
		dcm->dcm_cr |= CR_MODM;
		SEM_UNLOCK(dcm);
		DELAY(10); /* delay until done */
		(void) splx(s);
	}
	return (bits);
}

/*
 * Set board to either interrupt per-character or at a fixed interval.
 */
dcmsetischeme(brd, flags)
	int brd, flags;
{
	struct dcm_softc *sc = &dcm_softc[brd];
	struct dcmdevice *dcm = sc->sc_dcm;
	struct dcmischeme *dis = &sc->sc_scheme;
	int i;
	u_char mask;
	int perchar = flags & DIS_PERCHAR;

#ifdef DEBUG
	if (dcmdebug & DDB_INTSCHM)
		printf("%s: dcmsetischeme(%d): cur %d, ints %d, chars %d\n",
		       sc->sc_hd->hp_xname, perchar, dis->dis_perchar,
		       dis->dis_intr, dis->dis_char);
	if ((flags & DIS_RESET) == 0 && perchar == dis->dis_perchar) {
		printf("%s: dcmsetischeme: redundent request %d\n",
		       sc->sc_hd->hp_xname, perchar);
		return;
	}
#endif
	/*
	 * If perchar is non-zero, we enable interrupts on all characters
	 * otherwise we disable perchar interrupts and use periodic
	 * polling interrupts.
	 */
	dis->dis_perchar = perchar;
	mask = perchar ? 0xf : 0x0;
	for (i = 0; i < 256; i++)
		dcm->dcm_bmap[i].data_data = mask;
	/*
	 * Don't slow down tandem mode, interrupt on flow control
	 * chars for any port on the board.
	 */
	if (!perchar) {
		register struct tty *tp;
		int c;

		for (i = 0; i < NDCMPORT; i++) {
			tp = sc->sc_tty[i];

			if ((c = tp->t_cc[VSTART]) != _POSIX_VDISABLE)
				dcm->dcm_bmap[c].data_data |= (1 << i);
			if ((c = tp->t_cc[VSTOP]) != _POSIX_VDISABLE)
				dcm->dcm_bmap[c].data_data |= (1 << i);
		}
	}
	/*
	 * Board starts with timer disabled so if first call is to
	 * set perchar mode then we don't want to toggle the timer.
	 */
	if (flags == (DIS_RESET|DIS_PERCHAR))
		return;
	/*
	 * Toggle card 16.7ms interrupts (we first make sure that card
	 * has cleared the bit so it will see the toggle).
	 */
	while (dcm->dcm_cr & CR_TIMER)
		;
	SEM_LOCK(dcm);
	dcm->dcm_cr |= CR_TIMER;
	SEM_UNLOCK(dcm);
}

/*
 * Following are all routines needed for DCM to act as console
 */
#include <dev/cons.h>

void
dcmcnprobe(cp)
	struct consdev *cp;
{
	struct dcm_softc *sc;
	struct dcmdevice *dcm;
	struct hp_hw *hw;
	int unit;

	/* locate the major number */
	for (dcmmajor = 0; dcmmajor < nchrdev; dcmmajor++)
		if (cdevsw[dcmmajor].d_open == dcmopen)
			break;

	/*
	 * Implicitly assigns the lowest select code DCM card found to be
	 * logical unit 0 (actually CONUNIT).  If your config file does
	 * anything different, you're screwed.
	 */
	for (hw = sc_table; hw->hw_type; hw++)
		if (HW_ISDEV(hw, D_COMMDCM) && !badaddr((short *)hw->hw_kva))
			break;
	if (!HW_ISDEV(hw, D_COMMDCM)) {
		cp->cn_pri = CN_DEAD;
		return;
	}

	unit = CONUNIT;
	sc = &dcm_softc[DCMBOARD(CONUNIT)];
	dcm = sc->sc_dcm = (struct dcmdevice *)hw->hw_kva;

	/* initialize required fields */
	cp->cn_dev = makedev(dcmmajor, unit);
	switch (dcm->dcm_rsid) {
	case DCMID:
		cp->cn_pri = CN_NORMAL;
		break;

	case DCMID|DCMCON:
		cp->cn_pri = CN_REMOTE;
		break;

	default:
		cp->cn_pri = CN_DEAD;
		return;
	}

	/*
	 * If dcmconsole is initialized, raise our priority.
	 */
	if (dcmconsole == unit)
		cp->cn_pri = CN_REMOTE;
#ifdef KGDB_CHEAT
	/*
	 * This doesn't currently work, at least not with ite consoles;
	 * the console hasn't been initialized yet.
	 */
	if (major(kgdb_dev) == dcmmajor &&
	    DCMBOARD(DCMUNIT(kgdb_dev)) == DCMBOARD(unit)) {
		(void) dcminit(kgdb_dev, kgdb_rate);
		if (kgdb_debug_init) {
			/*
			 * We assume that console is ready for us...
			 * this assumes that a dca or ite console
			 * has been selected already and will init
			 * on the first putc.
			 */
			printf("dcm%d: ", DCMUNIT(kgdb_dev));
			kgdb_connect(1);
		}
	}
#endif
}

void
dcmcninit(cp)
	struct consdev *cp;
{

	dcminit(cp->cn_dev, dcmdefaultrate);
	dcmconsinit = 1;
	dcmconsole = DCMUNIT(cp->cn_dev);
}

dcminit(dev, rate)
	dev_t dev;
	int rate;
{
	struct dcm_softc *sc;
	struct dcmdevice *dcm;
	int s, mode, unit, board, port;

	unit = DCMUNIT(dev);
	board = DCMBOARD(unit);
	port = DCMPORT(unit);

	sc = &dcm_softc[board];
	dcm = sc->sc_dcm;

	mode = LC_8BITS | LC_1STOP;

	s = splhigh();

	/*
	 * Wait for transmitter buffer to empty.
	 */
	while (dcm->dcm_thead[port].ptr != dcm->dcm_ttail[port].ptr)
		DELAY(DCM_USPERCH(rate));

	/*
	 * Make changes known to hardware.
	 */
	dcm->dcm_data[port].dcm_baud = ttspeedtab(rate, dcmspeedtab);
	dcm->dcm_data[port].dcm_conf = mode;
	SEM_LOCK(dcm);
	dcm->dcm_cmdtab[port].dcm_data |= CT_CON;
	dcm->dcm_cr |= (1 << port);
	SEM_UNLOCK(dcm);

	/*
	 * Delay for config change to take place. Weighted by baud.
	 * XXX why do we do this?
	 */
	DELAY(16 * DCM_USPERCH(rate));
	splx(s);
}

int
dcmcngetc(dev)
	dev_t dev;
{
	struct dcm_softc *sc;
	struct dcmdevice *dcm;
	struct dcmrfifo *fifo;
	struct dcmpreg *pp;
	u_int head;
	int s, c, stat, unit, board, port;

	unit = DCMUNIT(dev);
	board = DCMBOARD(unit);
	port = DCMPORT(unit);

	sc = &dcm_softc[board];
	dcm = sc->sc_dcm;
	pp = dcm_preg(dcm, port);

	s = splhigh();
	head = pp->r_head & RX_MASK;
	fifo = &dcm->dcm_rfifos[3-port][head>>1];
	while (head == (pp->r_tail & RX_MASK))
		;
	/*
	 * If board interrupts are enabled, just let our received char
	 * interrupt through in case some other port on the board was
	 * busy.  Otherwise we must clear the interrupt.
	 */
	SEM_LOCK(dcm);
	if ((dcm->dcm_ic & IC_IE) == 0)
		stat = dcm->dcm_iir;
	SEM_UNLOCK(dcm);
	c = fifo->data_char;
	stat = fifo->data_stat;
	pp->r_head = (head + 2) & RX_MASK;
	splx(s);
	return (c);
}

/*
 * Console kernel output character routine.
 */
void
dcmcnputc(dev, c)
	dev_t dev;
	int c;
{
	struct dcm_softc *sc;
	struct dcmdevice *dcm;
	struct dcmpreg *pp;
	unsigned tail;
	int s, unit, board, port, stat;

	unit = DCMUNIT(dev);
	board = DCMBOARD(unit);
	port = DCMPORT(unit);

	sc = &dcm_softc[board];
	dcm = sc->sc_dcm;
	pp = dcm_preg(dcm, port);

	s = splhigh();
#ifdef KGDB
	if (dev != kgdb_dev)
#endif
	if (dcmconsinit == 0) {
		(void) dcminit(dev, dcmdefaultrate);
		dcmconsinit = 1;
	}
	tail = pp->t_tail & TX_MASK;
	while (tail != (pp->t_head & TX_MASK))
		;
	dcm->dcm_tfifos[3-port][tail].data_char = c;
	pp->t_tail = tail = (tail + 1) & TX_MASK;
	SEM_LOCK(dcm);
	dcm->dcm_cmdtab[port].dcm_data |= CT_TX;
	dcm->dcm_cr |= (1 << port);
	SEM_UNLOCK(dcm);
	while (tail != (pp->t_head & TX_MASK))
		;
	/*
	 * If board interrupts are enabled, just let our completion
	 * interrupt through in case some other port on the board
	 * was busy.  Otherwise we must clear the interrupt.
	 */
	if ((dcm->dcm_ic & IC_IE) == 0) {
		SEM_LOCK(dcm);
		stat = dcm->dcm_iir;
		SEM_UNLOCK(dcm);
	}
	splx(s);
}
#endif

/*	$OpenBSD: dcm.c,v 1.22 2005/01/15 21:08:36 miod Exp $	*/
/*	$NetBSD: dcm.c,v 1.41 1997/05/05 20:59:16 thorpej Exp $	*/

/*
 * Copyright (c) 1995, 1996, 1997 Jason R. Thorpe.  All rights reserved.
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
 * from Utah: $Hdr: dcm.c 1.29 92/01/21$
 *
 *	@(#)dcm.c	8.4 (Berkeley) 1/12/94
 */

/*
 * TODO:
 *	Timeouts
 *	Test console support.
 */

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
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <dev/cons.h>

#include <hp300/dev/dioreg.h>
#include <hp300/dev/diovar.h>
#include <hp300/dev/diodevs.h>
#include <hp300/dev/dcmreg.h>

#ifndef DEFAULT_BAUD_RATE
#define DEFAULT_BAUD_RATE 9600
#endif

const struct speedtab dcmspeedtab[] = {
	{	0,	BR_0		},
	{	50,	BR_50		},
	{	75,	BR_75		},
	{	110,	BR_110		},
	{	134,	BR_134		},
	{	150,	BR_150		},
	{	300,	BR_300		},
	{	600,	BR_600		},
	{	1200,	BR_1200		},
	{	1800,	BR_1800		},
	{	2400,	BR_2400		},
	{	4800,	BR_4800		},
	{	9600,	BR_9600		},
	{	19200,	BR_19200	},
	{	38400,	BR_38400	},
	{	-1,	-1		},
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
 * Stuff for DCM console support.  This could probably be done a little
 * better.
 */
static	struct dcmdevice *dcm_cn = NULL;	/* pointer to hardware */
static	int dcmconsinit;			/* has been initialized */
/* static	int dcm_lastcnpri = CN_DEAD; */	/* XXX last priority */

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

#define DCMUNIT(x)		(minor(x) & 0x7f)
#define DCMCUA(x)		(minor(x) & 0x80)
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

/*
 * Note that 8-port boards appear as 2 4-port boards at consecutive
 * select codes.
 */
#define	NDCMPORT	4

struct	dcm_softc {
	struct	device sc_dev;		/* generic device glue */
	struct	isr sc_isr;
	struct	dcmdevice *sc_dcm;	/* pointer to hardware */
	struct	tty *sc_tty[NDCMPORT];	/* our tty instances */
	struct	modemreg *sc_modem[NDCMPORT]; /* modem control */
	char	sc_mcndlast[NDCMPORT];	/* XXX last modem status for port */
	short	sc_softCAR;		/* mask of ports with soft-carrier */
	struct	dcmischeme sc_scheme;	/* interrupt scheme for board */
	u_char	sc_cua;			/* callout mode */

	/*
	 * Mask of soft-carrier bits in config flags.
	 */
#define	DCM_SOFTCAR	0x0000000f

	int	sc_flags;		/* misc. configuration info */

	/*
	 * Bits for sc_flags
	 */
#define	DCM_ACTIVE	0x00000001	/* indicates board is alive */
#define	DCM_ISCONSOLE	0x00000002	/* indicates board is console */
#define	DCM_STDDCE	0x00000010	/* re-map DCE to standard */
#define	DCM_FLAGMASK	(DCM_STDDCE)	/* mask of valid bits in config flags */

#ifdef DCMSTATS
	struct	dcmstats sc_stats;	/* metrics gathering */
#endif
};

cdev_decl(dcm);

int	dcmintr(void *);
void	dcmpint(struct dcm_softc *, int, int);
void	dcmrint(struct dcm_softc *);
void	dcmreadbuf(struct dcm_softc *, int);
void	dcmxint(struct dcm_softc *, int);
void	dcmmint(struct dcm_softc *, int, int);

int	dcmparam(struct tty *, struct termios *);
void	dcmstart(struct tty *);
int	dcmstop(struct tty *, int);
int	dcmmctl(dev_t, int, int);
void	dcmsetischeme(int, int);
void	dcminit(struct dcmdevice *, int, int);

int	dcmselftest(struct dcm_softc *);

int	dcm_console_scan(int, caddr_t, void *);
void	dcmcnprobe(struct consdev *);
void	dcmcninit(struct consdev *);
int	dcmcngetc(dev_t);
void	dcmcnputc(dev_t, int);

int	dcmmatch(struct device *, void *, void *);
void	dcmattach(struct device *, struct device *, void *);

struct cfattach dcm_ca = {
	sizeof(struct dcm_softc), dcmmatch, dcmattach
};

struct cfdriver dcm_cd = {
	NULL, "dcm", DV_TTY
};

int
dcmmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct dio_attach_args *da = aux;

	switch (da->da_id) {
	case DIO_DEVICE_ID_DCM:
	case DIO_DEVICE_ID_DCMREM:
		return (1);
	}

	return (0);
}

void
dcmattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct dcm_softc *sc = (struct dcm_softc *)self;
	struct dio_attach_args *da = aux;
	struct dcmdevice *dcm;
	int brd = self->dv_unit;
	int scode = da->da_scode;
	int i, mbits, code, ipl;

	sc->sc_flags = 0;

	if (scode == conscode) {
		dcm = (struct dcmdevice *)conaddr;
		sc->sc_flags |= DCM_ISCONSOLE;

		/*
		 * We didn't know which unit this would be during
		 * the console probe, so we have to fixup cn_dev here.
		 * Note that we always assume port 1 on the board.
		 */
		cn_tab->cn_dev = makedev(dcmmajor, (brd << 2) | DCMCONSPORT);
	} else {
		dcm = (struct dcmdevice *)iomap(dio_scodetopa(da->da_scode),
		    da->da_size);
		if (dcm == NULL) {
			printf("\n%s: can't map registers\n",
			    sc->sc_dev.dv_xname);
			return;
		}
	}

	sc->sc_dcm = dcm;

	ipl = DIO_IPL(dcm);
	printf(" ipl %d", ipl);

	/*
	 * XXX someone _should_ fix this; the self test screws
	 * autoconfig messages.
	 */
	if ((sc->sc_flags & DCM_ISCONSOLE) && dcmselftest(sc)) {
		printf("\n%s: self-test failed\n", sc->sc_dev.dv_xname);
		return;
	}

	/* Extract configuration info from flags. */
	sc->sc_softCAR = self->dv_cfdata->cf_flags & DCM_SOFTCAR;
	sc->sc_flags |= self->dv_cfdata->cf_flags & DCM_FLAGMASK;

	/* Mark our unit as configured. */
	sc->sc_flags |= DCM_ACTIVE;

	/* Establish the interrupt handler. */
	sc->sc_isr.isr_func = dcmintr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_ipl = ipl;
	sc->sc_isr.isr_priority = IPL_TTY;
	dio_intr_establish(&sc->sc_isr, self->dv_xname);

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

	/*
	 * Get current state of mdmin register on all ports, so that
	 * deltas will work properly.
	 */
	for (i = 0; i < NDCMPORT; i++) {
		code = sc->sc_modem[i]->mdmin;
		if (sc->sc_flags & DCM_STDDCE)
			code = hp2dce_in(code);
		sc->sc_mcndlast[i] = code;
	}

	dcm->dcm_ic = IC_IE;		/* turn all interrupts on */

	/*
	 * Need to reset baud rate, etc. of next print so reset dcmconsinit.
	 * Also make sure console is always "hardwired"
	 */
	if (sc->sc_flags & DCM_ISCONSOLE) {
		dcmconsinit = 0;
		sc->sc_softCAR |= (1 << DCMCONSPORT);
		printf(": console on port %d\n", DCMCONSPORT);
	} else
		printf("\n");

#ifdef KGDB
	if (major(kgdb_dev) == dcmmajor &&
	    DCMBOARD(DCMUNIT(kgdb_dev)) == brd) {
		if (dcmconsole == DCMUNIT(kgdb_dev))	/* XXX fixme */
			kgdb_dev = NODEV; /* can't debug over console port */
#ifndef KGDB_CHEAT
		/*
		 * The following could potentially be replaced
		 * by the corresponding code in dcmcnprobe.
		 */
		else {
			dcminit(dcm, DCMPORT(DCMUNIT(kgdb_dev)),
			    kgdb_rate);
			if (kgdb_debug_init) {
				printf("%s port %d: ", sc->sc_dev.dv_xname,
				    DCMPORT(DCMUNIT(kgdb_dev)));
				kgdb_connect(1);
			} else
				printf("%s port %d: kgdb enabled\n",
				    sc->sc_dev.dv_xname,
				    DCMPORT(DCMUNIT(kgdb_dev)));
		}
		/* end could be replaced */
#endif /* KGDB_CHEAT */
	}
#endif /* KGDB */
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

	if (brd >= dcm_cd.cd_ndevs || port >= NDCMPORT ||
	    (sc = dcm_cd.cd_devs[brd]) == NULL)
		return (ENXIO);

	if ((sc->sc_flags & DCM_ACTIVE) == 0)
		return (ENXIO);

	s = spltty();
	if (sc->sc_tty[port] == NULL) {
		tp = sc->sc_tty[port] = ttymalloc();
	} else
		tp = sc->sc_tty[port];
	splx(s);

	tp->t_oproc = dcmstart;
	tp->t_param = dcmparam;
	tp->t_dev = dev;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		/*
		 * Sanity clause: reset the card on first open.
		 * The card might be left in an inconsistent state
		 * if the card memory is read inadvertently.
		 */
		dcminit(sc->sc_dcm, port, dcmdefaultrate);

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
	if ((sc->sc_softCAR & (1 << port)) || DCMCUA(dev) ||
	    (dcmmctl(dev, MO_OFF, DMGET) & MI_CD))
		tp->t_state |= TS_CARR_ON;

	if (DCMCUA(dev)) {
		if (tp->t_state & TS_ISOPEN) {
			/* Ah, but someone already is dialed in... */
			splx(s);
			return (EBUSY);
		}
		sc->sc_cua = 1;		/* We go into CUA mode */
	}

#ifdef DEBUG
	if (dcmdebug & DDB_MODEM)
		printf("%s: dcmopen port %d softcarr %c\n",
		       sc->sc_dev.dv_xname, port,
		       (tp->t_state & TS_CARR_ON) ? '1' : '0');
#endif

	/* Wait for carrier if necessary. */
	if (flag & O_NONBLOCK) {
		if (!DCMCUA(dev) && sc->sc_cua) {
			/* Opening TTY non-blocking... but the CUA is busy */
			splx(s);
			return (EBUSY);
		}
	} else {
		while (sc->sc_cua ||
		    ((tp->t_cflag & CLOCAL) == 0 &&
		    (tp->t_state & TS_CARR_ON) == 0)) {
			tp->t_state |= TS_WOPEN;
			error = ttysleep(tp, (caddr_t)&tp->t_rawq,
			    TTIPRI | PCATCH, ttopen, 0);
			if (!DCMCUA(dev) && sc->sc_cua && error == EINTR)
				continue;
			if (error) {
				if (DCMCUA(dev))
					sc->sc_cua = 0;
				splx(s);
				return (error);
			}
			if (!DCMCUA(dev) && sc->sc_cua)
				continue;
		}
	}
	splx(s);

#ifdef DEBUG
	if (dcmdebug & DDB_OPENCLOSE)
		printf("%s port %d: dcmopen: st %x fl %x\n",
			sc->sc_dev.dv_xname, port, tp->t_state, tp->t_flags);
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

	sc = dcm_cd.cd_devs[board];
	tp = sc->sc_tty[port];

	(*linesw[tp->t_line].l_close)(tp, flag);

	s = spltty();

	if (tp->t_cflag & HUPCL || tp->t_state & TS_WOPEN ||
	    (tp->t_state & TS_ISOPEN) == 0)
		(void) dcmmctl(dev, MO_OFF, DMSET);
#ifdef DEBUG
	if (dcmdebug & DDB_OPENCLOSE)
		printf("%s port %d: dcmclose: st %x fl %x\n",
			sc->sc_dev.dv_xname, port, tp->t_state, tp->t_flags);
#endif
	sc->sc_cua = 0;
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
	struct tty *tp;

	unit = DCMUNIT(dev);
	board = DCMBOARD(unit);
	port = DCMPORT(unit);

	sc = dcm_cd.cd_devs[board];
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
	struct tty *tp;

	unit = DCMUNIT(dev);
	board = DCMBOARD(unit);
	port = DCMPORT(unit);

	sc = dcm_cd.cd_devs[board];
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

	sc = dcm_cd.cd_devs[board];

	return (sc->sc_tty[port]);
}

int
dcmintr(arg)
	void *arg;
{
	struct dcm_softc *sc = arg;
	struct dcmdevice *dcm = sc->sc_dcm;
	struct dcmischeme *dis = &sc->sc_scheme;
	int brd = sc->sc_dev.dv_unit;
	int code, i;
	int pcnd[4], mcode, mcnd[4];

	/*
	 * Do all guarded accesses right off to minimize
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
		       sc->sc_dev.dv_xname, code, pcnd[0], pcnd[1],
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
void
dcmpint(sc, port, code)
	struct dcm_softc *sc;
	int port, code;
{

	if (code & IT_SPEC)
		dcmreadbuf(sc, port);
	if (code & IT_TX)
		dcmxint(sc, port);
}

void
dcmrint(sc)
	struct dcm_softc *sc;
{
	int port;

	for (port = 0; port < NDCMPORT; port++)
		dcmreadbuf(sc, port);
}

void
dcmreadbuf(sc, port)
	struct dcm_softc *sc;
	int port;
{
	struct dcmdevice *dcm = sc->sc_dcm;
	struct dcmpreg *pp = dcm_preg(dcm, port);
	struct dcmrfifo *fifo;
	struct tty *tp;
	int c, stat;
	u_int head;
	int nch = 0;
#ifdef DCMSTATS
	struct dcmstats *dsp = &sc->sc_stats;

	dsp->rints++;
#endif
	tp = sc->sc_tty[port];
	if (tp == NULL)
		return;

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
			       sc->sc_dev.dv_xname, port,
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
				       sc->sc_dev.dv_xname, port,
				       stat, c&0xFF, c);
#endif
			if (stat & (RD_BD | RD_FE))
				c |= TTY_FE;
			else if (stat & RD_PE)
				c |= TTY_PE;
			else if (stat & RD_OVF)
				log(LOG_WARNING,
				    "%s port %d: silo overflow\n",
				    sc->sc_dev.dv_xname, port);
			else if (stat & RD_OE)
				log(LOG_WARNING,
				    "%s port %d: uart overflow\n",
				    sc->sc_dev.dv_xname, port);
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

void
dcmxint(sc, port)
	struct dcm_softc *sc;
	int port;
{
	struct tty *tp;

	tp = sc->sc_tty[port];
	if (tp == NULL || (tp->t_state & TS_ISOPEN) == 0)
		return;

	tp->t_state &= ~TS_BUSY;
	if (tp->t_state & TS_FLUSH)
		tp->t_state &= ~TS_FLUSH;
	(*linesw[tp->t_line].l_start)(tp);
}

void
dcmmint(sc, port, mcnd)
	struct dcm_softc *sc;
	int port, mcnd;
{
	int delta;
	struct tty *tp;
	struct dcmdevice *dcm = sc->sc_dcm;

	tp = sc->sc_tty[port];
	if (tp == NULL || (tp->t_state & TS_ISOPEN) == 0)
		return;

#ifdef DEBUG
	if (dcmdebug & DDB_MODEM)
		printf("%s port %d: dcmmint: mcnd %x mcndlast %x\n",
		       sc->sc_dev.dv_xname, port, mcnd, sc->sc_mcndlast[port]);
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
	u_long cmd;
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

	sc = dcm_cd.cd_devs[board];
	dcm = sc->sc_dcm;
	tp = sc->sc_tty[port];

#ifdef DEBUG
	if (dcmdebug & DDB_IOCTL)
		printf("%s port %d: dcmioctl: cmd %lx data %x flag %x\n",
		       sc->sc_dev.dv_xname, port, cmd, *data, flag);
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

		error = suser(p, 0);
		if (error)
			return (EPERM);

		userbits = *(int *)data;

		if ((userbits & TIOCFLAG_SOFTCAR) ||
		    ((sc->sc_flags & DCM_ISCONSOLE) &&
		    (port == DCMCONSPORT)))
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
	struct tty *tp;
	struct termios *t;
{
	struct dcm_softc *sc;
	struct dcmdevice *dcm;
	int unit, board, port, mode, cflag = t->c_cflag;
	int ospeed = ttspeedtab(t->c_ospeed, dcmspeedtab);

	unit = DCMUNIT(tp->t_dev);
	board = DCMBOARD(unit);
	port = DCMPORT(unit);

	sc = dcm_cd.cd_devs[board];
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
		       sc->sc_dev.dv_xname, port, cflag, mode, tp->t_ospeed,
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
	struct tty *tp;
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

	sc = dcm_cd.cd_devs[board];
	dcm = sc->sc_dcm;

	s = spltty();
#ifdef DCMSTATS
	dsp->xints++;
#endif
#ifdef DEBUG
	if (dcmdebug & DDB_OUTPUT)
		printf("%s port %d: dcmstart: state %x flags %x outcc %d\n",
		       sc->sc_dev.dv_xname, port, tp->t_state, tp->t_flags,
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
		printf("%s port %d: dcmstart: head %x tail %x outqcc %d\n",
		    sc->sc_dev.dv_xname, port, head, tail, tp->t_outq.c_cc);
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
	struct tty *tp;
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
	return (0);
}

/*
 * Modem control
 */
int
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

	sc = dcm_cd.cd_devs[brd];
	dcm = sc->sc_dcm;

#ifdef DEBUG
	if (dcmdebug & DDB_MODEM)
		printf("%s port %d: dcmmctl: bits 0x%x how %x\n",
		       sc->sc_dev.dv_xname, port, bits, how);
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
		splx(s);
	}
	return (bits);
}

/*
 * Set board to either interrupt per-character or at a fixed interval.
 */
void
dcmsetischeme(brd, flags)
	int brd, flags;
{
	struct dcm_softc *sc = dcm_cd.cd_devs[brd];
	struct dcmdevice *dcm = sc->sc_dcm;
	struct dcmischeme *dis = &sc->sc_scheme;
	int i;
	u_char mask;
	int perchar = flags & DIS_PERCHAR;

#ifdef DEBUG
	if (dcmdebug & DDB_INTSCHM)
		printf("%s: dcmsetischeme(%d): cur %d, ints %d, chars %d\n",
		       sc->sc_dev.dv_xname, perchar, dis->dis_perchar,
		       dis->dis_intr, dis->dis_char);
	if ((flags & DIS_RESET) == 0 && perchar == dis->dis_perchar) {
		printf("%s: dcmsetischeme: redundant request %d\n",
		       sc->sc_dev.dv_xname, perchar);
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
		struct tty *tp;
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

void
dcminit(dcm, port, rate)
	struct dcmdevice *dcm;
	int port, rate;
{
	int s, mode;

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

/*
 * Empirically derived self-test magic
 */
int
dcmselftest(sc)
	struct dcm_softc *sc;
{
	struct dcmdevice *dcm = sc->sc_dcm;
	int timo = 0;
	int s, rv;

	rv = 1;

	s = splhigh();
	dcm->dcm_rsid = DCMRS;
	DELAY(50000);	/* 5000 is not long enough */
	dcm->dcm_rsid = 0;
	dcm->dcm_ic = IC_IE;
	dcm->dcm_cr = CR_SELFT;
	while ((dcm->dcm_ic & IC_IR) == 0) {
		if (++timo == 20000)
			goto out;
		DELAY(1);
	}
	DELAY(50000);	/* XXX why is this needed ???? */
	while ((dcm->dcm_iir & IIR_SELFT) == 0) {
		if (++timo == 400000)
			goto out;
		DELAY(1);
	}
	DELAY(50000);	/* XXX why is this needed ???? */
	if (dcm->dcm_stcon != ST_OK) {
#if 0
		if (hd->hp_args->hw_sc != conscode)
			printf("dcm%d: self test failed: %x\n",
			       brd, dcm->dcm_stcon);
#endif
		goto out;
	}
	dcm->dcm_ic = IC_ID;
	rv = 0;

 out:
	splx(s);
	return (rv);
}

/*
 * Following are all routines needed for DCM to act as console
 */

int
dcm_console_scan(scode, va, arg)
	int scode;
	caddr_t va;
	void *arg;
{
	struct dcmdevice *dcm = (struct dcmdevice *)va;
	struct consdev *cp = arg;
	int force = 0, pri;

	switch (dcm->dcm_rsid) {
	case DCMID:
		pri = CN_NORMAL;
		break;

	case DCMID|DCMCON:
		pri = CN_REMOTE;
		break;

	default:
		return (0);
	}

#ifdef CONSCODE
	/*
	 * Raise our priority, if appropriate.
	 */
	if (scode == CONSCODE) {
		pri = CN_REMOTE;
		force = conforced = 1;
	}
#endif

	/* Only raise priority. */
	if (pri > cp->cn_pri)
		cp->cn_pri = pri;

	/*
	 * If our priority is higher than the currently-remembered
	 * console, stash our priority, for the benefit of dcmcninit().
	 */
	if (((cn_tab == NULL) || (cp->cn_pri > cn_tab->cn_pri)) || force) {
		cn_tab = cp;
		return (DIO_SIZE(scode, va));
	}
	return (0);
}

void
dcmcnprobe(cp)
	struct consdev *cp;
{

	/* locate the major number */
	for (dcmmajor = 0; dcmmajor < nchrdev; dcmmajor++)
		if (cdevsw[dcmmajor].d_open == dcmopen)
			break;

	/* initialize required fields */
	cp->cn_dev = makedev(dcmmajor, 0);	/* XXX */
	cp->cn_pri = CN_DEAD;

	/* Abort early if console already forced. */
	if (conforced)
		return;

	console_scan(dcm_console_scan, cp, HP300_BUS_DIO);

#ifdef KGDB_CHEAT
	/* XXX this needs to be fixed. */
	/*
	 * This doesn't currently work, at least not with ite consoles;
	 * the console hasn't been initialized yet.
	 */
	if (major(kgdb_dev) == dcmmajor &&
	    DCMBOARD(DCMUNIT(kgdb_dev)) == DCMBOARD(unit)) {
		dcminit(dcm_cn, DCMPORT(DCMUNIT(kgdb_dev)), kgdb_rate);
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

/* ARGSUSED */
void
dcmcninit(cp)
	struct consdev *cp;
{

	dcm_cn = (struct dcmdevice *)conaddr;
	dcminit(dcm_cn, DCMCONSPORT, dcmdefaultrate);
	dcmconsinit = 1;
}

/* ARGSUSED */
int
dcmcngetc(dev)
	dev_t dev;
{
	struct dcmrfifo *fifo;
	struct dcmpreg *pp;
	u_int head;
	int s, c, stat;

	pp = dcm_preg(dcm_cn, DCMCONSPORT);

	s = splhigh();
	head = pp->r_head & RX_MASK;
	fifo = &dcm_cn->dcm_rfifos[3-DCMCONSPORT][head>>1];
	while (head == (pp->r_tail & RX_MASK))
		;
	/*
	 * If board interrupts are enabled, just let our received char
	 * interrupt through in case some other port on the board was
	 * busy.  Otherwise we must clear the interrupt.
	 */
	SEM_LOCK(dcm_cn);
	if ((dcm_cn->dcm_ic & IC_IE) == 0)
		stat = dcm_cn->dcm_iir;
	SEM_UNLOCK(dcm_cn);
	c = fifo->data_char;
	stat = fifo->data_stat;
	pp->r_head = (head + 2) & RX_MASK;
	splx(s);
	return (c);
}

/*
 * Console kernel output character routine.
 */
/* ARGSUSED */
void
dcmcnputc(dev, c)
	dev_t dev;
	int c;
{
	struct dcmpreg *pp;
	unsigned tail;
	int s, stat;

	pp = dcm_preg(dcm_cn, DCMCONSPORT);

	s = splhigh();
#ifdef KGDB
	if (dev != kgdb_dev)
#endif
	if (dcmconsinit == 0) {
		dcminit(dcm_cn, DCMCONSPORT, dcmdefaultrate);
		dcmconsinit = 1;
	}
	tail = pp->t_tail & TX_MASK;
	while (tail != (pp->t_head & TX_MASK))
		;
	dcm_cn->dcm_tfifos[3-DCMCONSPORT][tail].data_char = c;
	pp->t_tail = tail = (tail + 1) & TX_MASK;
	SEM_LOCK(dcm_cn);
	dcm_cn->dcm_cmdtab[DCMCONSPORT].dcm_data |= CT_TX;
	dcm_cn->dcm_cr |= (1 << DCMCONSPORT);
	SEM_UNLOCK(dcm_cn);
	while (tail != (pp->t_head & TX_MASK))
		;
	/*
	 * If board interrupts are enabled, just let our completion
	 * interrupt through in case some other port on the board
	 * was busy.  Otherwise we must clear the interrupt.
	 */
	if ((dcm_cn->dcm_ic & IC_IE) == 0) {
		SEM_LOCK(dcm_cn);
		stat = dcm_cn->dcm_iir;
		SEM_UNLOCK(dcm_cn);
	}
	splx(s);
}

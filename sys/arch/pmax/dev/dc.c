/*	$NetBSD: dc.c,v 1.30 1996/10/16 02:10:33 jonathan Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	@(#)dc.c	8.5 (Berkeley) 6/2/95
 */

/*
 * devDC7085.c --
 *
 *     	This file contains machine-dependent routines that handle the
 *	output queue for the serial lines.
 *
 *	Copyright (C) 1989 Digital Equipment Corporation.
 *	Permission to use, copy, modify, and distribute this software and
 *	its documentation for any purpose and without fee is hereby granted,
 *	provided that the above copyright notice appears in all copies.
 *	Digital Equipment Corporation makes no representations about the
 *	suitability of this software for any purpose.  It is provided "as is"
 *	without express or implied warranty.
 *
 * from: Header: /sprite/src/kernel/dev/ds3100.md/RCS/devDC7085.c,
 *	v 1.4 89/08/29 11:55:30 nelson Exp  SPRITE (DECWRL)";
 */

/*
 * DC7085 (DZ-11 look alike) Driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/map.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <machine/conf.h>
#include <sys/device.h>
#include <machine/autoconf.h>
#include <dev/tc/tcvar.h>
#include <dev/tc/ioasicvar.h>

#include <machine/dc7085cons.h>
#include <machine/pmioctl.h>

#include <pmax/pmax/pmaxtype.h>
#include <pmax/pmax/cons.h>

#include <pmax/dev/lk201.h>

/*
 * XXX in dcvar.h or not?
 * #include <pmax/dev/pdma.h>
 */
#include "dcvar.h"

#include "tc.h"

#include <pmax/dev/lk201var.h>		/* XXX KbdReset band friends */

#include <pmax/dev/dcvar.h>
#include <pmax/dev/dc_cons.h>

extern struct cfdriver mainbus_cd;

#define DCUNIT(dev) (minor(dev) >> 2)
#define DCLINE(dev) (minor(dev) & 3)

/*
 * Autoconfiguration data for config.
 * 
 * Use the statically-allocated softc until old autoconfig code and
 * config.old are completely gone.
 */
int	old_dcmatch  __P((struct device * parent, void *cfdata, void *aux));
void	old_dcattach __P((struct device *parent, struct device *self, void *aux));


extern struct cfdriver dc_cd;
struct  cfdriver dc_cd = {
	NULL, "dc", DV_TTY
};



/*
 * Forward declarations
 */
struct tty *dctty __P((dev_t  dev));
void dcstart	__P((struct tty *));
void dcrint	 __P((struct dc_softc *sc));
void dcxint	__P((struct tty *));
int dcmctl	 __P((dev_t dev, int bits, int how));
void dcscan	__P((void *));
int dcparam	__P((struct tty *tp, struct termios *t));
static int cold_dcparam __P((struct tty *tp, struct termios *t, 
		      dcregs *dcaddr, int allow_19200));

extern void ttrstrt __P((void *));

void	dc_reset __P ((dcregs *dcaddr));

/* console I/O */
int  dcGetc	__P((dev_t));
void dcPutc	__P((dev_t, int));
void dcPollc	__P((dev_t, int));
void dc_consinit __P((dev_t dev, dcregs *dcaddr));


/* QVSS-compatible in-kernel X input event parser, pointer tracker */
void	(*dcDivertXInput) __P((int cc)); /* X windows keyboard input routine */
void	(*dcMouseEvent) __P((int));	/* X windows mouse motion event routine */
void	(*dcMouseButtons) __P((int));	/* X windows mouse buttons event routine */
#ifdef DEBUG
int	debugChar;
#endif


/*
 * The DC7085 doesn't interrupt on carrier transitions, so
 * we have to use a timer to watch it.
 */
int	dc_timer;		/* true if timer started */

/*
 * Pdma structures for fast output code
 */

struct speedtab dcspeedtab[] = {
	{ 0,	0,	},
	{ 50,	LPR_B50    },
	{ 75,	LPR_B75    },
	{ 110,	LPR_B110   },
	{ 134,	LPR_B134   },
	{ 150,	LPR_B150   },
	{ 300,	LPR_B300   },
	{ 600,	LPR_B600   },
	{ 1200,	LPR_B1200  },
	{ 1800,	LPR_B1800  },
	{ 2400,	LPR_B2400  },
	{ 4800,	LPR_B4800  },
	{ 9600,	LPR_B9600  },
	{ 19200,LPR_B19200 },
#ifdef notyet
	{ 19200,LPR_B38400 },	/* Overloaded with 19200, per chip. */
#endif
	{ -1,	-1 }
};

#ifndef	PORTSELECTOR
#define	ISPEED	TTYDEF_SPEED
#define	LFLAG	TTYDEF_LFLAG
#else
#define	ISPEED	B4800
#define	LFLAG	(TTYDEF_LFLAG & ~ECHO)
#endif

/*
 * Console line variables, for use when cold
 */
extern int cold;
dcregs *dc_cons_addr = 0;

/*
 * Is there a framebuffer console device using this serial driver?
 * XXX used for ugly special-cased console input that should be redone
 * more cleanly.
 */
static inline int raster_console __P((void));

static inline int
raster_console()
{
	return (cn_tab->cn_pri == CN_INTERNAL ||
		cn_tab->cn_pri == CN_NORMAL);
}


/* XXX move back into dc_consinit when debugged */
static struct consdev dccons = {
	NULL, NULL, dcGetc, dcPutc, dcPollc, NODEV, CN_REMOTE
};

/*
 * Special-case code to attach a console.
 * We were using PROM callbacks for console I/O,
 * and we just reset the chip under the console.
 * wire up this driver as console ASAP.
 *
 * Must be called at spltty() or higher.
 */
void
dc_consinit(dev, dcaddr)
	dev_t dev;
	register dcregs *dcaddr;
{
  	struct termios cterm;
  	struct tty ctty;

	/* save address in case we're cold */
	if (cold && dc_cons_addr == 0)
		dc_cons_addr = dcaddr;

	/* reset chip */
	dc_reset(dcaddr);

	dcaddr->dc_lpr = LPR_RXENAB | LPR_8_BIT_CHAR |
		LPR_B9600 | DCLINE(dev);
	wbflush();
	DELAY(10);

	bzero(&cterm, sizeof(cterm));
	bzero(&ctty, sizeof(ctty));
	ctty.t_dev = dev;
	dccons.cn_dev = dev;
	cterm.c_cflag |= CLOCAL;
	cterm.c_cflag = CS8;
	cterm.c_ospeed = 9600;
	*cn_tab = dccons;
	cold_dcparam(&ctty, &cterm, dcaddr, 0); /* XXX untested */
}


/*
 * Attach DC7085 (dz-11) device.
 */
int
dcattach(sc, addr, dtr_mask, rtscts_mask, speed,
	   console_line)
	register struct dc_softc *sc;
	void *addr;
	int dtr_mask, rtscts_mask, speed, console_line;
{
	register dcregs *dcaddr;
	register struct pdma *pdp;
	register struct tty *tp;
	register int line;
	int s;
	
	dcaddr = (dcregs *)addr;

	/*
	 * For a remote console, wait a while for previous output to
	 * complete.
	 * XXX both cn_dev == 0 and cn_pri == CN_DEAD are bug workarounds.
	 * The interface between ttys and cpu_cons.c should be reworked.
	 */
	if (sc->sc_dv.dv_unit == 0 &&	/* XXX why only unit 0? */
	    (major(cn_tab->cn_dev) == DCDEV || major(cn_tab->cn_dev) == 0) &&
	    (cn_tab->cn_pri == CN_REMOTE || (cn_tab->cn_pri == CN_DEAD))) {
		DELAY(10000);
	}
	/* reset chip and enable interrupts */
	dc_reset(dcaddr);
	dcaddr->dc_csr |= (CSR_MSE | CSR_TIE | CSR_RIE);

	/* init pseudo DMA structures */
	pdp = &sc->dc_pdma[0];
	for (line = 0; line < 4; line++) {
		pdp->p_addr = (void *)dcaddr;
		tp = sc->dc_tty[line] = ttymalloc();
		if (line != DCKBD_PORT && line != DCMOUSE_PORT)
			tty_attach(tp);
		tp->t_dev = makedev(DCDEV, 4 * sc->sc_dv.dv_unit + line);
		pdp->p_arg = (int) tp;
		pdp->p_fcn = dcxint;
		pdp++;
	}
	sc->dcsoftCAR = sc->sc_dv.dv_cfdata->cf_flags | 0xB;

	if (dc_timer == 0) {
		dc_timer = 1;
		timeout(dcscan, (void *)0, hz);
	}

	sc->dc_19200 = speed;
	sc->dc_modem = dtr_mask;
	sc->dc_rtscts = rtscts_mask;


	/*
	 * Special handling for consoles.
	 */
	if (sc->sc_dv.dv_unit == 0) {
		if (raster_console()) {
			s = spltty();
			dcaddr->dc_lpr = LPR_RXENAB | LPR_8_BIT_CHAR |
				LPR_B4800 | DCKBD_PORT;
			wbflush();
			dcaddr->dc_lpr = LPR_RXENAB | LPR_B4800 | LPR_OPAR |
				LPR_PARENB | LPR_8_BIT_CHAR | DCMOUSE_PORT;
			wbflush();
			DELAY(1000);
			KBDReset(makedev(DCDEV, DCKBD_PORT), dcPutc);
			MouseInit(makedev(DCDEV, DCMOUSE_PORT), dcPutc, dcGetc);
			splx(s);
		}
		else if (major(cn_tab->cn_dev) == DCDEV) {
			s = spltty();
			dc_consinit(cn_tab->cn_dev, dcaddr);
			dcaddr->dc_csr |= (CSR_MSE | CSR_TIE | CSR_RIE);
			splx(s);
		}
	}
	return (1);
}


/*
 * Reset chip.  Does not change modem control output bits
 * or modem state register.
 * Does not enable interrupts; caller must explicitly or
 * TIE and RIE on if desired (XXX not true yet)
 */
void
dc_reset(dcaddr)
	register dcregs *dcaddr;
{
	/* Reset CSR and wait until cleared. */
	dcaddr->dc_csr = CSR_CLR;
	wbflush();
	DELAY(10);
	while (dcaddr->dc_csr & CSR_CLR)
		;

	/* Enable scanner. */
	dcaddr->dc_csr = CSR_MSE;
	wbflush();
	DELAY(10);
}


int
dcopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	register struct tty *tp;
	register struct dc_softc *sc;
	register int unit, line;
	int s, error = 0;

	unit = DCUNIT(dev);
	line = DCLINE(dev);
	if (unit >= dc_cd.cd_ndevs || line > 4)
		return (ENXIO);

	sc = dc_cd.cd_devs[unit];
	if (sc->dc_pdma[line].p_addr == (void *)0)
		return (ENXIO);	  

	tp = sc->dc_tty[line];
	if (tp == NULL) {
		tp = sc->dc_tty[line] = ttymalloc();
		tty_attach(tp);
	}
	tp->t_oproc = dcstart;
	tp->t_param = dcparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
#ifndef PORTSELECTOR
		if (tp->t_ispeed == 0) {
#endif
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_lflag = LFLAG;
			tp->t_ispeed = tp->t_ospeed = ISPEED;
#ifdef PORTSELECTOR
			tp->t_cflag |= HUPCL;
#else
		}
#endif
		(void) dcparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if ((tp->t_state & TS_XCLUDE) && curproc->p_ucred->cr_uid != 0)
		return (EBUSY);
#ifdef HW_FLOW_CONTROL
	(void) dcmctl(dev, DML_DTR | DML_RTS, DMSET);
#else
	(void) dcmctl(dev, DML_DTR, DMSET);
#endif
	if ((sc->dcsoftCAR & (1 << line)) ||
	    (dcmctl(dev, 0, DMGET) & DML_CAR))
		tp->t_state |= TS_CARR_ON;
	s = spltty();
	while (!(flag & O_NONBLOCK) && !(tp->t_cflag & CLOCAL) &&
	       !(tp->t_state & TS_CARR_ON)) {
		tp->t_state |= TS_WOPEN;
		if ((error = ttysleep(tp, (caddr_t)&tp->t_rawq,
				      TTIPRI | PCATCH, ttopen, 0)) != 0)
			break;
	}
	splx(s);
	if (error)
		return (error);
	return ((*linesw[tp->t_line].l_open)(dev, tp));
}

/*ARGSUSED*/
int
dcclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	register struct dc_softc *sc;
	register struct tty *tp;
	register int line, bit;
	int s;

	sc = dc_cd.cd_devs[DCUNIT(dev)];
	line = DCLINE(dev);
	tp = sc->dc_tty[line];
	bit = 1 << (line + 8);
	s = spltty();
	/* turn off the break bit if it is set */
	if (sc->dc_brk & bit) {
		sc->dc_brk &= ~bit;
		ttyoutput(0, tp);
	}
	splx(s);
	(*linesw[tp->t_line].l_close)(tp, flag);
	if ((tp->t_cflag & HUPCL) || (tp->t_state & TS_WOPEN) ||
	    !(tp->t_state & TS_ISOPEN))
		(void) dcmctl(dev, 0, DMSET);
	return (ttyclose(tp));
}

int
dcread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
{
	register struct dc_softc *sc;
	register struct tty *tp;

	sc = dc_cd.cd_devs[DCUNIT(dev)];
	tp = sc->dc_tty[DCLINE(dev)];

#ifdef HW_FLOW_CONTROL
	if ((tp->t_cflag & CRTS_IFLOW) && (tp->t_state & TS_TBLOCK) &&
	    tp->t_rawq.c_cc < TTYHOG/5) {
		tp->t_state &= ~TS_TBLOCK;
		(void) dcmctl(dev, DML_RTS, DMBIS);
	}
#endif /* HW_FLOW_CONTROL */

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
dcwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
{
	register struct dc_softc *sc;
	register struct tty *tp;

	sc = dc_cd.cd_devs[DCUNIT(dev)];
	tp = sc->dc_tty[DCLINE(dev)];
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
dctty(dev)
        dev_t dev;
{
	register struct dc_softc *sc;
	register struct tty *tp;

	sc = dc_cd.cd_devs[DCUNIT(dev)];
	tp = sc->dc_tty[DCLINE(dev)];
        return (tp);
}

/*ARGSUSED*/
int
dcioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register struct dc_softc *sc;
	register struct tty *tp;
	register int unit;
	register int line;
	int error;


	unit = DCUNIT(dev);
	line = DCLINE(dev);
	sc = dc_cd.cd_devs[unit];
	tp = sc->dc_tty[line];

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	switch (cmd) {

	case TIOCSBRK:
		sc->dc_brk |= 1 << (line + 8);
		ttyoutput(0, tp);
		break;

	case TIOCCBRK:
		sc->dc_brk &= ~(1 << (line + 8));
		ttyoutput(0, tp);
		break;

	case TIOCSDTR:
		(void) dcmctl(dev, DML_DTR|DML_RTS, DMBIS);
		break;

	case TIOCCDTR:
		(void) dcmctl(dev, DML_DTR|DML_RTS, DMBIC);
		break;

	case TIOCMSET:
		(void) dcmctl(dev, *(int *)data, DMSET);
		break;

	case TIOCMBIS:
		(void) dcmctl(dev, *(int *)data, DMBIS);
		break;

	case TIOCMBIC:
		(void) dcmctl(dev, *(int *)data, DMBIC);
		break;

	case TIOCMGET:
		*(int *)data = dcmctl(dev, 0, DMGET);
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

/*
 * Set line parameters
 */

int
dcparam(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	register struct dc_softc *sc;
	register dcregs *dcaddr;


	/*
	 * Extract softc data, and pass entire request onto
	 * cold_dcparam() for argument checking and execution.
	 */
	sc = dc_cd.cd_devs[DCUNIT(tp->t_dev)];
	dcaddr = (dcregs *)sc->dc_pdma[0].p_addr;
	return (cold_dcparam(tp, t, dcaddr, sc->dc_19200));

}

int
cold_dcparam(tp, t, dcaddr, allow_19200)
	register struct tty *tp;
	register struct termios *t;
	register dcregs *dcaddr;
	int allow_19200;
{
	register int lpr;
	register int cflag = t->c_cflag;
	int unit = minor(tp->t_dev);
	int ospeed = ttspeedtab(t->c_ospeed, dcspeedtab);
	int s;
	int line;

	line = DCLINE(tp->t_dev);

	/* check requested parameters */
        if (ospeed < 0 || (t->c_ispeed && t->c_ispeed != t->c_ospeed) ||
            (cflag & CSIZE) == CS5 || (cflag & CSIZE) == CS6 ||
	    (t->c_ospeed >= 19200 && allow_19200 != 1))
                return (EINVAL);
        /* and copy to tty */
        tp->t_ispeed = t->c_ispeed;
        tp->t_ospeed = t->c_ospeed;
        tp->t_cflag = cflag;

	/*
	 * Handle console cases specially.
	 */
	if (raster_console()) {
		if (unit == DCKBD_PORT) {
			lpr = LPR_RXENAB | LPR_8_BIT_CHAR |
				LPR_B4800 | DCKBD_PORT;
			goto out;
		} else if (unit == DCMOUSE_PORT) {
			lpr = LPR_RXENAB | LPR_B4800 | LPR_OPAR |
				LPR_PARENB | LPR_8_BIT_CHAR | DCMOUSE_PORT;
			goto out;
		}
	} else if (tp->t_dev == cn_tab->cn_dev) {
		lpr = LPR_RXENAB | LPR_8_BIT_CHAR | LPR_B9600 | line;
		goto out;
	}
	if (ospeed == 0) {
		(void) dcmctl(unit, 0, DMSET);	/* hang up line */
		return (0);
	}
	lpr = LPR_RXENAB | ospeed | line;
	if ((cflag & CSIZE) == CS7)
		lpr |= LPR_7_BIT_CHAR;
	else
		lpr |= LPR_8_BIT_CHAR;
	if (cflag & PARENB)
		lpr |= LPR_PARENB;
	if (cflag & PARODD)
		lpr |= LPR_OPAR;
	if (cflag & CSTOPB)
		lpr |= LPR_2_STOP;
out:
	s = spltty();
	dcaddr->dc_lpr = lpr;
	wbflush();
	splx(s);
	DELAY(10);
	return (0);
}


/*
 * Check for interrupts from all devices.
 */
int
dcintr(xxxunit)
	void *xxxunit;
{
	register struct dc_softc *sc = xxxunit;
	register dcregs *dcaddr;
	register unsigned csr;

	dcaddr = (dcregs *)sc->dc_pdma[0].p_addr;
	while ((csr = dcaddr->dc_csr) & (CSR_RDONE | CSR_TRDY)) {
		if (csr & CSR_RDONE)
			dcrint(sc);
		if (csr & CSR_TRDY)
			dcxint(sc->dc_tty[((csr >> 8) & 03)]);
	}
	/* XXX check for spurious interrupts */
	return 0;
}

void
dcrint(sc)
	register struct dc_softc * sc;
{
	register dcregs *dcaddr;
	register struct tty *tp;
	register int c, cc;
	int overrun = 0;
	register struct tty **dc_tty;

	dc_tty = ((struct dc_softc*)dc_cd.cd_devs[0])->dc_tty;	/* XXX */

	dcaddr = (dcregs *)sc->dc_pdma[0].p_addr;	/*XXX*/
	while ((c = dcaddr->dc_rbuf) < 0) {	/* char present */
		cc = c & 0xff;
		tp = sc->dc_tty[((c >> 8) & 03)];

		if ((c & RBUF_OERR) && overrun == 0) {
			log(LOG_WARNING, "%s,%d: silo overflow\n",
				sc->sc_dv.dv_xname,
				(c >> 8) & 03);
			overrun = 1;
		}
		/* the keyboard requires special translation */
		if (raster_console() && tp == dc_tty[DCKBD_PORT]) {
#ifdef KADB
			if (cc == LK_DO) {
				spl0();
				kdbpanic();
				return;
			}
#endif
#ifdef DEBUG
			debugChar = cc;
#endif
			if (dcDivertXInput) {
				(*dcDivertXInput)(cc);
				return;
			}
			if ((cc = kbdMapChar(cc)) < 0)
				return;
		} else if (tp == dc_tty[DCMOUSE_PORT] && dcMouseButtons) {
			mouseInput(cc);
			return;
		}
		if (!(tp->t_state & TS_ISOPEN)) {
			wakeup((caddr_t)&tp->t_rawq);
#ifdef PORTSELECTOR
			if (!(tp->t_state & TS_WOPEN))
#endif
				return;
		}
		if (c & RBUF_FERR)
			cc |= TTY_FE;
		if (c & RBUF_PERR)
			cc |= TTY_PE;
#ifdef HW_FLOW_CONTROL
		if ((tp->t_cflag & CRTS_IFLOW) && !(tp->t_state & TS_TBLOCK) &&
		    tp->t_rawq.c_cc + tp->t_canq.c_cc >= TTYHOG) {
			tp->t_state &= ~TS_TBLOCK;
			(void) dcmctl(tp->t_dev, DML_RTS, DMBIC);
		}
#endif /* HWW_FLOW_CONTROL */
		(*linesw[tp->t_line].l_rint)(cc, tp);
	}
	DELAY(10);
}

void
dcxint(tp)
	register struct tty *tp;
{
	register struct dc_softc *sc;
	register struct pdma *dp;
	register dcregs *dcaddr;
	int line, linemask;

	sc = dc_cd.cd_devs[DCUNIT(tp->t_dev)];	/* XXX */

	line = DCLINE(tp->t_dev);
	linemask = 1 << line;

	dp = &sc->dc_pdma[line];
	if (dp->p_mem < dp->p_end) {
		dcaddr = (dcregs *)dp->p_addr;

#ifdef HW_FLOW_CONTROL
		/* check for hardware flow control of output */
		if ((tp->t_cflag & CCTS_OFLOW) && (sc->dc_rtscts & linemask)) {
			switch (line) {
			case 2:
				if (dcaddr->dc_msr & MSR_CTS2)
					break;
				goto stop;

			case 3:
				if (dcaddr->dc_msr & MSR_CTS3)
					break;
			stop:
				tp->t_state &= ~TS_BUSY;
				tp->t_state |= TS_TTSTOP;
				ndflush(&tp->t_outq, dp->p_mem - 
						(caddr_t)tp->t_outq.c_cf);
				dp->p_end = dp->p_mem = tp->t_outq.c_cf;
				dcaddr->dc_tcr &= ~(1 << line);
				wbflush();
				DELAY(10);
				return;
			}
		}
#endif /* HW_FLOW_CONTROL */
		dcaddr->dc_tdr = sc->dc_brk | *(u_char *)dp->p_mem;
		dp->p_mem++;

		wbflush();
		DELAY(10);
		return;
	}
	tp->t_state &= ~TS_BUSY;
	if (tp->t_state & TS_FLUSH)
		tp->t_state &= ~TS_FLUSH;
	else {
		ndflush(&tp->t_outq, dp->p_mem - (caddr_t) tp->t_outq.c_cf);
		dp->p_end = dp->p_mem = tp->t_outq.c_cf;
	}
	if (tp->t_line)
		(*linesw[tp->t_line].l_start)(tp);
	else
		dcstart(tp);
	if (tp->t_outq.c_cc == 0 || !(tp->t_state & TS_BUSY)) {
		dcaddr = (dcregs *)dp->p_addr;
		dcaddr->dc_tcr &= ~(1 << line);
		wbflush();
		DELAY(10);
	}
}

void
dcstart(tp)
	register struct tty *tp;
{
	register struct dc_softc *sc;
	register struct pdma *dp;
	register dcregs *dcaddr;
	register int cc;
	int line, s;

	sc = dc_cd.cd_devs[DCUNIT(tp->t_dev)];
	line = DCLINE(tp->t_dev);
	dp = &sc->dc_pdma[line];
	dcaddr = (dcregs *)dp->p_addr;
	s = spltty();
	if (tp->t_state & (TS_TIMEOUT|TS_BUSY|TS_TTSTOP))
		goto out;
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
	if (tp->t_outq.c_cc == 0)
		goto out;
	/* handle console specially */
	if (raster_console() && tp == sc->dc_tty[DCKBD_PORT]) {
		while (tp->t_outq.c_cc > 0) {
			cc = getc(&tp->t_outq) & 0x7f;
			cnputc(cc);
		}
		/*
		 * After we flush the output queue we may need to wake
		 * up the process that made the output.
		 */
		if (tp->t_outq.c_cc <= tp->t_lowat) {
			if (tp->t_state & TS_ASLEEP) {
				tp->t_state &= ~TS_ASLEEP;
				wakeup((caddr_t)&tp->t_outq);
			}
			selwakeup(&tp->t_wsel);
		}
		goto out;
	}
  	cc = ndqb(&tp->t_outq, 0);
	if (cc == 0)
		goto out;
	tp->t_state |= TS_BUSY;
	dp->p_end = dp->p_mem = tp->t_outq.c_cf;
	dp->p_end += cc;
	dcaddr->dc_tcr |= 1 << line;
	wbflush();
out:
	splx(s);
}

/*
 * Stop output on a line.
 */
/*ARGSUSED*/
int /* was void TTTTT */
dcstop(tp, flag)
	register struct tty *tp;
{
	register struct dc_softc *sc;
	register struct pdma *dp;
	register int s;

	sc = dc_cd.cd_devs[DCUNIT(tp->t_dev)];
	dp = &sc->dc_pdma[DCLINE(tp->t_dev)];
	s = spltty();
	if (tp->t_state & TS_BUSY) {
		dp->p_end = dp->p_mem;
		if (!(tp->t_state & TS_TTSTOP))
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
}

int
dcmctl(dev, bits, how)
	dev_t dev;
	int bits, how;
{
	register struct dc_softc *sc;
	register dcregs *dcaddr;
	register int line, mbits;
	int b, s;
	register int tcr, msr;

	line = DCLINE(dev);
	sc = dc_cd.cd_devs[DCUNIT(dev)];
	b = 1 << line;
	dcaddr = (dcregs *)sc->dc_pdma[line].p_addr;
	s = spltty();
	/* only channel 2 has modem control on a DECstation 2100/3100 */
	mbits = DML_DTR | DML_DSR | DML_CAR;
#ifdef HW_FLOW_CONTROL
	mbits != DML_RTS;
#endif /* HW_FLOW_CONTROL */
	switch (line) {
	case  2:  /* pmax partial-modem comms port, full-modem port on 3max */
		mbits = 0;
		tcr = dcaddr->dc_tcr;
		if (tcr & TCR_DTR2)
			mbits |= DML_DTR;
		if ((sc->dc_rtscts & (1<<line)) && (tcr & TCR_RTS2))
			mbits |= DML_RTS;
		msr = dcaddr->dc_msr;
		if (msr & MSR_CD2)
			mbits |= DML_CAR;
		if (msr & MSR_DSR2) {
			/*
			 * XXX really tests for DS_PMAX instead of DS_3MAX
			 * but close enough for now.  Vaxes?
			 */
			if ((sc->dc_rtscts & (1 << line )) == 0 &&
			    (sc->dc_modem & (1 << line )))
				mbits |= DML_CAR | DML_DSR;
			else
				mbits |= DML_DSR;
		}
		break;

	case 3: /* no modem control on pmax, console port on 3max */
	  	/*
		 * XXX really tests for DS_3MAX instead of DS_PMAX
		 * but close enough for now.  Vaxes?
		 */
		if ( sc->dc_modem & (1 << line )) {
			mbits = 0;
			tcr = dcaddr->dc_tcr;
			if (tcr & TCR_DTR3)
				mbits |= DML_DTR;
#ifdef HW_FLOW_CONTROL
			/* XXX OK for get, but not for set? */
			/*if ( sc->dc_rtscts & (1 << line ))*/
			if (tcr & TCR_RTS3)
				mbits |= DML_RTS;
#endif /*HW_FLOW_CONTROL*/
			msr = dcaddr->dc_msr;
			if (msr & MSR_CD3)
				mbits |= DML_CAR;
			if (msr & MSR_DSR3)
				mbits |= DML_DSR;
		}
	}
	switch (how) {
	case DMSET:
		mbits = bits;
		break;

	case DMBIS:
		mbits |= bits;
		break;

	case DMBIC:
		mbits &= ~bits;
		break;

	case DMGET:
		(void) splx(s);
		return (mbits);
	}
	switch (line) {
	case  2: /* 2 */
		tcr = dcaddr->dc_tcr;
		if (mbits & DML_DTR)
			tcr |= TCR_DTR2;
		else
			tcr &= ~TCR_DTR2;
		/*if (pmax_boardtype != DS_PMAX)*/
		if (sc->dc_rtscts & (1 << line)) {
			if (mbits & DML_RTS)
				tcr |= TCR_RTS2;
			else
				tcr &= ~TCR_RTS2;
		}
		dcaddr->dc_tcr = tcr;
		break;

	case 3:
		/* XXX DTR not supported on this line on 2100/3100 */
		/*if (pmax_boardtype != DS_PMAX)*/
		if (sc->dc_modem & (1 << line)) {
			tcr = dcaddr->dc_tcr;
			if (mbits & DML_DTR)
				tcr |= TCR_DTR3;
			else
				tcr &= ~TCR_DTR3;
#ifdef HW_FLOW_CONTROL
		/*if (sc->dc_rtscts & (1 << line))*/
			if (mbits & DML_RTS)
				tcr |= TCR_RTS3;
			else
				tcr &= ~TCR_RTS3;
#endif /* HW_FLOW_CONTROL */
			dcaddr->dc_tcr = tcr;
		}
	}
	(void) splx(s);
	return (mbits);
}

/*
 * This is called by timeout() periodically.
 * Check to see if modem status bits have changed.
 */
void
dcscan(arg)
	void *arg;
{
	register struct dc_softc *sc = dc_cd.cd_devs[0]; /* XXX */
	register dcregs *dcaddr;
	register struct tty *tp;
	register int unit, limit, dtr, dsr;
	int s;

	/* only channel 2 has modem control on a DECstation 2100/3100 */
	dtr = TCR_DTR2;
	dsr = MSR_DSR2;
#ifdef HW_FLOW_CONTROL
	/*limit = (pmax_boardtype == DS_PMAX) ? 2 : 3;*/
	limit =  (sc->dc_rtscts & (1 << 3)) :3  : 2;	/*XXX*/
#else
	limit = 2;
#endif
	s = spltty();
	for (unit = 2; unit <= limit; unit++, dtr >>= 2, dsr >>= 8) {
		tp = sc->dc_tty[unit];
		dcaddr = (dcregs *)sc->dc_pdma[unit].p_addr;
		if ((dcaddr->dc_msr & dsr) || (sc->dcsoftCAR & (1 << unit))) {
			/* carrier present */
			if (!(tp->t_state & TS_CARR_ON))
				(void)(*linesw[tp->t_line].l_modem)(tp, 1);
		} else if ((tp->t_state & TS_CARR_ON) &&
		    (*linesw[tp->t_line].l_modem)(tp, 0) == 0)
			dcaddr->dc_tcr &= ~dtr;
#ifdef HW_FLOW_CONTROL
		/*
		 * If we are using hardware flow control and output is stopped,
		 * then resume transmit.
		 */
		if ((tp->t_cflag & CCTS_OFLOW) && (tp->t_state & TS_TTSTOP) &&
		     /*pmax_boardtype != DS_PMAX*/
		    (sc->dc_rtscts & (1 << unit)) ) {
			switch (unit) {
			case 2:
				if (dcaddr->dc_msr & MSR_CTS2)
					break;
				continue;

			case 3:
				if (dcaddr->dc_msr & MSR_CTS3)
					break;
				continue;
			}
			tp->t_state &= ~TS_TTSTOP;
			dcstart(tp);
		}
#endif /* HW_FLOW_CONTROL */
	}
	splx(s);
	timeout(dcscan, (void *)0, hz);
}

/*
 * ----------------------------------------------------------------------------
 *
 * dcGetc --
 *
 *	Read a character from a serial line.
 *
 * Results:
 *	A character read from the serial port.
 *
 * Side effects:
 *	None.
 *
 * ----------------------------------------------------------------------------
 */
int
dcGetc(dev)
	dev_t dev;
{
	register dcregs *dcaddr;
	register int c;
	register int line;
	int s;

	line = DCLINE(dev);
	if (cold && dc_cons_addr) {
		dcaddr = dc_cons_addr;
	} else {
		struct dc_softc *sc;
		sc = dc_cd.cd_devs[DCUNIT(dev)];
		dcaddr = (dcregs *)sc->dc_pdma[line].p_addr;
	}
	if (!dcaddr)
		return (0);
	s = spltty();
	for (;;) {
		if (!(dcaddr->dc_csr & CSR_RDONE))
			continue;
		c = dcaddr->dc_rbuf;
		DELAY(10);
		if (((c >> 8) & 03) == line)
			break;
	}
	splx(s);
	return (c & 0xff);
}

/*
 * Send a char on a port, non interrupt driven.
 */
void
dcPutc(dev, c)
	dev_t dev;
	int c;
{
	register dcregs *dcaddr;
	register u_short tcr;
	register int timeout;
	int s, out_line, activeline;
	int brk;

	s = spltty();
	out_line = DCLINE(dev);
	if (cold && dc_cons_addr) {
		brk = 0;
		dcaddr = dc_cons_addr;
	} else {
		struct dc_softc *sc;

		sc = dc_cd.cd_devs[DCUNIT(dev)];
		dcaddr = (dcregs *)sc->dc_pdma[out_line].p_addr;
		brk = sc->dc_brk;
	}
	tcr = dcaddr->dc_tcr;
	dcaddr->dc_tcr = tcr | (1 << out_line);
	wbflush();
	DELAY(10);
	while (1) {
		/*
		 * Wait for transmitter to be not busy.
		 */
		timeout = 1000000;
		while (!(dcaddr->dc_csr & CSR_TRDY) && timeout > 0)
			timeout--;
		if (timeout == 0) {
			printf("dcPutc: timeout waiting for CSR_TRDY\n");
			break;
		}
		activeline = (dcaddr->dc_csr >> 8) & 3;
		/*
		 * Check to be sure its the right port.
		 */
		if (activeline != out_line) {
			tcr |= 1 << activeline;
			dcaddr->dc_tcr &= ~(1 << out_line);
			wbflush();
			DELAY(10);
			continue;
		}
		/*
		 * Start sending the character.
		 */
		dcaddr->dc_tdr = brk | (c & 0xff);
		wbflush();
		DELAY(10);
		/*
		 * Wait for character to be sent.
		 */
		while (1) {
			/*
			 * cc -O bug: this code produces and infinite loop!
			 * while (!(dcaddr->dc_csr & CSR_TRDY))
			 *	;
			 */
			timeout = 1000000;
			while (!(dcaddr->dc_csr & CSR_TRDY) && timeout > 0)
				timeout--;
			activeline = (dcaddr->dc_csr >> 8) & 3;
			if (activeline != out_line) {
				tcr |= 1 << activeline;
				dcaddr->dc_tcr &= ~(1 << activeline);
				wbflush();
				DELAY(10);
				continue;
			}
			dcaddr->dc_tcr &= ~(1 << out_line);
			wbflush();
			DELAY(10);
			break;
		}
		break;
	}
	/*
	 * Enable interrupts for other lines which became ready.
	 */
	if (tcr & 0xF) {
		dcaddr->dc_tcr = tcr;
		wbflush();
		DELAY(10);
	}

	splx(s);
}


/*
 * Enable/disable polling mode
 */
void
dcPollc(dev, on)
	dev_t dev;
	int on;
{
#if defined(DIAGNOSTIC) || defined(DEBUG)
	printf("dc_Pollc(%d, %d): not implemented\n", minor(dev), on);
#endif
}


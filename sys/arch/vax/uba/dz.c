/*	$NetBSD: dz.c,v 1.4 1996/10/13 03:35:15 christos Exp $	*/
/*
 * Copyright (c) 1996  Ken C. Wellsch.  All rights reserved.
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
#include <sys/device.h>

#include <machine/pte.h>
#include <machine/trap.h>

#include <vax/uba/ubareg.h>
#include <vax/uba/ubavar.h>

#include <vax/uba/dzreg.h>

/* A DZ-11 has 8 ports while a DZV/DZQ-11 has only 4. We use 8 by default */

#define	NDZLINE 	8

#define DZ_C2I(c)	((c)<<3)	/* convert controller # to index */
#define DZ_I2C(c)	((c)>>3)	/* convert minor to controller # */
#define DZ_PORT(u)	((u)&07)	/* extract the port # */

struct	dz_softc {
	struct	device	sc_dev;		/* Autoconf blaha */
	dzregs *	sc_addr;	/* controller reg address */
	int		sc_type;	/* DZ11 or DZV11? */
	int		sc_rxint;	/* Receive interrupt count XXX */
	u_char		sc_brk;		/* Break asserted on some lines */
	struct {
		struct	tty *	dz_tty;		/* what we work on */
		caddr_t		dz_mem;		/* pointers to clist output */
		caddr_t		dz_end;		/*   allowing pdma action */
	} sc_dz[NDZLINE];
};

/* Flags used to monitor modem bits, make them understood outside driver */

#define DML_DTR		TIOCM_DTR
#define DML_DCD		TIOCM_CD
#define DML_RI		TIOCM_RI
#define DML_BRK		0100000		/* no equivalent, we will mask */

static struct speedtab dzspeedtab[] =
{
  {       0,	0		},
  {      50,	DZ_LPR_B50	},
  {      75,	DZ_LPR_B75	},
  {     110,	DZ_LPR_B110	},
  {     134,	DZ_LPR_B134	},
  {     150,	DZ_LPR_B150	},
  {     300,	DZ_LPR_B300	},
  {     600,	DZ_LPR_B600	},
  {    1200,	DZ_LPR_B1200	},
  {    1800,	DZ_LPR_B1800	},
  {    2000,	DZ_LPR_B2000	},
  {    2400,	DZ_LPR_B2400	},
  {    3600,	DZ_LPR_B3600	},
  {    4800,	DZ_LPR_B4800	},
  {    7200,	DZ_LPR_B7200	},
  {    9600,	DZ_LPR_B9600	},
  {      -1,	-1		}
};

static	int	dz_match __P((struct device *, void *, void *));
static	void	dz_attach __P((struct device *, struct device *, void *));
static	void	dzrint __P((int));
static	void	dzxint __P((int));
static	void	dzstart __P((struct tty *));
static	int	dzparam __P((struct tty *, struct termios *));
static unsigned	dzmctl __P((struct dz_softc *, int, int, int));
static	void	dzscan __P((void *));
struct	tty *	dztty __P((dev_t));
	int	dzopen __P((dev_t, int, int, struct proc *));
	int	dzclose __P((dev_t, int, int, struct proc *));
	int	dzread __P((dev_t, struct uio *, int));
	int	dzwrite __P((dev_t, struct uio *, int));
	int	dzioctl __P((dev_t, int, caddr_t, int, struct proc *));
	void	dzstop __P((struct tty *, int));

struct	cfdriver dz_cd = {
	NULL, "dz", DV_TTY
};

struct	cfattach dz_ca = {
	sizeof(struct dz_softc), dz_match, dz_attach
};


/*
 * The DZ series doesn't interrupt on carrier transitions,
 * so we have to use a timer to watch it.
 */
static int	dz_timer = 0;	/* true if timer started */

#define DZ_DZ	8		/* Unibus DZ-11 board linecount */
#define DZ_DZV	4		/* Q-bus DZV-11 or DZQ-11 */

/* Autoconfig handles: setup the controller to interrupt, */
/* then complete the housecleaning for full operation */

static int
dz_match (parent, match, aux)
        struct device *parent;
        void *match, *aux;
{
	struct uba_attach_args *ua = aux;
	register dzregs *dzaddr;
	register int n;

	dzaddr = (dzregs *) ua->ua_addr;

	/* Reset controller to initialize, enable TX interrupts */
	/* to catch floating vector info elsewhere when completed */

	dzaddr->dz_csr = (DZ_CSR_MSE | DZ_CSR_TXIE);
	dzaddr->dz_tcr = 1;	/* Force a TX interrupt */

	DELAY(100000);	/* delay 1/10 second */

	dzaddr->dz_csr = DZ_CSR_RESET;

	/* Now wait up to 3 seconds for reset/clear to complete. */

	for (n = 0; n < 300; n++) {
		DELAY(10000);
		if ((dzaddr->dz_csr & DZ_CSR_RESET) == 0)
			break;
	}

	/* If the RESET did not clear after 3 seconds, */
	/* the controller must be broken. */

	if (n >= 300)
		return (0);

	/* Register the TX interrupt handler */

	ua->ua_ivec = dzxint;

       	return (1);
}

static void
dz_attach (parent, self, aux)
        struct device *parent, *self;
        void *aux;
{
	struct uba_softc *uh = (void *)parent;
	struct	dz_softc *sc = (void *)self;
	register struct uba_attach_args *ua = aux;
	register dzregs *dzaddr;
	register int n;

	dzaddr = (dzregs *) ua->ua_addr;
	sc->sc_addr = dzaddr;

#ifdef QBA
	if (uh->uh_type == QBA)
		sc->sc_type = DZ_DZV;
	else
#endif
		sc->sc_type = DZ_DZ;

	sc->sc_rxint = sc->sc_brk = 0;

	dzaddr->dz_csr = (DZ_CSR_MSE | DZ_CSR_RXIE | DZ_CSR_TXIE);
	dzaddr->dz_dtr = 0;	/* Make sure DTR bits are zero */
	dzaddr->dz_break = 0;	/* Status of BREAK bits, all off */

	/* Initialize our softc structure. Should be done in open? */

	for (n = 0; n < sc->sc_type; n++)
		sc->sc_dz[n].dz_tty = ttymalloc();

	/* Now register the RX interrupt handler */
	ubasetvec(self, ua->ua_cvec-1, dzrint);

	/* Alas no interrupt on modem bit changes, so we manually scan */

	if (dz_timer == 0) {
		dz_timer = 1;
		timeout(dzscan, (void *)0, hz);
	}

	printf("\n");
	return;
}

/* Receiver Interrupt */

static void
dzrint(cntlr)
	int cntlr;
{
	struct	dz_softc *sc = dz_cd.cd_devs[cntlr];
	volatile dzregs *dzaddr;
	register struct tty *tp;
	register int cc, line;
	register unsigned c;
	int overrun = 0;

	sc->sc_rxint++;

	dzaddr = sc->sc_addr;

	while ((c = dzaddr->dz_rbuf) & DZ_RBUF_DATA_VALID) {
		cc = c & 0xFF;
		line = DZ_PORT(c>>8);
		tp = sc->sc_dz[line].dz_tty;

		if (!(tp->t_state & TS_ISOPEN)) {
			wakeup((caddr_t)&tp->t_rawq);
			continue;
		}

		if ((c & DZ_RBUF_OVERRUN_ERR) && overrun == 0) {
			log(LOG_WARNING, "%s: silo overflow, line %d\n",
			    sc->sc_dev.dv_xname, line);
			overrun = 1;
		}
		/* A BREAK key will appear as a NULL with a framing error */
		if (c & DZ_RBUF_FRAMING_ERR)
			cc |= TTY_FE;
		if (c & DZ_RBUF_PARITY_ERR)
			cc |= TTY_PE;

		(*linesw[tp->t_line].l_rint)(cc, tp);
	}
	return;
}

/* Transmitter Interrupt */

static void
dzxint(cntlr)
	int cntlr;
{
	volatile dzregs *dzaddr;
	register struct dz_softc *sc = dz_cd.cd_devs[cntlr];
	register struct tty *tp;
	register unsigned csr;
	register int line;

	dzaddr = sc->sc_addr;

	/*
	 * Switch to POLLED mode.
	 *   Some simple measurements indicated that even on
	 *  one port, by freeing the scanner in the controller
	 *  by either providing a character or turning off
	 *  the port when output is complete, the transmitter
	 *  was ready to accept more output when polled again.
	 *   With just two ports running the game "worms,"
	 *  almost every interrupt serviced both transmitters!
	 *   Each UART is double buffered, so if the scanner
	 *  is quick enough and timing works out, we can even
	 *  feed the same port twice.
	 */

	dzaddr->dz_csr &= ~(DZ_CSR_TXIE);

	while (((csr = dzaddr->dz_csr) & DZ_CSR_TX_READY) != 0) {

		line = DZ_PORT(csr>>8);

		if (sc->sc_dz[line].dz_mem < sc->sc_dz[line].dz_end) {
			dzaddr->dz_tbuf = *sc->sc_dz[line].dz_mem++; 
			continue;
		}

		/*
		 * Turn off this TX port as all pending output
		 * has been completed - thus freeing the scanner
		 * on the controller to hopefully find another
		 * pending TX operation we can service now.
		 * (avoiding the overhead of another interrupt)
		 */

		dzaddr->dz_tcr &= ~(1 << line);

		tp = sc->sc_dz[line].dz_tty;

		tp->t_state &= ~TS_BUSY;

		if (tp->t_state & TS_FLUSH)
			tp->t_state &= ~TS_FLUSH;
		else {
			ndflush (&tp->t_outq, (sc->sc_dz[line].dz_mem -
					(caddr_t)tp->t_outq.c_cf));
			sc->sc_dz[line].dz_end = sc->sc_dz[line].dz_mem =
			    tp->t_outq.c_cf;
		}

		if (tp->t_line)
			(*linesw[tp->t_line].l_start)(tp);
		else
			dzstart(tp);
	}

	/*
	 * Re-enable TX interrupts.
	 */

	dzaddr->dz_csr |= (DZ_CSR_TXIE);
	return;
}

int
dzopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	register struct tty *tp;
	register int unit, line;
	struct	dz_softc *sc;
	int s, error = 0;

	unit = DZ_I2C(minor(dev));
	line = DZ_PORT(minor(dev));

	if (unit >= dz_cd.cd_ndevs ||  dz_cd.cd_devs[unit] == NULL)
		return (ENXIO);

	sc = dz_cd.cd_devs[unit];

	if (line >= sc->sc_type)
		return ENXIO;

	tp = sc->sc_dz[line].dz_tty;
	if (tp == NULL)
		return (ENODEV);
	tp->t_oproc   = dzstart;
	tp->t_param   = dzparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		if (tp->t_ispeed == 0) {
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		}
		(void) dzparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if ((tp->t_state & TS_XCLUDE) && p->p_ucred->cr_uid != 0)
		return (EBUSY);
	/* Use DMBIS and *not* DMSET or else we clobber incoming bits */
	if (dzmctl(sc, line, DML_DTR, DMBIS) & DML_DCD)
		tp->t_state |= TS_CARR_ON;
	s = spltty();
	while (!(flag & O_NONBLOCK) && !(tp->t_cflag & CLOCAL) &&
	       !(tp->t_state & TS_CARR_ON)) {
		tp->t_state |= TS_WOPEN;
		error = ttysleep(tp, (caddr_t)&tp->t_rawq,
				TTIPRI | PCATCH, ttopen, 0);
		if (error)
			break;
	}
	(void) splx(s);
	if (error)
		return (error);
	return ((*linesw[tp->t_line].l_open)(dev, tp));
}

/*ARGSUSED*/
int
dzclose (dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct	dz_softc *sc;
	register struct tty *tp;
	register int unit, line;

	
	unit = DZ_I2C(minor(dev));
	line = DZ_PORT(minor(dev));
	sc = dz_cd.cd_devs[unit];

	tp = sc->sc_dz[line].dz_tty;

	(*linesw[tp->t_line].l_close)(tp, flag);

	/* Make sure a BREAK state is not left enabled. */
	(void) dzmctl(sc, line, DML_BRK, DMBIC);

	/* Do a hangup if so required. */
	if ((tp->t_cflag & HUPCL) || (tp->t_state & TS_WOPEN) ||
	    !(tp->t_state & TS_ISOPEN))
		(void) dzmctl(sc, line, 0, DMSET);

	return (ttyclose(tp));
}

int
dzread (dev, uio, flag)
	dev_t dev;
	struct uio *uio;
{
	register struct tty *tp;
	struct	dz_softc *sc;

	sc = dz_cd.cd_devs[DZ_I2C(minor(dev))];

	tp = sc->sc_dz[DZ_PORT(minor(dev))].dz_tty;
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
dzwrite (dev, uio, flag)
	dev_t dev;
	struct uio *uio;
{
	register struct tty *tp;
	struct	dz_softc *sc;

	sc = dz_cd.cd_devs[DZ_I2C(minor(dev))];

	tp = sc->sc_dz[DZ_PORT(minor(dev))].dz_tty;
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

/*ARGSUSED*/
int
dzioctl (dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct	dz_softc *sc;
	register struct tty *tp;
	register int unit, line;
	int error;

	unit = DZ_I2C(minor(dev));
	line = DZ_PORT(minor(dev));
	sc = dz_cd.cd_devs[unit];
	tp = sc->sc_dz[line].dz_tty;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	switch (cmd) {

	case TIOCSBRK:
		(void) dzmctl(sc, line, DML_BRK, DMBIS);
		break;

	case TIOCCBRK:
		(void) dzmctl(sc, line, DML_BRK, DMBIC);
		break;

	case TIOCSDTR:
		(void) dzmctl(sc, line, DML_DTR, DMBIS);
		break;

	case TIOCCDTR:
		(void) dzmctl(sc, line, DML_DTR, DMBIC);
		break;

	case TIOCMSET:
		(void) dzmctl(sc, line, *(int *)data, DMSET);
		break;

	case TIOCMBIS:
		(void) dzmctl(sc, line, *(int *)data, DMBIS);
		break;

	case TIOCMBIC:
		(void) dzmctl(sc, line, *(int *)data, DMBIC);
		break;

	case TIOCMGET:
		*(int *)data = (dzmctl(sc, line, 0, DMGET) & ~DML_BRK);
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

struct tty *
dztty (dev)
        dev_t dev;
{
	struct	dz_softc *sc = dz_cd.cd_devs[DZ_I2C(minor(dev))];
        struct tty *tp = sc->sc_dz[DZ_PORT(minor(dev))].dz_tty;

        return (tp);
}

/*ARGSUSED*/
void
dzstop(tp, flag)
	register struct tty *tp;
{
	register struct dz_softc *sc;
	int unit, line, s;

	unit = DZ_I2C(minor(tp->t_dev));
	line = DZ_PORT(minor(tp->t_dev));
	sc = dz_cd.cd_devs[unit];

	s = spltty();

	if (tp->t_state & TS_BUSY) {
		sc->sc_dz[line].dz_end = sc->sc_dz[line].dz_mem;
		if (!(tp->t_state & TS_TTSTOP))
			tp->t_state |= TS_FLUSH;
	}
	(void) splx(s);
}

static void
dzstart (tp)
	register struct tty *tp;
{
	register struct dz_softc *sc;
	register dzregs *dzaddr;
	register int unit, line;
	register int cc;
	int s;

	unit = DZ_I2C(minor(tp->t_dev));
	line = DZ_PORT(minor(tp->t_dev));
	sc = dz_cd.cd_devs[unit];

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
	cc = ndqb(&tp->t_outq, 0);
	if (cc == 0) 
		goto out;

	tp->t_state |= TS_BUSY;

	dzaddr = sc->sc_addr;

	sc->sc_dz[line].dz_end = sc->sc_dz[line].dz_mem = tp->t_outq.c_cf;
	sc->sc_dz[line].dz_end += cc;
	dzaddr->dz_tcr |= (1 << line);	/* Enable this TX port */

out:
	(void) splx(s);
	return;
}

static int
dzparam(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	struct	dz_softc *sc;
	register dzregs *dzaddr;
	register int cflag = t->c_cflag;
	int unit, line;
	int ispeed = ttspeedtab(t->c_ispeed, dzspeedtab);
	int ospeed = ttspeedtab(t->c_ospeed, dzspeedtab);
	register unsigned lpr;
	int s;

	unit = DZ_I2C(minor(tp->t_dev));
	line = DZ_PORT(minor(tp->t_dev));
	sc = dz_cd.cd_devs[unit];

	/* check requested parameters */
        if (ospeed < 0 || ispeed < 0 || ispeed != ospeed)
                return (EINVAL);

        tp->t_ispeed = t->c_ispeed;
        tp->t_ospeed = t->c_ospeed;
        tp->t_cflag = cflag;

	if (ospeed == 0) {
		(void) dzmctl(sc, line, 0, DMSET);	/* hang up line */
		return (0);
	}

	s = spltty();
	dzaddr = sc->sc_addr;

	lpr = DZ_LPR_RX_ENABLE | ((ispeed&0xF)<<8) | line;

	switch (cflag & CSIZE)
	{
	  case CS5:
		lpr |= DZ_LPR_5_BIT_CHAR;
		break;
	  case CS6:
		lpr |= DZ_LPR_6_BIT_CHAR;
		break;
	  case CS7:
		lpr |= DZ_LPR_7_BIT_CHAR;
		break;
	  default:
		lpr |= DZ_LPR_8_BIT_CHAR;
		break;
	}
	if (cflag & PARENB)
		lpr |= DZ_LPR_PARENB;
	if (cflag & PARODD)
		lpr |= DZ_LPR_OPAR;
	if (cflag & CSTOPB)
		lpr |= DZ_LPR_2_STOP;

	dzaddr->dz_lpr = lpr;

	(void) splx(s);
	return (0);
}

static unsigned
dzmctl(sc, line, bits, how)
	register struct dz_softc *sc;
	int line, bits, how;
{
	register dzregs *dzaddr;
	register unsigned status;
	register unsigned mbits;
	register unsigned bit;
	int s;

	s = spltty();

	dzaddr = sc->sc_addr;

	mbits = 0;

	bit = (1 << line);

	/* external signals as seen from the port */

	status = dzaddr->dz_dcd;

	if (status & bit)
		mbits |= DML_DCD;

	status = dzaddr->dz_ring;

	if (status & bit)
		mbits |= DML_RI;

	/* internal signals/state delivered to port */

	status = dzaddr->dz_dtr;

	if (status & bit)
		mbits |= DML_DTR;

	if (sc->sc_brk & bit)
		mbits |= DML_BRK;

	switch (how)
	{
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

	if (mbits & DML_DTR)
		dzaddr->dz_dtr |= bit;
	else
		dzaddr->dz_dtr &= ~bit;

	if (mbits & DML_BRK)
		dzaddr->dz_break = (sc->sc_brk |= bit);
	else
		dzaddr->dz_break = (sc->sc_brk &= ~bit);

	(void) splx(s);
	return (mbits);
}

/*
 * This is called by timeout() periodically.
 * Check to see if modem status bits have changed.
 */
static void
dzscan(arg)
	void *arg;
{
	register dzregs *dzaddr;
	register struct dz_softc *sc;
	register struct tty *tp;
	register int n, bit, port;
	unsigned csr;
	int s;

	s = spltty();

	for (n = 0; n < dz_cd.cd_ndevs; n++) {

		if (dz_cd.cd_devs[n] == NULL)
			continue;

		sc = dz_cd.cd_devs[n];

		for (port = 0; port < sc->sc_type; port++) {

			dzaddr = sc->sc_addr;
			tp = sc->sc_dz[port].dz_tty;
			bit = (1 << port);
	
			if (dzaddr->dz_dcd & bit) { /* carrier present */

				if (!(tp->t_state & TS_CARR_ON))
					(void)(*linesw[tp->t_line].l_modem)
					    (tp, 1);
			} else if ((tp->t_state & TS_CARR_ON) &&
				(*linesw[tp->t_line].l_modem)(tp, 0) == 0)
				    dzaddr->dz_tcr &= ~bit;
	    	}

		/*
		 *  If the RX interrupt rate is this high, switch
		 *  the controller to Silo Alarm - which means don't
	 	 *  interrupt until the RX silo has 16 characters in
	 	 *  it (the silo is 64 characters in all).
		 *  Avoid oscillating SA on and off by not turning
		 *  if off unless the rate is appropriately low.
		 */

		dzaddr = sc->sc_addr;

		csr = dzaddr->dz_csr;

		if (sc->sc_rxint > (16*10)) {
			if ((csr & DZ_CSR_SAE) == 0)
				dzaddr->dz_csr = (csr | DZ_CSR_SAE);
	    	} else if ((csr & DZ_CSR_SAE) != 0)
			if (sc->sc_rxint < 10)
				dzaddr->dz_csr = (csr & ~(DZ_CSR_SAE));

		sc->sc_rxint = 0;
	}
	(void) splx(s);
	timeout(dzscan, (void *)0, hz);
	return;
}
